#include "xwa_runtime/config/modern_video_options_screen.h"

#include "xwa/assets/string_table.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa_runtime/config/modern_video_options.h"

#include <stdint.h>

enum {
	XWA_MODERN_MENU_KEY_ENTER = '\r',
	XWA_MODERN_MENU_KEY_ESCAPE = 0x1b,
	XWA_MODERN_MENU_KEY_LEFT = 0x25,
	XWA_MODERN_MENU_KEY_UP = 0x26,
	XWA_MODERN_MENU_KEY_RIGHT = 0x27,
	XWA_MODERN_MENU_KEY_DOWN = 0x28,
};

static int XwaModernVideoOptionsScreen_DrawCycle(uint8_t* value, const char* label,
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

int XwaModernVideoOptionsScreen_Update(int menu_center_x, int* cursor_row) {
	static const char* const window_mode_texts[] = { "Windowed", "Fullscreen" };
	static const char* const quality_texts[] = { "Off", "Low", "High" };
	static const char* const fsr_texts[] = { "Off", "Performance", "Balanced", "Quality", "Native AA" };
	static const char* const msaa_texts[] = { "Off", "2x", "4x", "8x" };
	static const char* const toggle_texts[] = { "Off", "On" };
	XwaModernVideoOptions options;
	char key_state;
	const char* text;
	uint8_t window_mode;
	uint8_t ssao;
	uint8_t fsr;
	uint8_t msaa;
	uint8_t original_fsr;
	uint8_t original_msaa;
	uint8_t motion_blur;
	uint8_t hdr;
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

	XwaModernVideoOptions_Get(&options);
	window_mode = (uint8_t)options.window_mode;
	ssao = (uint8_t)options.ssao_quality;
	fsr = (uint8_t)options.fsr_upscaling;
	msaa = (uint8_t)options.msaa;
	original_fsr = fsr;
	original_msaa = msaa;
	motion_blur = (uint8_t)options.motion_blur_quality;
	hdr = (uint8_t)(options.hdr_output != 0);
	y = 140;
	row_index = 0;
	changed = 0;
	key_state = (char)Config_GetMenuNavKey();
	if (key_state == XWA_MODERN_MENU_KEY_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		key_state = 0;
		if (--*cursor_row < 0) {
			*cursor_row = 6;
		}
	} else if (key_state == XWA_MODERN_MENU_KEY_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		key_state = 0;
		if (++*cursor_row >= 7) {
			*cursor_row = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	text = "OpenXWA Video Options";
	FrontendText_DrawCentered(15, text, &rect, g_colorLightBlue);
	title_width = FrontendText_MeasureWidth(text, 20);
	FrontendDraw_Line(menu_center_x - (int)(title_width >> 1), y + 17,
					  menu_center_x - (int)(title_width >> 1) + (int)title_width, y + 17, g_colorLightBlue);
	y += 20;

	changed |=
		XwaModernVideoOptionsScreen_DrawCycle(&window_mode, "Window Mode", window_mode_texts, 2,
											  menu_center_x, &y, &row_index, *cursor_row, &key_state, 60);
	changed |= XwaModernVideoOptionsScreen_DrawCycle(&ssao, "SSAO", quality_texts, 3, menu_center_x, &y,
													 &row_index, *cursor_row, &key_state, 61);
	changed |= XwaModernVideoOptionsScreen_DrawCycle(&fsr, "FSR Upscaling", fsr_texts, 5, menu_center_x, &y,
													 &row_index, *cursor_row, &key_state, 62);
	changed |= XwaModernVideoOptionsScreen_DrawCycle(&msaa, "MSAA", msaa_texts, 4, menu_center_x, &y,
													 &row_index, *cursor_row, &key_state, 63);
	changed |=
		XwaModernVideoOptionsScreen_DrawCycle(&motion_blur, "Motion Blur", quality_texts, 3, menu_center_x,
											  &y, &row_index, *cursor_row, &key_state, 64);
	changed |= XwaModernVideoOptionsScreen_DrawCycle(&hdr, "HDR Output", toggle_texts, 2, menu_center_x, &y,
													 &row_index, *cursor_row, &key_state, 65);

	if (changed) {
		if (msaa != original_msaa && msaa != XWA_MODERN_MSAA_OFF) {
			fsr = XWA_MODERN_FSR_OFF;
		} else if (fsr != original_fsr && fsr != XWA_MODERN_FSR_OFF) {
			msaa = XWA_MODERN_MSAA_OFF;
		}
		options.window_mode = (XwaModernWindowMode)window_mode;
		options.ssao_quality = (XwaModernSsaoQuality)ssao;
		options.fsr_upscaling = (XwaModernFsrUpscaling)fsr;
		options.msaa = (XwaModernMsaa)msaa;
		options.motion_blur_quality = (XwaModernMotionBlurQuality)motion_blur;
		options.hdr_output = hdr != 0;
		XwaModernVideoOptions_Set(&options);
	}

	text = FrontendString_Get(STR_BACK);
	text_x = menu_center_x - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
	button_pressed =
		FrontendButton_DrawMenuButton(text_x, y, text, 15, g_colorPaleBlue, 66, 0, "settingsound");
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

	XwaModernVideoOptions_Flush();
	return 1;
}
