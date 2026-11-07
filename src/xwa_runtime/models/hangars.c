#include "xwa_runtime/models/hangars.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/assets/opt_model.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/math/scalar.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/models/names.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. It reads the hangar settings
   of the mod and looks up the settings of the command ship in their
   layers. */

#define HANGARS_LAYERS 4
#define HANGARS_LIST_MAX 64
#define HANGARS_SWAPS_MAX 64
#define HANGARS_FLIGHT_GROUPS 192
#define HANGARS_FLOOR 0x7FFFFFFF
/* The largest length that a file can give, in game units. */
#define HANGARS_UNITS_MAX 0x3FFFFFFF

typedef char HangarsKey[XWA_MODEL_SOURCE_KEY_MAX];
typedef char xwa_hangars_flight_state_key[(sizeof(((XwaHangarsFlightState*)0)->shipKey) == sizeof(HangarsKey))
											  ? 1
											  : -1];

/* The settings of a droid. The first droid's fields start at FIELD_DROIDS,
   and the second droid's fields follow them. */
typedef enum HangarsDroidField {
	DROID_ENABLED,
	DROID_TYPE,
	DROID_MARKINGS,
	DROID_PROFILE,
	DROID_HEIGHT,
	DROID_MOVES,
	DROID_FLOOR,
	HANGARS_DROID_FIELDS
} HangarsDroidField;

/* The settings that a layer can set. The game takes each one from the first
   layer that sets it, so different settings can come from different layers. */
typedef enum HangarsField {
	FIELD_HANGAR_FLOOR,
	FIELD_HANGAR_INVERTED_FLOOR_HEIGHT,
	FIELD_HANGAR_MARKINGS,
	FIELD_SHUTTLE_ENABLED,
	FIELD_SHUTTLE_TYPE,
	FIELD_SHUTTLE_MARKINGS,
	FIELD_SHUTTLE_PROFILE,
	FIELD_SHUTTLE_POSITION,
	FIELD_SHUTTLE_HEIGHT,
	FIELD_SHUTTLE_HEADING,
	FIELD_SHUTTLE_FLOOR,
	FIELD_SHUTTLE_EXIT_SIDE,
	FIELD_SHUTTLE_STRAIGHT,
	FIELD_SHUTTLE_RISE,
	FIELD_DROIDS,
	FIELD_ROOF_CRANE_POSITION = FIELD_DROIDS + 2 * HANGARS_DROID_FIELDS,
	FIELD_ROOF_CRANE_AXIS,
	FIELD_ROOF_CRANE_TRAVEL,
	FIELD_PLAYER_OFFSET,
	FIELD_PLAYER_INVERTED_OFFSET,
	FIELD_PLAYER_RISE,
	FIELD_PLAYER_INVERTED_RISE,
	FIELD_PLAYER_STRAIGHT,
	FIELD_PLAYER_FLOOR,
	FIELD_PLAYER_INVERTED_FLOOR_CRAFTS,
	FIELD_PLAYER_CRAFT_OFFSETS,
	HANGARS_FIELDS
} HangarsField;

/* How a key's value is read. */
typedef enum HangarsKind {
	KIND_BOOL,
	KIND_FLOOR,        /* normal or inverted */
	KIND_METRES,       /* a length */
	KIND_METRES_2,     /* two lengths */
	KIND_METRES_3,     /* three lengths */
	KIND_DEGREES,      /* a 16-bit angle */
	KIND_TYPE,         /* an object type name */
	KIND_SHUTTLE_TYPE, /* an object type name, or default */
	KIND_MARKINGS,
	KIND_HANGAR_MARKINGS, /* markings, or command_ship */
	KIND_PROFILE,
	KIND_EXIT_SIDE,
	KIND_AXIS,
	KIND_TYPE_LIST,     /* a list of object type names */
	KIND_CRAFT_OFFSETS, /* a map of object type names to three lengths */
	KIND_SECTION,       /* a map of other keys */
} HangarsKind;

typedef struct HangarsKeyInfo {
	const char* name;
	HangarsKind kind;
	int field;
	const struct HangarsKeyInfo* keys; /* the keys of a KIND_SECTION */
	size_t keyCount;
} HangarsKeyInfo;

static const HangarsKeyInfo s_hangarKeys[] = {
	{ "floor", KIND_FLOOR, FIELD_HANGAR_FLOOR, NULL, 0 },
	{ "inverted_floor_height_m", KIND_METRES, FIELD_HANGAR_INVERTED_FLOOR_HEIGHT, NULL, 0 },
	{ "markings", KIND_HANGAR_MARKINGS, FIELD_HANGAR_MARKINGS, NULL, 0 },
};

static const HangarsKeyInfo s_shuttlePathKeys[] = {
	{ "exit_side", KIND_EXIT_SIDE, FIELD_SHUTTLE_EXIT_SIDE, NULL, 0 },
	{ "straight_m", KIND_METRES, FIELD_SHUTTLE_STRAIGHT, NULL, 0 },
	{ "rise_m", KIND_METRES, FIELD_SHUTTLE_RISE, NULL, 0 },
};

static const HangarsKeyInfo s_shuttleKeys[] = {
	{ "enabled", KIND_BOOL, FIELD_SHUTTLE_ENABLED, NULL, 0 },
	{ "type", KIND_SHUTTLE_TYPE, FIELD_SHUTTLE_TYPE, NULL, 0 },
	{ "markings", KIND_MARKINGS, FIELD_SHUTTLE_MARKINGS, NULL, 0 },
	{ "profile", KIND_PROFILE, FIELD_SHUTTLE_PROFILE, NULL, 0 },
	{ "position_m", KIND_METRES_2, FIELD_SHUTTLE_POSITION, NULL, 0 },
	{ "height_m", KIND_METRES, FIELD_SHUTTLE_HEIGHT, NULL, 0 },
	{ "heading_degrees", KIND_DEGREES, FIELD_SHUTTLE_HEADING, NULL, 0 },
	{ "floor", KIND_FLOOR, FIELD_SHUTTLE_FLOOR, NULL, 0 },
	{ "path", KIND_SECTION, 0, s_shuttlePathKeys, (sizeof s_shuttlePathKeys / sizeof s_shuttlePathKeys[0]) },
};

/* The droid keys. Their fields are offsets from the droid's first field. */
static const HangarsKeyInfo s_droidKeys[] = {
	{ "enabled", KIND_BOOL, DROID_ENABLED, NULL, 0 },
	{ "type", KIND_TYPE, DROID_TYPE, NULL, 0 },
	{ "markings", KIND_MARKINGS, DROID_MARKINGS, NULL, 0 },
	{ "profile", KIND_PROFILE, DROID_PROFILE, NULL, 0 },
	{ "height_m", KIND_METRES, DROID_HEIGHT, NULL, 0 },
	{ "moves", KIND_BOOL, DROID_MOVES, NULL, 0 },
	{ "floor", KIND_FLOOR, DROID_FLOOR, NULL, 0 },
};

static const HangarsKeyInfo s_roofCraneKeys[] = {
	{ "position_m", KIND_METRES_3, FIELD_ROOF_CRANE_POSITION, NULL, 0 },
	{ "axis", KIND_AXIS, FIELD_ROOF_CRANE_AXIS, NULL, 0 },
	{ "travel_m", KIND_METRES_2, FIELD_ROOF_CRANE_TRAVEL, NULL, 0 },
};

static const HangarsKeyInfo s_playerKeys[] = {
	{ "offset_m", KIND_METRES_3, FIELD_PLAYER_OFFSET, NULL, 0 },
	{ "inverted_offset_m", KIND_METRES_3, FIELD_PLAYER_INVERTED_OFFSET, NULL, 0 },
	{ "rise_m", KIND_METRES, FIELD_PLAYER_RISE, NULL, 0 },
	{ "inverted_rise_m", KIND_METRES, FIELD_PLAYER_INVERTED_RISE, NULL, 0 },
	{ "straight_m", KIND_METRES, FIELD_PLAYER_STRAIGHT, NULL, 0 },
	{ "floor", KIND_FLOOR, FIELD_PLAYER_FLOOR, NULL, 0 },
	{ "inverted_floor_crafts", KIND_TYPE_LIST, FIELD_PLAYER_INVERTED_FLOOR_CRAFTS, NULL, 0 },
	{ "craft_offsets_m", KIND_CRAFT_OFFSETS, FIELD_PLAYER_CRAFT_OFFSETS, NULL, 0 },
};

/* The camera keys that the hangar and the family base can set. The other
   shots follow an object or show the cockpit. */
static const int s_cameraKeys[2][XWA_HANGAR_CAMERA_KEYS] = {
	{ 0, 1, 1, 1, 0, 0, 1, 0, 0, 1 },
	{ 0, 1, 1, 1, 0, 0, 1, 1, 1, 1 },
};

typedef struct HangarsValue {
	uint8_t set;
	int32_t v[3];
	const char* text;            /* KIND_PROFILE */
	const AeronConfigNode* node; /* KIND_TYPE_LIST and KIND_CRAFT_OFFSETS */
} HangarsValue;

/* The crane arm and the parked objects of the hangar or the family base. */
typedef struct HangarsLayout {
	uint8_t present;
	uint8_t hasCraneArm;
	XwaHangarMapEntry craneArm;
	XwaHangarMapEntry* parked;
	int parkedCount;
} HangarsLayout;

typedef struct HangarsLayer HangarsLayer;

typedef struct HangarsIffLayer {
	int iff;
	HangarsLayer* layer;
} HangarsIffLayer;

struct HangarsLayer {
	HangarsValue values[HANGARS_FIELDS];
	HangarsLayout layouts[2];
	uint8_t cameraSet[2][XWA_HANGAR_CAMERA_KEYS];
	int32_t cameras[2][XWA_HANGAR_CAMERA_KEYS][3];
	const AeronConfigNode* swaps;
	const char* hangarModel; /* only in a ship's layer */
	HangarsIffLayer* iffs;
	int iffCount;
};

