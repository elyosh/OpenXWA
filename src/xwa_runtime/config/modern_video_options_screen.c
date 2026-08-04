#include "xwa_runtime/config/modern_video_options_screen.h"

#include "aeron/render.h"
#include "xwa/assets/string_table.h"
#include "xwa_runtime/config/modern_options_menu.h"
#include "xwa_runtime/config/modern_video_options.h"

#include <math.h>
#include <stdint.h>

int XwaModernVideoOptionsScreen_Update(int menu_center_x, int* cursor_row) {
	static const char* const window_mode_texts[] = { "Windowed", "Fullscreen" };
	static const char* const quality_texts[] = { "Off", "Low", "High" };
	static const char* const shadow_quality_texts[] = { "Standard", "High" };
	static const char* const motion_blur_texts[] = { "Off", "Low Quality", "High Quality" };
	static const char* const fsr_texts[] = { "Off", "Performance", "Balanced", "Quality", "Native AA" };
	static const char* const msaa_texts[] = { "Off", "2x", "4x", "8x" };
	static const char* const toggle_texts[] = { "Off", "On" };
	/* The cycle covers 2.2/2.4 only (option_count 2); index 2 exists so a
	 * config.yaml 'srgb' value (and the fixed Apple behavior) still displays
	 * truthfully — any left/right press moves into the offered pair. */
	static const char* const sdr_gamma_texts[] = { "2.2", "2.4", "sRGB" };
	static const char* const paper_white_texts[] = { "Auto",     "100 nits", "150 nits", "200 nits",
													 "250 nits", "300 nits", "400 nits" };
	XwaModernVideoOptions options;
	XwaModernOptionsMenu menu;
	uint8_t window_mode;
	uint8_t ssao;
	uint8_t shadow_quality;
	uint8_t fsr;
	uint8_t msaa;
	uint8_t original_fsr;
	uint8_t original_msaa;
	uint8_t motion_blur_quality;
	uint8_t motion_blur_amount;
	uint8_t hdr;
	uint8_t sdr_gamma;
	uint8_t paper_white;
	int hdr_rows_disabled;
	int motion_blur_amount_changed;
	int changed;
	int back;

	if (!cursor_row) {
		return 0;
	}

	XwaModernVideoOptions_Get(&options);
	window_mode = (uint8_t)options.window_mode;
	ssao = (uint8_t)options.ssao_quality;
	shadow_quality = (uint8_t)options.shadow_quality;
	fsr = (uint8_t)options.fsr_upscaling;
	msaa = (uint8_t)options.msaa;
	original_fsr = fsr;
	original_msaa = msaa;
	motion_blur_quality = (uint8_t)options.motion_blur_quality;
	motion_blur_amount = (uint8_t)lroundf(options.motion_blur_amount * 10.0f);
	hdr = (uint8_t)(options.hdr_output != 0);
	sdr_gamma = (uint8_t)options.sdr_gamma;
	paper_white = (uint8_t)options.paper_white;
	changed = 0;
	XwaModernOptionsMenu_Begin(&menu, menu_center_x, 140, cursor_row, 11);
	XwaModernOptionsMenu_DrawTitle(&menu, "OpenXWA Video Options");

	changed |=
		XwaModernOptionsMenu_DrawCycleU8(&menu, &window_mode, "Window Mode", window_mode_texts, 2, 60, 0);
	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &ssao, "SSAO", quality_texts, 3, 61, 0);
	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &shadow_quality, "Shadow Quality",
											 shadow_quality_texts, 2, 62, 0);
	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &fsr, "FSR Upscaling", fsr_texts, 5, 63, 0);
	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &msaa, "MSAA", msaa_texts, 4, 64, 0);
	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &motion_blur_quality, "Motion Blur", motion_blur_texts,
												3, 65, 0);
	motion_blur_amount_changed =
		XwaModernOptionsMenu_DrawSliderU8(&menu, &motion_blur_amount, "Motion Blur Amount", "0%", "100%", 10,
										  motion_blur_quality == XWA_MODERN_MOTION_BLUR_OFF);
	changed |= motion_blur_amount_changed;
	/* Greyed while the display cannot present HDR (OS HDR disabled or the
	 * display is not HDR-capable — the backend cannot distinguish the two). */
	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &hdr, "HDR Output", toggle_texts, 2, 66,
												!Aeron_OutputSupportsHdr());
#if defined(__APPLE__)
	/* Fixed platform behavior (piecewise sRGB, EDR white following system
	 * brightness); shown for parity with other platforms. */
	hdr_rows_disabled = 1;
#else
	/* Greyed while the HDR composition is not actually running (HDR Output
	 * off, or the display is in SDR mode): SDR presentation is byte-exact,
	 * so neither choice has an effect there. Enabling HDR Output above
	 * un-greys the rows once the deferred swapchain flip applies. */
	hdr_rows_disabled = !Aeron_OutputHdrEnabled();
#endif
	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &sdr_gamma, "SDR Content Gamma", sdr_gamma_texts, 2,
												67, hdr_rows_disabled);
	changed |= XwaModernOptionsMenu_DrawCycleU8(&menu, &paper_white, "HDR Paper White", paper_white_texts, 7,
												68, hdr_rows_disabled);

	if (changed) {
		if (msaa != original_msaa && msaa != XWA_MODERN_MSAA_OFF) {
			fsr = XWA_MODERN_FSR_OFF;
		} else if (fsr != original_fsr && fsr != XWA_MODERN_FSR_OFF) {
			msaa = XWA_MODERN_MSAA_OFF;
		}
		options.window_mode = (XwaModernWindowMode)window_mode;
		options.ssao_quality = (XwaModernSsaoQuality)ssao;
		options.shadow_quality = (XwaModernShadowQuality)shadow_quality;
		options.fsr_upscaling = (XwaModernFsrUpscaling)fsr;
		options.msaa = (XwaModernMsaa)msaa;
		options.motion_blur_quality = (XwaModernMotionBlurQuality)motion_blur_quality;
		if (motion_blur_amount_changed) {
			options.motion_blur_amount = (float)motion_blur_amount / 10.0f;
		}
		options.hdr_output = hdr != 0;
		options.sdr_gamma = (XwaModernSdrGamma)sdr_gamma;
		options.paper_white = (XwaModernPaperWhite)paper_white;
		XwaModernVideoOptions_Set(&options);
	}

	back = XwaModernOptionsMenu_DrawAction(&menu, FrontendString_Get(STR_BACK), 69, 0);
	back |= XwaModernOptionsMenu_TakeEscape(&menu);
	if (!back) {
		return 0;
	}

	XwaModernVideoOptions_Flush();
	return 1;
}
