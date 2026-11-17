#ifndef XWA_RUNTIME_INPUT_CONTROLLER_MAPPING_H
#define XWA_RUNTIME_INPUT_CONTROLLER_MAPPING_H

#include "xwa_runtime/input/controller_options.h"

#ifdef __cplusplus
extern "C" {
#endif

void XwaControllerMapping_Init(const XwaControllerOptions* options);
/* The options are installed by the next ApplyPending, at a host frame
 * boundary. */
void XwaControllerMapping_SetOptions(const XwaControllerOptions* options);
void XwaControllerMapping_ApplyPending(void);
void XwaControllerMapping_Update(const AeronInputSnapshot* input);
void XwaControllerMapping_Suspend(void);
void XwaControllerMapping_Shutdown(void);
const XwaControllerOptions* XwaControllerMapping_Options(void);
int XwaControllerMapping_Axis(XwaInputAxis axis);
/* Held bits: fire = 1, target/roll modifier = 2. */
uint16_t XwaControllerMapping_Modifiers(void);
uint16_t XwaControllerMapping_ReadKey(void);
/* The key of a held look binding, sent on every read like the original hat,
 * or 0. */
uint16_t XwaControllerMapping_ReadLookKey(void);
int XwaControllerMapping_RollHeld(XwaInputRoll side);
/* Returns and clears the latch set when a pause binding is pressed while
 * the game is paused. */
int XwaControllerMapping_TakeResume(void);
/* Reads the buttons that are bound to host actions (actions.h). Call it
 * once for each host frame, in flight and outside it: the other bindings
 * are only read in flight. */
void XwaControllerMapping_UpdateHostActions(const AeronInputSnapshot* input);
/* Returns 1 once for each press of a binding of the host action `action`
 * that XwaControllerMapping_UpdateHostActions saw. */
int XwaControllerMapping_TakeHostAction(XwaInputAction action);
void XwaControllerMapping_ReleaseCommands(void);
uint16_t XwaControllerMapping_ThrottlePosition(int16_t raw, AeronControllerKind kind, int source,
											   bool invert);
bool XwaControllerMapping_ThrottleSample(uint16_t* position, uint32_t* generation);
/* Whether a saved model is connected. */
int XwaControllerMapping_Present(void);
/* Returns 1 once after Present() changes. */
int XwaControllerMapping_ConsumePresenceChange(void);

/* Turns some inputs of a VR hand off for the bindings: they read as not
 * pressed. `hand` is 0 for the left hand and 1 for the right one. `buttons`
 * has one bit for each AeronVrHandButton, and `axes` one bit for each
 * AeronVrHandAxis; an axis bit covers the bindings that use the axis as a
 * button. Something else uses these inputs meanwhile, such as a pointer that
 * clicks with the trigger. An input that is still pressed when its bit is
 * cleared stays off until it is released. */
void XwaControllerMapping_SetSourceMask(int hand, uint32_t buttons, uint32_t axes);

/* Rumble goes to one device that can rumble: the VR hand that last held the
 * virtual stick, else a connected device of the first model that binds pitch, else of the first that binds
 * yaw, else of the first gamepad or VR hand model, the one with the lowest instance id. */
uint32_t XwaControllerMapping_RumbleInstance(void);
/* Returns 1 once after the rumble device changes. */
int XwaControllerMapping_ConsumeRumbleChange(void);
void XwaControllerMapping_SetRumbleEnabled(int enabled);
int XwaControllerMapping_Rumble(uint16_t low_frequency_rumble, uint16_t high_frequency_rumble,
								uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif
