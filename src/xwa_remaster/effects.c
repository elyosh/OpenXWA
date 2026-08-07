#include "xwa_remaster/effects.h"

#include "aeron/aeron.h"
#include "aeron/scene/billboard.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* One source segment can cross at most five texture periods in the recovered
 * trail definitions. Leave headroom for malformed/modded state while keeping
 * a hard renderer-side bound. */
#define EFFECT_MAX_TRAIL_QUADS (XWA_SNAP_MAX_TRAIL_POINTS * 8u)

typedef struct PreparedTrailQuad {
	uint16_t owner_slot;
	uint16_t owner_signature;
	uint8_t render_region;
	AeronTexture* texture;
	float corners[4][3];
	float uv[4][2];
	float colors[4][4];
} PreparedTrailQuad;

struct XwaRemasterTrails {
	PreparedTrailQuad* quads;
	uint32_t count;
	uint32_t capacity;
	int dropped;
};

typedef struct TrailEndpoint {
	float eye[3];
	float tex_v;
	float alpha;
	float width_factor;
} TrailEndpoint;

typedef struct PreparedParticleQuad {
	uint16_t owner_slot;
	uint16_t owner_signature;
	uint8_t source_kind;
	uint8_t render_region;
	uint8_t hide_owner_external;
	uint8_t hide_owner_film;
	AeronTexture* texture;
	float corners[4][3];
	float prev_corners[4][3];
	float uv[4][2];
	float color[4];
	uint8_t has_prev;
} PreparedParticleQuad;

struct XwaRemasterParticles {
	PreparedParticleQuad* quads;
	uint32_t count;
	uint32_t capacity;
	int dropped;
};

void XwaRemasterEffectView_Main(XwaRemasterEffectView* out, const XwaRemasterFlightView* view,
								const float world_to_eye[9]) {
	memset(out, 0, sizeof *out);
	memcpy(out->origin_world, view->origin_world, sizeof out->origin_world);
	memcpy(out->world_to_eye, world_to_eye, sizeof out->world_to_eye);
	/* Inverse of the orthonormal world-to-eye rows. */
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 3; c++)
			out->eye_to_scene[r * 3 + c] = world_to_eye[c * 3 + r];
	memcpy(out->scene_origin, view->camera.pos, sizeof out->scene_origin);
	out->focal_x_px = 0.5f * (float)view->viewport.width / tanf(view->camera.h_half_rad);
	out->focal_y_px = 0.5f * (float)view->viewport.height / tanf(view->camera.v_half_rad);
	out->classic_pixel_scale = view->classic_pixel_scale;
	out->near_z = view->camera.near_z;
}

void XwaRemasterEffectView_EyeLocal(XwaRemasterEffectView* out, const int32_t origin_world[3],
									const float origin_eye[3], const float world_to_eye[9],
									const AeronSceneCamera* camera, float classic_pixel_scale) {
	memset(out, 0, sizeof *out);
	memcpy(out->origin_world, origin_world, sizeof out->origin_world);
	if (origin_eye)
		memcpy(out->origin_eye, origin_eye, sizeof out->origin_eye);
	memcpy(out->world_to_eye, world_to_eye, sizeof out->world_to_eye);
	out->eye_to_scene[0] = out->eye_to_scene[4] = out->eye_to_scene[8] = 1.0f;
	const int width = camera->viewport.width > 0 ? camera->viewport.width : 1;
	const int height = camera->viewport.height > 0 ? camera->viewport.height : 1;
	out->focal_x_px = 0.5f * (float)width / tanf(camera->h_half_rad);
	out->focal_y_px = 0.5f * (float)height / tanf(camera->v_half_rad);
	out->classic_pixel_scale = classic_pixel_scale;
	out->near_z = camera->near_z;
}

XwaRemasterTrails* XwaRemasterTrails_Create(void) {
	return (XwaRemasterTrails*)calloc(1, sizeof(XwaRemasterTrails));
}

void XwaRemasterTrails_Destroy(XwaRemasterTrails* trails) {
	if (!trails)
		return;
	free(trails->quads);
	free(trails);
}

