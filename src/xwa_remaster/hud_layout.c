#include "xwa_remaster/hud_layout.h"

#include <math.h>
#include <string.h>

typedef struct HudAnchorDefinition {
	const char* name;
	XwaHudLayoutRect canonical;
	XwaHudLayoutOrigin origin;
	uint8_t has_radius;
	int canonical_radius;
} HudAnchorDefinition;

#define HUD_ANCHOR(name_, x_, y_, w_, h_, origin_) { name_, { x_, y_, w_, h_ }, origin_, 0, 0 }
#define HUD_RADAR(name_, x_, y_, w_, h_, origin_) { name_, { x_, y_, w_, h_ }, origin_, 1, 42 }

/*
 * The rectangles describe immutable 640x480 HUD geometry. Their horizontal
 * origins reproduce the original screenWidth/2 and screenWidth-offset
 * formulas; they are not per-resolution authoring positions.
 */
static const HudAnchorDefinition hud_anchor_definitions[XWA_HUD_ANCHOR_COUNT] = {
	HUD_ANCHOR("reticle", 0, 0, 640, 480, XWA_HUD_LAYOUT_CENTER),
	HUD_ANCHOR("threats", 0, 0, 640, 480, XWA_HUD_LAYOUT_CENTER),
	HUD_RADAR("fore_radar", 8, 386, 94, 94, XWA_HUD_LAYOUT_LEFT_BOTTOM),
	HUD_RADAR("aft_radar", 538, 386, 94, 94, XWA_HUD_LAYOUT_RIGHT_BOTTOM),
	HUD_ANCHOR("left_power", 0, 340, 32, 140, XWA_HUD_LAYOUT_LEFT_BOTTOM),
	HUD_ANCHOR("right_power", 608, 340, 32, 140, XWA_HUD_LAYOUT_RIGHT_BOTTOM),
	HUD_ANCHOR("left_shield", 0, 90, 100, 120, XWA_HUD_LAYOUT_LEFT_TOP),
	HUD_ANCHOR("right_beam", 540, 90, 100, 120, XWA_HUD_LAYOUT_RIGHT_TOP),
	HUD_ANCHOR("laser_charge", 220, 320, 100, 120, XWA_HUD_LAYOUT_CENTER_BOTTOM),
	HUD_ANCHOR("ion_charge", 320, 320, 100, 120, XWA_HUD_LAYOUT_CENTER_BOTTOM),
	HUD_ANCHOR("cmd_frame", 200, 350, 240, 130, XWA_HUD_LAYOUT_CENTER_BOTTOM),
	HUD_ANCHOR("cmd_text", 200, 355, 240, 125, XWA_HUD_LAYOUT_CENTER_BOTTOM),
	HUD_ANCHOR("cmd_crt", 250, 366, 140, 114, XWA_HUD_LAYOUT_CENTER_BOTTOM),
	HUD_ANCHOR("mfd_left_frame", 0, 280, 200, 200, XWA_HUD_LAYOUT_LEFT_BOTTOM),
	HUD_ANCHOR("mfd_left_title", 0, 338, 200, 12, XWA_HUD_LAYOUT_LEFT_BOTTOM),
	HUD_ANCHOR("mfd_left_body", 0, 349, 200, 120, XWA_HUD_LAYOUT_LEFT_BOTTOM),
	HUD_ANCHOR("mfd_right_frame", 440, 280, 200, 200, XWA_HUD_LAYOUT_RIGHT_BOTTOM),
	HUD_ANCHOR("mfd_right_title", 440, 338, 200, 12, XWA_HUD_LAYOUT_RIGHT_BOTTOM),
	HUD_ANCHOR("mfd_right_body", 440, 349, 200, 120, XWA_HUD_LAYOUT_RIGHT_BOTTOM),
	HUD_ANCHOR("top_speed", 160, 2, 75, 11, XWA_HUD_LAYOUT_CENTER_TOP),
	HUD_ANCHOR("top_throttle", 160, 11, 75, 11, XWA_HUD_LAYOUT_CENTER_TOP),
	HUD_ANCHOR("top_craft_name", 420, 2, 100, 10, XWA_HUD_LAYOUT_CENTER_TOP),
	HUD_ANCHOR("top_clock", 420, 11, 80, 11, XWA_HUD_LAYOUT_CENTER_TOP),
	HUD_ANCHOR("top_weapons", 285, 11, 75, 11, XWA_HUD_LAYOUT_CENTER_TOP),
	HUD_ANCHOR("top_countermeasure", 365, 11, 25, 11, XWA_HUD_LAYOUT_CENTER_TOP),
	HUD_ANCHOR("top_proving_status", 285, 11, 130, 11, XWA_HUD_LAYOUT_CENTER_TOP),
	HUD_ANCHOR("left_subsystem", 0, 0, 40, 200, XWA_HUD_LAYOUT_LEFT_TOP),
	HUD_ANCHOR("right_subsystem", 600, 0, 40, 200, XWA_HUD_LAYOUT_RIGHT_TOP),
	HUD_ANCHOR("message_system", 85, 64, 470, 11, XWA_HUD_LAYOUT_CENTER_TOP),
	HUD_ANCHOR("message_flight_group", 130, 320, 380, 11, XWA_HUD_LAYOUT_CENTER_BOTTOM),
	HUD_ANCHOR("message_ready", 110, 35, 420, 47, XWA_HUD_LAYOUT_CENTER_TOP),
	HUD_ANCHOR("network", 0, 0, 640, 480, XWA_HUD_LAYOUT_LEFT_TOP),
	HUD_ANCHOR("film_recording", 120, 11, 50, 11, XWA_HUD_LAYOUT_CENTER_TOP),
	HUD_ANCHOR("fps", 0, 0, 100, 20, XWA_HUD_LAYOUT_CENTER_BOTTOM),
	HUD_ANCHOR("target_edge_bounds", 0, 0, 640, 480, XWA_HUD_LAYOUT_VIEWPORT),
};

