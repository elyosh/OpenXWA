#ifndef XWA_RUNTIME_TRACKIR_H
#define XWA_RUNTIME_TRACKIR_H

#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/input/head_tracking.h"

/* Native TrackIR source, through the NPClient64.dll that TrackIR installs.
 * Windows only; elsewhere the poll always returns zero. No NaturalPoint SDK
 * headers, code, or DLLs are distributed. */
int XwaTrackIR_Poll(XwaHeadPose* pose, const XwaHeadTrackingOptions* options);
void XwaTrackIR_Shutdown(void);

#endif
