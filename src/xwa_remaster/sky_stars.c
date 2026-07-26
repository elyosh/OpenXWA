/* XWA's GPU reproduction of FlightStarfield_Render. */

#include "xwa_remaster/sky_stars.h"

#include "aeron/aeron.h"

#include <stdlib.h>
#include <string.h>

typedef struct SkyStarsVsUniform {
	float cube_to_clip[16];
	float view[4];
	float geom[4];
	float tone[4];
} SkyStarsVsUniform;

struct XwaRemasterSkyStars {
	AeronShader* vertex_shader;
	AeronShader* fragment_shader;
	AeronGraphicsPipeline* pipeline;
	AeronSampleCount pipeline_samples;
	SkyStarsVsUniform vertex_uniform;
	float fragment_uniform[4];
	uint32_t vertex_count;
};

static void sky_stars_mat4_mul(float out[16], const float lhs[16], const float rhs[16]) {
	float result[16];
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			result[r * 4 + c] = lhs[r * 4 + 0] * rhs[0 * 4 + c] + lhs[r * 4 + 1] * rhs[1 * 4 + c] +
								lhs[r * 4 + 2] * rhs[2 * 4 + c] + lhs[r * 4 + 3] * rhs[3 * 4 + c];
		}
	}
	memcpy(out, result, sizeof result);
}

static int sky_stars_ensure_pipeline(XwaRemasterSkyStars* stars, AeronSampleCount sample_count) {
	if (stars->pipeline && stars->pipeline_samples == sample_count) {
		return 1;
	}
	if (stars->pipeline) {
		Aeron_DestroyGraphicsPipeline(stars->pipeline);
		stars->pipeline = NULL;
	}
	AeronColorTargetStateDesc color_target = {
		.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
		.blend = {
			.enabled = 1,
			.src_color = AERON_BLEND_ONE,
			.dst_color = AERON_BLEND_ONE,
			.color_op = AERON_BLEND_OP_ADD,
			.src_alpha = AERON_BLEND_ONE,
			.dst_alpha = AERON_BLEND_ONE,
			.alpha_op = AERON_BLEND_OP_ADD,
		},
	};
	stars->pipeline = Aeron_CreateGraphicsPipeline(&(AeronGraphicsPipelineDesc) {
		.vertex_shader = stars->vertex_shader,
		.fragment_shader = stars->fragment_shader,
		.primitive_type = AERON_PRIMITIVE_TRIANGLES,
		.cull_mode = AERON_CULL_NONE,
		.depth_format = AERON_TEXTURE_FORMAT_D32_FLOAT,
		.depth = { .depth_test = 1, .depth_write = 0, .compare = AERON_COMPARE_GREATER_EQUAL },
		.color_target_count = 1,
		.color_targets = &color_target,
		.sample_count = sample_count,
	});
	stars->pipeline_samples = stars->pipeline ? sample_count : 0;
	return stars->pipeline != NULL;
}

XwaRemasterSkyStars* XwaRemasterSkyStars_Create(void) {
	XwaRemasterSkyStars* stars = (XwaRemasterSkyStars*)calloc(1, sizeof *stars);
	if (!stars) {
		return NULL;
	}
	stars->vertex_shader = Aeron_CreateShader(&(AeronShaderDesc) {
		.name = "sky_stars.vert",
		.stage = AERON_SHADER_STAGE_VERTEX,
		.uniform_buffer_count = 1,
	});
	stars->fragment_shader = Aeron_CreateShader(&(AeronShaderDesc) {
		.name = "sky_stars.frag",
		.stage = AERON_SHADER_STAGE_FRAGMENT,
		.uniform_buffer_count = 1,
	});
	if (!stars->vertex_shader || !stars->fragment_shader) {
		Aeron_Log("xwa.remaster", "starfield: GPU resource creation failed");
		XwaRemasterSkyStars_Destroy(stars);
		return NULL;
	}
	return stars;
}