static const XwaFlightObject* effect_find_owner(const XwaSnapshot* snapshot, uint16_t slot,
												uint16_t signature) {
	for (uint32_t i = 0; i < snapshot->flight_object_count; i++) {
		const XwaFlightObject* object = &snapshot->flight_objects[i];
		if (object->slot == slot && object->signature == signature)
			return object;
	}
	return NULL;
}

static void effect_world_to_eye(const XwaRemasterEffectView* view, const int32_t world[3], float eye[3]) {
	float delta[3];
	XwaRemasterWorld_DeltaI32(world, view->origin_world, delta);
	const float x = delta[0];
	const float y = delta[1];
	const float z = delta[2];
	for (int r = 0; r < 3; r++)
		eye[r] = view->origin_eye[r] + view->world_to_eye[r * 3] * x + view->world_to_eye[r * 3 + 1] * y +
				 view->world_to_eye[r * 3 + 2] * z;
}

static void effect_eye_to_scene(const XwaRemasterEffectView* view, const float eye[3], float scene[3]) {
	for (int r = 0; r < 3; r++)
		scene[r] = view->scene_origin[r] + view->eye_to_scene[r * 3] * eye[0] +
				   view->eye_to_scene[r * 3 + 1] * eye[1] + view->eye_to_scene[r * 3 + 2] * eye[2];
}

static void effect_point_to_eye(const XwaRemasterEffectView* view, const XwaPreciseWorldPoint* point,
								float eye[3]) {
	float delta[3];
	XwaRemasterWorld_LocalPoint(view->origin_world, point->base, point->offset, delta);
	for (int r = 0; r < 3; r++)
		eye[r] = view->origin_eye[r] + view->world_to_eye[r * 3] * delta[0] +
				 view->world_to_eye[r * 3 + 1] * delta[1] + view->world_to_eye[r * 3 + 2] * delta[2];
}

XwaRemasterParticles* XwaRemasterParticles_Create(void) {
	XwaRemasterParticles* particles = (XwaRemasterParticles*)calloc(1, sizeof *particles);
	if (!particles)
		return NULL;
	particles->quads =
		(PreparedParticleQuad*)calloc(XWA_SNAP_MAX_PARTICLES, sizeof *particles->quads);
	if (!particles->quads) {
		free(particles);
		return NULL;
	}
	particles->capacity = XWA_SNAP_MAX_PARTICLES;
	return particles;
}

void XwaRemasterParticles_Destroy(XwaRemasterParticles* particles) {
	if (!particles)
		return;
	free(particles->quads);
	free(particles);
}

static void particle_color(uint32_t argb, float out[4]) {
	const float a = (float)((argb >> 24) & 0xffu) * (1.0f / 255.0f);
	out[0] = XwaRemaster_SrgbToLinear((float)((argb >> 16) & 0xffu) * (1.0f / 255.0f)) * a;
	out[1] = XwaRemaster_SrgbToLinear((float)((argb >> 8) & 0xffu) * (1.0f / 255.0f)) * a;
	out[2] = XwaRemaster_SrgbToLinear((float)(argb & 0xffu) * (1.0f / 255.0f)) * a;
	out[3] = a;
}

static void particle_set_uv(PreparedParticleQuad* q, const XwaAssetRef* ref) {
	q->uv[0][0] = q->uv[3][0] = ref->u1;
	q->uv[1][0] = q->uv[2][0] = ref->u0;
	q->uv[0][1] = q->uv[1][1] = ref->v1;
	q->uv[2][1] = q->uv[3][1] = ref->v0;
}

static int particle_build_facing(PreparedParticleQuad* q, const XwaRemasterEffectView* view,
								 const XwaParticle* particle, int classic_w, int classic_h) {
	float center[3];
	effect_point_to_eye(view, &particle->world_pos, center);
	if (center[2] <= view->near_z)
		return 0;
	const float half_x =
		(float)classic_w * particle->size_scale * view->classic_pixel_scale / view->focal_x_px;
	const float half_y =
		(float)classic_h * particle->size_scale * view->classic_pixel_scale / view->focal_y_px;
	const float eye[4][3] = { { center[0] + half_x, center[1] + half_y, center[2] },
							  { center[0] - half_x, center[1] + half_y, center[2] },
							  { center[0] - half_x, center[1] - half_y, center[2] },
							  { center[0] + half_x, center[1] - half_y, center[2] } };
	for (int i = 0; i < 4; i++)
		effect_eye_to_scene(view, eye[i], q->corners[i]);
	return 1;
}

