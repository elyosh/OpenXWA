/* Shared craft-rendering policy — see xwa_remaster/ship.h. */

#include "xwa_remaster/ship.h"

#include "aeron/aeron.h"
#include "aeron/asset/flight_model.h"
#include "aeron/asset/opt_model.h"
#include "aeron/scene/billboard.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/glow_marks.h"
#include "xwa_remaster/opt_mesh.h"

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* ---- classic-lifetime HD assets ------------------------------------ */

typedef struct ShipMeshAsset {
	char key[XWA_SNAP_OPT_NAME_MAX];
	AeronSceneMesh* mesh;
	int runtime_opt;
	int pending_new;
} ShipMeshAsset;

typedef struct ShipDesiredAsset {
	char key[XWA_SNAP_OPT_NAME_MAX];
} ShipDesiredAsset;

static ShipMeshAsset s_meshes[XWA_SNAP_MAX_OPT_ASSETS];
static uint32_t s_mesh_count;
static ShipMeshAsset s_pending_meshes[XWA_SNAP_MAX_OPT_ASSETS];
static uint32_t s_pending_mesh_count;
static uint64_t s_pending_generation = UINT64_MAX;
static uint64_t s_opt_asset_generation = UINT64_MAX;
static uint64_t s_batch_generation;
static int s_batch_active;
static int s_batch_completes_generation;
static float s_opt_smooth_angle_degrees;
static float s_opt_emissive_strength;
static float s_opt_projectile_emissive_strength;
static float s_engine_emissive_strength;
static int s_force_opt_models;
static XwaShipPbrTuning g_pbr_tuning_default;
static XwaShipPbrTuning g_pbr_tuning;

static void ship_set_failure(AeronCommandBuffer* cmd, const char* format, ...) {
	char message[512];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof message, format, args);
	va_end(args);
	Aeron_CommandBufferSetFailure(cmd, message);
}

void XwaRemasterShip_Configure(float opt_smooth_angle_degrees, float opt_emissive_strength,
							   float opt_projectile_emissive_strength, float engine_emissive_strength,
							   int force_opt_models) {
	s_opt_smooth_angle_degrees = opt_smooth_angle_degrees;
	s_opt_emissive_strength = opt_emissive_strength;
	s_opt_projectile_emissive_strength = opt_projectile_emissive_strength;
	s_engine_emissive_strength = engine_emissive_strength;
	s_force_opt_models = force_opt_models != 0;
}

static void ship_mesh_key(const char* name, char key[XWA_SNAP_OPT_NAME_MAX]) {
	size_t n = 0;
	if (!name) {
		key[0] = '\0';
		return;
	}
	while (name[n] && name[n] != '.' && n + 1 < XWA_SNAP_OPT_NAME_MAX) {
		char c = name[n];
		key[n] = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
		n++;
	}
	key[n] = '\0';
}

static AeronSceneMesh* ship_mesh_load(AeronCommandBuffer* cmd, const char* key, int* out_runtime_opt) {
	char path[512];
	AeronFlightModel model;
	if (out_runtime_opt) {
		*out_runtime_opt = 0;
	}
	if (!s_force_opt_models) {
		snprintf(path, sizeof path, "%s/remaster/models/%s.glb", Aeron_AssetRoot(), key);
		if (Aeron_FlightModelBuild(path, &model)) {
			AeronSceneMeshCreateStatus create_status;
			AeronSceneMesh* mesh = AeronScene_MeshCreate(cmd, &model, key, &create_status);
			Aeron_FlightModelFree(&model);
			if (mesh) {
				Aeron_LogVerbose("xwa.remaster", "mesh: '%s' source=GLB", key);
				return mesh;
			}
			if (create_status != AERON_SCENE_MESH_CREATE_INVALID_SOURCE) {
				Aeron_LogWarn("xwa.remaster", "mesh: GLB resource creation failed '%s'", path);
				ship_set_failure(cmd, "Ship model '%s': GLB resource creation failed for '%s'", key, path);
				return NULL;
			}
			Aeron_LogWarn("xwa.remaster", "mesh: GLB content invalid '%s'; trying OPT", path);
		}
	}

	char opt_error[256];
	if (!XwaRemasterOptMesh_Build(Aeron_GetVfs(), key, s_opt_smooth_angle_degrees, s_opt_emissive_strength,
								  &model, opt_error, sizeof opt_error)) {
		Aeron_LogWarn("xwa.remaster", "mesh: OPT load failed '%s': %s", key, opt_error);
		ship_set_failure(cmd, "Ship model '%s': OPT load failed: %s", key, opt_error);
		return NULL;
	}
	AeronSceneMeshCreateStatus create_status;
	AeronSceneMesh* mesh = AeronScene_MeshCreate(cmd, &model, key, &create_status);
	Aeron_FlightModelFree(&model);
	if (!mesh) {
		Aeron_LogError("xwa.remaster", "mesh: OPT resource creation failed '%s' status=%d", key,
					   (int)create_status);
		ship_set_failure(cmd, "Ship model '%s': OPT resource creation failed (status %d)", key,
						 (int)create_status);
	} else {
		if (out_runtime_opt) {
			*out_runtime_opt = 1;
		}
		Aeron_LogVerbose("xwa.remaster", "mesh: '%s' source=OPT smooth_angle=%.3g emissive_strength=%.3g",
						 key, (double)s_opt_smooth_angle_degrees, (double)s_opt_emissive_strength);
	}
	return mesh;
}

int XwaRemasterShip_AssetsNeedSync(const XwaSnapshot* snapshot) {
	return snapshot && snapshot->opt_asset_generation != s_opt_asset_generation;
}

static void ship_mesh_asset_destroy(ShipMeshAsset* asset) {
	if (asset && asset->mesh) {
		XwaRemasterGlowMarks_InvalidateMesh(asset->mesh);
		AeronScene_MeshDestroy(asset->mesh);
	}
	if (asset) {
		memset(asset, 0, sizeof *asset);
	}
}

static void ship_pending_discard(void) {
	for (uint32_t i = 0; i < s_pending_mesh_count; i++) {
		if (s_pending_meshes[i].pending_new) {
			ship_mesh_asset_destroy(&s_pending_meshes[i]);
		}
	}
	memset(s_pending_meshes, 0, sizeof s_pending_meshes);
	s_pending_mesh_count = 0;
	s_pending_generation = UINT64_MAX;
}

