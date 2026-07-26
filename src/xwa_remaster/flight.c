/* Flight-scene HD driver — see xwa_remaster/flight.h. */

#include "xwa_remaster/flight.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/scene/billboard.h"
#include "aeron/scene/bloom.h"
#include "aeron/scene/image_cache.h"
#include "aeron/scene/present.h"
#include "aeron/scene/scene3d.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/effects.h"
#include "xwa_remaster/glow_marks.h"
#include "xwa_remaster/hud.h"
#include "xwa_remaster/hyperspace.h"
#include "xwa_remaster/ship.h"
#include "xwa_remaster/sky_stars.h"
#include "xwa_remaster/world.h"
#include "xwa_remaster/xwa_remaster.h"
#include "xwa_runtime/runtime/presentation.h"
#include "xwa_runtime/timing/host_clock.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FL_MB_REFERENCE_FRAME_US 32000u

/* Reversed-Z near plane in engine view units. Cockpit geometry sits
 * within a few units of the eye, so this must stay small; D32F
 * reversed-Z keeps distant precision regardless. */
#define FL_NEAR_Z 1.0f

enum {
	FL_FSR_RESET_INITIAL = 1u << 0,
	FL_FSR_RESET_MODE_CHANGE = 1u << 1,
	FL_FSR_RESET_RESOURCES_RECREATED = 1u << 2,
	FL_FSR_RESET_TEMPORAL_DISABLED = 1u << 3,
	FL_FSR_RESET_NO_PREVIOUS_SNAPSHOT = 1u << 4,
	FL_FSR_RESET_PREVIOUS_CAMERA_INVALID = 1u << 5,
	FL_FSR_RESET_REGION_CHANGE = 1u << 6,
	FL_FSR_RESET_PREVIOUS_HYPERSPACE = 1u << 7,
	FL_FSR_RESET_PREVIOUS_MAP = 1u << 8,
	FL_FSR_RESET_HANGAR_CHANGE = 1u << 9,
	FL_FSR_RESET_EXTERNAL_VIEW_CHANGE = 1u << 10,
	FL_FSR_RESET_FILM_OVERLAY_CHANGE = 1u << 11,
	FL_FSR_RESET_GAME_TIME_ROLLBACK = 1u << 12,
	FL_FSR_RESET_MOTION_CONTEXT_UNKNOWN = 1u << 13,
	FL_FSR_RESET_COCKPIT_VISIBILITY = 1u << 14,
	FL_FSR_RESET_COCKPIT_CURRENT_ANCHOR = 1u << 15,
	FL_FSR_RESET_COCKPIT_PREVIOUS_ANCHOR = 1u << 16,
	FL_FSR_RESET_COCKPIT_SEAT = 1u << 17,
	FL_FSR_RESET_COCKPIT_MODEL = 1u << 18,
	FL_FSR_RESET_COCKPIT_ANCHOR_ID = 1u << 19,
	FL_FSR_RESET_RENDER_RESUMED = 1u << 20,
	FL_FSR_RESET_UNSPECIFIED = 1u << 21,
};

#define FL_MAX_BACKDROP_QUADS (XWA_SNAP_MAX_BACKDROPS * (XWA_SNAP_MAX_STRIP_COORDS - 1))
typedef struct FlBackdropQuad {
	float corners[4][3];
	float uvs[4][2];
	XwaAssetRef ref;
} FlBackdropQuad;

/* One state-derived scene billboard (explosion/spark/debris/chaff
 * sprite, wreck flame), resolved before AeronScene_Begin (atlas
 * uploads precede the passes) and submitted back-to-front. */
typedef struct FlBillboard {
	float center[3];      /* view space; [2] is the adjusted depth */
	float half_w, half_h; /* view units at that depth */
	float rot;            /* roll about the view axis, radians */
	float depth_bias;     /* view units toward the camera (test only) */
	XwaAssetRef ref;
	uint32_t argb;
	/* Motion blur: complete PREV-frame geometry in the current frame's
	 * render-local coordinate system. This preserves animation-frame
	 * dimensions, scale/depth laws, roll and the selected camera-facing
	 * basis rather than approximating history with a translation. */
	float prev_corners[4][3];
	uint8_t has_prev;
} FlBillboard;
#define FL_MAX_BILLBOARDS 1024

/* One lens-flare source (classic LensFlare_QueueSource): the anchored
 * screen position the 7-quad train derives from at submit. */
typedef struct FlLensSource {
	float anchor_local[3]; /* render-local depth-occlusion source */
	float sx, sy;          /* projected source, top-origin frame px */
	uint32_t argb;         /* vertex tint: opaque white (suns, classic -1)
							* or the faint flash override */
} FlLensSource;
#define FL_MAX_LENS_SOURCES 4 /* classic queue capacity */
#define FL_LENS_RINGS 7

/* Point-light candidate pool (the classic scene table caps at 128;
 * engine-glow and pulse lights ride on top). */
#define FL_MAX_PL_CANDIDATES 192

typedef enum FlBeforeOpaqueMode {
	FL_BEFORE_OPAQUE_NONE = 0,
	FL_BEFORE_OPAQUE_HYPERSPACE,
	FL_BEFORE_OPAQUE_SKY_STARS,
} FlBeforeOpaqueMode;

/* The scene owns the pass topology (prepass / SSAO / forward color /
 * motion-blur resolve — AeronScene_Render); this driver submits mesh
 * instances and draws the non-mesh geometry through the scene's pass
 * hooks. `snap`/`assets` and the derived per-frame state below are the
 * hook context, valid during AeronScene_Render only. */
static struct {
	AeronScene3D* scene;
	XwaRemasterTrails* trails;
	XwaRemasterParticles* particles;
	XwaRemasterHyperspace* hyperspace;
	XwaRemasterSkyStars* sky_stars;
	FlBeforeOpaqueMode before_opaque_mode;
	int rt_w;
	int rt_h;
	AeronSampleCount scene_requested_samples;
	AeronSampleCount configured_msaa_samples;
	int configured_hdr_output;

	/* Tonemap + bloom present stage (the TIE model): the scene renders
	 * linear HDR and AeronSceneBloom accumulates the bright-pass chain.
	 * Stable opaque flight draws scene+bloom directly into the swapchain;
	 * layered transitions use the RGBA16F present_rt fallback. */
	AeronSceneBloom* bloom;
	AeronScenePresentChain* present;
	AeronScenePresentChain* present_direct;
	AeronTextureFormat direct_present_format;
	AeronRenderTarget* present_rt;
	AeronSampler* present_sampler;
	AeronTexture* present_scene_tex;
	AeronTexture* present_bloom_tex;
	int direct_ready;
	/* Optional flight-scene override for the PBR atlas sampler. NULL
	 * selects AeronScene3D's trilinear, non-anisotropic default. */
	AeronSampler* mesh_sampler;
	int mesh_sampler_configured;

	/* SSAO knobs loaded from the mandatory render.yaml resource, then
	 * live-mutated by the debug
	 * inspector via the XwaRemasterFlight_GetSsao/SetSsao accessors. */
	XwaFlightSsaoParams ssao;
	XwaFlightSsaoParams ssao_default;
	int config_loaded;
	XwaFlightShadowParams shadows;
	XwaFlightShadowParams shadows_default;
	XwaFlightHangarLightingParams hangar_lighting;
	XwaFlightHangarLightingParams hangar_lighting_default;
	/* Point-light calibration knobs (same lifecycle as ssao). */
	XwaFlightPointLightParams plight;
	XwaFlightPointLightParams plight_default;
	XwaFlightTextureFilteringParams texture_filtering;
	XwaFlightTextureFilteringParams texture_filtering_default;

	/* Motion-blur knobs (the TIE set: quality tier drives the scene's
	 * velocity prepass + reconstruct; shutter = fraction of the frame's
	 * motion blurred; camera_blur folds camera rotation/translation in;
	 * pause_keep_blur keeps blurring frozen frames). */
	int mb_quality; /* 0 off, 1 low, 2 high */
	float mb_shutter;
	int mb_camera_blur;
	int mb_pause_keep_blur;
	int mb_velocity_viz;
	int mb_fsr_direct_motion;

	/* FSR 3.1.4 is owned by AeronScene. The game only supplies
	 * configuration, frame timing, and history-reset boundaries. */
	AeronTemporalMode fsr_mode;
	float fsr_sharpness;
	int fsr_debug_view;
	uint64_t fsr_prev_host_us;
	int fsr_prev_host_valid;
	int fsr_reset_pending;
	uint32_t fsr_reset_reasons;
	uint32_t fsr_last_reset_reasons;
	uint32_t fsr_consecutive_reset_frames;

	/* Per-frame motion-blur context: valid prev pair, the prev camera's
	 * view-proj + position, the prev snapshot (borrowed for the render
	 * call), and the curr-index -> prev-index object map (-1 = no match:
	 * spawn frame, zero velocity). */
	int mb_enabled;
	float mb_prev_vp[16];
	const XwaSnapshot* mb_prev_snap;
	int32_t mb_prev_index[XWA_SNAP_MAX_FLIGHT_OBJECTS];
	/* Host-time span represented by the retained velocity buffer. */
	uint64_t mb_pose_host_us;
	uint64_t mb_velocity_span_us;
	int mb_pose_host_valid;

	/* Star skybox — an authored cube map replaces the classic CPU
	 * pixel starfield (procedural + random per launch, so the cube
	 * has no per-mission fidelity constraint; deviation register in
	 * docs/xwa_snapshot.md). Loaded once per process, latched on
	 * failure. sky_path is relative to the bake root. */
	AeronTexture* sky_cube;
	int process_assets_prepared;
	int sky_enabled;
	float sky_exposure;
	char sky_path[256];
	/* Sky source: 0 = authored cube map (sky_path), 1 = XWA-owned GPU
	 * reproduction of FlightStarfield_Render. Selected by skybox.mode. */
	int sky_mode;
	float star_brightness;
	float star_density;
	float star_grid;
	/* Star dot geometry in *classic viewport pixels* — multiplied by the
	 * viewport→render upscale factor at draw time so a star matches the
	 * classic single-pixel footprint at any render resolution. */
	float star_core_px;
	float star_feather;
	float star_flare;

	const XwaSnapshot* snap;
	/* Camera world->eye rows re-derived from the quaternion the scene
	 * consumes (not the raw snapshot rows): view-space geometry rotated
	 * by these matches the scene's view matrix exactly; the Q15->quat
	 * round-trip noise stays common to both paths. */
	float crows[9];
	int32_t origin_world[3];
	float camera_local[3];

	/* Pre-resolved frame data (atlas uploads must precede the scene's
	 * render passes). */
	FlBackdropQuad bd_quads[FL_MAX_BACKDROP_QUADS];
	uint32_t bd_count;
	FlBillboard bb[FL_MAX_BILLBOARDS];
	uint32_t bb_count;
	XwaAssetRef glow_ref;
	int glow_ok;
	/* Lens flares: sources + the per-ring flare sprite frames of
	 * LightingEffects group 1000 (classic ring->frame map). */
	FlLensSource lens_src[FL_MAX_LENS_SOURCES];
	uint32_t lens_src_count;
	XwaAssetRef lens_ref[FL_LENS_RINGS];
	int lens_dim[FL_LENS_RINGS][2]; /* classic level-0 dims */
	int lens_ok;

	/* Point-light candidates (classic source laws, classic-unit color
	 * x intensity premultiplied; knobs applied at finalize) + the
	 * final top-scored set handed to the PBR env. */
	XwaShipPointLight pl_cand[FL_MAX_PL_CANDIDATES];
	uint32_t pl_cand_count;
	XwaShipPointLight pl_final[XWA_SHIP_MAX_POINT_LIGHTS];

	/* Projectile instances, collected during the walk and submitted
	 * AFTER every ship (instances draw in submission order): their
	 * alpha-BLEND ranges neither write depth nor enter the prepass, so
	 * they only composite correctly against the complete opaque depth.
	 * The cockpit submits after them (canopy glass still tints them).
	 * Roll-aligned per the classic law at transform derive. */
	AeronSceneMeshInstance bolts[XWA_SNAP_MAX_FLIGHT_OBJECTS];
	uint32_t bolt_count;

	/* Mesh-table pool — AeronScene_AddMeshInstance borrows the table
	 * pointer for the frame, so tables must outlive the submit walk. Each
	 * object can require current and previous articulation, plus two cockpit
	 * tables. */
	AeronSceneMeshTable tables[XWA_SNAP_MAX_FLIGHT_OBJECTS * 2 + 2];
	uint32_t table_count;
} s;

static void fl_draw_before_opaque(AeronCommandBuffer* command_buffer, AeronRenderPass* render_pass, int rt_w,
								  int rt_h, void* user) {
	(void)user;
	switch (s.before_opaque_mode) {
		case FL_BEFORE_OPAQUE_HYPERSPACE:
			XwaRemasterHyperspace_Draw(command_buffer, render_pass, rt_w, rt_h, s.hyperspace);
			break;
		case FL_BEFORE_OPAQUE_SKY_STARS:
			XwaRemasterSkyStars_Draw(command_buffer, render_pass, rt_w, rt_h, s.sky_stars);
			break;
		case FL_BEFORE_OPAQUE_NONE:
		default:
			break;
	}
}

static void fl_request_fsr_reset(uint32_t reasons) {
	s.fsr_reset_pending = 1;
	s.fsr_reset_reasons |= reasons ? reasons : FL_FSR_RESET_UNSPECIFIED;
}

static void fl_append_fsr_reset_reason(char* text, size_t capacity, const char* reason) {
	const size_t used = strlen(text);
	if (used >= capacity) {
		return;
	}
	snprintf(text + used, capacity - used, "%s%s", used ? ", " : "", reason);
}

static void fl_format_fsr_reset_reasons(uint32_t reasons, char* text, size_t capacity) {
	if (!text || capacity == 0) {
		return;
	}
	text[0] = '\0';
#define FL_APPEND_FSR_REASON(bit, name)                                                                      \
	do {                                                                                                     \
		if (reasons & (bit))                                                                                 \
			fl_append_fsr_reset_reason(text, capacity, (name));                                              \
	} while (0)
	FL_APPEND_FSR_REASON(FL_FSR_RESET_INITIAL, "initialization");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_MODE_CHANGE, "mode change");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_RESOURCES_RECREATED, "sized resources recreated");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_TEMPORAL_DISABLED, "temporal scene disabled");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_NO_PREVIOUS_SNAPSHOT, "no previous snapshot");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_PREVIOUS_CAMERA_INVALID, "previous camera invalid");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_REGION_CHANGE, "region changed");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_PREVIOUS_HYPERSPACE, "previous frame was hyperspace");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_PREVIOUS_MAP, "previous frame was map view");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_HANGAR_CHANGE, "hangar state changed");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_EXTERNAL_VIEW_CHANGE, "external-view state changed");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_FILM_OVERLAY_CHANGE, "film-overlay state changed");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_GAME_TIME_ROLLBACK, "game time moved backwards");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_MOTION_CONTEXT_UNKNOWN, "motion context unavailable");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_COCKPIT_VISIBILITY, "cockpit visibility changed");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_COCKPIT_CURRENT_ANCHOR, "current cockpit anchor missing");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_COCKPIT_PREVIOUS_ANCHOR, "previous cockpit anchor missing");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_COCKPIT_SEAT, "cockpit seat changed");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_COCKPIT_MODEL, "cockpit model changed");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_COCKPIT_ANCHOR_ID, "cockpit anchor identity changed");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_RENDER_RESUMED, "flight rendering resumed");
	FL_APPEND_FSR_REASON(FL_FSR_RESET_UNSPECIFIED, "unspecified caller request");
#undef FL_APPEND_FSR_REASON
}

static void fl_report_fsr_reset(int reset_requested) {
	if (!reset_requested) {
		if (s.fsr_consecutive_reset_frames > 1) {
			Aeron_Log("xwa.remaster", "FSR history reset condition cleared after %u consecutive frames",
					  s.fsr_consecutive_reset_frames);
		}
		s.fsr_last_reset_reasons = 0;
		s.fsr_consecutive_reset_frames = 0;
		return;
	}

	const uint32_t reasons = s.fsr_reset_reasons ? s.fsr_reset_reasons : FL_FSR_RESET_UNSPECIFIED;
	if (reasons == s.fsr_last_reset_reasons) {
		s.fsr_consecutive_reset_frames++;
		if (s.fsr_consecutive_reset_frames != 2 && s.fsr_consecutive_reset_frames % 120 != 0) {
			return;
		}
	} else {
		s.fsr_last_reset_reasons = reasons;
		s.fsr_consecutive_reset_frames = 1;
	}

	char text[512];
	fl_format_fsr_reset_reasons(reasons, text, sizeof text);

	Aeron_Log("xwa.remaster", "FSR history reset frame %u: %s", s.fsr_consecutive_reset_frames,
			  text[0] ? text : "unknown reason");
}

static int fl_cfg_has_type(const AeronConfigFile* config, const char* path, AeronConfigNodeType type) {
	const AeronConfigNodeType actual = AeronConfigNode_Type(AeronConfigFile_GetNode(config, path));
	return actual == type || (type == AERON_CONFIG_FLOAT && actual == AERON_CONFIG_INT);
}

static int fl_cfg_validate(const AeronConfigFile* config, const char** error_path) {
	static const char* ints[] = {
		"presentation.vsync_divisor", "presentation.msaa_samples", "ssao.quality",
		"motion_blur.quality",        "shadows.atlas_size",         "shadows.cascade_count",
		"shadows.filter_quality",
	};
	static const char* floats[] = {
		"ssao.intensity",
		"ssao.power",
		"ssao.radius_view",
		"ssao.bias_view",
		"ssao.direct",
		"ssao.min_screen_frac",
		"ssao.max_screen_frac",
		"ssao.sample_jitter",
		"shadows.max_distance",
		"shadows.split_lambda",
		"shadows.split_1",
		"shadows.split_2",
		"shadows.split_3",
		"shadows.filter_radius",
		"shadows.light_angular_radius_degrees",
		"shadows.max_filter_radius",
		"shadows.pcss_min_filter_radius",
		"shadows.normal_bias",
		"shadows.depth_bias_texels",
		"shadows.slope_bias",
		"shadows.receiver_plane_bias",
		"shadows.caster_constant_bias",
		"shadows.caster_slope_bias",
		"shadows.transition_fraction",
		"shadows.distance_fade_fraction",
		"hangar_lighting.direction_x",
		"hangar_lighting.direction_y",
		"hangar_lighting.direction_z",
		"hangar_lighting.intensity",
		"hangar_lighting.color_r",
		"hangar_lighting.color_g",
		"hangar_lighting.color_b",
		"hangar_lighting.ambient_ceiling",
		"hangar_lighting.ambient_sides",
		"hangar_lighting.ambient_floor",
		"hangar_lighting.shadow_filter_radius",
		"point_lights.scale",
		"point_lights.range_scale",
		"point_lights.min_distance",
		"point_lights.spec_weight",
		"point_lights.diffuse_wrap",
		"point_lights.contrib_cap",
		"skybox.exposure",
		"skybox.star_brightness",
		"skybox.star_density",
		"skybox.star_grid",
		"skybox.star_core_px",
		"skybox.star_feather",
		"skybox.star_flare",
		"lighting.intensity",
		"lighting.spec_mul",
		"lighting.wrap",
		"lighting.ambient_r",
		"lighting.ambient_g",
		"lighting.ambient_b",
		"bloom.intensity",
		"motion_blur.shutter",
		"temporal_upscaling.sharpness",
		"texture_filtering.max_anisotropy",
	};
	static const char* bools[] = {
		"presentation.hdr_output",
		"ssao.debug_viz",
		"point_lights.enabled",
		"skybox.enabled",
		"motion_blur.camera_blur",
		"motion_blur.pause_keep_blur",
		"motion_blur.velocity_viz",
		"motion_blur.fsr_direct_motion",
		"texture_filtering.anisotropic",
		"shadows.normal_bias_face_normal",
		"shadows.explicit_splits",
		"shadows.contact_hardening",
		"shadows.debug_cascades",
		"hangar_lighting.enabled",
		"lighting.spec_geom_adapt",
	};
	static const char* strings[] = {
		"shadows.mode",
		"shadows.fit_mode",
		"skybox.path",
		"skybox.mode",
		"temporal_upscaling.mode",
	};
	for (size_t i = 0; i < sizeof ints / sizeof ints[0]; i++) {
		if (!fl_cfg_has_type(config, ints[i], AERON_CONFIG_INT)) {
			*error_path = ints[i];
			return 0;
		}
	}
	for (size_t i = 0; i < sizeof floats / sizeof floats[0]; i++) {
		if (!fl_cfg_has_type(config, floats[i], AERON_CONFIG_FLOAT)) {
			*error_path = floats[i];
			return 0;
		}
	}
	for (size_t i = 0; i < sizeof bools / sizeof bools[0]; i++) {
		if (!fl_cfg_has_type(config, bools[i], AERON_CONFIG_BOOL)) {
			*error_path = bools[i];
			return 0;
		}
	}
	for (size_t i = 0; i < sizeof strings / sizeof strings[0]; i++) {
		if (!fl_cfg_has_type(config, strings[i], AERON_CONFIG_STRING)) {
			*error_path = strings[i];
			return 0;
		}
	}
	return 1;
}

static int fl_ssao_config_valid(const XwaFlightSsaoParams* p) {
	return p->quality >= 0 && p->quality <= 2 && isfinite(p->intensity) && p->intensity >= 0.0f &&
		   isfinite(p->power) && p->power > 0.0f && isfinite(p->radius_view) && p->radius_view > 0.0f &&
		   isfinite(p->bias_view) && p->bias_view >= 0.0f && isfinite(p->direct) && p->direct >= 0.0f &&
		   p->direct <= 1.0f && isfinite(p->min_screen_frac) && p->min_screen_frac >= 0.0f &&
		   isfinite(p->max_screen_frac) && p->max_screen_frac >= 0.0f && isfinite(p->sample_jitter) &&
		   p->sample_jitter >= 0.0f && p->sample_jitter <= 1.0f;
}

static int fl_shadows_config_valid(const XwaFlightShadowParams* p) {
	return p->atlas_size >= 1024 && p->atlas_size <= 8192 &&
		   (p->atlas_size & (p->atlas_size - 1)) == 0 && p->cascade_count >= 1 &&
		   p->cascade_count <= AERON_SCENE_SHADOW_MAX_CASCADES && p->filter_quality >= 0 &&
		   p->filter_quality <= 3 && p->max_distance > FL_NEAR_Z && p->split_lambda >= 0.0f &&
		   p->split_lambda <= 1.0f && p->split_positions[0] > 0.0f &&
		   p->split_positions[0] < p->split_positions[1] && p->split_positions[1] < p->split_positions[2] &&
		   p->split_positions[2] < 1.0f && p->filter_radius >= 0.5f && p->filter_radius <= 3.0f &&
		   p->light_angular_radius_degrees >= 0.0f && p->light_angular_radius_degrees <= 5.0f &&
		   p->max_filter_radius >= p->filter_radius && p->max_filter_radius <= 16.0f &&
		   p->pcss_min_filter_radius >= 0.5f && p->pcss_min_filter_radius <= p->filter_radius &&
		   p->normal_bias >= 0.0f && p->depth_bias_texels >= 0.0f && p->slope_bias >= 0.0f &&
		   p->receiver_plane_bias >= 0.0f && p->receiver_plane_bias <= 2.0f &&
		   p->caster_constant_bias >= 0.0f && p->caster_constant_bias <= 16.0f &&
		   p->caster_slope_bias >= 0.0f && p->caster_slope_bias <= 16.0f &&
		   p->transition_fraction >= 0.0f && p->transition_fraction <= 0.5f &&
		   p->distance_fade_fraction >= 0.0f && p->distance_fade_fraction <= 0.5f;
}

