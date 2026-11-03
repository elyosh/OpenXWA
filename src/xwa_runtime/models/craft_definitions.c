#include "xwa_runtime/models/craft_definitions.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/assets/string_table.h"
#include "xwa_runtime/models/gunner_seats.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/models/names.h"
#include "xwa_runtime/models/units.h"
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
	/* The textureGroup of an object type without a list row: the free
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
	/* The new name from the file's `name` key, or NULL. */
	const char* name;
	/* The craft record that the file's craft_record and names keys write, or
	   -1 when it has neither. */
	int record;
	/* The craft record that the object type uses once the file applies. */
	int finalRecord;
	/* 0 for the spacecraft list, 1 for the equipment list. */
	int listIndex;
	/* The model key, or "" when the file has no model. */
	char model[XWA_MODEL_SOURCE_KEY_MAX];
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
static uint8_t s_gunnerCockpit[OBJ_Count];
static CraftNames* s_names;
static int s_nameCount;
/* The text of each SHIPLIST.TXT line after its ID, by craft type, read when
   a file first needs it. */
static char* s_shipListLines[XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT];
static int s_shipListRead;

typedef enum CraftFieldKind {
	CRAFT_FIELD_U8,
	CRAFT_FIELD_BOOL,
	CRAFT_FIELD_U16,
	CRAFT_FIELD_I16,
	CRAFT_FIELD_I32,
	/* A 16-bit weapon type, written as its name. `none` is 0. */
	CRAFT_FIELD_WEAPON,
	/* An 8-bit laser group kind, written as its name. */
	CRAFT_FIELD_LASER_KIND,
} CraftFieldKind;

/* A key of the file. A number field with a unit holds the game's value, and
   the file holds it in `unit`. */
typedef struct CraftField {
	const char* key;
	size_t offset;
	CraftFieldKind kind;
	XwaUnit unit;
} CraftField;

/* The craft_record keys of single values. */
static const CraftField s_recordFields[] = {
	{ "speed_mglt", offsetof(ModelDef, maxSpeed), CRAFT_FIELD_U16, XWA_UNIT_MGLT },
	{ "acceleration_mglt", offsetof(ModelDef, accelRate), CRAFT_FIELD_U16, XWA_UNIT_MGLT },
	{ "deceleration_mglt", offsetof(ModelDef, decelRate), CRAFT_FIELD_U16, XWA_UNIT_MGLT },
	{ "yaw_rate_degrees", offsetof(ModelDef, yawRate), CRAFT_FIELD_I16, XWA_UNIT_DEGREES },
	{ "roll_rate_degrees", offsetof(ModelDef, rollRate), CRAFT_FIELD_I16, XWA_UNIT_DEGREES },
	{ "pitch_rate_degrees", offsetof(ModelDef, pitchRate), CRAFT_FIELD_I16, XWA_UNIT_DEGREES },
	{ "auto_bank_percent", offsetof(ModelDef, autoBankFactor), CRAFT_FIELD_U16, XWA_UNIT_PERCENT_Q16 },
	{ "max_tumble_angle_degrees", offsetof(ModelDef, maxTumbleAngle), CRAFT_FIELD_U16, XWA_UNIT_DEGREES },
	{ "max_push_rate", offsetof(ModelDef, maxPushRate), CRAFT_FIELD_U16, XWA_UNIT_NONE },
	{ "hull", offsetof(ModelDef, hullStrength), CRAFT_FIELD_I32, XWA_UNIT_NONE },
	{ "shields", offsetof(ModelDef, shieldStrength), CRAFT_FIELD_I32, XWA_UNIT_NONE },
	{ "has_shields", offsetof(ModelDef, hasShields), CRAFT_FIELD_BOOL, XWA_UNIT_NONE },
	{ "has_hyperdrive", offsetof(ModelDef, hasHyperdrive), CRAFT_FIELD_BOOL, XWA_UNIT_NONE },
	{ "system_strength", offsetof(ModelDef, systemStrength), CRAFT_FIELD_U16, XWA_UNIT_NONE },
	{ "system_damage_hull_threshold", offsetof(ModelDef, systemDamageHullThreshold), CRAFT_FIELD_I32,
	  XWA_UNIT_NONE },
	{ "component_max_hp", offsetof(ModelDef, componentMaxHp), CRAFT_FIELD_U8, XWA_UNIT_NONE },
	{ "rating_weight", offsetof(ModelDef, ratingWeight), CRAFT_FIELD_U16, XWA_UNIT_NONE },
	{ "laser_converge_mode", offsetof(ModelDef, laserConvergeMode), CRAFT_FIELD_U8, XWA_UNIT_NONE },
	{ "countermeasures", offsetof(ModelDef, countermeasureCount), CRAFT_FIELD_U8, XWA_UNIT_NONE },
	{ "gunsight_offset_y", offsetof(ModelDef, gunsightOffsetY), CRAFT_FIELD_I32, XWA_UNIT_NONE },
	{ "ai_hits_to_evade", offsetof(ModelDef, field_13), CRAFT_FIELD_U8, XWA_UNIT_NONE },
};

