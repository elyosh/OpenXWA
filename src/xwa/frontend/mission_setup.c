#include "xwa/frontend/mission_setup.h"

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/linez.h"
#include "xwa/assets/ship_list.h"
#include "xwa/assets/sprite_resource.h"
#include "xwa/assets/string_table.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/briefing_script.h"
#include "xwa/frontend/concourse.h"
#include "xwa/frontend/family_transport_room.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_dialog.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_escape.h"
#include "xwa/frontend/frontend_file_list.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/frontend_mission_list.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/mission_briefing.h"
#include "xwa/frontend/skirmish.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/movie/movie.h"
#include "xwa/net/net.h"
#include "xwa/util/byte_order.h"
#include "xwa/util/memory.h"
#include "xwa/xwa_options.h"
#include "xwa/util/time.h"

#include <ctype.h>
#ifndef XWA_MODERN
#include <direct.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	MISSION_LIST_ENTRY_SIZE = 0x148,
	MISSION_LIST_LINE_SIZE = 256,
	MISSION_LIST_SECTION_NAME_SIZE = 128,
	MISSION_TEXT_BUFFER_SIZE = 4096,
	MISSION_LEGACY_TEXT_BUFFER_SIZE = 1024,
	SKIRMISH_FILE_FORMAT_MELEE_FLAG_VERSION = 6
};

// GLOBAL: XWA 0x9F4B98
MissionListEntry* g_missionList = NULL;
// GLOBAL: XWA 0x9F5EC0
int g_missionCount;
// GLOBAL: XWA 0x9F5E74
int g_selectedMissionListIndex;
// GLOBAL: XWA 0x7838A0
int g_missionSetupShipBmpGroupId;
// GLOBAL: XWA 0xAE2A38
int g_missionSetupRosterAuthoritative;
// GLOBAL: XWA 0x9F6084
int g_missionSetupDraggedPlayerId;
// GLOBAL: XWA 0x783194
MissionSetupSlotSummaryMode g_missionSetupSlotSummaryMode;
// GLOBAL: XWA 0x9F4BC8
MissionSetupActivePanel g_missionSetupActivePanel;
// GLOBAL: XWA 0x7830C4
int g_missionSetupBattleFirstMissionIdx;
// GLOBAL: XWA 0x7830CC
int g_missionSetupBattleLastMissionIdx;
// GLOBAL: XWA 0x7830E4
int g_missionSetupBattleSelectableMissionCount;
// GLOBAL: XWA 0x783174
int g_missionSetupBattleSelectedMissionOrdinal;
// GLOBAL: XWA 0x7830E8
int g_missionSetupMissionListScrollOffset;
// GLOBAL: XWA 0x7830BC
int g_missionSetupMissionListRowCount;
// GLOBAL: XWA 0x7830C0
int g_missionSetupSelectedMissionListIndex;
// GLOBAL: XWA 0x7830DC
int g_missionSetupSubpanelMode;
// GLOBAL: XWA 0x7830D4
int g_missionSetupPendingRandomSeed;
// GLOBAL: XWA 0x78318C
int g_skirmishFileRandomSeed;
// GLOBAL: XWA 0x7831A8
int g_missionSetupUseCombatSimPilotState;
// GLOBAL: XWA 0x7831A0
int g_missionSetupSkirmishSeedInitialized;
// GLOBAL: XWA 0x7830B4
int frame;
// GLOBAL: XWA 0x9F60D0
int g_teamCount;
// GLOBAL: XWA 0x9F4BA0
int g_teamFgCountScratch[10];
// GLOBAL: XWA 0x7830E0
int g_missionSetupConnectionStatsHoverActive;
// GLOBAL: XWA 0x9F6080
int g_missionSetupPlayerDragState;
// GLOBAL: XWA 0x9F60C8
int g_missionSetupRemoteTeamGoalType;
// GLOBAL: XWA 0x9F6040
char g_missionSetupSaveNameBuffer[64];
// GLOBAL: XWA 0x9F4B60
char g_selectedCombatSimSlotNameEditBuffer[32];
// GLOBAL: XWA 0x78317C
int g_missionSetupCarouselSlideOffset;
// GLOBAL: XWA 0x783184
int g_missionSetupCarouselQueuedSlideOffset;
// GLOBAL: XWA 0x9F4BCC
int g_missionSetupCraftSelectionChangedFlag;
// GLOBAL: XWA 0x78319C
int g_missionSetupCraftListScrollOffset;
// GLOBAL: XWA 0x783178
int g_missionSetupDisplayedCraftOrdinal;
// GLOBAL: XWA 0x7830D0
int g_missionSetupFilteredCraftCount;
// GLOBAL: XWA 0x7831A4
int g_missionSetupSelectedCraftCategory;
// GLOBAL: XWA 0x7830D8
int g_missionSetupTourButtonEnabled;
// GLOBAL: XWA 0x7830F0
char g_missionSetupCurrentBattleSectionName[128];
// GLOBAL: XWA 0x7830B0
MissionSetupTransition g_missionSetupPendingTransition;
// GLOBAL: XWA 0x783180
int g_unusedMissionSetupCarouselPanelColor;
// GLOBAL: XWA 0x9F6088
int g_missionSetupJoinBroadcastCooldownFrames;
// GLOBAL: XWA 0x9EB5E8
int g_frontendMissionInitClearedDword;
// GLOBAL: XWA 0x7830B8
int g_unusedMissionSetupInitDword7830B8;
// GLOBAL: XWA 0x783170
int g_unusedMissionSetupMissionListIndexLatch;
// GLOBAL: XWA 0x7830EC
uint32_t g_missionSetupLastHostBroadcastTick;
// GLOBAL: XWA 0x7830C8
int g_missionSetupParsedMissionNumber;
// GLOBAL: XWA 0x9F5EE0
CombatSimSlot g_combatSimSlots[16];
// GLOBAL: XWA 0x9F4BE0
CombatSimLoadoutOptions g_combatSimLoadoutOptions[16];
// GLOBAL: XWA 0x9F5D20
char g_combatSimSlotNames[16][20];
// GLOBAL: XWA 0x9F5E60
CombatSimSlot g_missionSetupSlotEditBackup;
// GLOBAL: XWA 0x783198
int g_missionSetupSlotEditBackupValid;
// GLOBAL: XWA 0x9F4B80
CombatSimSlot g_selectedCombatSimSlot;
// GLOBAL: XWA 0x783188
int g_selectedCombatSimSlotIdx;
// GLOBAL: XWA 0x9F5E80
char g_combatSimSkirmishFileName[64];
// GLOBAL: XWA 0x9EAA18
CraftTechStats* g_cachedCraftTechStats;
// GLOBAL: XWA 0x9EAA14
int g_cachedCraftTechStatsCount;
// GLOBAL: XWA 0x603168
const char* g_campaignDirNames[6] = { "missions",           "melee",    "combat",
									  "ALLIANCE\\SKIRMISH", "missions", "missions" };

static void MissionSetup_TrimTrailingLineBreak(char* text) {
	size_t length;

	length = strlen(text);
	if (length != 0 && text[length - 1] == '\n') {
		text[length - 1] = '\0';
	}
}

static void MissionSetup_CopyResolvedLine(char* dst, const char* src) {
	strcpy(dst, Linez_ResolveString((char*)src));
	MissionSetup_TrimTrailingLineBreak(dst);
}

static void MissionSetup_ConvertBracketControlCodes(char* text) {
	int pos;
	int closePos;

	pos = 1;
	do {
		if (text[pos - 1] == '[') {
			text[pos - 1] = 6;
			closePos = pos;
			if (pos < MISSION_TEXT_BUFFER_SIZE) {
				while (text[closePos] != ']') {
					if (++closePos >= MISSION_TEXT_BUFFER_SIZE) {
						goto next_char;
					}
				}

				text[closePos] = closePos + 1 < MISSION_TEXT_BUFFER_SIZE ? text[closePos + 1] : '\0';
				if (closePos + 1 < MISSION_TEXT_BUFFER_SIZE) {
					text[closePos + 1] = 1;
				}
			}
		}

	next_char:
		++pos;
	} while (pos - 1 < MISSION_TEXT_BUFFER_SIZE);
}

static __inline int MissionSetup_FindRosterIndexByPlayerId(int playerId) {
	int rosterIdx;

	for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
		if (g_mpRoster[rosterIdx].playerId == playerId && playerId) {
			break;
		}
	}

	return rosterIdx;
}

static __inline int MissionSetup_IsCraftRoleValidForCategory(const int* category, uint8_t* craftRole) {
	int isValid;

	isValid = 1;
	switch (*category) {
		case 1:
		case 2:
			break;

		case 3:
			if (*craftRole && *craftRole != 6 && *craftRole != 5) {
				isValid = 0;
			}
			break;

		case 4:
		case 10:
			*craftRole = 0;
			g_selectedCombatSimSlot.craftRole = 0;
			break;

		case 5:
			if (*craftRole > 4) {
				isValid = 0;
			}
			break;

		case 6:
			if (*craftRole > 3) {
				isValid = 0;
			}
			break;

		case 7:
		case 8:
		case 9:
			if (*craftRole > 2) {
				isValid = 0;
			}
			break;

		default:
			break;
	}

	return isValid;
}

static __inline void MissionSetup_ClampSelectedSlotForCraftCategory(CombatSimLoadoutOptions* loadoutOptions,
																	CraftTechStats* stats) {
	const int* category;

	if (g_selectedCombatSimSlot.craftType != 0) {
		memset(stats, 0, sizeof(*stats));
		stats->craftType = g_selectedCombatSimSlot.craftType;
		MissionSetup_GetCachedCraftTechStats(stats);
		if (stats->warheadRating == 0) {
			g_selectedCombatSimSlot.warhead = 0;
			loadoutOptions->selectedWarheadOption = 0;
		}
	}

	category = &g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].category;
	if (*category == 8) {
		g_selectedCombatSimSlot.numberOfWaves = 0;
	}

	if (*category == 6 || *category == 7) {
		g_selectedCombatSimSlot.numberOfCraft = 1;
		g_selectedCombatSimSlot.numberOfWaves = 0;
	}

	if (*category != 1 && *category != 2) {
		if (*category != 9 && *category != 8) {
			g_selectedCombatSimSlot.primaryFg = 1;
		}

		g_selectedCombatSimSlot.beam = 0;
		g_selectedCombatSimSlot.countermeasures = 0;
		loadoutOptions->selectedBeamOption = 0;
		loadoutOptions->selectedCountermeasureOption = 0;
	}

	switch (g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].category) {
		case 1:
		case 2:
			break;

		case 3:
			if (g_selectedCombatSimSlot.craftRole && g_selectedCombatSimSlot.craftRole != 6 &&
				g_selectedCombatSimSlot.craftRole != 5) {
				g_selectedCombatSimSlot.craftRole = 0;
			}
			break;

		case 4:
		case 10:
			g_selectedCombatSimSlot.craftRole = 0;
			break;

		case 5:
		case 6:
			if (g_selectedCombatSimSlot.craftRole > 4u) {
				g_selectedCombatSimSlot.craftRole = 1;
			}
			break;

		case 7:
		case 8:
		case 9:
			if (g_selectedCombatSimSlot.craftRole > 2u) {
				g_selectedCombatSimSlot.craftRole = 1;
			}
			break;

		default:
			break;
	}
}

static __inline int MissionSetup_ComputeSelectedSlotPointTotalForCraft(int craftType) {
	int pointTotal;

	pointTotal = g_selectedCombatSimSlot.numberOfCraft *
				 MissionSetup_ComputeCraftPointTotal(
					 craftType, g_selectedCombatSimSlot.warhead, g_selectedCombatSimSlot.beam,
					 g_selectedCombatSimSlot.countermeasures, g_selectedCombatSimSlot.groupAI,
					 g_selectedCombatSimSlot.craftRole);
	if (g_gameConfig.goalType == 1 && g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		if (!g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].flyable) {
			pointTotal *= g_selectedCombatSimSlot.numberOfWaves + 1;
		}
	} else {
		pointTotal *= g_selectedCombatSimSlot.numberOfWaves + 1;
	}

	return pointTotal;
}

static __inline int MissionSetup_DrawSelectedSlotCraftList(int allFlyable,
														   CombatSimLoadoutOptions* loadoutOptions,
														   CraftTechStats* stats, FrontendRect* rect) {
	int mouseX;
	int mouseY;
	int scanCount;
	int shipIdx;
	int selectable;
	int buttonResult;

	if (allFlyable) {
		FrontImage_DrawSpriteTranslucent("leftframe", 70, 86);
		FrontendText_Draw(10, FrontendString_Get(STR_GAME_ALL_FLYABLE), 70, 90, g_colorGreen);
	}

	FrontendCursor_GetPos(&mouseX, &mouseY);
	(void)mouseX;
	(void)mouseY;
	if ((unsigned int)g_missionSetupFilteredCraftCount > 16u) {
		FrontendDraw_RectAssign(rect, 551, 90, 570, 330);
		g_missionSetupCraftListScrollOffset =
			FrontendScrollbar_Draw(rect, g_missionSetupCraftListScrollOffset,
								   g_missionSetupFilteredCraftCount - 15, 0, 5, g_colorNavy, 10);
	}

	FrontendDraw_RectAssign(rect, 280, 90, 550, 104);
	scanCount = 0;
	g_missionSetupDisplayedCraftOrdinal = 0;
	for (shipIdx = 0; shipIdx < g_shipCount; ++shipIdx) {
		selectable = 0;
		if (g_selectedCombatSimSlot.ownerPlayerId || g_selectedCombatSimSlot.gunnerPlayerId) {
			if (g_shipList[shipIdx].flyable && g_shipList[shipIdx].skirmish) {
				selectable = 1;
			}
		} else if (g_shipList[shipIdx].category == g_missionSetupSelectedCraftCategory &&
				   g_shipList[shipIdx].skirmish) {
			selectable = 1;
		}

		if (!selectable) {
			continue;
		}

		if (scanCount >= g_missionSetupCraftListScrollOffset) {
			if (GetCraftTypeModelLongName(g_shipList[shipIdx].typeId) != NULL) {
				sprintf(g_frontendScratchBuffer, "%s (%s)", g_shipList[shipIdx].name,
						GetCraftTypeModelLongName(g_shipList[shipIdx].typeId));
			} else {
				sprintf(g_frontendScratchBuffer, "%s", g_shipList[shipIdx].name);
			}

			buttonResult =
				FrontendButton_DrawMenuButton(rect->left, rect->top + 3, g_frontendScratchBuffer, 10,
											  g_colorPaleBlue, shipIdx + 80, 0, "settingsound");
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				sprintf(g_frontendScratchBuffer, "%d",
						MissionSetup_ComputeSelectedSlotPointTotalForCraft(g_shipList[shipIdx].typeId));
				FrontendText_Draw(10, g_frontendScratchBuffer, rect->left + 230, rect->top + 3,
								  g_colorLightBlue);
			}

			if (buttonResult) {
				g_selectedCombatSimSlot.craftType = (uint16_t)g_shipList[shipIdx].typeId;
				MissionSetup_ClampSelectedSlotForCraftCategory(loadoutOptions, stats);
				g_missionSetupSubpanelMode = 0;
				if (g_selectedCombatSimSlot.craftType) {
					MissionSetup_LoadShipBmpForCraftType(g_selectedCombatSimSlot.craftType);
				} else {
					MissionSetup_UnloadShipBmp();
				}

				MissionSetup_DrawBackgroundAndPreview(0);
			}

			FrontendDraw_RectOffsetXY(rect, 0, 15);
			++g_missionSetupDisplayedCraftOrdinal;
			if ((unsigned int)g_missionSetupDisplayedCraftOrdinal >=
					(unsigned int)g_missionSetupFilteredCraftCount ||
				(unsigned int)g_missionSetupDisplayedCraftOrdinal >= 16u) {
				break;
			}
		}

		++scanCount;
	}

	return 1;
}

static __inline void MissionSetup_SelectMissionListEntryForMission(MissionListEntry* missionList) {
	unsigned int missionListIdx;
	int missionIdx;

	missionListIdx = 0;
	g_selectedMissionListIndex = 0;
	if ((unsigned int)g_missionCount > 0) {
		missionIdx = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
		do {
			if (missionList[missionListIdx].missionIdx == missionIdx) {
				break;
			}

			g_selectedMissionListIndex = (int)++missionListIdx;
		} while (missionListIdx < (unsigned int)g_missionCount);
	}
}

static __inline void MissionSetup_RefreshSelectedMissionListIndex(void) {
	if (g_missionList == NULL) {
		return;
	}

	MissionSetup_SelectMissionListEntryForMission(g_missionList);
}

static __inline int MissionSetup_DrawGameSettingsActionButton(FrontendRect* rect, FrontendRect* clippedRect,
															  int x, const char* spriteUp,
															  const char* spriteDown, UIString labelId,
															  int buttonId, int mouseX, int mouseY) {
	int color;
	int result;

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 425);
	color = g_colorPaleBlue;
	FrontendDraw_RectCopy(clippedRect, rect);
	FrontendDraw_RectClipToBounds(clippedRect);
	if (FrontendDraw_PointInRect(clippedRect, mouseX, mouseY)) {
		color = g_colorYellow;
	}

	result = FrontendButton_DrawSpriteHitTest(rect, spriteUp, spriteDown, NULL, 10, g_colorLightBlue,
											  buttonId, "settingsound");
	FrontendDraw_RectAssign(rect, x, 351, x + 99, 363);
	FrontendText_DrawCentered(10, FrontendString_Get(labelId), rect, color);
	return result;
}

static __inline int MissionSetup_DrawGameSettingsTileByteValue(FrontendRect* rect, FrontendRect* clippedRect,
															   int x, const char* spriteName,
															   const char* disabledSpriteName,
															   const uint8_t* value, UIString labelId,
															   UIString valueBaseLabelId, int buttonId,
															   int enabled, int mouseX, int mouseY) {
	int color;
	int result;
	const char* drawSpriteName;

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 425);
	result = 0;
	if (enabled) {
		color = g_colorPaleBlue;
		FrontendDraw_RectCopy(clippedRect, rect);
		FrontendDraw_RectClipToBounds(clippedRect);
		if (FrontendDraw_PointInRect(clippedRect, mouseX, mouseY)) {
			color = g_colorYellow;
		}

		FrontImage_SetSpriteFrame(spriteName, *value);
		result = FrontendButton_DrawSpriteHitTest(rect, spriteName, spriteName, NULL, 10, g_colorLightBlue,
												  buttonId, "settingsound");
	} else {
		color = g_colorLightBlue;
		drawSpriteName = disabledSpriteName != NULL ? disabledSpriteName : spriteName;
		FrontImage_SetSpriteFrame(drawSpriteName, *value);
		FrontImage_DrawSprite(drawSpriteName, x, 351);
	}

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 363);
	FrontendText_DrawCentered(10, FrontendString_Get(labelId), rect, color);
	FrontendDraw_RectAssign(rect, x, 413, x + 99, 425);
	FrontendText_DrawCentered(10, FrontendString_Get((UIString)(*value + valueBaseLabelId)), rect, color);

	return result;
}

static __inline int MissionSetup_DrawGameSettingsTileNumberValue(
	FrontendRect* rect, FrontendRect* clippedRect, int x, const char* spriteName,
	const char* disabledSpriteName, int spriteFrame, int useSpriteFrame, UIString labelId,
	const uint8_t* value, int buttonId, int enabled, int mouseX, int mouseY) {
	int color;
	int result;
	const char* drawSpriteName;

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 425);
	result = 0;
	if (enabled) {
		color = g_colorPaleBlue;
		FrontendDraw_RectCopy(clippedRect, rect);
		FrontendDraw_RectClipToBounds(clippedRect);
		if (FrontendDraw_PointInRect(clippedRect, mouseX, mouseY)) {
			color = g_colorYellow;
		}

		if (useSpriteFrame) {
			FrontImage_SetSpriteFrame(spriteName, spriteFrame);
		}
		result = FrontendButton_DrawSpriteHitTest(rect, spriteName, spriteName, NULL, 10, g_colorLightBlue,
												  buttonId, "settingsound");
	} else {
		color = g_colorLightBlue;
		drawSpriteName = disabledSpriteName != NULL ? disabledSpriteName : spriteName;
		if (useSpriteFrame) {
			FrontImage_SetSpriteFrame(drawSpriteName, spriteFrame);
		}
		FrontImage_DrawSprite(drawSpriteName, x, 351);
	}

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 363);
	FrontendText_DrawCentered(10, FrontendString_Get(labelId), rect, color);
	FrontendDraw_RectAssign(rect, x, 413, x + 99, 425);
	sprintf(g_frontendScratchBuffer, "%d", *value);
	FrontendText_DrawCentered(10, g_frontendScratchBuffer, rect, color);
	return result;
}

static __inline int MissionSetup_DrawGameSettingsLapsTile(FrontendRect* rect, FrontendRect* clippedRect,
														  int x, int enabled, int mouseX, int mouseY) {
	int color;
	int result;

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 425);
	result = 0;
	if (enabled) {
		color = g_colorPaleBlue;
		FrontendDraw_RectCopy(clippedRect, rect);
		FrontendDraw_RectClipToBounds(clippedRect);
		if (FrontendDraw_PointInRect(clippedRect, mouseX, mouseY)) {
			color = g_colorYellow;
		}

		result = FrontendButton_DrawSpriteHitTest(rect, "laps", "laps", NULL, 10, g_colorLightBlue, 33,
												  "settingsound");
	} else {
		color = g_colorLightBlue;
		FrontImage_DrawSprite("setting13", x, 351);
	}

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 363);
	FrontendText_DrawCentered(10, FrontendString_Get(STR_GAME_LAPS), rect, color);
	FrontendDraw_RectAssign(rect, x, 413, x + 99, 425);
	sprintf(g_frontendScratchBuffer, "%d", g_gameConfig.laps + 1);
	FrontendText_DrawCentered(10, g_frontendScratchBuffer, rect, color);
	return result;
}

static __inline int MissionSetup_DrawGameSettingsInitialDistanceTile(FrontendRect* rect,
																	 FrontendRect* clippedRect, int x,
																	 int enabled, int mouseX, int mouseY) {
	int color;
	int result;

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 425);
	result = 0;
	if (enabled) {
		color = g_colorPaleBlue;
		FrontendDraw_RectCopy(clippedRect, rect);
		FrontendDraw_RectClipToBounds(clippedRect);
		if (FrontendDraw_PointInRect(clippedRect, mouseX, mouseY)) {
			color = g_colorYellow;
		}

		FrontImage_SetSpriteFrame("setting11", g_gameConfig.initialDistance);
		result = FrontendButton_DrawSpriteHitTest(rect, "setting11", "setting11", NULL, 10, g_colorLightBlue,
												  31, "settingsound");
	} else {
		color = g_colorLightBlue;
		FrontImage_SetSpriteFrame("setting11", g_gameConfig.initialDistance);
		FrontImage_DrawSprite("setting11", x, 351);
	}

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 363);
	FrontendText_DrawCentered(10, FrontendString_Get(STR_GAME_INITIAL_DISTANCE), rect, color);
	FrontendDraw_RectAssign(rect, x, 413, x + 99, 425);
	sprintf(g_frontendScratchBuffer, "%d %s", g_gameConfig.initialDistance, FrontendString_Get(STR_KM));
	FrontendText_DrawCentered(10, g_frontendScratchBuffer, rect, color);
	return result;
}

static __inline int MissionSetup_DrawGameSettingsMaxPointsTile(FrontendRect* rect, FrontendRect* clippedRect,
															   int x, int enabled, int mouseX, int mouseY) {
	int color;
	int result;

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 425);
	result = 0;
	if (enabled) {
		color = g_colorPaleBlue;
		FrontendDraw_RectCopy(clippedRect, rect);
		FrontendDraw_RectClipToBounds(clippedRect);
		if (FrontendDraw_PointInRect(clippedRect, mouseX, mouseY)) {
			color = g_colorYellow;
		}

		FrontImage_SetSpriteFrame("setting10", g_gameConfig.maxPoints);
		result = FrontendButton_DrawSpriteHitTest(rect, "setting10", "setting10", NULL, 10, g_colorLightBlue,
												  29, "settingsound");
	} else {
		color = g_colorLightBlue;
		FrontImage_SetSpriteFrame("setting10", g_gameConfig.maxPoints);
		FrontImage_DrawSprite("setting10", x, 351);
	}

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 363);
	FrontendText_DrawCentered(10, FrontendString_Get(STR_GAME_MAX_POINTS), rect, color);
	FrontendDraw_RectAssign(rect, x, 413, x + 99, 425);
	if (g_gameConfig.maxPoints == 21) {
		strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_GAME_UNLIMITED));
	} else {
		sprintf(g_frontendScratchBuffer, "%d", 500 * g_gameConfig.maxPoints);
	}
	FrontendText_DrawCentered(10, g_frontendScratchBuffer, rect, color);
	return result;
}

static __inline int MissionSetup_DrawGameSettingsLastTeamTimeLimitTile(FrontendRect* rect,
																	   FrontendRect* clippedRect, int x,
																	   int enabled, int mouseX, int mouseY) {
	int color;
	int result;

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 425);
	result = 0;
	if (enabled) {
		color = g_colorPaleBlue;
		FrontendDraw_RectCopy(clippedRect, rect);
		FrontendDraw_RectClipToBounds(clippedRect);
		if (FrontendDraw_PointInRect(clippedRect, mouseX, mouseY)) {
			color = g_colorYellow;
		}

		FrontImage_SetSpriteFrame("setting4", g_gameConfig.lastTeamTimeLimit);
		result = FrontendButton_DrawSpriteHitTest(rect, "setting4", "setting4", NULL, 10, g_colorLightBlue,
												  23, "settingsound");
	} else {
		color = g_colorLightBlue;
		FrontImage_SetSpriteFrame("setting4", g_gameConfig.lastTeamTimeLimit);
		FrontImage_DrawSprite("setting4", x, 351);
	}

	FrontendDraw_RectAssign(rect, x, 351, x + 99, 363);
	FrontendText_DrawCentered(10, FrontendString_Get(STR_MISSION_TIME_LIMIT), rect, color);
	FrontendDraw_RectAssign(rect, x, 413, x + 99, 425);
	if (g_gameConfig.lastTeamTimeLimit) {
		sprintf(g_frontendScratchBuffer, "%d %s", g_gameConfig.lastTeamTimeLimit,
				FrontendString_Get(STR_MIN));
	} else {
		strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_CNONE));
	}
	FrontendText_DrawCentered(10, g_frontendScratchBuffer, rect, color);
	return result;
}

static __inline void MissionSetup_QueueCarouselSlide(int slideOffset) {
	if (slideOffset > 0) {
		if (g_missionSetupCarouselSlideOffset) {
			if (g_missionSetupCarouselQueuedSlideOffset < 0) {
				g_missionSetupCarouselQueuedSlideOffset = 100;
			} else if (g_missionSetupCarouselQueuedSlideOffset < 200) {
				g_missionSetupCarouselQueuedSlideOffset += 100;
			}
		} else {
			g_missionSetupCarouselSlideOffset = 100;
		}
	} else if (g_missionSetupCarouselSlideOffset) {
		if (g_missionSetupCarouselQueuedSlideOffset > 0) {
			g_missionSetupCarouselQueuedSlideOffset = -100;
		} else if (g_missionSetupCarouselQueuedSlideOffset > -200) {
			g_missionSetupCarouselQueuedSlideOffset -= 100;
		}
	} else {
		g_missionSetupCarouselSlideOffset = -100;
	}
}

static __inline void MissionSetup_ClampCarouselSelectedOrdinal(int visibleCount) {
	int selectedOrdinal;

	selectedOrdinal = g_missionSetupBattleSelectedMissionOrdinal;
	if (selectedOrdinal < 0) {
		g_missionSetupBattleSelectedMissionOrdinal = visibleCount + selectedOrdinal;
	} else if (selectedOrdinal >= visibleCount) {
		g_missionSetupBattleSelectedMissionOrdinal = visibleCount - selectedOrdinal;
	}
}

static __inline void MissionSetup_UpdateCarouselSlidePosition(int* left, int visibleCount) {
	int slideOffset;
	int queuedSlideOffset;
	int selectedOrdinal;

	slideOffset = g_missionSetupCarouselSlideOffset;
	if (slideOffset < 0) {
		if (slideOffset >= -6) {
			slideOffset += 1;
		} else if (slideOffset >= -12) {
			slideOffset += 2;
		} else {
			slideOffset += 6;
		}
		g_missionSetupCarouselSlideOffset = slideOffset;
		*left = *left + 100 * (slideOffset / 100) - slideOffset - 100;
	} else if (slideOffset > 0) {
		if (slideOffset <= 6) {
			slideOffset -= 1;
		} else if (slideOffset <= 12) {
			slideOffset -= 2;
		} else {
			slideOffset -= 6;
		}
		g_missionSetupCarouselSlideOffset = slideOffset;
		*left += 100 * (slideOffset / 100 + 1) - slideOffset;
	}

	if (slideOffset == 0) {
		selectedOrdinal = (70 - *left) / 100;
		g_missionSetupBattleSelectedMissionOrdinal = selectedOrdinal;
		queuedSlideOffset = g_missionSetupCarouselQueuedSlideOffset;
		if (queuedSlideOffset != 0) {
			if (queuedSlideOffset < 0) {
				g_missionSetupCarouselSlideOffset = -100;
				g_missionSetupCarouselQueuedSlideOffset = queuedSlideOffset + 100;
			} else {
				g_missionSetupCarouselSlideOffset = 100;
				g_missionSetupCarouselQueuedSlideOffset = queuedSlideOffset - 100;
			}
		}
	}

	MissionSetup_ClampCarouselSelectedOrdinal(visibleCount);
}

static __inline void MissionSetup_SetSkirmishTeamCount(int numberOfTeams) {
	int flightGroupIdx;
	int teamIdx;

	g_gameConfig.numberOfTeams = (uint8_t)numberOfTeams;
	for (flightGroupIdx = 0; flightGroupIdx < 16; ++flightGroupIdx) {
		g_frontendMission->flightGroups[flightGroupIdx].team =
			(uint8_t)(flightGroupIdx / (16 / numberOfTeams));
	}

	if (g_gameConfig.numberOfTeams > 2u) {
		for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
			if (g_gameConfig.teamGoals[teamIdx] > 1u) {
				g_gameConfig.teamGoals[teamIdx] = 0;
			}
		}
	}

	MissionSetup_CountActiveTeams();
	if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT) {
		g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
		g_frontendRightBarAnimState = 0;
		g_frontendRightBarPanelIndex = 1;
		FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
	}
}

static __inline void MissionSetup_SelectFirstUnlockedIfCurrentLocked(void) {
	int missionIdx;

	if (g_missionList == NULL) {
		return;
	}

	MissionSetup_SelectMissionListEntryForMission(g_missionList);
	missionIdx = g_selectedMissionListIndex;
	if (missionIdx >= g_missionCount || !g_missionList[missionIdx].lockedFlag) {
		return;
	}

	missionIdx = 0;
	if (g_missionCount > 0) {
		while (g_missionList[missionIdx].lockedFlag) {
			++missionIdx;
			if (missionIdx >= g_missionCount) {
				break;
			}
		}
		if (missionIdx < g_missionCount) {
			g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
				g_missionList[missionIdx].missionIdx;
			g_selectedMissionListIndex = missionIdx;
		}
	}
	FrontendMission_LoadCurrent();
	MissionSetup_CountActiveTeams();
	MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
	MissionSetup_LoadMissionDescText(g_briefingText);
}

// FUNCTION: XWA 0x54CE80
int MissionSetup_CountMissionListEntries(void* stream) {
	int count;
	char buffer[MISSION_LIST_LINE_SIZE];
	char currentSectionName[MISSION_LIST_SECTION_NAME_SIZE];
#ifdef XWA_MODERN
	XwaFile* file;
#else
	char* readLine;
#endif

	count = 0;
#ifdef XWA_MODERN
	file = (XwaFile*)stream;
	while (File_ReadLine(file, buffer, sizeof(buffer))) {
#else
	while (1) {
		readLine = fgets(buffer, sizeof(buffer), (FILE*)stream);
		if (readLine == NULL) {
			break;
		}
#endif
		if (buffer[0] != '/' || buffer[1] != '/') {
#ifdef XWA_MODERN
			MissionSetup_CopyResolvedLine(g_frontendScratchBuffer, buffer);
#else
			strcpy(g_frontendScratchBuffer, Linez_ResolveString(buffer));
#endif
			if (g_frontendScratchBuffer[0] == '[') {
				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] = '\0';
				strcpy(currentSectionName, &g_frontendScratchBuffer[1]);
			} else {
#ifdef XWA_MODERN
				if (!File_ReadLine(file, g_frontendScratchBuffer, MISSION_LIST_LINE_SIZE)) {
#else
				readLine = fgets(g_frontendScratchBuffer, MISSION_LIST_LINE_SIZE, (FILE*)stream);
				if (readLine == NULL) {
#endif
					return count;
				}

#ifdef XWA_MODERN
				if (!File_ReadLine(file, g_frontendScratchBuffer, MISSION_LIST_LINE_SIZE)) {
#else
				readLine = fgets(g_frontendScratchBuffer, MISSION_LIST_LINE_SIZE, (FILE*)stream);
				if (readLine == NULL) {
#endif
					return count;
				}

				++count;
			}
		}
	}

	return count;
}

// FUNCTION: XWA 0x5521F0
int MissionSetup_IsSkirmishMeleeFile(const char* fileName) {
	XwaFile* stream;
	uint32_t fileFormatVersion;
	uint32_t version;
	int meleeFlag;
	int i;
	char buffer[MISSION_LIST_LINE_SIZE];

	sprintf(buffer, "%s\\%s", g_campaignDirNames[MISSION_DIRECTORY_SKIRMISH], fileName);
	stream = File_Open(AERON_VFS_ROOT_ASSET, buffer, "rb");
	if (stream != NULL) {
		File_ReadCount(stream, g_frontendScratchBuffer, 0x40u);
		File_ReadDword(stream, &fileFormatVersion);
		if (fileFormatVersion > SKIRMISH_FILE_FORMAT_MELEE_FLAG_VERSION) {
			File_Close(stream);
			return 0;
		}
		version = fileFormatVersion;

		File_Seek(stream, 4, SEEK_CUR);
		File_Seek(stream, 320, SEEK_CUR);
		if (version >= 4) {
			File_Seek(stream, 4416, SEEK_CUR);
		}

		File_Seek(stream, 4, SEEK_CUR);
		File_Seek(stream, 4, SEEK_CUR);
		File_Seek(stream, 4, SEEK_CUR);
		File_Seek(stream, 4, SEEK_CUR);
		if (version >= 2) {
			File_Seek(stream, 4, SEEK_CUR);
		}

		if (version >= 3) {
			i = 10;
			do {
				File_Seek(stream, 4, SEEK_CUR);
				--i;
			} while (i != 0);
		}

		if (version >= 5) {
			File_Seek(stream, 320, SEEK_CUR);
		}

		if (version >= SKIRMISH_FILE_FORMAT_MELEE_FLAG_VERSION) {
			File_ReadDword(stream, &meleeFlag);
		} else {
			meleeFlag = 0;
		}

		File_Close(stream);
		return meleeFlag;
	}

	return 0;
}

// FUNCTION: XWA 0x552E50
int MissionSetup_CompareMissionListEntries(const void* left, const void* right) {
	const MissionListEntry* leftEntry;
	const MissionListEntry* rightEntry;
	int leftIsTemplate;
	int rightIsTemplate;

	leftEntry = (const MissionListEntry*)left;
	rightEntry = (const MissionListEntry*)right;

	leftIsTemplate =
		strcmp(leftEntry->sectionName, FrontendString_Get(STR_GAME_SAVE_SKIRMISH_TEMPLATES)) == 0;
	rightIsTemplate =
		strcmp(rightEntry->sectionName, FrontendString_Get(STR_GAME_SAVE_SKIRMISH_TEMPLATES)) == 0;

	if (leftIsTemplate == rightIsTemplate) {
		return strcmp(leftEntry->description, rightEntry->description);
	}

	return leftIsTemplate > rightIsTemplate ? -1 : 1;
}

// FUNCTION: XWA 0x547800
void MissionSetup_LoadMissionList(MissionDirectoryId missionDirectoryId) {
	FrontFilenameList* fileList;
	FrontFilenameListNode* node;
	XwaFile* stream;
	int entryIdx;
	int i;
	char buffer[MISSION_LIST_LINE_SIZE];
	char currentSectionName[MISSION_LIST_SECTION_NAME_SIZE];

	if (g_missionList != NULL) {
		Mem_Free(g_missionList);
		g_missionList = NULL;
	}

	g_missionCount = 0;
	if (missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
		sprintf(g_frontendScratchBuffer, "%s\\mission.lst", g_campaignDirNames[missionDirectoryId]);
		while (1) {
#ifdef XWA_MODERN
			stream = File_OpenAsset(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "r");
#else
			stream = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "r");
#endif
			if (stream != NULL) {
				break;
			}

			if (!FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_FAILED_TO_DETECT_CD1),
												  FrontendString_Get(STR_FAILED_TO_DETECT_CD2),
												  FrontendString_Get(STR_FAILED_TO_DETECT_CD3),
												  FrontendString_Get(STR_OKAY),
												  FrontendString_Get(STR_CANCEL))) {
				break;
			}
		}
		if (stream == NULL) {
			return;
		}

		g_missionCount = MissionSetup_CountMissionListEntries(stream);
		File_Seek(stream, 0, SEEK_SET);
		if (g_missionCount == 0) {
			File_Close(stream);
			return;
		}

		g_missionList = (MissionListEntry*)Mem_Alloc(sizeof(*g_missionList) * (size_t)g_missionCount);
		if (g_missionList == NULL) {
			File_Close(stream);
			return;
		}

		memset(currentSectionName, 0, sizeof(currentSectionName));
		entryIdx = 0;
		while ((unsigned int)entryIdx < (unsigned int)g_missionCount) {
			while (1) {
				do {
#ifdef XWA_MODERN
					if (!File_ReadLine(stream, buffer, sizeof(buffer))) {
#else
					if (fgets(buffer, sizeof(buffer), (FILE*)stream) == NULL) {
#endif
						g_missionCount = entryIdx;
						File_Close(stream);
						return;
					}
				} while (buffer[0] == '/' && buffer[1] == '/');

#ifdef XWA_MODERN
				MissionSetup_CopyResolvedLine(g_frontendScratchBuffer, buffer);
#else
				strcpy(g_frontendScratchBuffer, Linez_ResolveString(buffer));
				if (g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] == '\n') {
					g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] = '\0';
				}
#endif
				if (g_frontendScratchBuffer[0] != '[') {
					break;
				}

				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] = '\0';
				strcpy(currentSectionName, &g_frontendScratchBuffer[1]);
			}

			strcpy(g_missionList[entryIdx].sectionName, currentSectionName);
			g_missionList[entryIdx].lockedFlag = 0;
			g_missionList[entryIdx].missionIdx = atoi(g_frontendScratchBuffer);

#ifdef XWA_MODERN
			if (!File_ReadLine(stream, buffer, sizeof(buffer))) {
#else
			if (fgets(buffer, sizeof(buffer), (FILE*)stream) == NULL) {
#endif
				g_missionCount = entryIdx;
				File_Close(stream);
				return;
			}

#ifdef XWA_MODERN
			MissionSetup_CopyResolvedLine(g_frontendScratchBuffer, buffer);
#else
			strcpy(g_frontendScratchBuffer, Linez_ResolveString(buffer));
			if (g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] == '\n') {
				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] = '\0';
			}
#endif
			{
				unsigned int lowercaseIdx;

				for (lowercaseIdx = 0; lowercaseIdx < strlen(g_frontendScratchBuffer); ++lowercaseIdx) {
					g_frontendScratchBuffer[lowercaseIdx] =
						(char)tolower((unsigned char)g_frontendScratchBuffer[lowercaseIdx]);
				}
			}

			if (g_frontendScratchBuffer[0] == '*') {
				if (entryIdx == 0) {
					strcpy(g_missionList->fileName, &g_frontendScratchBuffer[2]);
				} else {
					if (!g_pilotData.tourOfDutyMissions[g_missionList[entryIdx - 1].missionIdx]
							 .completedCount) {
						g_missionList[entryIdx].lockedFlag = 1;
					}

					strcpy(g_missionList[entryIdx].fileName, &g_frontendScratchBuffer[2]);
				}
			} else if (g_frontendScratchBuffer[0] == '&') {
				g_missionList[entryIdx].lockedFlag = 1;
				strcpy(g_missionList[entryIdx].fileName, &g_frontendScratchBuffer[2]);
			} else {
				strcpy(g_missionList[entryIdx].fileName, g_frontendScratchBuffer);
			}

#ifdef XWA_MODERN
			if (!File_ReadLine(stream, buffer, sizeof(buffer))) {
#else
			if (fgets(buffer, sizeof(buffer), (FILE*)stream) == NULL) {
#endif
				g_missionCount = entryIdx;
				File_Close(stream);
				return;
			}

#ifdef XWA_MODERN
			MissionSetup_CopyResolvedLine(g_frontendScratchBuffer, buffer);
#else
			strcpy(g_frontendScratchBuffer, Linez_ResolveString(buffer));
			if (g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] == '\n') {
				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] = '\0';
			}
#endif
			strcpy(g_missionList[entryIdx].description, g_frontendScratchBuffer);

			++entryIdx;
		}

		File_Close(stream);
		return;
	}

#ifdef XWA_MODERN
	sprintf(g_frontendScratchBuffer, "%s\\*.skm", g_campaignDirNames[MISSION_DIRECTORY_SKIRMISH]);
	fileList = FrontendFileList_BuildSorted(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer);
	if (fileList == NULL) {
		g_missionCount = 1;
		g_missionList = (MissionListEntry*)Mem_Alloc(sizeof(*g_missionList));
		strcpy(g_missionList->fileName, g_pilotData.missionFileName);
		strcpy(g_missionList->description, FrontendString_Get(STR_GAME_NEW_SKIRMISH));
		strcpy(g_missionList->sectionName, FrontendString_Get(STR_QUICK_SKIRMISH));
		g_missionList->missionIdx = 0;
		g_missionList->lockedFlag = 0;
	} else {
		g_missionCount = fileList->count + 1;
		g_missionList = (MissionListEntry*)Mem_Alloc(sizeof(*g_missionList) * (size_t)g_missionCount);
		strcpy(g_missionList[0].fileName, g_pilotData.missionFileName);
		strcpy(g_missionList[0].description, FrontendString_Get(STR_GAME_NEW_SKIRMISH));
		strcpy(g_missionList[0].sectionName, FrontendString_Get(STR_QUICK_SKIRMISH));
		g_missionList[0].missionIdx = 0;
		g_missionList[0].lockedFlag = 0;

		node = fileList->head;
		if (fileList->count != 0) {
			entryIdx = 1;
			do {
				strcpy(g_missionList[entryIdx].fileName, node->fileName);
				strcpy(g_missionList[entryIdx].description, node->fileName);
				g_missionList[entryIdx].description[strlen(node->fileName) - 4] = '\0';
				g_missionList[entryIdx].missionIdx = 1;
				strcpy(g_missionList[entryIdx].sectionName,
					   FrontendString_Get(STR_GAME_SAVE_SKIRMISH_TEMPLATES));
				g_missionList[entryIdx].lockedFlag = 0;
				node = node->next;
				++entryIdx;
			} while ((unsigned int)(entryIdx - 1) < (unsigned int)fileList->count);
		}

		FrontendFileList_Free(fileList);
	}
#else
	if (_chdir(g_campaignDirNames[MISSION_DIRECTORY_SKIRMISH]) == 0) {
		fileList = FrontendFileList_BuildSorted(AERON_VFS_ROOT_ASSET, "*.skm");
		if (fileList != NULL) {
			g_missionCount = fileList->count + 1;
			g_missionList = (MissionListEntry*)Mem_Alloc(sizeof(*g_missionList) * (size_t)g_missionCount);
			strcpy(g_missionList[0].fileName, g_pilotData.missionFileName);
			strcpy(g_missionList[0].description, FrontendString_Get(STR_GAME_NEW_SKIRMISH));
			strcpy(g_missionList[0].sectionName, FrontendString_Get(STR_QUICK_SKIRMISH));
			g_missionList[0].missionIdx = 0;
			g_missionList[0].lockedFlag = 0;

			node = fileList->head;
			if ((unsigned int)fileList->count > 0) {
				entryIdx = 0;
				i = 1;
				do {
					MissionListEntry* entries;

					strcpy(g_missionList[i].fileName, node->fileName);
					strcpy(g_missionList[i].description, node->fileName);
					entries = g_missionList;
					entries[i].description[strlen(node->fileName) - 4] = '\0';
					g_missionList[i].missionIdx = 1;
					strcpy(g_missionList[i].sectionName,
						   FrontendString_Get(STR_GAME_SAVE_SKIRMISH_TEMPLATES));
					g_missionList[i].lockedFlag = 0;
					node = node->next;
					++i;
					++entryIdx;
				} while ((unsigned int)entryIdx < (unsigned int)fileList->count);
			}

			FrontendFileList_Free(fileList);
			_chdir("..");
		} else {
			g_missionCount = 1;
			g_missionList = (MissionListEntry*)Mem_Alloc(sizeof(*g_missionList));
			strcpy(g_missionList->fileName, g_pilotData.missionFileName);
			strcpy(g_missionList->description, FrontendString_Get(STR_GAME_NEW_SKIRMISH));
			strcpy(g_missionList->sectionName, FrontendString_Get(STR_QUICK_SKIRMISH));
			g_missionList->missionIdx = 0;
			g_missionList->lockedFlag = 0;
			_chdir("..");
		}
	} else {
		g_missionCount = 1;
		g_missionList = (MissionListEntry*)Mem_Alloc(sizeof(*g_missionList));
		strcpy(g_missionList->fileName, g_pilotData.missionFileName);
		strcpy(g_missionList->description, FrontendString_Get(STR_GAME_NEW_SKIRMISH));
		strcpy(g_missionList->sectionName, FrontendString_Get(STR_QUICK_SKIRMISH));
		g_missionList->missionIdx = 0;
		g_missionList->lockedFlag = 0;
	}
#endif

	if ((unsigned int)g_missionCount > 1) {
		i = 1;
		while ((unsigned int)i < (unsigned int)g_missionCount) {
			if (MissionSetup_IsSkirmishMeleeFile(g_missionList[i].fileName) == 1) {
				strcpy(g_missionList[i].sectionName,
					   FrontendString_Get(STR_GAME_SAVE_SKIRMISH_TEMPLATES_MELEE));
			}

			++i;
		}

		qsort(&g_missionList[1], (size_t)(g_missionCount - 1), MISSION_LIST_ENTRY_SIZE,
			  MissionSetup_CompareMissionListEntries);
	}
}

// FUNCTION: XWA 0x548140
void MissionSetup_LoadMissionDescText(char* outText4096) {
	unsigned int selectedMissionIdx;
	int missionDirectoryId;
	int missionDescriptionId;
	XwaFile* missionFile;
	int scanPos;
	int closePos;
	int16_t missionFormatVersion;
	char controlCode;
	char replacementChar;
	char missionBaseName[128];
	char textTailBuffer[MISSION_TEXT_BUFFER_SIZE];

	if (outText4096 == NULL) {
		return;
	}

	memset(outText4096, 0, MISSION_TEXT_BUFFER_SIZE);
	memset(textTailBuffer, 0, sizeof(textTailBuffer));

	selectedMissionIdx = 0;
	missionDirectoryId = g_pilotData.missionDirectoryId;
	missionDescriptionId = g_pilotData.missionDescriptionIds[missionDirectoryId];
	if ((unsigned int)g_missionCount > 0u) {
		while (g_missionList[selectedMissionIdx].missionIdx != missionDescriptionId) {
			++selectedMissionIdx;
			if (selectedMissionIdx >= (unsigned int)g_missionCount) {
				break;
			}
		}
	}

	if (selectedMissionIdx >= (unsigned int)g_missionCount) {
		return;
	}

	sprintf(g_frontendScratchBuffer, "%s\\%s", g_campaignDirNames[missionDirectoryId],
			g_missionList[selectedMissionIdx].fileName);
	missionFile = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "rb");
	if (missionFile == NULL) {
		return;
	}

	strcpy(missionBaseName, g_missionList[selectedMissionIdx].fileName);
	scanPos = (int)strlen(missionBaseName) - 1;
	if (scanPos > 0) {
		for (; scanPos > 0 && missionBaseName[scanPos] != '.'; --scanPos) {
		}

		if (scanPos > 0) {
			missionBaseName[scanPos] = '\0';
		}
	}
	File_ReadWord(missionFile, &missionFormatVersion);

	sprintf(textTailBuffer, "!%s_S00!", missionBaseName);
	if (missionFormatVersion == 18 || missionFormatVersion == 17) {
		File_Seek(missionFile, -MISSION_TEXT_BUFFER_SIZE, SEEK_END);
		scanPos = (int)strlen(textTailBuffer);
		File_ReadCount(missionFile, &textTailBuffer[scanPos], (size_t)(MISSION_TEXT_BUFFER_SIZE - scanPos));
		textTailBuffer[MISSION_TEXT_BUFFER_SIZE - 1] = '\0';
	} else if (missionFormatVersion == 12) {
		File_Seek(missionFile, -MISSION_LEGACY_TEXT_BUFFER_SIZE, SEEK_END);
		scanPos = (int)strlen(textTailBuffer);
		File_ReadCount(missionFile, &textTailBuffer[scanPos],
					   (size_t)(MISSION_LEGACY_TEXT_BUFFER_SIZE - scanPos));
		textTailBuffer[MISSION_LEGACY_TEXT_BUFFER_SIZE - 1] = '\0';
	} else {
		outText4096[0] = '\0';
		File_Close(missionFile);
		return;
	}

	while (scanPos < MISSION_TEXT_BUFFER_SIZE) {
		if (textTailBuffer[scanPos] == '#') {
			textTailBuffer[scanPos] = '\0';
			break;
		}

		++scanPos;
	}

	strncpy(outText4096, Linez_ResolveString(textTailBuffer), MISSION_TEXT_BUFFER_SIZE - 1);
	scanPos = 1;
	controlCode = 6;
	do {
		if (outText4096[scanPos - 1] == '[') {
			outText4096[scanPos - 1] = controlCode;
			if (scanPos < MISSION_TEXT_BUFFER_SIZE) {
				closePos = scanPos;
				for (;;) {
					if (outText4096[closePos] == ']') {
						replacementChar = outText4096[closePos + 1];
						outText4096[closePos] = replacementChar;
						outText4096[closePos + 1] = 1;
						break;
					}

					++closePos;
					if (closePos >= MISSION_TEXT_BUFFER_SIZE) {
						break;
					}
				}
			}
		}

		++scanPos;
	} while (scanPos - 1 < MISSION_TEXT_BUFFER_SIZE);
	File_Close(missionFile);
}

// FUNCTION: XWA 0x54D620
void MissionSetup_CountActiveTeams(void) {
	int numberOfTeams;
	int flightGroupIdx;
	int activeTeamCount;

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		memset(g_teamFgCountScratch, 0, sizeof(g_teamFgCountScratch));
		numberOfTeams = g_gameConfig.numberOfTeams;
		for (flightGroupIdx = 0; flightGroupIdx < numberOfTeams; ++flightGroupIdx) {
			g_teamFgCountScratch[flightGroupIdx] = 16 / numberOfTeams;
		}

		g_teamCount = numberOfTeams;
		return;
	}

	memset(g_teamFgCountScratch, 0, sizeof(g_teamFgCountScratch));
	activeTeamCount = 0;
	g_teamCount = activeTeamCount;
	for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_frontendMission->flightGroupCount;
		 ++flightGroupIdx) {
		if (g_frontendMission->flightGroups[flightGroupIdx].playerNumber != 0) {
			++g_teamFgCountScratch[g_frontendMission->flightGroups[flightGroupIdx].team];
		}
	}

	for (flightGroupIdx = 0; flightGroupIdx < 10; ++flightGroupIdx) {
		if (g_teamFgCountScratch[flightGroupIdx] != 0) {
			++activeTeamCount;
		}
	}

	g_teamCount = activeTeamCount;
}

// FUNCTION: XWA 0x54D6D0
int MissionSetup_RebuildCombatSimSlotsFromFrontendMission(void) {
	int nextSlotByTeam[10];
	int team;
	int slotIdx;

	memset(g_combatSimSlots, 0, sizeof(g_combatSimSlots));
	memset(g_combatSimLoadoutOptions, 0, sizeof(g_combatSimLoadoutOptions));
	memset(g_combatSimSkirmishFileName, 0, sizeof(g_combatSimSkirmishFileName));

	slotIdx = 0;
	for (team = 0; team < 10; ++team) {
		nextSlotByTeam[team] = slotIdx;
		slotIdx += g_teamFgCountScratch[team];
	}

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		int slotNumber;

		memset(g_gameConfig.teamGoals, 0, sizeof(g_gameConfig.teamGoals));
		slotNumber = 0;
		do {
			CombatSimLoadoutOptions* loadoutOptions;
			CombatSimSlot* slot;

			slotIdx = nextSlotByTeam[g_frontendMission->flightGroups[slotNumber].team];
			slot = &g_combatSimSlots[slotIdx];
			loadoutOptions = &g_combatSimLoadoutOptions[slotIdx];

			sprintf(g_combatSimSlotNames[slotNumber], "%s",
					FrontendString_Get((UIString)(STR_GAME_ONE + slotNumber)));
			slot->fgIndex = (uint16_t)slotNumber;
			slot->numberOfCraft = 1;
			slot->groupAI = 2;
			slot->craftRole = 1;
			slot->craftType = 0;
			slot->ownerPlayerId = 0;
			slot->gunnerPlayerId = 0;
			slot->numberOfWaves = 0;
			slot->warhead = 0;
			slot->countermeasures = 0;
			slot->beam = 0;
			slot->primaryFg = 0;

			loadoutOptions->warheadOptions[0] = 0;
			loadoutOptions->warheadOptions[1] = 3;
			loadoutOptions->warheadOptions[2] = 4;
			loadoutOptions->warheadOptions[3] = 2;
			loadoutOptions->warheadOptions[4] = 1;
			loadoutOptions->warheadOptions[5] = 5;
			loadoutOptions->warheadOptions[6] = 6;
			loadoutOptions->warheadOptions[7] = 7;
			loadoutOptions->warheadOptions[8] = 8;
			loadoutOptions->warheadOptionCount = 9;
			loadoutOptions->selectedWarheadOption = 0;
			loadoutOptions->beamOptions[0] = 0;
			loadoutOptions->beamOptions[1] = 1;
			loadoutOptions->beamOptions[2] = 2;
			loadoutOptions->beamOptions[3] = 3;
			loadoutOptions->beamOptionCount = 4;
			loadoutOptions->selectedBeamOption = 0;
			loadoutOptions->countermeasureOptions[0] = 0;
			loadoutOptions->countermeasureOptions[1] = 1;
			loadoutOptions->countermeasureOptions[2] = 2;
			loadoutOptions->countermeasureOptionCount = 3;
			loadoutOptions->selectedCountermeasureOption = 0;
			loadoutOptions->optionalCraftCategory = 1;
			loadoutOptions->selectedCraftOption = g_shipTypeToShipListIndex[1];
			loadoutOptions->craftOptionCount = 255;

			++nextSlotByTeam[g_frontendMission->flightGroups[slotNumber].team];
			++slotNumber;
		} while (slotNumber < 16);
	} else {
		int fgIdx;

		for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
			g_combatSimSlots[slotIdx].fgIndex = UINT16_MAX;
		}

		fgIdx = 0;
		if ((int16_t)g_frontendMission->flightGroupCount > 0) {
			do {
				CombatSimLoadoutOptions* loadoutOptions;
				CombatSimSlot* slot;

				if (g_frontendMission->flightGroups[fgIdx].playerNumber != 0) {
					int optionIdx;
					int optionCount;

					slotIdx = nextSlotByTeam[g_frontendMission->flightGroups[fgIdx].team];
					slot = &g_combatSimSlots[slotIdx];
					loadoutOptions = &g_combatSimLoadoutOptions[slotIdx];

					strcpy(g_combatSimSlotNames[fgIdx], g_frontendMission->flightGroups[fgIdx].name);
					slot->fgIndex = (uint16_t)fgIdx;
					slot->craftType = g_frontendMission->flightGroups[fgIdx].craftType;
					slot->ownerPlayerId = 0;
					slot->gunnerPlayerId = 0;
					slot->numberOfWaves = g_frontendMission->flightGroups[fgIdx].numberOfWaves;
					slot->numberOfCraft = g_frontendMission->flightGroups[fgIdx].numberOfCraft;
					slot->groupAI = g_frontendMission->flightGroups[fgIdx].groupAI;
					slot->craftRole = 0;
					slot->warhead = g_frontendMission->flightGroups[fgIdx].warhead;
					slot->countermeasures = g_frontendMission->flightGroups[fgIdx].countermeasures;
					slot->beam = g_frontendMission->flightGroups[fgIdx].beam;
					slot->primaryFg = 0;

					loadoutOptions->warheadOptions[0] = g_frontendMission->flightGroups[fgIdx].warhead;
					optionCount = 1;
					for (optionIdx = 0; optionIdx < 8; ++optionIdx) {
						if (g_frontendMission->flightGroups[fgIdx].optionalWarheads[optionIdx] != 0) {
							loadoutOptions->warheadOptions[optionCount] =
								g_frontendMission->flightGroups[fgIdx].optionalWarheads[optionIdx];
							++optionCount;
						}
					}

					if (g_frontendMission->flightGroups[fgIdx].warhead != 0) {
						loadoutOptions->warheadOptions[optionCount++] = 0;
					}

					loadoutOptions->warheadOptionCount = optionCount;
					loadoutOptions->selectedWarheadOption = 0;

					loadoutOptions->beamOptions[0] = g_frontendMission->flightGroups[fgIdx].beam;
					optionCount = 1;
					for (optionIdx = 0; optionIdx < 6; ++optionIdx) {
						if (g_frontendMission->flightGroups[fgIdx].optionalBeams[optionIdx] != 0) {
							loadoutOptions->beamOptions[optionCount] =
								g_frontendMission->flightGroups[fgIdx].optionalBeams[optionIdx];
							++optionCount;
						}
					}

					if (g_frontendMission->flightGroups[fgIdx].beam != 0) {
						loadoutOptions->beamOptions[optionCount++] = 0;
					}

					loadoutOptions->beamOptionCount = optionCount;
					loadoutOptions->selectedBeamOption = 0;

					loadoutOptions->countermeasureOptions[0] =
						g_frontendMission->flightGroups[fgIdx].countermeasures;
					optionCount = 1;
					for (optionIdx = 0; optionIdx < 4; ++optionIdx) {
						if (g_frontendMission->flightGroups[fgIdx].optionalCountermeasures[optionIdx] != 0) {
							loadoutOptions->countermeasureOptions[optionCount] =
								g_frontendMission->flightGroups[fgIdx].optionalCountermeasures[optionIdx];
							++optionCount;
						}
					}

					if (g_frontendMission->flightGroups[fgIdx].countermeasures != 0) {
						loadoutOptions->countermeasureOptions[optionCount++] = 0;
					}

					loadoutOptions->countermeasureOptionCount = optionCount;
					loadoutOptions->selectedCountermeasureOption = 0;
					loadoutOptions->optionalCraftCategory =
						g_frontendMission->flightGroups[fgIdx].optionalCraftCategory;

					switch (g_frontendMission->flightGroups[fgIdx].optionalCraftCategory) {
						case 1:
							loadoutOptions->selectedCraftOption =
								g_shipTypeToShipListIndex[g_frontendMission->flightGroups[fgIdx].craftType];
							loadoutOptions->craftOptionCount = 255;
							break;

						case 0:
						case 2:
						case 3:
							loadoutOptions->selectedCraftOption = 0;
							loadoutOptions->craftTypeOptions[0] =
								g_frontendMission->flightGroups[fgIdx].craftType;
							loadoutOptions->craftOptionCount = 1;
							break;

						case 4:
							optionCount = 1;
							loadoutOptions->craftTypeOptions[0] =
								g_frontendMission->flightGroups[fgIdx].craftType;
							loadoutOptions->craftCountOptions[0] =
								g_frontendMission->flightGroups[fgIdx].numberOfCraft;
							loadoutOptions->craftWaveOptions[0] =
								g_frontendMission->flightGroups[fgIdx].numberOfWaves;
							for (optionIdx = 0; optionIdx < 10; ++optionIdx) {
								if (g_frontendMission->flightGroups[fgIdx].optionalCraft[optionIdx] != 0) {
									loadoutOptions->craftTypeOptions[optionCount] =
										g_frontendMission->flightGroups[fgIdx].optionalCraft[optionIdx];
									loadoutOptions->craftCountOptions[optionCount] =
										g_frontendMission->flightGroups[fgIdx]
											.numberOfOptionalCraft[optionIdx];
									++optionCount;
									loadoutOptions->craftWaveOptions[optionCount - 1] =
										g_frontendMission->flightGroups[fgIdx]
											.numberOfOptionalCraftWaves[optionIdx];
								}
							}

							loadoutOptions->craftOptionCount = optionCount;
							loadoutOptions->selectedCraftOption = 0;
							break;

						default:
							break;
					}

					++nextSlotByTeam[g_frontendMission->flightGroups[fgIdx].team];
				}
				++fgIdx;
			} while (fgIdx < (int16_t)g_frontendMission->flightGroupCount);
		}
	}

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		int activeSlotCount;

		activeSlotCount = 0;
		for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
			if (g_combatSimSlots[slotIdx].fgIndex >= 0) {
				++activeSlotCount;
			}
		}

		if (activeSlotCount == 1) {
			for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
				if (g_combatSimSlots[slotIdx].fgIndex >= 0) {
					g_combatSimSlots[slotIdx].ownerPlayerId = 1;
					break;
				}
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x5552A0
int MissionSetup_GetCachedCraftTechStats(CraftTechStats* stats) {
	int index;
	CraftTechStats* baseStats;
	CraftTechStats* cachedStats;

	baseStats = g_cachedCraftTechStats;
	if (baseStats == NULL) {
		return BuildCraftTechStats(stats);
	}

	index = 0;
	if (index < g_cachedCraftTechStatsCount) {
		int craftType;

		craftType = stats->craftType;
		cachedStats = baseStats;
		for (; index < g_cachedCraftTechStatsCount;) {
			if (cachedStats->craftType == craftType) {
				memcpy(stats, &baseStats[index], sizeof(*stats));
				return 1;
			}

			++index;
			++cachedStats;
		}
	}

	return BuildCraftTechStats(stats);
}

// FUNCTION: XWA 0x555420
int MissionSetup_ComputeCraftPointTotal(int craftType, int warheadType, int beamType, int countermeasureType,
										int groupAI, int includeLoadoutAndAi) {
	int combatRating;
	CraftTechStats stats;

	if (craftType == 0) {
		return 0;
	}

	stats.craftType = craftType;
	MissionSetup_GetCachedCraftTechStats(&stats);
	combatRating = stats.combatRating;
	if (includeLoadoutAndAi) {
		switch (groupAI) {
			case 0:
				break;

			case 1:
				combatRating = 150 * stats.combatRating / 100;
				break;

			case 2:
				combatRating = 2 * stats.combatRating;
				break;

			case 3:
				combatRating = 3 * stats.combatRating;
				break;

			case 4:
				combatRating = 4 * stats.combatRating;
				break;

			default:
				combatRating = 5 * stats.combatRating;
				break;
		}
	}

	combatRating +=
		Mission_ComputeCraftLoadoutPointValue(craftType, warheadType, beamType, countermeasureType);
	return combatRating;
}

// FUNCTION: XWA 0x552BB0
int MissionSetup_GetTeamCraftPointTotal(int team) {
	int pointTotal;
	int slotIdx;
	int slotPointTotal;
	CombatSimSlot* slot;

	pointTotal = 0;
	for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
		slot = &g_combatSimSlots[slotIdx];
		if (slot->craftType != 0 && g_frontendMission->flightGroups[(int16_t)slot->fgIndex].team == team) {
			slotPointTotal =
				MissionSetup_ComputeCraftPointTotal(slot->craftType, slot->warhead, slot->beam,
													slot->countermeasures, slot->groupAI, slot->craftRole) *
				slot->numberOfCraft;
			if (g_gameConfig.goalType != 1 || g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH ||
				!g_shipList[g_shipTypeToShipListIndex[slot->craftType]].flyable) {
				slotPointTotal *= slot->numberOfWaves + 1;
			}

			pointTotal += slotPointTotal;
		}
	}

	return pointTotal;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5520E0
int MissionSetup_IsSlotWithinPointLimit(int slotIndex) {
	int teamCraftPointTotal;
	CombatSimSlot* slot;

	slot = &g_combatSimSlots[slotIndex];
	if (g_combatSimSlots[slotIndex].ownerPlayerId == 0) {
		return 1;
	}

	if (slot->craftType == 0) {
		return 0;
	}

	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
		g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		teamCraftPointTotal =
			MissionSetup_GetTeamCraftPointTotal(g_frontendMission->flightGroups[(int16_t)slot->fgIndex].team);
		if (g_gameConfig.maxPoints < 21 && teamCraftPointTotal > 500 * g_gameConfig.maxPoints) {
			return 0;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x555310
int MissionSetup_CanSelectSlotCraft(CombatSimSlot* slot) {
	int canSelectCraft;
	int team;
	int slotIdx;

	canSelectCraft = 0;
	team = g_frontendMission->flightGroups[(int16_t)slot->fgIndex].team;
	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			++canSelectCraft;
		} else if (slot->ownerPlayerId == 1) {
			canSelectCraft = 1;
		}
	} else {
		switch (g_gameConfig.craftSelection) {
			case 0:
				canSelectCraft = 0;
				break;

			case 1:
				if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH || slot->ownerPlayerId) {
					if (slot->ownerPlayerId == Net_GetLocalPlayerId()) {
						canSelectCraft = 1;
					}
				} else {
					if (FrontendNet_IsTeamLocalPlayer(team)) {
						canSelectCraft = 1;
					}

					for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
						if (g_frontendMission->flightGroups[(int16_t)g_combatSimSlots[slotIdx].fgIndex]
									.team == team &&
							(g_combatSimSlots[slotIdx].ownerPlayerId ||
							 g_combatSimSlots[slotIdx].gunnerPlayerId)) {
							break;
						}
					}

					if (slotIdx >= 16 && Net_IsHost()) {
						canSelectCraft = 1;
					}
				}
				break;

			case 2:
				if (Net_IsHost()) {
					canSelectCraft = 1;
				}
				break;

			default:
				canSelectCraft = 1;
				break;
		}
	}

	return canSelectCraft;
}

// FUNCTION: XWA 0x54FCC0
int MissionSetup_SelectedSlotHasChanges(void) {
	const char* savedSlotBytes;
	const char* selectedSlotBytes;
	unsigned int byteIndex;

	savedSlotBytes = (const char*)&g_combatSimSlots[g_selectedCombatSimSlotIdx];
	selectedSlotBytes = (const char*)&g_selectedCombatSimSlot;
	for (byteIndex = 0; byteIndex < sizeof(g_selectedCombatSimSlot); ++byteIndex) {
		if (*savedSlotBytes != *selectedSlotBytes) {
			return 1;
		}

		++selectedSlotBytes;
		++savedSlotBytes;
	}

	return 0;
}

// FUNCTION: XWA 0x552C80
int MissionSetup_SyncSlotLoadoutSelection(int slotIdx) {
	int optionIdx;

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		CraftTechStats craftCapabilities;

		if (g_combatSimSlots[slotIdx].craftType != 0) {
			memset(&craftCapabilities, 0, sizeof(craftCapabilities));
			craftCapabilities.craftType = g_combatSimSlots[slotIdx].craftType;
			MissionSetup_GetCachedCraftTechStats(&craftCapabilities);
			if (!craftCapabilities.warheadRating) {
				g_combatSimSlots[slotIdx].warhead = 0;
			}
		}

		g_combatSimLoadoutOptions[slotIdx].selectedWarheadOption = g_combatSimSlots[slotIdx].warhead;
		g_combatSimLoadoutOptions[slotIdx].selectedBeamOption = g_combatSimSlots[slotIdx].beam;
		g_combatSimLoadoutOptions[slotIdx].selectedCountermeasureOption =
			g_combatSimSlots[slotIdx].countermeasures;
	} else {
		for (optionIdx = 0; optionIdx < g_combatSimLoadoutOptions[slotIdx].warheadOptionCount; ++optionIdx) {
			if (g_combatSimLoadoutOptions[slotIdx].warheadOptions[optionIdx] ==
				g_combatSimSlots[slotIdx].warhead) {
				g_combatSimLoadoutOptions[slotIdx].selectedWarheadOption = optionIdx;
				break;
			}
		}

		for (optionIdx = 0; optionIdx < g_combatSimLoadoutOptions[slotIdx].beamOptionCount; ++optionIdx) {
			if (g_combatSimLoadoutOptions[slotIdx].beamOptions[optionIdx] == g_combatSimSlots[slotIdx].beam) {
				g_combatSimLoadoutOptions[slotIdx].selectedBeamOption = optionIdx;
				break;
			}
		}

		for (optionIdx = 0; optionIdx < g_combatSimLoadoutOptions[slotIdx].countermeasureOptionCount;
			 ++optionIdx) {
			if (g_combatSimLoadoutOptions[slotIdx].countermeasureOptions[optionIdx] ==
				g_combatSimSlots[slotIdx].countermeasures) {
				g_combatSimLoadoutOptions[slotIdx].selectedCountermeasureOption = optionIdx;
				break;
			}
		}

		if (g_combatSimLoadoutOptions[slotIdx].optionalCraftCategory != 1) {
			for (optionIdx = 0; optionIdx < g_combatSimLoadoutOptions[slotIdx].craftOptionCount;
				 ++optionIdx) {
				if (g_combatSimLoadoutOptions[slotIdx].craftTypeOptions[optionIdx] ==
						g_combatSimSlots[slotIdx].craftType &&
					g_combatSimLoadoutOptions[slotIdx].craftCountOptions[optionIdx] ==
						g_combatSimSlots[slotIdx].numberOfCraft &&
					g_combatSimLoadoutOptions[slotIdx].craftWaveOptions[optionIdx] ==
						g_combatSimSlots[slotIdx].numberOfWaves) {
					g_combatSimLoadoutOptions[slotIdx].selectedCraftOption = optionIdx;
					break;
				}
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x552160
int MissionSetup_AreReadyPlayersAssignedToSlots(void) {
	int slotIdx;
	int rosterIdx;
	int readyRosterCount;
	int assignedSlotIdx;
	int playerId;

	for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
		if (!MissionSetup_IsSlotWithinPointLimit(slotIdx)) {
			return 0;
		}
	}

	readyRosterCount = 0;
	for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
		if (g_mpRoster[rosterIdx].playerId && g_mpRosterReadyFlags[rosterIdx]) {
			++readyRosterCount;
		}
	}

	if (readyRosterCount < Net_CountReadyPlayers()) {
		return 0;
	}

	for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
		playerId = g_mpRoster[rosterIdx].playerId;
		if (playerId != 0) {
			for (assignedSlotIdx = 0; assignedSlotIdx < 16; ++assignedSlotIdx) {
				if (g_combatSimSlots[assignedSlotIdx].ownerPlayerId == playerId ||
					g_combatSimSlots[assignedSlotIdx].gunnerPlayerId == playerId) {
					break;
				}
			}

			if (assignedSlotIdx >= 16) {
				return 0;
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x54F800
int MissionSetup_FormatCombatSimSlotSummaryText(int slotIdx) {
	int fgIndex;
	size_t textLength;
	uint16_t craftType;
	const char* craftName;
	CombatSimSlot* slot;
	char* roleText;
	char* padEnd;
	char buffer[32];

	switch (g_missionSetupSlotSummaryMode) {
		case MISSION_SETUP_SLOT_SUMMARY_CRAFT:
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
				craftType = g_combatSimSlots[slotIdx].craftType;
				craftName = GetCraftTypeModelLongName(craftType);
				if (craftName == NULL) {
					craftName =
						g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIdx].craftType]].name;
				}

				strcpy(g_frontendScratchBuffer, craftName);
			} else {
				slot = &g_combatSimSlots[slotIdx];
				craftName = GetCraftTypeModelLongName(slot->craftType);
				if (craftName == NULL) {
					craftName = g_shipList[g_shipTypeToShipListIndex[slot->craftType]].name;
				}

				if (g_gameConfig.goalType) {
					if (g_shipList[g_shipTypeToShipListIndex[slot->craftType]].flyable) {
						sprintf(g_frontendScratchBuffer, "%d %s %s, %s %s", slot->numberOfCraft, craftName,
								g_combatSimSlotNames[slotIdx], FrontendString_Get(STR_GAME_UL),
								FrontendString_Get(STR_SKIRMISH_WAVES));
					} else if (!slot->numberOfWaves) {
						sprintf(g_frontendScratchBuffer, "%d %s %s", slot->numberOfCraft, craftName,
								g_combatSimSlotNames[slotIdx]);
					} else {
						sprintf(g_frontendScratchBuffer, "%d %s %s,  %d %s", slot->numberOfCraft, craftName,
								g_combatSimSlotNames[slotIdx], slot->numberOfWaves + 1,
								FrontendString_Get(STR_SKIRMISH_WAVES));
					}
				} else {
					if (slot->numberOfWaves == 99) {
						sprintf(g_frontendScratchBuffer, "%d %s %s, %s %s", slot->numberOfCraft, craftName,
								g_combatSimSlotNames[slotIdx], FrontendString_Get(STR_GAME_UL),
								FrontendString_Get(STR_SKIRMISH_WAVES));
					} else if (!slot->numberOfWaves) {
						sprintf(g_frontendScratchBuffer, "%d %s %s", slot->numberOfCraft, craftName,
								g_combatSimSlotNames[slotIdx]);
					} else {
						sprintf(g_frontendScratchBuffer, "%d %s %s,  %d %s", slot->numberOfCraft, craftName,
								g_combatSimSlotNames[slotIdx], slot->numberOfWaves + 1,
								FrontendString_Get(STR_SKIRMISH_WAVES));
					}
				}
			}
			break;

		case MISSION_SETUP_SLOT_SUMMARY_LOADOUT:
			g_frontendScratchBuffer[0] = '\0';
			if (g_combatSimSlots[slotIdx].warhead) {
				sprintf(buffer, "%s",
						FrontendString_Get(
							(UIString)(g_combatSimSlots[slotIdx].warhead + STR_GAME_WARHEAD_NONE_SHORT)));
				strcat(g_frontendScratchBuffer, buffer);
			}

			if (g_combatSimSlots[slotIdx].beam) {
				if (!g_frontendScratchBuffer[0]) {
					sprintf(buffer, "%s",
							FrontendString_Get(
								(UIString)(g_combatSimSlots[slotIdx].beam + STR_GAME_BEAM_NONE_SHORT)));
				} else {
					sprintf(buffer, " | %s",
							FrontendString_Get(
								(UIString)(g_combatSimSlots[slotIdx].beam + STR_GAME_BEAM_NONE_SHORT)));
				}

				strcat(g_frontendScratchBuffer, buffer);
			}

			if (g_combatSimSlots[slotIdx].countermeasures) {
				if (!g_frontendScratchBuffer[0]) {
					sprintf(buffer, "%s",
							FrontendString_Get((UIString)(g_combatSimSlots[slotIdx].countermeasures +
														  STR_GAME_COUNTER_NONE_SHORT)));
				} else {
					sprintf(buffer, " | %s",
							FrontendString_Get((UIString)(g_combatSimSlots[slotIdx].countermeasures +
														  STR_GAME_COUNTER_NONE_SHORT)));
				}

				strcat(g_frontendScratchBuffer, buffer);
			}

			if (!g_frontendScratchBuffer[0]) {
				strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_GAME_NO_WARHEADS));
			}
			break;

		case MISSION_SETUP_SLOT_SUMMARY_DUTY:
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				strcpy(g_frontendScratchBuffer,
					   FrontendString_Get(
						   (UIString)(g_combatSimSlots[slotIdx].craftRole + STR_GAME_STATIONARY)));
			} else {
				fgIndex = (int16_t)g_combatSimSlots[slotIdx].fgIndex;
				roleText = g_frontendMission->flightGroups[fgIndex].craftRole;
				if (roleText[0]) {
					sprintf(buffer, "%s", roleText);
					strcat(g_frontendScratchBuffer, buffer);
				} else {
					strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_GAME_NO_DUTY));
				}
			}
			break;
	}

	textLength = strlen(g_frontendScratchBuffer);
	if (textLength < 30) {
		padEnd = &g_frontendScratchBuffer[textLength + 1];
		do {
			padEnd[-1] = ' ';
			*padEnd++ = '\0';
		} while (padEnd < &g_frontendScratchBuffer[31]);
	}

	return 1;
}

// FUNCTION: XWA 0x55DC20
int MissionSetup_LoadShipBmpForCraftType(int craftType) {
	int groupId;

	groupId = craftType + 20000;
	if (g_missionSetupShipBmpGroupId != groupId) {
		if (g_missionSetupShipBmpGroupId) {
			MissionSetup_UnloadShipBmp();
		}

		SpriteResource_LoadGroup((int16_t)(craftType + 20000));
		g_missionSetupShipBmpGroupId = groupId;
		FrontImage_RegisterAtlasSprite("shipbmp", (uint16_t)groupId, 1, 1);
	}

	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x55DC70
int MissionSetup_UnloadShipBmp(void) {
	FrontImage_FreeResourceByName("shipbmp");
	SpriteResource_UnloadGroup((int16_t)g_missionSetupShipBmpGroupId);
	g_missionSetupShipBmpGroupId = 0;
	return 1;
}

// FUNCTION: XWA 0x55DD60
int MissionSetup_LoadBattleSprites(void) {
	SpriteResource_LoadGroup(15210);
	FrontImage_RegisterAtlasSprite("mislarge", 15210, 1, 53);
	FrontImage_RegisterAtlasSprite("missmall", 15210, 1000, 53);
	FrontImage_RegisterAtlasSprite("battleback", 15210, 2000, 8);
	FrontImage_RegisterAtlasSprite("battle00", 15210, 2100, 1);
	return 1;
}

// FUNCTION: XWA 0x55DDE0
int MissionSetup_FreeBattleSprites(void) {
	FrontImage_FreeResourceByName("mislarge");
	FrontImage_FreeResourceByName("missmall");
	FrontImage_FreeResourceByName("battleback");
	SpriteResource_UnloadGroup(15210);
	return 1;
}

// FUNCTION: XWA 0x55E0F0
int MultiplayerSetup_FreeTransportSprites(void) {
	FrontImage_FreeResourceByName("ipxbmp");
	FrontImage_FreeResourceByName("tcpipbmp");
	FrontImage_FreeResourceByName("modembmp");
	FrontImage_FreeResourceByName("serialbmp");
	FrontImage_FreeResourceByName("sipxbmp");
	FrontImage_FreeResourceByName("stcpipbmp");
	FrontImage_FreeResourceByName("smodembmp");
	FrontImage_FreeResourceByName("sserialbmp");
	SpriteResource_UnloadGroup(15230);
	return 1;
}

// FUNCTION: XWA 0x54CF90
int MissionSetup_DrawBackgroundAndPreview(int drawToCurrentSurface) {
	int frameWidth;
	int frameX;
	int y;
	int shipBmpHeight;
	const char* modelLongName;
	int craftType;
	int offscreenFrameWidth;
	int offscreenFrameX;
	int offscreenShipBmpHeight;
	FrontendRect spriteRect;

	FrontImage_RegisterResourceDefault("frontres\\combat\\multiplayer.bmp", "background");
	if (drawToCurrentSurface) {
		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontImage_DrawSprite("settingbar", 57, 348);
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
			if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_BATTLE_SELECT) {
				if (frame || g_missionSetupBattleFirstMissionIdx != 7) {
					FrontImage_GetResourceRect("battleback", &spriteRect);
					FrontImage_SetSpriteFrame("battleback", frame);
					FrontImage_DrawSprite("battleback",
										  ((spriteRect.left - spriteRect.right + 509) >> 1) + 65, 90);
				} else {
					FrontImage_GetResourceRect("battle00", &spriteRect);
					FrontImage_DrawSprite("battle00", ((spriteRect.left - spriteRect.right + 509) >> 1) + 65,
										  90);
				}
			} else {
				FrontImage_SetSpriteFrame("mislarge",
										  g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_TOUR]);
				FrontImage_DrawSprite("mislarge", 65, 90);
			}
		} else if ((g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH ||
					g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) &&
				   g_missionSetupActivePanel == MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT) {
			if (g_missionSetupSubpanelMode > 0) {
				FrontImage_GetResourceRect("leftframe", &spriteRect);
				frameWidth = spriteRect.right - spriteRect.left + 1;
				FrontImage_GetResourceRect("topframe", &spriteRect);
				frameX = frameWidth + 70;
				FrontImage_DrawSpriteTranslucent("topframe", frameWidth + 70,
												 spriteRect.top - spriteRect.bottom + 89);
				FrontImage_DrawSpriteTranslucent("bottomframe", frameWidth + 70, 330);
				for (y = 90; y < 330; y += 15) {
					FrontImage_DrawSpriteTranslucent("centerframe", frameX, y);
				}

				if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
					FrontImage_GetResourceRect("shipbmp", &spriteRect);
					shipBmpHeight = spriteRect.bottom - spriteRect.top + 1;
					FrontImage_DrawSprite("shipbmp", 70, ((360 - shipBmpHeight) >> 1) + 40);
					FrontendDraw_RectAssign(&spriteRect, 70, (shipBmpHeight >> 1) + 220, frameX,
											(shipBmpHeight >> 1) + 235);
					FrontendText_DrawCentered(10, FrontendString_Get(STR_GAME_CURRENT_CRAFT), &spriteRect,
											  g_colorLightBlue);
					FrontendDraw_RectOffsetXY(&spriteRect, 0, 15);
					if (g_selectedCombatSimSlot.craftType) {
						modelLongName = GetCraftTypeModelLongName(g_selectedCombatSimSlot.craftType);
						if (modelLongName) {
							sprintf(
								g_frontendScratchBuffer, "%s (%s)",
								g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].name,
								modelLongName);
							FrontendText_DrawCentered(10, g_frontendScratchBuffer, &spriteRect,
													  g_colorLightBlue);
						} else {
							craftType = g_selectedCombatSimSlot.craftType;
							sprintf(g_frontendScratchBuffer, "%s",
									g_shipList[g_shipTypeToShipListIndex[craftType]].name);
							FrontendText_DrawCentered(10, g_frontendScratchBuffer, &spriteRect,
													  g_colorLightBlue);
						}
					} else {
						craftType = 0;
						sprintf(g_frontendScratchBuffer, "%s",
								g_shipList[g_shipTypeToShipListIndex[craftType]].name);
						FrontendText_DrawCentered(10, g_frontendScratchBuffer, &spriteRect, g_colorLightBlue);
					}
				}
			} else if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				FrontImage_GetResourceRect("shipbmp", &spriteRect);
				FrontImage_DrawSprite("shipbmp", spriteRect.left - spriteRect.right + 564,
									  ((spriteRect.top - spriteRect.bottom + 359) >> 1) + 40);
			}
		}
	}

	FrontendDisplay_LockOffscreenSurface();
	FrontImage_DrawSpriteOpaque("background", 0, 0);
	FrontImage_DrawSprite("settingbar", 57, 348);
	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
		if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_BATTLE_SELECT) {
			if (frame || g_missionSetupBattleFirstMissionIdx != 7) {
				FrontImage_GetResourceRect("battleback", &spriteRect);
				FrontImage_SetSpriteFrame("battleback", frame);
				FrontImage_DrawSprite("battleback", ((spriteRect.left - spriteRect.right + 509) >> 1) + 65,
									  90);
			} else {
				FrontImage_GetResourceRect("battle00", &spriteRect);
				FrontImage_DrawSprite("battle00", ((spriteRect.left - spriteRect.right + 509) >> 1) + 65, 90);
			}
		} else {
			FrontImage_SetSpriteFrame("mislarge", g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_TOUR]);
			FrontImage_DrawSprite("mislarge", 65, 90);
		}
	} else if ((g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH ||
				g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) &&
			   g_missionSetupActivePanel == MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT) {
		if (g_missionSetupSubpanelMode > 0) {
			FrontImage_GetResourceRect("leftframe", &spriteRect);
			offscreenFrameWidth = spriteRect.right - spriteRect.left + 1;
			FrontImage_GetResourceRect("topframe", &spriteRect);
			offscreenFrameX = offscreenFrameWidth + 70;
			FrontImage_DrawSpriteTranslucent("topframe", offscreenFrameWidth + 70,
											 spriteRect.top - spriteRect.bottom + 89);
			FrontImage_DrawSpriteTranslucent("bottomframe", offscreenFrameWidth + 70, 330);
			for (y = 90; y < 330; y += 15) {
				FrontImage_DrawSpriteTranslucent("centerframe", offscreenFrameX, y);
			}

			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
				FrontImage_GetResourceRect("shipbmp", &spriteRect);
				offscreenShipBmpHeight = spriteRect.bottom - spriteRect.top + 1;
				FrontImage_DrawSprite("shipbmp", 70, ((360 - offscreenShipBmpHeight) >> 1) + 40);
				FrontendDraw_RectAssign(&spriteRect, 70, (offscreenShipBmpHeight >> 1) + 220, offscreenFrameX,
										(offscreenShipBmpHeight >> 1) + 235);
				FrontendText_DrawCentered(10, FrontendString_Get(STR_GAME_CURRENT_CRAFT), &spriteRect,
										  g_colorLightBlue);
				FrontendDraw_RectOffsetXY(&spriteRect, 0, 15);
				if (g_selectedCombatSimSlot.craftType) {
					modelLongName = GetCraftTypeModelLongName(g_selectedCombatSimSlot.craftType);
					if (modelLongName) {
						sprintf(g_frontendScratchBuffer, "%s (%s)",
								g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].name,
								modelLongName);
						FrontendText_DrawCentered(10, g_frontendScratchBuffer, &spriteRect, g_colorLightBlue);
					} else {
						craftType = g_selectedCombatSimSlot.craftType;
						sprintf(g_frontendScratchBuffer, "%s",
								g_shipList[g_shipTypeToShipListIndex[craftType]].name);
						FrontendText_DrawCentered(10, g_frontendScratchBuffer, &spriteRect, g_colorLightBlue);
					}
				} else {
					craftType = 0;
					sprintf(g_frontendScratchBuffer, "%s",
							g_shipList[g_shipTypeToShipListIndex[craftType]].name);
					FrontendText_DrawCentered(10, g_frontendScratchBuffer, &spriteRect, g_colorLightBlue);
				}
			}
		} else if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			FrontImage_GetResourceRect("shipbmp", &spriteRect);
			FrontImage_DrawSprite("shipbmp", spriteRect.left - spriteRect.right + 564,
								  ((spriteRect.top - spriteRect.bottom + 359) >> 1) + 40);
		}
	}

	FrontendDisplay_UnlockOffscreenSurface(1);
	return 1;
}

typedef struct MissionSetupLobbyPlayerRecord {
	int32_t playerId;
	int32_t ratingOrSlotIndex;
	int32_t averageLatencyMs;
	int32_t packetCount;
	int32_t packetDropCount;
	int32_t packetRetryCount;
} MissionSetupLobbyPlayerRecord;

typedef struct MissionSetupLobbySelectionPayload {
	char multiplayerGameName[32];
	int32_t unused20;
	int32_t missionDirectoryId;
	int32_t missionDescriptionId;
	int32_t maxPlayerCount;
	int32_t readyOrRosterCount;
	MissionSetupLobbyPlayerRecord players[19];
} MissionSetupLobbySelectionPayload;

typedef char
	xwa_mission_setup_lobby_player_record_size[(sizeof(MissionSetupLobbyPlayerRecord) == 0x18) ? 1 : -1];
typedef char xwa_mission_setup_lobby_selection_payload_header_size
	[(offsetof(MissionSetupLobbySelectionPayload, players) == 0x34) ? 1 : -1];
typedef char
	xwa_mission_setup_lobby_selection_payload_size[(sizeof(MissionSetupLobbySelectionPayload) == 0x1FC) ? 1
																										: -1];

// FUNCTION: XWA 0x549160
int MissionSetup_BroadcastLobbySelectionPacket(void) {
	int packetDwordCount;
	int playerCount;
	int rosterIdx;
	int playerIdx;
	int playerId;
	int outCount;
	NetPlayerInfo* playerRoster;
	MissionSetupLobbySelectionPayload* payload;
	MissionSetupLobbyPlayerRecord* record;

	payload = (MissionSetupLobbySelectionPayload*)g_frontendNetPacketScratch.payload;
	g_frontendNetPacketScratch.packetType = 111;
	memcpy(payload->multiplayerGameName, g_pilotData.multiplayerGameName,
		   sizeof(payload->multiplayerGameName));
	payload->missionDirectoryId = g_pilotData.missionDirectoryId;
	payload->missionDescriptionId = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
	payload->maxPlayerCount = 8;
	if (g_missionSetupRosterAuthoritative) {
		payload->readyOrRosterCount = 8;
		packetDwordCount = 14;
		playerRoster = Net_GetPlayerRoster(&outCount);
		for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
			playerIdx = 0;
			if (outCount > 0) {
				while (playerRoster[playerIdx].playerId == 0 || playerRoster[playerIdx].readyFlag == 0 ||
					   g_mpRoster[rosterIdx].playerId != playerRoster[playerIdx].playerId) {
					++playerIdx;
					if (playerIdx >= outCount) {
						break;
					}
				}
			}

			if (playerIdx == outCount) {
				g_mpRoster[rosterIdx].playerId = 0;
			}
		}

		for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
			record = &payload->players[rosterIdx];
			playerId = g_mpRoster[rosterIdx].playerId;
			record->playerId = playerId;
			record->ratingOrSlotIndex = g_mpRoster[rosterIdx].rating;
			record->averageLatencyMs = (int32_t)Net_GetAverageLatencyMs(playerId);
			record->packetCount = Net_GetPlayerPacketCount(playerId);
			record->packetDropCount = Net_GetPlayerPacketDropCount(playerId);
			record->packetRetryCount = Net_GetPlayerPacketRetryCount(playerId);
			packetDwordCount += 6;
		}
	} else {
		payload->readyOrRosterCount = Net_CountReadyPlayers();
		packetDwordCount = 14;
		playerRoster = Net_GetPlayerRoster(&outCount);
		playerCount = 0;
		if (outCount > 0) {
			for (playerIdx = 0; playerIdx < outCount; ++playerIdx) {
				if (playerRoster[playerIdx].readyFlag == 1) {
					record = &payload->players[playerCount];
					playerId = playerRoster[playerIdx].playerId;
					record->playerId = playerId;
					record->ratingOrSlotIndex = (int32_t)((uint8_t)playerRoster[playerIdx].playerName[0] - 1);
					record->averageLatencyMs = (int32_t)Net_GetAverageLatencyMs(playerId);
					record->packetCount = Net_GetPlayerPacketCount(playerId);
					record->packetDropCount = Net_GetPlayerPacketDropCount(playerId);
					record->packetRetryCount = Net_GetPlayerPacketRetryCount(playerId);
					++playerCount;
					packetDwordCount += 6;
				}
			}
		}
	}

	Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, (unsigned int)(4 * packetDwordCount));
	return 1;
}

// FUNCTION: XWA 0x549330
int MissionSetup_BroadcastStatePacket(int stateKind) {
	int packetSize;
	int readyPlayerCount;
	int rosterIdx;
	int outCount;
	uint8_t* writePtr;
	NetPlayerInfo* playerRoster;

	if (stateKind == 1) {
		g_frontendNetPacketScratch.packetType = 'J';
	} else if (stateKind == 2) {
		g_frontendNetPacketScratch.packetType = 'I';
	} else {
		g_frontendNetPacketScratch.packetType = stateKind == 3 ? 'O' : 'A';
	}

	memcpy(g_frontendNetPacketScratch.payload, g_pilotData.multiplayerGameName, 32u);
	g_frontendNetPacketScratch.payload[32] = (uint8_t)g_pilotData.missionDirectoryId;
#ifdef XWA_MODERN
	ByteOrder_WriteU16Le(&g_frontendNetPacketScratch.payload[33],
						 (uint16_t)g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId]);
#else
	*(uint16_t*)&g_frontendNetPacketScratch.payload[33] =
		(uint16_t)g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
#endif
	g_frontendNetPacketScratch.payload[35] = 8;
	g_frontendNetPacketScratch.payload[36] = (uint8_t)Net_CountReadyPlayers();

	packetSize = 41;
	playerRoster = Net_GetPlayerRoster(&outCount);
	writePtr = &g_frontendNetPacketScratch.payload[37];
	readyPlayerCount = 0;
	rosterIdx = 0;
	while (rosterIdx < outCount && readyPlayerCount < 8) {
		if (playerRoster[rosterIdx].readyFlag == 1) {
#ifdef XWA_MODERN
			ByteOrder_WriteU32Le(writePtr, (uint32_t)playerRoster[rosterIdx].playerId);
#else
			*(uint32_t*)writePtr = (uint32_t)playerRoster[rosterIdx].playerId;
#endif
			writePtr += 4;
#ifdef XWA_MODERN
			ByteOrder_WriteU32Le(writePtr, (uint32_t)((uint8_t)playerRoster[rosterIdx].playerName[0] - 1));
#else
			*(uint32_t*)writePtr = (uint32_t)((uint8_t)playerRoster[rosterIdx].playerName[0] - 1);
#endif
			writePtr += 4;
#ifdef XWA_MODERN
			ByteOrder_WriteU32Le(writePtr, Net_GetAverageLatencyMs(playerRoster[rosterIdx].playerId));
#else
			*(uint32_t*)writePtr = Net_GetAverageLatencyMs(playerRoster[rosterIdx].playerId);
#endif
			writePtr += 4;
			++readyPlayerCount;
		}

		packetSize += 12;
		++rosterIdx;
	}

	if (readyPlayerCount < 8) {
		int remainingRecords;

		remainingRecords = 8 - readyPlayerCount;
		packetSize += 12 * remainingRecords;
		do {
#ifdef XWA_MODERN
			memset(writePtr, 0, 12u);
			writePtr += 12;
#else
			*(uint32_t*)writePtr = 0;
			writePtr += 4;
			*(uint32_t*)writePtr = 0;
			writePtr += 4;
			*(uint32_t*)writePtr = 0;
			writePtr += 4;
#endif
			--remainingRecords;
		} while (remainingRecords);
	}

	writePtr[0] = g_gameConfig.difficulty;
	++packetSize;
	writePtr[1] = g_gameConfig.collisions;
	++packetSize;
	writePtr += 2;
	*writePtr++ = g_gameConfig.requirePassword;
	++packetSize;
	*writePtr++ = g_gameConfig.inProgressJoin;
	++packetSize;
	*writePtr++ = g_gameConfig.craftSelection;
	++packetSize;
	*writePtr++ = g_gameConfig.locatePlayers;
	++packetSize;
	*writePtr++ = g_gameConfig.lastTeamTimeLimit;
	++packetSize;
	*writePtr++ = g_gameConfig.eachTeamOwnRegion;
	++packetSize;
	*writePtr++ = g_gameConfig.environment;
	++packetSize;
	*writePtr++ = g_gameConfig.numberOfTeams;
	++packetSize;
	*writePtr++ = g_gameConfig.asyncFlag;
	++packetSize;
	*writePtr++ = g_gameConfig.laps;
	++packetSize;
	*writePtr++ = g_gameConfig.initialDistance;
	++packetSize;
	*writePtr++ = g_gameConfig.serverUpdateRate;
	++packetSize;
	*writePtr++ = g_gameConfig.maxPoints;
	++packetSize;
	*writePtr++ = g_gameConfig.goalType;
	++packetSize;
	*writePtr++ = g_gameConfig.timeLimit;
	++packetSize;

	if (stateKind == 1) {
#ifdef XWA_MODERN
		ByteOrder_WriteU32Le(writePtr, (uint32_t)g_missionSetupPendingRandomSeed);
#else
		*(uint32_t*)writePtr = (uint32_t)g_missionSetupPendingRandomSeed;
#endif
	} else {
#ifdef XWA_MODERN
		ByteOrder_WriteU32Le(writePtr, (uint32_t)g_gameConfig.randomSeed);
#else
		*(uint32_t*)writePtr = (uint32_t)g_gameConfig.randomSeed;
#endif
	}
	writePtr += 4;
	packetSize += 4;

	memcpy((uint8_t*)&g_frontendNetPacketScratch + packetSize, g_gameConfig.teamGoals,
		   sizeof(g_gameConfig.teamGoals));
	packetSize += (int)sizeof(g_gameConfig.teamGoals);

	memcpy((uint8_t*)&g_frontendNetPacketScratch + packetSize, g_combatSimSlots, sizeof(g_combatSimSlots));
	packetSize += (int)sizeof(g_combatSimSlots);

	return Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, (unsigned int)packetSize);
}

// FUNCTION: XWA 0x549570
int MissionSetup_BroadcastSkirmishMetadata(void) {
	if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
		return 0;
	}

	g_frontendNetPacketScratch.packetType = 'N';
	memcpy(g_frontendNetPacketScratch.payload, g_combatSimSlotNames, sizeof(g_combatSimSlotNames));
	memcpy(&g_frontendNetPacketScratch.payload[sizeof(g_combatSimSlotNames)], g_combatSimSkirmishFileName,
		   sizeof(g_combatSimSkirmishFileName));
	Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 0x184u);
	return 1;
}

// FUNCTION: XWA 0x5495D0
int MissionSetup_BroadcastGameConfigPacket(void) {
	uint8_t* payload;

	payload = g_frontendNetPacketScratch.payload;
	payload[0] = g_gameConfig.difficulty;
	payload[1] = g_gameConfig.collisions;
	payload[2] = g_gameConfig.requirePassword;
	payload[3] = g_gameConfig.inProgressJoin;
	payload[4] = g_gameConfig.craftSelection;
	payload[5] = g_gameConfig.locatePlayers;
	payload[6] = g_gameConfig.lastTeamTimeLimit;
	payload[7] = g_gameConfig.eachTeamOwnRegion;
	payload[8] = g_gameConfig.environment;
	payload[9] = g_gameConfig.numberOfTeams;
	payload[10] = g_gameConfig.asyncFlag;
	payload[11] = g_gameConfig.laps;
	payload[12] = g_gameConfig.initialDistance;
	payload[13] = g_gameConfig.serverUpdateRate;
	payload[14] = g_gameConfig.maxPoints;
	g_frontendNetPacketScratch.packetType = 'Y';
	payload[15] = g_gameConfig.goalType;
	payload[16] = g_gameConfig.timeLimit;
	ByteOrder_WriteU32Le(&payload[17], (uint32_t)g_gameConfig.randomSeed);
	Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 0x19u);
	return 1;
}

// FUNCTION: XWA 0x5496C0
int MissionSetup_BroadcastReadyPlayerRosterPacket(int toPlayerId) {
	int packetDwordCount;
	int outCount;
	int playerIdx;
	NetPlayerInfo* playerRoster;

	g_frontendNetPacketScratch.packetType = 't';
	*(uint32_t*)&g_frontendNetPacketScratch.payload[0] = 8u;
	*(uint32_t*)&g_frontendNetPacketScratch.payload[4] = (uint32_t)Net_CountReadyPlayers();
	packetDwordCount = 3;
	playerRoster = Net_GetPlayerRoster(&outCount);
	playerIdx = 0;
	if (outCount > 0) {
		do {
			if (playerRoster[playerIdx].readyFlag == 1) {
				((uint32_t*)&g_frontendNetPacketScratch)[packetDwordCount++] =
					(uint32_t)playerRoster[playerIdx].playerId;
				((uint32_t*)&g_frontendNetPacketScratch)[packetDwordCount++] =
					(uint32_t)((uint8_t)playerRoster[playerIdx].playerName[0] - 1);
				((uint32_t*)&g_frontendNetPacketScratch)[packetDwordCount++] =
					Net_GetAverageLatencyMs(playerRoster[playerIdx].playerId);
			}
			++playerIdx;
		} while (playerIdx < outCount);
	}

	return Net_SendPacketAndFlush(toPlayerId, &g_frontendNetPacketScratch,
								  (unsigned int)(4 * packetDwordCount));
}

static __inline void MissionSetup_InitSkirmishLoadoutDefaults(int includeCraftOptions) {
	unsigned int slotIdx;
	int warheadOptionCount;

	for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
		if (!includeCraftOptions) {
			g_combatSimLoadoutOptions[slotIdx].optionalCraftCategory = 1;
		}

		g_combatSimLoadoutOptions[slotIdx].warheadOptions[0] = 0;
		g_combatSimLoadoutOptions[slotIdx].warheadOptions[1] = 3;
		g_combatSimLoadoutOptions[slotIdx].warheadOptions[2] = 4;
		g_combatSimLoadoutOptions[slotIdx].warheadOptions[3] = 2;
		g_combatSimLoadoutOptions[slotIdx].warheadOptions[4] = 1;
		g_combatSimLoadoutOptions[slotIdx].warheadOptions[5] = 5;
		g_combatSimLoadoutOptions[slotIdx].warheadOptions[6] = 6;
		g_combatSimLoadoutOptions[slotIdx].warheadOptions[7] = 7;
		g_combatSimLoadoutOptions[slotIdx].warheadOptions[8] = 8;
		warheadOptionCount = 9;
		g_combatSimLoadoutOptions[slotIdx].warheadOptionCount = warheadOptionCount;
		g_combatSimLoadoutOptions[slotIdx].selectedWarheadOption = 0;

		g_combatSimLoadoutOptions[slotIdx].beamOptions[0] = 0;
		g_combatSimLoadoutOptions[slotIdx].beamOptions[1] = 1;
		g_combatSimLoadoutOptions[slotIdx].beamOptions[2] = 2;
		g_combatSimLoadoutOptions[slotIdx].beamOptions[3] = 3;
		g_combatSimLoadoutOptions[slotIdx].beamOptionCount = 4;
		g_combatSimLoadoutOptions[slotIdx].selectedBeamOption = 0;

		g_combatSimLoadoutOptions[slotIdx].countermeasureOptions[0] = 0;
		g_combatSimLoadoutOptions[slotIdx].countermeasureOptions[1] = 1;
		g_combatSimLoadoutOptions[slotIdx].countermeasureOptions[2] = 2;
		g_combatSimLoadoutOptions[slotIdx].countermeasureOptionCount = 3;
		g_combatSimLoadoutOptions[slotIdx].selectedCountermeasureOption = 0;

		if (includeCraftOptions) {
			g_combatSimLoadoutOptions[slotIdx].optionalCraftCategory = 1;
			g_combatSimLoadoutOptions[slotIdx].selectedCraftOption = g_shipTypeToShipListIndex[1];
			g_combatSimLoadoutOptions[slotIdx].craftOptionCount = 255;
		}
	}
}

// FUNCTION: XWA 0x552370
int MissionSetup_LoadSkirmishFile(const char* fileName, int updateCurrentName) {
	uint32_t value;
	uint32_t fileVersion;
	XwaFile* stream;
	char path[256];

	sprintf(path, "%s\\%s", g_campaignDirNames[MISSION_DIRECTORY_SKIRMISH], fileName);
	stream = File_Open(AERON_VFS_ROOT_ASSET, path, "rb");
	if (stream != NULL) {

		File_ReadCount(stream, g_frontendScratchBuffer, 0x40u);
		File_ReadDword(stream, &value);
		if (value > 6) {
			File_Close(stream);
			return 0;
		}

		fileVersion = value;
		if (updateCurrentName) {
			strcpy(g_combatSimSkirmishFileName, fileName);
			g_combatSimSkirmishFileName[strlen(fileName) - 4] = '\0';
		}

		File_ReadDword(stream, &value);
		g_missionSetupPendingRandomSeed = (int)value;
		File_ReadCount(stream, g_combatSimSlots, sizeof(g_combatSimSlots));
		if (fileVersion >= 4) {
			File_ReadCount(stream, g_combatSimLoadoutOptions, sizeof(g_combatSimLoadoutOptions));
			MissionSetup_InitSkirmishLoadoutDefaults(0);
		} else {
			MissionSetup_InitSkirmishLoadoutDefaults(1);
		}

		File_ReadDword(stream, &value);
		g_gameConfig.numberOfTeams = (uint8_t)value;
		File_ReadDword(stream, &value);
		g_gameConfig.eachTeamOwnRegion = (uint8_t)value;
		File_ReadDword(stream, &value);
		g_gameConfig.environment = (uint8_t)value;
		File_ReadDword(stream, &value);
		g_gameConfig.initialDistance = (uint8_t)value;
		if (fileVersion >= 2) {
			File_ReadDword(stream, &value);
			g_gameConfig.maxPoints = (uint8_t)value;
		} else {
			g_gameConfig.maxPoints = 21;
		}

		if (fileVersion >= 3) {
			unsigned int teamIndex;

			for (teamIndex = 0; teamIndex < 10; ++teamIndex) {
				File_ReadDword(stream, &value);
				g_gameConfig.teamGoals[teamIndex] = (uint8_t)value;
			}
		} else {
			memset(g_gameConfig.teamGoals, 0, sizeof(g_gameConfig.teamGoals));
		}

		if (fileVersion >= 5) {
			File_ReadCount(stream, g_combatSimSlotNames, sizeof(g_combatSimSlotNames));
		}

		if (fileVersion >= 6) {
			File_ReadDword(stream, &value);
			g_gameConfig.goalType = (uint8_t)value;
		} else {
			g_gameConfig.goalType = 0;
		}

		Skirmish_InitMissionDefaults();
		g_frontendMission->header.hangar = XWA_HANGAR_SKIRMISH;
		g_frontendMission->formatVersion = 18;
		g_frontendMission->header.secondaryVersion = 98;
		g_frontendMission->flightGroupCount = 16;

		for (value = 0; value < 8; ++value) {
			strcpy(g_frontendMission->teams[value].name,
				   FrontendString_Get((UIString)(STR_GAME_ONE + value)));
		}

		for (value = 0; value < 16; ++value) {
			g_frontendMission->flightGroups[value].team =
				(uint8_t)(value / (16 / g_gameConfig.numberOfTeams));
		}

		MissionSetup_CountActiveTeams();
		MissionSetup_LoadMissionDescText(g_briefingText);
		for (value = 0; value < 16; ++value) {
			g_combatSimSlots[value].fgIndex = (uint16_t)value;
		}

		File_Close(stream);
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x5529B0
int MissionSetup_SaveSkirmishFile(const char* baseName, int interactiveSave) {
	XwaFile* existingStream;
	XwaFile* stream;
	int slotIdx;
	int loadoutIdx;
	int teamIdx;
	char path[256];
#ifdef XWA_MODERN
	char serializedBaseName[0x40];
#endif

	sprintf(path, "%s\\%s.skm", g_campaignDirNames[MISSION_DIRECTORY_SKIRMISH], baseName);
	existingStream = File_Open(AERON_VFS_ROOT_ASSET, path, "rb");
	if (existingStream != NULL) {
		File_Close(existingStream);
		if (!g_missionSetupSubpanelMode && interactiveSave) {
			g_missionSetupSubpanelMode = 1;
			return 0;
		}
	}

	stream = File_Open(AERON_VFS_ROOT_ASSET, path, "wb");
	if (stream == NULL) {
		return 0;
	}

#ifdef XWA_MODERN
	memset(serializedBaseName, 0, sizeof(serializedBaseName));
	strncpy(serializedBaseName, baseName, sizeof(serializedBaseName) - 1);
	File_WriteCount(stream, serializedBaseName, sizeof(serializedBaseName));
#else
	File_WriteCount(stream, baseName, 0x40u);
#endif
	File_WriteDword(stream, 6);
	File_WriteDword(stream, g_skirmishFileRandomSeed);

	for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
		memcpy(&g_selectedCombatSimSlot, &g_combatSimSlots[slotIdx], sizeof(g_selectedCombatSimSlot));
		g_selectedCombatSimSlot.ownerPlayerId = 0;
		g_selectedCombatSimSlot.gunnerPlayerId = 0;
		File_WriteCount(stream, &g_selectedCombatSimSlot, sizeof(g_selectedCombatSimSlot));
	}

	for (loadoutIdx = 0; loadoutIdx < 16; ++loadoutIdx) {
		File_WriteCount(stream, &g_combatSimLoadoutOptions[loadoutIdx],
						sizeof(g_combatSimLoadoutOptions[loadoutIdx]));
	}

	File_WriteDword(stream, g_gameConfig.numberOfTeams);
	File_WriteDword(stream, g_gameConfig.eachTeamOwnRegion);
	File_WriteDword(stream, g_gameConfig.environment);
	File_WriteDword(stream, g_gameConfig.initialDistance);
	if (interactiveSave && g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		File_WriteDword(stream, 21);
	} else {
		File_WriteDword(stream, g_gameConfig.maxPoints);
	}

	for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
		File_WriteDword(stream, g_gameConfig.teamGoals[teamIdx]);
	}

	File_WriteCount(stream, g_combatSimSlotNames, sizeof(g_combatSimSlotNames));
	File_WriteDword(stream, g_gameConfig.goalType);
	File_Close(stream);
	MissionSetup_LoadMissionList((MissionDirectoryId)g_pilotData.missionDirectoryId);
	return 1;
}

// FUNCTION: XWA 0x5527D0
int MissionSetup_UpdateSaveSkirmishDialog(void) {
	FrontendRect dialogRect;

	FrontendDraw_RectAssign(&dialogRect, 72, 75, 440, 348);
	FrontendText_Draw(10, FrontendString_Get(STR_DESCRIPTION), 204, 135, g_colorLightBlue);
	FrontendDraw_RectAssign(&dialogRect, 84, 150, 428, 165);
	FrontendDraw_FillRectTranslucent(&dialogRect, 0, 0, (unsigned int)g_colorSlateBlue);

	if (g_missionSetupSubpanelMode == 1) {
		FrontendText_Draw(10, g_missionSetupSaveNameBuffer, dialogRect.left + 2, dialogRect.top + 2,
						  g_colorPaleBlue);
		FrontendDraw_RectAssign(&dialogRect, 84, 175, 428, 195);
		FrontendText_Draw(10, FrontendString_Get(STR_GAME_SAVE_ALREADY_EXISTS), dialogRect.left + 2,
						  dialogRect.top, g_colorRed);
		return 1;
	}

	g_activeTextFieldId = 1;
	if (FrontendText_DrawEditableField(&dialogRect, g_missionSetupSaveNameBuffer, 63, 1, 10,
									   "\\*$~|:<>?/\t\".")) {
		MissionSetup_SaveSkirmishFile(g_missionSetupSaveNameBuffer, 1);
		strcpy(g_combatSimSkirmishFileName, g_missionSetupSaveNameBuffer);
		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && Net_IsHost()) {
			MissionSetup_BroadcastStatePacket(0);
			MissionSetup_BroadcastSkirmishMetadata();
		}

		g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
		g_frontendRightBarAnimState = 0;
		g_activeTextFieldId = 0;
		FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
	}

	return 1;
}

// FUNCTION: XWA 0x551FE0
int MissionSetup_IsSkirmishSetupValid(void) {
	int teamCraftCounts[10];
	int slotIdx;
	int activeTeamCount;
	int teamIdx;

	if ((g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER || Net_IsHost()) &&
		g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		memset(teamCraftCounts, 0, sizeof(teamCraftCounts));
		for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
			if (g_combatSimSlots[slotIdx].craftType) {
				++teamCraftCounts[g_frontendMission->flightGroups[(int16_t)g_combatSimSlots[slotIdx].fgIndex]
									  .team];
			}
		}

		activeTeamCount = 0;
		for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
			if (teamCraftCounts[teamIdx] && ++activeTeamCount > 1) {
				break;
			}
		}

		if (activeTeamCount < 2) {
			return 0;
		}
	}

	for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			if (g_combatSimSlots[slotIdx].ownerPlayerId == 1) {
				return MissionSetup_IsSlotWithinPointLimit(slotIdx);
			}
		} else {
			if (g_combatSimSlots[slotIdx].ownerPlayerId == Net_GetLocalPlayerId()) {
				return MissionSetup_IsSlotWithinPointLimit(slotIdx);
			}

			if (g_combatSimSlots[slotIdx].gunnerPlayerId == Net_GetLocalPlayerId()) {
				return g_combatSimSlots[slotIdx].ownerPlayerId != 0;
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x5487C0
int MissionSetup_DrawMissionDescriptionPanel(void) {
	int xOffset;
	int missionIdx;
	int labelWidth;
	int labelRight;
	int rectLeft;
	int rectRight;
	int visibleRows;
	int lineCount;
	FrontendRect rect;

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
		xOffset = 230;
	} else {
		xOffset = g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE ? 230 : 0;
	}

	FrontendDraw_RectAssign(&rect, xOffset + 70, 75, xOffset + 540, 348);
	if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_MELEE) {
		FrontendText_Draw(12, FrontendString_Get(STR_GAME_MISSION_STATISTICS), xOffset + 72, 258,
						  g_colorLightBlue);
		FrontendText_Draw(10, FrontendString_Get(STR_GAME_TOUR), xOffset + 182, 272, g_colorLightBlue);
		FrontendText_Draw(10, FrontendString_Get(STR_GAME_COMBAT_CHAMBER), xOffset + 242, 272,
						  g_colorLightBlue);

		missionIdx = g_missionList[g_selectedMissionListIndex].missionIdx;
		labelWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_GAME_NUMBER_TIMES_FLOWN), 10);
		labelRight = xOffset + labelWidth + 72;
		FrontendText_Draw(10, FrontendString_Get(STR_GAME_NUMBER_TIMES_FLOWN), xOffset + 72, 286,
						  g_colorLightBlue);
		sprintf(g_frontendScratchBuffer, "%d", g_pilotData.tourOfDutyMissions[missionIdx].numberTimesFlown);
		FrontendText_Draw(10, g_frontendScratchBuffer, xOffset + 182, 286, 0xffff);
		sprintf(g_frontendScratchBuffer, "%d",
				g_pilotData.combatChamberMissions[missionIdx].numberTimesFlown);
		FrontendText_Draw(10, g_frontendScratchBuffer, xOffset + 242, 286, 0xffff);

		FrontendText_DrawRightAligned(10, FrontendString_Get(STR_BEST_SCORE), labelRight, 300,
									  g_colorLightBlue);
		sprintf(g_frontendScratchBuffer, "%d", g_pilotData.tourOfDutyMissions[missionIdx].bestScore);
		FrontendText_Draw(10, g_frontendScratchBuffer, xOffset + 182, 300, 0xffff);
		if (g_pilotData.combatChamberMissions[missionIdx].numberTimesFlown) {
			sprintf(g_frontendScratchBuffer, "%d", g_pilotData.combatChamberMissions[missionIdx].bestScore);
		} else {
			strcpy(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(10, g_frontendScratchBuffer, xOffset + 242, 300, 0xffff);

		FrontendText_DrawRightAligned(10, FrontendString_Get(STR_BEST_BONUS), labelRight, 314,
									  g_colorLightBlue);
		sprintf(g_frontendScratchBuffer, "%d", g_pilotData.tourOfDutyMissions[missionIdx].bestBonus / 10);
		FrontendText_Draw(10, g_frontendScratchBuffer, xOffset + 182, 314, 0xffff);
		if (g_pilotData.combatChamberMissions[missionIdx].numberTimesFlown) {
			sprintf(g_frontendScratchBuffer, "%d",
					g_pilotData.combatChamberMissions[missionIdx].bestBonus / 10);
		} else {
			strcpy(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(10, g_frontendScratchBuffer, xOffset + 242, 314, 0xffff);

		FrontendText_DrawRightAligned(10, FrontendString_Get(STR_BEST_TIME), labelRight, 328,
									  g_colorLightBlue);
		if (g_pilotData.tourOfDutyMissions[missionIdx].numberTimesFlown &&
			g_pilotData.tourOfDutyMissions[missionIdx].completedCount == 1) {
			Frontend_FormatSecondsToClockString(
				(unsigned int)g_pilotData.tourOfDutyMissions[missionIdx].bestTime);
		} else {
			strcpy(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(10, g_frontendScratchBuffer, xOffset + 182, 328, 0xffff);
		if (g_pilotData.combatChamberMissions[missionIdx].numberTimesFlown &&
			g_pilotData.combatChamberMissions[missionIdx].completedCount == 1) {
			Frontend_FormatSecondsToClockString(
				(unsigned int)g_pilotData.combatChamberMissions[missionIdx].bestTime);
		} else {
			strcpy(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(10, g_frontendScratchBuffer, xOffset + 242, 328, 0xffff);
	}

	rectRight = xOffset + 340;
	rectLeft = xOffset + 70;
	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
		FrontendDraw_RectAssign(&rect, xOffset + 70, 107, xOffset + 340, 345);
		visibleRows = 17;
	} else {
		FrontendDraw_RectAssign(&rect, xOffset + 70, 107, xOffset + 340, 247);
		visibleRows = 10;
	}

	FrontendText_Draw(12, FrontendString_Get(STR_MISSION_DESCRIPTION), rectLeft, 90, g_colorLightBlue);
	if (FrontendText_DrawWrappedClipped(10, g_briefingText, &rect, g_colorLightBlue, 4, 4096) + 1 >
		visibleRows) {
		rect.right = xOffset + 320;
		lineCount = FrontendText_DrawWrappedClipped(10, g_briefingText, &rect, g_colorLightBlue, 4, 4096);
		rect.left = xOffset + 321;
		rect.right = rectRight;
		g_frontendFirstVisibleLine =
			FrontendScrollbar_Draw(&rect, g_frontendFirstVisibleLine, lineCount + 1, 0, 5, g_colorNavy, 9);
		rect.left = rectLeft;
		rect.right = xOffset + 320;
	}

	FrontendText_DrawWrappedClipped(10, g_briefingText, &rect, g_colorLightBlue, 4,
									g_frontendFirstVisibleLine);
	return 1;
}

// FUNCTION: XWA 0x54E2C0
int MissionSetup_DrawPlayerConnectionStats(FrontendRect* rect, int playerId) {
	int latencyMs;
	int cappedLatencyMs;
	int packetDropRateBasisPoints;
	int latencyPenalty;
	int dropPenaltySource;
	int connectionRating;
	int x;
	int y;
	int mouseX;
	int mouseY;

	if (playerId != Net_GetHostPlayerId()) {
		latencyMs = (int)Net_GetAverageLatencyMs(playerId);
		cappedLatencyMs = latencyMs;
		if (latencyMs > 749) {
			cappedLatencyMs = 749;
		}
		latencyMs = cappedLatencyMs;

		packetDropRateBasisPoints = Net_GetPacketDropRateBasisPoints(playerId);
		latencyPenalty = latencyMs - 100;
		if (latencyPenalty < 0) {
			latencyPenalty = 0;
		}
		latencyPenalty /= 25;

		dropPenaltySource = packetDropRateBasisPoints - 100;
		if (dropPenaltySource < 0) {
			dropPenaltySource = 0;
		}
		dropPenaltySource = 10 * dropPenaltySource / 100;

		connectionRating = 100 - dropPenaltySource - latencyPenalty;
		if (connectionRating < 0) {
			connectionRating = 0;
		}

		x = rect->left;
		y = rect->bottom - 1;
		FrontendDraw_Line(x, y, x + 9, y, g_colorGreen);
		y -= 2;
		if (connectionRating < 95) {
			FrontendDraw_Line(x, y, x + 9, y, g_colorGreen);
		}

		y -= 2;
		if (connectionRating < 90) {
			FrontendDraw_Line(x, y, x + 9, y, g_colorYellow);
		}

		y -= 2;
		if (connectionRating < 85) {
			FrontendDraw_Line(x, y, x + 9, y, g_colorYellow);
		}

		y -= 2;
		if (connectionRating < 80) {
			FrontendDraw_Line(x, y, x + 9, y, g_colorRed);
		}

		y -= 2;
		if (connectionRating < 75) {
			FrontendDraw_Line(x, y, x + 9, y, g_colorRed);
		}

		sprintf(g_frontendScratchBuffer, "%s: %d %s  |  %s: %d%%  |  %s: %d%%",
				FrontendString_Get(STR_LATENCY), cappedLatencyMs, FrontendString_Get(STR_MS),
				FrontendString_Get(STR_PACKETS_DROPPED), packetDropRateBasisPoints / 100,
				FrontendString_Get(STR_CONNECTION_RATING), connectionRating);
		FrontendCursor_GetPos(&mouseX, &mouseY);
		if (FrontendDraw_PointInRect(rect, mouseX, mouseY)) {
			FrontendCursor_SetLabel(g_frontendScratchBuffer);
			g_missionSetupConnectionStatsHoverActive = 1;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x548CF0
int MissionSetup_DrawUnassignedPlayersPanel(int panelMode) {
	int canDragPlayers;
	int hoveredRow;
	int mouseX;
	int mouseY;
	int rosterIdx;
	int slotIdx;
	int playerId;
	MpRosterEntry* rosterEntry;
	FrontendRect rect;
	FrontendRect connectionStatsRect;

	if (g_missionSetupActivePanel != MISSION_SETUP_PANEL_TEAM_ASSIGNMENT) {
		return 0;
	}

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
		g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
		return 0;
	}

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
		return 0;
	}

	canDragPlayers = 0;
	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER || Net_IsHost()) {
		canDragPlayers = 1;
	}

	FrontendCursor_GetPos(&mouseX, &mouseY);
	FrontendDraw_RectAssign(&rect, 480, 75, 570, 348);
	FrontendDisplay_SetScreenClipRect640x480(&rect);
	FrontendDraw_RectAssign(&rect, 480, 75, 570, 108);
	if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_MELEE) {
		FrontendText_DrawWrappedClipped(10, FrontendString_Get(STR_UNASSIGNED_PLAYERS), &rect,
										g_colorLightBlue, 4, 0);
	}

	hoveredRow = 0;
	FrontendDraw_RectAssign(&rect, 480, 109, 570, 136);
	for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
		rosterEntry = &g_mpRoster[rosterIdx];
		playerId = rosterEntry->playerId;
		if (playerId == 0) {
			continue;
		}

		for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
			if (g_combatSimSlots[slotIdx].ownerPlayerId == playerId ||
				g_combatSimSlots[slotIdx].gunnerPlayerId == playerId) {
				break;
			}
		}

		if (slotIdx < 16 || (canDragPlayers && playerId == g_missionSetupDraggedPlayerId)) {
			continue;
		}

		if (playerId == g_missionSetupDraggedPlayerId) {
			if (rosterEntry->rating || rosterEntry->name[0]) {
				FrontendText_Draw(10, FrontendString_Get((UIString)(rosterEntry->rating + 54)), rect.left,
								  rect.top, g_colorMidGray);
				sprintf(g_frontendScratchBuffer, "%s", rosterEntry->name);
				FrontendText_Draw(10, g_frontendScratchBuffer, rect.left, rect.top + 14, g_colorMidGray);
			} else {
				sprintf(g_frontendScratchBuffer, "%s", FrontendString_Get(STR_JOIN_IN_PROGRESS));
				FrontendText_Draw(10, g_frontendScratchBuffer, rect.left, rect.top, g_colorMidGray);
			}
		} else if (!rosterEntry->rating && !rosterEntry->name[0]) {
			sprintf(g_frontendScratchBuffer, "%s", FrontendString_Get(STR_JOIN_IN_PROGRESS));
			FrontendText_Draw(10, g_frontendScratchBuffer, rect.left, rect.top, g_colorYellow);
		} else {
			sprintf(g_frontendScratchBuffer, "%c%s", 6,
					FrontendString_Get((UIString)(rosterEntry->rating + 54)));
			FrontendText_Draw(10, g_frontendScratchBuffer, rect.left, rect.top, g_colorYellow);
			sprintf(g_frontendScratchBuffer, "%s", rosterEntry->name);
			if (playerId != Net_GetLocalPlayerId() &&
				g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
				FrontendText_Draw(10, g_frontendScratchBuffer, rect.left, rect.top + 14, g_colorYellow);
			} else {
				FrontendText_Draw(10, g_frontendScratchBuffer, rect.left, rect.top + 14,
								  g_pulseColorRamp[(panelMode % 24) >> 1]);
			}
		}

		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
			((g_gameConfig.networkType == NET_TRANSPORT_TCPIP && g_gameConfig.asyncFlag) ||
			 g_gameConfig.networkType == NET_TRANSPORT_MODEM)) {
			FrontendDraw_RectCopy(&connectionStatsRect, &rect);
			connectionStatsRect.left = connectionStatsRect.right - 10;
			connectionStatsRect.bottom = connectionStatsRect.top + 14;
			MissionSetup_DrawPlayerConnectionStats(&connectionStatsRect, playerId);
		}

		if (canDragPlayers && !FrontendMouse_IsGateOwner(2) && !g_missionSetupDraggedPlayerId &&
			FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			hoveredRow = rosterIdx + 1;
			if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
				g_missionSetupDraggedPlayerId = playerId;
				FrontendMouse_SetInputGate(2);
				FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor2", g_cursorBitmap);
				hoveredRow = 0;
				g_missionSetupPlayerDragState = 2;
				if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
					g_frontendNetPacketScratch.packetType = 75;
					ByteOrder_WriteU32Le(g_frontendNetPacketScratch.payload,
										 (uint32_t)g_missionSetupDraggedPlayerId);
					Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 8u);
				}
			}
		}

		FrontendDraw_RectOffsetXY(&rect, 0, 28);
	}

	if (hoveredRow) {
		FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor1", g_cursorBitmap);
		if (g_missionSetupPlayerDragState < 2) {
			g_missionSetupPlayerDragState = 1;
		}

		FrontendCursor_SetLabel(FrontendString_Get(STR_GAME_CLICK_TO_DRAG));
	}

	FrontendDisplay_ResetScreenClipRect();
	return 1;
}

// FUNCTION: XWA 0x552F40
int MissionSetup_DrawSkirmishGoalTypePanel(void) {
	int combatGoalWidth;
	int descriptionWidth;
	int combinedHalfWidth;
	int descriptionLineCount;
	int totalSlotTextHeight;
	int spacingBudget;
	int teamSpacing;
	int teamSpacingClamped;
	int teamTop;
	int leftEdge;
	int teamIdx;
	int teamNameWidth;
	int teamNameY;
	int lineY;
	int selectableMode;
	int slotIdx;
	int slotScan;
	int buttonResult;
	int goalType;
	int hostPlayerId;
	FrontendRect rect;
	FrontendRect clipRect;

	FrontendDraw_RectAssign(&rect, 65, 75, 575, 95);
	combatGoalWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_GAME_GOAL_COMBAT), 10);
	descriptionWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_GAME_GOAL_DESC), 10);
	combinedHalfWidth = (combatGoalWidth + descriptionWidth + 10) >> 1;
	FrontendText_Draw(10, FrontendString_Get(STR_GAME_GOAL_DESC), 320 - combinedHalfWidth, 100,
					  g_colorLightBlue);
	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_CLIENT) {
		FrontendText_Draw(10, FrontendString_Get((UIString)(g_gameConfig.goalType + STR_GAME_GOAL_COMBAT)),
						  descriptionWidth - combinedHalfWidth + 330, 100, g_colorLightBlue);
	} else if (FrontendButton_DrawMenuButton(
				   descriptionWidth - combinedHalfWidth + 330, 100,
				   FrontendString_Get((UIString)(g_gameConfig.goalType + STR_GAME_GOAL_COMBAT)), 10,
				   g_colorPaleBlue, 60, 0, (char*)"settingsound")) {
		g_gameConfig.goalType ^= 1u;
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_HOST) {
			MissionSetup_BroadcastStatePacket(0);
		}
	}

	FrontendDraw_RectAssign(&rect, 400, 120, 575, 348);
	descriptionLineCount = FrontendText_DrawWrappedClipped(
		10, FrontendString_Get((UIString)(g_gameConfig.goalType + STR_GAME_GOAL_COMBAT_DESC)), &rect,
		g_colorLightBlue, 4, 4096);
	rect.top += ((rect.bottom - rect.top + 1) >> 1) - ((14 * (descriptionLineCount + 1)) >> 1);
	FrontendText_DrawWrappedClipped(
		10, FrontendString_Get((UIString)(g_gameConfig.goalType + STR_GAME_GOAL_COMBAT_DESC)), &rect,
		g_colorLightBlue, 4, 0);

	if (g_gameConfig.goalType) {
		FrontendDraw_RectAssign(&rect, 74, 120, 478, 348);
		combatGoalWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_GAME_ALL_TEAMS), 10);
		teamNameY = ((rect.bottom - rect.top + 1) >> 1) + rect.top - 6;
		FrontendText_Draw(10, FrontendString_Get(STR_GAME_ALL_TEAMS), 70, teamNameY, g_colorLightBlue);
		FrontendDraw_Line(combatGoalWidth + 72, teamNameY + 5, 144, teamNameY + 5, g_colorLightBlue);
		FrontendText_Draw(10, FrontendString_Get(STR_GAME_GOAL1), 154, teamNameY, g_colorLightBlue);
		return 1;
	}

	FrontendDraw_RectAssign(&rect, 74, 120, 478, 348);
	spacingBudget = rect.bottom - rect.top - 7;
	totalSlotTextHeight = 0;
	for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
		if (g_combatSimSlots[slotIdx].fgIndex != 0xffff) {
			totalSlotTextHeight += 14;
		}
	}

	if (g_teamCount > 1) {
		teamSpacing = 0;
		teamSpacingClamped = (spacingBudget - totalSlotTextHeight) / (g_teamCount - 1);
	} else {
		teamSpacingClamped = 0;
		teamSpacing = (spacingBudget - totalSlotTextHeight) / 2;
	}

	if (teamSpacingClamped > 28) {
		teamSpacing = (teamSpacingClamped - 28) >> 1;
		teamSpacingClamped = 28;
	}

	leftEdge = rect.left;
	teamTop = teamSpacing + rect.top;
	for (teamIdx = 0; teamIdx < g_teamCount; ++teamIdx) {
		int fgHeight;

		fgHeight = 14 * g_teamFgCountScratch[teamIdx];
		teamNameY = (fgHeight >> 1) + teamTop - 7;
		teamNameWidth = FrontendText_MeasureWidth(g_frontendMission->teams[teamIdx].name, 10);
		FrontendDraw_RectAssign(&clipRect, leftEdge, teamNameY, leftEdge + 80, teamNameY + 14);
		FrontendDisplay_SetScreenClipRect640x480(&clipRect);
		FrontendText_Draw(10, g_frontendMission->teams[teamIdx].name, leftEdge, teamNameY, g_colorLightBlue);
		FrontendDisplay_ResetScreenClipRect();

		lineY = teamNameY + 5;
		if (72 - teamNameWidth > 0) {
			FrontendDraw_Line(teamNameWidth + leftEdge + 2, lineY, leftEdge + 70, lineY, g_colorLightBlue);
		}

		selectableMode = 0;
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			selectableMode = 1;
		} else {
			switch (g_gameConfig.craftSelection) {
				case 0:
				case 2:
					if (Net_IsHost()) {
						selectableMode = 1;
					}
					break;
				case 1:
					selectableMode = FrontendNet_IsTeamLocalPlayer(teamIdx);
					if (!selectableMode) {
						for (slotScan = 0; slotScan < 16; ++slotScan) {
							if (g_frontendMission->flightGroups[(int16_t)g_combatSimSlots[slotScan].fgIndex]
										.team == teamIdx &&
								(g_combatSimSlots[slotScan].ownerPlayerId ||
								 g_combatSimSlots[slotScan].gunnerPlayerId)) {
								break;
							}
						}

						selectableMode = slotScan >= 16 && Net_IsHost();
					} else if (!Net_IsHost()) {
						selectableMode = 2;
					}
					break;
				default:
					selectableMode = 1;
					break;
			}
		}

		teamNameY = lineY - 5;
		if (selectableMode) {
			if (selectableMode == 1) {
				buttonResult = FrontendButton_DrawMenuButton(
					154, teamNameY,
					FrontendString_Get((UIString)(g_gameConfig.teamGoals[teamIdx] + STR_GAME_GOAL1)), 10,
					g_colorPaleBlue, teamIdx + 50, 0, (char*)"settingsound");
				if (buttonResult == 1) {
					++g_gameConfig.teamGoals[teamIdx];
					if (g_gameConfig.numberOfTeams > 2u) {
						if (g_gameConfig.teamGoals[teamIdx] >= 2u) {
							g_gameConfig.teamGoals[teamIdx] = 0;
						}
					} else if (g_gameConfig.teamGoals[teamIdx] >= 5u) {
						g_gameConfig.teamGoals[teamIdx] = 0;
					}

					if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						MissionSetup_BroadcastStatePacket(0);
					}
				} else if (buttonResult == 2) {
					uint8_t teamGoal = g_gameConfig.teamGoals[teamIdx];
					if (teamGoal == 0) {
						teamGoal = g_gameConfig.numberOfTeams > 2u ? 1 : 4;
					} else {
						teamGoal = teamGoal - 1;
					}

					g_gameConfig.teamGoals[teamIdx] = teamGoal;
					if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						MissionSetup_BroadcastStatePacket(0);
					}
				}
			} else {
				buttonResult = FrontendButton_DrawMenuButton(
					154, teamNameY,
					FrontendString_Get((UIString)(g_missionSetupRemoteTeamGoalType + STR_GAME_GOAL1)), 10,
					g_colorPaleBlue, teamIdx + 50, 0, (char*)"settingsound");
				if (buttonResult == 1) {
					goalType = ++g_missionSetupRemoteTeamGoalType;
					if (g_gameConfig.numberOfTeams > 2u) {
						if (goalType >= 2) {
							goalType = 0;
							g_missionSetupRemoteTeamGoalType = goalType;
						}
					} else if (goalType >= 5) {
						goalType = 0;
						g_missionSetupRemoteTeamGoalType = goalType;
					}

					g_frontendNetPacketScratch.packetType = 67;
					*(uint32_t*)g_frontendNetPacketScratch.payload = (uint32_t)teamIdx;
					*(uint32_t*)&g_frontendNetPacketScratch.payload[4] = (uint32_t)goalType;
					hostPlayerId = Net_GetHostPlayerId();
					Net_SendPacketAndFlush(hostPlayerId, &g_frontendNetPacketScratch, 0xCu);
				} else if (buttonResult == 2) {
					if (g_missionSetupRemoteTeamGoalType == 0) {
						goalType = g_gameConfig.numberOfTeams > 2u ? 1 : 4;
					} else {
						goalType = g_missionSetupRemoteTeamGoalType - 1;
					}

					g_missionSetupRemoteTeamGoalType = goalType;
					g_frontendNetPacketScratch.packetType = 67;
					*(uint32_t*)g_frontendNetPacketScratch.payload = (uint32_t)teamIdx;
					*(uint32_t*)&g_frontendNetPacketScratch.payload[4] = (uint32_t)goalType;
					hostPlayerId = Net_GetHostPlayerId();
					Net_SendPacketAndFlush(hostPlayerId, &g_frontendNetPacketScratch, 0xCu);
				}
			}
		} else {
			FrontendText_Draw(
				10, FrontendString_Get((UIString)(g_gameConfig.teamGoals[teamIdx] + STR_GAME_GOAL1)), 154,
				teamNameY, g_colorLightBlue);
		}

		teamTop += fgHeight + teamSpacingClamped;
	}

	return 1;
}

// FUNCTION: XWA 0x54FD00
int MissionSetup_DrawSelectedSlotEditPanel(int panelMode) {
	int top;
	int left;
	int xRight;
	int valueLeft;
	int mouseX;
	int mouseY;
	int flightGroupIdx;
	int team;
	int canSelectCraft;
	int canEditSlotName;
	int slotIdx;
	int teamPointTotal;
	int selectedPointTotal;
	int category;
	int noCraftCountChange;
	int noWaveChange;
	int unlimitedWaves;
	int supportsBeamAndCountermeasures;
	int buttonResult;
	int rosterIdx;
	int textX;
	int y;
	int labelWidth;
	int warheadCount;
	const char* modelLongName;
	const char* text;
	CombatSimLoadoutOptions* loadoutOptions;
	FrontendRect rect;
	FrontendRect previewRect;
	CraftTechStats stats;
	char buffer[128];

	FrontendDraw_RectAssign(&rect, 74, 93, 570, 348);
	top = rect.top;
	left = rect.left;
	xRight = rect.left + 140;
	FrontendCursor_GetPos(&mouseX, &mouseY);
	loadoutOptions = &g_combatSimLoadoutOptions[g_selectedCombatSimSlotIdx];
	flightGroupIdx = (int16_t)g_selectedCombatSimSlot.fgIndex;
	team = g_frontendMission->flightGroups[flightGroupIdx].team;
	canSelectCraft = MissionSetup_CanSelectSlotCraft(&g_selectedCombatSimSlot);
	canEditSlotName = g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH && canSelectCraft;

	if (g_missionSetupSubpanelMode && canSelectCraft) {
		if (g_missionSetupSubpanelMode == 1) {
			int categoryIdx;
			int categoryY;
			unsigned int shipIdx;

			categoryY = 90;
			for (categoryIdx = 0; categoryIdx < 12; ++categoryIdx) {
				if (g_missionSetupSelectedCraftCategory == categoryIdx) {
					if (categoryIdx) {
						FrontImage_DrawSpriteTranslucent("leftframe", 70, categoryY - 4);
					}

					FrontendText_Draw(10, FrontendString_Get((UIString)(categoryIdx + STR_SKIRMISH_NOT_USED)),
									  70, categoryY, g_colorGreen);
				} else if (FrontendButton_DrawMenuButton(
							   70, categoryY,
							   FrontendString_Get((UIString)(categoryIdx + STR_SKIRMISH_NOT_USED)), 10,
							   g_colorPaleBlue, categoryIdx + 50, 0, "settingsound")) {
					g_missionSetupSelectedCraftCategory = categoryIdx;
					if (!categoryIdx) {
						g_missionSetupSubpanelMode = 0;
						MissionSetup_UnloadShipBmp();
						MissionSetup_DrawBackgroundAndPreview(0);
						g_selectedCombatSimSlot.craftType = 0;
					}

					g_missionSetupCraftListScrollOffset = 0;
					g_missionSetupFilteredCraftCount = 0;
					g_missionSetupDisplayedCraftOrdinal = 0;
					for (shipIdx = 0; shipIdx < (unsigned int)g_shipCount;
						 g_missionSetupDisplayedCraftOrdinal = ++shipIdx) {
						if (g_shipList[shipIdx].category == g_missionSetupSelectedCraftCategory) {
							++g_missionSetupFilteredCraftCount;
						}
					}
				}

				categoryY += 20;
			}

			if (g_missionSetupSelectedCraftCategory) {
				MissionSetup_DrawSelectedSlotCraftList(0, loadoutOptions, &stats, &rect);
			}
		} else if (g_missionSetupSubpanelMode == 2) {
			MissionSetup_DrawSelectedSlotCraftList(1, loadoutOptions, &stats, &rect);
		} else if (g_missionSetupSubpanelMode == 3) {
			unsigned int shipIdx;

			if (g_selectedCombatSimSlot.ownerPlayerId || g_selectedCombatSimSlot.gunnerPlayerId) {
				g_missionSetupCraftListScrollOffset = 0;
				g_missionSetupFilteredCraftCount = 0;
				g_missionSetupDisplayedCraftOrdinal = 0;
				for (shipIdx = 0; shipIdx < (unsigned int)g_shipCount;
					 g_missionSetupDisplayedCraftOrdinal = (int)++shipIdx) {
					if (g_shipList[shipIdx].flyable && g_shipList[shipIdx].skirmish) {
						++g_missionSetupFilteredCraftCount;
					}
				}
				g_missionSetupSubpanelMode = 2;
				MissionSetup_DrawBackgroundAndPreview(0);
			} else {
				g_missionSetupSelectedCraftCategory =
					g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].category;
				g_missionSetupCraftListScrollOffset = 0;
				g_missionSetupFilteredCraftCount = 0;
				g_missionSetupDisplayedCraftOrdinal = 0;
				for (shipIdx = 0; shipIdx < (unsigned int)g_shipCount;
					 g_missionSetupDisplayedCraftOrdinal = (int)++shipIdx) {
					if (g_shipList[shipIdx].category == g_missionSetupSelectedCraftCategory &&
						g_shipList[shipIdx].skirmish) {
						++g_missionSetupFilteredCraftCount;
					}
				}
				g_missionSetupSubpanelMode = 1;
				MissionSetup_DrawBackgroundAndPreview(0);
			}
		}

		FrontendDisplay_ResetScreenClipRect();
		return 1;
	}

	if (g_missionSetupSubpanelMode) {
		g_missionSetupSubpanelMode = 0;
		MissionSetup_DrawBackgroundAndPreview(0);
	}

	{
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
			g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
			g_activeTextFieldId = 0;
			g_frontendRightBarAnimState = 0;
			FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
				if (canSelectCraft && MissionSetup_SelectedSlotHasChanges()) {
					g_frontendNetPacketScratch.packetType = 76;
					memcpy(g_frontendNetPacketScratch.payload, &g_selectedCombatSimSlotIdx,
						   sizeof(g_selectedCombatSimSlotIdx));
					memcpy(&g_frontendNetPacketScratch.payload[4], &g_selectedCombatSimSlot,
						   sizeof(g_selectedCombatSimSlot));
					Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 0x1Cu);
				}

				if (canEditSlotName && g_selectedCombatSimSlotNameEditBuffer[0] &&
					strcmp(g_selectedCombatSimSlotNameEditBuffer,
						   g_combatSimSlotNames[g_selectedCombatSimSlotIdx]) != 0) {
					g_frontendNetPacketScratch.packetType = 72;
					memcpy(g_frontendNetPacketScratch.payload, &g_selectedCombatSimSlotIdx,
						   sizeof(g_selectedCombatSimSlotIdx));
					memcpy(&g_frontendNetPacketScratch.payload[4], g_selectedCombatSimSlotNameEditBuffer,
						   sizeof(g_combatSimSlotNames[g_selectedCombatSimSlotIdx]));
					Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 0x1Cu);
				}
			}

			if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
				memcpy(&g_combatSimSlots[g_selectedCombatSimSlotIdx], &g_selectedCombatSimSlot,
					   sizeof(g_selectedCombatSimSlot));
				if (g_selectedCombatSimSlotNameEditBuffer[0]) {
					memcpy(g_combatSimSlotNames[g_selectedCombatSimSlotIdx],
						   g_selectedCombatSimSlotNameEditBuffer,
						   sizeof(g_combatSimSlotNames[g_selectedCombatSimSlotIdx]));
				}
			}

			if (MissionSetup_IsSkirmishSetupValid()) {
				if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
					g_frontendRightBarPanelIndex =
						g_pilotData.missionDirectoryId != MISSION_DIRECTORY_TOUR ? 2 : 4;
				} else {
					g_frontendRightBarPanelIndex = 2;
				}
			} else {
				g_frontendRightBarPanelIndex = 1;
			}
		}

		text = FrontendString_Get(STR_GAME_GROUP);
		labelWidth = FrontendText_MeasureWidth(text, 10);
		FrontendText_Draw(10, FrontendString_Get(STR_GAME_GROUP), left, top, g_colorLightBlue);
		if (canEditSlotName) {
			FrontendDraw_RectAssign(&rect, labelWidth + left + 7, top - 3, xRight - 10, top + 12);
			if (FrontendText_DrawEditableField(&rect, g_selectedCombatSimSlotNameEditBuffer, 10, 1, 10,
											   "\\*$~|:<>?/\t\"")) {
				g_activeTextFieldId = 0;
			}
		} else {
			FrontendText_Draw(10, g_selectedCombatSimSlotNameEditBuffer, labelWidth + left + 10, top,
							  g_colorLightBlue);
		}

		text = FrontendString_Get(STR_GAME_TEAM);
		labelWidth = FrontendText_MeasureWidth(text, 10);
		valueLeft = xRight + 10;
		FrontendText_Draw(10, FrontendString_Get(STR_GAME_TEAM), valueLeft, top, g_colorLightBlue);
		FrontendText_Draw(10, g_frontendMission->teams[team].name, xRight + labelWidth + 20, top,
						  g_colorLightBlue);

		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			top += 15;
			teamPointTotal = 0;
			for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
				if (slotIdx != g_selectedCombatSimSlotIdx && g_combatSimSlots[slotIdx].craftType != 0 &&
					g_frontendMission->flightGroups[(int16_t)g_combatSimSlots[slotIdx].fgIndex].team ==
						team) {
					selectedPointTotal =
						MissionSetup_ComputeCraftPointTotal(
							g_combatSimSlots[slotIdx].craftType, g_combatSimSlots[slotIdx].warhead,
							g_combatSimSlots[slotIdx].beam, g_combatSimSlots[slotIdx].countermeasures,
							g_combatSimSlots[slotIdx].groupAI, g_combatSimSlots[slotIdx].craftRole) *
						g_combatSimSlots[slotIdx].numberOfCraft;
					if (g_gameConfig.goalType != 1 ||
						g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH ||
						!g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIdx].craftType]].flyable) {
						selectedPointTotal *= g_combatSimSlots[slotIdx].numberOfWaves + 1;
					}

					teamPointTotal += selectedPointTotal;
				}
			}

			selectedPointTotal =
				MissionSetup_ComputeSelectedSlotPointTotalForCraft(g_selectedCombatSimSlot.craftType);
			teamPointTotal += selectedPointTotal;
			sprintf(g_frontendScratchBuffer, "%s %d", FrontendString_Get(STR_GAME_POINT_VALUE),
					selectedPointTotal);
			FrontendText_Draw(10, g_frontendScratchBuffer, left, top, g_colorLightBlue);
			if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER ||
				g_gameConfig.maxPoints == 21) {
				sprintf(g_frontendScratchBuffer, "%s %d", FrontendString_Get(STR_GAME_TEAM_POINTS),
						teamPointTotal);
				FrontendText_Draw(10, g_frontendScratchBuffer, left + 150, top, g_colorLightBlue);
			} else {
				sprintf(g_frontendScratchBuffer, "%s %d  (%s: %d)", FrontendString_Get(STR_GAME_TEAM_POINTS),
						teamPointTotal, FrontendString_Get(STR_GAME_MAX), 500 * g_gameConfig.maxPoints);
				if (teamPointTotal <= 500 * g_gameConfig.maxPoints) {
					FrontendText_Draw(10, g_frontendScratchBuffer, left + 150, top, g_colorLightBlue);
				} else {
					FrontendText_Draw(10, g_frontendScratchBuffer, left + 150, top, g_colorRed);
				}
			}
		}

		y = top + 30;
		if (g_selectedCombatSimSlot.ownerPlayerId || g_selectedCombatSimSlot.gunnerPlayerId) {
			text = FrontendString_Get(STR_GAME_PILOTED_BY);
			labelWidth = FrontendText_MeasureWidth(text, 10);
			FrontendText_Draw(10, FrontendString_Get(STR_GAME_PILOTED_BY), left, y, g_colorLightBlue);
			textX = left + labelWidth + 10;
			rosterIdx = MissionSetup_FindRosterIndexByPlayerId(g_selectedCombatSimSlot.ownerPlayerId);
			if (rosterIdx < 8) {
				if (g_mpRoster[rosterIdx].rating || g_mpRoster[rosterIdx].name[0]) {
					sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
							FrontendString_Get((UIString)(g_mpRoster[rosterIdx].rating + 54)), 1,
							g_mpRoster[rosterIdx].name);
				} else {
					sprintf(g_frontendScratchBuffer, "%s", FrontendString_Get(STR_JOIN_IN_PROGRESS));
				}

				if (g_mpRoster[rosterIdx].playerId == Net_GetLocalPlayerId() ||
					g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
					FrontendText_Draw(10, g_frontendScratchBuffer, textX, y,
									  g_pulseColorRamp[(panelMode % 24) >> 1]);
				} else {
					FrontendText_Draw(10, g_frontendScratchBuffer, textX, y, g_colorYellow);
				}
			} else {
				if (g_selectedCombatSimSlot.ownerPlayerId) {
					sprintf(g_frontendScratchBuffer, "%s", FrontendString_Get(STR_JOIN_IN_PROGRESS));
					FrontendText_Draw(10, g_frontendScratchBuffer, textX, y, g_colorYellow);
				} else {
					g_frontendScratchBuffer[0] = '\0';
				}
			}

			if (g_selectedCombatSimSlot.gunnerPlayerId) {
				rosterIdx = MissionSetup_FindRosterIndexByPlayerId(g_selectedCombatSimSlot.gunnerPlayerId);
				if (g_frontendScratchBuffer[0]) {
					textX += FrontendText_MeasureWidth(g_frontendScratchBuffer, 10) + 20;
				}

				if (rosterIdx < 8) {
					FrontendText_Draw(10, FrontendString_Get(STR_GAME_GUNNER), textX, y, g_colorLightBlue);
					labelWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_GAME_GUNNER), 10);
					if (g_mpRoster[rosterIdx].rating || g_mpRoster[rosterIdx].name[0]) {
						sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
								FrontendString_Get((UIString)(g_mpRoster[rosterIdx].rating + 54)), 1,
								g_mpRoster[rosterIdx].name);
					} else {
						sprintf(g_frontendScratchBuffer, "%s", FrontendString_Get(STR_JOIN_IN_PROGRESS));
					}

					if (g_mpRoster[rosterIdx].playerId == Net_GetLocalPlayerId() ||
						g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						FrontendText_Draw(10, g_frontendScratchBuffer, textX + labelWidth, y,
										  g_pulseColorRamp[(panelMode % 24) >> 1]);
					} else {
						FrontendText_Draw(10, g_frontendScratchBuffer, textX + labelWidth, y, g_colorYellow);
					}
				} else {
					sprintf(g_frontendScratchBuffer, "%s %c%s", FrontendString_Get(STR_GAME_GUNNER), 6,
							FrontendString_Get(STR_JOIN_IN_PROGRESS));
					FrontendText_Draw(10, g_frontendScratchBuffer, textX, y, g_colorLightBlue);
				}
			}

			y += 30;
			FrontendText_DrawRightAligned(10, FrontendString_Get(STR_GAME_WINGMAN_SKILL_LEVEL), xRight, y,
										  g_colorLightBlue);
			strcpy(g_frontendScratchBuffer,
				   FrontendString_Get((UIString)(g_selectedCombatSimSlot.groupAI + STR_SKIRMISH_NOVICE)));
			if (!canSelectCraft) {
				FrontendText_Draw(10, g_frontendScratchBuffer, valueLeft, y, g_colorLightBlue);
				goto draw_species;
			}
		} else {
			FrontendText_DrawRightAligned(10, FrontendString_Get(STR_GAME_PILOTED_BY), xRight, y,
										  g_colorLightBlue);
			if (g_selectedCombatSimSlot.craftType) {
				strcpy(g_frontendScratchBuffer,
					   FrontendString_Get((UIString)(g_selectedCombatSimSlot.groupAI + STR_SKIRMISH_NOVICE)));
				if (!canSelectCraft) {
					FrontendText_Draw(10, g_frontendScratchBuffer, valueLeft, y, g_colorLightBlue);
					goto draw_species;
				}
			} else {
				FrontendText_Draw(10, FrontendString_Get(STR_GAME_NOBODY), valueLeft, y, g_colorLightBlue);
				goto draw_species;
			}
		}

		buttonResult = FrontendButton_DrawMenuButton(valueLeft, y, g_frontendScratchBuffer, 10,
													 g_colorPaleBlue, 50, 0, "settingsound");
		if (buttonResult == 1) {
			if (++g_selectedCombatSimSlot.groupAI > 5u) {
				g_selectedCombatSimSlot.groupAI = 0;
			}
		} else if (buttonResult == 2) {
			if (!g_selectedCombatSimSlot.groupAI) {
				g_selectedCombatSimSlot.groupAI = 5;
			} else {
				--g_selectedCombatSimSlot.groupAI;
			}
		}

	draw_species:
		memset(&stats, 0, sizeof(stats));
		y += 15;
		stats.craftType = g_selectedCombatSimSlot.craftType;
		MissionSetup_GetCachedCraftTechStats(&stats);
		FrontendText_DrawRightAligned(10, FrontendString_Get(STR_GAME_SPECIES), xRight, y, g_colorLightBlue);
		if (g_selectedCombatSimSlot.craftType <= 0 ||
			(modelLongName = GetCraftTypeModelLongName(g_selectedCombatSimSlot.craftType)) == NULL) {
			sprintf(g_frontendScratchBuffer, "%s",
					g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].name);
		} else {
			sprintf(g_frontendScratchBuffer, "%s (%s)",
					g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].name,
					modelLongName);
		}

		if (canSelectCraft) {
			buttonResult = FrontendButton_DrawMenuButton(valueLeft, y, g_frontendScratchBuffer, 10,
														 g_colorPaleBlue, 51, 0, "settingsound");
			FrontendDraw_RectAssign(&previewRect, 395, 151, 548, 302);
			if (FrontendDraw_PointInRect(&previewRect, mouseX, mouseY) &&
				(FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick())) {
				FrontendMouse_ClearClicks();
				FrontendSound_PlayUISound((char*)"settingsound", 1, 0, 255,
										  12 * g_gameConfig.sfxDatapadVolume, 63);
				buttonResult = 1;
			}

			if (buttonResult) {
				if (loadoutOptions->optionalCraftCategory == 1 ||
					g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
					g_missionSetupSubpanelMode = 3;
				} else {
					if (++loadoutOptions->selectedCraftOption >= loadoutOptions->craftOptionCount) {
						loadoutOptions->selectedCraftOption = 0;
					}

					g_selectedCombatSimSlot.craftType =
						(uint16_t)loadoutOptions->craftTypeOptions[loadoutOptions->selectedCraftOption];
					g_selectedCombatSimSlot.numberOfCraft =
						(uint8_t)loadoutOptions->craftCountOptions[loadoutOptions->selectedCraftOption];
					g_selectedCombatSimSlot.numberOfWaves =
						(uint8_t)loadoutOptions->craftWaveOptions[loadoutOptions->selectedCraftOption];
				}
			}
		} else {
			FrontendText_Draw(10, g_frontendScratchBuffer, valueLeft, y, g_colorLightBlue);
		}

		y += 15;
		FrontendText_DrawRightAligned(10, FrontendString_Get(STR_GAME_CRAFT_COUNT), xRight, y,
									  g_colorLightBlue);
		category = g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].category;
		noCraftCountChange = category == 6 || category == 7;
		noWaveChange = category == 8 || category == 4;
		sprintf(g_frontendScratchBuffer, "%-30d", g_selectedCombatSimSlot.numberOfCraft);
		if (canSelectCraft && g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH &&
			!noCraftCountChange) {
			buttonResult = FrontendButton_DrawMenuButton(valueLeft, y, g_frontendScratchBuffer, 10,
														 g_colorPaleBlue, 52, 0, "settingsound");
			if (buttonResult == 1) {
				if (++g_selectedCombatSimSlot.numberOfCraft > 6u) {
					g_selectedCombatSimSlot.numberOfCraft = 1;
				}
			} else if (buttonResult == 2) {
				if (g_selectedCombatSimSlot.numberOfCraft <= 1u) {
					g_selectedCombatSimSlot.numberOfCraft = 6;
				} else {
					--g_selectedCombatSimSlot.numberOfCraft;
				}
			}
		} else {
			FrontendText_Draw(10, g_frontendScratchBuffer, valueLeft, y, g_colorLightBlue);
		}

		y += 15;
		FrontendText_DrawRightAligned(10, FrontendString_Get(STR_GAME_WAVES), xRight, y, g_colorLightBlue);
		unlimitedWaves = 0;
		if (g_selectedCombatSimSlot.numberOfWaves == 99) {
			strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_GAME_UNLIMITED));
			unlimitedWaves = 1;
		} else {
			if (g_gameConfig.goalType == 1 && g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				if (g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].flyable) {
					strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_GAME_UNLIMITED));
					unlimitedWaves = 1;
				} else {
					sprintf(g_frontendScratchBuffer, "%-30d", g_selectedCombatSimSlot.numberOfWaves + 1);
				}
			} else {
				sprintf(g_frontendScratchBuffer, "%-30d", g_selectedCombatSimSlot.numberOfWaves + 1);
			}
		}

		if (!canSelectCraft || g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH ||
			noCraftCountChange || noWaveChange || unlimitedWaves) {
			FrontendText_Draw(10, g_frontendScratchBuffer, valueLeft, y, g_colorLightBlue);
		} else {
			buttonResult = FrontendButton_DrawMenuButton(valueLeft, y, g_frontendScratchBuffer, 10,
														 g_colorPaleBlue, 53, 0, "settingsound");
			if (buttonResult == 1) {
				if (++g_selectedCombatSimSlot.numberOfWaves >= 19u) {
					g_selectedCombatSimSlot.numberOfWaves = 0;
				}
			} else if (buttonResult == 2) {
				if (!g_selectedCombatSimSlot.numberOfWaves) {
					g_selectedCombatSimSlot.numberOfWaves = 18;
				} else {
					--g_selectedCombatSimSlot.numberOfWaves;
				}
			}
		}

		y += 15;
		FrontendText_DrawRightAligned(10, FrontendString_Get(STR_GAME_WARHEADS), xRight, y, g_colorLightBlue);
		if (!stats.warheadRating) {
			FrontendText_Draw(10, FrontendString_Get(STR_NONE), valueLeft, y, g_colorLightBlue);
		} else {
			if (g_selectedCombatSimSlot.warhead) {
				if (g_selectedCombatSimSlot.craftType == 12 || g_selectedCombatSimSlot.craftType == 113) {
					stats.warheadRating = (unsigned int)stats.warheadRating >> 2;
				} else {
					stats.warheadRating = (unsigned int)stats.warheadRating >> 1;
				}
				warheadCount = stats.warheadRating;

				switch (g_selectedCombatSimSlot.warhead) {
					case 2:
						warheadCount = 2 * warheadCount / 4;
						if (!warheadCount) {
							warheadCount = 1;
						}
						break;

					case 4:
					case 6:
						warheadCount *= 3;
						/* Fall through. */

					case 1:
						warheadCount /= 4;
						if (!warheadCount) {
							warheadCount = 1;
						}
						break;

					case 3:
					case 5:
					case 7:
						if (!warheadCount) {
							warheadCount = 1;
						}
						break;

					case 8:
						if (!warheadCount) {
							warheadCount = 1;
						}
						break;

					default:
						break;
				}

				warheadCount <<= 1;
				if (g_selectedCombatSimSlot.craftType == 12) {
					warheadCount += 40;
				} else if (g_selectedCombatSimSlot.craftType == 113) {
					warheadCount += 6;
				}

				sprintf(buffer, "%s (%d)",
						FrontendString_Get((UIString)(g_selectedCombatSimSlot.warhead + STR_NONE)),
						warheadCount);
				sprintf(g_frontendScratchBuffer, "%-30s", buffer);
			} else {
				sprintf(g_frontendScratchBuffer, "%-30s", FrontendString_Get(STR_NONE));
			}

			if (!canSelectCraft || loadoutOptions->warheadOptionCount <= 1) {
				FrontendText_Draw(10, g_frontendScratchBuffer, valueLeft, y, g_colorLightBlue);
			} else {
				buttonResult = FrontendButton_DrawMenuButton(valueLeft, y, g_frontendScratchBuffer, 10,
															 g_colorPaleBlue, 54, 0, "settingsound");
				if (buttonResult == 1) {
					if (++loadoutOptions->selectedWarheadOption >= loadoutOptions->warheadOptionCount) {
						loadoutOptions->selectedWarheadOption = 0;
					}

					g_selectedCombatSimSlot.warhead =
						(uint8_t)loadoutOptions->warheadOptions[loadoutOptions->selectedWarheadOption];
				} else if (buttonResult == 2) {
					if (!loadoutOptions->selectedWarheadOption) {
						loadoutOptions->selectedWarheadOption = loadoutOptions->warheadOptionCount - 1;
					} else {
						--loadoutOptions->selectedWarheadOption;
					}
					g_selectedCombatSimSlot.warhead =
						(uint8_t)loadoutOptions->warheadOptions[loadoutOptions->selectedWarheadOption];
				}
			}
		}

		y += 15;
		category = g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].category;
		supportsBeamAndCountermeasures = category == 1 || category == 2;
		FrontendText_DrawRightAligned(10, FrontendString_Get(STR_GAME_BEAM_WEAPON), xRight, y,
									  g_colorLightBlue);
		sprintf(g_frontendScratchBuffer, "%-30s",
				FrontendString_Get((UIString)(g_selectedCombatSimSlot.beam + STR_BEAM_NONE)));
		if (!canSelectCraft || loadoutOptions->beamOptionCount <= 1 || !supportsBeamAndCountermeasures) {
			FrontendText_Draw(10, g_frontendScratchBuffer, valueLeft, y, g_colorLightBlue);
		} else {
			buttonResult = FrontendButton_DrawMenuButton(valueLeft, y, g_frontendScratchBuffer, 10,
														 g_colorPaleBlue, 55, 0, "settingsound");
			if (buttonResult == 1) {
				if (++loadoutOptions->selectedBeamOption >= loadoutOptions->beamOptionCount) {
					loadoutOptions->selectedBeamOption = 0;
				}

				g_selectedCombatSimSlot.beam =
					(uint8_t)loadoutOptions->beamOptions[loadoutOptions->selectedBeamOption];
			} else if (buttonResult == 2) {
				if (!loadoutOptions->selectedBeamOption) {
					loadoutOptions->selectedBeamOption = loadoutOptions->beamOptionCount - 1;
				} else {
					--loadoutOptions->selectedBeamOption;
				}
				g_selectedCombatSimSlot.beam =
					(uint8_t)loadoutOptions->beamOptions[loadoutOptions->selectedBeamOption];
			}
		}

		y += 15;
		FrontendText_DrawRightAligned(10, FrontendString_Get(STR_GAME_COUNTER_MEASURES), xRight, y,
									  g_colorLightBlue);
		sprintf(g_frontendScratchBuffer, "%-30s",
				FrontendString_Get((UIString)(g_selectedCombatSimSlot.countermeasures + STR_COUNTER_NONE)));
		if (!canSelectCraft || loadoutOptions->countermeasureOptionCount <= 1 ||
			!supportsBeamAndCountermeasures) {
			FrontendText_Draw(10, g_frontendScratchBuffer, valueLeft, y, g_colorLightBlue);
		} else {
			buttonResult = FrontendButton_DrawMenuButton(valueLeft, y, g_frontendScratchBuffer, 10,
														 g_colorPaleBlue, 56, 0, "settingsound");
			if (buttonResult == 1) {
				if (++loadoutOptions->selectedCountermeasureOption >=
					loadoutOptions->countermeasureOptionCount) {
					loadoutOptions->selectedCountermeasureOption = 0;
				}

				g_selectedCombatSimSlot.countermeasures =
					(uint8_t)
						loadoutOptions->countermeasureOptions[loadoutOptions->selectedCountermeasureOption];
			} else if (buttonResult == 2) {
				if (!loadoutOptions->selectedCountermeasureOption) {
					loadoutOptions->selectedCountermeasureOption =
						loadoutOptions->countermeasureOptionCount - 1;
				} else {
					--loadoutOptions->selectedCountermeasureOption;
				}
				g_selectedCombatSimSlot.countermeasures =
					(uint8_t)
						loadoutOptions->countermeasureOptions[loadoutOptions->selectedCountermeasureOption];
			}
		}

		y += 30;
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			FrontendText_DrawRightAligned(10, FrontendString_Get(STR_GAME_PRIMARY_FG), xRight, y,
										  g_colorLightBlue);
			sprintf(g_frontendScratchBuffer, "%-30s",
					FrontendString_Get((UIString)(g_selectedCombatSimSlot.primaryFg + STR_CONFIG_NO)));
			if (canSelectCraft) {
				if (FrontendButton_DrawMenuButton(valueLeft, y, g_frontendScratchBuffer, 10, g_colorPaleBlue,
												  57, 0, "settingsound")) {
					g_selectedCombatSimSlot.primaryFg ^= 1u;
				}
			} else {
				FrontendText_Draw(10, g_frontendScratchBuffer, valueLeft, y, g_colorLightBlue);
			}

			y += 30;
		}

		text = FrontendString_Get(STR_GAME_DUTY);
		labelWidth = FrontendText_MeasureWidth(text, 10);
		FrontendText_Draw(10, FrontendString_Get(STR_GAME_DUTY), left, y, g_colorLightBlue);
		textX = left + labelWidth + 10;
		y += 15;
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			sprintf(
				g_frontendScratchBuffer, "%s: %s",
				FrontendString_Get((UIString)(g_selectedCombatSimSlot.craftRole + STR_GAME_STATIONARY)),
				FrontendString_Get((UIString)(g_selectedCombatSimSlot.craftRole + STR_GAME_STATIONARY_DESC)));
		} else if (g_frontendMission->flightGroups[flightGroupIdx].craftRole[0]) {
			strcpy(g_frontendScratchBuffer, g_frontendMission->flightGroups[flightGroupIdx].craftRole);
		} else {
			strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_GAME_NO_DUTY));
		}

		if (canSelectCraft && g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH &&
			g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].category != 4) {
			buttonResult = FrontendButton_DrawMenuButton(textX, y, g_frontendScratchBuffer, 10,
														 g_colorPaleBlue, 58, 0, "settingsound");
			if (buttonResult == 1) {
				const int* craftCategory;
				uint8_t nextRole;

				craftCategory =
					&g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].category;
				nextRole = g_selectedCombatSimSlot.craftRole;
				do {
					g_selectedCombatSimSlot.craftRole = (uint8_t)++nextRole;
					if (nextRole >= 7u) {
						nextRole = 0;
						g_selectedCombatSimSlot.craftRole = 0;
					}
				} while (!MissionSetup_IsCraftRoleValidForCategory(craftCategory, &nextRole));
			} else if (buttonResult == 2) {
				const int* craftCategory;
				uint8_t nextRole;

				craftCategory =
					&g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].category;
				nextRole = g_selectedCombatSimSlot.craftRole;
				do {
					if (!nextRole) {
						nextRole = 6;
					} else {
						--nextRole;
					}

					g_selectedCombatSimSlot.craftRole = (uint8_t)nextRole;
				} while (!MissionSetup_IsCraftRoleValidForCategory(craftCategory, &nextRole));
			}
		} else {
			FrontendText_Draw(10, g_frontendScratchBuffer, textX, y, g_colorLightBlue);
		}

		FrontendDisplay_ResetScreenClipRect();
		return 1;
	}
}

// FUNCTION: XWA 0x54DC50
int MissionSetup_DrawFlightGroupAssignmentPanel(int panelMode) {
	int assignWidth;
	int goalsWidth;
	int armamentsWidth;
	int animFrame;
	int cursorX;
	int cursorY;
	int teamIdx;
	int slotIdx;
	int changed;
	FrontendRect rect;

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
		return 0;
	}

	FrontendDraw_RectAssign(&rect, 74, 77, 340, 90);
	FrontendCursor_GetPos(&cursorX, &cursorY);

	if (g_missionSetupActivePanel != MISSION_SETUP_PANEL_DEFAULT &&
		g_missionSetupActivePanel != MISSION_SETUP_PANEL_TEAM_ASSIGNMENT &&
		g_missionSetupActivePanel != MISSION_SETUP_PANEL_SKIRMISH_GOALS) {
		if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT &&
			g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			if (MissionSetup_CanSelectSlotCraft(&g_selectedCombatSimSlot)) {
				changed = 0;
				if (g_combatSimSlots[g_selectedCombatSimSlotIdx].ownerPlayerId !=
					g_selectedCombatSimSlot.ownerPlayerId) {
					changed = 1;
				}

				if (g_selectedCombatSimSlot.gunnerPlayerId == Net_GetLocalPlayerId() &&
					g_combatSimSlots[g_selectedCombatSimSlotIdx].gunnerPlayerId !=
						g_selectedCombatSimSlot.gunnerPlayerId) {
					changed = 1;
				}

				if (changed) {
					if (FrontendMouse_IsGateOwner(9) || FrontendMouse_IsGateOwner(8) ||
						FrontendMouse_IsGateOwner(10)) {
						FrontendMouse_ClearInputGate();
					}

					g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
					g_frontendRightBarAnimState = 0;
					g_frontendRightBarPanelIndex = MissionSetup_IsSkirmishSetupValid() ? 2 : 1;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
					return 1;
				}
			} else if (memcmp(&g_selectedCombatSimSlot, &g_combatSimSlots[g_selectedCombatSimSlotIdx],
							  sizeof(g_selectedCombatSimSlot)) != 0) {
				memcpy(&g_selectedCombatSimSlot, &g_combatSimSlots[g_selectedCombatSimSlotIdx],
					   sizeof(g_selectedCombatSimSlot));
				MissionSetup_DrawBackgroundAndPreview(0);
				if (g_selectedCombatSimSlot.ownerPlayerId == Net_GetLocalPlayerId()) {
					MissionSetup_SyncSlotLoadoutSelection(g_selectedCombatSimSlotIdx);
					return 1;
				}
			}
		}
	} else {
		assignWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_GAME_ASSIGN_FG), 10);
		rect.right = rect.left + assignWidth + 4;
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			FrontendDraw_RectOutline(&rect, 0, 0, g_colorSlateBlue);
			if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_TEAM_ASSIGNMENT) {
				FrontendText_Draw(10, FrontendString_Get(STR_GAME_ASSIGN_FG), rect.left + 2, rect.top,
								  g_colorGreen);
			} else if (FrontendButton_DrawMenuButton(rect.left + 2, rect.top,
													 FrontendString_Get(STR_GAME_ASSIGN_FG), 10,
													 g_colorPaleBlue, 41, 0, (char*)"settingsound")) {
				g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
			}

			goalsWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_GAME_GOALS), 10);
			rect.left = rect.right + 2;
			rect.right = rect.left + goalsWidth + 4;
			FrontendDraw_RectOutline(&rect, 0, 0, g_colorSlateBlue);
			if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_SKIRMISH_GOALS) {
				FrontendText_Draw(10, FrontendString_Get(STR_GAME_GOALS), rect.left + 2, rect.top,
								  g_colorGreen);
			} else if (FrontendButton_DrawMenuButton(rect.left + 2, rect.top,
													 FrontendString_Get(STR_GAME_GOALS), 10, g_colorPaleBlue,
													 40, 0, (char*)"settingsound")) {
				g_missionSetupActivePanel = MISSION_SETUP_PANEL_SKIRMISH_GOALS;
				for (teamIdx = 0; teamIdx < g_teamCount; ++teamIdx) {
					if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						continue;
					}

					switch (g_gameConfig.craftSelection) {
						case 0:
							Net_IsHost();
							break;
						case 1:
							if (!FrontendNet_IsTeamLocalPlayer(teamIdx)) {
								for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
									if (g_frontendMission
												->flightGroups[(int16_t)g_combatSimSlots[slotIdx].fgIndex]
												.team == teamIdx &&
										(g_combatSimSlots[slotIdx].ownerPlayerId ||
										 g_combatSimSlots[slotIdx].gunnerPlayerId)) {
										break;
									}
								}

								if (slotIdx >= 16) {
									Net_IsHost();
								}
							} else {
								g_missionSetupRemoteTeamGoalType = g_gameConfig.teamGoals[teamIdx];
							}
							break;
						case 2:
							Net_IsHost();
							break;
					}
				}
			}

			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH &&
				g_missionSetupActivePanel == MISSION_SETUP_PANEL_TEAM_ASSIGNMENT) {
				armamentsWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_GAME_ARMAMENTS), 10) + 10;
				animFrame = (panelMode % 24) >> 2;
				if (animFrame < 4) {
					FrontImage_SetSpriteFrame("tsettingleftu", animFrame);
					FrontImage_SetSpriteFrame("tsettingrightu", animFrame);
				} else {
					FrontImage_SetSpriteFrame("tsettingleftu", 6 - animFrame);
					FrontImage_SetSpriteFrame("tsettingrightu", 6 - animFrame);
				}

				FrontImage_GetResourceRect("tsettingleftu", &rect);
				FrontendDraw_RectOffsetXY(&rect, 310, 77);
				++rect.top;
				if (FrontendButton_DrawSpriteHitTest(&rect, "tsettingleftu", "tsettingleftd", NULL, 10,
													 g_colorPaleBlue, 43, "settingsound")) {
					if (g_missionSetupSlotSummaryMode == MISSION_SETUP_SLOT_SUMMARY_CRAFT) {
						g_missionSetupSlotSummaryMode = MISSION_SETUP_SLOT_SUMMARY_DUTY;
					} else {
						--g_missionSetupSlotSummaryMode;
					}
				}

				rect.left = rect.right + 1;
				rect.right = rect.left + armamentsWidth;
				if (FrontendDraw_PointInRect(&rect, cursorX, cursorY)) {
					if (!FrontendMouse_GetLeftDown() && !FrontendMouse_GetRightDown()) {
						FrontendText_DrawCentered(
							10,
							FrontendString_Get((UIString)(g_missionSetupSlotSummaryMode + STR_GAME_FG_INFO)),
							&rect, g_colorYellow);
						if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
							if (g_gameConfig.sfxDatapadEnabled) {
								FrontendSound_PlayUISound((char*)"settingsound", 1, 0, 255,
														  12 * g_gameConfig.sfxDatapadVolume, 63);
							}

							if (++g_missionSetupSlotSummaryMode > MISSION_SETUP_SLOT_SUMMARY_DUTY) {
								g_missionSetupSlotSummaryMode = MISSION_SETUP_SLOT_SUMMARY_CRAFT;
							}
						}
					} else {
						FrontendText_DrawCentered(
							10,
							FrontendString_Get((UIString)(g_missionSetupSlotSummaryMode + STR_GAME_FG_INFO)),
							&rect, g_colorRed);
					}
				} else {
					FrontendText_DrawCentered(
						10, FrontendString_Get((UIString)(g_missionSetupSlotSummaryMode + STR_GAME_FG_INFO)),
						&rect, g_colorPaleBlue);
				}

				FrontImage_GetResourceRect("tsettingrightu", &rect);
				FrontendDraw_RectOffsetXY(&rect, rect.right + armamentsWidth - rect.left + 311, 77);
				++rect.top;
				if (FrontendButton_DrawSpriteHitTest(&rect, "tsettingrightu", "tsettingrightd", NULL, 10,
													 g_colorPaleBlue, 42, "settingsound") &&
					++g_missionSetupSlotSummaryMode > MISSION_SETUP_SLOT_SUMMARY_DUTY) {
					g_missionSetupSlotSummaryMode = MISSION_SETUP_SLOT_SUMMARY_CRAFT;
				}
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x54E4B0
int MissionSetup_DrawTeamAssignmentPanel(int panelMode) {
	int panelContentHeight;
	int totalSlotTextHeight;
	int teamSpacing;
	int teamSpacingClamped;
	int panelX;
	int index;
	int teamTop;
	int teamRowY;
	int teamNameWidth;
	int teamPointTotal;
	int rowInTeam;
	int teamRowIdx;
	int hasGunnerSlot;
	int rosterIdx;
	int rating;
	int canSelectSlot;
	unsigned int borderColor;
	FrontendRect panelRect;
	FrontendRect slotRect;

	FrontendDraw_RectAssign(&panelRect, 74, 93, 478, 348);
	panelContentHeight = panelRect.bottom - panelRect.top - 7;
	totalSlotTextHeight = 0;
	for (index = 0; index < 16; ++index) {
		if (g_combatSimSlots[index].fgIndex != 0xffff) {
			totalSlotTextHeight += 14;
		}
	}

	if (g_teamCount > 1) {
		teamSpacing = 0;
		teamSpacingClamped = (panelContentHeight - totalSlotTextHeight) / (g_teamCount - 1);
	} else {
		teamSpacingClamped = 0;
		teamSpacing = (panelContentHeight - totalSlotTextHeight) / 2;
	}

	if (teamSpacingClamped > 28) {
		teamSpacing = (teamSpacingClamped - 28) >> 1;
		teamSpacingClamped = 28;
	}

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
		panelX = panelRect.left;
		teamTop = teamSpacing + panelRect.top;
		for (index = 0; index < g_teamCount; ++index) {
			int teamSlotHeight;

			teamSlotHeight = 14 * g_teamFgCountScratch[index];
			teamRowY = (teamSlotHeight >> 1) + teamTop - 7;
			sprintf(g_frontendScratchBuffer, "%d.", index + 1);
			FrontendText_Draw(10, g_frontendScratchBuffer, panelX, teamRowY, g_colorLightBlue);
			teamTop += teamSlotHeight + teamSpacingClamped;
		}
	} else {
		panelX = panelRect.left;
		teamTop = teamSpacing + panelRect.top;
		for (index = 0; index < g_teamCount; ++index) {
			int teamSlotHeight;

			teamSlotHeight = 14 * g_teamFgCountScratch[index];
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				teamRowY = (teamSlotHeight >> 1) + teamTop - 14;
			} else {
				teamRowY = (teamSlotHeight >> 1) + teamTop - 7;
			}

			teamNameWidth = FrontendText_MeasureWidth(g_frontendMission->teams[index].name, 10);
			FrontendDraw_RectAssign(&slotRect, panelX, teamRowY, panelX + 80, teamRowY + 14);
			FrontendDisplay_SetScreenClipRect640x480(&slotRect);
			FrontendText_Draw(10, g_frontendMission->teams[index].name, panelX, teamRowY, g_colorLightBlue);
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				teamPointTotal = MissionSetup_GetTeamCraftPointTotal(index);
				FrontendDraw_RectAssign(&slotRect, panelX, teamRowY + 12, panelX + 80, teamRowY + 26);
				FrontendDisplay_SetScreenClipRect640x480(&slotRect);
				sprintf(g_frontendScratchBuffer, "%s %d", FrontendString_Get(STR_GAME_PTS), teamPointTotal);
				if (g_gameConfig.maxPoints < 21u) {
					if (teamPointTotal > 500 * g_gameConfig.maxPoints) {
						if ((panelMode & 0xf) > 8) {
							FrontendText_Draw(10, g_frontendScratchBuffer, panelX, teamRowY + 12, g_colorRed);
						}
					} else {
						FrontendText_Draw(10, g_frontendScratchBuffer, panelX, teamRowY + 12,
										  g_colorLightBlue);
					}
				} else {
					FrontendText_Draw(10, g_frontendScratchBuffer, panelX, teamRowY + 12, g_colorLightBlue);
				}
			}

			FrontendDisplay_ResetScreenClipRect();
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				teamRowY += 10;
			} else {
				teamRowY += 5;
			}

			if (72 - teamNameWidth > 0) {
				FrontendDraw_Line(teamNameWidth + panelX + 2, teamRowY, panelX + 70, teamRowY,
								  g_colorLightBlue);
				if (g_teamFgCountScratch[index] > 1) {
					FrontendDraw_Line(panelX + 70, teamTop, panelX + 70, teamTop + teamSlotHeight,
									  g_colorLightBlue);
					FrontendDraw_Line(panelX + 70, teamTop, panelX + 78, teamTop, g_colorLightBlue);
					FrontendDraw_Line(panelX + 70, teamTop + teamSlotHeight, panelX + 78,
									  teamTop + teamSlotHeight, g_colorLightBlue);
				}
			}

			teamTop += teamSlotHeight + teamSpacingClamped;
		}
	}

	FrontendCursor_GetPos(&panelX, &teamTop);
	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
		FrontendDraw_RectAssign(&panelRect, 95, 93, 300, 106);
	} else {
		FrontendDraw_RectAssign(&panelRect, 154, 93, 478, 106);
	}
	FrontendDraw_RectOffsetXY(&panelRect, 0, teamSpacing);

	rowInTeam = 0;
	teamRowIdx = 0;
	for (index = 0; index < 16; ++index) {
		if ((int16_t)g_combatSimSlots[index].fgIndex < 0) {
			continue;
		}

		FrontendDraw_RectCopy(&slotRect, &panelRect);
		slotRect.right = slotRect.left + 140;
		if (g_combatSimSlots[index].craftType > 0 && g_combatSimSlots[index].primaryFg) {
			FrontendText_Draw(10, "!", slotRect.left - 5, slotRect.top, g_colorRed);
		}

		hasGunnerSlot = 0;
		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
			g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[index].craftType]].flyable == 2) {
			hasGunnerSlot = 1;
		}

		if (hasGunnerSlot) {
			slotRect.right = slotRect.left + 69;
		}

		if (FrontendMouse_IsGateOwner(2) &&
			(FrontendMouse_GetLeftClickFor(2) || FrontendMouse_GetRightClickFor(2)) &&
			FrontendDraw_PointInRect(&slotRect, panelX, teamTop)) {
			g_combatSimSlots[index].ownerPlayerId = g_missionSetupDraggedPlayerId;
			if (!g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[index].craftType]].flyable) {
				g_combatSimSlots[index].craftType = 0;
			}

			if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
				MissionSetup_BroadcastStatePacket(0);
				g_frontendNetPacketScratch.packetType = 75;
				memset(g_frontendNetPacketScratch.payload, 0, 4u);
				Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 8u);
			}

			FrontendMouse_ClearInputGate();
			g_missionSetupDraggedPlayerId = 0;
		}

		if (hasGunnerSlot) {
			slotRect.left += 72;
			slotRect.right = slotRect.left + 68;
			if (FrontendMouse_IsGateOwner(2) &&
				(FrontendMouse_GetLeftClickFor(2) || FrontendMouse_GetRightClickFor(2)) &&
				FrontendDraw_PointInRect(&slotRect, panelX, teamTop)) {
				g_combatSimSlots[index].gunnerPlayerId = g_missionSetupDraggedPlayerId;
				if (!g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[index].craftType]].flyable) {
					g_combatSimSlots[index].craftType = 0;
				}

				if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
					MissionSetup_BroadcastStatePacket(0);
					g_frontendNetPacketScratch.packetType = 75;
					memset(g_frontendNetPacketScratch.payload, 0, 4u);
					Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 8u);
				}

				FrontendMouse_ClearInputGate();
				g_missionSetupDraggedPlayerId = 0;
			}

			FrontendDraw_RectCopy(&slotRect, &panelRect);
			slotRect.right = slotRect.left + 69;
		}

		FrontendDisplay_SetScreenClipRect640x480(&slotRect);
		if (g_combatSimSlots[index].ownerPlayerId) {
			if ((Net_IsHost() || g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) &&
				!FrontendMouse_IsGateOwner(2) && !g_missionSetupDraggedPlayerId &&
				FrontendDraw_PointInRect(&slotRect, panelX, teamTop)) {
				FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor1", g_cursorBitmap);
				if (g_missionSetupPlayerDragState < 2) {
					g_missionSetupPlayerDragState = 1;
				}

				if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
					g_missionSetupDraggedPlayerId = g_combatSimSlots[index].ownerPlayerId;
					g_combatSimSlots[index].ownerPlayerId = 0;
					FrontendMouse_SetInputGate(2);
					FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor2", g_cursorBitmap);
					g_missionSetupPlayerDragState = 2;
					if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						g_frontendNetPacketScratch.packetType = 75;
						memcpy(g_frontendNetPacketScratch.payload, &g_missionSetupDraggedPlayerId, 4u);
						Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 8u);
					}
					MissionSetup_BroadcastStatePacket(0);
				}
			}

			for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
				if (g_mpRoster[rosterIdx].playerId == g_combatSimSlots[index].ownerPlayerId) {
					break;
				}
			}

			if (rosterIdx < 8) {
				rating = g_mpRoster[rosterIdx].rating;
				if (!rating && !g_mpRoster[rosterIdx].name[0]) {
					sprintf(g_frontendScratchBuffer, "%s", FrontendString_Get(STR_JOIN_IN_PROGRESS));
				} else if (g_mpRosterReadyFlags[rosterIdx]) {
					if (hasGunnerSlot) {
						sprintf(g_frontendScratchBuffer, "%s", g_mpRoster[rosterIdx].name);
					} else {
						sprintf(g_frontendScratchBuffer, "%s %s", FrontendString_Get((UIString)(rating + 54)),
								g_mpRoster[rosterIdx].name);
					}
				} else if (hasGunnerSlot) {
					sprintf(g_frontendScratchBuffer, "%s", g_mpRoster[rosterIdx].name);
				} else {
					sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
							FrontendString_Get((UIString)(rating + 54)), 1, g_mpRoster[rosterIdx].name);
				}

				if (g_mpRosterReadyFlags[rosterIdx]) {
					FrontendText_Draw(10, g_frontendScratchBuffer, slotRect.left, slotRect.top,
									  g_colorMidGray);
				} else if (g_mpRoster[rosterIdx].playerId == Net_GetLocalPlayerId() ||
						   g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
					FrontendText_Draw(10, g_frontendScratchBuffer, slotRect.left, slotRect.top,
									  g_pulseColorRamp[(panelMode % 24) >> 1]);
				} else {
					FrontendText_Draw(10, g_frontendScratchBuffer, slotRect.left, slotRect.top,
									  g_colorYellow);
				}

				if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
					((g_gameConfig.networkType == NET_TRANSPORT_TCPIP && g_gameConfig.asyncFlag) ||
					 g_gameConfig.networkType == NET_TRANSPORT_MODEM)) {
					slotRect.left = slotRect.right - 10;
					MissionSetup_DrawPlayerConnectionStats(&slotRect, g_mpRoster[rosterIdx].playerId);
				}
			}
		} else {
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				if (!g_combatSimSlots[index].craftType) {
					FrontendDraw_Line(slotRect.left, slotRect.bottom, slotRect.right, slotRect.bottom,
									  g_colorSlateBlue);
					FrontendDraw_Line(slotRect.left, slotRect.bottom - 2, slotRect.left, slotRect.bottom,
									  g_colorSlateBlue);
					borderColor = (unsigned int)g_colorSlateBlue;
					FrontendDraw_Line(slotRect.right, slotRect.bottom - 2, slotRect.right, slotRect.bottom,
									  (int)borderColor);
				} else {
					if (g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[index].craftType]].flyable) {
						FrontendDraw_Line(slotRect.left, slotRect.bottom, slotRect.right, slotRect.bottom,
										  g_colorPaleBlue);
						FrontendDraw_Line(slotRect.left, slotRect.bottom - 2, slotRect.left, slotRect.bottom,
										  g_colorPaleBlue);
						borderColor = (unsigned int)g_colorPaleBlue;
					} else {
						FrontendDraw_Line(slotRect.left, slotRect.bottom, slotRect.right, slotRect.bottom,
										  g_colorSlateBlue);
						FrontendDraw_Line(slotRect.left, slotRect.bottom - 2, slotRect.left, slotRect.bottom,
										  g_colorSlateBlue);
						borderColor = (unsigned int)g_colorSlateBlue;
					}

					FrontendDraw_Line(slotRect.right, slotRect.bottom - 2, slotRect.right, slotRect.bottom,
									  (int)borderColor);

					canSelectSlot = FrontendButton_DrawMenuButton(
						slotRect.left, slotRect.top,
						FrontendString_Get((UIString)(g_combatSimSlots[index].groupAI + 695)), 10,
						g_colorPaleBlue, index + 70, 0, (char*)"settingsound");
					if (canSelectSlot) {
						g_selectedCombatSimSlotIdx = index;
						MissionSetup_SyncSlotLoadoutSelection(index);
						g_missionSetupActivePanel = MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT;
						g_activeTextFieldId = 1;
						g_missionSetupCarouselSlideOffset = 0;
						g_missionSetupCarouselQueuedSlideOffset = 0;
						g_missionSetupCraftSelectionChangedFlag = 0;
						g_missionSetupSubpanelMode =
							g_combatSimSlots[g_selectedCombatSimSlotIdx].craftType == 0 ? 3 : 0;
						g_frontendRightBarAnimState = 2;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
						if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
							g_frontendNetPacketScratch.packetType = 82;
							Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 4u);
						}

						memcpy(&g_selectedCombatSimSlot, &g_combatSimSlots[index],
							   sizeof(g_selectedCombatSimSlot));
						memcpy(g_selectedCombatSimSlotNameEditBuffer, g_combatSimSlotNames[index],
							   sizeof(g_combatSimSlotNames[index]));
						if (g_selectedCombatSimSlot.craftType) {
							MissionSetup_LoadShipBmpForCraftType(g_selectedCombatSimSlot.craftType);
						} else {
							MissionSetup_UnloadShipBmp();
						}
					}
				}
			} else {
				FrontendDraw_Line(slotRect.left, slotRect.bottom, slotRect.right, slotRect.bottom,
								  g_colorPaleBlue);
				FrontendDraw_Line(slotRect.left, slotRect.bottom - 2, slotRect.left, slotRect.bottom,
								  g_colorPaleBlue);
				borderColor = (unsigned int)g_colorPaleBlue;
				FrontendDraw_Line(slotRect.right, slotRect.bottom - 2, slotRect.right, slotRect.bottom,
								  (int)borderColor);
			}
		}

		if (hasGunnerSlot) {
			slotRect.left = panelRect.left + 72;
			slotRect.right = slotRect.left + 68;
			FrontendDisplay_SetScreenClipRect640x480(&slotRect);
			++slotRect.left;
			if (g_combatSimSlots[index].gunnerPlayerId) {
				if ((Net_IsHost() || g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) &&
					!FrontendMouse_IsGateOwner(2) && !g_missionSetupDraggedPlayerId &&
					FrontendDraw_PointInRect(&slotRect, panelX, teamTop)) {
					FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor1", g_cursorBitmap);
					if (g_missionSetupPlayerDragState < 2) {
						g_missionSetupPlayerDragState = 1;
					}

					if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
						g_missionSetupDraggedPlayerId = g_combatSimSlots[index].gunnerPlayerId;
						g_combatSimSlots[index].gunnerPlayerId = 0;
						FrontendMouse_SetInputGate(2);
						FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor2", g_cursorBitmap);
						g_missionSetupPlayerDragState = 2;
						if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
							g_frontendNetPacketScratch.packetType = 75;
							memcpy(g_frontendNetPacketScratch.payload, &g_missionSetupDraggedPlayerId, 4u);
							Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 8u);
						}
						MissionSetup_BroadcastStatePacket(0);
					}
				}

				for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
					if (g_mpRoster[rosterIdx].playerId == g_combatSimSlots[index].gunnerPlayerId) {
						break;
					}
				}

				if (rosterIdx < 8) {
					if (!g_mpRoster[rosterIdx].rating && !g_mpRoster[rosterIdx].name[0]) {
						sprintf(g_frontendScratchBuffer, "%s", FrontendString_Get(STR_JOIN_IN_PROGRESS));
					} else if (g_mpRosterReadyFlags[rosterIdx]) {
						sprintf(g_frontendScratchBuffer, "%s", g_mpRoster[rosterIdx].name);
					} else {
						sprintf(g_frontendScratchBuffer, "%s", g_mpRoster[rosterIdx].name);
					}

					if (g_mpRosterReadyFlags[rosterIdx]) {
						FrontendText_Draw(10, g_frontendScratchBuffer, slotRect.left, slotRect.top,
										  g_colorMidGray);
					} else if (g_mpRoster[rosterIdx].playerId == Net_GetLocalPlayerId() ||
							   g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						FrontendText_Draw(10, g_frontendScratchBuffer, slotRect.left, slotRect.top,
										  g_pulseColorRamp[(panelMode % 24) >> 1]);
					} else {
						FrontendText_Draw(10, g_frontendScratchBuffer, slotRect.left, slotRect.top,
										  g_colorYellow);
					}

					if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
						((g_gameConfig.networkType == NET_TRANSPORT_TCPIP && g_gameConfig.asyncFlag) ||
						 g_gameConfig.networkType == NET_TRANSPORT_MODEM)) {
						slotRect.left = slotRect.right - 10;
						MissionSetup_DrawPlayerConnectionStats(&slotRect, g_mpRoster[rosterIdx].playerId);
					}
				}
			} else {
				FrontendDraw_Line(slotRect.left, slotRect.bottom, slotRect.right, slotRect.bottom,
								  g_colorMutedGreen);
				FrontendDraw_Line(slotRect.left, slotRect.bottom - 2, slotRect.left, slotRect.bottom,
								  g_colorMutedGreen);
				FrontendDraw_Line(slotRect.right, slotRect.bottom - 2, slotRect.right, slotRect.bottom,
								  g_colorMutedGreen);
			}
		}

		slotRect.left = slotRect.right + 7;
		slotRect.right = panelRect.right;
		FrontendDisplay_SetScreenClipRect640x480(&slotRect);
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			if (g_combatSimSlots[index].craftType) {
				MissionSetup_FormatCombatSimSlotSummaryText(index);
				canSelectSlot =
					FrontendButton_DrawMenuButton(slotRect.left, slotRect.top, g_frontendScratchBuffer, 10,
												  g_colorPaleBlue, index + 50, 0, (char*)"settingsound");
				if (canSelectSlot) {
					g_selectedCombatSimSlotIdx = index;
					MissionSetup_SyncSlotLoadoutSelection(index);
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT;
					g_activeTextFieldId = 1;
					g_missionSetupCarouselSlideOffset = 0;
					g_missionSetupCarouselQueuedSlideOffset = 0;
					g_missionSetupCraftSelectionChangedFlag = 0;
					g_missionSetupSubpanelMode =
						g_combatSimSlots[g_selectedCombatSimSlotIdx].craftType == 0 ? 3 : 0;
					g_frontendRightBarAnimState = 2;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
					if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						g_frontendNetPacketScratch.packetType = 82;
						Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 4u);
					}

					memcpy(&g_selectedCombatSimSlot, &g_combatSimSlots[index],
						   sizeof(g_selectedCombatSimSlot));
					memcpy(g_selectedCombatSimSlotNameEditBuffer, g_combatSimSlotNames[index],
						   sizeof(g_combatSimSlotNames[index]));
					if (g_selectedCombatSimSlot.craftType) {
						MissionSetup_LoadShipBmpForCraftType(g_selectedCombatSimSlot.craftType);
					} else {
						MissionSetup_UnloadShipBmp();
					}
				}
			} else {
				canSelectSlot = FrontendButton_DrawMenuButton(
					slotRect.left, slotRect.top, FrontendString_Get(STR_GAME_SET_FG_INFO), 10,
					g_colorPaleBlue, index + 50, 0, (char*)"settingsound");
				if (canSelectSlot) {
					g_selectedCombatSimSlotIdx = index;
					MissionSetup_SyncSlotLoadoutSelection(index);
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT;
					g_activeTextFieldId = 1;
					g_missionSetupCarouselSlideOffset = 0;
					g_missionSetupCarouselQueuedSlideOffset = 0;
					g_missionSetupCraftSelectionChangedFlag = 0;
					g_missionSetupSubpanelMode =
						g_combatSimSlots[g_selectedCombatSimSlotIdx].craftType == 0 ? 3 : 0;
					g_frontendRightBarAnimState = 2;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
					if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						g_frontendNetPacketScratch.packetType = 82;
						Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 4u);
					}

					memcpy(&g_selectedCombatSimSlot, &g_combatSimSlots[index],
						   sizeof(g_selectedCombatSimSlot));
					memcpy(g_selectedCombatSimSlotNameEditBuffer, g_combatSimSlotNames[index],
						   sizeof(g_combatSimSlotNames[index]));
					if (g_selectedCombatSimSlot.craftType) {
						MissionSetup_LoadShipBmpForCraftType(g_selectedCombatSimSlot.craftType);
					} else {
						MissionSetup_UnloadShipBmp();
					}
				}
			}
		} else {
			MissionSetup_FormatCombatSimSlotSummaryText(index);
			canSelectSlot = 1;
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
				canSelectSlot = g_combatSimSlots[index].ownerPlayerId != 0;
				if (canSelectSlot) {
					canSelectSlot = MissionSetup_CanSelectSlotCraft(&g_combatSimSlots[index]);
				}
			}

			if (canSelectSlot) {
				canSelectSlot =
					FrontendButton_DrawMenuButton(slotRect.left, slotRect.top, g_frontendScratchBuffer, 10,
												  g_colorPaleBlue, index + 50, 0, (char*)"settingsound");
				if (canSelectSlot) {
					g_selectedCombatSimSlotIdx = index;
					MissionSetup_SyncSlotLoadoutSelection(index);
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT;
					g_activeTextFieldId = 1;
					g_missionSetupCarouselSlideOffset = 0;
					g_missionSetupCarouselQueuedSlideOffset = 0;
					g_missionSetupCraftSelectionChangedFlag = 0;
					if (g_combatSimSlots[g_selectedCombatSimSlotIdx].craftType &&
						g_pilotData.missionDirectoryId != MISSION_DIRECTORY_MELEE) {
						g_missionSetupSubpanelMode = 0;
					} else {
						g_missionSetupSubpanelMode = 3;
					}

					g_frontendRightBarAnimState = 2;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
					if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						g_frontendNetPacketScratch.packetType = 82;
						Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 4u);
					}

					memcpy(&g_selectedCombatSimSlot, &g_combatSimSlots[index],
						   sizeof(g_selectedCombatSimSlot));
					memcpy(g_selectedCombatSimSlotNameEditBuffer, g_combatSimSlotNames[index],
						   sizeof(g_combatSimSlotNames[index]));
					if (g_selectedCombatSimSlot.craftType) {
						MissionSetup_LoadShipBmpForCraftType(g_selectedCombatSimSlot.craftType);
					} else {
						MissionSetup_UnloadShipBmp();
					}
				}
			} else {
				FrontendText_Draw(10, g_frontendScratchBuffer, slotRect.left, slotRect.top, g_colorLightBlue);
			}
		}

		FrontendDisplay_ResetScreenClipRect();
		++rowInTeam;
		FrontendDraw_RectOffsetXY(&panelRect, 0, 14);
		if (rowInTeam >= g_teamFgCountScratch[teamRowIdx]) {
			FrontendDraw_RectOffsetXY(&panelRect, 0, teamSpacingClamped);
			rowInTeam = 0;
			++teamRowIdx;
		}
	}

	if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT && !g_missionSetupSubpanelMode) {
		MissionSetup_DrawBackgroundAndPreview(0);
	}

	return 1;
}

// FUNCTION: XWA 0x5535F0
int MissionSetup_DrawMissionNavigationPanel(int panelMode) {
	int parsedBattleNumber;
	int oldMissionIdx;
	int scanIdx;
	int remaining;
	int selectedTeam;
	FrontendRect spriteRect;
	FrontendRect buttonRect;
	MissionListEntry* entry;
	struct {
		int slot;
		int name;
	} canChange;
	char currentSectionName[128];

	parsedBattleNumber = (panelMode % 32) >> 2;
	if (parsedBattleNumber < 5) {
		FrontImage_SetSpriteFrame("lsettingleftu", parsedBattleNumber);
		FrontImage_SetSpriteFrame("lsettingrightu", parsedBattleNumber);
		FrontImage_SetSpriteFrame("settingleftup", parsedBattleNumber);
		FrontImage_SetSpriteFrame("settingrightup", parsedBattleNumber);
	} else {
		FrontImage_SetSpriteFrame("lsettingleftu", 8 - parsedBattleNumber);
		FrontImage_SetSpriteFrame("lsettingrightu", 8 - parsedBattleNumber);
		FrontImage_SetSpriteFrame("settingleftup", 8 - parsedBattleNumber);
		FrontImage_SetSpriteFrame("settingrightup", 8 - parsedBattleNumber);
	}

	if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH &&
		g_missionSetupActivePanel == MISSION_SETUP_PANEL_BATTLE_SELECT) {
		FrontImage_GetResourceRect("lsettingleftu", &spriteRect);
		FrontendDraw_RectCopy(&buttonRect, &spriteRect);
		FrontendDraw_RectOffsetXY(&buttonRect, spriteRect.left - spriteRect.right + 57,
								  ((spriteRect.top - spriteRect.bottom + 274) >> 1) + 75);
		if (FrontendButton_DrawSpriteHitTest(&buttonRect, "lsettingleftu", "lsettingleftd",
											 FrontendString_Get(STR_GAME_PREVIOUS_BATTLE), 10,
											 g_colorLightBlue, 71, "settingsound")) {
			remaining = g_missionCount;
			entry = g_missionList;
			strcpy(currentSectionName, g_missionSetupCurrentBattleSectionName);
			scanIdx = g_missionSetupBattleFirstMissionIdx - 1;
			while (1) {
				if (scanIdx < 0) {
					scanIdx = g_missionCount - 1;
				}

				entry = &g_missionList[scanIdx];
				if (!entry->lockedFlag && g_pilotData.tourOfDutyMissions[entry->missionIdx].completedCount &&
					strcmp(entry->sectionName, currentSectionName) != 0) {
					g_missionSetupBattleLastMissionIdx = scanIdx;
					strcpy(g_missionSetupCurrentBattleSectionName, entry->sectionName);
					sscanf(entry->fileName, "%db%d", &parsedBattleNumber, &frame);
					break;
				}

				--scanIdx;
				if (--remaining == 0) {
					break;
				}
			}

			scanIdx = g_missionSetupBattleLastMissionIdx;
			if (scanIdx >= 0) {
				while (strcmp(g_missionList[scanIdx].sectionName, g_missionSetupCurrentBattleSectionName) ==
					   0) {
					if (--scanIdx < 0) {
						break;
					}
				}
				if (scanIdx >= 0) {
					g_missionSetupBattleFirstMissionIdx = scanIdx + 1;
				} else {
					g_missionSetupBattleFirstMissionIdx = 0;
				}
			} else {
				g_missionSetupBattleFirstMissionIdx = 0;
			}

			g_missionSetupBattleSelectableMissionCount = 0;
			for (scanIdx = g_missionSetupBattleFirstMissionIdx; scanIdx <= g_missionSetupBattleLastMissionIdx;
				 ++scanIdx) {
				entry = &g_missionList[scanIdx];
				if (!entry->lockedFlag && g_pilotData.tourOfDutyMissions[entry->missionIdx].completedCount) {
					++g_missionSetupBattleSelectableMissionCount;
				}
			}

			sscanf(g_missionList[g_missionSetupBattleFirstMissionIdx].fileName, "%db%d", &parsedBattleNumber,
				   &frame);
			g_missionSetupCarouselSlideOffset = 0;
			g_missionSetupCarouselQueuedSlideOffset = 0;
			if (g_selectedMissionListIndex < g_missionSetupBattleFirstMissionIdx ||
				g_selectedMissionListIndex > g_missionSetupBattleLastMissionIdx) {
				g_missionSetupBattleSelectedMissionOrdinal = 0;
			} else {
				g_missionSetupBattleSelectedMissionOrdinal =
					g_selectedMissionListIndex - g_missionSetupBattleFirstMissionIdx;
			}
			MissionSetup_DrawBackgroundAndPreview(0);
		}

		FrontImage_GetResourceRect("lsettingrightu", &spriteRect);
		FrontendDraw_RectCopy(&buttonRect, &spriteRect);
		FrontendDraw_RectOffsetXY(&buttonRect, 582, ((spriteRect.top - spriteRect.bottom + 274) >> 1) + 75);
		if (FrontendButton_DrawSpriteHitTest(&buttonRect, "lsettingrightu", "lsettingrightd",
											 FrontendString_Get(STR_GAME_NEXT_BATTLE), 10, g_colorLightBlue,
											 72, "settingsound")) {
			remaining = g_missionCount;
			strcpy(currentSectionName, g_missionSetupCurrentBattleSectionName);
			scanIdx = g_missionSetupBattleLastMissionIdx + 1;
			if (scanIdx >= g_missionCount) {
				scanIdx = 0;
			}

			while (1) {
				entry = &g_missionList[scanIdx];
				if (!entry->lockedFlag && g_pilotData.tourOfDutyMissions[entry->missionIdx].completedCount &&
					strcmp(entry->sectionName, currentSectionName) != 0) {
					g_missionSetupBattleFirstMissionIdx = scanIdx;
					strcpy(g_missionSetupCurrentBattleSectionName, entry->sectionName);
					sscanf(entry->fileName, "%db%d", &parsedBattleNumber, &frame);
					break;
				}

				if (++scanIdx >= g_missionCount) {
					scanIdx = 0;
				}
				if (--remaining == 0) {
					break;
				}
			}

			scanIdx = g_missionSetupBattleFirstMissionIdx;
			if (scanIdx < g_missionCount) {
				while (strcmp(g_missionList[scanIdx].sectionName, g_missionSetupCurrentBattleSectionName) ==
						   0 &&
					   g_pilotData.tourOfDutyMissions[g_missionList[scanIdx].missionIdx].completedCount) {
					if (++scanIdx >= g_missionCount) {
						break;
					}
				}

				g_missionSetupBattleLastMissionIdx = scanIdx - 1;
			}

			g_missionSetupBattleSelectableMissionCount = 0;
			for (scanIdx = g_missionSetupBattleFirstMissionIdx; scanIdx <= g_missionSetupBattleLastMissionIdx;
				 ++scanIdx) {
				entry = &g_missionList[scanIdx];
				if (!entry->lockedFlag && g_pilotData.tourOfDutyMissions[entry->missionIdx].completedCount) {
					++g_missionSetupBattleSelectableMissionCount;
				}
			}

			sscanf(g_missionList[g_missionSetupBattleFirstMissionIdx].fileName, "%db%d", &parsedBattleNumber,
				   &frame);
			g_missionSetupCarouselSlideOffset = 0;
			g_missionSetupCarouselQueuedSlideOffset = 0;
			if (g_selectedMissionListIndex < g_missionSetupBattleFirstMissionIdx ||
				g_selectedMissionListIndex > g_missionSetupBattleLastMissionIdx) {
				g_missionSetupBattleSelectedMissionOrdinal = 0;
			} else {
				g_missionSetupBattleSelectedMissionOrdinal =
					g_selectedMissionListIndex - g_missionSetupBattleFirstMissionIdx;
			}
			MissionSetup_DrawBackgroundAndPreview(0);
		}

		return 1;
	}

	if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH &&
		g_missionSetupActivePanel != MISSION_SETUP_PANEL_BATTLE_SELECT) {
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
			FrontImage_GetResourceRect("settingleftup", &spriteRect);
			FrontendDraw_RectCopy(&buttonRect, &spriteRect);
			FrontendDraw_RectOffsetXY(&buttonRect, spriteRect.left - spriteRect.right + 57, 65);
			if (FrontendButton_DrawSpriteHitTest(&buttonRect, "settingleftup", "settingleftdown",
												 FrontendString_Get(STR_GAME_PREVIOUS_BATTLE), 10,
												 g_colorLightBlue, 73, "settingsound")) {
				if (g_missionList != NULL) {
					oldMissionIdx = g_selectedMissionListIndex;
					strcpy(currentSectionName, g_missionList[g_selectedMissionListIndex].sectionName);
					scanIdx = oldMissionIdx;
					do {
						if (scanIdx) {
							--scanIdx;
						} else {
							scanIdx = g_missionCount - 1;
						}

						g_selectedMissionListIndex = scanIdx;
						if (scanIdx == oldMissionIdx) {
							break;
						}

						entry = &g_missionList[scanIdx];
					} while (entry->lockedFlag ||
							 !g_pilotData.tourOfDutyMissions[entry->missionIdx].completedCount ||
							 !strcmp(currentSectionName, entry->sectionName));

					g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
						g_missionList[scanIdx].missionIdx;
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_DEFAULT;
					if (g_frontendRightBarAnimState != 1) {
						g_frontendRightBarAnimState = 0;
						g_frontendRightBarPanelIndex = 1;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}

					FrontendMission_LoadCurrent();
					MissionSetup_CountActiveTeams();
					MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
					MissionSetup_LoadMissionDescText(g_briefingText);
					g_frontendFirstVisibleLine = 0;
					if (g_missionList != NULL) {
						scanIdx = 0;
						g_selectedMissionListIndex = 0;
						if (g_missionCount) {
							oldMissionIdx = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
							do {
								if (g_missionList[scanIdx].missionIdx == oldMissionIdx) {
									break;
								}

								g_selectedMissionListIndex = ++scanIdx;
							} while ((unsigned int)scanIdx < (unsigned int)g_missionCount);
						}
					}
				}
				if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && Net_IsHost()) {
					MissionSetup_BroadcastStatePacket(0);
				}
				MissionSetup_DrawBackgroundAndPreview(0);
			}

			FrontImage_GetResourceRect("settingrightup", &spriteRect);
			FrontendDraw_RectCopy(&buttonRect, &spriteRect);
			FrontendDraw_RectOffsetXY(&buttonRect, 582, 65);
			if (FrontendButton_DrawSpriteHitTest(&buttonRect, "settingrightup", "settingrightdown",
												 FrontendString_Get(STR_GAME_NEXT_BATTLE), 10,
												 g_colorLightBlue, 75, "settingsound")) {
				if (g_missionList != NULL) {
					oldMissionIdx = g_selectedMissionListIndex;
					strcpy(currentSectionName, g_missionList[g_selectedMissionListIndex].sectionName);
					scanIdx = oldMissionIdx;
					do {
						g_selectedMissionListIndex = ++scanIdx;
						if (scanIdx >= g_missionCount) {
							scanIdx = 0;
							g_selectedMissionListIndex = 0;
						}

						if (scanIdx == oldMissionIdx) {
							break;
						}

						entry = &g_missionList[scanIdx];
					} while (entry->lockedFlag ||
							 !g_pilotData.tourOfDutyMissions[entry->missionIdx].completedCount ||
							 !strcmp(currentSectionName, entry->sectionName));

					g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
						g_missionList[scanIdx].missionIdx;
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_DEFAULT;
					if (g_frontendRightBarAnimState != 1) {
						g_frontendRightBarAnimState = 0;
						g_frontendRightBarPanelIndex = 1;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}

					FrontendMission_LoadCurrent();
					MissionSetup_CountActiveTeams();
					MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
					MissionSetup_LoadMissionDescText(g_briefingText);
					g_frontendFirstVisibleLine = 0;
					if (g_missionList != NULL) {
						scanIdx = 0;
						g_selectedMissionListIndex = 0;
						if (g_missionCount) {
							oldMissionIdx = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
							do {
								if (g_missionList[scanIdx].missionIdx == oldMissionIdx) {
									break;
								}

								g_selectedMissionListIndex = ++scanIdx;
							} while ((unsigned int)scanIdx < (unsigned int)g_missionCount);
						}
					}
				}
				if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && Net_IsHost()) {
					MissionSetup_BroadcastStatePacket(0);
				}
				MissionSetup_DrawBackgroundAndPreview(0);
			}
		}

		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_CLIENT) {
			return 1;
		}

		FrontImage_GetResourceRect("lsettingleftu", &spriteRect);
		FrontendDraw_RectCopy(&buttonRect, &spriteRect);
		FrontendDraw_RectOffsetXY(&buttonRect, spriteRect.left - spriteRect.right + 57,
								  ((spriteRect.top - spriteRect.bottom + 274) >> 1) + 75);
		if (FrontendButton_DrawSpriteHitTest(&buttonRect, "lsettingleftu", "lsettingleftd",
											 FrontendString_Get(STR_GAME_PREVIOUS_MISSION), 10,
											 g_colorLightBlue, 71, "settingsound")) {
			if (g_missionList != NULL) {
				scanIdx = g_selectedMissionListIndex;
				do {
					if (scanIdx) {
						--scanIdx;
					} else {
						scanIdx = g_missionCount - 1;
					}

					g_selectedMissionListIndex = scanIdx;
				} while (
					(g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR &&
					 !g_pilotData.tourOfDutyMissions[g_missionList[scanIdx].missionIdx].completedCount) ||
					g_missionList[scanIdx].lockedFlag);

				g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
					g_missionList[scanIdx].missionIdx;
				if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_MELEE) {
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_DEFAULT;
				}

				if (g_frontendRightBarAnimState != 1) {
					g_frontendRightBarAnimState = 0;
					g_frontendRightBarPanelIndex = 1;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				}

				FrontendMission_LoadCurrent();
				MissionSetup_CountActiveTeams();
				MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
				MissionSetup_LoadMissionDescText(g_briefingText);
				g_frontendFirstVisibleLine = 0;
				if (g_missionList != NULL) {
					scanIdx = 0;
					g_selectedMissionListIndex = 0;
					if (g_missionCount) {
						oldMissionIdx = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
						do {
							if (g_missionList[scanIdx].missionIdx == oldMissionIdx) {
								break;
							}

							g_selectedMissionListIndex = ++scanIdx;
						} while ((unsigned int)scanIdx < (unsigned int)g_missionCount);
					}
				}
			}
			if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && Net_IsHost()) {
				MissionSetup_BroadcastStatePacket(0);
			}
			MissionSetup_DrawBackgroundAndPreview(0);
		}

		FrontImage_GetResourceRect("lsettingrightu", &spriteRect);
		FrontendDraw_RectCopy(&buttonRect, &spriteRect);
		FrontendDraw_RectOffsetXY(&buttonRect, 582, ((spriteRect.top - spriteRect.bottom + 274) >> 1) + 75);
		if (!FrontendButton_DrawSpriteHitTest(&buttonRect, "lsettingrightu", "lsettingrightd",
											  FrontendString_Get(STR_GAME_NEXT_MISSION), 10, g_colorLightBlue,
											  72, "settingsound")) {
			return 1;
		}

		if (g_missionList != NULL) {
			scanIdx = g_selectedMissionListIndex;
			do {
				g_selectedMissionListIndex = ++scanIdx;
				if (scanIdx >= g_missionCount) {
					scanIdx = 0;
					g_selectedMissionListIndex = 0;
				}
			} while ((g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR &&
					  !g_pilotData.tourOfDutyMissions[g_missionList[scanIdx].missionIdx].completedCount) ||
					 g_missionList[scanIdx].lockedFlag);

			g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
				g_missionList[scanIdx].missionIdx;
			if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_MELEE) {
				g_missionSetupActivePanel = MISSION_SETUP_PANEL_DEFAULT;
			}

			if (g_frontendRightBarAnimState != 1) {
				g_frontendRightBarAnimState = 0;
				g_frontendRightBarPanelIndex = 1;
				FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
										  63);
			}

			FrontendMission_LoadCurrent();
			MissionSetup_CountActiveTeams();
			MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
			MissionSetup_LoadMissionDescText(g_briefingText);
			g_frontendFirstVisibleLine = 0;
			if (g_missionList != NULL) {
				scanIdx = 0;
				g_selectedMissionListIndex = 0;
				if (g_missionCount) {
					oldMissionIdx = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
					do {
						if (g_missionList[scanIdx].missionIdx == oldMissionIdx) {
							break;
						}

						g_selectedMissionListIndex = ++scanIdx;
					} while ((unsigned int)scanIdx < (unsigned int)g_missionCount);
				}
			}
		}
		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && Net_IsHost()) {
			MissionSetup_BroadcastStatePacket(0);
		}
		MissionSetup_DrawBackgroundAndPreview(0);

		return 1;
	}

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		if (g_missionSetupActivePanel != MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT ||
			g_missionSetupSubpanelMode) {
			return 1;
		}

		canChange.slot = 0;
		canChange.name = 0;
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			canChange.slot = 1;
			canChange.name = 1;
		} else {
			switch (g_gameConfig.craftSelection) {
				case 0:
					break;

				case 1:
					if (g_selectedCombatSimSlot.ownerPlayerId) {
						if (g_selectedCombatSimSlot.ownerPlayerId == Net_GetLocalPlayerId()) {
							canChange.slot = 1;
							canChange.name = 1;
						}
					} else if (FrontendNet_IsTeamLocalPlayer(
								   g_frontendMission->flightGroups[(int16_t)g_selectedCombatSimSlot.fgIndex]
									   .team)) {
						canChange.slot = 1;
						canChange.name = 1;
					}

					selectedTeam =
						g_frontendMission->flightGroups[(int16_t)g_selectedCombatSimSlot.fgIndex].team;
					for (scanIdx = 0; scanIdx < 16; ++scanIdx) {
						if (g_frontendMission->flightGroups[(int16_t)g_combatSimSlots[scanIdx].fgIndex]
									.team == selectedTeam &&
							(g_combatSimSlots[scanIdx].ownerPlayerId ||
							 g_combatSimSlots[scanIdx].gunnerPlayerId)) {
							break;
						}
					}

					if (scanIdx >= 16 && Net_IsHost()) {
						canChange.slot = 1;
						canChange.name = 1;
					}
					break;

				case 2:
					if (Net_IsHost()) {
						canChange.slot = 1;
						canChange.name = 1;
					}
					break;

				default:
					canChange.slot = 1;
					canChange.name = 1;
					break;
			}
		}

		FrontImage_GetResourceRect("lsettingleftu", &spriteRect);
		FrontendDraw_RectCopy(&buttonRect, &spriteRect);
		FrontendDraw_RectOffsetXY(&buttonRect, spriteRect.left - spriteRect.right + 57,
								  ((spriteRect.top - spriteRect.bottom + 274) >> 1) + 75);
		if (FrontendButton_DrawSpriteHitTest(&buttonRect, "lsettingleftu", "lsettingleftd",
											 FrontendString_Get(STR_GAME_PREVIOUS_FG), 10, g_colorLightBlue,
											 71, "settingsound")) {
			if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
				if (canChange.slot && MissionSetup_SelectedSlotHasChanges()) {
					g_frontendNetPacketScratch.packetType = 76;
					*(uint32_t*)g_frontendNetPacketScratch.payload = (uint32_t)g_selectedCombatSimSlotIdx;
					memcpy(&g_frontendNetPacketScratch.payload[4], &g_selectedCombatSimSlot,
						   sizeof(g_selectedCombatSimSlot));
					Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 0x1Cu);
				}
				if (canChange.name && g_selectedCombatSimSlotNameEditBuffer[0] &&
					strcmp(g_selectedCombatSimSlotNameEditBuffer,
						   g_combatSimSlotNames[g_selectedCombatSimSlotIdx]) != 0) {
					g_frontendNetPacketScratch.packetType = 72;
					*(uint32_t*)g_frontendNetPacketScratch.payload = (uint32_t)g_selectedCombatSimSlotIdx;
					memcpy(&g_frontendNetPacketScratch.payload[4], g_selectedCombatSimSlotNameEditBuffer,
						   sizeof(g_combatSimSlotNames[g_selectedCombatSimSlotIdx]));
					Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 0x1Cu);
				}
				scanIdx = g_selectedCombatSimSlotIdx;
			} else {
				memcpy(&g_combatSimSlots[g_selectedCombatSimSlotIdx], &g_selectedCombatSimSlot,
					   sizeof(g_selectedCombatSimSlot));
				memcpy(g_combatSimSlotNames[g_selectedCombatSimSlotIdx],
					   g_selectedCombatSimSlotNameEditBuffer,
					   sizeof(g_combatSimSlotNames[g_selectedCombatSimSlotIdx]));
				scanIdx = g_selectedCombatSimSlotIdx;
			}

			do {
				if (scanIdx) {
					--scanIdx;
				} else {
					scanIdx = 15;
				}
			} while (g_combatSimSlots[scanIdx].fgIndex == 0xffff);

			g_selectedCombatSimSlotIdx = scanIdx;
			MissionSetup_SyncSlotLoadoutSelection(scanIdx);
			g_missionSetupCraftSelectionChangedFlag = 0;
			g_missionSetupSubpanelMode = 0;
			memcpy(&g_selectedCombatSimSlot, &g_combatSimSlots[g_selectedCombatSimSlotIdx],
				   sizeof(g_selectedCombatSimSlot));
			memcpy(g_selectedCombatSimSlotNameEditBuffer, g_combatSimSlotNames[g_selectedCombatSimSlotIdx],
				   sizeof(g_combatSimSlotNames[g_selectedCombatSimSlotIdx]));
			if (g_selectedCombatSimSlot.craftType) {
				MissionSetup_LoadShipBmpForCraftType(g_selectedCombatSimSlot.craftType);
			} else {
				MissionSetup_UnloadShipBmp();
			}
			MissionSetup_DrawBackgroundAndPreview(0);
		}

		FrontImage_GetResourceRect("lsettingrightu", &spriteRect);
		FrontendDraw_RectCopy(&buttonRect, &spriteRect);
		FrontendDraw_RectOffsetXY(&buttonRect, 582, ((spriteRect.top - spriteRect.bottom + 274) >> 1) + 75);
		if (FrontendButton_DrawSpriteHitTest(&buttonRect, "lsettingrightu", "lsettingrightd",
											 FrontendString_Get(STR_GAME_NEXT_FG), 10, g_colorLightBlue, 72,
											 "settingsound")) {
			if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
				if (canChange.slot && MissionSetup_SelectedSlotHasChanges()) {
					g_frontendNetPacketScratch.packetType = 76;
					*(uint32_t*)g_frontendNetPacketScratch.payload = (uint32_t)g_selectedCombatSimSlotIdx;
					memcpy(&g_frontendNetPacketScratch.payload[4], &g_selectedCombatSimSlot,
						   sizeof(g_selectedCombatSimSlot));
					Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 0x1Cu);
				}
				if (canChange.name && g_selectedCombatSimSlotNameEditBuffer[0] &&
					strcmp(g_selectedCombatSimSlotNameEditBuffer,
						   g_combatSimSlotNames[g_selectedCombatSimSlotIdx]) != 0) {
					g_frontendNetPacketScratch.packetType = 72;
					*(uint32_t*)g_frontendNetPacketScratch.payload = (uint32_t)g_selectedCombatSimSlotIdx;
					memcpy(&g_frontendNetPacketScratch.payload[4], g_selectedCombatSimSlotNameEditBuffer,
						   sizeof(g_combatSimSlotNames[g_selectedCombatSimSlotIdx]));
					Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 0x1Cu);
				}
				scanIdx = g_selectedCombatSimSlotIdx;
			} else {
				memcpy(&g_combatSimSlots[g_selectedCombatSimSlotIdx], &g_selectedCombatSimSlot,
					   sizeof(g_selectedCombatSimSlot));
				memcpy(g_combatSimSlotNames[g_selectedCombatSimSlotIdx],
					   g_selectedCombatSimSlotNameEditBuffer,
					   sizeof(g_combatSimSlotNames[g_selectedCombatSimSlotIdx]));
				scanIdx = g_selectedCombatSimSlotIdx;
			}

			do {
				if (++scanIdx >= 16) {
					scanIdx = 0;
				}
			} while (g_combatSimSlots[scanIdx].fgIndex == 0xffff);

			g_selectedCombatSimSlotIdx = scanIdx;
			MissionSetup_SyncSlotLoadoutSelection(scanIdx);
			memcpy(&g_selectedCombatSimSlot, &g_combatSimSlots[g_selectedCombatSimSlotIdx],
				   sizeof(g_selectedCombatSimSlot));
			g_missionSetupCraftSelectionChangedFlag = 0;
			g_missionSetupSubpanelMode = 0;
			memcpy(g_selectedCombatSimSlotNameEditBuffer, g_combatSimSlotNames[g_selectedCombatSimSlotIdx],
				   sizeof(g_combatSimSlotNames[g_selectedCombatSimSlotIdx]));
			if (g_selectedCombatSimSlot.craftType) {
				MissionSetup_LoadShipBmpForCraftType(g_selectedCombatSimSlot.craftType);
			} else {
				MissionSetup_UnloadShipBmp();
			}
			MissionSetup_DrawBackgroundAndPreview(0);
		}

		return 1;
	}

	return 1;
}

// FUNCTION: XWA 0x554AA0
int MissionSetup_DrawMissionListPanel(void) {
	int mouseX;
	int mouseY;
	int row;
	int scrollOffset;
	int missionIdx;
	uint16_t color;
	FrontendRect rect;
	FrontendRect iconRect;
	char currentSectionName[128];

	FrontendDraw_RectAssign(&rect, 72, 90, 480, 345);
	memset(currentSectionName, 0, sizeof(currentSectionName));
	if (g_missionSetupMissionListRowCount > 17) {
		FrontendDraw_RectAssign(&rect, 551, 90, 570, 345);
		g_missionSetupMissionListScrollOffset =
			FrontendScrollbar_Draw(&rect, g_missionSetupMissionListScrollOffset,
								   g_missionSetupMissionListRowCount, 0, 5, g_colorNavy, 8);
	}

	FrontendCursor_GetPos(&mouseX, &mouseY);
	FrontendDraw_RectAssign(&rect, 110, 90, 550, 104);
	row = 0;
	missionIdx = 0;
	if ((unsigned int)missionIdx < (unsigned int)g_missionCount) {
		scrollOffset = g_missionSetupMissionListScrollOffset;
		do {
			if (g_missionList[missionIdx].lockedFlag) {
				continue;
			}

			if (strcmp(g_missionList[missionIdx].sectionName, currentSectionName) != 0) {
				if (row >= scrollOffset && row - scrollOffset < 19) {
					rect.left -= 37;
					FrontendText_DrawAlignedInRect(12, g_missionList[missionIdx].sectionName, &rect, 0, 1,
												   g_colorOrangeRed);
					rect.left += 37;
					FrontendDraw_RectOffsetXY(&rect, 0, 15);
					scrollOffset = g_missionSetupMissionListScrollOffset;
				}

				strcpy(currentSectionName, g_missionList[missionIdx].sectionName);
				++row;
			}

			if (row - scrollOffset >= 18) {
				break;
			}

			if (row < scrollOffset || row - scrollOffset >= 17) {
				++row;
				if (row - scrollOffset >= 17) {
					break;
				}
				continue;
			}

			color = g_colorPaleBlue;
			if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
				color = g_colorYellow;
				if (FrontendMouse_GetLeftDblClick() || FrontendMouse_GetRightDblClick()) {
					int enabled;

					enabled = 1;
					if (g_gameConfig.sfxDatapadEnabled) {
						FrontendSound_PlayUISound((char*)"jewelsound", enabled, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}

					if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
							if (g_missionList[missionIdx].missionIdx) {
								MissionSetup_LoadSkirmishFile(g_missionList[missionIdx].fileName, enabled);
								g_missionSetupActivePanel = enabled;
							} else {
								memset(g_combatSimSlots, 0, sizeof(g_combatSimSlots));
								FrontendMission_LoadCurrent();
								MissionSetup_CountActiveTeams();
								MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
								MissionSetup_LoadMissionDescText(g_briefingText);
							}

							g_frontendFirstVisibleLine = 0;
							MissionSetup_BroadcastStatePacket(0);
							MissionSetup_BroadcastSkirmishMetadata();
							g_missionSetupActivePanel = enabled;
							g_frontendRightBarAnimState = 0;
							g_activeTextFieldId = 0;
							FrontendSound_PlayUISound((char*)"panelarm", enabled, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						} else if (g_missionList[missionIdx].fileName[0] != '1') {
							g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
								g_missionList[missionIdx].missionIdx;
							MissionSetup_SelectMissionListEntryForMission(g_missionList);
							FrontendMission_LoadCurrent();
							MissionSetup_CountActiveTeams();
							MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
							MissionSetup_LoadMissionDescText(g_briefingText);
							g_frontendFirstVisibleLine = 0;
							MissionSetup_BroadcastStatePacket(0);
							g_missionSetupActivePanel = enabled;
							g_frontendRightBarAnimState = 0;
							g_activeTextFieldId = 0;
							FrontendSound_PlayUISound((char*)"panelarm", enabled, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						} else {
							g_missionSetupActivePanel = enabled;
							g_frontendRightBarAnimState = 0;
							g_activeTextFieldId = 0;
							FrontendSound_PlayUISound((char*)"panelarm", enabled, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
					} else if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
						if (g_missionList[missionIdx].missionIdx) {
							MissionSetup_LoadSkirmishFile(g_missionList[missionIdx].fileName, enabled);
						} else {
							FrontendMission_LoadCurrent();
							MissionSetup_CountActiveTeams();
							MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
							MissionSetup_LoadMissionDescText(g_briefingText);
						}

						g_frontendFirstVisibleLine = 0;
						g_missionSetupActivePanel = enabled;
						g_frontendRightBarAnimState = 0;
						g_activeTextFieldId = 0;
						FrontendSound_PlayUISound((char*)"panelarm", enabled, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					} else {
						g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
							g_missionList[missionIdx].missionIdx;
						MissionSetup_SelectMissionListEntryForMission(g_missionList);
						FrontendMission_LoadCurrent();
						MissionSetup_CountActiveTeams();
						MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
						MissionSetup_LoadMissionDescText(g_briefingText);
						g_frontendFirstVisibleLine = 0;
						g_missionSetupActivePanel = enabled;
						g_frontendRightBarAnimState = 0;
						g_activeTextFieldId = 0;
						FrontendSound_PlayUISound((char*)"panelarm", enabled, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}
				} else if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
					g_missionSetupSelectedMissionListIndex = missionIdx;
				}
			}

			if (g_missionSetupSelectedMissionListIndex == missionIdx) {
				color = g_colorGreen2;
			}

			if (g_missionList[missionIdx].fileName[0] == '1' &&
				g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
				color = g_colorGray;
			}

			FrontendText_DrawAlignedInRect(12, g_missionList[missionIdx].description, &rect, 0, 1, color);
			FrontendDraw_RectOffsetXY(&rect, 0, 15);
			scrollOffset = g_missionSetupMissionListScrollOffset;
			++row;
			if (row - scrollOffset >= 17) {
				break;
			}
		} while ((unsigned int)++missionIdx < (unsigned int)g_missionCount);
	}

	memset(currentSectionName, 0, sizeof(currentSectionName));
	FrontendDraw_RectAssign(&rect, 110, 90, 550, 104);
	row = 0;
	if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
		for (missionIdx = 0; (unsigned int)missionIdx < (unsigned int)g_missionCount; ++missionIdx) {
			if (g_missionList[missionIdx].lockedFlag) {
				continue;
			}

			if (strcmp(g_missionList[missionIdx].sectionName, currentSectionName) != 0) {
				if (row >= g_missionSetupMissionListScrollOffset &&
					row - g_missionSetupMissionListScrollOffset < 17) {
					FrontendDraw_RectOffsetXY(&rect, 0, 15);
				}

				strcpy(currentSectionName, g_missionList[missionIdx].sectionName);
				++row;
			}

			if (row - g_missionSetupMissionListScrollOffset >= 17) {
				break;
			}

			if (row >= g_missionSetupMissionListScrollOffset) {
				int awardId;

				awardId = g_pilotData.combatChamberMissions[g_missionList[missionIdx].missionIdx].awardId;
				if (awardId) {
					FrontendDraw_RectCopy(&iconRect, &rect);
					iconRect.left -= 37;
					iconRect.right = iconRect.left + 37;
					sprintf(g_frontendScratchBuffer, "rcitlvl%d", awardId);
					FrontImage_DrawSprite(g_frontendScratchBuffer, iconRect.left, iconRect.top);
					strcpy(g_frontendScratchBuffer,
						   FrontendString_Get((UIString)(awardId + STR_QUICK_START)));
					FrontendButton_DrawSpriteAtOriginWithTooltip(&iconRect, NULL, g_frontendScratchBuffer, 12,
																 g_colorLightBlue);
				}

				FrontendDraw_RectOffsetXY(&rect, 0, 15);
			}

			++row;
			if (row - g_missionSetupMissionListScrollOffset >= 17) {
				break;
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x549760
int MissionSetup_DrawGameSettingsPanel(void) {
	int mouseX;
	int mouseY;
	int settingCount;
	int remainingSettings;
	int x;
	int canEdit;
	int changed;
	int buttonResult;
	int selectedOrdinal;
	int missionIdx;
	int right;
	int buttonId;
	int selectedMissionIdx;
	int canSelectSlot;
	int canChangeName;
	int actionX;
	int ownerPlayerId;
	int gunnerPlayerId;
	uint16_t fgIndex;
	int category;
	int nextValue;
	FrontendRect rect;
	FrontendRect clippedRect;
	CraftTechStats stats;
	CombatSimLoadoutOptions* loadoutOptions;

	FrontendCursor_GetPos(&mouseX, &mouseY);
	if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_BATTLE_SELECT) {
		selectedOrdinal = g_missionSetupBattleSelectedMissionOrdinal;
		settingCount = g_missionSetupBattleSelectableMissionCount;
		if (settingCount > 5) {
			x = 70 - 100 * selectedOrdinal;
		} else {
			x = 320 - ((100 * settingCount) >> 1);
		}

		changed = 0;
		if (settingCount > 5) {
			if (g_missionSetupCarouselSlideOffset) {
				MissionSetup_UpdateCarouselSlidePosition(&x, settingCount);
			} else {
				MissionSetup_ClampCarouselSelectedOrdinal(settingCount);
			}

			FrontImage_GetResourceRect("settingleftup", &rect);
			FrontendDraw_RectCopy(&clippedRect, &rect);
			FrontendDraw_RectOffsetXY(&clippedRect, rect.left - rect.right + 57,
									  ((rect.top - rect.bottom + 74) >> 1) + 350);
			if (FrontendButton_DrawSpriteHitTest(&clippedRect, "settingleftup", "settingleftdown",
												 FrontendString_Get(STR_GAME_PREVIOUS_MISSION), 10,
												 g_colorLightBlue, 35, "settingsound")) {
				MissionSetup_QueueCarouselSlide(100);
			}

			FrontImage_GetResourceRect("settingrightup", &rect);
			FrontendDraw_RectCopy(&clippedRect, &rect);
			FrontendDraw_RectOffsetXY(&clippedRect, 582, ((rect.top - rect.bottom + 74) >> 1) + 350);
			if (FrontendButton_DrawSpriteHitTest(&clippedRect, "settingrightup", "settingrightdown",
												 FrontendString_Get(STR_GAME_NEXT_MISSION), 10,
												 g_colorLightBlue, 36, "settingsound")) {
				MissionSetup_QueueCarouselSlide(-100);
			}
		}

		FrontendDraw_RectAssign(&rect, 70, 350, 570, 425);
		FrontendDisplay_SetScreenClipRect640x480(&rect);
		if (settingCount > 5) {
			missionIdx = g_missionSetupBattleLastMissionIdx;
			x -= 100;
		} else {
			missionIdx = g_missionSetupBattleFirstMissionIdx;
		}

		right = x + 99;
		buttonId = settingCount + 20;
		for (;;) {
			if (right >= 69) {
				int color;

				FrontendDraw_RectAssign(&rect, x, 351, right, 425);
				color = g_colorPaleBlue;
				FrontendDraw_RectCopy(&clippedRect, &rect);
				FrontendDraw_RectClipToBounds(&clippedRect);
				if (FrontendDraw_PointInRect(&clippedRect, mouseX, mouseY)) {
					color = g_colorYellow;
				}

				FrontImage_SetSpriteFrame("missmall", g_missionList[missionIdx].missionIdx);
				buttonResult = FrontendButton_DrawSpriteHitTest(&rect, "missmall", "missmall", NULL, 10,
																g_colorLightBlue, buttonId, "settingsound");
				FrontendDraw_RectAssign(&rect, x, 351, right, 363);
				sprintf(g_frontendScratchBuffer, "%s %d", FrontendString_Get(STR_MISSION_NUMBER),
						missionIdx - g_missionSetupBattleFirstMissionIdx + 1);
				FrontendText_DrawCentered(10, g_frontendScratchBuffer, &rect, color);
				if (buttonResult) {
					selectedMissionIdx = missionIdx;
					changed = 1;
				}

				--settingCount;
				--buttonId;
				if (settingCount == 0) {
					break;
				}
			}

			right += 100;
			x += 100;
			if (right >= 769) {
				break;
			}
			++missionIdx;
			if (missionIdx > g_missionSetupBattleLastMissionIdx) {
				missionIdx = g_missionSetupBattleFirstMissionIdx;
			}
		}

		if (changed) {
			if (g_missionList != NULL) {
				g_selectedMissionListIndex = selectedMissionIdx;
				g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
					g_missionList[selectedMissionIdx].missionIdx;
				g_missionSetupActivePanel = MISSION_SETUP_PANEL_DEFAULT;
				if (g_frontendRightBarAnimState != 1) {
					g_frontendRightBarAnimState = 0;
					g_frontendRightBarPanelIndex = 1;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				}
				FrontendMission_LoadCurrent();
				MissionSetup_CountActiveTeams();
				MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
				MissionSetup_LoadMissionDescText(g_briefingText);
				g_frontendFirstVisibleLine = 0;
				MissionSetup_RefreshSelectedMissionListIndex();
			}
			g_missionSetupBattleSelectedMissionOrdinal = 0;
			g_missionSetupCarouselQueuedSlideOffset = 0;
			if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && Net_IsHost()) {
				MissionSetup_BroadcastStatePacket(0);
			}
			FrontendDisplay_ResetScreenClipRect();
			MissionSetup_DrawBackgroundAndPreview(0);
		}

		FrontendDisplay_ResetScreenClipRect();
		return 1;
	}

	if (g_missionSetupActivePanel >= MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT &&
		g_missionSetupActivePanel != MISSION_SETUP_PANEL_SKIRMISH_GOALS) {
		FrontendDraw_RectAssign(&rect, 70, 350, 570, 425);
		FrontendDisplay_SetScreenClipRect640x480(&rect);
		if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT) {
			if (!g_missionSetupSubpanelMode) {
				do {
					canSelectSlot = MissionSetup_CanSelectSlotCraft(&g_selectedCombatSimSlot);
					canChangeName = canSelectSlot;
					if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
						if (g_gameConfig.craftSelection == 2 &&
							g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_CLIENT) {
							actionX = 220;
						} else {
							if (!canSelectSlot || !g_missionSetupSlotEditBackupValid) {
								actionX = 220;
							} else {
								actionX = 170;
							}
							if (canSelectSlot) {
								actionX -= 50;
							}
						}
					} else {
						actionX = 270;
					}

					if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, actionX, "donesetting",
																  "donesetting", STR_DONE, 20, mouseX,
																  mouseY)) {
						g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
						g_activeTextFieldId = 0;
						g_frontendRightBarAnimState = 0;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
						if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
							if (canSelectSlot && MissionSetup_SelectedSlotHasChanges()) {
								g_frontendNetPacketScratch.packetType = 76;
								*(uint32_t*)g_frontendNetPacketScratch.payload =
									(uint32_t)g_selectedCombatSimSlotIdx;
								memcpy(&g_frontendNetPacketScratch.payload[4], &g_selectedCombatSimSlot,
									   sizeof(g_selectedCombatSimSlot));
								Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch,
													   0x1Cu);
							}
							if (canChangeName && g_selectedCombatSimSlotNameEditBuffer[0] &&
								strcmp(g_selectedCombatSimSlotNameEditBuffer,
									   g_combatSimSlotNames[g_selectedCombatSimSlotIdx]) != 0) {
								g_frontendNetPacketScratch.packetType = 72;
								*(uint32_t*)g_frontendNetPacketScratch.payload =
									(uint32_t)g_selectedCombatSimSlotIdx;
								memcpy(&g_frontendNetPacketScratch.payload[4],
									   g_selectedCombatSimSlotNameEditBuffer,
									   sizeof(g_combatSimSlotNames[g_selectedCombatSimSlotIdx]));
								Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch,
													   0x1Cu);
							}
						} else {
							memcpy(&g_combatSimSlots[g_selectedCombatSimSlotIdx], &g_selectedCombatSimSlot,
								   sizeof(g_selectedCombatSimSlot));
							if (g_selectedCombatSimSlotNameEditBuffer[0]) {
								memcpy(g_combatSimSlotNames[g_selectedCombatSimSlotIdx],
									   g_selectedCombatSimSlotNameEditBuffer,
									   sizeof(g_combatSimSlotNames[g_selectedCombatSimSlotIdx]));
							}
						}
						if (MissionSetup_IsSkirmishSetupValid()) {
							if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
								g_frontendRightBarPanelIndex =
									g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR ? 4 : 2;
							} else {
								g_frontendRightBarPanelIndex = 2;
							}
						} else {
							g_frontendRightBarPanelIndex = 1;
						}
					}

					actionX += 100;
					if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
						break;
					}

					if (canSelectSlot) {
						if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, actionX, "clearfg",
																	  "clearfg", STR_GAME_CLEAR_FG, 23,
																	  mouseX, mouseY)) {
							loadoutOptions = &g_combatSimLoadoutOptions[g_selectedCombatSimSlotIdx];
							loadoutOptions->selectedWarheadOption = 0;
							loadoutOptions->selectedBeamOption = 0;
							g_selectedCombatSimSlot.craftType = 0;
							g_selectedCombatSimSlot.numberOfWaves = 0;
							g_selectedCombatSimSlot.numberOfCraft = 1;
							g_selectedCombatSimSlot.groupAI = 2;
							g_selectedCombatSimSlot.craftRole = 1;
							g_selectedCombatSimSlot.warhead = 0;
							g_selectedCombatSimSlot.countermeasures = 0;
							g_selectedCombatSimSlot.beam = 0;
							g_selectedCombatSimSlot.primaryFg = 0;
							loadoutOptions->selectedCountermeasureOption = 0;
							FrontendDisplay_ResetScreenClipRect();
							MissionSetup_DrawBackgroundAndPreview(0);
						}
						actionX += 100;
					}

					if (g_gameConfig.craftSelection == 2 &&
						g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_CLIENT) {
						break;
					}

					if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, actionX, "copysetting",
																  "copysetting", STR_GAME_COPY, 21, mouseX,
																  mouseY)) {
						memcpy(&g_missionSetupSlotEditBackup, &g_selectedCombatSimSlot,
							   sizeof(g_missionSetupSlotEditBackup));
						g_missionSetupSlotEditBackupValid = 1;
					}
					actionX += 100;
					if (!canSelectSlot || !g_missionSetupSlotEditBackupValid) {
						break;
					}

					if (!MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, actionX,
																   "pastesetting", "pastesetting",
																   STR_GAME_PASTE, 22, mouseX, mouseY)) {
						break;
					}

					ownerPlayerId = g_selectedCombatSimSlot.ownerPlayerId;
					gunnerPlayerId = g_selectedCombatSimSlot.gunnerPlayerId;
					fgIndex = g_selectedCombatSimSlot.fgIndex;
					memcpy(&g_selectedCombatSimSlot, &g_missionSetupSlotEditBackup,
						   sizeof(g_selectedCombatSimSlot));
					g_selectedCombatSimSlot.ownerPlayerId = ownerPlayerId;
					g_selectedCombatSimSlot.gunnerPlayerId = gunnerPlayerId;
					g_selectedCombatSimSlot.fgIndex = fgIndex;
					if (g_selectedCombatSimSlot.craftType) {
						memset(&stats, 0, sizeof(stats));
						stats.craftType = g_selectedCombatSimSlot.craftType;
						MissionSetup_GetCachedCraftTechStats(&stats);
						if (stats.warheadRating == 0) {
							g_selectedCombatSimSlot.warhead = 0;
						}
					}
					if ((ownerPlayerId || gunnerPlayerId) &&
						!g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].flyable) {
						g_selectedCombatSimSlot.craftType = 0;
					}

					category =
						g_shipList[g_shipTypeToShipListIndex[g_selectedCombatSimSlot.craftType]].category;
					if (category == 6 || category == 7) {
						g_selectedCombatSimSlot.numberOfCraft = 1;
						g_selectedCombatSimSlot.numberOfWaves = 0;
					}
					if (category == 4) {
						g_selectedCombatSimSlot.numberOfWaves = 0;
					}
					if (category != 1 && category != 2) {
						g_selectedCombatSimSlot.beam = 0;
						g_selectedCombatSimSlot.countermeasures = 0;
					}
					switch (category) {
						case 3:
							if (g_selectedCombatSimSlot.craftRole && g_selectedCombatSimSlot.craftRole != 6 &&
								g_selectedCombatSimSlot.craftRole != 5) {
								g_selectedCombatSimSlot.craftRole = 0;
							}
							break;
						case 4:
						case 10:
							g_selectedCombatSimSlot.craftRole = 0;
							break;
						case 5:
							if (g_selectedCombatSimSlot.craftRole > 4u) {
								g_selectedCombatSimSlot.craftRole = 1;
							}
							/* fall through */
						case 6:
							if (g_selectedCombatSimSlot.craftRole > 3u) {
								g_selectedCombatSimSlot.craftRole = 1;
							}
							break;
						case 7:
						case 8:
						case 9:
							if (g_selectedCombatSimSlot.craftRole > 2u) {
								g_selectedCombatSimSlot.craftRole = 1;
							}
							break;
						default:
							break;
					}
					if (g_selectedCombatSimSlot.craftType) {
						MissionSetup_LoadShipBmpForCraftType(g_selectedCombatSimSlot.craftType);
					} else {
						MissionSetup_UnloadShipBmp();
					}
					FrontendDisplay_ResetScreenClipRect();
					MissionSetup_DrawBackgroundAndPreview(0);
					loadoutOptions = &g_combatSimLoadoutOptions[g_selectedCombatSimSlotIdx];
					loadoutOptions->selectedWarheadOption = g_selectedCombatSimSlot.warhead;
					loadoutOptions->selectedBeamOption = g_selectedCombatSimSlot.beam;
					loadoutOptions->selectedCountermeasureOption = g_selectedCombatSimSlot.countermeasures;
					FrontendDisplay_ResetScreenClipRect();
					return 1;
				} while (0);
			} else {
				if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, 270, "cancelsetting",
															  "cancelsetting", STR_CANCEL, 21, mouseX,
															  mouseY)) {
					g_missionSetupSubpanelMode = 0;
					FrontendDisplay_ResetScreenClipRect();
					MissionSetup_DrawBackgroundAndPreview(0);
				}
				FrontendDisplay_ResetScreenClipRect();
				return 1;
			}
		} else if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_SAVE_SKIRMISH) {
			if (g_missionSetupSubpanelMode) {
				if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, 220, "yessetting",
															  "yessetting", STR_YES, 20, mouseX, mouseY)) {
					MissionSetup_SaveSkirmishFile(g_missionSetupSaveNameBuffer, 1);
					strcpy(g_combatSimSkirmishFileName, g_missionSetupSaveNameBuffer);
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
					g_frontendRightBarAnimState = 0;
					g_activeTextFieldId = 0;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				}
				if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, 320, "nosetting",
															  "nosetting", STR_NO, 21, mouseX, mouseY)) {
					g_missionSetupSubpanelMode = 0;
					FrontendDisplay_ResetScreenClipRect();
					return 1;
				}
			} else {
				if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, 220, "savesetting",
															  "savesetting", STR_GAME_SAVE, 20, mouseX,
															  mouseY)) {
					MissionSetup_SaveSkirmishFile(g_missionSetupSaveNameBuffer, 1);
					if (!g_missionSetupSubpanelMode) {
						strcpy(g_combatSimSkirmishFileName, g_missionSetupSaveNameBuffer);
						g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
						g_frontendRightBarAnimState = 0;
						g_activeTextFieldId = 0;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}
				}
				if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, 320, "cancelsetting",
															  "cancelsetting", STR_CANCEL, 21, mouseX,
															  mouseY)) {
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
					g_frontendRightBarAnimState = 0;
					g_activeTextFieldId = 0;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
					FrontendDisplay_ResetScreenClipRect();
					return 1;
				}
			}
		} else if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_MISSION_LIST) {
			if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, 170, "loadsetting",
														  "loadsetting", STR_GAME_LOAD, 20, mouseX, mouseY)) {
				if (g_missionList[g_missionSetupSelectedMissionListIndex].missionIdx) {
					MissionSetup_LoadSkirmishFile(
						g_missionList[g_missionSetupSelectedMissionListIndex].fileName, 1);
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
				} else {
					memset(g_combatSimSlots, 0, sizeof(g_combatSimSlots));
					FrontendMission_LoadCurrent();
					MissionSetup_CountActiveTeams();
					MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
					MissionSetup_LoadMissionDescText(g_briefingText);
				}
				g_frontendFirstVisibleLine = 0;
				MissionSetup_BroadcastStatePacket(0);
				MissionSetup_BroadcastSkirmishMetadata();
				g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
				g_frontendRightBarAnimState = 0;
				g_activeTextFieldId = 0;
				FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
										  63);
			}
			if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, 270, "cancelsetting",
														  "cancelsetting", STR_CANCEL, 21, mouseX, mouseY)) {
				g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
				g_frontendRightBarAnimState = 0;
				g_activeTextFieldId = 0;
				FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
										  63);
			}
			if (MissionSetup_DrawGameSettingsActionButton(&rect, &clippedRect, 370, "deletesetting",
														  "deletesettingd", STR_GAME_DELETE, 22, mouseX,
														  mouseY) &&
				g_missionSetupSelectedMissionListIndex != 0) {
				sprintf(g_frontendScratchBuffer, "%s\\%s", g_campaignDirNames[MISSION_DIRECTORY_SKIRMISH],
						g_missionList[g_missionSetupSelectedMissionListIndex].fileName);
				File_Remove(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer);
				MissionSetup_LoadMissionList((MissionDirectoryId)g_pilotData.missionDirectoryId);
				g_missionSetupSelectedMissionListIndex = 0;
				FrontendDisplay_ResetScreenClipRect();
				return 1;
			}
		}

		FrontendDisplay_ResetScreenClipRect();
		return 1;
	}

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
			settingCount = 4;
		} else if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			settingCount = g_gameConfig.goalType ? 6 : 4;
		} else {
			settingCount = 4;
		}
	} else if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		settingCount = (Net_IsHost() != 0) + 8;
	} else {
		settingCount = (Net_IsHost() != 0) + 5;
	}

	remainingSettings = settingCount;
	if (settingCount > 5) {
		x = 70 - 100 * g_missionSetupBattleSelectedMissionOrdinal;
		if (g_missionSetupCarouselSlideOffset) {
			MissionSetup_UpdateCarouselSlidePosition(&x, settingCount);
		} else {
			MissionSetup_ClampCarouselSelectedOrdinal(settingCount);
		}
		FrontImage_GetResourceRect("settingleftup", &rect);
		FrontendDraw_RectCopy(&clippedRect, &rect);
		FrontendDraw_RectOffsetXY(&clippedRect, rect.left - rect.right + 57,
								  ((rect.top - rect.bottom + 74) >> 1) + 350);
		if (FrontendButton_DrawSpriteHitTest(&clippedRect, "settingleftup", "settingleftdown",
											 FrontendString_Get(STR_GAME_PREVIOUS_SETTING), 10,
											 g_colorLightBlue, 35, "settingsound")) {
			MissionSetup_QueueCarouselSlide(100);
		}
		FrontImage_GetResourceRect("settingrightup", &rect);
		FrontendDraw_RectCopy(&clippedRect, &rect);
		FrontendDraw_RectOffsetXY(&clippedRect, 582, ((rect.top - rect.bottom + 74) >> 1) + 350);
		if (FrontendButton_DrawSpriteHitTest(&clippedRect, "settingrightup", "settingrightdown",
											 FrontendString_Get(STR_GAME_NEXT_SETTING), 10, g_colorLightBlue,
											 36, "settingsound")) {
			MissionSetup_QueueCarouselSlide(-100);
		}
	} else {
		g_missionSetupBattleSelectedMissionOrdinal = 0;
		x = 320 - 50 * settingCount;
	}

	changed = 0;
	canEdit = Net_IsHost() || g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER;
	FrontendDraw_RectAssign(&rect, 70, 350, 570, 425);
	FrontendDisplay_SetScreenClipRect640x480(&rect);
	if (settingCount > 5) {
		x -= 100 * settingCount;
	}

	do {
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
			if (x >= -30) {
				buttonResult =
					MissionSetup_DrawGameSettingsLapsTile(&rect, &clippedRect, x, canEdit, mouseX, mouseY);
				--remainingSettings;
				if (canEdit) {
					if (buttonResult == 1) {
						changed = 1;
						++g_gameConfig.laps;
						if (g_gameConfig.laps > 9u) {
							g_gameConfig.laps = 0;
						}
					} else if (buttonResult == 2) {
						changed = 1;
						g_gameConfig.laps = g_gameConfig.laps ? (uint8_t)(g_gameConfig.laps - 1) : 9;
					}
				}
			}
			x += 100;
			if (x >= 670) {
				break;
			}
		}

		if ((g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER ||
			 g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH || g_gameConfig.goalType) &&
			g_pilotData.missionDirectoryId != MISSION_DIRECTORY_TOUR) {
			if (x >= -30) {
				buttonResult = MissionSetup_DrawGameSettingsTileByteValue(
					&rect, &clippedRect, x, "setting5", NULL, &g_gameConfig.craftSelection,
					STR_CCRAFT_SELECTION, STR_CRAFT_OFF, 24, canEdit, mouseX, mouseY);
				--remainingSettings;
				if (canEdit) {
					if (buttonResult == 1) {
						changed = 1;
						nextValue = g_gameConfig.craftSelection + 1;
						++g_gameConfig.craftSelection;
						if ((unsigned int)nextValue > 2u) {
							nextValue = 0;
							g_gameConfig.craftSelection = 0;
						}
						if (nextValue == 0 && g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
							g_gameConfig.craftSelection = 1;
						}
					} else if (buttonResult == 2) {
						changed = 1;
						g_gameConfig.craftSelection =
							g_gameConfig.craftSelection ? (uint8_t)(g_gameConfig.craftSelection - 1) : 2;
						if (g_gameConfig.craftSelection == 0 &&
							g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
							g_gameConfig.craftSelection = 2;
						}
					}
				}
			}
			x += 100;
			if (x >= 670) {
				break;
			}
		}

		if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
			goto non_skirmish_settings;
		}

		if (x >= -30) {
			buttonResult = MissionSetup_DrawGameSettingsTileNumberValue(
				&rect, &clippedRect, x, "setting8", NULL, g_gameConfig.numberOfTeams >> 1, 1,
				STR_GAME_NUMBER_OF_TEAMS, &g_gameConfig.numberOfTeams, 27, canEdit, mouseX, mouseY);
			--remainingSettings;
			if (canEdit) {
				if (buttonResult == 1) {
					changed = 1;
					nextValue = 2 * g_gameConfig.numberOfTeams;
					if (nextValue > 8) {
						nextValue = 2;
					}
					MissionSetup_SetSkirmishTeamCount(nextValue);
				} else if (buttonResult == 2) {
					changed = 1;
					nextValue = g_gameConfig.numberOfTeams >> 1;
					if (nextValue < 2) {
						nextValue = 8;
					}
					MissionSetup_SetSkirmishTeamCount(nextValue);
				}
			}
		}
		x += 100;
		if (x >= 670) {
			break;
		}

		if (x >= -30) {
			buttonResult = MissionSetup_DrawGameSettingsTileByteValue(
				&rect, &clippedRect, x, "setting9", NULL, &g_gameConfig.environment, STR_GAME_ENVIRONMENT,
				STR_GAME_DEEP_SPACE, 28, canEdit, mouseX, mouseY);
			--remainingSettings;
			if (canEdit) {
				if (buttonResult == 1) {
					changed = 1;
					++g_gameConfig.environment;
					if (g_gameConfig.environment > 2u) {
						g_gameConfig.environment = 0;
					}
				} else if (buttonResult == 2) {
					changed = 1;
					g_gameConfig.environment =
						g_gameConfig.environment ? (uint8_t)(g_gameConfig.environment - 1) : 2;
				}
			}
		}
		x += 100;
		if (x >= 670) {
			break;
		}

		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			if (x >= -30) {
				buttonResult = MissionSetup_DrawGameSettingsMaxPointsTile(&rect, &clippedRect, x, canEdit,
																		  mouseX, mouseY);
				--remainingSettings;
				if (canEdit) {
					if (buttonResult == 1) {
						changed = 1;
						++g_gameConfig.maxPoints;
						if (g_gameConfig.maxPoints > 21u) {
							g_gameConfig.maxPoints = 1;
						}
						if (g_missionSetupActivePanel <= MISSION_SETUP_PANEL_TEAM_ASSIGNMENT) {
							if (!MissionSetup_IsSkirmishSetupValid()) {
								if (g_frontendRightBarPanelIndex != 1) {
									g_frontendRightBarAnimState = 5;
								}
							} else if (g_frontendRightBarPanelIndex != 2) {
								g_frontendRightBarAnimState = 4;
							}
						}
					} else if (buttonResult == 2) {
						changed = 1;
						g_gameConfig.maxPoints =
							g_gameConfig.maxPoints == 1 ? 21 : (uint8_t)(g_gameConfig.maxPoints - 1);
						if (!MissionSetup_IsSkirmishSetupValid()) {
							if (g_frontendRightBarPanelIndex != 1) {
								g_frontendRightBarAnimState = 5;
							}
						} else if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
							if (g_frontendRightBarPanelIndex != 2) {
								g_frontendRightBarAnimState = 4;
							}
						} else if (g_frontendRightBarPanelIndex != 3) {
							g_frontendRightBarAnimState = 6;
						}
					}
				}
			}
			x += 100;
			if (x >= 670) {
				break;
			}
		}

		if (g_gameConfig.goalType) {
			if (x >= -30) {
				buttonResult = MissionSetup_DrawGameSettingsTileNumberValue(
					&rect, &clippedRect, x, "setting4", NULL, 0, 0, STR_GAME_TIME_LIMIT,
					&g_gameConfig.timeLimit, 37, canEdit, mouseX, mouseY);
				--remainingSettings;
				if (canEdit) {
					if (buttonResult == 1) {
						changed = 1;
						if (++g_gameConfig.timeLimit > 20u) {
							g_gameConfig.timeLimit = 1;
						}
					} else if (buttonResult == 2) {
						changed = 1;
						g_gameConfig.timeLimit =
							g_gameConfig.timeLimit == 1 ? 20 : (uint8_t)(g_gameConfig.timeLimit - 1);
					}
				}
			}
			x += 100;
			if (x >= 670) {
				break;
			}
		}

		if (g_gameConfig.environment < 3u) {
			if (x >= -30) {
				buttonResult = MissionSetup_DrawGameSettingsInitialDistanceTile(&rect, &clippedRect, x,
																				canEdit, mouseX, mouseY);
				--remainingSettings;
				if (canEdit) {
					if (buttonResult == 1) {
						changed = 1;
						if (++g_gameConfig.initialDistance > 9u) {
							g_gameConfig.initialDistance = 1;
						}
					} else if (buttonResult == 2) {
						changed = 1;
						g_gameConfig.initialDistance = g_gameConfig.initialDistance == 1
														   ? 9
														   : (uint8_t)(g_gameConfig.initialDistance - 1);
					}
				}
			}
			x += 100;
			if (x >= 670) {
				break;
			}
		}

	non_skirmish_settings:
		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			if (g_gameConfig.goalType != 1 || g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
				if (x >= -30) {
					buttonResult = MissionSetup_DrawGameSettingsLastTeamTimeLimitTile(
						&rect, &clippedRect, x, canEdit, mouseX, mouseY);
					--remainingSettings;
					if (canEdit) {
						if (buttonResult == 1) {
							changed = 1;
							++g_gameConfig.lastTeamTimeLimit;
							if (g_gameConfig.lastTeamTimeLimit > 10u) {
								g_gameConfig.lastTeamTimeLimit = 0;
							}
						} else if (buttonResult == 2) {
							changed = 1;
							g_gameConfig.lastTeamTimeLimit =
								g_gameConfig.lastTeamTimeLimit ? (uint8_t)(g_gameConfig.lastTeamTimeLimit - 1)
															   : 10;
						}
					}
				}
				x += 100;
				if (x >= 670) {
					break;
				}
			}

			if (x >= -30) {
				buttonResult = MissionSetup_DrawGameSettingsTileByteValue(
					&rect, &clippedRect, x, "setting6", NULL, &g_gameConfig.locatePlayers, STR_LOCATE_PLAYERS,
					STR_POFF, 25, canEdit, mouseX, mouseY);
				--remainingSettings;
				if (canEdit) {
					if (buttonResult) {
						changed = 1;
						g_gameConfig.locatePlayers ^= 1u;
					}
				}
			}
			x += 100;
			if (x >= 670) {
				break;
			}

			if (canEdit) {
				if (x >= -30) {
					buttonResult = MissionSetup_DrawGameSettingsTileByteValue(
						&rect, &clippedRect, x, "setting1", NULL, &g_gameConfig.requirePassword,
						STR_JOINING_GAME, STR_JOPEN, 20, 1, mouseX, mouseY);
					--remainingSettings;
					if (buttonResult) {
						changed = 1;
						g_gameConfig.requirePassword ^= 1u;
					}
				}
				x += 100;
				if (x >= 670) {
					break;
				}
			}
		}

		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
			if (x >= -30) {
				buttonResult = MissionSetup_DrawGameSettingsTileByteValue(
					&rect, &clippedRect, x, "setting2", NULL, &g_gameConfig.difficulty, STR_DIFFICULTY,
					STR_EASY, 21, canEdit, mouseX, mouseY);
				--remainingSettings;
				if (canEdit) {
					if (buttonResult == 1) {
						changed = 1;
						++g_gameConfig.difficulty;
						if (g_gameConfig.difficulty > 2u) {
							g_gameConfig.difficulty = 0;
						}
					} else if (buttonResult == 2) {
						changed = 1;
						g_gameConfig.difficulty =
							g_gameConfig.difficulty ? (uint8_t)(g_gameConfig.difficulty - 1) : 2;
					}
				}
			}
			x += 100;
			if (x >= 670) {
				break;
			}
		}

		if (x >= -30) {
			buttonResult = MissionSetup_DrawGameSettingsTileByteValue(
				&rect, &clippedRect, x, "setting3", NULL, &g_gameConfig.collisions, STR_COLLISIONS, STR_OFF,
				22, canEdit, mouseX, mouseY);
			if (canEdit) {
				if (buttonResult) {
					changed = 1;
					g_gameConfig.collisions ^= 1u;
				}
			}
			--remainingSettings;
		}
		x += 100;
		if (x >= 670) {
			break;
		}

		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
			g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
			if (x >= -30) {
				buttonResult = MissionSetup_DrawGameSettingsTileByteValue(
					&rect, &clippedRect, x, "setting13", NULL, &g_gameConfig.invulnerable,
					STR_GAME_INVULNERABLE, STR_OFF, 33, canEdit, mouseX, mouseY);
				if (canEdit) {
					if (buttonResult) {
						changed = 1;
						g_gameConfig.invulnerable ^= 1u;
					}
				}
				--remainingSettings;
			}
			x += 100;
			if (x >= 670) {
				break;
			}
			if (x >= -30) {
				buttonResult = MissionSetup_DrawGameSettingsTileByteValue(
					&rect, &clippedRect, x, "setting14", NULL, &g_gameConfig.unlimitedAmmo,
					STR_GAME_UNLIMITED_AMMO, STR_OFF, 34, canEdit, mouseX, mouseY);
				if (canEdit) {
					if (buttonResult) {
						changed = 1;
						g_gameConfig.unlimitedAmmo ^= 1u;
					}
				}
				--remainingSettings;
			}
			x += 100;
			if (x >= 670) {
				break;
			}
		}
	} while (remainingSettings > 0);

	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && changed == 1) {
		MissionSetup_BroadcastStatePacket(0);
	}
	FrontendDisplay_ResetScreenClipRect();
	return 1;
}

// FUNCTION: XWA 0x545E70
int MissionSetup_DrawMissionTypeControls(void) {
	int result;
	int mouseX;
	int mouseY;
	int isHost;
	int missionTypeChanged;
	int clearSkirmishRequested;
	int rowCount;
	int scanIdx;
	int firstPlayerIff;
	char rightBarName[32] = "rightbar3";
	char leftBarName[32] = "leftbar3";
	char currentSectionName[128];
	FrontendRect rightBarRect;
	FrontendRect leftBarRect;
	FrontendRect buttonRect;

	missionTypeChanged = 0;
	clearSkirmishRequested = 0;
	rightBarName[8] = (char)(g_frontendRightBarPanelIndex + '0');
	leftBarName[7] = (char)(g_frontendLeftBarPanelIndex + '0');
	FrontendCursor_GetPos(&mouseX, &mouseY);
	FrontImage_GetResourceRect(leftBarName, &leftBarRect);
	FrontImage_GetResourceRect(rightBarName, &rightBarRect);

	if (!g_frontendRightBarAnimState) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (FrontImage_GetSpriteFrame(rightBarName) == 9) {
			g_frontendRightBarAnimState = 1;
		}
	} else if (g_frontendRightBarAnimState == 1) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
	} else if (g_frontendRightBarAnimState == 2) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			g_frontendRightBarAnimState = 3;
		}
	} else if (g_frontendRightBarAnimState == 4) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			g_frontendRightBarAnimState = 0;
			g_frontendRightBarPanelIndex = 2;
		}
	} else if (g_frontendRightBarAnimState == 5) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			g_frontendRightBarAnimState = 0;
			g_frontendRightBarPanelIndex = 1;
		}
	} else if (g_frontendRightBarAnimState == 6) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			g_frontendRightBarAnimState = 0;
			g_frontendRightBarPanelIndex = 3;
		}
	} else if (g_frontendRightBarAnimState == 7) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			g_frontendRightBarAnimState = 0;
			g_frontendRightBarPanelIndex = 4;
		}
	}

	if (!g_frontendLeftBarAnimState) {
		FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(leftBarName, 1);
		if (FrontImage_GetSpriteFrame(leftBarName) == 9) {
			g_frontendLeftBarAnimState = 1;
		}
		return 0;
	} else if (g_frontendLeftBarAnimState == 1) {
		FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
		FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[g_frontendLeftBarPanelIndex - 1]);
	} else if (g_frontendLeftBarAnimState == 2) {
		FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(leftBarName, 1);
		result = FrontImage_GetSpriteFrame(leftBarName);
		if (!result) {
			g_frontendLeftBarAnimState = 3;
			return result;
		}
		return 0;
	} else if (g_frontendLeftBarAnimState == 4) {
		FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(leftBarName, 1);
		if (!FrontImage_GetSpriteFrame(leftBarName)) {
			FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			g_frontendLeftBarAnimState = 0;
			g_frontendLeftBarPanelIndex = 2;
			FrontImage_SetSpriteFrame("leftbar2", 0);
		}
	} else if (g_frontendLeftBarAnimState == 5) {
		FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(leftBarName, 1);
		if (!FrontImage_GetSpriteFrame(leftBarName)) {
			FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			g_frontendLeftBarAnimState = 0;
			g_frontendLeftBarPanelIndex =
				(((g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_NET_CLIENT) - 1) & -3) + 5;
			FrontImage_SetSpriteFrame("leftbar4", 0);
		}
	} else {
		return 0;
	}

	isHost = Net_IsHost();
	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		isHost = 1;
	}
	if (g_frontendLeftBarAnimState != 1) {
		return 0;
	}

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH &&
		g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_NET_CLIENT) {
		FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[2]);
		if (FrontendButton_DrawSpriteWithHoverText(
				&buttonRect, (char*)"load", (char*)"load", (void*)FrontendString_Get(STR_GAME_LOAD),
				(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 13, (char*)"jewelsound")) {
			g_missionSetupActivePanel = MISSION_SETUP_PANEL_MISSION_LIST;
			g_missionSetupCarouselSlideOffset = 0;
			g_missionSetupCarouselQueuedSlideOffset = 0;
			g_missionSetupSelectedMissionListIndex = 0;
			g_missionSetupMissionListRowCount = g_missionCount;
			memset(currentSectionName, 0, sizeof(currentSectionName));
			rowCount = g_missionCount;
			if (g_missionCount > 0) {
				scanIdx = 0;
				do {
					if (g_missionList[scanIdx].lockedFlag) {
						g_missionSetupMissionListRowCount = --rowCount;
					} else {
						if (g_missionList[scanIdx].missionIdx ==
							g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId]) {
							g_missionSetupMissionListScrollOffset = scanIdx;
						}
						if (strcmp(g_missionList[scanIdx].sectionName, currentSectionName) != 0) {
							g_missionSetupMissionListRowCount = ++rowCount;
							strcpy(currentSectionName, g_missionList[scanIdx].sectionName);
						}
					}
					result = ++scanIdx;
				} while (result < g_missionCount);
			}
			if (rowCount < 18) {
				g_missionSetupMissionListScrollOffset = 0;
			}
			if (g_frontendRightBarAnimState != 3) {
				g_frontendRightBarAnimState = 2;
				FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
										  63);
			}
		}

		FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[3]);
		if (FrontendButton_DrawSpriteWithHoverText(
				&buttonRect, (char*)"save", (char*)"save", (void*)FrontendString_Get(STR_GAME_SAVE),
				(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 14, (char*)"jewelsound")) {
			g_missionSetupActivePanel = MISSION_SETUP_PANEL_SAVE_SKIRMISH;
			g_missionSetupCarouselSlideOffset = 0;
			g_missionSetupCarouselQueuedSlideOffset = 0;
			g_missionSetupSubpanelMode = 0;
			if (g_combatSimSkirmishFileName[0]) {
				strcpy(g_missionSetupSaveNameBuffer, g_combatSimSkirmishFileName);
			} else {
				strcpy(g_missionSetupSaveNameBuffer, FrontendString_Get(STR_QUICK_SKIRMISH));
			}
			if (g_frontendRightBarAnimState != 3) {
				g_frontendRightBarAnimState = 2;
				FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
										  63);
			}
		}

		FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[4]);
		if (FrontendButton_DrawSpriteWithHoverText(
				&buttonRect, (char*)"clear", (char*)"clear", (void*)FrontendString_Get(STR_GAME_NEW_SKIRMISH),
				(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 15, (char*)"jewelsound")) {
			clearSkirmishRequested = 1;
		}
	}

	FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[1]);
	if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
		if (isHost) {
			if (FrontendButton_DrawSpriteWithHoverText(
					&buttonRect, (char*)"skirmish", (char*)"skirmish",
					(void*)FrontendString_Get(STR_QUICK_SKIRMISH), (unsigned int)g_colorPaleBlue,
					(unsigned int)g_colorLightBlue, 18, (char*)"jewelsound")) {
				strcpy(g_pilotData.missionFileName, "temp.tie");
				missionTypeChanged = 1;
				g_pilotData.missionDirectoryId = MISSION_DIRECTORY_SKIRMISH;
				g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
				g_missionSetupBattleSelectedMissionOrdinal = 0;
				if (g_frontendRightBarAnimState != 1) {
					g_frontendRightBarAnimState = 0;
					g_frontendRightBarPanelIndex = 1;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				}
				g_frontendLeftBarAnimState = 5;
				FrontendMission_LoadCurrent();
				MissionSetup_CountActiveTeams();
				MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
				MissionSetup_LoadSkirmishFile("temp\\temp6753908.skm", 0);
				MissionSetup_LoadMissionDescText(g_briefingText);
				g_frontendFirstVisibleLine = 0;
				if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
					for (scanIdx = 0; scanIdx < 16; ++scanIdx) {
						if (g_combatSimSlots[scanIdx].craftType &&
							g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[scanIdx].craftType]]
								.flyable &&
							!g_combatSimSlots[scanIdx].ownerPlayerId) {
							g_combatSimSlots[scanIdx].ownerPlayerId = g_mpRoster[0].playerId;
							break;
						}
					}
				}
				if (g_missionList != NULL) {
					scanIdx = 0;
					g_selectedMissionListIndex = 0;
					if (g_missionCount != 0) {
						result = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
						do {
							if (g_missionList[scanIdx].missionIdx == result) {
								break;
							}
							g_selectedMissionListIndex = ++scanIdx;
						} while (scanIdx < g_missionCount);
					}
				}
			}
		} else {
			FrontendButton_DrawCenteredTintedSpriteWithTooltip(&buttonRect, "skirmish",
															   FrontendString_Get(STR_QUICK_SKIRMISH),
															   (unsigned int)g_colorLightBlue);
		}
	} else {
		FrontendButton_DrawCenteredTintedSpriteWithTooltip(
			&buttonRect, "skirmish", FrontendString_Get(STR_QUICK_SKIRMISH), (unsigned int)g_colorGreen);
	}

	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		FrontendDraw_RectCopy(&buttonRect, g_frontendSidebarButtonRects);
		if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_MELEE) {
			if (isHost) {
				if (FrontendButton_DrawSpriteWithHoverText(
						&buttonRect, (char*)"yard", (char*)"yard", (void*)FrontendString_Get(STR_MELEE),
						(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 12,
						(char*)"jewelsound")) {
					MissionSetup_SaveSkirmishFile("temp\\temp6753908", 0);
					missionTypeChanged = 1;
					g_pilotData.missionDirectoryId = MISSION_DIRECTORY_MELEE;
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
					g_missionSetupSlotSummaryMode = MISSION_SETUP_SLOT_SUMMARY_CRAFT;
					g_missionSetupBattleSelectedMissionOrdinal = 0;
					if (g_frontendRightBarAnimState != 1) {
						g_frontendRightBarAnimState = 0;
						g_frontendRightBarPanelIndex = 1;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}
					g_frontendLeftBarAnimState = 4;
					if (g_gameConfig.difficulty == 3) {
						g_gameConfig.difficulty = 0;
					}
					FrontendMission_LoadCurrent();
					MissionSetup_CountActiveTeams();
					MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
					MissionSetup_LoadMissionDescText(g_briefingText);
					g_frontendFirstVisibleLine = 0;
					if (g_missionList != NULL) {
						scanIdx = 0;
						g_selectedMissionListIndex = 0;
						if (g_missionCount != 0) {
							result = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
							while (g_missionList[scanIdx].missionIdx != result) {
								g_selectedMissionListIndex = ++scanIdx;
								if (scanIdx >= g_missionCount) {
									break;
								}
							}
							if (scanIdx < g_missionCount && g_missionList[scanIdx].lockedFlag) {
								result = 0;
								if (g_missionCount > 0) {
									while (g_missionList[result].lockedFlag) {
										++result;
										if (result >= g_missionCount) {
											break;
										}
									}
									if (result < g_missionCount) {
										g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
											g_missionList[result].missionIdx;
										g_selectedMissionListIndex = result;
									}
								}
								FrontendMission_LoadCurrent();
								MissionSetup_CountActiveTeams();
								MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
								MissionSetup_LoadMissionDescText(g_briefingText);
							}
						}
					}
				}
			} else {
				FrontendButton_DrawCenteredTintedSpriteWithTooltip(
					&buttonRect, "yard", FrontendString_Get(STR_MELEE), (unsigned int)g_colorLightBlue);
			}
		} else {
			FrontendButton_DrawCenteredTintedSpriteWithTooltip(
				&buttonRect, "yard", FrontendString_Get(STR_MELEE), (unsigned int)g_colorGreen);
			if (isHost && FrontendDraw_PointInRect(&buttonRect, mouseX, mouseY)) {
				if (FrontendMouse_GetRightClick()) {
					if (g_missionList != NULL) {
						if (g_gameConfig.sfxDatapadEnabled) {
							FrontendSound_PlayUISound((char*)"jewelsound", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
						result = g_selectedMissionListIndex;
						do {
							if (result) {
								--result;
							} else {
								result = g_missionCount - 1;
							}
							g_selectedMissionListIndex = result;
						} while (g_missionList[result].lockedFlag);
						g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
							g_missionList[result].missionIdx;
						if (g_frontendRightBarAnimState != 1) {
							g_frontendRightBarAnimState = 0;
							g_frontendRightBarPanelIndex = 1;
							FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
						FrontendMission_LoadCurrent();
						MissionSetup_CountActiveTeams();
						MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
						MissionSetup_LoadMissionDescText(g_briefingText);
						g_frontendFirstVisibleLine = 0;
						if (g_missionList != NULL) {
							scanIdx = 0;
							g_selectedMissionListIndex = 0;
							if (g_missionCount != 0) {
								result = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
								do {
									if (g_missionList[scanIdx].missionIdx == result) {
										break;
									}
									g_selectedMissionListIndex = ++scanIdx;
								} while (scanIdx < g_missionCount);
							}
						}
					}
				} else if (FrontendMouse_GetLeftClick()) {
					if (g_missionList != NULL) {
						if (g_gameConfig.sfxDatapadEnabled) {
							FrontendSound_PlayUISound((char*)"jewelsound", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
						result = g_selectedMissionListIndex;
						do {
							if (result == g_missionCount - 1) {
								result = 0;
							} else {
								++result;
							}
							g_selectedMissionListIndex = result;
						} while (g_missionList[result].lockedFlag);
						g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
							g_missionList[result].missionIdx;
						if (g_frontendRightBarAnimState != 1) {
							g_frontendRightBarAnimState = 0;
							g_frontendRightBarPanelIndex = 1;
							FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
						FrontendMission_LoadCurrent();
						MissionSetup_CountActiveTeams();
						MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
						MissionSetup_LoadMissionDescText(g_briefingText);
						g_frontendFirstVisibleLine = 0;
						if (g_missionList != NULL) {
							scanIdx = 0;
							g_selectedMissionListIndex = 0;
							if (g_missionCount != 0) {
								result = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
								do {
									if (g_missionList[scanIdx].missionIdx == result) {
										break;
									}
									g_selectedMissionListIndex = ++scanIdx;
								} while (scanIdx < g_missionCount);
							}
						}
					}
				}
				if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
					if (Net_IsHost()) {
						MissionSetup_BroadcastStatePacket(0);
					}
				}
			}
		}
	}

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		FrontendDraw_RectCopy(&buttonRect, g_frontendSidebarButtonRects);
		if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_TOUR) {
			if (isHost && g_missionSetupTourButtonEnabled) {
				if (FrontendButton_DrawSpriteWithHoverText(
						&buttonRect, (char*)"todrecord", (char*)"todrecord",
						(void*)FrontendString_Get(STR_TOUR), (unsigned int)g_colorPaleBlue,
						(unsigned int)g_colorLightBlue, 11, (char*)"jewelsound")) {
					MissionSetup_SaveSkirmishFile("temp\\temp6753908", 0);
					missionTypeChanged = 1;
					g_pilotData.missionDirectoryId = MISSION_DIRECTORY_TOUR;
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_BATTLE_SELECT;
					if (g_frontendRightBarAnimState != 1) {
						g_frontendRightBarAnimState = 0;
						g_frontendRightBarPanelIndex = 1;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}
					g_frontendLeftBarAnimState = 4;
					if (g_gameConfig.difficulty == 3) {
						g_gameConfig.difficulty = 0;
					}
					FrontendMission_LoadCurrent();
					MissionSetup_CountActiveTeams();
					MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
					MissionSetup_LoadMissionDescText(g_briefingText);
					g_frontendFirstVisibleLine = 0;
					MissionSetup_SelectFirstUnlockedIfCurrentLocked();
					{
						int battleNumber;
						MissionListEntry* entry;

						scanIdx = g_selectedMissionListIndex;
						entry = &g_missionList[scanIdx];
						g_missionSetupBattleSelectedMissionOrdinal = 0;
						g_missionSetupCarouselSlideOffset = 0;
						g_missionSetupCarouselQueuedSlideOffset = 0;
						if (!g_pilotData.tourOfDutyMissions[entry->missionIdx].completedCount) {
							g_selectedMissionListIndex = scanIdx - 1;
							entry = &g_missionList[scanIdx - 1];
							g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
								entry->missionIdx;
						}

						strcpy(g_missionSetupCurrentBattleSectionName, entry->sectionName);
						sscanf(entry->fileName, "%db%d", &battleNumber, &frame);
						scanIdx = 0;
						while (scanIdx < g_selectedMissionListIndex &&
							   strcmp(g_missionList[scanIdx].sectionName,
									  g_missionSetupCurrentBattleSectionName) != 0) {
							++scanIdx;
						}
						g_missionSetupBattleFirstMissionIdx = scanIdx;

						while (scanIdx < g_missionCount &&
							   strcmp(g_missionList[scanIdx].sectionName,
									  g_missionSetupCurrentBattleSectionName) == 0 &&
							   g_pilotData.tourOfDutyMissions[g_missionList[scanIdx].missionIdx]
								   .completedCount) {
							++scanIdx;
						}
						g_missionSetupBattleLastMissionIdx = scanIdx - 1;

						if (g_selectedMissionListIndex < g_missionSetupBattleFirstMissionIdx ||
							g_selectedMissionListIndex > g_missionSetupBattleLastMissionIdx) {
							g_missionSetupBattleSelectedMissionOrdinal = 0;
						} else {
							g_missionSetupBattleSelectedMissionOrdinal =
								g_selectedMissionListIndex - g_missionSetupBattleFirstMissionIdx;
						}

						g_missionSetupBattleSelectableMissionCount = 0;
						scanIdx = g_missionSetupBattleFirstMissionIdx;
						if (scanIdx <= g_missionSetupBattleLastMissionIdx) {
							do {
								if (!g_missionList[scanIdx].lockedFlag &&
									g_pilotData.tourOfDutyMissions[g_missionList[scanIdx].missionIdx]
										.completedCount) {
									++g_missionSetupBattleSelectableMissionCount;
								}
								result = ++scanIdx;
							} while (result <= g_missionSetupBattleLastMissionIdx);
						}
					}
				}
			} else {
				FrontendButton_DrawCenteredTintedSpriteWithTooltip(
					&buttonRect, "todrecord", FrontendString_Get(STR_TOUR), (unsigned int)g_colorGray);
			}
		} else {
			FrontendButton_DrawCenteredTintedSpriteWithTooltip(
				&buttonRect, "todrecord", FrontendString_Get(STR_TOUR), (unsigned int)g_colorGreen);
			if (isHost && FrontendDraw_PointInRect(&buttonRect, mouseX, mouseY)) {
				if (FrontendMouse_GetRightClick()) {
					if (g_missionList != NULL) {
						if (g_gameConfig.sfxDatapadEnabled) {
							FrontendSound_PlayUISound((char*)"jewelsound", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
						result = g_selectedMissionListIndex;
						do {
							if (result) {
								--result;
							} else {
								result = g_missionCount - 1;
							}
							g_selectedMissionListIndex = result;
						} while (g_missionList[result].lockedFlag ||
								 (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR &&
								  !g_pilotData.tourOfDutyMissions[g_missionList[result].missionIdx]
									   .completedCount));
						g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
							g_missionList[result].missionIdx;
						rowCount = (int16_t)g_frontendMission->flightGroupCount;
						for (scanIdx = 0; scanIdx < rowCount; ++scanIdx) {
							if (g_frontendMission->flightGroups[scanIdx].playerNumber) {
								break;
							}
						}
						firstPlayerIff = g_frontendMission->flightGroups[scanIdx].iff;
						g_missionSetupActivePanel = MISSION_SETUP_PANEL_DEFAULT;
						if (g_frontendRightBarAnimState != 1) {
							g_frontendRightBarAnimState = 0;
							g_frontendRightBarPanelIndex = 1;
							FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
						FrontendMission_LoadCurrent();
						rowCount = (int16_t)g_frontendMission->flightGroupCount;
						for (scanIdx = 0; scanIdx < rowCount; ++scanIdx) {
							if (g_frontendMission->flightGroups[scanIdx].playerNumber) {
								break;
							}
						}
						if (firstPlayerIff != g_frontendMission->flightGroups[scanIdx].iff) {
							missionTypeChanged = 1;
						}
						MissionSetup_CountActiveTeams();
						MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
						MissionSetup_LoadMissionDescText(g_briefingText);
						g_frontendFirstVisibleLine = 0;
						if (g_missionList != NULL) {
							scanIdx = 0;
							g_selectedMissionListIndex = 0;
							if (g_missionCount != 0) {
								result = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
								do {
									if (g_missionList[scanIdx].missionIdx == result) {
										break;
									}
									g_selectedMissionListIndex = ++scanIdx;
								} while (scanIdx < g_missionCount);
							}
						}
					}
					if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
						Net_IsHost()) {
						MissionSetup_BroadcastStatePacket(0);
					}
				} else if (FrontendMouse_GetLeftClick()) {
					if (g_missionList != NULL) {
						if (g_gameConfig.sfxDatapadEnabled) {
							FrontendSound_PlayUISound((char*)"jewelsound", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
						result = g_selectedMissionListIndex;
						do {
							if (result == g_missionCount - 1) {
								result = 0;
							} else {
								++result;
							}
							g_selectedMissionListIndex = result;
						} while (g_missionList[result].lockedFlag ||
								 (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR &&
								  !g_pilotData.tourOfDutyMissions[g_missionList[result].missionIdx]
									   .completedCount));
						g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
							g_missionList[result].missionIdx;
						rowCount = (int16_t)g_frontendMission->flightGroupCount;
						for (scanIdx = 0; scanIdx < rowCount; ++scanIdx) {
							if (g_frontendMission->flightGroups[scanIdx].playerNumber) {
								break;
							}
						}
						firstPlayerIff = g_frontendMission->flightGroups[scanIdx].iff;
						g_missionSetupActivePanel = MISSION_SETUP_PANEL_DEFAULT;
						if (g_frontendRightBarAnimState != 1) {
							g_frontendRightBarAnimState = 0;
							g_frontendRightBarPanelIndex = 1;
							FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
						FrontendMission_LoadCurrent();
						rowCount = (int16_t)g_frontendMission->flightGroupCount;
						for (scanIdx = 0; scanIdx < rowCount; ++scanIdx) {
							if (g_frontendMission->flightGroups[scanIdx].playerNumber) {
								break;
							}
						}
						if (firstPlayerIff != g_frontendMission->flightGroups[scanIdx].iff) {
							missionTypeChanged = 1;
						}
						MissionSetup_CountActiveTeams();
						MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
						MissionSetup_LoadMissionDescText(g_briefingText);
						g_frontendFirstVisibleLine = 0;
						if (g_missionList != NULL) {
							scanIdx = 0;
							g_selectedMissionListIndex = 0;
							if (g_missionCount != 0) {
								result = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
								do {
									if (g_missionList[scanIdx].missionIdx == result) {
										break;
									}
									g_selectedMissionListIndex = ++scanIdx;
								} while (scanIdx < g_missionCount);
							}
						}
					}
					if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
						Net_IsHost()) {
						MissionSetup_BroadcastStatePacket(0);
					}
				}
			}
		}
	}

	if (clearSkirmishRequested) {
		if (FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_GAME_CLEAR_SKIRMISH_CONFIRM1),
											 FrontendString_Get(STR_GAME_CLEAR_SKIRMISH_CONFIRM2),
											 FrontendString_Get(STR_GAME_CLEAR_SKIRMISH_CONFIRM3),
											 FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL))) {
			memset(g_combatSimSlots, 0, sizeof(g_combatSimSlots));
			FrontendMission_LoadCurrent();
			MissionSetup_CountActiveTeams();
			MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
			g_frontendFirstVisibleLine = 0;
			if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
				MissionSetup_BroadcastStatePacket(0);
				MissionSetup_BroadcastSkirmishMetadata();
			}
		}
	}

	if (missionTypeChanged) {
		FrontImage_FreeResourceByName("background");
		MissionSetup_DrawBackgroundAndPreview(0);
	}

	return missionTypeChanged;
}

// FUNCTION: XWA 0x5483D0
void MissionSetup_BuildBriefingText(char* outText) {
	unsigned int selectedMissionIdx;
	MissionListEntry* missionEntry;
	XwaFile* missionFile;
	int extensionPos;
	int battleNumber;
	int missionNumber;
	int16_t missionFormatVersion;
	int segmentPos;
	int segmentIdx;
	int copyIdx;
#ifndef XWA_MODERN
	int controlPos;
	int closePos;
#endif
	char ch;
#ifndef XWA_MODERN
	char replacementChar;
#endif
	char missionBaseName[128];
	char textTailBuffer[MISSION_TEXT_BUFFER_SIZE];
	char segmentSource[MISSION_TEXT_BUFFER_SIZE];
	char segmentText[MISSION_TEXT_BUFFER_SIZE];

	if (outText == NULL) {
		return;
	}

	memset(outText, 0, MISSION_TEXT_BUFFER_SIZE);
	memset(textTailBuffer, 0, sizeof(textTailBuffer));

	selectedMissionIdx = 0;
	while (selectedMissionIdx < (unsigned int)g_missionCount &&
		   g_missionList[selectedMissionIdx].missionIdx !=
			   g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId]) {
		++selectedMissionIdx;
	}

	if (selectedMissionIdx >= (unsigned int)g_missionCount) {
		return;
	}

	missionEntry = g_missionList;
	missionEntry += selectedMissionIdx;
	sprintf(g_frontendScratchBuffer, "%s\\%s", g_campaignDirNames[g_pilotData.missionDirectoryId],
			missionEntry->fileName);
	missionFile = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "rb");
	if (missionFile == NULL) {
		return;
	}

	strcpy(missionBaseName, g_missionList[selectedMissionIdx].fileName);
	extensionPos = (int)strlen(missionBaseName) - 1;
	while (extensionPos > 0) {
		if (missionBaseName[extensionPos] == '.') {
			missionBaseName[extensionPos] = '\0';
			break;
		}

		--extensionPos;
	}

	sscanf(g_missionList[selectedMissionIdx].fileName, "%c%c%d%c%d", &textTailBuffer[0], &textTailBuffer[1],
		   &battleNumber, &textTailBuffer[2], &missionNumber);

	File_ReadWord(missionFile, &missionFormatVersion);
	if (missionFormatVersion != 18 && missionFormatVersion != 17) {
		if (missionFormatVersion == 12) {
			File_Seek(missionFile, -MISSION_LEGACY_TEXT_BUFFER_SIZE, SEEK_END);
			File_ReadCount(missionFile, textTailBuffer,
						   (size_t)(MISSION_LEGACY_TEXT_BUFFER_SIZE - extensionPos));
			textTailBuffer[MISSION_LEGACY_TEXT_BUFFER_SIZE - 1] = '\0';
		} else {
			outText[0] = '\0';
			File_Close(missionFile);
			return;
		}
	} else {
		File_Seek(missionFile, -MISSION_TEXT_BUFFER_SIZE, SEEK_END);
		File_ReadCount(missionFile, textTailBuffer, (size_t)(MISSION_TEXT_BUFFER_SIZE - extensionPos));
		textTailBuffer[MISSION_TEXT_BUFFER_SIZE - 1] = '\0';
	}

	if (textTailBuffer[0] != '\0') {
		strcpy(segmentSource, textTailBuffer);
		outText[0] = '\0';

		for (segmentPos = 0; segmentPos < MISSION_TEXT_BUFFER_SIZE; ++segmentPos) {
			ch = segmentSource[segmentPos];
			if (ch == '#' || ch == '\0') {
				break;
			}
		}

		if (segmentPos < MISSION_TEXT_BUFFER_SIZE && segmentSource[segmentPos] != '\0') {
			++segmentPos;
			for (segmentIdx = 1;; ++segmentIdx) {
				if (segmentPos < MISSION_TEXT_BUFFER_SIZE) {
					do {
						if (segmentSource[segmentPos] == '$') {
							++segmentPos;
							continue;
						}

						break;
					} while (segmentPos < MISSION_TEXT_BUFFER_SIZE);
				}

				copyIdx = 0;
				while (segmentPos < MISSION_TEXT_BUFFER_SIZE) {
					ch = segmentSource[segmentPos];
					if (ch == '\0' || ch == '$') {
						break;
					}

					++segmentPos;
					segmentText[copyIdx++] = ch;
				}

				segmentText[copyIdx] = '\0';
				sprintf(textTailBuffer, "!S0%d0%d0%d!", (unsigned char)battleNumber,
						(unsigned char)missionNumber, segmentIdx);
				strcat(textTailBuffer, segmentText);
				strcat(outText, Linez_ResolveString(textTailBuffer));
				if (segmentSource[segmentPos] == '\0' || segmentPos >= MISSION_TEXT_BUFFER_SIZE) {
					break;
				}

				strcat(outText, "$$");
			}
		}
	}

#ifdef XWA_MODERN
	MissionSetup_ConvertBracketControlCodes(outText);
#else
	controlPos = 1;
	do {
		if (outText[controlPos - 1] == '[') {
			outText[controlPos - 1] = 6;
			closePos = controlPos;
			if (controlPos < MISSION_TEXT_BUFFER_SIZE) {
				while (closePos < MISSION_TEXT_BUFFER_SIZE) {
					if (outText[closePos] == ']') {
						replacementChar = outText[closePos + 1];
						outText[closePos] = replacementChar;
						outText[closePos + 1] = 1;
						break;
					}

					++closePos;
				}
			}
		}

		++controlPos;
	} while (controlPos - 1 < MISSION_TEXT_BUFFER_SIZE);
#endif
	File_Close(missionFile);
}

// FUNCTION: XWA 0x5438B0
int MissionSetup_Update(int frameCounter) {
	size_t specSize;
	int i;
	int j;
	int packetType;
	int outCount;
	int rosterIdx;
	int slotIdx;
	int changed;
	int battleNumber;
	int missionNumber;
	int titlePrefixLen;
	int scanIdx;
	int lastMissionIdx;
	MissionSetupActivePanel oldActivePanel;
	int draggedPlayerId;
	int maxRosterEntries;
	int playerId;
	unsigned int selectedIndex;
	uint32_t tickNow;
	XwaFile* specFile;
	NetPlayerInfo* playerRoster;
	MissionListEntry* missionEntry;
	FrontendRect rect;
	char missionNumberBuffer[32];
#ifdef XWA_MODERN
	static int entryMoviePending;

	if (!frameCounter || entryMoviePending) {
#else
	if (!frameCounter) {
#endif
		if (g_optMultiRegion != 0) {
			g_gameConfig.eachTeamOwnRegion = 1;
		} else {
			g_gameConfig.eachTeamOwnRegion = 0;
		}
		g_missionSetupTourButtonEnabled = 1;
#ifdef XWA_MODERN
		if (!entryMoviePending && !g_skipFrontendEntryMovie &&
			g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER && Movie_Play("pod", 1)) {
			entryMoviePending = 1;
			return 0;
		}
		entryMoviePending = 0;
#else
		if (!g_skipFrontendEntryMovie &&
			g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			Movie_Play("pod", 1);
		}
#endif
		if (g_frontendMissionLoaded) {
			if ((unsigned int)g_currentMissionId < 7u) {
				musicState = MUSIC_STATE_FRONTEND_1210;
				if (g_gameConfig.datapadMusicEnabled) {
					Music_SetState(MUSIC_STATE_FRONTEND_1210);
				} else {
					Music_Stop();
				}
			} else {
				musicState = MUSIC_STATE_FRONTEND_1240;
				if (g_gameConfig.datapadMusicEnabled) {
					Music_SetState(MUSIC_STATE_FRONTEND_1240);
				} else {
					Music_Stop();
				}
			}
		} else {
			musicState = MUSIC_STATE_FRONTEND_1240;
			if (g_gameConfig.datapadMusicEnabled) {
				Music_SetState(MUSIC_STATE_FRONTEND_1240);
			} else {
				Music_Stop();
			}
		}

		specFile = File_Open(AERON_VFS_ROOT_ASSET, "spec.rci", "rb");
		if (specFile != NULL) {
			specSize = (size_t)File_GetSize(specFile);
			if (g_cachedCraftTechStats != NULL) {
				Mem_Free(g_cachedCraftTechStats);
				g_cachedCraftTechStats = NULL;
			}
			g_cachedCraftTechStats = (CraftTechStats*)Mem_Alloc((size_t)specSize);
			if (g_cachedCraftTechStats != NULL) {
				File_ReadCount(specFile, g_cachedCraftTechStats, (size_t)specSize);
				g_cachedCraftTechStatsCount = specSize / 0x30;
			}
			File_Close(specFile);
		}

		MissionSetup_LoadBattleSprites();
		FrontendText_SetGlyphGradientBg(g_colorNearBlack);
		FrontendCursor_SetPos(
			g_frontendSidebarButtonRects[9].left +
				((g_frontendSidebarButtonRects[9].right - g_frontendSidebarButtonRects[9].left) >> 1),
			g_frontendSidebarButtonRects[9].top +
				((g_frontendSidebarButtonRects[9].bottom - g_frontendSidebarButtonRects[9].top) >> 1));
		g_unusedMissionSetupCarouselPanelColor = FrontendDisplay_PackRGB(0x20, 0x20, 0x40);
		MpRoster_CompactActiveEntries();
		g_missionSetupSlotEditBackupValid = 0;
		g_pilotData.campaignMode = 0;
		g_missionSetupSlotSummaryMode = MISSION_SETUP_SLOT_SUMMARY_CRAFT;
		g_missionSetupSubpanelMode = 0;
		g_missionSetupDraggedPlayerId = 0;
		g_missionSetupBattleSelectedMissionOrdinal = 0;
		g_missionSetupCarouselSlideOffset = 0;
		g_missionSetupCarouselQueuedSlideOffset = 0;
		g_activeTextFieldId = 0;
		g_frontendLeftBarAnimState = 0;
		g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_NONE;
		FrontImage_SetSpriteFrame("rightbar1", 0);
		FrontImage_SetSpriteFrame("rightbar2", 0);
		FrontImage_SetSpriteFrame("rightbar3", 0);
		FrontImage_SetSpriteFrame("rightbar4", 0);
		FrontImage_SetSpriteFrame("rightbar5", 0);
		g_missionSetupCraftSelectionChangedFlag = 0;
		g_frontendMissionOpcode99Count = 0;
		g_missionSetupJoinBroadcastCooldownFrames = 0;
		g_frontendMissionInitClearedDword = 0;

		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			g_pilotData.team = g_pilotData.factionStatistics[2].team;
			g_pilotData.currentFactionId = 2;
			g_missionSetupUseCombatSimPilotState = 1;
			g_pilotData.missionDirectoryId = g_pilotData.factionStatistics[2].missionDirectoryId;
			if (g_pilotData.factionStatistics[2].missionDirectoryId == MISSION_DIRECTORY_TOUR) {
				g_pilotData.missionDirectoryId = MISSION_DIRECTORY_SKIRMISH;
			}
			memcpy(g_pilotData.missionDescriptionIds, g_pilotData.factionStatistics[2].missionDescriptionIds,
				   sizeof(g_pilotData.missionDescriptionIds));
			g_pilotData.unk2 = g_pilotData.factionStatistics[2].m0048;
			g_pilotData.factionStatistics[2].m0048 = 0;
		} else {
			g_pilotData.currentFactionId = 0;
			g_missionSetupUseCombatSimPilotState = 0;
			g_pilotData.team = g_pilotData.factionStatistics[0].team;
			g_pilotData.missionDirectoryId = g_pilotData.factionStatistics[0].missionDirectoryId;
			if (g_pilotData.factionStatistics[0].missionDirectoryId == MISSION_DIRECTORY_MELEE) {
				g_pilotData.missionDirectoryId = MISSION_DIRECTORY_TOUR;
			}
			memcpy(g_pilotData.missionDescriptionIds, g_pilotData.factionStatistics[0].missionDescriptionIds,
				   sizeof(g_pilotData.missionDescriptionIds));
			g_pilotData.unk2 = g_pilotData.factionStatistics[0].m0048;
			g_pilotData.factionStatistics[0].m0048 = 0;
		}

		if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH ||
			g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_CLIENT) {
			g_frontendLeftBarPanelIndex = 2;
			FrontImage_SetSpriteFrame("leftbar2", 0);
		} else {
			g_frontendLeftBarPanelIndex = 5;
			FrontImage_SetSpriteFrame("leftbar5", 0);
		}
		g_frontendRightBarAnimState = 0;
		g_frontendRightBarPanelIndex = 1;
		g_unusedFrontendConcourseHostLatch = 0;
		if (g_briefingText != NULL) {
			Mem_Free(g_briefingText);
			g_briefingText = NULL;
		}
		g_briefingText = (char*)Mem_Alloc(0x1000u);
		g_frontendFirstVisibleLine = 0;
		g_unusedMissionSetupInitDword7830B8 = 0;
		g_frontendQuickStartLaunchFlag = 0;
		g_unusedMissionSetupMissionListIndexLatch = -1;
		g_missionSetupRosterAuthoritative = 0;
		g_missionSetupLastHostBroadcastTick = GetTickCount();
		g_pilotData.unk2 = 0;
		FrontendMission_LoadCurrent();
		MissionSetup_LoadMissionDescText(g_briefingText);
		MissionSetup_CountActiveTeams();
		g_selectedMissionListIndex = 0;
		if (g_missionList != NULL) {
			missionNumber = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
			if (g_missionCount != 0) {
				do {
					if (g_missionList[g_selectedMissionListIndex].missionIdx == missionNumber) {
						break;
					}
					++g_selectedMissionListIndex;
				} while ((unsigned int)g_selectedMissionListIndex < (unsigned int)g_missionCount);
			}
		}

		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
			if (!g_pilotData.tourOfDutyMissions[g_missionList->missionIdx].completedCount) {
				g_missionSetupTourButtonEnabled = 0;
			}
		} else if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
			g_missionSetupActivePanel = MISSION_SETUP_PANEL_BATTLE_SELECT;
			if (!g_pilotData.tourOfDutyMissions[g_missionList[g_selectedMissionListIndex].missionIdx]
					 .completedCount) {
				if (g_selectedMissionListIndex) {
					--g_selectedMissionListIndex;
					g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_TOUR] =
						g_missionList[g_selectedMissionListIndex].missionIdx;
				} else {
					g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
					g_pilotData.missionDirectoryId = MISSION_DIRECTORY_SKIRMISH;
					FrontendMission_LoadCurrent();
					MissionSetup_LoadMissionDescText(g_briefingText);
					MissionSetup_CountActiveTeams();
					g_selectedMissionListIndex = 0;
					if (g_missionList != NULL) {
						missionNumber = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
						if (g_missionCount != 0) {
							do {
								if (g_missionList[g_selectedMissionListIndex].missionIdx == missionNumber) {
									break;
								}
								++g_selectedMissionListIndex;
							} while ((unsigned int)g_selectedMissionListIndex < (unsigned int)g_missionCount);
						}
					}
				}
			}
			if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_BATTLE_SELECT) {
				strcpy(g_missionSetupCurrentBattleSectionName,
					   g_missionList[g_selectedMissionListIndex].sectionName);
				sscanf(g_missionList[g_selectedMissionListIndex].fileName, "%db%dm%d", &battleNumber, &frame,
					   &g_missionSetupParsedMissionNumber);
				selectedIndex = (unsigned int)g_selectedMissionListIndex;
				scanIdx = 0;
				if (g_selectedMissionListIndex > 0) {
					do {
						if (g_pilotData.tourOfDutyMissions[g_missionList[scanIdx].missionIdx]
								.completedCount &&
							strcmp(g_missionList[scanIdx].sectionName,
								   g_missionSetupCurrentBattleSectionName) == 0) {
							g_missionSetupBattleFirstMissionIdx = scanIdx;
							break;
						}
						++scanIdx;
					} while (scanIdx < g_selectedMissionListIndex);
				}
				if (scanIdx >= g_selectedMissionListIndex) {
					g_missionSetupBattleFirstMissionIdx = scanIdx;
				}

				if (scanIdx < g_missionCount) {
					do {
						if (strcmp(g_missionList[scanIdx].sectionName,
								   g_missionSetupCurrentBattleSectionName) != 0 ||
							!g_pilotData.tourOfDutyMissions[g_missionList[scanIdx].missionIdx]
								 .completedCount) {
							g_missionSetupBattleLastMissionIdx = scanIdx - 1;
							break;
						}
						++scanIdx;
					} while (scanIdx < g_missionCount);
				}
				if (scanIdx == g_missionCount) {
					g_missionSetupBattleLastMissionIdx = scanIdx - 1;
				}

				lastMissionIdx = g_missionSetupBattleLastMissionIdx;
				if (selectedIndex < (unsigned int)g_missionSetupBattleFirstMissionIdx ||
					selectedIndex > (unsigned int)lastMissionIdx) {
					g_missionSetupBattleSelectedMissionOrdinal = 0;
				} else {
					g_missionSetupBattleSelectedMissionOrdinal =
						(int)selectedIndex - g_missionSetupBattleFirstMissionIdx;
				}

				g_missionSetupBattleSelectableMissionCount = 0;
				for (scanIdx = g_missionSetupBattleFirstMissionIdx; scanIdx <= lastMissionIdx; ++scanIdx) {
					if (!g_missionList[scanIdx].lockedFlag &&
						g_pilotData.tourOfDutyMissions[g_missionList[scanIdx].missionIdx].completedCount) {
						++g_missionSetupBattleSelectableMissionCount;
					}
				}
			} else {
				g_missionSetupTourButtonEnabled = 0;
				g_pilotData.missionDirectoryId = MISSION_DIRECTORY_SKIRMISH;
				g_frontendLeftBarPanelIndex = 5;
				FrontImage_SetSpriteFrame("leftbar5", 0);
			}
		} else {
			g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
			if (!g_pilotData.tourOfDutyMissions[g_missionList->missionIdx].completedCount) {
				g_missionSetupTourButtonEnabled = 0;
			}
		}

		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
				if (g_combatSimSlots[slotIdx].fgIndex != -1) {
					g_combatSimSlots[slotIdx].fgIndex = (uint16_t)slotIdx;
					g_frontendMission->flightGroups[slotIdx].team =
						(uint8_t)(slotIdx / (16 / g_gameConfig.numberOfTeams));
				}
			}
		}
		srand(GetTickCount());
		g_missionSetupPendingRandomSeed = rand();

		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			playerRoster = Net_GetPlayerRoster(&outCount);
			for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
				for (i = 0; i < outCount; ++i) {
					if (g_combatSimSlots[slotIdx].ownerPlayerId == playerRoster[i].playerId) {
						break;
					}
				}
				if (i >= outCount) {
					g_combatSimSlots[slotIdx].ownerPlayerId = 0;
				}
			}
			for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
				for (i = 0; i < outCount; ++i) {
					if (g_combatSimSlots[slotIdx].gunnerPlayerId == playerRoster[i].playerId) {
						break;
					}
				}
				if (i >= outCount) {
					g_combatSimSlots[slotIdx].gunnerPlayerId = 0;
				}
			}
		}

		if (g_skipFrontendEntryMovie) {
			if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && Net_IsHost()) {
				MissionSetup_BroadcastStatePacket(0);
				MissionSetup_BroadcastSkirmishMetadata();
				g_frontendNetPacketScratch.packetType = 80;
				memset(g_frontendNetPacketScratch.payload, 0, 4u);
				Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 8u);
			}
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
					g_combatSimLoadoutOptions[slotIdx].optionalCraftCategory = 1;

					g_combatSimLoadoutOptions[slotIdx].warheadOptions[0] = 0;
					g_combatSimLoadoutOptions[slotIdx].warheadOptions[1] = 3;
					g_combatSimLoadoutOptions[slotIdx].warheadOptions[2] = 4;
					g_combatSimLoadoutOptions[slotIdx].warheadOptions[3] = 2;
					g_combatSimLoadoutOptions[slotIdx].warheadOptions[4] = 1;
					g_combatSimLoadoutOptions[slotIdx].warheadOptions[5] = 5;
					g_combatSimLoadoutOptions[slotIdx].warheadOptions[6] = 6;
					g_combatSimLoadoutOptions[slotIdx].warheadOptions[7] = 7;
					g_combatSimLoadoutOptions[slotIdx].warheadOptions[8] = 8;
					g_combatSimLoadoutOptions[slotIdx].warheadOptionCount = 9;
					g_combatSimLoadoutOptions[slotIdx].selectedWarheadOption =
						g_combatSimSlots[slotIdx].warhead;

					g_combatSimLoadoutOptions[slotIdx].beamOptions[0] = 0;
					g_combatSimLoadoutOptions[slotIdx].beamOptions[1] = 1;
					g_combatSimLoadoutOptions[slotIdx].beamOptions[2] = 2;
					g_combatSimLoadoutOptions[slotIdx].beamOptions[3] = 3;
					g_combatSimLoadoutOptions[slotIdx].beamOptionCount = 4;
					g_combatSimLoadoutOptions[slotIdx].selectedBeamOption = g_combatSimSlots[slotIdx].beam;

					g_combatSimLoadoutOptions[slotIdx].countermeasureOptions[0] = 0;
					g_combatSimLoadoutOptions[slotIdx].countermeasureOptions[1] = 1;
					g_combatSimLoadoutOptions[slotIdx].countermeasureOptions[2] = 2;
					g_combatSimLoadoutOptions[slotIdx].countermeasureOptionCount = 3;
					g_combatSimLoadoutOptions[slotIdx].selectedCountermeasureOption =
						g_combatSimSlots[slotIdx].countermeasures;
				}
			}
		} else {
			MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
			if (!g_missionSetupSkirmishSeedInitialized) {
				g_missionSetupSkirmishSeedInitialized = 1;
				g_skirmishFileRandomSeed = rand();
			}
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				MissionSetup_LoadSkirmishFile("temp\\temp6753908.skm", 0);
			}
		}

		if (!g_gameConfig.craftSelection) {
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				g_gameConfig.craftSelection = 1;
			}
		}
		if (g_gameConfig.difficulty == 3) {
			g_gameConfig.difficulty = 0;
		}
		for (i = 0; i < 8; ++i) {
			memset(&g_pilotData.networkPlayers[i], 0, sizeof(g_pilotData.networkPlayers[i]));
			g_pilotData.networkPlayers[i].m44 = 0;
			g_pilotData.networkPlayers[i].craftId = -1;
			g_pilotData.networkPlayers[i].warheadType = -1;
			g_pilotData.networkPlayers[i].beamType = -1;
			g_pilotData.networkPlayers[i].counterMeasuresType = -1;
		}
		memset(g_mpRosterReadyFlags, 0, sizeof(g_mpRosterReadyFlags));
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			strcpy(g_mpRoster[0].name, g_pilotData.name);
			g_mpRoster[0].playerId = 1;
			g_mpRoster[0].rating = g_pilotData.pilotRating;
		}
		if (!g_skipFrontendEntryMovie &&
			g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
				if (g_combatSimSlots[slotIdx].craftType &&
					g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIdx].craftType]].flyable &&
					!g_combatSimSlots[slotIdx].ownerPlayerId) {
					g_combatSimSlots[slotIdx].ownerPlayerId = g_mpRoster[0].playerId;
					break;
				}
			}
		}
		g_skipFrontendEntryMovie = 0;
		if (MissionSetup_IsSkirmishSetupValid()) {
			if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
				if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_TOUR) {
					g_frontendRightBarPanelIndex =
						(g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) + 2;
				} else {
					g_frontendRightBarPanelIndex = 1;
				}
			} else {
				g_frontendRightBarPanelIndex = 2;
			}
		} else {
			g_frontendRightBarPanelIndex = 1;
		}
		MissionSetup_DrawBackgroundAndPreview(1);
		FrontendText_ResetGlyphScratchBuffer(20);
		FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
	}

	if (g_missionSetupPendingTransition) {
		MissionSetup_DrawMissionTypeControls();
		if (g_frontendLeftBarAnimState == 3 && g_frontendRightBarAnimState == 3) {
			switch (g_missionSetupPendingTransition) {
				case MISSION_SETUP_TRANSITION_CONCOURSE:
					FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
					return 0;
				case MISSION_SETUP_TRANSITION_JOIN_GAME:
					FrontendScreen_SetCallbacks(FrontendNet_JoinGameScreen,
												FrontendMissionList_FreeScreenResources);
					return 0;
				case MISSION_SETUP_TRANSITION_MISSION_BRIEFING:
					for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
						if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
							if (g_combatSimSlots[slotIdx].ownerPlayerId == 1) {
								g_pilotData.team =
									g_frontendMission
										->flightGroups[(int16_t)g_combatSimSlots[slotIdx].fgIndex]
										.team;
							}
						} else {
							if (g_combatSimSlots[slotIdx].ownerPlayerId == Net_GetLocalPlayerId() ||
								g_combatSimSlots[slotIdx].gunnerPlayerId == Net_GetLocalPlayerId()) {
								g_pilotData.team =
									g_frontendMission
										->flightGroups[(int16_t)g_combatSimSlots[slotIdx].fgIndex]
										.team;
							}
						}
					}
					if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
						Skirmish_GenerateMission("temp.tie");
					}
					FrontendScreen_SetCallbacks(MissionBriefing_Update, MissionBriefing_Exit);
					return 0;
				default:
					break;
			}
		}
		return 0;
	}

	FrontendCursor_GetPos(&i, &j);
	oldActivePanel = g_missionSetupActivePanel;
	g_missionSetupConnectionStatsHoverActive = 0;
	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		if (Net_IsHost()) {
			tickNow = GetTickCount();
			if (tickNow - g_missionSetupLastHostBroadcastTick > 2000u) {
				MissionSetup_BroadcastReadyPlayerRosterPacket(0);
				MissionSetup_BroadcastLobbySelectionPacket();
				g_frontendNetPacketScratch.packetType = 80;
				memset(g_frontendNetPacketScratch.payload, 0, 4u);
				Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 8u);
				g_missionSetupLastHostBroadcastTick = tickNow;
			}
		}

		for (;;) {
			packetType = FrontendNet_ProcessNetworkPackets();
			if (!packetType) {
				break;
			}
			if (packetType == 'A') {
				if (g_frontendNetPacketArg0 || g_frontendNetPacketArg1) {
					if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
						if (FrontendMouse_IsGateOwner(9) || FrontendMouse_IsGateOwner(8) ||
							FrontendMouse_IsGateOwner(10)) {
							FrontendMouse_ClearInputGate();
						}
						g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
						if (g_frontendNetPacketArg0) {
							g_missionSetupBattleSelectedMissionOrdinal = 0;
						}
						if (g_frontendRightBarAnimState != 1) {
							g_frontendRightBarAnimState = 0;
							g_frontendRightBarPanelIndex = 1;
							if (MissionSetup_IsSkirmishSetupValid()) {
								g_frontendRightBarPanelIndex = 2;
							}
							FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
					} else {
						if (FrontendMouse_IsGateOwner(9) || FrontendMouse_IsGateOwner(8) ||
							FrontendMouse_IsGateOwner(10)) {
							FrontendMouse_ClearInputGate();
						}
						g_missionSetupActivePanel = MISSION_SETUP_PANEL_TEAM_ASSIGNMENT;
						g_missionSetupSlotSummaryMode = MISSION_SETUP_SLOT_SUMMARY_CRAFT;
						if (g_frontendNetPacketArg0) {
							g_missionSetupBattleSelectedMissionOrdinal = 0;
						}
						if (g_frontendRightBarAnimState != 1) {
							g_frontendRightBarAnimState = 0;
							g_frontendRightBarPanelIndex = 1;
							if (MissionSetup_IsSkirmishSetupValid()) {
								g_frontendRightBarPanelIndex = 2;
							}
							FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
					}
					if (g_frontendNetPacketArg0 && g_frontendLeftBarAnimState == 1) {
						if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
							g_frontendLeftBarAnimState = 4;
							FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						} else {
							g_frontendLeftBarAnimState = 5;
							FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
													  12 * g_gameConfig.sfxDatapadVolume, 63);
						}
					}
				}
				FrontendMission_LoadCurrent();
				FrontImage_FreeResourceByName("background");
				MissionSetup_DrawBackgroundAndPreview(0);
				MissionSetup_LoadMissionDescText(g_briefingText);
				MissionSetup_CountActiveTeams();
				{
					int missionListIdx = 0;
					g_selectedMissionListIndex = 0;
					if (g_missionList != NULL) {
						missionNumber = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
						if (g_missionCount != 0) {
							do {
								if (g_missionList[missionListIdx].missionIdx == missionNumber) {
									break;
								}
								g_selectedMissionListIndex = ++missionListIdx;
							} while ((unsigned int)missionListIdx < (unsigned int)g_missionCount);
						}
					}
				}
				g_frontendFirstVisibleLine = 0;
				g_unusedMissionSetupMissionListIndexLatch = -1;
				if (g_gameConfig.difficulty == 3) {
					g_gameConfig.difficulty = 0;
				}
				if (!g_gameConfig.craftSelection) {
					if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
						g_gameConfig.craftSelection = 1;
					}
				}
			} else {
				if (packetType == 'F') {
					Net_ShutdownDirectPlaySession();
					if (!Net_IsHost()) {
						FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_HOST_QUIT1),
														 FrontendString_Get(STR_HOST_QUIT2),
														 FrontendString_Get(STR_HOST_QUIT3), NULL, NULL);
					}
					g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NET_CLIENT;
					if (g_gameConfig.networkType) {
						g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_CONCOURSE;
					} else {
						g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_JOIN_GAME;
					}
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				} else if (packetType == 'i') {
					Net_ShutdownDirectPlaySession();
					FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_CONNECTION_ERROR1),
													 FrontendString_Get(STR_CONNECTION_ERROR2),
													 FrontendString_Get(STR_CONNECTION_ERROR3), NULL, NULL);
					g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NET_CLIENT;
					if (g_gameConfig.networkType) {
						g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_CONCOURSE;
					} else {
						g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_JOIN_GAME;
					}
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				} else if (packetType == 'J') {
					FrontendMission_LoadCurrent();
					MissionSetup_CountActiveTeams();
					if (!g_gameConfig.craftSelection &&
						g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
						g_gameConfig.craftSelection = 1;
					}
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					g_missionSetupRosterAuthoritative = 1;
					g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_MISSION_BRIEFING;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
					MissionSetup_DrawMissionTypeControls();
					return 0;
				} else if (packetType == '[') {
					g_skipFrontendEntryMovie = 1;
					g_frontendNetPacketScratch.packetType = 71;
					Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 4u);
					Net_ShutdownDirectPlaySession();
					FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_BOOT1),
													 FrontendString_Get(STR_BOOT2),
													 FrontendString_Get(STR_BOOT3), NULL, NULL);
					if (g_gameConfig.networkType) {
						g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_CONCOURSE;
					} else {
						g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_JOIN_GAME;
					}
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
					MissionSetup_DrawMissionTypeControls();
					return 0;
				} else if (packetType == 'K') {
					if (Net_IsHost() ||
						g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						draggedPlayerId = FrontendMouse_IsGateOwner(2) ? g_missionSetupDraggedPlayerId
																	   : g_frontendNetPacketArg0;
						if (!FrontendMouse_IsGateOwner(2)) {
							g_missionSetupDraggedPlayerId = g_frontendNetPacketArg0;
						}
						for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
							if (g_combatSimSlots[slotIdx].ownerPlayerId == draggedPlayerId) {
								g_combatSimSlots[slotIdx].ownerPlayerId = 0;
							} else if (g_combatSimSlots[slotIdx].gunnerPlayerId == draggedPlayerId) {
								g_combatSimSlots[slotIdx].gunnerPlayerId = 0;
							}
						}
					} else {
						g_missionSetupDraggedPlayerId = g_frontendNetPacketArg0;
					}
				} else if (packetType == 'u') {
					for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
						if (g_mpRoster[rosterIdx].playerId == g_frontendNetPacketSenderPlayerId) {
							g_mpRoster[rosterIdx].rating = g_frontendNetPacketArg0;
							break;
						}
					}
				}
				if (packetType == 'F' || packetType == 'i') {
					break;
				}
			}
		}
	}

	g_missionSetupPlayerDragState = 0;
	if (!frameCounter && g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
		Net_IsHost()) {
		g_frontendNetPacketScratch.packetType = 80;
		memset(g_frontendNetPacketScratch.payload, 0, 4u);
		Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 8u);
	}
	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && Net_IsHost() &&
		!g_missionSetupJoinBroadcastCooldownFrames && MissionSetup_AreReadyPlayersAssignedToSlots()) {
		MissionSetup_BroadcastSkirmishMetadata();
		MissionSetup_BroadcastStatePacket(1);
		g_missionSetupRosterAuthoritative = 1;
	}

	FrontendDraw_RectAssign(&rect, 70, 60, 570, 73);
	FrontendDraw_RectAssign(&rect, 65, 60, 575, 75);
	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		if (!g_combatSimSkirmishFileName[0]) {
			strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_QUICK_SKIRMISH));
		} else {
			strcpy(g_frontendScratchBuffer, g_combatSimSkirmishFileName);
		}
	} else if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_BATTLE_SELECT) {
		strcpy(g_frontendScratchBuffer, g_missionSetupCurrentBattleSectionName);
	} else if (g_selectedMissionListIndex < g_missionCount) {
		strcpy(g_frontendScratchBuffer, g_missionList[g_selectedMissionListIndex].sectionName);
		titlePrefixLen = 0;
		while (g_frontendScratchBuffer[titlePrefixLen] != '\0' &&
			   g_frontendScratchBuffer[titlePrefixLen] != ':') {
			++titlePrefixLen;
		}
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
			g_frontendScratchBuffer[titlePrefixLen] = ',';
			g_frontendScratchBuffer[titlePrefixLen + 1] = ' ';
			g_frontendScratchBuffer[titlePrefixLen + 2] = '\0';
			sscanf(g_missionList[g_selectedMissionListIndex].fileName, "%db%dm%d", &battleNumber, &frame,
				   &g_missionSetupParsedMissionNumber);
			if (frame || g_missionSetupParsedMissionNumber <= 7) {
				sprintf(missionNumberBuffer, "%s %d: ", FrontendString_Get(STR_MISSION_NUMBER),
						g_missionSetupParsedMissionNumber);
			} else {
				sprintf(missionNumberBuffer, "%s %d: ", FrontendString_Get(STR_MISSION_NUMBER),
						g_missionSetupParsedMissionNumber - 7);
			}
			strcat(g_frontendScratchBuffer, missionNumberBuffer);
		} else {
			g_frontendScratchBuffer[titlePrefixLen] = ':';
			g_frontendScratchBuffer[titlePrefixLen + 1] = ' ';
			g_frontendScratchBuffer[titlePrefixLen + 2] = '\0';
		}
		strcat(g_frontendScratchBuffer, g_missionList[g_selectedMissionListIndex].description);
	}
	FrontendText_DrawCentered(12, g_frontendScratchBuffer, &rect, g_colorLightBlue);

	MissionSetup_DrawMissionNavigationPanel(frameCounter);
	MissionSetup_DrawFlightGroupAssignmentPanel(frameCounter);
	switch (g_missionSetupActivePanel) {
		case MISSION_SETUP_PANEL_TEAM_ASSIGNMENT:
			MissionSetup_DrawTeamAssignmentPanel(frameCounter);
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
				MissionSetup_DrawMissionDescriptionPanel();
			}
			break;
		case MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT:
			MissionSetup_DrawSelectedSlotEditPanel(frameCounter);
			break;
		case MISSION_SETUP_PANEL_SAVE_SKIRMISH:
#ifdef XWA_MODERN
			MissionSetup_UpdateSaveSkirmishDialog();
#else
			MissionSetup_UpdateSaveSkirmishDialog(frameCounter);
#endif
			break;
		case MISSION_SETUP_PANEL_SKIRMISH_GOALS:
#ifdef XWA_MODERN
			MissionSetup_DrawSkirmishGoalTypePanel();
#else
			MissionSetup_DrawSkirmishGoalTypePanel(frameCounter);
#endif
			break;
		case MISSION_SETUP_PANEL_MISSION_LIST:
#ifdef XWA_MODERN
			MissionSetup_DrawMissionListPanel();
#else
			MissionSetup_DrawMissionListPanel(frameCounter);
#endif
			break;
		case MISSION_SETUP_PANEL_BATTLE_SELECT:
#ifdef XWA_MODERN
			NetSession_StubReturnTrue();
#else
			NetSession_StubReturnTrue(frameCounter);
#endif
			break;
		default:
			MissionSetup_DrawMissionDescriptionPanel();
			break;
	}
	MissionSetup_DrawGameSettingsPanel();
	MissionSetup_DrawUnassignedPlayersPanel(frameCounter);
	changed = MissionSetup_DrawMissionTypeControls();
	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && Net_IsHost() && changed) {
		MissionSetup_BroadcastStatePacket(0);
	}
	if (g_missionSetupJoinBroadcastCooldownFrames) {
		--g_missionSetupJoinBroadcastCooldownFrames;
	}

	FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[9]);
	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		if (g_frontendRightBarAnimState == 1) {
			if (MissionSetup_IsSkirmishSetupValid()) {
				if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_TOUR) {
					if (g_frontendRightBarPanelIndex != 2) {
						g_frontendRightBarAnimState = 4;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}
				} else if (g_missionSetupActivePanel != MISSION_SETUP_PANEL_BATTLE_SELECT) {
					if (g_frontendRightBarPanelIndex != 4) {
						g_frontendRightBarAnimState = 7;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}
				} else if (g_frontendRightBarPanelIndex != 1) {
					g_frontendRightBarAnimState = 5;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				}
			} else if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
				if (g_missionSetupActivePanel == MISSION_SETUP_PANEL_BATTLE_SELECT) {
					if (g_frontendRightBarPanelIndex != 1) {
						g_frontendRightBarAnimState = 5;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}
				} else if (g_frontendRightBarPanelIndex != 2) {
					g_frontendRightBarAnimState = 4;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				}
			} else if (g_frontendRightBarPanelIndex != 1) {
				g_frontendRightBarAnimState = 5;
				FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
										  63);
			}
		}

		if (g_frontendRightBarPanelIndex != 1 && g_frontendRightBarAnimState == 1) {
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
				changed = FrontendButton_DrawSpriteWithHoverText(
					&rect, (char*)"briefing", (char*)"briefing",
					(void*)FrontendString_Get(STR_GO_TO_BRIEFING), (unsigned int)g_colorPaleBlue,
					(unsigned int)g_colorLightBlue, 7, (char*)"buttonsound");
			} else {
				changed = FrontendButton_DrawSpriteWithHoverText(
					&rect, (char*)"begin", (char*)"begin", (void*)FrontendString_Get(STR_BEGIN),
					(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 7, (char*)"flysound");
			}
			if (changed) {
				g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_MISSION_BRIEFING;
				g_frontendLeftBarAnimState = 2;
				g_frontendRightBarAnimState = 2;
				FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
										  63);
				g_gameConfig.randomSeed = g_missionSetupPendingRandomSeed;
				return 0;
			}
		}
	} else {
		if (g_frontendRightBarAnimState == 1) {
			if (MissionSetup_IsSkirmishSetupValid()) {
				if (g_frontendRightBarPanelIndex == 1) {
					g_frontendRightBarAnimState = 4;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				}
			} else if (g_frontendRightBarPanelIndex == 2) {
				g_frontendRightBarAnimState = 5;
				FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
										  63);
			}
		}
		if (g_frontendRightBarAnimState == 1 && g_frontendRightBarPanelIndex == 2) {
			for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
				if (g_mpRoster[rosterIdx].playerId == Net_GetLocalPlayerId()) {
					break;
				}
			}
			if (rosterIdx < 8) {
				if (g_mpRosterReadyFlags[rosterIdx]) {
					if (FrontendButton_DrawSpriteWithHoverText(
							&rect, (char*)"begin", (char*)"begin", (void*)FrontendString_Get(STR_NOT_READY),
							(unsigned int)g_colorGreen, (unsigned int)g_colorLightBlue, 7,
							(char*)"flysound")) {
						g_frontendNetPacketScratch.packetType = 82;
						Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 4u);
					}
				} else if (FrontendButton_DrawSpriteWithHoverText(
							   &rect, (char*)"begin", (char*)"begin", (void*)FrontendString_Get(STR_READY),
							   (unsigned int)g_colorRed, (unsigned int)g_colorLightBlue, 7,
							   (char*)"flysound")) {
					g_frontendNetPacketScratch.packetType = 81;
					Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 4u);
				}
			}
		}
	}

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
		g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
		FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[8]);
		if (g_frontendRightBarPanelIndex >= 3 && g_frontendRightBarAnimState == 1 &&
			FrontendButton_DrawSpriteWithHoverText(
				&rect, (char*)"quickstart", (char*)"quickstart", (void*)FrontendString_Get(STR_QUICK_START),
				(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 240, (char*)"flysound")) {
			g_frontendQuickStartLaunchFlag = 1;
			g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_MISSION_BRIEFING;
			g_frontendLeftBarAnimState = 2;
			g_frontendRightBarAnimState = 2;
			FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			g_gameConfig.randomSeed = g_missionSetupPendingRandomSeed;
			return 0;
		}
	}

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
			if (g_frontendRightBarPanelIndex >= 3) {
				FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[7]);
			} else {
				FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[9]);
			}
		} else if (g_frontendRightBarPanelIndex == 1) {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[9]);
		} else {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[8]);
		}
		if (g_frontendRightBarAnimState == 1) {
			const char* backText = (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u)
									   ? FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT)
									   : FrontendString_Get(STR_BACK_TO_CONCOURSE);
			if (FrontendButton_DrawSpriteWithHoverText(
					&rect, (char*)"back", (char*)"back", (void*)backText, (unsigned int)g_colorPaleBlue,
					(unsigned int)g_colorLightBlue, 8, (char*)"buttonsound")) {
				g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NONE;
				g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_CONCOURSE;
				g_frontendLeftBarAnimState = 2;
				g_frontendRightBarAnimState = 2;
				FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
										  63);
				return 0;
			}
		}
	} else {
		if (g_frontendRightBarPanelIndex == 1) {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[9]);
		} else {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[8]);
		}
		if (g_frontendRightBarAnimState == 1) {
			const char* backText;
			if (Net_IsHost()) {
				if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
					backText = FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT);
				} else {
					backText = FrontendString_Get(STR_BACK_TO_CONCOURSE);
				}
				if (FrontendButton_DrawSpriteWithHoverText(
						&rect, (char*)"back", (char*)"back", (void*)backText, (unsigned int)g_colorPaleBlue,
						(unsigned int)g_colorLightBlue, 8, (char*)"buttonsound")) {
					changed = FrontendDialog_ShowConfirmDialog(
						FrontendString_Get(STR_HOSTING_GAME1), FrontendString_Get(STR_HOSTING_GAME2),
						FrontendString_Get(STR_HOSTING_GAME3), FrontendString_Get(STR_OKAY),
						FrontendString_Get(STR_CANCEL));
					if (changed) {
						g_skipFrontendEntryMovie = 1;
						g_frontendNetPacketScratch.packetType = 70;
						Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 4u);
						Net_ShutdownDirectPlaySession();
						g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NONE;
						g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_CONCOURSE;
						g_frontendLeftBarAnimState = 2;
						g_frontendRightBarAnimState = 2;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}
				}
			} else {
				if (!g_gameConfig.networkType) {
					backText = FrontendString_Get(STR_RETURN_JOIN_GAME);
				} else if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
					backText = FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT);
				} else {
					backText = FrontendString_Get(STR_BACK_TO_CONCOURSE);
				}
				if (FrontendButton_DrawSpriteWithHoverText(
						&rect, (char*)"back", (char*)"back", (void*)backText, (unsigned int)g_colorPaleBlue,
						(unsigned int)g_colorLightBlue, 8, (char*)"buttonsound") &&
					FrontendDialog_ShowConfirmDialog(
						FrontendString_Get(STR_GAME_IN_PROGRESS1), FrontendString_Get(STR_GAME_IN_PROGRESS2),
						FrontendString_Get(STR_GAME_IN_PROGRESS3), FrontendString_Get(STR_OKAY),
						FrontendString_Get(STR_CANCEL))) {
					g_skipFrontendEntryMovie = 1;
					g_frontendNetPacketScratch.packetType = 71;
					Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 4u);
					Net_ShutdownDirectPlaySession();
					g_missionSetupPendingTransition = MISSION_SETUP_TRANSITION_JOIN_GAME;
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				}
			}
		}
	}

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
		g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR &&
		g_missionSetupActivePanel == MISSION_SETUP_PANEL_DEFAULT) {
		if (g_frontendRightBarPanelIndex == 2) {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[8]);
		} else {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[6]);
		}
		if (g_frontendRightBarAnimState == 1 &&
			FrontendButton_DrawSpriteWithHoverText(
				&rect, (char*)"retbattle", (char*)"retbattle",
				(void*)FrontendString_Get(STR_GAME_RETURN_TO_SELECT_BATTLE), (unsigned int)g_colorPaleBlue,
				(unsigned int)g_colorLightBlue, 9, (char*)"buttonsound")) {
			g_missionSetupActivePanel = MISSION_SETUP_PANEL_BATTLE_SELECT;
			g_frontendRightBarAnimState = 5;
			FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			scanIdx = g_selectedMissionListIndex;
			missionEntry = &g_missionList[scanIdx];
			g_missionSetupBattleSelectedMissionOrdinal = 0;
			g_missionSetupCarouselSlideOffset = 0;
			g_missionSetupCarouselQueuedSlideOffset = 0;
			if (!g_pilotData.tourOfDutyMissions[missionEntry->missionIdx].completedCount) {
				g_selectedMissionListIndex = scanIdx - 1;
				missionEntry = &g_missionList[scanIdx - 1];
				g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] = missionEntry->missionIdx;
			}

			strcpy(g_missionSetupCurrentBattleSectionName, missionEntry->sectionName);
			sscanf(missionEntry->fileName, "%db%d", &battleNumber, &frame);
			scanIdx = 0;
			if (g_selectedMissionListIndex > 0) {
				while (strcmp(g_missionList[scanIdx].sectionName, g_missionSetupCurrentBattleSectionName) !=
					   0) {
					if (++scanIdx >= g_selectedMissionListIndex) {
						break;
					}
				}
				g_missionSetupBattleFirstMissionIdx = scanIdx;
			}
			if (scanIdx >= g_selectedMissionListIndex) {
				g_missionSetupBattleFirstMissionIdx = scanIdx;
			}

			if (scanIdx < g_missionCount) {
				while (strcmp(g_missionList[scanIdx].sectionName, g_missionSetupCurrentBattleSectionName) ==
						   0 &&
					   g_pilotData.tourOfDutyMissions[g_missionList[scanIdx].missionIdx].completedCount) {
					if (++scanIdx >= g_missionCount) {
						break;
					}
				}
				g_missionSetupBattleLastMissionIdx = scanIdx - 1;
			}
			if (scanIdx == g_missionCount) {
				g_missionSetupBattleLastMissionIdx = scanIdx - 1;
			}

			if (g_selectedMissionListIndex < g_missionSetupBattleFirstMissionIdx ||
				g_selectedMissionListIndex > g_missionSetupBattleLastMissionIdx) {
				g_missionSetupBattleSelectedMissionOrdinal = 0;
			} else {
				g_missionSetupBattleSelectedMissionOrdinal =
					g_selectedMissionListIndex - g_missionSetupBattleFirstMissionIdx;
			}

			g_missionSetupBattleSelectableMissionCount = 0;
			for (scanIdx = g_missionSetupBattleFirstMissionIdx; scanIdx <= g_missionSetupBattleLastMissionIdx;
				 ++scanIdx) {
				if (!g_missionList[scanIdx].lockedFlag &&
					g_pilotData.tourOfDutyMissions[g_missionList[scanIdx].missionIdx].completedCount) {
					++g_missionSetupBattleSelectableMissionCount;
				}
			}
			MissionSetup_DrawBackgroundAndPreview(0);
			return 0;
		}
	}

	if (Frontend_HandleEscapeQuit(1) == 1) {
		return 1;
	}

	if ((g_missionSetupActivePanel == MISSION_SETUP_PANEL_DEFAULT ||
		 g_missionSetupActivePanel == MISSION_SETUP_PANEL_TEAM_ASSIGNMENT) &&
		g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_HOST) {
		FrontImage_GetResourceRect("bootu", &rect);
		FrontendDraw_RectOffsetXY(&rect, 480, 334);
		FrontendButton_DrawSpriteAndTooltip(&rect, "bootu", FrontendString_Get(STR_REMOVE_PLAYER_FROM_GAME),
											12, g_colorLightBlue);
		++rect.top;
		FrontendText_DrawCentered(10, FrontendString_Get(STR_GAME_BOOT), &rect, g_colorPaleBlue);
		--rect.top;
	}

	if (g_missionSetupPlayerDragState == 1) {
		if (!g_missionSetupConnectionStatsHoverActive) {
			FrontendCursor_SetLabel(FrontendString_Get(STR_GAME_CLICK_TO_DRAG));
		}
	} else if (!g_missionSetupPlayerDragState) {
		FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor", g_cursorBitmap);
	}

	if (FrontendMouse_IsGateOwner(2)) {
		maxRosterEntries = g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER ? 1 : 8;
		for (rosterIdx = 0; rosterIdx < maxRosterEntries; ++rosterIdx) {
			if (g_mpRoster[rosterIdx].playerId == g_missionSetupDraggedPlayerId) {
				FrontendCursor_GetPos(&i, &j);
				sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
						FrontendString_Get((UIString)(g_mpRoster[rosterIdx].rating + 54)), 1,
						g_mpRoster[rosterIdx].name);
				if (g_mpRoster[rosterIdx].playerId == Net_GetLocalPlayerId() ||
					g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
					FrontendText_Draw(12, g_frontendScratchBuffer, i - 7, j - 7,
									  g_pulseColorRamp[(frameCounter % 24) >> 1]);
				} else {
					FrontendText_Draw(12, g_frontendScratchBuffer, i - 7, j - 7, g_colorYellow);
				}
				break;
			}
		}
		if (FrontendMouse_GetLeftClickFor(2) || FrontendMouse_GetRightClickFor(2)) {
			if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
				g_frontendNetPacketScratch.packetType = 75;
				memset(g_frontendNetPacketScratch.payload, 0, 4u);
				Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 8u);
			}
			if ((g_missionSetupActivePanel == MISSION_SETUP_PANEL_DEFAULT ||
				 g_missionSetupActivePanel == MISSION_SETUP_PANEL_TEAM_ASSIGNMENT) &&
				g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_HOST &&
				g_missionSetupDraggedPlayerId != Net_GetHostPlayerId()) {
				FrontImage_GetResourceRect("bootu", &rect);
				FrontendDraw_RectOffsetXY(&rect, 480, 334);
				FrontendCursor_GetPos(&i, &j);
				if (FrontendDraw_PointInRect(&rect, i, j)) {
					for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
						if (g_mpRoster[rosterIdx].playerId == g_missionSetupDraggedPlayerId) {
							g_frontendNetPacketScratch.packetType = 91;
							Net_SendPacketAndFlush(g_mpRoster[rosterIdx].playerId,
												   &g_frontendNetPacketScratch, 4u);
							Net_ClearPlayerReadyFlagWithLockGuard(g_mpRoster[rosterIdx].playerId);
							MissionSetup_BroadcastStatePacket(0);
							break;
						}
					}
				}
			}
			FrontendMouse_ClearInputGate();
			FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor", g_cursorBitmap);
			g_missionSetupDraggedPlayerId = 0;
		}
	}

	draggedPlayerId = g_missionSetupDraggedPlayerId;
	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		FrontendNet_UpdateAndDrawPanel(frameCounter);
		draggedPlayerId = g_missionSetupDraggedPlayerId;
	}
	if (g_missionSetupActivePanel != oldActivePanel) {
		MissionSetup_DrawBackgroundAndPreview(0);
		draggedPlayerId = g_missionSetupDraggedPlayerId;
	}

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
		for (rosterIdx = 0; rosterIdx < 8; ++rosterIdx) {
			playerId = g_mpRoster[rosterIdx].playerId;
			if (playerId) {
				for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
					if (g_combatSimSlots[slotIdx].ownerPlayerId == playerId ||
						g_combatSimSlots[slotIdx].gunnerPlayerId == playerId) {
						break;
					}
				}
				if (slotIdx >= 16 && playerId != draggedPlayerId) {
					for (slotIdx = 0; slotIdx < 16; ++slotIdx) {
						if (!g_combatSimSlots[slotIdx].ownerPlayerId) {
							g_combatSimSlots[slotIdx].ownerPlayerId = playerId;
							break;
						}
					}
				}
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x543720
int MissionSetup_Exit(int frameCounter) {
	(void)frameCounter;

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		MissionSetup_SaveSkirmishFile("temp\\temp6753908", 0);
	}
	if (g_missionList != NULL) {
		Mem_Free(g_missionList);
		g_missionList = NULL;
	}
	if (g_briefingText != NULL) {
		Mem_Free(g_briefingText);
		g_briefingText = NULL;
	}
	FrontImage_FreeResourceByName("background");
	MissionSetup_UnloadShipBmp();
	if (g_missionSetupUseCombatSimPilotState) {
		g_pilotData.factionStatistics[2].team = g_pilotData.team;
		g_pilotData.factionStatistics[2].missionDirectoryId = g_pilotData.missionDirectoryId;
		memcpy(g_pilotData.factionStatistics[2].missionDescriptionIds, g_pilotData.missionDescriptionIds,
			   sizeof(g_pilotData.factionStatistics[2].missionDescriptionIds));
		g_pilotData.factionStatistics[2].m0048 = g_pilotData.unk2;
	} else {
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].team = g_pilotData.team;
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionDirectoryId =
			g_pilotData.missionDirectoryId;
		memcpy(g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionDescriptionIds,
			   g_pilotData.missionDescriptionIds,
			   sizeof(g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionDescriptionIds));
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].m0048 = g_pilotData.unk2;
	}
	Frontend_ResetScrollableControls();
	FrontendMouse_ClearInputGate();
	MissionSetup_FreeBattleSprites();
	FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor", g_cursorBitmap);
	g_skirmishFileRandomSeed = g_gameConfig.randomSeed;
	if (g_cachedCraftTechStats != NULL) {
		Mem_Free(g_cachedCraftTechStats);
		g_cachedCraftTechStats = NULL;
	}
	if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
		g_missionSetupSkirmishSeedInitialized = 0;
	}
	return 0;
}
