/*
 * Frontend model-preview PiP — see xwa_remaster/preview.h.
 *
 * Renders through the same AeronScene_Render path as the flight
 * driver, with an IDENTITY camera: the record's transforms are
 * engine-computed model→EYE (basis + eye_delta, OPT-native axes), so
 * the scene's "world" space IS eye space — instances and glow fans
 * submit their captured coordinates unchanged, and the lighting env
 * keeps its eye-space convention (dir lights rotated by the captured
 * cam_rows, camera at the origin).
 *
 * One shared scene serves all slots: its color/depth RTs are
 * transient per record — the persistent per-slot output is the
 * tonemapped present RT (AeronScenePresentChain with src_coverage so
 * the transparent background survives the frontend z-merge). Records
 * render sequentially within the frame's command buffer.
 *
 * Classic projection: screen = viewport_center + 512 * xy/z
 * (g_projScale) over the record's dst extents, expressed through the
 * scene's parametric camera: h_half = atan(dst_w/1024), v_half =
 * atan(dst_h/1024), reversed-Z near 64 (engine world units).
 */

#include "xwa_remaster/preview.h"

#include "aeron/aeron.h"
#include "aeron/scene/present.h"
#include "aeron/scene/scene3d.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/ship.h"
#include "xwa_remaster/xwa_remaster.h"

#include <math.h>
#include <string.h>

#define PREVIEW_RT_W 2048
#define PREVIEW_RT_H 1536
#define PREVIEW_SLOTS 4      /* == XWA_SNAP_MAX_MODEL_PREVIEWS */
#define PREVIEW_NEAR_Z 64.0f /* reversed-Z near (engine world units) */

static struct {
	AeronScene3D* scene; /* shared; color/depth transient per record */
	AeronRenderTarget* present[PREVIEW_SLOTS];
	AeronScenePresentChain* chain;
	AeronSampler* chain_sampler;
	AeronSampleCount scene_requested_samples;
} s;

static int preview_ensure(void) {
	const AeronSampleCount requested_samples = XwaRemaster_MsaaSampleCount();
	if (s.scene && s.scene_requested_samples == requested_samples) {
		return s.chain && s.chain_sampler;
	}
	if (s.scene) {
		AeronScene_Destroy(s.scene);
		s.scene = NULL;
	}
	s.scene = AeronScene_Create(&(AeronScene3DDesc) {
		.rt_width = PREVIEW_RT_W,
		.rt_height = PREVIEW_RT_H,
		.color_format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
		.with_normal_rt = 1,
		.sample_count = requested_samples,
		.view_space_to_meters = 1.0f / 40.96f,
	});
	if (!s.scene) {
		Aeron_LogError("xwa.remaster", "preview: scene create failed");
		return 0;
	}
	s.scene_requested_samples = requested_samples;
	/* Transparent background — the src-coverage tonemap keeps it for
	 * the frontend z-merge PMA-over composite. */
	AeronScene_SetClearColor(s.scene, (const float[4]) { 0.0f, 0.0f, 0.0f, 0.0f });
	if (!s.chain) {
		s.chain = AeronScenePresentChain_Create(AERON_TEXTURE_FORMAT_RGBA16_FLOAT);
	}
	if (!s.chain_sampler) {
		s.chain_sampler =
			Aeron_CreateSampler(&(AeronSamplerDesc) { .min_filter = AERON_FILTER_LINEAR,
													  .mag_filter = AERON_FILTER_LINEAR,
													  .address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
													  .address_v = AERON_ADDRESS_CLAMP_TO_EDGE });
	}
	if (!s.chain || !s.chain_sampler) {
		Aeron_LogError("xwa.remaster", "preview: present resources create failed");
		return 0;
	}
	return 1;
}

/* "FlightModels\\XWING.OPT" -> "XWING" (the mirrored table lowercases). */
static AeronSceneMesh* preview_mesh(const char* opt_name) {
	const char* base = opt_name;
	for (const char* p = opt_name; *p; p++) {
		if (*p == '/' || *p == '\\') {
			base = p + 1;
		}
	}
	return XwaRemasterShip_MeshForName(base);
}

