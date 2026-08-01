#include "xwa_remaster/hud_fixed.h"

#include "xwa/assets/object_type.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/hud_boxes.h"
#include "xwa_remaster/hud_cmd.h"
#include "xwa_remaster/hud_layout.h"
#include "xwa_remaster/hud_text.h"
#include "xwa_remaster/world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static AeronDrawList2D* fixed_list;
static XwaHudPreparedDrawState fixed_draws;
static const XwaSnapshot* fixed_snapshot;
static float fixed_hud_unit_scale = 1.0f;
static float fixed_classic_pixel_scale = 1.0f;
static const XwaRemasterFlightView* fixed_flight_view;
static int fixed_target_w, fixed_target_h;
static float fixed_reticle_x_ref, fixed_reticle_y_ref;
static int fixed_reticle_valid;
static XwaHudProjectedArrow fixed_target_arrow;

static XwaHudLayoutOrigin fixed_scale_origin(XwaHudWidgetId widget) {
	switch (widget) {
		case XWA_HUD_WIDGET_FORE_RADAR_FRAME:
		case XWA_HUD_WIDGET_AFT_RADAR_FRAME:
			return XWA_HUD_LAYOUT_CENTER_TOP;
		case XWA_HUD_WIDGET_FORE_RADAR_SCOPE:
		case XWA_HUD_WIDGET_FORE_RADAR_BLIPS:
		case XWA_HUD_WIDGET_FORE_TARGET_MARKER:
		case XWA_HUD_WIDGET_LEFT_POWER:
		case XWA_HUD_WIDGET_SHIELD_HULL:
			return XWA_HUD_LAYOUT_LEFT_TOP;
		case XWA_HUD_WIDGET_AFT_RADAR_SCOPE:
		case XWA_HUD_WIDGET_AFT_RADAR_BLIPS:
		case XWA_HUD_WIDGET_AFT_TARGET_MARKER:
		case XWA_HUD_WIDGET_RIGHT_POWER:
		case XWA_HUD_WIDGET_BEAM:
			return XWA_HUD_LAYOUT_RIGHT_TOP;
		case XWA_HUD_WIDGET_MFD_LEFT_FRAME:
			return XWA_HUD_LAYOUT_LEFT_BOTTOM;
		case XWA_HUD_WIDGET_MFD_RIGHT_FRAME:
			return XWA_HUD_LAYOUT_RIGHT_BOTTOM;
		case XWA_HUD_WIDGET_CMD_FRAME:
		case XWA_HUD_WIDGET_LASER_CHARGE:
		case XWA_HUD_WIDGET_ION_CHARGE:
			return XWA_HUD_LAYOUT_CENTER_BOTTOM;
		case XWA_HUD_WIDGET_READINESS:
		case XWA_HUD_WIDGET_RETICLE:
		case XWA_HUD_WIDGET_THREATS:
			return XWA_HUD_LAYOUT_VIEWPORT;
		default:
			return XWA_HUD_LAYOUT_CENTER;
	}
}

static void fixed_point_ref(XwaHudWidgetId widget, float classic_x, float classic_top_y, float* out_x,
							float* out_y, float* out_scale) {
	XwaRemasterHudLayout_MapPoint(fixed_scale_origin(widget), XWA_HUD_CANONICAL_WIDTH,
								  XWA_HUD_CANONICAL_HEIGHT, fixed_target_w, fixed_target_h,
								  fixed_hud_unit_scale, classic_x, classic_top_y, out_x, out_y);
	*out_scale = fixed_hud_unit_scale;
}

static const XwaHudLayoutProfile* fixed_profile(XwaHudProfileIndex index) {
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	return XwaRemasterHudLayout_Profile(layout, index);
}

static const XwaHudWidgetDesc* fixed_widget(XwaHudWidgetId id) {
	uint32_t count = 0;
	const XwaHudWidgetDesc* widgets = XwaRemasterHud_WidgetRegistry(&count);
	for (uint32_t i = 0; i < count; i++)
		if (widgets[i].id == id)
			return &widgets[i];
	return NULL;
}

static void fixed_emit_ref(XwaHudWidgetId widget_id, int object_type, int frame, float center_x_ref,
						   float center_y_ref, float scale_ref, int screen_size, uint32_t argb,
						   int mirror_x) {
	const XwaHudWidgetDesc* widget = fixed_widget(widget_id);
	if (!widget)
		return;
	if (fixed_draws.record_count >= XWA_HUD_MAX_FIXED_DRAWS) {
		fixed_draws.dropped_records++;
		return;
	}
	XwaHudDrawRecord* record = &fixed_draws.records[fixed_draws.record_count++];
	memset(record, 0, sizeof *record);
	record->widget = widget->id;
	record->anchor = widget->anchor;
	record->object_type = (uint16_t)object_type;
	record->frame = (uint16_t)frame;
	record->screen_size = (uint16_t)screen_size;
	record->phase = widget->phase;
	record->mirror_x = (uint8_t)mirror_x;
	record->stable_order = widget->stable_order;
	record->sequence = fixed_draws.record_count - 1;
	record->argb = argb;
	record->center_x_ref = center_x_ref;
	record->center_y_ref = center_y_ref;
	record->scale_ref = scale_ref;
}

static void fixed_emit(XwaHudWidgetId widget_id, int object_type, int frame, float screen_x, float screen_y,
					   int screen_size, uint32_t argb, int mirror_x) {
	const XwaHudWidgetDesc* widget = fixed_widget(widget_id);
	if (!widget)
		return;
	float center_x_ref, center_y_ref, scale_ref;
	fixed_point_ref(widget_id, screen_x, 480.0f - screen_y, &center_x_ref, &center_y_ref, &scale_ref);
	fixed_emit_ref(widget_id, object_type, frame, center_x_ref, center_y_ref, scale_ref, screen_size, argb,
				   mirror_x);
}

static void fixed_emit_radar_record(XwaHudWidgetId widget_id, int frame, float center_x_ref,
									float center_y_ref, float scale_ref, int screen_size, uint32_t argb) {
	const XwaHudWidgetDesc* widget = fixed_widget(widget_id);
	if (!widget)
		return;
	if (fixed_draws.record_count >= XWA_HUD_MAX_FIXED_DRAWS) {
		fixed_draws.dropped_records++;
		return;
	}
	XwaHudDrawRecord* record = &fixed_draws.records[fixed_draws.record_count++];
	memset(record, 0, sizeof *record);
	record->widget = widget->id;
	record->anchor = widget->anchor;
	record->object_type = OBJ_HudTextureGroup12000;
	record->frame = (uint16_t)frame;
	record->screen_size = (uint16_t)screen_size;
	record->phase = widget->phase;
	record->stable_order = widget->stable_order;
	record->sequence = fixed_draws.record_count - 1;
	record->center_x_ref = center_x_ref;
	record->center_y_ref = center_y_ref;
	record->scale_ref = scale_ref;
	record->argb = argb;
}

static int fixed_element(const XwaHudState* h, int index) {
	return h && index >= 0 && index < 16 && (h->element_enabled_mask & (1u << index)) != 0;
}

static int fixed_compare_draw_records(const void* lhs, const void* rhs) {
	const XwaHudDrawRecord* a = (const XwaHudDrawRecord*)lhs;
	const XwaHudDrawRecord* b = (const XwaHudDrawRecord*)rhs;
	if (a->stable_order < b->stable_order)
		return -1;
	if (a->stable_order > b->stable_order)
		return 1;
	if (a->sequence < b->sequence)
		return -1;
	if (a->sequence > b->sequence)
		return 1;
	return 0;
}

