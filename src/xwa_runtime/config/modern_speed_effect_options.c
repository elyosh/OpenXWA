#include "xwa_runtime/config/modern_speed_effect_options.h"

/* Modern port module; no original counterpart. */

static const XwaOptionKey g_speedEffectOptionKeys[] = {
	XWA_OPTION_SWITCH(XwaModernSpeedEffectOptions, enabled),
	XWA_OPTION_NUMBER(XwaModernSpeedEffectOptions, brightness, 0.0f, 1.0f),
	XWA_OPTION_NUMBER(XwaModernSpeedEffectOptions, particles, 0.0f, 1024.0f),
};

static XwaModernSpeedEffectOptions g_shipped, g_mod, g_player;

static XwaOptionSection g_section = {
	.name = "speed_effect",
	.mod_file = "flight/flight.yaml",
	.log_channel = "xwa.flight",
	.keys = g_speedEffectOptionKeys,
	.key_count = sizeof g_speedEffectOptionKeys / sizeof g_speedEffectOptionKeys[0],
	.size = sizeof(XwaModernSpeedEffectOptions),
	.shipped = &g_shipped,
	.mod = &g_mod,
	.player = &g_player,
};

XwaOptionSection* XwaModernSpeedEffectOptions_Section(void) { return &g_section; }

void XwaModernSpeedEffectOptions_Get(XwaModernSpeedEffectOptions* out) {
	XwaOptionSection_Get(&g_section, out);
}
