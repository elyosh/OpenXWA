#ifndef XWA_RUNTIME_MODERN_SPEED_EFFECT_OPTIONS_H
#define XWA_RUNTIME_MODERN_SPEED_EFFECT_OPTIONS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The speed effect options. The speed effect draws small streaks of dust
 * around the camera, which show how fast the player's craft moves. The
 * options come from three places, and each one overrides the one before it:
 *   1. the `speed_effect` section of the shipped remaster/config.yaml. It
 *      has every value, and the effect is off;
 *   2. the `speed_effect` section of the mod's flight/flight.yaml;
 *   3. the `speed_effect` section of the player's config.yaml.
 * A key that a place leaves out keeps the value before it. */
typedef struct XwaModernSpeedEffectOptions {
	int enabled;
	/* The streaks' brightness at full speed, from 0 to 1. */
	float brightness;
	/* The number of streaks in the box around the camera. */
	float particles;
} XwaModernSpeedEffectOptions;

/* A key of a `speed_effect` section. A switch is an int set from true or
   false. A number is a float from `min` to `max`. `offset` is the field's
   offset in XwaModernSpeedEffectOptions. */
typedef struct XwaModernSpeedEffectOptionKey {
	const char* name;
	int is_switch;
	float min;
	float max;
	size_t offset;
} XwaModernSpeedEffectOptionKey;

/* The keys, in the order of the shipped file. In an override mask, bit `i`
   stands for key `i`. */
const XwaModernSpeedEffectOptionKey* XwaModernSpeedEffectOptions_Keys(size_t* count);

/* `shipped` holds the shipped values. `player` holds the player's values for
   the keys that `override_mask` has. */
void XwaModernSpeedEffectOptions_Configure(const XwaModernSpeedEffectOptions* shipped,
										   const XwaModernSpeedEffectOptions* player,
										   unsigned int override_mask);

/* Reads the `speed_effect` section of the mod's flight/flight.yaml, which
   replaces the shipped values. Called once at startup. */
void XwaModernSpeedEffectOptions_LoadMod(void);

/* The values in use: the mod's, or the shipped ones without a mod, with the
   player's values for the keys the player has set. */
void XwaModernSpeedEffectOptions_Get(XwaModernSpeedEffectOptions* out);

#ifdef __cplusplus
}
#endif

#endif
