#include "xwa_runtime/config/modern_controller_options_screen.h"

#include "aeron/aeron.h"
#include "aeron/input.h"
#include "xwa/assets/string_table.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/config/modern_options_menu.h"
#include "xwa_runtime/input/controller_mapping.h"

#include <stdio.h>
#include <string.h>

enum {
	CONTROLLER_KEY_DELETE = 0x2e,
	CONTROLLER_CAPTURE_THRESHOLD = 8192,
	CONTROLLER_PAGE_SIZE = 8,
};

typedef struct ControllerCaptureState {
	int axis;
	int button;
	int digital_axis;
	int wait_for_release;
	int16_t axis_baseline[AERON_CONTROLLER_AXIS_MAX];
} ControllerCaptureState;

typedef struct ControllerBindingRow {
	AeronControllerDigitalSourceKind kind;
	int source;
	int logical_button;
	int pov_direction;
	int capture_axis;
} ControllerBindingRow;

typedef struct ControllerBindingEditState {
	int active;
	AeronControllerKind controller_kind;
	AeronControllerDigitalSourceKind kind;
	int source;
	int pov_direction;
	uint16_t action;
	char title[64];
} ControllerBindingEditState;

static ControllerCaptureState g_controllerCapture = { -1, -1, -1, 0, { 0 } };
static ControllerBindingEditState g_controllerBindingEdit;
static int g_controllerButtonPage;
static char g_controllerBindingMessage[96];
static int g_controllerBindingMessageTtl;

static int ControllerScreen_DeviceOrdinal(const AeronInputSnapshot* input, int slot) {
	int ordinal = 0;
	int i;

	for (i = 0; i < slot; ++i) {
		if (input->controllers[i].connected &&
			strcmp(input->controllers[i].guid, input->controllers[slot].guid) == 0 &&
			strcmp(input->controllers[i].path, input->controllers[slot].path) == 0) {
			++ordinal;
		}
	}
	return ordinal;
}

static int ControllerScreen_NameOrdinal(const AeronInputSnapshot* input, int slot, int* duplicate_count) {
	int ordinal = 0;
	int count = 0;
	int i;

	for (i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		if (!input->controllers[i].connected ||
			strcmp(input->controllers[i].name, input->controllers[slot].name) != 0) {
			continue;
		}
		if (i < slot) {
			++ordinal;
		}
		++count;
	}
	if (duplicate_count) {
		*duplicate_count = count;
	}
	return ordinal;
}

static const AeronControllerSnapshot* ControllerScreen_Selected(const AeronControllerSelector* selector,
																int* selected_slot) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	const AeronControllerSnapshot* controller;

	if (selected_slot) {
		*selected_slot = -1;
	}
	if (!input) {
		return NULL;
	}
	controller = Aeron_SelectController(input, selector);
	if (controller && selected_slot) {
		*selected_slot = (int)(controller - input->controllers);
	}
	return controller;
}

static void ControllerScreen_SelectDevice(AeronControllerSelector* selector, int direction) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	const AeronControllerSnapshot* selected;
	int connected_slots[AERON_CONTROLLER_MAX];
	int count = 0;
	int current = 0;
	int slot;
	int i;

	if (!input) {
		memset(selector, 0, sizeof(*selector));
		return;
	}
	selected = Aeron_SelectController(input, selector);
	for (slot = 0; slot < AERON_CONTROLLER_MAX; ++slot) {
		if (input->controllers[slot].connected) {
			connected_slots[count++] = slot;
			if (&input->controllers[slot] == selected) {
				current = count;
			}
		}
	}
	if (selector->guid[0] && current == 0) {
		current = count + 1;
	}
	current += direction < 0 ? -1 : 1;
	if (current < 0) {
		current = count;
	} else if (current > count) {
		current = 0;
	}
	if (current == 0) {
		memset(selector, 0, sizeof(*selector));
		return;
	}
	i = connected_slots[current - 1];
	snprintf(selector->guid, sizeof(selector->guid), "%s", input->controllers[i].guid);
	snprintf(selector->path, sizeof(selector->path), "%s", input->controllers[i].path);
	selector->ordinal = ControllerScreen_DeviceOrdinal(input, i);
}

static void ControllerScreen_DeviceText(const AeronControllerSelector* selector, char* text, size_t capacity,
										const AeronControllerSnapshot** selected) {
	int slot = -1;
	const AeronControllerSnapshot* controller = ControllerScreen_Selected(selector, &slot);

	*selected = controller;
	if (!selector->guid[0] && !selector->path[0]) {
		if (controller) {
			snprintf(text, capacity, "Automatic: %.42s", controller->name);
		} else {
			snprintf(text, capacity, "%s", "Automatic (none connected)");
		}
	} else if (controller) {
		const AeronInputSnapshot* input = Aeron_InputSnapshot();
		int duplicate_count;
		const int ordinal = ControllerScreen_NameOrdinal(input, slot, &duplicate_count);
		if (duplicate_count > 1) {
			snprintf(text, capacity, "%.44s #%d", controller->name, ordinal + 1);
		} else {
			snprintf(text, capacity, "%.48s", controller->name);
		}
	} else {
		snprintf(text, capacity, "Unavailable: %.32s", selector->guid);
	}
}

static XwaControllerProfile* ControllerScreen_Profile(XwaModernInputOptions* options,
													  const AeronControllerSnapshot* controller) {
	return controller && controller->kind == AERON_CONTROLLER_KIND_JOYSTICK ? &options->controller.joystick
																			: &options->controller.gamepad;
}

static int ControllerScreen_AxisCount(const AeronControllerSnapshot* controller) {
	if (!controller) {
		return 0;
	}
	return controller->kind == AERON_CONTROLLER_KIND_GAMEPAD ? AERON_GAMEPAD_AXIS_COUNT
															 : controller->axis_count;
}