typedef struct HangarsShip {
	const char* key;
	HangarsLayer* layer;
} HangarsShip;

/* A player craft's settings, from `crafts`. */
typedef struct HangarsCraft {
	const char* key;
	uint8_t restSet[2];
	int32_t rest[2];
	uint8_t cameraSet[4];
	int32_t camera[4][3];
} HangarsCraft;

typedef struct HangarsSwap {
	HangarsKey source;
	HangarsKey target;
} HangarsSwap;

typedef struct HangarsLoadedModel {
	uint16_t handle;
	HangarsKey key; /* empty for the stock model */
} HangarsLoadedModel;

static const char s_settingsPath[] = "flight/hangars.yaml";
static AeronConfigFile* s_settings;
static HangarsLayer* s_default;
static HangarsShip* s_ships;
static int s_shipCount;
static HangarsCraft* s_crafts;
static int s_craftCount;
static AeronConfigFile* s_missionFile;
static HangarsLayer* s_mission;
static const AeronConfigNode* s_missionShips;

/* The resolved state. */
static int s_resolved;
/* Set when XwaHangars_Resolve picks the command ship, and cleared when the
   player leaves the hangar. Swaps apply only while it is set. */
static int s_inHangar;
static int s_provingGround;
static HangarsKey s_shipKey;
static int s_shipIff;
static uint8_t s_shipMarkings;
static const HangarsLayer* s_layers[HANGARS_LAYERS];
static int s_layerCount;
static XwaHangarObjects s_objects;
static XwaHangarPlayerCraft s_playerCrafts[HANGARS_LIST_MAX];
static uint16_t s_floorInvertedTypes[HANGARS_LIST_MAX];
static XwaHangarMapEntry* s_maps[2];
static int s_mapCounts[2];
static int s_mapPresent[2];
static XwaHangarMapEntry s_craneArms[2];
static int s_hasCraneArm[2];
static XwaHangarCamera s_cameras[2];
static HangarsSwap s_swaps[HANGARS_SWAPS_MAX];
static int s_swapCount;
static HangarsKey s_hangarModel;
static HangarsLoadedModel s_loaded[XWA_LOADED_MODEL_COUNT];

/* ---- values ------------------------------------------------------------- */

static int Hangars_IsNumber(const AeronConfigNode* node) {
	return AeronConfigNode_Type(node) == AERON_CONFIG_INT || AeronConfigNode_Type(node) == AERON_CONFIG_FLOAT;
}

static int Hangars_IsText(const AeronConfigNode* node, const char* text) {
	return AeronConfigNode_Type(node) == AERON_CONFIG_STRING &&
		   strcmp(AeronConfigNode_String(node, ""), text) == 0;
}

/* A length in metres, in game units (40.96 per metre). */
static int Hangars_ReadMetres(const AeronConfigNode* node, int32_t* out) {
	double units;

	if (!Hangars_IsNumber(node)) {
		return 0;
	}
	units = AeronConfigNode_Float(node, 0.0) * 40.96;
	if (!(units >= -HANGARS_UNITS_MAX && units <= HANGARS_UNITS_MAX)) {
		return 0;
	}
	*out = (int32_t)lround(units);
	return 1;
}

/* `count` lengths in metres. */
static int Hangars_ReadMetresList(const AeronConfigNode* node, size_t count, int32_t* out) {
	size_t index;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) != count) {
		return 0;
	}
	for (index = 0; index < count; ++index) {
		if (!Hangars_ReadMetres(AeronConfigNode_SequenceGet(node, index), &out[index])) {
			return 0;
		}
	}
	return 1;
}

/* An angle in degrees from -360 to 360, as a 16-bit angle. */
static int Hangars_ReadDegrees(const AeronConfigNode* node, int32_t* out) {
	const double degrees = AeronConfigNode_Float(node, 0.0);

	if (!Hangars_IsNumber(node) || !(degrees >= -360.0 && degrees <= 360.0)) {
		return 0;
	}
	*out = (int32_t)(lround(degrees * 65536.0 / 360.0) & 0xFFFF);
	return 1;
}

/* An object type name. */
static int Hangars_ReadObjectType(const AeronConfigNode* node, int32_t* out) {
	const int objectType = XwaNames_Find(&g_xwaObjectTypeNames, AeronConfigNode_String(node, ""));

	if (objectType <= 0 || objectType >= OBJ_Count) {
		return 0;
	}
	*out = objectType;
	return 1;
}

static int Hangars_ReadMarkings(const AeronConfigNode* node, int32_t* out) {
	const int64_t value = AeronConfigNode_Int(node, -1);

	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 0 || value > 255) {
		return 0;
	}
	*out = (int32_t)value;
	return 1;
}

/* Finds the text of `node` in `names` and returns its index. */
static int Hangars_ReadChoice(const AeronConfigNode* node, const char* const* names, int count,
							  int32_t* out) {
	int index;

	for (index = 0; index < count; ++index) {
		if (Hangars_IsText(node, names[index])) {
			*out = index;
			return 1;
		}
	}
	return 0;
}

static int Hangars_ReadFloor(const AeronConfigNode* node, int32_t* out) {
	static const char* const names[] = { "normal", "inverted" };

	return Hangars_ReadChoice(node, names, 2, out);
}

/* ---- reading ------------------------------------------------------------ */

/* Logs a value that can't be used. */
static void Hangars_WarnInvalidValue(const char* path, const AeronConfigNode* node, const char* where,
									 const char* key, const char* expected) {
	Aeron_LogWarn("xwa.models", "%s:%d: %s.%s must be %s. It is ignored", path, AeronConfigNode_Line(node),
				  where, key, expected);
}

static void Hangars_WarnUnknownKey(const char* path, const AeronConfigNode* node, const char* where,
								   const char* key) {
	Aeron_LogWarn("xwa.models", "%s:%d: %s.%s isn't a hangar setting. It is ignored", path,
				  AeronConfigNode_Line(node), where, key);
}

/* Reads `node`, a list of object type names, or a map of object type names
   to `[x, y, z]` (`offsets`), and logs the entries that can't be used. */
static void Hangars_WarnInvalidCraftEntries(const char* path, const AeronConfigNode* node, const char* where,
											const char* key, int offsets) {
	const size_t count = offsets ? AeronConfigNode_MapCount(node) : AeronConfigNode_SequenceCount(node);
	size_t index;

	for (index = 0; index < count; ++index) {
		const AeronConfigNode* item =
			offsets ? AeronConfigNode_MapValueAt(node, index) : AeronConfigNode_SequenceGet(node, index);
		const char* name = offsets ? AeronConfigNode_MapKeyAt(node, index) : AeronConfigNode_String(item, "");
		int32_t value[3];

		if (XwaNames_Find(&g_xwaObjectTypeNames, name) <= 0) {
			XwaNames_LogUnknown(&g_xwaObjectTypeNames, path, AeronConfigNode_Line(item), key, name);
		} else if (offsets && !Hangars_ReadMetresList(item, 3, value)) {
			Hangars_WarnInvalidValue(path, item, where, key, "a map of object types to [x, y, z] in metres");
		}
	}
	if (count > HANGARS_LIST_MAX) {
		Aeron_LogWarn("xwa.models", "%s:%d: %s.%s has more than %d crafts. The others are ignored", path,
					  AeronConfigNode_Line(node), where, key, HANGARS_LIST_MAX);
	}
}

/* Reads the value of `key` in `node` into `value`. Logs a value that can't
   be used, and leaves it unset. */
static void Hangars_ReadValue(const char* path, const char* where, const HangarsKeyInfo* key,
							  const AeronConfigNode* node, HangarsValue* value) {
	static const char* const exitSides[] = { "right", "top", "bottom" };
	static const char* const axes[] = { "x", "y", "z" };
	const char* expected = NULL;
	int ok = 0;

	memset(value, 0, sizeof *value);
	switch (key->kind) {
		case KIND_BOOL:
			ok = AeronConfigNode_Type(node) == AERON_CONFIG_BOOL;
			value->v[0] = AeronConfigNode_Bool(node, 0) != 0;
			expected = "true or false";
			break;
		case KIND_FLOOR:
			ok = Hangars_ReadFloor(node, value->v);
			expected = "normal or inverted";
			break;
		case KIND_METRES:
			ok = Hangars_ReadMetres(node, value->v);
			expected = "a length in metres";
			break;
		case KIND_METRES_2:
			ok = Hangars_ReadMetresList(node, 2, value->v);
			expected = "two lengths in metres";
			break;
		case KIND_METRES_3:
			ok = Hangars_ReadMetresList(node, 3, value->v);
			expected = "[x, y, z] in metres";
			break;
		case KIND_DEGREES:
			ok = Hangars_ReadDegrees(node, value->v);
			expected = "an angle from -360 to 360 degrees";
			break;
		case KIND_TYPE:
		case KIND_SHUTTLE_TYPE:
			if (key->kind == KIND_SHUTTLE_TYPE && Hangars_IsText(node, "default")) {
				ok = 1;
				value->v[0] = 0;
			} else if (!(ok = Hangars_ReadObjectType(node, value->v))) {
				XwaNames_LogUnknown(&g_xwaObjectTypeNames, path, AeronConfigNode_Line(node), key->name,
									AeronConfigNode_String(node, ""));
				return;
			}
			break;
		case KIND_MARKINGS:
			ok = Hangars_ReadMarkings(node, value->v);
			expected = "a number from 0 to 255";
			break;
		case KIND_HANGAR_MARKINGS:
			ok = Hangars_IsText(node, "command_ship") || Hangars_ReadMarkings(node, value->v);
			if (Hangars_IsText(node, "command_ship")) {
				value->v[0] = 0xFF;
			}
			expected = "command_ship or a number from 0 to 255";
			break;
		case KIND_PROFILE:
			value->text = AeronConfigNode_String(node, "");
			ok = value->text[0] != '\0' && strlen(value->text) < XWA_HANGAR_PROFILE_MAX;
			expected = "the name of an object profile";
			break;
		case KIND_EXIT_SIDE:
			ok = Hangars_ReadChoice(node, exitSides, 3, value->v);
			expected = "right, top or bottom";
			break;
		case KIND_AXIS:
			ok = Hangars_ReadChoice(node, axes, 3, value->v);
			expected = "x, y or z";
			break;
		case KIND_TYPE_LIST:
			ok = AeronConfigNode_Type(node) == AERON_CONFIG_SEQUENCE;
			if (ok) {
				Hangars_WarnInvalidCraftEntries(path, node, where, key->name, 0);
			}
			value->node = node;
			expected = "a list of object types";
			break;
		case KIND_CRAFT_OFFSETS:
			ok = AeronConfigNode_Type(node) == AERON_CONFIG_MAP ||
				 AeronConfigNode_Type(node) == AERON_CONFIG_NULL;
			if (ok) {
				Hangars_WarnInvalidCraftEntries(path, node, where, key->name, 1);
			}
			value->node = node;
			expected = "a map of object types to [x, y, z] in metres";
			break;
		case KIND_SECTION:
			return;
	}
	if (!ok) {
		Hangars_WarnInvalidValue(path, node, where, key->name, expected);
		memset(value, 0, sizeof *value);
		return;
	}
	value->set = 1;
}

