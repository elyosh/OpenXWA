/* State-derived HD rendering for XWA's three hyperspace phases. */

#include "xwa_remaster/hyperspace.h"

#include "aeron/aeron.h"
#include "xwa_remaster/flight.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct HyperStreakVertex {
	float position[3];
	float color[4];
} HyperStreakVertex;

typedef struct HyperTextureVertex {
	float position[3];
	float uv[2];
	float color[4];
} HyperTextureVertex;

struct XwaRemasterHyperspace {
	AeronShader* streak_vs;
	AeronShader* streak_fs;
	AeronShader* texture_vs;
	AeronShader* texture_fs;
	AeronGraphicsPipeline* streak_pipeline;
	AeronGraphicsPipeline* tunnel_pipeline;
	AeronGraphicsPipeline* flash_pipeline;
	AeronSampleCount pipeline_samples;
	AeronSampler* sampler;
	AeronBuffer* streak_vb;
	AeronBuffer* texture_vb;
	uint32_t streak_vb_capacity;
	uint32_t streak_vertex_count;
	XwaAssetRef texture_ref;
	uint8_t texture_mode; /* 0 none, 1 tunnel opaque, 2 flash PMA */
	float view_proj[16];
};

static AeronBlendStateDesc hyper_blend_opaque(void) { return (AeronBlendStateDesc) { 0 }; }

static AeronBlendStateDesc hyper_blend_additive(void) {
	return (AeronBlendStateDesc) {
		.enabled = 1,
		.src_color = AERON_BLEND_ONE,
		.dst_color = AERON_BLEND_ONE,
		.color_op = AERON_BLEND_OP_ADD,
		.src_alpha = AERON_BLEND_ZERO,
		.dst_alpha = AERON_BLEND_ONE,
		.alpha_op = AERON_BLEND_OP_ADD,
	};
}

static AeronBlendStateDesc hyper_blend_pma(void) {
	return (AeronBlendStateDesc) {
		.enabled = 1,
		.src_color = AERON_BLEND_ONE,
		.dst_color = AERON_BLEND_ONE_MINUS_SRC_ALPHA,
		.color_op = AERON_BLEND_OP_ADD,
		.src_alpha = AERON_BLEND_ONE,
		.dst_alpha = AERON_BLEND_ONE_MINUS_SRC_ALPHA,
		.alpha_op = AERON_BLEND_OP_ADD,
	};
}

static AeronGraphicsPipeline* hyper_create_pipeline(AeronShader* vs, AeronShader* fs, uint32_t stride,
													const AeronVertexAttributeDesc* attrs,
													uint32_t attr_count, AeronBlendStateDesc blend,
													AeronSampleCount sample_count) {
	const AeronVertexBufferLayoutDesc layout = { .slot = 0, .stride = stride };
	AeronColorTargetStateDesc targets[1] = { 0 };
	targets[0].format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT;
	targets[0].blend = blend;
	return Aeron_CreateGraphicsPipeline(&(AeronGraphicsPipelineDesc) {
		.vertex_shader = vs,
		.fragment_shader = fs,
		.primitive_type = AERON_PRIMITIVE_TRIANGLES,
		.cull_mode = AERON_CULL_NONE,
		.vertex_buffers = &layout,
		.vertex_buffer_count = 1,
		.attributes = attrs,
		.attribute_count = attr_count,
		.depth_format = AERON_TEXTURE_FORMAT_D32_FLOAT,
		.depth = { .depth_test = 0, .depth_write = 0, .compare = AERON_COMPARE_ALWAYS },
		.color_target_count = 1,
		.color_targets = targets,
		.sample_count = sample_count,
	});
}

static void hyper_destroy_pipelines(XwaRemasterHyperspace* h) {
	if (h->streak_pipeline)
		Aeron_DestroyGraphicsPipeline(h->streak_pipeline);
	if (h->tunnel_pipeline)
		Aeron_DestroyGraphicsPipeline(h->tunnel_pipeline);
	if (h->flash_pipeline)
		Aeron_DestroyGraphicsPipeline(h->flash_pipeline);
	h->streak_pipeline = NULL;
	h->tunnel_pipeline = NULL;
	h->flash_pipeline = NULL;
	h->pipeline_samples = 0;
}

