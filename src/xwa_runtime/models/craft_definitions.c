#include "xwa_runtime/models/craft_definitions.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/string_table.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/text/text_overrides.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

enum {
	CRAFT_LIST_COUNT = 2,
	/* The textureGroup of an object type without a list row: the reserved
	   slots. */
	CRAFT_NO_LIST = 0xFFFF,
	/* Craft records up to this one have the species record of the same
	   number (StringTable_GetModelDefIndexForSpeciesRecord). */
	CRAFT_LAST_NAMED_RECORD = 217,
	CRAFT_SHIP_LIST_LINE_MAX = 256,
};

static const char s_objectTypesDir[] = "flight/object_types";
static const char s_craftTypesDir[] = "flight/craft_types";
static const char* const s_listPaths[CRAFT_LIST_COUNT] = { "FlightModels\\Spacecraft0.LST",
														   "FlightModels\\Equipment0.LST" };

typedef struct CraftFile {
	char path[256];
	AeronConfigFile* file;
	const AeronConfigNode* root;
	int objectType;
	/* The craft record that the file's craft_record and names keys write, or
	   -1 when it has neither. */
	int record;
	/* The craft record that the object type uses once the file applies. */
	int finalRecord;
	int ignored;
} CraftFile;

typedef struct CraftTypeFile {
	char path[256];
	AeronConfigFile* file;
	const AeronConfigNode* root;
	int craftType;
	/* The object type of `object_type`, or -1 when the file leaves it out. */
	int objectType;
	int ignored;
} CraftTypeFile;

typedef struct CraftAddedRow {
	char line[XWA_MODEL_SOURCE_KEY_MAX + 32];
	int row;
} CraftAddedRow;

typedef struct CraftNames {
	int record;
	char* shortName;
	char* longName;
	char* plural;
	int gender; /* -1 when the file doesn't set it */
	char path[256];
} CraftNames;

static CraftAddedRow* s_addedRows[CRAFT_LIST_COUNT];
static int s_addedRowCount[CRAFT_LIST_COUNT];
/* 0 when the object type uses its own sounds, else the object type + 1. */
static uint16_t s_soundsLike[OBJ_Count];
/* 0 for the stock rule, 1 when a file allows the beam, 2 when it doesn't. */
static uint8_t s_beamRule[OBJ_Count];
static CraftNames* s_names;
static int s_nameCount;
/* The text of each SHIPLIST.TXT line after its ID, by craft type, read when
   a file first needs it. */
static char* s_shipListLines[XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT];
static int s_shipListRead;

static const char* const s_shipListCategories[] = {
	"Fighter",
	"Shuttle/Light Transport",
	"Utility Craft",
	"Container",
	"Freighter/Heavy Transport",
	"Starship",
	"Station",
	"Weapon emplacement",
	"Mine",
	"Satellite/Buoy",
	"Droid",
};

typedef enum CraftFieldKind {
	CRAFT_FIELD_U8,
	CRAFT_FIELD_BOOL,
	CRAFT_FIELD_U16,
	CRAFT_FIELD_I16,
	CRAFT_FIELD_I32,
} CraftFieldKind;

typedef struct CraftField {
	const char* key;
	size_t offset;
	CraftFieldKind kind;
} CraftField;

/* The craft_record keys of single values. */
static const CraftField s_recordFields[] = {
	{ "speed", offsetof(ModelDef, maxSpeed), CRAFT_FIELD_U16 },
	{ "acceleration", offsetof(ModelDef, accelRate), CRAFT_FIELD_U16 },
	{ "deceleration", offsetof(ModelDef, decelRate), CRAFT_FIELD_U16 },
	{ "yaw_rate", offsetof(ModelDef, yawRate), CRAFT_FIELD_I16 },
	{ "roll_rate", offsetof(ModelDef, rollRate), CRAFT_FIELD_I16 },
	{ "pitch_rate", offsetof(ModelDef, pitchRate), CRAFT_FIELD_I16 },
	{ "auto_bank", offsetof(ModelDef, autoBankFactor), CRAFT_FIELD_U16 },
	{ "max_tumble_angle", offsetof(ModelDef, maxTumbleAngle), CRAFT_FIELD_U16 },
	{ "max_push_rate", offsetof(ModelDef, maxPushRate), CRAFT_FIELD_U16 },
	{ "hull", offsetof(ModelDef, hullStrength), CRAFT_FIELD_I32 },
	{ "shields", offsetof(ModelDef, shieldStrength), CRAFT_FIELD_I32 },
	{ "has_shields", offsetof(ModelDef, hasShields), CRAFT_FIELD_BOOL },
	{ "has_hyperdrive", offsetof(ModelDef, hasHyperdrive), CRAFT_FIELD_BOOL },
	{ "system_strength", offsetof(ModelDef, systemStrength), CRAFT_FIELD_U16 },
	{ "system_damage_hull_threshold", offsetof(ModelDef, systemDamageHullThreshold), CRAFT_FIELD_I32 },
	{ "component_max_hp", offsetof(ModelDef, componentMaxHp), CRAFT_FIELD_U8 },
	{ "rating_weight", offsetof(ModelDef, ratingWeight), CRAFT_FIELD_U16 },
	{ "laser_converge_mode", offsetof(ModelDef, laserConvergeMode), CRAFT_FIELD_U8 },
	{ "countermeasures", offsetof(ModelDef, countermeasureCount), CRAFT_FIELD_U8 },
	{ "gunsight_offset_y", offsetof(ModelDef, gunsightOffsetY), CRAFT_FIELD_I32 },
	{ "ai_hits_to_evade", offsetof(ModelDef, field_13), CRAFT_FIELD_U8 },
};

/* The keys of one entry of `lasers`, and of `warheads`. */
static const CraftField s_laserFields[] = {
	{ "type", offsetof(ModelDef, laserGroupWeaponType), CRAFT_FIELD_U16 },
	{ "count", offsetof(ModelDef, laserGroupSlotCount), CRAFT_FIELD_U8 },
	{ "kind", offsetof(ModelDef, laserGroupWeaponKind), CRAFT_FIELD_U8 },
	{ "range", offsetof(ModelDef, laserGroupFireRange), CRAFT_FIELD_I32 },
	{ "cooldown", offsetof(ModelDef, laserGroupFireCooldownTicks), CRAFT_FIELD_U16 },
};
static const CraftField s_warheadFields[] = {
	{ "type", offsetof(ModelDef, warheadLauncherType), CRAFT_FIELD_U16 },
	{ "count", offsetof(ModelDef, warheadLauncherSlotCount), CRAFT_FIELD_U8 },
	{ "capacity", offsetof(ModelDef, warheadLauncherCapacity), CRAFT_FIELD_U8 },
};

/* The keys of one entry of `turrets`, for each of the two turret seats.
   `object_type` is the object type of the seat's model. */
static const CraftField s_turretFields[] = {
	{ "x", offsetof(ModelDef, turretSeatPosX), CRAFT_FIELD_I16 },
	{ "y", offsetof(ModelDef, turretSeatPosY), CRAFT_FIELD_I16 },
	{ "z", offsetof(ModelDef, turretSeatPosZ), CRAFT_FIELD_I16 },
	{ "mount_angle_a", offsetof(ModelDef, turretMountAngleA), CRAFT_FIELD_I16 },
	{ "mount_angle_b", offsetof(ModelDef, turretMountAngleB), CRAFT_FIELD_I16 },
	{ "object_type", offsetof(ModelDef, turretModelIndex), CRAFT_FIELD_U16 },
	{ "aim_limit_a", offsetof(ModelDef, turretAimLimitA), CRAFT_FIELD_I16 },
	{ "aim_limit_b", offsetof(ModelDef, turretAimLimitB), CRAFT_FIELD_I16 },
};

