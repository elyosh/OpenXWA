#include "xwa_runtime/config/modern_video_options.h"

#include "aeron/log.h"

#include <string.h>

static struct {
	XwaModernVideoOptions options;
	XwaModernVideoOptionsApplyFn apply;
	XwaModernVideoOptionsPersistFn persist;
	int configured;
	int dirty;
} g_modernVideoOptions;

static int XwaModernVideoOptions_IsValid(const XwaModernVideoOptions* options) {
	return options && options->window_mode >= XWA_MODERN_WINDOW_MODE_WINDOWED &&
		   options->window_mode <= XWA_MODERN_WINDOW_MODE_FULLSCREEN &&
		   options->ssao_quality >= XWA_MODERN_SSAO_OFF && options->ssao_quality <= XWA_MODERN_SSAO_HIGH &&
		   options->fsr_upscaling >= XWA_MODERN_FSR_OFF &&
		   options->fsr_upscaling <= XWA_MODERN_FSR_NATIVE_AA && options->msaa >= XWA_MODERN_MSAA_OFF &&
		   options->msaa <= XWA_MODERN_MSAA_8X &&
		   (options->fsr_upscaling == XWA_MODERN_FSR_OFF || options->msaa == XWA_MODERN_MSAA_OFF) &&
		   options->motion_blur_quality >= XWA_MODERN_MOTION_BLUR_OFF &&
		   options->motion_blur_quality <= XWA_MODERN_MOTION_BLUR_HIGH &&
		   options->sdr_gamma >= XWA_MODERN_SDR_GAMMA_2_2 && options->sdr_gamma <= XWA_MODERN_SDR_GAMMA_SRGB &&
		   options->paper_white >= XWA_MODERN_PAPER_WHITE_AUTO &&
		   options->paper_white <= XWA_MODERN_PAPER_WHITE_400;
}

static int XwaModernVideoOptions_AreEqual(const XwaModernVideoOptions* lhs,
										  const XwaModernVideoOptions* rhs) {
	return lhs->window_mode == rhs->window_mode && lhs->ssao_quality == rhs->ssao_quality &&
		   lhs->fsr_upscaling == rhs->fsr_upscaling && lhs->msaa == rhs->msaa &&
		   lhs->motion_blur_quality == rhs->motion_blur_quality && lhs->hdr_output == rhs->hdr_output &&
		   lhs->sdr_gamma == rhs->sdr_gamma && lhs->paper_white == rhs->paper_white;
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

int XwaModernVideoOptions_Flush(void) {
	char error[512];

	if (!g_modernVideoOptions.configured || !g_modernVideoOptions.dirty) {
		return 1;
	}
	if (!g_modernVideoOptions.persist) {
		Aeron_LogWarn("xwa.config", "modern video options are dirty but no persistence callback is registered");
		return 0;
	}
	error[0] = '\0';
	if (!g_modernVideoOptions.persist(&g_modernVideoOptions.options, error, sizeof error)) {
		Aeron_LogError("xwa.config", "%s",
				  error[0] ? error : "could not persist modern video options to user configuration");
		return 0;
	}

	g_modernVideoOptions.dirty = 0;
	return 1;
}

int XwaModernVideoOptions_IsDirty(void) { return g_modernVideoOptions.dirty; }
