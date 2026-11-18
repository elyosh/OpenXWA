#include "options/video_page.h"

#include "aeron/render.h"
#include "aeron/xr.h"
#include "options/options_window.h"
#include "xwa_runtime/config/modern_camera_options.h"
#include "xwa_runtime/config/modern_camera_shake_options.h"
#include "xwa_runtime/config/modern_target_readout_options.h"
#include "xwa_runtime/config/modern_video_options.h"

#include <math.h>

/* Changes apply at once, and Close saves them. The controls and labels match
 * the OpenXW Video tab. */

static bool XwaVideoPage_Display(AeronUiContext* ui, XwaModernVideoOptions* options) {
	static const char* const window_modes[] = { "Windowed", "Fullscreen", "Span Displays" };
	int window_mode = (int)options->window_mode;
	bool changed = false;

	AeronUi_Header(ui, "Display");
	if (AeronUi_Selector(ui, "Window Mode", &window_mode, window_modes, 3)) {
		options->window_mode = (XwaModernWindowMode)window_mode;
		changed = true;
	}
	if (options->window_mode == XWA_MODERN_WINDOW_MODE_SPAN)
		AeronUi_Help(ui, "Stretches one window across the displays that sit side by side. Uses "
						 "fullscreen when the system doesn't allow it.");
	changed |= AeronUi_Toggle(ui, "HDR Output", &options->hdr_output);
	if (options->hdr_output && !Aeron_OutputHdrEnabled())
		AeronUi_Help(ui, Aeron_OutputHdrStatusName(Aeron_OutputHdrStatus()));
#ifndef __APPLE__
	/* On macOS, the system controls the SDR gamma and the paper white. sRGB
	   is only listed when config.yaml sets it, because under Windows HDR it
	   looks washed out, like the desktop. */
	static const char* const gamma_names[] = { "2.2", "2.4", "sRGB" };
	int gamma = (int)options->sdr_gamma;
	int gamma_count = options->sdr_gamma == XWA_MODERN_SDR_GAMMA_SRGB ? 3 : 2;
	if (AeronUi_Selector(ui, "SDR Content Gamma", &gamma, gamma_names, gamma_count)) {
		options->sdr_gamma = (XwaModernSdrGamma)gamma;
		changed = true;
	}
	int automatic = options->paper_white == XWA_MODERN_PAPER_WHITE_AUTO;
	if (AeronUi_Toggle(ui, "Automatic Paper White", &automatic)) {
		options->paper_white = automatic ? XWA_MODERN_PAPER_WHITE_AUTO : XWA_MODERN_PAPER_WHITE_200;
		changed = true;
	}
	if (!automatic) {
		static const char* const nits[] = { "100 nits", "150 nits", "200 nits",
											"250 nits", "300 nits", "400 nits" };
		int index = (int)options->paper_white - XWA_MODERN_PAPER_WHITE_100;
		if (AeronUi_Selector(ui, "Paper White", &index, nits, 6)) {
			options->paper_white = (XwaModernPaperWhite)(index + XWA_MODERN_PAPER_WHITE_100);
			changed = true;
		}
	}
#endif
	return changed;
}

static bool XwaVideoPage_SurroundSize(AeronUiContext* ui, const char* label, float* value, float min,
									  float max, int step, const char* format) {
	int shown = (int)lroundf(*value);
	if (!AeronUi_SliderInt(ui, label, &shown, (int)min, (int)max, step, format))
		return false;
	*value = (float)shown;
	return true;
}

