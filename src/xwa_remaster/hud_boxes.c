#include "xwa_remaster/hud_boxes.h"

#include "aeron/scene/draw_list2d.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/hud_fixed.h"
#include "xwa_remaster/hud_layout.h"

#include <math.h>
#include <string.h>

static AeronDrawList2D* box_lists[2];
static XwaHudPreparedBoxState box_prepared;

static const XwaFlightObject* box_find_object(const XwaSnapshot* snapshot, const XwaHudTargetBox* box) {
	for (uint32_t i = 0; i < snapshot->flight_object_count; i++) {
		const XwaFlightObject* object = &snapshot->flight_objects[i];
		if (object->slot == box->slot && object->signature == box->signature)
			return object;
	}
	return NULL;
}

static void box_transform_point(const float m[16], const float p[3], float out[3]) {
	out[0] = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
	out[1] = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7];
	out[2] = m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11];
}

static void box_output_transform(const XwaHudLayoutProfile* profile, int target_w, int target_h, int* out_x,
								 int* out_y, float* out_scale) {
	XwaRemasterHudLayout_OutputTransform(profile, target_w, target_h, out_x, out_y, out_scale);
}

int XwaRemasterHudBoxes_Init(void) {
	if (box_lists[0] && box_lists[1])
		return 1;
	box_lists[0] = AeronDrawList_Create(XWA_HUD_MAX_PREPARED_BOXES * 8);
	box_lists[1] = AeronDrawList_Create(XWA_HUD_MAX_PREPARED_BOXES * 8);
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
	const float classic_pixel_ref = classic_pixel_px / output_scale;
	const float proj_scale =
		snapshot->flight_camera.proj_scale > 0.0f ? snapshot->flight_camera.proj_scale : 512.0f;
	const int max_box_size = (int)screen_w * 3 / 4;
	for (uint16_t i = 0; i < snapshot->hud.target_box_count; i++) {
		const XwaHudTargetBox* source = &snapshot->hud.target_boxes[i];
		const XwaFlightObject* object = box_find_object(snapshot, source);
		if (!object)
			continue;
		float local_point[3];
		if (source->component != 0xffffu) {
			float local[3];
			if (!XwaSnapshotExport_ComponentTargetGeometry(object->object_type, source->component, local,
														   NULL))
				continue;
			float model[16];
			if (!XwaRemasterFlight_ObjectModelMatrixAtOrigin(object, flight_view->origin_world, model))
				continue;
			box_transform_point(model, local, local_point);
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
		int box_h = depth > 0.0f ? (int)(proj_scale * (float)extent / depth) : 0;
		if (box_h < min_size)
			box_h = min_size;
		if (box_h > max_box_size)
			box_h = max_box_size;
		int box_w = box_h;
		if (source->component == 0xffffu) {
			box_w += 8;
			box_h += 8;
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
		box->w_px = box_w * classic_pixel_px;
		box->h_px = box_h * classic_pixel_px;
		box->x_px = center_x - (box_w / 2) * classic_pixel_px;
		box->y_px = center_y - (box_h / 2) * classic_pixel_px;
		box->edge_px = classic_pixel_px;
		int arm_x = box_w >> 3, arm_y = box_h >> 3;
		if (arm_x < 3)
			arm_x = 3;
		if (arm_x > box_w)
			arm_x = box_w;
		if (arm_y < 3)
			arm_y = 3;
		if (arm_y > box_h)
			arm_y = box_h;
		box->arm_x_px = arm_x * classic_pixel_px;
		box->arm_y_px = arm_y * classic_pixel_px;
		box->x_ref = (box->x_px - out_x) / output_scale;
		box->y_ref = (box->y_px - out_y) / output_scale;
		box->w_ref = box->w_px / output_scale;
		box->h_ref = box->h_px / output_scale;
		box->classic_pixel_ref = classic_pixel_ref;
		const uint32_t modes = snapshot->hud.mode_flags;
		box->readout =
			(uint8_t)(source->component == 0xffffu && source->color_index == 59u &&
					  ((snapshot->hud.mfd_enabled[0] != 0) || (modes & XWA_HUD_MODE_EXTERNAL_CAMERA) ||
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
	box_prepared.valid = 1;
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
