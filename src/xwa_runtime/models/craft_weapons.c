#include "xwa_runtime/models/craft_weapons.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/object/object.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/models/mission_settings.h"
#include "xwa_runtime/models/model_settings.h"
#include "xwa_runtime/models/skins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define CRAFT_WEAPONS_FLIGHT_YAML "flight/flight.yaml"

enum {
	CRAFT_WEAPONS_FIRST = OBJ_LaserRebel,
	CRAFT_WEAPONS_COUNT = 28,
};

static const char* const s_barNames[XWA_CRAFT_WEAPONS_BARS] = { "laser_high", "laser_low", "ion_high",
																"ion_low" };

/* The colours for any markings, or the changes for one markings value. -1
   means not set. */
typedef struct WeaponColorSet {
	int16_t defaultVariant;
	int16_t variant[CRAFT_WEAPONS_COUNT];
	int32_t impact[CRAFT_WEAPONS_COUNT];
	uint32_t bar[XWA_CRAFT_WEAPONS_BARS];
	uint8_t barMask;
} WeaponColorSet;

typedef struct CraftWeaponsType {
	WeaponColorSet colors;
	int8_t switchByIff; /* 1 or 0, -1 when not set */
	uint16_t markingsCount;
	uint8_t markings[256];
	WeaponColorSet byMarkings[];
} CraftWeaponsType;

static CraftWeaponsType* s_types[OBJ_Count];
static int32_t s_globalImpact[CRAFT_WEAPONS_COUNT];
/* The impact colour of each request of g_glowMarkRequestPool. */
static uint32_t s_requestTints[sizeof g_glowMarkRequestPool / sizeof g_glowMarkRequestPool[0]];

static void CraftWeapons_ClearSet(WeaponColorSet* set) {
	memset(set, 0, sizeof *set);
	set->defaultVariant = -1;
	memset(set->variant, 0xff, sizeof set->variant);
	memset(set->impact, 0xff, sizeof set->impact);
}

/* Returns the weapon index of map key `key` (280 to 307), or -1. */
static int CraftWeapons_WeaponKey(const char* key) {
	char* end;
	const long type = strtol(key, &end, 10);

	if (*key == '\0' || *end != '\0' || type < CRAFT_WEAPONS_FIRST ||
		type >= CRAFT_WEAPONS_FIRST + CRAFT_WEAPONS_COUNT) {
		return -1;
	}
	return (int)(type - CRAFT_WEAPONS_FIRST);
}

/* Reads `impact`, a map from weapon type to an RGB colour, into `impact`. */
static void CraftWeapons_ReadImpact(const char* path, const char* where, const AeronConfigNode* node,
									int32_t impact[CRAFT_WEAPONS_COUNT]) {
	size_t index;

	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
		const int weapon = CraftWeapons_WeaponKey(key);
		const int64_t rgb = AeronConfigNode_Int(value, -1);

		if (weapon < 0 || AeronConfigNode_Type(value) != AERON_CONFIG_INT || rgb < 0 || rgb > 0xffffff) {
			Aeron_LogWarn(
				"xwa.models",
				"%s: %s.impact.%s needs a weapon type from 280 to 307 and an RGB colour. It is ignored", path,
				where, key);
			continue;
		}
		impact[weapon] = (int32_t)rgb;
	}
}

