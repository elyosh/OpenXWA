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
#include "xwa_remaster/environment.h"
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

/* Debug override of objects' material animation state (debug tools): when
 * enabled, the player's target (or every object) shows these values. */
typedef struct XwaFlightMaterialAnimOverride {
	int enabled;
	int all_objects;
	uint32_t damage_stage; /* 0-3 */
	uint32_t energy;       /* 0-3 */
	uint32_t state_mask;
} XwaFlightMaterialAnimOverride;
void XwaRemasterFlight_GetMaterialAnimOverride(XwaFlightMaterialAnimOverride* out);
void XwaRemasterFlight_SetMaterialAnimOverride(const XwaFlightMaterialAnimOverride* in);
/* The player's material states this frame, bit per AERON_ANIM_STATE_*. */
uint32_t XwaRemasterFlight_GetPlayerMaterialStates(void);

void XwaRemasterFlight_GetShadows(XwaFlightShadowParams* out);
void XwaRemasterFlight_SetShadows(const XwaFlightShadowParams* in);
void XwaRemasterFlight_GetShadowsDefault(XwaFlightShadowParams* out);
void XwaRemasterFlight_GetShadowAtlasDebug(int* enabled, int* cascade);
void XwaRemasterFlight_SetShadowAtlasDebug(int enabled, int cascade);
void XwaRemasterFlight_GetShadowStats(AeronSceneDirectionalShadowStats* out);

/* Debug switches. The Probes switch turns model probes on or off (on by
 * default). When it is off, no probe set is placed. The ShowProbes switch
 * draws the probe outlines (off by default). */
void XwaRemasterFlight_SetProbes(int enabled);
int XwaRemasterFlight_Probes(void);
void XwaRemasterFlight_SetShowProbes(int show);
int XwaRemasterFlight_ShowProbes(void);

/* A probe set placed by the last flight frame, for the debug rows. It is a
 * copy, so it stays valid after the model's mesh is released. */
typedef struct XwaFlightProbeSetInfo {
	char model[64];
	uint16_t slot; /* the owner object's slot */
	uint32_t probe_count;
	uint32_t grid_resolution[3];
	uint64_t bytes;
} XwaFlightProbeSetInfo;

uint32_t XwaRemasterFlight_ProbeSets(const XwaFlightProbeSetInfo** out);

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
} XwaFlightHangarLightingParams;

void XwaRemasterFlight_GetHangarLighting(XwaFlightHangarLightingParams* out);
void XwaRemasterFlight_SetHangarLighting(const XwaFlightHangarLightingParams* in);
void XwaRemasterFlight_GetHangarLightingDefault(XwaFlightHangarLightingParams* out);

/* Camera exposure (remaster/config.yaml `exposure:` mapping + the debug
 * inspector). Fixed, the image is exposed at ev100 - compensation. Metered,
 * the flight scene meters it from its image (AeronScene_SetAutoExposure). */
typedef struct XwaFlightExposureParams {
	int   metered;
	float ev100;        /* fixed mode, and the anchor of partial metering */
	float compensation; /* EV added to brightness */
	float metering;     /* 0 = ev100, 1 = the metered EV */
	float min_ev100;
	float max_ev100;
	float brighten_speed; /* per second, when the scene gets darker */
	float darken_speed;   /* per second, when the scene gets brighter */
	float low_percent;
	float high_percent;
	float sky_weight;
	float near_weight;   /* geometry nearer than near_distance: the cockpit */
	float near_distance; /* metres */
	float edge_weight;
	int   max_channel; /* meter the largest of r, g and b instead of luminance */
	int   freeze;      /* keep the current EV; debug only, not in the file */
} XwaFlightExposureParams;

void XwaRemasterFlight_GetExposure(XwaFlightExposureParams* out);
void XwaRemasterFlight_SetExposure(const XwaFlightExposureParams* in);
void XwaRemasterFlight_GetExposureDefault(XwaFlightExposureParams* out);
/* The exposure the tonemap applies to scene values
 * (AeronScenePresent_ExposureFromEv100): the fixed one, or the newest the
 * meter read back. 1 before the configuration loads. */
