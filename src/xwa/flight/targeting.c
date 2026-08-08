#include "xwa/render/renderer_internal.h"

#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_map.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/flight/mission/mission.h"
#include "xwa_runtime/snapshot/snapshot_hud.h"

// GLOBAL: XWA 0x7CA180
uint16_t g_targetAngleScore;

static inline void Targeting_ProjectRatio(int* value, int forwardScore) {
	uint32_t numeratorHigh;

	numeratorHigh = (uint32_t)*value >> 24;
	*value = (int)((uint32_t)*value << 8);
	*value += 128;
	if ((uint32_t)*value < 128u) {
		++numeratorHigh;
	}

	if (numeratorHigh >= (uint32_t)forwardScore) {
		*value = 0x7fffff00;
		return;
	}

	*value = (int)((uint32_t)*value / (uint32_t)forwardScore);
}

// FUNCTION: XWA 0x503D60
int Targeting_GetObjectBoxExtent(int objectIdx) {
	ObjectRecord* obj;
	MobileObject* mobj;
	CraftData* craft;
	int modelIndex;
	int extent;

	mobj = g_objectTable[objectIdx].mobj;
	obj = &g_objectTable[objectIdx];
	if (mobj != NULL) {
		craft = mobj->pCraft;
		if (craft != NULL) {
			modelIndex = craft->modelIndex;
			if (modelIndex != 0xffff) {
				extent = ((int16_t)g_modelDefs[modelIndex].boundSizeZ +
						  (int16_t)g_modelDefs[modelIndex].boundSizeY +
						  (int16_t)g_modelDefs[modelIndex].boundSizeX) /
						 3;
				extent <<= (uint8_t)g_modelDefs[modelIndex].boundSizeShift;
				return extent;
			}
			return g_modelTypeTable[(uint16_t)obj->objectType].maxBoundsExtent;
		}
		return g_modelTypeTable[(uint16_t)obj->objectType].maxBoundsExtent;
	}

	return g_modelTypeTable[(uint16_t)obj->objectType].maxBoundsExtent;
}

