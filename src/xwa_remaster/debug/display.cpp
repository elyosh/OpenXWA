/*
 * Display menu: HDR output, frame pacing, upscaling, and the stereo view.
 */

#include "xwa_remaster/debug/debug_internal.h"

#include "xwa_remaster/flight.h"
#include "xwa_remaster/xwa_remaster.h"

#include "aeron/render.h"
#include "aeron/time.h"

static const char* xwa_format_name(AeronTextureFormat format) {
	switch (format) {
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
			return "other";
	}
}

/* ---- Output -------------------------------------------------------------- */

static void xwa_window_output(void*) {
	const AeronHdrOutputStatus status = Aeron_OutputHdrStatus();
	const bool hdr_active = status == AERON_HDR_OUTPUT_ACTIVE;

	bool want_hdr = XwaRemaster_GetHdrDesired() != 0;
	if (ImGui::Checkbox("HDR output", &want_hdr)) {
		/* Applied at the next frame: the swapchain change rebuilds the
		 * pipelines, which can't happen while the overlay is recorded. */
		XwaRemaster_SetHdrDesired(want_hdr ? 1 : 0);
	}
	if (status == AERON_HDR_OUTPUT_DISPLAY_SDR) {
		ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "The display isn't in HDR mode, so the image is SDR.");
		ImGui::SetItemTooltip("Turning HDR on in the system settings switches over without a restart.");
	} else if (status == AERON_HDR_OUTPUT_UNSUPPORTED) {
		ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "The %s backend refused HDR, so the image is SDR.",
						   Aeron_RenderDriverName());
	}

	float paper_white = Aeron_OutputPaperWhiteNits();
	bool auto_white = paper_white <= 0.0f;
	if (ImGui::Checkbox("Paper white from the system", &auto_white)) {
		Aeron_SetOutputPaperWhiteNits(auto_white ? 0.0f : 200.0f);
		paper_white = Aeron_OutputPaperWhiteNits();
	}
	if (!auto_white) {
		if (ImGui::SliderFloat("Paper white", &paper_white, 80.0f, 400.0f, "%.0f nits")) {
			Aeron_SetOutputPaperWhiteNits(paper_white);
		}
	}

	float content_gamma = Aeron_OutputSdrContentGamma();
	bool power_decode = content_gamma > 0.0f;
	ImGui::BeginDisabled(!hdr_active);
	ImGui::TextUnformatted("SDR images in HDR:");
	ImGui::SameLine();
	if (ImGui::RadioButton("sRGB curve", !power_decode) && power_decode) {
		Aeron_SetOutputSdrContentGamma(0.0f);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Power gamma", power_decode) && !power_decode) {
		Aeron_SetOutputSdrContentGamma(2.2f);
	}
	if (power_decode) {
		if (ImGui::SliderFloat("Gamma", &content_gamma, 1.8f, 2.6f, "%.2f")) {
			Aeron_SetOutputSdrContentGamma(content_gamma);
		}
	}
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("How the menus and the classic images are decoded. SDR output always\n"
						  "uses the sRGB curve.");

	ImGui::Separator();
	if (XwaDebug_BeginValues("output")) {
		XwaDebug_Value("HDR available", "%s", Aeron_OutputSupportsHdr() ? "yes" : "no");
		XwaDebug_Value("HDR output", "%s", Aeron_OutputHdrStatusName(status));
		XwaDebug_Value("SDR white", "%.2fx", (double)Aeron_OutputSdrWhiteLevel());
		XwaDebug_Value("Headroom", "%.2fx", (double)Aeron_OutputHdrHeadroom());
		XwaDebug_Value("Swapchain", "%s", xwa_format_name(Aeron_SwapchainFormat()));
		XwaDebug_EndValues();
	}
}

/* ---- Frame pacing -------------------------------------------------------- */

static void xwa_window_frame_pacing(void*) {
	static const char* vsync_names[] = { "Full refresh", "Half refresh" };
	int vsync_rate = Aeron_PresentationVsyncDivisor() - 1;
	if (ImGui::Combo("VSync rate", &vsync_rate, vsync_names, 2)) {
		Aeron_SetPresentationVsyncDivisor(vsync_rate + 1);
	}
	static const char* flight_names[] = { "1", "2" };
	int frames_in_flight = Aeron_FramesInFlight() - 1;
	if (ImGui::Combo("Frames in flight", &frames_in_flight, flight_names, 2)) {
		Aeron_SetFramesInFlight(frames_in_flight + 1);
	}
	ImGui::SetItemTooltip("With 2, the next frame starts while the GPU still draws the last one.");
	bool latency_pacing = Aeron_LatencyPacing() != 0;
	if (ImGui::Checkbox("Latency pacing", &latency_pacing)) {
		Aeron_SetLatencyPacing(latency_pacing);
	}
	ImGui::SetItemTooltip("Starts each frame later, so the input is read closer to when the frame\n"
						  "is shown. Only at full refresh.");

	ImGui::Separator();
	AeronPacingStats pacing;
	Aeron_GetPacingStats(&pacing);
	if (XwaDebug_BeginValues("pacing")) {
		XwaDebug_Value("Display refresh", "%.2f Hz", Aeron_DisplayRefreshRate());
		XwaDebug_Value("Presentation", "%.2f fps", Aeron_PresentationRate());
		XwaDebug_Value("Pacing delay", "%.2f ms%s", pacing.delay_us / 1000.0, pacing.active ? "" : " (off)");
		XwaDebug_Value("Image wait", "%.2f ms (median)", pacing.acquire_wait_us / 1000.0);
		XwaDebug_Value("Late frames", "%u per second", pacing.late_frames);
		XwaDebug_Value("GPU busy at vsync", "%u per second", pacing.busy_frames);
		XwaDebug_EndValues();
	}
}

