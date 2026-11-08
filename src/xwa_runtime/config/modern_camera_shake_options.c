#include "xwa_runtime/config/modern_camera_shake_options.h"

#include "xwa_runtime/models/flight_config.h"

/* Modern port module; no original counterpart. */

static const XwaOptionKey g_cameraShakeOptionKeys[] = {
	XWA_OPTION_SWITCH(XwaModernCameraShakeOptions, enabled),
};

static XwaModernCameraShakeOptions g_shipped, g_mod, g_player;

static XwaOptionSection g_section = {
	.name = "camera_shake",
	.mod_file = XWA_FLIGHT_CONFIG_PATH,
	.log_channel = "xwa.flight",
	.keys = g_cameraShakeOptionKeys,
	.key_count = sizeof g_cameraShakeOptionKeys / sizeof g_cameraShakeOptionKeys[0],
	.size = sizeof(XwaModernCameraShakeOptions),
	.shipped = &g_shipped,
	.mod = &g_mod,
	.player = &g_player,
};

XwaOptionSection* XwaModernCameraShakeOptions_Section(void) { return &g_section; }

void XwaModernCameraShakeOptions_Get(XwaModernCameraShakeOptions* out) {
	XwaOptionSection_Get(&g_section, out);
}
