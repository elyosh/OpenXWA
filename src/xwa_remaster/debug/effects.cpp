/*
 * Effects menu: the hyperspace jump and tunnel, and far laser bolts.
 */

#include "xwa_remaster/debug/debug_internal.h"

#include "xwa_remaster/flight.h"

static const ImGuiColorEditFlags k_hdr_color = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR;

/* ---- Hyperspace ------------------------------------------------------------ */

/* The sliders of a hyperspace camera: hyperspace_camera, or the camera of a
 * look. Returns true when one changed. */
static bool xwa_hyperspace_camera_sliders(XwaFlightHyperspaceCameraParams* c) {
	bool changed = false;
	changed |= ImGui::SliderFloat("Shake", &c->shake_degrees, 0.0f, 10.0f, "%.2f deg");
	ImGui::SetItemTooltip("The camera and the cockpit shake together in the tunnel.");
	changed |= ImGui::SliderFloat("Shake speed", &c->shake_speed, 0.0f, 4.0f, "%.2f");
	changed |= ImGui::SliderFloat("Shake start", &c->shake_start, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("When the shake starts, as a share of the time in the tunnel.");
	changed |= ImGui::SliderFloat("Shake full", &c->shake_full, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("When the shake is at full strength, as a share of the time in the tunnel.");
	changed |= ImGui::SliderFloat("Kicks per second", &c->kick_rate, 0.0f, 8.0f, "%.2f");
	ImGui::SetItemTooltip("Kicks go up to twice the shake.");
	changed |= ImGui::SliderFloat("Exit jolt", &c->jolt_degrees, 0.0f, 20.0f, "%.2f deg");
	ImGui::SetItemTooltip("The jolt when the craft comes out of hyperspace.");
	changed |= ImGui::SliderFloat("Jolt delay", &c->jolt_delay, 0.0f, 0.5f, "%.3f s");
	changed |= ImGui::SliderFloat("Jolt time", &c->jolt_time, 0.05f, 0.8f, "%.3f s");
	if (c->jolt_degrees > 0.0f && c->jolt_delay + 3.0f * c->jolt_time > 1.0f) {
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
						   "The exit lasts 1 s, so the end of the jolt is cut short.");
	}
	return changed;
}

static void xwa_hyperspace_preview(void) {
	static const char* const modes[] = { "Off", "Tunnel", "Jump cycle" };
	int preview = (int)XwaRemasterFlight_GetHyperspacePreview();
	if (ImGui::Combo("Preview", &preview, modes, IM_ARRAYSIZE(modes))) {
		XwaRemasterFlight_SetHyperspacePreview((XwaFlightHyperspacePreview)preview);
	}
	ImGui::SetItemTooltip("Draws hyperspace in place of the flight scene, without changing the\n"
						  "simulation. The jump cycle plays the entry, the tunnel and the exit, then\n"
						  "1 s of normal flight. The map and a real jump take over the preview.");
	float speed = XwaRemasterFlight_GetHyperspacePreviewSpeed();
	if (ImGui::SliderFloat("Preview speed", &speed, 0.05f, 1.0f, "%.2f")) {
		XwaRemasterFlight_SetHyperspacePreviewSpeed(speed);
	}
	float tunnel_time = XwaRemasterFlight_GetHyperspacePreviewLookBlend();
	if (ImGui::SliderFloat("Time in the tunnel", &tunnel_time, 0.0f, 1.0f, "%.2f")) {
		XwaRemasterFlight_SetHyperspacePreviewLookBlend(tunnel_time);
	}
	ImGui::SetItemTooltip("The tunnel preview never ends, so this sets how far through the tunnel it\n"
						  "is: 0 when the tunnel appears, 1 when it ends. It sets the shake and how\n"
						  "far a look's colors have spread.");
}

static void xwa_hyperspace_tunnel_tab(void) {
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
	ImGui::SetItemTooltip("Scales the bright highlights, the cap's light, and their bloom.");
	changed |= ImGui::SliderFloat("Focal length", &p.focal_length, 0.25f, 4.0f, "%.3f");
	ImGui::SetItemTooltip("Longer makes the opening look narrower.");
	changed |= ImGui::SliderFloat("Twist", &p.twist, -2.0f, 2.0f, "%.3f");
	changed |=
		ImGui::SliderFloat("Cap radius", &p.cap_radius, 0.001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("The radius of the cap's full-strength core.");
	changed |=
		ImGui::SliderFloat("Cap falloff", &p.cap_falloff, 0.1f, 64.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("How fast the cap's halo fades.");
	changed |= ImGui::ColorEdit3("Dark color", p.dark_color, k_hdr_color);
	changed |= ImGui::ColorEdit3("Body color", p.body_color, k_hdr_color);
	changed |= ImGui::ColorEdit3("Highlight color", p.highlight_color, k_hdr_color);
	changed |= ImGui::ColorEdit3("Cap color", p.cap_color, k_hdr_color);
	ImGui::SeparatorText("Light on the cockpit");
	changed |= ImGui::SliderFloat("Ambient", &p.mesh_ambient_strength, 0.0f, 4.0f, "%.2f");
	ImGui::SetItemTooltip("The tunnel's diffuse light.");
	changed |= ImGui::SliderFloat("Roughness", &p.mesh_environment_roughness, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("How blurred the tunnel's light is: 0 is sharp, 1 is diffuse.");
	changed |= ImGui::SliderFloat("Key", &p.mesh_key_strength, 0.0f, 4.0f, "%.2f");
	ImGui::SetItemTooltip("The directional light in the cap's color, with its highlights.");
	if (XwaDebug_ResetButton()) {
		XwaRemasterFlight_GetHyperspaceTunnelDefault(&p);
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetHyperspaceTunnel(&p);
	}
}

static void xwa_hyperspace_jump_tab(void) {
	XwaFlightHyperspaceJumpParams j;
	XwaRemasterFlight_GetHyperspaceJump(&j);
	bool changed = false;
	changed |=
		ImGui::SliderInt("Star count", &j.star_count, 1000, 262144, "%d", ImGuiSliderFlags_Logarithmic);
	changed |= ImGui::SliderFloat("Streak brightness", &j.streak_brightness, 0.0f, 16.0f, "%.2f",
								  ImGuiSliderFlags_Logarithmic);
	changed |= ImGui::SliderFloat("White-out brightness", &j.whiteout_brightness, 1.0f, 128.0f, "%.1f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("Multiplies the streaks at the peak of the entry.");
	changed |= ImGui::SliderFloat("Brightness range", &j.brightness_range, 1.0f, 1000.0f, "%.1f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("The brightest star divided by the faintest.");
	changed |= ImGui::SliderFloat("Streak width", &j.streak_width_px, 0.5f, 8.0f, "%.2f px");
	ImGui::SetItemTooltip("In pixels of a 1080-line view.");
	changed |= ImGui::SliderFloat("Width from brightness", &j.width_from_brightness, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("0: every streak has the same width. 1: the width follows the brightness.");
	changed |= ImGui::SliderFloat("Width variation", &j.width_variation, 1.0f, 4.0f, "%.2f");
	ImGui::SetItemTooltip("A random width factor, from 1/x to x.");
	changed |= ImGui::SliderFloat("Head widening", &j.head_widening, 1.0f, 16.0f, "%.2f");
	ImGui::SetItemTooltip("The largest width factor from perspective and radial widening together.");
	changed |= ImGui::SliderFloat("Radial widening", &j.radial_widening, 0.0f, 4.0f, "%.2f");
	ImGui::SetItemTooltip("Widens the streaks away from the vanishing point.");
	changed |= ImGui::SliderFloat("Tail taper", &j.tail_taper, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("The tapers fade the ends of a streak, over shares of its length.");
	changed |= ImGui::SliderFloat("Head taper", &j.head_taper, 0.0f, 1.0f, "%.2f");
	changed |= ImGui::SliderFloat("Taper variation", &j.taper_variation, 0.0f, 1.0f, "%.2f");
	changed |= ImGui::SliderFloat("Glow strength", &j.glow_strength, 0.0f, 1.0f, "%.2f");
	changed |= ImGui::SliderFloat("Glow width", &j.glow_width, 1.0f, 16.0f, "%.2f");
	ImGui::SetItemTooltip("The soft glow around each streak, in streak widths.");
	changed |= ImGui::SliderFloat("Tail brightness", &j.tail_brightness, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("Streaks go from the tail's brightness and color to the head's.");
	changed |= ImGui::ColorEdit3("Tail color", j.tail_color, k_hdr_color);
	changed |= ImGui::ColorEdit3("Streak tint", j.tint, k_hdr_color);
	changed |= ImGui::SliderFloat("Stretch scale", &j.stretch_scale, 0.25f, 4.0f, "%.2f",
								  ImGuiSliderFlags_Logarithmic);
	changed |= ImGui::SliderFloat("White fade", &j.white_fade_seconds, 0.05f, 0.5f, "%.3f s");
	changed |= ImGui::SliderFloat("Tunnel entry brightness", &j.tunnel_entry_brightness, 1.0f, 16.0f, "%.2f");
	changed |= ImGui::SliderFloat("Tunnel settle", &j.tunnel_settle_seconds, 0.05f, 2.0f, "%.2f s");
	changed |= XwaDebug_Checkbox("Forward exit", &j.exit_forward);
	ImGui::SetItemTooltip("On exit, the streaks shrink toward the camera and the scene comes toward\n"
						  "it. Off: the entry plays backward.");

	ImGui::SeparatorText("Light on the cockpit");
	changed |= ImGui::SliderFloat("Strength", &j.mesh_light_strength, 0.0f, 16.0f, "%.2f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("The streaks' light at the white peak, in white classic pixels.");
	changed |= ImGui::SliderFloat("Start", &j.mesh_light_start, 0.0f, 2.0f, "%.2f s");
	changed |=
		ImGui::SliderFloat("Curve", &j.mesh_light_curve, 0.25f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("The exponent of the rise from the start to the peak. 1 is straight,\n"
						  "higher rises later.");

	ImGui::SeparatorText("Scene warp");
	ImGui::TextDisabled("(?)");
	ImGui::SetItemTooltip("From the start to the end, the world moves toward the camera, the sky\n"
						  "zooms in, and black opens from the vanishing point.");
	changed |= ImGui::SliderFloat("Warp start", &j.warp_start, 0.0f, 2.0f, "%.2f s");
	changed |= ImGui::SliderFloat("Warp end", &j.warp_end, 0.2f, 2.5f, "%.2f s");
	changed |=
		ImGui::SliderFloat("Warp curve", &j.warp_curve, 0.25f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	changed |= ImGui::SliderFloat("World distance", &j.warp_distance_m, 0.0f, 200000.0f, "%.0f m",
								  ImGuiSliderFlags_Logarithmic);
	changed |=
		ImGui::SliderFloat("Sky zoom", &j.warp_sky_zoom, 1.0f, 16.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	changed |= ImGui::SliderFloat("Smear", &j.warp_smear, 0.05f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("The length of the trails, as a share of the movement.");
	changed |= ImGui::SliderFloat("Sky highlights", &j.warp_sky_highlights, 1.0f, 3.0f, "%.2f");
	ImGui::SetItemTooltip("Keeps the stars bright in their trails.");
	changed |= ImGui::SliderFloat("Reveal softness", &j.warp_reveal_softness, 0.25f, 2.0f, "%.2f");
	ImGui::SetItemTooltip("The width of the black's soft edge.");
	changed |= ImGui::SliderInt("World taps", &j.warp_taps, 8, 64);
	changed |= ImGui::SliderInt("Sky taps per pass", &j.warp_sky_taps, 4, 16);
	changed |= XwaDebug_Checkbox("Sky at half resolution", &j.warp_sky_half_resolution);

	if (XwaDebug_ResetButton()) {
		XwaRemasterFlight_GetHyperspaceJumpDefault(&j);
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetHyperspaceJump(&j);
	}
}

static void xwa_hyperspace_camera_tab(void) {
	XwaFlightHyperspaceCameraParams camera;
	XwaRemasterFlight_GetHyperspaceCamera(&camera);
	ImGui::TextDisabled("The camera of the normal jump. Each look has its own.");
	ImGui::PushID("hyperspace_camera");
	bool changed = xwa_hyperspace_camera_sliders(&camera);
	if (XwaDebug_ResetButton()) {
		XwaRemasterFlight_GetHyperspaceCameraDefault(&camera);
		changed = true;
	}
	ImGui::PopID();
	if (changed) {
		XwaRemasterFlight_SetHyperspaceCamera(&camera);
	}
}

static void xwa_hyperspace_looks_tab(void) {
	const int look_count = XwaRemasterFlight_HyperspaceLookCount();
	static int edited = 0;
	if (look_count <= 0) {
		ImGui::TextDisabled("remaster/config.yaml has no hyperspace looks.");
		return;
	}
	if (edited >= look_count) {
		edited = 0;
	}
	if (ImGui::BeginCombo("Look", XwaRemasterFlight_HyperspaceLookName(edited))) {
		for (int i = 0; i < look_count; i++) {
			if (ImGui::Selectable(XwaRemasterFlight_HyperspaceLookName(i), i == edited)) {
				edited = i;
			}
		}
		ImGui::EndCombo();
	}
	bool preview_look = XwaRemasterFlight_GetHyperspacePreviewLook() == edited;
	if (ImGui::Checkbox("Preview this look", &preview_look)) {
		XwaRemasterFlight_SetHyperspacePreviewLook(preview_look ? edited : -1);
	}

	XwaFlightHyperspaceLookParams look;
	XwaRemasterFlight_GetHyperspaceLook(edited, &look);
	bool changed = false;
	ImGui::SeparatorText("Tunnel");
	ImGui::TextDisabled("(?)");
	ImGui::SetItemTooltip("The tunnel's colors where the look has spread.");
	changed |= ImGui::ColorEdit3("Dark color", look.dark_color, k_hdr_color);
	changed |= ImGui::ColorEdit3("Body color", look.body_color, k_hdr_color);
	changed |= ImGui::ColorEdit3("Highlight color", look.highlight_color, k_hdr_color);
	changed |= ImGui::ColorEdit3("Cap color", look.cap_color, k_hdr_color);
	changed |= ImGui::SliderFloat("Spread start", &look.blend_start, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("When the look starts to spread, as a share of the time in the tunnel.");
	changed |= ImGui::SliderFloat("Spread end", &look.blend_end, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("When the look fills the tunnel, as a share of the time in the tunnel.");

	ImGui::SeparatorText("Edge");
	ImGui::TextDisabled("(?)");
	ImGui::SetItemTooltip("The thin glowing line at the edge of the area where the look has spread.");
	changed |= ImGui::ColorEdit3("Edge color", look.edge_color, k_hdr_color);
	changed |=
		ImGui::SliderFloat("Thickness", &look.edge_width, 0.01f, 4.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
	changed |= ImGui::SliderFloat("Unevenness", &look.front_noise, 0.0f, 8.0f, "%.2f");
	ImGui::SetItemTooltip("How far the edge's bumps reach ahead.");
	changed |= ImGui::SliderFloat("Bump count", &look.front_noise_scale, 0.05f, 16.0f, "%.2f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("Higher gives more, smaller bumps.");
	changed |= ImGui::SliderFloat("Curl", &look.front_warp, 0.0f, 4.0f, "%.2f");
	ImGui::SetItemTooltip("How much the bumps twist.");

	ImGui::SeparatorText("Exit streaks");
	changed |= ImGui::ColorEdit3("Tint", look.tint, k_hdr_color);
	changed |= ImGui::ColorEdit3("Tail color", look.tail_color, k_hdr_color);

	ImGui::SeparatorText("Camera");
	ImGui::TextDisabled("(?)");
	ImGui::SetItemTooltip("The look's shake replaces the normal one between its start and full, and\n"
						  "its jolt replaces the normal one.");
	ImGui::PushID("look_camera");
	changed |= xwa_hyperspace_camera_sliders(&look.camera);
	ImGui::PopID();

	if (XwaDebug_ResetButton()) {
		XwaRemasterFlight_GetHyperspaceLookDefault(edited, &look);
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetHyperspaceLook(edited, &look);
	}
}

static void xwa_window_hyperspace(void*) {
	xwa_hyperspace_preview();
	if (ImGui::BeginTabBar("hyperspace")) {
		if (ImGui::BeginTabItem("Tunnel")) {
			xwa_hyperspace_tunnel_tab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Jump")) {
			xwa_hyperspace_jump_tab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Camera")) {
			xwa_hyperspace_camera_tab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Looks")) {
			xwa_hyperspace_looks_tab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

static void xwa_hyperspace_closed(void*) {
	XwaRemasterFlight_SetHyperspacePreview(XWA_FLIGHT_HYPERSPACE_PREVIEW_OFF);
}

/* ---- Laser bolts -------------------------------------------------------------- */

static void xwa_window_laser_bolts(void*) {
	XwaFlightProjectileParams p;
	XwaRemasterFlight_GetProjectiles(&p);
	bool changed = false;
	changed |= ImGui::SliderFloat("Minimum width", &p.min_width_px, 0.0f, 4.0f, "%.2f px");
	ImGui::SetItemTooltip("Thinner bolts are drawn this wide, as a steady line. 0 turns it off.");
	changed |= ImGui::SliderFloat("Fade below width", &p.fade_width_px, 0.0f, 16.0f, "%.1f px");
	ImGui::SetItemTooltip("Thinner bolts get dimmer. In pixels of a 1080-line view. 0 turns it off.");
	changed |= ImGui::SliderFloat("Fade power", &p.fade_power, 0.0f, 3.0f, "%.2f");
	changed |= ImGui::SliderFloat("Emission scale", &p.emission_scale, 0.0f, 1.0f, "%.4f",
								  ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("How bright the bolts are drawn.");
	changed |=
		ImGui::SliderFloat("Bloom scale", &p.bloom_scale, 0.0f, 4.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
	ImGui::SetItemTooltip("How much light the bloom gets from the bolts.");
	changed |= XwaDebug_Checkbox("Bloom from the scene", &p.bloom_from_scene);
	ImGui::SetItemTooltip("Draws the bolts at the bloom scale, so their bloom comes from the scene\n"
						  "like any other light's, with no bloom draw of their own.");
	if (XwaDebug_ResetButton()) {
		XwaRemasterFlight_GetProjectilesDefault(&p);
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetProjectiles(&p);
	}
}

void XwaDebugEffects_Register(void) {
	XwaDebug_AddWindow(XWA_DEBUG_MENU_EFFECTS, "Hyperspace", xwa_window_hyperspace, 460, 640,
					   xwa_hyperspace_closed);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_EFFECTS, "Laser Bolts", xwa_window_laser_bolts, 400, 0);
}
