#include "xwa_remaster/dynamic_cockpit.h"

#include "aeron/log.h"
#include "aeron/scene/draw_list2d.h"
#include "xwa/assets/object_type.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/hud.h"
#include "xwa_remaster/hud_fixed.h"
#include "xwa_remaster/hud_layout.h"
#include "xwa_remaster/hud_text.h"

#include <math.h>
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
	/* Settings (XwaRemasterDcSettings). */
	XwaRemasterDcSettings settings;
	/* Frame state: active this frame, the cockpit's hide mask and the
	 * regions erased from the 2D HUD. */
	int active;
	uint32_t hide_mask;
	uint32_t erased;
	/* The sources the cockpit's screens sample: the only ones captured. */
	uint64_t source_mask;
	/* Boxes placed this frame (PlaceFrame). */
	int placed;
	/* Erased regions in target pixels (x0, y0, x1, y1): routed records are
	 * not drawn there on the 2D HUD. */
	int erase[XWA_DC_REGION_COUNT][4];
	int erase_count;
	/* The capture: the sources padded and merged, in target pixels, their
	 * slots in the atlas and each source's capture rectangle (-1: none). */
	int capture[XWA_DC_SOURCE_COUNT][4];
	int capture_slot[XWA_DC_SOURCE_COUNT][2];
	int capture_count;
	int8_t source_capture[XWA_DC_SOURCE_COUNT];
	/* Capture atlases per XwaRemasterDcLayer, their lists (open between
	 * PlaceFrame and Prepare) and their size. */
	AeronRenderTarget* atlas[2];
	AeronDrawList2D* capture_list[2];
	int atlas_w, atlas_h;
	int capture_open;
	int warm_pending;
	/* Display inputs of this frame. */
	AeronSceneScreenFrame frame;
	/* Sources fitted to the top text (42-49): grow-only boxes since the
	 * cockpit or the target size last changed (DCResetSubRegions). */
	float auto_box[8][4];
	uint32_t auto_valid;
	const void* auto_owner;
	int auto_w, auto_h;
	/* Hologram animation, advanced by game time. */
	float holo_time, holo_fade;
	int32_t last_game_ticks;
	int have_game_ticks;
} dc;

/* Display text starts at the OPT
 * cockpit lights' strength. */
static const XwaRemasterDcSettings dc_default_settings = {
	.enabled = 1,
	.classic_hud = 0,
	.holograms = 1,
	.display_emissive_strength = 1.5f,
	.hologram_emissive_strength = 1.5f,
};

/* Capture layout: the padding around each source (bilinear sampling at its
 * edges reads the same texels as on the HUD), the atlas size steps and
 * limit, and the most pieces an erase-clipped record splits into. */
#define DC_CAPTURE_PAD 2
#define DC_ATLAS_MIN_W 1024
#define DC_ATLAS_STEP 256
#define DC_ATLAS_MAX 8192
#define DC_MAX_PIECES 32

int XwaRemasterDc_Init(void) {
	if (!dc.overlay)
		dc.overlay = AeronDrawList_Create(0);
	for (int i = 0; i < 2; ++i)
		if (!dc.capture_list[i])
			dc.capture_list[i] = AeronDrawList_Create(8192);
	dc.overlay_highlight = -1;
	if (!dc.settings.display_emissive_strength)
		dc.settings = dc_default_settings;
	dc.holo_fade = dc.settings.holograms ? 1.0f : 0.0f;
	return dc.overlay != NULL && dc.capture_list[0] != NULL && dc.capture_list[1] != NULL;
}

void XwaRemasterDc_ReleaseTargets(void) {
	for (int i = 0; i < 2; ++i) {
		if (dc.atlas[i])
			Aeron_DestroyRenderTarget(dc.atlas[i]);
		dc.atlas[i] = NULL;
	}
	dc.atlas_w = dc.atlas_h = 0;
	dc.capture_open = 0;
	dc.active = 0;
}

void XwaRemasterDc_Shutdown(void) {
	XwaRemasterDc_ReleaseTargets();
	if (dc.overlay)
		AeronDrawList_Destroy(dc.overlay);
	for (int i = 0; i < 2; ++i)
		if (dc.capture_list[i])
			AeronDrawList_Destroy(dc.capture_list[i]);
	const XwaRemasterDcSettings settings = dc.settings;
	memset(&dc, 0, sizeof dc);
	dc.settings = settings;
}

void XwaRemasterDc_GetSettings(XwaRemasterDcSettings* out) {
	if (out)
		*out = dc.settings.display_emissive_strength ? dc.settings : dc_default_settings;
}

