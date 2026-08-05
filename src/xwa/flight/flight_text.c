#include "xwa/render/renderer_internal.h"

#include "xwa/assets/sprite_resource.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot_hud.h"
#endif

#include <string.h>

// GLOBAL: XWA 0x7FBB70
uint16_t g_flightWordWrapEnabled;
// GLOBAL: XWA 0x8D6BAC
uint16_t g_flightClearLineBgEnabled;
// GLOBAL: XWA 0x910E00
int16_t g_flightCursorX;
// GLOBAL: XWA 0x910DFA
int16_t g_flightCursorY;
// GLOBAL: XWA 0x8052B4
int16_t g_flightTextRenderOffsetX;
// GLOBAL: XWA 0x8052B6
int16_t g_flightTextRenderOffsetY;
// GLOBAL: XWA 0x8D9420
uint16_t g_flightTextPalette[256];
// GLOBAL: XWA 0x917E54
int16_t g_flightClipTop;
// GLOBAL: XWA 0x7C9DA0
int16_t g_flightClipLeft;
// GLOBAL: XWA 0x9109C4
int16_t g_flightClipBottom;
// GLOBAL: XWA 0x80DC50
int16_t g_flightClipRight;
// GLOBAL: XWA 0x910DF8
uint8_t g_flightTextBgColor;
// GLOBAL: XWA 0x7CAB5A
uint8_t g_flightTextColorIndex;
// GLOBAL: XWA 0x8B8E00
uint8_t g_flightTextShadowColor;
// GLOBAL: XWA 0x91AB68
uint32_t g_flightTextColorHwArgb;
// GLOBAL: XWA 0x7C9DA4
uint32_t g_flightTextShadowHwArgb;
// GLOBAL: XWA 0x8BF36C
uint8_t g_flightTextShadowEnabled;
// GLOBAL: XWA 0x6002CC
uint8_t g_unusedFlightRenderColorByte;
// GLOBAL: XWA 0x686AB8
float g_flightTextGlyphDepthZ = 1.0f;
// GLOBAL: XWA 0x5B46BC
float g_flightTextGlyphRhw = 1.0f;
// GLOBAL: XWA 0x7B32A0
char g_flightTextScratchBuffer[256];
// GLOBAL: XWA 0x782818
int g_flightTextQueueHead;
// GLOBAL: XWA 0x78281C
int g_flightTextQueueTail;
// GLOBAL: XWA 0x80B614
uint8_t g_flightFontTier;
// GLOBAL: XWA 0x7B7002
uint8_t g_flightFontLineHeight;
// GLOBAL: XWA 0x7B33D0
uint8_t g_flightFontScale;
// GLOBAL: XWA 0x91079E
int16_t g_flightTextReservedState91079E;
// GLOBAL: XWA 0x7B4BE0
uint8_t g_flightFontHasLowercase;
// GLOBAL: XWA 0x8C28D2
uint8_t g_flightFontHalfHeight;
// GLOBAL: XWA 0x8052A8
uint16_t g_flightFontHwScaleDivisor;
// GLOBAL: XWA 0x782814
uint8_t* g_flightFontGlyphWidthsHw;
// GLOBAL: XWA 0x808118
uint8_t* g_flightFontGlyphTableSw;
// GLOBAL: XWA 0x80DC52
uint16_t g_flightFontGlyphStrideSw;
// GLOBAL: XWA 0x7B4BF0
FlightTextGlyphBitmapHw* g_flightActiveFontHwMetadata;
// GLOBAL: XWA 0x7D4C80
uint8_t g_font0Widths[256];
// GLOBAL: XWA 0x7D4D80
uint8_t g_font1Widths[256];
// GLOBAL: XWA 0x7D4E80
uint8_t g_font2Widths[256];
// GLOBAL: XWA 0x8D5F60
FlightTextGlyphBitmapHw g_font0BitmapHw[FLIGHT_TEXT_HW_FONT_GLYPH_COUNT];
// GLOBAL: XWA 0x8D6560
FlightTextGlyphBitmapHw g_font1BitmapHw[FLIGHT_TEXT_HW_FONT_GLYPH_COUNT];
// GLOBAL: XWA 0x8D5960
FlightTextGlyphBitmapHw g_font2BitmapHw[FLIGHT_TEXT_HW_FONT_GLYPH_COUNT];
// GLOBAL: XWA 0x7CA1EC
uint8_t* g_flightFontSmallSw;
// GLOBAL: XWA 0x8B94D8
uint8_t* g_flightFontMediumSw;
// GLOBAL: XWA 0x686B18
int g_fontStretchFixup;
// GLOBAL: XWA 0x686B1C
int g_fontStretchDetected;

typedef struct FlightTextQueueEntry {
	uint32_t color;
	uint16_t x;
	uint16_t y;
	uint8_t scale;
	uint8_t ch;
} FlightTextQueueEntry;

enum {
	FLIGHT_TEXT_QUEUE_CAPACITY = 4096,
	FLIGHT_TEXT_TRI_FLAGS = 34322,
};

// GLOBAL: XWA 0x7CAB60
FlightTextQueueEntry g_flightTextQueue[FLIGHT_TEXT_QUEUE_CAPACITY];

// GLOBAL: XWA 0x5B3390
const uint16_t g_flightTextDecimalDivisors[6] = { 1, 1, 10, 100, 1000, 10000 };

// GLOBAL: XWA 0x5B6840
const uint8_t g_flightCharToColorLut[32] = {
	0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
	0x3c, 0x3d, 0x3e, 0x3f, 0xd5, 0xd5, 0xd4, 0xd3, 0x2c, 0x2d, 0x2e, 0x2f, 0x2c, 0x2d, 0x2e, 0x2f,
};

// GLOBAL: XWA 0x5A9474
const float g_flightTextMediumFontHwScale = 12.0f;
// GLOBAL: XWA 0x5A9478
const float g_flightTextSmallFontHwScale = 10.0f;
// GLOBAL: XWA 0x5A9550
const float g_flightTextNegTwoFloat = -2.0f;

// FUNCTION: XWA 0x451A30
uint8_t FlightText_MeasureGlyphBitmapWidth(FlightTextGlyphBitmapHw glyph, LPDDSURFACEDESC surfaceDesc,
										   uint8_t glyphSize) {
	uint32_t alphaMask;
	uint8_t measuredWidth;
	unsigned int x;
	unsigned int y;
	unsigned int bytesPerPixel;
	int column;
	int row;
	uint8_t* columnPixel;
	char found;

	for (alphaMask = surfaceDesc->ddpfPixelFormat.dwRGBAlphaBitMask; (alphaMask & 1u) == 0u;
		 alphaMask >>= 1) {
	}

	bytesPerPixel = surfaceDesc->ddpfPixelFormat.dwRGBBitCount >> 3;
	x = glyph.x;
	measuredWidth = (uint8_t)(glyphSize >> 2);
	y = glyph.y;
	found = 0;
	column = glyphSize - 1;
	while (column >= 0) {
		columnPixel =
			surfaceDesc->lPitch * (int)y + bytesPerPixel * (column + x) + (uint8_t*)surfaceDesc->lpSurface;
		glyph.texturePage = 1;
		for (row = 0; row < glyphSize; ++row) {
			switch (bytesPerPixel) {
				case 2:
					if (*(uint16_t*)columnPixel != 0) {
						found = 1;
						glyph.texturePage = 1;
						++column;
					}
					break;
				case 4:
					if (*(uint32_t*)columnPixel != 0) {
						found = 1;
						glyph.texturePage = 1;
						++column;
					}
					break;
				default:
					DebugPrintfChannel(0x10000, "Unhandled bpp finding character width!  BadBadBad!\n");
					break;
			}
			if (found) {
				break;
			}
			columnPixel += surfaceDesc->lPitch;
		}

		if (found && glyph.texturePage) {
			measuredWidth = (uint8_t)column;
			break;
		}
		--column;
	}

	if (glyph.texturePage == 0) {
		measuredWidth = glyphSize;
	}

	return measuredWidth;
}

