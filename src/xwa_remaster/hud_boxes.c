#include "xwa_remaster/hud_boxes.h"

#include "aeron/scene/draw_list2d.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/hud_fixed.h"
#include "xwa_remaster/hud_layout.h"
#include "xwa_runtime/config/modern_target_readout_options.h"
#include "xwa_runtime/models/craft_hud.h"
#include "xwa_runtime/timing/host_clock.h"

#include <math.h>
#include <string.h>

static AeronDrawList2D* box_lists[2];
static XwaHudPreparedBoxState box_prepared;

static const XwaFlightObject* box_find_object(const XwaSnapshot* snapshot, const XwaHudTargetBox* box) {
	return XwaSnapshot_FindFlightObject(snapshot, box->slot, box->signature);
}

static void box_output_transform(const XwaHudLayoutProfile* profile, int target_w, int target_h, int* out_x,
								 int* out_y, float* out_scale) {
	XwaRemasterHudLayout_OutputTransform(profile, target_w, target_h, out_x, out_y, out_scale);
}

/* Makes a linear colour premultiplied, with full alpha. */
static void box_opaque(const float rgb[3], float out[4]) {
	out[0] = rgb[0];
	out[1] = rgb[1];
	out[2] = rgb[2];
	out[3] = 1.0f;
}

/* The hull colour at `hull` from 0 to 1. The colours are blended in sRGB
   values, then made linear again. */
static void box_hull_color(const XwaModernTargetReadoutOptions* options, float hull, float out[4]) {
	const float* from = hull < 0.5f ? options->hull_color_low : options->hull_color_half;
	const float* to = hull < 0.5f ? options->hull_color_half : options->hull_color_full;
	const float t = hull < 0.5f ? hull * 2.0f : (hull - 0.5f) * 2.0f;
	float rgb[3];

	for (int i = 0; i < 3; i++) {
		const float a = XwaRemaster_LinearToSrgb(from[i]);
		const float b = XwaRemaster_LinearToSrgb(to[i]);
		rgb[i] = XwaRemaster_SrgbToLinear(a + (b - a) * t);
	}
	box_opaque(rgb, out);
}

static void box_add_readout_fill(XwaHudPreparedReadout* readout, float x, float y, float w, float h,
								 float frame_px, const float rgba[4]) {
	if (readout->fill_count >= XWA_HUD_MAX_READOUT_FILLS || w <= 0.0f || h <= 0.0f)
		return;
	XwaHudReadoutFill* fill = &readout->fills[readout->fill_count++];
	fill->x = x;
	fill->y = y;
	fill->w = w;
	fill->h = h;
	fill->frame_px = frame_px;
	memcpy(fill->rgba, rgba, sizeof fill->rgba);
}

/* Adds one bar of `length` by `thickness` at (x, y), filled to `value` from
   0 to 1, from the left, or upward when `vertical`. */
static void box_add_readout_bar(XwaHudPreparedReadout* readout, float x, float y, float length,
								float thickness, int vertical, float value, float frame_px,
								const float rgba[4]) {
	const float w = vertical ? thickness : length;
	const float h = vertical ? length : thickness;
	const float filled = value * length;

	if (vertical)
		box_add_readout_fill(readout, x, y + length - filled, w, filled, 0.0f, rgba);
	else
		box_add_readout_fill(readout, x, y, filled, h, 0.0f, rgba);
	/* The outline sits on the bar's edge: half inside and half outside. */
	if (frame_px > 0.0f)
		box_add_readout_fill(readout, x - frame_px * 0.5f, y - frame_px * 0.5f, w + frame_px, h + frame_px,
							 frame_px, rgba);
}

static float box_unit(float value) { return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); }

