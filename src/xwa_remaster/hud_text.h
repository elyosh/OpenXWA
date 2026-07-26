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
void XwaRemasterHudText_PrepareDrawList(AeronCommandBuffer* cmd, int target_w, int target_h);
void XwaRemasterHudText_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass,
							   AeronRenderTarget* target, int target_w, int target_h);
int XwaRemasterHudText_MapGlyph(const XwaHudPane* pane, const XwaHudGlyph* glyph, XwaHudProfileIndex profile,
								int screen_w, int screen_h, XwaHudMappedGlyph* out);
int XwaRemasterHudText_GlyphVisible(const XwaHudPane* pane, const XwaHudGlyph* glyph,
									XwaHudProfileIndex profile, int screen_w, int screen_h);

#endif
