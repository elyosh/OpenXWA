#include "xwa_runtime/config/modern_head_tracking_screen.h"

#include "xwa/assets/string_table.h"
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/config/modern_options_menu.h"
#include "xwa_runtime/input/trackir.h"

#include <stdint.h>

int XwaModernHeadTrackingScreen_Update(int menu_center_x, int* cursor_row) {
	static const char* const toggle_texts[] = { "Off", "On" };
	#ifdef _WIN32
	static const char* const source_texts[] = { "TrackIR", "OpenTrack UDP" };
#else
	static const char* const source_texts[] = { "OpenTrack UDP" };
#endif
	static const char* const invert_labels[XWA_HEAD_TRACK_AXIS_COUNT] = {
		"Invert Yaw", "Invert Pitch", "Invert Roll",
		"Invert X", "Invert Y", "Invert Z"
	};
	XwaModernInputOptions options;
	XwaModernOptionsMenu menu;
	uint8_t enabled;
	uint8_t source;
	uint8_t invert[XWA_HEAD_TRACK_AXIS_COUNT];
	int changed = 0;
	int previous_enabled;
	int axis;
	int back;

	if (!cursor_row) return 0;
	XwaModernInputOptions_Get(&options);
	enabled = (uint8_t)(options.head_tracking.enabled != 0);
	previous_enabled = enabled;
	#ifdef _WIN32
	source = (uint8_t)options.head_tracking.source;
#else
	/* Native NPClient is Windows-only; the one visible choice is UDP. */
	source = 0;
#endif
	for (axis = 0; axis < XWA_HEAD_TRACK_AXIS_COUNT; ++axis) {
		invert[axis] = (uint8_t)(options.head_tracking.invert[axis] != 0);
	}
	/* One master switch, one source, six inversion controls, and Back. */
	XwaModernOptionsMenu_Begin(&menu, menu_center_x, 120, cursor_row, 9);
	XwaModernOptionsMenu_DrawTitle(&menu, "Head Tracking Setup");

	changed |= XwaModernOptionsMenu_DrawCycleU8(
		&menu, &enabled, "Head Tracking Enabled", toggle_texts, 2, 160, 0);
	changed |= XwaModernOptionsMenu_DrawCycleU8(
		&menu, &source, "Head Tracking Source", source_texts,
		(int)(sizeof(source_texts) / sizeof(source_texts[0])), 161, !enabled);
	for (axis = 0; axis < XWA_HEAD_TRACK_AXIS_COUNT; ++axis) {
		changed |= XwaModernOptionsMenu_DrawCycleU8(
			&menu, &invert[axis], invert_labels[axis], toggle_texts, 2, 162 + axis, !enabled);
	}
	if (changed) {
		options.head_tracking.enabled = enabled != 0;
		#ifdef _WIN32
		options.head_tracking.source = source;
#else
		options.head_tracking.source = XWA_HEAD_TRACK_SOURCE_OPENTRACK_UDP;
#endif
		for (axis = 0; axis < XWA_HEAD_TRACK_AXIS_COUNT; ++axis) {
			options.head_tracking.invert[axis] = invert[axis] != 0;
		}
		if (XwaModernInputOptions_Set(&options) && previous_enabled && !enabled) {
			/* Disconnect immediately, including when settings are changed
			 * outside a mission and no subsequent camera poll is expected. */
			XwaTrackIR_Shutdown();
		}
	}

	back = XwaModernOptionsMenu_DrawAction(&menu, FrontendString_Get(STR_BACK), 168, 0);
	back |= XwaModernOptionsMenu_TakeEscape(&menu);
	if (back) {
		XwaModernInputOptions_Flush();
		return 1;
	}
	return 0;
}
