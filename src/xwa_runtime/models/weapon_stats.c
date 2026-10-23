#include "xwa_runtime/models/weapon_stats.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/models/model_settings.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define WEAPON_STATS_FLIGHT_YAML "flight/flight.yaml"
#define WEAPON_STATS_FIRST_WEAPON 280
#define WEAPON_STATS_WEAPONS 28
#define WEAPON_STATS_WARHEADS 12
#define WEAPON_STATS_HARDPOINT_TYPES 40
/* g_missionFlightGroups holds 192. */
#define WEAPON_STATS_FLIGHT_GROUPS 192
#define WEAPON_STATS_NAME_MAX 48
#define WEAPON_STATS_RANKS 6
#define WEAPON_STATS_DIFFICULTIES 3

/* The groups of the `weapons` section that have profiles. */
enum {
	WEAPON_STATS_GROUP_STATS,
	WEAPON_STATS_GROUP_WARHEADS,
	WEAPON_STATS_GROUP_ENERGY,
	WEAPON_STATS_GROUP_LINKING,
	WEAPON_STATS_GROUPS
};

static const char* const s_groups[WEAPON_STATS_GROUPS] = { "stats", "warhead_capacity_percent",
														   "energy_speed_q16", "linking" };
/* The keys that a flight group's entry uses to pick a profile in each group. */
static const char* const s_profileKeys[WEAPON_STATS_GROUPS] = { "weapon_profile", "warhead_profile",
																"energy_profile", "linking_profile" };

static const char* const s_stats[XWA_WEAPON_STATS] = {
	"duration_seconds",
	"duration_fraction",
	"score",
	"speed",
	"power",
	"power_speed_percent",
	"fire_rate",
	"duration_offset",
	"fire_ratio",
	"range",
	"precise",
	"spread_mask",
	"decharge_rate",
	"recharge_rate",
	"high_power_charge",
};

/* The forms of a stat: plain, for the player, for one difficulty, or for
   both. */
enum {
	WEAPON_STATS_FORM_PLAIN,
	WEAPON_STATS_FORM_PLAYER,
	WEAPON_STATS_FORM_EASY,
	WEAPON_STATS_FORM_PLAYER_EASY = WEAPON_STATS_FORM_EASY + WEAPON_STATS_DIFFICULTIES,
	WEAPON_STATS_FORMS = WEAPON_STATS_FORM_PLAYER_EASY + WEAPON_STATS_DIFFICULTIES
};

static const char* const s_forms[WEAPON_STATS_FORMS] = {
	"", "player", "easy", "medium", "hard", "player_easy", "player_medium", "player_hard"
};

static const char* const s_energyKeys[2] = { "all", "player" };

static const char* const s_linkingRuleNames[XWA_WEAPON_LINKING_RULES] = {
	"prevent_ai_lasers_ion_when_disabling",
	"prevent_ai_ion_when_not_disabling",
	"prevent_player_linked_lasers_ion",
};

typedef struct WeaponStatsForm {
	uint16_t set; /* one bit for each stat that is set */
	int32_t value[XWA_WEAPON_STATS];
} WeaponStatsForm;

/* One profile of a model file or of flight/flight.yaml. Each group has its own
   profiles, so `groups` has one bit for each group that has a profile with this
   name. */
typedef struct WeaponStatsProfile {
	char name[WEAPON_STATS_NAME_MAX]; /* "" for `default` */
	uint8_t groups;
	WeaponStatsForm weapons[WEAPON_STATS_WEAPONS][WEAPON_STATS_FORMS];
	int32_t warheadPercent[WEAPON_STATS_WARHEADS]; /* -1: not set */
	int32_t energy[2];                             /* all, player; -1: not set */
	int32_t laserLink;                             /* -1: not set */
	int8_t linking[XWA_WEAPON_LINKING_RULES];      /* -1: not set */
} WeaponStatsProfile;

typedef struct WeaponStatsSet {
	WeaponStatsProfile* profiles;
	int profileCount;
	int hasHardpointKinds;
	uint8_t hardpointKinds[WEAPON_STATS_HARDPOINT_TYPES];
} WeaponStatsSet;

