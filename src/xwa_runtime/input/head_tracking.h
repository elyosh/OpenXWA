#ifndef XWA_RUNTIME_HEAD_TRACKING_H
#define XWA_RUNTIME_HEAD_TRACKING_H

#include <stdint.h>

/* The local player's head pose, from whichever source the options select:
 * native TrackIR (Windows) or OpenTrack UDP. */
typedef struct XwaHeadPose {
	int16_t yaw_q16;   /* negative = look left */
	int16_t pitch_q16; /* positive = look up */
	int16_t roll_q16;
	float left_cm;
	float up_cm;
	float back_cm;
} XwaHeadPose;

/* Returns zero if disabled, unavailable, unfocused, or the data is stale. */
int XwaHeadTracking_Poll(XwaHeadPose* out);
/* Last pose sampled for the current flight frame; zero when tracking is unavailable. */
int XwaHeadTracking_CurrentPose(XwaHeadPose* out);
void XwaHeadTracking_ClearPose(void);
/* Transient camera displacement in world/OPT units, not saved flight state.
 * The cockpit renderer needs this to keep the mesh anchored to the craft. */
void XwaHeadTracking_SetCameraOffset(const float world_offset[3]);
void XwaHeadTracking_GetCameraOffset(float world_offset[3]);
/* Closes whichever source is connected and clears the pose. */
void XwaHeadTracking_Shutdown(void);

#endif
