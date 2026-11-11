#include "xwa_runtime/input/flight_controls.h"

#include "xwa/flight/flight.h"
#include "xwa/flight/player/player.h"
#include "xwa_runtime/input/capture.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/input/input_bridge.h"
#include "xwa_runtime/input/keyboard_mapping.h"
#include "xwa_runtime/input/mouse_flight.h"

void XwaFlightControls_Reset(void) {}

int XwaFlightControls_Sample(int* axisX, int* axisY, int* axisR, int* keyMods) {
	const XwaKeyboardRoute keyboard = XwaInput_ReconcileKeyboard();

	*axisX = *axisY = *axisR = *keyMods = 0;
	if (keyboard == XWA_KEYBOARD_BLOCKED) {
		XwaFlightControls_Reset();
		return 0;
	}
	*axisX = XwaControllerMapping_Axis(XWA_INPUT_AXIS_YAW);
	*axisY = XwaControllerMapping_Axis(XWA_INPUT_AXIS_PITCH);
	*axisR = XwaControllerMapping_Axis(XWA_INPUT_AXIS_ROLL);
	*keyMods = XwaControllerMapping_Modifiers();
	if (keyboard == XWA_KEYBOARD_GAMEPLAY) {
		*keyMods |= XwaKeyboardMapping_ReadButtons();
	}
	/* The mouse replaces any axis it moves. While cockpit mouse look is on,
	 * mouse flight is suspended. */
	if (g_padlockMouseLookEnabled) {
		XwaMouseFlight_Suspend();
	} else if (XwaMouseFlight_Sample()) {
		int mouseAxisX;
		int mouseAxisY;
		int mouseAxisR;

		XwaMouseFlight_GetAxes(&mouseAxisX, &mouseAxisY, &mouseAxisR);
		if (mouseAxisX != 0) {
			*axisX = mouseAxisX;
		}
		if (mouseAxisY != 0) {
			*axisY = mouseAxisY;
		}
		if (mouseAxisR != 0) {
			*axisR = mouseAxisR;
		}
		*keyMods |= XwaMouseFlight_ButtonsMask();
	}
	return 1;
}

uint16_t XwaFlightControls_ReadKey(int* fromKeyboard) {
	const XwaKeyboardRoute keyboard = XwaInput_ReconcileKeyboard();
	uint16_t key = 0;

	*fromKeyboard = 0;
	if (keyboard == XWA_KEYBOARD_BLOCKED) {
		return 0;
	}
	if (keyboard == XWA_KEYBOARD_GAMEPLAY) {
		key = XwaKeyboardMapping_ReadKey();
	} else if (FlightInput_HasKeyReady()) {
		key = (uint16_t)FlightInput_GetNextKey();
	}
	if (key) {
		*fromKeyboard = 1;
		return key;
	}
	key = XwaControllerMapping_ReadKey();
	if (!key) {
		key = XwaMouseFlight_ReadKey();
	}
	return key;
}

uint16_t XwaFlightControls_ReadLookKey(void) {
	return XwaInput_ReconcileKeyboard() == XWA_KEYBOARD_BLOCKED ? 0 : XwaControllerMapping_ReadLookKey();
}

int XwaFlightControls_RollHeld(XwaInputRoll side) {
	const XwaKeyboardRoute keyboard = XwaInput_ReconcileKeyboard();

	if (keyboard == XWA_KEYBOARD_BLOCKED) {
		return 0;
	}
	return (keyboard == XWA_KEYBOARD_GAMEPLAY && XwaKeyboardMapping_RollHeld(side)) ||
		   XwaControllerMapping_RollHeld(side);
}

int XwaFlightControls_LookHeld(XwaInputLook look) {
	return XwaInput_ReconcileKeyboard() == XWA_KEYBOARD_GAMEPLAY && XwaKeyboardMapping_LookHeld(look);
}

void XwaFlightControls_Recover(void) {
	XwaInput_FlushKeyboard();
	XwaControllerMapping_ReleaseCommands();
	XwaMouseFlight_DiscardPending();
	g_actionKey = 0;
}
