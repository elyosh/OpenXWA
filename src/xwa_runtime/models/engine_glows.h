#ifndef XWA_RUNTIME_MODELS_ENGINE_GLOWS_H
#define XWA_RUNTIME_MODELS_ENGINE_GLOWS_H

/* Engine glows that show by speed, from the `engine_glows` group of a model
 * settings file (model_settings.h):
 *
 *   engine_glows:
 *     0: {min_speed_percent: 1}     # hidden when the craft stops
 *     6: {min_speed_percent: 100}   # shown only at top speed
 *     7: {min_speed_mglt: 40}
 *
 * Keys are glow positions, in the model's engine glow order. A glow is
 * hidden while the craft's speed is below `min_speed_percent` of its top
 * speed (CraftData.aiFlight.maxSpeedCache), or below `min_speed_mglt`. Both
 * keys are optional. A craft with a top speed of 0 shows all its glows.
 *
 * The setting only changes drawing: the classic glows and their lights, and
 * the HD renderer's through the snapshot. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

enum { XWA_ENGINE_GLOWS_MASK_WORDS = 8 };

/* Drops the previous mission's settings. Called by model_settings.c. */
void XwaEngineGlows_ClearModelSettings(void);
/* Reads the `engine_glows` group of `objectType`'s model settings file, or
   does nothing when `group` is NULL. `path` is the file, for the log. Called
   by model_settings.c. */
void XwaEngineGlows_ReadModelSettings(unsigned int objectType, const char* path,
									  const struct AeronConfigNode* group);

/* Returns 1 when glow `glowIndex` of `objectType` is hidden at `speed`
   (MobileObject.speed), for a craft whose top speed is `maxSpeed`. */
int XwaEngineGlows_Hidden(unsigned int objectType, unsigned int glowIndex, uint16_t speed, uint16_t maxSpeed);
/* Sets bit N (word N >> 5) of `mask` for each hidden glow N, and clears the
   others. */
void XwaEngineGlows_HiddenMask(unsigned int objectType, uint16_t speed, uint16_t maxSpeed,
							   uint32_t mask[XWA_ENGINE_GLOWS_MASK_WORDS]);

#ifdef __cplusplus
}
#endif

#endif
