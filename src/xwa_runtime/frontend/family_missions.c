#include "xwa_runtime/frontend/family_missions.h"

#include "aeron/log.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa_runtime/frontend/frontend_config.h"

/* Modern port module; no original counterpart. */

static int s_loaded;
static int s_enabled = 1;

static void family_missions_load(void) {
	const AeronConfigNode* value;

	if (s_loaded) {
		return;
	}
	s_loaded = 1;
	s_enabled = 1;
	value = AeronConfigNode_MapGet(XwaFrontendConfig_Root(), "family_missions");
	if (value == NULL) {
		return;
	}
	if (AeronConfigNode_Type(value) == AERON_CONFIG_BOOL) {
		s_enabled = AeronConfigNode_Bool(value, 1) != 0;
	} else {
		Aeron_LogError("xwa.frontend", "%s:%d: family_missions must be true or false; it is ignored",
					   XWA_FRONTEND_CONFIG_PATH, AeronConfigNode_Line(value));
	}
}

int XwaFamilyMissions_MissionLoaded(void) {
	family_missions_load();
	return s_enabled ? g_frontendMissionLoaded : 0;
}

void XwaFamilyMissions_Shutdown(void) { s_loaded = 0; }
