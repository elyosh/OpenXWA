#include "xwa_runtime/input/input_bridge.h"

#include "aeron/aeron.h"
#include "aeron/compat/host.h"
#include "aeron/debug.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/player/player.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/input/capture.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/input/flight_controls.h"
#include "xwa_runtime/input/keyboard_mapping.h"
#include "xwa_runtime/runtime/flight_pause_task.h"
#include "xwa_runtime/runtime/flight_task.h"
#include "xwa_runtime/runtime/movie_task.h"
#include "xwa_runtime/runtime/presentation.h"

#include <stdbool.h>
#include <string.h>

enum {
	XWA_KEY_DOWN = 0x80,
	XWA_KEY_BACKSPACE = 0x08,
	XWA_KEY_TAB = 0x09,
	XWA_KEY_RETURN = 0x0d,
	XWA_KEY_ESCAPE = 0x1b,
};

// GLOBAL: XWA 0x9F6882
unsigned char g_mouseLeftDown = 0;
// GLOBAL: XWA 0x9F6883
unsigned char g_mouseRightDown = 0;

static XwaKeyboardRoute g_keyboardRoute;
static bool g_keyboardSuppressed;
static uint64_t g_keyboardFrame = UINT64_MAX;

XwaKeyboardRoute XwaInput_ReconcileKeyboard(void) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	XwaKeyboardRoute route = XWA_KEYBOARD_RAW;

	if (g_keyboardSuppressed || !input || !input->has_focus || XwaInput_IsCaptured() ||
		Aeron_DebugUiVisible()) {
		route = XWA_KEYBOARD_BLOCKED;
	} else if (XwaFlightTask_IsActive() && !FlightLoading_AreFrontendSurfacesAttached() &&
			   !XwaFlightTask_IsHoldingLoadingScreen() && !FlightDisplay_IsFrontendModalActive() &&
			   !XwaMovieTask_IsActive() && !XwaFlightPauseTask_IsActive() && !g_inHangarReady &&
			   (unsigned)g_localPlayer < 8 && g_players[g_localPlayer].msgTypeId == 0 &&
			   !g_filmPlaybackMode) {
		route = XWA_KEYBOARD_GAMEPLAY;
	}
	if (route != g_keyboardRoute) {
		/* Commands and text never cross a routing transition. Held keys must be released. */
		XwaInput_FlushKeyboard();
		g_keyboardRoute = route;
	}
	XwaKeyboardMapping_Enable(route == XWA_KEYBOARD_GAMEPLAY, input);
	return route;
}

static void XwaInput_UpdateKeyboard(bool suppress) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	XwaKeyboardRoute route;

	g_keyboardSuppressed = suppress;
	route = XwaInput_ReconcileKeyboard();
	if (!input || input->frame_id == g_keyboardFrame) {
		return;
	}
	g_keyboardFrame = input->frame_id;
	XwaKeyboardMapping_BeginFrame(input);
	if (route == XWA_KEYBOARD_GAMEPLAY) {
		for (uint16_t i = 0; !input->key_events_overflow && i < input->key_event_count; ++i) {
			const AeronKeyEvent* event = &input->key_events[i];
			XwaKeyboardMapping_Event(event, AeronCompat_IsKeySuppressed(event->chord.key) != 0);
		}
		/* DirectInput still serves raw consumers, but owns no gameplay backlog. */
		XwaInput_FlushRawKeyboard();
	} else if (route == XWA_KEYBOARD_BLOCKED) {
		XwaInput_FlushRawKeyboard();
	}
}

void XwaInput_UpdateFlight(int suppress) {
	XwaInput_UpdateKeyboard(suppress != 0);
	XwaControllerMapping_Update(Aeron_InputSnapshot());
	XwaFlightControls_UpdateThrottleContext();
}

