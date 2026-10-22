/* HD environment capture — see xwa_remaster/env_capture.h. */

#include "xwa_remaster/env_capture.h"

#include "aeron/aeron.h"
#include "aeron/asset/opt_model.h"
#include "aeron/image.h"
#include "aeron/log.h"
#include "aeron/render.h"
#include "aeron/scene/environment.h"
#include "aeron/scene/scene3d.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/environment.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/ship.h"
#include "xwa_remaster/sky_stars.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENV_CAPTURE_PI 3.14159265358979323846
#define ENV_CAPTURE_SPRITE_SIZE 256
/* Room around a sun's quads in its sprite, as a fraction of the half size. */
#define ENV_CAPTURE_SPRITE_MARGIN 1.1f

static struct {
	int pending;
	int face_size;
	float sun_radius;
	char status[384];
} g_capture = { 0, XWA_ENV_CAPTURE_DEFAULT_FACE_SIZE, XWA_ENV_CAPTURE_DEFAULT_SUN_RADIUS, "" };

/* One exported backdrop sun. */
typedef struct EnvCaptureSun {
	uint32_t backdrop; /* index into snap->backdrops */
	float direction_world[3];
	float color_linear[3];
	float intensity; /* 0 when no directional is linked to the backdrop */
	float luminance;
	int primary;
	float angular_size_degrees;
} EnvCaptureSun;

/* Cube faces in KTX2 order: major axis, then the axes texel columns and rows
 * grow along (envmap.h convention), in the cube frame. */
