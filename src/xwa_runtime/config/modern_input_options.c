#include "xwa_runtime/config/modern_input_options.h"

#include "aeron/log.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

static struct {
	XwaModernInputOptions defaults;
	XwaModernInputOptions options;
	XwaModernInputOptionsApplyFn apply;
	XwaModernInputOptionsPersistFn persist;
	int configured;
	int dirty;
} g_modernInputOptions;

static int XwaControllerProfile_HasUniqueSources(const XwaControllerProfile* profile) {
	int i;
	int j;

	for (i = 0; i < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++i) {
		if (profile->axes[i].source < 0) {
			continue;
		}
		for (j = i + 1; j < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++j) {
			if (profile->axes[i].source == profile->axes[j].source) {
				return 0;
			}
		}
	}
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++i) {
		if (profile->buttons[i] < 0) {
			continue;
		}
		for (j = i + 1; j < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++j) {
			if (profile->buttons[i] == profile->buttons[j]) {
				return 0;
			}
		}
	}
	return 1;
}

static int XwaControllerProfile_IsValid(const XwaControllerProfile* profile, int axis_limit, int button_limit,
										int pov_min, int pov_max) {
	int i;

	if (!profile || profile->pov_source < pov_min || profile->pov_source > pov_max) {
		return 0;
	}
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++i) {
		if (profile->axes[i].source < -1 || profile->axes[i].source >= axis_limit ||
			!isfinite(profile->axes[i].deadzone) || profile->axes[i].deadzone < 0.0f ||
			profile->axes[i].deadzone > 1.0f) {
			return 0;
		}
	}
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++i) {
		if (profile->buttons[i] < -1 || profile->buttons[i] >= button_limit) {
			return 0;
		}
	}
	return XwaControllerProfile_HasUniqueSources(profile);
}

static int XwaControllerGamepadPovIsUnambiguous(const XwaControllerProfile* profile) {
	int i;

	if (!profile->pov_source) {
		return 1;
	}
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++i) {
		if (profile->buttons[i] >= AERON_GAMEPAD_BUTTON_DPAD_UP &&
			profile->buttons[i] <= AERON_GAMEPAD_BUTTON_DPAD_RIGHT) {
			return 0;
		}
	}
	return 1;
}

int XwaModernInputOptions_Validate(const XwaModernInputOptions* options) {
	size_t guid_length;
	size_t i;

	if (!options || options->mouse_sensitivity < XWA_MODERN_MOUSE_SENSITIVITY_MIN ||
		options->mouse_sensitivity > XWA_MODERN_MOUSE_SENSITIVITY_MAX ||
		options->mouse_mode < XWA_MODERN_MOUSE_MODE_POSITION ||
		options->mouse_mode > XWA_MODERN_MOUSE_MODE_RATE ||
		options->controller.rumble_strength < XWA_CONTROLLER_RUMBLE_STRENGTH_MIN ||
		options->controller.rumble_strength > XWA_CONTROLLER_RUMBLE_STRENGTH_MAX ||
		options->controller.device.ordinal < 0 ||
		options->controller.device.ordinal > XWA_CONTROLLER_DEVICE_ORDINAL_MAX ||
		!memchr(options->controller.device.guid, '\0', sizeof(options->controller.device.guid)) ||
		!memchr(options->controller.device.path, '\0', sizeof(options->controller.device.path))) {
		return 0;
	}
	guid_length = strlen(options->controller.device.guid);
	if (guid_length != 0 && guid_length != 32) {
		return 0;
	}
	if (guid_length == 0 &&
		(options->controller.device.path[0] != '\0' || options->controller.device.ordinal != 0)) {
		return 0;
	}
	for (i = 0; i < guid_length; ++i) {
		if (!isxdigit((unsigned char)options->controller.device.guid[i])) {
			return 0;
		}
	}
	return XwaControllerProfile_IsValid(&options->controller.gamepad, AERON_GAMEPAD_AXIS_COUNT,
										AERON_GAMEPAD_BUTTON_COUNT, 0, 1) &&
		   XwaControllerGamepadPovIsUnambiguous(&options->controller.gamepad) &&
		   XwaControllerProfile_IsValid(&options->controller.joystick, AERON_CONTROLLER_AXIS_MAX,
										AERON_CONTROLLER_BUTTON_MAX, -1, AERON_CONTROLLER_HAT_MAX - 1);
}

