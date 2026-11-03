#ifndef XWA_RUNTIME_TRACKIR_H
#define XWA_RUNTIME_TRACKIR_H

#include <stdint.h>

/* Optional, Windows-only head pose. No NaturalPoint SDK files are included. */
typedef struct XwaTrackIRPose {
    int16_t yaw_q16;   /* negative = look left */
    int16_t pitch_q16; /* positive = look up */
    int16_t roll_q16;
    float left_cm;
    float up_cm;
    float back_cm;
} XwaTrackIRPose;

/* Returns zero if unavailable, unfocused, paused, or data is stale. */
int XwaTrackIR_Poll(XwaTrackIRPose* out);
/* Last pose sampled for the current flight frame; zero when tracking is unavailable. */
int XwaTrackIR_CurrentPose(XwaTrackIRPose* out);
void XwaTrackIR_ClearPose(void);
/* Transient camera displacement in world/OPT units, not saved flight state.
 * The cockpit renderer needs this to keep the mesh anchored to the craft. */
void XwaTrackIR_SetCameraOffset(const float world_offset[3]);
void XwaTrackIR_GetCameraOffset(float world_offset[3]);
void XwaTrackIR_Shutdown(void);

#endif
