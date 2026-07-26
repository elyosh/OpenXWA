#ifndef XWA_FRONTEND_FRONTEND_TEXT_H
#define XWA_FRONTEND_FRONTEND_TEXT_H

#include <stddef.h>
#include <stdint.h>

#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BitmapFont {
	void* pGlyphBits;
	uint32_t glyphBitOffset[256];
	uint8_t glyphHeight[256];
	uint8_t glyphWidth[256];
	uint32_t pointSize;
	uint8_t inUse;
	uint8_t charSpacing;
	uint8_t field_60A;
} BitmapFont;

/* Frontend text/glyph globals shared with FrontImage glyph rendering. */
extern int g_textColorCodes[6];
extern BitmapFont g_fontSlots[10];
extern BitmapFont* g_fontBySize[256];
extern int g_glyphGradientBg;
extern int g_glyphGradientFgCached;
extern int g_glyphGradientLut[32];
extern int g_glyphScratchTtl;
extern int g_glyphScratchReload;
extern uint16_t g_glyphScratchBuffer[65536];
extern int g_textFieldCursorCharIndex;
extern int g_textFieldLength;
extern int g_activeTextFieldId;
extern int g_textFieldColor;
extern int g_textFieldColorInitialized;

int FrontendText_ResetGlyphScratchBuffer(int frames);
int FrontendText_ResetGlyphScratch(void);
int FrontendText_GetGlyphGradientBg(void);
int FrontendText_SetGlyphGradientBg(int color);
int FrontendText_PushGlyphGradientBg(int newBgColor);
int FrontendText_PopGlyphGradientBg(void);
int FrontendText_LoadFontAtlasFile(const char* fileName, int slotIndex);
void FrontendText_SaveFontAtlasFile(const char* fileName, BitmapFont* font, size_t glyphBlobSize);
int FrontendText_FreeAllFonts(void);
int FrontendText_LoadFont(int pointSize);
int FrontendText_GetFontHeight(int fontSize);
int FrontendText_MeasureWidth(const char* str, int fontSize);
int FrontendText_Draw(int fontSize, const char* str, int x, int y, int color);
int FrontendText_DrawReveal(unsigned int fontSize, const char* str, int x, int y, int color, int revealCount);
int FrontendText_DrawRightAligned(int fontSize, const char* str, int xRight, int y, int color);
int FrontendText_DrawCentered(int fontSize, const char* str, FrontendRect* rect, int color);
int FrontendText_DrawCenteredReveal(int fontSize, const char* str, FrontendRect* rect, int color,
									int revealRadius);
int FrontendText_DrawAlignedInRect(int fontSize, const char* str, FrontendRect* rect, int centerH,
								   int centerV, int color);
int FrontendText_DrawWrapped(int fontSize, const char* str, FrontendRect* rect, int color, int lineSpacing,
							 int firstVisibleLine, int enableClip, int clipBottomAdjust);
int FrontendText_DrawWrappedClipped(int fontSize, const char* str, FrontendRect* rect, int color,
									int lineSpacing, int firstVisibleLine);
int FrontendText_DrawWrappedClippedEx(int fontSize, const char* str, FrontendRect* rect, int color,
									  int lineSpacing, int firstVisibleLine, int clipBottomAdjust);
void FrontendText_DrawFormattedWrappedText(FrontendRect* rect, const unsigned char* text,
										   short suppressCenteredHeadings);
int FrontendText_DrawEditableField(FrontendRect* rect, char* text, int maxChars, int fieldId,
								   unsigned int fontSize, const char* ignoredChars);
int Frontend_FormatSecondsToClockString(unsigned int seconds);

#ifdef __cplusplus
}
#endif

#endif