static void fixed_build_frames(const XwaHudState* h, const XwaRemasterHudVisibility* v) {
	const uint32_t hud_color = h->hud_colors[0];
	if (v->fixed_frames && fixed_element(h, 3)) {
		fixed_emit(XWA_HUD_WIDGET_FORE_RADAR_FRAME, OBJ_HudTextureGroup12000, 27, 206, 464, 256, hud_color,
				   0);
		if (!(h->mode_flags & XWA_HUD_MODE_PROVING_GROUND)) {
			fixed_emit(XWA_HUD_WIDGET_FORE_RADAR_FRAME, OBJ_HudTextureGroup12000, 49, 272, 464, 256,
					   0xffffffffu, 0);
		}
		fixed_emit(XWA_HUD_WIDGET_AFT_RADAR_FRAME, OBJ_HudTextureGroup12000, 28, 434, 464, 256, hud_color, 0);
		if (!(h->mode_flags & XWA_HUD_MODE_PROVING_GROUND)) {
			fixed_emit(XWA_HUD_WIDGET_AFT_RADAR_FRAME, OBJ_HudTextureGroup12000, 50, 350, 464, 256,
					   0xffffffffu, 0);
		}
	}
	if (v->radars) {
		if (fixed_element(h, 1))
			fixed_emit(XWA_HUD_WIDGET_FORE_RADAR_SCOPE, OBJ_HudTextureGroup12000, 45, 55, 433, 256, hud_color,
					   0);
		if (fixed_element(h, 2)) {
			const int frame = (h->instruments.system_flags & 0x100u) ? 46 : 4;
			fixed_emit(XWA_HUD_WIDGET_AFT_RADAR_SCOPE, OBJ_HudTextureGroup12000, frame, 585, 433, 256,
					   hud_color, 0);
		}
	}
	if (v->cmd && (fixed_element(h, 0) || (h->mode_flags & XWA_HUD_MODE_HANGAR_READY)))
		fixed_emit(XWA_HUD_WIDGET_CMD_FRAME, OBJ_HudTextureGroup12000, 11, 320, 62, 256, hud_color, 0);
	if (v->mfd_frames || v->film_mfds) {
		const int hangar = (h->mode_flags & XWA_HUD_MODE_HANGAR_READY) != 0;
		const int film = (h->mode_flags & XWA_HUD_MODE_FILM_PLAYBACK) != 0;
		int left = h->mfd_enabled[1] || hangar;
		int right = h->mfd_enabled[2] || hangar;
		if (film) {
			left = h->mfd_enabled[1] || h->film_mfd_visible;
			right = h->mfd_enabled[2] || h->film_mfd_visible;
			if (left && !(h->mode_flags & XWA_HUD_MODE_MAP)) {
				if (!h->hud_enabled && !h->film_mfd_visible)
					return;
				if (!h->film_mfd_visible && h->mfd_enabled[1] && (h->mode_flags & XWA_HUD_MODE_FILM_OVERLAY))
					return;
				if (!h->film_mfd_visible && h->mfd_enabled[0])
					return;
			}
		}
		if (left) {
			fixed_emit(XWA_HUD_WIDGET_MFD_LEFT_FRAME, OBJ_HudTextureGroup12000, 1, 100, 74, 512, hud_color,
					   0);
			fixed_emit(XWA_HUD_WIDGET_MFD_LEFT_FRAME, OBJ_HudTextureGroup12000, 2, 100, 74, 256,
					   !film && h->mfd_active == 1 ? h->hud_colors[1] : hud_color, 0);
		}
		if (film && right && !(h->mode_flags & XWA_HUD_MODE_MAP)) {
			if (!h->hud_enabled && !h->film_mfd_visible)
				return;
			if (!h->film_mfd_visible && h->mfd_enabled[1] && (h->mode_flags & XWA_HUD_MODE_FILM_OVERLAY))
				return;
			if (!h->film_mfd_visible && h->mfd_enabled[0])
				return;
		}
		if (right) {
			fixed_emit(XWA_HUD_WIDGET_MFD_RIGHT_FRAME, OBJ_HudTextureGroup12000, 1, 540, 74, 512, hud_color,
					   1);
			fixed_emit(XWA_HUD_WIDGET_MFD_RIGHT_FRAME, OBJ_HudTextureGroup12000, 2, 540, 74, 256,
					   !film && h->mfd_active == 2 ? h->hud_colors[1] : hud_color, 1);
		}
	}
}

static void fixed_build_power(const XwaHudState* h, const XwaRemasterHudVisibility* v) {
	if (!v->power)
		return;
	const XwaHudInstruments* c = &h->instruments;
	if (fixed_element(h, 1)) {
		if (c->active_features & 0x200u) {
			const uint32_t color = c->laser_projectile_type[0] != OBJ_LaserRebel ? 0xff00fc0fu : 0xffff3a06u;
			for (int i = 0; i < c->laser_redirect; i++)
				fixed_emit(XWA_HUD_WIDGET_LEFT_POWER, OBJ_HudTextureGroup12000, 12, 3, 442 + i * 10, 256,
						   color, 0);
		}
		if ((c->system_flags & 1u) && (c->active_features & 0x800u))
			for (int i = 0; i < c->shield_redirect; i++)
				fixed_emit(XWA_HUD_WIDGET_LEFT_POWER, OBJ_HudTextureGroup12000, 12, 3, 397 + i * 10, 256,
						   0xffffff00u, 0);
	}
	if (!fixed_element(h, 2) || !(c->active_features & 0x400u))
		return;
	const int has_beam = (c->system_flags & 0x100u) != 0;
	if (has_beam && (c->active_features & 0x1000u))
		for (int i = 0; i < c->beam_level; i++)
			fixed_emit(XWA_HUD_WIDGET_RIGHT_POWER, OBJ_HudTextureGroup12000, 12, 637, 397 + i * 10, 256,
					   0xffc600d7u, 0);
	int reserve = 8 - c->laser_redirect;
	if (has_beam) {
		if (c->system_flags & 1u)
			reserve += 2 - c->shield_redirect;
		reserve += 2 - c->beam_level;
	} else {
		reserve -= c->shield_redirect;
	}
	if (reserve < 0)
		reserve = 0;
	if (has_beam) {
		for (int i = 0; i < reserve; i++)
			fixed_emit(XWA_HUD_WIDGET_RIGHT_POWER, OBJ_HudTextureGroup12000, 14, 637, (int)(438.0 + i * 3.33),
					   256, 0xffffffffu, 0);
	} else {
		for (int i = 0; i < reserve; i++)
			fixed_emit(XWA_HUD_WIDGET_RIGHT_POWER, OBJ_HudTextureGroup12000, 13, 637, 439 + i * 5, 256,
					   0xffffffffu, 0);
	}
}

typedef struct FixedChargePoint {
	int x, y;
} FixedChargePoint;

static void fixed_sort_axis(int order[10], const XwaHudReticle* r, int start, int count, int axis) {
	for (int pass = count - 1; pass >= 0; pass--)
		for (int scan = start; scan < start + pass; scan++)
			if (r->aim_offset[order[scan]][axis] > r->aim_offset[order[scan + 1]][axis]) {
				const int t = order[scan];
				order[scan] = order[scan + 1];
				order[scan + 1] = t;
			}
}

