/*
 * Lighting menu: the classic light calibration, the probes and the captured
 * lighting, point lights, shadows, SSAO, the hangar, and the XWA rows of
 * Aeron's Environment window.
 */

#include "xwa_remaster/debug/debug_internal.h"
#include "xwa_remaster/debug_tools.h"

#include "xwa_remaster/captured_lighting.h"
#include "xwa_remaster/env_capture.h"
#include "xwa_remaster/environment.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/scene_lights.h"
#include "xwa_remaster/ship.h"

#include "aeron/scene/environment.h"
#include "aeron/scene/scene3d.h"
#include "xwa_runtime/models/cockpit_lights.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <cstdio>
#include <cstring>

static const ImGuiColorEditFlags k_hdr_color = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR;

/* ---- Calibration ---------------------------------------------------------- */

static void xwa_window_calibration(void*) {
	float illuminance = XwaRemaster_ClassicLightIlluminance();
	if (ImGui::SliderFloat("Classic light illuminance", &illuminance, 0.0f, 2000.0f, "%.1f W/m^2",
						   ImGuiSliderFlags_Logarithmic)) {
		XwaRemaster_SetClassicLightIlluminance(illuminance);
	}
	ImGui::SetItemTooltip("For a classic light of brightness 1: the suns and the game's lights.\n"
						  "186 W/m^2 is the sun at the Earth's distance. Now %.0f lux.",
						  (double)(illuminance * AERON_SCENE_LUMENS_PER_WATT));
	float radiance = XwaRemaster_ClassicRadiance();
	if (ImGui::SliderFloat("Classic image radiance", &radiance, 0.0f, 1000.0f, "%.1f W/m^2/sr",
						   ImGuiSliderFlags_Logarithmic)) {
		XwaRemaster_SetClassicRadiance(radiance);
	}
	ImGui::SetItemTooltip("For a white pixel of the classic images: backdrops, lit texels, screens.\n"
						  "59.2 matches a white surface lit by 186 W/m^2. Now %.0f cd/m^2.",
						  (double)(radiance * AERON_SCENE_LUMENS_PER_WATT));
	float intensity_scale = XwaRemasterEnvironment_IntensityScale();
	if (ImGui::SliderFloat("Environment intensity", &intensity_scale, 0.0f,
						   (float)XWA_ENV_INTENSITY_SCALE_MAX, "%.3f", ImGuiSliderFlags_Logarithmic)) {
		XwaRemasterEnvironment_SetIntensityScale(intensity_scale);
	}
	ImGui::SetItemTooltip("Multiplies the environments' skies and the light they give. The\n"
						  "captured lighting is built again.");

	XwaShipPbrTuning tuning;
	XwaRemasterShip_GetPbrTuning(&tuning);
	if (ImGui::ColorEdit3("Ambient fill", tuning.ambient, k_hdr_color)) {
		XwaRemasterShip_SetPbrTuning(&tuning);
	}
	ImGui::SetItemTooltip("Radiance in W/m^2/sr, where no environment gives one.");
	float emissive_scale = AeronScene_EmissiveScale();
	if (ImGui::SliderFloat("Emissive scale", &emissive_scale, 0.25f, 32.0f, "%.2f",
						   ImGuiSliderFlags_Logarithmic)) {
		AeronScene_SetEmissiveScale(emissive_scale);
	}
	ImGui::SetItemTooltip("Scales all emitted light: lit texels, screens, lasers, glows, explosions.");

	if (XwaDebug_ResetButton()) {
		XwaShipPbrTuning defaults;
		XwaRemasterShip_GetPbrTuningDefault(&defaults);
		memcpy(tuning.ambient, defaults.ambient, sizeof tuning.ambient);
		XwaRemasterShip_SetPbrTuning(&tuning);
		XwaRemaster_SetClassicLightIlluminance(XwaRemaster_ClassicLightIlluminanceDefault());
		XwaRemaster_SetClassicRadiance(XwaRemaster_ClassicRadianceDefault());
		XwaRemasterEnvironment_SetIntensityScale(XwaRemasterEnvironment_IntensityScaleDefault());
	}
}

/* ---- Probes ----------------------------------------------------------------- */

