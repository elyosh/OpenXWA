#include "options/options_window.h"

#include "options/controller_page.h"
#include "options/head_tracking_page.h"
#include "options/keyboard_page.h"
#include "options/mouse_page.h"
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/input/capture.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/runtime/port.h"
#include <stdio.h>
#include <string.h>

typedef void (*XwaOptionsPageFn)(AeronUiContext* ui, const AeronInputSnapshot* input);

enum {
	OPTIONS_PAGE_CONTROLLER,
	OPTIONS_PAGE_KEYBOARD,
	OPTIONS_PAGE_MOUSE,
	OPTIONS_PAGE_HEAD_TRACKING,
	OPTIONS_PAGE_COUNT
};

static struct {
	AeronUiContext* ui;
	XwaOptionsPageFn pages[OPTIONS_PAGE_COUNT];
	bool open, close_requested, close_forced, captures_controller, ready;
	/* The notice shown once after the controller settings were reset. */
	bool notice_open;
	int page;
	char error[1024];
	XwaControllerSettings controller;
	XwaKeyboardSettings keyboard;
} g_window;

static void XwaOptionsWindow_DrawController(AeronUiContext* ui, const AeronInputSnapshot* input) {
	XwaControllerSettings_Draw(&g_window.controller, ui, input);
}

static void XwaOptionsWindow_DrawKeyboard(AeronUiContext* ui, const AeronInputSnapshot* input) {
	(void)input;
	XwaKeyboardSettings_Draw(&g_window.keyboard, ui);
}

bool XwaOptionsWindow_Init(XwaAppUi* ui, char* error, size_t capacity) {
	memset(&g_window, 0, sizeof g_window);
	g_window.ui = XwaAppUi_Context(ui);
	g_window.pages[OPTIONS_PAGE_CONTROLLER] = XwaOptionsWindow_DrawController;
	g_window.pages[OPTIONS_PAGE_KEYBOARD] = XwaOptionsWindow_DrawKeyboard;
	g_window.pages[OPTIONS_PAGE_MOUSE] = XwaMousePage_Draw;
	g_window.pages[OPTIONS_PAGE_HEAD_TRACKING] = XwaHeadTrackingPage_Draw;
	g_window.ready = g_window.ui != NULL;
	if (!g_window.ready && error && capacity)
		snprintf(error, capacity, "the options window has no UI context");
	return g_window.ready;
}

void XwaOptionsWindow_FlushForExit(void) {
	if (!g_window.ready)
		return;
	char error[1024];
	if (g_window.open) {
		if (!XwaKeyboardSettings_Commit(&g_window.keyboard, error, sizeof error))
			Aeron_LogError("xwa.options", "%s", error);
		if (!XwaControllerSettings_Commit(&g_window.controller, error, sizeof error))
			Aeron_LogError("xwa.options", "%s", error);
	}
	if (!XwaModernInputOptions_FlushWithError(error, sizeof error))
		Aeron_LogError("xwa.options", "%s", error);
}

void XwaOptionsWindow_Shutdown(void) {
	XwaKeyboardSettings_CancelCapture(&g_window.keyboard, g_window.ui);
	if (g_window.ui)
		AeronUi_CancelControllerCapture(g_window.ui);
	if (g_window.open)
		XwaPort_SetOptionsWindowOpen(0);
	memset(&g_window, 0, sizeof g_window);
}

bool XwaOptionsWindow_Open(void) { return g_window.open; }

bool XwaOptionsWindow_CapturesKeyboard(void) {
	return g_window.open && AeronUi_KeyboardCaptureActive(g_window.ui);
}

bool XwaOptionsWindow_CapturesController(void) { return g_window.open && g_window.captures_controller; }

void XwaOptionsWindow_Show(void) {
	if (g_window.ui && !g_window.open) {
		static XwaModernInputOptions options;
		XwaModernInputOptions_Get(&options);
		g_window.open = true;
		XwaControllerSettings_Open(&g_window.controller, &options);
		XwaKeyboardSettings_Open(&g_window.keyboard, &options);
		g_window.error[0] = 0;
		XwaInput_SetCaptured(true);
		XwaPort_SetOptionsWindowOpen(1);
		Aeron_SetHostCursorVisible(1);
	}
}

void XwaOptionsWindow_RequestClose(void) {
	if (g_window.open)
		g_window.close_requested = true;
}

bool XwaOptionsWindow_CloseRequested(void) { return g_window.close_requested; }

void XwaOptionsWindow_CompleteClose(void) {
	XwaKeyboardSettings_CancelCapture(&g_window.keyboard, g_window.ui);
	XwaControllerSettings_CancelCapture(&g_window.controller, g_window.ui);
	AeronUi_CancelControllerCapture(g_window.ui);
	g_window.open = g_window.close_requested = g_window.close_forced = g_window.captures_controller = false;
	XwaPort_SetOptionsWindowOpen(g_window.notice_open);
}

void XwaOptionsWindow_ReportError(const char* error) {
	snprintf(g_window.error, sizeof g_window.error, "%s", error);
	g_window.close_requested = false;
}

void XwaOptionsWindow_ShowControllerResetNotice(void) {
	if (!g_window.ui || g_window.notice_open)
		return;
	g_window.notice_open = true;
	XwaInput_SetCaptured(true);
	/* The game cursor is hidden as for the window. */
	XwaPort_SetOptionsWindowOpen(1);
	Aeron_SetHostCursorVisible(1);
}