// FUNCTION: XWA 0x451740
int FlightText_BuildWidthTables(void) {
	TexLevel* texLevel;
	IDirectDrawSurface* surface;
	uint8_t currentPage;
	uint8_t page;
	uint16_t glyph;
	int result;
	DDSURFACEDESC surfaceDesc;

	currentPage = 0xff;
	g_flightFontGlyphWidthsHw = g_font0Widths;
	for (glyph = 0; glyph < FLIGHT_TEXT_HW_FONT_GLYPH_COUNT; ++glyph) {
		page = g_font0BitmapHw[glyph].texturePage;
		if (page != currentPage) {
			if (currentPage != 0xff) {
				surface = (IDirectDrawSurface*)((Std3DTextureSurface*)texLevel->image)->pSrcSurface;
				surface->lpVtbl->Unlock(surface, NULL);
			}
			texLevel = &g_modelTypeTable[MODEL_TEXTURE_FONT_MODEL_TYPE].texLevels[page];
			memset(&surfaceDesc, 0, sizeof(surfaceDesc));
			surfaceDesc.dwSize = sizeof(surfaceDesc);
			surface = (IDirectDrawSurface*)((Std3DTextureSurface*)texLevel->image)->pSrcSurface;
			result = surface->lpVtbl->Lock(surface, NULL, &surfaceDesc, DDLOCK_WAIT | DDLOCK_NOSYSLOCK, NULL);
			if (result != 0) {
				return DebugPrintfChannel(0x10000, "Unable to lock font %d surface.  Error %x.\n", 12,
										  result);
			}
		}
		currentPage = page;
		g_flightFontGlyphWidthsHw[glyph] =
			(uint8_t)(FlightText_MeasureGlyphBitmapWidth(g_font0BitmapHw[glyph], &surfaceDesc, 12) + 1);
	}

	g_flightFontGlyphWidthsHw = g_font1Widths;
	for (glyph = 0; glyph < FLIGHT_TEXT_HW_FONT_GLYPH_COUNT; ++glyph) {
		page = g_font1BitmapHw[glyph].texturePage;
		if (page != currentPage) {
			if (currentPage != 0xff) {
				surface = (IDirectDrawSurface*)((Std3DTextureSurface*)texLevel->image)->pSrcSurface;
				surface->lpVtbl->Unlock(surface, NULL);
			}
			texLevel = &g_modelTypeTable[MODEL_TEXTURE_FONT_MODEL_TYPE].texLevels[page];
			memset(&surfaceDesc, 0, sizeof(surfaceDesc));
			surfaceDesc.dwSize = sizeof(surfaceDesc);
			surface = (IDirectDrawSurface*)((Std3DTextureSurface*)texLevel->image)->pSrcSurface;
			result = surface->lpVtbl->Lock(surface, NULL, &surfaceDesc, DDLOCK_WAIT | DDLOCK_NOSYSLOCK, NULL);
			if (result != 0) {
				return DebugPrintfChannel(0x10000, "Unable to lock font %d surface.  Error %x.\n", 16,
										  result);
			}
		}
		currentPage = page;
		g_flightFontGlyphWidthsHw[glyph] =
			(uint8_t)(FlightText_MeasureGlyphBitmapWidth(g_font1BitmapHw[glyph], &surfaceDesc, 16) + 2);
	}

	g_flightFontGlyphWidthsHw = g_font2Widths;
	for (glyph = 0; glyph < FLIGHT_TEXT_HW_FONT_GLYPH_COUNT; ++glyph) {
		page = g_font2BitmapHw[glyph].texturePage;
		if (page != currentPage) {
			if (currentPage != 0xff) {
				surface = (IDirectDrawSurface*)((Std3DTextureSurface*)texLevel->image)->pSrcSurface;
				surface->lpVtbl->Unlock(surface, NULL);
			}
			texLevel = &g_modelTypeTable[MODEL_TEXTURE_FONT_MODEL_TYPE].texLevels[page];
			memset(&surfaceDesc, 0, sizeof(surfaceDesc));
			surfaceDesc.dwSize = sizeof(surfaceDesc);
			surface = (IDirectDrawSurface*)((Std3DTextureSurface*)texLevel->image)->pSrcSurface;
			result = surface->lpVtbl->Lock(surface, NULL, &surfaceDesc, DDLOCK_WAIT | DDLOCK_NOSYSLOCK, NULL);
			if (result != 0) {
				return DebugPrintfChannel(0x10000, "Unable to lock font %d surface.  Error %x.\n", 10,
										  result);
			}
		}
		currentPage = page;
		g_flightFontGlyphWidthsHw[glyph] =
			(uint8_t)(FlightText_MeasureGlyphBitmapWidth(g_font2BitmapHw[glyph], &surfaceDesc, 10) + 1);
	}

	surface = (IDirectDrawSurface*)((Std3DTextureSurface*)texLevel->image)->pSrcSurface;
	return surface->lpVtbl->Unlock(surface, NULL);
}

// FUNCTION: XWA 0x451B50
void FlightText_DetectStretchBug(void) {
	/* TODO: Revisit if an Aeron renderer backend needs the legacy Direct3D stretch-bug probe. */
}

// FUNCTION: XWA 0x451640
static int FlightText_GetGlyphCellClass(int gridX, int gridY, int gridSize) {
	int result;

	result = 0;
	if (gridY == 0) {
		if ((gridX + 2) % 3 == 0) {
			result = 1;
		}
	} else if (gridY + 1 == gridSize) {
		if (gridX % 3 == (gridY - 1) % 3) {
			result = 2;
		}
	} else if (gridX == 0) {
		if ((gridY - 1) % 3 == 0) {
			result = 2;
		}
	} else if (gridX + 1 == gridSize) {
		if ((gridX + 1) % 3 == (gridY - 1) % 3) {
			result = 1;
		}
	} else if ((gridX + 2) % 3 == gridY % 3) {
		result = 1;
	} else if ((gridX + 1) % 3 == gridY % 3) {
		result = 2;
	}

	return result;
}

// FUNCTION: XWA 0x451500
static uint16_t FlightText_FillGeneratedGlyphMap(uint8_t texturePage, FlightTextGlyphBitmapHw* glyphMap,
												 uint16_t firstGlyph, uint8_t glyphSize, int gridSize) {
	int gridY;

	for (gridY = 0; gridY < gridSize; ++gridY) {
		int gridX;

		for (gridX = 0; gridX < gridSize; ++gridX) {
			int cellClass;

			cellClass = 0;
			if (gridY == 0) {
				if ((gridX + 2) % 3 == 0) {
					cellClass = 1;
				}
			} else if (gridY + 1 == gridSize) {
				if ((gridY - 1) % 3 == gridX % 3) {
					cellClass = 2;
				}
			} else if (gridX == 0) {
				if ((gridY - 1) % 3 == 0) {
					cellClass = 2;
				}
			} else if (gridX + 1 == gridSize) {
				if ((gridX + 1) % 3 == (gridY - 1) % 3) {
					cellClass = 1;
				}
			} else if ((gridX + 2) % 3 == gridY % 3) {
				cellClass = 1;
			} else if ((gridX + 1) % 3 == gridY % 3) {
				cellClass = 2;
			}

			if (cellClass != 0 && firstGlyph < FLIGHT_TEXT_HW_FONT_GLYPH_COUNT) {
				glyphMap[firstGlyph].texturePage = texturePage;
				glyphMap[firstGlyph].x = (uint16_t)(glyphSize * (uint16_t)gridX);
				glyphMap[firstGlyph].y = (uint16_t)(glyphSize * (uint16_t)gridY);
				glyphMap[firstGlyph].field5 = (uint8_t)cellClass;
				++firstGlyph;
			}
		}
	}

	return firstGlyph;
}

#ifdef XWA_MODERN
static void FlightText_CopyGlyphsForMap(TexLevel* texLevels, const TexLevel* sourceTexLevel,
										const FlightTextGlyphBitmapHw* glyphMap, int glyphSize,
										unsigned int bytesPerPixel) {
	const Sprite* sourceImage;
	const uint8_t* sourcePixels;
	int glyph;

	sourceImage = sourceTexLevel->image;
	if (sourceImage == NULL) {
		return;
	}

	sourcePixels = SpriteResource_GetSpritePayload(sourceImage);
	if (sourcePixels == NULL || sourceTexLevel->width == 0) {
		return;
	}

	for (glyph = FLIGHT_TEXT_HW_FONT_FIRST_GLYPH; glyph < FLIGHT_TEXT_HW_FONT_GLYPH_COUNT; ++glyph) {
		const FlightTextGlyphBitmapHw* entry;
		TexLevel* dstLevel;
		uint8_t* dstPixels;
		int sourceX;
		int sourceY;
		int row;

		entry = &glyphMap[glyph];
		dstLevel = &texLevels[entry->texturePage];
		dstPixels = SpriteResource_GetMutableSpritePayload(dstLevel->image);
		if (dstPixels == NULL || dstLevel->width == 0) {
			continue;
		}

		sourceX = glyphSize * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) % 16);
		sourceY = glyphSize * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) / 16);
		for (row = 0; row < glyphSize; ++row) {
			const uint8_t* src;
			uint8_t* dst;

			src = sourcePixels + bytesPerPixel * (sourceX + sourceTexLevel->width * (sourceY + row));
			dst = dstPixels + bytesPerPixel * (entry->x + dstLevel->width * (entry->y + row));
			memcpy(dst, src, (size_t)glyphSize * bytesPerPixel);
		}
	}
}
#endif

