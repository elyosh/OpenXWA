#ifndef XWA_RUNTIME_INPUT_BRIDGE_H
#define XWA_RUNTIME_INPUT_BRIDGE_H

#include "aeron/input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Where flight keys go. Gameplay: through the keyboard bindings. Raw: to the
 * original DirectInput readers (frontend, datapad, pause, message typing,
 * film playback, hangar). Blocked: nowhere (no focus, input captured, debug
 * UI). A route change drops queued keys and blocks held keys until
 * release. */
typedef enum XwaKeyboardRoute {
	XWA_KEYBOARD_RAW,
	XWA_KEYBOARD_GAMEPLAY,
	XWA_KEYBOARD_BLOCKED
} XwaKeyboardRoute;

XwaKeyboardRoute XwaInput_ReconcileKeyboard(void);
/* Runs this host frame's key events through the bindings and updates the
 * controllers. Call once per host frame in flight. `suppress` blocks the
 * keyboard for the frame. */
void XwaInput_UpdateFlight(int suppress);

void XwaInputBridge_UpdateFrontendMouse(const AeronInputSnapshot* input);
void XwaInputBridge_UpdateFrontendKeyboard(const AeronInputSnapshot* input);
void XwaInputBridge_UpdateFrontend(const AeronInputSnapshot* input);

#ifdef __cplusplus
}
#endif

#endif
