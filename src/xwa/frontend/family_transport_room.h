#ifndef XWA_FRONTEND_FAMILY_TRANSPORT_ROOM_H
#define XWA_FRONTEND_FAMILY_TRANSPORT_ROOM_H

#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FamilyTransportRoomPendingTransition {
	FAMILY_TRANSPORT_ROOM_TRANSITION_NONE = 0,
	FAMILY_TRANSPORT_ROOM_TRANSITION_CONCOURSE = 1,
	FAMILY_TRANSPORT_ROOM_TRANSITION_HOST_GAME = 2,
	FAMILY_TRANSPORT_ROOM_TRANSITION_COMBAT_SIM_MENU = 3,
	FAMILY_TRANSPORT_ROOM_TRANSITION_BRIEFING_ROOM = 4,
} FamilyTransportRoomPendingTransition;

typedef struct FrontendFamilyAwardTextEntry {
	int awardId;
	char text[1024];
} FrontendFamilyAwardTextEntry;

extern FamilyTransportRoomPendingTransition g_familyTransportRoomPendingTransition;
extern int g_familyTransportRoomLabelColor;
extern int g_frontendFamilyDetailMode;
extern int g_frontendFamilyEmkayVoiceTimer;
extern int g_frontendFamilyPageResetPending;
extern char g_frontendFamilyDetailImagePath[140];
extern char g_frontendFamilyDetailTitle[168];
extern int g_frontendFamilySelectedAwardTextIdx;
extern int g_frontendFamilyAwardTextCount;
extern FrontendFamilyAwardTextEntry* g_frontendFamilyAwardTexts;
extern int g_frontendLeftBarAnimState;
extern int g_frontendLeftBarPanelIndex;
extern int g_frontendRightBarAnimState;
extern int g_frontendRightBarPanelIndex;
extern int g_familyTransportRoomPageOrRevealCounter;
extern FrontendRect g_frontendSidebarButtonRects[10];

int FamilyTransportRoom_Update(int frameCounter);
int FamilyTransportRoom_Exit(int frameCounter);
int FamilyTransportRoom_DrawPilotStatsPage(int statCategory);
int FamilyTransportRoom_DrawCombatSimulatorRecordPage(int statCategory);

int FrontendFamily_LoadAwardTextList(void);
int FrontendFamily_HandleTrophyHotspots(int unused);
int FrontendFamily_PlayEmkayVoiceLine(int allowMissionOrIdleLine);
int FrontendFamily_DrawRoomBackground(void);
int FrontendFamily_DrawEmailMonitor(int frameCounter);
int FrontendFamily_DrawSidebarsAndStatsControls(void);

#ifdef __cplusplus
}
#endif

#endif