static int16_t ControllerScreen_Axis(const AeronControllerSnapshot* controller, int source) {
	if (!controller || source < 0 || source >= ControllerScreen_AxisCount(controller)) {
		return 0;
	}
	return controller->kind == AERON_CONTROLLER_KIND_GAMEPAD ? controller->gamepad_axes[source]
															 : controller->raw_axes[source];
}

static void ControllerScreen_AxisName(const AeronControllerSnapshot* controller, int source, char* text,
									  size_t capacity) {
	if (source < 0) {
		snprintf(text, capacity, "%s", "Not mapped");
	} else if (controller && controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		snprintf(text, capacity, "%s", Aeron_GamepadAxisName((AeronGamepadAxis)source));
	} else {
		snprintf(text, capacity, "Axis %d", source);
	}
}

static void ControllerScreen_BeginAxisCapture(const AeronControllerSnapshot* controller, int logical_axis) {
	int source;

	g_controllerCapture.axis = logical_axis;
	g_controllerCapture.button = -1;
	g_controllerCapture.digital_axis = -1;
	g_controllerCapture.wait_for_release = 1;
	memset(g_controllerCapture.axis_baseline, 0, sizeof(g_controllerCapture.axis_baseline));
	for (source = 0; source < ControllerScreen_AxisCount(controller); ++source) {
		g_controllerCapture.axis_baseline[source] = ControllerScreen_Axis(controller, source);
	}
}

static int ControllerScreen_UpdateAxisCapture(XwaModernInputOptions* options,
											  const AeronControllerSnapshot* controller) {
	XwaControllerProfile* profile;
	int best_source = -1;
	int best_movement = CONTROLLER_CAPTURE_THRESHOLD;
	int source;
	int logical_axis;

	if (g_controllerCapture.axis < 0 || !controller) {
		return 0;
	}
	if (g_controllerCapture.wait_for_release) {
		g_controllerCapture.wait_for_release = 0;
		return 0;
	}
	for (source = 0; source < ControllerScreen_AxisCount(controller); ++source) {
		int movement =
			(int)ControllerScreen_Axis(controller, source) - (int)g_controllerCapture.axis_baseline[source];
		if (movement < 0) {
			movement = -movement;
		}
		if (movement > best_movement) {
			best_movement = movement;
			best_source = source;
		}
	}
	if (best_source < 0) {
		return 0;
	}
	profile = ControllerScreen_Profile(options, controller);
	for (logical_axis = 0; logical_axis < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++logical_axis) {
		if (logical_axis != g_controllerCapture.axis && profile->axes[logical_axis].source == best_source) {
			profile->axes[logical_axis].source = -1;
		}
	}
	profile->axes[g_controllerCapture.axis].source = best_source;
	g_controllerCapture.axis = -1;
	return XwaModernInputOptions_Set(options);
}

static void ControllerScreen_BeginDigitalAxisCapture(const AeronControllerSnapshot* controller) {
	int source;

	g_controllerCapture.axis = -1;
	g_controllerCapture.button = -1;
	g_controllerCapture.digital_axis = 1;
	g_controllerCapture.wait_for_release = 1;
	memset(g_controllerCapture.axis_baseline, 0, sizeof(g_controllerCapture.axis_baseline));
	for (source = 0; source < ControllerScreen_AxisCount(controller); ++source) {
		g_controllerCapture.axis_baseline[source] = ControllerScreen_Axis(controller, source);
	}
}

static int ControllerScreen_UpdateDigitalAxisCapture(const AeronControllerSnapshot* controller,
													 ControllerBindingRow* binding) {
	int best_source = -1;
	int best_movement = CONTROLLER_CAPTURE_THRESHOLD;
	int best_delta = 0;
	int source;

	if (g_controllerCapture.digital_axis < 0 || !controller || !binding) {
		return 0;
	}
	if (g_controllerCapture.wait_for_release) {
		g_controllerCapture.wait_for_release = 0;
		return 0;
	}
	for (source = 0; source < ControllerScreen_AxisCount(controller); ++source) {
		const int delta =
			(int)ControllerScreen_Axis(controller, source) - (int)g_controllerCapture.axis_baseline[source];
		const int movement = delta < 0 ? -delta : delta;
		if (movement > best_movement) {
			best_source = source;
			best_movement = movement;
			best_delta = delta;
		}
	}
	if (best_source < 0) {
		return 0;
	}
	memset(binding, 0, sizeof(*binding));
	binding->kind =
		best_delta > 0 ? AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE : AERON_CONTROLLER_DIGITAL_AXIS_NEGATIVE;
	binding->source = best_source;
	binding->logical_button = -1;
	binding->pov_direction = -1;
	g_controllerCapture.digital_axis = -1;
	return 1;
}

static int ControllerScreen_PovReleased(const AeronControllerSnapshot* controller) {
	int hat;

	if (!controller) {
		return 0;
	}
	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		return !(controller->gamepad_buttons &
				 ((1u << AERON_GAMEPAD_BUTTON_DPAD_UP) | (1u << AERON_GAMEPAD_BUTTON_DPAD_RIGHT) |
				  (1u << AERON_GAMEPAD_BUTTON_DPAD_DOWN) | (1u << AERON_GAMEPAD_BUTTON_DPAD_LEFT)));
	}
	for (hat = 0; hat < controller->hat_count; ++hat) {
		if (controller->raw_hats[hat] != AERON_CONTROLLER_HAT_CENTERED) {
			return 0;
		}
	}
	return 1;
}

