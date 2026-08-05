#ifndef XWA_REMASTER_HUD_H
#define XWA_REMASTER_HUD_H

#include "aeron/vfs.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"
#include "xwa_remaster/flight.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaHudLayout XwaHudLayout;
typedef uint8_t XwaHudProfileIndex;

#define XWA_HUD_MAX_FIXED_DRAWS 512

typedef struct XwaHudDrawRecord {
	uint16_t widget;
	uint16_t anchor;
	uint16_t object_type;
	uint16_t frame;
	uint16_t screen_size;
	uint8_t phase;
	uint8_t mirror_x;
	uint16_t stable_order;
	uint16_t sequence;
	int16_t rotation_angle;
	uint16_t reserved;
	float center_x_ref;
	float center_y_ref;
	float scale_ref;
	uint32_t argb;
} XwaHudDrawRecord;

typedef struct XwaHudPreparedDrawState {
	uint32_t layout_generation;
	uint32_t bundle_generation;
	uint8_t profile;
	uint8_t valid;
	uint16_t record_count;
	uint16_t dropped_records;
	uint16_t reserved;
	XwaHudDrawRecord records[XWA_HUD_MAX_FIXED_DRAWS];
} XwaHudPreparedDrawState;

/* Dispatcher-level visibility derived from the captured classic state. Widget
 * builders apply their own feature/damage gates after this coarse selection. */
typedef struct XwaRemasterHudVisibility {
	uint8_t film_mfds;
	uint8_t fixed_frames;
	uint8_t radars;
	uint8_t radar_blips;
	uint8_t power;
	uint8_t charge;
	uint8_t shield_hull;
	uint8_t beam;
	uint8_t reticle;
	uint8_t threats;
	uint8_t target_arrow;
	uint8_t cmd;
	uint8_t mfd_frames;
	uint8_t cmd_pip;
	uint8_t pane_glyphs;
} XwaRemasterHudVisibility;

typedef enum XwaHudAnchorId {
	XWA_HUD_ANCHOR_RETICLE = 0,
	XWA_HUD_ANCHOR_THREATS,
	XWA_HUD_ANCHOR_FORE_RADAR,
	XWA_HUD_ANCHOR_AFT_RADAR,
	XWA_HUD_ANCHOR_LEFT_POWER,
	XWA_HUD_ANCHOR_RIGHT_POWER,
	XWA_HUD_ANCHOR_LEFT_SHIELD,
	XWA_HUD_ANCHOR_RIGHT_BEAM,
	XWA_HUD_ANCHOR_LASER_CHARGE,
	XWA_HUD_ANCHOR_ION_CHARGE,
	XWA_HUD_ANCHOR_CMD_FRAME,
	XWA_HUD_ANCHOR_CMD_TEXT,
	XWA_HUD_ANCHOR_CMD_CRT,
	XWA_HUD_ANCHOR_MFD_LEFT_FRAME,
	XWA_HUD_ANCHOR_MFD_LEFT_TITLE,
	XWA_HUD_ANCHOR_MFD_LEFT_BODY,
	XWA_HUD_ANCHOR_MFD_RIGHT_FRAME,
	XWA_HUD_ANCHOR_MFD_RIGHT_TITLE,
	XWA_HUD_ANCHOR_MFD_RIGHT_BODY,
	XWA_HUD_ANCHOR_TOP_SPEED,
	XWA_HUD_ANCHOR_TOP_THROTTLE,
	XWA_HUD_ANCHOR_TOP_CRAFT_NAME,
	XWA_HUD_ANCHOR_TOP_CLOCK,
	XWA_HUD_ANCHOR_TOP_WEAPONS,
	XWA_HUD_ANCHOR_TOP_COUNTERMEASURE,
	XWA_HUD_ANCHOR_TOP_PROVING_STATUS,
	XWA_HUD_ANCHOR_LEFT_SUBSYSTEM,
	XWA_HUD_ANCHOR_RIGHT_SUBSYSTEM,
	XWA_HUD_ANCHOR_MESSAGE_SYSTEM,
	XWA_HUD_ANCHOR_MESSAGE_FLIGHT_GROUP,
	XWA_HUD_ANCHOR_MESSAGE_READY,
	XWA_HUD_ANCHOR_NETWORK,
	XWA_HUD_ANCHOR_FILM_RECORDING,
	XWA_HUD_ANCHOR_FPS,
	XWA_HUD_ANCHOR_TARGET_EDGE_BOUNDS,
	XWA_HUD_ANCHOR_COUNT
} XwaHudAnchorId;

