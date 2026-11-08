/* HD rendering of XWA's three hyperspace phases: the jump's star streaks and
 * scene warp on entry and exit (docs/hd_hyperspace_jump.md) and the analytic
 * tunnel. */

#include "xwa_remaster/hyperspace.h"

#include "aeron/aeron.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/scene_lights.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

/* Mirrors HyperspaceStreakVS in hyperspace_streak.vert.hlsl. */
typedef struct HyperStreakUniform {
	float view[4];
	float projection[4];
	float ship_right[4];
	float ship_up[4];
	float ship_forward[4];
	float motion[4];
	float appearance[4];
	float tint[4];
	float variation[4];
	float taper[4];
	float vanishing[4];
	float glow[4];
	uint32_t config[4];
} HyperStreakUniform;
typedef char HyperStreakUniformSizeCheck[sizeof(HyperStreakUniform) == 208 ? 1 : -1];

/* Mirrors HyperspaceStreakPS in hyperspace_streak.frag.hlsl. */
typedef struct HyperStreakFragmentUniform {
	float glow[4];
	float vanishing[4];
	float tail_color[4];
} HyperStreakFragmentUniform;
typedef char HyperStreakFragmentUniformSizeCheck[sizeof(HyperStreakFragmentUniform) == 48 ? 1 : -1];

/* Mirrors HyperspaceWarpSky in hyperspace_warp_sky.frag.hlsl. */
typedef struct HyperWarpSkyUniform {
	float size[4];
	float vanishing[4];
	float reveal[4];
	float zoom[4];
	float sky[4];
	float projection[4];
	float cube[4];
	float cube_level[4];
	float sky_rows[3][4];
	float overlay_rows[3][4];
} HyperWarpSkyUniform;
typedef char HyperWarpSkyUniformSizeCheck[sizeof(HyperWarpSkyUniform) == 224 ? 1 : -1];

/* Mirrors HyperspaceWarp in hyperspace_warp.frag.hlsl. */
typedef struct HyperWarpUniform {
	float size[4];
	float projection[4];
	float forward[4];
	float vanishing[4];
	float motion[4];
	float reveal[4];
	float config[4];
	float render[4];
	float screens[4];
} HyperWarpUniform;
typedef char HyperWarpUniformSizeCheck[sizeof(HyperWarpUniform) == 144 ? 1 : -1];

/* The sky warp's passes: the sky zoom is split into this many passes. */
#define HYPER_WARP_SKY_PASSES 3
/* In a forward exit with a sky cube, the sky comes from the cube while it is
 * shrunk to half its size or less, then blends to the scene as it grows
 * back to full size. This is the log of that zoom. */
#define HYPER_WARP_CUBE_LOG_ZOOM 0.6931472f

/* How the warp moves the scene. */
typedef enum HyperWarpMotion {
	/* The scene moves toward the camera: the entry, and the exit played
	 * backward. */
	HYPER_WARP_TOWARD,
	/* Forward exit: the scene comes in from beyond its rest position. The
	 * shrunk sky comes from the region's sky cube. */
	HYPER_WARP_ARRIVE,
	/* Forward exit without a sky cube: the scene stays at its rest
	 * position, and only its trails show the movement. */
	HYPER_WARP_TRAILS,
} HyperWarpMotion;

typedef struct HyperEnvironmentUniform {
	float roughness;
	float _pad[3];
} HyperEnvironmentUniform;
typedef char HyperEnvironmentUniformSizeCheck[sizeof(HyperEnvironmentUniform) == 16 ? 1 : -1];

enum {
	HYPER_ENVIRONMENT_FACE_SIZE = 32,
	HYPER_ENVIRONMENT_ATLAS_WIDTH = HYPER_ENVIRONMENT_FACE_SIZE * 6,
};

/* The recovered timeline (FlightObject_UpdatePlayerHyperspaceTransition):
 * 236 ticks per second, an outbound phase of 590 ticks and an inbound phase
 * that ends after 236 ticks. */
#define HYPER_TICKS_PER_SECOND 236.0f
#define HYPER_OUTBOUND_TICKS 590.0f
#define HYPER_INBOUND_TICKS 236.0f
/* The tunnel brightens over its last seconds, ready for the exit. */
#define HYPER_TUNNEL_EXIT_SECONDS 0.25f
/* Near plane of the streaks, in the units of the star distances (1 to 6). */
#define HYPER_STREAK_NEAR 0.02f

