#include "xwa_remaster/hud_cmd.h"

#include "aeron/aeron.h"
#include "aeron/scene/billboard.h"
#include "aeron/scene/draw_list2d.h"
#include "aeron/scene/present.h"
#include "aeron/scene/scene3d.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/effects.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/glow_marks.h"
#include "xwa_remaster/hud_cmd_math.h"
#include "xwa_remaster/hud_layout.h"
#include "xwa_remaster/ship.h"
#include "xwa_remaster/world.h"
#include "xwa_remaster/xwa_remaster.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CMD_NEAR_Z 64.0f
#define CMD_MAX_BILLBOARDS (XWA_SNAP_MAX_FLIGHT_OBJECTS + XWA_SNAP_MAX_MESH_SLOTS * 2)
#define CMD_MAX_POINT_CANDIDATES 192

typedef struct CmdBillboard {
	float center[3];
	float half_w, half_h;
	float rotation;
	float depth_bias;
	XwaAssetRef ref;
} CmdBillboard;

static struct {
	AeronScene3D* scene;
	XwaRemasterTrails* trails;
	XwaRemasterParticles* particles;
	AeronScenePresentChain* present;
	AeronRenderTarget* present_rt;
	AeronSampler* sampler;
	AeronDrawList2D* draw_list;
	int rt_w, rt_h;
	AeronSampleCount scene_requested_samples;
	AeronTexture* output;
	XwaHudCmdPreparedState prepared;
	CmdBillboard billboards[CMD_MAX_BILLBOARDS];
	uint32_t billboard_count;
	AeronSceneMeshTable tables[2];
	uint32_t table_count;
	XwaShipPointLight point_candidates[CMD_MAX_POINT_CANDIDATES];
	uint32_t point_candidate_count;
	XwaShipPointLight point_final[XWA_SHIP_MAX_POINT_LIGHTS];
	uint32_t particle_light_ids[XWA_SNAP_MAX_PARTICLE_EFFECTS];
	uint32_t particle_light_id_count;
} s_cmd;

static const XwaHudLayoutProfile* cmd_profile(XwaHudProfileIndex index) {
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	return XwaRemasterHudLayout_Profile(layout, index);
}

static const XwaFlightObject* cmd_find_object(const XwaSnapshot* snapshot, uint16_t slot,
											  uint16_t signature) {
	for (uint32_t i = 0; i < snapshot->flight_object_count; i++) {
		const XwaFlightObject* object = &snapshot->flight_objects[i];
		if (object->slot == slot && object->signature == signature)
			return object;
	}
	return NULL;
}

static void cmd_rows(const XwaHudCrt* crt, float rows[9]) {
	XwaRemasterHudCmdMath_OrthonormalRows(crt->camera_rows_q15, rows);
	const float determinant = rows[0] * (rows[4] * rows[8] - rows[5] * rows[7]) -
							  rows[1] * (rows[3] * rows[8] - rows[5] * rows[6]) +
							  rows[2] * (rows[3] * rows[7] - rows[4] * rows[6]);
	if (determinant < 0.0f) {
		/* Hud_PointCamera's docked/carried-target shortcut is expressed in
		 * the classic renderer's reflected object/view convention. The glTF
		 * eye-local boundary has already absorbed the classic model-axis
		 * mapping, so reverse its up and forward rows as a pair. One reversal
		 * would change handedness; the pair keeps the target presentation
		 * rotation fixed while selecting the carrier-facing (top) side. */
		for (int i = 3; i < 9; i++)
			rows[i] = -rows[i];
	}
}

static float cmd_camera_distance(const XwaHudCrt* crt) {
	if (crt->camera_distance > 0)
		return (float)crt->camera_distance;
	/* Compatibility with captures made before the scalar was published.
	 * The exact integer back-step still keeps this independent of the
	 * target's absolute world position. */
	return sqrtf((float)((double)crt->camera_back_step[0] * crt->camera_back_step[0] +
						 (double)crt->camera_back_step[1] * crt->camera_back_step[1] +
						 (double)crt->camera_back_step[2] * crt->camera_back_step[2]));
}

static void cmd_world_delta_to_eye(const float rows[9], const XwaHudCrt* crt, const XwaFlightObject* target,
								   const XwaFlightObject* object, float out[3]) {
	XwaRemasterHudCmdMath_ObjectEye(rows, cmd_camera_distance(crt), target->world_pos, object->world_pos,
									out);
}

static int cmd_eye_matrix(const XwaHudCrt* crt, const float rows[9], const XwaFlightObject* target,
						  const XwaFlightObject* object, int roll_align, float out[16]) {
	float camera_minus_object[3];
	XwaRemasterHudCmdMath_CameraMinusObject(rows, cmd_camera_distance(crt), target->world_pos,
											object->world_pos, camera_minus_object);
	float world[16];
	if (!XwaRemasterFlight_ObjectModelMatrixForCameraDelta(object, camera_minus_object, roll_align, world))
		return 0;
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			out[r * 4 + c] =
				rows[r * 3] * world[c] + rows[r * 3 + 1] * world[4 + c] + rows[r * 3 + 2] * world[8 + c];
		}
	}
	float eye[3];
	cmd_world_delta_to_eye(rows, crt, target, object, eye);
	out[3] = eye[0];
	out[7] = eye[1];
	out[11] = eye[2];
	out[12] = out[13] = out[14] = 0.0f;
	out[15] = 1.0f;
	return 1;
}

