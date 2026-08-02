#include "xwa/render/effects.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/string.h"

#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif

// GLOBAL: XWA 0x5A9A08
const float g_meshClipNearThreshold = 1.1f;
// GLOBAL: XWA 0x5A9A0C
const float g_meshClipScreenMin = 0.0f;

// GLOBAL: XWA 0x5A947C
const float g_sw3dLightZero = 0.0f;
// GLOBAL: XWA 0x5A9480
const float g_sw3dLightHalf = 0.5f;
// GLOBAL: XWA 0x5A9484
const float g_sw3dLightUnit = 1.0f;
// GLOBAL: XWA 0x5A9488
const float g_sw3dLightRoughDistanceScale = 0.29409999f;
// GLOBAL: XWA 0x5A948C
const float g_sw3dLightRoughSpecularMinorScale = 0.1936f;
// GLOBAL: XWA 0x5A9490
const float g_sw3dLightRoughSpecularMajorScale = -0.4632f;

static inline void RenderScene_AppendVisibleFace(SceneFace** outFace, SceneMesh* mesh, int faceIndex) {
	SceneFace* face;
	int phongSlotIndex;

	face = *outFace;
	face->faceId = g_faceIdCounter;
	face->faceIndex = faceIndex;
	face->pMesh = mesh;
	phongSlotIndex = g_phongSlotIndex;
	face->pPhongData = g_scenePhongData + sizeof(SoftwareLightSample) * g_phongSlotStride * phongSlotIndex;
	if (g_phongSlotIndex < 199) {
		++g_phongSlotIndex;
	}
	face->packed = faceIndex + (g_curLayerId << 16);
	face->pScanEdge = NULL;

	*outFace = face + 1;
	++g_visFaceCount;
}

// FUNCTION: XWA 0x47DE30
int RenderScene_CullMeshFacesFromView(SceneMesh* mesh) {
	Vec3f* faceNormal;
	FaceRecord* faceRecord;
	float* modelVerts;
	SceneFace* outFace;
	int faceIndex;
	int finalVisibleCount;

	mesh->faceBaseIndex = g_visFaceCount;
	g_meshEyePos.x = mesh->pos.x;
	g_meshEyePos.y = mesh->pos.y;
	g_meshEyePos.z = mesh->pos.z;
	if (g_bBackdropMeshMode) {
		g_meshEyePos.x = 0.0f;
		g_meshEyePos.y = 0.0f;
		g_meshEyePos.z = -100000.0f;
		Math3D_RotateVec3(&g_meshEyePos, (Matrix3x3*)mesh->orient);
		g_meshEyePos.x += mesh->pos.x;
		g_meshEyePos.y += mesh->pos.y;
		g_meshEyePos.z += mesh->pos.z;
	}

	faceNormal = mesh->pFaceNormals;
	faceRecord = mesh->pFaceGeom;
	modelVerts = &mesh->pModelVerts->x;
	outFace = &g_visFaceList[g_visFaceCount];
	faceIndex = 0;
	if (mesh->faceCount > 0) {
		do {
			float viewVec[3];
			int vertexIdx;

			vertexIdx = 3 * faceRecord[faceIndex].vertexIdx[0];
			viewVec[0] = g_meshEyePos.x - modelVerts[vertexIdx];
			viewVec[1] = g_meshEyePos.y - modelVerts[vertexIdx + 1];
			viewVec[2] = g_meshEyePos.z - modelVerts[vertexIdx + 2];

			if (Math3D_Dot3(viewVec, &faceNormal[faceIndex].x) >= g_sw3dZeroFloat) {
				RenderScene_AppendVisibleFace(&outFace, mesh, faceIndex);
			}

			++g_faceIdCounter;
			++faceIndex;
		} while (faceIndex < mesh->faceCount);
	}

	finalVisibleCount = g_visFaceCount;
	mesh->visFaceCount = finalVisibleCount - mesh->faceBaseIndex;
	return mesh->faceBaseIndex;
}

// FUNCTION: XWA 0x47E010
int RenderScene_CullMeshFaces(SceneMesh* mesh) {
	Vec3f* faceNormal;
	SceneFace* outFace;
	int faceIndex;
	int finalVisibleCount;

	mesh->faceBaseIndex = g_visFaceCount;
	g_meshEyePos.x = mesh->pos.x;
	g_meshEyePos.y = mesh->pos.y;
	g_meshEyePos.z = mesh->pos.z;
	if (g_bBackdropMeshMode) {
		g_meshEyePos.x = 0.0f;
		g_meshEyePos.y = 0.0f;
		g_meshEyePos.z = -100000.0f;
		Math3D_RotateVec3(&g_meshEyePos, (Matrix3x3*)mesh->orient);
		g_meshEyePos.x += mesh->pos.x;
		g_meshEyePos.y += mesh->pos.y;
		g_meshEyePos.z += mesh->pos.z;
	}

	outFace = &g_visFaceList[g_visFaceCount];
	faceIndex = 0;
	if (mesh->faceCount > 0) {
		faceNormal = mesh->pFaceNormals;
		do {
			if (Math3D_Dot3(g_fixedCullDir, &faceNormal->x) >= 0.0f) {
				RenderScene_AppendVisibleFace(&outFace, mesh, faceIndex);
			}

			++faceNormal;
			++faceIndex;
			++g_faceIdCounter;
		} while (faceIndex < mesh->faceCount);
	}

	finalVisibleCount = g_visFaceCount;
	mesh->visFaceCount = finalVisibleCount - mesh->faceBaseIndex;
	return mesh->faceBaseIndex;
}

// FUNCTION: XWA 0x47E190
int RenderScene_AppendMeshFacesNoCull(SceneMesh* mesh) {
	SceneFace* outFace;
	int faceIndex;
	int finalVisibleCount;

	mesh->faceBaseIndex = g_visFaceCount;
	g_meshEyePos.x = mesh->pos.x;
	g_meshEyePos.y = mesh->pos.y;
	g_meshEyePos.z = mesh->pos.z;
	if (g_bBackdropMeshMode) {
		g_meshEyePos.x = 0.0f;
		g_meshEyePos.y = 0.0f;
		g_meshEyePos.z = -100000.0f;
		Math3D_RotateVec3(&g_meshEyePos, (Matrix3x3*)mesh->orient);
		g_meshEyePos.x += mesh->pos.x;
		g_meshEyePos.y += mesh->pos.y;
		g_meshEyePos.z += mesh->pos.z;
	}

	outFace = &g_visFaceList[g_visFaceCount];
	faceIndex = 0;
	while (faceIndex < mesh->faceCount) {
		outFace->faceIndex = faceIndex;
		outFace->pMesh = mesh;
		outFace->pPhongData =
			g_scenePhongData + sizeof(SoftwareLightSample) * g_phongSlotStride * g_phongSlotIndex;
		if (g_phongSlotIndex < 199) {
			++g_phongSlotIndex;
		}
		outFace->packed = faceIndex + (g_curLayerId << 16);
		outFace->pScanEdge = NULL;
		++outFace;
		++g_visFaceCount;
		++faceIndex;
	}

	finalVisibleCount = g_visFaceCount;
	mesh->visFaceCount = finalVisibleCount - mesh->faceBaseIndex;
	return mesh->faceBaseIndex;
}

// FUNCTION: XWA 0x4E8110
void GlowMark_AppendObjectMeshTextureLayers(SceneMesh* mesh, ObjectMeshTextureLayerBlock* layerBlocks) {
	ObjectMeshTextureLayerBlock* block;

	for (block = layerBlocks; block != NULL; block = block->prevActive) {
		int layerIndex;

		layerIndex = 0;
		if (block->facePatchCount > 0) {
			MeshExtraTextureLayer* layer;
			Vec3f** meshVertexArrays;

			layer = block->facePatches;
			meshVertexArrays = block->meshVertexArrays;
			do {
				if (*meshVertexArrays == mesh->pModelVerts) {
					int textureLayerCount;

					textureLayerCount = mesh->textureLayerCount;
					if (textureLayerCount < 15) {
						mesh->layers[textureLayerCount] = layer;
						++mesh->textureLayerCount;
					}
				}

				++layerIndex;
				++meshVertexArrays;
				++layer;
			} while (layerIndex < block->facePatchCount);
		}
	}
}

// FUNCTION: XWA 0x4E93F0
void GlowMark_ClearPendingRequests(void) {
	int i;

	if (!g_useHardware3D) {
		return;
	}

	for (i = 0; i < g_glowMarkRequestCount; ++i) {
		g_objRenderState[g_glowMarkRequestPool[i].objectIndex].pendingGlowMarks = NULL;
	}

	g_glowMarkRequestCount = 0;
}

// FUNCTION: XWA 0x4E9440
GlowMarkRequest* GlowMark_QueueRequest(uint16_t objectIndex, int16_t effectParam, int16_t modelType,
									   float scaleU, float scaleV) {
	GlowMarkRequest* request;
	GlowMarkRequest* oldHead;

	if (!g_hitEffectsEnabled || effectParam == OBJ_DSLaserInternal ||
		g_objectTable[objectIndex].objectType == OBJ_DSFocusLens) {
		return NULL;
	}

	request = NULL;
	if (g_glowMarkRequestCount < 64) {
		request = &g_glowMarkRequestPool[g_glowMarkRequestCount];
		request->next = NULL;
		++g_glowMarkRequestCount;
	}

	if (request != NULL) {
		if (g_glowMarkWorldSegmentMode != 0) {
			request->worldSegmentMode = 1;
			request->geom.segment.startWorldX = g_glowMarkSegmentStartWorld[0];
			request->geom.segment.startWorldY = g_glowMarkSegmentStartWorld[1];
			request->geom.segment.startWorldZ = g_glowMarkSegmentStartWorld[2];
			request->geom.segment.endWorldX = g_glowMarkSegmentEndWorld[0];
			request->geom.segment.endWorldY = g_glowMarkSegmentEndWorld[1];
			request->geom.segment.endWorldZ = g_glowMarkSegmentEndWorld[2];
		} else {
			request->worldSegmentMode = 0;
			request->deriveUvAxesFromReference = 1;
			memcpy(&request->geom.plane, &g_glowMarkPlaneScratch, sizeof(g_glowMarkPlaneScratch));
		}

		request->objectIndex = objectIndex;
		request->scaleU = scaleU;
		request->scaleV = scaleV;
		request->modelType = (uint16_t)modelType;
		request->effectParam = (uint16_t)effectParam;
		request->persistentUntilCleared = 0;

		oldHead = g_objRenderState[objectIndex].pendingGlowMarks;
		g_objRenderState[objectIndex].pendingGlowMarks = request;
		g_objRenderState[objectIndex].pendingGlowMarks->next = oldHead;
	}

	return request;
}

