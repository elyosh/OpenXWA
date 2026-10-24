#include "xwa_runtime/models/weapon_rates.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/object/object.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/models/mission_settings.h"
#include "xwa_runtime/models/model_settings.h"
#include "xwa_runtime/models/weapon_stats.h"

#include <limits.h>

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
	WEAPON_RATES_ENERGY_TRANSFER,
	WEAPON_RATES_ENERGY_TRANSFER_PENALTY,
	WEAPON_RATES_ENERGY_TRANSFER_LASER_LIMIT,
	WEAPON_RATES_ENERGY_TRANSFER_SHIELD_LIMIT,
	WEAPON_RATES_TORPEDOES_PER_PASS,
	WEAPON_RATES_TORPEDOES_PER_TARGET,
	WEAPON_RATES_IMPACT_SPIN,
	WEAPON_RATES_IMPACT_SPIN_SPEED_PERCENT,
	WEAPON_RATES_IMPACT_SPIN_ANGLE_PERCENT,
	WEAPON_RATES_MAX_SYSTEM_DAMAGE,
	WEAPON_RATES_SYSTEM_DAMAGE_284,
	WEAPON_RATES_SYSTEM_DAMAGE_285,
	WEAPON_RATES_SYSTEM_DAMAGE_290,
	WEAPON_RATES_SYSTEM_DAMAGE_296,
	WEAPON_RATES_KEYS
};

/* The groups of the `weapons` section that this module reads. */
enum {
	WEAPON_RATES_GROUP_RATES,
	WEAPON_RATES_GROUP_ENERGY_TRANSFER,
	WEAPON_RATES_GROUP_AI_TORPEDOES,
	WEAPON_RATES_GROUP_IMPACT_SPIN,
	WEAPON_RATES_GROUP_SYSTEM_DAMAGE,
	WEAPON_RATES_GROUPS
};

static const char* const s_groups[WEAPON_RATES_GROUPS] = { "rates", "energy_transfer", "ai_torpedoes",
														   "impact_spin", "system_damage" };

typedef struct WeaponRatesKey {
	uint8_t group;
	const char* name;
} WeaponRatesKey;

static const WeaponRatesKey s_keys[WEAPON_RATES_KEYS] = {
	{ WEAPON_RATES_GROUP_RATES, "decharge" },
	{ WEAPON_RATES_GROUP_RATES, "recharge" },
	{ WEAPON_RATES_GROUP_RATES, "cooldown" },
	{ WEAPON_RATES_GROUP_RATES, "decharge_percent" },
	{ WEAPON_RATES_GROUP_RATES, "recharge_percent" },
	{ WEAPON_RATES_GROUP_ENERGY_TRANSFER, "rate" },
	{ WEAPON_RATES_GROUP_ENERGY_TRANSFER, "penalty" },
	{ WEAPON_RATES_GROUP_ENERGY_TRANSFER, "laser_limit" },
	{ WEAPON_RATES_GROUP_ENERGY_TRANSFER, "shield_limit" },
	{ WEAPON_RATES_GROUP_AI_TORPEDOES, "per_pass" },
	{ WEAPON_RATES_GROUP_AI_TORPEDOES, "per_target" },
	{ WEAPON_RATES_GROUP_IMPACT_SPIN, "enabled" },
	{ WEAPON_RATES_GROUP_IMPACT_SPIN, "speed_percent" },
	{ WEAPON_RATES_GROUP_IMPACT_SPIN, "angle_percent" },
	{ WEAPON_RATES_GROUP_SYSTEM_DAMAGE, "max" },
	{ WEAPON_RATES_GROUP_SYSTEM_DAMAGE, "284" },
	{ WEAPON_RATES_GROUP_SYSTEM_DAMAGE, "285" },
	{ WEAPON_RATES_GROUP_SYSTEM_DAMAGE, "290" },
	{ WEAPON_RATES_GROUP_SYSTEM_DAMAGE, "296" },
};

/* The values of a model file or of flight/flight.yaml. -1 means not set.
   `group` has a bit for each group that the file has. */
typedef struct WeaponRatesSet {
	uint8_t group;
	int32_t value[WEAPON_RATES_KEYS];
} WeaponRatesSet;

static int s_active;
static WeaponRatesSet s_defaults;
static int32_t s_dechargePercent;
static int32_t s_rechargePercent;
static WeaponRatesSet s_models[OBJ_Count];

/* Reads a number of 0 or more into `out`. If the node isn't such a number,
   logs it and returns 0. */
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

