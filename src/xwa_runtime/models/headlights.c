#include "xwa_runtime/models/headlights.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/flight/flight_light.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/mission/mission.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <math.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* A value in the mod's section is wrong: there are no headlights. */
static int s_disabled;
static int s_autoTurnOn;
/* The values that the mod sets, each with its flag. */
static int s_hasPosition;
static int s_hasColor;
static int s_hasPower;
static int s_hasAngle;
static float s_position[3];
static float s_color[3];
static float s_power;
static float s_angle;
/* The loaded mission turns them on. */
static int s_mission;

/* Reads the list of three numbers at `key`, if there is one, and sets
   `*found`. Returns 0 (logged) when it isn't a list of three numbers. */
static int Headlights_ReadVector(const AeronConfigNode* section, const char* key, float out[3], int* found) {
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
						   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), key);
			return 0;
		}
		values[i] = (float)AeronConfigNode_Float(item, 0.0);
	}
	memcpy(out, values, sizeof values);
	*found = 1;
	return 1;
}

void XwaHeadlights_Init(void) {
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	const AeronConfigNode* node;
	int ok = 1;

	s_disabled = 0;
	s_autoTurnOn = 1;
	s_hasPosition = s_hasColor = s_hasPower = s_hasAngle = 0;
	memset(s_position, 0, sizeof s_position);
	memset(s_color, 0, sizeof s_color);
	s_power = 0.0f;
	s_angle = 0.0f;
	root = XwaFlightConfig_Root();
	section = AeronConfigNode_MapGet(root, "headlights");
	if (section == NULL) {
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: headlights must be a map; there are no headlights",
					   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(section));
		s_disabled = 1;
		return;
	}
	ok &= Headlights_ReadVector(section, "position_m", s_position, &s_hasPosition);
	ok &= Headlights_ReadVector(section, "color", s_color, &s_hasColor);
	node = AeronConfigNode_MapGet(section, "power_w");
	if (node != NULL) {
		s_power = (float)AeronConfigNode_Float(node, -1.0);
		s_hasPower = 1;
		if (!(s_power >= 0.0f) || !isfinite(s_power)) {
			Aeron_LogError("xwa.flight", "%s:%d: headlights: power_w must be a number of watts, 0 or more",
						   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node));
			ok = 0;
		}
	}
	node = AeronConfigNode_MapGet(section, "angle_degrees");
	if (node != NULL) {
		s_angle = (float)AeronConfigNode_Float(node, -1.0);
		s_hasAngle = 1;
		if (!(s_angle >= 0.0f && s_angle <= 90.0f)) {
			Aeron_LogError("xwa.flight", "%s:%d: headlights: angle_degrees must be a number from 0 to 90",
						   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node));
			ok = 0;
		}
	}
	node = AeronConfigNode_MapGet(section, "auto_turn_on");
	if (node != NULL && AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogError("xwa.flight", "%s:%d: headlights: auto_turn_on must be true or false",
					   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node));
		ok = 0;
	}
	s_autoTurnOn = AeronConfigNode_Bool(node, 1);
	if (!ok) {
		Aeron_LogError("xwa.flight", "%s: there are no headlights", XWA_FLIGHT_CONFIG_PATH);
	}
	s_disabled = !ok;
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
	int deathStar;

	memset(out, 0, sizeof *out);
	if (s_disabled || g_inHangarReady) {
		return;
	}
	/* A mission that turns them on doesn't use the automatic rule. The game
	   keeps the Death Star mission's light until the reactor core blows up,
	   so g_dirLightCount isn't 0 there. The HD renderer doesn't use that
	   light inside the Death Star (fl_build_scene_lighting). */
	deathStar = g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR;
	out->on = (uint8_t)(s_mission || (s_autoTurnOn && (g_dirLightCount == 0 || deathStar)));
	out->mod_has_position = (uint8_t)s_hasPosition;
	out->mod_has_color = (uint8_t)s_hasColor;
	out->mod_has_power = (uint8_t)s_hasPower;
	out->mod_has_angle = (uint8_t)s_hasAngle;
	memcpy(out->mod_position_m, s_position, sizeof out->mod_position_m);
	memcpy(out->mod_color, s_color, sizeof out->mod_color);
	out->mod_power_w = s_power;
	out->mod_angle_degrees = s_angle;
}
