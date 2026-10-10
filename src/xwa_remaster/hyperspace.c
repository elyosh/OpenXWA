/* State-derived HD rendering for XWA's three hyperspace phases. */

#include "xwa_remaster/hyperspace.h"

#include "aeron/aeron.h"
#include "aeron/asset/opt_model.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/flight.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct HyperStreakVertex {
	float position[3];
	float color[4];
} HyperStreakVertex;

typedef struct HyperTunnelUniform {
	float view[4];
	float projection[4];
	float motion[4];
	float appearance[4];
	float geometry[4];
	float tunnel_right[4];
	float tunnel_up[4];
	float tunnel_forward[4];
	float dark_color[4];
	float body_color[4];
	float highlight_color[4];
	float cap_color[4];
} HyperTunnelUniform;

typedef struct HyperEnvironmentUniform {
	float roughness;
	float _pad[3];
} HyperEnvironmentUniform;
typedef char HyperEnvironmentUniformSizeCheck[sizeof(HyperEnvironmentUniform) == 16 ? 1 : -1];

enum {
	HYPER_ENVIRONMENT_FACE_SIZE = 32,
	HYPER_ENVIRONMENT_ATLAS_WIDTH = HYPER_ENVIRONMENT_FACE_SIZE * 6,
};

struct XwaRemasterHyperspace {
	AeronShader* streak_vs;
	AeronShader* streak_fs;
	AeronShader* tunnel_vs;
	AeronShader* tunnel_fs;
	AeronGraphicsPipeline* streak_pipeline;
	AeronGraphicsPipeline* tunnel_pipeline;
	AeronComputePipeline* environment_pipeline;
	AeronTexture* environment_atlas;
	AeronTexture* environment_cube;
	AeronSampler* environment_sampler;
	AeronSampleCount pipeline_samples;
	AeronBuffer* streak_vb;
	uint32_t streak_vb_capacity;
	uint32_t streak_vertex_count;
	uint8_t draw_background;
	float view_proj[16];
	XwaFlightHyperspaceTunnelParams params;
	HyperTunnelUniform tunnel_uniform;
	XwaRemasterHyperspaceLighting lighting;
};

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

static AeronGraphicsPipeline* hyper_create_fullscreen_pipeline(AeronShader* vs, AeronShader* fs,
															   AeronSampleCount sample_count) {
	AeronColorTargetStateDesc target = {
		.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
		.blend = hyper_blend_pma(),
	};
	return Aeron_CreateGraphicsPipeline(&(AeronGraphicsPipelineDesc) {
		.vertex_shader = vs,
		.fragment_shader = fs,
		.primitive_type = AERON_PRIMITIVE_TRIANGLES,
		.cull_mode = AERON_CULL_NONE,
		.depth_format = AERON_TEXTURE_FORMAT_D32_FLOAT,
		.depth = { .depth_test = 0, .depth_write = 0, .compare = AERON_COMPARE_ALWAYS },
		.color_target_count = 1,
		.color_targets = &target,
		.sample_count = sample_count,
	});
}

static void hyper_destroy_pipelines(XwaRemasterHyperspace* h) {
	if (h->streak_pipeline)
		Aeron_DestroyGraphicsPipeline(h->streak_pipeline);
	if (h->tunnel_pipeline)
		Aeron_DestroyGraphicsPipeline(h->tunnel_pipeline);
	h->streak_pipeline = NULL;
	h->tunnel_pipeline = NULL;
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
	if (h->pipeline_samples == sample_count && h->streak_pipeline && h->tunnel_pipeline) {
		return 1;
	}
	hyper_destroy_pipelines(h);
	h->streak_pipeline =
		hyper_create_pipeline(h->streak_vs, h->streak_fs, (uint32_t)sizeof(HyperStreakVertex), streak_attrs,
							  2, hyper_blend_additive(), sample_count);
	h->tunnel_pipeline = hyper_create_fullscreen_pipeline(h->tunnel_vs, h->tunnel_fs, sample_count);
	if (!h->streak_pipeline || !h->tunnel_pipeline) {
		hyper_destroy_pipelines(h);
		return 0;
	}
	h->pipeline_samples = sample_count;
	return 1;
}

