#include "xwa/frontend/briefing_script.h"

#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_sound.h"

#include <string.h>

enum {
	BRIEFING_SCRIPT_OPCODE_PAUSE = 1,
	BRIEFING_SCRIPT_OPCODE_CLEAR_TEXT = 3,
	BRIEFING_SCRIPT_OPCODE_TEXT_SLOT_0 = 4,
	BRIEFING_SCRIPT_OPCODE_TEXT_SLOT_1 = 5,
	BRIEFING_SCRIPT_OPCODE_SET_MAP_CENTER = 6,
	BRIEFING_SCRIPT_OPCODE_SET_MAP_SCALE = 7,
	BRIEFING_SCRIPT_OPCODE_CLEAR_FG_MARKERS = 8,
	BRIEFING_SCRIPT_OPCODE_FG_MARKER_FIRST = 9,
	BRIEFING_SCRIPT_OPCODE_FG_MARKER_LAST = 16,
	BRIEFING_SCRIPT_OPCODE_CLEAR_LABELS = 17,
	BRIEFING_SCRIPT_OPCODE_LABEL_FIRST = 18,
	BRIEFING_SCRIPT_OPCODE_LABEL_LAST = 25,
	BRIEFING_SCRIPT_OPCODE_ICON_TYPE_CLASS = 26,
	BRIEFING_SCRIPT_OPCODE_PRIMARY_HIGHLIGHT = 27,
	BRIEFING_SCRIPT_OPCODE_ICON_POSITION = 28,
	BRIEFING_SCRIPT_OPCODE_ICON_FLAGS = 29,
	BRIEFING_SCRIPT_OPCODE_SET_REGION = 30,
	BRIEFING_SCRIPT_OPCODE_CRAFT_STATS_REVEAL = 31,
	BRIEFING_SCRIPT_INITIAL_AGE = 0x50,
	BRIEFING_SCRIPT_MAX_OPCODE_WITH_ARG_COUNTS = 34
};

// GLOBAL: XWA 0x604C30
static const int16_t g_briefingScriptOpcodeArgCounts[BRIEFING_SCRIPT_MAX_OPCODE_WITH_ARG_COUNTS + 1] = {
	0, 0, 1, 0, 1, 1, 2, 2, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 4, 4, 4, 4, 4, 4, 4, 4, 3, 2, 3, 2, 1, 1, 0, 0, 0
};

// GLOBAL: XWA 0x9EB826
int16_t g_briefingPlaybackActive;
// GLOBAL: XWA 0x9EB832
int16_t g_unusedBriefingMissionResetWord;
// GLOBAL: XWA 0x9EB854
int16_t g_briefingMapCurrentRegionIdx;
// GLOBAL: XWA 0x9EB856
int16_t g_briefingMapRegionTransitionState;
// GLOBAL: XWA 0x9EB858
int16_t g_briefingCraftStatsRevealEnabled;
// GLOBAL: XWA 0x9F495E
int16_t g_briefingMapCenterDirty;
// GLOBAL: XWA 0x9F4960
int16_t g_briefingMapScaleDirty;
// GLOBAL: XWA 0x9F496A
int16_t g_briefingTextSlotsChanged;
// GLOBAL: XWA 0x9F499C
int16_t g_briefingMapFgMarkersChanged;
// GLOBAL: XWA 0x9F49FE
int16_t g_briefingMapLabelsChanged;
// GLOBAL: XWA 0x9F4A00
int16_t g_briefingScriptPauseMarkerReached;
// GLOBAL: XWA 0x9F4A02
int16_t g_briefingMapPrimaryHighlightActive;
// GLOBAL: XWA 0x9F4A04
int16_t g_briefingMapPrimaryHighlightIconIndex;
// GLOBAL: XWA 0x9F4A06
int16_t g_briefingMapPrimaryHighlightPhase;
// GLOBAL: XWA 0x9EB86A
int g_briefingTeamIndex;
// GLOBAL: XWA 0x9EB86E
int g_briefingLastNarratedTextBlockIdx;
// GLOBAL: XWA 0x9EB872
int g_briefingTextPageNumber;

// GLOBAL: XWA 0x9EB85A
FrontendBriefingVector g_briefingMapCenter;
// GLOBAL: XWA 0x9EB85E
FrontendBriefingVector g_briefingMapTargetCenter;
// GLOBAL: XWA 0x9EB862
FrontendBriefingVector g_briefingMapScale;
// GLOBAL: XWA 0x9EB866
FrontendBriefingVector g_briefingMapTargetScale;
// GLOBAL: XWA 0x9EB900
FrontendBriefingContent g_frontendBriefingContent;

