#include "xwa_remaster/vr_hands.h"

#include "aeron/aeron.h"
#include "aeron/asset/flight_model.h"
#include "aeron/asset/opt_model.h"
#include "aeron/scene/view_rig.h"
#include "xwa_remaster/cockpit_buttons.h"
#include "xwa_runtime/input/vr_stick.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A hand that moved less than this for this long isn't drawn. */
#define VR_HAND_STILL_M 0.002f
#define VR_HAND_STILL_US (10u * 1000u * 1000u)
/* The trigger counts as touched from this value: the hand then points. */
#define VR_HAND_POINT_TRIGGER 0.05f

typedef struct VrHandModel {
	/* Loading was tried: a missing model isn't looked for again. */
	int tried;
	AeronSceneMesh* mesh;
} VrHandModel;

typedef struct VrHandState {
	VrHandModel models[XWA_VR_HAND_POSE_COUNT];
	/* How the loaded models sit on the hand: the mod's settings for the
	   mod's gloves, and the defaults for the game's own. */
	const XwaVrHandConfig* config;
	AeronXrHand hand;
	/* Where the hand was when it last moved, and when. */
	float moved_position[3];
	uint64_t moved_us;
	int moved_valid;
	/* The transform of this frame and of the last one, for the motion
	   vectors. */
	float transform[16];
	float previous_transform[16];
	int transform_valid;
	int previous_valid;
} VrHandState;

static struct {
	VrHandState hands[2];
	float world_scale;
	int enabled;
	/* Whether the loaded models were looked for in the mod. */
	int loaded_use_mod;
} s = { .world_scale = 1.0f, .enabled = 1, .loaded_use_mod = 1 };

void XwaRemasterVrHands_SetEnabled(int enabled) { s.enabled = enabled != 0; }

void XwaRemasterVrHands_SetHands(const AeronXrHand hands[2], float world_scale) {
	const uint64_t now = Aeron_NowUs();
	s.world_scale = world_scale > 0.0f ? world_scale : 1.0f;
	for (int i = 0; i < 2; i++) {
		VrHandState* state = &s.hands[i];
		/* The transform that the last frame drew is this frame's history. */
		state->previous_valid = state->transform_valid;
		if (state->transform_valid) {
			memcpy(state->previous_transform, state->transform, sizeof state->transform);
		}
		state->transform_valid = 0;
		if (!hands || !hands[i].active) {
			memset(&state->hand, 0, sizeof state->hand);
			state->moved_valid = 0;
			continue;
		}
		state->hand = hands[i];
		float moved = 0.0f;
		for (int axis = 0; axis < 3; axis++) {
			const float delta = hands[i].grip.position_m[axis] - state->moved_position[axis];
			moved += delta * delta;
		}
		if (!state->moved_valid || moved > VR_HAND_STILL_M * VR_HAND_STILL_M) {
			memcpy(state->moved_position, hands[i].grip.position_m, sizeof state->moved_position);
			state->moved_us = now;
			state->moved_valid = 1;
		}
	}
}

/* Whether the hand is shown this frame. */
static int vr_hand_shown(const VrHandState* state) {
	return s.enabled && state->hand.active && state->moved_valid &&
		   Aeron_NowUs() - state->moved_us < VR_HAND_STILL_US;
}

/* A hand that holds the virtual stick or throttle is closed. */
static XwaVrHandPose vr_hand_pose(const VrHandState* state) {
	const int hand = (int)(state - s.hands);
	if (XwaVrStick_Held(hand)) {
		return XWA_VR_HAND_POSE_GRASP;
	}
	/* It points at a panel that has buttons. */
	if (XwaRemasterCockpitButtons_OnPanel(hand)) {
		return XWA_VR_HAND_POSE_POINT;
	}
	return state->hand.trigger >= VR_HAND_POINT_TRIGGER ? XWA_VR_HAND_POSE_POINT : XWA_VR_HAND_POSE_NEUTRAL;
}

int XwaRemasterVrHands_NeedSync(void) {
	if (s.loaded_use_mod != XwaVrHands_UseMod()) {
		return 1;
	}
	for (int hand = 0; hand < 2; hand++) {
		if (!vr_hand_shown(&s.hands[hand])) {
			continue;
		}
		for (int pose = 0; pose < XWA_VR_HAND_POSE_COUNT; pose++) {
			if (!s.hands[hand].models[pose].tried) {
				return 1;
			}
		}
	}
	return 0;
}

/* Reads a model file, or returns 0. */
static int vr_hand_read(AeronVfsRoot root, const char* folder, const char* key, char* path, size_t path_size,
						uint8_t** bytes, size_t* size) {
	snprintf(path, path_size, "%s/%s.glb", folder, key);
	return AeronVfs_ReadAll(Aeron_GetVfs(), root, path, 0, bytes, size);
}

/* Loads one pose of a hand. A mod's gloves replace the game's own when the
 * mod has the neutral model, at least: the hand then uses the mod's models
 * and the mod's settings. The neutral pose is loaded first, and it sets
 * `state->config`. */
