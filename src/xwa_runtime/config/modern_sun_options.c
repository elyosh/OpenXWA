#include "xwa_runtime/config/modern_sun_options.h"

/* Modern port module; no original counterpart. */

static const XwaOptionKey g_sunOptionKeys[] = {
	XWA_OPTION_SWITCH(XwaModernSunOptions, procedural_sun),
	XWA_OPTION_SWITCH(XwaModernSunOptions, procedural_flare),
	XWA_OPTION_NUMBER(XwaModernSunOptions, flare_intensity, 0.0f, 10.0f),
	XWA_OPTION_MOD_NUMBER(XwaModernSunOptions, brightness, 0.0f, 100.0f),
	XWA_OPTION_MOD_NUMBER(XwaModernSunOptions, core_brightness, 0.0f, 1000.0f),
};

/* The HD renderer reads them. */
static const char* const g_sunOtherKeys[] = { "flare_elements", NULL };

static XwaModernSunOptions g_shipped, g_mod, g_player;

static XwaOptionSection g_section = {
	.name = "suns",
	.mod_file = "flight/flight.yaml",
	.log_channel = "xwa.flight",
	.keys = g_sunOptionKeys,
	.key_count = sizeof g_sunOptionKeys / sizeof g_sunOptionKeys[0],
	.other_keys = g_sunOtherKeys,
	.size = sizeof(XwaModernSunOptions),
	.shipped = &g_shipped,
	.mod = &g_mod,
	.player = &g_player,
};

XwaOptionSection* XwaModernSunOptions_Section(void) { return &g_section; }

void XwaModernSunOptions_Get(XwaModernSunOptions* out) { XwaOptionSection_Get(&g_section, out); }
