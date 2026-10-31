#ifndef XWA_RUNTIME_CAMERA_CAMERA_MOTION_H
#define XWA_RUNTIME_CAMERA_CAMERA_MOTION_H

/* The lag of the enhanced camera. A slow copy of the player's craft follows
 * the craft with a delay. The difference between the two tells how far the
 * craft has turned and sped up lately. The exterior camera and the cockpit
 * use it to trail the craft.
 *
 * A mod sets the camera in the `camera` section of flight/flight.yaml:
 *
 *   camera:
 *     exterior_tilt: 10.0
 *     cockpit_sway: false
 *
 * The keys are those of the `camera` section of remaster/config.yaml. Every
 * key is optional, and a missing key keeps the shipped value. A mod that has
 * the section turns the feature on, unless the section has `enabled: false`.
 * The player's config.yaml still wins over the mod. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the `camera` section of flight/flight.yaml. Called once at
   startup. */
void XwaCameraMotion_Init(void);

/* Whether the feature is on. */
int XwaCameraMotion_Enabled(void);

/* Updates the lag. Called at the start of each camera update. It only
   follows the local player, and does nothing for the other players. */
void XwaCameraMotion_Update(int playerIdx);

/* The exterior camera's extra pitch and yaw, in Q16 units, for
   FVIEW_BuildCameraOrient. They turn the camera toward where the craft was
   heading a moment ago, and tilt it to look at the craft from above. They
   are zero when they don't apply, such as in the cockpit view, for another
   craft, or in a Death Star mission. */
void XwaCameraMotion_ExteriorOffsets(int playerIdx, int16_t* pitchQ16, int16_t* yawQ16);

/* How far the local player's eye has moved from the seat, in world axes and
   world units. The eye moves toward turns, and back while the craft speeds
   up. The cockpit and the things drawn with it move the other way. It is
   zero when it doesn't apply, such as in the exterior view. */
void XwaCameraMotion_CockpitSway(float out[3]);

#ifdef __cplusplus
}
#endif

#endif
