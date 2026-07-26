#include "xwa/frontend/frontend_button.h"

#include "xwa/frontend/frontend_image.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"

// GLOBAL: XWA 0x7831BC
int g_buttonOverlayTextEnabled;
// GLOBAL: XWA 0x7831C0
const char* g_buttonOverlayText;
// GLOBAL: XWA 0x7832C8
int g_buttonOverlayPressedStyle;
// GLOBAL: XWA 0x7832D0
unsigned char g_buttonHoverState[256];
// GLOBAL: XWA 0x7833F0
int g_frontButtonOriginGrayColorInitialized;
// GLOBAL: XWA 0x7833F4
int g_frontButtonOriginGrayColor;
// GLOBAL: XWA 0x7833F8
int g_frontButtonRectGrayColorInitialized;
// GLOBAL: XWA 0x7833FC
int g_frontButtonRectGrayColor;

// FUNCTION: XWA 0x556AD0
void FrontendButton_EnableOverlayText(void) {
	g_buttonOverlayTextEnabled = 1;
	g_buttonOverlayPressedStyle = 0;
}

// FUNCTION: XWA 0x556AF0
void FrontendButton_DisableOverlayText(void) { g_buttonOverlayTextEnabled = 0; }

void FrontendButton_UsePressedOverlayStyle(void) { g_buttonOverlayPressedStyle = 1; }

// FUNCTION: XWA 0x556B10
int FrontendButton_IsOverlayTextEnabled(void) { return g_buttonOverlayTextEnabled; }

// FUNCTION: XWA 0x5567E0
int FrontendButton_DrawTextButtonState(FrontendRect* rect, char* text, unsigned int fontSize, int normalColor,
									   int state) {
	unsigned int color;

	if ((unsigned char)state) {
		if ((unsigned char)state == 1) {
			color = (unsigned int)g_colorRed;
		} else if ((unsigned char)state == 2) {
			color = (unsigned int)g_colorYellow;
		} else {
			color = (unsigned int)state;
		}
	} else {
		color = (unsigned int)normalColor;
	}

	FrontendDraw_RectOffsetXY(rect, 0, 1);
	return FrontendText_DrawCentered(fontSize, text, rect, (int)color);
}

// FUNCTION: XWA 0x556A30
int FrontendButton_DrawOverlayText(FrontendRect* rect, const char* text) {
	int result;

	if (g_buttonOverlayPressedStyle) {
		FrontendDraw_RectOffsetXY(rect, 2, 2);
		FrontendText_DrawCentered(10, text, rect, g_colorPaleCyan);
		FrontendDraw_RectOffsetXY(rect, -2, -2);
		result = FrontendText_DrawCentered(10, text, rect, g_colorTeal);
	} else {
		FrontendDraw_RectOffsetXY(rect, 2, 2);
		FrontendText_DrawCentered(10, text, rect, g_colorTeal);
		FrontendDraw_RectOffsetXY(rect, -2, -2);
		result = FrontendText_DrawCentered(10, text, rect, g_colorPaleCyan);
	}

	g_buttonOverlayPressedStyle = 0;
	return result;
}

// FUNCTION: XWA 0x556840
int FrontendButton_DrawSpriteAtOriginWithTooltip(FrontendRect* rect, const char* spriteName,
												 const char* tooltipText, int fontSize, int textColor) {
	int result;
	const char* hoverText;
	int outX;
	int outY;

	(void)fontSize;
	(void)textColor;

	FrontImage_DrawSprite(spriteName, 0, 0);
	if (!g_frontButtonOriginGrayColorInitialized) {
		g_frontButtonOriginGrayColorInitialized = 1;
		g_frontButtonOriginGrayColor = FrontendDisplay_PackRGB(0x60, 0x60, 0x60);
	}

	FrontendCursor_GetPos(&outX, &outY);
	result = g_buttonOverlayTextEnabled;
	if (g_buttonOverlayTextEnabled) {
		result = FrontendButton_DrawOverlayText(rect, g_buttonOverlayText);
	}

	hoverText = tooltipText;
	if (tooltipText) {
		result = FrontendDraw_PointInRect(rect, outX, outY);
		if (result) {
			return FrontendCursor_SetLabel(hoverText);
		}
	}

	return result;
}

