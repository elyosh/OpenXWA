#include "xwa/frontend/frontend_dialog.h"

#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/frontend/net_transport.h"

#include <stdio.h>
#include <string.h>

// GLOBAL: XWA 0x7835D0
int g_frontDialogSavedMouseX;
// GLOBAL: XWA 0x7835D4
int g_frontDialogSavedMouseY;
// GLOBAL: XWA 0x7835D8
int g_frontDialogEditMaxChars;
// GLOBAL: XWA 0x7835E0
char g_frontDialogCancelLabel[128];
// GLOBAL: XWA 0x783660
int g_frontDialogPanelColor;
// GLOBAL: XWA 0x783450
char g_frontDialogOkayLabel[128];
// GLOBAL: XWA 0x7834D0
char g_frontDialogText2OrEdit[256];
// GLOBAL: XWA 0x783668
char g_frontDialogText0[256];
// GLOBAL: XWA 0x783768
char g_frontDialogText1[256];
// GLOBAL: XWA 0x9F4B44
int g_dialogResult;

#ifdef XWA_MODERN
static int g_frontDialogPromptActive;
static int g_frontDialogPromptResultReady;
static int g_frontDialogPromptOverlayTextWasEnabled;
static int g_frontDialogConfirmActive;
static int g_frontDialogConfirmResultReady;
static int g_frontDialogConfirmCursorWasVisible;
static int g_frontDialogConfirmOverlayTextWasEnabled;
#endif
static int g_frontDialogEditActive;
static int g_frontDialogEditSavedGlyphGradientBg;
static int g_frontDialogEditOverlayTextWasEnabled;
static char* g_frontDialogEditText;
static unsigned int g_frontDialogEditTextMaxChars;
static int g_frontDialogDeathStarTourActive;
static int g_frontDialogDeathStarTourCursorWasVisible;
static int g_frontDialogDeathStarTourOverlayTextWasEnabled;

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5599D0
int FrontendDialog_HasNetworkDismissPacket(void) {
	if (Net_HasQueuedPacketTypeOrBacklog(69)) {
		return 1;
	}
	if (Net_HasQueuedPacketTypeOrBacklog(70)) {
		return 1;
	}
	if (Net_HasQueuedPacketTypeOrBacklog(71)) {
		return 1;
	}
	if (Net_HasQueuedPacketTypeOrBacklog(83)) {
		return 1;
	}
	if (Net_HasQueuedPacketTypeOrBacklog(91)) {
		return 1;
	}
	if (Net_HasQueuedPacketTypeOrBacklog(92)) {
		return 1;
	}
	if (Net_HasQueuedPacketTypeOrBacklog(102)) {
		return 1;
	}
	if (Net_HasQueuedPacketTypeOrBacklog(74)) {
		return 1;
	}
	return Net_HasQueuedPacketTypeOrBacklog(73) != 0;
}

