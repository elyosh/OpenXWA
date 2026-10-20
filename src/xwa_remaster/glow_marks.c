/* XWA glow-mark state -> compact generic Aeron articulated overlays. */

#include "xwa_remaster/glow_marks.h"

#include "aeron/asset/opt_model.h"
#include "aeron/collision/face_bvh.h"
#include "aeron/scene/mesh_overlay.h"
#include "xwa_remaster/ship.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct GlowGeometryCache {
	uint16_t owner_signature;
	uint8_t pool_kind;
	uint8_t pool_index;
	uint32_t generation;
	const AeronSceneMesh* mesh;
	AeronSceneMeshOverlayVertex* vertices;
	uint32_t vertex_count;
} GlowGeometryCache;

static GlowGeometryCache s_cache[XWA_SNAP_MAX_GLOW_MARKS];
static uint32_t s_next_cache_replacement;
static AeronFaceBvhScratch s_candidate_scratch;

/* Integer hash (lowbias32) of a mark's pool slot and generation. */
static uint32_t mark_hash(uint32_t pool_index, uint32_t generation) {
	uint32_t x = pool_index * 0x9E3779B9u ^ generation;
	x ^= x >> 16;
	x *= 0x7FEB352Du;
	x ^= x >> 15;
	x *= 0x846CA68Bu;
	x ^= x >> 16;
	return x;
}

/* A blast-pool mark whose DAT image has an effects.yaml flipbook: its frame
 * at the flight clock, offset by a phase per mark, as an array-texture
 * overlay source. The image is sampled at tex * 0.35 + 0.3 (its crop),
 * tex being the classic frame's UV, over every face the classic pass
 * draws, with the game's CLAMP addressing: the classic square shows the
 * crop and the rest of the image spreads around it. `rect` is the whole
 * image in atlas UV, inset half a texel, `crop` the image UV range of the
 * classic square. */
static int blast_mark_frame(const XwaGlowMark* mark, const XwaSnapshot* snapshot, AeronTexture** texture,
							uint32_t* layer, uint32_t* max_lod, float rect[4], float crop[4]) {
	const AeronEffectData* effect_data = XwaRemasterShip_EffectData();
	int group = 0, frames_mode = 0, sprite_id = 0;
	if (!effect_data || mark->pool_kind != 1 ||
		!XwaSnapshotExport_ModelTextureBinding(mark->texture_object_type, &group, &frames_mode, &sprite_id) ||
		!frames_mode) {
		return 0;
	}
	char key[32];
	snprintf(key, sizeof key, "%d-%d", group, mark->texture_frame - 1);
	const int index = AeronEffectData_FindImage(effect_data, key);
	if (index < 0) {
		return 0;
	}
	const double phase = (double)mark_hash(mark->pool_index, mark->generation) / 4294967296.0 *
						 AeronEffectData_ImageDuration(effect_data, index);
	AeronEffectImage image;
	AeronEffectAtlas* atlas = XwaRemasterShip_EffectAtlas();
	const float page = (float)AeronEffectAtlas_PageSize(atlas);
	if (!AeronEffectData_ImageFrame(effect_data, index,
									(double)snapshot->game_time_ticks * XWA_SNAP_GAME_TICK_SECONDS + phase,
									&image) ||
		!(page > 0.0f)) {
		return 0;
	}
	*texture = AeronEffectAtlas_Texture(atlas, AERON_EFFECT_LAYER);
	*layer = image.layer[AERON_EFFECT_LAYER];
	*max_lod = image.lod[AERON_EFFECT_LAYER];
	const uint32_t* r = image.rect[AERON_EFFECT_LAYER];
	rect[0] = ((float)r[0] + 0.5f) / page;
	rect[1] = ((float)r[1] + 0.5f) / page;
	rect[2] = ((float)(r[0] + r[2]) - 0.5f) / page;
	rect[3] = ((float)(r[1] + r[3]) - 0.5f) / page;
	memcpy(crop, image.crop, sizeof image.crop);
	return *texture != NULL;
}

