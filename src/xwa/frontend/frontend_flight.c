#include "xwa/frontend/frontend_flight.h"

#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/config/pilot.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/concourse.h"
#include "xwa/frontend/film_room.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_file_stream.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/frontend_wave_stream.h"
#include "xwa/frontend/mission_briefing.h"
#include "xwa/frontend/mission_debrief.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/net_session.h"
#include "xwa/frontend/net_transport.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa_runtime/runtime/flight_task.h"
#include "xwa/xwa_options.h"

#include <stdio.h>
#include <string.h>

enum {
	FRONTEND_FLIGHT_STREAM_SLOT = 1,
	FRONTEND_FLIGHT_STREAM_TOTAL_BYTES = 1024000,
	FRONTEND_FLIGHT_STREAM_INITIAL_FILL = 512000,
};

// GLOBAL: XWA 0x783ED8
char cmdLine[256];
// GLOBAL: XWA 0x9EAB20
char g_filmFilePath[256];
// GLOBAL: XWA 0xABD7E0
PilotData g_pilotDataSnapshot;
static int g_frontendFlightPendingLaunch;
static int g_frontendFlightLowMemoryResourceReload;

static const char* FrontendFlight_GetLocalNetworkPilotName(void) {
	if (g_localPilotNetworkPlayerIndex < 0 || g_localPilotNetworkPlayerIndex >= 8) {
		return "";
	}

	return g_pilotData.networkPlayers[g_localPilotNetworkPlayerIndex].formalName;
}

static void FrontendFlight_UnloadLowMemoryResourceLists(void) {
	FrontImage_UnloadResourceList("frontres\\combat\\combat.lst");
	FrontImage_UnloadResourceList("frontres\\datapad\\top.lst");
	FrontImage_UnloadResourceList("frontres\\datapad\\awards.lst");
	FrontImage_UnloadResourceList("frontres\\icons\\icons.lst");
	FrontImage_UnloadResourceList("frontres\\skirmish\\skirmish.lst");
	FrontImage_UnloadResourceList("frontres\\family\\family.lst");
	FrontImage_UnloadResourceList("frontres\\cutscene\\cutscene.lst");
}

static void FrontendFlight_LoadLowMemoryResourceLists(void) {
	FrontImage_LoadResourceList("frontres\\combat\\combat.lst");
	FrontImage_LoadResourceList("frontres\\datapad\\top.lst");
	FrontImage_LoadResourceList("frontres\\datapad\\awards.lst");
	FrontImage_LoadResourceList("frontres\\icons\\icons.lst");
	FrontImage_LoadResourceList("frontres\\skirmish\\skirmish.lst");
	FrontImage_LoadResourceList("frontres\\family\\family.lst");
	FrontImage_LoadResourceList("frontres\\cutscene\\cutscene.lst");
}

static void FrontendFlight_SetMusicAndShowCursor(int state) {
	musicState = state;
	if (g_gameConfig.datapadMusicEnabled) {
		Music_SetState(state);
	} else {
		Music_Stop();
	}

	FrontendCursor_Show();
}

static void FrontendFlight_SetPostFlightCallbacks(FrontendScreenUpdateFn updateFn,
												  FrontendScreenExitFn exitFn) {
	FrontendScreen_SetCallbacks(updateFn, exitFn);
	/* FrontendFlight_CompleteLaunchSession runs from XwaPort, outside
	   FrontendScreen_RunFrame. The original SetCallbacks dirty bit is only
	   meaningful while replacing the current callback from inside that frame. */
	g_screenCallbacksDirty = 0;
	g_frameCounter = 0;
}

// FUNCTION: XWA 0x5710F0
int FrontendFlight_LaunchSession(int frameCounter) {
	if (!frameCounter) {
		g_frontendFlightPendingLaunch = 1;
	}

	return 0;
}

int FrontendFlight_HasPendingLaunch(void) { return g_frontendFlightPendingLaunch; }

