#include "xwa/frontend/frontend_text.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/util/byte_order.h"
#include "xwa/util/memory.h"

#include "aeron/log.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifndef XWA_MODERN
typedef struct FrontendTextSize {
	int cx;
	int cy;
} FrontendTextSize;

typedef struct FrontendTextWinRect {
	int left;
	int top;
	int right;
	int bottom;
} FrontendTextWinRect;

typedef void* FrontendTextHdc;
typedef void* FrontendTextGdiObject;
typedef void* FrontendTextHfont;

typedef HRESULT(XWA_DXAPI* FrontendTextSurfaceGetDcFunc)(IDirectDrawSurface*, FrontendTextHdc*);
typedef HRESULT(XWA_DXAPI* FrontendTextSurfaceReleaseDcFunc)(IDirectDrawSurface*, FrontendTextHdc);

#if defined(_WIN32)
__declspec(dllimport) FrontendTextHfont __stdcall
CreateFontA(int height, int width, int escapement, int orientation, int weight, unsigned int italic,
			unsigned int underline, unsigned int strikeOut, unsigned int charSet,
			unsigned int outputPrecision, unsigned int clipPrecision, unsigned int quality,
			unsigned int pitchAndFamily, const char* faceName);
__declspec(dllimport) int __stdcall DeleteObject(FrontendTextGdiObject object);
__declspec(dllimport) int __stdcall ExtTextOutA(FrontendTextHdc hdc, int x, int y, unsigned int options,
												const FrontendTextWinRect* rect, const char* text,
												unsigned int count, const int* dx);
__declspec(dllimport) FrontendTextHdc __stdcall GetDC(void* hwnd);
__declspec(dllimport) int __stdcall GetTextExtentPoint32A(FrontendTextHdc hdc, const char* text, int count,
														  FrontendTextSize* size);
__declspec(dllimport) int __stdcall ReleaseDC(void* hwnd, FrontendTextHdc hdc);
__declspec(dllimport) FrontendTextGdiObject __stdcall SelectObject(FrontendTextHdc hdc,
																   FrontendTextGdiObject object);
__declspec(dllimport) int __stdcall SetBkColor(FrontendTextHdc hdc, unsigned int color);
__declspec(dllimport) int __stdcall SetBkMode(FrontendTextHdc hdc, int mode);
__declspec(dllimport) int __stdcall SetMapMode(FrontendTextHdc hdc, int mode);
__declspec(dllimport) int __stdcall SetRect(FrontendTextWinRect* rect, int left, int top, int right,
											int bottom);
__declspec(dllimport) int __stdcall SetTextCharacterExtra(FrontendTextHdc hdc, int extra);
__declspec(dllimport) int __stdcall SetTextColor(FrontendTextHdc hdc, unsigned int color);
#else
static FrontendTextHfont CreateFontA(int height, int width, int escapement, int orientation, int weight,
									 unsigned int italic, unsigned int underline, unsigned int strikeOut,
									 unsigned int charSet, unsigned int outputPrecision,
									 unsigned int clipPrecision, unsigned int quality,
									 unsigned int pitchAndFamily, const char* faceName) {
	(void)height;
	(void)width;
	(void)escapement;
	(void)orientation;
	(void)weight;
	(void)italic;
	(void)underline;
	(void)strikeOut;
	(void)charSet;
	(void)outputPrecision;
	(void)clipPrecision;
	(void)quality;
	(void)pitchAndFamily;
	(void)faceName;
	return NULL;
}
static int DeleteObject(FrontendTextGdiObject object) {
	(void)object;
	return 0;
}
static int ExtTextOutA(FrontendTextHdc hdc, int x, int y, unsigned int options,
					   const FrontendTextWinRect* rect, const char* text, unsigned int count, const int* dx) {
	(void)hdc;
	(void)x;
	(void)y;
	(void)options;
	(void)rect;
	(void)text;
	(void)count;
	(void)dx;
	return 0;
}
static FrontendTextHdc GetDC(void* hwnd) {
	(void)hwnd;
	return NULL;
}
static int GetTextExtentPoint32A(FrontendTextHdc hdc, const char* text, int count, FrontendTextSize* size) {
	(void)hdc;
	(void)text;
	(void)count;
	if (size != NULL) {
		size->cx = 0;
		size->cy = 0;
	}
	return 0;
}
static int ReleaseDC(void* hwnd, FrontendTextHdc hdc) {
	(void)hwnd;
	(void)hdc;
	return 0;
}
static FrontendTextGdiObject SelectObject(FrontendTextHdc hdc, FrontendTextGdiObject object) {
	(void)hdc;
	return object;
}
static int SetBkColor(FrontendTextHdc hdc, unsigned int color) {
	(void)hdc;
	(void)color;
	return 0;
}
static int SetBkMode(FrontendTextHdc hdc, int mode) {
	(void)hdc;
	(void)mode;
	return 0;
}
static int SetMapMode(FrontendTextHdc hdc, int mode) {
	(void)hdc;
	(void)mode;
	return 0;
}
static int SetRect(FrontendTextWinRect* rect, int left, int top, int right, int bottom) {
	if (rect != NULL) {
		rect->left = left;
		rect->top = top;
		rect->right = right;
		rect->bottom = bottom;
	}
	return 1;
}
static int SetTextCharacterExtra(FrontendTextHdc hdc, int extra) {
	(void)hdc;
	(void)extra;
	return 0;
}
static int SetTextColor(FrontendTextHdc hdc, unsigned int color) {
	(void)hdc;
	(void)color;
	return 0;
}
#endif
#endif

enum {
	BITMAP_FONT_DISK_SIZE = 0x60b,
	BITMAP_FONT_SLOT_COUNT = 10,
	BITMAP_FONT_SIZE_COUNT = 256,
};

// GLOBAL: XWA 0x9F7EF7
int g_textColorCodes[6];
// GLOBAL: XWA 0x9F7FFB
BitmapFont g_fontSlots[BITMAP_FONT_SLOT_COUNT];
// GLOBAL: XWA 0x9FBC69
BitmapFont* g_fontBySize[BITMAP_FONT_SIZE_COUNT];
// GLOBAL: XWA 0xA213B1
int g_glyphGradientBg;
// GLOBAL: XWA 0xA213B5
static int g_savedGlyphGradientBg;
// GLOBAL: XWA 0xA213B9
int g_glyphGradientFgCached;
// GLOBAL: XWA 0xA213BD
int g_glyphGradientLut[32];
// GLOBAL: XWA 0x9FC069
int g_glyphScratchTtl;
// GLOBAL: XWA 0x9FC06D
int g_glyphScratchReload;
// GLOBAL: XWA 0x9FC071
uint16_t g_glyphScratchBuffer[65536];
// GLOBAL: XWA 0x7831B8
int g_textFieldCursorCharIndex;
// GLOBAL: XWA 0x7833D0
int g_textFieldLength;
// GLOBAL: XWA 0x7833D4
int g_activeTextFieldId;
// GLOBAL: XWA 0x7833E8
int g_textFieldColor;
// GLOBAL: XWA 0x7833EC
int g_textFieldColorInitialized;