static int resolve_texture_frame(XwaRemasterAssets* assets, int texture_object_type, int texture_frame,
								 XwaAssetRef* resolved_frame) {
	return XwaRemasterAssets_FlightModelFrame(assets, texture_object_type, texture_frame, resolved_frame);
}

static uint8_t projected_uv_outcode_within(float projected_u, float projected_v, float limit) {
	uint8_t outcode = 0;
	if (projected_u < -limit) {
		outcode = 1;
	} else if (projected_u > limit) {
		outcode = 2;
	}
	if (projected_v < -limit) {
		outcode |= 4;
	} else if (projected_v > limit) {
		outcode |= 8;
	}
	return outcode;
}

static uint8_t projected_uv_outcode(float projected_u, float projected_v) {
	return projected_uv_outcode_within(projected_u, projected_v, 0.5f);
}

static float dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

/* Slabs (mesh frame, metres) that contain every triangle the per-triangle
 * code of build_projected_geometry can keep, so a render-BVH query returns a
 * superset of its output. A vertex's projected u is dot(p - c, u') *
 * inv_scale_u in OPT units, u' being u_axis projected onto the mark plane;
 * a triangle survives the shared outcode test only if its u and v intervals
 * overlap +-limit, and in plane mode only if some vertex is within 1 OPT unit
 * of the plane. Returns 0 when the mark has no finite visible square. */
static uint32_t build_candidate_slabs(const XwaGlowMark* mark, AeronFaceBvhSlab slabs[3]) {
	float limit;
	if (mark->world_segment_mode) {
		/* The geometry is cached for the mark's lifetime while its scale
		 * animates, so use the largest visible square it will reach. */
		if (!(mark->layer_uv_scale_min > 0.0f)) {
			return 0;
		}
		limit = 0.5f / mark->layer_uv_scale_min;
	} else {
		limit = 0.5f;
	}
	const double units = AERON_OPT_UNITS_PER_METER;
	const float* axes[2] = { mark->u_axis, mark->v_axis };
	const float inv_scales[2] = { mark->inv_scale_u, mark->inv_scale_v };
	const float along_normal[2] = { dot3(mark->u_axis, mark->normal), dot3(mark->v_axis, mark->normal) };
	for (int axis = 0; axis < 2; axis++) {
		AeronFaceBvhSlab* slab = &slabs[axis];
		for (int coordinate_axis = 0; coordinate_axis < 3; coordinate_axis++) {
			slab->normal[coordinate_axis] =
				axes[axis][coordinate_axis] - along_normal[axis] * mark->normal[coordinate_axis];
		}
		const double center = dot3(mark->center, slab->normal) / units;
		const double half = (double)limit / (fabs((double)inv_scales[axis]) * units);
		slab->min = (float)(center - half);
		slab->max = (float)(center + half);
	}
	if (mark->world_segment_mode) {
		return 2;
	}
	memcpy(slabs[2].normal, mark->normal, sizeof slabs[2].normal);
	const double center = dot3(mark->center, mark->normal) / units;
	slabs[2].min = (float)(center - 1.0 / units);
	slabs[2].max = (float)(center + 1.0 / units);
	return 3;
}

/* Reserves room for one more triangle; the buffer doubles from 64 vertices. */
static int reserve_triangle(GlowGeometryCache* cache, uint32_t vertex_count, uint32_t* capacity) {
	if (vertex_count + 3 <= *capacity) {
		return 1;
	}
	const uint32_t grown_capacity = *capacity ? *capacity * 2 : 64;
	AeronSceneMeshOverlayVertex* grown =
		(AeronSceneMeshOverlayVertex*)realloc(cache->vertices, (size_t)grown_capacity * sizeof *grown);
	if (!grown) {
		return 0;
	}
	cache->vertices = grown;
	*capacity = grown_capacity;
	return 1;
}

