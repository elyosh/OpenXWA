#ifndef XWA_RUNTIME_FRONTEND_FRONTEND_FONTS_H
#define XWA_RUNTIME_FRONTEND_FRONTEND_FONTS_H

/* Frontend fonts from frontend/fonts/fonts.yaml.
 *
 * The file sets a TrueType or OpenType font for the frontend text:
 *
 *   version: 1
 *   default:                # for every font size without its own entry
 *     file: consolab.ttf    # in frontend/fonts/
 *     metrics: font         # game (the default) or font
 *     scale: 1.0            # optional: multiplies the font size
 *     spacing: 0.0          # optional: added to each advance with metrics: font, in em
 *     baseline: 0.0         # optional: moves the glyphs down, in em
 *   sizes:
 *     20: {scale: 0.95}     # the game's font size; missing keys come from default
 *
 * The game's font size is the em size in 640x480 units. With `metrics: game`
 * the text keeps the original layout, and each glyph is centred in the space
 * of the original glyph. With `metrics: font` the layout uses the font's
 * advances and line height. Game strings are Windows-1252. If the file has an
 * error, the error is logged with its line and the whole file is ignored.
 *
 * Without the file, or for a size it doesn't set, the functions return the
 * original values. */

#include <stdint.h>

#include "aeron/scene/font_face.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The advance of `ch` in 1/256 units, before the wide-screen scale. `classic`
 * is the original advance in units: the glyph width plus the font's
 * spacing. */
int XwaFrontendFonts_Advance256(int font_size, unsigned char ch, int classic);
/* Whether the layout uses the font's advances (`metrics: font`). */
int XwaFrontendFonts_UsesFontMetrics(int font_size);
/* The line height in units. `classic` is the original font's height. */
int XwaFrontendFonts_Height(int font_size, int classic);
/* Advances a text position by the advance of `ch` times `scale256` / 256.
 * `*x_frac` keeps the fraction, in 1/256 units. */
void XwaFrontendFonts_AdvanceText(int* x, int* x_frac, int font_size, unsigned char ch, int classic,
								  int scale256);

/* A font for the renderer. */
typedef struct XwaFrontendFont {
	const AeronFontFace* face;
	float em_units;       /* the em size in units: 640x480 units in the frontend, screen pixels in flight */
	float baseline_units; /* from the top of the text to the baseline, in units */
	const uint32_t* codepoints; /* the code point of each byte, 256 entries */
} XwaFrontendFont;

/* The font for `font_size`, or NULL when the file doesn't set one. The
 * pointer stays valid until XwaFrontendFonts_Shutdown. */
const XwaFrontendFont* XwaFrontendFonts_Get(int font_size);

/* Flight fonts from flight/fonts/fonts.yaml.
 *
 * The file has the same format as the frontend file, and the font files are
 * in flight/fonts/. The font sizes are the heights of the three flight font
 * tiers: 16 (tier 0), 12 (tier 1) and 10 (tier 2). A size is the em size in
 * the game's flight screen pixels.
 *
 * With `metrics: font`, the game lays out flight text with the font's
 * advances: XwaFlightFonts_WriteWidthTables writes them into the game's
 * width tables, cut to whole pixels. The line height stays the tier's
 * height. Without the file, the functions change nothing and return NULL. */

/* Returns the flight font for `font_size`, or NULL when the file doesn't set
   one. */
const XwaFrontendFont* XwaFlightFonts_Get(int font_size);
/* Returns whether flight text of `font_size` uses the font's advances. */
int XwaFlightFonts_UsesFontMetrics(int font_size);
/* Returns the advance of `ch` in flight screen pixels with `metrics: font`,
   and 0 without. */
float XwaFlightFonts_Advance(int font_size, unsigned char ch);
/* Writes the font's advances into g_font0Widths, g_font1Widths and
   g_font2Widths, for the sizes that use `metrics: font`. Call it after
   FlightText_BuildWidthTables. */
void XwaFlightFonts_WriteWidthTables(void);

/* Frees the frontend and flight fonts. */
void XwaFrontendFonts_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