static WeaponStatsSet s_defaults;
static WeaponStatsSet s_models[OBJ_Count];
static char s_flightGroupProfiles[WEAPON_STATS_FLIGHT_GROUPS][WEAPON_STATS_GROUPS][WEAPON_STATS_NAME_MAX];
static int s_hasRankFactors;
static float s_rankFactors[WEAPON_STATS_RANKS];
static int s_hasDifficultyFactors;
static float s_difficultyFactors[WEAPON_STATS_DIFFICULTIES];
static int s_limitLinkedWarheadFireRate;
static int8_t s_linkingRules[XWA_WEAPON_LINKING_RULES];

static void WeaponStats_FreeSet(WeaponStatsSet* set) {
	free(set->profiles);
	memset(set, 0, sizeof *set);
}

/* Copies `name` into `out` in lower case. The files call the values without a
   profile "default", and this module calls them "". Returns 0 when the name is
   too long. */
static int WeaponStats_ProfileName(const char* name, char out[WEAPON_STATS_NAME_MAX]) {
	size_t length = strlen(name);
	size_t index;

	if (length >= WEAPON_STATS_NAME_MAX) {
		return 0;
	}
	for (index = 0; index <= length; ++index) {
		out[index] = (char)tolower((unsigned char)name[index]);
	}
	if (strcmp(out, "default") == 0) {
		out[0] = '\0';
	}
	return 1;
}

/* Returns the profile `name` of `set`, and adds it if it is missing. Returns
   NULL when out of memory. */
static WeaponStatsProfile* WeaponStats_AddProfile(WeaponStatsSet* set, const char* name) {
	WeaponStatsProfile* profiles;
	WeaponStatsProfile* profile;
	int index;

	for (index = 0; index < set->profileCount; ++index) {
		if (strcmp(set->profiles[index].name, name) == 0) {
			return &set->profiles[index];
		}
	}
	profiles =
		(WeaponStatsProfile*)realloc(set->profiles, (size_t)(set->profileCount + 1) * sizeof *profiles);
	if (profiles == NULL) {
		return NULL;
	}
	set->profiles = profiles;
	profile = &profiles[set->profileCount++];
	memset(profile, 0, sizeof *profile);
	strcpy(profile->name, name);
	memset(profile->warheadPercent, 0xff, sizeof profile->warheadPercent);
	memset(profile->energy, 0xff, sizeof profile->energy);
	profile->laserLink = -1;
	memset(profile->linking, 0xff, sizeof profile->linking);
	return profile;
}

/* Reads an integer into `out`. If the node isn't an integer, logs it and
   returns 0. */
static int WeaponStats_ReadInt(const char* path, const char* where, const AeronConfigNode* node,
							   int32_t* out) {
	const int64_t value = AeronConfigNode_Int(node, 0);

	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < INT32_MIN || value > INT32_MAX) {
		Aeron_LogWarn("xwa.models", "%s: %s must be a number. It is ignored", path, where);
		return 0;
	}
	*out = (int32_t)value;
	return 1;
}

static int WeaponStats_ReadBool(const char* path, const char* where, const AeronConfigNode* node,
								int32_t* out) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogWarn("xwa.models", "%s: %s must be true or false. It is ignored", path, where);
		return 0;
	}
	*out = AeronConfigNode_Bool(node, 0) != 0;
	return 1;
}

/* Returns the index of `key` in `names`, or -1. */
static int WeaponStats_Find(const char* key, const char* const* names, int count) {
	int index;

	for (index = 0; index < count; ++index) {
		if (strcmp(key, names[index]) == 0) {
			return index;
		}
	}
	return -1;
}

/* Reads the stats in `node` into `form`. When `forms` is set, `node` can also
   have forms, which the caller reads. */
