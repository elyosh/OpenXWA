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
void XwaControllerMapping_ReleaseCommands(void);
uint16_t XwaControllerMapping_ThrottlePosition(int16_t raw, AeronControllerKind kind, int source,
											   bool invert);
bool XwaControllerMapping_ThrottleSample(uint16_t* position, uint32_t* generation);
/* Whether a saved model is connected. */
int XwaControllerMapping_Present(void);
/* Returns 1 once after Present() changes. */
int XwaControllerMapping_ConsumePresenceChange(void);

/* Rumble goes to one device that can rumble: a connected device of the first
 * model that binds pitch, else of the first that binds yaw, else of the first
 * gamepad model, the one with the lowest instance id. */
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
