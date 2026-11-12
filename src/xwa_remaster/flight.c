/* Flight-scene HD driver — see xwa_remaster/flight.h. */

#include "xwa_remaster/flight.h"
#include "xwa_remaster/dynamic_cockpit.h"
#include "xwa_remaster/hud_fixed.h"
#include "xwa_remaster/hud_readout.h"

#include "aeron/aeron.h"
#include "aeron/asset/opt_model.h"
#include "aeron/config_file.h"
#include "aeron/scene/billboard.h"
#include "aeron/scene/bloom.h"
#include "aeron/scene/environment.h"
#include "aeron/scene/image_cache.h"
#include "aeron/scene/present.h"
#include "aeron/scene/scene3d.h"
#include "aeron/scene/settings.h"
#include "aeron/scene/world.h"
#include "xwa_remaster/captured_lighting.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/effects.h"
#include "xwa_remaster/environment.h"
#include "xwa_remaster/flight_map.h"
#include "xwa_remaster/glow_marks.h"
#include "xwa_remaster/hud.h"
#include "xwa_remaster/hyperspace.h"
#include "xwa_remaster/probe_outlines.h"
#include "xwa_remaster/ship.h"
#include "xwa_remaster/skins.h"
#include "xwa_remaster/sky_stars.h"
#include "xwa_remaster/speed_effect.h"
#include "xwa_remaster/suns.h"
#include "xwa_remaster/xwa_remaster.h"
#include "xwa_runtime/config/modern_camera_shake_options.h"
#include "xwa_runtime/models/cockpit_lights.h"
#include "xwa_runtime/models/particle_lights.h"
#include "xwa_runtime/models/cockpit_view.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/runtime/presentation.h"
#include "xwa_runtime/timing/host_clock.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FL_MB_REFERENCE_FRAME_US 32000u
/* A game tick is 4 ms of game time. */
#define FL_MB_GAME_TICK_US 4000u
#define FL_DS_LASER_TRANSVERSE_SCALE 100.0f

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

/* The resets that are cuts for the eye too, where the metered exposure
 * takes its target at once instead of adapting. Region changes, hyperspace
 * and the hangar door adapt. */
#define FL_EXPOSURE_SNAP_REASONS                                                                             \
	(FL_FSR_RESET_INITIAL | FL_FSR_RESET_NO_PREVIOUS_SNAPSHOT | FL_FSR_RESET_PREVIOUS_MAP |                  \
	 FL_FSR_RESET_EXTERNAL_VIEW_CHANGE | FL_FSR_RESET_FILM_OVERLAY_CHANGE |                                  \
	 FL_FSR_RESET_GAME_TIME_ROLLBACK | FL_FSR_RESET_COCKPIT_MODEL | FL_FSR_RESET_RENDER_RESUMED)

#define FL_MAX_BACKDROP_QUADS (XWA_SNAP_MAX_BACKDROPS * (XWA_SNAP_MAX_STRIP_COORDS - 1))
typedef struct FlBackdropQuad {
	float corners[4][3];
	float uvs[4][2];
	XwaAssetRef ref;
	/* The snapshot index of the backdrop sun that the quad draws, or -1 for
	 * another backdrop. */
	int sun;
} FlBackdropQuad;

/* One state-derived scene billboard (explosion/spark/debris/chaff
 * sprite, wreck flame), resolved before AeronScene_Begin (atlas
 * uploads precede the passes) and submitted back-to-front. */
typedef struct FlBillboard {
	float center[3];         /* view space; [2] is the adjusted depth */
	float half_w, half_h;    /* view units at that depth */
	float rot;               /* roll about the view axis, radians */
	float depth_bias;        /* view units toward the camera (test only) */
	float emissive_strength; /* linear-RGB multiplier; alpha is unchanged */
	XwaAssetRef ref;
	uint32_t argb;
	/* Motion blur: complete PREV-frame geometry in the current frame's
	 * render-local coordinate system. This preserves animation-frame
	 * dimensions, scale/depth laws, roll and the selected camera-facing
	 * basis rather than approximating history with a translation. */
	float prev_corners[4][3];
	uint8_t has_prev;
} FlBillboard;

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

/* Bounded source pools: the game's light candidates, and the scene lights
 * (those and the models' lights) ranked into Aeron's capacity. */
#define FL_MAX_PL_CANDIDATES 1024
#define FL_MAX_PL_POOL 1024
/* A kept light whose score is under this many times the best dropped
 * light's fades toward it, so ranks swapping at the cut do not pop. */
#define FL_PL_FADE_BAND 2.0f

/* A scene light in the pool, with its rank score (the illuminance it can
 * give the camera's nearest point; INFINITY always keeps it). */
typedef struct FlPoolLight {
	AeronSceneLight light;
	float score;
	int model; /* authored in a model */
} FlPoolLight;

typedef enum FlBeforeOpaqueMode {
	FL_BEFORE_OPAQUE_NONE = 0,
	FL_BEFORE_OPAQUE_HYPERSPACE,
	FL_BEFORE_OPAQUE_SKY_STARS,
} FlBeforeOpaqueMode;

/* The energy effect an object slot last showed: the object (signature),
 * the effect, and when it started (animation seconds). */
typedef struct FlEnergyState {
	uint16_t signature;
	uint8_t energy;
	uint8_t valid;
	float start;
} FlEnergyState;

/* How long before now an effect starts when it was already showing when
 * the object first appeared. A `once` effect is then over, and a loop is
 * at some point of its cycle. */
#define FL_ENERGY_SETTLED_SECONDS 3600.0f

static uint32_t fl_energy(const XwaFlightObject* object);

/* Elements in blocks that never move, so pointers to them stay valid while
 * the pool grows. A frame empties the pool and keeps its blocks. */
typedef struct FlBlockPool {
	uint8_t** blocks;
	uint32_t block_count;
	uint32_t used;
} FlBlockPool;

enum { FL_TABLES_PER_BLOCK = 64, FL_ANIMS_PER_BLOCK = 256 };

/* Material states (bit per AERON_ANIM_STATE_*) and when each last fired,
 * in animation seconds. */
typedef struct FlStateOnsets {
	uint32_t mask;
	float onset[32];
} FlStateOnsets;

/* The player's target event, one at a time. */
enum {
	FL_TARGET_NONE,
	FL_TARGET_SELECTED,
	FL_TARGET_LASER_LOCK,
	FL_TARGET_WARHEAD_LOCKING,
	FL_TARGET_WARHEAD_LOCKED,
};

/* The scene owns the pass topology (prepass / SSAO / forward color /
 * motion-blur resolve — AeronScene_Render); this driver submits mesh
 * instances and draws the non-mesh geometry through the scene's pass
 * hooks. `snap`/`assets` and the derived per-frame state below are the
 * hook context, valid during AeronScene_Render only. */
static struct {
	AeronScene3D* scene;
	/* scene_serial counts the scenes created. The prepared_ fields record
	 * the scene and the mesh set whose pipelines fl_prepare_meshes last
	 * created. */
	uint64_t scene_serial;
	uint64_t prepared_scene_serial;
	uint64_t prepared_mesh_generation;
	XwaRemasterTrails* trails;
	XwaRemasterParticles* particles;
	XwaRemasterHyperspace* hyperspace;
	XwaRemasterSkyStars* sky_stars;
	XwaRemasterSpeedEffect* speed_effect;
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

	/* SSAO knobs loaded from the mandatory remaster/config.yaml resource, then
	 * live-mutated by the debug
	 * inspector via the XwaRemasterFlight_GetSsao/SetSsao accessors. */
	XwaFlightSsaoParams ssao;
	XwaFlightSsaoParams ssao_default;
	int config_loaded;
	XwaFlightShadowParams shadows;
	XwaFlightShadowParams shadows_default;
	int shadow_debug_atlas;
	int shadow_debug_atlas_cascade;
	XwaFlightHangarLightingParams hangar_lighting;
	XwaFlightHangarLightingParams hangar_lighting_default;
	XwaFlightExposureParams exposure;
	XwaFlightExposureParams exposure_default;
	int present_debug_view; /* AeronScenePresentDebugView */
	/* Point-light calibration knobs (same lifecycle as ssao). */
	XwaFlightPointLightParams plight;
	XwaFlightPointLightParams plight_default;
	float explosion_genus_emissive_strength;
	float glow_mark_emissive_strength;
	float cockpit_glow_mark_emissive_strength;
	/* The same, for the marks in world segment mode on the cockpit model:
	 * the shield and warhead hit flashes, which cover the whole cockpit. */
	float cockpit_hit_flash_emissive_strength;
	/* small_objects in remaster/config.yaml, in pixels of a 1080-line view. */
	float small_prepass_min_size;
	float small_draw_min_size;
	float map_ambient_min[3]; /* lighting.map_ambient_min_* */
	float death_star_ambient[3]; /* lighting.death_star_ambient_* */
	/* presentation.cockpit_fov_min_aspect: with a craft's field of view, a
	 * window narrower than this aspect gets a wider view, so that the
	 * horizontal field of view stays the one at this aspect. 0 turns it
	 * off. */
	float cockpit_fov_min_aspect;
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
	float mb_prev_ori[4];
	const XwaSnapshot* mb_prev_snap;
	int32_t* mb_prev_index;
	uint32_t mb_prev_index_capacity;
	/* Time covered by the retained velocity buffer: the game time between
	 * the two snapshots, or the host time when game time didn't move. */
	uint64_t mb_pose_host_us;
	uint64_t mb_velocity_span_us;
	int mb_pose_host_valid;

	int process_assets_prepared;
	/* Starfield exposure (starfield.exposure). */
	float sky_exposure;
	/* Environment selection resolved at the start of each flight frame. */
	XwaEnvironmentSelection environment;
	/* Probe sets placed this frame: the walk index of each owner object,
	 * the placed sets, and copies of their info for the debug rows. */
	uint32_t probe_set_object[AERON_SCENE_MAX_PROBE_SETS];
	XwaProbeOutlineSet probe_sets[AERON_SCENE_MAX_PROBE_SETS];
	XwaFlightProbeSetInfo probe_set_info[AERON_SCENE_MAX_PROBE_SETS];
	uint32_t probe_set_count;
	float star_brightness;
	float star_density;
	float star_grid;
	/* Star dot geometry in *classic viewport pixels* — multiplied by the
	 * viewport→render upscale factor at draw time so a star matches the
	 * classic single-pixel footprint at any render resolution. */
	float star_core_px;
	float star_feather;
	float star_flare;
	XwaFlightHyperspaceTunnelParams hyperspace_params;
	XwaFlightHyperspaceTunnelParams hyperspace_params_default;
	XwaFlightHyperspaceJumpParams hyperspace_jump;
	XwaFlightHyperspaceJumpParams hyperspace_jump_default;
	XwaFlightHyperspaceCameraParams hyperspace_camera;
	XwaFlightHyperspaceCameraParams hyperspace_camera_default;
	char hyperspace_look_names[XWA_FLIGHT_HYPERSPACE_LOOKS_MAX][XWA_FLIGHT_HYPERSPACE_LOOK_NAME_MAX];
	XwaFlightHyperspaceLookParams hyperspace_looks[XWA_FLIGHT_HYPERSPACE_LOOKS_MAX];
	XwaFlightHyperspaceLookParams hyperspace_looks_default[XWA_FLIGHT_HYPERSPACE_LOOKS_MAX];
	int hyperspace_look_count;
	/* The last unknown look name, so that it is only logged once. */
	char hyperspace_unknown_look[XWA_FLIGHT_HYPERSPACE_LOOK_NAME_MAX];
	int hyperspace_preview_look;
	float hyperspace_preview_look_blend;
	XwaFlightHyperspacePreview hyperspace_preview;
	float hyperspace_preview_speed;
	/* The preview's clock, in seconds of preview time, and the host time it
	 * was last advanced at. */
	double hyperspace_preview_seconds;
	uint64_t hyperspace_preview_host_us;
	/* The camera rotation of the preview's last frame. The preview has no
	 * previous snapshot to compute it from. */
	float hyperspace_preview_rotation[9];
	uint8_t hyperspace_preview_rotation_phase;
	int hyperspace_preview_rotation_valid;
	/* Set by the hyperspace scene: the exposure meter keeps its value. */
	int exposure_hold;
	/* The jump's entry or exit, drawn over the normal flight scene this
	 * frame (docs/hd_hyperspace_jump.md). */
	int hyperspace_world_pending;
	XwaRemasterHyperspaceMoment hyperspace_world_moment;
	int hyperspace_in_world;
	/* The scene warp's view-space near plane and keep depth this frame, and
	 * its debug view. */
	float hyperspace_near_z;
	float hyperspace_keep_depth;
	XwaFlightHyperspaceWarpView hyperspace_warp_view;
	/* The region's sky cube for the scene warp this frame
	 * (fl_note_hyperspace_sky): only its sky and overlay fields are set. */
	XwaRemasterHyperspaceWarpInput hyperspace_sky;
	/* The scene warp's image, presented instead of the scene this frame. */
	AeronTexture* present_scene_override;

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
	/* The frame's billboards. Aeron copies a billboard when it is added, so
	 * the array can grow. */
	FlBillboard* bb;
	uint32_t bb_count;
	uint32_t bb_capacity;
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
	 * x intensity premultiplied; knobs applied at finalize). */
	XwaShipPointLight pl_cand[FL_MAX_PL_CANDIDATES];
	uint32_t pl_cand_count;
	uint32_t pl_cand_dropped;
	uint32_t pl_invalid_count;
	/* The valid candidates and the models' lights as scene lights, culled
	 * by the view (frustum planes, camera position), ranked at submit. */
	FlPoolLight pl_pool[FL_MAX_PL_POOL];
	uint32_t pl_pool_count;
	float pl_frustum[5][4];
	float pl_eye[3];
	uint32_t pl_model_count;
	uint32_t pl_culled[2]; /* [model] */
	uint32_t pl_dropped[2];

	/* Projectile instances, collected during the walk and submitted after
	 * every ship. Roll-aligned per the classic law at transform derive.
	 * Aeron copies an instance when it is added, so the array can grow. */
	AeronSceneMeshInstance* bolts;
	uint32_t bolt_count;
	uint32_t bolt_capacity;

	/* AeronScene_AddMeshInstance borrows the mesh table and animation
	 * pointers until AeronScene_Render returns, so they come from pools
	 * whose blocks never move (FlBlockPool). */
	FlBlockPool table_pool;
	FlBlockPool anim_pool;
	/* Energy effect per object slot, for the slots of the snapshot's object
	 * table, and the mission they belong to. */
	FlEnergyState* energy_states;
	uint32_t energy_state_count;
	char energy_mission[sizeof(((XwaSnapshot*)0)->mission_path)];
	XwaFlightMaterialAnimOverride anim_override;
	/* Player states and the debug override's, with the animation time they
	 * were last updated at; the target event and the threat indicators as
	 * the classic HUD last drew them. */
	FlStateOnsets player_states;
	FlStateOnsets override_states;
	/* The debug override's energy effect and when it started. */
	uint32_t override_energy;
	float override_energy_start;
	float states_time;
	uint8_t target_event;
	uint8_t states_in_hangar;
	XwaHudThreats threats;
	int map_present;
	int suppress_hud;
} s;

/* The next element of `pool`, without taking it (pool->used++ takes it), or
 * NULL when out of memory. */
static void* fl_pool_peek(FlBlockPool* pool, size_t element_size, uint32_t per_block) {
	const uint32_t block = pool->used / per_block;
	if (block == pool->block_count) {
		uint8_t** blocks = realloc(pool->blocks, (size_t)(block + 1) * sizeof *blocks);
		if (!blocks) {
			return NULL;
		}
		pool->blocks = blocks;
		blocks[block] = malloc(element_size * per_block);
		if (!blocks[block]) {
			return NULL;
		}
		pool->block_count++;
	}
	return pool->blocks[block] + (size_t)(pool->used % per_block) * element_size;
}

static void fl_pool_free(FlBlockPool* pool) {
	for (uint32_t i = 0; i < pool->block_count; i++) {
		free(pool->blocks[i]);
	}
	free(pool->blocks);
	memset(pool, 0, sizeof *pool);
}

static AeronSceneMeshTable* fl_table_peek(void) {
	return fl_pool_peek(&s.table_pool, sizeof(AeronSceneMeshTable), FL_TABLES_PER_BLOCK);
}

static void fl_push_bolt(const AeronSceneMeshInstance* inst) {
	if (s.bolt_count == s.bolt_capacity) {
		const uint32_t capacity = s.bolt_capacity ? s.bolt_capacity * 2 : 256;
		AeronSceneMeshInstance* bolts = realloc(s.bolts, (size_t)capacity * sizeof *bolts);
		if (!bolts) {
			return;
		}
		s.bolts = bolts;
		s.bolt_capacity = capacity;
	}
	s.bolts[s.bolt_count++] = *inst;
}

static int fl_reserve_prev_index(uint32_t count) {
	if (count > s.mb_prev_index_capacity) {
		int32_t* index = realloc(s.mb_prev_index, (size_t)count * sizeof *index);
		if (!index) {
			return 0;
		}
		s.mb_prev_index = index;
		s.mb_prev_index_capacity = count;
	}
	return 1;
}

/* Gives the energy states one entry per slot of the snapshot's object table,
 * clears them when the mission changes, and updates them for every craft of
 * the snapshot, drawn or not. An effect starts when a craft's effect
 * changes. A craft seen for the first time, or after the clock went back,
 * gets its current effect with a start long before now, so a craft that
 * appears without shields doesn't flash. Updating twice for one snapshot
 * changes nothing. */
static void fl_prepare_energy_states(const XwaSnapshot* snap) {
	if (strcmp(s.energy_mission, snap->mission_path) != 0) {
		snprintf(s.energy_mission, sizeof s.energy_mission, "%s", snap->mission_path);
		if (s.energy_states) {
			memset(s.energy_states, 0, (size_t)s.energy_state_count * sizeof *s.energy_states);
		}
	}
	if (snap->flight_slot_count > s.energy_state_count) {
		FlEnergyState* states = realloc(s.energy_states, (size_t)snap->flight_slot_count * sizeof *states);
		if (!states) {
			return;
		}
		memset(states + s.energy_state_count, 0,
			   (size_t)(snap->flight_slot_count - s.energy_state_count) * sizeof *states);
		s.energy_states = states;
		s.energy_state_count = snap->flight_slot_count;
	}
	const float now = (float)((double)snap->game_time_ticks * XWA_SNAP_GAME_TICK_SECONDS);
	const uint32_t* craft = snap->flight_lists[XWA_SNAP_LIST_CRAFT];
	for (uint32_t i = 0; i < snap->flight_list_count[XWA_SNAP_LIST_CRAFT]; i++) {
		const XwaFlightObject* object = &snap->flight_objects[craft[i]];
		if (!object->has_craft || object->slot >= s.energy_state_count) {
			continue;
		}
		FlEnergyState* state = &s.energy_states[object->slot];
		const uint32_t energy = fl_energy(object);
		if (!state->valid || state->signature != object->signature || now < state->start) {
			state->start = now - FL_ENERGY_SETTLED_SECONDS;
		} else if (energy != state->energy) {
			state->start = now;
		}
		state->signature = object->signature;
		state->energy = (uint8_t)energy;
		state->valid = 1;
	}
}

/* XWA world -> cube basis of the procedural starfield (see the starfield
 * comment in XwaRemasterFlight_Render). */
static const float fl_world_to_cube[9] = {
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,
};

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
	if ((reasons & FL_EXPOSURE_SNAP_REASONS) && s.scene) {
		AeronScene_SnapExposure(s.scene);
	}
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
			Aeron_LogDebug("xwa.remaster", "FSR history reset condition cleared after %u consecutive frames",
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

	Aeron_LogDebug("xwa.remaster", "FSR history reset frame %u: %s", s.fsr_consecutive_reset_frames,
				   text[0] ? text : "unknown reason");
}

static int fl_cfg_has_type(const AeronConfigFile* config, const char* path, AeronConfigNodeType type) {
	const AeronConfigNodeType actual = AeronConfigNode_Type(AeronConfigFile_GetNode(config, path));
	return actual == type || (type == AERON_CONFIG_FLOAT && actual == AERON_CONFIG_INT);
}

static int fl_cfg_validate(const AeronConfigFile* config, const char** error_path) {
	static const char* ints[] = {
		"presentation.vsync_divisor", "presentation.frames_in_flight",     "presentation.msaa_samples",
		"motion_blur.quality",        "point_lights.cluster_depth_slices", "hyperspace_jump.star_count",
		"hyperspace_jump.warp_taps",  "hyperspace_jump.warp_sky_taps",
	};
	static const char* floats[] = {
		"presentation.cockpit_fov_min_aspect",
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
		"exposure.ev100",
		"exposure.compensation",
		"exposure.metering",
		"exposure.min_ev100",
		"exposure.max_ev100",
		"exposure.brighten_speed",
		"exposure.darken_speed",
		"exposure.low_percent",
		"exposure.high_percent",
		"exposure.sky_weight",
		"exposure.near_weight",
		"exposure.near_distance",
		"exposure.edge_weight",
		"point_lights.match_distance_m",
		"point_lights.range_scale",
		"point_lights.cockpit_explosion_light_scale",
		"point_lights.cockpit_explosion_distance_m",
		"starfield.exposure",
		"starfield.brightness",
		"starfield.density",
		"starfield.grid",
		"starfield.core_px",
		"starfield.feather",
		"starfield.flare",
		"hyperspace_tunnel.travel_speed",
		"hyperspace_tunnel.rotation_speed",
		"hyperspace_tunnel.noise_scale",
		"hyperspace_tunnel.brightness",
		"hyperspace_tunnel.highlight_strength",
		"hyperspace_tunnel.focal_length",
		"hyperspace_tunnel.twist",
		"hyperspace_tunnel.cap_radius",
		"hyperspace_tunnel.cap_falloff",
		"hyperspace_tunnel.mesh_ambient_strength",
		"hyperspace_tunnel.mesh_environment_roughness",
		"hyperspace_tunnel.mesh_key_strength",
		"hyperspace_tunnel.dark_color_r",
		"hyperspace_tunnel.dark_color_g",
		"hyperspace_tunnel.dark_color_b",
		"hyperspace_tunnel.body_color_r",
		"hyperspace_tunnel.body_color_g",
		"hyperspace_tunnel.body_color_b",
		"hyperspace_tunnel.highlight_color_r",
		"hyperspace_tunnel.highlight_color_g",
		"hyperspace_tunnel.highlight_color_b",
		"hyperspace_tunnel.cap_color_r",
		"hyperspace_tunnel.cap_color_g",
		"hyperspace_tunnel.cap_color_b",
		"hyperspace_jump.streak_brightness",
		"hyperspace_jump.whiteout_brightness",
		"hyperspace_jump.brightness_range",
		"hyperspace_jump.streak_width_px",
		"hyperspace_jump.width_from_brightness",
		"hyperspace_jump.width_variation",
		"hyperspace_jump.head_widening",
		"hyperspace_jump.radial_widening",
		"hyperspace_jump.tail_taper",
		"hyperspace_jump.head_taper",
		"hyperspace_jump.taper_variation",
		"hyperspace_jump.glow_strength",
		"hyperspace_jump.glow_width",
		"hyperspace_jump.tail_brightness",
		"hyperspace_jump.tail_color_r",
		"hyperspace_jump.tail_color_g",
		"hyperspace_jump.tail_color_b",
		"hyperspace_jump.stretch_scale",
		"hyperspace_jump.white_fade_seconds",
		"hyperspace_jump.tunnel_entry_brightness",
		"hyperspace_jump.tunnel_settle_seconds",
		"hyperspace_jump.mesh_light_strength",
		"hyperspace_jump.mesh_light_start",
		"hyperspace_jump.mesh_light_curve",
		"hyperspace_jump.tint_r",
		"hyperspace_jump.tint_g",
		"hyperspace_jump.tint_b",
		"hyperspace_jump.warp_start",
		"hyperspace_jump.warp_end",
		"hyperspace_jump.warp_curve",
		"hyperspace_jump.warp_distance_m",
		"hyperspace_jump.warp_sky_zoom",
		"hyperspace_jump.warp_smear",
		"hyperspace_jump.warp_sky_highlights",
		"hyperspace_jump.warp_reveal_softness",
		"classic_lighting.illuminance",
		"classic_lighting.image_radiance",
		"environment_maps.intensity_scale",
		"lighting.ambient_r",
		"lighting.ambient_g",
		"lighting.ambient_b",
		"lighting.map_ambient_min_r",
		"lighting.map_ambient_min_g",
		"lighting.map_ambient_min_b",
		"lighting.death_star_ambient_r",
		"lighting.death_star_ambient_g",
		"lighting.death_star_ambient_b",
		"lighting.emissive_scale",
		"bloom.intensity",
		"effects.explosion_genus_emissive_strength",
		"effects.glow_mark_emissive_strength",
		"effects.cockpit_glow_mark_emissive_strength",
		"effects.cockpit_hit_flash_emissive_strength",
		"dynamic_cockpit.display_emissive_strength",
		"dynamic_cockpit.hologram_emissive_strength",
		"motion_blur.shutter",
		"small_objects.prepass_min_size",
		"small_objects.draw_min_size",
		"temporal_upscaling.sharpness",
		"texture_filtering.max_anisotropy",
	};
	static const char* bools[] = {
		"presentation.hdr_output",       "point_lights.enabled",
		"point_lights.cluster_debug",    "motion_blur.camera_blur",
		"motion_blur.pause_keep_blur",   "motion_blur.velocity_viz",
		"texture_filtering.anisotropic", "hangar_lighting.enabled",
		"lighting.spec_geom_adapt",      "dynamic_cockpit.enabled",
		"dynamic_cockpit.classic_hud",   "dynamic_cockpit.holograms",
		"point_lights.model_lights",     "presentation.latency_pacing",
		"hyperspace_jump.exit_forward",  "hyperspace_jump.warp_sky_half_resolution",
		"models.backface_fill",
	};
	static const char* strings[] = {
		"presentation.cockpit_fov",
		"temporal_upscaling.mode",
		"exposure.mode",
		"exposure.channel",
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

/* Largest authored light values: illuminance in W/m^2 (ten times the sun at
 * the Earth's distance) and radiance in W/m^2/sr. */
#define FL_ILLUMINANCE_MAX 2000.0f
#define FL_RADIANCE_MAX 1000.0f

static int fl_hangar_lighting_config_valid(const XwaFlightHangarLightingParams* p, float direction_length) {
	return direction_length > 0.0001f && isfinite(direction_length) && p->intensity >= 0.0f &&
		   p->intensity <= FL_ILLUMINANCE_MAX && p->color[0] >= 0.0f && p->color[0] <= 4.0f &&
		   p->color[1] >= 0.0f && p->color[1] <= 4.0f && p->color[2] >= 0.0f && p->color[2] <= 4.0f &&
		   p->ambient_ceiling >= 0.0f && p->ambient_ceiling <= FL_RADIANCE_MAX && p->ambient_sides >= 0.0f &&
		   p->ambient_sides <= FL_RADIANCE_MAX && p->ambient_floor >= 0.0f &&
		   p->ambient_floor <= FL_RADIANCE_MAX;
}

#define FL_EXPOSURE_EV100_MIN -6.0f
#define FL_EXPOSURE_EV100_MAX 24.0f
#define FL_EXPOSURE_COMPENSATION_MAX 10.0f
#define FL_EXPOSURE_SPEED_MAX 20.0f
#define FL_EXPOSURE_NEAR_MAX 100.0f

static int fl_in_range(float v, float lo, float hi) { return isfinite(v) && v >= lo && v <= hi; }

static int fl_exposure_config_valid(const XwaFlightExposureParams* p) {
	return fl_in_range(p->ev100, FL_EXPOSURE_EV100_MIN, FL_EXPOSURE_EV100_MAX) &&
		   fl_in_range(p->compensation, -FL_EXPOSURE_COMPENSATION_MAX, FL_EXPOSURE_COMPENSATION_MAX) &&
		   fl_in_range(p->metering, 0.0f, 1.0f) &&
		   fl_in_range(p->min_ev100, FL_EXPOSURE_EV100_MIN, FL_EXPOSURE_EV100_MAX) &&
		   fl_in_range(p->max_ev100, p->min_ev100, FL_EXPOSURE_EV100_MAX) &&
		   fl_in_range(p->brighten_speed, 0.0f, FL_EXPOSURE_SPEED_MAX) && p->brighten_speed > 0.0f &&
		   fl_in_range(p->darken_speed, 0.0f, FL_EXPOSURE_SPEED_MAX) && p->darken_speed > 0.0f &&
		   fl_in_range(p->low_percent, 0.0f, 100.0f) &&
		   fl_in_range(p->high_percent, p->low_percent, 100.0f) && p->high_percent > p->low_percent &&
		   fl_in_range(p->sky_weight, 0.0f, 1.0f) && fl_in_range(p->near_weight, 0.0f, 1.0f) &&
		   fl_in_range(p->near_distance, 0.0f, FL_EXPOSURE_NEAR_MAX) &&
		   fl_in_range(p->edge_weight, 0.0f, 1.0f);
}

/* The largest point_lights.cockpit_explosion_distance_m, in metres. */
#define FL_COCKPIT_EXPLOSION_DISTANCE_MAX 100.0f

static int fl_point_lights_config_valid(const XwaFlightPointLightParams* p) {
	return p->cluster_depth_slices >= 4 && p->cluster_depth_slices <= 64 && isfinite(p->match_distance_m) &&
		   p->match_distance_m > 0.0f && isfinite(p->range_scale) && p->range_scale > 0.0f &&
		   fl_in_range(p->cockpit_explosion_light_scale, 0.0f, 1.0f) &&
		   fl_in_range(p->cockpit_explosion_distance_m, 0.0f, FL_COCKPIT_EXPLOSION_DISTANCE_MAX);
}

/* Reads the three numbers at `key` of `entry` into `out`, if the key is
   there. Logs an error and returns 0 when the value is wrong. */
static int fl_read_light_vector(const AeronConfigNode* entry, const char* path, const char* section,
								const char* name, const char* key, int non_negative, float out[3], int* found) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(entry, key);
	if (!node) {
		return 1;
	}
	int ok = AeronConfigNode_Type(node) == AERON_CONFIG_SEQUENCE && AeronConfigNode_SequenceCount(node) == 3;
	float values[3];
	for (size_t i = 0; ok && i < 3; i++) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, i);
		const AeronConfigNodeType type = AeronConfigNode_Type(item);
		values[i] = (float)AeronConfigNode_Float(item, 0.0);
		ok = (type == AERON_CONFIG_INT || type == AERON_CONFIG_FLOAT) && isfinite(values[i]) &&
			 (!non_negative || values[i] >= 0.0f);
	}
	if (!ok) {
		Aeron_LogError("xwa.remaster", "%s:%d: point_lights.%s.%s.%s must be a list of three %snumbers", path,
					   AeronConfigNode_Line(node), section, name, key, non_negative ? "non-negative " : "");
		return 0;
	}
	memcpy(out, values, sizeof values);
	if (found) {
		*found = 1;
	}
	return 1;
}

/* Reads the default values of the cockpit lights from
   point_lights.cockpit_lights. All three lights must be there, each
   with power_w. Logs an error and returns 0 when the section is wrong. */
static int fl_load_cockpit_lights_config(const AeronConfigFile* cf, const char* path) {
	const AeronConfigNode* section = AeronConfigFile_GetNode(cf, "point_lights.cockpit_lights");
	uint32_t found = 0;
	memset(s.plight.cockpit_lights, 0, sizeof s.plight.cockpit_lights);
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.remaster", "%s: point_lights.cockpit_lights must be a map of the three lights",
					   path);
		return 0;
	}
	for (size_t i = 0; i < AeronConfigNode_MapCount(section); i++) {
		const char* name = AeronConfigNode_MapKeyAt(section, i);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(section, i);
		const int slot = XwaCockpitLights_Find(name);
		if (slot < 0) {
			Aeron_LogError("xwa.remaster", "%s:%d: point_lights.cockpit_lights: unknown light %s", path,
						   AeronConfigNode_Line(entry), name ? name : "");
			return 0;
		}
		if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
			Aeron_LogError("xwa.remaster", "%s:%d: point_lights.cockpit_lights.%s must be a map", path,
						   AeronConfigNode_Line(entry), name);
			return 0;
		}
		for (size_t k = 0; k < AeronConfigNode_MapCount(entry); k++) {
			const char* key = AeronConfigNode_MapKeyAt(entry, k);
			if (!key || (strcmp(key, "position_m") != 0 && strcmp(key, "color") != 0 &&
						 strcmp(key, "power_w") != 0)) {
				Aeron_LogError("xwa.remaster", "%s:%d: point_lights.cockpit_lights.%s: unknown key %s", path,
							   AeronConfigNode_Line(AeronConfigNode_MapValueAt(entry, k)), name,
							   key ? key : "");
				return 0;
			}
		}
		XwaFlightCockpitLight* light = &s.plight.cockpit_lights[slot];
		const AeronConfigNode* power = AeronConfigNode_MapGet(entry, "power_w");
		const AeronConfigNodeType power_type = AeronConfigNode_Type(power);
		light->power_w = (float)AeronConfigNode_Float(power, -1.0);
		if ((power_type != AERON_CONFIG_INT && power_type != AERON_CONFIG_FLOAT) ||
			!isfinite(light->power_w) || light->power_w < 0.0f) {
			Aeron_LogError(
				"xwa.remaster",
				"%s:%d: point_lights.cockpit_lights.%s.power_w must be a number of watts, 0 or more", path,
				AeronConfigNode_Line(power ? power : entry), name);
			return 0;
		}
		if (!fl_read_light_vector(entry, path, "cockpit_lights", name, "position_m", 0, light->position_m,
								  NULL) ||
			!fl_read_light_vector(entry, path, "cockpit_lights", name, "color", 1, light->color,
								  &light->has_color)) {
			return 0;
		}
		found |= 1u << (uint32_t)slot;
	}
	for (int slot = 0; slot < XWA_SNAP_COCKPIT_LIGHT_COUNT; slot++) {
		if (!(found & (1u << (uint32_t)slot))) {
			Aeron_LogError("xwa.remaster", "%s: point_lights.cockpit_lights.%s is missing", path,
						   XwaCockpitLights_Name(slot));
			return 0;
		}
	}
	return 1;
}

/* Reads the default headlight from point_lights.headlights, which needs
   power_w. Logs an error and returns 0 when the section is wrong. */
static int fl_load_headlight_config(const AeronConfigFile* cf, const char* path) {
	static const char* const keys[] = { "position_m", "color", "power_w", "angle_degrees" };
	static const char* const vector_keys[2] = { "position_m", "color" };
	const AeronConfigNode* section = AeronConfigFile_GetNode(cf, "point_lights.headlights");
	XwaFlightHeadlight* light = &s.plight.headlight;
	float* const vectors[2] = { light->position_m, light->color };
	memset(light, 0, sizeof *light);
	light->position_m[2] = 20.0f;
	light->color[0] = light->color[1] = light->color[2] = 1.0f;
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.remaster", "%s: point_lights.headlights must be a map", path);
		return 0;
	}
	for (size_t k = 0; k < AeronConfigNode_MapCount(section); k++) {
		const char* key = AeronConfigNode_MapKeyAt(section, k);
		size_t i = 0;
		while (key && i < sizeof keys / sizeof keys[0] && strcmp(key, keys[i]) != 0) {
			i++;
		}
		if (!key || i == sizeof keys / sizeof keys[0]) {
			Aeron_LogError("xwa.remaster", "%s:%d: point_lights.headlights: unknown key %s", path,
						   AeronConfigNode_Line(AeronConfigNode_MapValueAt(section, k)), key ? key : "");
			return 0;
		}
	}
	for (int v = 0; v < 2; v++) {
		const AeronConfigNode* node = AeronConfigNode_MapGet(section, vector_keys[v]);
		if (!node) {
			continue;
		}
		int ok =
			AeronConfigNode_Type(node) == AERON_CONFIG_SEQUENCE && AeronConfigNode_SequenceCount(node) == 3;
		float values[3];
		for (size_t i = 0; ok && i < 3; i++) {
			const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, i);
			const AeronConfigNodeType type = AeronConfigNode_Type(item);
			values[i] = (float)AeronConfigNode_Float(item, 0.0);
			ok = (type == AERON_CONFIG_INT || type == AERON_CONFIG_FLOAT) && isfinite(values[i]) &&
				 (v == 0 || values[i] >= 0.0f);
		}
		if (!ok) {
			Aeron_LogError("xwa.remaster",
						   "%s:%d: point_lights.headlights.%s must be a list of three %snumbers", path,
						   AeronConfigNode_Line(node), vector_keys[v], v == 0 ? "" : "non-negative ");
			return 0;
		}
		memcpy(vectors[v], values, sizeof values);
	}
	const AeronConfigNode* power = AeronConfigNode_MapGet(section, "power_w");
	const AeronConfigNodeType power_type = AeronConfigNode_Type(power);
	light->power_w = (float)AeronConfigNode_Float(power, -1.0);
	if ((power_type != AERON_CONFIG_INT && power_type != AERON_CONFIG_FLOAT) || !isfinite(light->power_w) ||
		light->power_w < 0.0f) {
		Aeron_LogError("xwa.remaster",
					   "%s:%d: point_lights.headlights.power_w must be a number of watts, 0 or more", path,
					   AeronConfigNode_Line(power ? power : section));
		return 0;
	}
	const AeronConfigNode* angle = AeronConfigNode_MapGet(section, "angle_degrees");
	if (angle) {
		const AeronConfigNodeType angle_type = AeronConfigNode_Type(angle);
		light->angle_degrees = (float)AeronConfigNode_Float(angle, -1.0);
		if ((angle_type != AERON_CONFIG_INT && angle_type != AERON_CONFIG_FLOAT) ||
			!fl_in_range(light->angle_degrees, 0.0f, 90.0f)) {
			Aeron_LogError("xwa.remaster",
						   "%s:%d: point_lights.headlights.angle_degrees must be from 0 to 90", path,
						   AeronConfigNode_Line(angle));
			return 0;
		}
	}
	return 1;
}

/* Every group needs power_w. Logs an error and returns 0 when the section
   is wrong. */
