#include "xwa/frontend/flight_loading.h"

#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/flight_display.h"
#include "xwa/frontend/briefing_script.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_flight.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/mission_briefing.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/flight_net.h"
#include "xwa/flight/net_session.h"
#include "xwa/frontend/net_transport.h"
#include "xwa/render/renderer.h"
#include "xwa/util/time.h"

#include <stdio.h>
#include <string.h>

// GLOBAL: XWA 0xABD238
int g_unusedFlightLoadingReadyScreenFlag;
// GLOBAL: XWA 0x782DE0
int g_flightLoadingReadyScreenCurrentTick;
// GLOBAL: XWA 0x782DE4
int g_flightLoadingReadyScreenStartTick;
// GLOBAL: XWA 0xAE2A50
int g_frontendLaunchHumanPlayerCount;

// FUNCTION: XWA 0x4CC920
int FlightLoading_PulseAndDrawProgressScreen(int progressStep) {
	FlightNet_BroadcastStillLoadingPulse();
	return FlightLoading_DrawProgressScreen(progressStep);
}

// FUNCTION: XWA 0x531840
int FlightLoading_DrawProgressScreen(int progressStep) {
	const char* titleText;
	int progressBarWidth;
	FrontendRect rect;
	int x;

	g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();

	if (progressStep < 0) {
		if (progressStep == -2) {
			FrontImage_BuildAtlasBlendLut();
			FrontImage_RebuildPaletteCache();
			FrontendColor_Init();
			FrontendText_SetGlyphGradientBg(g_colorNearBlack);
			FrontImage_RegisterResourceDefault("frontres\\config\\dpmed.bmp", "background");
			if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR ||
				g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
				FrontendMission_LoadForBriefing();
			}
		}

		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontendDraw_RectAssign(&rect, 65, 60, 575, 75);
		if (g_filmFilePath[0]) {
			titleText = FrontendString_Get(STR_FILM_LOADING);
		} else {
			titleText = FrontendString_Get(STR_LETS_GET_READY);
		}
		FrontendText_DrawCentered(15, titleText, &rect, g_colorPaleBlue);
		FrontendDisplay_PresentFrame();
		FrontendDisplay_UnlockBackBuffer();
		return 1;
	}

	FrontImage_DrawSpriteOpaque("background", 0, 0);
	if (g_filmFilePath[0]) {
		FrontendDraw_RectAssign(&rect, 65, 60, 575, 75);
		FrontendText_DrawCentered(15, FrontendString_Get(STR_FILM_LOADING), &rect, g_colorPaleBlue);
	} else if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR ||
			   g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
		int firstTextBlock;
		int previousTextBlock;
		int y;

		FrontendDraw_RectAssign(&rect, 65, 60, 575, 75);
		FrontendText_DrawCentered(15, FrontendString_Get(STR_LETS_GET_READY), &rect, g_colorPaleBlue);
		BriefingScript_ResetState();
		BriefingScript_AdvanceToNextVisibleLine();
		firstTextBlock = g_briefingTextSlotBlockIdx[1];
		previousTextBlock = -1;
		for (y = 90; y < 426; y += 42) {
			int textBlock;

			FrontendDraw_RectAssign(&rect, 64, y, 573, y + 41);
			FrontendText_Draw(10, "*", rect.left - 10, rect.top + 1, 0xffff);
			FrontendText_DrawFormattedWrappedText(
				&rect,
				(const unsigned char*)g_frontendBriefingContent.textBlocks[g_briefingTextSlotBlockIdx[1]], 0);
			BriefingScript_AdvanceToNextVisibleLine();
			textBlock = g_briefingTextSlotBlockIdx[1];
			if (textBlock == firstTextBlock) {
				break;
			}
			if (textBlock == previousTextBlock) {
				BriefingScript_AdvanceToNextVisibleLine();
				textBlock = g_briefingTextSlotBlockIdx[1];
				if (textBlock == previousTextBlock) {
					break;
				}
			}
			if (!*g_frontendBriefingContent.textBlocks[textBlock]) {
				break;
			}
			previousTextBlock = textBlock;
		}
	} else {
		int textRowsUsed;

		FrontendDraw_RectAssign(&rect, 65, 60, 575, 75);
		if (g_combatSimSkirmishFileName[0]) {
			FrontendText_DrawCentered(15, g_combatSimSkirmishFileName, &rect, g_colorPaleBlue);
		} else {
			FrontendText_DrawCentered(15, FrontendString_Get(STR_QUICK_SKIRMISH), &rect, g_colorPaleBlue);
		}

		rect.top = 90;
		rect.bottom = 110;
		sprintf(g_frontendScratchBuffer, "%s %s", FrontendString_Get(STR_GAME_GOAL_DESC),
				FrontendString_Get((UIString)(g_gameConfig.goalType + STR_GAME_GOAL_COMBAT)));
		FrontendText_DrawCentered(10, g_frontendScratchBuffer, &rect, g_colorLightBlue);

		rect.top = 110;
		rect.bottom = 420;
		textRowsUsed =
			FrontendText_DrawWrappedClipped(
				10, FrontendString_Get((UIString)(g_gameConfig.goalType + STR_GAME_GOAL_COMBAT_DESC)), &rect,
				g_colorLightBlue, 4, 0) +
			1;
		if (g_gameConfig.goalType) {
			sprintf(g_frontendScratchBuffer, "* %s - %s", FrontendString_Get(STR_GAME_ALL_TEAMS),
					FrontendString_Get(STR_GAME_GOAL1));
			FrontendText_Draw(10, g_frontendScratchBuffer, 70, 14 * textRowsUsed + 130, g_colorLightBlue);
		} else {
			int teamTextWidth;
			int y;
			int teamIdx;

			sprintf(g_frontendScratchBuffer, "*  %s %d  -  ", FrontendString_Get(STR_TEAM), 8);
			teamTextWidth = FrontendText_MeasureWidth(g_frontendScratchBuffer, 10);
			y = 14 * textRowsUsed + 130;
			for (teamIdx = 0; teamIdx < g_gameConfig.numberOfTeams; ++teamIdx) {
				sprintf(g_frontendScratchBuffer, "*  %s %d  -  ", FrontendString_Get(STR_TEAM), teamIdx + 1);
				FrontendText_Draw(10, g_frontendScratchBuffer, 70, y, g_colorLightBlue);
				FrontendText_Draw(
					10, FrontendString_Get((UIString)(g_gameConfig.teamGoals[teamIdx] + STR_GAME_GOAL1)),
					teamTextWidth + 85, y, g_colorLightBlue);
				y += 30;
			}
		}
	}

	progressBarWidth = 495 / (9 - progressStep);
	for (x = 0; x < progressBarWidth; ++x) {
		FrontImage_DrawSprite("sbarcenter", x + 70, 75);
	}
	FrontImage_DrawSprite("sbarstart", 65, 75);
	if (progressBarWidth > 0) {
		FrontImage_DrawSprite("sbarspark", progressBarWidth + 70, 75);
	}

	FrontendDisplay_PresentFrame();
	FrontendDisplay_UnlockBackBuffer();
	return 1;
}