static int ControllerScreen_FirstPressedButton(const AeronControllerSnapshot* controller) {
	uint64_t pressed;
	int source;

	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		pressed = controller->gamepad_pressed_buttons;
		for (source = 0; source < AERON_GAMEPAD_BUTTON_COUNT; ++source) {
			if (pressed & (UINT64_C(1) << source)) {
				return source;
			}
		}
		return -1;
	}
	pressed = controller->raw_pressed_buttons;
	for (source = 0; source < controller->button_count; ++source) {
		if (pressed & (UINT64_C(1) << source)) {
			return source;
		}
	}
	return -1;
}

static int ControllerScreen_UpdatePovCapture(XwaModernInputOptions* options,
											 const AeronControllerSnapshot* controller) {
	XwaControllerProfile* profile;
	int source = -1;
	int logical_button;

	if (g_controllerCapture.button < 0 || !controller) {
		return 0;
	}
	if (g_controllerCapture.wait_for_release) {
		if (ControllerScreen_PovReleased(controller)) {
			g_controllerCapture.wait_for_release = 0;
		}
		return 0;
	}
	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		const uint32_t dpad = (1u << AERON_GAMEPAD_BUTTON_DPAD_UP) | (1u << AERON_GAMEPAD_BUTTON_DPAD_RIGHT) |
							  (1u << AERON_GAMEPAD_BUTTON_DPAD_DOWN) | (1u << AERON_GAMEPAD_BUTTON_DPAD_LEFT);
		if (controller->gamepad_pressed_buttons & dpad) {
			source = 1;
		}
	} else {
		for (source = 0; source < controller->hat_count; ++source) {
			if (controller->raw_hats[source] != AERON_CONTROLLER_HAT_CENTERED) {
				break;
			}
		}
		if (source >= controller->hat_count) {
			source = -1;
		}
	}
	if (source < 0) {
		return 0;
	}
	profile = ControllerScreen_Profile(options, controller);
	profile->pov_source = source;
	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		for (logical_button = 0; logical_button < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++logical_button) {
			AeronControllerDigitalSource* binding = &profile->buttons[logical_button];
			if (binding->kind == AERON_CONTROLLER_DIGITAL_BUTTON &&
				binding->index >= AERON_GAMEPAD_BUTTON_DPAD_UP &&
				binding->index <= AERON_GAMEPAD_BUTTON_DPAD_RIGHT) {
				binding->kind = AERON_CONTROLLER_DIGITAL_NONE;
				binding->index = 0;
				binding->threshold = XWA_CONTROLLER_DIGITAL_THRESHOLD_DEFAULT;
				profile->actions[logical_button] = 0;
			}
		}
	}
	g_controllerCapture.button = -1;
	return XwaModernInputOptions_Set(options);
}

void XwaModernControllerOptionsScreen_ResetCapture(void) {
	g_controllerCapture.axis = -1;
	g_controllerCapture.button = -1;
	g_controllerCapture.digital_axis = -1;
	g_controllerCapture.wait_for_release = 0;
}

void XwaModernControllerOptionsScreen_Leave(void) {
	XwaModernControllerOptionsScreen_ResetCapture();
	XwaControllerMapping_Rumble(0, 0, 0);
}

XwaModernControllerScreenResult XwaModernControllerOptionsScreen_Update(int menu_center_x, int* cursor_row) {
	static const char* const toggle_text[] = { "Off", "On" };
	XwaModernInputOptions options;
	XwaModernOptionsMenu menu;
	const AeronControllerSnapshot* selected;
	char device_text[96];
	char value[32];
	int changed;
	int pressed;

	if (!cursor_row) {
		return XWA_MODERN_CONTROLLER_SCREEN_STAY;
	}
	XwaModernInputOptions_Get(&options);
	ControllerScreen_DeviceText(&options.controller.device, device_text, sizeof(device_text), &selected);
	XwaModernOptionsMenu_Begin(&menu, menu_center_x, 110, cursor_row, 9);
	XwaModernOptionsMenu_DrawTitle(&menu, "Controller Setup");

	changed = XwaModernOptionsMenu_DrawValue(&menu, "Active Device", device_text, 100, 0);
	if (changed) {
		ControllerScreen_SelectDevice(&options.controller.device, changed);
		XwaModernInputOptions_Set(&options);
		XwaModernInputOptions_Get(&options);
		ControllerScreen_DeviceText(&options.controller.device, device_text, sizeof(device_text), &selected);
	}
	changed = XwaModernOptionsMenu_DrawValue(&menu, "Roll Enabled",
											 toggle_text[options.controller.roll_enabled != 0], 101, 0);
	if (changed) {
		options.controller.roll_enabled = !options.controller.roll_enabled;
		XwaModernInputOptions_Set(&options);
	}
	changed = XwaModernOptionsMenu_DrawValue(&menu, "Rumble Enabled",
											 toggle_text[options.controller.rumble_enabled != 0], 102, 0);
	if (changed) {
		options.controller.rumble_enabled = !options.controller.rumble_enabled;
		XwaModernInputOptions_Set(&options);
	}
	snprintf(value, sizeof(value), "%d", options.controller.rumble_strength);
	changed = XwaModernOptionsMenu_DrawValue(&menu, "Rumble Strength", value, 103, 0);
	if (changed) {
		options.controller.rumble_strength += changed;
		if (options.controller.rumble_strength < XWA_CONTROLLER_RUMBLE_STRENGTH_MIN) {
			options.controller.rumble_strength = XWA_CONTROLLER_RUMBLE_STRENGTH_MAX;
		} else if (options.controller.rumble_strength > XWA_CONTROLLER_RUMBLE_STRENGTH_MAX) {
			options.controller.rumble_strength = XWA_CONTROLLER_RUMBLE_STRENGTH_MIN;
		}
		XwaModernInputOptions_Set(&options);
	}
	pressed = XwaModernOptionsMenu_DrawAction(
		&menu, "Test Rumble", 104, !selected || !selected->has_rumble || !options.controller.rumble_enabled);
	if (pressed) {
		const uint16_t magnitude =
			(uint16_t)(options.controller.rumble_strength * 65535u / XWA_CONTROLLER_RUMBLE_STRENGTH_MAX);
		XwaControllerMapping_Rumble(magnitude, magnitude, 300);
	}
	if (XwaModernOptionsMenu_DrawAction(&menu, "Configure Axes", 105, 0)) {
		XwaControllerMapping_Rumble(0, 0, 0);
		return XWA_MODERN_CONTROLLER_SCREEN_AXES;
	}
	if (XwaModernOptionsMenu_DrawAction(&menu, "Configure Button Bindings", 106, 0)) {
		XwaControllerMapping_Rumble(0, 0, 0);
		return XWA_MODERN_CONTROLLER_SCREEN_BUTTONS;
	}
	if (XwaModernOptionsMenu_DrawAction(&menu, "Restore Controller Defaults", 107, 0)) {
		XwaModernInputOptions_RestoreControllerDefaults();
	}
	pressed = XwaModernOptionsMenu_DrawAction(&menu, FrontendString_Get(STR_BACK), 108, 0);
	pressed |= XwaModernOptionsMenu_TakeEscape(&menu);
	if (pressed) {
		XwaModernControllerOptionsScreen_Leave();
		XwaModernInputOptions_Flush();
		return XWA_MODERN_CONTROLLER_SCREEN_BACK;
	}
	return XWA_MODERN_CONTROLLER_SCREEN_STAY;
}

