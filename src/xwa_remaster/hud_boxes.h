#ifndef XWA_REMASTER_HUD_BOXES_H
#define XWA_REMASTER_HUD_BOXES_H

#include "aeron/scene/scene3d.h"
#include "xwa_remaster/hud.h"

#define XWA_HUD_MAX_PREPARED_BOXES XWA_SNAP_MAX_HUD_TARGET_BOXES

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaHudPreparedBox {
	uint16_t slot, signature, component;
	uint8_t color_index, selected, readout, layer;
	float x_px, y_px, w_px, h_px;
	float x_ref, y_ref, w_ref, h_ref;
	float edge_px, arm_x_px, arm_y_px;
	/* With line_px above 0, the box is drawn filled. */
	uint8_t fill;
} XwaHudPreparedBox;

enum { XWA_HUD_MAX_READOUT_FILLS = 12 };

/* A fill of the target readout's bars, in target pixels. A `frame_px` above
   0 draws an outline of that width inside the rectangle. `rgba` is
   premultiplied and linear. */
typedef struct XwaHudReadoutFill {
	float x, y, w, h;
	float frame_px;
	float rgba[4];
} XwaHudReadoutFill;

/* The target readout (modern_target_readout_options.h). `box` and
   `component_box` index XwaHudPreparedBoxState.boxes, or are -1. */
typedef struct XwaHudPreparedReadout {
	/* The target's text and bars show. */
	uint8_t valid;
	/* The component line shows. */
	uint8_t component_valid;
	int16_t box;
	int16_t component_box;
	/* Target pixels for each point: 1/1080 of the flight view's height. */
	float point_px;
	/* The size of a point of the readout's text and bars, in target pixels:
	   point_px times the HUD scale. */
	float hud_point_px;
	uint16_t fill_count;
	XwaHudReadoutFill fills[XWA_HUD_MAX_READOUT_FILLS];
} XwaHudPreparedReadout;

typedef struct XwaHudPreparedBoxState {
	uint32_t layout_generation;
	uint32_t bundle_generation;
	uint16_t target_w, target_h;
	uint16_t box_count, dropped_boxes;
	uint8_t profile, valid;
	uint8_t stick_marker_valid;
	uint8_t reserved;
	/* Mouse flight virtual-stick marker: center and box size in target px. */
	float stick_marker_x_px, stick_marker_y_px;
	float stick_marker_size_px, stick_marker_edge_px;
	uint32_t stick_marker_argb;
	AeronRectI camera_viewport;
	/* The width of the box lines, centred on the box's edges. 0 draws the
	   original game's boxes. */
	float line_px;
	XwaHudPreparedReadout readout;
	XwaHudPreparedBox boxes[XWA_HUD_MAX_PREPARED_BOXES];
} XwaHudPreparedBoxState;

int XwaRemasterHudBoxes_Init(void);
void XwaRemasterHudBoxes_Shutdown(void);
void XwaRemasterHudBoxes_Build(const XwaSnapshot* snapshot, XwaHudProfileIndex profile,
								   uint32_t bundle_generation, const XwaRemasterFlightView* flight_view,
								   int target_w, int target_h);
void XwaRemasterHudBoxes_PrepareDrawLists(AeronCommandBuffer* cmd, int target_w, int target_h);
void XwaRemasterHudBoxes_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass,
								AeronRenderTarget* color_target, int target_w, int target_h,
								XwaHudTargetBoxLayer layer);
const XwaHudPreparedBoxState* XwaRemasterHudBoxes_Prepared(void);
const XwaHudPreparedBox* XwaRemasterHudBoxes_SelectedReadout(void);

#ifdef __cplusplus
}
#endif

#endif