#undef HUD_RADAR
#undef HUD_ANCHOR

void XwaRemasterHudLayout_MapPoint(XwaHudLayoutOrigin origin, float source_w, float source_h, float target_w,
								   float target_h, float scale, float source_x, float source_y, float* out_x,
								   float* out_y) {
	float x = source_x * scale;
	float y = source_y * scale;
	switch (origin) {
		case XWA_HUD_LAYOUT_CENTER_TOP:
		case XWA_HUD_LAYOUT_CENTER_BOTTOM:
		case XWA_HUD_LAYOUT_CENTER:
			x = target_w * 0.5f + (source_x - source_w * 0.5f) * scale;
			break;
		case XWA_HUD_LAYOUT_RIGHT_TOP:
		case XWA_HUD_LAYOUT_RIGHT_BOTTOM:
			x = target_w - (source_w - source_x) * scale;
			break;
		case XWA_HUD_LAYOUT_VIEWPORT:
			x = source_w > 0.0f ? source_x * target_w / source_w : 0.0f;
			break;
		case XWA_HUD_LAYOUT_LEFT_TOP:
		case XWA_HUD_LAYOUT_LEFT_BOTTOM:
			break;
	}
	switch (origin) {
		case XWA_HUD_LAYOUT_LEFT_BOTTOM:
		case XWA_HUD_LAYOUT_CENTER_BOTTOM:
		case XWA_HUD_LAYOUT_RIGHT_BOTTOM:
			y = target_h - (source_h - source_y) * scale;
			break;
		case XWA_HUD_LAYOUT_CENTER:
			y = target_h * 0.5f + (source_y - source_h * 0.5f) * scale;
			break;
		case XWA_HUD_LAYOUT_VIEWPORT:
			y = source_h > 0.0f ? source_y * target_h / source_h : 0.0f;
			break;
		case XWA_HUD_LAYOUT_LEFT_TOP:
		case XWA_HUD_LAYOUT_CENTER_TOP:
		case XWA_HUD_LAYOUT_RIGHT_TOP:
			break;
	}
	if (out_x)
		*out_x = x;
	if (out_y)
		*out_y = y;
}