static const char* const s_topKeys[] = {
	"version",      "object_type", "model",        "list",  "model_type",
	"craft_record", "sounds_like", "beam_allowed", "names",
};
static const char* const s_modelTypeKeys[] = { "genus", "family", "flags", "craft_record_index" };
static const char* const s_recordArrayKeys[] = { "lasers",      "warheads",     "turrets",
												 "cockpit_eye", "cockpit_name", "mesh_attach" };
static const char* const s_nameKeys[] = { "short", "long", "plural", "gender" };
static const char* const s_craftTypeKeys[] = { "version", "craft_type", "object_type", "ship_list",
											   "tech_room" };
static const char* const s_shipListKeys[] = { "name",     "category", "flyable", "known",
											  "skirmish", "hidden",   "icon" };
/* The tech room keys, their IDs and the size of their field in the tech room
   (tech_library.h), which keeps one byte for the end of the text. */
static const struct {
	const char* key;
	const char* id;
	size_t size;
} s_techRoomFields[] = {
	{ "name", "NAME", 64 },         { "manufacturer", "MANU", 64 }, { "side", "SIDE", 64 },
	{ "description", "DESC", 256 }, { "crew", "CREW", 64 },
};

static int CraftDefinitions_InList(const char* key, const char* const* keys, size_t count) {
	size_t i;

	for (i = 0; i < count; ++i) {
		if (strcmp(key, keys[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

/* Logs each key of map `node` that isn't in `keys`. The key is then ignored. */
static void CraftDefinitions_CheckKeys(const char* path, const AeronConfigNode* node, const char* where,
									   const char* const* keys, size_t count) {
	size_t i;

	for (i = 0; i < AeronConfigNode_MapCount(node); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(node, i);

		if (!CraftDefinitions_InList(key, keys, count)) {
			Aeron_LogError("xwa.craft", "%s:%d: the file can't have the key %s%s%s. It is ignored", path,
						   AeronConfigNode_Line(AeronConfigNode_MapValueAt(node, i)), where,
						   where[0] ? "." : "", key);
		}
	}
}

/* Reads a whole number from `node`. Returns 0, after a log, when it isn't
   one from `min` to `max`. */
static int CraftDefinitions_ReadInt(const char* path, const AeronConfigNode* node, const char* what,
									int64_t min, int64_t max, int64_t* out) {
	if (AeronConfigNode_Type(node) == AERON_CONFIG_INT) {
		*out = AeronConfigNode_Int(node, 0);
	} else {
		Aeron_LogError("xwa.craft", "%s:%d: %s needs a whole number. It is ignored", path,
					   AeronConfigNode_Line(node), what);
		return 0;
	}
	if (*out < min || *out > max) {
		Aeron_LogError("xwa.craft", "%s:%d: %s is %lld, but it can only be %lld to %lld. It is ignored", path,
					   AeronConfigNode_Line(node), what, (long long)*out, (long long)min, (long long)max);
		return 0;
	}
	return 1;
}

static int CraftDefinitions_ReadBool(const char* path, const AeronConfigNode* node, const char* what,
									 int* out) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogError("xwa.craft", "%s:%d: %s needs true or false. It is ignored", path,
					   AeronConfigNode_Line(node), what);
		return 0;
	}
	*out = AeronConfigNode_Bool(node, 0);
	return 1;
}

/* Copies the text of `node` and turns it into Latin-1, the game's character
   set. Returns NULL, after a log, when it isn't text, is longer than
   `maxLength` bytes or has a character that Latin-1 doesn't have. */
static char* CraftDefinitions_ReadText(const char* path, const AeronConfigNode* node, const char* what,
									   size_t maxLength) {
	const char* text = AeronConfigNode_String(node, NULL);
	char* copy;

	if (text == NULL) {
		Aeron_LogError("xwa.craft", "%s:%d: %s isn't text. Put it in quotes. It is ignored", path,
					   AeronConfigNode_Line(node), what);
		return NULL;
	}
	copy = (char*)malloc(strlen(text) + 1);
	if (copy == NULL) {
		return NULL;
	}
	strcpy(copy, text);
	if (!XwaText_ToLatin1(copy)) {
		Aeron_LogError(
			"xwa.craft",
			"%s:%d: %s has a character that isn't in Latin-1, so the game can't show it. It is ignored", path,
			AeronConfigNode_Line(node), what);
		free(copy);
		return NULL;
	}
	if (strlen(copy) > maxLength) {
		Aeron_LogError("xwa.craft", "%s:%d: %s is longer than the game's %zu characters. It is ignored", path,
					   AeronConfigNode_Line(node), what, maxLength);
		free(copy);
		return NULL;
	}
	return copy;
}

static int64_t CraftDefinitions_GetField(const uint8_t* base, CraftFieldKind kind) {
	switch (kind) {
		case CRAFT_FIELD_U8:
		case CRAFT_FIELD_BOOL:
			return *base;
		case CRAFT_FIELD_U16: {
			uint16_t value;

			memcpy(&value, base, sizeof value);
			return value;
		}
		case CRAFT_FIELD_I16: {
			int16_t value;

			memcpy(&value, base, sizeof value);
			return value;
		}
		case CRAFT_FIELD_I32:
		default: {
			int32_t value;

			memcpy(&value, base, sizeof value);
			return value;
		}
	}
}

/* Reads the value of `node` for a field of `kind`, checks its range and
   writes it at `base`. */
static void CraftDefinitions_SetField(const char* path, int record, const char* what,
									  const AeronConfigNode* node, uint8_t* base, CraftFieldKind kind) {
	static const int64_t mins[] = { 0, 0, 0, INT16_MIN, INT32_MIN };
	static const int64_t maxes[] = { UINT8_MAX, 1, UINT16_MAX, INT16_MAX, INT32_MAX };
	const int64_t old = CraftDefinitions_GetField(base, kind);
	int64_t value;

	if (kind == CRAFT_FIELD_BOOL) {
		int flag;

		if (!CraftDefinitions_ReadBool(path, node, what, &flag)) {
			return;
		}
		value = flag;
	} else if (!CraftDefinitions_ReadInt(path, node, what, mins[kind], maxes[kind], &value)) {
		return;
	}
	switch (kind) {
		case CRAFT_FIELD_U8:
		case CRAFT_FIELD_BOOL:
			*base = (uint8_t)value;
			break;
		case CRAFT_FIELD_U16: {
			const uint16_t stored = (uint16_t)value;

			memcpy(base, &stored, sizeof stored);
			break;
		}
		case CRAFT_FIELD_I16: {
			const int16_t stored = (int16_t)value;

			memcpy(base, &stored, sizeof stored);
			break;
		}
		case CRAFT_FIELD_I32: {
			const int32_t stored = (int32_t)value;

			memcpy(base, &stored, sizeof stored);
			break;
		}
	}
	if (old != value) {
		Aeron_LogVerbose("xwa.craft", "craft record %d: %s %lld -> %lld", record, what, (long long)old,
						 (long long)value);
	}
}

static size_t CraftDefinitions_FieldSize(CraftFieldKind kind) {
	return kind == CRAFT_FIELD_U16 || kind == CRAFT_FIELD_I16 ? 2 : kind == CRAFT_FIELD_I32 ? 4 : 1;
}

/* Writes `lasers` or `warheads`: every group in order, and the groups that
   aren't listed are emptied. */
static void CraftDefinitions_SetGroups(const char* path, int record, const char* key,
									   const AeronConfigNode* node, const CraftField* fields,
									   size_t fieldCount, size_t groupCount) {
	uint8_t* def = (uint8_t*)&g_modelDefs[record];
	size_t group;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE ||
		AeronConfigNode_SequenceCount(node) > groupCount) {
		Aeron_LogError("xwa.craft",
					   "%s:%d: craft_record.%s needs a list of at most %zu groups. It is ignored", path,
					   AeronConfigNode_Line(node), key, groupCount);
		return;
	}
	for (group = 0; group < groupCount; ++group) {
		const AeronConfigNode* entry =
			group < AeronConfigNode_SequenceCount(node) ? AeronConfigNode_SequenceGet(node, group) : NULL;
		size_t f;

		if (entry != NULL && AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
			Aeron_LogError("xwa.craft", "%s:%d: group %zu of craft_record.%s needs a map. It is emptied",
						   path, AeronConfigNode_Line(entry), group + 1, key);
			entry = NULL;
		}
		for (f = 0; f < fieldCount; ++f) {
			uint8_t* base = def + fields[f].offset + group * CraftDefinitions_FieldSize(fields[f].kind);
			const AeronConfigNode* value = AeronConfigNode_MapGet(entry, fields[f].key);
			char what[64];

			snprintf(what, sizeof what, "%s[%zu].%s", key, group, fields[f].key);
			if (value == NULL) {
				memset(base, 0, CraftDefinitions_FieldSize(fields[f].kind));
			} else {
				CraftDefinitions_SetField(path, record, what, value, base, fields[f].kind);
			}
		}
		if (entry != NULL) {
			for (f = 0; f < AeronConfigNode_MapCount(entry); ++f) {
				const char* name = AeronConfigNode_MapKeyAt(entry, f);
				size_t known;

				for (known = 0; known < fieldCount && strcmp(fields[known].key, name) != 0; ++known) {
				}
				if (known == fieldCount) {
					Aeron_LogError("xwa.craft", "%s:%d: %s isn't a key of craft_record.%s. It is ignored",
								   path, AeronConfigNode_Line(AeronConfigNode_MapValueAt(entry, f)), name,
								   key);
				}
			}
		}
	}
}

/* Writes the values of the sequence `node` into `count` fields of `kind`. */
static void CraftDefinitions_SetArray(const char* path, int record, const char* key,
									  const AeronConfigNode* node, uint8_t* base, CraftFieldKind kind,
									  size_t count) {
	size_t i;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) != count) {
		Aeron_LogError("xwa.craft", "%s:%d: craft_record.%s needs a list of %zu numbers. It is ignored", path,
					   AeronConfigNode_Line(node), key, count);
		return;
	}
	for (i = 0; i < count; ++i) {
		char what[64];

		snprintf(what, sizeof what, "%s[%zu]", key, i);
		CraftDefinitions_SetField(path, record, what, AeronConfigNode_SequenceGet(node, i),
								  base + i * CraftDefinitions_FieldSize(kind), kind);
	}
}

