#include "xwa_remaster/hud.h"
#include "xwa_remaster/hud_boxes.h"
#include "xwa_remaster/hud_cmd.h"
#include "xwa_remaster/hud_fixed.h"
#include "xwa_remaster/hud_layout.h"
#include "xwa_remaster/hud_text.h"

#include "aeron/aeron.h"
#include "xwa/assets/object_type.h"

#include <stdio.h>
#include <string.h>

#define WIDGET(id_, anchor_, kind_, phase_, order_, asset_) { id_, anchor_, kind_, phase_, order_, asset_ }

static const XwaHudWidgetDesc hud_widgets[] = {
	WIDGET(XWA_HUD_WIDGET_FORE_RADAR_FRAME, XWA_HUD_ANCHOR_FORE_RADAR, XWA_HUD_WIDGET_KIND_SPRITE_GROUP,
		   XWA_HUD_PHASE_FRAMES, 10, XWA_HUD_ASSET_RADAR_FRAME),
	WIDGET(XWA_HUD_WIDGET_AFT_RADAR_FRAME, XWA_HUD_ANCHOR_AFT_RADAR, XWA_HUD_WIDGET_KIND_SPRITE_GROUP,
		   XWA_HUD_PHASE_FRAMES, 20, XWA_HUD_ASSET_RADAR_FRAME),
	WIDGET(XWA_HUD_WIDGET_FORE_RADAR_SCOPE, XWA_HUD_ANCHOR_FORE_RADAR, XWA_HUD_WIDGET_KIND_SPRITE_GROUP,
		   XWA_HUD_PHASE_FRAMES, 30, XWA_HUD_ASSET_RADAR_SCOPE),
	WIDGET(XWA_HUD_WIDGET_AFT_RADAR_SCOPE, XWA_HUD_ANCHOR_AFT_RADAR, XWA_HUD_WIDGET_KIND_SPRITE_GROUP,
		   XWA_HUD_PHASE_FRAMES, 40, XWA_HUD_ASSET_RADAR_SCOPE),
	WIDGET(XWA_HUD_WIDGET_FORE_RADAR_BLIPS, XWA_HUD_ANCHOR_FORE_RADAR, XWA_HUD_WIDGET_KIND_RADAR_BLIPS,
		   XWA_HUD_PHASE_RADAR, 50, XWA_HUD_ASSET_NONE),
	WIDGET(XWA_HUD_WIDGET_AFT_RADAR_BLIPS, XWA_HUD_ANCHOR_AFT_RADAR, XWA_HUD_WIDGET_KIND_RADAR_BLIPS,
		   XWA_HUD_PHASE_RADAR, 60, XWA_HUD_ASSET_NONE),
	WIDGET(XWA_HUD_WIDGET_FORE_TARGET_MARKER, XWA_HUD_ANCHOR_FORE_RADAR, XWA_HUD_WIDGET_KIND_RADAR_BLIPS,
		   XWA_HUD_PHASE_RADAR, 70, XWA_HUD_ASSET_RETICLE),
	WIDGET(XWA_HUD_WIDGET_AFT_TARGET_MARKER, XWA_HUD_ANCHOR_AFT_RADAR, XWA_HUD_WIDGET_KIND_RADAR_BLIPS,
		   XWA_HUD_PHASE_RADAR, 80, XWA_HUD_ASSET_RETICLE),
	WIDGET(XWA_HUD_WIDGET_LEFT_POWER, XWA_HUD_ANCHOR_LEFT_POWER, XWA_HUD_WIDGET_KIND_GAUGE,
		   XWA_HUD_PHASE_GAUGES, 90, XWA_HUD_ASSET_POWER),
	WIDGET(XWA_HUD_WIDGET_RIGHT_POWER, XWA_HUD_ANCHOR_RIGHT_POWER, XWA_HUD_WIDGET_KIND_GAUGE,
		   XWA_HUD_PHASE_GAUGES, 100, XWA_HUD_ASSET_POWER),
	WIDGET(XWA_HUD_WIDGET_LASER_CHARGE, XWA_HUD_ANCHOR_LASER_CHARGE, XWA_HUD_WIDGET_KIND_GAUGE,
		   XWA_HUD_PHASE_GAUGES, 110, XWA_HUD_ASSET_CHARGE),
	WIDGET(XWA_HUD_WIDGET_ION_CHARGE, XWA_HUD_ANCHOR_ION_CHARGE, XWA_HUD_WIDGET_KIND_GAUGE,
		   XWA_HUD_PHASE_GAUGES, 120, XWA_HUD_ASSET_CHARGE),
	WIDGET(XWA_HUD_WIDGET_SHIELD_HULL, XWA_HUD_ANCHOR_LEFT_SHIELD, XWA_HUD_WIDGET_KIND_GAUGE,
		   XWA_HUD_PHASE_GAUGES, 130, XWA_HUD_ASSET_SHIELD_HULL),
	WIDGET(XWA_HUD_WIDGET_BEAM, XWA_HUD_ANCHOR_RIGHT_BEAM, XWA_HUD_WIDGET_KIND_GAUGE, XWA_HUD_PHASE_GAUGES,
		   140, XWA_HUD_ASSET_BEAM),
	WIDGET(XWA_HUD_WIDGET_READINESS, XWA_HUD_ANCHOR_RETICLE, XWA_HUD_WIDGET_KIND_GAUGE, XWA_HUD_PHASE_GAUGES,
		   150, XWA_HUD_ASSET_RETICLE),
	WIDGET(XWA_HUD_WIDGET_RETICLE, XWA_HUD_ANCHOR_RETICLE, XWA_HUD_WIDGET_KIND_AIMING, XWA_HUD_PHASE_AIMING,
		   160, XWA_HUD_ASSET_RETICLE),
	WIDGET(XWA_HUD_WIDGET_THREATS, XWA_HUD_ANCHOR_THREATS, XWA_HUD_WIDGET_KIND_AIMING, XWA_HUD_PHASE_AIMING,
		   170, XWA_HUD_ASSET_THREATS),
	WIDGET(XWA_HUD_WIDGET_TARGET_ARROW, XWA_HUD_ANCHOR_TARGET_EDGE_BOUNDS, XWA_HUD_WIDGET_KIND_AIMING,
		   XWA_HUD_PHASE_AIMING, 180, XWA_HUD_ASSET_TARGET_ARROW),
	WIDGET(XWA_HUD_WIDGET_PADLOCK_ARROW, XWA_HUD_ANCHOR_TARGET_EDGE_BOUNDS, XWA_HUD_WIDGET_KIND_AIMING,
		   XWA_HUD_PHASE_AIMING, 190, XWA_HUD_ASSET_TARGET_ARROW),
	WIDGET(XWA_HUD_WIDGET_CMD_FRAME, XWA_HUD_ANCHOR_CMD_FRAME, XWA_HUD_WIDGET_KIND_SPRITE_GROUP,
		   XWA_HUD_PHASE_FRAMES, 200, XWA_HUD_ASSET_CMD_FRAME),
	WIDGET(XWA_HUD_WIDGET_MFD_LEFT_FRAME, XWA_HUD_ANCHOR_MFD_LEFT_FRAME, XWA_HUD_WIDGET_KIND_SPRITE_GROUP,
		   XWA_HUD_PHASE_FRAMES, 210, XWA_HUD_ASSET_MFD_FRAME),
	WIDGET(XWA_HUD_WIDGET_MFD_RIGHT_FRAME, XWA_HUD_ANCHOR_MFD_RIGHT_FRAME, XWA_HUD_WIDGET_KIND_SPRITE_GROUP,
		   XWA_HUD_PHASE_FRAMES, 220, XWA_HUD_ASSET_MFD_FRAME),
	WIDGET(XWA_HUD_WIDGET_TARGET_BOXES, XWA_HUD_ANCHOR_TARGET_EDGE_BOUNDS, XWA_HUD_WIDGET_KIND_TARGET_BOXES,
		   XWA_HUD_PHASE_WORLD_BOXES, 230, XWA_HUD_ASSET_NONE),
	WIDGET(XWA_HUD_WIDGET_CMD_PIP, XWA_HUD_ANCHOR_CMD_CRT, XWA_HUD_WIDGET_KIND_PIP, XWA_HUD_PHASE_CMD_PIP,
		   240, XWA_HUD_ASSET_NONE),
	WIDGET(XWA_HUD_WIDGET_CMD_COMPONENT_MARKER, XWA_HUD_ANCHOR_CMD_CRT, XWA_HUD_WIDGET_KIND_PIP,
		   XWA_HUD_PHASE_CMD_MARKER, 250, XWA_HUD_ASSET_NONE),
	WIDGET(XWA_HUD_WIDGET_PANE_GLYPHS, XWA_HUD_ANCHOR_CMD_TEXT, XWA_HUD_WIDGET_KIND_PANE_GLYPHS,
		   XWA_HUD_PHASE_PANE_GLYPHS, 260, XWA_HUD_ASSET_NONE),
};

