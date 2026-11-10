#ifndef XWA_RUNTIME_MODELS_MAG_PULSE_H
#define XWA_RUNTIME_MODELS_MAG_PULSE_H

/* What a mag pulse hit does to a craft, from the `mag_pulse` group of the
 * craft's model settings file and the `mag_pulse` section of
 * flight/flight.yaml:
 *   - drain_only: a hit on a player's craft only drains its lasers. In the
 *     stock game, it also knocks out the cannons when they work;
 *   - disable_time_ms: how long a hit stops an AI craft from firing.
 *
 * Each value comes from the craft's model first, then from flight.yaml,
 * then from the stock game. flight.yaml has one time for small craft and
 * one for large craft. The values belong to the model that the object type
 * loads in the mission. Everything here changes the simulation. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Reads the `mag_pulse` section of flight/flight.yaml. Called once at
   startup. */
void XwaMagPulse_Init(void);
/* Drops the previous mission's values. Called by the model settings reader. */
void XwaMagPulse_ClearModelSettings(void);
/* Reads the `mag_pulse` group of `objectType`'s model settings file, or does
   nothing when `group` is NULL. `path` is the file, for the log. Called by
   the model settings reader. */
void XwaMagPulse_ReadModelSettings(unsigned int objectType, const char* path,
								   const struct AeronConfigNode* group);

/* Returns 1 when a mag pulse hit on the craft `objIdx` only drains its
   lasers. */
int XwaMagPulse_DrainOnly(unsigned int objIdx);

/* Returns the ticks that a mag pulse hit adds to the fire timer of the AI
   craft `objIdx`, or `stock` when neither its model nor flight.yaml sets
   them. `small` tells whether the craft is a small craft. */
uint16_t XwaMagPulse_DisableTicks(unsigned int objIdx, int small, uint16_t stock);

#ifdef __cplusplus
}
#endif

#endif