static void CraftDefinitions_ApplyRecord(const CraftFile* file, const AeronConfigNode* node) {
	ModelDef* def = &g_modelDefs[file->record];
	const AeronConfigNode* value;
	size_t i;

	for (i = 0; i < AeronConfigNode_MapCount(node); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(node, i);
		size_t f;

		for (f = 0; f < sizeof s_recordFields / sizeof s_recordFields[0]; ++f) {
			if (strcmp(key, s_recordFields[f].key) == 0) {
				break;
			}
		}
		if (f < sizeof s_recordFields / sizeof s_recordFields[0]) {
			CraftDefinitions_SetField(file->path, file->record, key, AeronConfigNode_MapValueAt(node, i),
									  (uint8_t*)def + s_recordFields[f].offset, s_recordFields[f].kind);
		} else if (!CraftDefinitions_InList(key, s_recordArrayKeys,
											sizeof s_recordArrayKeys / sizeof s_recordArrayKeys[0])) {
			Aeron_LogError("xwa.craft", "%s:%d: the file can't have the key craft_record.%s. It is ignored",
						   file->path, AeronConfigNode_Line(AeronConfigNode_MapValueAt(node, i)), key);
		}
	}

	value = AeronConfigNode_MapGet(node, "lasers");
	if (value != NULL) {
		CraftDefinitions_SetGroups(file->path, file->record, "lasers", value, s_laserFields,
								   sizeof s_laserFields / sizeof s_laserFields[0], 3);
	}
	value = AeronConfigNode_MapGet(node, "warheads");
	if (value != NULL) {
		CraftDefinitions_SetGroups(file->path, file->record, "warheads", value, s_warheadFields,
								   sizeof s_warheadFields / sizeof s_warheadFields[0], 2);
	}
	value = AeronConfigNode_MapGet(node, "turrets");
	if (value != NULL) {
		CraftDefinitions_SetGroups(file->path, file->record, "turrets", value, s_turretFields,
								   sizeof s_turretFields / sizeof s_turretFields[0], 2);
	}
	value = AeronConfigNode_MapGet(node, "cockpit_eye");
	if (value != NULL) {
		int16_t eye[3] = { def->cockpitEyeX, def->cockpitEyeY, def->cockpitEyeZ };

		CraftDefinitions_SetArray(file->path, file->record, "cockpit_eye", value, (uint8_t*)eye,
								  CRAFT_FIELD_I16, 3);
		def->cockpitEyeX = eye[0];
		def->cockpitEyeY = eye[1];
		def->cockpitEyeZ = eye[2];
	}
	value = AeronConfigNode_MapGet(node, "mesh_attach");
	if (value != NULL) {
		CraftDefinitions_SetArray(file->path, file->record, "mesh_attach", value,
								  (uint8_t*)def->meshAttachData, CRAFT_FIELD_I32,
								  sizeof def->meshAttachData / sizeof def->meshAttachData[0]);
	}
	value = AeronConfigNode_MapGet(node, "cockpit_name");
	if (value != NULL) {
		/* The model source's keys `<name>cockpit` and `<name>exterior` must
		   fit in XWA_MODEL_SOURCE_KEY_MAX bytes. */
		char* name = CraftDefinitions_ReadText(file->path, value, "craft_record.cockpit_name",
											   XWA_MODEL_SOURCE_KEY_MAX - sizeof "exterior");

		if (name != NULL && strpbrk(name, "\\/.") != NULL) {
			Aeron_LogError("xwa.craft",
						   "%s:%d: craft_record.cockpit_name is a file name without a folder or "
						   "an extension. It is ignored",
						   file->path, AeronConfigNode_Line(value));
		} else if (name != NULL) {
			if (strcmp(def->name, name) != 0) {
				Aeron_LogVerbose("xwa.craft", "craft record %d: cockpit_name '%s' -> '%s'", file->record,
								 def->name, name);
			}
			snprintf(def->name, sizeof def->name, "%s", name);
		}
		free(name);
	}
}