static XwaHudLayout hud_layout;
static int hud_initialized;

typedef struct HudCachedFrame {
	int16_t object_type;
	int16_t frame;
	uint8_t state;
	XwaAssetRef ref;
} HudCachedFrame;

enum { HUD_FRAME_CACHE_CAPACITY = 96 };

static struct {
	HudCachedFrame frames[HUD_FRAME_CACHE_CAPACITY];
	uint8_t frame_count;
	const AeronFontAtlas* fonts[3];
	float font_atlas_scales[3];
	uint8_t render_phase;
	XwaRemasterHudPreparedAssets prepared;
} hud_assets;

typedef struct HudAssetFrameList {
	uint8_t count;
	uint8_t frames[12];
} HudAssetFrameList;

static const HudAssetFrameList hud_asset_frames[XWA_HUD_ASSET_COUNT] = {
	[XWA_HUD_ASSET_RADAR_FRAME] = { 4, { 27, 28, 49, 50 } },
	[XWA_HUD_ASSET_RADAR_SCOPE] = { 3, { 4, 45, 46 } },
	[XWA_HUD_ASSET_CMD_FRAME] = { 1, { 11 } },
	[XWA_HUD_ASSET_MFD_FRAME] = { 2, { 1, 2 } },
	[XWA_HUD_ASSET_POWER] = { 3, { 12, 13, 14 } },
	[XWA_HUD_ASSET_CHARGE] = { 4, { 23, 24, 25, 26 } },
	[XWA_HUD_ASSET_SHIELD_HULL] = { 5, { 39, 40, 41, 42, 43 } },
	[XWA_HUD_ASSET_BEAM] = { 10, { 29, 30, 31, 32, 33, 34, 35, 36, 37, 44 } },
	[XWA_HUD_ASSET_RETICLE] = { 8, { 5, 6, 7, 8, 9, 10, 47, 48 } },
	[XWA_HUD_ASSET_THREATS] = { 8, { 15, 16, 17, 18, 19, 20, 21, 22 } },
};

