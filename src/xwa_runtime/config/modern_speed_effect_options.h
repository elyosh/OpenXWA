#ifndef XWA_RUNTIME_MODERN_SPEED_EFFECT_OPTIONS_H
#define XWA_RUNTIME_MODERN_SPEED_EFFECT_OPTIONS_H

#include "xwa_runtime/config/option_section.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The speed effect options. The speed effect draws small streaks of dust
 * around the camera, which show how fast the player's craft moves. The
 * options are read from the `speed_effect` section of the shipped
 * remaster/config.yaml, the mod's flight/flight.yaml and the player's
 * config.yaml. The shipped file turns the effect off. */
typedef struct XwaModernSpeedEffectOptions {
	int enabled;
	/* The streaks' brightness at full speed, from 0 to 1. */
	float brightness;
	/* The number of streaks in the box around the camera. */
	float particles;
} XwaModernSpeedEffectOptions;

XwaOptionSection* XwaModernSpeedEffectOptions_Section(void);
/* The values in use. */
void XwaModernSpeedEffectOptions_Get(XwaModernSpeedEffectOptions* out);

#ifdef __cplusplus
}
#endif

#endif
