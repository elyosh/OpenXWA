#ifndef XWA_FRONTEND_MISSION_BRIEFING_H
#define XWA_FRONTEND_MISSION_BRIEFING_H

#include "xwa/frontend/frontend_rect.h"
#include "xwa/frontend/tech_library.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void* g_briefingSelectionImageBuffer;
extern void* g_briefingPreviewImageBuffer;
extern FrontendRect g_briefingMapSourceRect;
extern short g_briefingMapMajorGridColor;
extern short g_briefingMapMinorGridColor;
extern int   clipBottomAdjust;
extern int   Seed;
extern int   g_briefingSelectionRevealComplete;
extern int   g_briefingModelLightDx;
extern int   g_briefingModelLightDy;
extern int   g_briefingModelLightDz;
extern int   g_briefingDsBriefAnimTick;
extern int   g_briefingSelectionStatsRevealDone;
extern int   g_briefingSelectionPreviewBaseY;
extern int   g_briefingSelectionRevealRow;
extern int   g_briefingMapRegionTransitionFrame;
extern int   g_missionBriefingTextViewActive;
extern int   g_missionBriefingPendingExitAction;
extern int   g_missionBriefingLaunchSent;
extern int   g_missionBriefingReadyPlayerCount;
extern int   g_missionBriefingReadyPlayerIds[8];
extern int   g_frontendMissionOpcode99Count;
extern int   g_unusedBriefingSelectionImageLoadState;
extern int   g_missionBriefingLaunchCountdownMs;
extern int   g_missionBriefingTickNowMs;
extern int   g_missionBriefingLastTickMs;
extern int   g_missionBriefingLastCountdownSecondSent;
extern float yawDeg;
extern float rollDeg;
extern float angleDeg;
extern float g_briefingModelDefaultPitchDeg;
extern CraftTechStats stats;

int16_t BriefingMap_MouseInputStub(FrontendRect* viewportRect, FrontendRect* clipRect, int leftDown,
							   int rightDown, int mouseX, int mouseY);
int     MissionBriefing_HandleFlyPacketStub(void);
void    BriefingText_FreeAllocatedBuffers(void);
int16_t BriefingText_FreeAllocatedBuffersExit(void);
void    BriefingMap_ProjectPointToViewport(FrontendRect* viewport, int16_t mapX, int16_t mapY,
											int16_t* outX, int16_t* outY);
int16_t BriefingMap_StepS16TowardTarget(int16_t current, int16_t target, int step);
void    BriefingMap_AnimateViewState(void);
#if !defined(_MSC_VER) || _MSC_VER > 1100
void BriefingMap_UpdatePlaybackAnimation(void);
#else
void BriefingMap_UpdatePlaybackAnimation();
#endif
void    BriefingMap_DrawGrid(FrontendRect* viewportRect);
void    BriefingMap_DrawRevealedLabel(const char* text, int colorRampGroup, int16_t x, int16_t y,
									 int16_t revealCount, int shadeGroup);
void    BriefingMap_DrawRevealedLabelIfActive(const char* text, int colorRampGroup, int16_t x,
											  int16_t y, int revealCount, int shadeGroup);
void    BriefingMap_DrawCraftIconHighlight(FrontendRect* viewportRect, int unused, int16_t briefingIconIndex,
										   int highlightPhase);
void    BriefingMap_DrawOverlays(FrontendRect* viewportRect, int unused);
int     MissionBriefing_PlayNarrationSequenceStart(void);
int     MissionBriefing_PlayTextBlockVoice(int textBlockIndex);
int     MissionBriefing_DrawTextPage(void);
int     BriefingMap_DrawCraftCharacteristicsPanel(FrontendRect* rect, int revealWidth);
int16_t BriefingMap_DrawViewportAndSelection(FrontendRect* viewportRect, FrontendRect* clipRect,
											  int unused);
int     MissionBriefing_DrawMapViewport(FrontendRect* viewportRect, FrontendRect* clipRect,
										int highlightPhase);
int     MissionBriefing_DrawPlaybackControls(void);
int16_t MissionBriefing_HandleMapMouseInput(FrontendRect* viewportRect, FrontendRect* clipRect, int suppressInput,
										int leftDown, int rightDown, int mouseX, int mouseY);
int     MissionBriefing_Update(int frameCounter);
int    MissionBriefing_Exit(int frameCounter);

#ifdef __cplusplus
}
#endif

#endif
