/* Captured lighting — see xwa_remaster/captured_lighting.h. */

#include "xwa_remaster/captured_lighting.h"

#include "aeron/aeron.h"
#include "aeron/asset/opt_model.h"
#include "aeron/log.h"
#include "aeron/scene/environment_builder.h"
#include "xwa_remaster/env_capture.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/sky_stars.h"

#include <stdio.h>
#include <string.h>

/* XWA world -> cube frame: cube = (wx, wz, -wy). The faces are rendered in
 * this frame (XwaRemasterEnvCapture_FaceCamera). */
static const float cl_world_to_cube[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f };

static struct {
	XwaRemasterCapturedLightingOptions options;
	AeronScene3D* scene;
	XwaRemasterSkyStars* stars;
	AeronEnvironmentBuilder* builder;
	int failed;
	int ready;
	uint64_t signature; /* of the cubes */
	char mission_key[XWA_ENV_MISSION_KEY_MAX];
	unsigned region;
	uint32_t build_count;
	int applies;
	uint64_t frame_signature;
	/* Whether the region of `checked_signature` draws a backdrop that is not
	 * a sun. */
	int checked;
	uint64_t checked_signature;
	int has_backdrops;
} g;

void XwaRemasterCapturedLighting_Configure(const XwaRemasterCapturedLightingOptions* options) {
	if (options) {
		g.options = *options;
	}
}

void XwaRemasterCapturedLighting_Shutdown(void) {
	AeronEnvironmentBuilder_Destroy(g.builder);
	XwaRemasterSkyStars_Destroy(g.stars);
	AeronScene_Destroy(g.scene);
	const XwaRemasterCapturedLightingOptions options = g.options;
	memset(&g, 0, sizeof g);
	g.options = options;
}

/* ---- signature ---------------------------------------------------------- */

static uint64_t cl_hash(uint64_t hash, const void* data, size_t size) {
	const uint8_t* bytes = (const uint8_t*)data;
	for (size_t i = 0; i < size; i++) {
		hash = (hash ^ bytes[i]) * 0x100000001B3ull;
	}
	return hash;
}

static uint64_t cl_hash_u32(uint64_t hash, uint32_t value) { return cl_hash(hash, &value, sizeof value); }

/* Computes a signature of what the cubes show: the mission, the region, the
 * sky, the hide and show rules, and each backdrop's hidden flag. It leaves
 * out the backdrops' animation frames, so an animated backdrop does not
 * rebuild the cubes. */
static uint64_t cl_signature(const XwaSnapshot* snap, const XwaEnvironmentSelection* selection) {
	uint64_t hash = 0xCBF29CE484222325ull;
	char key[XWA_ENV_MISSION_KEY_MAX];
	const char* mission = XwaRemasterEnvironment_NormalizeMissionKey(snap->mission_path, key, sizeof key)
							  ? key
							  : snap->mission_path;
	hash = cl_hash(hash, mission, strlen(mission) + 1);
	hash = cl_hash_u32(hash, snap->flight_camera.mission_region);
	if (XwaRemasterEnvironment_KeepsBackdrops(selection)) {
		const AeronPreparedEnvironment* sky = selection->prepared;
		const XwaEnvironmentBackdropRule* rule = selection->backdrops;
		hash = cl_hash(hash, &sky->radiance, sizeof sky->radiance);
		hash = cl_hash(hash, sky->world_to_cube, sizeof sky->world_to_cube);
		hash = cl_hash(hash, &sky->settings.intensity, sizeof sky->settings.intensity);
		hash = cl_hash_u32(hash, (uint32_t)rule->hide_all | (uint32_t)rule->show_all << 1);
		hash = cl_hash_u32(hash, rule->hide_count);
		hash = cl_hash(hash, rule->hide, rule->hide_count * sizeof rule->hide[0]);
		hash = cl_hash_u32(hash, rule->show_count);
		hash = cl_hash(hash, rule->show, rule->show_count * sizeof rule->show[0]);
	} else {
		hash = cl_hash_u32(hash, 0xFFFFFFFFu); /* the starfield */
	}
	hash = cl_hash_u32(hash, snap->backdrop_count);
	for (uint32_t i = 0; i < snap->backdrop_count; i++) {
		hash = cl_hash_u32(hash, i << 1 | (snap->backdrops[i].hidden ? 1u : 0u));
	}
	return hash;
}

