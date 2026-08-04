#include "xwa_runtime/input/controller_mapping.h"

#include "aeron/aeron.h"

#include <string.h>

enum {
	CONTROLLER_AXIS_RANGE = 65535,
	CONTROLLER_AXIS_CENTER = 32768,
};

typedef struct XwaControllerMappingState {
	XwaControllerOptions options;
	int configured;
	uint32_t previous_instance_id;
	uint32_t digital_axis_buttons;
} XwaControllerMappingState;

static XwaControllerMappingState g_controllerMapping;

static int ControllerMapping_ProfileEqual(const XwaControllerProfile* lhs, const XwaControllerProfile* rhs) {
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
		if (lhs->buttons[i].kind != rhs->buttons[i].kind || lhs->buttons[i].index != rhs->buttons[i].index ||
			lhs->buttons[i].threshold != rhs->buttons[i].threshold) {
			return 0;
		}
	}
	return memcmp(lhs->actions, rhs->actions, sizeof(lhs->actions)) == 0;
}

static int ControllerMapping_OptionsEqual(const XwaControllerOptions* lhs, const XwaControllerOptions* rhs) {
	return strcmp(lhs->device.guid, rhs->device.guid) == 0 &&
		   strcmp(lhs->device.path, rhs->device.path) == 0 && lhs->device.ordinal == rhs->device.ordinal &&
		   lhs->rumble_enabled == rhs->rumble_enabled &&
		   ControllerMapping_ProfileEqual(&lhs->gamepad, &rhs->gamepad) &&
		   ControllerMapping_ProfileEqual(&lhs->joystick, &rhs->joystick);
}

const AeronControllerSnapshot* XwaControllerMapping_SelectedController(void) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();

	if (!input || !g_controllerMapping.configured) {
		return NULL;
	}
	return Aeron_SelectController(input, &g_controllerMapping.options.device);
}

static const XwaControllerProfile* ControllerMapping_Profile(const XwaControllerOptions* options,
															 const AeronControllerSnapshot* controller) {
	return controller->kind == AERON_CONTROLLER_KIND_GAMEPAD ? &options->gamepad : &options->joystick;
}

static int16_t ControllerMapping_AxisValue(const AeronControllerSnapshot* controller, int source) {
	if (source < 0) {
		return 0;
	}
	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		return source < AERON_GAMEPAD_AXIS_COUNT ? controller->gamepad_axes[source] : 0;
	}
	return source < controller->axis_count && source < AERON_CONTROLLER_AXIS_MAX
			   ? controller->raw_axes[source]
			   : 0;
}

static int ControllerMapping_IsTrigger(const AeronControllerSnapshot* controller, int source) {
	return controller->kind == AERON_CONTROLLER_KIND_GAMEPAD &&
		   (source == AERON_GAMEPAD_AXIS_LEFT_TRIGGER || source == AERON_GAMEPAD_AXIS_RIGHT_TRIGGER);
}

static uint32_t ControllerMapping_CenteredAxis(int16_t value, int invert, float deadzone) {
	double normalized = value < 0 ? (double)value / 32768.0 : (double)value / 32767.0;
	double magnitude;
	uint32_t mapped;

	magnitude = normalized < 0.0 ? -normalized : normalized;
	if (magnitude <= deadzone) {
		return CONTROLLER_AXIS_CENTER;
	}
	/* Shift SDL's complete signed range onto the complete WinMM range without
	 * losing the negative endpoint. */
	mapped = (uint32_t)((int32_t)value + CONTROLLER_AXIS_CENTER);
	return invert ? CONTROLLER_AXIS_RANGE - mapped : mapped;
}

static uint32_t ControllerMapping_TriggerAxis(int16_t value, int invert, float deadzone) {
	double normalized = (double)value / 32767.0;

	if (normalized < 0.0) {
		normalized = 0.0;
	} else if (normalized > 1.0) {
		normalized = 1.0;
	}
	if (normalized <= deadzone) {
		normalized = 0.0;
	}
	if (invert) {
		normalized = 1.0 - normalized;
	}
	return (uint32_t)(normalized * CONTROLLER_AXIS_RANGE);
}

static uint8_t ControllerMapping_Hat(const AeronControllerSnapshot* controller,
									 const XwaControllerProfile* profile) {
	uint8_t hat = AERON_CONTROLLER_HAT_CENTERED;

	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		if (!profile->pov_source) {
			return hat;
		}
		if (controller->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_UP)) {
			hat |= AERON_CONTROLLER_HAT_UP;
		}
		if (controller->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_RIGHT)) {
			hat |= AERON_CONTROLLER_HAT_RIGHT;
		}
		if (controller->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_DOWN)) {
			hat |= AERON_CONTROLLER_HAT_DOWN;
		}
		if (controller->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_LEFT)) {
			hat |= AERON_CONTROLLER_HAT_LEFT;
		}
		return hat;
	}
	if (profile->pov_source >= 0 && profile->pov_source < controller->hat_count &&
		profile->pov_source < AERON_CONTROLLER_HAT_MAX) {
		return controller->raw_hats[profile->pov_source];
	}
	return hat;
}

