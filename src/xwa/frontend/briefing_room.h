#ifndef XWA_FRONTEND_BRIEFING_ROOM_H
#define XWA_FRONTEND_BRIEFING_ROOM_H

#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	BRIEFING_ROOM_OOB_CRAFT_TYPE_COUNT = 512,
};

typedef enum BriefingRoomState {
	BRIEFING_ROOM_WAIT_NARRATION_OR_SKIP = 0,
	BRIEFING_ROOM_PLAY_ROOM_MOVIE = 1,
	BRIEFING_ROOM_START_BRIEFING_REVEAL = 2,
	BRIEFING_ROOM_REVEAL_BRIEFING_TEXT = 3,
	BRIEFING_ROOM_CLOSE_REVEAL_MASK = 4,
	BRIEFING_ROOM_ENTER_MISSION_BRIEFING = 5,
	BRIEFING_ROOM_START_NARRATION = 6,
	BRIEFING_ROOM_START_RANK_ANNOUNCEMENT = 7,
	BRIEFING_ROOM_WAIT_RANK_ANNOUNCEMENT = 8,
	BRIEFING_ROOM_DELAY_OFFICER_FOLLOWUP = 9,
	BRIEFING_ROOM_WAIT_OFFICER_FOLLOWUP = 10,
#ifdef XWA_MODERN
	BRIEFING_ROOM_WAIT_ROOM_MOVIE = 11,
#endif
	BRIEFING_ROOM_EXIT_TO_CONCOURSE = 99,
} BriefingRoomState;

extern int g_oobCraftCounts[BRIEFING_ROOM_OOB_CRAFT_TYPE_COUNT];
extern int g_oobEntryCount;
extern int g_oobMaxWidth;
extern char g_briefingNarrationWav[256];
extern int g_briefingRoomStateFrameCounter;
extern int g_briefingRoomState;
extern int g_briefingRoomRevealCount;
extern int g_unusedBriefingRoomInitState;
extern int g_briefingRoomSquadLogoX;
extern int g_briefingRoomSquadLogoY;

int BriefingRoom_ComputeOrderOfBattle(void);
int BriefingRoom_DrawOrderOfBattle(FrontendRect* rect, int revealCount);
int BriefingRoom_DrawMissionBriefingTextReveal(int revealRadius);
int BriefingRoom_SetupCurrentMissionSession(void);
int BriefingRoom_PrepareTourBriefing(void);
int BriefingRoom_Update(int frameCounter);
int BriefingRoom_Exit(int frameCounter);

#ifdef __cplusplus
}
#endif

#endif
