#include "xwa_remaster/hud_text.h"

#include "aeron/scene/draw_list2d.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/hud_boxes.h"
#include "xwa_remaster/hud_fixed.h"
#include "xwa_remaster/hud_layout.h"

#include <math.h>
#include <string.h>

typedef struct XwaHudPreparedText {
	uint32_t layout_generation;
	uint32_t bundle_generation;
	uint16_t screen_w, screen_h;
	uint16_t pane_count, glyph_count;
	uint8_t profile;
	uint8_t valid;
	uint8_t arrow_tier;
	uint8_t reserved[3];
	uint16_t arrow_glyph_count;
	XwaHudPane panes[XWA_SNAP_MAX_HUD_PANES];
	XwaHudGlyph glyphs[XWA_SNAP_MAX_HUD_GLYPHS];
	struct {
		uint8_t ch;
		uint8_t reserved[3];
		float x_ref, y_ref, size_ref;
		uint32_t argb;
	} arrow_glyphs[512];
} XwaHudPreparedText;

static AeronDrawList2D* text_list;
static XwaHudPreparedText text_prepared;

static XwaHudLayoutOrigin text_origin_for_pane(uint16_t pane) {
	switch ((XwaHudPaneId)pane) {
		case XWA_HUD_PANE_LEFT_SUBSYSTEM:
		case XWA_HUD_PANE_NETWORK:
			return XWA_HUD_LAYOUT_LEFT_TOP;
		case XWA_HUD_PANE_RIGHT_SUBSYSTEM:
			return XWA_HUD_LAYOUT_RIGHT_TOP;
		case XWA_HUD_PANE_MFD_RIGHT_TITLE:
		case XWA_HUD_PANE_MFD_RIGHT_BODY:
			return XWA_HUD_LAYOUT_RIGHT_BOTTOM;
		case XWA_HUD_PANE_MFD_LEFT_TITLE:
		case XWA_HUD_PANE_MFD_LEFT_BODY:
			return XWA_HUD_LAYOUT_LEFT_BOTTOM;
		case XWA_HUD_PANE_CMD:
		case XWA_HUD_PANE_MESSAGE_FLIGHT_GROUP:
		case XWA_HUD_PANE_FPS:
			return XWA_HUD_LAYOUT_CENTER_BOTTOM;
		default:
			return XWA_HUD_LAYOUT_CENTER_TOP;
	}
}

static XwaHudAnchorId text_anchor_for_pane(uint16_t pane) {
	switch ((XwaHudPaneId)pane) {
		case XWA_HUD_PANE_TOP_SPEED:
			return XWA_HUD_ANCHOR_TOP_SPEED;
		case XWA_HUD_PANE_TOP_THROTTLE:
			return XWA_HUD_ANCHOR_TOP_THROTTLE;
		case XWA_HUD_PANE_TOP_CRAFT_NAME:
			return XWA_HUD_ANCHOR_TOP_CRAFT_NAME;
		case XWA_HUD_PANE_TOP_CLOCK:
			return XWA_HUD_ANCHOR_TOP_CLOCK;
		case XWA_HUD_PANE_TOP_WEAPONS:
			return XWA_HUD_ANCHOR_TOP_WEAPONS;
		case XWA_HUD_PANE_TOP_COUNTERMEASURE:
			return XWA_HUD_ANCHOR_TOP_COUNTERMEASURE;
		case XWA_HUD_PANE_TOP_PROVING_STATUS:
			return XWA_HUD_ANCHOR_TOP_PROVING_STATUS;
		case XWA_HUD_PANE_LEFT_SUBSYSTEM:
			return XWA_HUD_ANCHOR_LEFT_SUBSYSTEM;
		case XWA_HUD_PANE_RIGHT_SUBSYSTEM:
			return XWA_HUD_ANCHOR_RIGHT_SUBSYSTEM;
		case XWA_HUD_PANE_CMD:
			return XWA_HUD_ANCHOR_CMD_TEXT;
		case XWA_HUD_PANE_RETICLE_COUNTS:
			return XWA_HUD_ANCHOR_RETICLE;
		case XWA_HUD_PANE_MFD_LEFT_TITLE:
			return XWA_HUD_ANCHOR_MFD_LEFT_TITLE;
		case XWA_HUD_PANE_MFD_LEFT_BODY:
			return XWA_HUD_ANCHOR_MFD_LEFT_BODY;
		case XWA_HUD_PANE_MFD_RIGHT_TITLE:
			return XWA_HUD_ANCHOR_MFD_RIGHT_TITLE;
		case XWA_HUD_PANE_MFD_RIGHT_BODY:
			return XWA_HUD_ANCHOR_MFD_RIGHT_BODY;
		case XWA_HUD_PANE_MESSAGE_SYSTEM:
			return XWA_HUD_ANCHOR_MESSAGE_SYSTEM;
		case XWA_HUD_PANE_MESSAGE_FLIGHT_GROUP:
			return XWA_HUD_ANCHOR_MESSAGE_FLIGHT_GROUP;
		case XWA_HUD_PANE_MESSAGE_READY:
			return XWA_HUD_ANCHOR_MESSAGE_READY;
		case XWA_HUD_PANE_NETWORK:
			return XWA_HUD_ANCHOR_NETWORK;
		case XWA_HUD_PANE_FILM_RECORDING:
			return XWA_HUD_ANCHOR_FILM_RECORDING;
		case XWA_HUD_PANE_FPS:
			return XWA_HUD_ANCHOR_FPS;
		default:
			return XWA_HUD_ANCHOR_COUNT;
	}
}