void XwaRemasterSkyStars_Destroy(XwaRemasterSkyStars* stars) {
	if (!stars) {
		return;
	}
	if (stars->pipeline) {
		Aeron_DestroyGraphicsPipeline(stars->pipeline);
	}
	if (stars->vertex_shader) {
		Aeron_DestroyShader(stars->vertex_shader);
	}
	if (stars->fragment_shader) {
		Aeron_DestroyShader(stars->fragment_shader);
	}
	free(stars);
}

int XwaRemasterSkyStars_Prepare(XwaRemasterSkyStars* stars, const AeronScene3D* scene,
								const float world_to_cube[9], const XwaRemasterSkyStarsParams* params) {
	if (!stars || !scene || !params || params->grid_n < 1.0f) {
		return 0;
	}
	const float* view_proj = AeronScene_JitteredViewProj(scene);
	if (!view_proj) {
		return 0;
	}

	static const float identity3[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	const float* basis = world_to_cube ? world_to_cube : identity3;
	float cube_to_world[16] = {
		basis[0], basis[3], basis[6], 0.0f, basis[1], basis[4], basis[7], 0.0f,
		basis[2], basis[5], basis[8], 0.0f, 0.0f,     0.0f,     0.0f,     1.0f,
	};
	sky_stars_mat4_mul(stars->vertex_uniform.cube_to_clip, view_proj, cube_to_world);

	int render_w, render_h, output_w, output_h;
	AeronScene_RenderDims(scene, &render_w, &render_h);
	AeronScene_RtDims(scene, &output_w, &output_h);
	if (render_w <= 0 || render_h <= 0 || output_w <= 0 || output_h <= 0) {
		return 0;
	}
	const float pixel_scale = (float)render_w / (float)output_w;
	const float exposure = params->exposure > 0.0f ? params->exposure : 1.0f;
	stars->vertex_uniform.view[0] = (float)render_w;
	stars->vertex_uniform.view[1] = (float)render_h;
	stars->vertex_uniform.view[2] = params->grid_n;
	stars->vertex_uniform.view[3] = params->density;
	stars->vertex_uniform.geom[0] = params->core_radius_px * pixel_scale;
	stars->vertex_uniform.geom[1] = params->pixel_pitch_px * pixel_scale;
	stars->vertex_uniform.geom[2] = (float)params->game_time_ms;
	stars->vertex_uniform.geom[3] = params->flare_strength;
	stars->vertex_uniform.tone[0] = exposure * params->brightness;
	stars->vertex_uniform.tone[1] = 1.0f;

	stars->fragment_uniform[0] = params->core_radius_px * pixel_scale;
	stars->fragment_uniform[1] = params->pixel_pitch_px * pixel_scale;
	stars->fragment_uniform[2] = params->feather_px * pixel_scale;
	stars->fragment_uniform[3] = 0.0f;

	const uint32_t grid_n = (uint32_t)params->grid_n;
	stars->vertex_count = 36u * grid_n * grid_n;
	return 1;
}

void XwaRemasterSkyStars_Draw(AeronCommandBuffer* command_buffer, AeronRenderPass* render_pass, int rt_w,
							  int rt_h, void* user) {
	(void)rt_w;
	(void)rt_h;
	XwaRemasterSkyStars* stars = (XwaRemasterSkyStars*)user;
	if (!stars || !render_pass || stars->vertex_count == 0) {
		return;
	}
	if (!sky_stars_ensure_pipeline(stars, Aeron_RenderPassGetSampleCount(render_pass))) {
		Aeron_CommandBufferSetFailure(command_buffer, "Procedural starfield pipeline preparation failed");
		return;
	}
	Aeron_BindGraphicsPipeline(render_pass, stars->pipeline);
	Aeron_BindUniformData(render_pass, AERON_SHADER_STAGE_VERTEX, 0, &stars->vertex_uniform,
						  sizeof stars->vertex_uniform);
	Aeron_BindUniformData(render_pass, AERON_SHADER_STAGE_FRAGMENT, 0, stars->fragment_uniform,
						  sizeof stars->fragment_uniform);
	Aeron_Draw(render_pass, stars->vertex_count, 0);
}