static void fixed_setup_charge_points(const XwaHudState* h, FixedChargePoint laser[16],
									  FixedChargePoint ion[16], int* out_laser, int* out_ion) {
	memset(laser, 0, sizeof(FixedChargePoint) * 16);
	memset(ion, 0, sizeof(FixedChargePoint) * 16);
	int laser_count = 0, ion_count = 0;
	const int total = h->reticle.laser_hardpoint_count > 10 ? 10 : h->reticle.laser_hardpoint_count;
	for (int i = 0; i < total; i++) {
		if (h->instruments.weapon_type[i] == 1)
			laser_count++;
		else if (h->instruments.weapon_type[i] == 2)
			ion_count++;
	}
	*out_laser = laser_count;
	*out_ion = ion_count;
	if (laser_count > 6 || ion_count > 4)
		return;
	int order[10];
	for (int i = 0; i < 10; i++)
		order[i] = i;
	int ion_y_offset = 0;
	if (laser_count == 1)
		laser[0] = (FixedChargePoint) { 320, 126 };
	else if (laser_count == 2) {
		const int swap = h->reticle.aim_offset[0][0] >= h->reticle.aim_offset[1][0];
		laser[0] = (FixedChargePoint) { swap ? 358 : 284, 126 };
		laser[1] = (FixedChargePoint) { swap ? 284 : 358, 126 };
	} else if (laser_count == 3) {
		fixed_sort_axis(order, &h->reticle, 0, 3, 0);
		laser[order[0]] = (FixedChargePoint) { 271, 126 };
		laser[order[1]] = (FixedChargePoint) { 321, 126 };
		laser[order[2]] = (FixedChargePoint) { 370, 126 };
	} else if (laser_count >= 4) {
		fixed_sort_axis(order, &h->reticle, 0, laser_count, 1);
		if (laser_count == 4) {
			if (h->reticle.aim_offset[order[0]][0] > h->reticle.aim_offset[order[1]][0]) {
				int t = order[0];
				order[0] = order[1];
				order[1] = t;
			}
			if (h->reticle.aim_offset[order[2]][0] > h->reticle.aim_offset[order[3]][0]) {
				int t = order[2];
				order[2] = order[3];
				order[3] = t;
			}
			laser[order[0]] = (FixedChargePoint) { 284, 133 };
			laser[order[1]] = (FixedChargePoint) { 358, 133 };
			laser[order[2]] = (FixedChargePoint) { 284, 126 };
			laser[order[3]] = (FixedChargePoint) { 358, 126 };
		} else if (laser_count == 5) {
			/* The five-hardpoint layout globals are zero in XWA 2.02. Keep
			 * the original duplicate lower-right assignment. */
			laser[order[0]] = (FixedChargePoint) { 320, 0 };
			laser[order[1]] = (FixedChargePoint) { 320, 0 };
			laser[order[2]] = (FixedChargePoint) { 320, 0 };
			laser[order[3]] = (FixedChargePoint) { 320, 0 };
			laser[order[4]] = (FixedChargePoint) { 320, 0 };
		} else {
			laser[order[0]] = (FixedChargePoint) { 320, 0 };
			laser[order[1]] = (FixedChargePoint) { 320, 0 };
			laser[order[2]] = (FixedChargePoint) { 320, 0 };
			laser[order[3]] = (FixedChargePoint) { 320, 0 };
			laser[order[4]] = (FixedChargePoint) { 320, 0 };
			laser[order[5]] = (FixedChargePoint) { 320, 0 };
		}
		ion_y_offset = 7;
	}
	if (ion_count == 1)
		ion[0] = (FixedChargePoint) { 320, 133 + ion_y_offset };
	else if (ion_count == 2) {
		const int a = laser_count, swap = h->reticle.aim_offset[a][0] >= h->reticle.aim_offset[a + 1][0];
		ion[0] = (FixedChargePoint) { swap ? 358 : 284, 133 + ion_y_offset };
		ion[1] = (FixedChargePoint) { swap ? 284 : 358, 133 + ion_y_offset };
	} else if (ion_count == 3) {
		fixed_sort_axis(order, &h->reticle, laser_count, 3, 0);
		ion[order[laser_count] - laser_count] = (FixedChargePoint) { 271, 133 + ion_y_offset };
		ion[order[laser_count + 1] - laser_count] = (FixedChargePoint) { 321, 133 + ion_y_offset };
		ion[order[laser_count + 2] - laser_count] = (FixedChargePoint) { 370, 133 + ion_y_offset };
	} else if (ion_count == 4) {
		fixed_sort_axis(order, &h->reticle, laser_count, 4, 1);
		for (int pair = 0; pair < 2; pair++) {
			int a = laser_count + pair * 2;
			if (h->reticle.aim_offset[order[a]][0] > h->reticle.aim_offset[order[a + 1]][0]) {
				int t = order[a];
				order[a] = order[a + 1];
				order[a + 1] = t;
			}
		}
		ion[order[laser_count] - laser_count] = (FixedChargePoint) { 284, 140 + ion_y_offset };
		ion[order[laser_count + 1] - laser_count] = (FixedChargePoint) { 358, 140 + ion_y_offset };
		ion[order[laser_count + 2] - laser_count] = (FixedChargePoint) { 284, 133 + ion_y_offset };
		ion[order[laser_count + 3] - laser_count] = (FixedChargePoint) { 358, 133 + ion_y_offset };
	}
}

static void fixed_emit_energy_bank(const XwaHudState* h, XwaHudWidgetId widget,
								   const FixedChargePoint points[16], int start, int count, int triple,
								   uint32_t primary, uint32_t secondary) {
	const XwaHudInstruments* c = &h->instruments;
	for (int i = 0; i < count; i++) {
		int charge = (int8_t)c->laser_charge[start + i];
		if (!(c->active_features & 2u) || !(c->active_features & 4u) || charge <= 0 ||
			!(c->working_subsystems & 0x10u))
			continue;
		charge++;
		uint32_t color = charge <= 64 ? secondary : primary;
		int display = charge <= 64 ? charge : charge - 64;
		int filled = display / 10;
		if (!filled && (charge < 10 || charge > 64))
			filled = 1;
		if (filled > 6)
			filled = 6;
		int empty = charge > 64 ? 6 - filled : 0;
		const int step = triple ? 7 : 10;
		int x = points[i].x - (triple ? 17 : 24);
		for (int segment = 0; segment < filled; segment++) {
			if (segment)
				x += step;
			fixed_emit(widget, OBJ_HudTextureGroup12000, triple ? 26 : 24, x, points[i].y, 256, color, 0);
		}
		for (int segment = 0; segment < empty; segment++) {
			x += step;
			fixed_emit(widget, OBJ_HudTextureGroup12000, triple ? 26 : 24, x, points[i].y, 256, secondary, 0);
		}
	}
}

static void fixed_build_charge(const XwaHudState* h, const XwaRemasterHudVisibility* v) {
	if (!v->charge || !fixed_element(h, 0))
		return;
	FixedChargePoint laser[16], ion[16];
	int laser_count, ion_count;
	fixed_setup_charge_points(h, laser, ion, &laser_count, &ion_count);
	if (!(h->instruments.installed_features & 2u) || !(h->instruments.installed_features & 4u))
		return;
	const int laser_frame = (laser_count == 3 || laser_count == 6) ? 25 : 23;
	for (int i = 0; i < laser_count; i++)
		fixed_emit(XWA_HUD_WIDGET_LASER_CHARGE, OBJ_HudTextureGroup12000, laser_frame, laser[i].x, laser[i].y,
				   256, h->hud_colors[0], 0);
	const uint32_t laser_primary =
		h->instruments.laser_projectile_type[0] == OBJ_LaserRebel ? 0xffff0000u : 0xff00fc0fu;
	const uint32_t laser_secondary =
		h->instruments.laser_projectile_type[0] == OBJ_LaserRebel ? 0xff9b1e00u : 0xff008c00u;
	fixed_emit_energy_bank(h, XWA_HUD_WIDGET_LASER_CHARGE, laser, 0, laser_count, laser_count == 3,
						   laser_primary, laser_secondary);
	const int ion_frame = ion_count == 3 ? 25 : 23;
	for (int i = 0; i < ion_count; i++)
		fixed_emit(XWA_HUD_WIDGET_ION_CHARGE, OBJ_HudTextureGroup12000, ion_frame, ion[i].x, ion[i].y, 256,
				   h->hud_colors[0], 0);
	fixed_emit_energy_bank(h, XWA_HUD_WIDGET_ION_CHARGE, ion, laser_count, ion_count, ion_count == 3,
						   0xff00b4ffu, 0xff006992u);
}

