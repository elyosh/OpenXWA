#include "xwa_remaster/text.h"

#include "xwa_remaster/color.h"
#include "xwa_runtime/frontend/frontend_fonts.h"

#include <math.h>
#include <string.h>

static float text_flight_advance(const AeronFontAtlas* font, uint8_t ch, float size_px) {
	if (!font || !font->glyphs || font->cell_h == 0 || ch < font->first_char ||
		ch >= font->first_char + font->num_chars) {
		return size_px * 0.6f;
	}
	const AeronFontGlyph* glyph = &font->glyphs[ch - font->first_char];
	return glyph->advance ? (float)glyph->advance * size_px / font->cell_h : size_px * 0.5f;
}

/* Fills `sprite` with glyph `ch` of a TrueType font atlas, drawn 1:1 at whole
 * pixels. The glyph is centred in `layout_advance`, the space that the layout
 * gave it from `pen_x`, and sits on the baseline at `baseline_y`. Returns 0
 * when the font has no glyph for `ch`. */
static int text_face_sprite(const AeronFontAtlas* atlas, const XwaFrontendFont* font, uint8_t ch, float em_px,
							float pen_x, float layout_advance, float baseline_y,
							AeronDrawList2DSprite* sprite) {
	float advance;

	if (ch < atlas->first_char || ch >= atlas->first_char + atlas->num_chars ||
		!AeronFontFace_Advance(font->face, font->codepoints[ch], &advance)) {
		return 0;
	}
	const AeronFontGlyph* metrics = &atlas->glyphs[ch - atlas->first_char];
	if (metrics->atlas_w == 0 || metrics->atlas_h == 0) {
		return 0;
	}
	advance *= em_px;
	sprite->texture = atlas->texture;
	sprite->filter = AERON_BLIT2D_FILTER_NEAREST;
	sprite->blend = AERON_BLIT2D_BLEND_PMA;
	sprite->src_u0 = (float)metrics->atlas_x / (float)atlas->atlas_w;
	sprite->src_v0 = (float)metrics->atlas_y / (float)atlas->atlas_h;
	sprite->src_u1 = (float)(metrics->atlas_x + metrics->atlas_w) / (float)atlas->atlas_w;
	sprite->src_v1 = (float)(metrics->atlas_y + metrics->atlas_h) / (float)atlas->atlas_h;
	sprite->dst_x = floorf(pen_x + (layout_advance - advance) * 0.5f + 0.5f) + (float)metrics->offset_x;
	sprite->dst_y = floorf(baseline_y + 0.5f) + (float)metrics->offset_y;
	sprite->dst_w = (float)metrics->atlas_w;
	sprite->dst_h = (float)metrics->atlas_h;
	return 1;
}

/* ---- flight fonts ------------------------------------------------------ */

int XwaRemasterText_FlightTierFontSize(int tier) { return tier == 0 ? 16 : (tier == 1 ? 12 : 10); }

int XwaRemasterText_FlightFaceMetrics(int tier, XwaRemasterFlightFace* out) {
	const int font_size = XwaRemasterText_FlightTierFontSize(tier);
	const XwaFrontendFont* font = XwaFlightFonts_Get(font_size);
	if (!out || !font)
		return 0;
	memset(out, 0, sizeof *out);
	out->font = font;
	out->font_size = font_size;
	return 1;
}

void XwaRemasterText_PrepareFlightFace(AeronCommandBuffer* cmd, XwaRemasterAssets* assets, int tier,
									   float size_px) {
	XwaRemasterFlightFace face;
	if (size_px > 0.0f && XwaRemasterText_FlightFaceMetrics(tier, &face))
		XwaRemasterAssets_PrepareFlightFaceFont(assets, cmd, face.font_size,
												face.font->em_units * size_px / (float)face.font_size);
}