static GlowGeometryCache* find_or_allocate_geometry_cache(const XwaGlowMark* mark,
														  const AeronSceneMesh* mesh) {
	for (uint32_t cache_index = 0; cache_index < XWA_SNAP_MAX_GLOW_MARKS; cache_index++) {
		GlowGeometryCache* cached_geometry = &s_cache[cache_index];
		if (cached_geometry->vertices && cached_geometry->owner_signature == mark->owner_signature &&
			cached_geometry->pool_kind == mark->pool_kind &&
			cached_geometry->pool_index == mark->pool_index &&
			cached_geometry->generation == mark->generation && cached_geometry->mesh == mesh) {
			return cached_geometry;
		}
	}
	GlowGeometryCache* replacement = &s_cache[s_next_cache_replacement++ % XWA_SNAP_MAX_GLOW_MARKS];
	free(replacement->vertices);
	memset(replacement, 0, sizeof *replacement);
	replacement->owner_signature = mark->owner_signature;
	replacement->pool_kind = mark->pool_kind;
	replacement->pool_index = mark->pool_index;
	replacement->generation = mark->generation;
	replacement->mesh = mesh;
	return replacement;
}

static void build_projected_geometry(GlowGeometryCache* cache, const XwaGlowMark* mark,
									 const AeronSceneMesh* mesh) {
	if (!cache || cache->vertices || !mesh->cpu_vertices || !mesh->cpu_indices) {
		return;
	}
	uint32_t output_capacity = 0;
	if (!reserve_triangle(cache, 0, &output_capacity)) {
		return;
	}
	/* Render-BVH candidates, ascending, when the mesh has one; otherwise
	 * every triangle. Either way the per-triangle code decides. */
	const uint32_t* candidates = NULL;
	uint32_t candidate_count = mesh->index_count / 3;
	AeronFaceBvhSlab slabs[3];
	const uint32_t slab_count = mesh->render_bvh ? build_candidate_slabs(mark, slabs) : 0;
	if (slab_count && mesh->render_bvh->face_count == mesh->index_count / 3) {
		const uint32_t count =
			AeronFaceBvh_CollectSlabs(mesh->render_bvh, slabs, slab_count, &s_candidate_scratch);
		if (count != UINT32_MAX) {
			candidates = s_candidate_scratch.indices;
			candidate_count = count;
		}
	}
	int mesh_filtered = 0;
	for (uint32_t word = 0; word < XWA_SNAP_GLOW_MARK_MESH_WORDS; word++) {
		mesh_filtered |= mark->mesh_mask[word] != 0;
	}
	uint32_t output_vertex_count = 0;
	for (uint32_t candidate_index = 0; candidate_index < candidate_count; candidate_index++) {
		const uint32_t triangle_index_offset =
			3 * (candidates ? candidates[candidate_index] : candidate_index);
		const AeronSceneMeshCpuVertex* triangle_vertices[3] = {
			&mesh->cpu_vertices[mesh->cpu_indices[triangle_index_offset]],
			&mesh->cpu_vertices[mesh->cpu_indices[triangle_index_offset + 1]],
			&mesh->cpu_vertices[mesh->cpu_indices[triangle_index_offset + 2]],
		};
		const int mesh_index = (int)floorf(triangle_vertices[0]->component + 0.5f);
		if (mesh_index < 0 || mesh_index >= XWA_SNAP_MAX_MESH_SLOTS ||
			(mesh_filtered && !(mark->mesh_mask[mesh_index / 64] & ((uint64_t)1 << (mesh_index % 64))))) {
			continue;
		}
		float edge_from_vertex_0_to_1[3];
		float edge_from_vertex_0_to_2[3];
		float face_normal[3];
		for (int coordinate_axis = 0; coordinate_axis < 3; coordinate_axis++) {
			edge_from_vertex_0_to_1[coordinate_axis] =
				triangle_vertices[1]->pos[coordinate_axis] - triangle_vertices[0]->pos[coordinate_axis];
			edge_from_vertex_0_to_2[coordinate_axis] =
				triangle_vertices[2]->pos[coordinate_axis] - triangle_vertices[0]->pos[coordinate_axis];
		}
		face_normal[0] = edge_from_vertex_0_to_1[1] * edge_from_vertex_0_to_2[2] -
						 edge_from_vertex_0_to_1[2] * edge_from_vertex_0_to_2[1];
		face_normal[1] = edge_from_vertex_0_to_1[2] * edge_from_vertex_0_to_2[0] -
						 edge_from_vertex_0_to_1[0] * edge_from_vertex_0_to_2[2];
		face_normal[2] = edge_from_vertex_0_to_1[0] * edge_from_vertex_0_to_2[1] -
						 edge_from_vertex_0_to_1[1] * edge_from_vertex_0_to_2[0];
		float face_normal_length = sqrtf(face_normal[0] * face_normal[0] + face_normal[1] * face_normal[1] +
										 face_normal[2] * face_normal[2]);
		if (face_normal_length < 1e-6f) {
			continue;
		}
		for (int coordinate_axis = 0; coordinate_axis < 3; coordinate_axis++) {
			face_normal[coordinate_axis] /= face_normal_length;
		}
		float summed_vertex_normals[3] = {
			triangle_vertices[0]->normal[0] + triangle_vertices[1]->normal[0] +
				triangle_vertices[2]->normal[0],
			triangle_vertices[0]->normal[1] + triangle_vertices[1]->normal[1] +
				triangle_vertices[2]->normal[1],
			triangle_vertices[0]->normal[2] + triangle_vertices[1]->normal[2] +
				triangle_vertices[2]->normal[2],
		};
		if (face_normal[0] * summed_vertex_normals[0] + face_normal[1] * summed_vertex_normals[1] +
				face_normal[2] * summed_vertex_normals[2] <
			0) {
			for (int coordinate_axis = 0; coordinate_axis < 3; coordinate_axis++) {
				face_normal[coordinate_axis] = -face_normal[coordinate_axis];
			}
		}
		float plane_distances[3];
		float projected_u[3];
		float projected_v[3];
		uint8_t shared_clip_mask = 15;
		for (int triangle_vertex_index = 0; triangle_vertex_index < 3; triangle_vertex_index++) {
			float relative_position[3];
			for (int coordinate_axis = 0; coordinate_axis < 3; coordinate_axis++) {
				/* Aeron retains flight-model vertices in metres, while the
				 * classic projector parameters remain in native OPT units. */
				relative_position[coordinate_axis] =
					triangle_vertices[triangle_vertex_index]->pos[coordinate_axis] *
						AERON_OPT_UNITS_PER_METER -
					mark->center[coordinate_axis];
			}
			plane_distances[triangle_vertex_index] = relative_position[0] * mark->normal[0] +
													 relative_position[1] * mark->normal[1] +
													 relative_position[2] * mark->normal[2];
			float position_on_projector_plane[3];
			for (int coordinate_axis = 0; coordinate_axis < 3; coordinate_axis++) {
				position_on_projector_plane[coordinate_axis] =
					relative_position[coordinate_axis] -
					plane_distances[triangle_vertex_index] * mark->normal[coordinate_axis];
			}
			projected_u[triangle_vertex_index] = (position_on_projector_plane[0] * mark->u_axis[0] +
												  position_on_projector_plane[1] * mark->u_axis[1] +
												  position_on_projector_plane[2] * mark->u_axis[2]) *
												 mark->inv_scale_u;
			projected_v[triangle_vertex_index] = (position_on_projector_plane[0] * mark->v_axis[0] +
												  position_on_projector_plane[1] * mark->v_axis[1] +
												  position_on_projector_plane[2] * mark->v_axis[2]) *
												 mark->inv_scale_v;
			shared_clip_mask &=
				projected_uv_outcode(projected_u[triangle_vertex_index], projected_v[triangle_vertex_index]);
		}
		/* A world-segment triangle entirely past one edge of the visible
		 * square |uv| <= 0.5 / layer_uv_scale draws nothing: the overlay
		 * shader discards outside it and UVs interpolate convexly. The
		 * geometry is cached while layer_uv_scale animates, so cull against
		 * the largest square the mark will reach. */
		if (mark->world_segment_mode && mark->layer_uv_scale_min > 0.0f) {
			const float visible_limit = 0.5f / mark->layer_uv_scale_min;
			uint8_t visible_clip_mask = 15;
			for (int triangle_vertex_index = 0; triangle_vertex_index < 3; triangle_vertex_index++) {
				visible_clip_mask &= projected_uv_outcode_within(
					projected_u[triangle_vertex_index], projected_v[triangle_vertex_index], visible_limit);
			}
			if (visible_clip_mask) {
				continue;
			}
		}
		if (!mark->world_segment_mode) {
			if (shared_clip_mask ||
				face_normal[0] * mark->normal[0] + face_normal[1] * mark->normal[1] +
						face_normal[2] * mark->normal[2] <
					0.0f ||
				(fabsf(plane_distances[0]) > 1.0f && fabsf(plane_distances[1]) > 1.0f &&
				 fabsf(plane_distances[2]) > 1.0f)) {
				continue;
			}
			const float u_normal_offset =
				(mark->u_axis[0] * face_normal[0] + mark->u_axis[1] * face_normal[1] +
				 mark->u_axis[2] * face_normal[2]) *
				mark->inv_scale_u;
			const float v_normal_offset =
				(mark->v_axis[0] * face_normal[0] + mark->v_axis[1] * face_normal[1] +
				 mark->v_axis[2] * face_normal[2]) *
				mark->inv_scale_v;
			for (int triangle_vertex_index = 0; triangle_vertex_index < 3; triangle_vertex_index++) {
				projected_u[triangle_vertex_index] -=
					plane_distances[triangle_vertex_index] * u_normal_offset;
				projected_v[triangle_vertex_index] -=
					plane_distances[triangle_vertex_index] * v_normal_offset;
			}
		}
		if (!reserve_triangle(cache, output_vertex_count, &output_capacity)) {
			free(cache->vertices);
			cache->vertices = NULL;
			return;
		}
		for (int triangle_vertex_index = 0; triangle_vertex_index < 3; triangle_vertex_index++) {
			AeronSceneMeshOverlayVertex* overlay_vertex = &cache->vertices[output_vertex_count++];
			memcpy(overlay_vertex->pos, triangle_vertices[triangle_vertex_index]->pos,
				   sizeof overlay_vertex->pos);
			overlay_vertex->uv[0] = projected_u[triangle_vertex_index];
			overlay_vertex->uv[1] = projected_v[triangle_vertex_index];
			overlay_vertex->component = triangle_vertices[triangle_vertex_index]->component;
		}
	}
	/* Shrink to the output; keep at least one vertex so an empty result
	 * stays cached instead of being rebuilt every frame. */
	AeronSceneMeshOverlayVertex* shrunk = (AeronSceneMeshOverlayVertex*)realloc(
		cache->vertices, (size_t)(output_vertex_count ? output_vertex_count : 1) * sizeof *shrunk);
	if (shrunk) {
		cache->vertices = shrunk;
	}
	cache->vertex_count = output_vertex_count;
}