/* Reads the groups of the `weapons` section `weapons` into `set`. */
static void WeaponRates_ReadSet(const char* path, const AeronConfigNode* weapons, WeaponRatesSet* set) {
	int group;
	int key;

	memset(set, 0, sizeof *set);
	memset(set->value, 0xff, sizeof set->value);
	for (group = 0; group < WEAPON_RATES_GROUPS; ++group) {
		const AeronConfigNode* node = AeronConfigNode_MapGet(weapons, s_groups[group]);

		if (node == NULL) {
			continue;
		}
		if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s: weapons.%s isn't a map. It is ignored", path, s_groups[group]);
			continue;
		}
		set->group |= (uint8_t)(1u << group);
	}
	for (key = 0; key < WEAPON_RATES_KEYS; ++key) {
		const AeronConfigNode* node;
		char name[64];

		if (!(set->group & (1u << s_keys[key].group))) {
			continue;
		}
		node = AeronConfigNode_MapGet(AeronConfigNode_MapGet(weapons, s_groups[s_keys[key].group]),
									  s_keys[key].name);
		if (node == NULL) {
			continue;
		}
		snprintf(name, sizeof name, "weapons.%s.%s", s_groups[s_keys[key].group], s_keys[key].name);
		if (key == WEAPON_RATES_IMPACT_SPIN) {
			if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
				Aeron_LogWarn("xwa.models", "%s: %s must be true or false. It is ignored", path, name);
			} else {
				set->value[key] = AeronConfigNode_Bool(node, 0) != 0;
			}
		} else {
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
		/* The backdrops code already logs this error. */
		return;
	}
	root = AeronConfigFile_Root(file);
	section = AeronConfigNode_MapGet(root, "weapons");
	if (AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1 || section == NULL) {
		AeronConfigFile_Destroy(file);
		return;
	}
	WeaponRates_ReadSet(WEAPON_RATES_FLIGHT_YAML, section, &s_defaults);
	s_active = (s_defaults.group & (1u << WEAPON_RATES_GROUP_RATES)) != 0;
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
	/* The weapon colours code logs a `weapons` group that isn't a map. */
	if (objectType >= OBJ_Count || AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		return;
	}
	WeaponRates_ReadSet(path, group, &s_models[objectType]);
}

/* Returns the model values of object type `objectType`, or NULL if it has
   none. */
static const WeaponRatesSet* WeaponRates_Model(unsigned int objectType) {
	XwaModelSettings_Ensure(objectType);
	return objectType < OBJ_Count && s_models[objectType].group ? &s_models[objectType] : NULL;
}

/* Whether the model has a `rates` map, which turns the rates on for its
   craft. */
static int WeaponRates_HasRates(const WeaponRatesSet* model) {
	return model != NULL && (model->group & (1u << WEAPON_RATES_GROUP_RATES)) != 0;
}

/* Returns the model's value of `key`. If the model has none, returns the
   default value, or -1 if that isn't set either. */
static int32_t WeaponRates_Value(const WeaponRatesSet* model, int key) {
	return model != NULL && model->value[key] >= 0 ? model->value[key] : s_defaults.value[key];
}

static int WeaponRates_IsTieFighterOrBomber(const ObjectRecord* object) {
	return object->objectType == OBJ_TIEFighter || object->objectType == OBJ_TIEBomber;
}

/* Returns `rate` scaled by the percent key, then by the global percent. */
static int WeaponRates_Scale(const WeaponRatesSet* model, int32_t rate, int percentKey, int32_t global) {
	int32_t percent = WeaponRates_Value(model, percentKey);

	if (percent < 0) {
		percent = 100;
	}
	return (int)((int64_t)rate * percent / 100 * global / 100);
}

/* Returns the weapon type that rack `rack` of object `objIdx` fires: its
   type, or the next type when its charge reaches the high-power charge. */
static unsigned int WeaponRates_RackWeapon(unsigned int objIdx, const WarheadInventoryEntry* rack) {
	const unsigned int weapon = rack->projectileTypeId;

	return (int8_t)rack->laserCharge >=
				   XwaWeaponStats_Value((int)objIdx, weapon, XWA_WEAPON_STAT_HIGH_POWER_CHARGE, 64)
			   ? weapon + 1
			   : weapon;
}

/* Returns 1 when the weapon profile of object `objIdx` sets `stat` for
   `weapon`. */
static int WeaponRates_HasStat(unsigned int objIdx, unsigned int weapon, XwaWeaponStat stat) {
	return XwaWeaponStats_Value((int)objIdx, weapon, stat, INT_MIN) != INT_MIN;
}

