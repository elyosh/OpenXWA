#include "xwa/render/renderer_internal.h"

#include "xwa/flight/object/damage.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/yard.h"

#ifdef XWA_MODERN
#include "xwa/frontend/frontend_display.h" /* g_flightRenderToFrontend */
#endif

#ifndef XWA_MODERN
__declspec(dllimport) extern int(__stdcall* VirtualProtect)(void* lpAddress, unsigned int dwSize,
															unsigned int flNewProtect,
															unsigned int* lpflOldProtect);
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* outputString);
#else
static int VirtualProtect(void* lpAddress, unsigned int dwSize, unsigned int flNewProtect,
						  unsigned int* lpflOldProtect);
static void OutputDebugStringA(const char* outputString) { DebugPrintf("%s", outputString); }

/* Host shim for the Win32 VirtualProtect the original called before switching renderer
 * mode: it made RenderScene_Initialize's code region writable so the engine could patch
 * its own machine code at runtime. The source port does not self-modify code, so the call
 * only needs to report success (non-zero) and hand back a plausible previous protection. */
static int VirtualProtect(void* lpAddress, unsigned int dwSize, unsigned int flNewProtect,
						  unsigned int* lpflOldProtect) {
	(void)lpAddress;
	(void)dwSize;
	(void)flNewProtect;
	if (lpflOldProtect) {
		*lpflOldProtect = 0x40u; /* PAGE_EXECUTE_READWRITE */
	}
	return 1;
}
#endif

// FUNCTION: XWA 0x4421C0
void RenderScene_SetDepthProjectionScale(float depthProjScale) { g_depthProjScale = depthProjScale; }

// FUNCTION: XWA 0x4421D0
void RenderScene_ResetDepthProjectionScale(void) { g_depthProjScale = 2048.0f; }

// FUNCTION: XWA 0x44FCC0
void RenderScene_SetProjectionDepthOverride(float depthZ) { g_renderProjectionDepthOverrideZ = depthZ; }

// FUNCTION: XWA 0x44FCD0
void RenderScene_EnableProjectionYClamp(float clampY0, float clampY1, float clampX0, float clampX1) {
	g_renderProjectionClampY0 = clampY0;
	g_renderProjectionClampY1 = clampY1;
	g_renderProjectionClampX0 = clampX0;
	g_renderProjectionClampX1 = clampX1;
	g_renderProjectionYClampEnabled = 1;
}

// FUNCTION: XWA 0x44FD00
void RenderScene_DisableProjectionYClamp(void) { g_renderProjectionYClampEnabled = 0; }

// FUNCTION: XWA 0x4E40B0
void RenderNonCraftSceneObject(uint16_t objectIndex) {
	uint16_t objectType;
	uint16_t frame;
	int objectTypeIdx;

	objectType = g_objectTable[objectIndex].objectType;
	objectTypeIdx = (uint16_t)objectType;

	if (g_modelTypeTable[objectTypeIdx].texLevels == NULL) {
		if (g_objectTable[objectIndex].typeSpecificByte[0] == 0) {
			Damage_QueueCraftBillboards(objectIndex);
			RenderScene_DrawObjectModel(&g_objectTable[objectIndex]);
		}
	} else {
		frame = g_objectTable[objectIndex].typeSpecificByte[0];
		if (viewZ >= 0) {
			int matrixX;
			int matrixY;
			uint16_t billboardAngle;
			int screenX;
			int absR0Z;
			int absR1Z;

			absR0Z = g_objViewMat_R0_Z;
			absR1Z = g_objViewMat_R1_Z;
			if (absR0Z < 0) {
				absR0Z = -absR0Z;
			}
			if (absR1Z < 0) {
				absR1Z = -absR1Z;
			}

			if (absR0Z < absR1Z) {
				matrixX = g_objViewMat_R0_X;
				matrixY = g_objViewMat_R0_Y;
			} else {
				matrixX = g_objViewMat_R1_X;
				matrixY = g_objViewMat_R1_Y;
			}

			if (matrixX < 0) {
				billboardAngle = trig2_arctan(matrixY, -matrixX);
			} else {
				billboardAngle = trig2_arctan(matrixY, matrixX);
				billboardAngle = -billboardAngle;
			}

			screenX = TRANSFM2_ProjectScreenX(viewX, viewZ);
			if (Xwa_IsProjectedCoordSigned16(screenX)) {
				int screenY;

				screenY = TRANSFM2_ProjectScreenY(viewY, viewZ);
				if (Xwa_IsProjectedCoordSigned16(screenY)) {
					int16_t queuedScreenX;
					int16_t queuedScreenY;

					queuedScreenX = (int16_t)screenX;
					queuedScreenY = (int16_t)(2 * ((uint16_t)g_flightVpHeight >> 1) - screenY);
					SceneBillboard_QueueProjectedTextured((uint16_t)objectType, frame, 256, queuedScreenX,
														  queuedScreenY, viewZ, (uint16_t)billboardAngle);
				}
			}
		}
	}
}