XwaRemasterShipSyncResult XwaRemasterShip_SyncAssets(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot,
													 uint64_t byte_budget, uint32_t copy_budget) {
	if (!cmd || !snapshot || !XwaRemasterShip_AssetsNeedSync(snapshot)) {
		return XWA_REMASTER_SHIP_SYNC_COMPLETE;
	}
	if (s_batch_active) {
		Aeron_LogError("xwa.remaster", "mesh asset synchronization started with an unfinished batch");
		ship_set_failure(cmd, "Ship model synchronization started with an unfinished batch");
		return XWA_REMASTER_SHIP_SYNC_FAILED;
	}

	ShipDesiredAsset desired[XWA_SNAP_MAX_OPT_ASSETS];
	uint32_t desired_count = 0;
	for (uint32_t i = 0; i < snapshot->opt_asset_count; i++) {
		char key[XWA_SNAP_OPT_NAME_MAX];
		ship_mesh_key(snapshot->opt_assets[i].name, key);
		if (!key[0]) {
			continue;
		}
		uint32_t found = desired_count;
		for (uint32_t j = 0; j < desired_count; j++) {
			if (strcmp(desired[j].key, key) == 0) {
				found = j;
				break;
			}
		}
		if (found < desired_count) {
			continue;
		}
		snprintf(desired[desired_count].key, sizeof desired[desired_count].key, "%s", key);
		desired_count++;
	}

	if (s_pending_generation != snapshot->opt_asset_generation) {
		ship_pending_discard();
		s_pending_generation = snapshot->opt_asset_generation;
	}

	s_batch_generation = snapshot->opt_asset_generation;
	s_batch_active = 1;
	s_batch_completes_generation = 1;

	for (uint32_t i = 0; i < desired_count; i++) {
		uint32_t found = s_pending_mesh_count;
		for (uint32_t j = 0; j < s_pending_mesh_count; j++) {
			if (strcmp(s_pending_meshes[j].key, desired[i].key) == 0) {
				found = j;
				break;
			}
		}
		if (found < s_pending_mesh_count) {
			continue;
		}
		if (s_pending_mesh_count >= XWA_SNAP_MAX_OPT_ASSETS) {
			Aeron_LogError("xwa.remaster", "mesh asset registry capacity exceeded");
			ship_set_failure(cmd, "Ship model asset registry capacity exceeded");
			return XWA_REMASTER_SHIP_SYNC_FAILED;
		}
		ShipMeshAsset* asset = &s_pending_meshes[s_pending_mesh_count];
		for (uint32_t j = 0; j < s_mesh_count; j++) {
			if (strcmp(s_meshes[j].key, desired[i].key) == 0) {
				*asset = s_meshes[j];
				asset->pending_new = 0;
				s_pending_mesh_count++;
				break;
			}
		}
		if (s_pending_mesh_count > found) {
			continue;
		}
		snprintf(asset->key, sizeof asset->key, "%s", desired[i].key);
		asset->mesh = ship_mesh_load(cmd, asset->key, &asset->runtime_opt);
		if (!asset->mesh) {
			memset(asset, 0, sizeof *asset);
			return XWA_REMASTER_SHIP_SYNC_FAILED;
		}
		asset->pending_new = 1;
		s_pending_mesh_count++;

		AeronCommandBufferUploadUsage usage;
		if (!Aeron_CommandBufferGetUploadUsage(cmd, &usage)) {
			ship_set_failure(cmd, "Could not query ship model upload usage");
			return XWA_REMASTER_SHIP_SYNC_FAILED;
		}
		if ((byte_budget && usage.staged_bytes >= byte_budget) ||
			(copy_budget && usage.copy_count >= copy_budget)) {
			for (uint32_t j = i + 1; j < desired_count; j++) {
				uint32_t pending = s_pending_mesh_count;
				for (uint32_t k = 0; k < s_pending_mesh_count; k++) {
					if (strcmp(s_pending_meshes[k].key, desired[j].key) == 0) {
						pending = k;
						break;
					}
				}
				if (pending == s_pending_mesh_count) {
					s_batch_completes_generation = 0;
					break;
				}
			}
			if (!s_batch_completes_generation) {
				return XWA_REMASTER_SHIP_SYNC_MORE;
			}
		}
	}

	return XWA_REMASTER_SHIP_SYNC_COMPLETE;
}

void XwaRemasterShip_CommitSyncBatch(void) {
	if (!s_batch_active) {
		return;
	}
	if (s_batch_completes_generation) {
		for (uint32_t i = 0; i < s_mesh_count; i++) {
			int retained = 0;
			for (uint32_t j = 0; j < s_pending_mesh_count; j++) {
				if (s_meshes[i].mesh == s_pending_meshes[j].mesh) {
					retained = 1;
					break;
				}
			}
			if (!retained) {
				ship_mesh_asset_destroy(&s_meshes[i]);
			}
		}
		memset(s_meshes, 0, sizeof s_meshes);
		s_mesh_count = s_pending_mesh_count;
		for (uint32_t i = 0; i < s_pending_mesh_count; i++) {
			s_meshes[i] = s_pending_meshes[i];
			s_meshes[i].pending_new = 0;
		}
		memset(s_pending_meshes, 0, sizeof s_pending_meshes);
		s_pending_mesh_count = 0;
		s_pending_generation = UINT64_MAX;
		s_opt_asset_generation = s_batch_generation;
		Aeron_LogDebug("xwa.remaster", "mesh assets: generation=%llu unique=%u",
					   (unsigned long long)s_opt_asset_generation, s_mesh_count);
	}
	s_batch_active = 0;
	s_batch_completes_generation = 0;
}

AeronSceneMesh* XwaRemasterShip_MeshForNameWithSource(const char* name, int* out_runtime_opt) {
	char key[XWA_SNAP_OPT_NAME_MAX];
	if (out_runtime_opt) {
		*out_runtime_opt = 0;
	}
	ship_mesh_key(name, key);
	for (uint32_t i = 0; i < s_mesh_count; i++) {
		if (strcmp(s_meshes[i].key, key) == 0) {
			if (out_runtime_opt) {
				*out_runtime_opt = s_meshes[i].runtime_opt;
			}
			return s_meshes[i].mesh;
		}
	}
	return NULL;
}

AeronSceneMesh* XwaRemasterShip_MeshForName(const char* name) {
	return XwaRemasterShip_MeshForNameWithSource(name, NULL);
}

float XwaRemasterShip_OptProjectileEmissiveStrength(void) { return s_opt_projectile_emissive_strength; }

void XwaRemasterShip_Shutdown(void) {
	ship_pending_discard();
	for (uint32_t i = 0; i < s_mesh_count; i++) {
		ship_mesh_asset_destroy(&s_meshes[i]);
	}
	memset(s_meshes, 0, sizeof s_meshes);
	s_mesh_count = 0;
	s_opt_asset_generation = UINT64_MAX;
	s_batch_active = 0;
	s_batch_completes_generation = 0;
	memset(&g_pbr_tuning, 0, sizeof g_pbr_tuning);
	memset(&g_pbr_tuning_default, 0, sizeof g_pbr_tuning_default);
}

/* 0xAARRGGBB -> linear RGBA floats. */
static void ship_color(uint32_t argb, float out[4]) {
	out[0] = XwaRemaster_SrgbToLinear((float)((argb >> 16) & 0xffu) / 255.0f);
	out[1] = XwaRemaster_SrgbToLinear((float)((argb >> 8) & 0xffu) / 255.0f);
	out[2] = XwaRemaster_SrgbToLinear((float)(argb & 0xffu) / 255.0f);
	out[3] = (float)((argb >> 24) & 0xffu) / 255.0f;
}

