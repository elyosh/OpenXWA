#include "xwa_runtime/models/mission_file.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa_runtime/models/names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

static char MissionFile_Lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

/* flight/missions/<mission path without its extension>.yaml in the mod folder, lower case,
   with '/' separators. */
static void MissionFile_Path(const char* missionPath, char* path, size_t pathSize) {
	const char* dot = strrchr(missionPath, '.');
	const char* separator = strrchr(missionPath, '\\');
	const char* slash = strrchr(missionPath, '/');
	size_t length;
	size_t index;
	size_t offset;

	if (slash != NULL && (separator == NULL || slash > separator)) {
		separator = slash;
	}
	length = dot != NULL && (separator == NULL || dot > separator) ? (size_t)(dot - missionPath)
																   : strlen(missionPath);
	offset = (size_t)snprintf(path, pathSize, "flight/missions/");
	for (index = 0; index < length && offset + 1 < pathSize; ++index) {
		const char c = missionPath[index];

		path[offset++] = c == '\\' ? '/' : MissionFile_Lower(c);
	}
	path[offset] = '\0';
	snprintf(path + offset, pathSize - offset, ".yaml");
}

int XwaMissionFile_Load(const char* missionPath, AeronConfigFile** out, char* path, size_t pathSize) {
	AeronConfigError error;
	const AeronConfigNode* root;

	*out = NULL;
	MissionFile_Path(missionPath, path, pathSize);
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, out, &error)) {
		if (error.code == AERON_CONFIG_ERROR_NOT_FOUND) {
			return 0;
		}
		Aeron_LogError("xwa.models", "%s:%d: %s; its selections are ignored", path, error.line,
					   error.message);
		return -1;
	}
	root = AeronConfigFile_Root(*out);
	/* An empty file sets nothing. */
	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP && AeronConfigNode_Type(root) != AERON_CONFIG_NULL) {
		Aeron_LogError("xwa.models", "%s: the file must be a map; its selections are ignored", path);
		AeronConfigFile_Destroy(*out);
		*out = NULL;
		return -1;
	}
	return 1;
}

/* g_missionFlightGroups and FrontendMission.flightGroups hold 192. */
#define MISSION_FILE_FLIGHT_GROUPS 192
#define MISSION_FILE_FLIGHT_GROUP_NAME 20

int XwaMissionFile_FlightGroup(const char* path, const char* key, const AeronConfigNode* entry,
							   const struct XwaFlightGroup* flightGroups, size_t stride, int count) {
	const AeronConfigNode* name = AeronConfigNode_MapGet(entry, "name");
	const AeronConfigNode* craft = AeronConfigNode_MapGet(entry, "craft");
	const XwaFlightGroup* fg;
	char* end = NULL;
	long fgIdx = key != NULL ? strtol(key, &end, 10) : -1;

	if (key == NULL || end == key || *end != '\0' || fgIdx < 0 || fgIdx >= MISSION_FILE_FLIGHT_GROUPS ||
		AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: '%s' must be a flight group number with a map. It is ignored",
					  path, AeronConfigNode_Line(entry), key != NULL ? key : "");
		return -1;
	}
	if (flightGroups == NULL || fgIdx >= count) {
		Aeron_LogWarn("xwa.models", "%s:%d: the mission has no flight group %s. The entry is ignored", path,
					  AeronConfigNode_Line(entry), key);
		return -1;
	}
	fg = (const XwaFlightGroup*)((const unsigned char*)flightGroups + (size_t)fgIdx * stride);
	if (name != NULL) {
		const char* expected = AeronConfigNode_String(name, NULL);
		size_t length = 0;

		while (length < MISSION_FILE_FLIGHT_GROUP_NAME && fg->name[length] != '\0') {
			++length;
		}
		if (expected == NULL || strlen(expected) != length || memcmp(expected, fg->name, length) != 0) {
			Aeron_LogWarn("xwa.models", "%s:%d: flight group %s is '%.*s', not '%s'. The entry is ignored",
						  path, AeronConfigNode_Line(name), key, (int)length, fg->name,
						  expected != NULL ? expected : "");
			return -1;
		}
	}
	if (craft != NULL) {
		const int objectType = fg->craftType < XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT
								   ? g_objectTypeTables.craftTypeToObjectType[fg->craftType]
								   : -1;
		int named;

		if (!XwaNames_Read(&g_xwaObjectTypeNames, path, "craft", craft, &named)) {
			return -1;
		}
		if (named != objectType) {
			Aeron_LogWarn("xwa.models", "%s:%d: flight group %s is a %s, not a %s. The entry is ignored",
						  path, AeronConfigNode_Line(craft), key,
						  objectType >= 0 ? XwaNames_Name(&g_xwaObjectTypeNames, objectType) : "?",
						  AeronConfigNode_String(craft, ""));
			return -1;
		}
	}
	return (int)fgIdx;
}
