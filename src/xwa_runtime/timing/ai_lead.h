#ifndef XWA_RUNTIME_AI_LEAD_H
#define XWA_RUNTIME_AI_LEAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The target movement for the AI shot lead in high-rate mode.
 *
 * paiman_calcplanelead aims ahead of the target by leadFrames times the
 * target's movement over one step (world - prevWorld). leadFrames is the
 * laser's flight time in steps. It comes from g_simStepScale, the number of
 * steps per second, which the AI step sets for 8-tick steps. In the
 * original, a step lasts 8 ticks on a fast PC, so both values use the same
 * step. In high-rate mode a step is one host frame of 2 to 4 ticks, so the
 * lead would be only about 26% of the original at 120 Hz and 52% at 60 Hz.
 *
 * So in high-rate mode, the lead uses the target's movement since the last
 * AI step instead. The AI steps aren't always 8 ticks apart. With 3-tick
 * steps the gaps are 9, 9 and 6 ticks. After a slow frame, one gap is long,
 * and the AI then catches up with one AI step per frame. So the movement is
 * scaled to the AI step's ticks.
 *
 * In compatibility timing, these functions do nothing and the lead is the
 * original one. */

/* Drops the recorded movement. The first AI step after this uses the
   original lead. */
void XwaAiLead_Reset(void);
/* Called by the object step just before it sets the prevWorld of object
   `slot` to its world position. */
void XwaAiLead_AddMovement(uint32_t slot);
/* Called at each AI step of the active region, before the AI runs, with the
   ticks of the AI step. */
void XwaAiLead_BeginAiStep(uint16_t elapsed_ticks);
/* Writes the movement of object `slot` since the last AI step, scaled to the
   AI step's ticks, to `delta_x`, `delta_y` and `delta_z`, and returns 1.
   Returns 0 in compatibility timing, and when the movement isn't known, such
   as for an object created since the last AI step. The caller then uses
   world - prevWorld, as the original does. */
int XwaAiLead_Movement(uint32_t slot, int* delta_x, int* delta_y, int* delta_z);

#ifdef __cplusplus
}
#endif

#endif