/* FS b1 — mirrors cbuffer PbrLightFS (scene_pbr_lighting.hlsli). */
typedef struct PbrLightFS {
	float light_intensity;
	float global_spec_mul;
	float debug_isolate_term;
	float light_wrap;
	float xvt_flat;
	float ssao_intensity;
	float ssao_power;
	float ssao_rt_w;
	float ssao_rt_h;
	float ssao_direct;
	float spec_geom_adapt;
	float _pad_tuning;
	float camera_pos_world[3], _pad0;
	float directional_dir[3], _pad1; /* surface -> light */
	float sun_color[3], _pad2;
	float amb_pos_x[3], _pad3;
	float amb_neg_x[3], _pad4;
	float amb_pos_y[3], _pad5;
	float amb_neg_y[3], _pad6;
	float amb_pos_z[3], _pad7;
	float amb_neg_z[3], _pad8;
	/* Additional diffuse-only directionals (backdrop suns/planets;
	 * the classic sums plain Lambert per light). */
	float extra_dir[3][4];
	float extra_col[3][4];
	/* Point-light evaluation: min distance, spec weight, diffuse wrap, cap. */
	float point_params[4];
	/* Detailed diffuse environment intensity and world-to-local basis. */
	float environment_params[4];
	float environment_right[4];
	float environment_up[4];
	float environment_forward[4];
} PbrLightFS;
typedef char PbrLightFSSizeCheck[sizeof(PbrLightFS) == 368 ? 1 : -1];

void XwaRemasterShip_GetPbrTuning(XwaShipPbrTuning* out) {
	if (out) {
		*out = g_pbr_tuning;
	}
}
void XwaRemasterShip_SetPbrTuning(const XwaShipPbrTuning* in) {
	if (in) {
		g_pbr_tuning = *in;
	}
}
void XwaRemasterShip_ConfigurePbrTuning(const XwaShipPbrTuning* in) {
	if (in) {
		g_pbr_tuning_default = *in;
		g_pbr_tuning = *in;
	}
}
void XwaRemasterShip_GetPbrTuningDefault(XwaShipPbrTuning* out) {
	if (out) {
		*out = g_pbr_tuning_default;
	}
}

void XwaRemasterShip_GetAmbientCube(XwaShipAmbientCube* out) {
	if (!out) {
		return;
	}
	for (int channel = 0; channel < 3; channel++) {
		const float ambient = g_pbr_tuning.ambient[channel];
		out->pos_x[channel] = out->neg_x[channel] = ambient;
		out->pos_y[channel] = out->neg_y[channel] = ambient;
		out->pos_z[channel] = out->neg_z[channel] = ambient;
	}
}

static float ship_directional_light_luminance(const XwaDirLight* light) {
	if (!light || !(light->intensity > 0.0f) || !isfinite(light->intensity)) {
		return 0.0f;
	}
	const float r = XwaRemaster_SrgbToLinear(light->color[0]);
	const float g = XwaRemaster_SrgbToLinear(light->color[1]);
	const float b = XwaRemaster_SrgbToLinear(light->color[2]);
	const float luminance = light->intensity * (0.2126f * r + 0.7152f * g + 0.0722f * b);
	return isfinite(luminance) && luminance > 0.0f ? luminance : 0.0f;
}

static int ship_directional_light_is_sun(const XwaDirLight* light) {
	return light && light->source_backdrop_model_type >= XWA_SNAP_TYPE_BACKDROP_SUN_FIRST &&
		   light->source_backdrop_model_type <= XWA_SNAP_TYPE_BACKDROP_SUN_LAST;
}

const XwaDirLight* XwaRemasterShip_SelectKeyDirectionalLight(const XwaDirLight* lights,
															 uint32_t light_count) {
	if (!lights) {
		return NULL;
	}
	const uint32_t count = light_count > XWA_SNAP_MAX_DIR_LIGHTS ? XWA_SNAP_MAX_DIR_LIGHTS : light_count;
	const XwaDirLight* brightest = NULL;
	const XwaDirLight* brightest_sun = NULL;
	float brightest_luminance = 0.0f;
	float brightest_sun_luminance = 0.0f;
	for (uint32_t i = 0; i < count; i++) {
		const float luminance = ship_directional_light_luminance(&lights[i]);
		if (luminance > brightest_luminance) {
			brightest = &lights[i];
			brightest_luminance = luminance;
		}
		if (ship_directional_light_is_sun(&lights[i]) && luminance > brightest_sun_luminance) {
			brightest_sun = &lights[i];
			brightest_sun_luminance = luminance;
		}
	}
	return brightest_sun ? brightest_sun : brightest;
}