int XwaModernControllerAxesScreen_Update(int menu_center_x, int* cursor_row) {
	static const char* const logical_names[] = { "Yaw", "Pitch", "Throttle", "Roll" };
	static const char* const toggle_text[] = { "No", "Yes" };
	XwaModernInputOptions options;
	XwaModernOptionsMenu menu;
	const AeronControllerSnapshot* selected;
	XwaControllerProfile* profile;
	char value[96];
	int axis;
	int changed;
	int back;

	if (!cursor_row) {
		return 0;
	}
	XwaModernInputOptions_Get(&options);
	selected = ControllerScreen_Selected(&options.controller.device, NULL);
	profile = ControllerScreen_Profile(&options, selected);
	XwaModernOptionsMenu_Begin(&menu, menu_center_x, 60, cursor_row, 13);
	if (g_controllerCapture.axis >= 0 && menu.key == XWA_MODERN_MENU_KEY_ESCAPE) {
		g_controllerCapture.axis = -1;
		XwaModernOptionsMenu_TakeEscape(&menu);
	}
	ControllerScreen_UpdateAxisCapture(&options, selected);
	XwaModernOptionsMenu_DrawTitle(&menu, "Controller Axis Mapping");
	for (axis = 0; axis < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++axis) {
		char source_name[48];
		const int16_t live = ControllerScreen_Axis(selected, profile->axes[axis].source);
		ControllerScreen_AxisName(selected, profile->axes[axis].source, source_name, sizeof(source_name));
		snprintf(value, sizeof(value), "%s (%+.2f)", source_name, live < 0 ? live / 32768.0 : live / 32767.0);
		changed =
			XwaModernOptionsMenu_DrawValue(&menu, logical_names[axis], value, 120 + axis * 3, !selected);
		if (changed) {
			ControllerScreen_BeginAxisCapture(selected, axis);
		}
		if (selected && XwaModernOptionsMenu_LastRowSelected(&menu) &&
			Keyboard_IsKeyDown(CONTROLLER_KEY_DELETE)) {
			profile->axes[axis].source = -1;
			g_controllerCapture.axis = -1;
			XwaModernInputOptions_Set(&options);
		}
		changed = XwaModernOptionsMenu_DrawValue(
			&menu, "  Invert", toggle_text[profile->axes[axis].invert != 0], 121 + axis * 3, !selected);
		if (changed) {
			profile->axes[axis].invert = !profile->axes[axis].invert;
			XwaModernInputOptions_Set(&options);
		}
		snprintf(value, sizeof(value), "%d%%", (int)(profile->axes[axis].deadzone * 100.0f + 0.5f));
		changed = XwaModernOptionsMenu_DrawValue(&menu, "  Deadzone", value, 122 + axis * 3, !selected);
		if (changed) {
			int percent = (int)(profile->axes[axis].deadzone * 100.0f + 0.5f) + changed;
			if (percent < 0) {
				percent = 100;
			} else if (percent > 100) {
				percent = 0;
			}
			profile->axes[axis].deadzone = percent / 100.0f;
			XwaModernInputOptions_Set(&options);
		}
		if (axis + 1 < XWA_CONTROLLER_LOGICAL_AXIS_COUNT) {
			menu.y += 20;
		}
	}
	if (g_controllerCapture.axis >= 0) {
		snprintf(value, sizeof(value), "Move an axis for %s (Esc cancels)",
				 logical_names[g_controllerCapture.axis]);
		FrontendText_DrawCentered(12, value, &(FrontendRect) { 0, menu.y, 639, menu.y + 15 }, g_colorGreen);
		menu.y += 20;
	} else {
		FrontendText_DrawCentered(12, "Enter remaps the selected axis; Delete unbinds it",
								  &(FrontendRect) { 0, menu.y, 639, menu.y + 15 }, g_colorGreen);
		menu.y += 20;
	}
	back = XwaModernOptionsMenu_DrawAction(&menu, FrontendString_Get(STR_BACK), 132, 0);
	back |= XwaModernOptionsMenu_TakeEscape(&menu);
	if (back) {
		g_controllerCapture.axis = -1;
		return 1;
	}
	return 0;
}

static int ControllerScreen_IsAxisDigitalKind(AeronControllerDigitalSourceKind kind) {
	return kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE || kind == AERON_CONTROLLER_DIGITAL_AXIS_NEGATIVE;
}

