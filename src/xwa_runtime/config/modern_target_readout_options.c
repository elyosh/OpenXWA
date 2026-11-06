#include "xwa_runtime/config/modern_target_readout_options.h"

#include "aeron/log.h"

static const char* const g_textSizeWords[] = { "small", "medium", "large", NULL };
static const char* const g_nameColorWords[] = { "single", "cmd", NULL };
static const char* const g_layoutWords[] = { "vertical", "horizontal", NULL };

static const XwaOptionKey g_targetReadoutKeys[] = {
	XWA_OPTION_SWITCH(XwaModernTargetReadoutOptions, enabled),
	XWA_OPTION_CHOICE(XwaModernTargetReadoutOptions, text_size, g_textSizeWords),
	XWA_OPTION_NUMBER(XwaModernTargetReadoutOptions, text_gap, 0.0f, 100.0f),
	XWA_OPTION_SWITCH(XwaModernTargetReadoutOptions, brighten_text),
	XWA_OPTION_CHOICE(XwaModernTargetReadoutOptions, name_colors, g_nameColorWords),
	XWA_OPTION_SWITCH(XwaModernTargetReadoutOptions, bars),
	XWA_OPTION_CHOICE(XwaModernTargetReadoutOptions, layout, g_layoutWords),
	XWA_OPTION_NUMBER(XwaModernTargetReadoutOptions, bar_length_min, 1.0f, 1000.0f),
	XWA_OPTION_NUMBER(XwaModernTargetReadoutOptions, bar_length_max, 1.0f, 1000.0f),
	XWA_OPTION_NUMBER(XwaModernTargetReadoutOptions, bar_thickness, 1.0f, 50.0f),
	XWA_OPTION_NUMBER(XwaModernTargetReadoutOptions, bar_gap, 0.0f, 100.0f),
	XWA_OPTION_NUMBER(XwaModernTargetReadoutOptions, bar_frame, 0.0f, 10.0f),
	XWA_OPTION_COLOR(XwaModernTargetReadoutOptions, shield_color),
	XWA_OPTION_COLOR(XwaModernTargetReadoutOptions, overshield_color),
	XWA_OPTION_COLOR(XwaModernTargetReadoutOptions, hull_color_full),
	XWA_OPTION_COLOR(XwaModernTargetReadoutOptions, hull_color_half),
	XWA_OPTION_COLOR(XwaModernTargetReadoutOptions, hull_color_low),
	XWA_OPTION_COLOR(XwaModernTargetReadoutOptions, system_color),
	XWA_OPTION_NUMBER(XwaModernTargetReadoutOptions, component_box_min, 0.0f, 1000.0f),
	XWA_OPTION_NUMBER(XwaModernTargetReadoutOptions, component_box_max, 0.0f, 1000.0f),
	XWA_OPTION_SWITCH(XwaModernTargetReadoutOptions, component_pulse),
	XWA_OPTION_NUMBER(XwaModernTargetReadoutOptions, hidden_within_m, 0.0f, 1000.0f),
};

/* Sets bar_length_max to bar_length_min when it is below it, and the same
   for component_box_max. A component box size of 0 means no limit, so then
   component_box_max isn't changed. Returns 1 when a value changes. With
   `warn`, it logs each change. */
static int target_readout_fix_limits(XwaModernTargetReadoutOptions* options, int warn) {
	int changed = 0;

	if (options->bar_length_max < options->bar_length_min) {
		if (warn) {
			Aeron_LogWarn("xwa.config",
						  "target_readout.bar_length_max (%g) is below bar_length_min (%g); both use %g",
						  (double)options->bar_length_max, (double)options->bar_length_min,
						  (double)options->bar_length_min);
		}
		options->bar_length_max = options->bar_length_min;
		changed = 1;
	}
	if (options->component_box_min > 0.0f && options->component_box_max > 0.0f &&
		options->component_box_max < options->component_box_min) {
		if (warn) {
			Aeron_LogWarn(
				"xwa.config",
				"target_readout.component_box_max (%g) is below component_box_min (%g); both use %g",
				(double)options->component_box_max, (double)options->component_box_min,
				(double)options->component_box_min);
		}
		options->component_box_max = options->component_box_min;
		changed = 1;
	}
	return changed;
}

static void target_readout_check(const void* options) {
	XwaModernTargetReadoutOptions copy = *(const XwaModernTargetReadoutOptions*)options;
	target_readout_fix_limits(&copy, 1);
}

static XwaModernTargetReadoutOptions g_shipped, g_mod, g_player;

static XwaOptionSection g_section = {
	.name = "target_readout",
	.mod_file = "flight/hud.yaml",
	.log_channel = "xwa.hud",
	.keys = g_targetReadoutKeys,
	.key_count = sizeof g_targetReadoutKeys / sizeof g_targetReadoutKeys[0],
	.size = sizeof(XwaModernTargetReadoutOptions),
	.shipped = &g_shipped,
	.mod = &g_mod,
	.player = &g_player,
	.check = target_readout_check,
};

XwaOptionSection* XwaModernTargetReadoutOptions_Section(void) { return &g_section; }

void XwaModernTargetReadoutOptions_Get(XwaModernTargetReadoutOptions* out) {
	if (!out) {
		return;
	}
	XwaOptionSection_Get(&g_section, out);
	target_readout_fix_limits(out, 0);
}