void XwaRemasterShip_SetPbrEnv(AeronScene3D* scene, const XwaDirLight* lights, uint32_t light_count,
							   const float cam_rows[9], const float cam_pos[3], const XwaShipAoParams* ao,
							   const XwaShipPointLightTuning* point_tuning,
							   const XwaShipAmbientCube* ambient_cube,
							   const XwaShipEnvironmentMap* environment_map) {
	if (!scene) {
		return;
	}

	PbrLightFS env = { 0 };
	env.light_intensity = g_pbr_tuning.light_intensity;
	env.debug_isolate_term = g_pbr_tuning.debug_isolate_term;
	if (ao) {
		env.ssao_intensity = ao->intensity;
		env.ssao_power = ao->power;
		env.ssao_rt_w = ao->rt_w;
		env.ssao_rt_h = ao->rt_h;
		env.ssao_direct = ao->direct;
	}
	/* Remaining shading knobs come from the required remaster/config.yaml profile
	 * through the process-wide tuning state. */
	env.global_spec_mul = g_pbr_tuning.global_spec_mul;
	env.light_wrap = g_pbr_tuning.light_wrap;
	env.spec_geom_adapt = g_pbr_tuning.spec_geom_adapt;
	AeronScene_SetPbrDebugViews(scene, env.debug_isolate_term != 0.0f || env.spec_geom_adapt == 0.0f);

	/* HD relight: authored ambient fill (tuning.ambient, linear) plus
	 * sun = decode(classic light color) x intensity. The game state
	 * contributes the art direction (light directions, colors, relative
	 * intensities); the response curve belongs to the shading and
	 * present chain. */

	/* The FS lights in the space the instances were submitted in:
	 * world-space instances pass cam_rows = NULL (light dirs stay
	 * world) + the world camera pos; eye-space instances (preview PiP)
	 * pass their world->eye rows + NULL pos (camera at the origin). */
	if (cam_pos) {
		memcpy(env.camera_pos_world, cam_pos, sizeof env.camera_pos_world);
	}
	if (ambient_cube) {
		memcpy(env.amb_pos_x, ambient_cube->pos_x, sizeof env.amb_pos_x);
		memcpy(env.amb_neg_x, ambient_cube->neg_x, sizeof env.amb_neg_x);
		memcpy(env.amb_pos_y, ambient_cube->pos_y, sizeof env.amb_pos_y);
		memcpy(env.amb_neg_y, ambient_cube->neg_y, sizeof env.amb_neg_y);
		memcpy(env.amb_pos_z, ambient_cube->pos_z, sizeof env.amb_pos_z);
		memcpy(env.amb_neg_z, ambient_cube->neg_z, sizeof env.amb_neg_z);
	} else {
		for (int c = 0; c < 3; c++) {
			const float amb = g_pbr_tuning.ambient[c];
			env.amb_pos_x[c] = env.amb_neg_x[c] = amb;
			env.amb_pos_y[c] = env.amb_neg_y[c] = amb;
			env.amb_pos_z[c] = env.amb_neg_z[c] = amb;
		}
	}
	if (environment_map && environment_map->texture && environment_map->sampler &&
		isfinite(environment_map->strength) && environment_map->strength > 0.0f) {
		const float* const source_basis[3] = {
			environment_map->right, environment_map->up, environment_map->forward
		};
		float* const destination_basis[3] = {
			env.environment_right, env.environment_up, env.environment_forward
		};
		for (int basis = 0; basis < 3; basis++) {
			if (cam_rows) {
				for (int row = 0; row < 3; row++) {
					destination_basis[basis][row] =
						cam_rows[row * 3 + 0] * source_basis[basis][0] +
						cam_rows[row * 3 + 1] * source_basis[basis][1] +
						cam_rows[row * 3 + 2] * source_basis[basis][2];
				}
			} else {
				memcpy(destination_basis[basis], source_basis[basis], 3 * sizeof(float));
			}
		}
		env.environment_params[0] = environment_map->strength;
		AeronScene_SetPbrEnvironmentMap(scene, environment_map->texture, environment_map->sampler);
	}

	/* A backdrop sun is the key even when a planet has slightly greater
	 * intensity. The classic sums plain Lambert per ADDITIONAL light, so
	 * the three brightest remaining lights feed diffuse-only extra slots.
	 * Each light's color is decoded to linear and scaled by its
	 * intensity. */
	const XwaDirLight* sorted[XWA_SNAP_MAX_DIR_LIGHTS];
	float sorted_luminance[XWA_SNAP_MAX_DIR_LIGHTS];
	const uint32_t n =
		!lights ? 0 : (light_count > XWA_SNAP_MAX_DIR_LIGHTS ? XWA_SNAP_MAX_DIR_LIGHTS : light_count);
	const XwaDirLight* key = XwaRemasterShip_SelectKeyDirectionalLight(lights, n);
	for (uint32_t i = 0; i < n; i++) {
		sorted[i] = &lights[i];
		sorted_luminance[i] = ship_directional_light_luminance(sorted[i]);
		for (uint32_t j = i; j > 0 && sorted_luminance[j] > sorted_luminance[j - 1]; j--) {
			const XwaDirLight* t = sorted[j];
			sorted[j] = sorted[j - 1];
			sorted[j - 1] = t;
			const float score = sorted_luminance[j];
			sorted_luminance[j] = sorted_luminance[j - 1];
			sorted_luminance[j - 1] = score;
		}
	}
	if (key) {
		uint32_t key_index = 0;
		while (key_index < n && sorted[key_index] != key) {
			key_index++;
		}
		for (; key_index > 0; key_index--) {
			sorted[key_index] = sorted[key_index - 1];
		}
		sorted[0] = key;
	}
	for (uint32_t li = 0; li < n && li < 4; li++) {
		const XwaDirLight* l = sorted[li];
		float d[3];
		if (cam_rows) {
			/* eye = (R0.w, R1.w, R2.w) — world->eye by the view's g_camMat. */
			for (int r = 0; r < 3; r++) {
				d[r] = cam_rows[r * 3 + 0] * l->world_dir[0] + cam_rows[r * 3 + 1] * l->world_dir[1] +
					   cam_rows[r * 3 + 2] * l->world_dir[2];
			}
		} else {
			memcpy(d, l->world_dir, sizeof d);
		}
		const float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
		if (len > 0.0001f) {
			d[0] /= len;
			d[1] /= len;
			d[2] /= len;
		}
		float col[3];
		for (int c = 0; c < 3; c++) {
			col[c] = XwaRemaster_SrgbToLinear(l->color[c]) * l->intensity;
		}
		if (li == 0) {
			memcpy(env.directional_dir, d, sizeof d);
			memcpy(env.sun_color, col, sizeof col);
		} else {
			memcpy(env.extra_dir[li - 1], d, sizeof d);
			memcpy(env.extra_col[li - 1], col, sizeof col);
		}
	}
	if (point_tuning) {
		env.point_params[0] = point_tuning->min_distance;
		env.point_params[1] = point_tuning->spec_weight;
		env.point_params[2] = point_tuning->diffuse_wrap;
		env.point_params[3] = point_tuning->contrib_cap;
	}
	AeronScene_SetFrameUniformData(scene, AERON_SHADER_STAGE_FRAGMENT, 1, &env, sizeof env);
}

/* 3x4 affine helpers. */
static void ship_mat3x4_identity(float out[3][4]) {
	memset(out, 0, 12 * sizeof(float));
	out[0][0] = out[1][1] = out[2][2] = 1.0f;
}

static void ship_mat3x4_rotation_about_pivot(float out[3][4], const float axis[3], const float pivot[3],
											 float angle) {
	float ax = axis[0], ay = axis[1], az = axis[2];
	const float len = sqrtf(ax * ax + ay * ay + az * az);
	if (len < 1e-4f) {
		ship_mat3x4_identity(out);
		return;
	}
	ax /= len;
	ay /= len;
	az /= len;
	const float c = cosf(angle), s = sinf(angle), omc = 1.0f - c;
	out[0][0] = c + ax * ax * omc;
	out[0][1] = ax * ay * omc - az * s;
	out[0][2] = ax * az * omc + ay * s;
	out[1][0] = ay * ax * omc + az * s;
	out[1][1] = c + ay * ay * omc;
	out[1][2] = ay * az * omc - ax * s;
	out[2][0] = az * ax * omc - ay * s;
	out[2][1] = az * ay * omc + ax * s;
	out[2][2] = c + az * az * omc;
	for (int r = 0; r < 3; r++) {
		out[r][3] = pivot[r] - (out[r][0] * pivot[0] + out[r][1] * pivot[1] + out[r][2] * pivot[2]);
	}
}

/* Compose two affine transforms using the shader's row-major convention:
 * out(v) = lhs(rhs(v)). */
static void ship_mat3x4_mul(float out[3][4], const float lhs[3][4], const float rhs[3][4]) {
	float result[3][4];
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			result[r][c] = lhs[r][0] * rhs[0][c] + lhs[r][1] * rhs[1][c] + lhs[r][2] * rhs[2][c];
		}
		result[r][3] = lhs[r][0] * rhs[0][3] + lhs[r][1] * rhs[1][3] + lhs[r][2] * rhs[2][3] + lhs[r][3];
	}
	memcpy(out, result, sizeof result);
}

static int ship_bwing_bridge_slot(const AeronSceneMesh* mesh, uint32_t slots) {
	if (!mesh) {
		return -1;
	}
	for (uint32_t mi = 0; mi < slots; mi++) {
		if (mesh->mesh_rot[mi].mesh_type == XWA_SNAP_MESH_BRIDGE) {
			return (int)mi;
		}
	}
	return -1;
}

static int ship_bwing_compensation(const AeronSceneMesh* mesh, const XwaFlightObject* f, uint32_t slots,
								   float out[3][4]) {
	if (f->object_type != XWA_SNAP_TYPE_BWING) {
		return 0;
	}
	const int bridge_slot = ship_bwing_bridge_slot(mesh, slots);
	if (bridge_slot < 0 || f->mesh_rotation[bridge_slot] == 0) {
		return 0;
	}
	/* The classic B-Wing walker rotates every mesh root by the Bridge
	 * rotation around model -Y before applying that root's local rotation. */
	static const float axis[3] = { 0.0f, -1.0f, 0.0f };
	static const float pivot[3] = { 0.0f, 0.0f, 0.0f };
	const float angle = (float)f->mesh_rotation[bridge_slot] * (-2.0f * 3.14159265358979323846f / 256.0f);
	ship_mat3x4_rotation_about_pivot(out, axis, pivot, angle);
	return 1;
}