// FUNCTION: XWA 0x450EC0
static void FlightText_CopyGlyphsToGeneratedTextures(TexLevel* texLevels, TexLevel* sourceTexBlocks,
													 int bytesPerPixel) {
#ifdef XWA_MODERN
	if (bytesPerPixel != 2 && bytesPerPixel != 4) {
		return;
	}

	FlightText_CopyGlyphsForMap(texLevels, &sourceTexBlocks[0], g_font0BitmapHw, 12, bytesPerPixel);
	FlightText_CopyGlyphsForMap(texLevels, &sourceTexBlocks[1], g_font1BitmapHw, 16, bytesPerPixel);
	FlightText_CopyGlyphsForMap(texLevels, &sourceTexBlocks[2], g_font2BitmapHw, 10, bytesPerPixel);
#else
	uint8_t* sourcePixels;
	Sprite* sourceImage;

	sourceImage = sourceTexBlocks[0].image;
	if (bytesPerPixel == 2) {
		sourcePixels = sourceImage->pixels;
	} else if (bytesPerPixel == 4) {
		sourcePixels = sourceImage->pixels;
	} else {
		return;
	}

	if (bytesPerPixel == 2) {
		uint8_t* dstPixels;
		FlightTextGlyphBitmapHw* entry;
		TexLevel* dstLevel;
		int sourceX;
		int sourceY;
		int glyph;
		int remainingGlyphs;
		int row;
		uint8_t texturePage;
		const uint8_t* src;
		uint8_t* dst;

		glyph = FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		remainingGlyphs = FLIGHT_TEXT_HW_FONT_GLYPH_COUNT - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		do {
			entry = &g_font0BitmapHw[glyph];
			texturePage = entry->texturePage;
			dstLevel = &texLevels[texturePage];
			dstPixels = dstLevel->image->pixels;
			sourceX = 12 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) % 16);
			sourceY = 12 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) / 16);
			for (row = 0; row < 12; ++row) {
				src = sourcePixels + bytesPerPixel * (sourceX + sourceTexBlocks[0].width * (sourceY + row));
				dst = dstPixels + bytesPerPixel * (entry->x + dstLevel->width * (entry->y + row));
				memcpy(dst, src, 12 * bytesPerPixel);
			}
			++glyph;
		} while (--remainingGlyphs != 0);

		sourcePixels = sourceTexBlocks[1].image->pixels;
		glyph = FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		remainingGlyphs = FLIGHT_TEXT_HW_FONT_GLYPH_COUNT - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		do {
			entry = &g_font1BitmapHw[glyph];
			dstLevel = &texLevels[entry->texturePage];
			dstPixels = dstLevel->image->pixels;
			sourceX = 16 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) % 16);
			sourceY = 16 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) / 16);
			for (row = 0; row < 16; ++row) {
				memcpy(dstPixels + bytesPerPixel * (entry->x + dstLevel->width * (entry->y + row)),
					   sourcePixels + bytesPerPixel * (sourceX + sourceTexBlocks[1].width * (sourceY + row)),
					   16 * bytesPerPixel);
			}
			++glyph;
		} while (--remainingGlyphs != 0);

		sourcePixels = sourceTexBlocks[2].image->pixels;
		glyph = FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		remainingGlyphs = FLIGHT_TEXT_HW_FONT_GLYPH_COUNT - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		do {
			entry = &g_font2BitmapHw[glyph];
			dstLevel = &texLevels[entry->texturePage];
			dstPixels = dstLevel->image->pixels;
			sourceX = 10 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) % 16);
			sourceY = 10 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) / 16);
			for (row = 0; row < 10; ++row) {
				memcpy(dstPixels + bytesPerPixel * (entry->x + dstLevel->width * (entry->y + row)),
					   sourcePixels + bytesPerPixel * (sourceX + sourceTexBlocks[2].width * (sourceY + row)),
					   10 * bytesPerPixel);
			}
			++glyph;
		} while (--remainingGlyphs != 0);
	} else if (bytesPerPixel == 4) {
		uint8_t* dstPixels;
		FlightTextGlyphBitmapHw* entry;
		TexLevel* dstLevel;
		int sourceX;
		int sourceY;
		int glyph;
		int remainingGlyphs;
		int row;

		glyph = FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		remainingGlyphs = FLIGHT_TEXT_HW_FONT_GLYPH_COUNT - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		do {
			entry = &g_font0BitmapHw[glyph];
			dstLevel = &texLevels[entry->texturePage];
			dstPixels = dstLevel->image->pixels;
			sourceX = 12 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) % 16);
			sourceY = 12 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) / 16);
			for (row = 0; row < 12; ++row) {
				memcpy(dstPixels + bytesPerPixel * (entry->x + dstLevel->width * (entry->y + row)),
					   sourcePixels + bytesPerPixel * (sourceX + sourceTexBlocks[0].width * (sourceY + row)),
					   12 * bytesPerPixel);
			}
			++glyph;
		} while (--remainingGlyphs != 0);

		sourcePixels = sourceTexBlocks[1].image->pixels;
		glyph = FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		remainingGlyphs = FLIGHT_TEXT_HW_FONT_GLYPH_COUNT - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		do {
			entry = &g_font1BitmapHw[glyph];
			dstLevel = &texLevels[entry->texturePage];
			dstPixels = dstLevel->image->pixels;
			sourceX = 16 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) % 16);
			sourceY = 16 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) / 16);
			for (row = 0; row < 16; ++row) {
				memcpy(dstPixels + bytesPerPixel * (entry->x + dstLevel->width * (entry->y + row)),
					   sourcePixels + bytesPerPixel * (sourceX + sourceTexBlocks[1].width * (sourceY + row)),
					   16 * bytesPerPixel);
			}
			++glyph;
		} while (--remainingGlyphs != 0);

		sourcePixels = sourceTexBlocks[2].image->pixels;
		glyph = FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		remainingGlyphs = FLIGHT_TEXT_HW_FONT_GLYPH_COUNT - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
		do {
			entry = &g_font2BitmapHw[glyph];
			dstLevel = &texLevels[entry->texturePage];
			dstPixels = dstLevel->image->pixels;
			sourceX = 10 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) % 16);
			sourceY = 10 * ((glyph - FLIGHT_TEXT_HW_FONT_FIRST_GLYPH) / 16);
			for (row = 0; row < 10; ++row) {
				memcpy(dstPixels + bytesPerPixel * (entry->x + dstLevel->width * (entry->y + row)),
					   sourcePixels + bytesPerPixel * (sourceX + sourceTexBlocks[2].width * (sourceY + row)),
					   10 * bytesPerPixel);
			}
			++glyph;
		} while (--remainingGlyphs != 0);
	}
#endif
}

// FUNCTION: XWA 0x450B20
TexLevel* FlightText_RemapHardwareFontTextures(TexLevel* sourceTexBlocks) {
	TexLevel* texLevels;
	int count;
	unsigned int bytesPerPixel;
	uint16_t glyph;
	int gridY;
	TexLevel* level;
	Sprite* sourceImage;

	texLevels = (TexLevel*)Memory_CallocTagged(MODEL_TEXTURE_SPECIESTMINFO_TAG,
											   FLIGHT_TEXT_HW_FONT_REMAPPED_PAGES, sizeof(TexLevel));
	memset(g_font0BitmapHw, 0, sizeof(g_font0BitmapHw));
	glyph = FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
	for (gridY = 0; gridY < 21; ++gridY) {
		int gridX;

		for (gridX = 0; gridX < 21; ++gridX) {
			int cellClass;

			cellClass = FlightText_GetGlyphCellClass(gridX, gridY, 21);
			if (cellClass != 0 && glyph < FLIGHT_TEXT_HW_FONT_GLYPH_COUNT) {
				g_font0BitmapHw[glyph].texturePage = 0;
				g_font0BitmapHw[glyph].x = (uint16_t)(12 * gridX);
				g_font0BitmapHw[glyph].y = (uint16_t)(12 * gridY);
				g_font0BitmapHw[glyph].field5 = (uint8_t)cellClass;
				++glyph;
			}
		}
	}
	DebugPrintfChannel(0x10000, "Filled charactermap%d through %d (should be 256).\n", 12, glyph);

	memset(g_font1BitmapHw, 0, sizeof(g_font1BitmapHw));
	glyph = FLIGHT_TEXT_HW_FONT_FIRST_GLYPH;
	for (gridY = 0; gridY < 16; ++gridY) {
		int gridX;

		for (gridX = 0; gridX < 16; ++gridX) {
			int cellClass;

			cellClass = FlightText_GetGlyphCellClass(gridX, gridY, 16);
			if (cellClass != 0 && glyph < FLIGHT_TEXT_HW_FONT_GLYPH_COUNT) {
				g_font1BitmapHw[glyph].texturePage = 1;
				g_font1BitmapHw[glyph].x = (uint16_t)(16 * gridX);
				g_font1BitmapHw[glyph].y = (uint16_t)(16 * gridY);
				g_font1BitmapHw[glyph].field5 = (uint8_t)cellClass;
				++glyph;
			}
		}
	}
	glyph = FlightText_FillGeneratedGlyphMap(2, g_font1BitmapHw, glyph, 16, 8);
	glyph = FlightText_FillGeneratedGlyphMap(3, g_font1BitmapHw, glyph, 16, 8);
	glyph = FlightText_FillGeneratedGlyphMap(4, g_font1BitmapHw, glyph, 16, 8);
	DebugPrintfChannel(0x10000, "Filled charactermap%d through %d (should be 256).\n", 16, glyph);

	memset(g_font2BitmapHw, 0, sizeof(g_font2BitmapHw));
	glyph = FlightText_FillGeneratedGlyphMap(5, g_font2BitmapHw, FLIGHT_TEXT_HW_FONT_FIRST_GLYPH, 10, 12);
	glyph = FlightText_FillGeneratedGlyphMap(6, g_font2BitmapHw, glyph, 10, 12);
	glyph = FlightText_FillGeneratedGlyphMap(7, g_font2BitmapHw, glyph, 10, 12);
	DebugPrintfChannel(0x10000, "Filled charactermap%d through %d (should be 256).\n", 10, glyph);

	level = texLevels;
	level->shift = 0;
	level->width = 256;
	level->height = 256;
	level->image = NULL;
	level->argbColor = -1;
	++level;
	level->shift = 0;
	level->width = 256;
	level->height = 256;
	level->image = NULL;
	level->argbColor = -1;
	++level;
	level->shift = 0;
	level->width = 128;
	level->height = 128;
	level->image = NULL;
	level->argbColor = -1;
	++level;
	level->shift = 0;
	level->width = 128;
	level->height = 128;
	level->image = NULL;
	level->argbColor = -1;
	++level;
	level->shift = 0;
	level->width = 128;
	level->height = 128;
	level->image = NULL;
	level->argbColor = -1;
	++level;
	level->shift = 0;
	level->width = 128;
	level->height = 128;
	level->image = NULL;
	level->argbColor = -1;
	++level;
	level->shift = 0;
	level->width = 128;
	level->height = 128;
	level->image = NULL;
	level->argbColor = -1;
	++level;
	level->shift = 0;
	level->width = 128;
	level->height = 128;
	level->image = NULL;
	level->argbColor = -1;

	sourceImage = sourceTexBlocks[0].image;
	bytesPerPixel = sourceImage->pixelDataSize / (sourceImage->width * sourceImage->height);
	for (count = 0; count < FLIGHT_TEXT_HW_FONT_REMAPPED_PAGES; ++count) {
		size_t size;
		Sprite* image;

		size = bytesPerPixel * (texLevels[count].width * texLevels[count].height) +
			   MODEL_TEXTURE_SPRITE_HEADER_SIZE;
		image = (Sprite*)Memory_AllocTagged(MODEL_TEXTURE_RESOURCEITEM_TAG, size);
		if (image != NULL) {
			/* The generated pages keep a verbatim copy of the source sprite header;
			 * their real dimensions live in the TexLevel entries. */
			memset(image, 0, size);
			memcpy(image, sourceImage, MODEL_TEXTURE_SPRITE_HEADER_SIZE);
		}
		texLevels[count].image = image;
	}

	FlightText_CopyGlyphsToGeneratedTextures(texLevels, sourceTexBlocks, bytesPerPixel);
	Memory_FreeTagged(MODEL_TEXTURE_RESOURCEITEM_TAG, sourceTexBlocks[0].image);
	Memory_FreeTagged(MODEL_TEXTURE_RESOURCEITEM_TAG, sourceTexBlocks[1].image);
	Memory_FreeTagged(MODEL_TEXTURE_RESOURCEITEM_TAG, sourceTexBlocks[2].image);
	Memory_FreeTagged(MODEL_TEXTURE_SPECIESTMINFO_TAG, sourceTexBlocks);
	g_modelTypeTable[MODEL_TEXTURE_FONT_MODEL_TYPE].texLevels = texLevels;
	g_modelTypeTable[MODEL_TEXTURE_FONT_MODEL_TYPE].frameCount = FLIGHT_TEXT_HW_FONT_REMAPPED_PAGES;
	return texLevels;
}

