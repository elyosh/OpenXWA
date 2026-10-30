#include "xwa_runtime/snapshot/snapshot_camera.h"

#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"

/* The fraction keeps a copy of the integer position it belongs to, and is used
   only while that copy still matches the camera position. Code that moves the
   camera after it is placed, such as the collision push, makes them differ. */
static double s_fraction[3];
static int32_t s_source[3];
static int s_valid;

void XwaSnapshotCamera_BeginUpdate(int playerIdx) {
	if (playerIdx == g_localPlayer) {
		s_valid = 0;
	}
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