XwaRemasterHyperspace* XwaRemasterHyperspace_Create(const XwaFlightHyperspaceTunnelParams* params) {
	if (!params) {
		return NULL;
	}
	XwaRemasterHyperspace* h = (XwaRemasterHyperspace*)calloc(1, sizeof *h);
	if (!h) {
		return NULL;
	}
	h->streak_vs = Aeron_CreateShader(&(AeronShaderDesc) {
		.name = "hyperspace_streak.vert", .stage = AERON_SHADER_STAGE_VERTEX, .uniform_buffer_count = 1 });
	h->streak_fs = Aeron_CreateShader(
		&(AeronShaderDesc) { .name = "hyperspace_streak.frag", .stage = AERON_SHADER_STAGE_FRAGMENT });
	h->tunnel_vs = Aeron_CreateShader(
		&(AeronShaderDesc) { .name = "hyperspace_tunnel.vert", .stage = AERON_SHADER_STAGE_VERTEX });
	h->tunnel_fs = Aeron_CreateShader(&(AeronShaderDesc) {
		.name = "hyperspace_tunnel.frag", .stage = AERON_SHADER_STAGE_FRAGMENT, .uniform_buffer_count = 1 });
	h->environment_pipeline = Aeron_CreateComputePipeline(&(AeronComputePipelineDesc) {
		.name = "hyperspace_environment.comp",
		.readwrite_storage_texture_count = 1,
		.uniform_buffer_count = 2,
		.thread_count_x = 8,
		.thread_count_y = 8,
		.thread_count_z = 1,
	});
	h->environment_atlas = Aeron_CreateTexture(&(AeronTextureDesc) {
		.width = HYPER_ENVIRONMENT_ATLAS_WIDTH,
		.height = HYPER_ENVIRONMENT_FACE_SIZE,
		.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
		.usage = AERON_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE | AERON_TEXTURE_USAGE_TRANSFER_SRC,
		.debug_name = "xwa.hyperspace.environment_atlas",
	});
	h->environment_cube = Aeron_CreateTexture(&(AeronTextureDesc) {
		.width = HYPER_ENVIRONMENT_FACE_SIZE,
		.height = HYPER_ENVIRONMENT_FACE_SIZE,
		.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
		.usage = AERON_TEXTURE_USAGE_SAMPLED | AERON_TEXTURE_USAGE_TRANSFER_DST,
		.cube = 1,
		.debug_name = "xwa.hyperspace.environment",
	});
	h->environment_sampler = Aeron_CreateSampler(&(AeronSamplerDesc) {
		.min_filter = AERON_FILTER_LINEAR,
		.mag_filter = AERON_FILTER_LINEAR,
		.mip_filter = AERON_FILTER_LINEAR,
		.address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
		.address_v = AERON_ADDRESS_CLAMP_TO_EDGE,
		.address_w = AERON_ADDRESS_CLAMP_TO_EDGE,
	});
	if (!h->streak_vs || !h->streak_fs || !h->tunnel_vs || !h->tunnel_fs ||
		!h->environment_pipeline || !h->environment_atlas || !h->environment_cube ||
		!h->environment_sampler) {
		Aeron_LogError("xwa.remaster", "hyperspace: GPU resource creation failed");
		XwaRemasterHyperspace_Destroy(h);
		return NULL;
	}
	h->params = *params;
	return h;
}

void XwaRemasterHyperspace_SetParams(XwaRemasterHyperspace* h,
									 const XwaFlightHyperspaceTunnelParams* params) {
	if (h && params) {
		h->params = *params;
	}
}

