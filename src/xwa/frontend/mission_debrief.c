#include "xwa/frontend/mission_debrief.h"

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/linez.h"
#include "xwa/assets/ship_list.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/briefing_room.h"
#include "xwa/frontend/briefing_script.h"
#include "xwa/frontend/concourse.h"
#include "xwa/frontend/cutscene.h"
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
#include "xwa/frontend/frontend_medals.h"
#include "xwa/frontend/frontend_mission.h"
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
#include "xwa/movie/movie.h"
#include "xwa/frontend/net_transport.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0x6033B0
const char g_frontendNoValueText[] = "----";

// GLOBAL: XWA 0x9EAA04
int g_missionOutcome;

// GLOBAL: XWA 0x9EAA0C
char* g_hintsText;

// GLOBAL: XWA 0x9EAA10
int g_debriefMissionHintsScrollOffset;

// GLOBAL: XWA 0x784B24
int g_debriefTextRevealFrame;

// GLOBAL: XWA 0x784B20
int g_awardCeremonyLabelFillColor;

// GLOBAL: XWA 0x784B58
int g_awardCeremonyState;

// GLOBAL: XWA 0x784C10
int g_awardCeremonyFrameCounter;

// GLOBAL: XWA 0x784AC8
int g_debriefTotalKillsSharedByMissionType[4];

// GLOBAL: XWA 0x784AD8
int g_debriefTeamUseSummaryRows[10];

// GLOBAL: XWA 0x784B04
int g_debriefLocalTeamRankIndex;

// GLOBAL: XWA 0x784B08
int g_debriefActiveTeamCount;

// GLOBAL: XWA 0x784B30
int g_debriefSortedTeamIds[10];

// GLOBAL: XWA 0x784B80
int g_debriefTeamHasPlayer[10];

// GLOBAL: XWA 0x784BA8
int g_debriefSortedPlayerIds[8];

// GLOBAL: XWA 0x784BD8
int g_debriefKillsOnRankIds[8];

// GLOBAL: XWA 0x784C18
int g_debriefKillsFromRankIds[8];

// GLOBAL: XWA 0x784C44
int g_debriefRankByPilot;

// GLOBAL: XWA 0x784BF8
int g_briefingTab;

// GLOBAL: XWA 0x784B00
int g_debriefAction;

// GLOBAL: XWA 0x784BCC
int g_pendingAward;

// GLOBAL: XWA 0x784C6C
int g_debriefDisconnectedFromNetGame;

// GLOBAL: XWA 0x784BD4
int g_debriefSkipMissionConfirmPending;

// GLOBAL: XWA 0x784B28
int g_debriefStatsPageNeedsRebuild;

// GLOBAL: XWA 0x784B60
int g_debriefPlayerKillsByMissionType[4];

// GLOBAL: XWA 0x784B70
int g_debriefNonPlayerKillsByMissionType[4];

// GLOBAL: XWA 0x784BC8
int g_debriefHasPlayerKillsByRating;

// GLOBAL: XWA 0x784BD0
int g_debriefHasCraftKillsByTypeSection;

// GLOBAL: XWA 0x784BFC
int g_debriefHasLossesFromPlayersSection;

// GLOBAL: XWA 0x784C00
int g_debriefNonPlayerKillsSharedTotal;

// GLOBAL: XWA 0x784C0C
int g_debriefCraftKillRowHasData;

// GLOBAL: XWA 0x784C38
int g_debriefPlayerKillsSharedTotal;

// GLOBAL: XWA 0x784C48
int g_debriefLossesToNonPlayerPilotsTotal;

// GLOBAL: XWA 0x784C58
int g_debriefLossesToPlayerPilotsTotal;

// GLOBAL: XWA 0x784C64
int g_debriefStatsScrollOffset;

// GLOBAL: XWA 0x784C68
int g_debriefStatsTotalRows;

// GLOBAL: XWA 0x9EAA08
int g_debriefMissionOverviewRowCount;

// GLOBAL: XWA 0x784B0C
int g_debriefMissionOverviewScrollOffset;

// GLOBAL: XWA 0x784B10
int g_debriefAssistTotalByMissionType[4];

// GLOBAL: XWA 0xABC96C (defined in flight.c): which game CD is in the drive.
extern int g_currentCdDisk;
// GLOBAL: XWA 0xABD7E0 (defined in frontend_flight.c): pre-launch pilot snapshot.
extern PilotData g_pilotDataSnapshot;

#ifdef XWA_MODERN
static int g_debriefDeathStarTourOutcomePending;
static int g_debriefPreCutscenePending;
static int g_debriefCutsceneSequenceActive;
static int g_debriefCutsceneSequencePhase;
static int g_debriefCutsceneSequenceEndPhase;

static int MissionDebrief_ContinueCutsceneSequence(int firstPhase, int endPhase) {
	if (!g_debriefCutsceneSequenceActive) {
		g_debriefCutsceneSequenceActive = 1;
		g_debriefCutsceneSequencePhase = firstPhase;
		g_debriefCutsceneSequenceEndPhase = endPhase;
	}
	while (g_debriefCutsceneSequencePhase <= g_debriefCutsceneSequenceEndPhase) {
		if (Cutscene_PlayForCurrentMissionPhase(g_debriefCutsceneSequencePhase) < 0) {
			return 0;
		}
		++g_debriefCutsceneSequencePhase;
	}
	g_debriefCutsceneSequenceActive = 0;
	return 1;
}

static int MissionDebrief_HandleDeathStarTourOutcomeResult(int deathStarResult) {
	unsigned int i;
	int saved;

	if (deathStarResult < 0) {
		g_debriefDeathStarTourOutcomePending = 1;
		return 1;
	}

	g_debriefDeathStarTourOutcomePending = 0;
	if (deathStarResult == 0) {
		FrontendCursor_Show();
		FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
		return 1;
	}
	if (deathStarResult == 1) {
		g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] = g_currentMissionId + 1;
		Concourse_LoadNextTourMission();
		MissionSetup_CountActiveTeams();
		MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
		for (i = 0; i < 16; ++i) {
			if (g_combatSimSlots[i].craftType) {
				g_combatSimSlots[i].ownerPlayerId = g_mpRoster[0].playerId;
			}
		}
		g_pilotData.team = 0;
		g_skipFrontendEntryMovie = 1;
		FrontendMission_InitPlayerState();
		FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
		return 1;
	}
	if (deathStarResult == 2) {
		g_pilotData.gameMode = g_frontendMissionSessionMode;
		g_pilotData.unk1 = 1;
		g_pilotData.numHumanPlayersLastMission = 1;
		memset(g_pilotData.killsFullOnPlayer, 0, sizeof(g_pilotData.killsFullOnPlayer));
		memset(g_pilotData.killsSharedOnPlayer, 0, sizeof(g_pilotData.killsSharedOnPlayer));
		memset(g_pilotData.killsFullOnFlightGroup, 0, sizeof(g_pilotData.killsFullOnFlightGroup));
		memset(g_pilotData.killsSharedOnFlightGroup, 0, sizeof(g_pilotData.killsSharedOnFlightGroup));
		memset(g_pilotData.killsFullFromPlayer, 0, sizeof(g_pilotData.killsFullFromPlayer));
		memset(g_pilotData.killsSharedFromPlayer, 0, sizeof(g_pilotData.killsSharedFromPlayer));
		memset(g_pilotData.killsFullFromFlightGroup, 0, sizeof(g_pilotData.killsFullFromFlightGroup));
		memset(g_pilotData.killsSharedFromFlightGroup, 0, sizeof(g_pilotData.killsSharedFromFlightGroup));
		memset(&g_pilotData.objectStats, 0, sizeof(g_pilotData.objectStats));
		memset(g_pilotData.teamsStatistics, 0, sizeof(g_pilotData.teamsStatistics));
		for (i = 0; i < 8; ++i) {
			g_pilotData.networkPlayers[i].totalScore = 0;
			g_pilotData.networkPlayers[i].kills = 0;
			g_pilotData.networkPlayers[i].killsShared = 0;
			g_pilotData.networkPlayers[i].m38 = 0;
			g_pilotData.networkPlayers[i].killsAssist = 0;
			g_pilotData.networkPlayers[i].totalLosses = 0;
			g_pilotData.networkPlayers[i].m60 = 0;
		}
		g_frontendLeftBarAnimState = 2;
		g_frontendRightBarAnimState = 2;
		saved = g_pilotData.factionStatistics[1].stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29];
		memcpy(&g_pilotData, &g_pilotDataSnapshot, sizeof(g_pilotData));
		g_pilotData.factionStatistics[1].stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29] = saved;
		g_debriefAction = 4;
		FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
		return 1;
	}

	return 0;
}
#endif

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x57ECE0
// Per-frame update for the post-mission debrief screen. On the first frame it
// prepares the leaderboards (MissionDebrief_Prepare), loads the mission list,
// builds the results/hints text, selects the active tab, and loads the
// background; every frame it draws the active tab, the award ceremony, and the
// action buttons, and routes the exit (fly again, select mission, briefing
// room, skip mission) including the multiplayer host-abort / disconnect paths.
int MissionDebrief_Update(int frameCounter) {
	struct {
		char longName[16];
		FrontendRect rect;
		FrontendRect src;
		char numberBuffer[20];
	} scratch;
	int outX;
	int outY;
	unsigned int i;
	int cdDisk;
	int missionId;
	int award;
	int result;
	int saved;
	int titleScan;

#ifdef XWA_MODERN
	if (g_debriefDeathStarTourOutcomePending) {
		result = FrontendDialog_ShowDeathStarTourOutcome();
		if (MissionDebrief_HandleDeathStarTourOutcomeResult(result)) {
			return 0;
		}
	}
	if (g_debriefPreCutscenePending) {
		result = Cutscene_PlayForCurrentMissionPhase(0);
		if (result < 0) {
			return 0;
		}
		g_debriefPreCutscenePending = 0;
		goto mission_debrief_after_pre_cutscene;
	}
#endif

	if (frameCounter == 0) {
		result = 0;

		// ---- First frame: CD swap check (single-player tour only) ----
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
			g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
			cdDisk = (unsigned int)g_pilotData.missionDescriptionIds[4] >= (unsigned int)g_diskId;
			if (cdDisk != g_currentCdDisk) {
				while (1) {
					result = 0;
					if (File_CheckGameCdPresent(cdDisk)) {
						g_currentCdDisk = cdDisk;
						result = 1;
					}
					if (result) {
						break;
					}
					if (cdDisk) {
						result = FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_FAILED_TO_DETECT2_1),
																  FrontendString_Get(STR_FAILED_TO_DETECT2_2),
																  NULL, FrontendString_Get(STR_OKAY),
																  FrontendString_Get(STR_CANCEL));
					} else {
						result = FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_FAILED_TO_DETECT1_1),
																  FrontendString_Get(STR_FAILED_TO_DETECT1_2),
																  NULL, FrontendString_Get(STR_OKAY),
																  FrontendString_Get(STR_CANCEL));
					}
					if (!result) {
						FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
						return 0;
					}
				}
			}
		}

		// ---- First frame: reset screen state ----
		FrontendText_SetGlyphGradientBg(g_colorNearBlack);
		g_debriefMissionOverviewRowCount = 0;
		g_debriefMissionOverviewScrollOffset = 0;
		g_pendingVoiceWav[0] = 0;
		g_debriefAction = 0;
		g_frontendLeftBarAnimState = 0;
		FrontImage_SetSpriteFrame("leftbar1", 0);
		FrontImage_SetSpriteFrame("leftbar2", 0);
		g_frontendRightBarAnimState = 0;
		if (g_pilotData.campaignMode) {
			if (g_pilotData.skipMissionsRemaining &&
				g_frontendMission->header.hangar != XWA_HANGAR_FAMILYTRANSPORT &&
				!g_pilotData.teamsStatistics[g_pilotData.team].isMissionCompleted &&
				((unsigned int)g_currentMissionId < 0x31 || (unsigned int)g_currentMissionId > 0x34)) {
				g_frontendRightBarPanelIndex = 3;
			} else {
				g_frontendRightBarPanelIndex = 2;
			}
		} else if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && !Net_IsHost()) {
			g_frontendRightBarPanelIndex = 1;
		} else {
			g_frontendRightBarPanelIndex = 2;
		}
		FrontImage_SetSpriteFrame("rightbar1", 0);
		FrontImage_SetSpriteFrame("rightbar2", 0);
		FrontImage_SetSpriteFrame("rightbar3", 0);
		FrontImage_LoadResourceList("frontres\\medals\\medals.lst");
		g_frontendMissionInitClearedDword = 0;
		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
#ifdef XWA_MODERN
			DebugPrintf("");
#else
			DebugPrintf();
#endif
		}
		Keyboard_FlushCharBuffer();
		g_frontendChatTeamOnly = 0;
		g_debriefDisconnectedFromNetGame = 0;
		g_frontendFirstVisibleLine = 0;
		g_debriefMissionHintsScrollOffset = 0;

		// Clear ready flags for any players still marked in the roster; if the
		// local player is among them, enter the disconnected debrief path.
		for (i = 0; i < 8; ++i) {
			if (g_pilotData.networkPlayers[i].directPlayId && g_pilotData.networkPlayers[i].m60) {
				Net_ClearPlayerReadyFlagWithLockGuard(g_pilotData.networkPlayers[i].directPlayId);
				if (g_pilotData.networkPlayers[i].directPlayId == Net_GetLocalPlayerId()) {
					g_debriefDisconnectedFromNetGame = 1;
				}
			}
		}
		Net_ResetRosterToLocalPlayerWithLockGuard();
		if (g_pilotData.newPromotion &&
			g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			scratch.longName[0] = (char)(g_pilotData.pilotRating + 1);
			scratch.longName[1] = 0;
			Net_SetPlayerNameWithLockGuard(Net_GetLocalPlayerId(), scratch.longName, g_pilotData.name);
		}

		MissionDebrief_Prepare();
		MissionSetup_LoadMissionList((MissionDirectoryId)g_pilotData.missionDirectoryId);
		if (g_missionList) {
			g_selectedMissionListIndex = 0;
			while ((unsigned int)g_selectedMissionListIndex < (unsigned int)g_missionCount) {
				if (g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] ==
					g_missionList[g_selectedMissionListIndex].missionIdx) {
					break;
				}
				++g_selectedMissionListIndex;
			}
		}

		g_pendingAward = 0;
		if (g_missionOutcome == 1 && g_pilotData.campaignMode) {
			// Campaign win: play the pre-debrief cutscene, then either the Death
			// Star tour outcome dialog or the medal/award computation.
#ifdef XWA_MODERN
			result = Cutscene_PlayForCurrentMissionPhase(0);
			if (result < 0) {
				g_debriefPreCutscenePending = 1;
				return 0;
			}
		mission_debrief_after_pre_cutscene:
#else
			Cutscene_PlayForCurrentMissionPhase(0);
#endif
			missionId = g_currentMissionId;
			if (missionId == 49 || missionId == 50 || missionId == 51) {
				musicState = MUSIC_STATE_FRONTEND_1280;
				if (g_gameConfig.datapadMusicEnabled) {
					Music_SetState(MUSIC_STATE_FRONTEND_1280);
				} else {
					Music_Stop();
				}
				result = FrontendDialog_ShowDeathStarTourOutcome();
#ifdef XWA_MODERN
				if (MissionDebrief_HandleDeathStarTourOutcomeResult(result)) {
					return 0;
				}
#else
				if (result == 0) {
					FrontendCursor_Show();
					FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
					return 0;
				}
				if (result == 1) {
					g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] =
						g_currentMissionId + 1;
					Concourse_LoadNextTourMission();
					MissionSetup_CountActiveTeams();
					MissionSetup_RebuildCombatSimSlotsFromFrontendMission();
					for (i = 0; i < 16; ++i) {
						if (g_combatSimSlots[i].craftType) {
							g_combatSimSlots[i].ownerPlayerId = g_mpRoster[0].playerId;
						}
					}
					g_pilotData.team = 0;
					g_skipFrontendEntryMovie = 1;
					FrontendMission_InitPlayerState();
					FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
					return 0;
				}
				if (result == 2) {
					g_pilotData.gameMode = g_frontendMissionSessionMode;
					g_pilotData.unk1 = 1;
					g_pilotData.numHumanPlayersLastMission = 1;
					memset(g_pilotData.killsFullOnPlayer, 0, sizeof(g_pilotData.killsFullOnPlayer));
					memset(g_pilotData.killsSharedOnPlayer, 0, sizeof(g_pilotData.killsSharedOnPlayer));
					memset(g_pilotData.killsFullOnFlightGroup, 0, sizeof(g_pilotData.killsFullOnFlightGroup));
					memset(g_pilotData.killsSharedOnFlightGroup, 0,
						   sizeof(g_pilotData.killsSharedOnFlightGroup));
					memset(g_pilotData.killsFullFromPlayer, 0, sizeof(g_pilotData.killsFullFromPlayer));
					memset(g_pilotData.killsSharedFromPlayer, 0, sizeof(g_pilotData.killsSharedFromPlayer));
					memset(g_pilotData.killsFullFromFlightGroup, 0,
						   sizeof(g_pilotData.killsFullFromFlightGroup));
					memset(g_pilotData.killsSharedFromFlightGroup, 0,
						   sizeof(g_pilotData.killsSharedFromFlightGroup));
					memset(&g_pilotData.objectStats, 0, sizeof(g_pilotData.objectStats));
					memset(g_pilotData.teamsStatistics, 0, sizeof(g_pilotData.teamsStatistics));
					for (i = 0; i < 8; ++i) {
						g_pilotData.networkPlayers[i].totalScore = 0;
						g_pilotData.networkPlayers[i].kills = 0;
						g_pilotData.networkPlayers[i].killsShared = 0;
						g_pilotData.networkPlayers[i].m38 = 0;
						g_pilotData.networkPlayers[i].killsAssist = 0;
						g_pilotData.networkPlayers[i].totalLosses = 0;
						g_pilotData.networkPlayers[i].m60 = 0;
					}
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					saved = g_pilotData.factionStatistics[1]
								.stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29];
					memcpy(&g_pilotData, &g_pilotDataSnapshot, sizeof(g_pilotData));
					g_pilotData.factionStatistics[1]
						.stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29] = saved;
					g_debriefAction = 4;
					FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
					return 0;
				}