int XwaRemasterText_FlightFace(XwaRemasterAssets* assets, int tier, float size_px,
							   XwaRemasterFlightFace* out) {
	if (!(size_px > 0.0f) || !XwaRemasterText_FlightFaceMetrics(tier, out))
		return 0;
	out->unit_px = size_px / (float)out->font_size;
	out->em_px = out->font->em_units * out->unit_px;
	out->atlas = XwaRemasterAssets_FlightFaceFont(assets, out->font_size, out->em_px);
	return out->atlas != NULL;
}

float XwaRemasterText_FlightAdvance(const AeronFontAtlas* bitmap, const XwaRemasterFlightFace* face,
									uint8_t ch, float size_px) {
	if (face && XwaFlightFonts_UsesFontMetrics(face->font_size)) {
		/* The whole-pixel width that the game's width tables get. */
		const float advance = floorf(XwaFlightFonts_Advance(face->font_size, ch));
		if (advance > 0.0f)
			return advance * size_px / (float)face->font_size;
	}
	return text_flight_advance(bitmap, ch, size_px);
}

int XwaRemasterText_FlightFaceSprite(const XwaRemasterFlightFace* face, uint8_t ch, float pen_x, float top_y,
									 float layout_advance, AeronDrawList2DSprite* out) {
	if (!face || !face->atlas || !out)
		return 0;
	return text_face_sprite(face->atlas, face->font, ch, face->em_px, pen_x, layout_advance,
							top_y + face->font->baseline_units * face->unit_px, out);
}

float XwaRemasterText_MeasureFlightString(const AeronFontAtlas* font, const XwaRemasterFlightFace* face,
										  const char* text, float size_px) {
	float width = 0.0f;
	if (!text || size_px <= 0.0f)
		return 0.0f;
	for (const uint8_t* p = (const uint8_t*)text; *p; p++) {
		if (*p == 0xfeu && p[1]) {
			p++;
			continue;
		}
		if (*p >= 0x20u)
			width += XwaRemasterText_FlightAdvance(font, face, *p, size_px);
	}
	return width;
}

int XwaRemasterText_AddFlightString(AeronDrawList2D* list, const AeronFontAtlas* font,
									const XwaRemasterFlightFace* face, const char* text, float x_px,
									float y_px, float size_px, XwaRemasterTextAlign align,
									uint32_t initial_argb, const AeronRectI* scissor) {
	const int use_face = face && face->atlas;
	if (!list || !text || size_px <= 0.0f ||
		(!use_face && (!font || !font->texture || !font->glyphs || font->atlas_w <= 0 || font->atlas_h <= 0)))
		return 0;
	if (align == XWA_REMASTER_TEXT_ALIGN_CENTER)
		x_px -= XwaRemasterText_MeasureFlightString(font, face, text, size_px) * 0.5f;
	else if (align == XWA_REMASTER_TEXT_ALIGN_RIGHT)
		x_px -= XwaRemasterText_MeasureFlightString(font, face, text, size_px);
	uint32_t argb = initial_argb;
	int emitted = 0;
	for (const uint8_t* p = (const uint8_t*)text; *p; p++) {
		if (*p == 0xfeu && p[1]) {
			argb = XwaSnapshotExport_FlightPaletteColor(XwaSnapshotExport_FlightColorCodePaletteIndex(*++p));
			continue;
		}
		if (*p < 0x20u)
			continue;
		const float advance = XwaRemasterText_FlightAdvance(font, face, *p, size_px);
		AeronDrawList2DSprite sprite = { 0 };
		int drawn = 0;
		if (use_face) {
			drawn = XwaRemasterText_FlightFaceSprite(face, *p, x_px, y_px, advance, &sprite);
		} else if (*p >= font->first_char && *p < font->first_char + font->num_chars) {
			const AeronFontGlyph* glyph = &font->glyphs[*p - font->first_char];
			if (glyph->atlas_w && glyph->atlas_h) {
				sprite.texture = font->texture;
				sprite.src_u0 = (float)glyph->atlas_x / font->atlas_w;
				sprite.src_v0 = (float)glyph->atlas_y / font->atlas_h;
				sprite.src_u1 = (float)(glyph->atlas_x + glyph->atlas_w) / font->atlas_w;
				sprite.src_v1 = (float)(glyph->atlas_y + glyph->atlas_h) / font->atlas_h;
				sprite.dst_x = x_px;
				sprite.dst_y = y_px;
				sprite.dst_w = size_px;
				sprite.dst_h = size_px;
				sprite.blend = AERON_BLIT2D_BLEND_PMA;
				sprite.filter = AERON_BLIT2D_FILTER_LINEAR;
				drawn = 1;
			}
		}
		if (drawn) {
			const float a = (float)((argb >> 24) & 255u) / 255.0f;
			sprite.tint[0] = XwaRemaster_SrgbToLinear((float)((argb >> 16) & 255u) / 255.0f) * a;
			sprite.tint[1] = XwaRemaster_SrgbToLinear((float)((argb >> 8) & 255u) / 255.0f) * a;
			sprite.tint[2] = XwaRemaster_SrgbToLinear((float)(argb & 255u) / 255.0f) * a;
			sprite.tint[3] = a;
			if (scissor)
				sprite.scissor = *scissor;
			AeronDrawList_AddSprite(list, &sprite);
			emitted++;
		}
		x_px += advance;
	}
	return emitted;
}

