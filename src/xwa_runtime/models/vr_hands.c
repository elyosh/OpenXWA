#include "xwa_runtime/models/vr_hands.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa_runtime/models/flight_config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

static const char* const kHandNames[2] = { "left", "right" };

/* The gloves that the game ships (tools/vr_hands). Their models are built in
   the space of the grip pose. */
static const XwaVrHandConfig kDefaults[2] = {
	{ .model = "vr_hand_left", .tip_m = { 0.0f, -0.128f, -0.010f } },
	{ .model = "vr_hand_right", .tip_m = { 0.0f, -0.128f, -0.010f } },
};

static XwaVrHandConfig s_hands[2];
static int s_use_mod = 1;

const char* XwaVrHands_PoseName(XwaVrHandPose pose) {
	static const char* const names[XWA_VR_HAND_POSE_COUNT] = { "neutral", "point", "grasp" };
	return pose >= 0 && pose < XWA_VR_HAND_POSE_COUNT ? names[pose] : NULL;
}

/* Reads the list of three numbers at `key` of a hand, if there is one.
   Returns 0 (logged) when it isn't a list of three numbers. */
static int VrHands_ReadVector(const AeronConfigNode* entry, const char* hand, const char* key, float out[3]) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(entry, key);
	float values[3];

	if (node == NULL) {
		return 1;
	}
	for (int i = 0; i < 3; ++i) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, (size_t)i);
		const AeronConfigNodeType type = AeronConfigNode_Type(item);

		if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) != 3 ||
			(type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) ||
			!isfinite(AeronConfigNode_Float(item, 0.0))) {
			Aeron_LogError("xwa.flight", "%s:%d: vr_hands.%s: %s must be a list of three numbers",
						   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), hand, key);
			return 0;
		}
		values[i] = (float)AeronConfigNode_Float(item, 0.0);
	}
	memcpy(out, values, sizeof values);
	return 1;
}

/* Reads a hand's entry over its defaults. Returns 0 (logged) when the entry
   is invalid. */
static int VrHands_ReadHand(const AeronConfigNode* entry, const char* hand, XwaVrHandConfig* config) {
	const AeronConfigNode* model;
	int ok = 1;

	if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: vr_hands.%s must be a map", XWA_FLIGHT_CONFIG_PATH,
					   AeronConfigNode_Line(entry), hand);
		return 0;
	}
	model = AeronConfigNode_MapGet(entry, "model");
	if (model != NULL) {
		const char* name = AeronConfigNode_String(model, NULL);

		if (name == NULL || name[0] == '\0' || strlen(name) >= sizeof config->model ||
			strpbrk(name, "/\\.") != NULL) {
			Aeron_LogError("xwa.flight",
						   "%s:%d: vr_hands.%s: model must be the start of a model file's name, without a "
						   "folder or an extension",
						   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(model), hand);
			ok = 0;
		} else {
			snprintf(config->model, sizeof config->model, "%s", name);
		}
	}
	ok &= VrHands_ReadVector(entry, hand, "offset_m", config->offset_m);
	ok &= VrHands_ReadVector(entry, hand, "rotation_deg", config->rotation_deg);
	ok &= VrHands_ReadVector(entry, hand, "tip_m", config->tip_m);
	return ok;
}

void XwaVrHands_Init(void) {
	const AeronConfigNode* section;

	memcpy(s_hands, kDefaults, sizeof s_hands);
	section = AeronConfigNode_MapGet(XwaFlightConfig_Root(), "vr_hands");
	if (section == NULL) {
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: vr_hands must be a map. The default hands are used",
					   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(section));
		return;
	}
	for (size_t i = 0; i < AeronConfigNode_MapCount(section); ++i) {
		const char* name = AeronConfigNode_MapKeyAt(section, i);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(section, i);
		int hand = -1;

		for (int side = 0; name != NULL && side < 2; ++side) {
			if (strcmp(name, kHandNames[side]) == 0) {
				hand = side;
			}
		}
		if (hand < 0) {
			Aeron_LogWarn("xwa.flight", "%s:%d: vr_hands: unknown hand %s; it is ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(entry), name != NULL ? name : "");
			continue;
		}
		XwaVrHandConfig config = s_hands[hand];
		if (VrHands_ReadHand(entry, name, &config)) {
			s_hands[hand] = config;
		} else {
			Aeron_LogError("xwa.flight", "%s: the default %s hand is used", XWA_FLIGHT_CONFIG_PATH, name);
		}
	}
}

const XwaVrHandConfig* XwaVrHands_Config(int hand) { return &s_hands[hand == 1 ? 1 : 0]; }

const XwaVrHandConfig* XwaVrHands_Defaults(int hand) { return &kDefaults[hand == 1 ? 1 : 0]; }

void XwaVrHands_SetUseMod(int use_mod) { s_use_mod = use_mod != 0; }

int XwaVrHands_UseMod(void) { return s_use_mod; }
