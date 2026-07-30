#include "xwa/frontend/family_transport_room.h"

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/linez.h"
#include "xwa/assets/ship_list.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/briefing_room.h"
#include "xwa/frontend/combat_sim_menu.h"
#include "xwa/frontend/concourse.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_email.h"
#include "xwa/frontend/frontend_escape.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_medals.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/frontend_wave_stream.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/net_session.h"
#include "xwa/util/memory.h"
#include "xwa/util/string.h"
#include "xwa/xwa_options.h"
#include "xwa_runtime/timing/host_clock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FRONTEND_FAMILY_AWARD_TEXT_CAPACITY = 100, FRONTEND_FAMILY_MEDAL_MISSION_COUNT = 100 };

// GLOBAL: XWA 0x78397C
FamilyTransportRoomPendingTransition g_familyTransportRoomPendingTransition;
// GLOBAL: XWA 0x7839D0
int g_familyTransportRoomLabelColor;
// GLOBAL: XWA 0x783B64
int g_frontendFamilyDetailMode;
// GLOBAL: XWA 0x783B08
int g_frontendFamilyEmkayVoiceTimer;
// GLOBAL: XWA 0x78398C
int g_frontendFamilyPageResetPending;
// GLOBAL: XWA 0x783A80
char g_frontendFamilyDetailImagePath[140];
// GLOBAL: XWA 0x7839D8
char g_frontendFamilyDetailTitle[168];
// GLOBAL: XWA 0x783B34
int g_frontendFamilySelectedAwardTextIdx;
// GLOBAL: XWA 0x9F4B14
int g_frontendFamilyAwardTextCount;
// GLOBAL: XWA 0x9F4B20
FrontendFamilyAwardTextEntry* g_frontendFamilyAwardTexts;
// GLOBAL: XWA 0x9F4B4C
int g_frontendLeftBarAnimState;
// GLOBAL: XWA 0x9F60C4
int g_frontendLeftBarPanelIndex;
// GLOBAL: XWA 0x9F4B48
int g_frontendRightBarAnimState;
// GLOBAL: XWA 0x9F4B94
int g_frontendRightBarPanelIndex;
// GLOBAL: XWA 0x783B0C
int g_familyTransportRoomPageOrRevealCounter;
// GLOBAL: XWA 0x783A58
int g_statsCraftScrollOffset;
// GLOBAL: XWA 0x783B74
int g_statsPageRowCount;
// GLOBAL: XWA 0x783990
int g_statsLossesToNpcPilots[3];
// GLOBAL: XWA 0x783B78
int g_statsLossesToPlayerPilots[3];
// GLOBAL: XWA 0x783B18
int g_statsSharedNpcPilotKills[3];
// GLOBAL: XWA 0x783B38
int g_statsNpcPilotKills[3];
// GLOBAL: XWA 0x783B58
int g_statsSharedPlayerPilotKills[3];
// GLOBAL: XWA 0x783980
int g_statsPlayerPilotKills[3];
// GLOBAL: XWA 0x783B28
int g_statsSharedKills[3];
// GLOBAL: XWA 0x783970
int g_statsAssists[3];
// GLOBAL: XWA 0x783A6C
int g_combatRecordScrollOffset;
// GLOBAL: XWA 0x783A70
int g_combatRecordLossesToNpcPilots[3];
// GLOBAL: XWA 0x7839A0
int g_combatRecordLossesToPlayerPilots[3];
// GLOBAL: XWA 0x7839C0
int g_combatRecordSharedNpcPilotKills[3];
// GLOBAL: XWA 0x7839B0
int g_combatRecordNpcPilotKills[3];
// GLOBAL: XWA 0x783A60
int g_combatRecordSharedPlayerPilotKills[3];
// GLOBAL: XWA 0x783B48
int g_combatRecordPlayerPilotKills[3];
// GLOBAL: XWA 0x783958
int g_combatRecordSharedKills[3];
// GLOBAL: XWA 0x783B68
int g_combatRecordAssists[3];
// GLOBAL: XWA 0x783B04
int g_combatRecordHasCraftRows;
// GLOBAL: XWA 0x783964
int g_combatRecordRowVisible;
// GLOBAL: XWA 0x783B00
int g_combatRecordHasPlayerRatingKills;
// GLOBAL: XWA 0x783968
int g_combatRecordHasPlayerRatingLosses;

// GLOBAL: XWA 0x603188
FrontendRect g_frontendSidebarButtonRects[10] = {
	{ 70, 449, 97, 473 },   { 104, 449, 128, 473 }, { 135, 449, 159, 473 }, { 166, 449, 190, 473 },
	{ 198, 449, 224, 473 }, { 410, 449, 437, 473 }, { 443, 449, 470, 473 }, { 476, 449, 503, 473 },
	{ 509, 449, 534, 473 }, { 541, 449, 567, 473 },
};

// GLOBAL: XWA 0x604278
int g_familyMedalZoomOffsetX[20] = { 190, 260, 330, 158, 378, 196, 328 };
// GLOBAL: XWA 0x6042A0
int g_familyMedalZoomOffsetY[20] = { 70, 90, 70, 170, 168, 300, 296 };

