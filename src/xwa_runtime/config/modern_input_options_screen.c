#include "xwa_runtime/config/modern_input_options_screen.h"

#include "xwa/assets/string_table.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa_runtime/config/modern_input_options.h"

#include <stdint.h>

enum {
	XWA_MODERN_MENU_KEY_ENTER = '\r',
	XWA_MODERN_MENU_KEY_ESCAPE = 0x1b,
	XWA_MODERN_MENU_KEY_LEFT = 0x25,
	XWA_MODERN_MENU_KEY_UP = 0x26,
	XWA_MODERN_MENU_KEY_RIGHT = 0x27,
	XWA_MODERN_MENU_KEY_DOWN = 0x28,
};

static int XwaModernInputOptionsScreen_DrawCycle(uint8_t* value, const char* label,
												 const char* const* value_texts, int option_count,
												 int menu_center_x, int* y, int* row_index, int cursor_row,
												 char* key_state, int button_id) {
	const char* value_text;
	int label_x;
	int value_x;
	int changed;

	label_x = menu_center_x - FrontendText_MeasureWidth(label, 15) - 10;
	changed = 0;
	if (cursor_row == *row_index) {
		FrontendText_Draw(15, label, label_x, *y, g_colorGreen);
		if (*key_state == XWA_MODERN_MENU_KEY_LEFT) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			*key_state = 0;
			changed = 2;
		} else if (*key_state == XWA_MODERN_MENU_KEY_RIGHT) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			*key_state = 0;
			changed = 1;
		}
	} else {
		FrontendText_Draw(15, label, label_x, *y, g_colorLightBlue);
	}

	value_x = menu_center_x + 10;
	value_text = value_texts[*value];
	changed = FrontendButton_DrawMenuButton(value_x, *y, value_text, 15, g_colorPaleBlue, button_id, 0,
											"settingsound") |
			  changed;
	if (changed == 1) {
		++*value;
		if (*value >= option_count) {
			*value = 0;
		}
	} else if (changed == 2) {
		if (*value) {
			--*value;
		} else {
			*value = (uint8_t)(option_count - 1);
		}
	}

	*y += 20;
	++*row_index;
	return changed;
}

int XwaModernInputOptionsScreen_Update(int menu_center_x, int* cursor_row) {
	static const char* const toggle_texts[] = { "Off", "On" };
	/* Config tokens stay 'position'/'rate'; the labels describe the feel:
	 * a virtual stick holds its deflection, direct control turns the ship
	 * only while the mouse moves. */
	static const char* const mode_texts[] = { "Virtual Stick", "Direct" };
	static const char* const sensitivity_texts[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
	XwaModernInputOptions options;
	char key_state;
	const char* text;
	uint8_t mouse_flight;
	uint8_t mouse_mode;
	uint8_t sensitivity;
	uint8_t invert_y;
	int y;
	int row_index;
	int changed;
	int text_x;
	int button_pressed;
	unsigned int title_width;
	FrontendRect rect;

	if (!cursor_row) {
		return 0;
	}

	XwaModernInputOptions_Get(&options);
	mouse_flight = (uint8_t)(options.mouse_flight_enabled != 0);
	mouse_mode = (uint8_t)options.mouse_mode;
	sensitivity = (uint8_t)(options.mouse_sensitivity - XWA_MODERN_MOUSE_SENSITIVITY_MIN);
	invert_y = (uint8_t)(options.mouse_invert_y != 0);
	y = 160;
	row_index = 0;
	changed = 0;
	key_state = (char)Config_GetMenuNavKey();
	if (key_state == XWA_MODERN_MENU_KEY_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		key_state = 0;
		if (--*cursor_row < 0) {
			*cursor_row = 4;
		}
	} else if (key_state == XWA_MODERN_MENU_KEY_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		key_state = 0;
		if (++*cursor_row >= 5) {
			*cursor_row = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	text = "OpenXWA Input Options";
	FrontendText_DrawCentered(15, text, &rect, g_colorLightBlue);
	title_width = FrontendText_MeasureWidth(text, 20);
	FrontendDraw_Line(menu_center_x - (int)(title_width >> 1), y + 17,
					  menu_center_x - (int)(title_width >> 1) + (int)title_width, y + 17, g_colorLightBlue);
	y += 20;

	changed |=
		XwaModernInputOptionsScreen_DrawCycle(&mouse_flight, "Mouse Flight Control", toggle_texts, 2,
											  menu_center_x, &y, &row_index, *cursor_row, &key_state, 70);
	changed |=
		XwaModernInputOptionsScreen_DrawCycle(&mouse_mode, "Mouse Control Mode", mode_texts, 2, menu_center_x,
											  &y, &row_index, *cursor_row, &key_state, 74);
	changed |=
		XwaModernInputOptionsScreen_DrawCycle(&sensitivity, "Mouse Sensitivity", sensitivity_texts, 9,
											  menu_center_x, &y, &row_index, *cursor_row, &key_state, 71);
	changed |=
		XwaModernInputOptionsScreen_DrawCycle(&invert_y, "Invert Mouse Y", toggle_texts, 2, menu_center_x, &y,
											  &row_index, *cursor_row, &key_state, 72);

	if (changed) {
		options.mouse_flight_enabled = mouse_flight != 0;
		options.mouse_mode = (XwaModernMouseMode)mouse_mode;
		options.mouse_sensitivity = sensitivity + XWA_MODERN_MOUSE_SENSITIVITY_MIN;
		options.mouse_invert_y = invert_y != 0;
		XwaModernInputOptions_Set(&options);
	}

	text = FrontendString_Get(STR_BACK);
	text_x = menu_center_x - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
	button_pressed =
		FrontendButton_DrawMenuButton(text_x, y, text, 15, g_colorPaleBlue, 73, 0, "settingsound");
	if (*cursor_row == row_index) {
		FrontendText_Draw(15, text, text_x, y, g_colorGreen);
		if (key_state == XWA_MODERN_MENU_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			key_state = 0;
			button_pressed |= 1;
		}
	}
	if (key_state == XWA_MODERN_MENU_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		button_pressed = 1;
	}
	if (!button_pressed) {
		return 0;
	}

	XwaModernInputOptions_Flush();
	return 1;
}
