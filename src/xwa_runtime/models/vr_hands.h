#ifndef XWA_RUNTIME_MODELS_VR_HANDS_H
#define XWA_RUNTIME_MODELS_VR_HANDS_H

/* The gloves that the cockpit shows where the player's VR controllers are.
 * The game ships a pair. A mod can replace them, and say how its models
 * sit on the hand, in the `vr_hands` section of its flight settings:
 *
 *   vr_hands:
 *     left:  { model: vr_hand_left,  offset_m: [0, 0, 0], rotation_deg: [0, 0, 0], tip_m: [0, -0.128, -0.01]
 * } right: { model: vr_hand_right, offset_m: [0, 0, 0], rotation_deg: [0, 0, 0], tip_m: [0, -0.128, -0.01] }
 *
 * Each key that the mod sets replaces the default. The section is read once
 * at startup. */

#ifdef __cplusplus
extern "C" {
#endif

/* The poses of a hand. Each one is a model of its own. */
typedef enum XwaVrHandPose {
	XWA_VR_HAND_POSE_NEUTRAL,
	XWA_VR_HAND_POSE_POINT,
	XWA_VR_HAND_POSE_GRASP,
	XWA_VR_HAND_POSE_COUNT,
} XwaVrHandPose;

typedef struct XwaVrHandConfig {
	/* The start of the model files' names: the files are
	   flight/models/<model>_neutral.glb, <model>_point.glb and
	   <model>_grasp.glb. Only the first one is required. */
	char model[48];
	/* Where the model sits on the hand: the model is turned by
	   `rotation_deg` (around X, then Y, then Z) and moved by `offset_m`, in
	   the space of the OpenXR grip pose. Both are zero for a model built in
	   that space. */
	float offset_m[3];
	float rotation_deg[3];
	/* The tip of the index finger of the `point` model, in the model's
	   space. */
	float tip_m[3];
} XwaVrHandConfig;

/* The name of a pose in the model files' names. */
const char* XwaVrHands_PoseName(XwaVrHandPose pose);

/* Reads the `vr_hands` section of flight/flight.yaml. */
void XwaVrHands_Init(void);

/* The settings of a hand: 0 is the left hand and 1 the right one. Config
 * gives the mod's settings, and Defaults those of the game's own gloves. */
const XwaVrHandConfig* XwaVrHands_Config(int hand);
const XwaVrHandConfig* XwaVrHands_Defaults(int hand);

/* Whether the mod's gloves are used when it has some (the "Use the Mod's
 * Hands" option). When off, the game's own gloves are always shown. */
void XwaVrHands_SetUseMod(int use_mod);
int XwaVrHands_UseMod(void);

#ifdef __cplusplus
}
#endif

#endif