static int text_x_edge(int coordinate, const XwaRemasterTextMapping* m) {
	return (int)lroundf(m->offset_x + (float)coordinate * m->scale_x);
}

static int text_y_edge(int coordinate, const XwaRemasterTextMapping* m) {
	return (int)lroundf((float)coordinate * m->scale_y);
}

static AeronRectI text_scissor(const XwaGlyph2D* glyph, const XwaRemasterTextMapping* m) {
	AeronRectI rect = { 0, 0, 0, 0 };
	if (glyph->clip_left <= 0 && glyph->clip_top <= 0 && glyph->clip_right >= XWA_CLASSIC_WIDTH - 1 &&
		glyph->clip_bottom >= XWA_CLASSIC_HEIGHT - 1) {
		return rect;
	}
	rect.x = text_x_edge(glyph->clip_left, m);
	rect.y = text_y_edge(glyph->clip_top, m);
	rect.width = text_x_edge(glyph->clip_right + 1, m) - rect.x;
	rect.height = text_y_edge(glyph->clip_bottom + 1, m) - rect.y;
	return rect;
}

void XwaRemasterText_PrepareFrontendFonts(AeronCommandBuffer* cmd, XwaRemasterAssets* assets,
										  const XwaSnapshot* snap, float scale_y) {
	uint8_t seen[256] = { 0 };
	for (uint32_t i = 0; snap && i < snap->glyph_count; i++) {
		const int font_size = snap->glyphs[i].font_size;
		if (font_size < 0 || font_size > 255 || seen[font_size])
			continue;
		seen[font_size] = 1;
		const XwaFrontendFont* font = XwaFrontendFonts_Get(font_size);
		if (font)
			XwaRemasterAssets_PrepareFaceFont(assets, cmd, font_size, font->em_units * scale_y);
	}
}

/* A glyph of a font from frontend/fonts/fonts.yaml, drawn 1:1 at whole
 * pixels. It is centred in the advance the layout used. */
static int text_add_face_glyph(AeronDrawList2D* list, const AeronFontAtlas* atlas,
							   const XwaFrontendFont* font, const XwaGlyph2D* glyph,
							   const XwaRemasterTextMapping* mapping) {
	AeronDrawList2DSprite sprite = { 0 };
	uint8_t rgba[4];
	const float pen =
		mapping->offset_x + ((float)glyph->x + (float)glyph->x_frac / 256.0f) * mapping->scale_x;
	const float layout_advance = (float)glyph->advance256 / 256.0f * mapping->scale_x;

	if (!text_face_sprite(atlas, font, glyph->ch, font->em_units * mapping->scale_y, pen, layout_advance,
						  ((float)glyph->y + font->baseline_units) * mapping->scale_y, &sprite)) {
		return 0;
	}
	sprite.scissor = text_scissor(glyph, mapping);
	XwaSnapshotExport_ColorToRgba(glyph->color, rgba);
	sprite.tint[0] = XwaRemaster_SrgbToLinear((float)rgba[0] / 255.0f);
	sprite.tint[1] = XwaRemaster_SrgbToLinear((float)rgba[1] / 255.0f);
	sprite.tint[2] = XwaRemaster_SrgbToLinear((float)rgba[2] / 255.0f);
	sprite.tint[3] = 1.0f;
	AeronDrawList_AddSprite(list, &sprite);
	return 1;
}

