/*
 * Debug view menu: the views drawn in place of the image, one at a time, and
 * the overlays drawn over it.
 */

#include "xwa_remaster/debug/debug_internal.h"

#include "xwa_remaster/dynamic_cockpit.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/ship.h"

#include "aeron/scene/present.h"

#include <cmath>
#include <cstdio>

/* The PBR terms that the material debug view isolates (PbrIsolateMode),
 * from 1. */
static const char* const k_material_terms[] = {
	"Base diffuse",
	"Specular",
	"Geometry term (G)",
	"N.V",
	"N.L",
	"Normal",
	"View vector",
	"N.V signed",
	"Environment diffuse",
	"Environment specular",
};

/* Turns off every view drawn in place of the image. */
static void xwa_view_clear(void) {
	XwaRemasterFlight_SetPresentDebugView(AERON_SCENE_PRESENT_IMAGE);

	XwaShipPbrTuning tuning;
	XwaRemasterShip_GetPbrTuning(&tuning);
	if (tuning.debug_isolate_term != 0.0f) {
		tuning.debug_isolate_term = 0.0f;
		XwaRemasterShip_SetPbrTuning(&tuning);
	}
	XwaFlightSsaoParams ssao;
	XwaRemasterFlight_GetSsao(&ssao);
	if (ssao.ssao_debug_viz) {
		ssao.ssao_debug_viz = 0;
		XwaRemasterFlight_SetSsao(&ssao);
	}
	XwaFlightMotionBlurParams blur;
	XwaRemasterFlight_GetMotionBlur(&blur);
	if (blur.velocity_viz) {
		blur.velocity_viz = 0;
		XwaRemasterFlight_SetMotionBlur(&blur);
	}
	XwaFlightPointLightParams lights;
	XwaRemasterFlight_GetPointLights(&lights);
	if (lights.cluster_debug) {
		lights.cluster_debug = 0;
		XwaRemasterFlight_SetPointLights(&lights);
	}
	XwaFlightShadowParams shadows;
	XwaRemasterFlight_GetShadows(&shadows);
	if (shadows.debug_cascades) {
		shadows.debug_cascades = 0;
		XwaRemasterFlight_SetShadows(&shadows);
	}
	int atlas = 0, cascade = -1;
	XwaRemasterFlight_GetShadowAtlasDebug(&atlas, &cascade);
	if (atlas) {
		XwaRemasterFlight_SetShadowAtlasDebug(0, cascade);
	}
	XwaFlightTemporalParams temporal;
	XwaRemasterFlight_GetTemporal(&temporal);
	if (temporal.debug_view) {
		temporal.debug_view = 0;
		XwaRemasterFlight_SetTemporal(&temporal);
	}
	XwaRemasterFlight_SetHyperspaceWarpView(XWA_FLIGHT_HYPERSPACE_WARP_VIEW_IMAGE);
}

/* A menu item for one view. Picking it turns the other views off and calls
 * `set`. */
template <typename Set>
static void xwa_view_item(const char* label, bool selected, Set set, const char* tip = nullptr) {
	if (ImGui::MenuItem(label, nullptr, selected) && !selected) {
		xwa_view_clear();
		set();
	}
	if (tip) {
		ImGui::SetItemTooltip("%s", tip);
	}
}