static uint16_t fixed_q16_div(uint32_t numerator, uint32_t denominator) {
	if (numerator == denominator || denominator == 0 || numerator >= denominator)
		return 0xffffu;
	while (numerator > 0xffffu || denominator > 0xffffu) {
		numerator >>= 1;
		denominator >>= 1;
	}
	return (uint16_t)((numerator << 16) / denominator);
}

static uint16_t fixed_q16_fraction(uint32_t value, uint16_t fraction) {
	if (fraction == 0xffffu)
		return (uint16_t)value;
	return (uint16_t)(((uint32_t)fraction * (value & 0xffffu) >> 16) + fraction * (value >> 16));
}

static void fixed_shield_segments(int shield, int half_max, uint16_t* lower, uint16_t* upper) {
	if (shield < 0)
		shield = 0;
	if (shield < half_max) {
		*lower = fixed_q16_fraction(9, fixed_q16_div((uint32_t)shield, (uint32_t)half_max));
		*upper = 0;
	} else {
		*lower = 9;
		*upper = fixed_q16_fraction(9, fixed_q16_div((uint32_t)(shield - half_max), (uint32_t)half_max));
	}
}

static void fixed_build_shield(const XwaHudState* h, const XwaRemasterHudVisibility* v) {
	if (!v->shield_hull || !fixed_element(h, 1))
		return;
	const XwaHudInstruments* c = &h->instruments;
	fixed_emit(XWA_HUD_WIDGET_SHIELD_HULL, OBJ_HudTextureGroup12000, 43, 38, 338, 256, h->hud_colors[0], 0);
	/* With a shield generator installed, the classic function returns
	 * before drawing the hull silhouette when its HUD feature is inactive.
	 * Craft without a generator still draw the silhouette. */
	if ((c->system_flags & 1u) && !(c->active_features & 0x20u))
		return;
	const int hull_third = c->hull_max / 3;
	int color_index = 2;
	if (c->hull_damage_flash)
		color_index = 3;
	else if (hull_third) {
		int damage_level = c->hull_damage / hull_third;
		if (damage_level > 2)
			damage_level = 2;
		color_index = 2 - damage_level;
	}
	static const uint32_t hull_colors[4] = { 0xffff0000u, 0xffffff00u, 0xff00ff00u, 0xffffffffu };
	if (c->shield_silhouette_sprite)
		fixed_emit(XWA_HUD_WIDGET_SHIELD_HULL, OBJ_HullIconTextureGroup26000,
				   c->shield_silhouette_sprite / 100, 43, 340, 256, hull_colors[color_index], 0);
	if (!(c->system_flags & 1u))
		return;
	uint16_t fl, fu, rl, ru;
	fixed_shield_segments(c->shield_front, c->shield_max / 2, &fl, &fu);
	fixed_shield_segments(c->shield_rear, c->shield_max / 2, &rl, &ru);
	if (!(c->working_subsystems & 1u))
		fl = fu = rl = ru = 0;
	if (c->shield_damage_flash && !c->last_shield_damage_side) {
		if (fu)
			fu = 10;
		else
			fl = 10;
		if (ru)
			ru = 10;
		else
			rl = 10;
	}
	static const uint32_t colors[11] = { 0xff000000u, 0xff700000u, 0xffc00d0du, 0xffff1111u,
										 0xff888900u, 0xffc9b400u, 0xffffe603u, 0xff007c00u,
										 0xff00b80au, 0xff00f40du, 0xffffffffu };
	if (fu)
		fixed_emit(XWA_HUD_WIDGET_SHIELD_HULL, OBJ_HudTextureGroup12000, 41, 44, 355, 256, colors[fu], 0);
	if (fl)
		fixed_emit(XWA_HUD_WIDGET_SHIELD_HULL, OBJ_HudTextureGroup12000, 39, 44, 353, 256, colors[fl], 0);
	if (ru)
		fixed_emit(XWA_HUD_WIDGET_SHIELD_HULL, OBJ_HudTextureGroup12000, 42, 44, 323, 256, colors[ru], 0);
	if (rl)
		fixed_emit(XWA_HUD_WIDGET_SHIELD_HULL, OBJ_HudTextureGroup12000, 40, 44, 325, 256, colors[rl], 0);
}

static void fixed_build_beam(const XwaHudState* h, const XwaRemasterHudVisibility* v) {
	if (!v->beam || !fixed_element(h, 2))
		return;
	const XwaHudInstruments* c = &h->instruments;
	if (!(c->installed_features & 0x10u))
		return;
	if (!(c->active_features & 0x10u)) {
		fixed_emit(XWA_HUD_WIDGET_BEAM, OBJ_HudTextureGroup12000, 44, 602, 338, 256, h->hud_colors[0], 0);
		return;
	}
	int present = c->beam_present;
	if (!(c->working_subsystems & 0x100u))
		present = 0;
	const int active = c->beam_active && (c->working_subsystems & 0x100u);
	fixed_emit(XWA_HUD_WIDGET_BEAM, OBJ_HudTextureGroup12000, 44, 602, 338, 256, h->hud_colors[0], 0);
	fixed_emit(XWA_HUD_WIDGET_BEAM, OBJ_HudTextureGroup12000, 29, 597, 339, 256,
			   active ? 0xffff0000u : 0xff730000u, 0);
	int screen_y = 317;
	static const int offsets[8] = { 3, 4, 2, 3, 5, 5, 6, 7 };
	for (int segment = 0; segment < 9; segment++) {
		int units = present - 1000 * segment;
		if (units > 1000)
			units = 1000;
		const int level = units / 333;
		if (level > 0) {
			const uint32_t color = level == 1 ? 0xff2e13c3u : level == 2 ? 0xff557fd7u : 0xff6fb1dbu;
			fixed_emit(XWA_HUD_WIDGET_BEAM, OBJ_HudTextureGroup12000, 30 + segment, 597, screen_y, 256, color,
					   0);
		}
		if (segment < 8)
			screen_y += offsets[segment];
	}
}

