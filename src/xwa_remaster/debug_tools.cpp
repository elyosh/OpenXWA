/*
 * XWA debug tools — ImGui tool windows for the Aeron debug overlay
 * (backquote toggles; see xwa_remaster.c). Mirrors the TIE inspector
 * set for the shared machinery:
 *
 *   PBR — Global : XwaShipPbrTuning + mesh texture filtering
 *   SSAO         : XwaFlightSsaoParams (live; render.yaml persists)
 *   HDR & Display: output mode + AeronScenePresent_* tonemap knobs
 *
 * All state mutations go through the public accessors — no renderer
 * internals are reached into. Compiled only when AERON_DEBUG_UI.
 */

#include "xwa_remaster/debug_tools.h"

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
#include "xwa_runtime/snapshot/snapshot.h"

#include <cstdio>
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

	changed |= ImGui::SliderFloat("Sun intensity", &t.light_intensity, 0.0f, 4.0f, "%.2f");
	changed |= ImGui::SliderFloat("Global specular", &t.global_spec_mul, 0.0f, 4.0f, "%.2f");
	ImGui::TextDisabled("Classic craft shading is pure diffuse (0). The period Gouraud\n"
						"OPT normals break a modern spec lobe into per-face patches.");
	changed |= ImGui::SliderFloat("Light wrap", &t.light_wrap, 0.0f, 1.0f, "%.2f");
	ImGui::TextDisabled("0 = Lambert (classic hard terminator), 1 = Half-Lambert.");
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
						"Edits are not persisted; copy them into render.yaml.");
	if (ImGui::Button("Reset texture filtering")) {
		XwaRemasterFlight_GetTextureFilteringDefault(&filtering);
		filtering_changed = true;
	}
	if (filtering_changed) {
		XwaRemasterFlight_SetTextureFiltering(&filtering);
	}

	ImGui::Separator();
	static const char* isolate_names[] = {
		"None", "Base diffuse", "Specular",    "Geometry term (G)", "N.V",
		"N.L",  "Normal",       "View vector", "N.V signed",        "Local lights",
	};
	int iso = (int)t.debug_isolate_term;
	if (ImGui::Combo("Isolate term", &iso, isolate_names,
					 (int)(sizeof isolate_names / sizeof isolate_names[0]))) {
		t.debug_isolate_term = (float)iso;
		changed = true;
	}

	ImGui::Separator();
	if (ImGui::Button("Reset to defaults")) {
		XwaRemasterShip_GetPbrTuningDefault(&t);
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

	int quality = p.quality;
	ImGui::RadioButton("Off", &quality, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Low (8 taps)", &quality, 1);
	ImGui::SameLine();
	ImGui::RadioButton("High (16 taps + blur)", &quality, 2);
	if (quality != p.quality) {
		p.quality = quality;
		changed = true;
	}

	changed |= ImGui::SliderFloat("Intensity", &p.intensity, 0.0f, 1.0f, "%.2f");
	changed |= ImGui::SliderFloat("Radius (view units)", &p.radius_view, 1.0f, 4096.0f, "%.1f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("XWA view unit ~= 2.4 cm. TIE's shipped calibration is 655\n"
						"(~16 m, craft-scale cavity shading; TIE shows it as 0.01 in\n"
						"its own km-scale view units). 16 ~= 0.4 m contact AO.");
	changed |= ImGui::SliderFloat("Bias (view units)", &p.bias_view, 0.0f, 256.0f, "%.2f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("Self-occlusion rejection; keep ~ radius / 20 (TIE: 32.8).");
	changed |= ImGui::SliderFloat("Power (contrast)", &p.power, 0.25f, 4.0f, "%.2f");
	changed |= ImGui::SliderFloat("Direct-diffuse weight", &p.direct, 0.0f, 1.0f, "%.2f");
	ImGui::TextDisabled("How much AO also occludes the direct sun diffuse\n"
						"(0 = ambient-only, physically strict).");
	bool viz = p.debug_viz != 0;
	if (ImGui::Checkbox("Debug viz (raw AO as grayscale)", &viz)) {
		p.debug_viz = viz ? 1 : 0;
		changed = true;
	}

	ImGui::Separator();
	ImGui::TextDisabled("Screen-space radius clamp. A world-space radius shrinks to\n"
						"sub-texel on far hulls (kernel aliases into a grid) and over-\n"
						"spreads on near ones. Fractions of NDC half-width; 0 = off.");
	changed |= ImGui::SliderFloat("Min screen radius", &p.min_screen_frac, 0.0f, 0.05f, "%.4f");
	ImGui::TextDisabled("Footprint floor on distant surfaces (~ frac * ao_w texels).\n"
						"Raise until the far grid disappears.");
	changed |= ImGui::SliderFloat("Max screen radius", &p.max_screen_frac, 0.0f, 0.25f, "%.4f");
	ImGui::TextDisabled("Footprint ceiling on near surfaces; raise the base radius,\n"
						"then lower this until the separated tap copies merge. 0 = off.");
	changed |= ImGui::SliderFloat("Sample jitter", &p.sample_jitter, 0.0f, 1.0f, "%.2f");
	ImGui::TextDisabled("Per-pixel radius jitter. Raise to dissolve the 'separate\n"
						"shadows' from discrete taps into blur-smoothed noise; too\n"
						"high adds grain. Free (one ALU hash). 0 = off.");

	ImGui::Separator();
	if (ImGui::Button("Reset to defaults")) {
		XwaRemasterFlight_GetSsaoDefault(&p);
		changed = true;
	}
	ImGui::TextDisabled("Edits apply on the next frame and are NOT persisted —\n"
						"copy values into resources/remaster/flight/render.yaml.");
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
	bool enabled = p.mode == XWA_FLIGHT_SHADOWS_PCF;
	if (ImGui::Checkbox("Enabled", &enabled)) {
		p.mode = enabled ? XWA_FLIGHT_SHADOWS_PCF : XWA_FLIGHT_SHADOWS_OFF;
		changed = true;
	}
	static const char* atlas_names[] = { "1024", "2048", "4096", "8192" };
	int atlas_index = p.atlas_size == 1024 ? 0 : (p.atlas_size == 2048 ? 1 : (p.atlas_size == 8192 ? 3 : 2));
	if (ImGui::Combo("Atlas size", &atlas_index, atlas_names, 4)) {
		p.atlas_size = 1024 << atlas_index;
		changed = true;
	}
	changed |= ImGui::SliderInt("Cascades", &p.cascade_count, 1, AERON_SCENE_SHADOW_MAX_CASCADES);
	static const char* fit_names[] = { "Stable sphere", "Frustum rectangle", "Scene-dependent PSSM" };
	changed |= ImGui::Combo("Cascade fit", &p.fit_mode, fit_names, 3);
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
	static const char* filter_names[] = { "Hard", "Low (3x3 PCF / 8-tap PCSS)",
										  "Medium (5x5 PCF / 16-tap PCSS)", "High (7x7 PCF / 24-tap PCSS)" };
	changed |= ImGui::Combo("Filter", &p.filter_quality, filter_names, 4);
	changed |= ImGui::SliderFloat("Filter radius", &p.filter_radius, 0.5f, 3.0f, "%.2f texels");
	bool contact_hardening = p.contact_hardening != 0;
	if (ImGui::Checkbox("Contact-hardening PCSS", &contact_hardening)) {
		p.contact_hardening = contact_hardening ? 1 : 0;
		changed = true;
	}
	if (!contact_hardening) {
		ImGui::BeginDisabled();
	}
	changed |=
		ImGui::SliderFloat("Light angular radius", &p.light_angular_radius_degrees, 0.0f, 5.0f, "%.3f deg");
	changed |= ImGui::SliderFloat("Maximum filter radius", &p.max_filter_radius, p.filter_radius, 16.0f,
								  "%.2f texels");
	changed |= ImGui::SliderFloat("Minimum PCSS radius", &p.pcss_min_filter_radius, 0.5f, p.filter_radius,
								  "%.2f texels");
	if (!contact_hardening) {
		ImGui::EndDisabled();
	}
	changed |= ImGui::SliderFloat("Normal bias", &p.normal_bias, 0.0f, 32.0f, "%.2f");
	bool face_normal_bias = p.normal_bias_face_normal != 0;
	if (ImGui::Checkbox("Bias along face normal", &face_normal_bias)) {
		p.normal_bias_face_normal = face_normal_bias ? 1 : 0;
		changed = true;
	}
	changed |= ImGui::SliderFloat("Depth bias", &p.depth_bias_texels, 0.0f, 8.0f, "%.2f texels");
	changed |= ImGui::SliderFloat("Slope bias", &p.slope_bias, 0.0f, 8.0f, "%.2f");
	changed |= ImGui::SliderFloat("Receiver-plane bias", &p.receiver_plane_bias, 0.0f, 2.0f, "%.2f");
	changed |= ImGui::SliderFloat("Caster constant bias", &p.caster_constant_bias, 0.0f, 8.0f, "%.2f");
	changed |= ImGui::SliderFloat("Caster slope bias", &p.caster_slope_bias, 0.0f, 8.0f, "%.2f");
	changed |= ImGui::SliderFloat("Cascade transition", &p.transition_fraction, 0.0f, 0.5f, "%.2f");
	changed |= ImGui::SliderFloat("Distance fade", &p.distance_fade_fraction, 0.0f, 0.5f, "%.2f");
	bool debug_cascades = p.debug_cascades != 0;
	if (ImGui::Checkbox("Visualize cascades", &debug_cascades)) {
		p.debug_cascades = debug_cascades ? 1 : 0;
		changed = true;
	}
	bool debug_atlas = p.debug_atlas != 0;
	if (ImGui::Checkbox("Visualize shadow atlas", &debug_atlas)) {
		p.debug_atlas = debug_atlas ? 1 : 0;
		changed = true;
	}
	if (!debug_atlas) {
		ImGui::BeginDisabled();
	}
	static const char* atlas_view_names[] = { "Whole atlas", "Cascade 0", "Cascade 1", "Cascade 2",
											  "Cascade 3" };
	int atlas_view = p.debug_atlas_cascade + 1;
	atlas_view = atlas_view < 0 ? 0 : (atlas_view > p.cascade_count ? p.cascade_count : atlas_view);
	if (ImGui::Combo("Atlas view", &atlas_view, atlas_view_names, p.cascade_count + 1)) {
		p.debug_atlas_cascade = atlas_view - 1;
		changed = true;
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
						"copy values into resources/remaster/flight/render.yaml.");
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
	const bool active = snap && snap->flight_camera_valid && snap->flight_camera.in_hangar && enabled;
	ImGui::TextDisabled("Scene override: %s", active ? "active" : "inactive");

	if (!enabled) {
		ImGui::BeginDisabled();
	}
	changed |= ImGui::SliderFloat3("Direction (surface to light)", p.direction, -1.0f, 1.0f, "%.3f");
	changed |= ImGui::SliderFloat("Intensity", &p.intensity, 0.0f, 4.0f, "%.2f");
	changed |= ImGui::ColorEdit3("Light color", p.color, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	ImGui::Separator();
	ImGui::TextUnformatted("Ambient cube (linear)");
	changed |= ImGui::SliderFloat("Ceiling fill (+Z)", &p.ambient_ceiling, 0.0f, 1.0f, "%.3f");
	changed |= ImGui::SliderFloat("Wall bounce", &p.ambient_sides, 0.0f, 1.0f, "%.3f");
	changed |= ImGui::SliderFloat("Floor bounce (-Z)", &p.ambient_floor, 0.0f, 1.0f, "%.3f");
	ImGui::Separator();
	changed |= ImGui::SliderFloat("Shadow filter radius", &p.shadow_filter_radius, 0.5f, 3.0f, "%.2f texels");
	if (!enabled) {
		ImGui::EndDisabled();
	}

	if (ImGui::Button("Reset to defaults")) {
		XwaRemasterFlight_GetHangarLightingDefault(&p);
		changed = true;
	}
	ImGui::TextDisabled("The hangar backdrop receives shadows but does not cast them.\n"
						"Edits are not persisted; copy them into render.yaml.");
	if (changed) {
		XwaRemasterFlight_SetHangarLighting(&p);
	}

	ImGui::End();
	*open = b ? 1 : 0;
}

/* ---- Point lights ---------------------------------------------------- */

static void xwa_tool_point_lights(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(440, 280), ImGuiCond_FirstUseEver);
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
	changed |=
		ImGui::SliderFloat("Intensity scale", &p.scale, 0.05f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("Multiplier on the classic intensities. The shader runs the\n"
						"classic curve (intensity*0.5/d), so 1.0 = classic magnitudes.");
	changed |=
		ImGui::SliderFloat("Range scale", &p.range_scale, 0.25f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("Multiplies the derived visibility window (~the classic 1%%\n"
						"contribution floor; the classic curve has no hard cutoff).");
	changed |= ImGui::SliderFloat("Min distance", &p.min_distance, 4.0f, 1024.0f, "%.0f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::TextDisabled("Near clamp on 0.5/d in view units (~2.4 cm each) — stops the\n"
						"blowup when a bolt skims a hull.");
	changed |= ImGui::SliderFloat("Specular weight", &p.spec_weight, 0.0f, 2.0f, "%.2f");
	changed |= ImGui::SliderFloat("Diffuse wrap", &p.diffuse_wrap, 0.0f, 1.0f, "%.2f");
	ImGui::TextDisabled("0 = Lambert (hard shaping), 1 = half-Lambert — grazing hulls\n"
						"under a passing bolt stay lit like the classic (no N.L there).");
	changed |= ImGui::SliderFloat("Contribution cap", &p.contrib_cap, 0.0f, 4.0f, "%.2f");
	ImGui::TextDisabled("Per-light lit-color cap, hue-preserving; the classic saturates\n"
						"at 1.0 (engine-glow sources exceed it by design). 0 = uncapped.");

	ImGui::Separator();
	if (ImGui::Button("Reset to defaults")) {
		XwaRemasterFlight_GetPointLightsDefault(&p);
		changed = true;
	}
	ImGui::TextDisabled("Edits apply on the next frame and are NOT persisted —\n"
						"copy values into resources/remaster/flight/render.yaml\n"
						"under point_lights: {scale, range_scale, min_distance,\n"
						"spec_weight, enabled}.");
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

	ImGui::TextDisabled("Runtime edits are not persisted to render.yaml.");
	ImGui::End();
	*open = b ? 1 : 0;
}

static void xwa_tool_hdr(int* open, void* user) {
	(void)user;
	ImGui::SetNextWindowSize(ImVec2(420, 360), ImGuiCond_FirstUseEver);
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
	static const char* vsync_names[] = { "Full refresh", "Half refresh" };
	int vsync_rate = Aeron_PresentationVsyncDivisor() - 1;
	if (ImGui::Combo("VSync rate", &vsync_rate, vsync_names, 2)) {
		Aeron_SetPresentationVsyncDivisor(vsync_rate + 1);
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
	if (ImGui::SliderFloat("Bloom intensity", &bloom_i, 0.0f, 2.0f, "%.2f")) {
		AeronSceneBloom_SetIntensity(bloom_i);
	}
	ImGui::TextDisabled("0 disables bloom (chain is skipped entirely).");

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
	Aeron_DebugRegisterTool("FSR 3.1.4", xwa_tool_fsr, nullptr);
	Aeron_DebugRegisterTool("HDR & Display", xwa_tool_hdr, nullptr);
	Aeron_DebugRegisterTool("HUD Snapshot", xwa_tool_hud_snapshot, nullptr);
	Aeron_DebugRegisterTool("Flight Input", xwa_tool_flight_input, nullptr);
}