static void WeaponStats_ReadForm(const char* path, const char* where, const AeronConfigNode* node, int forms,
								 WeaponStatsForm* form) {
	size_t index;

	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
		const int stat = WeaponStats_Find(key, s_stats, XWA_WEAPON_STATS);
		char name[160];
		int32_t read;

		if (stat < 0) {
			if (!forms || WeaponStats_Find(key, s_forms + 1, WEAPON_STATS_FORMS - 1) < 0) {
				Aeron_LogWarn("xwa.models", "%s: %s.%s isn't a weapon stat. It is ignored", path, where, key);
			}
			continue;
		}
		snprintf(name, sizeof name, "%s.%s", where, key);
		if (stat == XWA_WEAPON_STAT_PRECISE ? WeaponStats_ReadBool(path, name, value, &read)
											: WeaponStats_ReadInt(path, name, value, &read)) {
			form->value[stat] = read;
			form->set |= (uint16_t)(1u << stat);
		}
	}
}

/* Reads the stats of one weapon type in a `stats` profile. */
static void WeaponStats_ReadWeapon(const char* path, const char* where, const AeronConfigNode* node,
								   WeaponStatsForm forms[WEAPON_STATS_FORMS]) {
	int form;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: %s isn't a map. It is ignored", path, where);
		return;
	}
	WeaponStats_ReadForm(path, where, node, 1, &forms[WEAPON_STATS_FORM_PLAIN]);
	for (form = WEAPON_STATS_FORM_PLAIN + 1; form < WEAPON_STATS_FORMS; ++form) {
		const AeronConfigNode* formNode = AeronConfigNode_MapGet(node, s_forms[form]);
		char name[128];

		if (formNode == NULL) {
			continue;
		}
		snprintf(name, sizeof name, "%s.%s", where, s_forms[form]);
		if (AeronConfigNode_Type(formNode) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s: %s isn't a map. It is ignored", path, name);
			continue;
		}
		WeaponStats_ReadForm(path, name, formNode, 0, &forms[form]);
	}
}

/* Reads a key that must be a number from `first` to `first` + `count` - 1.
   Returns the number minus `first`, or -1. */
static int WeaponStats_NumberKey(const char* path, const char* where, const char* key, int first, int count) {
	char* end;
	const long value = strtol(key, &end, 10);

	if (*key == '\0' || *end != '\0' || value < first || value >= first + count) {
		Aeron_LogWarn("xwa.models", "%s: %s.%s must be a number from %d to %d. It is ignored", path, where,
					  key, first, first + count - 1);
		return -1;
	}
	return (int)(value - first);
}

/* Reads `node`, a profile of group `group`, into `profile`. */
static void WeaponStats_ReadProfile(const char* path, const char* where, int group,
									const AeronConfigNode* node, WeaponStatsProfile* profile) {
	size_t index;

	profile->groups |= (uint8_t)(1u << group);
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
		char name[128];
		int at;

		snprintf(name, sizeof name, "%s.%s", where, key);
		switch (group) {
			case WEAPON_STATS_GROUP_STATS:
				if ((at = WeaponStats_NumberKey(path, where, key, WEAPON_STATS_FIRST_WEAPON,
												WEAPON_STATS_WEAPONS)) >= 0) {
					WeaponStats_ReadWeapon(path, name, value, profile->weapons[at]);
				}
				break;
			case WEAPON_STATS_GROUP_WARHEADS:
				if ((at = WeaponStats_NumberKey(path, where, key, 0, WEAPON_STATS_WARHEADS)) >= 0 &&
					WeaponStats_ReadInt(path, name, value, &profile->warheadPercent[at]) &&
					profile->warheadPercent[at] < 0) {
					Aeron_LogWarn("xwa.models", "%s: %s must be 0 or more. It is ignored", path, name);
					profile->warheadPercent[at] = -1;
				}
				break;
			case WEAPON_STATS_GROUP_ENERGY:
				if ((at = WeaponStats_Find(key, s_energyKeys, 2)) < 0) {
					Aeron_LogWarn("xwa.models", "%s: %s isn't 'all' or 'player'. It is ignored", path, name);
				} else if (WeaponStats_ReadInt(path, name, value, &profile->energy[at]) &&
						   (profile->energy[at] < 0 || profile->energy[at] > 0x10000)) {
					Aeron_LogWarn("xwa.models", "%s: %s must be from 0 to 65536. It is ignored", path, name);
					profile->energy[at] = -1;
				}
				break;
			default: {
				int32_t read;

				if (strcmp(key, "laser_link") == 0) {
					if (WeaponStats_ReadInt(path, name, value, &read) && read >= 0) {
						profile->laserLink = read;
					}
				} else if ((at = WeaponStats_Find(key, s_linkingRuleNames, XWA_WEAPON_LINKING_RULES)) < 0) {
					Aeron_LogWarn("xwa.models", "%s: %s isn't a linking key. It is ignored", path, name);
				} else if (WeaponStats_ReadBool(path, name, value, &read)) {
					profile->linking[at] = (int8_t)read;
				}
				break;
			}
		}
	}
}

