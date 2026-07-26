/* XWA glow-mark state -> compact generic Aeron articulated overlays. */

#include "xwa_remaster/glow_marks.h"

#include "aeron/scene/mesh_overlay.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct GlowGeometryCache {
	uint16_t                     owner_signature;
	uint8_t                      pool_kind;
	uint8_t                      pool_index;
	uint32_t                     generation;
	const AeronSceneMesh*        mesh;
	AeronSceneMeshOverlayVertex* vertices;
	uint32_t                     vertex_count;
} GlowGeometryCache;

static GlowGeometryCache s_cache[XWA_SNAP_MAX_GLOW_MARKS];
static uint32_t          s_next_cache_replacement;

static int resolve_texture_frame(XwaRemasterAssets* assets, int texture_model_type, int texture_frame,
								 XwaAssetRef* resolved_frame) {
	return XwaRemasterAssets_FlightModelFrame(assets, texture_model_type, texture_frame, resolved_frame);
}

static uint8_t projected_uv_outcode(float projected_u, float projected_v) {
	uint8_t outcode = 0;
	if (projected_u < -0.5f) {
		outcode = 1;
	} else if (projected_u > 0.5f) {
		outcode = 2;
	}
	if (projected_v < -0.5f) {
		outcode |= 4;
	} else if (projected_v > 0.5f) {
		outcode |= 8;
	}
	return outcode;
}

static GlowGeometryCache* find_or_allocate_geometry_cache(const XwaGlowMark*    mark,
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
	replacement->pool_kind       = mark->pool_kind;
	replacement->pool_index      = mark->pool_index;
	replacement->generation      = mark->generation;
	replacement->mesh            = mesh;
	return replacement;
}

