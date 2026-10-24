#include "xwa_runtime/models/stats_profiles.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/models/model_settings.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/models/model_swaps.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define STATS_NAME_MAX 64
/* The profile cache of an object type: by player or not, and by markings. */
#define STATS_CACHE_UNSET (-2)

static const char* const s_fieldKeys[XWA_STATS_FIELDS] = {
	"speed",
	"acceleration",
	"deceleration",
	"pitch_rate",
	"roll_rate",
	"yaw_rate",
	"system_damage_hull_threshold",
	"hull",
	"system_strength",
	"shields",
	"speed_increment",
	"speed_decrement",
	"has_shields",
	"has_hyperdrive",
	"warhead_collision_damage",
};

typedef struct StatsProfile {
	char name[STATS_NAME_MAX];
	uint32_t setMask;
	int32_t value[XWA_STATS_FIELDS];
} StatsProfile;

typedef struct StatsProfilesType {
	int profileCount;
	/* The profile of a craft of this type, by player (1) or not (0) and by
	   markings: an index in `profiles`, -1 for none, or STATS_CACHE_UNSET. */
	int16_t cache[2][256];
	StatsProfile profiles[];
} StatsProfilesType;

/* A mission's choice of profile names for one model. */
typedef struct StatsMissionModel {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	char player[STATS_NAME_MAX];
	char all[STATS_NAME_MAX];
	uint8_t hasMarkings[256];
	char (*markings)[STATS_NAME_MAX]; /* 256 names, or NULL */
} StatsMissionModel;

static StatsProfilesType* s_types[OBJ_Count];
static StatsMissionModel* s_models;
static int s_modelCount;
/* -1 when not set. */
static int32_t s_percent[XWA_STATS_PERCENT_FIELDS];
static int32_t s_playerPercent[XWA_STATS_PERCENT_FIELDS];

static int StatsProfiles_Field(const char* key) {
	int field;

	for (field = 0; field < XWA_STATS_FIELDS; ++field) {
		if (strcmp(s_fieldKeys[field], key) == 0) {
			return field;
		}
	}
	return -1;
}

/* Copies `text` in lower case into `out`. Returns 0 when it doesn't fit. */
static int StatsProfiles_LowerName(const char* text, char out[STATS_NAME_MAX]) {
	size_t i;

	for (i = 0; text[i] != '\0'; ++i) {
		if (i + 1 >= STATS_NAME_MAX) {
			return 0;
		}
		out[i] = (char)tolower((unsigned char)text[i]);
	}
	out[i] = '\0';
	return 1;
}

static void StatsProfiles_ReadProfile(const char* path, const char* name, const AeronConfigNode* node,
									  StatsProfile* profile) {
	size_t index;

	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
		const int field = StatsProfiles_Field(key);
		const int isBool = field >= XWA_STATS_HAS_SHIELDS;
		const int64_t number = isBool ? 0 : AeronConfigNode_Int(value, 0);

		if (field < 0 || AeronConfigNode_Type(value) != (isBool ? AERON_CONFIG_BOOL : AERON_CONFIG_INT) ||
			number < INT32_MIN || number > INT32_MAX) {
			Aeron_LogWarn("xwa.models", "%s: stats_profiles.%s.%s isn't a known key with a %s. It is ignored",
						  path, name, key, isBool ? "true or false value" : "number");
			continue;
		}
		profile->value[field] = isBool ? AeronConfigNode_Bool(value, 0) != 0 : (int32_t)number;
		profile->setMask |= 1u << field;
	}
}

void XwaStatsProfiles_ClearModelSettings(void) {
	unsigned int objectType;

	for (objectType = 0; objectType < OBJ_Count; ++objectType) {
		free(s_types[objectType]);
		s_types[objectType] = NULL;
	}
}

