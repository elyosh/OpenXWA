#ifndef XWA_RUNTIME_ORIENTATION_HOOK_H
#define XWA_RUNTIME_ORIENTATION_HOOK_H

#include "xwa/flight/object/object.h"

typedef struct XwaOrientationAngles {
	Q16Angle yaw;
	Q16Angle pitch;
	Q16Angle roll;
} XwaOrientationAngles;

/*
 * Apply player pitch/yaw around the craft's current local axes. This is the
 * portable counterpart of the community XWA gimbal-lock hook.
 */
XwaOrientationAngles XwaOrientation_ApplyPitchYaw(XwaOrientationAngles current, int pitchDeltaQ16,
												   int negYawDeltaQ16);

#endif
