#ifndef XWA_RUNTIME_MODERN_TARGET_READOUT_OPTIONS_H
#define XWA_RUNTIME_MODERN_TARGET_READOUT_OPTIONS_H

#include "aeron/config_file.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The target readout options. The target readout shows the target's name,
 * cargo, distance and damage bars next to its box in the 3D view. The options
 * come from three places, and each one overrides the one before it:
 *   1. the `target_readout` section of the shipped remaster/config.yaml. It
 *      has every value, and the feature is off;
 *   2. the `target_readout` section of the mod's flight/hud.yaml;
 *   3. the `target_readout` section of the player's config.yaml.
 * The options screen only turns the feature on or off. */

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

typedef enum XwaModernTargetReadoutKeyKind {
	/* true or false, stored as an int. */
	XWA_TARGET_READOUT_KEY_SWITCH = 0,
	/* A float from `min` to `max`. */
	XWA_TARGET_READOUT_KEY_NUMBER,
	/* Three floats of 0 or more. */
	XWA_TARGET_READOUT_KEY_COLOR,
	/* One of the words in `choices`, stored as its index in an int. */
	XWA_TARGET_READOUT_KEY_CHOICE,
} XwaModernTargetReadoutKeyKind;

/* A key of a `target_readout` section. `offset` is the field's offset in
   XwaModernTargetReadoutOptions. */
typedef struct XwaModernTargetReadoutOptionKey {
	const char* name;
	XwaModernTargetReadoutKeyKind kind;
	float min;
	float max;
	/* For a choice: the words, ending with NULL. */
	const char* const* choices;
	size_t offset;
} XwaModernTargetReadoutOptionKey;

/* The keys, in the order of the shipped file. In an override mask, bit `i`
   stands for key `i`. */
const XwaModernTargetReadoutOptionKey* XwaModernTargetReadoutOptions_Keys(size_t* count);
unsigned int XwaModernTargetReadoutOptions_EnabledBit(void);
/* Sets the key's field in `out` from `node`. When the value is wrong, it
   returns 0, leaves `out` unchanged and writes what the key expects in
   `expected`, such as "true or false". */
int XwaModernTargetReadoutOptions_ReadKey(const XwaModernTargetReadoutOptionKey* key,
										  const AeronConfigNode* node, XwaModernTargetReadoutOptions* out,
										  char* expected, size_t expected_size);

typedef int (*XwaModernTargetReadoutOptionsPersistFn)(int enabled, char* error, size_t error_size);

/* Only the keys in `override_mask` are taken from `player`. */
void XwaModernTargetReadoutOptions_Configure(const XwaModernTargetReadoutOptions* shipped,
											 const XwaModernTargetReadoutOptions* player,
											 unsigned int override_mask,
											 XwaModernTargetReadoutOptionsPersistFn persist);
void XwaModernTargetReadoutOptions_GetShipped(XwaModernTargetReadoutOptions* out);
/* Replaces the shipped values with the mod's. */
void XwaModernTargetReadoutOptions_SetMod(const XwaModernTargetReadoutOptions* options);
/* Returns the values in use. They are the mod's values, or the shipped values
   when there is no mod. The keys that the player has set use the player's
   values. When bar_length_max is below bar_length_min, it is set to
   bar_length_min. The same goes for component_box_max, unless one of the
   two component box sizes is 0. */
void XwaModernTargetReadoutOptions_Get(XwaModernTargetReadoutOptions* out);
/* Turns the feature on or off for the player. Flush saves the change. */
void XwaModernTargetReadoutOptions_SetEnabled(int enabled);
int XwaModernTargetReadoutOptions_Flush(void);
int XwaModernTargetReadoutOptions_IsDirty(void);

#ifdef __cplusplus
}
#endif

#endif