/* Reads the profiles and the hardpoint kinds of `weapons`, a `weapons` section
   or group, into `set`. */
static void WeaponStats_ReadSet(const char* path, const AeronConfigNode* weapons, WeaponStatsSet* set) {
	const AeronConfigNode* kinds = AeronConfigNode_MapGet(weapons, "hardpoint_kinds");
	int group;

	for (group = 0; group < WEAPON_STATS_GROUPS; ++group) {
		const AeronConfigNode* node = AeronConfigNode_MapGet(weapons, s_groups[group]);
		size_t index;

		if (node == NULL) {
			continue;
		}
		if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s: weapons.%s isn't a map. It is ignored", path, s_groups[group]);
			continue;
		}
		for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
			const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
			char name[WEAPON_STATS_NAME_MAX];
			char where[128];
			WeaponStatsProfile* profile;

			snprintf(where, sizeof where, "weapons.%s.%s", s_groups[group],
					 AeronConfigNode_MapKeyAt(node, index));
			if (!WeaponStats_ProfileName(AeronConfigNode_MapKeyAt(node, index), name) ||
				AeronConfigNode_Type(value) != AERON_CONFIG_MAP) {
				Aeron_LogWarn("xwa.models",
							  "%s: %s must be a map with a name of up to %d characters. It is ignored", path,
							  where, WEAPON_STATS_NAME_MAX - 1);
				continue;
			}
			if ((profile = WeaponStats_AddProfile(set, name)) == NULL) {
				Aeron_LogError("xwa.models", "%s: out of memory", path);
				return;
			}
			WeaponStats_ReadProfile(path, where, group, value, profile);
		}
	}
	if (kinds != NULL) {
		size_t index;

		if (AeronConfigNode_Type(kinds) != AERON_CONFIG_SEQUENCE ||
			AeronConfigNode_SequenceCount(kinds) > WEAPON_STATS_HARDPOINT_TYPES) {
			Aeron_LogWarn("xwa.models",
						  "%s: weapons.hardpoint_kinds must be a list of up to %d kinds. It is ignored", path,
						  WEAPON_STATS_HARDPOINT_TYPES);
			return;
		}
		memset(set->hardpointKinds, 0, sizeof set->hardpointKinds);
		for (index = 0; index < AeronConfigNode_SequenceCount(kinds); ++index) {
			const AeronConfigNode* node = AeronConfigNode_SequenceGet(kinds, index);
			const int64_t kind = AeronConfigNode_Int(node, -1);

			if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || kind < 0 || kind > 255) {
				Aeron_LogWarn("xwa.models",
							  "%s: weapons.hardpoint_kinds[%d] must be a number from 0 to 255. It is 0", path,
							  (int)index);
				continue;
			}
			set->hardpointKinds[index] = (uint8_t)kind;
		}
		set->hasHardpointKinds = 1;
	}
}

/* Reads a list of `count` float32 factors into `out`. Returns 0 when the node
   isn't such a list. */