// GLOBAL: XWA 0x9EEF0A
char* g_briefingTextBlocksEndPadding[20];
// GLOBAL: XWA 0x9F4BD0
char* g_briefingText;

// GLOBAL: XWA 0x9EEF5A
FrontendBriefingMapIconState g_briefingMapCurrentRegionIcons[FRONTEND_BRIEFING_MAP_ICON_COUNT];
// GLOBAL: XWA 0x9F015A
FrontendBriefingMapIconState g_briefingMapRegionIconSnapshots[FRONTEND_BRIEFING_MAP_REGION_COUNT]
															 [FRONTEND_BRIEFING_MAP_ICON_COUNT];

// GLOBAL: XWA 0x9F4962
int16_t g_briefingTextSlotActive[2];
// GLOBAL: XWA 0x9F4966
int16_t g_briefingTextSlotBlockIdx[2];
// GLOBAL: XWA 0x9F496C
FrontendBriefingFgMarkerState g_briefingMapFgMarkers;
// GLOBAL: XWA 0x9F499E
FrontendBriefingLabelState g_briefingMapLabels;

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5673A0
void BriefingScript_AdvanceFrame(int16_t initializeState) {
	int16_t cursorWordIndex;
	int16_t readWordIndex;
	int16_t eventTime;
	int16_t nextWordIndex;
	int16_t args[7];
	char label[FRONTEND_BRIEFING_MAP_LABEL_SIZE];

	readWordIndex = g_frontendBriefingContent.script.cursorWordIndex;
	cursorWordIndex = readWordIndex;

	g_briefingTextSlotsChanged = 0;
	g_briefingMapFgMarkersChanged = 0;
	g_briefingMapLabelsChanged = 0;
	g_briefingMapCenterDirty = 0;
	g_briefingMapScaleDirty = 0;
	g_briefingScriptPauseMarkerReached = 0;

	if (g_frontendBriefingContent.script.words[(int16_t)readWordIndex] <=
		g_frontendBriefingContent.script.currentTime) {
		while (1) {
			int16_t opcode;
			int16_t opcodeIndex;
			int16_t argCount;
			int i;

			eventTime = g_frontendBriefingContent.script.words[(int16_t)readWordIndex];
			opcode = g_frontendBriefingContent.script.words[(int16_t)(readWordIndex + 1)];
			opcodeIndex = opcode;
			argCount = g_briefingScriptOpcodeArgCounts[opcodeIndex];
			nextWordIndex = readWordIndex + 2;

			for (i = 0; i < argCount; ++i) {
				args[i] = g_frontendBriefingContent.script.words[(int16_t)(nextWordIndex + i)];
			}
			nextWordIndex = (int16_t)(nextWordIndex + argCount);

			if (eventTime == g_frontendBriefingContent.script.currentTime) {
				switch (opcodeIndex) {
					case BRIEFING_SCRIPT_OPCODE_PAUSE:
						g_briefingScriptPauseMarkerReached = 1;
						break;

					case BRIEFING_SCRIPT_OPCODE_CLEAR_TEXT:
						memset(g_briefingTextSlotActive, 0, sizeof(g_briefingTextSlotActive));
						g_briefingTextSlotsChanged = 1;
						break;

					case BRIEFING_SCRIPT_OPCODE_TEXT_SLOT_0:
					case BRIEFING_SCRIPT_OPCODE_TEXT_SLOT_1: {
						int16_t slot;

						slot = opcode - BRIEFING_SCRIPT_OPCODE_TEXT_SLOT_0;
						g_briefingTextSlotActive[slot] = 1;
						g_briefingTextSlotBlockIdx[slot] = args[0];
						break;
					}

					case BRIEFING_SCRIPT_OPCODE_SET_MAP_CENTER:
						if (eventTime != 0 && !initializeState) {
							g_briefingMapTargetCenter.x = args[0];
							g_briefingMapTargetCenter.y = args[1];
							g_briefingMapCenterDirty = 1;
						} else {
							g_briefingMapTargetCenter.x = args[0];
							g_briefingMapCenter.x = args[0];
							g_briefingMapTargetCenter.y = args[1];
							g_briefingMapCenter.y = args[1];
							g_briefingMapCenterDirty = 1;
						}
						break;

					case BRIEFING_SCRIPT_OPCODE_SET_MAP_SCALE:
						if (eventTime != 0 && !initializeState) {
							g_briefingMapTargetScale.x = args[0];
							g_briefingMapTargetScale.y = args[1];
							g_briefingMapScaleDirty = 1;
						} else {
							g_briefingMapTargetScale.x = args[0];
							g_briefingMapScale.x = args[0];
							g_briefingMapTargetScale.y = args[1];
							g_briefingMapScale.y = args[1];
							g_briefingMapScaleDirty = 1;
						}
						break;

					case BRIEFING_SCRIPT_OPCODE_CLEAR_FG_MARKERS:
						memset(g_briefingMapFgMarkers.active, 0, sizeof(g_briefingMapFgMarkers.active));
						g_briefingMapFgMarkersChanged = 1;
						break;

					case BRIEFING_SCRIPT_OPCODE_FG_MARKER_FIRST:
					case 10:
					case 11:
					case 12:
					case 13:
					case 14:
					case 15:
					case BRIEFING_SCRIPT_OPCODE_FG_MARKER_LAST: {
						int16_t slot;

						if (!initializeState) {
							int16_t iff;

							iff = g_frontendMission->flightGroups[args[0]].iff;
							if (iff > 2) {
								iff = 2;
							}
							if (iff == 1) {
								if (g_gameConfig.sfxDatapadEnabled) {
									FrontendSound_PlayUISound("sfxTarget2", 1, 0, 127,
															  12 * g_gameConfig.sfxDatapadVolume, 63);
								}
							} else if (g_gameConfig.sfxDatapadEnabled) {
								FrontendSound_PlayUISound("sfxTarget1", 1, 0, 127,
														  12 * g_gameConfig.sfxDatapadVolume, 63);
							}
						}

						slot = opcode - BRIEFING_SCRIPT_OPCODE_FG_MARKER_FIRST;
						g_briefingMapFgMarkers.active[slot] = 1;
						g_briefingMapFgMarkers.age[slot] = initializeState ? BRIEFING_SCRIPT_INITIAL_AGE : 0;
						g_briefingMapFgMarkers.iconIdx[slot] = args[0];
						break;
					}

					case BRIEFING_SCRIPT_OPCODE_CLEAR_LABELS:
						memset(g_briefingMapLabels.active, 0, sizeof(g_briefingMapLabels.active));
						g_briefingMapLabelsChanged = 1;
						break;

					case BRIEFING_SCRIPT_OPCODE_LABEL_FIRST:
					case 19:
					case 20:
					case 21:
					case 22:
					case 23:
					case 24:
					case BRIEFING_SCRIPT_OPCODE_LABEL_LAST: {
						int16_t slot;

						if (!initializeState) {
							strcpy(label, g_frontendBriefingContent.mapLabelTexts[args[0]]);
							if ((int16_t)strlen(label) != 0) {
								if (g_gameConfig.sfxDatapadEnabled) {
									FrontendSound_PlayUISound("sfxText", 1, 0, 127,
															  12 * g_gameConfig.sfxDatapadVolume, 63);
								}
							}
						}

						slot = opcode - BRIEFING_SCRIPT_OPCODE_LABEL_FIRST;
						g_briefingMapLabels.active[slot] = 1;
						g_briefingMapLabels.age[slot] = initializeState ? BRIEFING_SCRIPT_INITIAL_AGE : 0;
						g_briefingMapLabels.textIdx[slot] = args[0];
						g_briefingMapLabels.x[slot] = args[1];
						g_briefingMapLabels.y[slot] = args[2];
						g_briefingMapLabels.style[slot] = args[3];
						break;
					}

					case BRIEFING_SCRIPT_OPCODE_ICON_TYPE_CLASS: {
						int16_t iconIndex;

						iconIndex = args[0];
						g_briefingMapCurrentRegionIcons[iconIndex].objectType = (int8_t)args[1];
						g_briefingMapCurrentRegionIcons[iconIndex].iconClass = (uint8_t)args[2];
						break;
					}

					case BRIEFING_SCRIPT_OPCODE_ICON_POSITION: {
						int16_t iconIndex;

						iconIndex = args[0];
						g_briefingMapCurrentRegionIcons[iconIndex].mapX = args[1];
						g_briefingMapCurrentRegionIcons[iconIndex].mapY = args[2];
						break;
					}

					case BRIEFING_SCRIPT_OPCODE_ICON_FLAGS: {
						int16_t iconIndex;

						iconIndex = args[0];
						g_briefingMapCurrentRegionIcons[iconIndex].drawFlags = args[1];
						break;
					}

					case BRIEFING_SCRIPT_OPCODE_PRIMARY_HIGHLIGHT:
						g_briefingMapPrimaryHighlightActive = args[0];
						if (args[0] == 1) {
							g_briefingMapPrimaryHighlightIconIndex = args[1];
							g_briefingMapPrimaryHighlightPhase = 0;
						}
						break;

					case BRIEFING_SCRIPT_OPCODE_SET_REGION:
						if (eventTime == 0) {
							g_briefingMapCurrentRegionIdx = args[0];
							g_briefingMapRegionTransitionState = 0;
						} else {
							if (initializeState) {
								int count;
								int16_t* active;

								memcpy(g_briefingMapRegionIconSnapshots[g_briefingMapCurrentRegionIdx],
									   g_briefingMapCurrentRegionIcons,
									   sizeof(g_briefingMapCurrentRegionIcons));
								g_briefingMapRegionTransitionState = 0;
								active = g_briefingMapLabels.active;
								count = 8;
								do {
									if (*active) {
										*active = 0;
									}
									++active;
									--count;
								} while (count);
								g_briefingMapLabelsChanged = 1;
								active = g_briefingMapFgMarkers.active;
								count = 8;
								do {
									if (*active) {
										*active = 0;
									}
									++active;
									--count;
								} while (count);
								g_briefingMapFgMarkersChanged = 1;
								memcpy(g_briefingMapCurrentRegionIcons,
									   g_briefingMapRegionIconSnapshots[args[0]],
									   sizeof(g_briefingMapCurrentRegionIcons));
								g_briefingMapCurrentRegionIdx = args[0];
								g_briefingMapPrimaryHighlightPhase = 0;
								g_briefingMapPrimaryHighlightActive = 0;
							} else {
								memcpy(&g_briefingMapRegionIconSnapshots[g_briefingMapCurrentRegionIdx][0],
									   g_briefingMapCurrentRegionIcons,
									   sizeof(g_briefingMapRegionIconSnapshots[0]));
								g_briefingMapCurrentRegionIdx = args[0];
								g_briefingMapPrimaryHighlightPhase = 0;
								g_briefingMapPrimaryHighlightActive = 0;
								g_briefingMapRegionTransitionState = 1;
							}
						}
						break;

					case BRIEFING_SCRIPT_OPCODE_CRAFT_STATS_REVEAL:
						g_briefingCraftStatsRevealEnabled = args[0];
						break;

					default:
						break;
				}
			}

			if (eventTime > g_frontendBriefingContent.script.currentTime) {
				cursorWordIndex = readWordIndex;
				break;
			}
			readWordIndex = nextWordIndex;
		}
	}

	++g_frontendBriefingContent.script.currentTime;
	g_frontendBriefingContent.script.cursorWordIndex = (int16_t)cursorWordIndex;
}