#endif
				missionId = g_currentMissionId;
			}

			// Medal/award: record the battle index whose medal matches this mission.
			if (g_medalCount) {
				award = g_pendingAward;
				for (i = 0; i < (unsigned int)g_medalCount; ++i) {
					if (missionId == g_medalValues[i]) {
						award = i + 1;
					}
				}
				g_pendingAward = award;
			} else {
				award = g_pendingAward;
			}
			if (g_pilotData.familyNewMedal) {
				g_pendingAward = award | 0x80000000;
			}
		}

		// ---- First frame common: music, cursor, tab, text, background ----
		if (g_missionOutcome != 0) {
			musicState = MUSIC_STATE_FRONTEND_1280;
			if (g_gameConfig.datapadMusicEnabled) {
				Music_SetState(MUSIC_STATE_FRONTEND_1280);
			} else {
				Music_Stop();
			}
			FrontendCursor_SetPos(
				g_frontendSidebarButtonRects[8].left +
					((g_frontendSidebarButtonRects[8].right - g_frontendSidebarButtonRects[8].left) >> 1),
				g_frontendSidebarButtonRects[8].top +
					((g_frontendSidebarButtonRects[8].bottom - g_frontendSidebarButtonRects[8].top) >> 1));
		} else {
			if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				musicState = MUSIC_STATE_FRONTEND_1270;
				if (g_gameConfig.datapadMusicEnabled) {
					Music_SetState(MUSIC_STATE_FRONTEND_1270);
				} else {
					Music_Stop();
				}
			} else {
				musicState = MUSIC_STATE_FRONTEND_1260;
				if (g_gameConfig.datapadMusicEnabled) {
					Music_SetState(MUSIC_STATE_FRONTEND_1260);
				} else {
					Music_Stop();
				}
			}
			FrontendCursor_SetPos(
				g_frontendSidebarButtonRects[9].left +
					((g_frontendSidebarButtonRects[9].right - g_frontendSidebarButtonRects[9].left) >> 1),
				g_frontendSidebarButtonRects[9].top +
					((g_frontendSidebarButtonRects[9].bottom - g_frontendSidebarButtonRects[9].top) >> 1));
		}

		g_debriefTextRevealFrame = 0;
		switch (g_pilotData.missionDirectoryId) {
			case 0:
			case 4:
			case 5:
				g_briefingTab = (g_missionOutcome == 1) + 1;
				break;
			case 1:
				if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
					g_frontendMission->header.hangar == XWA_HANGAR_JUNKYARD) {
					g_briefingTab = 1;
				} else {
					g_briefingTab = 0;
				}
				break;
			case 2:
			case 3:
				g_briefingTab = (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER);
				break;
			case 6:
				g_briefingTab = 1;
				break;
			default:
				break;
		}

		memset(g_mpRosterReadyFlags, 0, sizeof(g_mpRosterReadyFlags));
		g_debriefAssistTotalByMissionType[3] = 0;
		g_debriefStatsPageNeedsRebuild = 1;
		g_debriefSkipMissionConfirmPending = 0;
		if (g_briefingText) {
			Mem_Free(g_briefingText);
			g_briefingText = NULL;
		}
		g_briefingText = (char*)Mem_Alloc(0x1000);
		if (g_hintsText) {
			Mem_Free(g_hintsText);
			g_hintsText = NULL;
		}
		g_hintsText = (char*)Mem_Alloc(0x1000);
		if (g_missionOutcome == 2) {
			MissionDebrief_BuildText(g_briefingText, g_hintsText, 0);
		} else if (g_missionOutcome == 1) {
			MissionDebrief_BuildText(g_briefingText, g_hintsText, 1);
		} else {
			MissionDebrief_BuildText(g_briefingText, g_hintsText, 0);
		}
		MissionDebrief_MarkNetworkPlayersReady();
		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			for (i = 0; i < 8; ++i) {
				if (g_pilotData.networkPlayers[i].directPlayId == Net_GetLocalPlayerId()) {
					break;
				}
			}
		}

		if (g_pilotData.campaignMode) {
			if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				FrontImage_RegisterResourceDefault("frontres\\family\\markoholo.bmp", "background");
			} else {
				FrontImage_RegisterResourceDefault("frontres\\combat\\solo.bmp", "background");
			}
		} else {
			FrontImage_RegisterResourceDefault("frontres\\combat\\multiplayer.bmp", "background");
		}
		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			MissionSetup_BroadcastStatePacket(0);
		}
		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontendDisplay_LockOffscreenSurface();
		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontendDisplay_UnlockOffscreenSurface(1);

		if (!g_pendingAward) {
			FrontendText_ResetGlyphScratchBuffer(20);
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			if (g_frontendMission->header.hangar != XWA_HANGAR_QUICKSTART &&
				g_frontendMission->header.hangar != XWA_HANGAR_SKIRMISH) {
				char parsedSep;
				int battleNum;
				int missionNum;

				sscanf(g_missionList[g_selectedMissionListIndex].fileName, "%d%c%d%c%d", &result, &parsedSep,
					   &battleNum, &parsedSep, &missionNum);
				if (battleNum >= 0 && battleNum <= 99 && missionNum >= 0 && missionNum <= 99) {
					sprintf(scratch.numberBuffer, "%2d%2d%2d", battleNum, missionNum, 1);
					if (scratch.numberBuffer[0] == ' ') {
						scratch.numberBuffer[0] = '0';
					}
					if (scratch.numberBuffer[2] == ' ') {
						scratch.numberBuffer[2] = '0';
					}
					if (scratch.numberBuffer[4] == ' ') {
						scratch.numberBuffer[4] = '0';
					}
					if (g_missionOutcome == 1) {
						sprintf(g_frontendScratchBuffer, "wave\\frontend\\B%dM%d\\W%s.wav", battleNum,
								missionNum, scratch.numberBuffer);
					} else {
						sprintf(g_frontendScratchBuffer, "wave\\frontend\\B%dM%d\\L%s.wav", battleNum,
								missionNum, scratch.numberBuffer);
					}
					result = FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
					if (result) {
						strcpy(g_pendingVoiceWav, g_frontendScratchBuffer);
					} else {
						g_pendingVoiceWav[0] = 0;
					}
				}
			}
		}
		FrontendCursor_Show();
		// Falls through into the per-frame body with frameCounter == 0.
	}

	// ---- Per frame ----
	if (g_debriefDisconnectedFromNetGame && frameCounter < 2) {
		FrontendText_ResetGlyphScratch();
		FrontendDraw_RectAssign(&scratch.rect, 65, 60, 575, 420);
		FrontendText_DrawCentered(15, FrontendString_Get(STR_NETWORK_ERROR), &scratch.rect, g_colorLightBlue);
	}
	if (!(g_debriefDisconnectedFromNetGame && frameCounter < 2)) {
		if (g_pendingAward) {
			MissionDebrief_ShowAwardCeremony(frameCounter);
			return 0;
		}
		if (g_debriefAction) {
			MissionDebrief_DrawTabBar();
			if (g_frontendLeftBarAnimState == 3 && g_frontendRightBarAnimState == 3) {
				if (g_missionOutcome == 1 && g_pilotData.campaignMode) {
					FrontendWaveStream_Shutdown();
#ifdef XWA_MODERN
					if (!MissionDebrief_ContinueCutsceneSequence(1, 1)) {
						return 0;
					}
#else
					Cutscene_PlayForCurrentMissionPhase(1);
#endif
				}
				switch (g_debriefAction) {
					case 1:
						if (g_pilotData.campaignMode && g_missionOutcome != 1) {
							saved = g_pilotData.factionStatistics[1]
										.stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29];
							memcpy(&g_pilotData, &g_pilotDataSnapshot, sizeof(g_pilotData));
							g_pilotData.factionStatistics[1]
								.stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29] = saved;
						}
						FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
						return 0;
					case 4:
						if (g_pilotData.campaignMode) {
							saved = g_pilotData.factionStatistics[1]
										.stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29];
							memcpy(&g_pilotData, &g_pilotDataSnapshot, sizeof(g_pilotData));
							g_pilotData.factionStatistics[1]
								.stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29] = saved;
						}
						FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
						return 0;
					case 5:
						g_skipFrontendEntryMovie = 1;
						FrontendScreen_SetCallbacks(MissionSetup_Update, MissionSetup_Exit);
						return 0;
					case 6:
						Concourse_LoadNextTourMission();
						FrontendScreen_SetCallbacks(BriefingRoom_Update, BriefingRoom_Exit);
						return 0;
					case 7:
						if (FrontendWaveStream_IsPlaying()) {
							return 0;
						}
						saved = g_pilotData.factionStatistics[1]
									.stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29];
						memcpy(&g_pilotData, &g_pilotDataSnapshot, sizeof(g_pilotData));
						g_pilotData.factionStatistics[1]
							.stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29] = saved;
						--g_pilotData.skipMissionsRemaining;
						g_pilotData.tourOfDutyMissions[g_currentMissionId].completedCount = 2;
#ifdef XWA_MODERN
						if (!MissionDebrief_ContinueCutsceneSequence(0, 1)) {
							return 0;
						}
#else
						Cutscene_PlayForCurrentMissionPhase(0);
						Cutscene_PlayForCurrentMissionPhase(1);