/* The keys of one entry of `lasers`, and of `warheads`. */
static const CraftField s_laserFields[] = {
	{ "type", offsetof(ModelDef, laserGroupWeaponType), CRAFT_FIELD_WEAPON, XWA_UNIT_NONE },
	{ "count", offsetof(ModelDef, laserGroupSlotCount), CRAFT_FIELD_U8, XWA_UNIT_NONE },
	{ "kind", offsetof(ModelDef, laserGroupWeaponKind), CRAFT_FIELD_LASER_KIND, XWA_UNIT_NONE },
	{ "range_m", offsetof(ModelDef, laserGroupFireRange), CRAFT_FIELD_I32, XWA_UNIT_METRES },
	{ "cooldown_ms", offsetof(ModelDef, laserGroupFireCooldownTicks), CRAFT_FIELD_U16,
	  XWA_UNIT_MILLISECONDS },
};
static const CraftField s_warheadFields[] = {
	{ "type", offsetof(ModelDef, warheadLauncherType), CRAFT_FIELD_WEAPON, XWA_UNIT_NONE },
	{ "count", offsetof(ModelDef, warheadLauncherSlotCount), CRAFT_FIELD_U8, XWA_UNIT_NONE },
	{ "capacity", offsetof(ModelDef, warheadLauncherCapacity), CRAFT_FIELD_U8, XWA_UNIT_NONE },
};

/* The keys of a `turrets` entry. They are the gunner seat keys, plus
   `object_type`, the object type of the seat's cockpit model. */
static const char* const s_turretKeys[] = {
	"position_m",
	"mount_pitch_degrees",
	"mount_yaw_degrees",
	"aim_limit_pitch_degrees",
	"aim_limit_yaw_degrees",
	"object_type",
};

static const char* const s_topKeys[] = {
	"object_type", "name",         "model",       "list",         "genus", "family",
	"flags",       "craft_record", "sounds_like", "beam_allowed", "names",
};
static const char* const s_recordArrayKeys[] = { "lasers",        "warheads",     "turrets",
												 "cockpit_eye_m", "cockpit_name", "mesh_attach" };
static const char* const s_nameKeys[] = { "short", "long", "plural", "gender" };
static const char* const s_craftTypeKeys[] = { "craft_type", "object_type", "ship_list", "tech_room" };
static const char* const s_shipListKeys[] = { "name",     "category", "flyable", "known",
											  "skirmish", "hidden",   "icon" };