static void xwa_view_menu(void*) {
	const int present = XwaRemasterFlight_PresentDebugView();
	XwaShipPbrTuning tuning;
	XwaRemasterShip_GetPbrTuning(&tuning);
	const int term = (int)tuning.debug_isolate_term;
	XwaFlightSsaoParams ssao;
	XwaRemasterFlight_GetSsao(&ssao);
	XwaFlightMotionBlurParams blur;
	XwaRemasterFlight_GetMotionBlur(&blur);
	XwaFlightPointLightParams lights;
	XwaRemasterFlight_GetPointLights(&lights);
	XwaFlightShadowParams shadows;
	XwaRemasterFlight_GetShadows(&shadows);
	int atlas = 0, cascade = -1;
	XwaRemasterFlight_GetShadowAtlasDebug(&atlas, &cascade);
	XwaFlightTemporalParams temporal;
	XwaRemasterFlight_GetTemporal(&temporal);
	const int warp = (int)XwaRemasterFlight_GetHyperspaceWarpView();
	const bool normal = present == AERON_SCENE_PRESENT_IMAGE && term == 0 && !ssao.ssao_debug_viz &&
						!blur.velocity_viz && !lights.cluster_debug && !shadows.debug_cascades && !atlas &&
						!temporal.debug_view && warp == XWA_FLIGHT_HYPERSPACE_WARP_VIEW_IMAGE;

	xwa_view_item("Normal image", normal, [] {});

	if (ImGui::BeginMenu("Exposure")) {
		xwa_view_item(
			"False color", present == AERON_SCENE_PRESENT_FALSE_COLOR,
			[] { XwaRemasterFlight_SetPresentDebugView(AERON_SCENE_PRESENT_FALSE_COLOR); },
			"One color per stop from mid-grey. The Exposure window shows the legend.");
		xwa_view_item(
			"Meter weights", present == AERON_SCENE_PRESENT_METER_WEIGHT,
			[] { XwaRemasterFlight_SetPresentDebugView(AERON_SCENE_PRESENT_METER_WEIGHT); },
			"How much each pixel counts for the exposure meter. Needs the metered\n"
			"exposure.");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Material terms")) {
		for (int i = 0; i < IM_ARRAYSIZE(k_material_terms); i++) {
			const int mode = i + 1;
			xwa_view_item(k_material_terms[i], term == mode, [mode] {
				XwaShipPbrTuning t;
				XwaRemasterShip_GetPbrTuning(&t);
				t.debug_isolate_term = (float)mode;
				XwaRemasterShip_SetPbrTuning(&t);
			});
		}
		ImGui::EndMenu();
	}
	xwa_view_item(
		"Ambient occlusion", ssao.ssao_debug_viz != 0,
		[] {
			XwaFlightSsaoParams p;
			XwaRemasterFlight_GetSsao(&p);
			p.ssao_debug_viz = 1;
			XwaRemasterFlight_SetSsao(&p);
		},
		"The raw AO in grey. Needs SSAO.");
	xwa_view_item(
		"Motion vectors", blur.velocity_viz != 0,
		[] {
			XwaFlightMotionBlurParams p;
			XwaRemasterFlight_GetMotionBlur(&p);
			p.velocity_viz = 1;
			XwaRemasterFlight_SetMotionBlur(&p);
		},
		"Needs motion blur.");
	xwa_view_item(
		"Light clusters", lights.cluster_debug != 0,
		[] {
			XwaFlightPointLightParams p;
			XwaRemasterFlight_GetPointLights(&p);
			p.cluster_debug = 1;
			XwaRemasterFlight_SetPointLights(&p);
		},
		"How many point lights each screen tile has.");
	xwa_view_item("Shadow cascades", shadows.debug_cascades != 0, [] {
		XwaFlightShadowParams p;
		XwaRemasterFlight_GetShadows(&p);
		p.debug_cascades = 1;
		XwaRemasterFlight_SetShadows(&p);
	});
	if (ImGui::BeginMenu("Shadow atlas")) {
		xwa_view_item(
			"Whole atlas", atlas && cascade < 0, [] { XwaRemasterFlight_SetShadowAtlasDebug(1, -1); },
			"Orange lines mark the atlas tiles.");
		for (int c = 0; c < (int)shadows.cascade_count; c++) {
			char label[16];
			snprintf(label, sizeof label, "Cascade %d", c);
			xwa_view_item(label, atlas && cascade == c, [c] { XwaRemasterFlight_SetShadowAtlasDebug(1, c); });
		}
		ImGui::EndMenu();
	}
	xwa_view_item(
		"FSR inputs", temporal.debug_view != 0,
		[] {
			XwaFlightTemporalParams p;
			XwaRemasterFlight_GetTemporal(&p);
			p.debug_view = 1;
			XwaRemasterFlight_SetTemporal(&p);
		},
		"AMD's debug view of the motion, depth, disocclusion, reactive and detail\n"
		"protection data. Needs FSR.");
	if (ImGui::BeginMenu("Hyperspace warp")) {
		static const char* const names[] = { "Keep mask", "Warped sky", "Reveal" };
		for (int v = 1; v < XWA_FLIGHT_HYPERSPACE_WARP_VIEW_COUNT; v++) {
			xwa_view_item(names[v - 1], warp == v,
						  [v] { XwaRemasterFlight_SetHyperspaceWarpView((XwaFlightHyperspaceWarpView)v); });
		}
		ImGui::EndMenu();
	}
	ImGui::SetItemTooltip("Shown during the scene warp of a jump.");

	ImGui::SeparatorText("Overlays");
	XwaFlightLodParams lod;
	XwaRemasterFlight_GetLod(&lod);
	if (ImGui::MenuItem("Levels of detail", nullptr, lod.tint != 0)) {
		lod.tint = !lod.tint;
		XwaRemasterFlight_SetLod(&lod);
	}
	ImGui::SetItemTooltip("Tints levels 1, 2 and 3 in green, yellow and red.");
	const int probes = XwaRemasterFlight_ShowProbes();
	if (ImGui::MenuItem("Probes", nullptr, probes != 0)) {
		XwaRemasterFlight_SetShowProbes(!probes);
	}
	ImGui::SetItemTooltip("Each probe's influence (dimmer where it applies fully), its parallax\n"
						  "volume in grey, and the light grid's box in white with its points within\n"
						  "50 m.");
	const int boxes = XwaRemasterDc_DebugBoxes();
	if (ImGui::MenuItem("Dynamic cockpit boxes", nullptr, boxes != 0)) {
		XwaRemasterDc_SetDebugBoxes(!boxes, g_xwa_debug_dc_highlight);
	}
	ImGui::SetItemTooltip("The sources in green, the one picked in the Dynamic Cockpit window in\n"
						  "yellow, and the HUD regions they erase in red.");
}

