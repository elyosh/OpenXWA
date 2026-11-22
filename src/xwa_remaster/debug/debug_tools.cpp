/*
 * XWA debug UI: the menus of the Aeron debug overlay and the helpers its
 * windows share. The grave key shows the overlay (see xwa_remaster.c).
 * Built only with the debug UI (XWA_ENABLE_DEBUG_UI).
 *
 * The windows change the renderer's settings through the public accessors.
 * Their changes last until the game quits; remaster/config.yaml holds the
 * values to keep.
 */

#include "xwa_remaster/debug_tools.h"

#include "xwa_remaster/debug/debug_internal.h"

#include <cstdarg>

int g_xwa_debug_dc_highlight = -1;

void XwaDebug_AddWindow(const char* menu, const char* title, AeronDebugWindowFn draw, float width,
						float height, AeronDebugWindowFn closed) {
	AeronDebugWindowDesc desc = {};
	desc.menu = menu;
	desc.title = title;
	desc.draw = draw;
	desc.closed = closed;
	desc.width = width;
	desc.height = height;
	Aeron_DebugAddWindow(&desc);
}

bool XwaDebug_Checkbox(const char* label, int* value) {
	bool on = *value != 0;
	if (!ImGui::Checkbox(label, &on)) {
		return false;
	}
	*value = on ? 1 : 0;
	return true;
}

bool XwaDebug_ResetButton(const char* label) {
	const bool pressed = ImGui::Button(label);
	ImGui::SetItemTooltip("Puts back the values of remaster/config.yaml.");
	return pressed;
}

bool XwaDebug_BeginValues(const char* id) { return ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit); }

void XwaDebug_Value(const char* name, const char* format, ...) {
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextDisabled("%s", name);
	ImGui::TableNextColumn();
	va_list args;
	va_start(args, format);
	ImGui::TextV(format, args);
	va_end(args);
}

void XwaDebug_EndValues(void) { ImGui::EndTable(); }

extern "C" void XwaRemasterDebugTools_Register(void) {
	if (!Aeron_DebugUiAvailable()) {
		return;
	}
	static const char* const menus[] = {
		XWA_DEBUG_MENU_DISPLAY, XWA_DEBUG_MENU_IMAGE,   XWA_DEBUG_MENU_LIGHTING,
		XWA_DEBUG_MENU_MODELS,  XWA_DEBUG_MENU_EFFECTS, XWA_DEBUG_MENU_COCKPIT,
		XWA_DEBUG_MENU_AUDIO,   XWA_DEBUG_MENU_VIEW,    XWA_DEBUG_MENU_DIAGNOSTICS,
	};
	Aeron_DebugSetMenuOrder(menus, (int)(sizeof menus / sizeof menus[0]));
	XwaDebugDisplay_Register();
	XwaDebugImage_Register();
	XwaDebugLighting_Register();
	XwaDebugModels_Register();
	XwaDebugEffects_Register();
	XwaDebugCockpit_Register();
	XwaDebugView_Register();
	XwaDebugDiagnostics_Register();
}
