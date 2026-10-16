#include "xwa_runtime/models/collision_bvh.h"

#include "aeron/aeron.h"
#include "aeron/collision/face_bvh.h"
#include "aeron/log.h"
#include "xwa/flight/object/collision.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/models/opt_synth.h"

#include <stdint.h>

/* Verbatim body of collide_TestSweepAgainstOptNode's FaceData loop. */
void collide_TestSweepFace(const int* faceIndices, const Vec3f* faceNormal, const float* vertexBase) {
	Vec3f point;
	float axisAngle[4];
	float rotationMatrix[16];
	float hitT;
	Vec3f* normal;
	Vec3f* uAxisRef;
	const Vec3f* faceVertex;

	if (collide_IntersectSegmentWithFacePlane(&faceNormal->x, &vertexBase[3 * faceIndices[0]],
											  &g_collideSweepWalkerStart.x, &g_collideSweepWalkerEnd.x,
											  &hitT) &&
		(!g_collideSweepRejectNearStartHits || hitT >= g_collideSweepHitBackoff) &&
		hitT < g_collideSweepHitFraction) {
		point.x =
			(g_collideSweepWalkerEnd.x - g_collideSweepWalkerStart.x) * hitT + g_collideSweepWalkerStart.x;
		point.y =
			(g_collideSweepWalkerEnd.y - g_collideSweepWalkerStart.y) * hitT + g_collideSweepWalkerStart.y;
		point.z =
			(g_collideSweepWalkerEnd.z - g_collideSweepWalkerStart.z) * hitT + g_collideSweepWalkerStart.z;

		if (collide_PointInFacePolygon(&faceNormal->x, vertexBase, faceIndices, &point.x)) {
			g_collideSweepHitMeshOrdinal = g_collideSweepCurrentMeshOrdinal;
			g_collideSweepHitFraction = hitT;
			g_glowMarkWorldSegmentMode = 0;

			if (g_collideCurrentMeshRotationAngle != g_collideZeroFloat) {
				float rotationAxisZ;

				normal = g_glowMarkScratchNormalVec;
				*normal = *faceNormal;
				faceVertex = (const Vec3f*)&vertexBase[3 * faceIndices[0]];
				uAxisRef = g_glowMarkScratchUAxisRefVec;
				*uAxisRef = *faceVertex;

				axisAngle[0] = g_collideCurrentRotScaleData->rotationAxis.x * g_collideQ15ToUnitScale;
				axisAngle[1] = g_collideCurrentRotScaleData->rotationAxis.y * g_collideQ15ToUnitScale;
				rotationAxisZ = g_collideCurrentRotScaleData->rotationAxis.z * g_collideQ15ToUnitScale;
				axisAngle[3] = g_collideCurrentMeshRotationAngle;
				axisAngle[2] = rotationAxisZ;
				Math3D_BuildAxisAngleMatrix((Matrix3x3*)rotationMatrix, axisAngle);
				Math3D_RotateVec3(g_glowMarkScratchNormalVec, (Matrix3x3*)rotationMatrix);
				Math3D_RotateVec3(g_glowMarkScratchUAxisRefVec, (Matrix3x3*)rotationMatrix);
				g_glowMarkPlaneScratch.center.x =
					(g_collideSweepLocalEnd.x - g_collideSweepLocalStart.x) * hitT +
					g_collideSweepLocalStart.x;
				g_glowMarkPlaneScratch.center.y =
					(g_collideSweepLocalEnd.y - g_collideSweepLocalStart.y) * hitT +
					g_collideSweepLocalStart.y;
				g_glowMarkPlaneScratch.center.z =
					(g_collideSweepLocalEnd.z - g_collideSweepLocalStart.z) * hitT +
					g_collideSweepLocalStart.z;
			} else {
				g_glowMarkPlaneScratch.center = point;
				normal = g_glowMarkScratchNormalVec;
				*normal = *faceNormal;
				faceVertex = (const Vec3f*)&vertexBase[3 * faceIndices[0]];
				uAxisRef = g_glowMarkScratchUAxisRefVec;
				*uAxisRef = *faceVertex;
			}
		}
	}
}