static const XwaHudLayoutProfile* text_profile(const XwaHudLayout* layout) {
	return XwaRemasterHudLayout_Profile(layout, (XwaHudProfileIndex)text_prepared.profile);
}

int XwaRemasterHudText_MapGlyph(const XwaHudPane* pane, const XwaHudGlyph* glyph,
								XwaHudProfileIndex profile_id, int screen_w, int screen_h,
								XwaHudMappedGlyph* out) {
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	if (!pane || !glyph || !out || !layout || pane->id != glyph->pane || screen_w <= 0 || screen_h <= 0)
		return 0;
	const XwaHudAnchorId anchor_id = text_anchor_for_pane(glyph->pane);
	if (anchor_id >= XWA_HUD_ANCHOR_COUNT)
		return 0;
	const XwaHudLayoutProfile* profile = XwaRemasterHudLayout_Profile(layout, profile_id);
	if (!profile)
		return 0;
	const float source_pixel_scale = (float)profile->reference_h / screen_h;
	float x, y;
	XwaRemasterHudLayout_MapPoint(text_origin_for_pane(pane->id), screen_w, screen_h, profile->reference_w,
								  profile->reference_h, source_pixel_scale, pane->origin_x + glyph->x,
								  pane->origin_y + glyph->y, &x, &y);
	out->pane = glyph->pane;
	out->anchor = (uint16_t)anchor_id;
	out->x_ref = x;
	out->y_ref = y;
	out->w_ref = glyph->scale * source_pixel_scale;
	out->h_ref = glyph->scale * source_pixel_scale;
	return 1;
}

static int text_mapped_glyph_visible(const XwaHudLayout* layout, const XwaHudGlyph* glyph,
									 XwaHudProfileIndex profile_id, const XwaHudMappedGlyph* mapped) {
	const XwaHudLayoutProfile* profile = XwaRemasterHudLayout_Profile(layout, profile_id);
	if (!profile || glyph->scale == 0)
		return 0;
	const float clip_x0 = 0.0f;
	const float clip_y0 = 0.0f;
	const float clip_x1 = (float)profile->reference_w;
	const float clip_y1 = (float)profile->reference_h;
	const float classic_w = glyph->classic_w ? glyph->classic_w : glyph->scale;
	const float right = mapped->x_ref + classic_w * mapped->w_ref / glyph->scale;
	const float bottom = mapped->y_ref + mapped->h_ref;

	/* The normal classic hardware path drops the complete quad when any edge
	 * reaches or crosses the active flight viewport. Apply that decision after
	 * layout so each profile uses its own reference boundary. */
	return mapped->x_ref >= clip_x0 && mapped->y_ref >= clip_y0 && right < clip_x1 && bottom < clip_y1;
}

int XwaRemasterHudText_GlyphVisible(const XwaHudPane* pane, const XwaHudGlyph* glyph,
									XwaHudProfileIndex profile_id, int screen_w, int screen_h) {
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	XwaHudMappedGlyph mapped;
	return layout && XwaRemasterHudText_MapGlyph(pane, glyph, profile_id, screen_w, screen_h, &mapped) &&
		   text_mapped_glyph_visible(layout, glyph, profile_id, &mapped);
}

