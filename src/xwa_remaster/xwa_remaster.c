/*
 * XWA remaster driver entry — see xwa_remaster.h.
 *
 * Owns the cross-scene state: the shared asset resolver, the
 * HD / SPLIT / CLASSIC view mode + primary-mode fade ramp, and the overlay
 * layer submission. Scene rendering dispatches per snapshot
 * scene_kind to the peer drivers:
 *   FRONTEND -> frontend.c (2D reconstruction)
 *   LOADING  -> frontend.c (last presented 2D loading frame)
 *   FRONTEND_MODAL -> frontend.c (in-flight options/name-prompt UI)
 *   FLIGHT   -> flight.c   (full-frame 3D scene)
 *   CUTSCENE -> cutscene.c (transparent HD subtitle overlay)
 * Any other kind (or an unrenderable mode) submits nothing — the
 * classic layers show through.
 */

#include "xwa_remaster/xwa_remaster.h"

#include "aeron/aeron.h"
#include "aeron/debug.h"
#include "aeron/image.h"
#include "aeron/asset/opt_model.h"
#include "aeron/scene/blend_ramp.h"
#include "aeron/scene/view_rig.h"
#include "xwa_remaster/assets.h"
#include "xwa_remaster/captured_lighting.h"
#include "xwa_remaster/cutscene.h"
#include "xwa_remaster/debug_tools.h"
#include "xwa_remaster/env_capture.h"
#include "xwa_remaster/environment.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/frontend.h"
#include "xwa_remaster/hud.h"
#include "xwa_remaster/opt_mesh.h"
#include "xwa_remaster/ship.h"
#include "xwa_runtime/frontend/frontend_geometry.h"
#include "xwa_runtime/input/capture.h"
#include "xwa_runtime/input/keyboard_mapping.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/runtime/film_benchmark.h"
#include "xwa_runtime/runtime/movie_task.h"
#include "xwa_runtime/runtime/port.h"
#include "xwa_runtime/runtime/presentation.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RM_RESIZE_SETTLE_US 150000u
#define RM_ASSET_UPLOAD_BYTE_BUDGET (64u * 1024u * 1024u)
#define RM_ASSET_UPLOAD_COPY_BUDGET 4096u
#define RM_SHADOW_ATLAS_STANDARD 4096
#define RM_SHADOW_ATLAS_HIGH 8192

typedef enum {
	RM_VIEW_HD = 0, /* start on the reconstruction */
	RM_VIEW_SPLIT,
	RM_VIEW_CLASSIC
} RmViewMode;

/* What the flight scene is rendered into: one view, or a rig of views. */
typedef enum { RM_RIG_NONE = 0, RM_RIG_SURROUND, RM_RIG_DEBUG_STEREO, RM_RIG_HEADSET } RmRigKind;

static struct {
	int initialized;
	XwaRemasterAssets* assets;
	AeronBlendRamp ramp;
	RmViewMode mode;
	RmViewMode primary_mode;
	uint64_t last_tick;
	XwaSceneKind last_kind;
	/* Last scene render, re-submitted on host frames between sim
	 * ticks (both drivers return persistent textures). */
	AeronTexture* scene_tex;
	XwaSceneKind scene_kind;
	int scene_is_direct;
	/* Nonzero when scene_tex holds tonemapped scene output (flight),
	 * whose display model the tonemapper already applied; zero for
	 * decoded sRGB art (frontend, cutscene subtitles), which presents
	 * as LINEAR_DISPLAY so HDR output can remap its display gamma. */
	int scene_is_tonemapped;
	/* HDR output: desired flag + deferred apply (see the header). */
	int hdr_desired;
	int hdr_apply_pending;
	int pending_mode_valid;
	RmViewMode pending_mode;
	int pending_mode_fade;
	uint64_t pending_classic_frame_serial;
	int classic_suppressed_this_frame;
	int frontend_wide; /* the last rendered frontend picture used the wide layout */
	int render_pixel_width;
	int render_pixel_height;
	int observed_pixel_width;
	int observed_pixel_height;
	/* Set by --render-size: the render keeps its size when the window
	   changes. */
	int render_size_fixed;
	uint64_t observed_size_since_us;
	int force_scene_render;
	int flight_render_suspended;
	int hyperspace_preview_last;
	AeronSampleCount msaa_samples;
	XwaModernSurroundOptions surround;
	/* --flight-views surround: surround is on for this run. */
	int surround_forced;
	XwaRemasterDebugStereo debug_stereo;
	/* The rig given to the flight renderer, and whether its poses change
	 * from frame to frame. */
	RmRigKind rig_kind;
	int rig_view_width;
	int rig_view_height;
	int rig_tracked;
	uint64_t rig_generation;
	/* The surround settings that couldn't be used, so the reason is logged
	 * once and not on every frame. */
	XwaModernSurroundOptions surround_refused;
	/* The headset: how far the view moves for one metre of head movement,
	 * and whether the window shows what the headset shows. */
	float vr_world_scale;
	int vr_mirror;
	/* The headset: how large the HUD's cockpit panel and the virtual
	 * screen look. */
	float vr_panel_scale;
	float vr_screen_scale;
	/* This frame has eye images from Aeron (Aeron_XrBeginViews). */
	int xr_views_ready;
	AeronViewRig headset_rig;
	uint64_t headset_generation;
	AeronRenderTarget* xr_targets[AERON_MAX_VIEWS];
	AeronXrAreaMesh xr_hidden_areas[AERON_MAX_VIEWS];
	AeronXrAreaMesh xr_visible_areas[AERON_MAX_VIEWS];
	/* The last flight frame went to the headset's eye images. */
	int scene_is_headset;
	/* The last frame could not be rendered yet: models are still loading,
	 * or the headset gave no eye images. */
	int frame_waits;
	int surround_refused_width;
	/* The mouse position in the window, from -1 at the left and top edges to
	 * 1 at the right and bottom edges. */
	float mouse_x;
	float mouse_y;
} g;

/* Tells the presentation code how the frontend is shown, so the picture and
 * the mouse mapping agree: with the design screen's shape when the HD
 * frontend is shown, else as the classic 640x480 surface. */
static void XwaRemaster_SetFrontendShape(int hd_shown) {
	float width = 0.0f, height = 0.0f;
	if (hd_shown)
		XwaFrontendGeometry_DesignSize(&width, &height);
	XwaPresentation_SetFrontendShape((int)width, (int)height, hd_shown && g.frontend_wide);
}

static int XwaRemaster_SnapshotHasPresent(const XwaSnapshot* snap) {
	for (uint32_t i = 0; i < snap->surface_event_count; i++) {
		if (snap->surface_events[i].kind == XWA_SURFACE_EVENT_PRESENT) {
			return 1;
		}
	}
	return 0;
}

void XwaRemaster_SetHdrDesired(int want) {
	want = want ? 1 : 0;
	if (want != g.hdr_desired) {
		g.hdr_desired = want;
		g.hdr_apply_pending = 1;
	}
}

int XwaRemaster_GetHdrDesired(void) { return g.hdr_desired; }

AeronSampleCount XwaRemaster_MsaaSampleCount(void) { return g.msaa_samples; }

static AeronSampleCount XwaRemaster_ToSampleCount(XwaModernMsaa msaa) {
	switch (msaa) {
		case XWA_MODERN_MSAA_2X:
			return AERON_SAMPLE_COUNT_2;
		case XWA_MODERN_MSAA_4X:
			return AERON_SAMPLE_COUNT_4;
		case XWA_MODERN_MSAA_8X:
			return AERON_SAMPLE_COUNT_8;
		case XWA_MODERN_MSAA_OFF:
		default:
			return AERON_SAMPLE_COUNT_1;
	}
}

static XwaModernMsaa XwaRemaster_FromSampleCount(AeronSampleCount sample_count) {
	switch (sample_count) {
		case AERON_SAMPLE_COUNT_2:
			return XWA_MODERN_MSAA_2X;
		case AERON_SAMPLE_COUNT_4:
			return XWA_MODERN_MSAA_4X;
		case AERON_SAMPLE_COUNT_8:
			return XWA_MODERN_MSAA_8X;
		case AERON_SAMPLE_COUNT_1:
		default:
			return XWA_MODERN_MSAA_OFF;
	}
}

static AeronTemporalMode XwaRemaster_ToTemporalMode(XwaModernFsrUpscaling mode) {
	switch (mode) {
		case XWA_MODERN_FSR_PERFORMANCE:
			return AERON_TEMPORAL_PERFORMANCE;
		case XWA_MODERN_FSR_BALANCED:
			return AERON_TEMPORAL_BALANCED;
		case XWA_MODERN_FSR_QUALITY:
			return AERON_TEMPORAL_QUALITY;
		case XWA_MODERN_FSR_NATIVE_AA:
			return AERON_TEMPORAL_NATIVE_AA;
		case XWA_MODERN_FSR_OFF:
		default:
			return AERON_TEMPORAL_OFF;
	}
}

static XwaModernFsrUpscaling XwaRemaster_FromTemporalMode(AeronTemporalMode mode) {
	switch (mode) {
		case AERON_TEMPORAL_PERFORMANCE:
			return XWA_MODERN_FSR_PERFORMANCE;
		case AERON_TEMPORAL_BALANCED:
			return XWA_MODERN_FSR_BALANCED;
		case AERON_TEMPORAL_QUALITY:
			return XWA_MODERN_FSR_QUALITY;
		case AERON_TEMPORAL_NATIVE_AA:
			return XWA_MODERN_FSR_NATIVE_AA;
		case AERON_TEMPORAL_OFF:
		default:
			return XWA_MODERN_FSR_OFF;
	}
}

/* Aeron's decode gamma (0 = piecewise) <-> the discrete option values. */
static XwaModernSdrGamma XwaRemaster_SdrGammaFromDecode(float gamma) {
	if (gamma <= 0.0f) {
		return XWA_MODERN_SDR_GAMMA_SRGB;
	}
	return gamma >= 2.3f ? XWA_MODERN_SDR_GAMMA_2_4 : XWA_MODERN_SDR_GAMMA_2_2;
}

