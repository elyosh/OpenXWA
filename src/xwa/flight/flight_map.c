#include "xwa/flight/flight_map.h"

#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight.h"

#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/assets/flight_model.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"
#include "xwa/flight/object/damage.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_light.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/math/fixed.h"
#include "xwa/math/scalar.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"

// GLOBAL: XWA 0x749AB8
int g_unusedFlightMapRenderFlag;

const uint16_t g_flightMapIconByObjectType[OBJ_NoAsset_222 + 1] = {
	0x0000u, 0x0064u, 0x00c8u, 0x012cu, 0x0190u, 0x04b0u, 0x0514u, 0x0384u, 0x01f4u, 0x044cu, 0x1194u,
	0x4010u, 0x15e0u, 0x41a0u, 0x4330u, 0x36b0u, 0x23f0u, 0x3b60u, 0x02bcu, 0x0258u, 0x0578u, 0x0320u,
	0x03e8u, 0x2328u, 0x3714u, 0x1c20u, 0x3ee4u, 0x1af4u, 0x26acu, 0x1edcu, 0x0000u, 0x3bc4u, 0x3c28u,
	0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x4074u,
	0x2af8u, 0x0ed8u, 0x189cu, 0x0708u, 0x076cu, 0x0000u, 0x3afcu, 0x0834u, 0x08fcu, 0x24b8u, 0x0898u,
	0x3facu, 0x39d0u, 0x1900u, 0x1450u, 0x1450u, 0x1a2cu, 0x0fa0u, 0x2454u, 0x1324u, 0x0a28u, 0x1f40u,
	0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x2c24u, 0x2ee0u, 0x3138u, 0x2fa8u, 0x2e7cu, 0x300cu,
	0x2f44u, 0x2cecu, 0x319cu, 0x2904u, 0x3520u, 0x2d50u, 0x2bc0u, 0x3070u, 0x2e18u, 0x0000u, 0x0000u,
	0x0000u, 0x0000u, 0x0000u, 0x0b54u, 0x29ccu, 0x1838u, 0x30d4u, 0x413cu, 0x3db8u, 0x2008u, 0x206cu,
	0x42ccu, 0x0af0u, 0x0bb8u, 0x0a8cu, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u,
	0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x3f48u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u,
	0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x05dcu, 0x170cu, 0x0c1cu, 0x1770u, 0x1a90u, 0x2a30u,
	0x3e80u, 0x07d0u, 0x06a4u, 0x33f4u, 0x12c0u, 0x32c8u, 0x3390u, 0x3264u, 0x332cu, 0x413cu, 0x11f8u,
	0x2710u, 0x238cu, 0x2198u, 0x21fcu, 0x1068u, 0x28a0u, 0x3390u, 0x3264u, 0x0000u, 0x0000u, 0x1c84u,
	0x1ce8u, 0x1d4cu, 0x1db0u, 0x1e14u, 0x1e78u, 0x251cu, 0x0960u, 0x3a98u, 0x37dcu, 0x0c80u, 0x0ce4u,
	0x0d48u, 0x0640u, 0x3a34u, 0x2b5cu, 0x3c8cu, 0x3cf0u, 0x3d54u, 0x2a94u, 0x2968u, 0x2260u, 0x25e4u,
	0x34bcu, 0x3778u, 0x1004u, 0x22c4u, 0x09c4u, 0x1bbcu, 0x10ccu, 0x1bbcu, 0x0000u, 0x0000u, 0x0000u,
	0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x125cu, 0x1130u, 0x125cu, 0x2648u, 0x0e74u, 0x0dacu,
	0x0e10u, 0x0f3cu, 0x0f3cu, 0x14b4u, 0x1518u, 0x1b58u, 0x1b58u, 0x1b58u, 0x4394u, 0x43f8u, 0x13ecu,
	0x364cu, 0x396cu, 0x3840u, 0x38a4u, 0x3200u, 0x3458u, 0x2774u, 0x27d8u, 0x283cu, 0x283cu, 0x14b4u,
	0x1518u, 0x157cu, 0x0000u,
};

// FUNCTION: XWA 0x49F540
void RenderList_Reset(void) {
	g_renderObjectListCount = 0;
	g_renderListHead = NULL;
}

// FUNCTION: XWA 0x49F460
void RenderList_QueueObject(int objectIdx, int sortDepth, int viewX, int viewY, int viewZ, int cullFlags,
							int projectedRadius) {
	if (g_renderObjectListCount >= 1664) {
		return;
	}

	g_renderObjectListEntries[g_renderObjectListCount].sortDepth = sortDepth;
	g_renderObjectListEntries[g_renderObjectListCount].objectIdx = objectIdx;
	g_renderObjectListEntries[g_renderObjectListCount].next = g_renderListHead;
	g_renderObjectListEntries[g_renderObjectListCount].viewX = viewX;
	g_renderObjectListEntries[g_renderObjectListCount].viewY = viewY;
	g_renderObjectListEntries[g_renderObjectListCount].viewZ = viewZ;
	g_renderObjectListEntries[g_renderObjectListCount].cullFlags = cullFlags;
	g_renderObjectListEntries[g_renderObjectListCount].projectedRadius = projectedRadius;
	g_renderListHead = &g_renderObjectListEntries[g_renderObjectListCount];
	++g_renderObjectListCount;
}

// FUNCTION: XWA 0x49F640
void RenderList_SortDepthDescending(void) {
	int objectCount;
	int runLength;

	objectCount = g_renderObjectListCount;
	runLength = 1;
	if (objectCount <= runLength) {
		return;
	}

	do {
		RenderObjectListEntry* rightRun;
		RenderObjectListEntry* previous;
		RenderObjectListEntry* leftRun;
		RenderObjectListEntry* leftTail;
		int processedCount;

		rightRun = g_renderListHead;
		previous = NULL;
		leftRun = rightRun;
		processedCount = 0;
		if (objectCount > processedCount) {
			for (;;) {
				int leftCount;
				int rightCount;

				leftCount = 0;
				while (runLength > 0) {
					if (rightRun == NULL) {
						break;
					}
					++leftCount;
					leftTail = rightRun;
					rightRun = rightRun->next;
					if (leftCount >= runLength) {
						break;
					}
				}
				if (rightRun == NULL) {
					break;
				}

				rightCount = 0;
				if (runLength > 0) {
					while (rightRun != NULL) {
						int rightDepth;

						rightDepth = rightRun->sortDepth;
						if (leftRun->sortDepth >= rightDepth) {
							do {
								previous = leftRun;
								leftRun = leftRun->next;
							} while (previous != leftTail && leftRun->sortDepth >= rightDepth);
						}

						if (previous == leftTail) {
							break;
						}
						leftTail->next = rightRun->next;
						if (previous != NULL) {
							previous->next = rightRun;
							previous = rightRun;
							rightRun->next = leftRun;
						} else {
							g_renderListHead = rightRun;
							rightRun->next = leftRun;
							previous = g_renderListHead;
						}

						rightRun = leftTail->next;
						if (rightRun == NULL) {
							break;
						}
						++rightCount;
						if (rightCount >= runLength) {
							break;
						}
					}
				}

				if (previous == leftTail) {
					while (rightCount < runLength) {
						if (rightRun == NULL) {
							break;
						}
						++rightCount;
						leftTail = rightRun;
						rightRun = rightRun->next;
					}
				}

				leftRun = rightRun;
				previous = leftTail;
				if (rightRun == NULL) {
					break;
				}
				processedCount += runLength * 2;
				if (processedCount >= g_renderObjectListCount) {
					break;
				}
			}
		}

		objectCount = g_renderObjectListCount;
		runLength += runLength;
	} while (runLength < objectCount);
}

