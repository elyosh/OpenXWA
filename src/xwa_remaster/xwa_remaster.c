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
#include "aeron/scene/blend_ramp.h"
#include "xwa_remaster/assets.h"
#include "xwa_remaster/cutscene.h"
#include "xwa_remaster/debug_tools.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/frontend.h"
#include "xwa_remaster/hud.h"
#include "xwa_remaster/ship.h"
#include "xwa_runtime/runtime/movie_task.h"
#include "xwa_runtime/runtime/port.h"
#include "xwa_runtime/runtime/presentation.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <stdio.h>
#include <string.h>

#define RM_KEY_SPLIT_VIEW (AERON_KEY_F1 + 1)   /* F2 */
#define RM_KEY_PRIMARY_VIEW (AERON_KEY_F1 + 4) /* F5 */
#define RM_RESIZE_SETTLE_US 150000u
#define RM_ASSET_UPLOAD_BYTE_BUDGET (64u * 1024u * 1024u)
#define RM_ASSET_UPLOAD_COPY_BUDGET 4096u

typedef enum {
	RM_VIEW_HD = 0, /* start on the reconstruction */
	RM_VIEW_SPLIT,
	RM_VIEW_CLASSIC
} RmViewMode;

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
	/* HDR output: desired flag + deferred apply (see the header). */
	int hdr_desired;
	int hdr_apply_pending;
	int pending_mode_valid;
	RmViewMode pending_mode;
	int pending_mode_fade;
	uint64_t pending_classic_frame_serial;
	int classic_suppressed_this_frame;
	int render_pixel_width;
	int render_pixel_height;
	int observed_pixel_width;
	int observed_pixel_height;
	uint64_t observed_size_since_us;
	int force_scene_render;
	int flight_render_suspended;
	AeronSampleCount msaa_samples;
} g;

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

AeronSampleCount XwaRemaster_MsaaSampleCount(void) {
	return g.msaa_samples;
}

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

static int XwaRemaster_VideoOptionsValid(const XwaModernVideoOptions* options) {
	return options && options->ssao_quality >= XWA_MODERN_SSAO_OFF &&
		   options->ssao_quality <= XWA_MODERN_SSAO_HIGH && options->fsr_upscaling >= XWA_MODERN_FSR_OFF &&
		   options->fsr_upscaling <= XWA_MODERN_FSR_NATIVE_AA && options->msaa >= XWA_MODERN_MSAA_OFF &&
		   options->msaa <= XWA_MODERN_MSAA_8X &&
		   (options->fsr_upscaling == XWA_MODERN_FSR_OFF || options->msaa == XWA_MODERN_MSAA_OFF) &&
		   options->motion_blur_quality >= XWA_MODERN_MOTION_BLUR_OFF &&
		   options->motion_blur_quality <= XWA_MODERN_MOTION_BLUR_HIGH;
}

void XwaRemaster_GetVideoOptions(XwaModernVideoOptions* out) {
	XwaFlightSsaoParams ssao;
	XwaFlightMotionBlurParams motion_blur;
	XwaFlightTemporalParams temporal;

	if (!out) {
		return;
	}
	memset(out, 0, sizeof *out);
	memset(&ssao, 0, sizeof ssao);
	memset(&motion_blur, 0, sizeof motion_blur);
	XwaRemasterFlight_GetSsao(&ssao);
	XwaRemasterFlight_GetMotionBlur(&motion_blur);
	XwaRemasterFlight_GetTemporal(&temporal);
	out->ssao_quality = (XwaModernSsaoQuality)ssao.quality;
	out->fsr_upscaling = XwaRemaster_FromTemporalMode(temporal.mode);
	out->msaa = XwaRemaster_FromSampleCount(XwaRemaster_MsaaSampleCount());
	out->motion_blur_quality = (XwaModernMotionBlurQuality)motion_blur.quality;
	out->hdr_output = XwaRemaster_GetHdrDesired();
}

void XwaRemaster_SetVideoOptions(const XwaModernVideoOptions* options) {
	XwaFlightSsaoParams ssao;
	XwaFlightMotionBlurParams motion_blur;
	XwaFlightTemporalParams temporal;

	if (!XwaRemaster_VideoOptionsValid(options)) {
		return;
	}
	XwaRemasterFlight_GetSsao(&ssao);
	ssao.quality = options->ssao_quality;
	XwaRemasterFlight_SetSsao(&ssao);

	XwaRemasterFlight_GetMotionBlur(&motion_blur);
	motion_blur.quality = options->motion_blur_quality;
	XwaRemasterFlight_SetMotionBlur(&motion_blur);

	g.msaa_samples = XwaRemaster_ToSampleCount(options->msaa);
	XwaRemasterFlight_GetTemporal(&temporal);
	temporal.mode = XwaRemaster_ToTemporalMode(options->fsr_upscaling);
	XwaRemasterFlight_SetTemporal(&temporal);

	XwaRemaster_SetHdrDesired(options->hdr_output);
}

