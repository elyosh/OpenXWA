#include "xwa/frontend/concourse.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif

#include "aeron/log.h"

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/sprite_resource.h"
#include "xwa/assets/string_table.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/config/pilot.h"
#include "xwa/frontend/briefing_room.h"
#include "xwa/frontend/combat_sim_menu.h"
#include "xwa/frontend/family_transport_room.h"
#include "xwa/frontend/film_room.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_dialog.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_email.h"
#include "xwa/frontend/frontend_escape.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/frontend_mission_list.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/frontend_wave_stream.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/frontend/skirmish.h"
#include "xwa/frontend/tech_library.h"
#include "xwa/net/net.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/xwa_options.h"
#include "xwa/util/time.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	CONCOURSE_BACKGROUND_GROUP_ID = 15001,
	CONCOURSE_BACKGROUND_ATLAS_BASE_INDEX = 3100,
	CONCOURSE_PLANET_GROUP_ID_BASE = 15009,
};

// GLOBAL: XWA 0xABC96C
extern int g_currentCdDisk;

// GLOBAL: XWA 0x7838A4
int g_concoursePlanetGroupId;
// GLOBAL: XWA 0x782FF4
int g_concourseMarkoHovered;
// GLOBAL: XWA 0x782FF0
int g_concourseIdleVoiceFrame;
// GLOBAL: XWA 0x782FEC
int g_concourseLabelColor;
// GLOBAL: XWA 0xABC980
int g_concourseStarX[128];
// GLOBAL: XWA 0xABCB80
int g_concourseStarY[128];
// GLOBAL: XWA 0xABCD80
int g_concourseStarColorIdx[128];
// GLOBAL: XWA 0xABCFA0
int g_concourseStarPeriod[128];
// GLOBAL: XWA 0xABD1A0
int16_t g_concourseStarPalette[31];

// FUNCTION: XWA 0x55DD10
int Concourse_LoadBackground(void) {
	SpriteResource_LoadGroup(CONCOURSE_BACKGROUND_GROUP_ID);
	FrontImage_RegisterAtlasSprite("concourse", CONCOURSE_BACKGROUND_GROUP_ID,
								   CONCOURSE_BACKGROUND_ATLAS_BASE_INDEX, 1);
	return 1;
}

// FUNCTION: XWA 0x55DD40
int Concourse_FreeBackground(void) {
	FrontImage_FreeResourceByName("concourse");
	SpriteResource_UnloadGroup(CONCOURSE_BACKGROUND_GROUP_ID);
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x55DCA0
int Concourse_LoadPlanetSprite(int planetId) {
	SpriteResource_LoadGroup(planetId + CONCOURSE_PLANET_GROUP_ID_BASE);
	g_concoursePlanetGroupId = planetId + CONCOURSE_PLANET_GROUP_ID_BASE;
	FrontImage_RegisterAtlasSprite("planet", planetId + CONCOURSE_PLANET_GROUP_ID_BASE, 1, 1);
	return 1;
}

// FUNCTION: XWA 0x55DCE0
int Concourse_FreePlanetSprite(void) {
	FrontImage_FreeResourceByName("planet");
	SpriteResource_UnloadGroup((int16_t)g_concoursePlanetGroupId);
	g_concoursePlanetGroupId = 0;
	return 1;
}

// FUNCTION: XWA 0x53AB60
int Concourse_DrawPlanet(void) {
	FrontendRect out;
	int width;
	int height;
	int x;
	int y;

	if (g_frontendMissionLoaded) {
		srand(43235 * g_currentMissionId + 3452938);
		FrontImage_GetResourceRect("planet", &out);
	} else {
		srand(13965 * g_pilotData.missionDirectoryId +
			  7345 * g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId]);
		FrontImage_GetResourceRect("planet", &out);
	}

	width = out.right - out.left + 1;
	height = out.bottom - out.top + 1;
	x = rand() % 640;
	y = rand() % 70 + 55;
	FrontImage_DrawSprite("planet", x - (width >> 1), y - (height >> 1));
	return 1;
}