static int cmd_model_genus(const XwaFlightObject* object) {
	if (object->has_mobj) {
		switch (object->genus) {
			case XWA_SNAP_GENUS_FIGHTER:
			case XWA_SNAP_GENUS_TRANSPORT:
			case XWA_SNAP_GENUS_UTILITY:
			case XWA_SNAP_GENUS_FREIGHTER:
			case XWA_SNAP_GENUS_STARSHIP:
			case XWA_SNAP_GENUS_PLATFORM:
			case XWA_SNAP_GENUS_SATELLITE_BUOY:
			case XWA_SNAP_GENUS_LARGE_SCENERY:
			case XWA_SNAP_GENUS_CONTAINER:
			case XWA_SNAP_GENUS_PILOT_DROID:
			case XWA_SNAP_GENUS_WEAPON_EMPLACEMENT:
			case XWA_SNAP_GENUS_RUBBLE:
			case XWA_SNAP_GENUS_SALVAGE_JUNK:
				return 1;
			default:
				return 0;
		}
	}
	switch (object->genus) {
		case XWA_SNAP_GENUS_MINE:
		case XWA_SNAP_GENUS_ASTEROID:
		case XWA_SNAP_GENUS_DEBRIS:
		case XWA_SNAP_GENUS_DS_TUNNEL:
			return 1;
		default:
			return 0;
	}
}

static int cmd_projectile(const XwaFlightObject* object) {
	return object->genus == XWA_SNAP_GENUS_PLAYER_PROJECTILE ||
		   object->genus == XWA_SNAP_GENUS_NPC_PROJECTILE;
}

static AeronSceneMesh* cmd_mesh(const XwaFlightObject* object, int* out_runtime_opt) {
	const char* name = XwaSnapshotExport_ModelName(object->object_type);
	return name ? XwaRemasterShip_MeshForNameWithSource(name, out_runtime_opt) : NULL;
}

static float cmd_engine_glow_scale(const XwaFlightObject* object, uint64_t tick_index) {
	if (!object || !object->has_craft || !object->eg_output_scale || !object->eg_working)
		return 0.0f;
	if (object->object_kind == 5 || object->object_kind == 6) {
		const double window = (double)(4 * (3600 - (int)object->eg_max_speed));
		const double delta = (double)((int)object->speed - (int)object->eg_max_speed);
		if (window <= 0.0)
			return 12.0f;
		return delta <= window ? 1.0f + (float)(delta / window) * 11.0f : 12.0f;
	}
	const int margin =
		16 - (int)object->eg_shield_redirect - (int)object->eg_laser_redirect - (int)object->eg_beam_level;
	const float redirect =
		(float)((double)margin * 0.0625 * ((double)object->eg_output_scale * 0.000015259022));
	uint32_t sample = (uint32_t)tick_index ^ ((uint32_t)object->signature << 16) ^ object->slot;
	sample ^= sample >> 16;
	float scale = (float)((double)object->eg_throttle * 0.00001525902189669642 *
						  ((1.0 - (double)(sample & 0xfu) * 0.0040000002) * (double)redirect));
	return scale < 0.34999999f ? 0.34999999f : scale;
}

static int cmd_submit_model(AeronCommandBuffer* cmd, XwaRemasterAssets* assets, const XwaSnapshot* snapshot,
							const XwaHudCrt* crt, const float rows[9], const XwaFlightObject* target,
							const XwaFlightObject* object, int roll_align, int target_effects,
							const XwaAssetRef* glow_ref, float out_transform[16]) {
	float transform[16];
	if (!cmd_eye_matrix(crt, rows, target, object, roll_align, transform))
		return 0;
	if (out_transform)
		memcpy(out_transform, transform, sizeof transform);
	int runtime_opt = 0;
	AeronSceneMesh* mesh =
		object->object_type == XWA_SNAP_TYPE_DEBRIS_CHUNK
			? XwaRemasterShip_MeshForName(XwaSnapshotExport_ModelName(object->source_object_type))
			: cmd_mesh(object, &runtime_opt);
	if (!mesh)
		return 0;
	AeronSceneMeshInstance instance;
	memset(&instance, 0, sizeof instance);
	instance.mesh = mesh;
	instance.variant = object->node_switch;
	instance.no_local_lights = 1;
	instance.zero_velocity = 1;
	if (cmd_projectile(object) && runtime_opt) {
		instance.base_color_emissive_strength = XwaRemasterShip_OptProjectileEmissiveStrength();
	}
	instance.cull_mode = roll_align ? AERON_CULL_NONE : AERON_CULL_BACK;
	memcpy(instance.transform, transform, sizeof transform);
	memcpy(instance.prev_transform, transform, sizeof transform);
	if (object->object_type == XWA_SNAP_TYPE_DEBRIS_CHUNK && s_cmd.table_count < 2) {
		AeronSceneMeshTable* table = &s_cmd.tables[s_cmd.table_count];
		if (!XwaRemasterShip_BuildDebrisMeshTable(object, table))
			return 0;
		instance.mesh_table = table;
		s_cmd.table_count++;
	} else if (s_cmd.table_count < 2) {
		AeronSceneMeshTable* table = &s_cmd.tables[s_cmd.table_count];
		if (XwaRemasterShip_BuildMeshTable(mesh, object, table)) {
			instance.mesh_table = table;
			s_cmd.table_count++;
		}
	}
	AeronScene_AddMeshInstance(s_cmd.scene, &instance);
	if (target_effects) {
		XwaRemasterGlowMarks_SubmitObject(s_cmd.scene, cmd, assets, snapshot, object, mesh, transform,
										  instance.mesh_table);
		if (glow_ref) {
			XwaRemasterShip_SubmitEngineGlows(
				s_cmd.scene, mesh, transform, 1.0f, instance.mesh_table, object->eg_knockout_mask,
				cmd_engine_glow_scale(object, snapshot->tick_index), NULL, NULL, glow_ref);
		}
		if (object->has_craft && s_cmd.point_candidate_count < CMD_MAX_POINT_CANDIDATES) {
			s_cmd.point_candidate_count += XwaRemasterShip_CollectEngineGlowPointLights(
				mesh, transform, instance.mesh_table, object,
				&s_cmd.point_candidates[s_cmd.point_candidate_count],
				CMD_MAX_POINT_CANDIDATES - s_cmd.point_candidate_count);
		}
	}
	return 1;
}