float XwaRemasterFlight_Exposure(void);
/* The debug view drawn in place of the flight image
 * (AeronScenePresentDebugView): the image, false colour, or the meter's
 * weights. Debug only, not in the settings. */
int XwaRemasterFlight_PresentDebugView(void);
void XwaRemasterFlight_SetPresentDebugView(int view);
/* The flight scene's newest metering; 0 when there is none. */
int XwaRemasterFlight_GetExposureReadout(AeronSceneExposureReadout* out);
/* The fixed exposure of a view lit by its own lights (the CMD display, the
 * model preview): its brightest directional light metered as incident
 * light, EV100 = log2(E * 100 / 250) with E in lux, minus the compensation.
 * Without a light, ev100 - compensation. */
struct XwaRemasterDirLight;
float XwaRemasterFlight_LightMeterExposure(const struct XwaRemasterDirLight* lights, uint32_t count);

/* The default values of a cockpit light, from
 * point_lights.cockpit_lights. The light is a Blender point light of
 * `power_w` watts, at `position_m` from the seat's eye along the craft's
 * axes (X right, Y up, Z forward). `color` is linear. When `has_color` is 0,
 * the light uses the game's colour. Each key that the mod's
 * cockpit_lights section sets replaces the value here
 * (XwaCockpitLight.mod_*). */
typedef struct XwaFlightCockpitLight {
	float position_m[3];
	int has_color;
	float color[3];
	float power_w; /* 0 turns the light off */
} XwaFlightCockpitLight;

/* The default light of a particle effect group, from
 * point_lights.particle_lights. The mod's values replace these
 * (XwaParticleLight). Without `has_color`, the light has the game's colour. */
typedef struct XwaFlightParticleLight {
	int has_color;
	float color[3];
	float power_w; /* 0 turns the light off */
	int fade;
} XwaFlightParticleLight;

/* The default headlight, from point_lights.headlights. It is a Blender
 * area light of `power_w` watts, at `position_m` from the seat's eye along
 * the craft's axes (X right, Y up, Z forward), pointing along the nose.
 * `color` is linear. `angle_degrees` is where the cone ends, from the nose;
 * 0 for a one-sided light. Each key that the mod's headlights section sets
 * replaces the value here (XwaHeadlight.mod_*). */
typedef struct XwaFlightHeadlight {
	float position_m[3];
	float color[3];
	float power_w; /* 0 turns the light off */
	float angle_degrees;
} XwaFlightHeadlight;

/* Point-light knobs (remaster/config.yaml `point_lights:` mapping + the debug
 * inspector). The classic SOURCE laws fix each light's color and classic
 * intensity; the scene evaluates them as physical sphere lights as bright
 * as the classic law at match_distance_m (XwaRemasterShip_ScenePointLight). */
typedef struct XwaFlightPointLightParams {
	int enabled;
	int cluster_depth_slices;
	int cluster_debug;
	float match_distance_m;      /* metres at which a light is as bright as the classic law */
	float range_scale;           /* multiplies the derived falloff range */
	/* While the cockpit is drawn, the light of an explosion up to
	 * cockpit_explosion_distance_m from the eye is multiplied by
	 * cockpit_explosion_light_scale. From there to twice that distance,
	 * the scale goes back up to 1. These are the explosions of hits on the
	 * player's craft, whose lights would wash out the cockpit. */
	float cockpit_explosion_light_scale;
	float cockpit_explosion_distance_m;
	int model_lights;            /* lights authored in models, and the headlight */
	/* Multiplies the lights authored in models, to find a calibration by
	 * eye; debug only, not in the settings. 1 by default. */
	float model_light_scale;
	/* The cockpit lights, by slot. They are only used when
	 * model_lights is on. When it is off, the game's own lights are used. */
	XwaFlightCockpitLight cockpit_lights[XWA_SNAP_COCKPIT_LIGHT_COUNT];
	/* By XwaParticleLightGroup. Only used when model_lights is on. */
	XwaFlightParticleLight particle_lights[XWA_PARTICLE_LIGHT_GROUP_COUNT];
	/* Only used when model_lights is on. */
	XwaFlightHeadlight headlight;
} XwaFlightPointLightParams;

