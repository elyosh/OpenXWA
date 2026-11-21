#ifndef XWA_REMASTER_COCKPIT_BUTTONS_H
#define XWA_REMASTER_COCKPIT_BUTTONS_H

/* The buttons of the cockpit that a VR hand points at and presses.
 *
 * A material of a model can have buttons: rectangles in its texture
 * coordinates, each with the name of an input action (the `buttons` of its
 * AERON_flight_model extension). Each frame, a ray goes from the fingertip
 * of each hand along the controller's aim. Where it first meets the cockpit,
 * or the other hand's model, on a material that has buttons, a dot is drawn.
 * On a button, the button's label is drawn too, and pulling the trigger does
 * the button's action. */

#include "aeron/render.h"
#include "aeron/scene/scene3d.h"
#include "xwa_remaster/assets.h"
#include "xwa_remaster/hud_planes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The Cockpit Buttons option, the length of the ray in metres, and whether
 * the hand gets a short haptic tick on a button. */
void XwaRemasterCockpitButtons_SetOptions(int enabled, float ray_m, int haptics);

/* Tests the hands against the cockpit that the flight scene draws in this
 * frame, and does the press. `cockpit` is the cockpit's scene instance, and
 * `seat_rows` and `seat_pos` are the seat camera, as for
 * XwaRemasterVrHands_Instance. In a frame that draws no cockpit this isn't
 * called, and no hand is on a button. */
void XwaRemasterCockpitButtons_Update(const AeronSceneMeshInstance* cockpit, const float seat_rows[9],
									  const float seat_pos[3]);

/* Whether the hand points at a material that has buttons: the dot shows, and
 * the hand's model points. `hand` is 0 for the left hand, 1 for the right. */
int XwaRemasterCockpitButtons_OnPanel(int hand);
/* Whether the hand points at a button: its trigger then presses the button,
 * and must not reach the bindings. */
int XwaRemasterCockpitButtons_OnButton(int hand);

/* Prepares the dots and the labels on the HUD's planes, outside any pass.
 * `planes` is NULL when the HUD's markers aren't on planes: nothing is
 * drawn then. The seat camera is this frame's. */
void XwaRemasterCockpitButtons_PrepareMarkers(AeronCommandBuffer* cmd, XwaRemasterAssets* assets,
											  const XwaRemasterHudPlanes* planes, const float seat_rows[9],
											  const float seat_pos[3], int target_w, int target_h);
/* Draws them into a view's image, after the HUD's markers. `view` is as for
 * XwaRemasterHud_RenderMarkerPlanes. */
void XwaRemasterCockpitButtons_RenderMarkers(AeronCommandBuffer* cmd, AeronRenderPass* pass,
											 AeronRenderTarget* target, const float view[12]);
void XwaRemasterCockpitButtons_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
