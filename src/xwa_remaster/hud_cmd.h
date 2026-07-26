#ifndef XWA_REMASTER_HUD_CMD_H
#define XWA_REMASTER_HUD_CMD_H

#include "aeron/render.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"
#include "xwa_remaster/hud.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaHudCmdPreparedState {
	uint8_t valid;
	uint8_t marker_visible;
	uint8_t profile;
	uint8_t reserved;
	uint16_t target_slot;
	uint16_t target_signature;
	uint16_t internal_w;
	uint16_t internal_h;
	uint16_t classic_viewport_w;
	uint16_t classic_viewport_h;
	float rect_x_ref;
	float rect_y_ref;
	float rect_w_ref;
	float rect_h_ref;
	float marker_x;
	float marker_y;
} XwaHudCmdPreparedState;

int XwaRemasterHudCmd_Init(void);
void XwaRemasterHudCmd_Shutdown(void);
void XwaRemasterHudCmd_Prepare(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot,
							   XwaRemasterAssets* assets, XwaHudProfileIndex profile,
							   int target_w, int target_h);
void XwaRemasterHudCmd_PrepareDrawList(AeronCommandBuffer* cmd, int target_w, int target_h);
void XwaRemasterHudCmd_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass,
							  AeronRenderTarget* target, int target_w, int target_h);
const XwaHudCmdPreparedState* XwaRemasterHudCmd_Prepared(void);

#ifdef __cplusplus
}
#endif

#endif