static void xwa_window_probes(void*) {
	int probes = XwaRemasterFlight_Probes();
	if (XwaDebug_Checkbox("Probes", &probes)) {
		XwaRemasterFlight_SetProbes(probes);
	}
	ImGui::SetItemTooltip("Off: the models' probes don't light the scene.");
	const XwaFlightProbeSetInfo* sets = nullptr;
	const uint32_t set_count = XwaRemasterFlight_ProbeSets(&sets);
	if (set_count == 0) {
		ImGui::TextDisabled("No probe sets.");
	}
	for (uint32_t i = 0; i < set_count; ++i) {
		ImGui::BulletText("%s, object %u: %u probes, %ux%ux%u grid, %.1f MiB", sets[i].model,
						  (unsigned)sets[i].slot, sets[i].probe_count, sets[i].grid_resolution[0],
						  sets[i].grid_resolution[1], sets[i].grid_resolution[2],
						  (double)sets[i].bytes / (1024.0 * 1024.0));
	}

	int occlusion = XwaRemasterShip_ShipOcclusion();
	if (XwaDebug_Checkbox("Ship occlusion", &occlusion)) {
		XwaRemasterShip_SetShipOcclusion(occlusion);
	}
	ImGui::SetItemTooltip("Ships lose the probe light that they cover: the floor under and around\n"
						  "them, their landing gear, the ceiling's reflections.");
	int reflection = XwaRemasterShip_FloorReflection();
	if (XwaDebug_Checkbox("Floor reflection", &reflection)) {
		XwaRemasterShip_SetFloorReflection(reflection);
	}
	ImGui::SetItemTooltip("When the probes have a reflection plane, the floor reflects the ships on it.");
	ImGui::BeginDisabled(occlusion != 0);
	int all_sides = XwaRemasterShip_KeyShadowAllSides();
	if (XwaDebug_Checkbox("Shadow from all sides", &all_sides)) {
		XwaRemasterShip_SetKeyShadowAllSides(all_sides);
	}
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Without ship occlusion: in the key light's shadow, surfaces lose the\n"
						  "ceiling's probe light whichever way they face. Off: only when they face\n"
						  "the light.");

	ImGui::SeparatorText("Captured lighting");
	XwaRemasterCapturedLightingInfo captured;
	XwaRemasterCapturedLighting_Info(&captured);
	if (!captured.enabled) {
		ImGui::TextDisabled("Off (environment_maps.captured_lighting.enabled).");
		return;
	}
	if (XwaDebug_BeginValues("captured")) {
		XwaDebug_Value("Cubes", "%s, %d px",
					   captured.failed               ? "failed (see the log)"
					   : captured.nothing_to_capture ? "not used: nothing to capture here"
					   : !captured.ready             ? "none"
					   : captured.current            ? "current"
					   : captured.applies            ? "out of date"
													 : "kept, but not used here",
					   captured.size);
		if (captured.ready) {
			XwaDebug_Value("Signature", "%016llx", (unsigned long long)captured.signature);
		}
		XwaDebug_Value("Builds", "%u", captured.build_count);
		XwaDebug_EndValues();
	}
}

/* ---- Point lights ------------------------------------------------------------ */

