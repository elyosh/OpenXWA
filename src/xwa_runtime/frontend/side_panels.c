#include "xwa_runtime/frontend/side_panels.h"

#include "aeron/log.h"
#include "xwa/frontend/combat_sim_menu.h"
#include "xwa/frontend/family_transport_room.h"
#include "xwa/frontend/film_room.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/mission_briefing.h"
#include "xwa/frontend/mission_debrief.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa_runtime/frontend/frontend_config.h"

#include <string.h>

/* Modern port module; no original counterpart. */

typedef enum SidePanelScreen {
	SIDE_PANELS_SIMULATOR,
	SIDE_PANELS_TOUR_BRIEFING,
	SIDE_PANELS_TOUR_DEBRIEFING,
	SIDE_PANELS_FAMILY_MONITOR,
	SIDE_PANELS_FILM_ROOM,
	SIDE_PANELS_SCREEN_COUNT
} SidePanelScreen;

static const char* const k_keys[SIDE_PANELS_SCREEN_COUNT] = {
	"simulator", "tour_briefing", "tour_debriefing", "family_monitor", "film_room",
};

static int s_loaded;
static int s_hidden[SIDE_PANELS_SCREEN_COUNT];
static int s_anyHidden;

static void side_panels_load(void) {
	const AeronConfigNode* table;
	size_t count;

	if (s_loaded) {
		return;
	}
	s_loaded = 1;
	s_anyHidden = 0;
	memset(s_hidden, 0, sizeof s_hidden);
	table = AeronConfigNode_MapGet(XwaFrontendConfig_Root(), "side_panels");
	if (table == NULL) {
		return;
	}
	if (AeronConfigNode_Type(table) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.frontend", "%s:%d: side_panels must be a table; it is ignored",
					   XWA_FRONTEND_CONFIG_PATH, AeronConfigNode_Line(table));
		return;
	}
	count = AeronConfigNode_MapCount(table);
	for (size_t i = 0; i < count; i++) {
		const char* key = AeronConfigNode_MapKeyAt(table, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(table, i);
		int screen = 0;

		while (screen < SIDE_PANELS_SCREEN_COUNT && strcmp(k_keys[screen], key) != 0) {
			screen++;
		}
		if (screen == SIDE_PANELS_SCREEN_COUNT) {
			Aeron_LogError("xwa.frontend", "%s:%d: side_panels: unknown screen %s; it is ignored",
						   XWA_FRONTEND_CONFIG_PATH, AeronConfigNode_Line(value), key);
			continue;
		}
		if (AeronConfigNode_Type(value) != AERON_CONFIG_BOOL) {
			Aeron_LogError("xwa.frontend", "%s:%d: side_panels.%s must be true or false; it is ignored",
						   XWA_FRONTEND_CONFIG_PATH, AeronConfigNode_Line(value), key);
			continue;
		}
		s_hidden[screen] = !AeronConfigNode_Bool(value, 1);
		s_anyHidden |= s_hidden[screen];
	}
}

/* Returns 1 if the screen at the top of the stack hides the side panels.
   A briefing or debriefing outside the campaign counts as the simulator. */
static int side_panels_hidden_here(void) {
	FrontendScreenUpdateFn update;

	side_panels_load();
	if (!s_anyHidden || g_screenStackTop < 0 || g_screenStackTop >= FRONTEND_SCREEN_MAX_STACK) {
		return 0;
	}
	update = g_screenStates[g_screenStackTop].updateFn;
	if (update == MissionSetup_Update || update == MultiplayerSetup_Update) {
		return s_hidden[SIDE_PANELS_SIMULATOR];
	}
	if (update == MissionBriefing_Update) {
		return s_hidden[g_pilotData.campaignMode == 1 ? SIDE_PANELS_TOUR_BRIEFING : SIDE_PANELS_SIMULATOR];
	}
	if (update == MissionDebrief_Update) {
		return s_hidden[g_pilotData.campaignMode == 1 ? SIDE_PANELS_TOUR_DEBRIEFING : SIDE_PANELS_SIMULATOR];
	}
	if (update == FamilyTransportRoom_Update && g_frontendFamilyDetailMode == 1) {
		return s_hidden[SIDE_PANELS_FAMILY_MONITOR];
	}
	if (update == FilmRoom_Update) {
		return s_hidden[SIDE_PANELS_FILM_ROOM];
	}
	return 0;
}

int XwaSidePanels_HidesImage(const char* name) {
	return name != NULL && (strncmp(name, "leftbar", 7) == 0 || strncmp(name, "rightbar", 8) == 0) &&
		   side_panels_hidden_here();
}

int XwaSidePanels_MutesSound(const char* name) {
	return name != NULL && strcmp(name, "panelarm") == 0 && side_panels_hidden_here();
}

void XwaSidePanels_Shutdown(void) { s_loaded = 0; }