static bool XwaVideoPage_Surround(AeronUiContext* ui, XwaModernSurroundOptions* surround) {
	bool changed = false;

	AeronUi_Header(ui, "Surround Screens");
	changed |= AeronUi_Toggle(ui, "Surround", &surround->enabled);
	AeronUi_Help(ui, "Draws the flight view across three screens of the same size, with the right "
					 "perspective on each. The window must cover all three (see Span Displays). FSR "
					 "upscaling is off in this mode, so use MSAA.");
	if (!surround->enabled)
		return changed;
	changed |= XwaVideoPage_SurroundSize(ui, "Screen Width", &surround->screen_width_mm,
										 XWA_MODERN_SURROUND_SIZE_MIN_MM, XWA_MODERN_SURROUND_SIZE_MAX_MM, 1,
										 "%d mm");
	changed |= XwaVideoPage_SurroundSize(ui, "Screen Height", &surround->screen_height_mm,
										 XWA_MODERN_SURROUND_SIZE_MIN_MM, XWA_MODERN_SURROUND_SIZE_MAX_MM, 1,
										 "%d mm");
	AeronUi_Help(ui, "The visible picture of one screen, without the bezel.");
	changed |= XwaVideoPage_SurroundSize(ui, "Bezel", &surround->bezel_mm, 0.0f,
										 XWA_MODERN_SURROUND_BEZEL_MAX_MM, 1, "%d mm");
	AeronUi_Help(ui, "The width of the frame beside a screen's picture.");
	changed |=
		XwaVideoPage_SurroundSize(ui, "Distance", &surround->distance_mm, XWA_MODERN_SURROUND_DISTANCE_MIN_MM,
								  XWA_MODERN_SURROUND_DISTANCE_MAX_MM, 10, "%d mm");
	AeronUi_Help(ui, "From your eyes to the center screen.");
	changed |= XwaVideoPage_SurroundSize(ui, "Side Screen Angle", &surround->side_angle_deg, 0.0f,
										 XWA_MODERN_SURROUND_SIDE_ANGLE_MAX_DEG, 1, "%d degrees");
	AeronUi_Help(ui, "How far each side screen is turned toward you. 0 is flat.");
	return changed;
}

/* Set by the Restart Now button, until main.c takes it. */
static bool s_restart_requested;

bool XwaVideoPage_TakeRestartRequest(void) {
	const bool requested = s_restart_requested;
	s_restart_requested = false;
	return requested;
}

