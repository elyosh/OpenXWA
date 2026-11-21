#include "xwa_runtime/config/modern_video_options.h"

#include "aeron/log.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static struct {
	XwaModernVideoOptions options;
	XwaModernVideoOptionsApplyFn apply;
	XwaModernVideoOptionsPersistFn persist;
	int configured;
	int dirty;
} g_modernVideoOptions;

void XwaModernSurroundOptions_Default(XwaModernSurroundOptions* out) {
	*out = (XwaModernSurroundOptions) {
		.enabled = 0,
		.screen_width_mm = 597.0f,
		.screen_height_mm = 336.0f,
		.bezel_mm = 8.0f,
		.distance_mm = 700.0f,
		.side_angle_deg = 45.0f,
	};
}

static int surround_in_range(float value, float min, float max) {
	return isfinite(value) && value >= min && value <= max;
}

int XwaModernSurroundOptions_Valid(const XwaModernSurroundOptions* options) {
	return options &&
		   surround_in_range(options->screen_width_mm, XWA_MODERN_SURROUND_SIZE_MIN_MM,
							 XWA_MODERN_SURROUND_SIZE_MAX_MM) &&
		   surround_in_range(options->screen_height_mm, XWA_MODERN_SURROUND_SIZE_MIN_MM,
							 XWA_MODERN_SURROUND_SIZE_MAX_MM) &&
		   surround_in_range(options->bezel_mm, 0.0f, XWA_MODERN_SURROUND_BEZEL_MAX_MM) &&
		   surround_in_range(options->distance_mm, XWA_MODERN_SURROUND_DISTANCE_MIN_MM,
							 XWA_MODERN_SURROUND_DISTANCE_MAX_MM) &&
		   surround_in_range(options->side_angle_deg, 0.0f, XWA_MODERN_SURROUND_SIDE_ANGLE_MAX_DEG);
}

static int XwaModernVideoOptions_IsValid(const XwaModernVideoOptions* options) {
	return options && options->window_mode >= XWA_MODERN_WINDOW_MODE_WINDOWED &&
		   options->window_mode <= XWA_MODERN_WINDOW_MODE_SPAN &&
		   XwaModernSurroundOptions_Valid(&options->surround) && options->vr_mode >= XWA_MODERN_VR_OFF &&
		   options->vr_mode <= XWA_MODERN_VR_ON &&
		   surround_in_range(options->vr_render_scale, XWA_MODERN_VR_SCALE_MIN, XWA_MODERN_VR_SCALE_MAX) &&
		   surround_in_range(options->vr_world_scale, XWA_MODERN_VR_SCALE_MIN, XWA_MODERN_VR_SCALE_MAX) &&
		   surround_in_range(options->vr_panel_scale, XWA_MODERN_VR_PANEL_SCALE_MIN,
							 XWA_MODERN_VR_PANEL_SCALE_MAX) &&
		   surround_in_range(options->vr_screen_scale, XWA_MODERN_VR_SCALE_MIN, XWA_MODERN_VR_SCALE_MAX) &&
		   options->ssao_quality >= XWA_MODERN_SSAO_OFF && options->ssao_quality <= XWA_MODERN_SSAO_HIGH &&
		   options->shadow_quality >= XWA_MODERN_SHADOW_STANDARD &&
		   options->shadow_quality <= XWA_MODERN_SHADOW_HIGH &&
		   options->fsr_upscaling >= XWA_MODERN_FSR_OFF &&
		   options->fsr_upscaling <= XWA_MODERN_FSR_NATIVE_AA && options->msaa >= XWA_MODERN_MSAA_OFF &&
		   options->msaa <= XWA_MODERN_MSAA_8X &&
		   (options->fsr_upscaling == XWA_MODERN_FSR_OFF || options->msaa == XWA_MODERN_MSAA_OFF) &&
		   options->motion_blur_quality >= XWA_MODERN_MOTION_BLUR_OFF &&
		   options->motion_blur_quality <= XWA_MODERN_MOTION_BLUR_HIGH &&
		   isfinite(options->motion_blur_amount) && options->motion_blur_amount >= 0.0f &&
		   options->motion_blur_amount <= 1.0f && isfinite(options->bloom_amount) &&
		   options->bloom_amount >= 0.0f && options->bloom_amount <= XWA_MODERN_BLOOM_AMOUNT_MAX &&
		   options->sdr_gamma >= XWA_MODERN_SDR_GAMMA_2_2 &&
		   options->sdr_gamma <= XWA_MODERN_SDR_GAMMA_SRGB &&
		   options->paper_white >= XWA_MODERN_PAPER_WHITE_AUTO &&
		   options->paper_white <= XWA_MODERN_PAPER_WHITE_400 && isfinite(options->hud_scale) &&
		   options->hud_scale >= XWA_MODERN_HUD_SCALE_MIN && options->hud_scale <= XWA_MODERN_HUD_SCALE_MAX;
}

