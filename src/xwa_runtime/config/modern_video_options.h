#ifndef XWA_RUNTIME_MODERN_VIDEO_OPTIONS_H
#define XWA_RUNTIME_MODERN_VIDEO_OPTIONS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XwaModernSsaoQuality {
	XWA_MODERN_SSAO_OFF = 0,
	XWA_MODERN_SSAO_LOW,
	XWA_MODERN_SSAO_HIGH,
} XwaModernSsaoQuality;

typedef enum XwaModernShadowQuality {
	XWA_MODERN_SHADOW_STANDARD = 0, /* 4096 directional-shadow atlas */
	XWA_MODERN_SHADOW_HIGH,         /* 8192 directional-shadow atlas */
} XwaModernShadowQuality;

typedef enum XwaModernFsrUpscaling {
	XWA_MODERN_FSR_OFF = 0,
	XWA_MODERN_FSR_PERFORMANCE,
	XWA_MODERN_FSR_BALANCED,
	XWA_MODERN_FSR_QUALITY,
	XWA_MODERN_FSR_NATIVE_AA,
} XwaModernFsrUpscaling;

typedef enum XwaModernMsaa {
	XWA_MODERN_MSAA_OFF = 0,
	XWA_MODERN_MSAA_2X,
	XWA_MODERN_MSAA_4X,
	XWA_MODERN_MSAA_8X,
} XwaModernMsaa;

typedef enum XwaModernMotionBlurQuality {
	XWA_MODERN_MOTION_BLUR_OFF = 0,
	XWA_MODERN_MOTION_BLUR_LOW,
	XWA_MODERN_MOTION_BLUR_HIGH,
} XwaModernMotionBlurQuality;

typedef enum XwaModernWindowMode {
	XWA_MODERN_WINDOW_MODE_WINDOWED = 0,
	XWA_MODERN_WINDOW_MODE_FULLSCREEN,
} XwaModernWindowMode;

/* Decode gamma for SDR-authored content under HDR output. SRGB (the piecewise
 * curve) is last so the in-game cycle covers only 2.2/2.4: it is accepted from
 * config.yaml and is the fixed platform behavior on Apple, but is not offered
 * interactively — under Windows HDR it reproduces DWM's washed-out SDR look. */
typedef enum XwaModernSdrGamma {
	XWA_MODERN_SDR_GAMMA_2_2 = 0,
	XWA_MODERN_SDR_GAMMA_2_4,
	XWA_MODERN_SDR_GAMMA_SRGB,
} XwaModernSdrGamma;

/* Reference white for HDR output. AUTO follows the OS SDR white level (the
 * Windows SDR-content-brightness slider); the fixed steps are in nits. */
typedef enum XwaModernPaperWhite {
	XWA_MODERN_PAPER_WHITE_AUTO = 0,
	XWA_MODERN_PAPER_WHITE_100,
	XWA_MODERN_PAPER_WHITE_150,
	XWA_MODERN_PAPER_WHITE_200,
	XWA_MODERN_PAPER_WHITE_250,
	XWA_MODERN_PAPER_WHITE_300,
	XWA_MODERN_PAPER_WHITE_400,
} XwaModernPaperWhite;

typedef struct XwaModernVideoOptions {
	XwaModernWindowMode window_mode;
	XwaModernSsaoQuality ssao_quality;
	XwaModernShadowQuality shadow_quality;
	XwaModernFsrUpscaling fsr_upscaling;
	XwaModernMsaa msaa;
	XwaModernMotionBlurQuality motion_blur_quality;
	float motion_blur_amount; /* normalized shutter: 0.5 = 180 degrees */
	int hdr_output;
	XwaModernSdrGamma sdr_gamma;
	XwaModernPaperWhite paper_white;
} XwaModernVideoOptions;

enum {
	XWA_MODERN_VIDEO_OVERRIDE_SSAO = 1u << 0,
	XWA_MODERN_VIDEO_OVERRIDE_FSR = 1u << 1,
	XWA_MODERN_VIDEO_OVERRIDE_MOTION_BLUR = 1u << 2,
	XWA_MODERN_VIDEO_OVERRIDE_HDR = 1u << 3,
	XWA_MODERN_VIDEO_OVERRIDE_WINDOW_MODE = 1u << 4,
	XWA_MODERN_VIDEO_OVERRIDE_MSAA = 1u << 5,
	XWA_MODERN_VIDEO_OVERRIDE_SDR_GAMMA = 1u << 6,
	XWA_MODERN_VIDEO_OVERRIDE_PAPER_WHITE = 1u << 7,
	XWA_MODERN_VIDEO_OVERRIDE_MOTION_BLUR_AMOUNT = 1u << 8,
	XWA_MODERN_VIDEO_OVERRIDE_SHADOW_QUALITY = 1u << 9,
};

typedef void (*XwaModernVideoOptionsApplyFn)(const XwaModernVideoOptions* options);
typedef int (*XwaModernVideoOptionsPersistFn)(const XwaModernVideoOptions* options, char* error,
											  size_t error_size);

void XwaModernVideoOptions_Configure(const XwaModernVideoOptions* options, XwaModernVideoOptionsApplyFn apply,
									 XwaModernVideoOptionsPersistFn persist);
void XwaModernVideoOptions_Get(XwaModernVideoOptions* out);
int XwaModernVideoOptions_Set(const XwaModernVideoOptions* options);
int XwaModernVideoOptions_Flush(void);
int XwaModernVideoOptions_IsDirty(void);

#ifdef __cplusplus
}
#endif

#endif