static void fixed_build_radar(const XwaHudState* h, const XwaRemasterHudVisibility* v) {
	if (!v->radar_blips || h->radar_classic_radius == 0)
		return;
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	const XwaHudLayoutProfile* profile = fixed_profile((XwaHudProfileIndex)fixed_draws.profile);
	if (!layout || !profile)
		return;
	int marker_present[2] = { 0, 0 };
	float marker_x[2] = { 0.0f, 0.0f };
	float marker_y[2] = { 0.0f, 0.0f };
	float marker_scale[2] = { 0.0f, 0.0f };
	for (int radar = 0; radar < 2; radar++) {
		const int element = radar ? 2 : 1;
		if (!fixed_element(h, element))
			continue;
		const XwaHudWidgetId blip_widget =
			radar ? XWA_HUD_WIDGET_AFT_RADAR_BLIPS : XWA_HUD_WIDGET_FORE_RADAR_BLIPS;
		/* Blips are centered on g_hudRadarCenterOffsetX/Y (61,45), not
		 * the scope-sprite placement offsets (55,47). The two pairs are
		 * deliberately different in Hud_InitHUD: using the scope offsets
		 * moves both radars down and pushes them symmetrically outward. */
		float center_ref_x, center_ref_y, ignored_scale;
		fixed_point_ref(blip_widget, radar ? 579.0f : 61.0f, 45.0f, &center_ref_x, &center_ref_y,
						&ignored_scale);
		const float radius_x = h->radar_classic_radius * fixed_classic_pixel_scale;
		const float radius_y = radius_x;
		const float scale_x = radius_x / h->radar_classic_radius;
		const float scale_y = radius_y / h->radar_classic_radius;
		const float sprite_scale = fixed_hud_unit_scale;
		for (uint16_t i = 0; i < h->radar_blip_count; i++) {
			const XwaHudRadarBlip* blip = &h->radar_blips[i];
			if (blip->radar != radar)
				continue;
			const float x = center_ref_x + blip->local_x * scale_x;
			const float y = center_ref_y + blip->local_y * scale_y;
			fixed_emit_radar_record(blip_widget, 47, x, y, sprite_scale, 64,
									XwaSnapshotExport_FlightPaletteColor(blip->color_index));
		}
		if (h->radar_target_marker_visible && h->radar_target_marker_radar == radar) {
			marker_present[radar] = 1;
			marker_x[radar] = center_ref_x + h->radar_target_marker_local_x * scale_x;
			marker_y[radar] = center_ref_y + h->radar_target_marker_local_y * scale_y;
			marker_scale[radar] = sprite_scale;
		} else {
			/* Compatibility with snapshots produced before the dedicated
			 * marker descriptor: the selected accepted blip is equivalent. */
			for (uint16_t i = 0; i < h->radar_blip_count; i++) {
				const XwaHudRadarBlip* blip = &h->radar_blips[i];
				if (blip->radar == radar && blip->targeted) {
					marker_present[radar] = 1;
					marker_x[radar] = center_ref_x + blip->local_x * scale_x;
					marker_y[radar] = center_ref_y + blip->local_y * scale_y;
					marker_scale[radar] = sprite_scale;
					break;
				}
			}
		}
		/* The classic draws the selected blip with the ordinary point array,
		 * then overlays frame 48. Preserve that ordering while keeping the
		 * marker attached to the same authored radar. */
	}
	for (int radar = 0; radar < 2; radar++) {
		if (!marker_present[radar])
			continue;
		const XwaHudWidgetId marker_widget =
			radar ? XWA_HUD_WIDGET_AFT_TARGET_MARKER : XWA_HUD_WIDGET_FORE_TARGET_MARKER;
		fixed_emit_radar_record(marker_widget, 48, marker_x[radar], marker_y[radar], marker_scale[radar], 256,
								0xffffffffu);
	}
}

static const XwaFlightObject* fixed_player_object(const XwaSnapshot* snapshot, const XwaHudState* h) {
	if (!snapshot)
		return NULL;
	for (uint32_t i = 0; i < snapshot->flight_object_count; i++) {
		const XwaFlightObject* object = &snapshot->flight_objects[i];
		if (object->slot == h->player_slot && object->signature == h->player_signature)
			return object;
	}
	return NULL;
}

static const XwaFlightObject* fixed_target_object(const XwaSnapshot* snapshot, const XwaHudState* h) {
	if (!snapshot || !h->target.valid)
		return NULL;
	for (uint32_t i = 0; i < snapshot->flight_object_count; i++) {
		const XwaFlightObject* object = &snapshot->flight_objects[i];
		if (object->slot == h->target.slot && object->signature == h->target.signature)
			return object;
	}
	return NULL;
}

static int16_t fixed_arrow_angle(float dx, float dy) {
	const float angle = atan2f(fabsf(dx), fabsf(dy));
	return (int16_t)lroundf(angle * (65536.0f / (2.0f * 3.14159265358979323846f)));
}

static void fixed_clamp_arrow(float projected_x, float projected_y, float width, float height, float padding,
							  int behind, XwaHudProjectedArrow* out) {
	const float center_x = width * 0.5f;
	const float center_y = height * 0.5f;
	int16_t angle = fixed_arrow_angle(center_x - projected_x, center_y - projected_y);
	float edge_x = projected_x;
	float edge_y = (projected_y < 0.0f || projected_y > height) ? projected_y : height - projected_y;
	float quad_x = projected_x;
	float quad_y = edge_y;
	int16_t rotation = 0;

	if (behind && projected_x >= 0.0f && projected_x <= width && projected_y >= 0.0f &&
		projected_y <= height) {
		quad_y = padding;
		rotation = 32760;
	} else {
		if (projected_x < 0.0f) {
			edge_x = quad_x = padding;
			rotation = edge_y < center_y ? (int16_t)(32760 - angle) : angle;
		}
		if (edge_x > width) {
			edge_x = quad_x = width - padding;
			rotation = edge_y < center_y ? (int16_t)(angle + 32760) : (int16_t)-angle;
		}
		if (edge_y < 0.0f) {
			edge_y = quad_y = height - padding;
			rotation = edge_x < center_x ? angle : (int16_t)-angle;
		}
		if (edge_y > height) {
			edge_y = quad_y = padding;
			rotation = edge_x < center_x ? (int16_t)(32760 - angle) : (int16_t)(32760 + angle);
			edge_x = quad_x;
		}
		if (edge_x <= padding && edge_y >= height - padding) {
			edge_x = quad_x = padding;
			edge_y = quad_y = height - padding;
			rotation = angle;
		}
		if (edge_x >= width - padding && edge_y >= height - padding) {
			edge_x = quad_x = width - padding;
			edge_y = quad_y = height - padding;
			rotation = (int16_t)-angle;
		}
		if (edge_x <= padding && edge_y <= padding) {
			edge_x = quad_x = padding;
			edge_y = quad_y = padding;
			rotation = (int16_t)(32760 - angle);
		}
		if (edge_x >= width - padding && edge_y <= padding) {
			quad_x = width - padding;
			quad_y = padding;
			rotation = (int16_t)(32760 + angle);
		}
	}
	out->center_x_ref = quad_x;
	out->center_y_ref = height - quad_y;
	out->rotation_angle = rotation;
}

static void fixed_clamp_padlock(float projected_x, float projected_y, float width, float height,
								float padding, XwaHudProjectedArrow* out) {
	const float center_x = width * 0.5f;
	const float center_y = height * 0.5f;
	const int16_t angle = fixed_arrow_angle(center_x - projected_x, center_y - projected_y);
	float quad_x = projected_x;
	float quad_y = (projected_y < 0.0f || projected_y > height) ? projected_y : height - projected_y;
	int16_t rotation = 0;
	if (quad_x < 0.0f) {
		quad_x = padding;
		rotation = quad_y < center_y ? (int16_t)-angle : (int16_t)(angle + 32760);
	}
	if (quad_x > width) {
		quad_x = width - padding;
		rotation = quad_y < center_y ? angle : (int16_t)(32760 - angle);
	}
	if (quad_y < 0.0f) {
		quad_y = height - padding;
		rotation = quad_x < center_x ? (int16_t)(angle + 32760) : (int16_t)(32760 - angle);
	}
	if (quad_y > height - padding) {
		quad_y = padding;
		rotation = quad_x < center_x ? (int16_t)-angle : angle;
	}
	out->center_x_ref = quad_x;
	out->center_y_ref = height - quad_y;
	out->rotation_angle = rotation;
}

