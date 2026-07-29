#include "xwa/config/game_config.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/linez.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/audio/cd_audio.h"
#include "xwa/audio/music.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/cutscene.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_dialog.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/frontend_wave_stream.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/frontend/model_preview.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/movie/movie.h"
#include "xwa/net/net.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/time.h"

#ifdef XWA_MODERN
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/config/modern_input_options_screen.h"
#include "xwa_runtime/config/modern_video_options.h"
#include "xwa_runtime/config/modern_video_options_screen.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0xB0C7A0
GameConfig g_gameConfig;
// GLOBAL: XWA 0xB0CE20
int g_configRestrictedOptionsModalActive;
// GLOBAL: XWA 0xABD7C0
int g_configDatapadQuitConfirmed;
// GLOBAL: XWA 0x7829A0
int g_menuCursorRow;
// GLOBAL: XWA 0xB0C784
int g_configMenuCenterX;
// GLOBAL: XWA 0x782984
uint16_t* g_configJoystickActionPickerBoundActionCode;
// GLOBAL: XWA 0x78298C
int g_configMenuScrollIndex;
// GLOBAL: XWA 0x782990
char (*g_configCreditsLineBuffer)[128];
// GLOBAL: XWA 0x782994
int g_configCreditsScrollTopY;
// GLOBAL: XWA 0x782998
unsigned int g_configJoystickActionPickerSelectedIndex;
// GLOBAL: XWA 0x78299C
int g_cutsceneViewerScrollRow;
// GLOBAL: XWA 0x7829A8
int g_configJoystickActionPickerPanelColor;
// GLOBAL: XWA 0x782988
int g_cutsceneViewerSelectedRow;
// GLOBAL: XWA 0x7829A4
int g_cutsceneViewerPlayableRowCount;
// GLOBAL: XWA 0x7829AC
int g_configAuxScreenFrameCount;
// GLOBAL: XWA 0x7829B0
unsigned int g_configCreditsLineCount;
// GLOBAL: XWA 0xB0CA0C
int g_frontendSetupNeedsBaseRedraw;

#ifdef XWA_MODERN
static int g_configRestrictedOptionsModalRunActive;
static int g_configExitConfirmPending;
#endif
static int g_configOptionsDatapadInitialized;
#ifdef XWA_MODERN
static int g_configOptionsDatapadOpeningMovie;
static int g_configOptionsDatapadClosingMovie;
static int g_configCutscenePlaybackRow;
static int g_configCutscenePlaybackIndex;
#endif
#ifdef XWA_MODERN
static int g_configJoystickActionPickerRunActive;
static int g_configJoystickActionPickerSavedGlyphGradientBg;
#endif
#ifdef XWA_MODERN
static int g_configTextEditRunActive;
#endif
// GLOBAL: XWA 0x7829B4
uint32_t g_menuKeyRepeatTime;
// GLOBAL: XWA 0x7829B8
int g_pendingMenuScreen;

// GLOBAL: XWA 0xABC96C (defined in flight.c): which game CD is in the drive.
extern int g_currentCdDisk;

enum {
	CONFIG_MENU_KEY_REPEAT_MS = 100,
	CONFIG_KEY_ENTER = '\r',
	CONFIG_KEY_ESCAPE = 0x1b,
	CONFIG_KEY_VK_LEFT = 0x25,
	CONFIG_KEY_VK_UP = 0x26,
	CONFIG_KEY_VK_RIGHT = 0x27,
	CONFIG_KEY_VK_DOWN = 0x28,
};

typedef struct JoystickEntry {
	uint16_t actionCode;
	char name[20];
	char description[128];
} JoystickEntry;

typedef struct ConfigNumericStepperRepeatState {
	int leftHoldFrames;
	int rightHoldFrames;
} ConfigNumericStepperRepeatState;

// GLOBAL: XWA 0xB07C80
JoystickEntry g_joystickEntries[128];
// GLOBAL: XWA 0xB0C780
unsigned int g_joystickEntryCount;
// GLOBAL: XWA 0xB0CA20
ConfigNumericStepperRepeatState g_configNumericStepperRepeatByButtonId[128];

// FUNCTION: XWA 0x51D3D0
int Config_MainMenuScreen(void) {
	char keyState;
	const char* text;
	const char* line1;
	const char* line2;
	const char* line3;
	const char* okayLabel;
	const char* cancelLabel;
	int rowCount;
	int y;
	int rowIndex;
	int textX;
	int buttonPressed;
	unsigned int titleWidth;
	FrontendRect rect;
	char versionText[5];

#ifdef XWA_MODERN
	if (g_configExitConfirmPending) {
		int confirmResult;

		if (!FrontendDialog_TakeConfirmDialogResult(&confirmResult)) {
			return 0;
		}
		g_configExitConfirmPending = 0;
		if (confirmResult) {
			g_configDatapadQuitConfirmed = 1;
			return 1;
		}
	}
#endif

	rowCount = g_configRestrictedOptionsModalActive ? 7 : 10;
	y = 215 - ((25 * rowCount) >> 1);
	keyState = (char)Config_GetMenuNavKey();
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
			g_menuCursorRow = rowCount - 1;
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (++g_menuCursorRow >= rowCount) {
			g_menuCursorRow = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	text = FrontendString_Get(STR_CONFIG_MAIN_MENU);
	FrontendText_DrawCentered(20, text, &rect, g_colorLightBlue);
	text = FrontendString_Get(STR_CONFIG_MAIN_MENU);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	FrontendDraw_Line(g_configMenuCenterX - (int)(titleWidth >> 1), y + 17,
					  g_configMenuCenterX - (int)(titleWidth >> 1) + (int)titleWidth, y + 17,
					  g_colorLightBlue);

	y += 25;
	memcpy(versionText, "2.02", sizeof(versionText));
	sprintf(g_frontendScratchBuffer, "%s %s", FrontendString_Get(STR_CONFIG_VERSION), versionText);
	FrontendText_Draw(10, g_frontendScratchBuffer, 35, 420, g_colorPaleBlue);

	rowIndex = 0;
	text = FrontendString_Get(STR_CONFIG_GENERAL_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 20, g_colorPaleBlue, 20, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(20, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed = 1;
		}
	}
	if (buttonPressed) {
		g_activeTextFieldId = 0;
		g_pendingMenuScreen = 1;
		g_menuCursorRow = 0;
	}

	y += 25;
	++rowIndex;
	text = FrontendString_Get(STR_CONFIG_PERFORMANCE_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 20, g_colorPaleBlue, 28, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(20, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed = 1;
		}
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 16;
		g_menuCursorRow = g_gameConfig.performance;
	}

	y += 25;
	++rowIndex;
	text = FrontendString_Get(STR_CONFIG_VIDEO_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 20, g_colorPaleBlue, 21, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(20, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed = 1;
		}
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 2;
		g_menuCursorRow = 0;
	}

	y += 25;
	++rowIndex;
	text = FrontendString_Get(STR_CONFIG_SOUND_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 20, g_colorPaleBlue, 22, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(20, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed = 1;
		}
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 3;
		g_menuCursorRow = 0;
	}

	y += 25;
	++rowIndex;
	text = FrontendString_Get(STR_CONFIG_GAME_CONTROLLER_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 20, g_colorPaleBlue, 23, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(20, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed = 1;
		}
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 4;
		g_menuCursorRow = 0;
	}

	y += 25;
	++rowIndex;
	if (!g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_NETWORK_OPTIONS);
		titleWidth = FrontendText_MeasureWidth(text, 20);
		textX = g_configMenuCenterX - (int)(titleWidth >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 20, g_colorPaleBlue, 29, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(20, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed = 1;
			}
		}
		if (buttonPressed) {
			g_pendingMenuScreen = 17;
			g_menuCursorRow = 0;
		}

		y += 25;
		++rowIndex;
		text = FrontendString_Get(STR_CONFIG_VIEW_CREDITS);
		titleWidth = FrontendText_MeasureWidth(text, 20);
		textX = g_configMenuCenterX - (int)(titleWidth >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 20, g_colorPaleBlue, 26, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(20, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed = 1;
			}
		}
		if (buttonPressed) {
			g_pendingMenuScreen = 14;
			g_menuCursorRow = 0;
			g_configAuxScreenFrameCount = 0;
		}

		y += 25;
		++rowIndex;
		text = FrontendString_Get(STR_VIEW_CUTSCENES);
		titleWidth = FrontendText_MeasureWidth(text, 20);
		textX = g_configMenuCenterX - (int)(titleWidth >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 20, g_colorPaleBlue, 27, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(20, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed = 1;
			}
		}
		if (buttonPressed) {
			g_pendingMenuScreen = 15;
			g_configAuxScreenFrameCount = 0;
			g_menuCursorRow = 0;
		}

		y += 25;
		++rowIndex;
	}

	text = FrontendString_Get(STR_CONFIG_RETURN_TO_GAME);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 20, g_colorPaleBlue, 24, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(20, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed = 1;
		}
	}
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		keyState = 0;
		buttonPressed = 1;
	}
	if (buttonPressed) {
		return 1;
	}

	y += 25;
	++rowIndex;
#ifdef XWA_MODERN
	text = "Exit The Game";
#else
	text = FrontendString_Get(STR_CONFIG_EXIT_TO_WINDOWS);
#endif
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 20, g_colorPaleBlue, 25, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(20, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			buttonPressed = 1;
		}
	}
	if (!buttonPressed) {
		return 0;
	}

	if (!g_frontendSinglePlayerFlightSessionActive ||
		g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		cancelLabel = FrontendString_Get(STR_CANCEL);
		okayLabel = FrontendString_Get(STR_OKAY);
		line3 = FrontendString_Get(STR_ARE_YOU_SURE3);
		line2 = FrontendString_Get(STR_ARE_YOU_SURE2);
		line1 = FrontendString_Get(STR_ARE_YOU_SURE1);
	} else if (Net_IsHost()) {
		cancelLabel = FrontendString_Get(STR_CANCEL);
		okayLabel = FrontendString_Get(STR_OKAY);
		line3 = FrontendString_Get(STR_HOSTING_GAME3);
		line2 = FrontendString_Get(STR_HOSTING_GAME2);
		line1 = FrontendString_Get(STR_HOSTING_GAME1);
	} else {
		cancelLabel = FrontendString_Get(STR_CANCEL);
		okayLabel = FrontendString_Get(STR_OKAY);
		line3 = FrontendString_Get(STR_GAME_IN_PROGRESS3);
		line2 = FrontendString_Get(STR_GAME_IN_PROGRESS2);
		line1 = FrontendString_Get(STR_GAME_IN_PROGRESS1);
	}
#ifdef XWA_MODERN
	if (FrontendDialog_BeginConfirmDialog(line1, line2, line3, okayLabel, cancelLabel)) {
		g_configExitConfirmPending = 1;
	}
	return 0;
#else
	if (!FrontendDialog_ShowConfirmDialog(line1, line2, line3, okayLabel, cancelLabel)) {
		return 0;
	}

	g_configDatapadQuitConfirmed = 1;
	return 1;
#endif
}

// FUNCTION: XWA 0x51DE00
int Config_GeneralOptionsScreen(void) {
	char keyState;
	const char* text;
	int y;
	int rowIndex;
	int restoreDefaults;
	int textX;
	int buttonPressed;
	unsigned int titleWidth;
	FrontendRect rect;

	rowIndex = 0;
	restoreDefaults = 0;
	y = 40;
	keyState = (char)Config_GetMenuNavKey();
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
			g_menuCursorRow = 17;
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (++g_menuCursorRow >= 18) {
			g_menuCursorRow = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	FrontendText_DrawCentered(15, FrontendString_Get(STR_CONFIG_GENERAL_OPTIONS), &rect, g_colorLightBlue);
	titleWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_CONFIG_GENERAL_OPTIONS), 15);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	FrontendDraw_Line(textX, y + 17, textX + (int)titleWidth, y + 17, g_colorLightBlue);
	y += 20;

	if (!g_configRestrictedOptionsModalActive || g_pilotData.campaignMode) {
		Config_DrawOptionCycle(&g_gameConfig.tourDifficulty, STR_CONFIG_DIFFICULTY, STR_EASY, 3, &y,
							   &rowIndex, &keyState, 20);
		Config_DrawOptionCycle(&g_gameConfig.tourCollisions, STR_CONFIG_COLLISIONS, STR_CONFIG_NO, 2, &y,
							   &rowIndex, &keyState, 21);
		Config_DrawOptionCycle(&g_gameConfig.tourInvulnerable, STR_GAME_TOUR_INVULNERABLE, STR_CONFIG_NO, 2,
							   &y, &rowIndex, &keyState, 35);
		Config_DrawOptionCycle(&g_gameConfig.tourUnlimitedAmmo, STR_GAME_TOUR_UNLIMITED_AMMO, STR_CONFIG_NO,
							   2, &y, &rowIndex, &keyState, 36);
	} else {
		Config_DrawOptionCycle(&g_gameConfig.difficulty, STR_DIFFICULTY, STR_EASY, 3, &y, &rowIndex,
							   &keyState, 20);
		Config_DrawOptionCycle(&g_gameConfig.collisions, STR_COLLISIONS, STR_CONFIG_NO, 2, &y, &rowIndex,
							   &keyState, 21);
		Config_DrawOptionCycle(&g_gameConfig.invulnerable, STR_GAME_INVULNERABLE, STR_CONFIG_NO, 2, &y,
							   &rowIndex, &keyState, 35);
		Config_DrawOptionCycle(&g_gameConfig.unlimitedAmmo, STR_GAME_UNLIMITED_AMMO, STR_CONFIG_NO, 2, &y,
							   &rowIndex, &keyState, 36);
	}

	sprintf(g_frontendScratchBuffer, "%s 1", FrontendString_Get(STR_TAUNT_NUMBER));
	Config_DrawOptionTextEdit(g_gameConfig.taunt1, 46, 0, g_frontendScratchBuffer, &y, &rowIndex, &keyState,
							  22);
	sprintf(g_frontendScratchBuffer, "%s 2", FrontendString_Get(STR_TAUNT_NUMBER));
	Config_DrawOptionTextEdit(g_gameConfig.taunt2, 46, 1, g_frontendScratchBuffer, &y, &rowIndex, &keyState,
							  23);
	sprintf(g_frontendScratchBuffer, "%s 3", FrontendString_Get(STR_TAUNT_NUMBER));
	Config_DrawOptionTextEdit(g_gameConfig.taunt3, 46, 2, g_frontendScratchBuffer, &y, &rowIndex, &keyState,
							  24);
	sprintf(g_frontendScratchBuffer, "%s 4", FrontendString_Get(STR_TAUNT_NUMBER));
	Config_DrawOptionTextEdit(g_gameConfig.taunt4, 46, 3, g_frontendScratchBuffer, &y, &rowIndex, &keyState,
							  25);
	Config_DrawOptionNumericStepper(g_gameConfig.presetThrottle, STR_CONFIG_PRESET_1_THROTTLE, 15, 100, &y,
									&rowIndex, &keyState, 26);
	Config_DrawOptionSlider(g_gameConfig.presetLaser, STR_CONFIG_PRESET_1_LASER_LEVEL, STR_CONFIG_PRESET_ZERO,
							4, &y, &rowIndex, &keyState, 27);
	Config_DrawOptionSlider(g_gameConfig.presetShield, STR_CONFIG_PRESET_1_SHIELD_LEVEL,
							STR_CONFIG_PRESET_ZERO, 4, &y, &rowIndex, &keyState, 28);
	Config_DrawOptionSlider(g_gameConfig.presetBeam, STR_CONFIG_PRESET_1_BEAM_LEVEL, STR_CONFIG_PRESET_ZERO,
							4, &y, &rowIndex, &keyState, 29);
	Config_DrawOptionNumericStepper(&g_gameConfig.presetThrottle[1], STR_CONFIG_PRESET_2_THROTTLE, 15, 100,
									&y, &rowIndex, &keyState, 31);
	Config_DrawOptionSlider(&g_gameConfig.presetLaser[1], STR_CONFIG_PRESET_2_LASER_LEVEL,
							STR_CONFIG_PRESET_ZERO, 4, &y, &rowIndex, &keyState, 32);
	Config_DrawOptionSlider(&g_gameConfig.presetShield[1], STR_CONFIG_PRESET_2_SHIELD_LEVEL,
							STR_CONFIG_PRESET_ZERO, 4, &y, &rowIndex, &keyState, 33);
	Config_DrawOptionSlider(&g_gameConfig.presetBeam[1], STR_CONFIG_PRESET_2_BEAM_LEVEL,
							STR_CONFIG_PRESET_ZERO, 4, &y, &rowIndex, &keyState, 34);

	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		titleWidth = FrontendText_MeasureWidth(text, 15);
		FrontendText_Draw(15, text, g_configMenuCenterX - (int)(titleWidth >> 1), y, g_colorGray);
	} else {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		titleWidth = FrontendText_MeasureWidth(text, 15);
		textX = g_configMenuCenterX - (int)(titleWidth >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 49, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
		if (buttonPressed) {
			restoreDefaults = 1;
		}
	}

	y += 20;
	++rowIndex;
	text = FrontendString_Get(STR_BACK);
	titleWidth = FrontendText_MeasureWidth(text, 15);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 50, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		keyState = 0;
		buttonPressed = 1;
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 0;
		g_menuCursorRow = 0;
	}

	if (restoreDefaults) {
		buttonPressed = FrontendDialog_ShowConfirmDialog(
			FrontendString_Get(STR_CONFIG_OVERWRITE1), FrontendString_Get(STR_CONFIG_OVERWRITE2),
			FrontendString_Get(STR_CONFIG_OVERWRITE3), FrontendString_Get(STR_OKAY),
			FrontendString_Get(STR_CANCEL));
		if (buttonPressed != 0) {
			g_gameConfig.tourDifficulty = 1;
			g_gameConfig.tourCollisions = 1;
			g_gameConfig.tourUnlimitedAmmo = 0;
			g_gameConfig.tourInvulnerable = 0;
			strcpy(g_gameConfig.taunt1, FrontendString_Get(STR_TAUNT1));
			strcpy(g_gameConfig.taunt2, FrontendString_Get(STR_TAUNT2));
			strcpy(g_gameConfig.taunt3, FrontendString_Get(STR_TAUNT3));
			strcpy(g_gameConfig.taunt4, FrontendString_Get(STR_TAUNT4));
			g_gameConfig.presetThrottle[0] = 100;
			g_gameConfig.presetLaser[0] = 3;
			g_gameConfig.presetShield[0] = 2;
			g_gameConfig.presetBeam[0] = 2;
			g_gameConfig.presetThrottle[1] = 33;
			g_gameConfig.presetLaser[1] = 2;
			g_gameConfig.presetShield[1] = 2;
			g_gameConfig.presetBeam[1] = 2;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x51E9C0
int Config_SoundOptionsScreen(void) {
	char keyState;
	char pendingKeyState;
	const char* text;
	const char* line1;
	const char* line2;
	const char* line3;
	const char* okayLabel;
	const char* cancelLabel;
	int y;
	int rowIndex;
	int restoreDefaults;
	int textX;
	int buttonPressed;
	int changed;
	unsigned int titleWidth;
	FrontendRect rect;

	rowIndex = 0;
	restoreDefaults = 0;
	y = 70;
	keyState = (char)Config_GetMenuNavKey();
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
			g_menuCursorRow = 14;
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (++g_menuCursorRow >= 15) {
			g_menuCursorRow = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	text = FrontendString_Get(STR_CONFIG_SOUND_OPTIONS);
	FrontendText_DrawCentered(15, text, &rect, g_colorLightBlue);
	text = FrontendString_Get(STR_CONFIG_SOUND_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	FrontendDraw_Line(g_configMenuCenterX - (int)(titleWidth >> 1), y + 17,
					  g_configMenuCenterX - (int)(titleWidth >> 1) + (int)titleWidth, y + 17,
					  g_colorLightBlue);
	y += 20;

	Config_DrawOptionSlider(&g_gameConfig.datapadMusicVolume, STR_CONCOURSE_MUSIC_VOLUME, STR_CONFIG_VOL_OFF,
							10, &y, &rowIndex, &keyState, 20);
	g_gameConfig.datapadMusicEnabled = (g_gameConfig.datapadMusicVolume != 0);
	Config_DrawOptionSlider(&g_gameConfig.sfxDatapadVolume, STR_CONCOURSE_SFX_VOLUME, STR_CONFIG_VOL_OFF, 10,
							&y, &rowIndex, &keyState, 21);
	g_gameConfig.sfxDatapadEnabled = (g_gameConfig.sfxDatapadVolume != 0);
	Config_DrawOptionSlider(&g_gameConfig.musicVolume, STR_MUSIC, STR_CONFIG_VOL_OFF, 10, &y, &rowIndex,
							&keyState, 22);
	g_gameConfig.musicEnabled = (g_gameConfig.musicVolume != 0);

	if (g_configRestrictedOptionsModalActive) {
		Config_DrawOptionCycleDisabled(&g_gameConfig.sound3dEnabled, STR_3D_SOUND_ENABLED,
									   STR_CONFIG_3D_SOUND_OFF, 3, &y, &rowIndex, &keyState, 23);
	} else {
		Config_DrawOptionCycle(&g_gameConfig.sound3dEnabled, STR_3D_SOUND_ENABLED, STR_CONFIG_3D_SOUND_OFF, 3,
							   &y, &rowIndex, &keyState, 23);
	}

	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_NUMBER_OF_CHANNELS);
		textX = g_configMenuCenterX - FrontendText_MeasureWidth(text, 15) - 10;
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, 0xffff);
		} else {
			FrontendText_Draw(15, text, textX, y, g_colorGray);
		}
		sprintf(g_frontendScratchBuffer, "%-30d", g_gameConfig.numberOfSfx);
		FrontendText_Draw(15, g_frontendScratchBuffer, g_configMenuCenterX + 10, y, g_colorGray);
	} else {
		text = FrontendString_Get(STR_NUMBER_OF_CHANNELS);
		textX = g_configMenuCenterX - FrontendText_MeasureWidth(text, 15) - 10;
		changed = 0;
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			pendingKeyState = keyState;
			if (keyState == CONFIG_KEY_VK_LEFT) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				pendingKeyState = 0;
				keyState = 0;
				changed = 2;
			}
			if (pendingKeyState == CONFIG_KEY_VK_RIGHT) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				changed = 1;
			}
		} else {
			FrontendText_Draw(15, text, textX, y, g_colorLightBlue);
		}

		sprintf(g_frontendScratchBuffer, "%-30d", g_gameConfig.numberOfSfx);
		buttonPressed = FrontendButton_DrawMenuButton(g_configMenuCenterX + 10, y, g_frontendScratchBuffer,
													  15, g_colorPaleBlue, 24, 0, "settingsound") |
						changed;
		if (buttonPressed == 1) {
			++g_gameConfig.numberOfSfx;
			if (g_gameConfig.numberOfSfx >= 33) {
				g_gameConfig.numberOfSfx = 6;
			}
		} else if (buttonPressed == 2) {
			if (g_gameConfig.numberOfSfx == 6) {
				g_gameConfig.numberOfSfx = 32;
			} else {
				--g_gameConfig.numberOfSfx;
			}
		}
	}
	y += 20;
	++rowIndex;

	if (g_configRestrictedOptionsModalActive) {
		Config_DrawOptionCycleDisabled(&g_gameConfig.sfxQuality, STR_CONFIG_SFX_QUALITY, STR_CONFIG_NORMAL, 2,
									   &y, &rowIndex, &keyState, 34);
	} else {
		Config_DrawOptionCycle(&g_gameConfig.sfxQuality, STR_CONFIG_SFX_QUALITY, STR_CONFIG_NORMAL, 2, &y,
							   &rowIndex, &keyState, 34);
	}
	Config_DrawOptionSlider(&g_gameConfig.sfxExteriorVolume, STR_EXTERIOR_SFX, STR_CONFIG_VOL_OFF, 10, &y,
							&rowIndex, &keyState, 25);
	g_gameConfig.sfxExteriorEnabled = (g_gameConfig.sfxExteriorVolume != 0);
	Config_DrawOptionSlider(&g_gameConfig.sfxInteriorVolume, STR_INTERIOR_SFX, STR_CONFIG_VOL_OFF, 10, &y,
							&rowIndex, &keyState, 26);
	g_gameConfig.sfxInteriorEnabled = (g_gameConfig.sfxInteriorVolume != 0);
	Config_DrawOptionSlider(&g_gameConfig.sfxEngineVolume, STR_ENGINE_SOUND, STR_CONFIG_VOL_OFF, 10, &y,
							&rowIndex, &keyState, 27);
	g_gameConfig.sfxEngineEnabled = (g_gameConfig.sfxEngineVolume != 0);

	if (g_configRestrictedOptionsModalActive) {
		Config_DrawOptionCycleDisabled(&g_gameConfig.voicePilotEnabled, STR_PILOT_MESSAGES, STR_SOFF, 3, &y,
									   &rowIndex, &keyState, 28);
		Config_DrawOptionCycleDisabled(&g_gameConfig.voiceTacticalOfficerEnabled,
									   STR_TACTICAL_OFFICER_MESSAGES, STR_SOFF, 3, &y, &rowIndex, &keyState,
									   29);
		Config_DrawOptionCycleDisabled(&g_gameConfig.voiceSpecialEnabled, STR_SPECIAL_MESSAGES, STR_OFF, 2,
									   &y, &rowIndex, &keyState, 32);
	} else {
		Config_DrawOptionCycle(&g_gameConfig.voicePilotEnabled, STR_PILOT_MESSAGES, STR_SOFF, 3, &y,
							   &rowIndex, &keyState, 28);
		Config_DrawOptionCycle(&g_gameConfig.voiceTacticalOfficerEnabled, STR_TACTICAL_OFFICER_MESSAGES,
							   STR_SOFF, 3, &y, &rowIndex, &keyState, 29);
		Config_DrawOptionCycle(&g_gameConfig.voiceSpecialEnabled, STR_SPECIAL_MESSAGES, STR_OFF, 2, &y,
							   &rowIndex, &keyState, 32);
	}
	Config_DrawOptionSlider(&g_gameConfig.voiceVolume, STR_VOICE_VOLUME, STR_CONFIG_VOL_OFF, 10, &y,
							&rowIndex, &keyState, 33);

	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		titleWidth = FrontendText_MeasureWidth(text, 15);
		FrontendText_Draw(15, text, g_configMenuCenterX - (int)(titleWidth >> 1), y, g_colorGray);
	} else {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 49, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
		if (buttonPressed) {
			restoreDefaults = 1;
		}
	}

	y += 20;
	++rowIndex;
	text = FrontendString_Get(STR_BACK);
	textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 50, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		keyState = 0;
		buttonPressed = 1;
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 0;
		g_menuCursorRow = 3;
	}

	if (restoreDefaults == 1) {
		cancelLabel = FrontendString_Get(STR_CANCEL);
		okayLabel = FrontendString_Get(STR_OKAY);
		line3 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL3);
		line2 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL2);
		line1 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL1);
		if (FrontendDialog_ShowConfirmDialog(line1, line2, line3, okayLabel, cancelLabel)) {
			g_gameConfig.sfxDatapadVolume = 9;
			g_gameConfig.sfxExteriorVolume = 9;
			g_gameConfig.sfxInteriorVolume = 9;
			g_gameConfig.voiceVolume = 9;
			g_gameConfig.voicePilotEnabled = 1;
			g_gameConfig.voiceTacticalOfficerEnabled = 1;
			g_gameConfig.voiceCommanderEnabled = 1;
			g_gameConfig.voiceSpecialEnabled = 1;
			g_gameConfig.musicEnabled = 1;
			g_gameConfig.musicVolume = 5;
			g_gameConfig.numberOfSfx = 12;
			g_gameConfig.sfxExteriorEnabled = 1;
			g_gameConfig.sfxInteriorEnabled = 1;
			g_gameConfig.sfxEngineEnabled = 1;
			g_gameConfig.sfxDatapadEnabled = 1;
			g_gameConfig.datapadMusicEnabled = 1;
			g_gameConfig.sfxEngineVolume = 5;
			g_gameConfig.datapadMusicVolume = 5;
			g_gameConfig.sound3dEnabled = 0;
			g_gameConfig.sfxQuality = 0;
			if (g_gameConfig.performance) {
				if (g_gameConfig.performance == 1) {
					Config_SetDetailDefaultsMedium(64);
					return 0;
				}
				if (g_gameConfig.performance == 2) {
					Config_SetDetailDefaultsHigh(64);
					return 0;
				}
			} else {
				Config_SetDetailDefaultsLow(64);
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x51F300
int Config_ControllerOptionsScreen(void) {
	char keyState;
	const char* text;
	int y;
	int rowIndex;
	int textX;
	int buttonPressed;
	int restoreDefaults;
	unsigned int titleWidth;
	FrontendRect rect;

	rowIndex = 0;
	restoreDefaults = 0;
	y = 130;
	keyState = (char)Config_GetMenuNavKey();
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
#ifdef XWA_MODERN
			g_menuCursorRow = 9;
#else
			g_menuCursorRow = 8;
#endif
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
#ifdef XWA_MODERN
		if (++g_menuCursorRow >= 10) {
#else
		if (++g_menuCursorRow >= 9) {
#endif
			g_menuCursorRow = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	FrontendText_DrawCentered(15, FrontendString_Get(STR_CONFIG_GAME_CONTROLLER_OPTIONS), &rect,
							  g_colorLightBlue);
	titleWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_CONFIG_GAME_CONTROLLER_OPTIONS), 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	FrontendDraw_Line(textX, y + 17, textX + (int)titleWidth, y + 17, g_colorLightBlue);

	y += 20;
	Config_DrawOptionCycle(&g_gameConfig.rudderEnabled, STR_CONFIG_RUDDER_ENABLED, STR_CONFIG_NO, 2, &y,
						   &rowIndex, &keyState, 20);
	Config_DrawOptionCycle(&g_gameConfig.flipRudder, STR_CONFIG_FLIP_RUDDER, STR_CONFIG_NO, 2, &y, &rowIndex,
						   &keyState, 21);
	Config_DrawOptionCycle(&g_gameConfig.ffEnabled, STR_CONFIG_FF_ENABLED, STR_CONFIG_NO, 2, &y, &rowIndex,
						   &keyState, 22);
	Config_DrawOptionSlider(&g_gameConfig.ffStrength, STR_CONFIG_FF_STRENGTH, STR_CONFIG_WEAK, 9, &y,
							&rowIndex, &keyState, 23);
	Config_DrawOptionSlider(&g_gameConfig.ffCenter, STR_CONFIG_FF_CENTER, STR_CONFIG_WEAK, 9, &y, &rowIndex,
							&keyState, 24);
	Config_DrawOptionCycle(&g_gameConfig.flipY, STR_CONFIG_FLIP_Y_AXIS, STR_CONFIG_NO, 2, &y, &rowIndex,
						   &keyState, 25);

#ifdef XWA_MODERN
	text = "OpenXWA Input Options";
	textX = g_configMenuCenterX - (int)((unsigned int)FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 51, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 19;
		g_menuCursorRow = 0;
	}
	y += 20;
	++rowIndex;
#endif

	text = FrontendString_Get(STR_CONFIG_REMAP_JOYSTICK_BUTTONS);
	textX = g_configMenuCenterX - (int)((unsigned int)FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 48, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 13;
		g_menuCursorRow = 0;
	}

	y += 20;
	++rowIndex;
	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		titleWidth = FrontendText_MeasureWidth(text, 15);
		FrontendText_Draw(15, text, g_configMenuCenterX - (int)(titleWidth >> 1), y, g_colorGray);
	} else {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		textX = g_configMenuCenterX - (int)((unsigned int)FrontendText_MeasureWidth(text, 15) >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 49, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
		if (buttonPressed) {
			restoreDefaults = 1;
		}
	}

	y += 20;
	++rowIndex;
	text = FrontendString_Get(STR_BACK);
	textX = g_configMenuCenterX - (int)((unsigned int)FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 50, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		keyState = 0;
		buttonPressed = 1;
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 0;
		g_menuCursorRow = 4;
	}

	if (restoreDefaults == 1) {
		buttonPressed =
			FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_RESTORING_DEFAULTS_WILL1),
											 FrontendString_Get(STR_RESTORING_DEFAULTS_WILL2),
											 FrontendString_Get(STR_RESTORING_DEFAULTS_WILL3),
											 FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL));
		if (buttonPressed) {
			g_gameConfig.rudderEnabled = 1;
			g_gameConfig.ffStrength = 6;
			g_gameConfig.ffCenter = 0;
			g_gameConfig.flipRudder = 0;
			g_gameConfig.ffEnabled = (uint8_t)ForceFeedback_CheckDevice();
			g_gameConfig.flipY = 0;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x51F870
int Config_JoystickRemapScreen(void) {
	unsigned int totalButtonRows;
	int physicalButtonCount;
	unsigned int displayedPhysicalButtonCount;
	int povDirection;
	int firstPressedButton;
	char keyState;
	char label[256];
	const char* text;
	int y;
	int rowIndex;
	int buttonIndex;
	int textX;
	int buttonPressed;
	int selectedBindingNumber;
	int restoreDefaults;
	int menuRowCount;
	int dialogResult;
	unsigned int titleWidth;
	unsigned int defaultIndex;
	FrontendRect rect;

	totalButtonRows = (unsigned int)Joystick_GetButtonCount(0);
	physicalButtonCount = (int)totalButtonRows;
	displayedPhysicalButtonCount = (unsigned int)physicalButtonCount;
	if (Joystick_HasPov(0)) {
		totalButtonRows += 4;
	}
	if (totalButtonRows > 16) {
		totalButtonRows = 16;
		if (displayedPhysicalButtonCount > 12) {
			displayedPhysicalButtonCount = 12;
		}
	}
	physicalButtonCount = displayedPhysicalButtonCount;

	rowIndex = 0;
	selectedBindingNumber = 0;
	restoreDefaults = 0;
	menuRowCount = (int)totalButtonRows + 2;
	y = 220 - ((20 * menuRowCount) >> 1);
	keyState = (char)Config_GetMenuNavKey();
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
			g_menuCursorRow = menuRowCount - 1;
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (++g_menuCursorRow >= menuRowCount) {
			g_menuCursorRow = 0;
		}
	}

	povDirection = Joystick_GetPovDirection(0);
	firstPressedButton = Joystick_GetFirstPressedButton(0);
	if ((firstPressedButton != -1 && firstPressedButton < physicalButtonCount) || povDirection) {
		if (firstPressedButton != -1 && firstPressedButton < physicalButtonCount) {
			if (g_menuCursorRow != firstPressedButton) {
				FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}
			g_menuCursorRow = firstPressedButton;
		} else if (povDirection) {
			firstPressedButton = povDirection + physicalButtonCount - 1;
			if (g_menuCursorRow != firstPressedButton) {
				FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}
			g_menuCursorRow = firstPressedButton;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	FrontendText_DrawCentered(15, FrontendString_Get(STR_CONFIG_REMAP_JOYSTICK_BUTTONS), &rect,
							  g_colorLightBlue);
	titleWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_CONFIG_REMAP_JOYSTICK_BUTTONS), 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	FrontendDraw_Line(textX, y + 17, textX + (int)titleWidth, y + 17, g_colorLightBlue);

	y += 20;
	for (buttonIndex = 0; (unsigned int)buttonIndex < totalButtonRows; ++buttonIndex) {
		if ((unsigned int)buttonIndex < displayedPhysicalButtonCount) {
			sprintf(label, "%s %d", FrontendString_Get(STR_JOYSTICK_BUTTON), buttonIndex + 1);
			buttonPressed = Config_DrawJoystickBindingRow(&g_gameConfig.joyButtons[buttonIndex], label, &y,
														  &rowIndex, &keyState, buttonIndex + 20);
		} else {
			unsigned int povIndex;

			povIndex = (unsigned int)buttonIndex - displayedPhysicalButtonCount;
			sprintf(label, "%s %s", FrontendString_Get(STR_JOYSTICK_POV),
					FrontendString_Get((UIString)(STR_UP + povIndex)));
			buttonPressed = Config_DrawJoystickBindingRow(
				&g_gameConfig.joyButtons[16 + buttonIndex - physicalButtonCount], label, &y, &rowIndex,
				&keyState, buttonIndex + 20);
		}
		if (buttonPressed) {
			selectedBindingNumber = buttonIndex + 1;
		}
	}

	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		titleWidth = FrontendText_MeasureWidth(text, 15);
		FrontendText_Draw(15, text, g_configMenuCenterX - (int)(titleWidth >> 1), y, g_colorGray);
	} else {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		textX = g_configMenuCenterX - (int)((unsigned int)FrontendText_MeasureWidth(text, 15) >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 49, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
		if (buttonPressed) {
			restoreDefaults = 1;
		}
	}

	y += 20;
	++rowIndex;
	text = FrontendString_Get(STR_BACK);
	textX = g_configMenuCenterX - (int)((unsigned int)FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 50, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		keyState = 0;
		buttonPressed = 1;
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 4;
		g_menuCursorRow = 6;
	}

	if (restoreDefaults == 1) {
		dialogResult =
			FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_RESTORING_DEFAULTS_WILL1),
											 FrontendString_Get(STR_RESTORING_DEFAULTS_WILL2),
											 FrontendString_Get(STR_RESTORING_DEFAULTS_WILL3),
											 FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL));
		if (dialogResult != 0) {
			memset(g_gameConfig.joyButtons, 0, sizeof(g_gameConfig.joyButtons));
			for (defaultIndex = 0; defaultIndex < 16; ++defaultIndex) {
				g_gameConfig.joyButtons[defaultIndex] = 0;
				if (defaultIndex < displayedPhysicalButtonCount) {
					switch (defaultIndex) {
						case 0:
							g_gameConfig.joyButtons[defaultIndex] = 156;
							break;
						case 1:
							g_gameConfig.joyButtons[defaultIndex] = 157;
							break;
						case 2:
							g_gameConfig.joyButtons[defaultIndex] = 114;
							break;
						case 3:
							g_gameConfig.joyButtons[defaultIndex] = 108;
							break;
						case 4:
							g_gameConfig.joyButtons[defaultIndex] = 101;
							break;
						case 5:
							g_gameConfig.joyButtons[defaultIndex] = 105;
							break;
						case 6:
							g_gameConfig.joyButtons[defaultIndex] = 91;
							break;
						case 7:
							g_gameConfig.joyButtons[defaultIndex] = 8;
							break;
						case 8:
							g_gameConfig.joyButtons[defaultIndex] = 13;
							break;
						case 9:
							g_gameConfig.joyButtons[defaultIndex] = 93;
							break;
						default:
							break;
					}
				}
			}
			g_gameConfig.joyButtons[16] = 186;
			g_gameConfig.joyButtons[17] = 184;
			g_gameConfig.joyButtons[18] = 180;
			g_gameConfig.joyButtons[19] = 182;
		}
	}

	if (selectedBindingNumber) {
		int selectedIndex;

		selectedIndex = selectedBindingNumber - 1;
		if ((unsigned int)selectedIndex < displayedPhysicalButtonCount) {
			sprintf(label, "%s %d", FrontendString_Get(STR_JOYSTICK_BUTTON), selectedIndex + 1);
			Config_RunJoystickActionPicker(label, &g_gameConfig.joyButtons[selectedIndex]);
		} else {
			unsigned int povIndex;

			povIndex = (unsigned int)selectedIndex - displayedPhysicalButtonCount;
			sprintf(label, "%s %s", FrontendString_Get(STR_JOYSTICK_POV),
					FrontendString_Get((UIString)(STR_UP + povIndex)));
			Config_RunJoystickActionPicker(label, &g_gameConfig.joyButtons[16 + povIndex]);
		}
	}

	return 0;
}

