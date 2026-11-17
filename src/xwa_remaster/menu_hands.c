#include "xwa_remaster/menu_hands.h"

#include "aeron/aeron.h"
#include "aeron/asset/opt_model.h"
#include "aeron/asset/plain_gltf_model.h"
#include "aeron/scene/present.h"
#include "aeron/scene/scene3d.h"
#include "aeron/xr.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/scene_lights.h"
#include "xwa_remaster/ship.h"
#include "xwa_remaster/vr_hands.h"
#include "xwa_remaster/xwa_remaster.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The near plane of the hands' scene, in scene units: about 2 cm. */
#define MENU_HANDS_NEAR_Z 1.0f

/* The headset's model of a hand's controller. */
typedef struct MenuController {
	/* The model that `mesh` was built from; 0 before the first try. */
	uint32_t generation;
	AeronSceneMesh* mesh;
	AeronPlainGltfNodes* nodes;
	/* For each moving node of the runtime, its node in `nodes`, or -1. */
	int node_index[AERON_XR_MODEL_NODE_MAX];
	int node_count;
	AeronSceneMeshTable table;
} MenuController;

static struct {
	int controllers;
	AeronScene3D* scene;
	int scene_width, scene_height;
	AeronScenePresentChain* chain;
	AeronTextureFormat chain_format;
	AeronSampler* sampler;
	float exposure;
	int rendered;
	MenuController hands[2];
	/* A model file given for a test, in place of the runtime's. */
	uint8_t* test_glb;
	size_t test_glb_size;
	int test_tried;
} s = { .controllers = 1 };

void XwaRemasterMenuHands_SetControllers(int enabled) { s.controllers = enabled != 0; }

int XwaRemasterMenuHands_Wanted(void) {
	AeronXrHand hands[2];
	return Aeron_XrSessionRunning() && Aeron_XrHands(hands);
}

static void menu_controller_release(MenuController* controller) {
	if (controller->mesh) {
		AeronScene_MeshDestroy(controller->mesh);
	}
	free(controller->nodes);
	controller->mesh = NULL;
	controller->nodes = NULL;
	controller->node_count = 0;
}

/* XWA_VR_CONTROLLER_GLB in the environment names a glTF binary file of the
 * mod's folder that stands for the runtime's model of both controllers. It
 * is there to check the loading on a runtime that gives no models. */
static int menu_test_model(AeronXrControllerModel* out) {
	if (!s.test_tried) {
		const char* name = getenv("XWA_VR_CONTROLLER_GLB");
		s.test_tried = 1;
		if (name && name[0] &&
			!AeronVfs_ReadAll(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, name, 0, &s.test_glb, &s.test_glb_size)) {
			Aeron_LogWarn("xwa.remaster", "menu hands: the test model %s can't be read", name);
		}
	}
	if (!s.test_glb) {
		return 0;
	}
	memset(out, 0, sizeof *out);
	out->generation = 0xffffffffu;
	out->glb = s.test_glb;
	out->glb_size = s.test_glb_size;
	return 1;
}

/* Builds the controller's mesh from the runtime's file, once for each
 * model. A file that can't be used is reported once: the hand is then drawn
 * as in the cockpit. */
static void menu_controller_sync(AeronCommandBuffer* cmd, int hand) {
	MenuController* controller = &s.hands[hand];
	AeronXrControllerModel model;
	const int has_model = s.controllers && (Aeron_XrControllerModel(hand, &model) || menu_test_model(&model));
	if (!has_model) {
		if (controller->generation != 0) {
			menu_controller_release(controller);
			controller->generation = 0;
		}
		return;
	}
	if (controller->generation == model.generation) {
		return;
	}
	menu_controller_release(controller);
	controller->generation = model.generation;
	controller->nodes = (AeronPlainGltfNodes*)calloc(1, sizeof *controller->nodes);
	AeronFlightModel flight;
	char error[160], name[48];
	snprintf(name, sizeof name, "vr_controller_%s", hand ? "right" : "left");
	const uint64_t start = Aeron_NowUs();
	if (!controller->nodes || !Aeron_FlightModelBuildPlainGlb(model.glb, model.glb_size, name, &flight,
															  controller->nodes, error, sizeof error)) {
		Aeron_LogWarn("xwa.remaster", "menu hands: the %s controller's model can't be used (%s)",
					  hand ? "right" : "left", controller->nodes ? error : "out of memory");
		menu_controller_release(controller);
		return;
	}
	AeronSceneMeshCreateStatus status;
	controller->mesh = AeronScene_MeshCreate(cmd, &flight, name, &status);
	Aeron_FlightModelFree(&flight);
	if (!controller->mesh) {
		Aeron_LogWarn("xwa.remaster", "menu hands: the %s controller's model couldn't be created (status %d)",
					  hand ? "right" : "left", (int)status);
		menu_controller_release(controller);
		return;
	}
	controller->node_count = model.node_count;
	for (int i = 0; i < model.node_count; i++) {
		controller->node_index[i] = AeronPlainGltf_FindNode(controller->nodes, model.node_names[i]);
		if (controller->node_index[i] < 0) {
			Aeron_LogWarn("xwa.remaster", "menu hands: the model has no node named %s", model.node_names[i]);
		}
	}
	Aeron_LogInfo("xwa.remaster", "menu hands: the %s controller's model has %d parts, loaded in %.0f ms",
				  hand ? "right" : "left", controller->nodes->component_count,
				  (double)(Aeron_NowUs() - start) / 1000.0);
}