/* The VR mode is read when the game starts, so a change needs a restart. */
static bool XwaVideoPage_Vr(AeronUiContext* ui, XwaModernVideoOptions* options) {
	static const char* const modes[] = { "Off", "Automatic", "Required" };
	static int started_mode = -1;
	static int started_render_percent;
	int mode = (int)options->vr_mode;
	int render_percent = (int)lroundf(options->vr_render_scale * 100.0f);
	int world_percent = (int)lroundf(options->vr_world_scale * 100.0f);
	int panel_percent = (int)lroundf(options->vr_panel_scale * 100.0f);
	int screen_percent = (int)lroundf(options->vr_screen_scale * 100.0f);
	bool changed = false;

	if (!Aeron_XrSupported())
		return false;
	if (started_mode < 0) {
		started_mode = mode;
		started_render_percent = render_percent;
	}
	AeronUi_Header(ui, "Virtual Reality");
	if (AeronUi_Selector(ui, "VR Headset", &mode, modes, 3)) {
		options->vr_mode = (XwaModernVrMode)mode;
		changed = true;
	}
	AeronUi_Help(ui, "Automatic uses a headset when one is ready. Required shows an error and quits "
					 "when VR can't start. Applies the next time OpenXWA starts.");
	if (mode != XWA_MODERN_VR_OFF) {
		if (AeronUi_SliderInt(ui, "Headset Render Scale", &render_percent,
							  (int)(XWA_MODERN_VR_SCALE_MIN * 100.0f), (int)(XWA_MODERN_VR_SCALE_MAX * 100.0f), 5,
							  "%d%%")) {
			options->vr_render_scale = (float)render_percent / 100.0f;
			changed = true;
		}
		AeronUi_Help(ui, "The resolution of the headset's picture. 100% is what the headset recommends, "
						 "and lower is faster. Applies the next time OpenXWA starts.");
		if (AeronUi_SliderInt(ui, "World Scale", &world_percent, (int)(XWA_MODERN_VR_SCALE_MIN * 100.0f),
							  (int)(XWA_MODERN_VR_SCALE_MAX * 100.0f), 5, "%d%%")) {
			options->vr_world_scale = (float)world_percent / 100.0f;
			changed = true;
		}
		AeronUi_Help(ui, "How far the view moves when your head moves. Above 100%, the cockpit and the "
						 "world look smaller.");
		if (AeronUi_SliderInt(ui, "Cockpit Panel Size", &panel_percent,
							  (int)(XWA_MODERN_VR_PANEL_SCALE_MIN * 100.0f),
							  (int)(XWA_MODERN_VR_PANEL_SCALE_MAX * 100.0f), 5, "%d%%")) {
			options->vr_panel_scale = (float)panel_percent / 100.0f;
			changed = true;
		}
		AeronUi_Help(ui, "The size of the HUD panel in the cockpit: the radars, the gauges and the text.");
		if (AeronUi_SliderInt(ui, "Menu Screen Size", &screen_percent,
							  (int)(XWA_MODERN_VR_SCALE_MIN * 100.0f),
							  (int)(XWA_MODERN_VR_SCALE_MAX * 100.0f), 5, "%d%%")) {
			options->vr_screen_scale = (float)screen_percent / 100.0f;
			changed = true;
		}
		AeronUi_Help(ui, "The size of the flat screen that shows the menus, briefings and movies.");
		changed |= AeronUi_Toggle(ui, "Show Headset View in Window", &options->vr_mirror);
	}
	if ((mode != started_mode || (mode != XWA_MODERN_VR_OFF && render_percent != started_render_percent)) &&
		AeronUi_Button(ui, "Restart Now"))
		s_restart_requested = true;
	return changed;
}

static bool XwaVideoPage_FlightRendering(AeronUiContext* ui, XwaModernVideoOptions* options) {
	static const char* const quality[] = { "Off", "Low", "High" };
	static const char* const shadows[] = { "Standard", "High" };
	static const char* const fsr[] = { "Off", "Performance", "Balanced", "Quality", "Native AA" };
	static const char* const msaa[] = { "Off", "2x", "4x", "8x" };
	static const char* const blur[] = { "Off", "Low Quality", "High Quality" };
	bool changed = false;
	int index;

	AeronUi_Header(ui, "Flight Rendering");
	index = (int)options->ssao_quality;
	if (AeronUi_Selector(ui, "SSAO Quality", &index, quality, 3)) {
		options->ssao_quality = (XwaModernSsaoQuality)index;
		changed = true;
	}
	index = (int)options->shadow_quality;
	if (AeronUi_Selector(ui, "Shadow Quality", &index, shadows, 2)) {
		options->shadow_quality = (XwaModernShadowQuality)index;
		changed = true;
	}
	/* FSR and MSAA can't be on together: turning one on turns the other off. */
	index = (int)options->fsr_upscaling;
	if (AeronUi_Selector(ui, "FSR Upscaling", &index, fsr, 5)) {
		options->fsr_upscaling = (XwaModernFsrUpscaling)index;
		if (options->fsr_upscaling != XWA_MODERN_FSR_OFF)
			options->msaa = XWA_MODERN_MSAA_OFF;
		changed = true;
	}
	index = (int)options->msaa;
	if (AeronUi_Selector(ui, "MSAA", &index, msaa, 4)) {
		options->msaa = (XwaModernMsaa)index;
		if (options->msaa != XWA_MODERN_MSAA_OFF)
			options->fsr_upscaling = XWA_MODERN_FSR_OFF;
		changed = true;
	}
	index = (int)options->motion_blur_quality;
	if (AeronUi_Selector(ui, "Motion Blur", &index, blur, 3)) {
		options->motion_blur_quality = (XwaModernMotionBlurQuality)index;
		changed = true;
	}
	if (options->motion_blur_quality != XWA_MODERN_MOTION_BLUR_OFF) {
		/* The amount changes only when the player moves the slider, so a
		   config.yaml value between two steps is kept. */
		int amount = (int)lroundf(options->motion_blur_amount * 100.0f);
		if (AeronUi_SliderInt(ui, "Motion Blur Amount", &amount, 0, 100, 10, "%d%%")) {
			options->motion_blur_amount = (float)amount / 100.0f;
			changed = true;
		}
	}
	return changed;
}

