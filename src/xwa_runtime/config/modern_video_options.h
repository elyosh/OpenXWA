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

/* Low draws the fog with a formula: no clouds, no clear space around the
 * cockpit, and lasers and explosions don't light it. Medium and High fill a
 * grid of 80 or 160 cells across the view (AeronScene_SetFogQuality). */
typedef enum XwaModernFogQuality {
	XWA_MODERN_FOG_LOW = 0,
	XWA_MODERN_FOG_MEDIUM,
	XWA_MODERN_FOG_HIGH,
} XwaModernFogQuality;

typedef enum XwaModernWindowMode {
	XWA_MODERN_WINDOW_MODE_WINDOWED = 0,
	XWA_MODERN_WINDOW_MODE_FULLSCREEN,
	/* One borderless window over the displays that are next to each other. */
	XWA_MODERN_WINDOW_MODE_SPAN,
} XwaModernWindowMode;

/* Whether the game starts in VR. It is read when the game starts, so a change
 * applies at the next start. */
typedef enum XwaModernVrMode {
	XWA_MODERN_VR_OFF = 0,
	/* VR. When it can't start, the game says why and starts in the window. */
	XWA_MODERN_VR_ON,
} XwaModernVrMode;

/* A surround setup: three screens of the same size side by side, the side
 * screens turned toward the player. With it, the flight view is drawn once
 * for each screen, with that screen's own perspective. The sizes are in
 * millimetres, as the player measures them. */
typedef struct XwaModernSurroundOptions {
	int enabled;
	/* The size of one screen's picture, without its frame. */
	float screen_width_mm;
	float screen_height_mm;
	/* The width of a screen's frame at its side. */
	float bezel_mm;
	/* From the player's eyes to the centre screen. */
	float distance_mm;
	/* How far each side screen is turned toward the player. 0 is flat. */
	float side_angle_deg;
} XwaModernSurroundOptions;

#define XWA_MODERN_SURROUND_SIZE_MIN_MM 100.0f
#define XWA_MODERN_SURROUND_SIZE_MAX_MM 3000.0f
#define XWA_MODERN_SURROUND_BEZEL_MAX_MM 100.0f
#define XWA_MODERN_SURROUND_DISTANCE_MIN_MM 200.0f
#define XWA_MODERN_SURROUND_DISTANCE_MAX_MM 3000.0f
#define XWA_MODERN_SURROUND_SIDE_ANGLE_MAX_DEG 85.0f

/* Fills `out` with a setup of three 27-inch 16:9 screens. */
void XwaModernSurroundOptions_Default(XwaModernSurroundOptions* out);
int XwaModernSurroundOptions_Valid(const XwaModernSurroundOptions* options);

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
	XwaModernFogQuality fog_quality;
	float motion_blur_amount; /* normalized shutter: 0.5 = 180 degrees */
	/* The strength of the bloom, from 0 to XWA_MODERN_BLOOM_AMOUNT_MAX. At 1,
	   it is the shipped bloom. It also scales the glow of projectiles. */
	float bloom_amount;
	int hdr_output;
	XwaModernSdrGamma sdr_gamma;
	XwaModernPaperWhite paper_white;
	/* The size of the modern HUD, from XWA_MODERN_HUD_SCALE_MIN to
	   XWA_MODERN_HUD_SCALE_MAX. At 1, the HUD has the size that the stock
	   game gives it at most resolutions, in any window. */
	float hud_scale;
	XwaModernSurroundOptions surround;
	XwaModernVrMode vr_mode;
	/* The size of the headset's eye images: the size that the headset
	   recommends, times this scale. It is read when the game starts. */
	float vr_render_scale;
	/* How far the view moves in the game's world when the head moves one
	   metre, in metres. Above 1, the world looks smaller. */
	float vr_world_scale;
	/* Show what the headset shows in the window. */
	int vr_mirror;
	/* How large the cockpit panel that shows the HUD looks in a headset. At
	   1 it covers the same part of the view as the HUD does on a screen. */
	float vr_panel_scale;
	/* How large the flat screen that shows the menus, the briefings and the
	   movies looks in a headset, as a multiple of its size in the game's
	   files. */
	float vr_screen_scale;
} XwaModernVideoOptions;

#define XWA_MODERN_VR_SCALE_MIN 0.5f
#define XWA_MODERN_VR_SCALE_MAX 2.0f

#define XWA_MODERN_VR_PANEL_SCALE_MIN 0.25f
#define XWA_MODERN_VR_PANEL_SCALE_MAX 2.0f

#define XWA_MODERN_HUD_SCALE_MIN 0.5f
#define XWA_MODERN_HUD_SCALE_MAX 2.0f

#define XWA_MODERN_BLOOM_AMOUNT_MAX 1.5f

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
	XWA_MODERN_VIDEO_OVERRIDE_HUD_SCALE = 1u << 10,
	XWA_MODERN_VIDEO_OVERRIDE_SURROUND = 1u << 11,
	XWA_MODERN_VIDEO_OVERRIDE_SURROUND_SCREEN_WIDTH = 1u << 12,
	XWA_MODERN_VIDEO_OVERRIDE_SURROUND_SCREEN_HEIGHT = 1u << 13,
	XWA_MODERN_VIDEO_OVERRIDE_SURROUND_BEZEL = 1u << 14,
	XWA_MODERN_VIDEO_OVERRIDE_SURROUND_DISTANCE = 1u << 15,
	XWA_MODERN_VIDEO_OVERRIDE_SURROUND_SIDE_ANGLE = 1u << 16,
	XWA_MODERN_VIDEO_OVERRIDE_VR_MODE = 1u << 17,
	XWA_MODERN_VIDEO_OVERRIDE_VR_RENDER_SCALE = 1u << 18,
	XWA_MODERN_VIDEO_OVERRIDE_VR_WORLD_SCALE = 1u << 19,
	XWA_MODERN_VIDEO_OVERRIDE_VR_MIRROR = 1u << 20,
	XWA_MODERN_VIDEO_OVERRIDE_VR_PANEL_SCALE = 1u << 21,
	XWA_MODERN_VIDEO_OVERRIDE_VR_SCREEN_SCALE = 1u << 22,
	XWA_MODERN_VIDEO_OVERRIDE_BLOOM_AMOUNT = 1u << 23,
	XWA_MODERN_VIDEO_OVERRIDE_FOG_QUALITY = 1u << 24,
};

typedef void (*XwaModernVideoOptionsApplyFn)(const XwaModernVideoOptions* options);
typedef int (*XwaModernVideoOptionsPersistFn)(const XwaModernVideoOptions* options, char* error,
											  size_t error_size);

void XwaModernVideoOptions_Configure(const XwaModernVideoOptions* options, XwaModernVideoOptionsApplyFn apply,
									 XwaModernVideoOptionsPersistFn persist);
void XwaModernVideoOptions_Get(XwaModernVideoOptions* out);
int XwaModernVideoOptions_Set(const XwaModernVideoOptions* options);
/* Save the options if they changed. On failure, FlushWithError writes the
   reason in `error`, and Flush logs it. */
int XwaModernVideoOptions_FlushWithError(char* error, size_t error_size);
int XwaModernVideoOptions_Flush(void);
int XwaModernVideoOptions_IsDirty(void);

#ifdef __cplusplus
}
#endif

#endif
