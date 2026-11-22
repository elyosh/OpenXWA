/*
 * Image menu: exposure, tonemap and bloom, and motion blur.
 */

#include "xwa_remaster/debug/debug_internal.h"

#include "xwa_remaster/flight.h"

#include "aeron/scene/present.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>

/* ---- Exposure ------------------------------------------------------------ */

static void xwa_exposure_histogram(const AeronSceneExposureReadout& readout) {
	const float first = AERON_SCENE_EXPOSURE_FIRST_EV100;
	const float span = AERON_SCENE_EXPOSURE_BINS * AERON_SCENE_EXPOSURE_BIN_EV;
	char label[48];
	snprintf(label, sizeof label, "EV100 %.0f to %.0f", (double)first, (double)(first + span));
	ImGui::PlotHistogram("##exposure_histogram", readout.histogram, AERON_SCENE_EXPOSURE_BINS, 0, label, 0.0f,
						 FLT_MAX, ImVec2(-FLT_MIN, 80.0f));
	ImGui::SetItemTooltip("White lines: the range the meter averages. Yellow: the current EV100.");
	const ImVec2 lo = ImGui::GetItemRectMin();
	const ImVec2 hi = ImGui::GetItemRectMax();
	ImDrawList* draw = ImGui::GetWindowDrawList();
	auto mark = [&](float ev, ImU32 color) {
		const float t = std::clamp((ev - first) / span, 0.0f, 1.0f);
		const float x = lo.x + t * (hi.x - lo.x);
		draw->AddLine(ImVec2(x, lo.y), ImVec2(x, hi.y), color);
	};
	mark(readout.low_ev100, IM_COL32(255, 255, 255, 160));
	mark(readout.high_ev100, IM_COL32(255, 255, 255, 160));
	mark(readout.ev100, IM_COL32(255, 220, 64, 255));
}