static void XwaModernInputOptions_Normalize(XwaModernInputOptions* options) {
	int i;

	options->mouse_flight_enabled = options->mouse_flight_enabled != 0;
	options->mouse_invert_y = options->mouse_invert_y != 0;
	options->controller.roll_enabled = options->controller.roll_enabled != 0;
	options->controller.rumble_enabled = options->controller.rumble_enabled != 0;
	options->controller.gamepad.pov_source = options->controller.gamepad.pov_source != 0;
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++i) {
		options->controller.gamepad.axes[i].invert = options->controller.gamepad.axes[i].invert != 0;
		options->controller.joystick.axes[i].invert = options->controller.joystick.axes[i].invert != 0;
	}
}

static int XwaControllerProfile_AreEqual(const XwaControllerProfile* lhs, const XwaControllerProfile* rhs) {
	int i;

	if (lhs->pov_source != rhs->pov_source) {
		return 0;
	}
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++i) {
		if (lhs->axes[i].source != rhs->axes[i].source || lhs->axes[i].invert != rhs->axes[i].invert ||
			lhs->axes[i].deadzone != rhs->axes[i].deadzone) {
			return 0;
		}
	}
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++i) {
		if (lhs->buttons[i] != rhs->buttons[i]) {
			return 0;
		}
	}
	return memcmp(lhs->actions, rhs->actions, sizeof(lhs->actions)) == 0;
}

static int XwaControllerOptions_AreEqual(const XwaControllerOptions* lhs, const XwaControllerOptions* rhs) {
	return strcmp(lhs->device.guid, rhs->device.guid) == 0 &&
		   strcmp(lhs->device.path, rhs->device.path) == 0 && lhs->device.ordinal == rhs->device.ordinal &&
		   lhs->roll_enabled == rhs->roll_enabled && lhs->rumble_enabled == rhs->rumble_enabled &&
		   lhs->rumble_strength == rhs->rumble_strength &&
		   XwaControllerProfile_AreEqual(&lhs->gamepad, &rhs->gamepad) &&
		   XwaControllerProfile_AreEqual(&lhs->joystick, &rhs->joystick);
}

static int XwaModernInputOptions_AreEqual(const XwaModernInputOptions* lhs,
										  const XwaModernInputOptions* rhs) {
	return lhs->mouse_flight_enabled == rhs->mouse_flight_enabled && lhs->mouse_mode == rhs->mouse_mode &&
		   lhs->mouse_sensitivity == rhs->mouse_sensitivity && lhs->mouse_invert_y == rhs->mouse_invert_y &&
		   XwaControllerOptions_AreEqual(&lhs->controller, &rhs->controller);
}

void XwaModernInputOptions_Configure(const XwaModernInputOptions* defaults,
									 const XwaModernInputOptions* options, XwaModernInputOptionsApplyFn apply,
									 XwaModernInputOptionsPersistFn persist) {
	memset(&g_modernInputOptions, 0, sizeof(g_modernInputOptions));
	if (!XwaModernInputOptions_Validate(defaults) || !XwaModernInputOptions_Validate(options)) {
		Aeron_LogError("xwa.config", "cannot configure invalid modern input options");
		return;
	}

	g_modernInputOptions.defaults = *defaults;
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

int XwaModernInputOptions_Set(const XwaModernInputOptions* options) {
	XwaModernInputOptions normalized;

	if (!g_modernInputOptions.configured || !XwaModernInputOptions_Validate(options)) {
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

int XwaModernInputOptions_RestoreControllerDefaults(void) {
	XwaModernInputOptions options;

	if (!g_modernInputOptions.configured) {
		return 0;
	}
	options = g_modernInputOptions.options;
	options.controller = g_modernInputOptions.defaults.controller;
	return XwaModernInputOptions_Set(&options);
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
