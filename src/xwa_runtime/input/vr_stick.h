#ifndef XWA_RUNTIME_INPUT_VR_STICK_H
#define XWA_RUNTIME_INPUT_VR_STICK_H

#include "aeron/input.h"
#include "aeron/scene/view_rig.h"
#include "xwa_runtime/config/modern_input_options.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The virtual stick and throttle of the VR controllers. While the player
 * holds the squeeze of the stick hand, the hand's movement since the press
 * steers the craft, as a stick would. The squeeze of the other hand does the
 * same for the throttle: the hand moves forward and back.
 *
 * Like mouse flight, this is a computed source that XwaFlightControls reads.
 * It isn't a device axis, so it has no binding, and its values are in the
 * input record like any stick's. */

void XwaVrStick_SetOptions(const XwaModernInputOptions* options);

/* Reads the hands. Call it once for each host frame, before the bindings are
 * read. Outside flight, and while the game is paused or the options window
 * is open, no hand holds anything. */
void XwaVrStick_Update(const AeronInputSnapshot* input);

/* Whether this hand holds the virtual stick or the virtual throttle now.
 * `hand` is 0 for the left hand and 1 for the right one. */
int XwaVrStick_Held(int hand);

/* Adds the inputs of this hand that the virtual stick uses to the masks of
 * XwaControllerMapping_SetSourceMask: the squeeze while the virtual stick or
 * throttle is on, and the trigger while the hand holds the stick. */
void XwaVrStick_SourceMask(int hand, uint32_t* buttons, uint32_t* axes);

/* The stick's deflection, each axis in [-127, 127]. Returns 0, with the axes
 * at 0, when the stick isn't held and has returned to the centre. */
int XwaVrStick_Sample(int* axisX, int* axisY, int* axisR);

/* The fire bit (1) while the hand that holds the stick pulls its trigger. */
int XwaVrStick_ButtonsMask(void);

/* The throttle position while a hand holds the virtual throttle, and a
 * number that changes at each grab. `craft_throttle` is the craft's throttle
 * now: the first call of a grab keeps it as the position at the press.
 * Returns 0 when no hand holds the throttle. */
int XwaVrStick_ThrottleSample(uint16_t craft_throttle, uint16_t* position, uint32_t* generation);

/* The hand that last held the virtual stick, while the virtual stick is on:
 * the game's rumble goes to it. -1 when there is none. */
int XwaVrStick_RumbleHand(void);

/* How far the stick is pushed for a hand that was at `reference` when it
 * grabbed the stick and is at `now`: yaw, pitch and roll, each from -1 to 1,
 * with the dead zone applied. `mode` is XWA_MODERN_VR_STICK_TILT or
 * XWA_MODERN_VR_STICK_SLIDE, and `range` multiplies the travel that gives
 * the full deflection. */
void XwaVrStick_Deflection(XwaModernVrStickMode mode, float range, const AeronViewPose* reference,
						   const AeronViewPose* now, float out[3]);

#ifdef __cplusplus
}
#endif

#endif