static int ControllerMapping_PovDirection(uint8_t hat) {
	/* XWA has four POV actions. Vertical wins for diagonal hats. */
	if (hat & AERON_CONTROLLER_HAT_UP) {
		return 0;
	}
	if (hat & AERON_CONTROLLER_HAT_DOWN) {
		return 2;
	}
	if (hat & AERON_CONTROLLER_HAT_RIGHT) {
		return 1;
	}
	if (hat & AERON_CONTROLLER_HAT_LEFT) {
		return 3;
	}
	return -1;
}

static int ControllerMapping_HasPov(const AeronControllerSnapshot* controller,
									const XwaControllerProfile* profile) {
	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		return profile->pov_source != 0;
	}
	return profile->pov_source >= 0 && profile->pov_source < controller->hat_count;
}

static void ControllerMapping_LogUnavailableSources(const AeronControllerSnapshot* controller) {
	const XwaControllerProfile* profile;
	int unavailable_axes = 0;
	int unavailable_buttons = 0;
	int unavailable_pov = 0;
	int i;

	if (!controller || controller->kind != AERON_CONTROLLER_KIND_JOYSTICK) {
		return;
	}
	profile = &g_controllerMapping.options.joystick;
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++i) {
		unavailable_axes += profile->axes[i].source >= controller->axis_count;
	}
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++i) {
		const AeronControllerDigitalSource* binding = &profile->buttons[i];
		if (binding->kind == AERON_CONTROLLER_DIGITAL_BUTTON) {
			unavailable_buttons += binding->index >= controller->button_count;
		} else if (binding->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE ||
				   binding->kind == AERON_CONTROLLER_DIGITAL_AXIS_NEGATIVE) {
			unavailable_axes += binding->index >= controller->axis_count;
		}
	}
	unavailable_pov = profile->pov_source >= controller->hat_count;
	if (unavailable_axes || unavailable_buttons || unavailable_pov) {
		Aeron_LogWarn("xwa.input",
					  "Controller '%s' mapping references unavailable controls (%d axes, %d buttons, "
					  "invalid POV=%s)",
					  controller->name, unavailable_axes, unavailable_buttons,
					  unavailable_pov ? "yes" : "no");
	}
}

static void ControllerMapping_MapSnapshot(const XwaControllerOptions* options,
										  const AeronControllerSnapshot* controller, int has_focus,
										  uint32_t previous_axis_buttons, uint32_t* axis_buttons,
										  XwaControllerLogicalState* state) {
	const XwaControllerProfile* profile;
	int logical;

	if (axis_buttons) {
		*axis_buttons = 0;
	}
	if (!state) {
		return;
	}
	memset(state, 0, sizeof(*state));
	state->pov_direction = -1;
	for (logical = 0; logical < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++logical) {
		state->axes[logical] = CONTROLLER_AXIS_CENTER;
		state->source_axes[logical] = -1;
	}
	if (!options || !controller || !controller->connected) {
		return;
	}
	profile = ControllerMapping_Profile(options, controller);
	state->has_pov = ControllerMapping_HasPov(controller, profile);
	for (logical = 0; logical < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++logical) {
		const XwaControllerAxisBinding* binding = &profile->axes[logical];
		const int16_t value = ControllerMapping_AxisValue(controller, binding->source);
		state->source_axes[logical] = (int8_t)binding->source;
		state->source_axis_values[logical] = value;
		if (!has_focus) {
			continue;
		}
		state->axes[logical] =
			ControllerMapping_IsTrigger(controller, binding->source)
				? ControllerMapping_TriggerAxis(value, binding->invert, binding->deadzone)
				: ControllerMapping_CenteredAxis(value, binding->invert, binding->deadzone);
	}
	if (!has_focus) {
		return;
	}
	for (logical = 0; logical < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++logical) {
		const AeronControllerDigitalSource* binding = &profile->buttons[logical];
		const uint32_t bit = 1u << logical;
		if (Aeron_ControllerDigitalSourceDown(controller, binding, (previous_axis_buttons & bit) != 0)) {
			state->buttons |= 1u << logical;
			if (axis_buttons && binding->kind != AERON_CONTROLLER_DIGITAL_BUTTON) {
				*axis_buttons |= bit;
			}
		}
	}
	state->pov_direction = ControllerMapping_PovDirection(ControllerMapping_Hat(controller, profile));
}

