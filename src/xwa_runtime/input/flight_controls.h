#ifndef XWA_RUNTIME_INPUT_FLIGHT_CONTROLS_H
#define XWA_RUNTIME_INPUT_FLIGHT_CONTROLS_H

#include "xwa_runtime/input/actions.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The local flight controls for FlightInput_Read(): the keyboard bindings,
 * the controllers and the mouse together. */

void XwaFlightControls_Reset(void);
/* Fills the axes and the held bits (fire = 1, target/roll modifier = 2) for
 * one local read. Returns 0, with every output neutral, while the keyboard
 * route is blocked. */
int XwaFlightControls_Sample(int* axisX, int* axisY, int* axisR, int* keyMods);
/* The next key: the keyboard first, then the controllers, then the mouse.
 * On the gameplay route the keyboard key comes from the bindings; on the raw
 * route it is the next DirectInput key, as in the original. `fromKeyboard`
 * tells whether it came from the keyboard. */
uint16_t XwaFlightControls_ReadKey(int* fromKeyboard);
/* The key of a held controller look binding, or 0. */
uint16_t XwaFlightControls_ReadLookKey(void);
/* Whether a keyboard or controller roll action is held. */
int XwaFlightControls_RollHeld(XwaInputRoll side);
/* Whether a keyboard look action is held, for the cockpit look in
 * Flight_UpdateEntity(). */
int XwaFlightControls_LookHeld(XwaInputLook look);
/* Drops every queued command and blocks held keys until release, such as
 * when the game pauses or resumes. */
void XwaFlightControls_Recover(void);

#ifdef __cplusplus
}
#endif

#endif
