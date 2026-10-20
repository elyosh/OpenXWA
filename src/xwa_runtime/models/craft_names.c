#include "xwa_runtime/models/craft_names.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/flight_model.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/assets/ship_list.h"
#include "xwa/assets/string_table.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/models/model_source.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* XU's missions rename up to 10 models. */
#define CRAFT_NAMES_MAX 64
#define CRAFT_NAMES_TEXT_SIZE 256
/* g_strSpeciesNamesPlural holds 225 records. */
#define CRAFT_NAMES_RECORDS 225

typedef struct CraftNamesEntry {
	char model[XWA_MODEL_SOURCE_KEY_MAX];
	char shipList[CRAFT_NAMES_TEXT_SIZE];
	char name[CRAFT_NAMES_TEXT_SIZE];
	char shortName[CRAFT_NAMES_TEXT_SIZE];
	char plural[CRAFT_NAMES_TEXT_SIZE];
	int hasShipList, hasName, hasShortName, hasPlural, hasIcon;
	FrontendRect icon;
} CraftNamesEntry;

/* The values before the first change, to put back. */
typedef struct CraftNamesSavedRecord {
	int saved;
	char* shortName;
	char* longName;
	char* plural;
} CraftNamesSavedRecord;

typedef struct CraftNamesSavedShip {
	int saved;
	char name[sizeof((ShipListEntry*)0)->name];
	FrontendRect icon;
} CraftNamesSavedShip;

/* The mission that was read, and the one whose names are set: Apply copies
   the first into the second, since the names point into it. */
static CraftNamesEntry s_read[CRAFT_NAMES_MAX];
static int s_readCount;
static CraftNamesEntry s_applied[CRAFT_NAMES_MAX];
static int s_appliedCount;
static CraftNamesSavedRecord s_savedRecords[CRAFT_NAMES_RECORDS];
static CraftNamesSavedShip s_savedShips[SHIP_LIST_MAX_ENTRIES];

static int CraftNames_ReadText(const char* path, const char* model, const AeronConfigNode* entry,
							   const char* key, char* out, int* has) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(entry, key);
	const char* text = AeronConfigNode_String(node, NULL);

	if (node == NULL) {
		return 1;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING || strlen(text) >= CRAFT_NAMES_TEXT_SIZE) {
		Aeron_LogWarn("xwa.mission",
					  "%s:%d: craft_names: %s: %s must be a text of up to %d characters; it is "
					  "ignored",
					  path, AeronConfigNode_Line(node), model, key, CRAFT_NAMES_TEXT_SIZE - 1);
		return 0;
	}
	snprintf(out, CRAFT_NAMES_TEXT_SIZE, "%s", text);
	*has = 1;
	return 1;
}

static void CraftNames_ReadIcon(const char* path, const char* model, const AeronConfigNode* entry,
								CraftNamesEntry* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(entry, "map_icon");
	int32_t values[4];
	int i;

	if (node == NULL) {
		return;
	}
	for (i = 0; i < 4; ++i) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, (size_t)i);

		if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) != 4 ||
			AeronConfigNode_Type(item) != AERON_CONFIG_INT) {
			Aeron_LogWarn(
				"xwa.mission",
				"%s:%d: craft_names: %s: map_icon must be [left, top, right, bottom]; it is ignored", path,
				AeronConfigNode_Line(node), model);
			return;
		}
		values[i] = (int32_t)AeronConfigNode_Int(item, 0);
	}
	out->icon.left = values[0];
	out->icon.top = values[1];
	out->icon.right = values[2];
	out->icon.bottom = values[3];
	out->hasIcon = 1;
}

void XwaCraftNames_LoadMission(const char* missionPath) {
	AeronConfigFile* config = NULL;
	const AeronConfigNode* names;
	char path[300];
	size_t i;

	s_readCount = 0;
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	names = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "craft_names");
	if (names != NULL && AeronConfigNode_Type(names) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.mission", "%s:%d: craft_names must be a map of model keys; it is ignored", path,
					   AeronConfigNode_Line(names));
		names = NULL;
	}
	for (i = 0; i < AeronConfigNode_MapCount(names); ++i) {
		const char* model = AeronConfigNode_MapKeyAt(names, i);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(names, i);
		CraftNamesEntry* out = &s_read[s_readCount];

		if (s_readCount == CRAFT_NAMES_MAX) {
			Aeron_LogError("xwa.mission", "%s: names for more than %d models; the rest are ignored", path,
						   CRAFT_NAMES_MAX);
			break;
		}
		memset(out, 0, sizeof *out);
		if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP ||
			!XwaModelSource_NormalizeKey(model, out->model) || strcmp(out->model, model) != 0) {
			Aeron_LogWarn("xwa.mission",
						  "%s:%d: craft_names: '%s' must be a model key with a map of names; it is "
						  "ignored",
						  path, AeronConfigNode_Line(entry), model);
			continue;
		}
		CraftNames_ReadText(path, model, entry, "ship_list", out->shipList, &out->hasShipList);
		CraftNames_ReadText(path, model, entry, "name", out->name, &out->hasName);
		CraftNames_ReadText(path, model, entry, "short_name", out->shortName, &out->hasShortName);
		CraftNames_ReadText(path, model, entry, "plural", out->plural, &out->hasPlural);
		CraftNames_ReadIcon(path, model, entry, out);
		++s_readCount;
	}
	AeronConfigFile_Destroy(config);
}

