#include "xwa_runtime/config/modern_camera_options.h"

/* Modern port module; no original counterpart. */

static const XwaOptionKey g_cameraOptionKeys[] = {
	XWA_OPTION_SWITCH(XwaModernCameraOptions, enabled),
	XWA_OPTION_SWITCH(XwaModernCameraOptions, exterior_lag),
	XWA_OPTION_NUMBER(XwaModernCameraOptions, exterior_lag_strength, 0.0f, 2.0f),
	XWA_OPTION_NUMBER(XwaModernCameraOptions, exterior_lag_max, 0.0f, 15.0f),
	XWA_OPTION_NUMBER(XwaModernCameraOptions, exterior_tilt, -30.0f, 30.0f),
	XWA_OPTION_SWITCH(XwaModernCameraOptions, cockpit_sway),
	XWA_OPTION_NUMBER(XwaModernCameraOptions, cockpit_sway_strength_m, 0.0f, 0.5f),
	XWA_OPTION_NUMBER(XwaModernCameraOptions, cockpit_speed_sway_m, 0.0f, 0.05f),
	XWA_OPTION_NUMBER(XwaModernCameraOptions, cockpit_sway_max_m, 0.0f, 0.25f),
	XWA_OPTION_NUMBER(XwaModernCameraOptions, lag_time, 0.05f, 2.0f),
	XWA_OPTION_SWITCH(XwaModernCameraOptions, exterior_hud),
};

static XwaModernCameraOptions g_shipped, g_mod, g_player;

static XwaOptionSection g_section = {
	.name = "camera",
	.mod_file = "flight/flight.yaml",
	.log_channel = "xwa.flight",
	.keys = g_cameraOptionKeys,
	.key_count = sizeof g_cameraOptionKeys / sizeof g_cameraOptionKeys[0],
	.size = sizeof(XwaModernCameraOptions),
	.shipped = &g_shipped,
	.mod = &g_mod,
	.player = &g_player,
};

XwaOptionSection* XwaModernCameraOptions_Section(void) { return &g_section; }

void XwaModernCameraOptions_Get(XwaModernCameraOptions* out) { XwaOptionSection_Get(&g_section, out); }
