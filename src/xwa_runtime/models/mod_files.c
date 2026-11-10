#include "xwa_runtime/models/mod_files.h"

#include "aeron/aeron.h"
#include "aeron/log.h"
#include "xwa/assets/file_io.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* Stops the game with `message`. A game that is already stopping only logs
   it, so that one broken file that several modules read shows one
   message. */
static void ModFiles_Fatal(char* message) {
	if (File_HasFatalError()) {
		Aeron_LogError("xwa.models", "%s", message);
		return;
	}
	File_PrintFatalMessageAndExit(message, 1);
}

XwaModFileResult XwaModFiles_Load(const char* path, int allowEmpty, AeronConfigFile** out) {
	AeronConfigError error;
	AeronConfigNodeType type;
	char message[1024];

	*out = NULL;
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, out, &error)) {
		*out = NULL;
		if (error.code == AERON_CONFIG_ERROR_NOT_FOUND) {
			return XWA_MOD_FILE_MISSING;
		}
		if (!error.message[0]) {
			snprintf(message, sizeof message, "The mod's file %s can't be read.", path);
		} else if (error.line > 0) {
			snprintf(message, sizeof message, "The mod's file %s can't be read. Line %d: %s.", path,
					 error.line, error.message);
		} else {
			snprintf(message, sizeof message, "The mod's file %s can't be read: %s.", path, error.message);
		}
		ModFiles_Fatal(message);
		return XWA_MOD_FILE_FAILED;
	}
	type = AeronConfigNode_Type(AeronConfigFile_Root(*out));
	if (type != AERON_CONFIG_MAP && !(allowEmpty && type == AERON_CONFIG_NULL)) {
		AeronConfigFile_Destroy(*out);
		*out = NULL;
		snprintf(message, sizeof message,
				 "The mod's file %s can't be used, because it isn't a map of settings.", path);
		ModFiles_Fatal(message);
		return XWA_MOD_FILE_FAILED;
	}
	return XWA_MOD_FILE_LOADED;
}