// FUNCTION: XWA 0x49F730
void RenderList_SortDepthAscending(void) {
	int objectCount;
	int runLength;

	objectCount = g_renderObjectListCount;
	runLength = 1;
	if (objectCount <= runLength) {
		return;
	}

	do {
		RenderObjectListEntry* rightRun;
		RenderObjectListEntry* previous;
		RenderObjectListEntry* leftRun;
		RenderObjectListEntry* leftTail;
		int processedCount;

		rightRun = g_renderListHead;
		previous = NULL;
		leftRun = rightRun;
		processedCount = 0;
		if (objectCount > processedCount) {
			for (;;) {
				int leftCount;
				int rightCount;

				leftCount = 0;
				while (runLength > 0) {
					if (rightRun == NULL) {
						break;
					}
					++leftCount;
					leftTail = rightRun;
					rightRun = rightRun->next;
					if (leftCount >= runLength) {
						break;
					}
				}
				if (rightRun == NULL) {
					break;
				}

				rightCount = 0;
				if (runLength > 0) {
					while (rightRun != NULL) {
						int rightDepth;

						rightDepth = rightRun->sortDepth;
						if (leftRun->sortDepth <= rightDepth) {
							do {
								previous = leftRun;
								leftRun = leftRun->next;
							} while (previous != leftTail && leftRun->sortDepth <= rightDepth);
						}

						if (previous == leftTail) {
							break;
						}
						leftTail->next = rightRun->next;
						if (previous != NULL) {
							previous->next = rightRun;
							previous = rightRun;
							rightRun->next = leftRun;
						} else {
							g_renderListHead = rightRun;
							rightRun->next = leftRun;
							previous = g_renderListHead;
						}

						rightRun = leftTail->next;
						if (rightRun == NULL) {
							break;
						}
						++rightCount;
						if (rightCount >= runLength) {
							break;
						}
					}
				}

				if (previous == leftTail) {
					while (rightCount < runLength) {
						if (rightRun == NULL) {
							break;
						}
						++rightCount;
						leftTail = rightRun;
						rightRun = rightRun->next;
					}
				}

				leftRun = rightRun;
				previous = leftTail;
				if (rightRun == NULL) {
					break;
				}
				processedCount += runLength * 2;
				if (processedCount >= g_renderObjectListCount) {
					break;
				}
			}
		}

		objectCount = g_renderObjectListCount;
		runLength += runLength;
	} while (runLength < objectCount);
}

// FUNCTION: XWA 0x49F550
int RenderList_ProjectObjectBoundsForCulling(int objectIdx, unsigned int boundsRadius, int playerIdx) {
	int deltaY;
	int farZ;
	int cullRadius;
	int absViewCoord;

	g_camRelWorldX = g_objectTable[objectIdx].world_x - g_players[playerIdx].viewState.savedTargetX;
	g_camRelWorldZ = g_objectTable[objectIdx].world_z - g_players[playerIdx].viewState.savedTargetZ;
	deltaY = g_objectTable[objectIdx].world_y - g_players[playerIdx].viewState.savedTargetY;
	g_camRelWorldY = deltaY;

	farZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, deltaY, g_camRelWorldZ);
	viewZ = farZ;
	cullRadius = (int)boundsRadius;
	farZ += cullRadius;
	if (farZ < 0) {
		return 0;
	}

	if ((unsigned int)(farZ >> 4) > (unsigned int)cullRadius) {
		cullRadius = farZ >> 4;
	}

	viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	absViewCoord = viewX;
	if (absViewCoord < 0) {
		absViewCoord = -absViewCoord;
	}
	if (absViewCoord - cullRadius > farZ) {
		return 0;
	}

	viewY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	absViewCoord = viewY;
	if (absViewCoord < 0) {
		absViewCoord = -absViewCoord;
	}
	return absViewCoord - cullRadius <= farZ;
}