/* The game's light candidates, the models' lights, and those of both whose
 * reach misses the view (culled) or that rank below the scene's capacity
 * (dropped); the rest are submitted. */
typedef struct XwaFlightPointLightStats {
	uint32_t generated_count;
	uint32_t valid_count;
	uint32_t invalid_count;
	uint32_t candidate_overflow_count;
	uint32_t model_count;
	uint32_t culled_count, model_culled_count;
	uint32_t dropped_count, model_dropped_count;
	AeronSceneClusteredLightStats scene;
} XwaFlightPointLightStats;

/* The light of particle effect `effect` at `pos`, in scene units. Returns
 * -1 when model lights are off and the game's light must be used, 0 when
 * the light is off, and 1 when `out` holds it. */
int XwaRemasterFlight_ParticleLight(const XwaSnapshot* snapshot, const XwaParticleEffect* effect,
									const float pos[3], float exposure, AeronSceneLight* out);

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

/* Settings for far laser bolts (`projectiles:` in remaster/config.yaml).
 * The debug UI can change them while the game runs. */
typedef struct XwaFlightProjectileParams {
	/* Bolts thinner than this on screen, in render pixels, are drawn this
	 * wide, with their light spread over the wider strip
	 * (AeronScene_SetEmissionMinWidth). 0 is off. */
	float min_width_px;
	/* Bolts thinner than this on screen, in pixels of a 1080-line view,
	 * are dimmed by (width / fade_width_px)^fade_power
	 * (AeronScene_SetEmissionFade). 0 is off. */
	float fade_width_px;
	float fade_power;
	/* How bright projectiles are drawn, and how much light the bloom gets
	 * from them, as multiples of their converted emission
	 * (AeronScene_SetEmissionLight). */
	float emission_scale;
	float bloom_scale;
} XwaFlightProjectileParams;

void XwaRemasterFlight_GetProjectiles(XwaFlightProjectileParams* out);
void XwaRemasterFlight_SetProjectiles(const XwaFlightProjectileParams* in);
void XwaRemasterFlight_GetProjectilesDefault(XwaFlightProjectileParams* out);

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

/* Star streaks of the hyperspace entry and exit (docs/hd_hyperspace_jump.md),
 * loaded from the `hyperspace_jump` section of remaster/config.yaml and
 * live-editable through the debug inspector. */
typedef struct XwaFlightHyperspaceJumpParams {
	int star_count;
	float streak_brightness;
	float whiteout_brightness;
	float brightness_range;
	float streak_width_px;
	float width_from_brightness;
	float width_variation;
	float head_widening;
	float radial_widening;
	float tail_taper;
	float head_taper;
	float taper_variation;
	float glow_strength;
	float glow_width;
	float tail_brightness;
	float tail_color[3];
	float stretch_scale;
	float white_fade_seconds;
	float tunnel_entry_brightness;
	float tunnel_settle_seconds;
	float mesh_light_strength;
	float mesh_light_start;
	float mesh_light_curve;
	float tint[3];
	/* The scene warp (docs/hd_hyperspace_jump.md, Scene warp). */
	float warp_start;
	float warp_end;
	float warp_curve;
	float warp_distance_m;
	float warp_sky_zoom;
	float warp_smear;
	float warp_sky_highlights;
	float warp_reveal_softness;
	int warp_taps;
	int warp_sky_taps;
	/* 1: the sky passes run at half the output resolution. */
	int warp_sky_half_resolution;
	/* 0: the exit plays the entry backward. 1: the exit carries on forward,
	   so the streaks shrink toward the camera and the scene comes in toward
	   it. */
	int exit_forward;
} XwaFlightHyperspaceJumpParams;