static void ControllerScreen_DigitalSourceName(const AeronControllerSnapshot* controller,
											   AeronControllerDigitalSourceKind kind, int source, char* text,
											   size_t capacity) {
	if (kind == AERON_CONTROLLER_DIGITAL_NONE || source < 0) {
		snprintf(text, capacity, "%s", "Not mapped");
	} else if (kind == AERON_CONTROLLER_DIGITAL_BUTTON && controller &&
			   controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		snprintf(text, capacity, "%s", Aeron_GamepadButtonName((AeronGamepadButton)source));
	} else if (kind == AERON_CONTROLLER_DIGITAL_BUTTON) {
		snprintf(text, capacity, "Button %d", source);
	} else if (controller && controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		snprintf(text, capacity, "%s %c", Aeron_GamepadAxisName((AeronGamepadAxis)source),
				 kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE ? '+' : '-');
	} else {
		snprintf(text, capacity, "Axis %d %c", source,
				 kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE ? '+' : '-');
	}
}

static XwaControllerProfile* ControllerScreen_ProfileForKind(XwaModernInputOptions* options,
															 AeronControllerKind kind) {
	return kind == AERON_CONTROLLER_KIND_JOYSTICK ? &options->controller.joystick
												  : &options->controller.gamepad;
}

static int ControllerScreen_FindButton(const XwaControllerProfile* profile,
									   AeronControllerDigitalSourceKind kind, int source) {
	int logical;

	for (logical = 0; logical < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++logical) {
		if (profile->buttons[logical].kind == kind && profile->buttons[logical].index == source) {
			return logical;
		}
	}
	return -1;
}

static int ControllerScreen_FindFreeButton(const XwaControllerProfile* profile) {
	int logical;

	for (logical = 0; logical < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++logical) {
		if (profile->buttons[logical].kind == AERON_CONTROLLER_DIGITAL_NONE) {
			return logical;
		}
	}
	for (logical = 0; logical < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++logical) {
		if (profile->actions[logical] == 0) {
			return logical;
		}
	}
	return -1;
}

static int ControllerScreen_IsGamepadDpadButton(int source) {
	return source >= AERON_GAMEPAD_BUTTON_DPAD_UP && source <= AERON_GAMEPAD_BUTTON_DPAD_RIGHT;
}

static int ControllerScreen_PovDirection(uint8_t hat) {
	if (hat & AERON_CONTROLLER_HAT_UP) {
		return 0;
	}
	if (hat & AERON_CONTROLLER_HAT_RIGHT) {
		return 1;
	}
	if (hat & AERON_CONTROLLER_HAT_DOWN) {
		return 2;
	}
	if (hat & AERON_CONTROLLER_HAT_LEFT) {
		return 3;
	}
	return -1;
}

static int ControllerScreen_BuildBindingRows(const AeronControllerSnapshot* controller,
											 const XwaControllerProfile* profile, ControllerBindingRow* rows,
											 int capacity) {
	int count = 0;
	int logical;
	int source;
	int has_pov;
	int direction;

	if (!controller || !profile || !rows || capacity <= 0) {
		return 0;
	}
	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		for (source = 0; source < AERON_GAMEPAD_BUTTON_COUNT && count < capacity; ++source) {
			const int logical_button =
				ControllerScreen_FindButton(profile, AERON_CONTROLLER_DIGITAL_BUTTON, source);
			if (profile->pov_source && ControllerScreen_IsGamepadDpadButton(source)) {
				continue;
			}
			if (!(controller->gamepad_available_buttons & (1u << source)) && logical_button < 0) {
				continue;
			}
			rows[count].kind = AERON_CONTROLLER_DIGITAL_BUTTON;
			rows[count].source = source;
			rows[count].logical_button = logical_button;
			rows[count].pov_direction = -1;
			rows[count].capture_axis = 0;
			++count;
		}
		for (source = AERON_GAMEPAD_AXIS_LEFT_TRIGGER;
			 source <= AERON_GAMEPAD_AXIS_RIGHT_TRIGGER && count < capacity; ++source) {
			const int logical_button =
				ControllerScreen_FindButton(profile, AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE, source);
			if (!(controller->gamepad_available_axes & (1u << source)) && logical_button < 0) {
				continue;
			}
			rows[count].kind = AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE;
			rows[count].source = source;
			rows[count].logical_button = logical_button;
			rows[count].pov_direction = -1;
			rows[count].capture_axis = 0;
			++count;
		}
		has_pov = profile->pov_source != 0;
	} else {
		for (source = 0; source < controller->button_count && count < capacity; ++source) {
			rows[count].kind = AERON_CONTROLLER_DIGITAL_BUTTON;
			rows[count].source = source;
			rows[count].logical_button =
				ControllerScreen_FindButton(profile, AERON_CONTROLLER_DIGITAL_BUTTON, source);
			rows[count].pov_direction = -1;
			rows[count].capture_axis = 0;
			++count;
		}
		has_pov = profile->pov_source >= 0 && profile->pov_source < controller->hat_count;
	}
	for (logical = 0; logical < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT && count < capacity; ++logical) {
		const AeronControllerDigitalSource* binding = &profile->buttons[logical];
		if (!ControllerScreen_IsAxisDigitalKind(binding->kind)) {
			continue;
		}
		if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD &&
			binding->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE &&
			binding->index >= AERON_GAMEPAD_AXIS_LEFT_TRIGGER &&
			binding->index <= AERON_GAMEPAD_AXIS_RIGHT_TRIGGER) {
			continue;
		}
		rows[count].kind = binding->kind;
		rows[count].source = binding->index;
		rows[count].logical_button = logical;
		rows[count].pov_direction = -1;
		rows[count].capture_axis = 0;
		++count;
	}
	if (has_pov) {
		for (direction = 0; direction < 4 && count < capacity; ++direction) {
			rows[count].kind = AERON_CONTROLLER_DIGITAL_NONE;
			rows[count].source = -1;
			rows[count].logical_button = -1;
			rows[count].pov_direction = direction;
			rows[count].capture_axis = 0;
			++count;
		}
	}
	if (count < capacity) {
		rows[count].kind = AERON_CONTROLLER_DIGITAL_NONE;
		rows[count].source = -1;
		rows[count].logical_button = -1;
		rows[count].pov_direction = -1;
		rows[count].capture_axis = 1;
		++count;
	}
	return count;
}

