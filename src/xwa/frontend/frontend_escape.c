#include "xwa/frontend/frontend_escape.h"

#include "xwa/config/game_config.h"
#include "xwa/config/pilot.h"
#ifdef XWA_MODERN
#include "xwa/frontend/concourse.h"
#include "xwa_runtime/config/modern_pilot_profiles_screen.h"
#endif
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/net_transport.h"
#include "xwa/util/debug.h"

static int g_frontendEscapeConfigModalActive;

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5298A0
int Frontend_SavePersistentState(void) {
	Pilot_Save(0);
	Config_Write();
	return 1;
}

// FUNCTION: XWA 0x529330
int Frontend_HandleEscapeQuit(int sessionContext) {
	int mouseX;
	int mouseY;
	int exitConfirmed;
#ifdef XWA_MODERN
	int activePilotChanged;
#endif
	FrontendRect rc;

	FrontendCursor_GetPos(&mouseX, &mouseY);
	(void)mouseX;
	(void)mouseY;

	exitConfirmed = 0;
#ifdef XWA_MODERN
	activePilotChanged = 0;
#endif
	if (g_frontendEscapeConfigModalActive) {
		if (FrontendScreen_GetModalStatus() == FRONTEND_SCREEN_MODAL_INACTIVE ||
			FrontendScreen_GetModalStatus() == FRONTEND_SCREEN_MODAL_DONE) {
			g_frontendEscapeConfigModalActive = 0;
			exitConfirmed = g_configDatapadQuitConfirmed;
#ifdef XWA_MODERN
			activePilotChanged = XwaModernPilotProfilesScreen_TakeActiveChanged();
#endif
		}
	} else if (Keyboard_BufferContains(27)) {
		Keyboard_FlushCharBuffer();
		FrontendDraw_RectAssign(&rc, 0, 0, 639, 479);
		g_configDatapadQuitConfirmed = 0;
		if (FrontendScreen_BeginModal(Config_OptionsDatapadUpdate, &rc)) {
			g_frontendEscapeConfigModalActive = 1;
		}
	}

#ifdef XWA_MODERN
	if (activePilotChanged && !exitConfirmed) {
		FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
		return 0;
	}
#endif
	if (!exitConfirmed) {
		return 0;
	}

	if (sessionContext == 1 || sessionContext == 4) {
		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			if (Net_IsHost()) {
				g_frontendNetPacketScratch.packetType = (sessionContext == 1) ? 0x46u : 0x5cu;
				Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 4);
			} else {
				g_frontendNetPacketScratch.packetType = 0x47u;
				Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 4);
			}
		}
	}

	Config_Write();
	Net_ShutdownDirectPlaySessionForQuit();
	FrontendButton_DisableOverlayText();
	DebugPrintf(NULL);
	return 1;
}
