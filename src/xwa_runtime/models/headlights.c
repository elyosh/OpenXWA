#include "xwa_runtime/models/headlights.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/flight/flight_light.h"
#include "xwa/flight/hangar.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <math.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define HEADLIGHTS_FLIGHT_YAML "flight/flight.yaml"

static int s_defined;
static int s_autoTurnOn;
static float s_position[3];
static float s_color[3];
static float s_power;
/* The loaded mission turns them on. */
static int s_mission;

/* Reads the list of three numbers at `key`, if there is one. Returns 0
   (logged) when it isn't a list of three numbers. */
static int Headlights_ReadVector(const AeronConfigNode* section, const char* key, float out[3]) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(section, key);
	float values[3];
	int i;

	if (node == NULL) {
		return 1;
	}
	for (i = 0; i < 3; ++i) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, (size_t)i);
		const AeronConfigNodeType type = AeronConfigNode_Type(item);

		if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) != 3 ||
			(type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) ||
			!isfinite(AeronConfigNode_Float(item, 0.0))) {
			Aeron_LogError("xwa.flight", "%s:%d: headlights: %s must be a list of three numbers",
						   HEADLIGHTS_FLIGHT_YAML, AeronConfigNode_Line(node), key);
			return 0;
		}
		values[i] = (float)AeronConfigNode_Float(item, 0.0);
	}
	memcpy(out, values, sizeof values);
	return 1;
}

void XwaHeadlights_Init(void) {
	static const float kPosition[3] = { 0.0f, 0.0f, 20.0f };
	static const float kColor[3] = { 1.0f, 1.0f, 1.0f };
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	const AeronConfigNode* node;
	int ok = 1;

	s_defined = 0;
	s_autoTurnOn = 1;
	memcpy(s_position, kPosition, sizeof s_position);
	memcpy(s_color, kColor, sizeof s_color);
	s_power = 0.0f;
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, HEADLIGHTS_FLIGHT_YAML, &file,
									&error)) {
		/* mission_backdrops.c logs a file that can't be read. */
		return;
	}
	root = AeronConfigFile_Root(file);
	section = AeronConfigNode_MapGet(root, "headlights");
	if (AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1 || section == NULL) {
		AeronConfigFile_Destroy(file);
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: headlights must be a map; there are no headlights",
					   HEADLIGHTS_FLIGHT_YAML, AeronConfigNode_Line(section));
		AeronConfigFile_Destroy(file);
		return;
	}
	ok &= Headlights_ReadVector(section, "position_m", s_position);
	ok &= Headlights_ReadVector(section, "color", s_color);
	node = AeronConfigNode_MapGet(section, "power_w");
	s_power = (float)AeronConfigNode_Float(node, -1.0);
	if (!(s_power > 0.0f) || !isfinite(s_power)) {
		Aeron_LogError("xwa.flight", "%s:%d: headlights: power_w must be a number of watts above 0",
					   HEADLIGHTS_FLIGHT_YAML, AeronConfigNode_Line(node != NULL ? node : section));
		ok = 0;
	}
	node = AeronConfigNode_MapGet(section, "auto_turn_on");
	if (node != NULL && AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogError("xwa.flight", "%s:%d: headlights: auto_turn_on must be true or false",
					   HEADLIGHTS_FLIGHT_YAML, AeronConfigNode_Line(node));
		ok = 0;
	}
	s_autoTurnOn = AeronConfigNode_Bool(node, 1);
	if (!ok) {
		Aeron_LogError("xwa.flight", "%s: there are no headlights", HEADLIGHTS_FLIGHT_YAML);
	}
	s_defined = ok;
	AeronConfigFile_Destroy(file);
}

void XwaHeadlights_LoadMission(const char* missionPath) {
	AeronConfigFile* config = NULL;
	const AeronConfigNode* node;
	char path[300];

	s_mission = 0;
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	node = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "headlights");
	if (node != NULL && AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogWarn("xwa.mission", "%s:%d: headlights must be true or false; it is ignored", path,
					  AeronConfigNode_Line(node));
	} else {
		s_mission = AeronConfigNode_Bool(node, 0);
	}
	AeronConfigFile_Destroy(config);
}

void XwaHeadlights_Capture(XwaSnapshot* snapshot) {
	XwaHeadlight* out = &snapshot->headlight;

	memset(out, 0, sizeof *out);
	if (!s_defined) {
		return;
	}
	/* A mission that turns them on doesn't use the automatic rule. */
	out->on = (uint8_t)(s_mission ? !g_inHangarReady : s_autoTurnOn && g_dirLightCount == 0);
	memcpy(out->position_m, s_position, sizeof out->position_m);
	memcpy(out->color, s_color, sizeof out->color);
	out->power_w = s_power;
}