#endif
						FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
						return 0;
					default:
						return 0;
				}
			}
			return 0;
		}

		// Mission title bar (trailing " (...)" is stripped).
		FrontendDraw_RectAssign(&scratch.rect, 65, 52, 575, 68);
		FrontendDisplay_GetScreenClipRect(&scratch.src);
		FrontendDisplay_SetScreenClipRect640x480(&scratch.rect);
		sprintf(g_frontendScratchBuffer, "%s", g_missionList[g_selectedMissionListIndex].description);
		titleScan = (int)strlen(g_frontendScratchBuffer) - 1;
		while (titleScan != 0) {
			if (g_frontendScratchBuffer[titleScan] == '(') {
				g_frontendScratchBuffer[titleScan] = 0;
				break;
			}
			--titleScan;
		}
		FrontendText_DrawCentered(12, g_frontendScratchBuffer, &scratch.rect, g_colorLightBlue);
		FrontendDisplay_SetScreenClipRect640x480(&scratch.src);

		// Drain queued network packets (multiplayer only).
		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			while (1) {
				result = FrontendNet_ProcessNetworkPackets();
				if (!result) {
					break;
				}
				if (result == 'F') {
					Net_ShutdownDirectPlaySession();
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					g_debriefAction = 1;
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
					MissionDebrief_DrawTabBar();
					return 0;
				} else if (result == 'A') {
#ifdef XWA_MODERN
					DebugPrintf("");
#else
					DebugPrintf();
#endif
				} else if (result == 'S') {
					return 0;
				} else if (result == 'I') {
					memset(g_pilotData.killsFullOnPlayer, 0, sizeof(g_pilotData.killsFullOnPlayer));
					memset(g_pilotData.killsSharedOnPlayer, 0, sizeof(g_pilotData.killsSharedOnPlayer));
					memset(g_pilotData.killsFullOnFlightGroup, 0, sizeof(g_pilotData.killsFullOnFlightGroup));
					memset(g_pilotData.killsSharedOnFlightGroup, 0,
						   sizeof(g_pilotData.killsSharedOnFlightGroup));
					memset(g_pilotData.killsFullFromPlayer, 0, sizeof(g_pilotData.killsFullFromPlayer));
					memset(g_pilotData.killsSharedFromPlayer, 0, sizeof(g_pilotData.killsSharedFromPlayer));
					memset(g_pilotData.killsFullFromFlightGroup, 0,
						   sizeof(g_pilotData.killsFullFromFlightGroup));
					memset(g_pilotData.killsSharedFromFlightGroup, 0,
						   sizeof(g_pilotData.killsSharedFromFlightGroup));
					memset(&g_pilotData.objectStats, 0, sizeof(g_pilotData.objectStats));
					memset(g_pilotData.teamsStatistics, 0, sizeof(g_pilotData.teamsStatistics));
					for (i = 0; i < 8; ++i) {
						g_pilotData.networkPlayers[i].totalScore = 0;
						g_pilotData.networkPlayers[i].kills = 0;
						g_pilotData.networkPlayers[i].killsShared = 0;
						g_pilotData.networkPlayers[i].m38 = 0;
						g_pilotData.networkPlayers[i].killsAssist = 0;
						g_pilotData.networkPlayers[i].totalLosses = 0;
						g_pilotData.networkPlayers[i].m60 = 0;
					}
					CombatSim_ClearDisconnectedSlotOwners();
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					g_debriefAction = 4;
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
					MissionDebrief_DrawTabBar();
					return 0;
				} else if (result == '\\') {
					g_debriefSkipMissionConfirmPending = 1;
				} else if (result == 'Q') {
					for (i = 0; i < 8; ++i) {
						if (g_mpRoster[i].playerId == g_frontendNetPacketSenderPlayerId) {
							g_mpRosterReadyFlags[i] = 0;
							break;
						}
					}
				} else if (result == 'f') {
					g_frontendQuickStartLaunchFlag = 0;
					g_frontendSinglePlayerFlightSessionActive = 0;
					g_missionSetupRosterAuthoritative = 0;
					MpRoster_CompactActiveEntries();
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					g_debriefAction = 5;
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
					MissionDebrief_DrawTabBar();
					return 0;
				}
			}
		}

		// Active tab page; advance the queued voice line when the wave finishes.
		switch (g_briefingTab) {
			case 0:
				MissionDebrief_DrawMissionOverviewPage(frameCounter);
				if (!FrontendWaveStream_IsPlaying() && g_pendingVoiceWav[0]) {
					g_pendingVoiceWav[strlen(g_pendingVoiceWav) - 5] =
						g_pendingVoiceWav[strlen(g_pendingVoiceWav) - 5] + 1;
					result = FrontendWaveStream_PlayWaveFile(g_pendingVoiceWav, 0, 0);
					if (!result) {
						g_pendingVoiceWav[0] = 0;
					}
				}
				break;
			case 1:
				MissionDebrief_DrawPlayerStatisticsPage();
				break;
			case 2:
				MissionDebrief_DrawMissionResultsPage();
				if (!FrontendWaveStream_IsPlaying() && g_pendingVoiceWav[0]) {
					g_pendingVoiceWav[strlen(g_pendingVoiceWav) - 5] =
						g_pendingVoiceWav[strlen(g_pendingVoiceWav) - 5] + 1;
					result = FrontendWaveStream_PlayWaveFile(g_pendingVoiceWav, 0, 0);
					if (!result) {
						g_pendingVoiceWav[0] = 0;
					}
				}
				break;
			case 3:
				MissionDebrief_DrawMissionHintsPage();
				if (!FrontendWaveStream_IsPlaying() && g_pendingVoiceWav[0]) {
					g_pendingVoiceWav[strlen(g_pendingVoiceWav) - 5] =
						g_pendingVoiceWav[strlen(g_pendingVoiceWav) - 5] + 1;
					result = FrontendWaveStream_PlayWaveFile(g_pendingVoiceWav, 0, 0);
					if (!result) {
						g_pendingVoiceWav[0] = 0;
					}
				}
				break;
			default:
				break;
		}
		MissionDebrief_DrawTabBar();
		FrontendCursor_GetPos(&outX, &outY);

		// "Fly Again" (host or single-player).
		FrontendDraw_RectCopy(&scratch.rect, &g_frontendSidebarButtonRects[9]);
		if ((Net_IsHost() || g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) &&
			g_frontendRightBarAnimState == 1) {
			result = FrontendButton_DrawSpriteWithHoverText(&scratch.rect, "begin", "begin",
															(void*)FrontendString_Get(STR_FLY_AGAIN),
															g_colorPaleBlue, g_colorLightBlue, 7, "flysound");
			if (result) {
				if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
					if (g_pilotData.campaignMode && g_missionOutcome) {
						result = FrontendDialog_ShowConfirmDialog(
							FrontendString_Get(STR_DEBRIEF_REFLY_CONFIRM1),
							FrontendString_Get(STR_DEBRIEF_REFLY_CONFIRM2),
							FrontendString_Get(STR_DEBRIEF_REFLY_CONFIRM3), FrontendString_Get(STR_OKAY),
							FrontendString_Get(STR_CANCEL));
					} else {
						result = 1;
					}
					if (result) {
						g_pilotData.gameMode = g_frontendMissionSessionMode;
						g_pilotData.unk1 = 1;
						g_pilotData.numHumanPlayersLastMission = 1;
						memset(g_pilotData.killsFullOnPlayer, 0, sizeof(g_pilotData.killsFullOnPlayer));
						memset(g_pilotData.killsSharedOnPlayer, 0, sizeof(g_pilotData.killsSharedOnPlayer));
						memset(g_pilotData.killsFullOnFlightGroup, 0,
							   sizeof(g_pilotData.killsFullOnFlightGroup));
						memset(g_pilotData.killsSharedOnFlightGroup, 0,
							   sizeof(g_pilotData.killsSharedOnFlightGroup));
						memset(g_pilotData.killsFullFromPlayer, 0, sizeof(g_pilotData.killsFullFromPlayer));
						memset(g_pilotData.killsSharedFromPlayer, 0,
							   sizeof(g_pilotData.killsSharedFromPlayer));
						memset(g_pilotData.killsFullFromFlightGroup, 0,
							   sizeof(g_pilotData.killsFullFromFlightGroup));
						memset(g_pilotData.killsSharedFromFlightGroup, 0,
							   sizeof(g_pilotData.killsSharedFromFlightGroup));
						memset(&g_pilotData.objectStats, 0, sizeof(g_pilotData.objectStats));
						memset(g_pilotData.teamsStatistics, 0, sizeof(g_pilotData.teamsStatistics));
						for (i = 0; i < 8; ++i) {
							g_pilotData.networkPlayers[i].totalScore = 0;
							g_pilotData.networkPlayers[i].kills = 0;
							g_pilotData.networkPlayers[i].killsShared = 0;
							g_pilotData.networkPlayers[i].m38 = 0;
							g_pilotData.networkPlayers[i].killsAssist = 0;
							g_pilotData.networkPlayers[i].totalLosses = 0;
							g_pilotData.networkPlayers[i].m60 = 0;
						}
						g_frontendLeftBarAnimState = 2;
						g_frontendRightBarAnimState = 2;
						g_debriefAction = 4;
						FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
												  63);
					}
				} else {
					MissionSetup_BroadcastStatePacket(2);
				}
			}
		}

		if (!Net_IsHost() && g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			// Network client: "Done" / "Disconnect From Game Session".
			FrontendDraw_RectCopy(&scratch.rect, &g_frontendSidebarButtonRects[9]);
			if (g_frontendRightBarAnimState == 1) {
				if (g_debriefDisconnectedFromNetGame) {
					result = FrontendButton_DrawSpriteWithHoverText(
						&scratch.rect, "back", "back", (void*)FrontendString_Get(STR_DONE), g_colorPaleBlue,
						g_colorLightBlue, 8, "buttonsound");
				} else {
					result = FrontendButton_DrawSpriteWithHoverText(
						&scratch.rect, "back", "back",
						(void*)FrontendString_Get(STR_DISCONNECT_FROM_GAME_SESSION), g_colorPaleBlue,
						g_colorLightBlue, 8, "buttonsound");
					if (result) {
						result = FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_GAME_IN_PROGRESS1),
																  FrontendString_Get(STR_GAME_IN_PROGRESS2),
																  FrontendString_Get(STR_GAME_IN_PROGRESS3),
																  FrontendString_Get(STR_OKAY),
																  FrontendString_Get(STR_CANCEL));
					}
				}
				if (result) {
					g_frontendNetPacketScratch.packetType = 71;
					Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 4);
					Net_ShutdownDirectPlaySession();
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					g_debriefAction = 1;
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				}
			}
		} else {
			// Host / single-player: "Back" / "Return To Select Mission".
			FrontendDraw_RectCopy(&scratch.rect, &g_frontendSidebarButtonRects[8]);
			if (g_frontendRightBarAnimState == 1) {
				if (g_pilotData.campaignMode) {
					if (!g_frontendMissionLoaded || (unsigned int)g_currentMissionId >= 7) {
						result = FrontendButton_DrawSpriteWithHoverText(
							&scratch.rect, "back", "back", (void*)FrontendString_Get(STR_BACK_TO_CONCOURSE),
							g_colorPaleBlue, g_colorLightBlue, 8, "buttonsound");
					} else if (g_currentMissionId == 6 &&
							   g_pilotData.teamsStatistics[g_pilotData.team].isMissionCompleted) {
						result = FrontendButton_DrawSpriteWithHoverText(
							&scratch.rect, "back", "back", (void*)FrontendString_Get(STR_BACK_TO_CONCOURSE),
							g_colorPaleBlue, g_colorLightBlue, 8, "buttonsound");
					} else {
						result = FrontendButton_DrawSpriteWithHoverText(
							&scratch.rect, "back", "back",
							(void*)FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT), g_colorPaleBlue,
							g_colorLightBlue, 8, "buttonsound");
					}
				} else {
					char hangar;

					if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						hangar = g_frontendMission->header.hangar;
						if (hangar == XWA_HANGAR_JUNKYARD) {
							result = FrontendButton_DrawSpriteWithHoverText(
								&scratch.rect, "back", "back",
								(void*)FrontendString_Get(STR_BACK_TO_CONCOURSE), g_colorPaleBlue,
								g_colorLightBlue, 8, "buttonsound");
						} else {
							result = FrontendButton_DrawSpriteWithHoverText(
								&scratch.rect, "back", "back",
								(void*)FrontendString_Get(STR_RETURN_SELECT_MISSION), g_colorPaleBlue,
								g_colorLightBlue, 8, "buttonsound");
						}
					} else {
						result = FrontendButton_DrawSpriteWithHoverText(
							&scratch.rect, "back", "back",
							(void*)FrontendString_Get(STR_RETURN_SELECT_MISSION), g_colorPaleBlue,
							g_colorLightBlue, 8, "buttonsound");
					}
				}
				if (result) {
					if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
						g_frontendQuickStartLaunchFlag = 0;
						g_frontendSinglePlayerFlightSessionActive = 0;
						g_missionSetupRosterAuthoritative = 0;
						g_frontendLeftBarAnimState = 2;
						g_frontendRightBarAnimState = 2;
						if (g_pilotData.campaignMode) {
							g_debriefAction = 1;
						} else {
							g_debriefAction =
								(g_frontendMission->header.hangar != XWA_HANGAR_JUNKYARD) ? 5 : 1;
						}
						FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
												  63);
					} else {
						g_frontendNetPacketScratch.packetType = 102;
						Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 4);
					}
				}
			}
		}

		// "Leave Of Absence" (skip mission, panel 3 only).
		if (g_frontendRightBarPanelIndex == 3) {
			FrontendDraw_RectCopy(&scratch.rect, &g_frontendSidebarButtonRects[7]);
			if (g_frontendRightBarAnimState == 1) {
				result = FrontendButton_DrawSpriteWithHoverText(
					&scratch.rect, "leave", "leave", (void*)FrontendString_Get(STR_MAP_LEAVE_OF_ABSENCE),
					g_colorPaleBlue, g_colorLightBlue, 6, "buttonsound");
				if (result) {
					result = MissionDebrief_ConfirmSkipMission();
					if (result) {
						g_frontendQuickStartLaunchFlag = 0;
						g_frontendSinglePlayerFlightSessionActive = 0;
						g_missionSetupRosterAuthoritative = 0;
						g_frontendLeftBarAnimState = 2;
						g_frontendRightBarAnimState = 2;
						g_debriefAction = 7;
						FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
												  63);
					}
				}
			}
		}

		result = Frontend_HandleEscapeQuit(4);
		if (result == 1) {
			if (g_pilotData.campaignMode && g_missionOutcome != 1) {
				saved = g_pilotData.factionStatistics[1]
							.stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29];
				memcpy(&g_pilotData, &g_pilotDataSnapshot, sizeof(g_pilotData));
				g_pilotData.factionStatistics[1].stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29] =
					saved;
			}
			return 1;
		}
	}

	// ---- Disconnect / host-abort dialogs, then the shared chat panel ----
	if (g_debriefDisconnectedFromNetGame && frameCounter == 1) {
		Net_ShutdownDirectPlaySession();
		sprintf(g_frontendScratchBuffer, "%s.", g_pilotData.multiplayerGameName);
		FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_YOU_HAVE_BEEN_DISCONNECTED_FROM),
										 g_frontendScratchBuffer, NULL, NULL, NULL);
	} else if (g_flightNetHostAbortReceived && frameCounter == 1 && !Net_IsHost()) {
		FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_THE_HOST_ABORTED1),
										 FrontendString_Get(STR_THE_HOST_ABORTED2),
										 FrontendString_Get(STR_THE_HOST_ABORTED3), NULL, NULL);
	}
	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		FrontendNet_UpdateAndDrawPanel(frameCounter);
	}
	return 0;
}

// FUNCTION: XWA 0x57EC50
// Exit/teardown callback for the mission debrief screen. Frees the mission
// list, briefing text, and hints text buffers, unloads the medal image list
// and background, stops the wave stream, and resets scroll/keyboard/mouse gate
// state.
int MissionDebrief_Exit(int frameCounter) {
	(void)frameCounter;

	if (g_missionList) {
		Mem_Free(g_missionList);
		g_missionList = NULL;
	}
	if (g_briefingText) {
		Mem_Free(g_briefingText);
		g_briefingText = NULL;
	}
	if (g_hintsText) {
		Mem_Free(g_hintsText);
		g_hintsText = NULL;
	}
	FrontImage_UnloadResourceList("frontres\\medals\\medals.lst");
	FrontendWaveStream_Shutdown();
	FrontImage_FreeResourceByName("background");
	Frontend_ResetScrollableControls();
	Keyboard_FlushCharBuffer();
	FrontendMouse_ClearInputGate();
	return 0;
}