static AeronSceneMesh* vr_hand_load(AeronCommandBuffer* cmd, int hand, VrHandState* state,
									XwaVrHandPose pose) {
	char key[96], path[160];
	uint8_t* bytes = NULL;
	size_t size = 0;
	AeronFlightModel model;
	int found = 0;

	if (pose == XWA_VR_HAND_POSE_NEUTRAL) {
		state->config = XwaVrHands_Config(hand);
		snprintf(key, sizeof key, "%s_%s", state->config->model, XwaVrHands_PoseName(pose));
		found = XwaVrHands_UseMod() &&
				vr_hand_read(AERON_VFS_ROOT_MOD, "flight/models", key, path, sizeof path, &bytes, &size);
		if (!found) {
			state->config = XwaVrHands_Defaults(hand);
			snprintf(key, sizeof key, "%s_%s", state->config->model, XwaVrHands_PoseName(pose));
			found =
				vr_hand_read(AERON_VFS_ROOT_RESOURCE, "assets/models", key, path, sizeof path, &bytes, &size);
		}
		if (!found) {
			Aeron_LogError("xwa.remaster", "VR hands: no model %s.glb in the mod or in the game", key);
			return NULL;
		}
	} else {
		const int from_mod = state->config != XwaVrHands_Defaults(hand);
		snprintf(key, sizeof key, "%s_%s", state->config->model, XwaVrHands_PoseName(pose));
		/* Only the neutral pose is required. */
		if (!vr_hand_read(from_mod ? AERON_VFS_ROOT_MOD : AERON_VFS_ROOT_RESOURCE,
						  from_mod ? "flight/models" : "assets/models", key, path, sizeof path, &bytes,
						  &size)) {
			return NULL;
		}
	}
	const int built = Aeron_FlightModelBuildMemory(bytes, size, path, &model);
	free(bytes);
	if (!built) {
		Aeron_LogError("xwa.remaster", "VR hands: %s is not a valid model", path);
		return NULL;
	}
	AeronSceneMeshCreateStatus status;
	AeronSceneMesh* mesh = AeronScene_MeshCreate(cmd, &model, key, &status);
	Aeron_FlightModelFree(&model);
	if (!mesh) {
		Aeron_LogError("xwa.remaster", "VR hands: the model %s could not be created (status %d)", path,
					   (int)status);
		return NULL;
	}
	Aeron_LogInfo("xwa.remaster", "VR hands: loaded %s", path);
	return mesh;
}

/* Drops the loaded models, so that they are looked for again. */
static void vr_hand_unload(void) {
	for (int hand = 0; hand < 2; hand++) {
		for (int pose = 0; pose < XWA_VR_HAND_POSE_COUNT; pose++) {
			if (s.hands[hand].models[pose].mesh) {
				AeronScene_MeshDestroy(s.hands[hand].models[pose].mesh);
			}
		}
		memset(s.hands[hand].models, 0, sizeof s.hands[hand].models);
		s.hands[hand].config = NULL;
	}
}

void XwaRemasterVrHands_Sync(AeronCommandBuffer* cmd) {
	/* The player switched between the mod's gloves and the game's own. */
	if (s.loaded_use_mod != XwaVrHands_UseMod()) {
		vr_hand_unload();
		s.loaded_use_mod = XwaVrHands_UseMod();
	}
	for (int hand = 0; hand < 2; hand++) {
		VrHandState* state = &s.hands[hand];
		if (!vr_hand_shown(state)) {
			continue;
		}
		for (int pose = 0; pose < XWA_VR_HAND_POSE_COUNT; pose++) {
			VrHandModel* model = &state->models[pose];
			if (!model->tried) {
				model->tried = 1;
				/* Without the neutral model the hand has no models at all. */
				model->mesh = pose == XWA_VR_HAND_POSE_NEUTRAL || state->models[0].mesh
								  ? vr_hand_load(cmd, hand, state, (XwaVrHandPose)pose)
								  : NULL;
			}
		}
	}
}

void XwaRemasterVrHands_Shutdown(void) {
	vr_hand_unload();
	memset(s.hands, 0, sizeof s.hands);
}

/* A rotation by `degrees` around X, then Y, then Z, as a row-major matrix. */
static void vr_hand_rotation(const float degrees[3], float out[9]) {
	const float k = 3.14159265f / 180.0f;
	const float cx = cosf(degrees[0] * k), sx = sinf(degrees[0] * k);
	const float cy = cosf(degrees[1] * k), sy = sinf(degrees[1] * k);
	const float cz = cosf(degrees[2] * k), sz = sinf(degrees[2] * k);
	out[0] = cz * cy;
	out[1] = cz * sy * sx - sz * cx;
	out[2] = cz * sy * cx + sz * sx;
	out[3] = sz * cy;
	out[4] = sz * sy * sx + cz * cx;
	out[5] = sz * sy * cx - cz * sx;
	out[6] = -sy;
	out[7] = cy * sx;
	out[8] = cy * cx;
}

/* Builds the glove's transform, from the model's space to the scene.
 *
 * A camera at the grip pose gives the grip's axes in the scene: its rows
 * are the grip's +X, -Y and -Z, since eye space is rig space with Y and Z
 * negated. The model is first placed on the hand (rotation, then offset),
 * and the scene's loader has turned the model's (x, y, z) into
 * (-x, -z, y). The glove is scaled with the player. */