// GLOBAL: XWA 0x604458
int g_familyMedalHotspotEnabledByMission[FRONTEND_FAMILY_MEDAL_MISSION_COUNT] = {
	1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0,
	0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// GLOBAL: XWA 0x6042C8
const char* g_familyMedalDetailImagePathByMission[FRONTEND_FAMILY_MEDAL_MISSION_COUNT] = {
	"frontres\\medals\\key.bmp",
	"frontres\\medals\\drinkchit.bmp",
	"frontres\\medals\\flag.bmp",
	"frontres\\medals\\cortselu.bmp",
	"frontres\\medals\\newale.bmp",
	"frontres\\medals\\TieDebri'sAlt.bmp",
	"frontres\\medals\\poster.bmp",
	0,
	0,
	0,
	"frontres\\medals\\swords.bmp",
	0,
	0,
	"frontres\\medals\\probe.bmp",
	0,
	0,
	0,
	0,
	"frontres\\medals\\ship'swheel.bmp",
	0,
	0,
	0,
	0,
	"frontres\\medals\\civilaward.bmp",
	0,
	0,
	"frontres\\medals\\ceillingfan.bmp",
	0,
	0,
	"frontres\\medals\\blacksun.bmp",
	0,
	"frontres\\medals\\aeon.bmp",
	0,
	0,
	0,
	0,
	0,
	"frontres\\medals\\Dunari'sGuest.bmp",
	0,
	0,
	"frontres\\medals\\spynet.bmp",
	0,
	0,
	"frontres\\medals\\Pilot'sJacket.bmp",
	"frontres\\medals\\crest.bmp",
	0,
	0,
	0,
	"frontres\\medals\\spice.bmp",
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

static __inline void FamilyTransportRoom_DrawHoverLabel(FrontendRect* rect, UIString stringId) {
	unsigned int width;

	width = FrontendText_MeasureWidth(FrontendString_Get(stringId), 15);
	rect->left = 300 - (width >> 1);
	rect->top = 400;
	rect->right = width + rect->left + 40;
	rect->bottom = 440;
	FrontendDraw_FillRectTranslucent(rect, 0, 0, (unsigned int)g_familyTransportRoomLabelColor);
	FrontendText_PushGlyphGradientBg(g_familyTransportRoomLabelColor);
	FrontendText_DrawCentered(15, FrontendString_Get(stringId), rect, 0xffff);
	FrontendText_PopGlyphGradientBg();
}

static void FamilyTransportRoom_DrawNormalBackgroundLayer(void) {
	FrontendRect rect;
	char buffer[20];
	unsigned int i;

	FrontImage_DrawSpriteOpaque("background", 0, 0);
	if (g_pilotData.tourOfDutyMissions[8].completedCount) {
		FrontImage_GetResourceRect("cologne", &rect);
		FrontendButton_DrawSpriteAtOriginWithTooltip(&rect, "cologne", FrontendString_Get(STR_FAMILY_COLOGNE),
													 12, g_colorLightBlue);
	}

	if (!g_frontendMissionLoaded || (unsigned int)g_currentMissionId >= 7u) {
		FrontImage_DrawSprite("helmet", 0, 0);
		FrontImage_DrawSprite("jacket", 0, 0);
	}

	for (i = 0; i < (unsigned int)g_missionCount; ++i) {
		int missionIdx;

		missionIdx = g_missionList[i].missionIdx;
		if (g_pilotData.tourOfDutyMissions[missionIdx].completedCount == 1) {
			sprintf(buffer, "medal%d", missionIdx);
			FrontImage_DrawSprite(buffer, 0, 0);
		}
	}

	FrontImage_DrawSprite("medalcase", 0, 0);
	for (i = 0; i < (unsigned int)g_medalCount; ++i) {
		if (g_pilotData.tourOfDutyMissions[g_medalValues[i]].completedCount == 1) {
			sprintf(buffer, "battle%d", i);
			FrontImage_DrawSprite(buffer, 0, 0);
		}
	}

	if (!g_frontendMissionLoaded || (unsigned int)g_currentMissionId >= 7u) {
		sprintf(buffer, "rating%d", (unsigned int)g_pilotData.pilotRating >> 2);
		FrontImage_GetResourceRect(buffer, &rect);
		FrontImage_SetSpriteFrame(buffer, g_pilotData.pilotRating & 3);
		FrontendButton_DrawSpriteAtOriginWithTooltip(
			&rect, buffer, FrontendString_Get((UIString)(g_pilotData.pilotRating + STR_TARGET_DRONE)), 12,
			g_colorLightBlue);
	}

	for (i = 0; i < 6; ++i) {
		if (g_pilotData.kalidorCresent > (int)i) {
			sprintf(buffer, "kalidor%d", i);
			FrontImage_DrawSprite(buffer, 0, 0);
		}
	}

	if (g_pilotData.tourOfDutyMissions[22].completedCount) {
		FrontImage_GetResourceRect("ladyblue", &rect);
		FrontendButton_DrawSpriteAtOriginWithTooltip(
			&rect, "ladyblue", FrontendString_Get(STR_FAMILY_LADY_BLUE), 12, g_colorLightBlue);
	}
}

static void FamilyTransportRoom_DrawMedalCaseZoomLayer(void) {
	char buffer[20];
	unsigned int i;

	FrontImage_DrawSpriteOpaque("background", 0, 0);
	for (i = 0; i < (unsigned int)g_medalCount; ++i) {
		if (g_pilotData.tourOfDutyMissions[g_medalValues[i]].completedCount == 1) {
			sprintf(buffer, "battlezoom%d", i);
			FrontImage_DrawSprite(buffer, g_familyMedalZoomOffsetX[i], g_familyMedalZoomOffsetY[i]);
		}
	}

	for (i = 0; i < 6; ++i) {
		if (g_pilotData.kalidorCresent > (int)i) {
			sprintf(buffer, "kalidorzoom%d", i);
			FrontImage_DrawSprite(buffer, 224, 168);
		}
	}
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x564670
int FrontendFamily_DrawRoomBackground(void) {
	FrontendRect rect;
	unsigned int i;

	FrontImage_FreeResourceByName("background");
	switch (g_frontendFamilyDetailMode) {
		case 0:
			FrontendText_SetGlyphGradientBg(g_colorNearBlack);
			FrontImage_RegisterResourceDefault("frontres\\family\\familyroom.bmp", "background");
			for (i = 0; i < 2; ++i) {
				if (i == 1) {
					FrontendDisplay_LockOffscreenSurface();
				}
				FamilyTransportRoom_DrawNormalBackgroundLayer();
				if (i == 1) {
					FrontendDisplay_UnlockOffscreenSurface(1);
				}
			}
			return 1;

		case 1:
			FrontImage_RegisterResourceDefault("frontres\\family\\monitor.bmp", "background");
			break;

		case 2:
			FrontendCursor_SetPos(g_frontendSidebarButtonRects[9].left + 10,
								  g_frontendSidebarButtonRects[9].top + 10);
			FrontImage_RegisterResourceDefault(g_frontendFamilyDetailImagePath, "background");
			FrontendDisplay_ClearBackBuffer();
			FrontendDisplay_ClearOffscreenSurface();
			FrontImage_DrawSpriteOpaque("background", 0, 0);
			FrontendDraw_RectAssign(&rect, 116, 434, 524, 480);
			FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_colorSlateBlue);
			FrontendDisplay_LockOffscreenSurface();
			FrontImage_DrawSpriteOpaque("background", 0, 0);
			FrontendDraw_RectAssign(&rect, 116, 434, 524, 480);
			FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_colorSlateBlue);
			FrontendDisplay_UnlockOffscreenSurface(1);
			return 1;

		case 3:
			FrontendCursor_SetPos(g_frontendSidebarButtonRects[9].left + 10,
								  g_frontendSidebarButtonRects[9].top + 10);
			FrontImage_RegisterResourceDefault("frontres\\medals\\medback.bmp", "background");
			FrontendDisplay_ClearBackBuffer();
			FrontendDisplay_ClearOffscreenSurface();
			for (i = 0; i < 2; ++i) {
				if (i == 1) {
					FrontendDisplay_LockOffscreenSurface();
				}
				FamilyTransportRoom_DrawMedalCaseZoomLayer();
				if (i == 1) {
					FrontendDisplay_UnlockOffscreenSurface(1);
				}
			}
			return 1;

		case 4:
			FrontendCursor_SetPos(g_frontendSidebarButtonRects[9].left + 10,
								  g_frontendSidebarButtonRects[9].top + 10);
			FrontImage_RegisterResourceDefault(g_frontendFamilyDetailImagePath, "background");
			FrontendDisplay_ClearBackBuffer();
			FrontendDisplay_ClearOffscreenSurface();
			break;

		default:
			return 1;
	}

	FrontImage_DrawSpriteOpaque("background", 0, 0);
	FrontendDisplay_LockOffscreenSurface();
	FrontImage_DrawSpriteOpaque("background", 0, 0);
	FrontendDisplay_UnlockOffscreenSurface(1);
	return 1;
}

// FUNCTION: XWA 0x563630
int FrontendFamily_LoadAwardTextList(void) {
	XwaFile* stream;
	char buffer[1024];
	char* line;
	char* nextLine;
	int index;
	int awardId;

	if (g_frontendFamilyAwardTexts != NULL) {
		Mem_Free(g_frontendFamilyAwardTexts);
		g_frontendFamilyAwardTexts = NULL;
	}

	g_frontendFamilyAwardTextCount = 0;
	strcpy(g_frontendScratchBuffer, "frontres\\medals\\familyawards.txt");
	stream = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "r");
	if (stream != NULL) {
		if (g_frontendFamilyAwardTexts != NULL) {
			Mem_Free(g_frontendFamilyAwardTexts);
			g_frontendFamilyAwardTexts = NULL;
		}

		g_frontendFamilyAwardTexts = (FrontendFamilyAwardTextEntry*)Mem_Alloc(
			sizeof(FrontendFamilyAwardTextEntry) * FRONTEND_FAMILY_AWARD_TEXT_CAPACITY);
		if (g_frontendFamilyAwardTexts != NULL) {
			memset(g_frontendFamilyAwardTexts, 0,
				   sizeof(FrontendFamilyAwardTextEntry) * g_frontendFamilyAwardTextCount);
			index = 0;
			while (1) {
#ifdef XWA_MODERN
				line = File_ReadLine(stream, buffer, sizeof(buffer)) ? buffer : NULL;
#else
				line = fgets(buffer, sizeof(buffer), (FILE*)stream);
#endif
				if (line != NULL) {
					if (line[0] == '/' && line[1] == '/') {
						continue;
					}
					awardId = atoi(line);
				}

				if (line != NULL) {
					g_frontendFamilyAwardTexts[index].awardId = awardId;
					g_frontendFamilyAwardTexts[index].text[0] = '\0';
					while (1) {
#ifdef XWA_MODERN
						nextLine = File_ReadLine(stream, buffer, sizeof(buffer)) ? buffer : NULL;
#else
						nextLine = fgets(buffer, sizeof(buffer), (FILE*)stream);
#endif
						if (nextLine == NULL || nextLine[0] == '*') {
							break;
						}
						if (nextLine[0] == '/' && nextLine[1] == '/') {
							continue;
						}

#ifdef XWA_MODERN
						nextLine = File_ReadLine(stream, buffer, sizeof(buffer)) ? buffer : NULL;
						if (nextLine == NULL) {
							break;
						}
#else
						nextLine = fgets(buffer, sizeof(buffer), (FILE*)stream);
#endif
						if (nextLine[strlen(nextLine) - 1] == '\n') {
							nextLine[strlen(nextLine) - 1] = '\0';
						}

#ifdef XWA_MODERN
						{
							size_t textLength;

							textLength = strlen(g_frontendFamilyAwardTexts[index].text);
							strncat(g_frontendFamilyAwardTexts[index].text, Linez_ResolveString(nextLine),
									sizeof(g_frontendFamilyAwardTexts[index].text) - textLength - 1);
						}
#else
						strcat(g_frontendFamilyAwardTexts[index].text, Linez_ResolveString(nextLine));
#endif
					}

					++index;
#ifdef XWA_MODERN
					if (index >= FRONTEND_FAMILY_AWARD_TEXT_CAPACITY) {
						break;
					}
#endif
					if (nextLine == NULL) {
						break;
					}
				} else {
					break;
				}
			}

			g_frontendFamilyAwardTextCount = index;
		}
		File_Close(stream);
	}
	return 0;
}

