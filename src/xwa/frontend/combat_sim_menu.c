#include "xwa/frontend/combat_sim_menu.h"

#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/string_table.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/concourse.h"
#include "xwa/frontend/film_room.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_escape.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/mission_setup.h"

#include <string.h>

enum {
	COMBAT_SIM_MENU_PENDING_MULTIPLAYER_POD = 1,
	COMBAT_SIM_MENU_PENDING_SINGLE_POD = 2,
	COMBAT_SIM_MENU_PENDING_MULTIPLAYER = 3,
	COMBAT_SIM_MENU_PENDING_SINGLE_PLAYER = 4,
};

// GLOBAL: XWA 0x782FF8
int g_combatSimMenuTranslucentFillColor;
// GLOBAL: XWA 0x782FFC
int g_combatSimMenuPendingTransition;

int MultiplayerSetup_Update(int frameCounter);
#ifdef XWA_MODERN
int MultiplayerSetup_Exit(int frameCounter);
#else
void MultiplayerSetup_Exit(int frameCounter);
#endif

// FUNCTION: XWA 0x53B500
int CombatSimMenu_Update(int frameCounter) {
	FrontendRect rect;
	int mouseX;
	int mouseY;
	int color;
	unsigned int textWidth;

	if (frameCounter == 0) {
		g_combatSimMenuTranslucentFillColor = FrontendDisplay_PackRGB(0x60u, 0x0cu, 0x0cu);
		FrontendText_SetGlyphGradientBg(g_combatSimMenuTranslucentFillColor);
		g_combatSimMenuPendingTransition = 0;
		g_activeTextFieldId = 0;

		if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
			if (g_filmFeatureEnabled) {
				FrontendCursor_SetPos(320, 100);
			} else {
				FrontendCursor_SetPos(320, 160);
			}
		} else {
			FrontendCursor_SetPos(360, 260);
		}

		if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
			FrontImage_RegisterResourceDefault("frontres\\combat\\podopen.bmp", "background");
		} else {
			FrontImage_RegisterResourceDefault("frontres\\combat\\combat.bmp", "background");
			FrontImage_RegisterResourceDefault("frontres\\combat\\doors.flc", "doors");
			FrontImage_RegisterResourceDefault("frontres\\combat\\multi.flc", "multi");
			FrontImage_RegisterResourceDefault("frontres\\combat\\single.flc", "single");
		}

		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontendDisplay_LockOffscreenSurface();
		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontendDisplay_UnlockOffscreenSurface(1);
		FrontImage_SetSpriteFrame("doors", 0);
		FrontImage_SetSpriteFrame("multi", 0);
		FrontImage_SetSpriteFrame("single", 0);
	}

	FrontendCursor_GetPos(&mouseX, &mouseY);

	if (g_combatSimMenuPendingTransition != 0) {
		switch (g_combatSimMenuPendingTransition) {
			case COMBAT_SIM_MENU_PENDING_MULTIPLAYER_POD:
				if (Keyboard_BufferContains(27) || Keyboard_BufferContains(32) ||
					Keyboard_BufferContains(13) || Keyboard_BufferContains(8) ||
					FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
					Keyboard_FlushCharBuffer();
					FrontendScreen_SetCallbacks(MultiplayerSetup_Update, MultiplayerSetup_Exit);
				} else if (FrontImage_GetSpriteFrame("multi") == 34) {
					FrontendSound_PlayUISound("podopen", 1, 0, 255, 10 * g_gameConfig.sfxDatapadVolume, 63);
					FrontImage_RewindSpriteFrame("multi", 0);
				} else if (FrontImage_GetSpriteFrame("multi") != 0) {
					FrontImage_RewindSpriteFrame("multi", 0);
				} else {
					FrontendScreen_SetCallbacks(MultiplayerSetup_Update, MultiplayerSetup_Exit);
				}
				FrontImage_DrawSprite("multi", 24, 140);
				return 0;

			case COMBAT_SIM_MENU_PENDING_SINGLE_POD:
				if (Keyboard_BufferContains(27) || Keyboard_BufferContains(32) ||
					Keyboard_BufferContains(13) || Keyboard_BufferContains(8) ||
					FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
					Keyboard_FlushCharBuffer();
					FrontendScreen_SetCallbacks(MissionSetup_Update, MissionSetup_Exit);
				} else if (FrontImage_GetSpriteFrame("single") == 0) {
					FrontendSound_PlayUISound("podopen", 1, 0, 255, 10 * g_gameConfig.sfxDatapadVolume, 63);
					FrontImage_AdvanceSpriteFrame("single", 0);
				} else if (FrontImage_GetSpriteFrame("single") != 35) {
					FrontImage_AdvanceSpriteFrame("single", 0);
				} else {
					FrontendScreen_SetCallbacks(MissionSetup_Update, MissionSetup_Exit);
				}
				FrontImage_DrawSprite("single", 373, 313);
				return 0;

			case COMBAT_SIM_MENU_PENDING_MULTIPLAYER:
				FrontendScreen_SetCallbacks(MultiplayerSetup_Update, MultiplayerSetup_Exit);
				return 0;

			case COMBAT_SIM_MENU_PENDING_SINGLE_PLAYER:
				FrontendScreen_SetCallbacks(MissionSetup_Update, MissionSetup_Exit);
				return 0;

			default:
				return 0;
		}
	}

	if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
		textWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT), 15);
		rect.left = 300 - (textWidth >> 1);
		rect.right = textWidth + rect.left + 40;
		if (g_filmFeatureEnabled) {
			rect.top = 100;
			rect.bottom = 140;
		} else {
			rect.top = 140;
			rect.bottom = 180;
		}
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_combatSimMenuTranslucentFillColor);

		color = g_colorLightBlue;
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			color = g_colorYellow;
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				color = g_colorRed;
				FrontendSound_PlayUISound("jewelsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				memset(g_mpRoster, 0, sizeof(g_mpRoster));
				g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_SINGLEPLAYER;
				g_combatSimMenuPendingTransition = COMBAT_SIM_MENU_PENDING_SINGLE_PLAYER;
			}
		}
		FrontendText_DrawCentered(15, FrontendString_Get(STR_SINGLE_PLAYER), &rect, color);
	} else {
		FrontendDraw_RectAssign(&rect, 384, 252, 630, 424);
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			textWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_SINGLE_PLAYER), 15);
			rect.left = 300 - (textWidth >> 1);
			rect.top = 400;
			rect.right = textWidth + rect.left + 40;
			rect.bottom = 440;
			FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_combatSimMenuTranslucentFillColor);
			FrontendText_DrawCentered(15, FrontendString_Get(STR_SINGLE_PLAYER), &rect, 0xffff);

			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_SINGLEPLAYER;
				g_combatSimMenuPendingTransition = COMBAT_SIM_MENU_PENDING_SINGLE_POD;
				memset(g_mpRoster, 0, sizeof(g_mpRoster));
			}
		}
	}

	if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
		textWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT), 15);
		rect.left = 300 - (textWidth >> 1);
		rect.right = textWidth + rect.left + 40;
		if (g_filmFeatureEnabled) {
			rect.top = 180;
			rect.bottom = 220;
		} else {
			rect.top = 220;
			rect.bottom = 260;
		}
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_combatSimMenuTranslucentFillColor);

		color = g_colorLightBlue;
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			color = g_colorYellow;
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				color = g_colorRed;
				FrontendSound_PlayUISound("jewelsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				g_combatSimMenuPendingTransition = COMBAT_SIM_MENU_PENDING_MULTIPLAYER;
			}
		}
		FrontendText_DrawCentered(15, FrontendString_Get(STR_MULTIPLAYER), &rect, color);
	} else {
		FrontendDraw_RectAssign(&rect, 36, 148, 318, 256);
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			textWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_MULTIPLAYER), 15);
			rect.left = 300 - (textWidth >> 1);
			rect.top = 400;
			rect.right = textWidth + rect.left + 40;
			rect.bottom = 440;
			FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_combatSimMenuTranslucentFillColor);
			FrontendText_DrawCentered(15, FrontendString_Get(STR_MULTIPLAYER), &rect, 0xffff);

			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				g_combatSimMenuPendingTransition = COMBAT_SIM_MENU_PENDING_MULTIPLAYER_POD;
				FrontImage_SetSpriteFrame("multi", 34);
			}
		}
	}

	if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u && g_filmFeatureEnabled) {
		textWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT), 15);
		rect.left = 300 - (textWidth >> 1);
		rect.top = 260;
		rect.right = textWidth + rect.left + 40;
		rect.bottom = 300;
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_combatSimMenuTranslucentFillColor);

		color = g_colorLightBlue;
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			color = g_colorYellow;
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				color = g_colorRed;
				FrontendSound_PlayUISound("jewelsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				FrontendScreen_SetCallbacks(FilmRoom_Update, FilmRoom_Exit);
			}
		}
		FrontendText_DrawCentered(15, FrontendString_Get(STR_CONCOURSE_FILM_ROOM), &rect, color);
	}

	if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
		textWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT), 15);
		rect.left = 300 - (textWidth >> 1);
		rect.right = textWidth + rect.left + 40;
		if (g_filmFeatureEnabled) {
			rect.top = 340;
			rect.bottom = 380;
		} else {
			rect.top = 300;
			rect.bottom = 340;
		}
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_combatSimMenuTranslucentFillColor);

		color = g_colorLightBlue;
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			color = g_colorYellow;
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				color = g_colorRed;
				FrontendSound_PlayUISound("jewelsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
			}
		}
		FrontendText_DrawCentered(15, FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT), &rect, color);
	} else {
		FrontendDraw_RectAssign(&rect, 326, 22, 412, 148);
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			if (FrontImage_GetSpriteFrame("doors") == 0) {
				FrontendSound_PlayUISound("tourdooropen", 1, 0, 255, 8 * g_gameConfig.sfxDatapadVolume, 63);
			}
			FrontImage_AdvanceSpriteFrame("doors", 0);

			if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
				textWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT), 15);
			} else {
				textWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_BACK_TO_CONCOURSE), 15);
			}
			rect.left = 300 - (textWidth >> 1);
			rect.top = 400;
			rect.right = textWidth + rect.left + 40;
			rect.bottom = 440;

			if (FrontImage_GetSpriteFrame("doors") != 0) {
				FrontImage_DrawSprite("doors", 299, 90);
			}
			FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_combatSimMenuTranslucentFillColor);
			FrontendText_DrawCentered(15, FrontendString_Get(STR_BACK_TO_CONCOURSE), &rect, 0xffff);

			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
			}
		} else {
			if (FrontImage_GetSpriteFrame("doors") == 1) {
				FrontendSound_PlayUISound("tourdoorclose", 1, 0, 255, 8 * g_gameConfig.sfxDatapadVolume, 63);
			}
			FrontImage_RewindSpriteFrame("doors", 0);
			if (FrontImage_GetSpriteFrame("doors") != 0) {
				FrontImage_DrawSprite("doors", 299, 90);
			}
		}
	}

	if (Frontend_HandleEscapeQuit(1) == 1) {
		return 1;
	}
	return 0;
}

