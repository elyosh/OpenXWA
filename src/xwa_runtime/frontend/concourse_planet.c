#include "xwa_runtime/frontend/concourse_planet.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa_runtime/models/mission_file.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* The mission whose file was read, and what it sets. -1 is "not set". */
static char s_missionPath[256];
static int s_read;
static int s_image = -1;
static int s_x = -1;
static int s_y = -1;

/* The path of the current mission, such as "missions\1b6m4tftc-r.tie", or
   "" when the mission list doesn't have it. */
static void ConcoursePlanet_MissionPath(char* out, size_t outSize) {
	const int dir = g_pilotData.missionDirectoryId;
	int i;

	out[0] = 0;
	if (g_missionList == NULL || dir < 0 ||
		dir >= (int)(sizeof g_campaignDirNames / sizeof g_campaignDirNames[0])) {
		return;
	}
	for (i = 0; i < g_missionCount; ++i) {
		if (g_missionList[i].missionIdx == g_currentMissionId) {
			snprintf(out, outSize, "%s\\%s", g_campaignDirNames[dir], g_missionList[i].fileName);
			return;
		}
	}
}

static int ConcoursePlanet_ReadInt(const char* path, const AeronConfigNode* planet, const char* key, int min,
								   int max) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(planet, key);
	int64_t value;

	if (node == NULL) {
		return -1;
	}
	value = AeronConfigNode_Int(node, 0);
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < min || value > max) {
		Aeron_LogWarn("xwa.mission",
					  "%s:%d: concourse: front_planet: %s must be a number from %d to %d; it is ignored",
					  path, AeronConfigNode_Line(node), key, min, max);
		return -1;
	}
	return (int)value;
}

static void ConcoursePlanet_Read(void) {
	char missionPath[sizeof s_missionPath];
	AeronConfigFile* config = NULL;
	const AeronConfigNode* concourse;
	const AeronConfigNode* planet;
	char path[300];

	ConcoursePlanet_MissionPath(missionPath, sizeof missionPath);
	if (s_read && strcmp(missionPath, s_missionPath) == 0) {
		return;
	}
	s_read = 1;
	snprintf(s_missionPath, sizeof s_missionPath, "%s", missionPath);
	s_image = -1;
	s_x = -1;
	s_y = -1;
	if (!missionPath[0] || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	concourse = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "concourse");
	planet = AeronConfigNode_MapGet(concourse, "front_planet");
	if ((concourse != NULL && AeronConfigNode_Type(concourse) != AERON_CONFIG_MAP) ||
		(planet != NULL && AeronConfigNode_Type(planet) != AERON_CONFIG_MAP)) {
		Aeron_LogError("xwa.mission", "%s:%d: concourse must be a map with a front_planet map; it is ignored",
					   path, AeronConfigNode_Line(planet != NULL ? planet : concourse));
		planet = NULL;
	}
	if (planet != NULL) {
		s_image = ConcoursePlanet_ReadInt(path, planet, "image", 1, 32767);
		s_x = ConcoursePlanet_ReadInt(path, planet, "x", -32768, 32767);
		s_y = ConcoursePlanet_ReadInt(path, planet, "y", -32768, 32767);
	}
	AeronConfigFile_Destroy(config);
}

int XwaConcoursePlanet_Image(void) {
	ConcoursePlanet_Read();
	return s_image;
}

void XwaConcoursePlanet_Center(int* x, int* y) {
	ConcoursePlanet_Read();
	if (s_x != -1) {
		*x = s_x;
	}
	if (s_y != -1) {
		*y = s_y;
	}
}