#undef WIDGET

const XwaHudWidgetDesc* XwaRemasterHud_WidgetRegistry(uint32_t* out_count) {
	if (out_count) {
		*out_count = (uint32_t)(sizeof hud_widgets / sizeof hud_widgets[0]);
	}
	return hud_widgets;
}

static int hud_registry_error(char* error, uint32_t error_size, const char* message, unsigned int value) {
	if (error && error_size) {
		snprintf(error, error_size, message, value);
	}
	return 0;
}

int XwaRemasterHud_ValidateWidgetRegistry(char* error, uint32_t error_size) {
	uint8_t seen_ids[XWA_HUD_WIDGET_COUNT] = { 0 };
	uint16_t previous_order = 0;
	uint32_t count;
	const XwaHudWidgetDesc* widgets = XwaRemasterHud_WidgetRegistry(&count);
	if (count != XWA_HUD_WIDGET_COUNT) {
		return hud_registry_error(error, error_size, "registry count %u does not match widget enum", count);
	}
	for (uint32_t i = 0; i < count; i++) {
		const XwaHudWidgetDesc* w = &widgets[i];
		if (w->id >= XWA_HUD_WIDGET_COUNT || seen_ids[w->id]) {
			return hud_registry_error(error, error_size, "duplicate/invalid widget id %u", w->id);
		}
		if (w->anchor >= XWA_HUD_ANCHOR_COUNT) {
			return hud_registry_error(error, error_size, "invalid anchor id %u", w->anchor);
		}
		if (w->phase >= XWA_HUD_PHASE_COUNT) {
			return hud_registry_error(error, error_size, "invalid phase for widget %u", w->id);
		}
		if (w->asset >= XWA_HUD_ASSET_COUNT) {
			return hud_registry_error(error, error_size, "invalid asset for widget %u", w->id);
		}
		if (w->asset != XWA_HUD_ASSET_NONE && w->asset != XWA_HUD_ASSET_TARGET_ARROW &&
			!hud_asset_frames[w->asset].count) {
			return hud_registry_error(error, error_size, "asset has no flight-cache manifest for widget %u",
									  w->id);
		}
		if (w->stable_order <= previous_order) {
			return hud_registry_error(error, error_size, "unstable draw order at widget %u", w->id);
		}
		seen_ids[w->id] = 1;
		previous_order = w->stable_order;
	}
	if (error && error_size)
		error[0] = '\0';
	return 1;
}