// FUNCTION: XWA 0x51FF30
int Config_SinglePlayerVideoOptionsScreen(void) {
	unsigned int driverIndex;
	char keyState;
	const char* text;
	const char* line1;
	const char* line2;
	const char* line3;
	const char* okayLabel;
	const char* cancelLabel;
	int y;
	int rowIndex;
	int action;
	int textX;
	int buttonPressed;
	unsigned int titleWidth;
	unsigned int driverCount;
	DisplayDriverEntry* drivers;
	FrontendRect rect;

	driverIndex = 0;
	rowIndex = 0;
	action = 0;
	y = 40;
	keyState = (char)Config_GetMenuNavKey();
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
			g_menuCursorRow = 17;
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (++g_menuCursorRow >= 18) {
			g_menuCursorRow = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	text = FrontendString_Get(STR_CONFIG_VIDEO_SINGLE_PLAYER_OPTIONS);
	FrontendText_DrawCentered(15, text, &rect, g_colorLightBlue);
	text = FrontendString_Get(STR_CONFIG_VIDEO_SINGLE_PLAYER_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	FrontendDraw_Line(g_configMenuCenterX - (int)(titleWidth >> 1), y + 17,
					  g_configMenuCenterX - (int)(titleWidth >> 1) + (int)titleWidth, y + 17,
					  g_colorLightBlue);
	y += 20;

	Config_DrawVideoOptionRows(0, &y, &rowIndex, &keyState);

	if (g_gameConfig.use3dHardware[0]) {
		text = FrontendString_Get(STR_CONFIG_SP_HARDWARE_VIDEO_OPTIONS);
	} else {
		text = FrontendString_Get(STR_CONFIG_SP_SOFTWARE_VIDEO_OPTIONS);
	}
	textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 20, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (buttonPressed) {
		g_menuCursorRow = 0;
		g_pendingMenuScreen = 10 - (g_gameConfig.use3dHardware[0] != 0);
	}

	y += 20;
	++rowIndex;
	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_SAME_AS_MULTIPLAYER);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, 0xffff);
		} else {
			FrontendText_Draw(15, text, textX, y, g_colorGray);
		}
	} else {
		text = FrontendString_Get(STR_CONFIG_SAME_AS_MULTIPLAYER);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 21, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
		if (buttonPressed) {
			action = 1;
		}
	}

	y += 20;
	++rowIndex;
	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		titleWidth = FrontendText_MeasureWidth(text, 15);
		FrontendText_Draw(15, text, g_configMenuCenterX - (int)(titleWidth >> 1), y, g_colorGray);
	} else {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 22, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
		if (buttonPressed) {
			action = 2;
		}
	}

	y += 20;
	++rowIndex;
	text = FrontendString_Get(STR_BACK);
	textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 23, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		keyState = 0;
		buttonPressed = 1;
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 2;
#ifdef XWA_MODERN
		g_menuCursorRow = 1;
#else
		g_menuCursorRow = 0;
#endif
	}

	if (action == 1) {
		cancelLabel = FrontendString_Get(STR_CANCEL);
		okayLabel = FrontendString_Get(STR_OKAY);
		line3 = FrontendString_Get(STR_CONFIG_OVERWRITE3);
		line2 = FrontendString_Get(STR_CONFIG_OVERWRITE2);
		line1 = FrontendString_Get(STR_CONFIG_OVERWRITE1);
		if (FrontendDialog_ShowConfirmDialog(line1, line2, line3, okayLabel, cancelLabel)) {
			g_gameConfig.threedDevice[0] = g_gameConfig.threedDevice[1];
			g_gameConfig.use3dHardware[0] = g_gameConfig.use3dHardware[1];
			g_gameConfig.screenRes[0] = g_gameConfig.screenRes[1];
			g_gameConfig.brightness[0] = g_gameConfig.brightness[1];
			g_gameConfig.debris[0] = g_gameConfig.debris[1];
			g_gameConfig.backdrop[0] = g_gameConfig.backdrop[1];
			g_gameConfig.starDensity[0] = g_gameConfig.starDensity[1];
			g_gameConfig.lod[0] = g_gameConfig.lod[1];
			g_gameConfig.yardLod[0] = g_gameConfig.yardLod[1];
			g_gameConfig.textureRes[0] = g_gameConfig.textureRes[1];
			g_gameConfig.localLights[0] = g_gameConfig.localLights[1];
			g_gameConfig.diffuse[0] = g_gameConfig.diffuse[1];
			g_gameConfig.explosionRes[0] = g_gameConfig.explosionRes[1];
			DebugPrintf(NULL);
			return 0;
		}
	} else if (action == 2) {
		cancelLabel = FrontendString_Get(STR_CANCEL);
		okayLabel = FrontendString_Get(STR_OKAY);
		line3 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL3);
		line2 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL2);
		line1 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL1);
		if (FrontendDialog_ShowConfirmDialog(line1, line2, line3, okayLabel, cancelLabel)) {
			drivers = FrontendDisplay_GetDriverTable(&driverCount);
			g_gameConfig.threedDevice[0] = 0;
			g_gameConfig.use3dHardware[0] = 0;
			for (driverIndex = 0; driverIndex < driverCount; ++driverIndex) {
				unsigned int charIndex;

				for (charIndex = 0; drivers[driverIndex].name[charIndex] != '\0'; ++charIndex) {
					g_frontendScratchBuffer[charIndex] =
						(char)tolower((unsigned char)drivers[driverIndex].name[charIndex]);
				}
				g_frontendScratchBuffer[charIndex] = '\0';
				if (strstr(g_frontendScratchBuffer, "voodoo") || strstr(g_frontendScratchBuffer, "3dfx")) {
					g_gameConfig.threedDevice[0] = (uint8_t)driverIndex;
					g_gameConfig.use3dHardware[0] = 1;
					break;
				}
			}

			g_gameConfig.screenRes[0] = 0;
			g_gameConfig.brightness[0] = 2;
			g_gameConfig.debris[0] = 1;
			g_gameConfig.debrisDensity[0] = 4;
			g_gameConfig.backdrop[0] = 1;
			g_gameConfig.starDensity[0] = 2;
			g_gameConfig.lod[0] = 10;
			g_gameConfig.yardLod[0] = 10;
			g_gameConfig.textureRes[0] = 1;
			g_gameConfig.localLights[0] = 2;
			g_gameConfig.diffuse[0] = 1;
			g_gameConfig.explosionRes[0] = 1;
			DebugPrintf(NULL);
			if (g_gameConfig.performance) {
				if (g_gameConfig.performance == 1) {
					Config_SetDetailDefaultsMedium(1);
					return 0;
				}
				if (g_gameConfig.performance == 2) {
					Config_SetDetailDefaultsHigh(1);
					return 0;
				}
			} else {
				Config_SetDetailDefaultsLow(1);
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x520730
int Config_MultiplayerVideoOptionsScreen(void) {
	/* TODO: Reimplement Config_MultiplayerVideoOptionsScreen @ 0x520730. */
	return 0;
}

// FUNCTION: XWA 0x521E70
int Config_SinglePlayerSoftwareVideoScreen(void) {
	/* TODO: Reimplement Config_SinglePlayerSoftwareVideoScreen @ 0x521E70. */
	return 0;
}

// FUNCTION: XWA 0x5229F0
int Config_MultiplayerSoftwareVideoScreen(void) {
	/* TODO: Reimplement Config_MultiplayerSoftwareVideoScreen @ 0x5229F0. */
	return 0;
}

// FUNCTION: XWA 0x526D20
int Config_CreditsScreen(void) {
	XwaFile* stream;
	int loaded;
	unsigned int lineIndex;
	int lineTop;
	int lineBottom;
	int lineColor;
	FrontendRect rect;

	if (g_configAuxScreenFrameCount == 0) {
		if (!g_configRestrictedOptionsModalActive && g_gameConfig.datapadMusicEnabled) {
			Music_ResumeIfInitialized();
			Music_SetState(MUSIC_STATE_CLIMAX);
		}

		loaded = 1;
#ifdef XWA_MODERN
		stream = File_OpenAsset(AERON_VFS_ROOT_ASSET, "credits.txt", "r");
#else
		stream = File_Open(AERON_VFS_ROOT_ASSET, "credits.txt", "r");
#endif
		if (stream == NULL) {
			loaded = 0;
		} else {
			g_configCreditsLineCount = 0;
#ifdef XWA_MODERN
			for (;;) {
				if (!File_ReadLine(stream, g_frontendScratchBuffer, sizeof(g_frontendScratchBuffer))) {
					break;
				}
				++g_configCreditsLineCount;
			}
#else
			{
				char* readLine;

				readLine = fgets(g_frontendScratchBuffer, sizeof(g_frontendScratchBuffer), stream);
				while (readLine != NULL) {
					++g_configCreditsLineCount;
					readLine = fgets(g_frontendScratchBuffer, sizeof(g_frontendScratchBuffer), stream);
				}
			}
#endif

			if (g_configCreditsLineBuffer != NULL) {
				Mem_Free(g_configCreditsLineBuffer);
				g_configCreditsLineBuffer = NULL;
			}

			g_configCreditsLineBuffer = (char (*)[128])Mem_Alloc((size_t)g_configCreditsLineCount *
																 sizeof(*g_configCreditsLineBuffer));
			if (g_configCreditsLineBuffer == NULL) {
				loaded = 0;
			} else {
				File_Seek(stream, 0, SEEK_SET);
				for (lineIndex = 0; lineIndex < g_configCreditsLineCount; ++lineIndex) {
#ifdef XWA_MODERN
					size_t lineLength;

					if (!File_ReadLine(stream, g_frontendScratchBuffer, sizeof(g_frontendScratchBuffer))) {
						break;
					}

					lineLength = strlen(g_frontendScratchBuffer);
					if (lineLength != 0 && g_frontendScratchBuffer[lineLength - 1] == '\n') {
						g_frontendScratchBuffer[lineLength - 1] = '\0';
					}
#else
					if (fgets(g_frontendScratchBuffer, sizeof(g_frontendScratchBuffer), stream) == NULL) {
						break;
					}

					if (g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] == '\n') {
						g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] = '\0';
					}
#endif
					strncpy(g_configCreditsLineBuffer[lineIndex],
							Linez_ResolveString(g_frontendScratchBuffer),
							sizeof(g_configCreditsLineBuffer[0]));
				}
			}

			File_Close(stream);
		}

		if (!loaded) {
			if (g_configCreditsLineBuffer != NULL) {
				Mem_Free(g_configCreditsLineBuffer);
				g_configCreditsLineBuffer = NULL;
			}

			g_pendingMenuScreen = 0;
			g_menuCursorRow = 0;
			if (!g_configRestrictedOptionsModalActive && g_gameConfig.datapadMusicEnabled) {
				Music_PauseIfInitialized();
			}
			return 0;
		}

		g_configCreditsScrollTopY = 435;
		FrontendCursor_Hide();
		FrontendDisplay_SetFrameRate(60);
	}

	FrontendDraw_RectAssign(&rect, 0, 25, 640, 435);
	FrontendDisplay_SetScreenClipRect640x480(&rect);

	lineTop = g_configCreditsScrollTopY;
	lineColor = g_colorLightBlue;
	lineIndex = 0;
	if (lineIndex < g_configCreditsLineCount) {
		lineBottom = g_configCreditsScrollTopY + 15;
		for (; lineIndex < g_configCreditsLineCount; ++lineIndex) {
			if (lineBottom > 450) {
				break;
			}

			if (lineTop < 25 - (int)(15 * g_configCreditsLineCount)) {
				g_configCreditsScrollTopY = 435;
			}

			if (lineBottom < 25) {
				lineTop += 15;
				lineBottom += 15;
				continue;
			}

			FrontendDraw_RectAssign(&rect, 0, lineTop, 640, lineBottom);
			FrontendText_DrawAlignedInRect(12, g_configCreditsLineBuffer[lineIndex], &rect, 1, 0, lineColor);
			lineColor = g_colorLightBlue;
			if (g_configCreditsLineBuffer[lineIndex][0] != '\0') {
				lineColor = g_colorPaleBlue;
			}

			lineTop += 15;
			lineBottom += 15;
		}
	}

	--g_configCreditsScrollTopY;
	FrontendDisplay_ResetScreenClipRect();

	{
		int exitCredits;
		char keyState;

		exitCredits = 0;
		keyState = (char)Config_GetMenuNavKey();
		if (keyState == CONFIG_KEY_ESCAPE || FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick() ||
			keyState == CONFIG_KEY_ENTER || keyState == ' ') {
			Keyboard_FlushCharBuffer();
			exitCredits = 1;
		}

		if (exitCredits) {
			if (g_configCreditsLineBuffer != NULL) {
				Mem_Free(g_configCreditsLineBuffer);
				g_configCreditsLineBuffer = NULL;
			}
			FrontendCursor_Show();
			g_pendingMenuScreen = 0;
			g_menuCursorRow = 6;
			FrontendDisplay_SetFrameRate(24);
			if (g_gameConfig.datapadMusicEnabled) {
				Music_PauseIfInitialized();
			}
		} else {
			++g_configAuxScreenFrameCount;
		}
	}

	return 0;
}