/* Reads SHIPLIST.TXT once, for the stock lines of the ship list. */
static void CraftDefinitions_ReadShipList(void) {
	char line[512];
	XwaFile* stream;

	if (s_shipListRead) {
		return;
	}
	s_shipListRead = 1;
	stream = File_Open(AERON_VFS_ROOT_ASSET, "shiplist.txt", "r");
	if (stream == NULL) {
		return;
	}
	while (File_ReadLine(stream, line, sizeof line)) {
		char* end = line + strcspn(line, "\r\n");
		char* bang;
		long craftType;

		*end = '\0';
		if (strncmp(line, "!SPECIES_", 9) != 0 || (bang = strchr(line + 1, '!')) == NULL) {
			continue;
		}
		craftType = strtol(line + 9, &end, 10);
		if (end != bang || craftType <= 0 || craftType >= XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT ||
			s_shipListLines[craftType] != NULL) {
			continue;
		}
		s_shipListLines[craftType] = (char*)malloc(strlen(bang + 1) + 1);
		if (s_shipListLines[craftType] != NULL) {
			strcpy(s_shipListLines[craftType], bang + 1);
		}
	}
	File_Close(stream);
}

static void CraftDefinitions_Trim(char* text) {
	char* start = text;
	size_t length;

	while (*start == ' ' || *start == '\t') {
		++start;
	}
	memmove(text, start, strlen(start) + 1);
	length = strlen(text);
	while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t')) {
		text[--length] = '\0';
	}
}

/* Makes the SPECIES_<n> text override from `node`, over the line that the
   game would read today. `path` is the craft type file. */
static void CraftDefinitions_ApplyShipList(const char* path, int craftType, const AeronConfigNode* node) {
	char id[32];
	char base[CRAFT_SHIP_LIST_LINE_MAX];
	char* fields[13] = { NULL };
	char* token;
	char* name = NULL;
	char* category = NULL;
	char line[CRAFT_SHIP_LIST_LINE_MAX * 2];
	int fieldCount = 0;
	int hidden = 0;
	int flyable = 0;
	int known = 0;
	int skirmish = 0;
	int icon[4] = { 0 };
	int64_t value;
	const AeronConfigNode* item;
	const char* stock;
	int fromStrings;
	int i;
	int written;

	snprintf(id, sizeof id, "SPECIES_%d", craftType);
	CraftDefinitions_CheckKeys(path, node, "ship_list", s_shipListKeys,
							   sizeof s_shipListKeys / sizeof s_shipListKeys[0]);
	CraftDefinitions_ReadShipList();
	/* The line that the game would read today: text/strings.yaml's, else
	   SHIPLIST.TXT's. */
	stock = XwaText_Find(id);
	fromStrings = stock != NULL;
	if (stock == NULL) {
		stock = s_shipListLines[craftType];
	}
	snprintf(base, sizeof base, "%s", stock != NULL ? stock : "");
	/* Split the line as ShipList_Load does, so that empty fields are skipped. */
	for (token = strtok(base, ",\n"); token != NULL && fieldCount < 13; token = strtok(NULL, ",\n")) {
		fields[fieldCount++] = token;
	}
	for (i = 0; i < fieldCount; ++i) {
		CraftDefinitions_Trim(fields[i]);
	}
	if (fieldCount > 0) {
		hidden = fields[0][0] == '*';
	}
	if (fieldCount > 2) {
		flyable = strcmp(fields[2], "Flyable") == 0 ? 1 : strcmp(fields[2], "GunnerFlyable") == 0 ? 2 : 0;
	}
	known = fieldCount > 3 && strcmp(fields[3], "Known") == 0;
	skirmish = fieldCount > 4 && strcmp(fields[4], "Skirmish") == 0;
	for (i = 0; i < 4; ++i) {
		icon[i] = fieldCount > 9 + i ? atoi(fields[9 + i]) : 0;
	}

	item = AeronConfigNode_MapGet(node, "name");
	if (item != NULL) {
		name = CraftDefinitions_ReadText(path, item, "ship_list.name", CRAFT_SHIP_LIST_LINE_MAX - 1);
		if (name != NULL && strchr(name, ',') != NULL) {
			Aeron_LogError("xwa.craft", "%s:%d: ship_list.name can't have a comma. It is ignored", path,
						   AeronConfigNode_Line(item));
			free(name);
			name = NULL;
		}
	}
	item = AeronConfigNode_MapGet(node, "category");
	if (item != NULL) {
		category = CraftDefinitions_ReadText(path, item, "ship_list.category", 64);
		if (category != NULL && strchr(category, ',') != NULL) {
			Aeron_LogError("xwa.craft", "%s:%d: ship_list.category can't have a comma. It is ignored", path,
						   AeronConfigNode_Line(item));
			free(category);
			category = NULL;
		} else if (category != NULL &&
				   !CraftDefinitions_InList(category, s_shipListCategories,
											sizeof s_shipListCategories / sizeof s_shipListCategories[0])) {
			Aeron_LogWarn(
				"xwa.craft",
				"%s:%d: '%s' isn't one of the game's ship list categories, so the game leaves craft "
				"type %d out of the list",
				path, AeronConfigNode_Line(item), category, craftType);
		}
	}
	item = AeronConfigNode_MapGet(node, "flyable");
	if (item != NULL) {
		const char* text = AeronConfigNode_String(item, NULL);

		if (AeronConfigNode_Type(item) == AERON_CONFIG_BOOL) {
			flyable = AeronConfigNode_Bool(item, 0);
		} else if (text != NULL && strcmp(text, "gunner") == 0) {
			flyable = 2;
		} else {
			Aeron_LogError("xwa.craft", "%s:%d: ship_list.flyable needs true, false or gunner. It is ignored",
						   path, AeronConfigNode_Line(item));
		}
	}
	item = AeronConfigNode_MapGet(node, "known");
	if (item != NULL) {
		CraftDefinitions_ReadBool(path, item, "ship_list.known", &known);
	}
	item = AeronConfigNode_MapGet(node, "skirmish");
	if (item != NULL) {
		CraftDefinitions_ReadBool(path, item, "ship_list.skirmish", &skirmish);
	}
	item = AeronConfigNode_MapGet(node, "hidden");
	if (item != NULL) {
		CraftDefinitions_ReadBool(path, item, "ship_list.hidden", &hidden);
	}
	item = AeronConfigNode_MapGet(node, "icon");
	if (item != NULL) {
		if (AeronConfigNode_Type(item) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(item) != 4) {
			Aeron_LogError("xwa.craft",
						   "%s:%d: ship_list.icon needs [left, top, right, bottom]. It is ignored", path,
						   AeronConfigNode_Line(item));
		} else {
			int read[4];

			for (i = 0; i < 4; ++i) {
				if (!CraftDefinitions_ReadInt(path, AeronConfigNode_SequenceGet(item, i), "ship_list.icon", 0,
											  4096, &value)) {
					break;
				}
				read[i] = (int)value;
			}
			if (i == 4) {
				memcpy(icon, read, sizeof icon);
			}
		}
	}

	{
		const char* baseName = fieldCount > 0 ? fields[0] + (fields[0][0] == '*') : "";

		written = snprintf(line, sizeof line, "%s%s,%s,%s,%s,%s,%s,%s,%s,%s,%d,%d,%d,%d", hidden ? "*" : "",
						   name != NULL ? name : baseName,
						   category != NULL ? category : (fieldCount > 1 ? fields[1] : "Fighter"),
						   flyable == 1   ? "Flyable"
						   : flyable == 2 ? "GunnerFlyable"
										  : "Nonflyable",
						   known ? "Known" : "Unknown", skirmish ? "Skirmish" : "NoSkirmish",
						   fieldCount > 5 ? fields[5] : "0", fieldCount > 6 ? fields[6] : "0",
						   fieldCount > 7 ? fields[7] : "0", fieldCount > 8 ? fields[8] : "0", icon[0],
						   icon[1], icon[2], icon[3]);
	}
	free(name);
	free(category);
	if (written < 0 || written >= CRAFT_SHIP_LIST_LINE_MAX) {
		Aeron_LogError("xwa.craft",
					   "%s: the ship list line of craft type %d is longer than the game's %d "
					   "characters. It is ignored",
					   path, craftType, CRAFT_SHIP_LIST_LINE_MAX - 1);
		return;
	}
	if (fromStrings) {
		Aeron_LogInfo("xwa.craft", "%s: replaces the text of %s from text/strings.yaml", path, id);
	}
	if (!XwaText_Set(id, line)) {
		Aeron_LogError("xwa.craft", "%s: out of memory for the ship list of craft type %d", path, craftType);
	}
}