static bool XwaVideoPage_HudScale(AeronUiContext* ui, XwaModernVideoOptions* options) {
	/* config.yaml can have any value from 0.5 to 2. The selector shows the
	   nearest step, and the value changes only when the player picks another
	   step. */
	static const float values[] = {
		0.5f, 0.6f, 0.7f, 0.75f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f
	};
	static const char* const labels[] = { "50%",  "60%",  "70%",  "75%",  "80%",  "90%",
										  "100%", "110%", "125%", "150%", "175%", "200%" };
	enum { count = sizeof values / sizeof values[0] };
	int index = 0;

	for (int i = 1; i < count; ++i) {
		if (fabsf(values[i] - options->hud_scale) < fabsf(values[index] - options->hud_scale))
			index = i;
	}
	if (!AeronUi_Selector(ui, "HUD Scale", &index, labels, count))
		return false;
	options->hud_scale = values[index];
	return true;
}

/* A switch that a mod can turn on. It follows the mod until the player
   changes it. */
static void XwaVideoPage_SectionToggle(AeronUiContext* ui, XwaOptionSection* section, int enabled,
									   const char* label, const char* help) {
	if (AeronUi_Toggle(ui, label, &enabled))
		XwaOptionSection_SetEnabled(section, enabled);
	AeronUi_Help(ui, help);
}

void XwaVideoPage_Draw(AeronUiContext* ui, const AeronInputSnapshot* input) {
	XwaModernVideoOptions options;
	XwaModernCameraOptions camera;
	XwaModernCameraShakeOptions camera_shake;
	XwaModernTargetReadoutOptions target_readout;
	bool changed;

	(void)input;
	if (!AeronUi_BeginScroll(ui, "Video settings", XwaOptionsWindow_ScrollHeight()))
		return;
	XwaModernVideoOptions_Get(&options);
	changed = XwaVideoPage_Display(ui, &options);
	changed |= XwaVideoPage_Surround(ui, &options.surround);
	changed |= XwaVideoPage_Vr(ui, &options);
	changed |= XwaVideoPage_FlightRendering(ui, &options);

	AeronUi_Header(ui, "HUD");
	changed |= XwaVideoPage_HudScale(ui, &options);
	if (changed && !XwaModernVideoOptions_Set(&options))
		XwaOptionsWindow_ReportError("Could not change the video options.");
	XwaModernTargetReadoutOptions_Get(&target_readout);
	XwaVideoPage_SectionToggle(ui, XwaModernTargetReadoutOptions_Section(), target_readout.enabled,
							   "Target Readout",
							   "Shows the target's name, cargo, distance and damage next to the target box.");

	AeronUi_Header(ui, "Camera");
	XwaModernCameraOptions_Get(&camera);
	XwaVideoPage_SectionToggle(
		ui, XwaModernCameraOptions_Section(), camera.enabled, "Enhanced Camera",
		"Adds camera lag, tilt and the HUD to the exterior view, and sway to the cockpit view.");
	XwaModernCameraShakeOptions_Get(&camera_shake);
	XwaVideoPage_SectionToggle(ui, XwaModernCameraShakeOptions_Section(), camera_shake.enabled,
							   "Camera Shake", "Shakes the camera during hyperspace jumps.");
	AeronUi_EndScroll(ui);
}