// FUNCTION: XWA 0x448200
void RenderScene_InitHardwareFrame(void) {
	uint32_t maxVertexCount;
	int maxTriCount;
	uint32_t spanBytes;
	uint32_t triangleCapacity;
	uint64_t viewportOrigin;
	int viewportOriginX;
	int viewportOriginY;
	int zero;

	zero = 0;
	if (g_std3DStartScenePending) {
		std3D_StartScene();
		g_std3DStartScenePending = zero;
	}

	viewportOriginX = width - g_surfaceWidth;
	viewportOriginY = height - g_surfaceHeight;
	viewportOriginX = g_flightVpX + ((uint32_t)viewportOriginX >> 1);
	viewportOriginY = g_flightVpY + ((uint32_t)viewportOriginY >> 1);
	viewportOrigin = (uint64_t)(uint32_t)viewportOriginX;
	g_d3dIndexCount = zero;
	g_flightVpOriginX = (float)(int64_t)viewportOrigin;
	viewportOrigin = (uint64_t)(uint32_t)viewportOriginY;
	g_d3dVertexCount = zero;
	g_d3dVertexAlphaStateResetSlot = zero;
	g_capVertexAlpha = 1;
	g_flightVpOriginY = (float)(int64_t)viewportOrigin;
	spanBytes = (uint32_t)(24 * g_sceneSpanDataMax);
	maxVertexCount = spanBytes >> 7;
	maxTriCount = (int)((spanBytes >> 2) / 20u);
	g_maxBatchVerts = maxVertexCount;
	g_maxBatchTris = maxTriCount;
	if (maxVertexCount > g_pStd3DCurDevice->caps.maxVertexCount) {
		maxVertexCount = g_pStd3DCurDevice->caps.maxVertexCount;
		g_maxBatchVerts = (int)maxVertexCount;
	}
	if (maxVertexCount > 256) {
		maxVertexCount = 256;
		g_maxBatchVerts = 256;
	}
	if (maxTriCount > 256) {
		maxTriCount = 256;
		g_maxBatchTris = 256;
	}
	triangleCapacity = (g_pStd3DCurDevice->caps.maxBufferSize - 64u * maxVertexCount) / 24u;
	if (maxTriCount > (int)triangleCapacity) {
		g_maxBatchTris = (int)triangleCapacity;
	}
	g_flightVertexBuffer = (D3DTLVERTEX*)g_sceneSpanData;
	g_triBuffer = (Std3DRenderTri*)(g_sceneSpanData + 24 * (g_sceneSpanDataMax / 2));
}