// FUNCTION: XWA 0x53AC50
int Concourse_InitStarfield(void) {
	int drawSurfacePitch;
	int16_t* palette;
	int i;
	int colorValue;
	int starIndex;

	drawSurfacePitch = FrontendDisplay_GetDrawSurfacePitch();

	// Build the ascending half of the twinkle ramp: entries 0..15 brighten as
	// colorValue climbs from 64 by 12 each step.
	for (i = 0, colorValue = 64; i < 16; ++i, colorValue += 12) {
		g_concourseStarPalette[i] =
			(int16_t)FrontendDisplay_PackRGB((unsigned char)(colorValue - i - 17),
											 (unsigned char)(colorValue - i - 17), (unsigned char)colorValue);
	}

	// Mirror it into the descending half: entry 16+k copies entry 14-k, so the
	// 31-entry ramp peaks at entry 15 and falls back symmetrically.
	palette = &g_concourseStarPalette[16];
	for (i = -16; i > -31; --i) {
		*palette++ = g_concourseStarPalette[i + 30];
	}

	starIndex = 0;
	do {
		int x;
		int y;
		int colorIdx;
		int periodRand;

		x = rand() % 640;
		y = rand() % 100 + 25;
		if (*(uint16_t*)&g_drawSurfacePtr[2 * x + drawSurfacePitch * y] == 0) {
			g_concourseStarX[starIndex] = x;
			g_concourseStarY[starIndex] = y;
			colorIdx = rand() % 31;
			periodRand = rand();
			g_concourseStarColorIdx[starIndex] = colorIdx;
			g_concourseStarPeriod[starIndex] = periodRand % 8 + 2;
			++starIndex;
		}
	} while (starIndex < 128);

	return 1;
}

// FUNCTION: XWA 0x53AD50
int Concourse_DrawStarfield(int frameCounter) {
	int drawSurfacePitch;
	int i;

	drawSurfacePitch = FrontendDisplay_GetDrawSurfacePitch();
	for (i = 0; i < 128; ++i) {
		int period;

		*(uint16_t*)&g_drawSurfacePtr[2 * g_concourseStarX[i] + drawSurfacePitch * g_concourseStarY[i]] =
			(uint16_t)g_concourseStarPalette[g_concourseStarColorIdx[i]];
#ifdef XWA_MODERN
		/* Remaster snapshot observer: twinkling star pixel. */
		XwaSnapshot_EmitPaint(XWA_PAINT_PIXEL, g_concourseStarX[i], g_concourseStarY[i], g_concourseStarX[i],
							  g_concourseStarY[i], 0, 0,
							  (uint32_t)(uint16_t)g_concourseStarPalette[g_concourseStarColorIdx[i]]);
#endif
		period = g_concourseStarPeriod[i];
		if (period != 9 && frameCounter % 7 + 2 == period) {
			unsigned int colorIndex;

			g_concourseStarColorIdx[i] = g_concourseStarColorIdx[i] + 1;
			colorIndex = (unsigned int)g_concourseStarColorIdx[i];
			if (colorIndex > 30) {
				g_concourseStarColorIdx[i] = 0;
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x53AA90
int Concourse_LoadNextTourMission(void) {
	int missionDirectoryId;
	int missionDescriptionId;
	unsigned int missionListIndex;

	missionDirectoryId = g_pilotData.missionDirectoryId;
	g_pilotData.missionDirectoryId = MISSION_DIRECTORY_TOUR;
	missionDescriptionId = g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_TOUR];
	MissionSetup_LoadMissionList(MISSION_DIRECTORY_TOUR);
	if (g_missionList == NULL) {
		g_frontendMissionLoaded = 0;
	}

	for (missionListIndex = 0; missionListIndex < (unsigned int)g_missionCount; ++missionListIndex) {
		if (g_pilotData.tourOfDutyMissions[g_missionList[missionListIndex].missionIdx].completedCount == 0) {
			break;
		}
	}

	if (missionListIndex == (unsigned int)g_missionCount) {
		g_frontendMissionLoaded = 0;
	} else {
		g_currentMissionId = g_missionList[missionListIndex].missionIdx;
		g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] = g_currentMissionId;
#ifdef XWA_MODERN
		{
			int missionLoaded;

			missionLoaded = FrontendMission_LoadCurrent();
			if (!missionLoaded) {
				Aeron_Log("xwa.frontend", "Failed to load selected tour mission %d", g_currentMissionId);
			}
			g_frontendMissionLoaded = missionLoaded != 0;
		}
#else
		FrontendMission_LoadCurrent();
		g_frontendMissionLoaded = 1;
#endif
	}

	g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] = missionDescriptionId;
	g_pilotData.missionDirectoryId = missionDirectoryId;
	return 1;
}

// FUNCTION: XWA 0x53ADE0
int Concourse_PlayIdleVoice(int frameCounter) {
	(void)frameCounter;

	srand(GetTickCount());
	if (g_frontendMissionLoaded && g_frontendMission->header.hangar != XWA_HANGAR_FAMILYTRANSPORT) {
		int missionListIndex;
		int missionPart1;
		int missionPart2;
		int missionPart3;
		char separator;
		char buffer[12];

		for (missionListIndex = 0; missionListIndex < g_missionCount; ++missionListIndex) {
			if (g_missionList[missionListIndex].missionIdx == g_currentMissionId) {
				g_selectedMissionListIndex = missionListIndex;
				break;
			}
		}

		sscanf(g_missionList[missionListIndex].fileName, "%d%c%d%c%d", &missionPart1, &separator,
			   &missionPart2, &separator, &missionPart3);
		if (missionPart2 < 0 || missionPart2 > 99 || missionPart3 < 0 || missionPart3 > 99) {
			g_frontendScratchBuffer[0] = '\0';
		} else {
			sprintf(buffer, "%2d%2d%2d", missionPart2, missionPart3, 1);
			if (buffer[0] == ' ') {
				buffer[0] = '0';
			}

			if (buffer[2] == ' ') {
				buffer[2] = '0';
			}

			if (buffer[4] == ' ') {
				buffer[4] = '0';
			}

			sprintf(g_frontendScratchBuffer, "wave\\frontend\\T%s.wav", buffer);
		}

		if (g_frontendScratchBuffer[0] && FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0)) {
			return 1;
		}
	}

	if (!g_pilotData.newCraftAddedToTechRoom ||
		(sprintf(g_frontendScratchBuffer, "wave\\frontend\\T01PA0%d.wav", rand() % 2 + 1),
		 !FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0))) {
		switch (rand() % 24) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
				sprintf(g_frontendScratchBuffer, "wave\\frontend\\T01PA0%d.wav", rand() % 6 + 3);
				break;

			case 6:
			case 7:
			case 8:
				sprintf(g_frontendScratchBuffer, "wave\\frontend\\T01PA1%d.wav", rand() % 3 + 1);
				break;

			case 9:
			case 10:
				if (!g_frontendMissionLoaded) {
					strcpy(g_frontendScratchBuffer, "wave\\frontend\\T01PA15.wav");
				} else if (g_frontendMission->header.briefingLogo == 5) {
					strcpy(g_frontendScratchBuffer, "wave\\frontend\\T01PA14.wav");
				} else if (g_frontendMission->header.briefingLogo != 6) {
					sprintf(g_frontendScratchBuffer, "wave\\frontend\\T01PA0%d.wav", rand() % 6 + 3);
				} else {
					strcpy(g_frontendScratchBuffer, "wave\\frontend\\T01PA15.wav");
				}
				break;

			case 11:
			case 12:
			case 13:
			case 14:
				sprintf(g_frontendScratchBuffer, "wave\\frontend\\T01PA%d.wav", rand() % 4 + 16);
				break;

			case 15:
			case 16:
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22:
				sprintf(g_frontendScratchBuffer, "wave\\frontend\\T01PA%d.wav", rand() % 8 + 28);
				break;

			case 23:
				sprintf(g_frontendScratchBuffer, "wave\\frontend\\T01PA37.wav");
				break;

			default:
				g_frontendScratchBuffer[0] = '\0';
				break;
		}

		if (g_frontendScratchBuffer[0]) {
			FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
		}
	}

	return 1;
}