// FUNCTION: XWA 0x53B4C0
int CombatSimMenu_Exit(int frameCounter) {
	(void)frameCounter;

	FrontImage_FreeResourceByName("background");
	FrontImage_FreeResourceByName("multi");
	FrontImage_FreeResourceByName("doors");
	FrontImage_FreeResourceByName("single");
	Frontend_ResetScrollableControls();
	return 0;
}

// FUNCTION: XWA 0x53C120
int MultiplayerSetup_Update(int frameCounter) {
	(void)frameCounter;

	/* TODO: Dependency stub for CombatSimMenu_Update. Reimplement MultiplayerSetup_Update @ 0x53C120. */
	return 0;
}

#ifdef XWA_MODERN
int MultiplayerSetup_Exit(int frameCounter) {
	(void)frameCounter;

	FrontImage_FreeResourceByName("background");
	MultiplayerSetup_FreeTransportSprites();
	Keyboard_FlushCharBuffer();
	Frontend_ResetScrollableControls();
	strcpy(g_pilotData.multiplayerHostName, g_pilotData.multiplayerGameName);
	return 0;
}
#else
// FUNCTION: XWA 0x53C0D0
void MultiplayerSetup_Exit(int frameCounter) {
	(void)frameCounter;

	FrontImage_FreeResourceByName("background");
	MultiplayerSetup_FreeTransportSprites();
	Keyboard_FlushCharBuffer();
	Frontend_ResetScrollableControls();
	memcpy(g_pilotData.multiplayerHostName, g_pilotData.multiplayerGameName,
		   strlen(g_pilotData.multiplayerGameName) + 1);
}
#endif