// FUNCTION: XWA 0x49EE90
void FlightMap_UpdateCamera(int playerIdx) {
	int cameraFocusObjIdx;
	uint8_t mapCameraState;

	mapCameraState = g_players[playerIdx].mapCameraState;

	if (mapCameraState > 1u) {
		cameraFocusObjIdx = g_players[playerIdx].viewState.cameraFocusObjIdx;
		if (cameraFocusObjIdx != 0xffff) {
			g_players[playerIdx].viewState.viewRoll = g_objectTable[cameraFocusObjIdx].roll;
			g_players[playerIdx].viewState.viewPitch =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].pitch;
			g_players[playerIdx].viewState.viewYaw =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].yaw;
			Mission_ResolveObjectOrMissionPointWorldLoc(
				(unsigned int)g_players[playerIdx].viewState.cameraFocusObjIdx, 0, 0, 0);
			g_players[playerIdx].viewState.savedTargetX = worldlocx;
			g_players[playerIdx].viewState.savedTargetY = worldlocy;
			g_players[playerIdx].viewState.savedTargetZ = worldlocz;

			if (!g_flightSimSideEffectsSuppressed) {
				{
					int16_t currentAim;
					int step;

					currentAim = (int16_t)g_players[playerIdx].viewState.hudAimY;
					step = -((int)currentAim / 2);
					if (step > 0) {
						if (step > 0x1000) {
							step = 0x1000;
						}
						if (step < 0x10) {
							step = 0x10;
						}
						if (step > -(int)currentAim) {
							step = -(int)currentAim;
						}
					} else {
						if (step < -0x1000) {
							step = -0x1000;
						}
						if (step > -0x10) {
							step = -0x10;
						}
						if (step < -(int)currentAim) {
							step = -(int)currentAim;
						}
					}
					g_players[playerIdx].viewState.hudAimY = (uint16_t)(currentAim + step);
				}

				if ((g_players[playerIdx].mapCameraState & 0x80u) != 0) {
					int16_t currentAim;
					int targetDelta;
					int step;

					g_players[playerIdx].viewState.viewRoll = 0;
					g_players[playerIdx].viewState.viewPitch = 0x4000;
					g_players[playerIdx].viewState.viewYaw = 0;

					currentAim = (int16_t)g_players[playerIdx].viewState.hudAimX;
					targetDelta = -0x4000 - (int)currentAim;
					step = targetDelta / 2;
					if (step > 0) {
						if (step > 0x1000) {
							step = 0x1000;
						}
						if (step < 0x10) {
							step = 0x10;
						}
						if (step > targetDelta) {
							step = targetDelta;
						}
					} else {
						if (step < -0x1000) {
							step = -0x1000;
						}
						if (step > -0x10) {
							step = -0x10;
						}
						if (step < targetDelta) {
							step = targetDelta;
						}
					}
					g_players[playerIdx].viewState.hudAimX = (uint16_t)(currentAim + step);
				} else {
					int16_t currentAim;
					int step;

					currentAim = (int16_t)g_players[playerIdx].viewState.hudAimX;
					step = -((int)currentAim / 2);
					if (step > 0) {
						if (step > 0x1000) {
							step = 0x1000;
						}
						if (step < 0x10) {
							step = 0x10;
						}
						if (step > -(int)currentAim) {
							step = -(int)currentAim;
						}
					} else {
						if (step < -0x1000) {
							step = -0x1000;
						}
						if (step > -0x10) {
							step = -0x10;
						}
						if (step < -(int)currentAim) {
							step = -(int)currentAim;
						}
					}
					g_players[playerIdx].viewState.hudAimX = (uint16_t)(currentAim + step);
				}
			}
		} else {
			g_players[playerIdx].viewState.viewRoll = 0;
			g_players[playerIdx].viewState.viewPitch = 0x4000;
			g_players[playerIdx].viewState.viewYaw = 0;
			g_players[playerIdx].viewState.hudAimY = 0;
			g_players[playerIdx].viewState.hudAimX =
				(uint16_t)(int16_t)(-0x4000 * (int)(g_players[playerIdx].mapCameraState & 0x7fu) / 127);
		}

		FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
								(int16_t)g_players[playerIdx].viewState.viewPitch,
								(int16_t)g_players[playerIdx].viewState.viewYaw, 0,
								(int16_t)g_players[playerIdx].viewState.hudAimX,
								(int16_t)g_players[playerIdx].viewState.hudAimY, NULL, -1);
		if (g_players[playerIdx].viewState.cameraFocusObjIdx != 0xffff) {
			g_players[playerIdx].viewState.savedTargetX -=
				Xwa_Q15MulReuseFirstSlot(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_X);
			g_players[playerIdx].viewState.savedTargetY -=
				Xwa_Q15MulReuseFirstSlot(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_Y);
			g_players[playerIdx].viewState.savedTargetZ -=
				Xwa_Q15MulReuseFirstSlot(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_Z);
		}
		return;
	}

	cameraFocusObjIdx = g_players[playerIdx].viewState.cameraFocusObjIdx;
	if (cameraFocusObjIdx != 0xffff) {
		g_players[playerIdx].viewState.viewRoll = g_objectTable[cameraFocusObjIdx].roll;
		g_players[playerIdx].viewState.viewPitch =
			g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].pitch;
		g_players[playerIdx].viewState.viewYaw =
			g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].yaw;
		Mission_ResolveObjectOrMissionPointWorldLoc(
			(unsigned int)g_players[playerIdx].viewState.cameraFocusObjIdx, 0, 0, 0);
		g_players[playerIdx].viewState.savedTargetX = worldlocx;
		g_players[playerIdx].viewState.savedTargetY = worldlocy;
		g_players[playerIdx].viewState.savedTargetZ = worldlocz;
		FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
								(int16_t)g_players[playerIdx].viewState.viewPitch,
								(int16_t)g_players[playerIdx].viewState.viewYaw, 0,
								(int16_t)g_players[playerIdx].viewState.hudAimX,
								(int16_t)g_players[playerIdx].viewState.hudAimY, NULL, -1);
	} else {
		if (g_players[playerIdx].viewState.hudAimY != 0 || g_players[playerIdx].viewState.hudAimX != 0) {
			FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
									(int16_t)g_players[playerIdx].viewState.viewPitch,
									(int16_t)g_players[playerIdx].viewState.viewYaw, 0,
									(int16_t)g_players[playerIdx].viewState.hudAimX,
									(int16_t)g_players[playerIdx].viewState.hudAimY, NULL, -1);
			trig2_ctop(g_camMatR2_X, g_camMatR2_Y, g_camMatR2_Z);
			g_players[playerIdx].viewState.hudAimY = 0;
			g_players[playerIdx].viewState.hudAimX = 0;
			g_players[playerIdx].viewState.viewPitch = targetPitch;
			g_players[playerIdx].viewState.viewYaw = trig2_xyangle;
		}
		FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
								(int16_t)g_players[playerIdx].viewState.viewPitch,
								(int16_t)g_players[playerIdx].viewState.viewYaw, 0,
								(int16_t)g_players[playerIdx].viewState.hudAimX,
								(int16_t)g_players[playerIdx].viewState.hudAimY, NULL, -1);
	}

	if (g_players[playerIdx].viewState.cameraFocusObjIdx != 0xffff) {
		g_players[playerIdx].viewState.savedTargetX -=
			Xwa_Q15MulReuseFirstSlot(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_X);
		g_players[playerIdx].viewState.savedTargetY -=
			Xwa_Q15MulReuseFirstSlot(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_Y);
		g_players[playerIdx].viewState.savedTargetZ -=
			Xwa_Q15MulReuseFirstSlot(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_Z);
	}

	if (g_players[playerIdx].viewState.aimTargetIdx != 0xffff) {
		trig2_ctop(g_objectTable[g_players[playerIdx].viewState.aimTargetIdx].world_x -
					   g_players[playerIdx].viewState.savedTargetX,
				   g_objectTable[g_players[playerIdx].viewState.aimTargetIdx].world_y -
					   g_players[playerIdx].viewState.savedTargetY,
				   g_objectTable[g_players[playerIdx].viewState.aimTargetIdx].world_z -
					   g_players[playerIdx].viewState.savedTargetZ);
		g_players[playerIdx].viewState.viewRoll = 0;
		g_players[playerIdx].viewState.viewPitch = targetPitch;
		g_players[playerIdx].viewState.viewYaw = trig2_xyangle;
		FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
								(int16_t)g_players[playerIdx].viewState.viewPitch,
								(int16_t)g_players[playerIdx].viewState.viewYaw, 0, 0, 0, NULL, -1);
	}
}

