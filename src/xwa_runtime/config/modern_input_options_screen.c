#include "xwa_runtime/config/modern_input_options_screen.h"

#include "xwa/assets/string_table.h"
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/config/modern_options_menu.h"

#include <stdint.h>

int XwaModernInputOptionsScreen_Update(int menu_center_x, int* cursor_row) {
	static const char* const toggle_texts[] = { "Off", "On" };
	/* Config tokens stay 'position'/'rate'; the labels describe the feel:
	 * a virtual stick holds its deflection, direct control turns the ship
	 * only while the mouse moves. */
	static const char* const mode_texts[] = { "Virtual Stick", "Direct" };
	static const char* const sensitivity_texts[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
	XwaModernInputOptions options;
	XwaModernOptionsMenu menu;
	uint8_t mouse_flight;
	uint8_t mouse_mode;
	uint8_t sensitivity;
	uint8_t invert_y;
	int changed;
	int pressed;
	int result;

	if (!cursor_row) {
		return 0;
	}

	XwaModernInputOptions_Get(&options);
	mouse_flight = (uint8_t)(options.mouse_flight_enabled != 0);
	mouse_mode = (uint8_t)options.mouse_mode;
	sensitivity = (uint8_t)(options.mouse_sensitivity - XWA_MODERN_MOUSE_SENSITIVITY_MIN);
	invert_y = (uint8_t)(options.mouse_invert_y != 0);
	changed = 0;
	XwaModernOptionsMenu_Begin(&menu, menu_center_x, 160, cursor_row, 6);
	XwaModernOptionsMenu_DrawTitle(&menu, FrontendString_Get(STR_CONFIG_GAME_CONTROLLER_OPTIONS));

	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &mouse_flight, "Mouse Flight Control", toggle_texts, 2,
												70, 0);
	changed |=
		XwaModernOptionsMenu_DrawCycleU8(&menu, &mouse_mode, "Mouse Control Mode", mode_texts, 2, 74, 0);
	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &sensitivity, "Mouse Sensitivity", sensitivity_texts,
												9, 71, 0);
	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &invert_y, "Invert Mouse Y", toggle_texts, 2, 72, 0);

	if (changed) {
		options.mouse_flight_enabled = mouse_flight != 0;
		options.mouse_mode = (XwaModernMouseMode)mouse_mode;
		options.mouse_sensitivity = sensitivity + XWA_MODERN_MOUSE_SENSITIVITY_MIN;
		options.mouse_invert_y = invert_y != 0;
		XwaModernInputOptions_Set(&options);
	}

	result = 0;
	if (XwaModernOptionsMenu_DrawAction(&menu, "Controller Setup", 75, 0)) {
		result = 2;
	}
	pressed = XwaModernOptionsMenu_DrawAction(&menu, FrontendString_Get(STR_BACK), 73, 0);
	pressed |= XwaModernOptionsMenu_TakeEscape(&menu);
	if (pressed && result == 0) {
		result = 1;
	}
	if (!result) {
		return 0;
	}

	XwaModernInputOptions_Flush();
	return result;
}
