#ifndef XWA_REMASTER_GLOW_MARKS_H
#define XWA_REMASTER_GLOW_MARKS_H

#include "aeron/scene/scene3d.h"
#include "aeron/scene/mesh.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"

/* Draws the hit marks of `object` on `mesh`. Glow marks are shown at
 * `emissive_strength` times the classic radiance. Marks in world segment
 * mode use `world_segment_emissive_strength` instead: the game makes them
 * from a hit's path and sizes them from the size of the whole craft, so on
 * the cockpit model they cover most of the cockpit. A mark whose strength
 * is 0 isn't drawn. With `draw_blast_marks` 0, the blast marks (the
 * long-lived scorch marks) aren't drawn either. */
void XwaRemasterGlowMarks_SubmitObject(AeronScene3D* scene, AeronCommandBuffer* command_buffer,
									   XwaRemasterAssets* assets, const XwaSnapshot* snapshot,
									   const XwaFlightObject* object, const AeronSceneMesh* mesh,
									   const float transform[16], const AeronSceneMeshTable* mesh_table,
									   float emissive_strength, float world_segment_emissive_strength,
									   int draw_blast_marks);
void XwaRemasterGlowMarks_InvalidateMesh(const AeronSceneMesh* mesh);
void XwaRemasterGlowMarks_Shutdown(void);

#endif