void XwaRemasterFlight_GetHyperspaceJump(XwaFlightHyperspaceJumpParams* out);
void XwaRemasterFlight_SetHyperspaceJump(const XwaFlightHyperspaceJumpParams* in);
void XwaRemasterFlight_GetHyperspaceJumpDefault(XwaFlightHyperspaceJumpParams* out);

/* The camera in the player's hyperspace jump (docs/hd_hyperspace_jump.md,
 * Camera). The normal jump uses the `hyperspace_camera` section of
 * remaster/config.yaml. A look has its own `camera` group. The look's shake
 * gradually replaces the normal one between its shake_start and shake_full,
 * and its jolt replaces the normal one. Fractions of the tunnel go from 0,
 * when the tunnel appears, to 1, when it ends. */
typedef struct XwaFlightHyperspaceCameraParams {
	/* The camera shake in the tunnel. The cockpit moves with the camera.
	   These are how far the camera turns at full strength, in degrees, how
	   fast it shakes (1 is the normal speed), when the shake starts and when
	   it reaches full strength. */
	float shake_degrees;
	float shake_speed;
	float shake_start;
	float shake_full;
	/* The number of kicks per second. */
	float kick_rate;
	/* The jolt when the craft comes out of hyperspace. These are how far the
	   camera turns at the jolt's peak, in degrees, how long after the start
	   of the exit the jolt hits, and how fast it settles, in seconds. */
	float jolt_degrees;
	float jolt_delay;
	float jolt_time;
} XwaFlightHyperspaceCameraParams;

void XwaRemasterFlight_GetHyperspaceCamera(XwaFlightHyperspaceCameraParams* out);
void XwaRemasterFlight_SetHyperspaceCamera(const XwaFlightHyperspaceCameraParams* in);
void XwaRemasterFlight_GetHyperspaceCameraDefault(XwaFlightHyperspaceCameraParams* out);

/* Other looks of the player's jump, loaded from the `hyperspace_looks`
 * section of remaster/config.yaml and live-editable through the debug
 * inspector. A mission picks a look for jumps to a given region. In the
 * tunnel, the look's colours spread from the far end of the tunnel toward the
 * camera, behind an uneven glowing edge. The exit uses the look's colours,
 * and the start of the jump looks normal. In the tunnel, the look's camera
 * shake gradually replaces the normal one. At the exit, its jolt replaces the
 * normal one. */
#define XWA_FLIGHT_HYPERSPACE_LOOKS_MAX 8
#define XWA_FLIGHT_HYPERSPACE_LOOK_NAME_MAX 32

typedef struct XwaFlightHyperspaceLookParams {
	/* The tunnel's colours where the look has spread, as in
	   XwaFlightHyperspaceTunnelParams. */
	float dark_color[3];
	float body_color[3];
	float highlight_color[3];
	float cap_color[3];
	/* The edge of the area that the look's colours have reached: its colour,
	   its thickness, how far its bumps reach ahead, how many bumps there are,
	   and how much they twist. */
	float edge_color[3];
	float edge_width;
	float front_noise;
	float front_noise_scale;
	float front_warp;
	/* The colours of the streaks when the player comes out of hyperspace, as
	   in XwaFlightHyperspaceJumpParams. */
	float tint[3];
	float tail_color[3];
	/* When the look's colours start to spread and when they fill the tunnel,
	   as fractions of the time in the tunnel: 0 when the tunnel appears, 1
	   when it ends. */
	float blend_start;
	float blend_end;
	XwaFlightHyperspaceCameraParams camera;
} XwaFlightHyperspaceLookParams;