static void CraftWeapons_ReadSet(const char* path, const char* where, const AeronConfigNode* node,
								 WeaponColorSet* set) {
	const AeronConfigNode* variant = AeronConfigNode_MapGet(node, "variant");
	const AeronConfigNode* bars = AeronConfigNode_MapGet(node, "energy_bars");
	size_t index;
	int bar;

	for (index = 0; index < AeronConfigNode_MapCount(variant); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(variant, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(variant, index);
		const int64_t markings = AeronConfigNode_Int(value, -1);
		const int weapon = strcmp(key, "default") == 0 ? CRAFT_WEAPONS_COUNT : CraftWeapons_WeaponKey(key);

		if (weapon < 0 || AeronConfigNode_Type(value) != AERON_CONFIG_INT || markings < 0 || markings > 255) {
			Aeron_LogWarn("xwa.models",
						  "%s: %s.variant.%s needs 'default' or a weapon type from 280 to 307, and markings "
						  "from 0 to 255. It is ignored",
						  path, where, key);
			continue;
		}
		if (weapon == CRAFT_WEAPONS_COUNT) {
			set->defaultVariant = (int16_t)markings;
		} else {
			set->variant[weapon] = (int16_t)markings;
		}
	}
	CraftWeapons_ReadImpact(path, where, AeronConfigNode_MapGet(node, "impact"), set->impact);
	for (bar = 0; bar < XWA_CRAFT_WEAPONS_BARS; ++bar) {
		const AeronConfigNode* value = AeronConfigNode_MapGet(bars, s_barNames[bar]);
		const int64_t argb = AeronConfigNode_Int(value, -1);

		if (value == NULL) {
			continue;
		}
		if (AeronConfigNode_Type(value) != AERON_CONFIG_INT || argb <= 0 || argb > UINT32_MAX) {
			Aeron_LogWarn("xwa.models", "%s: %s.energy_bars.%s isn't an ARGB colour. It is ignored", path,
						  where, s_barNames[bar]);
			continue;
		}
		set->bar[bar] = (uint32_t)argb;
		set->barMask |= (uint8_t)(1u << bar);
	}
}

/* The markings of a bolt of weapon index `weapon`, as in XWAU: the value
   for any markings, then the markings value's default and own value. */
static uint8_t CraftWeapons_Variant(const CraftWeaponsType* type, const WeaponColorSet* byMarkings,
									int weapon) {
	int variant = type->colors.variant[weapon];

	if (variant < 0) {
		variant = type->colors.defaultVariant >= 0 ? type->colors.defaultVariant : 0;
	}
	if (byMarkings != NULL && byMarkings->defaultVariant >= 0) {
		variant = byMarkings->defaultVariant;
	}
	if (byMarkings != NULL && byMarkings->variant[weapon] >= 0) {
		variant = byMarkings->variant[weapon];
	}
	return (uint8_t)variant;
}

static const WeaponColorSet* CraftWeapons_ByMarkings(const CraftWeaponsType* type, int markings) {
	uint16_t index;

	for (index = 0; index < type->markingsCount; ++index) {
		if (type->markings[index] == markings) {
			return &type->byMarkings[index];
		}
	}
	return NULL;
}

/* Registers the bolt markings that a craft of `type` can give, so the HD
   renderer has their variants before the first shot. */
static void CraftWeapons_UseBoltMarkings(const CraftWeaponsType* type) {
	int weapon;
	uint16_t index;

	for (weapon = 0; weapon < CRAFT_WEAPONS_COUNT; ++weapon) {
		XwaSkins_UseMarkings((uint16_t)(CRAFT_WEAPONS_FIRST + weapon),
							 CraftWeapons_Variant(type, NULL, weapon));
		for (index = 0; index < type->markingsCount; ++index) {
			XwaSkins_UseMarkings((uint16_t)(CRAFT_WEAPONS_FIRST + weapon),
								 CraftWeapons_Variant(type, &type->byMarkings[index], weapon));
		}
	}
}

void XwaCraftWeapons_Init(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* section;

	memset(s_globalImpact, 0xff, sizeof s_globalImpact);
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, CRAFT_WEAPONS_FLIGHT_YAML, &file,
									&error)) {
		/* mission_backdrops.c logs a file that can't be read. */
		return;
	}
	root = AeronConfigFile_Root(file);
	section = AeronConfigNode_MapGet(root, "weapons");
	if (AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) == AERON_CONFIG_INT &&
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) == 1 && section != NULL) {
		CraftWeapons_ReadImpact(CRAFT_WEAPONS_FLIGHT_YAML, "weapons",
								AeronConfigNode_MapGet(section, "impact"), s_globalImpact);
	}
	AeronConfigFile_Destroy(file);
}

void XwaCraftWeapons_ClearModelSettings(void) {
	unsigned int objectType;

	for (objectType = 0; objectType < OBJ_Count; ++objectType) {
		free(s_types[objectType]);
		s_types[objectType] = NULL;
	}
}

void XwaCraftWeapons_ReadModelSettings(unsigned int objectType, const char* path,
									   const AeronConfigNode* group) {
	const AeronConfigNode* byMarkings;
	const AeronConfigNode* node;
	CraftWeaponsType* type;
	size_t count;
	size_t index;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: weapons isn't a map. It is ignored", path);
		return;
	}
	byMarkings = AeronConfigNode_MapGet(group, "by_markings");
	count = AeronConfigNode_MapCount(byMarkings);
	if (count > 256) {
		count = 256;
	}
	type = (CraftWeaponsType*)calloc(1, sizeof *type + count * sizeof type->byMarkings[0]);
	if (type == NULL) {
		return;
	}
	type->switchByIff = -1;
	node = AeronConfigNode_MapGet(group, "switch_by_iff");
	if (node != NULL && AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogWarn("xwa.models", "%s: weapons.switch_by_iff must be true or false. It is ignored", path);
	} else if (node != NULL) {
		type->switchByIff = (int8_t)(AeronConfigNode_Bool(node, 0) != 0);
	}
	CraftWeapons_ClearSet(&type->colors);
	CraftWeapons_ReadSet(path, "weapons.colors", AeronConfigNode_MapGet(group, "colors"), &type->colors);
	for (index = 0; index < count; ++index) {
		const char* key = AeronConfigNode_MapKeyAt(byMarkings, index);
		char* end;
		const long markings = strtol(key, &end, 10);
		char where[64];

		if (*key == '\0' || *end != '\0' || markings < 0 || markings > 255 ||
			CraftWeapons_ByMarkings(type, (int)markings) != NULL) {
			Aeron_LogWarn("xwa.models",
						  "%s: weapons.by_markings.%s needs markings from 0 to 255, once. It is ignored",
						  path, key);
			continue;
		}
		snprintf(where, sizeof where, "weapons.by_markings.%ld", markings);
		CraftWeapons_ClearSet(&type->byMarkings[type->markingsCount]);
		CraftWeapons_ReadSet(path, where, AeronConfigNode_MapValueAt(byMarkings, index),
							 &type->byMarkings[type->markingsCount]);
		type->markings[type->markingsCount++] = (uint8_t)markings;
	}
	s_types[objectType] = type;
	CraftWeapons_UseBoltMarkings(type);
}