#ifdef XWA_MODERN
static int Config_ContinueCutscenePlayback(void) {
	static const char* const introMovies[] = {
		"logofinal",
		"tgintro",
		"intro_final",
	};

	if (g_configCutscenePlaybackRow == 1) {
		while (!g_movieSkipRequested &&
			   g_configCutscenePlaybackIndex < (int)(sizeof(introMovies) / sizeof(introMovies[0]))) {
			const char* movie = introMovies[g_configCutscenePlaybackIndex++];
			if (Movie_Play(movie, 0)) {
				return 1;
			}
		}
		return 0;
	}

	if (g_configCutscenePlaybackIndex == 0) {
		g_configCutscenePlaybackIndex = 1;
		if (Movie_Play(g_cutsceneTable[g_configCutscenePlaybackRow - 2].movieName, 0)) {
			return 1;
		}
	}
	return 0;
}

static void Config_FinishCutscenePlayback(void) {
	g_configCutscenePlaybackRow = 0;
	g_configCutscenePlaybackIndex = 0;
	FrontendDisplay_ResetScreenClipRect();
	Config_DrawBackgroundToScreens();
	CDAudio_RequestResumePlayback();
}
#endif

// FUNCTION: XWA 0x527060
int Config_CutsceneViewerScreen(void) {
	char menuNavKey;
	int selectedPlayRow;
	int cutsceneDisplayRow;
	int outY;
	int outX;
	int dy;
	int y;
	int imageX;
	int cutsceneIndex;
	int requiredDisk;
	int backTextWidth;
	register int zero;
	FrontendRect out;
	FrontendRect rect;
	FrontendRect savedClipRect;

#ifdef XWA_MODERN
	if (g_configCutscenePlaybackRow != 0) {
		if (Config_ContinueCutscenePlayback()) {
			return 0;
		}
		Config_FinishCutscenePlayback();
	}
#endif

	FrontendCursor_GetPos(&outX, &outY);
	menuNavKey = (char)Config_GetMenuNavKey();

	if (!g_configAuxScreenFrameCount) {
		g_cutsceneViewerScrollRow = 0;
		g_cutsceneViewerSelectedRow = 0;
		g_cutsceneViewerPlayableRowCount = 1;

		for (cutsceneIndex = 0; (unsigned int)cutsceneIndex < (unsigned int)g_cutsceneCount;
			 ++cutsceneIndex) {
			if (g_pilotData.tourOfDutyMissions[g_cutsceneTable[cutsceneIndex].missionNumber].completedCount) {
				++g_cutsceneViewerPlayableRowCount;
			}
		}

		g_configAuxScreenFrameCount = 1;
	}

	if (menuNavKey == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		if ((unsigned int)g_cutsceneViewerSelectedRow > 0) {
			--g_cutsceneViewerSelectedRow;
		}
	} else if (menuNavKey == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		if ((unsigned int)g_cutsceneViewerSelectedRow <
			(unsigned int)(g_cutsceneViewerPlayableRowCount - 1)) {
			++g_cutsceneViewerSelectedRow;
		}
	}

	FrontendDraw_RectAssign(&rect, 31, 50, 50, 430);
	if ((unsigned int)g_cutsceneViewerPlayableRowCount > 5) {
		g_cutsceneViewerScrollRow = FrontendScrollbar_Draw(
			&rect, g_cutsceneViewerScrollRow, g_cutsceneViewerPlayableRowCount, 0, 5, g_colorNavy, 11);
	}

	FrontendDisplay_GetScreenClipRect(&savedClipRect);
	FrontendDraw_RectAssign(&rect, 40, 50, 600, 430);
	FrontendDisplay_SetScreenClipRect640x480(&rect);

	selectedPlayRow = 0;

	FrontImage_GetResourceRect("cutintro", &out);
	y = 50 - g_cutsceneViewerScrollRow * (out.bottom - out.top + 21);
	FrontendDraw_RectAssign(&rect, 65, y, 575, y + 14);
	FrontendText_DrawCentered(12, FrontendString_Get(STR_CONFIG_OPENING_CUTSCENE), &rect, 0xffff);

	y += 15;
	FrontendDraw_RectOffsetXY(&rect, 0, 15);
	FrontImage_GetResourceRect("cutintro", &out);
	imageX = out.right - out.left;
	++imageX;
	dy = out.bottom - out.top;
	imageX = 320 - (int)((unsigned int)imageX >> 1);
	dy += 6;
	FrontImage_DrawSpriteOpaque("cutintro", imageX, y + 3);
	FrontendDraw_RectOffsetXY(&out, imageX, y + 3);

	if (FrontendDraw_PointInRect(&out, outX, outY)) {
		FrontendDraw_RectOutline(&out, 0, 0, g_colorYellow);
		if (FrontendButton_DrawSimpleSpriteHitTest(&out, g_emptyString, g_emptyString, NULL, 15, 0xffff, 29,
												   "jewelsound")) {
			selectedPlayRow = 1;
		}
	}

	if (g_cutsceneViewerSelectedRow == 0) {
		FrontendDraw_RectOutline(&out, 0, 0, g_colorGreen);
		if (menuNavKey == CONFIG_KEY_ENTER) {
			selectedPlayRow = 1;
		}
	}

	y += dy;
	FrontendDraw_RectOffsetXY(&rect, 0, dy);

	if ((unsigned int)g_cutsceneCount > 0) {
		cutsceneDisplayRow = 2;
		imageX = y + 3;
		cutsceneIndex = 0;
		do {
			if (g_pilotData.tourOfDutyMissions[g_cutsceneTable[cutsceneIndex].missionNumber].completedCount) {
				FrontendText_DrawCentered(12, g_cutsceneTable[cutsceneIndex].description, &rect, 0xffff);

				y += 15;
				imageX += 15;
				FrontendDraw_RectOffsetXY(&rect, 0, 15);

				FrontImage_GetResourceRect(g_cutsceneTable[cutsceneIndex].thumbnailSprite, &out);
				requiredDisk = out.right - out.left;
				++requiredDisk;
				requiredDisk = 320 - (int)((unsigned int)requiredDisk >> 1);
				FrontImage_DrawSpriteOpaque(g_cutsceneTable[cutsceneIndex].thumbnailSprite, requiredDisk,
											imageX);
				FrontendDraw_RectOffsetXY(&out, requiredDisk, imageX);

				if (FrontendDraw_PointInRect(&out, outX, outY)) {
					FrontendDraw_RectOutline(&out, 0, 0, g_colorGreen);
					if (FrontendButton_DrawSimpleSpriteHitTest(&out, g_emptyString, g_emptyString, NULL, 15,
															   0xffff, cutsceneDisplayRow + 30,
															   "jewelsound")) {
						selectedPlayRow = cutsceneDisplayRow;
					}
				}

				if (g_cutsceneViewerSelectedRow == cutsceneDisplayRow - 1) {
					FrontendDraw_RectOutline(&out, 0, 0, g_colorGreen);
					if (menuNavKey == CONFIG_KEY_ENTER) {
						selectedPlayRow = cutsceneDisplayRow;
					}
				}

				y += dy;
				imageX += dy;
				FrontendDraw_RectOffsetXY(&rect, 0, dy);
				if (y > 480) {
					break;
				}
			}
			++cutsceneIndex;
			++cutsceneDisplayRow;
		} while ((unsigned int)(cutsceneDisplayRow - 2) < (unsigned int)g_cutsceneCount);
	}

	FrontendDisplay_SetScreenClipRect640x480(&savedClipRect);

	zero = 0;
	if (selectedPlayRow != zero) {
		FrontendDisplay_ResetScreenClipRect();
		CDAudio_SuspendPlayback();
		Keyboard_FlushCharBuffer();
		FrontendMouse_ClearClicks();

		if (selectedPlayRow == 1) {
#ifdef XWA_MODERN
			g_configCutscenePlaybackRow = selectedPlayRow;
			g_configCutscenePlaybackIndex = 0;
			g_movieSkipRequested = 0;
#else
			Movie_Play("logofinal", zero);
			if (g_movieSkipRequested == zero) {
				Movie_Play("tgintro", zero);
				if (g_movieSkipRequested == zero) {
					Movie_Play("intro_final", zero);
				}
			}
#endif
		} else {
			requiredDisk =
				(unsigned int)g_cutsceneTable[selectedPlayRow - 2].missionNumber >= (unsigned int)g_diskId;
			if (requiredDisk != g_currentCdDisk) {
				do {
					cutsceneIndex = zero;
					if (File_CheckGameCdPresent(requiredDisk)) {
						g_currentCdDisk = requiredDisk;
						cutsceneIndex = 1;
					}
					if (cutsceneIndex != zero) {
						break;
					}

					if (requiredDisk) {
						backTextWidth = FrontendDialog_ShowConfirmDialog(
							FrontendString_Get(STR_FAILED_TO_DETECT2_1),
							FrontendString_Get(STR_FAILED_TO_DETECT2_2), NULL, FrontendString_Get(STR_OKAY),
							FrontendString_Get(STR_CANCEL));
					} else {
						backTextWidth = FrontendDialog_ShowConfirmDialog(
							FrontendString_Get(STR_FAILED_TO_DETECT1_1),
							FrontendString_Get(STR_FAILED_TO_DETECT1_2), NULL, FrontendString_Get(STR_OKAY),
							FrontendString_Get(STR_CANCEL));
					}
					if (!backTextWidth) {
						Keyboard_FlushCharBuffer();
						g_pendingMenuScreen = zero;
						g_menuCursorRow = 7;
						return zero;
					}
				} while (1);
			}
#ifdef XWA_MODERN
			g_configCutscenePlaybackRow = selectedPlayRow;
			g_configCutscenePlaybackIndex = 0;
#else
			Movie_Play(g_cutsceneTable[selectedPlayRow - 2].movieName, zero);
#endif
		}

#ifdef XWA_MODERN
		if (Config_ContinueCutscenePlayback()) {
			return zero;
		}
		Config_FinishCutscenePlayback();
#else
		FrontendDisplay_ResetScreenClipRect();
		Config_DrawBackgroundToScreens();
		CDAudio_RequestResumePlayback();
#endif
	}

	backTextWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_BACK), 20);
	requiredDisk = FrontendButton_DrawMenuButton(595 - backTextWidth, 400, FrontendString_Get(STR_BACK), 20,
												 g_colorPaleBlue, 20, zero, "settingsound");
	if (menuNavKey == CONFIG_KEY_ESCAPE || requiredDisk) {
		Keyboard_FlushCharBuffer();
		g_pendingMenuScreen = zero;
		g_menuCursorRow = 7;
	}

	return zero;
}

#ifdef XWA_MODERN
static void Config_FinishOptionsDatapadOpen(void) {
	FrontendCursor_SetImageResourceForCurrentTheme("cursor", g_cursorBitmap);
	FrontendCursor_SetPos(32, 127);
	FrontendText_PushGlyphGradientBg(g_colorNearBlack);
	FrontendScrollbar_SaveState();
	Frontend_ResetScrollableControls();
	g_configMenuCenterX = 320;
	g_menuCursorRow = 0;
	g_configMenuScrollIndex = 0;
	g_pendingMenuScreen = 0;
	g_configExitConfirmPending = 0;
	g_frontendSetupNeedsBaseRedraw = 1;
	Keyboard_FlushCharBuffer();
	Config_LoadJoystickActionDictionary();
	FrontImage_RegisterResourceDefault("frontres\\config\\configb.bmp", "backconfig");
	Config_DrawBackgroundToScreens();
	FrontendText_ResetGlyphScratchBuffer(20);
}

static void Config_FinishOptionsDatapadClose(void) {
	g_activeTextFieldId = 0;
	FrontendDraw_ForceFullScreenPresent();
	Keyboard_FlushCharBuffer();
	FrontendScreen_PopState();
	FrontendText_PopGlyphGradientBg();
	if (!g_configDatapadQuitConfirmed) {
		FrontendWaveStream_Resume();
		Music_ResumeIfInitialized();
	}
	if (!g_configRestrictedOptionsModalActive) {
		if (g_gameConfig.datapadMusicEnabled) {
			Music_SetState(musicState);
			Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
		} else {
			Music_Stop();
		}
	}
	FrontendMouse_ClearInputGate();
	FrontImage_FreeResourceByName("backconfig");
	FrontendText_ResetGlyphScratch();
	FrontendScrollbar_RestoreState();
	g_configOptionsDatapadInitialized = 0;
	g_configExitConfirmPending = 0;
	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_HOST) {
		MissionSetup_BroadcastGameConfigPacket();
	}
}
#endif

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x51D100
int Config_OptionsDatapadUpdate(int frameState) {
	int done;
	int networkDismiss;
	FrontendRect clipRect;

	(void)frameState;
#ifdef XWA_MODERN
	if (g_configOptionsDatapadOpeningMovie) {
		g_configOptionsDatapadOpeningMovie = 0;
		Config_FinishOptionsDatapadOpen();
	}
	if (g_configOptionsDatapadClosingMovie) {
		g_configOptionsDatapadClosingMovie = 0;
		Config_FinishOptionsDatapadClose();
		return 1;
	}
#endif
	if (!g_configOptionsDatapadInitialized) {
		g_configOptionsDatapadInitialized = 1;
		Music_PauseIfInitialized();
		FrontendWaveStream_Pause();
#ifdef XWA_MODERN
		if (Movie_Play("datapad", 1)) {
			g_configOptionsDatapadOpeningMovie = 1;
			return 0;
		}
		Config_FinishOptionsDatapadOpen();
#else
		Movie_Play("datapad", 1);
		FrontendCursor_SetImageResourceForCurrentTheme("cursor", g_cursorBitmap);
		FrontendCursor_SetPos(32, 127);
		FrontendText_PushGlyphGradientBg(g_colorNearBlack);
		FrontendScrollbar_SaveState();
		Frontend_ResetScrollableControls();
		g_configMenuCenterX = 320;
		g_menuCursorRow = 0;
		g_configMenuScrollIndex = 0;
		g_pendingMenuScreen = 0;
		g_frontendSetupNeedsBaseRedraw = 1;
		Keyboard_FlushCharBuffer();
		Config_LoadJoystickActionDictionary();
		FrontImage_RegisterResourceDefault("frontres\\config\\configb.bmp", "backconfig");
		Config_DrawBackgroundToScreens();
		FrontendText_ResetGlyphScratchBuffer(20);
#endif
	}

	FrontendDraw_RectAssign(&clipRect, 31, 26, 605, 435);
	FrontendDisplay_SetScreenClipRect640x480(&clipRect);
	switch (g_pendingMenuScreen) {
		case 0:
			done = Config_MainMenuScreen();
			break;
		case 1:
			done = Config_GeneralOptionsScreen();
			break;
		case 2:
			done = Config_VideoOptionsMenu();
			break;
		case 3:
			done = Config_SoundOptionsScreen();
			break;
		case 4:
			done = Config_ControllerOptionsScreen();
			break;
		case 7:
			done = Config_SinglePlayerVideoOptionsScreen();
			break;
		case 8:
			done = Config_MultiplayerVideoOptionsScreen();
			break;
		case 9:
			done = Config_SinglePlayerHardwareVideoScreen();
			break;
		case 10:
			done = Config_SinglePlayerSoftwareVideoScreen();
			break;
		case 11:
			done = Config_MultiplayerHardwareVideoScreen();
			break;
		case 12:
			done = Config_MultiplayerSoftwareVideoScreen();
			break;
		case 13:
			done = Config_JoystickRemapScreen();
			break;
		case 14:
			done = Config_CreditsScreen();
			break;
		case 15:
			done = Config_CutsceneViewerScreen();
			break;
		case 16:
			done = Config_PerformanceOptionsScreen();
			break;
		case 17:
			done = Config_NetworkOptionsScreen();
			break;
#ifdef XWA_MODERN
		case 18:
			if (XwaModernVideoOptionsScreen_Update(g_configMenuCenterX, &g_menuCursorRow)) {
				g_pendingMenuScreen = 2;
				g_menuCursorRow = 0;
			}
			done = 0;
			break;
		case 19:
			if (XwaModernInputOptionsScreen_Update(g_configMenuCenterX, &g_menuCursorRow)) {
				g_pendingMenuScreen = 4;
				g_menuCursorRow = 6;
			}
			done = 0;
			break;
#endif
		default:
			done = 0;
			break;
	}

	networkDismiss = FrontendDialog_HasNetworkDismissPacket();
	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_HOST) {
		networkDismiss |= Net_HasQueuedJoinRequestOrBacklog();
	}
	if (networkDismiss) {
		done = 1;
	}

	FrontendDisplay_ResetScreenClipRect();
	if (done == 1) {
		Config_Write();
		FrontendMouse_ClearClicks();
#ifdef XWA_MODERN
		XwaModernVideoOptions_Flush();
		XwaModernInputOptions_Flush();
		if (Movie_Play("dpclose", 1)) {
			g_configOptionsDatapadClosingMovie = 1;
			return 0;
		}
		Config_FinishOptionsDatapadClose();
#else
		Movie_Play("dpclose", 1);
		g_activeTextFieldId = 0;
		FrontendDraw_ForceFullScreenPresent();
		Keyboard_FlushCharBuffer();
		FrontendScreen_PopState();
		FrontendText_PopGlyphGradientBg();
		if (!g_configDatapadQuitConfirmed) {
			FrontendWaveStream_Resume();
			Music_ResumeIfInitialized();
		}
		if (!g_configRestrictedOptionsModalActive) {
			if (g_gameConfig.datapadMusicEnabled) {
				Music_SetState(musicState);
				Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
			} else {
				Music_Stop();
			}
		}
		FrontendMouse_ClearInputGate();
		FrontImage_FreeResourceByName("backconfig");
		FrontendText_ResetGlyphScratch();
		FrontendScrollbar_RestoreState();
		g_configOptionsDatapadInitialized = 0;
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_HOST) {
			MissionSetup_BroadcastGameConfigPacket();
		}
#endif
	}

	return done;
}

// FUNCTION: XWA 0x526CB0
int Config_DrawBackgroundToScreens(void) {
	FrontImage_DrawSpriteOpaque("backconfig", 0, 0);
	FrontendDisplay_LockOffscreenSurface();
	FrontImage_DrawSpriteOpaque("backconfig", 0, 0);
	FrontendDisplay_UnlockOffscreenSurface(1);
	return 1;
}

// FUNCTION: XWA 0x528150
int Config_RunRestrictedOptionsModal(void) {
	FrontendRect rc;

#ifdef XWA_MODERN
	FrontendScreenModalStatus modalStatus;

	if (g_configRestrictedOptionsModalRunActive) {
		modalStatus = FrontendScreen_GetModalStatus();
		if (modalStatus != FRONTEND_SCREEN_MODAL_INACTIVE && modalStatus != FRONTEND_SCREEN_MODAL_DONE) {
			return 0;
		}

		FrontendCursor_Hide();
		FrontendDisplay_UnlockBackBuffer();
		FrontendDisplay_ClearBackBuffer();
		FrontendDisplay_ClearOffscreenSurface();
		FrontendDisplay_PresentFrame();
		FrontendDisplay_ClearBackBuffer();
		FrontendCursor_FreeResources();
		g_configRestrictedOptionsModalActive = 0;
		FrontendSound_UnloadList("sfx\\sfx.lst");
		g_configRestrictedOptionsModalRunActive = 0;
		g_configOptionsDatapadInitialized = 0;
		return g_configDatapadQuitConfirmed;
	}
#endif

	Keyboard_FlushCharBuffer();
	FrontendCursor_LoadResources();
	FrontImage_RebuildPaletteCache();
	FrontendColor_Init();
	FrontendText_SetGlyphGradientBg(g_colorNearBlack);
	FrontendSound_LoadList("sfx\\sfx.lst");
	g_configRestrictedOptionsModalActive = 1;
	FrontendCursor_Show();
	FrontendDraw_RectAssign(&rc, 0, 0, 639, 479);
	g_configDatapadQuitConfirmed = 0;
	g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
#ifdef XWA_MODERN
	if (!FrontendScreen_BeginModal(Config_OptionsDatapadUpdate, &rc)) {
		FrontendCursor_Hide();
		FrontendDisplay_UnlockBackBuffer();
		FrontendCursor_FreeResources();
		g_configRestrictedOptionsModalActive = 0;
		FrontendSound_UnloadList("sfx\\sfx.lst");
		g_configOptionsDatapadInitialized = 0;
		return 0;
	}

	g_configRestrictedOptionsModalRunActive = 1;
	return 0;
#else
	FrontendScreen_RunModal(Config_OptionsDatapadUpdate, &rc);
	FrontendCursor_Hide();
	FrontendDisplay_UnlockBackBuffer();
	FrontendDisplay_ClearBackBuffer();
	FrontendDisplay_ClearOffscreenSurface();
	FrontendDisplay_PresentFrame();
	FrontendDisplay_ClearBackBuffer();
	FrontendCursor_FreeResources();
	g_configRestrictedOptionsModalActive = 0;
	FrontendSound_UnloadList("sfx\\sfx.lst");
	return g_configDatapadQuitConfirmed;
#endif
}

// FUNCTION: XWA 0x528990
int Config_GetMenuNavKey(void) {
	uint32_t tickCount;
	int keyState;
	int32_t elapsed;

	tickCount = GetTickCount();
	if (Keyboard_IsKeyDown(CONFIG_KEY_VK_UP)) {
		elapsed = (int32_t)(tickCount - g_menuKeyRepeatTime);
		if (elapsed > CONFIG_MENU_KEY_REPEAT_MS) {
			g_menuKeyRepeatTime = tickCount;
			return CONFIG_KEY_VK_UP;
		}

		/* The original falls through with the elapsed millisecond value here.
		   Aeron frame timing can make that value collide with arrow VK codes. */
#ifdef XWA_MODERN
		return 0;
#else
		return elapsed;
#endif
	}

	keyState = Keyboard_IsKeyDown(CONFIG_KEY_VK_DOWN);
	if (keyState) {
		if ((int32_t)(tickCount - g_menuKeyRepeatTime) > CONFIG_MENU_KEY_REPEAT_MS) {
			g_menuKeyRepeatTime = tickCount;
			return CONFIG_KEY_VK_DOWN;
		}

		/* The original returns the nonzero key-down result until repeat fires. */
#ifdef XWA_MODERN
		return 0;
#else
		return keyState;
#endif
	}

	keyState = Keyboard_IsKeyDown(CONFIG_KEY_VK_LEFT);
	if (keyState) {
		if ((int32_t)(tickCount - g_menuKeyRepeatTime) > CONFIG_MENU_KEY_REPEAT_MS) {
			g_menuKeyRepeatTime = tickCount;
			return CONFIG_KEY_VK_LEFT;
		}

		/* The original returns the nonzero key-down result until repeat fires. */
#ifdef XWA_MODERN
		return 0;
#else
		return keyState;
#endif
	}

	if (Keyboard_IsKeyDown(CONFIG_KEY_VK_RIGHT)) {
		elapsed = (int32_t)(tickCount - g_menuKeyRepeatTime);
		if (elapsed > CONFIG_MENU_KEY_REPEAT_MS) {
			g_menuKeyRepeatTime = tickCount;
			return CONFIG_KEY_VK_RIGHT;
		}

		/* The original falls through with the elapsed millisecond value here.
		   Aeron frame timing can make that value collide with arrow VK codes. */
#ifdef XWA_MODERN
		return 0;
#else
		return elapsed;
#endif
	}

	return (unsigned char)Keyboard_DequeueChar();
}

// FUNCTION: XWA 0x523270
int Config_DrawOptionCycle(uint8_t* value, UIString labelId, int valueBaseStrId, int optionCount, int* y,
						   int* rowIndex, char* keyState, int buttonId) {
	return Config_DrawOptionCycleImpl(value, labelId, valueBaseStrId, optionCount, y, rowIndex, keyState,
									  buttonId, 0);
}

// FUNCTION: XWA 0x5232B0
int Config_DrawOptionCycleDisabled(uint8_t* value, UIString labelId, int valueBaseStrId, int optionCount,
								   int* y, int* rowIndex, char* keyState, int buttonId) {
	return Config_DrawOptionCycleImpl(value, labelId, valueBaseStrId, optionCount, y, rowIndex, keyState,
									  buttonId, 1);
}