int XwaRemasterFlight_HyperspaceLookCount(void);
/* The name of look `index`, or NULL. */
const char* XwaRemasterFlight_HyperspaceLookName(int index);
/* The index of the look named `name`, or -1. */
int XwaRemasterFlight_FindHyperspaceLook(const char* name);
void XwaRemasterFlight_GetHyperspaceLook(int index, XwaFlightHyperspaceLookParams* out);
void XwaRemasterFlight_SetHyperspaceLook(int index, const XwaFlightHyperspaceLookParams* in);
void XwaRemasterFlight_GetHyperspaceLookDefault(int index, XwaFlightHyperspaceLookParams* out);

/* The debug preview of hyperspace. It replaces what the flight scene shows,
 * never the simulation state. Map mode and real hyperspace come first. */
typedef enum XwaFlightHyperspacePreview {
	XWA_FLIGHT_HYPERSPACE_PREVIEW_OFF = 0,
	XWA_FLIGHT_HYPERSPACE_PREVIEW_TUNNEL = 1,     /* the tunnel alone */
	XWA_FLIGHT_HYPERSPACE_PREVIEW_JUMP_CYCLE = 2, /* entry, tunnel, exit, normal flight, on a loop */
} XwaFlightHyperspacePreview;

void XwaRemasterFlight_SetHyperspacePreview(XwaFlightHyperspacePreview mode);
XwaFlightHyperspacePreview XwaRemasterFlight_GetHyperspacePreview(void);
/* Playback speed of the preview, 0.05 to 1. */
void XwaRemasterFlight_SetHyperspacePreviewSpeed(float speed);
float XwaRemasterFlight_GetHyperspacePreviewSpeed(void);
/* The look that the preview draws, or -1 for the normal look. */
void XwaRemasterFlight_SetHyperspacePreviewLook(int index);
int XwaRemasterFlight_GetHyperspacePreviewLook(void);
/* How far through the tunnel the tunnel preview is, from 0 to 1. That
   preview never ends, so this sets how far the look has spread and how strong
   the camera shake is. */
void XwaRemasterFlight_SetHyperspacePreviewLookBlend(float blend);
float XwaRemasterFlight_GetHyperspacePreviewLookBlend(void);

/* What the scene warp shows during the entry and the exit. The debug views
 * replace the image. */
typedef enum XwaFlightHyperspaceWarpView {
	XWA_FLIGHT_HYPERSPACE_WARP_VIEW_IMAGE = 0,
	XWA_FLIGHT_HYPERSPACE_WARP_VIEW_KEEP_MASK = 1, /* the keep mask in red over the image */
	XWA_FLIGHT_HYPERSPACE_WARP_VIEW_SKY = 2,       /* the warped sky alone */
	XWA_FLIGHT_HYPERSPACE_WARP_VIEW_REVEAL = 3,    /* the black reveal as grey levels */
	XWA_FLIGHT_HYPERSPACE_WARP_VIEW_COUNT = 4,
} XwaFlightHyperspaceWarpView;

void XwaRemasterFlight_SetHyperspaceWarpView(XwaFlightHyperspaceWarpView view);
XwaFlightHyperspaceWarpView XwaRemasterFlight_GetHyperspaceWarpView(void);

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

/* presentation.cockpit_fov_min_aspect, for the debug UI. */
float XwaRemasterFlight_GetCockpitFovMinAspect(void);
void XwaRemasterFlight_SetCockpitFovMinAspect(float aspect);

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
	/* Viewport pixels per classic screen pixel. */
	float classic_pixel_scale;
	/* Viewport pixels per classic pixel of a projected size, such as a
	 * particle's. It is smaller than classic_pixel_scale when the view is
	 * wider than the classic projection (presentation.cockpit_fov_min_aspect). */
	float projection_pixel_scale;
} XwaRemasterFlightView;

int XwaRemasterFlight_BuildView(const XwaFlightCamera* camera, int target_w, int target_h,
								XwaRemasterFlightView* out);