static const XwaHudPane* text_find_pane(uint16_t pane_id) {
	for (uint16_t i = 0; i < text_prepared.pane_count; i++)
		if (text_prepared.panes[i].id == pane_id)
			return &text_prepared.panes[i];
	return NULL;
}

static float text_arrow_advance(const XwaFlightFontRef* font, uint8_t ch, float size_ref) {
	if (!font || ch < font->first_char || ch >= font->first_char + font->num_chars || font->cell_h == 0)
		return size_ref * 0.6f;
	const AeronFontGlyph* glyph = &font->glyphs[ch - font->first_char];
	return glyph->advance ? (float)glyph->advance * size_ref / font->cell_h : size_ref * 0.5f;
}

static void text_append_arrow_glyph(uint8_t ch, float x, float y, float size, uint32_t argb) {
	if (text_prepared.arrow_glyph_count >= 512)
		return;
	const uint16_t i = text_prepared.arrow_glyph_count++;
	text_prepared.arrow_glyphs[i].ch = ch;
	text_prepared.arrow_glyphs[i].x_ref = x;
	text_prepared.arrow_glyphs[i].y_ref = y;
	text_prepared.arrow_glyphs[i].size_ref = size;
	text_prepared.arrow_glyphs[i].argb = argb;
}

static float text_measure_arrow_string(const XwaFlightFontRef* font, const char* str, float size_ref) {
	float width = 0.0f;
	for (const uint8_t* p = (const uint8_t*)str; *p; p++) {
		if (*p == 0xfeu && p[1]) {
			p++;
			continue;
		}
		if (*p >= 0x20u)
			width += text_arrow_advance(font, *p, size_ref);
	}
	return width;
}

static void text_build_arrow_string(const XwaFlightFontRef* font, const char* str, float x, float y,
									float size_ref, uint32_t initial_argb) {
	uint32_t argb = initial_argb;
	for (const uint8_t* p = (const uint8_t*)str; *p; p++) {
		if (*p == 0xfeu && p[1]) {
			argb = XwaSnapshotExport_FlightPaletteColor(XwaSnapshotExport_FlightColorCodePaletteIndex(*++p));
			continue;
		}
		if (*p < 0x20u)
			continue;
		text_append_arrow_glyph(*p, x, y, size_ref, argb);
		x += text_arrow_advance(font, *p, size_ref);
	}
}

static void text_format_decimal(uint16_t value, unsigned int width, unsigned int min_digits, char out[4]) {
	if (width > 3)
		width = 3;
	if (value == 0xffffu) {
		for (unsigned int i = 0; i < width; i++)
			out[i] = '0';
		out[width] = '\0';
		return;
	}
	uint16_t remaining = value;
	int emitted = 0;
	for (unsigned int i = 0; i < width; i++) {
		unsigned int places = width - i - 1;
		uint16_t divisor = 1;
		while (places--)
			divisor = (uint16_t)(divisor * 10u);
		uint16_t digit = remaining / divisor;
		remaining = (uint16_t)(remaining - divisor * digit);
		if (emitted || width - i <= min_digits || digit != 0) {
			emitted = 1;
			if (digit > 9)
				digit = 9;
			out[i] = (char)('0' + digit);
		} else {
			out[i] = ' ';
		}
	}
	out[width] = '\0';
}