static void FrontendText_DecodeFontHeader(BitmapFont* font, const unsigned char* diskHeader) {
	int i;

	for (i = 0; i < 256; ++i) {
		font->glyphBitOffset[i] = ByteOrder_ReadU32Le(&diskHeader[4 + i * 4]);
	}

	memcpy(font->glyphHeight, &diskHeader[0x404], sizeof(font->glyphHeight));
	memcpy(font->glyphWidth, &diskHeader[0x504], sizeof(font->glyphWidth));
	font->pointSize = ByteOrder_ReadU32Le(&diskHeader[0x604]);
	font->inUse = diskHeader[0x608];
	font->charSpacing = diskHeader[0x609];
	font->field_60A = diskHeader[0x60a];
}

static void FrontendText_EncodeFontHeader(unsigned char* diskHeader, const BitmapFont* font,
										  size_t glyphBlobSize) {
	int i;

	memset(diskHeader, 0, BITMAP_FONT_DISK_SIZE);
	ByteOrder_WriteU32Le(&diskHeader[0], (uint32_t)glyphBlobSize);
	for (i = 0; i < 256; ++i) {
		ByteOrder_WriteU32Le(&diskHeader[4 + i * 4], font->glyphBitOffset[i]);
	}

	memcpy(&diskHeader[0x404], font->glyphHeight, sizeof(font->glyphHeight));
	memcpy(&diskHeader[0x504], font->glyphWidth, sizeof(font->glyphWidth));
	ByteOrder_WriteU32Le(&diskHeader[0x604], font->pointSize);
	diskHeader[0x608] = font->inUse;
	diskHeader[0x609] = font->charSpacing;
	diskHeader[0x60a] = font->field_60A;
}