static int fl_hangar_lighting_config_valid(const XwaFlightHangarLightingParams* p, float direction_length) {
	return direction_length > 0.0001f && isfinite(direction_length) && p->intensity >= 0.0f &&
		   p->intensity <= 4.0f && p->color[0] >= 0.0f && p->color[0] <= 4.0f && p->color[1] >= 0.0f &&
		   p->color[1] <= 4.0f && p->color[2] >= 0.0f && p->color[2] <= 4.0f &&
		   p->ambient_ceiling >= 0.0f && p->ambient_ceiling <= 4.0f && p->ambient_sides >= 0.0f &&
		   p->ambient_sides <= 4.0f && p->ambient_floor >= 0.0f && p->ambient_floor <= 4.0f &&
		   p->shadow_filter_radius >= 0.5f && p->shadow_filter_radius <= 3.0f;
}

static int fl_point_lights_config_valid(const XwaFlightPointLightParams* p) {
	return isfinite(p->scale) && p->scale >= 0.0f && isfinite(p->range_scale) && p->range_scale > 0.0f &&
		   isfinite(p->min_distance) && p->min_distance > 0.0f && isfinite(p->spec_weight) &&
		   p->spec_weight >= 0.0f && isfinite(p->diffuse_wrap) && p->diffuse_wrap >= 0.0f &&
		   p->diffuse_wrap <= 1.0f && isfinite(p->contrib_cap) && p->contrib_cap >= 0.0f;
}

static int fl_sky_config_valid(void) {
	return isfinite(s.sky_exposure) && s.sky_exposure >= 0.0f && isfinite(s.star_brightness) &&
		   s.star_brightness >= 0.0f && isfinite(s.star_density) && s.star_density >= 0.0f &&
		   s.star_density <= 1.0f && isfinite(s.star_grid) && s.star_grid >= 1.0f && s.star_grid <= 256.0f &&
		   isfinite(s.star_core_px) && s.star_core_px >= 0.0f && isfinite(s.star_feather) &&
		   s.star_feather >= 0.0f && isfinite(s.star_flare) && s.star_flare >= 0.0f;
}

static int fl_pbr_config_valid(const XwaShipPbrTuning* p) {
	return isfinite(p->light_intensity) && p->light_intensity >= 0.0f && isfinite(p->global_spec_mul) &&
		   p->global_spec_mul >= 0.0f && isfinite(p->light_wrap) && p->light_wrap >= 0.0f &&
		   p->light_wrap <= 1.0f && isfinite(p->ambient[0]) && p->ambient[0] >= 0.0f &&
		   isfinite(p->ambient[1]) && p->ambient[1] >= 0.0f && isfinite(p->ambient[2]) &&
		   p->ambient[2] >= 0.0f;
}