static int particle_build_stretched(PreparedParticleQuad* q, const XwaRemasterEffectView* view,
									const XwaParticle* particle, int classic_w, int local_mode) {
	float head[3], tail[3];
	effect_point_to_eye(view, &particle->world_pos, head);
	effect_point_to_eye(view, &particle->tail_world_pos, tail);
	if (head[2] < view->near_z || tail[2] < view->near_z)
		return 0;
	const float hx = view->focal_x_px * head[0] / head[2];
	const float hy = view->focal_y_px * head[1] / head[2];
	const float tx = view->focal_x_px * tail[0] / tail[2];
	const float ty = view->focal_y_px * tail[1] / tail[2];
	const float angle = atan2f(hy - ty, local_mode ? hx - tx : tx - hx);
	float thickness = (float)classic_w * particle->size_scale / head[2];
	if (local_mode)
		thickness *= 4.0f;
	thickness = thickness * view->classic_pixel_scale + 0.5f * view->classic_pixel_scale;
	float plus_x = sinf(angle) * thickness;
	float plus_y = cosf(angle) * thickness;
	if (local_mode) {
		/* Preserve the original signed per-axis clamp, including its
		 * asymmetric behavior for negative components. */
		if (plus_y < view->classic_pixel_scale)
			plus_y = view->classic_pixel_scale;
		if (plus_x < view->classic_pixel_scale)
			plus_x = view->classic_pixel_scale;
	}
	const float hdx = plus_x * head[2] / view->focal_x_px;
	const float hdy = plus_y * head[2] / view->focal_y_px;
	const float tdx = plus_x * tail[2] / view->focal_x_px;
	const float tdy = plus_y * tail[2] / view->focal_y_px;
	const float eye[4][3] = { { head[0] + hdx, head[1] + hdy, head[2] },
							  { head[0] - hdx, head[1] - hdy, head[2] },
							  { tail[0] - tdx, tail[1] - tdy, tail[2] },
							  { tail[0] + tdx, tail[1] + tdy, tail[2] } };
	for (int i = 0; i < 4; i++)
		effect_eye_to_scene(view, eye[i], q->corners[i]);
	return 1;
}

static int particle_effect_range_valid(const XwaSnapshot* snapshot, const XwaParticleEffect* effect) {
	return effect->first_particle < snapshot->particle_count &&
		   effect->particle_count <= snapshot->particle_count - effect->first_particle;
}

static const XwaParticleEffect* particle_find_previous_effect(const XwaSnapshot* previous,
															  uint32_t current_index,
															  const XwaParticleEffect* current) {
	if (!previous || current->stable_id == 0)
		return NULL;
	if (current_index < previous->particle_effect_count) {
		const XwaParticleEffect* candidate = &previous->particle_effects[current_index];
		if (candidate->stable_id == current->stable_id && particle_effect_range_valid(previous, candidate))
			return candidate;
	}
	for (uint32_t i = 0; i < previous->particle_effect_count; i++) {
		const XwaParticleEffect* candidate = &previous->particle_effects[i];
		if (candidate->stable_id == current->stable_id && particle_effect_range_valid(previous, candidate))
			return candidate;
	}
	return NULL;
}

static const XwaParticle* particle_find_previous_record(const XwaSnapshot* previous,
														const XwaParticleEffect* previous_effect,
														uint32_t current_index, const XwaParticle* current) {
	if (!previous_effect || current->stable_id == 0)
		return NULL;
	if (current_index < previous_effect->particle_count) {
		const XwaParticle* candidate = &previous->particles[previous_effect->first_particle + current_index];
		if (candidate->stable_id == current->stable_id)
			return candidate;
	}
	for (uint32_t i = 0; i < previous_effect->particle_count; i++) {
		const XwaParticle* candidate = &previous->particles[previous_effect->first_particle + i];
		if (candidate->stable_id == current->stable_id)
			return candidate;
	}
	return NULL;
}