// FUNCTION: XWA 0x5803A0
// Debrief "Mission Overview" tab. Draws the per-team/per-player results table
// (Score/Time, Kills, Deaths) from the leaderboards built by
// MissionDebrief_Prepare and the g_pilotData statistics, plus the multiplayer
// "Killed"/"Killed By" breakdown, then draws the page scrollbar.
int MissionDebrief_DrawMissionOverviewPage(int frameCounter) {
	int place;
	int prevStat;
	unsigned short colorCode;
	int teamLoopIndex;
	int y;
	int x;
	int i;
	int rectBottom;
	int* playerIdPtr;
	int playerId;
	int color;
	int killIndex;
	int killRowY;
	int killHeaderY;
	int killRowStart;
	int killCount;
	int killedCount;
	int rankId;
	int fgIndex;
	int killsFull;
	int killsShared;
	FrontendRect rect;
	FrontendRect src;

	if (Keyboard_IsKeyDown(0x12) && Keyboard_IsKeyDown(0x10) && Keyboard_IsKeyDown(0x11)) {
		FrontImage_DrawSprite("lh2", 184, 362);
	}
	FrontendDraw_RectAssign(&rect, 65, 90, 575, 106);
	FrontendText_DrawCentered(12, FrontendString_Get(STR_MISSION_OVERVIEW), &rect, g_colorLightBlue);
	FrontendDraw_RectAssign(&rect, 65, 111, 575, 411);
	FrontendDisplay_SetScreenClipRect640x480(&rect);
	g_debriefMissionOverviewRowCount = 0;
	y = 111 - 15 * g_debriefMissionOverviewScrollOffset;
	if (!g_frontendMission->header.hangar) {
		FrontendText_Draw(12, FrontendString_Get(STR_TIME_CAPS), 300, y, g_colorLightBlue);
	} else {
		FrontendText_Draw(12, FrontendString_Get(STR_SCORE_CAPS), 300, y, g_colorLightBlue);
	}
	if (g_frontendMission->header.hangar) {
		FrontendText_Draw(12, FrontendString_Get(STR_KILLS_CAPS), 400, y, g_colorLightBlue);
	}
	FrontendText_Draw(12, FrontendString_Get(STR_DEATHS), 465, y, g_colorLightBlue);
	++g_debriefMissionOverviewRowCount;
	y += 15;

	place = 0;
	if (!g_frontendMission->header.hangar) {
		prevStat = g_pilotData.teamsStatistics[g_debriefSortedTeamIds[0]].missionTime;
	} else {
		prevStat = g_pilotData.teamsStatistics[g_debriefSortedTeamIds[0]].missionScore;
	}
	for (teamLoopIndex = 0; teamLoopIndex < 10; ++teamLoopIndex) {
		x = 88;
		if (g_debriefSortedTeamIds[teamLoopIndex] == -1) {
			break;
		}
		if (g_debriefTeamHasPlayer[g_debriefSortedTeamIds[teamLoopIndex]]) {
			if (!g_frontendMission->header.hangar) {
				if (prevStat !=
					g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].missionTime) {
					place = teamLoopIndex;
					prevStat = g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].missionTime;
				}
			} else {
				if (prevStat !=
					g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].missionScore) {
					place = teamLoopIndex;
					prevStat =
						g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].missionScore;
				}
			}
			if (g_frontendMission->header.goalsUnimportant) {
				colorCode = (place >= 3) ? 1 : 3;
			} else if (place >= 1 ||
					   (colorCode = 3, !g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]]
											.isMissionCompleted)) {
				colorCode = 1;
			}
			if (!g_debriefRankByPilot) {
				if (g_frontendMission->header.hangar == XWA_HANGAR_QUICKSTART ||
					g_frontendMission->header.hangar == XWA_HANGAR_SKIRMISH) {
					sprintf(g_frontendScratchBuffer, "%c%d. %c%s", colorCode, place + 1, 1,
							g_frontendMission->teams[g_debriefSortedTeamIds[teamLoopIndex]].name);
					FrontendText_Draw(12, g_frontendScratchBuffer, 88, y, g_colorLightBlue);
					sprintf(g_frontendScratchBuffer, "%d",
							g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].missionScore);
					FrontendText_Draw(12, g_frontendScratchBuffer, 300, y, 0xFFFF);
					if (g_frontendMission->header.hangar) {
						sprintf(
							g_frontendScratchBuffer, "%d (%d)",
							g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].kills,
							g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].killsShared);
					} else {
						sprintf(g_frontendScratchBuffer, "%d",
								g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].kills);
					}
					FrontendText_Draw(12, g_frontendScratchBuffer, 400, y, 0xFFFF);
					sprintf(g_frontendScratchBuffer, "%d",
							g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].killsAssist);
					FrontendText_Draw(12, g_frontendScratchBuffer, 465, y, 0xFFFF);
					x = 103;
					y += 15;
				} else if (g_debriefTeamUseSummaryRows[g_debriefSortedTeamIds[teamLoopIndex]]) {
					sprintf(g_frontendScratchBuffer, "%c%d. %c%s", colorCode, place + 1, 1,
							g_frontendMission->teams[g_debriefSortedTeamIds[teamLoopIndex]].name);
					FrontendText_Draw(12, g_frontendScratchBuffer, 88, y, g_colorLightBlue);
					sprintf(g_frontendScratchBuffer, "%d",
							g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].missionScore);
					FrontendText_Draw(12, g_frontendScratchBuffer, 300, y, 0xFFFF);
					if (g_frontendMission->header.hangar) {
						sprintf(
							g_frontendScratchBuffer, "%d (%d)",
							g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].kills,
							g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].killsShared);
					} else {
						sprintf(g_frontendScratchBuffer, "%d",
								g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].kills);
					}
					FrontendText_Draw(12, g_frontendScratchBuffer, 400, y, 0xFFFF);
					sprintf(g_frontendScratchBuffer, "%d",
							g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].killsAssist);
					FrontendText_Draw(12, g_frontendScratchBuffer, 465, y, 0xFFFF);
					y += 15;
					x = 103;
				} else {
					sprintf(g_frontendScratchBuffer, "%c%d. %c%s", colorCode, place + 1, 1,
							g_frontendMission->teams[g_debriefSortedTeamIds[teamLoopIndex]].name);
					FrontendText_Draw(12, g_frontendScratchBuffer, 88, y, g_colorLightBlue);
					y += 15;
					x = 103;
				}
				++g_debriefMissionOverviewRowCount;
			} else {
				if (g_debriefTeamUseSummaryRows[g_debriefSortedTeamIds[teamLoopIndex]]) {
					for (i = 0; i < (int16_t)g_frontendMission->flightGroupCount; ++i) {
						if (g_frontendMission->flightGroups[i].playerNumber &&
							g_frontendMission->flightGroups[i].team ==
								g_debriefSortedTeamIds[teamLoopIndex]) {
							sprintf(g_frontendScratchBuffer, "%c%d. %c%s %c%s", colorCode, place + 1, 6,
									FrontendString_Get((UIString)(g_pilotData.flightGroupRating[i] + 54)), 1,
									g_frontendMission->flightGroups[i].name);
							FrontendText_Draw(12, g_frontendScratchBuffer, x, y, g_colorLightBlue);
							if (!g_frontendMission->header.hangar) {
								Frontend_FormatSecondsToClockString(
									g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]]
										.missionTime);
							} else {
								sprintf(g_frontendScratchBuffer, "%d",
										g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]]
											.missionScore);
							}
							FrontendText_Draw(12, g_frontendScratchBuffer, 300, y, 0xFFFF);
							if (g_frontendMission->header.hangar) {
								sprintf(
									g_frontendScratchBuffer, "%d (%d)",
									g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].kills,
									g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]]
										.killsShared);
							} else {
								sprintf(
									g_frontendScratchBuffer, "%d",
									g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].kills);
							}
							FrontendText_Draw(12, g_frontendScratchBuffer, 400, y, 0xFFFF);
							x = 465;
							sprintf(g_frontendScratchBuffer, "%d",
									g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]]
										.killsAssist);
							FrontendText_Draw(12, g_frontendScratchBuffer, x, y, 0xFFFF);
							y += 15;
							++g_debriefMissionOverviewRowCount;
						}
					}
				}
			}
			if (!g_debriefTeamUseSummaryRows[g_debriefSortedTeamIds[teamLoopIndex]]) {
				rectBottom = y + 14;
				playerIdPtr = g_debriefSortedPlayerIds;
				while (1) {
					playerId = *playerIdPtr;
					if (playerId == -1) {
						break;
					}
					if (g_frontendMission->flightGroups[g_pilotData.networkPlayers[playerId].flightGroupId]
							.team == g_debriefSortedTeamIds[teamLoopIndex]) {
						FrontendDraw_RectAssign(&rect, x, y, 295, rectBottom);
						FrontendDisplay_GetScreenClipRect(&src);
						FrontendDisplay_SetScreenClipRect640x480(&rect);
						if (!g_debriefRankByPilot) {
							if (g_pilotData.networkPlayers[playerId].m60) {
								sprintf(g_frontendScratchBuffer, "[%s %s]",
										FrontendString_Get(
											(UIString)(g_pilotData.networkPlayers[playerId].rating + 54)),
										g_pilotData.networkPlayers[playerId].friendlyName);
							} else {
								sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
										FrontendString_Get(
											(UIString)(g_pilotData.networkPlayers[playerId].rating + 54)),
										1, g_pilotData.networkPlayers[playerId].friendlyName);
							}
						} else {
							if (g_pilotData.networkPlayers[playerId].m60) {
								sprintf(g_frontendScratchBuffer, "%c%d. %c[%s %s]", colorCode, place + 1, 1,
										FrontendString_Get(
											(UIString)(g_pilotData.networkPlayers[playerId].rating + 54)),
										g_pilotData.networkPlayers[playerId].friendlyName);
							} else {
								sprintf(g_frontendScratchBuffer, "%c%d. %c%s %c%s", colorCode, place + 1, 6,
										FrontendString_Get(
											(UIString)(g_pilotData.networkPlayers[playerId].rating + 54)),
										1, g_pilotData.networkPlayers[playerId].friendlyName);
							}
						}
						if (g_pilotData.networkPlayers[playerId].m60) {
							color = g_colorGray;
						} else if (g_pilotData.networkPlayers[playerId].directPlayId ==
									   Net_GetLocalPlayerId() ||
								   g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
							color = g_pulseColorRamp[(frameCounter % 24) >> 1];
						} else {
							color = g_colorYellow;
						}
						FrontendText_Draw(12, g_frontendScratchBuffer, x, y, color);
						FrontendDisplay_SetScreenClipRect640x480(&src);
						if (g_frontendMission->header.hangar == XWA_HANGAR_QUICKSTART ||
							g_frontendMission->header.hangar == XWA_HANGAR_SKIRMISH) {
							if (g_debriefRankByPilot) {
								sprintf(g_frontendScratchBuffer, "%d",
										g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]]
											.missionScore);
							} else {
								sprintf(g_frontendScratchBuffer, "%d",
										g_pilotData.networkPlayers[playerId].totalScore);
							}
						} else if (g_frontendMission->header.hangar == XWA_HANGAR_JUNKYARD) {
							if (g_pilotData
									.teamsStatistics[g_frontendMission
														 ->flightGroups[g_pilotData.networkPlayers[playerId]
																			.flightGroupId]
														 .team]
									.missionTime > 200000) {
								strcpy(g_frontendScratchBuffer, FrontendString_Get(STR_DEBRIEF_INCOMPLETE));
							} else {
								Frontend_FormatSecondsToClockString(
									g_pilotData
										.teamsStatistics[g_frontendMission
															 ->flightGroups[g_pilotData
																				.networkPlayers[playerId]
																				.flightGroupId]
															 .team]
										.missionTime);
							}
						} else {
							sprintf(g_frontendScratchBuffer, "%d",
									g_pilotData.networkPlayers[playerId].totalScore);
						}
						FrontendText_Draw(12, g_frontendScratchBuffer, 300, y, 0xFFFF);
						if (g_debriefRankByPilot) {
							if (g_frontendMission->header.hangar) {
								sprintf(
									g_frontendScratchBuffer, "%d (%d)",
									g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].kills,
									g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]]
										.killsShared);
								FrontendText_Draw(12, g_frontendScratchBuffer, 400, y, 0xFFFF);
							} else if (g_pilotData
										   .teamsStatistics[g_frontendMission
																->flightGroups[g_pilotData
																				   .networkPlayers[playerId]
																				   .flightGroupId]
																.team]
										   .missionTime > 200000) {
								sprintf(
									g_frontendScratchBuffer, "%d",
									g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].kills);
								FrontendText_Draw(12, g_frontendScratchBuffer, 400, y, 0xFFFF);
							}
							sprintf(g_frontendScratchBuffer, "%d",
									g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]]
										.killsAssist);
						} else {
							if (g_frontendMission->header.hangar) {
								sprintf(g_frontendScratchBuffer, "%d (%d)",
										g_pilotData.networkPlayers[playerId].kills,
										g_pilotData.networkPlayers[playerId].killsShared);
							} else {
								sprintf(
									g_frontendScratchBuffer, "%d",
									g_pilotData.teamsStatistics[g_debriefSortedTeamIds[teamLoopIndex]].kills);
							}
							FrontendText_Draw(12, g_frontendScratchBuffer, 400, y, 0xFFFF);
							sprintf(g_frontendScratchBuffer, "%d",
									g_pilotData.networkPlayers[playerId].totalLosses);
						}
						FrontendText_Draw(12, g_frontendScratchBuffer, 465, y, 0xFFFF);
						rectBottom += 15;
						++g_debriefMissionOverviewRowCount;
						y += 15;
						x = g_debriefRankByPilot ? 88 : 103;
					}
					if (++playerIdPtr >= &g_debriefSortedPlayerIds[8]) {
						break;
					}
				}
			}
		}
	}

	if (g_pilotData.missionDirectoryId != 1 &&
		g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		y += 15;
		FrontendText_Draw(12, FrontendString_Get(STR_KILLED), x, y, g_colorLightBlue);
		killHeaderY = y;
		killRowY = y + 15;
		killCount = 1;
		killRowStart = killRowY;
		for (killIndex = 0; killIndex < 8; ++killIndex) {
			rankId = g_debriefKillsOnRankIds[killIndex];
			if (rankId == -1) {
				break;
			}
			if (rankId < 8) {
				if (g_pilotData.networkPlayers[rankId].directPlayId == Net_GetLocalPlayerId() ||
					(!g_pilotData.killsFullOnPlayer[rankId] && !g_pilotData.killsSharedOnPlayer[rankId])) {
					continue;
				}
				FrontendDisplay_GetScreenClipRect(&src);
				FrontendDraw_RectAssign(&rect, 88, killRowY, 246, src.bottom);
				FrontendDisplay_SetScreenClipRect640x480(&rect);
				if (g_pilotData.networkPlayers[rankId].m60) {
					sprintf(g_frontendScratchBuffer, "[%s %s]",
							FrontendString_Get((UIString)(g_pilotData.networkPlayers[rankId].rating + 54)),
							g_pilotData.networkPlayers[rankId].friendlyName);
					color = g_colorGray;
				} else {
					sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
							FrontendString_Get((UIString)(g_pilotData.networkPlayers[rankId].rating + 54)), 4,
							g_pilotData.networkPlayers[rankId].friendlyName);
					color = g_colorYellow;
				}
				FrontendText_Draw(12, g_frontendScratchBuffer, 88, killRowY, color);
				FrontendDisplay_SetScreenClipRect640x480(&src);
				killsFull = g_pilotData.killsFullOnPlayer[rankId];
				killsShared = g_pilotData.killsSharedOnPlayer[rankId];
			} else {
				fgIndex = rankId - 8;
				if (!g_debriefTeamUseSummaryRows[g_frontendMission->flightGroups[fgIndex].team] ||
					(!g_pilotData.killsFullOnFlightGroup[fgIndex] &&
					 !g_pilotData.killsSharedOnFlightGroup[fgIndex])) {
					continue;
				}
				sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
						FrontendString_Get((UIString)(g_pilotData.flightGroupRating[fgIndex] + 54)), 4,
						g_frontendMission->flightGroups[fgIndex].name);
				FrontendText_Draw(12, g_frontendScratchBuffer, 88, killRowY, g_colorYellow);
				killsFull = g_pilotData.killsFullOnFlightGroup[fgIndex];
				killsShared = g_pilotData.killsSharedOnFlightGroup[fgIndex];
			}
			sprintf(g_frontendScratchBuffer, "%d(%d)", killsFull, killsShared);
			FrontendText_Draw(12, g_frontendScratchBuffer, 248, killRowY, 0xFFFF);
			killRowY += 15;
			++killCount;
		}

		killedCount = killCount;
		FrontendText_Draw(12, FrontendString_Get(STR_KILLED_BY), 320, killHeaderY, g_colorLightBlue);
		killRowY = killRowStart;
		killCount = 1;
		for (killIndex = 0; killIndex < 8; ++killIndex) {
			rankId = g_debriefKillsOnRankIds[killIndex];
			if (rankId == -1) {
				break;
			}
			if (rankId < 8) {
				if (g_pilotData.networkPlayers[rankId].directPlayId == Net_GetLocalPlayerId() ||
					(!g_pilotData.killsFullFromPlayer[rankId] &&
					 !g_pilotData.killsSharedFromPlayer[rankId])) {
					continue;
				}
				FrontendDisplay_GetScreenClipRect(&src);
				FrontendDraw_RectAssign(&rect, 320, killRowY, 478, src.bottom);
				FrontendDisplay_SetScreenClipRect640x480(&rect);
				if (g_pilotData.networkPlayers[rankId].m60) {
					sprintf(g_frontendScratchBuffer, "[%s %s]",
							FrontendString_Get((UIString)(g_pilotData.networkPlayers[rankId].rating + 54)),
							g_pilotData.networkPlayers[rankId].friendlyName);
					color = g_colorGray;
				} else {
					sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
							FrontendString_Get((UIString)(g_pilotData.networkPlayers[rankId].rating + 54)), 4,
							g_pilotData.networkPlayers[rankId].friendlyName);
					color = g_colorYellow;
				}
				FrontendText_Draw(12, g_frontendScratchBuffer, 320, killRowY, color);
				FrontendDisplay_SetScreenClipRect640x480(&src);
				killsFull = g_pilotData.killsFullFromPlayer[rankId];
				killsShared = g_pilotData.killsSharedFromPlayer[rankId];
			} else {
				fgIndex = rankId - 8;
				if (!g_debriefTeamUseSummaryRows[g_frontendMission->flightGroups[fgIndex].team] ||
					(!g_pilotData.killsFullFromFlightGroup[fgIndex] &&
					 !g_pilotData.killsSharedFromFlightGroup[fgIndex])) {
					continue;
				}
				sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
						FrontendString_Get((UIString)(g_pilotData.flightGroupRating[fgIndex] + 54)), 4,
						g_frontendMission->flightGroups[fgIndex].name);
				FrontendText_Draw(12, g_frontendScratchBuffer, 320, killRowY, g_colorYellow);
				killsFull = g_pilotData.killsFullFromFlightGroup[fgIndex];
				killsShared = g_pilotData.killsSharedFromFlightGroup[fgIndex];
			}
			sprintf(g_frontendScratchBuffer, "%d(%d)", killsFull, killsShared);
			FrontendText_Draw(12, g_frontendScratchBuffer, 480, killRowY, 0xFFFF);
			killRowY += 15;
			++killCount;
		}

		if (killedCount <= killCount) {
			g_debriefMissionOverviewRowCount += killCount;
		} else {
			g_debriefMissionOverviewRowCount += killedCount;
		}
	}
	FrontendDisplay_ResetScreenClipRect();
	if ((unsigned int)g_debriefMissionOverviewRowCount > 0x14) {
		FrontendDraw_RectAssign(&rect, 556, 111, 575, 411);
		g_debriefMissionOverviewScrollOffset =
			FrontendScrollbar_Draw(&rect, g_debriefMissionOverviewScrollOffset,
								   g_debriefMissionOverviewRowCount, 0, 5, g_colorNavy, 10);
	}
	return 1;
}

