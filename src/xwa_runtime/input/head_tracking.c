/* Head tracking for the local player's camera. It keeps the pose of the
 * current flight frame and switches between the two sources. */
#include "xwa_runtime/input/head_tracking.h"
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/input/opentrack_udp.h"
#include "xwa_runtime/input/trackir.h"

#include <string.h>

/* Stored separately from recovered simulation/player state. */
static XwaHeadPose s_pose;
static int s_pose_valid;
static float s_camera_offset[3];

void XwaHeadTracking_ClearPose(void) {
	memset(&s_pose, 0, sizeof s_pose);
	memset(s_camera_offset, 0, sizeof s_camera_offset);
	s_pose_valid = 0;
}

void XwaHeadTracking_SetCameraOffset(const float world_offset[3]) {
	if (world_offset && s_pose_valid) {
		memcpy(s_camera_offset, world_offset, sizeof s_camera_offset);
	}
}

void XwaHeadTracking_GetCameraOffset(float world_offset[3]) {
	if (world_offset) {
		memcpy(world_offset, s_camera_offset, sizeof s_camera_offset);
	}
}

int XwaHeadTracking_CurrentPose(XwaHeadPose* out) {
	if (out) {
		*out = s_pose;
	}
	return s_pose_valid;
}

void XwaHeadTracking_Shutdown(void) {
	XwaOpenTrackUdp_Shutdown();
	XwaTrackIR_Shutdown();
	XwaHeadTracking_ClearPose();
}

int XwaHeadTracking_Poll(XwaHeadPose* out) {
	XwaHeadTrackingOptions options;
	int polled;

	if (!out) {
		return 0;
	}
	memset(out, 0, sizeof *out);
	XwaHeadTracking_ClearPose();
	XwaModernInputOptions_GetHeadTracking(&options);
	if (!options.enabled) {
		/* Disabled means no connection, polling, or leftover pose. */
		XwaOpenTrackUdp_Shutdown();
		XwaTrackIR_Shutdown();
		return 0;
	}
	/* Only one source stays connected; the other is released on a switch. */
	if (options.source == XWA_HEAD_TRACK_SOURCE_OPENTRACK_UDP) {
		XwaTrackIR_Shutdown();
		polled = XwaOpenTrackUdp_Poll(out, &options);
	} else {
		XwaOpenTrackUdp_Shutdown();
		polled = XwaTrackIR_Poll(out, &options);
	}
	if (!polled) {
		return 0;
	}
	s_pose = *out;
	s_pose_valid = 1;
	return 1;
}
