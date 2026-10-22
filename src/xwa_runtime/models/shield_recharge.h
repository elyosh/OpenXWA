#ifndef XWA_RUNTIME_MODELS_SHIELD_RECHARGE_H
#define XWA_RUNTIME_MODELS_SHIELD_RECHARGE_H

/* A mod's shield recharge rules, from the `shield_recharge` section of
 * flight/flight.yaml:
 *
 *   shield_recharge:
 *     starships: true                  # starships recharge
 *     starships_on_easy: false         # also on easy difficulty
 *     double_starfighter_shields: false
 *     rate_percent: 100
 *
 * Without the section, the stock rules apply. With it, the rate of each
 * craft at each periodic power update is:
 *   - for a player's craft, its model's `player_rate`, else its model rate,
 *     else 20;
 *   - for an AI starfighter, its model rate. The AI also sets its shields
 *     to recharge when they are below their maximum, which stock AI
 *     fighters only do in one maneuver or with no shields left;
 *   - for an AI starship, its model rate when `starships` is on and the
 *     difficulty isn't easy or `starships_on_easy` is on, else 0. The AI
 *     sets its shields to recharge under the same conditions;
 *   - for other AI craft, its model rate, and the AI sets its shields to
 *     recharge when the difficulty isn't easy or `starships_on_easy` is on.
 * The rate is then scaled by `rate_percent`. With
 * `double_starfighter_shields`, a starfighter's shields charge up to twice
 * the usual maximum.
 *
 * A model's rate comes from the `shield` group of its model settings file
 * (model_settings.h):
 *
 *   shield: {per_generator_rate: 5, player_rate: 40, delay_seconds: 30}
 *   shield: {rate: 20}
 *
 * `per_generator_rate` is given for each shield generator mesh that is
 * still alive, `rate` for the craft. Without the group, the model rate is
 * 20 for starfighters, 5 per live generator for the stock Star Destroyers,
 * the Interdictor and the Super Star Destroyer, 5 for other starships and 0
 * for the rest. After a craft's shields fall to 0, its flight group doesn't
 * recharge for `delay_seconds` of mission time.
 *
 * The rules change the simulation. */

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Reads the `shield_recharge` section of flight/flight.yaml. Called once
   at startup. */
void XwaShieldRecharge_Init(void);
/* Drops the previous mission's model rates. Called by model_settings.c. */
void XwaShieldRecharge_ClearModelSettings(void);
/* Reads the `shield` group of `objectType`'s model settings file, or does
   nothing when `group` is NULL. `path` is the file, for the log. Called by
   model_settings.c. */
void XwaShieldRecharge_ReadModelSettings(unsigned int objectType, const char* path,
										 const struct AeronConfigNode* group);
/* Starts a mission with no recharge delays. */
void XwaShieldRecharge_LoadMission(void);

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