void XwaInputBridge_UpdateFrontendMouse(const AeronInputSnapshot* input) {
	uint32_t buttons;
	uint32_t released;
	uint32_t doubleClicked;
	int insideClassic;

	if (input == 0) {
		return;
	}

	insideClassic = XwaPresentation_ToClassic(input->mouse.x, input->mouse.y, &g_mouseX, &g_mouseY);

	buttons = input->has_focus && input->mouse.inside_content && insideClassic
				  ? XwaInput_FilterMouseButtons(input->mouse.buttons)
				  : 0;
	released = input->has_focus && input->mouse.inside_content && insideClassic
				   ? XwaInput_FilterMouseButtons(input->mouse.released_buttons)
				   : 0;
	doubleClicked = input->has_focus && input->mouse.inside_content && insideClassic
						? XwaInput_FilterMouseButtons(input->mouse.double_clicked_buttons)
						: 0;

	g_mouseLeftDown = (buttons & AERON_MOUSE_BUTTON_LEFT) != 0;
	g_mouseRightDown = (buttons & AERON_MOUSE_BUTTON_RIGHT) != 0;

	if (released & AERON_MOUSE_BUTTON_LEFT) {
		g_mouseClickLatch.leftClick = 1;
	}

	if (released & AERON_MOUSE_BUTTON_RIGHT) {
		g_mouseClickLatch.rightClick = 1;
	}

	if (doubleClicked & AERON_MOUSE_BUTTON_LEFT) {
		g_mouseClickLatch.leftDblClick = 1;
	}
}

static void XwaInputBridge_SetFrontendKey(unsigned int key, int down) {
	if (key < sizeof(KeyState)) {
		KeyState[key] = down ? XWA_KEY_DOWN : 0;
	}
}

static void XwaInputBridge_SetFrontendKeyPair(unsigned int keyA, unsigned int keyB, int down) {
	XwaInputBridge_SetFrontendKey(keyA, down);
	XwaInputBridge_SetFrontendKey(keyB, down);
}

static void XwaInputBridge_AppendFrontendChar(unsigned char ch) {
	int nextIndex;

	nextIndex = g_charWriteIdx + 1;
	if (nextIndex == 1024) {
		nextIndex = 0;
	}

	if (nextIndex == g_charReadIdx) {
		++g_charReadIdx;
		if (g_charReadIdx == 1024) {
			g_charReadIdx = 0;
		}
	}

	g_charRingBuffer[g_charWriteIdx] = ch;
	g_charWriteIdx = nextIndex;
}

static unsigned int XwaInputBridge_MapAeronKeyToVirtualKey(int key) {
	if (key >= AERON_KEY_A && key < AERON_KEY_A + 26) {
		return (unsigned int)('A' + key - AERON_KEY_A);
	}

	if (key >= AERON_KEY_1 && key < AERON_KEY_1 + 9) {
		return (unsigned int)('1' + key - AERON_KEY_1);
	}

	if (key == AERON_KEY_1 + 9) {
		return '0';
	}

	if (key >= AERON_KEY_F1 && key < AERON_KEY_F1 + 12) {
		return 0x70u + (unsigned int)(key - AERON_KEY_F1);
	}

	switch (key) {
		case AERON_KEY_BACKSPACE:
			return 0x08u;
		case AERON_KEY_TAB:
			return 0x09u;
		case AERON_KEY_RETURN:
			return 0x0du;
		case AERON_KEY_ESCAPE:
			return 0x1bu;
		case AERON_KEY_SPACE:
			return 0x20u;
		case AERON_KEY_PAGEUP:
			return 0x21u;
		case AERON_KEY_PAGEDOWN:
			return 0x22u;
		case AERON_KEY_END:
			return 0x23u;
		case AERON_KEY_HOME:
			return 0x24u;
		case AERON_KEY_LEFT:
			return 0x25u;
		case AERON_KEY_UP:
			return 0x26u;
		case AERON_KEY_RIGHT:
			return 0x27u;
		case AERON_KEY_DOWN:
			return 0x28u;
		case AERON_KEY_PRINTSCREEN:
			return 0x2cu;
		case AERON_KEY_INSERT:
			return 0x2du;
		case AERON_KEY_DELETE:
			return 0x2eu;
		case AERON_KEY_PAUSE:
			return 0x13u;
		case AERON_KEY_CAPSLOCK:
			return 0x14u;
		case AERON_KEY_SCROLLLOCK:
			return 0x91u;
		case AERON_KEY_SEMICOLON:
			return 0xbau;
		case AERON_KEY_EQUALS:
			return 0xbbu;
		case AERON_KEY_COMMA:
			return 0xbcu;
		case AERON_KEY_MINUS:
			return 0xbdu;
		case AERON_KEY_PERIOD:
			return 0xbeu;
		case AERON_KEY_SLASH:
			return 0xbfu;
		case AERON_KEY_GRAVE:
			return 0xc0u;
		case AERON_KEY_LEFTBRACKET:
			return 0xdbu;
		case AERON_KEY_BACKSLASH:
			return 0xdcu;
		case AERON_KEY_RIGHTBRACKET:
			return 0xddu;
		case AERON_KEY_APOSTROPHE:
			return 0xdeu;
		case AERON_KEY_LGUI:
			return 0x5bu;
		case AERON_KEY_RGUI:
			return 0x5cu;
		default:
			return 0;
	}
}