static void box_build_readout_bars(const XwaSnapshot* snapshot, const XwaModernTargetReadoutOptions* options,
								   XwaHudPreparedReadout* readout, const XwaHudPreparedBox* box) {
	const XwaHudTarget* target = &snapshot->hud.target;
	const XwaFlightObject* object = XwaSnapshot_FindFlightObject(snapshot, target->slot, target->signature);
	if (!options->bars || !target->damage_shown || !object || !object->has_craft ||
		object->craft_state == XWA_SNAP_CRAFT_STATE_DYING ||
		object->craft_state == XWA_SNAP_CRAFT_STATE_DESTROYED)
		return;
	const float pt = readout->hud_point_px;
	float shield = target->shield_pct / 100.0f;
	if (shield > 2.0f)
		shield = 2.0f;
	const float hull = box_unit(target->hull_pct / 100.0f);
	const float system = box_unit(target->system_pct / 100.0f);
	float length = 0.7f * box->w_px / pt;
	if (length < options->bar_length_min)
		length = options->bar_length_min;
	if (length > options->bar_length_max)
		length = options->bar_length_max;
	length *= pt;
	const float thickness = options->bar_thickness * pt;
	const float gap = options->bar_gap * pt;
	const float frame = options->bar_frame * pt;
	/* The bars are measured from the outside of the box's lines. The original
	   game's lines are inside the box on the left and at the top, and outside
	   it on the right and at the bottom. Wide lines are centred on its edges. */
	const float half = box_prepared.line_px * 0.5f;
	const float left = box->x_px - half, top = box->y_px - half;
	const float right = box->x_px + box->w_px + (half > 0.0f ? half : box->edge_px);
	const float bottom = box->y_px + box->h_px + (half > 0.0f ? half : box->edge_px);
	const float center_x = (left + right) * 0.5f, center_y = (top + bottom) * 0.5f;
	float shield_rgba[4], overshield_rgba[4], hull_rgba[4], system_rgba[4];
	box_opaque(options->shield_color, shield_rgba);
	box_opaque(options->overshield_color, overshield_rgba);
	box_hull_color(options, hull, hull_rgba);
	box_opaque(options->system_color, system_rgba);

	float shield_x, shield_y, hull_x, hull_y, system_y;
	const int vertical = options->layout == XWA_TARGET_READOUT_LAYOUT_VERTICAL;
	if (vertical) {
		shield_x = left - gap - thickness;
		hull_x = right + gap;
		shield_y = hull_y = center_y - length * 0.5f;
		system_y = bottom + gap;
	} else {
		shield_x = hull_x = center_x - length * 0.5f;
		shield_y = bottom + gap;
		hull_y = shield_y + thickness + gap;
		system_y = hull_y + thickness + gap;
	}
	box_add_readout_bar(readout, shield_x, shield_y, length, thickness, vertical, box_unit(shield), frame,
						shield_rgba);
	if (shield > 1.0f)
		box_add_readout_bar(readout, shield_x, shield_y, length, thickness, vertical, shield - 1.0f, 0.0f,
							overshield_rgba);
	box_add_readout_bar(readout, hull_x, hull_y, length, thickness, vertical, hull, frame, hull_rgba);
	if (system < 1.0f)
		box_add_readout_bar(readout, center_x - length * 0.5f, system_y, length, thickness, 0, system, frame,
							system_rgba);
}

/* Finds the current target's box and its component's box, and works out
   the bars. The readout follows the CMD: it shows only in the frames where
   the CMD drew the target's text. */
/* The selected component's box of a capital ship, platform or freighter.
   The target readout changes only this component box: its size limits, its
   pulse and its line. */
static int box_is_readout_component(const XwaHudTargetBox* source) {
	return source->component != 0xffffu && source->selected && source->color_index == 0x3fu;
}

static int box_center_in_view(const XwaHudPreparedBox* box) {
	const AeronRectI viewport = box_prepared.camera_viewport;
	const float center_x = box->x_px + box->w_px * 0.5f;
	const float center_y = box->y_px + box->h_px * 0.5f;
	return center_x > viewport.x && center_x < viewport.x + viewport.width && center_y > viewport.y &&
		   center_y < viewport.y + viewport.height;
}

/* Finds the current target's box and its component's box, and works out
   the bars. The readout follows the CMD: it shows only in the frames where
   the CMD drew the target's text. The target's text and bars need the
   target's box in view, and the component line needs the component's box
   in view. */