int XwaRemasterFlight_ProjectLocal(const XwaRemasterFlightView* view, const float local[3], float* out_x,
								   float* out_y, float* out_depth);
int XwaRemasterFlight_ProjectWorldI32(const XwaRemasterFlightView* view, const int32_t world[3], float* out_x,
									  float* out_y, float* out_depth);
int XwaRemasterFlight_ProjectView(const XwaRemasterFlightView* view, const float eye[3], float* out_x,
								  float* out_y);
/* Clips an eye-space segment to the near plane (moving the endpoint behind
 * it) and projects both ends; returns 0 when it lies wholly behind. */
int XwaRemasterFlight_ClipProjectLine(const XwaRemasterFlightView* view, float a[3], float b[3], float* ax,
									  float* ay, float* bx, float* by);

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

typedef struct XwaFlightCaptureSky {
	float exposure;
	float star_brightness;
	float star_density;
	float star_grid;
} XwaFlightCaptureSky;

/* The starfield settings the flight frame draws with outside authored mode. */
void XwaRemasterFlight_CaptureSkyInfo(XwaFlightCaptureSky* out);

/* A square perspective capture camera with half-angle tangent `half_tan`
 * and world-space view axes (unit, right x down = forward), and the
 * world->view rows re-derived from its quaternion, as the flight frame
 * does. Returns 0 for an invalid basis. */
int XwaRemasterFlight_CaptureCamera(const float right[3], const float down[3], const float forward[3],
									float half_tan, int size, AeronSceneCamera* camera, float crows[9]);

/* After AeronScene_Begin: draws the sky into `scene`. With `sky`, it is
 * that environment's sky without its overlay, and `stars` may be NULL.
 * Otherwise it is the starfield drawn with `stars` (the scene's
 * BEFORE_OPAQUE hook), with the classic-pixel dot geometry scaled by
 * `star_upscale`. Returns 0 on failure. */
int XwaRemasterFlight_CaptureSetSky(AeronScene3D* scene, struct XwaRemasterSkyStars* stars,
									const XwaSnapshot* snap, const AeronPreparedEnvironment* sky,
									float star_upscale);

/* Whether backdrop record `index` is hidden: by the game, or by `hide`, the
 * region's hide and show rules (NULL for none). */
int XwaRemasterFlight_CaptureBackdropHidden(const XwaSnapshot* snap, XwaRemasterAssets* assets,
											uint32_t index, const XwaEnvironmentBackdropRule* hide);

/* Derives the region's backdrop quads for the capture camera rows `crows`
 * and, when `scene` is not NULL, submits them as SKY billboards. Backdrops
 * that `hide` hides (the region's rules; NULL for none) are left out. Axis
 * quads are kept by their own direction instead of the camera hemisphere.
 * `out_max_tan` receives the largest |x| / z or |y| / z over the quad
 * corners, or a negative value when a corner is not in front of the
 * camera. Returns the quad count. */
uint32_t XwaRemasterFlight_CaptureBackdrops(AeronScene3D* scene, const float crows[9],
											const XwaSnapshot* snap, XwaRemasterAssets* assets, int filter,
											const XwaEnvironmentBackdropRule* hide, float* out_max_tan);

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

/* Returns the RGBA16F target of the composed presentation, or NULL before
 * the first render. After XwaRemasterFlight_Render with direct_present 0,
 * it holds the final image, with the HUD. */
AeronRenderTarget* XwaRemasterFlight_ComposedTarget(void);

/* Call after XwaRemasterFlight_Render. Records the rendered frame as the
 * tonemap receives it into the target of the composed presentation: the
 * scene with its bloom, multiplied by the exposure, without the HUD. The
 * target then holds this image instead of the final image, until the next
 * render. Returns 0 if there is no rendered frame. */
int XwaRemasterFlight_RecordBeforeTonemap(AeronCommandBuffer* cmd);

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