static int cl_applies(const XwaSnapshot* snap, const XwaEnvironmentSelection* selection) {
	if (!g.options.enabled || !snap || !selection || snap->scene_kind != XWA_SCENE_FLIGHT ||
		!snap->flight_camera_valid) {
		return 0;
	}
	const XwaFlightCamera* cam = &snap->flight_camera;
	if (cam->in_hangar || cam->death_star_mode || cam->hyperspace_phase || cam->map_mode) {
		return 0;
	}
	return selection->mode == XWA_ENV_MODE_CLASSIC || XwaRemasterEnvironment_KeepsBackdrops(selection);
}

/* Returns whether the region draws a backdrop that is not a sun. Without
 * one, the cubes would show only the sky: the starfield, which lights almost
 * nothing, or the environment's sky, whose baked cubes are better. */
static int cl_has_backdrops(const XwaSnapshot* snap, XwaRemasterAssets* assets,
							const XwaEnvironmentSelection* selection) {
	static const float identity[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	const XwaEnvironmentBackdropRule* hide =
		XwaRemasterEnvironment_KeepsBackdrops(selection) ? selection->backdrops : NULL;
	return XwaRemasterFlight_CaptureBackdrops(NULL, identity, snap, assets,
											  XWA_FLIGHT_CAPTURE_BACKDROPS_NO_SUNS, hide, NULL) > 0;
}

/* ---- build -------------------------------------------------------------- */

/* Creates the face scene, its starfield and the builder on the first build.
 * After a failure, nothing is retried. */
static int cl_ensure(void) {
	if (g.builder) {
		return 1;
	}
	if (g.failed) {
		return 0;
	}
	const int size = g.options.size;
	g.scene = AeronScene_Create(&(AeronScene3DDesc) {
		.rt_width = size,
		.rt_height = size,
		.color_format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
		.sample_count = AERON_SAMPLE_COUNT_1,
		.temporal_mode = AERON_TEMPORAL_OFF,
		.view_space_to_meters = AERON_OPT_METERS_PER_UNIT,
	});
	if (g.scene) {
		AeronScene_SetClearColor(g.scene, (const float[4]) { 0.0f, 0.0f, 0.0f, 1.0f });
	}
	g.stars = XwaRemasterSkyStars_Create();
	g.builder = AeronEnvironmentBuilder_Create(&(AeronEnvironmentBuilderDesc) {
		.size = size,
		.specular_samples = g.options.specular_samples,
	});
	if (!g.scene || !g.stars || !g.builder) {
		Aeron_LogError(
			"xwa.remaster",
			"captured lighting: cannot create the face scene, the starfield or the builder (%d px)", size);
		AeronEnvironmentBuilder_Destroy(g.builder);
		XwaRemasterSkyStars_Destroy(g.stars);
		AeronScene_Destroy(g.scene);
		g.builder = NULL;
		g.stars = NULL;
		g.scene = NULL;
		g.failed = 1;
		return 0;
	}
	return 1;
}

/* Renders the six faces and records the filters into `cmd`. */
static int cl_record_build(AeronCommandBuffer* cmd, const XwaSnapshot* snap, XwaRemasterAssets* assets,
						   const XwaEnvironmentSelection* selection) {
	const int keep = XwaRemasterEnvironment_KeepsBackdrops(selection);
	const AeronPreparedEnvironment* sky = keep ? selection->prepared : NULL;
	const XwaEnvironmentBackdropRule* hide = keep ? selection->backdrops : NULL;
	const int size = g.options.size;
	/* Stars keep their angular size, as in the environment capture. */
	const float proj_scale = snap->flight_camera.proj_scale > 0.0f ? snap->flight_camera.proj_scale : 512.0f;
	const float star_upscale = (float)size / (2.0f * proj_scale);
	for (int face = 0; face < 6; face++) {
		AeronSceneCamera camera;
		float crows[9];
		if (!XwaRemasterEnvCapture_FaceCamera(face, size, &camera, crows) ||
			!AeronScene_Begin(g.scene, &camera)) {
			return 0;
		}
		/* The starfield's hook stays on the scene until cleared. */
		AeronScene_SetPassHook(g.scene, AERON_SCENE_HOOK_BEFORE_OPAQUE, NULL, NULL);
		if (!XwaRemasterFlight_CaptureSetSky(g.scene, g.stars, snap, sky, star_upscale)) {
			return 0;
		}
		XwaRemasterFlight_CaptureBackdrops(g.scene, crows, snap, assets, XWA_FLIGHT_CAPTURE_BACKDROPS_NO_SUNS,
										   hide, NULL);
		/* The columns come out mirrored (XwaRemasterEnvCapture_FaceCamera). */
		if (!AeronScene_Render(g.scene, cmd) ||
			!AeronEnvironmentBuilder_StoreFace(g.builder, cmd, face, AeronScene_ColorTexture(g.scene),
											   true)) {
			return 0;
		}
	}
	return AeronEnvironmentBuilder_Finish(g.builder, cmd);
}

static int cl_build(AeronCommandBuffer* cmd, const XwaSnapshot* snap, XwaRemasterAssets* assets,
					const XwaEnvironmentSelection* selection, uint64_t signature) {
	if (!cl_ensure()) {
		return 0;
	}
	Aeron_GpuDebugPush(cmd, "Captured lighting");
	const int built = cl_record_build(cmd, snap, assets, selection);
	Aeron_GpuDebugPop(cmd);
	if (!built) {
		g.failed = 1;
		g.ready = 0;
		Aeron_LogError("xwa.remaster", "captured lighting: the build failed for region %u",
					   (unsigned)snap->flight_camera.mission_region);
		return 0;
	}
	g.ready = 1;
	g.signature = signature;
	g.region = snap->flight_camera.mission_region;
	if (!XwaRemasterEnvironment_NormalizeMissionKey(snap->mission_path, g.mission_key,
													sizeof g.mission_key)) {
		snprintf(g.mission_key, sizeof g.mission_key, "unknown");
	}
	g.build_count++;
	Aeron_LogInfo("xwa.remaster", "captured lighting: built the cubes of %s region %u (%s)", g.mission_key,
				  g.region,
				  XwaRemasterEnvironment_KeepsBackdrops(selection) ? "environment sky" : "starfield");
	return 1;
}

void XwaRemasterCapturedLighting_Update(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
										XwaRemasterAssets* assets, const XwaEnvironmentSelection* selection) {
	g.applies = cl_applies(snap, selection);
	if (!g.applies || !cmd || !assets) {
		return;
	}
	g.frame_signature = cl_signature(snap, selection);
	if (!g.checked || g.checked_signature != g.frame_signature) {
		g.checked = 1;
		g.checked_signature = g.frame_signature;
		g.has_backdrops = cl_has_backdrops(snap, assets, selection);
		if (!g.has_backdrops) {
			Aeron_LogInfo("xwa.remaster",
						  "captured lighting: region %u draws no backdrop other than suns; "
						  "nothing to capture",
						  (unsigned)snap->flight_camera.mission_region);
		}
	}
	if (g.has_backdrops && !g.failed && (!g.ready || g.signature != g.frame_signature)) {
		cl_build(cmd, snap, assets, selection, g.frame_signature);
	}
}

int XwaRemasterCapturedLighting_Environment(const XwaSnapshot* snap, const XwaEnvironmentSelection* selection,
											AeronScenePbrEnvironment* out, const float** ambient_fill) {
	if (ambient_fill) {
		*ambient_fill = NULL;
	}
	if (!out || !cl_applies(snap, selection) || !g.ready || g.signature != cl_signature(snap, selection) ||
		!AeronEnvironmentBuilder_Result(g.builder, out)) {
		return 0;
	}
	memcpy(out->world_to_local, cl_world_to_cube, sizeof out->world_to_local);
	if (XwaRemasterEnvironment_KeepsBackdrops(selection)) {
		/* Use the environment's strengths, or 1 for a cube it does not have. */
		const AeronEnvironmentDefinition* settings = &selection->prepared->settings;
		out->diffuse_strength =
			selection->prepared->diffuse ? settings->intensity * settings->diffuse_strength : 1.0f;
		out->specular_strength =
			selection->prepared->specular ? settings->intensity * settings->specular_strength : 1.0f;
	} else {
		out->diffuse_strength = g.options.diffuse_strength;
		out->specular_strength = g.options.specular_strength;
		if (ambient_fill && g.options.has_ambient_fill) {
			*ambient_fill = g.options.ambient_fill;
		}
	}
	return 1;
}

/* ---- debug tool --------------------------------------------------------- */

void XwaRemasterCapturedLighting_Info(XwaRemasterCapturedLightingInfo* out) {
	if (!out) {
		return;
	}
	memset(out, 0, sizeof *out);
	out->enabled = g.options.enabled;
	out->size = g.options.size;
	out->applies = g.applies;
	out->ready = g.ready;
	out->current = g.ready && g.applies && g.signature == g.frame_signature;
	out->nothing_to_capture =
		g.applies && g.checked && g.checked_signature == g.frame_signature && !g.has_backdrops;
	out->failed = g.failed;
	out->signature = g.signature;
	out->build_count = g.build_count;
}