static void particle_build_previous(PreparedParticleQuad* q, const XwaParticleEffect* previous_effect,
									const XwaParticle* previous_particle,
									const XwaRemasterEffectView* history_view) {
	if (!previous_effect || !previous_particle || !history_view)
		return;
	int classic_w = 0, classic_h = 0, max_bounds = 0;
	if (!XwaSnapshotExport_ModelTexFrame(previous_effect->texture_model_type,
										 previous_particle->texture_frame, &classic_w, &classic_h,
										 &max_bounds) ||
		classic_w <= 0 || classic_h <= 0) {
		return;
	}
	PreparedParticleQuad previous_quad;
	memset(&previous_quad, 0, sizeof previous_quad);
	const int built =
		previous_effect->billboard_mode == XWA_PARTICLE_BILLBOARD_FACING
			? particle_build_facing(&previous_quad, history_view, previous_particle, classic_w, classic_h)
			: particle_build_stretched(&previous_quad, history_view, previous_particle, classic_w,
									   previous_effect->billboard_mode ==
										   XWA_PARTICLE_BILLBOARD_STRETCHED_LOCAL);
	if (!built)
		return;
	memcpy(q->prev_corners, previous_quad.corners, sizeof q->prev_corners);
	q->has_prev = 1;
}

void XwaRemasterParticles_Prepare(XwaRemasterParticles* particles, AeronCommandBuffer* cmd,
								  const XwaSnapshot* snapshot, const XwaSnapshot* previous_snapshot,
								  XwaRemasterAssets* assets, const XwaRemasterEffectView* view,
								  const XwaRemasterEffectView* history_view) {
	if (!particles)
		return;
	particles->count = 0;
	particles->dropped = 0;
	if (!cmd || !snapshot || !assets || !view || view->focal_x_px <= 0.0f || view->focal_y_px <= 0.0f)
		return;
	for (uint32_t i = 0; i < snapshot->particle_effect_count; i++) {
		const XwaParticleEffect* effect = &snapshot->particle_effects[i];
		if (!particle_effect_range_valid(snapshot, effect))
			continue;
		const XwaParticleEffect* previous_effect =
			particle_find_previous_effect(previous_snapshot, i, effect);
		float emitter_eye[3];
		effect_point_to_eye(view, &effect->emitter_world_pos, emitter_eye);
		const float emitter_near =
			effect->billboard_mode == XWA_PARTICLE_BILLBOARD_FACING && !effect->hide_owner_external ? 10.0f
																									: 1.0f;
		if (emitter_eye[2] < emitter_near)
			continue;
		for (uint32_t j = 0; j < effect->particle_count; j++) {
			if (particles->count >= particles->capacity) {
				particles->dropped = 1;
				break;
			}
			const XwaParticle* particle = &snapshot->particles[effect->first_particle + j];
			int classic_w = 0, classic_h = 0, max_bounds = 0;
			if (!XwaSnapshotExport_ModelTexFrame(effect->texture_model_type, particle->texture_frame,
												 &classic_w, &classic_h, &max_bounds) ||
				classic_w <= 0 || classic_h <= 0)
				continue;
			XwaAssetRef ref;
			if (!XwaRemasterAssets_FlightModelFrame(assets, effect->texture_model_type,
													particle->texture_frame, &ref))
				continue;
			PreparedParticleQuad* q = &particles->quads[particles->count];
			memset(q, 0, sizeof *q);
			q->owner_slot = effect->owner_slot;
			q->owner_signature = effect->owner_signature;
			q->source_kind = effect->source_kind;
			q->render_region = effect->render_region;
			q->hide_owner_external = effect->hide_owner_external;
			q->hide_owner_film = effect->hide_owner_film;
			q->texture = ref.texture;
			const int built = effect->billboard_mode == XWA_PARTICLE_BILLBOARD_FACING
								  ? particle_build_facing(q, view, particle, classic_w, classic_h)
								  : particle_build_stretched(q, view, particle, classic_w,
															 effect->billboard_mode ==
																 XWA_PARTICLE_BILLBOARD_STRETCHED_LOCAL);
			if (!built)
				continue;
			const XwaParticle* previous_particle =
				particle_find_previous_record(previous_snapshot, previous_effect, j, particle);
			particle_build_previous(q, previous_effect, previous_particle, history_view);
			particle_set_uv(q, &ref);
			particle_color(particle->argb_color, q->color);
			particles->count++;
		}
	}
	if (particles->dropped)
		Aeron_LogWarn("xwa.remaster", "prepared particle quad cap (%u) hit", particles->capacity);
}