int XwaRemasterShip_BuildMeshTable(const AeronSceneMesh* mesh, const XwaFlightObject* f,
								   AeronSceneMeshTable* out) {
	if (!mesh || !f || !out || !f->has_craft) {
		return 0;
	}
	const uint32_t slots =
		AERON_MAX_MESH_SLOTS < XWA_SNAP_MAX_MESH_SLOTS ? AERON_MAX_MESH_SLOTS : XWA_SNAP_MAX_MESH_SLOTS;
	float bwing_compensation[3][4];
	const int has_bwing_compensation = ship_bwing_compensation(mesh, f, slots, bwing_compensation);
	for (uint32_t mi = 0; mi < AERON_MAX_MESH_SLOTS; mi++) {
		if (has_bwing_compensation) {
			memcpy(out->rows[mi], bwing_compensation, sizeof out->rows[mi]);
		} else {
			ship_mat3x4_identity(out->rows[mi]);
		}
		out->visibility_packed[mi >> 2][mi & 3] = 1.0f;
		out->highlight_packed[mi >> 2][mi & 3] = 0.0f;
		out->markings_packed[mi >> 2][mi & 3] = 0.0f;
		out->emissive_packed[mi >> 2][mi & 3] = 1.0f;
	}
	int any = has_bwing_compensation;
	for (uint32_t mi = 0; mi < slots; mi++) {
		/* Classic blown-off gate: the node walk SKIPS a mesh when
		 * componentState[ordinal-1] != 0 (render_scene_core). */
		if (f->component_state[mi] != 0) {
			out->visibility_packed[mi >> 2][mi & 3] = 0.0f;
			any = 1;
			continue;
		}
		const AeronMeshRot* r = &mesh->mesh_rot[mi];
		if (!r->has_rotation || f->mesh_rotation[mi] == 0) {
			continue;
		}
		/* Classic: mesh.rotAngle = meshRotation[ordinal-1] * 2pi/256
		 * about the OPT rotation node's pivot/axis — but the classic
		 * chain NETS a rotation by -rotAngle on the vertices:
		 * Math3D_BuildAxisAngleMatrix stores R(angle)^T, ApplyRotation-
		 * Scale accumulates via MulMatrix3x3T, and the model->eye
		 * transform consumes viewOrient transposed (RotateVec3), an odd
		 * number of transposes. EngineGlow_MeshRotationAngleQ16's
		 * explicit -meshRotation corroborates. Our table is consumed
		 * straight (v' = R v), so negate here. */
		const float angle = (float)f->mesh_rotation[mi] * (-2.0f * 3.14159265358979323846f / 256.0f);
		float local_rotation[3][4];
		ship_mat3x4_rotation_about_pivot(local_rotation, r->axis, r->pivot, angle);
		if (has_bwing_compensation) {
			ship_mat3x4_mul(out->rows[mi], bwing_compensation, local_rotation);
		} else {
			memcpy(out->rows[mi], local_rotation, sizeof out->rows[mi]);
		}
		any = 1;
	}
	return any;
}

int XwaRemasterShip_BuildPreviousMeshTable(const AeronSceneMesh* mesh, const XwaFlightObject* current,
										   const XwaFlightObject* previous, AeronSceneMeshTable* out) {
	if (!mesh || !current || !previous || !out || !current->has_craft || !previous->has_craft ||
		current->object_type != previous->object_type) {
		return 0;
	}
	if (current->object_type != XWA_SNAP_TYPE_BWING && !mesh->has_any_rotation) {
		return 0;
	}
	const uint32_t slots =
		AERON_MAX_MESH_SLOTS < XWA_SNAP_MAX_MESH_SLOTS ? AERON_MAX_MESH_SLOTS : XWA_SNAP_MAX_MESH_SLOTS;
	if (current->object_type == XWA_SNAP_TYPE_BWING) {
		const int bridge_slot = ship_bwing_bridge_slot(mesh, slots);
		/* Bridge rotation drives every root, independently of whether the
		 * Bridge mesh itself has a usable local rotation node. */
		if (bridge_slot >= 0 && current->mesh_rotation[bridge_slot] != previous->mesh_rotation[bridge_slot]) {
			XwaRemasterShip_BuildMeshTable(mesh, previous, out);
			return 1;
		}
	}
	for (uint32_t mi = 0; mi < slots; ++mi) {
		if (mesh->mesh_rot[mi].has_rotation && current->mesh_rotation[mi] != previous->mesh_rotation[mi]) {
			/* BuildMeshTable always initializes out. Its zero return is expected
			 * when the previous pose is identity and the current pose is not. */
			XwaRemasterShip_BuildMeshTable(mesh, previous, out);
			return 1;
		}
	}
	return 0;
}

int XwaRemasterShip_BuildDebrisMeshTable(const XwaFlightObject* f, AeronSceneMeshTable* out) {
	if (!f || !out) {
		return 0;
	}
	const uint32_t comp = (uint32_t)(f->type_specific_0 >> 1);
	if (comp >= AERON_MAX_MESH_SLOTS) {
		return 0;
	}
	for (uint32_t mi = 0; mi < AERON_MAX_MESH_SLOTS; mi++) {
		ship_mat3x4_identity(out->rows[mi]);
		out->visibility_packed[mi >> 2][mi & 3] = mi == comp ? 1.0f : 0.0f;
		out->highlight_packed[mi >> 2][mi & 3] = 0.0f;
		out->markings_packed[mi >> 2][mi & 3] = 0.0f;
		out->emissive_packed[mi >> 2][mi & 3] = 1.0f;
	}
	if (f->spin_angle != 0) {
		/* The classic draw-site spin (render_scene_core debris block)
		 * runs the same BuildAxisAngleMatrix/MulMatrix3x3(T) chain as
		 * the rotary meshes, netting -angle on the vertices — negate
		 * for the straight-consumed table (see BuildMeshTable). It
		 * composes with the basis spin FVIEW_calcrotateorient already
		 * applied (the captured rows / Euler mirror include it) — the
		 * classic applies both. renderOffset/spin_axis are OPT-native
		 * model space, the cooked mesh's runtime space (the loader's
		 * swap_yz3 round-trips the glTF axis swap). */
		const float angle = (float)f->spin_angle * (-2.0f * 3.14159265358979323846f / 65536.0f);
		ship_mat3x4_rotation_about_pivot(out->rows[comp], f->spin_axis, f->render_offset, angle);
	}
	return 1;
}