// FUNCTION: XWA 0x4E8680
void GlowMark_ProcessPendingRequests(uint16_t objectIndex) {
	ObjectRenderState* renderStates;
	ObjectRecord* obj;
	GlowMarkRequest* request;
	ObjectMeshTextureLayerBlock* glowMarkTail;
	ObjectMeshTextureLayerBlock* previousTail;
	MemoryHandle modelHandle;
	MemoryHandle lockedModelHandle;

	renderStates = g_objRenderState;
	obj = &g_objectTable[objectIndex];
	request = renderStates[objectIndex].pendingGlowMarks;
	glowMarkTail = renderStates[objectIndex].glowMarkTail;
	previousTail = glowMarkTail;
	modelHandle = 0;
	lockedModelHandle = 0;

	while (request != NULL) {
		ObjectMeshTextureLayerBlock* patch;
		float patchRadius;
		float patchMinX;
		float patchMaxX;
		float patchMinY;
		float patchMaxY;
		float patchMinZ;
		float patchMaxZ;

		previousTail = glowMarkTail;
		if (request->modelType == 552u) {
			patch = GlowMark_AllocShieldPatch(request->effectParam, 0);
		} else {
			patch = GlowMark_AllocAnimatedPatch(request->effectParam, request->modelType);
		}
		glowMarkTail = patch;
		if (patch == NULL) {
			break;
		}

		patch->objectIndex = objectIndex;
		patch->persistentUntilCleared = request->persistentUntilCleared;
		patch->prevActive = previousTail;
		if (previousTail != NULL) {
			previousTail->nextActive = patch;
		}

		if (g_glowMarkWorldSegmentMode != 0) {
			float maxBoundsExtent;
			int localStartX;
			int localStartY;
			int localStartZ;
			int localEndX;
			int localEndY;
			int localEndZ;

			if (request->scaleU == -1.0f) {
				maxBoundsExtent = (float)g_modelTypeTable[(uint16_t)obj->objectType].maxBoundsExtent;
				g_glowMarkInvScaleU = 2.0f / maxBoundsExtent;
				g_glowMarkInvScaleV = g_glowMarkInvScaleU;
			} else {
				maxBoundsExtent = request->scaleU <= request->scaleV ? request->scaleV : request->scaleU;
				g_glowMarkInvScaleU = 1.0f / request->scaleU;
				g_glowMarkInvScaleV = 1.0f / request->scaleV;
			}

			request->geom.segment.startWorldX -= obj->world_x;
			request->geom.segment.startWorldY -= obj->world_y;
			request->geom.segment.startWorldZ -= obj->world_z;
			request->geom.segment.endWorldX -= obj->world_x;
			request->geom.segment.endWorldY -= obj->world_y;
			request->geom.segment.endWorldZ -= obj->world_z;

			if (obj->mobj != NULL) {
				MobileObject* mobj;

				mobj = obj->mobj;
				if (mobj->orientMatrixDirty) {
					FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
					FVIEW_calcrotateorient(obj->roll, obj->angleD, obj);
				}
				localStartX =
					Xwa_Dot3Q15Inline(mobj->cachedSideX, mobj->cachedSideY, mobj->cachedSideZ,
									  request->geom.segment.startWorldX, request->geom.segment.startWorldY,
									  request->geom.segment.startWorldZ);
				localStartY = -Xwa_Dot3Q15Inline(
					mobj->cachedFwdX, mobj->cachedFwdY, mobj->cachedFwdZ, request->geom.segment.startWorldX,
					request->geom.segment.startWorldY, request->geom.segment.startWorldZ);
				localStartZ = Xwa_Dot3Q15Inline(
					mobj->cachedUpX, mobj->cachedUpY, mobj->cachedUpZ, request->geom.segment.startWorldX,
					request->geom.segment.startWorldY, request->geom.segment.startWorldZ);
				localEndX = Xwa_Dot3Q15Inline(
					mobj->cachedSideX, mobj->cachedSideY, mobj->cachedSideZ, request->geom.segment.endWorldX,
					request->geom.segment.endWorldY, request->geom.segment.endWorldZ);
				localEndY = -Xwa_Dot3Q15Inline(
					mobj->cachedFwdX, mobj->cachedFwdY, mobj->cachedFwdZ, request->geom.segment.endWorldX,
					request->geom.segment.endWorldY, request->geom.segment.endWorldZ);
				localEndZ = Xwa_Dot3Q15Inline(
					mobj->cachedUpX, mobj->cachedUpY, mobj->cachedUpZ, request->geom.segment.endWorldX,
					request->geom.segment.endWorldY, request->geom.segment.endWorldZ);
			} else {
				FVIEW_calcrotatemove(obj->pitch, obj->yaw, NULL);
				FVIEW_calcrotateorient(obj->roll, obj->angleD, NULL);
				localStartX = Xwa_Dot3Q15Inline(
					g_fviewSideX_Q15, g_fviewSideY_Q15, g_fviewSideZ_Q15, request->geom.segment.startWorldX,
					request->geom.segment.startWorldY, request->geom.segment.startWorldZ);
				localStartY = -Xwa_Dot3Q15Inline(
					g_fviewFwdX_Q15, g_fviewFwdY_Q15, g_fviewFwdZ_Q15, request->geom.segment.startWorldX,
					request->geom.segment.startWorldY, request->geom.segment.startWorldZ);
				localStartZ = Xwa_Dot3Q15Inline(
					g_fviewUpX_Q15, g_fviewUpY_Q15, g_fviewUpZ_Q15, request->geom.segment.startWorldX,
					request->geom.segment.startWorldY, request->geom.segment.startWorldZ);
				localEndX = Xwa_Dot3Q15Inline(
					g_fviewSideX_Q15, g_fviewSideY_Q15, g_fviewSideZ_Q15, request->geom.segment.endWorldX,
					request->geom.segment.endWorldY, request->geom.segment.endWorldZ);
				localEndY = -Xwa_Dot3Q15Inline(
					g_fviewFwdX_Q15, g_fviewFwdY_Q15, g_fviewFwdZ_Q15, request->geom.segment.endWorldX,
					request->geom.segment.endWorldY, request->geom.segment.endWorldZ);
				localEndZ = Xwa_Dot3Q15Inline(
					g_fviewUpX_Q15, g_fviewUpY_Q15, g_fviewUpZ_Q15, request->geom.segment.endWorldX,
					request->geom.segment.endWorldY, request->geom.segment.endWorldZ);
			}

			patch->center.x = (float)(localStartX >> 1);
			patch->center.y = (float)(localStartY >> 1);
			patch->center.z = (float)(localStartZ >> 1);
			trig2_ctop(localStartX - localEndX, localStartY - localEndY, localStartZ - localEndZ);
			FVIEW_calcrotatemove((int16_t)targetPitch, (int16_t)trig2_xyangle, NULL);

			patch->normal.x = (float)-g_curMatR2_X * 0.000030518499f;
			patch->normal.y = (float)-g_curMatR2_Y * 0.000030518499f;
			patch->normal.z = (float)-g_curMatR2_Z * 0.000030518499f;
			patch->planeD = -(patch->normal.x * patch->center.x + patch->normal.y * patch->center.y +
							  patch->normal.z * patch->center.z);
			patch->uAxis.x = (float)g_curMatR0_X * 0.000030518499f;
			patch->uAxis.y = (float)g_curMatR0_Y * 0.000030518499f;
			patch->uAxis.z = (float)g_curMatR0_Z * 0.000030518499f;
			patch->vAxis.x = (float)-g_curMatR1_X * 0.000030518499f;
			patch->vAxis.y = (float)-g_curMatR1_Y * 0.000030518499f;
			patch->vAxis.z = (float)-g_curMatR1_Z * 0.000030518499f;
			patchRadius = (float)((int)trig2_polardistance >> 1) + maxBoundsExtent;
		} else {
			if (request->scaleU == -1.0f) {
				patchRadius = 500.0f;
				g_glowMarkInvScaleU = 0.0020000001f;
				g_glowMarkInvScaleV = 0.0020000001f;
			} else {
				patchRadius = request->scaleU <= request->scaleV ? request->scaleV : request->scaleU;
				g_glowMarkInvScaleU = 1.0f / request->scaleU;
				g_glowMarkInvScaleV = 1.0f / request->scaleV;
			}

			if (request->deriveUvAxesFromReference != 0) {
				float dx;
				float dy;
				float dz;
				float invLen;
				float uAxisX;
				float uAxisY;
				float uAxisZ;

				patch->normal.x = request->geom.plane.normal.x;
				patch->normal.y = request->geom.plane.normal.y;
				patch->normal.z = request->geom.plane.normal.z;
				dx = g_glowMarkPlaneScratch.center.x - request->geom.plane.uAxis.x;
				dy = g_glowMarkPlaneScratch.center.y - request->geom.plane.uAxis.y;
				dz = g_glowMarkPlaneScratch.center.z - request->geom.plane.uAxis.z;
				patch->center.x = request->geom.plane.center.x;
				patch->center.y = request->geom.plane.center.y;
				invLen = 1.0f / (float)sqrt(dx * dx + dy * dy + dz * dz);
				patch->center.z = request->geom.plane.center.z;
				patch->planeD = -(patch->normal.x * patch->center.x + patch->normal.y * patch->center.y +
								  patch->normal.z * patch->center.z);
				uAxisX = dx * invLen;
				uAxisY = dy * invLen;
				uAxisZ = dz * invLen;
				patch->uAxis.x = uAxisX;
				patch->uAxis.y = uAxisY;
				patch->uAxis.z = uAxisZ;
				patch->vAxis.x = uAxisY * patch->normal.z - uAxisZ * patch->normal.y;
				patch->vAxis.y = uAxisZ * patch->normal.x - uAxisX * patch->normal.z;
				patch->vAxis.z = uAxisX * patch->normal.y - uAxisY * patch->normal.x;
			} else {
				patch->normal.x = request->geom.plane.normal.x;
				patch->normal.y = request->geom.plane.normal.y;
				patch->normal.z = request->geom.plane.normal.z;
				patch->center.x = request->geom.plane.center.x;
				patch->center.y = request->geom.plane.center.y;
				patch->center.z = request->geom.plane.center.z;
				patch->planeD = -(patch->normal.x * patch->center.x + patch->normal.y * patch->center.y +
								  patch->normal.z * patch->center.z);
				patch->uAxis.x = request->geom.plane.uAxis.x;
				patch->uAxis.y = request->geom.plane.uAxis.y;
				patch->uAxis.z = request->geom.plane.uAxis.z;
				patch->vAxis.x = request->geom.plane.vAxis.x;
				patch->vAxis.y = request->geom.plane.vAxis.y;
				patch->vAxis.z = request->geom.plane.vAxis.z;
			}
		}

#ifdef XWA_MODERN
		XwaSnapshot_NoteGlowMarkProjector(patch, g_glowMarkInvScaleU, g_glowMarkInvScaleV,
										  g_glowMarkWorldSegmentMode);
#endif
		patch->facePatchCount = 0;
		patchMinX = patch->center.x - patchRadius;
		patchMaxX = patchRadius + patch->center.x;
		patchMinY = patch->center.y - patchRadius;
		patchMaxY = patchRadius + patch->center.y;
		patchMinZ = patch->center.z - patchRadius;
		patchMaxZ = patchRadius + patch->center.z;

		modelHandle = g_loadedModels.byObjectType[(uint16_t)obj->objectType];
		if (modelHandle == 0) {
			return;
		}

		{
			OptimizedPolyObject* model;
			int meshOrdinal;
			int rootIndex;

			model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
			OptModel_AdjustOptimizedPolyObjectPointers(model);

			meshOrdinal = 0;
			g_glowMarkMeshOrdinal = 0;
			for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
				OptNode* node;
				OptNodeType nodeType;
				MeshDescriptor* meshDesc;

				g_glowMarkMeshRotationAngle = 0.0f;
				node = model->rootNodes[rootIndex];
				nodeType = node->nodeType;
				if (nodeType != OPT_TEXTURE && nodeType != OPT_TEXTURE_REF) {
					MobileObject* mobj;

					meshOrdinal = meshOrdinal + 1;
					g_glowMarkMeshOrdinal = meshOrdinal;
					mobj = obj->mobj;
					if (mobj != NULL && mobj->pCraft != NULL &&
						mobj->pCraft->componentHp[meshOrdinal - 1] != 0) {
						g_glowMarkMeshRotationAngle =
							(float)mobj->pCraft->meshRotation[meshOrdinal - 1] * 0.024543673f;
					}

					meshDesc =
						ModelMesh_GetObjectTypeMeshDescriptor((uint16_t)obj->objectType, meshOrdinal - 1);
					if (meshDesc != NULL) {
						int boxLimit;

						boxLimit = (int)meshDesc->boxMin.x;
						if (patchMinX >= (float)boxLimit || patchMaxX >= (float)boxLimit) {
							boxLimit = (int)meshDesc->boxMax.x;
							if (patchMinX <= (float)boxLimit || patchMaxX <= (float)boxLimit) {
								boxLimit = (int)meshDesc->boxMin.y;
								if (patchMinY >= (float)boxLimit || patchMaxY >= (float)boxLimit) {
									boxLimit = (int)meshDesc->boxMax.y;
									if (patchMinY <= (float)boxLimit || patchMaxY <= (float)boxLimit) {
										boxLimit = (int)meshDesc->boxMin.z;
										if (patchMinZ >= (float)boxLimit || patchMaxZ >= (float)boxLimit) {
											boxLimit = (int)meshDesc->boxMax.z;
											if (patchMinZ <= (float)boxLimit ||
												patchMaxZ <= (float)boxLimit) {
												g_glowMarkTraversalActive = 1;
#ifdef XWA_MODERN
												XwaSnapshot_NoteGlowMarkMesh(
													glowMarkTail, (unsigned int)(g_glowMarkMeshOrdinal - 1));
#endif
												GlowMark_CollectModelFaces(glowMarkTail, model, node);
												DebugPrintf((const char*)(uintptr_t)(uint16_t)obj->objectType,
															g_glowMarkMeshOrdinal - 1);
											}
										}
									}
								}
							}
						}
					}
				}
				meshOrdinal = g_glowMarkMeshOrdinal;
			}
		}

		request = request->next;
	}

	renderStates = g_objRenderState;
	lockedModelHandle = modelHandle;
	if (glowMarkTail != NULL) {
		renderStates[objectIndex].glowMarkTail = glowMarkTail;
	} else if (previousTail != NULL) {
		renderStates[objectIndex].glowMarkTail = previousTail;
	}
	renderStates[objectIndex].pendingGlowMarks = NULL;
	if (lockedModelHandle != 0) {
		Memory_UnlockHandle(lockedModelHandle);
	}
}

// FUNCTION: XWA 0x448000
void RenderScene_DrawMesh(SceneMesh* mesh) {
	int savedVisFaceCount;
	SceneMesh* queuedMesh;

	g_projVertCount = 0;
	g_sceneEdgeCursor = 0;
	savedVisFaceCount = g_visFaceCount;
	if (g_meshQueueIndex != g_meshQueueMax && g_visFaceCount + mesh->faceCount <= g_sceneFaceMax &&
		mesh->vertexCount <= g_projVertMax && mesh->edgeCount <= g_sceneEdgeMax) {
		memcpy(&g_meshQueue[g_meshQueueIndex], mesh, sizeof(*mesh));
		queuedMesh = &g_meshQueue[g_meshQueueIndex];
		RenderScene_CullMeshFacesFromView(queuedMesh);
		if (queuedMesh->visFaceCount != 0) {
			queuedMesh->textureLayerCount = 0;
			if (!g_flightRenderToFrontend
#ifdef XWA_MODERN
				&& g_objRenderState != NULL && g_objectTable != NULL && queuedMesh->pObject != NULL
#endif
			) {
				ObjectMeshTextureLayerBlock* layerBlocks;

				layerBlocks = g_objRenderState[(uint16_t)(queuedMesh->pObject - g_objectTable)].glowMarkTail;
				if (layerBlocks != NULL) {
					GlowMark_AppendObjectMeshTextureLayers(queuedMesh, layerBlocks);
				}
			}

			if ((g_d3dVertexCount && g_d3dVertexCount + 4 * queuedMesh->visFaceCount > g_maxBatchVerts) ||
				queuedMesh->visFaceCount + g_d3dIndexCount > g_maxBatchTris) {
				std3D_LockExecuteBuffer();
				std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
				std3D_BeginInstructions();
				std3D_AddTriangles(g_triBuffer, g_d3dIndexCount);
				std3D_ExecuteBuffer();
				g_d3dIndexCount = 0;
				g_d3dVertexCount = 0;
			}

			if (queuedMesh->alphaFlag) {
				int vertexCount;

#ifdef XWA_MODERN
				RenderScene_EnsureMeshBatches();
#endif
				vertexCount = g_meshDeferredBatch->vertexCount;
				if (vertexCount != 0) {
					int visFaceCount;

					visFaceCount = queuedMesh->visFaceCount;
					if (vertexCount + 4 * visFaceCount > g_maxBatchVerts ||
						visFaceCount + g_meshDeferredBatch->triCount > g_maxBatchTris) {
						RenderBatch* batch;

						batch = RenderBatch_Alloc();
						batch->next = g_meshDeferredBatch;
						g_meshDeferredBatch = batch;
					}
				}
			}

			if (g_bBackdropMeshMode) {
				sw3d_ProjectMeshVerticesDistant(queuedMesh);
			} else {
				sw3d_ProjectMeshVertices(queuedMesh);
			}
			sw3d_RasterizeMeshFaces(queuedMesh);
			g_visFaceCount = savedVisFaceCount;
		}
	}
}