static int hyper_ensure_pipelines(XwaRemasterHyperspace* h, AeronSampleCount sample_count) {
	static const AeronVertexAttributeDesc streak_attrs[] = {
		{ .location = 0,
		  .buffer_slot = 0,
		  .format = AERON_VERTEX_FORMAT_FLOAT3,
		  .offset = (uint32_t)offsetof(HyperStreakVertex, position) },
		{ .location = 1,
		  .buffer_slot = 0,
		  .format = AERON_VERTEX_FORMAT_FLOAT4,
		  .offset = (uint32_t)offsetof(HyperStreakVertex, color) },
	};
	static const AeronVertexAttributeDesc texture_attrs[] = {
		{ .location = 0,
		  .buffer_slot = 0,
		  .format = AERON_VERTEX_FORMAT_FLOAT3,
		  .offset = (uint32_t)offsetof(HyperTextureVertex, position) },
		{ .location = 1,
		  .buffer_slot = 0,
		  .format = AERON_VERTEX_FORMAT_FLOAT2,
		  .offset = (uint32_t)offsetof(HyperTextureVertex, uv) },
		{ .location = 2,
		  .buffer_slot = 0,
		  .format = AERON_VERTEX_FORMAT_FLOAT4,
		  .offset = (uint32_t)offsetof(HyperTextureVertex, color) },
	};
	if (h->pipeline_samples == sample_count && h->streak_pipeline && h->tunnel_pipeline &&
		h->flash_pipeline) {
		return 1;
	}
	hyper_destroy_pipelines(h);
	h->streak_pipeline =
		hyper_create_pipeline(h->streak_vs, h->streak_fs, (uint32_t)sizeof(HyperStreakVertex), streak_attrs,
							  2, hyper_blend_additive(), sample_count);
	h->tunnel_pipeline =
		hyper_create_pipeline(h->texture_vs, h->texture_fs, (uint32_t)sizeof(HyperTextureVertex),
							  texture_attrs, 3, hyper_blend_opaque(), sample_count);
	h->flash_pipeline =
		hyper_create_pipeline(h->texture_vs, h->texture_fs, (uint32_t)sizeof(HyperTextureVertex),
							  texture_attrs, 3, hyper_blend_pma(), sample_count);
	if (!h->streak_pipeline || !h->tunnel_pipeline || !h->flash_pipeline) {
		hyper_destroy_pipelines(h);
		return 0;
	}
	h->pipeline_samples = sample_count;
	return 1;
}

XwaRemasterHyperspace* XwaRemasterHyperspace_Create(void) {
	XwaRemasterHyperspace* h = (XwaRemasterHyperspace*)calloc(1, sizeof *h);
	if (!h) {
		return NULL;
	}
	h->streak_vs = Aeron_CreateShader(&(AeronShaderDesc) {
		.name = "hyperspace_streak.vert", .stage = AERON_SHADER_STAGE_VERTEX, .uniform_buffer_count = 1 });
	h->streak_fs = Aeron_CreateShader(
		&(AeronShaderDesc) { .name = "hyperspace_streak.frag", .stage = AERON_SHADER_STAGE_FRAGMENT });
	h->texture_vs = Aeron_CreateShader(
		&(AeronShaderDesc) { .name = "hyperspace_texture.vert", .stage = AERON_SHADER_STAGE_VERTEX });
	h->texture_fs = Aeron_CreateShader(&(AeronShaderDesc) {
		.name = "hyperspace_texture.frag", .stage = AERON_SHADER_STAGE_FRAGMENT, .sampler_count = 1 });
	h->sampler = Aeron_CreateSampler(&(AeronSamplerDesc) {
		.min_filter = AERON_FILTER_LINEAR,
		.mag_filter = AERON_FILTER_LINEAR,
		.mip_filter = AERON_FILTER_LINEAR,
		.address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
		.address_v = AERON_ADDRESS_CLAMP_TO_EDGE,
		.address_w = AERON_ADDRESS_CLAMP_TO_EDGE,
		.max_lod = 1000.0f,
	});
	h->texture_vb =
		Aeron_CreateBuffer(&(AeronBufferDesc) { .size = 6u * (uint32_t)sizeof(HyperTextureVertex),
												.usage = AERON_BUFFER_USAGE_VERTEX,
												.debug_name = "xwa.hyperspace.texture_vertices" });
	if (!h->streak_vs || !h->streak_fs || !h->texture_vs || !h->texture_fs || !h->sampler || !h->texture_vb) {
		Aeron_LogError("xwa.remaster", "hyperspace: GPU resource creation failed");
		XwaRemasterHyperspace_Destroy(h);
		return NULL;
	}
	return h;
}