static const CraftWeaponsType* CraftWeapons_Type(unsigned int objectType) {
	if (objectType >= OBJ_Count) {
		return NULL;
	}
	XwaModelSettings_Ensure(objectType);
	return s_types[objectType];
}

uint8_t XwaCraftWeapons_BoltMarkings(unsigned int objectType, uint8_t markings, unsigned int weaponType) {
	const CraftWeaponsType* type = CraftWeapons_Type(objectType);

	if (type == NULL || weaponType < CRAFT_WEAPONS_FIRST ||
		weaponType >= CRAFT_WEAPONS_FIRST + CRAFT_WEAPONS_COUNT) {
		return 0;
	}
	return CraftWeapons_Variant(type, CraftWeapons_ByMarkings(type, markings),
								(int)(weaponType - CRAFT_WEAPONS_FIRST));
}

int XwaCraftWeapons_SwitchByIff(unsigned int objectType) {
	const CraftWeaponsType* type = CraftWeapons_Type(objectType);

	if (type != NULL && type->switchByIff >= 0) {
		return type->switchByIff;
	}
	return XwaMissionSettings_WeaponIffSwap();
}

unsigned int XwaCraftWeapons_IffWeaponType(uint8_t iff, unsigned int weaponType) {
	unsigned int index = weaponType - OBJ_LaserRebel;
	uint8_t side;

	if (weaponType < OBJ_LaserRebel || weaponType > OBJ_LaserImperialDS ||
		iff >= sizeof g_projectileRequiredIffByShooterIff) {
		return weaponType;
	}
	side = g_projectileRequiredIffByType[index];
	if (side != 0xffu && g_projectileRequiredIffByShooterIff[iff] != side) {
		return g_projectileAlternateIffTypeByType[index];
	}
	return weaponType;
}

int XwaCraftWeapons_ImpactColor(unsigned int objectType, int markings, unsigned int weaponType,
								uint32_t* rgb) {
	const CraftWeaponsType* type;
	const WeaponColorSet* byMarkings;
	int weapon;
	int32_t color;

	if (weaponType < CRAFT_WEAPONS_FIRST || weaponType >= CRAFT_WEAPONS_FIRST + CRAFT_WEAPONS_COUNT) {
		return 0;
	}
	weapon = (int)(weaponType - CRAFT_WEAPONS_FIRST);
	type = CraftWeapons_Type(objectType);
	byMarkings = type != NULL && markings >= 0 ? CraftWeapons_ByMarkings(type, markings) : NULL;
	color = s_globalImpact[weapon];
	if (byMarkings != NULL && byMarkings->impact[weapon] >= 0) {
		color = byMarkings->impact[weapon];
	} else if (type != NULL && type->colors.impact[weapon] >= 0) {
		color = type->colors.impact[weapon];
	}
	if (color <= 0) {
		return 0;
	}
	*rgb = (uint32_t)color;
	return 1;
}

void XwaCraftWeapons_TintImpactMark(const GlowMarkRequest* request, const MobileObject* bolt,
									unsigned int weaponType) {
	int markings = -1;
	uint32_t rgb;

	if (request == NULL) {
		return;
	}
	if (bolt->sourceObjIdx != -1 && g_objectTable[bolt->sourceObjIdx].mobj != NULL) {
		markings = g_objectTable[bolt->sourceObjIdx].mobj->nodeSwitchIndex;
	}
	if (XwaCraftWeapons_ImpactColor(bolt->sourceObjectType, markings, weaponType, &rgb)) {
		s_requestTints[request - g_glowMarkRequestPool] = rgb;
	}
}

void XwaCraftWeapons_ClearImpactTint(const GlowMarkRequest* request) {
	s_requestTints[request - g_glowMarkRequestPool] = 0;
}

uint32_t XwaCraftWeapons_ImpactTint(const GlowMarkRequest* request) {
	return s_requestTints[request - g_glowMarkRequestPool];
}

void XwaCraftWeapons_EnergyBarColors(unsigned int objectType, uint8_t markings,
									 uint32_t colors[XWA_CRAFT_WEAPONS_BARS]) {
	const CraftWeaponsType* type = CraftWeapons_Type(objectType);
	const WeaponColorSet* byMarkings;
	int bar;

	if (type == NULL) {
		return;
	}
	byMarkings = CraftWeapons_ByMarkings(type, markings);
	for (bar = 0; bar < XWA_CRAFT_WEAPONS_BARS; ++bar) {
		if (byMarkings != NULL && (byMarkings->barMask & (1u << bar))) {
			colors[bar] = byMarkings->bar[bar];
		} else if (type->colors.barMask & (1u << bar)) {
			colors[bar] = type->colors.bar[bar];
		}
	}
}