/* The craft record of the first object type whose model is `model`, or
   -1. */
static int CraftNames_Record(const char* model, unsigned int* objectType) {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	unsigned int type;

	for (type = 0; type < OBJ_Count; ++type) {
		if (XwaModelSource_TypeKey(type, key) && strcmp(key, model) == 0) {
			*objectType = type;
			return g_objectTypes[type].modelDefIndex;
		}
	}
	return -1;
}

/* The ship list entry of the craft type of the first object type that uses
   craft record `record`, or -1. */
static int CraftNames_ShipListIndex(int record) {
	unsigned int type;

	for (type = 0; type < OBJ_Count; ++type) {
		if (g_objectTypes[type].modelDefIndex == record) {
			const int craftType = Craft_FindCraftTypeForObjectType((unsigned short)type);
			int index;

			if (craftType < 0 || craftType >= SHIP_TYPE_TO_SHIP_LIST_CAPACITY || g_shipList == NULL) {
				return -1;
			}
			index = g_shipTypeToShipListIndex[craftType];
			return index >= 0 && index < g_shipCount ? index : -1;
		}
	}
	return -1;
}

void XwaCraftNames_Restore(void) {
	int i;

	for (i = 0; i < CRAFT_NAMES_RECORDS; ++i) {
		CraftNamesSavedRecord* saved = &s_savedRecords[i];

		if (saved->saved) {
			g_modelDefs[i].shortName = saved->shortName;
			g_modelDefs[i].longName = saved->longName;
			g_strSpeciesNamesPlural[i] = saved->plural;
			saved->saved = 0;
		}
	}
	for (i = 0; i < SHIP_LIST_MAX_ENTRIES; ++i) {
		CraftNamesSavedShip* saved = &s_savedShips[i];

		if (saved->saved && g_shipList != NULL && i < g_shipCount) {
			memcpy(g_shipList[i].name, saved->name, sizeof saved->name);
			g_shipList[i].iconRect = saved->icon;
		}
		saved->saved = 0;
	}
	s_appliedCount = 0;
}

void XwaCraftNames_Apply(void) {
	int i;
	int set = 0;

	XwaCraftNames_Restore();
	memcpy(s_applied, s_read, sizeof s_read[0] * (size_t)s_readCount);
	s_appliedCount = s_readCount;
	for (i = 0; i < s_appliedCount; ++i) {
		const CraftNamesEntry* entry = &s_applied[i];
		unsigned int objectType = 0;
		const int record = CraftNames_Record(entry->model, &objectType);
		int ship;

		if (record < 0 || record >= CRAFT_NAMES_RECORDS || record >= XWA_MODEL_DEF_COUNT) {
			Aeron_LogWarn("xwa.mission",
						  "craft_names: no object type with a craft record has the model %s; its "
						  "names are ignored",
						  entry->model);
			continue;
		}
		if (!s_savedRecords[record].saved) {
			s_savedRecords[record].saved = 1;
			s_savedRecords[record].shortName = g_modelDefs[record].shortName;
			s_savedRecords[record].longName = g_modelDefs[record].longName;
			s_savedRecords[record].plural = g_strSpeciesNamesPlural[record];
		}
		if (entry->hasName) {
			g_modelDefs[record].longName = (char*)entry->name;
		}
		if (entry->hasShortName) {
			g_modelDefs[record].shortName = (char*)entry->shortName;
		}
		if (entry->hasPlural) {
			g_strSpeciesNamesPlural[record] = (char*)entry->plural;
		}
		ship = (entry->hasShipList || entry->hasIcon) ? CraftNames_ShipListIndex(record) : -1;
		if (ship >= 0) {
			if (!s_savedShips[ship].saved) {
				s_savedShips[ship].saved = 1;
				memcpy(s_savedShips[ship].name, g_shipList[ship].name, sizeof s_savedShips[ship].name);
				s_savedShips[ship].icon = g_shipList[ship].iconRect;
			}
			if (entry->hasShipList) {
				snprintf(g_shipList[ship].name, sizeof g_shipList[ship].name, "%s", entry->shipList);
			}
			if (entry->hasIcon) {
				g_shipList[ship].iconRect = entry->icon;
			}
		}
		++set;
	}
	if (set != 0) {
		Aeron_LogInfo("xwa.mission", "craft names set: %d", set);
	}
}

const char* XwaCraftNames_ShipListName(int craftType) {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	int i;

	if (craftType < 0 || craftType >= XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT ||
		!XwaModelSource_TypeKey(g_objectTypeTables.craftTypeToObjectType[craftType], key)) {
		return NULL;
	}
	for (i = 0; i < s_readCount; ++i) {
		if (s_read[i].hasShipList && strcmp(s_read[i].model, key) == 0) {
			return s_read[i].shipList;
		}
	}
	return NULL;
}
