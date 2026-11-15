#include "xwa_runtime/models/projectiles.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/flight/object/laser.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/models/names.h"
#include "xwa_runtime/models/units.h"
#include "xwa_runtime/models/weapon_stats.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

enum {
	HOMING_ROWS = 6,
	HOMING_ROW_SIZE = 7,
};

static const char* const s_sideNames[3] = { "rebel", "imperial", "none" };
static const uint8_t s_sideValues[3] = { XWA_WEAPON_SIDE_REBEL, XWA_WEAPON_SIDE_IMPERIAL,
										 XWA_WEAPON_SIDE_NONE };

/* Reads `key` of `entry` in `unit`, from `min` to `max`. Returns 0 when the
   key isn't there, or with a warning when its value is wrong. */
static int Projectiles_ReadValue(const char* where, const AeronConfigNode* entry, const char* key,
								 XwaUnit unit, int64_t min, int64_t max, int32_t* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(entry, key);

	if (node == NULL) {
		return 0;
	}
	if (!XwaUnits_Read(node, unit, min, max, out)) {
		Aeron_LogWarn("xwa.mission", "%s:%d: %s.%s must be a number from %lld to %lld%s%s; it is ignored",
					  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), where, key,
					  (long long)(unit == XWA_UNIT_NONE ? min : XwaUnits_FromGame(unit, (double)min)),
					  (long long)(unit == XWA_UNIT_NONE ? max : XwaUnits_FromGame(unit, (double)max)),
					  unit == XWA_UNIT_NONE ? "" : " ", unit == XWA_UNIT_NONE ? "" : XwaUnits_Name(unit));
		return 0;
	}
	return 1;
}

static void Projectiles_ReadWeapon(const char* where, int weapon, const AeronConfigNode* entry) {
	static const char* const keys[] = { "power",           "speed_mglt",    "life_seconds", "life_fraction",
										"launch_offset_m", "warhead_class", "score",        "side",
										"side_model",      "homing_profile" };
	const unsigned int at = (unsigned int)(weapon - OBJ_LaserRebel);
	const AeronConfigNode* node;
	int32_t value;
	size_t i;

	if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.mission", "%s:%d: %s must be a map; it is ignored", XWA_FLIGHT_CONFIG_PATH,
					  AeronConfigNode_Line(entry), where);
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(entry); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(entry, i);
		size_t k;

		for (k = 0; k < sizeof keys / sizeof keys[0] && strcmp(key, keys[k]) != 0; ++k) {
		}
		if (k == sizeof keys / sizeof keys[0]) {
			Aeron_LogWarn("xwa.mission", "%s:%d: %s.%s isn't a setting; it is ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(AeronConfigNode_MapValueAt(entry, i)),
						  where, key);
		}
	}
	if (Projectiles_ReadValue(where, entry, "power", XWA_UNIT_NONE, INT32_MIN, INT32_MAX, &value)) {
		g_projectileDamageByType[at] = value;
	}
	if (Projectiles_ReadValue(where, entry, "speed_mglt", XWA_UNIT_MGLT, 0, UINT16_MAX, &value)) {
		g_projectileSpeedByType[at] = (uint16_t)value;
	}
	if (Projectiles_ReadValue(where, entry, "life_seconds", XWA_UNIT_NONE, 0, UINT16_MAX, &value)) {
		g_projectileLifetimeSecondsByType[at] = (uint16_t)value;
	}
	if (Projectiles_ReadValue(where, entry, "life_fraction", XWA_UNIT_NONE, 0, UINT16_MAX, &value)) {
		g_projectileLifetimeFracQ16ByType[at] = (uint16_t)value;
	}
	if (Projectiles_ReadValue(where, entry, "launch_offset_m", XWA_UNIT_METRES, INT16_MIN, INT16_MAX,
							  &value)) {
		g_projectileLaunchOffsetByType[at] = (int16_t)value;
	}
	if (Projectiles_ReadValue(where, entry, "warhead_class", XWA_UNIT_NONE, 0, UINT8_MAX, &value)) {
		g_projectileWarheadClassByType[at] = (uint8_t)value;
	}
	if (Projectiles_ReadValue(where, entry, "score", XWA_UNIT_NONE, 0, UINT16_MAX, &value)) {
		g_projectileScoreByType[at] = (uint16_t)value;
	}
	if ((node = AeronConfigNode_MapGet(entry, "side")) != NULL) {
		const char* side = AeronConfigNode_String(node, "");

		for (i = 0; i < 3 && strcmp(side, s_sideNames[i]) != 0; ++i) {
		}
		if (i < 3) {
			g_projectileRequiredIffByType[at] = s_sideValues[i];
		} else {
			Aeron_LogWarn("xwa.mission", "%s:%d: %s.side must be rebel, imperial or none; it is ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), where);
		}
	}
	if ((node = AeronConfigNode_MapGet(entry, "side_model")) != NULL) {
		int sideModel;

		if (XwaNames_Read(&g_xwaWeaponTypeNames, XWA_FLIGHT_CONFIG_PATH, "side_model", node, &sideModel)) {
			g_projectileAlternateIffTypeByType[at] = (uint16_t)sideModel;
		}
	}
	if (Projectiles_ReadValue(where, entry, "homing_profile", XWA_UNIT_NONE, 0, HOMING_ROWS - 1, &value)) {
		g_projectileHomingProfileBaseByType[at] = (uint8_t)(value * HOMING_ROW_SIZE);
	}
}