static int particle_hidden_for_view(const PreparedParticleQuad* q, const XwaSnapshot* snapshot) {
	if (!snapshot || q->source_kind != XWA_PARTICLE_SOURCE_OBJECT ||
		q->owner_slot != snapshot->flight_camera.player_obj_idx)
		return 0;
	return (q->hide_owner_film && snapshot->flight_camera.film_overlay) ||
		   (q->hide_owner_external && snapshot->flight_camera.external);
}

static void particle_submit(AeronScene3D* scene, const PreparedParticleQuad* q) {
	AeronSceneBillboardDesc desc;
	memset(&desc, 0, sizeof desc);
	desc.texture = q->texture;
	desc.blend = AERON_SCENE_BILLBOARD_BLEND_PMA;
	desc.stage = AERON_SCENE_BILLBOARD_STAGE_OVERLAY;
	memcpy(desc.corners, q->corners, sizeof desc.corners);
	memcpy(desc.uv, q->uv, sizeof desc.uv);
	for (int i = 0; i < 4; i++)
		memcpy(desc.colors[i], q->color, sizeof q->color);
	if (q->has_prev)
		desc.prev_corners = q->prev_corners;
	AeronScene_AddBillboard(scene, &desc);
}

void XwaRemasterParticles_SubmitRegion(const XwaRemasterParticles* particles, AeronScene3D* scene,
									   const XwaSnapshot* snapshot, uint8_t region) {
	if (!particles || !scene)
		return;
	for (uint32_t i = 0; i < particles->count; i++) {
		const PreparedParticleQuad* q = &particles->quads[i];
		if (q->render_region == region && !particle_hidden_for_view(q, snapshot))
			particle_submit(scene, q);
	}
}

void XwaRemasterParticles_SubmitOwner(const XwaRemasterParticles* particles, AeronScene3D* scene,
									  const XwaSnapshot* snapshot, uint16_t owner_slot,
									  uint16_t owner_signature) {
	if (!particles || !scene)
		return;
	for (uint32_t i = 0; i < particles->count; i++) {
		const PreparedParticleQuad* q = &particles->quads[i];
		if (q->source_kind == XWA_PARTICLE_SOURCE_OBJECT && q->owner_slot == owner_slot &&
			q->owner_signature == owner_signature && !particle_hidden_for_view(q, snapshot))
			particle_submit(scene, q);
	}
}

static float trail_alpha(const XwaTrailEmitter* emitter, float age) {
	if (age < emitter->alpha_fade_start)
		return 1.0f;
	const float alpha = 1.0f - (age - emitter->alpha_fade_start) * emitter->alpha_fade_rate;
	return alpha > 0.0f ? alpha : 0.0f;
}

static int trails_reserve(XwaRemasterTrails* trails) {
	if (trails->count < trails->capacity)
		return 1;
	if (trails->capacity >= EFFECT_MAX_TRAIL_QUADS) {
		trails->dropped = 1;
		return 0;
	}
	uint32_t capacity = trails->capacity ? trails->capacity * 2u : 256u;
	if (capacity > EFFECT_MAX_TRAIL_QUADS)
		capacity = EFFECT_MAX_TRAIL_QUADS;
	PreparedTrailQuad* quads = (PreparedTrailQuad*)realloc(trails->quads, capacity * sizeof *quads);
	if (!quads) {
		trails->dropped = 1;
		return 0;
	}
	trails->quads = quads;
	trails->capacity = capacity;
	return 1;
}