static void CraftDefinitions_ApplyTechRoom(const char* path, int craftType, const AeronConfigNode* node) {
	size_t f;

	for (f = 0; f < AeronConfigNode_MapCount(node); ++f) {
		const char* key = AeronConfigNode_MapKeyAt(node, f);
		size_t i;

		for (i = 0; i < sizeof s_techRoomFields / sizeof s_techRoomFields[0]; ++i) {
			if (strcmp(key, s_techRoomFields[i].key) == 0) {
				break;
			}
		}
		if (i == sizeof s_techRoomFields / sizeof s_techRoomFields[0]) {
			Aeron_LogError("xwa.craft", "%s:%d: the file can't have the key tech_room.%s. It is ignored",
						   path, AeronConfigNode_Line(AeronConfigNode_MapValueAt(node, f)), key);
			continue;
		}
		{
			char what[64];
			char id[32];
			char* text;

			snprintf(what, sizeof what, "tech_room.%s", key);
			snprintf(id, sizeof id, "SPEC_%d_%s", craftType, s_techRoomFields[i].id);
			text = CraftDefinitions_ReadText(path, AeronConfigNode_MapValueAt(node, f), what,
											 s_techRoomFields[i].size - 1);
			if (text == NULL) {
				continue;
			}
			if (XwaText_Find(id) != NULL) {
				Aeron_LogInfo("xwa.craft", "%s: replaces the text of %s from text/strings.yaml", path, id);
			}
			if (!XwaText_Set(id, text)) {
				Aeron_LogError("xwa.craft", "%s: out of memory for %s", path, id);
			}
			free(text);
		}
	}
}

/* Reads `names` for ApplyNames, which writes them after the string table
   loads. */
static void CraftDefinitions_ReadNames(const CraftFile* file, const AeronConfigNode* node) {
	CraftNames names;
	const AeronConfigNode* item;
	CraftNames* grown;

	memset(&names, 0, sizeof names);
	names.record = file->record;
	names.gender = -1;
	snprintf(names.path, sizeof names.path, "%s", file->path);
	CraftDefinitions_CheckKeys(file->path, node, "names", s_nameKeys,
							   sizeof s_nameKeys / sizeof s_nameKeys[0]);
	if (file->record > CRAFT_LAST_NAMED_RECORD) {
		Aeron_LogError(
			"xwa.craft",
			"%s: craft record %d has no species names of its own (only records 0 to %d do). Set them "
			"in text/strings.yaml instead. The names are ignored",
			file->path, file->record, CRAFT_LAST_NAMED_RECORD);
		return;
	}
	item = AeronConfigNode_MapGet(node, "short");
	if (item != NULL) {
		names.shortName = CraftDefinitions_ReadText(file->path, item, "names.short", 255);
	}
	item = AeronConfigNode_MapGet(node, "long");
	if (item != NULL) {
		names.longName = CraftDefinitions_ReadText(file->path, item, "names.long", 255);
	}
	item = AeronConfigNode_MapGet(node, "plural");
	if (item != NULL) {
		names.plural = CraftDefinitions_ReadText(file->path, item, "names.plural", 255);
	}
	item = AeronConfigNode_MapGet(node, "gender");
	if (item != NULL) {
		const char* gender = AeronConfigNode_String(item, "");

		names.gender = strcmp(gender, "m") == 0   ? 0
					   : strcmp(gender, "f") == 0 ? 1
					   : strcmp(gender, "n") == 0 ? 2
												  : -1;
		if (names.gender < 0) {
			Aeron_LogError("xwa.craft", "%s:%d: names.gender needs m, f or n. It is ignored", file->path,
						   AeronConfigNode_Line(item));
		}
	}
	grown = (CraftNames*)realloc(s_names, (size_t)(s_nameCount + 1) * sizeof *s_names);
	if (grown == NULL) {
		free(names.shortName);
		free(names.longName);
		free(names.plural);
		return;
	}
	s_names = grown;
	s_names[s_nameCount++] = names;
}

/* Reads `model` and checks that its GLB exists. Writes the key to `model`
   (XWA_MODEL_SOURCE_KEY_MAX bytes). Returns 0 when the file has no model,
   and -1, after a log, when it has one that can't be used. */
static int CraftDefinitions_ReadModel(const CraftFile* file, char* model) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(file->root, "model");
	const char* text = AeronConfigNode_String(node, NULL);
	char path[XWA_MODEL_SOURCE_KEY_MAX + 32];
	XwaFile* stream;
	size_t i;

	if (node == NULL) {
		return 0;
	}
	if (text == NULL || text[0] == '\0' || strlen(text) >= XWA_MODEL_SOURCE_KEY_MAX) {
		Aeron_LogError("xwa.craft", "%s:%d: model needs the name of a GLB in flight/models", file->path,
					   AeronConfigNode_Line(node));
		return -1;
	}
	for (i = 0; text[i] != '\0'; ++i) {
		const char c = (char)tolower((unsigned char)text[i]);

		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
			Aeron_LogError("xwa.craft", "%s:%d: model '%s' can only have letters, digits, '_' and '-'",
						   file->path, AeronConfigNode_Line(node), text);
			return -1;
		}
		model[i] = c;
	}
	model[i] = '\0';
	snprintf(path, sizeof path, "flight/models/%s.glb", model);
	stream = File_Open(AERON_VFS_ROOT_MOD, path, "rb");
	if (stream == NULL) {
		Aeron_LogError("xwa.craft", "%s:%d: model '%s' has no GLB (%s)", file->path,
					   AeronConfigNode_Line(node), model, path);
		return -1;
	}
	File_Close(stream);
	return 1;
}

/* Counts the rows of a list as FeDiskIo_LoadResources does: empty lines are
   skipped. Returns -1 when the list can't be read. */
static int CraftDefinitions_CountRows(const char* path) {
	char line[256];
	XwaFile* stream = File_Open(AERON_VFS_ROOT_ASSET, path, "rb");
	int count = 0;

	if (stream == NULL) {
		return -1;
	}
	while (File_ReadLine(stream, line, sizeof line)) {
		line[strcspn(line, "\r\n")] = '\0';
		count += line[0] != '\0';
	}
	File_Close(stream);
	return count;
}

static int CraftDefinitions_AddRow(int list, int objectType, const char* model, int firstRow) {
	CraftAddedRow* grown;
	CraftAddedRow* added;

	grown = (CraftAddedRow*)realloc(s_addedRows[list], (size_t)(s_addedRowCount[list] + 1) * sizeof *grown);
	if (grown == NULL) {
		return 0;
	}
	s_addedRows[list] = grown;
	added = &grown[s_addedRowCount[list]];
	added->row = firstRow + s_addedRowCount[list];
	snprintf(added->line, sizeof added->line, "FlightModels\\%s.opt", model);
	++s_addedRowCount[list];
	g_modelTypeTable[objectType].textureGroup = (uint16_t)list;
	g_modelTypeTable[objectType].frameCount = (uint16_t)added->row;
	g_modelTypeTable[objectType].recordFlags |= MODEL_TYPE_RECORD_TEXTURE_BACKED;
	return 1;
}

