#include "xwa_runtime/config/modern_options_menu.h"

#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"

enum {
	XWA_MODERN_MENU_KEY_UP = 0x26,
	XWA_MODERN_MENU_KEY_DOWN = 0x28,
	XWA_MODERN_MENU_ROW_HEIGHT = 20,
};

static void XwaModernOptionsMenu_PlayMove(void) {
	FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
}

static void XwaModernOptionsMenu_PlaySelect(void) {
	FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
}

static int XwaModernOptionsMenu_DrawValueImpl(XwaModernOptionsMenu* menu, const char* label,
											  const char* value, int button_id, int disabled,
											  int enter_steps) {
	int label_x;
	int result;

	label_x = menu->center_x - FrontendText_MeasureWidth(label, 15) - 10;
	result = 0;
	if (disabled) {
		FrontendText_Draw(15, label, label_x, menu->y, menu->cursor_row == menu->row ? 0xffff : g_colorGray);
		FrontendText_Draw(15, value, menu->center_x + 10, menu->y, g_colorGray);
	} else {
		FrontendText_Draw(15, label, label_x, menu->y,
						  menu->cursor_row == menu->row ? g_colorGreen : g_colorLightBlue);
		result = FrontendButton_DrawMenuButton(menu->center_x + 10, menu->y, value, 15, g_colorPaleBlue,
											   button_id, 0, "settingsound");
		if (result == 2) {
			/* Original option rows use right click as the previous direction. */
			result = -1;
		}
		if (menu->cursor_row == menu->row &&
			(menu->key == XWA_MODERN_MENU_KEY_LEFT || menu->key == XWA_MODERN_MENU_KEY_RIGHT)) {
			result = menu->key == XWA_MODERN_MENU_KEY_LEFT ? -1 : 1;
			XwaModernOptionsMenu_PlaySelect();
			Keyboard_FlushCharBuffer();
			menu->key = 0;
		} else if (enter_steps && menu->cursor_row == menu->row && menu->key == XWA_MODERN_MENU_KEY_ENTER) {
			result = 1;
			XwaModernOptionsMenu_PlaySelect();
			Keyboard_FlushCharBuffer();
			menu->key = 0;
		}
	}
	menu->y += XWA_MODERN_MENU_ROW_HEIGHT;
	++menu->row;
	return result;
}

int XwaModernOptionsMenu_Begin(XwaModernOptionsMenu* menu, int center_x, int start_y, int* cursor_row,
							   int row_count) {
	char key;

	if (!menu || !cursor_row || row_count <= 0) {
		return 0;
	}
	key = (char)Config_GetMenuNavKey();
	if (key == XWA_MODERN_MENU_KEY_UP) {
		XwaModernOptionsMenu_PlayMove();
		Keyboard_FlushCharBuffer();
		key = 0;
		if (--*cursor_row < 0) {
			*cursor_row = row_count - 1;
		}
	} else if (key == XWA_MODERN_MENU_KEY_DOWN) {
		XwaModernOptionsMenu_PlayMove();
		Keyboard_FlushCharBuffer();
		key = 0;
		if (++*cursor_row >= row_count) {
			*cursor_row = 0;
		}
	}
	menu->center_x = center_x;
	menu->y = start_y;
	menu->row = 0;
	menu->cursor_row = *cursor_row;
	menu->key = key;
	return 1;
}

void XwaModernOptionsMenu_DrawTitle(XwaModernOptionsMenu* menu, const char* title) {
	FrontendRect rect;
	unsigned int width;
	int left;

	FrontendDraw_RectAssign(&rect, 0, menu->y, 639, menu->y + 15);
	FrontendText_DrawCentered(15, title, &rect, g_colorLightBlue);
	width = FrontendText_MeasureWidth(title, 20);
	left = menu->center_x - (int)(width >> 1);
	FrontendDraw_Line(left, menu->y + 17, left + (int)width, menu->y + 17, g_colorLightBlue);
	menu->y += XWA_MODERN_MENU_ROW_HEIGHT;
}

int XwaModernOptionsMenu_DrawAction(XwaModernOptionsMenu* menu, const char* text, int button_id,
									int disabled) {
	int x;
	int pressed;

	x = menu->center_x - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
	pressed = 0;
	if (disabled) {
		FrontendText_Draw(15, text, x, menu->y, menu->cursor_row == menu->row ? 0xffff : g_colorGray);
	} else {
		pressed = FrontendButton_DrawMenuButton(x, menu->y, text, 15, g_colorPaleBlue, button_id, 0,
												"settingsound") != 0;
		if (menu->cursor_row == menu->row) {
			FrontendText_Draw(15, text, x, menu->y, g_colorGreen);
			if (menu->key == XWA_MODERN_MENU_KEY_ENTER) {
				XwaModernOptionsMenu_PlaySelect();
				Keyboard_FlushCharBuffer();
				menu->key = 0;
				pressed = 1;
			}
		}
	}
	menu->y += XWA_MODERN_MENU_ROW_HEIGHT;
	++menu->row;
	return pressed;
}

int XwaModernOptionsMenu_DrawValue(XwaModernOptionsMenu* menu, const char* label, const char* value,
								   int button_id, int disabled) {
	return XwaModernOptionsMenu_DrawValueImpl(menu, label, value, button_id, disabled, 1);
}

int XwaModernOptionsMenu_DrawCycleU8(XwaModernOptionsMenu* menu, uint8_t* value, const char* label,
									 const char* const* value_texts, int option_count, int button_id,
									 int disabled) {
	int step;

	if (!value || !value_texts || option_count <= 0) {
		return 0;
	}
	step = XwaModernOptionsMenu_DrawValueImpl(menu, label, value_texts[*value], button_id, disabled, 0);
	if (step > 0) {
		++*value;
		if (*value >= option_count) {
			*value = 0;
		}
	} else if (step < 0) {
		if (*value) {
			--*value;
		} else {
			*value = (uint8_t)(option_count - 1);
		}
	}
	return step != 0;
}

int XwaModernOptionsMenu_TakeEscape(XwaModernOptionsMenu* menu) {
	if (!menu || menu->key != XWA_MODERN_MENU_KEY_ESCAPE) {
		return 0;
	}
	Keyboard_FlushCharBuffer();
	menu->key = 0;
	return 1;
}

int XwaModernOptionsMenu_LastRowSelected(const XwaModernOptionsMenu* menu) {
	return menu && menu->cursor_row == menu->row - 1;
}