static void trail_color(uint32_t argb, float endpoint_alpha, float out[4]) {
	const float alpha = ((float)((argb >> 24) & 0xffu) * (1.0f / 255.0f)) * endpoint_alpha;
	out[0] = XwaRemaster_SrgbToLinear((float)((argb >> 16) & 0xffu) * (1.0f / 255.0f)) * alpha;
	out[1] = XwaRemaster_SrgbToLinear((float)((argb >> 8) & 0xffu) * (1.0f / 255.0f)) * alpha;
	out[2] = XwaRemaster_SrgbToLinear((float)(argb & 0xffu) * (1.0f / 255.0f)) * alpha;
	out[3] = alpha;
}

static void endpoint_lerp(const TrailEndpoint* a, const TrailEndpoint* b, float t, TrailEndpoint* out) {
	for (int i = 0; i < 3; i++)
		out->eye[i] = a->eye[i] + (b->eye[i] - a->eye[i]) * t;
	out->tex_v = a->tex_v + (b->tex_v - a->tex_v) * t;
	out->alpha = a->alpha + (b->alpha - a->alpha) * t;
	out->width_factor = a->width_factor + (b->width_factor - a->width_factor) * t;
}

static int trail_clip_near(const XwaRemasterEffectView* view, TrailEndpoint* a, TrailEndpoint* b) {
	const float near_z = view->near_z + 0.0001f;
	if (a->eye[2] < near_z && b->eye[2] < near_z)
		return 0;
	if (a->eye[2] < near_z) {
		const float t = (near_z - a->eye[2]) / (b->eye[2] - a->eye[2]);
		TrailEndpoint clipped;
		endpoint_lerp(a, b, t, &clipped);
		*a = clipped;
	} else if (b->eye[2] < near_z) {
		const float t = (near_z - a->eye[2]) / (b->eye[2] - a->eye[2]);
		TrailEndpoint clipped;
		endpoint_lerp(a, b, t, &clipped);
		*b = clipped;
	}
	return 1;
}

static float trail_local_v(float value, int end_of_forward_period) {
	float local = value - floorf(value);
	if (end_of_forward_period && fabsf(local) < 0.00001f)
		local = 1.0f;
	return local;
}

static void trails_append_quad(XwaRemasterTrails* trails, const XwaRemasterEffectView* view,
							   const XwaTrailEmitter* emitter, uint8_t render_region, const XwaAssetRef* ref,
							   const TrailEndpoint* a, const TrailEndpoint* b, float plus_ax, float plus_ay,
							   float plus_bx, float plus_by, float va, float vb) {
	if (!trails_reserve(trails))
		return;
	PreparedTrailQuad* q = &trails->quads[trails->count++];
	memset(q, 0, sizeof *q);
	q->owner_slot = emitter->owner_slot;
	q->owner_signature = emitter->owner_signature;
	q->render_region = render_region;
	q->texture = ref->texture;

	float eye[4][3] = {
		{ a->eye[0] + plus_ax * a->eye[2] / view->focal_x_px,
		  a->eye[1] + plus_ay * a->eye[2] / view->focal_y_px, a->eye[2] },
		{ a->eye[0] - plus_ax * a->eye[2] / view->focal_x_px,
		  a->eye[1] - plus_ay * a->eye[2] / view->focal_y_px, a->eye[2] },
		{ b->eye[0] - plus_bx * b->eye[2] / view->focal_x_px,
		  b->eye[1] - plus_by * b->eye[2] / view->focal_y_px, b->eye[2] },
		{ b->eye[0] + plus_bx * b->eye[2] / view->focal_x_px,
		  b->eye[1] + plus_by * b->eye[2] / view->focal_y_px, b->eye[2] },
	};
	for (int i = 0; i < 4; i++)
		effect_eye_to_scene(view, eye[i], q->corners[i]);

	const float dv = ref->v1 - ref->v0;
	q->uv[0][0] = q->uv[3][0] = ref->u1;
	q->uv[1][0] = q->uv[2][0] = ref->u0;
	q->uv[0][1] = q->uv[1][1] = ref->v0 + va * dv;
	q->uv[2][1] = q->uv[3][1] = ref->v0 + vb * dv;
	trail_color(emitter->argb_color, a->alpha, q->colors[0]);
	memcpy(q->colors[1], q->colors[0], sizeof q->colors[0]);
	trail_color(emitter->argb_color, b->alpha, q->colors[2]);
	memcpy(q->colors[3], q->colors[2], sizeof q->colors[2]);
}

