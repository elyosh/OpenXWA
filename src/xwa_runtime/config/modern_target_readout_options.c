#include "xwa_runtime/config/modern_target_readout_options.h"

#include "aeron/log.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char* const g_textSizeWords[] = { "small", "medium", "large", NULL };
static const char* const g_nameColorWords[] = { "single", "cmd", NULL };
static const char* const g_layoutWords[] = { "vertical", "horizontal", NULL };

#define READOUT_OFFSET(field) offsetof(XwaModernTargetReadoutOptions, field)
#define READOUT_SWITCH(field)                                                                                \
	{ #field, XWA_TARGET_READOUT_KEY_SWITCH, 0.0f, 1.0f, NULL, READOUT_OFFSET(field) }
#define READOUT_NUMBER(field, min, max)                                                                      \
	{ #field, XWA_TARGET_READOUT_KEY_NUMBER, min, max, NULL, READOUT_OFFSET(field) }
#define READOUT_COLOR(field) { #field, XWA_TARGET_READOUT_KEY_COLOR, 0.0f, 0.0f, NULL, READOUT_OFFSET(field) }
#define READOUT_CHOICE(field, words)                                                                         \
	{ #field, XWA_TARGET_READOUT_KEY_CHOICE, 0.0f, 0.0f, words, READOUT_OFFSET(field) }

static const XwaModernTargetReadoutOptionKey g_targetReadoutKeys[] = {
	READOUT_SWITCH(enabled),
	READOUT_CHOICE(text_size, g_textSizeWords),
	READOUT_NUMBER(text_gap, 0.0f, 100.0f),
	READOUT_SWITCH(brighten_text),
	READOUT_CHOICE(name_colors, g_nameColorWords),
	READOUT_SWITCH(bars),
	READOUT_CHOICE(layout, g_layoutWords),
	READOUT_NUMBER(bar_length_min, 1.0f, 1000.0f),
	READOUT_NUMBER(bar_length_max, 1.0f, 1000.0f),
	READOUT_NUMBER(bar_thickness, 1.0f, 50.0f),
	READOUT_NUMBER(bar_gap, 0.0f, 100.0f),
	READOUT_NUMBER(bar_frame, 0.0f, 10.0f),
	READOUT_COLOR(shield_color),
	READOUT_COLOR(overshield_color),
	READOUT_COLOR(hull_color_full),
	READOUT_COLOR(hull_color_half),
	READOUT_COLOR(hull_color_low),
	READOUT_COLOR(system_color),
	READOUT_NUMBER(component_box_min, 0.0f, 1000.0f),
	READOUT_NUMBER(component_box_max, 0.0f, 1000.0f),
	READOUT_SWITCH(component_pulse),
	READOUT_NUMBER(hidden_within_m, 0.0f, 1000.0f),
};

#define READOUT_KEY_COUNT (sizeof g_targetReadoutKeys / sizeof g_targetReadoutKeys[0])

/* Fails to compile when the keys don't fit in an override mask. */
typedef char ReadoutKeysFitTheMask[READOUT_KEY_COUNT <= sizeof(unsigned int) * 8 ? 1 : -1];

static struct {
	XwaModernTargetReadoutOptions shipped;
	XwaModernTargetReadoutOptions mod;
	XwaModernTargetReadoutOptions player;
	unsigned int override_mask;
	XwaModernTargetReadoutOptionsPersistFn persist;
	int has_mod;
	int configured;
	int dirty;
} g_modernTargetReadoutOptions;

const XwaModernTargetReadoutOptionKey* XwaModernTargetReadoutOptions_Keys(size_t* count) {
	if (count) {
		*count = READOUT_KEY_COUNT;
	}
	return g_targetReadoutKeys;
}

unsigned int XwaModernTargetReadoutOptions_EnabledBit(void) { return 1u; }

static size_t target_readout_field_size(const XwaModernTargetReadoutOptionKey* key) {
	switch (key->kind) {
		case XWA_TARGET_READOUT_KEY_NUMBER:
			return sizeof(float);
		case XWA_TARGET_READOUT_KEY_COLOR:
			return 3 * sizeof(float);
		case XWA_TARGET_READOUT_KEY_SWITCH:
		case XWA_TARGET_READOUT_KEY_CHOICE:
		default:
			return sizeof(int);
	}
}

static int target_readout_is_number(const AeronConfigNode* node) {
	const AeronConfigNodeType type = AeronConfigNode_Type(node);
	return type == AERON_CONFIG_INT || type == AERON_CONFIG_FLOAT;
}

/* Writes the choice's words as "a, b or c". */
static void target_readout_choice_words(const char* const* words, char* out, size_t out_size) {
	size_t count = 0;
	size_t used = 0;
	size_t i;

	while (words[count]) {
		++count;
	}
	out[0] = '\0';
	for (i = 0; i < count && used < out_size; ++i) {
		const char* separator = i == 0 ? "" : (i + 1 == count ? " or " : ", ");
		const int written = snprintf(out + used, out_size - used, "%s%s", separator, words[i]);
		if (written < 0) {
			return;
		}
		used += (size_t)written;
	}
}

int XwaModernTargetReadoutOptions_ReadKey(const XwaModernTargetReadoutOptionKey* key,
										  const AeronConfigNode* node, XwaModernTargetReadoutOptions* out,
										  char* expected, size_t expected_size) {
	char* field;
	char unused[1];

	if (!expected || expected_size == 0) {
		expected = unused;
		expected_size = sizeof unused;
	}
	expected[0] = '\0';
	if (!key || !out) {
		return 0;
	}
	field = (char*)out + key->offset;
	switch (key->kind) {
		case XWA_TARGET_READOUT_KEY_SWITCH: {
			int value;

			if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
				snprintf(expected, expected_size, "true or false");
				return 0;
			}
			value = AeronConfigNode_Bool(node, 0) != 0;
			memcpy(field, &value, sizeof value);
			return 1;
		}
		case XWA_TARGET_READOUT_KEY_NUMBER: {
			const double number = AeronConfigNode_Float(node, NAN);
			float value;

			if (!target_readout_is_number(node) || !(number >= key->min && number <= key->max)) {
				snprintf(expected, expected_size, "a number from %g to %g", (double)key->min,
						 (double)key->max);
				return 0;
			}
			value = (float)number;
			memcpy(field, &value, sizeof value);
			return 1;
		}
		case XWA_TARGET_READOUT_KEY_COLOR: {
			float value[3];
			size_t i;

			if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE ||
				AeronConfigNode_SequenceCount(node) != 3) {
				snprintf(expected, expected_size, "three numbers of 0 or more");
				return 0;
			}
			for (i = 0; i < 3; ++i) {
				const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, i);
				const double number = AeronConfigNode_Float(item, NAN);

				if (!target_readout_is_number(item) || !isfinite(number) || !(number >= 0.0)) {
					snprintf(expected, expected_size, "three numbers of 0 or more");
					return 0;
				}
				value[i] = (float)number;
			}
			memcpy(field, value, sizeof value);
			return 1;
		}
		case XWA_TARGET_READOUT_KEY_CHOICE: {
			const char* word =
				AeronConfigNode_Type(node) == AERON_CONFIG_STRING ? AeronConfigNode_String(node, NULL) : NULL;
			int i;

			for (i = 0; word && key->choices[i]; ++i) {
				if (strcmp(word, key->choices[i]) == 0) {
					memcpy(field, &i, sizeof i);
					return 1;
				}
			}
			target_readout_choice_words(key->choices, expected, expected_size);
			return 0;
		}
	}
	return 0;
}