/* Reads the keys of the map `node` into `layer`, adding `fieldOffset` to
   their fields. */
static void Hangars_ReadSection(const char* path, const AeronConfigNode* node, const char* where,
								const HangarsKeyInfo* keys, size_t keyCount, int fieldOffset,
								HangarsLayer* layer) {
	size_t index;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: %s must be a map. It is ignored", path,
					  AeronConfigNode_Line(node), where);
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* name = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
		size_t known;

		for (known = 0; known < keyCount && strcmp(keys[known].name, name) != 0; ++known) {
		}
		if (known == keyCount) {
			Hangars_WarnUnknownKey(path, value, where, name);
		} else if (keys[known].kind == KIND_SECTION) {
			char inner[160];

			snprintf(inner, sizeof inner, "%s.%s", where, name);
			Hangars_ReadSection(path, value, inner, keys[known].keys, keys[known].keyCount, fieldOffset,
								layer);
		} else {
			Hangars_ReadValue(path, where, &keys[known], value,
							  &layer->values[keys[known].field + fieldOffset]);
		}
	}
}

/* Reads a crane arm or a parked object. Returns 0 when it can't be used. */
static int Hangars_ReadLayoutEntry(const char* path, const AeronConfigNode* node, const char* where,
								   XwaHangarMapEntry* entry) {
	static const char* const keys[] = { "type",          "markings", "position_m", "heading_degrees",
										"pitch_degrees", "profile",  "floor" };
	const AeronConfigNode* position = AeronConfigNode_MapGet(node, "position_m");
	const AeronConfigNode* item;
	const char* profile;
	size_t index;
	size_t count;
	int32_t value;

	memset(entry, 0, sizeof *entry);
	entry->isFloorInverted = -1;
	strcpy(entry->objectProfile, "Default");
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: %s must be a map. It is ignored", path,
					  AeronConfigNode_Line(node), where);
		return 0;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* name = AeronConfigNode_MapKeyAt(node, index);
		size_t known;

		for (known = 0; known < (sizeof keys / sizeof keys[0]) && strcmp(keys[known], name) != 0; ++known) {
		}
		if (known == (sizeof keys / sizeof keys[0])) {
			Hangars_WarnUnknownKey(path, AeronConfigNode_MapValueAt(node, index), where, name);
		}
	}
	item = AeronConfigNode_MapGet(node, "type");
	if (item == NULL) {
		Hangars_WarnInvalidValue(path, node, where, "type", "an object type");
		return 0;
	}
	if (!Hangars_ReadObjectType(item, &value)) {
		XwaNames_LogUnknown(&g_xwaObjectTypeNames, path, AeronConfigNode_Line(item), "type",
							AeronConfigNode_String(item, ""));
		return 0;
	}
	entry->objectType = (uint16_t)value;
	count = AeronConfigNode_SequenceCount(position);
	if ((count != 2 && count != 3) || !Hangars_ReadMetresList(position, count, entry->position)) {
		Hangars_WarnInvalidValue(path, position ? position : node, where, "position_m",
								 "[x, y] on the floor, or [x, y, z], in metres");
		return 0;
	}
	if (count == 2) {
		entry->position[2] = HANGARS_FLOOR;
	}
	if ((item = AeronConfigNode_MapGet(node, "markings")) != NULL) {
		if (!Hangars_ReadMarkings(item, &value)) {
			Hangars_WarnInvalidValue(path, item, where, "markings", "a number from 0 to 255");
			return 0;
		}
		entry->markings = value;
	}
	if ((item = AeronConfigNode_MapGet(node, "heading_degrees")) != NULL) {
		if (!Hangars_ReadDegrees(item, &value)) {
			Hangars_WarnInvalidValue(path, item, where, "heading_degrees",
									 "an angle from -360 to 360 degrees");
			return 0;
		}
		entry->headingXY = (uint16_t)value;
	}
	if ((item = AeronConfigNode_MapGet(node, "pitch_degrees")) != NULL) {
		if (!Hangars_ReadDegrees(item, &value)) {
			Hangars_WarnInvalidValue(path, item, where, "pitch_degrees", "an angle from -360 to 360 degrees");
			return 0;
		}
		entry->headingZ = (uint16_t)value;
	}
	if ((item = AeronConfigNode_MapGet(node, "profile")) != NULL) {
		profile = AeronConfigNode_String(item, "");
		if (profile[0] == '\0' || strlen(profile) >= sizeof entry->objectProfile) {
			Hangars_WarnInvalidValue(path, item, where, "profile", "the name of an object profile");
			return 0;
		}
		strcpy(entry->objectProfile, profile);
	}
	if ((item = AeronConfigNode_MapGet(node, "floor")) != NULL) {
		if (!Hangars_ReadFloor(item, &value)) {
			Hangars_WarnInvalidValue(path, item, where, "floor", "normal or inverted");
			return 0;
		}
		entry->isFloorInverted = (int8_t)value;
	}
	return 1;
}

/* Reads `crane_arm` and `parked` of `node`. */
static void Hangars_ReadLayout(const char* path, const AeronConfigNode* node, const char* where,
							   HangarsLayout* layout) {
	const AeronConfigNode* craneArm = AeronConfigNode_MapGet(node, "crane_arm");
	const AeronConfigNode* parked = AeronConfigNode_MapGet(node, "parked");
	char inner[160];
	size_t index;

	layout->present = craneArm != NULL || parked != NULL;
	if (craneArm != NULL) {
		snprintf(inner, sizeof inner, "%s.crane_arm", where);
		layout->hasCraneArm = (uint8_t)Hangars_ReadLayoutEntry(path, craneArm, inner, &layout->craneArm);
	}
	if (parked == NULL) {
		return;
	}
	if (AeronConfigNode_Type(parked) != AERON_CONFIG_SEQUENCE) {
		Hangars_WarnInvalidValue(path, parked, where, "parked", "a list");
		return;
	}
	layout->parked =
		(XwaHangarMapEntry*)calloc(AeronConfigNode_SequenceCount(parked) + 1, sizeof *layout->parked);
	if (layout->parked == NULL) {
		return;
	}
	for (index = 0; index < AeronConfigNode_SequenceCount(parked); ++index) {
		snprintf(inner, sizeof inner, "%s.parked[%u]", where, (unsigned int)index);
		if (Hangars_ReadLayoutEntry(path, AeronConfigNode_SequenceGet(parked, index), inner,
									&layout->parked[layout->parkedCount])) {
			++layout->parkedCount;
		}
	}
}

/* Reads `cameras` of `node` for the hangar or the family base. */
static void Hangars_ReadCameras(const char* path, const AeronConfigNode* node, const char* where, int family,
								HangarsLayer* layer) {
	size_t index;

	if (node == NULL) {
		return;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Hangars_WarnInvalidValue(path, node, where, "cameras", "a map");
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* name = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* offset = AeronConfigNode_MapValueAt(node, index);
		char* end = NULL;
		const long key = strncmp(name, "key_", 4) == 0 ? strtol(name + 4, &end, 10) : -1;
		char inner[160];

		snprintf(inner, sizeof inner, "%s.cameras", where);
		if (key < 0 || key >= XWA_HANGAR_CAMERA_KEYS || end == NULL || *end != '\0' ||
			!s_cameraKeys[family][key]) {
			Aeron_LogWarn("xwa.models", "%s:%d: %s.%s isn't a camera key that can be set. It is ignored",
						  path, AeronConfigNode_Line(offset), inner, name);
		} else if (!Hangars_ReadMetresList(offset, 3, layer->cameras[family][key])) {
			Hangars_WarnInvalidValue(path, offset, inner, name, "[x, y, z] in metres");
		} else {
			layer->cameraSet[family][key] = 1;
		}
	}
}