struct XwaRemasterHyperspace {
	AeronShader* streak_vs;
	AeronShader* streak_fs;
	AeronShader* tunnel_vs;
	AeronShader* tunnel_fs;
	AeronShader* fullscreen_vs;
	AeronShader* warp_sky_fs;
	AeronShader* warp_fs;
	/* The tunnel in its own scene. */
	AeronGraphicsPipeline* tunnel_pipeline;
	/* Over the world scene: the streaks and the tunnel's fades into the
	 * layer, the sky passes and the composite. */
	AeronGraphicsPipeline* layer_streak_pipeline;
	AeronGraphicsPipeline* layer_tunnel_pipeline;
	AeronGraphicsPipeline* warp_sky_pipeline;
	AeronGraphicsPipeline* warp_pipeline;
	/* The layer at render size; two output-size targets that the sky passes
	 * go back and forth between, and that the composite writes into. */
	AeronRenderTarget* layer_rt;
	AeronRenderTarget* warp_rt[2];
	int layer_w, layer_h;
	int warp_w, warp_h;
	AeronSampler* point_sampler;
	AeronComputePipeline* environment_pipeline;
	AeronTexture* environment_atlas;
	AeronTexture* environment_cube;
	AeronSampler* environment_sampler;
	AeronSampleCount pipeline_samples;
	uint32_t streak_vertex_count;
	uint8_t draw_tunnel;
	/* The warp's progress (0 to 1) this frame, the view it uses, and
	 * whether the scene comes in toward the camera (forward exit). */
	float warp_progress;
	XwaRemasterHyperspaceTunnelView warp_view;
	uint8_t warp_forward;
	uint8_t world_scene;
	uint8_t hold_exposure;
	XwaFlightHyperspaceTunnelParams params;
	XwaFlightHyperspaceJumpParams jump;
	HyperTunnelUniform tunnel_uniform;
	HyperStreakUniform streak_uniform;
	HyperStreakFragmentUniform streak_fragment_uniform;
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

/* The hyperspace draws make their geometry from SV_VertexID, without a
 * depth test. `depth_attachment` matches a pass with the scene's depth. */
static AeronGraphicsPipeline* hyper_create_pipeline(AeronShader* vs, AeronShader* fs, const AeronBlendStateDesc* blend,
													int depth_attachment, AeronSampleCount sample_count) {
	AeronColorTargetStateDesc target = {
		.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
	};
	if (blend) {
		target.blend = *blend;
	}
	return Aeron_CreateGraphicsPipeline(&(AeronGraphicsPipelineDesc) {
		.vertex_shader = vs,
		.fragment_shader = fs,
		.primitive_type = AERON_PRIMITIVE_TRIANGLES,
		.cull_mode = AERON_CULL_NONE,
		.depth_format = depth_attachment ? AERON_TEXTURE_FORMAT_D32_FLOAT : AERON_TEXTURE_FORMAT_UNKNOWN,
		.depth = { .depth_test = 0, .depth_write = 0, .compare = AERON_COMPARE_ALWAYS },
		.color_target_count = 1,
		.color_targets = &target,
		.sample_count = sample_count,
	});
}

static void hyper_destroy_pipeline(AeronGraphicsPipeline** pipeline) {
	if (*pipeline)
		Aeron_DestroyGraphicsPipeline(*pipeline);
	*pipeline = NULL;
}

static int hyper_ensure_pipelines(XwaRemasterHyperspace* h, AeronSampleCount sample_count) {
	if (h->pipeline_samples == sample_count && h->tunnel_pipeline) {
		return 1;
	}
	hyper_destroy_pipeline(&h->tunnel_pipeline);
	h->pipeline_samples = 0;
	const AeronBlendStateDesc pma = hyper_blend_pma();
	h->tunnel_pipeline = hyper_create_pipeline(h->tunnel_vs, h->tunnel_fs, &pma, 1, sample_count);
	if (!h->tunnel_pipeline) {
		return 0;
	}
	h->pipeline_samples = sample_count;
	return 1;
}

static int hyper_ensure_warp_pipelines(XwaRemasterHyperspace* h) {
	if (h->layer_streak_pipeline && h->layer_tunnel_pipeline && h->warp_sky_pipeline && h->warp_pipeline) {
		return 1;
	}
	const AeronBlendStateDesc additive = hyper_blend_additive();
	const AeronBlendStateDesc pma = hyper_blend_pma();
	if (!h->layer_streak_pipeline)
		h->layer_streak_pipeline =
			hyper_create_pipeline(h->streak_vs, h->streak_fs, &additive, 0, AERON_SAMPLE_COUNT_1);
	if (!h->layer_tunnel_pipeline)
		h->layer_tunnel_pipeline =
			hyper_create_pipeline(h->tunnel_vs, h->tunnel_fs, &pma, 0, AERON_SAMPLE_COUNT_1);
	if (!h->warp_sky_pipeline)
		h->warp_sky_pipeline =
			hyper_create_pipeline(h->fullscreen_vs, h->warp_sky_fs, NULL, 0, AERON_SAMPLE_COUNT_1);
	if (!h->warp_pipeline)
		h->warp_pipeline = hyper_create_pipeline(h->fullscreen_vs, h->warp_fs, NULL, 0, AERON_SAMPLE_COUNT_1);
	return h->layer_streak_pipeline && h->layer_tunnel_pipeline && h->warp_sky_pipeline && h->warp_pipeline;
}

static void hyper_release_warp_targets(XwaRemasterHyperspace* h) {
	if (h->layer_rt)
		Aeron_DestroyRenderTarget(h->layer_rt);
	h->layer_rt = NULL;
	h->layer_w = h->layer_h = 0;
	for (int i = 0; i < 2; i++) {
		if (h->warp_rt[i])
			Aeron_DestroyRenderTarget(h->warp_rt[i]);
		h->warp_rt[i] = NULL;
	}
	h->warp_w = h->warp_h = 0;
}

/* The layer at render size and the warp targets at output size, made at the
 * first jump and remade when a size changes. */
static int hyper_ensure_warp_targets(XwaRemasterHyperspace* h, int render_w, int render_h, int output_w,
									 int output_h) {
	if (!h->layer_rt || h->layer_w != render_w || h->layer_h != render_h) {
		if (h->layer_rt)
			Aeron_DestroyRenderTarget(h->layer_rt);
		h->layer_rt = Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) {
			.width = render_w,
			.height = render_h,
			.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
			.debug_name = "xwa.hyperspace.layer",
		});
		h->layer_w = h->layer_rt ? render_w : 0;
		h->layer_h = h->layer_rt ? render_h : 0;
	}
	if (!h->warp_rt[0] || !h->warp_rt[1] || h->warp_w != output_w || h->warp_h != output_h) {
		static const char* const names[2] = { "xwa.hyperspace.warp_a", "xwa.hyperspace.warp_b" };
		int ok = 1;
		for (int i = 0; i < 2; i++) {
			if (h->warp_rt[i])
				Aeron_DestroyRenderTarget(h->warp_rt[i]);
			h->warp_rt[i] = Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) {
				.width = output_w,
				.height = output_h,
				.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
				.debug_name = names[i],
			});
			ok = ok && h->warp_rt[i];
		}
		h->warp_w = ok ? output_w : 0;
		h->warp_h = ok ? output_h : 0;
	}
	return h->layer_rt && h->warp_rt[0] && h->warp_rt[1];
}

