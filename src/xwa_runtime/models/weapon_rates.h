#ifndef XWA_RUNTIME_MODELS_WEAPON_RATES_H
#define XWA_RUNTIME_MODELS_WEAPON_RATES_H

/* Laser rates that a mod can set: how much charge a shot uses, how much
 * charge comes back at each power update, and the time between shots.
 *
 * The `weapons` section of flight/flight.yaml sets the rates for every
 * craft. A model settings file can change any of them for
 * its craft. All keys are optional:
 *
 *   weapons:
 *     rates: {decharge: 4, recharge: 2, cooldown: 47,
 *             decharge_percent: 100, recharge_percent: 100}
 *     decharge_percent: 100           # in flight/flight.yaml only
 *     recharge_percent: 100
 *
 * For each key, the game uses the value from the craft's model file. If
 * there is none, it uses the `rates` value from flight/flight.yaml, and if
 * that is missing too, the stock value:
 *   - decharge: 3 for the TIE Fighter and the TIE Bomber, 4 for other
 *     starfighters, 3 for other craft;
 *   - recharge: 3 for the TIE Fighter and the TIE Bomber, else 2;
 *   - cooldown: 47, about 0.2 seconds per laser fired;
 *   - the percents: 100.
 * The decharge rate is scaled by `rates.decharge_percent`, then by the
 * global `decharge_percent`. The recharge rate is scaled the same way.
 *
 * The rates replace the stock ones when flight/flight.yaml has `rates` or a
 * global percent, or when the craft's model file has `rates`. Otherwise the
 * stock code runs. The decharge rate only applies to the player's shots, and
 * only to laser racks of weapon types 1 to 3. AI shots still use 1 charge, as
 * in the stock game. The cooldown applies to every craft.
 *
 * Two rules of flight/flight.yaml also change when a craft can fire:
 *   - fire_needs_decharge_rate: a craft fires a base laser (types 280, 282
 *     and 284) only when the rack's charge is at least the craft's decharge
 *     rate, instead of above 0;
 *   - no_starship_fire_from_destroyed_racks: a freighter, starship,
 *     platform or satellite doesn't fire lasers or warheads from a rack
 *     whose mesh is destroyed.
 *
 * The same `weapons` section, in flight/flight.yaml or in a model file, has
 * other groups. They follow the same order: the model's value, then the value
 * of flight/flight.yaml, then the stock value. All keys are optional:
 *
 *   weapons:
 *     energy_transfer: {rate: 4, penalty: 0, laser_limit: 100, shield_limit: 800}
 *     ai_torpedoes: {per_pass: 6, per_target: 16}
 *     impact_spin: {enabled: true, speed_percent: 100, angle_percent: 100}
 *     system_damage: {max: 1000, 284: 1, 285: 2, 290: 4, 296: 30}
 *
 *   - energy_transfer changes how the player moves energy between the lasers
 *     and the shields. `rate` is the shield energy that one unit of laser
 *     charge is worth: 32 for the Missile Boat and 4 for other craft in the
 *     stock game. When the lasers charge the shields, each unit is worth
 *     `rate` minus `penalty`. `laser_limit` is the most laser charge that one
 *     key press moves, and `shield_limit` the most shield energy.
 *   - ai_torpedoes limits the warheads that an AI craft fires at a starship,
 *     a freighter, a platform or a similar target: `per_pass` in one attack
 *     run, and `per_target` homing on the target at once.
 *   - impact_spin decides whether a warhead hit spins the craft that it hits.
 *     The stock game uses the game option. `speed_percent` and
 *     `angle_percent` scale the spin.
 *   - system_damage changes the damage that ion weapons do to a craft's
 *     systems. `max` is the most system damage that the craft can take. The
 *     weapon keys give the damage of one hit of weapon types 284, 285, 290
 *     and 296. They come from the model of the craft that fired.
 *     With these values, or with the rates on, system damage stops at `max`,
 *     as in XWAU. The stock game stops adding damage once it reaches 1000,
 *     but a hit can take it past 1000.
 * A weapon profile can set the decharge rate, the recharge rate and the
 * cooldown of each weapon type. Its values take the place of the `rates`
 * values, and the percents still scale them. A rack uses the rates of its
 * weapon type. Once its charge reaches the profile's high_power_charge, it
 * uses the rates of the next type, its high-power type.
 *
 * The rates and the rules change the simulation. So do the other groups. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Reads the rates of the `weapons` section of flight/flight.yaml. Called
   once at startup. */