static void build_projected_geometry(GlowGeometryCache* cache, const XwaGlowMark* mark,
									 const AeronSceneMesh* mesh) {
	if (!cache || cache->vertices || !mesh->cpu_vertices || !mesh->cpu_indices) {
		return;
	}
	cache->vertices =
		(AeronSceneMeshOverlayVertex*)malloc((size_t)mesh->index_count * sizeof *cache->vertices);
	if (!cache->vertices) {
		return;
	}
	uint32_t output_vertex_count = 0;
	for (uint32_t triangle_index_offset = 0; triangle_index_offset + 2 < mesh->index_count;
		 triangle_index_offset += 3) {
		const AeronSceneMeshCpuVertex* triangle_vertices[3] = {
			&mesh->cpu_vertices[mesh->cpu_indices[triangle_index_offset]],
			&mesh->cpu_vertices[mesh->cpu_indices[triangle_index_offset + 1]],
			&mesh->cpu_vertices[mesh->cpu_indices[triangle_index_offset + 2]],
		};
		const int mesh_index = (int)floorf(triangle_vertices[0]->mesh_index + 0.5f);
		if (mesh_index < 0 || mesh_index >= 64 ||
			(mark->mesh_mask && !(mark->mesh_mask & ((uint64_t)1 << mesh_index)))) {
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
		face_normal[0]           = edge_from_vertex_0_to_1[1] * edge_from_vertex_0_to_2[2] -
								   edge_from_vertex_0_to_1[2] * edge_from_vertex_0_to_2[1];
		face_normal[1]           = edge_from_vertex_0_to_1[2] * edge_from_vertex_0_to_2[0] -
								   edge_from_vertex_0_to_1[0] * edge_from_vertex_0_to_2[2];
		face_normal[2]           = edge_from_vertex_0_to_1[0] * edge_from_vertex_0_to_2[1] -
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
		float   plane_distances[3];
		float   projected_u[3];
		float   projected_v[3];
		uint8_t shared_clip_mask = 15;
		for (int triangle_vertex_index = 0; triangle_vertex_index < 3; triangle_vertex_index++) {
			float relative_position[3];
			for (int coordinate_axis = 0; coordinate_axis < 3; coordinate_axis++) {
				relative_position[coordinate_axis] =
					triangle_vertices[triangle_vertex_index]->pos[coordinate_axis] -
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
		for (int triangle_vertex_index = 0; triangle_vertex_index < 3; triangle_vertex_index++) {
			AeronSceneMeshOverlayVertex* overlay_vertex = &cache->vertices[output_vertex_count++];
			memcpy(overlay_vertex->pos, triangle_vertices[triangle_vertex_index]->pos,
				   sizeof overlay_vertex->pos);
			overlay_vertex->uv[0]      = projected_u[triangle_vertex_index];
			overlay_vertex->uv[1]      = projected_v[triangle_vertex_index];
			overlay_vertex->mesh_index = triangle_vertices[triangle_vertex_index]->mesh_index;
		}
	}
	cache->vertex_count = output_vertex_count;
}

void XwaRemasterGlowMarks_SubmitObject(AeronScene3D* scene, AeronCommandBuffer* command_buffer,
									   XwaRemasterAssets* assets, const XwaSnapshot* snapshot,
									   const XwaFlightObject* object, const AeronSceneMesh* mesh,
									   const float transform[16], const AeronSceneMeshTable* mesh_table) {
	if (!scene || !command_buffer || !assets || !snapshot || !object || !mesh || !transform) {
		return;
	}
	for (uint32_t glow_mark_index = 0; glow_mark_index < snapshot->glow_mark_count; glow_mark_index++) {
		const XwaGlowMark* mark = &snapshot->glow_marks[glow_mark_index];
		if (mark->owner_slot != object->slot || mark->owner_signature != object->signature ||
			mark->texture_frame == 0 || mark->inv_scale_u == 0.0f || mark->inv_scale_v == 0.0f) {
			continue;
		}
		XwaAssetRef texture_frame;
		if (!resolve_texture_frame(assets, mark->texture_model_type, mark->texture_frame,
								   &texture_frame)) {
			continue;
		}
		GlowGeometryCache* cached_geometry = find_or_allocate_geometry_cache(mark, mesh);
		build_projected_geometry(cached_geometry, mark, mesh);
		if (!cached_geometry->vertices || !cached_geometry->vertex_count) {
			continue;
		}
		const float               atlas_frame_width  = texture_frame.u1 - texture_frame.u0;
		const float               atlas_frame_height = texture_frame.v1 - texture_frame.v0;
		AeronSceneMeshOverlayDesc overlay;
		memset(&overlay, 0, sizeof overlay);
		overlay.vertices     = cached_geometry->vertices;
		overlay.vertex_count = cached_geometry->vertex_count;
		overlay.texture      = texture_frame.texture;
		memcpy(overlay.transform, transform, sizeof overlay.transform);
		overlay.mesh_table  = mesh_table;
		overlay.uv_xform[0] = texture_frame.u0 + 0.5f * atlas_frame_width;
		overlay.uv_xform[1] = texture_frame.v0 + 0.5f * atlas_frame_height;
		overlay.uv_xform[2] = mark->layer_uv_scale * atlas_frame_width;
		overlay.uv_xform[3] = mark->layer_uv_scale * atlas_frame_height;
		overlay.uv_rect[0]  = texture_frame.u0;
		overlay.uv_rect[1]  = texture_frame.v0;
		overlay.uv_rect[2]  = texture_frame.u1;
		overlay.uv_rect[3]  = texture_frame.v1;
		overlay.color[0] = overlay.color[1] = overlay.color[2] = overlay.color[3] = 1.0f;
		/* The classic extra-texture pass reuses the hull's transformed
		 * vertices exactly. Aeron's compact overlay uses a separate vertex
		 * shader, so pull only its test depth slightly toward the camera to
		 * prevent coplanar precision differences from producing holes. */
		overlay.depth_bias_view = 0.05f;
		/* Remaster DAT atlases are premultiplied; PMA implements the
		 * classic source-alpha result without applying alpha twice. */
		overlay.blend     = AERON_SCENE_MESH_OVERLAY_BLEND_PMA;
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
}
