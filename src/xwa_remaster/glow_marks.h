#ifndef XWA_REMASTER_GLOW_MARKS_H
#define XWA_REMASTER_GLOW_MARKS_H

#include "aeron/scene/scene3d.h"
#include "aeron/scene/mesh.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"

void XwaRemasterGlowMarks_SubmitObject(AeronScene3D* scene, AeronCommandBuffer* command_buffer,
									   XwaRemasterAssets* assets, const XwaSnapshot* snapshot,
									   const XwaFlightObject* object, const AeronSceneMesh* mesh,
									   const float transform[16], const AeronSceneMeshTable* mesh_table,
									   float emissive_strength);
void XwaRemasterGlowMarks_InvalidateMesh(const AeronSceneMesh* mesh);
/* Loads effects.yaml (animated blast marks) into the process effect atlas
 * through `command_buffer`; its absence leaves the classic marks. */
int XwaRemasterGlowMarks_PrepareProcess(AeronCommandBuffer* command_buffer);
void XwaRemasterGlowMarks_Shutdown(void);

#endif
