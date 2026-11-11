#include "xwa_runtime/config/modern_input_options.h"

#include "aeron/log.h"

#include <stdio.h>
#include <string.h>

static struct {
	XwaModernInputOptions defaults;
	XwaControllerProfile gamepad_defaults;
	XwaModernInputOptions options;
	XwaModernInputOptionsApplyFn apply;
	XwaModernInputOptionsPersistFn persist;
	int configured;
	int dirty;
} g_modernInputOptions;

static int XwaModernInputOptions_Error(char* error, size_t error_size, const char* message) {
	if (error && error_size) {
		snprintf(error, error_size, "%s", message);
	}
	return 0;
}

static int XwaModernInputOptions_ValidateKeyboard(const XwaKeyboardBindings* keyboard, char* error,
												  size_t error_size) {
	size_t i;

	if (keyboard->count > XWA_KEYBOARD_BINDING_CAP) {
		return XwaModernInputOptions_Error(error, error_size, "keyboard binding capacity exceeded");
	}
	for (i = 0; i < keyboard->count; ++i) {
		const XwaKeyboardBinding* binding = &keyboard->bindings[i];
		size_t prior;

		if (!XwaInputActions_KeyboardBindable(binding->action) ||
			!XwaKeyboardMapping_SourceValid(binding->source)) {
			return XwaModernInputOptions_Error(error, error_size, "keyboard binding is invalid");
		}
		for (prior = 0; prior < i; ++prior) {
			if (keyboard->bindings[prior].source.key == binding->source.key &&
				keyboard->bindings[prior].source.modifiers == binding->source.modifiers) {
				return XwaModernInputOptions_Error(error, error_size, "keyboard key is bound twice");
			}
		}
	}
	return 1;
}

int XwaModernInputOptions_Validate(const XwaModernInputOptions* options, char* error, size_t error_size) {
	if (!options || options->mouse_sensitivity < XWA_MODERN_MOUSE_SENSITIVITY_MIN ||
		options->mouse_sensitivity > XWA_MODERN_MOUSE_SENSITIVITY_MAX ||
		options->mouse_mode < XWA_MODERN_MOUSE_MODE_POSITION ||
		options->mouse_mode > XWA_MODERN_MOUSE_MODE_RATE ||
		options->rumble_strength < XWA_MODERN_RUMBLE_STRENGTH_MIN ||
		options->rumble_strength > XWA_MODERN_RUMBLE_STRENGTH_MAX) {
		return XwaModernInputOptions_Error(error, error_size, "mouse or rumble setting is invalid");
	}
	return XwaControllerOptions_Validate(&options->controllers, error, error_size) &&
		   XwaModernInputOptions_ValidateKeyboard(&options->keyboard, error, error_size);
}

static void XwaModernInputOptions_Normalize(XwaModernInputOptions* options) {
	options->mouse_flight_enabled = options->mouse_flight_enabled != 0;
	options->mouse_invert_y = options->mouse_invert_y != 0;
	options->rumble_enabled = options->rumble_enabled != 0;
	XwaKeyboardMapping_Sort(&options->keyboard);
}

int XwaModernInputOptions_Equal(const XwaModernInputOptions* a, const XwaModernInputOptions* b) {
	return a->mouse_flight_enabled == b->mouse_flight_enabled && a->mouse_mode == b->mouse_mode &&
		   a->mouse_sensitivity == b->mouse_sensitivity && a->mouse_invert_y == b->mouse_invert_y &&
		   a->rumble_enabled == b->rumble_enabled && a->rumble_strength == b->rumble_strength &&
		   XwaControllerOptions_Equals(&a->controllers, &b->controllers) &&
		   XwaKeyboardMapping_Equal(&a->keyboard, &b->keyboard);
}

void XwaModernInputOptions_Configure(const XwaModernInputOptions* defaults,
									 const XwaControllerProfile* gamepad_defaults,
									 const XwaModernInputOptions* options, XwaModernInputOptionsApplyFn apply,
									 XwaModernInputOptionsPersistFn persist) {
	char error[256];

	memset(&g_modernInputOptions, 0, sizeof(g_modernInputOptions));
	if (!XwaModernInputOptions_Validate(defaults, error, sizeof error) ||
		!XwaModernInputOptions_Validate(options, error, sizeof error) || !gamepad_defaults ||
		!XwaControllerOptions_ValidateProfile(gamepad_defaults, AERON_CONTROLLER_KIND_GAMEPAD, error,
											  sizeof error)) {
		Aeron_LogError("xwa.config", "cannot configure invalid modern input options: %s", error);
		return;
	}

	g_modernInputOptions.defaults = *defaults;
	g_modernInputOptions.gamepad_defaults = *gamepad_defaults;
	g_modernInputOptions.options = *options;
	XwaModernInputOptions_Normalize(&g_modernInputOptions.defaults);
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

void XwaModernInputOptions_GetDefaults(XwaModernInputOptions* out) {
	if (out) {
		*out = g_modernInputOptions.defaults;
	}
}

const XwaControllerProfile* XwaModernInputOptions_GamepadDefaults(void) {
	return &g_modernInputOptions.gamepad_defaults;
}

int XwaModernInputOptions_Set(const XwaModernInputOptions* options) {
	static XwaModernInputOptions normalized;
	char error[256];

	if (!g_modernInputOptions.configured) {
		return 0;
	}
	if (!XwaModernInputOptions_Validate(options, error, sizeof error)) {
		Aeron_LogWarn("xwa.config", "input options not changed: %s", error);
		return 0;
	}
	normalized = *options;
	XwaModernInputOptions_Normalize(&normalized);
	if (XwaModernInputOptions_Equal(&normalized, &g_modernInputOptions.options)) {
		return 1;
	}

	g_modernInputOptions.options = normalized;
	g_modernInputOptions.dirty = 1;
	if (g_modernInputOptions.apply) {
		g_modernInputOptions.apply(&g_modernInputOptions.options);
	}
	return 1;
}

void XwaModernInputOptions_MarkDirty(void) {
	if (g_modernInputOptions.configured) {
		g_modernInputOptions.dirty = 1;
	}
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
	if (!g_modernInputOptions.persist(&g_modernInputOptions.options, error, sizeof(error))) {
		Aeron_LogError("xwa.config", "%s",
					   error[0] ? error : "could not persist modern input options to user configuration");
		return 0;
	}

	g_modernInputOptions.dirty = 0;
	return 1;
}

int XwaModernInputOptions_IsDirty(void) { return g_modernInputOptions.dirty; }