/* Returns the charge that a shot of `weapon` by object `objIdx` uses: the
   weapon profile's value, or the model's, or the stock value, scaled by the
   percents. */
static int WeaponRates_DechargeRate(unsigned int objIdx, const WeaponRatesSet* model, unsigned int weapon) {
	const ObjectRecord* object = &g_objectTable[objIdx];
	int32_t rate = WeaponRates_Value(model, WEAPON_RATES_DECHARGE);

	if (rate < 0) {
		if (object->genusId == GENUS_Fighter) {
			rate = WeaponRates_IsTieFighterOrBomber(object) ? 3 : 4;
		} else {
			rate = 3;
		}
	}
	rate = XwaWeaponStats_Value((int)objIdx, weapon, XWA_WEAPON_STAT_DECHARGE_RATE, rate);
	return WeaponRates_Scale(model, rate, WEAPON_RATES_DECHARGE_PERCENT, s_dechargePercent);
}

int XwaWeaponRates_Decharge(unsigned int objIdx, unsigned int rackIdx, int* rate) {
	const WeaponRatesSet* model = WeaponRates_Model(g_objectTable[objIdx].objectType);
	const unsigned int weapon = WeaponRates_RackWeapon(objIdx, &g_curCraft->warheadData[rackIdx]);

	if (!s_active && !WeaponRates_HasRates(model) &&
		!WeaponRates_HasStat(objIdx, weapon, XWA_WEAPON_STAT_DECHARGE_RATE)) {
		return 0;
	}
	*rate = WeaponRates_DechargeRate(objIdx, model, weapon);
	return 1;
}

void XwaWeaponRates_Recharge(unsigned int objIdx, unsigned int rackIdx, int16_t chargeBasis,
							 int16_t* chargeDelta) {
	const ObjectRecord* object = &g_objectTable[objIdx];
	const WeaponRatesSet* model = WeaponRates_Model(object->objectType);
	const unsigned int weapon = WeaponRates_RackWeapon(objIdx, &g_curCraft->warheadData[rackIdx]);
	int32_t rate;

	if (!s_active && !WeaponRates_HasRates(model) &&
		!WeaponRates_HasStat(objIdx, weapon, XWA_WEAPON_STAT_RECHARGE_RATE)) {
		return;
	}
	rate = WeaponRates_Value(model, WEAPON_RATES_RECHARGE);
	if (rate < 0) {
		rate = WeaponRates_IsTieFighterOrBomber(object) ? 3 : 2;
	}
	rate = XwaWeaponStats_Value((int)objIdx, weapon, XWA_WEAPON_STAT_RECHARGE_RATE, rate);
	rate = WeaponRates_Scale(model, rate, WEAPON_RATES_RECHARGE_PERCENT, s_rechargePercent);
	*chargeDelta = (int16_t)((int64_t)rate * chargeBasis);
}

int XwaWeaponRates_Cooldown(unsigned int objIdx, unsigned int weapon) {
	const int32_t cooldown =
		WeaponRates_Value(WeaponRates_Model(g_objectTable[objIdx].objectType), WEAPON_RATES_COOLDOWN);

	return XwaWeaponStats_Value((int)objIdx, weapon, XWA_WEAPON_STAT_FIRE_RATE,
								cooldown >= 0 ? cooldown : 47);
}