static void XwaOptionsWindow_CloseNotice(void) {
	g_window.notice_open = false;
	if (!g_window.open)
		XwaPort_SetOptionsWindowOpen(0);
}

static void XwaOptionsWindow_DrawNotice(const AeronInputSnapshot* input, float seconds) {
	AeronUi_BeginFrame(g_window.ui, &(AeronUiFrameDesc) { .input = input, .dt_seconds = seconds });
	if (AeronUi_BeginWindow(g_window.ui, "Controller Settings Reset",
							&(AeronUiWindowDesc) { .width_ref = 720, .centered = 1 })) {
		AeronUi_Help(g_window.ui,
					 "Controller support was improved, and your controller settings were reset. Gamepads "
					 "work right away with the default controls. To use a joystick, set it up again in "
					 "OpenXWA Options.");
		if (AeronUi_Button(g_window.ui, "OK"))
			XwaOptionsWindow_CloseNotice();
		AeronUi_EndWindow(g_window.ui);
	}
	AeronUiOutput output = AeronUi_EndFrame(g_window.ui);
	if (output.cancel_pressed)
		XwaOptionsWindow_CloseNotice();
	AeronUi_Submit(g_window.ui);
}

void XwaOptionsWindow_Frame(const AeronInputSnapshot* input, float seconds) {
	if (!input)
		return;
	if (!g_window.open) {
		if (g_window.notice_open)
			XwaOptionsWindow_DrawNotice(input, seconds);
		return;
	}
	static const char* pages[] = { "Controller", "Keyboard", "Mouse", "Head Tracking" };
	AeronUi_BeginFrame(g_window.ui, &(AeronUiFrameDesc) { .input = input, .dt_seconds = seconds });
	if (AeronUi_BeginWindow(g_window.ui, "OpenXWA Options",
							&(AeronUiWindowDesc) { .width_ref = 980, .height_ref = 1000, .centered = 1 })) {
		AeronUi_BeginTabBar(g_window.ui, "pages", pages, OPTIONS_PAGE_COUNT, &g_window.page);
		if (g_window.page != OPTIONS_PAGE_CONTROLLER) {
			XwaControllerSettings_CancelCapture(&g_window.controller, g_window.ui);
			g_window.controller.editor.binding_modal_open = 0;
			g_window.controller.restore_modal_open = 0;
		}
		if (g_window.page != OPTIONS_PAGE_KEYBOARD)
			XwaKeyboardSettings_CancelCapture(&g_window.keyboard, g_window.ui);
		if (g_window.pages[g_window.page])
			g_window.pages[g_window.page](g_window.ui, input);
		AeronUi_EndTabBar(g_window.ui);
		if (g_window.error[0])
			AeronUi_Error(g_window.ui, g_window.error);
		AeronUi_Separator(g_window.ui);
		if (AeronUi_Button(g_window.ui, "Close"))
			XwaOptionsWindow_RequestClose();
		if (g_window.page == OPTIONS_PAGE_CONTROLLER)
			XwaControllerSettings_DrawModals(&g_window.controller, g_window.ui, input);
		if (g_window.page == OPTIONS_PAGE_KEYBOARD)
			XwaKeyboardSettings_DrawModals(&g_window.keyboard, g_window.ui);
		AeronUi_EndWindow(g_window.ui);
	}
	AeronUiOutput output = AeronUi_EndFrame(g_window.ui);
	g_window.captures_controller = output.capture_all != 0;
	if (output.cancel_pressed)
		XwaOptionsWindow_RequestClose();
	AeronUi_Submit(g_window.ui);
}

bool XwaOptionsWindow_BeginFrame(const AeronInputSnapshot* input) {
	bool was_showing = g_window.open || g_window.notice_open;
	if (g_window.open)
		XwaControllerSettings_Discover(&g_window.controller, g_window.ui, input,
									   XwaModernInputOptions_GamepadDefaults());
	XwaControllerMapping_ApplyPending();
	/* The datapad closed under the window. */
	if (XwaPort_ConsumeOptionsWindowCloseRequest() && g_window.open) {
		g_window.close_requested = true;
		g_window.close_forced = true;
	}
	if (g_window.close_requested) {
		char error[1024];
		if (XwaControllerSettings_Commit(&g_window.controller, error, sizeof error) &&
			XwaKeyboardSettings_Commit(&g_window.keyboard, error, sizeof error) &&
			XwaModernInputOptions_FlushWithError(error, sizeof error))
			XwaOptionsWindow_CompleteClose();
		else if (g_window.close_forced) {
			Aeron_LogError("xwa.options", "%s", error);
			XwaOptionsWindow_CompleteClose();
		} else
			XwaOptionsWindow_ReportError(error);
	}
	XwaInput_BeginCaptureFrame(input, was_showing || g_window.open || g_window.notice_open || !input ||
										  !input->has_focus);
	XwaControllerMapping_Update(input);
	return false;
}

bool XwaOptionsWindow_ConsumeRuntimeRequest(void) {
	if (!XwaPort_ConsumeOptionsWindowRequest())
		return false;
	bool was_open = g_window.open;
	XwaOptionsWindow_Show();
	return !was_open && g_window.open;
}