XwaRemasterHyperspace* XwaRemasterHyperspace_Create(const XwaFlightHyperspaceTunnelParams* tunnel,
													const XwaFlightHyperspaceJumpParams* jump) {
	if (!tunnel || !jump) {
		return NULL;
	}
	XwaRemasterHyperspace* h = (XwaRemasterHyperspace*)calloc(1, sizeof *h);
	if (!h) {
		return NULL;
	}
	h->streak_vs = Aeron_CreateShader(&(AeronShaderDesc) {
		.name = "hyperspace_streak.vert", .stage = AERON_SHADER_STAGE_VERTEX, .uniform_buffer_count = 1 });
	h->streak_fs = Aeron_CreateShader(&(AeronShaderDesc) {
		.name = "hyperspace_streak.frag", .stage = AERON_SHADER_STAGE_FRAGMENT, .uniform_buffer_count = 1 });
	h->tunnel_vs = Aeron_CreateShader(&(AeronShaderDesc) {
		.name = "hyperspace_tunnel.vert", .stage = AERON_SHADER_STAGE_VERTEX });
	h->tunnel_fs = Aeron_CreateShader(&(AeronShaderDesc) {
		.name = "hyperspace_tunnel.frag", .stage = AERON_SHADER_STAGE_FRAGMENT, .uniform_buffer_count = 1 });
	h->fullscreen_vs =
		Aeron_CreateShader(&(AeronShaderDesc) { .name = "fullscreen.vert", .stage = AERON_SHADER_STAGE_VERTEX });
	h->warp_sky_fs = Aeron_CreateShader(&(AeronShaderDesc) { .name = "hyperspace_warp_sky.frag",
															 .stage = AERON_SHADER_STAGE_FRAGMENT,
															 .sampler_count = 4,
															 .uniform_buffer_count = 1 });
	h->warp_fs = Aeron_CreateShader(&(AeronShaderDesc) { .name = "hyperspace_warp.frag",
														 .stage = AERON_SHADER_STAGE_FRAGMENT,
														 .sampler_count = 5,
														 .uniform_buffer_count = 1 });
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
	h->point_sampler = Aeron_CreateSampler(&(AeronSamplerDesc) {
		.min_filter = AERON_FILTER_NEAREST,
		.mag_filter = AERON_FILTER_NEAREST,
		.mip_filter = AERON_FILTER_NEAREST,
		.address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
		.address_v = AERON_ADDRESS_CLAMP_TO_EDGE,
		.address_w = AERON_ADDRESS_CLAMP_TO_EDGE,
	});
	if (!h->streak_vs || !h->streak_fs || !h->tunnel_vs || !h->tunnel_fs || !h->fullscreen_vs ||
		!h->warp_sky_fs || !h->warp_fs || !h->environment_pipeline || !h->environment_atlas ||
		!h->environment_cube || !h->environment_sampler || !h->point_sampler) {
		Aeron_LogError("xwa.remaster", "hyperspace: GPU resource creation failed");
		XwaRemasterHyperspace_Destroy(h);
		return NULL;
	}
	h->params = *tunnel;
	h->jump = *jump;
	return h;
}

void XwaRemasterHyperspace_SetParams(XwaRemasterHyperspace* h,
									 const XwaFlightHyperspaceTunnelParams* params) {
	if (h && params) {
		h->params = *params;
	}
}

void XwaRemasterHyperspace_SetJumpParams(XwaRemasterHyperspace* h, const XwaFlightHyperspaceJumpParams* params) {
	if (h && params) {
		h->jump = *params;
	}
}

void XwaRemasterHyperspace_Destroy(XwaRemasterHyperspace* h) {
	if (!h) {
		return;
	}
	hyper_destroy_pipeline(&h->tunnel_pipeline);
	hyper_destroy_pipeline(&h->layer_streak_pipeline);
	hyper_destroy_pipeline(&h->layer_tunnel_pipeline);
	hyper_destroy_pipeline(&h->warp_sky_pipeline);
	hyper_destroy_pipeline(&h->warp_pipeline);
	hyper_release_warp_targets(h);
	AeronShader* const shaders[7] = { h->streak_vs,		h->streak_fs,	  h->tunnel_vs, h->tunnel_fs,
									  h->fullscreen_vs, h->warp_sky_fs, h->warp_fs };
	for (int i = 0; i < 7; i++) {
		if (shaders[i])
			Aeron_DestroyShader(shaders[i]);
	}
	if (h->point_sampler)
		Aeron_DestroySampler(h->point_sampler);
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
		for (int channel = 0; channel < 3; channel++) {
			lobes[lobe][channel] += color[channel];
		}
	}
}

/* During the jump's fades in the world scene, the tunnel lights the cockpit
 * evenly in its cap colour. */
static void hyper_build_tunnel_fill_lighting(XwaRemasterHyperspace* h, float opacity, float boost) {
	const float scale = h->params.brightness * h->params.highlight_strength * h->params.mesh_ambient_strength *
						opacity * boost * XwaRemaster_ClassicRadiance();
	if (!(scale > 0.0f) || !isfinite(scale)) {
		return;
	}
	float color[3];
	for (int channel = 0; channel < 3; channel++) {
		color[channel] = h->params.cap_color[channel] * scale;
	}
	hyper_add_uniform_ambient(&h->lighting.ambient_add, color);
	h->lighting.active = 1;
}

/* The streaks light the cockpit evenly in their tint. `level` is the light
 * in units of the classic radiance (hyper_streak_light_level). */
