/*
 * XWA debug tools — ImGui tool windows for the Aeron debug overlay
 * (backquote toggles; see xwa_remaster.c). Mirrors the TIE inspector
 * set for the shared machinery:
 *
 *   PBR — Global : XwaShipPbrTuning + mesh texture filtering
 *   SSAO         : XwaFlightSsaoParams (live; remaster/config.yaml persists)
 *   HDR & Display: output mode + AeronScenePresent_* tonemap knobs
 *
 * All state mutations go through the public accessors — no renderer
 * internals are reached into. Compiled only when AERON_DEBUG_UI.
 */

#include "xwa_remaster/debug_tools.h"
#include "xwa_remaster/dynamic_cockpit.h"
#include "xwa_remaster/env_capture.h"

#include "xwa_remaster/environment.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/hud.h"
#include "xwa_remaster/hud_boxes.h"
#include "xwa_remaster/hud_cmd.h"
#include "xwa_remaster/hud_layout.h"
#include "xwa_remaster/ship.h"
#include "xwa_remaster/xwa_remaster.h"

#include "aeron/debug.h"
#include "aeron/render.h"
#include "aeron/scene/bloom.h"
#include "aeron/scene/present.h"
#include "aeron/time.h"
#include "xwa/flight/flight_debug.h"
#include "xwa_runtime/models/cockpit_eye.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <cstdio>
#include <cstring>
#include <imgui.h>

/* ---- PBR — Global --------------------------------------------------- */