// FUNCTION: XWA 0x5814B0
int MissionDebrief_DrawPlayerStatisticsPage(void) {
	FrontendRect rect;
	char timeText[20];
	int localPlayerSlot;
	int playerSlot;
	int totalRows;
	int anyCraftKillRows;
	int rowHasData;
	int anyPlayerKillRows;
	int anyLossesFromPlayers;
	int missionType;
	int shipIndex;
	int typeId;
	int rating;
	int row;
	int y;
	int logoWidth;
	int logoCount;
	int partialLogo;
	int partialWidth;
	int x;
	int fullKills;
	int sharedKills;
	int localTeam;
	int missionScore;
	uint8_t hangarType;

	FrontendDraw_RectAssign(&rect, 65, 90, 575, 106);
	FrontendText_DrawCentered(12, FrontendString_Get(STR_PLAYER_STATISTICS), &rect, g_colorLightBlue);

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		localPlayerSlot = 0;
	} else {
		for (localPlayerSlot = 0; localPlayerSlot < 8; ++localPlayerSlot) {
			if (g_pilotData.networkPlayers[localPlayerSlot].directPlayId == Net_GetLocalPlayerId()) {
				break;
			}
		}
	}
	if (localPlayerSlot == 8) {
		return 0;
	}

	if (g_debriefStatsPageNeedsRebuild) {
		FrontendMissionSessionMode sessionMode = g_frontendMissionSessionMode;

		g_debriefStatsScrollOffset = 0;
		g_debriefStatsPageNeedsRebuild = 0;
		totalRows = (g_pilotData.missionDirectoryId == 3) ? 10 : 4;
		g_debriefStatsTotalRows = totalRows;
		if (g_frontendMission->header.hangar) {
			g_debriefStatsTotalRows = ++totalRows;
		}
		if (!g_frontendMission->header.goalsUnimportant &&
			g_frontendMission->header.hangar != XWA_HANGAR_QUICKSTART) {
			g_debriefStatsTotalRows = ++totalRows;
		}
		if (sessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			totalRows += 5;
			g_debriefStatsTotalRows = totalRows;
		}
		hangarType = g_frontendMission->header.hangar;
		if (hangarType == XWA_HANGAR_QUICKSTART || hangarType == XWA_HANGAR_JUNKYARD ||
			(hangarType == XWA_HANGAR_SKIRMISH && g_gameConfig.goalType == 1)) {
			g_debriefStatsTotalRows = ++totalRows;
		}
		if (g_pilotData.newPromotion) {
			g_debriefStatsTotalRows = ++totalRows;
		}

		g_debriefLossesToNonPlayerPilotsTotal = 0;
		g_debriefLossesToPlayerPilotsTotal = 0;
		g_debriefNonPlayerKillsSharedTotal = 0;
		g_debriefNonPlayerKillsByMissionType[0] = 0;
		g_debriefPlayerKillsSharedTotal = 0;
		g_debriefPlayerKillsByMissionType[0] = 0;
		g_debriefTotalKillsSharedByMissionType[0] = 0;
		g_debriefAssistTotalByMissionType[0] = 0;
		g_debriefHasCraftKillsByTypeSection = 0;

		anyCraftKillRows = 0;
		for (shipIndex = 0; shipIndex < g_shipCount; ++shipIndex) {
			g_debriefCraftKillRowHasData = 0;
			rowHasData = 0;
			for (missionType = 0; missionType < 1; ++missionType) {
				typeId = g_shipList[shipIndex].typeId + missionType * SHIP_TYPE_TO_SHIP_LIST_CAPACITY;
				if (g_pilotData.objectStats.killsPerCraftPerMT[0][typeId] ||
					g_pilotData.objectStats.killsSharedPerCraftPerMT[0][typeId]) {
					rowHasData = 1;
					anyCraftKillRows = 1;
					g_debriefCraftKillRowHasData = 1;
					g_debriefHasCraftKillsByTypeSection = 1;
				}
			}
			if (rowHasData) {
				g_debriefStatsTotalRows = ++totalRows;
			}
		}
		if (anyCraftKillRows) {
			totalRows += 2;
			g_debriefStatsTotalRows = totalRows;
		}

		anyPlayerKillRows = 0;
		g_debriefHasPlayerKillsByRating = 0;
		if (sessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			for (playerSlot = 0; playerSlot < 8; ++playerSlot) {
				if (g_pilotData.killsFullOnPlayer[playerSlot] ||
					g_pilotData.killsSharedOnPlayer[playerSlot]) {
					anyPlayerKillRows = 1;
					++totalRows;
				}
			}
			g_debriefStatsTotalRows = totalRows;
			g_debriefHasPlayerKillsByRating = anyPlayerKillRows;
			if (anyPlayerKillRows) {
				totalRows += 2;
				g_debriefStatsTotalRows = totalRows;
			}
		}

		anyLossesFromPlayers = 0;
		g_debriefHasLossesFromPlayersSection = 0;
		if (sessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			for (playerSlot = 0; playerSlot < 8; ++playerSlot) {
				if (g_pilotData.killsFullFromPlayer[playerSlot] ||
					g_pilotData.killsSharedFromPlayer[playerSlot]) {
					anyLossesFromPlayers = 1;
					++totalRows;
				}
			}
			g_debriefStatsTotalRows = totalRows;
			g_debriefHasLossesFromPlayersSection = anyLossesFromPlayers;
			if (anyLossesFromPlayers) {
				g_debriefStatsTotalRows = totalRows + 2;
			}
		}

		for (missionType = 0; missionType < 1; ++missionType) {
			for (shipIndex = 0; shipIndex < g_shipCount; ++shipIndex) {
				typeId = g_shipList[shipIndex].typeId + missionType * SHIP_TYPE_TO_SHIP_LIST_CAPACITY;
				g_debriefAssistTotalByMissionType[missionType] +=
					g_pilotData.objectStats.killsAssistsPerCraftPerMT[0][typeId];
				g_debriefTotalKillsSharedByMissionType[missionType] +=
					g_pilotData.objectStats.killsSharedPerCraftPerMT[0][typeId];
			}
		}

		for (missionType = 0; missionType < 1; ++missionType) {
			for (rating = 0; rating < 25; ++rating) {
				g_debriefPlayerKillsByMissionType[missionType] +=
					g_pilotData.objectStats.killsFullOnPlayerRatingPerMT[missionType][rating] +
					g_pilotData.objectStats.killsSharedOnPlayerRatingPerMT[missionType][rating];
			}
		}
		for (missionType = 0; missionType < 1; ++missionType) {
			for (rating = 0; rating < 6; ++rating) {
				g_debriefNonPlayerKillsByMissionType[missionType] +=
					g_pilotData.objectStats.killsFullOnAIRatingPerMT[missionType][rating] +
					g_pilotData.objectStats.killsSharedOnAIRatingPerMT[missionType][rating];
			}
		}
		for (missionType = 0; missionType < 1; ++missionType) {
			for (rating = 0; rating < 25; ++rating) {
				g_debriefLossesToPlayerPilotsTotal +=
					g_pilotData.objectStats.killedByPlayerRatingPerMT[missionType][rating];
			}
		}
		for (missionType = 0; missionType < 1; ++missionType) {
			for (rating = 0; rating < 6; ++rating) {
				g_debriefLossesToNonPlayerPilotsTotal +=
					g_pilotData.objectStats.killedByAIRatingPerMT[missionType][rating];
			}
		}
	}

	FrontendDraw_RectAssign(&rect, 551, 111, 570, 418);
	if (g_debriefStatsTotalRows > 21) {
		g_debriefStatsScrollOffset = FrontendScrollbar_Draw(&rect, g_debriefStatsScrollOffset,
															g_debriefStatsTotalRows, 0, 5, g_colorNavy, 10);
	}

	row = 0;
	y = 111;
	hangarType = g_frontendMission->header.hangar;

	if (hangarType == XWA_HANGAR_QUICKSTART || hangarType == XWA_HANGAR_JUNKYARD ||
		(hangarType == XWA_HANGAR_SKIRMISH && g_gameConfig.goalType == 1)) {
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			if (g_debriefRankByPilot) {
				sprintf(g_frontendScratchBuffer, "%c%s %c%s %c%s %c%d %c%s", 2, FrontendString_Get(STR_PLACE),
						1, FrontendString_Get((UIString)(g_debriefLocalTeamRankIndex + STR_1ST)), 4,
						FrontendString_Get(STR_OF), 1, g_debriefActiveTeamCount, 2,
						FrontendString_Get(STR_PILOTS));
			} else {
				sprintf(g_frontendScratchBuffer, "%c%s %c%s %c%s %c%d %c%s", 2, FrontendString_Get(STR_PLACE),
						1, FrontendString_Get((UIString)(g_debriefLocalTeamRankIndex + STR_1ST)), 4,
						FrontendString_Get(STR_OF), 1, g_debriefActiveTeamCount, 2,
						FrontendString_Get(STR_TEAMS));
			}
			FrontendText_Draw(12, g_frontendScratchBuffer, 88, 111, 0xFFFF);
			y = 126;
		}
		++row;
	}

	if (!g_frontendMission->header.goalsUnimportant &&
		g_frontendMission->header.hangar != XWA_HANGAR_QUICKSTART) {
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			if (g_missionOutcome == 2) {
				sprintf(g_frontendScratchBuffer, "%c%s %c%s", 2, FrontendString_Get(STR_RESULT), 1,
						FrontendString_Get(STR_DRAW));
			} else if (g_missionOutcome == 1) {
				localTeam =
					g_frontendMission->flightGroups[g_pilotData.networkPlayers[localPlayerSlot].flightGroupId]
						.team;
				Frontend_FormatSecondsToClockString(g_pilotData.teamsStatistics[localTeam].missionTime);
				strcpy(timeText, g_frontendScratchBuffer);
				sprintf(g_frontendScratchBuffer, "%c%s %c%s %s.", 2, FrontendString_Get(STR_RESULT), 1,
						FrontendString_Get(STR_COMPLETED_MISSION_IN), timeText);
			} else {
				sprintf(g_frontendScratchBuffer, "%c%s %c%s", 2, FrontendString_Get(STR_RESULT), 1,
						FrontendString_Get(STR_FAILED_MISSION));
			}
			FrontendText_Draw(12, g_frontendScratchBuffer, 88, y, 0xFFFF);
			y += 15;
		}
		++row;
	}

	if (g_frontendMission->header.hangar) {
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			if (g_frontendMission->header.hangar == XWA_HANGAR_SKIRMISH) {
				if (g_gameConfig.goalType == 1) {
					missionScore = g_pilotData.missionScore;
				} else {
					missionScore = g_pilotData.teamsStatistics[g_pilotData.team].missionScore;
				}
				sprintf(g_frontendScratchBuffer, "%c%s %c%d", 2, FrontendString_Get(STR_MISSION_SCORE), 1,
						missionScore);
			} else {
				sprintf(g_frontendScratchBuffer, "%c%s %c%d                    %c%s %c%d", 2,
						FrontendString_Get(STR_MISSION_SCORE), 1, g_pilotData.missionScore, 2,
						FrontendString_Get(STR_DEBRIEF_BONUS_SCORE), 1,
						g_pilotData.objectStats.totalScorePerMT[0] / 10);
			}
			FrontendText_Draw(12, g_frontendScratchBuffer, 88, y, 0xFFFF);
			y += 15;
		}
		++row;
	}

	if (g_pilotData.newPromotion) {
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			sprintf(g_frontendScratchBuffer, "%c%s %c%s", 2,
					FrontendString_Get((UIString)(g_pilotData.newPromotion + STR_NO_PROMOTION)), 6,
					FrontendString_Get((UIString)(g_pilotData.pilotRating + 22)));
			FrontendText_Draw(12, g_frontendScratchBuffer, 88, y, 0xFFFF);
			y += 15;
		}
		++row;
	}

	g_debriefCraftKillRowHasData = 0;
	if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
		y += 15;
	}
	++row;
	if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
		FrontendText_Draw(12, FrontendString_Get(STR_SUMMARY_OF_KILLS), 88, y, g_colorLightBlue);
		y += 15;
	}
	++row;
	if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
		FrontendText_Draw(12, FrontendString_Get(STR_TOTAL_KILLS), 88, y, g_colorLightBlue);
		sprintf(g_frontendScratchBuffer, "%d (%d)", g_pilotData.objectStats.totalKillsPerMT[0],
				g_debriefTotalKillsSharedByMissionType[0]);
		FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);
		y += 15;
	}
	++row;

	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			FrontendText_Draw(12, FrontendString_Get(STR_PLAYER_KILLS), 88, y, g_colorLightBlue);
			if (!g_pilotData.missionDirectoryId) {
				sprintf(g_frontendScratchBuffer, g_frontendNoValueText);
			} else {
				sprintf(g_frontendScratchBuffer, "%d (%d)", g_debriefPlayerKillsByMissionType[0],
						g_debriefPlayerKillsSharedTotal);
			}
			FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);
			y += 15;
		}
		++row;
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			FrontendText_Draw(12, FrontendString_Get(STR_NON_PLAYER_KILLS), 88, y, g_colorLightBlue);
			sprintf(g_frontendScratchBuffer, "%d (%d)", g_debriefNonPlayerKillsByMissionType[0],
					g_debriefNonPlayerKillsSharedTotal);
			FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);
			y += 15;
		}
		++row;
	}

	if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
		FrontendText_Draw(12, FrontendString_Get(STR_ASSISTS), 88, y, g_colorLightBlue);
		sprintf(g_frontendScratchBuffer, "%d", g_debriefAssistTotalByMissionType[0]);
		FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);
		y += 15;
	}
	++row;

	if (g_debriefHasPlayerKillsByRating) {
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			y += 15;
		}
		++row;
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			FrontendText_Draw(12, FrontendString_Get(STR_PLAYER_KILLS_BY_RATING), 88, y, g_colorLightBlue);
			y += 15;
		}
		++row;
		for (rating = 24; rating >= 0; --rating) {
			for (playerSlot = 0; playerSlot < 8; ++playerSlot) {
				if (g_pilotData.networkPlayers[playerSlot].rating == rating &&
					(g_pilotData.killsFullOnPlayer[playerSlot] ||
					 g_pilotData.killsSharedOnPlayer[playerSlot])) {
					if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
						sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
								FrontendString_Get((UIString)(rating + 54)), 1,
								g_pilotData.networkPlayers[playerSlot].friendlyName);
						FrontendText_Draw(12, g_frontendScratchBuffer, 88, y, g_colorYellow);
						sprintf(g_frontendScratchBuffer, "%d(%d)", g_pilotData.killsFullOnPlayer[playerSlot],
								g_pilotData.killsSharedOnPlayer[playerSlot]);
						FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);

						fullKills = g_pilotData.killsFullOnPlayer[playerSlot];
						sharedKills = g_pilotData.killsSharedOnPlayer[playerSlot];
						partialLogo = sharedKills & 1;
						logoCount = fullKills + (sharedKills >> 1);
						FrontImage_GetResourceRect("implogo1", &rect);
						logoWidth = rect.right - rect.left + 1;
						x = 313;
						while (logoCount > 0) {
							if (x + logoWidth > 555) {
								break;
							}
							FrontImage_DrawSprite("implogo1", x, y);
							x += logoWidth + 1;
							--logoCount;
						}
						if (partialLogo) {
							partialWidth = 5 * logoWidth;
							if (x + partialWidth / 10 < 555) {
								rect.right = rect.left + partialWidth / 10 - 1;
								FrontImage_DrawSpriteRectTransparent("implogo1", &rect, x, y);
							}
						}
						y += 15;
					}
					++row;
				}
			}
		}
	}

	if (g_debriefHasCraftKillsByTypeSection) {
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			y += 15;
		}
		++row;
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			FrontendText_Draw(12, FrontendString_Get(STR_CRAFT_KILLS_BY_TYPE), 88, y, g_colorLightBlue);
			y += 15;
		}
		++row;
	}

	for (shipIndex = 0; shipIndex < g_shipCount; ++shipIndex) {
		typeId = g_shipList[shipIndex].typeId;
		fullKills = g_pilotData.objectStats.killsPerCraftPerMT[0][typeId];
		sharedKills = g_pilotData.objectStats.killsSharedPerCraftPerMT[0][typeId];
		g_debriefCraftKillRowHasData = 0;
		if (fullKills || sharedKills) {
			g_debriefCraftKillRowHasData = 1;
			if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
				FrontendText_Draw(12, g_shipList[shipIndex].name, 88, y, g_colorRed);
				sprintf(g_frontendScratchBuffer, "%d (%d)", fullKills, sharedKills);
				FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);

				partialLogo = sharedKills & 1;
				logoCount = fullKills + (sharedKills >> 1);
				FrontImage_GetResourceRect("implogo1", &rect);
				logoWidth = rect.right - rect.left + 1;
				x = 313;
				while (logoCount > 0) {
					if (x + logoWidth > 555) {
						break;
					}
					FrontImage_DrawSprite("implogo1", x, y);
					x += logoWidth + 1;
					--logoCount;
				}
				if (partialLogo) {
					partialWidth = 5 * logoWidth;
					if (x + partialWidth / 10 < 555) {
						rect.right = rect.left + partialWidth / 10 - 1;
						FrontImage_DrawSpriteRectTransparent("implogo1", &rect, x, y);
					}
				}
				y += 15;
			}
			++row;
		}
	}

	if (g_pilotData.missionDirectoryId == 3) {
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			y += 15;
		}
		++row;
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			FrontendText_Draw(12, FrontendString_Get(STR_TOTAL_LOSSES), 88, y, g_colorLightBlue);
			y += 15;
		}
		++row;
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			FrontendText_Draw(12, FrontendString_Get(STR_TOTAL_CRAFT_LOSSES), 88, y, g_colorLightBlue);
			sprintf(g_frontendScratchBuffer, "%d", g_pilotData.objectStats.totalCraftLossesPerMT[0]);
			FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);
			y += 15;
		}
		++row;

		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
				FrontendText_Draw(12, FrontendString_Get(STR_TO_PLAYER_PILOTS), 88, y, g_colorLightBlue);
				sprintf(g_frontendScratchBuffer, "%d", g_debriefLossesToPlayerPilotsTotal);
				FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);
				y += 15;
			}
			++row;
			if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
				FrontendText_Draw(12, FrontendString_Get(STR_TO_NON_PLAYER_PILOTS), 88, y, g_colorLightBlue);
				sprintf(g_frontendScratchBuffer, "%d", g_debriefLossesToNonPlayerPilotsTotal);
				FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);
				y += 15;
			}
			++row;
		}

		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			FrontendText_Draw(12, FrontendString_Get(STR_TO_STARSHIPS), 88, y, g_colorLightBlue);
			sprintf(g_frontendScratchBuffer, "%d", g_pilotData.objectStats.lossesByStarshipsPerMT[0]);
			FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);
			y += 15;
		}
		++row;
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			FrontendText_Draw(12, FrontendString_Get(STR_TO_MINES), 88, y, g_colorLightBlue);
			sprintf(g_frontendScratchBuffer, "%d", g_pilotData.objectStats.lossesByMinesPerMT[0]);
			FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);
			y += 15;
		}
		++row;
		if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
			FrontendText_Draw(12, FrontendString_Get(STR_FROM_COLLISIONS), 88, y, g_colorLightBlue);
			sprintf(g_frontendScratchBuffer, "%d", g_pilotData.objectStats.lossesByCollisionsPerMT[0]);
			FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);
			y += 15;
		}
		++row;

		if (g_debriefHasLossesFromPlayersSection) {
			if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
				y += 15;
			}
			++row;
			if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
				FrontendText_Draw(12, FrontendString_Get(STR_LOSSES_FROM_PLAYERS), 88, y, g_colorLightBlue);
				y += 15;
			}
			++row;
			for (rating = 24; rating >= 0; --rating) {
				for (playerSlot = 0; playerSlot < 8; ++playerSlot) {
					if (g_pilotData.networkPlayers[playerSlot].rating == rating &&
						(g_pilotData.killsFullFromPlayer[playerSlot] ||
						 g_pilotData.killsSharedFromPlayer[playerSlot])) {
						if (row >= g_debriefStatsScrollOffset && row - g_debriefStatsScrollOffset < 21) {
							sprintf(g_frontendScratchBuffer, "%c%s %c%s", 6,
									FrontendString_Get((UIString)(rating + 54)), 1,
									g_pilotData.networkPlayers[playerSlot].friendlyName);
							FrontendText_Draw(12, g_frontendScratchBuffer, 88, y, g_colorYellow);
							sprintf(g_frontendScratchBuffer, "%d(%d)",
									g_pilotData.killsFullFromPlayer[playerSlot],
									g_pilotData.killsSharedFromPlayer[playerSlot]);
							FrontendText_Draw(12, g_frontendScratchBuffer, 263, y, 0xFFFF);

							fullKills = g_pilotData.killsFullFromPlayer[playerSlot];
							sharedKills = g_pilotData.killsSharedFromPlayer[playerSlot];
							partialLogo = sharedKills & 1;
							logoCount = fullKills + (sharedKills >> 1);
							FrontImage_GetResourceRect("reblogo1", &rect);
							logoWidth = rect.right - rect.left + 1;
							x = 313;
							while (logoCount > 0) {
								if (x + logoWidth > 555) {
									break;
								}
								/* The original full-loss icons use implogo1 here; only the clipped half uses
								 * reblogo1. */
								FrontImage_DrawSprite("implogo1", x, y);
								x += logoWidth + 1;
								--logoCount;
							}
							if (partialLogo) {
								partialWidth = 5 * logoWidth;
								if (x + partialWidth / 10 < 555) {
									rect.right = rect.left + partialWidth / 10 - 1;
									FrontImage_DrawSpriteRectTransparent("reblogo1", &rect, x, y);
								}
							}
							y += 15;
						}
						++row;
					}
				}
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x5827A0
// Draws the debrief screen's animated left/right side panels (rightbar%d /
// leftbar%d sprites) and, once the left bar is fully extended, the stack of tab
// buttons. Which tabs appear depends on session mode, hangar type and the
// mission outcome: Mission Overview (0), Player Statistics (1), Mission
// Debriefing (2) and Mission Hints (3). Clicking a tab switches g_briefingTab.
// Returns 1 once the tab buttons are drawn, 0 while the bars are still sliding.
int MissionDebrief_DrawTabBar(void) {
	FrontendRect rightBarRect;
	FrontendRect tabRect;
	char rightBarName[32] = "rightbar3";
	FrontendRect leftBarRect;
	char leftBarName[36];
	int showPlayerStats = 0;
	int showDebriefing = 0;
	int showHints = 0;
	int cursorY;
	int cursorX;
	int showOverview;
	int tabCount = 0;

	rightBarName[8] = (char)(g_frontendRightBarPanelIndex + '0');

	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER ||
		g_frontendMission->header.hangar == XWA_HANGAR_QUICKSTART ||
		g_frontendMission->header.hangar == XWA_HANGAR_SKIRMISH) {
		showOverview = 1;
		tabCount = 1;
	} else {
		showOverview = 0;
	}

	if (g_frontendMission->header.hangar != XWA_HANGAR_QUICKSTART) {
		showPlayerStats = 1;
		++tabCount;
	} else {
		showPlayerStats = 0;
	}

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
		(g_pilotData.missionDirectoryId == 4 || g_pilotData.missionDirectoryId == 5)) {
		if (g_missionOutcome != 1) {
			showHints = 1;
		} else {
			showDebriefing = 1;
		}
		++tabCount;
	}

	FrontImage_GetResourceRect(rightBarName, &rightBarRect);
	sprintf(leftBarName, "leftbar%d", tabCount);
	FrontImage_GetResourceRect(leftBarName, &leftBarRect);

	/* Right side bar slide-in / slide-out animation. */
	if (g_frontendRightBarAnimState == 0) {
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
		if (FrontImage_GetSpriteFrame(rightBarName) == 0) {
			g_frontendRightBarAnimState = 3;
		}
	} else if (g_frontendRightBarAnimState == 4) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (FrontImage_GetSpriteFrame(rightBarName) == 0) {
			g_frontendRightBarAnimState = 0;
			g_frontendRightBarPanelIndex = 2;
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}
	} else if (g_frontendRightBarAnimState == 5) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (FrontImage_GetSpriteFrame(rightBarName) == 0) {
			g_frontendRightBarAnimState = 0;
			g_frontendRightBarPanelIndex = 1;
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}
	}

	/* Left side bar slide-in animation; only state 1 (fully extended) shows tabs. */
	if (g_frontendLeftBarAnimState == 0) {
		FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(leftBarName, 1);
		if (FrontImage_GetSpriteFrame(leftBarName) == 9) {
			g_frontendLeftBarAnimState = 1;
		}
		return 0;
	}

	if (g_frontendLeftBarAnimState == 1) {
		FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
		FrontendDraw_RectCopy(&tabRect, &g_frontendSidebarButtonRects[tabCount - 1]);
		FrontendCursor_GetPos(&cursorX, &cursorY);

		if (showOverview) {
			if (g_briefingTab == 0) {
				FrontendButton_DrawCenteredTintedSpriteWithTooltip(
					&tabRect, "misoverview", FrontendString_Get(STR_MISSION_OVERVIEW), g_colorGreen);
			} else if (FrontendButton_DrawSpriteWithHoverText(&tabRect, "misoverview", "misoverview",
															  (void*)FrontendString_Get(STR_MISSION_OVERVIEW),
															  g_colorPaleBlue, g_colorLightBlue, 11,
															  "jewelsound")) {
				FrontendWaveStream_Shutdown();
				g_debriefStatsPageNeedsRebuild = 1;
				g_debriefTextRevealFrame = 0;
				g_briefingTab = 0;
				g_pendingVoiceWav[0] = 0;
			}
			--tabCount;
#ifdef XWA_MODERN
			if (tabCount > 0) {
#endif
				FrontendDraw_RectCopy(&tabRect, &g_frontendSidebarButtonRects[tabCount - 1]);
#ifdef XWA_MODERN
			}
#endif
		}

		if (showHints) {
			if (g_briefingTab == 3) {
				FrontendButton_DrawCenteredTintedSpriteWithTooltip(
					&tabRect, "hints", FrontendString_Get(STR_MISSION_HINTS), g_colorGreen);
			} else if (FrontendButton_DrawSpriteWithHoverText(
						   &tabRect, "hints", "hints", (void*)FrontendString_Get(STR_MISSION_HINTS),
						   g_colorPaleBlue, g_colorLightBlue, 14, "jewelsound")) {
				FrontendWaveStream_Shutdown();
				g_debriefStatsPageNeedsRebuild = 1;
				g_debriefTextRevealFrame = 0;
				g_briefingTab = 3;
				g_pendingVoiceWav[0] = 0;
			}
			--tabCount;
#ifdef XWA_MODERN
			if (tabCount > 0) {
#endif
				FrontendDraw_RectCopy(&tabRect, &g_frontendSidebarButtonRects[tabCount - 1]);
#ifdef XWA_MODERN
			}
#endif
		}

		if (showPlayerStats) {
			if (g_briefingTab == 1) {
				FrontendButton_DrawCenteredTintedSpriteWithTooltip(
					&tabRect, "playerstat", FrontendString_Get(STR_PLAYER_STATISTICS), g_colorGreen);
			} else if (FrontendButton_DrawSpriteWithHoverText(
						   &tabRect, "playerstat", "playerstat",
						   (void*)FrontendString_Get(STR_PLAYER_STATISTICS), g_colorPaleBlue,
						   g_colorLightBlue, 12, "jewelsound")) {
				FrontendWaveStream_Shutdown();
				g_debriefStatsPageNeedsRebuild = 1;
				g_debriefTextRevealFrame = 0;
				g_briefingTab = 1;
				g_pendingVoiceWav[0] = 0;
			}
			--tabCount;
#ifdef XWA_MODERN
			if (tabCount > 0) {
#endif
				FrontendDraw_RectCopy(&tabRect, &g_frontendSidebarButtonRects[tabCount - 1]);
#ifdef XWA_MODERN
			}
#endif
		}

		if (showDebriefing) {
			if (g_briefingTab == 2) {
				FrontendButton_DrawCenteredTintedSpriteWithTooltip(
					&tabRect, "misdebrief", FrontendString_Get(STR_MISSION_DEBRIEFING), g_colorGreen);
			} else if (FrontendButton_DrawSpriteWithHoverText(
						   &tabRect, "misdebrief", "misdebrief",
						   (void*)FrontendString_Get(STR_MISSION_DEBRIEFING), g_colorPaleBlue,
						   g_colorLightBlue, 13, "jewelsound")) {
				g_debriefStatsPageNeedsRebuild = 1;
				g_debriefTextRevealFrame = 0;
				g_briefingTab = 2;
				g_pendingVoiceWav[0] = 0;
			}
		}

		return 1;
	}

	if (g_frontendLeftBarAnimState == 2) {
		FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(leftBarName, 1);
		if (FrontImage_GetSpriteFrame(leftBarName) == 0) {
			g_frontendLeftBarAnimState = 3;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x582EA0
int MissionDebrief_MarkNetworkPlayersReady(void) {
	int i;

	for (i = 0; i < 8; ++i) {
		if (g_pilotData.networkPlayers[i].directPlayId) {
			Net_MarkPlayerReadyNoLock(g_pilotData.networkPlayers[i].directPlayId);
		}
	}

	return 1;
}

// FUNCTION: XWA 0x584280
int MissionDebrief_ConfirmSkipMission(void) {
	char waveName[20];
	char briefingOfficer;
	int promptLine;
	int confirmed;
	int responseLine;

	promptLine = rand() % 2;
	briefingOfficer = g_frontendMission->header.briefingOfficer;

	if (promptLine == 0) {
		if (briefingOfficer == 0) {
			sprintf(waveName, "G2DE001.wav");
		} else if (briefingOfficer == 1) {
			sprintf(waveName, "G2DE001.wav");
		} else if (briefingOfficer == 2) {
			sprintf(waveName, "G2DE001.wav");
		}
		strcpy(g_frontendScratchBuffer, "wave\\frontend\\MEDALANDPROMOTION");
		strcat(g_frontendScratchBuffer, waveName);
		FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);

		confirmed = FrontendDialog_ShowConfirmDialog(
			FrontendString_Get(STR_MAP_LEAVE_MESSAGE_1A), FrontendString_Get(STR_MAP_LEAVE_MESSAGE_1B),
			FrontendString_Get(STR_MAP_LEAVE_MESSAGE_1C), FrontendString_Get(STR_OKAY),
			FrontendString_Get(STR_CANCEL));
	} else {
		if (briefingOfficer == 0) {
			sprintf(waveName, "G2DE002.wav");
		} else if (briefingOfficer == 1) {
			sprintf(waveName, "G2DE002.wav");
		} else if (briefingOfficer == 2) {
			sprintf(waveName, "G2DE002.wav");
		}
		strcpy(g_frontendScratchBuffer, "wave\\frontend\\MEDALANDPROMOTION");
		strcat(g_frontendScratchBuffer, waveName);
		FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);

		confirmed = FrontendDialog_ShowConfirmDialog(
			FrontendString_Get(STR_MAP_LEAVE_MESSAGE_2A), FrontendString_Get(STR_MAP_LEAVE_MESSAGE_2B),
			FrontendString_Get(STR_MAP_LEAVE_MESSAGE_2C), FrontendString_Get(STR_OKAY),
			FrontendString_Get(STR_CANCEL));
	}

	FrontendWaveStream_Shutdown();

	if (confirmed) {
		responseLine = rand() % 5;
		if (g_frontendMission->header.briefingOfficer == 0) {
			sprintf(waveName, "G2DE00%d.wav", responseLine + 5);
		} else if (g_frontendMission->header.briefingOfficer == 1) {
			sprintf(waveName, "G2DE00%d.wav", responseLine + 5);
		} else if (g_frontendMission->header.briefingOfficer == 2) {
			sprintf(waveName, "G2DE00%d.wav", responseLine + 5);
		}
		strcpy(g_frontendScratchBuffer, "wave\\frontend\\MEDALANDPROMOTION");
		strcat(g_frontendScratchBuffer, waveName);
		FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
	}

	return confirmed;
}

