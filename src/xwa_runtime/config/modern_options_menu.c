#include "xwa_runtime/config/modern_options_menu.h"

#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_image.h"
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

int XwaModernOptionsMenu_DrawSliderU8(XwaModernOptionsMenu* menu, uint8_t* value, const char* label,
									  const char* min_label, const char* max_label, int notch_count,
									  int disabled) {
	FrontendRect rect;
	uint8_t old_value;
	int mouse_x;
	int mouse_y;
	int label_x;
	int slider_x;
	int min_label_width;
	int notch_x;
	int segment_width;
	int notch_width;
	int notch_index;
	int pixel_index;

	if (!menu || !value || !label || !min_label || !max_label || notch_count <= 0 ||
		notch_count > UINT8_MAX) {
		return 0;
	}
	if (*value > notch_count) {
		*value = (uint8_t)notch_count;
	}
	old_value = *value;
	FrontendCursor_GetPos(&mouse_x, &mouse_y);
	label_x = menu->center_x - FrontendText_MeasureWidth(label, 15) - 10;
	FrontendText_Draw(15, label, label_x, menu->y,
					  disabled ? (menu->cursor_row == menu->row ? 0xffff : g_colorGray)
							   : (menu->cursor_row == menu->row ? g_colorGreen : g_colorLightBlue));

	if (!disabled && menu->cursor_row == menu->row &&
		(menu->key == XWA_MODERN_MENU_KEY_LEFT || menu->key == XWA_MODERN_MENU_KEY_RIGHT)) {
		XwaModernOptionsMenu_PlaySelect();
		if (menu->key == XWA_MODERN_MENU_KEY_LEFT) {
			if (*value) {
				--*value;
			}
		} else if (*value < notch_count) {
			++*value;
		}
		Keyboard_FlushCharBuffer();
		menu->key = 0;
	}

	slider_x = menu->center_x + 10;
	min_label_width = FrontendText_MeasureWidth(min_label, 10);
	FrontendText_Draw(10, min_label, slider_x, menu->y + 4, disabled ? g_colorGray : g_colorLightBlue);
	FrontendDraw_RectAssign(&rect, slider_x, menu->y, min_label_width + slider_x + 1, menu->y + 15);
	if (!disabled && FrontendDraw_PointInRect(&rect, mouse_x, mouse_y) &&
		(FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) && *value != 0) {
		XwaModernOptionsMenu_PlaySelect();
		*value = 0;
	}

	notch_x = min_label_width + menu->center_x + 16;
	segment_width = 120 / notch_count;
	notch_width = segment_width + 1;
	for (notch_index = 0; notch_index < notch_count; ++notch_index) {
		FrontendDraw_RectAssign(&rect, notch_x, menu->y + 5, notch_x + notch_width, menu->y + 15);
		if (!disabled && FrontendDraw_PointInRect(&rect, mouse_x, mouse_y) &&
			(FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) && *value != notch_index + 1) {
			XwaModernOptionsMenu_PlaySelect();
			*value = (uint8_t)(notch_index + 1);
		}
		rect.right -= 2;
		if (notch_index < *value) {
			for (pixel_index = 0; pixel_index < notch_width; ++pixel_index) {
				FrontImage_DrawSprite("sbarcenter", rect.left + pixel_index, menu->y + 5);
			}
		}
		notch_x += notch_width;
	}

	FrontImage_DrawSprite("sbarstart", min_label_width + menu->center_x + 10, menu->y + 5);
	if (*value) {
		FrontImage_DrawSprite("sbarspark", min_label_width + *value * notch_width + menu->center_x + 15,
							  menu->y + 5);
	}
	FrontendText_Draw(10, max_label, notch_x + 15, menu->y + 4, disabled ? g_colorGray : g_colorLightBlue);
	FrontendDraw_RectAssign(&rect, notch_x + 15, menu->y,
							FrontendText_MeasureWidth(max_label, 10) + notch_x + 16, menu->y + 15);
	if (!disabled && FrontendDraw_PointInRect(&rect, mouse_x, mouse_y) &&
		(FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) && *value != notch_count) {
		XwaModernOptionsMenu_PlaySelect();
		*value = (uint8_t)notch_count;
	}

	menu->y += XWA_MODERN_MENU_ROW_HEIGHT;
	++menu->row;
	return *value != old_value;
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
