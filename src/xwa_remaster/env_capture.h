#ifndef XWA_REMASTER_ENV_CAPTURE_H
#define XWA_REMASTER_ENV_CAPTURE_H

/*
 * HD environment capture (development tool): renders the current region's
 * HD sky into six cube faces, renders each backdrop sun on its own as a
 * sprite, and writes them with the light list to
 *   <user root>/captures/environments/<mission>/region_<n>/
 * for `environment_bake --capture`.
 *
 * The debug action records a request; the remaster frame code runs it at
 * the start of the next HD flight frame, before that frame's command buffer
 * is acquired. The capture uses its own scenes and blocks on GPU fences.
 */

#include "xwa_remaster/assets.h"
#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XWA_ENV_CAPTURE_DEFAULT_FACE_SIZE 2048
#define XWA_ENV_CAPTURE_DEFAULT_SUN_RADIUS 0.27f

/* Nonzero when the snapshot is ordinary flight with a mission path: not
 * the hangar, hyperspace, the map or Death Star mode. `reason` (optional)
 * receives why not. */
int XwaRemasterEnvCapture_Available(const XwaSnapshot* snap, const char** reason);

/* Records a capture request: face size 1024, 2048 or 4096, and the sun
 * angular radius written to the lights (0.01 to 5 degrees). */
void XwaRemasterEnvCapture_Request(int face_size, float sun_angular_radius_degrees);
int XwaRemasterEnvCapture_Pending(void);

/* Runs a pending request against the snapshot, then clears it. The result
 * is logged and kept for XwaRemasterEnvCapture_Status. */
void XwaRemasterEnvCapture_Run(const XwaSnapshot* snap, XwaRemasterAssets* assets);

/* The last capture's outcome: the capture directory or the failure. Empty
 * before the first capture. */
const char* XwaRemasterEnvCapture_Status(void);

#ifdef __cplusplus
}
#endif

#endif
