#ifndef XWA_REMASTER_FLIGHT_H
#define XWA_REMASTER_FLIGHT_H

/*
 * Flight-scene HD driver: renders the captured flight frame —
 * craft/static/projectile OPT models, the
 * player cockpit, region backdrops, queued billboards and engine
 * glows — as a full-frame AeronScene3D-class pass, replicating the
 * classic pipeline's transforms exactly:
 *
 *   - object->eye basis mirrors FVIEW_ComputeObjectViewMatrix: the
 *     object matrix rows are (side, up, -fwd) and the compose reads
 *     them in (R0, R2, R1) order against the camera rows;
 *   - projection preserves the captured classic vertical framing and
 *     center offsets, then derives horizontal field of view from the
 *     renderer's full output aspect (vertical-constant / Hor+);
 *   - backdrop quads and glow fans replay engine-resolved geometry;
 *     billboards unproject their captured screen placement through
 *     the same focal, with the classic (tex_dim*size)>>9 extents.
 *
 * Consumes xwa_snapshot.h channels + the shared craft policy
 * (ship.c) and asset resolver only.
 */

#include "aeron/render.h"
#include "aeron/vfs.h"
#include "aeron/scene/scene3d.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SSAO knobs loaded from mandatory render.yaml, then mutated live by
 * the debug inspector or user menu (applied on the next rendered
 * frame). The user menu persists only the quality selection.
 * radius/bias are view-space units (XWA world unit ~= 1/40.96 m). */
typedef struct XwaFlightSsaoParams {
	int quality; /* 0 = off, 1 = low (8-tap), 2 = high (16-tap + blur) */
	float intensity;
	float power;
	float radius_view;
	float bias_view;
	float direct;
	int debug_viz;
	/* Screen-space radius clamp (fractions of NDC half-width; 0 = off).
	 * min_screen_frac raises the effective sampling radius on distant surfaces
	 * to remove the kernel-aliasing grid; max_screen_frac caps it on near
	 * surfaces so the finite tap set stays dense. */
	float min_screen_frac;
	float max_screen_frac;
	/* Per-pixel kernel-radius jitter [0,1]; breaks the discrete taps into
	 * blur-smoothable noise so a large footprint does not resolve into
	 * separate offset occlusions on near surfaces. 0 = off. */
	float sample_jitter;
} XwaFlightSsaoParams;

int XwaRemasterFlight_InitConfig(AeronVfs* vfs);
/* Returns the required shipped presentation baseline loaded by InitConfig. */
void XwaRemasterFlight_GetPresentationDefaults(AeronSampleCount* msaa_samples, int* hdr_output);
void XwaRemasterFlight_GetSsao(XwaFlightSsaoParams* out);
void XwaRemasterFlight_SetSsao(const XwaFlightSsaoParams* in);
void XwaRemasterFlight_GetSsaoDefault(XwaFlightSsaoParams* out);

enum {
	XWA_FLIGHT_SHADOWS_OFF = 0,
	XWA_FLIGHT_SHADOWS_PCF = 1,
};

/* Global directional-shadow settings (`shadows:` in render.yaml). */
typedef struct XwaFlightShadowParams {
	int mode;
	int atlas_size;
	int cascade_count;
	int fit_mode; /* AeronSceneShadowFitMode */
	float max_distance;
	float split_lambda;
	int explicit_splits;
	/* Normalized over [camera near, max_distance]; first cascade_count - 1 are active. */
	float split_positions[3];
	int filter_quality;
	float filter_radius;
	int contact_hardening;
	float light_angular_radius_degrees; /* source half-angle */
	float max_filter_radius;            /* atlas texels */
	float pcss_min_filter_radius;       /* atlas texels */
	float normal_bias;
	int normal_bias_face_normal;
	float depth_bias_texels;
	float slope_bias;
	float receiver_plane_bias;
	float caster_constant_bias;
	float caster_slope_bias;
	float transition_fraction;
	float distance_fade_fraction;
	int debug_cascades;
	int debug_atlas;
	int debug_atlas_cascade; /* -1 = whole atlas, 0..3 = one cascade */
} XwaFlightShadowParams;