typedef enum XwaHudDrawPhase {
	XWA_HUD_PHASE_FRAMES = 0,
	XWA_HUD_PHASE_GAUGES,
	XWA_HUD_PHASE_RADAR,
	XWA_HUD_PHASE_AIMING,
	XWA_HUD_PHASE_WORLD_BOXES,
	XWA_HUD_PHASE_CMD_PIP,
	XWA_HUD_PHASE_CMD_MARKER,
	XWA_HUD_PHASE_PANE_GLYPHS,
	XWA_HUD_PHASE_COUNT
} XwaHudDrawPhase;

typedef enum XwaHudWidgetKind {
	XWA_HUD_WIDGET_KIND_TARGET_BOXES = 0,
	XWA_HUD_WIDGET_KIND_SPRITE_GROUP,
	XWA_HUD_WIDGET_KIND_GAUGE,
	XWA_HUD_WIDGET_KIND_RADAR_BLIPS,
	XWA_HUD_WIDGET_KIND_AIMING,
	XWA_HUD_WIDGET_KIND_PIP,
	XWA_HUD_WIDGET_KIND_PANE_GLYPHS,
} XwaHudWidgetKind;

typedef enum XwaHudAssetId {
	XWA_HUD_ASSET_NONE = 0,
	XWA_HUD_ASSET_RADAR_FRAME,
	XWA_HUD_ASSET_RADAR_SCOPE,
	XWA_HUD_ASSET_CMD_FRAME,
	XWA_HUD_ASSET_MFD_FRAME,
	XWA_HUD_ASSET_POWER,
	XWA_HUD_ASSET_CHARGE,
	XWA_HUD_ASSET_SHIELD_HULL,
	XWA_HUD_ASSET_BEAM,
	XWA_HUD_ASSET_RETICLE,
	XWA_HUD_ASSET_THREATS,
	XWA_HUD_ASSET_TARGET_ARROW,
	XWA_HUD_ASSET_COUNT
} XwaHudAssetId;

typedef enum XwaHudWidgetId {
	XWA_HUD_WIDGET_TARGET_BOXES = 0,
	XWA_HUD_WIDGET_FORE_RADAR_FRAME,
	XWA_HUD_WIDGET_AFT_RADAR_FRAME,
	XWA_HUD_WIDGET_FORE_RADAR_SCOPE,
	XWA_HUD_WIDGET_AFT_RADAR_SCOPE,
	XWA_HUD_WIDGET_CMD_FRAME,
	XWA_HUD_WIDGET_MFD_LEFT_FRAME,
	XWA_HUD_WIDGET_MFD_RIGHT_FRAME,
	XWA_HUD_WIDGET_LEFT_POWER,
	XWA_HUD_WIDGET_RIGHT_POWER,
	XWA_HUD_WIDGET_LASER_CHARGE,
	XWA_HUD_WIDGET_ION_CHARGE,
	XWA_HUD_WIDGET_SHIELD_HULL,
	XWA_HUD_WIDGET_BEAM,
	XWA_HUD_WIDGET_READINESS,
	XWA_HUD_WIDGET_FORE_RADAR_BLIPS,
	XWA_HUD_WIDGET_AFT_RADAR_BLIPS,
	XWA_HUD_WIDGET_FORE_TARGET_MARKER,
	XWA_HUD_WIDGET_AFT_TARGET_MARKER,
	XWA_HUD_WIDGET_RETICLE,
	XWA_HUD_WIDGET_THREATS,
	XWA_HUD_WIDGET_TARGET_ARROW,
	XWA_HUD_WIDGET_PADLOCK_ARROW,
	XWA_HUD_WIDGET_CMD_PIP,
	XWA_HUD_WIDGET_CMD_COMPONENT_MARKER,
	XWA_HUD_WIDGET_PANE_GLYPHS,
	XWA_HUD_WIDGET_COUNT
} XwaHudWidgetId;

