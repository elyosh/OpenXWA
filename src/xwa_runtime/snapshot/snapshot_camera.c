#include "xwa_runtime/snapshot/snapshot_camera.h"

#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"

/* The fraction keeps a copy of the integer position it belongs to, and is used
   only while that copy still matches the camera position. Code that moves the
   camera after it is placed, such as the collision push, makes them differ. */
static double s_fraction[3];
static int32_t s_source[3];
static int s_valid;

/* The camera of ordinary flight, kept during hyperspace. */
static float s_ordinary_rows[9];
static int32_t s_ordinary_position[3];
static float s_ordinary_eye_offset[3];
static int s_ordinary_valid;

void XwaSnapshotCamera_BeginUpdate(int playerIdx) {
	if (playerIdx == g_localPlayer) {
		s_valid = 0;
		s_ordinary_valid = 0;
	}
}

void XwaSnapshotCamera_NoteHyperspaceOverride(int playerIdx) {
	if (playerIdx != g_localPlayer) {
		return;
	}
	const PlayerViewState* view = &g_players[playerIdx].viewState;
	FVIEW_CopyRenderCameraRows(s_ordinary_rows);
	s_ordinary_position[0] = view->savedTargetX;
	s_ordinary_position[1] = view->savedTargetY;
	s_ordinary_position[2] = view->savedTargetZ;
	s_ordinary_eye_offset[0] = g_players[playerIdx].seatEyeWorldOffsetX;
	s_ordinary_eye_offset[1] = g_players[playerIdx].seatEyeWorldOffsetY;
	s_ordinary_eye_offset[2] = g_players[playerIdx].seatEyeWorldOffsetZ;
	s_ordinary_valid = 1;
}

int XwaSnapshotCamera_OrdinaryCamera(float rows[9], int32_t position[3], float eye_offset[3]) {
	if (!s_ordinary_valid) {
		return 0;
	}
	for (int i = 0; i < 9; i++) {
		rows[i] = s_ordinary_rows[i];
	}
	for (int axis = 0; axis < 3; axis++) {
		position[axis] = s_ordinary_position[axis];
		eye_offset[axis] = s_ordinary_eye_offset[axis];
	}
	return 1;
}

void XwaSnapshotCamera_NoteExternalCamera(int playerIdx, int focusX, int focusY, int focusZ, double offset) {
	if (playerIdx != g_localPlayer) {
		return;
	}

	/* The forward row, from the smooth camera matrix when there is one. */
	float rows[9];
	FVIEW_CopyRenderCameraRows(rows);

	const PlayerViewState* view = &g_players[playerIdx].viewState;
	const int focus[3] = { focusX, focusY, focusZ };
	const int position[3] = { view->savedTargetX, view->savedTargetY, view->savedTargetZ };
	for (int axis = 0; axis < 3; axis++) {
		s_fraction[axis] = (double)focus[axis] - offset * (double)rows[6 + axis] - (double)position[axis];
		s_source[axis] = position[axis];
	}
	s_valid = 1;
}

void XwaSnapshotCamera_PositionFraction(const int32_t position[3], float fraction[3]) {
	const int matches =
		s_valid && s_source[0] == position[0] && s_source[1] == position[1] && s_source[2] == position[2];
	for (int axis = 0; axis < 3; axis++) {
		fraction[axis] = matches ? (float)s_fraction[axis] : 0.0f;
	}
}