void XwaRemasterHyperspace_Destroy(XwaRemasterHyperspace* h) {
	if (!h) {
		return;
	}
	if (h->streak_vb)
		Aeron_DestroyBuffer(h->streak_vb);
	if (h->texture_vb)
		Aeron_DestroyBuffer(h->texture_vb);
	hyper_destroy_pipelines(h);
	if (h->sampler)
		Aeron_DestroySampler(h->sampler);
	if (h->streak_vs)
		Aeron_DestroyShader(h->streak_vs);
	if (h->streak_fs)
		Aeron_DestroyShader(h->streak_fs);
	if (h->texture_vs)
		Aeron_DestroyShader(h->texture_vs);
	if (h->texture_fs)
		Aeron_DestroyShader(h->texture_fs);
	free(h);
}

static int hyper_ensure_streak_buffer(XwaRemasterHyperspace* h, uint32_t bytes) {
	if (bytes == 0) {
		return 1;
	}
	if (h->streak_vb && h->streak_vb_capacity >= bytes) {
		return 1;
	}
	uint32_t capacity = h->streak_vb_capacity ? h->streak_vb_capacity : 64u * 1024u;
	while (capacity < bytes) {
		capacity *= 2u;
	}
	if (h->streak_vb) {
		Aeron_DestroyBuffer(h->streak_vb);
	}
	h->streak_vb = Aeron_CreateBuffer(&(AeronBufferDesc) { .size = capacity,
														   .usage = AERON_BUFFER_USAGE_VERTEX,
														   .debug_name = "xwa.hyperspace.streak_vertices" });
	h->streak_vb_capacity = h->streak_vb ? capacity : 0;
	return h->streak_vb != NULL;
}

static void hyper_widescreen_remap(float p[3], const float camera_rows[9], float x_scale) {
	if (x_scale <= 1.0f) {
		return;
	}
	float eye[3];
	for (int r = 0; r < 3; r++) {
		eye[r] =
			camera_rows[r * 3 + 0] * p[0] + camera_rows[r * 3 + 1] * p[1] + camera_rows[r * 3 + 2] * p[2];
	}
	eye[0] *= x_scale;
	for (int c = 0; c < 3; c++) {
		p[c] = camera_rows[0 * 3 + c] * eye[0] + camera_rows[1 * 3 + c] * eye[1] +
			   camera_rows[2 * 3 + c] * eye[2];
	}
}

static void hyper_emit_streak(HyperStreakVertex* out, const XwaHyperspaceStreak* streak, float extent,
							  float transition_y, const float camera_rows[9], float x_scale) {
	XwaFlightObject synthetic;
	memset(&synthetic, 0, sizeof synthetic);
	synthetic.orient_dirty = 1;
	synthetic.pitch = 0x4000u;
	synthetic.roll = streak->roll;
	float model[16];
	XwaRemasterFlight_ObjectModelMatrixForCameraDelta(&synthetic, NULL, 0, model);
	const float half = (float)streak->half_width;
	const float corners[4][3] = {
		{ half, 0.0f, 0.0f },
		{ half, extent, 0.0f },
		{ -half, extent, 0.0f },
		{ -half, 0.0f, 0.0f },
	};
	static const uint8_t indices[6] = { 0, 1, 2, 0, 2, 3 };
	for (int v = 0; v < 6; v++) {
		const float* q = corners[indices[v]];
		float p[3] = {
			model[0] * q[0] + model[1] * q[1] + model[2] * q[2] + (float)streak->offset[0],
			model[4] * q[0] + model[5] * q[1] + model[6] * q[2] + (float)streak->offset[1] - transition_y,
			model[8] * q[0] + model[9] * q[1] + model[10] * q[2] + (float)streak->offset[2],
		};
		hyper_widescreen_remap(p, camera_rows, x_scale);
		memcpy(out[v].position, p, sizeof p);
		out[v].color[0] = out[v].color[1] = out[v].color[2] = out[v].color[3] = 1.0f;
	}
}