// FUNCTION: XWA 0x521860
int Config_SinglePlayerHardwareVideoScreen(void) {
	char keyState;
	const char* text;
	const char* line1;
	const char* line2;
	const char* line3;
	const char* okayLabel;
	const char* cancelLabel;
	int y;
	int rowIndex;
	int action;
	int textX;
	int buttonPressed;
	unsigned int titleWidth;
	FrontendRect rect;

	rowIndex = 0;
	action = 0;
	y = 100;
	keyState = (char)Config_GetMenuNavKey();
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
			g_menuCursorRow = 11;
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (++g_menuCursorRow >= 12) {
			g_menuCursorRow = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	text = FrontendString_Get(STR_CONFIG_SP_HARDWARE_VIDEO_OPTIONS);
	FrontendText_DrawCentered(15, text, &rect, g_colorLightBlue);
	text = FrontendString_Get(STR_CONFIG_SP_HARDWARE_VIDEO_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	FrontendDraw_Line(g_configMenuCenterX - (int)(titleWidth >> 1), y + 17,
					  g_configMenuCenterX - (int)(titleWidth >> 1) + (int)titleWidth, y + 17,
					  g_colorLightBlue);
	y += 20;

	Config_DrawHardwareVideoAdvancedRows(0, &y, &rowIndex, &keyState);

	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_SAME_AS_MULTIPLAYER);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, 0xffff);
		} else {
			FrontendText_Draw(15, text, textX, y, g_colorGray);
		}
	} else {
		text = FrontendString_Get(STR_CONFIG_SAME_AS_MULTIPLAYER);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 21, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
		if (buttonPressed) {
			action = 1;
		}
	}
	y += 20;
	++rowIndex;

	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		FrontendText_Draw(15, text, textX, y, g_colorGray);
	} else {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 22, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
		if (buttonPressed) {
			action = 2;
		}
	}
	y += 20;
	++rowIndex;

	text = FrontendString_Get(STR_BACK);
	textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 23, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		keyState = 0;
		buttonPressed = 1;
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 7;
		g_menuCursorRow = 14;
	}

	if (action == 1) {
		cancelLabel = FrontendString_Get(STR_CANCEL);
		okayLabel = FrontendString_Get(STR_OKAY);
		line3 = FrontendString_Get(STR_CONFIG_OVERWRITE3);
		line2 = FrontendString_Get(STR_CONFIG_OVERWRITE2);
		line1 = FrontendString_Get(STR_CONFIG_OVERWRITE1);
		if (FrontendDialog_ShowConfirmDialog(line1, line2, line3, okayLabel, cancelLabel)) {
			g_gameConfig.engineGlow[0] = g_gameConfig.engineGlow[1];
			g_gameConfig.hitEffects[0] = g_gameConfig.hitEffects[1];
			g_gameConfig.lensFlare[0] = g_gameConfig.lensFlare[1];
			g_gameConfig.hardwareMipmap[0] = g_gameConfig.hardwareMipmap[1];
			g_gameConfig.hudColor[0] = g_gameConfig.hudColor[1];
			g_gameConfig.palettizedTextures[0] = g_gameConfig.palettizedTextures[1];
			g_gameConfig.trails[0] = g_gameConfig.trails[1];
			g_gameConfig.bilinear[0] = g_gameConfig.bilinear[1];
			g_gameConfig.particleEffects[0] = g_gameConfig.particleEffects[1];
			return 0;
		}
	} else if (action == 2) {
		cancelLabel = FrontendString_Get(STR_CANCEL);
		okayLabel = FrontendString_Get(STR_OKAY);
		line3 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL3);
		line2 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL2);
		line1 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL1);
		if (FrontendDialog_ShowConfirmDialog(line1, line2, line3, okayLabel, cancelLabel)) {
			g_gameConfig.hitEffects[0] = 1;
			g_gameConfig.engineGlow[0] = 1;
			g_gameConfig.lensFlare[0] = 1;
			g_gameConfig.hudColor[0] = 0;
			g_gameConfig.hardwareMipmap[0] = 0;
			g_gameConfig.palettizedTextures[0] = 1;
			g_gameConfig.bilinear[0] = 1;
			g_gameConfig.trails[0] = 1;
			g_gameConfig.particleEffects[0] = 1;
			if (g_gameConfig.performance == 0) {
				Config_SetDetailDefaultsLow(2);
			} else if (g_gameConfig.performance == 1) {
				Config_SetDetailDefaultsMedium(2);
				return 0;
			} else if (g_gameConfig.performance == 2) {
				Config_SetDetailDefaultsHigh(2);
				return 0;
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x5223F0
int Config_MultiplayerHardwareVideoScreen(void) {
	char keyState;
	const char* text;
	const char* line1;
	const char* line2;
	const char* line3;
	const char* okayLabel;
	const char* cancelLabel;
	int y;
	int rowIndex;
	int action;
	int textX;
	int buttonPressed;
	unsigned int titleWidth;
	FrontendRect rect;

	rowIndex = 0;
	action = 0;
	y = 100;
	keyState = (char)Config_GetMenuNavKey();
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
			g_menuCursorRow = 11;
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (++g_menuCursorRow >= 12) {
			g_menuCursorRow = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	text = FrontendString_Get(STR_CONFIG_MP_HARDWARE_VIDEO_OPTIONS);
	FrontendText_DrawCentered(15, text, &rect, g_colorLightBlue);
	text = FrontendString_Get(STR_CONFIG_MP_HARDWARE_VIDEO_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	FrontendDraw_Line(g_configMenuCenterX - (int)(titleWidth >> 1), y + 17,
					  g_configMenuCenterX - (int)(titleWidth >> 1) + (int)titleWidth, y + 17,
					  g_colorLightBlue);
	y += 20;

	Config_DrawHardwareVideoAdvancedRows(1, &y, &rowIndex, &keyState);

	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_SAME_AS_SINGLE_PLAYER);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		FrontendText_Draw(15, text, textX, y, g_colorGray);
	} else {
		text = FrontendString_Get(STR_CONFIG_SAME_AS_SINGLE_PLAYER);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 21, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
		if (buttonPressed) {
			action = 1;
		}
	}
	y += 20;
	++rowIndex;

	if (g_configRestrictedOptionsModalActive) {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		FrontendText_Draw(15, text, textX, y, g_colorGray);
	} else {
		text = FrontendString_Get(STR_CONFIG_RESTORE_DEFAULTS);
		textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
		buttonPressed =
			FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 22, 0, "settingsound");
		if (g_menuCursorRow == rowIndex) {
			FrontendText_Draw(15, text, textX, y, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
		if (buttonPressed) {
			action = 2;
		}
	}
	y += 20;
	++rowIndex;

	text = FrontendString_Get(STR_BACK);
	textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 23, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		keyState = 0;
		buttonPressed = 1;
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 8;
		g_menuCursorRow = 13;
	}

	if (action == 1) {
		cancelLabel = FrontendString_Get(STR_CANCEL);
		okayLabel = FrontendString_Get(STR_OKAY);
		line3 = FrontendString_Get(STR_CONFIG_OVERWRITE3);
		line2 = FrontendString_Get(STR_CONFIG_OVERWRITE2);
		line1 = FrontendString_Get(STR_CONFIG_OVERWRITE1);
		if (FrontendDialog_ShowConfirmDialog(line1, line2, line3, okayLabel, cancelLabel)) {
			g_gameConfig.hitEffects[1] = g_gameConfig.hitEffects[0];
			g_gameConfig.engineGlow[1] = g_gameConfig.engineGlow[0];
			g_gameConfig.lensFlare[1] = g_gameConfig.lensFlare[0];
			g_gameConfig.hudColor[1] = g_gameConfig.hudColor[0];
			g_gameConfig.hardwareMipmap[1] = g_gameConfig.hardwareMipmap[0];
			g_gameConfig.palettizedTextures[1] = g_gameConfig.palettizedTextures[0];
			g_gameConfig.bilinear[1] = g_gameConfig.bilinear[0];
			g_gameConfig.trails[1] = g_gameConfig.trails[0];
			g_gameConfig.particleEffects[1] = g_gameConfig.particleEffects[0];
			return 0;
		}
	} else if (action == 2) {
		cancelLabel = FrontendString_Get(STR_CANCEL);
		okayLabel = FrontendString_Get(STR_OKAY);
		line3 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL3);
		line2 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL2);
		line1 = FrontendString_Get(STR_RESTORING_DEFAULTS_WILL1);
		if (FrontendDialog_ShowConfirmDialog(line1, line2, line3, okayLabel, cancelLabel)) {
			g_gameConfig.hitEffects[1] = 1;
			g_gameConfig.engineGlow[1] = 1;
			g_gameConfig.lensFlare[1] = 1;
			g_gameConfig.hudColor[1] = 0;
			g_gameConfig.hardwareMipmap[1] = 0;
			g_gameConfig.palettizedTextures[1] = 1;
			g_gameConfig.bilinear[1] = 1;
			g_gameConfig.trails[1] = 1;
			g_gameConfig.particleEffects[1] = 1;
			if (g_gameConfig.performance == 0) {
				Config_SetDetailDefaultsLow(16);
			} else if (g_gameConfig.performance == 1) {
				Config_SetDetailDefaultsMedium(16);
				return 0;
			} else if (g_gameConfig.performance == 2) {
				Config_SetDetailDefaultsHigh(16);
				return 0;
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x5276E0
int Config_PerformanceOptionsScreen(void) {
	char keyState;
	const char* text;
	int mouseX;
	int mouseY;
	int action;
	int textX;
	int textWidth;
	int buttonPressed;
	unsigned int rowColor;
	unsigned int titleWidth;
	FrontendRect rect;

	keyState = (char)Config_GetMenuNavKey();
	FrontendCursor_GetPos(&mouseX, &mouseY);
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
			g_menuCursorRow = 3;
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (++g_menuCursorRow >= 4) {
			g_menuCursorRow = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, 45, 639, 60);
	FrontendText_DrawCentered(15, FrontendString_Get(STR_CONFIG_PERFORMANCE_OPTIONS), &rect,
							  g_colorLightBlue);
	titleWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_CONFIG_PERFORMANCE_OPTIONS), 15);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	FrontendDraw_Line(textX, 62, textX + (int)titleWidth, 62, g_colorLightBlue);

	action = 0;

	text = FrontendString_Get(STR_CONFIG_PERFORMANCE_LOW);
	textWidth = FrontendText_MeasureWidth(text, 15);
	textX = g_configMenuCenterX - textWidth - 200;
	buttonPressed = 0;
	FrontendDraw_RectAssign(&rect, g_configMenuCenterX - 180, 75, 600, 175);
	if (g_configRestrictedOptionsModalActive) {
		rowColor = (unsigned int)g_colorGray;
		FrontendText_Draw(15, text, textX, 75, g_colorGray);
	} else {
		rowColor = (unsigned int)g_colorPaleBlue;
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			rowColor = (unsigned int)g_colorYellow;
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				rowColor = (unsigned int)g_colorRed;
				buttonPressed = 1;
			}
		}
		buttonPressed |= FrontendButton_DrawMenuButton(textX, 75, text, 15, rowColor, 20, 0, "settingsound");
	}
	if (g_gameConfig.performance == 0) {
		FrontendDraw_Line(textX + 6, 73, textX + textWidth - 6, 73, g_colorGreen);
		FrontendDraw_Line(textX + 3, 75, textX + textWidth - 3, 75, g_colorGreen);
		FrontendDraw_Line(textX + 3, 93, textX + textWidth - 3, 93, g_colorGreen);
		FrontendDraw_Line(textX + 6, 95, textX + textWidth - 6, 95, g_colorGreen);
	}
	if (g_menuCursorRow == 0) {
		if (g_configRestrictedOptionsModalActive) {
			FrontendText_Draw(15, text, textX, 75, 0xffff);
		} else {
			FrontendText_Draw(15, text, textX, 75, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
	}
	FrontendText_DrawWrappedClipped(15, FrontendString_Get(STR_CONFIG_PERFORMANCE_LOW_DESC), &rect,
									(int)rowColor, 5, 0);
	if (buttonPressed) {
		action = 1;
	}

	text = FrontendString_Get(STR_CONFIG_PERFORMANCE_MEDIUM);
	textWidth = FrontendText_MeasureWidth(text, 15);
	textX = g_configMenuCenterX - textWidth - 200;
	buttonPressed = 0;
	FrontendDraw_RectAssign(&rect, g_configMenuCenterX - 180, 185, 600, 285);
	if (g_configRestrictedOptionsModalActive) {
		rowColor = (unsigned int)g_colorGray;
		FrontendText_Draw(15, text, textX, 185, g_colorGray);
	} else {
		rowColor = (unsigned int)g_colorPaleBlue;
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			rowColor = (unsigned int)g_colorYellow;
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				rowColor = (unsigned int)g_colorRed;
				buttonPressed = 1;
			}
		}
		buttonPressed |= FrontendButton_DrawMenuButton(textX, 185, text, 15, rowColor, 21, 0, "settingsound");
	}
	if (g_gameConfig.performance == 1) {
		FrontendDraw_Line(textX + 6, 183, textX + textWidth - 6, 183, g_colorGreen);
		FrontendDraw_Line(textX + 3, 185, textX + textWidth - 3, 185, g_colorGreen);
		FrontendDraw_Line(textX + 3, 203, textX + textWidth - 3, 203, g_colorGreen);
		FrontendDraw_Line(textX + 6, 205, textX + textWidth - 6, 205, g_colorGreen);
	}
	if (g_menuCursorRow == 1) {
		if (g_configRestrictedOptionsModalActive) {
			FrontendText_Draw(15, text, textX, 185, 0xffff);
		} else {
			FrontendText_Draw(15, text, textX, 185, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
	}
	FrontendText_DrawWrappedClipped(15, FrontendString_Get(STR_CONFIG_PERFORMANCE_MEDIUM_DESC), &rect,
									(int)rowColor, 5, 0);
	if (buttonPressed) {
		action = 2;
	}

	text = FrontendString_Get(STR_CONFIG_PERFORMANCE_HIGH);
	textWidth = FrontendText_MeasureWidth(text, 15);
	textX = g_configMenuCenterX - textWidth - 200;
	buttonPressed = 0;
	FrontendDraw_RectAssign(&rect, g_configMenuCenterX - 180, 295, 600, 395);
	if (g_configRestrictedOptionsModalActive) {
		rowColor = (unsigned int)g_colorGray;
		FrontendText_Draw(15, text, textX, 295, g_colorGray);
	} else {
		rowColor = (unsigned int)g_colorPaleBlue;
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			rowColor = (unsigned int)g_colorYellow;
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				rowColor = (unsigned int)g_colorRed;
				buttonPressed = 1;
			}
		}
		buttonPressed |= FrontendButton_DrawMenuButton(textX, 295, text, 15, rowColor, 22, 0, "settingsound");
	}
	if (g_menuCursorRow == 2) {
		if (g_configRestrictedOptionsModalActive) {
			FrontendText_Draw(15, text, textX, 295, 0xffff);
		} else {
			FrontendText_Draw(15, text, textX, 295, g_colorGreen);
			if (keyState == CONFIG_KEY_ENTER) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				keyState = 0;
				buttonPressed |= 1;
			}
		}
	}
	if (g_gameConfig.performance == 2) {
		FrontendDraw_Line(textX + 6, 293, textX + textWidth - 6, 293, g_colorGreen);
		FrontendDraw_Line(textX + 3, 295, textX + textWidth - 3, 295, g_colorGreen);
		FrontendDraw_Line(textX + 3, 313, textX + textWidth - 3, 313, g_colorGreen);
		FrontendDraw_Line(textX + 6, 315, textX + textWidth - 6, 315, g_colorGreen);
	}
	FrontendText_DrawWrappedClipped(15, FrontendString_Get(STR_CONFIG_PERFORMANCE_HIGH_DESC), &rect,
									(int)rowColor, 5, 0);
	if (buttonPressed) {
		action = 3;
	}

	text = FrontendString_Get(STR_BACK);
	textX = g_configMenuCenterX - (int)((unsigned int)FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed =
		FrontendButton_DrawMenuButton(textX, 405, text, 15, g_colorPaleBlue, 29, 0, "settingsound");
	if (g_menuCursorRow == 3) {
		FrontendText_Draw(15, text, textX, 405, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		buttonPressed = 1;
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 0;
		g_menuCursorRow = 1;
	}

	if (action == 1) {
		if (FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_CONFIG_PERFORMANCE_CONFIRM1),
											 FrontendString_Get(STR_CONFIG_PERFORMANCE_CONFIRM2),
											 FrontendString_Get(STR_CONFIG_PERFORMANCE_CONFIRM3),
											 FrontendString_Get(STR_YES), FrontendString_Get(STR_NO))) {
			Config_SetDetailDefaultsLow(-1);
			return 0;
		}
	} else if (action == 2) {
		if (FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_CONFIG_PERFORMANCE_CONFIRM1),
											 FrontendString_Get(STR_CONFIG_PERFORMANCE_CONFIRM2),
											 FrontendString_Get(STR_CONFIG_PERFORMANCE_CONFIRM3),
											 FrontendString_Get(STR_YES), FrontendString_Get(STR_NO))) {
			Config_SetDetailDefaultsMedium(-1);
			return 0;
		}
	} else if (action == 3) {
		if (FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_CONFIG_PERFORMANCE_CONFIRM1),
											 FrontendString_Get(STR_CONFIG_PERFORMANCE_CONFIRM2),
											 FrontendString_Get(STR_CONFIG_PERFORMANCE_CONFIRM3),
											 FrontendString_Get(STR_YES), FrontendString_Get(STR_NO))) {
			Config_SetDetailDefaultsHigh(-1);
		}
	}

	return 0;
}

// FUNCTION: XWA 0x5285C0
int Config_NetworkOptionsScreen(void) {
	char keyState;
	uint8_t predictionRate;
	const char* text;
	int rowCount;
	int y;
	int rowIndex;
	int textX;
	int buttonPressed;
	unsigned int titleWidth;
	FrontendRect rect;

	rowIndex = 0;
	rowCount = (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_NET_CLIENT) + 4;
	y = 220 - ((20 * rowCount) >> 1);
	keyState = (char)Config_GetMenuNavKey();
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
			g_menuCursorRow = rowCount - 1;
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (++g_menuCursorRow >= rowCount) {
			g_menuCursorRow = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, y, 639, y + 15);
	text = FrontendString_Get(STR_CONFIG_NETWORK_OPTIONS);
	FrontendText_DrawCentered(15, text, &rect, g_colorLightBlue);
	text = FrontendString_Get(STR_CONFIG_NETWORK_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 15);
	FrontendDraw_Line(g_configMenuCenterX - (int)(titleWidth >> 1), y + 17,
					  g_configMenuCenterX - (int)(titleWidth >> 1) + (int)titleWidth, y + 17,
					  g_colorLightBlue);
	y += 20;

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_CLIENT) {
		Config_DrawOptionCycleDisabled(&g_gameConfig.requirePassword, STR_CONFIG_REQUIRE_PASSWORD,
									   STR_CONFIG_NO, 2, &y, &rowIndex, &keyState, 20);
		Config_DrawOptionCycleDisabled(&g_gameConfig.asyncFlag, STR_NETWORK_MODEL, STR_CONFIG_NO, 2, &y,
									   &rowIndex, &keyState, 22);
		predictionRate = (uint8_t)(((int)g_gameConfig.serverUpdateRate - 6) >> 1);
		if (predictionRate > 1) {
			predictionRate = 0;
		}
		Config_DrawOptionCycleDisabled(&predictionRate, STR_SERVER_PREDICTION_RATE, STR_MEDIUM_PREDICTION, 2,
									   &y, &rowIndex, &keyState, 23);
		g_gameConfig.serverUpdateRate = (uint8_t)(2 * (predictionRate + 3));
	} else {
		Config_DrawOptionCycle(&g_gameConfig.requirePassword, STR_CONFIG_REQUIRE_PASSWORD, STR_CONFIG_NO, 2,
							   &y, &rowIndex, &keyState, 20);
		text = FrontendString_Get(STR_PASSWORD);
		Config_DrawOptionTextEdit(g_gameConfig.password, 32, 1, (char*)text, &y, &rowIndex, &keyState, 21);
		Config_DrawOptionCycle(&g_gameConfig.asyncFlag, STR_NETWORK_MODEL, STR_CONFIG_NO, 2, &y, &rowIndex,
							   &keyState, 22);
		predictionRate = (uint8_t)(((int)g_gameConfig.serverUpdateRate - 6) >> 1);
		if (predictionRate > 1) {
			predictionRate = 0;
		}
		Config_DrawOptionCycle(&predictionRate, STR_SERVER_PREDICTION_RATE, STR_MEDIUM_PREDICTION, 2, &y,
							   &rowIndex, &keyState, 23);
		g_gameConfig.serverUpdateRate = (uint8_t)(2 * (predictionRate + 3));
	}

	text = FrontendString_Get(STR_BACK);
	textX = g_configMenuCenterX - (int)(FrontendText_MeasureWidth(text, 15) >> 1);
	buttonPressed = FrontendButton_DrawMenuButton(textX, y, text, 15, g_colorPaleBlue, 29, 0, "settingsound");
	if (g_menuCursorRow == rowIndex) {
		FrontendText_Draw(15, text, textX, y, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		keyState = 0;
		buttonPressed = 1;
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 0;
		g_menuCursorRow = 5;
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NET_HOST) {
			MissionSetup_BroadcastStatePacket(0);
		}
	}

	return 0;
}

// FUNCTION: XWA 0x522F60
int Config_DrawHardwareVideoAdvancedRows(int profileIdx, int* y, int* rowIndex, char* keyState) {
	int drawY;
	int hudColorTextWidth;
	unsigned int previewX;
	FrontendRect hudRect;

	drawY = *y;
	if (g_configRestrictedOptionsModalActive) {
		Config_DrawOptionCycleDisabled(&g_gameConfig.hardwareMipmap[profileIdx], STR_CONFIG_HARDWARE_MIPMAP,
									   STR_CONFIG_HMP_OFF, 3, &drawY, rowIndex, keyState, 24);
	} else {
		Config_DrawOptionCycle(&g_gameConfig.hardwareMipmap[profileIdx], STR_CONFIG_HARDWARE_MIPMAP,
							   STR_CONFIG_HMP_OFF, 3, &drawY, rowIndex, keyState, 24);
	}

	if (g_configRestrictedOptionsModalActive) {
		Config_DrawOptionCycleDisabled(&g_gameConfig.palettizedTextures[profileIdx],
									   STR_CONFIG_PALETTIZED_TEXTURES, STR_OFF, 2, &drawY, rowIndex, keyState,
									   25);
	} else {
		Config_DrawOptionCycle(&g_gameConfig.palettizedTextures[profileIdx], STR_CONFIG_PALETTIZED_TEXTURES,
							   STR_OFF, 2, &drawY, rowIndex, keyState, 25);
	}

	if (g_configRestrictedOptionsModalActive) {
		Config_DrawOptionCycleDisabled(&g_gameConfig.bilinear[profileIdx], STR_BILINEAR, STR_OFF, 2, &drawY,
									   rowIndex, keyState, 26);
	} else {
		Config_DrawOptionCycle(&g_gameConfig.bilinear[profileIdx], STR_BILINEAR, STR_OFF, 2, &drawY, rowIndex,
							   keyState, 26);
	}

	Config_DrawOptionCycle(&g_gameConfig.hitEffects[profileIdx], STR_HIT_EFFECTS, STR_OFF, 2, &drawY,
						   rowIndex, keyState, 27);
	Config_DrawOptionCycle(&g_gameConfig.particleEffects[profileIdx], STR_PARTICLE_EFFECTS, STR_OFF, 2,
						   &drawY, rowIndex, keyState, 28);
	Config_DrawOptionCycle(&g_gameConfig.trails[profileIdx], STR_PARTICLE_TRAILS, STR_OFF, 2, &drawY,
						   rowIndex, keyState, 29);
	Config_DrawOptionCycle(&g_gameConfig.engineGlow[profileIdx], STR_ENGINE_GLOW, STR_OFF, 2, &drawY,
						   rowIndex, keyState, 31);
	Config_DrawOptionCycle(&g_gameConfig.lensFlare[profileIdx], STR_LENS_FLARE, STR_OFF, 2, &drawY, rowIndex,
						   keyState, 32);
	Config_DrawOptionCycle(&g_gameConfig.hudColor[profileIdx], STR_HUD_COLOR, STR_HUD_COLOR1, 5, &drawY,
						   rowIndex, keyState, 33);

	FrontImage_GetResourceRect("hud", &hudRect);
	hudColorTextWidth =
		FrontendText_MeasureWidth(
			FrontendString_Get((UIString)(STR_HUD_COLOR1 + g_gameConfig.hudColor[profileIdx])), 15) +
		320;
	if ((unsigned int)hudColorTextWidth > 420) {
		previewX = (unsigned int)hudColorTextWidth;
	} else {
		previewX = 420;
	}

	FrontImage_DrawSpriteRectTinted("hud", &hudRect, previewX, hudRect.top + (drawY - hudRect.bottom) - 1,
									(unsigned int)g_hudPreviewColors[g_gameConfig.hudColor[profileIdx]]);
	FrontendDraw_RectInsetXY(&hudRect, -1, -1);
	FrontendDraw_RectOutline(&hudRect, previewX, hudRect.top + (drawY - hudRect.bottom), g_colorLightBlue);
	*y = drawY;
	return 0;
}