/* Verbatim body of collide_TestSweepAgainstOptNodeLoose's FaceData loop. */
void collide_TestSweepFaceLoose(const int* faceIndices, const float* faceNormal, const float* vertexBase) {
	float hitT;

	if (collide_IntersectSegmentWithFacePlane(faceNormal, &vertexBase[3 * faceIndices[0]],
											  &g_collideSweepWalkerStart.x, &g_collideSweepWalkerEnd.x,
											  &hitT) &&
		(!g_collideSweepRejectNearStartHits || hitT >= g_collideSweepHitBackoff) &&
		hitT < g_collideSweepHitFraction) {
		float hitPoint[3];
		float absX;
		float absY;
		float absZ;
		int axisU;
		int axisV;
		float pointProjected[3];
		int vertex0Base;
		int vertex1Base;
		int vertex2Base;
		float vertex0U;
		float vertex0V;
		float vertex1U;
		float vertex1V;
		float vertex2U;
		float vertex2V;
		int firstEdgeNegative;
		char inside;

		hitPoint[0] =
			(g_collideSweepWalkerEnd.x - g_collideSweepWalkerStart.x) * hitT + g_collideSweepWalkerStart.x;
		absX = faceNormal[0];
		hitPoint[1] =
			(g_collideSweepWalkerEnd.y - g_collideSweepWalkerStart.y) * hitT + g_collideSweepWalkerStart.y;
		pointProjected[0] = hitPoint[0];
		hitPoint[2] =
			(g_collideSweepWalkerEnd.z - g_collideSweepWalkerStart.z) * hitT + g_collideSweepWalkerStart.z;
		pointProjected[1] = hitPoint[1];
		pointProjected[2] = hitPoint[2];
		absY = faceNormal[1];
		absZ = faceNormal[2];

		if (absX < g_collideZeroFloat) {
			absX = -absX;
		}
		if (absY < g_collideZeroFloat) {
			absY = -absY;
		}
		if (absZ < g_collideZeroFloat) {
			absZ = -absZ;
		}
		if (absZ < absY || absZ < absX) {
			if (absY < absX || absY < absZ) {
				axisU = 1;
				axisV = 2;
			} else {
				axisU = 0;
				axisV = 2;
				pointProjected[1] = pointProjected[0];
			}
		} else {
			axisU = 0;
			axisV = 1;
			pointProjected[2] = pointProjected[1];
			pointProjected[1] = pointProjected[0];
		}

		vertex0Base = 3 * faceIndices[0];
		vertex1Base = 3 * faceIndices[1];
		vertex0U = vertexBase[vertex0Base + axisU];
		vertex0V = vertexBase[vertex0Base + axisV];
		vertex1U = vertexBase[vertex1Base + axisU];
		vertex1V = vertexBase[vertex1Base + axisV];
		g_collidePolygonEdgeCrossScratch = (pointProjected[1] - vertex0U) * (vertex1V - vertex0V) -
										   (pointProjected[2] - vertex0V) * (vertex1U - vertex0U);
		firstEdgeNegative = g_collidePolygonEdgeCrossScratch < g_collideZeroFloat;

		vertex2Base = 3 * faceIndices[2];
		vertex2U = vertexBase[vertex2Base + axisU];
		vertex2V = vertexBase[vertex2Base + axisV];
		g_collidePolygonEdgeCrossScratch = (pointProjected[1] - vertex1U) * (vertex2V - vertex1V) -
										   (pointProjected[2] - vertex1V) * (vertex2U - vertex1U);
		inside = g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat || firstEdgeNegative;
		if (g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat && firstEdgeNegative) {
			inside = 0;
		}
		if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat) {
			g_collidePolygonEdgeCrossScratch = -g_collidePolygonEdgeCrossScratch;
		}

		if (faceIndices[3] != -1) {
			int vertex3Base;
			float vertex3U;
			float vertex3V;

			vertex3Base = 3 * faceIndices[3];
			vertex3U = vertexBase[vertex3Base + axisU];
			vertex3V = vertexBase[vertex3Base + axisV];
			g_collidePolygonEdgeCrossScratch = (pointProjected[1] - vertex2U) * (vertex3V - vertex2V) -
											   (pointProjected[2] - vertex2V) * (vertex3U - vertex2U);
			if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat && !firstEdgeNegative) {
				inside = 0;
			}
			if (g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat && firstEdgeNegative) {
				inside = 0;
			}
			if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat) {
				g_collidePolygonEdgeCrossScratch = -g_collidePolygonEdgeCrossScratch;
			}
			vertex2U = vertex3U;
			vertex2V = vertex3V;
		}

		vertex0Base = 3 * faceIndices[0];
		vertex0U = vertexBase[vertex0Base + axisU];
		vertex0V = vertexBase[vertex0Base + axisV];
		g_collidePolygonEdgeCrossScratch = (pointProjected[1] - vertex2U) * (vertex0V - vertex2V) -
										   (pointProjected[2] - vertex2V) * (vertex0U - vertex2U);
		if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat && !firstEdgeNegative) {
			inside = 0;
		}
		if (g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat && firstEdgeNegative) {
			inside = 0;
		}
		if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat) {
			g_collidePolygonEdgeCrossScratch = -g_collidePolygonEdgeCrossScratch;
		}

		if (inside) {
			Vec3f* normal;
			Vec3f* uAxisRef;
			const float* firstFaceVertex;

			g_collideSweepHitFraction = hitT;
			g_collideSweepHitMeshOrdinal = g_collideSweepCurrentMeshOrdinal;
			g_glowMarkPlaneScratch.center.x = hitPoint[0];
			g_glowMarkPlaneScratch.center.y = hitPoint[1];
			g_glowMarkPlaneScratch.center.z = hitPoint[2];
			normal = g_glowMarkScratchNormalVec;
			normal->x = faceNormal[0];
			normal->y = faceNormal[1];
			normal->z = faceNormal[2];
			uAxisRef = g_glowMarkScratchUAxisRefVec;
			firstFaceVertex = &vertexBase[vertex0Base];
			uAxisRef->x = firstFaceVertex[0];
			uAxisRef->y = firstFaceVertex[1];
			uAxisRef->z = firstFaceVertex[2];
			g_glowMarkWorldSegmentMode = 0;
		} else if (g_collidePolygonEdgeCrossScratch < g_collideLooseSweepEdgeThreshold) {
			const float* looseHitVertex;
			const float* firstFaceVertex;

			g_collideSweepHitFraction = hitT;
			g_collideSweepHitMeshOrdinal = g_collideSweepCurrentMeshOrdinal;
			looseHitVertex = &vertexBase[vertex1Base];
			g_glowMarkPlaneScratch.center.x = looseHitVertex[0];
			g_glowMarkPlaneScratch.center.y = looseHitVertex[1];
			g_glowMarkPlaneScratch.center.z = looseHitVertex[2];
			g_glowMarkScratchNormalVec->x = faceNormal[0];
			g_glowMarkScratchNormalVec->y = faceNormal[1];
			g_glowMarkScratchNormalVec->z = faceNormal[2];
			firstFaceVertex = &vertexBase[vertex0Base];
			g_glowMarkScratchUAxisRefVec->x = firstFaceVertex[0];
			g_glowMarkScratchUAxisRefVec->y = firstFaceVertex[1];
			g_glowMarkScratchUAxisRefVec->z = firstFaceVertex[2];
			g_glowMarkWorldSegmentMode = 0;
		}
	}
}