void XwaRemasterDc_SetSettings(const XwaRemasterDcSettings* settings) {
	if (settings)
		dc.settings = *settings;
}

void XwaRemasterDc_ToggleHolograms(void) {
	dc.settings.holograms = !dc.settings.holograms;
	Aeron_LogInfo("xwa.remaster", "dynamic cockpit holograms %s", dc.settings.holograms ? "on" : "off");
}

int XwaRemasterDc_Active(void) { return dc.active; }

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

/* ---- Frame -------------------------------------------------------- */

/* Atlases of at least w x h, grown in DC_ATLAS_STEP steps (active is
 * kept: the atlas only grows between frames' captures). */
static int dc_ensure_atlas(int w, int h) {
	if (dc.atlas[0] && dc.atlas[1] && dc.atlas_w >= w && dc.atlas_h >= h)
		return 1;
	w = ((w > dc.atlas_w ? w : dc.atlas_w) + DC_ATLAS_STEP - 1) / DC_ATLAS_STEP * DC_ATLAS_STEP;
	h = ((h > dc.atlas_h ? h : dc.atlas_h) + DC_ATLAS_STEP - 1) / DC_ATLAS_STEP * DC_ATLAS_STEP;
	static const char* const names[2] = { "xwa.dc.graphics", "xwa.dc.text" };
	for (int i = 0; i < 2; ++i) {
		if (dc.atlas[i])
			Aeron_DestroyRenderTarget(dc.atlas[i]);
		dc.atlas[i] = Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) {
			.width = w, .height = h, .format = AERON_TEXTURE_FORMAT_RGBA8_SRGB, .debug_name = names[i] });
	}
	if (!dc.atlas[0] || !dc.atlas[1]) {
		Aeron_LogError("xwa.remaster", "dynamic cockpit capture atlas failed at %dx%d", w, h);
		XwaRemasterDc_ReleaseTargets();
		return 0;
	}
	dc.atlas_w = w;
	dc.atlas_h = h;
	/* Capture once now so the draw-list pipelines for this format are made
	 * with the other flight pipelines, not when the cockpit first shows. */
	dc.warm_pending = 1;
	return 1;
}

void XwaRemasterDc_BeginFrame(const XwaSnapshot* snapshot, const AeronSceneMesh* cockpit_mesh, int target_w,
							  int target_h) {
	dc.active = 0;
	dc.erased = 0;
	dc.hide_mask = 0;
	dc.source_mask = 0;
	dc.placed = 0;
	dc.capture_open = 0;
	if (!dc.settings.enabled || target_w <= 0 || target_h <= 0) {
		XwaRemasterDc_ReleaseTargets();
	} else if (dc_ensure_atlas(DC_ATLAS_MIN_W, DC_ATLAS_STEP) && cockpit_mesh &&
			   (cockpit_mesh->screen_rect_count > 0 || cockpit_mesh->screen_hide_mask != 0)) {
		dc.active = 1;
		dc.hide_mask = cockpit_mesh->screen_hide_mask;
		dc.source_mask = cockpit_mesh->screen_source_mask;
		dc.erased = dc.settings.classic_hud ? 0u : dc.hide_mask & ((1u << XWA_DC_REGION_COUNT) - 1u);
	}
	XwaRemasterHudFixed_SetDynamicCockpit(dc.active, dc.erased);
	if ((cockpit_mesh && cockpit_mesh != dc.auto_owner) || target_w != dc.auto_w || target_h != dc.auto_h) {
		dc.auto_valid = 0;
		if (cockpit_mesh)
			dc.auto_owner = cockpit_mesh;
		dc.auto_w = target_w;
		dc.auto_h = target_h;
	}

	/* Hologram time and fade follow game time, so they hold while paused
	 * (XWAU's 0.04 per frame at 60 fps). */
	float dt = 0.0f;
	if (snapshot) {
		if (dc.have_game_ticks && snapshot->game_time_ticks >= dc.last_game_ticks)
			dt = (float)((double)(snapshot->game_time_ticks - dc.last_game_ticks) *
						 XWA_SNAP_GAME_TICK_SECONDS);
		dc.last_game_ticks = snapshot->game_time_ticks;
		dc.have_game_ticks = 1;
	}
	if (dt > 0.25f)
		dt = 0.25f;
	dc.holo_time += dt;
	const float target_fade = dc.settings.holograms ? 1.0f : 0.0f;
	if (dc.holo_fade < target_fade)
		dc.holo_fade = fminf(target_fade, dc.holo_fade + 2.4f * dt);
	else if (dc.holo_fade > target_fade)
		dc.holo_fade = fmaxf(target_fade, dc.holo_fade - 2.4f * dt);
}