/* Reads the layer `node`. `extraKeys` are the keys that the caller reads. */
static HangarsLayer* Hangars_ReadLayer(const char* path, const AeronConfigNode* node, const char* where,
									   const char* const* extraKeys) {
	static const char* const keys[] = { "hangar", "shuttle",   "droids", "roof_crane", "player",
										"swaps",  "crane_arm", "parked", "cameras",    "family_base" };
	static const char* const familyKeys[] = { "crane_arm", "parked", "cameras" };
	HangarsLayer* layer = (HangarsLayer*)calloc(1, sizeof *layer);
	const AeronConfigNode* section;
	char inner[160];
	size_t index;

	if (layer == NULL) {
		return NULL;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		if (AeronConfigNode_Type(node) != AERON_CONFIG_NULL) {
			Aeron_LogWarn("xwa.models", "%s:%d: %s must be a map. It is ignored", path,
						  AeronConfigNode_Line(node), where);
		}
		return layer;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* name = AeronConfigNode_MapKeyAt(node, index);
		const char* const* extra = extraKeys;
		size_t known;

		for (known = 0; known < (sizeof keys / sizeof keys[0]) && strcmp(keys[known], name) != 0; ++known) {
		}
		while (extra != NULL && *extra != NULL && strcmp(*extra, name) != 0) {
			++extra;
		}
		if (known == (sizeof keys / sizeof keys[0]) && (extra == NULL || *extra == NULL)) {
			Hangars_WarnUnknownKey(path, AeronConfigNode_MapValueAt(node, index), where, name);
		}
	}
	if ((section = AeronConfigNode_MapGet(node, "hangar")) != NULL) {
		snprintf(inner, sizeof inner, "%s.hangar", where);
		Hangars_ReadSection(path, section, inner, s_hangarKeys,
							(sizeof s_hangarKeys / sizeof s_hangarKeys[0]), 0, layer);
	}
	if ((section = AeronConfigNode_MapGet(node, "shuttle")) != NULL) {
		snprintf(inner, sizeof inner, "%s.shuttle", where);
		Hangars_ReadSection(path, section, inner, s_shuttleKeys,
							(sizeof s_shuttleKeys / sizeof s_shuttleKeys[0]), 0, layer);
	}
	if ((section = AeronConfigNode_MapGet(node, "droids")) != NULL) {
		if (AeronConfigNode_Type(section) != AERON_CONFIG_SEQUENCE ||
			AeronConfigNode_SequenceCount(section) > 2) {
			Hangars_WarnInvalidValue(path, section, where, "droids", "a list of one or two droids");
		} else {
			for (index = 0; index < AeronConfigNode_SequenceCount(section); ++index) {
				snprintf(inner, sizeof inner, "%s.droids[%u]", where, (unsigned int)index);
				Hangars_ReadSection(path, AeronConfigNode_SequenceGet(section, index), inner, s_droidKeys,
									(sizeof s_droidKeys / sizeof s_droidKeys[0]),
									FIELD_DROIDS + (int)index * HANGARS_DROID_FIELDS, layer);
			}
		}
	}
	if ((section = AeronConfigNode_MapGet(node, "roof_crane")) != NULL) {
		snprintf(inner, sizeof inner, "%s.roof_crane", where);
		Hangars_ReadSection(path, section, inner, s_roofCraneKeys,
							(sizeof s_roofCraneKeys / sizeof s_roofCraneKeys[0]), 0, layer);
	}
	if ((section = AeronConfigNode_MapGet(node, "player")) != NULL) {
		snprintf(inner, sizeof inner, "%s.player", where);
		Hangars_ReadSection(path, section, inner, s_playerKeys,
							(sizeof s_playerKeys / sizeof s_playerKeys[0]), 0, layer);
	}
	layer->swaps = AeronConfigNode_MapGet(node, "swaps");
	Hangars_ReadLayout(path, node, where, &layer->layouts[0]);
	Hangars_ReadCameras(path, AeronConfigNode_MapGet(node, "cameras"), where, 0, layer);
	if ((section = AeronConfigNode_MapGet(node, "family_base")) != NULL) {
		snprintf(inner, sizeof inner, "%s.family_base", where);
		if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
			Hangars_WarnInvalidValue(path, section, where, "family_base", "a map");
		} else {
			for (index = 0; index < AeronConfigNode_MapCount(section); ++index) {
				const char* name = AeronConfigNode_MapKeyAt(section, index);
				size_t known;

				for (known = 0; known < (sizeof familyKeys / sizeof familyKeys[0]) &&
								strcmp(familyKeys[known], name) != 0;
					 ++known) {
				}
				if (known == (sizeof familyKeys / sizeof familyKeys[0])) {
					Hangars_WarnUnknownKey(path, AeronConfigNode_MapValueAt(section, index), inner, name);
				}
			}
			Hangars_ReadLayout(path, section, inner, &layer->layouts[1]);
			Hangars_ReadCameras(path, AeronConfigNode_MapGet(section, "cameras"), inner, 1, layer);
		}
	}
	return layer;
}

static void Hangars_FreeLayer(HangarsLayer* layer) {
	int index;

	if (layer == NULL) {
		return;
	}
	free(layer->layouts[0].parked);
	free(layer->layouts[1].parked);
	for (index = 0; index < layer->iffCount; ++index) {
		Hangars_FreeLayer(layer->iffs[index].layer);
	}
	free(layer->iffs);
	free(layer);
}

/* Reads a ship's layer, its hangar model and its IFF layers. */
static HangarsLayer* Hangars_ReadShip(const AeronConfigNode* node, const char* key) {
	static const char* const extraKeys[] = { "hangar_model", "by_iff", NULL };
	const AeronConfigNode* iffs = AeronConfigNode_MapGet(node, "by_iff");
	const AeronConfigNode* model = AeronConfigNode_MapGet(node, "hangar_model");
	HangarsLayer* layer;
	char where[160];
	size_t index;

	snprintf(where, sizeof where, "ships.%s", key);
	layer = Hangars_ReadLayer(s_settingsPath, node, where, extraKeys);
	if (layer == NULL) {
		return NULL;
	}
	if (model != NULL) {
		layer->hangarModel = AeronConfigNode_String(model, NULL);
		if (layer->hangarModel == NULL) {
			Hangars_WarnInvalidValue(s_settingsPath, model, where, "hangar_model", "a model name");
		}
	}
	if (iffs != NULL && AeronConfigNode_Type(iffs) != AERON_CONFIG_MAP) {
		Hangars_WarnInvalidValue(s_settingsPath, iffs, where, "by_iff", "a map");
		return layer;
	}
	layer->iffs = (HangarsIffLayer*)calloc(AeronConfigNode_MapCount(iffs) + 1, sizeof *layer->iffs);
	for (index = 0; layer->iffs != NULL && index < AeronConfigNode_MapCount(iffs); ++index) {
		const char* iff = AeronConfigNode_MapKeyAt(iffs, index);
		char* end = NULL;
		const long value = strtol(iff, &end, 10);
		char inner[200];

		snprintf(inner, sizeof inner, "%s.by_iff.%s", where, iff);
		if (end == iff || *end != '\0' || value < 0 || value > 255) {
			Aeron_LogWarn("xwa.models", "%s:%d: %s.by_iff: '%s' isn't an IFF from 0 to 255. It is ignored",
						  s_settingsPath, AeronConfigNode_Line(AeronConfigNode_MapValueAt(iffs, index)),
						  where, iff);
			continue;
		}
		layer->iffs[layer->iffCount].iff = (int)value;
		layer->iffs[layer->iffCount].layer =
			Hangars_ReadLayer(s_settingsPath, AeronConfigNode_MapValueAt(iffs, index), inner, NULL);
		if (layer->iffs[layer->iffCount].layer != NULL) {
			++layer->iffCount;
		}
	}
	return layer;
}

