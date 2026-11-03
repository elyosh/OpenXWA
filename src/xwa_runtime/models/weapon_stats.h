#ifndef XWA_RUNTIME_MODELS_WEAPON_STATS_H
#define XWA_RUNTIME_MODELS_WEAPON_STATS_H

/* Weapon profiles, which change the stats of each weapon type and some
 * other weapon values for each craft, and the weapon rules of the stock
 * game that a mod can change. A mod sets them in the `weapons` section of
 * its flight settings and in the `weapons` group of a model settings file.
 * A mission picks a profile for each flight group in the flight group's
 * entry of the mission file.
 *
 * For each stat, the game tries the player and difficulty forms first, then
 * the plain value. Each time, it tries the model's profile first, then the
 * profile in the flight settings. If none sets the stat, the stock value
 * stays. A flight group that uses a profile only gets that profile's
 * values: a key that the profile doesn't set gets the stock value, not the
 * value of the `default` profile.
 *
 * The factors of fire_ratio_by_rank and damage_by_difficulty are float32
 * values. The game multiplies them in float32, which gives the same result
 * on every platform.
 *
 * All of this changes the simulation. Without these settings, the stock
 * code runs. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;
struct XwaFlightGroup;

typedef enum XwaWeaponStat {
	XWA_WEAPON_STAT_DURATION_SECONDS,
	XWA_WEAPON_STAT_DURATION_FRACTION,
	XWA_WEAPON_STAT_SCORE,
	XWA_WEAPON_STAT_SPEED,
	XWA_WEAPON_STAT_POWER,
	XWA_WEAPON_STAT_POWER_SPEED_PERCENT,
	XWA_WEAPON_STAT_FIRE_RATE,
	XWA_WEAPON_STAT_DURATION_OFFSET,
	XWA_WEAPON_STAT_FIRE_RATIO,
	XWA_WEAPON_STAT_RANGE,
	XWA_WEAPON_STAT_PRECISE,
	XWA_WEAPON_STAT_SPREAD_MASK,
	XWA_WEAPON_STAT_DECHARGE_RATE,
	XWA_WEAPON_STAT_RECHARGE_RATE,
	XWA_WEAPON_STAT_HIGH_POWER_CHARGE,
	XWA_WEAPON_STATS
} XwaWeaponStat;

typedef enum XwaWeaponLinkingRule {
	XWA_WEAPON_LINKING_PREVENT_AI_LASERS_ION_WHEN_DISABLING,
	XWA_WEAPON_LINKING_PREVENT_AI_ION_WHEN_NOT_DISABLING,
	XWA_WEAPON_LINKING_PREVENT_PLAYER_LINKED_LASERS_ION,
	XWA_WEAPON_LINKING_RULES
} XwaWeaponLinkingRule;

/* Reads the `weapons` section of flight/flight.yaml. Called once at
   startup. */
void XwaWeaponStats_Init(void);
/* Drops the previous mission's model values. Called by the model settings
   reader. */
void XwaWeaponStats_ClearModelSettings(void);
/* Reads the profiles in the `weapons` group of the model settings file of
   `objectType`. Does nothing when `group` is NULL. `path` is the file name,
   for the log. Called by the model settings reader. */
void XwaWeaponStats_ReadModelSettings(unsigned int objectType, const char* path,
									  const struct AeronConfigNode* group);

/* Starts a mission with no profiles. Called when the mission's flight groups
   are read. */
void XwaWeaponStats_ClearFlightGroups(void);
/* Reads the profiles that flight group `fgIdx` picks in its entry `entry`.
   `fgKey` is the entry's key and `path` the mission file, for the log. Called
   when the mission's flight groups are read. Returns the number of profiles
   read. */
int XwaWeaponStats_ReadFlightGroup(const char* path, const char* fgKey, int fgIdx,
								   const struct AeronConfigNode* entry);

/* Returns `stat` for weapon type `weapon` (280 to 307) when object
   `srcObjIdx` fires it. Returns `stock` when no profile sets it. */
int XwaWeaponStats_Value(int srcObjIdx, unsigned int weapon, XwaWeaponStat stat, int stock);
/* Returns `stat` for weapon type `weapon` when an AI craft of object type
   `objectType` fires it, without a flight group. Returns `stock` when no
   profile sets it. */
int XwaWeaponStats_TypeValue(unsigned int objectType, unsigned int weapon, XwaWeaponStat stat, int stock);

/* Returns how many warheads a launcher of object `objIdx` loads.
   `capacity` is the launcher's capacity, and `warheadType` the warhead value
   of the flight group. With a profile, the result is the profile's percent of
   `capacity`. Without one, it is the stock MATH2_fraction(capacity,
   fracQ16). */
uint16_t XwaWeaponStats_WarheadCapacity(unsigned int objIdx, int warheadType, uint16_t capacity,
										uint16_t fracQ16);
/* Returns how much speed object `objIdx` gains or loses for each step of
   energy moved to or from its engines, as a Q16 fraction of its top speed.
   Returns `stock` when no profile sets it. */
int XwaWeaponStats_EnergySpeedStep(unsigned int objIdx, int stock);
/* Returns the laser link mode of object `objIdx`, or `stock` when no
   profile sets it. */
int XwaWeaponStats_LaserLink(unsigned int objIdx, int stock);
/* Returns 1 when linking rule `rule` applies to object `objIdx`. */
int XwaWeaponStats_LinkingRule(unsigned int objIdx, XwaWeaponLinkingRule rule);
/* Returns whether OPT hardpoint type `hardpointType` is a laser hardpoint
   (1), a warhead hardpoint (2) or neither (0) for object type `objectType`.
   `stock` is the value of the stock table. */
int XwaWeaponStats_HardpointKind(unsigned int objectType, int hardpointType, int stock);

/* The rule turbolaser_hardpoint_types uses these two maps. This one
   returns the weapon type of OPT hardpoint type `hardpointType`: 280 to 297
   for types 1 to 18, 301 to 305 for types 33 to 37, 307 for type 38, and 0
   for other types. */
int XwaWeaponStats_HardpointWeaponType(int hardpointType);
/* Returns the OPT hardpoint type of weapon type `weapon`. For the other
   weapon types, it returns `weapon` - 279, as in the stock game. */
int XwaWeaponStats_WeaponHardpointType(int weapon);

/* Returns 1 when flight/flight.yaml has fire_ratio_by_rank. */
int XwaWeaponStats_HasFireRatioByRank(void);
/* Returns `fireRatio` times the factor of the rank of the flight group of
   AI object `objIdx`. */
int XwaWeaponStats_FireRatioByRank(unsigned int objIdx, int fireRatio);
/* Writes `damage` times the factor of the difficulty into `out`, and
   returns 1. Returns 0 when flight/flight.yaml has no
   damage_by_difficulty. */
int XwaWeaponStats_DamageByDifficulty(int damage, int* out);
int XwaWeaponStats_LimitLinkedWarheadFireRate(void);

#ifdef __cplusplus
}
#endif

#endif