/* The tech room keys, their IDs and the size of their field in the tech room,
   which keeps one byte for the end of the text. */
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
		case CRAFT_FIELD_LASER_KIND:
			return *base;
		case CRAFT_FIELD_U16:
		case CRAFT_FIELD_WEAPON: {
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
									  const AeronConfigNode* node, uint8_t* base, CraftFieldKind kind,
									  XwaUnit unit) {
	static const int64_t mins[] = { 0, 0, 0, INT16_MIN, INT32_MIN };
	static const int64_t maxes[] = { UINT8_MAX, 1, UINT16_MAX, INT16_MAX, INT32_MAX };
	const int64_t old = CraftDefinitions_GetField(base, kind);
	int64_t value;
	int named;

	if (kind == CRAFT_FIELD_BOOL) {
		int flag;

		if (!CraftDefinitions_ReadBool(path, node, what, &flag)) {
			return;
		}
		value = flag;
	} else if (kind == CRAFT_FIELD_WEAPON) {
		const char* text = AeronConfigNode_String(node, NULL);

		if (text != NULL && strcmp(text, "none") == 0) {
			value = 0;
		} else if (XwaNames_Read(&g_xwaWeaponTypeNames, path, what, node, &named)) {
			value = named;
		} else {
			return;
		}
	} else if (kind == CRAFT_FIELD_LASER_KIND) {
		if (!XwaNames_Read(&g_xwaLaserKindNames, path, what, node, &named)) {
			return;
		}
		value = named;
	} else if (unit != XWA_UNIT_NONE) {
		int32_t game;

		if (!XwaUnits_Read(node, unit, mins[kind], maxes[kind], &game)) {
			Aeron_LogError("xwa.craft", "%s:%d: %s needs a number of %s from %.3f to %.3f. It is ignored",
						   path, AeronConfigNode_Line(node), what, XwaUnits_Name(unit),
						   XwaUnits_FromGame(unit, (double)mins[kind]),
						   XwaUnits_FromGame(unit, (double)maxes[kind]));
			return;
		}
		value = game;
	} else if (!CraftDefinitions_ReadInt(path, node, what, mins[kind], maxes[kind], &value)) {
		return;
	}
	switch (kind) {
		case CRAFT_FIELD_U8:
		case CRAFT_FIELD_BOOL:
		case CRAFT_FIELD_LASER_KIND:
			*base = (uint8_t)value;
			break;
		case CRAFT_FIELD_U16:
		case CRAFT_FIELD_WEAPON: {
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
	switch (kind) {
		case CRAFT_FIELD_U16:
		case CRAFT_FIELD_I16:
		case CRAFT_FIELD_WEAPON:
			return 2;
		case CRAFT_FIELD_I32:
			return 4;
		default:
			return 1;
	}
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
				CraftDefinitions_SetField(path, record, what, value, base, fields[f].kind, fields[f].unit);
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

/* Writes both turret seats from the `turrets` list. A seat that the list
   leaves out is emptied. */
static void CraftDefinitions_SetTurrets(const char* path, int record, const AeronConfigNode* node) {
	ModelDef* def = &g_modelDefs[record];
	size_t seat;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) > 2) {
		Aeron_LogError("xwa.craft",
					   "%s:%d: craft_record.turrets must be a list of at most 2 seats. It is ignored", path,
					   AeronConfigNode_Line(node));
		return;
	}
	for (seat = 0; seat < 2; ++seat) {
		const AeronConfigNode* entry =
			seat < AeronConfigNode_SequenceCount(node) ? AeronConfigNode_SequenceGet(node, seat) : NULL;
		const AeronConfigNode* objectTypeNode = AeronConfigNode_MapGet(entry, "object_type");
		int32_t value[XWA_GUNNER_SEAT_FIELDS] = { 0 };
		int objectType = 0;
		char what[48];
		size_t i;

		snprintf(what, sizeof what, "craft_record.turrets[%zu]", seat);
		if (entry != NULL && AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
			Aeron_LogError("xwa.craft", "%s:%d: %s must be a map. The seat is emptied", path,
						   AeronConfigNode_Line(entry), what);
			entry = NULL;
		}
		for (i = 0; entry != NULL && i < AeronConfigNode_MapCount(entry); ++i) {
			if (!CraftDefinitions_InList(AeronConfigNode_MapKeyAt(entry, i), s_turretKeys,
										 sizeof s_turretKeys / sizeof s_turretKeys[0])) {
				Aeron_LogError("xwa.craft", "%s:%d: %s isn't a valid key in %s. It is ignored", path,
							   AeronConfigNode_Line(AeronConfigNode_MapValueAt(entry, i)),
							   AeronConfigNode_MapKeyAt(entry, i), what);
			}
		}
		if ((entry != NULL && !XwaGunnerSeats_ReadPlacement(path, what, entry, value)) ||
			(objectTypeNode != NULL && !XwaNames_Read(&g_xwaObjectTypeNames, path, "turrets.object_type",
													  objectTypeNode, &objectType))) {
			Aeron_LogError("xwa.craft", "%s: %s is ignored", path, what);
			continue;
		}
		def->turretSeatPosX[seat] = (int16_t)value[XWA_GUNNER_SEAT_X];
		def->turretSeatPosY[seat] = (int16_t)value[XWA_GUNNER_SEAT_Y];
		def->turretSeatPosZ[seat] = (int16_t)value[XWA_GUNNER_SEAT_Z];
		def->turretMountAngleA[seat] = (int16_t)value[XWA_GUNNER_SEAT_MOUNT_ANGLE_A];
		def->turretMountAngleB[seat] = (int16_t)value[XWA_GUNNER_SEAT_MOUNT_ANGLE_B];
		def->turretAimLimitA[seat] = (int16_t)value[XWA_GUNNER_SEAT_AIM_LIMIT_A];
		def->turretAimLimitB[seat] = (int16_t)value[XWA_GUNNER_SEAT_AIM_LIMIT_B];
		def->turretObjectType[seat] = (uint16_t)objectType;
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
								  base + i * CraftDefinitions_FieldSize(kind), kind, XWA_UNIT_NONE);
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
									  (uint8_t*)def + s_recordFields[f].offset, s_recordFields[f].kind,
									  s_recordFields[f].unit);
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
		CraftDefinitions_SetTurrets(file->path, file->record, value);
	}
	value = AeronConfigNode_MapGet(node, "cockpit_eye_m");
	if (value != NULL) {
		int32_t eye[3];

		if (XwaGunnerSeats_ReadPosition(file->path, "craft_record.cockpit_eye_m", value, eye)) {
			def->cockpitEyeX = (int16_t)eye[0];
			def->cockpitEyeY = (int16_t)eye[1];
			def->cockpitEyeZ = (int16_t)eye[2];
		}
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
	const char* category = NULL;
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
	if (item != NULL && XwaNames_Read(&g_xwaShipListCategoryNames, path, "ship_list.category", item, &i)) {
		category = XwaNames_StockName(&g_xwaShipListCategoryTexts, i);
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

/* Returns whether `objectType` is a free slot, which has no list row and
   loads nothing. Slots 318 and 319 point to the spacecraft list, but their
   record flags say that they have no row. */
static int CraftDefinitions_IsFreeSlot(int objectType) {
	const ObjectTypeInfo* info = &g_objectTypes[objectType];

	return info->textureGroup == CRAFT_NO_LIST ||
		   (info->recordFlags & OBJECT_TYPE_RECORD_TEXTURE_BACKED) == 0;
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
	g_objectTypes[objectType].textureGroup = (uint16_t)list;
	g_objectTypes[objectType].frameCount = (uint16_t)added->row;
	g_objectTypes[objectType].recordFlags |= OBJECT_TYPE_RECORD_TEXTURE_BACKED;
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

/* Loads an object type file, checks its object type, and finds the craft
   record that it writes. Returns 0 when the file can't be used. */
static int CraftDefinitions_LoadFile(CraftFile* file) {
	AeronConfigError error;
	const AeronConfigNode* node;
	int objectType;

	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, file->path, &file->file, &error)) {
		Aeron_LogError("xwa.craft", "%s:%d: %s. The file is ignored", file->path, error.line, error.message);
		return 0;
	}
	file->root = AeronConfigFile_Root(file->file);
	if (AeronConfigNode_Type(file->root) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.craft", "%s: the file must be a map. It is ignored", file->path);
		return 0;
	}
	/* Renames apply only after all files are loaded, so `object_type` is
	   always a stock name. */
	node = AeronConfigNode_MapGet(file->root, "object_type");
	if (node == NULL || !XwaNames_Read(&g_xwaObjectTypeNames, file->path, "object_type", node, &objectType)) {
		Aeron_LogError("xwa.craft",
					   "%s: object_type must be the stock name of an object type. The file is "
					   "ignored",
					   file->path);
		return 0;
	}
	file->objectType = objectType;
	node = AeronConfigNode_MapGet(file->root, "name");
	if (node != NULL) {
		file->name = AeronConfigNode_String(node, NULL);
		if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING || !XwaNames_ValidName(file->name)) {
			Aeron_LogError("xwa.craft",
						   "%s:%d: name can only have lower-case letters, digits and '_', and at most %d "
						   "characters. It is ignored",
						   file->path, AeronConfigNode_Line(node), XWA_NAME_MAX - 1);
			file->name = NULL;
		}
	}
	file->finalRecord = g_objectTypes[file->objectType].modelDefIndex;
	file->record = -1;
	if (AeronConfigNode_MapGet(file->root, "craft_record") != NULL ||
		AeronConfigNode_MapGet(file->root, "names") != NULL) {
		if (file->finalRecord < 0 || file->finalRecord >= XWA_MODEL_DEF_COUNT) {
			Aeron_LogError("xwa.craft", "%s: object type %s has no craft record. The file is ignored",
						   file->path, XwaNames_StockName(&g_xwaObjectTypeNames, file->objectType));
			return 0;
		}
		file->record = file->finalRecord;
	}
	return 1;
}

