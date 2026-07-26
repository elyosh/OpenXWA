#include "xwa/frontend/frontend_mission_list.h"

#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/briefing_script.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/util/memory.h"

// FUNCTION: XWA 0x56DD60
int FrontendMissionList_FreeScreenResources(int frameCounter) {
	(void)frameCounter;

	if (g_missionList != NULL) {
		Mem_Free(g_missionList);
		g_missionList = NULL;
	}

	if (g_briefingText != NULL) {
		Mem_Free(g_briefingText);
		g_briefingText = NULL;
	}

	Frontend_ResetScrollableControls();
	FrontImage_FreeResourceByName("background");
	return 0;
}
