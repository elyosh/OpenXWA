#include "xwa/frontend/briefing_room.h"

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/ship_list.h"
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
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/frontend_wave_stream.h"
#include "xwa/frontend/mission_briefing.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/frontend/model_preview.h"
#include "xwa/movie/movie.h"
#include "xwa/util/memory.h"
#include "xwa/util/time.h"
#include "xwa_runtime/timing/host_clock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0x784080
int g_oobCraftCounts[BRIEFING_ROOM_OOB_CRAFT_TYPE_COUNT];
// GLOBAL: XWA 0x78498C
int g_oobEntryCount;
// GLOBAL: XWA 0x784990
int g_oobMaxWidth;
// GLOBAL: XWA 0x784880
char g_briefingNarrationWav[256];
// GLOBAL: XWA 0x784994
int g_briefingRoomStateFrameCounter;
// GLOBAL: XWA 0x784998
int g_briefingRoomState;
// GLOBAL: XWA 0x78499C
int g_briefingRoomRevealCount;
// GLOBAL: XWA 0x784078
int g_unusedBriefingRoomInitState;
// GLOBAL: XWA 0x784984
int g_briefingRoomSquadLogoX;
// GLOBAL: XWA 0x784980
int g_briefingRoomSquadLogoY;

extern int g_currentCdDisk;

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x578660
int BriefingRoom_PrepareTourBriefing(void) {
	int missionListIndex;
	int prefixNumber;
	int battleNumber;
	int missionNumber;
	char separator;
	char waveId[20];
	XwaFile* stream;

	g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_TOUR] = g_currentMissionId;
	g_pilotData.missionDirectoryId = MISSION_DIRECTORY_TOUR;
	g_pilotData.currentFactionId = 0;
	g_pilotData.unk2 = 0;

	strcpy(g_mpRoster[0].name, g_pilotData.name);
	g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_SINGLEPLAYER;
	g_mpRoster[0].playerId = 1;
	g_mpRoster[0].rating = g_pilotData.pilotRating;

	FrontendMission_LoadCurrent();
	MissionSetup_CountActiveTeams();
	MissionSetup_RebuildCombatSimSlotsFromFrontendMission();

	for (missionListIndex = 0; missionListIndex < 16; ++missionListIndex) {
		if (g_combatSimSlots[missionListIndex].craftType != 0) {
			g_combatSimSlots[missionListIndex].ownerPlayerId = g_mpRoster[0].playerId;
			g_pilotData.team =
				g_frontendMission->flightGroups[(int16_t)g_combatSimSlots[missionListIndex].fgIndex].team;
			break;
		}
	}

	g_pilotData.factionStatistics[g_pilotData.currentFactionId].team = g_pilotData.team;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionDirectoryId =
		g_pilotData.missionDirectoryId;
	memcpy(g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionDescriptionIds,
		   g_pilotData.missionDescriptionIds, sizeof(g_pilotData.missionDescriptionIds));
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].m0048 = g_pilotData.unk2;

	g_unusedFrontendMissionLaunchPrepared = 1;
#ifdef XWA_MODERN
	srand(XwaTime_GetElapsedTicks());
#else
	srand(GetTickCount());
#endif
	g_gameConfig.randomSeed = rand();
	MissionSetup_BuildBriefingText(g_briefingText);
	BriefingRoom_ComputeOrderOfBattle();

	for (missionListIndex = 0; missionListIndex < g_missionCount; ++missionListIndex) {
		if (g_missionList[missionListIndex].missionIdx == g_currentMissionId) {
			g_selectedMissionListIndex = missionListIndex;
			break;
		}
	}

	sscanf(g_missionList[missionListIndex].fileName, "%d%c%d%c%d", &prefixNumber, &separator, &battleNumber,
		   &separator, &missionNumber);
	if (battleNumber < 0 || battleNumber > 99) {
		g_briefingNarrationWav[0] = '\0';
	} else if (missionNumber < 0 || missionNumber > 99) {
		g_briefingNarrationWav[0] = '\0';
	} else {
		sprintf(waveId, "%2d%2d%2d", battleNumber, missionNumber, 1);
		if (waveId[0] == ' ') {
			waveId[0] = '0';
		}
		if (waveId[2] == ' ') {
			waveId[2] = '0';
		}
		if (waveId[4] == ' ') {
			waveId[4] = '0';
		}

		sprintf(g_briefingNarrationWav, "wave\\frontend\\B%dM%d\\N%s.wav", battleNumber, missionNumber,
				waveId);
		stream = File_Open(AERON_VFS_ROOT_ASSET, g_briefingNarrationWav, "rb");
		if (stream == NULL) {
			int randomLine;

			randomLine = rand() % 13;
			if (g_frontendMission->header.briefingOfficer == 0) {
				sprintf(waveId, "N01DE%2d.wav", randomLine);
				waveId[5] = waveId[5] == ' ' ? '0' : waveId[5];
			} else if (g_frontendMission->header.briefingOfficer == 1) {
				sprintf(waveId, "N01KU%2d.wav", randomLine);
				if (waveId[5] == ' ') {
					waveId[5] = '0';
				}
			} else if (g_frontendMission->header.briefingOfficer == 2) {
				sprintf(waveId, "N01ZL%2d.wav", randomLine);
				if (waveId[5] == ' ') {
					waveId[5] = '0';
				}
			} else {
				sprintf(waveId, "N01ZL%2d.wav", randomLine);
				if (waveId[5] == ' ') {
					waveId[5] = '0';
				}
			}
			sprintf(g_briefingNarrationWav, "wave\\frontend\\%s", waveId);
		} else {
			File_Close(stream);
		}
	}

	if (battleNumber < 0 || battleNumber > 99 || missionNumber < 0 || missionNumber > 99) {
		g_pendingVoiceWav[0] = '\0';
		return 1;
	}

	sprintf(waveId, "%2d%2d%2d", battleNumber, missionNumber, 1);
	if (waveId[0] == ' ') {
		waveId[0] = '0';
	}
	if (waveId[2] == ' ') {
		waveId[2] = '0';
	}
	if (waveId[4] == ' ') {
		waveId[4] = '0';
	}
	sprintf(g_pendingVoiceWav, "wave\\frontend\\B%dM%d\\S%s.wav", battleNumber, missionNumber, waveId);
	return 1;
}