static void hyper_cover_uv(const XwaAssetRef* ref, int rt_w, int rt_h, float out[4]) {
	float u0 = ref->u0, v0 = ref->v0, u1 = ref->u1, v1 = ref->v1;
	const int sw = ref->classic_w > 0 ? ref->classic_w : ref->w;
	const int sh = ref->classic_h > 0 ? ref->classic_h : ref->h;
	if (sw > 0 && sh > 0 && rt_w > 0 && rt_h > 0) {
		const float source_aspect = (float)sw / (float)sh;
		const float target_aspect = (float)rt_w / (float)rt_h;
		if (source_aspect > target_aspect) {
			const float keep = target_aspect / source_aspect;
			const float trim = (u1 - u0) * (1.0f - keep) * 0.5f;
			u0 += trim;
			u1 -= trim;
		} else if (source_aspect < target_aspect) {
			const float keep = source_aspect / target_aspect;
			const float trim = (v1 - v0) * (1.0f - keep) * 0.5f;
			v0 += trim;
			v1 -= trim;
		}
	}
	out[0] = u0;
	out[1] = v0;
	out[2] = u1;
	out[3] = v1;
}

static float hyper_texture_center_y(const XwaFlightCamera* camera) {
	if (!camera || camera->vp_h == 0) {
		return 0.0f;
	}

	/* FlightTexQuad::screenY is bottom-origin.  The classic hyperspace
	 * texture is centered at vp_center_y - proj_offset_y rather than at the
	 * raw viewport midpoint (RenderQuad_DrawRotatedSprite converts it back
	 * to top-origin screen space). */
	return 2.0f * ((float)camera->vp_center_y - (float)camera->proj_offset_y) / (float)camera->vp_h - 1.0f;
}

static int hyper_upload_texture_quad(XwaRemasterHyperspace* h, AeronCommandBuffer* cmd,
									 const XwaFlightCamera* camera, float alpha, int rt_w, int rt_h) {
	float uv[4];
	hyper_cover_uv(&h->texture_ref, rt_w, rt_h, uv);
	const float u0 = uv[0], v0 = uv[1], u1 = uv[2], v1 = uv[3];
	const float center_y = hyper_texture_center_y(camera);
	/* Keep the shifted quad covering the viewport.  This is the clip-space
	 * equivalent of the oversized classic sprite being clipped to the flight
	 * viewport. */
	const float half_h = 1.0f + (center_y < 0.0f ? -center_y : center_y);
	const float bottom = center_y - half_h;
	const float top = center_y + half_h;
	HyperTextureVertex verts[6] = {
		{ { -1, bottom, 0 }, { u0, v1 }, { alpha, alpha, alpha, alpha } },
		{ { -1, top, 0 }, { u0, v0 }, { alpha, alpha, alpha, alpha } },
		{ { 1, top, 0 }, { u1, v0 }, { alpha, alpha, alpha, alpha } },
		{ { -1, bottom, 0 }, { u0, v1 }, { alpha, alpha, alpha, alpha } },
		{ { 1, top, 0 }, { u1, v0 }, { alpha, alpha, alpha, alpha } },
		{ { 1, bottom, 0 }, { u1, v1 }, { alpha, alpha, alpha, alpha } },
	};
	return Aeron_UploadBufferDataCmd(cmd, h->texture_vb, 0, verts, (uint32_t)sizeof verts);
}