// FUNCTION: XWA 0x4CC8C0
int FlightLoading_ShowInitialProgressScreen(int attachFrontendSurfaces) {
	if (attachFrontendSurfaces) {
		FlightLoading_AttachFrontendSurfaces();
	}

	return FlightLoading_DrawProgressScreen(-1 - attachFrontendSurfaces);
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5316B0
int FlightLoading_GetReadyScreen(int frameCounter) {
	int readyPlayerCount;
	FrontendRect rect;

	if (!frameCounter) {
		FrontendCursor_Hide();
		g_unusedFlightLoadingReadyScreenFlag = 1;
		isHost = Net_IsHost();
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			isHost = 1;
			readyPlayerCount = 1;
		} else {
			readyPlayerCount = Net_CountReadyPlayers();
		}

		g_frontendLaunchHumanPlayerCount = readyPlayerCount;
		g_pilotData.numHumanPlayersLastMission = readyPlayerCount;
		g_flightLoadingReadyScreenStartTick = (int)GetTickCount();
		FrontImage_RegisterResourceDefault("frontres\\config\\configb.bmp", "background");
		if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			FrontImage_DrawSpriteOpaque("background", 0, 0);
			FrontendDisplay_LockOffscreenSurface();
			FrontImage_DrawSpriteOpaque("background", 0, 0);
			FrontendDisplay_UnlockOffscreenSurface(1);
			FrontendText_ResetGlyphScratch();
		} else {
			FrontendDisplay_ClearBackBuffer();
		}

		Music_Stop();
		FrontendText_SetGlyphGradientBg(g_colorNearBlack);
	}

	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
			FrontendDraw_RectAssign(&rect, 65, 60, 575, 75);
			FrontendText_DrawCentered(15, FrontendString_Get(STR_LETS_GET_READY), &rect, g_colorPaleBlue);
		} else {
			FrontendDraw_RectAssign(&rect, 65, 60, 575, 75);
			FrontendText_DrawCentered(15, FrontendString_Get(STR_LETS_GET_READY), &rect, g_colorPaleBlue);
		}
		if (isHost) {
			g_flightLoadingReadyScreenCurrentTick = (int)GetTickCount();
			if (g_flightLoadingReadyScreenCurrentTick <= g_flightLoadingReadyScreenStartTick + 2000) {
				goto wait_for_timeout;
			}

			MissionSetup_BroadcastStatePacket(0);
		} else {
		wait_for_timeout:
			g_flightLoadingReadyScreenCurrentTick = (int)GetTickCount();
			if (g_flightLoadingReadyScreenCurrentTick <= g_flightLoadingReadyScreenStartTick + 4000) {
				return 0;
			}
		}
	}

	g_unusedFlightLoadingReadyScreenFlag = 1;
	FrontImage_FreeResourceByName("background");
	FrontendScreen_SetCallbacks(FrontendFlight_LaunchSession, (FrontendScreenExitFn)NetSession_ExitStub);
	return 0;
}