int FrontendFlight_BeginPendingLaunch(void) {
	int playerCount;
	unsigned int missionListIndex;
	MissionListEntry* missionEntry;
	char missionPath[128];
	const char* pageFlipString;
	const char* fullscreenString;

	if (!g_frontendFlightPendingLaunch) {
		return 0;
	}
	g_frontendFlightPendingLaunch = 0;

	Config_Write();
	Pilot_Save(0);

	/* Port: original CD-drive probing/retry dialogs are replaced by the staged VFS asset root. */
	FrontendCursor_Hide();
	Frontend_MarkHostCdAvailable();

	DebugPrintf(0);
	FrontendWaveStream_Shutdown();
	/* Port: the original only unloads these lists on machines with <= 32 MB RAM. */
	g_frontendFlightLowMemoryResourceReload = 0;
	if (g_frontendFlightLowMemoryResourceReload) {
		FrontendFlight_UnloadLowMemoryResourceLists();
	}

	FrontImage_FreeAtlasResources();
	FrontendCursor_FreeResources();
	if (g_frontendDisplayLifecycleScratch) {
		Mem_Free(g_frontendDisplayLifecycleScratch);
		g_frontendDisplayLifecycleScratch = 0;
	}
	if (g_offscreenBackupBuffer) {
		Mem_Free(g_offscreenBackupBuffer);
		g_offscreenBackupBuffer = 0;
	}

	FrontendFileStream_FreeSlot(FRONTEND_FLIGHT_STREAM_SLOT);
	FrontendDisplay_FlipDirectDrawToGDISurface();
	FrontendDisplay_ReleaseSurfacesForFlight();
	FrontendSound_UnloadList("sfx\\sfx.lst");
	FrontendSound_ReleaseForFlight();
	playerCount = NetSession_GetPlayerCount();
	FrontendDisplay_SelectDriver(g_gameConfig.threedDevice[playerCount > 1]);
	FrontendDisplay_SelectActiveDirectDrawDevice();
	FrontendDisplay_SetWndProcMode(1);

	MissionSetup_LoadMissionList((MissionDirectoryId)g_pilotData.missionDirectoryId);
	memcpy(&g_pilotDataSnapshot, &g_pilotData, sizeof(g_pilotDataSnapshot));
	if (!g_missionList) {
		FrontendFlight_CompleteLaunchSession(0);
		return 0;
	}

	missionListIndex = 0;
	if (g_missionCount) {
		while (missionListIndex < (unsigned int)g_missionCount &&
			   g_missionList[missionListIndex].missionIdx !=
				   g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId]) {
			++missionListIndex;
		}
	}

	missionEntry = &g_missionList[missionListIndex];
	sprintf(missionPath, "%s\\%s", g_campaignDirNames[g_pilotData.missionDirectoryId],
			missionEntry->fileName);
	(void)missionPath;

	pageFlipString = g_optNoFullscreen ? "nopageflip" : "pageflip";
	fullscreenString = g_optNoFullscreen ? "nofullscreen" : "fullscreen";
	sprintf(cmdLine, "~%s\\%s~ ~%s~ ~%s~ %u ~%s~ 0 %u %s %s",
			g_campaignDirNames[g_pilotData.missionDirectoryId], missionEntry->fileName,
			FrontendFlight_GetLocalNetworkPilotName(), g_pilotData.name, (unsigned int)isHost,
			g_pilotData.multiplayerGameName, (unsigned int)g_frontendLaunchHumanPlayerCount, pageFlipString,
			fullscreenString);

	Mem_Free(g_missionList);
	g_missionList = 0;
	if (g_filmFilePath[0]) {
		memcpy(&g_pilotDataSnapshot, &g_pilotData, sizeof(g_pilotDataSnapshot));
	}

	if (!XwaFlightTask_Init(cmdLine, g_filmFilePath)) {
		if (g_filmFilePath[0]) {
			memcpy(&g_pilotData, &g_pilotDataSnapshot, sizeof(g_pilotData));
		}
		FrontendFlight_CompleteLaunchSession(0);
		return 0;
	}

	return 1;
}