// FUNCTION: XWA 0x57E430
int BriefingRoom_SetupCurrentMissionSession(void) {
	int playerId;
	int slotIndex;

	strcpy(g_mpRoster[0].name, g_pilotData.name);
	g_mpRoster[0].playerId = 1;
	g_mpRoster[0].rating = g_pilotData.pilotRating;
	g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_SINGLEPLAYER;

	g_pilotData.missionDirectoryId = MISSION_DIRECTORY_TOUR;
	g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_TOUR] = g_currentMissionId;
	g_pilotData.campaignMode = 1;
	FrontendMission_LoadCurrent();
	MissionSetup_CountActiveTeams();
	MissionSetup_RebuildCombatSimSlotsFromFrontendMission();

	playerId = g_mpRoster[0].playerId;
	for (slotIndex = 0; slotIndex < 16; ++slotIndex) {
		if (g_combatSimSlots[slotIndex].craftType != 0) {
			g_combatSimSlots[slotIndex].ownerPlayerId = playerId;
		}
	}

	g_pilotData.team = 0;
	g_unusedFrontendMissionLaunchPrepared = 1;
	g_skipFrontendEntryMovie = 1;
	FrontendMission_InitPlayerState();
	return 1;
}

// FUNCTION: XWA 0x579220
int BriefingRoom_DrawMissionBriefingTextReveal(int revealRadius) {
	FrontendRect rect;
	int lineCount;

	FrontendDraw_RectAssign(&rect, 65, 165, 575, 186);
	if (g_frontendMission->header.missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
		FrontendText_DrawCenteredReveal(12, FrontendString_Get(STR_FAMILY_MISSION_OVERVIEW), &rect,
										g_colorLightBlue, revealRadius);
	} else {
		FrontendText_DrawCenteredReveal(12, FrontendString_Get(STR_FAMILY_COMMANDER_BRIEFING), &rect,
										g_colorLightBlue, revealRadius);
	}

	FrontendDraw_RectAssign(&rect, 70, 191, 570, 383);
	lineCount = FrontendText_DrawWrappedClipped(12, g_briefingText, &rect, 0xffff, 4, 4096) + 1;
	if (lineCount > 12) {
		FrontendDraw_RectAssign(&rect, 70, 191, 550, 383);
		lineCount = FrontendText_DrawWrappedClipped(12, g_briefingText, &rect, 0xffff, 4, 4096) + 1;
		FrontendDraw_RectAssign(&rect, 551, 191, 570, 383);
		g_frontendFirstVisibleLine =
			FrontendScrollbar_Draw(&rect, g_frontendFirstVisibleLine, lineCount, 0, 5, g_colorNavy, 9);
		FrontendDraw_RectAssign(&rect, 70, 191, 550, 383);
	} else {
		FrontendDraw_RectAssign(&rect, 70, 191, 570, 383);
	}

	FrontendDraw_RectClipToBounds(&rect);
	FrontendText_DrawWrapped(12, g_briefingText, &rect, g_colorLightBlue, 4, g_frontendFirstVisibleLine, 0,
							 revealRadius);
	return 1;
}

