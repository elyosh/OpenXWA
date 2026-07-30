#include "xwa/frontend/frontend_net.h"

#include <string.h>

#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/frontend/net_transport.h"

// GLOBAL: XWA 0x9EAC40
MpRosterEntry g_mpRoster[8];
// GLOBAL: XWA 0x9EB8C0
int g_mpRosterReadyFlags[8];
// GLOBAL: XWA 0x9EAC20
int isHost;
// GLOBAL: XWA 0xABD7CC
int g_localPilotNetworkPlayerIndex;
// GLOBAL: XWA 0x783ED0
int g_frontendChatTeamOnly;
// GLOBAL: XWA 0x783FE0
int g_frontendNetPacketArg0;
// GLOBAL: XWA 0x783FE4
int g_frontendNetPacketArg1;
// GLOBAL: XWA 0x783FDC
int g_frontendNetPacketSenderPlayerId;
// GLOBAL: XWA 0x5AB8C0
FrontendNetGuid g_frontendNetXwaDirectPlayAppGuid;
// GLOBAL: XWA 0x9EB600
FrontendNetPacket g_frontendNetPacketScratch;

// FUNCTION: XWA 0x56DDB0
int FrontendNet_JoinGameScreen(int frameCounter) {
	(void)frameCounter;

	/* TODO: Reimplement FrontendNet_JoinGameScreen @ 0x56DDB0. */
	return 0;
}

// FUNCTION: XWA 0x5718F0
int FrontendNet_HostGameExit(int frameCounter) {
	(void)frameCounter;

	FrontImage_FreeResourceByName("background");
	Frontend_ResetScrollableControls();
	return 0;
}

// FUNCTION: XWA 0x571910
int FrontendNet_HostGameScreen(int frameCounter) {
	(void)frameCounter;

	/* TODO: Reimplement FrontendNet_HostGameScreen @ 0x571910. */
	return 0;
}

// FUNCTION: XWA 0x571090
int FrontendNet_IsTeamLocalPlayer(int team) {
	int slotIndex;

	for (slotIndex = 0; slotIndex < 16; ++slotIndex) {
		if (g_frontendMission->flightGroups[(int16_t)g_combatSimSlots[slotIndex].fgIndex].team == team &&
			g_combatSimSlots[slotIndex].ownerPlayerId != 0) {
			return g_combatSimSlots[slotIndex].ownerPlayerId == Net_GetLocalPlayerId();
		}
	}

	return 0;
}

// FUNCTION: XWA 0x571DE0
int FrontendNet_ProcessNetworkPackets(void) {
	/* TODO: Reimplement FrontendNet_ProcessNetworkPackets @ 0x571DE0. */
	return 0;
}

// FUNCTION: XWA 0x56F2D0
int FrontendNet_UpdateAndDrawPanel(int frameCounter) {
	(void)frameCounter;

	/* TODO: Reimplement FrontendNet_UpdateAndDrawPanel @ 0x56F2D0. */
	return 0;
}

// FUNCTION: XWA 0x57E850
int MpRoster_CompactActiveEntries(void) {
	unsigned int emptyIndex;
	unsigned int activeIndex;

	for (emptyIndex = 0; emptyIndex < 8; ++emptyIndex) {
		if (g_mpRoster[emptyIndex].playerId == 0) {
			for (activeIndex = emptyIndex + 1; activeIndex < 8; ++activeIndex) {
				if (g_mpRoster[activeIndex].playerId != 0) {
					g_mpRoster[emptyIndex] = g_mpRoster[activeIndex];
					memset(&g_mpRoster[activeIndex], 0, sizeof(g_mpRoster[activeIndex]));
					break;
				}
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x57E7F0
int CombatSim_ClearDisconnectedSlotOwners(void) {
	/* TODO: Reimplement CombatSim_ClearDisconnectedSlotOwners @ 0x57E7F0. */
	return 1;
}