static int WeaponStats_ReadFactors(const char* key, const AeronConfigNode* node, float* out, int count) {
	int index;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE ||
		AeronConfigNode_SequenceCount(node) != (size_t)count) {
		Aeron_LogWarn("xwa.models", "%s: weapons.%s must be a list of %d numbers. It is ignored",
					  WEAPON_STATS_FLIGHT_YAML, key, count);
		return 0;
	}
	for (index = 0; index < count; ++index) {
		const AeronConfigNode* value = AeronConfigNode_SequenceGet(node, (size_t)index);

		if (AeronConfigNode_Type(value) != AERON_CONFIG_INT &&
			AeronConfigNode_Type(value) != AERON_CONFIG_FLOAT) {
			Aeron_LogWarn("xwa.models", "%s: weapons.%s must be a list of %d numbers. It is ignored",
						  WEAPON_STATS_FLIGHT_YAML, key, count);
			return 0;
		}
		/* The importer writes enough digits to get the exact float32 value
		   back. */
		out[index] = AeronConfigNode_Type(value) == AERON_CONFIG_INT
						 ? (float)AeronConfigNode_Int(value, 0)
						 : (float)AeronConfigNode_Float(value, 0.0);
	}
	return 1;
}

void XwaWeaponStats_Init(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	const AeronConfigNode* node;

	WeaponStats_FreeSet(&s_defaults);
	s_hasRankFactors = 0;
	s_hasDifficultyFactors = 0;
	s_limitLinkedWarheadFireRate = 0;
	memset(s_linkingRules, 0, sizeof s_linkingRules);
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, WEAPON_STATS_FLIGHT_YAML, &file,
									&error)) {
		/* mission_backdrops.c logs a file that can't be read. */
		return;
	}
	root = AeronConfigFile_Root(file);
	section = AeronConfigNode_MapGet(root, "weapons");
	if (AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1 ||
		AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		AeronConfigFile_Destroy(file);
		return;
	}
	WeaponStats_ReadSet(WEAPON_STATS_FLIGHT_YAML, section, &s_defaults);
	if ((node = AeronConfigNode_MapGet(section, "fire_ratio_by_rank")) != NULL) {
		s_hasRankFactors =
			WeaponStats_ReadFactors("fire_ratio_by_rank", node, s_rankFactors, WEAPON_STATS_RANKS);
	}
	if ((node = AeronConfigNode_MapGet(section, "damage_by_difficulty")) != NULL) {
		s_hasDifficultyFactors = WeaponStats_ReadFactors("damage_by_difficulty", node, s_difficultyFactors,
														 WEAPON_STATS_DIFFICULTIES);
	}
	if ((node = AeronConfigNode_MapGet(section, "limit_linked_warhead_fire_rate")) != NULL) {
		int32_t value;

		if (WeaponStats_ReadBool(WEAPON_STATS_FLIGHT_YAML, "weapons.limit_linked_warhead_fire_rate", node,
								 &value)) {
			s_limitLinkedWarheadFireRate = value;
		}
	}
	if ((node = AeronConfigNode_MapGet(section, "linking_rules")) != NULL) {
		int rule;

		if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s: weapons.linking_rules isn't a map. It is ignored",
						  WEAPON_STATS_FLIGHT_YAML);
		}
		for (rule = 0; rule < XWA_WEAPON_LINKING_RULES; ++rule) {
			const AeronConfigNode* value = AeronConfigNode_MapGet(node, s_linkingRuleNames[rule]);
			char name[96];
			int32_t read;

			snprintf(name, sizeof name, "weapons.linking_rules.%s", s_linkingRuleNames[rule]);
			if (value != NULL && WeaponStats_ReadBool(WEAPON_STATS_FLIGHT_YAML, name, value, &read)) {
				s_linkingRules[rule] = (int8_t)read;
			}
		}
	}
	AeronConfigFile_Destroy(file);
}

void XwaWeaponStats_ClearModelSettings(void) {
	unsigned int objectType;

	for (objectType = 0; objectType < OBJ_Count; ++objectType) {
		WeaponStats_FreeSet(&s_models[objectType]);
	}
}