static void vr_hand_transform(const XwaVrHandConfig* config, const AeronXrHand* pose,
							  const float seat_rows[9], const float seat_pos[3], float out[16]) {
	AeronViewPose grip = pose->grip;
	float rows[9], position[3], fit[9];
	for (int axis = 0; axis < 3; axis++) {
		grip.position_m[axis] *= s.world_scale;
	}
	AeronViewPose_ComposeCamera(&grip, seat_rows, seat_pos, AERON_OPT_UNITS_PER_METER, rows, position);
	vr_hand_rotation(config->rotation_deg, fit);
	const float scale = AERON_OPT_UNITS_PER_METER * s.world_scale;
	for (int row = 0; row < 3; row++) {
		/* The grip's axes in the scene, as columns. */
		const float axes[3] = { rows[0 * 3 + row], -rows[1 * 3 + row], -rows[2 * 3 + row] };
		/* From the model's glTF axes to the scene. */
		float from_model[3];
		for (int column = 0; column < 3; column++) {
			from_model[column] =
				axes[0] * fit[0 * 3 + column] + axes[1] * fit[1 * 3 + column] + axes[2] * fit[2 * 3 + column];
		}
		/* The mesh holds (-x, -z, y) of the model's (x, y, z). */
		out[row * 4 + 0] = -scale * from_model[0];
		out[row * 4 + 1] = -scale * from_model[2];
		out[row * 4 + 2] = scale * from_model[1];
		out[row * 4 + 3] =
			position[row] + scale * (axes[0] * config->offset_m[0] + axes[1] * config->offset_m[1] +
									 axes[2] * config->offset_m[2]);
	}
	out[12] = 0.0f;
	out[13] = 0.0f;
	out[14] = 0.0f;
	out[15] = 1.0f;
}

void XwaRemasterVrHands_PoseTransform(const AeronViewPose* pose, const float seat_rows[9],
									  const float seat_pos[3], float out[16]) {
	static const XwaVrHandConfig no_fit = { 0 };
	AeronXrHand hand = { 0 };
	hand.grip = *pose;
	vr_hand_transform(&no_fit, &hand, seat_rows, seat_pos, out);
}

int XwaRemasterVrHands_Ray(int hand, const float seat_rows[9], const float seat_pos[3], float origin[3],
						   float direction[3], float* units_per_m) {
	if (hand < 0 || hand > 1) {
		return 0;
	}
	const VrHandState* state = &s.hands[hand];
	if (!vr_hand_shown(state) || !state->config || !state->models[XWA_VR_HAND_POSE_NEUTRAL].mesh) {
		return 0;
	}
	/* The fingertip is a point of the model, so it goes through the model's
	 * transform. The mesh holds (-x, -z, y) of the model's (x, y, z). */
	float transform[16];
	vr_hand_transform(state->config, &state->hand, seat_rows, seat_pos, transform);
	const float tip[3] = { -state->config->tip_m[0], -state->config->tip_m[2], state->config->tip_m[1] };
	for (int row = 0; row < 3; row++) {
		origin[row] = transform[row * 4 + 0] * tip[0] + transform[row * 4 + 1] * tip[1] +
					  transform[row * 4 + 2] * tip[2] + transform[row * 4 + 3];
	}
	/* A camera at the aim pose looks along the aim: its third row. */
	AeronViewPose aim = state->hand.aim;
	float rows[9], position[3];
	for (int axis = 0; axis < 3; axis++) {
		aim.position_m[axis] *= s.world_scale;
	}
	AeronViewPose_ComposeCamera(&aim, seat_rows, seat_pos, AERON_OPT_UNITS_PER_METER, rows, position);
	memcpy(direction, &rows[6], 3 * sizeof direction[0]);
	*units_per_m = AERON_OPT_UNITS_PER_METER * s.world_scale;
	return 1;
}

int XwaRemasterVrHands_Instance(int hand, const float seat_rows[9], const float seat_pos[3],
								AeronSceneMeshInstance* out) {
	if (hand < 0 || hand > 1) {
		return 0;
	}
	VrHandState* state = &s.hands[hand];
	if (!vr_hand_shown(state)) {
		return 0;
	}
	AeronSceneMesh* mesh = state->models[vr_hand_pose(state)].mesh;
	if (!mesh) {
		mesh = state->models[XWA_VR_HAND_POSE_NEUTRAL].mesh;
	}
	if (!mesh || !state->config) {
		return 0;
	}
	vr_hand_transform(state->config, &state->hand, seat_rows, seat_pos, state->transform);
	state->transform_valid = 1;
	memset(out, 0, sizeof *out);
	out->mesh = mesh;
	memcpy(out->transform, state->transform, sizeof out->transform);
	if (state->previous_valid) {
		memcpy(out->prev_transform, state->previous_transform, sizeof out->prev_transform);
	} else {
		memcpy(out->prev_transform, state->transform, sizeof out->prev_transform);
		out->zero_velocity = 1;
	}
	return 1;
}
