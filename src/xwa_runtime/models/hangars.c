#include "xwa_runtime/models/hangars.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/math/scalar.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/models/model_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. It reproduces the settings
   lookup of XWAU's hook_hangars (hangar.cpp: GetCommandShipModelIndex,
   GetCustomFileLines, CustomFileLinesHangarObjects) over the layers the
   model conversion imports. */

#define HANGARS_LAYERS 4
#define HANGARS_LIST_MAX 64
#define HANGARS_SWAPS_MAX 64
#define HANGARS_FLIGHT_GROUPS 192

typedef char HangarsKey[XWA_MODEL_SOURCE_KEY_MAX];

typedef struct HangarsSwap {
	HangarsKey source;
	HangarsKey target;
} HangarsSwap;

typedef struct HangarsLoadedModel {
	uint16_t handle;
	HangarsKey key; /* empty: the stock model */
} HangarsLoadedModel;

static const char s_settingsPath[] = "hangars.yaml";
static AeronConfigFile* s_settings;
static AeronConfigFile* s_missionFile;
static const AeronConfigNode* s_mission;

/* The resolved state. */
static int s_resolved;
/* From a resolution to leaving the hangar: HangarObjects swaps apply. */
static int s_inHangar;
static int s_provingGround;
static HangarsKey s_shipKey;
static int s_shipIff;
static uint8_t s_shipMarkings;
static const AeronConfigNode* s_layers[HANGARS_LAYERS];
static int s_layerCount;
static XwaHangarObjects s_objects;
static XwaHangarPlayerCraft s_playerCrafts[HANGARS_LIST_MAX];
static uint16_t s_floorInvertedTypes[HANGARS_LIST_MAX];
static XwaHangarMapEntry* s_maps[2];
static int s_mapCounts[2];
static int s_mapPresent[2];
static XwaHangarCamera s_cameras[2];
static HangarsSwap s_swaps[HANGARS_SWAPS_MAX];
static int s_swapCount;
static HangarsKey s_hangarModel;
static HangarsLoadedModel s_loaded[XWA_LOADED_MODEL_COUNT];

/* ---- loading ------------------------------------------------------------ */

/* Makes the models a layer swaps in loadable. */
static void Hangars_AddLayerKeys(const AeronConfigNode* layer) {
	const AeronConfigNode* swaps = AeronConfigNode_MapGet(layer, "swaps");
	size_t index;

	for (index = 0; index < AeronConfigNode_MapCount(swaps); ++index) {
		const char* target = AeronConfigNode_String(AeronConfigNode_MapValueAt(swaps, index), NULL);

		if (target == NULL || !XwaModelSource_AddKey(target)) {
			Aeron_LogWarn("xwa.models", "hangar swap '%s' -> '%s': no such model; the swap is ignored",
						  AeronConfigNode_MapKeyAt(swaps, index), target != NULL ? target : "");
		}
	}
}

void XwaHangars_Init(void) {
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* ships;
	size_t index;

	if (s_settings != NULL) {
		AeronConfigFile_Destroy(s_settings);
		s_settings = NULL;
	}
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
	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1) {
		Aeron_LogError("xwa.models", "%s: expected 'version: 1'; the hangars keep their stock layout",
					   s_settingsPath);
		AeronConfigFile_Destroy(s_settings);
		s_settings = NULL;
		return;
	}
	Hangars_AddLayerKeys(AeronConfigNode_MapGet(root, "default"));
	ships = AeronConfigNode_MapGet(root, "ships");
	for (index = 0; index < AeronConfigNode_MapCount(ships); ++index) {
		const AeronConfigNode* ship = AeronConfigNode_MapValueAt(ships, index);
		const AeronConfigNode* iffs = AeronConfigNode_MapGet(ship, "iff");
		const char* model = AeronConfigNode_String(AeronConfigNode_MapGet(ship, "model"), NULL);
		size_t iff;

		if (model != NULL && !XwaModelSource_AddKey(model)) {
			Aeron_LogWarn("xwa.models", "hangar of '%s': no model '%s'; it uses the stock hangar",
						  AeronConfigNode_MapKeyAt(ships, index), model);
		}
		Hangars_AddLayerKeys(ship);
		for (iff = 0; iff < AeronConfigNode_MapCount(iffs); ++iff) {
			Hangars_AddLayerKeys(AeronConfigNode_MapValueAt(iffs, iff));
		}
	}
	Aeron_LogInfo("xwa.models", "%s: %u ship hangars", s_settingsPath, (unsigned int)AeronConfigNode_MapCount(ships));
}