int XwaRemaster_Init(const XwaRemasterInitOptions* options) {
	XwaModernVideoOptions video_options;
	unsigned int video_override_mask;

	if (g.initialized) {
		return g.assets != NULL;
	}
	if (!options) {
		Aeron_Log("xwa.remaster", "resolved initialization options are required");
		return 0;
	}
	g.initialized = 1;
	XwaRemasterShip_Configure(options->opt_smooth_angle_degrees, options->opt_emissive_strength,
							  options->opt_projectile_emissive_strength, options->force_opt_models);
	if (!XwaRemasterFlight_InitConfig(Aeron_GetVfs())) {
		return 0;
	}
	XwaRemasterFlight_GetPresentationDefaults(&g.msaa_samples, &g.hdr_desired);
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
	if (video_override_mask & XWA_MODERN_VIDEO_OVERRIDE_HDR) {
		video_options.hdr_output = options->video_options.hdr_output;
	}
	XwaRemaster_SetVideoOptions(&video_options);
	g.hdr_apply_pending = 0;
	/* Aeron downgrades to SDR by itself when HDR is unavailable and re-applies
	 * the request if the display's HDR state changes later, so a failure here
	 * means the swapchain could not be configured at all. */
	if (!Aeron_SetOutputHdr(g.hdr_desired)) {
		Aeron_Log("xwa.remaster", "HDR output initialization failed");
		return 0;
	}
	Aeron_Log("xwa.remaster", "HDR output: desired %s, %s (headroom %.2f)", g.hdr_desired ? "on" : "off",
			  Aeron_OutputHdrStatusName(Aeron_OutputHdrStatus()), (double)Aeron_OutputHdrHeadroom());
	char remaster_root[1024];
	snprintf(remaster_root, sizeof remaster_root, "%s/remaster", Aeron_AssetRoot());
	g.assets = XwaRemasterAssets_Create(remaster_root, options->prefer_original_2d);
	Aeron_BlendRampInit(&g.ramp);
	g.mode = RM_VIEW_HD;
	g.primary_mode = RM_VIEW_HD;
	if (!Aeron_GetPresentationPixelSize(&g.render_pixel_width, &g.render_pixel_height)) {
		g.render_pixel_width = XWA_PRESENTATION_WIDTH;
		g.render_pixel_height = XWA_PRESENTATION_HEIGHT;
	}
	g.observed_pixel_width = g.render_pixel_width;
	g.observed_pixel_height = g.render_pixel_height;
	if (!g.assets) {
		Aeron_Log("xwa.remaster", "asset resolver init failed");
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
	Aeron_Log("xwa.remaster", "view mode: %s",
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

static void XwaRemaster_ToggleSplitView(void) {
	const RmViewMode selected_mode = g.pending_mode_valid ? g.pending_mode : g.mode;
	const RmViewMode target = selected_mode == RM_VIEW_SPLIT ? g.primary_mode : RM_VIEW_SPLIT;
	XwaRemaster_RequestViewMode(target, 0);
}

static void XwaRemaster_TogglePrimaryView(void) {
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

static void XwaRemaster_FatalGpu(const char* operation) {
	Aeron_RequestFatalRendererError(operation);
}

static void XwaRemaster_UpdateRenderSize(void) {
	int width;
	int height;
	if (!Aeron_GetPresentationPixelSize(&width, &height)) {
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
	}
	Aeron_Log("xwa.remaster", "physical render size: %dx%d", width, height);
}

void XwaRemaster_BeginFrame(const AeronInputSnapshot* input) {
	if (!g.initialized || !g.assets) {
		XwaPort_SetClassicFlightRenderingEnabled(1);
		return;
	}
	XwaRemaster_UpdateRenderSize();

	/* Deferred HDR composition flip (see XwaRemaster_SetHdrDesired). */
	if (g.hdr_apply_pending) {
		g.hdr_apply_pending = 0;
		if (!Aeron_SetOutputHdr(g.hdr_desired)) {
			XwaRemaster_FatalGpu("HDR output reconfiguration");
			return;
		}
	}

	if (input && input->key_pressed[AERON_KEY_GRAVE] && Aeron_DebugUiAvailable()) {
		Aeron_DebugUiToggle();
	}

	if (input && input->key_pressed[RM_KEY_PRIMARY_VIEW]) {
		XwaRemaster_TogglePrimaryView();
	} else if (input && input->key_pressed[RM_KEY_SPLIT_VIEW]) {
		XwaRemaster_ToggleSplitView();
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
	if (!snap) {
		return;
	}
	if (g.force_scene_render && snap->scene_kind != XWA_SCENE_FLIGHT &&
		snap->scene_kind != XWA_SCENE_CUTSCENE) {
		g.force_scene_render = 0;
	}

	/* Render on new sim ticks and reuse the persistent texture between them.
	 * Keep the last complete render across a scene transition until the new
	 * scene has finished any incremental asset synchronization. */
	if (snap->scene_kind != g.last_kind) {
		g.last_kind = snap->scene_kind;
		/* Cutscenes supply their opaque video independently of scene_tex; their
		 * optional subtitle texture is not a valid transition background. */
		if (snap->scene_kind == XWA_SCENE_CUTSCENE ||
			(g.scene_tex && g.scene_kind == XWA_SCENE_CUTSCENE)) {
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
	const int presentation_change =
		snap->scene_kind == XWA_SCENE_FLIGHT && g.scene_tex && g.scene_kind == XWA_SCENE_FLIGHT &&
		direct_present != g.scene_is_direct;
	/* Flight shutdown commits once after the task has released its camera and
	 * before the frontend callback draws its first frame. Preserve and, when
	 * necessary, resolve the last complete flight image before its assets are
	 * eligible for reconciliation. */
	const int retain_invalid_flight_frame =
		snap->scene_kind == XWA_SCENE_FLIGHT && !snap->flight_camera_valid && g.scene_tex &&
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
	const int assets_need_sync =
		!retain_invalid_flight_frame &&
		(frontend_assets_need_sync || ship_assets_need_sync || texture_assets_need_sync ||
		 process_assets_need_prepare);
	const int render_snapshot = snap->tick_index != g.last_tick || g.force_scene_render;
	const int retain_scene_frame =
		!render_snapshot || (snap->scene_kind == XWA_SCENE_LOADING && !XwaRemaster_SnapshotHasPresent(snap)) ||
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
			ship_sync = XWA_REMASTER_SHIP_SYNC_FAILED;
		}
		if (ship_sync == XWA_REMASTER_SHIP_SYNC_COMPLETE && ship_assets_need_sync) {
			ship_sync = XwaRemasterShip_SyncAssets(upload_cmd, snap,
												  RM_ASSET_UPLOAD_BYTE_BUDGET,
												  RM_ASSET_UPLOAD_COPY_BUDGET);
		}
		if (ship_sync == XWA_REMASTER_SHIP_SYNC_COMPLETE) {
			if (!XwaRemasterAssets_SyncFlightTextures(g.assets, upload_cmd, snap)) {
				ship_sync = XWA_REMASTER_SHIP_SYNC_FAILED;
			} else {
				texture_assets_prepared = texture_assets_need_sync;
			}
			if (ship_sync == XWA_REMASTER_SHIP_SYNC_COMPLETE && process_assets_need_prepare) {
				process_assets_prepared =
					XwaRemasterFlight_PrepareProcessAssets(upload_cmd, g.assets);
				if (!process_assets_prepared) {
					ship_sync = XWA_REMASTER_SHIP_SYNC_FAILED;
				}
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
			Aeron_Log("xwa.remaster",
					  "asset upload batch: staged=%llu reserved=%llu chunks=%u copies=%u passes=%u largest=%u",
					  (unsigned long long)upload_usage.staged_bytes,
					  (unsigned long long)upload_usage.reserved_bytes, upload_usage.chunk_count,
					  upload_usage.copy_count, upload_usage.copy_pass_count,
					  upload_usage.largest_upload_bytes);
		}
		if (ship_sync == XWA_REMASTER_SHIP_SYNC_MORE) {
			assets_pending = 1;
		}
	}

	/* Required classic output is re-submitted on idle game ticks. Opaque HD
	 * flight frames omit that layer; the remaster texture below then owns the
	 * complete presentation frame. */
	if (!assets_pending && flight_render_needed && (render_snapshot || presentation_change) &&
		(!retain_scene_frame || presentation_change)) {
		AeronTexture* next_scene_tex = g.scene_tex;
		int next_scene_is_direct = g.scene_is_direct;
		int render_output_required = retain_scene_frame;
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
				case XWA_SCENE_FRONTEND_MODAL:
					render_output_required = 1;
					Aeron_GpuDebugPush(cmd, "OpenXWA frontend reconstruction");
					next_scene_tex = XwaRemasterFrontend_Render(cmd, snap, g.assets);
					next_scene_is_direct = 0;
					Aeron_GpuDebugPop(cmd);
					break;
				case XWA_SCENE_FLIGHT:
					render_output_required =
						snap->flight_camera_valid && !snap->flight_camera.map_mode;
					Aeron_GpuDebugPush(cmd, "OpenXWA flight renderer");
					next_scene_tex =
						snap->flight_camera_valid
							? XwaRemasterFlight_Render(cmd, snap, g.assets, g.render_pixel_width,
													   g.render_pixel_height, direct_present)
							: NULL;
					next_scene_is_direct =
						next_scene_tex && XwaRemasterFlight_DirectPresentationReady();
					Aeron_GpuDebugPop(cmd);
					break;
				case XWA_SCENE_CUTSCENE:
					Aeron_GpuDebugPush(cmd, "OpenXWA cutscene subtitles");
					next_scene_tex = XwaRemasterCutscene_Render(
						cmd, snap, g.assets, g.render_pixel_width, g.render_pixel_height);
					next_scene_is_direct = 0;
					Aeron_GpuDebugPop(cmd);
					break;
				default:
					next_scene_tex = NULL;
					next_scene_is_direct = 0;
					break;
			}
		} else {
			render_output_required = 1;
			Aeron_GpuDebugPush(cmd, "OpenXWA flight presentation transition");
			next_scene_tex = XwaRemasterFlight_ResolvePresentation(cmd, direct_present);
			next_scene_is_direct =
				next_scene_tex && XwaRemasterFlight_DirectPresentationReady();
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
		g.scene_is_direct = next_scene_is_direct;
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
				.color_space = AERON_COLOR_SPACE_LINEAR_SRGB,
			};
			if (!Aeron_SubmitTextureLayer(&layer)) {
				XwaRemaster_FatalGpu("cutscene subtitle presentation");
				return;
			}
			XwaMovieTask_SuppressClassicSubtitles();
		}
		return;
	}

	/* Crossfade toward the overlay unless CLASSIC / nothing rendered. */
	const float target = (g.scene_tex && g.mode != RM_VIEW_CLASSIC) ? 1.0f : 0.0f;
	g.ramp.target = target;
	Aeron_BlendRampAdvance(&g.ramp, delta_us, target);
	if (!g.scene_tex || g.ramp.alpha <= 0.001f) {
		if (g.classic_suppressed_this_frame) {
			XwaPort_SubmitRetainedClassicFrame();
		}
		return; /* classic only — submit nothing */
	}
	if (g.scene_is_direct) {
		if ((g.scene_kind != snap->scene_kind || direct_present) &&
			!XwaRemasterFlight_SubmitDirectPresentation()) {
			XwaRemaster_FatalGpu("direct flight presentation");
		}
		return;
	}

	/* Frontend reconstruction remains a 4:3 texture inside the classic
	 * safe frame. Modern flight owns the full presentation frame; HUD
	 * profiles change only the authored overlay layout. */
	const XwaPresentationRect pr =
		g.scene_kind == XWA_SCENE_FLIGHT ? XwaPresentation_Frame() : XwaPresentation_ClassicSafeFrame();
	AeronTextureLayerDesc layer = {
		.texture = g.scene_tex,
		.logical_rect = { pr.x, pr.y, pr.width, pr.height },
		.blend_mode = AERON_LAYER_BLEND_PREMULTIPLIED,
		.color_space = AERON_COLOR_SPACE_LINEAR_SRGB,
		.tint_enabled = 1,
		.tint_rgba = { g.ramp.alpha, g.ramp.alpha, g.ramp.alpha, g.ramp.alpha },
	};
	if (g.mode == RM_VIEW_SPLIT) {
		layer.scissor = (AeronRectI) { pr.x + pr.width / 2, pr.y, pr.width / 2, pr.height };
	}
	if (!Aeron_SubmitTextureLayer(&layer)) {
		XwaRemaster_FatalGpu("HD texture-layer presentation");
	}
}

void XwaRemaster_Shutdown(void) {
	XwaRemasterHud_Shutdown();
	XwaRemasterCutscene_Shutdown();
	XwaRemasterFrontend_Shutdown();
	XwaRemasterFlight_Shutdown();
	XwaRemasterShip_Shutdown();
	if (g.assets) {
		XwaRemasterAssets_Destroy(g.assets);
	}
	memset(&g, 0, sizeof g);
}