static void trails_split_segment(XwaRemasterTrails* trails, const XwaRemasterEffectView* view,
								 const XwaTrailEmitter* emitter, uint8_t region, const XwaAssetRef* ref,
								 const TrailEndpoint* start, const TrailEndpoint* end, float plus_start_x,
								 float plus_start_y, float plus_end_x, float plus_end_y) {
	const float delta = end->tex_v - start->tex_v;
	if (fabsf(delta) < 0.000001f) {
		trails_append_quad(trails, view, emitter, region, ref, start, end, plus_start_x, plus_start_y,
						   plus_end_x, plus_end_y, trail_local_v(start->tex_v, 0),
						   trail_local_v(end->tex_v, 0));
		return;
	}

	const int forward = delta > 0.0f;
	float t0 = 0.0f;
	for (int split = 0; split < 8 && t0 < 1.0f; split++) {
		const float value0 = start->tex_v + delta * t0;
		const float boundary = forward ? floorf(value0 + 0.00001f) + 1.0f : ceilf(value0 - 0.00001f) - 1.0f;
		float t1 = (boundary - start->tex_v) / delta;
		if (t1 <= t0 + 0.000001f)
			t1 = 1.0f;
		if (t1 > 1.0f)
			t1 = 1.0f;
		TrailEndpoint a, b;
		endpoint_lerp(start, end, t0, &a);
		endpoint_lerp(start, end, t1, &b);
		const float pax = plus_start_x + (plus_end_x - plus_start_x) * t0;
		const float pay = plus_start_y + (plus_end_y - plus_start_y) * t0;
		const float pbx = plus_start_x + (plus_end_x - plus_start_x) * t1;
		const float pby = plus_start_y + (plus_end_y - plus_start_y) * t1;
		const int boundary_end = t1 < 1.0f && forward;
		trails_append_quad(trails, view, emitter, region, ref, &a, &b, pax, pay, pbx, pby,
						   trail_local_v(a.tex_v, 0), trail_local_v(b.tex_v, boundary_end));
		t0 = t1;
	}
}

static void trails_build_emitter(XwaRemasterTrails* trails, const XwaRemasterEffectView* view,
								 const XwaTrailEmitter* emitter, uint8_t region, const XwaTrailPoint* points,
								 const XwaAssetRef* ref) {
	if (emitter->point_count < 2 || view->focal_x_px <= 0.0f || view->focal_y_px <= 0.0f)
		return;
	TrailEndpoint prev;
	effect_world_to_eye(view, points[0].world_pos, prev.eye);
	prev.tex_v = points[0].tex_v;
	prev.alpha = trail_alpha(emitter, points[0].age_fade);
	prev.width_factor = 1.0f + emitter->start_alpha_bias * points[0].age_fade;
	const float screen_scale = 256.0f / prev.eye[2];
	float carried_plus_x = 0.0f, carried_plus_y = 0.0f;
	int have_carried = 0;

	for (uint32_t i = 1; i < emitter->point_count; i++) {
		TrailEndpoint next;
		const XwaTrailPoint* point = &points[i];
		effect_world_to_eye(view, point->world_pos, next.eye);
		float age = point->age_fade;
		if (age >= 1.0f) {
			/* Exact classic tail law: interpolate from the previous point by
			 * 1/nextAge (rather than the conventional age-range fraction). */
			const float t = age != 0.0f ? 1.0f / age : 1.0f;
			for (int a = 0; a < 3; a++)
				next.eye[a] = prev.eye[a] + (next.eye[a] - prev.eye[a]) * t;
			age = 1.0f;
		}
		next.tex_v = point->tex_v;
		next.alpha = trail_alpha(emitter, age);
		next.width_factor = 1.0f + emitter->start_alpha_bias * age;

		TrailEndpoint a = prev, b = next;
		if (!trail_clip_near(view, &a, &b)) {
			prev = next;
			have_carried = 0;
			continue;
		}
		const float sx0 = view->focal_x_px * a.eye[0] / a.eye[2];
		const float sy0 = view->focal_y_px * a.eye[1] / a.eye[2];
		const float sx1 = view->focal_x_px * b.eye[0] / b.eye[2];
		const float sy1 = view->focal_y_px * b.eye[1] / b.eye[2];
		const float angle = atan2f(sy0 - sy1, sx1 - sx0);
		const float direction_x = sinf(angle);
		const float direction_y = cosf(angle);
		const float width0 =
			screen_scale * emitter->ribbon_width * a.width_factor * view->classic_pixel_scale;
		const float width1 =
			screen_scale * emitter->ribbon_width * b.width_factor * view->classic_pixel_scale;
		float plus0_x = direction_x * width0;
		float plus0_y = direction_y * width0;
		if (have_carried && a.eye[2] == prev.eye[2]) {
			plus0_x = carried_plus_x;
			plus0_y = carried_plus_y;
		}
		const float plus1_x = direction_x * width1;
		const float plus1_y = direction_y * width1;
		trails_split_segment(trails, view, emitter, region, ref, &a, &b, plus0_x, plus0_y, plus1_x, plus1_y);
		carried_plus_x = plus1_x;
		carried_plus_y = plus1_y;
		have_carried = 1;
		prev = next;
	}
}