int XwaRemasterHyperspace_Prepare(XwaRemasterHyperspace* h, AeronCommandBuffer* cmd, const XwaSnapshot* snap,
								  XwaRemasterAssets* assets, const float view_proj[16],
								  const float camera_rows[9], int rt_w, int rt_h) {
	if (!h || !cmd || !snap || !assets || !view_proj || !camera_rows) {
		return 0;
	}
	h->streak_vertex_count = 0;
	h->texture_mode = 0;
	memcpy(h->view_proj, view_proj, sizeof h->view_proj);
	const uint8_t phase = snap->hyperspace.phase;
	const uint32_t ticks = snap->hyperspace.phase_elapsed_ticks;
	if (phase == XWA_HYPERSPACE_TUNNEL) {
		int frame = (int)(snap->hyperspace.tunnel_frame_q16 >> 16);
		if (frame < 1)
			frame = 1;
		if (XwaRemasterAssets_FlightModelFrame(assets, XWA_SNAP_TYPE_HYPER_TUNNEL, frame, &h->texture_ref) &&
			hyper_upload_texture_quad(h, cmd, &snap->flight_camera, 1.0f, rt_w, rt_h)) {
			h->texture_mode = 1;
		}
		return h->texture_mode != 0;
	}
	if (phase != XWA_HYPERSPACE_OUTBOUND && phase != XWA_HYPERSPACE_INBOUND) {
		return 0;
	}

	float extent = 0.0f;
	float transition_y = 0.0f;
	float flash_alpha = 0.0f;
	if (phase == XWA_HYPERSPACE_OUTBOUND) {
		if (ticks < 472u) {
			const float scaled = (float)(ticks >> 2);
			extent = scaled * scaled;
		} else {
			extent = 20000.0f;
			const float t = (float)(ticks * 2u - 944u);
			transition_y = (float)(int)(t * t);
			const int alpha = (int)(((float)(ticks - 472u) / 118.0f) * 255.0f);
			flash_alpha = (float)(uint16_t)alpha / 255.0f;
		}
	} else {
		int remaining = 236 - (int)ticks;
		if (remaining < 0)
			remaining = 0;
		if (remaining < 118) {
			extent = 10000.0f - (1.0f - (float)remaining / 118.0f) * 5000.0f;
		} else {
			extent = 10000.0f;
			const int alpha = (int)((1.0f - (float)ticks / 118.0f) * 255.0f);
			flash_alpha = (float)(uint16_t)alpha / 255.0f;
		}
		transition_y = (float)(int)(-0.1f * (float)(8 * remaining) * (float)(4 * remaining));
	}
	if (flash_alpha < 0.0f)
		flash_alpha = 0.0f;
	if (flash_alpha > 1.0f)
		flash_alpha = 1.0f;
	if (flash_alpha > 0.0f) {
		if (!XwaRemasterAssets_FlightModelFrame(assets, XWA_SNAP_TYPE_LIGHTING_1000, 2,
												&h->texture_ref) ||
			!hyper_upload_texture_quad(h, cmd, &snap->flight_camera, flash_alpha, rt_w, rt_h)) {
			return 0;
		}
		h->texture_mode = 2;
	}

	uint32_t count = snap->hyperspace_streak_count;
	if (count > XWA_SNAP_MAX_HYPERSPACE_STREAKS)
		count = XWA_SNAP_MAX_HYPERSPACE_STREAKS;
	const uint32_t vertex_count = count * 6u;
	const uint32_t bytes = vertex_count * (uint32_t)sizeof(HyperStreakVertex);
	if (vertex_count) {
		if (!hyper_ensure_streak_buffer(h, bytes)) {
			return 0;
		}
		HyperStreakVertex* verts = (HyperStreakVertex*)malloc(bytes);
		if (!verts) {
			return 0;
		}
		const float target_aspect = rt_h > 0 ? (float)rt_w / (float)rt_h : 4.0f / 3.0f;
		const float x_scale = target_aspect > 4.0f / 3.0f ? target_aspect / (4.0f / 3.0f) : 1.0f;
		for (uint32_t i = 0; i < count; i++) {
			hyper_emit_streak(&verts[i * 6u], &snap->hyperspace_streaks[i], extent, transition_y,
							  camera_rows, x_scale);
		}
		if (!Aeron_UploadBufferDataCmd(cmd, h->streak_vb, 0, verts, bytes)) {
			free(verts);
			return 0;
		}
		free(verts);
		h->streak_vertex_count = vertex_count;
	}
	return h->texture_mode != 0 || h->streak_vertex_count != 0;
}

void XwaRemasterHyperspace_Draw(AeronCommandBuffer* command_buffer, AeronRenderPass* pass, int rt_w, int rt_h,
								void* user) {
	XwaRemasterHyperspace* h = (XwaRemasterHyperspace*)user;
	if (!h || !pass) {
		return;
	}
	if (!hyper_ensure_pipelines(h, Aeron_RenderPassGetSampleCount(pass))) {
		Aeron_CommandBufferSetFailure(command_buffer, "Hyperspace pipeline preparation failed");
		return;
	}
	Aeron_SetViewport(pass, &(AeronRectI) { 0, 0, rt_w, rt_h });
	if (h->texture_mode && h->texture_ref.texture) {
		Aeron_BindGraphicsPipeline(pass, h->texture_mode == 1 ? h->tunnel_pipeline : h->flash_pipeline);
		Aeron_BindVertexBuffer(pass, 0, h->texture_vb, 0);
		Aeron_BindTextureSampler(pass, AERON_SHADER_STAGE_FRAGMENT, 0, h->texture_ref.texture, h->sampler);
		Aeron_Draw(pass, 6, 0);
	}
	if (h->streak_vertex_count && h->streak_vb) {
		Aeron_BindGraphicsPipeline(pass, h->streak_pipeline);
		Aeron_BindVertexBuffer(pass, 0, h->streak_vb, 0);
		Aeron_BindUniformData(pass, AERON_SHADER_STAGE_VERTEX, 0, h->view_proj, sizeof h->view_proj);
		Aeron_Draw(pass, h->streak_vertex_count, 0);
	}
}