// FUNCTION: XWA 0x5595A0
int FrontendDialog_ConfirmUpdateCallback(int frameState) {
	FrontendRect out;
	int done;
	int pressed;

	done = 0;
	if (!frameState) {
		Keyboard_FlushCharBuffer();
		if (g_frontDialogOkayLabel[0] || !g_frontDialogCancelLabel[0]) {
			FrontendCursor_SetPos(90, 236);
		} else {
			FrontendCursor_SetPos(526, 236);
		}
		FrontendDisplay_LockOffscreenSurface();
		FrontImage_DrawSprite("dialogbox", 0, 0);
		FrontendDisplay_UnlockOffscreenSurface(1);
		FrontendText_ResetGlyphScratchBuffer(20);
	} else {

		if (FrontendDialog_HasNetworkDismissPacket()) {
			g_dialogResult = 0;
			done = 1;
		}

		FrontendDraw_RectAssign(&out, 102, 206, 538, 287);
		if (!g_frontDialogText0[0]) {
			if (!g_frontDialogText1[0]) {
				if (g_frontDialogText2OrEdit[0]) {
					out.top -= 20;
				}
			} else if (g_frontDialogText2OrEdit[0]) {
				out.top -= 10;
			}
		} else if (!g_frontDialogText1[0]) {
			if (!g_frontDialogText2OrEdit[0]) {
				out.top += 20;
			}
		} else if (!g_frontDialogText2OrEdit[0]) {
			out.top += 10;
		}
		out.bottom = out.top + 20;
		FrontendText_DrawCentered(15, g_frontDialogText0, &out, 0xffff);
		FrontendDraw_RectOffsetXY(&out, 0, 20);
		FrontendText_DrawCentered(15, g_frontDialogText1, &out, 0xffff);
		FrontendDraw_RectOffsetXY(&out, 0, 20);
		FrontendText_DrawCentered(15, g_frontDialogText2OrEdit, &out, 0xffff);

		if (!g_frontDialogOkayLabel[0] && !g_frontDialogCancelLabel[0]) {
			FrontImage_GetResourceRect("yesbutton", &out);
			FrontendDraw_RectOffsetXY(&out, 62, 236 - ((out.bottom - out.top + 1) >> 1));
			pressed = FrontendButton_DrawSpriteHitTest(
				&out, "yesbutton", "yesbutton", FrontendString_Get(STR_OKAY), 12, 0xffff, 20, "buttonsound");
			if (Keyboard_DequeueChar() == '\r') {
				pressed = 1;
			}
			if (pressed) {
				g_dialogResult = 1;
				done = 1;
			}
		} else if (!g_frontDialogOkayLabel[0]) {
			FrontImage_GetResourceRect("nobutton", &out);
			FrontendDraw_RectOffsetXY(&out, 528, 236 - ((out.bottom - out.top + 1) >> 1));
			pressed = FrontendButton_DrawSpriteHitTest(&out, "nobutton", "nobutton", g_frontDialogCancelLabel,
													   12, 0xffff, 21, "buttonsound");
			if (Keyboard_DequeueChar() == 27) {
				pressed = 1;
			}
			if (pressed) {
				g_dialogResult = 0;
				done = 1;
			}
		} else if (!g_frontDialogCancelLabel[0]) {
			FrontImage_GetResourceRect("yesbutton", &out);
			FrontendDraw_RectOffsetXY(&out, 62, 236 - ((out.bottom - out.top + 1) >> 1));
			pressed = FrontendButton_DrawSpriteHitTest(&out, "yesbutton", "yesbutton", g_frontDialogOkayLabel,
													   12, 0xffff, 20, "buttonsound");
			if (Keyboard_DequeueChar() == '\r') {
				pressed = 1;
			}
			if (pressed) {
				g_dialogResult = 1;
				done = 1;
			}
		} else {
			FrontImage_GetResourceRect("yesbutton", &out);
			FrontendDraw_RectOffsetXY(&out, 62, 236 - ((out.bottom - out.top + 1) >> 1));
			pressed = FrontendButton_DrawSpriteHitTest(&out, "yesbutton", "yesbutton", g_frontDialogOkayLabel,
													   12, 0xffff, 20, "buttonsound");
			if (Keyboard_PeekChar() == '\r') {
				Keyboard_DequeueChar();
				pressed = 1;
			}
			if (pressed) {
				g_dialogResult = 1;
				done = 1;
			}

			FrontImage_GetResourceRect("nobutton", &out);
			FrontendDraw_RectOffsetXY(&out, 528, 236 - ((out.bottom - out.top + 1) >> 1));
			pressed = FrontendButton_DrawSpriteHitTest(&out, "nobutton", "nobutton", g_frontDialogCancelLabel,
													   12, 0xffff, 21, "buttonsound");
			if (Keyboard_DequeueChar() == 27) {
				Keyboard_DiscardChar();
				pressed = 1;
			}
			if (pressed) {
				g_dialogResult = 0;
				done = 1;
			}
		}
	}
	if (done) {
		return 1;
	}
	if (frameState) {
		return 0;
	}
	return 0;
}