void XwaWeaponRates_Init(void);
/* Drops the previous mission's model rates. Called by model_settings.c. */
void XwaWeaponRates_ClearModelSettings(void);
/* Reads `rates` from the `weapons` group of `objectType`'s model settings
   file, or does nothing when `group` is NULL. `path` is the file, for the
   log. Called by model_settings.c. */
void XwaWeaponRates_ReadModelSettings(unsigned int objectType, const char* path,
									  const struct AeronConfigNode* group);

/* Writes into `rate` the charge that a player's shot takes from rack
   `rackIdx` of object `objIdx`, and returns 1. Returns 0 for the stock rule.
   g_curCraft must be the object's craft. */
int XwaWeaponRates_Decharge(unsigned int objIdx, unsigned int rackIdx, int* rate);
/* Replaces `*chargeDelta`, the stock charge that comes back to laser rack
   `rackIdx` of object `objIdx`, with `chargeBasis` times the rack's recharge
   rate. Does nothing for the stock rule. g_curCraft must be the object's
   craft. */
void XwaWeaponRates_Recharge(unsigned int objIdx, unsigned int rackIdx, int16_t chargeBasis,
							 int16_t* chargeDelta);
/* Returns the time that each shot of a laser group of object `objIdx` adds
   before the group's next shot. `weapon` is the last weapon type that the
   group fired. The stock value is 47. */
int XwaWeaponRates_Cooldown(unsigned int objIdx, unsigned int weapon);
/* Returns 0 when the rules of flight/flight.yaml keep object `objIdx` from
   firing rack `rackIdx` of g_curCraft, its craft. `laser` is 1 for the
   laser racks, 0 for the warhead launchers. */
int XwaWeaponRates_CanFire(unsigned int objIdx, unsigned int rackIdx, int laser);

/* Returns the shield energy that one unit of laser charge of object
   `objIdx` is worth, or `stock`. */
int XwaWeaponRates_EnergyTransfer(unsigned int objIdx, int stock);
/* Returns what one unit of laser charge of object `objIdx` is worth when it
   charges the shields: `rate`, the value of XwaWeaponRates_EnergyTransfer,
   minus the penalty, and at least 1. */
int XwaWeaponRates_EnergyTransferToShields(unsigned int objIdx, int rate);
/* Returns the most laser charge that one transfer of object `objIdx` moves:
   100 in the stock game. */
int XwaWeaponRates_EnergyTransferLaserLimit(unsigned int objIdx);
/* Returns the most shield energy that one transfer of object `objIdx` moves:
   800 in the stock game. */
int XwaWeaponRates_EnergyTransferShieldLimit(unsigned int objIdx);
/* Return the warhead limits of AI object `objIdx` against a big target, or
   `stock`. */
int XwaWeaponRates_AiTorpedoesPerPass(unsigned int objIdx, int stock);
int XwaWeaponRates_AiTorpedoesPerTarget(unsigned int objIdx, int stock);
/* Returns 1 when a warhead hit spins object `objIdx`. `stock` is the game
   option. */
int XwaWeaponRates_ImpactSpin(unsigned int objIdx, int stock);
/* Scales `*speed` and `*angle`, the spin that object `impactObjIdx` gives
   object `objIdx`, when `impactObjIdx` is a warhead. */
void XwaWeaponRates_ScaleImpactSpin(unsigned int objIdx, unsigned int impactObjIdx, int* speed, int* angle);
/* Adds the system damage that a hit of `weapon`, fired as projectile
   `projectileObjIdx`, does to object `objIdx`, to `*damage`, and returns 1.
   Returns 0 for the stock rule. */
int XwaWeaponRates_AddSystemDamage(unsigned int objIdx, unsigned int projectileObjIdx, int weapon,
								   uint16_t* damage);

#ifdef __cplusplus
}
#endif

#endif