static void hyper_build_streak_lighting(XwaRemasterHyperspace* h, float level) {
	const float scale = level * XwaRemaster_ClassicRadiance();
	if (!(scale > 0.0f) || !isfinite(scale)) {
		return;
	}
	float color[3];
	for (int channel = 0; channel < 3; channel++) {
		color[channel] = h->jump.tint[channel] * scale;
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

/* `scale` is the tunnel's opacity during the jump's fades and 1 in the
 * tunnel. Its extra brightness is already in the tunnel uniform, so the
 * cube includes it; the key gets it from `boost`. */
static void hyper_build_tunnel_lighting(XwaRemasterHyperspace* h, const float camera_rows[9], float scale,
										float boost) {
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
	h->lighting.environment.strength = h->params.mesh_ambient_strength * scale;

	float key_direction[3] = { h->tunnel_uniform.tunnel_forward[0], h->tunnel_uniform.tunnel_forward[1],
							   h->tunnel_uniform.tunnel_forward[2] };
	hyper_view_to_world(camera_rows, key_direction, h->lighting.key.world_dir);
	float peak = 0.0f;
	const float key_scale = h->params.brightness * h->params.highlight_strength *
							h->params.mesh_key_strength * XwaRemaster_ClassicRadiance() * scale * boost;
	for (int channel = 0; channel < 3; channel++) {
		h->lighting.key.color[channel] = h->params.cap_color[channel] * key_scale;
		peak = fmaxf(peak, h->lighting.key.color[channel]);
	}
	if (hyper_normalize(h->lighting.key.world_dir) && isfinite(peak) && peak > 0.0f) {
		h->lighting.key.source_backdrop_object_type = 0;
		h->lighting.key.source_backdrop_index = -1;
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

int XwaRemasterHyperspace_HoldsExposure(const XwaRemasterHyperspace* h) { return h && h->hold_exposure; }

static float hyper_smoothstep(float edge0, float edge1, float x) {
	float t = (x - edge0) / (edge1 - edge0);
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return t * t * (3.0f - 2.0f * t);
}

/* What the moment shows. The streaks follow the entry's curves at
 * `jump_seconds`; the exit runs them backward. */
typedef struct HyperTimeline {
	float tunnel_opacity;
	float tunnel_boost;   /* extra tunnel brightness, 1 = normal */
	float tunnel_seconds; /* the tunnel's clock */
	float streak_fade;    /* 0 = no streaks */
	float jump_seconds;
	uint8_t hold_exposure;
	/* The exit carries on forward (exit_forward): the streaks shrink toward
	 * the camera and the scene comes in toward it. */
	uint8_t forward;
} HyperTimeline;

/* The streaks fade in at the start of the entry and out at the end of the
 * exit, over the normal scene. */
#define HYPER_STREAK_FADE_IN_SECONDS 0.4f
#define HYPER_STREAK_FADE_OUT_SECONDS 0.3f

/* The entry ends with the fade from the white streak field to the tunnel. */
static float hyper_white_peak_seconds(const XwaFlightHyperspaceJumpParams* jump) {
	return HYPER_OUTBOUND_TICKS / HYPER_TICKS_PER_SECOND - jump->white_fade_seconds;
}

static int hyper_timeline(const XwaFlightHyperspaceJumpParams* jump, const XwaRemasterHyperspaceMoment* moment,
						  HyperTimeline* out) {
	memset(out, 0, sizeof *out);
	const float seconds = fmaxf(moment->phase_ticks, 0.0f) / HYPER_TICKS_PER_SECOND;
	const float fade = jump->white_fade_seconds;
	const float entry_seconds = HYPER_OUTBOUND_TICKS / HYPER_TICKS_PER_SECOND;
	const float exit_seconds = HYPER_INBOUND_TICKS / HYPER_TICKS_PER_SECOND;
	const float tunnel_seconds = moment->tunnel_ticks / HYPER_TICKS_PER_SECOND;
	const float entry_boost = jump->tunnel_entry_brightness;
	switch (moment->phase) {
		case XWA_HYPERSPACE_OUTBOUND:
			out->tunnel_opacity = hyper_smoothstep(entry_seconds - fade, entry_seconds, seconds);
			out->tunnel_boost = entry_boost;
			/* The tunnel's clock runs from before the phase change, so its
			 * motion carries on through it. */
			out->tunnel_seconds = seconds - entry_seconds;
			out->streak_fade = (1.0f - out->tunnel_opacity) *
							   hyper_smoothstep(0.0f, HYPER_STREAK_FADE_IN_SECONDS, seconds);
			out->jump_seconds = seconds;
			out->hold_exposure = 1;
			return 1;
		case XWA_HYPERSPACE_TUNNEL: {
			out->tunnel_opacity = 1.0f;
			out->tunnel_boost = 1.0f;
			out->tunnel_seconds = seconds;
			if (moment->tunnel_only) {
				return 1;
			}
			const float settle = jump->tunnel_settle_seconds;
			float boost = 1.0f + (entry_boost - 1.0f) * (1.0f - hyper_smoothstep(0.0f, settle, seconds));
			const float remaining = tunnel_seconds - seconds;
			const float exit_boost =
				1.0f + (entry_boost - 1.0f) * (1.0f - hyper_smoothstep(0.0f, HYPER_TUNNEL_EXIT_SECONDS, remaining));
			out->tunnel_boost = fmaxf(boost, exit_boost);
			out->hold_exposure =
				seconds < settle + 0.1f || remaining < HYPER_TUNNEL_EXIT_SECONDS + 0.05f;
			return 1;
		}
		case XWA_HYPERSPACE_INBOUND: {
			out->tunnel_opacity = 1.0f - hyper_smoothstep(0.0f, fade, seconds);
			out->tunnel_boost = entry_boost;
			out->tunnel_seconds = tunnel_seconds + seconds;
			out->streak_fade =
				(1.0f - out->tunnel_opacity) *
				(1.0f - hyper_smoothstep(exit_seconds - HYPER_STREAK_FADE_OUT_SECONDS, exit_seconds, seconds));
			/* Hold the white-out during the fade, then run the entry
			 * backward: a fast snap back, then a slow settle into points. */
			float u = (seconds - fade) / (exit_seconds - fade);
			u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
			out->jump_seconds = hyper_white_peak_seconds(jump) * (1.0f - u);
			out->hold_exposure = 1;
			out->forward = jump->exit_forward ? 1 : 0;
			return 1;
		}
		default:
			return 0;
	}
}

/* The entry's curves at `seconds` into it. */
typedef struct HyperJumpCurves {
	float head_travel;
	float tail_share; /* tail travel as a share of the head's */
	float visible_share;
	float whiteout; /* 0 to 1 */
	float tint;
	float width;
} HyperJumpCurves;

static void hyper_jump_curves(const XwaFlightHyperspaceJumpParams* jump, float seconds, HyperJumpCurves* out) {
	const float peak = hyper_white_peak_seconds(jump);
	/* Exponential travel: a gentle start and a violent end. */
	out->head_travel = jump->stretch_scale * 0.025f * (expf(1.95f * seconds) - 1.0f);
	/* The tails stay on the stars, then leave them near the peak. */
	out->tail_share = 0.35f * hyper_smoothstep(1.6f, peak, seconds);
	out->visible_share = 0.25f + 0.75f * hyper_smoothstep(0.8f, 2.0f, seconds);
	const float whiteout = hyper_smoothstep(1.3f, peak, seconds);
	out->whiteout = whiteout * whiteout;
	out->tint = hyper_smoothstep(1.0f, peak, seconds);
	out->width = 1.0f + out->whiteout;
}

static void hyper_set_tunnel_uniform(XwaRemasterHyperspace* h, const XwaRemasterHyperspaceTunnelView* view,
									 const HyperTimeline* timeline, int rt_w, int rt_h) {
	HyperTunnelUniform* u = &h->tunnel_uniform;
	memset(u, 0, sizeof *u);
	u->view[0] = (float)rt_w;
	u->view[1] = (float)rt_h;
	/* The tunnel's colours are classic image values, shown at the classic
	 * radiance. Its cockpit lighting follows from that radiance. */
	u->appearance[0] = h->params.brightness * XwaRemaster_ClassicRadiance() * timeline->tunnel_boost;
	u->appearance[1] = h->params.highlight_strength;
	u->appearance[2] = timeline->tunnel_opacity;
	u->projection[0] = view->tan_half_fov_x;
	u->projection[1] = view->tan_half_fov_y;
	u->projection[2] = view->proj_offset_x;
	u->projection[3] = view->proj_offset_y;
	u->motion[0] = timeline->tunnel_seconds;
	u->motion[1] = h->params.travel_speed;
	u->motion[2] = h->params.rotation_speed;
	u->motion[3] = h->params.noise_scale;
	u->geometry[0] = h->params.focal_length;
	u->geometry[1] = h->params.twist;
	u->geometry[2] = h->params.cap_radius;
	u->geometry[3] = h->params.cap_falloff;
	memcpy(u->tunnel_right, view->right, 3 * sizeof(float));
	memcpy(u->tunnel_up, view->up, 3 * sizeof(float));
	memcpy(u->tunnel_forward, view->forward, 3 * sizeof(float));
	memcpy(u->dark_color, h->params.dark_color, 3 * sizeof(float));
	memcpy(u->body_color, h->params.body_color, 3 * sizeof(float));
	memcpy(u->highlight_color, h->params.highlight_color, 3 * sizeof(float));
	memcpy(u->cap_color, h->params.cap_color, 3 * sizeof(float));
}

/* The point the streaks run from, in px: where the ship's forward axis, or
 * its backward axis in a rear view, meets the screen. In a side view the
 * streaks are nearly parallel and have no vanishing point on screen, so the
 * radial widening fades out. Writes xy the point, z the radial widening, w
 * 1 / screen half-height. */
static void hyper_vanishing_point(const XwaRemasterHyperspaceTunnelView* view, float radial_widening, int rt_w,
								  int rt_h, float out[4]) {
	const float forward_z = view->forward[2];
	const float sign = forward_z >= 0.0f ? 1.0f : -1.0f;
	const float z = fabsf(forward_z);
	float weight = (z - 0.05f) / 0.25f;
	weight = weight < 0.0f ? 0.0f : (weight > 1.0f ? 1.0f : weight);
	out[0] = 0.5f * (float)rt_w;
	out[1] = 0.5f * (float)rt_h;
	if (z > 1.0e-3f) {
		const float ndc_x = sign * view->forward[0] / (z * view->tan_half_fov_x) + view->proj_offset_x;
		const float ndc_y = view->proj_offset_y - sign * view->forward[1] / (z * view->tan_half_fov_y);
		out[0] = (ndc_x * 0.5f + 0.5f) * (float)rt_w;
		out[1] = (0.5f - ndc_y * 0.5f) * (float)rt_h;
	}
	out[2] = radial_widening * weight;
	out[3] = rt_h > 0 ? 2.0f / (float)rt_h : 0.0f;
}

/* The streaks' light on the cockpit, on the entry's timeline: it rises from
 * `mesh_light_start` to `mesh_light_strength` at the white peak, as a power
 * `mesh_light_curve` of the time elapsed, and follows the streaks' fades. */
static float hyper_streak_light_level(const XwaFlightHyperspaceJumpParams* jump, float jump_seconds, float fade) {
	const float peak = hyper_white_peak_seconds(jump);
	const float start = fminf(jump->mesh_light_start, peak - 0.05f);
	float ramp = (jump_seconds - start) / (peak - start);
	ramp = ramp < 0.0f ? 0.0f : (ramp > 1.0f ? 1.0f : ramp);
	return jump->mesh_light_strength * powf(ramp, jump->mesh_light_curve) * fade;
}

/* The streaks' intensity: the brightness of a star of brightness 1. */
static float hyper_streak_intensity(const XwaFlightHyperspaceJumpParams* jump, const HyperJumpCurves* curves,
									float fade) {
	return jump->streak_brightness * (1.0f + (jump->whiteout_brightness - 1.0f) * curves->whiteout) *
		   XwaRemaster_ClassicRadiance() * fade;
}

static void hyper_set_streak_uniform(const XwaRemasterHyperspace* h, HyperStreakUniform* u,
									 const XwaRemasterHyperspaceTunnelView* view, const HyperJumpCurves* curves,
									 int forward, float intensity, uint32_t seed, int rt_w, int rt_h) {
	const XwaFlightHyperspaceJumpParams* jump = &h->jump;
	memset(u, 0, sizeof *u);
	u->view[0] = (float)rt_w;
	u->view[1] = (float)rt_h;
	u->view[2] = (float)rt_h / 1080.0f;
	u->view[3] = HYPER_STREAK_NEAR;
	u->projection[0] = view->tan_half_fov_x;
	u->projection[1] = view->tan_half_fov_y;
	u->projection[2] = view->proj_offset_x;
	u->projection[3] = view->proj_offset_y;
	/* The stars are placed in the ship's frame (right, up, forward). */
	memcpy(u->ship_right, view->right, 3 * sizeof(float));
	memcpy(u->ship_up, view->up, 3 * sizeof(float));
	memcpy(u->ship_forward, view->forward, 3 * sizeof(float));
	/* In a forward exit the stars move ahead by the head's travel, so the
	 * heads stay on the stars and the tails come in to them. */
	u->ship_forward[3] = forward ? curves->head_travel : 0.0f;
	u->motion[0] = curves->head_travel;
	u->motion[1] = curves->tail_share;
	u->motion[2] = curves->visible_share;
	u->motion[3] = curves->width;
	u->appearance[0] = intensity;
	u->appearance[1] = curves->tint;
	u->appearance[2] = jump->streak_width_px;
	u->appearance[3] = jump->head_widening;
	memcpy(u->tint, jump->tint, 3 * sizeof(float));
	u->variation[0] = jump->brightness_range;
	u->variation[1] = jump->width_from_brightness;
	u->variation[2] = jump->width_variation;
	u->variation[3] = jump->taper_variation;
	u->taper[0] = jump->tail_taper;
	u->taper[1] = jump->head_taper;
	hyper_vanishing_point(view, jump->radial_widening, rt_w, rt_h, u->vanishing);
	u->glow[0] = jump->glow_strength;
	u->glow[1] = jump->glow_width;
	u->config[0] = (uint32_t)jump->star_count;
	u->config[1] = seed;
}

/* The warp's progress on the entry's timeline: 0 before `warp_start`, 1 from
 * `warp_end`, the power `warp_curve` of the share of the time elapsed in
 * between. */
static float hyper_warp_progress(const XwaFlightHyperspaceJumpParams* jump, float jump_seconds) {
	const float span = jump->warp_end - jump->warp_start;
	if (!(span > 0.0f)) {
		return jump_seconds >= jump->warp_end ? 1.0f : 0.0f;
	}
	float ramp = (jump_seconds - jump->warp_start) / span;
	ramp = ramp < 0.0f ? 0.0f : (ramp > 1.0f ? 1.0f : ramp);
	return powf(ramp, jump->warp_curve);
}

int XwaRemasterHyperspace_Prepare(XwaRemasterHyperspace* h, AeronCommandBuffer* cmd, const float camera_rows[9],
								  int rt_w, int rt_h, const XwaRemasterHyperspaceMoment* moment,
								  const XwaRemasterHyperspaceTunnelView* tunnel_view, int world_scene) {
	if (!h || !cmd || !camera_rows || !moment || !tunnel_view) {
		return 0;
	}
	h->streak_vertex_count = 0;
	h->draw_tunnel = 0;
	h->world_scene = world_scene ? 1 : 0;
	h->hold_exposure = 0;
	h->warp_progress = 0.0f;
	h->warp_view = *tunnel_view;
	h->warp_forward = 0;
	memset(&h->lighting, 0, sizeof h->lighting);
	HyperTimeline timeline;
	if (!hyper_timeline(&h->jump, moment, &timeline)) {
		return 0;
	}
	h->hold_exposure = timeline.hold_exposure;
	hyper_set_tunnel_uniform(h, tunnel_view, &timeline, rt_w, rt_h);
	if (timeline.tunnel_opacity > 0.0f) {
		if (world_scene) {
			/* The fades are short and under the white-out: an even fill is
			 * enough for the cockpit. */
			hyper_build_tunnel_fill_lighting(h, timeline.tunnel_opacity, timeline.tunnel_boost);
		} else {
			if (h->params.mesh_ambient_strength > 0.0f && !hyper_generate_environment(h, cmd)) {
				return 0;
			}
			hyper_build_tunnel_lighting(h, camera_rows, timeline.tunnel_opacity, timeline.tunnel_boost);
		}
		h->draw_tunnel = 1;
	}
	if (world_scene) {
		h->warp_progress = hyper_warp_progress(&h->jump, timeline.jump_seconds);
		h->warp_forward = timeline.forward;
	}
	/* The streaks only show over the world scene, during the entry and the
	 * exit. */
	if (world_scene && timeline.streak_fade > 0.0f && h->jump.star_count > 0) {
		HyperJumpCurves curves;
		hyper_jump_curves(&h->jump, timeline.jump_seconds, &curves);
		hyper_set_streak_uniform(h, &h->streak_uniform, tunnel_view, &curves, timeline.forward,
								 hyper_streak_intensity(&h->jump, &curves, timeline.streak_fade), moment->seed,
								 rt_w, rt_h);
		HyperStreakFragmentUniform* fu = &h->streak_fragment_uniform;
		memset(fu, 0, sizeof *fu);
		fu->glow[0] = h->jump.glow_strength;
		fu->glow[1] = h->jump.glow_width;
		fu->glow[2] = h->jump.tail_brightness;
		fu->glow[3] = h->jump.head_widening;
		hyper_vanishing_point(tunnel_view, h->jump.radial_widening, rt_w, rt_h, fu->vanishing);
		memcpy(fu->tail_color, h->jump.tail_color, 3 * sizeof(float));
		hyper_build_streak_lighting(
			h, hyper_streak_light_level(&h->jump, timeline.jump_seconds, timeline.streak_fade));
		h->streak_vertex_count = (uint32_t)h->jump.star_count * 6u;
	}
	/* The cockpit's ambient light is set even when only the streaks draw. */
	h->lighting.active = 1;
	return h->draw_tunnel || h->streak_vertex_count != 0 || h->warp_progress > 0.0f;
}

static void hyper_draw_tunnel(XwaRemasterHyperspace* h, AeronRenderPass* pass, AeronGraphicsPipeline* pipeline) {
	Aeron_BindGraphicsPipeline(pass, pipeline);
	Aeron_BindUniformData(pass, AERON_SHADER_STAGE_FRAGMENT, 0, &h->tunnel_uniform,
						  (uint32_t)sizeof h->tunnel_uniform);
	Aeron_Draw(pass, 3, 0);
}

static void hyper_draw_streaks(XwaRemasterHyperspace* h, AeronRenderPass* pass) {
	Aeron_BindGraphicsPipeline(pass, h->layer_streak_pipeline);
	Aeron_BindUniformData(pass, AERON_SHADER_STAGE_VERTEX, 0, &h->streak_uniform,
						  (uint32_t)sizeof h->streak_uniform);
	Aeron_BindUniformData(pass, AERON_SHADER_STAGE_FRAGMENT, 0, &h->streak_fragment_uniform,
						  (uint32_t)sizeof h->streak_fragment_uniform);
	Aeron_Draw(pass, h->streak_vertex_count, 0);
}

void XwaRemasterHyperspace_Draw(AeronCommandBuffer* command_buffer, AeronRenderPass* pass, int rt_w, int rt_h,
								void* user) {
	XwaRemasterHyperspace* h = (XwaRemasterHyperspace*)user;
	if (!h || !pass || h->world_scene || !h->draw_tunnel) {
		return;
	}
	if (!hyper_ensure_pipelines(h, Aeron_RenderPassGetSampleCount(pass))) {
		Aeron_CommandBufferSetFailure(command_buffer, "Hyperspace pipeline preparation failed");
		return;
	}
	Aeron_SetViewport(pass, &(AeronRectI) { 0, 0, rt_w, rt_h });
	hyper_draw_tunnel(h, pass, h->tunnel_pipeline);
}

/* The streaks and the tunnel's fades, at render size, with premultiplied
 * alpha. */
static int hyper_draw_layer(XwaRemasterHyperspace* h, AeronCommandBuffer* cmd) {
	AeronRenderPass* pass = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
		.color_target = h->layer_rt,
		.clear_color = 1,
		.clear_color_rgba = { 0.0f, 0.0f, 0.0f, 0.0f },
		.command_buffer = cmd,
		.debug_label = "Hyperspace streak layer",
	});
	if (!pass) {
		return 0;
	}
	Aeron_SetViewport(pass, &(AeronRectI) { 0, 0, h->layer_w, h->layer_h });
	if (h->draw_tunnel) {
		hyper_draw_tunnel(h, pass, h->layer_tunnel_pipeline);
	}
	if (h->streak_vertex_count) {
		hyper_draw_streaks(h, pass);
	}
	Aeron_EndRenderPass(pass);
	return 1;
}

static int hyper_fullscreen_pass(AeronCommandBuffer* cmd, AeronGraphicsPipeline* pipeline,
								 AeronRenderTarget* target, int width, int height, AeronTexture* const* textures,
								 AeronSampler* const* samplers, uint32_t texture_count, const void* uniform,
								 uint32_t uniform_size, const char* label) {
	AeronRenderPass* pass = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
		.color_target = target,
		.discard_color = 1,
		.command_buffer = cmd,
		.debug_label = label,
	});
	if (!pass) {
		return 0;
	}
	Aeron_SetViewport(pass, &(AeronRectI) { 0, 0, width, height });
	Aeron_BindGraphicsPipeline(pass, pipeline);
	for (uint32_t i = 0; i < texture_count; i++) {
		Aeron_BindTextureSampler(pass, AERON_SHADER_STAGE_FRAGMENT, i, textures[i], samplers[i]);
	}
	Aeron_BindUniformData(pass, AERON_SHADER_STAGE_FRAGMENT, 0, uniform, uniform_size);
	Aeron_Draw(pass, 3, 0);
	Aeron_EndRenderPass(pass);
	return 1;
}