static void xwa_tool_pbr(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(440, 400), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("PBR — Global", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}

	XwaShipPbrTuning t;
	XwaRemasterShip_GetPbrTuning(&t);
	bool changed = false;

	changed |= ImGui::SliderFloat("Sun illuminance scale", &t.sun_illuminance_scale, 0.0f, 8.0f, "%.3f");
	ImGui::TextDisabled("Illuminance per unit of sun intensity; pi = as bright as the classic.");
	changed |=
		ImGui::ColorEdit3("Ambient fill", t.ambient, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	ImGui::TextDisabled("Linear ambient cube fill.");
	bool adapt = t.spec_geom_adapt != 0.0f;
	if (ImGui::Checkbox("Spec normal adaptation", &adapt)) {
		t.spec_geom_adapt = adapt ? 1.0f : 0.0f;
		changed = true;
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Mesh texture filtering");
	XwaFlightTextureFilteringParams filtering;
	XwaRemasterFlight_GetTextureFiltering(&filtering);
	bool filtering_changed = false;
	bool anisotropic = filtering.anisotropic != 0;
	if (ImGui::Checkbox("Anisotropic filtering", &anisotropic)) {
		filtering.anisotropic = anisotropic ? 1 : 0;
		filtering_changed = true;
	}
	if (!anisotropic) {
		ImGui::BeginDisabled();
	}
	filtering_changed |=
		ImGui::SliderFloat("Maximum anisotropy", &filtering.max_anisotropy, 1.0f, 16.0f, "%.0fx");
	if (!anisotropic) {
		ImGui::EndDisabled();
	}
	ImGui::TextDisabled("Applies to base-color, normal, material and emissive atlases.\n"
						"Edits are not persisted; copy them into remaster/config.yaml.");
	if (ImGui::Button("Reset texture filtering")) {
		XwaRemasterFlight_GetTextureFilteringDefault(&filtering);
		filtering_changed = true;
	}
	if (filtering_changed) {
		XwaRemasterFlight_SetTextureFiltering(&filtering);
	}

	ImGui::Separator();
	static const char* isolate_names[] = {
		"None",
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
	int iso = (int)t.debug_isolate_term;
	if (ImGui::Combo("Isolate term", &iso, isolate_names,
					 (int)(sizeof isolate_names / sizeof isolate_names[0]))) {
		t.debug_isolate_term = (float)iso;
		changed = true;
	}

	ImGui::Separator();
	bool material_override = t.debug_material_override != 0.0f;
	if (ImGui::Checkbox("Override materials", &material_override)) {
		t.debug_material_override = material_override ? 1.0f : 0.0f;
		changed = true;
	}
	ImGui::BeginDisabled(!material_override);
	changed |= ImGui::SliderFloat("Metallic", &t.debug_metallic, 0.0f, 1.0f, "%.2f");
	changed |= ImGui::SliderFloat("Roughness", &t.debug_roughness, 0.0f, 1.0f, "%.2f");
	ImGui::EndDisabled();
	ImGui::TextDisabled("Shades every PBR material with these values, e.g. metallic 1 and roughness 0.1\n"
						"to check environment reflections without authored PBR assets.");

	ImGui::Separator();
	bool backface_fill = XwaRemasterShip_BackfaceFill() != 0;
	if (ImGui::Checkbox("Back-face fill", &backface_fill)) {
		XwaRemasterShip_SetBackfaceFill(backface_fill ? 1 : 0);
	}
	ImGui::TextDisabled("Dark interior behind cracks between model surfaces, instead of the\n"
						"background. Off for comparison and profiling.");

	ImGui::Separator();
	if (ImGui::Button("Reset to defaults")) {
		XwaRemasterShip_GetPbrTuningDefault(&t);
		XwaRemasterShip_SetBackfaceFill(1);
		changed = true;
	}
	if (changed) {
		XwaRemasterShip_SetPbrTuning(&t);
	}

	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- SSAO ----------------------------------------------------------- */

static void xwa_tool_ssao(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(440, 360), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("SSAO", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}

	XwaFlightSsaoParams p;
	XwaRemasterFlight_GetSsao(&p);
	bool changed = false;

	int quality = p.ssao_quality;
	ImGui::RadioButton("Off", &quality, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Low (8 taps)", &quality, 1);
	ImGui::SameLine();
	ImGui::RadioButton("High (16 taps + blur)", &quality, 2);
	if (quality != p.ssao_quality) {
		p.ssao_quality = quality;
		changed = true;
	}

	changed |= ImGui::SliderFloat("Intensity", &p.ssao_intensity, 0.0f, 1.0f, "%.2f");
	changed |= ImGui::SliderFloat("Radius (view units)", &p.ssao_radius_view, 1.0f, 4096.0f, "%.1f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("XWA view unit ~= 2.4 cm. TIE's shipped calibration is 655\n"
						"(~16 m, craft-scale cavity shading; TIE shows it as 0.01 in\n"
						"its own km-scale view units). 16 ~= 0.4 m contact AO.");
	changed |= ImGui::SliderFloat("Bias (view units)", &p.ssao_bias_view, 0.0f, 256.0f, "%.2f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("Self-occlusion rejection; keep ~ radius / 20 (TIE: 32.8).");
	changed |= ImGui::SliderFloat("Power (contrast)", &p.ssao_power, 0.25f, 4.0f, "%.2f");
	changed |= ImGui::SliderFloat("Direct-diffuse weight", &p.ssao_direct, 0.0f, 1.0f, "%.2f");
	ImGui::TextDisabled("How much AO also occludes the direct sun diffuse\n"
						"(0 = ambient-only, physically strict).");
	bool viz = p.ssao_debug_viz != 0;
	if (ImGui::Checkbox("Debug viz (raw AO as grayscale)", &viz)) {
		p.ssao_debug_viz = viz ? 1 : 0;
		changed = true;
	}

	ImGui::Separator();
	ImGui::TextDisabled("Screen-space radius clamp. A world-space radius shrinks to\n"
						"sub-texel on far hulls (kernel aliases into a grid) and over-\n"
						"spreads on near ones. Fractions of NDC half-width; 0 = off.");
	changed |= ImGui::SliderFloat("Min screen radius", &p.ssao_min_screen_frac, 0.0f, 0.05f, "%.4f");
	ImGui::TextDisabled("Footprint floor on distant surfaces (~ frac * ao_w texels).\n"
						"Raise until the far grid disappears.");
	changed |= ImGui::SliderFloat("Max screen radius", &p.ssao_max_screen_frac, 0.0f, 0.25f, "%.4f");
	ImGui::TextDisabled("Footprint ceiling on near surfaces; raise the base radius,\n"
						"then lower this until the separated tap copies merge. 0 = off.");
	changed |= ImGui::SliderFloat("Sample jitter", &p.ssao_sample_jitter, 0.0f, 1.0f, "%.2f");
	ImGui::TextDisabled("Per-pixel radius jitter. Raise to dissolve the 'separate\n"
						"shadows' from discrete taps into blur-smoothed noise; too\n"
						"high adds grain. Free (one ALU hash). 0 = off.");

	ImGui::Separator();
	if (ImGui::Button("Reset to defaults")) {
		XwaRemasterFlight_GetSsaoDefault(&p);
		changed = true;
	}
	ImGui::TextDisabled("Edits apply on the next frame and are NOT persisted —\n"
						"copy values into resources/remaster/config.yaml.");
	if (changed) {
		XwaRemasterFlight_SetSsao(&p);
	}

	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- Directional shadows ------------------------------------------- */

static void xwa_tool_shadows(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(500, 520), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("Directional Shadows", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}

	XwaFlightShadowParams p;
	XwaRemasterFlight_GetShadows(&p);
	bool changed = false;
	bool enabled = p.enabled != 0;
	if (ImGui::Checkbox("Enabled", &enabled)) {
		p.enabled = enabled ? 1 : 0;
		changed = true;
	}
	static const char* atlas_names[] = { "1024", "2048", "4096", "8192" };
	int atlas_index = p.atlas_size == 1024 ? 0 : (p.atlas_size == 2048 ? 1 : (p.atlas_size == 8192 ? 3 : 2));
	if (ImGui::Combo("Atlas size", &atlas_index, atlas_names, 4)) {
		p.atlas_size = 1024 << atlas_index;
		changed = true;
	}
	int cascade_count = (int)p.cascade_count;
	if (ImGui::SliderInt("Cascades", &cascade_count, 1, AERON_SCENE_SHADOW_MAX_CASCADES)) {
		p.cascade_count = (uint32_t)cascade_count;
		changed = true;
	}
	static const char* fit_names[] = { "Stable sphere", "Frustum rectangle", "Scene-dependent PSSM" };
	int fit_mode = (int)p.fit_mode;
	if (ImGui::Combo("Cascade fit", &fit_mode, fit_names, 3)) {
		p.fit_mode = (uint32_t)fit_mode;
		changed = true;
	}
	changed |= ImGui::SliderFloat("Maximum distance", &p.max_distance, 4096.0f, 1048576.0f, "%.0f",
								  ImGuiSliderFlags_Logarithmic);
	bool explicit_splits = p.explicit_splits != 0;
	if (ImGui::Checkbox("Explicit splits", &explicit_splits)) {
		p.explicit_splits = explicit_splits ? 1 : 0;
		changed = true;
	}
	if (explicit_splits) {
		for (int split = 0; split < AERON_SCENE_SHADOW_MAX_CASCADES - 1; split++) {
			char label[24];
			snprintf(label, sizeof label, "Split %d position", split + 1);
			const float lower = split == 0 ? 0.001f : p.split_positions[split - 1] + 0.001f;
			const float upper = split == 2 ? 0.999f : p.split_positions[split + 1] - 0.001f;
			changed |= ImGui::SliderFloat(label, &p.split_positions[split], lower, upper, "%.3f");
		}
		ImGui::TextDisabled("The first %d position(s) are active.", p.cascade_count - 1);
	} else {
		changed |= ImGui::SliderFloat("Split lambda", &p.split_lambda, 0.0f, 1.0f, "%.2f");
	}
	ImGui::TextDisabled("Contact-hardening PCSS: 16-tap blocker search, 16 filter taps.");
	changed |=
		ImGui::SliderFloat("Light angular radius", &p.light_angular_radius_degrees, 0.0f, 5.0f, "%.3f deg");
	changed |= ImGui::SliderFloat("Maximum filter radius", &p.max_filter_radius, p.pcss_min_filter_radius,
								  16.0f, "%.2f texels");
	changed |= ImGui::SliderFloat("Minimum PCSS radius", &p.pcss_min_filter_radius, 0.5f, p.max_filter_radius,
								  "%.2f texels");
	changed |= ImGui::SliderFloat("Normal bias", &p.normal_bias_texels, 0.0f, 4.0f, "%.2f texels");
	changed |= ImGui::SliderFloat("Depth bias", &p.depth_bias_texels, 0.0f, 4.0f, "%.2f texels");
	changed |= ImGui::SliderFloat("Cascade transition", &p.transition_fraction, 0.0f, 0.5f, "%.2f");
	changed |= ImGui::SliderFloat("Distance fade", &p.distance_fade_fraction, 0.0f, 0.5f, "%.2f");
	changed |= ImGui::SliderFloat("Cockpit near tile", &p.receiver_local_near_distance_m, 0.0f, 4.0f, "%.2f m");
	ImGui::TextDisabled("Camera distance the cockpit's finer shadow tile covers; 0 = off.");
	bool debug_cascades = p.debug_cascades != 0;
	if (ImGui::Checkbox("Visualize cascades", &debug_cascades)) {
		p.debug_cascades = debug_cascades ? 1 : 0;
		changed = true;
	}
	int debug_atlas_value = 0;
	int debug_atlas_cascade = -1;
	XwaRemasterFlight_GetShadowAtlasDebug(&debug_atlas_value, &debug_atlas_cascade);
	bool debug_atlas = debug_atlas_value != 0;
	if (ImGui::Checkbox("Visualize shadow atlas", &debug_atlas)) {
		XwaRemasterFlight_SetShadowAtlasDebug(debug_atlas ? 1 : 0, debug_atlas_cascade);
	}
	if (!debug_atlas) {
		ImGui::BeginDisabled();
	}
	static const char* atlas_view_names[] = { "Whole atlas", "Cascade 0", "Cascade 1", "Cascade 2",
											  "Cascade 3" };
	int atlas_view = debug_atlas_cascade + 1;
	atlas_view = atlas_view < 0 ? 0 : (atlas_view > p.cascade_count ? p.cascade_count : atlas_view);
	if (ImGui::Combo("Atlas view", &atlas_view, atlas_view_names, p.cascade_count + 1)) {
		XwaRemasterFlight_SetShadowAtlasDebug(debug_atlas ? 1 : 0, atlas_view - 1);
	}
	if (!debug_atlas) {
		ImGui::EndDisabled();
	}
	ImGui::TextDisabled("Point-sampled after FSR/TAA; orange lines mark atlas tiles.");
	AeronSceneDirectionalShadowStats stats;
	XwaRemasterFlight_GetShadowStats(&stats);
	ImGui::Separator();
	if (stats.active) {
		ImGui::Text("Active: %u cascades, %u atlas", stats.cascade_count, stats.atlas_size);
		ImGui::Text("Candidates: %u, shadow-only dropped: %u", stats.candidate_count,
					stats.dropped_shadow_only);
		for (uint32_t cascade = 0; cascade < stats.cascade_count; cascade++) {
			ImGui::Text("C%u %.0f..%.0f: %u receivers, %u draws, %u triangles", cascade,
						stats.split_near[cascade], stats.split_far[cascade], stats.receiver_count[cascade],
						stats.caster_count[cascade], stats.triangle_count[cascade]);
			ImGui::Text("    %.2f x %.2f units/texel, %.2fx density", stats.world_units_per_texel_x[cascade],
						stats.world_units_per_texel_y[cascade], stats.texel_density_gain[cascade]);
		}
		if (stats.receiver_local_active) {
			ImGui::Text("Receiver-local %u: %u draws, %u triangles", stats.receiver_local_size,
						stats.receiver_local_caster_count, stats.receiver_local_triangle_count);
		}
	} else {
		ImGui::TextDisabled("Inactive (off, no flight scene, or no key directional light).");
	}
	if (ImGui::Button("Reset to defaults")) {
		XwaRemasterFlight_GetShadowsDefault(&p);
		changed = true;
	}
	ImGui::TextDisabled("Edits apply on the next frame and are NOT persisted —\n"
						"copy values into resources/remaster/config.yaml.");
	ImGui::TextDisabled("GPU group 'Directional shadows' is atlas rendering.\n"
						"With SSAO, opaque sampling is evaluated at half resolution in the SSAO group;\n"
						"transparent and receiver-local sampling remains in the main PBR color pass.");
	if (changed) {
		XwaRemasterFlight_SetShadows(&p);
	}

	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- Hangar lighting ----------------------------------------------- */

static void xwa_tool_hangar_lighting(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(460, 420), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("Hangar Lighting", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}

	XwaFlightHangarLightingParams p;
	XwaRemasterFlight_GetHangarLighting(&p);
	bool changed = false;
	bool enabled = p.enabled != 0;
	if (ImGui::Checkbox("Artificial ceiling light", &enabled)) {
		p.enabled = enabled ? 1 : 0;
		changed = true;
	}
	const XwaSnapshot* snap = XwaSnapshot_Current();
	const bool in_hangar = snap && snap->flight_camera_valid && snap->flight_camera.in_hangar;
	ImGui::TextDisabled("Hangar policy: %s", in_hangar && enabled ? "active" : "inactive");

	if (!enabled) {
		ImGui::BeginDisabled();
	}
	changed |= ImGui::SliderFloat3("Direction (surface to light)", p.direction, -1.0f, 1.0f, "%.3f");
	changed |= ImGui::SliderFloat("Intensity", &p.intensity, 0.0f, 4.0f, "%.2f");
	changed |= ImGui::ColorEdit3("Light color", p.color, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	bool ship_occlusion = XwaRemasterShip_ShipOcclusion() != 0;
	if (ImGui::Checkbox("Ship occlusion", &ship_occlusion)) {
		XwaRemasterShip_SetShipOcclusion(ship_occlusion ? 1 : 0);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"With the hangar's probes: ships take the probe light from what they cover (the floor\n"
			"under and around them, their landing gear, the ceiling's reflections).\n"
			"Replaces \"Shadow from all sides\".");
	}
	bool floor_reflection = XwaRemasterShip_FloorReflection() != 0;
	if (ImGui::Checkbox("Floor reflection", &floor_reflection)) {
		XwaRemasterShip_SetFloorReflection(floor_reflection ? 1 : 0);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"With the hangar's probes, when their package has a reflection plane: the floor reflects\n"
			"the ships standing on it.");
	}
	/* The key light shadow's cut, which ship occlusion replaces. */
	ImGui::BeginDisabled(ship_occlusion);
	bool all_sides = XwaRemasterShip_KeyShadowAllSides() != 0;
	if (ImGui::Checkbox("Shadow from all sides", &all_sides)) {
		XwaRemasterShip_SetKeyShadowAllSides(all_sides ? 1 : 0);
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip(
			"With the hangar's probes and without ship occlusion: in the light's shadow (under a ship),\n"
			"surfaces lose the probe light from the ceiling whichever way they face. Off: only as they face\n"
			"the light.");
	}
	ImGui::EndDisabled();
	ImGui::Separator();
	ImGui::TextUnformatted("Ambient cube (linear)");
	/* An authored hangar package's fill and diffuse cube replace this cube. */
	XwaEnvironmentSelection selection;
	const bool authored_hangar = XwaRemasterEnvironment_LastSelection(&selection, NULL, 0) &&
								 selection.mode == XWA_ENV_MODE_HANGAR && selection.package;
	if (authored_hangar) {
		ImGui::TextDisabled("Replaced by the authored hangar package '%s' (Environment tool).",
							selection.reference ? selection.reference->package : "?");
	}
	ImGui::BeginDisabled(authored_hangar);
	changed |= ImGui::SliderFloat("Ceiling fill (+Z)", &p.ambient_ceiling, 0.0f, 1.0f, "%.3f");
	changed |= ImGui::SliderFloat("Wall bounce", &p.ambient_sides, 0.0f, 1.0f, "%.3f");
	changed |= ImGui::SliderFloat("Floor bounce (-Z)", &p.ambient_floor, 0.0f, 1.0f, "%.3f");
	ImGui::EndDisabled();
	if (!enabled) {
		ImGui::EndDisabled();
	}

	if (ImGui::Button("Reset to defaults")) {
		XwaRemasterFlight_GetHangarLightingDefault(&p);
		changed = true;
	}
	ImGui::TextDisabled("The hangar backdrop receives shadows but does not cast them.\n"
						"Edits are not persisted; copy them into remaster/config.yaml.");
	if (changed) {
		XwaRemasterFlight_SetHangarLighting(&p);
	}

	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- Point lights ---------------------------------------------------- */

static void xwa_tool_point_lights(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(440, 520), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("Point Lights", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}

	XwaFlightPointLightParams p;
	XwaRemasterFlight_GetPointLights(&p);
	bool changed = false;

	bool enabled = p.enabled != 0;
	if (ImGui::Checkbox("Enabled", &enabled)) {
		p.enabled = enabled ? 1 : 0;
		changed = true;
	}
	bool clustered = p.clustered != 0;
	if (ImGui::Checkbox("Clustered (auto)", &clustered)) {
		p.clustered = clustered ? 1 : 0;
		changed = true;
	}
	bool cluster_debug = p.cluster_debug != 0;
	if (ImGui::Checkbox("Cluster occupancy", &cluster_debug)) {
		p.cluster_debug = cluster_debug ? 1 : 0;
		changed = true;
	}
	changed |= ImGui::SliderInt("Depth slices", &p.cluster_depth_slices, 4, 64);
	XwaFlightPointLightStats stats;
	XwaRemasterFlight_GetPointLightStats(&stats);
	ImGui::Text("Candidates %u, valid %u, invalid %u, overflow %u", stats.generated_count, stats.valid_count,
				stats.invalid_count, stats.candidate_overflow_count);
	ImGui::Text("Game: %u culled, %u dropped", stats.culled_count, stats.dropped_count);
	ImGui::Text("Model: %u, %u culled, %u dropped", stats.model_count, stats.model_culled_count,
				stats.model_dropped_count);
	ImGui::Text("Scene %u accepted, %u dropped; %u global", stats.scene.submitted_light_count,
				stats.scene.dropped_light_count, stats.scene.global_light_count);
	ImGui::Text("%s, tile %u, grid %ux%ux%u (%.1f MiB)",
				stats.scene.clustered_active ? "Clustered" : "Brute force", stats.scene.effective_tile_size,
				stats.scene.grid_x, stats.scene.grid_y, stats.scene.grid_z,
				(double)stats.scene.allocated_buffer_bytes / (1024.0 * 1024.0));
	changed |= ImGui::SliderFloat("Match distance (m)", &p.match_distance_m, 1.0f, 200.0f, "%.1f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("The game's lights (lasers, explosions, engines) are inverse-square,\n"
						"as bright as the classic law at this distance.");
	changed |=
		ImGui::SliderFloat("Range scale", &p.range_scale, 0.25f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("Multiplies the distance where a light falls under\n"
						"1/250 of a unit of illuminance and ends.");
	changed |= ImGui::SliderFloat("Model light intensity", &p.model_light_intensity, 0.0f, 0.1f, "%.6f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("Scene illuminance per lux of lights authored in models\n"
						"(candela). 1/683 = 0.001464; 0 = off.");

	ImGui::Separator();
	if (ImGui::Button("Reset to defaults")) {
		XwaRemasterFlight_GetPointLightsDefault(&p);
		changed = true;
	}
	ImGui::TextDisabled("Edits apply on the next frame and are NOT persisted —\n"
						"copy values into resources/remaster/config.yaml\n"
						"under point_lights.");
	if (changed) {
		XwaRemasterFlight_SetPointLights(&p);
	}

	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- HDR & Display --------------------------------------------------- */

static const char* xwa_fmt_name(AeronTextureFormat f) {
	switch (f) {
		case AERON_TEXTURE_FORMAT_RGBA8_UNORM:
			return "RGBA8 UNORM";
		case AERON_TEXTURE_FORMAT_RGBA8_SRGB:
			return "RGBA8 sRGB";
		case AERON_TEXTURE_FORMAT_BGRA8_UNORM:
			return "BGRA8 UNORM";
		case AERON_TEXTURE_FORMAT_BGRA8_SRGB:
			return "BGRA8 sRGB";
		case AERON_TEXTURE_FORMAT_RGBA16_FLOAT:
			return "RGBA16F (HDR)";
		case AERON_TEXTURE_FORMAT_R10G10B10A2_UNORM:
			return "RGB10A2";
		default:
			return "(other)";
	}
}

/* ---- Motion blur ----------------------------------------------------- */

static void xwa_tool_motion_blur(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(420, 240), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("Motion Blur", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}

	XwaFlightMotionBlurParams t;
	XwaRemasterFlight_GetMotionBlur(&t);
	bool changed = false;

	static const char* quality_names[] = { "Off", "Low", "High" };
	changed |= ImGui::Combo("Quality", &t.quality, quality_names, 3);
	changed |= ImGui::SliderFloat("Shutter", &t.shutter, 0.0f, 1.0f, "%.2f");
	ImGui::TextDisabled("Fraction of original 32 ms flight frame (0.5 = 180 degrees).");
	bool cb = t.camera_blur != 0;
	if (ImGui::Checkbox("Camera blur", &cb)) {
		t.camera_blur = cb ? 1 : 0;
		changed = true;
	}
	bool pk = t.pause_keep_blur != 0;
	if (ImGui::Checkbox("Keep blur while paused", &pk)) {
		t.pause_keep_blur = pk ? 1 : 0;
		changed = true;
	}
	bool viz = t.velocity_viz != 0;
	if (ImGui::Checkbox("Velocity viz", &viz)) {
		t.velocity_viz = viz ? 1 : 0;
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetMotionBlur(&t);
	}

	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- Hyperspace tunnel ---------------------------------------------- */

static void xwa_tool_hyperspace_tunnel(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(460, 620), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("Hyperspace Tunnel", &b)) {
		ImGui::End();
		if (!b) {
			XwaRemasterFlight_SetHyperspaceTunnelPreview(0);
		}
		*open = b ? 1 : 0;
		return;
	}

	bool preview = XwaRemasterFlight_HyperspaceTunnelPreviewEnabled() != 0;
	if (ImGui::Checkbox("Preview in flight", &preview)) {
		XwaRemasterFlight_SetHyperspaceTunnelPreview(preview ? 1 : 0);
	}
	ImGui::TextDisabled("Replaces the normal 3D flight scene without changing simulation state.\n"
						"Map mode and real hyperspace retain precedence; the cockpit remains visible.");

	ImGui::Separator();
	XwaFlightHyperspaceTunnelParams p;
	XwaRemasterFlight_GetHyperspaceTunnel(&p);
	bool changed = false;
	changed |= ImGui::SliderFloat("Travel speed", &p.travel_speed, 0.01f, 64.0f, "%.2f",
								  ImGuiSliderFlags_Logarithmic);
	changed |= ImGui::SliderFloat("Rotation speed", &p.rotation_speed, -8.0f, 8.0f, "%.3f");
	changed |=
		ImGui::SliderFloat("Noise scale", &p.noise_scale, 0.125f, 8.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
	changed |= ImGui::SliderFloat("Brightness", &p.brightness, 0.0f, 8.0f, "%.2f");
	changed |= ImGui::SliderFloat("Highlight strength", &p.highlight_strength, 0.0f, 8.0f, "%.2f");
	ImGui::TextDisabled("Scales cyan-white HDR highlights, the cap light, and their bloom.");
	changed |= ImGui::SliderFloat("Focal length", &p.focal_length, 0.25f, 4.0f, "%.3f");
	ImGui::TextDisabled("Larger focal lengths narrow the apparent opening.");
	changed |= ImGui::SliderFloat("Axial twist", &p.twist, -2.0f, 2.0f, "%.3f");
	changed |=
		ImGui::SliderFloat("Cap radius", &p.cap_radius, 0.001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
	changed |=
		ImGui::SliderFloat("Cap falloff", &p.cap_falloff, 0.1f, 64.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("Controls the full-strength core radius and exponential halo falloff.");
	changed |=
		ImGui::ColorEdit3("Dark color", p.dark_color, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	changed |=
		ImGui::ColorEdit3("Body color", p.body_color, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	changed |= ImGui::ColorEdit3("Highlight color", p.highlight_color,
								 ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	changed |=
		ImGui::ColorEdit3("Cap color", p.cap_color, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

	ImGui::Separator();
	ImGui::TextUnformatted("Mesh lighting");
	changed |= ImGui::SliderFloat("Mesh ambient strength", &p.mesh_ambient_strength, 0.0f, 4.0f, "%.2f");
	changed |=
		ImGui::SliderFloat("Environment roughness", &p.mesh_environment_roughness, 0.0f, 1.0f, "%.2f");
	changed |= ImGui::SliderFloat("Mesh key strength", &p.mesh_key_strength, 0.0f, 4.0f, "%.2f");
	ImGui::TextDisabled("Ambient controls diffuse tunnel color on the cockpit.\n"
						"Roughness controls angular blur: 0 is sharp, 1 is diffuse.\n"
						"Key controls cap-colored directional/specular response.");

	ImGui::Separator();
	if (ImGui::Button("Reset to YAML defaults")) {
		XwaRemasterFlight_GetHyperspaceTunnelDefault(&p);
		changed = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Copy YAML")) {
		char yaml[1280];
		std::snprintf(yaml, sizeof yaml,
					  "hyperspace_tunnel:\n"
					  "  travel_speed: %.4g\n"
					  "  rotation_speed: %.4g\n"
					  "  noise_scale: %.4g\n"
					  "  brightness: %.4g\n"
					  "  highlight_strength: %.4g\n"
					  "  focal_length: %.4g\n"
					  "  twist: %.4g\n"
					  "  cap_radius: %.4g\n"
					  "  cap_falloff: %.4g\n"
					  "  mesh_ambient_strength: %.4g\n"
					  "  mesh_environment_roughness: %.4g\n"
					  "  mesh_key_strength: %.4g\n"
					  "  dark_color_r: %.4g\n"
					  "  dark_color_g: %.4g\n"
					  "  dark_color_b: %.4g\n"
					  "  body_color_r: %.4g\n"
					  "  body_color_g: %.4g\n"
					  "  body_color_b: %.4g\n"
					  "  highlight_color_r: %.4g\n"
					  "  highlight_color_g: %.4g\n"
					  "  highlight_color_b: %.4g\n"
					  "  cap_color_r: %.4g\n"
					  "  cap_color_g: %.4g\n"
					  "  cap_color_b: %.4g\n",
					  (double)p.travel_speed, (double)p.rotation_speed, (double)p.noise_scale,
					  (double)p.brightness, (double)p.highlight_strength, (double)p.focal_length,
					  (double)p.twist, (double)p.cap_radius, (double)p.cap_falloff,
					  (double)p.mesh_ambient_strength, (double)p.mesh_environment_roughness,
					  (double)p.mesh_key_strength, (double)p.dark_color[0],
					  (double)p.dark_color[1], (double)p.dark_color[2], (double)p.body_color[0],
					  (double)p.body_color[1], (double)p.body_color[2], (double)p.highlight_color[0],
					  (double)p.highlight_color[1], (double)p.highlight_color[2], (double)p.cap_color[0],
					  (double)p.cap_color[1], (double)p.cap_color[2]);
		ImGui::SetClipboardText(yaml);
	}
	ImGui::TextDisabled("Edits apply immediately and are not persisted.");
	if (changed) {
		XwaRemasterFlight_SetHyperspaceTunnel(&p);
	}

	ImGui::End();
	if (!b) {
		XwaRemasterFlight_SetHyperspaceTunnelPreview(0);
	}
	*open = b ? 1 : 0;
}

/* ---- FSR 3.1.4 ------------------------------------------------------ */

static void xwa_tool_fsr(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(440, 400), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("FSR 3.1.4", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}

	XwaFlightTemporalParams params;
	XwaRemasterFlight_GetTemporal(&params);
	bool changed = false;
	static const char* mode_names[] = { "Off", "Native AA", "Quality", "Balanced", "Performance" };
	int mode = (int)params.mode;
	if (ImGui::Combo("Mode", &mode, mode_names, (int)(sizeof mode_names / sizeof mode_names[0]))) {
		params.mode = (AeronTemporalMode)mode;
		changed = true;
	}
	changed |= ImGui::SliderFloat("Sharpness", &params.sharpness, 0.0f, 1.0f, "%.2f");
	bool debug_view = params.debug_view != 0;
	if (ImGui::Checkbox("AMD FSR debug view", &debug_view)) {
		params.debug_view = debug_view ? 1 : 0;
		changed = true;
	}
	ImGui::TextDisabled("Composite view of FSR motion, depth, disocclusion,\n"
						"reactiveness and detail-protection data.");
	if (changed) {
		XwaRemasterFlight_SetTemporal(&params);
	}

	XwaFlightTemporalStats stats;
	XwaRemasterFlight_GetTemporalStats(&stats);
	ImGui::Separator();
	ImGui::TextUnformatted("Dimensions");
	if (stats.output_width > 0 && stats.output_height > 0) {
		ImGui::Text("Render : %d x %d", stats.render_width, stats.render_height);
		ImGui::Text("Output : %d x %d", stats.output_width, stats.output_height);
	} else {
		ImGui::TextDisabled("Flight scene inactive");
	}

	ImGui::Separator();
	ImGui::TextUnformatted("History reset");
	if (stats.history_reset_active) {
		ImGui::Text("Active for %u consecutive frame%s", stats.history_reset_consecutive_frames,
					stats.history_reset_consecutive_frames == 1 ? "" : "s");
		ImGui::TextWrapped("Reasons: %s",
						   stats.history_reset_reasons[0] ? stats.history_reset_reasons : "unknown reason");
	} else {
		ImGui::TextDisabled("Inactive");
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Selected shader profile");
	if (stats.profile_available) {
		ImGui::Text("Profile : %s", stats.profile.profile_name);
		ImGui::Text("Backend : %s", stats.profile.backend_driver);
		ImGui::Text("Precision: %s", stats.profile.fp16 ? "FP16" : "FP32");
		ImGui::Text("SPD      : %s", stats.profile.wave_spd ? "native wave" : "scalar");
		const bool direct_output =
			stats.profile.direct_history_output && params.sharpness <= 0.0f && !params.debug_view;
		ImGui::Text("Output   : %s", direct_output ? "direct internal history" : "external target");
	} else {
		ImGui::TextDisabled("Unavailable while FSR is inactive");
	}

	ImGui::TextDisabled("Runtime edits are not persisted to remaster/config.yaml.");
	ImGui::End();
	*open = b ? 1 : 0;
}

static void xwa_tool_hdr(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(420, 440), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("HDR & Display", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}

	const AeronHdrOutputStatus hdr_status = Aeron_OutputHdrStatus();
	const bool hdr_active = hdr_status == AERON_HDR_OUTPUT_ACTIVE;

	ImGui::TextUnformatted("Output");
	bool want_hdr = XwaRemaster_GetHdrDesired() != 0;
	if (ImGui::Checkbox("HDR output (when available)", &want_hdr)) {
		/* Applied at the next frame boundary — the swapchain flip
		 * rebuilds pipelines and must not run mid overlay recording. */
		XwaRemaster_SetHdrDesired(want_hdr ? 1 : 0);
	}
	if (hdr_status == AERON_HDR_OUTPUT_DISPLAY_SDR) {
		ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1),
						   "Display is not in HDR mode — SDR composition. Enabling HDR in the OS "
						   "switches over without a restart.");
	} else if (hdr_status == AERON_HDR_OUTPUT_UNSUPPORTED) {
		ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1),
						   "Display is in HDR mode but the '%s' backend refused the extended-linear "
						   "composition — SDR composition.",
						   Aeron_RenderDriverName());
	}
	float paper_white = Aeron_OutputPaperWhiteNits();
	bool auto_white = paper_white <= 0.0f;
	if (ImGui::Checkbox("Auto paper white (OS SDR white)", &auto_white)) {
		Aeron_SetOutputPaperWhiteNits(auto_white ? 0.0f : 200.0f);
		paper_white = Aeron_OutputPaperWhiteNits();
	}
	if (!auto_white) {
		if (ImGui::SliderFloat("Paper white", &paper_white, 80.0f, 400.0f, "%.0f nits")) {
			Aeron_SetOutputPaperWhiteNits(paper_white);
		}
	}
	ImGui::Text("SDR white %.2fx, headroom %.2fx", (double)Aeron_OutputSdrWhiteLevel(),
				(double)Aeron_OutputHdrHeadroom());

	static const char* vsync_names[] = { "Full refresh", "Half refresh" };
	int vsync_rate = Aeron_PresentationVsyncDivisor() - 1;
	if (ImGui::Combo("VSync rate", &vsync_rate, vsync_names, 2)) {
		Aeron_SetPresentationVsyncDivisor(vsync_rate + 1);
	}

	ImGui::Separator();
	ImGui::TextUnformatted("SDR content decode (HDR composition)");
	float content_gamma = Aeron_OutputSdrContentGamma();
	bool power_decode = content_gamma > 0.0f;
	if (ImGui::RadioButton("Piecewise sRGB", !power_decode) && power_decode) {
		Aeron_SetOutputSdrContentGamma(0.0f);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Power gamma", power_decode) && !power_decode) {
		Aeron_SetOutputSdrContentGamma(2.2f);
	}
	if (power_decode) {
		if (ImGui::SliderFloat("##sdrcontentgamma", &content_gamma, 1.8f, 2.6f, "%.2f")) {
			Aeron_SetOutputSdrContentGamma(content_gamma);
		}
	}
	ImGui::TextDisabled("Decode curve for frontend/classic (display-referred) layers.");
	if (!hdr_active) {
		ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1),
						   "SDR composition active — the piecewise curve is in use regardless.");
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Tonemap operator");
	int op = AeronScenePresent_TonemapOp();
	int new_op = op;
	ImGui::RadioButton("ACES", &new_op, AERON_SCENE_TONEMAP_ACES);
	ImGui::SameLine();
	ImGui::RadioButton("AGX (parametric)", &new_op, AERON_SCENE_TONEMAP_AGX_PARAMETRIC);
	if (new_op != op) {
		AeronScenePresent_SetTonemapOp(new_op);
	}
	int agx_look = AeronScenePresent_AgxLook();
	int new_agx_look = agx_look;
	ImGui::TextUnformatted("AgX look");
	ImGui::RadioButton("Base", &new_agx_look, AERON_SCENE_AGX_LOOK_BASE);
	ImGui::SameLine();
	ImGui::RadioButton("Punchy", &new_agx_look, AERON_SCENE_AGX_LOOK_PUNCHY);
	if (new_agx_look != agx_look) {
		AeronScenePresent_SetAgxLook(new_agx_look);
	}
	if (new_agx_look == AERON_SCENE_AGX_LOOK_PUNCHY) {
		float punchy_power = AeronScenePresent_AgxPunchyPower();
		if (ImGui::SliderFloat("Punchy power", &punchy_power, 0.5f, 2.0f, "%.2f")) {
			AeronScenePresent_SetAgxPunchyPower(punchy_power);
		}
		float punchy_saturation = AeronScenePresent_AgxPunchySaturation();
		if (ImGui::SliderFloat("Punchy saturation", &punchy_saturation, 0.0f, 2.0f,
						   "%.2f")) {
			AeronScenePresent_SetAgxPunchySaturation(punchy_saturation);
		}
	}

	ImGui::TextUnformatted("ACES pre-exposure");
	float aces_exp = AeronScenePresent_AcesExposure();
	if (ImGui::SliderFloat("##acesexp", &aces_exp, 1.0f, 3.0f, "%.2fx")) {
		AeronScenePresent_SetAcesExposure(aces_exp);
	}
	ImGui::TextUnformatted("Parametric AgX EOTF exponent");
	float gamma = AeronScenePresent_EotfExponent();
	if (ImGui::SliderFloat("##eotf", &gamma, 1.8f, 2.6f, "%.3f")) {
		AeronScenePresent_SetEotfExponent(gamma);
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Bloom present kernel");
	int bk = AeronScenePresent_BloomKernel();
	int new_bk = bk;
	ImGui::RadioButton("1 tap", &new_bk, AERON_SCENE_BLOOM_KERNEL_1_TAP);
	ImGui::SameLine();
	ImGui::RadioButton("4 taps", &new_bk, AERON_SCENE_BLOOM_KERNEL_4_TAP);
	if (new_bk != bk) {
		AeronScenePresent_SetBloomKernel(new_bk);
	}
	float bloom_i = AeronSceneBloom_Intensity();
	if (ImGui::SliderFloat("Bloom intensity", &bloom_i, 0.0f, 0.25f, "%.3f")) {
		AeronSceneBloom_SetIntensity(bloom_i);
	}
	ImGui::TextDisabled("Fraction of the light scattered; 0 disables bloom (chain is skipped entirely).");
	float emissive_scale = AeronScene_EmissiveScale();
	if (ImGui::SliderFloat("Emissive scale", &emissive_scale, 0.25f, 32.0f, "%.2f", ImGuiSliderFlags_Logarithmic)) {
		AeronScene_SetEmissiveScale(emissive_scale);
	}
	ImGui::TextDisabled("Scales all emitted light: illumination, screens, lasers, glows, explosions.");

	ImGui::Separator();
	ImGui::TextUnformatted("Diagnostics");
	ImGui::Text("HDR available    : %s", Aeron_OutputSupportsHdr() ? "yes" : "no");
	ImGui::Text("HDR output       : %s%s", Aeron_OutputHdrStatusName(hdr_status),
				hdr_active ? " (HDR_EXTENDED_LINEAR)" : " (SDR)");
	ImGui::Text("Display refresh  : %.2f Hz", Aeron_DisplayRefreshRate());
	ImGui::Text("Presentation rate: %.2f fps", Aeron_PresentationRate());
	ImGui::Text("Display headroom : %.2fx (drives the HDR peak scale)", (double)Aeron_OutputHdrHeadroom());
	ImGui::Text("Swapchain format : %s", xwa_fmt_name(Aeron_SwapchainFormat()));

	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- material animation ---------------------------------------------- */

static void xwa_tool_material_animation(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(360, 220), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("Material Animation", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}
	XwaFlightMaterialAnimOverride o;
	XwaRemasterFlight_GetMaterialAnimOverride(&o);
	bool enabled = o.enabled != 0, all = o.all_objects != 0;
	ImGui::Checkbox("Override", &enabled);
	ImGui::SameLine();
	ImGui::Checkbox("All objects (else the target)", &all);
	static const char* const stages[] = { "None", "75 %", "50 %", "25 %" };
	static const char* const energies[] = { "None", "Shields down", "Tractor beam", "Jamming beam" };
	int stage = (int)o.damage_stage, energy = (int)o.energy;
	ImGui::Combo("Hull damage", &stage, stages, 4);
	ImGui::Combo("Energy effect", &energy, energies, 4);
	unsigned int mask = o.state_mask;
	ImGui::InputScalar("State mask", ImGuiDataType_U32, &mask, nullptr, nullptr, "%08X",
					   ImGuiInputTextFlags_CharsHexadecimal);
	o.enabled = enabled ? 1 : 0;
	o.all_objects = all ? 1 : 0;
	o.damage_stage = (uint32_t)stage;
	o.energy = (uint32_t)energy;
	o.state_mask = mask & 0x7FFFFFFFu;
	XwaRemasterFlight_SetMaterialAnimOverride(&o);
	/* The state names of the converted models, by bit. */
	static const char* const states[31] = { "threat.laser",
											"threat.turret",
											"threat.beam",
											"threat.missile_locking",
											"threat.missile_locked",
											"cannon.1",
											"cannon.2",
											"cannon.3",
											"cannon.4",
											"cannon.5",
											"cannon.6",
											"cannon.7",
											"cannon.8",
											"broken.cmd",
											"broken.laser_ion",
											"broken.beam_weapon",
											"broken.shields",
											"broken.throttle",
											"broken.sensors",
											"broken.laser_recharge",
											"broken.engine_power",
											"broken.shield_recharge",
											"broken.beam_recharge",
											"hull.25",
											"hull.50",
											"hull.75",
											"laser_lock",
											"warhead.locking",
											"warhead.locked",
											"hangar",
											"target" };
	const uint32_t player = XwaRemasterFlight_GetPlayerMaterialStates();
	ImGui::Separator();
	ImGui::Text("Player states: %08X", player);
	for (int i = 0; i < 31; i++) {
		if (player >> i & 1u) {
			ImGui::BulletText("%s", states[i]);
		}
	}
	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- registration ---------------------------------------------------- */

static void xwa_tool_hud_snapshot(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(520, 500), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("HUD Snapshot", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}
	const XwaSnapshot* snap = XwaSnapshot_Current();
	if (!snap) {
		ImGui::TextDisabled("No committed snapshot.");
	} else {
		const XwaHudState& h = snap->hud;
		const XwaHudLayout* layout = XwaRemasterHud_Layout();
		ImGui::Text("tick=%llu valid=%u classic=%u epoch=%u", (unsigned long long)snap->tick_index, h.valid,
					h.classic_frame_valid, h.classic_frame_epoch);
		ImGui::Text("player=%u:%u enabled=%u elements=%03x modes=%04x", h.player_slot, h.player_signature,
					h.hud_enabled, h.element_enabled_mask, h.mode_flags);
		if (layout) {
			const XwaHudLayoutProfile* profile =
				XwaRemasterHudLayout_Profile(layout, layout->default_profile);
			ImGui::Text("layout ref=%dx%d source=original-logical default=%s generation=%u",
						profile ? profile->reference_w : 0, profile ? profile->reference_h : 0,
						profile ? profile->name : "<invalid>", layout->generation);
		}
		ImGui::Text("MFD active=%u pages=%u/%u/%u enabled=%u/%u/%u", h.mfd_active, h.mfd_page[0],
					h.mfd_page[1], h.mfd_page[2], h.mfd_enabled[0], h.mfd_enabled[1], h.mfd_enabled[2]);
		ImGui::Separator();
		const XwaHudInstruments& i = h.instruments;
		ImGui::Text("craft type=%u model=%u speed=%u throttle=%u output=%u", i.player_object_type,
					i.player_model_index, i.speed, i.throttle_speed, i.engine_output_scale);
		ImGui::Text("hull=%d/%d shields=%d/%d max=%d", i.hull_damage, i.hull_max, i.shield_front,
					i.shield_rear, i.shield_max);
		ImGui::Text("features installed=%04x active=%04x systems=%04x working=%04x", i.installed_features,
					i.active_features, i.system_flags, i.working_subsystems);
		ImGui::Text("laser slots=%u warhead launchers=%u beam=%u:%u cm=%u:%u", i.laser_slot_count,
					i.warhead_launcher_count, i.beam_type, i.beam_level, i.cm_type, i.cm_count);
		ImGui::Separator();
		ImGui::Text("target valid=%u id=%u:%u component=%u", h.target.valid, h.target.slot,
					h.target.signature, h.target.selected_component);
		ImGui::Text("name='%s' status='%s'", h.target.name, h.target.status);
		ImGui::Text("distance=%u.%02u shield/system/hull=%u/%u/%u", h.target.distance_whole,
					h.target.distance_frac, h.target.shield_pct, h.target.system_pct, h.target.hull_pct);
		const XwaHudCrt& crt = h.crt;
		ImGui::Text("CRT visible/self/map/marker=%u/%u/%u/%u target=%u:%u component=%u", crt.visible,
					crt.self_view, crt.map_view, crt.component_marker_visible, crt.target_slot,
					crt.target_signature, crt.selected_component);
		ImGui::Text("CRT viewport=%ux%u projection=%d,%u exclude=%u,%u", crt.classic_viewport_w,
					crt.classic_viewport_h, crt.proj_scale, crt.proj_aspect_y_q16,
					crt.projectile_exclude_slots[0], crt.projectile_exclude_slots[1]);
		ImGui::Text("CRT distance=%d backstep=%d,%d,%d focus=%d,%d,%d", crt.camera_distance,
					crt.camera_back_step[0], crt.camera_back_step[1], crt.camera_back_step[2],
					crt.component_focus[0], crt.component_focus[1], crt.component_focus[2]);
		ImGui::Text("CRT rows=%d,%d,%d / %d,%d,%d / %d,%d,%d", crt.camera_rows_q15[0], crt.camera_rows_q15[1],
					crt.camera_rows_q15[2], crt.camera_rows_q15[3], crt.camera_rows_q15[4],
					crt.camera_rows_q15[5], crt.camera_rows_q15[6], crt.camera_rows_q15[7],
					crt.camera_rows_q15[8]);
		ImGui::Separator();
		ImGui::Text("resolved panes=%u glyphs=%u dropped=%u scope errors=%u radar=%u radius=%u boxes=%u",
					h.pane_count, h.glyph_count, h.glyph_dropped, h.pane_scope_errors, h.radar_blip_count,
					h.radar_classic_radius, h.target_box_count);
		ImGui::Text("radar target marker=%u radar=%u local=%d,%d", h.radar_target_marker_visible,
					h.radar_target_marker_radar, h.radar_target_marker_local_x,
					h.radar_target_marker_local_y);
		XwaRemasterHudVisibility v;
		XwaRemasterHud_BuildVisibility(&h, &v);
		ImGui::Text("visible fixed/radar/blips/power/charge=%u/%u/%u/%u/%u", v.fixed_frames, v.radars,
					v.radar_blips, v.power, v.charge);
		ImGui::Text("visible shield/beam/reticle/threat/arrow=%u/%u/%u/%u/%u", v.shield_hull, v.beam,
					v.reticle, v.threats, v.target_arrow);
		ImGui::Text("visible CMD/MFD/PiP/film/panes=%u/%u/%u/%u/%u", v.cmd, v.mfd_frames, v.cmd_pip,
					v.film_mfds, v.pane_glyphs);
		const XwaRemasterHudPreparedAssets* prepared = XwaRemasterHud_PreparedAssets();
		ImGui::Separator();
		ImGui::Text("assets gen layout/bundle=%u/%u cache=%u violations=%u", prepared->layout_generation,
					prepared->bundle_generation, prepared->cache_entries, prepared->render_phase_violations);
		ImGui::Text("assets requested/resolved/missing=%08x/%08x/%08x", prepared->requested_asset_mask,
					prepared->resolved_asset_mask, prepared->missing_asset_mask);
		ImGui::Text("fonts requested/resolved/missing=%02x/%02x/%02x", prepared->requested_font_mask,
					prepared->resolved_font_mask, prepared->missing_font_mask);
		const XwaHudPreparedDrawState* draws = XwaRemasterHud_PreparedDrawState();
		ImGui::Text("fixed draws profile/count/dropped=%u/%u/%u", draws->profile, draws->record_count,
					draws->dropped_records);
		const XwaHudPreparedBoxState* boxes = XwaRemasterHudBoxes_Prepared();
		ImGui::Text("target boxes profile/count/dropped=%u/%u/%u valid=%u viewport=%d,%d %dx%d",
					boxes->profile, boxes->box_count, boxes->dropped_boxes, boxes->valid,
					boxes->camera_viewport.x, boxes->camera_viewport.y, boxes->camera_viewport.width,
					boxes->camera_viewport.height);
		const XwaHudCmdPreparedState* cmd_pip = XwaRemasterHudCmd_Prepared();
		ImGui::Text("CMD PiP valid=%u target=%u:%u internal=%ux%u classic=%ux%u marker=%u at %.3f,%.3f",
					cmd_pip->valid, cmd_pip->target_slot, cmd_pip->target_signature, cmd_pip->internal_w,
					cmd_pip->internal_h, cmd_pip->classic_viewport_w, cmd_pip->classic_viewport_h,
					cmd_pip->marker_visible, cmd_pip->marker_x, cmd_pip->marker_y);
		for (uint16_t box_idx = 0; box_idx < boxes->box_count; box_idx++) {
			const XwaHudPreparedBox& box = boxes->boxes[box_idx];
			ImGui::Text("box %u id=%u:%u component=%u color=%u selected/readout/layer=%u/%u/%u "
						"rect=%.1f,%.1f %.1fx%.1f",
						box_idx, box.slot, box.signature, box.component, box.color_index, box.selected,
						box.readout, box.layer, box.x_px, box.y_px, box.w_px, box.h_px);
		}
	}
	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- Flight input trace --------------------------------------------- */

static void xwa_tool_flight_input(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(470, 190), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("Flight Input", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}

	bool enabled = FlightDebug_JoystickTraceEnabled() != 0;
	if (ImGui::Checkbox("Log joystick integration trace", &enabled)) {
		FlightDebug_SetJoystickTraceEnabled(enabled ? 1 : 0);
	}
	ImGui::TextDisabled("One JOYTRACE log line per applied local pilot input.\n"
						"Includes SDL, WinMM, packed, smoothed, step and angle values.");

	ImGui::Separator();
	bool gimbal_fix = FlightDebug_GimbalLockFixEnabled() != 0;
	if (ImGui::Checkbox("High-precision orientation / gimbal fix", &gimbal_fix)) {
		FlightDebug_SetGimbalLockFixEnabled(gimbal_fix ? 1 : 0);
	}
	ImGui::TextDisabled("Off runs the original Q15 USER_calcdeltapitch path.\n"
						"Changes take effect on the next simulation tick.");

	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- Cockpit eye ---------------------------------------------------- */

static void xwa_tool_cockpit_eye(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(430, 190), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("Cockpit Eye", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}

	static int edit[3];
	static int edit_model = -1;
	int model = -1;
	int eye[3];
	if (XwaCockpitEye_GetPlayer(&model, eye)) {
		if (model != edit_model) {
			edit_model = model;
			memcpy(edit, eye, sizeof edit);
		}
		ImGui::Text("Player model %d: X=%d Y=%d Z=%d", model, eye[0], eye[1], eye[2]);
		ImGui::InputInt3("X, Y, Z", edit);
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
	} else {
		edit_model = -1;
		ImGui::TextDisabled("No player craft.");
	}
	ImGui::TextDisabled("Craft-table cockpit eye (+0x238), the values XWAU's CockpitPovX/Y/Z\n"
						"set. Edits last until the model is reloaded. A GLB sets it with one\n"
						"Cockpit (type 31) hardpoint on the craft model.");

	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- Dynamic cockpit ----------------------------------------------- */

static void xwa_tool_dynamic_cockpit(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_FirstUseEver);
	bool b = *open != 0;
	if (!ImGui::Begin("Dynamic Cockpit", &b)) {
		ImGui::End();
		*open = b ? 1 : 0;
		return;
	}
	XwaRemasterDcSettings settings;
	XwaRemasterDc_GetSettings(&settings);
	bool enabled = settings.enabled != 0, classic = settings.classic_hud != 0,
		 holograms = settings.holograms != 0;
	bool changed = ImGui::Checkbox("Enabled", &enabled);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Classic HUD", &classic);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Holograms", &holograms);
	changed |=
		ImGui::SliderFloat("Display strength", &settings.display_emissive_strength, 0.0f, 8.0f, "%.2f");
	changed |=
		ImGui::SliderFloat("Hologram strength", &settings.hologram_emissive_strength, 0.0f, 8.0f, "%.2f");
	if (changed) {
		settings.enabled = enabled ? 1 : 0;
		settings.classic_hud = classic ? 1 : 0;
		settings.holograms = holograms ? 1 : 0;
		XwaRemasterDc_SetSettings(&settings);
	}
	ImGui::Text("Active: %s", XwaRemasterDc_Active() ? "yes (cockpit with screens)" : "no");
	ImGui::Separator();
	static int highlight = -1;
	bool boxes = XwaRemasterDc_DebugBoxes() != 0;
	if (ImGui::Checkbox("Show boxes", &boxes)) {
		XwaRemasterDc_SetDebugBoxes(boxes ? 1 : 0, highlight);
	}
	ImGui::TextDisabled("Sources green (selected yellow), erasable HUD regions red, in render pixels.");
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
			if (ImGui::Selectable(XwaRemasterDc_SourceName(i), highlight == (int)i,
								  ImGuiSelectableFlags_SpanAllColumns)) {
				highlight = highlight == (int)i ? -1 : (int)i;
				XwaRemasterDc_SetDebugBoxes(XwaRemasterDc_DebugBoxes(), highlight);
			}
			ImGui::TableNextColumn();
			if (placed)
				ImGui::Text("%.1f, %.1f, %.1f, %.1f", box[0], box[1], box[2], box[3]);
			else
				ImGui::TextDisabled("-");
		}
		ImGui::EndTable();
	}
	ImGui::SeparatorText("Regions");
	for (unsigned i = 0; i < XWA_DC_REGION_COUNT; ++i) {
		float box[4];
		if (XwaRemasterDc_RegionBox(i, box))
			ImGui::Text("%2u %-26s %.1f, %.1f, %.1f, %.1f", i, XwaRemasterDc_RegionName(i), box[0], box[1],
						box[2], box[3]);
		else
			ImGui::TextDisabled("%2u %-26s -", i, XwaRemasterDc_RegionName(i));
	}
	ImGui::End();
	*open = b ? 1 : 0;
}

extern "C" void XwaRemasterDebugTools_EnvironmentRows(void* user) {
	(void)user;
	bool authored = XwaRemasterEnvironment_AuthoredEnvironments() != 0;
	if (ImGui::Checkbox("Authored environments", &authored)) {
		XwaRemasterEnvironment_SetAuthoredEnvironments(authored ? 1 : 0);
	}
	ImGui::TextDisabled(
		"Off: the hangar and flight regions use the classic path (for comparison and profiling).");
	XwaEnvironmentSelection selection;
	char mission[128];
	if (XwaRemasterEnvironment_LastSelection(&selection, mission, sizeof mission)) {
		ImGui::Text("Mission: %s, region %u", mission[0] ? mission : "(none)", (unsigned)selection.region);
		/* The normalized key is what the manifest's xwa.missions entry needs. */
		char key[XWA_ENV_MISSION_KEY_MAX];
		if (mission[0] && XwaRemasterEnvironment_NormalizeMissionKey(mission, key, sizeof key)) {
			ImGui::Text("Manifest key: %s", key);
			if (ImGui::SmallButton("Copy key")) {
				ImGui::SetClipboardText(key);
			}
			const char* prepared = XwaRemasterEnvironment_PreparedMissionKey();
			if (strcmp(prepared, key) != 0) {
				ImGui::Text("Prepared key: %s", prepared[0] ? prepared : "(none)");
			}
		}
		ImGui::Text("Selection: %s%s%s", XwaRemasterEnvironment_ModeName(selection.mode),
					selection.reference ? ", package " : "",
					selection.reference ? selection.reference->package : "");
		if (selection.fallback != XWA_ENV_FALLBACK_NONE) {
			ImGui::Text("Fallback: %s", XwaRemasterEnvironment_FallbackName(selection.fallback));
		}
	}
	const XwaEnvironmentProbeSet* sets = nullptr;
	const uint32_t set_count = XwaRemasterEnvironment_FrameProbeSets(&sets);
	ImGui::Text("Probe sets: %u", set_count);
	for (uint32_t i = 0; i < set_count; ++i) {
		ImGui::BulletText("%s, object %u: %s", sets[i].model, (unsigned)sets[i].slot, sets[i].package);
	}
	bool show_probes = XwaRemasterEnvironment_ShowProbes() != 0;
	if (ImGui::Checkbox("Show probes", &show_probes)) {
		XwaRemasterEnvironment_SetShowProbes(show_probes ? 1 : 0);
	}
	ImGui::SetItemTooltip("Outlines of each probe's influence (dimmer: where it applies fully), its parallax "
						  "volume (grey), the light grid's box (white) and its points within 50 m.");

	/* Capture the current region's HD sky for environment_bake --capture. */
	ImGui::SeparatorText("Capture");
	static const int face_sizes[] = { 1024, 2048, 4096 };
	static int face_index = 1;
	static float sun_radius = XWA_ENV_CAPTURE_DEFAULT_SUN_RADIUS;
	ImGui::Combo("Face size", &face_index,
				 "1024\0"
				 "2048\0"
				 "4096\0");
	ImGui::SliderFloat("Sun angular radius", &sun_radius, 0.01f, 5.0f, "%.2f deg");
	const char* reason = nullptr;
	const bool available = XwaRemasterEnvCapture_Available(XwaSnapshot_Current(), &reason) != 0;
	const bool pending = XwaRemasterEnvCapture_Pending() != 0;
	ImGui::BeginDisabled(!available || pending);
	if (ImGui::Button(pending ? "Capture pending" : "Capture region")) {
		XwaRemasterEnvCapture_Request(face_sizes[face_index], sun_radius);
	}
	ImGui::EndDisabled();
	if (!available && reason) {
		ImGui::SameLine();
		ImGui::TextDisabled("(%s)", reason);
	}
	if (pending) {
		ImGui::TextDisabled("Runs on the next HD flight frame.");
	}
	const char* status = XwaRemasterEnvCapture_Status();
	if (status[0]) {
		ImGui::TextWrapped("Last capture: %s", status);
	}
}

extern "C" void XwaRemasterDebugTools_Register(void) {
	if (!Aeron_DebugUiAvailable()) {
		return;
	}
	Aeron_DebugRegisterTool("PBR — Global", xwa_tool_pbr, nullptr);
	Aeron_DebugRegisterTool("SSAO", xwa_tool_ssao, nullptr);
	Aeron_DebugRegisterTool("Directional Shadows", xwa_tool_shadows, nullptr);
	Aeron_DebugRegisterTool("Hangar Lighting", xwa_tool_hangar_lighting, nullptr);
	Aeron_DebugRegisterTool("Point Lights", xwa_tool_point_lights, nullptr);
	Aeron_DebugRegisterTool("Motion Blur", xwa_tool_motion_blur, nullptr);
	Aeron_DebugRegisterTool("Hyperspace Tunnel", xwa_tool_hyperspace_tunnel, nullptr);
	Aeron_DebugRegisterTool("FSR 3.1.4", xwa_tool_fsr, nullptr);
	Aeron_DebugRegisterTool("HDR & Display", xwa_tool_hdr, nullptr);
	Aeron_DebugRegisterTool("HUD Snapshot", xwa_tool_hud_snapshot, nullptr);
	Aeron_DebugRegisterTool("Flight Input", xwa_tool_flight_input, nullptr);
	Aeron_DebugRegisterTool("Cockpit Eye", xwa_tool_cockpit_eye, nullptr);
	Aeron_DebugRegisterTool("Dynamic Cockpit", xwa_tool_dynamic_cockpit, nullptr);
	Aeron_DebugRegisterTool("Material Animation", xwa_tool_material_animation, nullptr);
}