void XwaStatsProfiles_ReadModelSettings(unsigned int objectType, const char* path,
										const AeronConfigNode* group) {
	StatsProfilesType* type;
	const size_t count = AeronConfigNode_MapCount(group);
	size_t index;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: stats_profiles isn't a map. It is ignored", path);
		return;
	}
	type = (StatsProfilesType*)calloc(1, sizeof *type + count * sizeof type->profiles[0]);
	if (type == NULL) {
		return;
	}
	for (index = 0; index < count; ++index) {
		const char* name = AeronConfigNode_MapKeyAt(group, index);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(group, index);
		StatsProfile* profile = &type->profiles[type->profileCount];

		if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP || !StatsProfiles_LowerName(name, profile->name)) {
			Aeron_LogWarn("xwa.models",
						  "%s: stats_profiles.%s needs a map and a name of up to %d characters. "
						  "It is ignored",
						  path, name, STATS_NAME_MAX - 1);
			continue;
		}
		StatsProfiles_ReadProfile(path, name, node, profile);
		type->profileCount++;
	}
	memset(type->cache, 0xfe, sizeof type->cache); /* STATS_CACHE_UNSET */
	s_types[objectType] = type;
}

static void StatsProfiles_ClearMission(void) {
	int i;

	for (i = 0; i < s_modelCount; ++i) {
		free(s_models[i].markings);
	}
	free(s_models);
	s_models = NULL;
	s_modelCount = 0;
	for (i = 0; i < XWA_STATS_PERCENT_FIELDS; ++i) {
		s_percent[i] = -1;
		s_playerPercent[i] = -1;
	}
}

/* Reads a profile name. Returns 0, logged, when it isn't a string that
   fits. */
static int StatsProfiles_ReadName(const char* path, const char* where, const AeronConfigNode* node,
								  char out[STATS_NAME_MAX]) {
	const char* text = AeronConfigNode_String(node, NULL);

	if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING || text == NULL ||
		!StatsProfiles_LowerName(text, out)) {
		Aeron_LogWarn("xwa.mission", "%s:%d: %s needs a profile name of up to %d characters; it is ignored",
					  path, AeronConfigNode_Line(node), where, STATS_NAME_MAX - 1);
		return 0;
	}
	return 1;
}

static void StatsProfiles_ReadPercents(const char* path, const char* where, const AeronConfigNode* node,
									   int32_t percent[XWA_STATS_PERCENT_FIELDS]) {
	size_t index;

	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
		const int field = StatsProfiles_Field(key);
		const int64_t number = AeronConfigNode_Int(value, -1);

		if (field < 0 || field >= XWA_STATS_PERCENT_FIELDS ||
			AeronConfigNode_Type(value) != AERON_CONFIG_INT || number < 0 || number > INT32_MAX) {
			Aeron_LogWarn("xwa.mission",
						  "%s:%d: stats_profiles.%s.%s needs a field that takes a percent and a "
						  "percent of 0 or more; it is ignored",
						  path, AeronConfigNode_Line(value), where, key);
			continue;
		}
		percent[field] = (int32_t)number;
	}
}

static void StatsProfiles_ReadModel(const char* path, const char* key, const AeronConfigNode* node,
									StatsMissionModel* model) {
	const AeronConfigNode* markings = AeronConfigNode_MapGet(node, "markings");
	size_t index;

	snprintf(model->key, sizeof model->key, "%s", key);
	if (AeronConfigNode_MapGet(node, "player") != NULL) {
		StatsProfiles_ReadName(path, "player", AeronConfigNode_MapGet(node, "player"), model->player);
	}
	if (AeronConfigNode_MapGet(node, "all") != NULL) {
		StatsProfiles_ReadName(path, "all", AeronConfigNode_MapGet(node, "all"), model->all);
	}
	for (index = 0; index < AeronConfigNode_MapCount(markings); ++index) {
		const char* text = AeronConfigNode_MapKeyAt(markings, index);
		char* end;
		const long value = strtol(text, &end, 10);

		if (*text == '\0' || *end != '\0' || value < 0 || value > 255) {
			Aeron_LogWarn("xwa.mission",
						  "%s: stats_profiles.models.%s.markings.%s needs markings from 0 to 255; "
						  "it is ignored",
						  path, key, text);
			continue;
		}
		if (model->markings == NULL &&
			(model->markings = (char (*)[STATS_NAME_MAX])calloc(256, STATS_NAME_MAX)) == NULL) {
			return;
		}
		model->hasMarkings[value] = (uint8_t)StatsProfiles_ReadName(
			path, "markings", AeronConfigNode_MapValueAt(markings, index), model->markings[value]);
	}
}