int XwaRemasterHud_Init(AeronVfs* vfs) {
	char error[192];
	(void)vfs;
	if (hud_initialized)
		return 1;
	if (!XwaRemasterHud_ValidateWidgetRegistry(error, sizeof error)) {
		Aeron_LogError("xwa.hud", "widget registry invalid: %s", error);
		return 0;
	}
	XwaRemasterHudLayout_Init(&hud_layout);
	if (!XwaRemasterHudFixed_Init()) {
		Aeron_LogError("xwa.hud", "fixed-widget draw list initialization failed");
		return 0;
	}
	if (!XwaRemasterHudBoxes_Init()) {
		XwaRemasterHudFixed_Shutdown();
		Aeron_LogError("xwa.hud", "target-box draw list initialization failed");
		return 0;
	}
	if (!XwaRemasterHudCmd_Init()) {
		XwaRemasterHudBoxes_Shutdown();
		XwaRemasterHudFixed_Shutdown();
		Aeron_LogError("xwa.hud", "CMD PiP initialization failed");
		return 0;
	}
	if (!XwaRemasterHudText_Init()) {
		XwaRemasterHudCmd_Shutdown();
		XwaRemasterHudBoxes_Shutdown();
		XwaRemasterHudFixed_Shutdown();
		Aeron_LogError("xwa.hud", "pane-glyph draw list initialization failed");
		return 0;
	}
	hud_initialized = 1;
	return 1;
}

const XwaHudLayout* XwaRemasterHud_Layout(void) { return hud_initialized ? &hud_layout : NULL; }

void XwaRemasterHud_Shutdown(void) {
	XwaRemasterHudText_Shutdown();
	XwaRemasterHudCmd_Shutdown();
	XwaRemasterHudBoxes_Shutdown();
	XwaRemasterHudFixed_Shutdown();
	memset(&hud_layout, 0, sizeof hud_layout);
	memset(&hud_assets, 0, sizeof hud_assets);
	hud_initialized = 0;
}

