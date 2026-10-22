#include "xwa_runtime/models/weapon_rates.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/object/object.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/models/mission_settings.h"
#include "xwa_runtime/models/model_settings.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define WEAPON_RATES_FLIGHT_YAML "flight/flight.yaml"

enum {
	WEAPON_RATES_DECHARGE,
	WEAPON_RATES_RECHARGE,
	WEAPON_RATES_COOLDOWN,
	WEAPON_RATES_DECHARGE_PERCENT,
	WEAPON_RATES_RECHARGE_PERCENT,
	WEAPON_RATES_KEYS
};

static const char* const s_keys[WEAPON_RATES_KEYS] = { "decharge", "recharge", "cooldown", "decharge_percent",
													   "recharge_percent" };

/* The `rates` of a model file or of flight/flight.yaml. -1 means not set. */
typedef struct WeaponRatesSet {
	uint8_t set;
	int32_t value[WEAPON_RATES_KEYS];
} WeaponRatesSet;

static int s_active;
static WeaponRatesSet s_defaults;
static int32_t s_dechargePercent;
static int32_t s_rechargePercent;
static WeaponRatesSet s_models[OBJ_Count];

/* Reads a number of 0 or more into `out`. Logs and returns 0 otherwise. */
static int WeaponRates_ReadInt(const char* path, const char* where, const AeronConfigNode* node,
							   int32_t* out) {
	const int64_t value = AeronConfigNode_Int(node, -1);

	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 0 || value > INT32_MAX) {
		Aeron_LogWarn("xwa.models", "%s: %s must be a number of 0 or more. It is ignored", path, where);
		return 0;
	}
	*out = (int32_t)value;
	return 1;
}

static void WeaponRates_ReadSet(const char* path, const char* where, const AeronConfigNode* rates,
								WeaponRatesSet* set) {
	int key;

	memset(set->value, 0xff, sizeof set->value);
	if (AeronConfigNode_Type(rates) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: %s isn't a map. It is ignored", path, where);
		return;
	}
	set->set = 1;
	for (key = 0; key < WEAPON_RATES_KEYS; ++key) {
		const AeronConfigNode* node = AeronConfigNode_MapGet(rates, s_keys[key]);
		char name[64];

		if (node != NULL) {
			snprintf(name, sizeof name, "%s.%s", where, s_keys[key]);
			WeaponRates_ReadInt(path, name, node, &set->value[key]);
		}
	}
}

void XwaWeaponRates_Init(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	const AeronConfigNode* node;

	s_active = 0;
	memset(&s_defaults, 0, sizeof s_defaults);
	memset(s_defaults.value, 0xff, sizeof s_defaults.value);
	s_dechargePercent = 100;
	s_rechargePercent = 100;
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, WEAPON_RATES_FLIGHT_YAML, &file,
									&error)) {
		/* mission_backdrops.c logs a file that can't be read. */
		return;
	}
	root = AeronConfigFile_Root(file);
	section = AeronConfigNode_MapGet(root, "weapons");
	if (AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1 || section == NULL) {
		AeronConfigFile_Destroy(file);
		return;
	}
	if ((node = AeronConfigNode_MapGet(section, "rates")) != NULL) {
		WeaponRates_ReadSet(WEAPON_RATES_FLIGHT_YAML, "weapons.rates", node, &s_defaults);
		s_active |= s_defaults.set;
	}
	if ((node = AeronConfigNode_MapGet(section, "decharge_percent")) != NULL) {
		s_active |= WeaponRates_ReadInt(WEAPON_RATES_FLIGHT_YAML, "weapons.decharge_percent", node,
										&s_dechargePercent);
	}
	if ((node = AeronConfigNode_MapGet(section, "recharge_percent")) != NULL) {
		s_active |= WeaponRates_ReadInt(WEAPON_RATES_FLIGHT_YAML, "weapons.recharge_percent", node,
										&s_rechargePercent);
	}
	AeronConfigFile_Destroy(file);
}

void XwaWeaponRates_ClearModelSettings(void) { memset(s_models, 0, sizeof s_models); }

void XwaWeaponRates_ReadModelSettings(unsigned int objectType, const char* path,
									  const AeronConfigNode* group) {
	const AeronConfigNode* rates;

	/* craft_weapons.c logs a `weapons` group that isn't a map. */
	if (objectType >= OBJ_Count || AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		return;
	}
	rates = AeronConfigNode_MapGet(group, "rates");
	if (rates != NULL) {
		WeaponRates_ReadSet(path, "weapons.rates", rates, &s_models[objectType]);
	}
}