void XwaHangars_LoadMission(const char* missionPath) {
	char path[300];

	if (s_missionFile != NULL) {
		AeronConfigFile_Destroy(s_missionFile);
		s_missionFile = NULL;
	}
	s_mission = NULL;
	s_resolved = 0;
	s_inHangar = 0;
	if (s_settings == NULL || missionPath == NULL ||
		XwaMissionFile_Load(missionPath, &s_missionFile, path, sizeof path) <= 0) {
		return;
	}
	s_mission = AeronConfigNode_MapGet(AeronConfigFile_Root(s_missionFile), "hangar");
	if (s_mission != NULL && AeronConfigNode_Type(s_mission) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.models", "%s: 'hangar' is not a map; ignored", path);
		s_mission = NULL;
	}
	Hangars_AddLayerKeys(s_mission);
}

int XwaHangars_IsActive(void) { return s_settings != NULL; }

/* ---- command ship (hook_hangars GetCommandShipModelIndex, ...Iff, ...Markings) */

/* The mothership of flight group `fgIdx`: its arrival mothership (when
   `arrivalFirst` and it arrives by one), else its departure mothership. */
static int Hangars_Mothership(int fgIdx, int arrivalFirst, uint16_t* objectType, uint8_t* iff,
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
	if (mothership->craftType >= XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT) {
		return 0;
	}
	*objectType = g_objectTypeTables.craftTypeToObjectType[mothership->craftType];
	*iff = mothership->iff;
	*markings = mothership->markings;
	return 1;
}

/* The hangar's command ship: the object the player docks with (the pending
   dock action), else the player flight group's arrival or departure
   mothership. */
