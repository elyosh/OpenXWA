#ifndef XWA_RUNTIME_INPUT_FLIGHT_CONTROLS_H
#define XWA_RUNTIME_INPUT_FLIGHT_CONTROLS_H

#include "xwa/flight/flight.h"
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

/* Continuous throttle. The controller throttle is an absolute position that
 * travels in the input record as a one-time command. A lever must move before
 * it takes control, so a lever at rest never undoes keyboard throttle
 * changes. */
/* Clears the movement baseline while the local player can't use the throttle.
 * Call once per host frame, after XwaControllerMapping_Update(). */
void XwaFlightControls_UpdateThrottleContext(void);
/* Whether the throttle position of a record applies to this player now: the
 * conditions under which the original throttle keys work. */
int XwaFlightControls_ThrottleEligible(unsigned int player);
/* Adds a throttle command to the local record when the lever moved. */
void XwaFlightControls_SampleThrottle(FlightInputFrameRecord* record);
void XwaFlightControls_ApplyThrottle(unsigned int player, const FlightInputFrameRecord* record);
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