/* Reads a player craft's `rest_height_m` and `player_camera_m`. */
static void Hangars_ReadCraft(const AeronConfigNode* node, const char* key, HangarsCraft* craft) {
	static const char* const floors[] = { "normal", "inverted" };
	static const char* const cameras[] = { "hangar", "hangar_inverted", "family_base",
										   "family_base_inverted" };
	const AeronConfigNode* rest = AeronConfigNode_MapGet(node, "rest_height_m");
	const AeronConfigNode* camera = AeronConfigNode_MapGet(node, "player_camera_m");
	char where[160];
	size_t index;
	int slot;

	memset(craft, 0, sizeof *craft);
	craft->key = key;
	snprintf(where, sizeof where, "crafts.%s", key);
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* name = AeronConfigNode_MapKeyAt(node, index);

		if (strcmp(name, "rest_height_m") != 0 && strcmp(name, "player_camera_m") != 0) {
			Hangars_WarnUnknownKey(s_settingsPath, AeronConfigNode_MapValueAt(node, index), where, name);
		}
	}
	for (index = 0; index < AeronConfigNode_MapCount(rest); ++index) {
		const char* name = AeronConfigNode_MapKeyAt(rest, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(rest, index);

		for (slot = 0; slot < 2 && strcmp(floors[slot], name) != 0; ++slot) {
		}
		if (slot == 2) {
			Hangars_WarnUnknownKey(s_settingsPath, value, where, name);
		} else if (!Hangars_ReadMetres(value, &craft->rest[slot])) {
			Hangars_WarnInvalidValue(s_settingsPath, value, where, name, "a length in metres");
		} else {
			craft->restSet[slot] = 1;
		}
	}
	for (index = 0; index < AeronConfigNode_MapCount(camera); ++index) {
		const char* name = AeronConfigNode_MapKeyAt(camera, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(camera, index);

		for (slot = 0; slot < 4 && strcmp(cameras[slot], name) != 0; ++slot) {
		}
		if (slot == 4) {
			Hangars_WarnUnknownKey(s_settingsPath, value, where, name);
		} else if (!Hangars_ReadMetresList(value, 3, craft->camera[slot])) {
			Hangars_WarnInvalidValue(s_settingsPath, value, where, name, "[x, y, z] in metres");
		} else {
			craft->cameraSet[slot] = 1;
		}
	}
}

/* Makes the models a layer swaps in loadable. */
static void Hangars_MakeLayerModelsLoadable(const HangarsLayer* layer) {
	const AeronConfigNode* swaps = layer != NULL ? layer->swaps : NULL;
	size_t index;

	for (index = 0; index < AeronConfigNode_MapCount(swaps); ++index) {
		const char* target = AeronConfigNode_String(AeronConfigNode_MapValueAt(swaps, index), NULL);

		if (target == NULL || !XwaModelSource_AddKey(target)) {
			Aeron_LogWarn("xwa.models", "hangar swap '%s' -> '%s': no such model; the swap is ignored",
						  AeronConfigNode_MapKeyAt(swaps, index), target != NULL ? target : "");
		}
	}
}

static void Hangars_FreeSettings(void) {
	int index;

	Hangars_FreeLayer(s_default);
	s_default = NULL;
	for (index = 0; index < s_shipCount; ++index) {
		Hangars_FreeLayer(s_ships[index].layer);
	}
	free(s_ships);
	s_ships = NULL;
	s_shipCount = 0;
	free(s_crafts);
	s_crafts = NULL;
	s_craftCount = 0;
	if (s_settings != NULL) {
		AeronConfigFile_Destroy(s_settings);
		s_settings = NULL;
	}
}

void XwaHangars_Init(void) {
	static const char* const keys[] = { "default", "ships", "crafts" };
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* ships;
	const AeronConfigNode* crafts;
	size_t index;

	Hangars_FreeSettings();
	s_resolved = 0;
	if (!XwaModelSource_ReplacementsEnabled()) {
		return;
	}
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, s_settingsPath, &s_settings,
									&error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.models", "%s:%d: %s; the hangars keep their stock layout", s_settingsPath,
						   error.line, error.message);
		}
		s_settings = NULL;
		return;
	}
	root = AeronConfigFile_Root(s_settings);
	/* An empty file sets nothing. */
	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP && AeronConfigNode_Type(root) != AERON_CONFIG_NULL) {
		Aeron_LogError("xwa.models", "%s: the file must be a map; the hangars keep their stock layout",
					   s_settingsPath);
		Hangars_FreeSettings();
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(root); ++index) {
		const char* name = AeronConfigNode_MapKeyAt(root, index);

		if (strcmp(name, keys[0]) != 0 && strcmp(name, keys[1]) != 0 && strcmp(name, keys[2]) != 0) {
			Aeron_LogWarn("xwa.models", "%s:%d: '%s' isn't a hangar setting. It is ignored", s_settingsPath,
						  AeronConfigNode_Line(AeronConfigNode_MapValueAt(root, index)), name);
		}
	}
	s_default = Hangars_ReadLayer(s_settingsPath, AeronConfigNode_MapGet(root, "default"), "default", NULL);
	Hangars_MakeLayerModelsLoadable(s_default);
	ships = AeronConfigNode_MapGet(root, "ships");
	s_ships = (HangarsShip*)calloc(AeronConfigNode_MapCount(ships) + 1, sizeof *s_ships);
	for (index = 0; s_ships != NULL && index < AeronConfigNode_MapCount(ships); ++index) {
		HangarsShip* ship = &s_ships[s_shipCount];
		int iff;

		ship->key = AeronConfigNode_MapKeyAt(ships, index);
		ship->layer = Hangars_ReadShip(AeronConfigNode_MapValueAt(ships, index), ship->key);
		if (ship->layer == NULL) {
			continue;
		}
		++s_shipCount;
		if (ship->layer->hangarModel != NULL && !XwaModelSource_AddKey(ship->layer->hangarModel)) {
			Aeron_LogError("xwa.models", "hangar of '%s': no model '%s'", ship->key,
						   ship->layer->hangarModel);
		}
		Hangars_MakeLayerModelsLoadable(ship->layer);
		for (iff = 0; iff < ship->layer->iffCount; ++iff) {
			Hangars_MakeLayerModelsLoadable(ship->layer->iffs[iff].layer);
		}
	}
	crafts = AeronConfigNode_MapGet(root, "crafts");
	s_crafts = (HangarsCraft*)calloc(AeronConfigNode_MapCount(crafts) + 1, sizeof *s_crafts);
	for (index = 0; s_crafts != NULL && index < AeronConfigNode_MapCount(crafts); ++index) {
		Hangars_ReadCraft(AeronConfigNode_MapValueAt(crafts, index), AeronConfigNode_MapKeyAt(crafts, index),
						  &s_crafts[s_craftCount++]);
	}
	Aeron_LogInfo("xwa.models", "%s: %d ship hangars, %d crafts", s_settingsPath, s_shipCount, s_craftCount);
}

void XwaHangars_LoadMission(const char* missionPath) {
	static const char* const extraKeys[] = { "use_ship_settings", NULL };
	const AeronConfigNode* hangar;
	char path[300];

	Hangars_FreeLayer(s_mission);
	s_mission = NULL;
	s_missionShips = NULL;
	if (s_missionFile != NULL) {
		AeronConfigFile_Destroy(s_missionFile);
		s_missionFile = NULL;
	}
	s_resolved = 0;
	s_inHangar = 0;
	if (s_settings == NULL || missionPath == NULL ||
		XwaMissionFile_Load(missionPath, &s_missionFile, path, sizeof path) <= 0) {
		return;
	}
	hangar = AeronConfigNode_MapGet(AeronConfigFile_Root(s_missionFile), "hangar");
	if (hangar == NULL) {
		return;
	}
	s_mission = Hangars_ReadLayer(path, hangar, "hangar", extraKeys);
	s_missionShips = AeronConfigNode_MapGet(hangar, "use_ship_settings");
	if (s_missionShips != NULL && AeronConfigNode_Type(s_missionShips) != AERON_CONFIG_MAP) {
		Hangars_WarnInvalidValue(path, s_missionShips, "hangar", "use_ship_settings",
								 "a map of models to models");
		s_missionShips = NULL;
	}
	Hangars_MakeLayerModelsLoadable(s_mission);
}

int XwaHangars_IsActive(void) { return s_settings != NULL; }

/* ---- command ship: its model, IFF and markings -------------------------- */

/* Finds the mothership of flight group `fgIdx`. This is its arrival
   mothership when `arrivalFirst` is set and it arrives by one, else its
   departure mothership. */
static int Hangars_FindMothership(int fgIdx, int arrivalFirst, uint16_t* objectType, uint8_t* iff,
								  uint8_t* markings) {
	const XwaFlightGroup* fg;
	const XwaFlightGroup* mothership;
	int mothershipIdx;

	if (fgIdx < 0 || fgIdx >= HANGARS_FLIGHT_GROUPS) {
		return 0;
	}
	fg = &g_missionFlightGroups[fgIdx].fg;
	if (arrivalFirst && fg->arrivalMethod) {
		mothershipIdx = fg->arrivalMothership;
	} else if (fg->departMethod) {
		mothershipIdx = fg->departureMothership;
	} else {
		return 0;
	}
	if (mothershipIdx >= HANGARS_FLIGHT_GROUPS) {
		return 0;
	}
	mothership = &g_missionFlightGroups[mothershipIdx].fg;
	*objectType = g_objectTypeTables.craftTypeToObjectType[mothership->craftType];
	*iff = mothership->iff;
	*markings = mothership->markings;
	return 1;
}

/* Finds the hangar's command ship. This is the object of the player's
   pending dock action, else the arrival or departure mothership of the
   player's flight group. */
static int Hangars_FindCommandShip(uint16_t* objectType, uint8_t* iff, uint8_t* markings) {
	const PlayerData* player = &g_players[g_localPlayer];
	int objIdx;

	*objectType = 0;
	*iff = 0;
	*markings = 0;
	if (g_hangarPlayerObjIdx == 0xFFFF) {
		return 0;
	}
	objIdx = player->pendingActionId == 0 || player->pendingActionId == 2 ? player->pendingActionParam : 0;
	if (objIdx == -1) {
		return Hangars_FindMothership(player->boundFlightGroupIdx, 0, objectType, iff, markings) &&
			   *objectType != 0;
	}
	if (objIdx == 0) {
		return Hangars_FindMothership(player->boundFlightGroupIdx, 1, objectType, iff, markings) &&
			   *objectType != 0;
	}
	if (objIdx < 0 || (uint32_t)objIdx >= g_objectTableSlotCount) {
		return 0;
	}
	if (g_objectTable[objIdx].objectType == 0) {
		return Hangars_FindMothership(g_objectTable[objIdx].flightGroupIdx, 1, objectType, iff, markings) &&
			   *objectType != 0;
	}
	*objectType = g_objectTable[objIdx].objectType;
	if (g_objectTable[objIdx].flightGroupIdx < HANGARS_FLIGHT_GROUPS) {
		*iff = g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.iff;
		*markings = g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.markings;
	}
	return 1;
}

/* ---- resolution --------------------------------------------------------- */

/* Returns the value of `field` from the first layer that sets it, or NULL
   when no layer sets it. */
static const HangarsValue* Hangars_FirstLayerValue(int field) {
	int layer;

	for (layer = 0; layer < s_layerCount; ++layer) {
		if (s_layers[layer]->values[field].set) {
			return &s_layers[layer]->values[field];
		}
	}
	return NULL;
}

static int Hangars_ResolvedInt(int field, int axis, int defaultValue) {
	const HangarsValue* value = Hangars_FirstLayerValue(field);

	return value != NULL ? value->v[axis] : defaultValue;
}

/* Gets a profile, or "Default" when no layer sets it. */
static void Hangars_ResolvedProfile(int field, char* profile) {
	const HangarsValue* value = Hangars_FirstLayerValue(field);

	snprintf(profile, XWA_HANGAR_PROFILE_MAX, "%s", value != NULL ? value->text : "Default");
}

/* Gets the object types of `player.inverted_floor_crafts` from the first
   layer that sets it. */
