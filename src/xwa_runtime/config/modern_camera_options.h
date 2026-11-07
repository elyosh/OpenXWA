#ifndef XWA_RUNTIME_MODERN_CAMERA_OPTIONS_H
#define XWA_RUNTIME_MODERN_CAMERA_OPTIONS_H

#include "xwa_runtime/config/option_section.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The camera options: the exterior camera's lag and tilt, the cockpit sway,
 * and the HUD in the exterior view. They are read from the `camera` section
 * of the shipped remaster/config.yaml, the mod's flight/flight.yaml and the
 * player's config.yaml. The shipped file turns them off.
 * The options screen only turns them on or off. */

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

XwaOptionSection* XwaModernCameraOptions_Section(void);
/* The values in use. */
void XwaModernCameraOptions_Get(XwaModernCameraOptions* out);

#ifdef __cplusplus
}
#endif

#endif