static float XwaRemaster_SdrGammaToDecode(XwaModernSdrGamma gamma) {
	switch (gamma) {
		case XWA_MODERN_SDR_GAMMA_2_2:
			return 2.2f;
		case XWA_MODERN_SDR_GAMMA_2_4:
			return 2.4f;
		case XWA_MODERN_SDR_GAMMA_SRGB:
			break;
	}
	return 0.0f;
}

/* Aeron's paper white (nits, 0 = follow OS SDR white) <-> the option steps. */
static const float g_paperWhiteNits[] = { 0.0f, 100.0f, 150.0f, 200.0f, 250.0f, 300.0f, 400.0f };

static XwaModernPaperWhite XwaRemaster_PaperWhiteFromNits(float nits) {
	size_t best = 1;
	size_t i;

	if (nits <= 0.0f) {
		return XWA_MODERN_PAPER_WHITE_AUTO;
	}
	/* Snap to the nearest step; only exact values round-trip from the store,
	 * but the debug tool can leave arbitrary nits in Aeron. */
	for (i = 2; i < sizeof g_paperWhiteNits / sizeof g_paperWhiteNits[0]; ++i) {
		if (fabsf(g_paperWhiteNits[i] - nits) < fabsf(g_paperWhiteNits[best] - nits)) {
			best = i;
		}
	}
	return (XwaModernPaperWhite)best;
}

static float XwaRemaster_PaperWhiteToNits(XwaModernPaperWhite paper_white) {
	if (paper_white < XWA_MODERN_PAPER_WHITE_AUTO || paper_white > XWA_MODERN_PAPER_WHITE_400) {
		return 0.0f;
	}
	return g_paperWhiteNits[paper_white];
}

static int XwaRemaster_VideoOptionsValid(const XwaModernVideoOptions* options) {
	return options && XwaModernSurroundOptions_Valid(&options->surround) &&
		   options->vr_world_scale >= XWA_MODERN_VR_SCALE_MIN &&
		   options->vr_world_scale <= XWA_MODERN_VR_SCALE_MAX &&
		   options->vr_panel_scale >= XWA_MODERN_VR_PANEL_SCALE_MIN &&
		   options->vr_panel_scale <= XWA_MODERN_VR_PANEL_SCALE_MAX &&
		   options->vr_screen_scale >= XWA_MODERN_VR_SCALE_MIN &&
		   options->vr_screen_scale <= XWA_MODERN_VR_SCALE_MAX &&
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
		   options->motion_blur_amount <= 1.0f && options->sdr_gamma >= XWA_MODERN_SDR_GAMMA_2_2 &&
		   options->sdr_gamma <= XWA_MODERN_SDR_GAMMA_SRGB &&
		   options->paper_white >= XWA_MODERN_PAPER_WHITE_AUTO &&
		   options->paper_white <= XWA_MODERN_PAPER_WHITE_400 && isfinite(options->hud_scale) &&
		   options->hud_scale >= XWA_MODERN_HUD_SCALE_MIN && options->hud_scale <= XWA_MODERN_HUD_SCALE_MAX;
}

void XwaRemaster_GetVideoOptions(XwaModernVideoOptions* out) {
	XwaFlightSsaoParams ssao;
	XwaFlightShadowParams shadows;
	XwaFlightMotionBlurParams motion_blur;
	XwaFlightTemporalParams temporal;

	if (!out) {
		return;
	}
	memset(out, 0, sizeof *out);
	memset(&ssao, 0, sizeof ssao);
	memset(&shadows, 0, sizeof shadows);
	memset(&motion_blur, 0, sizeof motion_blur);
	XwaRemasterFlight_GetSsao(&ssao);
	XwaRemasterFlight_GetShadows(&shadows);
	XwaRemasterFlight_GetMotionBlur(&motion_blur);
	XwaRemasterFlight_GetTemporal(&temporal);
	out->ssao_quality = (XwaModernSsaoQuality)ssao.ssao_quality;
	out->shadow_quality =
		shadows.atlas_size >= RM_SHADOW_ATLAS_HIGH ? XWA_MODERN_SHADOW_HIGH : XWA_MODERN_SHADOW_STANDARD;
	out->fsr_upscaling = XwaRemaster_FromTemporalMode(temporal.mode);
	out->msaa = XwaRemaster_FromSampleCount(XwaRemaster_MsaaSampleCount());
	out->motion_blur_quality = (XwaModernMotionBlurQuality)motion_blur.quality;
	out->motion_blur_amount = motion_blur.shutter;
	out->hdr_output = XwaRemaster_GetHdrDesired();
	out->sdr_gamma = XwaRemaster_SdrGammaFromDecode(Aeron_OutputSdrContentGamma());
	out->paper_white = XwaRemaster_PaperWhiteFromNits(Aeron_OutputPaperWhiteNits());
	out->hud_scale = XwaRemasterHud_UserScale();
	out->surround = g.surround;
	/* The mode and the eye images' scale apply at startup only, and the
	 * application keeps them. */
	out->vr_mode = XWA_MODERN_VR_OFF;
	out->vr_render_scale = 1.0f;
	out->vr_world_scale = g.vr_world_scale;
	out->vr_mirror = g.vr_mirror;
	out->vr_panel_scale = g.vr_panel_scale;
	out->vr_screen_scale = g.vr_screen_scale;
}

void XwaRemaster_SetVideoOptions(const XwaModernVideoOptions* options) {
	XwaFlightSsaoParams ssao;
	XwaFlightShadowParams shadows;
	XwaFlightMotionBlurParams motion_blur;
	XwaFlightTemporalParams temporal;

	if (!XwaRemaster_VideoOptionsValid(options)) {
		return;
	}
	XwaRemasterFlight_GetSsao(&ssao);
	ssao.ssao_quality = options->ssao_quality;
	XwaRemasterFlight_SetSsao(&ssao);

	XwaRemasterFlight_GetShadows(&shadows);
	shadows.atlas_size = RM_SHADOW_ATLAS_STANDARD;
	if (options->shadow_quality == XWA_MODERN_SHADOW_HIGH) {
		shadows.atlas_size = RM_SHADOW_ATLAS_HIGH;
	}
	XwaRemasterFlight_SetShadows(&shadows);

	XwaRemasterFlight_GetMotionBlur(&motion_blur);
	motion_blur.quality = options->motion_blur_quality;
	motion_blur.shutter = options->motion_blur_amount;
	XwaRemasterFlight_SetMotionBlur(&motion_blur);

	g.msaa_samples = XwaRemaster_ToSampleCount(options->msaa);
	XwaRemasterFlight_GetTemporal(&temporal);
	temporal.mode = XwaRemaster_ToTemporalMode(options->fsr_upscaling);
	XwaRemasterFlight_SetTemporal(&temporal);

#if !defined(__APPLE__)
	/* Apple keeps Aeron's platform defaults for both: the compositor presents
	 * SDR content with the piecewise curve, and EDR reference white follows
	 * the system brightness rather than absolute nits. The options are shown
	 * disabled in the UI. */
	Aeron_SetOutputSdrContentGamma(XwaRemaster_SdrGammaToDecode(options->sdr_gamma));
	Aeron_SetOutputPaperWhiteNits(XwaRemaster_PaperWhiteToNits(options->paper_white));
#endif

	XwaRemaster_SetHdrDesired(options->hdr_output);
	XwaRemasterHud_SetUserScale(options->hud_scale);
	g.surround = options->surround;
	g.surround.enabled = options->surround.enabled != 0;
	g.vr_world_scale = options->vr_world_scale;
	g.vr_mirror = options->vr_mirror != 0;
	g.vr_panel_scale = options->vr_panel_scale;
	g.vr_screen_scale = options->vr_screen_scale;
	XwaRemasterFlight_SetHudPanelScale(g.vr_panel_scale);
	Aeron_XrSetScreenScale(g.vr_screen_scale);
}

