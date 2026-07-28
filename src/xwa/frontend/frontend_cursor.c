#include "xwa/frontend/frontend_cursor.h"

#include "aeron/aeron.h"

#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/sprite_resource.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_text.h"
#include <string.h>

enum {
	FRONTEND_CURSOR_HOTSPOT_OFFSET = 5,
	FRONTEND_CURSOR_GROUP_ID = 15220,
};

// GLOBAL: XWA 0x9F65ED
int g_mouseX;
// GLOBAL: XWA 0x9F65F1
int g_mouseY;
// GLOBAL: XWA 0x9F65F5
int g_cursorPrevDrawX;
// GLOBAL: XWA 0x9F65F9
int g_cursorPrevDrawY;
// GLOBAL: XWA 0x9F65FD
unsigned char g_cursorVisible;
// GLOBAL: XWA 0x9F65FE
FrontendCursorBitmap g_cursorDefaultMask;
// GLOBAL: XWA 0x9F6662
unsigned char g_cursorDefaultSaveBuf[200];
// GLOBAL: XWA 0x9F672A
unsigned char* g_cursorMaskPixels;
// GLOBAL: XWA 0x9F672E
unsigned char* g_cursorSaveBuf;
// GLOBAL: XWA 0x9F6732
int g_cursorWidth;
// GLOBAL: XWA 0x9F6736
int g_cursorHeight;
// GLOBAL: XWA 0x9F673A
int g_cursorPrevDrawWidth;
// GLOBAL: XWA 0x9F673E
int g_cursorPrevDrawHeight;
// GLOBAL: XWA 0x9F6742
char g_cursorSpriteName[64];
// GLOBAL: XWA 0x9F6782
char g_cursorLabelText[256];

// GLOBAL: XWA 0x603820
const FrontendCursorBitmap g_defaultCursorBitmap = { {
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x01, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff,
	0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x01,
	0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x01, 0xff, 0x01, 0x00, 0x01, 0xff, 0xff, 0xff,
	0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00,
} };

static int FrontendCursor_HidePlatformCursor(void) { return Aeron_SetHostCursorVisible(0); }

// FUNCTION: XWA 0x55BB80
void FrontendCursor_Show(void) { g_cursorVisible = 1; }

// FUNCTION: XWA 0x55BB90
void FrontendCursor_Hide(void) { g_cursorVisible = 0; }

// FUNCTION: XWA 0x55BBA0
int FrontendCursor_IsVisible(void) { return g_cursorVisible; }

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x55BBF0
int FrontendCursor_SetLabel(const void* text) {
#ifdef XWA_MODERN
	size_t length;

	/* Avoid the original fixed-width source over-read for ordinary C strings. */
	length = strnlen((const char*)text, sizeof(g_cursorLabelText) - 1);
	memcpy(g_cursorLabelText, text, length);
	g_cursorLabelText[length] = 0;
#else
	memcpy(g_cursorLabelText, text, 255);
	g_cursorLabelText[255] = 0;
#endif
	return 1;
}

// FUNCTION: XWA 0x55BA50
int* FrontendCursor_GetPos(int* outX, int* outY) {
	*outX = g_mouseX + FRONTEND_CURSOR_HOTSPOT_OFFSET;
	*outY = g_mouseY + FRONTEND_CURSOR_HOTSPOT_OFFSET;
	return outY;
}

// FUNCTION: XWA 0x55BA70
int FrontendCursor_SetPos(int x, int y) {
	x -= FRONTEND_CURSOR_HOTSPOT_OFFSET;
	y -= FRONTEND_CURSOR_HOTSPOT_OFFSET;
	if (x > 640) {
		x = 640;
	} else if (x < 0) {
		x = 0;
	}

	if (y > 480) {
		y = 480;
	} else if (y < 0) {
		y = 0;
	}

	g_mouseX = x;
	g_mouseY = y;
	return g_SetCursorPos(x, y);
}

// FUNCTION: XWA 0x55BBB0
int FrontendCursor_HideOsCursor(void) { return FrontendCursor_HidePlatformCursor(); }

// FUNCTION: XWA 0x55BBD0
int FrontendCursor_ShowOsCursor(void) {
	/* Intentional SDL-port deviation: legacy callers must not expose the system cursor. */
	return FrontendCursor_HidePlatformCursor();
}

// FUNCTION: XWA 0x55B5E0
void FrontendCursor_Init(void) {
	g_cursorWidth = 10;
	g_cursorHeight = 10;
	g_cursorMaskPixels = g_cursorDefaultMask.pixels;
	g_cursorSaveBuf = g_cursorDefaultSaveBuf;
	memcpy(g_cursorMaskPixels, &g_defaultCursorBitmap, g_cursorWidth * g_cursorHeight);
	memset(g_cursorSpriteName, 0, sizeof(g_cursorSpriteName));
}