static void xwa_window_exposure(void*) {
	XwaFlightExposureParams exposure = {};
	XwaRemasterFlight_GetExposure(&exposure);
	bool changed = XwaDebug_Checkbox("Metered", &exposure.metered);
	const bool metered = exposure.metered != 0;
	changed |= ImGui::SliderFloat(metered ? "EV100 anchor" : "EV100", &exposure.ev100, 0.0f, 20.0f, "%.2f");
	ImGui::SetItemTooltip(metered ? "The EV100 that partial metering starts from. Higher is darker."
								  : "Higher is darker.");
	changed |= ImGui::SliderFloat("Compensation", &exposure.compensation, -5.0f, 5.0f, "%+.2f EV");
	if (metered) {
		changed |= ImGui::SliderFloat("Metering", &exposure.metering, 0.0f, 1.0f, "%.2f");
		ImGui::SetItemTooltip("0 keeps the anchor, 1 follows the meter.");
		changed |= ImGui::DragFloatRange2("Limits", &exposure.min_ev100, &exposure.max_ev100, 0.1f, -6.0f,
										  24.0f, "min %.1f", "max %.1f");
		changed |= ImGui::SliderFloat("Brighten speed", &exposure.brighten_speed, 0.1f, 10.0f, "%.2f EV/s");
		changed |= ImGui::SliderFloat("Darken speed", &exposure.darken_speed, 0.1f, 10.0f, "%.2f EV/s");
		changed |= ImGui::DragFloatRange2("Percentiles", &exposure.low_percent, &exposure.high_percent, 0.5f,
										  0.0f, 100.0f, "low %.0f%%", "high %.0f%%");
		ImGui::SetItemTooltip("The meter averages the pixels between these percentiles of brightness.");
		changed |= ImGui::SliderFloat("Sky weight", &exposure.sky_weight, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::SliderFloat("Near weight", &exposure.near_weight, 0.0f, 1.0f, "%.2f");
		ImGui::SetItemTooltip("For geometry closer than the near distance: the cockpit.");
		changed |= ImGui::SliderFloat("Near distance", &exposure.near_distance, 0.0f, 20.0f, "%.1f m");
		changed |= ImGui::SliderFloat("Edge weight", &exposure.edge_weight, 0.0f, 1.0f, "%.2f");
		ImGui::SetItemTooltip("The weight at the corners of the screen. The center has 1.");
		changed |= XwaDebug_Checkbox("Meter the largest channel", &exposure.max_channel);
		ImGui::SetItemTooltip("The largest of red, green and blue, instead of the luminance.");
		ImGui::SameLine();
		changed |= XwaDebug_Checkbox("Freeze", &exposure.freeze);
	}
	if (XwaDebug_ResetButton()) {
		XwaRemasterFlight_GetExposureDefault(&exposure);
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetExposure(&exposure);
	}

	ImGui::Separator();
	AeronSceneExposureReadout readout;
	if (metered && XwaRemasterFlight_GetExposureReadout(&readout)) {
		ImGui::Text("EV100 %.2f, target %.2f, metered %.2f%s", (double)readout.ev100,
					(double)readout.target_ev100, (double)readout.metered_ev100,
					readout.metered ? "" : " (nothing to meter)");
		ImGui::Text("Averaged from EV100 %.1f to %.1f, %.0f%% of the weight on geometry",
					(double)readout.low_ev100, (double)readout.high_ev100, 100.0 * readout.geometry_share);
		xwa_exposure_histogram(readout);
	} else if (metered) {
		ImGui::TextDisabled("No metering yet.");
	} else {
		ImGui::Text("Scene values are multiplied by %.4g before the tonemap.",
					(double)XwaRemasterFlight_Exposure());
		ImGui::SetItemTooltip("EV100 %.3f shows a scene value of 1 as 1.",
							  (double)AERON_SCENE_UNIT_EXPOSURE_EV100);
	}

	const int view = XwaRemasterFlight_PresentDebugView();
	if (view == AERON_SCENE_PRESENT_FALSE_COLOR) {
		ImGui::SeparatorText("False color");
		XwaDebugView_DrawFalseColorLegend();
	} else if (view == AERON_SCENE_PRESENT_METER_WEIGHT) {
		ImGui::SeparatorText("Meter weights");
		ImGui::TextUnformatted("Blue: sky. Orange: near geometry. Green: other geometry.\n"
							   "Brighter counts more. Black isn't counted.");
	}
}

/* ---- Tonemap and bloom ----------------------------------------------------- */

static void xwa_window_tonemap(void*) {
	int op = AeronScenePresent_TonemapOp();
	ImGui::TextUnformatted("Tonemap:");
	ImGui::SameLine();
	ImGui::RadioButton("AgX", &op, AERON_SCENE_TONEMAP_AGX_PARAMETRIC);
	ImGui::SameLine();
	ImGui::RadioButton("ACES", &op, AERON_SCENE_TONEMAP_ACES);
	if (op != AeronScenePresent_TonemapOp()) {
		AeronScenePresent_SetTonemapOp(op);
	}

	if (op == AERON_SCENE_TONEMAP_ACES) {
		float aces_exposure = AeronScenePresent_AcesExposure();
		if (ImGui::SliderFloat("ACES pre-exposure", &aces_exposure, 1.0f, 3.0f, "%.2fx")) {
			AeronScenePresent_SetAcesExposure(aces_exposure);
		}
	} else {
		float tail = AeronScenePresent_EotfExponent();
		if (ImGui::SliderFloat("Tail exponent", &tail, 1.8f, 2.6f, "%.3f")) {
			AeronScenePresent_SetEotfExponent(tail);
		}
		int look = AeronScenePresent_AgxLook();
		ImGui::TextUnformatted("Look:");
		ImGui::SameLine();
		ImGui::RadioButton("Base", &look, AERON_SCENE_AGX_LOOK_BASE);
		ImGui::SameLine();
		ImGui::RadioButton("Punchy", &look, AERON_SCENE_AGX_LOOK_PUNCHY);
		if (look != AeronScenePresent_AgxLook()) {
			AeronScenePresent_SetAgxLook(look);
		}
		if (look == AERON_SCENE_AGX_LOOK_PUNCHY) {
			float power = AeronScenePresent_AgxPunchyPower();
			if (ImGui::SliderFloat("Punchy power", &power, 0.5f, 2.0f, "%.2f")) {
				AeronScenePresent_SetAgxPunchyPower(power);
			}
			float saturation = AeronScenePresent_AgxPunchySaturation();
			if (ImGui::SliderFloat("Punchy saturation", &saturation, 0.0f, 2.0f, "%.2f")) {
				AeronScenePresent_SetAgxPunchySaturation(saturation);
			}
			float highlight_saturation = AeronScenePresent_AgxHighlightSaturation();
			if (ImGui::SliderFloat("Highlight saturation", &highlight_saturation, 0.0f, 3.0f, "%.2f")) {
				AeronScenePresent_SetAgxHighlightSaturation(highlight_saturation);
			}
			float range[2];
			AeronScenePresent_AgxHighlightRange(&range[0], &range[1]);
			if (ImGui::SliderFloat2("Highlights (start, end)", range, -2.0f, 12.0f, "%.1f EV")) {
				AeronScenePresent_SetAgxHighlightRange(range[0], range[1]);
			}
			float hue = AeronScenePresent_AgxHighlightHue();
			if (ImGui::SliderFloat("Highlight hue kept", &hue, 0.0f, 1.0f, "%.2f")) {
				AeronScenePresent_SetAgxHighlightHue(hue);
			}
		}
	}

	ImGui::SeparatorText("Bloom");
	int kernel = AeronScenePresent_BloomKernel();
	ImGui::TextUnformatted("Kernel:");
	ImGui::SameLine();
	ImGui::RadioButton("4 taps", &kernel, AERON_SCENE_BLOOM_KERNEL_4_TAP);
	ImGui::SameLine();
	ImGui::RadioButton("1 tap", &kernel, AERON_SCENE_BLOOM_KERNEL_1_TAP);
	if (kernel != AeronScenePresent_BloomKernel()) {
		AeronScenePresent_SetBloomKernel(kernel);
	}
	float intensity = XwaRemasterFlight_BloomIntensity();
	if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 0.25f, "%.3f")) {
		XwaRemasterFlight_SetBloomIntensity(intensity);
	}
	ImGui::SetItemTooltip("The share of the light that scatters. The player's bloom amount (%.0f%%)\n"
						  "multiplies it.",
						  (double)(XwaRemasterFlight_BloomAmount() * 100.0f));
}

/* ---- Motion blur ------------------------------------------------------------ */

static void xwa_window_motion_blur(void*) {
	XwaFlightMotionBlurParams params;
	XwaRemasterFlight_GetMotionBlur(&params);
	static const char* quality_names[] = { "Off", "Low", "High" };
	bool changed = ImGui::Combo("Quality", &params.quality, quality_names, 3);
	changed |= ImGui::SliderFloat("Shutter", &params.shutter, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip("The share of a 32 ms game frame. 0.5 is a 180 degree shutter.");
	changed |= XwaDebug_Checkbox("Camera blur", &params.camera_blur);
	ImGui::SetItemTooltip("The camera's own movement blurs the image too.");
	changed |= XwaDebug_Checkbox("Blur while paused", &params.pause_keep_blur);
	if (changed) {
		XwaRemasterFlight_SetMotionBlur(&params);
	}
}

void XwaDebugImage_Register(void) {
	XwaDebug_AddWindow(XWA_DEBUG_MENU_IMAGE, "Exposure", xwa_window_exposure, 420, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_IMAGE, "Tonemap & Bloom", xwa_window_tonemap, 400, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_IMAGE, "Motion Blur", xwa_window_motion_blur, 360, 0);
}