// FUNCTION: XWA 0x53B0C0
int Concourse_LoadMarkoVoiceClip(void) {
	char fileName[128];
	XwaFile* stream;

	srand(GetTickCount());
	FrontendSound_UnloadBufferByName("markovoice");
	fileName[0] = '\0';
	if (g_frontendMissionLoaded) {
		if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
			int missionListIndex;
			int missionPart1;
			int missionPart2, missionPart3;
			char separator;
			char buffer[12];

			for (missionListIndex = 0; missionListIndex < g_missionCount; ++missionListIndex) {
				if (g_missionList[missionListIndex].missionIdx == g_currentMissionId) {
					g_selectedMissionListIndex = missionListIndex;
					break;
				}
			}

			sscanf(g_missionList[missionListIndex].fileName, "%d%c%d%c%d", &missionPart1, &separator,
				   &missionPart2, &separator, &missionPart3);
			{
				int missionNumber = missionPart2;
				int missionPart = missionPart3;

				if (missionNumber >= 0 && missionNumber <= 99) {
					if (missionPart >= 0 && missionPart <= 99) {
						sprintf(buffer, "%2d%2d%2d", missionNumber, missionPart, 1);
						if (buffer[0] == ' ') {
							buffer[0] = '0';
						}

						if (buffer[2] == ' ') {
							buffer[2] = '0';
						}

						if (buffer[4] == ' ') {
							buffer[4] = '0';
						}

						sprintf(fileName, "wave\\frontend\\T%s.wav", buffer);
					} else {
						fileName[0] = '\0';
					}
				} else {
					fileName[0] = '\0';
				}
			}

			if (fileName[0]) {
				stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
				if (stream != NULL) {
					File_Close(stream);
				} else {
					fileName[0] = '\0';
				}
			}

			if (!fileName[0]) {
				if ((missionPart3 == 6 && missionPart2 == 0) || (missionPart3 == 3 && missionPart2 == 5)) {
					if (g_pilotData.tourOfDutyMissions[g_missionList[g_selectedMissionListIndex].missionIdx]
							.numberTimesFlown) {
						sprintf(fileName, "wave\\frontend\\T01MC1%d.wav", rand() % 2);
					} else {
						strcpy(fileName, "wave\\frontend\\T01MC08.wav");
					}
				} else if ((missionPart3 == 2 && missionPart2 == 3) ||
						   (missionPart3 == 5 && missionPart2 == 6)) {
					if (g_pilotData.tourOfDutyMissions[g_missionList[g_selectedMissionListIndex].missionIdx]
							.numberTimesFlown) {
						sprintf(fileName, "wave\\frontend\\T01MC1%d.wav", rand() % 2);
					} else {
						strcpy(fileName, "wave\\frontend\\T01MC09.wav");
					}
				} else {
					if (!g_pilotData.tourOfDutyMissions[g_missionList[g_selectedMissionListIndex].missionIdx]
							 .numberTimesFlown) {
						strcpy(fileName, "wave\\frontend\\T01MC07.wav");
					} else {
						fileName[0] = '\0';
					}
				}

				if (fileName[0]) {
					stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
					if (stream != NULL) {
						File_Close(stream);
					} else {
						fileName[0] = '\0';
					}
				}
			}
		}

		if (!fileName[0]) {
			if (!g_frontendFamilyHasNewEmail ||
				(sprintf(fileName, "wave\\frontend\\T01MC0%d.wav", rand() % 4 + 2),
				 stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb"), stream == NULL)) {
				if (rand() % 2 == 1) {
					strcpy(fileName, "wave\\frontend\\T01MC01.wav");
				}
			} else {
				File_Close(stream);
			}
		}
	} else if (rand() % 2 == 1) {
		strcpy(fileName, "wave\\frontend\\T01MC01.wav");
	}

	FrontendSound_LoadSound(fileName, "markovoice");
	return 1;
}

