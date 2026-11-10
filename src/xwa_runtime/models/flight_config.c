#include "xwa_runtime/models/flight_config.h"

#include "xwa_runtime/models/mod_files.h"

/* Modern port module; no original counterpart. */

static int s_loaded;
static AeronConfigFile* s_file;

const AeronConfigNode* XwaFlightConfig_Root(void) {
	if (!s_loaded) {
		s_loaded = 1;
		XwaModFiles_Load(XWA_FLIGHT_CONFIG_PATH, 1, &s_file);
	}
	return s_file ? AeronConfigFile_Root(s_file) : NULL;
}

void XwaFlightConfig_Shutdown(void) {
	AeronConfigFile_Destroy(s_file);
	s_file = NULL;
	s_loaded = 0;
}