// FUNCTION: XWA 0x559B50
int FrontendDialog_CreatePilotNameCallback(int frameState) {
	FrontendRect rect;
	const char* string;
	char* buttonText;
	int accepted;

	if (!frameState) {
		FrontendCursor_SetPos(417, 291);
		Keyboard_FlushCharBuffer();
		FrontImage_RegisterResourceDefault("frontres\\concourse\\create.bmp", "backname");
		FrontendDisplay_LockOffscreenSurface();
		FrontImage_DrawSpriteOpaque("backname", 0, 0);
		FrontImage_DrawSpriteTranslucent("createoverlay", 0, 0);
		FrontendDisplay_UnlockOffscreenSurface(1);
	}

	FrontendDraw_RectAssign(&rect, 245, 225, 445, 245);
	string = FrontendString_Get(STR_CREATE_A_NEW_PILOT);
	FrontendText_DrawCentered(15, string, &rect, 0xffff);

	FrontendDraw_RectAssign(&rect, 250, 245, 440, 265);
	accepted = FrontendText_DrawEditableField(&rect, g_frontDialogText0, 13, 0, 12, "\\*$");

	FrontendDraw_RectAssign(&rect, 250, 275, 440, 295);
	buttonText = (char*)FrontendString_Get(STR_CREATE_PILOT);
	accepted |= FrontendButton_HandleTextButton(&rect, buttonText, 15, 0xffff, 20, "buttonsound");

	if (Keyboard_PeekChar() == '\r') {
		Keyboard_DiscardChar();
		accepted = 1;
	} else if (Keyboard_PeekChar() == 27) {
		Keyboard_DiscardChar();
#ifdef XWA_MODERN
		g_frontDialogText0[0] = '\0';
		FrontImage_FreeResourceByName("backname");
		return 1;
#endif
	}

	if (!accepted || !g_frontDialogText0[0]) {
		return 0;
	}

	FrontImage_FreeResourceByName("backname");
	return 1;
}

#ifdef XWA_MODERN
static void FrontendDialog_FinishPilotNamePrompt(void) { g_frontDialogPromptResultReady = 1; }

static void FrontendDialog_FinishConfirmDialog(void) {
	if (!g_frontDialogConfirmActive) {
		return;
	}

	FrontendCursor_SetPos(g_frontDialogSavedMouseX, g_frontDialogSavedMouseY);
	FrontendText_ResetGlyphScratch();
	if (!g_frontDialogConfirmCursorWasVisible) {
		FrontendCursor_Hide();
	}
	if (g_frontDialogConfirmOverlayTextWasEnabled) {
		FrontendButton_EnableOverlayText();
	}
	FrontendText_PopGlyphGradientBg();
	g_frontDialogConfirmActive = 0;
	g_frontDialogConfirmResultReady = 1;
}