/* Returns whether a stock object type other than `objectType` uses craft
   record `record`. `fileOf` gives the file of each object type, or -1. An
   object type with a file uses the file's final record. */
static int CraftDefinitions_RecordShared(const CraftFile* files, const int* fileOf, int record,
										 int objectType) {
	int other;

	for (other = 0; other < XWA_STOCK_OBJECT_TYPE_COUNT; ++other) {
		const int file = fileOf[other];
		const int used = file >= 0 ? files[file].finalRecord : g_objectTypes[other].modelDefIndex;

		if (other != objectType && used == record) {
			return 1;
		}
	}
	return 0;
}

/* When a file changes a craft record that other object types also use, moves
   the file's object type to an unused record. This way the file only changes
   its own object type. If no record is free, the file is ignored. */
static void CraftDefinitions_SplitRecords(CraftFile* files, size_t count) {
	int fileOf[XWA_STOCK_OBJECT_TYPE_COUNT];
	size_t a;
	int objectType;
	int record;

	for (objectType = 0; objectType < XWA_STOCK_OBJECT_TYPE_COUNT; ++objectType) {
		fileOf[objectType] = -1;
	}
	for (a = 0; a < count; ++a) {
		if (!files[a].ignored) {
			fileOf[files[a].objectType] = (int)a;
		}
	}
	for (a = 0; a < count; ++a) {
		if (files[a].ignored || files[a].record < 0 ||
			!CraftDefinitions_RecordShared(files, fileOf, files[a].record, files[a].objectType)) {
			continue;
		}
		for (record = 0; record < XWA_MODEL_DEF_COUNT; ++record) {
			if (!CraftDefinitions_RecordShared(files, fileOf, record, -1)) {
				break;
			}
		}
		if (record == XWA_MODEL_DEF_COUNT) {
			Aeron_LogError("xwa.craft",
						   "%s: object type %s shares craft record %d, and no other craft record is free. "
						   "The file is ignored",
						   files[a].path, XwaNames_StockName(&g_xwaObjectTypeNames, files[a].objectType),
						   files[a].record);
			files[a].ignored = 1;
			fileOf[files[a].objectType] = -1;
			continue;
		}
		Aeron_LogInfo("xwa.craft",
					  "%s: object type %s shares craft record %d, so it now uses craft record %d",
					  files[a].path, XwaNames_StockName(&g_xwaObjectTypeNames, files[a].objectType),
					  files[a].record, record);
		files[a].record = record;
		files[a].finalRecord = record;
	}
}

