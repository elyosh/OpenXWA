#include "xwa_runtime/models/turret_ignore.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/flight/object/object.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/models/model_settings.h"
#include "xwa_runtime/models/names.h"

#include <stdint.h>
#include <string.h>

/* Modern port module; no original counterpart. */

enum {
	TURRET_IGNORE_FIRST_WEAPON = 280,
	TURRET_IGNORE_WEAPONS = 28,
	/* Genus values are 0 to 21. Bit 22 is for warheads. */
	TURRET_IGNORE_GENUS_COUNT = 22,
};

#define TURRET_IGNORE_WARHEAD_BIT (1u << TURRET_IGNORE_GENUS_COUNT)

/* The lists of a model file or of flight/flight.yaml. `hasList` has a bit
   for each weapon type that has a list, so that an empty list counts too.
   `mask` has a bit for each genus that the list ignores, and
   TURRET_IGNORE_WARHEAD_BIT. */
typedef struct TurretIgnoreSet {
	uint32_t hasList;
	uint32_t mask[TURRET_IGNORE_WEAPONS];
} TurretIgnoreSet;

static TurretIgnoreSet s_defaults;
static TurretIgnoreSet s_models[OBJ_Count];
/* The selected list, and the object it was selected for, or -1. */
static int s_selectedObjIdx = -1;
static uint32_t s_selectedMask;

/* Reads the names of `list`, the list of weapon type `weaponKey`, into a
   mask. Logs the names that it doesn't know. */
static uint32_t TurretIgnore_ReadList(const char* path, const char* weaponKey, const AeronConfigNode* list) {
	uint32_t mask = 0;
	size_t index;

	for (index = 0; index < AeronConfigNode_SequenceCount(list); ++index) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(list, index);
		const char* name = AeronConfigNode_String(item, NULL);
		int genus;

		if (AeronConfigNode_Type(item) == AERON_CONFIG_STRING && name != NULL &&
			strcmp(name, "warhead") == 0) {
			mask |= TURRET_IGNORE_WARHEAD_BIT;
			continue;
		}
		genus = AeronConfigNode_Type(item) == AERON_CONFIG_STRING && name != NULL
					? XwaNames_Find(&g_xwaGenusNames, name)
					: -1;
		if (genus < 0 || genus >= TURRET_IGNORE_GENUS_COUNT) {
			Aeron_LogWarn("xwa.models",
						  "%s:%d: weapons.turret_ignore.%s: %s isn't a genus name or warhead. It is ignored",
						  path, AeronConfigNode_Line(item), weaponKey, name != NULL ? name : "the value");
			continue;
		}
		mask |= 1u << genus;
	}
	return mask;
}

/* Reads the `turret_ignore` group of the `weapons` map `weapons` into
   `set`. */
static void TurretIgnore_ReadSet(const char* path, const AeronConfigNode* weapons, TurretIgnoreSet* set) {
	const AeronConfigNode* group = AeronConfigNode_MapGet(weapons, "turret_ignore");
	size_t index;

	memset(set, 0, sizeof *set);
	if (group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: weapons.turret_ignore isn't a map. It is ignored", path);
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(group); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(group, index);
		const AeronConfigNode* list = AeronConfigNode_MapValueAt(group, index);
		const int weapon = XwaNames_Find(&g_xwaWeaponTypeNames, key) - TURRET_IGNORE_FIRST_WEAPON;

		if (weapon < 0 || weapon >= TURRET_IGNORE_WEAPONS) {
			Aeron_LogWarn("xwa.models", "%s: weapons.turret_ignore.%s isn't a weapon type. It is ignored",
						  path, key);
			continue;
		}
		if (AeronConfigNode_Type(list) != AERON_CONFIG_SEQUENCE) {
			Aeron_LogWarn("xwa.models", "%s:%d: weapons.turret_ignore.%s must be a list. It is ignored", path,
						  AeronConfigNode_Line(list), key);
			continue;
		}
		set->hasList |= 1u << weapon;
		set->mask[weapon] = TurretIgnore_ReadList(path, key, list);
	}
}

void XwaTurretIgnore_Init(void) {
	const AeronConfigNode* section = AeronConfigNode_MapGet(XwaFlightConfig_Root(), "weapons");

	memset(&s_defaults, 0, sizeof s_defaults);
	XwaTurretIgnore_Clear();
	if (section != NULL && AeronConfigNode_Type(section) == AERON_CONFIG_MAP) {
		TurretIgnore_ReadSet(XWA_FLIGHT_CONFIG_PATH, section, &s_defaults);
	}
}

void XwaTurretIgnore_ClearModelSettings(void) {
	memset(s_models, 0, sizeof s_models);
	XwaTurretIgnore_Clear();
}

void XwaTurretIgnore_ReadModelSettings(unsigned int objectType, const char* path,
									   const AeronConfigNode* group) {
	/* The weapon colours code logs a `weapons` group that isn't a map. */
	if (objectType >= OBJ_Count || AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		return;
	}
	TurretIgnore_ReadSet(path, group, &s_models[objectType]);
}

void XwaTurretIgnore_Select(unsigned int objIdx, unsigned int rackIdx) {
	const unsigned int objectType = (uint16_t)g_objectTable[objIdx].objectType;
	const unsigned int weapon =
		(unsigned int)g_curCraft->warheadData[rackIdx].projectileTypeId - TURRET_IGNORE_FIRST_WEAPON;

	s_selectedObjIdx = (int)objIdx;
	s_selectedMask = 0;
	if (weapon >= TURRET_IGNORE_WEAPONS) {
		return;
	}
	XwaModelSettings_Ensure(objectType);
	if (objectType < OBJ_Count && (s_models[objectType].hasList & (1u << weapon)) != 0) {
		s_selectedMask = s_models[objectType].mask[weapon];
	} else if ((s_defaults.hasList & (1u << weapon)) != 0) {
		s_selectedMask = s_defaults.mask[weapon];
	}
}

void XwaTurretIgnore_Clear(void) {
	s_selectedObjIdx = -1;
	s_selectedMask = 0;
}

int XwaTurretIgnore_Skips(unsigned int objIdx) {
	const ObjectRecord* object;
	const unsigned int genus = (unsigned int)g_objectTable[objIdx].genusId;

	if (s_selectedMask == 0) {
		return 0;
	}
	object = &g_objectTable[objIdx];
	if (object->objectType == OBJ_None) {
		return 0;
	}
	/* A warhead is a projectile that the AI can target. Laser bolts are
	   projectiles too, but the AI can't target them. */
	if ((s_selectedMask & TURRET_IGNORE_WARHEAD_BIT) != 0 &&
		(genus == GENUS_PlayerProjectile || genus == GENUS_NpcProjectile) &&
		(g_objectTypes[(uint16_t)object->objectType].flags & 2u) != 0) {
		return 1;
	}
	return genus < TURRET_IGNORE_GENUS_COUNT && (s_selectedMask & (1u << genus)) != 0;
}

int XwaTurretIgnore_SkipsForCraft(unsigned int craftObjIdx, unsigned int objIdx) {
	return s_selectedObjIdx == (int)craftObjIdx && XwaTurretIgnore_Skips(objIdx);
}