int XwaWeaponRates_CanFire(unsigned int objIdx, unsigned int rackIdx, int laser) {
	const ObjectRecord* object = &g_objectTable[objIdx];
	const WarheadInventoryEntry* rack = &g_curCraft->warheadData[rackIdx];

	if (laser && XwaMissionSettings_FireNeedsDechargeRate() &&
		(rack->projectileTypeId == OBJ_LaserRebel || rack->projectileTypeId == OBJ_LaserImperial ||
		 rack->projectileTypeId == OBJ_LaserIon) &&
		(int8_t)rack->laserCharge < WeaponRates_DechargeRate(objIdx, WeaponRates_Model(object->objectType),
															 WeaponRates_RackWeapon(objIdx, rack))) {
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

/* Returns the value of `key` for object `objIdx`, or `stock` when neither
   its model file nor flight/flight.yaml sets it. */
static int WeaponRates_ObjectValue(unsigned int objIdx, int key, int stock) {
	const int32_t value = WeaponRates_Value(WeaponRates_Model(g_objectTable[objIdx].objectType), key);

	return value >= 0 ? value : stock;
}

int XwaWeaponRates_EnergyTransfer(unsigned int objIdx, int stock) {
	return WeaponRates_ObjectValue(objIdx, WEAPON_RATES_ENERGY_TRANSFER, stock);
}

int XwaWeaponRates_EnergyTransferToShields(unsigned int objIdx, int rate) {
	const int step = rate - WeaponRates_ObjectValue(objIdx, WEAPON_RATES_ENERGY_TRANSFER_PENALTY, 0);

	/* The game divides by the step. */
	return step > 0 ? step : 1;
}

int XwaWeaponRates_EnergyTransferLaserLimit(unsigned int objIdx) {
	return WeaponRates_ObjectValue(objIdx, WEAPON_RATES_ENERGY_TRANSFER_LASER_LIMIT, 100);
}

int XwaWeaponRates_EnergyTransferShieldLimit(unsigned int objIdx) {
	return WeaponRates_ObjectValue(objIdx, WEAPON_RATES_ENERGY_TRANSFER_SHIELD_LIMIT, 800);
}

int XwaWeaponRates_AiTorpedoesPerPass(unsigned int objIdx, int stock) {
	return WeaponRates_ObjectValue(objIdx, WEAPON_RATES_TORPEDOES_PER_PASS, stock);
}

int XwaWeaponRates_AiTorpedoesPerTarget(unsigned int objIdx, int stock) {
	return WeaponRates_ObjectValue(objIdx, WEAPON_RATES_TORPEDOES_PER_TARGET, stock);
}

int XwaWeaponRates_ImpactSpin(unsigned int objIdx, int stock) {
	return WeaponRates_ObjectValue(objIdx, WEAPON_RATES_IMPACT_SPIN, stock);
}

void XwaWeaponRates_ScaleImpactSpin(unsigned int objIdx, unsigned int impactObjIdx, int* speed, int* angle) {
	int32_t percent;

	if (laser_GetProjectileWarheadClass(g_objectTable[impactObjIdx].objectType) <= 0) {
		return;
	}
	percent = WeaponRates_ObjectValue(objIdx, WEAPON_RATES_IMPACT_SPIN_SPEED_PERCENT, 100);
	*speed = (int16_t)(((int64_t)(int16_t)*speed * percent + 50) / 100);
	percent = WeaponRates_ObjectValue(objIdx, WEAPON_RATES_IMPACT_SPIN_ANGLE_PERCENT, 100);
	*angle = (int16_t)(((int64_t)(int16_t)*angle * percent + 50) / 100);
}

int XwaWeaponRates_AddSystemDamage(unsigned int objIdx, unsigned int projectileObjIdx, int weapon,
								   uint16_t* damage) {
	const WeaponRatesSet* target = WeaponRates_Model(g_objectTable[objIdx].objectType);
	const MobileObject* projectile = g_objectTable[projectileObjIdx].mobj;
	const WeaponRatesSet* source = NULL;
	const unsigned int systemDamage = 1u << WEAPON_RATES_GROUP_SYSTEM_DAMAGE;
	int32_t max;
	int32_t added;
	int key;

	if (projectile != NULL) {
		source = WeaponRates_Model(projectile->sourceObjectType);
	}
	if (!s_active && !(s_defaults.group & systemDamage) &&
		(target == NULL || !(target->group & systemDamage)) &&
		(source == NULL || !(source->group & systemDamage))) {
		return 0;
	}
	max = WeaponRates_Value(target, WEAPON_RATES_MAX_SYSTEM_DAMAGE);
	if (max < 0) {
		max = 1000;
	}
	if (*damage >= max) {
		return 1;
	}
	switch (weapon) {
		case OBJ_LaserIon:
			key = WEAPON_RATES_SYSTEM_DAMAGE_284;
			added = 1;
			break;
		case OBJ_LaserIonTurbo:
			key = WEAPON_RATES_SYSTEM_DAMAGE_285;
			added = 2;
			break;
		case OBJ_WarheadIon:
			key = WEAPON_RATES_SYSTEM_DAMAGE_290;
			added = 4;
			break;
		case OBJ_WarheadIonPulse:
			key = WEAPON_RATES_SYSTEM_DAMAGE_296;
			added = 30;
			break;
		default:
			return 1;
	}
	if (WeaponRates_Value(source, key) >= 0) {
		added = WeaponRates_Value(source, key);
	}
	*damage = (uint16_t)((int64_t)*damage + added < max ? *damage + added : max);
	return 1;
}