// FUNCTION: XWA 0x520F20
int Config_DrawVideoOptionRows(int profileIdx, int* y, int* rowIndex, char* keyState) {
	int drawY;
	int labelX;
	int changed;
	int previousValue;
	int originalValue;
	int currentValue;
	unsigned int driverCount;
	DisplayDriverEntry* drivers;
	const char* label;
	char* activeKeyState;

	drawY = *y;
	if (g_configRestrictedOptionsModalActive) {
		label = FrontendString_Get(STR_CONFIG_VIDEO_DISPLAY_DRIVER);
		labelX = g_configMenuCenterX - FrontendText_MeasureWidth(label, 15) - 10;
		if (g_menuCursorRow == *rowIndex) {
			FrontendText_Draw(15, label, labelX, drawY, 0xffff);
		} else {
			FrontendText_Draw(15, label, labelX, drawY, g_colorGray);
		}
		drivers = FrontendDisplay_GetDriverTable(&driverCount);
		if (g_gameConfig.threedDevice[profileIdx] >= driverCount) {
			g_gameConfig.threedDevice[profileIdx] = 0;
		}
		FrontendText_Draw(12, drivers[g_gameConfig.threedDevice[profileIdx]].name, g_configMenuCenterX + 10,
						  drawY + 4, g_colorGray);
	} else {
		label = FrontendString_Get(STR_CONFIG_VIDEO_DISPLAY_DRIVER);
		labelX = g_configMenuCenterX - FrontendText_MeasureWidth(label, 15) - 10;
		changed = 0;
		if (g_menuCursorRow == *rowIndex) {
			FrontendText_Draw(15, label, labelX, drawY, g_colorGreen);
			if (*keyState == CONFIG_KEY_VK_LEFT) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				*keyState = 0;
				changed = 2;
			}
			if (*keyState == CONFIG_KEY_VK_RIGHT) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				*keyState = 0;
				changed = 1;
			}
		} else {
			FrontendText_Draw(15, label, labelX, drawY, g_colorLightBlue);
		}

		previousValue = g_gameConfig.threedDevice[profileIdx];
		drivers = FrontendDisplay_GetDriverTable(&driverCount);
		if (g_gameConfig.threedDevice[profileIdx] >= driverCount) {
			g_gameConfig.threedDevice[profileIdx] = 0;
		}
		changed = FrontendButton_DrawMenuButton(g_configMenuCenterX + 10, drawY + 4,
												drivers[g_gameConfig.threedDevice[profileIdx]].name, 12,
												g_colorPaleBlue, 24, 0, "settingsound") |
				  changed;
		if (changed == 1) {
			originalValue = g_gameConfig.screenRes[profileIdx];
			++g_gameConfig.threedDevice[profileIdx];
			if (g_gameConfig.threedDevice[profileIdx] >= driverCount) {
				g_gameConfig.threedDevice[profileIdx] = 0;
			}
			currentValue = g_gameConfig.screenRes[profileIdx];
			while (!FrontendDisplay_DriverSupportsResolutionBpp(g_gameConfig.threedDevice[profileIdx],
																currentValue, 16)) {
				++g_gameConfig.screenRes[profileIdx];
				if (g_gameConfig.screenRes[profileIdx] >= 6) {
					g_gameConfig.screenRes[profileIdx] = 0;
				}
				currentValue = g_gameConfig.screenRes[profileIdx];
				if (currentValue == originalValue) {
					g_gameConfig.screenRes[profileIdx] = 0;
					break;
				}
			}
		} else if (changed == 2) {
			if (g_gameConfig.threedDevice[profileIdx] == 0) {
				g_gameConfig.threedDevice[profileIdx] = (uint8_t)driverCount;
			}
			--g_gameConfig.threedDevice[profileIdx];
			while (!FrontendDisplay_DriverSupportsResolutionBpp(g_gameConfig.threedDevice[profileIdx],
																g_gameConfig.screenRes[profileIdx], 16)) {
				++g_gameConfig.screenRes[profileIdx];
				if (g_gameConfig.screenRes[profileIdx] >= 6) {
					g_gameConfig.screenRes[profileIdx] = 0;
				}
			}
		}

		if (previousValue != g_gameConfig.threedDevice[profileIdx] && g_gameConfig.threedDevice[profileIdx]) {
			g_gameConfig.use3dHardware[profileIdx] = 1;
		}
	}

	drawY += 20;
	++*rowIndex;
	previousValue = g_gameConfig.use3dHardware[profileIdx];
	if (g_configRestrictedOptionsModalActive) {
		Config_DrawOptionCycleDisabled(&g_gameConfig.use3dHardware[profileIdx], STR_USE_3D_HARDWARE,
									   STR_CONFIG_NO, 2, &drawY, rowIndex, keyState, 25);
		if (previousValue != g_gameConfig.use3dHardware[profileIdx] &&
			g_gameConfig.use3dHardware[profileIdx]) {
			g_gameConfig.brightness[profileIdx] = 0;
		}
	} else {
		Config_DrawOptionCycle(&g_gameConfig.use3dHardware[profileIdx], STR_USE_3D_HARDWARE, STR_CONFIG_NO, 2,
							   &drawY, rowIndex, keyState, 25);
		if (previousValue != g_gameConfig.use3dHardware[profileIdx] &&
			g_gameConfig.use3dHardware[profileIdx]) {
			g_gameConfig.brightness[profileIdx] = 0;
			DebugPrintf(NULL);
		}
	}

	if (!g_configRestrictedOptionsModalActive) {
		if (g_gameConfig.use3dHardware[profileIdx]) {
			if (!profileIdx) {
				Frontend3D_SetRuntimeHardwareEnabled(1);
			}
		} else {
			g_gameConfig.screenRes[profileIdx] = 0;
			if (!profileIdx) {
				Frontend3D_SetRuntimeHardwareEnabled(0);
			}
		}
	}

	if (g_configRestrictedOptionsModalActive) {
		label = FrontendString_Get(STR_SCREEN_RESOLUTION);
		labelX = g_configMenuCenterX - FrontendText_MeasureWidth(label, 15) - 10;
		if (g_menuCursorRow == *rowIndex) {
			FrontendText_Draw(15, label, labelX, drawY, 0xffff);
		} else {
			FrontendText_Draw(15, label, labelX, drawY, g_colorGray);
		}
		FrontendText_Draw(
			15, FrontendString_Get((UIString)(STR_SCREEN640X480 + g_gameConfig.screenRes[profileIdx])),
			g_configMenuCenterX + 10, drawY, g_colorGray);
		activeKeyState = keyState;
	} else {
		label = FrontendString_Get(STR_SCREEN_RESOLUTION);
		labelX = g_configMenuCenterX - FrontendText_MeasureWidth(label, 15) - 10;
		changed = 0;
		if (g_menuCursorRow == *rowIndex) {
			if (g_gameConfig.use3dHardware[profileIdx]) {
				FrontendText_Draw(15, label, labelX, drawY, g_colorGreen);
			} else {
				FrontendText_Draw(15, label, labelX, drawY, 0xffff);
			}
			activeKeyState = keyState;
			if (g_gameConfig.use3dHardware[profileIdx]) {
				if (*keyState == CONFIG_KEY_VK_LEFT) {
					FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
											  63);
					Keyboard_FlushCharBuffer();
					*keyState = 0;
					changed = 2;
				}
				if (*keyState == CONFIG_KEY_VK_RIGHT) {
					FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
											  63);
					Keyboard_FlushCharBuffer();
					*keyState = 0;
					changed = 1;
				}
			}
		} else {
			unsigned int labelColor;

			labelColor = g_gameConfig.use3dHardware[profileIdx] ? g_colorLightBlue : g_colorGray;
			FrontendText_Draw(15, label, labelX, drawY, labelColor);
			activeKeyState = keyState;
		}

		if (g_gameConfig.use3dHardware[profileIdx]) {
			changed |= FrontendButton_DrawMenuButton(
				g_configMenuCenterX + 10, drawY,
				FrontendString_Get((UIString)(STR_SCREEN640X480 + g_gameConfig.screenRes[profileIdx])), 15,
				g_colorPaleBlue, 26, 0, "settingsound");
		} else {
			FrontendText_Draw(
				15, FrontendString_Get((UIString)(STR_SCREEN640X480 + g_gameConfig.screenRes[profileIdx])),
				g_configMenuCenterX + 10, drawY, g_colorGray);
		}

		if (changed == 1) {
			originalValue = g_gameConfig.screenRes[profileIdx];
			do {
				++g_gameConfig.screenRes[profileIdx];
				if (g_gameConfig.screenRes[profileIdx] >= 6) {
					g_gameConfig.screenRes[profileIdx] = 0;
				}
				currentValue = g_gameConfig.screenRes[profileIdx];
				if (currentValue == originalValue) {
					g_gameConfig.screenRes[profileIdx] = 0;
					break;
				}
			} while (!FrontendDisplay_DriverSupportsResolutionBpp(g_gameConfig.threedDevice[profileIdx],
																  currentValue, 16));
		} else if (changed == 2) {
			originalValue = g_gameConfig.screenRes[profileIdx];
			do {
				if (g_gameConfig.screenRes[profileIdx]) {
					--g_gameConfig.screenRes[profileIdx];
				} else {
					g_gameConfig.screenRes[profileIdx] = 5;
				}
				currentValue = g_gameConfig.screenRes[profileIdx];
				if (currentValue == originalValue) {
					g_gameConfig.screenRes[profileIdx] = 0;
					break;
				}
			} while (!FrontendDisplay_DriverSupportsResolutionBpp(g_gameConfig.threedDevice[profileIdx],
																  currentValue, 16));
		}
	}

	drawY += 20;
	++*rowIndex;
	previousValue = g_gameConfig.brightness[profileIdx];
	if (g_configRestrictedOptionsModalActive) {
		Config_DrawOptionSliderDisabled(&g_gameConfig.brightness[profileIdx], STR_BRIGHTNESS, STR_DIM, 8,
										&drawY, rowIndex, activeKeyState, 27);
		if (!profileIdx && previousValue != g_gameConfig.brightness[0]) {
			DebugPrintf(NULL);
		}
	} else {
		Config_DrawOptionSlider(&g_gameConfig.brightness[profileIdx], STR_BRIGHTNESS, STR_DIM, 8, &drawY,
								rowIndex, activeKeyState, 27);
		if (!profileIdx && previousValue != g_gameConfig.brightness[0]) {
			DebugPrintf(NULL);
		}
	}

	Config_DrawOptionCycle(&g_gameConfig.debris[profileIdx], STR_SPACE_DEBRIS, STR_OFF, 2, &drawY, rowIndex,
						   activeKeyState, 28);
	if (!profileIdx) {
		Config_DrawOptionSlider(g_gameConfig.debrisDensity, STR_CONFIG_DEBRIS_DENSITY, STR_CONFIG_DEBRIS_FEW,
								7, &drawY, rowIndex, activeKeyState, 29);
	}
	Config_DrawOptionCycle(&g_gameConfig.backdrop[profileIdx], STR_BACKDROP, STR_OFF, 2, &drawY, rowIndex,
						   activeKeyState, 31);
	Config_DrawOptionCycle(&g_gameConfig.starDensity[profileIdx], STR_STARFIELD_DENSITY, STR_LOW_DENSITY, 3,
						   &drawY, rowIndex, activeKeyState, 32);
	Config_DrawOptionSlider(&g_gameConfig.lod[profileIdx], STR_LEVEL_OF_DETAIL, STR_NEAR, 20, &drawY,
							rowIndex, activeKeyState, 33);
	Config_DrawOptionSlider(&g_gameConfig.yardLod[profileIdx], STR_CONFIG_YARD_LOD, STR_CONFIG_YARD_FEW, 20,
							&drawY, rowIndex, activeKeyState, 38);
	if (g_configRestrictedOptionsModalActive) {
		Config_DrawOptionCycleDisabled(&g_gameConfig.textureRes[profileIdx], STR_TEXTURE_RESOLUTION, STR_LOW,
									   3, &drawY, rowIndex, activeKeyState, 34);
	} else {
		Config_DrawOptionCycle(&g_gameConfig.textureRes[profileIdx], STR_TEXTURE_RESOLUTION, STR_LOW, 3,
							   &drawY, rowIndex, activeKeyState, 34);
	}
	if (g_configRestrictedOptionsModalActive) {
		Config_DrawOptionCycleDisabled(&g_gameConfig.explosionRes[profileIdx], STR_EXPLOSION_TEXTURE_RES,
									   STR_LOW, 3, &drawY, rowIndex, activeKeyState, 35);
	} else {
		Config_DrawOptionCycle(&g_gameConfig.explosionRes[profileIdx], STR_EXPLOSION_TEXTURE_RES, STR_LOW, 3,
							   &drawY, rowIndex, activeKeyState, 35);
	}
	Config_DrawOptionCycle(&g_gameConfig.localLights[profileIdx], STR_LOCAL_LIGHT_SOURCE, STR_SOFF, 3, &drawY,
						   rowIndex, activeKeyState, 36);
	Config_DrawOptionCycle(&g_gameConfig.diffuse[profileIdx], STR_DIFFUSE_LIGHTING, STR_OFF, 2, &drawY,
						   rowIndex, activeKeyState, 37);

	*y = drawY;
	return 0;
}

// FUNCTION: XWA 0x51E660
int Config_VideoOptionsMenu(void) {
	char keyState;
	const char* title;
	const char* text;
	int textX;
	int titleX;
	int buttonPressed;
	unsigned int titleWidth;
	FrontendRect rect;

	keyState = (char)Config_GetMenuNavKey();
	if (keyState == CONFIG_KEY_VK_UP) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
		if (--g_menuCursorRow < 0) {
#ifdef XWA_MODERN
			g_menuCursorRow = 3;
#else
			g_menuCursorRow = 2;
#endif
		}
	} else if (keyState == CONFIG_KEY_VK_DOWN) {
		FrontendSound_PlayUISound("configsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		Keyboard_FlushCharBuffer();
		keyState = 0;
#ifdef XWA_MODERN
		if (++g_menuCursorRow >= 4) {
#else
		if (++g_menuCursorRow >= 3) {
#endif
			g_menuCursorRow = 0;
		}
	}

	FrontendDraw_RectAssign(&rect, 0, 178, 639, 193);
	FrontendText_DrawCentered(20, FrontendString_Get(STR_CONFIG_VIDEO_OPTIONS), &rect, g_colorLightBlue);
	titleWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_CONFIG_VIDEO_OPTIONS), 20);
	titleX = g_configMenuCenterX - (int)(titleWidth >> 1);
	FrontendDraw_Line(titleX, 195, titleX + (int)titleWidth, 195, g_colorLightBlue);

#ifdef XWA_MODERN
	text = "OpenXWA Video Options";
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed =
		FrontendButton_DrawMenuButton(textX, 203, text, 20, g_colorPaleBlue, 20, 0, "settingsound");
	if (g_menuCursorRow == 0) {
		FrontendText_Draw(20, text, textX, 203, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 18;
		g_menuCursorRow = 0;
	}
#endif

	text = FrontendString_Get(STR_CONFIG_VIDEO_SINGLE_PLAYER_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
#ifdef XWA_MODERN
	buttonPressed =
		FrontendButton_DrawMenuButton(textX, 228, text, 20, g_colorPaleBlue, 21, 0, "settingsound");
	if (g_menuCursorRow == 1) {
		FrontendText_Draw(20, text, textX, 228, g_colorGreen);
#else
	buttonPressed =
		FrontendButton_DrawMenuButton(textX, 203, text, 20, g_colorPaleBlue, 20, 0, "settingsound");
	if (g_menuCursorRow == 0) {
		FrontendText_Draw(20, text, textX, 203, g_colorGreen);
#endif
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 7;
		g_menuCursorRow = 0;
	}

	text = FrontendString_Get(STR_CONFIG_VIDEO_MULTIPLAYER_OPTIONS);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
#ifdef XWA_MODERN
	buttonPressed =
		FrontendButton_DrawMenuButton(textX, 253, text, 20, g_colorPaleBlue, 22, 0, "settingsound");
	if (g_menuCursorRow == 2) {
		FrontendText_Draw(20, text, textX, 253, g_colorGreen);
#else
	buttonPressed =
		FrontendButton_DrawMenuButton(textX, 228, text, 20, g_colorPaleBlue, 21, 0, "settingsound");
	if (g_menuCursorRow == 1) {
		FrontendText_Draw(20, text, textX, 228, g_colorGreen);
#endif
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 8;
		g_menuCursorRow = 0;
	}

#ifdef XWA_MODERN
	text = FrontendString_Get(STR_BACK);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed =
		FrontendButton_DrawMenuButton(textX, 278, text, 20, g_colorPaleBlue, 24, 0, "settingsound");
	if (g_menuCursorRow == 3) {
		FrontendText_Draw(20, text, textX, 278, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
#else
	text = FrontendString_Get(STR_BACK);
	titleWidth = FrontendText_MeasureWidth(text, 20);
	textX = g_configMenuCenterX - (int)(titleWidth >> 1);
	buttonPressed =
		FrontendButton_DrawMenuButton(textX, 253, text, 20, g_colorPaleBlue, 24, 0, "settingsound");
	if (g_menuCursorRow == 2) {
		FrontendText_Draw(20, text, textX, 253, g_colorGreen);
		if (keyState == CONFIG_KEY_ENTER) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			keyState = 0;
			buttonPressed |= 1;
		}
	}
#endif
	if (keyState == CONFIG_KEY_ESCAPE) {
		Keyboard_FlushCharBuffer();
		buttonPressed = 1;
	}
	if (buttonPressed) {
		g_pendingMenuScreen = 0;
		g_menuCursorRow = 2;
	}

	return 0;
}

// FUNCTION: XWA 0x5232F0
int Config_DrawOptionCycleImpl(uint8_t* value, UIString labelId, int valueBaseStrId, int optionCount, int* y,
							   int* rowIndex, char* keyState, int buttonId, int disabled) {
	const char* label;
	const char* valueText;
	int labelX;
	int valueX;
	int changed;
	int labelColor;

	label = FrontendString_Get(labelId);
	labelX = FrontendText_MeasureWidth(label, 15);
	labelX = g_configMenuCenterX - labelX - 10;
	changed = 0;

	if (g_menuCursorRow == *rowIndex) {
		if (disabled) {
			FrontendText_Draw(15, label, labelX, *y, 0xffff);
		} else {
			FrontendText_Draw(15, label, labelX, *y, g_colorGreen);
			if (*keyState == CONFIG_KEY_VK_LEFT) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				*keyState = 0;
				changed = 2;
			}
			if (*keyState == CONFIG_KEY_VK_RIGHT) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				*keyState = 0;
				changed = 1;
			}
		}
	} else {
		labelColor = disabled ? g_colorGray : g_colorLightBlue;
		FrontendText_Draw(15, label, labelX, *y, labelColor);
	}

	valueX = g_configMenuCenterX + 10;
	valueText = FrontendString_Get((UIString)(valueBaseStrId + *value));
	if (disabled) {
		FrontendText_Draw(15, valueText, valueX, *y, g_colorGray);
		changed = 0;
	} else {
		changed = FrontendButton_DrawMenuButton(valueX, *y, valueText, 15, g_colorPaleBlue, buttonId, 0,
												"settingsound") |
				  changed;
		if (changed == 1) {
			++*value;
			if (*value >= optionCount) {
				*value = 0;
			}
		} else if (changed == 2) {
			if (*value) {
				--*value;
			} else {
				*value = (uint8_t)(optionCount - 1);
			}
		}
	}

	*y += 20;
	++*rowIndex;
	return changed;
}

// FUNCTION: XWA 0x5234C0
int Config_DrawOptionNumericStepper(uint8_t* value, UIString labelId, unsigned int fontSize, int maxValue,
									int* y, int* rowIndex, char* keyState, int buttonId) {
	const char* label;
	char valueText[20];
	int mouseX;
	int mouseY;
	int labelX;
	int controlX;
	int valueButtonX;
	int valueFieldWidth;
	int valueWidth;
	int leftRepeatAction;
	int rightRepeatAction;
	int changed;
	ConfigNumericStepperRepeatState* repeatState;
	FrontendRect rect;

	FrontendCursor_GetPos(&mouseX, &mouseY);
	label = FrontendString_Get(labelId);
	labelX = g_configMenuCenterX - FrontendText_MeasureWidth(label, fontSize) - 10;
	leftRepeatAction = 0;
	changed = 0;

	if (g_menuCursorRow == *rowIndex) {
		FrontendText_Draw(15, label, labelX, *y, g_colorGreen);
		if (*keyState == CONFIG_KEY_VK_LEFT) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			*keyState = 0;
			changed = 2;
		}
		if (*keyState == CONFIG_KEY_VK_RIGHT) {
			FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			Keyboard_FlushCharBuffer();
			*keyState = 0;
			changed = 1;
		}
	} else {
		FrontendText_Draw(fontSize, label, labelX, *y + ((int)(15 - fontSize) >> 1), g_colorLightBlue);
	}

	controlX = g_configMenuCenterX + 10;
	repeatState = &g_configNumericStepperRepeatByButtonId[buttonId];
	FrontendDraw_RectAssign(&rect, controlX, *y + 3, g_configMenuCenterX + 20, *y + 15);
	if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
		if (FrontendMouse_GetLeftDown()) {
			++repeatState->leftHoldFrames;
			FrontImage_DrawSprite("tsettingleftd", rect.left, rect.top);
		} else if (FrontendMouse_GetRightDown()) {
			++repeatState->leftHoldFrames;
			FrontImage_DrawSprite("tsettingleftd", rect.left, rect.top);
		} else {
			FrontImage_DrawSprite("tsettingleftu", rect.left, rect.top);
			repeatState->leftHoldFrames = 0;
		}

		if ((FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) &&
			(unsigned int)repeatState->leftHoldFrames < 3) {
			repeatState->leftHoldFrames = 3;
		}
		if ((unsigned int)repeatState->leftHoldFrames > 1) {
			if ((unsigned int)repeatState->leftHoldFrames >= 24) {
				leftRepeatAction = 2;
			} else if ((repeatState->leftHoldFrames & 3) == 3) {
				leftRepeatAction = 1;
			}
		}
	} else {
		FrontImage_DrawSprite("tsettingleftu", rect.left, rect.top);
		repeatState->leftHoldFrames = 0;
	}

	if (leftRepeatAction == 1) {
		if (*value) {
			--*value;
		} else {
			*value = (uint8_t)maxValue;
		}
	} else if (leftRepeatAction == 2) {
		if (*value < 5) {
			*value = (uint8_t)(*value + maxValue - 4);
		} else {
			*value = (uint8_t)(*value - 5);
		}
	}

	valueButtonX = controlX + 12;
	sprintf(valueText, "%d", maxValue - 1);
	valueFieldWidth = FrontendText_MeasureWidth(valueText, fontSize) + 10;
	sprintf(valueText, "%d", *value);
	valueWidth = FrontendText_MeasureWidth(valueText, fontSize);
	changed =
		FrontendButton_DrawMenuButton(valueButtonX + ((unsigned int)(valueFieldWidth - valueWidth) >> 1),
									  *y + ((int)(15 - fontSize) >> 1), valueText, fontSize, g_colorPaleBlue,
									  buttonId, 0, "settingsound") |
		changed;
	if (changed == 1) {
		++*value;
		if (*value > maxValue) {
			*value = 0;
		}
	} else if (changed == 2) {
		if (*value) {
			--*value;
		} else {
			*value = (uint8_t)maxValue;
		}
	}

	FrontendDraw_RectAssign(&rect, valueButtonX + valueFieldWidth + 2, *y + 3,
							valueButtonX + valueFieldWidth + 12, *y + 15);
	rightRepeatAction = 0;
	if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
		if (FrontendMouse_GetLeftDown()) {
			++repeatState->rightHoldFrames;
			FrontImage_DrawSprite("tsettingrightd", rect.left, rect.top);
		} else if (FrontendMouse_GetRightDown()) {
			++repeatState->rightHoldFrames;
			FrontImage_DrawSprite("tsettingrightd", rect.left, rect.top);
		} else {
			FrontImage_DrawSprite("tsettingrightu", rect.left, rect.top);
			repeatState->rightHoldFrames = 0;
		}

		if ((FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) &&
			(unsigned int)repeatState->rightHoldFrames < 3) {
			repeatState->rightHoldFrames = 3;
		}
		if ((unsigned int)repeatState->rightHoldFrames > 1) {
			if ((unsigned int)repeatState->rightHoldFrames >= 24) {
				rightRepeatAction = 2;
			} else if ((repeatState->rightHoldFrames & 3) == 3) {
				rightRepeatAction = 1;
			}
		}
	} else {
		FrontImage_DrawSprite("tsettingrightu", rect.left, rect.top);
		repeatState->rightHoldFrames = 0;
	}

	if (rightRepeatAction == 1) {
		++*value;
		if (*value > maxValue) {
			*value = 0;
		}
	} else if (rightRepeatAction == 2) {
		*value = (uint8_t)(*value + 5);
		if (*value > maxValue) {
			*value = (uint8_t)(*value - maxValue - 1);
		}
	}

	*y += 20;
	++*rowIndex;
	return changed | rightRepeatAction;
}

// FUNCTION: XWA 0x523A30
int Config_DrawOptionSlider(uint8_t* value, UIString labelId, UIString rangeLabelId, int notchCount, int* y,
							int* rowIndex, char* keyState, int buttonId) {
	return Config_DrawOptionSliderImpl(value, labelId, rangeLabelId, notchCount, y, rowIndex, keyState,
									   buttonId, 0);
}

// FUNCTION: XWA 0x5239F0
int Config_DrawOptionSliderDisabled(uint8_t* value, UIString labelId, UIString rangeLabelId, int notchCount,
									int* y, int* rowIndex, char* keyState, int buttonId) {
	return Config_DrawOptionSliderImpl(value, labelId, rangeLabelId, notchCount, y, rowIndex, keyState,
									   buttonId, 1);
}

// FUNCTION: XWA 0x523A70
int Config_DrawOptionSliderImpl(uint8_t* value, UIString labelId, UIString rangeLabelId, int notchCount,
								int* y, int* rowIndex, char* keyState, int buttonId, int disabled) {
	const char* label;
	const char* minLabel;
	const char* maxLabel;
	int mouseX;
	int mouseY;
	int labelX;
	int sliderX;
	int minLabelWidth;
	int maxLabelWidth;
	int segmentWidth;
	int notchWidth;
	int notchX;
	int notchIndex;
	int pixelIndex;
	int changed;
	FrontendRect rect;

	(void)buttonId;
	FrontendCursor_GetPos(&mouseX, &mouseY);
	label = FrontendString_Get(labelId);
	labelX = g_configMenuCenterX - FrontendText_MeasureWidth(label, 15) - 10;
	changed = 0;

	if (g_menuCursorRow == *rowIndex) {
		if (disabled) {
			FrontendText_Draw(15, label, labelX, *y, 0xffff);
		} else {
			FrontendText_Draw(15, label, labelX, *y, g_colorGreen);
			if (*keyState == CONFIG_KEY_VK_LEFT) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				*keyState = 0;
				changed = 2;
			}
			if (*keyState == CONFIG_KEY_VK_RIGHT) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				Keyboard_FlushCharBuffer();
				*keyState = 0;
				changed = 1;
			}
		}
	} else {
		FrontendText_Draw(15, label, labelX, *y, disabled ? g_colorGray : g_colorLightBlue);
	}

	sliderX = g_configMenuCenterX + 10;
	minLabel = FrontendString_Get(rangeLabelId);
	minLabelWidth = FrontendText_MeasureWidth(minLabel, 10);
	FrontendText_Draw(10, minLabel, sliderX, *y + 4, disabled ? g_colorGray : g_colorLightBlue);
	FrontendDraw_RectAssign(&rect, sliderX, *y, minLabelWidth + sliderX + 1, *y + 15);
	if (!disabled && FrontendDraw_PointInRect(&rect, mouseX, mouseY) &&
		(FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown())) {
		FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		*value = 0;
	}

	notchX = minLabelWidth + g_configMenuCenterX + 16;
	segmentWidth = 120 / notchCount;
	if (notchCount > 0) {
		notchWidth = segmentWidth + 1;
		for (notchIndex = 0; notchIndex < notchCount; ++notchIndex) {
			FrontendDraw_RectAssign(&rect, notchX, *y + 5, notchX + notchWidth, *y + 15);
			if (!disabled && FrontendDraw_PointInRect(&rect, mouseX, mouseY) &&
				(FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown())) {
				if (*value != notchIndex + 1) {
					FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
											  63);
					*value = (uint8_t)(notchIndex + 1);
				}
			}

			rect.right -= 2;
			if (notchIndex < *value) {
				for (pixelIndex = 0; pixelIndex < notchWidth; ++pixelIndex) {
					FrontImage_DrawSprite("sbarcenter", rect.left + pixelIndex, *y + 5);
				}
			}
			notchX += notchWidth;
		}
	}

	FrontImage_DrawSprite("sbarstart", minLabelWidth + g_configMenuCenterX + 10, *y + 5);
	if (*value) {
		FrontImage_DrawSprite("sbarspark",
							  minLabelWidth + *value * (segmentWidth + 1) + g_configMenuCenterX + 15, *y + 5);
	}

	maxLabel = FrontendString_Get((UIString)(rangeLabelId + 1));
	maxLabelWidth = FrontendText_MeasureWidth(maxLabel, 10);
	FrontendText_Draw(10, maxLabel, notchX + 15, *y + 4, disabled ? g_colorGray : g_colorLightBlue);
	FrontendDraw_RectAssign(&rect, notchX + 15, *y, maxLabelWidth + notchX + 16, *y + 15);
	if (!disabled && FrontendDraw_PointInRect(&rect, mouseX, mouseY) &&
		(FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown())) {
		FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		*value = (uint8_t)notchCount;
	}

	if (!disabled) {
		if (changed == 1) {
			++*value;
			if (*value > notchCount) {
				*value = (uint8_t)notchCount;
			}
		} else if (changed == 2 && *value) {
			--*value;
		}
	}

	*y += 20;
	++*rowIndex;
	return changed;
}

