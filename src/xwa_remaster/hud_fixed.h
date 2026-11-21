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
	/* Where the arrow is in the view it was placed in. The arrow's text is
	 * laid out from it. */
	float center_x_ref;
	float center_y_ref;
	/* The rectangle that the arrow is kept on, in the same view. */
	float bounds_x_ref, bounds_y_ref, bounds_w_ref, bounds_h_ref;
	int16_t rotation_angle;
	/* Where the arrow is drawn. In a headset the arrow has a plane of its own
	 * (`on_plane`, `plane`: hud_planes.h), at infinity, and is drawn at the
	 * centre of that plane's drawing. Otherwise this is the same as the
	 * position above. */
	uint8_t on_plane;
	float draw_x_ref;
	float draw_y_ref;
	float plane[9];
} XwaHudProjectedArrow;
int XwaRemasterHudFixed_ProjectTargetArrow(const XwaSnapshot* snapshot, XwaHudProfileIndex profile,
										   const XwaRemasterFlightView* flight_view, int target_w,
										   int target_h, XwaHudProjectedArrow* out);
const XwaHudProjectedArrow* XwaRemasterHudFixed_TargetArrow(void);
int XwaRemasterHudFixed_ReticleCenter(float* out_x_ref, float* out_y_ref);
/* In a headset, writes the plane of the reticle group (hud_planes.h): the
 * seat's plane, at infinity, with the reticle's size. Returns 0 when the HUD
 * isn't drawn on planes. */
int XwaRemasterHudFixed_ReticlePlane(float out[9]);
/* Pixels of an eye image for one pixel of the HUD's drawings on planes
 * (XwaRemasterHudPlanes.eye_pixel_scale), or 1 when the HUD isn't drawn on
 * planes. */
float XwaRemasterHudFixed_EyePixelScale(void);
/* Draws the marker widgets on their planes. */
void XwaRemasterHudFixed_RenderPlanes(AeronCommandBuffer* cmd, AeronRenderPass* pass,
									  AeronRenderTarget* target, const float view[12]);
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
void XwaRemasterHudFixed_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
								int target_w, int target_h);

#endif