/* Fills the scene instance of a hand's controller. Returns 0 when the hand
 * has no usable model or the runtime doesn't know where it is. */
static int menu_controller_instance(int hand, const AeronXrHand* tracked, AeronSceneMeshInstance* out) {
	static const float seat_rows[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	static const float seat_pos[3] = { 0.0f, 0.0f, 0.0f };
	MenuController* controller = &s.hands[hand];
	AeronXrControllerModelState state;
	if (!controller->mesh) {
		return 0;
	}
	if (!Aeron_XrControllerModelState(hand, &state)) {
		/* The test model has no state: it is drawn at the hand's grip. */
		if (!s.test_glb || !tracked->active) {
			return 0;
		}
		memset(&state, 0, sizeof state);
		state.pose = tracked->grip;
	}
	const AeronViewPose* poses[AERON_PLAIN_GLTF_MAX_NODES] = { NULL };
	uint8_t hidden[AERON_PLAIN_GLTF_MAX_NODES] = { 0 };
	for (int i = 0; i < state.node_count && i < controller->node_count; i++) {
		const int node = controller->node_index[i];
		if (node >= 0) {
			poses[node] = &state.nodes[i];
			hidden[node] = !state.node_visible[i];
		}
	}
	AeronPlainGltf_Pose(controller->nodes, poses, hidden, &controller->table);
	memset(out, 0, sizeof *out);
	out->mesh = controller->mesh;
	out->mesh_table = &controller->table;
	XwaRemasterVrHands_PoseTransform(&state.pose, seat_rows, seat_pos, out->transform);
	memcpy(out->prev_transform, out->transform, sizeof out->prev_transform);
	out->zero_velocity = 1;
	return 1;
}

static int menu_hands_ensure(const AeronViewRig* rig) {
	if (s.scene && (s.scene_width != rig->width || s.scene_height != rig->height)) {
		AeronScene_Destroy(s.scene);
		s.scene = NULL;
	}
	if (!s.scene) {
		s.scene = AeronScene_Create(&(AeronScene3DDesc) {
			.rt_width = rig->width,
			.rt_height = rig->height,
			.color_format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
			.with_normal_rt = 1,
			.sample_count = XwaRemaster_MsaaSampleCount(),
			.view_space_to_meters = AERON_OPT_METERS_PER_UNIT,
		});
		if (!s.scene) {
			Aeron_LogError("xwa.remaster", "menu hands: scene create failed");
			return 0;
		}
		s.scene_width = rig->width;
		s.scene_height = rig->height;
		/* Transparent around the hands: the eye images go over the screen. */
		AeronScene_SetClearColor(s.scene, (const float[4]) { 0.0f, 0.0f, 0.0f, 0.0f });
	}
	if (!s.sampler) {
		s.sampler = Aeron_CreateSampler(&(AeronSamplerDesc) { .min_filter = AERON_FILTER_LINEAR,
															  .mag_filter = AERON_FILTER_LINEAR,
															  .address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
															  .address_v = AERON_ADDRESS_CLAMP_TO_EDGE });
	}
	return s.sampler != NULL;
}

int XwaRemasterMenuHands_RenderView(AeronCommandBuffer* cmd, const AeronViewRig* rig, int view) {
	static const float seat_rows[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	static const float seat_pos[3] = { 0.0f, 0.0f, 0.0f };
	AeronXrHand hands[2];
	s.rendered = 0;
	if (!cmd || !rig || view < 0 || view >= rig->count || !Aeron_XrHands(hands) || !menu_hands_ensure(rig)) {
		return 0;
	}
	/* The models are loaded with the first view of the frame. */
	if (view == 0) {
		if (XwaRemasterVrHands_NeedSync()) {
			XwaRemasterVrHands_Sync(cmd);
		}
		for (int hand = 0; hand < 2; hand++) {
			menu_controller_sync(cmd, hand);
		}
	}
	AeronSceneCamera camera;
	float rows[9];
	XwaRemasterFlight_RigViewCamera(&rig->views[view], rig->width, rig->height, AERON_OPT_UNITS_PER_METER,
									MENU_HANDS_NEAR_Z, &camera, rows);
	AeronScene_SetAnimationTime(s.scene, (double)Aeron_NowUs() * 1.0e-6);
	AeronScene_SetImageRadiance(s.scene, XwaRemaster_ClassicRadiance());
	if (!AeronScene_Begin(s.scene, &camera)) {
		return 0;
	}
	/* One light from above and behind the player's right shoulder, as
	 * bright as a classic light of brightness 1. The scene's axes are X to
	 * the right, Y down and Z ahead. */
	const float illuminance = XwaRemaster_ClassicLightIlluminance();
	const XwaRemasterDirLight light = {
		.world_dir = { 0.35f, -0.80f, -0.49f },
		.color = { illuminance, illuminance, illuminance },
		.source_backdrop_index = -1,
	};
	s.exposure = XwaRemasterFlight_LightMeterExposure(&light, 1);
	AeronScene_SetExposure(s.scene, s.exposure);
	XwaRemasterShip_SetPbrEnv(s.scene, &light, 1, -1, rows, NULL, NULL, NULL, -1.0f);
	int drawn = 0;
	for (int hand = 0; hand < 2; hand++) {
		AeronSceneMeshInstance instance;
		if (!hands[hand].active) {
			continue;
		}
		if (!menu_controller_instance(hand, &hands[hand], &instance) &&
			!XwaRemasterVrHands_Instance(hand, seat_rows, seat_pos, &instance)) {
			continue;
		}
		instance.cull_mode = AERON_CULL_BACK;
		AeronScene_AddMeshInstance(s.scene, &instance);
		drawn = 1;
	}
	if (!drawn || !AeronScene_Render(s.scene, cmd)) {
		return 0;
	}
	s.rendered = 1;
	return 1;
}

void XwaRemasterMenuHands_DrawView(AeronRenderPass* pass, AeronRenderTarget* target) {
	static const float tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	if (!s.rendered || !pass || !target) {
		return;
	}
	s.rendered = 0;
	const AeronTextureFormat format = Aeron_TextureGetFormat(Aeron_RenderTargetGetTexture(target));
	if (s.chain && s.chain_format != format) {
		AeronScenePresentChain_Destroy(s.chain);
		s.chain = NULL;
	}
	if (!s.chain) {
		s.chain = AeronScenePresentChain_Create(format);
		s.chain_format = format;
	}
	AeronTexture* scene_texture = Aeron_RenderTargetGetTexture(AeronScene_SceneRt(s.scene));
	if (!s.chain || !scene_texture) {
		return;
	}
	/* The scene's alpha is kept, so the image stays transparent around the
	 * hands. */
	AeronScenePresentChain_Draw(s.chain, pass, scene_texture, s.sampler, NULL, 0.0f, s.exposure, NULL,
								s.scene_width, s.scene_height, 1.0f, tint, 1, NULL, NULL);
}

void XwaRemasterMenuHands_Shutdown(void) {
	for (int hand = 0; hand < 2; hand++) {
		menu_controller_release(&s.hands[hand]);
		s.hands[hand].generation = 0;
	}
	if (s.chain) {
		AeronScenePresentChain_Destroy(s.chain);
		s.chain = NULL;
	}
	if (s.sampler) {
		Aeron_DestroySampler(s.sampler);
		s.sampler = NULL;
	}
	if (s.scene) {
		AeronScene_Destroy(s.scene);
		s.scene = NULL;
	}
	free(s.test_glb);
	s.test_glb = NULL;
	s.test_tried = 0;
}