static int fl_load_particle_lights_config(const AeronConfigFile* cf, const char* path) {
	const AeronConfigNode* section = AeronConfigFile_GetNode(cf, "point_lights.particle_lights");
	uint32_t found = 0;
	memset(s.plight.particle_lights, 0, sizeof s.plight.particle_lights);
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.remaster", "%s: point_lights.particle_lights must be a map of the four groups",
					   path);
		return 0;
	}
	for (size_t i = 0; i < AeronConfigNode_MapCount(section); i++) {
		const char* name = AeronConfigNode_MapKeyAt(section, i);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(section, i);
		const int group = XwaParticleLights_Find(name);
		if (group < 0) {
			Aeron_LogError("xwa.remaster", "%s:%d: point_lights.particle_lights: unknown group %s", path,
						   AeronConfigNode_Line(entry), name ? name : "");
			return 0;
		}
		if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
			Aeron_LogError("xwa.remaster", "%s:%d: point_lights.particle_lights.%s must be a map", path,
						   AeronConfigNode_Line(entry), name);
			return 0;
		}
		for (size_t k = 0; k < AeronConfigNode_MapCount(entry); k++) {
			const char* key = AeronConfigNode_MapKeyAt(entry, k);
			if (!key ||
				(strcmp(key, "color") != 0 && strcmp(key, "power_w") != 0 && strcmp(key, "fade") != 0)) {
				Aeron_LogError("xwa.remaster", "%s:%d: point_lights.particle_lights.%s: unknown key %s", path,
							   AeronConfigNode_Line(AeronConfigNode_MapValueAt(entry, k)), name,
							   key ? key : "");
				return 0;
			}
		}
		XwaFlightParticleLight* light = &s.plight.particle_lights[group];
		const AeronConfigNode* power = AeronConfigNode_MapGet(entry, "power_w");
		const AeronConfigNodeType power_type = AeronConfigNode_Type(power);
		light->power_w = (float)AeronConfigNode_Float(power, -1.0);
		if ((power_type != AERON_CONFIG_INT && power_type != AERON_CONFIG_FLOAT) ||
			!isfinite(light->power_w) || light->power_w < 0.0f) {
			Aeron_LogError(
				"xwa.remaster",
				"%s:%d: point_lights.particle_lights.%s.power_w must be a number of watts, 0 or more", path,
				AeronConfigNode_Line(power ? power : entry), name);
			return 0;
		}
		const AeronConfigNode* fade = AeronConfigNode_MapGet(entry, "fade");
		if (fade && AeronConfigNode_Type(fade) != AERON_CONFIG_BOOL) {
			Aeron_LogError("xwa.remaster", "%s:%d: point_lights.particle_lights.%s.fade must be true or false",
						   path, AeronConfigNode_Line(fade), name);
			return 0;
		}
		light->fade = AeronConfigNode_Bool(fade, 0);
		if (!fl_read_light_vector(entry, path, "particle_lights", name, "color", 1, light->color,
								  &light->has_color)) {
			return 0;
		}
		found |= 1u << (uint32_t)group;
	}
	for (int group = 0; group < XWA_PARTICLE_LIGHT_GROUP_COUNT; group++) {
		if (!(found & (1u << (uint32_t)group))) {
			Aeron_LogError("xwa.remaster", "%s: point_lights.particle_lights.%s is missing", path,
						   XwaParticleLights_Name(group));
			return 0;
		}
	}
	return 1;
}

static int fl_sky_config_valid(void) {
	return isfinite(s.sky_exposure) && s.sky_exposure >= 0.0f && isfinite(s.star_brightness) &&
		   s.star_brightness >= 0.0f && isfinite(s.star_density) && s.star_density >= 0.0f &&
		   s.star_density <= 1.0f && isfinite(s.star_grid) && s.star_grid >= 1.0f && s.star_grid <= 256.0f &&
		   isfinite(s.star_core_px) && s.star_core_px >= 0.0f && isfinite(s.star_feather) &&
		   s.star_feather >= 0.0f && isfinite(s.star_flare) && s.star_flare >= 0.0f;
}

static int fl_hyperspace_config_valid(const XwaFlightHyperspaceTunnelParams* p) {
	if (!(isfinite(p->travel_speed) && p->travel_speed > 0.0f && p->travel_speed <= 64.0f &&
		  isfinite(p->rotation_speed) && p->rotation_speed >= -8.0f && p->rotation_speed <= 8.0f &&
		  isfinite(p->noise_scale) && p->noise_scale >= 0.125f && p->noise_scale <= 8.0f &&
		  isfinite(p->brightness) && p->brightness >= 0.0f && p->brightness <= 16.0f &&
		  isfinite(p->highlight_strength) && p->highlight_strength >= 0.0f &&
		  p->highlight_strength <= 16.0f && isfinite(p->focal_length) && p->focal_length >= 0.25f &&
		  p->focal_length <= 4.0f && isfinite(p->twist) && p->twist >= -2.0f && p->twist <= 2.0f &&
		  isfinite(p->cap_radius) && p->cap_radius >= 0.001f && p->cap_radius <= 1.0f &&
		  isfinite(p->cap_falloff) && p->cap_falloff >= 0.1f && p->cap_falloff <= 64.0f &&
		  isfinite(p->mesh_ambient_strength) && p->mesh_ambient_strength >= 0.0f &&
		  p->mesh_ambient_strength <= 4.0f && isfinite(p->mesh_environment_roughness) &&
		  p->mesh_environment_roughness >= 0.0f && p->mesh_environment_roughness <= 1.0f &&
		  isfinite(p->mesh_key_strength) && p->mesh_key_strength >= 0.0f && p->mesh_key_strength <= 4.0f)) {
		return 0;
	}
	for (int channel = 0; channel < 3; channel++) {
		if (!isfinite(p->dark_color[channel]) || p->dark_color[channel] < 0.0f ||
			p->dark_color[channel] > 16.0f || !isfinite(p->body_color[channel]) ||
			p->body_color[channel] < 0.0f || p->body_color[channel] > 16.0f ||
			!isfinite(p->highlight_color[channel]) || p->highlight_color[channel] < 0.0f ||
			p->highlight_color[channel] > 16.0f || !isfinite(p->cap_color[channel]) ||
			p->cap_color[channel] < 0.0f || p->cap_color[channel] > 16.0f) {
			return 0;
		}
	}
	return 1;
}

static int fl_load_hyperspace_config(const AeronConfigFile* config, const char* config_path) {
	s.hyperspace_params.travel_speed =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.travel_speed", 0.0);
	s.hyperspace_params.rotation_speed =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.rotation_speed", 0.0);
	s.hyperspace_params.noise_scale =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.noise_scale", 0.0);
	s.hyperspace_params.brightness =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.brightness", 0.0);
	s.hyperspace_params.highlight_strength =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.highlight_strength", 0.0);
	s.hyperspace_params.focal_length =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.focal_length", 0.0);
	s.hyperspace_params.twist = (float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.twist", 0.0);
	s.hyperspace_params.cap_radius =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.cap_radius", 0.0);
	s.hyperspace_params.cap_falloff =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.cap_falloff", 0.0);
	s.hyperspace_params.mesh_ambient_strength =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.mesh_ambient_strength", 0.0);
	s.hyperspace_params.mesh_environment_roughness =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.mesh_environment_roughness", 0.0);
	s.hyperspace_params.mesh_key_strength =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_tunnel.mesh_key_strength", 0.0);
	static const char* dark_color_paths[3] = { "hyperspace_tunnel.dark_color_r",
											   "hyperspace_tunnel.dark_color_g",
											   "hyperspace_tunnel.dark_color_b" };
	static const char* body_color_paths[3] = { "hyperspace_tunnel.body_color_r",
											   "hyperspace_tunnel.body_color_g",
											   "hyperspace_tunnel.body_color_b" };
	static const char* highlight_color_paths[3] = { "hyperspace_tunnel.highlight_color_r",
													"hyperspace_tunnel.highlight_color_g",
													"hyperspace_tunnel.highlight_color_b" };
	static const char* cap_color_paths[3] = { "hyperspace_tunnel.cap_color_r",
											  "hyperspace_tunnel.cap_color_g",
											  "hyperspace_tunnel.cap_color_b" };
	for (int channel = 0; channel < 3; channel++) {
		s.hyperspace_params.dark_color[channel] =
			(float)AeronConfigFile_GetFloat(config, dark_color_paths[channel], 0.0);
		s.hyperspace_params.body_color[channel] =
			(float)AeronConfigFile_GetFloat(config, body_color_paths[channel], 0.0);
		s.hyperspace_params.highlight_color[channel] =
			(float)AeronConfigFile_GetFloat(config, highlight_color_paths[channel], 0.0);
		s.hyperspace_params.cap_color[channel] =
			(float)AeronConfigFile_GetFloat(config, cap_color_paths[channel], 0.0);
	}
	if (!fl_hyperspace_config_valid(&s.hyperspace_params)) {
		Aeron_LogError("xwa.remaster", "%s: invalid hyperspace_tunnel settings", config_path);
		return 0;
	}
	return 1;
}

/* The limits of the hyperspace_jump settings (docs/hd_hyperspace_jump.md). */
#define FL_JUMP_STAR_COUNT_MIN 1000
#define FL_JUMP_STAR_COUNT_MAX 262144
#define FL_JUMP_WARP_TAPS_MIN 8
#define FL_JUMP_WARP_TAPS_MAX 64
#define FL_JUMP_WARP_SKY_TAPS_MIN 4
#define FL_JUMP_WARP_SKY_TAPS_MAX 16
/* The warp's end comes at least this long after its start. */
#define FL_JUMP_WARP_MIN_SECONDS 0.1f

static float fl_clampf(float value, float lo, float hi) { return fminf(fmaxf(value, lo), hi); }

static int fl_hyperspace_jump_valid(const XwaFlightHyperspaceJumpParams* p) {
	if (!(p->star_count >= FL_JUMP_STAR_COUNT_MIN && p->star_count <= FL_JUMP_STAR_COUNT_MAX &&
		  fl_in_range(p->streak_brightness, 0.0f, 16.0f) && fl_in_range(p->whiteout_brightness, 1.0f, 128.0f) &&
		  fl_in_range(p->brightness_range, 1.0f, 1000.0f) && fl_in_range(p->streak_width_px, 0.5f, 8.0f) &&
		  fl_in_range(p->width_from_brightness, 0.0f, 1.0f) && fl_in_range(p->width_variation, 1.0f, 4.0f) &&
		  fl_in_range(p->head_widening, 1.0f, 16.0f) && fl_in_range(p->tail_taper, 0.0f, 1.0f) &&
		  fl_in_range(p->head_taper, 0.0f, 1.0f) && fl_in_range(p->taper_variation, 0.0f, 1.0f) &&
		  fl_in_range(p->radial_widening, 0.0f, 4.0f) && fl_in_range(p->glow_strength, 0.0f, 1.0f) &&
		  fl_in_range(p->glow_width, 1.0f, 16.0f) && fl_in_range(p->tail_brightness, 0.0f, 1.0f) &&
		  fl_in_range(p->stretch_scale, 0.25f, 4.0f) && fl_in_range(p->white_fade_seconds, 0.05f, 0.5f) &&
		  fl_in_range(p->tunnel_entry_brightness, 1.0f, 16.0f) &&
		  fl_in_range(p->tunnel_settle_seconds, 0.05f, 2.0f) && fl_in_range(p->mesh_light_strength, 0.0f, 16.0f) &&
		  fl_in_range(p->mesh_light_start, 0.0f, 2.0f) && fl_in_range(p->mesh_light_curve, 0.25f, 8.0f) &&
		  fl_in_range(p->warp_start, 0.0f, 2.0f) && fl_in_range(p->warp_end, 0.2f, 2.5f) &&
		  p->warp_end >= p->warp_start + FL_JUMP_WARP_MIN_SECONDS && fl_in_range(p->warp_curve, 0.25f, 8.0f) &&
		  fl_in_range(p->warp_distance_m, 0.0f, 200000.0f) && fl_in_range(p->warp_sky_zoom, 1.0f, 16.0f) &&
		  fl_in_range(p->warp_smear, 0.05f, 1.0f) && fl_in_range(p->warp_sky_highlights, 1.0f, 3.0f) &&
		  fl_in_range(p->warp_reveal_softness, 0.25f, 2.0f) && p->warp_taps >= FL_JUMP_WARP_TAPS_MIN &&
		  p->warp_taps <= FL_JUMP_WARP_TAPS_MAX && p->warp_sky_taps >= FL_JUMP_WARP_SKY_TAPS_MIN &&
		  p->warp_sky_taps <= FL_JUMP_WARP_SKY_TAPS_MAX)) {
		return 0;
	}
	for (int channel = 0; channel < 3; channel++) {
		if (!fl_in_range(p->tint[channel], 0.0f, 16.0f) || !fl_in_range(p->tail_color[channel], 0.0f, 16.0f)) {
			return 0;
		}
	}
	return 1;
}

static int fl_load_hyperspace_jump_config(const AeronConfigFile* config, const char* config_path) {
	XwaFlightHyperspaceJumpParams* p = &s.hyperspace_jump;
	p->star_count = (int)AeronConfigFile_GetInt(config, "hyperspace_jump.star_count", 0);
	p->streak_brightness = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.streak_brightness", 0.0);
	p->whiteout_brightness = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.whiteout_brightness", 0.0);
	p->brightness_range = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.brightness_range", 0.0);
	p->streak_width_px = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.streak_width_px", 0.0);
	p->width_from_brightness =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_jump.width_from_brightness", 0.0);
	p->width_variation = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.width_variation", 0.0);
	p->head_widening = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.head_widening", 0.0);
	p->tail_taper = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.tail_taper", 0.0);
	p->head_taper = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.head_taper", 0.0);
	p->taper_variation = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.taper_variation", 0.0);
	p->radial_widening = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.radial_widening", 0.0);
	p->glow_strength = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.glow_strength", 0.0);
	p->glow_width = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.glow_width", 0.0);
	p->tail_brightness = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.tail_brightness", 0.0);
	static const char* tail_color_paths[3] = { "hyperspace_jump.tail_color_r", "hyperspace_jump.tail_color_g",
											   "hyperspace_jump.tail_color_b" };
	for (int channel = 0; channel < 3; channel++) {
		p->tail_color[channel] = (float)AeronConfigFile_GetFloat(config, tail_color_paths[channel], 0.0);
	}
	p->stretch_scale = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.stretch_scale", 0.0);
	p->white_fade_seconds = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.white_fade_seconds", 0.0);
	p->tunnel_entry_brightness =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_jump.tunnel_entry_brightness", 0.0);
	p->tunnel_settle_seconds =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_jump.tunnel_settle_seconds", 0.0);
	p->mesh_light_strength = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.mesh_light_strength", 0.0);
	p->mesh_light_start = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.mesh_light_start", 0.0);
	p->mesh_light_curve = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.mesh_light_curve", 0.0);
	static const char* tint_paths[3] = { "hyperspace_jump.tint_r", "hyperspace_jump.tint_g",
										 "hyperspace_jump.tint_b" };
	for (int channel = 0; channel < 3; channel++) {
		p->tint[channel] = (float)AeronConfigFile_GetFloat(config, tint_paths[channel], 0.0);
	}
	p->warp_start = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.warp_start", 0.0);
	p->warp_end = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.warp_end", 0.0);
	p->warp_curve = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.warp_curve", 0.0);
	p->warp_distance_m = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.warp_distance_m", 0.0);
	p->warp_sky_zoom = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.warp_sky_zoom", 0.0);
	p->warp_smear = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.warp_smear", 0.0);
	p->warp_sky_highlights = (float)AeronConfigFile_GetFloat(config, "hyperspace_jump.warp_sky_highlights", 0.0);
	p->warp_reveal_softness =
		(float)AeronConfigFile_GetFloat(config, "hyperspace_jump.warp_reveal_softness", 0.0);
	p->warp_taps = (int)AeronConfigFile_GetInt(config, "hyperspace_jump.warp_taps", 0);
	p->warp_sky_taps = (int)AeronConfigFile_GetInt(config, "hyperspace_jump.warp_sky_taps", 0);
	p->exit_forward = AeronConfigFile_GetBool(config, "hyperspace_jump.exit_forward", 0) ? 1 : 0;
	p->warp_sky_half_resolution =
		AeronConfigFile_GetBool(config, "hyperspace_jump.warp_sky_half_resolution", 0) ? 1 : 0;
	if (!fl_hyperspace_jump_valid(p)) {
		Aeron_LogError("xwa.remaster", "%s: invalid hyperspace_jump settings", config_path);
		return 0;
	}
	return 1;
}

/* Reads `key` of a look, a number from `lo` to `hi`, into `out`. Logs and
 * returns 0 when it is missing or out of range. */
static int fl_look_number(const AeronConfigNode* look, const char* group, const char* key, float lo, float hi,
						  const char* name, const char* config_path, float* out) {
	const AeronConfigNode* parent = group ? AeronConfigNode_MapGet(look, group) : look;
	const AeronConfigNode* node = AeronConfigNode_MapGet(parent, key);
	const AeronConfigNodeType type = AeronConfigNode_Type(node);
	const double value = AeronConfigNode_Float(node, NAN);

	if ((type != AERON_CONFIG_FLOAT && type != AERON_CONFIG_INT) || !isfinite(value) || value < lo ||
		value > hi) {
		Aeron_LogError("xwa.remaster", "%s: hyperspace_looks.%s.%s%s%s must be a number from %g to %g",
					   config_path, name, group ? group : "", group ? "." : "", key, (double)lo, (double)hi);
		return 0;
	}
	*out = (float)value;
	return 1;
}

/* Reads `<key>_r`, `_g` and `_b` of a look into `out`. */
static int fl_look_color(const AeronConfigNode* look, const char* group, const char* key, const char* name,
						 const char* config_path, float out[3]) {
	static const char* const suffixes[3] = { "_r", "_g", "_b" };
	for (int channel = 0; channel < 3; channel++) {
		char channel_key[64];
		snprintf(channel_key, sizeof channel_key, "%s%s", key, suffixes[channel]);
		if (!fl_look_number(look, group, channel_key, 0.0f, 16.0f, name, config_path, &out[channel])) {
			return 0;
		}
	}
	return 1;
}

/* The keys of XwaFlightHyperspaceCameraParams and their ranges. */
static const struct {
	const char* key;
	size_t offset;
	float lo;
	float hi;
} fl_hyperspace_camera_keys[] = {
	{ "shake_degrees", offsetof(XwaFlightHyperspaceCameraParams, shake_degrees), 0.0f, 10.0f },
	{ "shake_speed", offsetof(XwaFlightHyperspaceCameraParams, shake_speed), 0.0f, 4.0f },
	{ "shake_start", offsetof(XwaFlightHyperspaceCameraParams, shake_start), 0.0f, 1.0f },
	{ "shake_full", offsetof(XwaFlightHyperspaceCameraParams, shake_full), 0.0f, 1.0f },
	{ "kick_rate", offsetof(XwaFlightHyperspaceCameraParams, kick_rate), 0.0f, 8.0f },
	{ "jolt_degrees", offsetof(XwaFlightHyperspaceCameraParams, jolt_degrees), 0.0f, 20.0f },
	{ "jolt_delay", offsetof(XwaFlightHyperspaceCameraParams, jolt_delay), 0.0f, 0.5f },
	{ "jolt_time", offsetof(XwaFlightHyperspaceCameraParams, jolt_time), 0.05f, 0.8f },
};

#define FL_HYPERSPACE_CAMERA_KEY_COUNT                                                                       \
	(sizeof fl_hyperspace_camera_keys / sizeof fl_hyperspace_camera_keys[0])

static float* fl_hyperspace_camera_field(XwaFlightHyperspaceCameraParams* p, size_t key) {
	return (float*)((char*)p + fl_hyperspace_camera_keys[key].offset);
}

/* Reads the camera keys of `node` into `out`. `where` is the node's name in
 * error messages, such as "hyperspace_camera". Logs an error and returns 0
 * when a key is missing or out of range, or when shake_full is below
 * shake_start. */
static int fl_read_hyperspace_camera(const AeronConfigNode* node, const char* where, const char* config_path,
									 XwaFlightHyperspaceCameraParams* out) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.remaster", "%s: %s must be a map", config_path, where);
		return 0;
	}
	for (size_t i = 0; i < FL_HYPERSPACE_CAMERA_KEY_COUNT; i++) {
		const AeronConfigNode* value_node = AeronConfigNode_MapGet(node, fl_hyperspace_camera_keys[i].key);
		const AeronConfigNodeType type = AeronConfigNode_Type(value_node);
		const double value = AeronConfigNode_Float(value_node, NAN);
		const float lo = fl_hyperspace_camera_keys[i].lo;
		const float hi = fl_hyperspace_camera_keys[i].hi;

		if ((type != AERON_CONFIG_FLOAT && type != AERON_CONFIG_INT) || !isfinite(value) || value < lo ||
			value > hi) {
			Aeron_LogError("xwa.remaster", "%s: %s.%s must be a number from %g to %g", config_path, where,
						   fl_hyperspace_camera_keys[i].key, (double)lo, (double)hi);
			return 0;
		}
		*fl_hyperspace_camera_field(out, i) = (float)value;
	}
	if (out->shake_full < out->shake_start) {
		Aeron_LogError("xwa.remaster", "%s: %s.shake_full must not be below shake_start", config_path, where);
		return 0;
	}
	return 1;
}

/* Fixes the values of `p`. A value that isn't a number is replaced by its
 * value in `d`, and the other values are clamped to their range. If
 * shake_full is below shake_start, it is set to shake_start. */
static void fl_sanitize_hyperspace_camera(XwaFlightHyperspaceCameraParams* p,
										  const XwaFlightHyperspaceCameraParams* d) {
	for (size_t i = 0; i < FL_HYPERSPACE_CAMERA_KEY_COUNT; i++) {
		float* value = fl_hyperspace_camera_field(p, i);
		*value = isfinite(*value)
					 ? fl_clampf(*value, fl_hyperspace_camera_keys[i].lo, fl_hyperspace_camera_keys[i].hi)
					 : *(const float*)((const char*)d + fl_hyperspace_camera_keys[i].offset);
	}
	if (p->shake_full < p->shake_start) {
		p->shake_full = p->shake_start;
	}
}

static int fl_load_hyperspace_camera_config(const AeronConfigFile* config, const char* config_path) {
	if (!fl_read_hyperspace_camera(AeronConfigFile_GetNode(config, "hyperspace_camera"), "hyperspace_camera",
								   config_path, &s.hyperspace_camera)) {
		return 0;
	}
	s.hyperspace_camera_default = s.hyperspace_camera;
	return 1;
}

static int fl_load_hyperspace_looks_config(const AeronConfigFile* config, const char* config_path) {
	const AeronConfigNode* section = AeronConfigFile_GetNode(config, "hyperspace_looks");

	s.hyperspace_look_count = 0;
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.remaster", "%s: hyperspace_looks must be a map of looks", config_path);
		return 0;
	}
	if (AeronConfigNode_MapCount(section) > XWA_FLIGHT_HYPERSPACE_LOOKS_MAX) {
		Aeron_LogError("xwa.remaster", "%s: hyperspace_looks can have at most %d looks", config_path,
					   XWA_FLIGHT_HYPERSPACE_LOOKS_MAX);
		return 0;
	}
	for (size_t i = 0; i < AeronConfigNode_MapCount(section); i++) {
		const char* name = AeronConfigNode_MapKeyAt(section, i);
		const AeronConfigNode* look = AeronConfigNode_MapValueAt(section, i);
		XwaFlightHyperspaceLookParams* p = &s.hyperspace_looks[s.hyperspace_look_count];

		if (strlen(name) >= XWA_FLIGHT_HYPERSPACE_LOOK_NAME_MAX ||
			AeronConfigNode_Type(look) != AERON_CONFIG_MAP) {
			Aeron_LogError("xwa.remaster",
						   "%s: hyperspace_looks.%s must be a map, with a name shorter than %d", config_path,
						   name, XWA_FLIGHT_HYPERSPACE_LOOK_NAME_MAX);
			return 0;
		}
		if (!fl_look_color(look, "tunnel", "dark_color", name, config_path, p->dark_color) ||
			!fl_look_color(look, "tunnel", "body_color", name, config_path, p->body_color) ||
			!fl_look_color(look, "tunnel", "highlight_color", name, config_path, p->highlight_color) ||
			!fl_look_color(look, "tunnel", "cap_color", name, config_path, p->cap_color) ||
			!fl_look_color(look, "tunnel", "edge_color", name, config_path, p->edge_color) ||
			!fl_look_number(look, "tunnel", "edge_width", 0.01f, 4.0f, name, config_path, &p->edge_width) ||
			!fl_look_number(look, "tunnel", "front_noise", 0.0f, 8.0f, name, config_path, &p->front_noise) ||
			!fl_look_number(look, "tunnel", "front_noise_scale", 0.05f, 16.0f, name, config_path,
							&p->front_noise_scale) ||
			!fl_look_number(look, "tunnel", "front_warp", 0.0f, 4.0f, name, config_path, &p->front_warp) ||
			!fl_look_color(look, "jump", "tint", name, config_path, p->tint) ||
			!fl_look_color(look, "jump", "tail_color", name, config_path, p->tail_color) ||
			!fl_look_number(look, NULL, "blend_start", 0.0f, 1.0f, name, config_path, &p->blend_start) ||
			!fl_look_number(look, NULL, "blend_end", 0.0f, 1.0f, name, config_path, &p->blend_end)) {
			return 0;
		}
		char camera_where[XWA_FLIGHT_HYPERSPACE_LOOK_NAME_MAX + 32];
		snprintf(camera_where, sizeof camera_where, "hyperspace_looks.%s.camera", name);
		if (!fl_read_hyperspace_camera(AeronConfigNode_MapGet(look, "camera"), camera_where, config_path,
									   &p->camera)) {
			return 0;
		}
		if (p->blend_end < p->blend_start) {
			Aeron_LogError("xwa.remaster", "%s: hyperspace_looks.%s.blend_end must not be below blend_start",
						   config_path, name);
			return 0;
		}
		snprintf(s.hyperspace_look_names[s.hyperspace_look_count], XWA_FLIGHT_HYPERSPACE_LOOK_NAME_MAX, "%s",
				 name);
		s.hyperspace_looks_default[s.hyperspace_look_count] = *p;
		s.hyperspace_look_count++;
	}
	s.hyperspace_preview_look = -1;
	s.hyperspace_preview_look_blend = 1.0f;
	return 1;
}

static int fl_pbr_config_valid(const XwaShipPbrTuning* p) {
	return isfinite(p->ambient[0]) && p->ambient[0] >= 0.0f && isfinite(p->ambient[1]) &&
		   p->ambient[1] >= 0.0f && isfinite(p->ambient[2]) && p->ambient[2] >= 0.0f;
}

