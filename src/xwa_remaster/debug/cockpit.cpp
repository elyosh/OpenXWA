/*
 * Cockpit menu: the pilot's eye and field of view, and the dynamic cockpit.
 */

#include "xwa_remaster/debug/debug_internal.h"

#include "xwa_remaster/dynamic_cockpit.h"
#include "xwa_remaster/flight.h"

#include "xwa_runtime/models/cockpit_eye.h"
#include "xwa_runtime/models/cockpit_view.h"

#include <cstdio>
#include <cstring>

/* ---- Cockpit eye ------------------------------------------------------------ */

static void xwa_window_cockpit_eye(void*) {
	static int edit[3];
	static int edit_model = -1;
	int model = -1;
	int eye[3];
	if (XwaCockpitEye_GetPlayer(&model, eye)) {
		if (model != edit_model) {
			edit_model = model;
			memcpy(edit, eye, sizeof edit);
		}
		ImGui::Text("Model %d: X %d, Y %d, Z %d", model, eye[0], eye[1], eye[2]);
		ImGui::InputInt3("X, Y, Z", edit);
		ImGui::SetItemTooltip("The eye in the craft table (+0x238). Edits last until the model is\n"
							  "loaded again. A GLB sets it with a Cockpit (type 31) hardpoint.");
		if (ImGui::Button("Apply")) {
			XwaCockpitEye_SetPlayer(edit);
		}
		ImGui::SameLine();
		if (ImGui::Button("Restore")) {
			XwaCockpitEye_RestorePlayer();
			XwaCockpitEye_GetPlayer(&model, edit);
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy as hardpoint")) {
			/* A Cockpit (type 31) hardpoint at OPT (X, -Y, Z) gives this eye.
			   In Blender (glTF imported Z-up, metres): (-x, y, z) * 1600/65536. */
			const float metres = 1600.0f / 65536.0f;
			char text[192];
			snprintf(text, sizeof text, "Cockpit hardpoint (type 31): OPT %d %d %d; Blender %.4f %.4f %.4f m",
					 eye[0], -eye[1], eye[2], -eye[0] * metres, -eye[1] * metres, eye[2] * metres);
			ImGui::SetClipboardText(text);
		}
		ImGui::SetItemTooltip("Copies the hardpoint's position in OPT units and in Blender metres.");
	} else {
		edit_model = -1;
		ImGui::TextDisabled("No player craft.");
	}

	ImGui::SeparatorText("Field of view");
	static const char* const fov_modes[] = { "Cockpit", "Wide", "Off" };
	int fov_mode = (int)XwaCockpitView_Mode();
	if (ImGui::Combo("Mode", &fov_mode, fov_modes, IM_ARRAYSIZE(fov_modes))) {
		XwaCockpitView_SetMode((XwaCockpitFovMode)fov_mode);
	}
	ImGui::SetItemTooltip("Cockpit: the view of the model's pilot eye. Wide: the wide view of the\n"
						  "model's settings. Off: the stock view.");
	float min_aspect = XwaRemasterFlight_GetCockpitFovMinAspect();
	if (ImGui::SliderFloat("Smallest aspect", &min_aspect, 0.0f, 2.5f, "%.3f")) {
		XwaRemasterFlight_SetCockpitFovMinAspect(min_aspect);
	}
	ImGui::SetItemTooltip("A narrower window keeps the horizontal view of this aspect. 1.778 is\n"
						  "16:9. 0 turns it off.");
	ImGui::Text("Vertical: %.1f degrees%s", XwaCockpitView_VerticalFovDegrees(),
				XwaCockpitView_Active() ? "" : " (stock)");
}

/* ---- Dynamic cockpit ---------------------------------------------------------- */

static void xwa_window_dynamic_cockpit(void*) {
	XwaRemasterDcSettings settings;
	XwaRemasterDc_GetSettings(&settings);
	bool changed = XwaDebug_Checkbox("Enabled", &settings.enabled);
	ImGui::SameLine();
	changed |= XwaDebug_Checkbox("Classic HUD", &settings.classic_hud);
	ImGui::SetItemTooltip("Erases nothing from the 2D HUD, like the D key of XWAU.");
	ImGui::SameLine();
	changed |= XwaDebug_Checkbox("Holograms", &settings.holograms);
	changed |=
		ImGui::SliderFloat("Display strength", &settings.display_emissive_strength, 0.0f, 8.0f, "%.2f");
	changed |=
		ImGui::SliderFloat("Hologram strength", &settings.hologram_emissive_strength, 0.0f, 8.0f, "%.2f");
	if (changed) {
		XwaRemasterDc_SetSettings(&settings);
	}
	ImGui::Text("In use: %s", XwaRemasterDc_Active() ? "yes" : "no (no cockpit with screens)");

	ImGui::SeparatorText("Sources");
	ImGui::TextDisabled("Pick a source to highlight its box in the Debug view menu's boxes.");
	if (ImGui::BeginTable("dc_sources", 3,
						  ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV,
						  ImVec2(0, 300))) {
		ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24);
		ImGui::TableSetupColumn("Source");
		ImGui::TableSetupColumn("Box (x0, y0, x1, y1)");
		ImGui::TableHeadersRow();
		for (unsigned i = 0; i < XWA_DC_SOURCE_COUNT; ++i) {
			float box[4];
			const bool placed = XwaRemasterDc_SourceBox(i, box) != 0;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%u", i);
			ImGui::TableNextColumn();
			if (ImGui::Selectable(XwaRemasterDc_SourceName(i), g_xwa_debug_dc_highlight == (int)i,
								  ImGuiSelectableFlags_SpanAllColumns)) {
				g_xwa_debug_dc_highlight = g_xwa_debug_dc_highlight == (int)i ? -1 : (int)i;
				XwaRemasterDc_SetDebugBoxes(XwaRemasterDc_DebugBoxes(), g_xwa_debug_dc_highlight);
			}
			ImGui::TableNextColumn();
			if (placed)
				ImGui::Text("%.1f, %.1f, %.1f, %.1f", box[0], box[1], box[2], box[3]);
			else
				ImGui::TextDisabled("-");
		}
		ImGui::EndTable();
	}
	ImGui::SeparatorText("HUD regions it erases");
	for (unsigned i = 0; i < XWA_DC_REGION_COUNT; ++i) {
		float box[4];
		if (XwaRemasterDc_RegionBox(i, box))
			ImGui::Text("%2u %-26s %.1f, %.1f, %.1f, %.1f", i, XwaRemasterDc_RegionName(i), box[0], box[1],
						box[2], box[3]);
		else
			ImGui::TextDisabled("%2u %-26s -", i, XwaRemasterDc_RegionName(i));
	}
}

void XwaDebugCockpit_Register(void) {
	XwaDebug_AddWindow(XWA_DEBUG_MENU_COCKPIT, "Cockpit Eye", xwa_window_cockpit_eye, 430, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_COCKPIT, "Dynamic Cockpit", xwa_window_dynamic_cockpit, 520, 560);
}