/* When two files change the same object type, ignores both files. */
static void CraftDefinitions_CheckOwners(CraftFile* files, size_t count) {
	size_t a;
	size_t b;

	for (a = 0; a < count; ++a) {
		if (files[a].ignored) {
			continue;
		}
		for (b = a + 1; b < count; ++b) {
			if (files[b].ignored) {
				continue;
			}
			if (files[a].objectType == files[b].objectType) {
				Aeron_LogError("xwa.craft", "%s and %s both change object type %s. Both files are ignored",
							   files[a].path, files[b].path,
							   XwaNames_StockName(&g_xwaObjectTypeNames, files[a].objectType));
				files[a].ignored = 1;
				files[b].ignored = 1;
			}
		}
	}
}

/* Replaces the settable flags with the flags listed in `flags`. If a name in
   the list isn't a settable flag, the key is ignored. */
static void CraftDefinitions_ApplyFlags(const CraftFile* file, const AeronConfigNode* node,
										ObjectTypeInfo* info) {
	uint16_t flags = 0;
	size_t i;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE) {
		Aeron_LogError("xwa.craft", "%s:%d: flags must be a list of flag names. It is ignored", file->path,
					   AeronConfigNode_Line(node));
		return;
	}
	for (i = 0; i < AeronConfigNode_SequenceCount(node); ++i) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, i);
		const char* name = AeronConfigNode_String(item, NULL);
		const int bit = XwaNames_Find(&g_xwaObjectTypeFlagNames, name);

		if (AeronConfigNode_Type(item) != AERON_CONFIG_STRING || bit < 0 ||
			(XWA_OBJECT_TYPE_SETTABLE_FLAGS & (1 << bit)) == 0) {
			Aeron_LogError("xwa.craft",
						   "%s:%d: '%s' isn't a flag that a file can set. Use player_targetable, "
						   "ai_targetable, nav_buoy, static_object, float_hardpoints, spins or "
						   "collision_face_groups. The flags key is ignored",
						   file->path, AeronConfigNode_Line(item), name != NULL ? name : "");
			return;
		}
		flags |= (uint16_t)(1u << bit);
	}
	info->flags = (uint16_t)((info->flags & ~XWA_OBJECT_TYPE_SETTABLE_FLAGS) | flags);
}

/* Returns the object type, other than `objectType`, that has the name
   `name`, or -1 when there is none. */
static int CraftDefinitions_NameOwner(const char* name, int objectType) {
	int other;

	for (other = 0; other < g_xwaObjectTypeNames.count; ++other) {
		const char* current = XwaNames_Name(&g_xwaObjectTypeNames, other);

		if (other != objectType && current != NULL && strcmp(current, name) == 0) {
			return other;
		}
	}
	return -1;
}

/* Renames the object types to the names in the files' `name` keys. If a name
   is already used by another object type, every file that uses this name
   loses its rename. This can give a stock name back, so the check runs again
   until every name is unique. */