int FrontendDialog_BeginConfirmDialog(const char* line1, const char* line2, const char* line3,
									  const char* okayLabel, const char* cancelLabel) {
	FrontendRect rc;

	if (g_frontDialogConfirmActive) {
		return 0;
	}

	/* An explicit new prompt supersedes an unclaimed result from a caller that
	   did not retain its original blocking call across frames. */
	g_frontDialogConfirmResultReady = 0;
	g_dialogResult = 0;
	FrontendText_PushGlyphGradientBg(FrontendDisplay_PackRGB(0x10, 0x10, 0x40));
	g_frontDialogConfirmOverlayTextWasEnabled = FrontendButton_IsOverlayTextEnabled();
	g_frontDialogConfirmCursorWasVisible = FrontendCursor_IsVisible();
	FrontendButton_DisableOverlayText();
	FrontendCursor_Show();
	if (g_gameConfig.sfxDatapadEnabled) {
		FrontendSound_PlayUISound("warningsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
	}

	FrontendDraw_RectAssign(&rc, 0, 0, 640, 480);
	FrontendCursor_GetPos(&g_frontDialogSavedMouseX, &g_frontDialogSavedMouseY);
	if (line1) {
		strcpy(g_frontDialogText0, line1);
	} else {
		g_frontDialogText0[0] = '\0';
	}
	if (line2) {
		strcpy(g_frontDialogText1, line2);
	} else {
		g_frontDialogText1[0] = '\0';
	}
	if (line3) {
		strcpy(g_frontDialogText2OrEdit, line3);
	} else {
		g_frontDialogText2OrEdit[0] = '\0';
	}
	if (okayLabel) {
		strcpy(g_frontDialogOkayLabel, okayLabel);
	} else {
		g_frontDialogOkayLabel[0] = '\0';
	}
	if (cancelLabel) {
		strcpy(g_frontDialogCancelLabel, cancelLabel);
	} else {
		g_frontDialogCancelLabel[0] = '\0';
	}

	if (!FrontendScreen_BeginModalWithCleanup(FrontendDialog_ConfirmUpdateCallback, &rc,
											  FrontendDialog_FinishConfirmDialog)) {
		if (!g_frontDialogConfirmCursorWasVisible) {
			FrontendCursor_Hide();
		}
		if (g_frontDialogConfirmOverlayTextWasEnabled) {
			FrontendButton_EnableOverlayText();
		}
		FrontendText_PopGlyphGradientBg();
		return 0;
	}

	g_frontDialogConfirmActive = 1;
	return 1;
}

int FrontendDialog_TakeConfirmDialogResult(int* result) {
	if (!g_frontDialogConfirmResultReady) {
		return 0;
	}

	g_frontDialogConfirmResultReady = 0;
	if (result) {
		*result = g_dialogResult;
	}
	return 1;
}
#endif

// FUNCTION: XWA 0x5593C0
int FrontendDialog_ShowConfirmDialog(const char* line1, const char* line2, const char* line3,
									 const char* okayLabel, const char* cancelLabel) {
#ifdef XWA_MODERN
	int result;

	if (FrontendDialog_TakeConfirmDialogResult(&result)) {
		return result;
	}
	FrontendDialog_BeginConfirmDialog(line1, line2, line3, okayLabel, cancelLabel);
	return 0;
#else
	int cursorWasVisible;
	int overlayTextWasEnabled;
	FrontendRect rc;

	FrontendText_PushGlyphGradientBg(FrontendDisplay_PackRGB(0x10, 0x10, 0x40));
	overlayTextWasEnabled = FrontendButton_IsOverlayTextEnabled();
	cursorWasVisible = FrontendCursor_IsVisible();
	FrontendButton_DisableOverlayText();
	FrontendCursor_Show();
	if (g_gameConfig.sfxDatapadEnabled) {
		FrontendSound_PlayUISound("warningsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
	}

	FrontendDraw_RectAssign(&rc, 0, 0, 640, 480);
	FrontendCursor_GetPos(&g_frontDialogSavedMouseX, &g_frontDialogSavedMouseY);
	if (line1) {
		strcpy(g_frontDialogText0, line1);
	} else {
		g_frontDialogText0[0] = '\0';
	}
	if (line2) {
		strcpy(g_frontDialogText1, line2);
	} else {
		g_frontDialogText1[0] = '\0';
	}
	if (line3) {
		strcpy(g_frontDialogText2OrEdit, line3);
	} else {
		g_frontDialogText2OrEdit[0] = '\0';
	}
	if (okayLabel) {
		strcpy(g_frontDialogOkayLabel, okayLabel);
	} else {
		g_frontDialogOkayLabel[0] = '\0';
	}
	if (cancelLabel) {
		strcpy(g_frontDialogCancelLabel, cancelLabel);
	} else {
		g_frontDialogCancelLabel[0] = '\0';
	}

	FrontendScreen_RunModal(FrontendDialog_ConfirmUpdateCallback, &rc);
	FrontendCursor_SetPos(g_frontDialogSavedMouseX, g_frontDialogSavedMouseY);
	FrontendText_ResetGlyphScratch();
	if (!cursorWasVisible) {
		FrontendCursor_Hide();
	}
	if (overlayTextWasEnabled) {
		FrontendButton_EnableOverlayText();
	}
	FrontendText_PopGlyphGradientBg();
	return g_dialogResult;
#endif
}

// FUNCTION: XWA 0x559A90
int FrontendDialog_PromptForPilotName(char* outName) {
#ifdef XWA_MODERN
	FrontendRect rect;

	if (!g_frontDialogPromptActive) {
		FrontendText_PushGlyphGradientBg(FrontendDisplay_PackRGB(0x10, 0x10, 0x20));
		g_frontDialogPromptOverlayTextWasEnabled = FrontendButton_IsOverlayTextEnabled();
		FrontendButton_DisableOverlayText();
		FrontendDraw_RectAssign(&rect, 0, 0, 640, 480);
		FrontendCursor_GetPos(&g_frontDialogSavedMouseX, &g_frontDialogSavedMouseY);
		memset(g_frontDialogText0, 0, sizeof(g_frontDialogText0));
		g_frontDialogPromptResultReady = 0;
		if (!FrontendScreen_BeginModalWithCleanup(FrontendDialog_CreatePilotNameCallback, &rect,
												  FrontendDialog_FinishPilotNamePrompt)) {
			if (g_frontDialogPromptOverlayTextWasEnabled) {
				FrontendButton_EnableOverlayText();
			}
			FrontendText_PopGlyphGradientBg();
			if (outName != 0) {
				outName[0] = '\0';
			}
			return 0;
		}

		g_frontDialogPromptActive = 1;
	}

	if (!g_frontDialogPromptResultReady) {
		if (outName != 0) {
			outName[0] = '\0';
		}
		return 0;
	}
	g_frontDialogPromptResultReady = 0;

	FrontendText_ResetGlyphScratch();
	if (g_frontDialogPromptOverlayTextWasEnabled) {
		FrontendButton_EnableOverlayText();
	}

	if (outName != 0) {
		memcpy(outName, g_frontDialogText0, 12);
		outName[12] = '\0';
	}
	FrontendText_PopGlyphGradientBg();
	g_frontDialogPromptActive = 0;
	return 1;
#else
	FrontendRect rect;
	int overlayTextWasEnabled;

	FrontendText_PushGlyphGradientBg(FrontendDisplay_PackRGB(0x10, 0x10, 0x20));
	overlayTextWasEnabled = FrontendButton_IsOverlayTextEnabled();
	FrontendButton_DisableOverlayText();
	FrontendDraw_RectAssign(&rect, 0, 0, 640, 480);
	FrontendCursor_GetPos(&g_frontDialogSavedMouseX, &g_frontDialogSavedMouseY);
	memset(g_frontDialogText0, 0, sizeof(g_frontDialogText0));
	FrontendScreen_RunModal(FrontendDialog_CreatePilotNameCallback, &rect);
	FrontendText_ResetGlyphScratch();
	if (overlayTextWasEnabled) {
		FrontendButton_EnableOverlayText();
	}
	memcpy(outName, g_frontDialogText0, 12);
	outName[12] = '\0';
	FrontendText_PopGlyphGradientBg();
	return 1;
#endif
}

// FUNCTION: XWA 0x559CE0
int FrontendDialog_EditText(char* text, unsigned int maxChars, const char* promptText) {
	FrontendScreenModalStatus modalStatus;
	FrontendRect rect;

	if (g_frontDialogEditActive) {
		modalStatus = FrontendScreen_GetModalStatus();
		if (modalStatus != FRONTEND_SCREEN_MODAL_INACTIVE && modalStatus != FRONTEND_SCREEN_MODAL_DONE) {
			return 0;
		}

		FrontendText_ResetGlyphScratch();
		if (g_frontDialogEditOverlayTextWasEnabled) {
			FrontendButton_EnableOverlayText();
		}
		memcpy(g_frontDialogEditText, g_frontDialogText0, g_frontDialogEditTextMaxChars);
		g_frontDialogEditText[g_frontDialogEditTextMaxChars] = '\0';
		FrontendText_SetGlyphGradientBg(g_frontDialogEditSavedGlyphGradientBg);
		g_frontDialogEditActive = 0;
		return 1;
	}

	g_frontDialogEditSavedGlyphGradientBg = FrontendText_GetGlyphGradientBg();
	FrontendText_SetGlyphGradientBg(FrontendDisplay_PackRGB(0x10, 0x10, 0x20));
	g_frontDialogEditOverlayTextWasEnabled = FrontendButton_IsOverlayTextEnabled();
	FrontendButton_DisableOverlayText();
	FrontendDraw_RectAssign(&rect, 0, 0, 640, 480);
	FrontendCursor_GetPos(&g_frontDialogSavedMouseX, &g_frontDialogSavedMouseY);
	memset(g_frontDialogText0, 0, sizeof(g_frontDialogText0));
	strcpy(g_frontDialogText0, text);
	g_frontDialogEditMaxChars = (int)maxChars;
	if (promptText) {
		strcpy(g_frontDialogText1, promptText);
	} else {
		memset(g_frontDialogText1, 0, sizeof(g_frontDialogText1));
	}

	g_frontDialogEditText = text;
	g_frontDialogEditTextMaxChars = maxChars;
	if (!FrontendScreen_BeginModal(FrontendDialog_EditTextCallback, &rect)) {
		if (g_frontDialogEditOverlayTextWasEnabled) {
			FrontendButton_EnableOverlayText();
		}
		FrontendText_SetGlyphGradientBg(g_frontDialogEditSavedGlyphGradientBg);
		return 0;
	}

	g_frontDialogEditActive = 1;
	return 0;
}

// FUNCTION: XWA 0x559E10
int FrontendDialog_EditTextCallback(int frameState) {
	FrontendRect rect;
	int savedGlyphGradientBg;
	int accepted;

	if (!frameState) {
		g_frontDialogPanelColor = FrontendDisplay_PackRGB(0x20, 0x20, 0x40);
		Keyboard_FlushCharBuffer();
		g_activeTextFieldId = 0;
		FrontendDraw_RectAssign(&rect, 160, 200, 480, 275);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_frontDialogPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_frontDialogPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_frontDialogPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_frontDialogPanelColor);
		FrontendFrame_DrawSpriteBorder(&rect);
		FrontendDraw_RectAssign(&rect, 160, 210, 480, 230);
		FrontendText_DrawCentered(15, g_frontDialogText1, &rect, g_colorLightBlue);
		FrontendDisplay_LockOffscreenSurface();
		FrontendDraw_RectAssign(&rect, 160, 200, 480, 275);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_frontDialogPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_frontDialogPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_frontDialogPanelColor);
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_frontDialogPanelColor);
		FrontendFrame_DrawSpriteBorder(&rect);
		FrontendDraw_RectAssign(&rect, 160, 210, 480, 230);
		FrontendText_DrawCentered(15, g_frontDialogText1, &rect, g_colorLightBlue);
		FrontendDisplay_UnlockOffscreenSurface(1);
	}

	FrontendDraw_RectAssign(&rect, 170, 240, 470, 265);
	savedGlyphGradientBg = FrontendText_GetGlyphGradientBg();
	FrontendText_SetGlyphGradientBg(g_colorNearBlack);
	FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_colorSlateBlue);
	accepted =
		FrontendText_DrawEditableField(&rect, g_frontDialogText0, g_frontDialogEditMaxChars, 0, 12, NULL);
	FrontendText_SetGlyphGradientBg(savedGlyphGradientBg);
	if (Keyboard_PeekChar() == '\r' || Keyboard_PeekChar() == 27) {
		Keyboard_DiscardChar();
		accepted = 1;
	}

	return accepted != 0;
}

