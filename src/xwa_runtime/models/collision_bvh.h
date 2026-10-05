#ifndef XWA_RUNTIME_MODELS_COLLISION_BVH_H
#define XWA_RUNTIME_MODELS_COLLISION_BVH_H

/* Modern port: FaceData collision for GLB replacement models. Their FaceData
 * nodes are stubs; the faces and a BVH live in the model's synthesizer side
 * data. Each candidate face runs through the same per-face code as a stock
 * FaceData loop, in ascending face order, so results match a linear scan. */

#include "xwa/assets/opt_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exact walk (collide_TestSweepAgainstOptNode): tests the faces whose BVH
   leaves the current walker segment can reach. */
void CollisionBvh_TestFaceData(OptimizedPolyObject* model, OptNode* faceDataNode);
/* Loose walk (collide_TestSweepAgainstOptNodeLoose): its edge-distance
   acceptance has no bounding box, so every face is tested in order. */
void CollisionBvh_TestFaceDataLoose(OptimizedPolyObject* model, OptNode* faceDataNode);

#ifdef __cplusplus
}
#endif

#endif