// FUNCTION: XWA 0x49F820
void FlightMap_DrawObjectPass(int pass) {
	RenderObjectListEntry* savedHead;
	RenderObjectListEntry* entry;

	g_sceneBillboardQueueCount = 0;
	savedHead = g_renderListHead;
	RenderScene_Initialize(1);
	entry = g_renderListHead;
	while (entry != NULL) {
		unsigned int objectIdx;
		int drawThisPass;

		objectIdx = entry->objectIdx;
		drawThisPass = 0;
		if (pass) {
			if (g_objectTable[objectIdx].world_z >= -65536) {
				drawThisPass = 1;
			}
		} else if (g_objectTable[objectIdx].world_z < -65536) {
			drawThisPass = 1;
		}

		if (drawThisPass) {
			g_unusedFlightMapRenderFlag = 0;
			g_camRelWorldX =
				g_objectTable[objectIdx].world_x - g_players[g_localPlayer].viewState.savedTargetX;
			g_camRelWorldY =
				g_objectTable[objectIdx].world_y - g_players[g_localPlayer].viewState.savedTargetY;
			g_camRelWorldZ =
				g_objectTable[objectIdx].world_z - g_players[g_localPlayer].viewState.savedTargetZ;
			viewZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
			viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
			viewY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);

			switch (g_objectTable[objectIdx].genusId) {
				case GENUS_Fighter:
				case GENUS_Transport:
				case GENUS_Utility:
				case GENUS_Freighter:
				case GENUS_Starship:
				case GENUS_Platform:
				case GENUS_SatelliteBuoy:
				case GENUS_LargeScenery:
				case GENUS_Container:
				case GENUS_PilotDroid:
				case GENUS_WeaponEmplacement:
				case GENUS_Rubble:
					if ((g_renderListHead->sortDepth >> 4) >
						g_modelTypeTable[(uint16_t)g_objectTable[objectIdx].objectType].maxBoundsExtent) {
						FlightMap_DrawObjectIconAtViewPos(objectIdx, viewX, viewY, viewZ);
					} else {
						g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
						FVIEW_SetObjectTransform(g_objectTable[objectIdx].roll,
												 g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
												 g_objectTable[objectIdx].angleD, &g_objectTable[objectIdx]);
						FlightLight_SetupObjectLighting(&g_objectTable[objectIdx]);
						Damage_QueueCraftBillboards(objectIdx);
						RenderScene_DrawObjectModel(&g_objectTable[objectIdx]);
						g_objectPointLightCount = 0;
					}
					if (objectIdx != g_players[g_localPlayer].viewState.cameraFocusObjIdx &&
						g_objectTable[objectIdx].playerOwnerIdx != -1) {
						MobileObject* mobj;
						CraftData* craft;
						int playerIff;
						uint8_t colorIndex;

						mobj = g_objectTable[objectIdx].mobj;
						switch ((uint8_t)mobj->iff) {
							case 0:
								colorIndex = 63;
								break;
							case 1:
							case 4:
								colorIndex = 55;
								break;
							case 2:
								colorIndex = 51;
								break;
							default:
								colorIndex = 59;
								break;
						}
						craft = mobj->pCraft;
						if (g_flightLocatePlayersEnabled ||
							(playerIff = (uint16_t)g_players[g_localPlayer].playerIff,
							 (int8_t)craft->iffVisibility[playerIff] > 0) ||
							!Object_IsHostileToTeam(objectIdx, playerIff)) {
							if ((uint8_t)Object_HasActiveDecoyBeam(objectIdx) == 0 &&
								(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx != objectIdx) {
								Targeting_DrawObjectBox(objectIdx, 0xffffu, colorIndex);
							}
						}
					}
					break;
				case GENUS_PlayerProjectile:
				case GENUS_NpcProjectile:
					FVIEW_SetObjectTransform(g_objectTable[objectIdx].roll, g_objectTable[objectIdx].pitch,
											 g_objectTable[objectIdx].yaw, g_objectTable[objectIdx].angleD,
											 &g_objectTable[objectIdx]);
					RenderBillboard_DrawRollAlignedObjectModel(objectIdx);
					break;
				case GENUS_Mine:
					if ((g_renderListHead->sortDepth >> 4) >
						g_modelTypeTable[(uint16_t)g_objectTable[objectIdx].objectType].maxBoundsExtent) {
						FlightMap_DrawObjectIconAtViewPos(objectIdx, viewX, viewY, viewZ);
						break;
					}
					g_sceneBillboardQueueCount = 0;
					if (objectIdx >= g_regionMainObjectSlotStart && objectIdx < g_regionMainObjectSlotEnd) {
						FVIEW_SetObjectTransform(g_objectTable[objectIdx].roll,
												 g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
												 g_objectTable[objectIdx].angleD, &g_objectTable[objectIdx]);
						SceneBillboard_QueueObjectTextured(objectIdx);
					} else {
						FVIEW_SetObjectTransform(g_objectTable[objectIdx].roll,
												 g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
												 g_objectTable[objectIdx].angleD, NULL);
						RenderNonCraftSceneObject(objectIdx);
					}
					SceneBillboard_RenderQueuedTextured(0);
					g_sceneBillboardQueueCount = 0;
					break;
				case GENUS_Asteroid:
				case GENUS_Debris:
				case GENUS_Explosion:
				case GENUS_DeathStarTunnelSegment:
					g_sceneBillboardQueueCount = 0;
					if (objectIdx >= g_regionMainObjectSlotStart && objectIdx < g_regionMainObjectSlotEnd) {
						FVIEW_SetObjectTransform(g_objectTable[objectIdx].roll,
												 g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
												 g_objectTable[objectIdx].angleD, &g_objectTable[objectIdx]);
						SceneBillboard_QueueObjectTextured(objectIdx);
					} else {
						FVIEW_SetObjectTransform(g_objectTable[objectIdx].roll,
												 g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
												 g_objectTable[objectIdx].angleD, NULL);
						RenderNonCraftSceneObject(objectIdx);
					}
					SceneBillboard_RenderQueuedTextured(0);
					g_sceneBillboardQueueCount = 0;
					break;
				default:
					break;
			}

			if (objectIdx == g_players[g_localPlayer].viewState.cameraFocusObjIdx) {
				Targeting_DrawObjectBox(objectIdx, 0xffffu, 0x2fu);
			} else if (objectIdx == (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx) {
				Targeting_DrawObjectBox(objectIdx, 0xffffu, 0x3bu);
			}

			FlightSurface_Lock();
			{
				int genusId;

				genusId = g_objectTable[objectIdx].genusId;
				if (genusId != GENUS_Explosion && genusId != GENUS_Debris) {
					int baseViewZ;
					int baseViewX;
					int baseViewY;

					baseViewZ = viewZ;
					baseViewX = viewX;
					baseViewY = viewY;
					if (viewZ > 0) {
						int screenX;
						int screenY;
						int clippedScreenX;
						int clippedScreenY;
						int iff;
						unsigned int colorIndex;
						int shouldDrawMapText;

						screenX = TRANSFM2_ProjectScreenX(viewX, baseViewZ);
						screenY = TRANSFM2_ProjectScreenY(baseViewY, baseViewZ);
						clippedScreenX = g_flightClipLeft + screenX;
						clippedScreenY = g_flightClipTop + screenY;

						if (objectIdx == (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx) {
							unsigned int objectOrWaypointIdx;

							objectOrWaypointIdx = 0xffffu;
							if (objectIdx >= g_activeRegionObjectSlotStart &&
								objectIdx < g_activeRegionCraftObjectSlotEnd) {
								objectOrWaypointIdx =
									pai_GetEffectiveAIController(g_objectTable[objectIdx].mobj->pCraft)
										->targetObjIdx;
							} else {
								MobileObject* mobj;

								mobj = g_objectTable[objectIdx].mobj;
								if (mobj != NULL && mobj->pCraft != NULL) {
									objectOrWaypointIdx = mobj->pCraft->modelIndex;
								}
							}
							if (objectOrWaypointIdx != 0xffffu) {
								int targetViewZ;

								Mission_ResolveObjectOrMissionPointWorldLoc(
									objectOrWaypointIdx, g_objectTable[objectIdx].flightGroupIdx,
									g_objectTable[objectIdx].regionIdx, 0);
								worldlocx -= g_players[g_localPlayer].viewState.savedTargetX;
								worldlocy -= g_players[g_localPlayer].viewState.savedTargetY;
								worldlocz -= g_players[g_localPlayer].viewState.savedTargetZ;
								viewX = TRANSFM2_CamMatDotRow0(worldlocx, worldlocy, worldlocz);
								viewY = TRANSFM2_CamMatDotRow1(worldlocx, worldlocy, worldlocz);
								targetViewZ = TRANSFM2_CamMatDotRow2(worldlocx, worldlocy, worldlocz);
								viewZ = targetViewZ;
								if (targetViewZ <= 0) {
									TRANSFM2_clipobjecteyez(baseViewX, baseViewY, baseViewZ);
									targetViewZ = viewZ;
								}
								g_flightDrawLineFn(g_flightClipLeft + TRANSFM2_ProjectScreenX(viewX, viewZ),
												   g_flightClipTop +
													   TRANSFM2_ProjectScreenY(viewY, targetViewZ),
												   clippedScreenX, clippedScreenY, 0x36u);
							}
						}

						if (g_objectTable[objectIdx].mobj != NULL) {
							iff = (uint8_t)g_objectTable[objectIdx].mobj->iff;
						} else {
							iff = g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.iff;
						}
						switch (iff) {
							case 0:
								colorIndex = 63;
								break;
							case 1:
							case 4:
								colorIndex = 55;
								break;
							case 2:
								colorIndex = 51;
								break;
							case 3:
								colorIndex = 59;
								break;
							case 5:
								colorIndex = 86;
								break;
							default:
								colorIndex = 86;
								break;
						}
						FlightText_SetBackgroundColor(colorIndex);
						FlightText_SetFontTier(0);

						shouldDrawMapText = 0;
						if (objectIdx < g_activeRegionCraftObjectSlotEnd ||
							g_objectTable[objectIdx].mobj == NULL ||
							g_objectTable[objectIdx].mobj->pCraft != NULL) {
							shouldDrawMapText = 1;
						}
						if (shouldDrawMapText) {
							int gridViewX;
							int gridViewY;
							int gridViewZ;
							int gridScreenX;
							int gridScreenY;

							viewX = TRANSFM2_CamMatDotRow0(
								g_camRelWorldX, g_camRelWorldY,
								-65536 - g_players[g_localPlayer].viewState.savedTargetZ);
							viewY = TRANSFM2_CamMatDotRow1(
								g_camRelWorldX, g_camRelWorldY,
								-65536 - g_players[g_localPlayer].viewState.savedTargetZ);
							gridViewZ = TRANSFM2_CamMatDotRow2(
								g_camRelWorldX, g_camRelWorldY,
								-65536 - g_players[g_localPlayer].viewState.savedTargetZ);
							viewZ = gridViewZ;
							if (gridViewZ <= 0) {
								TRANSFM2_clipobjecteyez(baseViewX, baseViewY, baseViewZ);
								gridViewZ = viewZ;
							}
							gridViewX = viewX;
							gridViewY = viewY;
							gridScreenX = g_flightClipLeft + TRANSFM2_ProjectScreenX(viewX, gridViewZ);
							gridScreenY = g_flightClipTop + TRANSFM2_ProjectScreenY(viewY, viewZ);
							g_flightDrawLineFn(gridScreenX, gridScreenY, clippedScreenX, clippedScreenY,
											   g_flightTextBgColor);

							if (g_objectTable[objectIdx].mobj != NULL &&
								g_objectTable[objectIdx].mobj->state == 0) {
								MobileObject* mobj;
								int nextViewX;
								int nextViewY;
								int nextViewZ;

								mobj = g_objectTable[objectIdx].mobj;
								if (mobj->orientMatrixDirty) {
									FVIEW_calcrotatemove(g_objectTable[objectIdx].pitch,
														 g_objectTable[objectIdx].yaw,
														 &g_objectTable[objectIdx]);
									FVIEW_calcrotateorient(g_objectTable[objectIdx].roll,
														   g_objectTable[objectIdx].angleD,
														   &g_objectTable[objectIdx]);
								}
								g_camRelWorldX += Xwa_Q15MulReuseFirstSlot(0x100, mobj->moveX);
								g_camRelWorldY += Xwa_Q15MulReuseFirstSlot(0x100, mobj->moveY);
								if (mobj->speed >= 0x400u) {
									g_camRelWorldX += mobj->moveX;
									g_camRelWorldY += mobj->moveY;
								} else {
									g_camRelWorldX +=
										Xwa_Q15MulReuseFirstSlot((int)(32 * mobj->speed), mobj->moveX);
									g_camRelWorldY +=
										Xwa_Q15MulReuseFirstSlot((int)(32 * mobj->speed), mobj->moveY);
								}

								nextViewX = TRANSFM2_CamMatDotRow0(
									g_camRelWorldX, g_camRelWorldY,
									-65536 - g_players[g_localPlayer].viewState.savedTargetZ);
								nextViewY = TRANSFM2_CamMatDotRow1(
									g_camRelWorldX, g_camRelWorldY,
									-65536 - g_players[g_localPlayer].viewState.savedTargetZ);
								nextViewZ = TRANSFM2_CamMatDotRow2(
									g_camRelWorldX, g_camRelWorldY,
									-65536 - g_players[g_localPlayer].viewState.savedTargetZ);
								viewX = nextViewX;
								viewY = nextViewY;
								viewZ = nextViewZ;
								if (nextViewZ <= 0) {
									TRANSFM2_clipobjecteyez(gridViewX, gridViewY, gridViewZ);
									nextViewZ = viewZ;
								}
								g_flightDrawLineFn(g_flightClipLeft + TRANSFM2_ProjectScreenX(viewX, viewZ),
												   g_flightClipTop +
													   TRANSFM2_ProjectScreenY(viewY, nextViewZ),
												   gridScreenX, gridScreenY, g_flightTextBgColor);
							}

							FlightText_SetBackgroundColor(0x40u);
							{
								unsigned int boxExtent;
								int boxSize;

								boxExtent =
									(unsigned int)((uint32_t)(g_projScaleInt *
															  Targeting_GetObjectBoxExtent(objectIdx)) /
												   (uint32_t)baseViewZ);
								if (boxExtent < (unsigned int)g_screenWidth / 0x50u) {
									boxExtent = (unsigned int)g_screenWidth / 0x50u;
								}
								if (boxExtent > (unsigned int)g_screenWidth >> 1) {
									boxExtent = (unsigned int)g_screenWidth >> 1;
								}
								boxSize = boxExtent + 4;

								if (g_objectTable[objectIdx].mobj == NULL &&
									g_objectTable[objectIdx].genusId == GENUS_Mine) {
									g_flightTextScratchBuffer[0] = '\0';
								} else {
									Hud_AppendObjectDisplayName(objectIdx, 2);
								}

								if (g_flightTextScratchBuffer[0] != '\0') {
									int labelY;

									labelY =
										(int16_t)clippedScreenY - boxSize / 2 - g_flightFontLineHeight - 1;
									FlightText_SetCursor(
										(int16_t)(clippedScreenX -
												  (FlightText_MeasureStringWidth(g_flightTextScratchBuffer) >>
												   1)),
										(int16_t)labelY);
									FlightText_DrawString(g_flightTextScratchBuffer);
								}

								if (g_objectTable[objectIdx].genusId != GENUS_PlayerProjectile &&
									g_objectTable[objectIdx].genusId != GENUS_NpcProjectile) {
									unsigned int cameraFocusObjIdx;

									clippedScreenY += boxSize >> 1;
									FlightText_SetBackgroundColor(0x40u);
									cameraFocusObjIdx = g_players[g_localPlayer].viewState.cameraFocusObjIdx;
									if (cameraFocusObjIdx != 0xffffu) {
										int range;
										int rangeWhole;

										pai_ObjectRefDirectionToObjectRef(cameraFocusObjIdx, objectIdx);
										FlightText_SetCursor(
											(int16_t)(clippedScreenX - FlightText_MeasureStringWidth("00")),
											(int16_t)(clippedScreenY + 1));
										trig2_polardistance *= 161;
										range = (uint16_t)(trig2_polardistance >> 16);
										if (range >= 0x2710) {
											range = 9999;
										}
										rangeWhole = range / 100;
										FlightText_DrawDecimalNumber((uint16_t)rangeWhole, 2u, 1u);
										g_flightDrawCharFn(0x2eu);
										FlightText_DrawDecimalNumber((uint16_t)(range - rangeWhole * 100), 2u,
																	 2u);
									}
								}
							}
						}
					}
				}
			}
			FlightSurface_Unlock();
			entry = g_renderListHead;
		}

		entry = entry->next;
		g_renderListHead = entry;
	}

	g_renderListHead = savedHead;
	RenderScene_DrawVisibleFaces();
}

// FUNCTION: XWA 0x4A04C0
void FlightMap_DrawObjectIconAtViewPos(int objectIdx, int viewX, int viewY, int viewZ) {
	const ObjectRecord* obj;
	const MobileObject* mobj;
	ObjectTypeId objectType;
	unsigned int iconHalf;
	unsigned int clipHalfY;
	unsigned int clipHalfX;
	uint8_t iff;
	int softwareGroupOffset;
	int screenX;
	int screenY;
	uint16_t iconId;
	FlightTexQuad quad;

	iconHalf = (unsigned int)(int)(g_flightHudScaleFactor * 16.0f);
	quad.depthZ = 1;
	quad.screenX = 0;
	obj = &g_objectTable[objectIdx];
	quad.screenY = 0;
	quad.rotationAngle = 0;
	quad.screenSize = 256;
	mobj = obj->mobj;
	objectType = obj->objectType;
	clipHalfY = 0;
	clipHalfX = 0;

	if (mobj != NULL) {
		iff = (uint8_t)mobj->iff;
	} else {
		iff = g_missionFlightGroups[obj->flightGroupIdx].fg.iff;
	}

	switch (iff) {
		case 0:
			softwareGroupOffset = 10;
			objectIdx = (int)0xff00b400u;
			break;
		case 1:
		case 4:
			softwareGroupOffset = 20;
			objectIdx = (int)0xffb82400u;
			break;
		case 2:
			softwareGroupOffset = 30;
			objectIdx = (int)0xff008cf8u;
			break;
		case 3:
			softwareGroupOffset = 40;
			objectIdx = (int)0xffc0a400u;
			break;
		case 5:
			softwareGroupOffset = 50;
			objectIdx = (int)0xffb844a8u;
			break;
		default:
			softwareGroupOffset = 50;
			objectIdx = (int)0xffb844a8u;
			break;
	}

	screenX = TRANSFM2_ProjectScreenX(viewX, viewZ);
	screenY = TRANSFM2_ProjectScreenY(viewY, viewZ);
	if (g_useHardware3D) {
		clipHalfY = iconHalf >> 1;
		clipHalfX = iconHalf >> 1;
	}

	if ((int16_t)screenX < 0 || (int16_t)(screenX + (int)clipHalfX) >= (int16_t)g_screenWidth ||
		(int16_t)screenY < 0 || (int16_t)(screenY + (int)clipHalfY) >= (int16_t)g_screenHeight) {
		return;
	}

	if (objectType != OBJ_None && (uint16_t)objectType <= (uint16_t)OBJ_NoAsset_222) {
		iconId = g_flightMapIconByObjectType[(uint16_t)objectType] != 0
					 ? g_flightMapIconByObjectType[(uint16_t)objectType]
					 : 0;
	} else {
		iconId = 0;
	}

	if (iconId == 0) {
		return;
	}

	if (!g_useHardware3D) {
		FlightSurface_Lock();
		Hud_SetDrawTargetSurface();
	}

	if (g_useHardware3D) {
		quad.screenY = g_screenHeight - screenY;
		quad.screenX = screenX;
		FeDiskIo_SelectTextureFrame(OBJ_MapIconTextureGroup14800, (uint16_t)(iconId / 100), 256);
		RenderQuad_DrawModelTexture(OBJ_MapIconTextureGroup14800, &quad, objectIdx);
	} else {
		Hud_SetupResourceData(softwareGroupOffset + 14600, iconId);
		if (g_curImage != NULL) {
			Hud_DrawImageToDIB(screenX - ((uint16_t)g_curImage->width >> 1),
							   screenY - ((uint16_t)g_curImage->height >> 1));
		}
		FlightSurface_Unlock();
	}
}

// FUNCTION: XWA 0x4A0C30
void FlightMap_DrawGrid(void) {
	int remaining;
	int baseY;
	int baseZ;
	int endX;
	int endY;
	int endZ;
	int gridYBase;
	int savedGridYBase;
	int gridCoord;
	int temporary;

	worldlocz = 0;
	worldlocy = 0;
	worldlocx = 0;

	gridCoord = worldlocy - 0x100000;
	remaining = 33;
	do {
		g_camRelWorldX = -0x100000 - g_players[g_localPlayer].viewState.savedTargetX;
		g_camRelWorldY = gridCoord - g_players[g_localPlayer].viewState.savedTargetY;
		g_camRelWorldZ = -0x10000 - g_players[g_localPlayer].viewState.savedTargetZ;
		gridCoord += 0x10000;

		baseZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		temporary = g_camMatR2_X;
		temporary *= 64;
		temporary += baseZ;
		endZ = temporary;
		viewZ = baseZ;
		if (endZ > 0 || baseZ > 0) {
			viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
			baseY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
			temporary = g_camMatR0_X;
			temporary *= 64;
			temporary += viewX;
			endX = temporary;
			temporary = g_camMatR1_X;
			temporary *= 64;
			temporary += baseY;
			endY = temporary;
			viewY = baseY;

			if (endZ <= 0) {
				temporary = endZ;
				endZ = viewZ;
				viewZ = temporary;
				temporary = endY;
				endY = viewY;
				viewY = temporary;
				temporary = endX;
				endX = viewX;
				viewX = temporary;
			}

			if (viewZ <= 0) {
				TRANSFM2_clipobjecteyez(endX, endY, endZ);
			}

			FlightSurface_Lock();
			g_flightDrawLineFn(g_flightClipLeft + TRANSFM2_ProjectScreenX(viewX, viewZ),
							   g_flightClipTop + TRANSFM2_ProjectScreenY(viewY, viewZ),
							   g_flightClipLeft + TRANSFM2_ProjectScreenX(endX, endZ),
							   g_flightClipTop + TRANSFM2_ProjectScreenY(endY, endZ), 0x31u);
			FlightSurface_Unlock();
		}
	} while (--remaining != 0);

	gridCoord = worldlocx;
	gridYBase = worldlocy;
	gridCoord -= 0x100000;
	gridYBase -= 0x100000;
	savedGridYBase = gridYBase;
	remaining = 33;
	do {
		g_camRelWorldX = gridCoord - g_players[g_localPlayer].viewState.savedTargetX;
		g_camRelWorldY = gridYBase - g_players[g_localPlayer].viewState.savedTargetY;
		g_camRelWorldZ = -0x10000 - g_players[g_localPlayer].viewState.savedTargetZ;
		gridCoord += 0x10000;

		baseZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		temporary = g_camMatR2_Y;
		temporary *= 64;
		temporary += baseZ;
		endZ = temporary;
		viewZ = baseZ;
		if (endZ > 0 || baseZ > 0) {
			viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
			baseY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
			temporary = g_camMatR0_Y;
			temporary *= 64;
			temporary += viewX;
			endX = temporary;
			temporary = g_camMatR1_Y;
			temporary *= 64;
			temporary += baseY;
			endY = temporary;
			viewY = baseY;

			if (endZ <= 0) {
				temporary = endZ;
				endZ = viewZ;
				viewZ = temporary;
				temporary = endY;
				endY = viewY;
				viewY = temporary;
				temporary = endX;
				endX = viewX;
				viewX = temporary;
			}

			if (viewZ <= 0) {
				TRANSFM2_clipobjecteyez(endX, endY, endZ);
			}

			FlightSurface_Lock();
			g_flightDrawLineFn(g_flightClipLeft + TRANSFM2_ProjectScreenX(viewX, viewZ),
							   g_flightClipTop + TRANSFM2_ProjectScreenY(viewY, viewZ),
							   g_flightClipLeft + TRANSFM2_ProjectScreenX(endX, endZ),
							   g_flightClipTop + TRANSFM2_ProjectScreenY(endY, endZ), 0x31u);
			FlightSurface_Unlock();
			gridYBase = savedGridYBase;
		}
	} while (--remaining != 0);
}

// FUNCTION: XWA 0x4A0730
void FlightMap_DrawObjectBoxCorners(int x, int y, int width, int height, uint8_t colorIndex) {
	int right;
	int bottom;
	int cornerWidth;
	int cornerHeight;

	bottom = y + height;
	if (bottom <= 0) {
		return;
	}

	right = x + width;
	if (right <= 0 || x >= g_flightVpWidth || y >= g_flightVpHeight || height <= 0 || width <= 0) {
		return;
	}

	cornerWidth = width >> 3;
	cornerHeight = height >> 3;
	if (cornerWidth < 3) {
		cornerWidth = 3;
	}
	if (cornerHeight < 3) {
		cornerHeight = 3;
	}
	if (cornerWidth > width) {
		cornerWidth = width;
	}
	if (cornerHeight > height) {
		cornerHeight = height;
	}

	FlightSurface_Lock();

	if (y >= 0) {
		int spanStart;
		int spanEnd;

		spanEnd = x + cornerWidth;
		spanStart = x;
		if (spanEnd > 0 && x < g_flightVpWidth) {
			int colStart;
			int colEnd;
			uint16_t* rowBase;
			unsigned int color;

			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			rowBase =
				(uint16_t*)((uint8_t*)g_flightSwFramebufferBase + g_surfacePitch * (g_flightClipTop + y));
			colEnd = g_flightClipLeft + spanEnd;
			colStart = g_flightClipLeft + spanStart;
			color = g_flightTextPalette[colorIndex];
			for (; colStart < colEnd; ++colStart) {
				rowBase[colStart] = color;
			}
		}

		spanStart = right - cornerWidth;
		spanEnd = spanStart + cornerWidth;
		if (spanEnd > 0 && spanStart < g_flightVpWidth) {
			int colStart;
			int colEnd;
			uint16_t* rowBase;
			unsigned int color;

			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			rowBase =
				(uint16_t*)((uint8_t*)g_flightSwFramebufferBase + g_surfacePitch * (g_flightClipTop + y));
			colEnd = g_flightClipLeft + spanEnd;
			colStart = g_flightClipLeft + spanStart;
			color = g_flightTextPalette[colorIndex];
			for (; colStart < colEnd; ++colStart) {
				rowBase[colStart] = color;
			}
		}
	}

	if (bottom <= g_flightVpHeight) {
		int spanStart;
		int spanEnd;

		spanEnd = x + cornerWidth;
		spanStart = x;
		if (spanEnd > 0 && x < g_flightVpWidth) {
			int colStart;
			int colEnd;
			uint16_t* rowBase;
			uint16_t color;

			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			rowBase = (uint16_t*)((uint8_t*)g_flightSwFramebufferBase +
								  g_surfacePitch * (g_flightClipTop + bottom - 1));
			colEnd = g_flightClipLeft + spanEnd;
			colStart = g_flightClipLeft + spanStart;
			color = g_flightTextPalette[colorIndex];
			for (; colStart < colEnd; ++colStart) {
				rowBase[colStart] = color;
			}
		}

		spanStart = right - cornerWidth;
		spanEnd = spanStart + cornerWidth;
		if (spanEnd > 0 && spanStart < g_flightVpWidth) {
			int colStart;
			int colEnd;
			uint16_t* rowBase;
			uint16_t color;

			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			rowBase = (uint16_t*)((uint8_t*)g_flightSwFramebufferBase +
								  g_surfacePitch * (g_flightClipTop + bottom - 1));
			colEnd = g_flightClipLeft + spanEnd;
			colStart = g_flightClipLeft + spanStart;
			color = g_flightTextPalette[colorIndex];
			for (; colStart < colEnd; ++colStart) {
				rowBase[colStart] = color;
			}
		}
	}

	if (cornerHeight > 1) {
		int scanY;
		int remaining;
		scanY = y + 1;
		remaining = cornerHeight - 1;
		do {
			if (scanY >= 0 && scanY < g_flightVpHeight) {
				if (x >= 0) {
					int colStart;
					int colEnd;
					uint16_t* rowBase;
					uint16_t color;

					color = g_flightTextPalette[colorIndex];
					rowBase = (uint16_t*)((uint8_t*)g_flightSwFramebufferBase +
										  g_surfacePitch * (g_flightClipTop + scanY));
					colEnd = g_flightClipLeft + (x + 1);
					colStart = g_flightClipLeft + (x);
					for (; colStart < colEnd; ++colStart) {
						rowBase[colStart] = color;
					}
				}
				if (right <= g_flightVpWidth) {
					int colStart;
					int colEnd;
					uint16_t* rowBase;
					uint16_t color;

					color = g_flightTextPalette[colorIndex];
					rowBase = (uint16_t*)((uint8_t*)g_flightSwFramebufferBase +
										  g_surfacePitch * (g_flightClipTop + scanY));
					colEnd = g_flightClipLeft + (right);
					colStart = g_flightClipLeft + (right - 1);
					for (; colStart < colEnd; ++colStart) {
						rowBase[colStart] = color;
					}
				}
			}
			++scanY;
			--remaining;
		} while (remaining != 0);
	}

	{
		int rowOffset;
		int lastRowOffset;
		int scanY;

		rowOffset = height - cornerHeight;
		lastRowOffset = height - 1;
		if (rowOffset < lastRowOffset) {
			scanY = y + rowOffset;
			do {
				if (rowOffset >= cornerHeight && scanY >= 0 && scanY < g_flightVpHeight) {
					if (x >= 0) {
						int colStart;
						int colEnd;
						uint16_t* rowBase;
						uint16_t color;

						color = g_flightTextPalette[colorIndex];
						rowBase = (uint16_t*)((uint8_t*)g_flightSwFramebufferBase +
											  g_surfacePitch * (g_flightClipTop + scanY));
						colEnd = g_flightClipLeft + (x + 1);
						colStart = g_flightClipLeft + (x);
						for (; colStart < colEnd; ++colStart) {
							rowBase[colStart] = color;
						}
					}
					if (right <= g_flightVpWidth) {
						int colStart;
						int colEnd;
						uint16_t* rowBase;
						uint16_t color;

						color = g_flightTextPalette[colorIndex];
						rowBase = (uint16_t*)((uint8_t*)g_flightSwFramebufferBase +
											  g_surfacePitch * (g_flightClipTop + scanY));
						colEnd = g_flightClipLeft + (right);
						colStart = g_flightClipLeft + (right - 1);
						for (; colStart < colEnd; ++colStart) {
							rowBase[colStart] = color;
						}
					}
				}
				++rowOffset;
				++scanY;
			} while (rowOffset < lastRowOffset);
		}
	}

	FlightSurface_Unlock();
}

static const uint8_t s_pickPrimaryGenusDispatch[GENUS_WeaponEmplacement + 1] = {
	0, 0, 0, 0, 0, 0, 3, 3, 3, 1, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2,
};

// FUNCTION: XWA 0x4A0FB0
int FlightMap_PickObjectNearestScreenCenter(int playerIdx) {
	uint8_t mapCameraState;
	uint32_t objectIdx;
	int bestScore;
	int bestObjectIdx;

	mapCameraState = g_players[playerIdx].mapCameraState;
	if (mapCameraState > 1u) {
		FVIEW_BuildCameraOrient(0, 0x4000, 0, 0, (int16_t)(-16384 * (int)(mapCameraState & 0x7fu) / 127), 0,
								NULL, -1);
	} else {
		int aimTargetIdx;

		FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
								(int16_t)g_players[playerIdx].viewState.viewPitch,
								(int16_t)g_players[playerIdx].viewState.viewYaw, 0,
								(int16_t)g_players[playerIdx].viewState.hudAimX,
								(int16_t)g_players[playerIdx].viewState.hudAimY, NULL, -1);
		aimTargetIdx = g_players[playerIdx].viewState.aimTargetIdx;
		if (aimTargetIdx != 0xffff) {
			trig2_ctop(g_objectTable[aimTargetIdx].world_x - g_players[playerIdx].viewState.savedTargetX,
					   g_objectTable[aimTargetIdx].world_y - g_players[playerIdx].viewState.savedTargetY,
					   g_objectTable[aimTargetIdx].world_z - g_players[playerIdx].viewState.savedTargetZ);
			FVIEW_BuildCameraOrient(0, (int16_t)targetPitch, (int16_t)trig2_xyangle, 0, 0, 0, NULL, -1);
		}
	}

	bestObjectIdx = 0xffff;
	bestScore = (int)(((uint32_t)g_screenWidth * (uint32_t)g_screenWidth +
					   (uint32_t)g_screenHeight * (uint32_t)g_screenHeight) >>
					  3);

	objectIdx = g_activeRegionObjectSlotStart;
	if (objectIdx < g_explosionObjectSlotEnd) {
		do {
			ObjectRecord* obj;
			int objectType;

			obj = &g_objectTable[objectIdx];
			objectType = obj->objectType;
			if (objectType != OBJ_None) {
				unsigned int genusId;

				genusId = obj->genusId;
				if (genusId <= GENUS_WeaponEmplacement) {
					switch (s_pickPrimaryGenusDispatch[genusId]) {
						case 0:
						case 1:
						case 2:
							if (RenderList_ProjectObjectBoundsForCulling(
									objectIdx, g_modelTypeTable[objectType].maxBoundsExtent, playerIdx)) {
								int projectedX;
								int projectedY;
								int score;

								projectedX = (viewX << perspShift) / viewZ;
								projectedY = (viewY << perspShift) / viewZ;
								score = projectedX * projectedX + projectedY * projectedY;

								if (objectIdx == (uint32_t)g_players[playerIdx].viewState.aimTargetIdx ||
									objectIdx == (uint32_t)g_players[playerIdx].viewState.cameraFocusObjIdx) {
									int biasX;
									int biasY;

									biasX = g_screenWidth >> 4;
									biasY = g_screenHeight >> 4;
									score += biasX * biasX + biasY * biasY;
								}

								if (score < bestScore) {
									bestScore = score;
									bestObjectIdx = objectIdx;
								}
							}
							break;
						default:
							break;
					}
				}
			}
			++objectIdx;
		} while (objectIdx < g_explosionObjectSlotEnd);
	}

	objectIdx = g_objScanStart;
	if (objectIdx < g_regionStaticObjectSlotEnd) {
		do {
			ObjectRecord* obj;
			int objectType;

			obj = &g_objectTable[objectIdx];
			objectType = obj->objectType;
			if (objectType != OBJ_None) {
				int genusId;

				genusId = obj->genusId;
				if (genusId == GENUS_Mine || genusId == GENUS_DeathStarTunnelSegment) {
					if (RenderList_ProjectObjectBoundsForCulling(
							objectIdx, g_modelTypeTable[objectType].maxBoundsExtent, playerIdx)) {
						int projectedX;
						int projectedY;
						int score;

						projectedX = (viewX << perspShift) / viewZ;
						projectedY = (viewY << perspShift) / viewZ;
						score = projectedX * projectedX + projectedY * projectedY;

						if (objectIdx == (uint32_t)g_players[playerIdx].viewState.aimTargetIdx ||
							objectIdx == (uint32_t)g_players[playerIdx].viewState.cameraFocusObjIdx) {
							int biasX;
							int biasY;

							biasX = g_screenWidth >> 4;
							biasY = g_screenHeight >> 4;
							score += biasX * biasX + biasY * biasY;
						}

						if (score < bestScore) {
							bestScore = score;
							bestObjectIdx = objectIdx;
						}
					}
				}
			}
			++objectIdx;
		} while (objectIdx < g_regionStaticObjectSlotEnd);
	}

	return bestObjectIdx;
}

static __inline void FlightMap_QueueDynamicRenderObject(int objectIdx, int sortDepth, int viewPosX,
														int viewPosY, int viewPosZ, int cullFlags,
														int projectedRadius) {
	if (g_renderObjectListCount >= 1664) {
		return;
	}

	g_renderObjectListEntries[g_renderObjectListCount].sortDepth = sortDepth;
	g_renderObjectListEntries[g_renderObjectListCount].objectIdx = objectIdx;
	g_renderObjectListEntries[g_renderObjectListCount].next = g_renderListHead;
	g_renderObjectListEntries[g_renderObjectListCount].viewX = viewPosX;
	g_renderObjectListEntries[g_renderObjectListCount].viewY = viewPosY;
	g_renderObjectListEntries[g_renderObjectListCount].viewZ = viewPosZ;
	g_renderObjectListEntries[g_renderObjectListCount].cullFlags = cullFlags;
	g_renderObjectListEntries[g_renderObjectListCount].projectedRadius = projectedRadius;
	g_renderListHead = &g_renderObjectListEntries[g_renderObjectListCount];
	++g_renderObjectListCount;
}

// FUNCTION: XWA 0x49EAD0
void FlightMap_RenderView(void) {
	uint32_t objectIdx;

	FlightText_SetClipRect(g_flightVpX, g_flightVpY, g_flightVpWidth + g_flightVpX,
						   g_flightVpHeight + g_flightVpY);
	g_renderObjectListCount = 0;
	g_renderListHead = NULL;

	for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_explosionObjectSlotEnd; ++objectIdx) {
		ObjectRecord* obj;
		unsigned int objectType;

		obj = &g_objectTable[objectIdx];
		objectType = obj->objectType;
		if (objectType == OBJ_None) {
			continue;
		}

		switch (obj->genusId) {
			case GENUS_Fighter:
			case GENUS_Transport:
			case GENUS_Utility:
			case GENUS_Freighter:
			case GENUS_Starship:
			case GENUS_Platform:
			case GENUS_SatelliteBuoy:
			case GENUS_Container:
			case GENUS_PilotDroid:
			case GENUS_WeaponEmplacement:
				if (RenderList_ProjectObjectBoundsForCulling(
						(uint16_t)objectIdx, g_modelTypeTable[objectType].maxBoundsExtent, g_localPlayer)) {
					FlightMap_QueueDynamicRenderObject((int)objectIdx, viewZ, viewX, viewY, viewZ, 0x1f,
													   g_screenWidth);
				}
				break;

			case GENUS_PlayerProjectile:
			case GENUS_NpcProjectile:
			case GENUS_Debris:
			case GENUS_Explosion:
				if (FlightView_IsObjectSphereVisible((int)objectIdx,
													 g_modelTypeTable[objectType].maxBoundsExtent)) {
					FlightMap_QueueDynamicRenderObject((int)objectIdx, viewZ, viewX, viewY, viewZ, 0x1f,
													   g_screenWidth);
				}
				break;

			default:
				break;
		}
	}

	for (objectIdx = g_objScanStart; objectIdx < g_regionStaticObjectSlotEnd; ++objectIdx) {
		ObjectRecord* obj;
		unsigned int objectType;
		unsigned int genusId;

		obj = &g_objectTable[objectIdx];
		objectType = obj->objectType;
		if (objectType == OBJ_None) {
			continue;
		}

		genusId = obj->genusId;
		if (genusId == GENUS_Mine || genusId == GENUS_DeathStarTunnelSegment) {
			if (RenderList_ProjectObjectBoundsForCulling(
					(uint16_t)objectIdx, g_modelTypeTable[objectType].maxBoundsExtent, g_localPlayer)) {
				RenderList_QueueObject((int)objectIdx, viewZ, viewX, viewY, viewZ, 0x1f,
									   *(int*)&g_screenWidth);
			}
		}
	}

	RenderList_SortDepthDescending();
	g_unusedFlightMapRenderFlag = 1;
	if (g_players[g_localPlayer].viewState.savedTargetZ < -65536) {
		FlightMap_DrawObjectPass(1);
		FlightMap_DrawGrid();
		FlightMap_DrawObjectPass(0);
	} else {
		FlightMap_DrawObjectPass(0);
		FlightMap_DrawGrid();
		FlightMap_DrawObjectPass(1);
	}

	RenderScene_Initialize(1);
	if (g_useHardware3D) {
		Hud_RenderHud();
		RenderScene_DrawVisibleFaces();
	}
	Hud_UpdateHUD();
	Hud_BlitSoftwareHudTextPanes();
	if (g_useHardware3D) {
		if (g_worldParticleEffects != NULL) {
			Particle_UpdateWorldEffects();
		}
		Particle_UpdateObjectEffects();
		ObjectTrail_RenderObjectTrails();
		RenderScene_DrawVisibleFaces();
		RenderScene_Initialize(1);
		FlightText_FlushQueue();
		RenderScene_DrawVisibleFaces();
	}
}
