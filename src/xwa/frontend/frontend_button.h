#ifndef XWA_FRONTEND_FRONTEND_BUTTON_H
#define XWA_FRONTEND_FRONTEND_BUTTON_H

#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_buttonOverlayTextEnabled;
extern const char* g_buttonOverlayText;
extern int g_buttonOverlayPressedStyle;
extern unsigned char g_buttonHoverState[256];
extern int g_frontButtonOriginGrayColorInitialized;
extern int g_frontButtonOriginGrayColor;
extern int g_frontButtonRectGrayColorInitialized;
extern int g_frontButtonRectGrayColor;

void FrontendButton_EnableOverlayText(void);
void FrontendButton_DisableOverlayText(void);
void FrontendButton_UsePressedOverlayStyle(void);
int FrontendButton_IsOverlayTextEnabled(void);
int FrontendButton_DrawTextButtonState(FrontendRect* rect, char* text, unsigned int fontSize, int normalColor,
									   int state);
int FrontendButton_DrawOverlayText(FrontendRect* rect, const char* text);
int FrontendButton_DrawSpriteAtOriginWithTooltip(FrontendRect* rect, const char* spriteName,
												 const char* tooltipText, int fontSize, int textColor);
void FrontendButton_DrawSpriteAndTooltip(FrontendRect* rect, const char* spriteName, const char* tooltipText,
										 int fontSize, int textColor);
int FrontendButton_DrawCenteredTintedSpriteWithTooltip(FrontendRect* rect, const char* spriteName,
													   const char* tooltipText, unsigned int tintColor);
int FrontendButton_HandleTextButton(FrontendRect* rect, char* text, unsigned int fontSize, int normalColor,
									int hoverSlot, char* clickSoundName);
int FrontendButton_DrawMenuButton(int x, int y, const char* str, unsigned int fontSize, int color,
								  int buttonId, int rightAlign, char* soundName);
int FrontendButton_DrawSimpleSpriteHitTest(FrontendRect* rect, const char* normalSprite,
										   const char* pressedSprite, const char* tooltipText, int fontSize,
										   int textColor, int hoverSlot, const char* hoverSound);
int FrontendButton_DrawSpriteHitTest(FrontendRect* rect, const char* normalSprite, const char* pressedSprite,
									 const char* tooltipText, int fontSize, int textColor, int hoverSlot,
									 const char* hoverSound);
int FrontendButton_DrawSpriteWithHoverText(FrontendRect* buttonRect, char* upSpriteName, char* downSpriteName,
										   void* hoverText, unsigned int normalTint, unsigned int pressedTint,
										   int hoverStateIndex, char* soundName);
int FrontendFrame_DrawSpriteBorder(const FrontendRect* rect);

#ifdef __cplusplus
}
#endif

#endif
