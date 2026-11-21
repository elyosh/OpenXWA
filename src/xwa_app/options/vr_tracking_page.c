#include "options/vr_tracking_page.h"

#include "aeron/head_tracker.h"
#include "aeron/xr.h"
#include "options/options_window.h"
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/config/modern_video_options.h"

#include <math.h>
#include <stdio.h>

/* Changes apply at once, and Close saves them. */

const char* XwaVrTrackingPage_Title(void) { return Aeron_XrSupported() ? "VR & Tracking" : "Head Tracking"; }

/* Set by the Restart Now button, until main.c takes it. */
static bool s_restart_requested;

bool XwaVrTrackingPage_TakeRestartRequest(void) {
	const bool requested = s_restart_requested;
	s_restart_requested = false;
	return requested;
}

/* The VR mode is read when the game starts, so a change needs a restart. */
static bool XwaVrTrackingPage_Headset(AeronUiContext* ui, XwaModernVideoOptions* options) {
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
	AeronUi_Header(ui, "Headset");
	if (AeronUi_Toggle(ui, "VR Headset", &mode)) {
		options->vr_mode = mode ? XWA_MODERN_VR_ON : XWA_MODERN_VR_OFF;
		changed = true;
	}
	AeronUi_Help(ui, "Applies the next time OpenXWA starts.");
	if (mode != XWA_MODERN_VR_OFF) {
		if (AeronUi_SliderInt(ui, "Headset Render Scale", &render_percent,
							  (int)(XWA_MODERN_VR_SCALE_MIN * 100.0f),
							  (int)(XWA_MODERN_VR_SCALE_MAX * 100.0f), 5, "%d%%")) {
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

/* Writes "12 degrees right": the value rounded, with the word for its
 * direction. */
static void XwaVrTrackingPage_FormatAxis(char* out, size_t capacity, float value, const char* singular,
										 const char* plural, const char* positive, const char* negative) {
	const long rounded = lroundf(value);
	const long size = rounded < 0 ? -rounded : rounded;
	const char* unit = size == 1 ? singular : plural;
	if (size == 0)
		snprintf(out, capacity, "0 %s", unit);
	else
		snprintf(out, capacity, "%ld %s %s", size, unit, rounded < 0 ? negative : positive);
}

/* Shows the head's pose as numbers, so the player can check each axis
 * without flying. */
static void XwaVrTrackingPage_PoseReadout(AeronUiContext* ui) {
	AeronViewPose pose;
	if (!Aeron_HeadTrackerPose(&pose))
		return;
	/* The orientation as yaw, then pitch, then roll, each around the head's
	 * own axes. These are the matrix entries that this needs. The rig's Y is
	 * up and its -Z is forward, so a positive yaw turns left and a positive
	 * roll tilts left. */
	const float x = pose.orientation[0];
	const float y = pose.orientation[1];
	const float z = pose.orientation[2];
	const float w = pose.orientation[3];
	const float m13 = 2.0f * (x * z + y * w);
	const float m21 = 2.0f * (x * y + z * w);
	const float m22 = 1.0f - 2.0f * (x * x + z * z);
	const float m23 = 2.0f * (y * z - x * w);
	const float m33 = 1.0f - 2.0f * (x * x + y * y);
	const float degrees = 180.0f / 3.14159265f;
	const float sin_pitch = fmaxf(-1.0f, fminf(1.0f, -m23));
	const float turn = -atan2f(m13, m33) * degrees;
	const float look = asinf(sin_pitch) * degrees;
	const float tilt = -atan2f(m21, m22) * degrees;

	char turn_text[48], look_text[48], tilt_text[48], line[192];
	XwaVrTrackingPage_FormatAxis(turn_text, sizeof turn_text, turn, "degree", "degrees", "right", "left");
	XwaVrTrackingPage_FormatAxis(look_text, sizeof look_text, look, "degree", "degrees", "up", "down");
	XwaVrTrackingPage_FormatAxis(tilt_text, sizeof tilt_text, tilt, "degree", "degrees", "right", "left");
	snprintf(line, sizeof line, "Turn %s, look %s, tilt %s.", turn_text, look_text, tilt_text);
	AeronUi_Help(ui, line);

	char right_text[48], up_text[48], forward_text[48];
	XwaVrTrackingPage_FormatAxis(right_text, sizeof right_text, pose.position_m[0] * 100.0f, "cm", "cm",
								 "right", "left");
	XwaVrTrackingPage_FormatAxis(up_text, sizeof up_text, pose.position_m[1] * 100.0f, "cm", "cm", "up",
								 "down");
	XwaVrTrackingPage_FormatAxis(forward_text, sizeof forward_text, -pose.position_m[2] * 100.0f, "cm", "cm",
								 "forward", "back");
	snprintf(line, sizeof line, "Move %s, %s, %s.", right_text, up_text, forward_text);
	AeronUi_Help(ui, line);
}

static void XwaVrTrackingPage_HeadTracking(AeronUiContext* ui) {
	static XwaModernInputOptions options;
	XwaModernInputOptions_Get(&options);
	AeronUi_Header(ui, "Head Tracking");
	/* The choices that this build supports, and the setting for each. */
	const char* names[XWA_MODERN_HEAD_TRACKING_COUNT];
	XwaModernHeadTracking kinds[XWA_MODERN_HEAD_TRACKING_COUNT];
	int count = 0;
	names[count] = "Off";
	kinds[count++] = XWA_MODERN_HEAD_TRACKING_OFF;
	const bool trackir = Aeron_HeadTrackerAvailable(AERON_HEAD_TRACKER_TRACKIR) != 0;
	if (trackir) {
		names[count] = "TrackIR";
		kinds[count++] = XWA_MODERN_HEAD_TRACKING_TRACKIR;
	}
	names[count] = "OpenTrack";
	kinds[count++] = XWA_MODERN_HEAD_TRACKING_OPENTRACK;
	/* A saved choice that this build doesn't support shows as Off. */
	int index = 0;
	for (int i = 0; i < count; i++) {
		if (kinds[i] == options.head_tracking)
			index = i;
	}
	if (AeronUi_Selector(ui, "Tracker", &index, names, count)) {
		options.head_tracking = kinds[index];
		if (!XwaModernInputOptions_Set(&options))
			XwaOptionsWindow_ReportError("The head tracking options are invalid.");
	}
	if (!trackir && kinds[index] == XWA_MODERN_HEAD_TRACKING_OFF)
		AeronUi_Help(ui, "TrackIR works only on Windows. Here, use OpenTrack.");
	if (kinds[index] == XWA_MODERN_HEAD_TRACKING_TRACKIR) {
		AeronUi_Help(ui, "Also works with OpenTrack's \"freetrack 2.0 Enhanced\" output.");
	} else if (kinds[index] == XWA_MODERN_HEAD_TRACKING_OPENTRACK) {
		char help[160];
		snprintf(help, sizeof help,
				 "In OpenTrack, choose the \"UDP over network\" output, with the address 127.0.0.1 and the "
				 "port %d.",
				 options.head_tracking_port);
		AeronUi_Help(ui, help);
	}
	if (kinds[index] == XWA_MODERN_HEAD_TRACKING_OFF)
		return;
	/* Aeron doesn't read the tracker while a VR session runs. */
	if (Aeron_XrSessionRunning()) {
		AeronUi_Help(ui, "In VR, the headset moves the view, and the tracker isn't used.");
		return;
	}
	switch (Aeron_HeadTrackerStatus()) {
		case AERON_HEAD_TRACKER_TRACKING:
			AeronUi_Help(ui, "Receiving your head movement.");
			XwaVrTrackingPage_PoseReadout(ui);
			break;
		case AERON_HEAD_TRACKER_WAITING:
			AeronUi_Help(ui, "Not receiving any head movement.");
			break;
		case AERON_HEAD_TRACKER_FAILED:
			AeronUi_Error(ui, Aeron_HeadTrackerError());
			break;
		default:
			break;
	}
}

static void XwaVrTrackingPage_VrControls(AeronUiContext* ui) {
	static XwaModernInputOptions options;
	XwaModernInputOptions_Get(&options);
	AeronUi_Header(ui, "VR Controls");
	bool changed = AeronUi_Toggle(ui, "Show Hands", &options.vr_show_hands);
	AeronUi_Help(ui, "Shows your hands in the cockpit, where your VR controllers are.");
	changed |= AeronUi_Toggle(ui, "Use the Mod's Hands", &options.vr_mod_hands) != 0;
	AeronUi_Help(ui, "When off, the game's own hands are shown even if the mod has some.");
	changed |= AeronUi_Toggle(ui, "Show Controllers in Menus", &options.vr_menu_controllers) != 0;
	AeronUi_Help(ui, "In the menus, shows your controllers when the headset provides their models. "
					 "Otherwise, your hands are shown.");
	changed |= AeronUi_Toggle(ui, "Use the Mod's Keyboard", &options.vr_mod_keyboard) != 0;
	AeronUi_Help(ui, "When off, the game's own virtual keyboard is shown even if the mod has one.");
	static const char* const stick_modes[] = { "Off", "Tilt", "Slide" };
	static const char* const hand_names[] = { "Left", "Right" };
	int stick = (int)options.vr_stick;
	if (AeronUi_Selector(ui, "Virtual Stick", &stick, stick_modes, XWA_MODERN_VR_STICK_MODE_COUNT)) {
		options.vr_stick = (XwaModernVrStickMode)stick;
		changed = true;
	}
	if (options.vr_stick == XWA_MODERN_VR_STICK_TILT)
		AeronUi_Help(ui, "Hold the grip and tilt your hand to steer. Twist it to roll.");
	else if (options.vr_stick == XWA_MODERN_VR_STICK_SLIDE)
		AeronUi_Help(
			ui, "Hold the grip and move your hand sideways, forward or back to steer. Twist it to roll.");
	changed |= AeronUi_Selector(ui, "Stick Hand", &options.vr_stick_hand, hand_names, 2) != 0;
	changed |= AeronUi_Toggle(ui, "Virtual Throttle", &options.vr_throttle) != 0;
	AeronUi_Help(ui, "Hold the grip of your other hand and move it forward or back.");
	int range_percent = (int)lroundf(options.vr_stick_range * 100.0f);
	if (AeronUi_SliderInt(ui, "Stick Range", &range_percent, (int)(XWA_MODERN_VR_STICK_RANGE_MIN * 100.0f),
						  (int)(XWA_MODERN_VR_STICK_RANGE_MAX * 100.0f), 10, "%d%%")) {
		options.vr_stick_range = (float)range_percent / 100.0f;
		changed = true;
	}
	AeronUi_Help(ui, "How far your hand moves for the stick's full travel.");
	changed |= AeronUi_Toggle(ui, "Cockpit Buttons", &options.vr_cockpit_buttons) != 0;
	AeronUi_Help(ui, "Point at a cockpit button and pull the trigger to press it.");
	if (changed && !XwaModernInputOptions_Set(&options))
		XwaOptionsWindow_ReportError("The VR options are invalid.");
}

void XwaVrTrackingPage_Draw(AeronUiContext* ui, const AeronInputSnapshot* input) {
	XwaModernVideoOptions video;

	(void)input;
	if (!AeronUi_BeginScroll(ui, "VR and tracking settings", XwaOptionsWindow_ScrollHeight()))
		return;
	XwaModernVideoOptions_Get(&video);
	if (XwaVrTrackingPage_Headset(ui, &video) && !XwaModernVideoOptions_Set(&video))
		XwaOptionsWindow_ReportError("Could not change the video options.");
	XwaVrTrackingPage_HeadTracking(ui);
	if (Aeron_XrSupported() && video.vr_mode != XWA_MODERN_VR_OFF)
		XwaVrTrackingPage_VrControls(ui);
	AeronUi_EndScroll(ui);
}