void XwaControllerMapping_MapSnapshot(const XwaControllerOptions* options,
									  const AeronControllerSnapshot* controller, int has_focus,
									  XwaControllerLogicalState* state) {
	ControllerMapping_MapSnapshot(options, controller, has_focus, 0, NULL, state);
}

void XwaControllerMapping_SetOptions(const XwaControllerOptions* options) {
	const uint32_t old_instance_id = XwaControllerMapping_SelectedInstanceId();
	const uint32_t previous_instance_id = g_controllerMapping.previous_instance_id;

	if (options && g_controllerMapping.configured &&
		ControllerMapping_OptionsEqual(options, &g_controllerMapping.options)) {
		return;
	}
	if (old_instance_id != 0) {
		Aeron_RumbleController(old_instance_id, 0, 0, 0);
	}
	memset(&g_controllerMapping, 0, sizeof(g_controllerMapping));
	/* Option changes must not masquerade as physical device changes. */
	g_controllerMapping.previous_instance_id = previous_instance_id;
	if (!options) {
		return;
	}
	g_controllerMapping.options = *options;
	g_controllerMapping.configured = 1;
	if ((options->device.guid[0] || options->device.path[0]) && !XwaControllerMapping_SelectedController()) {
		Aeron_LogWarn("xwa.input", "Configured controller is unavailable (GUID '%s', path '%s', ordinal %d)",
					  options->device.guid, options->device.path, options->device.ordinal);
	}
}

uint32_t XwaControllerMapping_SelectedInstanceId(void) {
	const AeronControllerSnapshot* controller = XwaControllerMapping_SelectedController();
	return controller ? controller->instance_id : 0;
}

int XwaControllerMapping_SelectedHasRumble(void) {
	const AeronControllerSnapshot* controller = XwaControllerMapping_SelectedController();
	return controller && controller->has_rumble;
}

int XwaControllerMapping_Rumble(uint16_t low_frequency_rumble, uint16_t high_frequency_rumble,
								uint32_t duration_ms) {
	const AeronControllerSnapshot* controller = XwaControllerMapping_SelectedController();
	const int stopping = (low_frequency_rumble == 0 && high_frequency_rumble == 0) || duration_ms == 0;

	if (!controller || !controller->has_rumble) {
		if (!stopping) {
			Aeron_LogWarn("xwa.input", "Rumble requested while the selected controller is %s",
						  controller ? "unsupported" : "disconnected");
		}
		return 0;
	}
	if (!stopping && !g_controllerMapping.options.rumble_enabled) {
		return 0;
	}
	return Aeron_RumbleController(controller->instance_id, low_frequency_rumble, high_frequency_rumble,
								  duration_ms);
}

int XwaControllerMapping_ConsumeSelectionChange(void) {
	const uint32_t instance_id = XwaControllerMapping_SelectedInstanceId();
	if (instance_id == g_controllerMapping.previous_instance_id) {
		return 0;
	}
	if (g_controllerMapping.previous_instance_id != 0) {
		Aeron_RumbleController(g_controllerMapping.previous_instance_id, 0, 0, 0);
	}
	Aeron_LogInfo("xwa.input", "Active controller changed from %u to %u",
				  g_controllerMapping.previous_instance_id, instance_id);
	g_controllerMapping.previous_instance_id = instance_id;
	g_controllerMapping.digital_axis_buttons = 0;
	ControllerMapping_LogUnavailableSources(XwaControllerMapping_SelectedController());
	return 1;
}

int XwaControllerMapping_GetState(XwaControllerLogicalState* state) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	const AeronControllerSnapshot* controller = XwaControllerMapping_SelectedController();

	ControllerMapping_MapSnapshot(&g_controllerMapping.options, controller, input && input->has_focus,
								  g_controllerMapping.digital_axis_buttons,
								  &g_controllerMapping.digital_axis_buttons, state);
	return controller != NULL;
}

void XwaControllerMapping_CopySelectedActions(uint16_t actions[XWA_CONTROLLER_ACTION_COUNT]) {
	const AeronControllerSnapshot* controller;
	const XwaControllerProfile* profile;

	if (!actions) {
		return;
	}
	controller = XwaControllerMapping_SelectedController();
	profile = controller && controller->kind == AERON_CONTROLLER_KIND_JOYSTICK
				  ? &g_controllerMapping.options.joystick
				  : &g_controllerMapping.options.gamepad;
	memcpy(actions, profile->actions, sizeof(profile->actions));
}