static void CraftDefinitions_ApplyRenames(CraftFile* files, size_t count) {
	size_t a;
	size_t b;
	int changed = 1;

	for (a = 0; a < count; ++a) {
		if (!files[a].ignored && files[a].name != NULL) {
			XwaNames_RenameObjectType(files[a].objectType, files[a].name);
		}
	}
	while (changed) {
		changed = 0;
		for (a = 0; a < count; ++a) {
			const char* name = files[a].name;
			int owner;

			if (files[a].ignored || name == NULL) {
				continue;
			}
			owner = CraftDefinitions_NameOwner(name, files[a].objectType);
			if (owner < 0) {
				continue;
			}
			for (b = 0; b < count; ++b) {
				if (!files[b].ignored && files[b].name != NULL && strcmp(files[b].name, name) == 0) {
					if (b == a) {
						Aeron_LogError("xwa.craft",
									   "%s: object type %s already has the name '%s'. The name is ignored",
									   files[b].path, XwaNames_StockName(&g_xwaObjectTypeNames, owner), name);
					} else {
						Aeron_LogError("xwa.craft", "%s: %s also uses the name '%s'. The name is ignored",
									   files[b].path, files[a].path, name);
					}
					XwaNames_RenameObjectType(files[b].objectType, NULL);
					files[b].name = NULL;
				}
			}
			changed = 1;
		}
	}
	for (a = 0; a < count; ++a) {
		if (!files[a].ignored && files[a].name != NULL) {
			Aeron_LogVerbose("xwa.craft", "%s: object type %s is now %s", files[a].path,
							 XwaNames_StockName(&g_xwaObjectTypeNames, files[a].objectType), files[a].name);
		}
	}
}

/* Writes `genus`, `family` and `flags`. If CraftDefinitions_SplitRecords
   moved the object type to another craft record, copies the old record into
   it. */
static void CraftDefinitions_ApplyObjectType(const CraftFile* file) {
	ObjectTypeInfo* info = &g_objectTypes[file->objectType];
	const AeronConfigNode* node;
	int value;

	node = AeronConfigNode_MapGet(file->root, "genus");
	if (node != NULL && XwaNames_Read(&g_xwaGenusNames, file->path, "genus", node, &value)) {
		info->genusId = (uint8_t)value;
	}
	node = AeronConfigNode_MapGet(file->root, "family");
	if (node != NULL && XwaNames_Read(&g_xwaFamilyNames, file->path, "family", node, &value)) {
		info->familyId = (uint8_t)value;
	}
	node = AeronConfigNode_MapGet(file->root, "flags");
	if (node != NULL) {
		CraftDefinitions_ApplyFlags(file, node, info);
	}
	if (file->finalRecord != info->modelDefIndex) {
		g_modelDefs[file->finalRecord] = g_modelDefs[info->modelDefIndex];
		info->modelDefIndex = (int16_t)file->finalRecord;
	}
}

static void CraftDefinitions_ApplyRules(const CraftFile* file) {
	const AeronConfigNode* node;
	int objectType;
	int flag;

	node = AeronConfigNode_MapGet(file->root, "sounds_like");
	if (node != NULL && XwaNames_Read(&g_xwaObjectTypeNames, file->path, "sounds_like", node, &objectType)) {
		s_soundsLike[file->objectType] = (uint16_t)(objectType + 1);
	}
	node = AeronConfigNode_MapGet(file->root, "beam_allowed");
	if (node != NULL && CraftDefinitions_ReadBool(file->path, node, "beam_allowed", &flag)) {
		s_beamRule[file->objectType] = (uint8_t)(flag ? 1 : 2);
	}
}

/* Loads a craft type file and checks its craft type and object type.
   Returns 0 when the file can't be used. */
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
	if (AeronConfigNode_Type(file->root) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.craft", "%s: the file must be a map. It is ignored", file->path);
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
		int objectType;

		if (!XwaNames_Read(&g_xwaObjectTypeNames, file->path, "object_type", node, &objectType)) {
			Aeron_LogError("xwa.craft", "%s: the file is ignored", file->path);
			return 0;
		}
		file->objectType = objectType;
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
	/* A free slot that no object type file fills has no list row, and
	   neither has object type 0. The game would list a craft that it can't
	   load. */
	if (CraftDefinitions_IsFreeSlot(objectType)) {
		Aeron_LogError("xwa.craft",
					   "%s: craft type %d would use object type %s, which loads nothing. The file is "
					   "ignored",
					   file->path, file->craftType, XwaNames_Name(&g_xwaObjectTypeNames, objectType));
		return 0;
	}
	if (*mapped != (uint16_t)objectType) {
		Aeron_LogVerbose("xwa.craft", "craft type %d is now object type %s instead of %s", file->craftType,
						 XwaNames_Name(&g_xwaObjectTypeNames, objectType),
						 XwaNames_Name(&g_xwaObjectTypeNames, *mapped));
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
						   "object type %s sounds like object type %s, which sounds like another one. Its "
						   "sounds_like is ignored",
						   XwaNames_Name(&g_xwaObjectTypeNames, objectType),
						   XwaNames_Name(&g_xwaObjectTypeNames, s_soundsLike[objectType] - 1));
			s_soundsLike[objectType] = 0;
		}
	}
}