typedef struct CraftFileNames {
	char** names;
	size_t count;
} CraftFileNames;

static int CraftDefinitions_CollectFile(void* userdata, const XwaFileEntry* entry) {
	CraftFileNames* list = (CraftFileNames*)userdata;
	char** grown;

	if (entry->is_directory) {
		return 1;
	}
	grown = (char**)realloc(list->names, (list->count + 1) * sizeof *grown);
	if (grown == NULL) {
		return 0;
	}
	list->names = grown;
	list->names[list->count] = (char*)malloc(strlen(entry->name) + 1);
	if (list->names[list->count] == NULL) {
		return 0;
	}
	strcpy(list->names[list->count++], entry->name);
	return 1;
}

static int CraftDefinitions_CompareNames(const void* left, const void* right) {
	return strcmp(*(char* const*)left, *(char* const*)right);
}

/* Lists the YAML files of `dir` in the mod folder, sorted by name. */
static void CraftDefinitions_ListFiles(const char* dir, CraftFileNames* list) {
	char pattern[64];

	snprintf(pattern, sizeof pattern, "%s/*.yaml", dir);
	File_Glob(AERON_VFS_ROOT_MOD, pattern, AERON_VFS_GLOB_FILES | AERON_VFS_GLOB_CASE_INSENSITIVE,
			  CraftDefinitions_CollectFile, list);
	if (list->count > 1) {
		qsort(list->names, list->count, sizeof *list->names, CraftDefinitions_CompareNames);
	}
}

static void CraftDefinitions_FreeNames(CraftFileNames* list) {
	size_t i;

	for (i = 0; i < list->count; ++i) {
		free(list->names[i]);
	}
	free(list->names);
	list->names = NULL;
	list->count = 0;
}

/* Loads an object type file, checks its version and object type, and finds
   the craft record that it writes. Returns 0 when the file can't be used. */
static int CraftDefinitions_LoadFile(CraftFile* file) {
	AeronConfigError error;
	const AeronConfigNode* modelType;
	int64_t value;

	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, file->path, &file->file, &error)) {
		Aeron_LogError("xwa.craft", "%s:%d: %s. The file is ignored", file->path, error.line, error.message);
		return 0;
	}
	file->root = AeronConfigFile_Root(file->file);
	if (AeronConfigNode_Type(file->root) != AERON_CONFIG_MAP ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(file->root, "version"), 0) != 1) {
		Aeron_LogError("xwa.craft", "%s: the file needs 'version: 1'. It is ignored", file->path);
		return 0;
	}
	if (!CraftDefinitions_ReadInt(file->path, AeronConfigNode_MapGet(file->root, "object_type"),
								  "object_type", 0, OBJ_Count - 1, &value)) {
		Aeron_LogError("xwa.craft", "%s: the file needs an object_type. It is ignored", file->path);
		return 0;
	}
	file->objectType = (int)value;
	file->finalRecord = g_modelTypeTable[file->objectType].modelIndex;
	modelType = AeronConfigNode_MapGet(file->root, "model_type");
	if (AeronConfigNode_MapGet(modelType, "craft_record_index") != NULL) {
		if (!CraftDefinitions_ReadInt(file->path, AeronConfigNode_MapGet(modelType, "craft_record_index"),
									  "model_type.craft_record_index", 0, XWA_MODEL_DEF_COUNT - 1, &value)) {
			return 0;
		}
		file->finalRecord = (int)value;
	}
	file->record = -1;
	if (AeronConfigNode_MapGet(file->root, "craft_record") != NULL ||
		AeronConfigNode_MapGet(file->root, "names") != NULL) {
		if (file->finalRecord < 0 || file->finalRecord >= XWA_MODEL_DEF_COUNT) {
			Aeron_LogError("xwa.craft", "%s: object type %d has no craft record. The file is ignored",
						   file->path, file->objectType);
			return 0;
		}
		file->record = file->finalRecord;
	}
	return 1;
}

/* Ignores both files when two object type files set the same object type or
   craft record, and a file whose craft_record_index names a record that
   another object type uses. */
static void CraftDefinitions_CheckOwners(CraftFile* files, size_t count) {
	size_t a;
	size_t b;

	for (a = 0; a < count; ++a) {
		if (files[a].ignored) {
			continue;
		}
		for (b = a + 1; b < count; ++b) {
			char why[64] = "";

			if (files[b].ignored) {
				continue;
			}
			if (files[a].objectType == files[b].objectType) {
				snprintf(why, sizeof why, "object type %d", files[a].objectType);
			} else if (files[a].record >= 0 && files[a].record == files[b].record) {
				snprintf(why, sizeof why, "craft record %d", files[a].record);
			}
			if (why[0] != '\0') {
				Aeron_LogError("xwa.craft", "%s and %s both set %s. Both files are ignored", files[a].path,
							   files[b].path, why);
				files[a].ignored = 1;
				files[b].ignored = 1;
			}
		}
	}

	for (a = 0; a < count; ++a) {
		int objectType;

		if (files[a].ignored || AeronConfigNode_MapGet(AeronConfigNode_MapGet(files[a].root, "model_type"),
													   "craft_record_index") == NULL) {
			continue;
		}
		for (objectType = 0; objectType < OBJ_Count; ++objectType) {
			int record = g_modelTypeTable[objectType].modelIndex;

			if (objectType == files[a].objectType) {
				continue;
			}
			for (b = 0; b < count; ++b) {
				if (!files[b].ignored && files[b].objectType == objectType) {
					record = files[b].finalRecord;
				}
			}
			if (record == files[a].finalRecord) {
				Aeron_LogError("xwa.craft",
							   "%s: craft record %d is used by object type %d. Only a record that no other "
							   "object type uses can be given. The file is ignored",
							   files[a].path, files[a].finalRecord, objectType);
				files[a].ignored = 1;
				break;
			}
		}
	}
}

static void CraftDefinitions_ApplyModelType(const CraftFile* file, const AeronConfigNode* node) {
	ModelTypeInfo* info = &g_modelTypeTable[file->objectType];
	int64_t value;

	CraftDefinitions_CheckKeys(file->path, node, "model_type", s_modelTypeKeys,
							   sizeof s_modelTypeKeys / sizeof s_modelTypeKeys[0]);
	if (AeronConfigNode_MapGet(node, "genus") != NULL &&
		CraftDefinitions_ReadInt(file->path, AeronConfigNode_MapGet(node, "genus"), "model_type.genus", 0,
								 UINT8_MAX, &value)) {
		info->genusId = (uint8_t)value;
	}
	if (AeronConfigNode_MapGet(node, "family") != NULL &&
		CraftDefinitions_ReadInt(file->path, AeronConfigNode_MapGet(node, "family"), "model_type.family", 0,
								 UINT8_MAX, &value)) {
		info->familyId = (uint8_t)value;
	}
	if (AeronConfigNode_MapGet(node, "flags") != NULL &&
		CraftDefinitions_ReadInt(file->path, AeronConfigNode_MapGet(node, "flags"), "model_type.flags", 0,
								 UINT16_MAX, &value)) {
		info->flags = (uint16_t)value;
	}
	if (AeronConfigNode_MapGet(node, "craft_record_index") != NULL) {
		Aeron_LogInfo("xwa.craft", "object type %d uses craft record %d instead of %d", file->objectType,
					  file->finalRecord, info->modelIndex);
		info->modelIndex = (int16_t)file->finalRecord;
	}
}

