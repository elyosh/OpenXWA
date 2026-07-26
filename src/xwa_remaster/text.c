#include "xwa_remaster/text.h"

#include "xwa_remaster/color.h"

#include <math.h>

static int text_edge(int coordinate, int target_extent, int classic_extent) {
	return (int)lroundf((float)coordinate * (float)target_extent / (float)classic_extent);
}

static AeronRectI text_scissor(const XwaGlyph2D* glyph, int target_width, int target_height) {
	AeronRectI rect = { 0, 0, 0, 0 };
	if (glyph->clip_left <= 0 && glyph->clip_top <= 0 && glyph->clip_right >= XWA_CLASSIC_WIDTH - 1 &&
		glyph->clip_bottom >= XWA_CLASSIC_HEIGHT - 1) {
		return rect;
	}
	rect.x = text_edge(glyph->clip_left, target_width, XWA_CLASSIC_WIDTH);
	rect.y = text_edge(glyph->clip_top, target_height, XWA_CLASSIC_HEIGHT);
	rect.width = text_edge(glyph->clip_right + 1, target_width, XWA_CLASSIC_WIDTH) - rect.x;
	rect.height = text_edge(glyph->clip_bottom + 1, target_height, XWA_CLASSIC_HEIGHT) - rect.y;
	return rect;
}

int XwaRemasterText_AddFrontendGlyph(AeronDrawList2D* list, XwaRemasterAssets* assets,
									 const XwaGlyph2D* glyph, int target_width, int target_height) {
	float atlas_scale = 1.0f;
	const AeronFontAtlas* font;
	const AeronFontGlyph* metrics;
	AeronDrawList2DSprite sprite = { 0 };
	uint8_t rgba[4];
	float scale_x;
	float scale_y;

	if (!list || !assets || !glyph || target_width <= 0 || target_height <= 0) {
		return 0;
	}
	font = XwaRemasterAssets_FrontendFont(assets, glyph->font_size, &atlas_scale);
	if (!font || !font->texture || font->atlas_w <= 0 || font->atlas_h <= 0 || atlas_scale <= 0.0f ||
		glyph->ch < font->first_char || glyph->ch >= font->first_char + font->num_chars) {
		return 0;
	}
	metrics = &font->glyphs[glyph->ch - font->first_char];
	if (metrics->atlas_w == 0 || metrics->atlas_h == 0) {
		return 0;
	}

	scale_x = (float)target_width / (float)XWA_CLASSIC_WIDTH;
	scale_y = (float)target_height / (float)XWA_CLASSIC_HEIGHT;
	sprite.texture = font->texture;
	sprite.filter = AERON_BLIT2D_FILTER_LINEAR;
	sprite.blend = AERON_BLIT2D_BLEND_PMA;
	sprite.scissor = text_scissor(glyph, target_width, target_height);
	sprite.src_u0 = (float)metrics->atlas_x / (float)font->atlas_w;
	sprite.src_v0 = (float)metrics->atlas_y / (float)font->atlas_h;
	sprite.src_u1 = (float)(metrics->atlas_x + metrics->atlas_w) / (float)font->atlas_w;
	sprite.src_v1 = (float)(metrics->atlas_y + metrics->atlas_h) / (float)font->atlas_h;
	sprite.dst_x = (float)glyph->x * scale_x;
	sprite.dst_y = (float)glyph->y * scale_y;
	sprite.dst_w = (float)metrics->atlas_w * scale_x / atlas_scale;
	sprite.dst_h = (float)metrics->atlas_h * scale_y / atlas_scale;
	XwaSnapshotExport_ColorToRgba(glyph->color, rgba);
	sprite.tint[0] = XwaRemaster_SrgbToLinear((float)rgba[0] / 255.0f);
	sprite.tint[1] = XwaRemaster_SrgbToLinear((float)rgba[1] / 255.0f);
	sprite.tint[2] = XwaRemaster_SrgbToLinear((float)rgba[2] / 255.0f);
	sprite.tint[3] = 1.0f;
	AeronDrawList_AddSprite(list, &sprite);
	return 1;
}
