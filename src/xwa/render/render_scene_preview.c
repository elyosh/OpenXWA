#include "xwa/render/renderer_internal.h"
#include "xwa/util/string.h"

// FUNCTION: XWA 0x482D60
void RenderScene_WalkPreviewOptNodeForProjection(OptimizedPolyObject* model, OptNode* node, SceneMesh* mesh) {
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
			for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
				OptNode* rootNode;

				rootNode = model->rootNodes[rootIndex];
				if (rootNode != NULL) {
					if (rootNode->pName == NULL || Xwa_CrtStricmp(rootNode->pName, refName) != 0) {
						int childIndex;

						childIndex = 0;
						if (rootNode->childCount <= 0) {
							resolvedNode = NULL;
						} else {
							while (1) {
								resolvedNode =
									OptModel_FindNodeByName(rootNode->pChildren[childIndex], refName);
								if (resolvedNode != NULL) {
									break;
								}
								++childIndex;
								if (childIndex >= rootNode->childCount) {
									resolvedNode = NULL;
									break;
								}
							}
						}
					} else {
						resolvedNode = rootNode;
					}
				} else {
					resolvedNode = NULL;
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
			case OPT_ROTSCALE:
				if (mesh->rotAngle != 0.0f) {
					Vec3f* pivot;
					Vec3f* axis;

					pivot = (Vec3f*)nodeData;
					axis = pivot + 1;
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
				}
				break;
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
				float viewOrient4;
				float viewOrient5;
				float viewOrient7;
				float viewOrient8;
				float orient0;
				float orient1;
				float orient2;
				float orient4;
				float orient5;
				float orient6;
				float orient7;
				float orient8;

				scale = (Vec3f*)nodeData;
				mesh->viewOrient[0] = scale->x * mesh->viewOrient[0];
				viewOrient4 = mesh->viewOrient[4];
				viewOrient5 = mesh->viewOrient[5];
				mesh->viewOrient[1] = mesh->viewOrient[1] * scale->y;
				viewOrient7 = mesh->viewOrient[7];
				viewOrient8 = mesh->viewOrient[8];
				mesh->viewOrient[2] = mesh->viewOrient[2] * scale->z;
				mesh->viewOrient[3] = scale->x * mesh->viewOrient[3];
				mesh->viewOrient[4] = viewOrient4 * scale->y;
				mesh->viewOrient[5] = viewOrient5 * scale->z;
				mesh->viewOrient[6] = scale->x * mesh->viewOrient[6];
				mesh->viewOrient[7] = viewOrient7 * scale->y;
				mesh->viewOrient[8] = viewOrient8 * scale->z;
				mesh->viewPos.x = scale->x * mesh->viewPos.x;
				mesh->viewPos.y = mesh->viewPos.y * scale->y;
				mesh->viewPos.z = mesh->viewPos.z * scale->z;

				invX = 1.0f / scale->x;
				orient0 = invX * mesh->orient[0];
				orient1 = invX * mesh->orient[1];
				orient2 = invX * mesh->orient[2];
				mesh->orient[0] = orient0;
				mesh->orient[1] = orient1;
				mesh->orient[2] = orient2;
				invX = 1.0f / scale->y;
				orient4 = invX * mesh->orient[4];
				orient5 = invX * mesh->orient[5];
				mesh->orient[3] = invX * mesh->orient[3];
				mesh->orient[4] = orient4;
				mesh->orient[5] = orient5;
				invX = 1.0f / scale->z;
				orient6 = invX * mesh->orient[6];
				orient7 = invX * mesh->orient[7];
				orient8 = invX * mesh->orient[8];
				mesh->orient[6] = orient6;
				mesh->orient[7] = orient7;
				mesh->orient[8] = orient8;
				break;
			}
			case OPT_MESHVERTS:
				mesh->vertexCount = (int)curNode->param1;
				mesh->pModelVerts = (Vec3f*)nodeData;
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
				faceRecords = (FaceRecord*)((uint8_t*)nodeData + sizeof(int));
				mesh->pFaceGeom = faceRecords;
				faceNormals = (Vec3f*)&faceRecords[curNode->param1];
				mesh->pFaceNormals = faceNormals;
				faceTexturing = (FaceTextureGradients*)&faceNormals[curNode->param1];
				mesh->pFaceTexturing = faceTexturing;
				inlineVertNormals = &faceTexturing[curNode->param1].gradient0;
				if (mesh->pMaterial == NULL) {
					mesh->pMaterial = (OptTextureData*)g_curTextureDesc;
					mesh->pTexels = (uint8_t*)g_curTextureDesc;
					mesh->pTexels = (uint8_t*)g_curTextureDesc + sizeof(OptTextureData);
#ifdef XWA_MODERN
					mesh->pPalette = g_curTexturePalette;
#else
					mesh->pPalette = (void*)(uintptr_t)((OptTextureData*)g_curTextureDesc)->paletteAddress;
#endif
				}

				if (mesh->pVertNormals == NULL) {
					mesh->pVertNormals = inlineVertNormals;
					g_projVertCount = 0;
					g_sceneEdgeCursor = 0;
					if (g_meshQueueIndex != g_meshQueueMax &&
						mesh->faceCount + g_visFaceCount <= g_sceneFaceMax &&
						mesh->vertexCount <= g_projVertMax && mesh->edgeCount <= g_sceneEdgeMax) {
						SceneMesh* queuedMesh;

						memcpy(&g_meshQueue[g_meshQueueIndex], mesh, sizeof(*mesh));
						queuedMesh = &g_meshQueue[g_meshQueueIndex];
						RenderScene_AppendMeshFacesNoCull(queuedMesh);
						if (queuedMesh->visFaceCount != 0) {
							sw3d_ProjectPreviewVisibleFaceVertices(queuedMesh);
							++g_meshQueueIndex;
						}
					}
					mesh->pVertNormals = NULL;
				} else {
					g_projVertCount = 0;
					g_sceneEdgeCursor = 0;
					if (g_meshQueueIndex != g_meshQueueMax &&
						mesh->faceCount + g_visFaceCount <= g_sceneFaceMax &&
						mesh->vertexCount <= g_projVertMax && mesh->edgeCount <= g_sceneEdgeMax) {
						SceneMesh* queuedMesh;

						memcpy(&g_meshQueue[g_meshQueueIndex], mesh, sizeof(*mesh));
						queuedMesh = &g_meshQueue[g_meshQueueIndex];
						RenderScene_AppendMeshFacesNoCull(queuedMesh);
						if (queuedMesh->visFaceCount != 0) {
							sw3d_ProjectPreviewVisibleFaceVertices(queuedMesh);
							++g_meshQueueIndex;
						}
					}
				}
				break;
			}
			case OPT_NODESWITCH:
				nodeSwitchSelection = g_nodeSwitchIndex + 1;
				if (nodeSwitchSelection > curNode->childCount) {
					nodeSwitchSelection = curNode->childCount;
				}
				break;
			case OPT_TEXTURE: {
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
				break;
			}
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
				g_curTextureDesc = mesh->pMaterial;
				mesh->pTexels = (uint8_t*)mesh->pMaterial + sizeof(OptTextureData);