/* The values in use, before the minimum and maximum checks. */
static void target_readout_combine(XwaModernTargetReadoutOptions* out) {
	size_t i;

	*out = g_modernTargetReadoutOptions.has_mod ? g_modernTargetReadoutOptions.mod
												: g_modernTargetReadoutOptions.shipped;
	for (i = 0; i < READOUT_KEY_COUNT; ++i) {
		if (g_modernTargetReadoutOptions.override_mask & (1u << i)) {
			const size_t offset = g_targetReadoutKeys[i].offset;
			memcpy((char*)out + offset, (const char*)&g_modernTargetReadoutOptions.player + offset,
				   target_readout_field_size(&g_targetReadoutKeys[i]));
		}
	}
}

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

/* Logs a warning for each limit that Get replaces. */
static void target_readout_check_limits(void) {
	XwaModernTargetReadoutOptions options;

	target_readout_combine(&options);
	target_readout_fix_limits(&options, 1);
}

void XwaModernTargetReadoutOptions_Configure(const XwaModernTargetReadoutOptions* shipped,
											 const XwaModernTargetReadoutOptions* player,
											 unsigned int override_mask,
											 XwaModernTargetReadoutOptionsPersistFn persist) {
	memset(&g_modernTargetReadoutOptions, 0, sizeof g_modernTargetReadoutOptions);
	if (!shipped || !player) {
		Aeron_LogError("xwa.config", "cannot configure the target readout options without values");
		return;
	}
	g_modernTargetReadoutOptions.shipped = *shipped;
	g_modernTargetReadoutOptions.player = *player;
	g_modernTargetReadoutOptions.override_mask = override_mask;
	g_modernTargetReadoutOptions.persist = persist;
	g_modernTargetReadoutOptions.configured = 1;
	target_readout_check_limits();
}

