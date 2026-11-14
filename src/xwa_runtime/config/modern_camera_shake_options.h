#ifndef XWA_RUNTIME_MODERN_CAMERA_SHAKE_OPTIONS_H
#define XWA_RUNTIME_MODERN_CAMERA_SHAKE_OPTIONS_H

#include "xwa_runtime/config/option_section.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The camera shake option. In modern graphics, the camera shakes and jolts in
 * the player's hyperspace jumps (docs/hd_hyperspace_jump.md, Camera). The
 * option is read from the `camera_shake` section of the shipped
 * remaster/config.yaml, the mod's flight/flight.yaml and the player's
 * config.yaml. The shipped file turns it on. The Video tab of OpenXWA
 * Options turns it on or off. */
typedef struct XwaModernCameraShakeOptions {
	int enabled;
} XwaModernCameraShakeOptions;

XwaOptionSection* XwaModernCameraShakeOptions_Section(void);
/* The values in use. */
void XwaModernCameraShakeOptions_Get(XwaModernCameraShakeOptions* out);

#ifdef __cplusplus
}
#endif

#endif
