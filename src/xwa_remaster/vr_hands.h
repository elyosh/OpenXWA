#ifndef XWA_REMASTER_VR_HANDS_H
#define XWA_REMASTER_VR_HANDS_H

/* The gloves that the flight scene draws where the player's VR controllers
 * are. Their models don't belong to the simulation: they are loaded here,
 * from the mod (flight/models) or else from the game's own resources
 * (assets/models), and a model that can't be loaded is only logged. */

#include "aeron/render.h"
#include "aeron/scene/mesh.h"
#include "aeron/scene/scene3d.h"
#include "aeron/xr.h"
#include "xwa_runtime/models/vr_hands.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Gives the hands of this frame, in the space of the view rig, and the
 * scale of the player in the game's world (the VR world scale). NULL means
 * no hands. Call it once for each frame. */
void XwaRemasterVrHands_SetHands(const AeronXrHand hands[2], float world_scale);
/* Whether the gloves are drawn at all (the Show Hands option). */
void XwaRemasterVrHands_SetEnabled(int enabled);

/* Nonzero when a hand is to be drawn and its models aren't loaded yet. */
int XwaRemasterVrHands_NeedSync(void);
/* Loads the models of the hands to draw. */
void XwaRemasterVrHands_Sync(AeronCommandBuffer* cmd);
void XwaRemasterVrHands_Shutdown(void);

/* Fills the scene instance of a hand's glove for this frame. `seat_rows` and
 * `seat_pos` are the seat camera's world-to-eye rows and its position in
 * scene units: the origin of the view rig. Returns 0 when the hand isn't
 * drawn: it is off, it has no model, or it has lain still for a while, as a
 * controller left on a desk does. The caller sets the instance's culling
 * and shadow flags. */
int XwaRemasterVrHands_Instance(int hand, const float seat_rows[9], const float seat_pos[3],
								AeronSceneMeshInstance* out);

#ifdef __cplusplus
}
#endif

#endif
