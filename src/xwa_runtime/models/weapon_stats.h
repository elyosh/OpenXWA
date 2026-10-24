#ifndef XWA_RUNTIME_MODELS_WEAPON_STATS_H
#define XWA_RUNTIME_MODELS_WEAPON_STATS_H

/* Weapon profiles let a mod change the stats of weapon types 280 to 307 for
 * each craft. They also change some other weapon values for each craft.
 *
 * A model settings file sets the values for the craft that use the model, in
 * its `weapons` group. The `weapons` section of flight/flight.yaml has the
 * same groups and sets the values for every craft. All keys are optional:
 *
 *   weapons:
 *     stats:
 *       default:
 *         280: {range: 65536, fire_ratio: 236, precise: false, spread_mask: 63}
 *         282:
 *           fire_rate: 47
 *           player_easy: {fire_rate: 39, power: 350}
 *       devastatorb7m2r:
 *         303: {power: 900, range: 204800}
 *     warhead_capacity_percent:
 *       default: {1: 50, 2: 50}
 *     energy_speed_q16:
 *       default: {all: 8192, player: 7864}
 *     linking:
 *       default: {laser_link: 2, prevent_player_linked_lasers_ion: true}
 *     hardpoint_kinds: [0, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 1]
 *
 * Each entry of `stats`, `warhead_capacity_percent`, `energy_speed_q16` and
 * `linking` is a profile. `default` is used when no profile is picked. A
 * mission picks a profile for a flight group in the flight group's entry of
 * the mission file:
 *
 *   flight_groups:
 *     13: { name: "Devastator", craft: 26, weapon_profile: devastatorb7m2r }
 *
 * `weapon_profile` picks a profile of `stats`, `warhead_profile` one of
 * `warhead_capacity_percent`, `energy_profile` one of `energy_speed_q16` and
 * `linking_profile` one of `linking`. When a flight group uses a profile,
 * only that profile's values count. A key that the profile doesn't set gets
 * the stock value, not the `default` value. If neither the model file nor
 * flight/flight.yaml has the profile, the flight group uses `default`.
 *
 * The stats of a weapon type are:
 *   - duration_seconds and duration_fraction: how long a shot lives, in
 *     seconds plus a Q16 fraction of a second;
 *   - score: the points for a kill with the weapon;
 *   - speed and power: the speed of a shot and the damage it does;
 *   - power_speed_percent: how much of the firing craft's speed is added to
 *     the damage, in percent. The stock value is 100;
 *   - fire_rate: the time between two shots of a craft's lasers or
 *     warheads;
 *   - duration_offset: how long a turret shot lives after it passes its
 *     target. The stock value is 59;
 *   - fire_ratio and range: the time between two shots of a turret, and its
 *     range. The stock values come from the craft record;
 *   - precise and spread_mask: whether an AI turret aims exactly, and how
 *     far its shots spread when it doesn't. In the stock game, turrets aim
 *     exactly. A player's turret shots always spread;
 *   - decharge_rate and recharge_rate: the charge that a shot uses, and the
 *     charge that comes back. They take the place of the laser rates;
 *   - high_power_charge: the charge at which a laser fires its high-power
 *     shot. The stock value is 64.
 * A weapon type can also set stats for the player, for one difficulty, or
 * for both: `player`, `easy`, `medium`, `hard`, `player_easy`,
 * `player_medium` and `player_hard`. For each stat, the game tries these
 * first, then the plain value. Each time, it tries the model's value first,
 * then the value in flight/flight.yaml. If none is set, it uses the stock
 * value.
 *
 * The other groups:
 *   - warhead_capacity_percent: how much of a launcher's capacity a craft
 *     loads, in percent, for each warhead type of a flight group (1 to 11,
 *     like the warhead values of a mission). The stock percents go from 25
 *     to 100. A craft always gets at least 1 warhead;
 *   - energy_speed_q16: how much speed a craft gains or loses for each step
 *     of energy moved to or from its engines, as a Q16 fraction of its top
 *     speed. `player` is for the player's craft, `all` for every craft. The
 *     stock value is 4096 for the TIE Bomber, 12288 for the TIE Fighter with
 *     spare engine power, and 8192 for other craft;
 *   - linking: `laser_link` sets the craft's laser link mode. The three
 *     rules stop the AI from firing lasers while it disables its target,
 *     stop the AI from firing ions when it doesn't, and stop the player from
 *     linking the lasers and the ions. `weapons.linking_rules` in
 *     flight/flight.yaml sets the rules for every profile;
 *   - hardpoint_kinds: whether each OPT hardpoint type, from 0 to 39, is a
 *     laser hardpoint (1) or a warhead hardpoint (2) when the model loads.
 *     Missing values are 0. The list replaces the stock table.
 *
 * flight/flight.yaml can also change these stock rules:
 *
 *   weapons:
 *     fire_ratio_by_rank: [4, 3, 2, 2, 1, 1]
 *     damage_by_difficulty: [0.5, 0.25, 1]
 *     limit_linked_warhead_fire_rate: true
 *     linking_rules: {prevent_ai_lasers_ion_when_disabling: true,
 *                     prevent_ai_ion_when_not_disabling: true,
 *                     prevent_player_linked_lasers_ion: true}
 *
 *   - fire_ratio_by_rank: the time between two shots of an AI turret or
 *     launcher is multiplied by the factor of its flight group's rank, from
 *     novice to super ace. In the stock game, the factor depends on the AI's
 *     skill: 4, 3, 1, 2, 2 or 2;
 *   - damage_by_difficulty: the damage that a craft's components take while
 *     its shields are up is multiplied by the factor of the difficulty: easy,
 *     medium and hard. In the stock game, the damage is halved on easy and
 *     divided by 4 on medium;
 *   - limit_linked_warhead_fire_rate: the time between two warhead shots is
 *     multiplied by the number of warheads fired.
 * The factors are float32 values, as in XWAU. The game multiplies them in
 * float32, which gives the same result on every platform.
 *
 * All of this changes the simulation. Without these settings, the stock code
 * runs. */

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