static int Hangars_ResolveInvertedFloorCrafts(uint16_t* types) {
	const HangarsValue* value = Hangars_FirstLayerValue(FIELD_PLAYER_INVERTED_FLOOR_CRAFTS);
	size_t index;
	int count = 0;

	for (index = 0;
		 value != NULL && index < AeronConfigNode_SequenceCount(value->node) && count < HANGARS_LIST_MAX;
		 ++index) {
		int32_t objectType;

		if (Hangars_ReadObjectType(AeronConfigNode_SequenceGet(value->node, index), &objectType)) {
			types[count++] = (uint16_t)objectType;
		}
	}
	return count;
}

/* Gets the crafts of `player.craft_offsets_m` from the first layer that
   sets it. */
static int Hangars_ResolveCraftOffsets(XwaHangarPlayerCraft* crafts) {
	const HangarsValue* value = Hangars_FirstLayerValue(FIELD_PLAYER_CRAFT_OFFSETS);
	size_t index;
	int count = 0;

	for (index = 0;
		 value != NULL && index < AeronConfigNode_MapCount(value->node) && count < HANGARS_LIST_MAX;
		 ++index) {
		const int objectType =
			XwaNames_Find(&g_xwaObjectTypeNames, AeronConfigNode_MapKeyAt(value->node, index));
		int32_t offset[3];

		if (objectType > 0 && objectType < OBJ_Count &&
			Hangars_ReadMetresList(AeronConfigNode_MapValueAt(value->node, index), 3, offset)) {
			crafts[count].objectType = (uint16_t)objectType;
			memcpy(crafts[count].offset, offset, sizeof crafts[count].offset);
			++count;
		}
	}
	return count;
}

static void Hangars_ResolveObjects(void) {
	XwaHangarObjects* o = &s_objects;
	int droid;
	int axis;

	memset(o, 0, sizeof *o);
	o->loadShuttle = Hangars_ResolvedInt(FIELD_SHUTTLE_ENABLED, 0, 1);
	o->shuttleObjectType = (uint16_t)Hangars_ResolvedInt(FIELD_SHUTTLE_TYPE, 0, 0);
	o->shuttleMarkings = Hangars_ResolvedInt(FIELD_SHUTTLE_MARKINGS, 0, 0);
	Hangars_ResolvedProfile(FIELD_SHUTTLE_PROFILE, o->shuttleObjectProfile);
	o->shuttlePosition[0] = Hangars_ResolvedInt(FIELD_SHUTTLE_POSITION, 0, 0x467);
	o->shuttlePosition[1] = Hangars_ResolvedInt(FIELD_SHUTTLE_POSITION, 1, 0x3BF);
	o->shuttlePosition[2] = Hangars_ResolvedInt(FIELD_SHUTTLE_HEIGHT, 0, 0);
	o->shuttleOrientation = (uint16_t)Hangars_ResolvedInt(FIELD_SHUTTLE_HEADING, 0, 0xA880);
	o->isShuttleFloorInverted = Hangars_ResolvedInt(FIELD_SHUTTLE_FLOOR, 0, 0);
	o->shuttleAnimation =
		(XwaHangarShuttleAnimation)Hangars_ResolvedInt(FIELD_SHUTTLE_EXIT_SIDE, 0, XWA_HANGAR_SHUTTLE_RIGHT);
	o->shuttleAnimationStraightLine = Hangars_ResolvedInt(FIELD_SHUTTLE_STRAIGHT, 0, 0);
	o->shuttleAnimationElevation = Hangars_ResolvedInt(FIELD_SHUTTLE_RISE, 0, 0);

	for (droid = 0; droid < 2; ++droid) {
		const int first = FIELD_DROIDS + droid * HANGARS_DROID_FIELDS;
		XwaHangarDroid* settings = &o->droids[droid];

		settings->load = Hangars_ResolvedInt(first + DROID_ENABLED, 0, 1);
		settings->positionZ = Hangars_ResolvedInt(first + DROID_HEIGHT, 0, 0);
		settings->isFloorInverted = Hangars_ResolvedInt(first + DROID_FLOOR, 0, 0);
		settings->update = settings->load && Hangars_ResolvedInt(first + DROID_MOVES, 0, 1);
		settings->objectType = (uint16_t)Hangars_ResolvedInt(first + DROID_TYPE, 0,
															 droid == 0 ? OBJ_HangarDroid : OBJ_HangarDroid2);
		settings->markings = Hangars_ResolvedInt(first + DROID_MARKINGS, 0, 0);
		Hangars_ResolvedProfile(first + DROID_PROFILE, settings->objectProfile);
	}

	o->roofCranePosition[0] = Hangars_ResolvedInt(FIELD_ROOF_CRANE_POSITION, 0, -1400);
	o->roofCranePosition[1] = Hangars_ResolvedInt(FIELD_ROOF_CRANE_POSITION, 1, 786);
	o->roofCranePosition[2] = Hangars_ResolvedInt(FIELD_ROOF_CRANE_POSITION, 2, -282);
	o->roofCraneAxis = Hangars_ResolvedInt(FIELD_ROOF_CRANE_AXIS, 0, 0);
	o->roofCraneLowOffset = Hangars_ResolvedInt(FIELD_ROOF_CRANE_TRAVEL, 0, 0);
	o->roofCraneHighOffset = Hangars_ResolvedInt(FIELD_ROOF_CRANE_TRAVEL, 1, 2800);
	o->isHangarFloorInverted = Hangars_ResolvedInt(FIELD_HANGAR_FLOOR, 0, 0);
	o->hangarFloorInvertedHeight = Hangars_ResolvedInt(FIELD_HANGAR_INVERTED_FLOOR_HEIGHT, 0, 0);
	o->hangarIff = (uint8_t)Hangars_ResolvedInt(FIELD_HANGAR_MARKINGS, 0, 0xFF);
	o->playerAnimationElevation = Hangars_ResolvedInt(FIELD_PLAYER_RISE, 0, 0);
	o->playerAnimationInvertedElevation =
		Hangars_ResolvedInt(FIELD_PLAYER_INVERTED_RISE, 0, o->playerAnimationElevation);
	o->playerAnimationStraightLine = Hangars_ResolvedInt(FIELD_PLAYER_STRAIGHT, 0, 0);
	for (axis = 0; axis < 3; ++axis) {
		o->playerOffset[axis] = Hangars_ResolvedInt(FIELD_PLAYER_OFFSET, axis, 0);
	}
	for (axis = 0; axis < 3; ++axis) {
		o->playerInvertedOffset[axis] =
			Hangars_ResolvedInt(FIELD_PLAYER_INVERTED_OFFSET, axis, o->playerOffset[axis]);
	}
	o->playerCraftCount = Hangars_ResolveCraftOffsets(s_playerCrafts);
	o->playerCrafts = s_playerCrafts;
	o->isPlayerFloorInverted = Hangars_ResolvedInt(FIELD_PLAYER_FLOOR, 0, 0);
	o->playerFloorInvertedTypeCount = Hangars_ResolveInvertedFloorCrafts(s_floorInvertedTypes);
	o->playerFloorInvertedTypes = s_floorInvertedTypes;
}

/* The stock layouts at the highest detail level, used when no layer has
   one. The crane arm is the line s_stockCraneLine names. */
typedef struct HangarsDefaultLine {
	uint16_t objectType;
	int x;
	int y;
	int z;
	uint16_t headingXY;
	uint16_t headingZ;
} HangarsDefaultLine;

static const HangarsDefaultLine s_defaultMap[] = {
	{ 314, -1059, 1313, HANGARS_FLOOR, 38600, 0 },
	{ 309, 725, -416, HANGARS_FLOOR, 23500, 0 },
	{ 313, -1352, 1440, HANGARS_FLOOR, 536, 0 },
	{ 315, -1065, -56, HANGARS_FLOOR, 47736, 0 },
	{ 315, 2079, 733, HANGARS_FLOOR, 6400, 0 },
	{ 315, -2488, 1215, HANGARS_FLOOR, 11864, 0 },
	{ 310, 794, -750, -848, 6800, 12500 },
	{ 310, -803, -1464, HANGARS_FLOOR, 49200, 0 },
	{ 310, -1563, 944, HANGARS_FLOOR, 51436, 0 },
	{ 83, -506, -2136, -722, 32836, 16480 },
	{ 83, -841, -1628, HANGARS_FLOOR, 53100, 0 },
	{ 83, 1240, -2098, -722, 272, 16480 },
	{ 84, -834, -2034, HANGARS_FLOOR, 32336, 0 },
	{ 80, -3474, -275, HANGARS_FLOOR, 29436, 0 },
	{ 2, -1038, -798, HANGARS_FLOOR, 7700, 0 },
	{ 50, 1146, -1288, -741, 50536, 0 },
	{ 315, 820, -665, HANGARS_FLOOR, 49136, 0 },
	{ 310, -873, -1456, -849, 60472, 20400 },
	{ 310, -3095, -592, HANGARS_FLOOR, 51436, 0 },
	{ 83, -1751, -1996, HANGARS_FLOOR, 31172, 0 },
	{ 83, 979, -2103, HANGARS_FLOOR, 37036, 0 },
	{ 84, -1374, -2068, HANGARS_FLOOR, 40536, 0 },
	{ 80, 2806, 1357, HANGARS_FLOOR, 20536, 0 },
	{ 80, -195, -1910, HANGARS_FLOOR, 54372, 0 },
	{ 3, 2178, 506, -830, 40036, 1500 },
	{ 2, 1100, -503, HANGARS_FLOOR, 49036, 0 },
	{ 1, -1003, 280, -825, 16600, 0 },
};