/* The vanishing point at output size (xy), how much the view faces the
 * direction of travel (z), and 1 / the distance from the vanishing point to
 * the farthest screen corner (w). */
static void hyper_warp_vanishing(const XwaRemasterHyperspaceTunnelView* view, int width, int height,
								 float out[4]) {
	hyper_vanishing_point(view, 1.0f, width, height, out);
	/* The ramp of the streaks' radial widening, on the signed forward
	 * component: rear views don't face the direction of travel. */
	float facing = (view->forward[2] - 0.05f) / 0.25f;
	facing = facing < 0.0f ? 0.0f : (facing > 1.0f ? 1.0f : facing);
	float farthest = 1.0f;
	const float corners[4][2] = {
		{ 0.0f, 0.0f }, { (float)width, 0.0f }, { 0.0f, (float)height }, { (float)width, (float)height }
	};
	for (int i = 0; i < 4; i++) {
		const float dx = corners[i][0] - out[0];
		const float dy = corners[i][1] - out[1];
		farthest = fmaxf(farthest, sqrtf(dx * dx + dy * dy));
	}
	out[2] = facing;
	out[3] = 1.0f / farthest;
}

static void hyper_size_uniform(float out[4], int width, int height) {
	out[0] = (float)width;
	out[1] = (float)height;
	out[2] = 1.0f / (float)width;
	out[3] = 1.0f / (float)height;
}

