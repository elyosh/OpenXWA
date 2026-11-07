#ifndef XWA_REMASTER_SPEED_EFFECT_H
#define XWA_REMASTER_SPEED_EFFECT_H

#include "aeron/scene/scene3d.h"
#include "xwa_remaster/effects.h"
#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The speed effect: small streaks of dust around the camera that show how
 * fast the player's craft moves (modern_speed_effect_options.h). The dust
 * is fixed in the world and repeats every 165 m on each axis, so the camera
 * always sits in a box of dust and its own movement moves the dust. The
 * dust is 3.3 times bigger and further away than it looks, so it crosses
 * the screen 3.3 times slower. */
typedef struct XwaRemasterSpeedEffect XwaRemasterSpeedEffect;

XwaRemasterSpeedEffect* XwaRemasterSpeedEffect_Create(void);
void XwaRemasterSpeedEffect_Destroy(XwaRemasterSpeedEffect* effect);

/* Queues the streaks as OVERLAY billboards of `scene`, when the effect is on
 * and the camera views the player's craft outside the hangar. */
void XwaRemasterSpeedEffect_Submit(const XwaRemasterSpeedEffect* effect, AeronScene3D* scene,
								   const XwaSnapshot* snapshot, const XwaRemasterEffectView* view);

#ifdef __cplusplus
}
#endif

#endif
