#ifndef XWA_RUNTIME_MODELS_WEAPON_RATES_H
#define XWA_RUNTIME_MODELS_WEAPON_RATES_H

/* Laser rates and other weapon values that a mod sets in the `weapons`
 * section of its flight settings and in the `weapons` group of a model
 * settings file: the laser charge rates and cooldown, the energy transfer,
 * the AI torpedo limits, the impact spin and the ion system damage.
 *
 * Each value comes from the craft's model first, then from the flight
 * settings, then from the stock game. The rates replace the stock ones only
 * when the flight settings have `rates` or a global percent, when the
 * craft's model has `rates`, or when the craft's weapon profile sets the
 * rate. Otherwise the stock code runs. The decharge rate only applies to the
 * player's shots from laser racks of weapon types 1 to 3. AI shots still use
 * 1 charge, as in the stock game. The cooldown applies to every craft.
 *
 * XwaWeaponRates_CanFire also applies two rules of the flight settings:
 * fire_needs_decharge_rate and no_starship_fire_from_destroyed_racks.
 *
 * A weapon profile's rates take the place of the `rates` values, and the
 * percents still scale them. A rack uses the rates of its weapon type, and
 * the rates of its high-power type once its charge reaches the profile's
 * high_power_charge.
 *
 * With `system_damage`, or with the rates on, system damage stops at `max`.
 * The stock game stops adding damage once it reaches 1000, but a hit can
 * take it past 1000.
 *
 * Everything here changes the simulation. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Reads the rates of the `weapons` section of flight/flight.yaml. Called
   once at startup. */
void XwaWeaponRates_Init(void);
/* Drops the previous mission's model rates. Called by the model settings
   reader. */
void XwaWeaponRates_ClearModelSettings(void);
/* Reads `rates` from the `weapons` group of `objectType`'s model settings
   file, or does nothing when `group` is NULL. `path` is the file, for the
   log. Called by the model settings reader. */
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
