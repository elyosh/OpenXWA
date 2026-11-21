#include "xwa_runtime/models/hangar_crafts.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/models/names.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

typedef struct HangarCraftList {
	uint16_t types[XWA_HANGAR_CRAFTS_MAX];
	int count;
} HangarCraftList;

static const char* const s_keys[XWA_HANGAR_CRAFT_LISTS] = { "fighters", "transports", "all" };

/* The original game's lists, in its menu order (Hangar_SetupReadyScene). */
static const HangarCraftList s_stock[XWA_HANGAR_CRAFT_LISTS] = {
	{ { OBJ_XWing, OBJ_YWing, OBJ_AWing, OBJ_BWing, OBJ_Z95 }, 5 },
	{ { OBJ_CorellianTransport2, OBJ_FamilyTransport, OBJ_MilleniumFalcon2 }, 3 },
	{ { OBJ_XWing, OBJ_YWing, OBJ_AWing, OBJ_BWing, OBJ_Z95, OBJ_CorellianTransport2, OBJ_FamilyTransport,
		OBJ_MilleniumFalcon2 },
	  8 },
};

static HangarCraftList s_lists[XWA_HANGAR_CRAFT_LISTS];
static int s_set;

/* Reads list `key` into `out`. Returns 0 when the value isn't a list. */
static int HangarCrafts_ReadList(const char* key, const AeronConfigNode* node, HangarCraftList* out) {
	const size_t count = AeronConfigNode_SequenceCount(node);
	char setting[32];
	size_t index;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE) {
		Aeron_LogWarn("xwa.models", "%s:%d: hangar_crafts.%s must be a list of object types; it is ignored",
					  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), key);
		return 0;
	}
	snprintf(setting, sizeof setting, "hangar_crafts.%s", key);
	out->count = 0;
	for (index = 0; index < count; ++index) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, index);
		const char* name = AeronConfigNode_String(item, "");
		const int objectType = XwaNames_Find(&g_xwaObjectTypeNames, name);
		int other;

		if (objectType < 0 || objectType >= OBJ_Count) {
			XwaNames_LogUnknown(&g_xwaObjectTypeNames, XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(item),
								setting, name);
			continue;
		}
		/* Hangar_BuildCraftSelectMenu reads the name from the craft record,
		   and indexes g_modelDefs with -1 when there is none. */
		if (objectType == OBJ_None || GetModelDefIndexFromType((uint16_t)objectType) < 0) {
			Aeron_LogWarn("xwa.models",
						  "%s:%d: %s: %s has no craft record, so the player can't fly it; it is ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(item), setting, name);
			continue;
		}
		for (other = 0; other < out->count && out->types[other] != (uint16_t)objectType; ++other) {
		}
		if (other < out->count) {
			Aeron_LogWarn("xwa.models", "%s:%d: %s lists %s twice; the second one is ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(item), setting, name);
			continue;
		}
		if (out->count == XWA_HANGAR_CRAFTS_MAX) {
			Aeron_LogWarn("xwa.models", "%s:%d: %s has more than %d craft; the rest are ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(item), setting, XWA_HANGAR_CRAFTS_MAX);
			break;
		}
		out->types[out->count++] = (uint16_t)objectType;
	}
	return 1;
}

void XwaHangarCrafts_Init(void) {
	const AeronConfigNode* section = AeronConfigNode_MapGet(XwaFlightConfig_Root(), "hangar_crafts");
	size_t index;
	int list;

	memcpy(s_lists, s_stock, sizeof s_lists);
	s_set = 0;
	if (section == NULL) {
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: hangar_crafts must be a map; it is ignored",
					  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(section));
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(section); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(section, index);

		for (list = 0; list < XWA_HANGAR_CRAFT_LISTS && strcmp(key, s_keys[list]) != 0; ++list) {
		}
		if (list == XWA_HANGAR_CRAFT_LISTS) {
			Aeron_LogWarn("xwa.models", "%s:%d: hangar_crafts.%s isn't a setting; it is ignored",
						  XWA_FLIGHT_CONFIG_PATH,
						  AeronConfigNode_Line(AeronConfigNode_MapValueAt(section, index)), key);
			continue;
		}
		if (HangarCrafts_ReadList(key, AeronConfigNode_MapValueAt(section, index), &s_lists[list])) {
			s_set = 1;
		}
	}
}

int XwaHangarCrafts_IsSet(void) { return s_set; }

int XwaHangarCrafts_IsTransport(uint16_t objectType) {
	const HangarCraftList* transports = &s_lists[XWA_HANGAR_CRAFTS_TRANSPORTS];
	int index;

	for (index = 0; index < transports->count; ++index) {
		if (transports->types[index] == objectType) {
			return 1;
		}
	}
	return 0;
}

int XwaHangarCrafts_Fill(XwaHangarCraftList list, uint16_t playerType, uint16_t* out, int count) {
	const HangarCraftList* craft;
	int index;

	if ((unsigned)list >= XWA_HANGAR_CRAFT_LISTS) {
		return count;
	}
	craft = &s_lists[list];
	for (index = 0; index < craft->count; ++index) {
		if (craft->types[index] != playerType) {
			out[count++] = craft->types[index];
		}
	}
	return count;
}
