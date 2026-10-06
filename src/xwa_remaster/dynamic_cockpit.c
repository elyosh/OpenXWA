#include "xwa_remaster/dynamic_cockpit.h"

#include "aeron/scene/draw_list2d.h"
#include "xwa_remaster/hud.h"
#include "xwa_remaster/hud_fixed.h"
#include "xwa_remaster/hud_layout.h"

#include <string.h>

/* Modern port module; no original counterpart (dynamic_cockpit.h). */

/* What a source or region is measured against. */
typedef enum DcAnchor {
	DC_ANCHOR_NONE = 0,
	DC_ANCHOR_LEFT_SENSOR,  /* HUD.dat 12000/4500 */
	DC_ANCHOR_RIGHT_SENSOR, /* 4600, or 400 without the aft sensor */
	DC_ANCHOR_SHIELDS,      /* 4300 */
	DC_ANCHOR_BEAM,         /* 4400 */
	DC_ANCHOR_TARGET,       /* 1100, the targeting computer */
	DC_ANCHOR_LEFT_MSG,     /* 100, left message box */
	DC_ANCHOR_RIGHT_MSG,    /* 100, right message box */
	DC_ANCHOR_TOP_LEFT,     /* 2700 */
	DC_ANCHOR_TOP_RIGHT,    /* 2800 */
	DC_ANCHOR_IMAGE_COUNT,
	/* Message panes of the HUD layout (screen_def / erase_screen_def). */
	DC_ANCHOR_PANE_READY = DC_ANCHOR_IMAGE_COUNT,
	DC_ANCHOR_PANE_SYSTEM,
	DC_ANCHOR_PANE_FLIGHT_GROUP,
	DC_ANCHOR_PANES_READY_SYSTEM,
} DcAnchor;

/* The draw record of each image anchor: widget and HUD.dat frame (frame 100
 * per image ID 100; 0 = any frame of the widget). */
static const struct {
	uint16_t widget;
	uint16_t frame;
} dc_anchor_records[DC_ANCHOR_IMAGE_COUNT] = {
	[DC_ANCHOR_LEFT_SENSOR] = { XWA_HUD_WIDGET_FORE_RADAR_SCOPE, 45 },
	[DC_ANCHOR_RIGHT_SENSOR] = { XWA_HUD_WIDGET_AFT_RADAR_SCOPE, 0 },
	[DC_ANCHOR_SHIELDS] = { XWA_HUD_WIDGET_SHIELD_HULL, 43 },
	[DC_ANCHOR_BEAM] = { XWA_HUD_WIDGET_BEAM, 44 },
	[DC_ANCHOR_TARGET] = { XWA_HUD_WIDGET_CMD_FRAME, 11 },
	[DC_ANCHOR_LEFT_MSG] = { XWA_HUD_WIDGET_MFD_LEFT_FRAME, 1 },
	[DC_ANCHOR_RIGHT_MSG] = { XWA_HUD_WIDGET_MFD_RIGHT_FRAME, 1 },
	[DC_ANCHOR_TOP_LEFT] = { XWA_HUD_WIDGET_FORE_RADAR_FRAME, 27 },
	[DC_ANCHOR_TOP_RIGHT] = { XWA_HUD_WIDGET_AFT_RADAR_FRAME, 28 },
};

/* A rectangle in the pixels of a width x height image. */
typedef struct DcRect {
	uint8_t anchor;
	float width, height, x0, y0, x1, y1;
	const char* name;
} DcRect;

/* Dynamic_Cockpit_Internal_Areas.cfg source_def / screen_def lines in index
 * order, with the four rectangles LoadDCInternalCoordinates overrides; the
 * anchors follow Direct3DDevice::Execute. Sources 42-56 are placed from the
 * HUD text and laser charges, not from this table.
 *
 * Deviation: the right message box uses the left box's rectangles mirrored
 * (source 12, region 6). The game draws that box as the left one mirrored
 * (g_mfdFrameQuadTexCoords), and the HD HUD lays out its text the same way;
 * XWAU's own rectangles sit 28 of 256 pixels further right, which crops the
 * left of the text and leaves it outside the erased region. */