/* Rows of the matrix from view space to a cube, with a zero w. */
static void hyper_cube_rows(const float view_to_cube[9], float rows[3][4]) {
	for (int r = 0; r < 3; r++) {
		memcpy(rows[r], &view_to_cube[r * 3], 3 * sizeof(float));
		rows[r][3] = 0.0f;
	}
}

/* Sets the cube settings of the sky passes. Only the first pass reads the
 * cubes, and only in HYPER_WARP_ARRIVE. */
static void hyper_set_sky_cube_uniform(HyperWarpSkyUniform* u, const XwaRemasterHyperspaceTunnelView* view,
									   const XwaRemasterHyperspaceWarpInput* in, HyperWarpMotion motion,
									   float log_zoom) {
	u->projection[0] = view->tan_half_fov_x;
	u->projection[1] = view->tan_half_fov_y;
	u->projection[2] = view->proj_offset_x;
	u->projection[3] = view->proj_offset_y;
	/* The scene's share: all of it, unless the sky is shrunk. */
	u->cube[3] = 1.0f;
	if (motion != HYPER_WARP_ARRIVE) {
		return;
	}
	float share = -log_zoom / HYPER_WARP_CUBE_LOG_ZOOM;
	share = share < 0.0f ? 0.0f : (share > 1.0f ? 1.0f : share);
	u->cube[3] = 1.0f - share * share * (3.0f - 2.0f * share);
	u->cube[0] = in->sky_scale;
	/* Cube texels per pixel at zoom 1: a pixel spans 2 tan(half-FOV) /
	 * height in tangent units, and a texel 2 / face size. */
	const float texels_per_px = view->tan_half_fov_y / (float)in->output_h;
	u->cube_level[0] = (float)Aeron_TextureGetWidth(in->sky_cube) * texels_per_px;
	u->cube_level[1] = (float)in->sky_max_level;
	hyper_cube_rows(in->sky_view_to_cube, u->sky_rows);
	if (in->overlay_cube) {
		u->cube[1] = in->overlay_scale;
		u->cube_level[2] = (float)Aeron_TextureGetWidth(in->overlay_cube) * texels_per_px;
		u->cube_level[3] = (float)in->overlay_max_level;
		hyper_cube_rows(in->overlay_view_to_cube, u->overlay_rows);
	}
}