static float cmd_billboard_rotation(const float transform[16]) {
	const float a0[3] = { transform[0], transform[4], transform[8] };
	const float a1[3] = { transform[1], transform[5], transform[9] };
	const float* axis = fabsf(a0[2]) < fabsf(a1[2]) ? a0 : a1;
	return axis[0] < 0.0f ? atan2f(axis[1], -axis[0]) : -atan2f(axis[1], axis[0]);
}

static void cmd_add_billboard(const XwaFlightObject* object, const float center[3], float rotation, int frame,
							  int object_type, int base_size, XwaRemasterAssets* assets,
							  const XwaHudCrt* crt) {
	if (s_cmd.billboard_count >= CMD_MAX_BILLBOARDS || frame <= 0 || center[2] < 1.0f)
		return;
	int w = 0, h = 0, max_bounds = 0;
	if (!XwaSnapshotExport_ModelTexFrame(object_type, frame, &w, &h, &max_bounds) || max_bounds <= 0)
		return;
	float z = center[2];
	if (object_type == XWA_SNAP_TYPE_EXPLOSION_2006) {
		z -= (float)object->type_specific_w;
		if (z <= 0.0f)
			z = 1.0f;
	}
	uint16_t base = (uint16_t)base_size;
	if (!base) {
		if (object->source_object_type == XWA_SNAP_TYPE_SSD && object_type == XWA_SNAP_TYPE_EXPLOSION_2006) {
			base = 0xffffu;
		} else {
			base =
				XwaRemasterFlight_ClassicBillboardBaseSize(object->instance_extent, object_type, max_bounds);
		}
	}
	if (!base)
		return;
	float projected = (float)max_bounds * (float)base / z;
	if (projected > 1024.0f)
		projected = 1024.0f;
	XwaAssetRef ref;
	if (!XwaRemasterAssets_FlightModelFrame(assets, object_type, frame, &ref))
		return;
	const float proj_scale = crt->proj_scale > 0 ? (float)crt->proj_scale : 512.0f;
	CmdBillboard* b = &s_cmd.billboards[s_cmd.billboard_count++];
	const float adjusted = z / center[2];
	b->center[0] = center[0] * adjusted;
	b->center[1] = center[1] * adjusted;
	b->center[2] = z;
	b->half_w = (float)w * projected * (1.0f / 512.0f) * z / proj_scale;
	b->half_h = (float)h * projected * (1.0f / 512.0f) * z / proj_scale;
	b->rotation = rotation;
	b->depth_bias = 0.0f;
	if (object_type >= XWA_SNAP_TYPE_EXPLOSION_2000 && object_type < XWA_SNAP_TYPE_EXPLOSION_2006) {
		const float pull = (float)((w > h ? w : h) << 8);
		if (pull < z)
			b->depth_bias = pull;
	}
	b->ref = ref;
}

static void cmd_add_object_billboard(const XwaFlightObject* object, const XwaHudCrt* crt, const float rows[9],
									 const XwaFlightObject* target, XwaRemasterAssets* assets) {
	if (!object->type_specific_0 || object->object_type == XWA_SNAP_TYPE_DEBRIS_CHUNK)
		return;
	float transform[16];
	if (!cmd_eye_matrix(crt, rows, target, object, 0, transform))
		return;
	const float center[3] = { transform[3], transform[7], transform[11] };
	cmd_add_billboard(object, center, cmd_billboard_rotation(transform), object->type_specific_0,
					  object->object_type, 0, assets, crt);
}

static void cmd_add_wreck_flames(const XwaFlightObject* object, const XwaHudCrt* crt, const float rows[9],
								 const XwaFlightObject* target, XwaRemasterAssets* assets) {
	if (!object->has_craft || !object->component_state[49])
		return;
	uint8_t mesh_types[XWA_SNAP_MAX_MESH_SLOTS];
	const int mesh_count = XwaSnapshotExport_ModelMeshTypes(object->object_type, mesh_types);
	float transform[16];
	if (!mesh_count || !cmd_eye_matrix(crt, rows, target, object, 0, transform))
		return;
	const float center[3] = { transform[3], transform[7], transform[11] };
	float rotation = cmd_billboard_rotation(transform);
	const float roll = (float)(int16_t)object->roll * (2.0f * 3.14159265358979323846f / 65536.0f);
	for (int i = 0; i < mesh_count; i++) {
		if (mesh_types[i] == XWA_SNAP_MESH_FUSELAGE && object->component_state[i] == 0) {
			rotation += roll;
			cmd_add_billboard(object, center, rotation, object->component_state[49], XWA_SNAP_TYPE_FLAME_2008,
							  256, assets, crt);
		}
	}
}

static int cmd_billboard_compare(const void* lhs, const void* rhs) {
	const float a = ((const CmdBillboard*)lhs)->center[2];
	const float b = ((const CmdBillboard*)rhs)->center[2];
	return a < b ? 1 : (a > b ? -1 : 0);
}