// FUNCTION: XWA 0x523F00
int Config_DrawOptionTextEdit(char* text, unsigned int maxLen, int unused, char* label, int* y, int* rowIndex,
							  char* keyState, int buttonId) {
	char buffer[256];
	int labelWidth;
	int labelX;
	int selected;
	int* yPtr;
	int valueX;

	(void)unused;
#ifdef XWA_MODERN
	if (g_configTextEditRunActive) {
		if (!FrontendDialog_EditText(text, maxLen, label)) {
			return 0;
		}
		g_configTextEditRunActive = 0;
	}
#endif

	labelWidth = FrontendText_MeasureWidth(label, 15);
	labelX = g_configMenuCenterX - labelWidth - 10;
	selected = 0;
	if (g_menuCursorRow == *rowIndex) {
		yPtr = y;
		FrontendText_Draw(15, label, labelX, *y, g_colorGreen);
		if (*keyState == CONFIG_KEY_ENTER) {
			Keyboard_FlushCharBuffer();
			*keyState = 0;
			selected = 1;
		}
	} else {
		yPtr = y;
		FrontendText_Draw(15, label, labelX, *y, g_colorLightBlue);
	}

	valueX = g_configMenuCenterX + 10;
	sprintf(buffer, "%-30s", text);
	selected |= FrontendButton_DrawMenuButton(valueX, *yPtr + 1, buffer, 12, g_colorPaleBlue, buttonId, 0,
											  "settingsound");
	if (selected) {
#ifdef XWA_MODERN
		g_configTextEditRunActive = 1;
#endif
		FrontendDialog_EditText(text, maxLen, label);
	}

	*yPtr += 20;
	++*rowIndex;
	return 0;
}

// FUNCTION: XWA 0x524020
int Config_DrawJoystickBindingRow(uint16_t* boundActionCode, char* label, int* y, int* rowIndex,
								  char* keyState, int buttonId) {
	char text[256];
	int* rowIndexPtr;
	int labelX;
	int valueX;
	int selected;
	int* yPtr;
	unsigned int entryIndex;
	JoystickEntry* entry;

	labelX = g_configMenuCenterX - FrontendText_MeasureWidth(label, 15) - 110;
	rowIndexPtr = rowIndex;
	selected = 0;
	if (g_menuCursorRow == *rowIndexPtr) {
		yPtr = y;
		FrontendText_Draw(15, label, labelX, *y, g_colorGreen);
		if (*keyState == '\r') {
			Keyboard_FlushCharBuffer();
			*keyState = 0;
			selected = 1;
		}
	} else {
		yPtr = y;
		FrontendText_Draw(15, label, labelX, *y, g_colorLightBlue);
	}

	entryIndex = 0;
	valueX = g_configMenuCenterX - 90;
	for (entry = g_joystickEntries; entryIndex < g_joystickEntryCount; ++entryIndex, ++entry) {
		if (entry->actionCode == *boundActionCode) {
			break;
		}
	}

	if (entryIndex == g_joystickEntryCount) {
		*boundActionCode = 0;
		entryIndex = 0;
	}

	if (!entryIndex) {
		strcpy(text, FrontendString_Get(STR_NOT_MAPPED));
		rowIndexPtr = rowIndex;
	} else {
		sprintf(text, "%s: %s", g_joystickEntries[entryIndex].name,
				g_joystickEntries[entryIndex].description);
	}

	selected |= FrontendButton_DrawMenuButton(valueX, *yPtr + 3, text, 12, g_colorPaleBlue, buttonId, 0,
											  "settingsound");
	*yPtr += 20;
	++*rowIndexPtr;
	return selected;
}

// FUNCTION: XWA 0x5261F0
int Config_LoadJoystickActionDictionary(void) {
	XwaFile* stream;
	char token[256];
	char* cursor;
	int tokenIndex;
	int entryIndex;

	stream = File_Open(AERON_VFS_ROOT_ASSET, "ALLIANCE/JOYSTICK.TXT", "r");
	if (stream == NULL) {
		return 0;
	}

	g_joystickEntryCount = 0;
	while (File_ReadLine(stream, g_frontendScratchBuffer, 128)) {
		size_t lineLength;

		lineLength = strlen(g_frontendScratchBuffer);
		if (lineLength != 0 && g_frontendScratchBuffer[lineLength - 1] == '\n') {
			g_frontendScratchBuffer[lineLength - 1] = '\0';
		}

		strcpy(token, Linez_ResolveString(g_frontendScratchBuffer));
		strcpy(g_frontendScratchBuffer, token);

		cursor = g_frontendScratchBuffer;
		tokenIndex = 0;
		while (*cursor != ' ' && *cursor != '\0' && tokenIndex < 256) {
			token[tokenIndex++] = *cursor++;
		}
		token[tokenIndex] = '\0';

		cursor = cursor + 1;
		entryIndex = (int)g_joystickEntryCount;
		g_joystickEntries[entryIndex].actionCode = (uint16_t)atoi(token);

		tokenIndex = 0;
		while (*cursor != ' ' && *cursor != '\0' && tokenIndex < 256) {
			token[tokenIndex++] = *cursor++;
		}
		token[tokenIndex] = '\0';
		strcpy(g_joystickEntries[entryIndex].name, token);
		strcpy(g_joystickEntries[entryIndex].description, cursor + 1);

		++g_joystickEntryCount;
	}

	File_Close(stream);
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x526910
uint16_t Config_ReadJoystickActionPickerKey(void) {
	int i;

	if (Keyboard_IsKeyDown(0x12)) {
		if (Keyboard_IsKeyDown(0xc0)) {
			return (uint16_t)(KEY_PAD_DOT | KEY_BACKSPACE | KEY_SPACE);
		}

		for (i = 0; i < 26; ++i) {
			if (Keyboard_IsKeyDown((unsigned char)(i + 65))) {
				return (uint16_t)(i + 128);
			}
		}

		for (i = 0; i < 10; ++i) {
			if (Keyboard_IsKeyDown((unsigned char)(i + 48))) {
				return (uint16_t)(i + 154);
			}
		}

		goto end;
	}

	if (Keyboard_IsKeyDown(0x10)) {
		for (i = 0; i < 12; ++i) {
			if (Keyboard_IsKeyDown((unsigned char)(i + 112))) {
				return (uint16_t)(i + 207);
			}
		}

		goto end;
	}

	if (Keyboard_IsKeyDown(0x11)) {
		for (i = 0; i < 10; ++i) {
			if (Keyboard_IsKeyDown((unsigned char)(i + 48))) {
				return (uint16_t)(i + 261);
			}
		}

		goto end;
	}

	for (i = 0; i < 12; ++i) {
		if (Keyboard_IsKeyDown((unsigned char)(i + 112))) {
			return (uint16_t)(i + 195);
		}
	}

	if (Keyboard_IsKeyDown(0x26) || Keyboard_IsKeyDown(0x28)) {
		return KEY_NONE;
	}
	if (Keyboard_IsKeyDown(0x25)) {
		return (uint16_t)(KEY_SPACE | KEY_ALT_E);
	}
	if (Keyboard_IsKeyDown(0x27)) {
		return (uint16_t)(KEY_SPACE | KEY_ALT_E | 1);
	}
	if (Keyboard_IsKeyDown(0x26)) {
		return (uint16_t)(KEY_QUOTES | KEY_ALT_E);
	}
	if (Keyboard_IsKeyDown(0x28)) {
		return (uint16_t)(KEY_QUOTES | KEY_ALT_E | 1);
	}
	if (Keyboard_IsKeyDown(0x2d)) {
		return KEY_INSERT;
	}
	if (Keyboard_IsKeyDown(0x2e)) {
		return KEY_DELETE;
	}
	if (Keyboard_IsKeyDown(0x24)) {
		return KEY_HOME;
	}
	if (Keyboard_IsKeyDown(0x23)) {
		return KEY_END;
	}
	if (Keyboard_IsKeyDown(0x21)) {
		return KEY_PAGEUP;
	}
	if (Keyboard_IsKeyDown(0x22)) {
		return KEY_PAGEDOWN;
	}
	if (Keyboard_IsKeyDown(0x2c)) {
		return (uint16_t)(KEY_COMMA | KEY_ALT_C);
	}
	if (Keyboard_IsKeyDown(0x91)) {
		return KEY_SCROLL_LOCK;
	}
	if (Keyboard_IsKeyDown(0x14)) {
		return (uint16_t)(KEY_SPACE | KEY_ALT_B | 0x10);
	}
	if (Keyboard_IsKeyDown(0x60)) {
		return KEY_PAD_0;
	}
	if (Keyboard_IsKeyDown(0x61)) {
		return KEY_PAD_1;
	}
	if (Keyboard_IsKeyDown(0x62)) {
		return KEY_PAD_2;
	}
	if (Keyboard_IsKeyDown(0x63)) {
		return KEY_PAD_3;
	}
	if (Keyboard_IsKeyDown(0x64)) {
		return KEY_PAD_4;
	}
	if (Keyboard_IsKeyDown(0x65)) {
		return KEY_PAD_5;
	}
	if (Keyboard_IsKeyDown(0x66)) {
		return KEY_PAD_6;
	}
	if (Keyboard_IsKeyDown(0x67)) {
		return KEY_PAD_7;
	}
	if (Keyboard_IsKeyDown(0x68)) {
		return KEY_PAD_8;
	}
	if (Keyboard_IsKeyDown(0x69)) {
		return KEY_PAD_9;
	}
	if (Keyboard_IsKeyDown(0x90)) {
		return (uint16_t)(KEY_LESS_THAN | 0x80);
	}
	if (Keyboard_IsKeyDown(0x6a)) {
		return KEY_PAD_STAR;
	}
	if (Keyboard_IsKeyDown(0x6b)) {
		return KEY_PAD_PLUS;
	}
	if (Keyboard_IsKeyDown(0x6d)) {
		return KEY_PAD_MINUS;
	}
	if (Keyboard_IsKeyDown(0x6e)) {
		return KEY_PAD_DOT;
	}
	if (Keyboard_IsKeyDown(0x6f)) {
		return KEY_PAD_SLASH;
	}

end:
	return (uint16_t)(unsigned char)Keyboard_DequeueChar();
}

// FUNCTION: XWA 0x526470
int Config_JoystickActionPickerUpdate(int frameState) {
	FrontendRect rect;
	int outX;
	int outY;
	char text[256];
	unsigned int entryIndex;
	JoystickEntry* entry;
	uint16_t actionKey;
	int pressed;

	if (!frameState) {
		g_configJoystickActionPickerPanelColor = FrontendDisplay_PackRGB(0x20, 0x20, 0x40);
		g_configMenuScrollIndex = 0;
		g_configJoystickActionPickerSelectedIndex = 0;

		entryIndex = 0;
		for (entry = g_joystickEntries; entryIndex < g_joystickEntryCount; ++entryIndex, ++entry) {
			if (*g_configJoystickActionPickerBoundActionCode == entry->actionCode) {
				g_configMenuScrollIndex = (int)entryIndex;
				g_configJoystickActionPickerSelectedIndex = entryIndex;
			}
		}

		Keyboard_FlushCharBuffer();
		FrontendDraw_RectAssign(&rect, 120, 90, 520, 390);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_configJoystickActionPickerPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_configJoystickActionPickerPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_configJoystickActionPickerPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_configJoystickActionPickerPanelColor);
		FrontendFrame_DrawSpriteBorder(&rect);
		FrontendDraw_RectAssign(&rect, 120, 90, 520, 104);
		FrontendText_DrawCentered(12, g_frontendScratchBuffer, &rect, g_colorLightBlue);
		FrontendDraw_Line(130, 104, 510, 104, g_colorLightBlue);

		FrontendDisplay_LockOffscreenSurface();
		FrontendDraw_RectAssign(&rect, 120, 90, 520, 390);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_configJoystickActionPickerPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_configJoystickActionPickerPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_configJoystickActionPickerPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_configJoystickActionPickerPanelColor);
		FrontendFrame_DrawSpriteBorder(&rect);
		FrontendDraw_RectAssign(&rect, 120, 90, 520, 104);
		FrontendText_DrawCentered(12, g_frontendScratchBuffer, &rect, g_colorLightBlue);
		FrontendDraw_Line(130, 104, 510, 104, g_colorLightBlue);
		FrontendDisplay_UnlockOffscreenSurface(1);
	}

	FrontendCursor_GetPos(&outX, &outY);
	(void)outX;
	(void)outY;

	FrontendDraw_RectAssign(&rect, 501, 105, 520, 390);
	g_configMenuScrollIndex = FrontendScrollbar_Draw(&rect, g_configMenuScrollIndex,
													 (int)g_joystickEntryCount, 0, 5, g_colorNavy, 2);

	FrontendDraw_RectAssign(&rect, 120, 105, 500, 390);
	FrontendDisplay_SetScreenClipRect640x480(&rect);
	FrontendDraw_RectAssign(&rect, 120, 105, 500, 119);
	FrontendDraw_RectInsetXY(&rect, 2, 0);

	entryIndex = (unsigned int)g_configMenuScrollIndex;
	entry = &g_joystickEntries[entryIndex];
	while (entryIndex < g_joystickEntryCount && entryIndex < (unsigned int)(g_configMenuScrollIndex + 19)) {
		sprintf(text, "%c%s: %c%s", 2, entry->name, 1, entry->description);
		if (entryIndex == g_configJoystickActionPickerSelectedIndex) {
			pressed = FrontendButton_DrawMenuButton(
				rect.left, rect.top, text, 12, g_colorGreen,
				(int)(entryIndex - (unsigned int)g_configMenuScrollIndex + 51), 0, "settingsound");
		} else {
			pressed = FrontendButton_DrawMenuButton(
				rect.left, rect.top, text, 12, g_colorPaleBlue,
				(int)(entryIndex - (unsigned int)g_configMenuScrollIndex + 51), 0, "settingsound");
		}

		if (pressed) {
			*g_configJoystickActionPickerBoundActionCode = entry->actionCode;
			return 1;
		}

		FrontendDraw_RectOffsetXY(&rect, 0, 15);
		++entryIndex;
		++entry;
	}

	FrontendDisplay_ResetScreenClipRect();
	actionKey = Config_ReadJoystickActionPickerKey();
	if (actionKey == KEY_ESCAPE) {
		return 1;
	}

	if (!g_joystickEntryCount) {
		return 0;
	}

	entryIndex = 0;
	for (entry = g_joystickEntries; entryIndex < g_joystickEntryCount; ++entryIndex, ++entry) {
		if (actionKey != KEY_NONE && actionKey == entry->actionCode) {
			if (g_gameConfig.sfxDatapadEnabled) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}
			g_configMenuScrollIndex = (int)entryIndex;
			*g_configJoystickActionPickerBoundActionCode = entry->actionCode;
			return 1;
		}
	}

	return 0;
}

#ifdef XWA_MODERN
static void Config_FinishJoystickActionPickerModal(void) {
	if (!g_configJoystickActionPickerRunActive) {
		return;
	}

	FrontendText_ResetGlyphScratch();
	FrontendText_SetGlyphGradientBg(g_configJoystickActionPickerSavedGlyphGradientBg);
	FrontendDisplay_ResetScreenClipRect();
	g_configJoystickActionPickerRunActive = 0;
}
#endif

// FUNCTION: XWA 0x5263C0
int Config_RunJoystickActionPicker(const char* title, uint16_t* boundActionCode) {
#ifdef XWA_MODERN
	FrontendScreenModalStatus modalStatus;
	FrontendRect rect;

	if (g_configJoystickActionPickerRunActive) {
		modalStatus = FrontendScreen_GetModalStatus();
		if (modalStatus != FRONTEND_SCREEN_MODAL_INACTIVE && modalStatus != FRONTEND_SCREEN_MODAL_DONE) {
			return 0;
		}

		Config_FinishJoystickActionPickerModal();
		return 1;
	}

	g_configJoystickActionPickerSavedGlyphGradientBg = FrontendText_GetGlyphGradientBg();
	FrontendText_SetGlyphGradientBg(FrontendDisplay_PackRGB(0x10, 0x10, 0x20));
	FrontendButton_IsOverlayTextEnabled();
	FrontendDraw_RectAssign(&rect, 0, 0, 640, 480);
	strcpy(g_frontendScratchBuffer, title);
	g_configJoystickActionPickerBoundActionCode = boundActionCode;
	if (!FrontendScreen_BeginModalWithCleanup(Config_JoystickActionPickerUpdate, &rect,
											  Config_FinishJoystickActionPickerModal)) {
		FrontendText_ResetGlyphScratch();
		FrontendText_SetGlyphGradientBg(g_configJoystickActionPickerSavedGlyphGradientBg);
		FrontendDisplay_ResetScreenClipRect();
		return 0;
	}

	g_configJoystickActionPickerRunActive = 1;
	return 0;
#else
	int savedGlyphGradientBg;
	FrontendRect rect;

	savedGlyphGradientBg = FrontendText_GetGlyphGradientBg();
	FrontendText_SetGlyphGradientBg(FrontendDisplay_PackRGB(0x10, 0x10, 0x20));
	FrontendButton_IsOverlayTextEnabled();
	FrontendDraw_RectAssign(&rect, 0, 0, 640, 480);
	strcpy(g_frontendScratchBuffer, title);
	g_configJoystickActionPickerBoundActionCode = boundActionCode;
	FrontendScreen_RunModal(Config_JoystickActionPickerUpdate, &rect);
	FrontendText_ResetGlyphScratch();
	FrontendText_SetGlyphGradientBg(savedGlyphGradientBg);
	FrontendDisplay_ResetScreenClipRect();
	return 1;
#endif
}

typedef enum ConfigKeyword {
	CONFIG_KEY_LASTPILOT = 0,
	CONFIG_KEY_BACKDROP1 = 1,
	CONFIG_KEY_STARDENSITY1 = 2,
	CONFIG_KEY_DEBRIS1 = 3,
	CONFIG_KEY_LOCALLIGHTS1 = 4,
	CONFIG_KEY_SPECULAR1 = 5,
	CONFIG_KEY_DIFFUSE1 = 6,
	CONFIG_KEY_DITHER1 = 7,
	CONFIG_KEY_TEXTURERES1 = 8,
	CONFIG_KEY_MIPMAP1 = 9,
	CONFIG_KEY_LOD1 = 10,
	CONFIG_KEY_YARD_LOD1 = 11,
	CONFIG_KEY_SCREENRES1 = 12,
	CONFIG_KEY_BPP1 = 13,
	CONFIG_KEY_BRIGHTNESS1 = 14,
	CONFIG_KEY_BACKDROP2 = 15,
	CONFIG_KEY_STARDENSITY2 = 16,
	CONFIG_KEY_DEBRIS2 = 17,
	CONFIG_KEY_LOCALLIGHTS2 = 18,
	CONFIG_KEY_SPECULAR2 = 19,
	CONFIG_KEY_DIFFUSE2 = 20,
	CONFIG_KEY_DITHER2 = 21,
	CONFIG_KEY_TEXTURERES2 = 22,
	CONFIG_KEY_MIPMAP2 = 23,
	CONFIG_KEY_LOD2 = 24,
	CONFIG_KEY_YARD_LOD2 = 25,
	CONFIG_KEY_SCREENRES2 = 26,
	CONFIG_KEY_BPP2 = 27,
	CONFIG_KEY_BRIGHTNESS2 = 28,
	CONFIG_KEY_NETWORKTYPE = 29,
	CONFIG_KEY_PHONENUMBER = 30,
	CONFIG_KEY_IPADDRESS = 31,
	CONFIG_KEY_SFX_EXTERIOR = 32,
	CONFIG_KEY_SFX_INTERIOR = 33,
	CONFIG_KEY_SFX_ENGINE = 34,
	CONFIG_KEY_SFX_DATAPAD = 35,
	CONFIG_KEY_VOICE_PILOT = 36,
	CONFIG_KEY_VOICE_TACTICAL = 37,
	CONFIG_KEY_VOICE_COMMANDER = 38,
	CONFIG_KEY_VOICE_SPECIAL = 39,
	CONFIG_KEY_MUSIC = 40,
	CONFIG_KEY_SFX_DATAPAD_VOLUME = 41,
	CONFIG_KEY_SFX_EXTERIOR_VOLUME = 42,
	CONFIG_KEY_SFX_INTERIOR_VOLUME = 43,
	CONFIG_KEY_SFX_ENGINE_VOLUME = 44,
	CONFIG_KEY_VOICE_VOLUME = 45,
	CONFIG_KEY_MUSIC_VOLUME = 46,
	CONFIG_KEY_DATAPAD_MUSIC_VOLUME = 47,
	CONFIG_KEY_JOYBUTTON1 = 48,
	CONFIG_KEY_JOYBUTTON20 = 67,
	CONFIG_KEY_DIFFICULTY = 80,
	CONFIG_KEY_COLLISIONS = 81,
	CONFIG_KEY_TOUR_DIFFICULTY = 82,
	CONFIG_KEY_TOUR_COLLISIONS = 83,
	CONFIG_KEY_CRAFT_JUMPING = 84,
	CONFIG_KEY_REQUIRE_PASSWORD = 85,
	CONFIG_KEY_IN_PROGRESS_JOIN = 86,
	CONFIG_KEY_CRAFT_SELECTION = 87,
	CONFIG_KEY_LOCATE_PLAYERS = 88,
	CONFIG_KEY_LAST_TEAM_TIME_LIMIT = 89,
	CONFIG_KEY_RANDOM_SEED = 90,
	CONFIG_KEY_PASSWORD = 91,
	CONFIG_KEY_ASYNC_FLAG = 92,
	CONFIG_KEY_EACH_TEAM_OWN_REGION = 93,
	CONFIG_KEY_NUMBER_OF_TEAMS = 94,
	CONFIG_KEY_ENVIRONMENT = 95,
	CONFIG_KEY_AI_OPPONENTS = 96,
	CONFIG_KEY_MAX_POINTS = 97,
	CONFIG_KEY_INITIAL_DISTANCE = 98,
	CONFIG_KEY_HELP_ON = 99,
	CONFIG_KEY_DATAPAD_MUSIC = 100,
	CONFIG_KEY_SERVER_UPDATE_RATE = 101,
	CONFIG_KEY_TAUNT1 = 102,
	CONFIG_KEY_TAUNT4 = 105,
	CONFIG_KEY_USE_3D_HARDWARE1 = 106,
	CONFIG_KEY_BILINEAR1 = 107,
	CONFIG_KEY_USE_3D_HARDWARE2 = 108,
	CONFIG_KEY_BILINEAR2 = 109,
	CONFIG_KEY_RUDDER_ENABLED = 110,
	CONFIG_KEY_FLIP_RUDDER = 111,
	CONFIG_KEY_FF_STRENGTH = 112,
	CONFIG_KEY_FF_CENTER = 113,
	CONFIG_KEY_FF_ENABLED = 114,
	CONFIG_KEY_FLIP_Y = 115,
	CONFIG_KEY_3D_SOUND_ENABLED = 116,
	CONFIG_KEY_NUMBER_OF_SFX = 117,
	CONFIG_KEY_HIT_EFFECTS1 = 118,
	CONFIG_KEY_ENGINE_GLOW1 = 119,
	CONFIG_KEY_LENSFLARE1 = 120,
	CONFIG_KEY_HIT_EFFECTS2 = 121,
	CONFIG_KEY_ENGINE_GLOW2 = 122,
	CONFIG_KEY_LENSFLARE2 = 123,
	CONFIG_KEY_THREED_DEVICE1 = 124,
	CONFIG_KEY_THREED_DEVICE2 = 125,
	CONFIG_KEY_TEAM_GOAL1 = 126,
	CONFIG_KEY_TEAM_GOAL8 = 133,
	CONFIG_KEY_HUD_COLOR1 = 134,
	CONFIG_KEY_HUD_COLOR2 = 135,
	CONFIG_KEY_PRESET_THROTTLE1 = 136,
	CONFIG_KEY_PRESET_LASER1 = 137,
	CONFIG_KEY_PRESET_SHIELD1 = 138,
	CONFIG_KEY_PRESET_BEAM1 = 139,
	CONFIG_KEY_PRESET_THROTTLE2 = 140,
	CONFIG_KEY_PRESET_LASER2 = 141,
	CONFIG_KEY_PRESET_SHIELD2 = 142,
	CONFIG_KEY_PRESET_BEAM2 = 143,
	CONFIG_KEY_INVULNERABLE = 144,
	CONFIG_KEY_UNLIMITED_AMMO = 145,
	CONFIG_KEY_TOUR_INVULNERABLE = 146,
	CONFIG_KEY_TOUR_UNLIMITED_AMMO = 147,
	CONFIG_KEY_SFX_QUALITY = 148,
	CONFIG_KEY_HARDWARE_MIPMAP1 = 149,
	CONFIG_KEY_HARDWARE_MIPMAP2 = 150,
	CONFIG_KEY_PALETTIZED_TEXTURES1 = 151,
	CONFIG_KEY_PALETTIZED_TEXTURES2 = 152,
	CONFIG_KEY_DEBRIS_DENSITY1 = 153,
	CONFIG_KEY_DEBRIS_DENSITY2 = 154,
	CONFIG_KEY_FIREWALL = 155,
	CONFIG_KEY_PORT_NUMBER = 156,
	CONFIG_KEY_COM_PORT = 157,
	CONFIG_KEY_BAUD_RATE = 158,
	CONFIG_KEY_STOP_BITS = 159,
	CONFIG_KEY_PARITY = 160,
	CONFIG_KEY_FLOW_CONTROL = 161,
	CONFIG_KEY_CUR_MODEM = 162,
	CONFIG_KEY_PARTICLE_EFFECTS1 = 163,
	CONFIG_KEY_PARTICLE_EFFECTS2 = 164,
	CONFIG_KEY_TRAILS1 = 165,
	CONFIG_KEY_TRAILS2 = 166,
	CONFIG_KEY_EXPLOSION_RES1 = 167,
	CONFIG_KEY_EXPLOSION_RES2 = 168,
	CONFIG_KEY_LAPS = 169,
	CONFIG_KEY_PERFORMANCE = 170,
	CONFIG_KEY_GOALTYPE = 171,
	CONFIG_KEY_TIME_LIMIT = 172,
	CONFIG_KEY_COUNT = 173
} ConfigKeyword;