void XwaStatsProfiles_LoadMission(const char* missionPath) {
	AeronConfigFile* config = NULL;
	const AeronConfigNode* section;
	const AeronConfigNode* models;
	char path[300];
	size_t index;
	unsigned int objectType;

	StatsProfiles_ClearMission();
	for (objectType = 0; objectType < OBJ_Count; ++objectType) {
		if (s_types[objectType] != NULL) {
			memset(s_types[objectType]->cache, 0xfe, sizeof s_types[objectType]->cache);
		}
	}
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	section = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "stats_profiles");
	if (section != NULL && AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.mission", "%s:%d: stats_profiles must be a map; it is ignored", path,
					   AeronConfigNode_Line(section));
		section = NULL;
	}
	models = AeronConfigNode_MapGet(section, "models");
	if (AeronConfigNode_MapCount(models) != 0) {
		s_models = (StatsMissionModel*)calloc(AeronConfigNode_MapCount(models), sizeof *s_models);
	}
	for (index = 0; s_models != NULL && index < AeronConfigNode_MapCount(models); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(models, index);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(models, index);
		char normalized[XWA_MODEL_SOURCE_KEY_MAX];

		if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP || !XwaModelSource_NormalizeKey(key, normalized) ||
			strcmp(normalized, key) != 0) {
			Aeron_LogWarn("xwa.mission",
						  "%s:%d: stats_profiles.models.%s needs a model key and a map; it is "
						  "ignored",
						  path, AeronConfigNode_Line(node), key);
			continue;
		}
		StatsProfiles_ReadModel(path, key, node, &s_models[s_modelCount++]);
	}
	StatsProfiles_ReadPercents(path, "percent", AeronConfigNode_MapGet(section, "percent"), s_percent);
	StatsProfiles_ReadPercents(path, "player_percent", AeronConfigNode_MapGet(section, "player_percent"),
							   s_playerPercent);
	AeronConfigFile_Destroy(config);
}

static int StatsProfiles_Find(const StatsProfilesType* type, const char* name) {
	int i;

	for (i = 0; i < type->profileCount; ++i) {
		if (strcmp(type->profiles[i].name, name) == 0) {
			return i;
		}
	}
	return -1;
}

/* The profile of a craft of `objectType` with `markings`: the mission's
   name for the model, then the variant for players and the difficulty. */
static int StatsProfiles_Choose(unsigned int objectType, const StatsProfilesType* type, int isPlayer,
								uint8_t markings) {
	static const char* const difficulties[3] = { "_easy", "_medium", "_hard" };
	const char* difficulty = g_flightDifficulty < 3 ? difficulties[g_flightDifficulty] : "";
	const char* name = "default";
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	char section[STATS_NAME_MAX * 2];
	int i;

	if (XwaModelSwaps_TypeKey(objectType, key)) {
		for (i = 0; i < s_modelCount; ++i) {
			const StatsMissionModel* model = &s_models[i];

			if (strcmp(model->key, key) != 0) {
				continue;
			}
			if (isPlayer && model->player[0] != '\0') {
				name = model->player;
			} else if (model->markings != NULL && model->hasMarkings[markings]) {
				name = model->markings[markings];
			} else if (model->all[0] != '\0') {
				name = model->all;
			}
			break;
		}
	}
	if (isPlayer) {
		snprintf(section, sizeof section, "%s_player%s", name, difficulty);
		if ((i = StatsProfiles_Find(type, section)) >= 0) {
			return i;
		}
		snprintf(section, sizeof section, "%s_player", name);
		if ((i = StatsProfiles_Find(type, section)) >= 0) {
			return i;
		}
	}
	snprintf(section, sizeof section, "%s%s", name, difficulty);
	if ((i = StatsProfiles_Find(type, section)) >= 0) {
		return i;
	}
	return StatsProfiles_Find(type, name);
}