void FrontendFlight_CompleteLaunchSession(int flightResult) {
	if (g_filmFilePath[0]) {
		memcpy(&g_pilotData, &g_pilotDataSnapshot, sizeof(g_pilotData));
	}

	if (g_frontendDisplayLifecycleScratch) {
		Mem_Free(g_frontendDisplayLifecycleScratch);
		g_frontendDisplayLifecycleScratch = 0;
	}
	g_frontendDisplayLifecycleScratch = Mem_Alloc((size_t)307200 * (size_t)(g_displayBpp >> 2));
	FrontendDisplay_SelectPrimaryDirectDrawDevice();
	FrontendDisplay_ReinitSurfaces();
	FrontendDisplay_SetWndProcMode(0);
	FrontendFileStream_InitSlotBuffer(FRONTEND_FLIGHT_STREAM_SLOT, FRONTEND_FLIGHT_STREAM_TOTAL_BYTES,
									  FRONTEND_FLIGHT_STREAM_INITIAL_FILL);
	FrontendSound_RecreateAfterFlight(0);
	FrontendSound_LoadList("sfx\\sfx.lst");
	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		DebugPrintf(0);
	}

	if (!g_gameConfig.use3dHardware[0]) {
		g_gameConfig.screenRes[0] = 0;
	}
	if (!g_gameConfig.use3dHardware[1]) {
		g_gameConfig.screenRes[1] = 0;
	}

	Music_Stop();
	Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
	Config_Write();
	strcpy(g_pilotData.pilotRatingName,
		   FrontendString_Get((UIString)(g_pilotData.pilotRating + STR_TARGET_DRONE)));
	Pilot_Save(0);
	if (g_frontendFlightLowMemoryResourceReload) {
		FrontendFlight_LoadLowMemoryResourceLists();
	}

	FrontImage_RebuildPaletteCache();
	FrontendColor_Init();
	FrontendText_SetGlyphGradientBg(g_colorNearBlack);
	FrontendCursor_LoadResources();
	FrontImage_InitAtlasSprites();

	if (!flightResult) {
		FrontendCursor_Show();
		Net_ShutdownDirectPlaySession();
		if (g_filmFilePath[0]) {
			g_filmFilePath[0] = '\0';
			g_skipFrontendEntryMovie = 1;
			FrontendFlight_SetPostFlightCallbacks(FilmRoom_Update, FilmRoom_Exit);
		} else {
			FrontendFlight_SetPostFlightCallbacks(Concourse_Update, Concourse_Exit);
		}
		FrontendCursor_Show();
		return;
	}

	if (flightResult == 2) {
		if (g_pilotData.campaignMode) {
			int savedValue;

			MissionDebrief_Prepare();
			if (g_missionOutcome != 1) {
				savedValue = g_pilotData.factionStatistics[1]
								 .stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29];
				memcpy(&g_pilotData, &g_pilotDataSnapshot, sizeof(g_pilotData));
				g_pilotData.factionStatistics[1].stats.killsPerCraftPerMT[1][12 * g_currentMissionId + 29] =
					savedValue;
			}
		}

		FrontendFlight_SetPostFlightCallbacks(Concourse_Update, Concourse_Exit);
		return;
	}

	if (flightResult != 3) {
		FrontendFlight_SetPostFlightCallbacks(MissionDebrief_Update, MissionDebrief_Exit);
		return;
	}

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
		int state;

		if (g_frontendMissionLoaded) {
			if ((unsigned int)g_currentMissionId >= 7u) {
				state = g_pilotData.campaignMode ? MUSIC_STATE_FRONTEND_1230 : MUSIC_STATE_FRONTEND_1240;
			} else {
				state = MUSIC_STATE_FRONTEND_1210;
			}
		} else {
			state = MUSIC_STATE_FRONTEND_1240;
		}

		if (g_frontendQuickStartLaunchFlag) {
			g_skipFrontendEntryMovie = 1;
			g_frontendQuickStartLaunchFlag = 0;
			FrontendFlight_SetPostFlightCallbacks(MissionSetup_Update, MissionSetup_Exit);
		} else {
			FrontendFlight_SetPostFlightCallbacks(MissionBriefing_Update, MissionBriefing_Exit);
		}
		FrontendFlight_SetMusicAndShowCursor(state);
		return;
	}

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE &&
		g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_MELEE] == 66) {
		FrontendFlight_SetPostFlightCallbacks(Concourse_Update, Concourse_Exit);
		FrontendFlight_SetMusicAndShowCursor(MUSIC_STATE_FRONTEND_1200);
		return;
	}

	g_skipFrontendEntryMovie = 1;
	if (g_pilotData.missionDirectoryId != MISSION_DIRECTORY_MELEE) {
		g_frontendQuickStartLaunchFlag = 0;
	}

	FrontendFlight_SetPostFlightCallbacks(MissionSetup_Update, MissionSetup_Exit);
	if (g_frontendMissionLoaded) {
		FrontendFlight_SetMusicAndShowCursor(
			(unsigned int)g_currentMissionId < 7u ? MUSIC_STATE_FRONTEND_1210 : MUSIC_STATE_FRONTEND_1240);
	} else {
		FrontendFlight_SetMusicAndShowCursor(MUSIC_STATE_FRONTEND_1240);
	}

	return;
}