static void box_build_readout(const XwaSnapshot* snapshot, const XwaModernTargetReadoutOptions* options,
							  int component_box) {
	XwaHudPreparedReadout* readout = &box_prepared.readout;
	const XwaHudTarget* target = &snapshot->hud.target;

	readout->box = -1;
	readout->component_box = -1;
	if (!options->enabled || !target->valid || !target->cmd_shown || !snapshot->hud.hud_enabled ||
		(snapshot->hud.mode_flags & XWA_HUD_MODE_HANGAR_READY))
		return;
	for (uint16_t i = 0; i < box_prepared.box_count; i++) {
		const XwaHudPreparedBox* box = &box_prepared.boxes[i];
		if (box->slot == target->slot && box->signature == target->signature && box->component == 0xffffu)
			readout->box = (int16_t)i;
	}
	if (component_box >= 0) {
		const XwaHudPreparedBox* box = &box_prepared.boxes[component_box];
		if (box->slot == target->slot && box->signature == target->signature) {
			readout->component_box = (int16_t)component_box;
			readout->component_valid = (uint8_t)box_center_in_view(box);
		}
	}
	if (readout->box < 0 || !box_center_in_view(&box_prepared.boxes[readout->box]))
		return;
	readout->valid = 1;
	box_build_readout_bars(snapshot, options, readout, &box_prepared.boxes[readout->box]);
}

int XwaRemasterHudBoxes_Init(void) {
	if (box_lists[0] && box_lists[1])
		return 1;
	/* The lists grow when a frame has more boxes. They start with room for
	 * 512. */
	box_lists[0] = AeronDrawList_Create(512 * 8);
	box_lists[1] = AeronDrawList_Create(512 * 8);
	if (!box_lists[0] || !box_lists[1]) {
		AeronDrawList_Destroy(box_lists[0]);
		AeronDrawList_Destroy(box_lists[1]);
		box_lists[0] = box_lists[1] = NULL;
		return 0;
	}
	return 1;
}

void XwaRemasterHudBoxes_Shutdown(void) {
	AeronDrawList_Destroy(box_lists[0]);
	AeronDrawList_Destroy(box_lists[1]);
	box_lists[0] = box_lists[1] = NULL;
	memset(&box_prepared, 0, sizeof box_prepared);
}