int XwaRemaster_Init(const XwaRemasterInitOptions* options) {
	XwaModernVideoOptions video_options;
	unsigned int video_override_mask;
	char opt_mesh_error[256];

	if (g.initialized) {
		return g.assets != NULL;
	}
	if (!options) {
		Aeron_LogError("xwa.remaster", "resolved initialization options are required");
		return 0;
	}
	g.initialized = 1;
	if (!XwaRemasterOptMesh_Init(Aeron_GetVfs(), opt_mesh_error, sizeof opt_mesh_error)) {
		Aeron_LogError("xwa.remaster", "OPT mesh initialization failed: %s", opt_mesh_error);
		return 0;
	}
	XwaRemasterShip_Configure(options->opt_smooth_angle_degrees, options->opt_emissive_strength,
							  options->opt_projectile_emissive_strength, options->engine_emissive_strength);
	XwaRemasterEnvironment_Configure(options->environment_maps_enabled, options->environment_maps_manifest,
									 options->environment_maps_sun_max_radiance);
	XwaRemasterCapturedLighting_Configure(&options->captured_lighting);
	if (!XwaRemasterFlight_InitConfig(Aeron_GetVfs())) {
		return 0;
	}
	XwaRemasterFlight_GetPresentationDefaults(&g.msaa_samples, &g.hdr_desired);
	XwaModernSurroundOptions_Default(&g.surround);
	g.vr_world_scale = 1.0f;
	g.vr_mirror = 1;
	g.vr_panel_scale = 1.0f;
	g.vr_screen_scale = 1.0f;
	XwaRemasterFlight_GetDebugStereoDefaults(&g.debug_stereo.ipd_m, &g.debug_stereo.fov_deg);
	g.debug_stereo.mouse_yaw_deg = 90.0f;
	g.debug_stereo.mouse_pitch_deg = 60.0f;
	g.surround_forced = options->flight_views == 1;
	/* A test run keeps the head still, so that its frames can be compared. */
	g.debug_stereo.enabled = options->flight_views == 2;
	g.debug_stereo.mouse_head = !g.debug_stereo.enabled;
	g.debug_stereo.yaw_deg = options->stereo_head[0];
	g.debug_stereo.pitch_deg = options->stereo_head[1];
	g.debug_stereo.roll_deg = options->stereo_head[2];
	if (!XwaRemasterHud_Init(Aeron_GetVfs())) {
		return 0;
	}
	/* Apply the required shipped HDR preference, optionally overlaid by the
	 * user configuration. This happens before any scene render so the present
	 * chains select the correct shader and swapchain format. Classic layers
	 * remain SDR content when composed onto an scRGB swapchain. */
	XwaRemaster_GetVideoOptions(&video_options);
	video_override_mask = options->video_options_override_mask;
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_SSAO) {
		video_options.ssao_quality = options->video_options.ssao_quality;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_SHADOW_QUALITY) {
		video_options.shadow_quality = options->video_options.shadow_quality;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_FSR) {
		video_options.fsr_upscaling = options->video_options.fsr_upscaling;
		if (video_options.fsr_upscaling != XWA_MODERN_FSR_OFF) {
			video_options.msaa = XWA_MODERN_MSAA_OFF;
		}
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_MSAA) {
		video_options.msaa = options->video_options.msaa;
		if (video_options.msaa != XWA_MODERN_MSAA_OFF) {
			video_options.fsr_upscaling = XWA_MODERN_FSR_OFF;
		}
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_MOTION_BLUR) {
		video_options.motion_blur_quality = options->video_options.motion_blur_quality;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_MOTION_BLUR_AMOUNT) {
		video_options.motion_blur_amount = options->video_options.motion_blur_amount;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_HDR) {
		video_options.hdr_output = options->video_options.hdr_output;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_SDR_GAMMA) {
		video_options.sdr_gamma = options->video_options.sdr_gamma;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_PAPER_WHITE) {
		video_options.paper_white = options->video_options.paper_white;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_HUD_SCALE) {
		video_options.hud_scale = options->video_options.hud_scale;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_SURROUND) {
		video_options.surround.enabled = options->video_options.surround.enabled;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_SURROUND_SCREEN_WIDTH) {
		video_options.surround.screen_width_mm = options->video_options.surround.screen_width_mm;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_SURROUND_SCREEN_HEIGHT) {
		video_options.surround.screen_height_mm = options->video_options.surround.screen_height_mm;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_SURROUND_BEZEL) {
		video_options.surround.bezel_mm = options->video_options.surround.bezel_mm;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_SURROUND_DISTANCE) {
		video_options.surround.distance_mm = options->video_options.surround.distance_mm;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_SURROUND_SIDE_ANGLE) {
		video_options.surround.side_angle_deg = options->video_options.surround.side_angle_deg;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_VR_WORLD_SCALE) {
		video_options.vr_world_scale = options->video_options.vr_world_scale;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_VR_MIRROR) {
		video_options.vr_mirror = options->video_options.vr_mirror;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_VR_PANEL_SCALE) {
		video_options.vr_panel_scale = options->video_options.vr_panel_scale;
	}
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_VR_SCREEN_SCALE) {
		video_options.vr_screen_scale = options->video_options.vr_screen_scale;
	}
	XwaRemaster_SetVideoOptions(&video_options);
	g.hdr_apply_pending = 0;
	/* Aeron downgrades to SDR by itself when HDR is unavailable and re-applies
	 * the request if the display's HDR state changes later, so a failure here
	 * means the swapchain could not be configured at all. */
	if (!Aeron_SetOutputHdr(g.hdr_desired)) {
		Aeron_LogError("xwa.remaster", "HDR output initialization failed");
		return 0;
	}
	Aeron_LogInfo("xwa.remaster", "HDR output: desired %s, %s (headroom %.2f)", g.hdr_desired ? "on" : "off",
				  Aeron_OutputHdrStatusName(Aeron_OutputHdrStatus()), (double)Aeron_OutputHdrHeadroom());
	g.assets = XwaRemasterAssets_Create(Aeron_ModRoot(), options->prefer_original_2d);
	Aeron_BlendRampInit(&g.ramp);
	g.last_tick = UINT64_MAX; /* Snapshot zero has not been rendered yet. */
	g.mode = RM_VIEW_HD;
	g.primary_mode = RM_VIEW_HD;
	if (options->render_width > 0 && options->render_height > 0) {
		g.render_size_fixed = 1;
		g.render_pixel_width = options->render_width;
		g.render_pixel_height = options->render_height;
		Aeron_LogInfo("xwa.remaster", "render size fixed at %dx%d", g.render_pixel_width,
					  g.render_pixel_height);
	} else if (!Aeron_GetPresentationPixelSize(&g.render_pixel_width, &g.render_pixel_height)) {
		g.render_pixel_width = XWA_PRESENTATION_WIDTH;
		g.render_pixel_height = XWA_PRESENTATION_HEIGHT;
	}
	g.observed_pixel_width = g.render_pixel_width;
	g.observed_pixel_height = g.render_pixel_height;
	if (!g.assets) {
		Aeron_LogError("xwa.remaster", "asset resolver init failed");
	}
#ifdef AERON_DEBUG_UI
	XwaRemasterDebugTools_Register();
#endif
	return g.assets != NULL;
}

static void XwaRemaster_SetBlendEndpoint(RmViewMode mode) {
	const float alpha = mode == RM_VIEW_CLASSIC ? 0.0f : 1.0f;
	g.ramp.alpha = alpha;
	g.ramp.target = alpha;
}

static void XwaRemaster_SetViewMode(RmViewMode mode, int fade) {
	if (!fade) {
		XwaRemaster_SetBlendEndpoint(mode);
	}
	g.mode = mode;
	Aeron_LogInfo("xwa.remaster", "view mode: %s",
				  g.mode == RM_VIEW_HD ? "HD" : (g.mode == RM_VIEW_SPLIT ? "SPLIT" : "CLASSIC"));
}

static void XwaRemaster_RequestViewMode(RmViewMode mode, int fade) {
	if (mode == RM_VIEW_HD) {
		g.pending_mode_valid = 0;
		XwaRemaster_SetViewMode(mode, fade);
		return;
	}
	if (g.pending_mode_valid) {
		g.pending_mode = mode;
		g.pending_mode_fade = fade;
		return;
	}
	if (g.classic_suppressed_this_frame) {
		/* Keep opaque HD on screen while classic rendering restarts. The mode
		 * becomes visible only after the shim reports a completed new frame. */
		g.pending_mode = mode;
		g.pending_mode_fade = fade;
		g.pending_mode_valid = 1;
		g.pending_classic_frame_serial = XwaPort_GetClassicFlightFrameSerial();
		return;
	}
	XwaRemaster_SetViewMode(mode, fade);
}

/* The classic renderer can't draw GLB replacement models. When at least one
 * can be loaded, the classic and split views stay off and only the HD view
 * is shown. */
static void XwaRemaster_ToggleSplitView(void) {
	/* The comparison with the original renderer shows one view. */
	if (XwaModelSource_HasReplacements() || g.rig_kind != RM_RIG_NONE) {
		return;
	}
	const RmViewMode selected_mode = g.pending_mode_valid ? g.pending_mode : g.mode;
	const RmViewMode target = selected_mode == RM_VIEW_SPLIT ? g.primary_mode : RM_VIEW_SPLIT;
	XwaRemaster_RequestViewMode(target, 0);
}

static void XwaRemaster_TogglePrimaryView(void) {
	if (XwaModelSource_HasReplacements() || g.rig_kind != RM_RIG_NONE) {
		return;
	}
	const RmViewMode selected_mode = g.pending_mode_valid ? g.pending_mode : g.mode;
	const RmViewMode old_primary = g.primary_mode;
	const RmViewMode target = old_primary == RM_VIEW_HD ? RM_VIEW_CLASSIC : RM_VIEW_HD;
	if (selected_mode == RM_VIEW_SPLIT) {
		/* SPLIT is a temporary comparison of the stored primary mode. Start
		 * the full-screen fade from that primary mode's exact endpoint. */
		XwaRemaster_SetBlendEndpoint(old_primary);
	}
	g.primary_mode = target;
	XwaRemaster_RequestViewMode(target, 1);
}

static int XwaRemaster_CanSuppressClassicFlight(void) {
	const XwaSnapshot* snap = XwaSnapshot_Current();
	return snap && snap->scene_kind == XWA_SCENE_FLIGHT && snap->flight_camera_valid && g.scene_tex &&
		   g.scene_kind == XWA_SCENE_FLIGHT && g.mode == RM_VIEW_HD && g.ramp.alpha >= 1.0f;
}

static int XwaRemaster_CanPresentFlightDirect(const XwaSnapshot* snap) {
	return snap && snap->scene_kind == XWA_SCENE_FLIGHT && snap->flight_camera_valid &&
		   g.mode == RM_VIEW_HD && !g.pending_mode_valid && g.ramp.alpha >= 1.0f &&
		   g.classic_suppressed_this_frame &&
		   XwaRemasterFlight_CanDirectPresent(g.render_pixel_width, g.render_pixel_height);
}

static void XwaRemaster_FatalGpu(const char* operation) { Aeron_RequestFatalRendererError(operation); }

