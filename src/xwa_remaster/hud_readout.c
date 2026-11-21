#include "xwa_remaster/hud_readout.h"

#include "aeron/asset/opt_model.h"
#include "aeron/scene/draw_list2d.h"
#include "xwa_remaster/hud_boxes.h"
#include "xwa_remaster/hud_text.h"
#include "xwa_runtime/config/modern_target_readout_options.h"

static AeronDrawList2D* readout_list;
static int readout_has_records;
/* Whether the list was prepared on planes. */
static int readout_on_planes;

int XwaRemasterHudReadout_Init(void) {
	if (readout_list)
		return 1;
	readout_list = AeronDrawList_Create(XWA_HUD_MAX_READOUT_GLYPHS + 4 * XWA_HUD_MAX_READOUT_FILLS);
	return readout_list != NULL;
}

void XwaRemasterHudReadout_Shutdown(void) {
	AeronDrawList_Destroy(readout_list);
	readout_list = NULL;
	readout_has_records = 0;
}

void XwaRemasterHudReadout_PrepareDrawList(AeronCommandBuffer* cmd, int target_w, int target_h) {
	const XwaHudPreparedBoxState* boxes = XwaRemasterHudBoxes_Prepared();

	readout_has_records = 0;
	readout_on_planes = 0;
	if (!readout_list || !cmd || target_w <= 0 || target_h <= 0)
		return;
	AeronDrawList_Begin(readout_list, NULL, target_w, target_h, AERON_DRAWLIST2D_LOAD, NULL);
	AeronDrawList_SetPlaneHardFills(readout_list, XwaRemasterHud_MarkersOnScreens());
	if (boxes && boxes->valid && (boxes->readout.valid || boxes->readout.component_valid) &&
		boxes->target_w == target_w && boxes->target_h == target_h) {
		/* In a headset the bars are on the plane of the target's box, and
		   drawings on planes aren't clipped to a rectangle. */
		const AeronRectI viewport = boxes->camera_viewport;
		const AeronRectI* const scissor = boxes->on_planes ? NULL : &viewport;
		int plane_box = -1;
		const int bars =
			!boxes->on_planes || XwaRemasterHudBoxes_UsePlane(readout_list, boxes->readout.box, &plane_box);
		for (uint16_t i = 0; bars && i < boxes->readout.fill_count; i++) {
			const XwaHudReadoutFill* fill = &boxes->readout.fills[i];
			if (fill->frame_px > 0.0f)
				AeronDrawList_AddFrame(readout_list, fill->x, fill->y, fill->w, fill->h, fill->frame_px,
									   fill->rgba, AERON_BLIT2D_BLEND_PMA, scissor);
			else
				AeronDrawList_AddFill(readout_list, fill->x, fill->y, fill->w, fill->h, fill->rgba,
									  AERON_BLIT2D_BLEND_PMA, scissor);
		}
		XwaRemasterHudText_AddReadoutGlyphs(readout_list, scissor);
		readout_has_records = 1;
		readout_on_planes = boxes->on_planes;
	}
	(void)AeronDrawList_Prepare(readout_list, cmd);
}

void XwaRemasterHudReadout_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
								  int target_w, int target_h, AeronScene3D* scene) {
	/* A readout on planes is drawn by XwaRemasterHudReadout_RenderPlanes. */
	if (!readout_has_records || readout_on_planes || !cmd || !pass || !target || target_w <= 0 ||
		target_h <= 0)
		return;
	XwaModernTargetReadoutOptions options;
	XwaModernTargetReadoutOptions_Get(&options);
	AeronTexture* depth = scene ? Aeron_DepthTargetGetTexture(AeronScene_DepthRt(scene)) : NULL;
	int scene_w = 0, scene_h = 0;
	if (scene)
		AeronScene_RtDims(scene, &scene_w, &scene_h);
	if (options.hidden_within_m > 0.0f && depth && scene_w > 0 && scene_h > 0) {
		const AeronDrawList2DDepthMask mask = {
			.depth = depth,
			.size_px = { (float)scene_w, (float)scene_h },
			.hide_above =
				AeronScene_DepthAtDistance(scene, options.hidden_within_m / AERON_OPT_METERS_PER_UNIT),
		};
		AeronDrawList_SetDepthMask(readout_list, &mask);
	} else {
		AeronDrawList_SetDepthMask(readout_list, NULL);
	}
	AeronDrawList_RenderIntoPass(readout_list, cmd, pass, target);
}

void XwaRemasterHudReadout_RenderPlanes(AeronCommandBuffer* cmd, AeronRenderPass* pass,
										AeronRenderTarget* target, const float view[12]) {
	if (readout_has_records && readout_on_planes && cmd && pass && target && view)
		AeronDrawList_RenderPlanes(readout_list, cmd, pass, target, view, 1.0f);
}
