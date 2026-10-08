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
#include "aeron/scene/scene3d.h"
#include "aeron/scene/settings.h"
#include "aeron/vfs.h"
#include "xwa_remaster/assets.h"
#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Common scene settings are loaded from Aeron's Scene3D defaults, then mutated
 * live by the debug inspector or user menu. Distances are native view units. */
typedef AeronSceneSsaoSettings XwaFlightSsaoParams;

int XwaRemasterFlight_InitConfig(AeronVfs* vfs);
/* Returns the required shipped presentation baseline loaded by InitConfig. */
void XwaRemasterFlight_GetPresentationDefaults(AeronSampleCount* msaa_samples, int* hdr_output);
void XwaRemasterFlight_GetSsao(XwaFlightSsaoParams* out);
void XwaRemasterFlight_SetSsao(const XwaFlightSsaoParams* in);
void XwaRemasterFlight_GetSsaoDefault(XwaFlightSsaoParams* out);

typedef AeronSceneShadowSettings XwaFlightShadowParams;

void XwaRemasterFlight_GetShadows(XwaFlightShadowParams* out);
void XwaRemasterFlight_SetShadows(const XwaFlightShadowParams* in);
void XwaRemasterFlight_GetShadowsDefault(XwaFlightShadowParams* out);
void XwaRemasterFlight_GetShadowAtlasDebug(int* enabled, int* cascade);
void XwaRemasterFlight_SetShadowAtlasDebug(int enabled, int cascade);
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

/* Point-light knobs (remaster/config.yaml `point_lights:` mapping + the debug
 * inspector). The classic SOURCE laws fix each light's color and classic
 * intensity; the scene evaluates them as physical sphere lights as bright
 * as the classic law at XWA_SHIP_POINT_LIGHT_MATCH_DISTANCE_M
 * (XwaRemasterShip_ScenePointLight), so scale 1 keeps that calibration. */
typedef struct XwaFlightPointLightParams {
	int enabled;
	int clustered;
	int cluster_depth_slices;
	int cluster_debug;
	float scale;        /* classic-source intensity multiplier */
	float range_scale;  /* multiplies the derived falloff range */
	float spec_weight;  /* 0..2 — punctual specular lobe weight */
	float model_light_intensity; /* intensity multiplier of lights authored in models */
} XwaFlightPointLightParams;

typedef struct XwaFlightPointLightStats {
	uint32_t generated_count;
	uint32_t valid_count;
	uint32_t invalid_count;
	uint32_t candidate_overflow_count;
	AeronSceneClusteredLightStats scene;
} XwaFlightPointLightStats;

void XwaRemasterFlight_GetPointLights(XwaFlightPointLightParams* out);
void XwaRemasterFlight_SetPointLights(const XwaFlightPointLightParams* in);
void XwaRemasterFlight_GetPointLightsDefault(XwaFlightPointLightParams* out);
void XwaRemasterFlight_GetPointLightStats(XwaFlightPointLightStats* out);

/* Mesh-texture filtering shared by the base-color, normal,
 * metallic-roughness and emissive atlases. Loaded from remaster/config.yaml's
 * `texture_filtering:` mapping and live-editable in the debug UI. */
typedef struct XwaFlightTextureFilteringParams {
	int anisotropic;
	float max_anisotropy;
} XwaFlightTextureFilteringParams;

void XwaRemasterFlight_GetTextureFiltering(XwaFlightTextureFilteringParams* out);
void XwaRemasterFlight_SetTextureFiltering(const XwaFlightTextureFilteringParams* in);
void XwaRemasterFlight_GetTextureFilteringDefault(XwaFlightTextureFilteringParams* out);

/* Motion-blur knobs (remaster/config.yaml `motion_blur:` mapping + the debug
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

/* Procedural hyperspace-tunnel settings loaded from remaster/config.yaml and
 * live-editable through the debug inspector. The preview override affects
 * rendering only; it never changes the captured flight simulation state. */
typedef struct XwaFlightHyperspaceTunnelParams {
	float travel_speed;
	float rotation_speed;
	float noise_scale;
	float brightness;
	float highlight_strength;
	float focal_length;
	float twist;
	float cap_radius;
	float cap_falloff;
	float mesh_ambient_strength;
	float mesh_environment_roughness;
	float mesh_key_strength;
	float dark_color[3];
	float body_color[3];
	float highlight_color[3];
	float cap_color[3];
} XwaFlightHyperspaceTunnelParams;