static void ControllerScreen_BindingLabel(const AeronControllerSnapshot* controller,
										  const XwaControllerProfile* profile,
										  const ControllerBindingRow* binding, char* text, size_t capacity) {
	static const char* const directions[] = { "Up", "Right", "Down", "Left" };

	if (binding->capture_axis) {
		snprintf(text, capacity, "%s", "Bind axis direction...");
		return;
	}
	if (binding->pov_direction >= 0) {
		if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
			snprintf(text, capacity, "D-pad %s", directions[binding->pov_direction]);
		} else {
			snprintf(text, capacity, "Hat %d %s", profile->pov_source, directions[binding->pov_direction]);
		}
		return;
	}
	ControllerScreen_DigitalSourceName(controller, binding->kind, binding->source, text, capacity);
	if (binding->logical_button >= 0 && ControllerScreen_IsAxisDigitalKind(binding->kind)) {
		const size_t length = strlen(text);
		snprintf(text + length, capacity - length, " [%d%%]",
				 (int)(profile->buttons[binding->logical_button].threshold * 100.0f + 0.5f));
	}
}

static void ControllerScreen_SetBindingMessage(const char* text) {
	snprintf(g_controllerBindingMessage, sizeof(g_controllerBindingMessage), "%s", text ? text : "");
	g_controllerBindingMessageTtl = 96;
}

static void ControllerScreen_BeginBindingEdit(const AeronControllerSnapshot* controller,
											  const XwaControllerProfile* profile,
											  const ControllerBindingRow* binding, const char* title) {
	int action_index;

	if (binding->pov_direction < 0 && binding->logical_button < 0 &&
		ControllerScreen_FindFreeButton(profile) < 0) {
		ControllerScreen_SetBindingMessage("Maximum of 16 button bindings reached");
		return;
	}
	memset(&g_controllerBindingEdit, 0, sizeof(g_controllerBindingEdit));
	g_controllerBindingEdit.active = 1;
	g_controllerBindingEdit.controller_kind = controller->kind;
	g_controllerBindingEdit.kind = binding->kind;
	g_controllerBindingEdit.source = binding->source;
	g_controllerBindingEdit.pov_direction = binding->pov_direction;
	action_index = binding->pov_direction >= 0 ? XWA_CONTROLLER_LOGICAL_BUTTON_COUNT + binding->pov_direction
											   : binding->logical_button;
	g_controllerBindingEdit.action = action_index >= 0 ? profile->actions[action_index] : 0;
	snprintf(g_controllerBindingEdit.title, sizeof(g_controllerBindingEdit.title), "%.63s", title);
	Config_RunJoystickActionPicker(g_controllerBindingEdit.title, &g_controllerBindingEdit.action);
}

static int ControllerScreen_UpdateBindingEdit(void) {
	XwaModernInputOptions options;
	XwaControllerProfile* profile;
	int logical;

	if (!g_controllerBindingEdit.active) {
		return 0;
	}
	if (!Config_RunJoystickActionPicker(g_controllerBindingEdit.title, &g_controllerBindingEdit.action)) {
		return 1;
	}
	XwaModernInputOptions_Get(&options);
	profile = ControllerScreen_ProfileForKind(&options, g_controllerBindingEdit.controller_kind);
	if (g_controllerBindingEdit.pov_direction >= 0) {
		profile->actions[XWA_CONTROLLER_LOGICAL_BUTTON_COUNT + g_controllerBindingEdit.pov_direction] =
			g_controllerBindingEdit.action;
	} else {
		logical = ControllerScreen_FindButton(profile, g_controllerBindingEdit.kind,
											  g_controllerBindingEdit.source);
		if (logical < 0 && g_controllerBindingEdit.action != 0) {
			logical = ControllerScreen_FindFreeButton(profile);
		}
		if (logical >= 0) {
			if (g_controllerBindingEdit.action == 0) {
				profile->buttons[logical].kind = AERON_CONTROLLER_DIGITAL_NONE;
				profile->buttons[logical].index = 0;
				profile->buttons[logical].threshold = XWA_CONTROLLER_DIGITAL_THRESHOLD_DEFAULT;
				profile->actions[logical] = 0;
			} else {
				profile->buttons[logical].kind = g_controllerBindingEdit.kind;
				profile->buttons[logical].index = (uint8_t)g_controllerBindingEdit.source;
				if (!ControllerScreen_IsAxisDigitalKind(g_controllerBindingEdit.kind)) {
					profile->buttons[logical].threshold = XWA_CONTROLLER_DIGITAL_THRESHOLD_DEFAULT;
				}
				profile->actions[logical] = g_controllerBindingEdit.action;
			}
		} else if (g_controllerBindingEdit.action != 0) {
			ControllerScreen_SetBindingMessage("Maximum of 16 button bindings reached");
		}
	}
	if (!XwaModernInputOptions_Set(&options)) {
		ControllerScreen_SetBindingMessage("Could not update controller binding");
	}
	memset(&g_controllerBindingEdit, 0, sizeof(g_controllerBindingEdit));
	return 0;
}