void XwaWeaponStats_ReadModelSettings(unsigned int objectType, const char* path,
									  const AeronConfigNode* group) {
	/* craft_weapons.c logs a `weapons` group that isn't a map. */
	if (objectType >= OBJ_Count || AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		return;
	}
	WeaponStats_ReadSet(path, group, &s_models[objectType]);
}

void XwaWeaponStats_ClearFlightGroups(void) {
	memset(s_flightGroupProfiles, 0, sizeof s_flightGroupProfiles);
}

int XwaWeaponStats_ReadFlightGroup(const char* path, const char* fgKey, int fgIdx,
								   const AeronConfigNode* entry) {
	int group;
	int set = 0;

	if (fgIdx < 0 || fgIdx >= WEAPON_STATS_FLIGHT_GROUPS) {
		return 0;
	}
	for (group = 0; group < WEAPON_STATS_GROUPS; ++group) {
		const AeronConfigNode* node = AeronConfigNode_MapGet(entry, s_profileKeys[group]);

		if (node == NULL) {
			continue;
		}
		if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING ||
			!WeaponStats_ProfileName(AeronConfigNode_String(node, ""), s_flightGroupProfiles[fgIdx][group])) {
			Aeron_LogWarn("xwa.mission",
						  "%s: flight group %s: %s must be a name of up to %d characters; it is ignored",
						  path, fgKey, s_profileKeys[group], WEAPON_STATS_NAME_MAX - 1);
			s_flightGroupProfiles[fgIdx][group][0] = '\0';
			continue;
		}
		++set;
	}
	return set;
}

/* Returns the values of the model of object type `objectType`, or NULL if it
   has none. */
static const WeaponStatsSet* WeaponStats_Model(unsigned int objectType) {
	XwaModelSettings_Ensure(objectType);
	return objectType < OBJ_Count &&
				   (s_models[objectType].profileCount != 0 || s_models[objectType].hasHardpointKinds)
			   ? &s_models[objectType]
			   : NULL;
}

static const WeaponStatsProfile* WeaponStats_FindProfile(const WeaponStatsSet* set, const char* name,
														 int group) {
	int index;

	if (set == NULL) {
		return NULL;
	}
	for (index = 0; index < set->profileCount; ++index) {
		if ((set->profiles[index].groups & (1u << group)) && strcmp(set->profiles[index].name, name) == 0) {
			return &set->profiles[index];
		}
	}
	return NULL;
}

/* Finds the profiles of group `group` for a craft of object type `objectType`
   and flight group `fgIdx`, in the model file and in flight/flight.yaml. If
   neither has the flight group's profile, it uses `default`. */
static void WeaponStats_Profiles(unsigned int objectType, int fgIdx, int group,
								 const WeaponStatsProfile** model, const WeaponStatsProfile** global) {
	const WeaponStatsSet* set = WeaponStats_Model(objectType);
	const char* name =
		fgIdx >= 0 && fgIdx < WEAPON_STATS_FLIGHT_GROUPS ? s_flightGroupProfiles[fgIdx][group] : "";

	*model = WeaponStats_FindProfile(set, name, group);
	*global = WeaponStats_FindProfile(&s_defaults, name, group);
	if (name[0] != '\0' && *model == NULL && *global == NULL) {
		*model = WeaponStats_FindProfile(set, "", group);
		*global = WeaponStats_FindProfile(&s_defaults, "", group);
	}
}

/* Returns `value` with the type and the limits that the hook gives the stat. */
static int WeaponStats_Cast(XwaWeaponStat stat, int32_t value) {
	switch (stat) {
		case XWA_WEAPON_STAT_DURATION_SECONDS:
		case XWA_WEAPON_STAT_DURATION_FRACTION:
			return (uint16_t)value;
		case XWA_WEAPON_STAT_SCORE:
		case XWA_WEAPON_STAT_SPEED:
			return (int16_t)value;
		case XWA_WEAPON_STAT_POWER_SPEED_PERCENT:
			return value < 0 ? 0 : value;
		case XWA_WEAPON_STAT_PRECISE:
			return value != 0;
		default:
			return value;
	}
}

