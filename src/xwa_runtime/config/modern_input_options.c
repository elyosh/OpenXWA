#include "xwa_runtime/config/modern_input_options.h"

#include "aeron/log.h"

#include <string.h>

static struct {
	XwaModernInputOptions options;
	XwaModernInputOptionsApplyFn apply;
	XwaModernInputOptionsPersistFn persist;
	int configured;
	int dirty;
} g_modernInputOptions;

static int XwaModernInputOptions_IsValid(const XwaModernInputOptions* options) {
	return options && options->mouse_sensitivity >= XWA_MODERN_MOUSE_SENSITIVITY_MIN &&
		   options->mouse_sensitivity <= XWA_MODERN_MOUSE_SENSITIVITY_MAX &&
		   options->mouse_mode >= XWA_MODERN_MOUSE_MODE_POSITION &&
		   options->mouse_mode <= XWA_MODERN_MOUSE_MODE_RATE;
}

static void XwaModernInputOptions_Normalize(XwaModernInputOptions* options) {
	options->mouse_flight_enabled = options->mouse_flight_enabled != 0;
	options->mouse_invert_y = options->mouse_invert_y != 0;
}

static int XwaModernInputOptions_AreEqual(const XwaModernInputOptions* lhs,
										  const XwaModernInputOptions* rhs) {
	return lhs->mouse_flight_enabled == rhs->mouse_flight_enabled && lhs->mouse_mode == rhs->mouse_mode &&
		   lhs->mouse_sensitivity == rhs->mouse_sensitivity && lhs->mouse_invert_y == rhs->mouse_invert_y;
}

void XwaModernInputOptions_Configure(const XwaModernInputOptions* options, XwaModernInputOptionsApplyFn apply,
									 XwaModernInputOptionsPersistFn persist) {
	memset(&g_modernInputOptions, 0, sizeof g_modernInputOptions);
	if (!XwaModernInputOptions_IsValid(options)) {
		Aeron_LogError("xwa.config", "cannot configure invalid modern input options");
		return;
	}

	g_modernInputOptions.options = *options;
	XwaModernInputOptions_Normalize(&g_modernInputOptions.options);
	g_modernInputOptions.apply = apply;
	g_modernInputOptions.persist = persist;
	g_modernInputOptions.configured = 1;
}

void XwaModernInputOptions_Get(XwaModernInputOptions* out) {
	if (out) {
		*out = g_modernInputOptions.options;
	}
}

int XwaModernInputOptions_Set(const XwaModernInputOptions* options) {
	XwaModernInputOptions normalized;

	if (!g_modernInputOptions.configured || !XwaModernInputOptions_IsValid(options)) {
		return 0;
	}
	normalized = *options;
	XwaModernInputOptions_Normalize(&normalized);
	if (XwaModernInputOptions_AreEqual(&normalized, &g_modernInputOptions.options)) {
		return 1;
	}

	g_modernInputOptions.options = normalized;
	g_modernInputOptions.dirty = 1;
	if (g_modernInputOptions.apply) {
		g_modernInputOptions.apply(&g_modernInputOptions.options);
	}
	return 1;
}

int XwaModernInputOptions_Flush(void) {
	char error[512];

	if (!g_modernInputOptions.configured || !g_modernInputOptions.dirty) {
		return 1;
	}
	if (!g_modernInputOptions.persist) {
		Aeron_LogWarn("xwa.config",
					  "modern input options are dirty but no persistence callback is registered");
		return 0;
	}
	error[0] = '\0';
	if (!g_modernInputOptions.persist(&g_modernInputOptions.options, error, sizeof error)) {
		Aeron_LogError("xwa.config", "%s",
					   error[0] ? error : "could not persist modern input options to user configuration");
		return 0;
	}

	g_modernInputOptions.dirty = 0;
	return 1;
}

int XwaModernInputOptions_IsDirty(void) { return g_modernInputOptions.dirty; }