static int Hangars_CommandShip(uint16_t* objectType, uint8_t* iff, uint8_t* markings) {
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
		return Hangars_Mothership(player->boundFlightGroupIdx, 0, objectType, iff, markings) &&
			   *objectType != 0;
	}
	if (objIdx == 0) {
		return Hangars_Mothership(player->boundFlightGroupIdx, 1, objectType, iff, markings) &&
			   *objectType != 0;
	}
	if (objIdx < 0 || (uint32_t)objIdx >= g_objectTableSlotCount) {
		return 0;
	}
	if (g_objectTable[objIdx].objectType == 0) {
		return Hangars_Mothership(g_objectTable[objIdx].flightGroupIdx, 1, objectType, iff, markings) &&
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

/* The first layer's value of HangarObjects key `name`; NULL when no layer
   sets it. A null value is set: the key's default applies. */
static const AeronConfigNode* Hangars_Value(const char* name) {
	int layer;

	for (layer = 0; layer < s_layerCount; ++layer) {
		const AeronConfigNode* value = AeronConfigNode_MapGet(AeronConfigNode_MapGet(s_layers[layer], "objects"), name);

		if (value != NULL) {
			return value;
		}
	}
	return NULL;
}

static int Hangars_Int(const char* name, int defaultValue) {
	const AeronConfigNode* value = Hangars_Value(name);

	return AeronConfigNode_Type(value) == AERON_CONFIG_INT ? (int)AeronConfigNode_Int(value, defaultValue)
														   : defaultValue;
}

/* A profile key: "Default" when unset or empty. */
static void Hangars_Profile(const char* name, char* profile) {
	const char* value = AeronConfigNode_String(Hangars_Value(name), NULL);

	snprintf(profile, XWA_HANGAR_PROFILE_MAX, "%s", value != NULL && value[0] != '\0' ? value : "Default");
}

/* A list key's numbers; returns its length. */
static int Hangars_List(const char* name, int* values) {
	const AeronConfigNode* list = Hangars_Value(name);
	size_t count = AeronConfigNode_SequenceCount(list);
	size_t index;

	if (count > HANGARS_LIST_MAX) {
		count = HANGARS_LIST_MAX;
	}
	for (index = 0; index < count; ++index) {
		values[index] = (int)AeronConfigNode_Int(AeronConfigNode_SequenceGet(list, index), 0);
	}
	return (int)count;
}

/* ASCII case-insensitive equality, as the hook's _stricmp compares. */
static int Hangars_EqualNoCase(const char* a, const char* b) {
	for (; *a != '\0' && *b != '\0'; ++a, ++b) {
		const char ca = *a >= 'A' && *a <= 'Z' ? (char)(*a - 'A' + 'a') : *a;
		const char cb = *b >= 'A' && *b <= 'Z' ? (char)(*b - 'A' + 'a') : *b;

		if (ca != cb) {
			return 0;
		}
	}
	return *a == *b;
}

/* CustomFileLinesHangarObjects::UpdateIfChanged, with its defaults. */
static void Hangars_ResolveObjects(void) {
	XwaHangarObjects* o = &s_objects;
	const char* animation = AeronConfigNode_String(Hangars_Value("ShuttleAnimation"), NULL);
	int droidsPositionZ;
	int loadDroids;
	int isDroidsFloorInverted;
	int types[HANGARS_LIST_MAX];
	int offsets[3][HANGARS_LIST_MAX];
	int count;
	int index;

	memset(o, 0, sizeof *o);
	o->loadShuttle = Hangars_Int("LoadShuttle", 1) == 1;
	o->shuttleModelType = (uint16_t)Hangars_Int("ShuttleModelIndex", 0);
	o->shuttleMarkings = Hangars_Int("ShuttleMarkings", 0);
	Hangars_Profile("ShuttleObjectProfile", o->shuttleObjectProfile);
	o->shuttlePosition[0] = Hangars_Int("ShuttlePositionX", 0x467);
	o->shuttlePosition[1] = Hangars_Int("ShuttlePositionY", 0x3BF);
	o->shuttlePosition[2] = Hangars_Int("ShuttlePositionZ", 0);
	o->shuttleOrientation = (uint16_t)Hangars_Int("ShuttleOrientation", 0xA880);
	o->isShuttleFloorInverted = Hangars_Int("IsShuttleFloorInverted", 0) != 0;
	o->shuttleAnimation = XWA_HANGAR_SHUTTLE_RIGHT;
	if (animation != NULL && Hangars_EqualNoCase(animation, "Top")) {
		o->shuttleAnimation = XWA_HANGAR_SHUTTLE_TOP;
	} else if (animation != NULL && Hangars_EqualNoCase(animation, "Bottom")) {
		o->shuttleAnimation = XWA_HANGAR_SHUTTLE_BOTTOM;
	}
	o->shuttleAnimationStraightLine = Hangars_Int("ShuttleAnimationStraightLine", 0);
	o->shuttleAnimationElevation = Hangars_Int("ShuttleAnimationElevation", 0);

	loadDroids = Hangars_Int("LoadDroids", 1) == 1;
	droidsPositionZ = Hangars_Int("DroidsPositionZ", 0);
	isDroidsFloorInverted = Hangars_Int("IsDroidsFloorInverted", 0) != 0;
	for (index = 0; index < 2; ++index) {
		static const char* const keys[2][7] = {
			{ "LoadDroid1", "Droid1PositionZ", "IsDroid1FloorInverted", "Droid1Update", "Droid1ModelIndex",
			  "Droid1Markings", "Droid1ObjectProfile" },
			{ "LoadDroid2", "Droid2PositionZ", "IsDroid2FloorInverted", "Droid2Update", "Droid2ModelIndex",
			  "Droid2Markings", "Droid2ObjectProfile" },
		};
		XwaHangarDroid* droid = &o->droids[index];

		droid->load = loadDroids && Hangars_Int(keys[index][0], 1) == 1;
		droid->positionZ = Hangars_Int(keys[index][1], droidsPositionZ);
		droid->isFloorInverted = isDroidsFloorInverted || Hangars_Int(keys[index][2], 0) != 0;
		droid->update = droid->load && Hangars_Int(keys[index][3], 1) != 0;
		droid->modelType = (uint16_t)Hangars_Int(keys[index][4], index == 0 ? OBJ_HangarDroid : OBJ_HangarDroid2);
		droid->markings = Hangars_Int(keys[index][5], 0);
		Hangars_Profile(keys[index][6], droid->objectProfile);
	}

	o->roofCranePosition[0] = Hangars_Int("HangarRoofCranePositionX", -1400);
	o->roofCranePosition[1] = Hangars_Int("HangarRoofCranePositionY", 786);
	o->roofCranePosition[2] = Hangars_Int("HangarRoofCranePositionZ", -282);
	o->roofCraneAxis = Hangars_Int("HangarRoofCraneAxis", 0);
	o->roofCraneLowOffset = Hangars_Int("HangarRoofCraneLowOffset", 0);
	o->roofCraneHighOffset = Hangars_Int("HangarRoofCraneHighOffset", 2800);
	o->isHangarFloorInverted = Hangars_Int("IsHangarFloorInverted", 0) != 0;
	o->hangarFloorInvertedHeight = Hangars_Int("HangarFloorInvertedHeight", 0);
	o->hangarIff = (uint8_t)Hangars_Int("HangarIff", -1);
	o->drawShadows = Hangars_Int("DrawShadows", o->isHangarFloorInverted ? 0 : 1) != 0;
	o->playerAnimationElevation = Hangars_Int("PlayerAnimationElevation", 0);
	o->playerAnimationInvertedElevation = Hangars_Int("PlayerAnimationInvertedElevation", o->playerAnimationElevation);
	o->playerAnimationStraightLine = Hangars_Int("PlayerAnimationStraightLine", 0);
	o->playerOffset[0] = Hangars_Int("PlayerOffsetX", 0);
	o->playerOffset[1] = Hangars_Int("PlayerOffsetY", 0);
	o->playerOffset[2] = Hangars_Int("PlayerOffsetZ", 0);
	o->playerInvertedOffset[0] = Hangars_Int("PlayerInvertedOffsetX", o->playerOffset[0]);
	o->playerInvertedOffset[1] = Hangars_Int("PlayerInvertedOffsetY", o->playerOffset[1]);
	o->playerInvertedOffset[2] = Hangars_Int("PlayerInvertedOffsetZ", o->playerOffset[2]);

	/* The per-type offsets apply only when the four lists match. */
	count = Hangars_List("PlayerModelIndices", types);
	if (Hangars_List("PlayerOffsetsX", offsets[0]) == count && Hangars_List("PlayerOffsetsY", offsets[1]) == count &&
		Hangars_List("PlayerOffsetsZ", offsets[2]) == count) {
		for (index = 0; index < count; ++index) {
			s_playerCrafts[index].objectType = (uint16_t)types[index];
			s_playerCrafts[index].offset[0] = offsets[0][index];
			s_playerCrafts[index].offset[1] = offsets[1][index];
			s_playerCrafts[index].offset[2] = offsets[2][index];
		}
		o->playerCrafts = s_playerCrafts;
		o->playerCraftCount = count;
	}
	o->isPlayerFloorInverted = Hangars_Int("IsPlayerFloorInverted", 0) != 0;
	count = Hangars_List("PlayerFloorInvertedModelIndices", types);
	for (index = 0; index < count; ++index) {
		s_floorInvertedTypes[index] = (uint16_t)types[index];
	}
	o->playerFloorInvertedTypes = s_floorInvertedTypes;
	o->playerFloorInvertedTypeCount = count;
}

/* The first layer's section `name`; NULL when no layer has it. */
static const AeronConfigNode* Hangars_Section(const char* name) {
	int layer;

	for (layer = 0; layer < s_layerCount; ++layer) {
		const AeronConfigNode* section = AeronConfigNode_MapGet(s_layers[layer], name);

		if (section != NULL) {
			return section;
		}
	}
	return NULL;
}

/* hook_hangars' layouts without a map section (HangarMapHook,
   FamHangarMapHook): the stock layouts at the highest detail level. */
typedef struct HangarsDefaultLine {
	uint16_t objectType;
	int x;
	int y;
	int z;
	uint16_t headingXY;
	uint16_t headingZ;
} HangarsDefaultLine;

#define HANGARS_FLOOR 0x7FFFFFFF

static const HangarsDefaultLine s_defaultMap[] = {
	{ 314, -1059, 1313, HANGARS_FLOOR, 38600, 0 },	   { 309, 725, -416, HANGARS_FLOOR, 23500, 0 },
	{ 313, -1352, 1440, HANGARS_FLOOR, 536, 0 },	   { 315, -1065, -56, HANGARS_FLOOR, 47736, 0 },
	{ 315, 2079, 733, HANGARS_FLOOR, 6400, 0 },		   { 315, -2488, 1215, HANGARS_FLOOR, 11864, 0 },
	{ 310, 794, -750, -848, 6800, 12500 },			   { 310, -803, -1464, HANGARS_FLOOR, 49200, 0 },
	{ 310, -1563, 944, HANGARS_FLOOR, 51436, 0 },	   { 83, -506, -2136, -722, 32836, 16480 },
	{ 83, -841, -1628, HANGARS_FLOOR, 53100, 0 },	   { 83, 1240, -2098, -722, 272, 16480 },
	{ 84, -834, -2034, HANGARS_FLOOR, 32336, 0 },	   { 80, -3474, -275, HANGARS_FLOOR, 29436, 0 },
	{ 2, -1038, -798, HANGARS_FLOOR, 7700, 0 },		   { 50, 1146, -1288, -741, 50536, 0 },
	{ 315, 820, -665, HANGARS_FLOOR, 49136, 0 },	   { 310, -873, -1456, -849, 60472, 20400 },
	{ 310, -3095, -592, HANGARS_FLOOR, 51436, 0 },	   { 83, -1751, -1996, HANGARS_FLOOR, 31172, 0 },
	{ 83, 979, -2103, HANGARS_FLOOR, 37036, 0 },	   { 84, -1374, -2068, HANGARS_FLOOR, 40536, 0 },
	{ 80, 2806, 1357, HANGARS_FLOOR, 20536, 0 },	   { 80, -195, -1910, HANGARS_FLOOR, 54372, 0 },
	{ 3, 2178, 506, -830, 40036, 1500 },			   { 2, 1100, -503, HANGARS_FLOOR, 49036, 0 },
	{ 1, -1003, 280, -825, 16600, 0 },
};

static const HangarsDefaultLine s_defaultFamilyMap[] = {
	{ 315, 821, -6618, HANGARS_FLOOR, 30600, 0 },	   { 314, 2431, -5447, HANGARS_FLOOR, 61536, 0 },
	{ 309, 626, -6397, HANGARS_FLOOR, 736, 0 },		   { 313, 1577, -6663, HANGARS_FLOOR, 500, 0 },
	{ 80, -1954, -7445, HANGARS_FLOOR, 31236, 0 },	   { 80, -1938, -7445, -4997, 31236, 0 },
	{ 83, -1231, -8544, -5003, 32736, 0x4000 },		   { 83, -1481, -8444, -5003, 45736, 0x4000 },
	{ 83, -1821, -8464, -5003, 32736, 0x4000 },		   { 84, -1834, -6154, HANGARS_FLOOR, 42736, 0 },
	{ 84, 1939, -8013, HANGARS_FLOOR, 31800, 0 },	   { 80, -1954, -7445, -4888, 31236, 0 },
	{ 80, -1978, -7386, -4781, 31736, 0 },			   { 310, 1484, -8339, -5133, 29700, 20400 },
	{ 310, 1384, -8339, HANGARS_FLOOR, 32036, 0 },	   { 83, -2061, -8344, -5003, 32736, 0x4000 },
	{ 83, -1511, -6734, HANGARS_FLOOR, 27736, 0x8000 },
};

static void Hangars_ResolveMap(int family) {
	const AeronConfigNode* section = Hangars_Section(family ? "fam_map" : "map");
	size_t count = AeronConfigNode_SequenceCount(section);
	size_t index;

	free(s_maps[family]);
	s_maps[family] = NULL;
	s_mapCounts[family] = 0;
	s_mapPresent[family] = 1;
	if (section == NULL) {
		const HangarsDefaultLine* lines = family ? s_defaultFamilyMap : s_defaultMap;

		count = family ? sizeof s_defaultFamilyMap / sizeof *lines : sizeof s_defaultMap / sizeof *lines;
		s_maps[family] = (XwaHangarMapEntry*)calloc(count, sizeof *s_maps[family]);
		if (s_maps[family] == NULL) {
			return;
		}
		for (index = 0; index < count; ++index) {
			XwaHangarMapEntry* entry = &s_maps[family][index];

			entry->objectType = lines[index].objectType;
			entry->position[0] = lines[index].x;
			entry->position[1] = lines[index].y;
			entry->position[2] = lines[index].z;
			entry->headingXY = lines[index].headingXY;
			entry->headingZ = lines[index].headingZ;
			strcpy(entry->objectProfile, "Default");
			entry->isFloorInverted = -1;
		}
		s_mapCounts[family] = (int)count;
		return;
	}
	if (count == 0) {
		return;
	}
	s_maps[family] = (XwaHangarMapEntry*)calloc(count, sizeof *s_maps[family]);
	if (s_maps[family] == NULL) {
		return;
	}
	for (index = 0; index < count; ++index) {
		const AeronConfigNode* line = AeronConfigNode_SequenceGet(section, index);
		XwaHangarMapEntry* entry = &s_maps[family][index];
		const char* profile;

		if (AeronConfigNode_SequenceCount(line) != 9) {
			continue; /* a line the hook skips: objectType 0 */
		}
		entry->objectType = (uint16_t)AeronConfigNode_Int(AeronConfigNode_SequenceGet(line, 0), 0);
		entry->markings = (int)AeronConfigNode_Int(AeronConfigNode_SequenceGet(line, 1), 0);
		entry->position[0] = (int)AeronConfigNode_Int(AeronConfigNode_SequenceGet(line, 2), 0);
		entry->position[1] = (int)AeronConfigNode_Int(AeronConfigNode_SequenceGet(line, 3), 0);
		entry->position[2] = (int)AeronConfigNode_Int(AeronConfigNode_SequenceGet(line, 4), 0);
		entry->headingXY = (uint16_t)AeronConfigNode_Int(AeronConfigNode_SequenceGet(line, 5), 0);
		entry->headingZ = (uint16_t)AeronConfigNode_Int(AeronConfigNode_SequenceGet(line, 6), 0);
		profile = AeronConfigNode_String(AeronConfigNode_SequenceGet(line, 7), NULL);
		snprintf(entry->objectProfile, sizeof entry->objectProfile, "%s", profile != NULL ? profile : "Default");
		entry->isFloorInverted = (int8_t)AeronConfigNode_Int(AeronConfigNode_SequenceGet(line, 8), -1);
	}
	s_mapCounts[family] = (int)count;
}

static void Hangars_ResolveCamera(int family) {
	const AeronConfigNode* section = Hangars_Section(family ? "fam_camera" : "camera");
	XwaHangarCamera* camera = &s_cameras[family];
	size_t index;

	memset(camera, 0, sizeof *camera);
	camera->present = section != NULL;
	for (index = 0; index < AeronConfigNode_MapCount(section); ++index) {
		const int key = atoi(AeronConfigNode_MapKeyAt(section, index));
		const AeronConfigNode* offset = AeronConfigNode_MapValueAt(section, index);
		int axis;

		if (key < 0 || key >= XWA_HANGAR_CAMERA_KEYS || AeronConfigNode_SequenceCount(offset) != 3) {
			continue;
		}
		for (axis = 0; axis < 3; ++axis) {
			camera->offsets[key][axis] = (int)AeronConfigNode_Int(AeronConfigNode_SequenceGet(offset, axis), 0);
		}
	}
}

/* Each source model's first swap, in layer order. */
static void Hangars_ResolveSwaps(void) {
	int layer;

	s_swapCount = 0;
	for (layer = 0; layer < s_layerCount; ++layer) {
		const AeronConfigNode* swaps = AeronConfigNode_MapGet(s_layers[layer], "swaps");
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

int XwaHangars_Resolve(void) {
	const AeronConfigNode* root;
	const AeronConfigNode* ship = NULL;
	HangarsKey key;
	uint16_t objectType;
	uint8_t iff;
	uint8_t markings;
	int hasShip;
	int changed;

	if (s_settings == NULL) {
		return 0;
	}
	root = AeronConfigFile_Root(s_settings);
	key[0] = '\0';
	hasShip = !g_provingGroundsModeActive && Hangars_CommandShip(&objectType, &iff, &markings) &&
			  XwaModelSource_TypeKey(objectType, 1, key);
	if (!hasShip) {
		key[0] = '\0';
		iff = 0;
		markings = 0;
	} else {
		/* A mission that swaps the mothership's model uses the substitute's
		   settings and hangar. */
		const char* substitute =
			AeronConfigNode_String(AeronConfigNode_MapGet(AeronConfigNode_MapGet(s_mission, "ships"), key), NULL);

		if (substitute != NULL && strlen(substitute) < sizeof key) {
			strcpy(key, substitute);
		}
		ship = AeronConfigNode_MapGet(AeronConfigNode_MapGet(root, "ships"), key);
	}
	changed = !s_resolved || s_provingGround != g_provingGroundsModeActive || strcmp(s_shipKey, key) != 0 ||
			  s_shipIff != (hasShip ? iff : -1);
	s_resolved = 1;
	s_inHangar = 1;
	s_provingGround = g_provingGroundsModeActive;
	strcpy(s_shipKey, key);
	s_shipIff = hasShip ? iff : -1;
	s_shipMarkings = markings;

	/* mission > ship IFF > ship > default; the proving ground reads the
	   default only. */
	s_layerCount = 0;
	if (!g_provingGroundsModeActive && s_mission != NULL) {
		s_layers[s_layerCount++] = s_mission;
	}
	if (ship != NULL) {
		char iffKey[8];
		const AeronConfigNode* variant;

		snprintf(iffKey, sizeof iffKey, "%u", (unsigned int)iff);
		variant = AeronConfigNode_MapGet(AeronConfigNode_MapGet(ship, "iff"), iffKey);
		if (variant != NULL) {
			s_layers[s_layerCount++] = variant;
		}
		s_layers[s_layerCount++] = ship;
	}
	if (AeronConfigNode_MapGet(root, "default") != NULL) {
		s_layers[s_layerCount++] = AeronConfigNode_MapGet(root, "default");
	}

	Hangars_ResolveObjects();
	Hangars_ResolveMap(0);
	Hangars_ResolveMap(1);
	Hangars_ResolveCamera(0);
	Hangars_ResolveCamera(1);
	Hangars_ResolveSwaps();
	s_hangarModel[0] = '\0';
	{
		const char* model = AeronConfigNode_String(AeronConfigNode_MapGet(ship, "model"), NULL);

		if (model != NULL && strlen(model) < sizeof s_hangarModel && XwaModelSource_AddKey(model)) {
			strcpy(s_hangarModel, model);
		}
	}
	if (changed) {
		Aeron_LogInfo("xwa.models", "hangar: ship '%s' IFF %d (%s), model '%s', %d layers, %d map lines, %d swaps",
					  hasShip ? s_shipKey : "", s_shipIff, ship != NULL ? "settings" : "default settings",
					  s_hangarModel[0] != '\0' ? s_hangarModel : "hangar", s_layerCount, s_mapCounts[0],
					  s_swapCount);
	}
	return changed;
}

const XwaHangarObjects* XwaHangars_Objects(void) { return &s_objects; }

const XwaHangarMapEntry* XwaHangars_Map(int family, int* count) {
	family = family != 0;
	*count = s_mapCounts[family];
	return s_mapPresent[family] ? s_maps[family] : NULL;
}

const XwaHangarCamera* XwaHangars_Camera(int family) { return &s_cameras[family != 0]; }

uint8_t XwaHangars_CommandShipMarkings(void) { return s_shipMarkings; }

/* ---- models ------------------------------------------------------------- */

/* The key the settings load for `objectType`; empty for the stock model. */
static void Hangars_DesiredKey(uint16_t objectType, char* key) {
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
	if (!s_inHangar || !XwaModelSource_TypeKey(objectType, 1, typeKey)) {
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

	Hangars_DesiredKey(objectType, key);
	if (key[0] == '\0') {
		return 0;
	}
	written = snprintf(fileName, fileNameSize, "%s.opt", key);
	return written > 0 && (unsigned int)written < fileNameSize;
}

int XwaHangars_TypeFileName(uint16_t objectType, char* fileName, unsigned int fileNameSize) {
	HangarsKey key;
	int written;

	if (s_settings == NULL || !XwaModelSource_TypeKey(objectType, 0, key)) {
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
	Hangars_DesiredKey(objectType, s_loaded[objectType].key);
}

void XwaHangars_ReleaseStaleModel(uint16_t objectType) {
	const uint16_t handle = objectType < XWA_LOADED_MODEL_COUNT ? g_loadedModels.byObjectType[objectType] : 0;
	HangarsKey desired;
	const char* loaded;

	if (handle == 0) {
		return;
	}
	/* A handle this module did not see loaded holds the stock model. */
	loaded = s_loaded[objectType].handle == handle ? s_loaded[objectType].key : "";
	Hangars_DesiredKey(objectType, desired);
	if (strcmp(loaded, desired) == 0) {
		return;
	}
	Aeron_LogInfo("xwa.models", "hangar: type %u reloads '%s' instead of '%s'", (unsigned int)objectType,
				  desired[0] != '\0' ? desired : "stock", loaded[0] != '\0' ? loaded : "stock");
	OptModel_FreeHandle(handle);
	g_loadedModels.byObjectType[objectType] = 0;
	g_modelTypeTable[objectType].curTexLevel = NULL;
	g_modelBoundsCached[objectType] = 0;
	s_loaded[objectType].handle = 0;
}

/* Gives every type the objects of the hangar (`hangar`) or of the rest of
   the mission use the model the settings want for them now. */
static void Hangars_ReloadUsedTypes(int hangar) {
	static uint8_t used[XWA_LOADED_MODEL_COUNT];
	uint8_t hangarFlightGroup;
	uint32_t objIdx;
	unsigned int objectType;

	if (s_settings == NULL || g_launchRefObjIdx < 0 || (uint32_t)g_launchRefObjIdx >= g_objectTableSlotCount) {
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

void XwaHangars_ReapplyHangarModels(void) { Hangars_ReloadUsedTypes(1); }

void XwaHangars_LeaveHangar(void) {
	if (!s_inHangar) {
		return;
	}
	s_inHangar = 0;
	Hangars_ReloadUsedTypes(0);
}

/* ---- placement ---------------------------------------------------------- */

int XwaHangars_CraftElevation(uint16_t objectType, int inverted) {
	const AeronConfigNode* elevation = NULL;
	HangarsKey key;

	Hangars_DesiredKey(objectType, key);
	if (s_settings != NULL && (key[0] != '\0' || XwaModelSource_TypeKey(objectType, 1, key))) {
		const AeronConfigNode* crafts = AeronConfigNode_MapGet(AeronConfigFile_Root(s_settings), "crafts");

		elevation = AeronConfigNode_MapGet(AeronConfigNode_MapGet(crafts, key), "elevation");
	}
	if (AeronConfigNode_SequenceCount(elevation) == 2) {
		return (int)AeronConfigNode_Int(AeronConfigNode_SequenceGet(elevation, inverted != 0), 0);
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
	const AeronConfigNode* camera = NULL;
	const AeronConfigNode* triple;
	HangarsKey key;
	int axis;

	/* The hook reads the [Camera] of the type's own model, never a swap. */
	if (s_settings != NULL && XwaModelSource_TypeKey(objectType, 1, key)) {
		const AeronConfigNode* crafts = AeronConfigNode_MapGet(AeronConfigFile_Root(s_settings), "crafts");

		camera = AeronConfigNode_MapGet(AeronConfigNode_MapGet(crafts, key), "camera");
	}
	triple = AeronConfigNode_SequenceGet(camera, (size_t)((family != 0) * 2 + (inverted != 0)));
	if (AeronConfigNode_SequenceCount(camera) != 4 || AeronConfigNode_SequenceCount(triple) != 3) {
		return 0;
	}
	for (axis = 0; axis < 3; ++axis) {
		offset[axis] = (int)AeronConfigNode_Int(AeronConfigNode_SequenceGet(triple, (size_t)axis), 0);
	}
	return 1;
}