void XwaRemasterTrails_Prepare(XwaRemasterTrails* trails, AeronCommandBuffer* cmd,
							   const XwaSnapshot* snapshot, XwaRemasterAssets* assets,
							   const XwaRemasterEffectView* view) {
	if (!trails)
		return;
	trails->count = 0;
	trails->dropped = 0;
	if (!cmd || !snapshot || !assets || !view)
		return;
	for (uint32_t i = 0; i < snapshot->trail_emitter_count; i++) {
		const XwaTrailEmitter* emitter = &snapshot->trail_emitters[i];
		if (emitter->first_point >= snapshot->trail_point_count ||
			emitter->point_count > snapshot->trail_point_count - emitter->first_point)
			continue;
		const XwaFlightObject* owner =
			effect_find_owner(snapshot, emitter->owner_slot, emitter->owner_signature);
		if (!owner)
			continue;
		XwaAssetRef ref;
		if (!XwaRemasterAssets_FlightModelFrame(assets, emitter->texture_model_type, emitter->texture_frame,
												&ref))
			continue;
		trails_build_emitter(trails, view, emitter, owner->render_region,
							 &snapshot->trail_points[emitter->first_point], &ref);
	}
	if (trails->dropped)
		Aeron_LogWarn("xwa.remaster", "prepared trail quad cap (%u) hit", EFFECT_MAX_TRAIL_QUADS);
}

static void trails_submit_quad(AeronScene3D* scene, const PreparedTrailQuad* q) {
	AeronSceneBillboardDesc desc;
	memset(&desc, 0, sizeof desc);
	desc.texture = q->texture;
	desc.blend = AERON_SCENE_BILLBOARD_BLEND_PMA;
	desc.stage = AERON_SCENE_BILLBOARD_STAGE_OVERLAY;
	memcpy(desc.corners, q->corners, sizeof desc.corners);
	memcpy(desc.uv, q->uv, sizeof desc.uv);
	memcpy(desc.colors, q->colors, sizeof desc.colors);
	/* Historical trail geometry remains in world space. Supplying equal
	 * previous corners gives it camera velocity without pretending the
	 * whole ribbon moved with the projectile. */
	desc.prev_corners = q->corners;
	AeronScene_AddBillboard(scene, &desc);
}

void XwaRemasterTrails_SubmitRegion(const XwaRemasterTrails* trails, AeronScene3D* scene, uint8_t region) {
	if (!trails || !scene)
		return;
	for (uint32_t i = 0; i < trails->count; i++)
		if (trails->quads[i].render_region == region)
			trails_submit_quad(scene, &trails->quads[i]);
}

void XwaRemasterTrails_SubmitOwner(const XwaRemasterTrails* trails, AeronScene3D* scene, uint16_t owner_slot,
								   uint16_t owner_signature) {
	if (!trails || !scene)
		return;
	for (uint32_t i = 0; i < trails->count; i++) {
		const PreparedTrailQuad* q = &trails->quads[i];
		if (q->owner_slot == owner_slot && q->owner_signature == owner_signature)
			trails_submit_quad(scene, q);
	}
}