// FUNCTION: XWA 0x563A20
int FrontendFamily_HandleTrophyHotspots(int unused) {
	int hotspotActive;
	(void)unused;

	if (!g_missionList) {
		return 0;
	}

	{
		int outX;
		int outY;
		unsigned int missionListIndex;
		FrontendRect rect;
		char buffer[20];
		int buttonResult;

		hotspotActive = 0;
		FrontendCursor_GetPos(&outX, &outY);
		if (!g_frontendFamilyDetailMode) {
			missionListIndex = 0;
			if ((unsigned int)g_missionCount > 0) {
				do {
					if (g_pilotData.tourOfDutyMissions[g_missionList[missionListIndex].missionIdx]
							.completedCount == 1) {
						sprintf(buffer, "medal%d", g_missionList[missionListIndex].missionIdx);
						if (g_familyMedalHotspotEnabledByMission[g_missionList[missionListIndex]
																	 .missionIdx]) {
							FrontImage_GetResourceRect(buffer, &rect);
							if (FrontendDraw_PointInRect(&rect, outX, outY)) {
								hotspotActive = 1;
							}

							buttonResult = FrontendButton_DrawSimpleSpriteHitTest(
								&rect, g_emptyString, g_emptyString,
								FrontendString_Get((UIString)(g_missionList[missionListIndex].missionIdx +
															  STR_FAMILY_MEDAL0)),
								12, g_colorLightBlue, (int)missionListIndex + 35, "settingsound");
							if (buttonResult != 0) {
								if (!g_frontendFamilyDetailMode) {
									unsigned int awardTextIndex;

									g_frontendRightBarAnimState = 0;
									g_frontendFamilyDetailMode = 2;
									g_familyTransportRoomPageOrRevealCounter = 0;
									strcpy(
										g_frontendFamilyDetailImagePath,
										g_familyMedalDetailImagePathByMission[g_missionList[missionListIndex]
																				  .missionIdx]);
									FrontendFamily_DrawRoomBackground();
									FrontendSound_PlayUISound("panelarm", 1, 0, 255,
															  12 * g_gameConfig.sfxDatapadVolume, 63);
									strcpy(g_frontendFamilyDetailTitle,
										   FrontendString_Get(
											   (UIString)(g_missionList[missionListIndex].missionIdx +
														  STR_FAMILY_MEDAL0)));
									g_frontendFamilySelectedAwardTextIdx = 0;
									for (awardTextIndex = 0;
										 awardTextIndex < (unsigned int)g_frontendFamilyAwardTextCount;
										 ++awardTextIndex) {
										if (g_frontendFamilyAwardTexts[awardTextIndex].awardId ==
											g_missionList[missionListIndex].missionIdx + 1) {
											g_frontendFamilySelectedAwardTextIdx = (int)awardTextIndex;
											break;
										}
									}
								}
							}
						}
					}

					++missionListIndex;
				} while (missionListIndex < (unsigned int)g_missionCount);
			}

			FrontImage_GetResourceRect("medalcase", &rect);
			if (FrontendDraw_PointInRect(&rect, outX, outY)) {
				hotspotActive = 1;
			}
			buttonResult = FrontendButton_DrawSimpleSpriteHitTest(&rect, g_emptyString, g_emptyString,
																  FrontendString_Get(STR_FAMILY_MEDAL_CASE),
																  12, g_colorLightBlue, 28, "settingsound");
			if (buttonResult != 0) {
				g_frontendRightBarAnimState = 0;
				g_frontendFamilyDetailMode = 3;
				g_familyTransportRoomPageOrRevealCounter = 0;
				g_frontendFamilyDetailTitle[0] = '\0';
				FrontendFamily_DrawRoomBackground();
				FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}

			if (!g_frontendMissionLoaded || (unsigned int)g_currentMissionId >= 7u) {
				FrontImage_GetResourceRect("jacket", &rect);
				if (FrontendDraw_PointInRect(&rect, outX, outY)) {
					hotspotActive = 1;
				}
				buttonResult = FrontendButton_DrawSimpleSpriteHitTest(
					&rect, g_emptyString, g_emptyString,
					FrontendString_Get((UIString)(g_pilotData.pilotRank + STR_FAMILY_PLAYER_RANK + 1)), 12,
					g_colorLightBlue, 29, "settingsound");
				if (buttonResult != 0) {
					g_frontendRightBarAnimState = 0;
					g_frontendFamilyDetailMode = 4;
					g_familyTransportRoomPageOrRevealCounter = 0;
					sprintf(g_frontendFamilyDetailImagePath, "frontres\\medals\\rankzoom%d.bmp",
							g_pilotData.pilotRank);
					strcpy(
						g_frontendFamilyDetailTitle,
						FrontendString_Get((UIString)(g_pilotData.pilotRank + STR_FAMILY_PLAYER_RANK + 1)));
					FrontendFamily_DrawRoomBackground();
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				}
			}

			if (!g_frontendMissionLoaded || (unsigned int)g_currentMissionId >= 7u) {
				sprintf(buffer, "rating%d", (unsigned int)g_pilotData.pilotRating >> 2);
				FrontImage_GetResourceRect(buffer, &rect);
				FrontImage_SetSpriteFrame(buffer, g_pilotData.pilotRating & 3);
				FrontendButton_DrawSpriteAtOriginWithTooltip(
					&rect, g_emptyString,
					FrontendString_Get((UIString)(g_pilotData.pilotRating + STR_TARGET_DRONE)), 12,
					g_colorLightBlue);
			}

			if (g_pilotData.tourOfDutyMissions[22].completedCount) {
				FrontImage_GetResourceRect("ladyblue", &rect);
				if (FrontendDraw_PointInRect(&rect, outX, outY)) {
					hotspotActive = 1;
				}
				buttonResult = FrontendButton_DrawSimpleSpriteHitTest(
					&rect, g_emptyString, g_emptyString, FrontendString_Get(STR_FAMILY_LADY_BLUE), 12,
					g_colorLightBlue, 31, "settingsound");
				if (buttonResult != 0) {
					unsigned int awardTextIndex;

					g_frontendRightBarAnimState = 0;
					g_frontendFamilyDetailMode = 2;
					g_familyTransportRoomPageOrRevealCounter = 0;
					strcpy(g_frontendFamilyDetailImagePath, "frontres\\medals\\ladyblue.bmp");
					FrontendFamily_DrawRoomBackground();
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
					strcpy(g_frontendFamilyDetailTitle, FrontendString_Get(STR_FAMILY_LADY_BLUE));
					g_frontendFamilySelectedAwardTextIdx = 0;
					for (awardTextIndex = 0; awardTextIndex < (unsigned int)g_frontendFamilyAwardTextCount;
						 ++awardTextIndex) {
						if (g_frontendFamilyAwardTexts[awardTextIndex].awardId == 23) {
							g_frontendFamilySelectedAwardTextIdx = (int)awardTextIndex;
							break;
						}
					}
				}
			}

			if (g_pilotData.tourOfDutyMissions[8].completedCount) {
				FrontImage_GetResourceRect("cologne", &rect);
				if (FrontendDraw_PointInRect(&rect, outX, outY)) {
					hotspotActive = 1;
				}
				buttonResult = FrontendButton_DrawSimpleSpriteHitTest(
					&rect, g_emptyString, g_emptyString, FrontendString_Get(STR_FAMILY_COLOGNE), 12,
					g_colorLightBlue, 32, "settingsound");
				if (buttonResult != 0) {
					unsigned int awardTextIndex;

					g_frontendRightBarAnimState = 0;
					g_frontendFamilyDetailMode = 2;
					g_familyTransportRoomPageOrRevealCounter = 0;
					strcpy(g_frontendFamilyDetailImagePath, "frontres\\medals\\cologne.bmp");
					FrontendFamily_DrawRoomBackground();
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
					strcpy(g_frontendFamilyDetailTitle, FrontendString_Get(STR_FAMILY_COLOGNE));
					g_frontendFamilySelectedAwardTextIdx = 0;
					for (awardTextIndex = 0; awardTextIndex < (unsigned int)g_frontendFamilyAwardTextCount;
						 ++awardTextIndex) {
						if (g_frontendFamilyAwardTexts[awardTextIndex].awardId == 9) {
							g_frontendFamilySelectedAwardTextIdx = (int)awardTextIndex;
							break;
						}
					}
				}
			}
		} else if (g_frontendFamilyDetailMode == 3) {
			unsigned int medalIndex;

			medalIndex = 0;
			if ((unsigned int)g_medalCount > 0) {
				do {
					if (g_pilotData.tourOfDutyMissions[g_medalValues[medalIndex]].completedCount == 1) {
						sprintf(buffer, "battlezoom%d", medalIndex);
						FrontImage_GetResourceRect(buffer, &rect);
						FrontendDraw_RectOffsetXY(&rect, g_familyMedalZoomOffsetX[medalIndex],
												  g_familyMedalZoomOffsetY[medalIndex]);
						FrontendButton_DrawSpriteAtOriginWithTooltip(
							&rect, g_emptyString,
							FrontendString_Get((UIString)(medalIndex + STR_FAMILY_BATTLE_MEDAL1)), 12,
							g_colorLightBlue);
					}

					++medalIndex;
				} while (medalIndex < (unsigned int)g_medalCount);
			}

			for (medalIndex = 0; medalIndex < 6; ++medalIndex) {
				if ((unsigned int)g_pilotData.kalidorCresent > medalIndex) {
					sprintf(buffer, "kalidorzoom%d", medalIndex);
					FrontImage_GetResourceRect(buffer, &rect);
					FrontendDraw_RectOffsetXY(&rect, 224, 168);
					FrontendButton_DrawSpriteAtOriginWithTooltip(
						&rect, g_emptyString,
						FrontendString_Get((UIString)(g_pilotData.kalidorCresent + STR_FAMILY_KALIDOR1 - 1)),
						12, g_colorLightBlue);
				}
			}
		}

		if (hotspotActive) {
			FrontendCursor_SetImageResourceForCurrentTheme("cursor3", g_cursorBitmap);
		} else {
			FrontendCursor_SetImageResourceForCurrentTheme("cursor", g_cursorBitmap);
		}

		return hotspotActive;
	}
}

// FUNCTION: XWA 0x564300
int FrontendFamily_PlayEmkayVoiceLine(int allowMissionOrIdleLine) {
	srand(XwaTime_GetElapsedTicks());
	FrontImage_SetSpriteFrame("mkeye", 0);

	if (!allowMissionOrIdleLine) {
		if (g_pilotData.emkayAnnounceNewRank) {
			strcpy(g_frontendScratchBuffer, "wave\\frontend\\N01MC08.wav");
			if (FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0)) {
				return 1;
			}
			g_frontendScratchBuffer[0] = '\0';
		}

		if (g_pilotData.emkayAnnounceNewAward == 1) {
			sprintf(g_frontendScratchBuffer, "wave\\frontend\\N01MC0%d.wav", rand() % 3 + 4);
			if (!FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0)) {
				g_frontendScratchBuffer[0] = '\0';
				return 0;
			}

			return 1;
		}

		if (g_pilotData.emkayAnnounceNewAward != 2) {
			return 0;
		}

		strcpy(g_frontendScratchBuffer, "wave\\frontend\\N01MC07.wav");
		if (!FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0)) {
			g_frontendScratchBuffer[0] = '\0';
			return 0;
		}

		return 1;
	}

	if (g_frontendMissionLoaded) {
		if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
			int missionListIndex;
			int battleNumber;
			int missionNumber;
			int unusedMissionPrefix;
			char separator;
			char fileNumber[7];

			for (missionListIndex = 0; missionListIndex < g_missionCount; ++missionListIndex) {
				if (g_missionList[missionListIndex].missionIdx == g_currentMissionId) {
					g_selectedMissionListIndex = missionListIndex;
					break;
				}
			}

			sscanf(g_missionList[missionListIndex].fileName, "%d%c%d%c%d", &unusedMissionPrefix, &separator,
				   &battleNumber, &separator, &missionNumber);
			if ((unsigned int)battleNumber >= 100u || (unsigned int)missionNumber >= 100u) {
				g_frontendScratchBuffer[0] = '\0';
			} else {
				sprintf(fileNumber, "%2d%2d%2d", battleNumber, missionNumber, 1);
				if (fileNumber[0] == ' ') {
					fileNumber[0] = '0';
				}
				if (fileNumber[2] == ' ') {
					fileNumber[2] = '0';
				}
				if (fileNumber[4] == ' ') {
					fileNumber[4] = '0';
				}
				sprintf(g_frontendScratchBuffer, "wave\\frontend\\B%dM%d\\N%s.wav", battleNumber,
						missionNumber, fileNumber);
			}

			if (g_frontendScratchBuffer[0]) {
				if (!FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0)) {
					if (g_frontendMission->header.briefingLogo == 7) {
						strcpy(g_frontendScratchBuffer, "wave\\frontend\\N01MC01.wav");
					} else if (g_frontendMission->header.briefingLogo == 10) {
						strcpy(g_frontendScratchBuffer, "wave\\frontend\\N01MC02.wav");
					} else if (g_frontendMission->header.briefingLogo == 9) {
						strcpy(g_frontendScratchBuffer, "wave\\frontend\\N01MC03.wav");
					} else {
						g_frontendScratchBuffer[0] = '\0';
					}

					if (g_frontendScratchBuffer[0] &&
						!FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0)) {
						g_frontendScratchBuffer[0] = '\0';
					}
				}
			}
		} else {
			g_frontendScratchBuffer[0] = '\0';
		}
	}

	if (g_frontendScratchBuffer[0]) {
		return 1;
	}

	if (!g_frontendMissionLoaded) {
		return 1;
	}

	if ((unsigned int)g_currentMissionId < 7u && g_frontendFamilyHasNewEmail) {
		sprintf(g_frontendScratchBuffer, "wave\\frontend\\T01MC0%d.wav", rand() % 4 + 2);
		return FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
	}

	if (!g_frontendMissionLoaded) {
		return 1;
	}

	switch (rand() % 6) {
		case 0:
			strcpy(g_frontendScratchBuffer, "wave\\frontend\\N01MC09.wav");
			break;
		case 1:
			strcpy(g_frontendScratchBuffer, "wave\\frontend\\N01MC10.wav");
			break;
		case 2:
			strcpy(g_frontendScratchBuffer, "wave\\frontend\\N01MC11.wav");
			break;
		default:
			g_frontendScratchBuffer[0] = '\0';
			break;
	}

	if (g_frontendScratchBuffer[0]) {
		FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
	}

	return 1;
}