#ifdef XWA_MODERN
static void MissionDebrief_FinishAwardCeremonyMovie(void) {
	FrontendDisplay_ClearBackBuffer();
	FrontendDisplay_ClearOffscreenSurface();
	FrontImage_DrawSpriteOpaque("cerecase", 0, 0);
	FrontendDisplay_LockOffscreenSurface();
	FrontImage_DrawSpriteOpaque("cerecase", 0, 0);
	FrontendDisplay_UnlockOffscreenSurface(1);
	FrontImage_FreeResourceByName("cerecase");
	g_awardCeremonyState = 1;
	g_awardCeremonyFrameCounter = 0;
}
#endif

// FUNCTION: XWA 0x584500
int MissionDebrief_ShowAwardCeremony(int frameCounter) {
	FrontendRect rect;
	char waveName[20];
	int awardId;
	int x;
	int width;
	int height;
	int textWidth;
	int revealCount;
	int parsedFirst;
	char separator;
	int battleNumber;
	int missionNumber;
	UIString awardNameString;

	if (frameCounter == 0) {
		g_awardCeremonyState = 0;
		g_awardCeremonyFrameCounter = 0;
	}

	g_pilotData.emkayAnnounceNewAward = 2;

	switch (g_awardCeremonyState) {
		case 1:
			awardId = g_pendingAward & 0x7fffffff;

			if (awardId != 0 && (g_pendingAward & 0x80000000) != 0) {
				sprintf(g_frontendScratchBuffer, "lbattlezoom%d", awardId - 1);
				FrontImage_GetResourceRect(g_frontendScratchBuffer, &rect);
				width = rect.right - rect.left + 1;
				height = rect.bottom - rect.top + 1;
				x = 152 - (width >> 1);
				FrontImage_DrawSprite(g_frontendScratchBuffer, x, 360 - (height >> 1));
				FrontImage_AdvanceSpriteFrame(g_frontendScratchBuffer, 1);

				strcpy(g_frontendScratchBuffer,
					   FrontendString_Get((UIString)(STR_FAMILY_BATTLE_MEDAL1 + awardId - 1)));
				rect.left = x + width + 30;
				textWidth = FrontendText_MeasureWidth(g_frontendScratchBuffer, 12);
				rect.top = 345;
				rect.right = rect.left + textWidth + 30;
				rect.bottom = 375;
				FrontendDraw_FillRectTranslucent(&rect, 0, 0, g_awardCeremonyLabelFillColor);
				FrontendText_DrawCentered(12, g_frontendScratchBuffer, &rect, g_colorLightBlue);

				sprintf(g_frontendScratchBuffer, "lkalidorzoom%d", g_pilotData.kalidorCresent - 1);
				FrontImage_GetResourceRect(g_frontendScratchBuffer, &rect);
				width = rect.right - rect.left + 1;
				height = rect.bottom - rect.top + 1;
				x = 388 - (width >> 1);
				FrontImage_DrawSprite(g_frontendScratchBuffer, x, 120 - (height >> 1));
				FrontImage_AdvanceSpriteFrame(g_frontendScratchBuffer, 1);

				strcpy(g_frontendScratchBuffer,
					   FrontendString_Get((UIString)(STR_FAMILY_KALIDOR1 + g_pilotData.kalidorCresent - 1)));
				textWidth = FrontendText_MeasureWidth(g_frontendScratchBuffer, 12);
				rect.left = x - textWidth - 60;
				rect.top = 105;
				rect.right = rect.left + textWidth + 30;
				rect.bottom = 135;
				revealCount = g_awardCeremonyFrameCounter;
				++g_awardCeremonyFrameCounter;
				FrontendText_DrawCenteredReveal(12, g_frontendScratchBuffer, &rect, g_colorLightBlue,
												revealCount);
			} else {
				if (awardId != 0) {
					sprintf(g_frontendScratchBuffer, "lbattlezoom%d", awardId - 1);
					FrontImage_GetResourceRect(g_frontendScratchBuffer, &rect);
					width = rect.right - rect.left + 1;
					height = rect.bottom - rect.top + 1;
					FrontImage_DrawSprite(g_frontendScratchBuffer, 320 - (width >> 1), 240 - (height >> 1));
					FrontImage_AdvanceSpriteFrame(g_frontendScratchBuffer, 1);
					awardNameString = (UIString)(STR_FAMILY_BATTLE_MEDAL1 + awardId - 1);
				} else {
					sprintf(g_frontendScratchBuffer, "lkalidorzoom%d", g_pilotData.kalidorCresent - 1);
					FrontImage_GetResourceRect(g_frontendScratchBuffer, &rect);
					width = rect.right - rect.left + 1;
					height = rect.bottom - rect.top + 1;
					FrontImage_DrawSprite(g_frontendScratchBuffer, 320 - (width >> 1), 240 - (height >> 1));
					FrontImage_AdvanceSpriteFrame(g_frontendScratchBuffer, 1);
					awardNameString = (UIString)(STR_FAMILY_KALIDOR1 + g_pilotData.kalidorCresent - 1);
				}
				strcpy(g_frontendScratchBuffer, FrontendString_Get(awardNameString));

				textWidth = FrontendText_MeasureWidth(g_frontendScratchBuffer, 15);
				rect.left = 300 - (textWidth >> 1);
				rect.top = 400;
				rect.right = rect.left + textWidth + 40;
				rect.bottom = 440;
				FrontendDraw_FillRectTranslucent(&rect, 0, 0, g_awardCeremonyLabelFillColor);
				FrontendText_DrawCentered(15, g_frontendScratchBuffer, &rect, g_colorLightBlue);
			}

			if (!FrontendWaveStream_IsPlaying() && g_awardCeremonyFrameCounter > 240) {
				if (g_frontendMission->header.hangar != XWA_HANGAR_QUICKSTART &&
					g_frontendMission->header.hangar != XWA_HANGAR_SKIRMISH) {
					sscanf(g_missionList[g_selectedMissionListIndex].fileName, "%d%c%d%c%d", &parsedFirst,
						   &separator, &battleNumber, &separator, &missionNumber);
					if (battleNumber >= 0 && battleNumber <= 99 && missionNumber >= 0 &&
						missionNumber <= 99) {
						sprintf(waveName, "%2d%2d%2d", battleNumber, missionNumber, 1);
						if (waveName[0] == ' ') {
							waveName[0] = '0';
						}
						if (waveName[2] == ' ') {
							waveName[2] = '0';
						}
						if (waveName[4] == ' ') {
							waveName[4] = '0';
						}

						if (g_missionOutcome == 1) {
							sprintf(g_frontendScratchBuffer, "wave\\frontend\\B%dM%d\\W%s.wav", battleNumber,
									missionNumber, waveName);
						} else {
							sprintf(g_frontendScratchBuffer, "wave\\frontend\\B%dM%d\\L%s.wav", battleNumber,
									missionNumber, waveName);
						}
						parsedFirst = FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
						if (parsedFirst) {
							strcpy(g_pendingVoiceWav, g_frontendScratchBuffer);
						} else {
							g_pendingVoiceWav[0] = '\0';
						}
					}
				}

				FrontendText_ResetGlyphScratchBuffer(20);
				FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				g_pendingAward = 0;
				g_awardCeremonyState = 2;
				FrontImage_DrawSpriteOpaque("background", 0, 0);
				FrontendDisplay_LockOffscreenSurface();
				FrontImage_DrawSpriteOpaque("background", 0, 0);
				FrontendDisplay_UnlockOffscreenSurface(1);
				FrontendText_SetGlyphGradientBg(g_colorNearBlack);
			}

			++g_awardCeremonyFrameCounter;
			break;

		case 0:
			awardId = g_pendingAward & 0x7fffffff;

			FrontImage_RegisterResourceDefault("frontres\\medals\\medalcase.bmp", "cerecase");
			g_awardCeremonyLabelFillColor = FrontendDisplay_PackRGB(0x60, 0x0c, 0x0c);
			FrontendText_SetGlyphGradientBg(g_awardCeremonyLabelFillColor);
#ifdef XWA_MODERN
			waveName[0] = '\0';
#endif

			if (awardId != 0) {
				sprintf(g_frontendScratchBuffer, "lbattlezoom%d", awardId - 1);
				FrontImage_SetSpriteFrame(g_frontendScratchBuffer, 0);
				sprintf(g_frontendScratchBuffer, "lkalidorzoom%d", g_pilotData.kalidorCresent - 1);
				FrontImage_SetSpriteFrame(g_frontendScratchBuffer, 0);

				if (g_frontendMission->header.briefingOfficer == 0) {
					sprintf(waveName, "G3DE0%2d.wav", awardId);
					if (waveName[5] == ' ') {
						waveName[5] = '0';
					}
				} else if (g_frontendMission->header.briefingOfficer == 1) {
					sprintf(waveName, "G3KU0%2d.wav", awardId);
					if (waveName[5] == ' ') {
						waveName[5] = '0';
					}
				} else if (g_frontendMission->header.briefingOfficer == 2 ||
						   g_frontendMission->header.briefingOfficer == 3) {
					sprintf(waveName, "G3ZL0%2d.wav", awardId);
					if (waveName[5] == ' ') {
						waveName[5] = '0';
					}
				}
			} else {
				sprintf(g_frontendScratchBuffer, "lkalidorzoom%d", g_pilotData.kalidorCresent - 1);
				FrontImage_SetSpriteFrame(g_frontendScratchBuffer, 0);

				if (g_frontendMission->header.briefingOfficer == 0) {
					sprintf(waveName, "G3DE0%2d.wav", g_pilotData.kalidorCresent + 7);
					if (waveName[5] == ' ') {
						waveName[5] = '0';
					}
				} else if (g_frontendMission->header.briefingOfficer == 1) {
					sprintf(waveName, "G3KU0%2d.wav", g_pilotData.kalidorCresent + 7);
					if (waveName[5] == ' ') {
						waveName[5] = '0';
					}
				} else if (g_frontendMission->header.briefingOfficer == 2 ||
						   g_frontendMission->header.briefingOfficer == 3) {
					sprintf(waveName, "G3ZL0%2d.wav", g_pilotData.kalidorCresent + 7);
					if (waveName[5] == ' ') {
						waveName[5] = '0';
					}
				}
			}

			sprintf(g_frontendScratchBuffer, "wave\\frontend\\MEDALANDPROMOTION\\%s", waveName);
			parsedFirst = FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
#ifdef XWA_MODERN
			if (Movie_Play("ceremony", 1)) {
				g_awardCeremonyState = 3;
				break;
			}
			MissionDebrief_FinishAwardCeremonyMovie();
			break;

		case 3:
			MissionDebrief_FinishAwardCeremonyMovie();
#else
			parsedFirst = Movie_Play("ceremony", 1);
			FrontendDisplay_ClearBackBuffer();
			FrontendDisplay_ClearOffscreenSurface();
			FrontImage_DrawSpriteOpaque("cerecase", 0, 0);
			FrontendDisplay_LockOffscreenSurface();
			FrontImage_DrawSpriteOpaque("cerecase", 0, 0);
			FrontendDisplay_UnlockOffscreenSurface(1);
			FrontImage_FreeResourceByName("cerecase");
			g_awardCeremonyState = 1;
			g_awardCeremonyFrameCounter = 0;
#endif
			break;
	}

	if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick() || Keyboard_BufferContains(27) ||
		Keyboard_BufferContains(32) || Keyboard_BufferContains(13) || Keyboard_BufferContains(8)) {
		if (g_frontendMission->header.hangar != XWA_HANGAR_QUICKSTART &&
			g_frontendMission->header.hangar != XWA_HANGAR_SKIRMISH) {
			sscanf(g_missionList[g_selectedMissionListIndex].fileName, "%d%c%d%c%d", &parsedFirst, &separator,
				   &battleNumber, &separator, &missionNumber);
			if (battleNumber >= 0 && battleNumber <= 99 && missionNumber >= 0 && missionNumber <= 99) {
				sprintf(waveName, "%2d%2d%2d", battleNumber, missionNumber, 1);
				if (waveName[0] == ' ') {
					waveName[0] = '0';
				}
				if (waveName[2] == ' ') {
					waveName[2] = '0';
				}
				if (waveName[4] == ' ') {
					waveName[4] = '0';
				}

				if (g_missionOutcome == 1) {
					sprintf(g_frontendScratchBuffer, "wave\\frontend\\B%dM%d\\W%s.wav", battleNumber,
							missionNumber, waveName);
				} else {
					sprintf(g_frontendScratchBuffer, "wave\\frontend\\B%dM%d\\L%s.wav", battleNumber,
							missionNumber, waveName);
				}
				parsedFirst = FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
				if (parsedFirst) {
					strcpy(g_pendingVoiceWav, g_frontendScratchBuffer);
				} else {
					g_pendingVoiceWav[0] = '\0';
				}
			}
		}

		FrontendText_ResetGlyphScratchBuffer(20);
		FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		g_pendingAward = 0;
		g_awardCeremonyState = 2;
		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontendDisplay_LockOffscreenSurface();
		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontendDisplay_UnlockOffscreenSurface(1);
		FrontendText_SetGlyphGradientBg(g_colorNearBlack);
	}

	return 1;
}

