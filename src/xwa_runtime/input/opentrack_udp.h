#ifndef XWA_RUNTIME_OPENTRACK_UDP_H
#define XWA_RUNTIME_OPENTRACK_UDP_H

#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/input/trackir.h"

/* Loopback-only OpenTrack "UDP over network" provider, port 4242.
 * Returns the latest valid pose, or zero when unavailable/stale.
 * Does not depend on the OpenTrack application or its libraries. */
int XwaOpenTrackUdp_Poll(XwaTrackIRPose* pose, const XwaHeadTrackingOptions* options);
void XwaOpenTrackUdp_Shutdown(void);

#endif