static int hud_widget_visible(uint16_t id, const XwaRemasterHudVisibility* v) {
	switch ((XwaHudWidgetId)id) {
		case XWA_HUD_WIDGET_FORE_RADAR_FRAME:
		case XWA_HUD_WIDGET_AFT_RADAR_FRAME:
			return v->fixed_frames;
		case XWA_HUD_WIDGET_FORE_RADAR_SCOPE:
		case XWA_HUD_WIDGET_AFT_RADAR_SCOPE:
			return v->radars;
		case XWA_HUD_WIDGET_CMD_FRAME:
			return v->cmd;
		case XWA_HUD_WIDGET_MFD_LEFT_FRAME:
		case XWA_HUD_WIDGET_MFD_RIGHT_FRAME:
			return v->mfd_frames || v->film_mfds;
		case XWA_HUD_WIDGET_LEFT_POWER:
		case XWA_HUD_WIDGET_RIGHT_POWER:
			return v->power;
		case XWA_HUD_WIDGET_LASER_CHARGE:
		case XWA_HUD_WIDGET_ION_CHARGE:
			return v->charge;
		case XWA_HUD_WIDGET_SHIELD_HULL:
			return v->shield_hull;
		case XWA_HUD_WIDGET_BEAM:
			return v->beam;
		case XWA_HUD_WIDGET_READINESS:
		case XWA_HUD_WIDGET_RETICLE:
			return v->reticle;
		case XWA_HUD_WIDGET_FORE_TARGET_MARKER:
		case XWA_HUD_WIDGET_AFT_TARGET_MARKER:
			return v->radar_blips;
		case XWA_HUD_WIDGET_THREATS:
			return v->threats;
		case XWA_HUD_WIDGET_TARGET_ARROW:
		case XWA_HUD_WIDGET_PADLOCK_ARROW:
			return v->target_arrow;
		case XWA_HUD_WIDGET_CMD_PIP:
		case XWA_HUD_WIDGET_CMD_COMPONENT_MARKER:
			return v->cmd_pip;
		case XWA_HUD_WIDGET_PANE_GLYPHS:
			return v->pane_glyphs;
		default:
			return 0;
	}
}

uint32_t XwaRemasterHud_BuildAssetRequests(const XwaHudState* hud, uint8_t* out_font_mask) {
	XwaRemasterHudVisibility visibility;
	XwaRemasterHud_BuildVisibility(hud, &visibility);
	uint32_t requested = 0;
	uint32_t count = 0;
	const XwaHudWidgetDesc* widgets = XwaRemasterHud_WidgetRegistry(&count);
	for (uint32_t i = 0; i < count; i++) {
		if (hud_widget_visible(widgets[i].id, &visibility) && widgets[i].asset != XWA_HUD_ASSET_NONE)
			requested |= 1u << widgets[i].asset;
	}
	uint8_t fonts = 0;
	if (visibility.pane_glyphs && hud) {
		for (uint16_t i = 0; i < hud->glyph_count; i++) {
			if (hud->glyphs[i].font_tier < 3)
				fonts |= (uint8_t)(1u << hud->glyphs[i].font_tier);
		}
	}
	if (visibility.target_arrow && hud && hud->target.valid) {
		const int scale = (int)(hud->classic_hud_scale * 10.0f);
		fonts |= (uint8_t)(1u << (scale < 12 ? 2 : (scale < 15 ? 1 : 0)));
	}
	if (hud && hud->target.valid && hud->target_box_count) {
		const int scale = (int)(hud->classic_hud_scale * 10.0f);
		fonts |= (uint8_t)(1u << (scale < 12 ? 2 : (scale < 15 ? 1 : 0)));
	}
	if (out_font_mask)
		*out_font_mask = fonts;
	return requested;
}

static HudCachedFrame* hud_find_frame(int object_type, int frame) {
	for (uint8_t i = 0; i < hud_assets.frame_count; i++) {
		HudCachedFrame* cached = &hud_assets.frames[i];
		if (cached->object_type == object_type && cached->frame == frame)
			return cached;
	}
	return NULL;
}

static int hud_cache_frame(XwaRemasterAssets* assets, int object_type, int frame) {
	HudCachedFrame* cached = hud_find_frame(object_type, frame);
	if (cached)
		return cached->state == 1;
	if (hud_assets.frame_count >= HUD_FRAME_CACHE_CAPACITY)
		return 0;
	cached = &hud_assets.frames[hud_assets.frame_count++];
	memset(cached, 0, sizeof *cached);
	cached->object_type = (int16_t)object_type;
	cached->frame = (int16_t)frame;
	cached->state = XwaRemasterAssets_FlightModelFrame(assets, object_type, frame, &cached->ref) ? 1 : 2;
	return cached->state == 1;
}

