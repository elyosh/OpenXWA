#ifndef XWA_FRONTEND_BRIEFING_SCRIPT_H
#define XWA_FRONTEND_BRIEFING_SCRIPT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	FRONTEND_BRIEFING_SCRIPT_WORD_COUNT = 6400,
	FRONTEND_BRIEFING_MAP_LABEL_COUNT = 128,
	FRONTEND_BRIEFING_TEXT_BLOCK_COUNT = 128,
	FRONTEND_BRIEFING_MAP_LABEL_SIZE = 0x28,
	FRONTEND_BRIEFING_TEXT_BLOCK_SIZE = 0x140,
	FRONTEND_BRIEFING_MAP_ICON_COUNT = 192,
	FRONTEND_BRIEFING_MAP_ICON_STATE_SIZE = 24,
	FRONTEND_BRIEFING_MAP_REGION_COUNT = 4,
	FRONTEND_BRIEFING_SCRIPT_FORMAT_SIZE = 0x320A,
	FRONTEND_BRIEFING_SCRIPT_OLD_SIZE = 0xC8A,
	FRONTEND_BRIEFING_SECONDARY_VERSION_98 = 98,
};

typedef struct FrontendBriefingVector {
	int16_t x;
	int16_t y;
} FrontendBriefingVector;

typedef struct FrontendBriefingScript {
	int16_t durationTicks;
	int16_t currentTime;
	int16_t cursorWordIndex;
	int16_t headerWord06;
	int16_t headerWord08;
	int16_t words[FRONTEND_BRIEFING_SCRIPT_WORD_COUNT];
} FrontendBriefingScript;

typedef struct FrontendBriefingContent {
	FrontendBriefingScript script;
	char* mapLabelTexts[FRONTEND_BRIEFING_MAP_LABEL_COUNT];
	char* textBlocks[FRONTEND_BRIEFING_TEXT_BLOCK_COUNT];
} FrontendBriefingContent;

typedef struct FrontendBriefingMapIconState {
	int8_t objectType;
	uint8_t iconClass;
	int16_t mapX;
	int16_t mapY;
	int16_t drawFlags;
	uint8_t gap08[16];
} FrontendBriefingMapIconState;

typedef char xwa_frontend_briefing_map_icon_state_size
	[(sizeof(FrontendBriefingMapIconState) == FRONTEND_BRIEFING_MAP_ICON_STATE_SIZE) ? 1 : -1];

typedef struct FrontendBriefingFgMarkerState {
	int16_t active[8];
	int16_t iconIdx[8];
	int16_t age[8];
} FrontendBriefingFgMarkerState;

typedef struct FrontendBriefingLabelState {
	int16_t active[8];
	int16_t textIdx[8];
	int16_t x[8];
	int16_t y[8];
	int16_t age[8];
	int16_t style[8];
} FrontendBriefingLabelState;

extern int16_t g_briefingPlaybackActive;
extern int16_t g_unusedBriefingMissionResetWord;
extern int16_t g_briefingMapCurrentRegionIdx;
extern int16_t g_briefingMapRegionTransitionState;
extern int16_t g_briefingCraftStatsRevealEnabled;
extern int16_t g_briefingMapCenterDirty;
extern int16_t g_briefingMapScaleDirty;
extern int16_t g_briefingTextSlotsChanged;
extern int16_t g_briefingMapFgMarkersChanged;
extern int16_t g_briefingMapLabelsChanged;
extern int16_t g_briefingScriptPauseMarkerReached;
extern int16_t g_briefingMapPrimaryHighlightActive;
extern int16_t g_briefingMapPrimaryHighlightIconIndex;
extern int16_t g_briefingMapPrimaryHighlightPhase;
extern int g_briefingTeamIndex;
extern int g_briefingLastNarratedTextBlockIdx;
extern int g_briefingTextPageNumber;

extern FrontendBriefingVector g_briefingMapCenter;
extern FrontendBriefingVector g_briefingMapTargetCenter;
extern FrontendBriefingVector g_briefingMapScale;
extern FrontendBriefingVector g_briefingMapTargetScale;
extern FrontendBriefingContent g_frontendBriefingContent;

extern char* g_briefingTextBlocksEndPadding[20];
extern char* g_briefingText;

extern FrontendBriefingMapIconState g_briefingMapCurrentRegionIcons[FRONTEND_BRIEFING_MAP_ICON_COUNT];
extern FrontendBriefingMapIconState g_briefingMapRegionIconSnapshots[FRONTEND_BRIEFING_MAP_REGION_COUNT]
																	[FRONTEND_BRIEFING_MAP_ICON_COUNT];

extern int16_t g_briefingTextSlotActive[2];
extern int16_t g_briefingTextSlotBlockIdx[2];
extern FrontendBriefingFgMarkerState g_briefingMapFgMarkers;
extern FrontendBriefingLabelState g_briefingMapLabels;

void BriefingScript_InitDefaultScript(void);
void BriefingScript_ResetState(void);
void BriefingScript_AdvanceFrame(int16_t initializeState);
int16_t BriefingScript_AdvanceUntilTime(int16_t targetTime, int16_t initializeState);
void BriefingScript_AdvanceToNextVisibleLine(void);
void BriefingScript_AdvanceOrResetAtEnd(void);

#ifdef __cplusplus
}
#endif

#endif