#ifdef XWA_MODERN
				g_curTexturePalette = OptModel_ResolveTexturePalette(curNode);
				mesh->pPalette = g_curTexturePalette;
#else
				mesh->pPalette = (void*)(uintptr_t)((OptTextureData*)g_curTextureDesc)->paletteAddress;
#endif
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
		RenderScene_WalkPreviewOptNodeForProjection(model, curNode->pChildren[nodeSwitchSelection - 1], mesh);
	} else if (lodChildSelection) {
		if (lodChildSelection != -1) {
			++g_curLayerId;
			RenderScene_WalkPreviewOptNodeForProjection(model, curNode->pChildren[lodChildSelection - 1],
														mesh);
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
			RenderScene_WalkPreviewOptNodeForProjection(model, curNode->pChildren[childIndex], &childMesh);
		}
	}
}

// FUNCTION: XWA 0x481640
int RenderScene_ProjectPreviewWireframeModel(ObjectRecord* obj) {
	MemoryHandle modelHandle;
	MobileObject* mobj;
	OptimizedPolyObject* model;
	SceneMesh mesh;
	Matrix3x3 out;
	float axisAngle[4];
	SceneMesh savedMesh;
	int rootIndex;
	int meshOrdinal;
	int saveMeshForBwing;

	g_projectedFaceTraceCount = 0;
	modelHandle = g_loadedModels.byObjectType[(uint16_t)obj->objectType];
	mobj = obj->mobj;
	if (mobj != NULL) {
		g_nodeSwitchIndex = mobj->nodeSwitchIndex;
	} else {
		g_nodeSwitchIndex = 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	memset(&mesh, 0, sizeof(mesh));
	mesh.pObject = obj;
	mesh.viewPos.x = (float)(obj->world_x - g_players[g_localPlayer].viewState.savedTargetX);
	mesh.viewPos.y = (float)(obj->world_y - g_players[g_localPlayer].viewState.savedTargetY);
	mesh.viewPos.z = (float)(obj->world_z - g_players[g_localPlayer].viewState.savedTargetZ);
	if (g_cockpitViewActive) {
		mesh.viewPos.x = -g_players[g_localPlayer].hardpointWorldX -
						 (double)g_players[g_localPlayer].viewState.cameraPanDeltaX * 0.0625;
		mesh.viewPos.y = -g_players[g_localPlayer].hardpointWorldY -
						 (double)g_players[g_localPlayer].viewState.cameraPanDeltaY * 0.0625;
		mesh.viewPos.z = -g_players[g_localPlayer].hardpointWorldZ -
						 (double)g_players[g_localPlayer].viewState.cameraPanDeltaZ * 0.0625;
	}

	mesh.viewOrient[0] = g_viewMtx00;
	mesh.viewOrient[1] = g_viewMtx10;
	mesh.viewOrient[3] = g_viewMtx01;
	mesh.viewOrient[4] = g_viewMtx11;
	mesh.viewOrient[2] = g_viewMtx20;
	mesh.viewOrient[6] = g_viewMtx02;
	mesh.viewOrient[7] = g_viewMtx12;
	mesh.viewOrient[5] = g_viewMtx21;
	mesh.viewOrient[8] = g_viewMtx22;
	Math3D_RotateVec3(&mesh.viewPos, (Matrix3x3*)mesh.viewOrient);

	mesh.viewOrient[0] = g_objViewMatF_R0_X;
	mesh.viewOrient[1] = g_objViewMatF_R0_Y;
	mesh.viewOrient[3] = g_objViewMatF_R1_X;
	mesh.viewOrient[4] = g_objViewMatF_R1_Y;
	mesh.viewOrient[2] = g_objViewMatF_R0_Z;
	mesh.viewOrient[6] = g_objViewMatF_R2_X;
	mesh.viewOrient[7] = g_objViewMatF_R2_Y;
	mesh.viewOrient[5] = g_objViewMatF_R1_Z;
	mesh.orient[0] = g_objViewMatF_R0_X;
	mesh.orient[1] = g_objViewMatF_R1_X;
	mesh.viewOrient[8] = g_objViewMatF_R2_Z;
	mesh.orient[3] = g_objViewMatF_R0_Y;
	mesh.orient[4] = g_objViewMatF_R1_Y;
	mesh.pos.x = -mesh.viewPos.x;
	mesh.orient[2] = g_objViewMatF_R2_X;
	mesh.pos.y = -mesh.viewPos.y;
	mesh.orient[6] = g_objViewMatF_R0_Z;
	mesh.orient[7] = g_objViewMatF_R1_Z;
	mesh.orient[5] = g_objViewMatF_R2_Y;
	mesh.pos.z = -mesh.viewPos.z;
	mesh.orient[8] = g_objViewMatF_R2_Z;
	Math3D_RotateVec3(&mesh.pos, (Matrix3x3*)mesh.orient);

	g_modelNodeWalkUnusedScratch0 = 0;
	g_curTextureDesc = ModelTexture_GetDefaultWhiteTexture();
#ifdef XWA_MODERN
	g_curTexturePalette = ModelTexture_GetDefaultWhiteTexture()->data.shadeTable;
#endif
	g_modelNodeWalkUnusedScratch1 = 0;
	g_curVertNormals = 0;
	g_modelNodeWalkUnusedScratch2 = 0;
	g_curMeshFlags = 0;
	g_curVertexCount = 0;

	meshOrdinal = 0;
	saveMeshForBwing = 0;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* node;
		OptNodeType nodeType;

		mesh.rotAngle = 0.0f;
		node = model->rootNodes[rootIndex];
		nodeType = node->nodeType;
		if (nodeType != OPT_TEXTURE && nodeType != OPT_TEXTURE_REF) {
			++meshOrdinal;
			mobj = obj->mobj;
			if (mobj != NULL && mobj->pCraft != NULL) {
				CraftData* craft;

				craft = mobj->pCraft;
				if (craft->componentState[meshOrdinal - 1] != 0) {
					continue;
				}

				if (obj->objectType == OBJ_BWing) {
					int bridgeIndex;

					bridgeIndex = g_bwingBridgeMeshIndexCache;
					if (bridgeIndex == -1) {
						bridgeIndex = ModelMesh_FindBridgeIndex(model);
						g_bwingBridgeMeshIndexCache = bridgeIndex;
					}
					if (bridgeIndex != -1 && obj->mobj->pCraft->meshRotation[bridgeIndex] != 0) {
						memcpy(&savedMesh, &mesh, sizeof(savedMesh));
						saveMeshForBwing = 1;
						axisAngle[0] = 0.0f;
						axisAngle[1] = -1.0f;
						axisAngle[2] = 0.0f;
						axisAngle[3] =
							(float)((double)obj->mobj->pCraft->meshRotation[bridgeIndex] * 0.024543673);
						Math3D_BuildAxisAngleMatrix(&out, axisAngle);
						Math3D_MulMatrix3x3((Matrix3x3*)mesh.orient, &out);
						Math3D_RotateVec3(&mesh.pos, &out);
						Math3D_MulMatrix3x3T((Matrix3x3*)mesh.viewOrient, &out);
					}
				}

				mesh.rotAngle = (float)((double)craft->meshRotation[meshOrdinal - 1] * 0.024543673);
			}
		}

		++g_curLayerId;
		RenderScene_WalkPreviewOptNodeForProjection(model, node, &mesh);
		if (saveMeshForBwing) {
			memcpy(&mesh, &savedMesh, sizeof(mesh));
			saveMeshForBwing = 0;
		}
	}

	return Memory_UnlockHandle(modelHandle);
}