static void cmd_submit_billboards(void) {
	static const float uv[4][2] = { { 1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 } };
	qsort(s_cmd.billboards, s_cmd.billboard_count, sizeof s_cmd.billboards[0], cmd_billboard_compare);
	for (uint32_t i = 0; i < s_cmd.billboard_count; i++) {
		const CmdBillboard* b = &s_cmd.billboards[i];
		const float c = cosf(b->rotation), sn = sinf(b->rotation);
		const float right[2] = { b->half_w * c, -b->half_w * sn };
		const float up[2] = { b->half_h * sn, b->half_h * c };
		AeronSceneBillboardDesc desc;
		memset(&desc, 0, sizeof desc);
		desc.texture = b->ref.texture;
		desc.blend = AERON_SCENE_BILLBOARD_BLEND_PMA;
		desc.stage = AERON_SCENE_BILLBOARD_STAGE_OVERLAY;
		desc.depth_bias_view = b->depth_bias;
		for (int corner = 0; corner < 4; corner++) {
			const float sx = (corner == 0 || corner == 3) ? 1.0f : -1.0f;
			const float sy = corner <= 1 ? 1.0f : -1.0f;
			desc.corners[corner][0] = b->center[0] + sx * right[0] + sy * up[0];
			desc.corners[corner][1] = b->center[1] + sx * right[1] + sy * up[1];
			desc.corners[corner][2] = b->center[2];
			desc.uv[corner][0] = b->ref.u0 + uv[corner][0] * (b->ref.u1 - b->ref.u0);
			desc.uv[corner][1] = b->ref.v0 + uv[corner][1] * (b->ref.v1 - b->ref.v0);
			desc.colors[corner][0] = desc.colors[corner][1] = desc.colors[corner][2] =
				desc.colors[corner][3] = 1.0f;
		}
		AeronScene_AddBillboard(s_cmd.scene, &desc);
	}
}

static float cmd_point_range(float intensity, float cull_radius) {
	float range = 50.0f * intensity;
	if (range < cull_radius)
		range = cull_radius;
	if (range > 32768.0f)
		range = 32768.0f;
	return range;
}

static void cmd_add_point_light(const XwaSnapshot* snapshot, const XwaFlightObject* object,
								const float eye[3]) {
	if (s_cmd.point_candidate_count >= CMD_MAX_POINT_CANDIDATES)
		return;
	float color[3], intensity;
	int cull;
	if (!XwaSnapshotExport_PointLightForObject(
			object->object_type, object->genus, object->type_specific_0, object->type_specific_w,
			object->instance_extent, snapshot->flight_camera.brightness_q8, color, &intensity, &cull) ||
		intensity <= 0.0f)
		return;
	XwaShipPointLight* light = &s_cmd.point_candidates[s_cmd.point_candidate_count++];
	memcpy(light->pos, eye, sizeof light->pos);
	light->range = cmd_point_range(intensity, (float)cull);
	for (int i = 0; i < 3; i++)
		light->color[i] = XwaRemaster_SrgbToLinear(color[i]) * intensity;
}

static void cmd_add_particle_lights(const XwaSnapshot* snapshot, const float rows[9],
									const XwaFlightObject* target, uint16_t owner_slot,
									uint16_t owner_signature) {
	for (uint32_t i = 0; i < snapshot->particle_effect_count; i++) {
		const XwaParticleEffect* effect = &snapshot->particle_effects[i];
		if (!effect->point_light || effect->source_kind != XWA_PARTICLE_SOURCE_OBJECT ||
			effect->owner_slot != owner_slot || effect->owner_signature != owner_signature)
			continue;
		int seen = 0;
		for (uint32_t j = 0; j < s_cmd.particle_light_id_count; j++) {
			if (s_cmd.particle_light_ids[j] == effect->stable_id) {
				seen = 1;
				break;
			}
		}
		if (seen || s_cmd.point_candidate_count >= CMD_MAX_POINT_CANDIDATES)
			continue;
		if (s_cmd.particle_light_id_count < XWA_SNAP_MAX_PARTICLE_EFFECTS)
			s_cmd.particle_light_ids[s_cmd.particle_light_id_count++] = effect->stable_id;
		float delta[3];
		XwaRemasterWorld_LocalPoint(target->world_pos, effect->emitter_world_pos.base,
									effect->emitter_world_pos.offset, delta);
		XwaShipPointLight* light = &s_cmd.point_candidates[s_cmd.point_candidate_count++];
		for (int r = 0; r < 3; r++)
			light->pos[r] = rows[r * 3] * delta[0] + rows[r * 3 + 1] * delta[1] + rows[r * 3 + 2] * delta[2];
		light->pos[2] += cmd_camera_distance(&snapshot->hud.crt);
		light->range = cmd_point_range(125.0f, 0.0f);
		light->color[0] = light->color[1] = 125.0f;
		light->color[2] = XwaRemaster_SrgbToLinear(0.75f) * 125.0f;
	}
}