// FUNCTION: XWA 0x55DE20
int FrontendCursor_LoadResources(void) {
	SpriteResource_LoadGroup(FRONTEND_CURSOR_GROUP_ID);
	FrontImage_RegisterAtlasSprite("cursor", FRONTEND_CURSOR_GROUP_ID, 0x0fa2, 1);
	FrontImage_RegisterAtlasSprite("cursor1", FRONTEND_CURSOR_GROUP_ID, 0x0fa0, 1);
	FrontImage_RegisterAtlasSprite("cursor2", FRONTEND_CURSOR_GROUP_ID, 0x0fa1, 1);
	FrontImage_RegisterAtlasSprite("cursor3", FRONTEND_CURSOR_GROUP_ID, 0x0fa4, 1);
	FrontImage_RegisterAtlasSprite("famcursor", FRONTEND_CURSOR_GROUP_ID, 0x138a, 1);
	FrontImage_RegisterAtlasSprite("famcursor1", FRONTEND_CURSOR_GROUP_ID, 0x1388, 1);
	FrontImage_RegisterAtlasSprite("famcursor2", FRONTEND_CURSOR_GROUP_ID, 0x1389, 1);
	FrontImage_RegisterAtlasSprite("famcursor3", FRONTEND_CURSOR_GROUP_ID, 0x138b, 1);
	FrontImage_RegisterAtlasSprite("tsettingleftu", FRONTEND_CURSOR_GROUP_ID, 0x0f6e, 5);
	FrontImage_RegisterAtlasSprite("tsettingleftd", FRONTEND_CURSOR_GROUP_ID, 0x0f78, 1);
	FrontImage_RegisterAtlasSprite("tsettingrightu", FRONTEND_CURSOR_GROUP_ID, 0x0f82, 5);
	FrontImage_RegisterAtlasSprite("tsettingrightd", FRONTEND_CURSOR_GROUP_ID, 0x0f8c, 1);
	return 1;
}

// FUNCTION: XWA 0x55DF60
int FrontendCursor_FreeResources(void) {
	FrontImage_FreeResourceByName("cursor");
	FrontImage_FreeResourceByName("cursor1");
	FrontImage_FreeResourceByName("cursor2");
	FrontImage_FreeResourceByName("cursor3");
	FrontImage_FreeResourceByName("famcursor");
	FrontImage_FreeResourceByName("famcursor1");
	FrontImage_FreeResourceByName("famcursor2");
	FrontImage_FreeResourceByName("famcursor3");
	FrontImage_FreeResourceByName("tsettingleftu");
	FrontImage_FreeResourceByName("tsettingleftd");
	FrontImage_FreeResourceByName("tsettingrightu");
	FrontImage_FreeResourceByName("tsettingrightd");
	SpriteResource_UnloadGroup(FRONTEND_CURSOR_GROUP_ID);
	return 1;
}

// FUNCTION: XWA 0x55BAD0
int FrontendCursor_SetImageFromResourceName(char* resourceName, void* saveBuf) {
	int resourceIndex;
	ImageResource* image;
	FrontendRect out;

	resourceIndex = FrontImage_FindResourceByName(resourceName);
	if (resourceIndex == -1) {
		return 0;
	}

	image = g_resourceTable[resourceIndex].desc->image;
	FrontImage_GetResourceRect(resourceName, &out);
	if (image == 0) {
		g_cursorMaskPixels = 0;
	} else {
		g_cursorMaskPixels = image->pixels;
	}

	g_cursorSaveBuf = (unsigned char*)saveBuf;
	g_cursorWidth = out.right - out.left + 1;
	g_cursorHeight = out.bottom - out.top + 1;
	strcpy(g_cursorSpriteName, resourceName);
	return 1;
}

// FUNCTION: XWA 0x53B420
int FrontendCursor_SetImageResourceForCurrentTheme(char* name, void* outBuffer) {
	char themedName[32];

	strcpy(themedName, "fam");
	memset(themedName + 4, 0, 28);
	if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
		strcat(themedName, name);
		FrontendCursor_SetImageFromResourceName(themedName, outBuffer);
		return 1;
	}

	FrontendCursor_SetImageFromResourceName(name, outBuffer);
	return 1;
}