// FUNCTION: XWA 0x489310
void RenderScene_Initialize(int resetFlag) {
	uint8_t* mask;
	int visibleWidth;
	int visibleStartX;
	int currentMode;
	uint8_t runCount;
	unsigned int scanX;
	unsigned int scanY;
	unsigned int fpuControl;
	int useHardware;

	visibleStartX = 0;
	visibleWidth = 0;
	useHardware = g_useHardware3D;
	mask = g_cockpitMaskRle;
	do {
		if (useHardware) {
			currentMode = g_currentRenderMode;
			if (currentMode == 2) {
				break;
			}
			currentMode = 2;
		} else {
			currentMode = g_currentRenderMode;
			if (currentMode == 1) {
				break;
			}
			DebugPrintf(g_renderSoftwareRenderingMessage);
			VirtualProtect((void*)RenderScene_Initialize, 0x80000u, 0x40u,
						   &g_renderSceneVirtualProtectOldProtect);
			currentMode = 1;
		}
		g_currentRenderMode = currentMode;
	} while (0);

	if (g_useHardware3D) {
		if (currentMode != 2) {
			g_currentRenderMode = 2;
		}
	} else if (currentMode != 1) {
		DebugPrintf(g_renderSoftwareRenderingMessage);
		VirtualProtect((void*)RenderScene_Initialize, 0x80000u, 0x40u,
					   &g_renderSceneVirtualProtectOldProtect);
		g_currentRenderMode = 1;
	}

	fpuControl = g_sw3dFpuControlWordScratch;
	fpuControl &= 0xfffffcffu;
	g_sw3dFpuControlWordScratch = (uint16_t)fpuControl;
	g_sw3dCockpitMaskSentinelMaxX = 1.0e32f;
	g_sw3dCockpitMaskSentinelMaxY = 1.0e32f;
	g_sw3dCockpitMaskSentinelStartX = 0.0f;
	g_sw3dCockpitMaskSentinelEndX = 0.0f;
	g_sw3dCockpitMaskSentinelDepthZ = 1.0e32f;

	currentMode = g_useHardware3D;

	if (!currentMode) {
		if (resetFlag) {
			g_pSceneSpanDataCur = g_sceneSpanData;
			g_sceneSpanPtrAvail = g_sceneSpanPtrMax;
			g_visFaceDrawStartIndex = 0;
			g_visFaceCount = 0;
			g_phongSlotIndex = 0;
			g_meshQueueIndex = 0;
			g_pSceneSpanDataEnd =
				g_sceneSpanData + sizeof(SceneSpan) * g_sceneSpanDataMax - sizeof(SceneSpan);

			if (g_flightRenderToFrontend || g_sceneBypassCockpitMask) {
				for (scanY = 0; scanY < g_screenHeight; ++scanY) {
					((SceneSpan**)g_scanlineSpanHeads)[scanY] = NULL;
				}
			} else {
				for (scanY = 0; scanY < g_screenHeight; ++scanY) {
					SceneSpan* previousSpan;

					visibleWidth = 0;
					visibleStartX = 0;
					previousSpan = NULL;
					((SceneSpan**)g_scanlineSpanHeads)[scanY] = NULL;
					runCount = *mask++;
					scanX = 0;
					if (runCount != 0) {
						uint8_t savedRunCount;
						unsigned int spanEndX;

						spanEndX = 0;
						savedRunCount = runCount;
						while (1) {
							if (scanX >= g_screenWidth) {
								break;
							}
							if (*mask == 0) {
								uint8_t nextRunType;

								do {
									++mask;
									scanX += *mask;
									visibleStartX += *mask;
									nextRunType = mask[1];
									++mask;
									--runCount;
								} while (nextRunType == 0);
								savedRunCount = runCount;
								spanEndX = scanX;
							}
							if (*mask == 1) {
								uint8_t nextRunType;

								do {
									++mask;
									spanEndX += *mask;
									visibleWidth += *mask;
									nextRunType = mask[1];
									++mask;
									--runCount;
								} while (nextRunType == 1);
								savedRunCount = runCount;
							}
							if (visibleWidth != 0) {
								SceneSpan* span;

								span = (SceneSpan*)g_pSceneSpanDataCur;
								if (previousSpan == NULL) {
									((SceneSpan**)g_scanlineSpanHeads)[scanY] = span;
								} else {
									previousSpan->next = span;
								}
								previousSpan = span;
								g_pSceneSpanDataCur += sizeof(SceneSpan);
								span->startX = visibleStartX;
								span->endX = visibleStartX + visibleWidth;
								span->pFace = &g_sw3dCockpitMaskSentinelFace;
								span->next = NULL;
								visibleStartX += visibleWidth;
								visibleWidth = 0;
							}
							scanX = spanEndX;
							if (runCount == 0) {
								if (*mask != 0xffu) {
									OutputDebugStringA(g_renderCockpitMaskErrorMessage);
									runCount = savedRunCount;
								} else {
									++mask;
								}
								if (runCount == 0) {
									break;
								}
							}
						}
					}
				}
			}
		} else {
			g_visFaceDrawStartIndex = g_visFaceCount;
		}
	} else {
		g_sceneSpanPtrAvail = g_sceneSpanPtrMax;
		g_visFaceDrawStartIndex = 0;
		g_visFaceCount = 0;
		g_phongSlotIndex = 0;
		g_meshQueueIndex = 0;
		g_pSceneSpanDataCur = g_sceneSpanData;
		g_pSceneSpanDataEnd = g_sceneSpanData + sizeof(SceneSpan) * g_sceneSpanDataMax - sizeof(SceneSpan);
	}

	g_phongSlotStride = (g_sw3dLightSampleBlockSize + (unsigned int)g_flightVpWidth - 1u) /
						(unsigned int)g_sw3dLightSampleBlockSize;
	g_sw3dLightSampleCacheSceneStampBase += (uint16_t)g_flightVpHeight;
	g_invProjScale = g_renderUnitScale / (float)(unsigned int)g_projScaleInt;
	if (g_useHardware3D) {
		RenderScene_InitHardwareFrame();
	}
}

// FUNCTION: XWA 0x448B80
char RenderScene_ClearFrameBuffers(void) {
	DDBLTFX fx;

	/* Color-clear the 3D back buffer, then clear the z-buffer. The DDBLT_COLORFILL
	 * clears the shim render target's color target (DDShim_ClearRenderTargetColor). */
	memset(&fx, 0, sizeof(fx));
	fx.dwSize = 100;
	fx.dwROP = 0x00CC0020; /* SRCCOPY */
	fx.dwFillColor = g_flightTextPalette[g_flightColorEscapeBypassChar];
	g_flightBackBufferSurface->lpVtbl->Blt(g_flightBackBufferSurface, NULL, NULL, NULL,
										   DDBLT_WAIT | DDBLT_COLORFILL, &fx);
	return std3D_ClearZBuffer();
}

