#ifndef XWA_REMASTER_MENU_BEAM_H
#define XWA_REMASTER_MENU_BEAM_H

/* The beam from a VR hand to the virtual screen.
 *
 * In the menus, and in every scene where the game is shown on the headset's
 * virtual screen, the hand that points at the screen moves the mouse there.
 * The beam shows where it points: a thin line from the controller to the
 * pointer, drawn into eye images that hold nothing else and that the headset
 * shows over the screen. */

#include "aeron/render.h"
#include "aeron/scene/view_rig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Whether a beam is to be drawn in this frame: a hand points at the virtual
 * screen. */
int XwaRemasterMenuBeam_Wanted(void);

/* Clears the eye images `targets` and draws the beam into them. `rig` has
 * the eyes in the space of the view rig, in metres, without the VR world
 * scale: the hands and the screen are at their real size. Returns 0 when
 * nothing could be drawn; the eye images must then not be shown. */
int XwaRemasterMenuBeam_Render(const AeronViewRig* rig, AeronRenderTarget* const* targets);

void XwaRemasterMenuBeam_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