static const StatsProfile* StatsProfiles_Of(unsigned int objIdx) {
	const ObjectRecord* object = &g_objectTable[objIdx];
	StatsProfilesType* type;
	int isPlayer;
	uint8_t markings;
	int16_t* cached;

	if (object->objectType >= OBJ_Count || object->mobj == NULL) {
		return NULL;
	}
	XwaModelSettings_Ensure(object->objectType);
	type = s_types[object->objectType];
	if (type == NULL) {
		return NULL;
	}
	isPlayer = object->playerOwnerIdx != -1;
	markings = object->mobj->nodeSwitchIndex;
	cached = &type->cache[isPlayer][markings];
	if (*cached == STATS_CACHE_UNSET) {
		*cached = (int16_t)StatsProfiles_Choose(object->objectType, type, isPlayer, markings);
	}
	return *cached >= 0 ? &type->profiles[*cached] : NULL;
}

/* value * percent / 100, wrapping at 32 bits. */
static int32_t StatsProfiles_Percent(int32_t value, int32_t percent) {
	return (int32_t)((uint32_t)value * (uint32_t)percent) / 100;
}

/* The mission's percent of `field` for the craft of `objIdx`, or -1. */
static int32_t StatsProfiles_PercentOf(unsigned int objIdx, XwaStatsField field) {
	if (g_objectTable[objIdx].playerOwnerIdx != -1 && s_playerPercent[field] != -1) {
		return s_playerPercent[field];
	}
	return s_percent[field];
}

int32_t XwaStatsProfiles_Value(unsigned int objIdx, XwaStatsField field, int32_t stock) {
	const StatsProfile* profile = StatsProfiles_Of(objIdx);
	int32_t value = stock;

	if (profile != NULL && (profile->setMask & (1u << field))) {
		value = profile->value[field];
	}
	if (field < XWA_STATS_PERCENT_FIELDS) {
		const int32_t percent = StatsProfiles_PercentOf(objIdx, field);

		if (percent != -1) {
			value = StatsProfiles_Percent(value, percent);
		}
	}
	return value;
}

int XwaStatsProfiles_DefaultValue(unsigned int objectType, XwaStatsField field, int32_t* value) {
	const StatsProfilesType* type;
	int index;

	if (objectType >= OBJ_Count) {
		return 0;
	}
	XwaModelSettings_Ensure(objectType);
	type = s_types[objectType];
	if (type == NULL || (index = StatsProfiles_Find(type, "default")) < 0 ||
		!(type->profiles[index].setMask & (1u << field))) {
		return 0;
	}
	*value = type->profiles[index].value[field];
	return 1;
}

void XwaStatsProfiles_ScaleSpeed(unsigned int objIdx) {
	MobileObject* mobj = g_objectTable[objIdx].mobj;
	CraftData* craft = mobj != NULL ? mobj->pCraft : NULL;
	int16_t oldSpeed;
	int16_t recordSpeed;
	int speedPercent;

	if (craft == NULL || craft->modelDefIndex == (ModelDefIndex)0xFFFF ||
		craft->aiFlight.maxSpeedCache == (int16_t)g_modelDefs[craft->modelDefIndex].maxSpeed) {
		return;
	}
	/* The speed's share of the record's top speed, in Q16, times
	   the craft's top speed, rounded. */
	oldSpeed = (int16_t)mobj->speed;
	recordSpeed = (int16_t)g_modelDefs[craft->modelDefIndex].maxSpeed;
	speedPercent = recordSpeed == 0 ? 0 : oldSpeed * 0x10000 / recordSpeed;
	mobj->speed = (uint16_t)(int16_t)((craft->aiFlight.maxSpeedCache * speedPercent + 0x8000) / 0x10000);
}