static uint32_t cmd_finalize_point_lights(XwaShipPointLightTuning* tuning) {
	memset(tuning, 0, sizeof *tuning);
	XwaFlightPointLightParams config;
	XwaRemasterFlight_GetPointLights(&config);
	if (!config.enabled || !s_cmd.point_candidate_count)
		return 0;
	float scores[CMD_MAX_POINT_CANDIDATES];
	for (uint32_t i = 0; i < s_cmd.point_candidate_count; i++) {
		XwaShipPointLight* light = &s_cmd.point_candidates[i];
		for (int c = 0; c < 3; c++)
			light->color[c] *= config.scale;
		light->range *= config.range_scale;
		const float distance2 =
			light->pos[0] * light->pos[0] + light->pos[1] * light->pos[1] + light->pos[2] * light->pos[2];
		const float luminance =
			0.2126f * light->color[0] + 0.7152f * light->color[1] + 0.0722f * light->color[2];
		scores[i] = luminance / (distance2 + light->range * light->range);
	}
	const uint32_t count = s_cmd.point_candidate_count < XWA_SHIP_MAX_POINT_LIGHTS
							   ? s_cmd.point_candidate_count
							   : XWA_SHIP_MAX_POINT_LIGHTS;
	uint8_t used[CMD_MAX_POINT_CANDIDATES] = { 0 };
	for (uint32_t output = 0; output < count; output++) {
		int best = -1;
		for (uint32_t i = 0; i < s_cmd.point_candidate_count; i++)
			if (!used[i] && (best < 0 || scores[i] > scores[best]))
				best = (int)i;
		if (best < 0)
			break;
		used[best] = 1;
		s_cmd.point_final[output] = s_cmd.point_candidates[best];
	}
	tuning->min_distance = config.min_distance;
	tuning->spec_weight = config.spec_weight;
	tuning->diffuse_wrap = config.diffuse_wrap;
	tuning->contrib_cap = config.contrib_cap;
	tuning->light_count = count;
	return count;
}

static void cmd_submit_point_lights(uint32_t count) {
	for (uint32_t i = 0; i < count; ++i) {
		AeronSceneLight light = { 0 };
		memcpy(light.pos, s_cmd.point_final[i].pos, sizeof light.pos);
		light.radius = s_cmd.point_final[i].range;
		memcpy(light.color, s_cmd.point_final[i].color, sizeof light.color);
		AeronScene_AddLight(s_cmd.scene, &light);
	}
}

static void cmd_destroy_targets(void) {
	if (s_cmd.present_rt)
		Aeron_DestroyRenderTarget(s_cmd.present_rt);
	if (s_cmd.scene)
		AeronScene_Destroy(s_cmd.scene);
	s_cmd.present_rt = NULL;
	s_cmd.scene = NULL;
	s_cmd.rt_w = s_cmd.rt_h = 0;
	s_cmd.scene_requested_samples = 0;
}

static int cmd_ensure_targets(int width, int height) {
	if (!s_cmd.present)
		s_cmd.present = AeronScenePresentChain_Create(AERON_TEXTURE_FORMAT_RGBA16_FLOAT);
	if (!s_cmd.sampler)
		s_cmd.sampler = Aeron_CreateSampler(&(AeronSamplerDesc) { .min_filter = AERON_FILTER_LINEAR,
																  .mag_filter = AERON_FILTER_LINEAR,
																  .address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
																  .address_v = AERON_ADDRESS_CLAMP_TO_EDGE });
	if (!s_cmd.present || !s_cmd.sampler) {
		Aeron_LogError("xwa.hud", "CMD PiP present chain creation failed");
		return 0;
	}
	const AeronSampleCount requested_samples = XwaRemaster_MsaaSampleCount();
	if (s_cmd.scene && s_cmd.present_rt && s_cmd.rt_w == width && s_cmd.rt_h == height &&
		s_cmd.scene_requested_samples == requested_samples)
		return 1;
	cmd_destroy_targets();
	s_cmd.scene = AeronScene_Create(&(AeronScene3DDesc) { .rt_width = width,
														  .rt_height = height,
														  .color_format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
														  .with_normal_rt = 0,
														  .sample_count = requested_samples });
	s_cmd.present_rt =
		Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = width,
															.height = height,
															.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
															.debug_name = "xwa.hud.cmd_present" });
	if (!s_cmd.scene || !s_cmd.present_rt) {
		Aeron_LogError("xwa.hud", "CMD PiP target creation failed at %dx%d", width, height);
		cmd_destroy_targets();
		return 0;
	}
	AeronScene_SetClearColor(s_cmd.scene, (const float[4]) { 0, 0, 0, 0 });
	s_cmd.rt_w = width;
	s_cmd.rt_h = height;
	s_cmd.scene_requested_samples = requested_samples;
	return 1;
}

int XwaRemasterHudCmd_Init(void) {
	if (!s_cmd.draw_list)
		s_cmd.draw_list = AeronDrawList_Create(8);
	if (!s_cmd.trails)
		s_cmd.trails = XwaRemasterTrails_Create();
	if (!s_cmd.particles)
		s_cmd.particles = XwaRemasterParticles_Create();
	return s_cmd.draw_list != NULL && s_cmd.trails != NULL && s_cmd.particles != NULL;
}

void XwaRemasterHudCmd_Shutdown(void) {
	cmd_destroy_targets();
	XwaRemasterParticles_Destroy(s_cmd.particles);
	s_cmd.particles = NULL;
	XwaRemasterTrails_Destroy(s_cmd.trails);
	s_cmd.trails = NULL;
	if (s_cmd.draw_list)
		AeronDrawList_Destroy(s_cmd.draw_list);
	if (s_cmd.sampler)
		Aeron_DestroySampler(s_cmd.sampler);
	if (s_cmd.present)
		AeronScenePresentChain_Destroy(s_cmd.present);
	memset(&s_cmd, 0, sizeof s_cmd);
}