void XwaDebugView_DrawFalseColorLegend(void) {
	/* Matches AeronTonemapFalseColor's bands (linear colors). */
	static const float bands[13][3] = {
		{ 0.02f, 0.00f, 0.05f }, { 0.15f, 0.00f, 0.30f }, { 0.00f, 0.00f, 0.60f }, { 0.00f, 0.20f, 1.00f },
		{ 0.00f, 0.50f, 0.50f }, { 0.00f, 0.40f, 0.10f }, { 0.25f, 0.25f, 0.25f }, { 0.30f, 0.70f, 0.00f },
		{ 1.00f, 1.00f, 0.00f }, { 1.00f, 0.40f, 0.00f }, { 1.00f, 0.00f, 0.00f }, { 1.00f, 0.00f, 0.50f },
		{ 1.00f, 1.00f, 1.00f },
	};
	static const char* const labels[13] = { "< -5", "-5", "-4", "-3", "-2", "-1",  "grey",
											"+1",   "+2", "+3", "+4", "+5", "clip" };
	for (int i = 0; i < 13; i++) {
		const ImVec4 color(powf(bands[i][0], 1.0f / 2.2f), powf(bands[i][1], 1.0f / 2.2f),
						   powf(bands[i][2], 1.0f / 2.2f), 1.0f);
		if (i > 0) {
			ImGui::SameLine(0.0f, 2.0f);
		}
		ImGui::PushID(i);
		ImGui::ColorButton("##band", color, ImGuiColorEditFlags_NoTooltip, ImVec2(28.0f, 16.0f));
		ImGui::PopID();
		ImGui::SetItemTooltip("%s", labels[i]);
	}
	ImGui::TextUnformatted("Stops from mid-grey (0.18 after the exposure). Grey is mid-grey to +1.\n"
						   "White is where AgX clips.");
}

void XwaDebugView_Register(void) { Aeron_DebugAddMenuItems(XWA_DEBUG_MENU_VIEW, xwa_view_menu, nullptr); }