static void dc_place_boxes(int target_w, int target_h) {
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

/* ---- Sources from the HUD text and laser charges ------------------ */

enum {
	DC_AUTO_MISSILES_L = 42,
	DC_AUTO_MISSILES_R = 43,
	DC_AUTO_SPEED = 44,
	DC_AUTO_CHAFF = 45,
	DC_AUTO_NAME = 46,
	DC_AUTO_TIME = 47,
	DC_AUTO_MISSILES_B = 48,
	DC_AUTO_MISSILES_MIS = 49,
	DC_ENERGY_DUAL_L = 50,
	DC_ENERGY_DUAL_R = 51,
	DC_ENERGY_QUAD_LU = 52,
	DC_ENERGY_QUAD_LD = 53,
	DC_ENERGY_QUAD_RU = 54,
	DC_ENERGY_QUAD_RD = 55,
	DC_ENERGY_ALL = 56,
};

static void dc_union(float box[4], const float r[4], int first) {
	if (first) {
		memcpy(box, r, 4 * sizeof *box);
		return;
	}
	box[0] = fminf(box[0], r[0]);
	box[1] = fminf(box[1], r[1]);
	box[2] = fmaxf(box[2], r[2]);
	box[3] = fmaxf(box[3], r[3]);
}

static void dc_grow_auto(unsigned source, const float r[4]) {
	const unsigned slot = source - DC_AUTO_MISSILES_L;
	dc_union(dc.auto_box[slot], r, !(dc.auto_valid & (1u << slot)));
	dc.auto_valid |= 1u << slot;
}

/* PrimarySurface::ExtractDCText's per-region character rules, applied to
 * the HUD panes holding the same text. `context` is the missile side: 0
 * before the ':' of the warhead counts, 1 after. */
static void dc_visit_glyph(void* context, uint16_t pane, uint8_t ch, const float rect[4]) {
	int* missile_side = (int*)context;
	const int digit = ch >= '0' && ch <= '9';
	switch ((XwaHudPaneId)pane) {
		case XWA_HUD_PANE_TOP_SPEED:
			if (digit)
				dc_grow_auto(DC_AUTO_SPEED, rect);
			break;
		case XWA_HUD_PANE_TOP_CLOCK:
			if (digit || ch == ':')
				dc_grow_auto(DC_AUTO_TIME, rect);
			break;
		case XWA_HUD_PANE_TOP_COUNTERMEASURE:
			if (ch != ' ')
				dc_grow_auto(DC_AUTO_CHAFF, rect);
			break;
		case XWA_HUD_PANE_TOP_CRAFT_NAME:
			dc_grow_auto(DC_AUTO_NAME, rect);
			break;
		case XWA_HUD_PANE_TOP_WEAPONS:
			dc_grow_auto(DC_AUTO_MISSILES_B, rect);
			if (ch == ' ')
				break;
			dc_grow_auto(DC_AUTO_MISSILES_MIS, rect);
			if (ch == ':')
				*missile_side = 1;
			else
				dc_grow_auto(*missile_side ? DC_AUTO_MISSILES_R : DC_AUTO_MISSILES_L, rect);
			break;
		default:
			break;
	}
}

/* DcEnergyBarData::ReduceAndLabelBoxes: the laser-charge boxes (HUD.dat
 * 2300, lasers and ion cannons alike) reduced to the strip through their
 * centres, labelled by count, and their union. */
static void dc_place_energy_bars(int target_w, int target_h) {
	const XwaHudPreparedDrawState* draw = XwaRemasterHud_PreparedDrawState();
	if (!draw || !draw->valid)
		return;
	float boxes[16][4];
	int count = 0;
	for (uint16_t i = 0; i < draw->record_count && count < 16; ++i) {
		const XwaHudDrawRecord* r = &draw->records[i];
		float quad[4];
		if ((r->widget == XWA_HUD_WIDGET_LASER_CHARGE || r->widget == XWA_HUD_WIDGET_ION_CHARGE) &&
			r->object_type == OBJ_HudTextureGroup12000 && r->frame == 23 &&
			XwaRemasterHudFixed_RecordQuad(r, target_w, target_h, quad)) {
			boxes[count][0] = quad[0];
			boxes[count][1] = quad[1];
			boxes[count][2] = quad[0] + quad[2];
			boxes[count][3] = quad[1] + quad[3];
			count++;
		}
	}
	if (count == 0)
		return;
	float min_y = boxes[0][1], max_y = boxes[0][3];
	for (int i = 1; i < count; ++i) {
		min_y = fminf(min_y, boxes[i][1]);
		max_y = fmaxf(max_y, boxes[i][3]);
	}
	const float mid_x = 0.5f * (float)target_w, mid_y = 0.5f * (min_y + max_y);
	float all[4];
	for (int i = 0; i < count; ++i) {
		const float w = boxes[i][2] - boxes[i][0], h = boxes[i][3] - boxes[i][1];
		const float x = 0.5f * (boxes[i][0] + boxes[i][2]), y = 0.5f * (boxes[i][1] + boxes[i][3]);
		const float reduced[4] = { x - 28.5f / 64.0f * w, y - 0.5f / 64.0f * h, x + 28.5f / 64.0f * w,
								   y + 0.5f / 64.0f * h };
		dc_union(all, reduced, i == 0);
		int source = -1;
		if (count == 2)
			source = x < mid_x ? DC_ENERGY_DUAL_L : DC_ENERGY_DUAL_R;
		else if (count == 4)
			source = y < mid_y ? (x < mid_x ? DC_ENERGY_QUAD_LU : DC_ENERGY_QUAD_RU)
							   : (x < mid_x ? DC_ENERGY_QUAD_LD : DC_ENERGY_QUAD_RD);
		if (source >= 0) {
			memcpy(dc.source_box[source], reduced, sizeof reduced);
			dc.source_valid |= (uint64_t)1 << source;
		}
	}
	memcpy(dc.source_box[DC_ENERGY_ALL], all, sizeof all);
	dc.source_valid |= (uint64_t)1 << DC_ENERGY_ALL;
}

static void dc_place_dynamic(int target_w, int target_h) {
	int missile_side = 0;
	XwaRemasterHudText_ForEachGlyph(target_w, target_h, dc_visit_glyph, &missile_side);
	for (unsigned slot = 0; slot < 8; ++slot) {
		if (!(dc.auto_valid & (1u << slot)))
			continue;
		memcpy(dc.source_box[DC_AUTO_MISSILES_L + slot], dc.auto_box[slot], sizeof dc.auto_box[slot]);
		dc.source_valid |= (uint64_t)1 << (DC_AUTO_MISSILES_L + slot);
	}
	dc_place_energy_bars(target_w, target_h);
}

/* The display inputs (AeronSceneScreenFrame) of this frame. */
static void dc_build_frame(const XwaSnapshot* snapshot) {
	AeronSceneScreenFrame* f = &dc.frame;
	memset(f, 0, sizeof *f);
	/* Each source where its capture rectangle sits in the atlas. */
	for (unsigned i = 0; i < XWA_DC_SOURCE_COUNT; ++i) {
		const int c = dc.source_capture[i];
		if (!(dc.source_valid & ((uint64_t)1 << i)) || c < 0)
			continue;
		const float dx = (float)(dc.capture_slot[c][0] - dc.capture[c][0]);
		const float dy = (float)(dc.capture_slot[c][1] - dc.capture[c][1]);
		f->valid[i >> 5] |= 1u << (i & 31u);
		f->source[i][0] = (dc.source_box[i][0] + dx) / (float)dc.atlas_w;
		f->source[i][1] = (dc.source_box[i][1] + dy) / (float)dc.atlas_h;
		f->source[i][2] = (dc.source_box[i][2] + dx) / (float)dc.atlas_w;
		f->source[i][3] = (dc.source_box[i][3] + dy) / (float)dc.atlas_h;
	}
	const XwaHudState* hud = snapshot ? &snapshot->hud : NULL;
	/* D17: the backgrounds the reticle frames follow in hud_fixed.c. */
	if (hud && hud->target.valid && hud->reticle.missile_lock_state == 3)
		f->color_state = 2u;
	else if (hud && hud->reticle.in_range)
		f->color_state = 1u;
	f->flags = dc.settings.classic_hud ? AERON_SCENE_SCREEN_HIDE_TRANSPARENT : AERON_SCENE_SCREEN_BAR_VISIBLE;
	f->display_strength = dc.settings.display_emissive_strength;
	/* CraftData.throttleSpeed (0xF0), XWAU's EngineThrottleInput. */
	f->bar_level = hud ? (float)hud->instruments.throttle_speed / 65535.0f : 0.0f;
	f->holo_time = dc.holo_time;
	f->holo_fade = dc.holo_fade;
	const uint32_t hud_color = hud ? hud->hud_colors[0] : 0xff00ff00u;
	f->holo_color[0] = XwaRemaster_SrgbToLinear(((hud_color >> 16) & 255u) / 255.0f);
	f->holo_color[1] = XwaRemaster_SrgbToLinear(((hud_color >> 8) & 255u) / 255.0f);
	f->holo_color[2] = XwaRemaster_SrgbToLinear((hud_color & 255u) / 255.0f);
	f->holo_color[3] = dc.settings.hologram_emissive_strength;
}

/* ---- Capture layout ----------------------------------------------- */

static int dc_overlap(const int a[4], const int b[4]) {
	return a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3];
}

