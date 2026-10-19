#ifndef XWA_REMASTER_TEXT_H
#define XWA_REMASTER_TEXT_H

#include "aeron/scene/draw_list2d.h"
#include "xwa_remaster/assets.h"
#include "xwa_runtime/frontend/frontend_fonts.h"
#include "xwa_runtime/runtime/presentation.h"
#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Adds one captured frontend glyph in a target that represents the classic
 * 640x480 coordinate space. The target dimensions select the output scale. */
int XwaRemasterText_AddFrontendGlyph(AeronDrawList2D* list, XwaRemasterAssets* assets,
									 const XwaGlyph2D* glyph, int target_width, int target_height);

/* How 640x480 units map to target pixels: x * scale_x + offset_x across and
 * y * scale_y down. Glyphs are sized with scale_y on both axes, so they
 * aren't stretched. */
typedef struct XwaRemasterTextMapping {
	float scale_x, scale_y;
	float offset_x;
} XwaRemasterTextMapping;

int XwaRemasterText_AddFrontendGlyphMapped(AeronDrawList2D* list, XwaRemasterAssets* assets,
										   const XwaGlyph2D* glyph, const XwaRemasterTextMapping* mapping);

/* Builds the atlases of the fonts from frontend/fonts/fonts.yaml that the
 * snapshot's glyphs use, for a mapping with `scale_y` target pixels per
 * unit down. Call it outside any pass, before adding the glyphs. Glyphs
 * without an atlas use the original font. */
void XwaRemasterText_PrepareFrontendFonts(AeronCommandBuffer* cmd, XwaRemasterAssets* assets,
										  const XwaSnapshot* snap, float scale_y);

/* A TrueType font for flight text (flight/fonts/fonts.yaml), at one size on
 * screen. */
typedef struct XwaRemasterFlightFace {
	const XwaFrontendFont* font;
	int font_size;               /* the height of the font's tier */
	const AeronFontAtlas* atlas; /* NULL when only the metrics are known */
	float em_px;                 /* the em size in target pixels */
	float unit_px;               /* target pixels per flight screen pixel */
} XwaRemasterFlightFace;

/* Returns the height of flight font tier `tier`: 16, 12 or 10. */
int XwaRemasterText_FlightTierFontSize(int tier);
/* Gets the TrueType font of `tier`, without an atlas. Returns 0 when the
 * flight fonts file doesn't set one. */
int XwaRemasterText_FlightFaceMetrics(int tier, XwaRemasterFlightFace* out);
/* Builds the atlas of the TrueType font of `tier`, for text `size_px` target
 * pixels high. Call it outside any pass, before XwaRemasterText_FlightFace. */
void XwaRemasterText_PrepareFlightFace(AeronCommandBuffer* cmd, XwaRemasterAssets* assets, int tier,
									   float size_px);
/* Gets the TrueType font of `tier` with the atlas for text `size_px` target
 * pixels high. Returns 0 when there is no font, or its atlas wasn't built. */
int XwaRemasterText_FlightFace(XwaRemasterAssets* assets, int tier, float size_px,
							   XwaRemasterFlightFace* out);
/* Returns the advance of `ch` in flight text `size_px` high, for text that the HD
 * renderer lays out itself. With `metrics: font`, it is the whole-pixel
 * width that the game's width tables get. Otherwise it is the advance of
 * `bitmap`, the bitmap tier. `face` may be NULL. */
float XwaRemasterText_FlightAdvance(const AeronFontAtlas* bitmap, const XwaRemasterFlightFace* face,
									uint8_t ch, float size_px);
/* Fills `out` with glyph `ch` of the TrueType font, with the texture, the
 * rectangles, the filter and the blend. The glyph is centred in
 * `layout_advance` from `pen_x`, and `top_y` is the top of its line. The
 * caller sets the tint and the scissor. Returns 0 when there is no glyph. */
int XwaRemasterText_FlightFaceSprite(const XwaRemasterFlightFace* face, uint8_t ch, float pen_x, float top_y,
									 float layout_advance, AeronDrawList2DSprite* out);

/* Flight-font byte strings use the recovered 0xfe,color escape convention.
 * size_px is the rendered font cell height in target pixels. With a `face`
 * that has an atlas, the string is drawn with the TrueType font; `face` may
 * be NULL. */
float XwaRemasterText_MeasureFlightString(const AeronFontAtlas* font, const XwaRemasterFlightFace* face,
										  const char* text, float size_px);
typedef enum XwaRemasterTextAlign {
	XWA_REMASTER_TEXT_ALIGN_LEFT = 0,
	XWA_REMASTER_TEXT_ALIGN_CENTER = 1,
	XWA_REMASTER_TEXT_ALIGN_RIGHT = 2,
} XwaRemasterTextAlign;
int XwaRemasterText_AddFlightString(AeronDrawList2D* list, const AeronFontAtlas* font,
									const XwaRemasterFlightFace* face, const char* text, float x_px,
									float y_px, float size_px, XwaRemasterTextAlign align,
									uint32_t initial_argb, const AeronRectI* scissor);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_TEXT_H */