static int hud_cache_asset(XwaRemasterAssets* assets, XwaHudAssetId asset, const XwaHudState* hud) {
	if (asset == XWA_HUD_ASSET_NONE)
		return 1;
	if (asset == XWA_HUD_ASSET_TARGET_ARROW)
		return hud_cache_frame(assets, OBJ_HudTextureGroup13000_Sprite000, 1);
	const HudAssetFrameList* list = &hud_asset_frames[asset];
	int ok = list->count != 0;
	for (uint8_t i = 0; i < list->count; i++)
		ok &= hud_cache_frame(assets, OBJ_HudTextureGroup12000, list->frames[i]);
	if (asset == XWA_HUD_ASSET_SHIELD_HULL && hud && hud->instruments.shield_silhouette_sprite)
		ok &= hud_cache_frame(assets, OBJ_HullIconTextureGroup26000,
								hud->instruments.shield_silhouette_sprite / 100);
	return ok;
}

void XwaRemasterHud_PrepareFrame(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot,
								 XwaRemasterAssets* assets, const XwaRemasterFlightView* flight_view,
								 int target_w, int target_h) {
	const XwaHudState* hud = snapshot ? &snapshot->hud : NULL;
	if (!cmd || !assets || !hud_initialized)
		return;
	if (!XwaRemasterHudLayout_Resolve(&hud_layout, target_w, target_h))
		return;
	const uint32_t layout_generation = hud_layout.generation;
	const uint32_t bundle_generation = XwaRemasterAssets_Generation(assets);
	if (hud_assets.prepared.bundle_generation != bundle_generation) {
		memset(&hud_assets, 0, sizeof hud_assets);
		hud_assets.prepared.bundle_generation = bundle_generation;
	}
	hud_assets.prepared.layout_generation = layout_generation;
	uint8_t requested_fonts = 0;
	const uint32_t requested = XwaRemasterHud_BuildAssetRequests(hud, &requested_fonts);
	hud_assets.prepared.requested_asset_mask = requested;
	hud_assets.prepared.requested_font_mask = requested_fonts;
	for (int asset = 1; asset < XWA_HUD_ASSET_COUNT; asset++) {
		const uint32_t bit = 1u << asset;
		if (!(requested & bit))
			continue;
		if (hud_cache_asset(assets, (XwaHudAssetId)asset, hud)) {
			hud_assets.prepared.resolved_asset_mask |= bit;
			hud_assets.prepared.missing_asset_mask &= ~bit;
		} else {
			hud_assets.prepared.missing_asset_mask |= bit;
		}
	}
	for (int tier = 0; tier < 3; tier++) {
		const uint8_t bit = (uint8_t)(1u << tier);
		if (!(requested_fonts & bit) || (hud_assets.prepared.resolved_font_mask & bit))
			continue;
		hud_assets.fonts[tier] = XwaRemasterAssets_FlightFont(
				assets, tier, &hud_assets.font_atlas_scales[tier]);
		if (hud_assets.fonts[tier]) {
			hud_assets.prepared.resolved_font_mask |= bit;
			hud_assets.prepared.missing_font_mask &= (uint8_t)~bit;
		} else {
			hud_assets.prepared.missing_font_mask |= bit;
		}
	}
	hud_assets.prepared.cache_entries = hud_assets.frame_count;
	const XwaHudProfileIndex profile = hud_layout.default_profile;
	XwaRemasterHudCmd_Prepare(cmd, snapshot, assets, profile, target_w, target_h);
	XwaRemasterHudFixed_Build(snapshot, profile, bundle_generation, flight_view, target_w, target_h);
	XwaRemasterHudBoxes_Build(snapshot, profile, bundle_generation, flight_view, target_w, target_h);
	XwaRemasterHudText_Build(snapshot, profile, bundle_generation);
	XwaRemasterHudBoxes_PrepareDrawLists(cmd, target_w, target_h);
	XwaRemasterHudFixed_PrepareDrawList(cmd, target_w, target_h);
	XwaRemasterHudCmd_PrepareDrawList(cmd, target_w, target_h);
	XwaRemasterHudText_PrepareDrawList(cmd, target_w, target_h);
}