/* ---- Upscaling ------------------------------------------------------------ */

static void xwa_window_upscaling(void*) {
	XwaFlightTemporalParams params;
	XwaRemasterFlight_GetTemporal(&params);
	bool changed = false;
	static const char* mode_names[] = { "Off", "Native AA", "Quality", "Balanced", "Performance" };
	int mode = (int)params.mode;
	if (ImGui::Combo("FSR 3.1 mode", &mode, mode_names, IM_ARRAYSIZE(mode_names))) {
		params.mode = (AeronTemporalMode)mode;
		changed = true;
	}
	changed |= ImGui::SliderFloat("Sharpness", &params.sharpness, 0.0f, 1.0f, "%.2f");
	if (changed) {
		XwaRemasterFlight_SetTemporal(&params);
	}

	XwaFlightTemporalStats stats;
	XwaRemasterFlight_GetTemporalStats(&stats);
	ImGui::Separator();
	if (stats.output_width <= 0 || stats.output_height <= 0) {
		ImGui::TextDisabled("No flight scene.");
		return;
	}
	if (XwaDebug_BeginValues("fsr")) {
		XwaDebug_Value("Render", "%d x %d", stats.render_width, stats.render_height);
		XwaDebug_Value("Output", "%d x %d", stats.output_width, stats.output_height);
		if (stats.history_reset_active) {
			XwaDebug_Value("History reset", "for %u frames: %s", stats.history_reset_consecutive_frames,
						   stats.history_reset_reasons[0] ? stats.history_reset_reasons : "unknown reason");
		} else {
			XwaDebug_Value("History reset", "no");
		}
		if (stats.profile_available) {
			const bool direct_output =
				stats.profile.direct_history_output && params.sharpness <= 0.0f && !params.debug_view;
			XwaDebug_Value("Shader profile", "%s", stats.profile.profile_name);
			XwaDebug_Value("Backend", "%s", stats.profile.backend_driver);
			XwaDebug_Value("Precision", "%s", stats.profile.fp16 ? "FP16" : "FP32");
			XwaDebug_Value("SPD", "%s", stats.profile.wave_spd ? "native wave" : "scalar");
			XwaDebug_Value("Writes to", "%s", direct_output ? "its history" : "a separate target");
		}
		XwaDebug_EndValues();
	}
}

/* ---- Stereo view ---------------------------------------------------------- */

static void xwa_window_stereo_view(void*) {
	int count = 1, width = 0, height = 0;
	XwaRemasterFlight_GetViewLayout(&count, &width, &height);
	ImGui::Text("%d %s of %d x %d", count, count == 1 ? "view" : "views", width, height);
	ImGui::Separator();

	XwaRemasterDebugStereo stereo;
	XwaRemaster_GetDebugStereo(&stereo);
	bool changed = XwaDebug_Checkbox("Stereo view", &stereo.enabled);
	ImGui::SetItemTooltip("Draws a headset's two eyes side by side, to test the renderer without a\n"
						  "headset. The game camera is the seat, and the head moves from it.");
	changed |= XwaDebug_Checkbox("The head moves the flight view", &stereo.flat_head);
	ImGui::SetItemTooltip("With the stereo view off, the head below moves the flight view, like a\n"
						  "head tracker.");
	changed |= ImGui::SliderFloat("Eye distance", &stereo.ipd_m, 0.0f, 0.2f, "%.3f m");
	changed |= ImGui::SliderFloat("Field of view", &stereo.fov_deg, 30.0f, 150.0f, "%.0f deg");
	ImGui::SetItemTooltip("Horizontal, for one eye.");
	changed |= XwaDebug_Checkbox("The mouse turns the head", &stereo.mouse_head);
	if (stereo.mouse_head) {
		changed |=
			ImGui::SliderFloat("Yaw at the window's edge", &stereo.mouse_yaw_deg, 0.0f, 180.0f, "%.0f deg");
		changed |= ImGui::SliderFloat("Pitch at the window's edge", &stereo.mouse_pitch_deg, 0.0f, 90.0f,
									  "%.0f deg");
	} else {
		changed |= ImGui::SliderFloat("Yaw (right)", &stereo.yaw_deg, -180.0f, 180.0f, "%.1f deg");
		changed |= ImGui::SliderFloat("Pitch (up)", &stereo.pitch_deg, -90.0f, 90.0f, "%.1f deg");
	}
	changed |= ImGui::SliderFloat("Roll (clockwise)", &stereo.roll_deg, -90.0f, 90.0f, "%.1f deg");
	changed |= ImGui::SliderFloat3("Position (right, up, forward)", stereo.position_m, -1.0f, 1.0f, "%.3f m");
	if (ImGui::Button("Center the head")) {
		stereo.yaw_deg = stereo.pitch_deg = stereo.roll_deg = 0.0f;
		stereo.position_m[0] = stereo.position_m[1] = stereo.position_m[2] = 0.0f;
		changed = true;
	}
	if (changed) {
		XwaRemaster_SetDebugStereo(&stereo);
	}
}

void XwaDebugDisplay_Register(void) {
	XwaDebug_AddWindow(XWA_DEBUG_MENU_DISPLAY, "Output", xwa_window_output, 400, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_DISPLAY, "Frame Pacing", xwa_window_frame_pacing, 360, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_DISPLAY, "Upscaling", xwa_window_upscaling, 380, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_DISPLAY, "Stereo View", xwa_window_stereo_view, 440, 0);
}