// FUNCTION: XWA 0x450EB0
int FlightText_GetHardwareRemapTextureCount(void) { return FLIGHT_TEXT_HW_FONT_REMAPPED_PAGES; }

static __inline void FlightText_InitProjVertex(ProjVertex* vert, float sx, float sy, float w, float tu,
											   float tv) {
	vert->extraLayerUVCount = 0;
	vert->sx = sx;
	vert->sy = sy;
	vert->litColor[0] = 1.0f;
	vert->litColor[1] = 0.0f;
	vert->litColor[2] = 0.0f;
	vert->litColor[3] = 0.0f;
	vert->tu = tu;
	vert->tv = tv;
	vert->w = w;
}

static __inline int FlightText_AllVerticesInsideViewport(const ProjVertex* verts, int count) {
	int i;

	for (i = 0; i < count; ++i) {
		if (verts[i].sx < g_renderZeroFloat || verts[i].sx >= (float)g_screenWidth ||
			verts[i].sy < g_renderZeroFloat || verts[i].sy >= (float)g_screenHeight) {
			return 0;
		}
	}

	return 1;
}

static __inline int FlightText_ClipGlyphPolygon(ProjVertex* verts) {
	int clippedCount;
	int i;
	int prevVert;

	g_clipCountB = 0;
	clippedCount = g_clipCountA;
#ifdef XWA_MODERN
	if (clippedCount > 0) {
#endif
		prevVert = g_clipIdxA[clippedCount - 1];
		for (i = 0; i < g_clipCountA; ++i) {
			int curVert;

			curVert = g_clipIdxA[i];
			RenderClip_ClipPolyTop(prevVert, curVert, verts);
			prevVert = curVert;
		}
		clippedCount = g_clipCountB;
#ifdef XWA_MODERN
	}
#endif

	g_clipCountA = 0;
#ifdef XWA_MODERN
	if (clippedCount > 0) {
#endif
		prevVert = g_clipIdxB[clippedCount - 1];
		for (i = 0; i < g_clipCountB; ++i) {
			int curVert;

			curVert = g_clipIdxB[i];
			RenderClip_ClipPolyBottom(prevVert, curVert, verts);
			prevVert = curVert;
		}
		clippedCount = g_clipCountA;
#ifdef XWA_MODERN
	}
#endif

	g_clipCountB = 0;
#ifdef XWA_MODERN
	if (clippedCount > 0) {
#endif
		prevVert = g_clipIdxA[clippedCount - 1];
		for (i = 0; i < g_clipCountA; ++i) {
			int curVert;

			curVert = g_clipIdxA[i];
			RenderClip_ClipPolyLeft(prevVert, curVert, verts);
			prevVert = curVert;
		}
		clippedCount = g_clipCountB;
#ifdef XWA_MODERN
	}
#endif

	g_clipCountA = 0;
#ifdef XWA_MODERN
	if (clippedCount > 0) {
#endif
		prevVert = g_clipIdxB[clippedCount - 1];
		for (i = 0; i < g_clipCountB; ++i) {
			int curVert;

			curVert = g_clipIdxB[i];
			RenderClip_ClipPolyRight(prevVert, curVert, verts);
			prevVert = curVert;
		}
#ifdef XWA_MODERN
	}
#endif

	return g_clipCountA;
}

static __inline int FlightText_EmitGlyphPolygon(ProjVertex* verts, TexLevel* texLevel,
												Std3DTextureSurface* texture) {
	int vertexCount;
	int i;
	int result;
	float sy;
	float w;
	float tu;
	float tv;

	vertexCount = g_clipCountA;
	if (vertexCount + g_d3dVertexCount > g_maxBatchVerts || vertexCount + g_d3dIndexCount > g_maxBatchTris) {
		std3D_LockExecuteBuffer();
		std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
		std3D_BeginInstructions();
		std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
		std3D_ExecuteBuffer();
		g_d3dIndexCount = 0;
		g_d3dVertexCount = 0;
		vertexCount = g_clipCountA;
	}

	if (g_capVertexAlpha) {
		if ((uint32_t)texLevel->argbColor > 0xff000000u) {
			texLevel->argbColor = (int32_t)((uint32_t)texLevel->argbColor & 0xfeffffffu);
			vertexCount = g_clipCountA;
		}
		g_capVertexAlpha = 0;
	}

	for (i = 0; i < vertexCount; ++i) {
		int vertIdx;

		vertIdx = g_clipIdxA[i];
		sy = verts[vertIdx].sy;
		w = verts[vertIdx].w;
		tu = verts[vertIdx].tu;
		tv = verts[vertIdx].tv;
		g_flightVertexBuffer[g_d3dVertexCount].sx = verts[vertIdx].sx + g_flightVpOriginX;
		g_flightVertexBuffer[g_d3dVertexCount].sy = sy + g_flightVpOriginY;
		g_flightVertexBuffer[g_d3dVertexCount].sz = w;
		g_flightVertexBuffer[g_d3dVertexCount].rhw = g_flightTextGlyphRhw;
		g_flightVertexBuffer[g_d3dVertexCount].tu = tu;
		g_flightVertexBuffer[g_d3dVertexCount].tv = tv;
		g_flightVertexBuffer[g_d3dVertexCount].color = (uint32_t)texLevel->argbColor;
		g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
		g_clipIdxA[i] = g_d3dVertexCount++;
	}

	g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
	g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[1];
	g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[2];
	g_triBuffer[g_d3dIndexCount].texture = &texture->cacheNode;
	g_triBuffer[g_d3dIndexCount].flags = FLIGHT_TEXT_TRI_FLAGS;
	result = ++g_d3dIndexCount;

	if (g_clipCountA == 4) {
		g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[2];
		g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[3];
		g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[0];
		g_triBuffer[g_d3dIndexCount].texture = &texture->cacheNode;
		g_triBuffer[g_d3dIndexCount].flags = FLIGHT_TEXT_TRI_FLAGS;
		result = ++g_d3dIndexCount;
	}

	return result;
}