void XwaRemasterFlight_GetShadows(XwaFlightShadowParams* out);
void XwaRemasterFlight_SetShadows(const XwaFlightShadowParams* in);
void XwaRemasterFlight_GetShadowsDefault(XwaFlightShadowParams* out);
void XwaRemasterFlight_GetShadowStats(AeronSceneDirectionalShadowStats* out);

/* Artificial ceiling light used only during the enclosed hangar scene.
 * Direction follows XwaDirLight's surface-to-light world convention. */
typedef struct XwaFlightHangarLightingParams {
	int enabled;
	float direction[3];
	float intensity;
	float color[3]; /* sRGB directional-light color */
	float ambient_ceiling;
	float ambient_sides;
	float ambient_floor;
	float shadow_filter_radius;
} XwaFlightHangarLightingParams;

void XwaRemasterFlight_GetHangarLighting(XwaFlightHangarLightingParams* out);
void XwaRemasterFlight_SetHangarLighting(const XwaFlightHangarLightingParams* in);
void XwaRemasterFlight_GetHangarLightingDefault(XwaFlightHangarLightingParams* out);

/* Point-light knobs (render.yaml `point_lights:` mapping + the debug
 * inspector). The classic SOURCE laws fix each light's color, classic
 * intensity and effective reach; the shader evaluates the CLASSIC
 * attenuation curve (intensity * 0.5 / d, windowed where the classic
 * contribution falls under ~1%) with modern per-pixel shading. scale
 * is therefore dimensionless (1 = classic lit-color magnitudes). */
typedef struct XwaFlightPointLightParams {
	int enabled;
	float scale;        /* classic-intensity multiplier (1 = classic) */
	float range_scale;  /* multiplies the derived visibility range */
	float min_distance; /* near clamp on 0.5/d, world units */
	float spec_weight;  /* 0..2 — punctual specular lobe weight */
	float diffuse_wrap; /* 0 = Lambert, 1 = half-Lambert (classic pool) */
	float contrib_cap;  /* per-light lit-color cap (classic saturates at 1) */
} XwaFlightPointLightParams;

void XwaRemasterFlight_GetPointLights(XwaFlightPointLightParams* out);
void XwaRemasterFlight_SetPointLights(const XwaFlightPointLightParams* in);
void XwaRemasterFlight_GetPointLightsDefault(XwaFlightPointLightParams* out);

/* Mesh-texture filtering shared by the base-color, normal,
 * metallic-roughness and emissive atlases. Loaded from render.yaml's
 * `texture_filtering:` mapping and live-editable in the debug UI. */
typedef struct XwaFlightTextureFilteringParams {
	int anisotropic;
	float max_anisotropy;
} XwaFlightTextureFilteringParams;

void XwaRemasterFlight_GetTextureFiltering(XwaFlightTextureFilteringParams* out);
void XwaRemasterFlight_SetTextureFiltering(const XwaFlightTextureFilteringParams* in);
void XwaRemasterFlight_GetTextureFilteringDefault(XwaFlightTextureFilteringParams* out);

/* Motion-blur knobs (render.yaml `motion_blur:` mapping + the debug
 * inspector) — the TIE set. Velocity derives from the prev/curr
 * snapshot pair and is normalized to XWA's original 32 ms logical
 * flight frame; the scene owns the velocity prepass + reconstruct. */
typedef struct XwaFlightMotionBlurParams {
	int quality;         /* 0 = off, 1 = low, 2 = high */
	float shutter;       /* fraction of the original logical frame (0.5 = 180deg) */
	int camera_blur;     /* fold camera rotation/translation into velocity */
	int pause_keep_blur; /* keep blurring while the sim is paused */
	int velocity_viz;    /* false-color velocity debug overlay */
} XwaFlightMotionBlurParams;

void XwaRemasterFlight_GetMotionBlur(XwaFlightMotionBlurParams* out);
void XwaRemasterFlight_SetMotionBlur(const XwaFlightMotionBlurParams* in);

/* FSR controls exposed by the debug inspector. The user menu persists only
 * the mode; sharpness and debug visualization remain session-only. */
typedef struct XwaFlightTemporalParams {
	AeronTemporalMode mode;
	float sharpness;
	int debug_view;
} XwaFlightTemporalParams;