void XwaModernTargetReadoutOptions_GetShipped(XwaModernTargetReadoutOptions* out) {
	if (out) {
		*out = g_modernTargetReadoutOptions.shipped;
	}
}

void XwaModernTargetReadoutOptions_SetMod(const XwaModernTargetReadoutOptions* options) {
	if (options) {
		g_modernTargetReadoutOptions.mod = *options;
		g_modernTargetReadoutOptions.has_mod = 1;
		target_readout_check_limits();
	}
}

void XwaModernTargetReadoutOptions_Get(XwaModernTargetReadoutOptions* out) {
	if (!out) {
		return;
	}
	target_readout_combine(out);
	target_readout_fix_limits(out, 0);
}

void XwaModernTargetReadoutOptions_SetEnabled(int enabled) {
	XwaModernTargetReadoutOptions current;

	if (!g_modernTargetReadoutOptions.configured) {
		return;
	}
	XwaModernTargetReadoutOptions_Get(&current);
	if (current.enabled == (enabled != 0)) {
		return;
	}
	g_modernTargetReadoutOptions.player.enabled = enabled != 0;
	g_modernTargetReadoutOptions.override_mask |= XwaModernTargetReadoutOptions_EnabledBit();
	g_modernTargetReadoutOptions.dirty = 1;
}

int XwaModernTargetReadoutOptions_Flush(void) {
	char error[512];

	if (!g_modernTargetReadoutOptions.configured || !g_modernTargetReadoutOptions.dirty) {
		return 1;
	}
	if (!g_modernTargetReadoutOptions.persist) {
		Aeron_LogWarn("xwa.config", "the target readout switch changed, but nothing saves it");
		return 0;
	}
	error[0] = '\0';
	if (!g_modernTargetReadoutOptions.persist(g_modernTargetReadoutOptions.player.enabled, error,
											  sizeof error)) {
		Aeron_LogError("xwa.config", "%s",
					   error[0] ? error : "could not save the target readout switch to config.yaml");
		return 0;
	}
	g_modernTargetReadoutOptions.dirty = 0;
	return 1;
}

int XwaModernTargetReadoutOptions_IsDirty(void) { return g_modernTargetReadoutOptions.dirty; }
