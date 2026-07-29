#ifndef XWA_RUNTIME_MOUSE_FLIGHT_H
#define XWA_RUNTIME_MOUSE_FLIGHT_H

#include "xwa_runtime/config/modern_input_options.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Modern mouse flight control: converts captured mouse motion into virtual
 * joystick axes and buttons, merged into the recovered joystick path by
 * FlightInput_Read. Position mode holds the deflection where the mouse put
 * it; rate mode maps per-frame mouse velocity to deflection. */

void XwaMouseFlight_SetOptions(const XwaModernInputOptions* options);

/* Call while another consumer owns the captured pointer (cockpit mouse
 * look): drops pending motion and recenters the virtual stick. */
void XwaMouseFlight_Suspend(void);

/* Accumulates the current host frame's mouse deltas. Must run every host
 * frame: the snapshot deltas reset each frame and the flight loop may
 * sample slower than the host rate. Idempotent per Aeron input frame. */
void XwaMouseFlight_Pump(void);

/* Converts the motion accumulated since the previous call into the current
 * virtual stick deflection. Returns nonzero while the mouse is actively
 * steering (option on, window focused, pointer captured). */
int XwaMouseFlight_Sample(void);

/* This frame's virtual stick deflection, each clamped to [-127, 127].
 * While the right button is held past the tap window, mouse X drives the
 * roll axis and yaw/pitch are suppressed. */
void XwaMouseFlight_GetAxes(int* axisX, int* axisY, int* axisR);

/* Held mouse buttons as virtual joystick button bits (bit 0 = left, bit 2 =
 * middle, bit 3 = X1) for the g_gameConfig.joyButtons bindings. The right
 * button is the roll-lock/tap modifier and is absent from this mask. */
int XwaMouseFlight_ButtonsMask(void);

/* Consumes a pending right-button tap (pressed and released within the
 * joystick button 2 tap window). The caller emits the target-in-sight
 * action key for it. */
int XwaMouseFlight_TakeTargetTap(void);

/* HUD virtual-stick marker: nonzero when it should be shown (position mode,
 * actively steering); fills the held deflection, each in [-127, 127]. */
int XwaMouseFlight_GetHudMarker(int* deflectionX, int* deflectionY);

#ifdef __cplusplus
}
#endif

#endif