/* out = a * b, for quaternions stored as (x, y, z, w). */
static void XwaRemaster_QuatMultiply(const float a[4], const float b[4], float out[4]) {
	out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
	out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
	out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
	out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

void XwaRemaster_GetDebugStereo(XwaRemasterDebugStereo* out) {
	if (out) {
		*out = g.debug_stereo;
	}
}

void XwaRemaster_SetDebugStereo(const XwaRemasterDebugStereo* in) {
	if (in) {
		g.debug_stereo = *in;
	}
}

/* Builds the surround rig for the render target. Returns 0, and logs why
 * once, when the settings or the screens can't be used. */
static int XwaRemaster_BuildSurroundRig(AeronViewRig* rig) {
	const char* problem = NULL;
	AeronRectI displays[4];
	const int display_count = Aeron_GetWindowDisplayRects(displays, 4);
	/* Three screens of different sizes can't be one surround setup. One
	 * screen is a wide screen, or a driver's surround display. */
	if (display_count == 3 &&
		(displays[0].width != displays[1].width || displays[1].width != displays[2].width ||
		 displays[0].height != displays[1].height || displays[1].height != displays[2].height)) {
		problem = "the three screens under the window don't have the same resolution";
	}
	/* Each view is a third of the render target. */
	const int view_width = g.render_pixel_width / 3;
	const int view_height = g.render_pixel_height;
	const AeronRectI rects[3] = {
		{ 0, 0, view_width, view_height },
		{ view_width, 0, view_width, view_height },
		{ 2 * view_width, 0, view_width, view_height },
	};
	const AeronSurroundDesc desc = {
		.screen_width_m = g.surround.screen_width_mm * 0.001f,
		.screen_height_m = g.surround.screen_height_mm * 0.001f,
		.bezel_m = g.surround.bezel_mm * 0.001f,
		.distance_m = g.surround.distance_mm * 0.001f,
		.side_angle_rad = g.surround.side_angle_deg * (3.14159265f / 180.0f),
	};
	if (!problem && !AeronViewRig_FromSurround(&desc, rects, rig)) {
		problem = "with these sizes and this angle, a side screen faces away from the player";
	}
	if (!problem) {
		g.surround_refused_width = 0;
		return 1;
	}
	if (memcmp(&g.surround_refused, &g.surround, sizeof g.surround) != 0 ||
		g.surround_refused_width != g.render_pixel_width) {
		g.surround_refused = g.surround;
		g.surround_refused_width = g.render_pixel_width;
		Aeron_LogWarn("xwa.remaster", "surround is off: %s", problem);
	}
	return 0;
}

/* Builds the debug UI's stereo rig: two eyes side by side in the render
 * target. */
static int XwaRemaster_BuildDebugStereoRig(AeronViewRig* rig) {
	const XwaRemasterDebugStereo* d = &g.debug_stereo;
	const int view_width = g.render_pixel_width / 2;
	const int view_height = g.render_pixel_height;
	if (view_width <= 0 || view_height <= 0) {
		return 0;
	}
	const float degrees = 3.14159265f / 180.0f;
	/* The vertical field of view that makes the eye's pixels square. */
	const float half_width = 0.5f * d->fov_deg * degrees;
	const float half_height = atanf(tanf(half_width) * (float)view_height / (float)view_width);
	const AeronViewFov fov = { -half_width, half_width, half_height, -half_height };
	const float yaw = (d->mouse_head ? g.mouse_x * d->mouse_yaw_deg : d->yaw_deg) * degrees;
	const float pitch = (d->mouse_head ? -g.mouse_y * d->mouse_pitch_deg : d->pitch_deg) * degrees;
	const float roll = d->roll_deg * degrees;
	/* The rig's axes are X right, Y up and -Z forward, so looking right is
	 * a negative turn around Y, looking up a positive turn around X, and a
	 * clockwise roll a negative turn around Z. The head turns around Y
	 * first, then X, then Z: orientation = qy * qx * qz. */
	const float qy[4] = { 0.0f, sinf(-0.5f * yaw), 0.0f, cosf(-0.5f * yaw) };
	const float qx[4] = { sinf(0.5f * pitch), 0.0f, 0.0f, cosf(0.5f * pitch) };
	const float qz[4] = { 0.0f, 0.0f, sinf(-0.5f * roll), cosf(-0.5f * roll) };
	float qyx[4];
	AeronViewPose head;
	XwaRemaster_QuatMultiply(qy, qx, qyx);
	XwaRemaster_QuatMultiply(qyx, qz, head.orientation);
	head.position_m[0] = d->position_m[0];
	head.position_m[1] = d->position_m[1];
	head.position_m[2] = -d->position_m[2];
	return AeronViewRig_DebugStereo(d->ipd_m, &fov, &head, view_width, view_height, rig);
}

/* Builds the headset's rig for a flight frame: asks Aeron for the eyes'
 * poses and images. Returns 0 when no VR session runs. While a session runs
 * and Aeron has no eye images for this frame, the last rig is kept, so the
 * flight scene keeps its size, and g.xr_views_ready says that the frame
 * can't be rendered for the headset. */
static int XwaRemaster_BuildHeadsetRig(const XwaSnapshot* snap, AeronViewRig* rig) {
	AeronXrFrame frame;

	g.xr_views_ready = 0;
	if (!Aeron_XrSessionRunning() || !snap || snap->scene_kind != XWA_SCENE_FLIGHT ||
		!snap->flight_camera_valid) {
		return 0;
	}
	if (!Aeron_XrBeginViews(&frame)) {
		if (g.rig_kind != RM_RIG_HEADSET) {
			return 0;
		}
		*rig = g.headset_rig;
		return 1;
	}
	/* The head's movement in the game's world. */
	for (int i = 0; i < frame.rig.count; i++) {
		for (int axis = 0; axis < 3; axis++) {
			frame.rig.views[i].pose.position_m[axis] *= g.vr_world_scale;
		}
		g.xr_targets[i] = frame.color[i];
		g.xr_hidden_areas[i] = frame.hidden_area[i];
		g.xr_visible_areas[i] = frame.visible_area[i];
	}
	for (int axis = 0; axis < 3; axis++) {
		frame.rig.center.position_m[axis] *= g.vr_world_scale;
	}
	/* A recentre moves the views in one step. */
	if (frame.rig.generation != g.headset_generation) {
		g.headset_generation = frame.rig.generation;
		g.rig_generation++;
	}
	*rig = frame.rig;
	g.headset_rig = frame.rig;
	g.xr_views_ready = 1;
	return 1;
}

/* Chooses the views of the flight scene for this frame and gives them to the
 * flight renderer and to the presentation. */
static void XwaRemaster_UpdateViewRig(const XwaSnapshot* snap) {
	AeronViewRig rig;
	RmRigKind kind = RM_RIG_NONE;
	/* The head turns the view, so the cockpit look keys don't. */
	XwaPort_SetCockpitLookSuppressed(Aeron_XrSessionRunning());
	if (XwaRemaster_BuildHeadsetRig(snap, &rig)) {
		kind = RM_RIG_HEADSET;
	} else if (g.debug_stereo.enabled && XwaRemaster_BuildDebugStereoRig(&rig)) {
		kind = RM_RIG_DEBUG_STEREO;
	} else if ((g.surround.enabled || g.surround_forced) && XwaRemaster_BuildSurroundRig(&rig)) {
		kind = RM_RIG_SURROUND;
	}
	const int view_width = kind != RM_RIG_NONE ? rig.width : 0;
	const int view_height = kind != RM_RIG_NONE ? rig.height : 0;
	if (kind != g.rig_kind || view_width != g.rig_view_width || view_height != g.rig_view_height) {
		/* The last flight frame has another layout. */
		g.rig_generation++;
		g.force_scene_render = 1;
		if (g.scene_kind == XWA_SCENE_FLIGHT) {
			g.scene_tex = NULL;
			g.scene_is_direct = 0;
		}
		if (kind != RM_RIG_NONE) {
			Aeron_LogInfo("xwa.remaster", "flight views: %s, %d views of %dx%d",
						  kind == RM_RIG_SURROUND       ? "surround"
						  : kind == RM_RIG_DEBUG_STEREO ? "debug stereo"
														: "headset",
						  rig.count, view_width, view_height);
			/* The comparison with the original renderer shows one view. */
			g.pending_mode_valid = 0;
			g.primary_mode = RM_VIEW_HD;
			XwaRemaster_SetViewMode(RM_VIEW_HD, 0);
		} else {
			Aeron_LogInfo("xwa.remaster", "flight views: one view");
		}
	}
	g.rig_kind = kind;
	g.rig_view_width = view_width;
	g.rig_view_height = view_height;
	g.rig_tracked = kind != RM_RIG_NONE && rig.tracked;
	if (kind != RM_RIG_NONE) {
		rig.generation = g.rig_generation;
	}
	XwaRemasterFlight_SetViewRig(kind != RM_RIG_NONE ? &rig : NULL);
	XwaRemasterFlight_SetViewTargets(kind == RM_RIG_HEADSET && g.xr_views_ready ? g.xr_targets : NULL,
									 AERON_OPT_UNITS_PER_METER * g.vr_world_scale);
	const int headset_areas = kind == RM_RIG_HEADSET && g.xr_views_ready;
	XwaRemasterFlight_SetViewAreas(headset_areas ? g.xr_hidden_areas : NULL,
								   headset_areas ? g.xr_visible_areas : NULL);
	if (kind == RM_RIG_SURROUND) {
		const AeronRectI center = rig.views[rig.primary].rect;
		XwaPresentation_SetSurround(center.x, center.width, g.render_pixel_width);
	} else {
		XwaPresentation_SetSurround(0, 0, 0);
	}
	/* A tracked rig renders on every frame, because the head moves between
	 * the game's ticks. */
	if (g.rig_tracked && snap && snap->scene_kind == XWA_SCENE_FLIGHT) {
		g.force_scene_render = 1;
	}
}

static void XwaRemaster_UpdateRenderSize(void) {
	int width;
	int height;
	if (g.render_size_fixed || !Aeron_GetPresentationPixelSize(&width, &height)) {
		return;
	}
	const uint64_t now_us = Aeron_NowUs();
	if (width != g.observed_pixel_width || height != g.observed_pixel_height) {
		g.observed_pixel_width = width;
		g.observed_pixel_height = height;
		g.observed_size_since_us = now_us;
		return;
	}
	if ((width == g.render_pixel_width && height == g.render_pixel_height) ||
		now_us - g.observed_size_since_us < RM_RESIZE_SETTLE_US) {
		return;
	}
	g.render_pixel_width = width;
	g.render_pixel_height = height;
	const XwaSnapshot* snap = XwaSnapshot_Current();
	if (snap && (snap->scene_kind == XWA_SCENE_FLIGHT || snap->scene_kind == XWA_SCENE_CUTSCENE)) {
		/* The old texture has different physical dimensions. Invalidate it
		 * before this host frame selects a presentation path. */
		g.force_scene_render = 1;
		g.scene_tex = NULL;
	} else if (snap && (snap->scene_kind == XWA_SCENE_FRONTEND || snap->scene_kind == XWA_SCENE_LOADING ||
						snap->scene_kind == XWA_SCENE_FRONTEND_MODAL)) {
		/* Frontend retained surfaces migrate when the replacement frame is
		 * rendered. Keep the old texture visible until that succeeds. */
		g.force_scene_render = 1;
	}
	Aeron_LogInfo("xwa.remaster", "physical render size: %dx%d", width, height);
}

void XwaRemaster_BeginFrame(const AeronInputSnapshot* input) {
	if (!g.initialized || !g.assets) {
		XwaPort_SetClassicFlightRenderingEnabled(1);
		return;
	}
	XwaRemaster_UpdateRenderSize();
	if (input && input->window_width > 0 && input->window_height > 0) {
		g.mouse_x = 2.0f * (float)input->mouse.raw_x / (float)input->window_width - 1.0f;
		g.mouse_y = 2.0f * (float)input->mouse.raw_y / (float)input->window_height - 1.0f;
		g.mouse_x = g.mouse_x < -1.0f ? -1.0f : (g.mouse_x > 1.0f ? 1.0f : g.mouse_x);
		g.mouse_y = g.mouse_y < -1.0f ? -1.0f : (g.mouse_y > 1.0f ? 1.0f : g.mouse_y);
	}

	/* Deferred HDR composition flip (see XwaRemaster_SetHdrDesired). */
	if (g.hdr_apply_pending) {
		g.hdr_apply_pending = 0;
		if (!Aeron_SetOutputHdr(g.hdr_desired)) {
			XwaRemaster_FatalGpu("HDR output reconfiguration");
			return;
		}
	}

	/* Grave, Ctrl+F5 and Ctrl+F2 are host shortcuts (keyboard_mapping.h).
	 * Their keys stay suppressed until release, so the game never sees them. */
	if (!XwaPort_ShortcutsCaptured()) {
		const int debug_key = XwaKeyboardMapping_Trigger(input, XWA_KEYBOARD_SHORTCUT_DEBUG);
		const int renderer_key = XwaKeyboardMapping_Trigger(input, XWA_KEYBOARD_SHORTCUT_RENDERER);
		const int split_key = XwaKeyboardMapping_Trigger(input, XWA_KEYBOARD_SHORTCUT_SPLIT_VIEW);

		if (debug_key >= 0 && Aeron_DebugUiAvailable()) {
			Aeron_DebugUiToggle();
			XwaInput_SuppressKey(debug_key);
		}
		if (renderer_key >= 0) {
			XwaRemaster_TogglePrimaryView();
			XwaInput_SuppressKey(renderer_key);
		} else if (split_key >= 0) {
			XwaRemaster_ToggleSplitView();
			XwaInput_SuppressKey(split_key);
		}
	}

	g.classic_suppressed_this_frame = XwaRemaster_CanSuppressClassicFlight() && !g.pending_mode_valid;
	XwaPort_SetClassicFlightRenderingEnabled(!g.classic_suppressed_this_frame);
}

void XwaRemaster_Frame(int32_t delta_us) {
	if (!g.initialized || !g.assets) {
		return;
	}
	if (g.pending_mode_valid && XwaPort_GetClassicFlightFrameSerial() != g.pending_classic_frame_serial) {
		XwaRemaster_SetViewMode(g.pending_mode, g.pending_mode_fade);
		g.pending_mode_valid = 0;
	}

	const XwaSnapshot* snap = XwaSnapshot_Current();
	XwaRemaster_UpdateViewRig(snap);
	if (!snap) {
		return;
	}
	const int hyperspace_preview =
		XwaRemasterFlight_GetHyperspacePreview() != XWA_FLIGHT_HYPERSPACE_PREVIEW_OFF;
	const int hyperspace_preview_visible = hyperspace_preview && snap->scene_kind == XWA_SCENE_FLIGHT &&
										   snap->flight_camera_valid && !snap->flight_camera.map_mode &&
										   !snap->flight_camera.hyperspace_phase;
	if (snap->scene_kind == XWA_SCENE_FLIGHT && snap->flight_camera_valid &&
		(hyperspace_preview_visible || hyperspace_preview != g.hyperspace_preview_last)) {
		/* Preview animation is host-clock driven, so render it on every host
		 * frame and also force one ordinary frame when the override is disabled. */
		g.force_scene_render = 1;
	}
	g.hyperspace_preview_last = hyperspace_preview;
	/* A frozen film renders its frame on every host frame, so that a GPU
	 * capture, which ends at a present, gets one. */
	if (XwaFilmBenchmark_IsFrozen() && snap->scene_kind == XWA_SCENE_FLIGHT) {
		g.force_scene_render = 1;
	}
	if (g.force_scene_render && snap->scene_kind != XWA_SCENE_FLIGHT &&
		snap->scene_kind != XWA_SCENE_CUTSCENE) {
		g.force_scene_render = 0;
	}
	/* An environment capture runs from a flight frame, so a pending one
	 * renders the next frame even while the game is paused. */
	if (XwaRemasterEnvCapture_Pending() && snap->scene_kind == XWA_SCENE_FLIGHT) {
		g.force_scene_render = 1;
	}

	/* Render on new sim ticks and reuse the persistent texture between them.
	 * Keep the last complete render across a scene transition until the new
	 * scene has finished any incremental asset synchronization. */
	if (snap->scene_kind != g.last_kind) {
		g.last_kind = snap->scene_kind;
		/* Cutscenes supply their opaque video independently of scene_tex; their
		 * optional subtitle texture is not a valid transition background. */
		if (snap->scene_kind == XWA_SCENE_CUTSCENE || (g.scene_tex && g.scene_kind == XWA_SCENE_CUTSCENE)) {
			g.scene_tex = NULL;
			g.scene_is_direct = 0;
		}
	}
	const int flight_render_needed =
		snap->scene_kind != XWA_SCENE_FLIGHT || g.mode != RM_VIEW_CLASSIC || g.ramp.alpha > 0.0f;
	if (snap->scene_kind == XWA_SCENE_FLIGHT) {
		if (!flight_render_needed && !g.flight_render_suspended) {
			g.flight_render_suspended = 1;
			g.scene_tex = NULL;
			g.scene_is_direct = 0;
			XwaRemasterFlight_InvalidateHistory();
		} else if (flight_render_needed && g.flight_render_suspended) {
			g.flight_render_suspended = 0;
			/* The latest snapshot must render even if the game is paused and its
			 * tick index did not advance while CLASSIC was active. */
			g.force_scene_render = 1;
		}
	} else {
		g.flight_render_suspended = 0;
	}
	const int direct_present = XwaRemaster_CanPresentFlightDirect(snap);
	const int presentation_change = snap->scene_kind == XWA_SCENE_FLIGHT && g.scene_tex &&
									g.scene_kind == XWA_SCENE_FLIGHT && direct_present != g.scene_is_direct;
	/* Flight shutdown commits once after the task has released its camera and
	 * before the frontend callback draws its first frame. Preserve and, when
	 * necessary, resolve the last complete flight image before its assets are
	 * eligible for reconciliation. */
	const int retain_invalid_flight_frame = snap->scene_kind == XWA_SCENE_FLIGHT &&
											!snap->flight_camera_valid && g.scene_tex &&
											g.scene_kind == XWA_SCENE_FLIGHT;

	/* Classic OPT and texture-model lifetimes drive HD residency. While a
	 * settled CLASSIC flight suspends HD work, generation changes remain dirty
	 * and are reconciled before the first resumed render. */
	const int frontend_assets_need_sync =
		flight_render_needed && XwaRemasterAssets_FrontendAssetsNeedSync(g.assets, snap);
	const int ship_assets_need_sync = flight_render_needed && XwaRemasterShip_AssetsNeedSync(snap);
	const int texture_assets_need_sync =
		flight_render_needed && XwaRemasterAssets_FlightTexturesNeedSync(g.assets, snap);
	const int process_assets_need_prepare = flight_render_needed && snap->scene_kind == XWA_SCENE_LOADING &&
											XwaRemasterFlight_ProcessAssetsNeedPrepare();
	/* Authored environments are prepared when the mission key changes
	 * (or a debug reload is pending), never from the flight render. */
	const int environment_needs_prepare = flight_render_needed && XwaRemasterEnvironment_NeedsPrepare(snap);
	const int assets_need_sync =
		!retain_invalid_flight_frame &&
		(frontend_assets_need_sync || ship_assets_need_sync || texture_assets_need_sync ||
		 process_assets_need_prepare || environment_needs_prepare);
	/* A headset's flight frame can only be rendered into this frame's eye
	 * images. Without them, the frame is rendered on a later host frame. */
	const int headset_waits =
		g.rig_kind == RM_RIG_HEADSET && !g.xr_views_ready && snap->scene_kind == XWA_SCENE_FLIGHT;
	const int render_snapshot =
		!headset_waits && (snap->tick_index != g.last_tick || g.force_scene_render);
	const int frontend_scene = snap->scene_kind == XWA_SCENE_FRONTEND ||
							   snap->scene_kind == XWA_SCENE_FRONTEND_MODAL ||
							   snap->scene_kind == XWA_SCENE_LOADING;
	const int frontend_idle = frontend_scene && !snap->draw_2d_count && !snap->paint_cmd_count &&
							  !snap->glyph_count && !snap->surface_event_count && !snap->model_preview_count;
	/* Idle host ticks reuse the frontend output and its cursor textures. */
	const int retain_scene_frame =
		!render_snapshot || frontend_idle || snap->retain_frame ||
		(snap->scene_kind == XWA_SCENE_LOADING && !XwaRemaster_SnapshotHasPresent(snap)) ||
		retain_invalid_flight_frame;
	int assets_pending = 0;

	if (flight_render_needed && assets_need_sync) {
		XwaRemasterShipSyncResult ship_sync = XWA_REMASTER_SHIP_SYNC_COMPLETE;
		AeronCommandBufferUploadUsage upload_usage = { 0 };
		int process_assets_prepared = 0;
		int texture_assets_prepared = 0;
		AeronCommandBuffer* upload_cmd = Aeron_AcquireCommandBuffer();
		if (!upload_cmd) {
			XwaRemaster_FatalGpu("HD asset command-buffer acquisition");
			return;
		}
		Aeron_GpuDebugPush(upload_cmd, "OpenXWA HD asset synchronization");
		if (!XwaRemasterAssets_SyncFrontendAssets(g.assets, upload_cmd, snap)) {
			Aeron_CommandBufferSetFailure(upload_cmd, "Frontend asset preparation failed");
			ship_sync = XWA_REMASTER_SHIP_SYNC_FAILED;
		}
		if (ship_sync == XWA_REMASTER_SHIP_SYNC_COMPLETE && ship_assets_need_sync) {
			ship_sync = XwaRemasterShip_SyncAssets(upload_cmd, snap, RM_ASSET_UPLOAD_BYTE_BUDGET,
												   RM_ASSET_UPLOAD_COPY_BUDGET);
			if (ship_sync == XWA_REMASTER_SHIP_SYNC_FAILED)
				Aeron_CommandBufferSetFailure(upload_cmd, "Ship model asset preparation failed");
		}
		if (ship_sync == XWA_REMASTER_SHIP_SYNC_COMPLETE) {
			if (!XwaRemasterAssets_SyncFlightTextures(g.assets, upload_cmd, snap)) {
				Aeron_CommandBufferSetFailure(upload_cmd, "Flight texture asset preparation failed");
				ship_sync = XWA_REMASTER_SHIP_SYNC_FAILED;
			} else {
				texture_assets_prepared = texture_assets_need_sync;
			}
			if (ship_sync == XWA_REMASTER_SHIP_SYNC_COMPLETE && process_assets_need_prepare) {
				process_assets_prepared = XwaRemasterFlight_PrepareProcessAssets(upload_cmd, g.assets);
				if (!process_assets_prepared) {
					Aeron_CommandBufferSetFailure(upload_cmd, "Flight process asset preparation failed");
					ship_sync = XWA_REMASTER_SHIP_SYNC_FAILED;
				}
			}
			if (ship_sync == XWA_REMASTER_SHIP_SYNC_COMPLETE && environment_needs_prepare) {
				XwaRemasterEnvironment_Prepare(upload_cmd, snap);
			}
		}
		Aeron_GpuDebugPop(upload_cmd);
		(void)Aeron_CommandBufferGetUploadUsage(upload_cmd, &upload_usage);
		if (ship_sync == XWA_REMASTER_SHIP_SYNC_FAILED) {
			Aeron_CancelCommandBuffer(upload_cmd);
			XwaRemaster_FatalGpu("HD asset preparation");
			return;
		}
		if (!Aeron_SubmitCommandBuffer(upload_cmd)) {
			XwaRemaster_FatalGpu("HD asset upload submission");
			return;
		}
		if (ship_assets_need_sync) {
			XwaRemasterShip_CommitSyncBatch();
		}
		if (frontend_assets_need_sync) {
			XwaRemasterAssets_CommitFrontendAssets(g.assets, snap->frontend_asset_generation);
		}
		if (texture_assets_prepared) {
			XwaRemasterAssets_CommitFlightTextures(g.assets, snap->texture_asset_generation);
		}
		if (process_assets_prepared) {
			XwaRemasterFlight_CommitProcessAssets();
		}
		if (upload_usage.staged_bytes >= 16u * 1024u * 1024u || upload_usage.copy_count >= 64u) {
			Aeron_LogDebug(
				"xwa.remaster",
				"asset upload batch: staged=%llu reserved=%llu chunks=%u copies=%u passes=%u largest=%u",
				(unsigned long long)upload_usage.staged_bytes,
				(unsigned long long)upload_usage.reserved_bytes, upload_usage.chunk_count,
				upload_usage.copy_count, upload_usage.copy_pass_count, upload_usage.largest_upload_bytes);
		}
		if (ship_sync == XWA_REMASTER_SHIP_SYNC_MORE) {
			assets_pending = 1;
		}
	}

	g.frame_waits = assets_pending || headset_waits;
	/* Required classic output is re-submitted on idle game ticks. Opaque HD
	 * flight frames omit that layer; the remaster texture below then owns the
	 * complete presentation frame. */
	if (!assets_pending && flight_render_needed && (render_snapshot || presentation_change) &&
		(!retain_scene_frame || presentation_change)) {
		AeronTexture* next_scene_tex = g.scene_tex;
		int next_scene_is_direct = g.scene_is_direct;
		int next_scene_is_tonemapped = g.scene_is_tonemapped;
		int next_scene_is_headset = 0;
		int render_output_required = retain_scene_frame;
		/* A pending environment capture renders and reads back its own scenes
		 * before this frame's command buffer exists; the frame then renders
		 * normally. */
		if (!retain_scene_frame && snap->scene_kind == XWA_SCENE_FLIGHT && XwaRemasterEnvCapture_Pending()) {
			XwaRemasterEnvCapture_Run(snap, g.assets);
		}
		AeronCommandBuffer* cmd = Aeron_AcquireCommandBuffer();
		if (!cmd) {
			XwaRemaster_FatalGpu("HD frame command-buffer acquisition");
			return;
		}
		Aeron_GpuDebugPush(cmd, "OpenXWA HD frame");
		if (!retain_scene_frame) {
			switch (snap->scene_kind) {
				case XWA_SCENE_FRONTEND:
				case XWA_SCENE_LOADING:
				case XWA_SCENE_FRONTEND_MODAL: {
					/* The frontend picture has the design screen's shape in every
					   layout; only the mapping of its 640x480 units changes. */
					g.frontend_wide = snap->frontend_layout == XWA_FRONTEND_LAYOUT_WIDE;
					XwaRemaster_SetFrontendShape(1);
					const XwaPresentationRect frame = XwaPresentation_Frame();
					const XwaPresentationRect safe = XwaPresentation_FrontendFrame();
					/* Match Aeron_ProjectLogicalRect exactly so the frontend texture
					 * reaches the swapchain without a whole-frame size conversion. */
					const int frontend_width =
						(int)((long long)safe.width * g.render_pixel_width / frame.width);
					const int frontend_height =
						(int)((long long)safe.height * g.render_pixel_height / frame.height);
					render_output_required = 1;
					Aeron_GpuDebugPush(cmd, "OpenXWA frontend reconstruction");
					next_scene_tex =
						XwaRemasterFrontend_Render(cmd, snap, g.assets, frontend_width, frontend_height);
					next_scene_is_direct = 0;
					next_scene_is_tonemapped = 0;
					Aeron_GpuDebugPop(cmd);
					break;
				}
				case XWA_SCENE_FLIGHT:
					render_output_required = snap->flight_camera_valid;
					XwaRemasterAssets_UpdateFrameSets(g.assets, snap);
					Aeron_GpuDebugPush(cmd, "OpenXWA flight renderer");
					next_scene_tex = snap->flight_camera_valid
										 ? XwaRemasterFlight_Render(cmd, snap, g.assets, g.render_pixel_width,
																	g.render_pixel_height, direct_present)
										 : NULL;
					next_scene_is_direct = next_scene_tex && XwaRemasterFlight_DirectPresentationReady();
					next_scene_is_tonemapped = next_scene_tex != NULL;
					next_scene_is_headset = next_scene_tex && XwaRemasterFlight_ViewTargetsRendered();
					Aeron_GpuDebugPop(cmd);
					break;
				case XWA_SCENE_CUTSCENE:
					Aeron_GpuDebugPush(cmd, "OpenXWA cutscene subtitles");
					next_scene_tex = XwaRemasterCutscene_Render(cmd, snap, g.assets, g.render_pixel_width,
																g.render_pixel_height);
					next_scene_is_direct = 0;
					next_scene_is_tonemapped = 0;
					Aeron_GpuDebugPop(cmd);
					break;
				default:
					next_scene_tex = NULL;
					next_scene_is_direct = 0;
					next_scene_is_tonemapped = 0;
					break;
			}
		} else {
			render_output_required = 1;
			Aeron_GpuDebugPush(cmd, "OpenXWA flight presentation transition");
			next_scene_tex = XwaRemasterFlight_ResolvePresentation(cmd, direct_present);
			next_scene_is_direct = next_scene_tex && XwaRemasterFlight_DirectPresentationReady();
			/* Retained/transition frames are flight output: tonemapped. */
			next_scene_is_tonemapped = next_scene_tex != NULL;
			Aeron_GpuDebugPop(cmd);
		}
		Aeron_GpuDebugPop(cmd);
		if (render_output_required && !next_scene_tex) {
			Aeron_CancelCommandBuffer(cmd);
			XwaRemaster_FatalGpu("HD render-target production");
			return;
		}
		if (!Aeron_SubmitCommandBuffer(cmd)) {
			XwaRemaster_FatalGpu("HD frame submission");
			return;
		}
		g.scene_tex = next_scene_tex;
		g.scene_kind = snap->scene_kind;
		if (next_scene_is_direct != g.scene_is_direct && snap->scene_kind == XWA_SCENE_FLIGHT) {
			Aeron_LogInfo("xwa.remaster", "flight presentation: %s (%dx%d)",
						  next_scene_is_direct ? "direct to swapchain" : "composed", g.render_pixel_width,
						  g.render_pixel_height);
		}
		g.scene_is_direct = next_scene_is_direct;
		g.scene_is_tonemapped = next_scene_is_tonemapped;
		g.scene_is_headset = next_scene_is_headset;
		/* The command buffer that draws the eye images is submitted, so
		 * Aeron can give them to the headset. */
		if (next_scene_is_headset) {
			Aeron_XrSubmitViews();
			if (g.vr_mirror && !XwaRemasterFlight_SubmitViewMirror()) {
				XwaRemaster_FatalGpu("headset mirror presentation");
				return;
			}
		}
		g.last_tick = snap->tick_index;
		g.force_scene_render = 0;
	} else if (!assets_pending && flight_render_needed && render_snapshot && retain_scene_frame) {
		g.last_tick = snap->tick_index;
		g.force_scene_render = 0;
	}

	if (snap->scene_kind == XWA_SCENE_CUTSCENE) {
		if (g.mode == RM_VIEW_HD && g.scene_tex) {
			const XwaPresentationRect safe = XwaPresentation_ClassicSafeFrame();
			const AeronTextureLayerDesc layer = {
				.texture = g.scene_tex,
				.logical_rect = { safe.x, safe.y, safe.width, safe.height },
				.blend_mode = AERON_LAYER_BLEND_PREMULTIPLIED,
				.color_space = AERON_COLOR_SPACE_LINEAR_DISPLAY,
			};
			if (!Aeron_SubmitTextureLayer(&layer)) {
				XwaRemaster_FatalGpu("cutscene subtitle presentation");
				return;
			}
			XwaMovieTask_SuppressClassicSubtitles();
		}
		return;
	}

	/* A movie without a fade started in this tick. Its frontend frame was
	 * replayed above, so the persistent surface and the save stack are up to
	 * date, but it isn't shown: with nothing submitted, the screen keeps the
	 * last frame until the movie's first frame. */
	if (snap->hold_presentation &&
		(snap->scene_kind == XWA_SCENE_FRONTEND || snap->scene_kind == XWA_SCENE_FRONTEND_MODAL)) {
		return;
	}

	/* Only view switches drive the fade. Missing output falls back to classic
	 * below without fading down HD, so the first ready HD frame is opaque. */
	const float target = g.mode != RM_VIEW_CLASSIC ? 1.0f : 0.0f;
	g.ramp.target = target;
	Aeron_BlendRampAdvance(&g.ramp, delta_us, target);
	if (!g.scene_tex || g.ramp.alpha <= 0.001f) {
		/* The classic surface is shown, so map input to it. */
		XwaRemaster_SetFrontendShape(0);
		if (g.classic_suppressed_this_frame) {
			XwaPort_SubmitRetainedClassicFrame();
		}
		return; /* classic only — submit nothing */
	}
	/* The flight frame is in the headset's eye images. Nothing of it goes on
	 * the virtual screen. */
	if (g.scene_is_headset && g.scene_kind == XWA_SCENE_FLIGHT) {
		return;
	}
	if (g.scene_is_direct) {
		if ((g.scene_kind != snap->scene_kind || direct_present) &&
			!XwaRemasterFlight_SubmitDirectPresentation()) {
			XwaRemaster_FatalGpu("direct flight presentation");
		}
		return;
	}

	/* The frontend reconstruction has the design screen's shape and is
	 * fitted in the frame. Modern flight owns the full presentation frame; HUD
	 * profiles change only the authored overlay layout. */
	XwaRemaster_SetFrontendShape(g.scene_kind != XWA_SCENE_FLIGHT);
	const XwaPresentationRect pr =
		g.scene_kind == XWA_SCENE_FLIGHT ? XwaPresentation_Frame() : XwaPresentation_FrontendFrame();
	AeronTextureLayerDesc layer = {
		.texture = g.scene_tex,
		.logical_rect = { pr.x, pr.y, pr.width, pr.height },
		.blend_mode = AERON_LAYER_BLEND_PREMULTIPLIED,
		/* Tonemapped flight output already carries its display model —
		 * a display gamma remap would apply the display gamma twice
		 * and make the texture path diverge from direct presentation
		 * (visible when SPLIT view forces this path under HDR). The
		 * frontend's texture holds sRGB values (frontend.c); presentation
		 * decodes it with the display gamma. */
		.color_space = g.scene_is_tonemapped ? AERON_COLOR_SPACE_LINEAR_SRGB : AERON_COLOR_SPACE_SRGB,
		.tint_enabled = 1,
		.tint_rgba = { g.ramp.alpha, g.ramp.alpha, g.ramp.alpha, g.ramp.alpha },
	};
	if (g.mode == RM_VIEW_SPLIT) {
		layer.scissor = (AeronRectI) { pr.x + pr.width / 2, pr.y, pr.width / 2, pr.height };
	}
	if (!Aeron_SubmitTextureLayer(&layer)) {
		XwaRemaster_FatalGpu("HD texture-layer presentation");
	}
	/* The cursor follows the mouse every frame, and the snapshot only says
	   whether it shows. The options window has the system cursor, and the
	   game doesn't take snapshots while the window pauses it, so the last
	   snapshot can still show the game's cursor. */
	if ((g.scene_kind == XWA_SCENE_FRONTEND || g.scene_kind == XWA_SCENE_FRONTEND_MODAL ||
		 g.scene_kind == XWA_SCENE_LOADING) &&
		!XwaPort_OptionsWindowOpen())
		XwaRemasterFrontend_PresentCursor(g.ramp.alpha, g.mode == RM_VIEW_SPLIT);
}

int XwaRemaster_FrameWaits(void) {
	/* The exposure in the capture's and the benchmark's logs is the meter's,
	 * which comes back from the GPU two frames after the first render. */
	return g.frame_waits || (g.scene_kind == XWA_SCENE_FLIGHT && XwaRemasterFlight_ExposureReadoutPending());
}

#define RM_BENCHMARK_WARMUP_RENDERS 30
#define RM_BENCHMARK_SAMPLES 7
#define RM_BENCHMARK_SAMPLE_RENDERS 60

static int rm_benchmark_compare_double(const void* a, const void* b) {
	const double x = *(const double*)a;
	const double y = *(const double*)b;
	return x < y ? -1 : x > y;
}

/* Renders the current snapshot `count` times with two renders in flight, as
 * scene_capture's benchmark does. Returns the wall time and the CPU time
 * spent recording, in microseconds. */
static int rm_benchmark_run(const XwaSnapshot* snap, int count, uint64_t* wall_us, uint64_t* cpu_us) {
	AeronGpuFence* pending[2] = { NULL, NULL };
	const uint64_t start_us = Aeron_NowUs();
	uint64_t cpu = 0;
	int ok = 1;

	for (int i = 0; i < count && ok; ++i) {
		AeronGpuFence** slot = &pending[i % 2];
		AeronGpuFence* retired = *slot;
		if (retired && !Aeron_WaitForGpuFence(retired)) {
			ok = 0;
			break;
		}
		const uint64_t cpu_start_us = Aeron_NowUs();
		XwaRemasterAssets_UpdateFrameSets(g.assets, snap);
		AeronCommandBuffer* cmd = Aeron_AcquireCommandBuffer();
		if (!cmd) {
			ok = 0;
			break;
		}
		Aeron_GpuDebugPush(cmd, "OpenXWA film benchmark frame");
		AeronTexture* tex = XwaRemasterFlight_Render(cmd, snap, g.assets, g.render_pixel_width,
													 g.render_pixel_height, /*direct_present=*/0);
		Aeron_GpuDebugPop(cmd);
		if (!tex) {
			Aeron_CancelCommandBuffer(cmd);
			ok = 0;
			break;
		}
		AeronGpuFence* next = Aeron_SubmitCommandBufferAndAcquireFence(cmd);
		if (!next) {
			ok = 0;
			break;
		}
		*slot = next;
		/* Released after the next submit, as scene_capture does: SDL's Metal
		 * backend retires a command buffer during the next submit. */
		Aeron_ReleaseGpuFence(retired);
		cpu += Aeron_NowUs() - cpu_start_us;
	}
	for (int i = 0; i < 2; ++i) {
		if (pending[i] && !Aeron_WaitForGpuFence(pending[i])) {
			ok = 0;
		}
	}
	if (!Aeron_WaitForGpuIdle()) {
		ok = 0;
	}
	for (int i = 0; i < 2; ++i) {
		Aeron_ReleaseGpuFence(pending[i]);
	}
	*wall_us = Aeron_NowUs() - start_us;
	*cpu_us = cpu;
	return ok;
}

/* Returns why the film benchmark or capture can't render the frozen flight
 * frame `snap` again, or NULL if it can. */
static const char* rm_frozen_frame_problem(const XwaSnapshot* snap) {
	if (!g.initialized || !g.assets) {
		return "the HD renderer isn't running";
	}
	if (!snap || snap->scene_kind != XWA_SCENE_FLIGHT || !snap->flight_camera_valid) {
		return "the frozen frame isn't a flight view";
	}
	if (XwaRemasterEnvCapture_Pending()) {
		return "an environment capture is pending";
	}
	if (g.force_scene_render || g.last_tick != snap->tick_index || !g.scene_tex ||
		g.scene_kind != XWA_SCENE_FLIGHT) {
		/* The frame's render didn't run: HD view off, or assets still loading. */
		return "the HD renderer didn't draw the frozen frame";
	}
	return NULL;
}

/* Writes the exposure of the last render into `out`, for the log. */
static void rm_exposure_text(char* out, size_t size) {
	AeronSceneExposureReadout exposure;
	if (XwaRemasterFlight_GetExposureReadout(&exposure)) {
		snprintf(out, size, "%.5g (metered EV100 %.3f)", (double)XwaRemasterFlight_Exposure(),
				 (double)exposure.ev100);
	} else {
		snprintf(out, size, "%.5g (fixed)", (double)XwaRemasterFlight_Exposure());
	}
}

int XwaRemaster_BenchmarkFlightFrames(void) {
	const XwaSnapshot* snap = XwaSnapshot_Current();
	const char* reason = rm_frozen_frame_problem(snap);
	if (reason) {
		Aeron_LogError("xwa.benchmark", "film benchmark: can't measure: %s", reason);
		return 0;
	}

	/* The exposure sets the range of the point lights, so it changes the cost. */
	char exposure_text[64];
	rm_exposure_text(exposure_text, sizeof exposure_text);
	int view_count, view_width, view_height;
	XwaRemasterFlight_GetViewLayout(&view_count, &view_width, &view_height);
	Aeron_LogInfo("xwa.benchmark",
				  "film benchmark: measuring %d renders at %dx%d, %d %s of %dx%d, exposure %s",
				  RM_BENCHMARK_WARMUP_RENDERS + RM_BENCHMARK_SAMPLES * RM_BENCHMARK_SAMPLE_RENDERS,
				  g.render_pixel_width, g.render_pixel_height, view_count, view_count == 1 ? "view" : "views",
				  view_width, view_height, exposure_text);
	uint64_t wall_us;
	uint64_t cpu_us;
	if (!Aeron_WaitForGpuIdle() || !rm_benchmark_run(snap, RM_BENCHMARK_WARMUP_RENDERS, &wall_us, &cpu_us)) {
		Aeron_LogError("xwa.benchmark", "film benchmark: the warm-up renders failed");
		return 0;
	}
	XwaRemasterFlight_LogLodStats();
	XwaRemasterFlight_LogFrameStats();
	double frame_ms[RM_BENCHMARK_SAMPLES];
	double cpu_ms[RM_BENCHMARK_SAMPLES];
	for (int i = 0; i < RM_BENCHMARK_SAMPLES; ++i) {
		/* The counters are only reset at the start of a host frame, and all
		 * these renders are in one host frame. So the increase over the
		 * first sample, divided by its renders, gives the counts for one
		 * render. */
		AeronRenderDataStats before;
		Aeron_GetRenderDataStats(&before);
		if (!rm_benchmark_run(snap, RM_BENCHMARK_SAMPLE_RENDERS, &wall_us, &cpu_us)) {
			Aeron_LogError("xwa.benchmark", "film benchmark: the renders of sample %d failed", i + 1);
			return 0;
		}
		if (i == 0) {
			AeronRenderDataStats after;
			Aeron_GetRenderDataStats(&after);
			const double renders = RM_BENCHMARK_SAMPLE_RENDERS;
			Aeron_LogInfo(
				"xwa.benchmark",
				"film benchmark: each render: %.0f draws in %.0f render passes, %.1f command buffers "
				"submitted, %.1f KiB uploaded",
				(double)(after.draw_count - before.draw_count) / renders,
				(double)(after.render_pass_count - before.render_pass_count) / renders,
				(double)(after.command_buffer_submission_count - before.command_buffer_submission_count) /
					renders,
				(double)(after.upload_staged_bytes - before.upload_staged_bytes) / renders / 1024.0);
		}
		frame_ms[i] = (double)wall_us / 1000.0 / RM_BENCHMARK_SAMPLE_RENDERS;
		cpu_ms[i] = (double)cpu_us / 1000.0 / RM_BENCHMARK_SAMPLE_RENDERS;
		Aeron_LogInfo("xwa.benchmark", "film benchmark: sample %d: %.3f ms per frame, %.3f ms recording",
					  i + 1, frame_ms[i], cpu_ms[i]);
	}
	qsort(frame_ms, RM_BENCHMARK_SAMPLES, sizeof frame_ms[0], rm_benchmark_compare_double);
	qsort(cpu_ms, RM_BENCHMARK_SAMPLES, sizeof cpu_ms[0], rm_benchmark_compare_double);
	Aeron_LogInfo("xwa.benchmark",
				  "film benchmark: frame time: median %.3f ms, min %.3f ms, max %.3f ms; "
				  "recording: median %.3f ms",
				  frame_ms[RM_BENCHMARK_SAMPLES / 2], frame_ms[0], frame_ms[RM_BENCHMARK_SAMPLES - 1],
				  cpu_ms[RM_BENCHMARK_SAMPLES / 2]);
	return 1;
}

static float rm_half_to_float(uint16_t half) {
	const uint32_t exponent = (half >> 10) & 31u;
	const uint32_t mantissa = half & 1023u;
	if (exponent == 0) {
		const float value = mantissa ? ldexpf((float)mantissa, -24) : 0.0f;
		return (half & 0x8000u) ? -value : value;
	}
	const uint32_t bits = ((uint32_t)(half & 0x8000u) << 16) |
						  (exponent == 31 ? 0x7f800000u : (exponent + 112u) << 23) | (mantissa << 13);
	float value;
	memcpy(&value, &bits, sizeof value);
	return value;
}

/* Submits `cmd` and waits until the GPU has run it. */
static int rm_submit_and_wait(AeronCommandBuffer* cmd) {
	AeronGpuFence* fence = Aeron_SubmitCommandBufferAndAcquireFence(cmd);
	if (!fence) {
		return 0;
	}
	const int waited = Aeron_WaitForGpuFence(fence);
	Aeron_ReleaseGpuFence(fence);
	return waited;
}

/* Reads the RGBA16F target `target` of `width` x `height` back as RGB
 * floats, top row first. Negative and non-finite values become 0. */
static int rm_read_rgb(AeronRenderTarget* target, int width, int height, uint16_t* half, float* rgb) {
	const size_t pixels = (size_t)width * (size_t)height;
	AeronTexture* texture = target ? Aeron_RenderTargetGetTexture(target) : NULL;
	if (!texture || Aeron_TextureGetWidth(texture) != width || Aeron_TextureGetHeight(texture) != height ||
		Aeron_TextureGetFormat(texture) != AERON_TEXTURE_FORMAT_RGBA16_FLOAT ||
		!Aeron_ReadRenderTargetRawPixels(target, half, pixels * 8u, (size_t)width * 8u)) {
		return 0;
	}
	for (size_t i = 0; i < pixels; ++i) {
		for (int c = 0; c < 3; ++c) {
			const float value = rm_half_to_float(half[i * 4u + (size_t)c]);
			rgb[i * 3u + (size_t)c] = value > 0.0f && isfinite(value) ? value : 0.0f;
		}
	}
	return 1;
}

int XwaRemaster_CaptureFlightFrame(const char* name) {
	const XwaSnapshot* snap = XwaSnapshot_Current();
	const char* reason = rm_frozen_frame_problem(snap);
	/* A headset's frame is in the eye images, which can't be read back. */
	if (!reason && g.rig_kind == RM_RIG_HEADSET) {
		reason = "the frame is drawn for the headset; start with --no-vr to capture";
	}
	if (reason) {
		Aeron_LogError("xwa.benchmark", "film capture: can't capture: %s", reason);
		return 0;
	}
	const int width = g.render_pixel_width;
	const int height = g.render_pixel_height;
	const size_t pixels = (size_t)width * (size_t)height;
	char png_path[1100];
	char hdr_path[1100];
	snprintf(png_path, sizeof png_path, "capture/%s.png", name);
	snprintf(hdr_path, sizeof hdr_path, "capture/%s.hdr", name);
	uint16_t* half = malloc(pixels * 8u);
	float* rgb = malloc(pixels * 3u * sizeof *rgb);
	uint8_t* bytes = malloc(pixels * 3u);
	const char* failure = NULL;

	if (!half || !rgb || !bytes) {
		failure = "out of memory";
	} else if (!AeronVfs_CreateDirectory(Aeron_GetVfs(), AERON_VFS_ROOT_USER, "capture")) {
		failure = "the capture folder couldn't be created in the user folder";
	}
	/* The final image, as the composed presentation shows it. */
	if (!failure) {
		AeronCommandBuffer* cmd = Aeron_AcquireCommandBuffer();
		AeronTexture* texture = NULL;
		if (cmd) {
			XwaRemasterAssets_UpdateFrameSets(g.assets, snap);
			Aeron_GpuDebugPush(cmd, "OpenXWA film capture frame");
			texture = XwaRemasterFlight_Render(cmd, snap, g.assets, width, height, /*direct_present=*/0);
			Aeron_GpuDebugPop(cmd);
			if (!texture) {
				Aeron_CancelCommandBuffer(cmd);
			}
		}
		if (!texture || !rm_submit_and_wait(cmd) ||
			!rm_read_rgb(XwaRemasterFlight_ComposedTarget(), width, height, half, rgb)) {
			failure = "the frame's render failed";
		}
	}
	if (!failure) {
		/* The image holds linear display values. The PNG stores them in sRGB. */
		for (size_t i = 0; i < pixels * 3u; ++i) {
			const float value = fminf(rgb[i], 1.0f);
			const float srgb =
				value <= 0.0031308f ? 12.92f * value : 1.055f * powf(value, 1.0f / 2.4f) - 0.055f;
			bytes[i] = (uint8_t)(srgb * 255.0f + 0.5f);
		}
		if (!Aeron_ImageWritePngRgb8(Aeron_GetVfs(), AERON_VFS_ROOT_USER, png_path, width, height, bytes)) {
			failure = "the PNG couldn't be written";
		}
	}
	/* The same frame, before the tonemap. With several views the scene only
	 * holds its last view, so the frame is rendered again, with each view
	 * composed before the tonemap. */
	int view_count;
	XwaRemasterFlight_GetViewLayout(&view_count, NULL, NULL);
	if (!failure) {
		AeronCommandBuffer* cmd = Aeron_AcquireCommandBuffer();
		int recorded = 0;
		if (cmd && view_count > 1) {
			XwaRemasterFlight_SetComposeBeforeTonemap(1);
			recorded =
				XwaRemasterFlight_Render(cmd, snap, g.assets, width, height, /*direct_present=*/0) != NULL;
			XwaRemasterFlight_SetComposeBeforeTonemap(0);
		} else if (cmd) {
			recorded = XwaRemasterFlight_RecordBeforeTonemap(cmd);
		}
		if (cmd && !recorded) {
			Aeron_CancelCommandBuffer(cmd);
		}
		if (!recorded || !rm_submit_and_wait(cmd) ||
			!rm_read_rgb(XwaRemasterFlight_ComposedTarget(), width, height, half, rgb)) {
			failure = "the render before the tonemap failed";
		} else if (!Aeron_ImageWriteHdr(Aeron_GetVfs(), AERON_VFS_ROOT_USER, hdr_path, width, height, rgb)) {
			failure = "the HDR image couldn't be written";
		}
	}
	free(half);
	free(rgb);
	free(bytes);
	if (failure) {
		Aeron_LogError("xwa.benchmark", "film capture: %s", failure);
		return 0;
	}
	char exposure_text[64];
	rm_exposure_text(exposure_text, sizeof exposure_text);
	if (view_count == 1) {
		Aeron_LogInfo("xwa.benchmark", "film capture: wrote %s and %s in the user folder, %dx%d, exposure %s",
					  png_path, hdr_path, width, height, exposure_text);
	} else {
		Aeron_LogInfo("xwa.benchmark",
					  "film capture: wrote %s and %s in the user folder, %dx%d with %d views, exposure %s",
					  png_path, hdr_path, width, height, view_count, exposure_text);
	}
	return 1;
}

void XwaRemaster_Shutdown(void) {
	XwaRemasterHud_Shutdown();
	XwaRemasterCutscene_Shutdown();
	XwaRemasterFrontend_Shutdown();
	XwaRemasterCapturedLighting_Shutdown();
	XwaRemasterFlight_Shutdown();
	XwaRemasterShip_Shutdown();
	if (g.assets) {
		XwaRemasterAssets_Destroy(g.assets);
	}
	memset(&g, 0, sizeof g);
}