static int XwaModernVideoOptions_AreEqual(const XwaModernVideoOptions* lhs,
										  const XwaModernVideoOptions* rhs) {
	return lhs->window_mode == rhs->window_mode && lhs->ssao_quality == rhs->ssao_quality &&
		   lhs->shadow_quality == rhs->shadow_quality && lhs->fsr_upscaling == rhs->fsr_upscaling &&
		   lhs->msaa == rhs->msaa && lhs->motion_blur_quality == rhs->motion_blur_quality &&
		   lhs->motion_blur_amount == rhs->motion_blur_amount && lhs->bloom_amount == rhs->bloom_amount &&
		   lhs->hdr_output == rhs->hdr_output && lhs->sdr_gamma == rhs->sdr_gamma &&
		   lhs->paper_white == rhs->paper_white && lhs->hud_scale == rhs->hud_scale &&
		   lhs->surround.enabled == rhs->surround.enabled &&
		   lhs->surround.screen_width_mm == rhs->surround.screen_width_mm &&
		   lhs->surround.screen_height_mm == rhs->surround.screen_height_mm &&
		   lhs->surround.bezel_mm == rhs->surround.bezel_mm &&
		   lhs->surround.distance_mm == rhs->surround.distance_mm &&
		   lhs->surround.side_angle_deg == rhs->surround.side_angle_deg && lhs->vr_mode == rhs->vr_mode &&
		   lhs->vr_render_scale == rhs->vr_render_scale && lhs->vr_world_scale == rhs->vr_world_scale &&
		   lhs->vr_mirror == rhs->vr_mirror && lhs->vr_panel_scale == rhs->vr_panel_scale &&
		   lhs->vr_screen_scale == rhs->vr_screen_scale;
}

void XwaModernVideoOptions_Configure(const XwaModernVideoOptions* options, XwaModernVideoOptionsApplyFn apply,
									 XwaModernVideoOptionsPersistFn persist) {
	memset(&g_modernVideoOptions, 0, sizeof g_modernVideoOptions);
	if (!XwaModernVideoOptions_IsValid(options)) {
		Aeron_LogError("xwa.config", "cannot configure invalid modern video options");
		return;
	}

	g_modernVideoOptions.options = *options;
	g_modernVideoOptions.options.hdr_output = options->hdr_output != 0;
	g_modernVideoOptions.options.surround.enabled = options->surround.enabled != 0;
	g_modernVideoOptions.options.vr_mirror = options->vr_mirror != 0;
	g_modernVideoOptions.apply = apply;
	g_modernVideoOptions.persist = persist;
	g_modernVideoOptions.configured = 1;
}

void XwaModernVideoOptions_Get(XwaModernVideoOptions* out) {
	if (out) {
		*out = g_modernVideoOptions.options;
	}
}

int XwaModernVideoOptions_Set(const XwaModernVideoOptions* options) {
	XwaModernVideoOptions normalized;

	if (!g_modernVideoOptions.configured || !XwaModernVideoOptions_IsValid(options)) {
		return 0;
	}
	normalized = *options;
	normalized.hdr_output = normalized.hdr_output != 0;
	normalized.surround.enabled = normalized.surround.enabled != 0;
	normalized.vr_mirror = normalized.vr_mirror != 0;
	if (XwaModernVideoOptions_AreEqual(&normalized, &g_modernVideoOptions.options)) {
		return 1;
	}

	g_modernVideoOptions.options = normalized;
	g_modernVideoOptions.dirty = 1;
	if (g_modernVideoOptions.apply) {
		g_modernVideoOptions.apply(&g_modernVideoOptions.options);
	}
	return 1;
}

int XwaModernVideoOptions_FlushWithError(char* error, size_t error_size) {
	if (error && error_size) {
		error[0] = '\0';
	}
	if (!g_modernVideoOptions.configured || !g_modernVideoOptions.dirty) {
		return 1;
	}
	if (!g_modernVideoOptions.persist) {
		if (error && error_size) {
			snprintf(error, error_size, "%s", "the video options changed, but nothing saves them");
		}
		return 0;
	}
	if (!g_modernVideoOptions.persist(&g_modernVideoOptions.options, error, error_size)) {
		if (error && error_size && !error[0]) {
			snprintf(error, error_size, "%s", "could not save the video options to config.yaml");
		}
		return 0;
	}
	g_modernVideoOptions.dirty = 0;
	return 1;
}

int XwaModernVideoOptions_Flush(void) {
	char error[512];

	if (!XwaModernVideoOptions_FlushWithError(error, sizeof error)) {
		Aeron_LogError("xwa.config", "%s", error);
		return 0;
	}
	return 1;
}

int XwaModernVideoOptions_IsDirty(void) { return g_modernVideoOptions.dirty; }