int XwaRemasterShip_BuildCockpitMeshTable(const AeronSceneMesh* mesh, const XwaCockpit* c,
										  const XwaFlightObject* anchor, AeronSceneMeshTable* out) {
	if (!mesh || !c || !anchor || !out || !anchor->has_craft) {
		return 0;
	}
	if (c->seat == 0) {
		/* The classic swaps only the model handle, then walks the cockpit
		 * OPT with the anchor ObjectRecord's live craft state. */
		return XwaRemasterShip_BuildMeshTable(mesh, anchor, out);
	}

	const uint32_t slots =
		AERON_MAX_MESH_SLOTS < XWA_SNAP_MAX_MESH_SLOTS ? AERON_MAX_MESH_SLOTS : XWA_SNAP_MAX_MESH_SLOTS;
	float bwing_compensation[3][4];
	const int has_bwing_compensation =
		ship_bwing_compensation(mesh, anchor, slots, bwing_compensation);
	for (uint32_t mi = 0; mi < AERON_MAX_MESH_SLOTS; mi++) {
		if (has_bwing_compensation) {
			memcpy(out->rows[mi], bwing_compensation, sizeof out->rows[mi]);
		} else {
			ship_mat3x4_identity(out->rows[mi]);
		}
		out->visibility_packed[mi >> 2][mi & 3] = 1.0f;
		out->highlight_packed[mi >> 2][mi & 3] = 0.0f;
		out->markings_packed[mi >> 2][mi & 3] = 0.0f;
		out->emissive_packed[mi >> 2][mi & 3] = 1.0f;
	}
	const float q16 = 2.0f * 3.14159265358979323846f / 65536.0f;
	int any = has_bwing_compensation;
	/* The classic node walk carries the beam rotation into subsequent
	 * gun meshes, and half of it into launcher meshes. */
	const AeronMeshRot* beam_rot = NULL;
	for (uint32_t mi = 0; mi < slots; mi++) {
		if (anchor->component_state[mi] != 0) {
			out->visibility_packed[mi >> 2][mi & 3] = 0.0f;
			any = 1;
			continue;
		}
		const AeronMeshRot* r = &mesh->mesh_rot[mi];
		if (!r->has_rotation) {
			continue;
		}
		if (r->mesh_type == XWA_SNAP_MESH_ROTARY_BEAM) {
			beam_rot = r;
			if (c->aim_angle_b != 0) {
				float local_rotation[3][4];
				ship_mat3x4_rotation_about_pivot(local_rotation, r->axis, r->pivot,
											 (float)c->aim_angle_b * q16);
				if (has_bwing_compensation) {
					ship_mat3x4_mul(out->rows[mi], bwing_compensation, local_rotation);
				} else {
					memcpy(out->rows[mi], local_rotation, sizeof out->rows[mi]);
				}
				any = 1;
			}
			continue;
		}

		if (r->mesh_type == XWA_SNAP_MESH_ROTARY_GUN_TURRET ||
			r->mesh_type == XWA_SNAP_MESH_ROTARY_LAUNCHER) {
			const int inherit_half = r->mesh_type == XWA_SNAP_MESH_ROTARY_LAUNCHER;
			const float inherited_angle =
				beam_rot ? (float)c->aim_angle_b * q16 * (inherit_half ? 0.5f : 1.0f) : 0.0f;
			float own[3][4];
			float local_rotation[3][4];
			ship_mat3x4_identity(own);
			if (c->aim_angle_a != 0) {
				ship_mat3x4_rotation_about_pivot(own, r->axis, r->pivot, (float)-c->aim_angle_a * q16);
			}
			if (inherited_angle != 0.0f) {
				float inherited[3][4];
				ship_mat3x4_rotation_about_pivot(inherited, beam_rot->axis, beam_rot->pivot, inherited_angle);
				ship_mat3x4_mul(local_rotation, inherited, own);
			} else if (c->aim_angle_a != 0) {
				memcpy(local_rotation, own, sizeof local_rotation);
			}
			if (c->aim_angle_a != 0 || inherited_angle != 0.0f) {
				if (has_bwing_compensation) {
					ship_mat3x4_mul(out->rows[mi], bwing_compensation, local_rotation);
				} else {
					memcpy(out->rows[mi], local_rotation, sizeof out->rows[mi]);
				}
				any = 1;
			}
		}
	}
	return any;
}

int XwaRemasterShip_BuildPreviousCockpitMeshTable(
	const AeronSceneMesh* mesh, const XwaCockpit* current_cockpit,
	const XwaFlightObject* current_anchor, const XwaCockpit* previous_cockpit,
	const XwaFlightObject* previous_anchor, AeronSceneMeshTable* out) {
	if (!mesh || !current_cockpit || !current_anchor || !previous_cockpit || !previous_anchor || !out ||
		!current_anchor->has_craft || !previous_anchor->has_craft ||
		current_anchor->object_type != previous_anchor->object_type ||
		current_cockpit->seat != previous_cockpit->seat) {
		return 0;
	}

	if (current_cockpit->seat == 0) {
		if (memcmp(current_anchor->component_state, previous_anchor->component_state,
				   sizeof current_anchor->component_state) != 0) {
			XwaRemasterShip_BuildMeshTable(mesh, previous_anchor, out);
			return 1;
		}
		return XwaRemasterShip_BuildPreviousMeshTable(mesh, current_anchor, previous_anchor, out);
	}

	int changed = current_cockpit->aim_angle_a != previous_cockpit->aim_angle_a ||
				  current_cockpit->aim_angle_b != previous_cockpit->aim_angle_b ||
				  memcmp(current_anchor->component_state, previous_anchor->component_state,
						 sizeof current_anchor->component_state) != 0;
	if (!changed && current_anchor->object_type == XWA_SNAP_TYPE_BWING) {
		const uint32_t slots = AERON_MAX_MESH_SLOTS < XWA_SNAP_MAX_MESH_SLOTS
							   ? AERON_MAX_MESH_SLOTS
							   : XWA_SNAP_MAX_MESH_SLOTS;
		const int bridge_slot = ship_bwing_bridge_slot(mesh, slots);
		changed = bridge_slot >= 0 &&
				  current_anchor->mesh_rotation[bridge_slot] != previous_anchor->mesh_rotation[bridge_slot];
	}
	if (!changed) {
		return 0;
	}

	/* The previous pose may be identity; the initialized table is still
	 * required when the current cockpit has just begun moving. */
	XwaRemasterShip_BuildCockpitMeshTable(mesh, previous_cockpit, previous_anchor, out);
	return 1;
}

/* Small xorshift for the classic glow flicker — cosmetic randomness
 * that classically came from the render-side GameRand2 (which the
 * read-only snapshot capture must not consume). */
static uint32_t ship_flicker_rand(void) {
	static uint32_t st = 0x9E3779B9u;
	st ^= st << 13;
	st ^= st >> 17;
	st ^= st << 5;
	return st;
}

float XwaRemasterShip_EngineGlowScale(const XwaFlightObject* f) {
	if (!f || !f->has_craft || !f->eg_output_scale || !f->eg_working) {
		return 0.0f;
	}
	if (f->object_kind != 5 && f->object_kind != 6) {
		/* Power-margin formula (EngineGlow_RenderObjectGlows): margin =
		 * 16 - redirects, scaled by engineOutputScale and throttle,
		 * ~6% flicker, floor 0.35. */
		const int margin =
			16 - (int)f->eg_shield_redirect - (int)f->eg_laser_redirect - (int)f->eg_beam_level;
		const float redirect =
			(float)((double)margin * 0.0625 * ((double)f->eg_output_scale * 0.000015259022));
		float s = (float)((double)f->eg_throttle * 0.00001525902189669642 *
						  ((1.0 - (double)(ship_flicker_rand() & 0xf) * 0.0040000002) * (double)redirect));
		if (s < 0.34999999f) {
			s = 0.34999999f;
		}
		return s;
	}
	/* objectKind 5/6: speed-boost window over maxSpeedCache. */
	const double boost_window = (double)(4 * (3600 - (int)f->eg_max_speed));
	const double speed_delta = (double)((int)f->speed - (int)f->eg_max_speed);
	if (boost_window <= 0.0) {
		return 12.0f;
	}
	if (speed_delta <= boost_window) {
		return 1.0f - (float)(speed_delta / boost_window) * -11.0f;
	}
	return 12.0f;
}