static int ControllerScreen_PressedPovDirection(const AeronControllerSnapshot* controller,
												const XwaControllerProfile* profile) {
	uint32_t pressed;

	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD && profile->pov_source) {
		pressed = controller->gamepad_pressed_buttons;
		if (pressed & (1u << AERON_GAMEPAD_BUTTON_DPAD_UP)) {
			return 0;
		}
		if (pressed & (1u << AERON_GAMEPAD_BUTTON_DPAD_RIGHT)) {
			return 1;
		}
		if (pressed & (1u << AERON_GAMEPAD_BUTTON_DPAD_DOWN)) {
			return 2;
		}
		if (pressed & (1u << AERON_GAMEPAD_BUTTON_DPAD_LEFT)) {
			return 3;
		}
	} else if (controller->kind == AERON_CONTROLLER_KIND_JOYSTICK && profile->pov_source >= 0 &&
			   profile->pov_source < controller->hat_count) {
		return ControllerScreen_PovDirection(controller->raw_hats[profile->pov_source]);
	}
	return -1;
}

static void ControllerScreen_SelectPressedBinding(const AeronControllerSnapshot* controller,
												  const XwaControllerProfile* profile,
												  const ControllerBindingRow* bindings, int binding_count,
												  int* cursor_row) {
	int source;
	int direction;
	int index;

	if (!controller || !cursor_row || g_controllerCapture.button >= 0 ||
		g_controllerCapture.digital_axis >= 0) {
		return;
	}
	source = ControllerScreen_FirstPressedButton(controller);
	direction = ControllerScreen_PressedPovDirection(controller, profile);
	for (index = 0; index < binding_count; ++index) {
		const ControllerBindingRow* row = &bindings[index];
		int active = direction >= 0 && row->pov_direction == direction;
		if (!active && direction < 0 && source >= 0 && row->kind == AERON_CONTROLLER_DIGITAL_BUTTON &&
			row->source == source) {
			active = 1;
		}
		if (!active && direction < 0 && ControllerScreen_IsAxisDigitalKind(row->kind)) {
			const int16_t value = ControllerScreen_Axis(controller, row->source);
			const double magnitude = row->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE
										 ? (value > 0 ? (double)value / 32767.0 : 0.0)
										 : (value < 0 ? (double)-value / 32768.0 : 0.0);
			const float threshold = row->logical_button >= 0 ? profile->buttons[row->logical_button].threshold
															 : XWA_CONTROLLER_DIGITAL_THRESHOLD_DEFAULT;
			active = magnitude >= threshold;
		}
		if (active) {
			g_controllerButtonPage = index / CONTROLLER_PAGE_SIZE;
			*cursor_row = 1 + index % CONTROLLER_PAGE_SIZE;
			return;
		}
	}
}