// FUNCTION: XWA 0x556990
void FrontendButton_DrawSpriteAndTooltip(FrontendRect* rect, const char* spriteName, const char* tooltipText,
										 int fontSize, int textColor) {
	const char* hoverText;
	int outX;
	int outY;

	(void)fontSize;
	(void)textColor;

	FrontImage_DrawSprite(spriteName, rect->left, rect->top);
	if (!g_frontButtonRectGrayColorInitialized) {
		g_frontButtonRectGrayColorInitialized = 1;
		g_frontButtonRectGrayColor = FrontendDisplay_PackRGB(0x60, 0x60, 0x60);
	}

	FrontendCursor_GetPos(&outX, &outY);
	if (g_buttonOverlayTextEnabled) {
		FrontendButton_DrawOverlayText(rect, g_buttonOverlayText);
	}

	hoverText = tooltipText;
	if (tooltipText) {
		if (FrontendDraw_PointInRect(rect, outX, outY)) {
			FrontendCursor_SetLabel(hoverText);
		}
	}
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5568E0
int FrontendButton_DrawCenteredTintedSpriteWithTooltip(FrontendRect* rect, const char* spriteName,
													   const char* tooltipText, unsigned int tintColor) {
	const char* drawName;
	FrontendRect sourceRect;
	int left;
	int top;
	int outX;
	int outY;
	int result;
	const char* hoverText;

	drawName = spriteName;
	FrontImage_GetResourceRect(spriteName, &sourceRect);
	left = rect->left;
	top = rect->top;
	outX = (sourceRect.left + rect->right - sourceRect.right - left) >> 1;
	outY = (sourceRect.top + rect->bottom - sourceRect.bottom - top) >> 1;
	result = FrontImage_DrawSpriteRectTinted(drawName, &sourceRect, left + outX, top + outY, tintColor);
	hoverText = tooltipText;
	if (tooltipText) {
		FrontendCursor_GetPos(&outX, &outY);
		result = FrontendDraw_PointInRect(rect, outX, outY);
		if (result) {
			return FrontendCursor_SetLabel(hoverText);
		}
	}

	return result;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x555FE0
int FrontendButton_HandleTextButton(FrontendRect* rect, char* text, unsigned int fontSize, int normalColor,
									int hoverSlot, char* clickSoundName) {
	int outY;
	int outX;

	FrontendCursor_GetPos(&outX, &outY);
	if (FrontendDraw_PointInRect(rect, outX, outY)) {
		if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
			FrontendButton_DrawTextButtonState(rect, text, fontSize, normalColor, 1);
			if (!g_buttonHoverState[hoverSlot]) {
				if (g_gameConfig.sfxDatapadEnabled) {
					FrontendSound_PlayUISound(clickSoundName, 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
											  63);
				}
			}

			g_buttonHoverState[hoverSlot] = 1;
		} else {
			FrontendButton_DrawTextButtonState(rect, text, fontSize, normalColor, 2);
			g_buttonHoverState[hoverSlot] = 0;
		}

		if (FrontendMouse_GetLeftClick()) {
			return 1;
		}

		if (FrontendMouse_GetRightClick()) {
			return 2;
		}
	} else {
		g_buttonHoverState[hoverSlot] = 0;
		FrontendButton_DrawTextButtonState(rect, text, fontSize, normalColor, 0);
	}

	return 0;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x556100
int FrontendButton_DrawMenuButton(int x, int y, const char* str, unsigned int fontSize, int color,
								  int buttonId, int rightAlign, char* soundName) {
	int textWidth;
	int drawX;
	int outY;
	int outX;
	FrontendRect rect;

	FrontendCursor_GetPos(&outX, &outY);
	textWidth = FrontendText_MeasureWidth(str, fontSize);
	drawX = x;
	if (rightAlign) {
		drawX = x - textWidth;
	}

	FrontendDraw_RectAssign(&rect, drawX - 2, y - 1, textWidth + drawX + 2, (int)fontSize + y + 1);
	if (FrontendDraw_PointInRect(&rect, outX, outY)) {
		if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
			FrontendText_Draw(fontSize, str, drawX, y, g_colorRed);
			if (!g_buttonHoverState[buttonId]) {
				if (g_gameConfig.sfxDatapadEnabled) {
					FrontendSound_PlayUISound(soundName, 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				}
			}

			g_buttonHoverState[buttonId] = 1;
		} else {
			FrontendText_Draw(fontSize, str, drawX, y, g_colorYellow);
			g_buttonHoverState[buttonId] = 0;
		}

		if (FrontendMouse_GetLeftClick()) {
			return 1;
		}

		if (FrontendMouse_GetRightClick()) {
			return 2;
		}
	} else {
		g_buttonHoverState[buttonId] = 0;
		FrontendText_Draw(fontSize, str, drawX, y, color);
	}

	return 0;
}

// FUNCTION: XWA 0x556260
int FrontendButton_DrawSimpleSpriteHitTest(FrontendRect* rect, const char* normalSprite,
										   const char* pressedSprite, const char* tooltipText, int fontSize,
										   int textColor, int hoverSlot, const char* hoverSound) {
	int outY;
	int outX;

	FrontendCursor_GetPos(&outX, &outY);
	if (FrontendDraw_PointInRect(rect, outX, outY)) {
		if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
			FrontendButton_UsePressedOverlayStyle();
			FrontendButton_DrawSpriteAtOriginWithTooltip(rect, pressedSprite, tooltipText, fontSize,
														 textColor);
			return 1;
		} else if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
			FrontendButton_UsePressedOverlayStyle();
			FrontendButton_DrawSpriteAtOriginWithTooltip(rect, pressedSprite, tooltipText, fontSize,
														 textColor);
			if (!g_buttonHoverState[hoverSlot]) {
				if (g_gameConfig.sfxDatapadEnabled) {
					FrontendSound_PlayUISound((char*)hoverSound, 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				}
			}

			g_buttonHoverState[hoverSlot] = 1;
			return 0;
		} else {
			FrontendButton_DrawSpriteAtOriginWithTooltip(rect, normalSprite, tooltipText, fontSize,
														 textColor);
			g_buttonHoverState[hoverSlot] = 0;
			return 0;
		}
	} else {
		g_buttonHoverState[hoverSlot] = 0;
		FrontendButton_DrawSpriteAtOriginWithTooltip(rect, normalSprite, tooltipText, fontSize, textColor);
		return 0;
	}
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x556660
int FrontendButton_DrawSpriteHitTest(FrontendRect* rect, const char* normalSprite, const char* pressedSprite,
									 const char* tooltipText, int fontSize, int textColor, int hoverSlot,
									 const char* hoverSound) {
	int outY;
	int outX;
	FrontendRect clippedRect;

	FrontendCursor_GetPos(&outX, &outY);
	FrontendDraw_RectCopy(&clippedRect, rect);
	FrontendDraw_RectClipToBounds(&clippedRect);
	if (FrontendDraw_PointInRect(&clippedRect, outX, outY)) {
		if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
			FrontendButton_UsePressedOverlayStyle();
			FrontendButton_DrawSpriteAndTooltip(rect, pressedSprite, tooltipText, fontSize, textColor);
			return FrontendMouse_GetLeftClick() ? 1 : 2;
		} else if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
			FrontendButton_UsePressedOverlayStyle();
			FrontendButton_DrawSpriteAndTooltip(rect, pressedSprite, tooltipText, fontSize, textColor);
			if (!g_buttonHoverState[hoverSlot]) {
				if (g_gameConfig.sfxDatapadEnabled) {
					FrontendSound_PlayUISound((char*)hoverSound, 1, 0, 255,
											  12 * g_gameConfig.sfxDatapadVolume, 63);
				}
			}

			g_buttonHoverState[hoverSlot] = 1;
			return 0;
		} else {
			FrontendButton_DrawSpriteAndTooltip(rect, normalSprite, tooltipText, fontSize, textColor);
			g_buttonHoverState[hoverSlot] = 0;
			return 0;
		}
	} else {
		g_buttonHoverState[hoverSlot] = 0;
		FrontendButton_DrawSpriteAndTooltip(rect, normalSprite, tooltipText, fontSize, textColor);
		return 0;
	}
}

