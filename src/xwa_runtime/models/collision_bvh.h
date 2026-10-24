#ifndef XWA_RUNTIME_MODELS_COLLISION_BVH_H
#define XWA_RUNTIME_MODELS_COLLISION_BVH_H

/* Modern port: FaceData collision for GLB replacement models. Their FaceData
   nodes are stubs. The faces and a BVH are kept in the model's synthesizer side
   data, with the faces stored in the BVH's leaf order. Each candidate face goes
   through the same per-face code as a stock FaceData loop. The exact walk tests
   its candidates in ascending face order, so it gets the same result as a
   linear scan over the stored faces. */

#include "xwa/assets/opt_model.h"
#include "xwa/flight/object/collision.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One FaceData face of the exact and loose walks, moved verbatim from the
   loops of collide_TestSweepAgainstOptNode and
   collide_TestSweepAgainstOptNodeLoose. Stock FaceData loops and the
   replacement-model paths below both call them. */
void collide_TestSweepFace(const int* faceIndices, const Vec3f* faceNormal, const float* vertexBase);
void collide_TestSweepFaceLoose(const int* faceIndices, const float* faceNormal, const float* vertexBase);

/* Exact walk (collide_TestSweepAgainstOptNode): tests the faces whose BVH
   leaves the current walker segment can reach. */
void CollisionBvh_TestFaceData(OptimizedPolyObject* model, OptNode* faceDataNode);
/* Loose walk (collide_TestSweepAgainstOptNodeLoose): its edge-distance
   acceptance has no bounding box, so every face is tested, in authored
   order. */
void CollisionBvh_TestFaceDataLoose(OptimizedPolyObject* model, OptNode* faceDataNode);

#ifdef __cplusplus
}
#endif

#endif