/* The erased regions, rounded to the pixels whose centres they cover. */
static void dc_layout_erase(void) {
	dc.erase_count = 0;
	for (unsigned i = 0; i < XWA_DC_REGION_COUNT; ++i) {
		if (!(dc.erased & dc.region_valid & (1u << i)))
			continue;
		const float* r = dc.region_box[i];
		int* e = dc.erase[dc.erase_count];
		e[0] = (int)lroundf(fminf(r[0], r[2]));
		e[1] = (int)lroundf(fminf(r[1], r[3]));
		e[2] = (int)lroundf(fmaxf(r[0], r[2]));
		e[3] = (int)lroundf(fmaxf(r[1], r[3]));
		if (e[2] > e[0] && e[3] > e[1])
			dc.erase_count++;
	}
}

/* The capture rectangles: the valid sources the cockpit's screens sample,
 * padded by DC_CAPTURE_PAD (outside the target too: the screens sample
 * whole sources), merged where they overlap, shelf-packed tallest first
 * into the atlas. */
static void dc_layout_capture(void) {
	dc.capture_count = 0;
	memset(dc.source_capture, -1, sizeof dc.source_capture);
	int padded[XWA_DC_SOURCE_COUNT][4];
	const uint64_t captured = dc.source_valid & dc.source_mask;
	for (unsigned i = 0; i < XWA_DC_SOURCE_COUNT; ++i) {
		if (!(captured & ((uint64_t)1 << i)))
			continue;
		const float* b = dc.source_box[i];
		int* p = padded[i];
		p[0] = (int)floorf(fminf(b[0], b[2])) - DC_CAPTURE_PAD;
		p[1] = (int)floorf(fminf(b[1], b[3])) - DC_CAPTURE_PAD;
		p[2] = (int)ceilf(fmaxf(b[0], b[2])) + DC_CAPTURE_PAD;
		p[3] = (int)ceilf(fmaxf(b[1], b[3])) + DC_CAPTURE_PAD;
		memcpy(dc.capture[dc.capture_count++], p, sizeof padded[i]);
	}
	for (int merged = 1; merged;) {
		merged = 0;
		for (int a = 0; a < dc.capture_count && !merged; ++a)
			for (int b = a + 1; b < dc.capture_count && !merged; ++b) {
				if (!dc_overlap(dc.capture[a], dc.capture[b]))
					continue;
				int* r = dc.capture[a];
				const int* o = dc.capture[b];
				r[0] = r[0] < o[0] ? r[0] : o[0];
				r[1] = r[1] < o[1] ? r[1] : o[1];
				r[2] = r[2] > o[2] ? r[2] : o[2];
				r[3] = r[3] > o[3] ? r[3] : o[3];
				memcpy(dc.capture[b], dc.capture[--dc.capture_count], sizeof dc.capture[b]);
				merged = 1;
			}
	}
	for (unsigned i = 0; i < XWA_DC_SOURCE_COUNT; ++i) {
		if (!(captured & ((uint64_t)1 << i)))
			continue;
		for (int c = 0; c < dc.capture_count && dc.source_capture[i] < 0; ++c)
			if (dc.capture[c][0] <= padded[i][0] && dc.capture[c][1] <= padded[i][1] &&
				dc.capture[c][2] >= padded[i][2] && dc.capture[c][3] >= padded[i][3])
				dc.source_capture[i] = (int8_t)c;
	}

	int order[XWA_DC_SOURCE_COUNT];
	int width = DC_ATLAS_MIN_W;
	for (int c = 0; c < dc.capture_count; ++c) {
		const int w = dc.capture[c][2] - dc.capture[c][0];
		width = w > width ? w : width;
		int j = c;
		for (; j > 0; --j) {
			const int* prev = dc.capture[order[j - 1]];
			if (prev[3] - prev[1] >= dc.capture[c][3] - dc.capture[c][1])
				break;
			order[j] = order[j - 1];
		}
		order[j] = c;
	}
	int x = 0, y = 0, shelf = 0;
	for (int k = 0; k < dc.capture_count; ++k) {
		const int* r = dc.capture[order[k]];
		const int w = r[2] - r[0], h = r[3] - r[1];
		if (x + w > width) {
			x = 0;
			y += shelf;
			shelf = 0;
		}
		dc.capture_slot[order[k]][0] = x;
		dc.capture_slot[order[k]][1] = y;
		x += w;
		shelf = h > shelf ? h : shelf;
	}
	if (width > DC_ATLAS_MAX || y + shelf > DC_ATLAS_MAX || !dc_ensure_atlas(width, y + shelf)) {
		dc.capture_count = 0;
		memset(dc.source_capture, -1, sizeof dc.source_capture);
	}
}