static int FrontendText_IsEditableTextChar(unsigned char ch) {
	return isalpha(ch) || isdigit(ch) || ispunct(ch);
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5580A0
int FrontendText_ResetGlyphScratchBuffer(int frames) {
	memset(g_glyphScratchBuffer, 0, sizeof(g_glyphScratchBuffer));
	g_glyphScratchTtl = frames;
	g_glyphScratchReload = frames;

	return 1;
}

// FUNCTION: XWA 0x5580D0
int FrontendText_ResetGlyphScratch(void) {
	g_glyphScratchTtl = 0;
	g_glyphScratchReload = 0;

	return 1;
}

// FUNCTION: XWA 0x5580F0
int FrontendText_GetGlyphGradientBg(void) { return g_glyphGradientBg; }

// FUNCTION: XWA 0x558100
int FrontendText_SetGlyphGradientBg(int color) {
	g_glyphGradientBg = color;
	g_glyphGradientFgCached = 0x7fffffff;

	return 1;
}

// FUNCTION: XWA 0x558120
int FrontendText_PushGlyphGradientBg(int newBgColor) {
	g_savedGlyphGradientBg = g_glyphGradientBg;
	g_glyphGradientBg = newBgColor;
	g_glyphGradientFgCached = 0x7fffffff;

	return 1;
}

// FUNCTION: XWA 0x558150
int FrontendText_PopGlyphGradientBg(void) {
	g_glyphGradientBg = g_savedGlyphGradientBg;
	g_glyphGradientFgCached = 0x7fffffff;

	return 1;
}

// FUNCTION: XWA 0x558000
int FrontendText_LoadFontAtlasFile(const char* fileName, int slotIndex) {
#ifdef XWA_MODERN
	unsigned char diskHeader[BITMAP_FONT_DISK_SIZE];
#endif
	BitmapFont* font;
	XwaFile* stream;
	size_t glyphBlobSize;
	void* glyphBits;

	font = &g_fontSlots[slotIndex];
#ifdef XWA_MODERN
	stream = File_OpenAsset(AERON_VFS_ROOT_ASSET, fileName, "rb");
#else
	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
#endif
	if (stream == NULL) {
#ifdef XWA_MODERN
		Aeron_Log("xwa.assets", "Failed to open frontend font '%s'", fileName);
#endif
		return 0;
	}

#ifdef XWA_MODERN
	File_ReadCount(stream, diskHeader, sizeof(diskHeader));
	glyphBlobSize = ByteOrder_ReadU32Le(&diskHeader[0]);
	FrontendText_DecodeFontHeader(font, diskHeader);
#else
	File_ReadCount(stream, font, BITMAP_FONT_DISK_SIZE);
	glyphBlobSize = (size_t)font->pGlyphBits;
#endif
	glyphBits = Mem_Alloc(glyphBlobSize);
	font->pGlyphBits = glyphBits;
	if (glyphBits == NULL) {
#ifdef XWA_MODERN
		Aeron_Log("xwa.assets", "Failed to allocate frontend font '%s' glyph data (%zu bytes)", fileName,
				  glyphBlobSize);
#endif
		font->inUse = 0;
		File_Close(stream);
		return 0;
	}

	File_ReadCount(stream, glyphBits, glyphBlobSize);
	g_fontBySize[font->pointSize] = font;
	File_Close(stream);
	return 1;
}

// FUNCTION: XWA 0x557F80
void FrontendText_SaveFontAtlasFile(const char* fileName, BitmapFont* font, size_t glyphBlobSize) {
	unsigned char diskHeader[BITMAP_FONT_DISK_SIZE];
	XwaFile* stream;

	if (font == NULL) {
		return;
	}

	stream = File_Open(AERON_VFS_ROOT_USER, (char*)fileName, "wb");
	if (stream == NULL) {
		return;
	}

	FrontendText_EncodeFontHeader(diskHeader, font, glyphBlobSize);
	File_WriteCount(stream, diskHeader, sizeof(diskHeader));
	File_WriteCount(stream, font->pGlyphBits, glyphBlobSize);
	File_Close(stream);
}

// FUNCTION: XWA 0x557150
int FrontendText_FreeAllFonts(void) {
	int i;

	for (i = 0; i < BITMAP_FONT_SLOT_COUNT; ++i) {
		if (g_fontSlots[i].inUse == 1) {
			if (g_fontSlots[i].pGlyphBits != NULL) {
				Mem_Free(g_fontSlots[i].pGlyphBits);
				g_fontSlots[i].pGlyphBits = NULL;
			}

			g_fontSlots[i].inUse = 0;
		}
	}

	memset(g_fontBySize, 0, sizeof(g_fontBySize));
	return 0;
}

// FUNCTION: XWA 0x556B20
int FrontendText_LoadFont(int pointSize) {
	BitmapFont* font;
	XwaFile* stream;
	char fileName[1024];
	int slotIndex;
#ifndef XWA_MODERN
	FrontendTextHdc hdc;
	FrontendTextHfont fontHandle;
	int glyphOffsetIndex;
	size_t glyphBlobCapacity;
	int wasBackBufferLocked;
	char* glyphWrite;
	FrontendTextGdiObject oldFont;
	FrontendTextGdiObject selectedFont;
	unsigned int rowSize;
	FrontendTextSize glyphSize;
	int newGlyphEnd;
	FrontendTextWinRect surfaceRect;
	FrontendTextWinRect textRect;
	FrontendTextHdc screenDc;
	char glyphChars[2];
	unsigned char* rowPixels;
	unsigned char* glyphHeight;
	int glyphIndex;
	int rowIndex;
	int totalGlyphBytes;
	int hr;
	int x;
#endif

	if (pointSize > 255) {
		pointSize = 255;
	} else if (pointSize <= 0) {
		pointSize = 1;
	}

	font = NULL;
#ifndef XWA_MODERN
	glyphBlobCapacity = 0;
#endif
	if (g_fontBySize[pointSize] != NULL) {
		return 1;
	}

	for (slotIndex = 0; slotIndex < BITMAP_FONT_SLOT_COUNT; ++slotIndex) {
		if (!g_fontSlots[slotIndex].inUse) {
			font = &g_fontSlots[slotIndex];
			break;
		}
	}

	if (font == NULL) {
		return 0;
	}

	sprintf(fileName, "times%u.abp", (unsigned int)pointSize);
#ifdef XWA_MODERN
	stream = File_OpenAsset(AERON_VFS_ROOT_ASSET, fileName, "rb");
#else
	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
#endif
	if (stream != NULL) {
		File_Close(stream);
		if (FrontendText_LoadFontAtlasFile(fileName, slotIndex)) {
			return 1;
		}
	}

#ifdef XWA_MODERN
	return 0;
#else
	fontHandle = CreateFontA(-pointSize, 0, 0, 0, 400, 0, 0, 0, 0, 4, 0, 3, 0x12, "times new roman");
	if (fontHandle == NULL) {
		return 0;
	}

	wasBackBufferLocked = g_backBufferLocked.word & 0xff;
	FrontendDisplay_UnlockBackBuffer();
	hdc = GetDC(NULL);
	oldFont = SelectObject(hdc, fontHandle);
	SetMapMode(hdc, 1);
	SetTextCharacterExtra(hdc, 0);

	glyphHeight = font->glyphHeight;
	font->glyphWidth[0] = 0;
	for (glyphIndex = 1; glyphIndex < BITMAP_FONT_SIZE_COUNT; ++glyphIndex) {
		glyphChars[0] = (char)glyphIndex;
		glyphChars[1] = '\0';
		GetTextExtentPoint32A(hdc, glyphChars, 1, &glyphSize);
		if (glyphSize.cx < 0) {
			glyphSize.cx = 0;
		}
		if (glyphSize.cy < 0) {
			glyphSize.cy = 0;
		}
		glyphHeight[glyphIndex] = (unsigned char)glyphSize.cy;
		font->glyphWidth[glyphIndex] = (unsigned char)glyphSize.cx;
		glyphBlobCapacity += (size_t)(glyphSize.cx * glyphSize.cy);
	}

	*glyphHeight = font->glyphHeight[1];
	SelectObject(hdc, oldFont);
	screenDc = hdc;
	ReleaseDC(NULL, screenDc);

	glyphWrite = (char*)Mem_Alloc(glyphBlobCapacity);
	if (glyphWrite == NULL) {
		DeleteObject(fontHandle);
		if (wasBackBufferLocked) {
			g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
		}
		return 0;
	}

	font->pGlyphBits = glyphWrite;
	font->glyphBitOffset[0] = 0;
	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = 640;
	surfaceRect.bottom = 480;

	do {
		hr = g_backBufferSurface->lpVtbl->BltFast(g_backBufferSurface, 0, 0, g_offscreenSurface, &surfaceRect,
												  0);
		if (hr != 0) {
			if (hr == DX_DDERR_SURFACELOST) {
				if (FrontendDisplay_RestoreLostSurfaces()) {
					DeleteObject(fontHandle);
					if (wasBackBufferLocked) {
						g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
					}
					Mem_Free(glyphWrite);
					return 0;
				}
				hr = DX_DDERR_WASSTILLDRAWING;
			} else if (hr != DX_DDERR_WASSTILLDRAWING) {
				Mem_Free(glyphWrite);
				DeleteObject(fontHandle);
				if (wasBackBufferLocked) {
					g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				}
				return 0;
			}
		}
	} while (hr != 0);

	totalGlyphBytes = 0;
	for (glyphIndex = 1, glyphOffsetIndex = 8; glyphIndex < BITMAP_FONT_SIZE_COUNT;
		 glyphOffsetIndex += 4, ++glyphIndex) {
		do {
			hr = ((FrontendTextSurfaceGetDcFunc)g_offscreenSurface->lpVtbl->GetDC)(g_offscreenSurface, &hdc);
			if (hr != 0) {
				if (hr == DX_DDERR_SURFACELOST) {
					hr = FrontendDisplay_RestoreLostSurfaces();
					if (hr != 0) {
						goto finish_generation;
					}
				} else if (hr != DX_DDERR_WASSTILLDRAWING) {
					goto finish_generation;
				}
			}
		} while (hr != 0);

		SetMapMode(hdc, 1);
		selectedFont = SelectObject(hdc, fontHandle);
		SetTextColor(hdc, 0xffffff);
		SetBkColor(hdc, 0);
		SetBkMode(hdc, 2);
		SetRect(&textRect, 0, 0, 640, 480);
		glyphChars[0] = (char)glyphIndex;
		glyphChars[1] = '\0';
		ExtTextOutA(hdc, 0, 0, 2, &textRect, glyphChars, 1, NULL);
		SelectObject(hdc, selectedFont);
		((FrontendTextSurfaceReleaseDcFunc)g_offscreenSurface->lpVtbl->ReleaseDC)(g_offscreenSurface, hdc);

		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
#ifdef XWA_MODERN
		font->glyphBitOffset[glyphIndex] = (uint32_t)totalGlyphBytes;
#else
		*(uint32_t*)((unsigned char*)font + glyphOffsetIndex) = (uint32_t)totalGlyphBytes;
#endif
		if (font->glyphHeight[glyphIndex] > 0) {
			rowPixels = g_drawSurfacePtr;
			rowIndex = 0;
			do {
				if (g_displayBpp != 8) {
					if (g_displayBpp == 16) {
						for (x = 0; x < font->glyphWidth[glyphIndex]; ++x) {
							if (*(uint16_t*)&rowPixels[x * 2] != 0) {
								fileName[x] = (char)0xff;
							} else {
								fileName[x] = 0;
							}
						}
						rowSize = (unsigned int)FrontImage_EncodeGlyphRow(
							(int*)g_rleRowBuffer, (unsigned char*)fileName, font->glyphWidth[glyphIndex]);
					}
				} else {
					rowSize = (unsigned int)FrontImage_EncodeGlyphRow((int*)g_rleRowBuffer, rowPixels,
																	  font->glyphWidth[glyphIndex]);
				}

				newGlyphEnd = totalGlyphBytes + (int)rowSize;
				if (newGlyphEnd >= (int)glyphBlobCapacity) {
					char* newGlyphBits;

					newGlyphBits = (char*)Mem_Realloc(
						font->pGlyphBits, glyphBlobCapacity = (size_t)((int)(glyphBlobCapacity * 3) / 2));
					if (newGlyphBits == NULL) {
						Mem_Free(font->pGlyphBits);
						font->pGlyphBits = NULL;
						break;
					}
					glyphWrite = newGlyphBits + totalGlyphBytes;
					font->pGlyphBits = newGlyphBits;
				}

				if (font->pGlyphBits == NULL) {
					break;
				}

				memcpy(glyphWrite, g_rleRowBuffer, rowSize);
				totalGlyphBytes = newGlyphEnd;
				glyphWrite += rowSize;
				rowPixels = g_drawSurfacePtr + g_drawSurfacePitch;
				g_drawSurfacePtr += g_drawSurfacePitch;
				++rowIndex;
			} while ((unsigned int)rowIndex < font->glyphHeight[glyphIndex]);
		}

		FrontendDisplay_UnlockBackBuffer();
		if (font->pGlyphBits == NULL) {
			break;
		}
	}

finish_generation:
	do {
		hr = g_offscreenSurface->lpVtbl->BltFast(g_offscreenSurface, 0, 0, g_backBufferSurface, &surfaceRect,
												 0);
		if (hr != 0) {
			if (hr == DX_DDERR_SURFACELOST) {
				hr = FrontendDisplay_RestoreLostSurfaces();
				if (hr != 0) {
					glyphIndex = 0;
					break;
				}
			}
			if (hr != DX_DDERR_WASSTILLDRAWING) {
				glyphIndex = 0;
				break;
			}
		}
	} while (hr != 0);

	if (wasBackBufferLocked) {
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	}

	DeleteObject(fontHandle);
	if (totalGlyphBytes < (int)glyphBlobCapacity && Mem_Realloc(font->pGlyphBits, totalGlyphBytes) == NULL) {
		Mem_Free(font->pGlyphBits);
		font->pGlyphBits = NULL;
		glyphIndex = 0;
	}

	if (glyphIndex == BITMAP_FONT_SIZE_COUNT) {
		font->pointSize = (uint32_t)pointSize;
		font->inUse = 1;
		font->charSpacing = 0;
		font->field_60A = 1;
		g_fontBySize[pointSize] = font;
		sprintf(fileName, "times%u.abp", (unsigned int)pointSize);
		FrontendText_SaveFontAtlasFile(fileName, font, (size_t)totalGlyphBytes);
		return 1;
	}

	if (font->pGlyphBits != NULL) {
		Mem_Free(font->pGlyphBits);
	}

	return 0;
#endif
}

// FUNCTION: XWA 0x557EE0
int FrontendText_GetFontHeight(int fontSize) {
	BitmapFont* font;

	if (fontSize >= 0 && fontSize <= 255) {
		font = g_fontBySize[fontSize];
		if (font != NULL) {
			return font->glyphHeight[0];
		}
	}

	return 0;
}

// FUNCTION: XWA 0x557F10
int FrontendText_MeasureWidth(const char* str, int fontSize) {
	const char* p;
	BitmapFont* font;
	char ch;
	int width;

	p = str;
	if (str == NULL) {
		return 0;
	}

	if (fontSize < 0 || fontSize > 255) {
		return 0;
	}

	font = g_fontBySize[fontSize];
	if (font == NULL) {
		return 0;
	}

	ch = *p;
	for (width = 0; ch != '\0'; ++p) {
		if ((unsigned char)ch > 6) {
			width += font->glyphWidth[(unsigned char)*p];
			width += font->charSpacing;
		}

		ch = p[1];
	}

	return width - font->charSpacing;
}

// FUNCTION: XWA 0x557310
int FrontendText_Draw(int fontSize, const char* str, int x, int y, int color) {
	BitmapFont* font;
	unsigned int currentColor;
	const char* text;
	int result;
	unsigned char ch;
	intptr_t glyph[521];

	text = str;
	if (text == NULL) {
		return 0;
	}

	if (fontSize < 0 || fontSize > 255) {
		return 0;
	}

	font = g_fontBySize[fontSize];
	if (font == NULL) {
		return 0;
	}

	currentColor = (unsigned int)color;
	result = 0;
	if (*text != '\0') {
		while (1) {
			if (x >= 640) {
				break;
			}

			ch = (unsigned char)*text;
			if (ch == 1) {
				currentColor = (unsigned int)color;
			} else if (ch >= 2 && ch <= 7) {
				currentColor = (unsigned int)g_textColorCodes[ch - 1];
			} else {
				const uint8_t* glyphWidth = font->glyphWidth;

				glyph[0] = glyphWidth[(unsigned char)*text];
				glyph[1] = font->glyphHeight[(unsigned char)*text];
				glyph[3] = 0;
				glyph[2] = 1;
				glyph[8] =
					(intptr_t)((unsigned char*)font->pGlyphBits + font->glyphBitOffset[(unsigned char)*text]);

				result |= FrontImage_DrawGlyph(glyph, x, y, currentColor, 1);
#ifdef XWA_MODERN
				/* Remaster snapshot observer. */
				XwaSnapshot_EmitGlyph((int)fontSize, (unsigned char)*text, x, y, currentColor);
#endif
				x += font->charSpacing + glyphWidth[(unsigned char)*text];
			}

			++text;
			if (*text == '\0') {
				break;
			}
		}
	}

	return result;
}

// FUNCTION: XWA 0x5571A0
int FrontendText_DrawReveal(unsigned int fontSize, const char* str, int x, int y, int color,
							int revealCount) {
	BitmapFont* font;
	unsigned int currentColor;
	int result;
	int index;
	unsigned char ch;
	intptr_t glyph[521];

	if (str == NULL) {
		return 0;
	}

	if ((int)fontSize < 0 || (int)fontSize > 255) {
		return 0;
	}

	font = g_fontBySize[fontSize];
	if (font == NULL) {
		return 0;
	}

	currentColor = (unsigned int)color;
	result = 0;
	index = 0;
	if (*str != '\0') {
		while (1) {
			if (x >= 640 || index > revealCount) {
				break;
			}

			ch = (unsigned char)str[index];
			if (ch == 1) {
				currentColor = (unsigned int)color;
			} else if (ch >= 2 && ch <= 7) {
				currentColor = (unsigned int)g_textColorCodes[ch - 1];
			} else {
				glyph[0] = font->glyphWidth[(unsigned char)str[index]];
				glyph[1] = font->glyphHeight[(unsigned char)str[index]];
				glyph[3] = 0;
				glyph[2] = 1;
				glyph[8] = (intptr_t)((unsigned char*)font->pGlyphBits +
									  font->glyphBitOffset[(unsigned char)str[index]]);

				if (index == revealCount) {
					result |= FrontImage_DrawGlyph(glyph, x, y, (unsigned int)g_textColorCodes[4], 1);
				} else {
					result |= FrontImage_DrawGlyph(glyph, x, y, currentColor, 1);
				}
#ifdef XWA_MODERN
				/* Remaster snapshot observer. */
				XwaSnapshot_EmitGlyph((int)fontSize, (unsigned char)str[index], x, y,
									  index == revealCount ? (unsigned int)g_textColorCodes[4]
														   : currentColor);
#endif
				x += font->charSpacing + font->glyphWidth[(unsigned char)str[index]];
			}

			++index;
			if (str[index] == '\0') {
				break;
			}
		}
	}

	return result;
}

// FUNCTION: XWA 0x557450
int FrontendText_DrawRightAligned(int fontSize, const char* str, int xRight, int y, int color) {
	BitmapFont* font;
	unsigned int currentColor;
	const char* text;
	int result;
	int x;
	intptr_t glyph[521];

	if (str == NULL) {
		return 0;
	}

	if (fontSize < 0 || fontSize > 255) {
		return 0;
	}

	font = g_fontBySize[fontSize];
	if (font == NULL) {
		return 0;
	}

	currentColor = (unsigned int)color;
	x = xRight - FrontendText_MeasureWidth(str, fontSize);
	text = str;
	result = 0;
	while (*text != '\0') {
		unsigned char ch;

		if (x >= 640) {
			break;
		}

		ch = (unsigned char)*text;
		if (ch == 1) {
			currentColor = (unsigned int)color;
		} else if ((unsigned char)ch >= 2 && (unsigned char)ch <= 7) {
			currentColor = (unsigned int)g_textColorCodes[(unsigned char)ch - 1];
		} else {
			glyph[0] = font->glyphWidth[(unsigned char)*text];
			glyph[1] = font->glyphHeight[(unsigned char)*text];
			glyph[3] = 0;
			glyph[2] = 1;
			glyph[8] =
				(intptr_t)((unsigned char*)font->pGlyphBits + font->glyphBitOffset[(unsigned char)*text]);

			result |= FrontImage_DrawGlyph(glyph, x, y, currentColor, 1);
#ifdef XWA_MODERN
			/* Remaster snapshot observer. */
			XwaSnapshot_EmitGlyph((int)fontSize, (unsigned char)*text, x, y, currentColor);
#endif
			x += font->charSpacing + font->glyphWidth[(unsigned char)*text];
		}

		++text;
	}

	return result;
}

// FUNCTION: XWA 0x5575A0
int FrontendText_DrawCentered(int fontSize, const char* str, FrontendRect* rect, int color) {
	BitmapFont* font;
	int x;
	int y;
	char ch;
	int glyphIndex;
	int glyphHeight;
	int result;
	unsigned int currentColor;
	int textWidth;
	intptr_t glyph[521];

	if (str == NULL) {
		return 0;
	}

	if (fontSize < 0 || fontSize > 255) {
		return 0;
	}

	font = g_fontBySize[fontSize];
	if (font == NULL) {
		return 0;
	}

	currentColor = (unsigned int)color;
	textWidth = FrontendText_MeasureWidth(str, fontSize);
	x = rect->left + ((rect->right - rect->left) >> 1) - (textWidth >> 1);
	y = rect->top + ((rect->bottom - rect->top) >> 1);
	y -= FrontendText_GetFontHeight(fontSize) >> 1;

	result = 0;
	if (*str != '\0') {
		const char* text;

		text = str;
		while (1) {
			if (x >= 640) {
				break;
			}

			ch = *text;
			if (*text == 1) {
				currentColor = (unsigned int)color;
			} else if ((unsigned char)ch < 2 || (unsigned char)ch > 7) {
				glyphIndex = (unsigned char)*text;
				glyph[0] = font->glyphWidth[glyphIndex];
				glyphHeight = font->glyphHeight[glyphIndex];
				glyph[3] = 0;
				glyph[2] = 1;
				glyph[1] = glyphHeight;
				glyph[8] = font->glyphBitOffset[glyphIndex];
				glyph[8] += (intptr_t)font->pGlyphBits;

				result |= FrontImage_DrawGlyph(glyph, x, y, currentColor, 1);
#ifdef XWA_MODERN
				/* Remaster snapshot observer. */
				XwaSnapshot_EmitGlyph((int)fontSize, (unsigned char)*text, x, y, currentColor);
#endif
				x += font->charSpacing + font->glyphWidth[(unsigned char)*text];
			} else {
				int colorIndex = (unsigned char)ch;

				currentColor = (unsigned int)g_textColorCodes[colorIndex - 1];
			}

			++text;
			if (*text == '\0') {
				break;
			}
		}
	}

	return result;
}

// FUNCTION: XWA 0x557720
int FrontendText_DrawCenteredReveal(int fontSize, const char* str, FrontendRect* rect, int color,
									int revealRadius) {
	BitmapFont* font;
	unsigned int currentColor;
	int midIndex;
	int firstRevealIndex;
	int lastRevealIndex;
	int x;
	int y;
	int index;
	int result;
	int textWidth;
	char ch;
	intptr_t glyph[521];

	if (str == NULL) {
		return 0;
	}

	if (fontSize < 0 || fontSize > 255) {
		return 0;
	}

	font = g_fontBySize[fontSize];
	if (font == NULL) {
		return 0;
	}

	midIndex = (int)strlen(str) >> 1;
	firstRevealIndex = midIndex - revealRadius;
	if (firstRevealIndex < 0) {
		firstRevealIndex = -1;
	}

	lastRevealIndex = midIndex + revealRadius;
	currentColor = (unsigned int)color;
	textWidth = FrontendText_MeasureWidth(str, fontSize);
	x = rect->left + ((rect->right - rect->left) >> 1) - (textWidth >> 1);
	y = rect->top + ((rect->bottom - rect->top) >> 1);
	y -= FrontendText_GetFontHeight(fontSize) >> 1;
	index = 0;
	result = 0;
	if (*str != '\0') {
		while (1) {
			if (x >= 640) {
				break;
			}

			ch = str[index];
			if (ch == 1) {
				currentColor = (unsigned int)color;
			} else if ((unsigned char)ch >= 2 && (unsigned char)ch <= 7) {
				currentColor = (unsigned int)g_textColorCodes[(unsigned char)ch - 1];
			} else {
				glyph[0] = font->glyphWidth[(unsigned char)str[index]];
				glyph[1] = font->glyphHeight[(unsigned char)str[index]];
				glyph[3] = 0;
				glyph[2] = 1;
				glyph[8] = (intptr_t)((unsigned char*)font->pGlyphBits +
									  font->glyphBitOffset[(unsigned char)str[index]]);

				if (index >= firstRevealIndex && index <= lastRevealIndex) {
					if (index == firstRevealIndex || index == lastRevealIndex) {
						result |= FrontImage_DrawGlyph(glyph, x, y, (unsigned int)g_textColorCodes[4], 1);
#ifdef XWA_MODERN
						/* Remaster snapshot observer. */
						XwaSnapshot_EmitGlyph((int)fontSize, (unsigned char)str[index], x, y,
											  (unsigned int)g_textColorCodes[4]);
#endif
					} else {
						result |= FrontImage_DrawGlyph(glyph, x, y, currentColor, 1);
#ifdef XWA_MODERN
						/* Remaster snapshot observer. */
						XwaSnapshot_EmitGlyph((int)fontSize, (unsigned char)str[index], x, y, currentColor);
#endif
					}
				}

				x += font->charSpacing + font->glyphWidth[(unsigned char)str[index]];
			}

			++index;
			if (str[index] == '\0') {
				break;
			}
		}
	}

	return result;
}

// FUNCTION: XWA 0x557910
int FrontendText_DrawAlignedInRect(int fontSize, const char* str, FrontendRect* rect, int centerH,
								   int centerV, int color) {
	BitmapFont* font;
	unsigned char ch;
	unsigned int currentColor;
	int result;
	int x;
	int y;
	intptr_t glyph[521];

	if (str == NULL) {
		return 0;
	}

	if (fontSize < 0 || fontSize > 255) {
		return 0;
	}

	font = g_fontBySize[fontSize];
	if (font == NULL) {
		return 0;
	}

	currentColor = (unsigned int)color;
	x = FrontendText_MeasureWidth(str, fontSize) >> 1;
	if (centerH) {
		x = rect->left + ((rect->right - rect->left) >> 1) - x;
	} else {
		x = rect->left;
	}

	if (centerV) {
		y = rect->top + ((rect->bottom - rect->top) >> 1);
		y -= FrontendText_GetFontHeight(fontSize) >> 1;
	} else {
		y = rect->top;
	}

	{
		const char* text;

		text = str;
		result = 0;
		if (*text != '\0') {
			while (1) {
				if (x >= 640) {
					break;
				}

				ch = (unsigned char)*text;
				if (ch == 1) {
					currentColor = (unsigned int)color;
				} else if (ch >= 2 && ch <= 7) {
					currentColor = (unsigned int)g_textColorCodes[ch - 1];
				} else {
					const uint8_t* glyphWidth = font->glyphWidth;

					glyph[0] = glyphWidth[(unsigned char)*text];
					glyph[1] = font->glyphHeight[(unsigned char)*text];
					glyph[3] = 0;
					glyph[2] = 1;
					glyph[8] = (intptr_t)((unsigned char*)font->pGlyphBits +
										  font->glyphBitOffset[(unsigned char)*text]);

					result |= FrontImage_DrawGlyph(glyph, x, y, currentColor, 1);
#ifdef XWA_MODERN
					/* Remaster snapshot observer. */
					XwaSnapshot_EmitGlyph((int)fontSize, (unsigned char)*text, x, y, currentColor);
#endif
					x += font->charSpacing + glyphWidth[(unsigned char)*text];
				}

				++text;
				if (*text == '\0') {
					break;
				}
			}
		}
	}

	return result;
}

// FUNCTION: XWA 0x557B10
int FrontendText_DrawWrapped(int fontSize, const char* str, FrontendRect* rect, int color, int lineSpacing,
							 int firstVisibleLine, int enableClip, int clipBottomAdjust) {
	char word[256];
	intptr_t glyph[521];
	BitmapFont* font;
	FrontendRect savedClip;
	FrontendRect clippedRect;
	int lineIndex;
	int y;
	int savedWordLength;
	int atLineStart;
	int savedIndex;
	unsigned int currentColor;
	int result;
	unsigned char currentChar;
	int index;
	int wordLength;
	int x;

	index = 0;
	if (str == NULL) {
		return 0;
	}

	if (*str == '\0') {
		return 0;
	}

	if (fontSize < 0 || fontSize > 255) {
		return 0;
	}

	font = g_fontBySize[fontSize];
	if (font == NULL) {
		return 0;
	}

	currentColor = (unsigned int)color;
	x = rect->left;
	wordLength = 0;
	result = 0;
	y = rect->top;
	savedIndex = 0;
	savedWordLength = 0;
	atLineStart = 1;
	lineIndex = 0;
	FrontendDraw_RectCopy(&clippedRect, rect);
	if (enableClip) {
		FrontendDisplay_GetScreenClipRect(&savedClip);
		FrontendDisplay_SetScreenClipRect640x480(rect);
		if (clippedRect.top + 2 * clipBottomAdjust < clippedRect.bottom) {
			clippedRect.bottom = clippedRect.top + 2 * clipBottomAdjust;
		}

		FrontendDraw_RectClipToBounds(&clippedRect);
		FrontendDisplay_SetScreenClipRect640x480(&clippedRect);
	}

	do {
		const char* base = str;
		unsigned int nextColor;

		currentChar = (unsigned char)str[index];
		nextColor = currentColor;
		if (currentChar == 1) {
			nextColor = (unsigned int)color;
		} else if (currentChar >= 2 && currentChar <= 7) {
			nextColor = g_textColorCodes[currentChar - 1];
		} else if (currentChar != ' ' && currentChar != '\n' && str[index + 1] != '\0' &&
				   currentChar != '$') {
			word[wordLength++] = (char)currentChar;
			atLineStart = 0;
			savedWordLength = wordLength;
		} else {
			int wordWidth;
			int right;
			int i;
			int size = fontSize;

			if (atLineStart == 1 && currentChar == ' ') {
				++index;
				savedIndex = index;
				if (str[index] != '\0') {
					continue;
				}

				currentChar = '\0';
			}

			if (currentChar != '\n' && currentChar != '$') {
				word[wordLength++] = (char)currentChar;
				savedWordLength = wordLength;
			}

			word[wordLength] = '\0';
			wordWidth = FrontendText_MeasureWidth(word, size);
			right = rect->right;
			if (x + wordWidth > right - size) {
				if (lineIndex >= firstVisibleLine) {
					y += size + lineSpacing;
				}

				x = rect->left;
				atLineStart = 1;
				++lineIndex;
			}

			for (i = 0; i < wordLength; ++i) {

				if (lineIndex < firstVisibleLine) {
					x += font->charSpacing + font->glyphWidth[(unsigned char)word[i]];
					if (x > right) {
						x = rect->left;
						++lineIndex;
					}
				} else {
					unsigned char ch;

					ch = (unsigned char)word[i];
					glyph[0] = font->glyphWidth[ch];
					glyph[1] = font->glyphHeight[ch];
					glyph[3] = 0;
					glyph[2] = 1;
					glyph[8] = (intptr_t)((unsigned char*)font->pGlyphBits + font->glyphBitOffset[ch]);
					result |= FrontImage_DrawGlyph(glyph, x, y, currentColor, 1);
#ifdef XWA_MODERN
					/* Remaster snapshot observer. */
					XwaSnapshot_EmitGlyph((int)fontSize, ch, x, y, currentColor);
#endif
					x += font->glyphWidth[ch] + font->charSpacing;
					right = rect->right;
					if (x > right - size) {
						y += size + lineSpacing;
						x = rect->left;
						++lineIndex;
					}

					wordLength = savedWordLength;
				}
			}

			if (currentChar == '\n' || currentChar == '$') {
				if (lineIndex >= firstVisibleLine) {
					y += (int)fontSize + lineSpacing;
				}

				x = rect->left;
				atLineStart = 1;
				++lineIndex;
			}

			index = savedIndex;
			base = str;
			savedWordLength = 0;
			wordLength = 0;
		}

		++index;
		currentColor = nextColor;
		savedIndex = index;
	} while (str[index] != '\0');

	if (enableClip) {
		FrontendDisplay_SetScreenClipRect640x480(&savedClip);
	}

	return lineIndex;
}

// FUNCTION: XWA 0x557AB0
int FrontendText_DrawWrappedClipped(int fontSize, const char* str, FrontendRect* rect, int color,
									int lineSpacing, int firstVisibleLine) {
	return FrontendText_DrawWrapped(fontSize, str, rect, color, lineSpacing, firstVisibleLine, 1, -1);
}

// FUNCTION: XWA 0x557AE0
int FrontendText_DrawWrappedClippedEx(int fontSize, const char* str, FrontendRect* rect, int color,
									  int lineSpacing, int firstVisibleLine, int clipBottomAdjust) {
	return FrontendText_DrawWrapped(fontSize, str, rect, color, lineSpacing, firstVisibleLine, 1,
									clipBottomAdjust);
}

// FUNCTION: XWA 0x569BE0
void FrontendText_DrawFormattedWrappedText(FrontendRect* rect, const unsigned char* text,
										   short suppressCenteredHeadings) {
	FrontendRect rectLine;
	char lineBuffer[320];
	int maxLineWidth;
	short lineStart;
	int index;
	short drawStart;
	short drawEnd;
	short nextLineStart;
	int lastSpaceOffset;
	short finished;

	FrontendDraw_RectCopy(&rectLine, rect);
	maxLineWidth = rectLine.right - rectLine.left;
	index = 0;
	lineStart = 0;
	rectLine.bottom = rectLine.top + 13;
	lineBuffer[0] = '\0';
	drawEnd = -1;
	drawStart = -1;
	nextLineStart = 0;
	lastSpaceOffset = 0;
	finished = 0;

	do {
		unsigned char ch = text[(short)index];

		if (ch != '$' && ch != '\0') {
			if (ch == ' ') {
				lastSpaceOffset = index;
			}

			if (!isspace(ch)) {
				while (!isspace(text[(short)index])) {
					unsigned char ch = text[(short)index];
					if (ch == '$' || ch == '\0') {
						break;
					}

					if (ch == '[') {
						lineBuffer[index - lineStart] = 2;
					} else if (ch == ']') {
						lineBuffer[index - lineStart] = 1;
					} else {
						lineBuffer[index - lineStart] = (char)ch;
					}

					++index;
				}
			} else {
				lineBuffer[index - lineStart] = (char)text[(short)index];
				++index;
			}

			lineBuffer[index - lineStart] = '\0';
			if ((short)FrontendText_MeasureWidth(lineBuffer, 10) >= (short)maxLineWidth) {
				drawStart = lineStart;
				lineStart = lastSpaceOffset + 1;
				drawEnd = lastSpaceOffset;
				nextLineStart = lastSpaceOffset + 1;
				index = lastSpaceOffset + 1;
			}
			ch = 1;
		}
		if (ch == '$' || ch == '\0') {
			drawStart = lineStart;
			if (suppressCenteredHeadings || text[(short)index] != '$') {
				drawEnd = index;
			} else {
				drawEnd = index - 1;
			}

			lineStart = index + 1;
			nextLineStart = ++index;
			if (text[(short)index] == '\0') {
				finished = 1;
			}
		}

		{
			short inColor;
			short outIndex;
			short i;

			inColor = 0;
			for (i = 0; i < drawStart; ++i) {
				if (text[i] == '[') {
					inColor = 1;
				}
				if (text[i] == ']') {
					inColor = 0;
				}
			}

			if (drawStart != -1) {
				outIndex = 0;
				if (inColor) {
					lineBuffer[0] = 2;
					outIndex = 1;
				}
				lineBuffer[outIndex] = '\0';

				if (drawStart <= drawEnd) {
					for (i = drawStart; i <= drawEnd; ++i) {
						unsigned char outputChar = text[i];
						if (outputChar != '[' && outputChar != ']') {
							lineBuffer[outIndex] = (char)outputChar;
							lineStart = nextLineStart;
						} else if (outputChar == '[') {
							lineBuffer[outIndex] = 2;
						} else {
							lineBuffer[outIndex] = 1;
						}
						++outIndex;
					}
				}

				lineBuffer[outIndex] = '\0';
				if (!suppressCenteredHeadings && lineBuffer[0] == '>') {
					FrontendDraw_RectOffsetXY(&rectLine, 0, 1);
					FrontendText_DrawCentered(10, &lineBuffer[1], &rectLine, g_colorYellow);
					FrontendDraw_RectOffsetXY(&rectLine, 0, -1);
				} else {
					FrontendText_DrawAlignedInRect(10, lineBuffer, &rectLine, 0, 1, 0xffff);
				}

				drawEnd = -1;
				drawStart = -1;
				FrontendDraw_RectOffsetXY(&rectLine, 0, 14);
			}
		}
	} while (!finished);
}

// FUNCTION: XWA 0x555CF0
int FrontendText_DrawEditableField(FrontendRect* rect, char* text, int maxChars, int fieldId,
								   unsigned int fontSize, const char* ignoredChars) {
	FrontendRect savedClip;
	FrontendRect clipRect;
	int cursorX;
	int cursorY;
	int result;
	unsigned char ch;
	const char* ignoredScan;
	int textWidth;
	int fieldWidth;
	int scrollOffset;

	if (!g_textFieldColorInitialized) {
		g_textFieldColor = FrontendDisplay_PackRGB(0xff, 0xff, 0xff);
		g_textFieldColorInitialized = 1;
	}

	g_textFieldCursorCharIndex = (int)strlen(text);
	result = 0;
	g_textFieldLength = (int)strlen(text);

	FrontendCursor_GetPos(&cursorX, &cursorY);
	if (FrontendDraw_PointInRect(rect, cursorX, cursorY) &&
		(FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick())) {
		g_activeTextFieldId = fieldId;
	}

	if (g_activeTextFieldId == fieldId) {
		ch = (unsigned char)Keyboard_PeekChar();
		ignoredScan = ignoredChars;
		if (ignoredChars != NULL && *ignoredChars != '\0') {
			while (ch != (unsigned char)*ignoredScan) {
				++ignoredScan;
				if (*ignoredScan == '\0') {
					break;
				}
			}

			if (*ignoredScan != '\0') {
				Keyboard_DiscardChar();
				ch = 0;
			}
		}

		if (ch != 0) {
			if (ch != 27) {
				Keyboard_DiscardChar();
			}

			if (FrontendText_IsEditableTextChar(ch) || ch == ' ') {
				if (g_textFieldLength < maxChars - 1) {
					text[g_textFieldLength] = (char)ch;
					++g_textFieldLength;
					++g_textFieldCursorCharIndex;
					text[g_textFieldLength] = '\0';
				}
			} else if (ch == 13 || ch == 9) {
				result = 1;
			} else if (ch == 8 && g_textFieldLength != 0) {
				--g_textFieldLength;
				--g_textFieldCursorCharIndex;
				text[g_textFieldLength] = '\0';
			}
		}
	}

	textWidth = FrontendText_MeasureWidth(text, fontSize);
	fieldWidth = rect->right - rect->left + 1;
	scrollOffset = 0;
	if (textWidth > fieldWidth) {
		scrollOffset = rect->right - (int)fontSize - textWidth - rect->left + 1;
	}

	FrontendDisplay_GetScreenClipRect(&savedClip);
	FrontendDraw_RectAssign(&clipRect, rect->left + 2, rect->top + 2, rect->right, rect->bottom);
	FrontendDisplay_SetScreenClipRect640x480(&clipRect);
	FrontendText_Draw(fontSize, text, rect->left + scrollOffset + 2, rect->top + 2, g_textFieldColor);
	FrontendDisplay_SetScreenClipRect640x480(&savedClip);

	if (g_activeTextFieldId == fieldId) {
		char savedChar;
		int caretWidth;

		savedChar = text[g_textFieldCursorCharIndex];
		text[g_textFieldCursorCharIndex] = '\0';
		caretWidth = FrontendText_MeasureWidth(text, fontSize);
		text[g_textFieldCursorCharIndex] = savedChar;

		if (FrontendDisplay_GetFrameCounter() % 10 < 5) {
			FrontendDraw_RectCopy(&savedClip, rect);
			savedClip.left += caretWidth + scrollOffset + 1;
			savedClip.right = savedClip.left + 1;
			savedClip.top += 3;
			savedClip.bottom = (int)fontSize + savedClip.top;
			FrontendDraw_Rect(&savedClip, 0, 0, (short)g_textFieldColor, 1);
		}
	}

	return result;
}

// FUNCTION: XWA 0x529460
int Frontend_FormatSecondsToClockString(unsigned int seconds) {
	unsigned int hours;
	unsigned int minutes;
	unsigned int secondsRemainder;

	secondsRemainder = seconds % 60u;
	minutes = seconds / 60u % 60u;
	hours = seconds / 3600u;
	if (!hours) {
		return sprintf(g_frontendScratchBuffer, "%02d:%02d", minutes, secondsRemainder);
	}
	return sprintf(g_frontendScratchBuffer, "%02d:%02d:%02d", hours, minutes, secondsRemainder);
}