typedef struct XwaHudWidgetDesc {
	uint16_t id;
	uint16_t anchor;
	uint8_t kind;
	uint8_t phase;
	uint16_t stable_order;
	uint16_t asset;
} XwaHudWidgetDesc;

typedef struct XwaRemasterHudPreparedAssets {
	uint32_t layout_generation;
	uint32_t bundle_generation;
	uint32_t requested_asset_mask;
	uint32_t resolved_asset_mask;
	uint32_t missing_asset_mask;
	uint8_t requested_font_mask;
	uint8_t resolved_font_mask;
	uint8_t missing_font_mask;
	uint8_t cache_entries;
	uint32_t render_phase_violations;
} XwaRemasterHudPreparedAssets;

void XwaRemasterHud_BuildVisibility(const XwaHudState* hud, XwaRemasterHudVisibility* out);
const XwaHudWidgetDesc* XwaRemasterHud_WidgetRegistry(uint32_t* out_count);
int XwaRemasterHud_ValidateWidgetRegistry(char* error, uint32_t error_size);
uint32_t XwaRemasterHud_BuildAssetRequests(const XwaHudState* hud, uint8_t* out_font_mask);
int XwaRemasterHud_Init(AeronVfs* vfs);
const XwaHudLayout* XwaRemasterHud_Layout(void);
void XwaRemasterHud_Shutdown(void);
/* Resolves lightweight references from the mission-resident asset set, then
 * builds transient HUD draw data. It performs no I/O, upload, or residency work. */
void XwaRemasterHud_PrepareFrame(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot,
								 XwaRemasterAssets* assets, const XwaRemasterFlightView* flight_view,
								 int target_w, int target_h);
const XwaRemasterHudPreparedAssets* XwaRemasterHud_PreparedAssets(void);
void XwaRemasterHud_BeginRenderPhase(void);
void XwaRemasterHud_EndRenderPhase(void);
const XwaAssetRef* XwaRemasterHud_AssetFrame(int object_type, int classic_frame_1based);
const AeronFontAtlas* XwaRemasterHud_FlightFont(int tier,
		float* out_atlas_scale);
const XwaHudPreparedDrawState* XwaRemasterHud_PreparedDrawState(void);
void XwaRemasterHud_RenderTargetBoxes(AeronCommandBuffer* cmd, AeronRenderPass* pass,
									  AeronRenderTarget* target, int target_w, int target_h,
									  XwaHudTargetBoxLayer layer);
void XwaRemasterHud_RenderFixed(AeronCommandBuffer* cmd, AeronRenderPass* pass,
								AeronRenderTarget* target, int target_w, int target_h);
void XwaRemasterHud_RenderCmd(AeronCommandBuffer* cmd, AeronRenderPass* pass,
							  AeronRenderTarget* target, int target_w, int target_h);
void XwaRemasterHud_RenderText(AeronCommandBuffer* cmd, AeronRenderPass* pass,
							   AeronRenderTarget* target, int target_w, int target_h);
void XwaRemasterHud_Render2D(AeronCommandBuffer* cmd, AeronRenderPass* pass,
							AeronRenderTarget* target, int target_w, int target_h);

#ifdef __cplusplus
}
#endif

#endif