static void CraftDefinitions_ApplyRules(const CraftFile* file) {
	const AeronConfigNode* node;
	int64_t value;
	int flag;

	node = AeronConfigNode_MapGet(file->root, "sounds_like");
	if (node != NULL && CraftDefinitions_ReadInt(file->path, node, "sounds_like", 0, OBJ_Count - 1, &value)) {
		s_soundsLike[file->objectType] = (uint16_t)(value + 1);
	}
	node = AeronConfigNode_MapGet(file->root, "beam_allowed");
	if (node != NULL && CraftDefinitions_ReadBool(file->path, node, "beam_allowed", &flag)) {
		s_beamRule[file->objectType] = (uint8_t)(flag ? 1 : 2);
	}
}

/* Loads a craft type file and checks its version, craft type and object
   type. Returns 0 when the file can't be used. */
static int CraftDefinitions_LoadCraftTypeFile(CraftTypeFile* file) {
	AeronConfigError error;
	const AeronConfigNode* node;
	int64_t value;

	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, file->path, &file->file, &error)) {
		Aeron_LogError("xwa.craft", "%s:%d: %s. The file is ignored", file->path, error.line, error.message);
		return 0;
	}
	file->root = AeronConfigFile_Root(file->file);
	if (AeronConfigNode_Type(file->root) != AERON_CONFIG_MAP ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(file->root, "version"), 0) != 1) {
		Aeron_LogError("xwa.craft", "%s: the file needs 'version: 1'. It is ignored", file->path);
		return 0;
	}
	if (!CraftDefinitions_ReadInt(file->path, AeronConfigNode_MapGet(file->root, "craft_type"), "craft_type",
								  1, XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT - 1, &value)) {
		Aeron_LogError("xwa.craft", "%s: the file needs a craft_type. It is ignored", file->path);
		return 0;
	}
	file->craftType = (int)value;
	file->objectType = -1;
	node = AeronConfigNode_MapGet(file->root, "object_type");
	if (node != NULL) {
		if (!CraftDefinitions_ReadInt(file->path, node, "object_type", 0, OBJ_Count - 1, &value)) {
			Aeron_LogError("xwa.craft", "%s: the file is ignored", file->path);
			return 0;
		}
		file->objectType = (int)value;
	}
	return 1;
}

/* Ignores both files when two craft type files set the same craft type. */
static void CraftDefinitions_CheckCraftTypeOwners(CraftTypeFile* files, size_t count) {
	size_t a;
	size_t b;

	for (a = 0; a < count; ++a) {
		if (files[a].ignored) {
			continue;
		}
		for (b = a + 1; b < count; ++b) {
			if (!files[b].ignored && files[a].craftType == files[b].craftType) {
				Aeron_LogError("xwa.craft", "%s and %s both set craft type %d. Both files are ignored",
							   files[a].path, files[b].path, files[a].craftType);
				files[a].ignored = 1;
				files[b].ignored = 1;
			}
		}
	}
}

/* Links the craft type to its object type and makes the text overrides of
   its ship list line and tech room entry. Returns 0 when the file is
   ignored. */
static int CraftDefinitions_ApplyCraftTypeFile(const CraftTypeFile* file) {
	uint16_t* mapped = &g_objectTypeTables.craftTypeToObjectType[file->craftType];
	const int objectType = file->objectType >= 0 ? file->objectType : *mapped;
	const AeronConfigNode* node;

	CraftDefinitions_CheckKeys(file->path, file->root, "", s_craftTypeKeys,
							   sizeof s_craftTypeKeys / sizeof s_craftTypeKeys[0]);
	/* A reserved slot that no object type file fills has no list row, and
	   neither has object type 0: the game would list a craft that it can't
	   load. */
	if (g_modelTypeTable[objectType].textureGroup == CRAFT_NO_LIST) {
		Aeron_LogError("xwa.craft",
					   "%s: craft type %d would use object type %d, which loads nothing. The file is "
					   "ignored",
					   file->path, file->craftType, objectType);
		return 0;
	}
	if (*mapped != (uint16_t)objectType) {
		Aeron_LogInfo("xwa.craft", "craft type %d now is object type %d instead of %d", file->craftType,
					  objectType, *mapped);
		*mapped = (uint16_t)objectType;
	}
	node = AeronConfigNode_MapGet(file->root, "ship_list");
	if (node != NULL && AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.craft", "%s:%d: ship_list needs a map. It is ignored", file->path,
					   AeronConfigNode_Line(node));
	} else if (node != NULL) {
		CraftDefinitions_ApplyShipList(file->path, file->craftType, node);
	}
	node = AeronConfigNode_MapGet(file->root, "tech_room");
	if (node != NULL && AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.craft", "%s:%d: tech_room needs a map. It is ignored", file->path,
					   AeronConfigNode_Line(node));
	} else if (node != NULL) {
		CraftDefinitions_ApplyTechRoom(file->path, file->craftType, node);
	}
	return 1;
}

/* Checks a chain of sounds_like: the object type that a type sounds like
   must use its own sounds. */
static void CraftDefinitions_CheckSounds(void) {
	int objectType;

	for (objectType = 0; objectType < OBJ_Count; ++objectType) {
		if (s_soundsLike[objectType] != 0 && s_soundsLike[s_soundsLike[objectType] - 1] != 0) {
			Aeron_LogError("xwa.craft",
						   "object type %d sounds like object type %d, which sounds like another one. Its "
						   "sounds_like is ignored",
						   objectType, s_soundsLike[objectType] - 1);
			s_soundsLike[objectType] = 0;
		}
	}
}

