#include "xwa_remaster/text.h"

#include "xwa_remaster/color.h"

#include <math.h>

static float text_flight_advance(const XwaFlightFontRef* font, uint8_t ch, float size_px) {
	if (!font || !font->glyphs || font->cell_h == 0 || ch < font->first_char ||
		ch >= font->first_char + font->num_chars) {
		return size_px * 0.6f;
	}
	const AeronFontGlyph* glyph = &font->glyphs[ch - font->first_char];
	return glyph->advance ? (float)glyph->advance * size_px / font->cell_h : size_px * 0.5f;
}

float XwaRemasterText_MeasureFlightString(const XwaFlightFontRef* font, const char* text,
										 float size_px) {
	float width = 0.0f;
	if (!text || size_px <= 0.0f)
		return 0.0f;
	for (const uint8_t* p = (const uint8_t*)text; *p; p++) {
		if (*p == 0xfeu && p[1]) {
			p++;
			continue;
		}
		if (*p >= 0x20u)
			width += text_flight_advance(font, *p, size_px);
	}
	return width;
}

int XwaRemasterText_AddFlightString(AeronDrawList2D* list, const XwaFlightFontRef* font,
									const char* text, float x_px, float y_px, float size_px,
									XwaRemasterTextAlign align, uint32_t initial_argb,
									const AeronRectI* scissor) {
	if (!list || !font || !font->texture || !font->glyphs || !text || size_px <= 0.0f ||
		font->atlas_w <= 0 || font->atlas_h <= 0)
		return 0;
	if (align == XWA_REMASTER_TEXT_ALIGN_CENTER)
		x_px -= XwaRemasterText_MeasureFlightString(font, text, size_px) * 0.5f;
	else if (align == XWA_REMASTER_TEXT_ALIGN_RIGHT)
		x_px -= XwaRemasterText_MeasureFlightString(font, text, size_px);
	uint32_t argb = initial_argb;
	int emitted = 0;
	for (const uint8_t* p = (const uint8_t*)text; *p; p++) {
		if (*p == 0xfeu && p[1]) {
			argb = XwaSnapshotExport_FlightPaletteColor(
				XwaSnapshotExport_FlightColorCodePaletteIndex(*++p));
			continue;
		}
		if (*p < 0x20u)
			continue;
		const float advance = text_flight_advance(font, *p, size_px);
		if (*p >= font->first_char && *p < font->first_char + font->num_chars) {
			const AeronFontGlyph* glyph = &font->glyphs[*p - font->first_char];
			if (glyph->atlas_w && glyph->atlas_h) {
				const float a = (float)((argb >> 24) & 255u) / 255.0f;
				AeronDrawList2DSprite sprite = { 0 };
				sprite.texture = font->texture;
				sprite.src_u0 = (float)glyph->atlas_x / font->atlas_w;
				sprite.src_v0 = (float)glyph->atlas_y / font->atlas_h;
				sprite.src_u1 = (float)(glyph->atlas_x + glyph->atlas_w) / font->atlas_w;
				sprite.src_v1 = (float)(glyph->atlas_y + glyph->atlas_h) / font->atlas_h;
				sprite.dst_x = x_px;
				sprite.dst_y = y_px;
				sprite.dst_w = size_px;
				sprite.dst_h = size_px;
				sprite.tint[0] = XwaRemaster_SrgbToLinear((float)((argb >> 16) & 255u) / 255.0f) * a;
				sprite.tint[1] = XwaRemaster_SrgbToLinear((float)((argb >> 8) & 255u) / 255.0f) * a;
				sprite.tint[2] = XwaRemaster_SrgbToLinear((float)(argb & 255u) / 255.0f) * a;
				sprite.tint[3] = a;
				sprite.blend = AERON_BLIT2D_BLEND_PMA;
				sprite.filter = AERON_BLIT2D_FILTER_LINEAR;
				if (scissor)
					sprite.scissor = *scissor;
				AeronDrawList_AddSprite(list, &sprite);
				emitted++;
			}
		}
		x_px += advance;
	}
	return emitted;
}

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