// FUNCTION: XWA 0x531D40
int FlightLoading_FreeReadyScreenResources(void) {
	FrontImage_FreeResourceByName("background");
	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR ||
		g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE) {
		BriefingText_FreeAllocatedBuffersExit();
	}

	return 1;
}

// FUNCTION: XWA 0x50CDE0
int FlightLoading_AttachFrontendSurfaces(void) {
	DDSURFACEDESC desc;
	DDBLTFX bltFx;
	HRESULT bltResult;
	HRESULT restoreResult;
	IDirectDrawSurface* clearSurface;

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = 108;
	desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	if (g_useHardware3D) {
		desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	}
	desc.dwHeight = g_surfaceHeight;
	desc.dwWidth = g_surfaceWidth;
	bltResult = g_flightDirectDraw->lpVtbl->CreateSurface(g_flightDirectDraw, &desc,
														  &g_flightOverlayOffscreenSurface, NULL);
	if (bltResult) {
		return 0;
	}
	bltResult = g_flightDirectDraw->lpVtbl->CreateSurface(g_flightDirectDraw, &desc,
														  &g_flightOverlayBackBufferSurface, NULL);
	if (bltResult) {
		if (g_flightOverlayOffscreenSurface) {
			g_flightOverlayOffscreenSurface->lpVtbl->Release(g_flightOverlayOffscreenSurface);
			g_flightOverlayOffscreenSurface = NULL;
		}
		return 0;
	}

	clearSurface = g_flightPrimarySurface;
	bltFx.dwSize = 100;
	bltFx.dwFillColor = 0;
	do {
		bltResult = clearSurface->lpVtbl->Blt(clearSurface, NULL, NULL, NULL, DDBLT_COLORFILL, &bltFx);
	} while (bltResult &&
			 (bltResult != DX_DDERR_SURFACELOST ||
			  (restoreResult = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface)) == 0) &&
			 bltResult == DX_DDERR_WASSTILLDRAWING);

	clearSurface = g_flightBackBufferSurface;
	bltFx.dwSize = 100;
	bltFx.dwFillColor = 0;
	do {
		bltResult = clearSurface->lpVtbl->Blt(clearSurface, NULL, NULL, NULL, DDBLT_COLORFILL, &bltFx);
	} while (bltResult &&
			 (bltResult != DX_DDERR_SURFACELOST ||
			  (restoreResult = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface)) == 0) &&
			 bltResult == DX_DDERR_WASSTILLDRAWING);

	clearSurface = g_flightOverlayBackBufferSurface;
	bltFx.dwSize = 100;
	bltFx.dwFillColor = 0;
	do {
		bltResult = clearSurface->lpVtbl->Blt(clearSurface, NULL, NULL, NULL, DDBLT_COLORFILL, &bltFx);
	} while (bltResult &&
			 (bltResult != DX_DDERR_SURFACELOST ||
			  (restoreResult = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface)) == 0) &&
			 bltResult == DX_DDERR_WASSTILLDRAWING);

	clearSurface = g_flightOverlayOffscreenSurface;
	bltFx.dwSize = 100;
	bltFx.dwFillColor = 0;
	do {
		bltResult = clearSurface->lpVtbl->Blt(clearSurface, NULL, NULL, NULL, DDBLT_COLORFILL, &bltFx);
	} while (bltResult &&
			 (bltResult != DX_DDERR_SURFACELOST ||
			  (restoreResult = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface)) == 0) &&
			 bltResult == DX_DDERR_WASSTILLDRAWING);
	return FrontendDisplay_AttachExternalSurfaces(
		g_flightPrimarySurface, g_flightBackBufferSurface, g_flightOverlayBackBufferSurface,
		g_flightOverlayOffscreenSurface, g_surfaceWidth, g_surfaceHeight);
}

// FUNCTION: XWA 0x50D000
int FlightLoading_DetachFrontendSurfaces(void) {
	FlightLoading_FreeReadyScreenResources();
	FrontendDisplay_DetachExternalSurfaces();
	if (g_flightOverlayBackBufferSurface) {
		g_flightOverlayBackBufferSurface->lpVtbl->Release(g_flightOverlayBackBufferSurface);
		g_flightOverlayBackBufferSurface = NULL;
	}
	if (g_flightOverlayOffscreenSurface) {
		g_flightOverlayOffscreenSurface->lpVtbl->Release(g_flightOverlayOffscreenSurface);
		g_flightOverlayOffscreenSurface = NULL;
	}
	return 1;
}

/* Port task-shell glue (no original counterpart): the tick shell (xwa_port.c)
 * queries whether the in-flight frontend overlay surfaces are currently attached. */
int FlightLoading_AreFrontendSurfacesAttached(void) {
	return g_flightOverlayBackBufferSurface != 0 || g_flightOverlayOffscreenSurface != 0;
}