// FUNCTION: XWA 0x480A80
int16_t RenderScene_DrawObjectModel(ObjectRecord* obj) {
	MobileObject* mobj;
	MemoryHandle modelHandle;
	OptimizedPolyObject* model;
	SceneMesh mesh;
	Vec3f pivot;
	Matrix3x3 rhs;
	Matrix3x3 out;
	float axisAngle[4];
	float turretAxisAngle[4];
	SceneMesh savedMesh;
	int saveMeshForBwing;
	int rootIndex;
	int meshOrdinal;
	int result;

	mobj = obj->mobj;
	if (mobj) {
		g_nodeSwitchIndex = mobj->nodeSwitchIndex;
	} else {
		g_nodeSwitchIndex = 0;
	}

	modelHandle = g_loadedModels.byObjectType[(uint16_t)obj->objectType];
	result = modelHandle;
	if (modelHandle) {

		model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
		OptModel_AdjustOptimizedPolyObjectPointers(model);
		memset(&mesh, 0, sizeof(mesh));

		mesh.pObject = obj;
		mesh.viewPos.x = (float)(obj->world_x - g_players[g_localPlayer].viewState.savedTargetX);
		mesh.viewPos.y = (float)(obj->world_y - g_players[g_localPlayer].viewState.savedTargetY);
		mesh.viewPos.z = (float)(obj->world_z - g_players[g_localPlayer].viewState.savedTargetZ);
		if (g_cockpitViewActive) {
			mesh.viewPos.x = -g_players[g_localPlayer].hardpointWorldX -
							 g_players[g_localPlayer].viewState.cameraPanDeltaX * g_cockpitPanPositionScale;
			mesh.viewPos.y = -g_players[g_localPlayer].hardpointWorldY -
							 g_players[g_localPlayer].viewState.cameraPanDeltaY * g_cockpitPanPositionScale;
			mesh.viewPos.z = -g_players[g_localPlayer].hardpointWorldZ -
							 g_players[g_localPlayer].viewState.cameraPanDeltaZ * g_cockpitPanPositionScale;
		}

		mesh.viewOrient[0] = g_viewMtx00;
		mesh.viewOrient[1] = g_viewMtx10;
		mesh.viewOrient[2] = g_viewMtx20;
		mesh.viewOrient[3] = g_viewMtx01;
		mesh.viewOrient[4] = g_viewMtx11;
		mesh.viewOrient[5] = g_viewMtx21;
		mesh.viewOrient[6] = g_viewMtx02;
		mesh.viewOrient[7] = g_viewMtx12;
		mesh.viewOrient[8] = g_viewMtx22;
		Math3D_RotateVec3(&mesh.viewPos, (Matrix3x3*)mesh.viewOrient);

		mesh.viewOrient[0] = g_objViewMatF_R0_X;
		mesh.viewOrient[1] = g_objViewMatF_R0_Y;
		mesh.viewOrient[2] = g_objViewMatF_R0_Z;
		mesh.viewOrient[3] = g_objViewMatF_R1_X;
		mesh.viewOrient[4] = g_objViewMatF_R1_Y;
		mesh.viewOrient[5] = g_objViewMatF_R1_Z;
		mesh.viewOrient[6] = g_objViewMatF_R2_X;
		mesh.viewOrient[7] = g_objViewMatF_R2_Y;
		mesh.viewOrient[8] = g_objViewMatF_R2_Z;
		mesh.orient[0] = g_objViewMatF_R0_X;
		mesh.orient[1] = g_objViewMatF_R1_X;
		mesh.orient[2] = g_objViewMatF_R2_X;
		mesh.orient[3] = g_objViewMatF_R0_Y;
		mesh.orient[4] = g_objViewMatF_R1_Y;
		mesh.pos.x = -mesh.viewPos.x;
		mesh.pos.y = -mesh.viewPos.y;
		mesh.orient[5] = g_objViewMatF_R2_Y;
		mesh.orient[6] = g_objViewMatF_R0_Z;
		mesh.orient[7] = g_objViewMatF_R1_Z;
		mesh.pos.z = -mesh.viewPos.z;
		mesh.orient[8] = g_objViewMatF_R2_Z;
		Math3D_RotateVec3(&mesh.pos, (Matrix3x3*)mesh.orient);

		if (g_cockpitViewActive && g_players[g_localPlayer].currentSeatIdx == 2) {
			pivot.x = mesh.pos.x;
			pivot.y = mesh.pos.y;
			pivot.z = mesh.pos.z;
			mesh.pos.x = g_sw3dZeroFloat;
			mesh.pos.y = g_sw3dZeroFloat;
			mesh.pos.z = g_sw3dZeroFloat;

			mesh.viewPos.x = Math3D_RotateVec3X(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.x;
			mesh.viewPos.y = Math3D_RotateVec3Y(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.y;
			mesh.viewPos.z = Math3D_RotateVec3Z(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.z;

			rhs.m[0] = g_sw3dUnitFloat;
			rhs.m[3] = g_sw3dZeroFloat;
			rhs.m[6] = g_sw3dZeroFloat;
			rhs.m[1] = g_sw3dZeroFloat;
			rhs.m[4] = g_sw3dNegUnitFloat;
			rhs.m[7] = g_sw3dZeroFloat;
			rhs.m[2] = g_sw3dZeroFloat;
			rhs.m[5] = g_sw3dZeroFloat;
			rhs.m[8] = g_sw3dNegUnitFloat;

			Math3D_MulMatrix3x3((Matrix3x3*)mesh.orient, &rhs);
			Math3D_RotateVec3(&mesh.pos, &rhs);
			Math3D_MulMatrix3x3T((Matrix3x3*)mesh.viewOrient, &rhs);

			mesh.pos.x = pivot.x + mesh.pos.x;
			mesh.pos.y = pivot.y + mesh.pos.y;
			mesh.pos.z = pivot.z + mesh.pos.z;

			mesh.viewPos.x = mesh.viewPos.x - Math3D_RotateVec3X(&pivot, (Matrix3x3*)mesh.viewOrient);
			mesh.viewPos.y = mesh.viewPos.y - Math3D_RotateVec3Y(&pivot, (Matrix3x3*)mesh.viewOrient);
			mesh.viewPos.z = mesh.viewPos.z - Math3D_RotateVec3Z(&pivot, (Matrix3x3*)mesh.viewOrient);
		}

		g_modelNodeWalkUnusedScratch0 = 0;
		g_curTextureId = (uint16_t)(-2 - obj->objectSignature);
		g_curTextureDesc = ModelTexture_GetDefaultWhiteTexture();
		g_modelNodeWalkUnusedScratch1 = 0;
		g_curVertNormals = 0;
		g_modelNodeWalkUnusedScratch2 = 0;
		g_curMeshFlags = 0;
		g_curVertexCount = 0;
		saveMeshForBwing = 0;
		meshOrdinal = 0;

		mobj = obj->mobj;
		if (mobj && mobj->spinAngleQ16 && obj->genusId == GENUS_Debris) {
			pivot.x = (float)mobj->renderOffsetX;
			pivot.y = (float)mobj->renderOffsetY;
			pivot.z = (float)mobj->renderOffsetZ;

			mesh.pos.x -= pivot.x;
			mesh.pos.y -= pivot.y;
			mesh.pos.z -= pivot.z;

			mesh.viewPos.x = Math3D_RotateVec3X(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.x;
			mesh.viewPos.y = Math3D_RotateVec3Y(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.y;
			mesh.viewPos.z = Math3D_RotateVec3Z(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.z;

			axisAngle[0] = mobj->spinAxisX;
			axisAngle[1] = mobj->spinAxisY;
			axisAngle[2] = mobj->spinAxisZ;
			axisAngle[3] = (float)((double)mobj->spinAngleQ16 * g_q16AngleToRadians);
			Math3D_BuildAxisAngleMatrix(&rhs, axisAngle);
			Math3D_MulMatrix3x3((Matrix3x3*)mesh.orient, &rhs);
			Math3D_RotateVec3(&mesh.pos, &rhs);
			Math3D_MulMatrix3x3T((Matrix3x3*)mesh.viewOrient, &rhs);

			mesh.pos.x = pivot.x + mesh.pos.x;
			mesh.pos.y = pivot.y + mesh.pos.y;
			mesh.pos.z = pivot.z + mesh.pos.z;

			mesh.viewPos.x = mesh.viewPos.x - Math3D_RotateVec3X(&pivot, (Matrix3x3*)mesh.viewOrient);
			mesh.viewPos.y = mesh.viewPos.y - Math3D_RotateVec3Y(&pivot, (Matrix3x3*)mesh.viewOrient);
			mesh.viewPos.z = mesh.viewPos.z - Math3D_RotateVec3Z(&pivot, (Matrix3x3*)mesh.viewOrient);
		}

		for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
			OptNode* node;
			OptNodeType nodeType;
			int drawMesh;
			MeshDescriptor* descriptor;

			mesh.rotAngle = 0.0f;
			node = model->rootNodes[rootIndex];
			nodeType = node->nodeType;
			if (nodeType != OPT_TEXTURE && nodeType != OPT_TEXTURE_REF) {
				++meshOrdinal;
				mobj = obj->mobj;
				if (mobj && mobj->pCraft) {
					if (mobj->pCraft->componentState[meshOrdinal - 1]) {
						continue;
					}

					if (obj->objectType == OBJ_BWing) {
						int bridgeIndex;

						bridgeIndex = g_bwingBridgeMeshIndexCache;
						if (bridgeIndex == -1) {
							bridgeIndex = ModelMesh_FindBridgeIndex(model);
							g_bwingBridgeMeshIndexCache = bridgeIndex;
						}
						if (bridgeIndex != -1) {
							mobj = obj->mobj;
							if (mobj->pCraft->meshRotation[bridgeIndex]) {
								memcpy(&savedMesh, &mesh, sizeof(savedMesh));
								saveMeshForBwing = 1;
								turretAxisAngle[0] = 0.0f;
								turretAxisAngle[1] = -1.0f;
								turretAxisAngle[2] = 0.0f;
								turretAxisAngle[3] =
									mobj->pCraft->meshRotation[bridgeIndex] * g_meshRotationToRadians;
								Math3D_BuildAxisAngleMatrix(&out, turretAxisAngle);
								Math3D_MulMatrix3x3((Matrix3x3*)mesh.orient, &out);
								Math3D_RotateVec3(&mesh.pos, &out);
								Math3D_MulMatrix3x3T((Matrix3x3*)mesh.viewOrient, &out);
							}
						}
					}

					g_curMeshType = 0;
					if (g_cockpitViewActive && g_players[g_localPlayer].currentSeatIdx) {
						CraftData* playerCraft;
						int meshType;

						playerCraft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
						descriptor = ModelMesh_FindDescriptorNodeRecursive(node, model);
						if (descriptor) {
							meshType = descriptor->meshType;
							g_curMeshType = meshType;
						} else {
							meshType = g_curMeshType;
						}

						if (meshType >= MESH_RotaryGunTurret && meshType <= MESH_RotaryLauncher) {
							mesh.rotAngle = (int16_t)playerCraft->turretAim
												.aimAngleA[g_players[g_localPlayer].currentSeatIdx - 1] *
											g_turretRotationToRadians;
						} else if (meshType == MESH_RotaryBeamSystem) {
							mesh.rotAngle = -(int16_t)playerCraft->turretAim
												 .aimAngleB[g_players[g_localPlayer].currentSeatIdx - 1] *
											g_turretRotationToRadians;
						} else {
							mesh.rotAngle = 0 * g_turretRotationToRadians;
						}
					} else if (obj->objectType == OBJ_Centrifuge) {
						int mechanismIndex;

						mechanismIndex = -1;
						switch (meshOrdinal) {
							case 4:
								mechanismIndex = 0;
								break;
							case 5:
								mechanismIndex = 1;
								break;
							case 13:
								mechanismIndex = 2;
								break;
							default:
								break;
						}
						if (mechanismIndex != -1) {
							mesh.rotAngle = (float)((double)(uint16_t)(int64_t)g_yardContext
														.centrifugeMechanisms[mechanismIndex]
														.meshRotationAccum *
													g_turretRotationToRadians);
						} else {
							mesh.rotAngle =
								obj->mobj->pCraft->meshRotation[meshOrdinal - 1] * g_meshRotationToRadians;
						}
					} else {
						mesh.rotAngle =
							obj->mobj->pCraft->meshRotation[meshOrdinal - 1] * g_meshRotationToRadians;
					}
				}
			}

			++g_curLayerId;
			drawMesh = 1;
			{
				int objectType;
				int collisionObjIdx;

				objectType = (uint16_t)obj->objectType;
				switch (objectType) {
					case OBJ_SmeltingRoom:
					case OBJ_Centrifuge:
					case OBJ_Asteroid03:
						collisionObjIdx =
							g_objectTable[g_players[g_localPlayer].objectIndex].mobj->collisionObjIdx;
						if (collisionObjIdx == 0xFFFF ||
							g_objectTable[collisionObjIdx].objectType != obj->objectType) {
							descriptor = ModelMesh_FindDescriptorNodeRecursive(node, model);
							if (descriptor && descriptor->meshType != MESH_MainHull) {
								drawMesh = 0;
							}
						}
						break;

					default:
						break;
				}
			}

			if (drawMesh) {
				descriptor = ModelMesh_FindDescriptorNodeRecursive(node, model);
				if (obj->objectType == OBJ_Asteroid03 && descriptor) {
					ObjectRecord* playerObj;

					playerObj = &g_objectTable[g_players[g_localPlayer].objectIndex];
					g_approxDist = collide_roughdistance3d(playerObj->world_x - obj->world_x,
														   playerObj->world_y - obj->world_y,
														   playerObj->world_z - obj->world_z);
					switch (descriptor->meshType) {
						case MESH_MainHull:
							if ((unsigned int)g_approxDist < 0x9240u) {
								drawMesh = 0;
							}
							break;

						case MESH_Wing:
							break;

						default:
							if ((unsigned int)g_approxDist > 0x6400u) {
								drawMesh = 0;
							}
							break;
					}
				}
			}

			if (obj->objectType == OBJ_LaserImperialDS) {
				float laserScale;

				g_renderFlags = 64;
				laserScale = -(g_deathStarTunnelLaserRegions[regionIdx].remainingDistance /
							   (float)g_modelTypeTable[OBJ_LaserImperialDS].maxBoundsExtent);
				mesh.viewOrient[0] *= g_sw3dLaserScale;
				mesh.viewOrient[1] *= g_sw3dLaserScale;
				mesh.viewOrient[2] *= g_sw3dLaserScale;
				mesh.viewOrient[3] *= laserScale;
				mesh.viewOrient[4] *= laserScale;
				mesh.viewOrient[5] *= laserScale;
				mesh.viewOrient[6] *= g_sw3dLaserScale;
				mesh.viewOrient[7] *= g_sw3dLaserScale;
				mesh.viewOrient[8] *= g_sw3dLaserScale;
			}

			g_meshRenderFlags = g_renderFlags;
			if (drawMesh) {
				RenderScene_DrawModelNode(model, node, &mesh);
			}
			if (meshOrdinal > 0) {
				g_curTextureId = (uint16_t)-1;
			}
			if (saveMeshForBwing) {
				saveMeshForBwing = 0;
				memcpy(&mesh, &savedMesh, sizeof(mesh));
			}
		}

		result = Memory_UnlockHandle(modelHandle);
	}
	return (int16_t)result;
}

// FUNCTION: XWA 0x4836F0
int RenderScene_DrawObjectModelHardware(ObjectRecord* obj) {
	MobileObject* mobj;
	MemoryHandle modelHandle;
	OptimizedPolyObject* model;
	Matrix3x3 viewMatrix;
	SceneMesh mesh;
	Vec3f pivot;
	Matrix3x3 out;
	float axisAngle[4];
	SceneMesh savedMesh;
	int saveMeshForBwing;
	int rootIndex;
	int meshOrdinal;
	int result;

	mobj = obj->mobj;
	if (mobj) {
		g_nodeSwitchIndex = mobj->nodeSwitchIndex;
	} else {
		g_nodeSwitchIndex = 0;
	}

	modelHandle = g_loadedModels.byObjectType[(uint16_t)obj->objectType];
	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);
	memset(&mesh, 0, sizeof(mesh));

	mesh.pObject = obj;
	mesh.viewPos.x = (float)(obj->world_x - g_players[g_localPlayer].viewState.savedTargetX);
	mesh.viewPos.y = (float)(obj->world_y - g_players[g_localPlayer].viewState.savedTargetY);
	mesh.viewPos.z = (float)(obj->world_z - g_players[g_localPlayer].viewState.savedTargetZ);
	if (g_cockpitViewActive) {
		mesh.viewPos.x = -g_players[g_localPlayer].hardpointWorldX -
						 g_players[g_localPlayer].viewState.cameraPanDeltaX * g_cockpitPanPositionScale;
		mesh.viewPos.y = -g_players[g_localPlayer].hardpointWorldY -
						 g_players[g_localPlayer].viewState.cameraPanDeltaY * g_cockpitPanPositionScale;
		mesh.viewPos.z = -g_players[g_localPlayer].hardpointWorldZ -
						 g_players[g_localPlayer].viewState.cameraPanDeltaZ * g_cockpitPanPositionScale;
	}

	viewMatrix.m[0] = g_viewMtx00;
	viewMatrix.m[1] = g_viewMtx10;
	viewMatrix.m[3] = g_viewMtx01;
	viewMatrix.m[4] = g_viewMtx11;
	viewMatrix.m[2] = g_viewMtx20;
	viewMatrix.m[6] = g_viewMtx02;
	viewMatrix.m[7] = g_viewMtx12;
	viewMatrix.m[5] = g_viewMtx21;
	viewMatrix.m[8] = g_viewMtx22;
	Math3D_RotateVec3(&mesh.viewPos, &viewMatrix);

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
	g_curTextureId = (uint16_t)(-2 - obj->objectSignature);
	g_curMeshFlags = 0;
	g_curTextureDesc = ModelTexture_GetDefaultWhiteTexture();
	g_modelNodeWalkUnusedScratch1 = 0;
	g_curVertNormals = 0;
	g_modelNodeWalkUnusedScratch2 = 0;
	g_curVertexCount = 0;
	meshOrdinal = 0;

	mobj = obj->mobj;
	saveMeshForBwing = 0;
	if (mobj && mobj->spinAngleQ16 && obj->genusId == GENUS_Debris) {
		pivot.x = (float)mobj->renderOffsetX;
		pivot.y = (float)mobj->renderOffsetY;
		pivot.z = (float)mobj->renderOffsetZ;

		mesh.pos.x -= pivot.x;
		mesh.pos.y -= pivot.y;
		mesh.pos.z -= pivot.z;

		mesh.viewPos.x = Math3D_RotateVec3X(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.x;
		mesh.viewPos.y = Math3D_RotateVec3Y(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.y;
		mesh.viewPos.z = Math3D_RotateVec3Z(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.z;

		axisAngle[0] = mobj->spinAxisX;
		axisAngle[1] = mobj->spinAxisY;
		axisAngle[2] = mobj->spinAxisZ;
		axisAngle[3] = (float)((double)mobj->spinAngleQ16 * g_q16AngleToRadians);
		Math3D_BuildAxisAngleMatrix(&out, axisAngle);
		Math3D_MulMatrix3x3((Matrix3x3*)mesh.orient, &out);
		Math3D_RotateVec3(&mesh.pos, &out);
		Math3D_MulMatrix3x3T((Matrix3x3*)mesh.viewOrient, &out);

		mesh.pos.x = pivot.x + mesh.pos.x;
		mesh.pos.y = pivot.y + mesh.pos.y;
		mesh.pos.z = pivot.z + mesh.pos.z;

		mesh.viewPos.x = mesh.viewPos.x - Math3D_RotateVec3X(&pivot, (Matrix3x3*)mesh.viewOrient);
		mesh.viewPos.y = mesh.viewPos.y - Math3D_RotateVec3Y(&pivot, (Matrix3x3*)mesh.viewOrient);
		mesh.viewPos.z = mesh.viewPos.z - Math3D_RotateVec3Z(&pivot, (Matrix3x3*)mesh.viewOrient);
	}

	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* node;
		OptNodeType nodeType;

		mesh.rotAngle = 0.0f;
		node = model->rootNodes[rootIndex];
		nodeType = node->nodeType;
		if (nodeType != OPT_TEXTURE && nodeType != OPT_TEXTURE_REF) {
			++meshOrdinal;
			mobj = obj->mobj;
			if (mobj && mobj->pCraft) {
				if (mobj->pCraft->componentState[meshOrdinal - 1]) {
					continue;
				}

				if (obj->objectType == OBJ_BWing) {
					int bridgeIndex;

					bridgeIndex = g_bwingBridgeMeshIndexCache;
					if (bridgeIndex == -1) {
						bridgeIndex = ModelMesh_FindBridgeIndex(model);
						g_bwingBridgeMeshIndexCache = bridgeIndex;
					}
					if (bridgeIndex != -1) {
						mobj = obj->mobj;
						if (mobj->pCraft->meshRotation[bridgeIndex]) {
							memcpy(&savedMesh, &mesh, sizeof(savedMesh));
							saveMeshForBwing = 1;
							axisAngle[0] = 0.0f;
							axisAngle[1] = -1.0f;
							axisAngle[2] = 0.0f;
							axisAngle[3] = mobj->pCraft->meshRotation[bridgeIndex] * g_meshRotationToRadians;
							Math3D_BuildAxisAngleMatrix(&out, axisAngle);
							Math3D_MulMatrix3x3((Matrix3x3*)mesh.orient, &out);
							Math3D_RotateVec3(&mesh.pos, &out);
							Math3D_MulMatrix3x3T((Matrix3x3*)mesh.viewOrient, &out);
						}
					}
				}

				g_curMeshType = 0;
				if (g_cockpitViewActive && g_players[g_localPlayer].currentSeatIdx) {
					CraftData* playerCraft;
					int meshType;
					MeshDescriptor* descriptor;

					playerCraft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
					descriptor = ModelMesh_FindDescriptorNodeRecursive(node, model);
					if (descriptor) {
						meshType = descriptor->meshType;
						g_curMeshType = meshType;
					} else {
						meshType = g_curMeshType;
					}

					if (meshType >= MESH_RotaryGunTurret && meshType <= MESH_RotaryLauncher) {
						mesh.rotAngle = (int16_t)playerCraft->turretAim
											.aimAngleA[g_players[g_localPlayer].currentSeatIdx - 1] *
										g_turretRotationToRadians;
					} else if (meshType == MESH_RotaryBeamSystem) {
						mesh.rotAngle = -(int16_t)playerCraft->turretAim
											 .aimAngleB[g_players[g_localPlayer].currentSeatIdx - 1] *
										g_turretRotationToRadians;
					} else {
						mesh.rotAngle = 0 * g_turretRotationToRadians;
					}
				} else {
					mesh.rotAngle =
						obj->mobj->pCraft->meshRotation[meshOrdinal - 1] * g_meshRotationToRadians;
				}
			}
		}

		++g_curLayerId;
		RenderScene_DrawModelNodeHardware(model, node, &mesh);
		if (saveMeshForBwing) {
			saveMeshForBwing = 0;
			memcpy(&mesh, &savedMesh, sizeof(mesh));
		}
	}

	result = Memory_UnlockHandle(modelHandle);
	return result;
}

// FUNCTION: XWA 0x481AD0
void RenderScene_DrawNoAssetSourceModel(ObjectRecord* obj, int nodeSwitchIndex) {
	int objectType;
	MobileObject* mobj;
	MemoryHandle modelHandle;
	OptimizedPolyObject* model;
	SceneMesh mesh;
	Vec3f pivot;
	Matrix3x3 out;
	float axisAngle[4];
	int rootIndex;

	objectType = (uint16_t)obj->objectType;
	if ((uint16_t)objectType == OBJ_NoAsset_222) {
		mobj = obj->mobj;
		if (mobj) {
			objectType = mobj->sourceObjectType;
		}
	}

	mobj = obj->mobj;
	if (mobj) {
		g_nodeSwitchIndex = mobj->nodeSwitchIndex;
	} else {
		g_nodeSwitchIndex = 0;
	}

	modelHandle = g_loadedModels.byObjectType[(uint16_t)objectType];
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
	g_modelNodeWalkUnusedScratch1 = 0;
	g_curVertNormals = 0;
	g_modelNodeWalkUnusedScratch2 = 0;
	g_curMeshFlags = 0;
	g_curVertexCount = 0;

	mobj = obj->mobj;
	if (mobj && mobj->spinAngleQ16 && obj->genusId == GENUS_Debris) {
		pivot.x = (float)mobj->renderOffsetX;
		pivot.y = (float)mobj->renderOffsetY;
		pivot.z = (float)mobj->renderOffsetZ;

		mesh.pos.x -= pivot.x;
		mesh.pos.y -= pivot.y;
		mesh.pos.z -= pivot.z;

		mesh.viewPos.x = Math3D_RotateVec3X(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.x;
		mesh.viewPos.y = Math3D_RotateVec3Y(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.y;
		mesh.viewPos.z = Math3D_RotateVec3Z(&pivot, (Matrix3x3*)mesh.viewOrient) + mesh.viewPos.z;

		axisAngle[0] = mobj->spinAxisX;
		axisAngle[1] = mobj->spinAxisY;
		axisAngle[2] = mobj->spinAxisZ;
		axisAngle[3] = (float)((double)mobj->spinAngleQ16 * 0.00009587379924285257);
		Math3D_BuildAxisAngleMatrix(&out, axisAngle);
		Math3D_MulMatrix3x3((Matrix3x3*)mesh.orient, &out);
		Math3D_RotateVec3(&mesh.pos, &out);
		Math3D_MulMatrix3x3T((Matrix3x3*)mesh.viewOrient, &out);

		mesh.pos.x = pivot.x + mesh.pos.x;
		mesh.pos.y = pivot.y + mesh.pos.y;
		mesh.pos.z = pivot.z + mesh.pos.z;

		mesh.viewPos.x = mesh.viewPos.x - Math3D_RotateVec3X(&pivot, (Matrix3x3*)mesh.viewOrient);
		mesh.viewPos.y = mesh.viewPos.y - Math3D_RotateVec3Y(&pivot, (Matrix3x3*)mesh.viewOrient);
		mesh.viewPos.z = mesh.viewPos.z - Math3D_RotateVec3Z(&pivot, (Matrix3x3*)mesh.viewOrient);
	}

	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* node;
		OptNodeType nodeType;

		node = model->rootNodes[rootIndex];
		nodeType = node->nodeType;
		if (nodeType == OPT_TEXTURE || nodeType == OPT_TEXTURE_REF) {
			++g_curLayerId;
			RenderScene_DrawModelNode(model, node, &mesh);
			++nodeSwitchIndex;
		} else if (rootIndex == nodeSwitchIndex) {
			++g_curLayerId;
			RenderScene_DrawModelNode(model, node, &mesh);
		}
	}

	Memory_UnlockHandle(modelHandle);
}