/* Load the renderer's settings from the mandatory shipped configuration once. */
int XwaRemasterFlight_InitConfig(AeronVfs* vfs) {
	AeronSceneTonemapSettings tonemap;
	if (s.config_loaded) {
		return 1;
	}

	static const char* scene_defaults_path = "aeron/scene3d_defaults.yaml";
	AeronConfigFile* scene_defaults = NULL;
	AeronConfigError scene_error;
	if (!vfs ||
		!AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_RESOURCE, scene_defaults_path, &scene_defaults)) {
		Aeron_LogError("xwa.remaster", "required Aeron Scene3D defaults unavailable or invalid: %s",
					   scene_defaults_path);
		return 0;
	}
	if (!AeronSceneSettings_Load(AeronConfigFile_Root(scene_defaults), &s.ssao, &s.shadows, &tonemap,
								 &scene_error)) {
		Aeron_LogError("xwa.remaster", "%s:%d:%d: %s", scene_defaults_path, scene_error.line,
					   scene_error.column, scene_error.message);
		AeronConfigFile_Destroy(scene_defaults);
		return 0;
	}
	AeronConfigFile_Destroy(scene_defaults);

	static const char* path = "remaster/config.yaml";
	AeronConfigFile* cf = NULL;
	if (!vfs || !AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_RESOURCE, path, &cf)) {
		Aeron_LogError("xwa.remaster", "required shipped configuration unavailable or invalid: %s", path);
		return 0;
	}
	const char* error_path = NULL;
	if (!fl_cfg_validate(cf, &error_path)) {
		Aeron_LogError("xwa.remaster", "%s: missing or invalid required setting '%s'", path, error_path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	if (!AeronSceneSettings_Overlay(AeronConfigFile_Root(cf), &s.ssao, &s.shadows, &tonemap, &scene_error)) {
		Aeron_LogError("xwa.remaster", "%s:%d:%d: %s", path, scene_error.line, scene_error.column,
					   scene_error.message);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	/* Every value read below is required and type-checked above. The zero
	 * arguments only satisfy the generic accessor API; they are unreachable
	 * as configuration fallbacks. */
	const int vsync_divisor = (int)AeronConfigFile_GetInt(cf, "presentation.vsync_divisor", 0);
	if (!Aeron_SetPresentationVsyncDivisor(vsync_divisor)) {
		Aeron_LogError("xwa.remaster", "%s: presentation.vsync_divisor must be 1 or 2", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	const int frames_in_flight = (int)AeronConfigFile_GetInt(cf, "presentation.frames_in_flight", 0);
	if (!Aeron_SetFramesInFlight(frames_in_flight)) {
		Aeron_LogError("xwa.remaster", "%s: presentation.frames_in_flight must be 1 or 2", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	Aeron_SetLatencyPacing(AeronConfigFile_GetBool(cf, "presentation.latency_pacing", 0));
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
			Aeron_LogError("xwa.remaster", "%s: presentation.msaa_samples must be 1, 2, 4, or 8", path);
			AeronConfigFile_Destroy(cf);
			return 0;
	}
	s.configured_hdr_output = AeronConfigFile_GetBool(cf, "presentation.hdr_output", 0);
	{
		static const char* const names[] = { "cockpit", "wide", "off" };
		const char* name = AeronConfigFile_GetString(cf, "presentation.cockpit_fov", NULL);
		int mode = -1;
		for (int i = 0; i < 3; i++) {
			if (name && strcmp(name, names[i]) == 0)
				mode = i;
		}
		if (mode < 0) {
			Aeron_LogError("xwa.remaster", "%s: presentation.cockpit_fov must be cockpit, wide or off", path);
			AeronConfigFile_Destroy(cf);
			return 0;
		}
		XwaCockpitView_SetMode((XwaCockpitFovMode)mode);
	}
	s.cockpit_fov_min_aspect =
		(float)AeronConfigFile_GetFloat(cf, "presentation.cockpit_fov_min_aspect", 0.0);
	if (!fl_in_range(s.cockpit_fov_min_aspect, 0.0f, 4.0f)) {
		Aeron_LogError("xwa.remaster", "%s: presentation.cockpit_fov_min_aspect must be between 0 and 4",
					   path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.shadow_debug_atlas_cascade = -1;
	s.hangar_lighting.enabled = AeronConfigFile_GetBool(cf, "hangar_lighting.enabled", 0);
	s.hangar_lighting.direction[0] = (float)AeronConfigFile_GetFloat(cf, "hangar_lighting.direction_x", 0.0);
	s.hangar_lighting.direction[1] = (float)AeronConfigFile_GetFloat(cf, "hangar_lighting.direction_y", 0.0);
	s.hangar_lighting.direction[2] = (float)AeronConfigFile_GetFloat(cf, "hangar_lighting.direction_z", 0.0);
	s.hangar_lighting.intensity = (float)AeronConfigFile_GetFloat(cf, "hangar_lighting.intensity", 0.0);
	s.hangar_lighting.color[0] = (float)AeronConfigFile_GetFloat(cf, "hangar_lighting.color_r", 0.0);
	s.hangar_lighting.color[1] = (float)AeronConfigFile_GetFloat(cf, "hangar_lighting.color_g", 0.0);
	s.hangar_lighting.color[2] = (float)AeronConfigFile_GetFloat(cf, "hangar_lighting.color_b", 0.0);
	s.hangar_lighting.ambient_ceiling =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.ambient_ceiling", 0.0);
	s.hangar_lighting.ambient_sides =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.ambient_sides", 0.0);
	s.hangar_lighting.ambient_floor =
		(float)AeronConfigFile_GetFloat(cf, "hangar_lighting.ambient_floor", 0.0);
	const float hangar_direction_length =
		sqrtf(s.hangar_lighting.direction[0] * s.hangar_lighting.direction[0] +
			  s.hangar_lighting.direction[1] * s.hangar_lighting.direction[1] +
			  s.hangar_lighting.direction[2] * s.hangar_lighting.direction[2]);
	if (!fl_hangar_lighting_config_valid(&s.hangar_lighting, hangar_direction_length)) {
		Aeron_LogError("xwa.remaster", "%s: invalid hangar_lighting settings", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	for (int axis = 0; axis < 3; axis++) {
		s.hangar_lighting.direction[axis] /= hangar_direction_length;
	}
	const char* exposure_mode = AeronConfigFile_GetString(cf, "exposure.mode", "");
	const char* exposure_channel = AeronConfigFile_GetString(cf, "exposure.channel", "");
	s.exposure.metered = strcmp(exposure_mode, "metered") == 0;
	s.exposure.max_channel = strcmp(exposure_channel, "max") == 0;
	s.exposure.ev100 = (float)AeronConfigFile_GetFloat(cf, "exposure.ev100", 0.0);
	s.exposure.compensation = (float)AeronConfigFile_GetFloat(cf, "exposure.compensation", 0.0);
	s.exposure.metering = (float)AeronConfigFile_GetFloat(cf, "exposure.metering", 0.0);
	s.exposure.min_ev100 = (float)AeronConfigFile_GetFloat(cf, "exposure.min_ev100", 0.0);
	s.exposure.max_ev100 = (float)AeronConfigFile_GetFloat(cf, "exposure.max_ev100", 0.0);
	s.exposure.brighten_speed = (float)AeronConfigFile_GetFloat(cf, "exposure.brighten_speed", 0.0);
	s.exposure.darken_speed = (float)AeronConfigFile_GetFloat(cf, "exposure.darken_speed", 0.0);
	s.exposure.low_percent = (float)AeronConfigFile_GetFloat(cf, "exposure.low_percent", 0.0);
	s.exposure.high_percent = (float)AeronConfigFile_GetFloat(cf, "exposure.high_percent", 0.0);
	s.exposure.sky_weight = (float)AeronConfigFile_GetFloat(cf, "exposure.sky_weight", 0.0);
	s.exposure.near_weight = (float)AeronConfigFile_GetFloat(cf, "exposure.near_weight", 0.0);
	s.exposure.near_distance = (float)AeronConfigFile_GetFloat(cf, "exposure.near_distance", 0.0);
	s.exposure.edge_weight = (float)AeronConfigFile_GetFloat(cf, "exposure.edge_weight", 0.0);
	s.exposure.freeze = 0;
	if ((!s.exposure.metered && strcmp(exposure_mode, "fixed") != 0) ||
		(!s.exposure.max_channel && strcmp(exposure_channel, "luminance") != 0)) {
		Aeron_LogError("xwa.remaster",
					   "%s: exposure.mode must be fixed or metered, and exposure.channel "
					   "luminance or max",
					   path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	if (!fl_exposure_config_valid(&s.exposure)) {
		Aeron_LogError("xwa.remaster",
					   "%s: invalid exposure settings: EVs between %g and %g with min_ev100 <= max_ev100, "
					   "compensation within %g EV, metering and the weights between 0 and 1, speeds above 0 "
					   "and up to %g, 0 <= low_percent < high_percent <= 100, and near_distance up to %g m",
					   path, (double)FL_EXPOSURE_EV100_MIN, (double)FL_EXPOSURE_EV100_MAX,
					   (double)FL_EXPOSURE_COMPENSATION_MAX, (double)FL_EXPOSURE_SPEED_MAX,
					   (double)FL_EXPOSURE_NEAR_MAX);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.plight.enabled = AeronConfigFile_GetBool(cf, "point_lights.enabled", 0);
	s.plight.cluster_depth_slices = (int)AeronConfigFile_GetInt(cf, "point_lights.cluster_depth_slices", 0);
	s.plight.cluster_debug = AeronConfigFile_GetBool(cf, "point_lights.cluster_debug", 0);
	s.plight.match_distance_m = (float)AeronConfigFile_GetFloat(cf, "point_lights.match_distance_m", 0.0);
	s.plight.range_scale = (float)AeronConfigFile_GetFloat(cf, "point_lights.range_scale", 0.0);
	s.plight.cockpit_explosion_light_scale =
		(float)AeronConfigFile_GetFloat(cf, "point_lights.cockpit_explosion_light_scale", -1.0);
	s.plight.cockpit_explosion_distance_m =
		(float)AeronConfigFile_GetFloat(cf, "point_lights.cockpit_explosion_distance_m", -1.0);
	s.plight.model_lights = AeronConfigFile_GetBool(cf, "point_lights.model_lights", 0);
	s.plight.model_light_scale = 1.0f;
	if (!fl_point_lights_config_valid(&s.plight)) {
		Aeron_LogError("xwa.remaster",
					   "%s: invalid point_lights settings. cockpit_explosion_light_scale must be from 0 to "
					   "1, and cockpit_explosion_distance_m from 0 to %.0f",
					   path, (double)FL_COCKPIT_EXPLOSION_DISTANCE_MAX);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	if (!fl_load_cockpit_lights_config(cf, path) || !fl_load_particle_lights_config(cf, path) ||
		!fl_load_headlight_config(cf, path)) {
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.texture_filtering.anisotropic = AeronConfigFile_GetBool(cf, "texture_filtering.anisotropic", 0);
	s.texture_filtering.max_anisotropy =
		(float)AeronConfigFile_GetFloat(cf, "texture_filtering.max_anisotropy", 0.0);
	if (s.texture_filtering.max_anisotropy < 1.0f || s.texture_filtering.max_anisotropy > 16.0f) {
		Aeron_LogError("xwa.remaster", "%s: texture_filtering.max_anisotropy must be between 1 and 16", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.sky_exposure = (float)AeronConfigFile_GetFloat(cf, "starfield.exposure", 0.0);
	s.star_brightness = (float)AeronConfigFile_GetFloat(cf, "starfield.brightness", 0.0);
	s.star_density = (float)AeronConfigFile_GetFloat(cf, "starfield.density", 0.0);
	s.star_grid = (float)AeronConfigFile_GetFloat(cf, "starfield.grid", 0.0);
	s.star_core_px = (float)AeronConfigFile_GetFloat(cf, "starfield.core_px", 0.0);
	s.star_feather = (float)AeronConfigFile_GetFloat(cf, "starfield.feather", 0.0);
	s.star_flare = (float)AeronConfigFile_GetFloat(cf, "starfield.flare", 0.0);
	if (!fl_sky_config_valid()) {
		Aeron_LogError("xwa.remaster", "%s: invalid starfield settings", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	if (!fl_load_hyperspace_config(cf, path) || !fl_load_hyperspace_jump_config(cf, path) ||
		!fl_load_hyperspace_camera_config(cf, path) || !fl_load_hyperspace_looks_config(cf, path)) {
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	if (!XwaRemasterEnvironment_Init(vfs)) {
		AeronConfigFile_Destroy(cf);
		return 0;
	}

	const float classic_light_illuminance =
		(float)AeronConfigFile_GetFloat(cf, "classic_lighting.illuminance", 0.0);
	if (!isfinite(classic_light_illuminance) || classic_light_illuminance <= 0.0f ||
		classic_light_illuminance > FL_ILLUMINANCE_MAX) {
		Aeron_LogError("xwa.remaster", "%s: classic_lighting.illuminance must be above 0 and at most %g",
					   path, (double)FL_ILLUMINANCE_MAX);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	XwaRemaster_ConfigureClassicLightIlluminance(classic_light_illuminance);

	const float classic_radiance =
		(float)AeronConfigFile_GetFloat(cf, "classic_lighting.image_radiance", 0.0);
	if (!isfinite(classic_radiance) || classic_radiance <= 0.0f || classic_radiance > FL_RADIANCE_MAX) {
		Aeron_LogError("xwa.remaster", "%s: classic_lighting.image_radiance must be above 0 and at most %g",
					   path, (double)FL_RADIANCE_MAX);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	XwaRemaster_ConfigureClassicRadiance(classic_radiance);

	const float intensity_scale =
		(float)AeronConfigFile_GetFloat(cf, "environment_maps.intensity_scale", -1.0);
	if (!isfinite(intensity_scale) || intensity_scale < 0.0f ||
		intensity_scale > XWA_ENV_INTENSITY_SCALE_MAX) {
		Aeron_LogError("xwa.remaster", "%s: environment_maps.intensity_scale must be from 0 to %g", path,
					   (double)XWA_ENV_INTENSITY_SCALE_MAX);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	XwaRemasterEnvironment_ConfigureIntensityScale(intensity_scale);

	XwaShipPbrTuning pbr;
	memset(&pbr, 0, sizeof pbr);
	pbr.spec_geom_adapt = AeronConfigFile_GetBool(cf, "lighting.spec_geom_adapt", 0) ? 1.0f : 0.0f;
	pbr.ambient[0] = (float)AeronConfigFile_GetFloat(cf, "lighting.ambient_r", 0.0);
	pbr.ambient[1] = (float)AeronConfigFile_GetFloat(cf, "lighting.ambient_g", 0.0);
	pbr.ambient[2] = (float)AeronConfigFile_GetFloat(cf, "lighting.ambient_b", 0.0);
	if (!fl_pbr_config_valid(&pbr)) {
		Aeron_LogError("xwa.remaster", "%s: invalid lighting settings", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	XwaRemasterShip_ConfigurePbrTuning(&pbr);
	static const char* const map_ambient_keys[3] = { "lighting.map_ambient_min_r", "lighting.map_ambient_min_g",
													 "lighting.map_ambient_min_b" };
	for (int c = 0; c < 3; c++) {
		s.map_ambient_min[c] = (float)AeronConfigFile_GetFloat(cf, map_ambient_keys[c], 0.0);
		if (!isfinite(s.map_ambient_min[c]) || s.map_ambient_min[c] < 0.0f) {
			Aeron_LogError("xwa.remaster", "%s: %s must be a number, 0 or more", path, map_ambient_keys[c]);
			AeronConfigFile_Destroy(cf);
			return 0;
		}
	}
	static const char* const death_star_ambient_keys[3] = { "lighting.death_star_ambient_r",
															"lighting.death_star_ambient_g",
															"lighting.death_star_ambient_b" };
	for (int c = 0; c < 3; c++) {
		s.death_star_ambient[c] = (float)AeronConfigFile_GetFloat(cf, death_star_ambient_keys[c], 0.0);
		if (!fl_in_range(s.death_star_ambient[c], 0.0f, 1000.0f)) {
			Aeron_LogError("xwa.remaster", "%s: %s must be a number from 0 to 1000", path,
						   death_star_ambient_keys[c]);
			AeronConfigFile_Destroy(cf);
			return 0;
		}
	}
	const float emissive_scale = (float)AeronConfigFile_GetFloat(cf, "lighting.emissive_scale", 0.0);
	if (!isfinite(emissive_scale) || emissive_scale < 0.0f) {
		Aeron_LogError("xwa.remaster", "%s: lighting.emissive_scale must be finite and non-negative", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	AeronScene_SetEmissiveScale(emissive_scale);

	AeronScenePresent_ApplySettings(&tonemap);

	/* Fraction of the light the bloom scatters (process-wide present knob;
	 * 0 = off, chain skipped). */
	const float bloom_intensity = (float)AeronConfigFile_GetFloat(cf, "bloom.intensity", 0.0);
	if (!isfinite(bloom_intensity) || bloom_intensity < 0.0f || bloom_intensity > 1.0f) {
		Aeron_LogError("xwa.remaster", "%s: bloom.intensity must be between 0 and 1", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	AeronSceneBloom_SetIntensity(bloom_intensity);
	s.explosion_genus_emissive_strength =
		(float)AeronConfigFile_GetFloat(cf, "effects.explosion_genus_emissive_strength", 0.0);
	if (!isfinite(s.explosion_genus_emissive_strength) || s.explosion_genus_emissive_strength < 1.0f) {
		Aeron_LogError("xwa.remaster",
					   "%s: effects.explosion_genus_emissive_strength must be finite and at least 1", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.glow_mark_emissive_strength =
		(float)AeronConfigFile_GetFloat(cf, "effects.glow_mark_emissive_strength", 0.0);
	if (!isfinite(s.glow_mark_emissive_strength) || s.glow_mark_emissive_strength < 1.0f) {
		Aeron_LogError("xwa.remaster",
					   "%s: effects.glow_mark_emissive_strength must be finite and at least 1", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.cockpit_glow_mark_emissive_strength =
		(float)AeronConfigFile_GetFloat(cf, "effects.cockpit_glow_mark_emissive_strength", 0.0);
	if (!isfinite(s.cockpit_glow_mark_emissive_strength) || s.cockpit_glow_mark_emissive_strength < 0.0f) {
		Aeron_LogError("xwa.remaster",
					   "%s: effects.cockpit_glow_mark_emissive_strength must be a number, 0 or more", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.cockpit_hit_flash_emissive_strength =
		(float)AeronConfigFile_GetFloat(cf, "effects.cockpit_hit_flash_emissive_strength", -1.0);
	if (!isfinite(s.cockpit_hit_flash_emissive_strength) || s.cockpit_hit_flash_emissive_strength < 0.0f) {
		Aeron_LogError("xwa.remaster",
					   "%s: effects.cockpit_hit_flash_emissive_strength must be a number, 0 or more", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	XwaRemasterShip_SetBackfaceFill(AeronConfigFile_GetBool(cf, "models.backface_fill", 1));
	if (!XwaRemasterSuns_LoadFlareElements(cf, path)) {
		AeronConfigFile_Destroy(cf);
		return 0;
	}

	/* Dynamic Cockpit. */
	const XwaRemasterDcSettings dc_settings = {
		.enabled = AeronConfigFile_GetBool(cf, "dynamic_cockpit.enabled", 1),
		.classic_hud = AeronConfigFile_GetBool(cf, "dynamic_cockpit.classic_hud", 0),
		.holograms = AeronConfigFile_GetBool(cf, "dynamic_cockpit.holograms", 1),
		.display_emissive_strength =
			(float)AeronConfigFile_GetFloat(cf, "dynamic_cockpit.display_emissive_strength", 0.0),
		.hologram_emissive_strength =
			(float)AeronConfigFile_GetFloat(cf, "dynamic_cockpit.hologram_emissive_strength", 0.0),
	};
	if (!isfinite(dc_settings.display_emissive_strength) || dc_settings.display_emissive_strength <= 0.0f ||
		!isfinite(dc_settings.hologram_emissive_strength) || dc_settings.hologram_emissive_strength <= 0.0f) {
		Aeron_LogError("xwa.remaster", "%s: dynamic_cockpit emissive strengths must be finite and positive",
					   path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	XwaRemasterDc_SetSettings(&dc_settings);

	s.mb_quality = (int)AeronConfigFile_GetInt(cf, "motion_blur.quality", 0);
	if (s.mb_quality < 0 || s.mb_quality > 2) {
		Aeron_LogError("xwa.remaster", "%s: motion_blur.quality must be between 0 and 2", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.mb_shutter = (float)AeronConfigFile_GetFloat(cf, "motion_blur.shutter", 0.0);
	if (!isfinite(s.mb_shutter) || s.mb_shutter < 0.0f) {
		Aeron_LogError("xwa.remaster", "%s: motion_blur.shutter must be finite and non-negative", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.small_prepass_min_size = (float)AeronConfigFile_GetFloat(cf, "small_objects.prepass_min_size", 0.0);
	s.small_draw_min_size = (float)AeronConfigFile_GetFloat(cf, "small_objects.draw_min_size", 0.0);
	if (!isfinite(s.small_prepass_min_size) || s.small_prepass_min_size < 0.0f ||
		!isfinite(s.small_draw_min_size) || s.small_draw_min_size < 0.0f) {
		Aeron_LogError("xwa.remaster", "%s: small_objects sizes must be numbers, 0 or more", path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.mb_camera_blur = AeronConfigFile_GetBool(cf, "motion_blur.camera_blur", 0);
	s.mb_pause_keep_blur = AeronConfigFile_GetBool(cf, "motion_blur.pause_keep_blur", 0);
	s.mb_velocity_viz = AeronConfigFile_GetBool(cf, "motion_blur.velocity_viz", 0);

	const char* fsr_mode = AeronConfigFile_GetString(cf, "temporal_upscaling.mode", NULL);
	if (!AeronTemporal_ParseMode(fsr_mode, &s.fsr_mode)) {
		Aeron_LogError("xwa.remaster", "%s: invalid temporal_upscaling.mode '%s'", path, fsr_mode);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	if (s.fsr_mode != AERON_TEMPORAL_OFF && s.configured_msaa_samples != AERON_SAMPLE_COUNT_1) {
		Aeron_LogError("xwa.remaster",
					   "%s: temporal_upscaling.mode and presentation.msaa_samples cannot both be enabled",
					   path);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.fsr_sharpness = (float)AeronConfigFile_GetFloat(cf, "temporal_upscaling.sharpness", 0.0);
	if (s.fsr_sharpness < 0.0f || s.fsr_sharpness > 1.0f) {
		Aeron_LogError("xwa.remaster", "%s: temporal_upscaling.sharpness %.3f must be between 0 and 1", path,
					   (double)s.fsr_sharpness);
		AeronConfigFile_Destroy(cf);
		return 0;
	}
	s.ssao_default = s.ssao;
	s.shadows_default = s.shadows;
	s.hangar_lighting_default = s.hangar_lighting;
	s.exposure_default = s.exposure;
	s.plight_default = s.plight;
	s.texture_filtering_default = s.texture_filtering;
	s.hyperspace_params_default = s.hyperspace_params;
	s.hyperspace_jump_default = s.hyperspace_jump;
	s.hyperspace_preview_speed = 1.0f;
	fl_request_fsr_reset(FL_FSR_RESET_INITIAL);
	AeronConfigFile_Destroy(cf);
	s.config_loaded = 1;
	Aeron_LogInfo("xwa.remaster",
				  "flight ssao: quality %d intensity %.2f power %.2f radius %.1f bias %.2f "
				  "direct %.2f%s",
				  s.ssao.ssao_quality, (double)s.ssao.ssao_intensity, (double)s.ssao.ssao_power,
				  (double)s.ssao.ssao_radius_view, (double)s.ssao.ssao_bias_view, (double)s.ssao.ssao_direct,
				  s.ssao.ssao_debug_viz ? " (debug viz)" : "");
	if (s.texture_filtering.anisotropic) {
		Aeron_LogInfo("xwa.remaster", "flight mesh filtering: trilinear + %.0fx anisotropic",
					  (double)s.texture_filtering.max_anisotropy);
	} else {
		Aeron_LogInfo("xwa.remaster", "flight mesh filtering: trilinear");
	}
	Aeron_LogInfo("xwa.remaster", "flight FSR 3.1.4 mode: %s%s", AeronTemporal_ModeName(s.fsr_mode),
				  s.fsr_sharpness > 0.0f ? " with RCAS" : "");
	Aeron_LogInfo("xwa.remaster",
				  "presentation: %.2f Hz display, VSync divisor %d, %.2f fps target, %d frames in flight, "
				  "latency pacing %s",
				  Aeron_DisplayRefreshRate(), Aeron_PresentationVsyncDivisor(), Aeron_PresentationRate(),
				  Aeron_FramesInFlight(), Aeron_LatencyPacing() ? "on" : "off");
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

int XwaRemasterFlight_ProcessAssetsNeedPrepare(void) { return !s.process_assets_prepared; }

int XwaRemasterFlight_PrepareProcessAssets(AeronCommandBuffer* cmd, XwaRemasterAssets* assets) {
	if (!cmd || !assets || s.process_assets_prepared || !fl_cfg_ensure()) {
		return 0;
	}
	if (!XwaRemasterAssets_PrepareFlightFonts(assets, cmd)) {
		return 0;
	}
	int loaded_font_tiers = 0;
	for (int tier = 0; tier < 3; tier++) {
		loaded_font_tiers += XwaRemasterAssets_FlightFont(assets, tier, NULL) != NULL;
	}
	Aeron_LogInfo("xwa.remaster", "flight HUD fonts: %d/3 tiers loaded", loaded_font_tiers);
	if (loaded_font_tiers != 3) {
		return 0;
	}
	/* Authored environments are optional: failures leave the classic path. */
	return XwaRemasterEnvironment_PrepareProcess(cmd);
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
	s.shadows.enabled = in->enabled ? 1 : 0;
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
	s.shadows.light_angular_radius_degrees = fminf(fmaxf(s.shadows.light_angular_radius_degrees, 0.0f), 5.0f);
	s.shadows.max_filter_radius = fminf(fmaxf(s.shadows.max_filter_radius, 1.0f), 16.0f);
	s.shadows.pcss_min_filter_radius =
		fminf(fmaxf(s.shadows.pcss_min_filter_radius, 0.5f), s.shadows.max_filter_radius);
	s.shadows.normal_bias_texels = fminf(fmaxf(s.shadows.normal_bias_texels, 0.0f), 4.0f);
	s.shadows.depth_bias_texels = fminf(fmaxf(s.shadows.depth_bias_texels, 0.0f), 4.0f);
	s.shadows.transition_fraction = fminf(fmaxf(s.shadows.transition_fraction, 0.0f), 0.5f);
	s.shadows.distance_fade_fraction = fminf(fmaxf(s.shadows.distance_fade_fraction, 0.0f), 0.5f);
	s.shadows.debug_cascades = in->debug_cascades ? 1 : 0;
	s.shadows.receiver_local_near_distance_m =
		fminf(fmaxf(s.shadows.receiver_local_near_distance_m, 0.0f), 10.0f);
}

void XwaRemasterFlight_GetShadowsDefault(XwaFlightShadowParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.shadows_default;
	}
}

void XwaRemasterFlight_GetShadowAtlasDebug(int* enabled, int* cascade) {
	if (enabled)
		*enabled = s.shadow_debug_atlas;
	if (cascade)
		*cascade = s.shadow_debug_atlas_cascade;
}

/* Debug switches. They keep their value across flight sessions. */
static int s_probes_enabled = 1;
static int s_show_probes;

void XwaRemasterFlight_SetProbes(int enabled) { s_probes_enabled = enabled != 0; }

int XwaRemasterFlight_Probes(void) { return s_probes_enabled; }

void XwaRemasterFlight_SetShowProbes(int show) { s_show_probes = show != 0; }

int XwaRemasterFlight_ShowProbes(void) { return s_show_probes; }

uint32_t XwaRemasterFlight_ProbeSets(const XwaFlightProbeSetInfo** out) {
	if (out) {
		*out = s.probe_set_info;
	}
	return s.probe_set_count;
}

void XwaRemasterFlight_SetShadowAtlasDebug(int enabled, int cascade) {
	s.shadow_debug_atlas = enabled ? 1 : 0;
	if (cascade < -1)
		cascade = -1;
	if (cascade >= (int)s.shadows.cascade_count)
		cascade = (int)s.shadows.cascade_count - 1;
	s.shadow_debug_atlas_cascade = cascade;
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
	params->intensity = fminf(fmaxf(params->intensity, 0.0f), FL_ILLUMINANCE_MAX);
	for (int channel = 0; channel < 3; channel++) {
		params->color[channel] = fminf(fmaxf(params->color[channel], 0.0f), 4.0f);
	}
	params->ambient_ceiling = fminf(fmaxf(params->ambient_ceiling, 0.0f), FL_RADIANCE_MAX);
	params->ambient_sides = fminf(fmaxf(params->ambient_sides, 0.0f), FL_RADIANCE_MAX);
	params->ambient_floor = fminf(fmaxf(params->ambient_floor, 0.0f), FL_RADIANCE_MAX);
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

void XwaRemasterFlight_GetExposure(XwaFlightExposureParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.exposure;
	}
}

void XwaRemasterFlight_SetExposure(const XwaFlightExposureParams* in) {
	if (!in || !fl_cfg_ensure()) {
		return;
	}
	XwaFlightExposureParams p = *in;
	p.ev100 = fminf(fmaxf(p.ev100, FL_EXPOSURE_EV100_MIN), FL_EXPOSURE_EV100_MAX);
	p.compensation =
		fminf(fmaxf(p.compensation, -FL_EXPOSURE_COMPENSATION_MAX), FL_EXPOSURE_COMPENSATION_MAX);
	p.metering = fminf(fmaxf(p.metering, 0.0f), 1.0f);
	p.min_ev100 = fminf(fmaxf(p.min_ev100, FL_EXPOSURE_EV100_MIN), FL_EXPOSURE_EV100_MAX);
	p.max_ev100 = fminf(fmaxf(p.max_ev100, p.min_ev100), FL_EXPOSURE_EV100_MAX);
	p.brighten_speed = fminf(fmaxf(p.brighten_speed, 0.01f), FL_EXPOSURE_SPEED_MAX);
	p.darken_speed = fminf(fmaxf(p.darken_speed, 0.01f), FL_EXPOSURE_SPEED_MAX);
	p.low_percent = fminf(fmaxf(p.low_percent, 0.0f), 99.0f);
	p.high_percent = fminf(fmaxf(p.high_percent, p.low_percent + 1.0f), 100.0f);
	p.sky_weight = fminf(fmaxf(p.sky_weight, 0.0f), 1.0f);
	p.near_weight = fminf(fmaxf(p.near_weight, 0.0f), 1.0f);
	p.near_distance = fminf(fmaxf(p.near_distance, 0.0f), FL_EXPOSURE_NEAR_MAX);
	p.edge_weight = fminf(fmaxf(p.edge_weight, 0.0f), 1.0f);
	s.exposure = p;
}

void XwaRemasterFlight_GetExposureDefault(XwaFlightExposureParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.exposure_default;
	}
}

float XwaRemasterFlight_Exposure(void) {
	if (!fl_cfg_ensure()) {
		return 1.0f;
	}
	AeronSceneExposureReadout readout;
	if (s.exposure.metered && !s.map_present && s.scene && AeronScene_GetExposureReadout(s.scene, &readout)) {
		return readout.exposure;
	}
	return AeronScenePresent_ExposureFromEv100(s.exposure.ev100 - s.exposure.compensation);
}

float XwaRemasterFlight_LightMeterExposure(const XwaRemasterDirLight* lights, uint32_t count) {
	if (!fl_cfg_ensure()) {
		return 1.0f;
	}
	float illuminance = 0.0f;
	for (uint32_t i = 0; lights && i < count; i++) {
		const float* c = lights[i].color;
		illuminance = fmaxf(illuminance, 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]);
	}
	const float ev100 = illuminance > 0.0f && isfinite(illuminance)
							? log2f(illuminance * AERON_SCENE_LUMENS_PER_WATT * 100.0f / 250.0f)
							: s.exposure.ev100;
	return AeronScenePresent_ExposureFromEv100(ev100 - s.exposure.compensation);
}

int XwaRemasterFlight_PresentDebugView(void) { return s.present_debug_view; }

void XwaRemasterFlight_SetPresentDebugView(int view) {
	s.present_debug_view = view >= AERON_SCENE_PRESENT_IMAGE && view <= AERON_SCENE_PRESENT_METER_WEIGHT
							   ? view
							   : AERON_SCENE_PRESENT_IMAGE;
}

int XwaRemasterFlight_GetExposureReadout(AeronSceneExposureReadout* out) {
	return out && s.scene && AeronScene_GetExposureReadout(s.scene, out);
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
		s.plight.model_light_scale =
			isfinite(in->model_light_scale) ? fminf(fmaxf(in->model_light_scale, 0.01f), 1000.0f) : 1.0f;
		s.plight.cockpit_explosion_light_scale =
			isfinite(in->cockpit_explosion_light_scale)
				? fminf(fmaxf(in->cockpit_explosion_light_scale, 0.0f), 1.0f)
				: 1.0f;
		s.plight.cockpit_explosion_distance_m =
			isfinite(in->cockpit_explosion_distance_m)
				? fminf(fmaxf(in->cockpit_explosion_distance_m, 0.0f), FL_COCKPIT_EXPLOSION_DISTANCE_MAX)
				: 0.0f;
		for (int p = 0; p < XWA_SNAP_COCKPIT_LIGHT_COUNT; p++) {
			const float power = in->cockpit_lights[p].power_w;
			s.plight.cockpit_lights[p].power_w = isfinite(power) ? fminf(fmaxf(power, 0.0f), 1.0e7f) : 0.0f;
		}
		for (int g = 0; g < XWA_PARTICLE_LIGHT_GROUP_COUNT; g++) {
			const float power = in->particle_lights[g].power_w;
			s.plight.particle_lights[g].power_w = isfinite(power) ? fminf(fmaxf(power, 0.0f), 1.0e7f) : 0.0f;
		}
		const float headlight_power = in->headlight.power_w;
		s.plight.headlight.power_w =
			isfinite(headlight_power) ? fminf(fmaxf(headlight_power, 0.0f), 1.0e8f) : 0.0f;
	}
}

void XwaRemasterFlight_GetPointLightsDefault(XwaFlightPointLightParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.plight_default;
	}
}

void XwaRemasterFlight_GetPointLightStats(XwaFlightPointLightStats* out) {
	if (!out) {
		return;
	}
	memset(out, 0, sizeof *out);
	out->valid_count = s.pl_cand_count - s.pl_invalid_count;
	out->invalid_count = s.pl_invalid_count;
	out->candidate_overflow_count = s.pl_cand_dropped;
	out->generated_count = s.pl_cand_count + s.pl_cand_dropped;
	out->model_count = s.pl_model_count;
	out->culled_count = s.pl_culled[0];
	out->model_culled_count = s.pl_culled[1];
	out->dropped_count = s.pl_dropped[0];
	out->model_dropped_count = s.pl_dropped[1];
	AeronScene_GetClusteredLightStats(s.scene, &out->scene);
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
			Aeron_LogWarn("xwa.remaster",
						  "flight: anisotropic mesh sampler creation failed; keeping previous filtering");
			return 0;
		}
	}
	if (s.scene && !AeronScene_SetMeshSampler(s.scene, sampler)) {
		Aeron_DestroySampler(sampler);
		Aeron_LogError("xwa.remaster", "flight: mesh sampler configuration failed");
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

void XwaRemasterFlight_GetHyperspaceTunnel(XwaFlightHyperspaceTunnelParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.hyperspace_params;
	}
}

void XwaRemasterFlight_SetHyperspaceTunnel(const XwaFlightHyperspaceTunnelParams* in) {
	if (!in || !fl_cfg_ensure()) {
		return;
	}
	XwaFlightHyperspaceTunnelParams p = *in;
	if (!isfinite(p.travel_speed))
		p.travel_speed = s.hyperspace_params_default.travel_speed;
	if (!isfinite(p.rotation_speed))
		p.rotation_speed = s.hyperspace_params_default.rotation_speed;
	if (!isfinite(p.noise_scale))
		p.noise_scale = s.hyperspace_params_default.noise_scale;
	if (!isfinite(p.brightness))
		p.brightness = s.hyperspace_params_default.brightness;
	if (!isfinite(p.highlight_strength))
		p.highlight_strength = s.hyperspace_params_default.highlight_strength;
	if (!isfinite(p.focal_length))
		p.focal_length = s.hyperspace_params_default.focal_length;
	if (!isfinite(p.twist))
		p.twist = s.hyperspace_params_default.twist;
	if (!isfinite(p.cap_radius))
		p.cap_radius = s.hyperspace_params_default.cap_radius;
	if (!isfinite(p.cap_falloff))
		p.cap_falloff = s.hyperspace_params_default.cap_falloff;
	if (!isfinite(p.mesh_ambient_strength))
		p.mesh_ambient_strength = s.hyperspace_params_default.mesh_ambient_strength;
	if (!isfinite(p.mesh_environment_roughness))
		p.mesh_environment_roughness = s.hyperspace_params_default.mesh_environment_roughness;
	if (!isfinite(p.mesh_key_strength))
		p.mesh_key_strength = s.hyperspace_params_default.mesh_key_strength;
	p.travel_speed = fminf(fmaxf(p.travel_speed, 0.01f), 64.0f);
	p.rotation_speed = fminf(fmaxf(p.rotation_speed, -8.0f), 8.0f);
	p.noise_scale = fminf(fmaxf(p.noise_scale, 0.125f), 8.0f);
	p.brightness = fminf(fmaxf(p.brightness, 0.0f), 16.0f);
	p.highlight_strength = fminf(fmaxf(p.highlight_strength, 0.0f), 16.0f);
	p.focal_length = fminf(fmaxf(p.focal_length, 0.25f), 4.0f);
	p.twist = fminf(fmaxf(p.twist, -2.0f), 2.0f);
	p.cap_radius = fminf(fmaxf(p.cap_radius, 0.001f), 1.0f);
	p.cap_falloff = fminf(fmaxf(p.cap_falloff, 0.1f), 64.0f);
	p.mesh_ambient_strength = fminf(fmaxf(p.mesh_ambient_strength, 0.0f), 4.0f);
	p.mesh_environment_roughness = fminf(fmaxf(p.mesh_environment_roughness, 0.0f), 1.0f);
	p.mesh_key_strength = fminf(fmaxf(p.mesh_key_strength, 0.0f), 4.0f);
	for (int channel = 0; channel < 3; channel++) {
		if (!isfinite(p.dark_color[channel]))
			p.dark_color[channel] = s.hyperspace_params_default.dark_color[channel];
		if (!isfinite(p.body_color[channel]))
			p.body_color[channel] = s.hyperspace_params_default.body_color[channel];
		if (!isfinite(p.highlight_color[channel]))
			p.highlight_color[channel] = s.hyperspace_params_default.highlight_color[channel];
		if (!isfinite(p.cap_color[channel]))
			p.cap_color[channel] = s.hyperspace_params_default.cap_color[channel];
		p.dark_color[channel] = fminf(fmaxf(p.dark_color[channel], 0.0f), 16.0f);
		p.body_color[channel] = fminf(fmaxf(p.body_color[channel], 0.0f), 16.0f);
		p.highlight_color[channel] = fminf(fmaxf(p.highlight_color[channel], 0.0f), 16.0f);
		p.cap_color[channel] = fminf(fmaxf(p.cap_color[channel], 0.0f), 16.0f);
	}
	s.hyperspace_params = p;
	XwaRemasterHyperspace_SetParams(s.hyperspace, &p);
}

void XwaRemasterFlight_GetHyperspaceTunnelDefault(XwaFlightHyperspaceTunnelParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.hyperspace_params_default;
	}
}

void XwaRemasterFlight_GetHyperspaceJump(XwaFlightHyperspaceJumpParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.hyperspace_jump;
	}
}

void XwaRemasterFlight_SetHyperspaceJump(const XwaFlightHyperspaceJumpParams* in) {
	if (!in || !fl_cfg_ensure()) {
		return;
	}
	const XwaFlightHyperspaceJumpParams* d = &s.hyperspace_jump_default;
	XwaFlightHyperspaceJumpParams p = *in;
	/* A value that isn't a number goes back to its default; others are
	 * clamped to their range. */
#define FL_JUMP_SANITIZE(field, lo, hi) p.field = isfinite(p.field) ? fl_clampf(p.field, lo, hi) : d->field
	FL_JUMP_SANITIZE(streak_brightness, 0.0f, 16.0f);
	FL_JUMP_SANITIZE(whiteout_brightness, 1.0f, 128.0f);
	FL_JUMP_SANITIZE(brightness_range, 1.0f, 1000.0f);
	FL_JUMP_SANITIZE(streak_width_px, 0.5f, 8.0f);
	FL_JUMP_SANITIZE(width_from_brightness, 0.0f, 1.0f);
	FL_JUMP_SANITIZE(width_variation, 1.0f, 4.0f);
	FL_JUMP_SANITIZE(head_widening, 1.0f, 16.0f);
	FL_JUMP_SANITIZE(tail_taper, 0.0f, 1.0f);
	FL_JUMP_SANITIZE(head_taper, 0.0f, 1.0f);
	FL_JUMP_SANITIZE(taper_variation, 0.0f, 1.0f);
	FL_JUMP_SANITIZE(radial_widening, 0.0f, 4.0f);
	FL_JUMP_SANITIZE(glow_strength, 0.0f, 1.0f);
	FL_JUMP_SANITIZE(glow_width, 1.0f, 16.0f);
	FL_JUMP_SANITIZE(tail_brightness, 0.0f, 1.0f);
	FL_JUMP_SANITIZE(tail_color[0], 0.0f, 16.0f);
	FL_JUMP_SANITIZE(tail_color[1], 0.0f, 16.0f);
	FL_JUMP_SANITIZE(tail_color[2], 0.0f, 16.0f);
	FL_JUMP_SANITIZE(stretch_scale, 0.25f, 4.0f);
	FL_JUMP_SANITIZE(white_fade_seconds, 0.05f, 0.5f);
	FL_JUMP_SANITIZE(tunnel_entry_brightness, 1.0f, 16.0f);
	FL_JUMP_SANITIZE(tunnel_settle_seconds, 0.05f, 2.0f);
	FL_JUMP_SANITIZE(mesh_light_strength, 0.0f, 16.0f);
	FL_JUMP_SANITIZE(mesh_light_start, 0.0f, 2.0f);
	FL_JUMP_SANITIZE(mesh_light_curve, 0.25f, 8.0f);
	FL_JUMP_SANITIZE(tint[0], 0.0f, 16.0f);
	FL_JUMP_SANITIZE(tint[1], 0.0f, 16.0f);
	FL_JUMP_SANITIZE(tint[2], 0.0f, 16.0f);
	FL_JUMP_SANITIZE(warp_start, 0.0f, 2.0f);
	FL_JUMP_SANITIZE(warp_end, 0.2f, 2.5f);
	FL_JUMP_SANITIZE(warp_curve, 0.25f, 8.0f);
	FL_JUMP_SANITIZE(warp_distance_m, 0.0f, 200000.0f);
	FL_JUMP_SANITIZE(warp_sky_zoom, 1.0f, 16.0f);
	FL_JUMP_SANITIZE(warp_smear, 0.05f, 1.0f);
	FL_JUMP_SANITIZE(warp_sky_highlights, 1.0f, 3.0f);
	FL_JUMP_SANITIZE(warp_reveal_softness, 0.25f, 2.0f);
#undef FL_JUMP_SANITIZE
	if (p.star_count < FL_JUMP_STAR_COUNT_MIN)
		p.star_count = FL_JUMP_STAR_COUNT_MIN;
	if (p.star_count > FL_JUMP_STAR_COUNT_MAX)
		p.star_count = FL_JUMP_STAR_COUNT_MAX;
	p.warp_taps = p.warp_taps < FL_JUMP_WARP_TAPS_MIN ? FL_JUMP_WARP_TAPS_MIN
					: (p.warp_taps > FL_JUMP_WARP_TAPS_MAX ? FL_JUMP_WARP_TAPS_MAX : p.warp_taps);
	p.warp_sky_taps = p.warp_sky_taps < FL_JUMP_WARP_SKY_TAPS_MIN
						  ? FL_JUMP_WARP_SKY_TAPS_MIN
						  : (p.warp_sky_taps > FL_JUMP_WARP_SKY_TAPS_MAX ? FL_JUMP_WARP_SKY_TAPS_MAX
																		 : p.warp_sky_taps);
	/* The end stays after the start: a start moved past it pushes it. */
	if (p.warp_end < p.warp_start + FL_JUMP_WARP_MIN_SECONDS)
		p.warp_end = fminf(p.warp_start + FL_JUMP_WARP_MIN_SECONDS, 2.5f);
	if (p.warp_start > p.warp_end - FL_JUMP_WARP_MIN_SECONDS)
		p.warp_start = p.warp_end - FL_JUMP_WARP_MIN_SECONDS;
	p.exit_forward = p.exit_forward ? 1 : 0;
	p.warp_sky_half_resolution = p.warp_sky_half_resolution ? 1 : 0;
	s.hyperspace_jump = p;
	XwaRemasterHyperspace_SetJumpParams(s.hyperspace, &p);
}

void XwaRemasterFlight_GetHyperspaceJumpDefault(XwaFlightHyperspaceJumpParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.hyperspace_jump_default;
	}
}

int XwaRemasterFlight_HyperspaceLookCount(void) { return fl_cfg_ensure() ? s.hyperspace_look_count : 0; }

const char* XwaRemasterFlight_HyperspaceLookName(int index) {
	if (!fl_cfg_ensure() || index < 0 || index >= s.hyperspace_look_count) {
		return NULL;
	}
	return s.hyperspace_look_names[index];
}

int XwaRemasterFlight_FindHyperspaceLook(const char* name) {
	if (!name || !fl_cfg_ensure()) {
		return -1;
	}
	for (int i = 0; i < s.hyperspace_look_count; i++) {
		if (strcmp(s.hyperspace_look_names[i], name) == 0) {
			return i;
		}
	}
	return -1;
}

void XwaRemasterFlight_GetHyperspaceLook(int index, XwaFlightHyperspaceLookParams* out) {
	if (out && fl_cfg_ensure() && index >= 0 && index < s.hyperspace_look_count) {
		*out = s.hyperspace_looks[index];
	}
}

void XwaRemasterFlight_SetHyperspaceLook(int index, const XwaFlightHyperspaceLookParams* in) {
	if (!in || !fl_cfg_ensure() || index < 0 || index >= s.hyperspace_look_count) {
		return;
	}
	const XwaFlightHyperspaceLookParams* d = &s.hyperspace_looks_default[index];
	XwaFlightHyperspaceLookParams p = *in;
	/* A value that isn't a number goes back to its default; others are
	 * clamped to their range. */
#define FL_LOOK_SANITIZE(field, lo, hi) p.field = isfinite(p.field) ? fl_clampf(p.field, lo, hi) : d->field
	for (int channel = 0; channel < 3; channel++) {
		FL_LOOK_SANITIZE(dark_color[channel], 0.0f, 16.0f);
		FL_LOOK_SANITIZE(body_color[channel], 0.0f, 16.0f);
		FL_LOOK_SANITIZE(highlight_color[channel], 0.0f, 16.0f);
		FL_LOOK_SANITIZE(cap_color[channel], 0.0f, 16.0f);
		FL_LOOK_SANITIZE(edge_color[channel], 0.0f, 16.0f);
		FL_LOOK_SANITIZE(tint[channel], 0.0f, 16.0f);
		FL_LOOK_SANITIZE(tail_color[channel], 0.0f, 16.0f);
	}
	FL_LOOK_SANITIZE(edge_width, 0.01f, 4.0f);
	FL_LOOK_SANITIZE(front_noise, 0.0f, 8.0f);
	FL_LOOK_SANITIZE(front_noise_scale, 0.05f, 16.0f);
	FL_LOOK_SANITIZE(front_warp, 0.0f, 4.0f);
	FL_LOOK_SANITIZE(blend_start, 0.0f, 1.0f);
	FL_LOOK_SANITIZE(blend_end, 0.0f, 1.0f);
#undef FL_LOOK_SANITIZE
	if (p.blend_end < p.blend_start) {
		p.blend_end = p.blend_start;
	}
	fl_sanitize_hyperspace_camera(&p.camera, &d->camera);
	s.hyperspace_looks[index] = p;
}

void XwaRemasterFlight_GetHyperspaceCamera(XwaFlightHyperspaceCameraParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.hyperspace_camera;
	}
}

void XwaRemasterFlight_SetHyperspaceCamera(const XwaFlightHyperspaceCameraParams* in) {
	if (!in || !fl_cfg_ensure()) {
		return;
	}
	XwaFlightHyperspaceCameraParams p = *in;
	fl_sanitize_hyperspace_camera(&p, &s.hyperspace_camera_default);
	s.hyperspace_camera = p;
}

void XwaRemasterFlight_GetHyperspaceCameraDefault(XwaFlightHyperspaceCameraParams* out) {
	if (out && fl_cfg_ensure()) {
		*out = s.hyperspace_camera_default;
	}
}

void XwaRemasterFlight_GetHyperspaceLookDefault(int index, XwaFlightHyperspaceLookParams* out) {
	if (out && fl_cfg_ensure() && index >= 0 && index < s.hyperspace_look_count) {
		*out = s.hyperspace_looks_default[index];
	}
}

void XwaRemasterFlight_SetHyperspacePreview(XwaFlightHyperspacePreview mode) {
	if (mode != XWA_FLIGHT_HYPERSPACE_PREVIEW_TUNNEL && mode != XWA_FLIGHT_HYPERSPACE_PREVIEW_JUMP_CYCLE) {
		mode = XWA_FLIGHT_HYPERSPACE_PREVIEW_OFF;
	}
	if (mode != s.hyperspace_preview) {
		/* Each mode starts from its beginning. */
		s.hyperspace_preview_seconds = 0.0;
		s.hyperspace_preview_host_us = XwaTime_GetElapsedUs();
	}
	s.hyperspace_preview = mode;
}

XwaFlightHyperspacePreview XwaRemasterFlight_GetHyperspacePreview(void) { return s.hyperspace_preview; }

void XwaRemasterFlight_SetHyperspacePreviewSpeed(float speed) {
	s.hyperspace_preview_speed = isfinite(speed) ? fl_clampf(speed, 0.05f, 1.0f) : 1.0f;
}

float XwaRemasterFlight_GetHyperspacePreviewSpeed(void) {
	return s.hyperspace_preview_speed > 0.0f ? s.hyperspace_preview_speed : 1.0f;
}

void XwaRemasterFlight_SetHyperspacePreviewLook(int index) {
	s.hyperspace_preview_look = index >= 0 && index < s.hyperspace_look_count ? index : -1;
}

int XwaRemasterFlight_GetHyperspacePreviewLook(void) { return s.hyperspace_preview_look; }

void XwaRemasterFlight_SetHyperspacePreviewLookBlend(float blend) {
	s.hyperspace_preview_look_blend = isfinite(blend) ? fl_clampf(blend, 0.0f, 1.0f) : 1.0f;
}

float XwaRemasterFlight_GetHyperspacePreviewLookBlend(void) { return s.hyperspace_preview_look_blend; }

void XwaRemasterFlight_SetHyperspaceWarpView(XwaFlightHyperspaceWarpView view) {
	s.hyperspace_warp_view =
		view >= 0 && view < XWA_FLIGHT_HYPERSPACE_WARP_VIEW_COUNT ? view : XWA_FLIGHT_HYPERSPACE_WARP_VIEW_IMAGE;
}

XwaFlightHyperspaceWarpView XwaRemasterFlight_GetHyperspaceWarpView(void) { return s.hyperspace_warp_view; }

float XwaRemasterFlight_GetCockpitFovMinAspect(void) { return s.cockpit_fov_min_aspect; }

void XwaRemasterFlight_SetCockpitFovMinAspect(float aspect) {
	if (fl_in_range(aspect, 0.0f, 4.0f))
		s.cockpit_fov_min_aspect = aspect;
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
		Aeron_LogWarn("xwa.remaster", "FSR cannot be enabled while MSAA is active");
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
	XwaRemasterDc_ReleaseTargets();
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
			Aeron_LogError("xwa.remaster", "flight: trail renderer create failed");
			return 0;
		}
	}
	if (!s.particles) {
		s.particles = XwaRemasterParticles_Create();
		if (!s.particles) {
			Aeron_LogError("xwa.remaster", "flight: particle renderer create failed");
			return 0;
		}
	}
	if (!s.hyperspace) {
		s.hyperspace = XwaRemasterHyperspace_Create(&s.hyperspace_params, &s.hyperspace_jump);
		if (!s.hyperspace) {
			Aeron_LogError("xwa.remaster", "flight: hyperspace renderer create failed");
			return 0;
		}
	}
	if (!s.sky_stars) {
		s.sky_stars = XwaRemasterSkyStars_Create();
		if (!s.sky_stars) {
			Aeron_LogError("xwa.remaster", "flight: starfield renderer create failed");
			return 0;
		}
	}
	if (!s.speed_effect) {
		s.speed_effect = XwaRemasterSpeedEffect_Create();
		if (!s.speed_effect) {
			Aeron_LogError("xwa.remaster", "flight: speed effect create failed");
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
		Aeron_LogError("xwa.remaster", "flight: present resources create failed");
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
		.view_space_to_meters = AERON_OPT_METERS_PER_UNIT,
	});
	if (!s.scene) {
		Aeron_LogError("xwa.remaster", "flight: scene create failed at %dx%d", target_w, target_h);
		s.rt_w = s.rt_h = 0;
		return 0;
	}
	s.scene_requested_samples = requested_samples;
	s.scene_serial++;
	/* Decode the classic RGB555 clear color 0x0001 into the linear HDR scene. */
	const float classic_clear_blue = powf(2.0f / 31.0f, 2.2f);
	AeronScene_SetClearColor(s.scene, (const float[4]) { 0.0f, 0.0f, classic_clear_blue, 1.0f });
	if (s.mesh_sampler && !AeronScene_SetMeshSampler(s.scene, s.mesh_sampler)) {
		Aeron_LogError("xwa.remaster", "flight: scene mesh sampler configuration failed");
		return 0;
	}
	AeronScene_SetEffectAtlas(s.scene, XwaRemasterShip_EffectAtlas());

	s.bloom = AeronSceneBloom_Create(target_w, target_h);
	s.present_rt =
		Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = target_w,
															.height = target_h,
															.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
															.debug_name = "xwa.flight.present" });
	if (!s.bloom) {
		Aeron_LogError("xwa.remaster", "flight: bloom create failed");
		return 0;
	}
	if (!s.present_rt) {
		Aeron_LogError("xwa.remaster", "flight: present target create failed");
		return 0;
	}
	Aeron_LogInfo("xwa.remaster", "flight render targets: %dx%d", target_w, target_h);
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

/* If `view_rotation` isn't NULL, it turns the view: the eye-space
 * coordinates of the built camera are `view_rotation` times those of `cam`.
 * The hyperspace camera shake and jolt use it (fl_hyperspace_view_rotation). */
static int fl_build_view_at_origin(const XwaFlightCamera* cam, const int32_t origin_world[3], int target_w,
								   int target_h, const float view_rotation[9], XwaRemasterFlightView* out) {
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
	AeronWorld_LocalI32(origin_world, cam->world_pos, scene_camera->pos);
	/* The camera includes the enhanced camera's sway. */
	for (int axis = 0; axis < 3; axis++) {
		scene_camera->pos[axis] += cam->world_pos_fraction[axis] + cam->sway[axis];
	}
	const float aspect = (float)target_w / (float)target_h;
	/* With a craft's field of view, a window narrower than the minimum
	 * aspect keeps the horizontal field of view of that aspect: the whole
	 * view gets wider. */
	const float widen =
		cam->cockpit_fov && s.cockpit_fov_min_aspect > aspect ? s.cockpit_fov_min_aspect / aspect : 1.0f;
	scene_camera->v_half_rad = atanf(hh * widen / ps);
	scene_camera->h_half_rad = atanf(tanf(scene_camera->v_half_rad) * aspect);
	scene_camera->near_z = FL_NEAR_Z;
	scene_camera->proj_x_offset = (cx - hw) / hw;
	scene_camera->proj_y_offset = (hh - cy) / hh;
	scene_camera->viewport = (AeronRectI) { 0, 0, target_w, target_h };
	if (view_rotation) {
		float rows[9];
		for (int row = 0; row < 3; row++) {
			for (int column = 0; column < 3; column++) {
				rows[row * 3 + column] = view_rotation[row * 3 + 0] * cam->rows[0 * 3 + column] +
										 view_rotation[row * 3 + 1] * cam->rows[1 * 3 + column] +
										 view_rotation[row * 3 + 2] * cam->rows[2 * 3 + column];
			}
		}
		fl_rows_to_quat(rows, scene_camera->ori);
	} else {
		fl_rows_to_quat(cam->rows, scene_camera->ori);
	}
	out->viewport = scene_camera->viewport;
	out->classic_pixel_scale = (float)target_h / source_h;
	out->projection_pixel_scale = out->classic_pixel_scale / widen;
	AeronScene_ComputeViewProj(scene_camera, out->view_proj);
	return 1;
}

int XwaRemasterFlight_BuildView(const XwaFlightCamera* cam, int target_w, int target_h,
								XwaRemasterFlightView* out) {
	return cam ? fl_build_view_at_origin(cam, cam->world_pos, target_w, target_h, NULL, out) : 0;
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
	AeronWorld_LocalI32(view->origin_world, world, local);
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

int XwaRemasterFlight_ClipProjectLine(const XwaRemasterFlightView* view, float a[3], float b[3], float* ax,
									  float* ay, float* bx, float* by) {
	const float near_z = view->camera.near_z;
	if (a[2] < near_z && b[2] < near_z)
		return 0;
	if (a[2] < near_z || b[2] < near_z) {
		float* behind = a[2] < near_z ? a : b;
		const float* front = a[2] < near_z ? b : a;
		const float t = (near_z - behind[2]) / (front[2] - behind[2]);
		behind[0] += (front[0] - behind[0]) * t;
		behind[1] += (front[1] - behind[1]) * t;
		behind[2] = near_z;
	}
	return XwaRemasterFlight_ProjectView(view, a, ax, ay) && XwaRemasterFlight_ProjectView(view, b, bx, by);
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

/* Model metres -> scene units from a model->space basis + native-unit translation, the
 * transpose consumption the model preview validated (Math3D_RotateVec3
 * is column-major over the stored rows). */
static void fl_model_matrix(const float basis[9], const float delta[3], float m[16]) {
	const float scale = AERON_OPT_UNITS_PER_METER;
	m[0] = scale * basis[0];
	m[1] = scale * basis[3];
	m[2] = scale * basis[6];
	m[3] = delta[0];
	m[4] = scale * basis[1];
	m[5] = scale * basis[4];
	m[6] = scale * basis[7];
	m[7] = delta[1];
	m[8] = scale * basis[2];
	m[9] = scale * basis[5];
	m[10] = scale * basis[8];
	m[11] = delta[2];
	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = 0.0f;
	m[15] = 1.0f;
}

void XwaRemasterFlight_TransformOptPoint(const float model_meters_to_scene[16], const float point_opt[3],
										 float out_scene[3]) {
	const float point_meters[3] = {
		point_opt[0] * AERON_OPT_METERS_PER_UNIT,
		point_opt[1] * AERON_OPT_METERS_PER_UNIT,
		point_opt[2] * AERON_OPT_METERS_PER_UNIT,
	};
	for (int row = 0; row < 3; row++) {
		const float* m = &model_meters_to_scene[row * 4];
		out_scene[row] = m[0] * point_meters[0] + m[1] * point_meters[1] + m[2] * point_meters[2] + m[3];
	}
}

static int fl_is_death_star_beam(const XwaSnapshot* snap, const XwaFlightObject* object) {
	const XwaDeathStarBeam* beam = &snap->death_star_beam;
	return beam->active && object->object_type == XWA_SNAP_TYPE_LASER_IMPERIAL_DS &&
		   object->slot == beam->object_slot && object->signature == beam->object_signature;
}

static void fl_apply_death_star_beam_scale(const XwaSnapshot* snap, float model[16]) {
	const float scale[3] = { FL_DS_LASER_TRANSVERSE_SCALE, snap->death_star_beam.length_scale,
							 FL_DS_LASER_TRANSVERSE_SCALE };
	for (int row = 0; row < 3; row++) {
		for (int axis = 0; axis < 3; axis++) {
			model[row * 4 + axis] *= scale[axis];
		}
	}
}

static void fl_object_local(const XwaFlightObject* object, float out[3]) {
	AeronWorld_LocalI32(s.origin_world, object->world_pos, out);
}

static int fl_object_is_projectile(const XwaFlightObject* object) {
	return object->genus == XWA_SNAP_GENUS_PLAYER_PROJECTILE ||
		   object->genus == XWA_SNAP_GENUS_NPC_PROJECTILE;
}

/* Returns the name of the model to draw for an object. A shot can use a
 * weapon model from the settings of the craft that fired it. */
static const char* fl_object_model_name(const XwaFlightObject* object) {
	return fl_object_is_projectile(object)
			   ? XwaSnapshotExport_ShotModelName(object->object_type, object->source_object_type)
			   : XwaSnapshotExport_ModelName(object->object_type);
}

/* The meshes found by model name in this frame, kept by name pointer. Every
 * object and projectile looks up its mesh once or more per frame, and a name
 * pointer from fl_object_model_name names the same model for the whole frame.
 * An entry is valid for one frame and one mesh generation
 * (XwaRemasterShip_MeshGeneration). */
#define FL_MESH_MEMO_SIZE 128u

typedef struct FlMeshMemo {
	const char* name;
	AeronSceneMesh* mesh;
	uint64_t generation;
	uint32_t frame;
} FlMeshMemo;

static FlMeshMemo s_mesh_memo[FL_MESH_MEMO_SIZE];
static uint32_t s_mesh_memo_frame;

/* Same as XwaRemasterShip_MeshForName, but cached for the frame. */
static AeronSceneMesh* fl_mesh_for_name(const char* name) {
	if (!name) {
		return XwaRemasterShip_MeshForName(name);
	}
	const uint64_t generation = XwaRemasterShip_MeshGeneration();
	FlMeshMemo* memo = &s_mesh_memo[((uintptr_t)name >> 3) & (FL_MESH_MEMO_SIZE - 1u)];
	if (memo->name != name || memo->frame != s_mesh_memo_frame || memo->generation != generation) {
		memo->name = name;
		memo->mesh = XwaRemasterShip_MeshForName(name);
		memo->generation = generation;
		memo->frame = s_mesh_memo_frame;
	}
	return memo->mesh;
}

static int fl_object_in_render_region(const XwaFlightObject* object, uint8_t region) {
	return object->render_region == region && object->slot_class != XWA_SNAP_SLOT_OTHER;
}

/* Shared pose law for ordinary flight and the map. Projectile roll alignment
 * is view-dependent; every other object uses the captured orientation as-is. */
static void fl_object_pose(const XwaFlightObject* object, const int32_t camera_world[3], int roll_align,
						   float basis[9], float local[3], float model[16]) {
	float cur[3][3];
	if (object->has_mobj && !object->orient_dirty)
		fl_curmat_from_cached(object->rows, cur);
	else
		fl_curmat_from_euler(object, cur);
	if (roll_align) {
		float camera_delta[3];
		AeronWorld_DeltaI32(camera_world, object->world_pos, camera_delta);
		const float side_dot =
			camera_delta[0] * cur[0][0] + camera_delta[1] * cur[0][1] + camera_delta[2] * cur[0][2];
		const float up_dot =
			camera_delta[0] * cur[1][0] + camera_delta[1] * cur[1][1] + camera_delta[2] * cur[1][2];
		if (side_dot != 0.0f || up_dot != 0.0f) {
			const int dq16 = (int)lrintf(atan2f(up_dot, side_dot) / FL_Q16_TO_RAD) - 0x4000;
			fl_transformaxes(cur, cur[2], dq16);
		}
	}
	fl_object_world(cur, basis);
	fl_object_local(object, local);
	fl_model_matrix(basis, local, model);
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
	AeronWorld_LocalI32(origin_world, object->world_pos, local);
	fl_model_matrix(basis, local, out);
	return 1;
}

/* Prev-frame transform for the motion-blur velocity prepass. */
static const XwaFlightObject* fl_instance_motion(AeronSceneMeshInstance* inst, const XwaFlightObject* current,
												 uint32_t walk_index, int is_bolt) {
	if (!s.mb_enabled) {
		inst->zero_velocity = 1;
		return NULL;
	}
	/* Tunnel slots are static-world proxies recycled as the active window advances. */
	if (current->genus == XWA_SNAP_GENUS_DS_TUNNEL) {
		memcpy(inst->prev_transform, inst->transform, sizeof inst->prev_transform);
		return current;
	}
	if (s.mb_prev_index[walk_index] < 0) {
		inst->zero_velocity = 1;
		return NULL;
	}
	const XwaFlightObject* pf = &s.mb_prev_snap->flight_objects[s.mb_prev_index[walk_index]];
	float prev_local[3];
	AeronWorld_LocalI32(s.origin_world, pf->world_pos, prev_local);
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
	return slot >= 0 ? XwaSnapshot_FlightObjectAtSlot(snap, (uint32_t)slot) : NULL;
}

static const XwaFlightObject* fl_cockpit_anchor(const XwaSnapshot* snap, const XwaFlightCamera* cam) {
	if (!snap || !cam) {
		return NULL;
	}
	const int32_t slot = cam->in_hangar ? cam->player_obj_idx : cam->focus_obj_idx;
	const XwaFlightObject* anchor = fl_object_by_slot(snap, slot);
	return anchor && fl_object_in_render_region(anchor, cam->region) &&
				   anchor->genus != XWA_SNAP_GENUS_EXPLOSION
			   ? anchor
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
		memcmp(cam->world_pos_fraction, prev_cam->world_pos_fraction, sizeof cam->world_pos_fraction) != 0 ||
		memcmp(cam->sway, prev_cam->sway, sizeof cam->sway) != 0 ||
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
		   memcmp(cockpit->seat_eye_world_offset, prev_cockpit->seat_eye_world_offset,
				  sizeof cockpit->seat_eye_world_offset) != 0 ||
		   memcmp(cockpit->camera_pan, prev_cockpit->camera_pan, sizeof cockpit->camera_pan) != 0;
}

/* Rotate inverted gunner cockpits about the model-local X axis,
 * keeping the seat's eye point fixed relative to the camera. */
static void fl_apply_inverted_cockpit_pivot(float basis[9], const XwaCockpit* cockpit,
										   const float sway[3], const float camera_local[3], float position[3]) {
	for (int axis = 0; axis < 3; ++axis) {
		basis[3 + axis] = -basis[3 + axis];
		basis[6 + axis] = -basis[6 + axis];
	}
	for (int axis = 0; axis < 3; ++axis) {
		const float rotated_eye = basis[axis] * cockpit->seat_eye_local[0] +
								  basis[3 + axis] * cockpit->seat_eye_local[1] +
								  basis[6 + axis] * cockpit->seat_eye_local[2];
		position[axis] = camera_local[axis] - cockpit->camera_pan[axis] * 0.0625f -
						 sway[axis] - rotated_eye;
	}
}

/* `sway` is XwaFlightCamera.sway. The camera at `camera_local` includes it, so
 * the cockpit is placed back by it, at the seat. */
static int fl_cockpit_model_matrix(const XwaCockpit* cockpit, const float sway[3],
								   const XwaFlightObject* anchor, const float camera_rows[9],
								   const float camera_local[3], float out[16]) {
	if (!cockpit || !sway || !anchor || !camera_rows || !camera_local || !out) {
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
		eye_offset[axis] =
			-(cockpit->seat_eye_world_offset[axis] + cockpit->camera_pan[axis] * 0.0625f + sway[axis]);
	}
	float delta[3];
	fl_world_to_view(camera_rows, eye_offset[0], eye_offset[1], eye_offset[2], delta);

	float position[3];
	for (int axis = 0; axis < 3; ++axis) {
		position[axis] = camera_local[axis] + camera_rows[0 * 3 + axis] * delta[0] +
						 camera_rows[1 * 3 + axis] * delta[1] + camera_rows[2 * 3 + axis] * delta[2];
	}
	if (cockpit->inverted) {
		fl_apply_inverted_cockpit_pivot(basis, cockpit, sway, camera_local, position);
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
/* Scene and camera a view-space quad is submitted to: the flight frame's,
 * or an environment capture's. */
typedef struct FlQuadTarget {
	AeronScene3D* scene;
	const float* crows;
	const float* camera_local;
} FlQuadTarget;

/* Converts a quad's view-space corners to render-local space:
 * local = camera_local + crows^T * view. */
static void fl_view_corners_to_local(const FlQuadTarget* target, const float corners[4][3], float out[4][3]) {
	for (int v = 0; v < 4; v++) {
		for (int j = 0; j < 3; j++) {
			out[v][j] = target->camera_local[j] + target->crows[0 * 3 + j] * corners[v][0] +
						target->crows[1 * 3 + j] * corners[v][1] + target->crows[2 * 3 + j] * corners[v][2];
		}
	}
}

static void fl_submit_view_quad_to(const FlQuadTarget* target, AeronSceneBillboardStage stage,
								   const float corners[4][3], const float uvs[4][2], const XwaAssetRef* ref,
								   uint32_t argb, const float* anchor_local, float depth_bias,
								   const float (*prev_corners)[3], float emissive_strength) {
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
	tint[0] = XwaRemaster_SrgbToLinear((float)((argb >> 16) & 0xffu) / 255.0f) * tint[3] * emissive_strength;
	tint[1] = XwaRemaster_SrgbToLinear((float)((argb >> 8) & 0xffu) / 255.0f) * tint[3] * emissive_strength;
	tint[2] = XwaRemaster_SrgbToLinear((float)(argb & 0xffu) / 255.0f) * tint[3] * emissive_strength;
	const float du = ref->u1 - ref->u0;
	const float dv = ref->v1 - ref->v0;
	fl_view_corners_to_local(target, corners, d.corners);
	for (int v = 0; v < 4; v++) {
		d.uv[v][0] = ref->u0 + uvs[v][0] * du;
		d.uv[v][1] = ref->v0 + uvs[v][1] * dv;
		memcpy(d.colors[v], tint, sizeof tint);
	}
	if (prev_corners)
		d.prev_corners = prev_corners; /* copied by AddBillboard */
	AeronScene_AddBillboard(target->scene, &d);
}

static void fl_submit_view_quad_ex(AeronSceneBillboardStage stage, const float corners[4][3],
								   const float uvs[4][2], const XwaAssetRef* ref, uint32_t argb,
								   const float* anchor_local, float depth_bias,
								   const float (*prev_corners)[3], float emissive_strength) {
	const FlQuadTarget target = { s.scene, s.crows, s.camera_local };
	fl_submit_view_quad_to(&target, stage, corners, uvs, ref, argb, anchor_local, depth_bias, prev_corners,
						   emissive_strength);
}

/* The corona of a procedural sun moves by this many units per second of
 * game time. */
#define FL_STAR_TIME_PER_SECOND 0.6
/* Each sun's corona time is offset by this many units times its backdrop
 * index, so two suns don't show the same rays. */
#define FL_STAR_TIME_OFFSET 37.0

/* Queues a backdrop sun as a procedural star on its quad (AeronSceneBillboardStar).
 * The sun's colour comes from its image's `light`; a sun without one is
 * white. `radiance` is the radiance of the white of the sun's disc. */
static void fl_submit_star_quad(const FlBackdropQuad* q, const XwaSnapshot* snap, float radiance) {
	const FlQuadTarget target = { s.scene, s.crows, s.camera_local };
	AeronSceneBillboardDesc d;
	memset(&d, 0, sizeof d);
	d.blend = AERON_SCENE_BILLBOARD_BLEND_PMA;
	d.stage = AERON_SCENE_BILLBOARD_STAGE_SKY;
	d.shader = AERON_SCENE_BILLBOARD_SHADER_STAR;
	for (int c = 0; c < 3; c++) {
		d.star.tint[c] = q->ref.has_light ? q->ref.light[c] : 1.0f;
	}
	d.star.time =
		(float)((double)snap->game_time_ticks * XWA_SNAP_GAME_TICK_SECONDS * FL_STAR_TIME_PER_SECOND +
				(double)q->sun * FL_STAR_TIME_OFFSET);
	d.star.core_radiance = radiance * snap->suns.core_brightness;
	fl_view_corners_to_local(&target, q->corners, d.corners);
	memcpy(d.uv, q->uvs, sizeof d.uv);
	for (int v = 0; v < 4; v++) {
		d.colors[v][0] = d.colors[v][1] = d.colors[v][2] = radiance;
		d.colors[v][3] = 1.0f;
	}
	AeronScene_AddBillboard(s.scene, &d);
}

static void fl_submit_view_quad(AeronSceneBillboardStage stage, const float corners[4][3],
								const float uvs[4][2], const XwaAssetRef* ref, uint32_t argb,
								const float* anchor_local) {
	fl_submit_view_quad_ex(stage, corners, uvs, ref, argb, anchor_local, 0.0f, NULL, 1.0f);
}

/* Resolve a object-type-bound DAT frame from the baked resdata group
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

static int fl_backdrop_is_sun(const XwaBackdrop* b) {
	return b->object_type >= XWA_SNAP_TYPE_BACKDROP_SUN_FIRST &&
		   b->object_type <= XWA_SNAP_TYPE_BACKDROP_SUN_LAST;
}

/* Backdrop selection for fl_derive_backdrops: every record, every record
 * except backdrop suns, or one record index (>= 0). */
enum { FL_BACKDROPS_ALL = -1, FL_BACKDROPS_NO_SUNS = XWA_FLIGHT_CAPTURE_BACKDROPS_NO_SUNS };

/* Whether `rule` hides the DAT image that `object_type` draws for `frame`. */
static int fl_backdrop_hidden(XwaRemasterAssets* assets, const XwaEnvironmentBackdropRule* rule,
							  int object_type, int frame) {
	int group = 0;
	int image = 0;
	return rule && XwaRemasterAssets_FlightModelImageId(assets, object_type, frame, &group, &image) &&
		   XwaRemasterEnvironment_BackdropHidden(rule, group, image);
}

/* Logs the DAT image of each backdrop of the region, and whether `rule`
 * hides it, once per mission and region, so the manifest's hide and show
 * lists can be checked against the mission. */
static void fl_log_kept_backdrops(const XwaSnapshot* snap, XwaRemasterAssets* assets,
								  const XwaEnvironmentBackdropRule* rule) {
	static char logged_mission[sizeof snap->mission_path];
	static int logged_region = -1;
	const int region = snap->flight_camera.mission_region;
	if (region == logged_region && strcmp(logged_mission, snap->mission_path) == 0) {
		return;
	}
	logged_region = region;
	snprintf(logged_mission, sizeof logged_mission, "%s", snap->mission_path);
	char line[1024];
	size_t length = 0;
	line[0] = '\0';
	for (uint32_t i = 0; i < snap->backdrop_count && length + 32 < sizeof line; i++) {
		const XwaBackdrop* b = &snap->backdrops[i];
		const int frame = (b->flags & 1u) ? b->frame : 1;
		int group = 0;
		int image = 0;
		if (!XwaRemasterAssets_FlightModelImageId(assets, b->object_type, frame, &group, &image)) {
			continue;
		}
		length +=
			(size_t)snprintf(line + length, sizeof line - length, " %d-%d%s", group, image,
							 XwaRemasterEnvironment_BackdropHidden(rule, group, image) ? " (hidden)" : "");
	}
	Aeron_LogInfo("xwa.remaster", "environment: region %d keeps its backdrops:%s", region,
				  line[0] ? line : " none");
}

/* `camera_independent` (environment capture) replaces the axis quads'
 * camera-hemisphere gate with the record's own direction: a quad is kept
 * when a camera looking at it would draw it. `hide`, when set, leaves out
 * the quads whose image it hides (a sky with `backdrops: keep`). */
static uint32_t fl_derive_backdrops(const XwaSnapshot* snap, const float crows[9], XwaRemasterAssets* assets,
									int filter, int camera_independent,
									const XwaEnvironmentBackdropRule* hide, FlBackdropQuad* out,
									uint32_t capacity) {
	uint32_t count = 0;
	for (uint32_t i = 0; i < snap->backdrop_count; i++) {
		const XwaBackdrop* b = &snap->backdrops[i];
		if (b->hidden || (filter >= 0 && (uint32_t)filter != i) ||
			(filter == FL_BACKDROPS_NO_SUNS && fl_backdrop_is_sun(b))) {
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
			if (!fl_resolve_type_frame(assets, b->object_type, frame, &ref)) {
				continue;
			}
			int32_t hh = b->strip_half_height;
			int prev_h = ref.classic_h;
			int seg_in_frame = 1;
			for (int seg = 1; seg <= b->strip_segment_count && count < capacity; seg++) {
				float u0;
				if (seg > 1) {
					seg_in_frame++;
					if (seg_in_frame > b->strip_segments_per_frame) {
						seg_in_frame -= b->strip_segments_per_frame;
						frame++;
						if (!fl_resolve_type_frame(assets, b->object_type, frame, &ref)) {
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
				if (fl_backdrop_hidden(assets, hide, b->object_type, frame)) {
					continue;
				}
				FlBackdropQuad* q = &out[count++];
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
				/* A sun's strip that fits in one frame has u from 0 to 1 across
				 * its segments, so a procedural star can be drawn on them. */
				q->sun = fl_backdrop_is_sun(b) && b->strip_segment_count <= b->strip_segments_per_frame
							 ? (int)i
							 : -1;
			}
		} else if (b->side == 4 || b->side == 5) {
			/* Axis quad: half width along world Y, half height along
			 * world X, both rotated by the camera; extent =
			 * angular_scale * (classic_dim * 1.5) / 256 around the
			 * direction / 512. Visibility gates on the camera forward
			 * row's Z sign (side 4 faces one world pole, 5 the other). */
			const float fwd_z = camera_independent ? b->world_dir[2] : crows[8];
			if ((b->side == 4 && !(fwd_z > 0.0f)) || (b->side == 5 && !(fwd_z < 0.0f))) {
				continue;
			}
			if (count >= capacity || !fl_resolve_type_frame(assets, b->object_type, frame, &ref) ||
				ref.classic_w <= 0 || ref.classic_h <= 0 ||
				fl_backdrop_hidden(assets, hide, b->object_type, frame)) {
				continue;
			}
			const int ext_w = ((int)b->angular_scale * (ref.classic_w + (ref.classic_w >> 1))) >> 8;
			const int ext_h = ((int)b->angular_scale * (ref.classic_h + (ref.classic_h >> 1))) >> 8;
			float v[3];
			fl_world_to_view(crows, b->world_dir[0], b->world_dir[1], b->world_dir[2], v);
			for (int a = 0; a < 3; a++) {
				v[a] = (float)((int)v[a] >> 9); /* classic >>9 on the int view dir */
			}
			FlBackdropQuad* q = &out[count++];
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
			q->sun = fl_backdrop_is_sun(b) ? (int)i : -1;
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
	AeronWorld_LocalI32(s.origin_world, cam->world_pos, camera_local);
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
					   const float (*prev_corners)[3], float emissive_strength) {
	if (s.bb_count == s.bb_capacity) {
		const uint32_t capacity = s.bb_capacity ? s.bb_capacity * 2 : 1024;
		FlBillboard* grown = realloc(s.bb, (size_t)capacity * sizeof *grown);
		if (!grown) {
			return;
		}
		s.bb = grown;
		s.bb_capacity = capacity;
	}
	FlBillboard* b = &s.bb[s.bb_count++];
	memcpy(b->center, g->center, sizeof b->center);
	b->half_w = g->half_w;
	b->half_h = g->half_h;
	b->rot = g->rot;
	b->depth_bias = g->depth_bias;
	b->emissive_strength = emissive_strength;
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
	if (!fl_object_in_render_region(f, cam->region))
		return 0;
	if (f->slot_class == XWA_SNAP_SLOT_TRANSIENT)
		return f->genus != XWA_SNAP_GENUS_DEBRIS || !cam->external;
	return f->slot_class == XWA_SNAP_SLOT_MAIN &&
		   (f->genus == XWA_SNAP_GENUS_EXPLOSION || f->genus == XWA_SNAP_GENUS_DEBRIS);
}

static float fl_object_billboard_emissive_strength(const XwaFlightObject* f) {
	/* The map has no bloom pass, so retain its established LDR appearance. */
	if (!s.map_present && f->genus == XWA_SNAP_GENUS_EXPLOSION)
		return s.explosion_genus_emissive_strength;
	return 1.0f;
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
	AeronWorld_DeltaI32(f->world_pos, cam->world_pos, delta_world);
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
	if (!(cam->death_star_mode && type == XWA_SNAP_TYPE_EXPLOSION_2006) && proj > 1024.0f)
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
	fl_bb_push(&geometry, &ref, 0xffffffffu, previous_corners_ptr, fl_object_billboard_emissive_strength(f));
}

static int fl_wreck_flame_base_geometry(const XwaFlightObject* f, const XwaFlightCamera* cam,
										const float world_to_view[9], FlBillboardGeometry* out,
										uint8_t mesh_types[XWA_SNAP_MAX_MESH_SLOTS], int* mesh_count) {
	if (!f->has_craft || f->anim_frame == 0)
		return 0;
	*mesh_count = XwaSnapshotExport_ModelMeshTypes(f->object_type, mesh_types);
	if (*mesh_count == 0)
		return 0;
	int w = 0, h = 0, maxb = 0;
	if (!XwaSnapshotExport_ModelTexFrame(XWA_SNAP_TYPE_FLAME_2008, f->anim_frame, &w, &h, &maxb) ||
		maxb <= 0) {
		return 0;
	}
	float delta_world[3];
	AeronWorld_DeltaI32(f->world_pos, cam->world_pos, delta_world);
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
 * frame from the anim_frame byte (componentState[49] originally), base size 256. */
static void fl_derive_wreck_flames(const XwaFlightObject* f, uint32_t snap_index, XwaRemasterAssets* assets,
								   const XwaFlightCamera* history_cam, const float history_world_to_view[9]) {
	const XwaFlightCamera* cam = &s.snap->flight_camera;
	uint8_t mesh_types[XWA_SNAP_MAX_MESH_SLOTS];
	int mesh_count = 0;
	FlBillboardGeometry base_geometry;
	if (!fl_wreck_flame_base_geometry(f, cam, s.crows, &base_geometry, mesh_types, &mesh_count))
		return;
	XwaAssetRef ref;
	if (!fl_resolve_type_frame(assets, XWA_SNAP_TYPE_FLAME_2008, f->anim_frame, &ref))
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
			if (previous_mesh_types[mesh] == XWA_SNAP_MESH_FUSELAGE &&
				XwaFlightObject_ComponentState(pf, mesh) == 0) {
				rot += roll_rad;
				previous_rot[mesh] = rot;
				previous_flame[mesh] = 1;
			}
		}
	}

	const float roll_rad = (float)(int16_t)f->roll * FL_Q16_TO_RAD;
	float rot = base_geometry.rot;
	for (int mesh = 0; mesh < mesh_count; mesh++) {
		if (mesh_types[mesh] != XWA_SNAP_MESH_FUSELAGE || XwaFlightObject_ComponentState(f, mesh) != 0) {
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
		fl_bb_push(&geometry, &ref, 0xffffffffu, previous_corners_ptr, 1.0f);
	}
}

static int fl_bb_depth_cmp(const void* pa, const void* pb) {
	const float za = ((const FlBillboard*)pa)->center[2];
	const float zb = ((const FlBillboard*)pb)->center[2];
	return za < zb ? 1 : (za > zb ? -1 : 0);
}

/* Derives the billboards of entry `i`: a transient object, or an explosion,
 * debris or salvage of the main slots. */
static void fl_derive_billboard_object(const XwaSnapshot* snap, uint32_t i, XwaRemasterAssets* assets,
									   const XwaFlightCamera* history_cam,
									   const float history_world_to_view[9]) {
	const XwaFlightObject* f = &snap->flight_objects[i];
	const XwaFlightCamera* cam = &snap->flight_camera;
	/* Slot topology is authoritative: several dynamic effect spawners
	 * leave ObjectRecord.regionIdx stale. */
	if (!fl_object_in_render_region(f, cam->region)) {
		return;
	}
	if (f->slot_class == XWA_SNAP_SLOT_TRANSIENT) {
		if (f->genus == XWA_SNAP_GENUS_DEBRIS && cam->external) {
			return; /* classic hides transient debris externally */
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

static void fl_derive_billboards(const XwaSnapshot* snap, XwaRemasterAssets* assets,
								 const XwaFlightCamera* history_cam, const float history_world_to_view[9]) {
	static const XwaFlightObjectList lists[] = { XWA_SNAP_LIST_EXPLOSION, XWA_SNAP_LIST_DEBRIS,
												 XWA_SNAP_LIST_TRANSIENT };
	s.bb_count = 0;
	for (uint32_t li = 0; li < sizeof lists / sizeof lists[0]; li++) {
		for (uint32_t k = 0; k < snap->flight_list_count[lists[li]]; k++) {
			fl_derive_billboard_object(snap, snap->flight_lists[lists[li]][k], assets, history_cam,
									   history_world_to_view);
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

/* Authored environment lights that request a flare are direction sources at
 * infinity like backdrop suns. Their unit directions are scaled to the
 * backdrop Q20 magnitude so the anchor's depth reference also lands at the
 * far plane. */
static void fl_derive_authored_lens_sources(const AeronPreparedEnvironment* environment,
											const XwaRemasterFlightView* flight_view) {
	for (uint32_t i = 0; i < environment->light_count && s.lens_src_count < FL_MAX_LENS_SOURCES; i++) {
		const AeronEnvironmentLight* light = &environment->lights[i];
		if (!light->lens_flare) {
			continue;
		}
		float v[3];
		fl_world_to_view(s.crows, light->direction_world[0], light->direction_world[1],
						 light->direction_world[2], v);
		if (v[2] <= 0.0f) {
			continue;
		}
		const float far_scale = 1048576.0f;
		const float anchor[3] = { s.camera_local[0] + light->direction_world[0] * far_scale,
								  s.camera_local[1] + light->direction_world[1] * far_scale,
								  s.camera_local[2] + light->direction_world[2] * far_scale };
		fl_lens_add_source(flight_view, anchor, 0xffffffffu);
	}
}

static void fl_derive_lens_flares(const XwaSnapshot* snap, XwaRemasterAssets* assets,
								  const XwaRemasterFlightView* flight_view,
								  const AeronPreparedEnvironment* authored) {
	s.lens_src_count = 0;
	s.lens_ok = 0;
	const XwaFlightCamera* cam = &snap->flight_camera;

	/* The procedural flare (fl_submit_sun_flare) replaces the suns' trains. */
	const int sun_trains = !snap->suns.procedural_flare;
	if (authored && sun_trains) {
		fl_derive_authored_lens_sources(authored, flight_view);
	}
	/* Backdrop suns: direction sources at infinity (raw Q20 world
	 * dirs; projection is magnitude-independent, the anchor's large
	 * magnitude lands the depth reference at the far plane). An authored
	 * sky replaces them. */
	for (uint32_t i = 0;
		 sun_trains && !authored && i < snap->backdrop_count && s.lens_src_count < FL_MAX_LENS_SOURCES; i++) {
		const XwaBackdrop* b = &snap->backdrops[i];
		if (b->object_type < XWA_SNAP_TYPE_BACKDROP_SUN_FIRST ||
			b->object_type > XWA_SNAP_TYPE_BACKDROP_SUN_LAST) {
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

	/* Explosion flashes: the render walk's case-13 window (current-region
	 * MAIN slots only — transient sparks never queue flares). */
	for (uint32_t k = 0;
		 k < snap->flight_list_count[XWA_SNAP_LIST_EXPLOSION] && s.lens_src_count < FL_MAX_LENS_SOURCES;
		 k++) {
		const XwaFlightObject* f = &snap->flight_objects[snap->flight_lists[XWA_SNAP_LIST_EXPLOSION][k]];
		if (!fl_object_in_render_region(f, cam->region) || f->slot_class != XWA_SNAP_SLOT_MAIN ||
			f->genus != XWA_SNAP_GENUS_EXPLOSION) {
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
		AeronWorld_DeltaI32(f->world_pos, cam->world_pos, delta_world);
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
			(float)s.lens_dim[r][0] * k * (1.0f / 512.0f) * flight_view->projection_pixel_scale;
		const float half_h_px =
			(float)s.lens_dim[r][1] * k * (1.0f / 512.0f) * flight_view->projection_pixel_scale;
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
 * Classic SOURCE laws (positions, colors, classic intensities) from
 * FlightLight_AppendScenePointLightForObject, evaluated as physical
 * sphere lights (clustered, inverse-square, per-pixel Lambert + spec —
 * the sanctioned look-better deviation from the classic per-object
 * 8-light 1/d vertex law; XwaRemasterShip_ScenePointLight). Sources:
 *   - explosion-genus sprites / projectiles / DS fixtures via the
 *     XwaSnapshotExport_PointLightForObject accessor (the classic
 *     type switch + intensity tables), with the classic append gates;
 *   - capital engine glows via the HD mesh's engine glows (collected in
 *     the instance walk where mesh + articulation exist);
 *   - the cockpit lights: the HUD warning lights and the hyperspace
 *     lights, at the eye of the player's seat (fl_add_cockpit_lights).
 * The classic cull radii only selected lights per object and are not
 * used. The classic Local Lights config level is NOT honored (period
 * option). Aeron assigns the submitted sources to spatial light clusters.
 * They and the models' lights share one pool, ranked into the scene's
 * capacity (fl_submit_point_lights). */

/* Start the frame's light pool for `view`: its frustum planes (as the
 * scene's culling, without the far plane; dot(xyz, p) + w is the signed
 * distance, positive inside) and camera position. */
static void fl_begin_point_lights(const XwaRemasterFlightView* view) {
	s.pl_cand_count = 0;
	s.pl_cand_dropped = 0;
	s.pl_invalid_count = 0;
	s.pl_pool_count = 0;
	s.pl_model_count = 0;
	memset(s.pl_culled, 0, sizeof s.pl_culled);
	memset(s.pl_dropped, 0, sizeof s.pl_dropped);
	const float* m = view->view_proj;
	for (int k = 0; k < 4; k++) {
		const float w = m[12 + k];
		s.pl_frustum[0][k] = w + m[0 + k]; /* x >= -w */
		s.pl_frustum[1][k] = w - m[0 + k]; /* x <= w */
		s.pl_frustum[2][k] = w + m[4 + k]; /* y >= -w */
		s.pl_frustum[3][k] = w - m[4 + k]; /* y <= w */
		s.pl_frustum[4][k] = w - m[8 + k]; /* near (reverse Z): z <= w */
	}
	for (int p = 0; p < 5; p++) {
		const float length =
			sqrtf(s.pl_frustum[p][0] * s.pl_frustum[p][0] + s.pl_frustum[p][1] * s.pl_frustum[p][1] +
				  s.pl_frustum[p][2] * s.pl_frustum[p][2]);
		const float inverse = length > 0.0f ? 1.0f / length : 0.0f;
		for (int k = 0; k < 4; k++) {
			s.pl_frustum[p][k] *= inverse;
		}
	}
	memcpy(s.pl_eye, view->camera.pos, sizeof s.pl_eye);
}

/* Add a scene light to the pool unless what it lights misses the view: the
 * sphere of its range around it (plus a tube's half length), or a spot
 * light's cone (AeronScene_LightBounds), which can reach past the range
 * along its axis. Its score is its peak intensity over the squared
 * distance from the camera to its nearest point (at least its source
 * radius away); `keep` scores it INFINITY. A full pool replaces its
 * lowest score. */
static void fl_pool_add(const AeronSceneLight* light, int model, int keep) {
	const float half_length =
		sqrtf(light->half_axis[0] * light->half_axis[0] + light->half_axis[1] * light->half_axis[1] +
			  light->half_axis[2] * light->half_axis[2]);
	const float reach = light->radius + half_length;
	float bound[3], bound_radius;
	AeronScene_LightBounds(light, bound, &bound_radius);
	for (int p = 0; p < 5; p++) {
		const float* plane = s.pl_frustum[p];
		if (plane[0] * light->pos[0] + plane[1] * light->pos[1] + plane[2] * light->pos[2] + plane[3] <
				-reach ||
			plane[0] * bound[0] + plane[1] * bound[1] + plane[2] * bound[2] + plane[3] < -bound_radius) {
			s.pl_culled[model]++;
			return;
		}
	}
	float score = INFINITY;
	if (!keep) {
		const float d[3] = { light->pos[0] - s.pl_eye[0], light->pos[1] - s.pl_eye[1],
							 light->pos[2] - s.pl_eye[2] };
		const float source_radius = fabsf(light->source_radius);
		float distance = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) - half_length - source_radius;
		if (distance < source_radius) {
			distance = source_radius;
		}
		float peak = light->color[0] > light->color[1] ? light->color[0] : light->color[1];
		peak = light->color[2] > peak ? light->color[2] : peak;
		score = peak / (distance * distance);
	}
	FlPoolLight* slot;
	if (s.pl_pool_count < FL_MAX_PL_POOL) {
		slot = &s.pl_pool[s.pl_pool_count++];
	} else {
		slot = &s.pl_pool[0];
		for (uint32_t i = 1; i < FL_MAX_PL_POOL; i++) {
			if (s.pl_pool[i].score < slot->score) {
				slot = &s.pl_pool[i];
			}
		}
		if (!(score > slot->score)) {
			s.pl_dropped[model]++;
			return;
		}
		s.pl_dropped[slot->model]++;
	}
	slot->light = *light;
	slot->score = score;
	slot->model = model;
}

static XwaShipPointLight* fl_pl_push(const float pos[3], const float color_x_intensity[3]) {
	if (s.pl_cand_count >= FL_MAX_PL_CANDIDATES) {
		s.pl_cand_dropped++;
		return NULL;
	}
	XwaShipPointLight* l = &s.pl_cand[s.pl_cand_count++];
	memset(l, 0, sizeof *l);
	memcpy(l->pos, pos, 3 * sizeof(float));
	memcpy(l->color, color_x_intensity, 3 * sizeof(float));
	return l;
}

/* A projectile's light is its streak: the bolt model's long bound axis,
 * placed as the bolt is drawn (the superlaser with its beam scale). Its
 * origin is the bolt's tip, outside the streak (a fighter laser trails 5.5
 * to 22.5 m behind it), where the classic law's point sat. Returns zero
 * when the model is unavailable. */
static int fl_projectile_streak(const XwaSnapshot* snap, const XwaFlightObject* f,
								const int32_t camera_world[3], float center[3], float half_axis[3]) {
	const AeronSceneMesh* mesh = fl_mesh_for_name(fl_object_model_name(f));
	if (!mesh) {
		return 0;
	}
	float basis[9], local[3], model[16];
	fl_object_pose(f, camera_world, 1, basis, local, model);
	if (fl_is_death_star_beam(snap, f)) {
		fl_apply_death_star_beam_scale(snap, model);
	}
	int long_axis = 0;
	float long_length = 0.0f;
	for (int axis = 0; axis < 3; axis++) {
		const float scale =
			sqrtf(model[0 * 4 + axis] * model[0 * 4 + axis] + model[1 * 4 + axis] * model[1 * 4 + axis] +
				  model[2 * 4 + axis] * model[2 * 4 + axis]);
		const float length = (mesh->bound_max[axis] - mesh->bound_min[axis]) * scale;
		if (length > long_length) {
			long_axis = axis;
			long_length = length;
		}
	}
	float mid[3], half[3] = { 0.0f, 0.0f, 0.0f };
	for (int axis = 0; axis < 3; axis++) {
		mid[axis] = 0.5f * (mesh->bound_min[axis] + mesh->bound_max[axis]);
	}
	half[long_axis] = 0.5f * (mesh->bound_max[long_axis] - mesh->bound_min[long_axis]);
	for (int r = 0; r < 3; r++) {
		center[r] = model[r * 4 + 0] * mid[0] + model[r * 4 + 1] * mid[1] + model[r * 4 + 2] * mid[2] +
					model[r * 4 + 3];
		half_axis[r] = model[r * 4 + 0] * half[0] + model[r * 4 + 1] * half[1] + model[r * 4 + 2] * half[2];
	}
	return 1;
}

/* Accessor-law sources (everything except engine glows and cockpit
 * lights). */
/* Whether a bolt's model has lights of its own, which replace the stock
 * bolt light. */
static int fl_projectile_has_model_lights(const XwaFlightObject* f) {
	const AeronSceneMesh* mesh = fl_mesh_for_name(fl_object_model_name(f));
	return mesh && mesh->light_count > 0;
}

/* Whether the object in `slot` with `signature` is the craft that the local
 * player flies. hud.player_slot is g_players[g_localPlayer].objectIndex, and
 * it is 0xffff unless the object in that slot still has the player's
 * boundObjectSignature. When the craft is destroyed, objectIndex keeps its
 * slot, and that slot can then hold another object. The cockpit is drawn
 * for the camera focus, and when the cockpit is visible, the focus is
 * always this craft. */
static int fl_is_player_craft(const XwaSnapshot* snap, uint16_t slot, uint16_t signature) {
	return snap->hud.player_slot != 0xffffu && slot == snap->hud.player_slot &&
		   signature == snap->hud.player_signature;
}

/* The group of a particle effect's light (XwaParticleLightGroup): only
 * the cockpit hits create effects 4 and 2. */
static XwaParticleLightGroup fl_particle_light_group(const XwaSnapshot* snap,
													 const XwaParticleEffect* effect) {
	if (effect->effect_type == 4) {
		return XWA_PARTICLE_LIGHT_COCKPIT_SPARKS;
	}
	if (effect->effect_type == 2) {
		return XWA_PARTICLE_LIGHT_COCKPIT_SMOKE;
	}
	return effect->source_kind == XWA_PARTICLE_SOURCE_OBJECT &&
				   fl_is_player_craft(snap, effect->owner_slot, effect->owner_signature)
			   ? XWA_PARTICLE_LIGHT_PLAYER_CRAFT_DAMAGE
			   : XWA_PARTICLE_LIGHT_CRAFT_DAMAGE;
}

/* A group's light is a Blender point light: P watts give P * 683 / (4 pi)
 * candela, which the scene takes at 683 lm/W (AERON_SCENE_LUMENS_PER_WATT).
 * With fade, it dims linearly over the effect's life. */
int XwaRemasterFlight_ParticleLight(const XwaSnapshot* snapshot, const XwaParticleEffect* effect,
									const float pos[3], float exposure, AeronSceneLight* out) {
	if (!s.plight.model_lights) {
		return -1;
	}
	const XwaParticleLightGroup index = fl_particle_light_group(snapshot, effect);
	const XwaParticleLight* mod = &snapshot->particle_lights[index];
	const XwaFlightParticleLight* d = &s.plight.particle_lights[index];
	const float power_w = mod->has_power ? mod->power_w : d->power_w;
	float fade = 1.0f;
	if ((mod->has_fade ? mod->fade : d->fade) && effect->lifetime_ticks > 0) {
		fade = 1.0f - (float)effect->age_ticks / (float)effect->lifetime_ticks;
		fade = fade < 0.0f ? 0.0f : (fade > 1.0f ? 1.0f : fade);
	}
	if (!(power_w > 0.0f) || !(fade > 0.0f)) {
		return 0;
	}
	const float u = AERON_OPT_UNITS_PER_METER;
	float transform[16] = { 0 };
	for (int r = 0; r < 3; r++) {
		transform[r * 4 + r] = u;
		transform[r * 4 + 3] = pos[r];
	}
	transform[15] = 1.0f;
	AeronFlightLight light;
	memset(&light, 0, sizeof light);
	for (int c = 0; c < 3; c++) {
		light.color[c] = mod->has_color ? mod->color[c]
						 : d->has_color ? d->color[c]
										: XwaRemaster_SrgbToLinear(c < 2 ? 1.0f : 0.75f);
	}
	light.intensity = power_w * 683.0f / (4.0f * 3.14159265f) * fade;
	light.shape = AERON_FLIGHT_LIGHT_POINT;
	const float scale = 1.0f / AERON_SCENE_LUMENS_PER_WATT;
	return AeronScene_FlightLight(&light, transform, u, scale, exposure, out) ? 1 : 0;
}

/* The scale of the light of an explosion at `local`. While the cockpit is
 * drawn, an explosion up to point_lights.cockpit_explosion_distance_m from
 * the eye gets point_lights.cockpit_explosion_light_scale, and from there to
 * twice that distance the scale goes back up to 1. A hit on the player's
 * craft puts its explosion on the hull, a metre or two from the eye, where
 * the inverse-square light washes out the cockpit. */
static float fl_cockpit_explosion_light_scale(int cockpit_drawn, const float local[3]) {
	const float scale = s.plight.cockpit_explosion_light_scale;
	const float near_m = s.plight.cockpit_explosion_distance_m;
	if (!cockpit_drawn || !(scale < 1.0f) || !(near_m > 0.0f)) {
		return 1.0f;
	}
	const float d[3] = { local[0] - s.pl_eye[0], local[1] - s.pl_eye[1], local[2] - s.pl_eye[2] };
	const float distance_m = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) * AERON_OPT_METERS_PER_UNIT;
	if (distance_m <= near_m) {
		return scale;
	}
	if (distance_m >= 2.0f * near_m) {
		return 1.0f;
	}
	return scale + (1.0f - scale) * (distance_m - near_m) / near_m;
}

static void fl_derive_point_lights(const XwaSnapshot* snap) {
	if (!s.plight.enabled) {
		return;
	}
	const XwaFlightCamera* cam = &snap->flight_camera;
	const int cockpit_drawn = fl_cockpit_drawn(snap, cam);
	/* Superlaser beam light (FlightView_Render's direct scene-light write:
	 * green, intensity 100000, cull 0x4000 — appended FIRST like the
	 * classic). With the beam's object available it lights along the
	 * drawn beam instead, in the object walk below. */
	const float ds_beam_intensity = 100000.0f;
	const float ds_beam_color[3] = { 0.0f, XwaRemaster_SrgbToLinear(1.0f) * ds_beam_intensity,
									 XwaRemaster_SrgbToLinear(0.2f) * ds_beam_intensity };
	if (cam->ds_beam_active && !snap->death_star_beam.active) {
		float local[3];
		AeronWorld_LocalI32(s.origin_world, cam->ds_beam_world_pos, local);
		fl_pl_push(local, ds_beam_color);
	}
	for (uint32_t i = 0; i < snap->flight_object_count; i++) {
		const XwaFlightObject* f = &snap->flight_objects[i];
		/* Mirrors FlightView's active-main and local-effect slot walks. */
		if (!fl_object_in_render_region(f, cam->region) ||
			(f->slot_class != XWA_SNAP_SLOT_MAIN && f->slot_class != XWA_SNAP_SLOT_TRANSIENT)) {
			continue;
		}
		/* Classic append gate (genus/type list; the engine-glow branch
		 * for ordinary craft is collected in the instance walk). */
		const int gate =
			f->genus == XWA_SNAP_GENUS_EXPLOSION || f->genus == XWA_SNAP_GENUS_PLAYER_PROJECTILE ||
			f->genus == XWA_SNAP_GENUS_NPC_PROJECTILE || f->genus == XWA_SNAP_GENUS_DS_TUNNEL ||
			f->object_type == XWA_SNAP_TYPE_DS_REACTOR ||
			(f->object_type == XWA_SNAP_TYPE_FALCON2 && cam->death_star_mode && snap->dir_light_count == 0);
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
		float half_axis[3];
		if (fl_is_death_star_beam(snap, f)) {
			/* The beam's light (the classic's point near the target, and
			 * the beam object's own) becomes one line source along the
			 * drawn beam: its diffuse falls as 1/d out to kilometres, as
			 * the classic law did, and fades as the beam shortens. */
			if (fl_projectile_streak(snap, f, cam->world_pos, local, half_axis)) {
				XwaShipPointLight* light = fl_pl_push(local, ds_beam_color);
				if (light) {
					memcpy(light->half_axis, half_axis, sizeof half_axis);
					light->match_distance_m =
						sqrtf(half_axis[0] * half_axis[0] + half_axis[1] * half_axis[1] +
							  half_axis[2] * half_axis[2]) *
						AERON_OPT_METERS_PER_UNIT;
				}
			}
		} else if (fl_object_is_projectile(f) && fl_projectile_has_model_lights(f)) {
			/* The bolt's model lights it (fl_add_model_lights). */
		} else if (fl_object_is_projectile(f) &&
				   fl_projectile_streak(snap, f, cam->world_pos, local, half_axis)) {
			XwaShipPointLight* light = fl_pl_push(local, cxi);
			if (light) {
				memcpy(light->half_axis, half_axis, sizeof half_axis);
			}
		} else {
			fl_object_local(f, local);
			const float scale = f->genus == XWA_SNAP_GENUS_EXPLOSION
									? fl_cockpit_explosion_light_scale(cockpit_drawn, local)
									: 1.0f;
			if (scale > 0.0f) {
				for (int c = 0; c < 3; c++) {
					cxi[c] *= scale;
				}
				fl_pl_push(local, cxi);
			}
		}
	}
	/* The lights of the particle effects on objects
	 * (Particle_AppendObjectEffectPointLights). The pool always keeps the
	 * cockpit sparks and smoke. */
	const float exposure = XwaRemasterFlight_Exposure();
	for (uint32_t i = 0; i < snap->particle_effect_count; i++) {
		const XwaParticleEffect* effect = &snap->particle_effects[i];
		if (!effect->point_light || effect->source_kind != XWA_PARTICLE_SOURCE_OBJECT ||
			effect->render_region != cam->region) {
			continue;
		}
		float local[3];
		AeronWorld_LocalPointI32F32(s.origin_world, effect->emitter_world_pos.base,
									effect->emitter_world_pos.offset, local);
		AeronSceneLight light;
		const int mod_light = XwaRemasterFlight_ParticleLight(snap, effect, local, exposure, &light);
		if (mod_light > 0) {
			const XwaParticleLightGroup group = fl_particle_light_group(snap, effect);
			fl_pool_add(&light, 0,
						group == XWA_PARTICLE_LIGHT_COCKPIT_SPARKS ||
							group == XWA_PARTICLE_LIGHT_COCKPIT_SMOKE);
		} else if (mod_light < 0) {
			const float intensity = 125.0f;
			const float cxi[3] = { intensity, intensity, XwaRemaster_SrgbToLinear(0.75f) * intensity };
			fl_pl_push(local, cxi);
		}
	}
}

/* Metres from the eye of the player's seat, along the craft's right, up
 * and forward axes, to world units. player_bw's rows are right,
 * ship-backward and up. */
static void fl_seat_eye_transform(const XwaSnapshot* snap, const float player_bw[9],
								  const float player_pos[3], float transform[16]) {
	const XwaCockpit* k = &snap->cockpit;
	const float u = AERON_OPT_UNITS_PER_METER;
	memset(transform, 0, 16 * sizeof(float));
	for (int r = 0; r < 3; r++) {
		transform[r * 4 + 0] = player_bw[0 * 3 + r] * u;
		transform[r * 4 + 1] = player_bw[2 * 3 + r] * u;
		transform[r * 4 + 2] = -player_bw[1 * 3 + r] * u;
		transform[r * 4 + 3] = player_pos[r] + player_bw[0 * 3 + r] * k->seat_eye_local[0] +
							   player_bw[1 * 3 + r] * k->seat_eye_local[1] +
							   player_bw[2 * 3 + r] * k->seat_eye_local[2];
	}
	transform[15] = 1.0f;
}

/* Adds the cockpit lights. Each one blinks with the classic cycle and fade
 * (FlightLight_AppendCockpitLights), replayed from the simulation clock. It
 * sits at the eye of the player's seat, and the player's basis carries it
 * to the world.
 *
 * When model lights are on, each light is a Blender point light of P watts,
 * which gives P * 683 / (4 pi) candela. Its values come from
 * point_lights.cockpit_lights, and each key that the mod sets
 * (XwaCockpitLight.mod_*) replaces the default. The pool always keeps these
 * lights. When model lights are off, each light is the game's light. */
static void fl_add_cockpit_lights(const XwaSnapshot* snap, const float player_bw[9],
								  const float player_pos[3]) {
	if (!s.plight.enabled || !snap->cockpit_lights_active || !player_bw) {
		return;
	}
	float transform[16];
	fl_seat_eye_transform(snap, player_bw, player_pos, transform);
	const float world[3] = { transform[3], transform[7], transform[11] };
	for (int p = 0; p < XWA_SNAP_COCKPIT_LIGHT_COUNT; p++) {
		const XwaCockpitLight* pl = &snap->cockpit_lights[p];
		if (!pl->enabled) {
			continue;
		}
		const double elapsed = (double)(snap->game_time_ticks - pl->start_time);
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
		if (s.plight.model_lights) {
			const XwaFlightCockpitLight* d = &s.plight.cockpit_lights[p];
			const float power_w = pl->mod_has_power ? pl->mod_power_w : d->power_w;
			const float* position_m = pl->mod_has_position ? pl->mod_position_m : d->position_m;
			if (!(power_w > 0.0f)) {
				continue;
			}
			AeronFlightLight light;
			memset(&light, 0, sizeof light);
			light.position.x = position_m[0];
			light.position.y = position_m[1];
			light.position.z = position_m[2];
			for (int c = 0; c < 3; c++) {
				light.color[c] = pl->mod_has_color ? pl->mod_color[c]
								 : d->has_color    ? d->color[c]
												   : XwaRemaster_SrgbToLinear(pl->color[c]);
			}
			light.intensity = power_w * 683.0f / (4.0f * 3.14159265f) * fade;
			light.shape = AERON_FLIGHT_LIGHT_POINT;
			AeronSceneLight scene_light;
			if (AeronScene_FlightLight(&light, transform, AERON_OPT_UNITS_PER_METER,
									   1.0f / AERON_SCENE_LUMENS_PER_WATT, XwaRemasterFlight_Exposure(),
									   &scene_light)) {
				fl_pool_add(&scene_light, 0, 1);
			}
			continue;
		}
		float cxi[3];
		for (int c = 0; c < 3; c++) {
			cxi[c] = XwaRemaster_SrgbToLinear(pl->color[c]) * pl->intensity * fade;
		}
		fl_pl_push(world, cxi);
	}
}

/* Adds the player craft's headlight (XwaHeadlight). It works like a light
 * of a GLB model made in Blender: a one-sided light of P watts gives
 * P * 683 / pi candela straight ahead, and the scene takes 683 lm/W
 * (AERON_SCENE_LUMENS_PER_WATT). With an angle, it is a spot light with the
 * same intensity straight ahead, which fades out towards the angle. Each
 * value that the mod sets (XwaHeadlight.mod_*) replaces the default from
 * point_lights.headlights. The light starts at the seat's eye in every
 * view, as the cockpit lights do, and points along the nose. The light pool
 * never drops it. The hyperspace tunnel doesn't add it. */
static void fl_add_headlight(const XwaSnapshot* snap, const float player_bw[9], const float player_pos[3]) {
	const XwaHeadlight* h = &snap->headlight;
	const XwaFlightHeadlight* d = &s.plight.headlight;
	if (!s.plight.enabled || !s.plight.model_lights || !h->on || !player_bw) {
		return;
	}
	const float power_w = h->mod_has_power ? h->mod_power_w : d->power_w;
	const float* position_m = h->mod_has_position ? h->mod_position_m : d->position_m;
	const float angle_degrees = h->mod_has_angle ? h->mod_angle_degrees : d->angle_degrees;
	if (!(power_w > 0.0f)) {
		return;
	}
	float transform[16];
	fl_seat_eye_transform(snap, player_bw, player_pos, transform);
	AeronFlightLight light;
	memset(&light, 0, sizeof light);
	light.position.x = position_m[0];
	light.position.y = position_m[1];
	light.position.z = position_m[2];
	memcpy(light.color, h->mod_has_color ? h->mod_color : d->color, sizeof light.color);
	light.intensity = power_w * 683.0f / 3.14159265f;
	light.shape = AERON_FLIGHT_LIGHT_POINT;
	light.direction.z = 1.0f;
	light.spot_outer_angle = angle_degrees * (3.14159265f / 180.0f);
	AeronSceneLight scene_light;
	if (AeronScene_FlightLight(&light, transform, AERON_OPT_UNITS_PER_METER,
							   1.0f / AERON_SCENE_LUMENS_PER_WATT, XwaRemasterFlight_Exposure(),
							   &scene_light)) {
		fl_pool_add(&scene_light, 0, 1);
	}
}

/* Convert the candidates to scene lights with the calibration into the
 * pool, discarding invalid records. */
static void fl_finalize_point_lights(void) {
	if (!s.plight.enabled) {
		return;
	}
	const float exposure = XwaRemasterFlight_Exposure();
	for (uint32_t i = 0; i < s.pl_cand_count; i++) {
		AeronSceneLight light;
		if (!XwaRemasterShip_ScenePointLight(&s.pl_cand[i], s.plight.match_distance_m, s.plight.range_scale,
											 exposure, &light)) {
			s.pl_invalid_count++;
			continue;
		}
		fl_pool_add(&light, 0, 0);
	}
}

/* Places a probe set on each drawn object whose model mesh has probes,
 * closest to the camera first, up to the scene's limit. The instance of
 * each owner object uses its own set. Other instances use the first set
 * whose bounds they overlap, so the closest set wins where sets meet. */
static void fl_place_probe_sets(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
								const XwaRemasterFlightView* view) {
	const XwaFlightCamera* cam = &snap->flight_camera;
	AeronProbeSet* sets[AERON_SCENE_MAX_PROBE_SETS];
	float distances[AERON_SCENE_MAX_PROBE_SETS];
	uint32_t count = 0;
	s.probe_set_count = 0;
	/* The explosions and projectiles have no probes. */
	static const XwaFlightObjectList lists[] = { XWA_SNAP_LIST_CRAFT, XWA_SNAP_LIST_DEBRIS,
												 XWA_SNAP_LIST_TRANSIENT };
	for (uint32_t li = 0; s_probes_enabled && li < sizeof lists / sizeof lists[0]; li++) {
		for (uint32_t li_k = 0; li_k < snap->flight_list_count[lists[li]]; li_k++) {
			const uint32_t i = snap->flight_lists[lists[li]][li_k];
			const XwaFlightObject* f = &snap->flight_objects[i];
			if (!fl_object_in_render_region(f, cam->region) || f->genus == XWA_SNAP_GENUS_EXPLOSION ||
				fl_object_is_projectile(f) || f->object_type == XWA_SNAP_TYPE_DEBRIS_CHUNK) {
				continue;
			}
			const AeronSceneMesh* mesh =
				XwaRemasterShip_MeshForName(XwaSnapshotExport_ModelName(f->object_type));
			AeronProbeSet* probes = mesh ? mesh->probes : NULL;
			if (!probes) {
				continue;
			}
			float delta[3];
			AeronWorld_DeltaI32(cam->world_pos, f->world_pos, delta);
			const float distance = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
			/* Insert by distance; the farthest drops past the limit. */
			uint32_t k = count < AERON_SCENE_MAX_PROBE_SETS ? count++ : AERON_SCENE_MAX_PROBE_SETS;
			for (; k > 0 && distances[k - 1] > distance; k--) {
				if (k < AERON_SCENE_MAX_PROBE_SETS) {
					distances[k] = distances[k - 1];
					sets[k] = sets[k - 1];
					s.probe_set_object[k] = s.probe_set_object[k - 1];
				}
			}
			if (k < AERON_SCENE_MAX_PROBE_SETS) {
				distances[k] = distance;
				sets[k] = probes;
				s.probe_set_object[k] = i;
			}
		}
	}
	for (uint32_t k = 0; k < count; k++) {
		const XwaFlightObject* f = &snap->flight_objects[s.probe_set_object[k]];
		XwaProbeOutlineSet* set = &s.probe_sets[s.probe_set_count];
		float basis[9], local[3];
		/* The pose the object walk gives its instance. */
		fl_object_pose(f, cam->world_pos, 0, basis, local, set->model);
		if (AeronScene_AddProbeSet(s.scene, sets[k], set->model) < 0) {
			continue;
		}
		set->probes = sets[k];
		XwaFlightProbeSetInfo* info = &s.probe_set_info[s.probe_set_count];
		const char* model = XwaSnapshotExport_ModelName(f->object_type);
		snprintf(info->model, sizeof info->model, "%s", model ? model : "");
		info->slot = f->slot;
		info->probe_count = sets[k]->layout.probe_count;
		memcpy(info->grid_resolution, sets[k]->layout.grid_resolution, sizeof info->grid_resolution);
		info->bytes = sets[k]->bytes;
		s.probe_set_object[s.probe_set_count++] = s.probe_set_object[k];
	}
	if (s.probe_set_count && s_show_probes) {
		XwaRemasterProbeOutlines_Prepare(cmd, s.scene, view, s.crows, s.probe_sets, s.probe_set_count);
	}
}

/* Returns the probe set of the hangar model (the launch reference object)
 * if this frame placed it, else NULL. */
static const AeronProbeSet* fl_hangar_probe_set(const XwaSnapshot* snap) {
	for (uint32_t k = 0; k < s.probe_set_count; k++) {
		if ((int32_t)snap->flight_objects[s.probe_set_object[k]].slot ==
			snap->flight_camera.hangar_launch_ref_obj_idx) {
			return s.probe_sets[k].probes;
		}
	}
	return NULL;
}

/* The probe set owned by the object at walk index `i`, as index + 1; 0 = none. */
static uint8_t fl_probe_set_owner(uint32_t i) {
	for (uint32_t k = 0; k < s.probe_set_count; k++) {
		if (s.probe_set_object[k] == i) {
			return (uint8_t)(k + 1);
		}
	}
	return 0;
}

/* Lights authored in a model (AeronFlightLight), placed by its instance,
 * into the pool; `keep` always keeps them (the cockpit's). Their intensity
 * is authored in candela, so point_lights.match_distance_m does not apply;
 * the scene takes it at 683 lm/W (AERON_SCENE_LUMENS_PER_WATT).
 * `skip_baked` leaves out the lights baked into the model's probes, while
 * its probe set is placed. `variant` is the instance's variant index: a
 * light that names variants shines only with them. Lights in the instance's
 * `hidden_tag_groups` are left out. With `one_light`, only the first light
 * that shines is added. Projectiles use it: a battle can have hundreds of
 * them, so each one gets a single light. */
static void fl_add_model_lights(const AeronSceneMesh* mesh, const float transform[16],
								const AeronSceneMeshTable* table, uint32_t variant,
								uint32_t hidden_tag_groups, int keep, int skip_baked, int one_light) {
	if (!s.plight.enabled || !s.plight.model_lights) {
		return;
	}
	const float exposure = XwaRemasterFlight_Exposure();
	for (uint32_t i = 0; i < mesh->light_count; ++i) {
		AeronSceneLight light;
		if (skip_baked && mesh->lights[i].baked) {
			continue;
		}
		if (AeronScene_MeshLight(mesh, i, transform, AERON_OPT_UNITS_PER_METER, table, variant,
								 hidden_tag_groups, s.plight.model_light_scale / AERON_SCENE_LUMENS_PER_WATT,
								 exposure, &light)) {
			s.pl_model_count++;
			fl_pool_add(&light, 1, keep);
			if (one_light) {
				return;
			}
		}
	}
}

/* A pool light's score and index. These keys are sorted instead of the pool
 * entries, which are large. */
typedef struct FlPoolKey {
	float score;
	uint32_t index;
} FlPoolKey;

/* Whether `a` ranks before `b`: the higher score first, then the earlier pool
 * entry, so that no two keys are equal. */
static int fl_pool_key_before(const FlPoolKey* a, const FlPoolKey* b) {
	if (a->score != b->score) {
		return a->score > b->score;
	}
	return a->index < b->index;
}

/* Sorts `count` keys with fl_pool_key_before, using `scratch`, which holds as
 * many keys: insertion sort of short runs, then merges. */
static void fl_sort_pool_keys(FlPoolKey* keys, FlPoolKey* scratch, uint32_t count) {
	enum { RUN = 16 };

	for (uint32_t start = 0; start < count; start += RUN) {
		const uint32_t end = start + RUN < count ? start + RUN : count;
		for (uint32_t i = start + 1; i < end; i++) {
			const FlPoolKey key = keys[i];
			uint32_t j = i;
			while (j > start && fl_pool_key_before(&key, &keys[j - 1])) {
				keys[j] = keys[j - 1];
				j--;
			}
			keys[j] = key;
		}
	}
	FlPoolKey* from = keys;
	FlPoolKey* to = scratch;
	for (uint32_t width = RUN; width < count; width *= 2) {
		for (uint32_t start = 0; start < count; start += 2 * width) {
			const uint32_t middle = start + width < count ? start + width : count;
			const uint32_t end = start + 2 * width < count ? start + 2 * width : count;
			uint32_t a = start;
			uint32_t b = middle;
			uint32_t out = start;
			while (a < middle && b < end) {
				to[out++] = fl_pool_key_before(&from[b], &from[a]) ? from[b++] : from[a++];
			}
			while (a < middle) {
				to[out++] = from[a++];
			}
			while (b < end) {
				to[out++] = from[b++];
			}
		}
		FlPoolKey* swap = from;
		from = to;
		to = swap;
	}
	if (from != keys) {
		memcpy(keys, from, (size_t)count * sizeof *keys);
	}
}

/* Submit the pool's best lights within the scene's capacity. When some
 * are dropped, a kept light under FL_PL_FADE_BAND times the best dropped
 * one's score fades toward zero at it: scores change continuously as the
 * camera moves, so a light crossing the cut is already dark. */
static void fl_submit_point_lights(void) {
	if (s.pl_pool_count <= AERON_SCENE_POINT_LIGHT_CAPACITY) {
		for (uint32_t i = 0; i < s.pl_pool_count; ++i) {
			AeronScene_AddLight(s.scene, &s.pl_pool[i].light);
		}
		return;
	}
	static FlPoolKey keys[FL_MAX_PL_POOL];
	static FlPoolKey scratch[FL_MAX_PL_POOL];
	for (uint32_t i = 0; i < s.pl_pool_count; i++) {
		keys[i].score = s.pl_pool[i].score;
		keys[i].index = i;
	}
	fl_sort_pool_keys(keys, scratch, s.pl_pool_count);
	const uint32_t count = AERON_SCENE_POINT_LIGHT_CAPACITY;
	for (uint32_t i = count; i < s.pl_pool_count; i++) {
		s.pl_dropped[s.pl_pool[keys[i].index].model]++;
	}
	const float cut = s.pl_pool[keys[count].index].score;
	for (uint32_t i = 0; i < count; i++) {
		FlPoolLight* l = &s.pl_pool[keys[i].index];
		if (isfinite(cut) && l->score < cut * FL_PL_FADE_BAND) {
			const float fade = (l->score / cut - 1.0f) / (FL_PL_FADE_BAND - 1.0f);
			for (int c = 0; c < 3; c++) {
				l->light.color[c] *= fade;
			}
		}
		AeronScene_AddLight(s.scene, &l->light);
	}
}

static int fl_configure_clustered_lights(AeronScene3D* scene) {
	return AeronScene_SetClusteredLights(scene, &(AeronSceneClusteredLightDesc) {
													.depth_slices = (uint32_t)s.plight.cluster_depth_slices,
													.debug_view = s.plight.cluster_debug,
												});
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
							   NULL, b->depth_bias, b->has_prev ? b->prev_corners : NULL,
							   b->emissive_strength);
	}
}

typedef struct FlMapSubmitContext {
	AeronCommandBuffer* cmd;
	XwaRemasterAssets* assets;
} FlMapSubmitContext;

typedef struct FlPreparedMesh {
	AeronSceneMesh* mesh;
	AeronSceneMeshTable* table;
	const AeronSceneMaterialAnim* anim;
} FlPreparedMesh;

/* The hull-damage stage for a hull damage and maximum: 1 at or below 75 %
 * hull, 2 at or below 50 %, 3 at or below 25 %, else 0. */
static uint32_t fl_hull_stage(int32_t damage, int32_t max) {
	if (max <= 0) {
		return 0;
	}
	const double hull = 100.0 * (1.0 - (double)damage / (double)max);
	return hull <= 25.0 ? 3u : hull <= 50.0 ? 2u : hull <= 75.0 ? 1u : 0u;
}

/* The hull-damage stage of an object. */
static uint32_t fl_damage_stage(const XwaFlightObject* object) {
	return object->has_craft ? fl_hull_stage(object->hull_damage, object->hull_max) : 0u;
}

/* The energy effect an object shows:
 * 3 under a jamming beam, else 2 under a tractor beam, else 1 when its
 * shields are down (or it has none), else 0. */
static uint32_t fl_energy(const XwaFlightObject* object) {
	if (!object->has_craft) {
		return 0;
	}
	if (object->beam_jamming) {
		return 3;
	}
	if (object->beam_tractor) {
		return 2;
	}
	return object->shield_front + object->shield_rear == 0 ? 1u : 0u;
}

/* When the object's current energy effect started, in animation seconds,
 * from the states of fl_prepare_energy_states. An object without a state
 * gets a start long before now. */
static float fl_energy_start(const XwaFlightObject* object) {
	const float now = s.snap ? (float)((double)s.snap->game_time_ticks * XWA_SNAP_GAME_TICK_SECONDS) : 0.0f;
	if (object->slot < s.energy_state_count) {
		const FlEnergyState* state = &s.energy_states[object->slot];
		if (state->valid && state->signature == object->signature) {
			return state->start;
		}
	}
	return now - FL_ENERGY_SETTLED_SECONDS;
}

/* The cannon-state bits (cannon.1 .. cannon.8) of hardpoints first to last. */
static uint32_t fl_cannon_range(int first, int last) {
	uint32_t bits = 0;
	for (int i = first < 0 ? 0 : first; i <= last && i < 8; i++) {
		bits |= 1u << i;
	}
	return bits;
}

/* The cannons that fire next, bit i for hardpoint i. Per cannon set, link
 * mode 1 marks the next hardpoint, 2 every second one from it to the end of
 * the set, 3 the set and 4 every hardpoint; set 0 is the lasers and set 1
 * the ions, counted by weapon type. None is ready with warheads armed;
 * otherwise the set not selected is not, unless a mode is 4; nor is a
 * cannon without energy. */
static uint32_t fl_cannons_ready(const XwaHudState* hud) {
	const XwaHudInstruments* in = &hud->instruments;
	const int count =
		in->laser_slot_count < XWA_SNAP_MAX_WEAPON_SLOTS ? in->laser_slot_count : XWA_SNAP_MAX_WEAPON_SLOTS;
	int lasers = 0;
	int ions = 0;
	for (int i = 0; i < count; i++) {
		lasers += in->weapon_type[i] == 1;
		ions += in->weapon_type[i] == 2;
	}
	uint32_t ready = 0;
	int all = 0;
	for (int j = 0; j < in->cannon_count && j < 3; j++) {
		const int first = j == 0 ? 0 : j == 1 ? lasers : lasers + ions;
		const int last = j == 0 ? lasers - 1 : j == 1 ? lasers + ions - 1 : count - 1;
		switch (in->laser_link_mode[j]) {
			case 1:
				ready |= fl_cannon_range(in->laser_link_next_slot[j], in->laser_link_next_slot[j]);
				break;
			case 2:
				for (int i = in->laser_link_next_slot[j]; i <= last; i += 2) {
					ready |= fl_cannon_range(i, i);
				}
				break;
			case 3:
				ready |= fl_cannon_range(first, last);
				break;
			case 4:
				all = 1;
				ready |= fl_cannon_range(0, count - 1);
				break;
			default:
				break;
		}
	}
	if (hud->reticle.weapon_mode) {
		ready &= ~fl_cannon_range(0, count - 1);
	} else if (!all) {
		ready &= ~(hud->reticle.selected_warhead == 0 ? fl_cannon_range(lasers, lasers + ions - 1)
													  : fl_cannon_range(0, lasers - 1));
	}
	for (int i = 0; i < count; i++) {
		if (in->laser_charge[i] == 0) {
			ready &= ~fl_cannon_range(i, i);
		}
	}
	return ready;
}

/* The broken-instrument states from the craft's instrument masks, at the
 * start and now: CMD 0x1, lasers/ions 0xE (all three), beam 0x10, shields
 * 0x20, throttle 0x40, sensors 0x180 (both), laser recharge 0x200, engine
 * 0x400, shield recharge 0x800, beam recharge 0x1000. The beam, shields and
 * their recharges count only on a craft that started with them. */
static uint32_t fl_broken_states(uint16_t initial, uint16_t current) {
	uint32_t m = 0;
	m |= (current & 0x1) == 0 ? 1u << AERON_ANIM_STATE_BROKEN_CMD : 0u;
	m |= (current & 0xE) != 0xE ? 1u << AERON_ANIM_STATE_BROKEN_LASER_ION : 0u;
	m |= (initial & ~current & 0x10) != 0 ? 1u << AERON_ANIM_STATE_BROKEN_BEAM_WEAPON : 0u;
	m |= (initial & ~current & 0x20) != 0 ? 1u << AERON_ANIM_STATE_BROKEN_SHIELDS : 0u;
	m |= (current & 0x40) == 0 ? 1u << AERON_ANIM_STATE_BROKEN_THROTTLE : 0u;
	m |= (current & 0x180) != 0x180 ? 1u << AERON_ANIM_STATE_BROKEN_SENSORS : 0u;
	m |= (current & 0x200) == 0 ? 1u << AERON_ANIM_STATE_BROKEN_LASER_RECHARGE : 0u;
	m |= (current & 0x400) == 0 ? 1u << AERON_ANIM_STATE_BROKEN_ENGINE_POWER : 0u;
	m |= (initial & ~current & 0x800) != 0 ? 1u << AERON_ANIM_STATE_BROKEN_SHIELD_RECHARGE : 0u;
	m |= (initial & ~current & 0x1000) != 0 ? 1u << AERON_ANIM_STATE_BROKEN_BEAM_RECHARGE : 0u;
	return m;
}

/* Sets the states to `mask`, moving the onsets of the `fired` ones to now. */
static void fl_state_onsets_update(FlStateOnsets* states, uint32_t mask, uint32_t fired, float now) {
	for (int i = 0; i < 32; i++) {
		if (fired >> i & 1u) {
			states->onset[i] = now;
		}
	}
	states->mask = mask;
}

/* This frame's player states, on the player's craft and cockpit: the
 * threat indicators as the classic HUD last drew them (cleared on hangar
 * entry), the cannons ready, the broken instruments, the hull stage, the
 * target event (warhead lock over laser lock over a selected target, with
 * `target` set for any of them) and the hangar. A state fires when it
 * becomes set; `target` when the target event becomes "selected". The debug
 * override's states fire the same way, and its energy effect starts when it
 * changes. All of it starts over when the clock goes back. Repeating the
 * update for a snapshot changes nothing. */
static void fl_update_material_states(const XwaSnapshot* snap, float now) {
	if (now < s.states_time) {
		memset(&s.player_states, 0, sizeof s.player_states);
		memset(&s.override_states, 0, sizeof s.override_states);
		s.override_energy = 0;
		memset(&s.threats, 0, sizeof s.threats);
		s.target_event = FL_TARGET_NONE;
	}
	s.states_time = now;
	const XwaHudState* hud = &snap->hud;
	const uint8_t in_hangar = snap->flight_camera.in_hangar != 0;
	if (in_hangar && !s.states_in_hangar) {
		memset(&s.threats, 0, sizeof s.threats);
	}
	s.states_in_hangar = in_hangar;
	if (hud->threats.valid) {
		s.threats = hud->threats;
	}
	uint32_t mask = in_hangar ? 1u << AERON_ANIM_STATE_HANGAR : 0u;
	uint8_t target = FL_TARGET_NONE;
	if (hud->valid) {
		mask |= s.threats.laser ? 1u << AERON_ANIM_STATE_THREAT_LASER : 0u;
		mask |= s.threats.turret ? 1u << AERON_ANIM_STATE_THREAT_TURRET : 0u;
		mask |= s.threats.beam ? 1u << AERON_ANIM_STATE_THREAT_BEAM : 0u;
		mask |= s.threats.missile == 1 ? 1u << AERON_ANIM_STATE_THREAT_MISSILE_LOCKING : 0u;
		mask |= s.threats.missile == 2 ? 1u << AERON_ANIM_STATE_THREAT_MISSILE_LOCKED : 0u;
		mask |= fl_cannons_ready(hud) << AERON_ANIM_STATE_CANNON_1;
		mask |= fl_broken_states(hud->instruments.installed_features, hud->instruments.active_features);
		const uint32_t hull = fl_hull_stage(hud->instruments.hull_damage, hud->instruments.hull_max);
		mask |= hull == 1   ? 1u << AERON_ANIM_STATE_HULL_75
				: hull == 2 ? 1u << AERON_ANIM_STATE_HULL_50
				: hull == 3 ? 1u << AERON_ANIM_STATE_HULL_25
							: 0u;
		target = hud->target.valid ? FL_TARGET_SELECTED : FL_TARGET_NONE;
		if (hud->reticle.in_range) {
			target = FL_TARGET_LASER_LOCK;
		}
		if (hud->reticle.weapon_mode && hud->reticle.missile_lock_state == 2) {
			target = FL_TARGET_WARHEAD_LOCKING;
		} else if (hud->reticle.weapon_mode && hud->reticle.missile_lock_state == 3) {
			target = FL_TARGET_WARHEAD_LOCKED;
		}
		mask |= target == FL_TARGET_LASER_LOCK        ? 1u << AERON_ANIM_STATE_LASER_LOCK
				: target == FL_TARGET_WARHEAD_LOCKING ? 1u << AERON_ANIM_STATE_WARHEAD_LOCKING
				: target == FL_TARGET_WARHEAD_LOCKED  ? 1u << AERON_ANIM_STATE_WARHEAD_LOCKED
													  : 0u;
		mask |= target != FL_TARGET_NONE ? 1u << AERON_ANIM_STATE_TARGET : 0u;
	}
	uint32_t fired = mask & ~s.player_states.mask & ~(1u << AERON_ANIM_STATE_TARGET);
	if (target == FL_TARGET_SELECTED && s.target_event != FL_TARGET_SELECTED) {
		fired |= 1u << AERON_ANIM_STATE_TARGET;
	}
	s.target_event = target;
	fl_state_onsets_update(&s.player_states, mask, fired, now);
	const uint32_t forced = s.anim_override.enabled ? s.anim_override.state_mask : 0u;
	fl_state_onsets_update(&s.override_states, forced, forced & ~s.override_states.mask, now);
	const uint32_t forced_energy = s.anim_override.enabled ? s.anim_override.energy : 0u;
	if (forced_energy != s.override_energy) {
		s.override_energy = forced_energy;
		s.override_energy_start = now;
	}
}

/* Begins the scene's frame with its exposure, fixed or metered, and with
 * the classic images (screens, glows, explosions) at the classic radiance. */
/* Creates the pipelines of the loaded models when the scene or the models
 * change: a scene keeps its pipelines for its whole life, and a new one (a
 * new size or MSAA count) starts without them. This runs in a frame that is
 * already a transition, such as the first one of a mission, rather than in
 * the frame that first shows each model. */
static void fl_prepare_meshes(void) {
	const uint64_t generation = XwaRemasterShip_MeshGeneration();
	if (s.prepared_scene_serial == s.scene_serial && s.prepared_mesh_generation == generation) {
		return;
	}
	s.prepared_scene_serial = s.scene_serial;
	s.prepared_mesh_generation = generation;
	const uint64_t start_us = Aeron_NowUs();
	uint32_t failed = 0;
	const uint32_t models = XwaRemasterShip_PrepareMeshes(s.scene, &failed);
	Aeron_LogInfo("xwa.remaster", "flight: pipelines of %u model(s) prepared in %.1f ms", models,
				  (double)(Aeron_NowUs() - start_us) / 1000.0);
	if (failed) {
		Aeron_LogWarn("xwa.remaster", "flight: the pipelines of %u model(s) couldn't all be created", failed);
	}
}

static int fl_begin_scene(const AeronSceneCamera* camera) {
	/* The map has a fixed exposure; leaving it snaps the meter. */
	if (s.exposure.metered && !s.map_present) {
		const XwaFlightExposureParams* e = &s.exposure;
		const AeronSceneAutoExposure meter = {
			.ev100 = e->ev100,
			.compensation = e->compensation,
			.metering = e->metering,
			.min_ev100 = e->min_ev100,
			.max_ev100 = e->max_ev100,
			.brighten_speed = e->brighten_speed,
			.darken_speed = e->darken_speed,
			.low_percent = e->low_percent,
			.high_percent = e->high_percent,
			.sky_weight = e->sky_weight,
			.near_weight = e->near_weight,
			.near_distance = e->near_distance,
			.edge_weight = e->edge_weight,
			.channel = e->max_channel ? AERON_SCENE_EXPOSURE_MAX : AERON_SCENE_EXPOSURE_LUMINANCE,
			.freeze = e->freeze || s.exposure_hold,
		};
		AeronScene_SetAutoExposure(s.scene, &meter);
	} else {
		AeronScene_SetAutoExposure(s.scene, NULL);
		AeronScene_SetExposure(s.scene, XwaRemasterFlight_Exposure());
	}
	AeronScene_SetImageRadiance(s.scene, XwaRemaster_ClassicRadiance());
	if (!AeronScene_Begin(s.scene, camera)) {
		return 0;
	}
	fl_prepare_meshes();
	XwaRemasterSuns_PrepareScene(s.scene, s.scene_serial, s.snap && s.snap->suns.procedural_flare);
	return 1;
}

/* Sets the scene's animation clock from the snapshot and updates the
 * material states at that time. */
static void fl_set_animation_time(const XwaSnapshot* snap) {
	const double now = (double)snap->game_time_ticks * XWA_SNAP_GAME_TICK_SECONDS;
	AeronScene_SetAnimationTime(s.scene, now);
	fl_update_material_states(snap, (float)now);
}

/* An object's material animation state for this frame, from the pool. The
 * player's craft carries the player states. */
static const AeronSceneMaterialAnim* fl_object_anim(const XwaFlightObject* object) {
	AeronSceneMaterialAnim* anim = fl_pool_peek(&s.anim_pool, sizeof *anim, FL_ANIMS_PER_BLOCK);
	if (!anim) {
		return NULL;
	}
	s.anim_pool.used++;
	memset(anim, 0, sizeof *anim);
	anim->seed = (uint32_t)object->slot << 16 | object->signature;
	anim->damage_stage = fl_damage_stage(object);
	anim->energy = fl_energy(object);
	if (s.snap && (int32_t)object->slot == s.snap->flight_camera.player_obj_idx) {
		anim->state_mask = s.player_states.mask;
		memcpy(anim->onset, s.player_states.onset, sizeof anim->onset);
	}
	const XwaFlightMaterialAnimOverride* o = &s.anim_override;
	const XwaHudTarget* target = s.snap ? &s.snap->hud.target : NULL;
	if (o->enabled && (o->all_objects || (target && target->valid && target->slot == object->slot &&
										  target->signature == object->signature))) {
		anim->damage_stage = o->damage_stage;
		anim->energy = o->energy;
		anim->state_mask = o->state_mask;
		memcpy(anim->onset, s.override_states.onset, sizeof anim->onset);
		anim->energy_start = s.override_energy_start;
	} else {
		anim->energy_start = fl_energy_start(object);
	}
	return anim;
}

void XwaRemasterFlight_GetMaterialAnimOverride(XwaFlightMaterialAnimOverride* out) {
	if (out) {
		*out = s.anim_override;
	}
}

void XwaRemasterFlight_SetMaterialAnimOverride(const XwaFlightMaterialAnimOverride* in) {
	if (in) {
		s.anim_override = *in;
	}
}

uint32_t XwaRemasterFlight_GetPlayerMaterialStates(void) { return s.player_states.mask; }

/* Resolve the model and its current articulation once for both flight views.
 * Callers retain ownership of motion, lighting, shadows and effect submission. */
static int fl_prepare_object_mesh(const XwaFlightObject* object, const char* model_name_override,
								  FlPreparedMesh* out) {
	if (!object || !out) {
		return 0;
	}
	memset(out, 0, sizeof *out);
	if (object->object_type == XWA_SNAP_TYPE_DEBRIS_CHUNK) {
		out->mesh = XwaRemasterShip_MeshForName(XwaSnapshotExport_ModelName(object->source_object_type));
		out->table = out->mesh ? fl_table_peek() : NULL;
		if (!out->table || !XwaRemasterShip_BuildDebrisMeshTable(out->mesh, object, out->table)) {
			out->table = NULL;
			return 0;
		}
		s.table_pool.used++;
		return 1;
	}

	const char* model_name = model_name_override ? model_name_override : fl_object_model_name(object);
	out->mesh = fl_mesh_for_name(model_name);
	if (!out->mesh) {
		return 0;
	}
	if (XwaRemasterShip_NeedsMeshTable(out->mesh, object)) {
		AeronSceneMeshTable* candidate = fl_table_peek();
		if (candidate && XwaRemasterShip_BuildMeshTable(out->mesh, object, candidate)) {
			out->table = candidate;
			s.table_pool.used++;
		}
	}
	out->anim = fl_object_anim(object);
	return 1;
}

/* The map owns eligibility and icon substitution. This helper owns only the
 * already-selected modern primitive, using the same pose, articulation and
 * sprite laws as ordinary flight. */
static int fl_map_submit_object(const XwaFlightMapObject* map_object, const XwaFlightObject* f,
								uint32_t snapshot_index, void* user) {
	FlMapSubmitContext* context = (FlMapSubmitContext*)user;
	if (!context || !context->cmd || !context->assets || !map_object || !f)
		return 0;

	if (map_object->render_kind == XWA_FLIGHT_MAP_RENDER_SCENE_OBJECT) {
		const uint32_t old_count = s.bb_count;
		fl_derive_object_billboard(f, snapshot_index, context->assets, NULL, NULL);
		if (s.bb_count != old_count)
			return 1;
	}

	const int is_bolt = fl_object_is_projectile(f);
	float bw[9], object_local[3], model_matrix[16];
	fl_object_pose(f, s.snap->flight_camera.world_pos, is_bolt, bw, object_local, model_matrix);

	FlPreparedMesh prepared;
	if (!fl_prepare_object_mesh(f, NULL, &prepared)) {
		return 1;
	}

	AeronSceneMeshInstance instance;
	memset(&instance, 0, sizeof instance);
	instance.mesh = prepared.mesh;
	memcpy(instance.transform, model_matrix, sizeof model_matrix);
	memcpy(instance.prev_transform, model_matrix, sizeof model_matrix);
	instance.variant = f->node_switch;
	instance.hidden_tag_groups = XwaRemasterShip_HiddenTagGroups(prepared.mesh, f);
	XwaRemasterSkins_CheckMarkings(instance.mesh, instance.variant);
	instance.mesh_table = prepared.table;
	instance.prev_mesh_table = prepared.table;
	instance.anim = prepared.anim;
	instance.zero_velocity = 1;
	instance.cull_mode = is_bolt ? AERON_CULL_NONE : AERON_CULL_BACK;
	if (is_bolt) {
		instance.shadow_flags = AERON_SCENE_INSTANCE_NO_CAST_SHADOW | AERON_SCENE_INSTANCE_NO_RECEIVE_SHADOW;
		instance.velocity_stamp = 1;
	}
	AeronScene_AddMeshInstance(s.scene, &instance);
	if (!is_bolt && f->object_type != XWA_SNAP_TYPE_DEBRIS_CHUNK) {
		XwaRemasterGlowMarks_SubmitObject(s.scene, context->cmd, context->assets, s.snap, f, prepared.mesh,
										  model_matrix, prepared.table, 1.0f, 1.0f, 1);
		if (map_object->render_kind == XWA_FLIGHT_MAP_RENDER_CRAFT)
			fl_derive_wreck_flames(f, snapshot_index, context->assets, NULL, NULL);
		if (s.glow_ok) {
			XwaRemasterShip_SubmitEngineGlows(s.scene, prepared.mesh, model_matrix, AERON_OPT_UNITS_PER_METER,
											  prepared.table, f->eg_knockout_mask, f->eg_hidden_mask,
											  XwaRemasterShip_EngineGlowScale(f), s.crows, s.camera_local,
											  &s.glow_ref);
		}
	}
	return 1;
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
	const XwaFlightObject* f = fl_object_by_slot(snap, cam->player_obj_idx);
	if (!f) {
		return;
	}
	float cur[3][3], bw[9];
	if (f->has_mobj && !f->orient_dirty)
		fl_curmat_from_cached(f->rows, cur);
	else
		fl_curmat_from_euler(f, cur);
	fl_object_world(cur, bw);
	memcpy(anchor_bw, bw, 9 * sizeof(float));
	*anchor_found = 1;
	*player_f = f;
	memcpy(player_bw, bw, 9 * sizeof(float));
	fl_object_local(f, player_local);
	*player_found = 1;
}

static int fl_hyper_normalize_axis(float axis[3]) {
	const float length = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
	if (!isfinite(length) || length <= 0.0001f) {
		return 0;
	}
	axis[0] /= length;
	axis[1] /= length;
	axis[2] /= length;
	return 1;
}

static void fl_hyper_build_tunnel_view(const AeronSceneCamera* camera, const float player_bw[9],
									   int player_found, XwaRemasterHyperspaceTunnelView* out) {
	memset(out, 0, sizeof *out);
	out->right[0] = 1.0f;
	out->up[1] = -1.0f;
	out->forward[2] = 1.0f;
	out->tan_half_fov_x = fmaxf(tanf(camera->h_half_rad), 0.0001f);
	out->tan_half_fov_y = fmaxf(tanf(camera->v_half_rad), 0.0001f);
	out->proj_offset_x = camera->proj_x_offset;
	out->proj_offset_y = camera->proj_y_offset;
	if (!player_found) {
		return;
	}

	/* player_bw uses the recovered (R0, R2, R1) row order: right,
	 * ship-backward, and up. Negate its longitudinal row so signed tunnel
	 * motion follows the craft's travel direction. */
	float right[3], up[3], forward[3];
	fl_world_to_view(s.crows, player_bw[0], player_bw[1], player_bw[2], right);
	fl_world_to_view(s.crows, player_bw[6], player_bw[7], player_bw[8], up);
	fl_world_to_view(s.crows, -player_bw[3], -player_bw[4], -player_bw[5], forward);
	if (!fl_hyper_normalize_axis(forward)) {
		return;
	}
	const float right_forward = right[0] * forward[0] + right[1] * forward[1] + right[2] * forward[2];
	for (int axis = 0; axis < 3; axis++) {
		right[axis] -= forward[axis] * right_forward;
	}
	if (!fl_hyper_normalize_axis(right)) {
		return;
	}
	const float up_forward = up[0] * forward[0] + up[1] * forward[1] + up[2] * forward[2];
	const float up_right = up[0] * right[0] + up[1] * right[1] + up[2] * right[2];
	for (int axis = 0; axis < 3; axis++) {
		up[axis] -= forward[axis] * up_forward + right[axis] * up_right;
	}
	if (!fl_hyper_normalize_axis(up)) {
		return;
	}
	memcpy(out->right, right, sizeof out->right);
	memcpy(out->up, up, sizeof out->up);
	memcpy(out->forward, forward, sizeof out->forward);
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
		w[a] =
			-(snap->cockpit.seat_eye_world_offset[a] + snap->cockpit.camera_pan[a] * 0.0625f + cam->sway[a]);
	}
	fl_world_to_view(s.crows, w[0], w[1], w[2], delta);
	float pw[3];
	for (int j = 0; j < 3; j++) {
		pw[j] = s.camera_local[j] + s.crows[0 * 3 + j] * delta[0] + s.crows[1 * 3 + j] * delta[1] +
				s.crows[2 * 3 + j] * delta[2];
	}
	if (snap->cockpit.inverted) {
		fl_apply_inverted_cockpit_pivot(bw, &snap->cockpit, cam->sway, s.camera_local, pw);
	}
	float m[16];
	fl_model_matrix(bw, pw, m);
	AeronSceneMeshInstance inst;
	memset(&inst, 0, sizeof inst);
	inst.mesh = cockpit_mesh;
	inst.variant = player_f ? player_f->node_switch : 0;
	XwaRemasterSkins_CheckMarkings(inst.mesh, inst.variant);
	memcpy(inst.transform, m, sizeof m);
	memcpy(inst.prev_transform, m, sizeof m);
	inst.zero_velocity = 1;
	inst.cull_mode = AERON_CULL_BACK;
	inst.anim = player_f ? fl_object_anim(player_f) : NULL;
	AeronSceneMeshTable* tb = fl_table_peek();
	XwaRemasterCockpitControls controls;
	XwaRemasterShip_CockpitControls(snap, player_f, &controls);
	if (tb && XwaRemasterShip_BuildCockpitMeshTable(cockpit_mesh, &snap->cockpit, player_f, &controls, tb)) {
		inst.mesh_table = tb;
		s.table_pool.used++;
	}
	AeronScene_AddMeshInstance(s.scene, &inst);
	fl_add_model_lights(cockpit_mesh, m, inst.mesh_table, inst.variant, 0, 1, 0, 0);
	/* No blast marks on the cockpit: the game sizes a warhead's mark from the
	 * whole craft, so it would cover most of the cockpit and the view. */
	if (player_f && fl_is_player_craft(snap, player_f->slot, player_f->signature) &&
		(s.cockpit_glow_mark_emissive_strength > 0.0f || s.cockpit_hit_flash_emissive_strength > 0.0f)) {
		XwaRemasterGlowMarks_SubmitObject(s.scene, cmd, assets, snap, player_f, cockpit_mesh, m,
										  inst.mesh_table, s.cockpit_glow_mark_emissive_strength,
										  s.cockpit_hit_flash_emissive_strength, 0);
	}
	if (s.glow_ok && player_f) {
		XwaRemasterShip_SubmitEngineGlows(s.scene, cockpit_mesh, m, AERON_OPT_UNITS_PER_METER,
										  inst.mesh_table, player_f->eg_knockout_mask,
										  player_f->eg_hidden_mask, XwaRemasterShip_EngineGlowScale(player_f),
										  s.crows, s.camera_local, &s.glow_ref);
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

/* Draws the scene with its bloom and exposure over the whole target, with
 * `chain` and its current debug view. */
static void fl_draw_present_scene(AeronRenderPass* pass, AeronScenePresentChain* chain, int target_w,
								  int target_h) {
	const AeronRectI full_target = { 0, 0, target_w, target_h };
	Aeron_SetViewport(pass, &full_target);
	Aeron_SetScissor(pass, &full_target);
	static const float present_tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	/* The flight scene's exposure, on the GPU: metered from this frame. The
	 * map keeps the CPU value. */
	AeronTexture* const exposure_tex = s.map_present ? NULL : AeronScene_ExposureTexture(s.scene);
	AeronScenePresentChain_Draw(chain, pass, s.present_scene_tex, s.present_sampler, s.present_bloom_tex,
								s.map_present ? 0.0f : AeronSceneBloom_Intensity(),
								exposure_tex ? 1.0f : XwaRemasterFlight_Exposure(), exposure_tex, s.rt_w,
								s.rt_h,
								/*bar_y_uv=*/1.0f, present_tint, /*src_coverage=*/0);
}

/* Shared tone-map + semantic-HUD draw sequence. The target can be the FP16
 * composition fallback or Aeron's borrowed swapchain target. */
static void fl_draw_present(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
							AeronScenePresentChain* chain, int target_w, int target_h) {
	if (!cmd || !pass || !target || !chain || !s.present_scene_tex || !s.present_sampler) {
		return;
	}
	/* The weight view needs the meter, which the map doesn't run. */
	AeronScenePresentDebug debug = { .view = (AeronScenePresentDebugView)s.present_debug_view };
	if (debug.view == AERON_SCENE_PRESENT_METER_WEIGHT &&
		(s.map_present || !AeronScene_GetMeterDebug(s.scene, &debug))) {
		debug.view = AERON_SCENE_PRESENT_IMAGE;
	}
	AeronScenePresentChain_SetDebug(chain, &debug);
	fl_draw_present_scene(pass, chain, target_w, target_h);
	if (s.suppress_hud) {
		return;
	}
	/* With the Dynamic Cockpit active the HUD's lists already leave out
	 * what the cockpit's screens show (XwaRemasterDc_AddSprite). */
	if (!s.map_present) {
		Aeron_GpuDebugMarker(cmd, "HUD target boxes before fixed");
		XwaRemasterHud_RenderTargetBoxes(cmd, pass, target, target_w, target_h,
										 XWA_HUD_TARGET_BOX_BEFORE_FIXED);
	}
	Aeron_GpuDebugMarker(cmd, "HUD fixed");
	XwaRemasterHud_RenderFixed(cmd, pass, target, target_w, target_h);
	if (!s.map_present) {
		Aeron_GpuDebugMarker(cmd, "HUD target boxes after fixed");
		XwaRemasterHud_RenderTargetBoxes(cmd, pass, target, target_w, target_h,
										 XWA_HUD_TARGET_BOX_AFTER_FIXED);
		Aeron_GpuDebugMarker(cmd, "HUD target readout");
		XwaRemasterHudReadout_Render(cmd, pass, target, target_w, target_h, s.scene);
	}
	Aeron_GpuDebugMarker(cmd, "HUD CMD");
	XwaRemasterHud_RenderCmd(cmd, pass, target, target_w, target_h);
	Aeron_GpuDebugMarker(cmd, "HUD text");
	XwaRemasterHud_RenderText(cmd, pass, target, target_w, target_h);
	XwaRemasterDc_RenderDebugOverlay(cmd, pass, target);
	if (s.map_present) {
		Aeron_GpuDebugMarker(cmd, "Flight map deferred text");
		XwaRemasterFlightMap_RenderDeferredText(cmd, pass, target);
	}
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

AeronRenderTarget* XwaRemasterFlight_ComposedTarget(void) { return s.present_rt; }

int XwaRemasterFlight_RecordBeforeTonemap(AeronCommandBuffer* cmd) {
	if (!cmd || !s.present || !s.present_rt || !s.present_scene_tex || !s.present_sampler) {
		return 0;
	}
	AeronRenderPass* pass = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
		.color_target = s.present_rt,
		.clear_color = 1,
		.clear_color_rgba = { 0.0f, 0.0f, 0.0f, 0.0f },
		.command_buffer = cmd,
		.debug_label = "OpenXWA flight capture before tonemap",
	});
	if (!pass) {
		return 0;
	}
	AeronScenePresentChain_SetDebug(s.present,
									&(AeronScenePresentDebug) { .view = AERON_SCENE_PRESENT_BEFORE_TONEMAP });
	fl_draw_present_scene(pass, s.present, s.rt_w, s.rt_h);
	Aeron_EndRenderPass(pass);
	return 1;
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
	s.present_scene_tex = s.present_scene_override ? s.present_scene_override
												   : Aeron_RenderTargetGetTexture(AeronScene_SceneRt(s.scene));
	s.present_scene_override = NULL;
	s.present_bloom_tex = NULL;
	if (!s.present_scene_tex) {
		s.direct_ready = 0;
		return NULL;
	}
	if (!s.map_present && s.bloom && AeronSceneBloom_Intensity() > 0.0f) {
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

static void fl_add_ambient_cube(XwaShipAmbientCube* base, const XwaShipAmbientCube* add) {
	float* const base_lobes[6] = { base->pos_x, base->neg_x, base->pos_y,
								   base->neg_y, base->pos_z, base->neg_z };
	const float* const add_lobes[6] = {
		add->pos_x, add->neg_x, add->pos_y, add->neg_y, add->pos_z, add->neg_z
	};
	for (int lobe = 0; lobe < 6; lobe++) {
		for (int channel = 0; channel < 3; channel++) {
			base_lobes[lobe][channel] += add_lobes[lobe][channel];
		}
	}
}

/* During the jump's entry and exit, the scene warp doesn't touch anything
 * nearer than the keep distance, and draws the streaks and the tunnel's
 * fades over everything farther. This is the smallest keep distance, which
 * covers the cockpit. It grows to cover the whole player craft. */
#define FL_HYPERSPACE_KEEP_DISTANCE_M 5.0f

static float fl_smoothstep(float edge0, float edge1, float x) {
	const float u = fl_clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return u * u * (3.0f - 2.0f * u);
}

static void fl_lerp3(float out[3], const float a[3], const float b[3], float w) {
	for (int channel = 0; channel < 3; channel++) {
		out[channel] = a[channel] + (b[channel] - a[channel]) * w;
	}
}

/* Returns the camera settings of the jump at `moment`: the look's settings
 * if the jump has a look, otherwise the normal jump's. */
static const XwaFlightHyperspaceCameraParams*
fl_hyperspace_camera_for(const XwaRemasterHyperspaceMoment* moment) {
	if (moment->look >= 0 && moment->look < s.hyperspace_look_count) {
		return &s.hyperspace_looks[moment->look].camera;
	}
	return &s.hyperspace_camera;
}

/* Returns 1 when the Camera Shake option (camera_shake) is on. */
static int fl_hyperspace_camera_allowed(void) {
	XwaModernCameraShakeOptions options;
	XwaModernCameraShakeOptions_Get(&options);
	return options.enabled != 0;
}

/* Returns how far through the tunnel `moment` is, from 0 to 1. The tunnel
 * preview has no end, so the debug UI sets this value for it. */
static float fl_hyperspace_tunnel_progress(const XwaRemasterHyperspaceMoment* moment) {
	if (moment->tunnel_only) {
		return s.hyperspace_preview_look_blend;
	}
	return moment->tunnel_ticks > 0.0f ? fl_clampf(moment->phase_ticks / moment->tunnel_ticks, 0.0f, 1.0f)
									   : 0.0f;
}

/* Returns the strength of the shake at `progress`: 0 before `start`, 1 after
 * `full`, and a straight line in between. */
static float fl_hyperspace_camera_ramp(float start, float full, float progress) {
	if (full <= start) {
		return progress >= start ? 1.0f : 0.0f;
	}
	return fl_clampf((progress - start) / (full - start), 0.0f, 1.0f);
}

#define FL_DEGREES_TO_RADIANS (3.14159265f / 180.0f)
#define FL_TWO_PI 6.28318531f

/* Sets the effect's tunnel and streak settings for `moment`, with its look.
 * The entry keeps the normal look. In the tunnel, the look spreads from the
 * far end toward the camera between blend_start and blend_end. The exit uses
 * the look fully. */
static void fl_hyperspace_apply_look(const XwaRemasterHyperspaceMoment* moment) {
	XwaFlightHyperspaceJumpParams jump = s.hyperspace_jump;
	XwaRemasterHyperspaceFront front;
	const float progress = fl_hyperspace_tunnel_progress(moment);

	memset(&front, 0, sizeof front);

	if (moment->look >= 0 && moment->look < s.hyperspace_look_count) {
		const XwaFlightHyperspaceLookParams* look = &s.hyperspace_looks[moment->look];
		float weight = 0.0f;

		if (moment->phase == XWA_HYPERSPACE_TUNNEL) {
			weight = look->blend_end > look->blend_start
						 ? fl_smoothstep(look->blend_start, look->blend_end, progress)
						 : (progress >= look->blend_start ? 1.0f : 0.0f);
		} else if (moment->phase == XWA_HYPERSPACE_INBOUND) {
			weight = 1.0f;
		}
		if (moment->phase != XWA_HYPERSPACE_OUTBOUND) {
			front.active = 1;
			front.progress = weight;
			memcpy(front.dark_color, look->dark_color, sizeof front.dark_color);
			memcpy(front.body_color, look->body_color, sizeof front.body_color);
			memcpy(front.highlight_color, look->highlight_color, sizeof front.highlight_color);
			memcpy(front.cap_color, look->cap_color, sizeof front.cap_color);
			memcpy(front.edge_color, look->edge_color, sizeof front.edge_color);
			front.edge_width = look->edge_width;
			front.noise = look->front_noise;
			front.noise_scale = look->front_noise_scale;
			front.warp = look->front_warp;
		}
		fl_lerp3(jump.tint, jump.tint, look->tint, weight);
		fl_lerp3(jump.tail_color, jump.tail_color, look->tail_color, weight);
	}
	XwaRemasterHyperspace_SetParams(s.hyperspace, &s.hyperspace_params);
	XwaRemasterHyperspace_SetJumpParams(s.hyperspace, &jump);
	XwaRemasterHyperspace_SetFront(s.hyperspace, &front);
}

/* ---- Hyperspace camera shake and jolt (docs/hd_hyperspace_jump.md, Camera)
 * They depend only on the hyperspace moment and the settings, so the previous
 * snapshot gives the previous frame's camera exactly. Angles are in radians:
 * a positive pitch turns the view down, a positive yaw to the right and a
 * positive roll clockwise. */

static uint32_t fl_hash32(uint32_t x) {
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

/* Returns a pseudo-random number from 0 to 1 for `x`. */
static float fl_hash_unit(uint32_t x) { return (float)(fl_hash32(x) >> 8) * (1.0f / 16777216.0f); }

/* Writes a fast tremble at `seconds` to `out`, up to about 1 on each axis.
 * Each axis is the sum of two sines at unrelated frequencies, and roll is
 * half as strong. The seed picks the phases, so two jumps don't shake the
 * same way. */
static void fl_hyperspace_tremble(float seconds, float speed, uint32_t seed, float out[3]) {
	static const float frequencies[3][2] = { { 7.3f, 11.9f }, { 6.1f, 13.7f }, { 4.3f, 9.1f } };
	static const float weights[3] = { 1.0f, 1.0f, 0.5f };
	for (int axis = 0; axis < 3; axis++) {
		const float phase0 = fl_hash_unit(seed + 2u * (uint32_t)axis) * FL_TWO_PI;
		const float phase1 = fl_hash_unit(seed + 2u * (uint32_t)axis + 1u) * FL_TWO_PI;
		out[axis] =
			weights[axis] * (0.6f * sinf(FL_TWO_PI * frequencies[axis][0] * speed * seconds + phase0) +
							 0.4f * sinf(FL_TWO_PI * frequencies[axis][1] * speed * seconds + phase1));
	}
}

/* How long a kick takes to reach its peak, and how long it can still be
 * seen after it starts, in seconds. */
#define FL_HYPERSPACE_KICK_RISE_S 0.08f
#define FL_HYPERSPACE_KICK_LENGTH_S 0.5f

/* Writes the kicks of the shake at `seconds` to `out`. Time is split into
 * slots of 1 / `rate` seconds, with one kick in each slot. The seed and the
 * slot number pick the kick's time and direction. A kick peaks at 1 after
 * 80 ms and has almost faded after 0.4 s. */
static void fl_hyperspace_kicks(float seconds, float rate, uint32_t seed, float out[3]) {
	out[0] = out[1] = out[2] = 0.0f;
	if (rate <= 0.0f || seconds <= 0.0f) {
		return;
	}
	const int64_t last = (int64_t)floorf(seconds * rate);
	int64_t first = (int64_t)floorf((seconds - FL_HYPERSPACE_KICK_LENGTH_S) * rate);
	if (first < 0) {
		first = 0;
	}
	for (int64_t slot = first; slot <= last; slot++) {
		const uint32_t key = fl_hash32(seed ^ (uint32_t)slot * 0x9e3779b9u);
		const float at = ((float)slot + 0.1f + 0.8f * fl_hash_unit(key)) / rate;
		const float u = (seconds - at) / FL_HYPERSPACE_KICK_RISE_S;
		if (u < 0.0f) {
			continue;
		}
		const float shape = u * expf(1.0f - u);
		const float pitch = fl_hash_unit(key + 1u) * 2.0f - 1.0f;
		const float yaw = fl_hash_unit(key + 2u) * 2.0f - 1.0f;
		const float roll = (fl_hash_unit(key + 3u) * 2.0f - 1.0f) * 0.5f;
		const float length = sqrtf(pitch * pitch + yaw * yaw + roll * roll);
		if (length < 1.0e-3f) {
			continue;
		}
		out[0] += shape * pitch / length;
		out[1] += shape * yaw / length;
		out[2] += shape * roll / length;
	}
}

/* Adds the camera shake of `camera` at `moment` to `out`, scaled by
 * `weight`. The tremble goes up to shake_degrees, and the kicks up to twice
 * that. */
static void fl_hyperspace_add_shake(const XwaRemasterHyperspaceMoment* moment,
									const XwaFlightHyperspaceCameraParams* camera, float weight,
									float out[3]) {
	const float amplitude = camera->shake_degrees * weight * FL_DEGREES_TO_RADIANS;
	if (amplitude <= 0.0f) {
		return;
	}
	const float seconds = moment->phase_ticks / 236.0f;
	float tremble[3], kicks[3];
	fl_hyperspace_tremble(seconds, camera->shake_speed, moment->seed, tremble);
	fl_hyperspace_kicks(seconds, camera->kick_rate, moment->seed ^ 0x6a09e667u, kicks);
	for (int axis = 0; axis < 3; axis++) {
		out[axis] += amplitude * (tremble[axis] + 2.0f * kicks[axis]);
	}
}

/* Writes the camera shake in the tunnel at `moment` to `out`. The normal
 * jump's shake grows from its shake_start to its shake_full. With a look, the
 * look's shake gradually replaces the normal one between the look's
 * shake_start and shake_full, as the look's colours do. We mix the two
 * shakes, not their speeds, so the tremble doesn't speed up or slow down. */
static void fl_hyperspace_shake_angles(const XwaRemasterHyperspaceMoment* moment, float out[3]) {
	const float progress = fl_hyperspace_tunnel_progress(moment);
	const XwaFlightHyperspaceCameraParams* normal = &s.hyperspace_camera;
	const XwaFlightHyperspaceCameraParams* look = fl_hyperspace_camera_for(moment);
	const float look_weight =
		look != normal ? fl_hyperspace_camera_ramp(look->shake_start, look->shake_full, progress) : 0.0f;
	const float normal_weight = fl_hyperspace_camera_ramp(normal->shake_start, normal->shake_full, progress);

	out[0] = out[1] = out[2] = 0.0f;
	fl_hyperspace_add_shake(moment, normal, normal_weight * (1.0f - look_weight), out);
	if (look != normal) {
		fl_hyperspace_add_shake(moment, look, look_weight, out);
	}
}

/* The jolt's swing frequency, and when the jolt fades out, in seconds from
 * the start of the exit. The exit lasts 236 ticks (1 s). */
#define FL_HYPERSPACE_JOLT_HZ 5.0f
#define FL_HYPERSPACE_JOLT_FADE_START_S 0.7f
#define FL_HYPERSPACE_JOLT_FADE_END_S 1.0f

/* Writes the jolt at the exit at `moment` to `out`. The camera is kicked,
 * mostly down, and swings back and forth like a struck spring until it
 * settles. A small tremble fades out more slowly. The first swing is the
 * largest and reaches jolt_degrees. The jolt is 0 before jolt_delay and at
 * the end of the exit. */
static void fl_hyperspace_jolt_angles(const XwaRemasterHyperspaceMoment* moment,
									  const XwaFlightHyperspaceCameraParams* camera, float out[3]) {
	out[0] = out[1] = out[2] = 0.0f;
	const float amplitude = camera->jolt_degrees * FL_DEGREES_TO_RADIANS;
	const float seconds = moment->phase_ticks / 236.0f;
	const float u = seconds - camera->jolt_delay;
	if (amplitude <= 0.0f || u <= 0.0f) {
		return;
	}
	const float fade =
		1.0f - fl_smoothstep(FL_HYPERSPACE_JOLT_FADE_START_S, FL_HYPERSPACE_JOLT_FADE_END_S, seconds);
	const float omega = FL_TWO_PI * FL_HYPERSPACE_JOLT_HZ;
	/* exp(-u / time) * sin(omega * u) peaks at u = atan(omega * time) / omega.
	   Dividing by that peak makes the first swing reach jolt_degrees. */
	const float peak_u = atanf(omega * camera->jolt_time) / omega;
	const float peak = expf(-peak_u / camera->jolt_time) * sinf(omega * peak_u);
	const float decay = expf(-u / camera->jolt_time);
	const float kick = amplitude * fade * decay / peak;
	const float swing = omega * u;
	const float roll_sign = (moment->seed & 1u) ? 1.0f : -1.0f;
	const float yaw_sign = (moment->seed & 2u) ? 1.0f : -1.0f;
	float tremble[3];

	fl_hyperspace_tremble(seconds, 1.0f, moment->seed, tremble);
	/* The tremble fades in over 50 ms, so the camera doesn't jump when the
	   jolt starts. */
	const float shiver =
		0.3f * amplitude * fade * fl_clampf(u / 0.05f, 0.0f, 1.0f) * expf(-u / (2.0f * camera->jolt_time));
	out[0] = kick * sinf(swing) + shiver * tremble[0];
	out[1] = kick * 0.2f * yaw_sign * sinf(swing * 0.9f) + shiver * tremble[1];
	out[2] = kick * 0.35f * roll_sign * sinf(swing * 1.1f) + shiver * tremble[2];
}

/* Multiplies two 3x3 row-major matrices: out = a * b. `out` may not be `a`
 * or `b`. */
static void fl_mat3_mul(const float a[9], const float b[9], float out[9]) {
	for (int row = 0; row < 3; row++) {
		for (int column = 0; column < 3; column++) {
			out[row * 3 + column] = a[row * 3 + 0] * b[0 * 3 + column] + a[row * 3 + 1] * b[1 * 3 + column] +
									a[row * 3 + 2] * b[2 * 3 + column];
		}
	}
}

/* The view rotation of the hyperspace camera at `moment`, for
 * fl_build_view_at_origin: the shake in the tunnel and the jolt at the exit.
 * Writes the identity and returns 0 when there is none. */
static int fl_hyperspace_view_rotation(const XwaRemasterHyperspaceMoment* moment, float out[9]) {
	static const float identity[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	float angles[3] = { 0.0f, 0.0f, 0.0f };

	memcpy(out, identity, sizeof identity);
	if (!fl_hyperspace_camera_allowed()) {
		return 0;
	}
	const XwaFlightHyperspaceCameraParams* camera = fl_hyperspace_camera_for(moment);
	if (moment->phase == XWA_HYPERSPACE_TUNNEL) {
		fl_hyperspace_shake_angles(moment, angles);
	} else if (moment->phase == XWA_HYPERSPACE_INBOUND) {
		fl_hyperspace_jolt_angles(moment, camera, angles);
	}
	if (angles[0] == 0.0f && angles[1] == 0.0f && angles[2] == 0.0f) {
		return 0;
	}

	/* The camera's axes, in the eye space of the steady camera (x right, y
	   down, z forward), turn by C = pitch * yaw * roll. The eye-space
	   coordinates of the turned camera are C^T times the steady ones. */
	const float cp = cosf(angles[0]), sp = sinf(angles[0]);
	const float cy = cosf(angles[1]), sy = sinf(angles[1]);
	const float cr = cosf(angles[2]), sr = sinf(angles[2]);
	const float pitch[9] = { 1.0f, 0.0f, 0.0f, 0.0f, cp, sp, 0.0f, -sp, cp };
	const float yaw[9] = { cy, 0.0f, sy, 0.0f, 1.0f, 0.0f, -sy, 0.0f, cy };
	const float roll[9] = { cr, -sr, 0.0f, sr, cr, 0.0f, 0.0f, 0.0f, 1.0f };
	float pitch_yaw[9], turn[9];
	fl_mat3_mul(pitch, yaw, pitch_yaw);
	fl_mat3_mul(pitch_yaw, roll, turn);
	for (int row = 0; row < 3; row++) {
		for (int column = 0; column < 3; column++) {
			out[row * 3 + column] = turn[column * 3 + row];
		}
	}
	return 1;
}

/* The largest distance from the camera to any point of a model whose origin
 * is at `origin` (camera-local, OPT units): the distance to the origin plus
 * the distance from the origin to the farthest corner of the model's bounds,
 * which are in meters. Returns 0 without a model. */
static float fl_max_camera_distance_to_model(const AeronSceneMesh* mesh, const float origin[3]) {
	if (!mesh) {
		return 0.0f;
	}
	float corner = 0.0f, offset = 0.0f;
	for (int axis = 0; axis < 3; axis++) {
		const float c = fmaxf(fabsf(mesh->bound_min[axis]), fabsf(mesh->bound_max[axis]));
		const float d = origin[axis] - s.camera_local[axis];
		corner += c * c;
		offset += d * d;
	}
	return sqrtf(offset) + sqrtf(corner) * AERON_OPT_UNITS_PER_METER;
}

/* Prepares the jump's entry or exit over the normal flight scene. Returns 0
 * when it draws nothing this frame. */
static int fl_prepare_world_hyperspace(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
									   const XwaRemasterFlightView* flight_view,
									   const XwaFlightObject* cockpit_anchor) {
	if (!s.hyperspace_world_pending || !s.hyperspace) {
		return 0;
	}
	float anchor_bw[9], player_bw[9], player_local[3];
	int anchor_found, player_found;
	const XwaFlightObject* player_f;
	fl_hyper_find_poses(snap, &snap->flight_camera, anchor_bw, &anchor_found, &player_f, player_bw, player_local,
						&player_found);
	XwaRemasterHyperspaceTunnelView tunnel_view;
	fl_hyper_build_tunnel_view(&flight_view->camera, player_bw, player_found, &tunnel_view);
	/* The keep distance reaches past the far side of the player's craft and,
	 * in cockpit views, of the cockpit model, so they stay in place. */
	float keep_distance = FL_HYPERSPACE_KEEP_DISTANCE_M * AERON_OPT_UNITS_PER_METER;
	if (player_found && player_f) {
		const AeronSceneMesh* mesh = XwaRemasterShip_MeshForName(XwaSnapshotExport_ModelName(player_f->object_type));
		keep_distance = fmaxf(keep_distance, fl_max_camera_distance_to_model(mesh, player_local));
	}
	if (fl_cockpit_drawn(snap, &snap->flight_camera) && cockpit_anchor) {
		const AeronSceneMesh* mesh = XwaRemasterShip_MeshForName(snap->cockpit.model_name);
		float m[16];
		if (mesh && fl_cockpit_model_matrix(&snap->cockpit, snap->flight_camera.sway, cockpit_anchor, s.crows,
											s.camera_local, m)) {
			const float origin[3] = { m[3], m[7], m[11] };
			keep_distance = fmaxf(keep_distance, fl_max_camera_distance_to_model(mesh, origin));
		}
	}
	int render_w = 0, render_h = 0;
	AeronScene_RenderDims(s.scene, &render_w, &render_h);
	if (render_w <= 0 || render_h <= 0) {
		render_w = s.rt_w;
		render_h = s.rt_h;
	}
	fl_hyperspace_apply_look(&s.hyperspace_world_moment);
	if (!XwaRemasterHyperspace_Prepare(s.hyperspace, cmd, s.crows, render_w, render_h, &s.hyperspace_world_moment,
									   &tunnel_view, 1)) {
		return 0;
	}
	s.hyperspace_near_z = flight_view->camera.near_z > 0.0f ? flight_view->camera.near_z : 0.001f;
	/* Reversed Z with an infinite far plane: depth = near / distance, both in
	 * view units (OPT units). */
	s.hyperspace_keep_depth = s.hyperspace_near_z / keep_distance;
	s.exposure_hold = XwaRemasterHyperspace_HoldsExposure(s.hyperspace);
	return 1;
}

/* The matrix from the hyperspace view space (x right, y down, z forward) to
 * a cube: the camera's view-to-world, then `world_to_cube`. */
static void fl_view_to_cube(const float world_to_cube[9], float out[9]) {
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			out[r * 3 + c] = world_to_cube[r * 3 + 0] * s.crows[c * 3 + 0] +
							 world_to_cube[r * 3 + 1] * s.crows[c * 3 + 1] +
							 world_to_cube[r * 3 + 2] * s.crows[c * 3 + 2];
		}
	}
}

/* Draws the jump's entry or exit over the finished scene (the scene warp),
 * and has the result presented instead of the scene. */
static int fl_render_world_hyperspace(AeronCommandBuffer* cmd) {
	int render_w = 0, render_h = 0, output_w = 0, output_h = 0;
	AeronScene_RenderDims(s.scene, &render_w, &render_h);
	AeronScene_RtDims(s.scene, &output_w, &output_h);
	XwaRemasterHyperspaceWarpInput input = s.hyperspace_sky;
	input.scene = Aeron_RenderTargetGetTexture(AeronScene_SceneRt(s.scene));
	input.depth = Aeron_DepthTargetGetTexture(AeronScene_DepthRt(s.scene));
	input.output_w = output_w;
	input.output_h = output_h;
	input.render_w = render_w;
	input.render_h = render_h;
	input.near_z = s.hyperspace_near_z;
	input.keep_depth = s.hyperspace_keep_depth;
	input.units_per_meter = AERON_OPT_UNITS_PER_METER;
	input.view = (int)s.hyperspace_warp_view;
	input.screens = AeronScene_ScreenLayer(s.scene);
	s.present_scene_override = XwaRemasterHyperspace_RenderWarp(s.hyperspace, cmd, &input);
	return s.present_scene_override != NULL;
}

/* Picks the jump's stars: each region and each direction gets its own. */
static uint32_t fl_hyperspace_seed(const XwaSnapshot* snap, uint8_t phase) {
	uint32_t seed = ((uint32_t)snap->flight_camera.mission_region + 1u) * 0x9e3779b1u;
	if (phase == XWA_HYPERSPACE_INBOUND) {
		seed ^= 0x85ebca6bu;
	}
	return seed;
}

static int fl_real_hyperspace_moment(const XwaSnapshot* snap, XwaRemasterHyperspaceMoment* out) {
	memset(out, 0, sizeof *out);
	out->phase = snap->hyperspace.phase;
	out->phase_ticks = (float)snap->hyperspace.phase_elapsed_ticks;
	out->tunnel_ticks = (float)XWA_REMASTER_HYPERSPACE_TUNNEL_TICKS;
	out->seed = fl_hyperspace_seed(snap, out->phase);
	out->look = -1;
	if (snap->hyperspace.look[0] != '\0') {
		out->look = XwaRemasterFlight_FindHyperspaceLook(snap->hyperspace.look);
		if (out->look < 0 && strcmp(s.hyperspace_unknown_look, snap->hyperspace.look) != 0) {
			snprintf(s.hyperspace_unknown_look, sizeof s.hyperspace_unknown_look, "%s",
					 snap->hyperspace.look);
			Aeron_LogWarn("xwa.remaster",
						  "the mission asks for the hyperspace look '%s', which the game doesn't have",
						  snap->hyperspace.look);
		}
	}
	return 1;
}

/* The debug preview's moment, on its own clock. The jump cycle is the
 * entry, the tunnel and the exit, as long as in the game, then 1 s of normal
 * flight, for which this returns 0. */
static int fl_preview_hyperspace_moment(const XwaSnapshot* snap, uint64_t host_time_us,
										XwaRemasterHyperspaceMoment* out) {
	enum {
		FL_PREVIEW_OUTBOUND_TICKS = 590,
		FL_PREVIEW_TUNNEL_TICKS = XWA_REMASTER_HYPERSPACE_TUNNEL_TICKS,
		FL_PREVIEW_INBOUND_TICKS = 237,
		FL_PREVIEW_NORMAL_TICKS = 236,
	};
	if (host_time_us > s.hyperspace_preview_host_us) {
		double elapsed = (double)(host_time_us - s.hyperspace_preview_host_us) / 1000000.0;
		if (elapsed > 0.1) {
			elapsed = 0.1;
		}
		s.hyperspace_preview_seconds += elapsed * (double)XwaRemasterFlight_GetHyperspacePreviewSpeed();
	}
	s.hyperspace_preview_host_us = host_time_us;
	memset(out, 0, sizeof *out);
	out->look = s.hyperspace_preview_look;
	if (s.hyperspace_preview == XWA_FLIGHT_HYPERSPACE_PREVIEW_TUNNEL) {
		out->phase = XWA_HYPERSPACE_TUNNEL;
		out->phase_ticks = (float)(fmod(s.hyperspace_preview_seconds, 4096.0) * 236.0);
		out->tunnel_only = 1;
		out->seed = fl_hyperspace_seed(snap, out->phase);
		return 1;
	}
	if (s.hyperspace_preview != XWA_FLIGHT_HYPERSPACE_PREVIEW_JUMP_CYCLE) {
		return 0;
	}
	const double cycle = FL_PREVIEW_OUTBOUND_TICKS + FL_PREVIEW_TUNNEL_TICKS + FL_PREVIEW_INBOUND_TICKS +
						 FL_PREVIEW_NORMAL_TICKS;
	double ticks = fmod(s.hyperspace_preview_seconds * 236.0, cycle);
	out->tunnel_ticks = (float)FL_PREVIEW_TUNNEL_TICKS;
	if (ticks < FL_PREVIEW_OUTBOUND_TICKS) {
		out->phase = XWA_HYPERSPACE_OUTBOUND;
	} else if ((ticks -= FL_PREVIEW_OUTBOUND_TICKS) < FL_PREVIEW_TUNNEL_TICKS) {
		out->phase = XWA_HYPERSPACE_TUNNEL;
	} else if ((ticks -= FL_PREVIEW_TUNNEL_TICKS) < FL_PREVIEW_INBOUND_TICKS) {
		out->phase = XWA_HYPERSPACE_INBOUND;
	} else {
		return 0;
	}
	out->phase_ticks = (float)ticks;
	out->seed = fl_hyperspace_seed(snap, out->phase);
	return 1;
}

static AeronTexture* fl_render_hyperspace(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
										  XwaRemasterAssets* assets, const XwaRemasterFlightView* flight_view,
										  uint64_t host_time_us, int direct_present,
										  const XwaRemasterHyperspaceMoment* moment) {
	float anchor_bw[9], player_bw[9], player_local[3];
	int anchor_found, player_found;
	const XwaFlightObject* player_f;
	fl_hyper_find_poses(snap, &snap->flight_camera, anchor_bw, &anchor_found, &player_f, player_bw,
						player_local, &player_found);
	XwaRemasterHyperspaceTunnelView tunnel_view;
	fl_hyper_build_tunnel_view(&flight_view->camera, player_bw, player_found, &tunnel_view);

	int render_w = 0, render_h = 0;
	AeronScene_RenderDims(s.scene, &render_w, &render_h);
	if (render_w <= 0 || render_h <= 0) {
		render_w = s.rt_w;
		render_h = s.rt_h;
	}
	if (s.hyperspace) {
		fl_hyperspace_apply_look(moment);
	}
	if (!s.hyperspace || !XwaRemasterHyperspace_Prepare(s.hyperspace, cmd, s.crows, render_w, render_h, moment,
														&tunnel_view, 0)) {
		return NULL;
	}
	s.exposure_hold = XwaRemasterHyperspace_HoldsExposure(s.hyperspace);
	XwaRemasterHyperspaceLighting hyperspace_lighting;
	const int lighting_ready = XwaRemasterHyperspace_GetPreparedLighting(s.hyperspace, &hyperspace_lighting);
	/* Cockpit glow texture is the only ordinary-flight atlas dependency
	 * retained by the special scene. */
	s.glow_ok = XwaRemasterAssets_FlightAtlasFrame(assets, 1000, 0, &s.glow_ref);
	s.table_pool.used = 0;
	s.anim_pool.used = 0;
	fl_prepare_energy_states(snap);
	fl_begin_point_lights(flight_view);
	s.snap = snap;

	/* The tunnel is drawn by a game hook without motion vectors. Disable
	 * temporal reconstruction for this special scene and reset history
	 * before returning to ordinary flight. */
	fl_set_temporal(host_time_us, 0);
	fl_set_animation_time(snap);
	if (!fl_begin_scene(&flight_view->camera)) {
		Aeron_CommandBufferSetFailure(cmd, "Hyperspace scene initialization failed");
		s.snap = NULL;
		return NULL;
	}
	XwaRemasterDc_ApplyToScene(s.scene);
	s.before_opaque_mode = FL_BEFORE_OPAQUE_HYPERSPACE;
	AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, fl_draw_before_opaque, NULL);
	/* SSAO remains active for the 3D cockpit. Motion blur is deliberately
	 * disabled: the tunnel/streak hook has no velocity attachment, and the
	 * hyperspace effects encode their own motion in geometry and shader animation. */
	AeronScene_SetPost(s.scene, &(AeronScenePostDesc) {
									.ssao_quality = s.ssao.ssao_quality,
									.ssao_intensity = s.ssao.ssao_intensity,
									.ssao_power = s.ssao.ssao_power,
									.ssao_radius_view = s.ssao.ssao_radius_view,
									.ssao_bias_view = s.ssao.ssao_bias_view,
									.ssao_direct = s.ssao.ssao_direct,
									.ssao_debug_viz = s.ssao.ssao_debug_viz,
									.ssao_min_screen_frac = s.ssao.ssao_min_screen_frac,
									.ssao_max_screen_frac = s.ssao.ssao_max_screen_frac,
									.ssao_sample_jitter = s.ssao.ssao_sample_jitter,
									.mb_quality = 0,
								});
	AeronScene_SetMotionContext(s.scene, NULL, NULL, 1);

	if (player_found && player_f) {
		fl_add_cockpit_lights(snap, player_bw, player_local);
	}
	fl_finalize_point_lights();
	if (!fl_configure_clustered_lights(s.scene)) {
		Aeron_CommandBufferSetFailure(cmd, "Hyperspace clustered-light configuration failed");
		s.snap = NULL;
		return NULL;
	}
	const XwaShipAoParams ao = {
		.intensity = s.ssao.ssao_intensity,
		.power = s.ssao.ssao_power,
		.direct = s.ssao.ssao_direct,
	};
	const int ao_on = s.ssao.ssao_quality > 0 && s.ssao.ssao_intensity > 0.0f;
	XwaShipAmbientCube ambient;
	XwaRemasterShip_GetAmbientCube(&ambient);
	if (lighting_ready) {
		fl_add_ambient_cube(&ambient, &hyperspace_lighting.ambient_add);
	}
	XwaRemasterDirLight game_lights[XWA_SNAP_MAX_DIR_LIGHTS];
	const XwaRemasterDirLight* directional_lights = game_lights;
	uint32_t directional_light_count =
		XwaRemaster_ReadGameDirLights(snap->dir_lights, snap->dir_light_count, game_lights);
	if (moment->phase == XWA_HYPERSPACE_TUNNEL) {
		directional_lights = lighting_ready ? &hyperspace_lighting.key : NULL;
		directional_light_count = lighting_ready ? hyperspace_lighting.key_count : 0;
	} else if (lighting_ready && hyperspace_lighting.key_count &&
			   directional_light_count < XWA_SNAP_MAX_DIR_LIGHTS) {
		/* During the jump's fades the tunnel's key joins the game's lights,
		 * which the game is fading at the same time. */
		game_lights[directional_light_count++] = hyperspace_lighting.key;
	}
	const XwaShipEnvironmentMap* environment_map =
		lighting_ready && hyperspace_lighting.environment.texture ? &hyperspace_lighting.environment : NULL;
	XwaRemasterShip_SetPbrEnv(s.scene, directional_lights, directional_light_count, -1, NULL, s.camera_local,
							  ao_on ? &ao : NULL, &ambient, environment_map, -1.0f);
	fl_submit_hyperspace_cockpit(cmd, assets, snap, &snap->flight_camera, anchor_bw, anchor_found, player_f);
	fl_submit_point_lights();

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

/* Whether the region's environment draws a sky with `backdrops: keep`. */
static int fl_environment_keeps_backdrops(void) {
	return XwaRemasterEnvironment_KeepsBackdrops(&s.environment);
}

typedef struct FlSceneLighting {
	const XwaRemasterDirLight* lights;
	uint32_t light_count;
	/* The game's lights, read as linear lights (scene_lights.h). */
	XwaRemasterDirLight game_lights[XWA_SNAP_MAX_DIR_LIGHTS];
	uint32_t game_light_count;
	XwaRemasterDirLight hangar_key;
	XwaShipAmbientCube ambient_cube;
	const XwaShipAmbientCube* ambient;
	int hangar_override;
	/* Region environment: the prepared environment to bind, the combined
	 * light list (environment lights, then kept recovered lights), the
	 * explicit key and the shadow policy. key_index -1 keeps the recovered
	 * key selection. environment_lights is set when the environment's lights
	 * replace the recovered ones. */
	const AeronPreparedEnvironment* environment;
	XwaRemasterDirLight authored_lights[XWA_SNAP_MAX_DIR_LIGHTS];
	int32_t key_index;
	int shadow_allowed;
	float shadow_angular_radius_degrees;
	int environment_lights;
	/* When `captured` is set, these captured lighting cubes
	 * (captured_lighting.h) are bound in place of the environment's cubes. */
	AeronScenePbrEnvironment captured_environment;
	int captured;
} FlSceneLighting;

static int fl_is_default_hangar_lighting(const XwaSnapshot* snap) {
	if (!snap || snap->dir_light_count != 1) {
		return 0;
	}

	const XwaDirLight* light = &snap->dir_lights[0];
	const float epsilon = 0.0001f;
	return fabsf(light->world_dir[0]) <= epsilon && fabsf(light->world_dir[1]) <= epsilon &&
		   fabsf(light->world_dir[2] - 1.0f) <= epsilon && fabsf(light->intensity - 0.75f) <= epsilon &&
		   fabsf(light->color[0] - 1.0f) <= epsilon && fabsf(light->color[1] - 1.0f) <= epsilon &&
		   fabsf(light->color[2] - 1.0f) <= epsilon;
}

/* An environment light as a scene directional. Never a backdrop source. */
static void fl_environment_light_to_dir_light(const AeronEnvironmentLight* light, XwaRemasterDirLight* out) {
	memset(out, 0, sizeof *out);
	out->source_backdrop_index = -1;
	memcpy(out->world_dir, light->direction_world, sizeof out->world_dir);
	memcpy(out->color, light->color_linear, sizeof out->color);
}

/* While an environment is bound, it replaces the game's ambient. Its fill
 * is the least light the diffuse cube gives: where the cube is dimmer, the
 * shaders add part of the fill. */
static void fl_set_ambient_fill(FlSceneLighting* out, const float fill[3]) {
	memset(&out->ambient_cube, 0, sizeof out->ambient_cube);
	memcpy(out->ambient_cube.minimum, fill, sizeof out->ambient_cube.minimum);
	out->ambient = &out->ambient_cube;
}

static void fl_set_environment_ambient(FlSceneLighting* out, const AeronPreparedEnvironment* environment) {
	fl_set_ambient_fill(out, environment->settings.ambient_fill);
}

/* With `backdrop_light_scale`, multiplies the game lights of backdrops that
 * are not suns. */
static void fl_scale_backdrop_lights(const XwaSnapshot* snap, FlSceneLighting* out) {
	const float scale = s.environment.backdrops->light_scale;
	if (scale == 1.0f) {
		return;
	}
	uint32_t scaled = 0;
	for (uint32_t i = 0; i < out->game_light_count; i++) {
		XwaRemasterDirLight* light = &out->game_lights[i];
		const int16_t source = light->source_backdrop_index;
		if (source >= 0 && (uint32_t)source < snap->backdrop_count &&
			!fl_backdrop_is_sun(&snap->backdrops[source])) {
			for (int channel = 0; channel < 3; channel++) {
				light->color[channel] *= scale;
			}
			scaled++;
		}
	}
	static char logged_mission[sizeof snap->mission_path];
	static int logged_region = -1;
	const int region = snap->flight_camera.mission_region;
	if (scaled && (region != logged_region || strcmp(logged_mission, snap->mission_path) != 0)) {
		logged_region = region;
		snprintf(logged_mission, sizeof logged_mission, "%s", snap->mission_path);
		Aeron_LogInfo("xwa.remaster", "environment: region %d scales the lights of %u backdrop(s) by %g",
					  region, scaled, (double)scale);
	}
}

/* With `hidden_backdrop_lights: remove`, uses the recovered lights, except
 * the lights of hidden backdrops that are not suns. The cube's lighting
 * replaces them. Suns keep their light, because a sun painted in the sky is
 * clipped at white and is too dim to light the ship. */
static void fl_remove_hidden_backdrop_lights(const XwaSnapshot* snap, XwaRemasterAssets* assets,
											 FlSceneLighting* out) {
	uint32_t count = 0;
	uint32_t dropped = 0;
	for (uint32_t i = 0; i < out->game_light_count; i++) {
		const XwaRemasterDirLight* light = &out->game_lights[i];
		const int16_t source = light->source_backdrop_index;
		if (source >= 0 && (uint32_t)source < snap->backdrop_count) {
			const XwaBackdrop* b = &snap->backdrops[source];
			if (!fl_backdrop_is_sun(b) && fl_backdrop_hidden(assets, s.environment.backdrops, b->object_type,
															 (b->flags & 1u) ? b->frame : 1)) {
				dropped++;
				continue;
			}
		}
		out->authored_lights[count++] = *light;
	}
	out->lights = out->authored_lights;
	out->light_count = count;
	static char logged_mission[sizeof snap->mission_path];
	static int logged_region = -1;
	const int region = snap->flight_camera.mission_region;
	if (dropped && (region != logged_region || strcmp(logged_mission, snap->mission_path) != 0)) {
		logged_region = region;
		snprintf(logged_mission, sizeof logged_mission, "%s", snap->mission_path);
		Aeron_LogInfo("xwa.remaster", "environment: region %d leaves out the lights of %u hidden backdrop(s)",
					  region, dropped);
	}
}

/* Keeps the frame's sky cube for the scene warp. A sky that keeps the
 * mission's backdrops uses the captured cube, which has them, when there is
 * one. Otherwise the environment's sky, then the captured cube of a region
 * without an environment. The captured cube's level 0 is the sky itself;
 * its other levels are blurred for reflections. */
static void fl_note_hyperspace_sky(const AeronPreparedEnvironment* sky_environment, int keep_backdrops,
								   const FlSceneLighting* lighting) {
	XwaRemasterHyperspaceWarpInput* sky = &s.hyperspace_sky;
	memset(sky, 0, sizeof *sky);
	const AeronScenePbrEnvironment* captured =
		lighting->captured && lighting->captured_environment.specular ? &lighting->captured_environment : NULL;
	AeronPreparedEnvironment scaled;
	const AeronPreparedEnvironment* environment =
		sky_environment ? XwaRemasterEnvironment_ScaleIntensity(sky_environment, &scaled) : NULL;
	if (captured && (keep_backdrops || !environment)) {
		sky->sky_cube = captured->specular;
		sky->sky_sampler = captured->sampler;
		fl_view_to_cube(captured->world_to_local, sky->sky_view_to_cube);
		sky->sky_scale = 1.0f;
		sky->sky_max_level = 0;
	} else if (environment) {
		sky->sky_cube = environment->radiance;
		sky->sky_sampler = environment->sampler;
		fl_view_to_cube(environment->world_to_cube, sky->sky_view_to_cube);
		sky->sky_scale = environment->settings.intensity;
		sky->sky_max_level = Aeron_TextureGetMipCount(environment->radiance) - 1;
	}
	if (sky->sky_cube && environment && environment->overlay) {
		sky->overlay_cube = environment->overlay;
		sky->overlay_sampler = environment->sampler;
		fl_view_to_cube(environment->overlay_world_to_cube, sky->overlay_view_to_cube);
		sky->overlay_scale = environment->settings.intensity;
		sky->overlay_max_level = Aeron_TextureGetMipCount(environment->overlay) - 1;
	}
	if (!sky->sky_sampler) {
		sky->sky_cube = NULL;
		sky->overlay_cube = NULL;
	}
	if (!sky->overlay_sampler) {
		sky->overlay_cube = NULL;
	}
	if (sky->sky_max_level < 0) {
		sky->sky_max_level = 0;
	}
	if (sky->overlay_max_level < 0) {
		sky->overlay_max_level = 0;
	}
}

/* Applies a region environment. Its fill replaces the ambient. If the
 * environment has directional lights (an environment with a sky always has
 * them), they replace the directional lights derived from backdrops, unless
 * the environment keeps the backdrops: then the recovered lights stay,
 * scaled by fl_scale_backdrop_lights, minus those
 * fl_remove_hidden_backdrop_lights leaves out, and the key light is chosen
 * from them as without an environment.
 * Recovered lights without a backdrop source are kept, because they are not
 * known to belong to the sky. The environment's declared primary light then
 * becomes the key light and the only shadow caster. An environment without
 * lights keeps the recovered lights and key selection. */
static void fl_apply_authored_lighting(const XwaSnapshot* snap, XwaRemasterAssets* assets,
									   const AeronPreparedEnvironment* environment, FlSceneLighting* out) {
	out->environment = environment;
	fl_set_environment_ambient(out, environment);
	if (fl_environment_keeps_backdrops()) {
		fl_scale_backdrop_lights(snap, out);
		if (s.environment.backdrops->remove_hidden_lights) {
			fl_remove_hidden_backdrop_lights(snap, assets, out);
		}
		return;
	}
	if (!environment->definition->has_directional_lights) {
		return;
	}
	uint32_t count = 0;
	for (uint32_t i = 0; i < environment->light_count && count < XWA_SNAP_MAX_DIR_LIGHTS; i++) {
		fl_environment_light_to_dir_light(&environment->lights[i], &out->authored_lights[count++]);
	}
	uint32_t kept = 0;
	for (uint32_t i = 0; i < out->game_light_count && count < XWA_SNAP_MAX_DIR_LIGHTS; i++) {
		if (out->game_lights[i].source_backdrop_object_type == 0) {
			out->authored_lights[count++] = out->game_lights[i];
			kept++;
		}
	}
	/* In flight every recovered directional comes from a backdrop, so a kept
	 * untagged light is unexpected (a special scene, or a backdrop the
	 * snapshot could not trace) and may double-light the region. Logged
	 * once per environment and region. */
	static const AeronPreparedEnvironment* warned_environment;
	static uint8_t warned_region;
	if (kept > 0 && (warned_environment != environment || warned_region != snap->flight_camera.region)) {
		warned_environment = environment;
		warned_region = snap->flight_camera.region;
		Aeron_LogWarn("xwa.remaster",
					  "environment: authored region %u keeps %u recovered directional(s) without a backdrop "
					  "source; check for double lighting",
					  (unsigned)snap->flight_camera.region, kept);
	}
	out->lights = out->authored_lights;
	out->light_count = count;
	out->key_index = environment->primary_light_index;
	const AeronEnvironmentLight* primary =
		environment->primary_light_index >= 0 ? &environment->lights[environment->primary_light_index] : NULL;
	out->shadow_allowed = primary && primary->casts_shadows;
	out->shadow_angular_radius_degrees = primary ? primary->angular_radius_degrees : 0.0f;
	out->environment_lights = 1;
}

static void fl_build_hangar_lighting(const XwaSnapshot* snap, FlSceneLighting* out) {
	if (!s.hangar_lighting.enabled) {
		return;
	}

	/* The configured HD relight replaces only the classic hangar's fixed
	 * white key. Preserve state-driven hangar lighting such as carrier alarms. */
	out->hangar_override = 1;
	if (fl_is_default_hangar_lighting(snap)) {
		/* The configured colour is sRGB, and the intensity is illuminance in W/m^2. */
		memcpy(out->hangar_key.world_dir, s.hangar_lighting.direction, sizeof out->hangar_key.world_dir);
		for (int channel = 0; channel < 3; channel++) {
			out->hangar_key.color[channel] =
				XwaRemaster_SrgbToLinear(s.hangar_lighting.color[channel]) * s.hangar_lighting.intensity;
		}
		out->hangar_key.source_backdrop_object_type = 0;
		out->hangar_key.source_backdrop_index = -1;
		out->lights = &out->hangar_key;
		out->light_count = 1;
	}

	const XwaRemasterDirLight* ambient_light =
		XwaRemasterShip_SelectKeyDirectionalLight(out->lights, out->light_count);
	/* The ambient takes the key light's hue, not its brightness. */
	float ambient_color[3] = { 1.0f, 1.0f, 1.0f };
	if (ambient_light) {
		const float* color = ambient_light->color;
		const float peak = fmaxf(color[0], fmaxf(color[1], color[2]));
		if (peak > 0.0f) {
			for (int channel = 0; channel < 3; channel++) {
				ambient_color[channel] = color[channel] / peak;
			}
		}
	}
	for (int channel = 0; channel < 3; channel++) {
		out->ambient_cube.pos_x[channel] = out->ambient_cube.neg_x[channel] =
			s.hangar_lighting.ambient_sides * ambient_color[channel];
		out->ambient_cube.pos_y[channel] = out->ambient_cube.neg_y[channel] =
			s.hangar_lighting.ambient_sides * ambient_color[channel];
		out->ambient_cube.pos_z[channel] = s.hangar_lighting.ambient_ceiling * ambient_color[channel];
		out->ambient_cube.neg_z[channel] = s.hangar_lighting.ambient_floor * ambient_color[channel];
	}
	out->ambient = &out->ambient_cube;
}

static void fl_build_scene_lighting(const XwaSnapshot* snap, XwaRemasterAssets* assets,
									const XwaFlightCamera* cam, FlSceneLighting* out) {
	memset(out, 0, sizeof *out);
	out->game_light_count =
		XwaRemaster_ReadGameDirLights(snap->dir_lights, snap->dir_light_count, out->game_lights);
	out->lights = out->game_lights;
	out->light_count = out->game_light_count;
	out->key_index = -1;
	out->shadow_allowed = 1;
	out->shadow_angular_radius_degrees = s.shadows.light_angular_radius_degrees;
	/* The hangar uses its own light rig and the probes of the hangar model.
	 * The region's environment only provides the sky in the hangar. In a
	 * hangar without probes, the environment's fill, cubes and suns would
	 * light the interior through walls that cast no shadow. Its suns would
	 * also take the only shadow map from the rig's key light, whose shadow
	 * removes probe light under parked ships. */
	if (cam->in_hangar) {
		fl_build_hangar_lighting(snap, out);
		return;
	}
	/* The Death Star mission lights its interior with the light of a
	 * backdrop that the game doesn't draw there. As a sun, it would reach
	 * every surface from one direction, through the walls, since there are
	 * no shadows in the tunnel. Without an environment, the interior gets an
	 * ambient fill in place of that light. The tunnel's lights and the
	 * headlights light the rest. */
	if (cam->death_star_mode && s.environment.mode != XWA_ENV_MODE_AUTHORED) {
		out->light_count = 0;
		fl_set_ambient_fill(out, s.death_star_ambient);
		return;
	}
	if (s.environment.mode == XWA_ENV_MODE_AUTHORED && s.environment.prepared) {
		fl_apply_authored_lighting(snap, assets, s.environment.prepared, out);
	}
	/* The captured cubes replace the cubes of an environment whose sky keeps
	 * the backdrops, or add cubes to a region without an environment. */
	const float* fill = NULL;
	if (XwaRemasterCapturedLighting_Environment(snap, &s.environment, &out->captured_environment, &fill)) {
		out->captured = 1;
		if (fill) {
			fl_set_ambient_fill(out, fill);
		}
	}
}

/* The shadow-casting key: lights[key_index] when in range (an authored
 * primary), otherwise the recovered key selection. NULL without lights. */
static const XwaRemasterDirLight* fl_key_light(const XwaRemasterDirLight* lights, uint32_t light_count,
											   int32_t key_index) {
	if (!lights || light_count == 0) {
		return NULL;
	}
	return key_index >= 0 && (uint32_t)key_index < light_count
			   ? &lights[key_index]
			   : XwaRemasterShip_SelectKeyDirectionalLight(lights, light_count);
}

static int fl_key_directional_light(const XwaRemasterDirLight* lights, uint32_t light_count,
									int32_t key_index, float out_direction[3]) {
	const XwaRemasterDirLight* key = fl_key_light(lights, light_count, key_index);
	if (!key || !out_direction) {
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

/* The procedural lens flare, from the sun that casts the shadows: the
 * environment's primary light when the environment's lights replace the
 * game's, if it asks for a flare; otherwise the backdrop sun that made the
 * key light, on the quads it is drawn on this frame. The key light is
 * chosen even when shadows are off. Without such a sun, no flare. */
static void fl_submit_sun_flare(const XwaSnapshot* snap, const FlSceneLighting* lighting) {
	if (!snap->suns.procedural_flare) {
		return;
	}
	const XwaRemasterDirLight* key =
		fl_key_light(lighting->lights, lighting->light_count, lighting->key_index);
	if (!key) {
		return;
	}
	float tint[3] = { 1.0f, 1.0f, 1.0f };
	float tangent = 0.0f;
	if (lighting->environment_lights) {
		if (!lighting->environment || lighting->key_index < 0 ||
			(uint32_t)lighting->key_index >= lighting->environment->light_count) {
			return;
		}
		const AeronEnvironmentLight* light = &lighting->environment->lights[lighting->key_index];
		if (!light->lens_flare) {
			return;
		}
		/* The light's colour with its brightest channel at 1. */
		const float* color = light->color_linear;
		const float peak = fmaxf(color[0], fmaxf(color[1], color[2]));
		for (int c = 0; c < 3 && peak > 0.0f; c++) {
			tint[c] = XwaRemaster_LinearToSrgb(color[c] / peak);
		}
		tangent = tanf(light->angular_radius_degrees * (3.14159265f / 180.0f));
		XwaRemasterSuns_SetFlare(s.scene, s.camera_local, light->direction_world, tangent, tint,
								 snap->suns.flare_intensity);
		return;
	}
	const int16_t source = key->source_backdrop_index;
	if (source < 0 || (uint32_t)source >= snap->backdrop_count ||
		!fl_backdrop_is_sun(&snap->backdrops[source])) {
		return;
	}
	float corners[8][4][3];
	float uvs[8][4][2];
	uint32_t count = 0;
	for (uint32_t i = 0; i < s.bd_count && count < 8; i++) {
		const FlBackdropQuad* q = &s.bd_quads[i];
		if (q->sun != source) {
			continue;
		}
		memcpy(corners[count], q->corners, sizeof corners[0]);
		memcpy(uvs[count], q->uvs, sizeof uvs[0]);
		if (count == 0 && q->ref.has_light) {
			memcpy(tint, q->ref.light, sizeof tint);
		}
		count++;
	}
	if (!XwaRemasterSuns_StarDiscRadius((const float (*)[4][3])corners, (const float (*)[4][2])uvs, count,
										&tangent)) {
		return;
	}
	XwaRemasterSuns_SetFlare(s.scene, s.camera_local, snap->backdrops[source].world_dir, tangent, tint,
							 snap->suns.flare_intensity);
}

static void fl_mb_reset_timing(uint64_t host_time_us) {
	s.mb_pose_host_us = host_time_us;
	s.mb_velocity_span_us = 0;
	s.mb_pose_host_valid = 1;
}

/* The simulation moves in whole game ticks, so a host frame can carry up
 * to 4 ms more or less game time than it lasted. Dividing the movement by
 * the host time would then make the blur too long or too short, by several
 * times on a short frame. Game time is the time the movement covers. */
static void fl_mb_record_pose(uint64_t host_time_us, uint32_t game_ticks) {
	if (game_ticks > 0) {
		s.mb_velocity_span_us = (uint64_t)game_ticks * FL_MB_GAME_TICK_US;
	} else {
		s.mb_velocity_span_us =
			s.mb_pose_host_valid && host_time_us > s.mb_pose_host_us ? host_time_us - s.mb_pose_host_us : 0;
	}
	s.mb_pose_host_us = host_time_us;
	s.mb_pose_host_valid = 1;
}

static float fl_mb_logical_frame_shutter(void) {
	if (s.mb_velocity_span_us == 0) {
		return 0.0f;
	}
	return (float)((double)s.mb_shutter * (double)FL_MB_REFERENCE_FRAME_US / (double)s.mb_velocity_span_us);
}

static AeronTexture* fl_render_map(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
								   XwaRemasterAssets* assets, const XwaRemasterFlightView* view,
								   uint64_t host_time_us, int direct_present) {
	if (!snap->flight_map.active)
		return NULL;
	s.snap = snap;
	s.map_present = 1;
	s.mb_enabled = 0;
	s.table_pool.used = 0;
	s.anim_pool.used = 0;
	fl_prepare_energy_states(snap);
	s.bb_count = 0;
	s.glow_ok = XwaRemasterAssets_FlightAtlasFrame(assets, 1000, 0, &s.glow_ref);
	fl_set_temporal(host_time_us, 0);

	XwaRemasterEffectView effect_view;
	XwaRemasterEffectView_Main(&effect_view, view, s.crows);
	XwaRemasterParticles_Prepare(s.particles, cmd, snap, NULL, assets, &effect_view, NULL);
	XwaRemasterTrails_Prepare(s.trails, cmd, snap, assets, &effect_view);

	fl_set_animation_time(snap);
	if (!fl_begin_scene(&view->camera)) {
		s.snap = NULL;
		return NULL;
	}
	AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, NULL, NULL);
	AeronScene_SetPost(s.scene, NULL);
	AeronScene_SetMotionContext(s.scene, NULL, NULL, 1);
	AeronScene_SetDirectionalShadow(s.scene, NULL);

	FlMapSubmitContext submit_context = { .cmd = cmd, .assets = assets };
	/* The map's overlays use the classic palette colours. The present pass
	 * multiplies the scene by the map's fixed exposure, so they are drawn
	 * at 1 / exposure to keep those colours. */
	if (!XwaRemasterFlightMap_Prepare(cmd, s.scene, snap, assets, view, s.crows,
									  1.0f / XwaRemasterFlight_Exposure(), fl_map_submit_object,
									  &submit_context)) {
		s.snap = NULL;
		return NULL;
	}
	XwaRemasterParticles_SubmitRegion(s.particles, s.scene, snap, snap->flight_camera.region);
	XwaRemasterTrails_SubmitRegion(s.trails, s.scene, snap->flight_camera.region);
	fl_submit_billboards();
	XwaRemasterDirLight lights[XWA_SNAP_MAX_DIR_LIGHTS];
	const uint32_t light_count =
		XwaRemaster_ReadGameDirLights(snap->dir_lights, snap->dir_light_count, lights);
	XwaShipAmbientCube ambient;
	XwaRemasterShip_GetAmbientCube(&ambient);
	float* const faces[6] = { ambient.pos_x, ambient.neg_x, ambient.pos_y,
							  ambient.neg_y, ambient.pos_z, ambient.neg_z };
	for (int f = 0; f < 6; f++) {
		for (int c = 0; c < 3; c++) {
			faces[f][c] = fmaxf(faces[f][c], s.map_ambient_min[c]);
		}
	}
	XwaRemasterShip_SetPbrEnv(s.scene, lights, light_count, -1, NULL, s.camera_local, NULL, &ambient, NULL,
							  -1.0f);
	if (!AeronScene_Render(s.scene, cmd)) {
		s.snap = NULL;
		return NULL;
	}
	s.snap = NULL;
	return fl_present(cmd, direct_present);
}

/* ---- environment capture support ------------------------------------- */

void XwaRemasterFlight_CaptureSkyInfo(XwaFlightCaptureSky* out) {
	if (!out) {
		return;
	}
	memset(out, 0, sizeof *out);
	out->exposure = s.sky_exposure;
	out->star_brightness = s.star_brightness;
	out->star_density = s.star_density;
	out->star_grid = s.star_grid;
}

int XwaRemasterFlight_CaptureCamera(const float right[3], const float down[3], const float forward[3],
									float half_tan, int size, AeronSceneCamera* camera, float crows[9]) {
	if (!right || !down || !forward || !camera || !crows || !(half_tan > 0.0f) || size <= 0) {
		return 0;
	}
	const float rows[9] = { right[0], right[1],   right[2],   down[0],   down[1],
							down[2],  forward[0], forward[1], forward[2] };
	memset(camera, 0, sizeof *camera);
	fl_rows_to_quat(rows, camera->ori);
	const float length = sqrtf(camera->ori[0] * camera->ori[0] + camera->ori[1] * camera->ori[1] +
							   camera->ori[2] * camera->ori[2] + camera->ori[3] * camera->ori[3]);
	if (!(length > 0.5f) || !isfinite(length)) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		camera->ori[i] /= length;
	}
	camera->h_half_rad = atanf(half_tan);
	camera->v_half_rad = camera->h_half_rad;
	camera->near_z = FL_NEAR_Z;
	camera->viewport = (AeronRectI) { 0, 0, size, size };
	/* Backdrop geometry and the scene projection agree when both come from
	 * the same quaternion (see s.crows). */
	fl_quat_to_mat3(camera->ori, crows);
	return 1;
}

int XwaRemasterFlight_CaptureSetSky(AeronScene3D* scene, XwaRemasterSkyStars* stars, const XwaSnapshot* snap,
									const AeronPreparedEnvironment* sky, float star_upscale) {
	if (!scene || !snap || (!sky && !stars)) {
		return 0;
	}
	if (sky) {
		/* The overlay is not part of the captured sky. */
		AeronPreparedEnvironment without_overlay;
		XwaRemasterEnvironment_ScaleIntensity(sky, &without_overlay);
		without_overlay.overlay = NULL;
		AeronScene_SetSkyEnvironment(scene, &without_overlay);
		return 1;
	}
	const XwaRemasterSkyStarsParams params = {
		.exposure = s.sky_exposure,
		.brightness = s.star_brightness,
		.density = s.star_density,
		.grid_n = s.star_grid,
		.core_radius_px = s.star_core_px * star_upscale,
		.feather_px = s.star_feather * star_upscale,
		.pixel_pitch_px = star_upscale,
		.flare_strength = s.star_flare,
		.game_time_ticks = (uint32_t)snap->game_time_ticks,
	};
	if (!XwaRemasterSkyStars_Prepare(stars, scene, fl_world_to_cube, &params)) {
		return 0;
	}
	AeronScene_SetPassHook(scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, XwaRemasterSkyStars_Draw, stars);
	return 1;
}

int XwaRemasterFlight_CaptureBackdropHidden(const XwaSnapshot* snap, XwaRemasterAssets* assets,
											uint32_t index, const XwaEnvironmentBackdropRule* hide) {
	if (!snap || index >= snap->backdrop_count) {
		return 1;
	}
	const XwaBackdrop* b = &snap->backdrops[index];
	return b->hidden || fl_backdrop_hidden(assets, hide, b->object_type, (b->flags & 1u) ? b->frame : 1);
}

uint32_t XwaRemasterFlight_CaptureBackdrops(AeronScene3D* scene, const float crows[9],
											const XwaSnapshot* snap, XwaRemasterAssets* assets, int filter,
											const XwaEnvironmentBackdropRule* hide, float* out_max_tan) {
	static FlBackdropQuad quads[FL_MAX_BACKDROP_QUADS];
	if (out_max_tan) {
		*out_max_tan = 0.0f;
	}
	if (!crows || !snap || !assets) {
		return 0;
	}
	const uint32_t count =
		fl_derive_backdrops(snap, crows, assets, filter, 1, hide, quads, FL_MAX_BACKDROP_QUADS);
	static const float origin[3] = { 0.0f, 0.0f, 0.0f };
	const FlQuadTarget target = { scene, crows, origin };
	float max_tan = 0.0f;
	for (uint32_t i = 0; i < count; i++) {
		for (int c = 0; c < 4; c++) {
			const float* v = quads[i].corners[c];
			if (!(v[2] > 0.0f)) {
				max_tan = -1.0f;
			} else if (max_tan >= 0.0f) {
				max_tan = fmaxf(max_tan, fmaxf(fabsf(v[0]), fabsf(v[1])) / v[2]);
			}
		}
		if (scene) {
			fl_submit_view_quad_to(&target, AERON_SCENE_BILLBOARD_STAGE_SKY, quads[i].corners, quads[i].uvs,
								   &quads[i].ref, 0xffffffffu, NULL, 0.0f, NULL,
								   XwaRemaster_ClassicRadiance());
		}
	}
	if (out_max_tan) {
		*out_max_tan = max_tan;
	}
	return count;
}

AeronTexture* XwaRemasterFlight_Render(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
									   XwaRemasterAssets* assets, int target_w, int target_h,
									   int direct_present) {
	if (!cmd || !snap || !assets || !snap->flight_camera_valid) {
		return NULL;
	}
	const XwaFlightCamera* cam = &snap->flight_camera;
	if (!fl_ensure(target_w, target_h)) {
		return NULL;
	}
	/* Starts a new frame for fl_mesh_for_name. 0 is never a frame, so the
	 * empty entries never match. */
	if (++s_mesh_memo_frame == 0) {
		s_mesh_memo_frame = 1;
	}
	const uint64_t host_time_us = XwaTime_GetElapsedUs();
	/* Get the hyperspace moment first, because its camera shake or jolt turns
	 * the view that everything else uses. The map view doesn't shake. */
	XwaRemasterHyperspaceMoment hyperspace_moment;
	int hyperspace = 0;
	if (!cam->map_mode) {
		if (cam->hyperspace_phase) {
			hyperspace = fl_real_hyperspace_moment(snap, &hyperspace_moment);
		} else if (s.hyperspace_preview != XWA_FLIGHT_HYPERSPACE_PREVIEW_OFF) {
			hyperspace = fl_preview_hyperspace_moment(snap, host_time_us, &hyperspace_moment);
		}
	}
	float view_rotation[9];
	const int view_rotated = hyperspace && fl_hyperspace_view_rotation(&hyperspace_moment, view_rotation);
	/* The previous frame's rotation, for motion vectors. In a real jump, it is
	 * computed from the previous snapshot. The preview runs on its own clock,
	 * so it keeps the rotation of its last frame. When the preview's phase
	 * changes, the previous rotation is set to the current one, so the cut
	 * doesn't show up as camera motion. */
	float previous_view_rotation[9];
	const float* previous_rotation = NULL;
	int preview_rotation_changed = 0;
	const XwaSnapshot* prev = XwaSnapshot_Previous();
	if (hyperspace && !cam->hyperspace_phase) {
		if (s.hyperspace_preview_rotation_valid &&
			s.hyperspace_preview_rotation_phase == hyperspace_moment.phase) {
			memcpy(previous_view_rotation, s.hyperspace_preview_rotation, sizeof previous_view_rotation);
		} else {
			memcpy(previous_view_rotation, view_rotation, sizeof previous_view_rotation);
		}
		previous_rotation = previous_view_rotation;
		preview_rotation_changed = memcmp(previous_view_rotation, view_rotation, sizeof view_rotation) != 0;
		memcpy(s.hyperspace_preview_rotation, view_rotation, sizeof view_rotation);
		s.hyperspace_preview_rotation_phase = hyperspace_moment.phase;
		s.hyperspace_preview_rotation_valid = 1;
	} else {
		s.hyperspace_preview_rotation_valid = 0;
		XwaRemasterHyperspaceMoment previous_moment;
		if (prev && prev->flight_camera_valid && prev->flight_camera.hyperspace_phase &&
			!prev->flight_camera.map_mode && fl_real_hyperspace_moment(prev, &previous_moment) &&
			fl_hyperspace_view_rotation(&previous_moment, previous_view_rotation)) {
			previous_rotation = previous_view_rotation;
		}
	}

	XwaRemasterFlightView flight_view;
	if (!fl_build_view_at_origin(cam, cam->world_pos, s.rt_w, s.rt_h, view_rotated ? view_rotation : NULL,
								 &flight_view)) {
		return NULL;
	}
	memcpy(s.origin_world, flight_view.origin_world, sizeof s.origin_world);
	memcpy(s.camera_local, flight_view.camera.pos, sizeof s.camera_local);
	/* Dynamic Cockpit: decided before the HUD prepares its lists (which
	 * route through it), captured after. */
	XwaRemasterDc_BeginFrame(snap,
							 !cam->map_mode && fl_cockpit_drawn(snap, cam)
								 ? XwaRemasterShip_MeshForName(snap->cockpit.model_name)
								 : NULL,
							 s.rt_w, s.rt_h);
	XwaRemasterHud_PrepareFrame(cmd, snap, assets, &flight_view, s.rt_w, s.rt_h);
	XwaRemasterDc_Prepare(cmd, snap, s.rt_w, s.rt_h);
	fl_quat_to_mat3(flight_view.camera.ori, s.crows);
	if (cam->map_mode) {
		s.suppress_hud = 0;
		return fl_render_map(cmd, snap, assets, &flight_view, host_time_us, direct_present);
	}
	s.map_present = 0;
	/* One environment selection per frame, before any sky, backdrop,
	 * directional or lens-flare decision. */
	XwaRemasterEnvironment_Resolve(snap, &s.environment);
	s.exposure_hold = 0;
	s.hyperspace_world_pending = 0;
	s.hyperspace_in_world = 0;
	/* The tunnel has its own scene. The entry and the exit are drawn over the
	 * normal flight scene, so it stays on screen until the white-out covers
	 * it and comes back as the white fades. */
	if (hyperspace && hyperspace_moment.phase == XWA_HYPERSPACE_TUNNEL) {
		s.suppress_hud = !cam->hyperspace_phase;
		return fl_render_hyperspace(cmd, snap, assets, &flight_view, host_time_us, direct_present,
									&hyperspace_moment);
	}
	if (hyperspace) {
		s.hyperspace_world_moment = hyperspace_moment;
		s.hyperspace_world_pending = 1;
	}
	s.suppress_hud = 0;
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
	XwaRemasterFlightView previous_flight_view;
	const int mb_on = s.mb_quality > 0 || s.fsr_mode != AERON_TEMPORAL_OFF;
	s.mb_enabled = 0;
	int mb_regen = 0;
	int paused = 0;
	if (mb_on && prev && prev->flight_camera_valid) {
		const XwaFlightCamera* pc = &prev->flight_camera;
		if (pc->region == cam->region && pc->hyperspace_phase != XWA_HYPERSPACE_TUNNEL && !pc->map_mode &&
			pc->in_hangar == cam->in_hangar && pc->external == cam->external &&
			pc->film_overlay == cam->film_overlay && snap->game_time_ticks >= prev->game_time_ticks) {
			fl_build_view_at_origin(pc, s.origin_world, s.rt_w, s.rt_h, previous_rotation,
									&previous_flight_view);
			memcpy(s.mb_prev_vp, previous_flight_view.view_proj, sizeof s.mb_prev_vp);
			memcpy(s.mb_prev_ori, previous_flight_view.camera.ori, sizeof s.mb_prev_ori);
			s.mb_enabled = 1;
			/* Hold velocity while a paused snapshot is truly unchanged. Camera
			 * panning and cockpit articulation can still change with frozen sim
			 * time, and must regenerate velocity for both temporal consumers. */
			paused = snap->game_time_ticks == prev->game_time_ticks;
			mb_regen = !paused || fl_temporal_pose_changed(snap, prev) || preview_rotation_changed;
			/* Prev lookup by stable identity: objectSignature is unique
			 * per spawn (slot reuse bumps it), so signature + slot pins
			 * the same live object across the pair. */
			if (fl_reserve_prev_index(snap->flight_object_count)) {
				for (uint32_t i = 0; i < snap->flight_object_count; i++) {
					const XwaFlightObject* f = &snap->flight_objects[i];
					const XwaFlightObject* pf = XwaSnapshot_FindFlightObject(prev, f->slot, f->signature);
					s.mb_prev_index[i] = pf ? (int32_t)(pf - prev->flight_objects) : -1;
				}
			} else {
				s.mb_enabled = 0;
			}
		}
	}
	if (!s.mb_enabled || !s.mb_pose_host_valid) {
		fl_mb_reset_timing(host_time_us);
	} else if (mb_regen) {
		fl_mb_record_pose(host_time_us, (uint32_t)(snap->game_time_ticks - prev->game_time_ticks));
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
	 * sources (glow lights and cockpit lights append during/after the
	 * walk). */
	fl_derive_billboards(snap, assets, billboard_history_cam, billboard_history_crows);
	/* An environment with a sky replaces all of these together: its sky,
	 * suns, lights and flares replace the starfield, the recovered
	 * backdrop sprites, the directional lights derived from backdrops and
	 * the backdrop sun flares. With `backdrops: keep`, only the starfield
	 * is replaced: the backdrops are drawn over the sky, minus the hidden
	 * images, and keep their flares and lights, unless the region removes
	 * the lights of hidden backdrops (fl_remove_hidden_backdrop_lights). An
	 * environment without a sky keeps the starfield, the sprites and the
	 * flares. */
	const AeronPreparedEnvironment* region_environment =
		s.environment.mode == XWA_ENV_MODE_AUTHORED ? s.environment.prepared : NULL;
	const AeronPreparedEnvironment* sky_environment =
		region_environment && region_environment->radiance ? region_environment : NULL;
	const int keep_backdrops = fl_environment_keeps_backdrops();
	const AeronPreparedEnvironment* replacing_sky = keep_backdrops ? NULL : sky_environment;
	if (keep_backdrops) {
		fl_log_kept_backdrops(snap, assets, s.environment.backdrops);
	}
	fl_derive_lens_flares(snap, assets, &flight_view, replacing_sky);
	fl_begin_point_lights(&flight_view);
	fl_derive_point_lights(snap);
	s.bd_count = replacing_sky ? 0
							   : fl_derive_backdrops(snap, s.crows, assets, FL_BACKDROPS_ALL, 0,
													 keep_backdrops ? s.environment.backdrops : NULL,
													 s.bd_quads, FL_MAX_BACKDROP_QUADS);
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

	XwaRemasterCapturedLighting_Update(cmd, snap, assets, &s.environment);
	FlSceneLighting scene_lighting;
	fl_build_scene_lighting(snap, assets, cam, &scene_lighting);
	fl_note_hyperspace_sky(sky_environment, keep_backdrops, &scene_lighting);
	float shadow_light_dir[3];
	/* Inside the Death Star, only the lights of a region environment can
	 * cast shadows, when the environment allows it. Without an environment,
	 * there are no directional lights there (fl_build_scene_lighting). */
	const int shadows_active = (!cam->death_star_mode || scene_lighting.environment_lights) &&
							   s.shadows.enabled && scene_lighting.shadow_allowed &&
							   fl_key_directional_light(scene_lighting.lights, scene_lighting.light_count,
														scene_lighting.key_index, shadow_light_dir);
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
			if (pc->hyperspace_phase == XWA_HYPERSPACE_TUNNEL)
				motion_reset_reasons |= FL_FSR_RESET_PREVIOUS_HYPERSPACE;
			if (pc->map_mode)
				motion_reset_reasons |= FL_FSR_RESET_PREVIOUS_MAP;
			if (pc->in_hangar != cam->in_hangar)
				motion_reset_reasons |= FL_FSR_RESET_HANGAR_CHANGE;
			if (pc->external != cam->external)
				motion_reset_reasons |= FL_FSR_RESET_EXTERNAL_VIEW_CHANGE;
			if (pc->film_overlay != cam->film_overlay)
				motion_reset_reasons |= FL_FSR_RESET_FILM_OVERLAY_CHANGE;
			if (snap->game_time_ticks < prev->game_time_ticks)
				motion_reset_reasons |= FL_FSR_RESET_GAME_TIME_ROLLBACK;
		}
		fl_request_fsr_reset(motion_reset_reasons ? motion_reset_reasons
												  : FL_FSR_RESET_MOTION_CONTEXT_UNKNOWN);
	}
	fl_set_temporal(host_time_us, 1);
	fl_set_animation_time(snap);
	/* Before the scene begins: it sets the exposure hold. */
	s.hyperspace_in_world = fl_prepare_world_hyperspace(cmd, snap, &flight_view, cockpit_anchor);
	if (!fl_begin_scene(&flight_view.camera)) {
		Aeron_CommandBufferSetFailure(cmd, "Flight scene initialization failed");
		s.snap = NULL;
		return NULL;
	}
	XwaRemasterDc_ApplyToScene(s.scene);
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
			.light_angular_radius_degrees = scene_lighting.shadow_angular_radius_degrees,
			.max_filter_radius = s.shadows.max_filter_radius,
			.pcss_min_filter_radius = s.shadows.pcss_min_filter_radius,
			.normal_bias_texels = s.shadows.normal_bias_texels,
			.depth_bias_texels = s.shadows.depth_bias_texels,
			.transition_fraction = s.shadows.transition_fraction,
			.distance_fade_fraction = s.shadows.distance_fade_fraction,
			.debug_cascades = s.shadows.debug_cascades,
			.receiver_local_near_distance =
				s.shadows.receiver_local_near_distance_m * AERON_OPT_UNITS_PER_METER,
			.debug_atlas = s.shadow_debug_atlas,
			.debug_atlas_cascade = s.shadow_debug_atlas_cascade,
		};
		AeronScene_SetDirectionalShadow(s.scene, &shadow);
	}

	/* The sky: the region's environment, or the procedural starfield that
	 * replaces the classic CPU pixel starfield (FlightStarfield_Render).
	 * There is no starfield in Death Star mode. The sky is drawn under the
	 * SKY backdrops and all meshes. World->cube basis: XWA world XY is the
	 * horizontal plane, +Z up (starfield elevation bins measure from XY
	 * toward +-Z; backdrop strips extend along Z): world X -> cube X, world
	 * Z -> cube +Y (up), world Y -> cube -Z (forward). The starfield's
	 * twinkle advances with the simulation clock (game_time_ticks), so it
	 * is deterministic and pauses with the game. */
	if (sky_environment) {
		AeronPreparedEnvironment scaled_sky;
		AeronScene_SetSkyEnvironment(s.scene,
									 XwaRemasterEnvironment_ScaleIntensity(sky_environment, &scaled_sky));
		/* Kept backdrops bring their own suns. */
		for (uint32_t i = 0; !keep_backdrops && i < sky_environment->light_count; i++) {
			AeronScene_AddSkySun(s.scene, &sky_environment->lights[i]);
		}
	} else if (!cam->death_star_mode) {
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
			.game_time_ticks = (uint32_t)snap->game_time_ticks,
		};
		if (!XwaRemasterSkyStars_Prepare(s.sky_stars, s.scene, fl_world_to_cube, &stars)) {
			Aeron_CommandBufferSetFailure(cmd, "Procedural starfield preparation failed");
			s.snap = NULL;
			return NULL;
		}
		s.before_opaque_mode = FL_BEFORE_OPAQUE_SKY_STARS;
		AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, fl_draw_before_opaque, NULL);
	}


	/* Backdrops — classic draws them first with depth compare and write
	 * both OFF (states 0x612/0x2612); the SKY stage's far-depth GE test
	 * against the cleared / prepass buffer reproduces that (behind all
	 * geometry, visible elsewhere). */
	const float backdrop_radiance = XwaRemaster_ClassicRadiance();
	for (uint32_t i = 0; i < s.bd_count; i++) {
		if (snap->suns.procedural_sun && s.bd_quads[i].sun >= 0) {
			fl_submit_star_quad(&s.bd_quads[i], snap, backdrop_radiance * snap->suns.brightness);
			continue;
		}
		fl_submit_view_quad_ex(AERON_SCENE_BILLBOARD_STAGE_SKY, s.bd_quads[i].corners, s.bd_quads[i].uvs,
							   &s.bd_quads[i].ref, 0xffffffffu, NULL, 0.0f, NULL, backdrop_radiance);
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
									.ssao_quality = s.ssao.ssao_quality,
									.ssao_intensity = s.ssao.ssao_intensity,
									.ssao_power = s.ssao.ssao_power,
									.ssao_radius_view = s.ssao.ssao_radius_view,
									.ssao_bias_view = s.ssao.ssao_bias_view,
									.ssao_direct = s.ssao.ssao_direct,
									.ssao_debug_viz = s.ssao.ssao_debug_viz,
									.ssao_min_screen_frac = s.ssao.ssao_min_screen_frac,
									.ssao_max_screen_frac = s.ssao.ssao_max_screen_frac,
									.ssao_sample_jitter = s.ssao.ssao_sample_jitter,
									.mb_quality = mb_on ? s.mb_quality : 0,
									.mb_shutter = mb_shutter,
									.mb_camera_blur = s.mb_camera_blur,
									.mb_velocity_viz = s.mb_velocity_viz,
								});
	AeronScene_SetMotionContext(s.scene, s.mb_enabled ? s.mb_prev_vp : NULL, s.mb_enabled ? s.mb_prev_ori : NULL,
								mb_regen);

	/* The lit PBR env is submitted AFTER the instance walk (frame
	 * uniforms are copied at submit and bound inside Render) so the
	 * walk can contribute the engine-glow point lights. */
	const XwaShipAoParams ao = {
		.intensity = s.ssao.ssao_intensity,
		.power = s.ssao.ssao_power,
		.direct = s.ssao.ssao_direct,
	};
	const int ao_on = s.ssao.ssao_quality > 0 && s.ssao.ssao_intensity > 0.0f;

	s.bolt_count = 0;
	s.table_pool.used = 0;
	s.anim_pool.used = 0;
	fl_prepare_energy_states(snap);
	fl_place_probe_sets(cmd, snap, &flight_view);

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
		if (!fl_object_in_render_region(f, cam->region) || f->genus == XWA_SNAP_GENUS_EXPLOSION) {
			continue; /* other regions; explosions are billboards */
		}
		/* Transforms derive for every in-region object — the cockpit
		 * anchors to one even when its own model is suppressed. */
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
		const int is_bolt = fl_object_is_projectile(f);
		float bw[9];
		float object_local[3];
		float model_matrix[16];
		fl_object_pose(f, cam->world_pos, is_bolt, bw, object_local, model_matrix);
		if (fl_is_death_star_beam(snap, f)) {
			fl_apply_death_star_beam_scale(snap, model_matrix);
		}
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
			FlPreparedMesh prepared;
			if (!fl_prepare_object_mesh(f, NULL, &prepared)) {
				continue; /* never fall through to a whole-model draw */
			}
			AeronSceneMeshInstance inst;
			memset(&inst, 0, sizeof inst);
			inst.mesh = prepared.mesh;
			memcpy(inst.transform, model_matrix, sizeof model_matrix);
			inst.variant = f->node_switch; /* markings survive the detach copy */
			XwaRemasterSkins_CheckMarkings(inst.mesh, inst.variant);
			inst.mesh_table = prepared.table;
			inst.anim = prepared.anim;
			inst.cull_mode = AERON_CULL_BACK;
			fl_instance_motion(&inst, f, i, /*is_bolt=*/0);
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
		const char* model_name_override = NULL;
		if (cam->external && (int32_t)f->slot == cam->player_obj_idx &&
			snap->cockpit.exterior_name[0] != '\0') {
			model_name_override = snap->cockpit.exterior_name;
		}
		FlPreparedMesh prepared;
		if (!fl_prepare_object_mesh(f, model_name_override, &prepared)) {
			continue;
		}

		AeronSceneMeshInstance inst;
		memset(&inst, 0, sizeof inst);
		inst.mesh = prepared.mesh;
		memcpy(inst.transform, model_matrix, sizeof model_matrix);
		inst.variant = f->node_switch;
		inst.hidden_tag_groups = XwaRemasterShip_HiddenTagGroups(prepared.mesh, f);
		inst.probe_set_owner = fl_probe_set_owner(i);
		XwaRemasterSkins_CheckMarkings(inst.mesh, inst.variant);
		if (scene_lighting.hangar_override && (int32_t)f->slot == cam->hangar_launch_ref_obj_idx) {
			/* The enclosed backdrop contains the implied ceiling emitter.
			 * It receives object shadows but must not occlude that light. */
			inst.shadow_flags |= AERON_SCENE_INSTANCE_NO_CAST_SHADOW;
		}
		if (is_bolt) {
			inst.shadow_flags |= AERON_SCENE_INSTANCE_NO_CAST_SHADOW | AERON_SCENE_INSTANCE_NO_RECEIVE_SHADOW;
		}
		inst.cull_mode = AERON_CULL_BACK;
		const XwaFlightObject* previous_f = fl_instance_motion(&inst, f, i, is_bolt);
		if (fl_is_death_star_beam(snap, f) && s.mb_enabled) {
			if (previous_f && fl_is_death_star_beam(s.mb_prev_snap, previous_f)) {
				float previous_local[3];
				AeronWorld_LocalI32(s.origin_world, previous_f->world_pos, previous_local);
				fl_model_matrix(bw, previous_local, inst.prev_transform);
				fl_apply_death_star_beam_scale(s.mb_prev_snap, inst.prev_transform);
			} else {
				inst.zero_velocity = 1;
			}
		}
		/* Rotary-mesh articulation (cranes, radar dishes, droid arms)
		 * from the captured craft state — tables borrow pool slots for
		 * the frame (the scene keeps the pointer until Render). */
		inst.mesh_table = prepared.table;
		inst.anim = prepared.anim;
		AeronSceneMeshTable* prev_tb = fl_table_peek();
		if (prev_tb && XwaRemasterShip_BuildPreviousMeshTable(prepared.mesh, f, previous_f, prev_tb)) {
			inst.prev_mesh_table = prev_tb;
			s.table_pool.used++;
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
			fl_push_bolt(&inst);
			/* A bolt model with lights is lit by its first light that
			 * shines, for the colour it shows, instead of the stock light
			 * (fl_derive_point_lights). */
			fl_add_model_lights(prepared.mesh, model_matrix, inst.mesh_table, inst.variant,
								inst.hidden_tag_groups, 0, 0, 1);
		} else {
			AeronScene_AddMeshInstance(s.scene, &inst);
			fl_add_model_lights(prepared.mesh, model_matrix, inst.mesh_table, inst.variant,
								inst.hidden_tag_groups, 0, inst.probe_set_owner != 0, 0);
			XwaRemasterGlowMarks_SubmitObject(s.scene, cmd, assets, snap, f, prepared.mesh, model_matrix,
											  inst.mesh_table, s.glow_mark_emissive_strength,
											  s.glow_mark_emissive_strength, 1);
			/* State-derived engine glows for this craft (classic scale
			 * gates — no craft / dead subsystems — return 0 and skip). */
			if (s.glow_ok) {
				XwaRemasterShip_SubmitEngineGlows(
					s.scene, prepared.mesh, model_matrix, AERON_OPT_UNITS_PER_METER, inst.mesh_table,
					f->eg_knockout_mask, f->eg_hidden_mask, XwaRemasterShip_EngineGlowScale(f), s.crows,
					s.camera_local, &s.glow_ref);
			}
			/* Capital engine glows double as point lights (the classic
			 * modelIndex branch: raw dims over 2000 only). */
			if (s.plight.enabled && f->has_craft) {
				const uint32_t remaining = FL_MAX_PL_CANDIDATES - s.pl_cand_count;
				uint32_t dropped = 0;
				s.pl_cand_count += XwaRemasterShip_CollectEngineGlowPointLights(
					prepared.mesh, model_matrix, inst.mesh_table, f,
					remaining ? &s.pl_cand[s.pl_cand_count] : NULL, remaining, &dropped);
				s.pl_cand_dropped += dropped;
			}
		}
	}

	/* Projectile instances submit after every ship. The scene's transparent
	 * pass sorts the blended ranges from far to near, so the order of
	 * submission doesn't change how the bolts composite; it only keeps the
	 * cockpit last. */
	for (uint32_t bi = 0; bi < s.bolt_count; bi++) {
		AeronScene_AddMeshInstance(s.scene, &s.bolts[bi]);
	}

	/* Cockpit lights and headlight (player basis known after
	 * the walk), then the finalized candidates into the light pool
	 * (submitted with the cockpit's lights) + the lit PBR env. */
	if (player_found && player_f) {
		fl_add_cockpit_lights(snap, player_bw, player_local);
		fl_add_headlight(snap, player_bw, player_local);
	}
	fl_finalize_point_lights();
	if (!fl_configure_clustered_lights(s.scene)) {
		Aeron_CommandBufferSetFailure(cmd, "Flight clustered-light configuration failed");
		s.snap = NULL;
		return NULL;
	}
	/* The configured ceiling light is baked into the hangar's probes. Its
	 * shadow applies only where the probes apply, with the strength that
	 * the probe set gives. */
	const AeronProbeSet* hangar_probes = NULL;
	if (cam->in_hangar && scene_lighting.lights == &scene_lighting.hangar_key) {
		hangar_probes = fl_hangar_probe_set(snap);
	}
	/* The jump's streaks and the tunnel's fades add their light to the
	 * cockpit's ambient. */
	const XwaShipAmbientCube* scene_ambient = scene_lighting.ambient;
	XwaShipAmbientCube hyperspace_ambient;
	XwaRemasterHyperspaceLighting hyperspace_lighting;
	if (s.hyperspace_in_world && XwaRemasterHyperspace_GetPreparedLighting(s.hyperspace, &hyperspace_lighting)) {
		if (scene_ambient) {
			hyperspace_ambient = *scene_ambient;
		} else {
			XwaRemasterShip_GetAmbientCube(&hyperspace_ambient);
		}
		fl_add_ambient_cube(&hyperspace_ambient, &hyperspace_lighting.ambient_add);
		scene_ambient = &hyperspace_ambient;
	}
	XwaRemasterShip_SetPbrEnv(s.scene, scene_lighting.lights, scene_lighting.light_count,
							  scene_lighting.key_index, NULL, s.camera_local, ao_on ? &ao : NULL, scene_ambient,
							  NULL, hangar_probes ? hangar_probes->layout.key_light_shadow : -1.0f);
	if (scene_lighting.captured) {
		AeronScene_SetPbrEnvironment(s.scene, &scene_lighting.captured_environment);
	} else if (scene_lighting.environment) {
		AeronPreparedEnvironment scaled_environment;
		AeronScene_SetPbrPreparedEnvironment(
			s.scene, XwaRemasterEnvironment_ScaleIntensity(scene_lighting.environment, &scaled_environment));
	}

	/* Player cockpit (lit; classic runs the normal light setup). Gates
	 * mirror the classic draw sites: the hangar draws it whenever the
	 * view is internal; flight requires the cockpit toggle plus the
	 * seat's availability flag. Transform derived from STATE: the
	 * anchor object's basis, eye anchored at -(seat eye offset + camera
	 * pan / 16) rotated into eye space (rotation only — the classic
	 * g_cockpitViewActive override). Turret seats rotate the cockpit's
	 * rotary meshes by the captured aim. */
	if (fl_cockpit_drawn(snap, cam) && cockpit_anchor) {
		AeronSceneMesh* cockpit_mesh = XwaRemasterShip_MeshForName(snap->cockpit.model_name);
		if (cockpit_mesh) {
			float m[16];
			fl_cockpit_model_matrix(&snap->cockpit, cam->sway, cockpit_anchor, s.crows, s.camera_local, m);
			AeronSceneMeshInstance inst;
			memset(&inst, 0, sizeof inst);
			inst.mesh = cockpit_mesh;
			inst.variant = cockpit_anchor->node_switch;
			XwaRemasterSkins_CheckMarkings(inst.mesh, inst.variant);
			memcpy(inst.transform, m, sizeof m);
			memcpy(inst.prev_transform, m, sizeof m);
			if (cockpit_history_valid &&
				fl_cockpit_model_matrix(&prev->cockpit, prev->flight_camera.sway, previous_cockpit_anchor,
										previous_crows, previous_flight_view.camera.pos,
										inst.prev_transform)) {
				inst.zero_velocity = 0;
			} else {
				/* A history reset makes the zero-vector fallback correct for the
				 * first frame after a cockpit/view discontinuity. */
				inst.zero_velocity = 1;
			}
			inst.cull_mode = AERON_CULL_BACK;
			/* The cockpit shadows itself in its own map (the hidden exterior
			 * already casts on the rest of the scene), with both faces: its
			 * walls are single-sided, facing in. */
			inst.shadow_flags = AERON_SCENE_INSTANCE_USE_RECEIVER_LOCAL_SHADOW |
								AERON_SCENE_INSTANCE_CAST_RECEIVER_LOCAL_ONLY |
								AERON_SCENE_INSTANCE_SHADOW_DOUBLE_SIDED;
			/* Holograms and transparent screens carry the cockpit's motion
			 * vectors, so motion blur does not smear them with the view
			 * behind. */
			inst.velocity_stamp_screens = 1;
			/* They write no depth, so the hyperspace scene warp would move
			 * them with the sky behind. It draws them over the warped scene
			 * instead. */
			inst.screens_to_layer = s.hyperspace_in_world ? 1 : 0;
			inst.anim = player_f ? fl_object_anim(player_f) : NULL;
			AeronSceneMeshTable* tb = fl_table_peek();
			XwaRemasterCockpitControls controls;
			XwaRemasterShip_CockpitControls(snap, cockpit_anchor, &controls);
			if (tb && XwaRemasterShip_BuildCockpitMeshTable(cockpit_mesh, &snap->cockpit, cockpit_anchor,
															&controls, tb)) {
				inst.mesh_table = tb;
				s.table_pool.used++;
			}
			if (cockpit_history_valid) {
				AeronSceneMeshTable* prev_tb = fl_table_peek();
				XwaRemasterCockpitControls previous_controls;
				XwaRemasterShip_CockpitControls(prev, previous_cockpit_anchor, &previous_controls);
				if (prev_tb && XwaRemasterShip_BuildPreviousCockpitMeshTable(
								   cockpit_mesh, &snap->cockpit, cockpit_anchor, &controls, &prev->cockpit,
								   previous_cockpit_anchor, &previous_controls, prev_tb)) {
					inst.prev_mesh_table = prev_tb;
					s.table_pool.used++;
				}
			}
			AeronScene_AddMeshInstance(s.scene, &inst);
			fl_add_model_lights(cockpit_mesh, m, inst.mesh_table, inst.variant, 0, 1, 0, 0);
			/* No blast marks on the cockpit (see fl_submit_hyperspace_cockpit). */
			if (player_f && fl_is_player_craft(snap, player_f->slot, player_f->signature) &&
				(s.cockpit_glow_mark_emissive_strength > 0.0f ||
				 s.cockpit_hit_flash_emissive_strength > 0.0f)) {
				XwaRemasterGlowMarks_SubmitObject(s.scene, cmd, assets, snap, player_f, cockpit_mesh, m,
												  inst.mesh_table, s.cockpit_glow_mark_emissive_strength,
												  s.cockpit_hit_flash_emissive_strength, 0);
			}
			/* Cockpit-model engine glows (classic: the cockpit glow set
			 * when internal + cockpit visible), scale/knockouts from
			 * the PLAYER craft's state. */
			if (s.glow_ok && player_f) {
				XwaRemasterShip_SubmitEngineGlows(
					s.scene, cockpit_mesh, m, AERON_OPT_UNITS_PER_METER, inst.mesh_table,
					player_f->eg_knockout_mask, player_f->eg_hidden_mask,
					XwaRemasterShip_EngineGlowScale(player_f), s.crows, s.camera_local, &s.glow_ref);
			}
		}
	}
	/* Every light is in the pool once the cockpit's are. */
	fl_submit_point_lights();

	/* Classic effects order: particles, persistent warhead trails, then
	 * queued scene billboards. */
	XwaRemasterParticles_SubmitRegion(s.particles, s.scene, snap, cam->region);
	XwaRemasterTrails_SubmitRegion(s.trails, s.scene, cam->region);

	/* State-derived explosion/debris/spark sprites and wreck flames
	 * (OVERLAY stage, back-to-front; engine glows submit per instance
	 * above, in the classic per-object order). */
	fl_submit_billboards();
	XwaRemasterSpeedEffect_Submit(s.speed_effect, s.scene, snap, &effect_view);
	/* Lens-flare trains (LENS stage — the scene draws them after its
	 * color passes with GPU anchor occlusion, before bloom). */
	fl_submit_lens_flares(snap, &flight_view);
	fl_submit_sun_flare(snap, &scene_lighting);

	/* Only the flight view: the map shows ships small on purpose. */
	const float px_per_1080 = (float)s.rt_h / 1080.0f;
	AeronScene_SetSmallInstanceCull(s.scene, s.small_prepass_min_size * px_per_1080,
									s.small_draw_min_size * px_per_1080);
	const int rendered = AeronScene_Render(s.scene, cmd);
	AeronScene_SetSmallInstanceCull(s.scene, 0.0f, 0.0f);
	AeronScene_SetPassHook(s.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, NULL, NULL);
	s.before_opaque_mode = FL_BEFORE_OPAQUE_NONE;
	s.snap = NULL; /* hooks must not outlive the snapshot */
	const int hyperspace_in_world = s.hyperspace_in_world;
	s.hyperspace_in_world = 0;
	if (!rendered) {
		return NULL;
	}
	if (hyperspace_in_world && !fl_render_world_hyperspace(cmd)) {
		return NULL;
	}
	return fl_present(cmd, direct_present);
}

void XwaRemasterFlight_Shutdown(void) {
	fl_pool_free(&s.table_pool);
	fl_pool_free(&s.anim_pool);
	free(s.bolts);
	free(s.bb);
	free(s.mb_prev_index);
	free(s.energy_states);
	XwaRemasterFlightMap_Shutdown();
	XwaRemasterProbeOutlines_Shutdown();
	XwaRemasterEnvironment_Shutdown();
	XwaRemasterHyperspace_Destroy(s.hyperspace);
	s.hyperspace = NULL;
	XwaRemasterSkyStars_Destroy(s.sky_stars);
	s.sky_stars = NULL;
	XwaRemasterSpeedEffect_Destroy(s.speed_effect);
	s.speed_effect = NULL;
	XwaRemasterParticles_Destroy(s.particles);
	s.particles = NULL;
	XwaRemasterTrails_Destroy(s.trails);
	s.trails = NULL;
	XwaRemasterGlowMarks_Shutdown();
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