void XwaRemasterShip_SubmitEngineGlows(AeronScene3D* scene, const AeronSceneMesh* mesh,
									   const float transform[16], float model_scale,
									   const AeronSceneMeshTable* table, uint32_t knockout_mask, float scale,
									   const float crows[9], const float cam_pos[3], const XwaAssetRef* tex) {
	if (!scene || !mesh || !mesh->engine_glow_count || !transform || scale <= 0.0f ||
		s_engine_emissive_strength <= 0.0f || !tex || !tex->texture) {
		return;
	}
	const float k = model_scale > 0.0f ? model_scale : 1.0f;
	/* Classic corner UVs, RenderQuad_DrawGlow rim order; center UV is
	 * the sub-rect middle (scene derives center pos/UV as averages). */
	static const float rim_uv[4][2] = {
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f },
	};
	const float du = tex->u1 - tex->u0;
	const float dv = tex->v1 - tex->v0;

	for (uint32_t gi = 0; gi < mesh->engine_glow_count; gi++) {
		const AeronFlightEngineGlow* g = &mesh->engine_glows[gi];
		if (!g->enabled || (gi < 32 && (knockout_mask & (1u << gi)))) {
			continue;
		}

		/* Emitter anchor + axes in model space, articulated by the
		 * instance's mesh table (the same affine the mesh's vertices
		 * get in the VS — glows follow their rotary mesh exactly). */
		float p[3] = { g->position.x, g->position.y, g->position.z };
		float ax_look[3] = { g->look.x, g->look.y, g->look.z };
		float ax_right[3] = { g->right.x, g->right.y, g->right.z };
		float ax_up[3] = { g->up.x, g->up.y, g->up.z };
		if (table && g->component_index < AERON_MAX_MESH_SLOTS) {
			const float (*rw)[4] = table->rows[g->component_index];
			float tp[3], tv[3];
			for (int r = 0; r < 3; r++) {
				tp[r] = rw[r][0] * p[0] + rw[r][1] * p[1] + rw[r][2] * p[2] + rw[r][3];
			}
			memcpy(p, tp, sizeof tp);
			for (int r = 0; r < 3; r++)
				tv[r] = rw[r][0] * ax_look[0] + rw[r][1] * ax_look[1] + rw[r][2] * ax_look[2];
			memcpy(ax_look, tv, sizeof tv);
			for (int r = 0; r < 3; r++)
				tv[r] = rw[r][0] * ax_right[0] + rw[r][1] * ax_right[1] + rw[r][2] * ax_right[2];
			memcpy(ax_right, tv, sizeof tv);
			for (int r = 0; r < 3; r++)
				tv[r] = rw[r][0] * ax_up[0] + rw[r][1] * ax_up[1] + rw[r][2] * ax_up[2];
			memcpy(ax_up, tv, sizeof tv);
		}

		/* Model -> instance space (transform is fl_model_matrix layout:
		 * rows of the 3x4 are the space-axes' model components; scale k
		 * rides the matrix — axes divide it back out). */
		float sp[3], s_look[3], s_right[3], s_up[3];
		for (int r = 0; r < 3; r++) {
			sp[r] = transform[r * 4 + 0] * p[0] + transform[r * 4 + 1] * p[1] + transform[r * 4 + 2] * p[2] +
					transform[r * 4 + 3];
			s_look[r] = (transform[r * 4 + 0] * ax_look[0] + transform[r * 4 + 1] * ax_look[1] +
						 transform[r * 4 + 2] * ax_look[2]) /
						k;
			s_right[r] = (transform[r * 4 + 0] * ax_right[0] + transform[r * 4 + 1] * ax_right[1] +
						  transform[r * 4 + 2] * ax_right[2]) /
						 k;
			s_up[r] = (transform[r * 4 + 0] * ax_up[0] + transform[r * 4 + 1] * ax_up[1] +
					   transform[r * 4 + 2] * ax_up[2]) /
					  k;
		}

		/* Space -> view (identity camera when crows/cam_pos are NULL). */
		float c[3], look_v[3], right_v[3], up_v[3];
		if (crows && cam_pos) {
			const float d0 = sp[0] - cam_pos[0], d1 = sp[1] - cam_pos[1], d2 = sp[2] - cam_pos[2];
			for (int r = 0; r < 3; r++) {
				c[r] = crows[r * 3 + 0] * d0 + crows[r * 3 + 1] * d1 + crows[r * 3 + 2] * d2;
				look_v[r] = crows[r * 3 + 0] * s_look[0] + crows[r * 3 + 1] * s_look[1] +
							crows[r * 3 + 2] * s_look[2];
				right_v[r] = crows[r * 3 + 0] * s_right[0] + crows[r * 3 + 1] * s_right[1] +
							 crows[r * 3 + 2] * s_right[2];
				up_v[r] =
					crows[r * 3 + 0] * s_up[0] + crows[r * 3 + 1] * s_up[1] + crows[r * 3 + 2] * s_up[2];
			}
		} else {
			memcpy(c, sp, sizeof c);
			memcpy(look_v, s_look, sizeof look_v);
			memcpy(right_v, s_right, sizeof right_v);
			memcpy(up_v, s_up, sizeof up_v);
		}

		/* Classic culls (EngineGlow_BuildProjectedQuad): behind/at the
		 * eye plane, or projected size under a pixel (max dim x 256 /
		 * viewZ, the classic focal). Dims scale with the model. */
		const float dim_x = k * g->dimensions.x;
		const float dim_y = k * g->dimensions.y;
		const float dim_z = k * g->dimensions.z;
		if (c[2] < 1.0f) {
			continue;
		}
		float maxdim = dim_x > dim_y ? dim_x : dim_y;
		if (dim_z > maxdim) {
			maxdim = dim_z;
		}
		if (maxdim * 256.0f / c[2] < 1.0f) {
			continue;
		}

		/* Corner build — rect for elongated dims, view-aligned diamond
		 * for round ones (ratio in (0.85, 1.2)); the classic clamps the
		 * geometric scale at 0.8. */
		const float clamped = scale >= 0.80000001f ? 0.80000001f : scale;
		float corners[4][3];
		const float ratio = g->dimensions.y != 0.0f ? g->dimensions.x / g->dimensions.y : 1.0f;
		if (ratio <= 0.85000002f || ratio >= 1.2f) {
			/* Classic axis pairing (EngineGlow_BuildRectQuadCorners via
			 * BuildProjectedQuad's call: firstAxisView = the rotated
			 * RIGHT axis rides the `upAxisView` param with dims.x, the
			 * UP axis the `rightAxisView` param with dims.y). */
			const float as = dim_x * clamped * 0.5f;
			const float bs = dim_y * clamped * 0.5f;
			for (int r = 0; r < 3; r++) {
				const float a = right_v[r] * as;
				const float b = up_v[r] * bs;
				corners[0][r] = c[r] - a + b;
				corners[1][r] = c[r] + a + b;
				corners[2][r] = c[r] + a - b;
				corners[3][r] = c[r] - a - b;
			}
		} else {
			const float radius = clamped * dim_x * 1.415f * 0.5f;
			const float dot_f = right_v[0] * c[0] + right_v[1] * c[1] + right_v[2] * c[2];
			const float dot_s = up_v[0] * c[0] + up_v[1] * c[1] + up_v[2] * c[2];
			const float len = sqrtf(dot_f * dot_f + dot_s * dot_s);
			float major[3], minor[3];
			if (len == 0.0f) {
				memcpy(major, right_v, sizeof major);
				memcpy(minor, up_v, sizeof minor);
			} else {
				const float fw = dot_f / len;
				const float sw = dot_s / len;
				for (int r = 0; r < 3; r++) {
					major[r] = up_v[r] * sw + right_v[r] * fw;
					minor[r] = right_v[r] * sw - up_v[r] * fw;
				}
			}
			for (int r = 0; r < 3; r++) {
				corners[0][r] = c[r] + minor[r] * radius;
				corners[1][r] = c[r] + major[r] * radius;
				corners[2][r] = c[r] - minor[r] * radius;
				corners[3][r] = c[r] - major[r] * radius;
			}
		}

		/* Look-axis depth extrusion (EngineGlow_ExtrudeQuadAlongViewNormal):
		 * per corner, push along the look axis by the corner's depth
		 * delta from the center — the facing-dependent sign and the
		 * asymmetric gains reproduce the classic exactly (10000/32768
		 * on the negative side, scale x dim.z on the positive). */
		{
			const float center_dot = look_v[0] * c[0] + look_v[1] * c[1] + look_v[2] * c[2];
			const float pos_gain = scale * dim_z;
			for (int ci = 0; ci < 4; ci++) {
				const float z_delta = center_dot < 0.0f ? corners[ci][2] - c[2] : c[2] - corners[ci][2];
				const float gain = z_delta < 0.0f ? (10000.0f / 32768.0f) : pos_gain;
				for (int r = 0; r < 3; r++) {
					corners[ci][r] += look_v[r] * z_delta * gain;
				}
			}
		}

		/* Colors: glb extras are sRGB 0..1 (decoded from the OPT's
		 * 0xAARRGGBB) — linearize like every other classic color. The
		 * glow atlas is PREMULTIPLIED (imgbake KTX2 convention), so the
		 * PMA blend + tint RGB pre-scaled by tint alpha keep the
		 * classic straight-alpha modulate (straight blending crushed
		 * the soft glow rims by applying alpha twice). Emissive strength
		 * boosts HDR RGB without changing alpha coverage. */
		float core[4], outer[4];
		core[3] = g->core_rgba[3];
		outer[3] = g->outer_rgba[3];
		for (int ch = 0; ch < 3; ch++) {
			core[ch] = XwaRemaster_SrgbToLinear(g->core_rgba[ch]) * core[3] * s_engine_emissive_strength;
			outer[ch] = XwaRemaster_SrgbToLinear(g->outer_rgba[ch]) * outer[3] * s_engine_emissive_strength;
		}

		AeronSceneBillboardDesc d;
		memset(&d, 0, sizeof d);
		d.texture = tex->texture;
		d.blend = AERON_SCENE_BILLBOARD_BLEND_PMA;
		d.stage = AERON_SCENE_BILLBOARD_STAGE_OVERLAY;
		d.center_color = core;
		for (int v = 0; v < 4; v++) {
			/* View corners -> submission space (inverse of the camera
			 * rotation above; identity camera passes through). */
			if (crows && cam_pos) {
				for (int r = 0; r < 3; r++) {
					d.corners[v][r] = cam_pos[r] + crows[0 * 3 + r] * corners[v][0] +
									  crows[1 * 3 + r] * corners[v][1] + crows[2 * 3 + r] * corners[v][2];
				}
			} else {
				memcpy(d.corners[v], corners[v], sizeof d.corners[v]);
			}
			d.uv[v][0] = tex->u0 + rim_uv[v][0] * du;
			d.uv[v][1] = tex->v0 + rim_uv[v][1] * dv;
			memcpy(d.colors[v], outer, sizeof outer);
		}
		AeronScene_AddBillboard(scene, &d);
	}
}