static const struct {
	const char* suffix;
	const char* label;
	float ma[3], sc[3], tc[3];
} k_faces[6] = {
	{ "px", "+X", { 1, 0, 0 }, { 0, 0, -1 }, { 0, -1, 0 } },
	{ "nx", "-X", { -1, 0, 0 }, { 0, 0, 1 }, { 0, -1, 0 } },
	{ "py", "+Y", { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
	{ "ny", "-Y", { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } },
	{ "pz", "+Z", { 0, 0, 1 }, { 1, 0, 0 }, { 0, -1, 0 } },
	{ "nz", "-Z", { 0, 0, -1 }, { -1, 0, 0 }, { 0, -1, 0 } },
};

static void env_capture_status(int failed, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vsnprintf(g_capture.status, sizeof g_capture.status, format, args);
	va_end(args);
	if (failed) {
		Aeron_LogError("xwa.remaster", "environment capture: %s", g_capture.status);
	} else {
		Aeron_LogInfo("xwa.remaster", "environment capture: %s", g_capture.status);
	}
}

int XwaRemasterEnvCapture_Available(const XwaSnapshot* snap, const char** reason) {
	const char* why = NULL;
	char key[XWA_ENV_MISSION_KEY_MAX];
	if (!snap || snap->scene_kind != XWA_SCENE_FLIGHT || !snap->flight_camera_valid) {
		why = "not in flight";
	} else if (snap->flight_camera.in_hangar) {
		why = "not available in the hangar";
	} else if (snap->flight_camera.hyperspace_phase) {
		why = "not available in hyperspace";
	} else if (snap->flight_camera.death_star_mode) {
		why = "not available in Death Star mode";
	} else if (snap->flight_camera.map_mode) {
		why = "not available on the map";
	} else if (!XwaRemasterEnvironment_NormalizeMissionKey(snap->mission_path, key, sizeof key)) {
		why = "no mission path";
	}
	if (reason) {
		*reason = why;
	}
	return why == NULL;
}

void XwaRemasterEnvCapture_Request(int face_size, float sun_angular_radius_degrees) {
	g_capture.face_size = face_size == 1024 || face_size == 4096 ? face_size : 2048;
	g_capture.sun_radius = sun_angular_radius_degrees >= 0.01f && sun_angular_radius_degrees <= 5.0f
							   ? sun_angular_radius_degrees
							   : XWA_ENV_CAPTURE_DEFAULT_SUN_RADIUS;
	g_capture.pending = 1;
}

int XwaRemasterEnvCapture_Pending(void) { return g_capture.pending; }

const char* XwaRemasterEnvCapture_Status(void) { return g_capture.status; }

/* ---- geometry ------------------------------------------------------------ */

/* XWA world <-> cube frame: cube = B w with B = {1,0,0, 0,0,1, 0,-1,0}. */
static void env_capture_cube_to_world(const float c[3], float w[3]) {
	w[0] = c[0];
	w[1] = -c[2];
	w[2] = c[1];
}

static void env_capture_world_to_cube(const float w[3], float c[3]) {
	c[0] = w[0];
	c[1] = w[2];
	c[2] = -w[1];
}

static void env_capture_cross(const float a[3], const float b[3], float out[3]) {
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

static int env_capture_normalize(float v[3]) {
	const float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (!(length > 1e-6f) || !isfinite(length)) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		v[i] /= length;
	}
	return 1;
}

/* ---- rendering ----------------------------------------------------------- */

static float env_capture_half_to_float(uint16_t h) {
	const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
	const uint32_t exponent = (h >> 10) & 0x1Fu;
	uint32_t mantissa = h & 0x3FFu;
	uint32_t bits;
	if (exponent == 0) {
		if (mantissa == 0) {
			bits = sign;
		} else {
			/* Subnormal: renormalize. */
			int e = -1;
			do {
				e++;
				mantissa <<= 1;
			} while ((mantissa & 0x400u) == 0);
			bits = sign | ((uint32_t)(127 - 15 - e) << 23) | ((mantissa & 0x3FFu) << 13);
		}
	} else if (exponent == 31) {
		bits = sign | 0x7F800000u | (mantissa << 13);
	} else {
		bits = sign | ((exponent + 127u - 15u) << 23) | (mantissa << 13);
	}
	float value;
	memcpy(&value, &bits, sizeof value);
	return value;
}

/* Renders the scene recorded since AeronScene_Begin and reads its colour
 * target back as RGBA16F. */
static int env_capture_render(AeronScene3D* scene, const char* label, uint16_t* pixels, int size) {
	AeronCommandBuffer* cmd = Aeron_AcquireCommandBuffer();
	if (!cmd) {
		return 0;
	}
	Aeron_GpuDebugPush(cmd, label);
	const int rendered = AeronScene_Render(scene, cmd);
	Aeron_GpuDebugPop(cmd);
	if (!rendered) {
		Aeron_CancelCommandBuffer(cmd);
		return 0;
	}
	AeronGpuFence* fence = Aeron_SubmitCommandBufferAndAcquireFence(cmd);
	if (!fence) {
		return 0;
	}
	const int waited = Aeron_WaitForGpuFence(fence);
	Aeron_ReleaseGpuFence(fence);
	const size_t pitch = (size_t)size * 8u;
	return waited &&
		   Aeron_ReadRenderTargetRawPixels(AeronScene_ColorRt(scene), pixels, pitch * (size_t)size, pitch);
}

/* Linear RGB from RGBA16F, with optional horizontal and vertical mirrors. */
static void env_capture_to_rgb(const uint16_t* pixels, int size, int mirror_x, int mirror_y, float* rgb) {
	for (int y = 0; y < size; y++) {
		const int sy = mirror_y ? size - 1 - y : y;
		for (int x = 0; x < size; x++) {
			const int sx = mirror_x ? size - 1 - x : x;
			const uint16_t* p = pixels + ((size_t)sy * size + sx) * 4u;
			float* out = rgb + ((size_t)y * size + x) * 3u;
			for (int c = 0; c < 3; c++) {
				const float v = env_capture_half_to_float(p[c]);
				out[c] = v > 0.0f && isfinite(v) ? v : 0.0f;
			}
		}
	}
}

static AeronScene3D* env_capture_scene(int size) {
	AeronScene3D* scene = AeronScene_Create(&(AeronScene3DDesc) {
		.rt_width = size,
		.rt_height = size,
		.color_format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
		.sample_count = AERON_SAMPLE_COUNT_1,
		.temporal_mode = AERON_TEMPORAL_OFF,
		.view_space_to_meters = AERON_OPT_METERS_PER_UNIT,
	});
	if (scene) {
		AeronScene_SetClearColor(scene, (const float[4]) { 0.0f, 0.0f, 0.0f, 1.0f });
	}
	return scene;
}

/* ---- suns ---------------------------------------------------------------- */

static float env_capture_luminance(const float rgb[3]) {
	return 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
}

static int env_capture_sun_order(const void* a, const void* b) {
	const EnvCaptureSun* x = (const EnvCaptureSun*)a;
	const EnvCaptureSun* y = (const EnvCaptureSun*)b;
	if (x->primary != y->primary) {
		return x->primary ? -1 : 1;
	}
	if (x->luminance != y->luminance) {
		return x->luminance > y->luminance ? -1 : 1;
	}
	return x->backdrop < y->backdrop ? -1 : (x->backdrop > y->backdrop ? 1 : 0);
}

/* Every visible backdrop sun of the region, with the directional linked to
 * it, ordered primary first, then by decreasing luminance. Suns that `hide`
 * hides are left out. */
static uint32_t env_capture_collect_suns(const XwaSnapshot* snap, XwaRemasterAssets* assets,
										 const XwaEnvironmentBackdropRule* hide, EnvCaptureSun* suns) {
	XwaRemasterDirLight lights[XWA_SNAP_MAX_DIR_LIGHTS];
	const uint32_t light_count =
		XwaRemaster_ReadGameDirLights(snap->dir_lights, snap->dir_light_count, lights);
	const XwaRemasterDirLight* key = XwaRemasterShip_SelectKeyDirectionalLight(lights, light_count);
	uint32_t count = 0;
	for (uint32_t i = 0; i < snap->backdrop_count; i++) {
		const XwaBackdrop* b = &snap->backdrops[i];
		if (b->object_type < XWA_SNAP_TYPE_BACKDROP_SUN_FIRST ||
			b->object_type > XWA_SNAP_TYPE_BACKDROP_SUN_LAST ||
			XwaRemasterFlight_CaptureBackdropHidden(snap, assets, i, hide)) {
			continue;
		}
		EnvCaptureSun* sun = &suns[count];
		memset(sun, 0, sizeof *sun);
		sun->backdrop = i;
		memcpy(sun->direction_world, b->world_dir, sizeof sun->direction_world);
		if (!env_capture_normalize(sun->direction_world)) {
			continue;
		}
		sun->color_linear[0] = sun->color_linear[1] = sun->color_linear[2] = 1.0f;
		/* The brightest directional produced by this backdrop. */
		const XwaRemasterDirLight* linked = NULL;
		float linked_peak = 0.0f;
		for (uint32_t j = 0; j < light_count; j++) {
			const XwaRemasterDirLight* light = &lights[j];
			const float peak = fmaxf(light->color[0], fmaxf(light->color[1], light->color[2]));
			if (light->source_backdrop_index != (int16_t)i || !(peak > 0.0f)) {
				continue;
			}
			if (!linked || peak > linked_peak) {
				linked = light;
				linked_peak = peak;
			}
		}
		if (linked) {
			for (int c = 0; c < 3; c++) {
				sun->color_linear[c] = linked->color[c] / linked_peak;
			}
			sun->intensity = linked_peak > 16.0f ? 16.0f : linked_peak;
			sun->luminance = sun->intensity * env_capture_luminance(sun->color_linear);
			sun->primary = linked == key;
		}
		count++;
	}
	qsort(suns, count, sizeof suns[0], env_capture_sun_order);
	return count;
}

/* Renders one sun's quads alone along its direction into a sprite. */
static int env_capture_sun_sprite(const XwaSnapshot* snap, XwaRemasterAssets* assets, AeronScene3D* scene,
								  uint16_t* pixels, float* rgb, EnvCaptureSun* sun, const char* path) {
	const float* forward = sun->direction_world;
	/* World +Z up, or +Y within 1 degree of the poles. */
	float up[3] = { 0.0f, 0.0f, 1.0f };
	if (fabsf(forward[2]) > cosf((float)(ENV_CAPTURE_PI / 180.0))) {
		up[1] = 1.0f;
		up[2] = 0.0f;
	}
	float right[3], down[3];
	env_capture_cross(forward, up, right);
	if (!env_capture_normalize(right)) {
		return 0;
	}
	env_capture_cross(forward, right, down);
	AeronSceneCamera camera;
	float crows[9];
	float max_tan = 0.0f;
	if (!XwaRemasterFlight_CaptureCamera(right, down, forward, 1.0f, ENV_CAPTURE_SPRITE_SIZE, &camera,
										 crows) ||
		XwaRemasterFlight_CaptureBackdrops(NULL, crows, snap, assets, (int)sun->backdrop, NULL, &max_tan) ==
			0 ||
		!(max_tan > 0.0f)) {
		Aeron_LogWarn("xwa.remaster", "environment capture: sun backdrop %u has no quads in front of it",
					  sun->backdrop);
		return 0;
	}
	const float half_tan = max_tan * ENV_CAPTURE_SPRITE_MARGIN;
	sun->angular_size_degrees = (float)(2.0 * atan((double)half_tan) * 180.0 / ENV_CAPTURE_PI);
	if (sun->angular_size_degrees > 60.0f) {
		Aeron_LogWarn("xwa.remaster", "environment capture: sun backdrop %u spans %.1f degrees (limit 60)",
					  sun->backdrop, (double)sun->angular_size_degrees);
		return 0;
	}
	if (!XwaRemasterFlight_CaptureCamera(right, down, forward, half_tan, ENV_CAPTURE_SPRITE_SIZE, &camera,
										 crows) ||
		!AeronScene_Begin(scene, &camera)) {
		return 0;
	}
	XwaRemasterFlight_CaptureBackdrops(scene, crows, snap, assets, (int)sun->backdrop, NULL, NULL);
	if (!env_capture_render(scene, "Environment capture sun sprite", pixels, ENV_CAPTURE_SPRITE_SIZE)) {
		return 0;
	}
	/* Sky sprites put texture row 0 on the camera's down side (sky_sun_basis
	 * in scene3d.c), so the rendered image is stored bottom row first. */
	env_capture_to_rgb(pixels, ENV_CAPTURE_SPRITE_SIZE, 0, 1, rgb);
	return Aeron_ImageWriteHdr(Aeron_GetVfs(), AERON_VFS_ROOT_USER, path, ENV_CAPTURE_SPRITE_SIZE,
							   ENV_CAPTURE_SPRITE_SIZE, rgb);
}

/* ---- capture.yaml ---------------------------------------------------------- */

typedef struct EnvCaptureText {
	char* data;
	size_t size;
	size_t capacity;
	int ok;
} EnvCaptureText;

static void env_capture_printf(EnvCaptureText* t, const char* format, ...) {
	if (!t->ok) {
		return;
	}
	va_list args;
	va_start(args, format);
	const int n = vsnprintf(t->data + t->size, t->capacity - t->size, format, args);
	va_end(args);
	if (n < 0 || (size_t)n >= t->capacity - t->size) {
		t->ok = 0;
		return;
	}
	t->size += (size_t)n;
}

static int env_capture_write_description(const char* path, const char* name, const char* mission,
										 const XwaSnapshot* snap, const AeronPreparedEnvironment* sky,
										 int face_size, const EnvCaptureSun* suns, uint32_t sun_count) {
	static char buffer[8192];
	EnvCaptureText t = { buffer, 0, sizeof buffer, 1 };
	XwaFlightCaptureSky stars;
	XwaRemasterFlight_CaptureSkyInfo(&stars);
	env_capture_printf(&t,
					   "# HD environment capture: environment_bake --capture <this directory> "
					   "--bake-root <mod folder>\n"
					   "version: 1\n"
					   "environment: \"%s\"\n"
					   "mission: \"%s\"\n"
					   "region: %u\n"
					   "face_size: %d\n"
					   "sky_faces: \"sky\"\n",
					   name, mission, (unsigned)snap->flight_camera.region, face_size);
	env_capture_printf(&t, "directional_lights:%s\n", sun_count ? "" : " []");
	for (uint32_t i = 0; i < sun_count; i++) {
		const EnvCaptureSun* sun = &suns[i];
		float cube[3];
		env_capture_world_to_cube(sun->direction_world, cube);
		env_capture_printf(&t,
						   "  - name: sun_%u\n"
						   "    direction_cube: [%.9g, %.9g, %.9g]\n"
						   "    color_linear: [%.9g, %.9g, %.9g]\n"
						   "    intensity: %.9g\n"
						   "    primary: %s\n"
						   "    casts_shadows: %s\n"
						   "    angular_radius_degrees: %.9g\n"
						   "    lens_flare: true\n"
						   "    visual:\n"
						   "      kind: sprite\n"
						   "      sprite: \"sun_%u.hdr\"\n"
						   "      angular_size_degrees: %.9g\n"
						   "      sprite_strength: 1\n",
						   i, (double)cube[0], (double)cube[1], (double)cube[2], (double)sun->color_linear[0],
						   (double)sun->color_linear[1], (double)sun->color_linear[2], (double)sun->intensity,
						   sun->primary ? "true" : "false", sun->primary ? "true" : "false",
						   (double)g_capture.sun_radius, i, (double)sun->angular_size_degrees);
	}
	env_capture_printf(&t,
					   "capture_settings:\n"
					   "  starfield_exposure: %.9g\n"
					   "  star_brightness: %.9g\n"
					   "  star_density: %.9g\n"
					   "  star_grid: %.9g\n"
					   "  game_time_ticks: %lld\n"
					   "  proj_scale: %.9g\n",
					   (double)stars.exposure, (double)stars.star_brightness, (double)stars.star_density,
					   (double)stars.star_grid, (long long)snap->game_time_ticks,
					   (double)snap->flight_camera.proj_scale);
	/* With `backdrops: keep`, the region's sky replaces the starfield. */
	if (sky && sky->definition) {
		env_capture_printf(&t, "  sky_environment: \"%s\"\n", sky->definition->name);
	}
	return t.ok && AeronVfs_WriteAllAtomic(Aeron_GetVfs(), AERON_VFS_ROOT_USER, path, t.data, t.size);
}

/* ---- capture --------------------------------------------------------------- */

int XwaRemasterEnvCapture_FaceCamera(int face, int size, AeronSceneCamera* camera, float crows[9]) {
	if (face < 0 || face >= 6) {
		return 0;
	}
	/* The face's texel axes are mirrored (sc x tc = -ma), so the camera looks
	 * along ma with right = -sc, and the columns come out mirrored. */
	float right[3], down[3], forward[3], neg_sc[3];
	for (int i = 0; i < 3; i++) {
		neg_sc[i] = -k_faces[face].sc[i];
	}
	env_capture_cube_to_world(neg_sc, right);
	env_capture_cube_to_world(k_faces[face].tc, down);
	env_capture_cube_to_world(k_faces[face].ma, forward);
	return XwaRemasterFlight_CaptureCamera(right, down, forward, 1.0f, size, camera, crows);
}

static int env_capture_run(const XwaSnapshot* snap, XwaRemasterAssets* assets, const char* directory,
						   const char* name, const char* mission) {
	const int face_size = g_capture.face_size;
	/* In a region with `backdrops: keep`, capture what the game shows: the
	 * environment's sky, without the hidden backdrops. */
	XwaEnvironmentSelection selection;
	XwaRemasterEnvironment_Resolve(snap, &selection);
	const int keep = XwaRemasterEnvironment_KeepsBackdrops(&selection);
	const AeronPreparedEnvironment* sky = keep ? selection.prepared : NULL;
	const XwaEnvironmentBackdropRule* hide = keep ? selection.backdrops : NULL;
	EnvCaptureSun suns[XWA_SNAP_MAX_BACKDROPS];
	uint32_t sun_count = env_capture_collect_suns(snap, assets, hide, suns);
	uint32_t dropped = 0;
	if (sun_count > AERON_ENV_MAX_LIGHTS) {
		dropped = sun_count - AERON_ENV_MAX_LIGHTS;
		sun_count = AERON_ENV_MAX_LIGHTS;
		Aeron_LogWarn("xwa.remaster", "environment capture: %u backdrop sun(s) past the fourth dropped",
					  dropped);
	}

	AeronScene3D* face_scene = env_capture_scene(face_size);
	AeronScene3D* sprite_scene = sun_count ? env_capture_scene(ENV_CAPTURE_SPRITE_SIZE) : NULL;
	XwaRemasterSkyStars* stars = XwaRemasterSkyStars_Create();
	uint16_t* pixels = (uint16_t*)malloc((size_t)face_size * face_size * 8u);
	float* rgb = (float*)malloc((size_t)face_size * face_size * 3u * sizeof(float));
	int ok = face_scene && (sun_count == 0 || sprite_scene) && stars && pixels && rgb;
	if (!ok) {
		env_capture_status(1, "cannot create the capture resources");
	}

	/* One classic pixel spans 1 / proj_scale radians at the screen centre, a
	 * face pixel 2 / face_size: stars keep their angular size. */
	const float proj_scale = snap->flight_camera.proj_scale > 0.0f ? snap->flight_camera.proj_scale : 512.0f;
	const float star_upscale = (float)face_size / (2.0f * proj_scale);
	char path[256];
	for (int face = 0; ok && face < 6; face++) {
		AeronSceneCamera camera;
		float crows[9];
		char label[64];
		snprintf(label, sizeof label, "Environment capture face %s", k_faces[face].label);
		snprintf(path, sizeof path, "%s/sky_%s.hdr", directory, k_faces[face].suffix);
		ok = XwaRemasterEnvCapture_FaceCamera(face, face_size, &camera, crows) &&
			 AeronScene_Begin(face_scene, &camera) &&
			 XwaRemasterFlight_CaptureSetSky(face_scene, stars, snap, sky, star_upscale);
		if (ok) {
			XwaRemasterFlight_CaptureBackdrops(face_scene, crows, snap, assets,
											   XWA_FLIGHT_CAPTURE_BACKDROPS_NO_SUNS, hide, NULL);
			ok = env_capture_render(face_scene, label, pixels, face_size);
		}
		if (ok) {
			/* The columns are mirrored back (XwaRemasterEnvCapture_FaceCamera). */
			env_capture_to_rgb(pixels, face_size, 1, 0, rgb);
			ok = Aeron_ImageWriteHdr(Aeron_GetVfs(), AERON_VFS_ROOT_USER, path, face_size, face_size, rgb);
		}
		if (!ok) {
			env_capture_status(1, "face %s failed (%s)", k_faces[face].label, path);
		}
	}

	/* Suns whose sprite cannot be made are reported and left out. */
	uint32_t exported = 0;
	for (uint32_t i = 0; ok && i < sun_count; i++) {
		EnvCaptureSun sun = suns[i];
		snprintf(path, sizeof path, "%s/sun_%u.hdr", directory, exported);
		if (env_capture_sun_sprite(snap, assets, sprite_scene, pixels, rgb, &sun, path)) {
			suns[exported++] = sun;
		} else {
			dropped++;
		}
	}
	if (ok) {
		snprintf(path, sizeof path, "%s/capture.yaml", directory);
		ok = env_capture_write_description(path, name, mission, snap, sky, face_size, suns, exported);
		if (!ok) {
			env_capture_status(1, "cannot write %s", path);
		}
	}
	if (ok) {
		int primary = 0;
		for (uint32_t i = 0; i < exported; i++) {
			primary |= suns[i].primary;
		}
		env_capture_status(0, "%s (user root): %d px faces, %u sun(s)%s%s", directory, face_size, exported,
						   exported && !primary ? ", no primary (the key light is not a sun)" : "",
						   dropped ? ", some suns dropped (see log)" : "");
	}

	free(rgb);
	free(pixels);
	XwaRemasterSkyStars_Destroy(stars);
	AeronScene_Destroy(sprite_scene);
	AeronScene_Destroy(face_scene);
	return ok;
}

void XwaRemasterEnvCapture_Run(const XwaSnapshot* snap, XwaRemasterAssets* assets) {
	if (!g_capture.pending) {
		return;
	}
	g_capture.pending = 0;
	const char* reason = NULL;
	if (!assets || !XwaRemasterEnvCapture_Available(snap, &reason)) {
		env_capture_status(1, "%s", reason ? reason : "no assets");
		return;
	}
	char key[XWA_ENV_MISSION_KEY_MAX];
	XwaRemasterEnvironment_NormalizeMissionKey(snap->mission_path, key, sizeof key);
	const unsigned region = snap->flight_camera.region;

	/* Directory: the key with '/' as '_'. Environment name: the same,
	 * restricted to the environment-name alphabet, plus the region. */
	char mission_dir[XWA_ENV_MISSION_KEY_MAX];
	char name[96];
	snprintf(mission_dir, sizeof mission_dir, "%s", key);
	for (char* p = mission_dir; *p; p++) {
		if (*p == '/') {
			*p = '_';
		}
	}
	snprintf(name, sizeof name, "captured.%s.%u", mission_dir, region);
	for (char* p = name; *p; p++) {
		const int allowed = (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '.' || *p == '_';
		if (!allowed) {
			*p = '_';
		}
	}
	if (!AeronEnvironment_IsValidName(name)) {
		env_capture_status(1, "environment name '%s' is not valid (too long?)", name);
		return;
	}
	char directory[224];
	snprintf(directory, sizeof directory, "captures/environments/%s/region_%u", mission_dir, region);
	if (!AeronVfs_CreateDirectory(Aeron_GetVfs(), AERON_VFS_ROOT_USER, directory)) {
		env_capture_status(1, "cannot create %s in the user root", directory);
		return;
	}
	Aeron_LogInfo("xwa.remaster", "environment capture: %s region %u at %d px", key, region,
				  g_capture.face_size);
	env_capture_run(snap, assets, directory, name, key);
}