int XwaRemasterHudFixed_ProjectTargetArrow(const XwaSnapshot* snapshot, XwaHudProfileIndex profile_id,
										   const XwaRemasterFlightView* flight_view, int target_w,
										   int target_h, XwaHudProjectedArrow* out) {
	if (!out)
		return 0;
	memset(out, 0, sizeof *out);
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	if (!snapshot || !snapshot->hud.valid || !snapshot->flight_camera_valid || !layout || !flight_view ||
		target_w <= 0 || target_h <= 0)
		return 0;
	const XwaHudLayoutProfile* profile = fixed_profile(profile_id);
	if (!profile || !profile->valid)
		return 0;
	const XwaHudLayoutAnchor* bounds = &profile->anchors[XWA_HUD_ANCHOR_TARGET_EDGE_BOUNDS];
	int output_x, output_y;
	float output_scale;
	XwaRemasterHudLayout_OutputTransform(profile, target_w, target_h, &output_x, &output_y, &output_scale);
	const float bounds_x = output_x + bounds->rect.x * output_scale;
	const float bounds_y = output_y + bounds->rect.y * output_scale;
	const float width = bounds->rect.w * output_scale;
	const float height = bounds->rect.h * output_scale;
	if (width <= 0.0f || height <= 0.0f)
		return 0;
	const XwaHudState* h = &snapshot->hud;
	float target_x, target_y;
	int behind = 0;
	if (h->target.padlock_active) {
		const XwaFlightCamera* camera = &snapshot->flight_camera;
		const float projection = camera->proj_scale > 0.0f ? camera->proj_scale : 512.0f;
		const float yaw_px = truncf(h->reticle.look_yaw * 0.05405405405405406f);
		const float pitch_px = truncf(h->reticle.look_pitch * -0.05405405405405406f);
		const float eye[3] = { -yaw_px / projection, -pitch_px / projection, 1.0f };
		if (!XwaRemasterFlight_ProjectView(flight_view, eye, &target_x, &target_y))
			return 0;
		out->padlock = 1;
	} else {
		const XwaFlightObject* target = fixed_target_object(snapshot, h);
		if (!target)
			return 0;
		const XwaFlightCamera* camera = &snapshot->flight_camera;
		float delta[3];
		XwaRemasterWorld_DeltaI32(target->world_pos, camera->world_pos, delta);
		float view[3];
		for (int row = 0; row < 3; row++)
			view[row] = camera->rows[row * 3] * delta[0] + camera->rows[row * 3 + 1] * delta[1] +
						camera->rows[row * 3 + 2] * delta[2];
		behind = view[2] < 0.0f;
		if (behind) {
			view[2] = -view[2];
			if (view[2] < 0.0001f)
				view[2] = 0.0001f;
			if (!XwaRemasterFlight_ProjectView(flight_view, view, &target_x, &target_y))
				return 0;
		} else if (!XwaRemasterFlight_ProjectWorldI32(flight_view, target->world_pos, &target_x, &target_y,
													  NULL)) {
			return 0;
		}
	}
	const AeronRectI scene = flight_view->viewport;
	if (!behind && target_x >= scene.x && target_x < scene.x + scene.width && target_y >= scene.y &&
		target_y < scene.y + scene.height)
		return 0;
	const float projected_x = target_x - bounds_x;
	const float projected_y = target_y - bounds_y;
	const float padding = 6.0f * flight_view->classic_pixel_scale;
	XwaHudProjectedArrow clamped = { 0 };
	if (out->padlock)
		fixed_clamp_padlock(projected_x, projected_y, width, height, padding, &clamped);
	else
		fixed_clamp_arrow(projected_x, projected_y, width, height, padding, behind, &clamped);
	if (!XwaRemasterHudLayout_TargetToReference(profile, target_w, target_h, bounds_x + clamped.center_x_ref,
												bounds_y + clamped.center_y_ref, &out->center_x_ref,
												&out->center_y_ref))
		return 0;
	out->rotation_angle = clamped.rotation_angle;
	out->visible = 1;
	out->behind = (uint8_t)behind;
	return 1;
}

static void fixed_build_target_arrow(const XwaHudState* h, const XwaRemasterHudVisibility* v) {
	if (!v->target_arrow || !h->target.valid || !(h->instruments.working_subsystems & 4u))
		return;
	if (!XwaRemasterHudFixed_ProjectTargetArrow(fixed_snapshot, (XwaHudProfileIndex)fixed_draws.profile,
												fixed_flight_view, fixed_target_w, fixed_target_h,
												&fixed_target_arrow))
		return;
	const XwaHudProjectedArrow arrow = fixed_target_arrow;
	const XwaHudWidgetId widget = arrow.padlock ? XWA_HUD_WIDGET_PADLOCK_ARROW : XWA_HUD_WIDGET_TARGET_ARROW;
	const XwaHudWidgetDesc* desc = fixed_widget(widget);
	if (!desc || fixed_draws.record_count >= XWA_HUD_MAX_FIXED_DRAWS) {
		fixed_draws.dropped_records++;
		return;
	}
	XwaHudDrawRecord* record = &fixed_draws.records[fixed_draws.record_count++];
	memset(record, 0, sizeof *record);
	record->widget = desc->id;
	record->anchor = desc->anchor;
	record->object_type = OBJ_HudTextureGroup13000_Sprite000;
	record->frame = 1;
	record->screen_size = 256;
	record->phase = desc->phase;
	record->stable_order = desc->stable_order;
	record->sequence = fixed_draws.record_count - 1;
	record->rotation_angle = arrow.rotation_angle;
	record->center_x_ref = arrow.center_x_ref;
	record->center_y_ref = arrow.center_y_ref;
	record->scale_ref = fixed_hud_unit_scale;
	record->argb = arrow.behind ? 0xffc8c800u : 0xffe6e600u;
}

static int fixed_project_reticle(const XwaSnapshot* snapshot, XwaHudProfileIndex profile_id,
								 const XwaRemasterFlightView* flight_view, int target_w, int target_h,
								 float* out_x_ref, float* out_y_ref) {
	if (!snapshot || !snapshot->flight_camera_valid || !flight_view || !out_x_ref || !out_y_ref)
		return 0;
	const XwaHudLayoutProfile* profile = fixed_profile(profile_id);
	if (!profile)
		return 0;
	const XwaHudState* h = &snapshot->hud;
	float target_x, target_y;
	const float center_eye[3] = { 0.0f, 0.0f, 1.0f };
	if (!XwaRemasterFlight_ProjectView(flight_view, center_eye, &target_x, &target_y))
		return 0;
	if (h->reticle.look_yaw || h->reticle.look_pitch) {
		const XwaFlightObject* player = fixed_player_object(snapshot, h);
		if (player && player->has_mobj) {
			float local[3];
			XwaRemasterWorld_LocalI32(flight_view->origin_world, player->world_pos, local);
			for (int axis = 0; axis < 3; axis++)
				local[axis] += player->rows[3 + axis] * (1000000.0f / 32768.0f);
			XwaRemasterFlight_ProjectLocal(flight_view, local, &target_x, &target_y, NULL);
		}
	}
	return XwaRemasterHudLayout_TargetToReference(profile, target_w, target_h, target_x, target_y, out_x_ref,
												  out_y_ref);
}