static void xwa_window_point_lights(void*) {
	XwaFlightPointLightParams p;
	XwaRemasterFlight_GetPointLights(&p);
	bool changed = XwaDebug_Checkbox("Enabled", &p.enabled);
	changed |= ImGui::SliderInt("Cluster depth slices", &p.cluster_depth_slices, 4, 64);
	changed |= XwaDebug_Checkbox("Merge distant spot lights", &p.merge_spot_lights);
	ImGui::SetItemTooltip("Spot lights close together that point the same way, such as a ship's\n"
						  "engines, are shaded as one light far from them.");
	changed |= ImGui::SliderFloat("Match distance", &p.match_distance_m, 1.0f, 200.0f, "%.1f m",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("The game's lights (lasers, explosions, engines) fall off with the square\n"
						  "of the distance, and are as bright as the classic lights at this distance.");
	changed |=
		ImGui::SliderFloat("Range scale", &p.range_scale, 0.25f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("Multiplies the distance where each light ends.");
	changed |= ImGui::SliderFloat("Cockpit explosion scale", &p.cockpit_explosion_light_scale, 0.0f, 1.0f,
								  "%.3f", ImGuiSliderFlags_Logarithmic);
	changed |= ImGui::SliderFloat("Cockpit explosion distance", &p.cockpit_explosion_distance_m, 0.0f, 20.0f,
								  "%.1f m");
	ImGui::SetItemTooltip("In the cockpit, the scale multiplies the light of the explosions this\n"
						  "close to the eye, such as hits on the player's craft. It goes back to 1\n"
						  "at twice the distance.");
	changed |= XwaDebug_Checkbox("Model lights", &p.model_lights);
	ImGui::SetItemTooltip("The lights in the models, in candela, and the headlight.");
	changed |= ImGui::SliderFloat("Model light scale", &p.model_light_scale, 0.1f, 100.0f, "x%.2f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("Multiplies the models' lights, but not the headlight.");
	if (ImGui::TreeNode("Cockpit lights")) {
		for (int slot = 0; slot < XWA_SNAP_COCKPIT_LIGHT_COUNT; slot++) {
			const char* name = XwaCockpitLights_Name(slot);
			changed |= ImGui::SliderFloat(name ? name : "?", &p.cockpit_lights[slot].power_w, 0.0f, 100000.0f,
										  "%.1f W", ImGuiSliderFlags_Logarithmic);
		}
		ImGui::TextDisabled("Used with model lights. A mod's power_w replaces them.");
		ImGui::TreePop();
	}
	if (XwaDebug_ResetButton()) {
		XwaRemasterFlight_GetPointLightsDefault(&p);
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetPointLights(&p);
	}

	XwaFlightPointLightStats stats;
	XwaRemasterFlight_GetPointLightStats(&stats);
	ImGui::Separator();
	if (XwaDebug_BeginValues("point_lights")) {
		XwaDebug_Value("Candidates", "%u: %u valid, %u invalid, %u overflow", stats.generated_count,
					   stats.valid_count, stats.invalid_count, stats.candidate_overflow_count);
		XwaDebug_Value("Game lights", "%u culled, %u dropped", stats.culled_count, stats.dropped_count);
		XwaDebug_Value("Model lights", "%u: %u culled, %u dropped", stats.model_count,
					   stats.model_culled_count, stats.model_dropped_count);
		XwaDebug_Value("Scene", "%u accepted, %u dropped", stats.scene.submitted_light_count,
					   stats.scene.dropped_light_count);
		XwaDebug_Value("Spot lights", "%u merged into %u", stats.scene.grouped_spot_light_count,
					   stats.scene.merged_spot_light_count);
		if (stats.scene.clustered_active) {
			XwaDebug_Value("Clusters", "tile %u, grid %ux%ux%u, %.1f MiB", stats.scene.effective_tile_size,
						   stats.scene.grid_x, stats.scene.grid_y, stats.scene.grid_z,
						   (double)stats.scene.allocated_buffer_bytes / (1024.0 * 1024.0));
		} else {
			XwaDebug_Value("Clusters", "none (no lights)");
		}
		XwaDebug_EndValues();
	}
}

/* ---- Shadows ------------------------------------------------------------------ */

static void xwa_window_shadows(void*) {
	XwaFlightShadowParams p;
	XwaRemasterFlight_GetShadows(&p);
	bool changed = XwaDebug_Checkbox("Enabled", &p.enabled);
	static const char* atlas_names[] = { "1024", "2048", "4096", "8192" };
	int atlas_index = p.atlas_size == 1024 ? 0 : (p.atlas_size == 2048 ? 1 : (p.atlas_size == 8192 ? 3 : 2));
	if (ImGui::Combo("Atlas size", &atlas_index, atlas_names, 4)) {
		p.atlas_size = 1024u << atlas_index;
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
	changed |= XwaDebug_Checkbox("Explicit splits", &p.explicit_splits);
	if (p.explicit_splits) {
		for (int split = 0; split < AERON_SCENE_SHADOW_MAX_CASCADES - 1; split++) {
			char label[24];
			snprintf(label, sizeof label, "Split %d", split + 1);
			const float lower = split == 0 ? 0.001f : p.split_positions[split - 1] + 0.001f;
			const float upper =
				split == AERON_SCENE_SHADOW_MAX_CASCADES - 2 ? 0.999f : p.split_positions[split + 1] - 0.001f;
			ImGui::BeginDisabled(split >= (int)p.cascade_count - 1);
			changed |= ImGui::SliderFloat(label, &p.split_positions[split], lower, upper, "%.3f");
			ImGui::EndDisabled();
		}
	} else {
		changed |= ImGui::SliderFloat("Split lambda", &p.split_lambda, 0.0f, 1.0f, "%.2f");
		ImGui::SetItemTooltip("0 splits the distance evenly, 1 logarithmically.");
	}
	changed |=
		ImGui::SliderFloat("Light angular radius", &p.light_angular_radius_degrees, 0.0f, 5.0f, "%.3f deg");
	ImGui::SetItemTooltip("The larger the light, the faster shadows get softer away from what\n"
						  "casts them.");
	changed |= ImGui::SliderFloat("Largest filter radius", &p.max_filter_radius, p.pcss_min_filter_radius,
								  16.0f, "%.2f texels");
	changed |= ImGui::SliderFloat("Smallest filter radius", &p.pcss_min_filter_radius, 0.5f,
								  p.max_filter_radius, "%.2f texels");
	changed |= ImGui::SliderFloat("Normal bias", &p.normal_bias_texels, 0.0f, 4.0f, "%.2f texels");
	changed |= ImGui::SliderFloat("Depth bias", &p.depth_bias_texels, 0.0f, 4.0f, "%.2f texels");
	changed |= ImGui::SliderFloat("Cascade transition", &p.transition_fraction, 0.0f, 0.5f, "%.2f");
	ImGui::SetItemTooltip("The share of each cascade that blends into the next.");
	changed |= ImGui::SliderFloat("Distance fade", &p.distance_fade_fraction, 0.0f, 0.5f, "%.2f");
	ImGui::SetItemTooltip("The share of the maximum distance over which the shadows fade out.");
	changed |=
		ImGui::SliderFloat("Cockpit tile distance", &p.receiver_local_near_distance_m, 0.0f, 4.0f, "%.2f m");
	ImGui::SetItemTooltip("How far from the camera the cockpit's finer shadow tile reaches. 0 turns\n"
						  "it off.");
	if (XwaDebug_ResetButton()) {
		XwaRemasterFlight_GetShadowsDefault(&p);
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetShadows(&p);
	}

	AeronSceneDirectionalShadowStats stats;
	XwaRemasterFlight_GetShadowStats(&stats);
	ImGui::Separator();
	if (!stats.active) {
		ImGui::TextDisabled("Inactive: off, no flight scene, or no key light.");
		return;
	}
	ImGui::Text("%u cascades in a %u atlas. %u candidates, %u shadow-only dropped.", stats.cascade_count,
				stats.atlas_size, stats.candidate_count, stats.dropped_shadow_only);
	if (ImGui::BeginTable("cascades", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
		ImGui::TableSetupColumn("Cascade");
		ImGui::TableSetupColumn("Range");
		ImGui::TableSetupColumn("Receivers, draws, triangles");
		ImGui::TableSetupColumn("Units per texel");
		ImGui::TableSetupColumn("Density");
		ImGui::TableHeadersRow();
		for (uint32_t c = 0; c < stats.cascade_count; c++) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%u", c);
			ImGui::TableNextColumn();
			ImGui::Text("%.0f to %.0f", stats.split_near[c], stats.split_far[c]);
			ImGui::TableNextColumn();
			ImGui::Text("%u, %u, %u", stats.receiver_count[c], stats.caster_count[c],
						stats.triangle_count[c]);
			ImGui::TableNextColumn();
			ImGui::Text("%.2f x %.2f", stats.world_units_per_texel_x[c], stats.world_units_per_texel_y[c]);
			ImGui::TableNextColumn();
			ImGui::Text("%.2fx", stats.texel_density_gain[c]);
		}
		ImGui::EndTable();
	}
	if (stats.receiver_local_active) {
		ImGui::Text("Cockpit tile %u: %u draws, %u triangles", stats.receiver_local_size,
					stats.receiver_local_caster_count, stats.receiver_local_triangle_count);
	}
}

/* ---- SSAO ---------------------------------------------------------------------- */

/* A view unit is an OPT unit: 1600/65536 m, about 2.4 cm. */
static const float k_metres_per_view_unit = 1600.0f / 65536.0f;

static bool xwa_slider_view_metres(const char* label, float* view_units, float min_m, float max_m) {
	float metres = *view_units * k_metres_per_view_unit;
	if (!ImGui::SliderFloat(label, &metres, min_m, max_m, "%.3f m", ImGuiSliderFlags_Logarithmic)) {
		return false;
	}
	*view_units = metres / k_metres_per_view_unit;
	return true;
}

static void xwa_window_ssao(void*) {
	XwaFlightSsaoParams p;
	XwaRemasterFlight_GetSsao(&p);
	bool changed = false;
	int quality = p.ssao_quality;
	ImGui::RadioButton("Off", &quality, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Low (8 taps)", &quality, 1);
	ImGui::SameLine();
	ImGui::RadioButton("High (16 taps, blurred)", &quality, 2);
	if (quality != p.ssao_quality) {
		p.ssao_quality = quality;
		changed = true;
	}
	int method = p.ssao_method;
	ImGui::RadioButton("GTAO", &method, AERON_SCENE_SSAO_GTAO);
	ImGui::SameLine();
	ImGui::RadioButton("Hemisphere", &method, AERON_SCENE_SSAO_HEMISPHERE);
	if (method != p.ssao_method) {
		p.ssao_method = method;
		changed = true;
	}
	const bool gtao = p.ssao_method == AERON_SCENE_SSAO_GTAO;

	changed |= ImGui::SliderFloat("Intensity", &p.ssao_intensity, 0.0f, 1.0f, "%.2f");
	changed |= xwa_slider_view_metres("Radius", &p.ssao_radius_view, 0.02f, 100.0f);
	changed |= xwa_slider_view_metres("Bias", &p.ssao_bias_view, 0.0f, 6.25f);
	ImGui::SetItemTooltip("Keeps surfaces from shading themselves. Keep it near 1/20 of the radius.");
	ImGui::BeginDisabled(gtao);
	changed |= ImGui::SliderFloat("Power", &p.ssao_power, 0.25f, 4.0f, "%.2f");
	ImGui::EndDisabled();
	ImGui::SetItemTooltip(gtao ? "Not used by GTAO." : "Raises the contrast.");
	changed |= ImGui::SliderFloat("Direct light weight", &p.ssao_direct, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("How much the AO also darkens the sun's direct light. 0 darkens only the\n"
						  "ambient light, which is physically correct.");

	ImGui::SeparatorText("Radius on screen");
	changed |= ImGui::SliderFloat("Smallest", &p.ssao_min_screen_frac, 0.0f, 0.05f, "%.4f");
	ImGui::SetItemTooltip("As a share of half the screen's width. Raise it until the grid on far\n"
						  "hulls disappears. 0 turns it off.");
	changed |= ImGui::SliderFloat("Largest", &p.ssao_max_screen_frac, 0.0f, 0.25f, "%.4f");
	ImGui::SetItemTooltip("Lower it until the separate copies of the shadows on near surfaces\n"
						  "merge. 0 turns it off.");
	changed |= ImGui::SliderFloat("Jitter", &p.ssao_sample_jitter, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("Changes the radius from pixel to pixel, which turns the separate copies\n"
						  "into noise that the blur smooths. Too much adds grain.");

	if (XwaDebug_ResetButton()) {
		XwaRemasterFlight_GetSsaoDefault(&p);
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetSsao(&p);
	}
}

/* ---- Hangar ----------------------------------------------------------------------- */

static void xwa_window_hangar(void*) {
	XwaFlightHangarLightingParams p;
	XwaRemasterFlight_GetHangarLighting(&p);
	bool changed = XwaDebug_Checkbox("Ceiling light", &p.enabled);
	const XwaSnapshot* snap = XwaSnapshot_Current();
	const bool in_hangar = snap && snap->flight_camera_valid && snap->flight_camera.in_hangar;
	ImGui::SameLine();
	ImGui::TextDisabled(in_hangar && p.enabled ? "(in use)" : "(not in use)");

	ImGui::BeginDisabled(!p.enabled);
	changed |= ImGui::SliderFloat3("Direction", p.direction, -1.0f, 1.0f, "%.3f");
	ImGui::SetItemTooltip("From the surface toward the light.");
	changed |= ImGui::SliderFloat("Illuminance", &p.intensity, 0.0f, 2000.0f, "%.2f W/m^2",
								  ImGuiSliderFlags_Logarithmic);
	changed |= ImGui::ColorEdit3("Color", p.color, k_hdr_color);
	ImGui::SeparatorText("Ambient light");
	changed |= ImGui::SliderFloat("From the ceiling", &p.ambient_ceiling, 0.0f, 10.0f, "%.3f");
	changed |= ImGui::SliderFloat("From the walls", &p.ambient_sides, 0.0f, 10.0f, "%.3f");
	changed |= ImGui::SliderFloat("From the floor", &p.ambient_floor, 0.0f, 10.0f, "%.3f");
	ImGui::EndDisabled();

	if (XwaDebug_ResetButton()) {
		XwaRemasterFlight_GetHangarLightingDefault(&p);
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetHangarLighting(&p);
	}
}

/* ---- XWA rows of the Environment window ---------------------------------------------- */

extern "C" void XwaRemasterDebugTools_EnvironmentRows(void* user) {
	(void)user;
	int authored = XwaRemasterEnvironment_AuthoredEnvironments();
	if (XwaDebug_Checkbox("Authored environments", &authored)) {
		XwaRemasterEnvironment_SetAuthoredEnvironments(authored);
	}
	ImGui::SetItemTooltip("Off: every region uses the classic path.");

	XwaEnvironmentSelection selection;
	char mission[128];
	if (XwaRemasterEnvironment_LastSelection(&selection, mission, sizeof mission)) {
		ImGui::Text("Mission: %s, region %u", mission[0] ? mission : "none", (unsigned)selection.region);
		char key[XWA_ENV_MISSION_KEY_MAX];
		if (mission[0] && XwaRemasterEnvironment_NormalizeMissionKey(mission, key, sizeof key)) {
			ImGui::Text("Manifest key: %s", key);
			ImGui::SetItemTooltip("The key of the mission's xwa.missions entry in the manifest.");
			ImGui::SameLine();
			if (ImGui::SmallButton("Copy")) {
				ImGui::SetClipboardText(key);
			}
			const char* prepared = XwaRemasterEnvironment_PreparedMissionKey();
			if (strcmp(prepared, key) != 0) {
				ImGui::Text("Prepared key: %s", prepared[0] ? prepared : "none");
			}
		}
		ImGui::Text("Selection: %s%s%s", XwaRemasterEnvironment_ModeName(selection.mode),
					selection.reference ? ", environment " : "",
					selection.reference ? selection.reference->environment : "");
		if (selection.fallback != XWA_ENV_FALLBACK_NONE) {
			ImGui::Text("Fallback: %s", XwaRemasterEnvironment_FallbackName(selection.fallback));
		}
		const XwaEnvironmentBackdropRule* backdrops = selection.backdrops;
		if (backdrops && backdrops->mode == XWA_ENV_BACKDROPS_KEEP) {
			ImGui::Text("Backdrops kept. Hidden: %s. Shown: %s.",
						backdrops->hide_all     ? "all"
						: backdrops->hide_count ? "a list"
												: "none",
						backdrops->show_all     ? "all"
						: backdrops->show_count ? "a list"
												: "none");
			ImGui::Text("Lights of the hidden backdrops: %s",
						backdrops->remove_hidden_lights ? "removed, except suns" : "kept");
			ImGui::Text("Backdrop lights other than suns: x %.2f", (double)backdrops->light_scale);
		}
	}

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
	ImGui::SetItemTooltip("Saves the region's HD sky for environment_bake --capture. It runs on the\n"
						  "next HD flight frame.");
	if (!available && reason) {
		ImGui::SameLine();
		ImGui::TextDisabled("(%s)", reason);
	}
	const char* status = XwaRemasterEnvCapture_Status();
	if (status[0]) {
		ImGui::TextWrapped("Last capture: %s", status);
	}
}

void XwaDebugLighting_Register(void) {
	XwaDebug_AddWindow(XWA_DEBUG_MENU_LIGHTING, "Calibration", xwa_window_calibration, 440, 0);
	/* Aeron's Environment window, added now so that it is in the menu before
	 * the first flight creates the environment library. */
	AeronEnvironmentLibrary_RegisterDebugTool(nullptr, XwaRemasterDebugTools_EnvironmentRows, nullptr);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_LIGHTING, "Probes", xwa_window_probes, 440, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_LIGHTING, "Point Lights", xwa_window_point_lights, 440, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_LIGHTING, "Shadows", xwa_window_shadows, 480, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_LIGHTING, "SSAO", xwa_window_ssao, 420, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_LIGHTING, "Hangar", xwa_window_hangar, 420, 0);
}
