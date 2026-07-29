/* WinMM joystick API shim backed by the mapped Aeron controller. */

#define XWA_WINMM_COMPAT_IMPLEMENTATION
#include "xwa_runtime/compat/winmm/joystick.h"

#include "xwa_runtime/input/controller_mapping.h"

#include <string.h>

enum {
	WINMM_JOY_AXIS_RANGE = 65535,
	WINMM_JOY_BUTTON_COUNT = XWA_CONTROLLER_LOGICAL_BUTTON_COUNT,
};

static WinmmJoystickTraceSample g_winmmJoystickTraceSample;
static int g_winmmJoystickTraceValid;

uint32_t XWA_WINMMAPI joyGetNumDevs(void) {
	/* XWA consumes one explicitly selected logical controller. */
	return 1;
}

MMRESULT XWA_WINMMAPI joyGetDevCapsA(uint32_t uJoyID, JOYCAPSA* pjc, uint32_t cbjc) {
	const AeronControllerSnapshot* controller;
	XwaControllerLogicalState state;

	if (!pjc || cbjc < sizeof(*pjc)) {
		return JOYERR_PARMS;
	}
	controller = XwaControllerMapping_SelectedController();
	if (uJoyID != 0 || !controller) {
		return JOYERR_UNPLUGGED;
	}
	XwaControllerMapping_GetState(&state);
	memset(pjc, 0, sizeof(*pjc));
	strncpy(pjc->szPname, controller->name, sizeof(pjc->szPname) - 1);
	pjc->wXmax = WINMM_JOY_AXIS_RANGE;
	pjc->wYmax = WINMM_JOY_AXIS_RANGE;
	pjc->wZmax = WINMM_JOY_AXIS_RANGE;
	pjc->wRmax = WINMM_JOY_AXIS_RANGE;
	pjc->wNumButtons = WINMM_JOY_BUTTON_COUNT;
	pjc->wMaxButtons = WINMM_JOY_BUTTON_COUNT;
	pjc->wNumAxes = XWA_CONTROLLER_LOGICAL_AXIS_COUNT;
	pjc->wMaxAxes = XWA_CONTROLLER_LOGICAL_AXIS_COUNT;
	pjc->wCaps = JOYCAPS_HASZ | JOYCAPS_HASR;
	if (state.has_pov) {
		pjc->wCaps |= JOYCAPS_HASPOV;
	}
	return JOYERR_NOERROR;
}

MMRESULT XWA_WINMMAPI joyGetPosEx(uint32_t uJoyID, JOYINFOEX* pji) {
	const AeronControllerSnapshot* controller;
	XwaControllerLogicalState state;
	WinmmJoystickTraceSample trace;
	uint32_t buttons;

	if (!pji) {
		return JOYERR_PARMS;
	}
	controller = XwaControllerMapping_SelectedController();
	if (uJoyID != 0 || !controller || !XwaControllerMapping_GetState(&state)) {
		return JOYERR_UNPLUGGED;
	}
	pji->dwXpos = state.axes[XWA_CONTROLLER_AXIS_YAW];
	pji->dwYpos = state.axes[XWA_CONTROLLER_AXIS_PITCH];
	pji->dwZpos = state.axes[XWA_CONTROLLER_AXIS_THROTTLE];
	pji->dwRpos = state.axes[XWA_CONTROLLER_AXIS_ROLL];
	pji->dwUpos = 0;
	pji->dwVpos = 0;
	buttons = state.buttons;
	pji->dwButtons = buttons;
	pji->dwButtonNumber = 0;
	while (pji->dwButtonNumber < WINMM_JOY_BUTTON_COUNT && !(buttons & (1u << pji->dwButtonNumber))) {
		++pji->dwButtonNumber;
	}
	pji->dwButtonNumber = pji->dwButtonNumber < WINMM_JOY_BUTTON_COUNT ? pji->dwButtonNumber + 1 : 0;
	pji->dwPOV = state.pov_direction < 0 ? JOY_POVCENTERED : (uint32_t)state.pov_direction * 9000u;
	pji->dwReserved1 = 0;
	pji->dwReserved2 = 0;

	memset(&trace, 0, sizeof(trace));
	trace.deviceId = controller->instance_id;
	trace.sourceAxisX = state.source_axes[XWA_CONTROLLER_AXIS_YAW];
	trace.sourceAxisY = state.source_axes[XWA_CONTROLLER_AXIS_PITCH];
	trace.sourceAxisR = state.source_axes[XWA_CONTROLLER_AXIS_ROLL];
	trace.sourceValueX = state.source_axis_values[XWA_CONTROLLER_AXIS_YAW];
	trace.sourceValueY = state.source_axis_values[XWA_CONTROLLER_AXIS_PITCH];
	trace.sourceValueR = state.source_axis_values[XWA_CONTROLLER_AXIS_ROLL];
	trace.winmmX = pji->dwXpos;
	trace.winmmY = pji->dwYpos;
	trace.winmmR = pji->dwRpos;
	g_winmmJoystickTraceSample = trace;
	g_winmmJoystickTraceValid = 1;
	return JOYERR_NOERROR;
}

int WinmmJoystick_GetLastTraceSample(WinmmJoystickTraceSample* sample) {
	if (!sample || !g_winmmJoystickTraceValid) {
		return 0;
	}
	*sample = g_winmmJoystickTraceSample;
	return 1;
}

void WinmmJoystick_ResetTrace(void) {
	memset(&g_winmmJoystickTraceSample, 0, sizeof(g_winmmJoystickTraceSample));
	g_winmmJoystickTraceValid = 0;
}