void XwaRemasterHudBoxes_Build(const XwaSnapshot* snapshot, XwaHudProfileIndex profile,
							   uint32_t bundle_generation, const XwaRemasterFlightView* flight_view,
							   int target_w, int target_h) {
	memset(&box_prepared, 0, sizeof box_prepared);
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	const XwaHudLayoutProfile* layout_profile = XwaRemasterHudLayout_Profile(layout, profile);
	if (!snapshot || !snapshot->hud.valid || !snapshot->flight_camera_valid || !flight_view || !layout ||
		!layout_profile || target_w <= 0 || target_h <= 0 || snapshot->flight_camera.map_mode)
		return;
	box_prepared.layout_generation = layout->generation;
	box_prepared.bundle_generation = bundle_generation;
	box_prepared.profile = (uint8_t)profile;
	box_prepared.target_w = (uint16_t)(target_w > 0xffff ? 0xffff : target_w);
	box_prepared.target_h = (uint16_t)(target_h > 0xffff ? 0xffff : target_h);
	box_prepared.camera_viewport = flight_view->viewport;
	const AeronRectI viewport = box_prepared.camera_viewport;
	if (viewport.width <= 0 || viewport.height <= 0)
		return;
	int out_x, out_y;
	float output_scale;
	box_output_transform(layout_profile, target_w, target_h, &out_x, &out_y, &output_scale);
	if (output_scale <= 0.0f)
		return;
	const float screen_w = snapshot->flight_camera.screen_w ? snapshot->flight_camera.screen_w : 640.0f;
	const float classic_pixel_px = flight_view->classic_pixel_scale;
	const float proj_scale =
		snapshot->flight_camera.proj_scale > 0.0f ? snapshot->flight_camera.proj_scale : 512.0f;
	const int max_box_size = (int)screen_w * 3 / 4;
	XwaModernTargetReadoutOptions readout_options;
	XwaModernTargetReadoutOptions_Get(&readout_options);
	box_prepared.readout.point_px = (float)viewport.height / 1080.0f;
	box_prepared.readout.hud_point_px = box_prepared.readout.point_px * layout_profile->scale.user;
	box_prepared.line_px = XwaCraftHud_TargetBoxLineWidth() * box_prepared.readout.point_px;
	int readout_component_box = -1;
	for (uint16_t i = 0; i < snapshot->hud.target_box_count; i++) {
		const XwaHudTargetBox* source = &snapshot->hud.target_boxes[i];
		const XwaFlightObject* object = box_find_object(snapshot, source);
		if (!object)
			continue;
		float local_point[3];
		if (source->component != 0xffffu) {
			float center_opt[3];
			if (!XwaSnapshotExport_ComponentTargetGeometry(object->object_type, source->component, center_opt,
														   NULL))
				continue;
			float model[16];
			if (!XwaRemasterFlight_ObjectModelMatrixAtOrigin(object, flight_view->origin_world, model))
				continue;
			XwaRemasterFlight_TransformOptPoint(model, center_opt, local_point);
		}
		float center_x, center_y, depth;
		if (source->component != 0xffffu) {
			if (!XwaRemasterFlight_ProjectLocal(flight_view, local_point, &center_x, &center_y, &depth))
				continue;
		} else if (!XwaRemasterFlight_ProjectWorldI32(flight_view, object->world_pos, &center_x, &center_y,
													  &depth)) {
			continue;
		}
		int extent = source->extent;
		int min_size;
		if (source->component != 0xffffu) {
			if (!source->selected && depth > 32768.0f)
				extent = 0;
			min_size = source->selected ? 2 : 1;
		} else {
			min_size = 8;
		}
		/* The game's box size, in game pixels. The game rounds it down to a
		   whole pixel. It is kept exact here, so that the box grows smoothly. */
		float box_h = depth > 0.0f ? proj_scale * (float)extent / depth : 0.0f;
		if (box_h < (float)min_size)
			box_h = (float)min_size;
		if (box_h > (float)max_box_size)
			box_h = (float)max_box_size;
		float box_w = box_h;
		if (source->component == 0xffffu) {
			box_w += 8.0f;
			box_h += 8.0f;
		}
		if (box_prepared.box_count >= XWA_HUD_MAX_PREPARED_BOXES) {
			box_prepared.dropped_boxes++;
			continue;
		}
		XwaHudPreparedBox* box = &box_prepared.boxes[box_prepared.box_count++];
		memset(box, 0, sizeof *box);
		box->slot = source->slot;
		box->signature = source->signature;
		box->component = source->component;
		box->color_index = source->color_index;
		box->selected = source->selected;
		box->layer = source->layer;
		/* The box is centred on its projected centre. The game's corner is
		   centre - size / 2 rounded down, so the box shifts by half a pixel
		   when the size changes. */
		box->w_px = box_w * classic_pixel_px;
		box->h_px = box_h * classic_pixel_px;
		box->x_px = center_x - box->w_px * 0.5f;
		box->y_px = center_y - box->h_px * 0.5f;
		box->edge_px = classic_pixel_px;
		/* The arms are 1/8 of the box, and at least 3 game pixels. */
		float arm_x = box_w / 8.0f, arm_y = box_h / 8.0f;
		if (arm_x < 3.0f)
			arm_x = 3.0f;
		if (arm_x > box_w)
			arm_x = box_w;
		if (arm_y < 3.0f)
			arm_y = 3.0f;
		if (arm_y > box_h)
			arm_y = box_h;
		box->arm_x_px = arm_x * classic_pixel_px;
		box->arm_y_px = arm_y * classic_pixel_px;
		const int readout_component = readout_options.enabled && box_is_readout_component(source);
		/* With the target readout, the component's box is kept between
		   component_box_min and component_box_max points. */
		if (readout_component) {
			const float point_px = box_prepared.readout.point_px;
			float size = box->w_px;
			if (readout_options.component_box_min > 0.0f &&
				size < readout_options.component_box_min * point_px)
				size = readout_options.component_box_min * point_px;
			if (readout_options.component_box_max > 0.0f &&
				size > readout_options.component_box_max * point_px)
				size = readout_options.component_box_max * point_px;
			if (size != box->w_px) {
				const float classic_size = size / classic_pixel_px;
				float arm = classic_size / 8.0f;
				if (arm < 3.0f)
					arm = 3.0f;
				if (arm > classic_size)
					arm = classic_size;
				box->w_px = box->h_px = size;
				box->x_px = center_x - size * 0.5f;
				box->y_px = center_y - size * 0.5f;
				box->arm_x_px = box->arm_y_px = arm * classic_pixel_px;
			}
		}
		/* With target_boxes.line_width, the arms are 1/8 of the box, and a
		   box of 4 game pixels or less is filled. */
		if (box_prepared.line_px > 0.0f) {
			box->arm_x_px = box->w_px * 0.125f;
			box->arm_y_px = box->h_px * 0.125f;
			box->fill =
				(uint8_t)(box->w_px <= 4.0f * classic_pixel_px || box->h_px <= 4.0f * classic_pixel_px);
		}
		/* The text is placed from the box before the pulse, so it doesn't
		   move. */
		box->x_ref = (box->x_px - out_x) / output_scale;
		box->y_ref = (box->y_px - out_y) / output_scale;
		box->w_ref = box->w_px / output_scale;
		box->h_ref = box->h_px / output_scale;
		if (readout_component) {
			readout_component_box = box_prepared.box_count - 1;
			if (readout_options.component_pulse) {
				/* v goes from 5 down to 0 in a second, then starts again. The
				   box shrinks to 70% while its arms go from 0.275 to 0.125 of
				   its size. */
				const float v = 5.0f - 5.0f * (float)(XwaTime_GetElapsedUs() % 1000000u) / 1000000.0f;
				const float size = box->w_px * (0.7f + 0.3f * v / 5.0f);
				const float arm = (0.125f + 0.03f * v) * size;
				box->w_px = box->h_px = size;
				box->x_px = center_x - size * 0.5f;
				box->y_px = center_y - size * 0.5f;
				box->arm_x_px = box->arm_y_px = arm;
			}
		}
		const uint32_t modes = snapshot->hud.mode_flags;
		box->readout =
			(uint8_t)(source->component == 0xffffu && source->color_index == 59u &&
					  ((snapshot->hud.mfd_enabled[0] != 0) ||
					   ((modes & XWA_HUD_MODE_EXTERNAL_CAMERA) && !(modes & XWA_HUD_MODE_EXTERIOR_HUD)) ||
					   ((modes & XWA_HUD_MODE_FILM_PLAYBACK) && (modes & XWA_HUD_MODE_FILM_OVERLAY))) &&
					  !(modes & XWA_HUD_MODE_HANGAR_READY) && center_x > viewport.x &&
					  center_x < viewport.x + viewport.width && center_y > viewport.y &&
					  center_y < viewport.y + viewport.height);
	}
	/* Mouse flight virtual-stick marker, anchored to the reticle center. The
	 * fixed-HUD build runs before this one, so the reticle reference position
	 * is valid whenever the reticle is drawn this frame. */
	{
		float reticle_x_ref, reticle_y_ref;
		XwaRemasterHudVisibility visibility;

		XwaRemasterHud_BuildVisibility(&snapshot->hud, &visibility);
		if (snapshot->hud.reticle.stick_marker && visibility.reticle && snapshot->hud.reticle.visible &&
			XwaRemasterHudFixed_ReticleCenter(&reticle_x_ref, &reticle_y_ref)) {
			const float range_px = (float)viewport.height / 6.0f;
			const float center_x = reticle_x_ref * output_scale + (float)out_x;
			const float center_y = reticle_y_ref * output_scale + (float)out_y;

			box_prepared.stick_marker_valid = 1;
			box_prepared.stick_marker_x_px =
				center_x + (float)snapshot->hud.reticle.stick_marker_x * range_px / 127.0f;
			/* Y flipped: the marker points where the nose is commanded toward
			 * (positive stick = pull back = pitch up = up on screen). */
			box_prepared.stick_marker_y_px =
				center_y - (float)snapshot->hud.reticle.stick_marker_y * range_px / 127.0f;
			box_prepared.stick_marker_size_px = 4.0f * classic_pixel_px;
			box_prepared.stick_marker_edge_px = classic_pixel_px;
			/* Configured HUD color (the reticle tint); component-marker
			 * palette entry if the capture carried no usable color. */
			box_prepared.stick_marker_argb = (snapshot->hud.hud_colors[0] >> 24) != 0
												 ? snapshot->hud.hud_colors[0]
												 : XwaSnapshotExport_FlightPaletteColor(63);
		}
	}
	box_build_readout(snapshot, &readout_options, readout_component_box);
	box_prepared.valid = 1;
}

