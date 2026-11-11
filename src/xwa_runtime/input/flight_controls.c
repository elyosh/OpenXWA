#include "xwa_runtime/input/flight_controls.h"

#include "aeron/aeron.h"
#include "aeron/debug.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/input/capture.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/input/input_bridge.h"
#include "xwa_runtime/input/keyboard_mapping.h"
#include "xwa_runtime/input/mouse_flight.h"
#include "xwa_runtime/runtime/flight_pause_task.h"
#include "xwa_runtime/runtime/flight_task.h"
#include "xwa_runtime/runtime/movie_task.h"

#include <stdbool.h>

static struct {
	bool valid;
	uint16_t position;
	uint32_t generation, signature;
	int object;
} g_throttleBaseline;

static void XwaFlightControls_ResetThrottle(void) { g_throttleBaseline.valid = false; }

void XwaFlightControls_Reset(void) { XwaFlightControls_ResetThrottle(); }

int XwaFlightControls_ThrottleEligible(unsigned int player) {
	const PlayerData* data;
	const ObjectRecord* object;

	if (player >= 8) {
		return 0;
	}
	data = &g_players[player];
	/* hyperspacePhase: Flight_ProcessPlayerActions() returns early.
	   mapCameraState: the throttle keys are ignored in map mode.
	   hasCheckpointFlag: every throttle key checks it.
	   msgTypeId: actions are skipped while typing. */
	if (!data->connectedFlag || data->hyperspacePhase || data->mapCameraState || data->hasCheckpointFlag ||
		data->msgTypeId || data->regionSessionId) {
		return 0;
	}
	if (!g_objectTable || data->objectIndex < 0 || data->objectIndex >= 0xffff) {
		return 0;
	}
	object = &g_objectTable[data->objectIndex];
	return object->objectSignature == data->boundObjectSignature && object->mobj && object->mobj->pCraft;
}

static bool XwaFlightControls_LocalThrottleEligible(void) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	return input && input->has_focus && !XwaInput_IsCaptured() && !Aeron_DebugUiVisible() &&
		   XwaInput_ReconcileKeyboard() != XWA_KEYBOARD_BLOCKED && XwaFlightTask_IsActive() &&
		   !FlightLoading_AreFrontendSurfacesAttached() && !XwaFlightTask_IsHoldingLoadingScreen() &&
		   !XwaFlightPauseTask_IsActive() && !FlightDisplay_IsFrontendModalActive() &&
		   !XwaMovieTask_IsActive() && !g_filmPlaybackMode &&
		   XwaFlightControls_ThrottleEligible((unsigned int)g_localPlayer);
}

void XwaFlightControls_UpdateThrottleContext(void) {
	if (!XwaFlightControls_LocalThrottleEligible())
		XwaFlightControls_ResetThrottle();
}

void XwaFlightControls_SampleThrottle(FlightInputFrameRecord* record) {
	uint16_t position;
	uint32_t generation;
	record->keyMods &= (uint8_t)~XWA_INPUT_THROTTLE_PRESENT;
	record->throttle = 0;
	bool pause = record->key == KEY_ALT_P && g_flightPlayerCount == 1;
	if (pause || !XwaFlightControls_LocalThrottleEligible() ||
		!XwaControllerMapping_ThrottleSample(&position, &generation)) {
		XwaFlightControls_ResetThrottle();
		return;
	}
	int object = g_players[g_localPlayer].objectIndex;
	uint32_t signature = g_players[g_localPlayer].boundObjectSignature;
	if (!g_throttleBaseline.valid || g_throttleBaseline.generation != generation ||
		g_throttleBaseline.object != object || g_throttleBaseline.signature != signature) {
		g_throttleBaseline.valid = true;
		g_throttleBaseline.position = position;
		g_throttleBaseline.generation = generation;
		g_throttleBaseline.object = object;
		g_throttleBaseline.signature = signature;
		return;
	}
	int delta = (int)position - g_throttleBaseline.position;
	if (delta < 0)
		delta = -delta;
	/* 0.1% jitter threshold; small movements accumulate against the last accepted position. */
	if (delta && (delta >= 66 || position == 0 || position == UINT16_MAX)) {
		g_throttleBaseline.position = position;
		record->keyMods |= XWA_INPUT_THROTTLE_PRESENT;
		record->throttle = position;
	}
}

void XwaFlightControls_ApplyThrottle(unsigned int player, const FlightInputFrameRecord* record) {
	if ((record->keyMods & XWA_INPUT_THROTTLE_PRESENT) && XwaFlightControls_ThrottleEligible(player))
		g_objectTable[g_players[player].objectIndex].mobj->pCraft->throttleSpeed = record->throttle;
}

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
	XwaFlightControls_ResetThrottle();
}