int XwaRemasterText_AddFrontendGlyph(AeronDrawList2D* list, XwaRemasterAssets* assets,
									 const XwaGlyph2D* glyph, int target_width, int target_height) {
	if (target_width <= 0 || target_height <= 0) {
		return 0;
	}
	const XwaRemasterTextMapping mapping = { (float)target_width / (float)XWA_CLASSIC_WIDTH,
											 (float)target_height / (float)XWA_CLASSIC_HEIGHT, 0.0f };
	return XwaRemasterText_AddFrontendGlyphMapped(list, assets, glyph, &mapping);
}

int XwaRemasterText_AddFrontendGlyphMapped(AeronDrawList2D* list, XwaRemasterAssets* assets,
										   const XwaGlyph2D* glyph, const XwaRemasterTextMapping* mapping) {
	float atlas_scale = 1.0f;
	const AeronFontAtlas* font;
	const AeronFontGlyph* metrics;
	AeronDrawList2DSprite sprite = { 0 };
	uint8_t rgba[4];
	float scale_y;

	if (!list || !assets || !glyph || !mapping || mapping->scale_x <= 0.0f || mapping->scale_y <= 0.0f) {
		return 0;
	}
	{
		const XwaFrontendFont* face_font = XwaFrontendFonts_Get(glyph->font_size);
		const AeronFontAtlas* atlas =
			face_font
				? XwaRemasterAssets_FaceFont(assets, glyph->font_size, face_font->em_units * mapping->scale_y)
				: NULL;
		if (atlas) {
			return text_add_face_glyph(list, atlas, face_font, glyph, mapping);
		}
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

	scale_y = mapping->scale_y;
	sprite.texture = font->texture;
	sprite.filter = AERON_BLIT2D_FILTER_LINEAR;
	sprite.blend = AERON_BLIT2D_BLEND_PMA;
	sprite.scissor = text_scissor(glyph, mapping);
	sprite.src_u0 = (float)metrics->atlas_x / (float)font->atlas_w;
	sprite.src_v0 = (float)metrics->atlas_y / (float)font->atlas_h;
	sprite.src_u1 = (float)(metrics->atlas_x + metrics->atlas_w) / (float)font->atlas_w;
	sprite.src_v1 = (float)(metrics->atlas_y + metrics->atlas_h) / (float)font->atlas_h;
	/* The position uses the mapping, plus the sub-unit fraction the game keeps
	 * on wide screens. The size uses scale_y on both axes, so the glyph isn't
	 * stretched. */
	sprite.dst_x = mapping->offset_x + ((float)glyph->x + (float)glyph->x_frac / 256.0f) * mapping->scale_x;
	sprite.dst_y = (float)glyph->y * scale_y;
	sprite.dst_w = (float)metrics->atlas_w * scale_y / atlas_scale;
	sprite.dst_h = (float)metrics->atlas_h * scale_y / atlas_scale;
	XwaSnapshotExport_ColorToRgba(glyph->color, rgba);
	sprite.tint[0] = XwaRemaster_SrgbToLinear((float)rgba[0] / 255.0f);
	sprite.tint[1] = XwaRemaster_SrgbToLinear((float)rgba[1] / 255.0f);
	sprite.tint[2] = XwaRemaster_SrgbToLinear((float)rgba[2] / 255.0f);
	sprite.tint[3] = 1.0f;
	AeronDrawList_AddSprite(list, &sprite);
	return 1;
}