// FUNCTION: XWA 0x452440
static void FlightText_SubmitGlyphQuad(int x, int y, int w, int h, uint8_t ch, int color) {
	FlightTextGlyphBitmapHw* glyph;
	TexLevel* texLevel;
	Std3DTextureSurface* textureSurface;
	ProjVertex verts[40];
	float baseU;
	float baseV;
	float stepUV;
	float depthZ;
	int glyphKind;
	int x0;
	int y0;
	int x1;
	int y1;
	int x2;
	int y2;
	struct {
		float u0;
		float v0;
		float u1;
		float v1;
		float u2;
		float v2;
	} uv;
	if (g_useHardware3D) {

		glyph = &g_flightActiveFontHwMetadata[ch];
		texLevel = &g_modelTypeTable[418].texLevels[glyph->texturePage];
		if (texLevel == NULL) {
			return;
		}

		textureSurface = (Std3DTextureSurface*)texLevel->image;
		glyphKind = glyph->field5;

		{
			float texSize;

			texSize = (float)texLevel->width;
			baseU = (float)glyph->x / texSize;
			baseV = (float)glyph->y / texSize;
			stepUV = (float)g_flightFontHwScaleDivisor / texSize;
		}

		switch (glyphKind) {
			case 1:
				x0 = -w;
				y0 = 0;
				x1 = w + g_fontStretchFixup;
				y1 = 0;
				x2 = x1;
				y2 = 2 * h;
				uv.u0 = baseU - stepUV;
				uv.v0 = baseV;
				uv.u1 = baseU + stepUV;
				uv.v1 = baseV;
				uv.u2 = uv.u1;
				uv.v2 = baseV - stepUV * g_flightTextNegTwoFloat;
				break;
			case 2:
				x0 = g_fontStretchFixup + 2 * w;
				y0 = h;
				x1 = 0;
				y1 = h;
				x2 = 0;
				y2 = -h;
				uv.u0 = baseU - stepUV * g_flightTextNegTwoFloat;
				uv.v0 = baseV + stepUV;
				uv.u1 = baseU;
				uv.v1 = uv.v0;
				uv.u2 = baseU;
				uv.v2 = baseV - stepUV;
				break;
			default:
				x0 = 0;
				y0 = 0;
				x1 = g_fontStretchFixup + 2 * w;
				y1 = 0;
				x2 = 0;
				y2 = 2 * h;
				uv.u0 = baseU;
				uv.v0 = baseV;
				uv.u1 = baseU - stepUV * g_flightTextNegTwoFloat;
				uv.v1 = baseV;
				uv.u2 = baseU;
				uv.v2 = baseV - stepUV * g_flightTextNegTwoFloat;
				break;
		}

		texLevel->argbColor = color;
		depthZ = g_flightTextGlyphDepthZ;
		std3D_AddToTextureCache(textureSurface);

		g_clipCountA = 3;
		g_clipVertCursor = 3;
		g_clipIdxA[0] = 0;
		g_clipIdxA[1] = 1;
		g_clipIdxA[2] = 2;
		FlightText_InitProjVertex(&verts[0], (float)(x + x0), (float)(y + y0), depthZ, uv.u0, uv.v0);
		FlightText_InitProjVertex(&verts[1], (float)(x + x1), (float)(y + y1), depthZ, uv.u1, uv.v1);
		FlightText_InitProjVertex(&verts[2], (float)(x + x2), (float)(y + y2), depthZ, uv.u2, uv.v2);

		if (!FlightText_AllVerticesInsideViewport(verts, g_clipCountA)) {
			if (g_fontStretchFixup != 0) {
				if (FlightText_ClipGlyphPolygon(verts) < 2) {
					return;
				}
			} else {
				int glyphWidth;
				float widthUV;
				float rightU;
				int leftBottomY;
				int rightX;

				glyphWidth = g_flightFontGlyphWidthsHw[ch];
				widthUV = (float)glyphWidth / (float)texLevel->width;

				g_clipCountA = 4;
				g_clipVertCursor = 4;
				g_clipIdxA[0] = 0;
				g_clipIdxA[1] = 1;
				g_clipIdxA[2] = 2;
				g_clipIdxA[3] = 3;

				verts[0].sx = (float)x;
				verts[0].sy = (float)y;
				verts[0].tu = baseU;
				verts[0].tv = baseV;

				if (g_flightFontHwScaleDivisor == 10) {
					rightX = x + glyphWidth;
					verts[1].sx = (float)(rightX + g_fontStretchFixup);
					verts[1].tu = baseU + widthUV;
				} else if (g_flightFontHwScaleDivisor == 16) {
					rightX = x + glyphWidth;
					verts[1].sx = (float)(rightX + g_fontStretchFixup);
					verts[1].tu = baseU + widthUV;
				} else {
					rightX = x + glyphWidth;
					verts[1].sx = (float)rightX;
					rightU = widthUV;
					if (g_fontStretchFixup != 0) {
						rightU -= g_renderNegUnitFloat / (float)texLevel->width;
					}
					verts[1].tu = rightU + baseU;
				}
				verts[1].sy = (float)y;
				verts[1].tv = baseV;

				if (g_flightFontHwScaleDivisor == 10) {
					verts[2].sy = (float)(y + h + g_fontStretchDetected);
					verts[2].sx = (float)(rightX + g_fontStretchFixup);
					verts[2].tu = baseU + widthUV;
				} else if (g_flightFontHwScaleDivisor == 16) {
					verts[2].sy = (float)(y + h - g_fontStretchDetected);
					verts[1].sx = (float)(rightX + g_fontStretchFixup);
					verts[1].tu = (widthUV - g_renderNegUnitFloat / (float)texLevel->width) + baseU;
				} else {
					verts[2].sy = (float)(y + h + g_fontStretchDetected);
					verts[2].sx = (float)rightX;
					rightU = widthUV;
					if (g_fontStretchFixup != 0) {
						rightU -= g_renderNegUnitFloat / (float)texLevel->width;
					}
					verts[2].tu = rightU + baseU;
				}

				verts[2].tv = baseV + stepUV;
				if (g_flightFontHwScaleDivisor == 16) {
					leftBottomY = y + h - g_fontStretchDetected;
				} else {
					leftBottomY = y + h + g_fontStretchDetected;
				}
				FlightText_InitProjVertex(&verts[3], (float)x, (float)leftBottomY, depthZ, baseU,
										  baseV + stepUV);

				if (!FlightText_AllVerticesInsideViewport(verts, g_clipCountA)) {
					DebugPrintfChannel(0x10000, "Clipped character %c (x %d,y %d,w %d,h %d).\n", ch, x, y,
									   glyphWidth, h);
					return;
				}
			}
		}

		FlightText_EmitGlyphPolygon(verts, texLevel, textureSurface);
	}
}