// FUNCTION: XWA 0x567170
void BriefingScript_ResetState(void) {
	memset(&g_briefingMapLabels.active[0], 0, sizeof(g_briefingMapLabels.active[0]) * 2);
	g_briefingMapScale.x = (int16_t)(32 + g_briefingMapLabels.active[0]);
	g_briefingMapScale.y = 32;
	g_briefingMapTargetScale.x = 32;
	g_briefingMapTargetScale.y = 32;
	memset(&g_briefingMapLabels.active[2], 0, sizeof(g_briefingMapLabels.active[0]) * 2);
	memset(&g_briefingMapFgMarkers.active[0], 0, sizeof(g_briefingMapFgMarkers.active[0]) * 2);
	memset(&g_briefingMapLabels.active[4], 0, sizeof(g_briefingMapLabels.active[0]) * 2);
	memset(&g_briefingMapFgMarkers.active[2], 0, sizeof(g_briefingMapFgMarkers.active[0]) * 2);
	memset(&g_briefingMapLabels.active[6], 0, sizeof(g_briefingMapLabels.active[0]) * 2);
	memset(&g_briefingMapFgMarkers.active[4], 0, sizeof(g_briefingMapFgMarkers.active[0]) * 2);
	g_briefingMapPrimaryHighlightActive = 0;
	g_briefingMapPrimaryHighlightIconIndex = 0;
	g_briefingMapPrimaryHighlightPhase = 0;
	memset(g_briefingTextSlotActive, 0, sizeof(g_briefingTextSlotActive));
	memset(&g_briefingMapFgMarkers.active[6], 0, sizeof(g_briefingMapFgMarkers.active[0]) * 2);
	memset(g_briefingMapCurrentRegionIcons, 0, sizeof(g_briefingMapCurrentRegionIcons));
	memset(g_briefingMapRegionIconSnapshots, 0, sizeof(g_briefingMapRegionIconSnapshots));
	g_briefingMapCenter.x = 0;
	g_briefingMapCenter.y = 0;
	g_briefingMapTargetCenter.x = 0;
	g_briefingMapTargetCenter.y = 0;
	g_briefingMapCurrentRegionIdx = 0;
	g_briefingMapRegionTransitionState = 0;
	g_briefingCraftStatsRevealEnabled = 0;
	g_frontendBriefingContent.script.currentTime = 0;
	g_frontendBriefingContent.script.cursorWordIndex = 0;
	BriefingScript_AdvanceFrame(1);
}