const XwaRemasterHudPreparedAssets* XwaRemasterHud_PreparedAssets(void) { return &hud_assets.prepared; }

void XwaRemasterHud_BeginRenderPhase(void) { hud_assets.render_phase = 1; }

void XwaRemasterHud_EndRenderPhase(void) { hud_assets.render_phase = 0; }

static void hud_uncached_asset(void) {
	if (hud_assets.render_phase)
		hud_assets.prepared.render_phase_violations++;
}

const XwaAssetRef* XwaRemasterHud_AssetFrame(int object_type, int classic_frame_1based) {
	HudCachedFrame* cached = hud_find_frame(object_type, classic_frame_1based);
	if (!cached) {
		hud_uncached_asset();
		return NULL;
	}
	return cached->state == 1 ? &cached->ref : NULL;
}

const AeronFontAtlas* XwaRemasterHud_FlightFont(int tier,
		float* out_atlas_scale) {
	if (tier < 0 || tier >= 3 || !(hud_assets.prepared.resolved_font_mask & (1u << tier))) {
		hud_uncached_asset();
		return NULL;
	}
	if (out_atlas_scale)
		*out_atlas_scale = hud_assets.font_atlas_scales[tier];
	return hud_assets.fonts[tier];
}

void XwaRemasterHud_BuildVisibility(const XwaHudState* h, XwaRemasterHudVisibility* out) {
	const uint32_t modes = h ? h->mode_flags : 0;
	const int film = (modes & XWA_HUD_MODE_FILM_PLAYBACK) != 0;
	const int film_overlay = (modes & XWA_HUD_MODE_FILM_OVERLAY) != 0;
	const int hangar = (modes & XWA_HUD_MODE_HANGAR_READY) != 0;
	const int map = (modes & XWA_HUD_MODE_MAP) != 0;
	const int external = (modes & XWA_HUD_MODE_EXTERNAL_CAMERA) != 0;
	const int hyperspace = (modes & XWA_HUD_MODE_HYPERSPACE) != 0;
	const int region = (modes & XWA_HUD_MODE_REGION_SESSION) != 0;
	const int mission_end = (modes & XWA_HUD_MODE_MISSION_END) != 0;
	memset(out, 0, sizeof *out);
	if (!h) {
		return;
	}

	/* Network/recording/message/film glyphs are selected after the primary
	 * dispatcher even when the main HUD is disabled. The capture stream itself
	 * remains the final authority for which panes actually contain records. */
	out->pane_glyphs = 1;
	out->film_mfds = (uint8_t)(film && h->film_mfd_visible);
	if (film && film_overlay) {
		return;
	}

	if (!h->hud_enabled) {
		out->target_arrow = (uint8_t)(external && !film_overlay);
		return;
	}

	if (hangar || map) {
		out->cmd = 1;
		out->mfd_frames = 1;
		out->cmd_pip = 1;
		return;
	}
	if (!h->valid) {
		return;
	}

	/* MFD overlay mode suppresses the fixed cockpit instruments but leaves the
	 * targeting layer alive in ordinary flight. */
	if (h->mfd_enabled[0]) {
		if (!region && !mission_end && !hyperspace) {
			out->reticle = 1;
			out->threats = 1;
			out->target_arrow = (uint8_t)!film_overlay;
		}
		return;
	}

	if (mission_end) {
		return;
	}
	if (external) {
		out->target_arrow = (uint8_t)(!region && !hyperspace);
		return;
	}

	out->fixed_frames = 1;
	out->radars = 1;
	out->radar_blips = (uint8_t)!hyperspace;
	out->power = 1;
	out->charge = 1;
	out->shield_hull = 1;
	out->beam = 1;
	out->reticle = 1;
	out->threats = 1;
	out->target_arrow = (uint8_t)!hyperspace;
	out->cmd = 1;
	out->mfd_frames = 1;
	out->cmd_pip = 1;
}