void XwaRemasterGlowMarks_SubmitObject(AeronScene3D* scene, AeronCommandBuffer* command_buffer,
									   XwaRemasterAssets* assets, const XwaSnapshot* snapshot,
									   const XwaFlightObject* object, const AeronSceneMesh* mesh,
									   const float transform[16], const AeronSceneMeshTable* mesh_table,
									   float emissive_strength) {
	if (!scene || !command_buffer || !assets || !snapshot || !object || !mesh || !transform) {
		return;
	}
	for (uint32_t glow_mark_index = 0; glow_mark_index < snapshot->glow_mark_count; glow_mark_index++) {
		const XwaGlowMark* mark = &snapshot->glow_marks[glow_mark_index];
		if (mark->owner_slot != object->slot || mark->owner_signature != object->signature ||
			mark->texture_frame == 0 || mark->inv_scale_u == 0.0f || mark->inv_scale_v == 0.0f) {
			continue;
		}
		AeronTexture* blast_texture = NULL;
		uint32_t blast_layer = 0, blast_max_lod = 0;
		float blast_rect[4], blast_crop[4];
		const int blast = blast_mark_frame(mark, snapshot, &blast_texture, &blast_layer, &blast_max_lod,
										   blast_rect, blast_crop);
		XwaAssetRef texture_frame;
		if (blast) {
			/* The classic square: the crop of the image. */
			const float width = blast_rect[2] - blast_rect[0], height = blast_rect[3] - blast_rect[1];
			memset(&texture_frame, 0, sizeof texture_frame);
			texture_frame.u0 = blast_rect[0] + blast_crop[0] * width;
			texture_frame.v0 = blast_rect[1] + blast_crop[1] * height;
			texture_frame.u1 = blast_rect[0] + blast_crop[2] * width;
			texture_frame.v1 = blast_rect[1] + blast_crop[3] * height;
		} else if (!resolve_texture_frame(assets, mark->texture_object_type, mark->texture_frame,
										  &texture_frame)) {
			continue;
		}
		GlowGeometryCache* cached_geometry = find_or_allocate_geometry_cache(mark, mesh);
		build_projected_geometry(cached_geometry, mark, mesh);
		if (!cached_geometry->vertices || !cached_geometry->vertex_count) {
			continue;
		}
		const float atlas_frame_width = texture_frame.u1 - texture_frame.u0;
		const float atlas_frame_height = texture_frame.v1 - texture_frame.v0;
		AeronSceneMeshOverlayDesc overlay;
		memset(&overlay, 0, sizeof overlay);
		overlay.vertices = cached_geometry->vertices;
		overlay.vertex_count = cached_geometry->vertex_count;
		overlay.texture = texture_frame.texture;
		overlay.texture_array = blast_texture;
		overlay.texture_layer = blast_layer;
		overlay.texture_max_lod = blast_max_lod;
		memcpy(overlay.transform, transform, sizeof overlay.transform);
		overlay.mesh_table = mesh_table;
		overlay.uv_xform[0] = texture_frame.u0 + 0.5f * atlas_frame_width;
		overlay.uv_xform[1] = texture_frame.v0 + 0.5f * atlas_frame_height;
		overlay.uv_xform[2] = mark->layer_uv_scale * atlas_frame_width;
		overlay.uv_xform[3] = mark->layer_uv_scale * atlas_frame_height;
		overlay.uv_rect[0] = blast ? blast_rect[0] : texture_frame.u0;
		overlay.uv_rect[1] = blast ? blast_rect[1] : texture_frame.v0;
		overlay.uv_rect[2] = blast ? blast_rect[2] : texture_frame.u1;
		overlay.uv_rect[3] = blast ? blast_rect[3] : texture_frame.v1;
		overlay.texture_clamp = (uint8_t)blast;
		const float mark_emissive_strength = mark->pool_kind == 0 ? emissive_strength : 1.0f;
		overlay.color[0] = overlay.color[1] = overlay.color[2] = mark_emissive_strength;
		overlay.color[3] = 1.0f;
		/* The classic extra-texture pass reuses the hull's transformed
		 * vertices exactly. Aeron's compact overlay uses a separate vertex
		 * shader, so pull only its test depth slightly toward the camera to
		 * prevent coplanar precision differences from producing holes. */
		overlay.depth_bias_view = 0.05f;
		/* Remaster DAT atlases are premultiplied; PMA implements the
		 * classic source-alpha result without applying alpha twice. */
		overlay.blend = AERON_SCENE_MESH_OVERLAY_BLEND_PMA;
		overlay.cull_mode = AERON_CULL_BACK;
		AeronScene_AddMeshOverlay(scene, &overlay);
	}
}

void XwaRemasterGlowMarks_InvalidateMesh(const AeronSceneMesh* mesh) {
	for (uint32_t cache_index = 0; cache_index < XWA_SNAP_MAX_GLOW_MARKS; cache_index++) {
		GlowGeometryCache* cached_geometry = &s_cache[cache_index];
		if (cached_geometry->mesh != mesh) {
			continue;
		}
		free(cached_geometry->vertices);
		memset(cached_geometry, 0, sizeof *cached_geometry);
	}
}

void XwaRemasterGlowMarks_Shutdown(void) {
	for (uint32_t cache_index = 0; cache_index < XWA_SNAP_MAX_GLOW_MARKS; cache_index++) {
		free(s_cache[cache_index].vertices);
	}
	memset(s_cache, 0, sizeof s_cache);
	s_next_cache_replacement = 0;
	AeronFaceBvhScratch_Free(&s_candidate_scratch);
}
