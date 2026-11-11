#include "options/head_tracking_page.h"

#include "options/options_window.h"
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/input/head_tracking.h"

#include <stdio.h>
#include <stdlib.h>

/* The port is typed as text. It applies once it is a whole number in range;
 * until then the field keeps the text and the last valid port stays in use. */
static bool XwaHeadTrackingPage_DrawPort(AeronUiContext* ui, XwaHeadTrackingOptions* head) {
	static char text[8];
	static int shown_port;
	char* end;
	long port;

	/* Restore Defaults or a reopened window changes the port under the field. */
	if (shown_port != head->udp_port) {
		snprintf(text, sizeof text, "%d", head->udp_port);
		shown_port = head->udp_port;
	}
	const int edited =
		AeronUi_InputText(ui, "OpenTrack UDP Port", text, sizeof text,
						  head->enabled ? AERON_UI_INPUT_TEXT_NONE : AERON_UI_INPUT_TEXT_READ_ONLY);
	port = strtol(text, &end, 10);
	if (end == text || *end || port < XWA_HEAD_TRACK_UDP_PORT_MIN || port > XWA_HEAD_TRACK_UDP_PORT_MAX) {
		AeronUi_Error(ui, "Enter a port from 1024 to 65535.");
		return false;
	}
	if (!edited || port == head->udp_port)
		return false;
	head->udp_port = (int)port;
	shown_port = head->udp_port;
	return true;
}

/* The head tracking options apply at once and are saved on Close. */
void XwaHeadTrackingPage_Draw(AeronUiContext* ui, const AeronInputSnapshot* input) {
#ifdef _WIN32
	static const char* const sources[] = { "TrackIR", "OpenTrack UDP" };
#else
	/* Native TrackIR is Windows-only; the one choice is UDP. */
	static const char* const sources[] = { "OpenTrack UDP" };
#endif
	static const char* const invert_labels[XWA_HEAD_TRACK_AXIS_COUNT] = { "Invert Yaw",  "Invert Pitch",
																		  "Invert Roll", "Invert X",
																		  "Invert Y",    "Invert Z" };
	static XwaModernInputOptions options;
	(void)input;
	XwaModernInputOptions_Get(&options);
	const int was_enabled = options.head_tracking.enabled != 0;
#ifdef _WIN32
	int source = options.head_tracking.source;
#else
	int source = 0;
#endif

	AeronUi_Header(ui, "Head Tracking");
	bool changed = AeronUi_Toggle(ui, "Head Tracking", &options.head_tracking.enabled);
	if (AeronUi_SelectorEnabled(ui, "Head Tracking Source", &source, sources,
								(int)(sizeof sources / sizeof sources[0]), options.head_tracking.enabled)) {
		changed = true;
	}
#ifdef _WIN32
	options.head_tracking.source = source;
#else
	options.head_tracking.source = XWA_HEAD_TRACK_SOURCE_OPENTRACK_UDP;
#endif
	if (options.head_tracking.source == XWA_HEAD_TRACK_SOURCE_TRACKIR) {
		AeronUi_Help(ui, "Uses the installed TrackIR software. Start it before flying.");
	} else {
		changed |= XwaHeadTrackingPage_DrawPort(ui, &options.head_tracking);
		AeronUi_Help(ui, "In OpenTrack, set the output to \"UDP over network\" with the address 127.0.0.1 "
						 "and the same port.");
	}

	AeronUi_Header(ui, "Inversion");
	AeronUi_Help(ui, "Inversion is relative to OpenXWA's standard head-motion directions.");
	for (int axis = 0; axis < XWA_HEAD_TRACK_AXIS_COUNT; ++axis)
		changed |= AeronUi_Toggle(ui, invert_labels[axis], &options.head_tracking.invert[axis]);

	if (AeronUi_Button(ui, "Restore Head Tracking Defaults")) {
		static XwaModernInputOptions defaults;
		XwaModernInputOptions_GetDefaults(&defaults);
		options.head_tracking = defaults.head_tracking;
		changed = true;
	}
	if (!changed)
		return;
	if (!XwaModernInputOptions_Set(&options)) {
		XwaOptionsWindow_ReportError("The head tracking options are invalid.");
		return;
	}
	/* Disconnect at once, also outside a mission where no camera poll
	 * would notice the change. */
	if (was_enabled && !options.head_tracking.enabled)
		XwaHeadTracking_Shutdown();
}
