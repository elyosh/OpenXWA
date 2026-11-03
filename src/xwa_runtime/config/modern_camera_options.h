#ifndef XWA_RUNTIME_MODERN_CAMERA_OPTIONS_H
#define XWA_RUNTIME_MODERN_CAMERA_OPTIONS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The camera options: the exterior camera's lag and tilt, the cockpit sway,
 * and the HUD in the exterior view. They come from three places, and each
 * one overrides the one before it:
 *   1. the `camera` section of the shipped remaster/config.yaml, which has
 *      every value, with the feature off;
 *   2. the `camera` section of the mod's flight/flight.yaml;
 *   3. the `camera` section of the player's config.yaml.
 * The options screen only turns the feature on or off. */

typedef struct XwaModernCameraOptions {
	int enabled;
	int exterior_lag;
	/* Radians that the exterior camera turns for each radian of lag. */
	float exterior_lag_strength;
	/* Degrees. */
	float exterior_lag_max;
	/* Degrees above the craft. */
	float exterior_tilt;
	int cockpit_sway;
	/* Metres that the eye moves for each radian of lag. */
	float cockpit_sway_strength_m;
	/* Metres that the eye moves back for each MGLT of speed change. */
	float cockpit_speed_sway_m;
	/* Metres, on each axis. */
	float cockpit_sway_max_m;
	/* Seconds. */
	float lag_time;
	int exterior_hud;
} XwaModernCameraOptions;

/* A key of a `camera` section. A switch is an int set from true or false. A
   number is a float from `min` to `max`. `offset` is the field's offset in
   XwaModernCameraOptions. */
typedef struct XwaModernCameraOptionKey {
	const char* name;
	int is_switch;
	float min;
	float max;
	size_t offset;
} XwaModernCameraOptionKey;

/* The keys, in the order of the shipped file. In an override mask, bit `i`
   stands for key `i`. */
const XwaModernCameraOptionKey* XwaModernCameraOptions_Keys(size_t* count);
/* The bit of `camera.enabled` in an override mask. */
unsigned int XwaModernCameraOptions_EnabledBit(void);

/* Saves the player's switch in config.yaml. */
typedef int (*XwaModernCameraOptionsPersistFn)(int enabled, char* error, size_t error_size);

/* `shipped` holds the shipped values. `player` holds the player's values for
   the keys that `override_mask` has. */
void XwaModernCameraOptions_Configure(const XwaModernCameraOptions* shipped,
									  const XwaModernCameraOptions* player, unsigned int override_mask,
									  XwaModernCameraOptionsPersistFn persist);
void XwaModernCameraOptions_GetShipped(XwaModernCameraOptions* out);
/* Replaces the shipped values with the mod's. */
void XwaModernCameraOptions_SetMod(const XwaModernCameraOptions* options);
/* The values in use: the mod's, or the shipped ones without a mod, with the
   player's values for the keys the player has set. */
void XwaModernCameraOptions_Get(XwaModernCameraOptions* out);
/* The player turns the feature on or off. Flush saves the choice. */
void XwaModernCameraOptions_SetEnabled(int enabled);
int XwaModernCameraOptions_Flush(void);
int XwaModernCameraOptions_IsDirty(void);

#ifdef __cplusplus
}
#endif

#endif