// FUNCTION: XWA 0x5563C0
int FrontendButton_DrawSpriteWithHoverText(FrontendRect* buttonRect, char* upSpriteName, char* downSpriteName,
										   void* hoverText, unsigned int normalTint, unsigned int pressedTint,
										   int hoverStateIndex, char* soundName) {
	int outX;
	int outY;
	FrontendRect sourceRect;
	FrontendRect clippedRect;

	FrontendCursor_GetPos(&outX, &outY);
	FrontendDraw_RectCopy(&clippedRect, buttonRect);
	FrontendDraw_RectClipToBounds(&clippedRect);
	if (FrontendDraw_PointInRect(&clippedRect, outX, outY)) {
		if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
			int left;
			int top;
			int centeredX;
			int centeredY;

			FrontendButton_UsePressedOverlayStyle();
			FrontImage_GetResourceRect(downSpriteName, &sourceRect);
			left = buttonRect->left;
			centeredX = sourceRect.left + buttonRect->right - sourceRect.right;
			top = buttonRect->top;
			centeredY = (sourceRect.top + buttonRect->bottom - sourceRect.bottom - top) >> 1;
			outX = (centeredX - left) >> 1;
			FrontImage_DrawSpriteRectTinted(downSpriteName, &sourceRect, left + outX, top + centeredY,
											pressedTint);
			return FrontendMouse_GetLeftClick() ? 1 : 2;
		}

		if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
			int left;
			int top;
			int centeredX;
			int centeredY;

			FrontendButton_UsePressedOverlayStyle();
			FrontImage_GetResourceRect(downSpriteName, &sourceRect);
			left = buttonRect->left;
			centeredX = sourceRect.left + buttonRect->right - sourceRect.right;
			top = buttonRect->top;
			centeredY = (sourceRect.top + buttonRect->bottom - sourceRect.bottom - top) >> 1;
			outX = (centeredX - left) >> 1;
			FrontImage_DrawSpriteRectTinted(downSpriteName, &sourceRect, left + outX, top + centeredY,
											pressedTint);
			if (!g_buttonHoverState[hoverStateIndex]) {
				if (g_gameConfig.sfxDatapadEnabled) {
					FrontendSound_PlayUISound(soundName, 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				}
			}

			g_buttonHoverState[hoverStateIndex] = 1;
		} else {
			int left;
			int top;
			int centeredX;
			int centeredY;

			FrontImage_GetResourceRect(upSpriteName, &sourceRect);
			left = buttonRect->left;
			centeredX = sourceRect.left + buttonRect->right - sourceRect.right;
			top = buttonRect->top;
			centeredY = (sourceRect.top + buttonRect->bottom - sourceRect.bottom - top) >> 1;
			outX = (centeredX - left) >> 1;
			FrontImage_DrawSpriteRectTinted(upSpriteName, &sourceRect, left + outX, top + centeredY,
											(unsigned int)g_colorYellow);
			g_buttonHoverState[hoverStateIndex] = 0;
		}

		if (hoverText) {
			FrontendCursor_SetLabel(hoverText);
			return 0;
		}
	} else {
		int left;
		int top;
		int centeredX;
		int centeredY;

		g_buttonHoverState[hoverStateIndex] = 0;
		FrontImage_GetResourceRect(upSpriteName, &sourceRect);
		left = buttonRect->left;
		centeredX = sourceRect.left + buttonRect->right - sourceRect.right;
		top = buttonRect->top;
		centeredY = (sourceRect.top + buttonRect->bottom - sourceRect.bottom - top) >> 1;
		outX = (centeredX - left) >> 1;
		FrontImage_DrawSpriteRectTinted(upSpriteName, &sourceRect, left + outX, top + centeredY, normalTint);
	}

	return 0;
}

