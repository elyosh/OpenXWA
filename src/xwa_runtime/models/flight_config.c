#include "xwa_runtime/models/flight_config.h"

#include "aeron/aeron.h"
#include "aeron/log.h"

#include <string.h>

/* Modern port module; no original counterpart. */

static int s_loaded;
static AeronConfigFile* s_file;

const AeronConfigNode* XwaFlightConfig_Root(void) {
	AeronConfigError error;

	if (!s_loaded) {
		s_loaded = 1;
		memset(&error, 0, sizeof(error));
		if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, XWA_FLIGHT_CONFIG_PATH, &s_file,
										&error)) {
			s_file = NULL;
			if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
				Aeron_LogError("xwa.models", "%s:%d: %s; the game ignores the file", XWA_FLIGHT_CONFIG_PATH,
							   error.line, error.message[0] ? error.message : "cannot be read");
			}
		}
	}
	return s_file ? AeronConfigFile_Root(s_file) : NULL;
}

void XwaFlightConfig_Shutdown(void) {
	AeronConfigFile_Destroy(s_file);
	s_file = NULL;
	s_loaded = 0;
}