// FUNCTION: XWA 0x5397D0
int Concourse_Update(int frameCounter) {
	char pilotName[32];
	char localPlayerInfo[2];
	unsigned short port;
	int outX;
	int outY;
	FrontendRect rect;
	int frame;

	if (!g_pilotData.name[0] && FrontendDialog_PromptForPilotName(pilotName) && pilotName[0]) {
		if (g_gameConfig.sfxDatapadEnabled) {
			FrontendSound_PlayUISound("logonsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}

		Pilot_CreateNew(pilotName);
	}
#ifdef XWA_MODERN
	/* The original prompt blocks here. The port modal is nonblocking, so do not
	   let concourse initialization replace its callback before it completes. */
	if (!g_pilotData.name[0]) {
		return 0;
	}
#endif

	frame = frameCounter;
	if (!frameCounter) {
		if (g_frontendMissionLoaded) {
			if ((unsigned int)g_currentMissionId < 7u) {
				FrontendCursor_SetPos(172, 357);
			} else if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				FrontendCursor_SetPos(145, 363);
			} else {
				FrontendCursor_SetPos(320, 216);
			}
		} else {
			FrontendCursor_SetPos(90, 224);
		}

		FrontendMouse_ClearClicks();
		Keyboard_FlushCharBuffer();
		DebugPrintf(NULL);
		g_concourseMarkoHovered = 0;
		/*if (!Joystick_GetCount()) {
			FrontendDialog_ShowConfirmDialog(
				FrontendString_Get(STR_JOY_ERROR1), FrontendString_Get(STR_JOY_ERROR2),
				FrontendString_Get(STR_JOY_ERROR3), FrontendString_Get(STR_OKAY), NULL);
			return 1;
		}*/

		{
			int cdDetected;

			g_currentCdDisk = 0;
			cdDetected = 0;
			if (File_CheckGameCdPresent(0)) {
				g_currentCdDisk = 0;
				cdDetected = 1;
			} else if (File_CheckGameCdPresent(1)) {
				cdDetected = 1;
				g_currentCdDisk = 1;
			}

			if (!cdDetected) {
				for (;;) {
					int cdRetryOk;
					int keepRetrying;

					cdRetryOk = 0;
					if (File_CheckGameCdPresent(0)) {
						g_currentCdDisk = 0;
						cdRetryOk = 1;
					} else if (File_CheckGameCdPresent(1)) {
						g_currentCdDisk = 1;
						cdRetryOk = 1;
					}

					if (cdRetryOk) {
						File_DetectGameAndCdPaths("\\wave\\rebel_pilot1\\A0P1002.wav");
						FrontImage_LoadResourceList("frontres\\combat\\combat.lst");
						FrontImage_LoadResourceList("frontres\\datapad\\top.lst");
						FrontImage_LoadResourceList("frontres\\datapad\\awards.lst");
						FrontImage_LoadResourceList("frontres\\icons\\icons.lst");
						FrontImage_LoadResourceList("frontres\\skirmish\\skirmish.lst");
						FrontImage_LoadResourceList("frontres\\family\\family.lst");
						FrontImage_LoadResourceList("frontres\\cutscene\\cutscene.lst");
						FrontImage_LoadResourceList("frontres\\config\\config.lst");
						FrontendSound_LoadList("sfx\\sfx.lst");
						SpriteResource_FreeGroups();
						SpriteResource_LoadCatalog("Resdata.txt");
						FrontImage_InitAtlasSprites();
						FrontendCursor_LoadResources();
						FrontendDisplay_ClearBackBuffer();
						break;
					}

					FrontendDisplay_DisableOffscreenRestore();
					FrontendDisplay_ClearBackBuffer();
					FrontendDisplay_PresentFrame();
					FrontendDisplay_ClearBackBuffer();
					FrontendDisplay_EnableOffscreenRestore();
					if (FrontImage_ResourceExists("dialogbox")) {
						keepRetrying = FrontendDialog_ShowConfirmDialog(
							FrontendString_Get(STR_FAILED_TO_DETECT_CD1),
							FrontendString_Get(STR_FAILED_TO_DETECT_CD2),
							FrontendString_Get(STR_FAILED_TO_DETECT_CD3), FrontendString_Get(STR_OKAY),
							FrontendString_Get(STR_CANCEL));
					} else {
						keepRetrying = FrontendDialog_ShowConfirmDialog(
							FrontendString_Get(STR_FAILED_TO_DETECT_CD1),
							FrontendString_Get(STR_FAILED_TO_DETECT_CD2),
							FrontendString_Get(STR_FAILED_TO_DETECT_CD4), FrontendString_Get(STR_OKAY),
							FrontendString_Get(STR_CANCEL));
					}

					if (!keepRetrying) {
						return 1;
					}

					File_DetectGameAndCdPaths("\\wave\\rebel_pilot1\\A0P1002.wav");
				}
			}
		}

		Frontend_MarkHostCdAvailable();
		if (g_frontendChatLogBuffer) {
			memset(g_frontendChatLogBuffer, 0, 0x400u);
			g_frontendChatLogUsedBytes = 0;
		}

		g_unusedFrontendConcourseHostLatch = 1;
		g_skipFrontendEntryMovie = 0;
		g_frontendQuickStartLaunchFlag = 0;
		g_frontendSinglePlayerFlightSessionActive = 0;
		if (g_optSkipIntro) {
			g_pilotData.team = g_pilotData.factionStatistics[g_pilotData.currentFactionId].team;
			g_pilotData.missionDirectoryId =
				g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionDirectoryId;
			memcpy(g_pilotData.missionDescriptionIds,
				   g_pilotData.factionStatistics[g_pilotData.currentFactionId].missionDescriptionIds,
				   sizeof(g_pilotData.missionDescriptionIds));
			g_pilotData.unk2 = g_pilotData.factionStatistics[g_pilotData.currentFactionId].m0048;
		}

		g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NONE;
		g_missionSetupRosterAuthoritative = 0;
		g_unusedConcourseEntryResetFlag = 0;
		if (g_optIsHost) {
			musicState = MUSIC_STATE_FRONTEND_1240;
			if (g_gameConfig.datapadMusicEnabled) {
				Music_SetState(MUSIC_STATE_FRONTEND_1240);
				Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
			} else {
				Music_Stop();
			}

			if (!g_hostCdAvailable) {
				if (!ErrorText_LoadLine(3, g_frontendScratchBuffer)) {
					FrontendDisplay_ShowGameMessageBox(
						"ERROR:  Not Host CD!\n\nYou cannot host an internet game with the Client CD.\n"
						"You must insert the Host CD into your CD-ROM drive\nto host an internet game.\n\n"
						"Press ENTER to exit.");
				} else {
					FrontendDisplay_ShowGameMessageBox(g_frontendScratchBuffer);
				}

				return 1;
			}

			memset(g_mpRoster, 0, sizeof(g_mpRoster));
			strcpy(g_pilotData.multiplayerGameName, "Internet game.");
			g_optIsHost = 0;
			g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NET_HOST;
			g_gameConfig.networkType = NET_TRANSPORT_TCPIP;
			isHost = 1;
			FrontendDisplay_FlipDirectDrawToGDISurface();
			strcpy(g_frontendScratchBuffer, g_gameConfig.ipAddress);
			port = g_gameConfig.firewall ? g_gameConfig.portNumber : 0;
			Net_SetNetworkPort(&port);
			localPlayerInfo[0] = (char)g_pilotData.pilotRating + 1;
			localPlayerInfo[1] = '\0';
			FrontendCursor_ShowOsCursor();
			if (!Net_StartNetworkSession(*(const XwaGuid*)&g_frontendNetXwaDirectPlayAppGuid, localPlayerInfo,
										 g_pilotData.name, isHost, g_pilotData.multiplayerGameName,
										 g_gameConfig.networkType, 0, 0, g_frontendScratchBuffer, NULL)) {
				FrontendDisplay_UnlockBackBuffer();
				FrontendCursor_HideOsCursor();
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NONE;
			} else {
				int localPlayerId;

				FrontendDisplay_UnlockBackBuffer();
				FrontendCursor_HideOsCursor();
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				localPlayerId = Net_GetLocalPlayerId();
				Net_SetPlayerReady(localPlayerId);
				memset(g_mpRoster, 0, sizeof(g_mpRoster));
				strcpy(g_mpRoster[0].name, g_pilotData.name);
				g_mpRoster[0].playerId = Net_GetLocalPlayerId();
				g_mpRoster[0].rating = g_pilotData.pilotRating;
				FrontendScreen_SetCallbacks(MissionSetup_Update, MissionSetup_Exit);
			}

			return 0;
		}

		if (g_optIsClient) {
			musicState = MUSIC_STATE_FRONTEND_1240;
			if (g_gameConfig.datapadMusicEnabled) {
				Music_SetState(MUSIC_STATE_FRONTEND_1240);
				Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
			} else {
				Music_Stop();
			}

			g_optIsClient = 0;
			g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NET_CLIENT;
			g_gameConfig.networkType = NET_TRANSPORT_TCPIP;
			FrontendScreen_SetCallbacks(FrontendNet_JoinGameScreen, FrontendMissionList_FreeScreenResources);
			return 0;
		}

		Concourse_LoadNextTourMission();
		if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
			FrontendScreen_SetCallbacks(FamilyTransportRoom_Update, FamilyTransportRoom_Exit);
			return 0;
		}

		musicState = MUSIC_STATE_FRONTEND_1200;
		if (g_gameConfig.datapadMusicEnabled) {
			Music_SetState(MUSIC_STATE_FRONTEND_1200);
			Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
		} else {
			Music_Stop();
		}

		FrontendEmail_LoadList();
		g_frontendFamilyHasNewEmail = 0;
		if (g_frontendMissionLoaded && g_frontendEmailCount > 0) {
			int i;

			for (i = 0; i < g_frontendEmailCount; ++i) {
				if ((unsigned int)g_frontendEmailEntries[i].field00 <= (unsigned int)g_currentMissionId &&
					!g_pilotData.emailsStatus[g_frontendEmailEntries[i].emailIndex]) {
					g_frontendFamilyHasNewEmail = 1;
					if (g_frontendMission->header.hangar != XWA_HANGAR_FAMILYTRANSPORT) {
						g_pilotData.emailsStatus[g_frontendEmailEntries[i].emailIndex] = 1;
					}
				}
			}
		}

		if (g_frontendMissionLoaded) {
			int flightGroupCount;
			int playerStartRegion;
			int i;

			srand((unsigned int)g_currentMissionId + 3452938u);
			flightGroupCount = (int16_t)g_frontendMission->flightGroupCount;
			playerStartRegion = 0;
			for (i = 0; i < flightGroupCount; ++i) {
				if (g_frontendMission->flightGroups[i].playerNumber == 1) {
					playerStartRegion =
						g_frontendMission->flightGroups[i].missionPointRegions[XWA_FG_POINT_START_1];
					break;
				}
			}

			for (i = 0; i < flightGroupCount; ++i) {
				if (g_frontendMission->flightGroups[i].craftType == 0xb7 &&
					g_frontendMission->flightGroups[i].missionPointRegions[XWA_FG_POINT_START_1] ==
						playerStartRegion) {
					int backdrop;

					backdrop = g_frontendMission->flightGroups[i].backdrop;
					if ((unsigned int)backdrop < 0x3du && backdrop != 0 && backdrop != 55) {
						Concourse_LoadPlanetSprite(g_frontendMission->flightGroups[i].backdrop);
						break;
					}
				}
			}
		} else {
			int planetRand;

			srand((unsigned int)(g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] +
								 58758 * g_pilotData.missionDirectoryId));
			do {
				planetRand = rand() % 60 + 1;
			} while (planetRand == 55);
			Concourse_LoadPlanetSprite(planetRand);
		}

		FrontImage_LoadResourceList("frontres\\concourse\\concourse.lst");
		Concourse_LoadBackground();
		FrontendDisplay_ClearBackBuffer();
		FrontendDisplay_ClearOffscreenSurface();
		Concourse_DrawPlanet();
		FrontImage_DrawSprite("concourse", 0, 0);
		FrontendDisplay_LockOffscreenSurface();
		Concourse_DrawPlanet();
		FrontImage_DrawSprite("concourse", 0, 0);
		FrontendDisplay_UnlockOffscreenSurface(1);
		g_concourseLabelColor = FrontendDisplay_PackRGB(0x60, 0x0c, 0x0c);
		FrontendText_SetGlyphGradientBg(g_concourseLabelColor);
		FrontendText_ResetGlyphScratchBuffer(15);
		Concourse_InitStarfield();
		FrontendCursor_Show();
		frame = 0;
		g_concourseIdleVoiceFrame = rand() % 480;
	}

	if (g_concourseIdleVoiceFrame == frame) {
		Concourse_PlayIdleVoice(frame);
	}

	Concourse_DrawStarfield(frame);
	FrontImage_DrawSpriteOpaque("tourdoor", 231, 174);
	FrontImage_DrawSprite("mkanim", 110, 358);
	FrontImage_DrawSpriteOpaque("traindoor", 536, 174);
	FrontImage_DrawSpriteOpaque("combatdoor", 35, 174);
	FrontImage_DrawSpriteOpaque("filmdoor", 536, 286);
	FrontImage_DrawSpriteOpaque("techdoor", 426, 219);
	FrontendCursor_GetPos(&outX, &outY);

	FrontImage_GetResourceRect("traindoor", &rect);
	FrontendDraw_RectOffsetXY(&rect, 536, 174);
	if (FrontendDraw_PointInRect(&rect, outX, outY)) {
		if (!FrontImage_GetSpriteFrame("traindoor")) {
			FrontendSound_PlayUISound("tourdooropen", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}

		FrontImage_AdvanceSpriteFrame("traindoor", 0);
		if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
			Skirmish_SetupProvingGroundsSession();
			FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
		}

		{
			int width;

			width = FrontendText_MeasureWidth(FrontendString_Get(STR_CONCOURSE_TRAINING_SIMULATOR), 15);
			rect.left = 300 - (width >> 1);
			rect.top = 400;
			rect.right = width + rect.left + 40;
			rect.bottom = 440;
			FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_concourseLabelColor);
			FrontendText_DrawCentered(15, FrontendString_Get(STR_CONCOURSE_TRAINING_SIMULATOR), &rect,
									  0xffff);
		}
	} else {
		if (FrontImage_GetSpriteFrame("traindoor") == 1) {
			FrontendSound_PlayUISound("tourdoorclose", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}

		FrontImage_RewindSpriteFrame("traindoor", 0);
	}

	FrontImage_GetResourceRect("combatdoor", &rect);
	FrontendDraw_RectOffsetXY(&rect, 35, 174);
	if (FrontendDraw_PointInRect(&rect, outX, outY)) {
		if (!FrontImage_GetSpriteFrame("combatdoor")) {
			FrontendSound_PlayUISound("tourdooropen", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}

		FrontImage_AdvanceSpriteFrame("combatdoor", 0);
		if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
			FrontendScreen_SetCallbacks(CombatSimMenu_Update, CombatSimMenu_Exit);
		}

		{
			int width;

			width = FrontendText_MeasureWidth(FrontendString_Get(STR_CONCOURSE_COMBAT_SIMULATOR), 15);
			rect.left = 300 - (width >> 1);
			rect.top = 400;
			rect.right = width + rect.left + 40;
			rect.bottom = 440;
			FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_concourseLabelColor);
			FrontendText_DrawCentered(15, FrontendString_Get(STR_CONCOURSE_COMBAT_SIMULATOR), &rect, 0xffff);
		}
	} else {
		if (FrontImage_GetSpriteFrame("combatdoor") == 1) {
			FrontendSound_PlayUISound("tourdoorclose", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}

		FrontImage_RewindSpriteFrame("combatdoor", 0);
	}

	if (g_frontendMissionLoaded && g_frontendMission->header.hangar != XWA_HANGAR_FAMILYTRANSPORT) {
		FrontImage_GetResourceRect("tourdoor", &rect);
		FrontendDraw_RectOffsetXY(&rect, 231, 174);
		if (FrontendDraw_PointInRect(&rect, outX, outY)) {
			if (!FrontImage_GetSpriteFrame("tourdoor")) {
				FrontendSound_PlayUISound("tourdooropen", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}

			FrontImage_AdvanceSpriteFrame("tourdoor", 0);
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				FrontendScreen_SetCallbacks(BriefingRoom_Update, BriefingRoom_Exit);
			}

			{
				int width;

				if ((unsigned int)g_currentMissionId <= 0x31u || (unsigned int)g_currentMissionId > 0x34u) {
					width = FrontendText_MeasureWidth(FrontendString_Get(STR_CONCOURSE_TOUR_OF_DUTY), 15);
				} else {
					width =
						FrontendText_MeasureWidth(g_missionList[g_selectedMissionListIndex].description, 15);
				}

				rect.left = 300 - (width >> 1);
				rect.top = 400;
				rect.right = width + rect.left + 40;
				rect.bottom = 440;
				FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_concourseLabelColor);
				if ((unsigned int)g_currentMissionId <= 0x31u || (unsigned int)g_currentMissionId > 0x34u) {
					FrontendText_DrawCentered(15, FrontendString_Get(STR_CONCOURSE_TOUR_OF_DUTY), &rect,
											  0xffff);
				} else {
					FrontendText_DrawCentered(15, g_missionList[g_selectedMissionListIndex].description,
											  &rect, 0xffff);
				}
			}
		} else {
			if (FrontImage_GetSpriteFrame("tourdoor") == 1) {
				FrontendSound_PlayUISound("tourdoorclose", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}

			FrontImage_RewindSpriteFrame("tourdoor", 0);
		}
	}

	FrontendDraw_RectAssign(&rect, 90, 328, 200, 408);
	if (FrontendDraw_PointInRect(&rect, outX, outY)) {
		if (!g_concourseMarkoHovered) {
			if (!FrontendSound_GetPlayingCount("markovoice")) {
				Concourse_LoadMarkoVoiceClip();
				FrontendSound_PlayUISound("markovoice", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}

			g_concourseMarkoHovered = 1;
		}

		if (FrontImage_GetSpriteFrame("mkanim") < 20) {
			FrontImage_AdvanceSpriteFrame("mkanim", 0);
		}

		if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
			FrontendScreen_SetCallbacks(FamilyTransportRoom_Update, FamilyTransportRoom_Exit);
		}

		{
			int width;

			width = FrontendText_MeasureWidth(FrontendString_Get(STR_CONCOURSE_FAMILY_TRANSPORT), 15);
			rect.left = 300 - (width >> 1);
			rect.top = 400;
			rect.right = width + rect.left + 40;
			rect.bottom = 440;
			FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_concourseLabelColor);
			FrontendText_DrawCentered(15, FrontendString_Get(STR_CONCOURSE_FAMILY_TRANSPORT), &rect, 0xffff);
		}
	} else {
		g_concourseMarkoHovered = 0;
		if (FrontImage_GetSpriteFrame("mkanim")) {
			FrontImage_AdvanceSpriteFrame("mkanim", 1);
		}
	}

	if (g_filmFeatureEnabled) {
		FrontImage_GetResourceRect("filmdoor", &rect);
		FrontendDraw_RectOffsetXY(&rect, 536, 286);
		if (FrontendDraw_PointInRect(&rect, outX, outY)) {
			if (!FrontImage_GetSpriteFrame("filmdoor")) {
				FrontendSound_PlayUISound("tourdooropen", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}

			FrontImage_AdvanceSpriteFrame("filmdoor", 0);
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				FrontendScreen_SetCallbacks(FilmRoom_Update, FilmRoom_Exit);
			}

			{
				int width;

				width = FrontendText_MeasureWidth("Film Room", 15);
				rect.left = 300 - (width >> 1);
				rect.top = 400;
				rect.right = width + rect.left + 40;
				rect.bottom = 440;
				FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_concourseLabelColor);
				FrontendText_DrawCentered(15, FrontendString_Get(STR_CONCOURSE_FILM_ROOM), &rect, 0xffff);
			}
		} else {
			if (FrontImage_GetSpriteFrame("filmdoor") == 1) {
				FrontendSound_PlayUISound("tourdoorclose", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}

			FrontImage_RewindSpriteFrame("filmdoor", 0);
		}
	}

	if (!(frame % 3)) {
		FrontImage_AdvanceSpriteFrame("techdoor", 1);
	}

	FrontImage_GetResourceRect("techdoor", &rect);
	FrontendDraw_RectOffsetXY(&rect, 426, 219);
	if (FrontendDraw_PointInRect(&rect, outX, outY)) {
		if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
			FrontendScreen_SetCallbacks(TechLibrary_Update, TechLibrary_Exit);
		}

		{
			int width;

			width = FrontendText_MeasureWidth(FrontendString_Get(STR_CONCOURSE_TECH_LIBRARY), 15);
			rect.left = 300 - (width >> 1);
			rect.top = 400;
			rect.right = width + rect.left + 40;
			rect.bottom = 440;
			FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_concourseLabelColor);
			FrontendText_DrawCentered(15, FrontendString_Get(STR_CONCOURSE_TECH_LIBRARY), &rect, 0xffff);
		}
	}

	FrontImage_SetSpriteFrame("leftb", 9);
	FrontImage_DrawSprite("leftb", 0, 240);
	return Frontend_HandleEscapeQuit(0) == 1;
}

// FUNCTION: XWA 0x539760
int Concourse_Exit(int frameCounter) {
	(void)frameCounter;

	FrontendWaveStream_Shutdown();
	FrontImage_UnloadResourceList("frontres\\concourse\\concourse.lst");
	Concourse_FreeBackground();
	Concourse_FreePlanetSprite();
	if (g_missionList != NULL) {
		Mem_Free(g_missionList);
		g_missionList = NULL;
	}

	if (g_frontendEmailEntries != NULL) {
		Mem_Free(g_frontendEmailEntries);
		g_frontendEmailEntries = NULL;
	}

	FrontendSound_UnloadBufferByName("markovoice");
	return 0;
}