void XwaStatsProfiles_ApplyNewCraft(unsigned int objIdx) {
	MobileObject* mobj = g_objectTable[objIdx].mobj;
	CraftData* craft = mobj != NULL ? mobj->pCraft : NULL;
	const ModelDef* record;
	const StatsProfile* profile;
	int32_t percent;
	int hasShields;
	int hasHyperdrive;

	if (craft == NULL || craft->modelDefIndex == (ModelDefIndex)0xFFFF) {
		return;
	}
	record = &g_modelDefs[craft->modelDefIndex];
	profile = StatsProfiles_Of(objIdx);
	craft->aiFlight.maxSpeedCache =
		(int16_t)XwaStatsProfiles_Value(objIdx, XWA_STATS_SPEED, craft->aiFlight.maxSpeedCache);
	craft->aiFlight.pitchRate =
		(int16_t)XwaStatsProfiles_Value(objIdx, XWA_STATS_PITCH_RATE, craft->aiFlight.pitchRate);
	craft->aiFlight.rollRate =
		(int16_t)XwaStatsProfiles_Value(objIdx, XWA_STATS_ROLL_RATE, craft->aiFlight.rollRate);
	craft->aiFlight.turnRate =
		(int16_t)XwaStatsProfiles_Value(objIdx, XWA_STATS_YAW_RATE, craft->aiFlight.turnRate);
	craft->systemDamageHullThreshold =
		XwaStatsProfiles_Value(objIdx, XWA_STATS_DAMAGE_THRESHOLD, craft->systemDamageHullThreshold);
	craft->hullMax = XwaStatsProfiles_Value(objIdx, XWA_STATS_HULL, craft->hullMax);

	/* The shields keep their share of the record's value. */
	if (profile != NULL && (profile->setMask & (1u << XWA_STATS_SHIELDS))) {
		const uint32_t recordShields = (uint32_t)record->shieldStrength;
		const uint64_t shields = (uint32_t)profile->value[XWA_STATS_SHIELDS];

		craft->shieldFront =
			recordShields == 0 ? 0 : (int)((uint32_t)craft->shieldFront * shields / recordShields);
		craft->shieldRear =
			recordShields == 0 ? 0 : (int)((uint32_t)craft->shieldRear * shields / recordShields);
	}
	percent = StatsProfiles_PercentOf(objIdx, XWA_STATS_SHIELDS);
	if (percent != -1) {
		craft->shieldFront = (int)((uint32_t)craft->shieldFront * (uint32_t)percent / 100u);
		craft->shieldRear = (int)((uint32_t)craft->shieldRear * (uint32_t)percent / 100u);
	}

	/* Removes the shields and the hyperdrive as
	   Mission_InitFlightGroupObjectSlot does, with the profile's values. */
	hasShields = XwaStatsProfiles_Value(objIdx, XWA_STATS_HAS_SHIELDS, record->hasShields != 0) != 0;
	if (!hasShields && g_spawnStatus1 != 8 && g_spawnStatus2 != 8 && g_spawnStatus1 != 16 &&
		g_spawnStatus2 != 16) {
		craft->systemFlags ^= 1;
		craft->shieldFront = 0;
		craft->shieldRear = 0;
		craft->installedHudFeatureMask ^= 0x800;
	}
	hasHyperdrive = XwaStatsProfiles_Value(objIdx, XWA_STATS_HAS_HYPERDRIVE, record->hasHyperdrive != 0) != 0;
	if (!hasHyperdrive && g_spawnStatus1 != 9 && g_spawnStatus2 != 9 && g_spawnStatus1 != 16 &&
		g_spawnStatus2 != 16) {
		craft->systemFlags ^= 0x80;
	}
	craft->workingSubsystems = craft->systemFlags;
	craft->activeHudFeatureMask = craft->installedHudFeatureMask;
	XwaStatsProfiles_ScaleSpeed(objIdx);
}