uint32_t XwaRemasterShip_CollectEngineGlowPointLights(const AeronSceneMesh* mesh, const float transform[16],
													  const AeronSceneMeshTable* table,
													  const XwaFlightObject* f, XwaShipPointLight* out,
													  uint32_t max, uint32_t* dropped) {
	if (dropped) {
		*dropped = 0;
	}
	if (!mesh || !mesh->engine_glow_count || !transform || !f || (!out && max > 0)) {
		return 0;
	}
	/* Classic engineScale for the LIGHT law
	 * (FlightLight_AppendScenePointLightForObject's modelIndex branch):
	 * power margin x output scale only — no throttle term, no flicker,
	 * no floor (all of which belong to the glow SIZE law). */
	const float engine_scale = (float)(16 - f->eg_laser_redirect - f->eg_shield_redirect - f->eg_beam_level) *
							   0.0625f * ((float)f->eg_output_scale * 0.000015259022f);
	if (engine_scale <= 0.0f) {
		return 0;
	}

	uint32_t n = 0;
	for (uint32_t gi = 0; gi < mesh->engine_glow_count; gi++) {
		const AeronFlightEngineGlow* g = &mesh->engine_glows[gi];
		/* Classic gates: disabled emitters and small glows skip (raw
		 * OPT dims; only LARGE engines — capitals — light their hull).
		 * The classic light law does NOT consult damage knockouts. */
		const float dim_x = g->dimensions.x * AERON_OPT_UNITS_PER_METER;
		const float dim_y = g->dimensions.y * AERON_OPT_UNITS_PER_METER;
		const float dim_z = g->dimensions.z * AERON_OPT_UNITS_PER_METER;
		if (!g->enabled || (dim_x <= 2000.0f && dim_y <= 2000.0f)) {
			continue;
		}
		const float intensity = dim_z * engine_scale * 300.0f;
		if (intensity <= 0.0f) {
			continue;
		}
		if (n >= max) {
			if (dropped) {
				(*dropped)++;
			}
			continue;
		}

		/* Anchor: mesh-table articulation + instance transform (the
		 * same chain the glow quads use). */
		float p[3] = { g->position.x, g->position.y, g->position.z };
		if (table && g->component_index < AERON_MAX_MESH_SLOTS) {
			const float (*rw)[4] = table->rows[g->component_index];
			float tp[3];
			for (int r = 0; r < 3; r++) {
				tp[r] = rw[r][0] * p[0] + rw[r][1] * p[1] + rw[r][2] * p[2] + rw[r][3];
			}
			memcpy(p, tp, sizeof tp);
		}
		XwaShipPointLight* l = &out[n++];
		for (int r = 0; r < 3; r++) {
			l->pos[r] = transform[r * 4 + 0] * p[0] + transform[r * 4 + 1] * p[1] +
						transform[r * 4 + 2] * p[2] + transform[r * 4 + 3];
		}
		/* Visibility-law window (classic 0.5/d has no shading cutoff):
		 * ~1% floor at 50 * intensity, never below the classic cull radius. */
		{
			const float cull = engine_scale * 16384.0f;
			float rng = 50.0f * intensity;
			if (rng < cull) {
				rng = cull;
			}
			l->range = rng;
		}
		for (int c = 0; c < 3; c++) {
			l->color[c] = XwaRemaster_SrgbToLinear(g->core_rgba[c]) * intensity;
		}
	}
	return n;
}
