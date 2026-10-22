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
 * The rates and the rules change the simulation. */

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

/* Writes the charge that a player's shot from object `objIdx` takes from
   its rack into `rate`, and returns 1. Returns 0 for the stock rule. */
int XwaWeaponRates_Decharge(unsigned int objIdx, int* rate);
/* Replaces `*chargeDelta`, the stock charge that comes back to each laser
   rack of object `objIdx`, with `chargeBasis` times the craft's recharge
   rate. Does nothing for the stock rule. */
void XwaWeaponRates_Recharge(unsigned int objIdx, int16_t chargeBasis, int16_t* chargeDelta);
/* Returns the time that each laser fired by object `objIdx` adds before
   its next shot: 47 for the stock rule. */
int XwaWeaponRates_Cooldown(unsigned int objIdx);
/* Returns 0 when the rules of flight/flight.yaml keep object `objIdx` from
   firing rack `rackIdx` of g_curCraft, its craft. `laser` is 1 for the
   laser racks, 0 for the warhead launchers. */
int XwaWeaponRates_CanFire(unsigned int objIdx, unsigned int rackIdx, int laser);

#ifdef __cplusplus
}
#endif

#endif