// FUNCTION: XWA 0x44FD10
int RenderScene_DrawNodeMeshFaces(SceneMesh* mesh) {
	int result;
	int savedVisFaceCount;
	SceneMesh* queuedMesh;

	result = g_meshQueueIndex;
	g_projVertCount = 0;
	g_sceneEdgeCursor = 0;
	savedVisFaceCount = g_visFaceCount;

	if (g_meshQueueIndex != g_meshQueueMax && g_visFaceCount + mesh->faceCount <= g_sceneFaceMax &&
		mesh->vertexCount <= g_projVertMax && mesh->edgeCount <= g_sceneEdgeMax) {
		memcpy(&g_meshQueue[g_meshQueueIndex], mesh, sizeof(*mesh));
		queuedMesh = &g_meshQueue[g_meshQueueIndex];
		RenderScene_CullMeshFaces(queuedMesh);
		result = queuedMesh->visFaceCount;
		if (result != 0) {
			queuedMesh->textureLayerCount = 0;
			if (g_d3dVertexCount + 8 * result > g_maxBatchVerts ||
				g_d3dIndexCount + 2 * result > g_maxBatchTris) {
				std3D_LockExecuteBuffer();
				std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
				std3D_BeginInstructions();
				std3D_AddTriangles(g_triBuffer, g_d3dIndexCount);
				std3D_ExecuteBuffer();
				g_d3dIndexCount = 0;
				g_d3dVertexCount = 0;
			}

			RenderScene_TransformProjectVertices(queuedMesh);
			result = RenderScene_ClipAndEmitFace(queuedMesh);
			g_visFaceCount = savedVisFaceCount;
		}
	}

	return result;
}

// FUNCTION: XWA 0x480370
void RenderScene_DrawSceneMesh(SceneMesh* mesh) {
	SceneMesh* queuedMesh;

	if (g_useHardware3D) {
		RenderScene_DrawMesh(mesh);
	} else {
		g_projVertCount = 0;
		g_sceneEdgeCursor = 0;
		if (g_meshQueueIndex != g_meshQueueMax && g_visFaceCount + mesh->faceCount <= g_sceneFaceMax &&
			mesh->vertexCount <= g_projVertMax && mesh->edgeCount <= g_sceneEdgeMax) {
			memcpy(&g_meshQueue[g_meshQueueIndex], mesh, sizeof(*mesh));
			queuedMesh = &g_meshQueue[g_meshQueueIndex];
			RenderScene_CullMeshFacesFromView(queuedMesh);
			if (queuedMesh->visFaceCount != 0) {
				if (g_bBackdropMeshMode) {
					sw3d_ProjectMeshVerticesDistant(queuedMesh);
				} else {
					sw3d_ProjectMeshVertices(queuedMesh);
				}
				sw3d_RasterizeMeshFaces(queuedMesh);
				++g_meshQueueIndex;
			}
		}
	}
}

// FUNCTION: XWA 0x4807B0
MeshClipFlags RenderScene_ComputeMeshDescriptorClipFlags(SceneMesh* mesh) {
	MeshDescriptor* descriptor;
	unsigned int cornerIdx;
	uint8_t sharedOutsideFlags;

	descriptor = mesh->pMeshDescriptor;
	cornerIdx = 0;

	g_meshClipBoxCornersView[0].x = descriptor->boxMax.x;
	g_meshClipBoxCornersView[0].y = descriptor->boxMax.y;
	g_meshClipBoxCornersView[0].z = descriptor->boxMax.z;
	g_meshClipBoxCornersView[1].x = descriptor->boxMin.x;
	g_meshClipBoxCornersView[1].y = descriptor->boxMax.y;
	g_meshClipBoxCornersView[1].z = descriptor->boxMax.z;
	g_meshClipBoxCornersView[2].x = descriptor->boxMax.x;
	g_meshClipBoxCornersView[2].y = descriptor->boxMin.y;
	g_meshClipBoxCornersView[2].z = descriptor->boxMax.z;
	g_meshClipBoxCornersView[3].x = descriptor->boxMin.x;
	g_meshClipBoxCornersView[3].y = descriptor->boxMin.y;
	g_meshClipBoxCornersView[3].z = descriptor->boxMax.z;
	g_meshClipBoxCornersView[4].x = descriptor->boxMax.x;
	g_meshClipBoxCornersView[4].y = descriptor->boxMax.y;
	g_meshClipBoxCornersView[4].z = descriptor->boxMin.z;
	g_meshClipBoxCornersView[5].x = descriptor->boxMin.x;
	g_meshClipBoxCornersView[5].y = descriptor->boxMax.y;
	g_meshClipBoxCornersView[5].z = descriptor->boxMin.z;
	g_meshClipBoxCornersView[6].x = descriptor->boxMax.x;
	g_meshClipBoxCornersView[6].y = descriptor->boxMin.y;
	g_meshClipBoxCornersView[6].z = descriptor->boxMin.z;
	g_meshClipBoxCornersView[7].x = descriptor->boxMin.x;
	g_meshClipBoxCornersView[7].y = descriptor->boxMin.y;
	g_meshClipBoxCornersView[7].z = descriptor->boxMin.z;

	do {
		float invZ;
		float screenX;
		float screenY;
		float translatedY;

		Math3D_RotateVec3(&g_meshClipBoxCornersView[cornerIdx], (Matrix3x3*)mesh->viewOrient);
		g_meshClipBoxCornersView[cornerIdx].x += mesh->viewPos.x;
		translatedY = mesh->viewPos.y;
		translatedY += g_meshClipBoxCornersView[cornerIdx].y;
		g_meshClipBoxCornersView[cornerIdx].y = translatedY;
		g_meshClipBoxCornersView[cornerIdx].z += mesh->viewPos.z;

		g_meshClipCornerOutcodes[cornerIdx] = 0;
		if (g_meshClipBoxCornersView[cornerIdx].z < g_meshClipNearThreshold) {
			g_meshClipCornerOutcodes[cornerIdx] = MESH_CLIP_NEAR;
			if (g_meshClipBoxCornersView[cornerIdx].z < g_sw3dUnitFloat) {
				g_meshClipBoxCornersView[cornerIdx].z = 1.0f;
			}
		}

		invZ = g_projScale / g_meshClipBoxCornersView[cornerIdx].z;
		screenX = invZ * g_meshClipBoxCornersView[cornerIdx].x + g_flightVpCenterXf;
		g_meshClipInvZ[cornerIdx] = invZ;
		g_meshClipScreenX[cornerIdx] = screenX;
		if (screenX < g_meshClipScreenMin) {
			g_meshClipCornerOutcodes[cornerIdx] |= MESH_CLIP_LEFT;
		} else if (screenX > (float)(g_flightVpWidth - 3)) {
			g_meshClipCornerOutcodes[cornerIdx] |= MESH_CLIP_RIGHT;
		}

		screenY = invZ * g_meshClipBoxCornersView[cornerIdx].y + g_projOffsetYf;
		screenY += g_flightVpCenterYf;
		g_meshClipScreenY[cornerIdx] = screenY;
		if (screenY < g_meshClipScreenMin) {
			g_meshClipCornerOutcodes[cornerIdx] |= MESH_CLIP_TOP;
		} else if (screenY > (float)(g_flightVpHeight - 3)) {
			g_meshClipCornerOutcodes[cornerIdx] |= MESH_CLIP_BOTTOM;
		}

		++cornerIdx;
	} while (cornerIdx < 8);

	sharedOutsideFlags = g_meshClipCornerOutcodes[3] & g_meshClipCornerOutcodes[2] &
						 g_meshClipCornerOutcodes[1] & g_meshClipCornerOutcodes[0] &
						 g_meshClipCornerOutcodes[7] & g_meshClipCornerOutcodes[6] &
						 g_meshClipCornerOutcodes[5] & g_meshClipCornerOutcodes[4];
	if (sharedOutsideFlags != 0) {
		return 0;
	}

	g_meshClipCornerOutcodes[0] |= g_meshClipCornerOutcodes[1] | g_meshClipCornerOutcodes[2] |
								   g_meshClipCornerOutcodes[3] | g_meshClipCornerOutcodes[4] |
								   g_meshClipCornerOutcodes[5] | g_meshClipCornerOutcodes[6] |
								   g_meshClipCornerOutcodes[7];
	if (g_meshClipCornerOutcodes[0] == 0) {
		return MESH_CLIP_FULLY_INSIDE;
	}
	return (MeshClipFlags)g_meshClipCornerOutcodes[0];
}

void RenderScene_SetMeshGapInt(SceneMesh* mesh, int offset, intptr_t value) {
	mesh->nodeFlags[offset / (int)sizeof(mesh->nodeFlags[0])] = (int)value;
}

void RenderScene_ApplyRotationScale(SceneMesh* mesh, Vec3f* pivot, Vec3f* axis, float angle) {
	Matrix3x3 rotMatrix;
	float axisAngle[4];

	mesh->pos.x -= pivot->x;
	mesh->pos.y -= pivot->y;
	mesh->pos.z -= pivot->z;
	mesh->viewPos.x += Math3D_RotateVec3X(pivot, (Matrix3x3*)mesh->viewOrient);
	mesh->viewPos.y += Math3D_RotateVec3Y(pivot, (Matrix3x3*)mesh->viewOrient);
	mesh->viewPos.z += Math3D_RotateVec3Z(pivot, (Matrix3x3*)mesh->viewOrient);

	axisAngle[0] = axis->x * 0.000030517578f;
	axisAngle[1] = axis->y * 0.000030517578f;
	axisAngle[2] = axis->z * 0.000030517578f;
	axisAngle[3] = angle;
	Math3D_BuildAxisAngleMatrix(&rotMatrix, axisAngle);
	Math3D_MulMatrix3x3((Matrix3x3*)mesh->orient, &rotMatrix);
	Math3D_RotateVec3(&mesh->pos, &rotMatrix);
	Math3D_MulMatrix3x3T((Matrix3x3*)mesh->viewOrient, &rotMatrix);

	mesh->pos.x += pivot->x;
	mesh->pos.y += pivot->y;
	mesh->pos.z += pivot->z;
	mesh->viewPos.x -= Math3D_RotateVec3X(pivot, (Matrix3x3*)mesh->viewOrient);
	mesh->viewPos.y -= Math3D_RotateVec3Y(pivot, (Matrix3x3*)mesh->viewOrient);
	mesh->viewPos.z -= Math3D_RotateVec3Z(pivot, (Matrix3x3*)mesh->viewOrient);
}

