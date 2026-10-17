#ifndef XWA_REMASTER_PROBE_OUTLINES_H
#define XWA_REMASTER_PROBE_OUTLINES_H

/*
 * Debug outlines of the frame's probe sets (Environment tool, Show probes):
 * each probe's influence, its full-weight inner shape and its parallax
 * volume, the light grid's box, and the grid points near the camera. Drawn
 * after the scene's meshes, tested against their depth.
 */

#include "aeron/scene/probe_set.h"
#include "aeron/scene/scene3d.h"
#include "xwa_remaster/flight.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A placed probe set and the model matrix of its owner object (model
 * metres to scene units, as passed to AeronScene_AddProbeSet). */
typedef struct XwaProbeOutlineSet {
	const AeronProbeSet* probes;
	float model[16];
} XwaProbeOutlineSet;

/* Builds and uploads the outlines and sets the scene's after-meshes
 * callback. `world_to_eye` rotates scene units into the view's eye space.
 * Returns 0 when the list cannot be created or uploaded. */
int XwaRemasterProbeOutlines_Prepare(AeronCommandBuffer* cmd, AeronScene3D* scene,
									 const XwaRemasterFlightView* view, const float world_to_eye[9],
									 const XwaProbeOutlineSet* sets, uint32_t count);
void XwaRemasterProbeOutlines_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