static void XwaInputBridge_UpdateFrontendKeyState(const AeronInputSnapshot* input) {
	int key;

	memset(KeyState, 0, sizeof(KeyState));
	if (!input->has_focus || XwaInput_IsCaptured()) {
		return;
	}

	for (key = 0; key < AERON_KEY_COUNT; ++key) {
		unsigned int virtualKey;

		if (!input->key_down[key] || AeronCompat_IsKeySuppressed(key)) {
			continue;
		}

		virtualKey = XwaInputBridge_MapAeronKeyToVirtualKey(key);
		XwaInputBridge_SetFrontendKey(virtualKey, 1);
	}

	XwaInputBridge_SetFrontendKeyPair(0x10u, 0xa0u, input->key_down[AERON_KEY_LSHIFT]);
	XwaInputBridge_SetFrontendKeyPair(0x10u, 0xa1u, input->key_down[AERON_KEY_RSHIFT]);
	XwaInputBridge_SetFrontendKeyPair(0x11u, 0xa2u, input->key_down[AERON_KEY_LCTRL]);
	XwaInputBridge_SetFrontendKeyPair(0x11u, 0xa3u, input->key_down[AERON_KEY_RCTRL]);
	XwaInputBridge_SetFrontendKeyPair(0x12u, 0xa4u, input->key_down[AERON_KEY_LALT]);
	XwaInputBridge_SetFrontendKeyPair(0x12u, 0xa5u, input->key_down[AERON_KEY_RALT]);
}

static void XwaInputBridge_AppendFrontendText(const AeronInputSnapshot* input) {
	uint32_t i;

	if (!input->has_focus || XwaInput_IsCaptured()) {
		return;
	}

	for (i = 0; i < input->text_length; ++i) {
		unsigned char ch = (unsigned char)input->text[i];

		/* TODO: Recover the original frontend codepage conversion. */
		if (ch >= 0x20 && ch < 0x80) {
			XwaInputBridge_AppendFrontendChar(ch);
		}
	}

	if (input->key_pressed[AERON_KEY_BACKSPACE] && !AeronCompat_IsKeySuppressed(AERON_KEY_BACKSPACE)) {
		XwaInputBridge_AppendFrontendChar(XWA_KEY_BACKSPACE);
	}

	if (input->key_pressed[AERON_KEY_TAB] && !AeronCompat_IsKeySuppressed(AERON_KEY_TAB)) {
		XwaInputBridge_AppendFrontendChar(XWA_KEY_TAB);
	}

	if (input->key_pressed[AERON_KEY_RETURN] && !AeronCompat_IsKeySuppressed(AERON_KEY_RETURN)) {
		XwaInputBridge_AppendFrontendChar(XWA_KEY_RETURN);
	}

	if (input->key_pressed[AERON_KEY_ESCAPE] && !AeronCompat_IsKeySuppressed(AERON_KEY_ESCAPE)) {
		XwaInputBridge_AppendFrontendChar(XWA_KEY_ESCAPE);
	}
}

void XwaInputBridge_UpdateFrontendKeyboard(const AeronInputSnapshot* input) {
	if (input == 0) {
		return;
	}

	XwaInputBridge_UpdateFrontendKeyState(input);
	XwaInputBridge_AppendFrontendText(input);
}

void XwaInputBridge_UpdateFrontend(const AeronInputSnapshot* input) {
	XwaInput_ReconcileKeyboard();
	XwaInputBridge_UpdateFrontendMouse(input);
	XwaInputBridge_UpdateFrontendKeyboard(input);
}
