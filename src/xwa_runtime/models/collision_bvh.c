#include "xwa_runtime/models/collision_bvh.h"

#include "aeron/aeron.h"
#include "aeron/collision/face_bvh.h"
#include "aeron/log.h"
#include "xwa/flight/object/collision.h"
#include "xwa_runtime/models/opt_synth.h"

#include <stdint.h>

/* Larger of the swept (+-10.0) and local-sweep (+-1e-4) plane epsilons of
   collide_IntersectSegmentWithFacePlane, so one query covers both callers. */
#define COLLISION_BVH_PLANE_TOLERANCE 10.0f

static AeronFaceBvhScratch s_scratch;
static int s_scratchFailureLogged;

static const XwaOptSynthMesh* CollisionBvh_FindMesh(OptimizedPolyObject* model, OptNode* faceDataNode) {
	const XwaOptSynthMesh* mesh;

	mesh = OptModel_FindSynthMesh(model, faceDataNode);
	if (mesh == NULL) {
		/* The synthesizer creates an entry for every stub. */
		Aeron_LogCritical("xwa.collision", "GLB replacement model has a FaceData node without side data");
		Aeron_RequestFatalError("OpenXWA", "Internal error: a replacement model lost its collision data.");
	}
	return mesh;
}

void CollisionBvh_TestFaceData(OptimizedPolyObject* model, OptNode* faceDataNode) {
	const XwaOptSynthMesh* mesh;
	const float* vertexBase;
	uint32_t count;
	uint32_t index;

	mesh = CollisionBvh_FindMesh(model, faceDataNode);
	if (mesh == NULL) {
		return;
	}
	vertexBase = (const float*)g_collideCurrentMeshVertsNode->param2;
	count = AeronFaceBvh_Collect(mesh->bvh, &g_collideSweepWalkerStart.x, &g_collideSweepWalkerEnd.x,
								 COLLISION_BVH_PLANE_TOLERANCE, &s_scratch);
	if (count == UINT32_MAX) {
		/* Out of memory for candidates: every face, in order, gives the same
		   result. */
		if (!s_scratchFailureLogged) {
			s_scratchFailureLogged = 1;
			Aeron_LogWarn("xwa.collision", "BVH scratch allocation failed; testing every face");
		}
		for (index = 0; index < mesh->faceCount; ++index) {
			collide_TestSweepFace(mesh->faces[index].vertex_index, (const Vec3f*)mesh->faces[index].normal,
								  vertexBase);
		}
		return;
	}
	for (index = 0; index < count; ++index) {
		const AeronOptFace* face = &mesh->faces[s_scratch.indices[index]];

		collide_TestSweepFace(face->vertex_index, (const Vec3f*)face->normal, vertexBase);
	}
}

void CollisionBvh_TestFaceDataLoose(OptimizedPolyObject* model, OptNode* faceDataNode) {
	const XwaOptSynthMesh* mesh;
	const float* vertexBase;
	uint32_t index;

	mesh = CollisionBvh_FindMesh(model, faceDataNode);
	if (mesh == NULL) {
		return;
	}
	vertexBase = (const float*)g_collideCurrentMeshVertsNode->param2;
	for (index = 0; index < mesh->faceCount; ++index) {
		collide_TestSweepFaceLoose(mesh->faces[index].vertex_index, mesh->faces[index].normal, vertexBase);
	}
}
