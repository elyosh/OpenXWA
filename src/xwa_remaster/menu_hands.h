#ifndef XWA_REMASTER_MENU_HANDS_H
#define XWA_REMASTER_MENU_HANDS_H

/* What the player holds in the menus of VR.
 *
 * Outside the cockpit, each hand is drawn where its controller is, in the
 * eye images that the headset shows over the virtual screen, with the beam.
 * A hand is drawn as the headset's own model of the controller, with its
 * trigger, buttons and stick moving, so that the player finds the buttons
 * by looking. Few runtimes give these models (SteamVR does). Without one,
 * the hand is drawn as in the cockpit.
 *
 * The hands are in a small scene of their own, with one fixed light. */

#include "aeron/render.h"
#include "aeron/scene/view_rig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The Show Controllers in Menus option. When off, the hands are drawn as in
 * the cockpit. */
void XwaRemasterMenuHands_SetControllers(int enabled);

/* Whether a hand is to be drawn in this frame. */
int XwaRemasterMenuHands_Wanted(void);

/* Renders the hands for one view of `rig`, into the scene's own image.
 * `cmd` has no pass open. Returns 0 when there is nothing to draw. */
int XwaRemasterMenuHands_RenderView(AeronCommandBuffer* cmd, const AeronViewRig* rig, int view);
/* Draws what XwaRemasterMenuHands_RenderView rendered into the open pass on
 * `target`, an eye image, keeping it transparent around the hands. */
void XwaRemasterMenuHands_DrawView(AeronRenderPass* pass, AeronRenderTarget* target);

void XwaRemasterMenuHands_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