void XwaRemasterHyperspace_Destroy(XwaRemasterHyperspace* h) {
	if (!h) {
		return;
	}
	if (h->streak_vb)
		Aeron_DestroyBuffer(h->streak_vb);
	hyper_destroy_pipelines(h);
	if (h->streak_vs)
		Aeron_DestroyShader(h->streak_vs);
	if (h->streak_fs)
		Aeron_DestroyShader(h->streak_fs);
	if (h->tunnel_vs)
		Aeron_DestroyShader(h->tunnel_vs);
	if (h->tunnel_fs)
		Aeron_DestroyShader(h->tunnel_fs);
	if (h->environment_pipeline)
		Aeron_DestroyComputePipeline(h->environment_pipeline);
	if (h->environment_atlas)
		Aeron_DestroyTexture(h->environment_atlas);
	if (h->environment_cube)
		Aeron_DestroyTexture(h->environment_cube);
	if (h->environment_sampler)
		Aeron_DestroySampler(h->environment_sampler);
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
	/* The shared model matrix expects meter-space geometry; streak seeds retain XWA model units. */
	const float model_units_to_meters = AERON_OPT_METERS_PER_UNIT;
	const float half = (float)streak->half_width * model_units_to_meters;
	extent *= model_units_to_meters;
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

static float hyper_transition_center_y(const XwaFlightCamera* camera) {
	if (!camera || camera->vp_h == 0) {
		return 0.5f;
	}

	/* The recovered transition sprite uses bottom-origin screen coordinates. */
	return 1.0f - ((float)camera->vp_center_y - (float)camera->proj_offset_y) / (float)camera->vp_h;
}

static float hyper_dot3(const float a[3], const float b[3]) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static int hyper_normalize(float value[3]) {
	const float length = sqrtf(hyper_dot3(value, value));
	if (!isfinite(length) || length <= 0.0001f) {
		return 0;
	}
	value[0] /= length;
	value[1] /= length;
	value[2] /= length;
	return 1;
}

static void hyper_view_to_world(const float camera_rows[9], const float view[3], float world[3]) {
	for (int component = 0; component < 3; component++) {
		world[component] = camera_rows[0 * 3 + component] * view[0] +
						   camera_rows[1 * 3 + component] * view[1] +
						   camera_rows[2 * 3 + component] * view[2];
	}
}

static void hyper_add_uniform_ambient(XwaShipAmbientCube* cube, const float color[3]) {
	float* const lobes[6] = { cube->pos_x, cube->neg_x, cube->pos_y, cube->neg_y, cube->pos_z, cube->neg_z };
	for (int lobe = 0; lobe < 6; lobe++) {
		memcpy(lobes[lobe], color, 3 * sizeof(float));
	}
}

static void hyper_build_transition_lighting(XwaRemasterHyperspace* h, float flash_alpha) {
	float color[3];
	const float scale = h->params.brightness * h->params.highlight_strength *
						h->params.mesh_ambient_strength * flash_alpha;
	for (int channel = 0; channel < 3; channel++) {
		color[channel] = h->params.cap_color[channel] * scale;
	}
	hyper_add_uniform_ambient(&h->lighting.ambient_add, color);
	h->lighting.active = 1;
}

static int hyper_generate_environment(XwaRemasterHyperspace* h, AeronCommandBuffer* cmd) {
	const AeronComputeTextureBinding output = { .texture = h->environment_atlas };
	AeronComputePass* pass = Aeron_BeginComputePass(&(AeronComputePassDesc) {
		.command_buffer = cmd,
		.write_textures = &output,
		.write_texture_count = 1,
		.debug_label = "Hyperspace diffuse environment",
	});
	if (!pass) {
		return 0;
	}
	const HyperEnvironmentUniform environment = {
		.roughness = h->params.mesh_environment_roughness,
	};
	Aeron_BindComputePipeline(pass, h->environment_pipeline);
	Aeron_BindComputeUniformData(pass, 0, &h->tunnel_uniform, sizeof h->tunnel_uniform);
	Aeron_BindComputeUniformData(pass, 1, &environment, sizeof environment);
	Aeron_DispatchCompute(pass, HYPER_ENVIRONMENT_ATLAS_WIDTH / 8, HYPER_ENVIRONMENT_FACE_SIZE / 8, 1);
	Aeron_EndComputePass(pass);

	/* Metal cannot expose one face of a cube as a writable storage view.
	 * Generate into a 2D atlas and copy each completed face into the sampled cube. */
	for (uint32_t face = 0; face < 6; face++) {
		if (!Aeron_CopyTextureCmd(cmd, &(AeronTextureCopyDesc) {
				.source = h->environment_atlas,
				.source_x = face * HYPER_ENVIRONMENT_FACE_SIZE,
				.destination = h->environment_cube,
				.destination_layer = face,
				.width = HYPER_ENVIRONMENT_FACE_SIZE,
				.height = HYPER_ENVIRONMENT_FACE_SIZE,
			})) {
			return 0;
		}
	}
	return 1;
}

static void hyper_build_tunnel_lighting(XwaRemasterHyperspace* h, const float camera_rows[9]) {
	float* const world_basis[3] = {
		h->lighting.environment.right,
		h->lighting.environment.up,
		h->lighting.environment.forward,
	};
	const float* const view_basis[3] = {
		h->tunnel_uniform.tunnel_right,
		h->tunnel_uniform.tunnel_up,
		h->tunnel_uniform.tunnel_forward,
	};
	for (int basis = 0; basis < 3; basis++) {
		hyper_view_to_world(camera_rows, view_basis[basis], world_basis[basis]);
		if (!hyper_normalize(world_basis[basis])) {
			h->lighting.active = 1;
			return;
		}
	}
	h->lighting.environment.texture = h->environment_cube;
	h->lighting.environment.sampler = h->environment_sampler;
	h->lighting.environment.strength = h->params.mesh_ambient_strength;

	float key_direction[3] = { h->tunnel_uniform.tunnel_forward[0], h->tunnel_uniform.tunnel_forward[1],
							   h->tunnel_uniform.tunnel_forward[2] };
	hyper_view_to_world(camera_rows, key_direction, h->lighting.key.world_dir);
	float linear_key[3];
	float intensity = 0.0f;
	const float key_scale = h->params.brightness * h->params.highlight_strength * h->params.mesh_key_strength;
	for (int channel = 0; channel < 3; channel++) {
		linear_key[channel] = h->params.cap_color[channel] * key_scale;
		intensity = fmaxf(intensity, linear_key[channel]);
	}
	if (hyper_normalize(h->lighting.key.world_dir) && isfinite(intensity) && intensity > 0.0f) {
		h->lighting.key.intensity = intensity;
		for (int channel = 0; channel < 3; channel++) {
			h->lighting.key.color[channel] = XwaRemaster_LinearToSrgb(linear_key[channel] / intensity);
		}
		h->lighting.key.source_backdrop_model_type = 0;
		h->lighting.key_count = 1;
	}
	h->lighting.active = 1;
}

int XwaRemasterHyperspace_GetPreparedLighting(const XwaRemasterHyperspace* h,
											  XwaRemasterHyperspaceLighting* out) {
	if (!h || !out || !h->lighting.active) {
		return 0;
	}
	*out = h->lighting;
	return 1;
}

int XwaRemasterHyperspace_Prepare(XwaRemasterHyperspace* h, AeronCommandBuffer* cmd, const XwaSnapshot* snap,
								  const float view_proj[16], const float camera_rows[9], int rt_w, int rt_h, int force_tunnel,
								  float tunnel_time_seconds,
								  const XwaRemasterHyperspaceTunnelView* tunnel_view) {
	if (!h || !cmd || !snap || !view_proj || !camera_rows) {
		return 0;
	}
	h->streak_vertex_count = 0;
	h->draw_background = 0;
	memset(&h->lighting, 0, sizeof h->lighting);
	memset(&h->tunnel_uniform, 0, sizeof h->tunnel_uniform);
	h->tunnel_uniform.view[0] = (float)rt_w;
	h->tunnel_uniform.view[1] = (float)rt_h;
	h->tunnel_uniform.view[2] = 0.5f;
	h->tunnel_uniform.view[3] = hyper_transition_center_y(&snap->flight_camera);
	h->tunnel_uniform.appearance[0] = h->params.brightness;
	h->tunnel_uniform.appearance[1] = h->params.highlight_strength;
	memcpy(h->tunnel_uniform.cap_color, h->params.cap_color, 3 * sizeof(float));
	memcpy(h->view_proj, view_proj, sizeof h->view_proj);
	const uint8_t phase = force_tunnel ? XWA_HYPERSPACE_TUNNEL : snap->hyperspace.phase;
	const uint32_t ticks = force_tunnel ? 0u : snap->hyperspace.phase_elapsed_ticks;
	/* The entry/exit flash must share the tunnel's ship axis and eye rays,
	 * including off-center projection and head-tracked camera movement. */
	if (tunnel_view) {
		h->tunnel_uniform.projection[0] = tunnel_view->tan_half_fov_x;
		h->tunnel_uniform.projection[1] = tunnel_view->tan_half_fov_y;
		h->tunnel_uniform.projection[2] = tunnel_view->proj_offset_x;
		h->tunnel_uniform.projection[3] = tunnel_view->proj_offset_y;
		memcpy(h->tunnel_uniform.tunnel_forward, tunnel_view->forward, 3 * sizeof(float));
	}
	if (phase == XWA_HYPERSPACE_TUNNEL) {
		if (tunnel_view) {
			h->tunnel_uniform.projection[0] = tunnel_view->tan_half_fov_x;
			h->tunnel_uniform.projection[1] = tunnel_view->tan_half_fov_y;
			h->tunnel_uniform.projection[2] = tunnel_view->proj_offset_x;
			h->tunnel_uniform.projection[3] = tunnel_view->proj_offset_y;
			/* XWA's simulation clock has 236 timing ticks per second. */
			h->tunnel_uniform.motion[0] = force_tunnel ? tunnel_time_seconds : (float)ticks / 236.0f;
			h->tunnel_uniform.motion[1] = h->params.travel_speed;
			h->tunnel_uniform.motion[2] = h->params.rotation_speed;
			h->tunnel_uniform.motion[3] = h->params.noise_scale;
			h->tunnel_uniform.geometry[0] = h->params.focal_length;
			h->tunnel_uniform.geometry[1] = h->params.twist;
			h->tunnel_uniform.geometry[2] = h->params.cap_radius;
			h->tunnel_uniform.geometry[3] = h->params.cap_falloff;
			memcpy(h->tunnel_uniform.tunnel_right, tunnel_view->right, 3 * sizeof(float));
			memcpy(h->tunnel_uniform.tunnel_up, tunnel_view->up, 3 * sizeof(float));
			memcpy(h->tunnel_uniform.tunnel_forward, tunnel_view->forward, 3 * sizeof(float));
			memcpy(h->tunnel_uniform.dark_color, h->params.dark_color, 3 * sizeof(float));
			memcpy(h->tunnel_uniform.body_color, h->params.body_color, 3 * sizeof(float));
			memcpy(h->tunnel_uniform.highlight_color, h->params.highlight_color, 3 * sizeof(float));
			if (h->params.mesh_ambient_strength > 0.0f && !hyper_generate_environment(h, cmd)) {
				return 0;
			}
			hyper_build_tunnel_lighting(h, camera_rows);
			h->draw_background = 1;
			return 1;
		}
		return 0;
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
		h->tunnel_uniform.appearance[2] = flash_alpha;
		h->tunnel_uniform.appearance[3] = 1.0f;
		h->draw_background = 1;
	}
	hyper_build_transition_lighting(h, flash_alpha);

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
			hyper_emit_streak(&verts[i * 6u], &snap->hyperspace_streaks[i], extent, transition_y, camera_rows,
							  x_scale);
		}
		if (!Aeron_UploadBufferDataCmd(cmd, h->streak_vb, 0, verts, bytes)) {
			free(verts);
			return 0;
		}
		free(verts);
		h->streak_vertex_count = vertex_count;
	}
	return h->draw_background || h->streak_vertex_count != 0;
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
	if (h->draw_background) {
		Aeron_BindGraphicsPipeline(pass, h->tunnel_pipeline);
		Aeron_BindUniformData(pass, AERON_SHADER_STAGE_FRAGMENT, 0, &h->tunnel_uniform,
							  (uint32_t)sizeof h->tunnel_uniform);
		Aeron_Draw(pass, 3, 0);
	}
	if (h->streak_vertex_count && h->streak_vb) {
		Aeron_BindGraphicsPipeline(pass, h->streak_pipeline);
		Aeron_BindVertexBuffer(pass, 0, h->streak_vb, 0);
		Aeron_BindUniformData(pass, AERON_SHADER_STAGE_VERTEX, 0, h->view_proj, sizeof h->view_proj);
		Aeron_Draw(pass, h->streak_vertex_count, 0);
	}
}