// GLOBAL: XWA 0x600620
static const char* const g_configKeywords[CONFIG_KEY_COUNT + 1] = {
	"lastpilot",
	"backdrop1",
	"stardensity1",
	"debris1",
	"locallights1",
	"specular1",
	"diffuse1",
	"dither1",
	"textureres1",
	"mipmap1",
	"lod1",
	"yard_lod1",
	"screenres1",
	"bpp1",
	"brightness1",
	"backdrop2",
	"stardensity2",
	"debris2",
	"locallights2",
	"specular2",
	"diffuse2",
	"dither2",
	"textureres2",
	"mipmap2",
	"lod2",
	"yard_lod2",
	"screenres2",
	"bpp2",
	"brightness2",
	"networktype",
	"phonenumber",
	"ipaddress",
	"sfx_exterior",
	"sfx_interior",
	"sfx_engine",
	"sfx_datapad",
	"voice_pilot",
	"voice_tactical_officer",
	"voice_commander",
	"voice_special",
	"music",
	"sfx_datapad_volume",
	"sfx_exterior_volume",
	"sfx_interior_volume",
	"sfx_engine_volume",
	"voice_volume",
	"music_volume",
	"datapad_music_volume",
	"joybutton1",
	"joybutton2",
	"joybutton3",
	"joybutton4",
	"joybutton5",
	"joybutton6",
	"joybutton7",
	"joybutton8",
	"joybutton9",
	"joybutton10",
	"joybutton11",
	"joybutton12",
	"joybutton13",
	"joybutton14",
	"joybutton15",
	"joybutton16",
	"joybutton17",
	"joybutton18",
	"joybutton19",
	"joybutton20",
	"joybutton21",
	"joybutton22",
	"joybutton23",
	"joybutton24",
	"joybutton25",
	"joybutton26",
	"joybutton27",
	"joybutton28",
	"joybutton29",
	"joybutton30",
	"joybutton31",
	"joybutton32",
	"difficulty",
	"collisions",
	"tour_difficulty",
	"tour_collisions",
	"craft_jumping",
	"require_password",
	"in_progress_join",
	"craft_selection",
	"locate_players",
	"last_team_time_limit",
	"random_seed",
	"password",
	"async_flag",
	"each_team_own_region",
	"number_of_teams",
	"environment",
	"ai_opponents",
	"max_points",
	"initial_distance",
	"help_on",
	"datapad_music",
	"server_update_rate",
	"taunt1",
	"taunt2",
	"taunt3",
	"taunt4",
	"use_3d_hardware1",
	"bilinear1",
	"use_3d_hardware2",
	"bilinear2",
	"rudder_enabled",
	"flip_rudder",
	"ff_strength",
	"ff_center",
	"ff_enabled",
	"flip_y",
	"3d_sound_enabled",
	"number_of_sfx",
	"hit_effects1",
	"engine_glow1",
	"lensflare1",
	"hit_effects2",
	"engine_glow2",
	"lensflare2",
	"threed_device1",
	"threed_device2",
	"team_goal1",
	"team_goal2",
	"team_goal3",
	"team_goal4",
	"team_goal5",
	"team_goal6",
	"team_goal7",
	"team_goal8",
	"hud_color1",
	"hud_color2",
	"preset_throttle1",
	"preset_laser1",
	"preset_shield1",
	"preset_beam1",
	"preset_throttle2",
	"preset_laser2",
	"preset_shield2",
	"preset_beam2",
	"invulnerable",
	"unlimited_ammo",
	"tour_invulnerable",
	"tour_unlimited_ammo",
	"sfx_quality",
	"hardware_mipmap1",
	"hardware_mipmap2",
	"palettized_textures1",
	"palettized_textures2",
	"debris_density1",
	"debris_density2",
	"firewall",
	"port_number",
	"com_port",
	"baud_rate",
	"stop_bits",
	"parity",
	"flow_control",
	"cur_modem",
	"particle_effects1",
	"particle_effects2",
	"trails1",
	"trails2",
	"explosion_res1",
	"explosion_res2",
	"laps",
	"performance",
	"goaltype",
	"time_limit",
	"",
};

static int Config_GetJoystickButtonCount(int joystickIndex) { return Joystick_GetButtonCount(joystickIndex); }

static void Config_CopyFixedText(char* dst, size_t dstSize, const char* src) {
	size_t length;

	if (dstSize == 0) {
		return;
	}

	memset(dst, 0, dstSize);
	if (src == NULL) {
		return;
	}

	length = strlen(src);
	if (length >= dstSize) {
		length = dstSize - 1;
	}

	memcpy(dst, src, length);
}

static void Config_CopyLoadedText(char* dst, size_t dstSize, const char* src) {
	size_t length;

	if (dstSize == 0) {
		return;
	}

	memset(dst, 0, dstSize);
	if (src == NULL) {
		return;
	}

	length = strlen(src);
	if (length > dstSize) {
		length = dstSize;
	}

	memcpy(dst, src, length);
}

static uint8_t Config_ReadU8(const char* value) { return (uint8_t)atoi(value); }

static uint16_t Config_ReadU16(const char* value) { return (uint16_t)atoi(value); }

static int Config_FindKeyword(const char* keyword) {
	int i;

	for (i = 0; i < CONFIG_KEY_COUNT && g_configKeywords[i][0] != '\0'; ++i) {
		if (strcmp(g_configKeywords[i], keyword) == 0) {
			return i;
		}
	}

	return -1;
}

static char* Config_SplitLine(char* line) {
	char* value;
	size_t length;

	length = strlen(line);
	if (length != 0 && line[length - 1] == '\n') {
		line[--length] = '\0';
	}
	if (length != 0 && line[length - 1] == '\r') {
		line[--length] = '\0';
	}

	value = strchr(line, ' ');
	if (value == NULL) {
		return NULL;
	}

	*value = '\0';
	return value + 1;
}

static int Config_Printf(XwaFile* stream, const char* format, ...) {
	char buffer[512];
	int count;
	va_list args;

	va_start(args, format);
	count = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	if (count < 0) {
		return 0;
	}

	if ((size_t)count >= sizeof(buffer)) {
		count = (int)sizeof(buffer) - 1;
	}

	return File_WriteCount(stream, buffer, (size_t)count) ? 1 : 0;
}

static void Config_SetInitialGraphicsDefaults(void) {
	int i;

	for (i = 0; i < 2; ++i) {
		g_gameConfig.backdrop[i] = 1;
		g_gameConfig.starDensity[i] = 2;
		g_gameConfig.debris[i] = 1;
		g_gameConfig.localLights[i] = 2;
		g_gameConfig.specular[i] = 1;
		g_gameConfig.diffuse[i] = 1;
		g_gameConfig.dither[i] = 1;
		g_gameConfig.textureRes[i] = 1;
		g_gameConfig.mipmap[i] = 10;
		g_gameConfig.lod[i] = 10;
#ifdef XWA_MODERN
		g_gameConfig.screenRes[i] = FLIGHT_RES_1024x768;
#else
		g_gameConfig.screenRes[i] = 0;
#endif
		g_gameConfig.bpp[i] = 0;
#ifdef XWA_MODERN
		g_gameConfig.brightness[i] = 0;
#else
		g_gameConfig.brightness[i] = 2;
#endif
		g_gameConfig.use3dHardware[i] = 1;
		g_gameConfig.bilinear[i] = 1;
		g_gameConfig.hitEffects[i] = 1;
		g_gameConfig.engineGlow[i] = 1;
		g_gameConfig.lensFlare[i] = 1;
		g_gameConfig.threedDevice[i] = 0;
		g_gameConfig.hudColor[i] = 0;
#ifdef XWA_MODERN
		g_gameConfig.hardwareMipmap[i] = 2;
#else
		g_gameConfig.hardwareMipmap[i] = 0;
#endif
		g_gameConfig.palettizedTextures[i] = 1;
		g_gameConfig.debrisDensity[i] = 4;
		g_gameConfig.particleEffects[i] = 1;
		g_gameConfig.trails[i] = 1;
		g_gameConfig.explosionRes[i] = 1;
	}

	/* TODO: Reimplement the legacy FrontendDisplay_GetDriverTable voodoo/3dfx preference if any
	   compatibility UI still needs a stable hardware-device index. */
}

static void Config_SetInitialJoystickDefaults(void) {
	static const uint16_t defaultButtons[] = { 156, 157, 114, 108, 101, 105, 91, 8, 13, 93 };
	int buttonCount;
	int i;

	buttonCount = Config_GetJoystickButtonCount(0);
	if (buttonCount < 4) {
		buttonCount = 4;
	}

	for (i = 0; i < buttonCount; ++i) {
		if (i >= 16) {
			break;
		}
		if (i < (int)(sizeof(defaultButtons) / sizeof(defaultButtons[0]))) {
			g_gameConfig.joyButtons[i] = defaultButtons[i];
		}
	}

	g_gameConfig.joyButtons[16] = 186;
	g_gameConfig.joyButtons[17] = 184;
	g_gameConfig.joyButtons[18] = 180;
	g_gameConfig.joyButtons[19] = 182;
}

static void Config_SetInitialDefaults(void) {
	int systemRamMb;

	memset(&g_gameConfig, 0, sizeof(g_gameConfig));
	Config_SetInitialGraphicsDefaults();

	g_gameConfig.curModem = 0;
	g_gameConfig.firewall = 0;
	g_gameConfig.portNumber = 0;
	g_gameConfig.comPort = 0;
	g_gameConfig.baudRate = 12;
	g_gameConfig.stopBits = 0;
	g_gameConfig.parity = 0;
	g_gameConfig.flowControl = 0;
	g_gameConfig.networkType = 0;
	g_gameConfig.sfxExteriorEnabled = 1;
	g_gameConfig.sfxInteriorEnabled = 1;
	g_gameConfig.sfxEngineEnabled = 1;
	g_gameConfig.sfxDatapadEnabled = 1;
	g_gameConfig.voicePilotEnabled = 2;
	g_gameConfig.voiceTacticalOfficerEnabled = 2;
	g_gameConfig.voiceCommanderEnabled = 1;
	g_gameConfig.voiceSpecialEnabled = 1;
	g_gameConfig.musicEnabled = 1;
	g_gameConfig.datapadMusicEnabled = 1;
	g_gameConfig.sfxDatapadVolume = 9;
	g_gameConfig.sfxExteriorVolume = 9;
	g_gameConfig.sfxInteriorVolume = 9;
	g_gameConfig.sfxEngineVolume = 5;
	g_gameConfig.voiceVolume = 9;
	g_gameConfig.musicVolume = 5;
	g_gameConfig.datapadMusicVolume = 5;
	g_gameConfig.difficulty = 1;
	g_gameConfig.collisions = 0;
	g_gameConfig.tourDifficulty = 1;
	g_gameConfig.tourCollisions = 1;
	g_gameConfig.craftJumping = 1;
	g_gameConfig.requirePassword = 0;
	g_gameConfig.inProgressJoin = 0;
	g_gameConfig.craftSelection = 1;
	g_gameConfig.locatePlayers = 1;
	g_gameConfig.lastTeamTimeLimit = 1;
	g_gameConfig.timeLimit = 10;
	g_gameConfig.randomSeed = 0;
	g_gameConfig.asyncFlag = 0;
	g_gameConfig.eachTeamOwnRegion = 0;
	g_gameConfig.numberOfTeams = 2;
	g_gameConfig.environment = 0;
	g_gameConfig.aiOpponents = 0;
	g_gameConfig.maxPoints = 10;
	g_gameConfig.initialDistance = 3;
	g_gameConfig.helpOn = 1;
	g_gameConfig.serverUpdateRate = 8;
	g_gameConfig.invulnerable = 0;
	g_gameConfig.unlimitedAmmo = 0;
	g_gameConfig.tourInvulnerable = 0;
	g_gameConfig.tourUnlimitedAmmo = 0;
	g_gameConfig.laps = 3;
	g_gameConfig.rudderEnabled = 1;
	g_gameConfig.ffStrength = 6;
	g_gameConfig.ffCenter = 0;
	g_gameConfig.flipRudder = 0;
	g_gameConfig.ffEnabled = (uint8_t)ForceFeedback_CheckDevice();
	g_gameConfig.flipY = 0;
	g_gameConfig.sound3dEnabled = 0;
	g_gameConfig.numberOfSfx = 8;
	g_gameConfig.sfxQuality = 0;
	g_gameConfig.presetThrottle[0] = 100;
	g_gameConfig.presetLaser[0] = 3;
	g_gameConfig.presetShield[0] = 2;
	g_gameConfig.presetBeam[0] = 2;
	g_gameConfig.presetThrottle[1] = 33;
	g_gameConfig.presetLaser[1] = 2;
	g_gameConfig.presetShield[1] = 2;
	g_gameConfig.presetBeam[1] = 2;

	Config_SetInitialJoystickDefaults();
	g_gameConfig.goalType = 0;

	Config_CopyFixedText(g_gameConfig.taunt1, sizeof(g_gameConfig.taunt1), FrontendString_Get(STR_TAUNT1));
	Config_CopyFixedText(g_gameConfig.taunt2, sizeof(g_gameConfig.taunt2), FrontendString_Get(STR_TAUNT2));
	Config_CopyFixedText(g_gameConfig.taunt3, sizeof(g_gameConfig.taunt3), FrontendString_Get(STR_TAUNT3));
	Config_CopyFixedText(g_gameConfig.taunt4, sizeof(g_gameConfig.taunt4), FrontendString_Get(STR_TAUNT4));

	systemRamMb = 1024;
	if (systemRamMb > 64) {
		Config_SetDetailDefaultsHigh(-1);
	} else if (systemRamMb > 32) {
		Config_SetDetailDefaultsMedium(-1);
	} else {
		Config_SetDetailDefaultsLow(-1);
	}
}