// FUNCTION: XWA 0x564BC0
int FrontendFamily_DrawEmailMonitor(int frameCounter) {
	if (g_frontendFamilyHasNewEmail) {
		FrontImage_SetSpriteFrame("emailmon", frameCounter % 20 / 4);
		FrontImage_DrawSprite("emailmon", 426, 243);
	} else {
		FrontImage_SetSpriteFrame("lightmon", frameCounter % 24 / 4);
		FrontImage_DrawSprite("lightmon", 426, 243);
	}

	return 1;
}

// FUNCTION: XWA 0x560E60
int FamilyTransportRoom_DrawPilotStatsPage(int statCategory) {
	FrontendRect out;
	int factionIndex;
	int visibleRows;
	int y;
	int yRating;
	int yStats;
	int yLossHeader;
	int yLossDetails;
	int yCraftList;
	int yCraftValue;
	int shipIndex;
	int rowIndex;

	if (statCategory == 2) {
		return FamilyTransportRoom_DrawCombatSimulatorRecordPage(2);
	}

	factionIndex = statCategory != 0 ? 3 : 0;

	if (g_frontendFamilyPageResetPending) {
		g_statsCraftScrollOffset = 0;
		g_statsPageRowCount = 0;

		for (shipIndex = 0; shipIndex < g_shipCount; ++shipIndex) {
			int typeId;

			typeId = g_shipList[shipIndex].typeId;
			if (g_pilotData.factionStatistics[factionIndex].stats.killsPerCraftPerMT[statCategory][typeId] ||
				g_pilotData.factionStatistics[factionIndex]
					.stats.killsSharedPerCraftPerMT[statCategory][typeId]) {
				++g_statsPageRowCount;
			}
		}

		memset(g_statsLossesToNpcPilots, 0, sizeof(g_statsLossesToNpcPilots));
		memset(g_statsLossesToPlayerPilots, 0, sizeof(g_statsLossesToPlayerPilots));
		memset(g_statsSharedNpcPilotKills, 0, sizeof(g_statsSharedNpcPilotKills));
		memset(g_statsNpcPilotKills, 0, sizeof(g_statsNpcPilotKills));
		memset(g_statsSharedPlayerPilotKills, 0, sizeof(g_statsSharedPlayerPilotKills));
		memset(g_statsPlayerPilotKills, 0, sizeof(g_statsPlayerPilotKills));
		memset(g_statsSharedKills, 0, sizeof(g_statsSharedKills));
		memset(g_statsAssists, 0, sizeof(g_statsAssists));

		for (shipIndex = 0; shipIndex < g_shipCount; ++shipIndex) {
			int typeId;

			typeId = g_shipList[shipIndex].typeId;
			g_statsAssists[statCategory] += g_pilotData.factionStatistics[factionIndex]
												.stats.killsAssistsPerCraftPerMT[statCategory][typeId];
			g_statsSharedKills[statCategory] += g_pilotData.factionStatistics[factionIndex]
													.stats.killsSharedPerCraftPerMT[statCategory][typeId];
		}

		for (rowIndex = 0; rowIndex < 25; ++rowIndex) {
			g_statsPlayerPilotKills[statCategory] +=
				g_pilotData.factionStatistics[factionIndex]
					.stats.killsFullOnPlayerRatingPerMT[statCategory][rowIndex];
			g_statsSharedPlayerPilotKills[statCategory] +=
				g_pilotData.factionStatistics[factionIndex]
					.stats.killsSharedOnPlayerRatingPerMT[statCategory][rowIndex];
		}

		for (rowIndex = 0; rowIndex < 6; ++rowIndex) {
			g_statsNpcPilotKills[statCategory] += g_pilotData.factionStatistics[factionIndex]
													  .stats.killsFullOnAIRatingPerMT[statCategory][rowIndex];
			g_statsSharedNpcPilotKills[statCategory] +=
				g_pilotData.factionStatistics[factionIndex]
					.stats.killsSharedOnAIRatingPerMT[statCategory][rowIndex];
		}

		for (rowIndex = 0; rowIndex < 25; ++rowIndex) {
			g_statsLossesToPlayerPilots[statCategory] +=
				g_pilotData.factionStatistics[factionIndex]
					.stats.killedByPlayerRatingPerMT[statCategory][rowIndex];
		}

		for (rowIndex = 0; rowIndex < 6; ++rowIndex) {
			g_statsLossesToNpcPilots[statCategory] +=
				g_pilotData.factionStatistics[factionIndex]
					.stats.killedByAIRatingPerMT[statCategory][rowIndex];
		}

		g_frontendFamilyPageResetPending = 0;
	}

	if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
		sprintf(g_frontendScratchBuffer, "%s", g_pilotData.name);
	} else {
		sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
				FrontendString_Get((UIString)(g_pilotData.pilotRank + STR_FAMILY_PLAYER_RANK + 1)), 1,
				g_pilotData.name);
	}
	FrontendText_Draw(12, g_frontendScratchBuffer, 65, 66, g_colorYellow);

	y = 81;
	visibleRows = 13;
	if (g_frontendMissionLoaded && g_frontendMission->header.hangar != XWA_HANGAR_FAMILYTRANSPORT) {
		sprintf(g_frontendScratchBuffer, "%c%s: %c%s", 2, FrontendString_Get(STR_FAMILY_ASSIGNED_TO), 1,
				FrontendString_Get((UIString)(g_frontendMission->header.briefingLogo + 727)));
		FrontendText_Draw(12, g_frontendScratchBuffer, 65, 81, 0xffff);
		y = 96;
	} else if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
		visibleRows = 14;
	} else {
		sprintf(g_frontendScratchBuffer, "%s", FrontendString_Get(STR_FAMILY_ON_LEAVE));
		FrontendText_Draw(12, g_frontendScratchBuffer, 65, 81, g_colorLightBlue);
		y = 96;
	}

	sprintf(g_frontendScratchBuffer, "%c%s: %c%d", 2, FrontendString_Get(STR_TOTAL_SCORE), 1,
			g_pilotData.factionStatistics[factionIndex].score);
	FrontendText_Draw(12, g_frontendScratchBuffer, 65, y, 0xffff);
	sprintf(g_frontendScratchBuffer, "%c%s: %c%d", 2, FrontendString_Get(STR_DEBRIEF_BONUS_SCORE), 1,
			(unsigned int)g_pilotData.factionStatistics[factionIndex].bonusScore / 10u);
	FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);

	yRating = y + 15;
	sprintf(g_frontendScratchBuffer, "%c%s %c%s", 2, FrontendString_Get(STR_PILOT_RATING), 6,
			FrontendString_Get((UIString)(g_pilotData.pilotRating + STR_TARGET_DRONE)));
	FrontendText_Draw(12, g_frontendScratchBuffer, 65, yRating, 0xffff);
	if ((unsigned int)g_pilotData.pilotRating < 24u) {
		sprintf(g_frontendScratchBuffer, "%c%s %c%d%%", 2, FrontendString_Get(STR_PROMOTION_POINTS), 1,
				g_pilotData.nextPromotionPercent);
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, yRating, 0xffff);
	}

	yStats = yRating + 30;
	if (g_pilotData.factionStatistics[factionIndex].stats.totalKillsPerMT[statCategory] ||
		g_pilotData.factionStatistics[factionIndex].stats.killsSharedPerCraftPerMT[statCategory]) {
		sprintf(g_frontendScratchBuffer, "%c%s %c%d (%d)", 2, FrontendString_Get(STR_TOTAL_KILLS), 1,
				g_pilotData.factionStatistics[factionIndex].stats.totalKillsPerMT[statCategory],
				g_statsSharedKills[statCategory]);
	} else {
		sprintf(g_frontendScratchBuffer, "%c%s %c----", 2, FrontendString_Get(STR_TOTAL_KILLS), 1);
	}
	FrontendText_Draw(12, g_frontendScratchBuffer, 65, yStats, 0xffff);
	if (!g_statsAssists[statCategory]) {
		sprintf(g_frontendScratchBuffer, "%c%s %c----", 2, FrontendString_Get(STR_ASSISTS), 1);
	} else {
		sprintf(g_frontendScratchBuffer, "%c%s %c%d", 2, FrontendString_Get(STR_ASSISTS), 1,
				g_statsAssists[statCategory]);
	}
	FrontendText_Draw(12, g_frontendScratchBuffer, 350, yStats, 0xffff);

	yLossHeader = yStats + 30;
	if (!g_pilotData.factionStatistics[factionIndex].stats.totalCraftLossesPerMT[statCategory]) {
		sprintf(g_frontendScratchBuffer, "%c%s %c----", 2, FrontendString_Get(STR_TOTAL_CRAFT_LOSSES), 1);
	} else {
		sprintf(g_frontendScratchBuffer, "%c%s %c%d", 2, FrontendString_Get(STR_TOTAL_CRAFT_LOSSES), 1,
				g_pilotData.factionStatistics[factionIndex].stats.totalCraftLossesPerMT[statCategory]);
	}
	FrontendText_Draw(12, g_frontendScratchBuffer, 65, yLossHeader, 0xffff);

	yLossDetails = yLossHeader + 15;
	if (!g_pilotData.factionStatistics[factionIndex].stats.lossesByStarshipsPerMT[statCategory]) {
		sprintf(g_frontendScratchBuffer, "%c%s %c----", 2, FrontendString_Get(STR_TO_STARSHIPS), 1);
	} else {
		sprintf(g_frontendScratchBuffer, "%c%s %c%d", 2, FrontendString_Get(STR_TO_STARSHIPS), 1,
				g_pilotData.factionStatistics[factionIndex].stats.lossesByStarshipsPerMT[statCategory]);
	}
	FrontendText_Draw(12, g_frontendScratchBuffer, 65, yLossDetails, 0xffff);
	if (!g_pilotData.factionStatistics[factionIndex].stats.lossesByMinesPerMT[statCategory]) {
		sprintf(g_frontendScratchBuffer, "%c%s %c----", 2, FrontendString_Get(STR_TO_MINES), 1);
	} else {
		sprintf(g_frontendScratchBuffer, "%c%s %c%d", 2, FrontendString_Get(STR_TO_MINES), 1,
				g_pilotData.factionStatistics[factionIndex].stats.lossesByMinesPerMT[statCategory]);
	}
	FrontendText_Draw(12, g_frontendScratchBuffer, 225, yLossDetails, 0xffff);
	if (!g_pilotData.factionStatistics[factionIndex].stats.lossesByCollisionsPerMT[statCategory]) {
		sprintf(g_frontendScratchBuffer, "%c%s %c----", 2, FrontendString_Get(STR_FROM_COLLISIONS), 1);
	} else {
		sprintf(g_frontendScratchBuffer, "%c%s %c%d", 2, FrontendString_Get(STR_FROM_COLLISIONS), 1,
				g_pilotData.factionStatistics[factionIndex].stats.lossesByCollisionsPerMT[statCategory]);
	}
	FrontendText_Draw(12, g_frontendScratchBuffer, 365, yLossDetails, 0xffff);

	yCraftList = yLossDetails + 30;
	if (g_statsPageRowCount) {
		FrontendText_Draw(12, FrontendString_Get(STR_CRAFT_KILLS_BY_TYPE), 65, yCraftList, g_colorLightBlue);
		yCraftList += 15;
	}

	FrontendDraw_RectAssign(&out, 556, yCraftList, 575, yCraftList + 15 * visibleRows);
	if (g_statsPageRowCount > visibleRows) {
		g_statsCraftScrollOffset =
			FrontendScrollbar_Draw(&out, g_statsCraftScrollOffset, g_statsPageRowCount, 0, 5, g_colorNavy, 3);
	}

	rowIndex = 0;
	yCraftValue = yCraftList + 1;
	for (shipIndex = 0; shipIndex < g_shipCount; ++shipIndex) {
		int typeId;
		int hasKills;

		typeId = g_shipList[shipIndex].typeId;
		hasKills = 0;
		if (g_pilotData.factionStatistics[factionIndex].stats.killsPerCraftPerMT[statCategory][typeId] ||
			g_pilotData.factionStatistics[factionIndex]
				.stats.killsSharedPerCraftPerMT[statCategory][typeId]) {
			hasKills = 1;
		}
		if (hasKills) {
			if (rowIndex >= g_statsCraftScrollOffset && rowIndex - g_statsCraftScrollOffset < visibleRows) {
				int logoValue;
				int logoWidth;
				int x;
				int logoCount;

				FrontendText_Draw(12, g_shipList[g_shipTypeToShipListIndex[typeId]].name, 65, yCraftList,
								  g_colorLightBlue);
				sprintf(g_frontendScratchBuffer, "%d (%d)",
						g_pilotData.factionStatistics[factionIndex]
							.stats.killsPerCraftPerMT[statCategory][typeId],
						g_pilotData.factionStatistics[factionIndex]
							.stats.killsSharedPerCraftPerMT[statCategory][typeId]);
				FrontendText_Draw(10, g_frontendScratchBuffer, 240, yCraftValue, 0xffff);

				logoValue = g_pilotData.factionStatistics[factionIndex]
								.stats.killsPerCraftPerMT[statCategory][typeId] +
							(g_pilotData.factionStatistics[factionIndex]
								 .stats.killsSharedPerCraftPerMT[statCategory][typeId] >>
							 1);
				FrontImage_GetResourceRect("implogo1", &out);
				logoWidth = out.right - out.left + 1;
				x = 290;
				logoCount = logoValue / 10;
				while (logoCount > 0) {
					if (x + logoWidth > 555) {
						break;
					}
					FrontImage_DrawSprite("implogo1", x, yCraftList);
					x += logoWidth + 1;
					--logoCount;
				}

				if (logoValue % 10) {
					int partialWidth;

					partialWidth = (logoValue % 10) * logoWidth;
					if (x + partialWidth / 10 < 555) {
						out.right = partialWidth / 10 + out.left - 1;
						FrontImage_DrawSpriteRectTransparent("implogo1", &out, x, yCraftList);
					}
				}

				yCraftList += 15;
				yCraftValue += 15;
			}
			++rowIndex;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x561860
int FamilyTransportRoom_DrawCombatSimulatorRecordPage(int statCategory) {
	FrontendRect out;
	int scrollOffset;
	int y;
	int row;
	int drawnRow;
	int rating;
	int shipIndex;

	if (!g_pilotData.name[0]) {
		return 0;
	}

	if (g_frontendFamilyPageResetPending) {
		int hasRows;

		g_combatRecordScrollOffset = 0;
		g_frontendFamilyPageResetPending = 0;
		g_statsPageRowCount = 18;
		if (g_frontendMissionLoaded && g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT &&
			(unsigned int)g_currentMissionId < 7u) {
			g_statsPageRowCount = 17;
		}

		memset(g_combatRecordLossesToNpcPilots, 0, sizeof(g_combatRecordLossesToNpcPilots));
		memset(g_combatRecordLossesToPlayerPilots, 0, sizeof(g_combatRecordLossesToPlayerPilots));
		memset(g_combatRecordSharedNpcPilotKills, 0, sizeof(g_combatRecordSharedNpcPilotKills));
		memset(g_combatRecordNpcPilotKills, 0, sizeof(g_combatRecordNpcPilotKills));
		memset(g_combatRecordSharedPlayerPilotKills, 0, sizeof(g_combatRecordSharedPlayerPilotKills));
		memset(g_combatRecordPlayerPilotKills, 0, sizeof(g_combatRecordPlayerPilotKills));
		memset(g_combatRecordSharedKills, 0, sizeof(g_combatRecordSharedKills));
		memset(g_combatRecordAssists, 0, sizeof(g_combatRecordAssists));

		g_combatRecordHasCraftRows = 0;
		hasRows = 0;
		for (shipIndex = 0; shipIndex < g_shipCount; ++shipIndex) {
			int typeId;

			typeId = g_shipList[shipIndex].typeId;
			g_combatRecordRowVisible = 0;
			if (g_pilotData.factionStatistics[2].stats.killsPerCraftPerMT[statCategory][typeId] ||
				g_pilotData.factionStatistics[2].stats.killsSharedPerCraftPerMT[statCategory][typeId]) {
				g_combatRecordRowVisible = 1;
				hasRows = 1;
				g_combatRecordHasCraftRows = 1;
			}

			if (g_combatRecordRowVisible) {
				++g_statsPageRowCount;
			}
		}
		if (hasRows) {
			g_statsPageRowCount += 2;
		}

		hasRows = 0;
		g_combatRecordHasPlayerRatingKills = 0;
		for (rating = 0; rating < 25; ++rating) {
			if (g_pilotData.factionStatistics[2].stats.killsFullOnPlayerRatingPerMT[statCategory][rating] ||
				g_pilotData.factionStatistics[2].stats.killsSharedOnPlayerRatingPerMT[statCategory][rating]) {
				hasRows = 1;
				++g_statsPageRowCount;
			}
		}
		g_combatRecordHasPlayerRatingKills = hasRows;
		if (hasRows) {
			g_statsPageRowCount += 2;
		}

		hasRows = 0;
		g_combatRecordHasPlayerRatingLosses = 0;
		for (rating = 0; rating < 25; ++rating) {
			g_combatRecordRowVisible = 0;
			if (g_pilotData.factionStatistics[2].stats.killedByPlayerRatingPerMT[statCategory][rating]) {
				g_combatRecordRowVisible = 1;
				hasRows = 1;
			}

			if (g_combatRecordRowVisible) {
				++g_statsPageRowCount;
			}
		}
		g_combatRecordHasPlayerRatingLosses = hasRows;
		if (hasRows) {
			g_statsPageRowCount += 2;
		}

		for (shipIndex = 0; shipIndex < g_shipCount; ++shipIndex) {
			int typeId;

			typeId = g_shipList[shipIndex].typeId;
			g_combatRecordAssists[statCategory] +=
				g_pilotData.factionStatistics[2].stats.killsAssistsPerCraftPerMT[statCategory][typeId];
			g_combatRecordSharedKills[statCategory] +=
				g_pilotData.factionStatistics[2].stats.killsSharedPerCraftPerMT[statCategory][typeId];
		}

		for (rating = 0; rating < 25; ++rating) {
			g_combatRecordPlayerPilotKills[statCategory] +=
				g_pilotData.factionStatistics[2].stats.killsFullOnPlayerRatingPerMT[statCategory][rating];
			g_combatRecordSharedPlayerPilotKills[statCategory] +=
				g_pilotData.factionStatistics[2].stats.killsSharedOnPlayerRatingPerMT[statCategory][rating];
		}

		for (rating = 0; rating < 6; ++rating) {
			g_combatRecordNpcPilotKills[statCategory] +=
				g_pilotData.factionStatistics[2].stats.killsFullOnAIRatingPerMT[statCategory][rating];
			g_combatRecordSharedNpcPilotKills[statCategory] +=
				g_pilotData.factionStatistics[2].stats.killsSharedOnAIRatingPerMT[statCategory][rating];
		}

		for (rating = 0; rating < 25; ++rating) {
			g_combatRecordLossesToPlayerPilots[statCategory] +=
				g_pilotData.factionStatistics[2].stats.killedByPlayerRatingPerMT[statCategory][rating];
		}

		for (rating = 0; rating < 6; ++rating) {
			g_combatRecordLossesToNpcPilots[statCategory] +=
				g_pilotData.factionStatistics[2].stats.killedByAIRatingPerMT[statCategory][rating];
		}
	}

	FrontendDraw_RectAssign(&out, 556, 81, 575, 411);
	if (g_statsPageRowCount > 22) {
		g_combatRecordScrollOffset = FrontendScrollbar_Draw(&out, g_combatRecordScrollOffset,
															g_statsPageRowCount, 0, 5, g_colorNavy, 3);
	}

	scrollOffset = g_combatRecordScrollOffset;
	y = 66;
	if (scrollOffset <= 0 && -scrollOffset < 22) {
		if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
			sprintf(g_frontendScratchBuffer, "%s", g_pilotData.name);
		} else {
			sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
					FrontendString_Get((UIString)(g_pilotData.pilotRank + STR_FAMILY_PLAYER_RANK + 1)), 1,
					g_pilotData.name);
		}
		FrontendText_Draw(12, g_frontendScratchBuffer, 65, 66, g_colorYellow);
		scrollOffset = g_combatRecordScrollOffset;
		y = 81;
	}

	row = 1;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		const char* assignment;

		if (g_frontendMissionLoaded) {
			if (g_frontendMission->header.hangar != XWA_HANGAR_FAMILYTRANSPORT) {
				assignment = FrontendString_Get((UIString)(g_frontendMission->header.briefingLogo + 727));
			} else if ((unsigned int)g_currentMissionId < 7u) {
				goto draw_total_score;
			} else {
				assignment = FrontendString_Get(STR_FAMILY_ON_LEAVE);
			}
		} else {
			assignment = FrontendString_Get(STR_FAMILY_ON_LEAVE);
		}

		sprintf(g_frontendScratchBuffer, "%c%s: %c%s", 2, FrontendString_Get(STR_FAMILY_ASSIGNED_TO), 1,
				assignment);
		FrontendText_Draw(12, g_frontendScratchBuffer, 65, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
		row = 2;
	}

draw_total_score:
	if (row >= scrollOffset && row - scrollOffset < 22) {
		sprintf(g_frontendScratchBuffer, "%c%s: %c%d", 2, FrontendString_Get(STR_TOTAL_SCORE), 1,
				g_pilotData.factionStatistics[2].score);
		FrontendText_Draw(12, g_frontendScratchBuffer, 65, y, 0xffff);
		sprintf(g_frontendScratchBuffer, "%c%s: %c%d", 2, FrontendString_Get(STR_DEBRIEF_BONUS_SCORE), 1,
				(unsigned int)g_pilotData.factionStatistics[2].bonusScore / 10u);
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	drawnRow = row + 1;
	if (drawnRow >= scrollOffset && drawnRow - scrollOffset < 22) {
		sprintf(g_frontendScratchBuffer, "%c%s %c%s", 2, FrontendString_Get(STR_PILOT_RATING), 6,
				FrontendString_Get((UIString)(g_pilotData.pilotRating + STR_TARGET_DRONE)));
		FrontendText_Draw(12, g_frontendScratchBuffer, 65, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		if ((unsigned int)g_pilotData.pilotRating < 24u && drawnRow >= scrollOffset &&
			drawnRow - scrollOffset < 22) {
			sprintf(g_frontendScratchBuffer, "%c%s %c%d%%", 2, FrontendString_Get(STR_PROMOTION_POINTS), 1,
					g_pilotData.nextPromotionPercent);
			FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
			scrollOffset = g_combatRecordScrollOffset;
		}
	}

	y += 15;
	row = drawnRow + 1;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		FrontendText_Draw(12, FrontendString_Get(STR_SUMMARY_OF_KILLS), 65, y, g_colorLightBlue);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		FrontendText_Draw(12, FrontendString_Get(STR_TOTAL_KILLS), 65, y, g_colorLightBlue);
		sprintf(g_frontendScratchBuffer, "%d (%d)",
				g_pilotData.factionStatistics[2].stats.totalKillsPerMT[statCategory],
				g_combatRecordSharedKills[statCategory]);
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		int kills;

		FrontendText_Draw(12, FrontendString_Get(STR_PLAYER_KILLS), 65, y, g_colorLightBlue);
		kills = g_combatRecordPlayerPilotKills[statCategory];
		if (kills || g_combatRecordSharedPlayerPilotKills[statCategory]) {
			sprintf(g_frontendScratchBuffer, "%d (%d)", kills,
					g_combatRecordSharedPlayerPilotKills[statCategory]);
		} else {
			sprintf(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		int kills;

		FrontendText_Draw(12, FrontendString_Get(STR_NON_PLAYER_KILLS), 65, y, g_colorLightBlue);
		kills = g_combatRecordNpcPilotKills[statCategory];
		if (kills || g_combatRecordSharedNpcPilotKills[statCategory]) {
			sprintf(g_frontendScratchBuffer, "%d (%d)", kills,
					g_combatRecordSharedNpcPilotKills[statCategory]);
		} else {
			sprintf(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		FrontendText_Draw(12, FrontendString_Get(STR_ASSISTS), 65, y, g_colorLightBlue);
		if (g_combatRecordAssists[statCategory]) {
			sprintf(g_frontendScratchBuffer, "%d", g_combatRecordAssists[statCategory]);
		} else {
			sprintf(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (g_combatRecordHasPlayerRatingKills) {
		if (row >= scrollOffset && row - scrollOffset < 22) {
			y += 15;
		}
		++row;
		if (row >= scrollOffset && row - scrollOffset < 22) {
			y += 15;
		}
		++row;
	}

	for (rating = 24; rating >= 0; --rating) {
		int kills;
		int sharedKills;

		kills = g_pilotData.factionStatistics[2].stats.killsFullOnPlayerRatingPerMT[statCategory][rating];
		sharedKills =
			g_pilotData.factionStatistics[2].stats.killsSharedOnPlayerRatingPerMT[statCategory][rating];
		if (kills || sharedKills) {
			if (row >= scrollOffset && row - scrollOffset < 22) {
				int logoValue;
				int logoWidth;
				int x;
				int logoCount;

				sprintf(g_frontendScratchBuffer, "%c%s", 6,
						FrontendString_Get((UIString)(rating + STR_TARGET_DRONE)));
				FrontendText_Draw(12, g_frontendScratchBuffer, 65, y, g_colorLightBlue);
				if (kills || sharedKills) {
					sprintf(g_frontendScratchBuffer, "%d(%d)", kills, sharedKills);
				} else {
					sprintf(g_frontendScratchBuffer, "----");
				}
				FrontendText_Draw(12, g_frontendScratchBuffer, 240, y, 0xffff);

				logoValue = kills + (sharedKills >> 1);
				FrontImage_GetResourceRect("implogo1", &out);
				logoWidth = out.right - out.left + 1;
				x = 290;
				logoCount = logoValue / 10;
				while (logoCount > 0) {
					if (x + logoWidth > 555) {
						break;
					}
					FrontImage_DrawSprite("implogo1", x, y);
					x += logoWidth + 1;
					--logoCount;
				}

				if (logoValue % 10) {
					int partialWidth;

					partialWidth = (logoValue % 10) * logoWidth;
					if (x + partialWidth / 10 < 555) {
						out.right = partialWidth / 10 + out.left - 1;
						FrontImage_DrawSpriteRectTransparent("implogo1", &out, x, y);
					}
				}

				scrollOffset = g_combatRecordScrollOffset;
				y += 15;
			}
			++row;
		}
	}

	if (g_combatRecordHasCraftRows) {
		if (row >= scrollOffset && row - scrollOffset < 22) {
			y += 15;
		}
		++row;
		if (row >= scrollOffset && row - scrollOffset < 22) {
			FrontendText_Draw(12, FrontendString_Get(STR_CRAFT_KILLS_BY_TYPE), 65, y, g_colorLightBlue);
			scrollOffset = g_combatRecordScrollOffset;
			y += 15;
		}
		++row;
	}

	for (shipIndex = 0; shipIndex < g_shipCount; ++shipIndex) {
		int typeId;
		int kills;
		int sharedKills;

		g_combatRecordRowVisible = 0;
		typeId = g_shipList[shipIndex].typeId;
		kills = g_pilotData.factionStatistics[2].stats.killsPerCraftPerMT[statCategory][typeId];
		sharedKills = g_pilotData.factionStatistics[2].stats.killsSharedPerCraftPerMT[statCategory][typeId];
		if (kills || sharedKills) {
			g_combatRecordRowVisible = 1;
		}

		if (g_combatRecordRowVisible) {
			if (row >= scrollOffset && row - scrollOffset < 22) {
				int logoValue;
				int logoWidth;
				int x;
				int logoCount;

				FrontendText_Draw(12, g_shipList[shipIndex].name, 65, y, g_colorRed);
				if (kills || sharedKills) {
					sprintf(g_frontendScratchBuffer, "%d (%d)", kills, sharedKills);
				} else {
					sprintf(g_frontendScratchBuffer, "----");
				}
				FrontendText_Draw(12, g_frontendScratchBuffer, 240, y, 0xffff);

				logoValue = kills + (sharedKills >> 1);
				FrontImage_GetResourceRect("implogo1", &out);
				logoWidth = out.right - out.left + 1;
				x = 290;
				logoCount = logoValue / 10;
				while (logoCount > 0) {
					if (x + logoWidth > 555) {
						break;
					}
					FrontImage_DrawSprite("implogo1", x, y);
					x += logoWidth + 1;
					--logoCount;
				}

				if (logoValue % 10) {
					int partialWidth;

					partialWidth = (logoValue % 10) * logoWidth;
					if (x + partialWidth / 10 < 555) {
						out.right = partialWidth / 10 + out.left - 1;
						FrontImage_DrawSpriteRectTransparent("implogo1", &out, x, y);
					}
				}

				scrollOffset = g_combatRecordScrollOffset;
				y += 15;
			}
			++row;
		}
	}

	if (row >= scrollOffset && row - scrollOffset < 22) {
		y += 15;
	}
	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		FrontendText_Draw(12, FrontendString_Get(STR_TOTAL_LOSSES), 65, y, g_colorLightBlue);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		FrontendText_Draw(12, FrontendString_Get(STR_TOTAL_CRAFT_LOSSES), 65, y, g_colorLightBlue);
		if (g_pilotData.factionStatistics[2].stats.totalCraftLossesPerMT[statCategory]) {
			sprintf(g_frontendScratchBuffer, "%d",
					g_pilotData.factionStatistics[2].stats.totalCraftLossesPerMT[statCategory]);
		} else {
			sprintf(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		FrontendText_Draw(12, FrontendString_Get(STR_TO_PLAYER_PILOTS), 65, y, g_colorLightBlue);
		if (g_combatRecordLossesToPlayerPilots[statCategory]) {
			sprintf(g_frontendScratchBuffer, "%d", g_combatRecordLossesToPlayerPilots[statCategory]);
		} else {
			sprintf(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		FrontendText_Draw(12, FrontendString_Get(STR_TO_NON_PLAYER_PILOTS), 65, y, g_colorLightBlue);
		if (g_combatRecordLossesToNpcPilots[statCategory]) {
			sprintf(g_frontendScratchBuffer, "%d", g_combatRecordLossesToNpcPilots[statCategory]);
		} else {
			sprintf(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		FrontendText_Draw(12, FrontendString_Get(STR_TO_STARSHIPS), 65, y, g_colorLightBlue);
		if (g_pilotData.factionStatistics[2].stats.lossesByStarshipsPerMT[statCategory]) {
			sprintf(g_frontendScratchBuffer, "%d",
					g_pilotData.factionStatistics[2].stats.lossesByStarshipsPerMT[statCategory]);
		} else {
			sprintf(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		FrontendText_Draw(12, FrontendString_Get(STR_TO_MINES), 65, y, g_colorLightBlue);
		if (g_pilotData.factionStatistics[2].stats.lossesByMinesPerMT[statCategory]) {
			sprintf(g_frontendScratchBuffer, "%d",
					g_pilotData.factionStatistics[2].stats.lossesByMinesPerMT[statCategory]);
		} else {
			sprintf(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (row >= scrollOffset && row - scrollOffset < 22) {
		int losses;

		FrontendText_Draw(12, FrontendString_Get(STR_FROM_COLLISIONS), 65, y, g_colorLightBlue);
		losses = g_pilotData.factionStatistics[2].stats.lossesByCollisionsPerMT[statCategory];
		if (losses) {
			sprintf(g_frontendScratchBuffer, "%d", losses);
		} else {
			sprintf(g_frontendScratchBuffer, "----");
		}
		FrontendText_Draw(12, g_frontendScratchBuffer, 350, y, 0xffff);
		scrollOffset = g_combatRecordScrollOffset;
		y += 15;
	}

	++row;
	if (g_combatRecordHasPlayerRatingLosses) {
		if (row >= scrollOffset && row - scrollOffset < 22) {
			y += 15;
		}
		++row;
		if (row >= scrollOffset && row - scrollOffset < 22) {
			FrontendText_Draw(12, FrontendString_Get(STR_LOSSES_FROM_PLAYERS_BY_RATING), 65, y,
							  g_colorLightBlue);
			scrollOffset = g_combatRecordScrollOffset;
			y += 15;
		}
		++row;
	}

	for (rating = 24; rating >= 0; --rating) {
		int losses;

		losses = g_pilotData.factionStatistics[2].stats.killedByPlayerRatingPerMT[statCategory][rating];
		if (losses) {
			if (row >= scrollOffset && row - scrollOffset < 22) {
				int logoWidth;
				int x;
				int logoCount;

				sprintf(g_frontendScratchBuffer, "%c%s", 6,
						FrontendString_Get((UIString)(rating + STR_TARGET_DRONE)));
				FrontendText_Draw(12, g_frontendScratchBuffer, 65, y, g_colorLightBlue);
				if (losses) {
					sprintf(g_frontendScratchBuffer, "%d", losses);
				} else {
					sprintf(g_frontendScratchBuffer, "----");
				}
				FrontendText_Draw(12, g_frontendScratchBuffer, 240, y, 0xffff);

				FrontImage_GetResourceRect("reblogo1", &out);
				logoWidth = out.right - out.left + 1;
				x = 290;
				logoCount = losses / 10;
				while (logoCount > 0) {
					if (x + logoWidth > 555) {
						break;
					}
					FrontImage_DrawSprite("reblogo1", x, y);
					x += logoWidth + 1;
					--logoCount;
				}

				if (losses % 10) {
					int partialWidth;

					partialWidth = (losses % 10) * logoWidth;
					if (x + partialWidth / 10 < 555) {
						out.right = partialWidth / 10 + out.left - 1;
						FrontImage_DrawSpriteRectTransparent("reblogo1", &out, x, y);
					}
				}

				scrollOffset = g_combatRecordScrollOffset;
				y += 15;
			}
			++row;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x562AF0
int FrontendFamily_DrawSidebarsAndStatsControls(void) {
	int result;
	int outY;
	int outX;
	FrontendRect rightRect;
	FrontendRect buttonRect;
	FrontendRect leftRect;
	char leftBarName[32] = "leftbar1";
	char rightBarName[32] = "rightbar1";

	leftBarName[7] = (char)(g_frontendLeftBarPanelIndex + '0');
	rightBarName[8] = (char)(g_frontendRightBarPanelIndex + '0');
	FrontImage_GetResourceRect(leftBarName, &leftRect);
	FrontImage_GetResourceRect(rightBarName, &rightRect);

	if (!g_frontendRightBarAnimState) {
		FrontImage_DrawSprite(rightBarName, rightRect.left - rightRect.right + 639,
							  rightRect.top - rightRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (FrontImage_GetSpriteFrame(rightBarName) == 9) {
			g_frontendRightBarAnimState = 1;
		}
	} else if (g_frontendRightBarAnimState == 1) {
		FrontImage_DrawSprite(rightBarName, rightRect.left - rightRect.right + 639,
							  rightRect.top - rightRect.bottom + 479);
	} else if (g_frontendRightBarAnimState == 2) {
		FrontImage_DrawSprite(rightBarName, rightRect.left - rightRect.right + 639,
							  rightRect.top - rightRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			g_frontendRightBarAnimState = 3;
		}
	} else if (g_frontendRightBarAnimState == 4) {
		FrontImage_DrawSprite(rightBarName, rightRect.left - rightRect.right + 639,
							  rightRect.top - rightRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			g_frontendRightBarAnimState = 3;
			g_frontendFamilyDetailMode = 0;
			FrontendFamily_DrawRoomBackground();
		}
	}

	if (!g_frontendLeftBarAnimState) {
		FrontImage_DrawSprite(leftBarName, 0, leftRect.top - leftRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(leftBarName, 1);
		if (FrontImage_GetSpriteFrame(leftBarName) == 9) {
			g_frontendLeftBarAnimState = 1;
		}
	} else if (g_frontendLeftBarAnimState == 1) {
		FrontImage_DrawSprite(leftBarName, 0, leftRect.top - leftRect.bottom + 479);
	} else if (g_frontendLeftBarAnimState == 2) {
		FrontImage_DrawSprite(leftBarName, 0, leftRect.top - leftRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(leftBarName, 1);
		if (!FrontImage_GetSpriteFrame(leftBarName)) {
			g_frontendLeftBarAnimState = 3;
		}
	} else if (g_frontendLeftBarAnimState == 4) {
		FrontImage_DrawSprite(leftBarName, 0, leftRect.top - leftRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(leftBarName, 1);
		if (!FrontImage_GetSpriteFrame(leftBarName)) {
			g_frontendLeftBarAnimState = 3;
			g_frontendFamilyDetailMode = 0;
		}
	}

	if (g_frontendFamilyDetailMode == 1) {
		FrontendCursor_GetPos(&outX, &outY);
		if (g_frontendLeftBarAnimState == 1) {
			FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[3]);
			if (g_familyTransportRoomPageOrRevealCounter == 3) {
				g_frontendFamilyHasNewEmail = 0;
				FrontendButton_DrawCenteredTintedSpriteWithTooltip(&buttonRect, "nonewemail",
																   FrontendString_Get(STR_FAMILY_CHECK_EMAIL),
																   (unsigned int)g_colorGreen);
			} else {
				if (g_frontendFamilyHasNewEmail) {
					result = FrontendButton_DrawSpriteWithHoverText(
						&buttonRect, "newemail", "newemail",
						(void*)FrontendString_Get(STR_FAMILY_CHECK_EMAIL), (unsigned int)g_colorPaleBlue,
						(unsigned int)g_colorLightBlue, 12, "jewelsound");
				} else {
					result = FrontendButton_DrawSpriteWithHoverText(
						&buttonRect, "nonewemail", "nonewemail",
						(void*)FrontendString_Get(STR_FAMILY_CHECK_EMAIL), (unsigned int)g_colorPaleBlue,
						(unsigned int)g_colorLightBlue, 12, "jewelsound");
				}

				if (result) {
					g_frontendFamilyHasNewEmail = 0;
					g_familyTransportRoomPageOrRevealCounter = 3;
					g_frontendFamilyPageResetPending = 1;
				}
			}

			FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[2]);
			if (g_familyTransportRoomPageOrRevealCounter == 2) {
				FrontendButton_DrawCenteredTintedSpriteWithTooltip(
					&buttonRect, "comrecord", FrontendString_Get(STR_FAMILY_COMBAT_CHAMBER_STATISTICS),
					(unsigned int)g_colorGreen);
			} else {
				result = FrontendButton_DrawSpriteWithHoverText(
					&buttonRect, "comrecord", "comrecord",
					(void*)FrontendString_Get(STR_FAMILY_COMBAT_CHAMBER_STATISTICS),
					(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 13, "jewelsound");
				if (result) {
					g_familyTransportRoomPageOrRevealCounter = 2;
					g_frontendFamilyPageResetPending = 1;
				}
			}

			FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[1]);
			if (g_familyTransportRoomPageOrRevealCounter == 1) {
				FrontendButton_DrawCenteredTintedSpriteWithTooltip(
					&buttonRect, "azrecord", FrontendString_Get(STR_FAMILY_FAMILY_STATISTICS),
					(unsigned int)g_colorGreen);
			} else {
				result = FrontendButton_DrawSpriteWithHoverText(
					&buttonRect, "azrecord", "azrecord",
					(void*)FrontendString_Get(STR_FAMILY_FAMILY_STATISTICS), (unsigned int)g_colorPaleBlue,
					(unsigned int)g_colorLightBlue, 14, "jewelsound");
				if (result) {
					g_familyTransportRoomPageOrRevealCounter = 1;
					g_frontendFamilyPageResetPending = 1;
				}
			}

			FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[0]);
			if (!g_familyTransportRoomPageOrRevealCounter) {
				FrontendButton_DrawCenteredTintedSpriteWithTooltip(
					&buttonRect, "todrecord", FrontendString_Get(STR_FAMILY_TOUR_STATISTICS),
					(unsigned int)g_colorGreen);
			} else {
				result = FrontendButton_DrawSpriteWithHoverText(
					&buttonRect, "todrecord", "todrecord",
					(void*)FrontendString_Get(STR_FAMILY_TOUR_STATISTICS), (unsigned int)g_colorPaleBlue,
					(unsigned int)g_colorLightBlue, 15, "jewelsound");
				if (result) {
					g_familyTransportRoomPageOrRevealCounter = 0;
					g_frontendFamilyPageResetPending = 1;
				}
			}
		}

		if (g_frontendRightBarAnimState == 1) {
			FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[9]);
			result = FrontendButton_DrawSpriteWithHoverText(
				&buttonRect, "back", "back", (void*)FrontendString_Get(STR_DONE),
				(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 241, "jewelsound");
			if (result) {
				g_frontendLeftBarAnimState = 4;
				g_frontendRightBarAnimState = 4;
				return 1;
			}
		}
	} else if (g_frontendFamilyDetailMode > 1 && g_frontendRightBarAnimState == 1) {
		FrontendDraw_RectCopy(&buttonRect, &g_frontendSidebarButtonRects[9]);
		result = FrontendButton_DrawSpriteWithHoverText(
			&buttonRect, "back", "back", (void*)FrontendString_Get(STR_DONE), (unsigned int)g_colorPaleBlue,
			(unsigned int)g_colorLightBlue, 241, "jewelsound");
		if (result) {
			g_frontendRightBarAnimState = 4;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x55FF30
int FamilyTransportRoom_Update(int frameCounter) {
	int trophyHotspotActive;
	int outX;
	FrontendRect rect;
	unsigned int screenTick;

	if (frameCounter) {
		screenTick = (unsigned int)frameCounter;
	} else {
		screenTick = 0;
	}

	if (!screenTick) {
		musicState = MUSIC_STATE_FRONTEND_1210;
		if (g_gameConfig.datapadMusicEnabled) {
			Music_SetState(MUSIC_STATE_FRONTEND_1210);
		} else {
			Music_Stop();
		}

		g_frontendFamilyDetailMode = 0;
		if (g_frontendMissionLoaded && g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
			FrontendCursor_SetPos(172, 357);
		}

		g_familyTransportRoomPendingTransition = FAMILY_TRANSPORT_ROOM_TRANSITION_NONE;
		g_frontendLeftBarAnimState = 3;
		g_frontendLeftBarPanelIndex = 4;
		FrontImage_SetSpriteFrame("leftbar4", 0);
		g_frontendRightBarAnimState = 3;
		g_frontendRightBarPanelIndex = 1;
		FrontImage_SetSpriteFrame("rightbar1", 0);
		FrontImage_SetSpriteFrame("rightbar2", 0);
		FrontImage_SetSpriteFrame("rightbar3", 0);
		FrontImage_LoadResourceList("frontres\\medals\\medals.lst");
		FrontendEmail_LoadList();
		FrontendFamily_LoadAwardTextList();
		g_frontendFamilyHasNewEmail = 0;
		g_frontendFamilyEmkayVoiceTimer = 0;
		if (g_frontendMissionLoaded) {
			unsigned int i;

			for (i = 0; i < (unsigned int)g_frontendEmailCount; ++i) {
				if ((unsigned int)g_frontendEmailEntries[i].field00 <= (unsigned int)g_currentMissionId &&
					!g_pilotData.emailsStatus[g_frontendEmailEntries[i].emailIndex]) {
					g_frontendFamilyHasNewEmail = 1;
					g_pilotData.emailsStatus[g_frontendEmailEntries[i].emailIndex] = 1;
				}
			}
		}

		FrontendText_SetGlyphGradientBg(g_colorNearBlack);
		g_familyTransportRoomLabelColor = FrontendDisplay_PackRGB(0x60, 0x0c, 0x0c);
		MissionSetup_LoadMissionList(MISSION_DIRECTORY_TOUR);
		FrontendFamily_DrawRoomBackground();
		if (!g_skipFrontendEntryMovie) {
			int voiceStarted;

			voiceStarted = FrontendFamily_PlayEmkayVoiceLine(g_frontendFamilyEmkayVoiceTimer);
			if (!voiceStarted) {
				g_frontendFamilyEmkayVoiceTimer = 48;
			} else {
				++g_frontendFamilyEmkayVoiceTimer;
			}
			g_skipFrontendEntryMovie = 0;
		} else {
			g_frontendFamilyEmkayVoiceTimer = 9999;
			g_skipFrontendEntryMovie = 0;
		}
	}

	if (g_familyTransportRoomPendingTransition) {
		FrontendFamily_DrawSidebarsAndStatsControls();
		if (g_frontendLeftBarAnimState == 3 && g_frontendRightBarAnimState == 3) {
			switch (g_familyTransportRoomPendingTransition) {
				case FAMILY_TRANSPORT_ROOM_TRANSITION_CONCOURSE:
					g_pilotData.campaignMode = 0;
					FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
					return 0;
				case FAMILY_TRANSPORT_ROOM_TRANSITION_HOST_GAME:
					g_pilotData.campaignMode = 0;
					FrontendScreen_SetCallbacks(FrontendNet_HostGameScreen, FrontendNet_HostGameExit);
					return 0;
				case FAMILY_TRANSPORT_ROOM_TRANSITION_COMBAT_SIM_MENU:
					g_pilotData.campaignMode = 0;
					FrontendScreen_SetCallbacks(CombatSimMenu_Update, CombatSimMenu_Exit);
					return 0;
				case FAMILY_TRANSPORT_ROOM_TRANSITION_BRIEFING_ROOM:
					FrontendScreen_SetCallbacks(BriefingRoom_Update, BriefingRoom_Exit);
					break;
				default:
					break;
			}
		}
		return 0;
	}

	trophyHotspotActive = FrontendFamily_HandleTrophyHotspots(0);
	FrontendCursor_GetPos(&outX, &frameCounter);
	if (g_optWkey && Keyboard_BufferContains('w')) {
		int missionId;

		Keyboard_FlushCharBuffer();
		missionId = g_currentMissionId;
		g_pilotData.tourOfDutyMissions[missionId].numberTimesFlown = 1;
		g_pilotData.tourOfDutyMissions[missionId].completedCount = 1;
		Concourse_LoadNextTourMission();
		FrontendFamily_DrawRoomBackground();
	}

	if (!g_frontendFamilyDetailMode) {
		int isFamilyMission;

		if (FrontendWaveStream_IsPlaying()) {
			FrontImage_DrawSprite("mkeye", 160, 200);
			if (screenTick & 1) {
				FrontImage_AdvanceSpriteFrame("mkeye", 1);
			}
		} else if ((unsigned int)g_frontendFamilyEmkayVoiceTimer < 9999u) {
			if (g_frontendFamilyEmkayVoiceTimer == 48) {
				FrontendFamily_PlayEmkayVoiceLine(48);
				g_frontendFamilyEmkayVoiceTimer = 9999;
			} else {
				++g_frontendFamilyEmkayVoiceTimer;
			}
		}

		FrontendFamily_DrawEmailMonitor(screenTick);
		isFamilyMission = 0;
		if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
			isFamilyMission = 1;
		}

		if (!trophyHotspotActive) {
			if (isFamilyMission) {
				FrontendDraw_RectAssign(&rect, 0, 150, 120, 287);
				if (FrontendDraw_PointInRect(&rect, outX, frameCounter)) {
					FamilyTransportRoom_DrawHoverLabel(&rect, STR_CONCOURSE_COMBAT_SIMULATOR);
					if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
						g_familyTransportRoomPendingTransition =
							FAMILY_TRANSPORT_ROOM_TRANSITION_COMBAT_SIM_MENU;
					}
				}
			} else {
				FrontendDraw_RectAssign(&rect, 0, 150, 120, 287);
				if (FrontendDraw_PointInRect(&rect, outX, frameCounter)) {
					FamilyTransportRoom_DrawHoverLabel(&rect, STR_FAMILY_BACK_TO_CALAMARI);
					if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
						g_familyTransportRoomPendingTransition = FAMILY_TRANSPORT_ROOM_TRANSITION_CONCOURSE;
					}
				}
			}

			FrontendDraw_RectAssign(&rect, 403, 228, 499, 307);
			if (FrontendDraw_PointInRect(&rect, outX, frameCounter)) {
				FrontendCursor_SetImageResourceForCurrentTheme("cursor3", g_cursorBitmap);
				FamilyTransportRoom_DrawHoverLabel(&rect, STR_STATISTICS);
				if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
					g_frontendLeftBarAnimState = 0;
					g_frontendRightBarAnimState = 0;
					g_frontendFamilyPageResetPending = 1;
					g_frontendFamilyDetailMode = 1;
					g_familyTransportRoomPageOrRevealCounter = g_frontendFamilyHasNewEmail != 0 ? 3 : 0;
					FrontendFamily_DrawRoomBackground();
				}
			} else {
				FrontendCursor_SetImageResourceForCurrentTheme("cursor", g_cursorBitmap);
			}

			if (g_frontendMissionLoaded && g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				FrontendDraw_RectAssign(&rect, 103, 288, 234, 416);
				if (FrontendDraw_PointInRect(&rect, outX, frameCounter)) {
					FamilyTransportRoom_DrawHoverLabel(&rect, STR_FAMILY_PLAY_MISSION);
					if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
						g_familyTransportRoomPendingTransition =
							FAMILY_TRANSPORT_ROOM_TRANSITION_BRIEFING_ROOM;
					}
				}
			}
		}
	} else if (g_frontendFamilyDetailMode == 1) {
		switch (g_familyTransportRoomPageOrRevealCounter) {
			case 0:
			case 1:
			case 2:
				FamilyTransportRoom_DrawPilotStatsPage(g_familyTransportRoomPageOrRevealCounter);
				break;
			case 3:
				FrontendEmail_DrawInbox();
				break;
			case 4:
				NetSession_StubReturnTrue();
				break;
			default:
				break;
		}
	} else if (g_frontendFamilyDetailMode > 1) {
		FrontendCursor_SetImageResourceForCurrentTheme("cursor", g_cursorBitmap);
		frameCounter = 10;
		FrontendDraw_RectAssign(&rect, 65, 10, 575, 25);
		FrontendText_DrawCenteredReveal(12, g_frontendFamilyDetailTitle, &rect, g_colorLightBlue,
										g_familyTransportRoomPageOrRevealCounter++);
		if (g_frontendFamilyDetailMode == 2 && g_frontendFamilyAwardTexts != NULL) {
			FrontendDraw_RectAssign(&rect, 120, 436, 520, 480);
			FrontendText_DrawWrappedClipped(
				12, g_frontendFamilyAwardTexts[g_frontendFamilySelectedAwardTextIdx].text, &rect,
				g_colorLightBlue, 2, 0);
		}
	}

	FrontendFamily_DrawSidebarsAndStatsControls();
	{
		int result;

		result = Frontend_HandleEscapeQuit(1);
		return result == 1;
	}
}

// FUNCTION: XWA 0x55FE90
int FamilyTransportRoom_Exit(int frameCounter) {
	(void)frameCounter;

	FrontImage_FreeResourceByName("background");
	Frontend_ResetScrollableControls();
	FrontendWaveStream_Shutdown();
	FrontImage_UnloadResourceList("frontres\\medals\\medals.lst");
	FrontendCursor_SetImageResourceForCurrentTheme("cursor", g_cursorBitmap);
	if (g_missionList != NULL) {
		Mem_Free(g_missionList);
		g_missionList = NULL;
	}
	if (g_frontendEmailEntries != NULL) {
		Mem_Free(g_frontendEmailEntries);
		g_frontendEmailEntries = NULL;
	}
	if (g_frontendFamilyAwardTexts != NULL) {
		Mem_Free(g_frontendFamilyAwardTexts);
		g_frontendFamilyAwardTexts = NULL;
	}
	g_pilotData.emkayAnnounceNewAward = 0;
	g_pilotData.emkayAnnounceNewRank = 0;
	return 0;
}