// FUNCTION: XWA 0x567240
int16_t BriefingScript_AdvanceUntilTime(int16_t targetTime, int16_t initializeState) {
	if (targetTime != g_frontendBriefingContent.script.currentTime - 1) {
		if (g_frontendBriefingContent.script.currentTime > targetTime) {
			BriefingScript_ResetState();
		}
		if (g_frontendBriefingContent.script.currentTime <= targetTime) {
			do {
				BriefingScript_AdvanceFrame(initializeState);
			} while (g_frontendBriefingContent.script.currentTime <= targetTime);
		}
		return 1;
	}
	return 0;
}

// FUNCTION: XWA 0x567290
void BriefingScript_AdvanceToNextVisibleLine(void) {
	int16_t currentTime;
	int16_t found;
	int16_t anyTextSlotActive;
	int16_t visibleLineCount;
	int16_t opcode;
	int16_t targetTime;
	int16_t targetOpcode;

	currentTime = g_frontendBriefingContent.script.currentTime;
	found = 0;
	anyTextSlotActive = 0;
	visibleLineCount = 0;
	BriefingScript_ResetState();
	opcode = 0;
	while (1) {
		int16_t* textSlotActive;
		int i;

		if (opcode == BRIEFING_SCRIPT_MAX_OPCODE_WITH_ARG_COUNTS) {
			break;
		}
		opcode = g_frontendBriefingContent.script.words[g_frontendBriefingContent.script.cursorWordIndex + 1];
		if (g_briefingTextSlotsChanged) {
			visibleLineCount = 0;
			anyTextSlotActive = 0;
		}

		textSlotActive = g_briefingTextSlotActive;
		for (i = 2; i != 0; --i) {
			if (*textSlotActive) {
				anyTextSlotActive = 1;
			}
			++textSlotActive;
		}
		if (anyTextSlotActive) {
			++visibleLineCount;
		}
		if ((g_briefingScriptPauseMarkerReached || visibleLineCount == 1) &&
			g_frontendBriefingContent.script.currentTime >= currentTime) {
			found = 1;
		} else {
			BriefingScript_AdvanceFrame(1);
		}
		if (found) {
			break;
		}
	}

	if (g_briefingScriptPauseMarkerReached || visibleLineCount == 1) {
		targetTime = g_frontendBriefingContent.script.currentTime;
		targetOpcode = 0;
	} else {
		int16_t cursorWordIndex;

		cursorWordIndex = g_frontendBriefingContent.script.cursorWordIndex;
		targetTime = g_frontendBriefingContent.script.words[cursorWordIndex];
		targetOpcode = g_frontendBriefingContent.script.words[cursorWordIndex + 1];
	}
	if (targetOpcode == BRIEFING_SCRIPT_MAX_OPCODE_WITH_ARG_COUNTS) {
		g_briefingLastNarratedTextBlockIdx = 0xffff;
		g_briefingTextPageNumber = 0;
		BriefingScript_ResetState();
		return;
	}
	BriefingScript_AdvanceUntilTime(targetTime, 0);
}

// FUNCTION: XWA 0x567B90
void BriefingScript_AdvanceOrResetAtEnd(void) {
	if (g_frontendBriefingContent.script.currentTime < g_frontendBriefingContent.script.durationTicks) {
		BriefingScript_AdvanceFrame(0);
		return;
	}
	g_briefingLastNarratedTextBlockIdx = 0xffff;
	g_briefingTextPageNumber = 0;
	BriefingScript_ResetState();
}

// FUNCTION: XWA 0x567130
void BriefingScript_InitDefaultScript(void) {
	g_frontendBriefingContent.script.durationTicks = 200;
	g_frontendBriefingContent.script.currentTime = 0;
	g_frontendBriefingContent.script.cursorWordIndex = 0;
	g_frontendBriefingContent.script.headerWord06 = 2;
	g_frontendBriefingContent.script.headerWord08 = 0;
	g_frontendBriefingContent.script.words[0] = 9999;
	g_frontendBriefingContent.script.words[1] = 34;
	BriefingScript_ResetState();
}