// FUNCTION: XWA 0x55A620
int FrontendFrame_DrawSpriteBorder(const FrontendRect* rect) {
	FrontendRect spriteRect;
	int topCornerHeight;
	int leftWidth;
	int rightXOffset;
	int bottomYOffset;
	int topTiles;
	int sideTiles;
	int x;
	int y;
	int i;

	FrontImage_GetResourceRect("frametlc", &spriteRect);
	topCornerHeight = spriteRect.bottom - spriteRect.top + 1;
	leftWidth = spriteRect.right - spriteRect.left;

	FrontImage_GetResourceRect("frameright", &spriteRect);
	rightXOffset = spriteRect.left - spriteRect.right + leftWidth;
	++leftWidth;

	FrontImage_GetResourceRect("framebrc", &spriteRect);
	bottomYOffset = spriteRect.bottom - spriteRect.top + 1;

	FrontImage_GetResourceRect("framebottom", &spriteRect);
	bottomYOffset = bottomYOffset - spriteRect.bottom + spriteRect.top - 1;

	x = rect->left;
	y = rect->top - topCornerHeight + 2;
	sideTiles = (rect->bottom - rect->top + 1) / 15;
	topTiles = (rect->right - rect->left + 1) / 10;
	for (i = 0; i < topTiles; ++i) {
		FrontImage_DrawSprite("frametop", x, y);
		x += 10;
	}

	FrontImage_DrawSprite("frametlc", rect->left - leftWidth + 2, y);
	FrontImage_DrawSprite("frametrc", rect->right - 1, y);

	x = rect->left - leftWidth;
	y = rect->top;
	if (sideTiles > 0) {
		x += 2;
		i = sideTiles;
		do {
			FrontImage_DrawSprite("frameleft", x, y);
			FrontImage_DrawSprite("frameright", rightXOffset + rect->right - 1, y);
			y += 15;
		} while (--i != 0);
	}

	x = rect->left;
	y = rect->bottom - 1;
	i = topTiles;
	if (i > 0) {
		y += bottomYOffset;
		do {
			FrontImage_DrawSprite("framebottom", x, y);
			x += 10;
		} while (--i != 0);
	}

	y = rect->bottom - 1;
	FrontImage_DrawSprite("frameblc", rect->left - leftWidth + 2, y);
	FrontImage_DrawSprite("framebrc", rect->right - 1, y);
	return 1;
}