void XwaRemasterHudCmd_Prepare(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot,
							   XwaRemasterAssets* assets, XwaHudProfileIndex profile_id, int target_w,
							   int target_h) {
	memset(&s_cmd.prepared, 0, sizeof s_cmd.prepared);
	s_cmd.output = NULL;
	if (!cmd || !snapshot || !assets || !snapshot->hud.crt.visible)
		return;
	const XwaHudLayoutProfile* profile = cmd_profile(profile_id);
	if (!profile || !profile->valid)
		return;
	const XwaHudLayoutAnchor* anchor = &profile->anchors[XWA_HUD_ANCHOR_CMD_CRT];
	if (anchor->rect.w <= 0 || anchor->rect.h <= 0)
		return;
	const float screen_h =
		snapshot->flight_camera.screen_h ? (float)snapshot->flight_camera.screen_h : 480.0f;
	const float hud_scale = snapshot->hud.classic_hud_scale > 0.0f ? snapshot->hud.classic_hud_scale : 1.0f;
	const float density = hud_scale * 480.0f / screen_h;
	const float rect_w_ref = anchor->rect.w * density;
	const float rect_h_ref = anchor->rect.h * density;
	const float rect_x_ref = anchor->rect.x + 0.5f * (anchor->rect.w - rect_w_ref);
	const float rect_y_ref = anchor->rect.y + anchor->rect.h - rect_h_ref;
	float output_scale;
	XwaRemasterHudLayout_OutputTransform(profile, target_w, target_h, NULL, NULL, &output_scale);
	if (output_scale <= 0.0f)
		return;
	const XwaHudCrt* crt = &snapshot->hud.crt;
	const XwaFlightObject* target = cmd_find_object(snapshot, crt->target_slot, crt->target_signature);
	if (!target)
		return;
	const int width = (int)lround((double)rect_w_ref * (double)output_scale);
	const int height = (int)lround((double)rect_h_ref * (double)output_scale);
	if (width <= 0 || height <= 0)
		return;
	if (!cmd_ensure_targets(width, height)) {
		Aeron_CommandBufferSetFailure(cmd, "HUD CMD render-target preparation failed");
		return;
	}
	float rows[9];
	cmd_rows(crt, rows);
	AeronSceneCamera camera;
	memset(&camera, 0, sizeof camera);
	camera.ori[0] = 1.0f;
	const float proj_scale = crt->proj_scale > 0 ? (float)crt->proj_scale : 512.0f;
	const float half_w = crt->classic_viewport_w ? (float)(crt->classic_viewport_w >> 1) : 88.0f;
	const float half_h = crt->classic_viewport_h ? (float)(crt->classic_viewport_h >> 1) : 72.0f;
	const float aspect_y = crt->proj_aspect_y_q16 ? crt->proj_aspect_y_q16 * (1.0f / 65536.0f) : 1.0f;
	camera.h_half_rad = atanf(half_w / proj_scale);
	camera.v_half_rad = atanf(half_h / (proj_scale * aspect_y));
	camera.near_z = CMD_NEAR_Z;
	camera.viewport = (AeronRectI) { 0, 0, width, height };
	const float effect_origin_eye[3] = { 0.0f, 0.0f, cmd_camera_distance(crt) };
	XwaRemasterEffectView effect_view;
	XwaRemasterEffectView_EyeLocal(&effect_view, target->world_pos, effect_origin_eye, rows, &camera,
								   (float)height /
									   (crt->classic_viewport_h ? (float)crt->classic_viewport_h : 144.0f));
	/* CMD has no motion-blur pass, so particle history is intentionally absent. */
	XwaRemasterParticles_Prepare(s_cmd.particles, cmd, snapshot, NULL, assets, &effect_view, NULL);
	XwaRemasterTrails_Prepare(s_cmd.trails, cmd, snapshot, assets, &effect_view);
	if (!AeronScene_Begin(s_cmd.scene, &camera)) {
		Aeron_CommandBufferSetFailure(cmd, "HUD CMD scene initialization failed");
		return;
	}
	AeronScene_SetPost(s_cmd.scene, &(AeronScenePostDesc) { 0 });
	s_cmd.billboard_count = 0;
	s_cmd.table_count = 0;
	s_cmd.point_candidate_count = 0;
	s_cmd.particle_light_id_count = 0;
	XwaAssetRef glow_ref;
	const int glow_ok = XwaRemasterAssets_FlightAtlasFrame(assets, 1000, 0, &glow_ref);
	float target_transform[16];
	memset(target_transform, 0, sizeof target_transform);
	if (cmd_projectile(target)) {
		cmd_submit_model(cmd, assets, snapshot, crt, rows, target, target, 1, 0, glow_ok ? &glow_ref : NULL,
						 target_transform);
	} else if (cmd_model_genus(target)) {
		cmd_submit_model(cmd, assets, snapshot, crt, rows, target, target, 0, 1, glow_ok ? &glow_ref : NULL,
						 target_transform);
	}
	cmd_add_wreck_flames(target, crt, rows, target, assets);
	if (target->has_craft && target->carried_object_slot != 0xffffu &&
		target->carried_object_slot != snapshot->hud.player_slot) {
		const XwaFlightObject* carried =
			cmd_find_object(snapshot, target->carried_object_slot, target->carried_object_signature);
		if (carried && carried->has_craft) {
			cmd_submit_model(cmd, assets, snapshot, crt, rows, target, carried, 0, 0, NULL, NULL);
			cmd_add_wreck_flames(carried, crt, rows, target, assets);
		}
	}
	for (uint32_t i = 0; i < snapshot->flight_object_count; i++) {
		const XwaFlightObject* object = &snapshot->flight_objects[i];
		if (object->region != target->region)
			continue;
		if (cmd_projectile(object) && object->slot_class == XWA_SNAP_SLOT_MAIN) {
			if (object->slot == crt->projectile_exclude_slots[0] ||
				object->slot == crt->projectile_exclude_slots[1])
				continue;
			const int source_match = crt->map_view
										 ? object->source_obj == (int16_t)target->slot
										 : object->source_obj == (int16_t)target->slot ||
											   object->source_obj == (int16_t)snapshot->hud.player_slot;
			if (!source_match)
				continue;
			cmd_submit_model(cmd, assets, snapshot, crt, rows, target, object, 1, 0, NULL, NULL);
			float eye[3];
			cmd_world_delta_to_eye(rows, crt, target, object, eye);
			cmd_add_point_light(snapshot, object, eye);
			/* The classic map-view branch draws the projectile model and
			 * continues before its particle/trail effects. */
			if (!crt->map_view) {
				XwaRemasterParticles_SubmitOwner(s_cmd.particles, s_cmd.scene, snapshot, object->slot,
												 object->signature);
				XwaRemasterTrails_SubmitOwner(s_cmd.trails, s_cmd.scene, object->slot, object->signature);
				cmd_add_particle_lights(snapshot, rows, target, object->slot, object->signature);
			}
		} else if (object->genus == XWA_SNAP_GENUS_EXPLOSION &&
				   (object->slot_class == XWA_SNAP_SLOT_MAIN ||
					object->slot_class == XWA_SNAP_SLOT_TRANSIENT)) {
			cmd_add_object_billboard(object, crt, rows, target, assets);
			float eye[3];
			cmd_world_delta_to_eye(rows, crt, target, object, eye);
			cmd_add_point_light(snapshot, object, eye);
			if (object->slot_class == XWA_SNAP_SLOT_MAIN) {
				XwaRemasterParticles_SubmitOwner(s_cmd.particles, s_cmd.scene, snapshot, object->slot,
												 object->signature);
				cmd_add_particle_lights(snapshot, rows, target, object->slot, object->signature);
			}
		}
	}
	/* Hud_Update3DCrt draws the target's own effects after its projectile /
	 * explosion scan. This can intentionally submit it a second time when a
	 * projectile target also passed the scan, matching the classic ordering. */
	XwaRemasterParticles_SubmitOwner(s_cmd.particles, s_cmd.scene, snapshot, target->slot, target->signature);
	XwaRemasterTrails_SubmitOwner(s_cmd.trails, s_cmd.scene, target->slot, target->signature);
	cmd_add_particle_lights(snapshot, rows, target, target->slot, target->signature);
	cmd_submit_billboards();
	XwaShipPointLightTuning point_tuning;
	const uint32_t point_count = cmd_finalize_point_lights(&point_tuning);
	cmd_submit_point_lights(point_count);
	XwaRemasterShip_SetPbrEnv(s_cmd.scene, snapshot->dir_lights, snapshot->dir_light_count, rows, NULL, NULL,
							  point_count ? &point_tuning : NULL, /*ambient_cube=*/NULL);
	if (!AeronScene_Render(s_cmd.scene, cmd)) {
		return;
	}
	AeronTexture* scene_texture = Aeron_RenderTargetGetTexture(AeronScene_SceneRt(s_cmd.scene));
	if (!scene_texture)
		return;
	AeronRenderPass* pass =
		Aeron_BeginRenderPass(&(AeronRenderPassDesc) { .color_target = s_cmd.present_rt,
													   .clear_color = 1,
													   .clear_color_rgba = { 0, 0, 0, 0 },
													   .command_buffer = cmd,
													   .debug_label = "OpenXWA CMD display tonemap" });
	if (!pass)
		return;
	static const float tint[4] = { 1, 1, 1, 1 };
	AeronScenePresentChain_Draw(s_cmd.present, pass, scene_texture, s_cmd.sampler, NULL, 0.0f, width, height,
								/*bar_y_uv=*/1.0f, tint, /*src_coverage=*/1);
	Aeron_EndRenderPass(pass);
	s_cmd.output = Aeron_RenderTargetGetTexture(s_cmd.present_rt);
	s_cmd.prepared.valid = s_cmd.output != NULL;
	s_cmd.prepared.profile = (uint8_t)profile_id;
	s_cmd.prepared.target_slot = target->slot;
	s_cmd.prepared.target_signature = target->signature;
	s_cmd.prepared.internal_w = (uint16_t)(width > 0xffff ? 0xffff : width);
	s_cmd.prepared.internal_h = (uint16_t)(height > 0xffff ? 0xffff : height);
	s_cmd.prepared.classic_viewport_w = crt->classic_viewport_w ? crt->classic_viewport_w : 176;
	s_cmd.prepared.classic_viewport_h = crt->classic_viewport_h ? crt->classic_viewport_h : 144;
	s_cmd.prepared.rect_x_ref = rect_x_ref;
	s_cmd.prepared.rect_y_ref = rect_y_ref;
	s_cmd.prepared.rect_w_ref = rect_w_ref;
	s_cmd.prepared.rect_h_ref = rect_h_ref;
	if (s_cmd.prepared.valid && crt->component_marker_visible && target_transform[15] != 0.0f) {
		const float focus[3] = { (float)crt->component_focus[0], (float)crt->component_focus[1],
								 (float)crt->component_focus[2] };
		const float eye[3] = {
			target_transform[0] * focus[0] + target_transform[1] * focus[1] + target_transform[2] * focus[2] +
				target_transform[3],
			target_transform[4] * focus[0] + target_transform[5] * focus[1] + target_transform[6] * focus[2] +
				target_transform[7],
			target_transform[8] * focus[0] + target_transform[9] * focus[1] +
				target_transform[10] * focus[2] + target_transform[11],
		};
		if (eye[2] > 0.0f) {
			const float viewport_w = crt->classic_viewport_w ? (float)crt->classic_viewport_w : 176.0f;
			const float viewport_h = crt->classic_viewport_h ? (float)crt->classic_viewport_h : 144.0f;
			s_cmd.prepared.marker_x = 0.5f + proj_scale * eye[0] / eye[2] / viewport_w;
			s_cmd.prepared.marker_y = 0.5f + proj_scale * aspect_y * eye[1] / eye[2] / viewport_h;
			s_cmd.prepared.marker_visible = 1;
		}
	}
}