int XwaRemasterHudFixed_ReticleCenter(float* out_x_ref, float* out_y_ref) {
	if (!fixed_reticle_valid || !out_x_ref || !out_y_ref)
		return 0;
	*out_x_ref = fixed_reticle_x_ref;
	*out_y_ref = fixed_reticle_y_ref;
	return 1;
}

const XwaHudProjectedArrow* XwaRemasterHudFixed_TargetArrow(void) {
	return fixed_target_arrow.visible ? &fixed_target_arrow : NULL;
}

static void fixed_emit_reticle_group(XwaHudWidgetId widget_id, int frame, float dx, float dy,
									 int density_scaled, uint32_t argb) {
	const XwaHudLayoutProfile* profile = fixed_profile((XwaHudProfileIndex)fixed_draws.profile);
	const XwaHudWidgetDesc* widget = fixed_widget(widget_id);
	if (!profile || !widget || !fixed_reticle_valid)
		return;
	const float offset_scale = density_scaled ? fixed_hud_unit_scale : fixed_classic_pixel_scale;
	const float offset_x = dx * offset_scale;
	const float offset_y = dy * offset_scale;
	const float size_scale = fixed_hud_unit_scale;
	fixed_emit_ref(widget_id, OBJ_HudTextureGroup12000, frame, fixed_reticle_x_ref + offset_x,
				   fixed_reticle_y_ref + offset_y, size_scale, 256, argb, 0);
}

static void fixed_build_reticle_threats(const XwaHudState* h, const XwaRemasterHudVisibility* v) {
	const XwaHudReticle* r = &h->reticle;
	if (abs(r->look_yaw) >= 8192 || abs(r->look_pitch) >= 8192)
		return;
	if (v->reticle && r->visible) {
		if (r->weapon_mode == 0) {
			if (r->seat == 0) {
				for (int i = 0; i < r->laser_hardpoint_count && i < 16; i++) {
					if (r->laser_hardpoint_index[i] == -1)
						continue;
					fixed_emit_reticle_group(XWA_HUD_WIDGET_READINESS, 10, r->aim_offset[i][0],
											 r->aim_offset[i][1], 0, h->hud_colors[0]);
					if (r->ready[i])
						fixed_emit_reticle_group(XWA_HUD_WIDGET_READINESS, 9, r->aim_offset[i][0],
												 r->aim_offset[i][1], 0, 0xffffffffu);
				}
			}
			fixed_emit_reticle_group(XWA_HUD_WIDGET_RETICLE, 5, 0.0f, 0.0f, 1, h->hud_colors[0]);
			if (r->in_range)
				fixed_emit_reticle_group(XWA_HUD_WIDGET_RETICLE, 6, 0.0f, 0.0f, 1, 0xffffffffu);
		} else if (r->weapon_mode == 1) {
			fixed_emit_reticle_group(XWA_HUD_WIDGET_RETICLE, 7, 0.0f, 0.0f, 1, h->hud_colors[0]);
			if (h->target.valid && (r->missile_lock_state == 2 || r->missile_lock_state == 3)) {
				uint32_t lock_color = 0xffff0000u;
				if (r->missile_lock_state == 2) {
					const XwaFlightObject* player = fixed_player_object(fixed_snapshot, h);
					const int lock_range = player && player->object_type == OBJ_MissileBoat ? 354 : 708;
					const int divisor = lock_range / 200;
					const int ticks = (int16_t)h->instruments.warhead_lock_ticks;
					const int step = ticks / divisor;
					lock_color = (uint32_t)(ticks > step ? (ticks == lock_range ? -65536 : -256 * (step + 1))
														 : -3604736);
				}
				fixed_emit_reticle_group(XWA_HUD_WIDGET_RETICLE, 8, 0.0f, 0.0f, 1, lock_color);
			}
		}
	}
	if (!v->threats)
		return;
	const int laser = r->weapon_mode == 0;
	const int states[4] = { h->threats.laser, h->threats.turret, h->threats.beam, h->threats.missile };
	const int dx_l[4] = { -18, -8, 9, 19 }, dy_l[4] = { -28, -33, -33, -28 };
	const int dx_w[4] = { -19, -6, 7, 19 }, dy_w[4] = { -34, -32, -32, -34 };
	for (int i = 0; i < 4; i++) {
		uint32_t color;
		if (i == 3)
			color = states[i] == 0 ? h->hud_colors[0] : (states[i] > 1 ? 0xffff0000u : 0xfffcd400u);
		else
			color = states[i] ? 0xff10bc00u : h->hud_colors[0];
		fixed_emit_reticle_group(XWA_HUD_WIDGET_THREATS, (laser ? 15 : 19) + i,
								 (float)(laser ? dx_l[i] : dx_w[i]), (float)(laser ? dy_l[i] : dy_w[i]), 1,
								 color);
	}
}

int XwaRemasterHudFixed_Init(void) {
	if (!fixed_list)
		fixed_list = AeronDrawList_Create(XWA_HUD_MAX_FIXED_DRAWS);
	return fixed_list != NULL;
}

void XwaRemasterHudFixed_Shutdown(void) {
	AeronDrawList_Destroy(fixed_list);
	fixed_list = NULL;
	fixed_snapshot = NULL;
	fixed_hud_unit_scale = 1.0f;
	fixed_classic_pixel_scale = 1.0f;
	fixed_flight_view = NULL;
	fixed_target_w = fixed_target_h = 0;
	fixed_reticle_valid = 0;
	memset(&fixed_target_arrow, 0, sizeof fixed_target_arrow);
	memset(&fixed_draws, 0, sizeof fixed_draws);
}

void XwaRemasterHudFixed_Build(const XwaSnapshot* snapshot, XwaHudProfileIndex profile,
							   uint32_t bundle_generation, const XwaRemasterFlightView* flight_view,
							   int target_w, int target_h) {
	memset(&fixed_draws, 0, sizeof fixed_draws);
	memset(&fixed_target_arrow, 0, sizeof fixed_target_arrow);
	fixed_reticle_valid = 0;
	fixed_hud_unit_scale = 1.0f;
	fixed_classic_pixel_scale = 1.0f;
	fixed_snapshot = snapshot;
	fixed_flight_view = flight_view;
	fixed_target_w = target_w;
	fixed_target_h = target_h;
	const XwaHudState* hud = snapshot ? &snapshot->hud : NULL;
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	const XwaHudLayoutProfile* selected = fixed_profile(profile);
	const uint32_t special_modes = XWA_HUD_MODE_HANGAR_READY | XWA_HUD_MODE_MAP;
	if (!hud || (!hud->valid && !(hud->mode_flags & special_modes)) || !layout || !selected ||
		!selected->valid) {
		fixed_snapshot = NULL;
		fixed_flight_view = NULL;
		fixed_target_w = fixed_target_h = 0;
		return;
	}
	fixed_draws.layout_generation = layout->generation;
	fixed_draws.bundle_generation = bundle_generation;
	fixed_draws.profile = (uint8_t)profile;
	fixed_draws.valid = 1;
	/* The original uses screen edges or screen center for placement and
	 * g_flightHudScaleFactor only for local offsets and size. Scale those
	 * recovered HUD units by the output/source height ratio. */
	const float classic_screen_h =
		snapshot->flight_camera.screen_h > 0 ? (float)snapshot->flight_camera.screen_h : 480.0f;
	const float classic_hud_scale = hud->classic_hud_scale > 0.0f ? hud->classic_hud_scale : 1.0f;
	fixed_classic_pixel_scale = target_h / classic_screen_h;
	fixed_hud_unit_scale = classic_hud_scale * fixed_classic_pixel_scale;
	fixed_reticle_valid = fixed_project_reticle(snapshot, profile, flight_view, target_w, target_h,
												&fixed_reticle_x_ref, &fixed_reticle_y_ref);
	XwaRemasterHudVisibility visibility;
	XwaRemasterHud_BuildVisibility(hud, &visibility);
	fixed_build_frames(hud, &visibility);
	fixed_build_power(hud, &visibility);
	fixed_build_charge(hud, &visibility);
	fixed_build_shield(hud, &visibility);
	fixed_build_beam(hud, &visibility);
	fixed_build_radar(hud, &visibility);
	fixed_build_reticle_threats(hud, &visibility);
	fixed_build_target_arrow(hud, &visibility);
	qsort(fixed_draws.records, fixed_draws.record_count, sizeof fixed_draws.records[0],
		  fixed_compare_draw_records);
	fixed_hud_unit_scale = 1.0f;
	fixed_classic_pixel_scale = 1.0f;
	fixed_snapshot = NULL;
	fixed_flight_view = NULL;
	fixed_target_w = fixed_target_h = 0;
}