/* Draws a box with lines of `line_px` centred on its edges, and the same four
   corners. */
static void box_add_wide_box(AeronDrawList2D* list, const XwaHudPreparedBox* box, float line_px,
							 const float rgba[4], const AeronRectI* scissor) {
	const float x = box->x_px, y = box->y_px, w = box->w_px, h = box->h_px;
	const float half = line_px * 0.5f;

	if (box->fill) {
		AeronDrawList_AddFill(list, x, y, w, h, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
		return;
	}
	/* Each corner is a horizontal arm that covers the corner, and a vertical
	   arm that starts under it. */
	const float arm_x = box->arm_x_px + half;
	const float arm_y = box->arm_y_px > half ? box->arm_y_px - half : 0.0f;
	const float left = x - half, right = x + w - box->arm_x_px;
	const float top = y - half, bottom = y + h - half;
	AeronDrawList_AddFill(list, left, top, arm_x, line_px, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, right, top, arm_x, line_px, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, left, bottom, arm_x, line_px, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, right, bottom, arm_x, line_px, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, left, y + half, line_px, arm_y, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, x + w - half, y + half, line_px, arm_y, rgba, AERON_BLIT2D_BLEND_PMA,
						  scissor);
	AeronDrawList_AddFill(list, left, y + h - box->arm_y_px, line_px, arm_y, rgba, AERON_BLIT2D_BLEND_PMA,
						  scissor);
	AeronDrawList_AddFill(list, x + w - half, y + h - box->arm_y_px, line_px, arm_y, rgba,
						  AERON_BLIT2D_BLEND_PMA, scissor);
}

static void box_prepare_layer(AeronCommandBuffer* cmd, int target_w, int target_h,
							  XwaHudTargetBoxLayer layer, AeronDrawList2D* box_list) {
	if (!box_list || !cmd || target_w <= 0 || target_h <= 0 ||
		(layer != XWA_HUD_TARGET_BOX_BEFORE_FIXED && layer != XWA_HUD_TARGET_BOX_AFTER_FIXED))
		return;
	AeronDrawList_Begin(box_list, NULL, target_w, target_h, AERON_DRAWLIST2D_LOAD, NULL);
	if (!box_prepared.valid || box_prepared.target_w != target_w ||
		box_prepared.target_h != target_h) {
		(void)AeronDrawList_Prepare(box_list, cmd);
		return;
	}
	const XwaHudLayout* layout = XwaRemasterHud_Layout();
	if (!layout || box_prepared.layout_generation != layout->generation) {
		(void)AeronDrawList_Prepare(box_list, cmd);
		return;
	}
	const int has_stick_marker = layer == XWA_HUD_TARGET_BOX_AFTER_FIXED && box_prepared.stick_marker_valid;
	int has_layer = has_stick_marker;
	for (uint16_t i = 0; !has_layer && i < box_prepared.box_count; i++) {
		if (box_prepared.boxes[i].layer == (uint8_t)layer) {
			has_layer = 1;
		}
	}
	if (!has_layer) {
		(void)AeronDrawList_Prepare(box_list, cmd);
		return;
	}
	const AeronRectI scissor = box_prepared.camera_viewport;
	if (has_stick_marker) {
		const uint32_t argb = box_prepared.stick_marker_argb;
		const float a = ((argb >> 24) & 255) / 255.0f;
		const float rgba[4] = {
			XwaRemaster_SrgbToLinear(((argb >> 16) & 255) / 255.0f) * a,
			XwaRemaster_SrgbToLinear(((argb >> 8) & 255) / 255.0f) * a,
			XwaRemaster_SrgbToLinear((argb & 255) / 255.0f) * a,
			a,
		};
		const float size = box_prepared.stick_marker_size_px;

		AeronDrawList_AddFrame(box_list, box_prepared.stick_marker_x_px - size * 0.5f,
							   box_prepared.stick_marker_y_px - size * 0.5f, size, size,
							   box_prepared.stick_marker_edge_px, rgba, AERON_BLIT2D_BLEND_PMA, &scissor);
	}
	for (uint16_t i = 0; i < box_prepared.box_count; i++) {
		const XwaHudPreparedBox* box = &box_prepared.boxes[i];
		if (box->layer != (uint8_t)layer)
			continue;
		const uint32_t argb = XwaSnapshotExport_FlightPaletteColor(box->color_index);
		const float a = ((argb >> 24) & 255) / 255.0f;
		const float rgba[4] = {
			XwaRemaster_SrgbToLinear(((argb >> 16) & 255) / 255.0f) * a,
			XwaRemaster_SrgbToLinear(((argb >> 8) & 255) / 255.0f) * a,
			XwaRemaster_SrgbToLinear((argb & 255) / 255.0f) * a,
			a,
		};
		if (box_prepared.line_px > 0.0f) {
			box_add_wide_box(box_list, box, box_prepared.line_px, rgba, &scissor);
			continue;
		}
		const float x = box->x_px, y = box->y_px, w = box->w_px, h = box->h_px;
		const float ex = box->edge_px, ax = box->arm_x_px, ay = box->arm_y_px;
		AeronDrawList_AddFill(box_list, x, y, ax, ex, rgba, AERON_BLIT2D_BLEND_PMA, &scissor);
		AeronDrawList_AddFill(box_list, x + w - ax, y, ax, ex, rgba, AERON_BLIT2D_BLEND_PMA, &scissor);
		AeronDrawList_AddFill(box_list, x, y + h, ax, ex, rgba, AERON_BLIT2D_BLEND_PMA, &scissor);
		/* Hud_DrawBoxOverlayHW gives only the lower-right horizontal
		 * arm an inclusive right endpoint (right + 1). This is observable:
		 * it covers the right vertical arm's pixel column at the join. */
		AeronDrawList_AddFill(box_list, x + w - ax, y + h, ax + ex, ex, rgba, AERON_BLIT2D_BLEND_PMA,
							  &scissor);
		/* The classic hardware path does not shorten vertical arms by the
		 * horizontal thickness: upper arms overlap and lower arms meet the
		 * horizontal edge. Preserve that coverage at fractional HD density. */
		AeronDrawList_AddFill(box_list, x, y, ex, ay, rgba, AERON_BLIT2D_BLEND_PMA, &scissor);
		AeronDrawList_AddFill(box_list, x + w, y, ex, ay, rgba, AERON_BLIT2D_BLEND_PMA, &scissor);
		AeronDrawList_AddFill(box_list, x, y + h - ay, ex, ay, rgba, AERON_BLIT2D_BLEND_PMA, &scissor);
		AeronDrawList_AddFill(box_list, x + w, y + h - ay, ex, ay, rgba, AERON_BLIT2D_BLEND_PMA, &scissor);
	}
	(void)AeronDrawList_Prepare(box_list, cmd);
}

void XwaRemasterHudBoxes_PrepareDrawLists(AeronCommandBuffer* cmd, int target_w, int target_h) {
	box_prepare_layer(cmd, target_w, target_h, XWA_HUD_TARGET_BOX_BEFORE_FIXED, box_lists[0]);
	box_prepare_layer(cmd, target_w, target_h, XWA_HUD_TARGET_BOX_AFTER_FIXED, box_lists[1]);
}

void XwaRemasterHudBoxes_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass,
								AeronRenderTarget* color_target, int target_w, int target_h,
								XwaHudTargetBoxLayer layer) {
	if (!cmd || !pass || !color_target || target_w <= 0 || target_h <= 0 ||
		(layer != XWA_HUD_TARGET_BOX_BEFORE_FIXED && layer != XWA_HUD_TARGET_BOX_AFTER_FIXED))
		return;
	const int index = layer == XWA_HUD_TARGET_BOX_AFTER_FIXED ? 1 : 0;
	AeronDrawList_RenderIntoPass(box_lists[index], cmd, pass, color_target);
}

const XwaHudPreparedBoxState* XwaRemasterHudBoxes_Prepared(void) { return &box_prepared; }

const XwaHudPreparedBox* XwaRemasterHudBoxes_SelectedReadout(void) {
	for (uint16_t i = 0; i < box_prepared.box_count; i++)
		if (box_prepared.boxes[i].readout)
			return &box_prepared.boxes[i];
	return NULL;
}
