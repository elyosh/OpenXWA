#include "xwa_runtime/input/capture.h"

#include "aeron/aeron.h"
#include "aeron/compat/dinput.h"
#include "aeron/compat/host.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/input/dinput.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/input/flight_controls.h"
#include "xwa_runtime/input/keyboard_mapping.h"
#include "xwa_runtime/input/mouse_flight.h"
#include <string.h>

static bool g_captured;
static uint8_t g_blockedKeys[AERON_KEY_COUNT];
static uint32_t g_blockedMouse;

void XwaInput_FlushRawKeyboard(void) {
	if (g_dinputKeyboardDevice) {
		uint32_t count = UINT32_MAX;
		g_dinputKeyboardDevice->lpVtbl->GetDeviceData(g_dinputKeyboardDevice, sizeof(DIDEVICEOBJECTDATA),
													  NULL, &count, 0);
	}
	g_dinputShiftDown = g_dinputCtrlDown = g_dinputAltDown = 0;
	g_keyReady = 0;
	g_lastKeyCode = 0;
	Keyboard_FlushCharBuffer();
}

static void XwaInput_ApplyKeySuppression(void) {
	for (int key = 0; key < AERON_KEY_COUNT; ++key)
		AeronCompat_SetKeySuppressed(key, g_captured || g_blockedKeys[key]);
}

void XwaInput_SuppressKey(int key) {
	if ((unsigned)key >= AERON_KEY_COUNT)
		return;
	g_blockedKeys[key] = 1;
	XwaInput_ApplyKeySuppression();
}

void XwaInput_BlockHeldKeys(void) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	if (input)
		for (int key = 0; key < AERON_KEY_COUNT; ++key)
			g_blockedKeys[key] |= input->key_down[key];
	XwaInput_ApplyKeySuppression();
}

void XwaInput_FlushKeyboard(void) {
	XwaKeyboardMapping_Suspend();
	XwaInput_BlockHeldKeys();
	XwaInput_FlushRawKeyboard();
}

void XwaInput_SetCaptured(bool capture) {
	if (capture == g_captured)
		return;
	g_captured = capture;
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	if (input) {
		for (int key = 0; key < AERON_KEY_COUNT; ++key)
			g_blockedKeys[key] = input->key_down[key];
		g_blockedMouse = input->mouse.buttons;
	}
	XwaInput_FlushKeyboard();
	g_actionKey = 0;
	g_ctrlAxisX = g_ctrlAxisY = g_ctrlAxisR = 0;
	g_keyMods = 0;
	XwaFlightControls_Reset();
	XwaMouseFlight_Suspend();
	if (capture) {
		Aeron_SetRelativeMouseMode(0);
		XwaControllerMapping_Suspend();
	}
	XwaInput_ApplyKeySuppression();
}

void XwaInput_BeginCaptureFrame(const AeronInputSnapshot* input, bool capture) {
	if (input) {
		for (int key = 0; key < AERON_KEY_COUNT; ++key)
			if (!input->key_down[key] && !input->key_released[key])
				g_blockedKeys[key] = 0;
		g_blockedMouse &= input->mouse.buttons | input->mouse.released_buttons;
	}
	XwaInput_SetCaptured(capture);
	XwaInput_ApplyKeySuppression();
	if (capture)
		XwaInput_FlushKeyboard();
}

bool XwaInput_IsCaptured(void) { return g_captured; }

uint32_t XwaInput_FilterMouseButtons(uint32_t buttons) { return g_captured ? 0 : buttons & ~g_blockedMouse; }

void XwaInput_ResetCapture(void) {
	g_captured = false;
	g_blockedMouse = 0;
	memset(g_blockedKeys, 0, sizeof g_blockedKeys);
	XwaInput_ApplyKeySuppression();
	XwaMouseFlight_Suspend();
}