// FUNCTION: XWA 0x55AF90
int FrontendDialog_DeathStarTourOutcomeCallback(int frameState) {
	FrontendRect rect;
	int cursorX;
	int cursorY;
	int animFrame;
	int settingFrame;
	int completedCount;
	int completedMissionIdx;
	int done;
	int textLen;
	int textY;
	int textColor;
	int textClicked;
	int charIdx;
	char glyph[2];
	int textWidth;

	done = 0;
	if (!frameState) {
		Keyboard_FlushCharBuffer();
		FrontendCursor_SetPos(90, 236);
		FrontendDisplay_LockOffscreenSurface();
		FrontImage_DrawSpriteOpaque("deathback", 0, 0);
		FrontendDisplay_UnlockOffscreenSurface(1);
		FrontendText_ResetGlyphScratchBuffer(20);
	} else {
		FrontendCursor_GetPos(&cursorX, &cursorY);
		settingFrame = (frameState % 32) >> 2;
		if (settingFrame < 5) {
			FrontImage_SetSpriteFrame("lsettingleftu", settingFrame);
			FrontImage_SetSpriteFrame("lsettingrightu", settingFrame);
		} else {
			animFrame = 8 - settingFrame;
			FrontImage_SetSpriteFrame("lsettingleftu", animFrame);
			FrontImage_SetSpriteFrame("lsettingrightu", animFrame);
		}

		completedCount = g_selectedMissionListIndex - 48;
		FrontendDraw_RectAssign(&rect, 65, 80, 575, 100);
		FrontendText_DrawCentered(20, g_missionList[g_selectedMissionListIndex + 1].description, &rect,
								  g_colorRed);

		FrontendDraw_RectAssign(&rect, 65, 380, 575, 400);
		for (completedMissionIdx = 0; completedMissionIdx < completedCount; ++completedMissionIdx) {
			sprintf(g_frontDialogText0, "%s - %s", g_missionList[completedMissionIdx + 49].description,
					FrontendString_Get(STR_DS_COMPLETE));
			FrontendText_DrawCentered(15, g_frontDialogText0, &rect, g_colorGreen);
			FrontendDraw_RectOffsetXY(&rect, 0, 20);
		}

		strcpy(g_frontDialogText0, FrontendString_Get(STR_CONTINUE));
		textLen = (int)strlen(g_frontDialogText0);
		textY = 240 - ((20 * textLen) >> 1);
		FrontendDraw_RectAssign(&rect, 540, textY, 560, textY + 20 * textLen);
		textClicked = 0;
		if (FrontendDraw_PointInRect(&rect, cursorX, cursorY)) {
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				FrontendSound_PlayUISound("flysound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				textColor = g_colorRed;
				textClicked = 1;
			} else {
				textColor = g_colorYellow;
			}
		} else {
			textColor = g_colorPaleBlue;
		}
		glyph[1] = '\0';
		for (charIdx = 0; charIdx < textLen; ++charIdx) {
			glyph[0] = g_frontDialogText0[charIdx];
			FrontendText_Draw(20, glyph, 540, textY, textColor);
			textY += 20;
		}

		FrontImage_GetResourceRect("lsettingrightu", &rect);
		FrontendDraw_RectOffsetXY(&rect, 570, 240 - ((rect.bottom - rect.top + 1) >> 1));
		textClicked |= FrontendButton_DrawSpriteHitTest(&rect, "lsettingrightu", "lsettingrightd", NULL, 10,
														g_colorLightBlue, 20, "settingsound");
		if (textClicked) {
			g_dialogResult = 1;
			done = 1;
		}

		strcpy(g_frontDialogText0, FrontendString_Get(STR_DIALOG_CONCOURSE));
		textLen = (int)strlen(g_frontDialogText0);
		textY = 240 - ((20 * textLen) >> 1);
		FrontendDraw_RectAssign(&rect, 80, textY, 100, textY + 20 * textLen);
		textClicked = 0;
		if (FrontendDraw_PointInRect(&rect, cursorX, cursorY)) {
			if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
				FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				textColor = g_colorRed;
				textClicked = 1;
			} else {
				textColor = g_colorYellow;
			}
		} else {
			textColor = g_colorPaleBlue;
		}
		glyph[1] = '\0';
		for (charIdx = 0; charIdx < textLen; ++charIdx) {
			glyph[0] = g_frontDialogText0[charIdx];
			FrontendText_Draw(20, glyph, 80, textY, textColor);
			textY += 20;
		}

		FrontImage_GetResourceRect("lsettingleftu", &rect);
		FrontendDraw_RectOffsetXY(&rect, rect.left - rect.right + 69,
								  240 - ((rect.bottom - rect.top + 1) >> 1));
		textClicked |= FrontendButton_DrawSpriteHitTest(&rect, "lsettingleftu", "lsettingleftd", NULL, 10,
														g_colorLightBlue, 21, "settingsound");
		if (textClicked) {
			g_dialogResult = 0;
			done = 1;
		}

		textWidth = FrontendText_MeasureWidth(FrontendString_Get(STR_REFLY), 15);
		if (FrontendButton_DrawMenuButton(320 - (textWidth >> 1), 460, FrontendString_Get(STR_REFLY), 15,
										  g_colorPaleBlue, 22, 0, "settingsound")) {
			g_dialogResult = 2;
			done = 1;
		}
	}

	return done;
}