void XwaRemasterHudCmd_PrepareDrawList(AeronCommandBuffer* cmd, int target_w, int target_h) {
	if (!cmd || !s_cmd.draw_list || target_w <= 0 || target_h <= 0)
		return;
	AeronDrawList_Begin(s_cmd.draw_list, NULL, target_w, target_h, AERON_DRAWLIST2D_LOAD, NULL);
	if (!s_cmd.prepared.valid || !s_cmd.output) {
		(void)AeronDrawList_Prepare(s_cmd.draw_list, cmd);
		return;
	}
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	const XwaHudLayoutProfile* profile = cmd_profile((XwaHudProfileIndex)s_cmd.prepared.profile);
	if (!layout || !profile) {
		(void)AeronDrawList_Prepare(s_cmd.draw_list, cmd);
		return;
	}
	int out_x, out_y;
	float scale;
	XwaRemasterHudLayout_OutputTransform(profile, target_w, target_h, &out_x, &out_y, &scale);
	if (scale <= 0.0f) {
		(void)AeronDrawList_Prepare(s_cmd.draw_list, cmd);
		return;
	}
	const float x = out_x + s_cmd.prepared.rect_x_ref * scale;
	const float y = out_y + s_cmd.prepared.rect_y_ref * scale;
	const float w = s_cmd.prepared.rect_w_ref * scale;
	const float h = s_cmd.prepared.rect_h_ref * scale;
	const int scissor_x0 = (int)floorf(x), scissor_y0 = (int)floorf(y);
	const int scissor_x1 = (int)ceilf(x + w), scissor_y1 = (int)ceilf(y + h);
	const AeronRectI scissor = { scissor_x0, scissor_y0, scissor_x1 - scissor_x0, scissor_y1 - scissor_y0 };
	AeronDrawList2DSprite sprite = { 0 };
	sprite.texture = s_cmd.output;
	sprite.src_u1 = sprite.src_v1 = 1.0f;
	sprite.dst_x = x;
	sprite.dst_y = y;
	sprite.dst_w = w;
	sprite.dst_h = h;
	sprite.tint[0] = sprite.tint[1] = sprite.tint[2] = sprite.tint[3] = 1.0f;
	sprite.blend = AERON_BLIT2D_BLEND_PMA;
	sprite.filter = fabsf(w - (float)s_cmd.rt_w) > 0.5f || fabsf(h - (float)s_cmd.rt_h) > 0.5f
						? AERON_BLIT2D_FILTER_LINEAR
						: AERON_BLIT2D_FILTER_NEAREST;
	sprite.scissor = scissor;
	AeronDrawList_AddSprite(s_cmd.draw_list, &sprite);
	if (s_cmd.prepared.marker_visible) {
		const uint32_t argb = XwaSnapshotExport_FlightPaletteColor(63);
		const float a = ((argb >> 24) & 255) / 255.0f;
		const float color[4] = {
			XwaRemaster_SrgbToLinear(((argb >> 16) & 255) / 255.0f) * a,
			XwaRemaster_SrgbToLinear(((argb >> 8) & 255) / 255.0f) * a,
			XwaRemaster_SrgbToLinear((argb & 255) / 255.0f) * a,
			a,
		};
		const float pixel_x = w / (float)s_cmd.prepared.classic_viewport_w;
		const float pixel_y = h / (float)s_cmd.prepared.classic_viewport_h;
		const float mx = x + s_cmd.prepared.marker_x * w - 2.0f * pixel_x;
		const float my = y + s_cmd.prepared.marker_y * h - 2.0f * pixel_y;
		AeronDrawList_AddFill(s_cmd.draw_list, mx, my, 4.0f * pixel_x, pixel_y, color, AERON_BLIT2D_BLEND_PMA,
							  &scissor);
		AeronDrawList_AddFill(s_cmd.draw_list, mx, my + 3.0f * pixel_y, 4.0f * pixel_x, pixel_y, color,
							  AERON_BLIT2D_BLEND_PMA, &scissor);
		AeronDrawList_AddFill(s_cmd.draw_list, mx, my + pixel_y, pixel_x, 2.0f * pixel_y, color,
							  AERON_BLIT2D_BLEND_PMA, &scissor);
		AeronDrawList_AddFill(s_cmd.draw_list, mx + 3.0f * pixel_x, my + pixel_y, pixel_x, 2.0f * pixel_y,
							  color, AERON_BLIT2D_BLEND_PMA, &scissor);
	}
	(void)AeronDrawList_Prepare(s_cmd.draw_list, cmd);
}

void XwaRemasterHudCmd_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
							  int target_w, int target_h) {
	if (!cmd || !pass || !target || !s_cmd.draw_list || !s_cmd.prepared.valid || !s_cmd.output ||
		target_w <= 0 || target_h <= 0)
		return;
	AeronDrawList_RenderIntoPass(s_cmd.draw_list, cmd, pass, target);
}

const XwaHudCmdPreparedState* XwaRemasterHudCmd_Prepared(void) { return &s_cmd.prepared; }