static const HangarsDefaultLine s_defaultFamilyMap[] = {
	{ 315, 821, -6618, HANGARS_FLOOR, 30600, 0 },
	{ 314, 2431, -5447, HANGARS_FLOOR, 61536, 0 },
	{ 309, 626, -6397, HANGARS_FLOOR, 736, 0 },
	{ 313, 1577, -6663, HANGARS_FLOOR, 500, 0 },
	{ 80, -1954, -7445, HANGARS_FLOOR, 31236, 0 },
	{ 80, -1938, -7445, -4997, 31236, 0 },
	{ 83, -1231, -8544, -5003, 32736, 0x4000 },
	{ 83, -1481, -8444, -5003, 45736, 0x4000 },
	{ 83, -1821, -8464, -5003, 32736, 0x4000 },
	{ 84, -1834, -6154, HANGARS_FLOOR, 42736, 0 },
	{ 84, 1939, -8013, HANGARS_FLOOR, 31800, 0 },
	{ 80, -1954, -7445, -4888, 31236, 0 },
	{ 80, -1978, -7386, -4781, 31736, 0 },
	{ 310, 1484, -8339, -5133, 29700, 20400 },
	{ 310, 1384, -8339, HANGARS_FLOOR, 32036, 0 },
	{ 83, -2061, -8344, -5003, 32736, 0x4000 },
	{ 83, -1511, -6734, HANGARS_FLOOR, 27736, 0x8000 },
};

/* The line of each stock layout that holds the crane arm. */
static const int s_stockCraneLine[2] = { 1, 2 };

static void Hangars_StockLayoutEntry(const HangarsDefaultLine* line, XwaHangarMapEntry* entry) {
	memset(entry, 0, sizeof *entry);
	entry->objectType = line->objectType;
	entry->position[0] = line->x;
	entry->position[1] = line->y;
	entry->position[2] = line->z;
	entry->headingXY = line->headingXY;
	entry->headingZ = line->headingZ;
	strcpy(entry->objectProfile, "Default");
	entry->isFloorInverted = -1;
}

/* The crane arm and the parked objects of the first layer that has them. */
static void Hangars_ResolveMap(int family) {
	const HangarsLayout* layout = NULL;
	int layer;
	int index;

	free(s_maps[family]);
	s_maps[family] = NULL;
	s_mapCounts[family] = 0;
	s_mapPresent[family] = 1;
	s_hasCraneArm[family] = 0;
	for (layer = 0; layer < s_layerCount && layout == NULL; ++layer) {
		if (s_layers[layer]->layouts[family].present) {
			layout = &s_layers[layer]->layouts[family];
		}
	}
	if (layout == NULL) {
		const HangarsDefaultLine* lines = family ? s_defaultFamilyMap : s_defaultMap;
		const int count = family ? (int)(sizeof s_defaultFamilyMap / sizeof s_defaultFamilyMap[0])
								 : (int)(sizeof s_defaultMap / sizeof s_defaultMap[0]);

		s_maps[family] = (XwaHangarMapEntry*)calloc((size_t)count, sizeof *s_maps[family]);
		if (s_maps[family] == NULL) {
			return;
		}
		for (index = 0; index < count; ++index) {
			if (index == s_stockCraneLine[family]) {
				Hangars_StockLayoutEntry(&lines[index], &s_craneArms[family]);
				s_hasCraneArm[family] = 1;
			} else {
				Hangars_StockLayoutEntry(&lines[index], &s_maps[family][s_mapCounts[family]++]);
			}
		}
		return;
	}
	s_hasCraneArm[family] = layout->hasCraneArm;
	s_craneArms[family] = layout->craneArm;
	if (layout->parkedCount == 0) {
		return;
	}
	s_maps[family] = (XwaHangarMapEntry*)malloc((size_t)layout->parkedCount * sizeof *s_maps[family]);
	if (s_maps[family] == NULL) {
		return;
	}
	memcpy(s_maps[family], layout->parked, (size_t)layout->parkedCount * sizeof *s_maps[family]);
	s_mapCounts[family] = layout->parkedCount;
}

/* Each camera key from the first layer that sets it. */
static void Hangars_ResolveCamera(int family) {
	XwaHangarCamera* camera = &s_cameras[family];
	int key;
	int layer;

	memset(camera, 0, sizeof *camera);
	for (key = 0; key < XWA_HANGAR_CAMERA_KEYS; ++key) {
		for (layer = 0; layer < s_layerCount && !camera->set[key]; ++layer) {
			if (s_layers[layer]->cameraSet[family][key]) {
				camera->set[key] = 1;
				memcpy(camera->offsets[key], s_layers[layer]->cameras[family][key],
					   sizeof camera->offsets[key]);
			}
		}
	}
}

/* Collects the swaps. When several layers swap the same model, the first
   layer wins. */
static void Hangars_ResolveSwaps(void) {
	int layer;

	s_swapCount = 0;
	for (layer = 0; layer < s_layerCount; ++layer) {
		const AeronConfigNode* swaps = s_layers[layer]->swaps;
		size_t index;

		for (index = 0; index < AeronConfigNode_MapCount(swaps); ++index) {
			const char* source = AeronConfigNode_MapKeyAt(swaps, index);
			const char* target = AeronConfigNode_String(AeronConfigNode_MapValueAt(swaps, index), NULL);
			int known = 0;
			int swap;

			for (swap = 0; swap < s_swapCount && !known; ++swap) {
				known = strcmp(s_swaps[swap].source, source) == 0;
			}
			/* A swap to a model that cannot load is ignored (reported at load). */
			if (known || target == NULL || s_swapCount == HANGARS_SWAPS_MAX ||
				strlen(source) >= sizeof(HangarsKey) || strlen(target) >= sizeof(HangarsKey) ||
				!XwaModelSource_AddKey(target)) {
				continue;
			}
			strcpy(s_swaps[s_swapCount].source, source);
			strcpy(s_swaps[s_swapCount].target, target);
			++s_swapCount;
		}
	}
}

static const HangarsLayer* Hangars_FindShip(const char* key) {
	int index;

	for (index = 0; index < s_shipCount; ++index) {
		if (strcmp(s_ships[index].key, key) == 0) {
			return s_ships[index].layer;
		}
	}
	return NULL;
}

/* Builds the resolved tables for the command ship in s_shipKey and
   s_shipIff. s_shipIff is -1 when there is no command ship. `ship` is the
   ship layer that applies, or NULL. */
static void Hangars_BuildResolvedTables(const HangarsLayer* ship) {
	int index;

	/* The layers are searched in this order: mission, ship IFF, ship,
	   default. The proving ground uses only the default layer. */
	s_layerCount = 0;
	if (!s_provingGround && s_mission != NULL) {
		s_layers[s_layerCount++] = s_mission;
	}
	if (ship != NULL) {
		const int iff = s_shipIff >= 0 ? s_shipIff : 0;

		for (index = 0; index < ship->iffCount; ++index) {
			if (ship->iffs[index].iff == iff) {
				s_layers[s_layerCount++] = ship->iffs[index].layer;
				break;
			}
		}
		s_layers[s_layerCount++] = ship;
	}
	if (s_default != NULL) {
		s_layers[s_layerCount++] = s_default;
	}

	Hangars_ResolveObjects();
	Hangars_ResolveMap(0);
	Hangars_ResolveMap(1);
	Hangars_ResolveCamera(0);
	Hangars_ResolveCamera(1);
	Hangars_ResolveSwaps();
	s_hangarModel[0] = '\0';
	if (ship != NULL && ship->hangarModel != NULL) {
		if (strlen(ship->hangarModel) < sizeof s_hangarModel && XwaModelSource_AddKey(ship->hangarModel)) {
			strcpy(s_hangarModel, ship->hangarModel);
		} else {
			/* The stock hangar would be the wrong hangar, so this is a fatal
			   error. */
			char message[256];

			snprintf(message, sizeof message,
					 "The hangar of %s can't be shown, because its model %s can't be loaded.", s_shipKey,
					 ship->hangarModel);
			File_PrintFatalMessageAndExit(message, 1);
		}
	}
}

int XwaHangars_Resolve(void) {
	const HangarsLayer* ship = NULL;
	HangarsKey key;
	uint16_t objectType;
	uint8_t iff;
	uint8_t markings;
	int hasShip;
	int changed;

	if (s_settings == NULL) {
		return 0;
	}
	key[0] = '\0';
	hasShip = !g_provingGroundsModeActive && Hangars_FindCommandShip(&objectType, &iff, &markings) &&
			  XwaModelSource_TypeKey(objectType, key);
	if (!hasShip) {
		key[0] = '\0';
		iff = 0;
		markings = 0;
	} else {
		/* A mission can make the command ship use the settings and hangar of
		   another model. */
		const char* substitute = AeronConfigNode_String(AeronConfigNode_MapGet(s_missionShips, key), NULL);

		if (substitute != NULL && strlen(substitute) < sizeof key) {
			strcpy(key, substitute);
		}
		ship = Hangars_FindShip(key);
	}
	changed = !s_resolved || s_provingGround != g_provingGroundsModeActive || strcmp(s_shipKey, key) != 0 ||
			  s_shipIff != (hasShip ? iff : -1);
	s_resolved = 1;
	s_inHangar = 1;
	s_provingGround = g_provingGroundsModeActive;
	strcpy(s_shipKey, key);
	s_shipIff = hasShip ? iff : -1;
	s_shipMarkings = markings;
	Hangars_BuildResolvedTables(ship);
	if (changed) {
		Aeron_LogInfo(
			"xwa.models", "hangar: ship '%s' IFF %d (%s), model '%s', %d layers, %d parked objects, %d swaps",
			hasShip ? s_shipKey : "", s_shipIff, ship != NULL ? "settings" : "default settings",
			s_hangarModel[0] != '\0' ? s_hangarModel : "hangar", s_layerCount, s_mapCounts[0], s_swapCount);
	}
	return changed;
}