static void text_build_target_arrow(const XwaSnapshot* snapshot, XwaHudProfileIndex profile) {
	XwaRemasterHudVisibility visibility;
	XwaRemasterHud_BuildVisibility(&snapshot->hud, &visibility);
	if (!visibility.target_arrow || !snapshot->hud.target.valid ||
		!(snapshot->hud.instruments.working_subsystems & 4u))
		return;
	const XwaHudProjectedArrow* prepared_arrow = XwaRemasterHudFixed_TargetArrow();
	if (!prepared_arrow)
		return;
	const XwaHudProjectedArrow arrow = *prepared_arrow;
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	const XwaHudLayoutProfile* active = XwaRemasterHudLayout_Profile(layout, profile);
	if (!active)
		return;
	const XwaHudLayoutRect* bounds = &active->anchors[XWA_HUD_ANCHOR_TARGET_EDGE_BOUNDS].rect;
	const float screen_w = snapshot->flight_camera.screen_w ? snapshot->flight_camera.screen_w : 640.0f;
	const float screen_h = snapshot->flight_camera.screen_h ? snapshot->flight_camera.screen_h : 480.0f;
	const float px_x = (float)active->reference_h / screen_h;
	const float px_y = (float)active->reference_h / screen_h;
	int font_scale = (int)(snapshot->hud.classic_hud_scale * 10.0f);
	if (font_scale < 1)
		font_scale = 1;
	const int tier = font_scale < 12 ? 2 : (font_scale < 15 ? 1 : 0);
	const XwaFlightFontRef* font = XwaRemasterHud_FlightFont(tier);
	text_prepared.arrow_tier = (uint8_t)tier;
	const float size_ref = font_scale * px_y;
	const float line_h = (font_scale - (font_scale >> 2)) * px_y;
	const float name_w = text_measure_arrow_string(font, snapshot->hud.target.name, size_ref);
	const float local_x = arrow.center_x_ref - bounds->x;
	const float local_y = arrow.center_y_ref - bounds->y;
	const float pad_x = 6.0f * px_x;
	const float pad_y = 6.0f * px_y;
	float name_x = 0.0f, name_y = 0.0f, distance_x = 0.0f, distance_y = 0.0f;
	if (fabsf(local_x - pad_x) < px_x * 1.5f) {
		name_x = 12.0f * px_x;
		distance_x = 9.0f * px_x;
		name_y = -line_h;
	} else if (fabsf(local_x - (bounds->w - pad_x)) < px_x * 1.5f) {
		name_x = -name_w - 15.0f * px_x;
		distance_x = -35.0f * px_x;
		name_y = -line_h;
	} else if (fabsf(local_y - (bounds->h - pad_y)) < px_y * 1.5f) {
		name_x = -name_w * 0.5f;
		name_y = -15.0f * px_y - line_h;
		distance_x = -12.0f * px_x;
		distance_y = -15.0f * px_y;
	} else {
		name_x = -name_w * 0.5f;
		name_y = line_h + 5.0f * px_y;
		distance_x = -12.0f * px_x;
		distance_y = 7.0f * px_y;
	}
	if (!arrow.padlock && local_y <= 10.0f * px_y) {
		const float adjust = 10.0f * px_y - local_y;
		name_y += adjust;
		distance_y += adjust;
	}
	if (local_y >= bounds->h - 10.0f * px_y) {
		const float adjust = bounds->h - local_y - 10.0f * px_y;
		name_y += adjust;
		distance_y += adjust;
	}
	if (local_x <= -name_x)
		name_x = -local_x;
	const int horizontal_edge =
		fabsf(local_y - pad_y) < px_y * 1.5f || fabsf(local_y - (bounds->h - pad_y)) < px_y * 1.5f;
	const int vertical_edge =
		fabsf(local_x - pad_x) < px_x * 1.5f || fabsf(local_x - (bounds->w - pad_x)) < px_x * 1.5f;
	if (local_x - name_x >= bounds->w && horizontal_edge && !vertical_edge)
		name_x = bounds->w + 2.0f * name_x - local_x;
	text_build_arrow_string(font, snapshot->hud.target.name, arrow.center_x_ref + name_x,
							arrow.center_y_ref + name_y, size_ref, snapshot->hud.hud_colors[0]);
	const uint32_t distance_argb = XwaSnapshotExport_FlightPaletteColor(
		XwaSnapshotExport_FlightColorCodePaletteIndex(arrow.behind ? 0x4d : 0x4e));
	char whole[4], fraction[4];
	text_format_decimal(snapshot->hud.target.distance_whole, 2, 1, whole);
	text_format_decimal(snapshot->hud.target.distance_frac, 2, 2, fraction);
	const float distance_base_x = arrow.center_x_ref + distance_x;
	const float distance_base_y = arrow.center_y_ref + distance_y;
	const uint32_t whole_argb =
		snapshot->hud.target.distance_whole == 0xffffu
			? XwaSnapshotExport_FlightPaletteColor(XwaSnapshotExport_FlightColorCodePaletteIndex('@'))
			: distance_argb;
	text_build_arrow_string(font, whole, distance_base_x, distance_base_y, size_ref, whole_argb);
	text_build_arrow_string(font, ".", distance_base_x + 10.0f * px_x, distance_base_y, size_ref,
							distance_argb);
	text_build_arrow_string(font, fraction, distance_base_x + 13.0f * px_x, distance_base_y, size_ref,
							distance_argb);
}

