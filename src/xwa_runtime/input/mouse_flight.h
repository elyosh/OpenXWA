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

/* The held fire bit (1) while the left button is down. The right button is
 * the roll-lock/tap modifier. */
int XwaMouseFlight_ButtonsMask(void);

/* The next mouse command: a right-button tap (pressed and released within
 * the joystick button 2 tap window) targets the craft in sights, the middle
 * button targets the nearest fighter and the first side button toggles the
 * padlock view. None while the player types a message. */
uint16_t XwaMouseFlight_ReadKey(void);

/* Drops pending motion and commands. */
void XwaMouseFlight_DiscardPending(void);

/* HUD virtual-stick marker: nonzero when it should be shown (position mode,
 * actively steering); fills the held deflection, each in [-127, 127]. */
int XwaMouseFlight_GetHudMarker(int* deflectionX, int* deflectionY);

#ifdef __cplusplus
}
#endif

#endif