/* The sky passes: the zoom's log span, from the smear's old end to the
 * current zoom, split into taps^3 evenly spaced samples. Returns the target
 * holding the warped sky, or NULL on failure. */
static AeronRenderTarget* hyper_warp_sky(XwaRemasterHyperspace* h, AeronCommandBuffer* cmd,
										 const XwaRemasterHyperspaceWarpInput* in, HyperWarpMotion motion,
										 const float vanishing[4], const float reveal[4]) {
	const XwaFlightHyperspaceJumpParams* jump = &h->jump;
	const int taps = jump->warp_sky_taps;
	/* The zoom fades out in views that don't face the direction of travel.
	 * In a forward exit the sky starts shrunk and grows back to its size;
	 * with trails only it keeps its size, and the smear reaches back to
	 * where it would have been. */
	const float zoom_change = h->warp_progress * vanishing[2] * logf(jump->warp_sky_zoom);
	const float log_zoom =
		motion == HYPER_WARP_TOWARD ? zoom_change : (motion == HYPER_WARP_ARRIVE ? -zoom_change : 0.0f);
	const float span = jump->warp_smear * zoom_change;
	float step = span;
	for (int pass = 0; pass < HYPER_WARP_SKY_PASSES; pass++) {
		step /= (float)taps;
	}
	const float radiance = XwaRemaster_ClassicRadiance();
	AeronRenderTarget* const targets[HYPER_WARP_SKY_PASSES] = { h->warp_rt[0], h->warp_rt[1], h->warp_rt[0] };
	static const char* const labels[HYPER_WARP_SKY_PASSES] = { "Hyperspace sky warp 1", "Hyperspace sky warp 2",
																"Hyperspace sky warp 3" };
	AeronTexture* source = in->scene;
	for (int pass = 0; pass < HYPER_WARP_SKY_PASSES; pass++) {
		HyperWarpSkyUniform u;
		memset(&u, 0, sizeof u);
		hyper_size_uniform(u.size, in->output_w, in->output_h);
		memcpy(u.vanishing, vanishing, sizeof u.vanishing);
		u.reveal[0] = reveal[0];
		u.reveal[1] = reveal[1];
		/* The first pass starts at the smear's old end, with its samples
		 * centred in their spacing. Each later pass spaces its samples by
		 * the span of all the passes before it. */
		u.zoom[0] = pass == 0 ? log_zoom - span + 0.5f * step : 0.0f;
		u.zoom[1] = step;
		u.zoom[2] = (float)taps;
		u.zoom[3] = pass == 0 ? 0.0f : (pass == HYPER_WARP_SKY_PASSES - 1 ? 2.0f : 1.0f);
		u.sky[0] = jump->warp_sky_highlights;
		u.sky[1] = radiance > 0.0f ? 1.0f / radiance : 1.0f;
		hyper_set_sky_cube_uniform(&u, &h->warp_view, in, motion, log_zoom);
		/* Without a cube, the tunnel's cube fills the bindings; it isn't
		 * read. */
		const int cubes = motion == HYPER_WARP_ARRIVE;
		AeronTexture* const textures[4] = {
			source, in->depth, cubes ? in->sky_cube : h->environment_cube,
			cubes && in->overlay_cube ? in->overlay_cube : h->environment_cube
		};
		AeronSampler* const samplers[4] = {
			h->environment_sampler, h->point_sampler, cubes ? in->sky_sampler : h->environment_sampler,
			cubes && in->overlay_cube ? in->overlay_sampler : h->environment_sampler
		};
		if (!source || !hyper_fullscreen_pass(cmd, h->warp_sky_pipeline, targets[pass], in->output_w,
											  in->output_h, textures, samplers, 4, &u, (uint32_t)sizeof u,
											  labels[pass])) {
			return NULL;
		}
		source = Aeron_RenderTargetGetTexture(targets[pass]);
		step *= (float)taps;
	}
	return targets[HYPER_WARP_SKY_PASSES - 1];
}

