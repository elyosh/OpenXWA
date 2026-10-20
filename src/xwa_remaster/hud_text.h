#ifndef XWA_REMASTER_HUD_TEXT_H
#define XWA_REMASTER_HUD_TEXT_H

#include "xwa_remaster/hud.h"

typedef struct XwaHudMappedGlyph {
	uint16_t pane;
	uint16_t anchor;
	float x_ref, y_ref;
	float w_ref, h_ref;
} XwaHudMappedGlyph;

int XwaRemasterHudText_Init(void);
void XwaRemasterHudText_Shutdown(void);
void XwaRemasterHudText_Build(const XwaSnapshot* snapshot, XwaHudProfileIndex profile,
							  uint32_t bundle_generation);
/* Draws the glyphs with the flight font from flight/fonts/fonts.yaml when
 * there is one, and builds its atlases through `assets`. Otherwise it uses
 * the bitmap tiers. Call it outside any pass. */
void XwaRemasterHudText_PrepareDrawList(AeronCommandBuffer* cmd, XwaRemasterAssets* assets, int target_w,
										int target_h);
/* Visits the pane glyphs the prepared text draws, each with its pane, its
 * character and its cell (x0, y0, x1, y1 in target pixels, the classic
 * advance wide). */
typedef void (*XwaHudGlyphVisitor)(void* context, uint16_t pane, uint8_t ch, const float rect[4]);
void XwaRemasterHudText_ForEachGlyph(int target_w, int target_h, XwaHudGlyphVisitor visit, void* context);
void XwaRemasterHudText_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
							   int target_w, int target_h);
int XwaRemasterHudText_MapGlyph(const XwaHudPane* pane, const XwaHudGlyph* glyph, XwaHudProfileIndex profile,
								int screen_w, int screen_h, XwaHudMappedGlyph* out);
int XwaRemasterHudText_GlyphVisible(const XwaHudPane* pane, const XwaHudGlyph* glyph,
									XwaHudProfileIndex profile, int screen_w, int screen_h);

#endif