static int WeaponStats_Lookup(unsigned int objectType, int fgIdx, int player, unsigned int weapon,
							  XwaWeaponStat stat, int stock) {
	const WeaponStatsProfile* profiles[2];
	int forms[4];
	int formCount = 0;
	int form;

	if (weapon < WEAPON_STATS_FIRST_WEAPON || weapon >= WEAPON_STATS_FIRST_WEAPON + WEAPON_STATS_WEAPONS ||
		stat >= XWA_WEAPON_STATS) {
		return stock;
	}
	WeaponStats_Profiles(objectType, fgIdx, WEAPON_STATS_GROUP_STATS, &profiles[0], &profiles[1]);
	if (profiles[0] == NULL && profiles[1] == NULL) {
		return stock;
	}
	/* The forms for the player and for the difficulty come first. */
	if (player) {
		if (g_flightDifficulty < WEAPON_STATS_DIFFICULTIES) {
			forms[formCount++] = WEAPON_STATS_FORM_PLAYER_EASY + g_flightDifficulty;
		}
		forms[formCount++] = WEAPON_STATS_FORM_PLAYER;
	}
	if (g_flightDifficulty < WEAPON_STATS_DIFFICULTIES) {
		forms[formCount++] = WEAPON_STATS_FORM_EASY + g_flightDifficulty;
	}
	forms[formCount++] = WEAPON_STATS_FORM_PLAIN;
	for (form = 0; form < formCount; ++form) {
		int at;

		for (at = 0; at < 2; ++at) {
			const WeaponStatsForm* values;

			if (profiles[at] == NULL) {
				continue;
			}
			values = &profiles[at]->weapons[weapon - WEAPON_STATS_FIRST_WEAPON][forms[form]];
			if (values->set & (1u << stat)) {
				return WeaponStats_Cast(stat, values->value[stat]);
			}
		}
	}
	return stock;
}

int XwaWeaponStats_Value(int srcObjIdx, unsigned int weapon, XwaWeaponStat stat, int stock) {
	const ObjectRecord* object;

	if (srcObjIdx < 0) {
		return stock;
	}
	object = &g_objectTable[srcObjIdx];
	return WeaponStats_Lookup(object->objectType, object->flightGroupIdx, object->playerOwnerIdx != -1,
							  weapon, stat, stock);
}

int XwaWeaponStats_TypeValue(unsigned int objectType, unsigned int weapon, XwaWeaponStat stat, int stock) {
	return WeaponStats_Lookup(objectType, -1, 0, weapon, stat, stock);
}

uint16_t XwaWeaponStats_WarheadCapacity(unsigned int objIdx, int warheadType, uint16_t capacity,
										uint16_t fracQ16) {
	const WeaponStatsProfile* profiles[2];
	int at;

	if (warheadType >= 0 && warheadType < WEAPON_STATS_WARHEADS) {
		WeaponStats_Profiles(g_objectTable[objIdx].objectType, g_objectTable[objIdx].flightGroupIdx,
							 WEAPON_STATS_GROUP_WARHEADS, &profiles[0], &profiles[1]);
		for (at = 0; at < 2; ++at) {
			if (profiles[at] != NULL && profiles[at]->warheadPercent[warheadType] >= 0) {
				return (uint16_t)(((int64_t)capacity * profiles[at]->warheadPercent[warheadType] + 50) / 100);
			}
		}
	}
	return MATH2_fraction(capacity, fracQ16);
}

int XwaWeaponStats_EnergySpeedStep(unsigned int objIdx, int stock) {
	const WeaponStatsProfile* profiles[2];
	int key;
	int at;

	WeaponStats_Profiles(g_objectTable[objIdx].objectType, g_objectTable[objIdx].flightGroupIdx,
						 WEAPON_STATS_GROUP_ENERGY, &profiles[0], &profiles[1]);
	for (key = g_objectTable[objIdx].playerOwnerIdx != -1; key >= 0; --key) {
		for (at = 0; at < 2; ++at) {
			if (profiles[at] != NULL && profiles[at]->energy[key] >= 0) {
				return profiles[at]->energy[key];
			}
		}
	}
	return stock;
}