// FUNCTION: XWA 0x450940
void FlightText_FlushQueue(void) {
	int queueIdx;
	int queueHead;
	uint8_t lastScale;

	if (!g_useHardware3D) {
		return;
	}

	queueIdx = g_flightTextQueueTail;
	queueHead = g_flightTextQueueHead;
	lastScale = 0xffu;
	if (queueIdx == queueHead) {
		return;
	}

	{
		FlightTextGlyphBitmapHw* font0Bitmap;
		uint8_t* font1Widths;
		FlightTextGlyphBitmapHw* font1Bitmap;

		font0Bitmap = g_font0BitmapHw;
		font1Widths = g_font1Widths;
		font1Bitmap = g_font1BitmapHw;
		do {
			uint8_t scale;
			uint16_t lineHeight;

			scale = g_flightTextQueue[queueIdx].scale;
			if (scale != lastScale) {
				lastScale = scale;
				if (scale < 12u) {
					scale = 10;
				}

				lineHeight = scale;
				if (lineHeight < 12u) {
					g_flightFontTier = 2;
					g_flightFontLineHeight = 10;
					g_flightFontHwScaleDivisor = 10;
					g_flightFontGlyphWidthsHw = g_font2Widths;
					g_flightActiveFontHwMetadata = g_font2BitmapHw;
				} else if (lineHeight < 15u) {
					g_flightFontTier = 1;
					g_flightFontLineHeight = 12;
					g_flightFontHwScaleDivisor = 12;
					g_flightFontGlyphWidthsHw = g_font0Widths;
					g_flightActiveFontHwMetadata = font0Bitmap;
				} else {
					g_flightFontTier = 0;
					g_flightFontLineHeight = 16;
					g_flightFontHwScaleDivisor = 16;
					g_flightFontGlyphWidthsHw = font1Widths;
					g_flightActiveFontHwMetadata = font1Bitmap;
				}
			}

			FlightText_SubmitGlyphQuad(g_flightTextQueue[queueIdx].x, g_flightTextQueue[queueIdx].y,
									   g_flightFontLineHeight, g_flightFontLineHeight,
									   g_flightTextQueue[queueIdx].ch,
									   (int)g_flightTextQueue[queueIdx].color);
			queueIdx = g_flightTextQueueTail + 1;
#ifdef XWA_MODERN
			if (queueIdx >= FLIGHT_TEXT_QUEUE_CAPACITY) {
#else
			if (queueIdx > FLIGHT_TEXT_QUEUE_CAPACITY) {
#endif
				queueIdx = 0;
			}
			g_flightTextQueueTail = queueIdx;
		} while (queueIdx != g_flightTextQueueHead);
	}

	return;
}

// FUNCTION: XWA 0x434DC0
void FlightText_SetScratch(const char* text) {
	char* out;

	out = g_flightTextScratchBuffer;
	if (text != NULL) {
		while (*text != '\0') {
			*out = *text;
			++out;
			++text;
		}
	}
	*out = '\0';
}

// FUNCTION: XWA 0x434E60
uint16_t FlightText_FormatScratchInt(int value) {
	char* out;
	char* digitsStart;
	uint16_t digitCount;
	uint16_t negative;
	int digitIndex;
	int writeValue;

	out = g_flightTextScratchBuffer;
	negative = 0;
	digitsStart = out;
	digitCount = 1;

	if (value < 0) {
		digitsStart = &g_flightTextScratchBuffer[1];
		g_flightTextScratchBuffer[0] = '-';
		out = digitsStart;
		negative = 1;
		value = -value;
	}

	if (value != 0) {
		digitCount = 0;
		for (writeValue = value; writeValue > 0; writeValue /= 10) {
			++digitCount;
		}

		digitIndex = digitCount;
		if (digitIndex > 0) {
			out += digitCount;
			do {
				int digit = value % 10;
				value /= 10;
				*(out - 1) = (char)(digit + '0');
				--out;
				--digitIndex;
			} while (digitIndex != 0);
			out = digitsStart;
		}
	} else {
		*out = '0';
	}

	out[digitCount] = '\0';
	if (negative) {
		++digitCount;
	}
	return digitCount;
}

// FUNCTION: XWA 0x435380
void FlightText_AppendScratchDecimalNumber(uint16_t value, unsigned int width, unsigned int minDigits) {
	uint16_t emittedDigit;

	if (value == 0xffffu) {
		while (width > 0) {
			/* inlined FlightText_AppendScratchChar('0') */
			char* out;

			out = g_flightTextScratchBuffer;
			if (*out != '\0') {
				do {
					++out;
				} while (*out != '\0');
			}
			*out = '0';
			*++out = '\0';
			--width;
		}
		return;
	}

	emittedDigit = 0;
	while (width > 0) {
		uint16_t digit;
		uint16_t divisor;

		divisor = g_flightTextDecimalDivisors[width];
		digit = value / divisor;
		value -= divisor * digit;

		if (emittedDigit || width <= minDigits || digit != 0) {
			emittedDigit = 1;
			if (digit > 9) {
				digit = 9;
			}
			digit += '0';
		} else {
			digit = ' ';
		}

		if (digit == ' ') {
			/* inlined FlightText_AppendScratchChar(' ') */
			char* out;

			out = g_flightTextScratchBuffer;
			if (*out != '\0') {
				do {
					++out;
				} while (*out != '\0');
			}
			*out = ' ';
			*++out = '\0';
		} else {
			/* inlined FlightText_AppendScratchChar(digit) */
			char* out;

			out = g_flightTextScratchBuffer;
			if (*out != '\0') {
				do {
					++out;
				} while (*out != '\0');
			}
			*out = (char)digit;
			*++out = '\0';
		}
		--width;
	}
}

// FUNCTION: XWA 0x435220
void FlightText_DrawDecimalNumber(uint16_t value, unsigned int width, unsigned int minDigits) {
	uint16_t emittedDigit;
	uint16_t remainingValue;

	if (value == 0xffffu) {
		uint16_t savedColorIndex;
		uint16_t savedShadowEnabled;
		unsigned int zeroColorIndex;

		savedShadowEnabled = g_flightTextShadowEnabled;
		savedColorIndex = g_flightTextColorIndex;
		g_flightTextShadowEnabled = 0;

		/* inlined FlightText_MapColorEscapeToPaletteIndex('@') */
		if ((uint8_t)'@' == g_flightColorEscapeBypassChar) {
			zeroColorIndex = '@';
		} else {
			zeroColorIndex = g_flightCharToColorLut[0];
		}
		g_flightTextColorIndex = zeroColorIndex;

		if (g_useHardware3D) {
			/* inlined FlightText_PaletteIndexToHardwareArgb(zeroColorIndex) */
			uint16_t rgb16 = g_flightTextPalette[(uint8_t)zeroColorIndex];
			unsigned int packed;
			if (Display_IsPixelFormat555()) {
				packed = (rgb16 & 0x1f) + 8 * ((rgb16 & 0x3e0) + 8 * (rgb16 & 0x7c00));
			} else {
				packed = (rgb16 & 0x1f) + 4 * ((rgb16 & 0x7e0) + 8 * (rgb16 & 0xf800));
			}
			g_flightTextColorHwArgb = (uint32_t)(8 * packed) | 0xff000000u;
		}

		while (width > 0) {
			g_flightDrawCharFn('0');
			--width;
		}

		g_flightTextShadowEnabled = savedShadowEnabled;
		g_flightTextColorIndex = savedColorIndex;
		return;
	}

	emittedDigit = 0;
	remainingValue = value;
	while (width > 0) {
		uint16_t divisor;
		uint16_t digit;
		unsigned int ch;

		divisor = g_flightTextDecimalDivisors[width];
		digit = remainingValue / divisor;
		remainingValue -= divisor * digit;

		if (emittedDigit || width <= minDigits || digit != 0) {
			emittedDigit = 1;
			if (digit > 9) {
				digit = 9;
			}
			ch = '0' + digit;
		} else {
			ch = ' ';
		}

		g_flightDrawCharFn((uint8_t)ch);
		--width;
	}
}

// FUNCTION: XWA 0x434FB0
void FlightText_DrawString(const char* str) {
	unsigned int code;
	char wordBuf[80];
	const char* wordScan;
	uint16_t wordLen;
	int nextWordWidth;
	uint16_t rgb16;
	uint32_t hwArgb;

	if (str == NULL || *str == '\0') {
		return;
	}

	do {
		if ((uint8_t)*str == 0xfeu) {
			++str;
			code = (uint8_t)*str;
			goto set_color;
		}
		if ((uint8_t)*str < 0x10u) {
			code = (uint8_t)*str;
		set_color:
			g_flightTextColorIndex = (code >= 0x40u && code != g_flightColorEscapeBypassChar)
										 ? g_flightCharToColorLut[code - 0x40u]
										 : (uint8_t)code;
			if (g_useHardware3D) {
				/* inlined FlightText_PaletteIndexToHardwareArgb(g_flightTextColorIndex) */
				rgb16 = g_flightTextPalette[g_flightTextColorIndex];
				if (Display_IsPixelFormat555()) {
					hwArgb = (uint32_t)(8 * ((rgb16 & 0x1f) + 8 * ((rgb16 & 0x3e0) + 8 * (rgb16 & 0x7c00)))) |
							 0xff000000u;
				} else {
					hwArgb = (uint32_t)(8 * ((rgb16 & 0x1f) + 4 * ((rgb16 & 0x7e0) + 8 * (rgb16 & 0xf800)))) |
							 0xff000000u;
				}
				g_flightTextColorHwArgb = hwArgb;
			}
		} else {
			if ((uint8_t)*str == ' ' && g_flightWordWrapEnabled) {
				wordScan = str + 1;
				wordLen = 0;
				while (*wordScan != ' ' && *wordScan != '\0'
#ifdef XWA_MODERN
					   && wordLen < (uint16_t)(sizeof(wordBuf) - 1u)
#endif
				) {
					wordBuf[wordLen] = *wordScan;
					++wordLen;
					++wordScan;
				}
				wordBuf[wordLen] = '\0';

				nextWordWidth = FlightText_MeasureStringWidth(wordBuf);
				nextWordWidth += FlightText_MeasureStringWidth(" ");
				if (nextWordWidth + g_flightCursorX > g_flightClipRight - 2) {
					g_flightDrawCharFn('\n');
				} else {
					g_flightDrawCharFn((uint8_t)*str);
				}
			} else {
				g_flightDrawCharFn((uint8_t)*str);
			}
		}
	} while (*++str != '\0');
}

// FUNCTION: XWA 0x435130
void FlightText_DrawStringCentered(const char* str) {
	uint16_t width;
	int16_t cursorX;
	uint16_t candidateX;
	int cursorY;

	width = FlightText_MeasureStringWidth(str) >> 1;
	cursorX = g_flightClipLeft;
	candidateX = (uint16_t)(((int)g_flightClipRight + (int)cursorX) / 2 - width);
	if ((uint16_t)candidateX >= (int)cursorX) {
		cursorX = (int16_t)candidateX;
	}

	cursorY = g_flightCursorY;
	g_flightCursorY = (int16_t)cursorY;
	g_flightCursorX = cursorX;
	FlightText_DrawString(str);
}

// FUNCTION: XWA 0x4351A0
void FlightText_DrawStringRightAligned(const char* str) {
	uint16_t width;
	uint16_t cursorX;
	int cursorY;

	width = FlightText_MeasureStringWidth(str);
	cursorX = g_flightClipRight;
	cursorX -= width;
	if ((uint16_t)cursorX >= 0x8000u) {
		cursorX = 0;
	}
	if ((uint16_t)cursorX < (int)g_flightClipLeft) {
		cursorX = g_flightClipLeft;
	}

	cursorY = g_flightCursorY;
	g_flightCursorY = (int16_t)cursorY;
	g_flightCursorX = (int16_t)cursorX;
	FlightText_DrawString(str);
}

// FUNCTION: XWA 0x4E6F20
uint16_t FlightText_MeasureStringWidth(const char* str) {
	uint16_t totalWidth;
	uint8_t ch;
	uint8_t chFolded;
	int glyphWidth;

	totalWidth = 0;
	while (1) {
		ch = (uint8_t)*str++;
		chFolded = ch;
		if (ch == 0 || ch == '\n') {
			break;
		}
		if (ch < 0x20u) {
			continue;
		}
		if (ch == 0xfeu) {
			++str;
			continue;
		}

		if (g_flightFontTier != 0 && !g_flightFontHasLowercase && ch >= 'a' && ch <= 'z') {
			ch -= 32;
			chFolded = ch;
		}

		if (g_useHardware3D) {
			glyphWidth = (((uint16_t)g_flightFontHwScaleDivisor >> 1) +
						  g_flightFontScale * g_flightFontGlyphWidthsHw[chFolded]) /
						 (uint16_t)g_flightFontHwScaleDivisor;
		} else {
			glyphWidth = g_flightFontGlyphTableSw[(uint16_t)g_flightFontGlyphStrideSw * (uint8_t)(ch - 32)];
		}
		totalWidth = (uint16_t)(totalWidth + glyphWidth);
	}

	return totalWidth;
}

// FUNCTION: XWA 0x434F20
void FlightText_TruncateStringToWidth(char* str, unsigned int maxWidth) {
	char* buf;
	uint16_t width;
	int trailingIdx;
	char* trailingPtr;

	buf = g_flightTextScratchBuffer;
	if (str != NULL) {
		buf = str;
	}

	if (buf != NULL) {
		width = FlightText_MeasureStringWidth(buf);
		if (width > maxWidth) {
			trailingIdx = (int)strlen(buf) - 1;
			trailingPtr = &buf[trailingIdx];
			width = FlightText_MeasureStringWidth(buf);
			while (width > maxWidth) {
				while (trailingIdx > 0) {
					char ch = *trailingPtr;

					if (ch >= 32 && ch != 0xfe) {
						*trailingPtr = '\0';
						--trailingIdx;
						break;
					}
					--trailingIdx;
				}
#ifdef XWA_MODERN
				if (trailingPtr > buf) {
					--trailingPtr;
				}
#else
				--trailingPtr;
#endif
				width = FlightText_MeasureStringWidth(buf);
			}
		}
	}
}

// FUNCTION: XWA 0x434E30
void FlightText_AppendScratchChar(uint8_t ch) {
	char* out;

	out = g_flightTextScratchBuffer;
	if (*out != '\0') {
		do {
			++out;
		} while (*out != '\0');
	}
	*out = ch;
	*++out = '\0';
}

// FUNCTION: XWA 0x434DF0
void FlightText_AppendScratchString(const char* text) {
	char* out;

	out = g_flightTextScratchBuffer;
	if (*out != '\0') {
		do {
			++out;
		} while (*out != '\0');
	}

	if (text != NULL) {
		while (*text != '\0') {
			*out = *text;
			++out;
			++text;
		}
	}
	*out = '\0';
}

// FUNCTION: XWA 0x4349D0
void FlightText_SetCursor(int x, int y) {
	g_flightCursorY = (int16_t)y;
	g_flightCursorX = (int16_t)x;
}

// FUNCTION: XWA 0x4349F0
int FlightText_SetRenderOffset(int16_t x, int16_t y) {
	int result;

	if ((uint16_t)x >= (uint32_t)g_screenWidth) {
		DebugPrintfChannel(0x10000, "Text offset coordinate X (%d) is out of bounds.\n", (uint16_t)x);
		x = 0;
	}

	result = (uint16_t)y;
	if ((uint16_t)y >= (uint32_t)g_screenHeight) {
		result =
			DebugPrintfChannel(0x10000, "Text offset coordinate Y (%d) is out of bounds.\n", (uint16_t)y);
		y = 0;
	}

	g_flightTextRenderOffsetX = x;
	g_flightTextRenderOffsetY = y;
	return result;
}

// FUNCTION: XWA 0x434A60
int16_t FlightText_SetClipRect(int16_t left, int16_t top, uint16_t right, uint16_t bottom) {
	if (top < 0) {
		top = 0;
	}
	if (left < 0) {
		left = 0;
	}
	if (right > (uint32_t)g_screenWidth) {
		right = (uint16_t)g_screenWidth;
	}
	if (bottom > (uint32_t)g_screenHeight) {
		bottom = (uint16_t)g_screenHeight;
	}

	g_flightClipTop = top;
	g_flightClipLeft = left;
	g_flightClipBottom = (int16_t)bottom;
	g_flightClipRight = (int16_t)right;
	return (int16_t)bottom;
}

// FUNCTION: XWA 0x434C40
uint16_t FlightText_SetWordWrap(uint16_t enabled) {
	g_flightWordWrapEnabled = enabled;
	return enabled;
}

// FUNCTION: XWA 0x434C60
uint16_t FlightText_SetClearLineBackground(uint16_t enabled) {
	g_flightClearLineBgEnabled = enabled;
	return enabled;
}

// FUNCTION: XWA 0x434B70
void FlightText_SetBackgroundColor(uint32_t charOrIndex) {
	if (charOrIndex < 0x40u || charOrIndex == g_flightColorEscapeBypassChar) {
		g_flightTextBgColor = (uint8_t)charOrIndex;
	} else {
		g_flightTextBgColor = g_flightCharToColorLut[charOrIndex - 0x40u];
	}
}

// FUNCTION: XWA 0x434AD0
void FlightText_SetColor(unsigned int charOrIndex) {
	uint16_t rgb16;
	uint32_t hwArgb;

	if (charOrIndex >= 0x40u && charOrIndex != g_flightColorEscapeBypassChar) {
		g_flightTextColorIndex = g_flightCharToColorLut[charOrIndex - 0x40u];
	} else {
		g_flightTextColorIndex = (uint8_t)charOrIndex;
	}
	if (g_useHardware3D) {
		rgb16 = g_flightTextPalette[g_flightTextColorIndex];
		if (Display_IsPixelFormat555()) {
			hwArgb =
				(uint32_t)(8 * ((rgb16 & 0x1f) + 8 * ((rgb16 & 0x3e0) + 8 * (rgb16 & 0x7c00)))) | 0xff000000u;
		} else {
			hwArgb =
				(uint32_t)(8 * ((rgb16 & 0x1f) + 4 * ((rgb16 & 0x7e0) + 8 * (rgb16 & 0xf800)))) | 0xff000000u;
		}
		g_flightTextColorHwArgb = hwArgb;
	}
}

// FUNCTION: XWA 0x434BA0
void FlightText_SetShadowColor(unsigned int charOrIndex) {
	uint16_t rgb16;
	uint32_t hwArgb;

	if (charOrIndex < 0x40u || charOrIndex == g_flightColorEscapeBypassChar) {
		g_flightTextShadowColor = (uint8_t)charOrIndex;
	} else {
		g_flightTextShadowColor = g_flightCharToColorLut[charOrIndex - 0x40u];
	}
	if (g_useHardware3D) {
		rgb16 = g_flightTextPalette[g_flightTextShadowColor];
		if (Display_IsPixelFormat555()) {
			hwArgb =
				(uint32_t)(8 * ((rgb16 & 0x1f) + 8 * ((rgb16 & 0x3e0) + 8 * (rgb16 & 0x7c00)))) | 0xff000000u;
		} else {
			hwArgb =
				(uint32_t)(8 * ((rgb16 & 0x1f) + 4 * ((rgb16 & 0x7e0) + 8 * (rgb16 & 0xf800)))) | 0xff000000u;
		}
		g_flightTextShadowHwArgb = hwArgb;
	}
}

// FUNCTION: XWA 0x434C70
char FlightText_SetFontTier(int tier) {
	int tierByte;
	uint8_t lineHeight;
	float scaledLineHeight;
	float scaleFactor;

	g_flightFontTier = (uint8_t)tier;
	tierByte = (uint8_t)tier;

	switch (tierByte) {
		case 2:
			if (g_flightResolutionMode < 0 || g_flightResolutionMode > FLIGHT_RES_1600x1200) {
				return (char)g_flightResolutionMode;
			}

			if (g_useHardware3D) {
				scaleFactor = g_flightHudScaleFactor;
				scaledLineHeight = scaleFactor * g_flightTextMediumFontHwScale;
				g_flightFontGlyphTableSw = NULL;
				lineHeight = (uint8_t)scaledLineHeight;
				g_flightFontLineHeight = lineHeight;
				g_flightFontGlyphStrideSw = 0;
				g_flightFontHasLowercase = 1;
				g_flightFontHalfHeight = (uint8_t)(lineHeight >> 1);
				FlightText_SelectHardwareFontForLineHeight(lineHeight);
				g_flightFontScale = g_flightFontLineHeight;
				g_flightFontLineHeight = (uint8_t)(g_flightFontLineHeight - (g_flightFontLineHeight >> 2));
				return (char)g_flightFontLineHeight;
			}

			g_flightFontLineHeight = 10;
			g_flightFontGlyphTableSw = g_flightFontMediumSw;
			g_flightFontGlyphStrideSw = 82;
			g_flightFontHasLowercase = 1;
			g_flightFontHalfHeight = 5;
			return (char)g_flightResolutionMode;

		case 1:
			if (g_flightResolutionMode < 0 || g_flightResolutionMode > FLIGHT_RES_1600x1200) {
				return (char)g_flightResolutionMode;
			}

			if (g_useHardware3D) {
				scaleFactor = g_flightHudScaleFactor;
				scaledLineHeight = scaleFactor * g_flightTextMediumFontHwScale;
				g_flightFontGlyphTableSw = NULL;
				lineHeight = (uint8_t)scaledLineHeight;
				g_flightFontLineHeight = lineHeight;
				g_flightFontGlyphStrideSw = 0;
				g_flightFontHasLowercase = 1;
				g_flightFontHalfHeight = (uint8_t)(lineHeight >> 1);
				FlightText_SelectHardwareFontForLineHeight(lineHeight);
				g_flightFontScale = g_flightFontLineHeight;
				g_flightFontLineHeight = (uint8_t)(g_flightFontLineHeight - (g_flightFontLineHeight >> 2));
				return (char)g_flightFontLineHeight;
			}

			g_flightFontLineHeight = 10;
			g_flightFontGlyphTableSw = g_flightFontMediumSw;
			g_flightFontGlyphStrideSw = 82;
			g_flightFontHasLowercase = 1;
			g_flightFontHalfHeight = 5;
			return (char)g_flightResolutionMode;

		case 0:
			if (g_flightResolutionMode < 0 || g_flightResolutionMode > FLIGHT_RES_1600x1200) {
				return (char)g_flightResolutionMode;
			}

			if (g_useHardware3D) {
				scaleFactor = g_flightHudScaleFactor;
				scaledLineHeight = scaleFactor * g_flightTextSmallFontHwScale;
				g_flightFontGlyphTableSw = NULL;
				lineHeight = (uint8_t)scaledLineHeight;
				g_flightFontLineHeight = lineHeight;
				g_flightFontGlyphStrideSw = 0;
				g_flightFontHasLowercase = 1;
				g_flightFontHalfHeight = (uint8_t)(lineHeight >> 1);
				FlightText_SelectHardwareFontForLineHeight(lineHeight);
				g_flightFontScale = g_flightFontLineHeight;
				g_flightFontLineHeight = (uint8_t)(g_flightFontLineHeight - (g_flightFontLineHeight >> 2));
				return (char)g_flightFontLineHeight;
			}

			g_flightFontLineHeight = 8;
			g_flightFontGlyphTableSw = g_flightFontSmallSw;
			g_flightFontGlyphStrideSw = 66;
			g_flightFontHasLowercase = 1;
			g_flightFontHalfHeight = 4;
			return (char)g_flightResolutionMode;

		default:
			return (char)(tierByte - 2);
	}
}

// FUNCTION: XWA 0x450A80
uint16_t FlightText_SelectHardwareFontForLineHeight(uint16_t lineHeight) {
	if (lineHeight < 12u) {
		g_flightFontTier = 2;
		g_flightFontLineHeight = 10;
		g_flightFontHwScaleDivisor = 10;
		g_flightFontGlyphWidthsHw = g_font2Widths;
		g_flightActiveFontHwMetadata = g_font2BitmapHw;
		return 10;
	}

	if (lineHeight < 15u) {
		g_flightFontTier = 1;
		g_flightFontLineHeight = 12;
		g_flightFontHwScaleDivisor = 12;
		g_flightFontGlyphWidthsHw = g_font0Widths;
		g_flightActiveFontHwMetadata = g_font0BitmapHw;
		return lineHeight;
	}

	g_flightFontTier = 0;
	g_flightFontLineHeight = 16;
	g_flightFontHwScaleDivisor = 16;
	g_flightFontGlyphWidthsHw = g_font1Widths;
	g_flightActiveFontHwMetadata = g_font1BitmapHw;
	return 16;
}

// FUNCTION: XWA 0x452400
void FlightText_SetHardwareGlyphDepth(float depthZ) {
	if (g_flightConfPowerVr) {
		g_flightTextGlyphRhw = g_renderUnitFloat / depthZ;
		g_flightTextGlyphDepthZ = depthZ;
	} else {
		g_flightTextGlyphDepthZ = depthZ;
		g_flightTextGlyphRhw = depthZ;
	}
}

// FUNCTION: XWA 0x434C50
uint8_t FlightText_SetShadowEnabled(uint8_t enabled) {
	g_flightTextShadowEnabled = enabled;
	return enabled;
}

// FUNCTION: XWA 0x4D2F10
uint8_t FlightText_DrawHardwareGlyph(uint8_t ch) {
	int16_t drawX;
	int nextHead;
	int headBefore;
	int glyphWidth;
	int16_t offsetY;
	uint8_t savedLineHeight;

	if (ch == '\n') {
		g_flightCursorY += g_flightFontLineHeight;
		g_flightCursorX = g_flightClipLeft;
		return (uint8_t)g_flightClipLeft;
	}

	if (ch < 0x20u) {
		return ch;
	}

	if (g_flightFontTier != 0 && !g_flightFontHasLowercase && ch >= 'a' && ch <= 'z') {
		ch = (uint8_t)(ch - 32u);
	}

	drawX = g_flightCursorX;
	glyphWidth =
		(((uint16_t)g_flightFontHwScaleDivisor >> 1) + g_flightFontScale * g_flightFontGlyphWidthsHw[ch]) /
		(uint16_t)g_flightFontHwScaleDivisor;
	savedLineHeight = g_flightFontLineHeight;

	if (g_flightCursorX + glyphWidth >= g_flightClipRight && g_flightWordWrapEnabled) {
		drawX = g_flightClipLeft;
		g_flightCursorY += g_flightFontLineHeight;
		g_flightCursorX = g_flightClipLeft;
	}

	headBefore = g_flightTextQueueHead;
	nextHead = headBefore + 1;
	if (nextHead >= FLIGHT_TEXT_QUEUE_CAPACITY) {
		nextHead = 0;
	}
	offsetY = g_flightTextRenderOffsetY;
	if (g_flightTextShadowEnabled) {
		if (nextHead != g_flightTextQueueTail) {
			g_flightTextQueue[g_flightTextQueueHead].color = g_flightTextShadowHwArgb;
			g_flightTextQueue[g_flightTextQueueHead].x = (int16_t)(drawX + g_flightTextRenderOffsetX + 1);
			g_flightTextQueue[g_flightTextQueueHead].y = (int16_t)(g_flightCursorY + offsetY);
			g_flightTextQueue[g_flightTextQueueHead].scale = g_flightFontScale;
			g_flightTextQueue[g_flightTextQueueHead].ch = ch;
#ifdef XWA_MODERN
			XwaSnapshotHud_EmitFlightGlyph(ch, g_flightFontTier, g_flightTextQueue[g_flightTextQueueHead].x,
										   g_flightTextQueue[g_flightTextQueueHead].y, g_flightFontScale,
										   (uint8_t)glyphWidth, g_flightTextShadowHwArgb);
#endif

			headBefore = nextHead++;
			g_flightTextQueueHead = headBefore;
			if (nextHead >= FLIGHT_TEXT_QUEUE_CAPACITY) {
				nextHead = 0;
			}
		}
	}

	if (nextHead != g_flightTextQueueTail) {
		g_flightTextQueue[headBefore].color = g_flightTextColorHwArgb;
		g_flightTextQueue[headBefore].x = (int16_t)(drawX + g_flightTextRenderOffsetX);
		g_flightTextQueue[headBefore].y = (int16_t)(g_flightCursorY + offsetY);
		g_flightTextQueue[headBefore].scale = g_flightFontScale;
		g_flightTextQueue[headBefore].ch = ch;
#ifdef XWA_MODERN
		XwaSnapshotHud_EmitFlightGlyph(ch, g_flightFontTier, g_flightTextQueue[headBefore].x,
									   g_flightTextQueue[headBefore].y, g_flightFontScale,
									   (uint8_t)glyphWidth, g_flightTextColorHwArgb);
#endif
		g_flightTextQueueHead = nextHead;
	} else {
		DebugPrintfChannel(0x10000, "Textqueue overflow.  '%c' (%d)\n", ch, ch);
		drawX = g_flightCursorX;
	}

	g_flightCursorX = (int16_t)(drawX + glyphWidth);
	if (g_flightCursorX >= g_flightClipRight && g_flightWordWrapEnabled) {
		g_flightCursorY += savedLineHeight;
		g_flightCursorX = g_flightClipLeft;
	}

	return (uint8_t)glyphWidth;
}

// FUNCTION: XWA 0x4D2A40
uint8_t FlightText_DrawSoftwareGlyph(uint8_t ch) {
	/* TODO: Reimplement FlightText_DrawSoftwareGlyph @ 0x4D2A40. */
	return ch;
}