static void text_build_target_box_readouts(const XwaSnapshot* snapshot) {
	const XwaHudPreparedBoxState* boxes = XwaRemasterHudBoxes_Prepared();
	if (!boxes || !boxes->valid || !snapshot->hud.target.valid)
		return;
	int font_scale = (int)(snapshot->hud.classic_hud_scale * 10.0f);
	if (font_scale < 1)
		font_scale = 1;
	const int tier = font_scale < 12 ? 2 : (font_scale < 15 ? 1 : 0);
	const XwaFlightFontRef* font = XwaRemasterHud_FlightFont(tier);
	text_prepared.arrow_tier = (uint8_t)tier;
	const uint32_t name_argb =
		XwaSnapshotExport_FlightPaletteColor(XwaSnapshotExport_FlightColorCodePaletteIndex(0x43u));
	const uint32_t value_argb =
		XwaSnapshotExport_FlightPaletteColor(XwaSnapshotExport_FlightColorCodePaletteIndex(0x47u));
	for (uint16_t i = 0; i < boxes->box_count; i++) {
		const XwaHudPreparedBox* box = &boxes->boxes[i];
		if (!box->readout)
			continue;
		const float px = box->classic_pixel_ref;
		const float size_ref = font_scale * px;
		const float line_h = (font_scale - (font_scale >> 2)) * px;
		const float center_x = box->x_ref + box->w_ref * 0.5f;
		const float bottom_y = box->y_ref + box->h_ref;
		float width = text_measure_arrow_string(font, snapshot->hud.target.name, size_ref);
		text_build_arrow_string(font, snapshot->hud.target.name, center_x - width * 0.5f, box->y_ref - line_h,
								size_ref, name_argb);

		char digits[4];
		const float upper_y = box->y_ref - 2.0f * px;
		const float lower_y = bottom_y - line_h + 2.0f * px;
		text_format_decimal(snapshot->hud.target.hull_pct, 3, 1, digits);
		width = text_measure_arrow_string(font, "100", size_ref);
		text_build_arrow_string(font, digits, box->x_ref - width, lower_y, size_ref, value_argb);
		text_format_decimal(snapshot->hud.target.system_pct, 3, 1, digits);
		text_build_arrow_string(font, digits, box->x_ref + box->w_ref + 3.0f * px, upper_y, size_ref,
								value_argb);
		text_format_decimal(snapshot->hud.target.shield_pct, 3, 1, digits);
		width = text_measure_arrow_string(font, "200", size_ref);
		text_build_arrow_string(font, digits, box->x_ref - width, upper_y, size_ref, value_argb);
		text_format_decimal(snapshot->hud.target.distance_whole, 2, 1, digits);
		const float distance_x = box->x_ref + box->w_ref;
		text_build_arrow_string(font, digits, distance_x, lower_y, size_ref, value_argb);
		text_build_arrow_string(font, ".", distance_x + 10.0f * px, lower_y, size_ref, value_argb);
		text_format_decimal(snapshot->hud.target.distance_frac, 2, 2, digits);
		text_build_arrow_string(font, digits, distance_x + 13.0f * px, lower_y, size_ref, value_argb);
		width = text_measure_arrow_string(font, snapshot->hud.target.status, size_ref);
		text_build_arrow_string(font, snapshot->hud.target.status, center_x - width * 0.5f, bottom_y,
								size_ref, value_argb);
	}
}

int XwaRemasterHudText_Init(void) {
	if (text_list)
		return 1;
	text_list = AeronDrawList_Create(XWA_SNAP_MAX_HUD_GLYPHS + 128);
	return text_list != NULL;
}

void XwaRemasterHudText_Shutdown(void) {
	AeronDrawList_Destroy(text_list);
	text_list = NULL;
	memset(&text_prepared, 0, sizeof text_prepared);
}