int XwaModernControllerButtonsScreen_Update(int menu_center_x, int* cursor_row) {
	ControllerBindingRow bindings[AERON_CONTROLLER_BUTTON_MAX + AERON_CONTROLLER_AXIS_MAX * 2 + 5];
	ControllerBindingRow captured_binding;
	XwaModernInputOptions options;
	XwaModernOptionsMenu menu;
	const AeronControllerSnapshot* selected;
	XwaControllerProfile* profile;
	char label[64];
	char value[64];
	int binding_count;
	int page_count;
	int start;
	int count;
	int index;
	int row_count;
	int changed;
	int back;

	if (ControllerScreen_UpdateBindingEdit()) {
		return 0;
	}
	XwaModernInputOptions_Get(&options);
	selected = ControllerScreen_Selected(&options.controller.device, NULL);
	profile = ControllerScreen_Profile(&options, selected);
	if (ControllerScreen_UpdateDigitalAxisCapture(selected, &captured_binding)) {
		captured_binding.logical_button =
			ControllerScreen_FindButton(profile, captured_binding.kind, captured_binding.source);
		ControllerScreen_BindingLabel(selected, profile, &captured_binding, label, sizeof(label));
		ControllerScreen_BeginBindingEdit(selected, profile, &captured_binding, label);
		return 0;
	}
	binding_count = ControllerScreen_BuildBindingRows(selected, profile, bindings,
													  (int)(sizeof(bindings) / sizeof(bindings[0])));
	page_count = (binding_count + CONTROLLER_PAGE_SIZE - 1) / CONTROLLER_PAGE_SIZE;
	if (page_count < 1) {
		page_count = 1;
	}
	if (g_controllerButtonPage >= page_count) {
		g_controllerButtonPage = page_count - 1;
	}
	ControllerScreen_SelectPressedBinding(selected, profile, bindings, binding_count, cursor_row);
	start = g_controllerButtonPage * CONTROLLER_PAGE_SIZE;
	count = binding_count - start;
	if (count < 0) {
		count = 0;
	} else if (count > CONTROLLER_PAGE_SIZE) {
		count = CONTROLLER_PAGE_SIZE;
	}
	row_count = 1 + count + (page_count > 1) + 1;
	if (*cursor_row >= row_count) {
		*cursor_row = row_count - 1;
	}
	if (!XwaModernOptionsMenu_Begin(&menu, menu_center_x, 110, cursor_row, row_count)) {
		return 0;
	}
	if (g_controllerCapture.button >= 0 && menu.key == XWA_MODERN_MENU_KEY_ESCAPE) {
		g_controllerCapture.button = -1;
		XwaModernOptionsMenu_TakeEscape(&menu);
	}
	if (g_controllerCapture.digital_axis >= 0 && menu.key == XWA_MODERN_MENU_KEY_ESCAPE) {
		g_controllerCapture.digital_axis = -1;
		XwaModernOptionsMenu_TakeEscape(&menu);
	}
	ControllerScreen_UpdatePovCapture(&options, selected);
	XwaModernOptionsMenu_DrawTitle(&menu, "Controller Button Bindings");

	if (selected && selected->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		snprintf(value, sizeof(value), "%s", profile->pov_source ? "D-pad" : "Not mapped");
	} else if (selected && profile->pov_source >= 0) {
		snprintf(value, sizeof(value), "Hat %d", profile->pov_source);
	} else {
		snprintf(value, sizeof(value), "%s", "Not mapped");
	}
	changed = XwaModernOptionsMenu_DrawValue(&menu, "POV Source", value, 139, !selected);
	if (changed) {
		g_controllerCapture.axis = -1;
		g_controllerCapture.digital_axis = -1;
		g_controllerCapture.button = XWA_CONTROLLER_LOGICAL_BUTTON_COUNT;
		g_controllerCapture.wait_for_release = 1;
	}
	if (selected && XwaModernOptionsMenu_LastRowSelected(&menu) &&
		Keyboard_IsKeyDown(CONTROLLER_KEY_DELETE)) {
		profile->pov_source = selected->kind == AERON_CONTROLLER_KIND_GAMEPAD ? 0 : -1;
		g_controllerCapture.button = -1;
		XwaModernInputOptions_Set(&options);
	}

	for (index = 0; index < count; ++index) {
		ControllerBindingRow* binding = &bindings[start + index];
		uint16_t action = binding->pov_direction >= 0
							  ? profile->actions[XWA_CONTROLLER_LOGICAL_BUTTON_COUNT + binding->pov_direction]
						  : binding->logical_button >= 0 ? profile->actions[binding->logical_button]
														 : 0;
		ControllerScreen_BindingLabel(selected, profile, binding, label, sizeof(label));
		if (Config_DrawJoystickBindingRow(&action, label, &menu.y, &menu.row, &menu.key, 140 + index)) {
			if (binding->capture_axis) {
				ControllerScreen_BeginDigitalAxisCapture(selected);
				return 0;
			}
			ControllerScreen_BeginBindingEdit(selected, profile, binding, label);
			return 0;
		}
		if (binding->logical_button >= 0 && ControllerScreen_IsAxisDigitalKind(binding->kind) &&
			XwaModernOptionsMenu_LastRowSelected(&menu) &&
			(menu.key == XWA_MODERN_MENU_KEY_LEFT || menu.key == XWA_MODERN_MENU_KEY_RIGHT)) {
			AeronControllerDigitalSource* digital = &profile->buttons[binding->logical_button];
			int percent = (int)(digital->threshold * 100.0f + 0.5f);
			percent += menu.key == XWA_MODERN_MENU_KEY_LEFT ? -5 : 5;
			if (percent < 5) {
				percent = 5;
			} else if (percent > 100) {
				percent = 100;
			}
			digital->threshold = percent / 100.0f;
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			menu.key = 0;
			XwaModernInputOptions_Set(&options);
		}
		if (XwaModernOptionsMenu_LastRowSelected(&menu) && Keyboard_IsKeyDown(CONTROLLER_KEY_DELETE)) {
			if (binding->pov_direction >= 0) {
				profile->actions[XWA_CONTROLLER_LOGICAL_BUTTON_COUNT + binding->pov_direction] = 0;
			} else if (binding->logical_button >= 0) {
				profile->buttons[binding->logical_button].kind = AERON_CONTROLLER_DIGITAL_NONE;
				profile->buttons[binding->logical_button].index = 0;
				profile->buttons[binding->logical_button].threshold =
					XWA_CONTROLLER_DIGITAL_THRESHOLD_DEFAULT;
				profile->actions[binding->logical_button] = 0;
			}
			XwaModernInputOptions_Set(&options);
		}
	}
	if (page_count > 1) {
		snprintf(value, sizeof(value), "Page %d of %d", g_controllerButtonPage + 1, page_count);
		changed = XwaModernOptionsMenu_DrawValue(&menu, "Bindings", value, 149, 0);
		if (changed) {
			g_controllerButtonPage += changed;
			if (g_controllerButtonPage < 0) {
				g_controllerButtonPage = page_count - 1;
			} else if (g_controllerButtonPage >= page_count) {
				g_controllerButtonPage = 0;
			}
			*cursor_row = 1;
			g_controllerCapture.button = -1;
			g_controllerCapture.digital_axis = -1;
		}
	}
	if (g_controllerCapture.digital_axis >= 0) {
		FrontendText_DrawCentered(12, "Move an axis direction (Esc cancels)",
								  &(FrontendRect) { 0, menu.y, 639, menu.y + 15 }, g_colorGreen);
		menu.y += 20;
	} else if (g_controllerCapture.button >= 0) {
		FrontendText_DrawCentered(12, "Press a D-pad or hat direction (Esc cancels)",
								  &(FrontendRect) { 0, menu.y, 639, menu.y + 15 }, g_colorGreen);
		menu.y += 20;
	} else if (g_controllerBindingMessageTtl > 0) {
		FrontendText_DrawCentered(12, g_controllerBindingMessage,
								  &(FrontendRect) { 0, menu.y, 639, menu.y + 15 }, g_colorGreen);
		--g_controllerBindingMessageTtl;
		menu.y += 20;
	}
	FrontendText_DrawCentered(12, "Press a control to find it; Enter or click assigns; Delete unbinds",
							  &(FrontendRect) { 0, menu.y, 639, menu.y + 15 }, g_colorGreen);
	menu.y += 20;
	FrontendText_DrawCentered(12, "Left/Right adjusts an axis binding threshold",
							  &(FrontendRect) { 0, menu.y, 639, menu.y + 15 }, g_colorGreen);
	menu.y += 20;
	back = XwaModernOptionsMenu_DrawAction(&menu, FrontendString_Get(STR_BACK), 150, 0);
	back |= XwaModernOptionsMenu_TakeEscape(&menu);
	if (back) {
		g_controllerCapture.button = -1;
		g_controllerCapture.digital_axis = -1;
		return 1;
	}
	return 0;
}