/* Reads `key` of `entry`, a list of HOMING_ROW_SIZE numbers from 0 to 65535,
   into `out`. */
static void Projectiles_ReadRates(const char* where, const AeronConfigNode* entry, const char* key,
								  uint16_t* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(entry, key);
	uint16_t rates[HOMING_ROW_SIZE];
	size_t i;

	if (node == NULL) {
		return;
	}
	if (AeronConfigNode_Type(node) == AERON_CONFIG_SEQUENCE &&
		AeronConfigNode_SequenceCount(node) == HOMING_ROW_SIZE) {
		for (i = 0; i < HOMING_ROW_SIZE; ++i) {
			const AeronConfigNode* rate = AeronConfigNode_SequenceGet(node, i);
			const int64_t value = AeronConfigNode_Int(rate, -1);

			if (AeronConfigNode_Type(rate) != AERON_CONFIG_INT || value < 0 || value > UINT16_MAX) {
				break;
			}
			rates[i] = (uint16_t)value;
		}
		if (i == HOMING_ROW_SIZE) {
			memcpy(out, rates, sizeof rates);
			return;
		}
	}
	Aeron_LogWarn("xwa.mission", "%s:%d: %s.%s must be a list of %d numbers from 0 to 65535; it is ignored",
				  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), where, key, HOMING_ROW_SIZE);
}

static void Projectiles_ReadHomingProfiles(const AeronConfigNode* section) {
	size_t i;

	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.mission", "%s:%d: homing_profiles must be a map; it is ignored",
					   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(section));
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(section); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(section, i);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(section, i);
		char* end;
		const long row = strtol(key, &end, 10);
		char where[64];

		if (key[0] == '\0' || *end != '\0' || row < 0 || row >= HOMING_ROWS) {
			Aeron_LogWarn("xwa.mission", "%s:%d: homing_profiles.%s isn't a row from 0 to %d; it is ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(entry), key, HOMING_ROWS - 1);
			continue;
		}
		snprintf(where, sizeof where, "homing_profiles.%ld", row);
		Projectiles_ReadRates(where, entry, "turn_rates",
							  &g_projectileHomingTurnRateByProfile[row * HOMING_ROW_SIZE]);
		Projectiles_ReadRates(where, entry, "speed_rates",
							  &g_projectileHomingSpeedAdjustRateByProfile[row * HOMING_ROW_SIZE]);
	}
}

void XwaProjectiles_Init(void) {
	const AeronConfigNode* root = XwaFlightConfig_Root();
	const AeronConfigNode* section = AeronConfigNode_MapGet(root, "projectiles");
	size_t i;

	if (section != NULL && AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.mission", "%s:%d: projectiles must be a map; it is ignored",
					   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(section));
	} else if (section != NULL) {
		for (i = 0; i < AeronConfigNode_MapCount(section); ++i) {
			const char* key = AeronConfigNode_MapKeyAt(section, i);
			const AeronConfigNode* entry = AeronConfigNode_MapValueAt(section, i);
			const int weapon = XwaNames_Find(&g_xwaWeaponTypeNames, key);
			char where[96];

			if (weapon < OBJ_LaserRebel || weapon > OBJ_NoAsset_307) {
				XwaNames_LogUnknown(&g_xwaWeaponTypeNames, XWA_FLIGHT_CONFIG_PATH,
									AeronConfigNode_Line(entry), "projectiles", key);
				continue;
			}
			snprintf(where, sizeof where, "projectiles.%s", key);
			Projectiles_ReadWeapon(where, weapon, entry);
		}
	}
	if ((section = AeronConfigNode_MapGet(root, "homing_profiles")) != NULL) {
		Projectiles_ReadHomingProfiles(section);
	}
}