void XwaRemasterHudFixed_PrepareDrawList(AeronCommandBuffer* cmd, int target_w, int target_h) {
	if (!cmd || !fixed_list || target_w <= 0 || target_h <= 0)
		return;
	AeronDrawList_Begin(fixed_list, NULL, target_w, target_h, AERON_DRAWLIST2D_LOAD, NULL);
	if (!fixed_draws.valid) {
		(void)AeronDrawList_Prepare(fixed_list, cmd);
		return;
	}
	const XwaHudLayoutProfile* profile = fixed_profile((XwaHudProfileIndex)fixed_draws.profile);
	if (!profile) {
		(void)AeronDrawList_Prepare(fixed_list, cmd);
		return;
	}
	int out_x, out_y;
	float scale;
	XwaRemasterHudLayout_OutputTransform(profile, target_w, target_h, &out_x, &out_y, &scale);
	if (scale <= 0.0f) {
		(void)AeronDrawList_Prepare(fixed_list, cmd);
		return;
	}
	XwaRemasterHud_BeginRenderPhase();
	for (uint16_t i = 0; i < fixed_draws.record_count; i++) {
		const XwaHudDrawRecord* r = &fixed_draws.records[i];
		const XwaAssetRef* asset = XwaRemasterHud_AssetFrame(r->object_type, r->frame);
		if (!asset || asset->classic_w <= 0 || asset->classic_h <= 0)
			continue;
		const float size_scale = (float)r->screen_size / 256.0f * r->scale_ref * scale;
		const float w = asset->classic_w * size_scale;
		const float h = asset->classic_h * size_scale;
		const float a = ((r->argb >> 24) & 255) / 255.0f;
		AeronDrawList2DSprite sprite = { 0 };
		sprite.texture = asset->texture;
		sprite.src_u0 = r->mirror_x ? asset->u1 : asset->u0;
		sprite.src_u1 = r->mirror_x ? asset->u0 : asset->u1;
		sprite.src_v0 = asset->v0;
		sprite.src_v1 = asset->v1;
		sprite.dst_x = out_x + r->center_x_ref * scale - w * 0.5f;
		sprite.dst_y = out_y + r->center_y_ref * scale - h * 0.5f;
		sprite.dst_w = w;
		sprite.dst_h = h;
		sprite.tint[0] = XwaRemaster_SrgbToLinear(((r->argb >> 16) & 255) / 255.0f) * a;
		sprite.tint[1] = XwaRemaster_SrgbToLinear(((r->argb >> 8) & 255) / 255.0f) * a;
		sprite.tint[2] = XwaRemaster_SrgbToLinear((r->argb & 255) / 255.0f) * a;
		sprite.tint[3] = a;
		sprite.blend = AERON_BLIT2D_BLEND_PMA;
		sprite.filter = AERON_BLIT2D_FILTER_LINEAR;
		if (r->widget == XWA_HUD_WIDGET_TARGET_ARROW || r->widget == XWA_HUD_WIDGET_PADLOCK_ARROW) {
			const float theta = r->rotation_angle * (2.0f * 3.14159265358979323846f / 65536.0f);
			const float c = cosf(theta), s = sinf(theta), hw = w * 0.5f, hh = h * 0.5f;
			const float cx = out_x + r->center_x_ref * scale;
			const float cy = out_y + r->center_y_ref * scale;
			static const float local[4][2] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
			AeronDrawList2DQuad4 quad = { 0 };
			for (int corner = 0; corner < 4; corner++) {
				const float x = local[corner][0] * hw;
				const float y = local[corner][1] * hh;
				/* RenderQuad_DrawRotatedSprite uses x'=cos*x+sin*y,
				 * y'=cos*y-sin*x in top-origin screen space. Using the
				 * conventional opposite-sign matrix reverses every diagonal
				 * target-arrow rotation while leaving 0/180 degrees looking
				 * deceptively correct. */
				quad.corners[corner][0] = cx + c * x + s * y;
				quad.corners[corner][1] = cy - s * x + c * y;
				quad.q[corner] = 1.0f;
			}
			quad.corners[0][2] = quad.corners[2][2] = sprite.src_u0;
			quad.corners[1][2] = quad.corners[3][2] = sprite.src_u1;
			quad.corners[0][3] = quad.corners[1][3] = sprite.src_v0;
			quad.corners[2][3] = quad.corners[3][3] = sprite.src_v1;
			memcpy(quad.tint, sprite.tint, sizeof quad.tint);
			quad.texture = sprite.texture;
			quad.blend = sprite.blend;
			quad.filter = sprite.filter;
			AeronDrawList_AddQuad4(fixed_list, &quad);
		} else {
			AeronDrawList_AddSprite(fixed_list, &sprite);
		}
	}
	XwaRemasterHud_EndRenderPhase();
	(void)AeronDrawList_Prepare(fixed_list, cmd);
}

void XwaRemasterHudFixed_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
								int target_w, int target_h) {
	if (!cmd || !pass || !target || !fixed_list || !fixed_draws.valid || target_w <= 0 ||
		target_h <= 0)
		return;
	AeronDrawList_RenderIntoPass(fixed_list, cmd, pass, target);
}

const XwaHudPreparedDrawState* XwaRemasterHud_PreparedDrawState(void) { return &fixed_draws; }

void XwaRemasterHud_RenderTargetBoxes(AeronCommandBuffer* cmd, AeronRenderPass* pass,
									  AeronRenderTarget* target, int target_w, int target_h,
									  XwaHudTargetBoxLayer layer) {
	XwaRemasterHudBoxes_Render(cmd, pass, target, target_w, target_h, layer);
}

void XwaRemasterHud_RenderFixed(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
								int target_w, int target_h) {
	XwaRemasterHudFixed_Render(cmd, pass, target, target_w, target_h);
}

void XwaRemasterHud_RenderCmd(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
							  int target_w, int target_h) {
	XwaRemasterHudCmd_Render(cmd, pass, target, target_w, target_h);
}

void XwaRemasterHud_RenderText(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
							   int target_w, int target_h) {
	XwaRemasterHudText_Render(cmd, pass, target, target_w, target_h);
}

void XwaRemasterHud_Render2D(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
							 int target_w, int target_h) {
	XwaRemasterHud_RenderFixed(cmd, pass, target, target_w, target_h);
	XwaRemasterHud_RenderCmd(cmd, pass, target, target_w, target_h);
	XwaRemasterHud_RenderText(cmd, pass, target, target_w, target_h);
}