int XwaWeaponStats_LaserLink(unsigned int objIdx, int stock) {
	const WeaponStatsProfile* profiles[2];
	int at;

	WeaponStats_Profiles(g_objectTable[objIdx].objectType, g_objectTable[objIdx].flightGroupIdx,
						 WEAPON_STATS_GROUP_LINKING, &profiles[0], &profiles[1]);
	for (at = 0; at < 2; ++at) {
		if (profiles[at] != NULL && profiles[at]->laserLink >= 0) {
			return profiles[at]->laserLink;
		}
	}
	return stock;
}

int XwaWeaponStats_LinkingRule(unsigned int objIdx, XwaWeaponLinkingRule rule) {
	const WeaponStatsProfile* profiles[2];
	int at;

	if (rule >= XWA_WEAPON_LINKING_RULES) {
		return 0;
	}
	WeaponStats_Profiles(g_objectTable[objIdx].objectType, g_objectTable[objIdx].flightGroupIdx,
						 WEAPON_STATS_GROUP_LINKING, &profiles[0], &profiles[1]);
	for (at = 0; at < 2; ++at) {
		if (profiles[at] != NULL && profiles[at]->linking[rule] >= 0) {
			return profiles[at]->linking[rule];
		}
	}
	return s_linkingRules[rule];
}

int XwaWeaponStats_HardpointKind(unsigned int objectType, int hardpointType, int stock) {
	const WeaponStatsSet* model = WeaponStats_Model(objectType);
	const WeaponStatsSet* set = model != NULL && model->hasHardpointKinds ? model : &s_defaults;

	if (!set->hasHardpointKinds) {
		return stock;
	}
	return hardpointType >= 0 && hardpointType < WEAPON_STATS_HARDPOINT_TYPES
			   ? set->hardpointKinds[hardpointType]
			   : 0;
}

int XwaWeaponStats_HardpointWeaponType(int hardpointType) {
	if (hardpointType >= 1 && hardpointType <= 18) {
		return 280 + hardpointType - 1;
	}
	if (hardpointType >= 33 && hardpointType <= 37) {
		return 301 + hardpointType - 33;
	}
	return hardpointType == 38 ? 307 : 0;
}

int XwaWeaponStats_WeaponHardpointType(int weapon) {
	if (weapon >= 301 && weapon <= 305) {
		return 33 + weapon - 301;
	}
	return weapon == 307 ? 38 : weapon - 279;
}

int XwaWeaponStats_HasFireRatioByRank(void) { return s_hasRankFactors; }

int XwaWeaponStats_FireRatioByRank(unsigned int objIdx, int fireRatio) {
	const unsigned int fgIdx = g_objectTable[objIdx].flightGroupIdx;
	unsigned int rank;
	float scaled;

	if (fgIdx >= WEAPON_STATS_FLIGHT_GROUPS) {
		return fireRatio;
	}
	rank = g_missionFlightGroups[fgIdx].fg.groupAI;
	if (rank >= WEAPON_STATS_RANKS) {
		return fireRatio;
	}
	/* The hook multiplies the 16-bit fire ratio in float32. */
	scaled = (float)(uint16_t)fireRatio * s_rankFactors[rank];
	return (int)scaled;
}

int XwaWeaponStats_DamageByDifficulty(int damage, int* out) {
	float scaled;

	if (!s_hasDifficultyFactors) {
		return 0;
	}
	scaled = (float)damage *
			 s_difficultyFactors[g_flightDifficulty < WEAPON_STATS_DIFFICULTIES ? g_flightDifficulty : 2];
	*out = (int)scaled;
	return 1;
}

int XwaWeaponStats_LimitLinkedWarheadFireRate(void) { return s_limitLinkedWarheadFireRate; }
