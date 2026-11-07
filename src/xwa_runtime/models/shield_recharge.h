#ifndef XWA_RUNTIME_MODELS_SHIELD_RECHARGE_H
#define XWA_RUNTIME_MODELS_SHIELD_RECHARGE_H

/* A mod's shield recharge rules, from the `shield_recharge` section of its
 * flight settings, and the recharge rate of each model, from the `shield`
 * group of its model settings file.
 *
 * Without the section, the stock rules apply. With it, the AI also sets the
 * shields of a craft with a recharge rate to recharge while they are below
 * their maximum. Stock AI fighters only do this in one maneuver, or when
 * they have no shields left. For craft other than starfighters, the AI only
 * does it when the difficulty isn't easy or `starships_on_easy` is on. The
 * rate is scaled by `rate_percent` last.
 *
 * The delay after a craft's shields fall to 0 is kept for each flight
 * group, and saved with the flight state.
 *
 * The rules change the simulation. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Reads the `shield_recharge` section of flight/flight.yaml. Called once
   at startup. */
void XwaShieldRecharge_Init(void);
/* Drops the previous mission's model rates. Called by the model settings
   reader. */
void XwaShieldRecharge_ClearModelSettings(void);
/* Reads the `shield` group of `objectType`'s model settings file, or does
   nothing when `group` is NULL. `path` is the file, for the log. Called by
   the model settings reader. */
void XwaShieldRecharge_ReadModelSettings(unsigned int objectType, const char* path,
										 const struct AeronConfigNode* group);
/* Starts a mission with no recharge delays. */
void XwaShieldRecharge_LoadMission(void);

enum { XWA_SHIELD_RECHARGE_FLIGHT_GROUPS = 256 };
/* For films: copies the mission time until which each flight group doesn't
   recharge (0: it recharges) to `delayUntil`, or back from it. */
void XwaShieldRecharge_SaveFlightState(int32_t delayUntil[XWA_SHIELD_RECHARGE_FLIGHT_GROUPS]);
void XwaShieldRecharge_RestoreFlightState(const int32_t delayUntil[XWA_SHIELD_RECHARGE_FLIGHT_GROUPS]);

/* Replaces `*rate`, the stock recharge rate of object `objIdx`, with the
   mod's, and sets the AI's shields to recharge as the rules say. `aiUpdate`
   is 1 at the AI's periodic power update, 0 for a player's craft. Does
   nothing without the section. g_curCraft is the object's craft. */
void XwaShieldRecharge_Rate(unsigned int objIdx, int aiUpdate, int* rate);
/* Returns the shield strength that bounds a shield of object `objIdx`
   during recharge: `shields`, doubled for starfighters when the section
   asks for it. */
int XwaShieldRecharge_MaxShield(unsigned int objIdx, int shields);

#ifdef __cplusplus
}
#endif

#endif