AeronTexture* XwaRemasterPreview_Render(AeronCommandBuffer* cmd, const XwaModelPreview* p, int slot,
										const XwaDirLight* lights, uint32_t light_count,
										const XwaAssetRef* glow_tex) {
	if (!cmd || !p || slot < 0 || slot >= PREVIEW_SLOTS || p->dst_w <= 0 || p->dst_h <= 0) {
		return NULL;
	}
	if (!preview_ensure()) {
		Aeron_CommandBufferSetFailure(cmd, "Model-preview resource preparation failed");
		return NULL;
	}
	if (!s.present[slot]) {
		s.present[slot] =
			Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = PREVIEW_RT_W,
																.height = PREVIEW_RT_H,
																.format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
																.debug_name = "xwa.preview.present" });
	}
	if (!s.present[slot]) {
		Aeron_CommandBufferSetFailure(cmd, "Model-preview render-target creation failed");
		return NULL;
	}

	/* Identity camera (world == eye) + the classic projection over the
	 * record's dst extents. */
	AeronSceneCamera cam;
	memset(&cam, 0, sizeof cam);
	cam.ori[0] = 1.0f; /* identity (w, x, y, z) */
	cam.h_half_rad = atanf((float)p->dst_w / 1024.0f);
	cam.v_half_rad = atanf((float)p->dst_h / 1024.0f);
	cam.near_z = PREVIEW_NEAR_Z;
	if (!AeronScene_Begin(s.scene, &cam)) {
		Aeron_CommandBufferSetFailure(cmd, "Model-preview scene initialization failed");
		return NULL;
	}

	XwaFlightSsaoParams ssao = { 0 };
	XwaRemasterFlight_GetSsao(&ssao);
	AeronScene_SetPost(s.scene, &(AeronScenePostDesc) {
									.ssao_quality = ssao.quality,
									.ssao_intensity = ssao.intensity,
									.ssao_power = ssao.power,
									.ssao_radius_view = ssao.radius_view,
									.ssao_bias_view = ssao.bias_view,
									.ssao_direct = ssao.direct,
									.ssao_debug_viz = ssao.debug_viz,
									.ssao_min_screen_frac = ssao.min_screen_frac,
									.ssao_max_screen_frac = ssao.max_screen_frac,
									.ssao_sample_jitter = ssao.sample_jitter,
								});
	int render_w;
	int render_h;
	AeronScene_RenderDims(s.scene, &render_w, &render_h);
	const XwaShipAoParams ao = {
		.intensity = ssao.intensity,
		.power = ssao.power,
		.rt_w = (float)render_w,
		.rt_h = (float)render_h,
		.direct = ssao.direct,
	};
	const int ao_on = ssao.quality > 0 && ssao.intensity > 0.0f;

	/* Eye-space lighting env: the engine's own preview light
	 * (world-space dir_lights channel) rotated by this view's captured
	 * camera; camera at the origin. */
	XwaRemasterShip_SetPbrEnv(s.scene, lights, light_count, p->cam_rows, NULL, ao_on ? &ao : NULL,
							  /*point_tuning=*/NULL, /*ambient_cube=*/NULL);

	/* Model->eye ("world") instance. The engine consumes the captured
	 * basis through Math3D_RotateVec3, which is COLUMN-major (out.x =
	 * m0*x + m3*y + m6*z) — so the classic transform is the TRANSPOSE
	 * of the stored rows: eye = basis^T * v + eye_delta. The engine
	 * normalized the classic model's vertices at load (record's
	 * model_scale); the cooked mesh is raw OPT geometry. A missing
	 * mirrored mesh just renders an empty transparent PiP. */
	AeronSceneMesh* mesh = preview_mesh(p->opt_name);
	if (mesh) {
		AeronSceneMeshInstance inst;
		memset(&inst, 0, sizeof inst);
		inst.mesh = mesh;
		const float k = p->model_scale > 0.0f ? p->model_scale : 1.0f;
		float* m = inst.transform;
		m[0] = k * p->obj_basis[0];
		m[1] = k * p->obj_basis[3];
		m[2] = k * p->obj_basis[6];
		m[3] = p->eye_delta[0];
		m[4] = k * p->obj_basis[1];
		m[5] = k * p->obj_basis[4];
		m[6] = k * p->obj_basis[7];
		m[7] = p->eye_delta[1];
		m[8] = k * p->obj_basis[2];
		m[9] = k * p->obj_basis[5];
		m[10] = k * p->obj_basis[8];
		m[11] = p->eye_delta[2];
		m[12] = 0.0f;
		m[13] = 0.0f;
		m[14] = 0.0f;
		m[15] = 1.0f;
		memcpy(inst.prev_transform, inst.transform, sizeof inst.transform);
		inst.no_local_lights = 1; /* studio sun + ambient only */
		inst.zero_velocity = 1;
		/* The classic renderer never rasterizes back-facing OPT polygons
		 * (CPU facing test in RenderScene_BuildMeshVisibleFaces); without
		 * culling, near-coincident interior faces z-fight the hull.
		 * Cooked glbs are CCW-front; BACK keeps the hull solid under
		 * this projection (verified empirically). */
		inst.cull_mode = AERON_CULL_BACK;
		AeronScene_AddMeshInstance(s.scene, &inst);

		/* State-derived engine glows from the mesh's own glb extras —
		 * the classic preview renders every emitter at a fixed base
		 * scale 0.6 with the ~6% flicker (identity camera: the
		 * instance transform IS the eye placement). */
		if (glow_tex) {
			static uint32_t flick = 0x2545F491u;
			flick ^= flick << 13;
			flick ^= flick >> 17;
			flick ^= flick << 5;
			const float scale = (float)(0.60000002 - (double)(flick & 0xf) * 0.0024999999);
			XwaRemasterShip_SubmitEngineGlows(s.scene, mesh, inst.transform, k,
											  /*table=*/NULL, /*knockout_mask=*/0, scale,
											  /*crows=*/NULL, /*cam_pos=*/NULL, glow_tex);
		}
	}

	if (!AeronScene_Render(s.scene, cmd)) {
		return NULL;
	}

	/* Tonemap into the slot's present RT (no bloom; src_coverage keeps
	 * the transparent background). The scene color RT is transient —
	 * the next record's render reuses it. */
	AeronTexture* scene_tex = Aeron_RenderTargetGetTexture(AeronScene_SceneRt(s.scene));
	if (!scene_tex) {
		Aeron_CommandBufferSetFailure(cmd, "Model-preview scene produced no output");
		return NULL;
	}
	AeronRenderPass* pp = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
		.color_target = s.present[slot],
		.clear_color = 1,
		.clear_color_rgba = { 0.0f, 0.0f, 0.0f, 0.0f },
		.command_buffer = cmd,
		.debug_label = "OpenXWA model preview tonemap",
	});
	if (!pp) {
		return NULL;
	}
	static const float tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	AeronScenePresentChain_Draw(s.chain, pp, scene_tex, s.chain_sampler, /*bloom_tex=*/NULL, 0.0f,
								PREVIEW_RT_W, PREVIEW_RT_H, /*bar_y_uv=*/1.0f, tint,
								/*src_coverage=*/1);
	Aeron_EndRenderPass(pp);
	return Aeron_RenderTargetGetTexture(s.present[slot]);
}

void XwaRemasterPreview_Shutdown(void) {
	/* Meshes follow the classic processed-OPT lifetime in XwaRemasterShip. */
	for (int i = 0; i < PREVIEW_SLOTS; i++) {
		if (s.present[i]) {
			Aeron_DestroyRenderTarget(s.present[i]);
		}
	}
	if (s.chain_sampler) {
		Aeron_DestroySampler(s.chain_sampler);
	}
	if (s.chain) {
		AeronScenePresentChain_Destroy(s.chain);
	}
	if (s.scene) {
		AeronScene_Destroy(s.scene);
	}
	memset(&s, 0, sizeof s);
}