/* Load the mandatory renderer configuration once. */
int XwaRemasterFlight_InitConfig(AeronVfs* vfs) {
	if (s.config_loaded) {
		return 1;
	}

	static const char* path = "remaster/flight/render.yaml";
	AeronConfigFile* cf = NULL;
	if (!vfs || !AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_RESOURCE, path, &cf)) {
		Aeron_Log("xwa.remaster", "required render configuration unavailable or invalid: %s", path);
		return 0;
	}
	const char* error_path = NULL;
	if (!fl_cfg_validate(cf, &error_path)) {
		Aeron_Log("xwa.remaster", "%s: missing or invalid required setting '%s'", path, error_path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	/* Every value read below is required and type-checked above. The zero
	 * arguments only satisfy the generic accessor API; they are unreachable
	 * as configuration fallbacks. */
	const int vsync_divisor = (int)AeronConfigFile_GetInt(cf, "presentation.vsync_divisor", 0);
	if (!Aeron_SetPresentationVsyncDivisor(vsync_divisor)) {
		Aeron_Log("xwa.remaster", "%s: presentation.vsync_divisor must be 1 or 2", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	const int msaa_samples = (int)AeronConfigFile_GetInt(cf, "presentation.msaa_samples", 0);
	switch (msaa_samples) {
		case 1:
			s.configured_msaa_samples = AERON_SAMPLE_COUNT_1;
			break;
		case 2:
			s.configured_msaa_samples = AERON_SAMPLE_COUNT_2;
			break;
		case 4:
			s.configured_msaa_samples = AERON_SAMPLE_COUNT_4;
			break;
		case 8:
			s.configured_msaa_samples = AERON_SAMPLE_COUNT_8;
			break;
		default:
			Aeron_Log("xwa.remaster", "%s: presentation.msaa_samples must be 1, 2, 4, or 8", path);
			AeronConfigFile_Destroy(cf);
			return 0;
	}
	s.configured_hdr_output = AeronConfigFile_GetBool(cf, "presentation.hdr_output", 0);
	s.ssao.quality = (int)AeronConfigFile_GetInt(cf, "ssao.quality", 0);
	s.ssao.intensity = (float)AeronConfigFile_GetFloat(cf, "ssao.intensity", 0.0);
	s.ssao.power = (float)AeronConfigFile_GetFloat(cf, "ssao.power", 0.0);
	s.ssao.radius_view = (float)AeronConfigFile_GetFloat(cf, "ssao.radius_view", 0.0);
	s.ssao.bias_view = (float)AeronConfigFile_GetFloat(cf, "ssao.bias_view", 0.0);
	s.ssao.direct = (float)AeronConfigFile_GetFloat(cf, "ssao.direct", 0.0);
	s.ssao.debug_viz = AeronConfigFile_GetBool(cf, "ssao.debug_viz", 0);
	s.ssao.min_screen_frac = (float)AeronConfigFile_GetFloat(cf, "ssao.min_screen_frac", 0.0);
	s.ssao.max_screen_frac = (float)AeronConfigFile_GetFloat(cf, "ssao.max_screen_frac", 0.0);
	s.ssao.sample_jitter = (float)AeronConfigFile_GetFloat(cf, "ssao.sample_jitter", 0.0);
	if (!fl_ssao_config_valid(&s.ssao)) {
		Aeron_Log("xwa.remaster", "%s: invalid ssao settings", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	const char* shadow_mode = AeronConfigFile_GetString(cf, "shadows.mode", NULL);
	if (strcmp(shadow_mode, "off") == 0) {
		s.shadows.mode = XWA_FLIGHT_SHADOWS_OFF;
	} else if (strcmp(shadow_mode, "pcf") == 0) {
		s.shadows.mode = XWA_FLIGHT_SHADOWS_PCF;
	} else {
		Aeron_Log("xwa.remaster", "%s: shadows.mode must be 'off' or 'pcf'", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.shadows.atlas_size = (int)AeronConfigFile_GetInt(cf, "shadows.atlas_size", 0);
	s.shadows.cascade_count = (int)AeronConfigFile_GetInt(cf, "shadows.cascade_count", 0);
	const char* shadow_fit_mode = AeronConfigFile_GetString(cf, "shadows.fit_mode", NULL);
	if (strcmp(shadow_fit_mode, "stable") == 0) {
		s.shadows.fit_mode = AERON_SCENE_SHADOW_FIT_STABLE;
	} else if (strcmp(shadow_fit_mode, "frustum") == 0) {
		s.shadows.fit_mode = AERON_SCENE_SHADOW_FIT_FRUSTUM;
	} else if (strcmp(shadow_fit_mode, "scene_dependent") == 0) {
		s.shadows.fit_mode = AERON_SCENE_SHADOW_FIT_SCENE_DEPENDENT;
	} else {
		Aeron_Log("xwa.remaster", "%s: shadows.fit_mode must be 'stable', 'frustum', or 'scene_dependent'",
				  path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.shadows.max_distance = (float)AeronConfigFile_GetFloat(cf, "shadows.max_distance", 0.0);
	s.shadows.split_lambda = (float)AeronConfigFile_GetFloat(cf, "shadows.split_lambda", 0.0);
	s.shadows.explicit_splits = AeronConfigFile_GetBool(cf, "shadows.explicit_splits", 0);
	s.shadows.split_positions[0] = (float)AeronConfigFile_GetFloat(cf, "shadows.split_1", 0.0);
	s.shadows.split_positions[1] = (float)AeronConfigFile_GetFloat(cf, "shadows.split_2", 0.0);
	s.shadows.split_positions[2] = (float)AeronConfigFile_GetFloat(cf, "shadows.split_3", 0.0);
	s.shadows.filter_quality = (int)AeronConfigFile_GetInt(cf, "shadows.filter_quality", 0);
	s.shadows.filter_radius = (float)AeronConfigFile_GetFloat(cf, "shadows.filter_radius", 0.0);
	s.shadows.contact_hardening = AeronConfigFile_GetBool(cf, "shadows.contact_hardening", 0);
	s.shadows.light_angular_radius_degrees = (float)AeronConfigFile_GetFloat(
		cf, "shadows.light_angular_radius_degrees", 0.0);
	s.shadows.max_filter_radius = (float)AeronConfigFile_GetFloat(cf, "shadows.max_filter_radius", 0.0);
	s.shadows.pcss_min_filter_radius =
		(float)AeronConfigFile_GetFloat(cf, "shadows.pcss_min_filter_radius", 0.0);
	s.shadows.normal_bias = (float)AeronConfigFile_GetFloat(cf, "shadows.normal_bias", 0.0);
	s.shadows.normal_bias_face_normal = AeronConfigFile_GetBool(cf, "shadows.normal_bias_face_normal", 0);
	s.shadows.depth_bias_texels = (float)AeronConfigFile_GetFloat(cf, "shadows.depth_bias_texels", 0.0);
	s.shadows.slope_bias = (float)AeronConfigFile_GetFloat(cf, "shadows.slope_bias", 0.0);
	s.shadows.receiver_plane_bias =
		(float)AeronConfigFile_GetFloat(cf, "shadows.receiver_plane_bias", 0.0);
	s.shadows.caster_constant_bias =
		(float)AeronConfigFile_GetFloat(cf, "shadows.caster_constant_bias", 0.0);
	s.shadows.caster_slope_bias =
		(float)AeronConfigFile_GetFloat(cf, "shadows.caster_slope_bias", 0.0);
	s.shadows.transition_fraction =
		(float)AeronConfigFile_GetFloat(cf, "shadows.transition_fraction", 0.0);
	s.shadows.distance_fade_fraction =
		(float)AeronConfigFile_GetFloat(cf, "shadows.distance_fade_fraction", 0.0);
	s.shadows.debug_cascades = AeronConfigFile_GetBool(cf, "shadows.debug_cascades", 0);
	s.shadows.debug_atlas_cascade = -1;
	if (!fl_shadows_config_valid(&s.shadows)) {
		Aeron_Log("xwa.remaster", "%s: invalid shadows settings", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.hangar_lighting.enabled = AeronConfigFile_GetBool(cf, "hangar_lighting.enabled", 0);
	s.hangar_lighting.direction[0] =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.direction_x", 0.0);
	s.hangar_lighting.direction[1] =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.direction_y", 0.0);
	s.hangar_lighting.direction[2] =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.direction_z", 0.0);
	s.hangar_lighting.intensity =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.intensity", 0.0);
	s.hangar_lighting.color[0] =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.color_r", 0.0);
	s.hangar_lighting.color[1] =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.color_g", 0.0);
	s.hangar_lighting.color[2] =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.color_b", 0.0);
	s.hangar_lighting.ambient_ceiling =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.ambient_ceiling", 0.0);
	s.hangar_lighting.ambient_sides =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.ambient_sides", 0.0);
	s.hangar_lighting.ambient_floor =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.ambient_floor", 0.0);
	s.hangar_lighting.shadow_filter_radius = (float)AeronConfigFile_GetFloat(
		cf, "hangar_lighting.shadow_filter_radius", 0.0);
	const float hangar_direction_length =
		sqrtf(s.hangar_lighting.direction[0] * s.hangar_lighting.direction[0] +
			  s.hangar_lighting.direction[1] * s.hangar_lighting.direction[1] +
			  s.hangar_lighting.direction[2] * s.hangar_lighting.direction[2]);
	if (!fl_hangar_lighting_config_valid(&s.hangar_lighting, hangar_direction_length)) {
		Aeron_Log("xwa.remaster", "%s: invalid hangar_lighting settings", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	for (int axis = 0; axis < 3; axis++) {
		s.hangar_lighting.direction[axis] /= hangar_direction_length;
	}
	s.plight.enabled = AeronConfigFile_GetBool(cf, "point_lights.enabled", 0);
	s.plight.scale = (float)AeronConfigFile_GetFloat(cf, "point_lights.scale", 0.0);
	s.plight.range_scale = (float)AeronConfigFile_GetFloat(cf, "point_lights.range_scale", 0.0);
	s.plight.min_distance = (float)AeronConfigFile_GetFloat(cf, "point_lights.min_distance", 0.0);
	s.plight.spec_weight = (float)AeronConfigFile_GetFloat(cf, "point_lights.spec_weight", 0.0);
	s.plight.diffuse_wrap = (float)AeronConfigFile_GetFloat(cf, "point_lights.diffuse_wrap", 0.0);
	s.plight.contrib_cap = (float)AeronConfigFile_GetFloat(cf, "point_lights.contrib_cap", 0.0);
	if (!fl_point_lights_config_valid(&s.plight)) {
		Aeron_Log("xwa.remaster", "%s: invalid point_lights settings", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.texture_filtering.anisotropic = AeronConfigFile_GetBool(cf, "texture_filtering.anisotropic", 0);
	s.texture_filtering.max_anisotropy =
		(float)AeronConfigFile_GetFloat(cf, "texture_filtering.max_anisotropy", 0.0);
	if (s.texture_filtering.max_anisotropy < 1.0f || s.texture_filtering.max_anisotropy > 16.0f) {
		Aeron_Log("xwa.remaster", "%s: texture_filtering.max_anisotropy must be between 1 and 16", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.sky_enabled = AeronConfigFile_GetBool(cf, "skybox.enabled", 0);
	s.sky_exposure = (float)AeronConfigFile_GetFloat(cf, "skybox.exposure", 0.0);
	const char* sky_path = AeronConfigFile_GetString(cf, "skybox.path", NULL);
	if (!sky_path[0] || strlen(sky_path) >= sizeof s.sky_path) {
		Aeron_Log("xwa.remaster", "%s: skybox.path must be non-empty and shorter than %zu bytes", path,
				  sizeof s.sky_path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	snprintf(s.sky_path, sizeof s.sky_path, "%s", sky_path);

	/* Sky source selection: "cube" uses the authored cube map;
	 * "stars"/"procedural" uses the GPU twinkling starfield. */
	const char* sky_mode = AeronConfigFile_GetString(cf, "skybox.mode", NULL);
	if (strcmp(sky_mode, "cube") == 0) {
		s.sky_mode = 0;
	} else if (strcmp(sky_mode, "stars") == 0 || strcmp(sky_mode, "procedural") == 0) {
		s.sky_mode = 1;
	} else {
		Aeron_Log("xwa.remaster", "%s: skybox.mode must be 'cube', 'stars' or 'procedural'", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.star_brightness = (float)AeronConfigFile_GetFloat(cf, "skybox.star_brightness", 0.0);
	s.star_density = (float)AeronConfigFile_GetFloat(cf, "skybox.star_density", 0.0);
	s.star_grid = (float)AeronConfigFile_GetFloat(cf, "skybox.star_grid", 0.0);
	s.star_core_px = (float)AeronConfigFile_GetFloat(cf, "skybox.star_core_px", 0.0);
	s.star_feather = (float)AeronConfigFile_GetFloat(cf, "skybox.star_feather", 0.0);
	s.star_flare = (float)AeronConfigFile_GetFloat(cf, "skybox.star_flare", 0.0);
	if (!fl_sky_config_valid()) {
		Aeron_Log("xwa.remaster", "%s: invalid skybox settings", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}

	XwaShipPbrTuning pbr;
	memset(&pbr, 0, sizeof pbr);
	pbr.light_intensity = (float)AeronConfigFile_GetFloat(cf, "lighting.intensity", 0.0);
	pbr.global_spec_mul = (float)AeronConfigFile_GetFloat(cf, "lighting.spec_mul", 0.0);
	pbr.light_wrap = (float)AeronConfigFile_GetFloat(cf, "lighting.wrap", 0.0);
	pbr.spec_geom_adapt = AeronConfigFile_GetBool(cf, "lighting.spec_geom_adapt", 0) ? 1.0f : 0.0f;
	pbr.ambient[0] = (float)AeronConfigFile_GetFloat(cf, "lighting.ambient_r", 0.0);
	pbr.ambient[1] = (float)AeronConfigFile_GetFloat(cf, "lighting.ambient_g", 0.0);
	pbr.ambient[2] = (float)AeronConfigFile_GetFloat(cf, "lighting.ambient_b", 0.0);
	if (!fl_pbr_config_valid(&pbr)) {
		Aeron_Log("xwa.remaster", "%s: invalid lighting settings", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	XwaRemasterShip_ConfigurePbrTuning(&pbr);

	/* Bloom contribution weight (process-wide present knob; 0 = off,
	 * chain skipped). */
	const float bloom_intensity = (float)AeronConfigFile_GetFloat(cf, "bloom.intensity", 0.0);
	if (!isfinite(bloom_intensity) || bloom_intensity < 0.0f) {
		Aeron_Log("xwa.remaster", "%s: bloom.intensity must be finite and non-negative", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	AeronSceneBloom_SetIntensity(bloom_intensity);

	s.mb_quality = (int)AeronConfigFile_GetInt(cf, "motion_blur.quality", 0);
	if (s.mb_quality < 0 || s.mb_quality > 2) {
		Aeron_Log("xwa.remaster", "%s: motion_blur.quality must be between 0 and 2", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.mb_shutter = (float)AeronConfigFile_GetFloat(cf, "motion_blur.shutter", 0.0);
	if (!isfinite(s.mb_shutter) || s.mb_shutter < 0.0f) {
		Aeron_Log("xwa.remaster", "%s: motion_blur.shutter must be finite and non-negative", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.mb_camera_blur = AeronConfigFile_GetBool(cf, "motion_blur.camera_blur", 0);
	s.mb_pause_keep_blur = AeronConfigFile_GetBool(cf, "motion_blur.pause_keep_blur", 0);
	s.mb_velocity_viz = AeronConfigFile_GetBool(cf, "motion_blur.velocity_viz", 0);
	s.mb_fsr_direct_motion = AeronConfigFile_GetBool(cf, "motion_blur.fsr_direct_motion", 0);

	const char* fsr_mode = AeronConfigFile_GetString(cf, "temporal_upscaling.mode", NULL);
	if (!AeronTemporal_ParseMode(fsr_mode, &s.fsr_mode)) {
		Aeron_Log("xwa.remaster", "%s: invalid temporal_upscaling.mode '%s'", path, fsr_mode);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	if (s.fsr_mode != AERON_TEMPORAL_OFF && s.configured_msaa_samples != AERON_SAMPLE_COUNT_1) {
		Aeron_Log("xwa.remaster",
				  "%s: temporal_upscaling.mode and presentation.msaa_samples cannot both be enabled", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.fsr_sharpness = (float)AeronConfigFile_GetFloat(cf, "temporal_upscaling.sharpness", 0.0);
	if (s.fsr_sharpness < 0.0f || s.fsr_sharpness > 1.0f) {
		Aeron_Log("xwa.remaster", "%s: temporal_upscaling.sharpness %.3f must be between 0 and 1", path,
				  (double)s.fsr_sharpness);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.ssao_default = s.ssao;
	s.shadows_default = s.shadows;
	s.hangar_lighting_default = s.hangar_lighting;
	s.plight_default = s.plight;
	s.texture_filtering_default = s.texture_filtering;
	fl_request_fsr_reset(FL_FSR_RESET_INITIAL);
	AeronConfigFile_Destroy(cf);
	s.config_loaded = 1;
	Aeron_Log("xwa.remaster",
			  "flight ssao: quality %d intensity %.2f power %.2f radius %.1f bias %.2f "
			  "direct %.2f%s",
			  s.ssao.quality, (double)s.ssao.intensity, (double)s.ssao.power, (double)s.ssao.radius_view,
			  (double)s.ssao.bias_view, (double)s.ssao.direct, s.ssao.debug_viz ? " (debug viz)" : "");
	if (s.texture_filtering.anisotropic) {
		Aeron_Log("xwa.remaster", "flight mesh filtering: trilinear + %.0fx anisotropic",
				  (double)s.texture_filtering.max_anisotropy);
	} else {
		Aeron_Log("xwa.remaster", "flight mesh filtering: trilinear");
	}
	Aeron_Log("xwa.remaster", "flight FSR 3.1.4 mode: %s%s", AeronTemporal_ModeName(s.fsr_mode),
			  s.fsr_sharpness > 0.0f ? " with RCAS" : "");
	Aeron_Log("xwa.remaster", "presentation: %.2f Hz display, VSync divisor %d, %.2f fps target",
			  Aeron_DisplayRefreshRate(), Aeron_PresentationVsyncDivisor(), Aeron_PresentationRate());
	return 1;
}

static int fl_cfg_ensure(void) { return XwaRemasterFlight_InitConfig(Aeron_GetVfs()); }

void XwaRemasterFlight_GetPresentationDefaults(AeronSampleCount* msaa_samples, int* hdr_output) {
	if (!fl_cfg_ensure()) {
		return;
	}
	if (msaa_samples) {
		*msaa_samples = s.configured_msaa_samples;
	}
	if (hdr_output) {
		*hdr_output = s.configured_hdr_output;
	}
}

int XwaRemasterFlight_ProcessAssetsNeedPrepare(void) {
	return !s.process_assets_prepared;
}

int XwaRemasterFlight_PrepareProcessAssets(AeronCommandBuffer* cmd, XwaRemasterAssets* assets) {
	if (!cmd || !assets || s.process_assets_prepared || !fl_cfg_ensure()) {
		return 0;
	}
	if (!XwaRemasterAssets_PrepareFlightFonts(assets, cmd)) {
		return 0;
	}
	int loaded_font_tiers = 0;
	for (int tier = 0; tier < 3; tier++) {
		loaded_font_tiers += XwaRemasterAssets_FlightFont(assets, tier) != NULL;
	}
	Aeron_Log("xwa.remaster", "flight HUD fonts: %d/3 tiers loaded", loaded_font_tiers);
	if (loaded_font_tiers != 3) {
		return 0;
	}
	/* The authored cube map is only needed in cube mode; the procedural
	 * starfield (sky_mode 1) never samples it, so skip the load + GPU
	 * upload entirely. */
	if (s.sky_mode == 0) {
		char sky_path[600];
		snprintf(sky_path, sizeof sky_path, "%s/%s", XwaRemasterAssets_Root(assets), s.sky_path);
		s.sky_cube = Aeron_ImageLoadCubemapKtx2(cmd, sky_path);
		Aeron_Log("xwa.remaster", "flight skybox %s: %s", s.sky_cube ? "loaded" : "load failed", sky_path);
		if (!s.sky_cube) {
			return 0;
		}
	} else {
		Aeron_Log("xwa.remaster", "flight sky: procedural starfield (cube map not loaded)");
	}
	return 1;
}

void XwaRemasterFlight_CommitProcessAssets(void) { s.process_assets_prepared = 1; }

void XwaRemasterFlight_GetSsao(XwaFlightSsaoParams* out) {
	if (out) {
		if (!fl_cfg_ensure())
			return;
		*out = s.ssao;
	}
}

void XwaRemasterFlight_SetSsao(const XwaFlightSsaoParams* in) {
	if (in) {
		if (!fl_cfg_ensure())
			return;
		s.ssao = *in;
	}
}

void XwaRemasterFlight_GetSsaoDefault(XwaFlightSsaoParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.ssao_default;
	}
}

void XwaRemasterFlight_GetShadows(XwaFlightShadowParams* out) {
	if (out) {
		if (!fl_cfg_ensure())
			return;
		*out = s.shadows;
	}
}

static void fl_sanitize_shadow_split_positions(float positions[3]) {
	const float gap = 0.001f;
	positions[0] = fminf(fmaxf(positions[0], gap), 1.0f - 3.0f * gap);
	positions[1] = fminf(fmaxf(positions[1], positions[0] + gap), 1.0f - 2.0f * gap);
	positions[2] = fminf(fmaxf(positions[2], positions[1] + gap), 1.0f - gap);
}

void XwaRemasterFlight_SetShadows(const XwaFlightShadowParams* in) {
	if (!in || !fl_cfg_ensure())
		return;
	s.shadows = *in;
	s.shadows.mode = in->mode == XWA_FLIGHT_SHADOWS_PCF ? XWA_FLIGHT_SHADOWS_PCF : XWA_FLIGHT_SHADOWS_OFF;
	if (s.shadows.atlas_size != 1024 && s.shadows.atlas_size != 2048 && s.shadows.atlas_size != 4096 &&
		s.shadows.atlas_size != 8192) {
		s.shadows.atlas_size = s.shadows_default.atlas_size;
	}
	s.shadows.cascade_count =
		s.shadows.cascade_count < 1
			? 1
			: (s.shadows.cascade_count > AERON_SCENE_SHADOW_MAX_CASCADES ? AERON_SCENE_SHADOW_MAX_CASCADES
																		 : s.shadows.cascade_count);
	s.shadows.fit_mode = s.shadows.fit_mode < AERON_SCENE_SHADOW_FIT_STABLE ||
								 s.shadows.fit_mode > AERON_SCENE_SHADOW_FIT_SCENE_DEPENDENT
							 ? s.shadows_default.fit_mode
							 : s.shadows.fit_mode;
	s.shadows.max_distance = fmaxf(s.shadows.max_distance, FL_NEAR_Z + 1.0f);
	s.shadows.split_lambda = fminf(fmaxf(s.shadows.split_lambda, 0.0f), 1.0f);
	s.shadows.explicit_splits = in->explicit_splits ? 1 : 0;
	fl_sanitize_shadow_split_positions(s.shadows.split_positions);
	s.shadows.filter_quality =
		s.shadows.filter_quality < 0 ? 0 : (s.shadows.filter_quality > 3 ? 3 : s.shadows.filter_quality);
	s.shadows.filter_radius = fminf(fmaxf(s.shadows.filter_radius, 0.5f), 3.0f);
	s.shadows.contact_hardening = in->contact_hardening ? 1 : 0;
	s.shadows.light_angular_radius_degrees = fminf(fmaxf(s.shadows.light_angular_radius_degrees, 0.0f), 5.0f);
	s.shadows.max_filter_radius = fminf(fmaxf(s.shadows.max_filter_radius, s.shadows.filter_radius), 16.0f);
	s.shadows.pcss_min_filter_radius =
		fminf(fmaxf(s.shadows.pcss_min_filter_radius, 0.5f), s.shadows.filter_radius);
	s.shadows.normal_bias = fmaxf(s.shadows.normal_bias, 0.0f);
	s.shadows.normal_bias_face_normal = in->normal_bias_face_normal ? 1 : 0;
	s.shadows.depth_bias_texels = fmaxf(s.shadows.depth_bias_texels, 0.0f);
	s.shadows.slope_bias = fmaxf(s.shadows.slope_bias, 0.0f);
	s.shadows.receiver_plane_bias = fminf(fmaxf(s.shadows.receiver_plane_bias, 0.0f), 2.0f);
	s.shadows.caster_constant_bias = fminf(fmaxf(s.shadows.caster_constant_bias, 0.0f), 16.0f);
	s.shadows.caster_slope_bias = fminf(fmaxf(s.shadows.caster_slope_bias, 0.0f), 16.0f);
	s.shadows.transition_fraction = fminf(fmaxf(s.shadows.transition_fraction, 0.0f), 0.5f);
	s.shadows.distance_fade_fraction = fminf(fmaxf(s.shadows.distance_fade_fraction, 0.0f), 0.5f);
	s.shadows.debug_cascades = in->debug_cascades ? 1 : 0;
	s.shadows.debug_atlas = in->debug_atlas ? 1 : 0;
	s.shadows.debug_atlas_cascade = in->debug_atlas_cascade;
	s.shadows.debug_atlas_cascade =
		s.shadows.debug_atlas_cascade < -1
			? -1
			: (s.shadows.debug_atlas_cascade >= s.shadows.cascade_count ? s.shadows.cascade_count - 1
																		: s.shadows.debug_atlas_cascade);
}

void XwaRemasterFlight_GetShadowsDefault(XwaFlightShadowParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.shadows_default;
	}
}

void XwaRemasterFlight_GetShadowStats(AeronSceneDirectionalShadowStats* out) {
	if (out) {
		memset(out, 0, sizeof *out);
		if (s.scene) {
			AeronScene_GetDirectionalShadowStats(s.scene, out);
		}
	}
}

static void fl_sanitize_hangar_lighting(XwaFlightHangarLightingParams* params) {
	params->enabled = params->enabled ? 1 : 0;
	const float length =
		sqrtf(params->direction[0] * params->direction[0] + params->direction[1] * params->direction[1] +
			  params->direction[2] * params->direction[2]);
	if (length > 0.0001f && isfinite(length)) {
		for (int axis = 0; axis < 3; axis++) {
			params->direction[axis] /= length;
		}
	} else {
		memcpy(params->direction, s.hangar_lighting_default.direction, sizeof params->direction);
	}
	params->intensity = fminf(fmaxf(params->intensity, 0.0f), 4.0f);
	for (int channel = 0; channel < 3; channel++) {
		params->color[channel] = fminf(fmaxf(params->color[channel], 0.0f), 4.0f);
	}
	params->ambient_ceiling = fminf(fmaxf(params->ambient_ceiling, 0.0f), 4.0f);
	params->ambient_sides = fminf(fmaxf(params->ambient_sides, 0.0f), 4.0f);
	params->ambient_floor = fminf(fmaxf(params->ambient_floor, 0.0f), 4.0f);
	params->shadow_filter_radius = fminf(fmaxf(params->shadow_filter_radius, 0.5f), 3.0f);
}

void XwaRemasterFlight_GetHangarLighting(XwaFlightHangarLightingParams* out) {
	if (out) {
		if (!fl_cfg_ensure())
			return;
		*out = s.hangar_lighting;
	}
}

void XwaRemasterFlight_SetHangarLighting(const XwaFlightHangarLightingParams* in) {
	if (!in || !fl_cfg_ensure())
		return;
	s.hangar_lighting = *in;
	fl_sanitize_hangar_lighting(&s.hangar_lighting);
}

void XwaRemasterFlight_GetHangarLightingDefault(XwaFlightHangarLightingParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.hangar_lighting_default;
	}
}

void XwaRemasterFlight_GetPointLights(XwaFlightPointLightParams* out) {
	if (out) {
		if (!fl_cfg_ensure())
			return;
		*out = s.plight;
	}
}

void XwaRemasterFlight_SetPointLights(const XwaFlightPointLightParams* in) {
	if (in) {
		if (!fl_cfg_ensure())
			return;
		s.plight = *in;
	}
}

void XwaRemasterFlight_GetPointLightsDefault(XwaFlightPointLightParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.plight_default;
	}
}

static int fl_apply_texture_filtering(const XwaFlightTextureFilteringParams* in) {
	AeronSampler* sampler = NULL;
	if (in->anisotropic) {
		sampler = Aeron_CreateSampler(&(AeronSamplerDesc) {
			.min_filter = AERON_FILTER_LINEAR,
			.mag_filter = AERON_FILTER_LINEAR,
			.mip_filter = AERON_FILTER_LINEAR,
			.address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
			.address_v = AERON_ADDRESS_CLAMP_TO_EDGE,
			.address_w = AERON_ADDRESS_CLAMP_TO_EDGE,
			.max_lod = 1000.0f,
			.enable_anisotropy = 1,
			.max_anisotropy = in->max_anisotropy,
		});
		if (!sampler) {
			Aeron_Log("xwa.remaster",
					  "flight: anisotropic mesh sampler creation failed; keeping previous filtering");
			return 0;
		}
	}
	if (s.scene && !AeronScene_SetMeshSampler(s.scene, sampler)) {
		Aeron_DestroySampler(sampler);
		Aeron_Log("xwa.remaster", "flight: mesh sampler configuration failed");
		return 0;
	}
	if (s.mesh_sampler) {
		Aeron_DestroySampler(s.mesh_sampler);
	}
	s.mesh_sampler = sampler;
	s.mesh_sampler_configured = 1;
	s.texture_filtering = *in;
	return 1;
}

void XwaRemasterFlight_GetTextureFiltering(XwaFlightTextureFilteringParams* out) {
	if (out) {
		if (!fl_cfg_ensure())
			return;
		*out = s.texture_filtering;
	}
}

void XwaRemasterFlight_SetTextureFiltering(const XwaFlightTextureFilteringParams* in) {
	if (!in || !fl_cfg_ensure())
		return;
	XwaFlightTextureFilteringParams p = *in;
	p.anisotropic = p.anisotropic != 0;
	if (p.max_anisotropy < 1.0f)
		p.max_anisotropy = 1.0f;
	if (p.max_anisotropy > 16.0f)
		p.max_anisotropy = 16.0f;
	if (p.anisotropic == s.texture_filtering.anisotropic &&
		p.max_anisotropy == s.texture_filtering.max_anisotropy && s.mesh_sampler_configured)
		return;
	fl_apply_texture_filtering(&p);
}

void XwaRemasterFlight_GetTextureFilteringDefault(XwaFlightTextureFilteringParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.texture_filtering_default;
	}
}

void XwaRemasterFlight_GetMotionBlur(XwaFlightMotionBlurParams* out) {
	if (out) {
		if (!fl_cfg_ensure())
			return;
		out->quality = s.mb_quality;
		out->shutter = s.mb_shutter;
		out->camera_blur = s.mb_camera_blur;
		out->pause_keep_blur = s.mb_pause_keep_blur;
		out->velocity_viz = s.mb_velocity_viz;
	}
}

void XwaRemasterFlight_SetMotionBlur(const XwaFlightMotionBlurParams* in) {
	if (in) {
		if (!fl_cfg_ensure())
			return;
		s.mb_quality = in->quality < 0 ? 0 : (in->quality > 2 ? 2 : in->quality);
		s.mb_shutter = in->shutter;
		s.mb_camera_blur = in->camera_blur;
		s.mb_pause_keep_blur = in->pause_keep_blur;
		s.mb_velocity_viz = in->velocity_viz;
	}
}

void XwaRemasterFlight_GetTemporal(XwaFlightTemporalParams* out) {
	if (!out) {
		return;
	}
	memset(out, 0, sizeof *out);
	if (!fl_cfg_ensure()) {
		return;
	}
	out->mode = s.fsr_mode;
	out->sharpness = s.fsr_sharpness;
	out->debug_view = s.fsr_debug_view;
}

void XwaRemasterFlight_SetTemporal(const XwaFlightTemporalParams* in) {
	if (!in || !fl_cfg_ensure()) {
		return;
	}
	AeronTemporalMode mode = in->mode;
	if (mode < AERON_TEMPORAL_OFF || mode > AERON_TEMPORAL_PERFORMANCE) {
		mode = AERON_TEMPORAL_OFF;
	}
	if (mode != AERON_TEMPORAL_OFF && XwaRemaster_MsaaSampleCount() != AERON_SAMPLE_COUNT_1) {
		Aeron_Log("xwa.remaster", "FSR cannot be enabled while MSAA is active");
		mode = AERON_TEMPORAL_OFF;
	}
	if (mode != s.fsr_mode) {
		s.fsr_mode = mode;
		fl_request_fsr_reset(FL_FSR_RESET_MODE_CHANGE);
	}
	s.fsr_sharpness = fminf(fmaxf(in->sharpness, 0.0f), 1.0f);
	s.fsr_debug_view = in->debug_view ? 1 : 0;
}

void XwaRemasterFlight_GetTemporalStats(XwaFlightTemporalStats* out) {
	if (!out) {
		return;
	}
	memset(out, 0, sizeof *out);
	if (!s.scene) {
		return;
	}
	out->history_reset_active = s.fsr_consecutive_reset_frames != 0;
	out->history_reset_consecutive_frames = s.fsr_consecutive_reset_frames;
	fl_format_fsr_reset_reasons(s.fsr_last_reset_reasons, out->history_reset_reasons,
								sizeof out->history_reset_reasons);
	AeronScene_RenderDims(s.scene, &out->render_width, &out->render_height);
	AeronScene_RtDims(s.scene, &out->output_width, &out->output_height);
	out->profile_available = AeronScene_GetTemporalProfileInfo(s.scene, &out->profile);
}

static void fl_destroy_sized_resources(void) {
	s.direct_ready = 0;
	s.present_scene_tex = NULL;
	s.present_bloom_tex = NULL;
	if (s.present_rt) {
		Aeron_DestroyRenderTarget(s.present_rt);
	}
	if (s.bloom) {
		AeronSceneBloom_Destroy(s.bloom);
	}
	if (s.scene) {
		AeronScene_Destroy(s.scene);
	}
	s.present_rt = NULL;
	s.bloom = NULL;
	s.scene = NULL;
	s.rt_w = 0;
	s.rt_h = 0;
	s.scene_requested_samples = 0;
	s.mb_enabled = 0;
	s.mb_prev_snap = NULL;
	s.snap = NULL;
	s.fsr_prev_host_valid = 0;
	fl_request_fsr_reset(FL_FSR_RESET_RESOURCES_RECREATED);
}

static int fl_ensure(int target_w, int target_h) {
	if (!fl_cfg_ensure())
		return 0;
	if (!s.mesh_sampler_configured && !fl_apply_texture_filtering(&s.texture_filtering)) {
		return 0;
	}
	if (target_w <= 0 || target_h <= 0) {
		return 0;
	}
	if (!s.trails) {
		s.trails = XwaRemasterTrails_Create();
		if (!s.trails) {
			Aeron_Log("xwa.remaster", "flight: trail renderer create failed");
			return 0;
		}
	}
	if (!s.particles) {
		s.particles = XwaRemasterParticles_Create();
		if (!s.particles) {
			Aeron_Log("xwa.remaster", "flight: particle renderer create failed");
			return 0;
		}
	}
	if (!s.hyperspace) {
		s.hyperspace = XwaRemasterHyperspace_Create();
		if (!s.hyperspace) {
			Aeron_Log("xwa.remaster", "flight: hyperspace renderer create failed");
			return 0;
		}
	}
	if (s.sky_mode == 1 && !s.sky_stars) {
		s.sky_stars = XwaRemasterSkyStars_Create();
		if (!s.sky_stars) {
			Aeron_Log("xwa.remaster", "flight: starfield renderer create failed");
			return 0;
		}
	}
	if (!s.present) {
		s.present = AeronScenePresentChain_Create(AERON_TEXTURE_FORMAT_RGBA16_FLOAT);
	}
	if (!s.present_sampler) {
		s.present_sampler =
			Aeron_CreateSampler(&(AeronSamplerDesc) { .min_filter = AERON_FILTER_LINEAR,
													  .mag_filter = AERON_FILTER_LINEAR,
													  .address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
													  .address_v = AERON_ADDRESS_CLAMP_TO_EDGE });
	}
	if (!s.present || !s.present_sampler) {
		Aeron_Log("xwa.remaster", "flight: present resources create failed");
		return 0;
	}
	const AeronSampleCount requested_samples = XwaRemaster_MsaaSampleCount();
	if (s.scene && s.rt_w == target_w && s.rt_h == target_h &&
		s.scene_requested_samples == requested_samples) {
		return 1;
	}
	fl_destroy_sized_resources();
	s.rt_w = target_w;
	s.rt_h = target_h;
	s.scene = AeronScene_Create(&(AeronScene3DDesc) {
		.rt_width = target_w,
		.rt_height = target_h,
		.color_format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
		.with_normal_rt = 1, /* SSAO / motion-blur G-buffer */
		.sample_count = requested_samples,
		.temporal_mode = s.fsr_mode,
		.temporal_sharpness = s.fsr_sharpness,
		.view_space_to_meters = 1.0f / 40.96f,
	});
	if (!s.scene) {
		Aeron_Log("xwa.remaster", "flight: scene create failed at %dx%d", target_w, target_h);
		s.rt_w = s.rt_h = 0;
		return 0;
	}
	s.scene_requested_samples = requested_samples;
	/* Decode the classic RGB555 clear color 0x0001 into the linear HDR scene. */
	const float classic_clear_blue = powf(2.0f / 31.0f, 2.2f);
	AeronScene_SetClearColor(s.scene, (const float[4]) { 0.0f, 0.0f, classic_clear_blue, 1.0f });
	if (s.mesh_sampler && !AeronScene_SetMeshSampler(s.scene, s.mesh_sampler)) {
		Aeron_Log("xwa.remaster", "flight: scene mesh sampler configuration failed");
		return 0;
	}

	s.bloom = AeronSceneBloom_Create(target_w, target_h);
	s.present_rt =
		Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = target_w,
															.height = target_h,
															.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
															.debug_name = "xwa.flight.present" });
	if (!s.bloom) {
		Aeron_Log("xwa.remaster", "flight: bloom create failed");
		return 0;
	}
	if (!s.present_rt) {
		Aeron_Log("xwa.remaster", "flight: present target create failed");
		return 0;
	}
	Aeron_Log("xwa.remaster", "flight render targets: %dx%d", target_w, target_h);
	return 1;
}

/* Row-major world->eye rotation rows -> (w, x, y, z) quaternion —
 * Shepperd's method, mirroring TIE's tie_snap_mat3_to_quat. */
static void fl_rows_to_quat(const float m[9], float q[4]) {
	const float trace = m[0] + m[4] + m[8];
	if (trace > 0.0f) {
		const float sq = sqrtf(trace + 1.0f) * 2.0f;
		q[0] = 0.25f * sq;
		q[1] = (m[7] - m[5]) / sq;
		q[2] = (m[2] - m[6]) / sq;
		q[3] = (m[3] - m[1]) / sq;
	} else if (m[0] > m[4] && m[0] > m[8]) {
		const float sq = sqrtf(1.0f + m[0] - m[4] - m[8]) * 2.0f;
		q[0] = (m[7] - m[5]) / sq;
		q[1] = 0.25f * sq;
		q[2] = (m[1] + m[3]) / sq;
		q[3] = (m[2] + m[6]) / sq;
	} else if (m[4] > m[8]) {
		const float sq = sqrtf(1.0f + m[4] - m[0] - m[8]) * 2.0f;
		q[0] = (m[2] - m[6]) / sq;
		q[1] = (m[1] + m[3]) / sq;
		q[2] = 0.25f * sq;
		q[3] = (m[5] + m[7]) / sq;
	} else {
		const float sq = sqrtf(1.0f + m[8] - m[0] - m[4]) * 2.0f;
		q[0] = (m[3] - m[1]) / sq;
		q[1] = (m[2] + m[6]) / sq;
		q[2] = (m[5] + m[7]) / sq;
		q[3] = 0.25f * sq;
	}
}

/* Clone of the scene's scene_quat_to_mat3 (scene3d.c) — any
 * drift between the two is a bug. */
/* Scene camera from a captured flight camera: the classic projection
 * (screen = vp_center + proj_scale * xy/z, y down, TRANSFM2_*;
 * reversed-Z depth = FL_NEAR_Z / z) expressed through the scene's
 * parametric camera (half-FOV angles + off-center NDC offsets). Used
 * for the frame camera AND the previous snapshot's camera (motion
 * blur). */
static void fl_quat_to_mat3(const float q[4], float m[9]) {
	const float w = q[0], x = q[1], y = q[2], z = q[3];
	const float xx = x * x, yy = y * y, zz = z * z;
	const float xy = x * y, xz = x * z, yz = y * z;
	const float wx = w * x, wy = w * y, wz = w * z;
	m[0] = 1.0f - 2.0f * (yy + zz);
	m[1] = 2.0f * (xy - wz);
	m[2] = 2.0f * (xz + wy);
	m[3] = 2.0f * (xy + wz);
	m[4] = 1.0f - 2.0f * (xx + zz);
	m[5] = 2.0f * (yz - wx);
	m[6] = 2.0f * (xz - wy);
	m[7] = 2.0f * (yz + wx);
	m[8] = 1.0f - 2.0f * (xx + yy);
}

static int fl_build_view_at_origin(const XwaFlightCamera* cam, const int32_t origin_world[3], int target_w,
								   int target_h, XwaRemasterFlightView* out) {
	if (!cam || !out || target_w <= 0 || target_h <= 0) {
		return 0;
	}
	memset(out, 0, sizeof *out);
	AeronSceneCamera* scene_camera = &out->camera;
	/* The captured focal defines the preserved vertical field of view. */
	const float ps = cam->proj_scale > 0.0f ? cam->proj_scale : 512.0f;
	const float source_w =
		cam->vp_w > 0 ? (float)cam->vp_w : (cam->screen_w > 0 ? (float)cam->screen_w : 640.0f);
	const float source_h =
		cam->vp_h > 0 ? (float)cam->vp_h : (cam->screen_h > 0 ? (float)cam->screen_h : 480.0f);
	const float hw = source_w * 0.5f;
	const float hh = source_h * 0.5f;
	const float cx = (float)cam->vp_center_x;
	/* Classic screen-Y center is vp_center + proj_offset (BOTH terms:
	 * TRANSFM2_ProjectScreenY and the model vertex path agree); the
	 * cockpit view shifts the horizon through a nonzero offset. */
	const float cy = (float)cam->vp_center_y + (float)cam->proj_offset_y;

	memcpy(out->origin_world, origin_world, sizeof out->origin_world);
	XwaRemasterWorld_LocalI32(origin_world, cam->world_pos, scene_camera->pos);
	scene_camera->v_half_rad = atanf(hh / ps);
	const float aspect = (float)target_w / (float)target_h;
	scene_camera->h_half_rad = atanf(tanf(scene_camera->v_half_rad) * aspect);
	scene_camera->near_z = FL_NEAR_Z;
	scene_camera->proj_x_offset = (cx - hw) / hw;
	scene_camera->proj_y_offset = (hh - cy) / hh;
	scene_camera->viewport = (AeronRectI) { 0, 0, target_w, target_h };
	fl_rows_to_quat(cam->rows, scene_camera->ori);
	out->viewport = scene_camera->viewport;
	out->classic_pixel_scale = (float)target_h / source_h;
	AeronScene_ComputeViewProj(scene_camera, out->view_proj);
	return 1;
}

int XwaRemasterFlight_BuildView(const XwaFlightCamera* cam, int target_w, int target_h,
								XwaRemasterFlightView* out) {
	return cam ? fl_build_view_at_origin(cam, cam->world_pos, target_w, target_h, out) : 0;
}

int XwaRemasterFlight_ProjectLocal(const XwaRemasterFlightView* view, const float local[3], float* out_x,
								   float* out_y, float* out_depth) {
	if (!view || !local || !out_x || !out_y) {
		return 0;
	}
	const float* m = view->view_proj;
	const float cx = m[0] * local[0] + m[1] * local[1] + m[2] * local[2] + m[3];
	const float cy = m[4] * local[0] + m[5] * local[1] + m[6] * local[2] + m[7];
	const float cw = m[12] * local[0] + m[13] * local[1] + m[14] * local[2] + m[15];
	if (out_depth) {
		*out_depth = cw;
	}
	if (cw <= 0.0f) {
		return 0;
	}
	const float ndc_x = cx / cw;
	const float ndc_y = cy / cw;
	*out_x = view->viewport.x + (ndc_x + 1.0f) * 0.5f * view->viewport.width;
	*out_y = view->viewport.y + (1.0f - ndc_y) * 0.5f * view->viewport.height;
	return 1;
}

int XwaRemasterFlight_ProjectWorldI32(const XwaRemasterFlightView* view, const int32_t world[3], float* out_x,
									  float* out_y, float* out_depth) {
	float local[3];
	if (!view || !world)
		return 0;
	XwaRemasterWorld_LocalI32(view->origin_world, world, local);
	return XwaRemasterFlight_ProjectLocal(view, local, out_x, out_y, out_depth);
}

int XwaRemasterFlight_ProjectView(const XwaRemasterFlightView* view, const float eye[3], float* out_x,
								  float* out_y) {
	if (!view || !eye || !out_x || !out_y || eye[2] <= 0.0f) {
		return 0;
	}
	const float ndc_x = eye[0] / (eye[2] * tanf(view->camera.h_half_rad)) + view->camera.proj_x_offset;
	const float ndc_y = -eye[1] / (eye[2] * tanf(view->camera.v_half_rad)) + view->camera.proj_y_offset;
	*out_x = view->viewport.x + (ndc_x + 1.0f) * 0.5f * view->viewport.width;
	*out_y = view->viewport.y + (1.0f - ndc_y) * 0.5f * view->viewport.height;
	return 1;
}

/* ---- state-derived object transforms (TIE-style) --------------------
 * The driver derives every object transform from the SIM-STATE channel
 * (flight_objects) so the HD view is not bound to classic render
 * decisions such as widescreen, free camera, and interpolation;
 * validation is the classic view in SPLIT mode. */

/* curMat rows from the mobile object's cached Q15 rows — the fast path
 * of FVIEW_SetObjectTransform: R0 = side, R1 = up, R2 = -fwd. `rows`
 * is the record's side, fwd, up triplet. */
static void fl_curmat_from_cached(const int16_t rows[9], float cur[3][3]) {
	const float q = 1.0f / 32768.0f;
	cur[0][0] = rows[0] * q;
	cur[0][1] = rows[1] * q;
	cur[0][2] = rows[2] * q;
	cur[1][0] = rows[6] * q;
	cur[1][1] = rows[7] * q;
	cur[1][2] = rows[8] * q;
	cur[2][0] = -rows[3] * q;
	cur[2][1] = -rows[4] * q;
	cur[2][2] = -rows[5] * q;
}

#define FL_Q16_TO_RAD (2.0f * 3.14159265358979323846f / 65536.0f)

/* Rodrigues rotation of all three curMat rows about `axis` by a Q16
 * angle — the float mirror of FVIEW_transformaxes (new = M.row with
 * new_x = m00 x + m10 y + m20 z, i.e. M applied transposed). */
static void fl_transformaxes(float cur[3][3], const float axis[3], int angle_q16) {
	if ((int16_t)angle_q16 == 0) {
		return;
	}
	const float a = (float)(int16_t)angle_q16 * FL_Q16_TO_RAD;
	const float c = cosf(a);
	const float sn = sinf(a);
	const float t = 1.0f - c;
	const float x = axis[0], y = axis[1], z = axis[2];
	/* m[i][j] laid out as FVIEW_transformaxes computes m00..m22. */
	const float m[3][3] = {
		{ c + t * x * x, sn * z + t * y * x, -sn * y + t * z * x },
		{ -sn * z + t * y * x, c + t * y * y, sn * x + t * z * y },
		{ sn * y + t * z * x, -sn * x + t * z * y, c + t * z * z },
	};
	for (int r = 0; r < 3; r++) {
		const float ox = cur[r][0], oy = cur[r][1], oz = cur[r][2];
		/* new_x = m00 ox + m10 oy + m20 oz (the engine's application). */
		cur[r][0] = m[0][0] * ox + m[1][0] * oy + m[2][0] * oz;
		cur[r][1] = m[0][1] * ox + m[1][1] * oy + m[2][1] * oz;
		cur[r][2] = m[0][2] * ox + m[1][2] * oy + m[2][2] * oz;
	}
}

/* curMat rows from the record's Q16 Euler angles — the float mirror of
 * FVIEW_calcrotatemove + FVIEW_calcrotateorient (statics and dirty
 * orientations; spin included when the record carries one). */
static void fl_curmat_from_euler(const XwaFlightObject* f, float cur[3][3]) {
	const float aA = (float)(int16_t)(0xc000 - f->pitch) * FL_Q16_TO_RAD;
	const float aB = (float)(int16_t)(-(int16_t)f->yaw) * FL_Q16_TO_RAD;
	const float cB = cosf(aB), sB = sinf(aB);
	const float cA = cosf(aA), sA = sinf(aA);
	cur[0][0] = cB;
	cur[0][1] = sB;
	cur[0][2] = 0.0f;
	cur[2][0] = -sB * cA;
	cur[2][1] = cB * cA;
	cur[2][2] = sA;
	cur[1][0] = -sB * sA;
	cur[1][1] = cB * sA;
	cur[1][2] = -cA;
	fl_transformaxes(cur, cur[1], (int16_t)f->angle_d);
	fl_transformaxes(cur, cur[2], (int16_t)f->roll);
	if (f->spin_angle != 0) {
		fl_transformaxes(cur, f->spin_axis, f->spin_angle);
	}
}

/* Model->world basis rows: the curMat rows in the engine's consumption
 * order (R0, R2, R1) — FVIEW_ComputeObjectViewMatrix's row read, with
 * the camera factor moved into the scene's view matrix. */
static void fl_object_world(const float cur[3][3], float out[9]) {
	const float* src[3] = { cur[0], cur[2], cur[1] };
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			out[i * 3 + j] = src[i][j];
		}
	}
}

/* Model matrix from a model->space basis (rows) + translation, the
 * transpose consumption the model preview validated (Math3D_RotateVec3
 * is column-major over the stored rows). */
static void fl_model_matrix(const float basis[9], const float delta[3], float m[16]) {
	m[0] = basis[0];
	m[1] = basis[3];
	m[2] = basis[6];
	m[3] = delta[0];
	m[4] = basis[1];
	m[5] = basis[4];
	m[6] = basis[7];
	m[7] = delta[1];
	m[8] = basis[2];
	m[9] = basis[5];
	m[10] = basis[8];
	m[11] = delta[2];
	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = 0.0f;
	m[15] = 1.0f;
}

static void fl_object_local(const XwaFlightObject* object, float out[3]) {
	XwaRemasterWorld_LocalI32(s.origin_world, object->world_pos, out);
}

int XwaRemasterFlight_ObjectModelMatrixForCameraDelta(const XwaFlightObject* object,
													  const float camera_minus_object[3], int roll_align,
													  float out[16]) {
	if (!object || !out)
		return 0;
	float cur[3][3];
	if (object->has_mobj && !object->orient_dirty)
		fl_curmat_from_cached(object->rows, cur);
	else
		fl_curmat_from_euler(object, cur);
	if (roll_align && camera_minus_object) {
		const float side_dot = camera_minus_object[0] * cur[0][0] + camera_minus_object[1] * cur[0][1] +
							   camera_minus_object[2] * cur[0][2];
		const float up_dot = camera_minus_object[0] * cur[1][0] + camera_minus_object[1] * cur[1][1] +
							 camera_minus_object[2] * cur[1][2];
		if (side_dot != 0.0f || up_dot != 0.0f) {
			const int dq16 = (int)lrintf(atan2f(up_dot, side_dot) / FL_Q16_TO_RAD) - 0x4000;
			fl_transformaxes(cur, cur[2], dq16);
		}
	}
	float basis[9];
	fl_object_world(cur, basis);
	fl_model_matrix(basis, (const float[3]) { 0.0f, 0.0f, 0.0f }, out);
	return 1;
}

int XwaRemasterFlight_ObjectModelMatrixAtOrigin(const XwaFlightObject* object, const int32_t origin_world[3],
												float out[16]) {
	if (!object || !origin_world || !out)
		return 0;
	float cur[3][3];
	if (object->has_mobj && !object->orient_dirty)
		fl_curmat_from_cached(object->rows, cur);
	else
		fl_curmat_from_euler(object, cur);
	float basis[9], local[3];
	fl_object_world(cur, basis);
	XwaRemasterWorld_LocalI32(origin_world, object->world_pos, local);
	fl_model_matrix(basis, local, out);
	return 1;
}

/* Prev-frame transform for the motion-blur velocity prepass. Matched
 * objects re-derive the PREV snapshot record's basis through the same
 * cached/Euler law as the current one; projectiles keep the CURRENT
 * roll-aligned rotation and take only the prev position (the alignment
 * is view-dependent — the TIE rule). Unmatched objects (spawn frames)
 * stamp zero velocity: camera-consistent "static" treatment would
 * smear a bogus streak. Returns the matched record so rigid and articulated
 * motion use the same history identity. */
static const XwaFlightObject* fl_instance_motion(AeronSceneMeshInstance* inst, uint32_t walk_index,
												 int is_bolt) {
	if (!s.mb_enabled || s.mb_prev_index[walk_index] < 0) {
		inst->zero_velocity = 1;
		return NULL;
	}
	const XwaFlightObject* pf = &s.mb_prev_snap->flight_objects[s.mb_prev_index[walk_index]];
	float prev_local[3];
	XwaRemasterWorld_LocalI32(s.origin_world, pf->world_pos, prev_local);
	if (is_bolt) {
		memcpy(inst->prev_transform, inst->transform, sizeof inst->prev_transform);
		inst->prev_transform[3] = prev_local[0];
		inst->prev_transform[7] = prev_local[1];
		inst->prev_transform[11] = prev_local[2];
		return pf;
	}
	float cur[3][3];
	if (pf->has_mobj && !pf->orient_dirty) {
		fl_curmat_from_cached(pf->rows, cur);
	} else {
		fl_curmat_from_euler(pf, cur);
	}
	float bw[9];
	fl_object_world(cur, bw);
	fl_model_matrix(bw, prev_local, inst->prev_transform);
	return pf;
}

static void fl_world_to_view(const float cam[9], float x, float y, float z, float out[3]);

static const XwaFlightObject* fl_object_by_slot(const XwaSnapshot* snap, int32_t slot) {
	if (!snap || slot < 0) {
		return NULL;
	}
	for (uint32_t i = 0; i < snap->flight_object_count; ++i) {
		if ((int32_t)snap->flight_objects[i].slot == slot) {
			return &snap->flight_objects[i];
		}
	}
	return NULL;
}

static const XwaFlightObject* fl_cockpit_anchor(const XwaSnapshot* snap, const XwaFlightCamera* cam) {
	if (!snap || !cam) {
		return NULL;
	}
	const int32_t slot = cam->in_hangar ? cam->player_obj_idx : cam->focus_obj_idx;
	const XwaFlightObject* anchor = fl_object_by_slot(snap, slot);
	return anchor && anchor->region == cam->region && anchor->genus != XWA_SNAP_GENUS_EXPLOSION ? anchor
																								: NULL;
}

static int fl_cockpit_drawn(const XwaSnapshot* snap, const XwaFlightCamera* cam) {
	if (!snap || !cam || !snap->cockpit_valid || cam->external || cam->film_overlay) {
		return 0;
	}
	if (cam->in_hangar) {
		return 1;
	}
	return cam->cockpit_visible &&
		   (snap->cockpit.seat == 0 ? snap->cockpit.look_available : snap->cockpit.toggle_available);
}

/* If view/turret state changes between snapshots whose simulation time is
 * equal, the velocity buffer must still advance. Otherwise motion blur keeps
 * sampling the last simulated velocity while FSR receives a current pose
 * paired with stale blur timing. */
static int fl_temporal_pose_changed(const XwaSnapshot* current, const XwaSnapshot* previous) {
	const XwaFlightCamera* cam = &current->flight_camera;
	const XwaFlightCamera* prev_cam = &previous->flight_camera;
	if (memcmp(cam->world_pos, prev_cam->world_pos, sizeof cam->world_pos) != 0 ||
		memcmp(cam->rows, prev_cam->rows, sizeof cam->rows) != 0 || cam->proj_scale != prev_cam->proj_scale ||
		cam->vp_w != prev_cam->vp_w || cam->vp_h != prev_cam->vp_h ||
		cam->vp_center_x != prev_cam->vp_center_x || cam->vp_center_y != prev_cam->vp_center_y ||
		cam->proj_offset_y != prev_cam->proj_offset_y) {
		return 1;
	}
	if (current->cockpit_valid != previous->cockpit_valid || !current->cockpit_valid) {
		return current->cockpit_valid != previous->cockpit_valid;
	}
	const XwaCockpit* cockpit = &current->cockpit;
	const XwaCockpit* prev_cockpit = &previous->cockpit;
	return cockpit->seat != prev_cockpit->seat || cockpit->aim_angle_a != prev_cockpit->aim_angle_a ||
		   cockpit->aim_angle_b != prev_cockpit->aim_angle_b ||
		   memcmp(cockpit->hardpoint_world, prev_cockpit->hardpoint_world, sizeof cockpit->hardpoint_world) !=
			   0 ||
		   memcmp(cockpit->camera_pan, prev_cockpit->camera_pan, sizeof cockpit->camera_pan) != 0;
}

static int fl_cockpit_model_matrix(const XwaCockpit* cockpit, const XwaFlightObject* anchor,
								   const float camera_rows[9], const float camera_local[3], float out[16]) {
	if (!cockpit || !anchor || !camera_rows || !camera_local || !out) {
		return 0;
	}
	float cur[3][3];
	if (anchor->has_mobj && !anchor->orient_dirty) {
		fl_curmat_from_cached(anchor->rows, cur);
	} else {
		fl_curmat_from_euler(anchor, cur);
	}
	float basis[9];
	fl_object_world(cur, basis);

	float eye_offset[3];
	for (int axis = 0; axis < 3; ++axis) {
		eye_offset[axis] = -(cockpit->hardpoint_world[axis] + cockpit->camera_pan[axis] * 0.0625f);
	}
	float delta[3];
	fl_world_to_view(camera_rows, eye_offset[0], eye_offset[1], eye_offset[2], delta);
	if (cockpit->seat == 2) {
		/* Classic gunner rear-turret flip about the hardpoint. The flip acts
		 * in eye space, so move the anchor basis there and back. */
		float eye_basis[9];
		for (int row = 0; row < 3; ++row) {
			for (int col = 0; col < 3; ++col) {
				eye_basis[row * 3 + col] = basis[row * 3 + 0] * camera_rows[col * 3 + 0] +
										   basis[row * 3 + 1] * camera_rows[col * 3 + 1] +
										   basis[row * 3 + 2] * camera_rows[col * 3 + 2];
			}
		}
		float flipped[3];
		for (int col = 0; col < 3; ++col) {
			flipped[col] =
				eye_basis[col] * delta[0] + eye_basis[3 + col] * delta[1] + eye_basis[6 + col] * delta[2];
		}
		flipped[1] = -flipped[1];
		flipped[2] = -flipped[2];
		for (int row = 0; row < 3; ++row) {
			delta[row] = eye_basis[row * 3 + 0] * flipped[0] + eye_basis[row * 3 + 1] * flipped[1] +
						 eye_basis[row * 3 + 2] * flipped[2];
			eye_basis[row * 3 + 1] = -eye_basis[row * 3 + 1];
			eye_basis[row * 3 + 2] = -eye_basis[row * 3 + 2];
		}
		for (int row = 0; row < 3; ++row) {
			for (int col = 0; col < 3; ++col) {
				basis[row * 3 + col] = eye_basis[row * 3 + 0] * camera_rows[0 * 3 + col] +
									   eye_basis[row * 3 + 1] * camera_rows[1 * 3 + col] +
									   eye_basis[row * 3 + 2] * camera_rows[2 * 3 + col];
			}
		}
	}

	float position[3];
	for (int axis = 0; axis < 3; ++axis) {
		position[axis] = camera_local[axis] + camera_rows[0 * 3 + axis] * delta[0] +
						 camera_rows[1 * 3 + axis] * delta[1] + camera_rows[2 * 3 + axis] * delta[2];
	}
	fl_model_matrix(basis, position, out);
	return 1;
}

/* The classic default quad texcoords (g_defaultQuadTexCoords) in the
 * builders' corner order — used where no per-quad UVs are captured
 * (billboards). */
static const float fl_default_uvs[4][2] = { { 1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 } };

/* Queue one textured quad built in VIEW space (4 corners + per-corner
 * texcoords, remapped into the resolved atlas frame's sub-rect) as a
 * batched scene billboard; tint is raw engine 0xAARRGGBB. Corners
 * convert to render-local space through the camera (the scene's view matrix
 * reproduces the view positions exactly — crows is derived from the
 * same quaternion). `anchor_local` is the LENS-stage occlusion source
 * (NULL for the depth-tested stages). */
static void fl_submit_view_quad_ex(AeronSceneBillboardStage stage, const float corners[4][3],
								   const float uvs[4][2], const XwaAssetRef* ref, uint32_t argb,
								   const float* anchor_local, float depth_bias,
								   const float (*prev_corners)[3]) {
	AeronSceneBillboardDesc d;
	memset(&d, 0, sizeof d);
	d.texture = ref->texture;
	/* Baked KTX2 atlases are PREMULTIPLIED (the imgbake convention);
	 * the PMA blend keeps single-alpha semantics — straight-alpha
	 * blending of PMA texels applies alpha twice and crushed every
	 * soft-alpha sprite (flare ghosts, glow rims). */
	d.blend = AERON_SCENE_BILLBOARD_BLEND_PMA;
	d.stage = stage;
	d.depth_bias_view = depth_bias;
	if (anchor_local) {
		memcpy(d.anchor_world, anchor_local, sizeof d.anchor_world);
	}
	float tint[4];
	tint[3] = (float)((argb >> 24) & 0xffu) / 255.0f;
	/* PMA modulate: the tint's alpha scales the premultiplied RGB too
	 * (equivalent to the classic straight-alpha vertex modulate). */
	tint[0] = XwaRemaster_SrgbToLinear((float)((argb >> 16) & 0xffu) / 255.0f) * tint[3];
	tint[1] = XwaRemaster_SrgbToLinear((float)((argb >> 8) & 0xffu) / 255.0f) * tint[3];
	tint[2] = XwaRemaster_SrgbToLinear((float)(argb & 0xffu) / 255.0f) * tint[3];
	const float du = ref->u1 - ref->u0;
	const float dv = ref->v1 - ref->v0;
	for (int v = 0; v < 4; v++) {
		/* local = camera_local + crows^T * view */
		for (int j = 0; j < 3; j++) {
			d.corners[v][j] = s.camera_local[j] + s.crows[0 * 3 + j] * corners[v][0] +
							  s.crows[1 * 3 + j] * corners[v][1] + s.crows[2 * 3 + j] * corners[v][2];
		}
		d.uv[v][0] = ref->u0 + uvs[v][0] * du;
		d.uv[v][1] = ref->v0 + uvs[v][1] * dv;
		memcpy(d.colors[v], tint, sizeof tint);
	}
	if (prev_corners)
		d.prev_corners = prev_corners; /* copied by AddBillboard */
	AeronScene_AddBillboard(s.scene, &d);
}

static void fl_submit_view_quad(AeronSceneBillboardStage stage, const float corners[4][3],
								const float uvs[4][2], const XwaAssetRef* ref, uint32_t argb,
								const float* anchor_local) {
	fl_submit_view_quad_ex(stage, corners, uvs, ref, argb, anchor_local, 0.0f, NULL);
}

/* Resolve a model-type-bound DAT frame from the baked resdata group
 * atlases, per the classic binding kind: group-frame types select the
 * frame's POSITION in group order (FeDiskIo_SelectTextureFrame,
 * 1-based; frame <= 0 selects nothing); single-sprite types bind one
 * fixed DAT id (the draw's frame does not pick the image). */
static int fl_resolve_type_frame(XwaRemasterAssets* assets, int object_type, int frame, XwaAssetRef* out) {
	return XwaRemasterAssets_FlightModelFrame(assets, object_type, frame, out);
}

/* ---- backdrops (state-derived) --------------------------------------
 * Derived per frame from the backdrops STATE channel + the camera,
 * replicating the classic hardware path (Backdrop_RenderCurrentRegion):
 * flags&2 and sides 0-3 render the mission-static world-space
 * coordinate strip; sides 4/5 render one axis quad. Backdrops rotate
 * with the camera only (directions at infinity — no translation).
 *
 * Extents use the atlas frames' classic dims — the classic level-0
 * texture dims at MAX detail. (The classic engine shrinks these levels
 * when the explosion-detail config is lowered or a record's
 * angular_scale is tiny (FeDiskIo_SelectTextureFrame mip walk); the HD
 * render always derives from full-detail dims by design.) */

/* World -> view by the camera rows (rotation only). */
static void fl_world_to_view(const float cam[9], float x, float y, float z, float out[3]) {
	for (int r = 0; r < 3; r++) {
		out[r] = cam[r * 3 + 0] * x + cam[r * 3 + 1] * y + cam[r * 3 + 2] * z;
	}
}

static uint32_t fl_derive_backdrops(const XwaSnapshot* snap, const float crows[9],
									XwaRemasterAssets* assets) {
	uint32_t count = 0;
	for (uint32_t i = 0; i < snap->backdrop_count; i++) {
		const XwaBackdrop* b = &snap->backdrops[i];
		if (b->hidden) {
			continue;
		}
		int frame = (b->flags & 1u) ? b->frame : 1;
		XwaAssetRef ref;
		if ((b->flags & 2u) != 0 || b->side <= 3) {
			/* Coordinate strip (Backdrop_DrawCoordinateStrip): segment
			 * quads between consecutive world strip coords, extruded
			 * +/- half height on world Z; U slides across each frame's
			 * segments, the frame advancing every segments-per-frame
			 * with the half height rescaled by the frame-height ratio. */
			if (b->strip_segment_count == 0 || b->strip_segments_per_frame == 0) {
				continue;
			}
			if (!fl_resolve_type_frame(assets, b->model_type, frame, &ref)) {
				continue;
			}
			int32_t hh = b->strip_half_height;
			int prev_h = ref.classic_h;
			int seg_in_frame = 1;
			for (int seg = 1; seg <= b->strip_segment_count && count < FL_MAX_BACKDROP_QUADS; seg++) {
				float u0;
				if (seg > 1) {
					seg_in_frame++;
					if (seg_in_frame > b->strip_segments_per_frame) {
						seg_in_frame -= b->strip_segments_per_frame;
						frame++;
						if (!fl_resolve_type_frame(assets, b->model_type, frame, &ref)) {
							break;
						}
						if (ref.classic_h > 0 && prev_h > 0 && ref.classic_h != prev_h) {
							hh = ref.classic_h * hh / prev_h; /* classic int math */
							prev_h = ref.classic_h;
						}
						u0 = 0.0f;
					} else {
						u0 = (float)(seg_in_frame - 1) / (float)b->strip_segments_per_frame;
					}
				} else {
					u0 = 0.0f;
				}
				const float u1 = (float)seg_in_frame / (float)b->strip_segments_per_frame;
				FlBackdropQuad* q = &s.bd_quads[count++];
				const float* c0 = b->strip_coords[seg - 1];
				const float* c1 = b->strip_coords[seg];
				fl_world_to_view(crows, c0[0], c0[1], c0[2] + (float)hh, q->corners[0]);
				fl_world_to_view(crows, c1[0], c1[1], c1[2] + (float)hh, q->corners[1]);
				fl_world_to_view(crows, c1[0], c1[1], c1[2] - (float)hh, q->corners[2]);
				fl_world_to_view(crows, c0[0], c0[1], c0[2] - (float)hh, q->corners[3]);
				q->uvs[0][0] = u0;
				q->uvs[0][1] = 0.0f;
				q->uvs[1][0] = u1;
				q->uvs[1][1] = 0.0f;
				q->uvs[2][0] = u1;
				q->uvs[2][1] = 1.0f;
				q->uvs[3][0] = u0;
				q->uvs[3][1] = 1.0f;
				q->ref = ref;
			}
		} else if (b->side == 4 || b->side == 5) {
			/* Axis quad: half width along world Y, half height along
			 * world X, both rotated by the camera; extent =
			 * angular_scale * (classic_dim * 1.5) / 256 around the
			 * direction / 512. Visibility gates on the camera forward
			 * row's Z sign (side 4 faces one world pole, 5 the other). */
			const float fwd_z = crows[8];
			if ((b->side == 4 && !(fwd_z > 0.0f)) || (b->side == 5 && !(fwd_z < 0.0f))) {
				continue;
			}
			if (count >= FL_MAX_BACKDROP_QUADS ||
				!fl_resolve_type_frame(assets, b->model_type, frame, &ref) || ref.classic_w <= 0 ||
				ref.classic_h <= 0) {
				continue;
			}
			const int ext_w = ((int)b->angular_scale * (ref.classic_w + (ref.classic_w >> 1))) >> 8;
			const int ext_h = ((int)b->angular_scale * (ref.classic_h + (ref.classic_h >> 1))) >> 8;
			float v[3];
			fl_world_to_view(crows, b->world_dir[0], b->world_dir[1], b->world_dir[2], v);
			for (int a = 0; a < 3; a++) {
				v[a] = (float)((int)v[a] >> 9); /* classic >>9 on the int view dir */
			}
			FlBackdropQuad* q = &s.bd_quads[count++];
			for (int a = 0; a < 3; a++) {
				const float h = crows[a * 3 + 0] * (float)ext_h; /* cam column X */
				const float w = crows[a * 3 + 1] * (float)ext_w; /* cam column Y */
				q->corners[0][a] = v[a] + h + w;
				q->corners[1][a] = v[a] + h - w;
				q->corners[2][a] = v[a] - h - w;
				q->corners[3][a] = v[a] - h + w;
			}
			memcpy(q->uvs, fl_default_uvs, sizeof q->uvs);
			q->ref = ref;
		}
	}
	return count;
}

/* ---- scene billboards (state-derived) --------------------------------
 * Float mirrors of the classic billboard pipeline, driven entirely by
 * flight-object state (no draw capture):
 *   dispatch — the render walk's slot/genus routing (flight_view.c):
 *     MAIN slots billboard genus Explosion/Debris and flame genus
 *     SalvageJunk; TRANSIENT slots billboard any genus (chaff, sparks)
 *     with angleD forced 0, hiding GENUS_Debris in external view.
 *     STATIC-genus slots are intentionally NOT derived: the classic
 *     RenderNonCraftSceneObject passes the object TYPE as the queue's
 *     object-index field (original bug, verified at 0x4E40B0), so its
 *     queue entries alias whatever object occupies that low slot —
 *     garbage-dependent output not worth reproducing.
 *   geometry — SceneBillboard_QueueObjectTextured + flush laws: base
 *     size (instanceExtent<<8)/maxBounds (<<3 unless debris sprites;
 *     0xffff for SSD type-2006), projected size maxBounds*base/z
 *     clamped to 1024 (unclamped for type 2006 in the Death Star
 *     hangar), half extents (dim * projected) >> 9 px through the
 *     classic focal; type 2006 pulls the DEPTH in by typeSpecificWord
 *     while the screen position keeps the true-depth projection.
 *   Intentional deviations: no 32-entry queue cap, no int16 screen-
 *     projection rejects (free FOV renders what the classic screen-
 *     culled), continuous depth (no z>>8 quantization). */

/* The classic billboard roll law: of the object-view rows R0 (object
 * right) and R1 (object -forward — the (R0, R2, R1) row order), take
 * the one more parallel to the view plane and fold its eye-space x/y
 * through trig2_arctan's sign handling. Transient slots re-transform
 * with angleD forced 0 (the walk's FVIEW_SetObjectTransform call). */
typedef struct FlBillboardGeometry {
	float center[3];
	float half_w;
	float half_h;
	float rot;
	float depth_bias;
} FlBillboardGeometry;

uint16_t XwaRemasterFlight_ClassicBillboardBaseSize(int32_t instance_extent, int object_type,
													int max_bounds) {
	if (max_bounds <= 0) {
		return 0;
	}

	/* Classic arithmetic is unsigned 32-bit here.  The result is then
	 * assigned to SceneBillboardQueueEntry::screenSize (int16_t) and
	 * reloaded as uint16_t by the flush.  Preserve that late narrowing;
	 * narrowing instanceExtent before this calculation is not equivalent. */
	uint32_t base = ((uint32_t)instance_extent << 8) / (uint32_t)max_bounds;
	if (object_type < XWA_SNAP_TYPE_DEBRIS_SPRITE_0 || object_type > XWA_SNAP_TYPE_DEBRIS_SPRITE_3) {
		base <<= 3;
	}
	return (uint16_t)base;
}

static float fl_billboard_roll(const XwaFlightObject* f, int force_zero_angle_d,
							   const float world_to_view[9]) {
	float cur[3][3];
	if (force_zero_angle_d && f->angle_d != 0) {
		XwaFlightObject tmp = *f;
		tmp.angle_d = 0;
		fl_curmat_from_euler(&tmp, cur);
	} else if (f->has_mobj && !f->orient_dirty) {
		fl_curmat_from_cached(f->rows, cur);
	} else {
		fl_curmat_from_euler(f, cur);
	}
	float a0[3], a1[3];
	fl_world_to_view(world_to_view, cur[0][0], cur[0][1], cur[0][2], a0);
	fl_world_to_view(world_to_view, cur[2][0], cur[2][1], cur[2][2], a1);
	const float* ax = fabsf(a0[2]) < fabsf(a1[2]) ? a0 : a1;
	return ax[0] < 0.0f ? atan2f(ax[1], -ax[0]) : -atan2f(ax[1], ax[0]);
}

static void fl_bb_geometry_corners_local(const FlBillboardGeometry* g, const XwaFlightCamera* cam,
										 const float world_to_view[9], float out[4][3]) {
	const float ca = cosf(g->rot);
	const float sa = sinf(g->rot);
	const float ax[2] = { g->half_w * ca, -g->half_w * sa };
	const float ay[2] = { g->half_h * sa, g->half_h * ca };
	float camera_local[3];
	XwaRemasterWorld_LocalI32(s.origin_world, cam->world_pos, camera_local);
	for (int c = 0; c < 4; c++) {
		const float sw = (c == 0 || c == 3) ? 1.0f : -1.0f;
		const float sh = c <= 1 ? 1.0f : -1.0f;
		const float view[3] = { g->center[0] + sw * ax[0] + sh * ay[0],
								g->center[1] + sw * ax[1] + sh * ay[1], g->center[2] };
		for (int j = 0; j < 3; j++)
			out[c][j] = camera_local[j] + world_to_view[0 * 3 + j] * view[0] +
						world_to_view[1 * 3 + j] * view[1] + world_to_view[2 * 3 + j] * view[2];
	}
}

static void fl_bb_push(const FlBillboardGeometry* g, const XwaAssetRef* ref, uint32_t argb,
					   const float (*prev_corners)[3]) {
	if (s.bb_count >= FL_MAX_BILLBOARDS) {
		return;
	}
	FlBillboard* b = &s.bb[s.bb_count++];
	memcpy(b->center, g->center, sizeof b->center);
	b->half_w = g->half_w;
	b->half_h = g->half_h;
	b->rot = g->rot;
	b->depth_bias = g->depth_bias;
	b->ref = *ref;
	b->argb = argb;
	b->has_prev = prev_corners != NULL;
	if (b->has_prev)
		memcpy(b->prev_corners, prev_corners, sizeof b->prev_corners);
}

static const XwaFlightObject* fl_bb_previous_object(uint32_t snap_index) {
	if (!s.mb_enabled || s.mb_prev_index[snap_index] < 0)
		return NULL;
	return &s.mb_prev_snap->flight_objects[s.mb_prev_index[snap_index]];
}

static int fl_object_billboard_candidate(const XwaFlightObject* f, const XwaFlightCamera* cam) {
	if (f->slot_class == XWA_SNAP_SLOT_TRANSIENT)
		return f->genus != XWA_SNAP_GENUS_DEBRIS || !cam->external;
	return f->slot_class == XWA_SNAP_SLOT_MAIN &&
		   (f->genus == XWA_SNAP_GENUS_EXPLOSION || f->genus == XWA_SNAP_GENUS_DEBRIS);
}

static int fl_object_billboard_geometry(const XwaFlightObject* f, const XwaFlightCamera* cam,
										const float world_to_view[9], FlBillboardGeometry* out) {
	const int type = f->object_type;
	if (type == XWA_SNAP_TYPE_DEBRIS_CHUNK)
		return 0; /* rendered through the source-craft mesh path */
	const int frame = f->type_specific_0;
	if (frame == 0)
		return 0; /* classic: frame 0 = not drawn */
	int w = 0, h = 0, maxb = 0;
	if (!XwaSnapshotExport_ModelTexFrame(type, frame, &w, &h, &maxb) || maxb <= 0)
		return 0; /* classic queue gate: tex levels loaded + frame in range */

	float delta_world[3];
	XwaRemasterWorld_DeltaI32(f->world_pos, cam->world_pos, delta_world);
	float v[3];
	fl_world_to_view(world_to_view, delta_world[0], delta_world[1], delta_world[2], v);
	if (v[2] < 1.0f)
		return 0;
	float z = v[2];
	if (type == XWA_SNAP_TYPE_EXPLOSION_2006) {
		z -= (float)f->type_specific_w;
		if (z <= 0.0f)
			z = 1.0f;
	}
	uint16_t base;
	if (f->source_object_type == XWA_SNAP_TYPE_SSD && type == XWA_SNAP_TYPE_EXPLOSION_2006) {
		base = 0xffffu;
	} else {
		base = XwaRemasterFlight_ClassicBillboardBaseSize(f->instance_extent, type, maxb);
		if (base == 0)
			return 0;
	}
	float proj = (float)maxb * (float)base / z;
	if (!(cam->ds_hangar && type == XWA_SNAP_TYPE_EXPLOSION_2006) && proj > 1024.0f)
		proj = 1024.0f;
	const float ps = cam->proj_scale > 0.0f ? cam->proj_scale : 512.0f;
	const float px2vw = z / ps;
	out->half_w = (float)w * proj * (1.0f / 512.0f) * px2vw;
	out->half_h = (float)h * proj * (1.0f / 512.0f) * px2vw;
	const float zs = z / v[2];
	out->center[0] = v[0] * zs;
	out->center[1] = v[1] * zs;
	out->center[2] = z;
	out->rot = fl_billboard_roll(f, f->slot_class == XWA_SNAP_SLOT_TRANSIENT, world_to_view);
	out->depth_bias = 0.0f;
	if (type >= XWA_SNAP_TYPE_EXPLOSION_2000 && type < XWA_SNAP_TYPE_EXPLOSION_2006) {
		const float pull = (float)((w > h ? w : h) << 8);
		if (pull < z)
			out->depth_bias = pull;
	}
	return 1;
}

/* One object sprite (SceneBillboard_QueueObjectTextured mirror). */
static void fl_derive_object_billboard(const XwaFlightObject* f, uint32_t snap_index,
									   XwaRemasterAssets* assets, const XwaFlightCamera* history_cam,
									   const float history_world_to_view[9]) {
	const XwaFlightCamera* cam = &s.snap->flight_camera;
	FlBillboardGeometry geometry;
	if (!fl_object_billboard_geometry(f, cam, s.crows, &geometry))
		return;
	XwaAssetRef ref;
	if (!fl_resolve_type_frame(assets, f->object_type, f->type_specific_0, &ref))
		return;
	float previous_corners[4][3];
	const float (*previous_corners_ptr)[3] = NULL;
	const XwaFlightObject* pf = fl_bb_previous_object(snap_index);
	if (pf && pf->object_type == f->object_type && history_cam && history_world_to_view &&
		fl_object_billboard_candidate(pf, history_cam)) {
		FlBillboardGeometry previous_geometry;
		if (fl_object_billboard_geometry(pf, history_cam, history_world_to_view, &previous_geometry)) {
			fl_bb_geometry_corners_local(&previous_geometry, history_cam, history_world_to_view,
										 previous_corners);
			previous_corners_ptr = previous_corners;
		}
	}
	/* Classic billboards draw with color arg -1 = opaque white (the
	 * texture as authored; the level argbColor is draw scratch). */
	fl_bb_push(&geometry, &ref, 0xffffffffu, previous_corners_ptr);
}

static int fl_wreck_flame_base_geometry(const XwaFlightObject* f, const XwaFlightCamera* cam,
										const float world_to_view[9], FlBillboardGeometry* out,
										uint8_t mesh_types[XWA_SNAP_MAX_MESH_SLOTS], int* mesh_count) {
	if (!f->has_craft || f->component_state[49] == 0)
		return 0;
	*mesh_count = XwaSnapshotExport_ModelMeshTypes(f->object_type, mesh_types);
	if (*mesh_count == 0)
		return 0;
	int w = 0, h = 0, maxb = 0;
	if (!XwaSnapshotExport_ModelTexFrame(XWA_SNAP_TYPE_FLAME_2008, f->component_state[49], &w, &h, &maxb) ||
		maxb <= 0) {
		return 0;
	}
	float delta_world[3];
	XwaRemasterWorld_DeltaI32(f->world_pos, cam->world_pos, delta_world);
	fl_world_to_view(world_to_view, delta_world[0], delta_world[1], delta_world[2], out->center);
	if (out->center[2] < 1.0f)
		return 0;
	float proj = (float)maxb * 256.0f / out->center[2];
	if (proj > 1024.0f)
		proj = 1024.0f;
	const float ps = cam->proj_scale > 0.0f ? cam->proj_scale : 512.0f;
	const float px2vw = out->center[2] / ps;
	out->half_w = (float)w * proj * (1.0f / 512.0f) * px2vw;
	out->half_h = (float)h * proj * (1.0f / 512.0f) * px2vw;
	out->rot = fl_billboard_roll(f, 0, world_to_view);
	out->depth_bias = 0.0f;
	return 1;
}

/* Wreck flames (Damage_QueueCraftBillboards mirror): one type-2008
 * sprite per destroyed MESH_Fuselage slot, all at the object center,
 * the object's roll accumulating onto the billboard angle per flame,
 * frame from componentState[49], base size 256. */
static void fl_derive_wreck_flames(const XwaFlightObject* f, uint32_t snap_index, XwaRemasterAssets* assets,
								   const XwaFlightCamera* history_cam, const float history_world_to_view[9]) {
	const XwaFlightCamera* cam = &s.snap->flight_camera;
	uint8_t mesh_types[XWA_SNAP_MAX_MESH_SLOTS];
	int mesh_count = 0;
	FlBillboardGeometry base_geometry;
	if (!fl_wreck_flame_base_geometry(f, cam, s.crows, &base_geometry, mesh_types, &mesh_count))
		return;
	XwaAssetRef ref;
	if (!fl_resolve_type_frame(assets, XWA_SNAP_TYPE_FLAME_2008, f->component_state[49], &ref))
		return;

	FlBillboardGeometry previous_base;
	uint8_t previous_mesh_types[XWA_SNAP_MAX_MESH_SLOTS];
	float previous_rot[XWA_SNAP_MAX_MESH_SLOTS];
	uint8_t previous_flame[XWA_SNAP_MAX_MESH_SLOTS] = { 0 };
	int previous_mesh_count = 0;
	const XwaFlightObject* pf = fl_bb_previous_object(snap_index);
	const int have_previous_base =
		pf && pf->object_type == f->object_type && history_cam && history_world_to_view &&
		pf->slot_class == XWA_SNAP_SLOT_MAIN && pf->genus == XWA_SNAP_GENUS_SALVAGE_JUNK &&
		fl_wreck_flame_base_geometry(pf, history_cam, history_world_to_view, &previous_base,
									 previous_mesh_types, &previous_mesh_count);
	if (have_previous_base) {
		float rot = previous_base.rot;
		const float roll_rad = (float)(int16_t)pf->roll * FL_Q16_TO_RAD;
		for (int mesh = 0; mesh < previous_mesh_count; mesh++) {
			if (previous_mesh_types[mesh] == XWA_SNAP_MESH_FUSELAGE && pf->component_state[mesh] == 0) {
				rot += roll_rad;
				previous_rot[mesh] = rot;
				previous_flame[mesh] = 1;
			}
		}
	}

	const float roll_rad = (float)(int16_t)f->roll * FL_Q16_TO_RAD;
	float rot = base_geometry.rot;
	for (int mesh = 0; mesh < mesh_count; mesh++) {
		if (mesh_types[mesh] != XWA_SNAP_MESH_FUSELAGE || f->component_state[mesh] != 0) {
			continue;
		}
		rot += roll_rad; /* classic: billboardAngle += roll per queued flame */
		FlBillboardGeometry geometry = base_geometry;
		geometry.rot = rot;
		float previous_corners[4][3];
		const float (*previous_corners_ptr)[3] = NULL;
		if (have_previous_base && mesh < previous_mesh_count && previous_flame[mesh]) {
			FlBillboardGeometry previous_geometry = previous_base;
			previous_geometry.rot = previous_rot[mesh];
			fl_bb_geometry_corners_local(&previous_geometry, history_cam, history_world_to_view,
										 previous_corners);
			previous_corners_ptr = previous_corners;
		}
		fl_bb_push(&geometry, &ref, 0xffffffffu, previous_corners_ptr);
	}
}

static int fl_bb_depth_cmp(const void* pa, const void* pb) {
	const float za = ((const FlBillboard*)pa)->center[2];
	const float zb = ((const FlBillboard*)pb)->center[2];
	return za < zb ? 1 : (za > zb ? -1 : 0);
}

static void fl_derive_billboards(const XwaSnapshot* snap, XwaRemasterAssets* assets,
								 const XwaFlightCamera* history_cam, const float history_world_to_view[9]) {
	s.bb_count = 0;
	const XwaFlightCamera* cam = &snap->flight_camera;
	/* slot_class was captured from the active main + local transient
	 * ranges that the classic walks.  Do not add a region-byte gate:
	 * several dynamic effect spawners leave ObjectRecord.regionIdx stale. */
	for (uint32_t i = 0; i < snap->flight_object_count; i++) {
		const XwaFlightObject* f = &snap->flight_objects[i];
		if (f->slot_class == XWA_SNAP_SLOT_TRANSIENT) {
			if (f->genus == XWA_SNAP_GENUS_DEBRIS && cam->external) {
				continue; /* classic hides transient debris externally */
			}
			fl_derive_object_billboard(f, i, assets, history_cam, history_world_to_view);
		} else if (f->slot_class == XWA_SNAP_SLOT_MAIN) {
			if (f->genus == XWA_SNAP_GENUS_EXPLOSION || f->genus == XWA_SNAP_GENUS_DEBRIS) {
				fl_derive_object_billboard(f, i, assets, history_cam, history_world_to_view);
			} else if (f->genus == XWA_SNAP_GENUS_SALVAGE_JUNK) {
				fl_derive_wreck_flames(f, i, assets, history_cam, history_world_to_view);
			}
		}
	}
	/* Classic flush order: back-to-front across the whole set. */
	qsort(s.bb, s.bb_count, sizeof s.bb[0], fl_bb_depth_cmp);
}

/* ---- lens flares (state-derived) --------------------------------------
 * Float mirrors of the classic lens-flare pipeline (lens_flare.c):
 * sources are backdrop SUNS (types 521-530, in front of the camera)
 * and EXPLOSION FLASHES (types 2001-2006 while frameCount - 4 - frame
 * is within [0, 5] and viewZ < 0x4000, main-region slots — the render
 * walk's case 13); the classic caps 4 sources and requires the source
 * ON SCREEN. Each source draws a 7-quad train along the source <->
 * screen-center line (LensFlare_QueueSource positions, ring sizes
 * {256,160,128,96,256,256,96} x projScale/512, LightingEffects group
 * 1000 frames {3,4,5,4,7,8,7}), rings interleaved across sources in
 * the classic flush order, on top of everything.
 *
 * The classic CPU swept-collision visibility ray (incl. the cockpit-
 * model test) is replaced by the LENS stage's GPU depth-sample
 * occlusion at the source anchor — per-pixel scene geometry, cockpit
 * included, soft-faded instead of the classic binary pop (documented
 * deviation). The classic lensFlare config option is NOT honored (a
 * period toggle — the HD renderer always draws flares); the on-screen
 * gate uses the HD frame. */

static const int fl_lens_frame[FL_LENS_RINGS] = { 3, 4, 5, 4, 7, 8, 7 };
static const float fl_lens_size[FL_LENS_RINGS] = { 256.0f, 160.0f, 128.0f, 96.0f, 256.0f, 256.0f, 96.0f };
/* flareSpriteOrColor (renderer.c @ 0x5FF538): faint white overrides
 * for the explosion-flash window, indexed by flare frame. */
static const uint32_t fl_lens_flash_argb[6] = { 0x08ffffffu, 0x16ffffffu, 0x32ffffffu,
												0x32ffffffu, 0x16ffffffu, 0x16ffffffu };

static void fl_lens_add_source(const XwaRemasterFlightView* flight_view, const float anchor_local[3],
							   uint32_t argb) {
	if (s.lens_src_count >= FL_MAX_LENS_SOURCES) {
		return; /* classic queue capacity */
	}
	float sx, sy;
	if (!XwaRemasterFlight_ProjectLocal(flight_view, anchor_local, &sx, &sy, NULL)) {
		return;
	}
	const AeronRectI viewport = flight_view->viewport;
	if (sx < viewport.x || sx >= viewport.x + viewport.width || sy < viewport.y ||
		sy >= viewport.y + viewport.height) {
		return;
	}
	FlLensSource* src = &s.lens_src[s.lens_src_count++];
	memcpy(src->anchor_local, anchor_local, sizeof src->anchor_local);
	src->sx = sx;
	src->sy = sy;
	src->argb = argb;
}

static void fl_derive_lens_flares(const XwaSnapshot* snap, XwaRemasterAssets* assets,
								  const XwaRemasterFlightView* flight_view) {
	s.lens_src_count = 0;
	s.lens_ok = 0;
	const XwaFlightCamera* cam = &snap->flight_camera;

	/* Backdrop suns: direction sources at infinity (raw Q20 world
	 * dirs; projection is magnitude-independent, the anchor's large
	 * magnitude lands the depth reference at the far plane). */
	for (uint32_t i = 0; i < snap->backdrop_count && s.lens_src_count < FL_MAX_LENS_SOURCES; i++) {
		const XwaBackdrop* b = &snap->backdrops[i];
		if (b->model_type < XWA_SNAP_TYPE_BACKDROP_SUN_FIRST ||
			b->model_type > XWA_SNAP_TYPE_BACKDROP_SUN_LAST) {
			continue;
		}
		float v[3];
		fl_world_to_view(s.crows, b->world_dir[0], b->world_dir[1], b->world_dir[2], v);
		if (v[2] <= 0.0f) {
			continue; /* classic viewDir z > 0 gate */
		}
		const float anchor[3] = { s.camera_local[0] + b->world_dir[0], s.camera_local[1] + b->world_dir[1],
								  s.camera_local[2] + b->world_dir[2] };
		/* Classic passes -1: DrawModelTexture stomps the level tint
		 * with it -> opaque white (the sprite as authored). */
		fl_lens_add_source(flight_view, anchor, 0xffffffffu);
	}

	/* Explosion flashes: the render walk's case-13 window (active MAIN
	 * slots only — transient sparks never queue flares).  MAIN already
	 * encodes the current region; ObjectRecord.regionIdx is not a gate. */
	for (uint32_t i = 0; i < snap->flight_object_count && s.lens_src_count < FL_MAX_LENS_SOURCES; i++) {
		const XwaFlightObject* f = &snap->flight_objects[i];
		if (f->slot_class != XWA_SNAP_SLOT_MAIN || f->genus != XWA_SNAP_GENUS_EXPLOSION) {
			continue;
		}
		if (f->object_type <= XWA_SNAP_TYPE_EXPLOSION_2000 || f->object_type > XWA_SNAP_TYPE_EXPLOSION_2006) {
			continue;
		}
		const int flare_frame =
			XwaSnapshotExport_ModelFrameCount(f->object_type) - 4 - (int)f->type_specific_0;
		if (flare_frame < 0 || flare_frame > 5) {
			continue;
		}
		float v[3];
		float delta_world[3];
		XwaRemasterWorld_DeltaI32(f->world_pos, cam->world_pos, delta_world);
		fl_world_to_view(s.crows, delta_world[0], delta_world[1], delta_world[2], v);
		if (v[2] <= 0.0f || v[2] >= 16384.0f) {
			continue; /* classic viewZ < 0x4000 */
		}
		float local[3];
		fl_object_local(f, local);
		fl_lens_add_source(flight_view, local, fl_lens_flash_argb[flare_frame]);
	}

	if (s.lens_src_count == 0) {
		return;
	}
	/* Ring sprite frames + classic dims (atlas uploads before the
	 * scene passes; any missing frame disables the effect). */
	for (int r = 0; r < FL_LENS_RINGS; r++) {
		int w = 0, h = 0, maxb = 0;
		if (!XwaSnapshotExport_ModelTexFrame(XWA_SNAP_TYPE_LIGHTING_1000, fl_lens_frame[r], &w, &h, &maxb) ||
			!XwaRemasterAssets_FlightAtlasFrame(assets, 1000, fl_lens_frame[r] - 1, &s.lens_ref[r])) {
			s.lens_src_count = 0;
			return;
		}
		s.lens_dim[r][0] = w;
		s.lens_dim[r][1] = h;
	}
	s.lens_ok = 1;
}

/* Submit the flare trains as LENS billboards, ring-major across the
 * sources (the classic flush order). Quads sit at a fixed shallow
 * view depth — the LENS pass has no depth test; occlusion comes from
 * the anchor's depth-buffer visibility. */
static void fl_submit_lens_flares(const XwaSnapshot* snap, const XwaRemasterFlightView* flight_view) {
	if (!s.lens_ok || s.lens_src_count == 0) {
		return;
	}
	const XwaFlightCamera* cam = &snap->flight_camera;
	/* The lens depth is arbitrary (no depth test; position/size scale
	 * linearly with it, so the screen result is depth-invariant). Keep a
	 * comfortably large view depth for stable quad construction. */
	const float zl = 4096.0f;
	/* The classic reflection center is the raw viewport center (NOT
	 * the projection center): vp_center_x, and vp_center_y measured in
	 * the queue's bottom-origin space == vp_h - vp_center_y here. */
	const float source_w = cam->vp_w ? (float)cam->vp_w : (cam->screen_w ? (float)cam->screen_w : 640.0f);
	const float source_h = cam->vp_h ? (float)cam->vp_h : (cam->screen_h ? (float)cam->screen_h : 480.0f);
	const float ccx = flight_view->viewport.x + flight_view->viewport.width * 0.5f +
					  (cam->vp_center_x - source_w * 0.5f) * flight_view->classic_pixel_scale;
	const float ccy = flight_view->viewport.y + flight_view->viewport.height * 0.5f +
					  (source_h - cam->vp_center_y - source_h * 0.5f) * flight_view->classic_pixel_scale;

	for (int r = 0; r < FL_LENS_RINGS; r++) {
		const float projection = cam->proj_scale > 0.0f ? cam->proj_scale : 512.0f;
		const float k = fl_lens_size[r] * (projection / 512.0f);
		const float half_w_px =
			(float)s.lens_dim[r][0] * k * (1.0f / 512.0f) * flight_view->classic_pixel_scale;
		const float half_h_px =
			(float)s.lens_dim[r][1] * k * (1.0f / 512.0f) * flight_view->classic_pixel_scale;
		for (uint32_t si = 0; si < s.lens_src_count; si++) {
			const FlLensSource* src = &s.lens_src[si];
			const float dx = src->sx - ccx;
			const float dy = src->sy - ccy;
			float qx, qy;
			switch (r) {
				case 0:
					qx = src->sx - dx * 0.0625f;
					qy = src->sy - dy * 0.0625f;
					break;
				case 1:
					qx = src->sx - dx * 0.25f;
					qy = src->sy - dy * 0.25f;
					break;
				case 2:
					qx = ccx + dx * 0.125f;
					qy = ccy + dy * 0.125f;
					break;
				case 3:
					qx = ccx - dx * 0.25f;
					qy = ccy - dy * 0.25f;
					break;
				case 4:
					qx = ccx - dx * 0.5f;
					qy = ccy - dy * 0.5f;
					break;
				case 5:
					qx = ccx - dx;
					qy = ccy - dy;
					break;
				default:
					qx = ccx + dx * 0.5f;
					qy = ccy + dy * 0.5f;
					break;
			}
			/* Output pixels -> view at the fixed lens depth through the
			 * prepared scene projection. */
			const float ndc_x = 2.0f * (qx - flight_view->viewport.x) / flight_view->viewport.width - 1.0f;
			const float ndc_y = 1.0f - 2.0f * (qy - flight_view->viewport.y) / flight_view->viewport.height;
			const float cx =
				(ndc_x - flight_view->camera.proj_x_offset) * zl * tanf(flight_view->camera.h_half_rad);
			const float cy =
				(flight_view->camera.proj_y_offset - ndc_y) * zl * tanf(flight_view->camera.v_half_rad);
			const float hw =
				half_w_px * zl * 2.0f * tanf(flight_view->camera.h_half_rad) / flight_view->viewport.width;
			const float hh =
				half_h_px * zl * 2.0f * tanf(flight_view->camera.v_half_rad) / flight_view->viewport.height;
			float corners[4][3];
			corners[0][0] = cx + hw;
			corners[0][1] = cy + hh;
			corners[0][2] = zl;
			corners[1][0] = cx - hw;
			corners[1][1] = cy + hh;
			corners[1][2] = zl;
			corners[2][0] = cx - hw;
			corners[2][1] = cy - hh;
			corners[2][2] = zl;
			corners[3][0] = cx + hw;
			corners[3][1] = cy - hh;
			corners[3][2] = zl;
			fl_submit_view_quad(AERON_SCENE_BILLBOARD_STAGE_LENS, corners, fl_default_uvs, &s.lens_ref[r],
								src->argb, src->anchor_local);
		}
	}
}

/* ---- point lights (state-derived) -------------------------------------
 * Classic SOURCE laws (positions, colors, classic intensities, cull
 * radii) from FlightLight_AppendScenePointLightForObject, evaluated as
 * modern PBR punctual lights (frame-global, windowed inverse-square,
 * per-pixel Lambert + spec — the sanctioned look-better deviation from
 * the classic per-object 8-light 1/d vertex law). Sources:
 *   - explosion-genus sprites / projectiles / DS fixtures via the
 *     XwaSnapshotExport_PointLightForObject accessor (the classic
 *     type switch + intensity tables), with the classic append gates;
 *   - capital engine glows via the cooked glb extras (collected in
 *     the instance walk where mesh + articulation exist);
 *   - the local player's weapon-fire pulses (classic cycle/fade
 *     envelope replayed from game_time_ms at the hardpoint).
 * The classic Local Lights config level is NOT honored (period
 * option); the classic receiver-side gates (8-per-object selection,
 * screen-size gate, Starship half-scale) are replaced by the
 * frame-global cap with distance-to-camera priority. */

static void fl_pl_push(const float pos[3], float range, const float color_x_intensity[3]) {
	if (s.pl_cand_count >= FL_MAX_PL_CANDIDATES) {
		return;
	}
	XwaShipPointLight* l = &s.pl_cand[s.pl_cand_count++];
	memcpy(l->pos, pos, 3 * sizeof(float));
	l->range = range;
	memcpy(l->color, color_x_intensity, 3 * sizeof(float));
}

/* Window range from the classic VISIBILITY law: the classic 0.5/d
 * contribution has no shading cutoff (its cull radius only gates
 * per-object selection), so the window sits where the contribution
 * falls under ~1% (d = 50 * intensity), never below the classic cull
 * radius, capped for the GPU early-out. */
static float fl_pl_range(float classic_intensity, float cull_radius) {
	float r = 50.0f * classic_intensity;
	if (r < cull_radius) {
		r = cull_radius;
	}
	if (r > 32768.0f) {
		r = 32768.0f;
	}
	return r;
}

/* Accessor-law sources (everything except engine glows + pulses). */
static void fl_derive_point_lights(const XwaSnapshot* snap) {
	s.pl_cand_count = 0;
	if (!s.plight.enabled) {
		return;
	}
	const XwaFlightCamera* cam = &snap->flight_camera;
	/* DS tunnel turbolaser beam (FlightView_Render's direct scene-
	 * light write: green, intensity 100000, cull 0x4000 — appended
	 * FIRST like the classic). */
	if (cam->ds_beam_active) {
		const float i = 100000.0f;
		const float cx[3] = { 0.0f, XwaRemaster_SrgbToLinear(1.0f) * i, XwaRemaster_SrgbToLinear(0.2f) * i };
		float local[3];
		XwaRemasterWorld_LocalI32(s.origin_world, cam->ds_beam_world_pos, local);
		fl_pl_push(local, fl_pl_range(i, 16384.0f), cx);
	}
	for (uint32_t i = 0; i < snap->flight_object_count; i++) {
		const XwaFlightObject* f = &snap->flight_objects[i];
		/* Mirrors FlightView's active-main and local-effect slot walks;
		 * their dynamically spawned records may carry a stale region byte. */
		if (f->slot_class == XWA_SNAP_SLOT_OTHER) {
			continue;
		}
		/* Classic append gate (genus/type list; the engine-glow branch
		 * for ordinary craft is collected in the instance walk). */
		const int gate =
			f->genus == XWA_SNAP_GENUS_EXPLOSION || f->genus == XWA_SNAP_GENUS_PLAYER_PROJECTILE ||
			f->genus == XWA_SNAP_GENUS_NPC_PROJECTILE || f->genus == XWA_SNAP_GENUS_DS_TUNNEL ||
			f->object_type == XWA_SNAP_TYPE_DS_REACTOR ||
			(f->object_type == XWA_SNAP_TYPE_FALCON2 && cam->ds_hangar && snap->dir_light_count == 0);
		if (!gate) {
			continue;
		}
		float color[3];
		float intensity = 0.0f;
		int cull = 0;
		if (!XwaSnapshotExport_PointLightForObject(f->object_type, f->genus, f->type_specific_0,
												   f->type_specific_w, f->instance_extent, cam->brightness_q8,
												   color, &intensity, &cull) ||
			intensity <= 0.0f) {
			continue;
		}
		float cxi[3];
		for (int c = 0; c < 3; c++) {
			cxi[c] = XwaRemaster_SrgbToLinear(color[c]) * intensity;
		}
		float local[3];
		fl_object_local(f, local);
		fl_pl_push(local, fl_pl_range(intensity, (float)cull), cxi);
	}
	/* Particle_AppendObjectEffectPointLights contributes one warm light at
	 * each eligible object-attached emitter (effect type 1 is excluded).
	 * The recovered classic helper stores receiver-relative coordinates;
	 * at the HD boundary this becomes the intended world emitter position
	 * and participates in the same frame-global PBR light selection. */
	for (uint32_t i = 0; i < snap->particle_effect_count; i++) {
		const XwaParticleEffect* effect = &snap->particle_effects[i];
		if (!effect->point_light || effect->source_kind != XWA_PARTICLE_SOURCE_OBJECT ||
			effect->region != cam->region) {
			continue;
		}
		const float intensity = 125.0f;
		const float cxi[3] = { intensity, intensity, XwaRemaster_SrgbToLinear(0.75f) * intensity };
		float local[3];
		XwaRemasterWorld_LocalPoint(s.origin_world, effect->emitter_world_pos.base,
									effect->emitter_world_pos.offset, local);
		fl_pl_push(local, fl_pl_range(intensity, 0.0f), cxi);
	}
}

/* Weapon-fire pulse lights: the classic cycle/fade envelope
 * (FlightLight_AppendLocalPlayerPulses) replayed from the sim clock;
 * position = the weapon hardpoint in the player craft's model space,
 * carried to world through the player basis. */
static void fl_derive_pulse_lights(const XwaSnapshot* snap, const float player_bw[9],
								   const float player_pos[3]) {
	if (!s.plight.enabled || !snap->light_pulse_active || !player_bw) {
		return;
	}
	const XwaCockpit* k = &snap->cockpit;
	float world[3];
	for (int r = 0; r < 3; r++) {
		world[r] = player_pos[r] + player_bw[0 * 3 + r] * k->hardpoint_local[0] +
				   player_bw[1 * 3 + r] * k->hardpoint_local[1] +
				   player_bw[2 * 3 + r] * k->hardpoint_local[2];
	}
	for (int p = 0; p < XWA_SNAP_MAX_LIGHT_PULSES; p++) {
		const XwaLightPulse* pl = &snap->light_pulses[p];
		if (!pl->enabled) {
			continue;
		}
		const double elapsed = (double)(snap->game_time_ms - pl->start_time);
		const double phase_t =
			elapsed - (double)(int)(elapsed * pl->inv_cycle_ticks) * (double)pl->cycle_ticks;
		const float phase = (float)(phase_t * pl->inv_cycle_ticks);
		float fade;
		if (phase > 0.5f) {
			if (pl->fade_ticks <= 0.0f) {
				continue;
			}
			const float h = (phase - 0.5f) * 2.0f;
			if (h > pl->fade_ticks) {
				continue;
			}
			fade = 1.0f - h / pl->fade_ticks;
		} else if (pl->fade_ticks > 0.0f) {
			const float h = phase * 2.0f;
			fade = h <= pl->fade_ticks ? h / pl->fade_ticks : 1.0f;
		} else {
			fade = 1.0f;
		}
		float cxi[3];
		for (int c = 0; c < 3; c++) {
			cxi[c] = XwaRemaster_SrgbToLinear(pl->color[c]) * pl->intensity * fade;
		}
		fl_pl_push(world, fl_pl_range(pl->intensity * fade, pl->cull_radius * fade), cxi);
	}
}

/* Apply calibration, select the brightest records, and return the count
 * ready for generic scene-light submission. */
static uint32_t fl_finalize_point_lights(XwaShipPointLightTuning* tuning) {
	memset(tuning, 0, sizeof *tuning);
	if (!s.plight.enabled || s.pl_cand_count == 0) {
		return 0;
	}
	float scores[FL_MAX_PL_CANDIDATES];
	for (uint32_t i = 0; i < s.pl_cand_count; i++) {
		XwaShipPointLight* l = &s.pl_cand[i];
		for (int c = 0; c < 3; c++) {
			l->color[c] *= s.plight.scale;
		}
		l->range *= s.plight.range_scale;
		const float dx = l->pos[0] - s.camera_local[0];
		const float dy = l->pos[1] - s.camera_local[1];
		const float dz = l->pos[2] - s.camera_local[2];
		const float d2 = dx * dx + dy * dy + dz * dz;
		const float lum = 0.2126f * l->color[0] + 0.7152f * l->color[1] + 0.0722f * l->color[2];
		scores[i] = lum / (d2 + l->range * l->range);
	}
	/* Top-N by score: simple selection over the candidate pool. */
	uint32_t n = s.pl_cand_count < XWA_SHIP_MAX_POINT_LIGHTS ? s.pl_cand_count : XWA_SHIP_MAX_POINT_LIGHTS;
	uint8_t used[FL_MAX_PL_CANDIDATES] = { 0 };
	for (uint32_t k = 0; k < n; k++) {
		int best = -1;
		float best_s = -1.0f;
		for (uint32_t i = 0; i < s.pl_cand_count; i++) {
			if (!used[i] && scores[i] > best_s) {
				best_s = scores[i];
				best = (int)i;
			}
		}
		if (best < 0) {
			break;
		}
		used[best] = 1;
		s.pl_final[k] = s.pl_cand[best];
	}
	tuning->min_distance = s.plight.min_distance;
	tuning->spec_weight = s.plight.spec_weight;
	tuning->diffuse_wrap = s.plight.diffuse_wrap;
	tuning->contrib_cap = s.plight.contrib_cap;
	tuning->light_count = n;
	return n;
}

static void fl_submit_point_lights(uint32_t count) {
	for (uint32_t i = 0; i < count; ++i) {
		AeronSceneLight light = { 0 };
		memcpy(light.pos, s.pl_final[i].pos, sizeof light.pos);
		light.radius = s.pl_final[i].range;
		memcpy(light.color, s.pl_final[i].color, sizeof light.color);
		AeronScene_AddLight(s.scene, &light);
	}
}

/* Submit the derived set as OVERLAY billboards (corner build shared
 * with the retired capture path: rotated view-plane axes, (+h+w, +h-w,
 * -h-w, -h+w) loop matching the default UV order). */
static void fl_submit_billboards(void) {
	for (uint32_t i = 0; i < s.bb_count; i++) {
		const FlBillboard* b = &s.bb[i];
		const float ca = cosf(b->rot);
		const float sa = sinf(b->rot);
		const float ax[2] = { b->half_w * ca, -b->half_w * sa };
		const float ay[2] = { b->half_h * sa, b->half_h * ca };
		float corners[4][3];
		for (int c = 0; c < 4; c++) {
			const float sw = (c == 0 || c == 3) ? 1.0f : -1.0f;
			const float sh = (c <= 1) ? 1.0f : -1.0f;
			corners[c][0] = b->center[0] + sw * ax[0] + sh * ay[0];
			corners[c][1] = b->center[1] + sw * ax[1] + sh * ay[1];
			corners[c][2] = b->center[2];
		}
		fl_submit_view_quad_ex(AERON_SCENE_BILLBOARD_STAGE_OVERLAY, corners, fl_default_uvs, &b->ref, b->argb,
							   NULL, b->depth_bias, b->has_prev ? b->prev_corners : NULL);
	}
}

/* Locate the two poses the hyperspace special path still consumes. It
 * deliberately does not submit any world object: the classic path draws only
 * the effect field and (optionally) the player cockpit. */
static void fl_hyper_find_poses(const XwaSnapshot* snap, const XwaFlightCamera* cam, float anchor_bw[9],
								int* anchor_found, const XwaFlightObject** player_f, float player_bw[9],
								float player_local[3], int* player_found) {
	*anchor_found = 0;
	*player_found = 0;
	*player_f = NULL;
	/* FlightView_RenderCockpitModel is a separate special-mode draw and
	 * always passes the local player's object, unlike the ordinary flight
	 * render-list cockpit which is attached to cameraFocusObjIdx. */
	const int32_t anchor_slot = cam->player_obj_idx;
	for (uint32_t i = 0; i < snap->flight_object_count; i++) {
		const XwaFlightObject* f = &snap->flight_objects[i];
		if ((int32_t)f->slot != anchor_slot && (int32_t)f->slot != cam->player_obj_idx) {
			continue;
		}
		float cur[3][3], bw[9];
		if (f->has_mobj && !f->orient_dirty)
			fl_curmat_from_cached(f->rows, cur);
		else
			fl_curmat_from_euler(f, cur);
		fl_object_world(cur, bw);
		if ((int32_t)f->slot == anchor_slot) {
			memcpy(anchor_bw, bw, 9 * sizeof(float));
			*anchor_found = 1;
		}
		if ((int32_t)f->slot == cam->player_obj_idx) {
			*player_f = f;
			memcpy(player_bw, bw, 9 * sizeof(float));
			fl_object_local(f, player_local);
			*player_found = 1;
		}
	}
}

/* Hyperspace uses the same state-derived cockpit transform and articulation
 * as ordinary flight, but mirrors its own classic gate: cockpitVisible alone
 * selects the cockpit inside the special-mode branch. */
static void fl_submit_hyperspace_cockpit(AeronCommandBuffer* cmd, XwaRemasterAssets* assets,
										 const XwaSnapshot* snap, const XwaFlightCamera* cam,
										 const float anchor_bw[9], int anchor_found,
										 const XwaFlightObject* player_f) {
	if (!snap->cockpit_valid || !cam->cockpit_visible || !anchor_found) {
		return;
	}
	AeronSceneMesh* cockpit_mesh = XwaRemasterShip_MeshForName(snap->cockpit.model_name);
	if (!cockpit_mesh) {
		return;
	}
	float bw[9];
	memcpy(bw, anchor_bw, sizeof bw);
	float w[3], delta[3];
	for (int a = 0; a < 3; a++) {
		w[a] = -(snap->cockpit.hardpoint_world[a] + snap->cockpit.camera_pan[a] * 0.0625f);
	}
	fl_world_to_view(s.crows, w[0], w[1], w[2], delta);
	if (snap->cockpit.seat == 2) {
		float be[9];
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				be[i * 3 + j] = bw[i * 3 + 0] * s.crows[j * 3 + 0] + bw[i * 3 + 1] * s.crows[j * 3 + 1] +
								bw[i * 3 + 2] * s.crows[j * 3 + 2];
			}
		}
		float e[3];
		for (int c = 0; c < 3; c++) {
			e[c] = be[c] * delta[0] + be[3 + c] * delta[1] + be[6 + c] * delta[2];
		}
		e[1] = -e[1];
		e[2] = -e[2];
		for (int r = 0; r < 3; r++) {
			delta[r] = be[r * 3 + 0] * e[0] + be[r * 3 + 1] * e[1] + be[r * 3 + 2] * e[2];
			be[r * 3 + 1] = -be[r * 3 + 1];
			be[r * 3 + 2] = -be[r * 3 + 2];
		}
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				bw[i * 3 + j] = be[i * 3 + 0] * s.crows[0 * 3 + j] + be[i * 3 + 1] * s.crows[1 * 3 + j] +
								be[i * 3 + 2] * s.crows[2 * 3 + j];
			}
		}
	}
	float pw[3];
	for (int j = 0; j < 3; j++) {
		pw[j] = s.camera_local[j] + s.crows[0 * 3 + j] * delta[0] + s.crows[1 * 3 + j] * delta[1] +
				s.crows[2 * 3 + j] * delta[2];
	}
	float m[16];
	fl_model_matrix(bw, pw, m);
	AeronSceneMeshInstance inst;
	memset(&inst, 0, sizeof inst);
	inst.mesh = cockpit_mesh;
	memcpy(inst.transform, m, sizeof m);
	memcpy(inst.prev_transform, m, sizeof m);
	inst.no_local_lights = 1;
	inst.zero_velocity = 1;
	inst.cull_mode = AERON_CULL_BACK;
	AeronSceneMeshTable* tb = &s.tables[s.table_count];
	if (XwaRemasterShip_BuildCockpitMeshTable(cockpit_mesh, &snap->cockpit, tb)) {
		inst.mesh_table = tb;
		s.table_count++;
	}
	AeronScene_AddMeshInstance(s.scene, &inst);
	if (player_f) {
		XwaRemasterGlowMarks_SubmitObject(s.scene, cmd, assets, snap, player_f, cockpit_mesh, m,
										  inst.mesh_table);
	}
	if (s.glow_ok && player_f) {
		XwaRemasterShip_SubmitEngineGlows(
			s.scene, cockpit_mesh, m, 1.0f, inst.mesh_table, player_f->eg_knockout_mask,
			XwaRemasterShip_EngineGlowScale(player_f), s.crows, s.camera_local, &s.glow_ref);
	}
}

static int fl_ensure_direct_present(void) {
	const AeronTextureFormat format = Aeron_SwapchainFormat();
	if (format == AERON_TEXTURE_FORMAT_UNKNOWN) {
		return 0;
	}
	if (s.present_direct && s.direct_present_format == format) {
		return 1;
	}
	if (s.present_direct) {
		AeronScenePresentChain_Destroy(s.present_direct);
		s.present_direct = NULL;
	}
	s.direct_present_format = AERON_TEXTURE_FORMAT_UNKNOWN;
	s.present_direct = AeronScenePresentChain_Create(format);
	if (!s.present_direct) {
		Aeron_RequestFatalRendererError("direct flight presentation resource creation");
		return 0;
	}
	s.direct_present_format = format;
	return 1;
}

int XwaRemasterFlight_CanDirectPresent(int target_w, int target_h) {
	return Aeron_CanRenderDirectToSwapchain(target_w, target_h) && fl_ensure_direct_present();
}

/* Shared tone-map + semantic-HUD draw sequence. The target can be the FP16
 * composition fallback or Aeron's borrowed swapchain target. */
static void fl_draw_present(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
							AeronScenePresentChain* chain, int target_w, int target_h) {
	if (!cmd || !pass || !target || !chain || !s.present_scene_tex || !s.present_sampler) {
		return;
	}
	const AeronRectI full_target = { 0, 0, target_w, target_h };
	Aeron_SetViewport(pass, &full_target);
	Aeron_SetScissor(pass, &full_target);
	static const float present_tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	AeronScenePresentChain_Draw(chain, pass, s.present_scene_tex, s.present_sampler, s.present_bloom_tex,
								AeronSceneBloom_Intensity(), s.rt_w, s.rt_h,
								/*bar_y_uv=*/1.0f, present_tint, /*src_coverage=*/0);
	Aeron_GpuDebugMarker(cmd, "HUD target boxes before fixed");
	XwaRemasterHud_RenderTargetBoxes(cmd, pass, target, target_w, target_h, XWA_HUD_TARGET_BOX_BEFORE_FIXED);
	Aeron_GpuDebugMarker(cmd, "HUD fixed");
	XwaRemasterHud_RenderFixed(cmd, pass, target, target_w, target_h);
	Aeron_GpuDebugMarker(cmd, "HUD target boxes after fixed");
	XwaRemasterHud_RenderTargetBoxes(cmd, pass, target, target_w, target_h, XWA_HUD_TARGET_BOX_AFTER_FIXED);
	Aeron_GpuDebugMarker(cmd, "HUD CMD");
	XwaRemasterHud_RenderCmd(cmd, pass, target, target_w, target_h);
	Aeron_GpuDebugMarker(cmd, "HUD text");
	XwaRemasterHud_RenderText(cmd, pass, target, target_w, target_h);
}

AeronTexture* XwaRemasterFlight_ResolvePresentation(AeronCommandBuffer* cmd, int direct_present) {
	s.direct_ready = 0;
	if (!s.present_scene_tex || !s.present_sampler) {
		return NULL;
	}
	if (direct_present) {
		if (!fl_ensure_direct_present()) {
			return NULL;
		}
		s.direct_ready = 1;
		return s.present_scene_tex;
	}
	if (!cmd || !s.present || !s.present_rt) {
		return NULL;
	}
	AeronRenderPass* pass = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
		.color_target = s.present_rt,
		.clear_color = 1,
		.clear_color_rgba = { 0.0f, 0.0f, 0.0f, 0.0f },
		.command_buffer = cmd,
		.debug_label = "OpenXWA flight tonemap + bloom",
	});
	if (!pass) {
		return NULL;
	}
	fl_draw_present(cmd, pass, s.present_rt, s.present, s.rt_w, s.rt_h);
	Aeron_EndRenderPass(pass);
	return Aeron_RenderTargetGetTexture(s.present_rt);
}

static void fl_direct_present_callback(AeronCommandBuffer* cmd, AeronRenderPass* pass,
									   AeronRenderTarget* target, int target_w, int target_h,
									   void* userdata) {
	(void)userdata;
	if (!s.direct_ready || target_w != s.rt_w || target_h != s.rt_h) {
		return;
	}
	fl_draw_present(cmd, pass, target, s.present_direct, target_w, target_h);
}

int XwaRemasterFlight_SubmitDirectPresentation(void) {
	if (!s.direct_ready) {
		return 0;
	}
	return Aeron_SubmitSwapchainRenderLayer(&(AeronSwapchainRenderLayerDesc) {
		.callback = fl_direct_present_callback,
		.required_width = s.rt_w,
		.required_height = s.rt_h,
		.debug_label = "OpenXWA flight direct presentation",
	});
}

int XwaRemasterFlight_DirectPresentationReady(void) { return s.direct_ready; }

void XwaRemasterFlight_InvalidateHistory(void) {
	s.fsr_prev_host_valid = 0;
	s.mb_pose_host_valid = 0;
	s.mb_velocity_span_us = 0;
	s.mb_enabled = 0;
	s.mb_prev_snap = NULL;
	fl_request_fsr_reset(FL_FSR_RESET_RENDER_RESUMED);
}

/* Both ordinary flight and the dedicated hyperspace scene arrive here with
 * AeronScene_Render complete. Bloom remains in the scene command buffer; the
 * final draws are either deferred to Aeron_Present or recorded into present_rt. */
static AeronTexture* fl_present(AeronCommandBuffer* cmd, int direct_present) {
	s.present_scene_tex = Aeron_RenderTargetGetTexture(AeronScene_SceneRt(s.scene));
	s.present_bloom_tex = NULL;
	if (!s.present_scene_tex) {
		s.direct_ready = 0;
		return NULL;
	}
	if (s.bloom && AeronSceneBloom_Intensity() > 0.0f) {
		if (!AeronSceneBloom_Apply(s.bloom, cmd, s.present_scene_tex, s.rt_w, s.rt_h, s.rt_h)) {
			Aeron_CommandBufferSetFailure(cmd, "Flight bloom recording failed");
			return NULL;
		}
		AeronRenderTarget* bloom_rt = AeronSceneBloom_ColorRt(s.bloom);
		s.present_bloom_tex = bloom_rt ? Aeron_RenderTargetGetTexture(bloom_rt) : NULL;
		if (!s.present_bloom_tex) {
			Aeron_CommandBufferSetFailure(cmd, "Flight bloom produced no output");
			return NULL;
		}
	}
	return XwaRemasterFlight_ResolvePresentation(cmd, direct_present);
}

static void fl_set_temporal(uint64_t host_time_us, int scene_supported) {
	const AeronTemporalMode mode = scene_supported ? s.fsr_mode : AERON_TEMPORAL_OFF;
	const int enabled = mode != AERON_TEMPORAL_OFF;
	if (!enabled) {
		AeronScene_SetTemporal(s.scene, &(AeronSceneTemporalDesc) { .mode = AERON_TEMPORAL_OFF });
		s.fsr_prev_host_valid = 0;
		fl_request_fsr_reset(FL_FSR_RESET_TEMPORAL_DISABLED);
		return;
	}

	float frame_delta_ms = 16.6667f;
	if (s.fsr_prev_host_valid && host_time_us > s.fsr_prev_host_us) {
		frame_delta_ms = (float)((double)(host_time_us - s.fsr_prev_host_us) / 1000.0);
	}
	s.fsr_prev_host_us = host_time_us;
	s.fsr_prev_host_valid = 1;
	fl_report_fsr_reset(s.fsr_reset_pending);
	AeronScene_SetTemporal(s.scene, &(AeronSceneTemporalDesc) {
										.mode = mode,
										.frame_time_delta_ms = frame_delta_ms,
										.sharpness = s.fsr_sharpness,
										.reset_history = s.fsr_reset_pending,
										.debug_view = s.fsr_debug_view,
									});
	s.fsr_reset_pending = 0;
	s.fsr_reset_reasons = 0;
}

static AeronTexture* fl_render_hyperspace(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
										  XwaRemasterAssets* assets, const XwaRemasterFlightView* flight_view,
										  uint64_t host_time_us, int direct_present) {
	if (!s.hyperspace || !XwaRemasterHyperspace_Prepare(s.hyperspace, cmd, snap, assets,
														flight_view->view_proj, s.crows, s.rt_w, s.rt_h)) {
		return NULL;
	}
	/* Cockpit glow texture is the only ordinary-flight atlas dependency
	 * retained by the special scene. */
	s.glow_ok = XwaRemasterAssets_FlightAtlasFrame(assets, 1000, 0, &s.glow_ref);
	s.table_count = 0;
	s.pl_cand_count = 0;
	s.snap = snap;

	/* The tunnel is drawn by a game hook without motion vectors. Disable
	 * temporal reconstruction for this special scene and reset history
	 * before returning to ordinary flight. */
	fl_set_temporal(host_time_us, 0);
	if (!AeronScene_Begin(s.scene, &flight_view->camera)) {
		Aeron_CommandBufferSetFailure(cmd, "Hyperspace scene initialization failed");
		s.snap = NULL;
		return NULL;
	}
	s.before_opaque_mode = FL_BEFORE_OPAQUE_HYPERSPACE;
	AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, fl_draw_before_opaque, NULL);
	/* SSAO remains active for the 3D cockpit. Motion blur is deliberately
	 * disabled: the tunnel/streak hook has no velocity attachment, and the
	 * classic effect already encodes its own motion in geometry/animation. */
	AeronScene_SetPost(s.scene, &(AeronScenePostDesc) {
									.ssao_quality = s.ssao.quality,
									.ssao_intensity = s.ssao.intensity,
									.ssao_power = s.ssao.power,
									.ssao_radius_view = s.ssao.radius_view,
									.ssao_bias_view = s.ssao.bias_view,
									.ssao_direct = s.ssao.direct,
									.ssao_debug_viz = s.ssao.debug_viz,
									.ssao_min_screen_frac = s.ssao.min_screen_frac,
									.ssao_max_screen_frac = s.ssao.max_screen_frac,
									.ssao_sample_jitter = s.ssao.sample_jitter,
									.mb_quality = 0,
									.mb_fsr_direct_motion = s.mb_fsr_direct_motion,
								});
	AeronScene_SetMotionContext(s.scene, NULL, 1);

	float anchor_bw[9], player_bw[9], player_local[3];
	int anchor_found, player_found;
	const XwaFlightObject* player_f;
	fl_hyper_find_poses(snap, &snap->flight_camera, anchor_bw, &anchor_found, &player_f, player_bw,
						player_local, &player_found);
	if (player_found && player_f) {
		fl_derive_pulse_lights(snap, player_bw, player_local);
	}
	XwaShipPointLightTuning point_tuning;
	const uint32_t point_count = fl_finalize_point_lights(&point_tuning);
	fl_submit_point_lights(point_count);
	int render_w, render_h;
	AeronScene_RenderDims(s.scene, &render_w, &render_h);
	const XwaShipAoParams ao = {
		.intensity = s.ssao.intensity,
		.power = s.ssao.power,
		.rt_w = (float)render_w,
		.rt_h = (float)render_h,
		.direct = s.ssao.direct,
	};
	const int ao_on = s.ssao.quality > 0 && s.ssao.intensity > 0.0f;
	XwaRemasterShip_SetPbrEnv(s.scene, snap->dir_lights, snap->dir_light_count, NULL, s.camera_local,
							  ao_on ? &ao : NULL, point_count ? &point_tuning : NULL,
							  /*ambient_cube=*/NULL);
	fl_submit_hyperspace_cockpit(cmd, assets, snap, &snap->flight_camera, anchor_bw, anchor_found, player_f);

	if (!AeronScene_Render(s.scene, cmd)) {
		AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, NULL, NULL);
		s.before_opaque_mode = FL_BEFORE_OPAQUE_NONE;
		s.snap = NULL;
		return NULL;
	}
	AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, NULL, NULL);
	s.before_opaque_mode = FL_BEFORE_OPAQUE_NONE;
	s.snap = NULL;
	return fl_present(cmd, direct_present);
}

typedef struct FlSceneLighting {
	const XwaDirLight* lights;
	uint32_t light_count;
	XwaDirLight hangar_key;
	XwaShipAmbientCube ambient_cube;
	const XwaShipAmbientCube* ambient;
	int hangar_override;
} FlSceneLighting;

static void fl_build_scene_lighting(const XwaSnapshot* snap, const XwaFlightCamera* cam,
									FlSceneLighting* out) {
	memset(out, 0, sizeof *out);
	out->lights = snap->dir_lights;
	out->light_count = snap->dir_light_count;
	if (!cam->in_hangar || !s.hangar_lighting.enabled) {
		return;
	}

	memcpy(out->hangar_key.world_dir, s.hangar_lighting.direction, sizeof out->hangar_key.world_dir);
	out->hangar_key.intensity = s.hangar_lighting.intensity;
	memcpy(out->hangar_key.color, s.hangar_lighting.color, sizeof out->hangar_key.color);
	out->lights = &out->hangar_key;
	out->light_count = 1;
	out->hangar_override = 1;

	for (int channel = 0; channel < 3; channel++) {
		out->ambient_cube.pos_x[channel] = out->ambient_cube.neg_x[channel] = s.hangar_lighting.ambient_sides;
		out->ambient_cube.pos_y[channel] = out->ambient_cube.neg_y[channel] = s.hangar_lighting.ambient_sides;
		out->ambient_cube.pos_z[channel] = s.hangar_lighting.ambient_ceiling;
		out->ambient_cube.neg_z[channel] = s.hangar_lighting.ambient_floor;
	}
	out->ambient = &out->ambient_cube;
}

static int fl_key_directional_light(const XwaDirLight* lights, uint32_t light_count, float out_direction[3]) {
	if (!lights || !out_direction || light_count == 0) {
		return 0;
	}
	const XwaDirLight* key = XwaRemasterShip_SelectKeyDirectionalLight(lights, light_count);
	if (!key) {
		return 0;
	}
	memcpy(out_direction, key->world_dir, 3 * sizeof(float));
	const float length = sqrtf(out_direction[0] * out_direction[0] + out_direction[1] * out_direction[1] +
							   out_direction[2] * out_direction[2]);
	if (length <= 0.0001f) {
		return 0;
	}
	out_direction[0] /= length;
	out_direction[1] /= length;
	out_direction[2] /= length;
	return 1;
}

static void fl_mb_reset_timing(uint64_t host_time_us) {
	s.mb_pose_host_us = host_time_us;
	s.mb_velocity_span_us = 0;
	s.mb_pose_host_valid = 1;
}

static void fl_mb_record_pose(uint64_t host_time_us) {
	s.mb_velocity_span_us =
		s.mb_pose_host_valid && host_time_us > s.mb_pose_host_us ? host_time_us - s.mb_pose_host_us : 0;
	s.mb_pose_host_us = host_time_us;
	s.mb_pose_host_valid = 1;
}

static float fl_mb_logical_frame_shutter(void) {
	if (s.mb_velocity_span_us == 0) {
		return 0.0f;
	}
	return (float)((double)s.mb_shutter * (double)FL_MB_REFERENCE_FRAME_US / (double)s.mb_velocity_span_us);
}

AeronTexture* XwaRemasterFlight_Render(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
									   XwaRemasterAssets* assets, int target_w, int target_h,
									   int direct_present) {
	if (!cmd || !snap || !assets || !snap->flight_camera_valid) {
		return NULL;
	}
	const XwaFlightCamera* cam = &snap->flight_camera;
	if (cam->map_mode) {
		return NULL; /* map reconstruction remains classic */
	}
	if (!fl_ensure(target_w, target_h)) {
		return NULL;
	}
	XwaRemasterFlightView flight_view;
	if (!XwaRemasterFlight_BuildView(cam, s.rt_w, s.rt_h, &flight_view)) {
		return NULL;
	}
	memcpy(s.origin_world, flight_view.origin_world, sizeof s.origin_world);
	memcpy(s.camera_local, flight_view.camera.pos, sizeof s.camera_local);
	XwaRemasterHud_Prewarm(cmd, snap, assets, &flight_view, s.rt_w, s.rt_h);
	fl_quat_to_mat3(flight_view.camera.ori, s.crows);
	const uint64_t host_time_us = XwaTime_GetElapsedUs();
	if (cam->hyperspace_phase) {
		return fl_render_hyperspace(cmd, snap, assets, &flight_view, host_time_us, direct_present);
	}
	s.snap = snap;
	/* The hook persists on the scene object; explicitly clear it when
	 * returning to the ordinary world path. */
	AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, NULL, NULL);
	s.before_opaque_mode = FL_BEFORE_OPAQUE_NONE;
	const XwaFlightObject* cockpit_anchor = fl_cockpit_anchor(snap, cam);

	/* ---- Motion-blur frame context (the TIE model) --------------------
	 * Velocity derives from the prev/curr SNAPSHOT pair (one sim advance
	 * per host tick in normal play — deviation register entry 1). A
	 * valid prev requires the same view mode; otherwise the camera
	 * teleported / the scene was re-entered and any velocity would be a
	 * garbage smear, so prev falls back to curr (zero velocity, held
	 * buffer invalidated via NULL prev_view_proj). */
	const XwaSnapshot* prev = XwaSnapshot_Previous();
	XwaRemasterFlightView previous_flight_view;
	const int mb_on = s.mb_quality > 0 || s.fsr_mode != AERON_TEMPORAL_OFF;
	s.mb_enabled = 0;
	int mb_regen = 0;
	int paused = 0;
	if (mb_on && prev && prev->flight_camera_valid) {
		const XwaFlightCamera* pc = &prev->flight_camera;
		if (pc->region == cam->region && !pc->hyperspace_phase && !pc->map_mode &&
			pc->in_hangar == cam->in_hangar && pc->external == cam->external &&
			pc->film_overlay == cam->film_overlay && snap->game_time_ms >= prev->game_time_ms) {
			fl_build_view_at_origin(pc, s.origin_world, s.rt_w, s.rt_h, &previous_flight_view);
			memcpy(s.mb_prev_vp, previous_flight_view.view_proj, sizeof s.mb_prev_vp);
			s.mb_enabled = 1;
			/* Hold velocity while a paused snapshot is truly unchanged. Camera
			 * panning and cockpit articulation can still change with frozen sim
			 * time, and must regenerate velocity for both temporal consumers. */
			paused = snap->game_time_ms == prev->game_time_ms;
			mb_regen = !paused || fl_temporal_pose_changed(snap, prev);
			/* Prev lookup by stable identity: objectSignature is unique
			 * per spawn (slot reuse bumps it), so signature + slot pins
			 * the same live object across the pair. Same-index fast path
			 * first (slots are stable while an object lives). */
			for (uint32_t i = 0; i < snap->flight_object_count; i++) {
				const XwaFlightObject* f = &snap->flight_objects[i];
				s.mb_prev_index[i] = -1;
				if (i < prev->flight_object_count && prev->flight_objects[i].signature == f->signature &&
					prev->flight_objects[i].slot == f->slot) {
					s.mb_prev_index[i] = (int32_t)i;
					continue;
				}
				for (uint32_t j = 0; j < prev->flight_object_count; j++) {
					if (prev->flight_objects[j].signature == f->signature &&
						prev->flight_objects[j].slot == f->slot) {
						s.mb_prev_index[i] = (int32_t)j;
						break;
					}
				}
			}
		}
	}
	if (!s.mb_enabled || !s.mb_pose_host_valid) {
		fl_mb_reset_timing(host_time_us);
	} else if (mb_regen) {
		fl_mb_record_pose(host_time_us);
	}
	s.mb_prev_snap = s.mb_enabled ? prev : NULL;
	float previous_crows[9];
	const XwaFlightCamera* billboard_history_cam = NULL;
	const float* billboard_history_crows = NULL;
	const XwaFlightObject* previous_cockpit_anchor = NULL;
	int cockpit_history_valid = 0;
	if (s.mb_enabled) {
		fl_quat_to_mat3(previous_flight_view.camera.ori, previous_crows);
		/* Camera blur needs the geometry that the previous camera actually
		 * faced. Object-only blur keeps both endpoints in the current
		 * camera-facing basis so camera rotation does not leak into velocity. */
		billboard_history_cam = s.mb_camera_blur ? &prev->flight_camera : cam;
		billboard_history_crows = s.mb_camera_blur ? previous_crows : s.crows;
		previous_cockpit_anchor = fl_cockpit_anchor(prev, &prev->flight_camera);
		cockpit_history_valid = cockpit_anchor && previous_cockpit_anchor && fl_cockpit_drawn(snap, cam) &&
								fl_cockpit_drawn(prev, &prev->flight_camera) &&
								snap->cockpit.seat == prev->cockpit.seat &&
								strcmp(snap->cockpit.model_name, prev->cockpit.model_name) == 0 &&
								cockpit_anchor->slot == previous_cockpit_anchor->slot &&
								cockpit_anchor->signature == previous_cockpit_anchor->signature &&
								cockpit_anchor->object_type == previous_cockpit_anchor->object_type;
	}
	const int current_cockpit_drawn = fl_cockpit_drawn(snap, cam);
	const int previous_cockpit_drawn = s.mb_enabled && fl_cockpit_drawn(prev, &prev->flight_camera);
	uint32_t cockpit_reset_reasons = 0;
	if (current_cockpit_drawn != previous_cockpit_drawn) {
		cockpit_reset_reasons |= FL_FSR_RESET_COCKPIT_VISIBILITY;
	}
	if (current_cockpit_drawn && !cockpit_history_valid) {
		if (!cockpit_anchor) {
			cockpit_reset_reasons |= FL_FSR_RESET_COCKPIT_CURRENT_ANCHOR;
		}
		if (!previous_cockpit_anchor) {
			cockpit_reset_reasons |= FL_FSR_RESET_COCKPIT_PREVIOUS_ANCHOR;
		}
		if (s.mb_enabled && prev) {
			if (snap->cockpit.seat != prev->cockpit.seat) {
				cockpit_reset_reasons |= FL_FSR_RESET_COCKPIT_SEAT;
			}
			if (strcmp(snap->cockpit.model_name, prev->cockpit.model_name) != 0) {
				cockpit_reset_reasons |= FL_FSR_RESET_COCKPIT_MODEL;
			}
			if (cockpit_anchor && previous_cockpit_anchor &&
				(cockpit_anchor->slot != previous_cockpit_anchor->slot ||
				 cockpit_anchor->signature != previous_cockpit_anchor->signature ||
				 cockpit_anchor->object_type != previous_cockpit_anchor->object_type)) {
				cockpit_reset_reasons |= FL_FSR_RESET_COCKPIT_ANCHOR_ID;
			}
		}
	}
	if (cockpit_reset_reasons) {
		fl_request_fsr_reset(cockpit_reset_reasons);
	}

	/* Derive billboard/backdrop/lens-flare quads and resolve their
	 * frames (atlas uploads happen before the scene's render passes
	 * open); point-light candidates start with the accessor-law
	 * sources (glow lights + pulses append during/after the walk). */
	fl_derive_billboards(snap, assets, billboard_history_cam, billboard_history_crows);
	fl_derive_lens_flares(snap, assets, &flight_view);
	fl_derive_point_lights(snap);
	s.bd_count = fl_derive_backdrops(snap, s.crows, assets);
	XwaRemasterEffectView effect_view;
	XwaRemasterEffectView_Main(&effect_view, &flight_view, s.crows);
	XwaRemasterEffectView previous_effect_view;
	const XwaRemasterEffectView* particle_history_view = NULL;
	if (s.mb_enabled) {
		if (s.mb_camera_blur) {
			XwaRemasterEffectView_Main(&previous_effect_view, &previous_flight_view, previous_crows);
			particle_history_view = &previous_effect_view;
		} else {
			/* Previous particle state reconstructed in the current facing
			 * basis leaves only particle/object motion in the velocity stamp. */
			particle_history_view = &effect_view;
		}
	}
	XwaRemasterParticles_Prepare(s.particles, cmd, snap, s.mb_prev_snap, assets, &effect_view,
								 particle_history_view);
	XwaRemasterTrails_Prepare(s.trails, cmd, snap, assets, &effect_view);
	/* Engine glows sample LightingEffects group 1000 frame 1 (classic
	 * FeDiskIo_SelectTextureFrame(1000, 1, 256)) — frame index 0 in the
	 * baked atlas. */
	s.glow_ok = XwaRemasterAssets_FlightAtlasFrame(assets, 1000, 0, &s.glow_ref);

	FlSceneLighting scene_lighting;
	fl_build_scene_lighting(snap, cam, &scene_lighting);
	float shadow_light_dir[3];
	const int shadows_active =
		s.shadows.mode == XWA_FLIGHT_SHADOWS_PCF &&
		fl_key_directional_light(scene_lighting.lights, scene_lighting.light_count, shadow_light_dir);
	if (!s.mb_enabled) {
		uint32_t motion_reset_reasons = 0;
		if (!prev) {
			motion_reset_reasons |= FL_FSR_RESET_NO_PREVIOUS_SNAPSHOT;
		} else if (!prev->flight_camera_valid) {
			motion_reset_reasons |= FL_FSR_RESET_PREVIOUS_CAMERA_INVALID;
		} else {
			const XwaFlightCamera* pc = &prev->flight_camera;
			if (pc->region != cam->region)
				motion_reset_reasons |= FL_FSR_RESET_REGION_CHANGE;
			if (pc->hyperspace_phase)
				motion_reset_reasons |= FL_FSR_RESET_PREVIOUS_HYPERSPACE;
			if (pc->map_mode)
				motion_reset_reasons |= FL_FSR_RESET_PREVIOUS_MAP;
			if (pc->in_hangar != cam->in_hangar)
				motion_reset_reasons |= FL_FSR_RESET_HANGAR_CHANGE;
			if (pc->external != cam->external)
				motion_reset_reasons |= FL_FSR_RESET_EXTERNAL_VIEW_CHANGE;
			if (pc->film_overlay != cam->film_overlay)
				motion_reset_reasons |= FL_FSR_RESET_FILM_OVERLAY_CHANGE;
			if (snap->game_time_ms < prev->game_time_ms)
				motion_reset_reasons |= FL_FSR_RESET_GAME_TIME_ROLLBACK;
		}
		fl_request_fsr_reset(motion_reset_reasons ? motion_reset_reasons
												  : FL_FSR_RESET_MOTION_CONTEXT_UNKNOWN);
	}
	fl_set_temporal(host_time_us, 1);
	if (!AeronScene_Begin(s.scene, &flight_view.camera)) {
		Aeron_CommandBufferSetFailure(cmd, "Flight scene initialization failed");
		s.snap = NULL;
		return NULL;
	}
	if (shadows_active) {
		const AeronSceneDirectionalShadowDesc shadow = {
			.enabled = 1,
			.light_dir = { shadow_light_dir[0], shadow_light_dir[1], shadow_light_dir[2] },
			.world_origin = { (double)s.origin_world[0], (double)s.origin_world[1],
							  (double)s.origin_world[2] },
			.atlas_size = (uint32_t)s.shadows.atlas_size,
			.cascade_count = (uint32_t)s.shadows.cascade_count,
			.fit_mode = (uint32_t)s.shadows.fit_mode,
			.max_distance = s.shadows.max_distance,
			.split_lambda = s.shadows.split_lambda,
			.explicit_splits = s.shadows.explicit_splits,
			.split_positions = { s.shadows.split_positions[0], s.shadows.split_positions[1],
								 s.shadows.split_positions[2] },
			.filter_quality = (uint32_t)s.shadows.filter_quality,
			.filter_radius = scene_lighting.hangar_override ? s.hangar_lighting.shadow_filter_radius
															: s.shadows.filter_radius,
			.contact_hardening = s.shadows.contact_hardening,
			.light_angular_radius_degrees = s.shadows.light_angular_radius_degrees,
			.max_filter_radius = s.shadows.max_filter_radius,
			.pcss_min_filter_radius = s.shadows.pcss_min_filter_radius,
			.normal_bias = s.shadows.normal_bias,
			.normal_bias_face_normal = s.shadows.normal_bias_face_normal,
			.depth_bias_texels = s.shadows.depth_bias_texels,
			.slope_bias = s.shadows.slope_bias,
			.receiver_plane_bias = s.shadows.receiver_plane_bias,
			.caster_constant_bias = s.shadows.caster_constant_bias,
			.caster_slope_bias = s.shadows.caster_slope_bias,
			.transition_fraction = s.shadows.transition_fraction,
			.distance_fade_fraction = s.shadows.distance_fade_fraction,
			.debug_cascades = s.shadows.debug_cascades,
			.debug_atlas = s.shadows.debug_atlas,
			.debug_atlas_cascade = s.shadows.debug_atlas_cascade,
		};
		AeronScene_SetDirectionalShadow(s.scene, &shadow);
	}

	/* Star skybox — replaces the classic CPU pixel starfield
	 * (FlightStarfield_Render). Drawn scene-side under the SKY
	 * backdrops and all meshes. The classic gates the starfield off in
	 * the Death Star hangar scene (ds_hangar).
	 * World→cube basis: XWA world XY is the horizontal plane, +Z up
	 * (starfield elevation bins measure from XY toward ±Z; backdrop
	 * strips extend along Z) — world X→cube X, world Z→cube +Y (up),
	 * world Y→cube -Z (forward): the same proper rotation TIE uses, so
	 * shared authored cubes orient identically in both games. The
	 * procedural starfield (skybox.mode: stars) hashes stars in this same
	 * cube basis, so it orients consistently with authored cubes; its
	 * twinkle advances off the sim clock (game_time_ms), not wall time,
	 * so it is deterministic and pauses with the game. */
	static const float fl_world_to_cube[9] = {
		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,
	};
	if (!cam->ds_hangar) {
		if (s.sky_mode == 1) {
			/* Classic stars are one flight-viewport pixel; the HD scene
			 * renders the same FOV at rt_h. Scaling the classic-pixel dot
			 * geometry by this upscale makes a star match the classic
			 * footprint at any render resolution. */
			const float classic_h =
				cam->vp_h > 0 ? (float)cam->vp_h : (cam->screen_h > 0 ? (float)cam->screen_h : 480.0f);
			const float upscale = (s.rt_h > 0 && classic_h > 0.0f) ? (float)s.rt_h / classic_h : 1.0f;
			const XwaRemasterSkyStarsParams stars = {
				.exposure = s.sky_exposure,
				.brightness = s.star_brightness,
				.density = s.star_density,
				.grid_n = s.star_grid,
				.core_radius_px = s.star_core_px * upscale,
				.feather_px = s.star_feather * upscale,
				.pixel_pitch_px = upscale,
				.flare_strength = s.star_flare,
				.game_time_ms = (uint32_t)snap->game_time_ms,
			};
			if (!XwaRemasterSkyStars_Prepare(s.sky_stars, s.scene, fl_world_to_cube, &stars)) {
				Aeron_CommandBufferSetFailure(cmd, "Procedural starfield preparation failed");
				s.snap = NULL;
				return NULL;
			}
			s.before_opaque_mode = FL_BEFORE_OPAQUE_SKY_STARS;
			AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, fl_draw_before_opaque, NULL);
		} else if (s.sky_cube) {
			AeronScene_SetSkyCube(s.scene, s.sky_cube, fl_world_to_cube, s.sky_exposure);
		}
	}

	/* Backdrops — classic draws them first with depth compare and write
	 * both OFF (states 0x612/0x2612); the SKY stage's far-depth GE test
	 * against the cleared / prepass buffer reproduces that (behind all
	 * geometry, visible elsewhere). */
	for (uint32_t i = 0; i < s.bd_count; i++) {
		fl_submit_view_quad(AERON_SCENE_BILLBOARD_STAGE_SKY, s.bd_quads[i].corners, s.bd_quads[i].uvs,
							&s.bd_quads[i].ref, 0xffffffffu, NULL);
	}
	/* Post stack: SSAO per the render config + motion blur (both
	 * activate the scene's depth/normal(+velocity) prepass + forward
	 * topology). Shutter: pause zeroes it unless the keep-blur debug
	 * knob holds it (a paused frame of motion legitimately carries
	 * blur, like pausing a video) — the TIE rule. The retained velocity
	 * is a whole simulation-snapshot displacement, so normalize it to
	 * XWA's original 32 ms logical frame before applying the shutter
	 * fraction. This preserves the calibrated step-8 appearance. */
	const int mb_allow = !paused || s.mb_pause_keep_blur;
	const float mb_shutter = mb_allow ? fl_mb_logical_frame_shutter() : 0.0f;
	AeronScene_SetPost(s.scene, &(AeronScenePostDesc) {
									.ssao_quality = s.ssao.quality,
									.ssao_intensity = s.ssao.intensity,
									.ssao_power = s.ssao.power,
									.ssao_radius_view = s.ssao.radius_view,
									.ssao_bias_view = s.ssao.bias_view,
									.ssao_direct = s.ssao.direct,
									.ssao_debug_viz = s.ssao.debug_viz,
									.ssao_min_screen_frac = s.ssao.min_screen_frac,
									.ssao_max_screen_frac = s.ssao.max_screen_frac,
									.ssao_sample_jitter = s.ssao.sample_jitter,
									.mb_quality = mb_on ? s.mb_quality : 0,
									.mb_shutter = mb_shutter,
									.mb_camera_blur = s.mb_camera_blur,
									.mb_velocity_viz = s.mb_velocity_viz,
									.mb_fsr_direct_motion = s.mb_fsr_direct_motion,
								});
	AeronScene_SetMotionContext(s.scene, s.mb_enabled ? s.mb_prev_vp : NULL, mb_regen);

	/* The lit PBR env is submitted AFTER the instance walk (frame
	 * uniforms are copied at submit and bound inside Render) so the
	 * walk can contribute the engine-glow point lights. */
	int render_w, render_h;
	AeronScene_RenderDims(s.scene, &render_w, &render_h);
	const XwaShipAoParams ao = {
		.intensity = s.ssao.intensity,
		.power = s.ssao.power,
		.rt_w = (float)render_w,
		.rt_h = (float)render_h,
		.direct = s.ssao.direct,
	};
	const int ao_on = s.ssao.quality > 0 && s.ssao.intensity > 0.0f;

	s.bolt_count = 0;
	s.table_count = 0;

	/* Derive every object transform from the STATE channel: cached rows
	 * when the mobile object's cache is fresh, the Euler compose
	 * otherwise (statics re-derive per frame in the classic too). Lit
	 * models submit as scene instances; unlit projectiles collect for
	 * the BEFORE_OPAQUE hook (env swap). */
	/* Cockpit anchor: the classic hangar loop anchors the cockpit to
	 * the PLAYER object; the flight loop to the camera FOCUS object. */
	const int32_t anchor_slot = cam->in_hangar ? cam->player_obj_idx : cam->focus_obj_idx;
	const XwaFlightObject* player_f = NULL;
	float player_bw[9];
	float player_local[3];
	int player_found = 0;
	for (uint32_t i = 0; i < snap->flight_object_count; i++) {
		const XwaFlightObject* f = &snap->flight_objects[i];
		if (f->region != cam->region || f->genus == XWA_SNAP_GENUS_EXPLOSION) {
			continue; /* other regions; explosions are billboards */
		}
		/* Transforms derive for every in-region object — the cockpit
		 * anchors to one even when its own model is suppressed. */
		float cur[3][3];
		if (f->has_mobj && !f->orient_dirty) {
			fl_curmat_from_cached(f->rows, cur);
		} else {
			fl_curmat_from_euler(f, cur);
		}
		/* Projectiles: classic roll alignment
		 * (RenderBillboard_DrawRollAlignedObjectModel @0x40FA80): spin
		 * the flat ribbon about its flight axis so the broad face
		 * tracks the camera — roll += arctan(up.d, side.d) - 0x4000
		 * with d = camera - object, applied about the same axis the
		 * Euler roll composes on (cur[2]; rotations about one axis
		 * commute, so the post-rotation equals the classic full
		 * recompose with the adjusted roll). NO minimum-size clamp:
		 * the classic draws the model as-is (sub-pixel bolts flicker
		 * in the original too, and at 4.5x the classic resolution
		 * anything classic-visible covers >= 4.5 px here). */
		const int is_bolt =
			f->genus == XWA_SNAP_GENUS_PLAYER_PROJECTILE || f->genus == XWA_SNAP_GENUS_NPC_PROJECTILE;
		if (is_bolt) {
			float d[3];
			XwaRemasterWorld_DeltaI32(cam->world_pos, f->world_pos, d);
			const float side_dot = d[0] * cur[0][0] + d[1] * cur[0][1] + d[2] * cur[0][2];
			const float up_dot = d[0] * cur[1][0] + d[1] * cur[1][1] + d[2] * cur[1][2];
			if (side_dot != 0.0f || up_dot != 0.0f) {
				const int dq16 = (int)lrintf(atan2f(up_dot, side_dot) / FL_Q16_TO_RAD) - 0x4000;
				fl_transformaxes(cur, cur[2], dq16);
			}
		}
		float bw[9];
		float object_local[3];
		fl_object_world(cur, bw);
		fl_object_local(f, object_local);
		if ((int32_t)f->slot == cam->player_obj_idx) {
			player_f = f; /* cockpit glow scale/knockouts key to the player */
			memcpy(player_bw, bw, sizeof player_bw);
			memcpy(player_local, object_local, sizeof player_local);
			player_found = 1;
		}
		/* Debris chunks (type 222, GENUS_Debris): the classic redirects
		 * the draw to the SOURCE craft's model with ONLY the detached
		 * component's mesh drawn, spinning about the component focus
		 * point (RenderScene_DrawNoAssetSourceModel). Transient-slot
		 * debris hides in external view (render-list build rule). Bare
		 * model only — no engine glows, no craft articulation. */
		if (f->object_type == XWA_SNAP_TYPE_DEBRIS_CHUNK) {
			if (cam->external && f->slot_class == XWA_SNAP_SLOT_TRANSIENT) {
				continue;
			}
			AeronSceneMesh* chunk_mesh =
				XwaRemasterShip_MeshForName(XwaSnapshotExport_ModelName(f->source_object_type));
			AeronSceneMeshTable* tb = &s.tables[s.table_count];
			if (!chunk_mesh || !XwaRemasterShip_BuildDebrisMeshTable(f, tb)) {
				continue; /* never fall through to a whole-model draw */
			}
			s.table_count++;
			AeronSceneMeshInstance inst;
			memset(&inst, 0, sizeof inst);
			inst.mesh = chunk_mesh;
			float m[16];
			fl_model_matrix(bw, object_local, m);
			memcpy(inst.transform, m, sizeof m);
			inst.variant = f->node_switch; /* markings survive the detach copy */
			inst.mesh_table = tb;
			inst.no_local_lights = 1;
			inst.cull_mode = AERON_CULL_BACK;
			fl_instance_motion(&inst, i, /*is_bolt=*/0);
			AeronScene_AddMeshInstance(s.scene, &inst);
			continue;
		}
		/* Classic internal view hides the anchor exterior. Keep deriving it
		 * while directional shadows are active so it can enter the
		 * shadow-only queue without becoming visible. */
		const int hide_object_model = !cam->external && !cam->film_overlay && (int32_t)f->slot == anchor_slot;
		if (hide_object_model && !shadows_active) {
			continue;
		}
		/* Classic external view swaps the PLAYER object's model to the
		 * "<name>Exterior.opt" variant (the flyable OPT has no cockpit
		 * detail); flight and hangar loops both apply it. */
		AeronSceneMesh* mesh;
		int runtime_opt = 0;
		if (cam->external && (int32_t)f->slot == cam->player_obj_idx &&
			snap->cockpit.exterior_name[0] != '\0') {
			mesh = XwaRemasterShip_MeshForNameWithSource(snap->cockpit.exterior_name, &runtime_opt);
		} else {
			mesh = XwaRemasterShip_MeshForNameWithSource(XwaSnapshotExport_ModelName(f->object_type),
														 &runtime_opt);
		}
		if (!mesh) {
			continue;
		}

		AeronSceneMeshInstance inst;
		memset(&inst, 0, sizeof inst);
		inst.mesh = mesh;
		float m[16];
		fl_model_matrix(bw, object_local, m);
		memcpy(inst.transform, m, sizeof m);
		inst.variant = f->node_switch;
		inst.no_local_lights = 1; /* frame-global punctual lights instead */
		if (scene_lighting.hangar_override && (int32_t)f->slot == cam->hangar_launch_ref_obj_idx) {
			/* The enclosed backdrop contains the implied ceiling emitter.
			 * It receives object shadows but must not occlude that light. */
			inst.shadow_flags |= AERON_SCENE_INSTANCE_NO_CAST_SHADOW;
		}
		if (is_bolt) {
			inst.shadow_flags |= AERON_SCENE_INSTANCE_NO_CAST_SHADOW | AERON_SCENE_INSTANCE_NO_RECEIVE_SHADOW;
		}
		if (is_bolt && runtime_opt) {
			/* The original renderer makes both projectile genera fully
			 * bright independently of the OPT palette. Runtime conversion
			 * has no such semantic, so render its base texture as HDR
			 * emission; authored GLBs retain their material-only path. */
			inst.base_color_emissive_strength = XwaRemasterShip_OptProjectileEmissiveStrength();
		}
		inst.cull_mode = AERON_CULL_BACK;
		const XwaFlightObject* previous_f = fl_instance_motion(&inst, i, is_bolt);
		/* Rotary-mesh articulation (cranes, radar dishes, droid arms)
		 * from the captured craft state — tables borrow pool slots for
		 * the frame (the scene keeps the pointer until Render). */
		AeronSceneMeshTable* tb = &s.tables[s.table_count];
		if (XwaRemasterShip_BuildMeshTable(mesh, f, tb)) {
			inst.mesh_table = tb;
			s.table_count++;
		}
		AeronSceneMeshTable* prev_tb = &s.tables[s.table_count];
		if (XwaRemasterShip_BuildPreviousMeshTable(mesh, f, previous_f, prev_tb)) {
			inst.prev_mesh_table = prev_tb;
			s.table_count++;
		}
		if (hide_object_model) {
			const int is_hidden_player = (int32_t)f->slot == cam->player_obj_idx;
			if (is_hidden_player) {
				inst.shadow_flags |= AERON_SCENE_INSTANCE_EXCLUDE_FROM_RECEIVER_LOCAL_SHADOW;
			}
			AeronScene_AddShadowCaster(s.scene, &inst);
			continue;
		}
		if (is_bolt) {
			/* Classic OPT bolts are double-sided flat ribbons; the roll
			 * alignment can leave the broad face wound either way. The
			 * BLEND-classified ribbon never enters the depth prepass, so
			 * its motion vectors come from the alpha-tested velocity
			 * STAMP sweep instead. */
			inst.cull_mode = AERON_CULL_NONE;
			inst.velocity_stamp = 1;
			s.bolts[s.bolt_count++] = inst;
		} else {
			AeronScene_AddMeshInstance(s.scene, &inst);
			XwaRemasterGlowMarks_SubmitObject(s.scene, cmd, assets, snap, f, mesh, m, inst.mesh_table);
			/* State-derived engine glows for this craft (classic scale
			 * gates — no craft / dead subsystems — return 0 and skip). */
			if (s.glow_ok) {
				XwaRemasterShip_SubmitEngineGlows(s.scene, mesh, m, 1.0f, inst.mesh_table,
												  f->eg_knockout_mask, XwaRemasterShip_EngineGlowScale(f),
												  s.crows, s.camera_local, &s.glow_ref);
			}
			/* Capital engine glows double as point lights (the classic
			 * modelIndex branch: raw dims over 2000 only). */
			if (s.plight.enabled && f->has_craft && s.pl_cand_count < FL_MAX_PL_CANDIDATES) {
				s.pl_cand_count += XwaRemasterShip_CollectEngineGlowPointLights(
					mesh, m, inst.mesh_table, f, &s.pl_cand[s.pl_cand_count],
					FL_MAX_PL_CANDIDATES - s.pl_cand_count);
			}
		}
	}

	/* Projectile instances submit AFTER every ship: instances draw in
	 * submission order, and the bolts' alpha-BLEND ranges (soft-edged
	 * classic bolt textures) neither write depth nor enter the prepass,
	 * so they only composite correctly against the complete opaque
	 * depth. The cockpit submits after them, keeping the canopy-glass
	 * tint over bolts seen through it. (Re-authored OPAQUE emissive
	 * bolts are order-independent; this ordering stays harmless.) */
	for (uint32_t bi = 0; bi < s.bolt_count; bi++) {
		AeronScene_AddMeshInstance(s.scene, &s.bolts[bi]);
	}

	/* Weapon-fire pulse lights (player basis known after the walk),
	 * then the finalized punctual set + the lit PBR env. */
	if (player_found && player_f) {
		fl_derive_pulse_lights(snap, player_bw, player_local);
	}
	XwaShipPointLightTuning point_tuning;
	const uint32_t point_count = fl_finalize_point_lights(&point_tuning);
	fl_submit_point_lights(point_count);
	XwaRemasterShip_SetPbrEnv(s.scene, scene_lighting.lights, scene_lighting.light_count, NULL,
							  s.camera_local, ao_on ? &ao : NULL,
							  point_count ? &point_tuning : NULL,
							  scene_lighting.ambient);

	/* Player cockpit (lit; classic runs the normal light setup). Gates
	 * mirror the classic draw sites: the hangar draws it whenever the
	 * view is internal; flight requires the cockpit toggle plus the
	 * seat's availability flag. Transform derived from STATE: the
	 * anchor object's basis, eye anchored at -(seat hardpoint + camera
	 * pan / 16) rotated into eye space (rotation only — the classic
	 * g_cockpitViewActive override). Turret seats rotate the cockpit's
	 * rotary meshes by the captured aim. */
	if (fl_cockpit_drawn(snap, cam) && cockpit_anchor) {
		AeronSceneMesh* cockpit_mesh = XwaRemasterShip_MeshForName(snap->cockpit.model_name);
		if (cockpit_mesh) {
			float m[16];
			fl_cockpit_model_matrix(&snap->cockpit, cockpit_anchor, s.crows, s.camera_local, m);
			AeronSceneMeshInstance inst;
			memset(&inst, 0, sizeof inst);
			inst.mesh = cockpit_mesh;
			memcpy(inst.transform, m, sizeof m);
			memcpy(inst.prev_transform, m, sizeof m);
			inst.no_local_lights = 1;
			if (cockpit_history_valid &&
				fl_cockpit_model_matrix(&prev->cockpit, previous_cockpit_anchor, previous_crows,
										previous_flight_view.camera.pos, inst.prev_transform)) {
				inst.zero_velocity = 0;
			} else {
				/* A history reset makes the zero-vector fallback correct for the
				 * first frame after a cockpit/view discontinuity. */
				inst.zero_velocity = 1;
			}
			inst.cull_mode = AERON_CULL_BACK;
			inst.shadow_flags =
				AERON_SCENE_INSTANCE_NO_CAST_SHADOW | AERON_SCENE_INSTANCE_USE_RECEIVER_LOCAL_SHADOW;
			AeronSceneMeshTable* tb = &s.tables[s.table_count];
			if (XwaRemasterShip_BuildCockpitMeshTable(cockpit_mesh, &snap->cockpit, tb)) {
				inst.mesh_table = tb;
				s.table_count++;
			}
			if (cockpit_history_valid && prev->cockpit.seat != 0 && cockpit_mesh->has_any_rotation) {
				AeronSceneMeshTable* prev_tb = &s.tables[s.table_count];
				/* The builder initializes an identity table even when all previous
				 * aim angles are zero; retain it so a newly rotating turret does
				 * not incorrectly reuse its current articulation as history. */
				XwaRemasterShip_BuildCockpitMeshTable(cockpit_mesh, &prev->cockpit, prev_tb);
				inst.prev_mesh_table = prev_tb;
				s.table_count++;
			}
			AeronScene_AddMeshInstance(s.scene, &inst);
			if (player_f) {
				XwaRemasterGlowMarks_SubmitObject(s.scene, cmd, assets, snap, player_f, cockpit_mesh, m,
												  inst.mesh_table);
			}
			/* Cockpit-model engine glows (classic: the cockpit glow set
			 * when internal + cockpit visible), scale/knockouts from
			 * the PLAYER craft's state. */
			if (s.glow_ok && player_f) {
				XwaRemasterShip_SubmitEngineGlows(
					s.scene, cockpit_mesh, m, 1.0f, inst.mesh_table, player_f->eg_knockout_mask,
					XwaRemasterShip_EngineGlowScale(player_f), s.crows, s.camera_local, &s.glow_ref);
			}
		}
	}

	/* Classic effects order: particles, persistent warhead trails, then
	 * queued scene billboards. */
	XwaRemasterParticles_SubmitRegion(s.particles, s.scene, snap, cam->region);
	XwaRemasterTrails_SubmitRegion(s.trails, s.scene, cam->region);

	/* State-derived explosion/debris/spark sprites and wreck flames
	 * (OVERLAY stage, back-to-front; engine glows submit per instance
	 * above, in the classic per-object order). */
	fl_submit_billboards();
	/* Lens-flare trains (LENS stage — the scene draws them after its
	 * color passes with GPU anchor occlusion, before bloom). */
	fl_submit_lens_flares(snap, &flight_view);

	if (!AeronScene_Render(s.scene, cmd)) {
		AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, NULL, NULL);
		s.before_opaque_mode = FL_BEFORE_OPAQUE_NONE;
		s.snap = NULL;
		return NULL;
	}
	AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, NULL, NULL);
	s.before_opaque_mode = FL_BEFORE_OPAQUE_NONE;
	s.snap = NULL; /* hooks must not outlive the snapshot */
	return fl_present(cmd, direct_present);
}

void XwaRemasterFlight_Shutdown(void) {
	XwaRemasterHyperspace_Destroy(s.hyperspace);
	s.hyperspace = NULL;
	XwaRemasterSkyStars_Destroy(s.sky_stars);
	s.sky_stars = NULL;
	XwaRemasterParticles_Destroy(s.particles);
	s.particles = NULL;
	XwaRemasterTrails_Destroy(s.trails);
	s.trails = NULL;
	XwaRemasterGlowMarks_Shutdown();
	if (s.sky_cube) {
		Aeron_DestroyTexture(s.sky_cube);
	}
	if (s.present_sampler) {
		Aeron_DestroySampler(s.present_sampler);
	}
	fl_destroy_sized_resources();
	if (s.mesh_sampler) {
		Aeron_DestroySampler(s.mesh_sampler);
	}
	if (s.present) {
		AeronScenePresentChain_Destroy(s.present);
	}
	if (s.present_direct) {
		AeronScenePresentChain_Destroy(s.present_direct);
	}
	memset(&s, 0, sizeof s);
}