void XwaRemasterHudText_Build(const XwaSnapshot* snapshot, XwaHudProfileIndex profile,
							  uint32_t bundle_generation) {
	memset(&text_prepared, 0, sizeof text_prepared);
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	if (!snapshot || !layout || !snapshot->hud.valid || !XwaRemasterHudLayout_Profile(layout, profile))
		return;
	text_prepared.layout_generation = layout->generation;
	text_prepared.bundle_generation = bundle_generation;
	text_prepared.profile = (uint8_t)profile;
	text_prepared.screen_w = snapshot->flight_camera.screen_w ? snapshot->flight_camera.screen_w : 640;
	text_prepared.screen_h = snapshot->flight_camera.screen_h ? snapshot->flight_camera.screen_h : 480;
	text_prepared.pane_count = snapshot->hud.pane_count;
	text_prepared.glyph_count = snapshot->hud.glyph_count;
	memcpy(text_prepared.panes, snapshot->hud.panes,
		   (size_t)text_prepared.pane_count * sizeof text_prepared.panes[0]);
	for (uint16_t i = 0; i < text_prepared.pane_count; i++) {
		XwaHudPane* pane = &text_prepared.panes[i];
		if (pane->id == XWA_HUD_PANE_RETICLE_COUNTS) {
			float center_x_ref, center_y_ref;
			const XwaHudLayoutProfile* active = XwaRemasterHudLayout_Profile(layout, profile);
			const float pixel_scale = (float)active->reference_h / text_prepared.screen_h;
			if (XwaRemasterHudFixed_ReticleCenter(&center_x_ref, &center_y_ref) && pixel_scale > 0.0f) {
				pane->origin_x = (int16_t)lroundf(text_prepared.screen_w * 0.5f +
												  (center_x_ref - active->reference_w * 0.5f) / pixel_scale);
				pane->origin_y = (int16_t)lroundf(center_y_ref / pixel_scale);
			}
		}
	}
	memcpy(text_prepared.glyphs, snapshot->hud.glyphs,
		   (size_t)text_prepared.glyph_count * sizeof text_prepared.glyphs[0]);
	text_build_target_arrow(snapshot, profile);
	text_build_target_box_readouts(snapshot);
	if (text_prepared.glyph_count == 0 && text_prepared.arrow_glyph_count == 0)
		return;
	text_prepared.valid = 1;
}