// FUNCTION: XWA 0x55B630
void FrontendCursor_Draw(void) {
	int cursorH;
	int cursorW;
	int clippedW;
	int clippedH;
	int clippedHSave;
	int rowStride;
	unsigned char* backBuf;
	unsigned char* saveDst;
	unsigned char* maskSrc;
	FrontendRect cursorRect;
	FrontendRect rect;

	if (g_mouseX < 0 || g_mouseX >= 640 || g_mouseY < 0 || g_mouseY >= 480) {
		return;
	}

	cursorW = g_cursorWidth;
	cursorH = g_cursorHeight;
	rowStride = g_cursorWidth;
	cursorRect.left = 0;
	cursorRect.top = 0;
	cursorRect.right = g_cursorWidth - 1;
	cursorRect.bottom = g_cursorHeight - 1;
	FrontendDraw_RectOffsetXY(&cursorRect, g_mouseX, g_mouseY);
	FrontendDraw_RectCopy(&rect, &cursorRect);
	FrontendDraw_RectClipToBounds(&cursorRect);
	if ((unsigned int)g_dirtyRectCount > 0) {
		FrontendDraw_AddDirtyRect(&cursorRect);
	}

	clippedW = cursorRect.right - rect.right;
	clippedW += cursorW;
	clippedH = cursorRect.bottom - rect.bottom;
	clippedH += cursorH;
	clippedHSave = clippedH;
	backBuf = FrontendDisplay_LockBackBuffer();
	saveDst = g_cursorSaveBuf;
	g_drawSurfacePtr = backBuf;
	maskSrc = g_cursorMaskPixels;

	if (g_cursorSpriteName[0] != 0) {
		if (g_displayBpp != 8) {
			if (g_displayBpp == 16) {
				unsigned char* src = backBuf + 2 * g_mouseX + g_drawSurfacePitch * g_mouseY;

				if (clippedHSave > 0) {
					int rows = clippedHSave;

					do {
						if (clippedW > 0) {
							int i;

							for (i = 0; i < clippedW; ++i) {
								memcpy(saveDst + 2 * i, src + 2 * i, sizeof(unsigned short));
							}

							cursorW = rowStride;
						}

						saveDst += 2 * cursorW;
						src += 2 * (g_drawSurfacePitch >> 1);
						--rows;
					} while (rows);
					clippedH = clippedHSave;
				}
			}
		} else {
			unsigned char* src = backBuf + g_mouseX + g_drawSurfacePitch * g_mouseY;

			if (clippedH > 0) {
				int rows = clippedH;

				do {
					memcpy(saveDst, src, (size_t)clippedW);

					saveDst += rowStride;
					src += g_drawSurfacePitch;
					--rows;
				} while (rows);
				clippedH = clippedHSave;
			}
		}

		FrontImage_DrawSprite(g_cursorSpriteName, g_mouseX, g_mouseY);
	} else if (g_displayBpp != 8) {
		if (g_displayBpp == 16) {
			unsigned char* dst = backBuf + 2 * g_mouseX + g_drawSurfacePitch * g_mouseY;

			if (clippedH > 0) {
				int rows = clippedH;

				do {
					if (clippedW > 0) {
						int i;

						for (i = 0; i < clippedW; ++i) {
							int mask;
							unsigned short pixel;

							memcpy(saveDst + 2 * i, dst + 2 * i, sizeof(unsigned short));
							mask = maskSrc[i];
							switch (mask) {
								case 1:
									pixel = 0x001f;
									memcpy(dst + 2 * i, &pixel, sizeof(pixel));
									break;
								case 255:
									pixel = 0xffff;
									memcpy(dst + 2 * i, &pixel, sizeof(pixel));
									break;
							}
						}

						cursorW = rowStride;
						clippedH = clippedHSave;
					}

					maskSrc += cursorW;
					dst += 2 * (g_drawSurfacePitch >> 1);
					saveDst += 2 * cursorW;
					--rows;
				} while (rows);
			}
		}
	} else {
		unsigned char* dst = backBuf + g_mouseX + g_drawSurfacePitch * g_mouseY;

		if (clippedH > 0) {
			int rows = clippedH;

			do {
				memcpy(saveDst, dst, (size_t)clippedW);
				if (clippedW > 0) {
					int i;

					for (i = 0; i < clippedW; ++i) {
						if (maskSrc[i]) {
							dst[i] = maskSrc[i];
						}
					}
				}

				saveDst += rowStride;
				dst += g_drawSurfacePitch;
				maskSrc += rowStride;
				--rows;
			} while (rows);
			clippedH = clippedHSave;
		}
	}

	if (g_cursorLabelText[0]) {
		int labelWidth;
		int labelX;
		int labelY;
		int borderColor;
		int savedGradientBg;

		labelWidth = FrontendText_MeasureWidth(g_cursorLabelText, 12);
		labelX = g_mouseX + g_cursorWidth;
		labelY = g_mouseY + g_cursorHeight;
		if (labelX + labelWidth + 5 >= 640) {
			labelX = 634 - labelWidth;
		}

		if (labelY + 17 >= 480) {
			labelY = 462;
		}

		FrontendDraw_RectAssign(&rect, labelX, labelY, labelX + labelWidth + 5, labelY + 15);
		borderColor = FrontendDisplay_PackRGB(0x60, 0x60, 0x60);
		FrontendDraw_Rect(&rect, 0, 0, 16, 1);
		FrontendDraw_RectOutline(&rect, 0, 0, borderColor);
		if ((unsigned int)g_dirtyRectCount > 0) {
			FrontendDraw_AddDirtyRect(&rect);
		}

		savedGradientBg = g_glyphGradientBg;
		FrontendText_SetGlyphGradientBg(0);
		++rect.top;
		FrontendText_DrawCentered(12, g_cursorLabelText, &rect, g_textColorCodes[3]);
		FrontendText_SetGlyphGradientBg(savedGradientBg);
	}

	FrontendDisplay_UnlockBackBuffer();
	g_cursorPrevDrawWidth = clippedW;
	g_cursorPrevDrawX = g_mouseX;
	g_cursorPrevDrawY = g_mouseY;
	g_cursorPrevDrawHeight = clippedH;
}
