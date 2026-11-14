#ifndef XWA_RUNTIME_MODERN_TARGET_READOUT_OPTIONS_H
#define XWA_RUNTIME_MODERN_TARGET_READOUT_OPTIONS_H

#include "xwa_runtime/config/option_section.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The target readout options. The target readout shows the target's name,
 * cargo, distance and damage bars next to its box in the 3D view. The
 * options are read from the `target_readout` section of the shipped
 * remaster/config.yaml, the mod's flight/hud.yaml and the player's
 * config.yaml. The shipped file turns the readout off.
 * The Video tab of OpenXWA Options only turns it on or off. */

typedef enum XwaTargetReadoutTextSize {
	XWA_TARGET_READOUT_TEXT_SMALL = 0,
	XWA_TARGET_READOUT_TEXT_MEDIUM,
	XWA_TARGET_READOUT_TEXT_LARGE,
} XwaTargetReadoutTextSize;

typedef enum XwaTargetReadoutLayout {
	XWA_TARGET_READOUT_LAYOUT_VERTICAL = 0,
	XWA_TARGET_READOUT_LAYOUT_HORIZONTAL,
} XwaTargetReadoutLayout;

typedef enum XwaTargetReadoutNameColors {
	XWA_TARGET_READOUT_NAME_SINGLE = 0,
	XWA_TARGET_READOUT_NAME_CMD,
} XwaTargetReadoutNameColors;

/* Sizes are in points: 1/1080 of the flight view's height. Colours are
   linear. */
typedef struct XwaModernTargetReadoutOptions {
	int enabled;
	/* XwaTargetReadoutTextSize. */
	int text_size;
	/* The space between the last line of text and the top of the box. */
	float text_gap;
	int brighten_text;
	/* XwaTargetReadoutNameColors. */
	int name_colors;
	int bars;
	/* XwaTargetReadoutLayout. */
	int layout;
	/* A bar is 70% of the box width, kept between these two lengths. */
	float bar_length_min;
	float bar_length_max;
	float bar_thickness;
	/* The space between the box and the bars, and between two bars. */
	float bar_gap;
	/* The width of the bar outline. */
	float bar_frame;
	float shield_color[3];
	/* The colour of the shields above 100%. */
	float overshield_color[3];
	float hull_color_full[3];
	float hull_color_half[3];
	float hull_color_low[3];
	float system_color[3];
	/* 0 means no limit. */
	float component_box_min;
	float component_box_max;
	/* The selected component's box shrinks to 70% and jumps back, once a
	   second. */
	int component_pulse;
	/* Hides the readout where the 3D scene is closer than this many metres.
	   0 turns this off. */
	float hidden_within_m;
} XwaModernTargetReadoutOptions;

XwaOptionSection* XwaModernTargetReadoutOptions_Section(void);
/* Returns the values in use. When bar_length_max is below bar_length_min, it
   is set to bar_length_min. The same goes for component_box_max, unless one
   of the two component box sizes is 0. */
void XwaModernTargetReadoutOptions_Get(XwaModernTargetReadoutOptions* out);

#ifdef __cplusplus
}
#endif

#endif