const char* XwaRemasterHud_AnchorName(XwaHudAnchorId id) {
	return id >= 0 && id < XWA_HUD_ANCHOR_COUNT ? hud_anchor_definitions[id].name : NULL;
}

const XwaHudLayoutRect* XwaRemasterHudLayout_CanonicalAnchor(XwaHudAnchorId id) {
	return id >= 0 && id < XWA_HUD_ANCHOR_COUNT ? &hud_anchor_definitions[id].canonical : NULL;
}

void XwaRemasterHudLayout_Init(XwaHudLayout* out) {
	if (!out)
		return;
	memset(out, 0, sizeof *out);
	out->profile_count = 1;
	out->default_profile = 0;
	strcpy(out->profiles[0].name, "logical");
}

int XwaRemasterHudLayout_Resolve(XwaHudLayout* layout, int target_w, int target_h) {
	if (!layout || target_w <= 0 || target_h <= 0)
		return 0;
	XwaHudLayoutProfile* profile = &layout->profiles[0];
	if (profile->valid && profile->reference_w == target_w && profile->reference_h == target_h)
		return 1;

	const float scale = (float)target_h / XWA_HUD_CANONICAL_HEIGHT;
	memset(profile->anchors, 0, sizeof profile->anchors);
	profile->reference_w = target_w;
	profile->reference_h = target_h;
	for (int i = 0; i < XWA_HUD_ANCHOR_COUNT; i++) {
		const HudAnchorDefinition* definition = &hud_anchor_definitions[i];
		XwaHudLayoutAnchor* anchor = &profile->anchors[i];
		float x, y;
		XwaRemasterHudLayout_MapPoint(definition->origin, XWA_HUD_CANONICAL_WIDTH, XWA_HUD_CANONICAL_HEIGHT,
									  target_w, target_h, scale, definition->canonical.x,
									  definition->canonical.y, &x, &y);
		anchor->rect.x = (int)lroundf(x);
		anchor->rect.y = (int)lroundf(y);
		anchor->rect.w = definition->origin == XWA_HUD_LAYOUT_VIEWPORT
							 ? target_w
							 : (int)lroundf(definition->canonical.w * scale);
		anchor->rect.h = definition->origin == XWA_HUD_LAYOUT_VIEWPORT
							 ? target_h
							 : (int)lroundf(definition->canonical.h * scale);
		anchor->has_radius = definition->has_radius;
		anchor->radius = (int)lroundf(definition->canonical_radius * scale);
	}
	profile->valid = 1;
	layout->generation++;
	return 1;
}

const XwaHudLayoutProfile* XwaRemasterHudLayout_Profile(const XwaHudLayout* layout,
														XwaHudProfileIndex index) {
	if (!layout || index != 0 || !layout->profiles[0].valid)
		return NULL;
	return &layout->profiles[0];
}

void XwaRemasterHudLayout_OutputTransform(const XwaHudLayoutProfile* profile, int target_w, int target_h,
										  int* out_x, int* out_y, float* out_scale) {
	if (out_x)
		*out_x = 0;
	if (out_y)
		*out_y = 0;
	if (out_scale)
		*out_scale = 0.0f;
	if (!profile || !profile->valid || profile->reference_w != target_w || profile->reference_h != target_h)
		return;
	if (out_scale)
		*out_scale = 1.0f;
}

int XwaRemasterHudLayout_TargetToReference(const XwaHudLayoutProfile* profile, int target_w, int target_h,
										   float target_x, float target_y, float* out_x, float* out_y) {
	if (!profile || !profile->valid || profile->reference_w != target_w || profile->reference_h != target_h ||
		!out_x || !out_y)
		return 0;
	*out_x = target_x;
	*out_y = target_y;
	return 1;
}
