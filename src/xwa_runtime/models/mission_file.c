#include "xwa_runtime/models/mission_file.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

static char MissionFile_Lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

/* remaster/missions/<mission path without its extension>.yaml, lower case,
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
	offset = (size_t)snprintf(path, pathSize, "remaster/missions/");
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
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_ASSET, path, out, &error)) {
		if (error.code == AERON_CONFIG_ERROR_NOT_FOUND) {
			return 0;
		}
		Aeron_LogError("xwa.models", "%s:%d: %s; its selections are ignored", path, error.line,
					   error.message);
		return -1;
	}
	root = AeronConfigFile_Root(*out);
	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP ||
		AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1) {
		Aeron_LogError("xwa.models", "%s: expected 'version: 1'; its selections are ignored", path);
		AeronConfigFile_Destroy(*out);
		*out = NULL;
		return -1;
	}
	return 1;
}