static AeronFaceBvhScratch s_scratch;
static int s_scratchFailureLogged;

static const XwaOptSynthMesh* CollisionBvh_FindMesh(OptimizedPolyObject* model, OptNode* faceDataNode) {
	const XwaOptSynthMesh* mesh;

	mesh = OptSynth_FindMeshForNode(model, faceDataNode);
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
	/* A component without faces has no BVH and nothing to hit. */
	if (mesh == NULL || mesh->faceCount == 0) {
		return;
	}
	vertexBase = (const float*)g_collideCurrentMeshVertsNode->param2;
	/* The query uses the plane epsilon of the current sweep: 10.0 for the
	   swept test and 1e-4 for the local one. Both callers set it before they
	   walk the model, and the negative epsilon always has the same size. With
	   the larger value, the local sweep would get many faces that can't pass
	   collide_IntersectSegmentWithFacePlane. */
	count = AeronFaceBvh_Collect(mesh->bvh, &g_collideSweepWalkerStart.x, &g_collideSweepWalkerEnd.x,
								 g_collidePlaneEpsilonPos, &s_scratch);
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
	/* Authored order: a linear scan runs faster over it than over the leaf
	   order, and ties resolve as they would in a stock FaceData loop. */
	for (index = 0; index < mesh->faceCount; ++index) {
		const AeronOptFace* face = &mesh->faces[mesh->authoredOrder[index]];

		collide_TestSweepFaceLoose(face->vertex_index, face->normal, vertexBase);
	}
}