/* The model rates of object type `objectType`, or NULL. */
static const WeaponRatesSet* WeaponRates_Model(unsigned int objectType) {
	XwaModelSettings_Ensure(objectType);
	return objectType < OBJ_Count && s_models[objectType].set ? &s_models[objectType] : NULL;
}

/* The model's value of `key`, else the default one, else -1. */
static int32_t WeaponRates_Value(const WeaponRatesSet* model, int key) {
	return model != NULL && model->value[key] >= 0 ? model->value[key] : s_defaults.value[key];
}

static int WeaponRates_IsTieFighterOrBomber(const ObjectRecord* object) {
	return object->objectType == OBJ_TIEFighter || object->objectType == OBJ_TIEBomber;
}

/* `rate` scaled by the key's percent, then by the global percent. */
static int WeaponRates_Scale(const WeaponRatesSet* model, int32_t rate, int percentKey, int32_t global) {
	int32_t percent = WeaponRates_Value(model, percentKey);

	if (percent < 0) {
		percent = 100;
	}
	return (int)((int64_t)rate * percent / 100 * global / 100);
}

static int WeaponRates_DechargeRate(const ObjectRecord* object, const WeaponRatesSet* model) {
	int32_t rate = WeaponRates_Value(model, WEAPON_RATES_DECHARGE);

	if (rate < 0) {
		if (object->genusId == GENUS_Fighter) {
			rate = WeaponRates_IsTieFighterOrBomber(object) ? 3 : 4;
		} else {
			rate = 3;
		}
	}
	return WeaponRates_Scale(model, rate, WEAPON_RATES_DECHARGE_PERCENT, s_dechargePercent);
}

int XwaWeaponRates_Decharge(unsigned int objIdx, int* rate) {
	const ObjectRecord* object = &g_objectTable[objIdx];
	const WeaponRatesSet* model = WeaponRates_Model(object->objectType);

	if (!s_active && model == NULL) {
		return 0;
	}
	*rate = WeaponRates_DechargeRate(object, model);
	return 1;
}

void XwaWeaponRates_Recharge(unsigned int objIdx, int16_t chargeBasis, int16_t* chargeDelta) {
	const ObjectRecord* object = &g_objectTable[objIdx];
	const WeaponRatesSet* model = WeaponRates_Model(object->objectType);
	int32_t rate;

	if (!s_active && model == NULL) {
		return;
	}
	rate = WeaponRates_Value(model, WEAPON_RATES_RECHARGE);
	if (rate < 0) {
		rate = WeaponRates_IsTieFighterOrBomber(object) ? 3 : 2;
	}
	rate = WeaponRates_Scale(model, rate, WEAPON_RATES_RECHARGE_PERCENT, s_rechargePercent);
	*chargeDelta = (int16_t)((int64_t)rate * chargeBasis);
}

int XwaWeaponRates_Cooldown(unsigned int objIdx) {
	const int32_t cooldown =
		WeaponRates_Value(WeaponRates_Model(g_objectTable[objIdx].objectType), WEAPON_RATES_COOLDOWN);

	return cooldown >= 0 ? cooldown : 47;
}

int XwaWeaponRates_CanFire(unsigned int objIdx, unsigned int rackIdx, int laser) {
	const ObjectRecord* object = &g_objectTable[objIdx];
	const WarheadInventoryEntry* rack = &g_curCraft->warheadData[rackIdx];

	if (laser && XwaMissionSettings_FireNeedsDechargeRate() &&
		(rack->projectileTypeId == OBJ_LaserRebel || rack->projectileTypeId == OBJ_LaserImperial ||
		 rack->projectileTypeId == OBJ_LaserIon) &&
		(int8_t)rack->laserCharge < WeaponRates_DechargeRate(object, WeaponRates_Model(object->objectType))) {
		return 0;
	}
	if (XwaMissionSettings_NoStarshipFireFromDestroyedRacks() &&
		(object->genusId == GENUS_Freighter || object->genusId == GENUS_Starship ||
		 object->genusId == GENUS_Platform || object->genusId == GENUS_SatelliteBuoy) &&
		rack->lastFireMeshIdx < XWA_CRAFT_MESH_SLOT_COUNT &&
		g_curCraft->componentHp[rack->lastFireMeshIdx] == 0) {
		return 0;
	}
	return 1;
}