// FUNCTION: XWA 0x578A60
int BriefingRoom_DrawOrderOfBattle(FrontendRect* rect, int revealCount) {
	int linesPerPage;
	int pageIndex;
	int revealPhase;
	int savedRevealPhase;
	int entryIndex;
	FrontendRect textRect;
	uint8_t missionType;

	FrontendDraw_RectCopy(&textRect, rect);
	missionType = g_frontendMission->header.missionType;
	if (missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
		strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_MISSION_CRAFT));
	} else {
		strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_ORDER_OF_BATTLE));
	}
	FrontendText_DrawReveal(12, g_frontendScratchBuffer, textRect.left, textRect.top, g_colorLightBlue,
							revealCount);

	textRect.top += 20;
	linesPerPage = (textRect.bottom - textRect.top + 1) / 15;
	textRect.bottom = textRect.top + 14;
	pageIndex = 0;
	revealPhase = revealCount % (g_oobMaxWidth + 100);
	savedRevealPhase = revealPhase;
	if (g_oobEntryCount > linesPerPage) {
		pageIndex =
			revealCount / (g_oobMaxWidth + 100) % ((linesPerPage + g_oobEntryCount - 1) / linesPerPage);
	}

	entryIndex = 0;
	if ((int16_t)g_frontendMission->flightGroupCount > 0) {
		int flightGroupIndex;

		flightGroupIndex = 0;
		do {
			XwaFlightGroup* flightGroup;
			ShipListEntry* shipEntry;
			int category;

			flightGroup = &g_frontendMission->flightGroups[flightGroupIndex];
			if (flightGroup->team == g_pilotData.team && flightGroup->arrival[0].triggers[0].condition == 0 &&
				flightGroup->arrival[0].triggers[0].variableType == 1 &&
				flightGroup->arrival[0].triggers[0].variable == 0 &&
				flightGroup->arrival[0].triggers[1].condition == 0 &&
				flightGroup->arrival[0].triggers[1].variableType == 1 &&
				flightGroup->arrival[0].triggers[1].variable == 0 && flightGroup->arrival[0].t1OrT2 == 0) {
				shipEntry = &g_shipList[g_shipTypeToShipListIndex[flightGroup->craftType]];
				category = shipEntry->category;
				if (category == 6) {
					if (entryIndex >= linesPerPage * pageIndex &&
						entryIndex < linesPerPage * (pageIndex + 1)) {
						sprintf(g_frontendScratchBuffer, "%s %s", shipEntry->name, flightGroup->name);
						if (g_oobEntryCount > linesPerPage) {
							if (revealPhase >= 100) {
								revealPhase = savedRevealPhase;
								FrontendText_DrawReveal(12, g_frontendScratchBuffer, textRect.left,
														textRect.top, g_colorLightBlue,
														(int)strlen(g_frontendScratchBuffer) -
															savedRevealPhase + 100);
							} else {
								FrontendText_DrawReveal(12, g_frontendScratchBuffer, textRect.left,
														textRect.top, g_colorLightBlue, revealPhase);
							}
						} else {
							FrontendText_DrawReveal(12, g_frontendScratchBuffer, textRect.left, textRect.top,
													g_colorLightBlue, revealCount);
						}
						FrontendDraw_RectOffsetXY(&textRect, 0, 15);
					}
					++entryIndex;
				} else if (category < 6 && category != 4 && category != 1 && shipEntry->flyable) {
					if (entryIndex >= linesPerPage * pageIndex &&
						entryIndex < linesPerPage * (pageIndex + 1)) {
						sprintf(g_frontendScratchBuffer, "%s %s", shipEntry->name, flightGroup->name);
						if (g_oobEntryCount > linesPerPage) {
							if (revealPhase >= 100) {
								revealPhase = savedRevealPhase;
								FrontendText_DrawReveal(12, g_frontendScratchBuffer, textRect.left,
														textRect.top, g_colorLightBlue,
														(int)strlen(g_frontendScratchBuffer) -
															savedRevealPhase + 100);
							} else {
								FrontendText_DrawReveal(12, g_frontendScratchBuffer, textRect.left,
														textRect.top, g_colorLightBlue, revealPhase);
							}
						} else {
							FrontendText_DrawReveal(12, g_frontendScratchBuffer, textRect.left, textRect.top,
													g_colorLightBlue, revealCount);
						}
						FrontendDraw_RectOffsetXY(&textRect, 0, 15);
					}
					++entryIndex;
				}
			}
			++flightGroupIndex;
		} while (flightGroupIndex < (int16_t)g_frontendMission->flightGroupCount);
	}

	{
		int shipIndex;

		for (shipIndex = 0; shipIndex < g_shipCount; ++shipIndex) {
			ShipListEntry* shipEntry;
			int category;
			int count;

			shipEntry = &g_shipList[shipIndex];
			category = shipEntry->category;
			if (category == 4) {
				continue;
			}

			if (category == 6) {
				return 1;
			}

			count = g_oobCraftCounts[shipEntry->typeId];
			if (count == 0) {
				continue;
			}

			if (entryIndex >= linesPerPage * pageIndex && entryIndex < linesPerPage * (pageIndex + 1)) {
				sprintf(g_frontendScratchBuffer, "%d %s", count, shipEntry->name);
				if (g_oobEntryCount > linesPerPage) {
					if (revealPhase >= 100) {
						FrontendText_DrawReveal(
							12, g_frontendScratchBuffer, textRect.left, textRect.top, g_colorLightBlue,
							(int)strlen(g_frontendScratchBuffer) - savedRevealPhase + 100);
					} else {
						FrontendText_DrawReveal(12, g_frontendScratchBuffer, textRect.left, textRect.top,
												g_colorLightBlue, revealPhase);
					}
				} else {
					FrontendText_DrawReveal(12, g_frontendScratchBuffer, textRect.left, textRect.top,
											g_colorLightBlue, revealCount);
				}
				FrontendDraw_RectOffsetXY(&textRect, 0, 15);
			}
			++entryIndex;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x578F60
int BriefingRoom_ComputeOrderOfBattle(void) {
	int flightGroupIndex;
	int typeId;
	int flightGroupCount;

	for (typeId = 0; typeId < BRIEFING_ROOM_OOB_CRAFT_TYPE_COUNT; ++typeId) {
		g_oobCraftCounts[typeId] = 0;
	}

	flightGroupCount = (int16_t)g_frontendMission->flightGroupCount;
	for (flightGroupIndex = 0; flightGroupIndex < flightGroupCount; ++flightGroupIndex) {
		XwaFlightGroup* flightGroup;
		ShipListEntry* shipEntry;
		int category;

		flightGroup = &g_frontendMission->flightGroups[flightGroupIndex];
		if (flightGroup->team != g_pilotData.team || flightGroup->arrival[0].triggers[0].condition != 0 ||
			flightGroup->arrival[0].triggers[0].variableType != 1 ||
			flightGroup->arrival[0].triggers[0].variable != 0 ||
			flightGroup->arrival[0].triggers[1].condition != 0 ||
			flightGroup->arrival[0].triggers[1].variableType != 1 ||
			flightGroup->arrival[0].triggers[1].variable != 0 || flightGroup->arrival[0].t1OrT2 != 0) {
			continue;
		}

		shipEntry = &g_shipList[g_shipTypeToShipListIndex[flightGroup->craftType]];
		category = shipEntry->category;
		if (category == 1 || (category < 6 && category != 4 && !shipEntry->flyable)) {
			g_oobCraftCounts[flightGroup->craftType] +=
				(flightGroup->numberOfWaves + 1) * flightGroup->numberOfCraft;
		}
	}

	g_oobEntryCount = 0;
	g_oobMaxWidth = 0;
	for (typeId = 0; typeId < BRIEFING_ROOM_OOB_CRAFT_TYPE_COUNT; ++typeId) {
		int count;

		count = g_oobCraftCounts[typeId];
		if (count != 0) {
			int width;

			sprintf(g_frontendScratchBuffer, "%d %s", count,
					g_shipList[g_shipTypeToShipListIndex[typeId]].name);
			width = (int)strlen(g_frontendScratchBuffer);
			if (width > g_oobMaxWidth) {
				g_oobMaxWidth = width;
			}
			++g_oobEntryCount;
		}
	}

	for (flightGroupIndex = 0; flightGroupIndex < flightGroupCount; ++flightGroupIndex) {
		XwaFlightGroup* flightGroup;
		ShipListEntry* shipEntry;
		int category;

		flightGroup = &g_frontendMission->flightGroups[flightGroupIndex];
		if (flightGroup->team != g_pilotData.team || flightGroup->arrival[0].triggers[0].condition != 0 ||
			flightGroup->arrival[0].triggers[0].variableType != 1 ||
			flightGroup->arrival[0].triggers[1].condition != 0 ||
			flightGroup->arrival[0].triggers[1].variableType != 1 || flightGroup->arrival[0].t1OrT2 != 0) {
			continue;
		}

		shipEntry = &g_shipList[g_shipTypeToShipListIndex[flightGroup->craftType]];
		category = shipEntry->category;
		if (category == 6) {
			int width;

			++g_oobEntryCount;
			sprintf(g_frontendScratchBuffer, "%s %s",
					g_shipList[g_shipTypeToShipListIndex[flightGroup->craftType]].name, flightGroup->name);
			width = (int)strlen(g_frontendScratchBuffer);
			if (width > g_oobMaxWidth) {
				g_oobMaxWidth = width;
			}
		} else if (category < 6 && category != 4 && category != 1 && shipEntry->flyable) {
			int width;

			++g_oobEntryCount;
			sprintf(g_frontendScratchBuffer, "%s %s",
					g_shipList[g_shipTypeToShipListIndex[flightGroup->craftType]].name, flightGroup->name);
			width = (int)strlen(g_frontendScratchBuffer);
			if (width > g_oobMaxWidth) {
				g_oobMaxWidth = width;
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x5775E0
int BriefingRoom_Update(int frameCounter) {
	FrontendRect rect;
	FrontendRect out;
	FrontendRect rightBarRect;
	FrontendRect savedClip;
	/* The original falls through with the stack buffer unchanged for invalid rank/officer cases. */
	char rankBuffer[20];
	int state;

	if (frameCounter == 0) {
		int prepareOk;

#ifndef XWA_MODERN
		int currentDiskMatches;

		currentDiskMatches = 0;
		if ((unsigned int)g_currentMissionId >= (unsigned int)g_diskId) {
			if (g_currentCdDisk == 1) {
				currentDiskMatches = 1;
			}
		} else if (g_currentCdDisk == 0) {
			currentDiskMatches = 1;
		}
		if (!currentDiskMatches) {
			for (;;) {
				int detected;

				detected = 0;
				if ((unsigned int)g_currentMissionId >= (unsigned int)g_diskId) {
					if (File_CheckGameCdPresent(1)) {
						g_currentCdDisk = 1;
						detected = 1;
					}
				} else if (File_CheckGameCdPresent(0)) {
					g_currentCdDisk = 0;
					detected = 1;
				}

				if (detected) {
					break;
				}

				if ((unsigned int)g_currentMissionId >= (unsigned int)g_diskId) {
					prepareOk = FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_FAILED_TO_DETECT2_1),
																 FrontendString_Get(STR_FAILED_TO_DETECT2_2),
																 NULL, FrontendString_Get(STR_OKAY),
																 FrontendString_Get(STR_CANCEL));
				} else {
					prepareOk = FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_FAILED_TO_DETECT1_1),
																 FrontendString_Get(STR_FAILED_TO_DETECT1_2),
																 NULL, FrontendString_Get(STR_OKAY),
																 FrontendString_Get(STR_CANCEL));
				}
				if (!prepareOk) {
					FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
					return 0;
				}
			}
		}
#endif

		if ((unsigned int)g_currentMissionId > 0x31u && (unsigned int)g_currentMissionId <= 0x34u) {
			BriefingRoom_SetupCurrentMissionSession();
			FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
			return 0;
		}

		FrontendDisplay_SwitchDriver(0);
		FrontendColor_Init();
		g_frontendLeftBarAnimState = 0;
		FrontImage_SetSpriteFrame("leftbar2", 0);
		g_frontendRightBarAnimState = 3;
		FrontImage_SetSpriteFrame("rightbar1", 0);
		FrontImage_SetSpriteFrame("rightbar2", 0);
		FrontImage_SetSpriteFrame("rightbar3", 0);

		g_briefingRoomState = BRIEFING_ROOM_START_NARRATION;
		if (g_pilotData.tacOfficerAnnounceNewRank) {
			g_briefingRoomState = BRIEFING_ROOM_START_RANK_ANNOUNCEMENT;
		}
		g_unusedBriefingRoomInitState = 0;
		g_frontendFirstVisibleLine = 0;
		FrontendText_SetGlyphGradientBg(g_colorNearBlack);

		if (g_frontendMission->header.missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
			FrontImage_RegisterResourceDefault("frontres\\family\\markostart.bmp", "briefstart");
			g_briefingRoomState = BRIEFING_ROOM_PLAY_ROOM_MOVIE;
#ifdef XWA_MODERN
			Music_SetDatapadState(MUSIC_STATE_FRONTEND_1210);
#else
			musicState = MUSIC_STATE_FRONTEND_1210;
			if (g_gameConfig.datapadMusicEnabled) {
				Music_SetState(MUSIC_STATE_FRONTEND_1210);
				Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
			} else {
				Music_Stop();
			}
#endif
		} else {
			FrontImage_RegisterResourceDefault("frontres\\combat\\briefoff.bmp", "briefstart");
		}

		if (g_briefingText != NULL) {
			Mem_Free(g_briefingText);
			g_briefingText = NULL;
		}
		g_briefingText = (char*)Mem_Alloc(0x1000u);
		FrontImage_DrawSpriteOpaque("briefstart", 0, 0);
		FrontendDisplay_LockOffscreenSurface();
		FrontImage_DrawSpriteOpaque("briefstart", 0, 0);
		FrontendDisplay_UnlockOffscreenSurface(1);
		FrontendText_ResetGlyphScratchBuffer(20);

		prepareOk = BriefingRoom_PrepareTourBriefing();
		sprintf(g_frontendScratchBuffer, "frontres\\tour\\squad%d.flc",
				g_frontendMission->header.briefingLogo - 3);
		if (g_frontendMission->header.briefingLogo < 3u) {
			sprintf(g_frontendScratchBuffer, "frontres\\tour\\squad1.flc");
			g_briefingRoomSquadLogoX = 71;
			g_briefingRoomSquadLogoY = 140;
		} else {
			unsigned int logoIndex;

			logoIndex = g_frontendMission->header.briefingLogo;
			logoIndex += -3;
			if (logoIndex == 1) {
				g_briefingRoomSquadLogoX = 71;
				g_briefingRoomSquadLogoY = 140;
			} else if (logoIndex == 2) {
				g_briefingRoomSquadLogoX = 71;
				g_briefingRoomSquadLogoY = 157;
			} else if (logoIndex == 3) {
				g_briefingRoomSquadLogoX = 70;
				g_briefingRoomSquadLogoY = 155;
			} else {
				g_briefingRoomSquadLogoX = 71;
				g_briefingRoomSquadLogoY = 157;
			}
		}

		FrontImage_RegisterResourceDefault(g_frontendScratchBuffer, "squadlogo");
		FrontImage_SetSpriteFrame("squadlogo", 0);
		FrontendCursor_Hide();
		if (!prepareOk) {
			g_briefingRoomState = BRIEFING_ROOM_EXIT_TO_CONCOURSE;
		}
	}

	state = g_briefingRoomState;
	switch (state) {
		case BRIEFING_ROOM_WAIT_NARRATION_OR_SKIP:
			if (Keyboard_BufferContains(27) || Keyboard_BufferContains(32) || Keyboard_BufferContains(13) ||
				Keyboard_BufferContains(8)) {
				Keyboard_FlushCharBuffer();
				g_briefingRoomState = BRIEFING_ROOM_START_BRIEFING_REVEAL;
				FrontImage_RegisterResourceDefault("frontres\\combat\\solo.bmp", "background");
				FrontImage_DrawSpriteOpaque("background", 0, 0);
				FrontendDisplay_LockOffscreenSurface();
				FrontImage_DrawSpriteOpaque("background", 0, 0);
				FrontendDisplay_UnlockOffscreenSurface(1);
			}
			if (!FrontendWaveStream_IsPlaying() || (unsigned int)g_briefingRoomStateFrameCounter > 0x18u) {
				g_briefingRoomState = BRIEFING_ROOM_PLAY_ROOM_MOVIE;
			}
			++g_briefingRoomStateFrameCounter;
			break;

		case BRIEFING_ROOM_PLAY_ROOM_MOVIE:
			FrontendDisplay_DisableOffscreenRestore();
			FrontendDisplay_UnlockBackBuffer();
#ifdef XWA_MODERN
			if (g_frontendMission->header.missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
				if (Movie_Play("marko", 1)) {
					g_briefingRoomState = BRIEFING_ROOM_WAIT_ROOM_MOVIE;
					break;
				}
			} else {
				if (Movie_Play("briefroom", 1)) {
					g_briefingRoomState = BRIEFING_ROOM_WAIT_ROOM_MOVIE;
					break;
				}
			}
			/* A missing movie follows the same post-playback path. */
			g_briefingRoomState = BRIEFING_ROOM_WAIT_ROOM_MOVIE;
			/* fall through */

		case BRIEFING_ROOM_WAIT_ROOM_MOVIE:
#else
			if (g_frontendMission->header.missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
				Movie_Play("marko", 1);
			} else {
				Movie_Play("briefroom", 1);
			}
#endif
			g_briefingRoomState = BRIEFING_ROOM_START_BRIEFING_REVEAL;
			g_briefingRoomStateFrameCounter = 0;
			g_briefingRoomRevealCount = 0;
			FrontendDisplay_EnableOffscreenRestore();
			if (g_frontendMission->header.missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
				FrontImage_RegisterResourceDefault("frontres\\family\\markoholo.bmp", "background");
			} else {
				FrontImage_RegisterResourceDefault("frontres\\combat\\solo.bmp", "background");
			}
			FrontImage_DrawSpriteOpaque("background", 0, 0);
			FrontendDisplay_LockOffscreenSurface();
			FrontImage_DrawSpriteOpaque("background", 0, 0);
			FrontendDisplay_UnlockOffscreenSurface(1);
			break;

		case BRIEFING_ROOM_START_BRIEFING_REVEAL:
			FrontendCursor_SetPos(g_frontendSidebarButtonRects[9].left + 10,
								  g_frontendSidebarButtonRects[9].top + 10);
			FrontendWaveStream_PlayWaveFile(g_pendingVoiceWav, 0, 0);
			g_briefingRoomState = BRIEFING_ROOM_REVEAL_BRIEFING_TEXT;
			FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			g_frontendRightBarAnimState = 0;
			g_briefingRoomStateFrameCounter = 0;
			g_briefingRoomRevealCount = 0;
			break;

		case BRIEFING_ROOM_REVEAL_BRIEFING_TEXT:
		case BRIEFING_ROOM_CLOSE_REVEAL_MASK:
			FrontendCursor_Show();
			FrontendDraw_RectAssign(&out, 158, 52, 491, 68);
			sprintf(g_frontendScratchBuffer, "%s", g_missionList[g_selectedMissionListIndex].description);
			{
				size_t index;

#ifdef XWA_MODERN
				index = strlen(g_frontendScratchBuffer);
				if (index > 1) {
					--index;
#else
				index = strlen(g_frontendScratchBuffer) - 1;
#endif
					while (index != 0) {
						if (g_frontendScratchBuffer[index] == '(') {
							g_frontendScratchBuffer[index] = '\0';
							break;
						}
						--index;
					}
#ifdef XWA_MODERN
				}
#endif
			}
			FrontendText_DrawCenteredReveal(12, g_frontendScratchBuffer, &out, g_colorLightBlue,
											g_briefingRoomRevealCount);

			if (g_briefingRoomState == BRIEFING_ROOM_CLOSE_REVEAL_MASK) {
				int closeBottom;

				FrontendDisplay_GetScreenClipRect(&savedClip);
				FrontendDraw_RectAssign(&rect, 0, 0, 510, 335);
				FrontendDraw_RectOffsetXY(&rect, 65, 60);
				closeBottom = g_briefingRoomStateFrameCounter;
				rect.top += 15;
				closeBottom = rect.bottom - 11 * closeBottom;
				if (closeBottom <= rect.top) {
					rect.bottom = (int16_t)rect.top;
					FrontendDisplay_SetScreenClipRect640x480(&rect);
					g_briefingRoomState = BRIEFING_ROOM_ENTER_MISSION_BRIEFING;
				} else {
					unsigned int lineCount;
					int lineSpan;

					rect.bottom = closeBottom;
					FrontendDraw_Line(rect.left + 5, rect.bottom, rect.right - 5, rect.bottom,
									  g_textShadeRamps[3][7]);
					lineCount = rand() % 5 + 7;
					lineSpan = rect.right - rect.left - 9;
					while (lineCount > 0) {
						int x;

						x = rand() % lineSpan + rect.left + 5;
						if (g_frontendMission->header.missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
							FrontendDraw_LineAntialiased(x, closeBottom, 616, 338,
														 g_textShadeRamps[3][rand() % 6]);
						} else {
							FrontendDraw_LineAntialiased(x, closeBottom, 320, 450,
														 g_textShadeRamps[3][rand() % 6]);
						}
						--lineCount;
					}
					FrontendDisplay_SetScreenClipRect640x480(&rect);
				}
			} else {
				FrontendDisplay_GetScreenClipRect(&savedClip);
				FrontendDraw_RectAssign(&rect, 0, 0, 510, 335);
				FrontendDraw_RectOffsetXY(&rect, 65, 60);
				FrontendDisplay_SetScreenClipRect640x480(&rect);
			}

			if (g_frontendMission->header.missionType != XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
				int logoCenterX;
				int oobLeft;
				int titleX;
				int titleY;
				int oobTop;

				FrontImage_GetResourceRect("squadlogo", &out);
				titleY = g_briefingRoomSquadLogoY;
				FrontImage_DrawSprite("squadlogo", g_briefingRoomSquadLogoX, titleY);
				FrontImage_AdvanceSpriteFrame("squadlogo", 1);
				logoCenterX = g_briefingRoomSquadLogoX + ((out.right - out.left + 1) >> 1);
				titleY -= 40;
				strcpy(g_frontendScratchBuffer,
					   FrontendString_Get((UIString)(g_frontendMission->header.briefingLogo + 727)));
				titleX = logoCenterX - (FrontendText_MeasureWidth(g_frontendScratchBuffer, 15) >> 1);
				if (g_briefingRoomState == BRIEFING_ROOM_REVEAL_BRIEFING_TEXT &&
					(unsigned int)g_briefingRoomRevealCount < strlen(g_frontendScratchBuffer)) {
					g_frontendScratchBuffer[g_briefingRoomRevealCount + 1] =
						g_frontendScratchBuffer[g_briefingRoomRevealCount];
					g_frontendScratchBuffer[g_briefingRoomRevealCount + 2] = '\0';
					g_frontendScratchBuffer[g_briefingRoomRevealCount] = 5;
				}
				FrontendText_Draw(15, g_frontendScratchBuffer, titleX, titleY, g_colorOrangeRed);
				oobLeft = out.right - out.left + g_briefingRoomSquadLogoX + 20;
				if (g_oobEntryCount > 20) {
					oobTop = 90;
				} else {
					oobTop = 15 * (((20 - g_oobEntryCount) >> 1) + 6);
				}
				FrontendDraw_RectAssign(&out, oobLeft, oobTop, 570, 420);
				BriefingRoom_DrawOrderOfBattle(&out, g_briefingRoomRevealCount);
			} else {
				FrontImage_GetResourceRect("squadlogo", &out);
				FrontImage_DrawSprite("squadlogo", 254, 155);
				FrontendDraw_RectAssign(&out, 70, 90, 570, 165);
				BriefingRoom_DrawOrderOfBattle(&out, g_briefingRoomRevealCount);
				BriefingRoom_DrawMissionBriefingTextReveal(g_briefingRoomRevealCount);
			}

			FrontendDisplay_SetScreenClipRect640x480(&savedClip);
			if (g_briefingRoomState == BRIEFING_ROOM_REVEAL_BRIEFING_TEXT) {
				if ((unsigned int)g_briefingRoomStateFrameCounter > 0xc8u &&
					!FrontendWaveStream_IsPlaying()) {
					g_pendingVoiceWav[strlen(g_pendingVoiceWav) - 5] =
						(char)(g_pendingVoiceWav[strlen(g_pendingVoiceWav) - 5] + 1);
					if (!FrontendWaveStream_PlayWaveFile(g_pendingVoiceWav, 0, 0)) {
						g_briefingRoomState = BRIEFING_ROOM_CLOSE_REVEAL_MASK;
						g_briefingRoomStateFrameCounter = 0;
						FrontendWaveStream_Shutdown();
					}
				}

				FrontImage_GetResourceRect("rightbar1", &rightBarRect);
				if (g_frontendRightBarAnimState == 0) {
					FrontImage_DrawSprite("rightbar1", rightBarRect.left - rightBarRect.right + 639,
										  rightBarRect.top - rightBarRect.bottom + 479);
					FrontImage_AdvanceSpriteFrame("rightbar1", 1);
					if (FrontImage_GetSpriteFrame("rightbar1") == 9) {
						g_frontendRightBarAnimState = 1;
					}
				} else if (g_frontendRightBarAnimState == 1) {
					FrontImage_DrawSprite("rightbar1", rightBarRect.left - rightBarRect.right + 639,
										  rightBarRect.top - rightBarRect.bottom + 479);
					FrontendDraw_RectCopy(&out, &g_frontendSidebarButtonRects[9]);
					if (FrontendButton_DrawSpriteWithHoverText(&out, (char*)"briefing", (char*)"briefing",
															   (char*)FrontendString_Get(STR_GO_TO_BRIEFING),
															   g_colorPaleBlue, g_colorLightBlue, 240,
															   (char*)"jewelsound")) {
						g_frontendRightBarAnimState = 2;
						FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
												  12 * g_gameConfig.sfxDatapadVolume, 63);
					}
				} else if (g_frontendRightBarAnimState == 2) {
					FrontImage_DrawSprite("rightbar1", rightBarRect.left - rightBarRect.right + 639,
										  rightBarRect.top - rightBarRect.bottom + 479);
					FrontImage_AdvanceSpriteFrame("rightbar1", 1);
					if (!FrontImage_GetSpriteFrame("rightbar1")) {
						g_briefingRoomState = BRIEFING_ROOM_ENTER_MISSION_BRIEFING;
						g_frontendRightBarAnimState = 3;
					}
				}
			}

			++g_briefingRoomStateFrameCounter;
			++g_briefingRoomRevealCount;
			if (Keyboard_BufferContains(27) || Keyboard_BufferContains(32) || Keyboard_BufferContains(13) ||
				Keyboard_BufferContains(8)) {
				Keyboard_FlushCharBuffer();
				g_briefingRoomState = BRIEFING_ROOM_ENTER_MISSION_BRIEFING;
			}
			break;

		case BRIEFING_ROOM_ENTER_MISSION_BRIEFING:
			g_pilotData.campaignMode = 1;
			FrontendScreen_SetCallbacks(MissionBriefing_Update, MissionBriefing_Exit);
			break;

		case BRIEFING_ROOM_START_NARRATION:
			FrontendWaveStream_PlayWaveFile(g_briefingNarrationWav, 0, 0);
			g_briefingRoomState = BRIEFING_ROOM_WAIT_NARRATION_OR_SKIP;
			g_briefingRoomStateFrameCounter = 0;
#ifdef XWA_MODERN
			Music_SetDatapadState(MUSIC_STATE_FRONTEND_1230);
#else
			musicState = MUSIC_STATE_FRONTEND_1230;
			if (g_gameConfig.datapadMusicEnabled) {
				Music_SetState(MUSIC_STATE_FRONTEND_1230);
				Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
			} else {
				Music_Stop();
			}
#endif
			break;

		case BRIEFING_ROOM_START_RANK_ANNOUNCEMENT:
			if (g_pilotData.tacOfficerAnnounceNewRank != 1) {
				if (g_frontendMission->header.briefingOfficer == 0) {
					sprintf(rankBuffer, "N01DE%d.wav", g_pilotData.tacOfficerAnnounceNewRank + 27);
				} else if (g_frontendMission->header.briefingOfficer == 1) {
					sprintf(rankBuffer, "N01KU%d.wav", g_pilotData.tacOfficerAnnounceNewRank + 27);
				} else if (g_frontendMission->header.briefingOfficer == 2) {
					sprintf(rankBuffer, "N01ZL%d.wav", g_pilotData.tacOfficerAnnounceNewRank + 27);
				}
			}
			sprintf(g_frontendScratchBuffer, "wave\\frontend\\%s", rankBuffer);
			FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
#ifdef XWA_MODERN
			Music_SetDatapadState(MUSIC_STATE_FRONTEND_1230);
#else
			musicState = MUSIC_STATE_FRONTEND_1230;
			if (g_gameConfig.datapadMusicEnabled) {
				Music_SetState(MUSIC_STATE_FRONTEND_1230);
				Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
			} else {
				Music_Stop();
			}
#endif
			g_briefingRoomState = BRIEFING_ROOM_WAIT_RANK_ANNOUNCEMENT;
			g_pilotData.emkayAnnounceNewRank = g_pilotData.tacOfficerAnnounceNewRank;
			g_pilotData.tacOfficerAnnounceNewRank = 0;
			g_pilotData.familyNewMedal = 0;
			break;

		case BRIEFING_ROOM_WAIT_RANK_ANNOUNCEMENT:
			if (Keyboard_BufferContains(27) || Keyboard_BufferContains(32) || Keyboard_BufferContains(13) ||
				Keyboard_BufferContains(8)) {
				Keyboard_FlushCharBuffer();
				g_briefingRoomState = BRIEFING_ROOM_START_NARRATION;
				FrontImage_RegisterResourceDefault("frontres\\combat\\solo.bmp", "background");
				FrontImage_DrawSpriteOpaque("background", 0, 0);
				FrontendDisplay_LockOffscreenSurface();
				FrontImage_DrawSpriteOpaque("background", 0, 0);
				FrontendDisplay_UnlockOffscreenSurface(1);
			}
			if (!FrontendWaveStream_IsPlaying()) {
				g_briefingRoomStateFrameCounter = 0;
				g_briefingRoomState = BRIEFING_ROOM_DELAY_OFFICER_FOLLOWUP;
			}
			break;

		case BRIEFING_ROOM_DELAY_OFFICER_FOLLOWUP: {
			int nextFrame;

			nextFrame = g_briefingRoomStateFrameCounter;
			if ((unsigned int)g_briefingRoomStateFrameCounter > 0x18u) {
				if (g_frontendMission->header.briefingOfficer == 0) {
					sprintf(rankBuffer, "N01DE%d.wav", rand() % 2 + 36);
				} else if (g_frontendMission->header.briefingOfficer == 1) {
					sprintf(rankBuffer, "N01KU%d.wav", rand() % 2 + 36);
				} else if (g_frontendMission->header.briefingOfficer == 2) {
					sprintf(rankBuffer, "N01ZL%d.wav", rand() % 2 + 36);
				}
				sprintf(g_frontendScratchBuffer, "wave\\frontend\\%s", rankBuffer);
				FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
				g_briefingRoomState = BRIEFING_ROOM_WAIT_OFFICER_FOLLOWUP;
				nextFrame = 0;
			}
			g_briefingRoomStateFrameCounter = nextFrame + 1;
			break;
		}

		case BRIEFING_ROOM_WAIT_OFFICER_FOLLOWUP:
			if (Keyboard_BufferContains(27) || Keyboard_BufferContains(32) || Keyboard_BufferContains(13) ||
				Keyboard_BufferContains(8)) {
				Keyboard_FlushCharBuffer();
				g_briefingRoomState = BRIEFING_ROOM_START_NARRATION;
				FrontImage_RegisterResourceDefault("frontres\\combat\\solo.bmp", "background");
				FrontImage_DrawSpriteOpaque("background", 0, 0);
				FrontendDisplay_LockOffscreenSurface();
				FrontImage_DrawSpriteOpaque("background", 0, 0);
				FrontendDisplay_UnlockOffscreenSurface(1);
			}
			if (!FrontendWaveStream_IsPlaying()) {
				g_briefingRoomState = BRIEFING_ROOM_START_NARRATION;
			}
			break;

		default:
			g_pilotData.campaignMode = 0;
			FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
			break;
	}

	{
		int previousDevice;

		previousDevice = g_gameConfig.threedDevice[0];
		if (Frontend_HandleEscapeQuit(0) == 1) {
			FrontendDisplay_RestorePrimaryDriver();
			FrontendColor_Init();
			return 1;
		}
		if (g_gameConfig.threedDevice[0] != previousDevice) {
			ModelPreview_FreeTexture237();
			ModelPreview_FreeResources();
			FrontendDisplay_SwitchDriver(0);
			FrontendColor_Init();
			FrontendDisplay_LockOffscreenSurface();
			FrontImage_DrawSpriteOpaque("background", 0, 0);
			FrontendDisplay_UnlockOffscreenSurface(1);
			FrontendDisplay_BlitOffscreenToFront();
		}
	}
	return 0;
}

// FUNCTION: XWA 0x577560
int BriefingRoom_Exit(int frameCounter) {
	(void)frameCounter;

	FrontImage_FreeResourceByName("background");
	FrontImage_FreeResourceByName("briefstart");
	FrontImage_FreeResourceByName("squadlogo");
	Frontend_ResetScrollableControls();
	FrontendWaveStream_Shutdown();
	FrontendCursor_Show();
	if (g_briefingText != NULL) {
		Mem_Free(g_briefingText);
		g_briefingText = NULL;
	}
	if (g_missionList != NULL) {
		Mem_Free(g_missionList);
		g_missionList = NULL;
	}
	FrontendMouse_ClearInputGate();
	return 0;
}