void XwaHangars_SaveFlightState(XwaHangarsFlightState* state) {
	memset(state, 0, sizeof *state);
	state->resolved = (uint8_t)s_resolved;
	state->inHangar = (uint8_t)s_inHangar;
	state->provingGround = (uint8_t)(s_provingGround != 0);
	state->shipMarkings = s_shipMarkings;
	state->shipIff = s_shipIff;
	memcpy(state->shipKey, s_shipKey, sizeof state->shipKey);
}

void XwaHangars_RestoreFlightState(const XwaHangarsFlightState* state) {
	if (s_settings == NULL) {
		return;
	}
	s_resolved = state->resolved;
	s_inHangar = state->inHangar;
	s_provingGround = state->provingGround;
	s_shipMarkings = state->shipMarkings;
	s_shipIff = state->shipIff;
	memcpy(s_shipKey, state->shipKey, sizeof s_shipKey);
	s_shipKey[sizeof s_shipKey - 1] = '\0';
	if (!s_resolved) {
		return;
	}
	Hangars_BuildResolvedTables(s_shipIff >= 0 ? Hangars_FindShip(s_shipKey) : NULL);
	if (s_inHangar) {
		XwaHangars_ReapplyHangarModels();
	}
}

const XwaHangarObjects* XwaHangars_Objects(void) { return &s_objects; }

const XwaHangarMapEntry* XwaHangars_Map(int family, int* count) {
	family = family != 0;
	*count = s_mapCounts[family];
	return s_mapPresent[family] ? s_maps[family] : NULL;
}

const XwaHangarMapEntry* XwaHangars_CraneArm(int family) {
	family = family != 0;
	return s_mapPresent[family] && s_hasCraneArm[family] ? &s_craneArms[family] : NULL;
}

const XwaHangarCamera* XwaHangars_Camera(int family) { return &s_cameras[family != 0]; }

uint8_t XwaHangars_CommandShipMarkings(void) { return s_shipMarkings; }

int XwaHangars_CommandShipType(void) {
	uint16_t objectType;
	uint8_t iff;
	uint8_t markings;

	return Hangars_FindCommandShip(&objectType, &iff, &markings) && objectType != 0 ? objectType : -1;
}

/* ---- models ------------------------------------------------------------- */

/* The key the settings load for `objectType`; empty for the stock model. */
static void Hangars_WantedModelKey(uint16_t objectType, char* key) {
	HangarsKey typeKey;
	int swap;

	key[0] = '\0';
	if (s_settings == NULL || !s_resolved) {
		return;
	}
	if (objectType == OBJ_Hangar && s_hangarModel[0] != '\0') {
		strcpy(key, s_hangarModel);
		return;
	}
	if (!s_inHangar || !XwaModelSource_TypeKey(objectType, typeKey)) {
		return;
	}
	for (swap = 0; swap < s_swapCount; ++swap) {
		if (strcmp(s_swaps[swap].source, typeKey) == 0) {
			strcpy(key, s_swaps[swap].target);
			return;
		}
	}
}

int XwaHangars_ModelFileName(uint16_t objectType, char* fileName, unsigned int fileNameSize) {
	HangarsKey key;
	int written;

	Hangars_WantedModelKey(objectType, key);
	if (key[0] == '\0') {
		return 0;
	}
	written = snprintf(fileName, fileNameSize, "%s.opt", key);
	return written > 0 && (unsigned int)written < fileNameSize;
}

int XwaHangars_TypeFileName(uint16_t objectType, char* fileName, unsigned int fileNameSize) {
	HangarsKey key;
	int written;

	if (s_settings == NULL || !XwaModelSource_TypeKey(objectType, key)) {
		return 0;
	}
	written = snprintf(fileName, fileNameSize, "%s.opt", key);
	return written > 0 && (unsigned int)written < fileNameSize;
}

void XwaHangars_ModelLoaded(uint16_t objectType, uint16_t handle) {
	if (objectType >= XWA_LOADED_MODEL_COUNT) {
		return;
	}
	s_loaded[objectType].handle = handle;
	Hangars_WantedModelKey(objectType, s_loaded[objectType].key);
}

void XwaHangars_ReleaseStaleModel(uint16_t objectType) {
	const uint16_t handle = objectType < XWA_LOADED_MODEL_COUNT ? g_loadedModels.byObjectType[objectType] : 0;
	HangarsKey desired;
	const char* loaded;

	if (handle == 0) {
		return;
	}
	/* If this module didn't see the handle load, it holds the stock model. */
	loaded = s_loaded[objectType].handle == handle ? s_loaded[objectType].key : "";
	Hangars_WantedModelKey(objectType, desired);
	if (strcmp(loaded, desired) == 0) {
		return;
	}
	Aeron_LogInfo("xwa.models", "hangar: type %u reloads '%s' instead of '%s'", (unsigned int)objectType,
				  desired[0] != '\0' ? desired : "stock", loaded[0] != '\0' ? loaded : "stock");
	OptModel_FreeHandle(handle);
	g_loadedModels.byObjectType[objectType] = 0;
	g_objectTypes[objectType].curTexLevel = NULL;
	g_modelBoundsCached[objectType] = 0;
	s_loaded[objectType].handle = 0;
}

/* Reloads the models that the settings now want for the object types in
   use. With `hangar` set, these are the types of the hangar's objects,
   else the types of the other objects of the mission. */

static void Hangars_ReloadModelsInUse(int hangar) {
	static uint8_t used[XWA_LOADED_MODEL_COUNT];
	uint8_t hangarFlightGroup;
	uint32_t objIdx;
	unsigned int objectType;

	if (s_settings == NULL || g_launchRefObjIdx < 0 ||
		(uint32_t)g_launchRefObjIdx >= g_objectTableSlotCount) {
		return;
	}
	hangarFlightGroup = g_objectTable[g_launchRefObjIdx].flightGroupIdx;
	memset(used, 0, sizeof used);
	for (objIdx = 0; objIdx < g_objectTableSlotCount; ++objIdx) {
		const ObjectRecord* object = &g_objectTable[objIdx];

		if (object->objectType == 0 || object->objectType >= XWA_LOADED_MODEL_COUNT ||
			object->objectType == OBJ_Hangar || object->objectType == OBJ_FamilyBase) {
			continue;
		}
		if ((object->flightGroupIdx == hangarFlightGroup) == (hangar != 0)) {
			used[object->objectType] = 1;
		}
	}
	for (objectType = 0; objectType < XWA_LOADED_MODEL_COUNT; ++objectType) {
		if (!used[objectType] || g_loadedModels.byObjectType[objectType] == 0) {
			continue;
		}
		XwaHangars_ReleaseStaleModel((uint16_t)objectType);
		if (g_loadedModels.byObjectType[objectType] == 0) {
			Hangar_LoadCraftModelByType((uint16_t)objectType);
			Math_SetFpuSinglePrecisionMode();
		}
	}
}

void XwaHangars_ReapplyHangarModels(void) { Hangars_ReloadModelsInUse(1); }

void XwaHangars_LeaveHangar(void) {
	if (!s_inHangar) {
		return;
	}
	s_inHangar = 0;
	Hangars_ReloadModelsInUse(0);
}

/* ---- placement ---------------------------------------------------------- */

/* The settings of the player craft model `key`, or NULL. */
static const HangarsCraft* Hangars_FindCraft(const char* key) {
	int index;

	for (index = 0; index < s_craftCount; ++index) {
		if (strcmp(s_crafts[index].key, key) == 0) {
			return &s_crafts[index];
		}
	}
	return NULL;
}

int XwaHangars_CraftElevation(uint16_t objectType, int inverted) {
	const HangarsCraft* craft = NULL;
	HangarsKey key;

	Hangars_WantedModelKey(objectType, key);
	if (s_settings != NULL && (key[0] != '\0' || XwaModelSource_TypeKey(objectType, key))) {
		craft = Hangars_FindCraft(key);
	}
	if (craft != NULL && craft->restSet[inverted != 0]) {
		return craft->rest[inverted != 0];
	}
	switch (objectType) {
		case OBJ_BWing:
			return 50;
		case OBJ_Shuttle:
			return 138;
		default:
			return ModelBounds_GetSizeZ(objectType) / 2;
	}
}

int XwaHangars_PlayerFloorInverted(uint16_t objectType) {
	int index;

	if (s_objects.isPlayerFloorInverted) {
		return 1;
	}
	for (index = 0; index < s_objects.playerFloorInvertedTypeCount; ++index) {
		if (s_objects.playerFloorInvertedTypes[index] == objectType) {
			return 1;
		}
	}
	return 0;
}

void XwaHangars_PlayerOffset(uint16_t objectType, int offset[3]) {
	const int* source =
		XwaHangars_PlayerFloorInverted(objectType) ? s_objects.playerInvertedOffset : s_objects.playerOffset;
	int index;

	for (index = 0; index < s_objects.playerCraftCount; ++index) {
		if (s_objects.playerCrafts[index].objectType == objectType) {
			source = s_objects.playerCrafts[index].offset;
			break;
		}
	}
	memcpy(offset, source, 3 * sizeof *offset);
}

int XwaHangars_CraftCamera(uint16_t objectType, int family, int inverted, int offset[3]) {
	const HangarsCraft* craft = NULL;
	const int slot = (family != 0) * 2 + (inverted != 0);
	HangarsKey key;
	int axis;

	/* The camera comes from the type's own model, never from a swap. */
	if (s_settings != NULL && XwaModelSource_TypeKey(objectType, key)) {
		craft = Hangars_FindCraft(key);
	}
	if (craft == NULL || !craft->cameraSet[slot]) {
		return 0;
	}
	for (axis = 0; axis < 3; ++axis) {
		offset[axis] = craft->camera[slot][axis];
	}
	return 1;
}