static void Config_ApplyKeywordValue(int keyword, const char* value) {
	uint8_t parsedU8;
	int profile;

	parsedU8 = Config_ReadU8(value);

	switch (keyword) {
		case CONFIG_KEY_LASTPILOT:
			Config_CopyLoadedText(g_gameConfig.lastPilotName, 13, value);
			break;
		case CONFIG_KEY_BACKDROP1:
		case CONFIG_KEY_BACKDROP2:
			g_gameConfig.backdrop[keyword == CONFIG_KEY_BACKDROP2] = parsedU8;
			break;
		case CONFIG_KEY_STARDENSITY1:
		case CONFIG_KEY_STARDENSITY2:
			g_gameConfig.starDensity[keyword == CONFIG_KEY_STARDENSITY2] = parsedU8;
			break;
		case CONFIG_KEY_DEBRIS1:
		case CONFIG_KEY_DEBRIS2:
			g_gameConfig.debris[keyword == CONFIG_KEY_DEBRIS2] = parsedU8;
			break;
		case CONFIG_KEY_LOCALLIGHTS1:
		case CONFIG_KEY_LOCALLIGHTS2:
			g_gameConfig.localLights[keyword == CONFIG_KEY_LOCALLIGHTS2] = parsedU8;
			break;
		case CONFIG_KEY_SPECULAR1:
		case CONFIG_KEY_SPECULAR2:
			g_gameConfig.specular[keyword == CONFIG_KEY_SPECULAR2] = parsedU8;
			break;
		case CONFIG_KEY_DIFFUSE1:
		case CONFIG_KEY_DIFFUSE2:
			g_gameConfig.diffuse[keyword == CONFIG_KEY_DIFFUSE2] = parsedU8;
			break;
		case CONFIG_KEY_DITHER1:
		case CONFIG_KEY_DITHER2:
			g_gameConfig.dither[keyword == CONFIG_KEY_DITHER2] = parsedU8;
			break;
		case CONFIG_KEY_TEXTURERES1:
		case CONFIG_KEY_TEXTURERES2:
			g_gameConfig.textureRes[keyword == CONFIG_KEY_TEXTURERES2] = parsedU8;
			break;
		case CONFIG_KEY_MIPMAP1:
		case CONFIG_KEY_MIPMAP2:
			g_gameConfig.mipmap[keyword == CONFIG_KEY_MIPMAP2] = parsedU8;
			break;
		case CONFIG_KEY_LOD1:
		case CONFIG_KEY_LOD2:
			g_gameConfig.lod[keyword == CONFIG_KEY_LOD2] = parsedU8;
			break;
		case CONFIG_KEY_YARD_LOD1:
		case CONFIG_KEY_YARD_LOD2:
			g_gameConfig.yardLod[keyword == CONFIG_KEY_YARD_LOD2] = parsedU8;
			break;
		case CONFIG_KEY_SCREENRES1:
		case CONFIG_KEY_SCREENRES2:
			g_gameConfig.screenRes[keyword == CONFIG_KEY_SCREENRES2] = parsedU8;
			break;
		case CONFIG_KEY_BPP1:
		case CONFIG_KEY_BPP2:
			g_gameConfig.bpp[keyword == CONFIG_KEY_BPP2] = parsedU8;
			break;
		case CONFIG_KEY_BRIGHTNESS1:
		case CONFIG_KEY_BRIGHTNESS2:
			g_gameConfig.brightness[keyword == CONFIG_KEY_BRIGHTNESS2] = parsedU8;
			break;
		case CONFIG_KEY_NETWORKTYPE:
			g_gameConfig.networkType = parsedU8;
			break;
		case CONFIG_KEY_PHONENUMBER:
			Config_CopyLoadedText(g_gameConfig.phoneNumber, sizeof(g_gameConfig.phoneNumber), value);
			break;
		case CONFIG_KEY_IPADDRESS:
			Config_CopyLoadedText(g_gameConfig.ipAddress, sizeof(g_gameConfig.ipAddress), value);
			break;
		case CONFIG_KEY_SFX_EXTERIOR:
			g_gameConfig.sfxExteriorEnabled = parsedU8;
			break;
		case CONFIG_KEY_SFX_INTERIOR:
			g_gameConfig.sfxInteriorEnabled = parsedU8;
			break;
		case CONFIG_KEY_SFX_ENGINE:
			g_gameConfig.sfxEngineEnabled = parsedU8;
			break;
		case CONFIG_KEY_SFX_DATAPAD:
			g_gameConfig.sfxDatapadEnabled = parsedU8;
			break;
		case CONFIG_KEY_VOICE_PILOT:
			g_gameConfig.voicePilotEnabled = parsedU8;
			break;
		case CONFIG_KEY_VOICE_TACTICAL:
			g_gameConfig.voiceTacticalOfficerEnabled = parsedU8;
			break;
		case CONFIG_KEY_VOICE_COMMANDER:
			g_gameConfig.voiceCommanderEnabled = parsedU8;
			break;
		case CONFIG_KEY_VOICE_SPECIAL:
			g_gameConfig.voiceSpecialEnabled = parsedU8;
			break;
		case CONFIG_KEY_MUSIC:
			g_gameConfig.musicEnabled = parsedU8;
			break;
		case CONFIG_KEY_SFX_DATAPAD_VOLUME:
			g_gameConfig.sfxDatapadVolume = parsedU8;
			break;
		case CONFIG_KEY_SFX_EXTERIOR_VOLUME:
			g_gameConfig.sfxExteriorVolume = parsedU8;
			break;
		case CONFIG_KEY_SFX_INTERIOR_VOLUME:
			g_gameConfig.sfxInteriorVolume = parsedU8;
			break;
		case CONFIG_KEY_SFX_ENGINE_VOLUME:
			g_gameConfig.sfxEngineVolume = parsedU8;
			break;
		case CONFIG_KEY_VOICE_VOLUME:
			g_gameConfig.voiceVolume = parsedU8;
			break;
		case CONFIG_KEY_MUSIC_VOLUME:
			g_gameConfig.musicVolume = parsedU8;
			break;
		case CONFIG_KEY_DATAPAD_MUSIC_VOLUME:
			g_gameConfig.datapadMusicVolume = parsedU8;
			break;
		case CONFIG_KEY_DIFFICULTY:
			g_gameConfig.difficulty = parsedU8;
			break;
		case CONFIG_KEY_COLLISIONS:
			g_gameConfig.collisions = parsedU8;
			break;
		case CONFIG_KEY_TOUR_DIFFICULTY:
			g_gameConfig.tourDifficulty = parsedU8;
			break;
		case CONFIG_KEY_TOUR_COLLISIONS:
			g_gameConfig.tourCollisions = parsedU8;
			break;
		case CONFIG_KEY_CRAFT_JUMPING:
			g_gameConfig.craftJumping = parsedU8;
			break;
		case CONFIG_KEY_REQUIRE_PASSWORD:
			g_gameConfig.requirePassword = parsedU8;
			break;
		case CONFIG_KEY_IN_PROGRESS_JOIN:
			g_gameConfig.inProgressJoin = parsedU8;
			break;
		case CONFIG_KEY_CRAFT_SELECTION:
			g_gameConfig.craftSelection = parsedU8;
			break;
		case CONFIG_KEY_LOCATE_PLAYERS:
			g_gameConfig.locatePlayers = parsedU8;
			break;
		case CONFIG_KEY_LAST_TEAM_TIME_LIMIT:
			g_gameConfig.lastTeamTimeLimit = parsedU8;
			break;
		case CONFIG_KEY_RANDOM_SEED:
			g_gameConfig.randomSeed = atoi(value);
			break;
		case CONFIG_KEY_PASSWORD:
			Config_CopyLoadedText(g_gameConfig.password, sizeof(g_gameConfig.password), value);
			break;
		case CONFIG_KEY_ASYNC_FLAG:
			g_gameConfig.asyncFlag = parsedU8;
			break;
		case CONFIG_KEY_EACH_TEAM_OWN_REGION:
			g_gameConfig.eachTeamOwnRegion = parsedU8;
			break;
		case CONFIG_KEY_NUMBER_OF_TEAMS:
			g_gameConfig.numberOfTeams = parsedU8;
			break;
		case CONFIG_KEY_ENVIRONMENT:
			g_gameConfig.environment = parsedU8;
			break;
		case CONFIG_KEY_AI_OPPONENTS:
			g_gameConfig.aiOpponents = parsedU8;
			break;
		case CONFIG_KEY_MAX_POINTS:
			g_gameConfig.maxPoints = parsedU8;
			break;
		case CONFIG_KEY_INITIAL_DISTANCE:
			g_gameConfig.initialDistance = parsedU8;
			break;
		case CONFIG_KEY_HELP_ON:
			g_gameConfig.helpOn = parsedU8;
			break;
		case CONFIG_KEY_DATAPAD_MUSIC:
			g_gameConfig.datapadMusicEnabled = parsedU8;
			break;
		case CONFIG_KEY_SERVER_UPDATE_RATE:
			g_gameConfig.serverUpdateRate = parsedU8;
			break;
		case CONFIG_KEY_USE_3D_HARDWARE1:
		case CONFIG_KEY_USE_3D_HARDWARE2:
			g_gameConfig.use3dHardware[keyword == CONFIG_KEY_USE_3D_HARDWARE2] = parsedU8;
			break;
		case CONFIG_KEY_BILINEAR1:
		case CONFIG_KEY_BILINEAR2:
			g_gameConfig.bilinear[keyword == CONFIG_KEY_BILINEAR2] = parsedU8;
			break;
		case CONFIG_KEY_RUDDER_ENABLED:
			g_gameConfig.rudderEnabled = parsedU8;
			break;
		case CONFIG_KEY_FLIP_RUDDER:
			g_gameConfig.flipRudder = parsedU8;
			break;
		case CONFIG_KEY_FF_STRENGTH:
			g_gameConfig.ffStrength = parsedU8;
			break;
		case CONFIG_KEY_FF_CENTER:
			g_gameConfig.ffCenter = parsedU8;
			break;
		case CONFIG_KEY_FF_ENABLED:
			g_gameConfig.ffEnabled = parsedU8;
			break;
		case CONFIG_KEY_FLIP_Y:
			g_gameConfig.flipY = parsedU8;
			break;
		case CONFIG_KEY_3D_SOUND_ENABLED:
			g_gameConfig.sound3dEnabled = parsedU8;
			break;
		case CONFIG_KEY_NUMBER_OF_SFX:
			g_gameConfig.numberOfSfx = parsedU8;
			break;
		case CONFIG_KEY_HIT_EFFECTS1:
		case CONFIG_KEY_HIT_EFFECTS2:
			g_gameConfig.hitEffects[keyword == CONFIG_KEY_HIT_EFFECTS2] = parsedU8;
			break;
		case CONFIG_KEY_ENGINE_GLOW1:
		case CONFIG_KEY_ENGINE_GLOW2:
			g_gameConfig.engineGlow[keyword == CONFIG_KEY_ENGINE_GLOW2] = parsedU8;
			break;
		case CONFIG_KEY_LENSFLARE1:
		case CONFIG_KEY_LENSFLARE2:
			g_gameConfig.lensFlare[keyword == CONFIG_KEY_LENSFLARE2] = parsedU8;
			break;
		case CONFIG_KEY_THREED_DEVICE1:
		case CONFIG_KEY_THREED_DEVICE2:
			g_gameConfig.threedDevice[keyword == CONFIG_KEY_THREED_DEVICE2] = parsedU8;
			break;
		case CONFIG_KEY_HUD_COLOR1:
		case CONFIG_KEY_HUD_COLOR2:
			g_gameConfig.hudColor[keyword == CONFIG_KEY_HUD_COLOR2] = parsedU8;
			break;
		case CONFIG_KEY_PRESET_THROTTLE1:
		case CONFIG_KEY_PRESET_THROTTLE2:
			profile = keyword == CONFIG_KEY_PRESET_THROTTLE2;
			g_gameConfig.presetThrottle[profile] = parsedU8;
			if (g_gameConfig.presetThrottle[profile] > 100) {
				g_gameConfig.presetThrottle[profile] = 100;
			}
			break;
		case CONFIG_KEY_PRESET_LASER1:
		case CONFIG_KEY_PRESET_LASER2:
			profile = keyword == CONFIG_KEY_PRESET_LASER2;
			g_gameConfig.presetLaser[profile] = parsedU8;
			if (g_gameConfig.presetLaser[profile] > 4) {
				g_gameConfig.presetLaser[profile] = 4;
			}
			break;
		case CONFIG_KEY_PRESET_SHIELD1:
		case CONFIG_KEY_PRESET_SHIELD2:
			profile = keyword == CONFIG_KEY_PRESET_SHIELD2;
			g_gameConfig.presetShield[profile] = parsedU8;
			if (g_gameConfig.presetShield[profile] > 4) {
				g_gameConfig.presetShield[profile] = 4;
			}
			break;
		case CONFIG_KEY_PRESET_BEAM1:
		case CONFIG_KEY_PRESET_BEAM2:
			profile = keyword == CONFIG_KEY_PRESET_BEAM2;
			g_gameConfig.presetBeam[profile] = parsedU8;
			if (g_gameConfig.presetBeam[profile] > 4) {
				g_gameConfig.presetBeam[profile] = 4;
			}
			break;
		case CONFIG_KEY_INVULNERABLE:
			g_gameConfig.invulnerable = parsedU8;
			break;
		case CONFIG_KEY_UNLIMITED_AMMO:
			g_gameConfig.unlimitedAmmo = parsedU8;
			break;
		case CONFIG_KEY_TOUR_INVULNERABLE:
			g_gameConfig.tourInvulnerable = parsedU8;
			break;
		case CONFIG_KEY_TOUR_UNLIMITED_AMMO:
			g_gameConfig.tourUnlimitedAmmo = parsedU8;
			break;
		case CONFIG_KEY_SFX_QUALITY:
			g_gameConfig.sfxQuality = parsedU8;
			break;
		case CONFIG_KEY_HARDWARE_MIPMAP1:
		case CONFIG_KEY_HARDWARE_MIPMAP2:
			g_gameConfig.hardwareMipmap[keyword == CONFIG_KEY_HARDWARE_MIPMAP2] = parsedU8;
			break;
		case CONFIG_KEY_PALETTIZED_TEXTURES1:
		case CONFIG_KEY_PALETTIZED_TEXTURES2:
			g_gameConfig.palettizedTextures[keyword == CONFIG_KEY_PALETTIZED_TEXTURES2] = parsedU8;
			break;
		case CONFIG_KEY_DEBRIS_DENSITY1:
		case CONFIG_KEY_DEBRIS_DENSITY2:
			g_gameConfig.debrisDensity[keyword == CONFIG_KEY_DEBRIS_DENSITY2] = parsedU8;
			break;
		case CONFIG_KEY_FIREWALL:
			g_gameConfig.firewall = parsedU8;
			break;
		case CONFIG_KEY_PORT_NUMBER:
			g_gameConfig.portNumber = Config_ReadU16(value);
			break;
		case CONFIG_KEY_COM_PORT:
			g_gameConfig.comPort = parsedU8;
			break;
		case CONFIG_KEY_BAUD_RATE:
			g_gameConfig.baudRate = parsedU8;
			break;
		case CONFIG_KEY_STOP_BITS:
			g_gameConfig.stopBits = parsedU8;
			break;
		case CONFIG_KEY_PARITY:
			g_gameConfig.parity = parsedU8;
			break;
		case CONFIG_KEY_FLOW_CONTROL:
			g_gameConfig.flowControl = parsedU8;
			break;
		case CONFIG_KEY_CUR_MODEM:
			g_gameConfig.curModem = parsedU8;
			break;
		case CONFIG_KEY_PARTICLE_EFFECTS1:
		case CONFIG_KEY_PARTICLE_EFFECTS2:
			g_gameConfig.particleEffects[keyword == CONFIG_KEY_PARTICLE_EFFECTS2] = parsedU8;
			break;
		case CONFIG_KEY_TRAILS1:
		case CONFIG_KEY_TRAILS2:
			g_gameConfig.trails[keyword == CONFIG_KEY_TRAILS2] = parsedU8;
			break;
		case CONFIG_KEY_EXPLOSION_RES1:
		case CONFIG_KEY_EXPLOSION_RES2:
			g_gameConfig.explosionRes[keyword == CONFIG_KEY_EXPLOSION_RES2] = parsedU8;
			break;
		case CONFIG_KEY_LAPS:
			g_gameConfig.laps = parsedU8;
			break;
		case CONFIG_KEY_PERFORMANCE:
			g_gameConfig.performance = parsedU8;
			break;
		case CONFIG_KEY_GOALTYPE:
			g_gameConfig.goalType = parsedU8;
			break;
		case CONFIG_KEY_TIME_LIMIT:
			g_gameConfig.timeLimit = parsedU8;
			break;
		default:
			if (keyword >= CONFIG_KEY_JOYBUTTON1 && keyword <= CONFIG_KEY_JOYBUTTON20) {
				g_gameConfig.joyButtons[keyword - CONFIG_KEY_JOYBUTTON1] = Config_ReadU16(value);
			} else if (keyword >= CONFIG_KEY_TAUNT1 && keyword <= CONFIG_KEY_TAUNT4) {
				Config_CopyLoadedText(&g_gameConfig.taunt1[(keyword - CONFIG_KEY_TAUNT1) * 70], 70, value);
			} else if (keyword >= CONFIG_KEY_TEAM_GOAL1 && keyword <= CONFIG_KEY_TEAM_GOAL8) {
				g_gameConfig.teamGoals[keyword - CONFIG_KEY_TEAM_GOAL1] = parsedU8;
			}
			break;
	}
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x528220
int Config_SetDetailDefaultsLow(char groupMask) {
	if ((groupMask & 1) != 0) {
		g_gameConfig.backdrop[0] = 0;
		g_gameConfig.starDensity[0] = 0;
		g_gameConfig.debris[0] = 0;
		g_gameConfig.textureRes[0] = 0;
		g_gameConfig.lod[0] = 5;
		g_gameConfig.yardLod[0] = 5;
		g_gameConfig.debrisDensity[0] = 0;
		g_gameConfig.explosionRes[0] = 0;
	}
	if ((groupMask & 2) != 0) {
		g_gameConfig.localLights[0] = 0;
		g_gameConfig.hitEffects[0] = 0;
		g_gameConfig.engineGlow[0] = 0;
		g_gameConfig.lensFlare[0] = 0;
		g_gameConfig.hardwareMipmap[0] = 0;
		g_gameConfig.particleEffects[0] = 0;
		g_gameConfig.trails[0] = 0;
	}
	if ((groupMask & 4) != 0) {
		g_gameConfig.specular[0] = 0;
		g_gameConfig.mipmap[0] = 5;
	}
	if ((groupMask & 8) != 0) {
		g_gameConfig.backdrop[1] = 0;
		g_gameConfig.starDensity[1] = 0;
		g_gameConfig.debris[1] = 0;
		g_gameConfig.textureRes[1] = 0;
		g_gameConfig.lod[1] = 5;
		g_gameConfig.yardLod[1] = 5;
		g_gameConfig.debrisDensity[1] = 0;
		g_gameConfig.explosionRes[1] = 0;
	}
	if ((groupMask & 0x10) != 0) {
		g_gameConfig.localLights[1] = 0;
		g_gameConfig.hitEffects[1] = 0;
		g_gameConfig.engineGlow[1] = 0;
		g_gameConfig.lensFlare[1] = 0;
		g_gameConfig.hardwareMipmap[1] = 0;
		g_gameConfig.particleEffects[1] = 0;
		g_gameConfig.trails[1] = 0;
	}
	if ((groupMask & 0x20) != 0) {
		g_gameConfig.specular[1] = 0;
		g_gameConfig.mipmap[1] = 5;
	}
	if ((groupMask & 0x40) != 0) {
		g_gameConfig.voicePilotEnabled = 0;
		g_gameConfig.voiceTacticalOfficerEnabled = 0;
		g_gameConfig.voiceCommanderEnabled = 0;
		g_gameConfig.voiceSpecialEnabled = 1;
		g_gameConfig.musicEnabled = 0;
		g_gameConfig.musicVolume = 0;
	}
	if (groupMask & 0x80) {
		g_gameConfig.ffEnabled = 0;
	}

	g_gameConfig.performance = 0;
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x528330
int Config_SetDetailDefaultsMedium(char groupMask) {
	if ((groupMask & 1) != 0) {
		g_gameConfig.backdrop[0] = 1;
		g_gameConfig.starDensity[0] = 1;
		g_gameConfig.debris[0] = 1;
		g_gameConfig.textureRes[0] = 1;
		g_gameConfig.lod[0] = 10;
		g_gameConfig.yardLod[0] = 10;
		g_gameConfig.debrisDensity[0] = 4;
		g_gameConfig.explosionRes[0] = 1;
	}
	if ((groupMask & 2) != 0) {
		g_gameConfig.localLights[0] = 1;
		g_gameConfig.hitEffects[0] = 1;
		g_gameConfig.engineGlow[0] = 1;
		g_gameConfig.lensFlare[0] = 1;
		g_gameConfig.hardwareMipmap[0] = 0;
		g_gameConfig.particleEffects[0] = 1;
		g_gameConfig.trails[0] = 1;
	}
	if ((groupMask & 4) != 0) {
		g_gameConfig.specular[0] = 1;
		g_gameConfig.mipmap[0] = 10;
	}
	if ((groupMask & 8) != 0) {
		g_gameConfig.backdrop[1] = 1;
		g_gameConfig.starDensity[1] = 0;
		g_gameConfig.debris[1] = 0;
		g_gameConfig.textureRes[1] = 1;
		g_gameConfig.lod[1] = 10;
		g_gameConfig.yardLod[1] = 10;
		g_gameConfig.debrisDensity[1] = 0;
		g_gameConfig.explosionRes[1] = 0;
	}
	if ((groupMask & 0x10) != 0) {
		g_gameConfig.localLights[1] = 1;
		g_gameConfig.hitEffects[1] = 1;
		g_gameConfig.engineGlow[1] = 1;
		g_gameConfig.lensFlare[1] = 0;
		g_gameConfig.hardwareMipmap[1] = 0;
		g_gameConfig.particleEffects[1] = 1;
		g_gameConfig.trails[1] = 1;
	}
	if ((groupMask & 0x20) != 0) {
		g_gameConfig.specular[1] = 0;
		g_gameConfig.mipmap[1] = 5;
	}
	if ((groupMask & 0x40) != 0) {
		g_gameConfig.voicePilotEnabled = 1;
		g_gameConfig.voiceTacticalOfficerEnabled = 1;
		g_gameConfig.voiceCommanderEnabled = 1;
		g_gameConfig.voiceSpecialEnabled = 1;
		g_gameConfig.musicEnabled = 1;
		g_gameConfig.musicVolume = 5;
		g_gameConfig.numberOfSfx = 12;
	}
	if (groupMask & 0x80) {
		g_gameConfig.ffEnabled = (uint8_t)ForceFeedback_CheckDevice();
	}

	g_gameConfig.performance = 1;
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x528470
int Config_SetDetailDefaultsHigh(char groupMask) {
	if ((groupMask & 1) != 0) {
		g_gameConfig.backdrop[0] = 1;
		g_gameConfig.starDensity[0] = 2;
		g_gameConfig.debris[0] = 1;
		g_gameConfig.textureRes[0] = 2;
		g_gameConfig.lod[0] = 15;
		g_gameConfig.yardLod[0] = 15;
		g_gameConfig.debrisDensity[0] = 6;
		g_gameConfig.explosionRes[0] = 2;
	}
	if ((groupMask & 2) != 0) {
		g_gameConfig.localLights[0] = 2;
		g_gameConfig.hitEffects[0] = 1;
		g_gameConfig.engineGlow[0] = 1;
		g_gameConfig.lensFlare[0] = 1;
		g_gameConfig.hardwareMipmap[0] = 0;
		g_gameConfig.particleEffects[0] = 1;
		g_gameConfig.trails[0] = 1;
	}
	if ((groupMask & 4) != 0) {
		g_gameConfig.specular[0] = 1;
		g_gameConfig.mipmap[0] = 10;
	}
	if ((groupMask & 8) != 0) {
		g_gameConfig.backdrop[1] = 1;
		g_gameConfig.starDensity[1] = 1;
		g_gameConfig.debris[1] = 1;
		g_gameConfig.textureRes[1] = 2;
		g_gameConfig.yardLod[1] = 10;
		g_gameConfig.lod[1] = 10;
		g_gameConfig.debrisDensity[1] = 0;
		g_gameConfig.explosionRes[1] = 1;
	}
	if ((groupMask & 0x10) != 0) {
		g_gameConfig.localLights[1] = 1;
		g_gameConfig.hitEffects[1] = 1;
		g_gameConfig.engineGlow[1] = 1;
		g_gameConfig.lensFlare[1] = 1;
		g_gameConfig.hardwareMipmap[1] = 0;
		g_gameConfig.particleEffects[1] = 1;
		g_gameConfig.trails[1] = 1;
	}
	if ((groupMask & 0x20) != 0) {
		g_gameConfig.specular[1] = 1;
		g_gameConfig.mipmap[1] = 10;
	}
	if ((groupMask & 0x40) != 0) {
		g_gameConfig.voicePilotEnabled = 2;
		g_gameConfig.voiceTacticalOfficerEnabled = 2;
		g_gameConfig.voiceCommanderEnabled = 1;
		g_gameConfig.voiceSpecialEnabled = 1;
		g_gameConfig.musicEnabled = 1;
		g_gameConfig.musicVolume = 5;
		g_gameConfig.numberOfSfx = 16;
	}
	if (groupMask & 0x80) {
		g_gameConfig.ffEnabled = (uint8_t)ForceFeedback_CheckDevice();
	}

	g_gameConfig.performance = 2;
	return 1;
}

// FUNCTION: XWA 0x5241B0
int Config_Load(void) {
	XwaFile* stream;
	char line[256];
	char* value;
	int keyword;

	Config_SetInitialDefaults();

	stream = File_Open(AERON_VFS_ROOT_USER, "config.cfg", "r");
	if (stream == NULL) {
		return 0;
	}

	while (File_ReadLine(stream, line, sizeof(line))) {
		value = Config_SplitLine(line);
		if (value == NULL) {
			continue;
		}

		keyword = Config_FindKeyword(line);
		if (keyword != -1) {
			Config_ApplyKeywordValue(keyword, value);
		}
	}

	g_gameConfig.use3dHardware[0] = 1;
	g_gameConfig.use3dHardware[1] = 1;

	return File_Close(stream);
}

// FUNCTION: XWA 0x526D00
int Config_GetSinglePlayerHardware3D(void) { return g_gameConfig.use3dHardware[0]; }

// FUNCTION: XWA 0x526D10
int Config_SetSinglePlayerHardware3D(uint8_t value) {
	g_gameConfig.use3dHardware[0] = value;
	return 1;
}

// FUNCTION: XWA 0x526220
int Config_GetDisplayDriverIndex(int profileIdx) { return g_gameConfig.threedDevice[profileIdx]; }

// FUNCTION: XWA 0x5255B0
int Config_Write(void) {
	XwaFile* stream;
	int i;

	Config_CopyFixedText(g_gameConfig.lastPilotName, 13, g_pilotData.name);

	stream = File_Open(AERON_VFS_ROOT_USER, "config.cfg", "w");
	if (stream == NULL) {
		return 0;
	}

	Config_Printf(stream, "lastpilot %s\n", g_pilotData.name);
	for (i = 0; i < 2; ++i) {
		Config_Printf(stream, "backdrop%d %d\n", i + 1, g_gameConfig.backdrop[i]);
		Config_Printf(stream, "stardensity%d %d\n", i + 1, g_gameConfig.starDensity[i]);
		Config_Printf(stream, "debris%d %d\n", i + 1, g_gameConfig.debris[i]);
		Config_Printf(stream, "locallights%d %d\n", i + 1, g_gameConfig.localLights[i]);
		Config_Printf(stream, "specular%d %d\n", i + 1, g_gameConfig.specular[i]);
		Config_Printf(stream, "diffuse%d %d\n", i + 1, g_gameConfig.diffuse[i]);
		Config_Printf(stream, "dither%d %d\n", i + 1, g_gameConfig.dither[i]);
		Config_Printf(stream, "textureres%d %d\n", i + 1, g_gameConfig.textureRes[i]);
		Config_Printf(stream, "mipmap%d %d\n", i + 1, g_gameConfig.mipmap[i]);
		Config_Printf(stream, "lod%d %d\n", i + 1, g_gameConfig.lod[i]);
		Config_Printf(stream, "yard_lod%d %d\n", i + 1, g_gameConfig.yardLod[i]);
		Config_Printf(stream, "screenres%d %d\n", i + 1, g_gameConfig.screenRes[i]);
		Config_Printf(stream, "bpp%d %d\n", i + 1, g_gameConfig.bpp[i]);
		Config_Printf(stream, "brightness%d %d\n", i + 1, g_gameConfig.brightness[i]);
		Config_Printf(stream, "use_3d_hardware%d %d\n", i + 1, g_gameConfig.use3dHardware[i]);
		Config_Printf(stream, "bilinear%d %d\n", i + 1, g_gameConfig.bilinear[i]);
		Config_Printf(stream, "hit_effects%d %d\n", i + 1, g_gameConfig.hitEffects[i]);
		Config_Printf(stream, "engine_glow%d %d\n", i + 1, g_gameConfig.engineGlow[i]);
		Config_Printf(stream, "lensflare%d %d\n", i + 1, g_gameConfig.lensFlare[i]);
		Config_Printf(stream, "threed_device%d %d\n", i + 1, g_gameConfig.threedDevice[i]);
		Config_Printf(stream, "hud_color%d %d\n", i + 1, g_gameConfig.hudColor[i]);
		Config_Printf(stream, "hardware_mipmap%d %d\n", i + 1, g_gameConfig.hardwareMipmap[i]);
		Config_Printf(stream, "palettized_textures%d %d\n", i + 1, g_gameConfig.palettizedTextures[i]);
		Config_Printf(stream, "debris_density%d %d\n", i + 1, g_gameConfig.debrisDensity[i]);
		Config_Printf(stream, "particle_effects%d %d\n", i + 1, g_gameConfig.particleEffects[i]);
		Config_Printf(stream, "trails%d %d\n", i + 1, g_gameConfig.trails[i]);
		Config_Printf(stream, "explosion_res%d %d\n", i + 1, g_gameConfig.explosionRes[i]);
	}

	Config_Printf(stream, "cur_modem %d\n", g_gameConfig.curModem);
	Config_Printf(stream, "firewall %d\n", g_gameConfig.firewall);
	Config_Printf(stream, "port_number %d\n", g_gameConfig.portNumber);
	Config_Printf(stream, "com_port %d\n", g_gameConfig.comPort);
	Config_Printf(stream, "baud_rate %d\n", g_gameConfig.baudRate);
	Config_Printf(stream, "stop_bits %d\n", g_gameConfig.stopBits);
	Config_Printf(stream, "parity %d\n", g_gameConfig.parity);
	Config_Printf(stream, "flow_control %d\n", g_gameConfig.flowControl);
	Config_Printf(stream, "networktype %d\n", g_gameConfig.networkType);
	Config_Printf(stream, "phonenumber %s\n", g_gameConfig.phoneNumber);
	Config_Printf(stream, "ipaddress %s\n", g_gameConfig.ipAddress);
	Config_Printf(stream, "server_update_rate %d\n", g_gameConfig.serverUpdateRate);
	Config_Printf(stream, "sfx_exterior %d\n", g_gameConfig.sfxExteriorEnabled);
	Config_Printf(stream, "sfx_interior %d\n", g_gameConfig.sfxInteriorEnabled);
	Config_Printf(stream, "sfx_engine %d\n", g_gameConfig.sfxEngineEnabled);
	Config_Printf(stream, "sfx_datapad %d\n", g_gameConfig.sfxDatapadEnabled);
	Config_Printf(stream, "voice_pilot %d\n", g_gameConfig.voicePilotEnabled);
	Config_Printf(stream, "voice_tactical_officer %d\n", g_gameConfig.voiceTacticalOfficerEnabled);
	Config_Printf(stream, "voice_commander %d\n", g_gameConfig.voiceCommanderEnabled);
	Config_Printf(stream, "voice_special %d\n", g_gameConfig.voiceSpecialEnabled);
	Config_Printf(stream, "music %d\n", g_gameConfig.musicEnabled);
	Config_Printf(stream, "sfx_datapad_volume %d\n", g_gameConfig.sfxDatapadVolume);
	Config_Printf(stream, "sfx_exterior_volume %d\n", g_gameConfig.sfxExteriorVolume);
	Config_Printf(stream, "sfx_interior_volume %d\n", g_gameConfig.sfxInteriorVolume);
	Config_Printf(stream, "sfx_engine_volume %d\n", g_gameConfig.sfxEngineVolume);
	Config_Printf(stream, "voice_volume %d\n", g_gameConfig.voiceVolume);
	Config_Printf(stream, "music_volume %d\n", g_gameConfig.musicVolume);
	Config_Printf(stream, "datapad_music_volume %d\n", g_gameConfig.datapadMusicVolume);
	Config_Printf(stream, "datapad_music %d\n", g_gameConfig.datapadMusicEnabled);

	for (i = 0; i < 20; ++i) {
		Config_Printf(stream, "joybutton%d %d\n", i + 1, g_gameConfig.joyButtons[i]);
	}

	Config_Printf(stream, "difficulty %d\n", g_gameConfig.difficulty);
	Config_Printf(stream, "collisions %d\n", g_gameConfig.collisions);
	Config_Printf(stream, "tour_difficulty %d\n", g_gameConfig.tourDifficulty);
	Config_Printf(stream, "tour_collisions %d\n", g_gameConfig.tourCollisions);
	Config_Printf(stream, "craft_jumping %d\n", g_gameConfig.craftJumping);
	Config_Printf(stream, "require_password %d\n", g_gameConfig.requirePassword);
	Config_Printf(stream, "in_progress_join %d\n", g_gameConfig.inProgressJoin);
	Config_Printf(stream, "craft_selection %d\n", g_gameConfig.craftSelection);
	Config_Printf(stream, "locate_players %d\n", g_gameConfig.locatePlayers);
	Config_Printf(stream, "last_team_time_limit %d\n", g_gameConfig.lastTeamTimeLimit);
	Config_Printf(stream, "random_seed %d\n", g_gameConfig.randomSeed);
	Config_Printf(stream, "password %s\n", g_gameConfig.password);
	Config_Printf(stream, "async_flag %d\n", g_gameConfig.asyncFlag);
	Config_Printf(stream, "each_team_own_region %d\n", g_gameConfig.eachTeamOwnRegion);
	Config_Printf(stream, "number_of_teams %d\n", g_gameConfig.numberOfTeams);
	Config_Printf(stream, "environment %d\n", g_gameConfig.environment);
	Config_Printf(stream, "ai_opponents %d\n", g_gameConfig.aiOpponents);
	Config_Printf(stream, "max_points %d\n", g_gameConfig.maxPoints);
	Config_Printf(stream, "initial_distance %d\n", g_gameConfig.initialDistance);
	Config_Printf(stream, "help_on %d\n", g_gameConfig.helpOn);
	Config_Printf(stream, "taunt1 %s\n", g_gameConfig.taunt1);
	Config_Printf(stream, "taunt2 %s\n", g_gameConfig.taunt2);
	Config_Printf(stream, "taunt3 %s\n", g_gameConfig.taunt3);
	Config_Printf(stream, "taunt4 %s\n", g_gameConfig.taunt4);
	Config_Printf(stream, "rudder_enabled %d\n", g_gameConfig.rudderEnabled);
	Config_Printf(stream, "flip_rudder %d\n", g_gameConfig.flipRudder);
	Config_Printf(stream, "ff_strength %d\n", g_gameConfig.ffStrength);
	Config_Printf(stream, "ff_center %d\n", g_gameConfig.ffCenter);
	Config_Printf(stream, "ff_enabled %d\n", g_gameConfig.ffEnabled);
	Config_Printf(stream, "flip_y %d\n", g_gameConfig.flipY);
	Config_Printf(stream, "3d_sound_enabled %d\n", g_gameConfig.sound3dEnabled);
	Config_Printf(stream, "number_of_sfx %d\n", g_gameConfig.numberOfSfx);
	Config_Printf(stream, "goaltype %d\n", g_gameConfig.goalType);

	for (i = 0; i < 8; ++i) {
		Config_Printf(stream, "team_goal%d %d\n", i + 1, g_gameConfig.teamGoals[i]);
	}

	for (i = 0; i < 2; ++i) {
		Config_Printf(stream, "preset_throttle%d %d\n", i + 1, g_gameConfig.presetThrottle[i]);
		Config_Printf(stream, "preset_laser%d %d\n", i + 1, g_gameConfig.presetLaser[i]);
		Config_Printf(stream, "preset_shield%d %d\n", i + 1, g_gameConfig.presetShield[i]);
		Config_Printf(stream, "preset_beam%d %d\n", i + 1, g_gameConfig.presetBeam[i]);
	}

	Config_Printf(stream, "invulnerable %d\n", g_gameConfig.invulnerable);
	Config_Printf(stream, "unlimited_ammo %d\n", g_gameConfig.unlimitedAmmo);
	Config_Printf(stream, "tour_invulnerable %d\n", g_gameConfig.tourInvulnerable);
	Config_Printf(stream, "tour_unlimited_ammo %d\n", g_gameConfig.tourUnlimitedAmmo);
	Config_Printf(stream, "sfx_quality %d\n", g_gameConfig.sfxQuality);
	Config_Printf(stream, "laps %d\n", g_gameConfig.laps);
	Config_Printf(stream, "performance %d\n", g_gameConfig.performance);
	Config_Printf(stream, "time_limit %d\n", g_gameConfig.timeLimit);

	return File_Close(stream);
}