/* Applies the object type files `list` of s_objectTypesDir. */
static void CraftDefinitions_ApplyObjectTypes(const CraftFileNames* list) {
	CraftFile* files;
	int rowCounts[CRAFT_LIST_COUNT];
	size_t i;
	int applied = 0;

	if (list->count == 0) {
		return;
	}
	files = (CraftFile*)calloc(list->count, sizeof *files);
	if (files == NULL) {
		Aeron_LogError("xwa.craft", "out of memory for the files of %s", s_objectTypesDir);
		return;
	}
	for (i = 0; i < CRAFT_LIST_COUNT; ++i) {
		rowCounts[i] = CraftDefinitions_CountRows(s_listPaths[i]);
	}

	for (i = 0; i < list->count; ++i) {
		snprintf(files[i].path, sizeof files[i].path, "%s/%s", s_objectTypesDir, list->names[i]);
		files[i].ignored = !CraftDefinitions_LoadFile(&files[i]);
	}
	CraftDefinitions_CheckOwners(files, list->count);

	for (i = 0; i < list->count; ++i) {
		CraftFile* file = &files[i];
		const ModelTypeInfo* info;
		const AeronConfigNode* node;
		char model[XWA_MODEL_SOURCE_KEY_MAX];
		int hasModel;
		int listIndex = 0;

		if (file->ignored) {
			continue;
		}
		info = &g_modelTypeTable[file->objectType];
		CraftDefinitions_CheckKeys(file->path, file->root, "", s_topKeys,
								   sizeof s_topKeys / sizeof s_topKeys[0]);
		node = AeronConfigNode_MapGet(file->root, "list");
		if (node != NULL) {
			const char* text = AeronConfigNode_String(node, "");

			if (strcmp(text, "equipment") == 0) {
				listIndex = 1;
			} else if (strcmp(text, "spacecraft") != 0) {
				Aeron_LogError("xwa.craft", "%s:%d: list needs spacecraft or equipment. The file is ignored",
							   file->path, AeronConfigNode_Line(node));
				continue;
			}
		}
		hasModel = CraftDefinitions_ReadModel(file, model);
		if (hasModel < 0) {
			Aeron_LogError("xwa.craft", "%s: the file is ignored", file->path);
			continue;
		}
		if (info->textureGroup == CRAFT_NO_LIST && !hasModel) {
			Aeron_LogError("xwa.craft",
						   "%s: object type %d is a reserved slot, which needs a model. The file is "
						   "ignored",
						   file->path, file->objectType);
			continue;
		}
		if (hasModel && rowCounts[listIndex] < 0) {
			Aeron_LogError("xwa.craft",
						   "%s: %s can't be read, so the model can't get a row. The file is ignored",
						   file->path, s_listPaths[listIndex]);
			continue;
		}
		Aeron_LogInfo("xwa.craft", "%s: object type %d: %s%s%s", file->path, file->objectType,
					  info->textureGroup == CRAFT_NO_LIST ? "fills a reserved slot" : "changes a stock type",
					  hasModel ? ", model " : "", hasModel ? model : "");

		node = AeronConfigNode_MapGet(file->root, "model_type");
		if (node != NULL) {
			CraftDefinitions_ApplyModelType(file, node);
		}
		if (hasModel && !CraftDefinitions_AddRow(listIndex, file->objectType, model, rowCounts[listIndex])) {
			Aeron_LogError("xwa.craft", "%s: out of memory for the model's row", file->path);
		}
		node = AeronConfigNode_MapGet(file->root, "craft_record");
		if (node != NULL) {
			if (AeronConfigNode_Type(node) == AERON_CONFIG_MAP) {
				CraftDefinitions_ApplyRecord(file, node);
			} else {
				Aeron_LogError("xwa.craft", "%s:%d: craft_record needs a map. It is ignored", file->path,
							   AeronConfigNode_Line(node));
			}
		}
		node = AeronConfigNode_MapGet(file->root, "names");
		if (node != NULL) {
			CraftDefinitions_ReadNames(file, node);
		}
		CraftDefinitions_ApplyRules(file);
		++applied;
	}
	CraftDefinitions_CheckSounds();

	for (i = 0; i < list->count; ++i) {
		AeronConfigFile_Destroy(files[i].file);
	}
	free(files);
	Aeron_LogInfo("xwa.craft", "%s: %d of %zu files apply, with %d added rows", s_objectTypesDir, applied,
				  list->count, s_addedRowCount[0] + s_addedRowCount[1]);
}

/* Applies the craft type files `list` of s_craftTypesDir, after the object
   type files, which can give a reserved slot the row that a craft type
   needs. */
static void CraftDefinitions_ApplyCraftTypes(const CraftFileNames* list) {
	CraftTypeFile* files;
	size_t i;
	int applied = 0;

	if (list->count == 0) {
		return;
	}
	files = (CraftTypeFile*)calloc(list->count, sizeof *files);
	if (files == NULL) {
		Aeron_LogError("xwa.craft", "out of memory for the files of %s", s_craftTypesDir);
		return;
	}
	for (i = 0; i < list->count; ++i) {
		snprintf(files[i].path, sizeof files[i].path, "%s/%s", s_craftTypesDir, list->names[i]);
		files[i].ignored = !CraftDefinitions_LoadCraftTypeFile(&files[i]);
	}
	CraftDefinitions_CheckCraftTypeOwners(files, list->count);
	for (i = 0; i < list->count; ++i) {
		if (!files[i].ignored) {
			applied += CraftDefinitions_ApplyCraftTypeFile(&files[i]);
		}
	}

	for (i = 0; i < list->count; ++i) {
		AeronConfigFile_Destroy(files[i].file);
	}
	free(files);
	for (i = 0; i < XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT; ++i) {
		free(s_shipListLines[i]);
		s_shipListLines[i] = NULL;
	}
	s_shipListRead = 0;
	Aeron_LogInfo("xwa.craft", "%s: %d of %zu files apply", s_craftTypesDir, applied, list->count);
}

void XwaCraftDefinitions_Apply(void) {
	CraftFileNames objectTypes = { NULL, 0 };
	CraftFileNames craftTypes = { NULL, 0 };

	CraftDefinitions_ListFiles(s_objectTypesDir, &objectTypes);
	CraftDefinitions_ListFiles(s_craftTypesDir, &craftTypes);
	if (objectTypes.count + craftTypes.count != 0 && !XwaModelSource_ReplacementsConfigured()) {
		Aeron_LogInfo("xwa.craft",
					  "model replacements are off, so the %zu files of %s and the %zu files of %s are "
					  "skipped",
					  objectTypes.count, s_objectTypesDir, craftTypes.count, s_craftTypesDir);
	} else {
		CraftDefinitions_ApplyObjectTypes(&objectTypes);
		CraftDefinitions_ApplyCraftTypes(&craftTypes);
	}
	CraftDefinitions_FreeNames(&objectTypes);
	CraftDefinitions_FreeNames(&craftTypes);
}

void XwaCraftDefinitions_ApplyNames(void) {
	int i;

	for (i = 0; i < s_nameCount; ++i) {
		const CraftNames* names = &s_names[i];
		const int record = names->record;

		if (names->shortName != NULL) {
			g_strSpeciesNames[record] = names->shortName;
			g_modelDefs[record].shortName = names->shortName;
		}
		if (names->longName != NULL) {
			g_strSpeciesNamesLong[record] = names->longName;
			g_modelDefs[record].longName = names->longName;
		}
		if (names->plural != NULL) {
			g_strSpeciesNamesPlural[record] = names->plural;
		}
		if (names->gender == 1 && g_strGoalCondFeminine[0] == NULL) {
			Aeron_LogError("xwa.craft",
						   "%s: STRINGS.TXT has no goal texts for feminine craft, so names.gender "
						   "is ignored",
						   names->path);
		} else if (names->gender == 2 && g_strGoalCondNeutered[0] == NULL) {
			Aeron_LogError("xwa.craft",
						   "%s: STRINGS.TXT has no goal texts for neuter craft, so names.gender is "
						   "ignored",
						   names->path);
		} else if (names->gender >= 0) {
			g_craftGender[record] = (unsigned char)names->gender;
		}
	}
	if (s_nameCount != 0) {
		Aeron_LogInfo("xwa.craft", "names of %d craft records", s_nameCount);
	}
}

unsigned int XwaCraftDefinitions_SoundObjectType(unsigned int objectType) {
	return objectType < OBJ_Count && s_soundsLike[objectType] != 0
			   ? (unsigned int)s_soundsLike[objectType] - 1
			   : objectType;
}

int XwaCraftDefinitions_BeamAllowed(unsigned int objectType) {
	if (objectType < OBJ_Count && s_beamRule[objectType] != 0) {
		return s_beamRule[objectType] == 1;
	}
	return !(objectType == OBJ_XWing || objectType == OBJ_YWing || objectType == OBJ_BWing ||
			 objectType == OBJ_AWing || objectType == OBJ_Z95 || objectType == OBJ_TIEFighter);
}

int XwaCraftDefinitions_NextAddedRow(int list, int* rowIndex, char* line, size_t lineSize) {
	int i;

	if (list < 0 || list >= CRAFT_LIST_COUNT) {
		return 0;
	}
	for (i = 0; i < s_addedRowCount[list]; ++i) {
		const CraftAddedRow* added = &s_addedRows[list][i];

		if (added->row >= *rowIndex) {
			snprintf(line, lineSize, "%s", added->line);
			*rowIndex = added->row;
			return 1;
		}
	}
	return 0;
}
