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

typedef struct XwaModernVideoOptions {
	XwaModernWindowMode window_mode;
	XwaModernSsaoQuality ssao_quality;
	XwaModernFsrUpscaling fsr_upscaling;
	XwaModernMsaa msaa;
	XwaModernMotionBlurQuality motion_blur_quality;
	int hdr_output;
} XwaModernVideoOptions;

enum {
	XWA_MODERN_VIDEO_OVERRIDE_SSAO = 1u << 0,
	XWA_MODERN_VIDEO_OVERRIDE_FSR = 1u << 1,
	XWA_MODERN_VIDEO_OVERRIDE_MOTION_BLUR = 1u << 2,
	XWA_MODERN_VIDEO_OVERRIDE_HDR = 1u << 3,
	XWA_MODERN_VIDEO_OVERRIDE_WINDOW_MODE = 1u << 4,
	XWA_MODERN_VIDEO_OVERRIDE_MSAA = 1u << 5,
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