void XwaRemasterDc_PlaceFrame(int target_w, int target_h) {
	dc.source_valid = 0;
	dc.region_valid = 0;
	dc.erase_count = 0;
	dc.capture_count = 0;
	dc.capture_open = 0;
	if (target_w <= 0 || target_h <= 0)
		return;
	dc_place_boxes(target_w, target_h);
	dc_place_dynamic(target_w, target_h);
	dc.placed = 1;
	if (dc.active) {
		dc_layout_erase();
		dc_layout_capture();
	}
	if (dc.atlas[0] && dc.atlas[1] && (dc.active || dc.warm_pending)) {
		static const float clear_rgba[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		for (int i = 0; i < 2; ++i) {
			AeronDrawList_Begin(dc.capture_list[i], NULL, dc.atlas_w, dc.atlas_h, AERON_DRAWLIST2D_LOAD, NULL);
			/* A transparent pixel makes the warm-up capture build the
			 * HUD sprites' pipeline for the atlas format. */
			if (!dc.active)
				AeronDrawList_AddFill(dc.capture_list[i], 0.0f, 0.0f, 1.0f, 1.0f, clear_rgba,
									  AERON_BLIT2D_BLEND_PMA, NULL);
		}
		dc.capture_open = 1;
	}
}

/* ---- Record routing ----------------------------------------------- */

/* Adds `record` to `list` moved by (dx, dy) and clipped to `scissor`
 * (NULL or empty: unclipped). */
typedef void (*DcEmitFn)(AeronDrawList2D* list, const void* record, int dx, int dy, const AeronRectI* scissor);

static int dc_scissor_set(const AeronRectI* s) { return s && s->width > 0 && s->height > 0; }

/* The parts of `box` outside the erased regions, or -1 when it meets none
 * (or splits into more than DC_MAX_PIECES: drawn whole). */
static int dc_visible_pieces(const int box[4], int pieces[DC_MAX_PIECES][4]) {
	int n = 1, touched = 0;
	memcpy(pieces[0], box, 4 * sizeof *box);
	for (int e = 0; e < dc.erase_count; ++e) {
		const int* r = dc.erase[e];
		int next[DC_MAX_PIECES][4], m = 0;
		for (int p = 0; p < n; ++p) {
			const int* q = pieces[p];
			if (!dc_overlap(q, r)) {
				if (m == DC_MAX_PIECES)
					return -1;
				memcpy(next[m++], q, sizeof next[0]);
				continue;
			}
			touched = 1;
			const int y0 = q[1] > r[1] ? q[1] : r[1], y1 = q[3] < r[3] ? q[3] : r[3];
			const int split[4][4] = { { q[0], q[1], q[2], r[1] },
									  { q[0], r[3], q[2], q[3] },
									  { q[0], y0, r[0], y1 },
									  { r[2], y0, q[2], y1 } };
			for (int k = 0; k < 4; ++k) {
				if (split[k][2] <= split[k][0] || split[k][3] <= split[k][1])
					continue;
				if (m == DC_MAX_PIECES)
					return -1;
				memcpy(next[m++], split[k], sizeof next[0]);
			}
		}
		memcpy(pieces, next, (size_t)m * sizeof next[0]);
		n = m;
	}
	return touched ? n : -1;
}

/* `bounds` (x0, y0, x1, y1) covers what the record draws, `scissor` its
 * own clip. */
static void dc_route(AeronDrawList2D* screen, XwaRemasterDcLayer layer, const float bounds[4],
					 const AeronRectI* scissor, DcEmitFn emit, const void* record) {
	int box[4] = { (int)floorf(bounds[0]), (int)floorf(bounds[1]), (int)ceilf(bounds[2]), (int)ceilf(bounds[3]) };
	if (dc_scissor_set(scissor)) {
		box[0] = box[0] > scissor->x ? box[0] : scissor->x;
		box[1] = box[1] > scissor->y ? box[1] : scissor->y;
		box[2] = box[2] < scissor->x + scissor->width ? box[2] : scissor->x + scissor->width;
		box[3] = box[3] < scissor->y + scissor->height ? box[3] : scissor->y + scissor->height;
	}
	/* Only the on-screen part draws, and the pieces cut from it become
	 * scissors, which must lie within the screen. */
	box[0] = box[0] > 0 ? box[0] : 0;
	box[1] = box[1] > 0 ? box[1] : 0;
	box[2] = box[2] < dc.auto_w ? box[2] : dc.auto_w;
	box[3] = box[3] < dc.auto_h ? box[3] : dc.auto_h;
	if (box[2] <= box[0] || box[3] <= box[1])
		return;

	int pieces[DC_MAX_PIECES][4];
	const int n = dc_visible_pieces(box, pieces);
	if (n < 0) {
		/* The record's own scissor cut to the screen, or none. */
		const AeronRectI clip = { box[0], box[1], box[2] - box[0], box[3] - box[1] };
		emit(screen, record, 0, 0, dc_scissor_set(scissor) ? &clip : NULL);
	}
	for (int p = 0; p < n; ++p) {
		const AeronRectI clip = { pieces[p][0], pieces[p][1], pieces[p][2] - pieces[p][0],
								  pieces[p][3] - pieces[p][1] };
		emit(screen, record, 0, 0, &clip);
	}

	/* The capture clip is the slot, cut by the record's own scissor, not
	 * the record's box: records only draw inside their box, and a clip
	 * shared by every record of the slot lets the draw list merge them
	 * (one draw per slot and texture instead of one per glyph). */
	for (int c = 0; c < dc.capture_count; ++c) {
		const int* r = dc.capture[c];
		if (!dc_overlap(box, r))
			continue;
		const int dx = dc.capture_slot[c][0] - r[0], dy = dc.capture_slot[c][1] - r[1];
		int       x0 = r[0], y0 = r[1], x1 = r[2], y1 = r[3];
		if (dc_scissor_set(scissor)) {
			x0 = x0 > scissor->x ? x0 : scissor->x;
			y0 = y0 > scissor->y ? y0 : scissor->y;
			x1 = x1 < scissor->x + scissor->width ? x1 : scissor->x + scissor->width;
			y1 = y1 < scissor->y + scissor->height ? y1 : scissor->y + scissor->height;
		}
		const AeronRectI clip = { x0 + dx, y0 + dy, x1 - x0, y1 - y0 };
		emit(dc.capture_list[layer], record, dx, dy, &clip);
	}
}

static int dc_routing(void) { return dc.active && dc.capture_open; }

static void dc_emit_sprite(AeronDrawList2D* list, const void* record, int dx, int dy, const AeronRectI* scissor) {
	AeronDrawList2DSprite sprite = *(const AeronDrawList2DSprite*)record;
	sprite.dst_x += (float)dx;
	sprite.dst_y += (float)dy;
	sprite.scissor = dc_scissor_set(scissor) ? *scissor : (AeronRectI) { 0 };
	AeronDrawList_AddSprite(list, &sprite);
}

void XwaRemasterDc_AddSprite(AeronDrawList2D* screen, XwaRemasterDcLayer layer,
							 const AeronDrawList2DSprite* sprite) {
	if (!screen || !sprite)
		return;
	if (!dc_routing()) {
		AeronDrawList_AddSprite(screen, sprite);
		return;
	}
	/* Trapezoid insets may widen the top edge. */
	const float spread = fmaxf(fabsf(sprite->trap_top_dx_left_px), fabsf(sprite->trap_top_dx_right_px));
	const float bounds[4] = { fminf(sprite->dst_x, sprite->dst_x + sprite->dst_w) - spread,
							  fminf(sprite->dst_y, sprite->dst_y + sprite->dst_h),
							  fmaxf(sprite->dst_x, sprite->dst_x + sprite->dst_w) + spread,
							  fmaxf(sprite->dst_y, sprite->dst_y + sprite->dst_h) };
	dc_route(screen, layer, bounds, &sprite->scissor, dc_emit_sprite, sprite);
}

static void dc_emit_quad4(AeronDrawList2D* list, const void* record, int dx, int dy, const AeronRectI* scissor) {
	AeronDrawList2DQuad4 quad = *(const AeronDrawList2DQuad4*)record;
	for (int c = 0; c < 4; ++c) {
		quad.corners[c][0] += (float)dx;
		quad.corners[c][1] += (float)dy;
	}
	quad.scissor = dc_scissor_set(scissor) ? *scissor : (AeronRectI) { 0 };
	AeronDrawList_AddQuad4(list, &quad);
}

void XwaRemasterDc_AddQuad4(AeronDrawList2D* screen, XwaRemasterDcLayer layer, const AeronDrawList2DQuad4* quad) {
	if (!screen || !quad)
		return;
	if (!dc_routing()) {
		AeronDrawList_AddQuad4(screen, quad);
		return;
	}
	float bounds[4] = { quad->corners[0][0], quad->corners[0][1], quad->corners[0][0], quad->corners[0][1] };
	for (int c = 1; c < 4; ++c) {
		bounds[0] = fminf(bounds[0], quad->corners[c][0]);
		bounds[1] = fminf(bounds[1], quad->corners[c][1]);
		bounds[2] = fmaxf(bounds[2], quad->corners[c][0]);
		bounds[3] = fmaxf(bounds[3], quad->corners[c][1]);
	}
	dc_route(screen, layer, bounds, &quad->scissor, dc_emit_quad4, quad);
}

typedef struct DcFill {
	float x, y, w, h;
	const float* rgba;
	AeronBlit2DBlend blend;
} DcFill;

static void dc_emit_fill(AeronDrawList2D* list, const void* record, int dx, int dy, const AeronRectI* scissor) {
	const DcFill* f = (const DcFill*)record;
	AeronDrawList_AddFill(list, f->x + (float)dx, f->y + (float)dy, f->w, f->h, f->rgba, f->blend,
						  dc_scissor_set(scissor) ? scissor : NULL);
}

void XwaRemasterDc_AddFill(AeronDrawList2D* screen, XwaRemasterDcLayer layer, float x, float y, float w, float h,
						   const float rgba[4], AeronBlit2DBlend blend, const AeronRectI* scissor) {
	if (!screen || !rgba)
		return;
	if (!dc_routing()) {
		AeronDrawList_AddFill(screen, x, y, w, h, rgba, blend, scissor);
		return;
	}
	const DcFill fill = { x, y, w, h, rgba, blend };
	const float bounds[4] = { x, y, x + w, y + h };
	dc_route(screen, layer, bounds, scissor, dc_emit_fill, &fill);
}

/* ---- Frame end ----------------------------------------------------- */

void XwaRemasterDc_Prepare(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot, int target_w, int target_h) {
	if (!dc.placed) {
		/* The HUD was not built this frame: nothing is placed. */
		dc.source_valid = 0;
		dc.region_valid = 0;
		dc.capture_count = 0;
	}
	if (cmd && dc.capture_open) {
		static const char* const labels[2] = { "OpenXWA DC graphics", "OpenXWA DC text" };
		for (int i = 0; i < 2; ++i) {
			/* Cleared even when empty: the screens sample every source. */
			const int prepared = AeronDrawList_Prepare(dc.capture_list[i], cmd);
			AeronRenderPass* pass = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
				.color_target = dc.atlas[i],
				.clear_color = 1,
				.clear_color_rgba = { 0, 0, 0, 0 },
				.command_buffer = cmd,
				.debug_label = labels[i],
			});
			if (!pass)
				continue;
			if (prepared)
				AeronDrawList_RenderIntoPass(dc.capture_list[i], cmd, pass, dc.atlas[i]);
			Aeron_EndRenderPass(pass);
		}
		dc.warm_pending = 0;
	}
	dc.capture_open = 0;
	if (cmd && dc.active)
		dc_build_frame(snapshot);
	XwaRemasterDc_PrepareDebugOverlay(cmd, target_w, target_h);
}

void XwaRemasterDc_ApplyToScene(AeronScene3D* scene) {
	if (dc.active && dc.atlas[0] && dc.atlas[1])
		AeronScene_SetScreenSources(scene, Aeron_RenderTargetGetTexture(dc.atlas[0]),
									Aeron_RenderTargetGetTexture(dc.atlas[1]), &dc.frame);
	else
		AeronScene_SetScreenSources(scene, NULL, NULL, NULL);
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