AeronTexture* XwaRemasterHyperspace_RenderWarp(XwaRemasterHyperspace* h, AeronCommandBuffer* cmd,
											   const XwaRemasterHyperspaceWarpInput* in) {
	if (!h || !cmd || !in || !in->scene || !in->depth || in->output_w <= 0 || in->output_h <= 0 ||
		in->render_w <= 0 || in->render_h <= 0) {
		return NULL;
	}
	if (!hyper_ensure_warp_pipelines(h) ||
		!hyper_ensure_warp_targets(h, in->render_w, in->render_h, in->output_w, in->output_h)) {
		Aeron_CommandBufferSetFailure(cmd, "Hyperspace warp preparation failed");
		return NULL;
	}
	if (!hyper_draw_layer(h, cmd)) {
		Aeron_CommandBufferSetFailure(cmd, "Hyperspace streak layer recording failed");
		return NULL;
	}
	const XwaFlightHyperspaceJumpParams* jump = &h->jump;
	const XwaRemasterHyperspaceTunnelView* view = &h->warp_view;
	const float progress = h->warp_progress;
	const int warp = progress > 0.0f;
	float vanishing[4];
	hyper_warp_vanishing(view, in->output_w, in->output_h, vanishing);
	const float reveal[4] = { progress, jump->warp_reveal_softness, warp ? 1.0f : 0.0f,
							  jump->warp_sky_highlights };
	const HyperWarpMotion motion =
		!h->warp_forward ? HYPER_WARP_TOWARD : (in->sky_cube ? HYPER_WARP_ARRIVE : HYPER_WARP_TRAILS);

	/* Before the warp starts and once the reveal is complete, the composite
	 * doesn't read the sky; it is bound anyway. */
	AeronRenderTarget* sky = h->warp_rt[0];
	if (warp && progress < 1.0f) {
		sky = hyper_warp_sky(h, cmd, in, motion, vanishing, reveal);
		if (!sky) {
			Aeron_CommandBufferSetFailure(cmd, "Hyperspace sky warp recording failed");
			return NULL;
		}
	}
	AeronRenderTarget* out = sky == h->warp_rt[0] ? h->warp_rt[1] : h->warp_rt[0];

	HyperWarpUniform u;
	memset(&u, 0, sizeof u);
	hyper_size_uniform(u.size, in->output_w, in->output_h);
	u.projection[0] = view->tan_half_fov_x;
	u.projection[1] = view->tan_half_fov_y;
	u.projection[2] = view->proj_offset_x;
	u.projection[3] = view->proj_offset_y;
	memcpy(u.forward, view->forward, 3 * sizeof(float));
	memcpy(u.vanishing, vanishing, sizeof u.vanishing);
	/* In a forward exit the world comes in toward the camera from beyond its
	 * rest position, so its distance is negative. With trails only it stays
	 * at its rest position, and the smear reaches back to where it would
	 * have been. */
	const float distance = progress * jump->warp_distance_m * in->units_per_meter;
	const float moved = motion == HYPER_WARP_TOWARD ? distance : (motion == HYPER_WARP_ARRIVE ? -distance : 0.0f);
	const float side = motion == HYPER_WARP_TOWARD ? 1.0f : -1.0f;
	u.motion[0] = moved;
	u.motion[1] = moved - jump->warp_smear * distance;
	u.motion[2] = in->near_z;
	u.motion[3] = in->keep_depth;
	memcpy(u.reveal, reveal, sizeof u.reveal);
	u.config[0] = (float)jump->warp_taps;
	u.config[1] = XwaRemaster_ClassicRadiance();
	u.config[2] = (float)in->view;
	u.config[3] = side;
	hyper_size_uniform(u.render, in->render_w, in->render_h);
	u.screens[0] = in->screens ? 1.0f : 0.0f;
	/* Without the screen layer, the scene fills its binding; it isn't
	 * read. */
	AeronTexture* const textures[5] = { in->scene, in->depth, Aeron_RenderTargetGetTexture(sky),
										Aeron_RenderTargetGetTexture(h->layer_rt),
										in->screens ? in->screens : in->scene };
	AeronSampler* const samplers[5] = { h->environment_sampler, h->point_sampler, h->environment_sampler,
										h->environment_sampler, h->environment_sampler };
	if (!textures[2] || !textures[3] ||
		!hyper_fullscreen_pass(cmd, h->warp_pipeline, out, in->output_w, in->output_h, textures, samplers, 5,
							   &u, (uint32_t)sizeof u, "Hyperspace warp composite")) {
		Aeron_CommandBufferSetFailure(cmd, "Hyperspace warp composite recording failed");
		return NULL;
	}
	return Aeron_RenderTargetGetTexture(out);
}