static const DcRect dc_sources[XWA_DC_SOURCE_COUNT] = {
	{ DC_ANCHOR_LEFT_SENSOR, 128, 128, 25.5f, 18.5f, 114.5f, 106.5f, "LEFT_SENSOR_SRC" },
	{ DC_ANCHOR_RIGHT_SENSOR, 128, 128, 13.5f, 19.5f, 102.5f, 107.5f, "RIGHT_SENSOR_SRC" },
	{ DC_ANCHOR_LEFT_SENSOR, 128, 128, 9, 19, 15, 62, "LASER_RECHARGE_SRC" },
	{ DC_ANCHOR_LEFT_SENSOR, 128, 128, 9, 64, 15, 107, "SHIELD_RECHARGE_SRC" },
	{ DC_ANCHOR_RIGHT_SENSOR, 128, 128, 113, 19, 119, 62, "ENGINE_POWER_SRC" },
	{ DC_ANCHOR_RIGHT_SENSOR, 128, 128, 113, 64, 119, 107, "BEAM_RECHARGE_SRC" },
	{ DC_ANCHOR_SHIELDS, 128, 128, 40.5f, 25.5f, 98.5f, 99.5f, "SHIELDS_SRC" },
	{ DC_ANCHOR_BEAM, 128, 128, 29.5f, 25.5f, 87.5f, 99.5f, "BEAM_LEVEL_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 12.5f, 5.5f, 245.5f, 128, "TARGETING_COMPUTER_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 60, -12, 127, 5, "QUAD_LASERS_LEFT_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 135, -12, 202, 5, "QUAD_LASERS_RIGHT_SRC" },
	{ DC_ANCHOR_LEFT_MSG, 256, 256, 28, 64, 218, 200, "LEFT_TEXT_BOX_SRC" },
	{ DC_ANCHOR_RIGHT_MSG, 256, 256, 38, 64, 228, 200, "RIGHT_TEXT_BOX_SRC" }, /* XWAU: 66..256 */
	{ DC_ANCHOR_TOP_LEFT, 256, 32, 67, 0, 147, 25, "SPEED_THROTTLE_SRC" },
	{ DC_ANCHOR_TOP_LEFT, 256, 32, 200, 0, 256, 25, "MISSILES_SRC" },
	{ DC_ANCHOR_TOP_RIGHT, 256, 32, 110, 0, 190, 25, "NAME_TIME_SRC" },
	{ DC_ANCHOR_TOP_RIGHT, 256, 32, 20, 0, 72, 25, "NUM_SHIPS_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 60, -12, 202, 5, "QUAD_LASERS_BOTH_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 60, -4, 127, 5, "DUAL_LASERS_L_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 135, -4, 202, 5, "DUAL_LASERS_R_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 60, -4, 202, 5, "DUAL_LASERS_BOTH_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 55, -12, 202, 5, "B_WING_LASERS_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 60, -22, 202, 5, "SIX_LASERS_BOTH_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 60, -22, 127, 5, "SIX_LASERS_L_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 135, -22, 202, 5, "SIX_LASERS_R_SRC" },
	{ DC_ANCHOR_SHIELDS, 128, 128, 34, 13, 63, 25, "SHIELDS_FRONT_SRC" },
	{ DC_ANCHOR_SHIELDS, 128, 128, 34, 99, 63, 115, "SHIELDS_BACK_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "KW_TEXT_CMD_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "KW_TEXT_TOP_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "KW_TEXT_RADIOSYS_SRC" },
	{ DC_ANCHOR_PANE_READY, 0, 0, 0, 0, 0, 0, "TEXT_RADIO_SRC" },
	{ DC_ANCHOR_PANE_SYSTEM, 0, 0, 0, 0, 0, 0, "TEXT_SYSTEM_SRC" },
	{ DC_ANCHOR_PANE_FLIGHT_GROUP, 0, 0, 0, 0, 0, 0, "TEXT_CMD_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 38, 15, 218, 28, "TARGETED_OBJ_NAME_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 14, 54, 53, 78, "TARGETED_OBJ_SHD_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 14, 82, 58, 106, "TARGETED_OBJ_HULL_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 14, 110, 100, 128, "TARGETED_OBJ_CARGO_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 203, 54, 242, 78, "TARGETED_OBJ_SYS_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 198, 82, 242, 106, "TARGETED_OBJ_DIST_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 136, 110, 242, 128, "TARGETED_OBJ_SUBCMP_SRC" },
	{ DC_ANCHOR_TARGET, 256, 128, 60, -30, 199, 1, "EIGHT_LASERS_BOTH_SRC" },
	{ DC_ANCHOR_SHIELDS, 128, 128, 104, 20, 106, 108, "THROTTLE_BAR_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "AUTOSIZE_MISSILES_L_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "AUTOSIZE_MISSILES_R_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "AUTOSIZE_SPEED_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "AUTOSIZE_CHAFF_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "AUTOSIZE_NAME_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "AUTOSIZE_TIME_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "AUTOSIZE_MISSILES_B_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "AUTOSIZE_MISSILES_MIS_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "DUAL_LASER_ENERGY_BAR_L_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "DUAL_LASER_ENERGY_BAR_R_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "QUAD_LASER_ENERGY_BAR_LU_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "QUAD_LASER_ENERGY_BAR_LD_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "QUAD_LASER_ENERGY_BAR_RU_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "QUAD_LASER_ENERGY_BAR_RD_SRC" },
	{ DC_ANCHOR_NONE, 0, 0, 0, 0, 0, 0, "ALL_LASER_ENERGY_BAR_SRC" },
};

/* erase_def / erase_screen_def lines in index order (g_HUDRegionNames). */
static const DcRect dc_regions[XWA_DC_REGION_COUNT] = {
	{ DC_ANCHOR_LEFT_SENSOR, 128, 128, 5, 13, 119, 109.5f, "LEFT_SENSOR_REGION" },
	{ DC_ANCHOR_RIGHT_SENSOR, 128, 128, 10, 13, 119, 109.5f, "RIGHT_SENSOR_REGION" },
	{ DC_ANCHOR_SHIELDS, 128, 128, 26, 14, 107, 116, "SHIELDS_REGION" },
	{ DC_ANCHOR_BEAM, 128, 128, 25, 14, 102, 116, "BEAM_REGION" },
	{ DC_ANCHOR_TARGET, 256, 128, 0, -35, 256, 128, "TARGET_AND_LASERS_REGION" },
	{ DC_ANCHOR_LEFT_MSG, 256, 256, 26, 50, 229, 202, "LEFT_TEXT_BOX_REGION" },
	{ DC_ANCHOR_RIGHT_MSG, 256, 256, 27, 50, 230, 202, "RIGHT_TEXT_BOX_REGION" }, /* XWAU: 53..256 */
	{ DC_ANCHOR_TOP_LEFT, 256, 32, 13, 0, 242, 35, "TOP_LEFT_REGION" },
	{ DC_ANCHOR_TOP_RIGHT, 256, 32, 14, 0, 243, 35, "TOP_RIGHT_REGION" },
	{ DC_ANCHOR_PANES_READY_SYSTEM, 0, 0, 0, 0, 0, 0, "TEXT_RADIOSYS_REGION" },
	{ DC_ANCHOR_PANE_FLIGHT_GROUP, 0, 0, 0, 0, 0, 0, "TEXT_CMD_REGION" },
};

static struct {
	/* Boxes in target pixels (x0, y0, x1, y1) and their valid bits. */
	float source_box[XWA_DC_SOURCE_COUNT][4];
	float region_box[XWA_DC_REGION_COUNT][4];
	uint64_t source_valid;
	uint32_t region_valid;
	/* Debug overlay. */
	AeronDrawList2D* overlay;
	int overlay_enabled;
	int overlay_highlight;
	int overlay_prepared;
} dc;

int XwaRemasterDc_Init(void) {
	if (!dc.overlay)
		dc.overlay = AeronDrawList_Create(0);
	dc.overlay_highlight = -1;
	return dc.overlay != NULL;
}

void XwaRemasterDc_Shutdown(void) {
	if (dc.overlay)
		AeronDrawList_Destroy(dc.overlay);
	memset(&dc, 0, sizeof dc);
}

/* The quad (x0, y0, x1, y1) each image anchor's record covers this frame. */
static void dc_image_anchors(int target_w, int target_h, float quads[DC_ANCHOR_IMAGE_COUNT][4],
							 uint32_t* valid) {
	*valid = 0;
	const XwaHudPreparedDrawState* draw = XwaRemasterHud_PreparedDrawState();
	if (!draw || !draw->valid)
		return;
	for (uint16_t i = 0; i < draw->record_count; ++i) {
		const XwaHudDrawRecord* r = &draw->records[i];
		for (int a = DC_ANCHOR_NONE + 1; a < DC_ANCHOR_IMAGE_COUNT; ++a) {
			if ((*valid & (1u << a)) || r->widget != dc_anchor_records[a].widget ||
				(dc_anchor_records[a].frame && r->frame != dc_anchor_records[a].frame))
				continue;
			float quad[4];
			if (!XwaRemasterHudFixed_RecordQuad(r, target_w, target_h, quad))
				continue;
			quads[a][0] = quad[0];
			quads[a][1] = quad[1];
			quads[a][2] = quad[0] + quad[2];
			quads[a][3] = quad[1] + quad[3];
			*valid |= 1u << a;
		}
	}
}

static int dc_pane_box(XwaHudAnchorId id, float box[4]) {
	const XwaHudLayoutProfile* profile = XwaRemasterHudLayout_Profile(XwaRemasterHud_Layout(), 0);
	if (!profile)
		return 0;
	const XwaHudLayoutRect* r = &profile->anchors[id].rect;
	if (r->w <= 0 || r->h <= 0)
		return 0;
	box[0] = (float)r->x;
	box[1] = (float)r->y;
	box[2] = (float)(r->x + r->w);
	box[3] = (float)(r->y + r->h);
	return 1;
}

/* Places `rect`: an image rectangle maps onto its anchor's quad as
 * ComputeCoordsFromUV does (mirrored draws included, as XWAU ignores the
 * mirror); a pane rectangle is the pane's layout anchor. */
static int dc_place(const DcRect* rect, const float quads[DC_ANCHOR_IMAGE_COUNT][4], uint32_t quads_valid,
					float box[4]) {
	if (rect->anchor > DC_ANCHOR_NONE && rect->anchor < DC_ANCHOR_IMAGE_COUNT) {
		if (!(quads_valid & (1u << rect->anchor)))
			return 0;
		const float* q = quads[rect->anchor];
		const float w = q[2] - q[0], h = q[3] - q[1];
		box[0] = q[0] + rect->x0 / rect->width * w;
		box[1] = q[1] + rect->y0 / rect->height * h;
		box[2] = q[0] + rect->x1 / rect->width * w;
		box[3] = q[1] + rect->y1 / rect->height * h;
		return 1;
	}
	switch (rect->anchor) {
		case DC_ANCHOR_PANE_READY:
			return dc_pane_box(XWA_HUD_ANCHOR_MESSAGE_READY, box);
		case DC_ANCHOR_PANE_SYSTEM:
			return dc_pane_box(XWA_HUD_ANCHOR_MESSAGE_SYSTEM, box);
		case DC_ANCHOR_PANE_FLIGHT_GROUP:
			return dc_pane_box(XWA_HUD_ANCHOR_MESSAGE_FLIGHT_GROUP, box);
		case DC_ANCHOR_PANES_READY_SYSTEM: {
			float other[4];
			if (!dc_pane_box(XWA_HUD_ANCHOR_MESSAGE_READY, box) ||
				!dc_pane_box(XWA_HUD_ANCHOR_MESSAGE_SYSTEM, other))
				return 0;
			box[0] = box[0] < other[0] ? box[0] : other[0];
			box[1] = box[1] < other[1] ? box[1] : other[1];
			box[2] = box[2] > other[2] ? box[2] : other[2];
			box[3] = box[3] > other[3] ? box[3] : other[3];
			return 1;
		}
		default:
			return 0;
	}
}

void XwaRemasterDc_Prepare(const XwaSnapshot* snapshot, int target_w, int target_h) {
	(void)snapshot;
	dc.source_valid = 0;
	dc.region_valid = 0;
	if (target_w <= 0 || target_h <= 0)
		return;
	float quads[DC_ANCHOR_IMAGE_COUNT][4];
	uint32_t quads_valid;
	dc_image_anchors(target_w, target_h, quads, &quads_valid);
	for (unsigned i = 0; i < XWA_DC_SOURCE_COUNT; ++i)
		if (dc_place(&dc_sources[i], quads, quads_valid, dc.source_box[i]))
			dc.source_valid |= (uint64_t)1 << i;
	for (unsigned i = 0; i < XWA_DC_REGION_COUNT; ++i)
		if (dc_place(&dc_regions[i], quads, quads_valid, dc.region_box[i]))
			dc.region_valid |= 1u << i;
}

int XwaRemasterDc_SourceBox(unsigned source, float box[4]) {
	if (source >= XWA_DC_SOURCE_COUNT || !(dc.source_valid & ((uint64_t)1 << source)))
		return 0;
	memcpy(box, dc.source_box[source], sizeof dc.source_box[source]);
	return 1;
}

int XwaRemasterDc_RegionBox(unsigned region, float box[4]) {
	if (region >= XWA_DC_REGION_COUNT || !(dc.region_valid & (1u << region)))
		return 0;
	memcpy(box, dc.region_box[region], sizeof dc.region_box[region]);
	return 1;
}

const char* XwaRemasterDc_SourceName(unsigned source) {
	return source < XWA_DC_SOURCE_COUNT ? dc_sources[source].name : NULL;
}

const char* XwaRemasterDc_RegionName(unsigned region) {
	return region < XWA_DC_REGION_COUNT ? dc_regions[region].name : NULL;
}

/* ---- Debug overlay ------------------------------------------------- */

int XwaRemasterDc_DebugBoxes(void) { return dc.overlay_enabled; }

void XwaRemasterDc_SetDebugBoxes(int enabled, int highlight) {
	dc.overlay_enabled = enabled != 0;
	dc.overlay_highlight = highlight;
}

static void dc_outline(const float box[4], const float color[4], float border) {
	AeronDrawList2DRRect r = { 0 };
	r.dst_x = box[0] < box[2] ? box[0] : box[2];
	r.dst_y = box[1] < box[3] ? box[1] : box[3];
	r.dst_w = box[2] > box[0] ? box[2] - box[0] : box[0] - box[2];
	r.dst_h = box[3] > box[1] ? box[3] - box[1] : box[1] - box[3];
	if (r.dst_w < 1.0f)
		r.dst_w = 1.0f;
	if (r.dst_h < 1.0f)
		r.dst_h = 1.0f;
	r.border_px = border;
	r.soft_px = 1.0f;
	memcpy(r.border, color, sizeof r.border);
	AeronDrawList_AddRRect(dc.overlay, &r);
}

void XwaRemasterDc_PrepareDebugOverlay(AeronCommandBuffer* cmd, int target_w, int target_h) {
	dc.overlay_prepared = 0;
	if (!dc.overlay_enabled || !dc.overlay || !cmd || target_w <= 0 || target_h <= 0)
		return;
	static const float source_color[4] = { 0.0f, 0.8f, 0.0f, 0.8f };
	static const float region_color[4] = { 0.8f, 0.0f, 0.0f, 0.8f };
	static const float highlight_color[4] = { 1.0f, 0.85f, 0.0f, 1.0f };
	AeronDrawList_Begin(dc.overlay, NULL, target_w, target_h, AERON_DRAWLIST2D_LOAD, NULL);
	for (unsigned i = 0; i < XWA_DC_REGION_COUNT; ++i)
		if (dc.region_valid & (1u << i))
			dc_outline(dc.region_box[i], region_color, 1.0f);
	for (unsigned i = 0; i < XWA_DC_SOURCE_COUNT; ++i)
		if ((dc.source_valid & ((uint64_t)1 << i)) && (int)i != dc.overlay_highlight)
			dc_outline(dc.source_box[i], source_color, 1.0f);
	if (dc.overlay_highlight >= 0 && dc.overlay_highlight < XWA_DC_SOURCE_COUNT &&
		(dc.source_valid & ((uint64_t)1 << dc.overlay_highlight)))
		dc_outline(dc.source_box[dc.overlay_highlight], highlight_color, 2.0f);
	dc.overlay_prepared = AeronDrawList_Prepare(dc.overlay, cmd);
}

void XwaRemasterDc_RenderDebugOverlay(AeronCommandBuffer* cmd, AeronRenderPass* pass,
									  AeronRenderTarget* target) {
	if (dc.overlay_prepared && dc.overlay_enabled && cmd && pass && target)
		AeronDrawList_RenderIntoPass(dc.overlay, cmd, pass, target);
}