// FUNCTION: XWA 0x55AEB0
int FrontendDialog_ShowDeathStarTourOutcome(void) {
	FrontendRect screenRect;
	int overlayTextWasEnabled;
	int cursorWasVisible;

#ifdef XWA_MODERN
	FrontendScreenModalStatus modalStatus;

	if (g_frontDialogDeathStarTourActive) {
		modalStatus = FrontendScreen_GetModalStatus();
		if (modalStatus != FRONTEND_SCREEN_MODAL_INACTIVE && modalStatus != FRONTEND_SCREEN_MODAL_DONE) {
			return -1;
		}

		FrontImage_FreeResourceByName("deathback");
		FrontendText_ResetGlyphScratch();
		if (!g_frontDialogDeathStarTourCursorWasVisible) {
			FrontendCursor_Hide();
		}
		if (g_frontDialogDeathStarTourOverlayTextWasEnabled) {
			FrontendButton_EnableOverlayText();
		}
		FrontendText_PopGlyphGradientBg();
		g_frontDialogDeathStarTourActive = 0;
		return g_dialogResult;
	}

	FrontendText_PushGlyphGradientBg(FrontendDisplay_PackRGB(0x40, 0x40, 0x40));
	g_frontDialogDeathStarTourOverlayTextWasEnabled = FrontendButton_IsOverlayTextEnabled();
	g_frontDialogDeathStarTourCursorWasVisible = FrontendCursor_IsVisible();
	FrontendButton_DisableOverlayText();
	FrontendCursor_Show();
	FrontendDraw_RectAssign(&screenRect, 0, 0, 640, 480);
	FrontendCursor_GetPos(&g_frontDialogSavedMouseX, &g_frontDialogSavedMouseY);

	sprintf(g_frontDialogText0, "frontres\\tour\\deathstar%d.bmp", g_selectedMissionListIndex - 48);
	FrontImage_RegisterResourceDefault(g_frontDialogText0, "deathback");
	if (!FrontendScreen_BeginModal(FrontendDialog_DeathStarTourOutcomeCallback, &screenRect)) {
		FrontImage_FreeResourceByName("deathback");
		FrontendText_ResetGlyphScratch();
		if (!g_frontDialogDeathStarTourCursorWasVisible) {
			FrontendCursor_Hide();
		}
		if (g_frontDialogDeathStarTourOverlayTextWasEnabled) {
			FrontendButton_EnableOverlayText();
		}
		FrontendText_PopGlyphGradientBg();
		return 0;
	}

	g_frontDialogDeathStarTourActive = 1;
	return -1;
#else
	FrontendText_PushGlyphGradientBg(FrontendDisplay_PackRGB(0x40, 0x40, 0x40));
	overlayTextWasEnabled = FrontendButton_IsOverlayTextEnabled();
	cursorWasVisible = FrontendCursor_IsVisible();
	FrontendButton_DisableOverlayText();
	FrontendCursor_Show();
	FrontendDraw_RectAssign(&screenRect, 0, 0, 640, 480);
	FrontendCursor_GetPos(&g_frontDialogSavedMouseX, &g_frontDialogSavedMouseY);

	sprintf(g_frontDialogText0, "frontres\\tour\\deathstar%d.bmp", g_selectedMissionListIndex - 48);
	FrontImage_RegisterResourceDefault(g_frontDialogText0, "deathback");
	FrontendScreen_RunModal(FrontendDialog_DeathStarTourOutcomeCallback, &screenRect);
	FrontImage_FreeResourceByName("deathback");
	FrontendText_ResetGlyphScratch();
	if (!cursorWasVisible) {
		FrontendCursor_Hide();
	}
	if (overlayTextWasEnabled) {
		FrontendButton_EnableOverlayText();
	}
	FrontendText_PopGlyphGradientBg();
	return g_dialogResult;
#endif
}