// FUNCTION: XWA 0x482000
void RenderScene_DrawModelNode(OptimizedPolyObject* model, OptNode* node, SceneMesh* mesh) {
	OptimizedPolyObject* curModel;
	OptNode* curNode;
	void* nodeData;
	int lodChildSelection;
	int nodeSwitchSelection;
	float axisAngle[4];
	float rotMatrixStorage[16];

	curModel = model;
	curNode = node;
	while (curNode != NULL && curNode->nodeType == OPT_NODEREF) {
		OptNode* resolvedNode;

		resolvedNode = (OptNode*)curNode->param1;
		if (resolvedNode == NULL) {
			const char* refName;
			int rootIndex;

			refName = (const char*)curNode->param2;
			for (rootIndex = 0; rootIndex < curModel->rootNodeCount; ++rootIndex) {
				OptNode* rootNode;

				rootNode = curModel->rootNodes[rootIndex];
				if (rootNode != NULL) {
					if (rootNode->pName != NULL && Xwa_CrtStricmp(rootNode->pName, refName) == 0) {
						resolvedNode = rootNode;
					} else {
						int childIndex;

						for (childIndex = 0; childIndex < rootNode->childCount; ++childIndex) {
							resolvedNode = OptModel_FindNodeByName(rootNode->pChildren[childIndex], refName);
							if (resolvedNode != NULL) {
								break;
							}
						}
					}
				}
				if (resolvedNode != NULL) {
					break;
				}
			}
			curNode->param1 = (intptr_t)resolvedNode;
		}
		curNode = resolvedNode;
	}

	if (curNode == NULL) {
		return;
	}
	nodeData = curNode->param2;
	lodChildSelection = 0;
	nodeSwitchSelection = 0;

	if (nodeData != NULL) {
		switch (curNode->nodeType) {
			case OPT_MESHDESC:
				mesh->pMeshDescriptor = (MeshDescriptor*)nodeData;
				break;
			case OPT_ROTSCALE: {
				Vec3f* pivot;
				Vec3f* axis;

				pivot = (Vec3f*)nodeData;
				axis = pivot + 1;
				if (g_cockpitViewActive && g_players[g_localPlayer].currentSeatIdx) {
					int rotaryType;

					rotaryType = g_curMeshType - MESH_RotaryGunTurret;
					if (rotaryType != 0) {
						--rotaryType;
						if (rotaryType != 0) {
							rotaryType -= MESH_RotaryBeamSystem - MESH_RotaryLauncher;
							if (rotaryType == 0) {
								g_curRotScale = pivot;
								g_curRotAngle = mesh->rotAngle;
							}
						} else {
							if (g_curRotAngle != 0.0f) {
								mesh->pos.x -= g_curRotScale->x;
								mesh->pos.y -= g_curRotScale->y;
								mesh->pos.z -= g_curRotScale->z;
								mesh->viewPos.x +=
									Math3D_RotateVec3X(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.y +=
									Math3D_RotateVec3Y(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.z +=
									Math3D_RotateVec3Z(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								axisAngle[0] = g_curRotScale[1].x * 0.000030517578f;
								axisAngle[1] = g_curRotScale[1].y * 0.000030517578f;
								axisAngle[2] = g_curRotScale[1].z * 0.000030517578f;
								axisAngle[3] = g_curRotAngle * 0.5f;
								Math3D_BuildAxisAngleMatrix((Matrix3x3*)rotMatrixStorage, axisAngle);
								Math3D_MulMatrix3x3((Matrix3x3*)mesh->orient, (Matrix3x3*)rotMatrixStorage);
								Math3D_RotateVec3(&mesh->pos, (Matrix3x3*)rotMatrixStorage);
								Math3D_MulMatrix3x3T((Matrix3x3*)mesh->viewOrient,
													 (Matrix3x3*)rotMatrixStorage);
								mesh->pos.x += g_curRotScale->x;
								mesh->pos.y += g_curRotScale->y;
								mesh->pos.z += g_curRotScale->z;
								mesh->viewPos.x -=
									Math3D_RotateVec3X(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.y -=
									Math3D_RotateVec3Y(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.z -=
									Math3D_RotateVec3Z(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								g_unusedCockpitRotScaleType22Data = pivot;
							}
						}
					} else {
						if (g_curRotAngle != 0.0f) {
							mesh->pos.x -= g_curRotScale->x;
							mesh->pos.y -= g_curRotScale->y;
							mesh->pos.z -= g_curRotScale->z;
							mesh->viewPos.x +=
								Math3D_RotateVec3X(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
							mesh->viewPos.y +=
								Math3D_RotateVec3Y(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
							mesh->viewPos.z +=
								Math3D_RotateVec3Z(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
							axisAngle[0] = g_curRotScale[1].x * 0.000030517578f;
							axisAngle[1] = g_curRotScale[1].y * 0.000030517578f;
							axisAngle[2] = g_curRotScale[1].z * 0.000030517578f;
							axisAngle[3] = g_curRotAngle;
							Math3D_BuildAxisAngleMatrix((Matrix3x3*)rotMatrixStorage, axisAngle);
							Math3D_MulMatrix3x3((Matrix3x3*)mesh->orient, (Matrix3x3*)rotMatrixStorage);
							Math3D_RotateVec3(&mesh->pos, (Matrix3x3*)rotMatrixStorage);
							Math3D_MulMatrix3x3T((Matrix3x3*)mesh->viewOrient, (Matrix3x3*)rotMatrixStorage);
							mesh->pos.x += g_curRotScale->x;
							mesh->pos.y += g_curRotScale->y;
							mesh->pos.z += g_curRotScale->z;
							mesh->viewPos.x -=
								Math3D_RotateVec3X(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
							mesh->viewPos.y -=
								Math3D_RotateVec3Y(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
							mesh->viewPos.z -=
								Math3D_RotateVec3Z(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
							g_unusedCockpitRotScaleType21Data = pivot;
						}
					}
				}
				if (mesh->rotAngle != 0.0f) {
					mesh->pos.x -= pivot->x;
					mesh->pos.y -= pivot->y;
					mesh->pos.z -= pivot->z;
					mesh->viewPos.x += Math3D_RotateVec3X(pivot, (Matrix3x3*)mesh->viewOrient);
					mesh->viewPos.y += Math3D_RotateVec3Y(pivot, (Matrix3x3*)mesh->viewOrient);
					mesh->viewPos.z += Math3D_RotateVec3Z(pivot, (Matrix3x3*)mesh->viewOrient);
					axisAngle[0] = axis->x * 0.000030517578f;
					axisAngle[1] = axis->y * 0.000030517578f;
					axisAngle[2] = axis->z * 0.000030517578f;
					axisAngle[3] = mesh->rotAngle;
					Math3D_BuildAxisAngleMatrix((Matrix3x3*)rotMatrixStorage, axisAngle);
					Math3D_MulMatrix3x3((Matrix3x3*)mesh->orient, (Matrix3x3*)rotMatrixStorage);
					Math3D_RotateVec3(&mesh->pos, (Matrix3x3*)rotMatrixStorage);
					Math3D_MulMatrix3x3T((Matrix3x3*)mesh->viewOrient, (Matrix3x3*)rotMatrixStorage);
					mesh->pos.x += pivot->x;
					mesh->pos.y += pivot->y;
					mesh->pos.z += pivot->z;
					mesh->viewPos.x -= Math3D_RotateVec3X(pivot, (Matrix3x3*)mesh->viewOrient);
					mesh->viewPos.y -= Math3D_RotateVec3Y(pivot, (Matrix3x3*)mesh->viewOrient);
					mesh->viewPos.z -= Math3D_RotateVec3Z(pivot, (Matrix3x3*)mesh->viewOrient);
				} else if (g_cockpitViewActive && g_players[g_localPlayer].currentSeatIdx &&
						   g_curMeshType == MESH_RotaryBeamSystem) {
					g_curRotScale = pivot;
					g_curRotAngle = mesh->rotAngle;
				}
				break;
			}
			case OPT_FACEGROUP:
				if (viewZ <= 0 || g_forcedLodLevel) {
					lodChildSelection = g_forcedLodLevel != 0 ? g_forcedLodLevel : 1;
					if (lodChildSelection > curNode->childCount) {
						lodChildSelection = -1;
					}
				} else {
					float lodThreshold;

					lodThreshold = 1.0f;
					if (g_lodDistanceScale > 0.0f) {
						lodThreshold = 1.0f / ((float)viewZ * g_lodDistanceScale);
					}
					lodChildSelection = 1;
					while (lodChildSelection <= curNode->childCount &&
						   ((float*)nodeData)[lodChildSelection - 1] > lodThreshold) {
						++lodChildSelection;
					}
					if (lodChildSelection > curNode->childCount) {
						lodChildSelection = -1;
					}
				}
				if (lodChildSelection == 1 && mesh->pMeshDescriptor != NULL) {
					MeshClipFlags clipFlags;

					clipFlags = (MeshClipFlags)g_renderFlags;
					if (g_renderFlags == 64 ||
						(g_cockpitViewActive && g_players[g_localPlayer].currentSeatIdx)) {
						clipFlags = MESH_CLIP_RIGHT | MESH_CLIP_LEFT | MESH_CLIP_BOTTOM | MESH_CLIP_TOP |
									MESH_CLIP_NEAR;
					} else if (g_renderFlags != MESH_CLIP_FULLY_INSIDE) {
						clipFlags = RenderScene_ComputeMeshDescriptorClipFlags(mesh);
					}
					g_meshRenderFlags = clipFlags;
					if (!clipFlags) {
						lodChildSelection = -1;
					}
				} else {
					g_meshRenderFlags = g_renderFlags;
				}
				break;
			case OPT_TYPE_19:
				memcpy(mesh->nodeFlags, nodeData, 12);
				break;
			case OPT_TYPE_2: {
				Vec3f* offset;
				Matrix3x3* transform;

				offset = (Vec3f*)nodeData;
				transform = (Matrix3x3*)((uint8_t*)nodeData + sizeof(Vec3f));
				Math3D_MulMatrix3x3((Matrix3x3*)mesh->viewOrient, transform);
				Math3D_RotateVec3(&mesh->viewPos, transform);
				mesh->viewPos.x += offset->x;
				mesh->viewPos.y += offset->y;
				mesh->viewPos.z += offset->z;
				Math3D_MulMatrix3x3T((Matrix3x3*)mesh->orient, transform);
				mesh->pos.x -= Math3D_RotateVec3X(offset, (Matrix3x3*)mesh->orient);
				mesh->pos.y -= Math3D_RotateVec3Y(offset, (Matrix3x3*)mesh->orient);
				mesh->pos.z -= Math3D_RotateVec3Z(offset, (Matrix3x3*)mesh->orient);
				break;
			}
			case OPT_TYPE_4: {
				Vec3f* offset;

				offset = (Vec3f*)nodeData;
				mesh->viewPos.x += offset->x;
				mesh->viewPos.y += offset->y;
				mesh->viewPos.z += offset->z;
				mesh->pos.x -= Math3D_RotateVec3X(offset, (Matrix3x3*)mesh->orient);
				mesh->pos.y -= Math3D_RotateVec3Y(offset, (Matrix3x3*)mesh->orient);
				mesh->pos.z -= Math3D_RotateVec3Z(offset, (Matrix3x3*)mesh->orient);
				break;
			}
			case OPT_TYPE_5:
				Math3D_MulMatrix3x3((Matrix3x3*)mesh->viewOrient, (Matrix3x3*)nodeData);
				Math3D_RotateVec3(&mesh->viewPos, (Matrix3x3*)nodeData);
				Math3D_MulMatrix3x3T((Matrix3x3*)mesh->orient, (Matrix3x3*)nodeData);
				break;
			case OPT_TYPE_6: {
				Vec3f* scale;
				float invX;
				float invY;
				float invZ;

				scale = (Vec3f*)nodeData;
				mesh->viewOrient[0] *= scale->x;
				mesh->viewOrient[1] *= scale->y;
				mesh->viewOrient[2] *= scale->z;
				mesh->viewOrient[3] *= scale->x;
				mesh->viewOrient[4] *= scale->y;
				mesh->viewOrient[5] *= scale->z;
				mesh->viewOrient[6] *= scale->x;
				mesh->viewOrient[7] *= scale->y;
				mesh->viewOrient[8] *= scale->z;
				mesh->viewPos.x *= scale->x;
				mesh->viewPos.y *= scale->y;
				mesh->viewPos.z *= scale->z;

				invX = 1.0f / scale->x;
				invY = 1.0f / scale->y;
				invZ = 1.0f / scale->z;
				mesh->orient[0] *= invX;
				mesh->orient[1] *= invX;
				mesh->orient[2] *= invX;
				mesh->orient[3] *= invY;
				mesh->orient[4] *= invY;
				mesh->orient[5] *= invY;
				mesh->orient[6] *= invZ;
				mesh->orient[7] *= invZ;
				mesh->orient[8] *= invZ;
				break;
			}
			case OPT_MESHVERTS:
				mesh->vertexCount = (int)curNode->param1;
				mesh->pModelVerts = (Vec3f*)nodeData;
				g_faceIdCounter = 0;
				break;
			case OPT_VERTNORMALS:
				g_curVertNormals = (intptr_t)curNode->param2;
				mesh->pVertNormals = (Vec3f*)nodeData;
				break;
			case OPT_TEXCOORDS:
				mesh->pUVs = (OptTexCoord*)nodeData;
				break;
			case OPT_TYPE_10:
				if (curNode->param1 == 8 || curNode->param1 == 7) {
					mesh->field_136 = (int)g_curMeshFlags;
				} else if (curNode->param1 == 6 || curNode->param1 == 5) {
					mesh->field_156 = (int)g_curMeshFlags;
				} else {
					mesh->nodeFlags[3] = (int)g_curMeshFlags;
				}
				break;
			case OPT_FACEDATA:
			case OPT_FACEDATA_15:
			case OPT_FACEDATA_16:
			case OPT_FACEDATA_17: {
				FaceRecord* faceRecords;
				Vec3f* faceNormals;
				FaceTextureGradients* faceTexturing;
				Vec3f* inlineVertNormals;

				mesh->faceCount = (int)curNode->param1;
				memcpy(&mesh->edgeCount, nodeData, sizeof(mesh->edgeCount));
				faceRecords = (FaceRecord*)((uint8_t*)nodeData + 4);
				mesh->pFaceGeom = faceRecords;
				faceRecords += curNode->param1;
				faceNormals = (Vec3f*)faceRecords;
				mesh->pFaceNormals = faceNormals;
				faceTexturing = (FaceTextureGradients*)&faceNormals[curNode->param1];
				mesh->pFaceTexturing = faceTexturing;
				inlineVertNormals = &faceTexturing[curNode->param1].gradient0;

				if (mesh->pMaterial == NULL) {
					mesh->pMaterial = (OptTextureData*)g_curTextureDesc;
					mesh->pTexels = (uint8_t*)g_curTextureDesc + sizeof(OptTextureData);
#ifdef XWA_MODERN
					mesh->pPalette = g_curTexturePalette;
#else
					mesh->pPalette = (void*)(uintptr_t)((OptTextureData*)g_curTextureDesc)->paletteAddress;
#endif
					mesh->alphaFlag = g_curMeshFlags;
				}
				if (mesh->pVertNormals == NULL) {
					mesh->pVertNormals = inlineVertNormals;
					RenderScene_DrawSceneMesh(mesh);
					mesh->pVertNormals = NULL;
				} else {
					RenderScene_DrawSceneMesh(mesh);
				}
				break;
			}
			case OPT_TEXTURE: {
				if (g_bindMeshTextures && (uint16_t)g_curTextureId != (uint16_t)-1 &&
					g_modelTextureOverrideSlots[(uint16_t)g_curTextureId].textureNode != NULL) {
					mesh->field_164 =
						(intptr_t)g_modelTextureOverrideSlots[(uint16_t)g_curTextureId].textureNode->pName;
					mesh->pMaterial = (OptTextureData*)g_modelTextureOverrideSlots[(uint16_t)g_curTextureId]
										  .textureNode->param2;
					g_curTextureDesc = mesh->pMaterial;
					mesh->pTexels = (uint8_t*)mesh->pMaterial + sizeof(OptTextureData);
#ifdef XWA_MODERN
					g_curTexturePalette = OptModel_ResolveTexturePalette(
						g_modelTextureOverrideSlots[(uint16_t)g_curTextureId].textureNode);
					mesh->pPalette = g_curTexturePalette;
#else
					mesh->pPalette = (void*)(uintptr_t)((OptTextureData*)g_curTextureDesc)->paletteAddress;
#endif
					mesh->alphaFlag = 0;
					g_curMeshFlags = 0;
				} else {
					mesh->field_164 = (intptr_t)curNode->pName;
					mesh->pMaterial = (OptTextureData*)curNode->param2;
					g_curTextureDesc = mesh->pMaterial;
					mesh->pTexels = (uint8_t*)mesh->pMaterial + sizeof(OptTextureData);
#ifdef XWA_MODERN
					g_curTexturePalette = OptModel_ResolveTexturePalette(curNode);
					mesh->pPalette = g_curTexturePalette;
#else
					mesh->pPalette = (void*)(uintptr_t)((OptTextureData*)g_curTextureDesc)->paletteAddress;
#endif
					if (curNode->childCount != 0) {
						OptNode* textureChild;

						textureChild = curNode->pChildren[0];
						mesh->alphaFlag = (intptr_t)textureChild->param2;
						g_curMeshFlags = (intptr_t)textureChild->param2;
					} else {
						mesh->alphaFlag = 0;
						g_curMeshFlags = 0;
					}
				}
				break;
			}
			case OPT_TEXTURE_REF: {
				intptr_t alphaFlag;
				D3DInfoNode* textureInfo;

				if (g_bindMeshTextures && (uint16_t)g_curTextureId != (uint16_t)-1 &&
					g_modelTextureOverrideSlots[(uint16_t)g_curTextureId].textureNode != NULL) {
					textureInfo = (D3DInfoNode*)g_modelTextureOverrideSlots[(uint16_t)g_curTextureId]
									  .textureNode->param2;
					mesh->pTexture = textureInfo;
					alphaFlag = textureInfo->hasAlphaData;
				} else {
					textureInfo = (D3DInfoNode*)nodeData;
					mesh->pTexture = textureInfo;
					alphaFlag = textureInfo->hasAlphaData;
				}
				mesh->alphaFlag = alphaFlag;
				mesh->pMaterial = &mesh->pTexture->textureData;
				break;
			}
			case OPT_NODESWITCH:
				nodeSwitchSelection = g_nodeSwitchIndex + 1;
				if (nodeSwitchSelection > curNode->childCount) {
					nodeSwitchSelection = curNode->childCount;
				}
				break;
			case OPT_NODEREF:
			case OPT_TYPE_8:
			case OPT_TYPE_9:
			case OPT_TYPE_12:
			case OPT_TYPE_14:
			case OPT_TYPE_18:
			case OPT_HARDPOINT:
			case OPT_TEXALPHA:
			default:
				break;
		}
	} else {
		switch (curNode->nodeType) {
			case OPT_TYPE_10:
				if (curNode->param1 == 8 || curNode->param1 == 7) {
					mesh->field_136 = (int)g_curMeshFlags;
				} else if (curNode->param1 == 6 || curNode->param1 == 5) {
					mesh->field_156 = (int)g_curMeshFlags;
				} else {
					mesh->nodeFlags[3] = (int)g_curMeshFlags;
				}
				break;
			case OPT_TEXTURE: {
				mesh->field_164 = (intptr_t)curNode->pName;
				mesh->pMaterial = (OptTextureData*)curNode->param2;
				g_curTextureDesc = (OptTextureData*)curNode->param2;
				mesh->pTexels = (uint8_t*)mesh->pMaterial + sizeof(OptTextureData);
#ifdef XWA_MODERN
				g_curTexturePalette = OptModel_ResolveTexturePalette(curNode);
				mesh->pPalette = g_curTexturePalette;
#else
				mesh->pPalette = (void*)(uintptr_t)((OptTextureData*)g_curTextureDesc)->paletteAddress;
#endif
				if (curNode->childCount != 0) {
					OptNode* textureChild;

					textureChild = curNode->pChildren[0];
					g_curMeshFlags = (intptr_t)textureChild->param2;
					mesh->alphaFlag = (intptr_t)textureChild->param2;
				} else {
					mesh->alphaFlag = 0;
					g_curMeshFlags = 0;
				}
				break;
			}
			case OPT_NODESWITCH:
				nodeSwitchSelection = g_nodeSwitchIndex + 1;
				if (nodeSwitchSelection > curNode->childCount) {
					nodeSwitchSelection = curNode->childCount;
				}
				break;
			default:
				break;
		}
	}

	if (curNode->childCount == 0) {
		return;
	}

	if (nodeSwitchSelection) {
		++g_curLayerId;
		RenderScene_DrawModelNode(curModel, curNode->pChildren[nodeSwitchSelection - 1], mesh);
	} else if (lodChildSelection) {
		if (lodChildSelection != -1) {
			++g_curLayerId;
			RenderScene_DrawModelNode(curModel, curNode->pChildren[lodChildSelection - 1], mesh);
		}
	} else {
		SceneMesh childMesh;
		int childIndex;

		childMesh = *mesh;
		g_modelNodeWalkUnusedScratch0 = 0;
		g_modelNodeWalkUnusedScratch1 = 0;
		g_curVertNormals = 0;
		g_modelNodeWalkUnusedScratch2 = 0;
		g_curMeshFlags = 0;
		g_curVertexCount = 0;

		for (childIndex = 0; childIndex < curNode->childCount; ++childIndex) {
			++g_curLayerId;
			RenderScene_DrawModelNode(curModel, curNode->pChildren[childIndex], &childMesh);
		}
	}
}

// FUNCTION: XWA 0x483EB0
void RenderScene_DrawModelNodeHardware(OptimizedPolyObject* model, OptNode* node, SceneMesh* mesh) {
	OptNode* curNode;
	void* nodeData;
	int lodChildSelection;
	int nodeSwitchSelection;
	float axisAngle[4];
	float rotMatrixStorage[16];

	curNode = node;
	while (curNode != NULL && curNode->nodeType == OPT_NODEREF) {
		OptNode* resolvedNode;

		resolvedNode = (OptNode*)curNode->param1;
		if (resolvedNode == NULL) {
			const char* refName;
			int rootIndex;

			refName = (const char*)curNode->param2;
			rootIndex = 0;
			if (model->rootNodeCount > 0) {
				do {
					OptNode* rootNode;

					rootNode = model->rootNodes[rootIndex];
					if (rootNode == NULL) {
						resolvedNode = NULL;
					} else if (rootNode->pName != NULL && Xwa_CrtStricmp(rootNode->pName, refName) == 0) {
						resolvedNode = rootNode;
					} else {
						int childIndex;

						resolvedNode = NULL;
						for (childIndex = 0; childIndex < rootNode->childCount; ++childIndex) {
							resolvedNode = OptModel_FindNodeByName(rootNode->pChildren[childIndex], refName);
							if (resolvedNode != NULL) {
								break;
							}
						}
					}
					if (resolvedNode != NULL) {
						break;
					}
					++rootIndex;
				} while (rootIndex < model->rootNodeCount);
			}
			curNode->param1 = (intptr_t)resolvedNode;
		}
		curNode = resolvedNode;
	}
	if (curNode == NULL) {
		return;
	}
	nodeData = curNode->param2;
	lodChildSelection = 0;
	nodeSwitchSelection = 0;

	if (nodeData != NULL) {
		switch (curNode->nodeType) {
			case OPT_ROTSCALE: {
				Vec3f* pivot;
				Vec3f* axis;

				pivot = (Vec3f*)nodeData;
				axis = pivot + 1;
				if (g_cockpitViewActive && g_players[g_localPlayer].currentSeatIdx) {
					switch (g_curMeshType) {
						case MESH_RotaryGunTurret:
							if (g_curRotAngle != g_sw3dZeroFloat) {
								mesh->pos.x -= g_curRotScale->x;
								mesh->pos.y -= g_curRotScale->y;
								mesh->pos.z -= g_curRotScale->z;
								mesh->viewPos.x +=
									Math3D_RotateVec3X(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.y +=
									Math3D_RotateVec3Y(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.z +=
									Math3D_RotateVec3Z(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								axisAngle[0] = g_curRotScale[1].x * g_modelNodeAxisScale;
								axisAngle[1] = g_curRotScale[1].y * g_modelNodeAxisScale;
								axisAngle[2] = g_curRotScale[1].z * g_modelNodeAxisScale;
								axisAngle[3] = g_curRotAngle;
								Math3D_BuildAxisAngleMatrix((Matrix3x3*)rotMatrixStorage, axisAngle);
								Math3D_MulMatrix3x3((Matrix3x3*)mesh->orient, (Matrix3x3*)rotMatrixStorage);
								Math3D_RotateVec3(&mesh->pos, (Matrix3x3*)rotMatrixStorage);
								Math3D_MulMatrix3x3T((Matrix3x3*)mesh->viewOrient,
													 (Matrix3x3*)rotMatrixStorage);
								mesh->pos.x += g_curRotScale->x;
								mesh->pos.y += g_curRotScale->y;
								mesh->pos.z += g_curRotScale->z;
								mesh->viewPos.x -=
									Math3D_RotateVec3X(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.y -=
									Math3D_RotateVec3Y(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.z -=
									Math3D_RotateVec3Z(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								g_unusedCockpitRotScaleType21Data = pivot;
							}
							break;
						case MESH_RotaryLauncher:
							if (g_curRotAngle != g_sw3dZeroFloat) {
								mesh->pos.x -= g_curRotScale->x;
								mesh->pos.y -= g_curRotScale->y;
								mesh->pos.z -= g_curRotScale->z;
								mesh->viewPos.x +=
									Math3D_RotateVec3X(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.y +=
									Math3D_RotateVec3Y(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.z +=
									Math3D_RotateVec3Z(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								axisAngle[0] = g_curRotScale[1].x * g_modelNodeAxisScale;
								axisAngle[1] = g_curRotScale[1].y * g_modelNodeAxisScale;
								axisAngle[2] = g_curRotScale[1].z * g_modelNodeAxisScale;
								axisAngle[3] = g_curRotAngle * g_modelNodeHalfAngleScale;
								Math3D_BuildAxisAngleMatrix((Matrix3x3*)rotMatrixStorage, axisAngle);
								Math3D_MulMatrix3x3((Matrix3x3*)mesh->orient, (Matrix3x3*)rotMatrixStorage);
								Math3D_RotateVec3(&mesh->pos, (Matrix3x3*)rotMatrixStorage);
								Math3D_MulMatrix3x3T((Matrix3x3*)mesh->viewOrient,
													 (Matrix3x3*)rotMatrixStorage);
								mesh->pos.x += g_curRotScale->x;
								mesh->pos.y += g_curRotScale->y;
								mesh->pos.z += g_curRotScale->z;
								mesh->viewPos.x -=
									Math3D_RotateVec3X(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.y -=
									Math3D_RotateVec3Y(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								mesh->viewPos.z -=
									Math3D_RotateVec3Z(g_curRotScale, (Matrix3x3*)mesh->viewOrient);
								g_unusedCockpitRotScaleType22Data = pivot;
							}
							break;
						case MESH_RotaryBeamSystem:
							g_curRotScale = pivot;
							g_curRotAngle = mesh->rotAngle;
							break;
						default:
							break;
					}
				}
				if (mesh->rotAngle != g_sw3dZeroFloat) {
					mesh->pos.x -= pivot->x;
					mesh->pos.y -= pivot->y;
					mesh->pos.z -= pivot->z;
					mesh->viewPos.x += Math3D_RotateVec3X(pivot, (Matrix3x3*)mesh->viewOrient);
					mesh->viewPos.y += Math3D_RotateVec3Y(pivot, (Matrix3x3*)mesh->viewOrient);
					mesh->viewPos.z += Math3D_RotateVec3Z(pivot, (Matrix3x3*)mesh->viewOrient);
					axisAngle[0] = axis->x * g_modelNodeAxisScale;
					axisAngle[1] = axis->y * g_modelNodeAxisScale;
					axisAngle[2] = axis->z * g_modelNodeAxisScale;
					axisAngle[3] = mesh->rotAngle;
					Math3D_BuildAxisAngleMatrix((Matrix3x3*)rotMatrixStorage, axisAngle);
					Math3D_MulMatrix3x3((Matrix3x3*)mesh->orient, (Matrix3x3*)rotMatrixStorage);
					Math3D_RotateVec3(&mesh->pos, (Matrix3x3*)rotMatrixStorage);
					Math3D_MulMatrix3x3T((Matrix3x3*)mesh->viewOrient, (Matrix3x3*)rotMatrixStorage);
					mesh->pos.x += pivot->x;
					mesh->pos.y += pivot->y;
					mesh->pos.z += pivot->z;
					mesh->viewPos.x -= Math3D_RotateVec3X(pivot, (Matrix3x3*)mesh->viewOrient);
					mesh->viewPos.y -= Math3D_RotateVec3Y(pivot, (Matrix3x3*)mesh->viewOrient);
					mesh->viewPos.z -= Math3D_RotateVec3Z(pivot, (Matrix3x3*)mesh->viewOrient);
				} else if (g_cockpitViewActive && g_players[g_localPlayer].currentSeatIdx &&
						   g_curMeshType == MESH_RotaryBeamSystem) {
					g_curRotScale = pivot;
					g_curRotAngle = mesh->rotAngle;
				}
				break;
			}
			case OPT_FACEGROUP:
				lodChildSelection = g_forcedLodLevel;
				if (viewZ > 0 && lodChildSelection == 0) {
					float lodThreshold;

					lodThreshold = 1.0f;
					if (g_lodDistanceScale > g_sw3dZeroFloat) {
						lodThreshold = g_sw3dUnitFloat / ((float)viewZ * g_lodDistanceScale);
					}
					lodChildSelection = 1;
					while (lodChildSelection <= curNode->childCount &&
						   ((float*)nodeData)[lodChildSelection - 1] > lodThreshold) {
						++lodChildSelection;
					}
					if (lodChildSelection > curNode->childCount) {
						lodChildSelection = -1;
					}
				} else if (lodChildSelection == 0) {
					lodChildSelection = 1;
				} else if (lodChildSelection > curNode->childCount) {
					lodChildSelection = -1;
				}
				break;
			case OPT_TYPE_19:
				mesh->nodeFlags[0] = ((int*)nodeData)[0];
				mesh->nodeFlags[1] = ((int*)nodeData)[1];
				mesh->nodeFlags[2] = ((int*)nodeData)[2];
				break;
			case OPT_TYPE_2: {
				Vec3f* offset;
				Matrix3x3* transform;

				offset = (Vec3f*)nodeData;
				transform = (Matrix3x3*)((uint8_t*)nodeData + sizeof(Vec3f));
				Math3D_MulMatrix3x3((Matrix3x3*)mesh->viewOrient, transform);
				Math3D_RotateVec3(&mesh->viewPos, transform);
				mesh->viewPos.x += offset->x;
				mesh->viewPos.y += offset->y;
				mesh->viewPos.z += offset->z;
				Math3D_MulMatrix3x3T((Matrix3x3*)mesh->orient, transform);
				mesh->pos.x -= Math3D_RotateVec3X(offset, (Matrix3x3*)mesh->orient);
				mesh->pos.y -= Math3D_RotateVec3Y(offset, (Matrix3x3*)mesh->orient);
				mesh->pos.z -= Math3D_RotateVec3Z(offset, (Matrix3x3*)mesh->orient);
				break;
			}
			case OPT_TYPE_4: {
				Vec3f* offset;

				offset = (Vec3f*)nodeData;
				mesh->viewPos.x += offset->x;
				mesh->viewPos.y += offset->y;
				mesh->viewPos.z += offset->z;
				mesh->pos.x -= Math3D_RotateVec3X(offset, (Matrix3x3*)mesh->orient);
				mesh->pos.y -= Math3D_RotateVec3Y(offset, (Matrix3x3*)mesh->orient);
				mesh->pos.z -= Math3D_RotateVec3Z(offset, (Matrix3x3*)mesh->orient);
				break;
			}
			case OPT_TYPE_5:
				Math3D_MulMatrix3x3((Matrix3x3*)mesh->viewOrient, (Matrix3x3*)nodeData);
				Math3D_RotateVec3(&mesh->viewPos, (Matrix3x3*)nodeData);
				Math3D_MulMatrix3x3T((Matrix3x3*)mesh->orient, (Matrix3x3*)nodeData);
				break;
			case OPT_TYPE_6: {
				Vec3f* scale;
				float invX;
				float invY;
				float invZ;

				scale = (Vec3f*)nodeData;
				mesh->viewOrient[0] *= scale->x;
				mesh->viewOrient[1] *= scale->y;
				mesh->viewOrient[2] *= scale->z;
				mesh->viewOrient[3] *= scale->x;
				mesh->viewOrient[4] *= scale->y;
				mesh->viewOrient[5] *= scale->z;
				mesh->viewOrient[6] *= scale->x;
				mesh->viewOrient[7] *= scale->y;
				mesh->viewOrient[8] *= scale->z;
				mesh->viewPos.x *= scale->x;
				mesh->viewPos.y *= scale->y;
				mesh->viewPos.z *= scale->z;

				invX = 1.0f / scale->x;
				mesh->orient[0] *= invX;
				mesh->orient[1] *= invX;
				mesh->orient[2] *= invX;
				invY = 1.0f / scale->y;
				mesh->orient[3] *= invY;
				mesh->orient[4] *= invY;
				mesh->orient[5] *= invY;
				invZ = 1.0f / scale->z;
				mesh->orient[6] *= invZ;
				mesh->orient[7] *= invZ;
				mesh->orient[8] *= invZ;
				break;
			}
			case OPT_MESHVERTS:
				mesh->vertexCount = (int)curNode->param1;
				mesh->pModelVerts = (Vec3f*)nodeData;
				g_faceIdCounter = 0;
				break;
			case OPT_VERTNORMALS:
				g_curVertNormals = (intptr_t)curNode->param2;
				mesh->pVertNormals = (Vec3f*)nodeData;
				break;
			case OPT_TEXCOORDS:
				mesh->pUVs = (OptTexCoord*)nodeData;
				break;
			case OPT_TYPE_10:
				if (curNode->param1 == 8 || curNode->param1 == 7) {
					mesh->field_136 = (int)g_curMeshFlags;
				} else if (curNode->param1 == 6 || curNode->param1 == 5) {
					mesh->field_156 = (int)g_curMeshFlags;
				} else {
					mesh->nodeFlags[3] = (int)g_curMeshFlags;
				}
				break;
			case OPT_FACEDATA:
			case OPT_FACEDATA_15:
			case OPT_FACEDATA_16:
			case OPT_FACEDATA_17: {
				uint8_t* faceData;
				Vec3f* inlineVertNormals;

				mesh->faceCount = (int)curNode->param1;
				memcpy(&mesh->edgeCount, nodeData, sizeof(mesh->edgeCount));
				faceData = (uint8_t*)nodeData + sizeof(mesh->edgeCount);
				mesh->pFaceGeom = (FaceRecord*)faceData;
				faceData += curNode->param1 * sizeof(FaceRecord);
				mesh->pFaceNormals = (Vec3f*)faceData;
				faceData += curNode->param1 * sizeof(Vec3f);
				mesh->pFaceTexturing = (FaceTextureGradients*)faceData;
				faceData += curNode->param1 * sizeof(FaceTextureGradients);
				inlineVertNormals = (Vec3f*)faceData;

				if (mesh->pVertNormals == NULL) {
					mesh->pVertNormals = inlineVertNormals;
					RenderScene_DrawNodeMeshFaces(mesh);
					mesh->pVertNormals = NULL;
				} else {
					RenderScene_DrawNodeMeshFaces(mesh);
				}
				break;
			}
			case OPT_TEXTURE_REF:
				mesh->pTexture = (D3DInfoNode*)nodeData;
				break;
			case OPT_NODESWITCH:
				nodeSwitchSelection = g_nodeSwitchIndex + 1;
				if (nodeSwitchSelection > curNode->childCount) {
					nodeSwitchSelection = curNode->childCount;
				}
				break;
			case OPT_NODEREF:
			case OPT_TYPE_8:
			case OPT_TYPE_9:
			case OPT_TYPE_12:
			case OPT_TYPE_14:
			case OPT_TYPE_18:
			case OPT_HARDPOINT:
			case OPT_TEXALPHA:
			default:
				break;
		}
	} else {
		switch (curNode->nodeType) {
			case OPT_TYPE_10:
				if (curNode->param1 == 8 || curNode->param1 == 7) {
					mesh->field_136 = (int)g_curMeshFlags;
				} else if (curNode->param1 == 6 || curNode->param1 == 5) {
					mesh->field_156 = (int)g_curMeshFlags;
				} else {
					mesh->nodeFlags[3] = (int)g_curMeshFlags;
				}
				break;
			case OPT_NODESWITCH:
				nodeSwitchSelection = g_nodeSwitchIndex + 1;
				if (nodeSwitchSelection > curNode->childCount) {
					nodeSwitchSelection = curNode->childCount;
				}
				break;
			default:
				break;
		}
	}

	if (curNode->childCount == 0) {
		return;
	}

	if (nodeSwitchSelection) {
		++g_curLayerId;
		RenderScene_DrawModelNodeHardware(model, curNode->pChildren[nodeSwitchSelection - 1], mesh);
	} else if (lodChildSelection) {
		if (lodChildSelection != -1) {
			++g_curLayerId;
			RenderScene_DrawModelNodeHardware(model, curNode->pChildren[lodChildSelection - 1], mesh);
		}
	} else {
		SceneMesh childMesh;
		int childIndex;

		childMesh = *mesh;
		g_modelNodeWalkUnusedScratch0 = 0;
		g_modelNodeWalkUnusedScratch1 = 0;
		g_curVertNormals = 0;
		g_modelNodeWalkUnusedScratch2 = 0;
		g_curMeshFlags = 0;
		g_curVertexCount = 0;

		for (childIndex = 0; childIndex < curNode->childCount; ++childIndex) {
			++g_curLayerId;
			RenderScene_DrawModelNodeHardware(model, curNode->pChildren[childIndex], &childMesh);
		}
	}
}

static __inline float RenderScene_RoughDistance3(float x, float y, float z, float zero, float minorScale) {
	float absX;
	float absY;
	float absZ;

	absX = x;
	absY = y;
	absZ = z;
	if (absX < zero) {
		absX = -absX;
	}
	if (absY < zero) {
		absY = -absY;
	}
	if (absZ < zero) {
		absZ = -absZ;
	}

	if (absX >= absY && absX >= absZ) {
		return (absZ + absY) * minorScale + absX;
	}
	if (absY >= absX && absY >= absZ) {
		return (absZ + absX) * minorScale + absY;
	}
	return (absY + absX) * minorScale + absZ;
}

// FUNCTION: XWA 0x442480
void RenderScene_ComputeVertexLighting(SceneMesh* mesh, ProjVertex* outVert, Vec3f* normal, Vec3f* pos,
									   Vec3f* eyePos) {
	float* red;
	float* green;
	float* blue;
	int directionalLightingEnabled;
	int genusId;
	int lightIdx;

	(void)eyePos;

	genusId = mesh->pObject->genusId;
	if (genusId == GENUS_NpcProjectile || genusId == GENUS_PlayerProjectile) {
		outVert->litColor[0] = 1.0f;
		outVert->litColor[1] = 1.0f;
		outVert->litColor[3] = 1.0f;
		outVert->litColor[2] = 1.0f;
		return;
	}

	directionalLightingEnabled = g_dirLightingEnabled;
	if (directionalLightingEnabled) {
		outVert->litColor[0] = 1.0f;
		red = &outVert->litColor[1];
		blue = &outVert->litColor[3];
		green = &outVert->litColor[2];
		*red = 0.0f;
		*blue = 0.0f;
		*green = 0.0f;

		for (lightIdx = 0; lightIdx < g_dirLightCount; ++lightIdx) {
			DirectionalLight* light;
			float dotXY;
			float dot;

			light = &g_directionalLights[lightIdx];
			dotXY = light->localDirY * normal->y + light->localDirX * normal->x;
			dot = light->localDirZ * normal->z + dotXY;
			if (dot > g_renderZeroFloat) {
				dot *= light->intensity;
				*red += light->colorR * dot;
				*blue += light->colorB * dot;
				*green += light->colorG * dot;
			}
		}
	} else {
		outVert->litColor[0] = 1.0f;
		red = &outVert->litColor[1];
		*red = 0.40000001f;
		outVert->litColor[3] = 0.40000001f;
		outVert->litColor[2] = 0.40000001f;
		blue = &outVert->litColor[3];
		green = &outVert->litColor[2];
	}

	for (lightIdx = 0; lightIdx < g_objectPointLightCount; ++lightIdx) {
		Vec3f delta;
		float distance;
		float invDistance;
		float dot;
		float scale;

		delta.x = g_objectPointLights[lightIdx].x - pos->x;
		delta.y = g_objectPointLights[lightIdx].y - pos->y;
		delta.z = g_objectPointLights[lightIdx].z - pos->z;
		dot = delta.y * normal->y + delta.x * normal->x;
		dot += delta.z * normal->z;

		distance = RenderScene_RoughDistance3(delta.x, delta.y, delta.z, g_renderZeroFloat,
											  g_renderRoughDistanceScale);
		invDistance = g_renderUnitFloat / distance;
		if (invDistance * dot >= g_renderPointLightFacingThreshold) {
			scale = invDistance * g_renderHalfFloat;
			if (scale > g_renderZeroFloat) {
				scale *= g_objectPointLights[lightIdx].intensity;
				*red += g_objectPointLights[lightIdx].colorR * scale;
				*green += g_objectPointLights[lightIdx].colorG * scale;
				*blue += g_objectPointLights[lightIdx].colorB * scale;
			}
		}
	}

	{
		float redExcess;
		float greenExcess;
		float blueExcess;
		float redBleed;
		float greenBleed;
		float blueBleed;
		float newRed;
		float newGreen;
		float newBlue;

		blueExcess = g_renderZeroFloat;
		greenExcess = 0.0f;
		redExcess = 0.0f;
		if (*red >= g_renderUnitFloat) {
			redExcess = *red - g_renderUnitFloat;
		}

		if (*green >= g_renderUnitFloat) {
			greenExcess = *green - g_renderUnitFloat;
		}

		if (*blue >= g_renderUnitFloat) {
			blueExcess = *blue - g_renderUnitFloat;
		}

		redBleed = (blueExcess + greenExcess) * g_renderOverbrightBleedScale;
		greenBleed = blueExcess + redExcess;
		blueBleed = greenExcess + redExcess;
		newGreen = greenBleed * g_renderOverbrightBleedScale + *green;
		newRed = redBleed + *red;
		newBlue = blueBleed * g_renderOverbrightBleedScale + *blue;

		*red = newRed;
		*green = newGreen;
		*blue = newBlue;

		if (newRed > g_renderUnitFloat) {
			*red = 1.0f;
		}
		if (newBlue > g_renderUnitFloat) {
			*blue = 1.0f;
		}
		if (newGreen > g_renderUnitFloat) {
			*green = 1.0f;
		}
	}
}

// FUNCTION: XWA 0x4421E0
int sw3d_ProjectMeshVerticesDistant(SceneMesh* mesh) {
	float projectScale;
	int vertexIdx;
	int faceIter;
	SceneFace* face;
	ProjVertex* outVert;
	ProjVertex* remappedVert;
	const int* cornerIndices;
	int cornerIdx;
	int uvIdx;
	int normalIdx;
	int remappedIdx;
	int vertBaseIndex;
	float w;
	Vec3f vec;

	projectScale = g_projScale / mesh->viewPos.z;
	face = &g_visFaceList[mesh->faceBaseIndex];
	vertBaseIndex = g_projVertCount;
	mesh->vertBaseIndex = vertBaseIndex;
	outVert = &g_projVertList[vertBaseIndex];
	mesh->projVertCursor = 0;
	projectScale *= g_renderDistantDepth;

	for (vertexIdx = 0; vertexIdx < mesh->vertexCount; ++vertexIdx) {
		g_vertexRemap[vertexIdx] = -1;
	}

	for (faceIter = 0; faceIter < mesh->visFaceCount; ++faceIter, ++face) {
		cornerIndices = mesh->pFaceGeom[face->faceIndex].vertexIdx;
		face->maxVertW = 0.0f;
		face->minVertW = g_projScale;

		for (cornerIdx = 0; cornerIdx < 4; ++cornerIdx) {
			vertexIdx = cornerIndices[0];
			uvIdx = cornerIndices[8];
			normalIdx = cornerIndices[12];
			++cornerIndices;
			if (vertexIdx == -1) {
				break;
			}

			remappedIdx = g_vertexRemap[vertexIdx];
			if (remappedIdx == -1) {
				g_vertexRemap[vertexIdx] = mesh->projVertCursor;

				++mesh->projVertCursor;
				vec = mesh->pModelVerts[vertexIdx];
				Math3D_RotateVec3(&vec, (Matrix3x3*)mesh->viewOrient);
				vec.x += mesh->viewPos.x;
				vec.y += mesh->viewPos.y;
				vec.z += mesh->viewPos.z;
				vec.z -= g_renderNegDistantDepth;

				outVert->w = projectScale / vec.z;
				outVert->sx = vec.x * outVert->w;
				w = outVert->w;
				outVert->sy = vec.y * outVert->w;
				outVert->sx += (float)(g_flightVpWidth >> 1);
				outVert->sy += (float)(g_projOffsetY + (g_flightVpHeight >> 1));
				RenderScene_ComputeVertexLighting(mesh, outVert, &mesh->pVertNormals[normalIdx],
												  &mesh->pModelVerts[vertexIdx], &g_meshEyePos);
				++outVert;
				outVert[-1].tu = mesh->pUVs[uvIdx].u;
				outVert[-1].tv = mesh->pUVs[uvIdx].v;
			} else {
				remappedVert = &g_projVertList[mesh->vertBaseIndex + remappedIdx];
				w = remappedVert->w;
			}

			if (w > face->maxVertW) {
				face->maxVertW = w;
			}
			if (w < face->minVertW) {
				face->minVertW = w;
			}
		}
	}

	return g_projVertCount += mesh->projVertCursor;
}

// FUNCTION: XWA 0x44FE50
void RenderScene_TransformProjectVertices(SceneMesh* mesh) {
	SceneFace* face;
	ProjVertex* outVert;
	int vertexIdx;
	int faceIter;
	int vertBaseIndex;
	Vec3f vec;
	Matrix3x3 mat;
	double viewMtx00;
	double viewMtx01;
	double viewMtx02;
	double viewMtx10;
	double viewMtx11;
	double viewMtx12;
	double viewMtx20;
	double viewMtx21;
	double viewMtx22;

	face = &g_visFaceList[mesh->faceBaseIndex];
	vertBaseIndex = g_projVertCount;
	mesh->vertBaseIndex = vertBaseIndex;
	outVert = &g_projVertList[vertBaseIndex];
	mesh->projVertCursor = 0;

	for (vertexIdx = 0; vertexIdx < mesh->vertexCount; ++vertexIdx) {
		g_vertexRemap[vertexIdx] = -1;
	}

	for (faceIter = 0; faceIter < mesh->visFaceCount; ++faceIter, ++face) {
		FaceRecord* faceRecord;
		const int* cornerIndices;
		int cornerIdx;
		float faceWTotal;

		faceRecord = &mesh->pFaceGeom[face->faceIndex];
		face->maxVertW = 0.0f;
		face->minVertW = g_projScale;
		faceWTotal = 0.0f;
		cornerIndices = faceRecord->vertexIdx;

		for (cornerIdx = 0; cornerIdx < 4; ++cornerIdx) {
			int remappedIdx;
			float faceW;

			vertexIdx = *cornerIndices;
			++cornerIndices;
			if (vertexIdx == -1) {
				break;
			}

			remappedIdx = g_vertexRemap[vertexIdx];
			if (remappedIdx == -1) {
				float savedTargetX;
				float savedTargetY;
				float clampedY;

				g_vertexRemap[vertexIdx] = mesh->projVertCursor;
				++mesh->projVertCursor;

				vec = mesh->pModelVerts[vertexIdx];
				Math3D_RotateVec3(&vec, (Matrix3x3*)mesh->viewOrient);
				vec.x += mesh->viewPos.x;
				vec.y += mesh->viewPos.y;
				vec.z += mesh->viewPos.z;

				mat.m[0] = (float)(viewMtx00 = g_viewMtx00);
				mat.m[1] = (float)(viewMtx01 = g_viewMtx01);
				mat.m[2] = (float)(viewMtx02 = g_viewMtx02);
				mat.m[3] = (float)(viewMtx10 = g_viewMtx10);
				mat.m[4] = (float)(viewMtx11 = g_viewMtx11);
				mat.m[5] = (float)(viewMtx12 = g_viewMtx12);
				mat.m[6] = (float)(viewMtx20 = g_viewMtx20);
				mat.m[7] = (float)(viewMtx21 = g_viewMtx21);
				mat.m[8] = (float)(viewMtx22 = g_viewMtx22);
				Math3D_RotateVec3(&vec, &mat);

				savedTargetX = (float)g_players[g_localPlayer].viewState.savedTargetX;
				savedTargetY = (float)g_players[g_localPlayer].viewState.savedTargetY;
				clampedY = vec.y + savedTargetY;
				vec.x += savedTargetX;

				if (g_renderProjectionYClampEnabled) {
					if (vec.x < g_renderProjectionClampX0) {
						if (clampedY > g_renderProjectionClampY0) {
							clampedY = g_renderProjectionClampY0;
						}
					} else {
						if (vec.x > g_renderProjectionClampX1) {
							if (clampedY > g_renderProjectionClampY1) {
								clampedY = g_renderProjectionClampY1;
							}
						} else {
							float clampY;

							clampY = (vec.x - g_renderProjectionClampX0) /
										 (g_renderProjectionClampX1 - g_renderProjectionClampX0) *
										 (g_renderProjectionClampY1 - g_renderProjectionClampY0) +
									 g_renderProjectionClampY0;
							if (clampedY > clampY) {
								clampedY = clampY;
							}
						}
					}
				}

				vec.z = g_renderProjectionDepthOverrideZ - g_renderProjectionDepthBias -
						(float)g_players[g_localPlayer].viewState.savedTargetZ;
				vec.x -= savedTargetX;
				vec.y = clampedY - savedTargetY;

				mat.m[0] = (float)(viewMtx00 = g_viewMtx00);
				mat.m[1] = (float)(viewMtx10 = g_viewMtx10);
				mat.m[2] = (float)(viewMtx20 = g_viewMtx20);
				mat.m[3] = (float)(viewMtx01 = g_viewMtx01);
				mat.m[4] = (float)(viewMtx11 = g_viewMtx11);
				mat.m[5] = (float)(viewMtx21 = g_viewMtx21);
				mat.m[6] = (float)(viewMtx02 = g_viewMtx02);
				mat.m[7] = (float)(viewMtx12 = g_viewMtx12);
				mat.m[8] = (float)(viewMtx22 = g_viewMtx22);
				Math3D_RotateVec3(&vec, &mat);

				if (vec.z < g_renderUnitFloat) {
					outVert->w = vec.z - g_renderUnitFloat;
					outVert->sx = vec.x;
					outVert->sy = vec.y;
					face->nearClipState = -1;
					faceW = g_projScale;
				} else {
					outVert->w = g_projScale / vec.z;
					outVert->sx = vec.x * outVert->w;
					faceW = outVert->w;
					outVert->sy = vec.y * outVert->w;
					outVert->sx += (float)(g_flightVpWidth >> 1);
					outVert->sy += (float)(g_projOffsetY + (g_flightVpHeight >> 1));
				}

				outVert->lightIntensity = 0.0f;
				outVert->litColor[0] = 0.0f;
				outVert->litColor[1] = 0.0f;
				outVert->litColor[3] = 0.0f;
				outVert->litColor[2] = 0.0f;
				outVert->tu = 0.0f;
				outVert->tv = 0.0f;
				outVert->extraLayerUVCount = 0;
				++outVert;
			} else {
				ProjVertex* remappedVert;

				remappedVert = &g_projVertList[mesh->vertBaseIndex + remappedIdx];
				if (remappedVert->w < g_renderZeroFloat) {
					face->nearClipState = -1;
					faceW = g_projScale;
				} else {
					faceW = remappedVert->w;
				}
			}

			faceWTotal += faceW;
			if (faceW > face->maxVertW) {
				face->maxVertW = faceW;
			}
			if (faceW < face->minVertW) {
				face->minVertW = faceW;
			}
		}
	}

	g_projVertCount += mesh->projVertCursor;
}

static inline void RenderScene_CopyTextureGradient(float* outGradient, const Vec3f* gradient) {
	outGradient[0] = gradient->x;
	outGradient[1] = gradient->y;
	outGradient[2] = gradient->z;
}

// FUNCTION: XWA 0x439A00
SceneFace* RenderScene_TransformFaceTextureGradients(SceneFace* face,
													 const FaceTextureGradients* faceTexGradients,
													 const float* viewPosAndOrient) {
	double x;
	double y;
	double z;

	RenderScene_CopyTextureGradient(&face->gradients[0], &faceTexGradients->gradient0);

	x = face->gradients[0];
	z = face->gradients[2];
	y = face->gradients[1];

	face->gradients[0] = x * viewPosAndOrient[3] + viewPosAndOrient[9] * z + viewPosAndOrient[6] * y;
	face->gradients[1] = viewPosAndOrient[7] * y + viewPosAndOrient[10] * z + viewPosAndOrient[4] * x;
	face->gradients[2] = viewPosAndOrient[11] * z + viewPosAndOrient[8] * y + viewPosAndOrient[5] * x;

	RenderScene_CopyTextureGradient(&face->gradients[3], &faceTexGradients->gradient1);

	x = face->gradients[3];
	z = face->gradients[5];
	y = face->gradients[4];

	face->gradients[3] = x * viewPosAndOrient[3] + viewPosAndOrient[9] * z + viewPosAndOrient[6] * y;
	face->gradients[4] = viewPosAndOrient[10] * z + viewPosAndOrient[7] * y + viewPosAndOrient[4] * x;
	face->gradients[5] = viewPosAndOrient[11] * z + viewPosAndOrient[8] * y + viewPosAndOrient[5] * x;
	return face;
}

// FUNCTION: XWA 0x442820
int sw3d_ProjectMeshVertices(SceneMesh* mesh) {
	SceneFace* face;
	ProjVertex* outVert;
	int vertexIdx;
	int faceIter;
	FaceRecord* faceRecord;
	const int* cornerIndices;
	int cornerIdx;
	int uvIdx;
	int normalIdx;
	int remappedIdx;
	float faceW;
	float faceWTotal;
	Vec3f vec;

	face = &g_visFaceList[mesh->faceBaseIndex];
	outVert = &g_projVertList[g_projVertCount];
	mesh->vertBaseIndex = g_projVertCount;
	mesh->projVertCursor = 0;

	for (vertexIdx = 0; vertexIdx < mesh->vertexCount; ++vertexIdx) {
		g_vertexRemap[vertexIdx] = -1;
	}

	for (faceIter = 0; faceIter < mesh->visFaceCount; ++faceIter, ++face) {
		if (!g_hwMipmapFilter) {
			RenderScene_TransformFaceTextureGradients(face, &mesh->pFaceTexturing[face->faceIndex],
													  &mesh->viewPos.x);
		}

		faceRecord = &mesh->pFaceGeom[face->faceIndex];
		face->maxVertW = 0.0f;
		face->minVertW = g_projScale;
		faceWTotal = 0.0f;
		face->nearClipState = 0;
		cornerIndices = faceRecord->vertexIdx;

		for (cornerIdx = 0; cornerIdx < 4; ++cornerIdx) {
			vertexIdx = cornerIndices[0];
			uvIdx = cornerIndices[8];
			normalIdx = cornerIndices[12];
			++cornerIndices;
			if (vertexIdx == -1) {
				break;
			}

			remappedIdx = g_vertexRemap[vertexIdx];
			if (remappedIdx == -1) {
				g_vertexRemap[vertexIdx] = mesh->projVertCursor;
				++mesh->projVertCursor;

				vec = mesh->pModelVerts[vertexIdx];
				Math3D_RotateVec3(&vec, (Matrix3x3*)mesh->viewOrient);
				vec.x += mesh->viewPos.x;
				vec.y += mesh->viewPos.y;
				vec.z += mesh->viewPos.z;

				if (vec.z < g_renderUnitFloat) {
					outVert->w = vec.z - g_renderUnitFloat;
					outVert->sx = vec.x;
					outVert->sy = vec.y;
					--face->nearClipState;
					faceW = g_projScale;
				} else {
					outVert->w = g_projScale / vec.z;
					outVert->sx = vec.x * outVert->w;
					faceW = outVert->w;
					outVert->sy = vec.y * outVert->w;
					outVert->sx += g_flightVpCenterXf;
					outVert->sy += g_projOffsetYf + g_flightVpCenterYf;
				}

				RenderScene_ComputeVertexLighting(mesh, outVert, &mesh->pVertNormals[normalIdx],
												  &mesh->pModelVerts[vertexIdx], &g_meshEyePos);
				outVert->tu = mesh->pUVs[uvIdx].u;
				outVert->tv = mesh->pUVs[uvIdx].v;
				outVert->extraLayerUVCount = mesh->textureLayerCount;
				if (mesh->textureLayerCount > 0) {
					int layerIdx;

					for (layerIdx = 0; layerIdx < outVert->extraLayerUVCount; ++layerIdx) {
						outVert->extraLayerUVs[layerIdx] = mesh->layers[layerIdx]->texCoords[vertexIdx];
					}
				}
				++outVert;
			} else {
				ProjVertex* remappedVert;

				remappedVert = &g_projVertList[mesh->vertBaseIndex + remappedIdx];
				if (remappedVert->w < g_renderZeroFloat) {
					--face->nearClipState;
					faceW = g_projScale;
				} else {
					faceW = remappedVert->w;
				}
			}

			faceWTotal += faceW;
			if (faceW > face->maxVertW) {
				face->maxVertW = faceW;
			}
			if (faceW < face->minVertW) {
				face->minVertW = faceW;
			}
		}

		if (!g_hwMipmapFilter && mesh->pUVs != NULL) {
			int baseVertexIdx;
			int textureArea;
			float uvU;
			float uvV;
			float cof00;
			float cof01;
			float cof02;
			float cof10;
			float cof11;
			float cof12;
			float cof20;
			float cof21;
			float cof22;
			float invDet;
			float scaledInvDet;
			float oldG0;
			float oldG1;
			float oldG2;
			float oldG3;
			float oldG4;
			float oldG5;
			float oldG6;
			float oldG7;
			float oldG8;
			float screenOffsetY;
			float areaScale;
			float cornerCount;
			float lodScale;
			float mipScale;

			cornerIndices -= 4;
			uvIdx = cornerIndices[8];
			baseVertexIdx = cornerIndices[0];
			vec = mesh->pModelVerts[baseVertexIdx];
			Math3D_RotateVec3(&vec, (Matrix3x3*)mesh->viewOrient);
			vec.x += mesh->viewPos.x;
			vec.y += mesh->viewPos.y;
			vec.z += mesh->viewPos.z;

			uvU = mesh->pUVs[uvIdx].u;
			uvV = mesh->pUVs[uvIdx].v;

			face->gradients[6] = vec.x - face->gradients[0] * uvU - uvV * face->gradients[3];
			face->gradients[7] = vec.y - face->gradients[1] * uvU - uvV * face->gradients[4];
			face->gradients[8] = vec.z - face->gradients[2] * uvU - uvV * face->gradients[5];

			cof00 = face->gradients[8] * face->gradients[4] - face->gradients[5] * face->gradients[7];
			cof01 = face->gradients[5] * face->gradients[6] - face->gradients[8] * face->gradients[3];
			cof20 = face->gradients[5] * face->gradients[1] - face->gradients[2] * face->gradients[4];
			cof02 = face->gradients[7] * face->gradients[3] - face->gradients[4] * face->gradients[6];
			cof10 = face->gradients[2] * face->gradients[7] - face->gradients[8] * face->gradients[1];
			cof11 = face->gradients[8] * face->gradients[0] - face->gradients[2] * face->gradients[6];
			cof12 = face->gradients[1] * face->gradients[6] - face->gradients[7] * face->gradients[0];
			cof21 = face->gradients[2] * face->gradients[3] - face->gradients[5] * face->gradients[0];
			cof22 = face->gradients[4] * face->gradients[0] - face->gradients[1] * face->gradients[3];
			if (cof20 == 0.0f && cof21 == 0.0f && cof22 == 0.0f) {
				cof22 = 1.0f;
			}

			invDet =
				1.0f / (cof21 * face->gradients[7] + cof22 * face->gradients[8] + cof20 * face->gradients[6]);
			scaledInvDet = invDet * g_invProjScale;

			oldG0 = scaledInvDet * cof00;
			oldG1 = scaledInvDet * cof01;
			oldG2 = invDet * cof02;
			oldG3 = scaledInvDet * cof10;
			oldG4 = scaledInvDet * cof11;
			oldG5 = invDet * cof12;
			oldG6 = scaledInvDet * cof20;
			oldG7 = scaledInvDet * cof21;
			oldG8 = invDet * cof22;

			face->gradients[0] = oldG0;
			face->gradients[1] = oldG1;
			face->gradients[2] = oldG2;
			face->gradients[3] = oldG3;
			face->gradients[4] = oldG4;
			face->gradients[5] = oldG5;
			face->gradients[6] = oldG6;
			face->gradients[7] = oldG7;
			face->gradients[8] = oldG8;

			screenOffsetY = g_projOffsetYf + g_flightVpCenterYf;
			face->gradients[2] = oldG2 - g_flightVpCenterXf * oldG0;
			face->gradients[2] -= screenOffsetY * oldG1;
			face->gradients[5] = oldG5 - g_flightVpCenterXf * oldG3;
			face->gradients[5] -= screenOffsetY * oldG4;

			areaScale = oldG0 * oldG4 - oldG3 * oldG1;
			face->gradients[8] = oldG8 - g_flightVpCenterXf * oldG6;
			face->gradients[8] -= screenOffsetY * oldG7;
			if (areaScale < 0.0f) {
				areaScale = -areaScale;
			}

			if (cornerIndices[3] == -1) {
				cornerCount = 3.0f;
			} else {
				cornerCount = 4.0f;
			}
			lodScale = cornerCount / faceWTotal * g_projScale;
			mipScale = lodScale * lodScale * areaScale;
			textureArea = mesh->pMaterial->height;
			textureArea *= mesh->pMaterial->width;
			mipScale *= (float)(textureArea << 8);
			face->mipLevel = (int)mipScale;
		}
	}

	g_projVertCount += mesh->projVertCursor;
	return g_projVertCount;
}

// FUNCTION: XWA 0x438AF0
static void sw3d_ComputeVertexLightIntensity(SceneMesh* mesh, ProjVertex* outVert, const Vec3f* normal,
											 const Vec3f* modelPos, const Vec3f* eyePos) {
	float* lightIntensity;
	float dotXY;
	unsigned int genusId;
	int lightIdx;

	genusId = mesh->pObject->genusId;
	if (genusId == GENUS_NpcProjectile || genusId == GENUS_PlayerProjectile) {
		outVert->lightIntensity = 0.40000001f;
		return;
	}

	if (g_dirLightingEnabled) {
		lightIntensity = &outVert->lightIntensity;
		*lightIntensity = 0.2f;
		for (lightIdx = 0; lightIdx < g_dirLightCount; ++lightIdx) {
			float dot;
			float intensity;

			dotXY = g_directionalLights[lightIdx].localDirY * normal->y +
					g_directionalLights[lightIdx].localDirX * normal->x;
			dot = dotXY + g_directionalLights[lightIdx].localDirZ * normal->z;
			if (dot > g_sw3dLightZero) {
				intensity = g_directionalLights[lightIdx].intensity;
				if (!g_useHardware3D) {
					intensity *= g_sw3dLightHalf;
				}
				*lightIntensity += dot * intensity;
				if (*lightIntensity >= g_sw3dLightUnit) {
					*lightIntensity = 1.0f;
					return;
				}
			}
		}
	} else {
		lightIntensity = &outVert->lightIntensity;
		*lightIntensity = 0.40000001f;
	}

	for (lightIdx = 0; lightIdx < g_objectPointLightCount; ++lightIdx) {
		PointLight* light;
		float deltaX;
		float deltaY;
		float deltaZ;
		float dot;

		light = &g_objectPointLights[lightIdx];
		deltaZ = light->z - modelPos->z;
		deltaY = light->y - modelPos->y;
		deltaX = light->x - modelPos->x;
		dotXY = deltaX * normal->x + deltaY * normal->y;
		dot = dotXY + deltaZ * normal->z;
		if (dot >= g_sw3dLightZero) {
			float distance;
			float distanceSquared;
			float scale;
			float specular;

			distance = RenderScene_RoughDistance3(deltaX, deltaY, deltaZ, g_sw3dLightZero,
												  g_sw3dLightRoughDistanceScale);
			distanceSquared = distance * distance;
			scale = dot / distanceSquared;
			if (g_specularEnabled) {
				float halfX;
				float halfY;
				float halfZ;
				float halfDot;
				float absHalfX;
				float absHalfY;
				float absHalfZ;
				float specularDenom;

				halfY = eyePos->y - modelPos->y + deltaY;
				halfX = eyePos->x - modelPos->x + deltaX;
				halfZ = eyePos->z - modelPos->z + deltaZ;
				halfDot = (halfX * normal->x + halfY * normal->y + halfZ * normal->z) * g_sw3dLightHalf;
				absHalfX = halfX;
				absHalfY = halfY;
				absHalfZ = halfZ;
				if (absHalfX < g_sw3dLightZero) {
					absHalfX = -absHalfX;
				}
				if (absHalfY < g_sw3dLightZero) {
					absHalfY = -absHalfY;
				}
				if (absHalfZ < g_sw3dLightZero) {
					absHalfZ = -absHalfZ;
				}
				if (absHalfX >= absHalfY && absHalfX >= absHalfZ) {
					specularDenom = (absHalfY + absHalfZ) * g_sw3dLightRoughSpecularMinorScale -
									absHalfX * g_sw3dLightRoughSpecularMajorScale;
				} else if (absHalfY >= absHalfX && absHalfY >= absHalfZ) {
					specularDenom = (absHalfX + absHalfZ) * g_sw3dLightRoughSpecularMinorScale -
									absHalfY * g_sw3dLightRoughSpecularMajorScale;
				} else {
					specularDenom = (absHalfX + absHalfY) * g_sw3dLightRoughSpecularMinorScale -
									absHalfZ * g_sw3dLightRoughSpecularMajorScale;
				}
				halfDot /= specularDenom;
				if (halfDot >= g_sw3dLightHalf) {
					specular = halfDot * halfDot * halfDot;
					specular *= specular;
					specular *= specular;
					specular *= specular;
					specular *= specular;
				} else {
					specular = g_sw3dLightZero;
				}
			} else {
				specular = g_sw3dLightZero;
			}
			scale += specular;
			if (scale > g_sw3dLightZero) {
				*lightIntensity += scale * light->intensity;
				if (*lightIntensity >= g_sw3dLightUnit) {
					*lightIntensity = 1.0f;
					return;
				}
			}
		}
	}
}

// FUNCTION: XWA 0x47EA40
int sw3d_ProjectPreviewVisibleFaceVertices(SceneMesh* mesh) {
	SceneFace* face;
	ProjVertex* outVert;
	int vertexIdx;
	int vertBaseIndex;
	int faceIter;
	const int* cornerIndices;
	float faceWTotal;
	int cornerIdx;
	int normalIdx;
	int remappedIdx;
	float faceW;
	int traceCount;
	Vec3f vec;

	face = &g_visFaceList[mesh->faceBaseIndex];
	vertBaseIndex = g_projVertCount;
	mesh->vertBaseIndex = vertBaseIndex;
	outVert = &g_projVertList[vertBaseIndex];
	mesh->projVertCursor = 0;

	for (vertexIdx = 0; vertexIdx < mesh->vertexCount; ++vertexIdx) {
		g_vertexRemap[vertexIdx] = -1;
	}

	for (faceIter = 0; faceIter < mesh->visFaceCount; ++faceIter, ++face) {
		RenderScene_TransformFaceTextureGradients(face, &mesh->pFaceTexturing[face->faceIndex],
												  &mesh->viewPos.x);
		cornerIndices = mesh->pFaceGeom[face->faceIndex].vertexIdx;
		face->maxVertW = 0.0f;
		cornerIdx = 0;
		faceWTotal = 0.0f;
		face->minVertW = (float)(unsigned int)g_projScaleInt;
		traceCount = g_projectedFaceTraceCount;
		for (; cornerIdx < 4; ++cornerIdx) {
			vertexIdx = cornerIndices[0];
			normalIdx = cornerIndices[12];
			++cornerIndices;
			if (vertexIdx == -1) {
				break;
			}

			remappedIdx = g_vertexRemap[vertexIdx];
			if (remappedIdx == -1) {
				g_vertexRemap[vertexIdx] = mesh->projVertCursor;
				++mesh->projVertCursor;

				vec = mesh->pModelVerts[vertexIdx];
				Math3D_RotateVec3(&vec, (Matrix3x3*)mesh->viewOrient);
				vec.x += mesh->viewPos.x;
				vec.y += mesh->viewPos.y;
				vec.z += mesh->viewPos.z;

				if (vec.z < g_sw3dUnitFloat) {
					outVert->w = vec.z - g_sw3dUnitFloat;
					outVert->sx = vec.x;
					outVert->sy = vec.y;
					face->nearClipState = -1;
					faceW = (float)(unsigned int)g_projScaleInt;
				} else {
					outVert->w = (float)(unsigned int)g_projScaleInt / vec.z;
					outVert->sx = vec.x * outVert->w;
					faceW = outVert->w;
					outVert->sy = vec.y * outVert->w;
					outVert->sx += (float)(g_flightVpWidth >> 1);
					outVert->sy += (float)(g_projOffsetY + (g_flightVpHeight >> 1));
				}

				sw3d_ComputeVertexLightIntensity(mesh, outVert, &mesh->pVertNormals[normalIdx],
												 &mesh->pModelVerts[vertexIdx], &g_meshEyePos);
				traceCount = g_projectedFaceTraceCount;
				if (traceCount < 5000) {
					g_projectedFaceTraceX[traceCount] = (int)outVert->sx;
					g_projectedFaceTraceY[traceCount] = (int)outVert->sy;
					g_projectedFaceTraceCount = traceCount + 1;
					traceCount = g_projectedFaceTraceCount;
				}
				++outVert;
			} else {
				ProjVertex* remappedVert;

				remappedVert = &g_projVertList[mesh->vertBaseIndex + remappedIdx];
				if (remappedVert->w < g_sw3dZeroFloat) {
					face->nearClipState = -1;
					traceCount = g_projectedFaceTraceCount;
					faceW = (float)(unsigned int)g_projScaleInt;
				} else {
					faceW = remappedVert->w;
				}
				if (traceCount < 5000) {
					g_projectedFaceTraceX[traceCount] = (int)remappedVert->sx;
					g_projectedFaceTraceY[traceCount] = (int)remappedVert->sy;
					g_projectedFaceTraceCount = traceCount + 1;
					traceCount = g_projectedFaceTraceCount;
				}
			}

			faceWTotal += faceW;
			if (faceW > face->maxVertW) {
				face->maxVertW = faceW;
				traceCount = g_projectedFaceTraceCount;
			}
			if (faceW < face->minVertW) {
				face->minVertW = faceW;
				traceCount = g_projectedFaceTraceCount;
			}
		}

		g_projectedFaceTraceX[traceCount] = 0;
		g_projectedFaceTraceY[traceCount] = 0;
		g_projectedFaceTraceCount = traceCount + 1;
	}

	g_projVertCount += mesh->projVertCursor;
	return g_projVertCount;
}