void XwaRemasterHudText_PrepareDrawList(AeronCommandBuffer* cmd, int target_w, int target_h) {
	if (!cmd || !text_list || target_w <= 0 || target_h <= 0)
		return;
	AeronDrawList_Begin(text_list, NULL, target_w, target_h, AERON_DRAWLIST2D_LOAD, NULL);
	if (!text_prepared.valid) {
		(void)AeronDrawList_Prepare(text_list, cmd);
		return;
	}
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	if (!layout || text_prepared.layout_generation != layout->generation) {
		(void)AeronDrawList_Prepare(text_list, cmd);
		return;
	}
	const XwaHudLayoutProfile* profile = text_profile(layout);
	if (!profile) {
		(void)AeronDrawList_Prepare(text_list, cmd);
		return;
	}
	int out_x, out_y;
	float output_scale;
	XwaRemasterHudLayout_OutputTransform(profile, target_w, target_h, &out_x, &out_y, &output_scale);
	if (output_scale <= 0.0f) {
		(void)AeronDrawList_Prepare(text_list, cmd);
		return;
	}
	XwaRemasterHud_BeginRenderPhase();
	if (text_prepared.arrow_glyph_count) {
		const XwaFlightFontRef* font = XwaRemasterHud_FlightFont(text_prepared.arrow_tier);
		if (font && font->texture) {
			for (uint16_t i = 0; i < text_prepared.arrow_glyph_count; i++) {
				const uint8_t ch = text_prepared.arrow_glyphs[i].ch;
				if (ch < font->first_char || ch >= font->first_char + font->num_chars)
					continue;
				const AeronFontGlyph* metrics = &font->glyphs[ch - font->first_char];
				if (!metrics->atlas_w || !metrics->atlas_h || font->atlas_w <= 0 || font->atlas_h <= 0)
					continue;
				const uint32_t argb = text_prepared.arrow_glyphs[i].argb;
				const float a = ((argb >> 24) & 255) / 255.0f;
				AeronDrawList2DSprite sprite = { 0 };
				sprite.texture = font->texture;
				sprite.src_u0 = (float)metrics->atlas_x / font->atlas_w;
				sprite.src_v0 = (float)metrics->atlas_y / font->atlas_h;
				sprite.src_u1 = (float)(metrics->atlas_x + metrics->atlas_w) / font->atlas_w;
				sprite.src_v1 = (float)(metrics->atlas_y + metrics->atlas_h) / font->atlas_h;
				sprite.dst_x = out_x + text_prepared.arrow_glyphs[i].x_ref * output_scale;
				sprite.dst_y = out_y + text_prepared.arrow_glyphs[i].y_ref * output_scale;
				sprite.dst_w = text_prepared.arrow_glyphs[i].size_ref * output_scale;
				sprite.dst_h = text_prepared.arrow_glyphs[i].size_ref * output_scale;
				sprite.tint[0] = XwaRemaster_SrgbToLinear(((argb >> 16) & 255) / 255.0f) * a;
				sprite.tint[1] = XwaRemaster_SrgbToLinear(((argb >> 8) & 255) / 255.0f) * a;
				sprite.tint[2] = XwaRemaster_SrgbToLinear((argb & 255) / 255.0f) * a;
				sprite.tint[3] = a;
				sprite.blend = AERON_BLIT2D_BLEND_PMA;
				sprite.filter = AERON_BLIT2D_FILTER_LINEAR;
				AeronDrawList_AddSprite(text_list, &sprite);
			}
		}
	}
	for (uint16_t i = 0; i < text_prepared.glyph_count; i++) {
		const XwaHudGlyph* glyph = &text_prepared.glyphs[i];
		const XwaHudPane* pane = text_find_pane(glyph->pane);
		XwaHudMappedGlyph mapped;
		if (glyph->font_tier >= 3 || glyph->scale == 0 ||
			!XwaRemasterHudText_MapGlyph(pane, glyph, (XwaHudProfileIndex)text_prepared.profile,
										 text_prepared.screen_w, text_prepared.screen_h, &mapped) ||
			!text_mapped_glyph_visible(layout, glyph, (XwaHudProfileIndex)text_prepared.profile, &mapped))
			continue;
		const XwaFlightFontRef* font = XwaRemasterHud_FlightFont(glyph->font_tier);
		if (!font || !font->texture || glyph->ch < font->first_char ||
			glyph->ch >= font->first_char + font->num_chars)
			continue;
		const AeronFontGlyph* metrics = &font->glyphs[glyph->ch - font->first_char];
		if (!metrics->atlas_w || !metrics->atlas_h || font->atlas_w <= 0 || font->atlas_h <= 0)
			continue;
		const float a = ((glyph->argb >> 24) & 255) / 255.0f;
		AeronDrawList2DSprite sprite = { 0 };
		sprite.texture = font->texture;
		sprite.src_u0 = (float)metrics->atlas_x / font->atlas_w;
		sprite.src_v0 = (float)metrics->atlas_y / font->atlas_h;
		sprite.src_u1 = (float)(metrics->atlas_x + metrics->atlas_w) / font->atlas_w;
		sprite.src_v1 = (float)(metrics->atlas_y + metrics->atlas_h) / font->atlas_h;
		sprite.dst_x = out_x + mapped.x_ref * output_scale;
		sprite.dst_y = out_y + mapped.y_ref * output_scale;
		sprite.dst_w = mapped.w_ref * output_scale;
		sprite.dst_h = mapped.h_ref * output_scale;
		sprite.tint[0] = XwaRemaster_SrgbToLinear(((glyph->argb >> 16) & 255) / 255.0f) * a;
		sprite.tint[1] = XwaRemaster_SrgbToLinear(((glyph->argb >> 8) & 255) / 255.0f) * a;
		sprite.tint[2] = XwaRemaster_SrgbToLinear((glyph->argb & 255) / 255.0f) * a;
		sprite.tint[3] = a;
		sprite.blend = AERON_BLIT2D_BLEND_PMA;
		sprite.filter = AERON_BLIT2D_FILTER_LINEAR;
		AeronDrawList_AddSprite(text_list, &sprite);
	}
	XwaRemasterHud_EndRenderPhase();
	(void)AeronDrawList_Prepare(text_list, cmd);
}

void XwaRemasterHudText_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
							   int target_w, int target_h) {
	if (!cmd || !pass || !target || !text_list || !text_prepared.valid || target_w <= 0 ||
		target_h <= 0)
		return;
	AeronDrawList_RenderIntoPass(text_list, cmd, pass, target);
}