// FUNCTION: XWA 0x505C90
int Craft_IsSelectableDamageComponentMesh(ObjectTypeId objectType, int meshIndex) {
	MeshType meshType;
	int targetId;
	int meshCount;
	int candidateMesh;

	meshType = ModelMesh_GetObjectTypeMeshType(objectType, meshIndex);
	if (meshType == MESH_MiscHull || meshType == MESH_Antenna) {
		return 0;
	}

	targetId = ModelMesh_GetTargetId(objectType, meshIndex);
	if (targetId == 0) {
		return 1;
	}

	if (targetId == 1 && meshType != MESH_MainHull && meshType != MESH_Fuselage) {
		return targetId;
	}

	meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
	for (candidateMesh = 0; candidateMesh < meshCount; ++candidateMesh) {
		if (ModelMesh_GetTargetId(objectType, candidateMesh) == targetId &&
			ModelMesh_GetObjectTypeMeshType(objectType, candidateMesh) == meshType) {
			return candidateMesh == meshIndex;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x502F50
int16_t Targeting_ScoreCandidate(uint16_t candidateObjIdx, int16_t mode, int playerIdx,
								 uint16_t subSystemIdx) {
	ObjectRecord* playerObj;
	unsigned int candidateRef;
	int deltaX;
	int deltaY;
	int deltaZ;
	int forwardScore;
	int shift;
	int sideScore;
	int upScore;
	int weightedUpScore;
	int maxBoundsExtent;
	int boundsScore;

	g_targetAngleScore = 0xffffu;

	if (g_players[playerIdx].objectIndex == 0xffff) {
		return false;
	}

	candidateRef = (uint16_t)candidateObjIdx;
	pai_ObjectRefUpdateApproxRangeScore(candidateRef, (unsigned int)g_players[playerIdx].objectIndex);

	if ((uint32_t)g_targetRangeScore < 0xa0000u) {
		Mission_ResolveObjectOrMissionPointWorldLoc(candidateRef, 0, 0, 0);
		if ((uint16_t)candidateObjIdx == (uint16_t)g_players[playerIdx].currentTargetObjectIdx) {
			if (g_objectTable[candidateRef].objectType != 0) {
				if (candidateRef >= (uint32_t)g_activeRegionObjectSlotStart &&
					candidateRef < (uint32_t)g_activeRegionCraftObjectSlotEnd &&
					g_objectTable[candidateRef].mobj->pCraft != NULL) {
					int targetX;
					int targetY;
					int targetZ;

					if (subSystemIdx != 0xffffu) {
						pai_RotateLocalVectorToWorldScratch(
							&g_objectTable[candidateRef],
							ModelMesh_GetCenterX(g_objectTable[candidateRef].objectType, subSystemIdx),
							ModelMesh_GetCenterZ(g_objectTable[candidateRef].objectType, subSystemIdx),
							-ModelMesh_GetCenterY(g_objectTable[candidateRef].objectType, subSystemIdx));
						worldlocx += g_rotatedX;
						worldlocy += g_rotatedY;
						worldlocz += g_rotatedZ;
						targetX = worldlocx;
						targetY = worldlocy;
						targetZ = worldlocz;
					} else {
						targetX = worldlocx;
						targetY = worldlocy;
						targetZ = worldlocz;
					}

					g_targetRangeScore = collide_roughdistance3d(
						targetX - g_objectTable[g_players[playerIdx].objectIndex].world_x,
						targetY - g_objectTable[g_players[playerIdx].objectIndex].world_y,
						targetZ - g_objectTable[g_players[playerIdx].objectIndex].world_z);
				}
			}
		}

		shift = 4;
		playerObj = &g_objectTable[g_players[playerIdx].objectIndex];
		deltaX = (worldlocx - playerObj->world_x) >> 4;
		deltaY = (worldlocy - playerObj->world_y) >> 4;
		deltaZ = (worldlocz - playerObj->world_z) >> 4;
	} else {
		Mission_ResolveObjectOrMissionPointWorldLoc(candidateRef, 0, 0, 0);
		shift = 8;
		playerObj = &g_objectTable[g_players[playerIdx].objectIndex];
		deltaX = (worldlocx - playerObj->world_x) >> 8;
		deltaY = (worldlocy - playerObj->world_y) >> 8;
		deltaZ = (worldlocz - playerObj->world_z) >> 8;
	}

	if (playerObj->mobj->orientMatrixDirty) {
		FVIEW_calcrotatemove(playerObj->pitch, playerObj->yaw, playerObj);
		FVIEW_calcrotateorient(g_objectTable[g_players[playerIdx].objectIndex].roll,
							   g_objectTable[g_players[playerIdx].objectIndex].angleD,
							   &g_objectTable[g_players[playerIdx].objectIndex]);
	}

	if (g_players[playerIdx].currentSeatIdx != 0) {
		forwardScore = Xwa_Q15MulReuseFirstSlot((int16_t)deltaX, g_players[playerIdx].turretCamMat[0]) +
					   Xwa_Q15MulReuseFirstSlot((int16_t)deltaY, g_players[playerIdx].turretCamMat[1]) +
					   Xwa_Q15MulReuseFirstSlot((int16_t)deltaZ, g_players[playerIdx].turretCamMat[2]);
	} else {
		forwardScore =
			Xwa_Q15MulReuseFirstSlot((int16_t)deltaX,
									 g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedFwdX) +
			Xwa_Q15MulReuseFirstSlot((int16_t)deltaY,
									 g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedFwdY) +
			Xwa_Q15MulReuseFirstSlot((int16_t)deltaZ,
									 g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedFwdZ);
	}

	if (forwardScore <= 0 || forwardScore > 0x20000) {
		return false;
	}

	if (mode != 0 && forwardScore < 0x2000) {
		++shift;
	}

	if (g_players[playerIdx].currentSeatIdx != 0) {
		sideScore = Xwa_Q15MulReuseFirstSlot((int16_t)deltaX, g_players[playerIdx].turretCamMat[3]) +
					Xwa_Q15MulReuseFirstSlot((int16_t)deltaY, g_players[playerIdx].turretCamMat[4]) +
					Xwa_Q15MulReuseFirstSlot((int16_t)deltaZ, g_players[playerIdx].turretCamMat[5]);
	} else {
		sideScore =
			Xwa_Q15MulReuseFirstSlot((int16_t)deltaX,
									 g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedSideX) +
			Xwa_Q15MulReuseFirstSlot((int16_t)deltaY,
									 g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedSideY) +
			Xwa_Q15MulReuseFirstSlot((int16_t)deltaZ,
									 g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedSideZ);
	}

	if (sideScore < 0) {
		sideScore = -sideScore;
	}

	Targeting_ProjectRatio(&sideScore, forwardScore);
	if (sideScore > 160) {
		return false;
	}

	if (g_players[playerIdx].currentSeatIdx != 0) {
		upScore = Xwa_Q15MulReuseFirstSlot((int16_t)deltaX, g_players[playerIdx].turretCamMat[6]) +
				  Xwa_Q15MulReuseFirstSlot((int16_t)deltaY, g_players[playerIdx].turretCamMat[7]) +
				  Xwa_Q15MulReuseFirstSlot((int16_t)deltaZ, g_players[playerIdx].turretCamMat[8]);
	} else {
		upScore = Xwa_Q15MulReuseFirstSlot((int16_t)deltaX,
										   g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedUpX) +
				  Xwa_Q15MulReuseFirstSlot((int16_t)deltaY,
										   g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedUpY) +
				  Xwa_Q15MulReuseFirstSlot((int16_t)deltaZ,
										   g_objectTable[g_players[playerIdx].objectIndex].mobj->cachedUpZ);
	}

	if (upScore < 0) {
		upScore = -upScore;
	}

	Targeting_ProjectRatio(&upScore, forwardScore);
	if (upScore > 100) {
		return false;
	}

	weightedUpScore = (29789 * upScore * 2) >> 16;
	if (g_objectTable[candidateRef].mobj != NULL) {
		if (g_objectTable[candidateRef].mobj->pCraft != NULL) {
			ModelDef* modelDef;

			modelDef = &g_modelDefs[g_objectTable[candidateRef].mobj->pCraft->modelIndex];
			maxBoundsExtent =
				((int16_t)(modelDef->boundSizeX + modelDef->boundSizeY + modelDef->boundSizeZ) / 3)
				<< (uint8_t)modelDef->boundSizeShift;
		} else {
			maxBoundsExtent =
				g_modelTypeTable[(uint16_t)g_objectTable[candidateRef].objectType].maxBoundsExtent;
		}
	} else {
		maxBoundsExtent = g_modelTypeTable[(uint16_t)g_objectTable[candidateRef].objectType].maxBoundsExtent;
	}

	boundsScore = maxBoundsExtent >> shift;
	Targeting_ProjectRatio(&boundsScore, forwardScore);
	if (boundsScore <= 0) {
		boundsScore = 1;
	}

	if (mode == 0) {
		boundsScore *= 3;
		if (boundsScore < 10) {
			boundsScore = 9;
		}
	}

	g_targetAngleScore = (uint16_t)(weightedUpScore + sideScore);
	if (sideScore < boundsScore) {
		if (weightedUpScore < boundsScore) {
			return (int16_t)1;
		}
	}
	return (int16_t)0;
}

// FUNCTION: XWA 0x503A30
void Targeting_DrawObjectBox(uint16_t objectIdx, uint16_t componentIdx, uint8_t colorIndex) {
	int worldX;
	int worldY;
	int worldZ;
	int relX;
	int relY;
	int relZ;
	int objViewX;
	int objViewY;
	int objViewZ;
	int screenX;
	int screenY;
	int extent;
	int minBoxSize;
	int maxBoxSize;
	int boxWidth;
	int boxHeight;
	int playerIdx;
	int objectRef;
	int componentRef;
	int rotatedX;
	int rotatedY;
	int rotatedZ;

	if (objectIdx == 0xffffu || g_replayViewMode ||
		!g_players[g_localPlayer].hyperspaceRuntime.targetBoxEnabled) {
		return;
	}

#ifdef XWA_MODERN
	{
		const ObjectRecord* captureObj = &g_objectTable[objectIdx];
		const int captureExtent =
			componentIdx == 0xffffu
				? Targeting_GetObjectBoxExtent(objectIdx)
				: ModelMesh_GetComponentMaxExtent((uint16_t)captureObj->objectType, componentIdx) / 3;
		const PlayerData* capturePlayer = &g_players[g_localPlayer];
		/* The classic component-size branch compares only the component
		 * index; preserve that exact gate even if another object's box uses
		 * the same component index. Whole-object selected is diagnostic. */
		const int selected = componentIdx == 0xffffu
								 ? objectIdx == (uint16_t)capturePlayer->currentTargetObjectIdx
								 : componentIdx == (uint16_t)capturePlayer->selectedTargetComponent;
		XwaSnapshotHud_NoteTargetBox(objectIdx, captureObj->objectSignature, componentIdx, colorIndex,
									 selected, captureExtent);
	}
#endif

	objectRef = objectIdx;
	componentRef = componentIdx;
	Mission_ResolveObjectOrMissionPointWorldLoc(objectRef, 0, 0, 0);

	if (componentRef != 0xffff) {
		pai_RotateLocalVectorToWorldScratch(
			&g_objectTable[objectRef],
			ModelMesh_GetCenterX(g_objectTable[objectRef].objectType, componentRef),
			ModelMesh_GetCenterZ(g_objectTable[objectRef].objectType, componentRef),
			-ModelMesh_GetCenterY(g_objectTable[objectRef].objectType, componentRef));
		rotatedY = g_rotatedY;
		rotatedX = g_rotatedX;
		rotatedZ = g_rotatedZ;
		worldY = worldlocy + rotatedY;
		worldX = worldlocx + rotatedX;
		worldZ = worldlocz + rotatedZ;
		worldlocx = worldX;
		worldlocy = worldY;
		worldlocz = worldZ;
	} else {
		worldX = worldlocx;
		worldY = worldlocy;
		worldZ = worldlocz;
	}

	relX = worldX - g_players[g_localPlayer].viewState.savedTargetX;
	relY = worldY - g_players[g_localPlayer].viewState.savedTargetY;
	relZ = worldZ - g_players[g_localPlayer].viewState.savedTargetZ;
	objViewZ = TRANSFM2_CamMatDotRow2(relX, relY, relZ);
	if (objViewZ > 0) {
		objViewX = TRANSFM2_CamMatDotRow0(relX, relY, relZ);
		objViewY = TRANSFM2_CamMatDotRow1(relX, relY, relZ);
		screenX = TRANSFM2_ProjectScreenX(objViewX, objViewZ);
		screenY = TRANSFM2_ProjectScreenY(objViewY, objViewZ);
	}
	if (objViewZ <= 0) {
		return;
	}

	if (componentRef != 0xffff) {
		MeshType meshType;
		int componentExtent;

		componentExtent = ModelMesh_GetComponentMaxExtent(g_objectTable[objectRef].objectType, componentRef);
		meshType = ModelMesh_GetObjectTypeMeshType(g_objectTable[objectRef].objectType, componentRef);
		if (meshType == MESH_WeaponSystem1 || meshType == MESH_WeaponSystem2) {
			extent = componentExtent / 3;
		} else {
			extent = componentExtent / 3;
		}

		playerIdx = g_localPlayer;
		if (g_players[playerIdx].selectedTargetComponent != (int16_t)componentRef) {
			if (objViewZ > 0x8000) {
				extent = 0;
			}
			minBoxSize = 1;
		} else {
			minBoxSize = 2;
		}
	} else {
		extent = Targeting_GetObjectBoxExtent(objectRef);
		playerIdx = g_localPlayer;
		minBoxSize = 8;
	}

	boxHeight = (int)((uint32_t)(g_projScaleInt * extent) / (uint32_t)objViewZ);
	maxBoxSize = (int)(((uint32_t)g_screenWidth >> 2) + ((uint32_t)g_screenWidth >> 1));
	if (boxHeight < minBoxSize) {
		boxHeight = minBoxSize;
	}
	if (boxHeight > maxBoxSize) {
		boxHeight = maxBoxSize;
	}
	if (componentRef != 0xffff) {
		boxWidth = boxHeight;
	} else {
		boxWidth = boxHeight + 8;
		boxHeight += 8;
	}

	if (g_players[playerIdx].mapCameraState) {
		FlightMap_DrawObjectBoxCorners(screenX - boxWidth / 2, screenY - boxHeight / 2, boxWidth, boxHeight,
									   colorIndex);
		return;
	}

	if (objViewZ > 1024) {
		objViewZ -= 1024;
	}
	Hud_DrawBoxInXTrans(screenX - boxWidth / 2, screenY - boxHeight / 2, boxWidth, boxHeight, colorIndex,
						objViewZ);
}

// FUNCTION: XWA 0x5036F0
void Targeting_DrawSceneObjectBoxes(void) {
	uint8_t missionType;
	int leadingTeam;
	int leadingScore;
	uint32_t teamIdx;
	uint32_t objectIdx;

	missionType = g_missionHeader.body.missionType;
	leadingTeam = 10;
	if ((missionType == XWA_MISSION_TYPE_QUICK_START || missionType == XWA_MISSION_TYPE_SKIRMISH) &&
		g_missionHeader.body.goalsUnimportant) {
		leadingScore = 0;
		for (teamIdx = 0; teamIdx < 8; ++teamIdx) {
			int score;

			score = g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][teamIdx] +
					g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIdx];
			if (score > leadingScore) {
				leadingScore = score;
				leadingTeam = teamIdx;
			}
		}
	}

	objectIdx = g_activeRegionObjectSlotStart;
	if (objectIdx >= g_activeRegionCraftObjectSlotEnd) {
		return;
	}
	do {
		MobileObject* mobj;
		ObjectRecord* obj;
		uint8_t colorIndex;
		int playerIff;
		int objTeam;
		int objTeamScore;

		obj = &g_objectTable[objectIdx];
		if (obj->objectType == 0) {
			continue;
		}
		if (g_players[g_localPlayer].objectIndex == (int)objectIdx) {
			continue;
		}
		if (obj->objectType >= 0x196u && obj->objectType <= 0x1a1u) {
			continue;
		}

		mobj = obj->mobj;
		colorIndex = 0;
		objTeam = (uint8_t)mobj->team;
		objTeamScore = g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][objTeam] +
					   g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][objTeam];
		if ((g_missionHeader.body.missionType == XWA_MISSION_TYPE_QUICK_START ||
			 g_missionHeader.body.missionType == XWA_MISSION_TYPE_SKIRMISH) &&
			objTeam == (uint16_t)g_players[g_localPlayer].playerIff && obj->genusId == 0) {
			colorIndex = 47;
		} else {
			playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
			if (leadingTeam == 10 || objTeamScore != leadingScore) {
				if (obj->playerOwnerIdx != -1 && obj->playerOwnerIdx != g_localPlayer) {
					if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_QUICK_START) {
						playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
						if (g_objectTable[(uint16_t)objectIdx].mobj != NULL) {
							objTeam = g_objectTable[(uint16_t)objectIdx].mobj->team;
						} else {
							objTeam = g_missionFlightGroups[g_objectTable[(uint16_t)objectIdx].flightGroupIdx]
										  .fg.team;
						}
						colorIndex =
							objTeam == playerIff || g_missionTeams[playerIff].allies[objTeam] ? 211 : 51;
					} else {
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
					}
				}
			} else {
				colorIndex = 212;
			}
		}

		if (colorIndex == 0) {
			continue;
		}

		{
			CraftData* craft;

			craft = mobj->pCraft;
			if (!g_flightLocatePlayersEnabled) {
				playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
				if ((int8_t)craft->iffVisibility[playerIff] <= 0) {
					if (g_objectTable[(uint16_t)objectIdx].mobj != NULL) {
						objTeam = g_objectTable[(uint16_t)objectIdx].mobj->team;
					} else {
						objTeam =
							g_missionFlightGroups[g_objectTable[(uint16_t)objectIdx].flightGroupIdx].fg.team;
					}
					if (objTeam != playerIff && !g_missionTeams[playerIff].allies[objTeam]) {
						continue;
					}
				}
			}
		}

		if ((uint16_t)objectIdx == 0xffffu ||
			(uint16_t)objectIdx >= (uint32_t)g_activeRegionCraftObjectSlotEnd ||
			g_objectTable[(uint16_t)objectIdx].playerOwnerIdx == -1 ||
			g_objectTable[(uint16_t)objectIdx].mobj->pCraft == NULL ||
			(g_objectTable[(uint16_t)objectIdx].mobj->pCraft->workingSubsystems & 0x100u) == 0 ||
			!g_objectTable[(uint16_t)objectIdx].mobj->pCraft->beamActive ||
			g_objectTable[(uint16_t)objectIdx].mobj->pCraft->beamTypeId != 3 ||
			!g_objectTable[(uint16_t)objectIdx].mobj->pCraft->beamTimer) {
			if ((uint16_t)g_players[g_localPlayer].currentTargetObjectIdx != objectIdx) {
				Targeting_DrawObjectBox((uint16_t)objectIdx, 0xffffu, colorIndex);
			}
		}
	} while (++objectIdx < g_activeRegionCraftObjectSlotEnd);
}