typedef struct XwaFlightTemporalStats {
	int render_width;
	int render_height;
	int output_width;
	int output_height;
	int history_reset_active;
	uint32_t history_reset_consecutive_frames;
	char history_reset_reasons[512];
	int profile_available;
	AeronTemporalProfileInfo profile;
} XwaFlightTemporalStats;

void XwaRemasterFlight_GetTemporal(XwaFlightTemporalParams* out);
void XwaRemasterFlight_SetTemporal(const XwaFlightTemporalParams* in);
void XwaRemasterFlight_GetTemporalStats(XwaFlightTemporalStats* out);

/* Renderer-owned main-flight view. HUD layouts never participate in its
 * construction. The captured classic projection supplies vertical framing;
 * the active viewport aspect supplies the horizontal field of view. */
typedef struct XwaRemasterFlightView {
	AeronSceneCamera camera;
	float view_proj[16];
	int32_t origin_world[3];
	AeronRectI viewport;
	float classic_pixel_scale;
} XwaRemasterFlightView;

int XwaRemasterFlight_BuildView(const XwaFlightCamera* camera, int target_w, int target_h,
								XwaRemasterFlightView* out);
int XwaRemasterFlight_ProjectLocal(const XwaRemasterFlightView* view, const float local[3], float* out_x,
								   float* out_y, float* out_depth);
int XwaRemasterFlight_ProjectWorldI32(const XwaRemasterFlightView* view, const int32_t world[3], float* out_x,
									  float* out_y, float* out_depth);
int XwaRemasterFlight_ProjectView(const XwaRemasterFlightView* view, const float eye[3], float* out_x,
								  float* out_y);

/* Reproduce SceneBillboard_QueueObjectTextured's base-size arithmetic,
 * including its late int16_t queue-store / uint16_t reload.  The source
 * instance extent itself remains the engine's full 32-bit value. */
uint16_t XwaRemasterFlight_ClassicBillboardBaseSize(int32_t instance_extent, int object_type, int max_bounds);

/* Pure snapshot-pose helper shared by scene instances and world-linked HUD
 * elements. Produces the exact OPT-native model-to-render-local matrix used by
 * the modern flight renderer. */
int XwaRemasterFlight_ObjectModelMatrixAtOrigin(const XwaFlightObject* object, const int32_t origin_world[3],
												float out[16]);
int XwaRemasterFlight_ObjectModelMatrixForCameraDelta(const XwaFlightObject* object,
													  const float camera_minus_object[3], int roll_align,
													  float out[16]);

/* One-shot process assets prepared by the first flight loading snapshot.
 * PrepareProcessAssets requires no open GPU render pass. */
int XwaRemasterFlight_ProcessAssetsNeedPrepare(void);
int  XwaRemasterFlight_PrepareProcessAssets(AeronCommandBuffer* cmd, XwaRemasterAssets* assets);
void XwaRemasterFlight_CommitProcessAssets(void);

/* Returns whether the current full-frame output can use Aeron's direct
 * swapchain render layer at the requested dimensions. */
int XwaRemasterFlight_CanDirectPresent(int target_w, int target_h);

/* Render the snapshot's flight scene into the internal HDR target and prepare
 * either direct or composed presentation. Returns a borrowed persistent texture
 * used to track frame availability. `cmd` must have no open pass. */
AeronTexture* XwaRemasterFlight_Render(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
									   XwaRemasterAssets* assets, int target_w, int target_h,
									   int direct_present);

/* Re-record only the presentation stage when a persistent flight frame changes
 * between direct and texture-layer composition. */
AeronTexture* XwaRemasterFlight_ResolvePresentation(AeronCommandBuffer* cmd, int direct_present);

/* Queues the prepared direct frame into Aeron's swapchain pass. */
int XwaRemasterFlight_SubmitDirectPresentation(void);

/* Returns whether the current persistent frame was prepared for direct output. */
int XwaRemasterFlight_DirectPresentationReady(void);

/* Invalidates temporal state after a period in which flight frames were not
 * rendered. The next frame starts fresh FSR and motion-blur timing history. */
void XwaRemasterFlight_InvalidateHistory(void);

void XwaRemasterFlight_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_FLIGHT_H */