void XwaRemasterFlight_GetHyperspaceTunnel(XwaFlightHyperspaceTunnelParams* out);
void XwaRemasterFlight_SetHyperspaceTunnel(const XwaFlightHyperspaceTunnelParams* in);
void XwaRemasterFlight_GetHyperspaceTunnelDefault(XwaFlightHyperspaceTunnelParams* out);
void XwaRemasterFlight_SetHyperspaceTunnelPreview(int enabled);
int XwaRemasterFlight_HyperspaceTunnelPreviewEnabled(void);

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

/* Snapshot-pose helpers shared by scene instances and world-linked HUD elements.
 * Matrices accept model positions in metres with OPT axes and produce render-local
 * positions in native XWA units. Origin and camera delta also use native XWA units.
 * AtOrigin includes object translation; ForCameraDelta returns only the basis
 * and metre-to-native scale, with optional projectile roll alignment. */
int XwaRemasterFlight_ObjectModelMatrixAtOrigin(const XwaFlightObject* object, const int32_t origin_world[3],
												float out[16]);
int XwaRemasterFlight_ObjectModelMatrixForCameraDelta(const XwaFlightObject* object,
													  const float camera_minus_object[3], int roll_align,
													  float out[16]);

/* Transform a model-space point in native OPT units through a mesh instance's
 * row-major affine matrix (model metres -> scene units). Works with both the
 * flight render-local and PiP eye-local transforms; translation is not rescaled. */
void XwaRemasterFlight_TransformOptPoint(const float model_meters_to_scene[16], const float point_opt[3],
										 float out_scene[3]);

/* One-shot process assets prepared by the first flight loading snapshot.
 * PrepareProcessAssets requires no open GPU render pass. */
int XwaRemasterFlight_ProcessAssetsNeedPrepare(void);
int XwaRemasterFlight_PrepareProcessAssets(AeronCommandBuffer* cmd, XwaRemasterAssets* assets);
void XwaRemasterFlight_CommitProcessAssets(void);

/* ---- environment capture support (env_capture.c) ----------------------
 * The capture renders the HD sky into its own scenes with the flight
 * module's sky configuration and backdrop derivation. Capture cameras sit
 * at the render-local origin; the sky is translation independent. */

/* Backdrop selection for XwaRemasterFlight_CaptureBackdrops: every
 * record except backdrop suns, or one record index (>= 0). */
#define XWA_FLIGHT_CAPTURE_BACKDROPS_NO_SUNS (-2)

struct XwaRemasterSkyStars;

typedef enum XwaFlightCaptureSkyMode {
	XWA_FLIGHT_CAPTURE_SKY_NONE,  /* cube mode without a loaded cube */
	XWA_FLIGHT_CAPTURE_SKY_STARS, /* procedural starfield */
	XWA_FLIGHT_CAPTURE_SKY_CUBE,  /* configured sky cube */
} XwaFlightCaptureSkyMode;

typedef struct XwaFlightCaptureSky {
	XwaFlightCaptureSkyMode mode;
	float exposure;
	float star_brightness;
	float star_density;
	float star_grid;
} XwaFlightCaptureSky;

/* The configured sky source the flight frame draws outside authored mode. */
void XwaRemasterFlight_CaptureSkyInfo(XwaFlightCaptureSky* out);

/* A square perspective capture camera with half-angle tangent `half_tan`
 * and world-space view axes (unit, right x down = forward), and the
 * world->view rows re-derived from its quaternion, as the flight frame
 * does. Returns 0 for an invalid basis. */
int XwaRemasterFlight_CaptureCamera(const float right[3], const float down[3], const float forward[3],
									float half_tan, int size, AeronSceneCamera* camera, float crows[9]);

/* After AeronScene_Begin: draws the configured sky into `scene`. Stars use
 * `stars` (the scene's BEFORE_OPAQUE hook) with the classic-pixel dot
 * geometry scaled by `star_upscale`. Returns 0 on failure. */
int XwaRemasterFlight_CaptureSetSky(AeronScene3D* scene, struct XwaRemasterSkyStars* stars,
									const XwaSnapshot* snap, float star_upscale);

/* Derives the region's backdrop quads for the capture camera rows `crows`
 * and, when `scene` is not NULL, submits them as SKY billboards. Axis
 * quads are kept by their own direction instead of the camera hemisphere.
 * `out_max_tan` receives the largest |x| / z or |y| / z over the quad
 * corners, or a negative value when a corner is not in front of the
 * camera. Returns the quad count. */
uint32_t XwaRemasterFlight_CaptureBackdrops(AeronScene3D* scene, const float crows[9],
											const XwaSnapshot* snap, XwaRemasterAssets* assets, int filter,
											float* out_max_tan);

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
