#ifndef XWA_FLIGHT_FLIGHT_TEXT_H
#define XWA_FLIGHT_FLIGHT_TEXT_H

#include "xwa/assets/sprite_texture.h"
#include "xwa_runtime/compat/directx/ddraw.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	FLIGHT_TEXT_HW_FONT_GLYPH_COUNT = 256,
	FLIGHT_TEXT_HW_FONT_FIRST_GLYPH = 32,
	FLIGHT_TEXT_HW_FONT_REMAPPED_PAGES = 8,
};

typedef struct FlightTextGlyphBitmapHw {
	uint8_t texturePage;
	uint16_t x;
	uint16_t y;
	uint8_t field5;
} FlightTextGlyphBitmapHw;

extern FlightTextGlyphBitmapHw g_font0BitmapHw[256];
extern FlightTextGlyphBitmapHw g_font1BitmapHw[256];
extern FlightTextGlyphBitmapHw g_font2BitmapHw[256];

uint8_t FlightText_MeasureGlyphBitmapWidth(FlightTextGlyphBitmapHw glyph, LPDDSURFACEDESC surfaceDesc,
										   uint8_t glyphSize);
int FlightText_BuildWidthTables(void);
void FlightText_DetectStretchBug(void);
TexLevel* FlightText_RemapHardwareFontTextures(TexLevel* sourceTexBlocks);
int FlightText_GetHardwareRemapTextureCount(void);

#ifdef __cplusplus
}
#endif

#endif