// FUNCTION: XWA 0x582ED0
// Builds the debrief leaderboards and computes the mission outcome. Flags which
// teams have a human player, then insertion-sorts team ids, player ids, and the
// kills-on/kills-from ranking ids by mission score, completion state, and kill
// counts. Sets g_missionOutcome (0=loss/consequences, 1=win/results,
// 2=multiplayer-other) and announces a new family award on a win.
int MissionDebrief_Prepare(void) {
	unsigned int i;
	unsigned int candidate;
	int existing;
	unsigned int sortIndex;
	unsigned int playerSlot;
	int rankAdjust;
	int bestScore;
	unsigned int completedTeamCount;
	int outcome;

	memset(g_debriefTeamHasPlayer, 0, sizeof(g_debriefTeamHasPlayer));
	memset(g_debriefTeamUseSummaryRows, 0, sizeof(g_debriefTeamUseSummaryRows));
	i = 0;
	g_debriefActiveTeamCount = i;
	g_debriefLocalTeamRankIndex = i;
	g_missionOutcome = 1;

	// Flag each team that fields at least one human player.
	if (g_frontendMission->header.hangar == XWA_HANGAR_SKIRMISH) {
		for (; i < g_frontendMission->flightGroupCount; ++i) {
			if (g_frontendMission->flightGroups[i].playerNumber) {
				if (!g_debriefTeamHasPlayer[g_frontendMission->flightGroups[i].team]) {
					++g_debriefActiveTeamCount;
				}
				g_debriefTeamHasPlayer[g_frontendMission->flightGroups[i].team] = 1;
			}
		}
	} else {
		for (i = 0; i < 8; ++i) {
			if (g_pilotData.networkPlayers[i].directPlayId) {
				if (!g_debriefTeamHasPlayer
						[g_frontendMission->flightGroups[g_pilotData.networkPlayers[i].flightGroupId].team]) {
					++g_debriefActiveTeamCount;
				}
				g_debriefTeamHasPlayer
					[g_frontendMission->flightGroups[g_pilotData.networkPlayers[i].flightGroupId].team] = 1;
			}
		}
	}

	memset(g_debriefSortedTeamIds, 0xFF, sizeof(g_debriefSortedTeamIds));
	memset(g_debriefKillsFromRankIds, 0xFF, sizeof(g_debriefKillsFromRankIds));
	memset(g_debriefKillsOnRankIds, 0xFF, sizeof(g_debriefKillsOnRankIds));
	memset(g_debriefSortedPlayerIds, 0xFF, sizeof(g_debriefSortedPlayerIds));

	// Junkyard time-trial: give active teams that never finished a sentinel time.
	if (g_frontendMission->header.hangar == XWA_HANGAR_JUNKYARD) {
		for (i = 0; i < 10; ++i) {
			if (g_debriefTeamHasPlayer[i] && g_pilotData.teamsStatistics[i].missionTime == 0) {
				g_pilotData.teamsStatistics[i].missionTime = 215999;
			}
		}
	}

	// Insertion-sort the team ids into g_debriefSortedTeamIds, best rank first.
	for (i = 0; i < 10; ++i) {
		candidate = i;
		if (!g_debriefTeamHasPlayer[i]) {
			continue;
		}
		for (sortIndex = 0; sortIndex < 10; ++sortIndex) {
			existing = g_debriefSortedTeamIds[sortIndex];
			if (existing == -1) {
				g_debriefSortedTeamIds[sortIndex] = candidate;
				break;
			}
			if (g_frontendMission->header.hangar == XWA_HANGAR_QUICKSTART) {
				if (g_pilotData.teamsStatistics[candidate].missionScore >
					g_pilotData.teamsStatistics[existing].missionScore) {
					g_debriefSortedTeamIds[sortIndex] = candidate;
					candidate = existing;
				}
			} else if (g_frontendMission->header.hangar == XWA_HANGAR_JUNKYARD) {
				if (g_pilotData.teamsStatistics[existing].isMissionCompleted) {
					if (g_pilotData.teamsStatistics[candidate].missionTime <
						g_pilotData.teamsStatistics[existing].missionTime) {
						g_debriefSortedTeamIds[sortIndex] = candidate;
						candidate = existing;
					}
				} else if (g_pilotData.teamsStatistics[candidate].isMissionCompleted == 1) {
					g_debriefSortedTeamIds[sortIndex] = candidate;
					candidate = existing;
				} else if (g_pilotData.teamsStatistics[candidate].isMissionCompleted) {
					if (g_pilotData.teamsStatistics[candidate].missionTime <
						g_pilotData.teamsStatistics[existing].missionTime) {
						g_debriefSortedTeamIds[sortIndex] = candidate;
						candidate = existing;
					}
				} else {
					if (g_pilotData.teamsStatistics[candidate].missionScore >
						g_pilotData.teamsStatistics[existing].missionTime) {
						g_debriefSortedTeamIds[sortIndex] = candidate;
						candidate = existing;
					}
				}
			} else if (g_frontendMission->header.hangar == XWA_HANGAR_SKIRMISH) {
				if (g_gameConfig.goalType) {
					if (g_pilotData.teamsStatistics[candidate].missionScore >
						g_pilotData.teamsStatistics[existing].missionScore) {
						g_debriefSortedTeamIds[sortIndex] = candidate;
						candidate = existing;
					}
				} else {
					int existingCompleted = g_pilotData.teamsStatistics[existing].isMissionCompleted;

					if (!((existingCompleted ||
						   g_pilotData.teamsStatistics[candidate].isMissionCompleted != 1) &&
						  ((existingCompleted == 1 &&
							!g_pilotData.teamsStatistics[candidate].isMissionCompleted) ||
						   g_pilotData.teamsStatistics[candidate].missionScore <=
							   g_pilotData.teamsStatistics[existing].missionScore))) {
						g_debriefSortedTeamIds[sortIndex] = candidate;
						candidate = existing;
					}
				}
			} else {
				int existingCompleted = g_pilotData.teamsStatistics[existing].isMissionCompleted;

				if (!((existingCompleted || g_pilotData.teamsStatistics[candidate].isMissionCompleted != 1) &&
					  ((existingCompleted == 1 &&
						!g_pilotData.teamsStatistics[candidate].isMissionCompleted) ||
					   g_pilotData.teamsStatistics[candidate].missionScore <=
						   g_pilotData.teamsStatistics[existing].missionScore))) {
					g_debriefSortedTeamIds[sortIndex] = candidate;
					candidate = existing;
				}
			}
		}
	}

	// Record where the local player's team landed in the ranking.
	for (i = 0; i < 10; ++i) {
		if (g_debriefTeamHasPlayer[i] && g_debriefSortedTeamIds[i] == g_pilotData.team) {
			g_debriefLocalTeamRankIndex = i;
			break;
		}
	}

	// Junkyard: spread the sentinel time across ranks so scores break ties.
	rankAdjust = 8;
	bestScore = 0;
	for (i = 0; i < 10; ++i) {
		int team = g_debriefSortedTeamIds[i];

		if (
#ifdef XWA_MODERN
			team != -1 &&
#endif
			g_debriefTeamHasPlayer[team] && g_pilotData.teamsStatistics[team].missionTime == 215999) {
			g_pilotData.teamsStatistics[team].missionTime -= rankAdjust;
			if (g_pilotData.teamsStatistics[team].missionScore > bestScore) {
				bestScore = g_pilotData.teamsStatistics[team].missionScore;
				--rankAdjust;
			}
		}
	}

	// Insertion-sort the active network players into g_debriefSortedPlayerIds.
	for (i = 0; i < 8; ++i) {
		candidate = i;
		if (!g_pilotData.networkPlayers[i].directPlayId) {
			continue;
		}
		for (sortIndex = 0; sortIndex < 8; ++sortIndex) {
			existing = g_debriefSortedPlayerIds[sortIndex];
			if (existing == -1) {
				g_debriefSortedPlayerIds[sortIndex] = candidate;
				break;
			}
			if (g_pilotData
						.teamsStatistics
							[g_frontendMission
								 ->flightGroups[g_pilotData.networkPlayers[candidate].flightGroupId]
								 .team]
						.isMissionCompleted == 1 &&
				!g_pilotData
					 .teamsStatistics[g_frontendMission
										  ->flightGroups[g_pilotData.networkPlayers[existing].flightGroupId]
										  .team]
					 .isMissionCompleted) {
				g_debriefSortedPlayerIds[sortIndex] = candidate;
				candidate = existing;
			} else if (g_pilotData
						   .teamsStatistics
							   [g_frontendMission
									->flightGroups[g_pilotData.networkPlayers[candidate].flightGroupId]
									.team]
						   .isMissionCompleted ||
					   g_pilotData
							   .teamsStatistics
								   [g_frontendMission
										->flightGroups[g_pilotData.networkPlayers[existing].flightGroupId]
										.team]
							   .isMissionCompleted != 1) {
				if (!g_frontendMission->header.hangar) {
					if (g_pilotData
							.teamsStatistics
								[g_frontendMission
									 ->flightGroups[g_pilotData.networkPlayers[candidate].flightGroupId]
									 .team]
							.missionTime <
						g_pilotData
							.teamsStatistics
								[g_frontendMission
									 ->flightGroups[g_pilotData.networkPlayers[existing].flightGroupId]
									 .team]
							.missionTime) {
						g_debriefSortedPlayerIds[sortIndex] = candidate;
						candidate = existing;
					}
				} else {
					if (g_pilotData.networkPlayers[candidate].totalScore >
						g_pilotData.networkPlayers[existing].totalScore) {
						g_debriefSortedPlayerIds[sortIndex] = candidate;
						candidate = existing;
					}
				}
			}
		}
	}

	// Rank network players by the kills they scored.
	for (playerSlot = 0; playerSlot < 8; ++playerSlot) {
		candidate = playerSlot;
		if (g_pilotData.networkPlayers[playerSlot].directPlayId) {
			for (sortIndex = 0; sortIndex < 8; ++sortIndex) {
				existing = g_debriefKillsOnRankIds[sortIndex];
				if (existing == -1) {
					g_debriefKillsOnRankIds[sortIndex] = candidate;
					break;
				}
				if ((unsigned int)g_pilotData.killsFullOnPlayer[candidate] >
					(unsigned int)g_pilotData.killsFullOnPlayer[existing]) {
					g_debriefKillsOnRankIds[sortIndex] = candidate;
					candidate = existing;
				}
			}
		}
	}

	// Quick-start: fold AI flight groups (rank id = fg + 8) into the kills-on
	// ranking, comparing full kills then shared kills. Player ranks use the
	// per-player arrays; flight-group ranks use the contiguous per-fg arrays.
	if (g_frontendMission->header.hangar == XWA_HANGAR_QUICKSTART) {
		for (i = 0; i < g_frontendMission->flightGroupCount; ++i) {
			candidate = i + 8;
			if (!g_frontendMission->flightGroups[i].playerNumber) {
				continue;
			}
			for (playerSlot = 0; playerSlot < 8; ++playerSlot) {
				if (g_pilotData.networkPlayers[playerSlot].directPlayId &&
					g_pilotData.networkPlayers[playerSlot].flightGroupId == i) {
					break;
				}
			}
			if (playerSlot != 8) {
				continue;
			}
			for (sortIndex = 0; sortIndex < 8; ++sortIndex) {
				unsigned int candFull;
				unsigned int candShared;
				unsigned int existingFull;
				unsigned int existingShared;

				existing = g_debriefKillsOnRankIds[sortIndex];
				if (existing == -1) {
					g_debriefKillsOnRankIds[sortIndex] = candidate;
					break;
				}
				if (candidate < 8) {
					candFull = g_pilotData.killsFullOnPlayer[candidate];
					candShared = g_pilotData.killsSharedOnPlayer[candidate];
				} else {
					candFull = g_pilotData.killsFullOnFlightGroup[candidate - 8];
					candShared = g_pilotData.killsSharedOnFlightGroup[candidate - 8];
				}
				if (existing < 8) {
					existingFull = g_pilotData.killsFullOnPlayer[existing];
					existingShared = g_pilotData.killsSharedOnPlayer[existing];
				} else {
					existingFull = g_pilotData.killsFullOnFlightGroup[existing - 8];
					existingShared = g_pilotData.killsSharedOnFlightGroup[existing - 8];
				}
				if (candFull > existingFull || (candFull == existingFull && candShared > existingShared)) {
					g_debriefKillsOnRankIds[sortIndex] = candidate;
					candidate = existing;
				}
			}
		}
	}

	// Rank network players by the kills scored against them.
	for (playerSlot = 0; playerSlot < 8; ++playerSlot) {
		candidate = playerSlot;
		if (g_pilotData.networkPlayers[playerSlot].directPlayId) {
			for (sortIndex = 0; sortIndex < 8; ++sortIndex) {
				existing = g_debriefKillsFromRankIds[sortIndex];
				if (existing == -1) {
					g_debriefKillsFromRankIds[sortIndex] = candidate;
					break;
				}
				if ((unsigned int)g_pilotData.killsFullFromPlayer[candidate] >
					(unsigned int)g_pilotData.killsFullFromPlayer[existing]) {
					g_debriefKillsFromRankIds[sortIndex] = candidate;
					candidate = existing;
				}
			}
		}
	}

	// Quick-start: fold AI flight groups into the kills-from ranking likewise.
	if (g_frontendMission->header.hangar == XWA_HANGAR_QUICKSTART) {
		for (i = 0; i < g_frontendMission->flightGroupCount; ++i) {
			candidate = i + 8;
			if (!g_frontendMission->flightGroups[i].playerNumber) {
				continue;
			}
			for (playerSlot = 0; playerSlot < 8; ++playerSlot) {
				if (g_pilotData.networkPlayers[playerSlot].directPlayId &&
					g_pilotData.networkPlayers[playerSlot].flightGroupId == i) {
					break;
				}
			}
			if (playerSlot != 8) {
				continue;
			}
			for (sortIndex = 0; sortIndex < 8; ++sortIndex) {
				unsigned int candFull;
				unsigned int candShared;
				unsigned int existingFull;
				unsigned int existingShared;

				existing = g_debriefKillsFromRankIds[sortIndex];
				if (existing == -1) {
					g_debriefKillsFromRankIds[sortIndex] = candidate;
					break;
				}
				if (candidate < 8) {
					candFull = g_pilotData.killsFullFromPlayer[candidate];
					candShared = g_pilotData.killsSharedFromPlayer[candidate];
				} else {
					candFull = g_pilotData.killsFullFromFlightGroup[candidate - 8];
					candShared = g_pilotData.killsSharedFromFlightGroup[candidate - 8];
				}
				if (existing < 8) {
					existingFull = g_pilotData.killsFullFromPlayer[existing];
					existingShared = g_pilotData.killsSharedFromPlayer[existing];
				} else {
					existingFull = g_pilotData.killsFullFromFlightGroup[existing - 8];
					existingShared = g_pilotData.killsSharedFromFlightGroup[existing - 8];
				}
				if (candFull > existingFull || (candFull == existingFull && candShared > existingShared)) {
					g_debriefKillsFromRankIds[sortIndex] = candidate;
					candidate = existing;
				}
			}
		}
	}

	// Rank by individual pilot when every team is a single flight group, and
	// always for the junkyard.
	g_debriefRankByPilot = 0;
	if (g_frontendMission->header.hangar == XWA_HANGAR_QUICKSTART) {
		for (i = 0; i < g_teamCount; ++i) {
			if (g_teamFgCountScratch[i] > 1) {
				break;
			}
		}
		if (i == g_teamCount) {
			g_debriefRankByPilot = 1;
		}
	} else if (g_frontendMission->header.hangar == XWA_HANGAR_JUNKYARD) {
		g_debriefRankByPilot = 1;
	}

	// Compute the mission outcome (unless goals are unimportant or quick-start).
	if (!g_frontendMission->header.goalsUnimportant &&
		g_frontendMission->header.hangar != XWA_HANGAR_QUICKSTART) {
		completedTeamCount = 0;
		for (i = 0; i < 10; ++i) {
			if (g_pilotData.teamsStatistics[i].isMissionCompleted == 1) {
				++completedTeamCount;
			}
		}
		if (completedTeamCount && completedTeamCount <= 1) {
			outcome = g_pilotData.teamsStatistics[g_pilotData.team].isMissionCompleted != 0;
		} else {
			if (Net_GetPlayerCount() > 1) {
				if (!g_frontendMission->header.hangar) {
					outcome = g_pilotData.teamsStatistics[g_pilotData.team].missionTime ==
								  g_pilotData.teamsStatistics[g_debriefSortedTeamIds[0]].missionTime &&
							  g_pilotData.teamsStatistics[g_pilotData.team].isMissionCompleted == 1;
				} else {
					outcome = 2;
				}
			} else {
				outcome = 0;
			}
		}
		g_missionOutcome = outcome;
	}

	if (g_missionOutcome == 1 && g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
		g_pilotData.emkayAnnounceNewAward = 1;
	}
	return 1;
}