/* Checks the keys, the list and the model of `file`, and writes the list and
   the model into `file`. `rowCounts` holds the row counts of the two lists,
   or -1 when a list can't be read. Returns 0, after a log, when the file
   can't be used. */
static int CraftDefinitions_CheckFile(CraftFile* file, const int* rowCounts) {
	const AeronConfigNode* node;
	int hasModel;

	CraftDefinitions_CheckKeys(file->path, file->root, "", s_topKeys, sizeof s_topKeys / sizeof s_topKeys[0]);
	file->listIndex = 0;
	node = AeronConfigNode_MapGet(file->root, "list");
	if (node != NULL) {
		const char* text = AeronConfigNode_String(node, "");

		if (strcmp(text, "equipment") == 0) {
			file->listIndex = 1;
		} else if (strcmp(text, "spacecraft") != 0) {
			Aeron_LogError("xwa.craft", "%s:%d: list needs spacecraft or equipment. The file is ignored",
						   file->path, AeronConfigNode_Line(node));
			return 0;
		}
	}
	file->model[0] = '\0';
	hasModel = CraftDefinitions_ReadModel(file, file->model);
	if (hasModel < 0) {
		Aeron_LogError("xwa.craft", "%s: the file is ignored", file->path);
		return 0;
	}
	if (CraftDefinitions_IsFreeSlot(file->objectType) && !hasModel) {
		Aeron_LogError("xwa.craft",
					   "%s: object type %s is a free slot, so the file must give it a model. The file is "
					   "ignored",
					   file->path, XwaNames_StockName(&g_xwaObjectTypeNames, file->objectType));
		return 0;
	}
	if (hasModel && rowCounts[file->listIndex] < 0) {
		Aeron_LogError("xwa.craft", "%s: %s can't be read, so the model can't get a row. The file is ignored",
					   file->path, s_listPaths[file->listIndex]);
		return 0;
	}
	return 1;
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
		if (!files[i].ignored) {
			files[i].ignored = !CraftDefinitions_CheckFile(&files[i], rowCounts);
		}
	}
	CraftDefinitions_SplitRecords(files, list->count);
	CraftDefinitions_ApplyRenames(files, list->count);

	for (i = 0; i < list->count; ++i) {
		CraftFile* file = &files[i];
		const AeronConfigNode* node;
		const int hasModel = file->model[0] != '\0';

		if (file->ignored) {
			continue;
		}
		Aeron_LogVerbose("xwa.craft", "%s: object type %s: %s%s%s", file->path,
						 XwaNames_Name(&g_xwaObjectTypeNames, file->objectType),
						 CraftDefinitions_IsFreeSlot(file->objectType) ? "fills a free slot"
																	   : "changes a stock type",
						 hasModel ? ", model " : "", file->model);

		CraftDefinitions_ApplyObjectType(file);
		if (hasModel && !CraftDefinitions_AddRow(file->listIndex, file->objectType, file->model,
												 rowCounts[file->listIndex])) {
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

/* Adds gunner cockpit model `key` to `keys` if it isn't there yet. Skips the
   stock cockpit. Returns 0 when out of memory. */
static int CraftDefinitions_AddCockpitKey(CraftFileNames* keys, const char* key) {
	char normalized[XWA_MODEL_SOURCE_KEY_MAX];
	char** grown;
	size_t i;

	if (!XwaModelSource_NormalizeKey(key, normalized) ||
		strcmp(normalized, "corelliantransportgunner") == 0) {
		return 1;
	}
	for (i = 0; i < keys->count; ++i) {
		if (strcmp(keys->names[i], normalized) == 0) {
			return 1;
		}
	}
	grown = (char**)realloc(keys->names, (keys->count + 1) * sizeof *grown);
	if (grown == NULL) {
		return 0;
	}
	keys->names = grown;
	keys->names[keys->count] = (char*)malloc(strlen(normalized) + 1);
	if (keys->names[keys->count] == NULL) {
		return 0;
	}
	strcpy(keys->names[keys->count++], normalized);
	return 1;
}

/* Returns whether an object type other than `objectType` loads a model and
   uses craft record `record`. */
static int CraftDefinitions_RecordUsed(int record, int objectType) {
	int other;

	for (other = 0; other < OBJ_Count; ++other) {
		if (other != objectType && !CraftDefinitions_IsFreeSlot(other) &&
			g_objectTypes[other].modelDefIndex == record) {
			return 1;
		}
	}
	return 0;
}

/* Finds the cockpit models that the gunner seats in the model settings files
   use. Gives each one a hidden object type in a free slot, with a row in the
   equipment list, like the stock cockpit (type 317). The cockpit model then
   loads in every mission with a craft that has the seat, including Death Star
   missions. */
static void CraftDefinitions_AddGunnerCockpits(void) {
	CraftFileNames files = { NULL, 0 };
	CraftFileNames keys = { NULL, 0 };
	const int firstRow = CraftDefinitions_CountRows(s_listPaths[1]);
	int objectType = 1;
	size_t i;

	CraftDefinitions_ListFiles("flight/models", &files);
	for (i = 0; i < files.count; ++i) {
		char path[300];
		AeronConfigFile* file = NULL;
		const AeronConfigNode* seats;
		size_t seat;

		snprintf(path, sizeof path, "flight/models/%s", files.names[i]);
		if (!AeronConfigFile_LoadYaml(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, &file)) {
			continue;
		}
		seats = AeronConfigNode_MapGet(AeronConfigNode_MapGet(AeronConfigFile_Root(file), "gunner_seats"),
									   "seats");
		for (seat = 0; seat < AeronConfigNode_SequenceCount(seats); ++seat) {
			const char* key = AeronConfigNode_String(
				AeronConfigNode_MapGet(AeronConfigNode_SequenceGet(seats, seat), "cockpit"), NULL);

			if (key != NULL && !CraftDefinitions_AddCockpitKey(&keys, key)) {
				Aeron_LogError("xwa.craft", "out of memory for the gunner cockpits");
			}
		}
		AeronConfigFile_Destroy(file);
	}
	CraftDefinitions_FreeNames(&files);

	for (i = 0; i < keys.count; ++i) {
		char glb[XWA_MODEL_SOURCE_KEY_MAX + 32];
		XwaFile* stream;
		ObjectTypeInfo* info;

		snprintf(glb, sizeof glb, "flight/models/%s.glb", keys.names[i]);
		stream = File_Open(AERON_VFS_ROOT_MOD, glb, "rb");
		if (stream == NULL) {
			Aeron_LogError("xwa.craft", "gunner cockpit %s has no GLB (%s). Its seats can't be used",
						   keys.names[i], glb);
			continue;
		}
		File_Close(stream);
		while (objectType < XWA_STOCK_OBJECT_TYPE_COUNT &&
			   (!CraftDefinitions_IsFreeSlot(objectType) ||
				CraftDefinitions_RecordUsed(g_objectTypes[objectType].modelDefIndex, objectType) ||
				g_objectTypes[objectType].modelDefIndex < 0)) {
			++objectType;
		}
		if (objectType == XWA_STOCK_OBJECT_TYPE_COUNT || firstRow < 0) {
			Aeron_LogError("xwa.craft",
						   "no free object type is left for gunner cockpit %s. Its seats can't be used",
						   keys.names[i]);
			continue;
		}
		info = &g_objectTypes[objectType];
		info->genusId = 2;
		info->familyId = 6;
		info->flags = 0;
		/* Use the stock cockpit's asset flags, so that the model also loads in
		   Death Star missions. */
		info->assetFlags = 0x01;
		if (!CraftDefinitions_AddRow(1, objectType, keys.names[i], firstRow)) {
			Aeron_LogError("xwa.craft", "out of memory for the row of gunner cockpit %s", keys.names[i]);
			continue;
		}
		s_gunnerCockpit[objectType] = 1;
		Aeron_LogVerbose("xwa.craft", "gunner cockpit %s uses object type %s", keys.names[i],
						 XwaNames_StockName(&g_xwaObjectTypeNames, objectType));
	}
	CraftDefinitions_FreeNames(&keys);
}

/* Applies the craft type files `list` of s_craftTypesDir. It runs after the
   object type files, because they can give a free slot the row that a craft
   type needs. */
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

	XwaNames_ResetObjectTypeNames();
	CraftDefinitions_ListFiles(s_objectTypesDir, &objectTypes);
	CraftDefinitions_ListFiles(s_craftTypesDir, &craftTypes);
	if (objectTypes.count + craftTypes.count != 0 && !XwaModelSource_ReplacementsConfigured()) {
		Aeron_LogInfo("xwa.craft",
					  "model replacements are off, so the %zu files of %s and the %zu files of %s are "
					  "skipped",
					  objectTypes.count, s_objectTypesDir, craftTypes.count, s_craftTypesDir);
	} else {
		CraftDefinitions_ApplyObjectTypes(&objectTypes);
		CraftDefinitions_AddGunnerCockpits();
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

int XwaCraftDefinitions_GunnerCockpit(unsigned int objectType) {
	return objectType < OBJ_Count && s_gunnerCockpit[objectType] != 0;
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
