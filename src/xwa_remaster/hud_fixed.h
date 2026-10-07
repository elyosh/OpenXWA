#ifndef XWA_REMASTER_HUD_FIXED_H
#define XWA_REMASTER_HUD_FIXED_H

#include "xwa_remaster/hud.h"

#include "aeron/scene/draw_list2d.h"

int XwaRemasterHudFixed_Init(void);
void XwaRemasterHudFixed_Shutdown(void);
void XwaRemasterHudFixed_Build(const XwaSnapshot* snapshot, XwaHudProfileIndex profile,
							   uint32_t bundle_generation, const XwaRemasterFlightView* flight_view,
							   int target_w, int target_h);
void XwaRemasterHudFixed_PrepareDrawList(AeronCommandBuffer* cmd, int target_w, int target_h);
typedef struct XwaHudProjectedArrow {
	uint8_t visible;
	uint8_t behind;
	uint8_t padlock;
	uint8_t reserved;
	float center_x_ref;
	float center_y_ref;
	int16_t rotation_angle;
} XwaHudProjectedArrow;
int XwaRemasterHudFixed_ProjectTargetArrow(const XwaSnapshot* snapshot, XwaHudProfileIndex profile,
										   const XwaRemasterFlightView* flight_view, int target_w,
										   int target_h, XwaHudProjectedArrow* out);
const XwaHudProjectedArrow* XwaRemasterHudFixed_TargetArrow(void);
int XwaRemasterHudFixed_ReticleCenter(float* out_x_ref, float* out_y_ref);
/* The quad (x, y, w, h in target pixels) a record of
 * XwaRemasterHud_PreparedDrawState draws, as its sprite is placed; 0 when
 * the record draws nothing. */
int XwaRemasterHudFixed_RecordQuad(const XwaHudDrawRecord* record, int target_w, int target_h, float quad[4]);
/* Dynamic Cockpit state, set before the list is prepared: while active,
 * the panel images (box borders the cockpit displays don't show) are drawn
 * but not captured, those of the `erased_regions` (dynamic_cockpit.c
 * region bits) not at all, and the other records go through
 * XwaRemasterDc_AddSprite / AddQuad4. */
void XwaRemasterHudFixed_SetDynamicCockpit(int active, uint32_t erased_regions);
void XwaRemasterHudFixed_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass,
								AeronRenderTarget* target, int target_w, int target_h);

#endif