// FUNCTION: XWA 0x583840
// Builds the localized debrief text from the trailing text block of the current
// mission file. The file's last 8 KB (or 12 KB when useWinText) holds the
// '$'-separated debrief lines; each line is keyed and resolved through the
// localization dictionary. useWinText assembles the '!W<b><m><n>!' win lines into
// outResults; otherwise the '!L...' loss lines go to outResults and the trailing
// '!<file>_L00!' consequences text goes to outHints. '[..]' colour markup is then
// converted to the in-band escape bytes (start = 0x06, end = 0x01). Callers pass
// g_briefingText and g_hintsText.
//
// Port note: the original opened and read the mission file through the C runtime
// (fopen/fseek/fread); this port routes the same accesses through the Aeron VFS
// File_* API, so the file-I/O call sites diverge from the original instructions.
void MissionDebrief_BuildText(char* outResults, char* outHints, int useWinText) {
	char baseName[128];
	char fileBuffer[4096];
	char textBuffer[4096];
	char lineBuffer[4096];
	XwaFile* stream;
	int targetMissionId;
	unsigned int missionIndex;
	int extPos;
	int firstNumber;
	int secondNumber;
	int16_t fileMarker;
	int pos;
	int lineLen;
	int lineNumber;
	int i;
	int j;

	if (outResults == NULL || outHints == NULL) {
		return;
	}

	memset(outResults, 0, 0x1000);
	memset(outHints, 0, 0x1000);
	memset(fileBuffer, 0, sizeof(fileBuffer));

	// Locate the current mission within the loaded mission list.
	targetMissionId = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
	for (missionIndex = 0; missionIndex < (unsigned int)g_missionCount; ++missionIndex) {
		if (g_missionList[missionIndex].missionIdx == targetMissionId) {
			break;
		}
	}
	if (missionIndex >= (unsigned int)g_missionCount) {
		return;
	}

	sprintf(g_frontendScratchBuffer, "%s\\%s", g_campaignDirNames[g_pilotData.missionDirectoryId],
			g_missionList[missionIndex].fileName);
	stream = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "rb");
	if (stream == NULL) {
		return;
	}

	// Strip the extension from the file name to form the localization key prefix.
	strcpy(baseName, g_missionList[missionIndex].fileName);
	extPos = (int)strlen(baseName) - 1;
	if (extPos > 0) {
		while (baseName[extPos] != '.') {
			if (--extPos <= 0) {
				break;
			}
		}
		if (extPos > 0) {
			baseName[extPos] = 0;
		}
	}

	// The file name encodes the two numbers used to build the per-line keys; the
	// interleaved characters are parsed into scratch and discarded.
	sscanf(g_missionList[missionIndex].fileName, "%c%c%d%c%d", &fileBuffer[0], &fileBuffer[1], &firstNumber,
		   &fileBuffer[2], &secondNumber);

	if (g_pilotData.missionDirectoryId != 2 && g_frontendMission->header.hangar != XWA_HANGAR_SKIRMISH) {
		File_ReadWord(stream, &fileMarker);
		if (fileMarker == 18) {
			if (useWinText) {
				File_Seek(stream, -12288, SEEK_END);
				File_ReadCount(stream, fileBuffer, 4096 - extPos);
				fileBuffer[4095] = 0;
				if (fileBuffer[0]) {
					strcpy(textBuffer, fileBuffer);
					*outResults = 0;
					pos = 0;
					for (lineNumber = 1;; ++lineNumber) {
						while (pos < 4096 && textBuffer[pos] == '$') {
							++pos;
						}
						lineLen = 0;
						while (pos < 4096) {
							char ch = textBuffer[pos];
							if (ch == 0 || ch == '$') {
								break;
							}
							++pos;
							lineBuffer[lineLen++] = ch;
						}
						lineBuffer[lineLen] = 0;
						sprintf(fileBuffer, "!W0%d0%d0%d!", (unsigned char)firstNumber,
								(unsigned char)secondNumber, lineNumber);
						strcat(fileBuffer, lineBuffer);
						strcat(outResults, Linez_ResolveString(fileBuffer));
						if (pos >= 4096 || textBuffer[pos] == 0) {
							break;
						}
						strcat(outResults, "$$");
					}
				}
			} else {
				File_Seek(stream, -8192, SEEK_END);
				File_ReadCount(stream, fileBuffer, 4096 - extPos);
				fileBuffer[4095] = 0;
				if (fileBuffer[0]) {
					memset(textBuffer, 0, sizeof(textBuffer));
					strcpy(textBuffer, fileBuffer);
					*outResults = 0;

					// The loss text is delimited from the consequences text by '#'.
					pos = 0;
					while (pos < 4096 && textBuffer[pos] != '#') {
						++pos;
					}
					if (pos < 4096) {
						textBuffer[pos] = 0;
					}

					pos = 0;
					for (lineNumber = 1;; ++lineNumber) {
						while (pos < 4096 && textBuffer[pos] == '$') {
							++pos;
						}
						lineLen = 0;
						while (pos < 4096) {
							char ch = textBuffer[pos];
							if (ch == 0 || ch == '$') {
								break;
							}
							++pos;
							lineBuffer[lineLen++] = ch;
						}
						lineBuffer[lineLen] = 0;
						sprintf(fileBuffer, "!L0%d0%d0%d!", (unsigned char)firstNumber,
								(unsigned char)secondNumber, lineNumber);
						strcat(fileBuffer, lineBuffer);
						strcat(outResults, Linez_ResolveString(fileBuffer));
						if (pos >= 4096 || textBuffer[pos] == 0) {
							break;
						}
						strcat(outResults, "$$");
					}

					// Resolve the consequences block (past the loss text) into outHints.
					++pos;
					*outHints = 0;
					if (textBuffer[pos]) {
						while (pos < 4096 && textBuffer[pos] == '$') {
							++pos;
						}
						sprintf(fileBuffer, "!%s_L00!", baseName);
						strcat(fileBuffer, &textBuffer[pos]);
						strcat(outHints, Linez_ResolveString(fileBuffer));
					}
				}
			}

			// Convert '[..]' colour markup to the in-band escape bytes in both
			// output buffers: '[' becomes 0x06 and ']' is dropped, shifting the
			// following byte left and marking the end with 0x01.
			for (i = 0; i < 4096; ++i) {
				if (outResults[i] == '[') {
					outResults[i] = 6;
					j = i;
					while (outResults[j] != ']') {
						if (++j >= 4096) {
							break;
						}
					}
					if (j < 4096) {
						outResults[j] = outResults[j + 1];
						outResults[j + 1] = 1;
					}
				}
			}
			for (i = 0; i < 4096; ++i) {
				if (outHints[i] == '[') {
					outHints[i] = 6;
					j = i;
					while (outHints[j] != ']') {
						if (++j >= 4096) {
							break;
						}
					}
					if (j < 4096) {
						outHints[j] = outHints[j + 1];
						outHints[j + 1] = 1;
					}
				}
			}
		}
	}

	File_Close(stream);
}

// FUNCTION: XWA 0x583EE0
// Draws the debrief 'Mission Debriefing' tab: a centered reveal title above the
// word-wrapped results/consequences body. The title reads 'Mission Results' on a
// win and 'Mission Consequences' otherwise. When the wrapped body overflows the
// page a vertical scrollbar is drawn and the body is narrowed to leave room for it.
int MissionDebrief_DrawMissionResultsPage(void) {
	FrontendRect rect;
	int lineCount;

	if (g_briefingText == NULL) {
		return 0;
	}

	FrontendDraw_RectAssign(&rect, 65, 90, 575, 106);

	// Suppressed for quick-start launches and missions whose goals are flagged
	// unimportant; in that case only the shared reveal counter advances.
	if (!g_frontendMission->header.goalsUnimportant &&
		g_frontendMission->header.hangar != XWA_HANGAR_QUICKSTART) {
		// 'Mission Results' only for a clean single-player win (outcome 1); a loss
		// (0) or multiplayer-other result (2) shows 'Mission Consequences'.
		if (g_missionOutcome != 2 && g_missionOutcome == 1) {
			FrontendText_DrawCenteredReveal(12, FrontendString_Get(STR_MISSION_RESULTS), &rect,
											g_colorLightBlue, g_debriefTextRevealFrame);
		} else {
			FrontendText_DrawCenteredReveal(12, FrontendString_Get(STR_MISSION_CONSEQUENCES), &rect,
											g_colorLightBlue, g_debriefTextRevealFrame);
		}

		// Measure the wrapped body at full width: a firstVisibleLine past the end
		// draws nothing, so the call only returns the total line count.
		FrontendDraw_RectAssign(&rect, 70, 111, 570, 415);
		lineCount = FrontendText_DrawWrappedClipped(12, g_briefingText, &rect, g_colorLightBlue, 4, 4096) + 1;
		if (lineCount > 19) {
			// Re-measure at the narrower width left by the scrollbar, then draw it.
			FrontendDraw_RectAssign(&rect, 70, 111, 550, 415);
			lineCount =
				FrontendText_DrawWrappedClipped(12, g_briefingText, &rect, g_colorLightBlue, 4, 4096) + 1;
			FrontendDraw_RectAssign(&rect, 551, 111, 570, 415);
			g_frontendFirstVisibleLine =
				FrontendScrollbar_Draw(&rect, g_frontendFirstVisibleLine, lineCount, 0, 5, g_colorNavy, 9);
			FrontendDraw_RectAssign(&rect, 70, 111, 550, 415);
		} else {
			FrontendDraw_RectAssign(&rect, 70, 111, 570, 415);
		}

		FrontendText_DrawWrappedClippedEx(12, g_briefingText, &rect, g_colorLightBlue, 4,
										  g_frontendFirstVisibleLine, g_debriefTextRevealFrame);
	}

	++g_debriefTextRevealFrame;
	return 1;
}

// FUNCTION: XWA 0x5840C0
// Draws the debrief 'Mission Hints' tab: a centered reveal title above the
// word-wrapped hints body. When the wrapped body overflows the page a vertical
// scrollbar is drawn and the body is narrowed to leave room for it.
int MissionDebrief_DrawMissionHintsPage(void) {
	FrontendRect rect;
	int lineCount;

	if (g_hintsText == NULL) {
		return 0;
	}

	FrontendDraw_RectAssign(&rect, 65, 90, 575, 106);

	// Suppressed for quick-start launches and missions whose goals are flagged
	// unimportant; in that case only the shared reveal counter advances.
	if (!g_frontendMission->header.goalsUnimportant &&
		g_frontendMission->header.hangar != XWA_HANGAR_QUICKSTART) {
		FrontendText_DrawCenteredReveal(12, FrontendString_Get(STR_MISSION_HINTS), &rect, g_colorLightBlue,
										g_debriefTextRevealFrame);

		// Measure the wrapped body at full width: a firstVisibleLine past the end
		// draws nothing, so the call only returns the total line count.
		FrontendDraw_RectAssign(&rect, 70, 111, 570, 415);
		lineCount = FrontendText_DrawWrappedClipped(12, g_hintsText, &rect, g_colorLightBlue, 4, 4096) + 1;
		if (lineCount > 19) {
			// Re-measure at the narrower width left by the scrollbar, then draw it.
			FrontendDraw_RectAssign(&rect, 70, 111, 550, 415);
			lineCount =
				FrontendText_DrawWrappedClipped(12, g_hintsText, &rect, g_colorLightBlue, 4, 4096) + 1;
			FrontendDraw_RectAssign(&rect, 551, 111, 570, 415);
			g_debriefMissionHintsScrollOffset = FrontendScrollbar_Draw(
				&rect, g_debriefMissionHintsScrollOffset, lineCount, 0, 5, g_colorNavy, 11);
			FrontendDraw_RectAssign(&rect, 70, 111, 550, 415);
		} else {
			FrontendDraw_RectAssign(&rect, 70, 111, 570, 415);
		}

		FrontendText_DrawWrappedClippedEx(12, g_hintsText, &rect, g_colorLightBlue, 4,
										  g_debriefMissionHintsScrollOffset, g_debriefTextRevealFrame);
	}

	++g_debriefTextRevealFrame;
	return 1;
}
