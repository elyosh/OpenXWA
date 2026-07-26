#include "xwa/flight/ai/ai_internal.h"

// GLOBAL: XWA 0x917E60
int g_paifightSearchOriginX;

// GLOBAL: XWA 0x917E5C
int g_paifightSearchOriginY;

// GLOBAL: XWA 0x917E68
int g_paifightSearchOriginZ;

// GLOBAL: XWA 0x9AF180
uint8_t g_paifightGunnerTargetCandidateSet[3328];

// GLOBAL: XWA 0x5B6DE8
static const unsigned int g_aiFighterShootMaxRangeBySkill[3] = { 0x6000, 0x8000, 0xa000 };

// GLOBAL: XWA 0x5B6DF4
static const uint8_t g_aiFighterShootLinkSlot3ValueBySkill[3] = { 3, 4, 5 };

static __inline int16_t paifight_ObjectMatchesTargetPair(uint16_t objIdx,
														 MissionTriggerVariableType target1Type,
														 uint16_t target1, int16_t targetOrMode,
														 MissionTriggerVariableType target2Type,
														 uint16_t target2);
static __inline int paifight_AttackTargetPassesLiveFilter(ObjectRecord* object);
static __inline CraftData* paifight_AssignGunnerTurretTarget(unsigned int laserSlot, int16_t targetObjIdx);
static __inline uint32_t paifight_ApplyTurretTargetRangePenalty(uint16_t targetObjIdx, uint32_t rangeScore);
static __inline int paifight_IsWarheadClassProjectile(ObjectTypeId objectType);
static __inline void paifight_SetGunnerSelfDefenseSearchOrigin(unsigned int laserSlot);
static __inline int paifight_HasClearTurretSelfDefenseShotYXZ(uint16_t targetObjIdx);
static __inline int paifight_HasClearTurretSelfDefenseShotXYZ(uint16_t targetObjIdx);
static __inline int paifight_HasClearTurretSelfDefenseShotZXY(uint16_t targetObjIdx);
static __inline int paifight_GunnerSelfDefenseTargetIsAlreadyCovered(uint16_t targetObjIdx);
static __inline int paifight_IsCraftAttackingSelf(uint16_t candidateObjIdx, CraftData* candidateCraft);

// FUNCTION: XWA 0x4A83F0
uint16_t paifight_SelectTargetComponentMesh(uint16_t targetObjIdx) {
	uint16_t targetMesh;
	uint16_t candidateCount;
	uint16_t meshIdx;
	uint32_t bestDistance;
	int isSuperStarDestroyer;
	int meshCount;
	uint16_t objectType;
	uint8_t candidateMeshes[52];

	targetMesh = 0;
	bestDistance = 0x01000000u;
	candidateCount = 0;
	candidateMeshes[0] = 0;
	objectType = g_objectTable[targetObjIdx].objectType;
	isSuperStarDestroyer = objectType == OBJ_SuperStarDestroyer;

	if (targetObjIdx < g_activeRegionObjectSlotStart || targetObjIdx >= g_activeRegionCraftObjectSlotEnd) {
		return 0;
	}

	meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
	for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
		uint16_t meshType;

		meshType = ModelMesh_GetObjectTypeMeshType(g_objectTable[targetObjIdx].objectType, meshIdx);
		if (meshType == MESH_MainHull || meshType == MESH_Fuselage) {
			if (isSuperStarDestroyer) {
				uint32_t distance;

				distance = Object_DirectionAndDistanceToMeshCenter((uint16_t)g_paiContext.aiObjIdx,
																   targetObjIdx, meshIdx);
				if (distance < bestDistance) {
					targetMesh = meshIdx;
					bestDistance = distance;
				}
			} else {
				candidateMeshes[candidateCount++] = (uint8_t)meshIdx;
			}
		}
	}

	if (isSuperStarDestroyer) {
		return targetMesh;
	}

	return candidateMeshes[GameRandRange(candidateCount)];
}

// FUNCTION: XWA 0x4AB460
bool paifight_OrderSlotHasFutureTargets(uint8_t orderSlot, uint8_t regionIdx) {
	unsigned int orderIndex;

	orderIndex = orderSlot + 4 * regionIdx;
	if (paifight_HasFutureFgTargets(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.orders[orderIndex]
										.target1Type,
									g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.orders[orderIndex]
										.target1,
									g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.orders[orderIndex]
										.target1OrTarget2,
									g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.orders[orderIndex]
										.target2Type,
									g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.orders[orderIndex]
										.target2)) {
		return true;
	}

	if (paifight_HasFutureFgTargets(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.orders[orderIndex]
										.secondaryTargetTypes[XWA_ORDER_TARGET_3],
									g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.orders[orderIndex]
										.secondaryTargets[XWA_ORDER_TARGET_3],
									g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.orders[orderIndex]
										.target3OrTarget4,
									g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.orders[orderIndex]
										.secondaryTargetTypes[XWA_ORDER_TARGET_4],
									g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.orders[orderIndex]
										.secondaryTargets[XWA_ORDER_TARGET_4])) {
		return true;
	}

	return false;
}

// FUNCTION: XWA 0x4A6D40
bool paifight_OrderSlotHasRemainingTargets(uint8_t orderSlot, uint8_t regionIdx) {
	unsigned int orderIndex;
	uint8_t planId;
	uint16_t count;

	orderIndex = orderSlot + 4 * regionIdx;
	planId = g_builtinPlanIdByNameIndex
		[g_orderLeaderBuiltinPlanNameIndex
			 [g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				  .fg.orders[orderIndex]
				  .order]];

	if (strcmp(g_planTable[planId].name, "disableldr1pln") == 0) {
		g_paiContext.aiRequireLiveOrderTarget = 1;
	} else {
		g_paiContext.aiRequireLiveOrderTarget = 0;
	}
	g_paiContext.aiTargetSearchFlags = 2;

	if (strcmp(g_planTable[planId].name, "capescortersldr1pln") == 0) {
		if (paifight_FindEscortLeaderTargetFromOrder(orderSlot, regionIdx) == 0xffffu) {
			return false;
		}
		return true;
	}

	if (strcmp(g_planTable[planId].name, "inspectldr1pln") == 0) {
		count = paifight_CountHiddenInspectTargets(
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[orderIndex]
				.target1Type,
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[orderIndex]
				.target1,
			(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[orderIndex]
				.target1OrTarget2,
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[orderIndex]
				.target2Type,
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[orderIndex]
				.target2);
		if (count == 0xffffu) {
			count = paifight_CountHiddenInspectTargets(
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.orders[orderIndex]
					.secondaryTargetTypes[XWA_ORDER_TARGET_3],
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.orders[orderIndex]
					.secondaryTargets[XWA_ORDER_TARGET_3],
				(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.orders[orderIndex]
					.target3OrTarget4,
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.orders[orderIndex]
					.secondaryTargetTypes[XWA_ORDER_TARGET_4],
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.orders[orderIndex]
					.secondaryTargets[XWA_ORDER_TARGET_4]);
		}

		if (count == 0xffffu) {
			return false;
		}
		return true;
	}

	count = paifight_CountRemainingOrderTargets(
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target1Type,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target1,
		(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target1OrTarget2,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target2Type,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target2);
	if (count == 0xffffu) {
		count = paifight_CountRemainingOrderTargets(
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[orderIndex]
				.secondaryTargetTypes[XWA_ORDER_TARGET_3],
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[orderIndex]
				.secondaryTargets[XWA_ORDER_TARGET_3],
			(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[orderIndex]
				.target3OrTarget4,
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[orderIndex]
				.secondaryTargetTypes[XWA_ORDER_TARGET_4],
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[orderIndex]
				.secondaryTargets[XWA_ORDER_TARGET_4]);
	}

	if (count == 0xffffu) {
		return false;
	}
	return true;
}

// FUNCTION: XWA 0x4AB530
bool paifight_HasFutureFgTargets(uint16_t target1Type, uint16_t target1, int16_t targetRelationOp,
								 uint16_t target2Type, uint16_t target2) {
	uint16_t fgIdx;

	if (target1Type == 0 && target2Type == 0) {
		return false;
	}

	for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		if (g_missionFgStats[fgIdx].arrivalEnabled || g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) {
			uint16_t matchesTarget1;
			uint16_t matchesTarget2;

			matchesTarget1 = Mission_FlightGroupMatchesTriggerVariable(fgIdx, target1Type, target1);
			matchesTarget2 = Mission_FlightGroupMatchesTriggerVariable(fgIdx, target2Type, target2);

			if (targetRelationOp == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if ((int16_t)matchesTarget1 != 0 &&
				(!g_missionFgStats[fgIdx].hasArrived || g_missionFgStats[fgIdx].wavesRemaining)) {
				return true;
			}
		}
	}

	return false;
}

// FUNCTION: XWA 0x4A71D0
int16_t paifight_CountHiddenInspectTargets(uint16_t target1Type, uint16_t target1, int16_t targetRelationOp,
										   uint16_t target2Type, uint16_t target2) {
	int hiddenCount;
	int team;

	if (target1Type == 0 && target2Type == 0) {
		return -1;
	}

	hiddenCount = 0;
	team = g_objectTable[g_paiContext.aiObjIdx].mobj->team;
	{
		uint32_t regionIdx;

		for (regionIdx = 0; regionIdx < (uint32_t)g_activeMissionRegionCount; ++regionIdx) {
			uint32_t regionSlotsPerRegion;
			uint32_t craftSlotsPerRegion;
			uint32_t regionBase;
			uint32_t regionCraftEnd;
			uint16_t objectIdx;

			regionSlotsPerRegion = g_regionObjectSlotEnd / (uint32_t)g_missionRegionCount;
			craftSlotsPerRegion = g_craftObjectSlotsTotal / (uint32_t)g_missionRegionCount;

			regionBase = regionIdx * regionSlotsPerRegion;
			regionCraftEnd = regionBase + craftSlotsPerRegion;
			objectIdx = (uint16_t)regionBase;
			while ((uint32_t)objectIdx < regionCraftEnd) {
				if (g_objectTable[objectIdx].objectType != OBJ_None &&
					g_objectTable[objectIdx].flightGroupIdx !=
						g_paiContext.curOrderCoord.fields.flightGroupIdx) {
					int matchesTarget1;
					int matchesTarget2;

					matchesTarget1 = Mission_ObjectMatchesTriggerVariable(objectIdx, target1Type, target1);
					matchesTarget2 = Mission_ObjectMatchesTriggerVariable(objectIdx, target2Type, target2);
					if (targetRelationOp == 1) {
						matchesTarget1 |= matchesTarget2;
					} else {
						matchesTarget1 &= matchesTarget2;
					}

					if ((int16_t)matchesTarget1 && pai_IsObjectTargetable(objectIdx)) {
						if (g_objectTable[objectIdx].mobj != NULL &&
							(int8_t)g_objectTable[objectIdx].mobj->pCraft->iffVisibility[team] <= 0) {
							++hiddenCount;
						}
					}
				}

				++objectIdx;
			}
		}
	}

	if (hiddenCount == 0) {
		return -1;
	}

	return (int16_t)hiddenCount;
}

// FUNCTION: XWA 0x4A6780
int16_t paifight_FindNearestUninspectedOrderTarget(uint16_t target1Type, uint16_t target1,
												   int16_t targetOrMode, uint16_t target2Type,
												   uint16_t target2) {
	uint32_t bestRangeScore;
	uint16_t bestObjIdx;
	uint16_t objectIdx;

	if ((uint16_t)target1Type == 0 && (uint16_t)target2Type == 0) {
		return -1;
	}

	bestObjIdx = 0xffffu;
	bestRangeScore = 0xffffffffu;
	for (objectIdx = (uint16_t)g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		if (g_objectTable[objectIdx].objectType != OBJ_None &&
			g_objectTable[objectIdx].flightGroupIdx != g_paiContext.curOrderCoord.fields.flightGroupIdx) {
			int matchesTarget1;
			int matchesTarget2;

			matchesTarget1 = Mission_ObjectMatchesTriggerVariable(objectIdx, target1Type, target1);
			matchesTarget2 = Mission_ObjectMatchesTriggerVariable(objectIdx, target2Type, target2);
			if (targetOrMode == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if ((uint16_t)matchesTarget1 &&
				objectIdx != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx &&
				pai_IsObjectTargetable(objectIdx) &&
				(int8_t)g_objectTable[objectIdx]
						.mobj->pCraft->iffVisibility[g_objectTable[g_paiContext.aiObjIdx].mobj->team] <= 0 &&
				((g_paiContext.aiTargetSearchFlags & 4u) == 0 ||
				 (uint8_t)pai_IsObjectWithinCurrentOrderRange(objectIdx)) &&
				((g_paiContext.aiTargetSearchFlags & 1u) == 0 ||
				 (uint16_t)paifight_TargetHasAttackCapacity(objectIdx, 0))) {
				uint32_t rangeScore;

				pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, objectIdx);
				if ((uint32_t)g_targetRangeScore <= 0x4000u ||
					(uint8_t)Object_HasActiveDecoyBeam(objectIdx) != 1) {
					rangeScore = (uint32_t)g_targetRangeScore;
					if (rangeScore < bestRangeScore) {
						bestRangeScore = rangeScore;
						bestObjIdx = objectIdx;
					}
				}
			}
		}
	}

	return (int16_t)bestObjIdx;
}

// FUNCTION: XWA 0x4A66B0
int16_t paifight_FindInspectOrderTargetFromOrder(uint8_t orderSlot, uint8_t regionIdx) {
	unsigned int orderIndex;
	int16_t result;

	orderIndex = orderSlot + 4 * regionIdx;
	result = paifight_FindNearestUninspectedOrderTarget(
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target1Type,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target1,
		(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target1OrTarget2,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target2Type,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target2);
	if (result != -1) {
		return result;
	}

	return paifight_FindNearestUninspectedOrderTarget(
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.secondaryTargetTypes[XWA_ORDER_TARGET_3],
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.secondaryTargets[XWA_ORDER_TARGET_3],
		(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target3OrTarget4,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.secondaryTargetTypes[XWA_ORDER_TARGET_4],
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.secondaryTargets[XWA_ORDER_TARGET_4]);
}

// FUNCTION: XWA 0x4A6F80
int16_t paifight_CountRemainingOrderTargets(uint16_t target1Type, uint16_t target1, int16_t targetRelationOp,
											uint16_t target2Type, uint16_t target2) {
	int remainingCount;
	uint32_t regionIdx;

	if ((uint16_t)target1Type == 0 && (uint16_t)target2Type == 0) {
		return -1;
	}

	remainingCount = 0;
	for (regionIdx = 0; regionIdx < (uint32_t)g_activeMissionRegionCount; ++regionIdx) {
		uint32_t craftEnd;
		uint32_t staticStart;
		uint32_t staticEnd;
		uint32_t regionBase;
		uint16_t objectIdx;

		regionBase = regionIdx * (g_regionObjectSlotEnd / (uint32_t)g_missionRegionCount);
		craftEnd = regionBase + g_craftObjectSlotsTotal / (uint32_t)g_missionRegionCount;
		staticStart = regionBase + g_regionMainObjectSlotsTotal / (uint32_t)g_missionRegionCount;
		staticEnd = staticStart + g_regionStaticObjectSlotsTotal / (uint32_t)g_missionRegionCount;

		objectIdx = (uint16_t)regionBase;
		while ((uint32_t)objectIdx < craftEnd) {
			ObjectRecord* object;

			object = &g_objectTable[objectIdx];
			if (object->objectType != OBJ_None &&
				object->flightGroupIdx != g_paiContext.curOrderCoord.fields.flightGroupIdx) {
				int matchesTarget1;
				int matchesTarget2;

				matchesTarget1 = Mission_ObjectMatchesTriggerVariable(objectIdx, target1Type, target1);
				matchesTarget2 = Mission_ObjectMatchesTriggerVariable(objectIdx, target2Type, target2);
				if (targetRelationOp == 1) {
					matchesTarget1 |= matchesTarget2;
				} else {
					matchesTarget1 &= matchesTarget2;
				}

				if ((int16_t)matchesTarget1 && pai_IsObjectTargetable(objectIdx)) {
					MobileObject* mobj;

					mobj = object->mobj;
					if (mobj != NULL) {
						CraftData* craft;

						craft = mobj->pCraft;
						if (!g_paiContext.aiRequireLiveOrderTarget || craft->workingSubsystems) {
							if (!g_paiContext.aiRequireLiveOrderTarget || !craft->wasCaptured ||
								g_objectTable[g_paiContext.aiObjIdx].mobj->team != mobj->team) {
								++remainingCount;
							}
						}
					}
				}
			}

			++objectIdx;
		}

		objectIdx = (uint16_t)staticStart;
		while ((uint32_t)objectIdx < staticEnd) {
			if (g_objectTable[objectIdx].objectType != OBJ_None &&
				(g_modelTypeTable[(uint16_t)g_objectTable[objectIdx].objectType].flags & 2u) != 0) {
				uint16_t flightGroupIdx;
				int matchesTarget1;
				int matchesTarget2;

				flightGroupIdx = g_objectTable[objectIdx].flightGroupIdx;
				matchesTarget1 =
					Mission_FlightGroupMatchesTriggerVariable(flightGroupIdx, target1Type, target1);
				matchesTarget2 =
					Mission_FlightGroupMatchesTriggerVariable(flightGroupIdx, target2Type, target2);
				if (targetRelationOp == 1) {
					matchesTarget1 |= matchesTarget2;
				} else {
					matchesTarget1 &= matchesTarget2;
				}

				if ((int16_t)matchesTarget1 && pai_IsObjectTargetable(objectIdx)) {
					++remainingCount;
				}
			}

			++objectIdx;
		}
	}

	if (remainingCount == 0) {
		return -1;
	}

	return (int16_t)remainingCount;
}

// FUNCTION: XWA 0x4AB700
char paifight_scanforplayerinspecttypeorder(void) {
	uint16_t targetObjIdx;

	targetObjIdx = (uint16_t)paifight_FindNearestUninspectedOrderTarget(
		TRIGVAR_SHIP_TYPE, g_curCraft->playerCommandCraftTypeFilter, 0, TRIGVAR_TEAM,
		g_curCraft->playerCommandTeamFilter);
	if (targetObjIdx != 0xffffu) {
		g_paiContext.aiController->targetObjIdx = targetObjIdx;
		g_paiContext.aiController->targetSignature = g_objectTable[targetObjIdx].objectSignature;
		g_paiContext.aiController->hasLiveTarget = 1;
		return 1;
	} else {
		return 0;
	}
}

// FUNCTION: XWA 0x4AA790
int16_t paifight_FindNearestMatchingTargetFromOrigin(MissionTriggerVariableType target1Type, uint16_t target1,
													 int16_t target1OrTarget2,
													 MissionTriggerVariableType target2Type, uint16_t target2,
													 int16_t requireClearSweep) {
	uint32_t bestRangeScore;
	uint16_t bestObjIdx;
	uint16_t objectIdx;

	if ((uint16_t)target1Type == 0 && (uint16_t)target2Type == 0) {
		return -1;
	}

	objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
	bestObjIdx = 0xffffu;
	bestRangeScore = UINT32_MAX;
	while ((uint32_t)objectIdx < g_activeRegionCraftObjectSlotEnd) {
		if (g_objectTable[objectIdx].objectType != OBJ_None) {
			int matchesTarget1;
			int matchesTarget2;

			matchesTarget1 = Mission_ObjectMatchesTriggerVariable(objectIdx, target1Type, target1);
			matchesTarget2 = Mission_ObjectMatchesTriggerVariable(objectIdx, target2Type, target2);
			if (target1OrTarget2 == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if ((int16_t)matchesTarget1) {
				CraftData* craft;

				craft = g_objectTable[objectIdx].mobj->pCraft;
				if ((!g_paiContext.aiRequireLiveOrderTarget || craft->workingSubsystems) &&
					pai_IsObjectTargetable(objectIdx)) {
					uint32_t rangeScore;

					rangeScore = (uint32_t)collide_roughdistance3d(
						g_objectTable[objectIdx].world_x - g_paifightSearchOriginX,
						g_objectTable[objectIdx].world_y - g_paifightSearchOriginY,
						g_objectTable[objectIdx].world_z - g_paifightSearchOriginZ);
					g_targetRangeScore = (int)rangeScore;
					if (rangeScore < bestRangeScore) {
						if (requireClearSweep != 0) {
							Mission_ResolveObjectOrMissionPointWorldLoc(objectIdx, 0, 0, 0);
							g_collisionProbeWorldX = worldlocx;
							g_collisionProbeWorldY = worldlocy;
							g_collisionProbeWorldZ = worldlocz;
							if (collide_CheckSweptModelCollision(g_paiContext.aiObjIdx,
																 g_paiContext.aiObjIdx) == 0) {
								bestRangeScore = (uint32_t)g_targetRangeScore;
								bestObjIdx = (int16_t)objectIdx;
							}
						} else {
							bestRangeScore = rangeScore;
							bestObjIdx = (int16_t)objectIdx;
						}
					}
				}
			}
		}

		++objectIdx;
	}

	objectIdx = (uint16_t)g_objScanStart;
	while ((uint32_t)objectIdx < g_regionStaticObjectSlotEnd) {
		if (g_objectTable[objectIdx].objectType != OBJ_None &&
			(g_modelTypeTable[(uint16_t)g_objectTable[objectIdx].objectType].flags & 2u) != 0) {
			int matchesTarget1;
			int matchesTarget2;

			matchesTarget1 = Mission_ObjectMatchesTriggerVariable(objectIdx, target1Type, target1);
			matchesTarget2 = Mission_ObjectMatchesTriggerVariable(objectIdx, target2Type, target2);
			if (target1OrTarget2 == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if ((int16_t)matchesTarget1 && pai_IsObjectTargetable(objectIdx)) {
				uint32_t rangeScore;

				Mission_ResolveObjectOrMissionPointWorldLoc(objectIdx, 0, 0, 0);
				rangeScore = (uint32_t)collide_roughdistance3d(worldlocx - g_paifightSearchOriginX,
															   worldlocy - g_paifightSearchOriginY,
															   worldlocz - g_paifightSearchOriginZ);
				g_targetRangeScore = (int)rangeScore;
				if (rangeScore < bestRangeScore) {
					if (requireClearSweep != 0) {
						g_collisionProbeWorldX = worldlocx;
						g_collisionProbeWorldY = worldlocy;
						g_collisionProbeWorldZ = worldlocz;
						if (collide_CheckSweptModelCollision(g_paiContext.aiObjIdx, g_paiContext.aiObjIdx) ==
							0) {
							bestRangeScore = (uint32_t)g_targetRangeScore;
							bestObjIdx = (int16_t)objectIdx;
						}
					} else {
						bestRangeScore = rangeScore;
						bestObjIdx = (int16_t)objectIdx;
					}
				}
			}
		}

		++objectIdx;
	}

	if (bestRangeScore > 0x10000u) {
		bestObjIdx = 0xffffu;
	}

	return (int16_t)bestObjIdx;
}

// FUNCTION: XWA 0x4A6190
int16_t paifight_FindNearestAttackerOfMatchingTarget(uint16_t target1Type, uint16_t target1,
													 int16_t targetOrMode, uint16_t target2Type,
													 uint16_t target2) {
	uint32_t bestRangeScore;
	uint16_t bestObjIdx;
	uint16_t targetObjIdx;

	if ((uint16_t)target1Type == 0 && (uint16_t)target2Type == 0) {
		return -1;
	}

	bestObjIdx = 0xffffu;
	bestRangeScore = UINT32_MAX;
	for (targetObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
		 (uint32_t)targetObjIdx < g_activeRegionCraftObjectSlotEnd; ++targetObjIdx) {
		if (g_objectTable[targetObjIdx].objectType != OBJ_None) {
			int16_t matchesTarget1;
			int16_t matchesTarget2;

			matchesTarget1 = Mission_ObjectMatchesTriggerVariable(targetObjIdx, target1Type, target1);
			matchesTarget2 = Mission_ObjectMatchesTriggerVariable(targetObjIdx, target2Type, target2);
			if (targetOrMode == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if (matchesTarget1) {
				if ((g_paiContext.aiTargetSearchFlags & 0x20u) != 0 &&
					g_objectTable[targetObjIdx].genusId != 0) {
					uint32_t projectileRangeLimit;
					uint16_t bestProjectileObjIdx;
					uint16_t projectileObjIdx;

					projectileRangeLimit = g_paiContext.aiTargetSearchRangeLimit;
					bestProjectileObjIdx = 0xffffu;
					if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
						projectileRangeLimit +=
							(uint32_t)
								g_modelTypeTable[(uint16_t)g_objectTable[g_paiContext.aiObjIdx].objectType]
									.maxBoundsExtent;
					}

					for (projectileObjIdx = (uint16_t)g_projectileObjectSlotStart;
						 (uint32_t)projectileObjIdx < g_projectileObjectSlotEnd; ++projectileObjIdx) {
						ObjectRecord* projectileObject;
						uint16_t projectileType;

						projectileType = g_objectTable[projectileObjIdx].objectType;
						projectileObject = &g_objectTable[projectileObjIdx];
						if (projectileType != OBJ_None && projectileObject->genusId != GENUS_Explosion &&
#ifdef XWA_MODERN
							projectileType >= OBJ_LaserRebel && projectileType <= OBJ_LaserImperialDS &&
							g_projectileWarheadClassByType[projectileType - OBJ_LaserRebel] != 0 &&
#else
							((const uint8_t*)&g_projectileDamageByType[14])[projectileType] != 0 &&
#endif
							projectileObject->mobj->pWarheadGuidance->targetObjIdx == targetObjIdx) {
							uint32_t rangeScore;

							rangeScore = (uint32_t)collide_roughdistance3d(
								projectileObject->world_x - g_paifightSearchOriginX,
								projectileObject->world_y - g_paifightSearchOriginY,
								projectileObject->world_z - g_paifightSearchOriginZ);
							g_targetRangeScore = (int)rangeScore;
							if (rangeScore < projectileRangeLimit) {
								int turretTargetCount;
								unsigned int laserSlotCount;
								WarheadInventoryEntry* weaponSlot;

								turretTargetCount = 0;
								laserSlotCount = g_curCraft->laserSlotCount;
								weaponSlot = g_curCraft->warheadData;
								while (laserSlotCount > 0) {
									if (weaponSlot->turretTargetObjIdx == (int16_t)projectileObjIdx) {
										++turretTargetCount;
									}
									++weaponSlot;
									--laserSlotCount;
								}

								if (turretTargetCount == 0) {
									if (!g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
										Mission_ResolveObjectOrMissionPointWorldLoc(projectileObjIdx, 0, 0,
																					0);
										g_collisionProbeWorldX = worldlocx;
										g_collisionProbeWorldY = worldlocy;
										g_collisionProbeWorldZ = worldlocz;
										g_collisionStagedModelProbe = 1;
										if (collide_CheckSweptModelCollision(g_paiContext.aiObjIdx,
																			 g_paiContext.aiObjIdx) != 0) {
											g_collisionStagedModelProbe = 0;
											continue;
										}
										g_collisionStagedModelProbe = 0;
										rangeScore = (uint32_t)g_targetRangeScore;
									}

									projectileRangeLimit = rangeScore;
									bestProjectileObjIdx = projectileObjIdx;
								}
							}
						}
					}

					{
						int16_t projectileResult;

						projectileResult = (int16_t)bestProjectileObjIdx;
						if (projectileResult != -1) {
							return projectileResult;
						}
					}
				}

				{
					CraftData* targetCraft;
					int16_t hasAttackedByTeam;
					int remainingTeams;
					char* attackedByTeam;

					hasAttackedByTeam = 0;
					targetCraft = g_objectTable[targetObjIdx].mobj->pCraft;
					remainingTeams = 10;
					attackedByTeam = targetCraft->attackedByTeam;
					while (remainingTeams != 0) {
						if (*attackedByTeam != 0) {
							hasAttackedByTeam = 1;
						}
						++attackedByTeam;
						--remainingTeams;
					}

					if (hasAttackedByTeam) {
						uint16_t attackerObjIdx;

						for (attackerObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
							 (uint32_t)attackerObjIdx < g_activeRegionCraftObjectSlotEnd; ++attackerObjIdx) {
							if (g_objectTable[attackerObjIdx].objectType != OBJ_None) {
								AiController* attackerAi;
								int16_t maneuverMode;
								int isAttackingTarget;

								attackerAi =
									pai_GetEffectiveAIController(g_objectTable[attackerObjIdx].mobj->pCraft);
								maneuverMode = attackerAi->maneuverMode;
								isAttackingTarget =
									((maneuverMode == 11 || maneuverMode == 12 || maneuverMode == 23) &&
									 attackerAi->targetObjIdx == targetObjIdx) ||
									targetCraft->aiFlight.threatObjIdx == attackerObjIdx;
								if (isAttackingTarget &&
									strcmp(g_planTable[attackerAi->currentPlanId].name, "inspectldr1pln") !=
										0 &&
									targetObjIdx != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx &&
									pai_IsObjectTargetable(attackerObjIdx)) {
									if ((g_objectTable[attackerObjIdx].playerOwnerIdx == -1 ||
										 !Object_IsFriendlyToTeam(
											 attackerObjIdx,
											 g_objectTable[g_paiContext.aiObjIdx].mobj->team)) &&
										((g_paiContext.aiTargetSearchFlags & 4u) == 0 ||
										 pai_IsObjectWithinCurrentOrderRange(attackerObjIdx)) &&
										((g_paiContext.aiTargetSearchFlags & 1u) == 0 ||
										 (uint16_t)paifight_TargetHasAttackCapacity(attackerObjIdx, 0xffu))) {
										if ((g_paiContext.aiTargetSearchFlags & 0x60u) != 0) {
											g_targetRangeScore = collide_roughdistance3d(
												g_objectTable[attackerObjIdx].world_x -
													g_paiContext.aiTargetSearchOriginX,
												g_objectTable[attackerObjIdx].world_y -
													g_paiContext.aiTargetSearchOriginY,
												g_objectTable[attackerObjIdx].world_z -
													g_paiContext.aiTargetSearchOriginZ);
										} else {
											pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx,
																				attackerObjIdx);
										}

										if ((g_paiContext.aiTargetSearchFlags & 0x10u) == 0 ||
											((uint32_t)g_targetRangeScore <=
												 g_paiContext.aiTargetSearchRangeLimit &&
											 !Object_IsFriendlyToTeam(
												 attackerObjIdx,
												 g_objectTable[g_paiContext.aiObjIdx].mobj->team))) {
											if ((uint32_t)g_targetRangeScore < bestRangeScore) {
												bestRangeScore = (uint32_t)g_targetRangeScore;
												bestObjIdx = attackerObjIdx;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return (int16_t)bestObjIdx;
}

// FUNCTION: XWA 0x4A60C0
int16_t paifight_FindAttackerOfOrderTargetFromOrder(uint8_t orderSlot, uint8_t regionIdx) {
	unsigned int orderIndex;
	uint16_t target1Type;
	uint16_t target1;
	int16_t targetOrMode;
	uint16_t target2Type;
	uint16_t target2;
	int16_t result;

	orderIndex = orderSlot + 4 * regionIdx;
	target2 =
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target2;
	target2Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .target2Type;
	targetOrMode = (int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					   .fg.orders[orderIndex]
					   .target1OrTarget2;
	target1 =
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target1;
	target1Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .target1Type;
	result = paifight_FindNearestAttackerOfMatchingTarget(target1Type, target1, targetOrMode, target2Type,
														  target2);
	if (result != -1) {
		return result;
	}

	target2 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				  .fg.orders[orderIndex]
				  .secondaryTargets[XWA_ORDER_TARGET_4];
	target2Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .secondaryTargetTypes[XWA_ORDER_TARGET_4];
	targetOrMode = (int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					   .fg.orders[orderIndex]
					   .target3OrTarget4;
	target1 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				  .fg.orders[orderIndex]
				  .secondaryTargets[XWA_ORDER_TARGET_3];
	target1Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .secondaryTargetTypes[XWA_ORDER_TARGET_3];
	return paifight_FindNearestAttackerOfMatchingTarget(target1Type, target1, targetOrMode, target2Type,
														target2);
}

static __inline int16_t paifight_ObjectMatchesTargetPair(uint16_t objIdx,
														 MissionTriggerVariableType target1Type,
														 uint16_t target1, int16_t targetOrMode,
														 MissionTriggerVariableType target2Type,
														 uint16_t target2) {
	int16_t matchesTarget1;
	int16_t matchesTarget2;

	matchesTarget1 = Mission_ObjectMatchesTriggerVariable(objIdx, target1Type, target1);
	matchesTarget2 = Mission_ObjectMatchesTriggerVariable(objIdx, target2Type, target2);
	if (targetOrMode == 1) {
		return matchesTarget1 | matchesTarget2;
	}

	return matchesTarget1 & matchesTarget2;
}

static __inline int16_t paifight_ObjectMatchesTargetPairKeepFirst(uint16_t objIdx,
																  MissionTriggerVariableType target1Type,
																  uint16_t target1, int16_t targetOrMode,
																  MissionTriggerVariableType target2Type,
																  uint16_t target2) {
	int16_t matchesTarget1;
	int16_t matchesTarget2;

	matchesTarget1 = Mission_ObjectMatchesTriggerVariable(objIdx, target1Type, target1);
	matchesTarget2 = Mission_ObjectMatchesTriggerVariable(objIdx, target2Type, target2);
	if (targetOrMode == 1) {
		matchesTarget1 |= matchesTarget2;
	} else {
		matchesTarget1 &= matchesTarget2;
	}

	return matchesTarget1;
}

static __inline int paifight_AttackTargetPassesLiveFilter(ObjectRecord* object) {
	MobileObject* mobj;
	CraftData* craft;

	mobj = object->mobj;
	craft = mobj->pCraft;
	if (!g_paiContext.aiRequireLiveOrderTarget) {
		return 1;
	}

	if (craft->workingSubsystems && (!g_paiContext.aiRequireLiveOrderTarget || !craft->wasCaptured ||
									 g_objectTable[g_paiContext.aiObjIdx].mobj->team != mobj->team)) {
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4A5800
int16_t paifight_FindNearestAttackOrderTarget(uint16_t target1Type, uint16_t target1, int16_t targetOrMode,
											  uint16_t target2Type, uint16_t target2) {
	uint32_t candidateCount;
	uint32_t bestRangeScore;
	uint32_t bestObjIdx;
	uint16_t objectIdx;

	if ((uint16_t)target1Type == 0 && (uint16_t)target2Type == 0) {
		return -1;
	}

	candidateCount = 0;
	objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
	while ((uint32_t)objectIdx < g_activeRegionCraftObjectSlotEnd) {
		if (g_objectTable[objectIdx].objectType != OBJ_None &&
			g_objectTable[objectIdx].flightGroupIdx != g_paiContext.curOrderCoord.fields.flightGroupIdx &&
			paifight_ObjectMatchesTargetPairKeepFirst(objectIdx, target1Type, target1, targetOrMode,
													  target2Type, target2) &&
			objectIdx != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx &&
			pai_IsObjectTargetable(objectIdx) &&
			paifight_AttackTargetPassesLiveFilter(&g_objectTable[objectIdx]) &&
			((g_paiContext.aiTargetSearchFlags & 4u) == 0 ||
			 (uint8_t)pai_IsObjectWithinCurrentOrderRange(objectIdx))) {
			++candidateCount;
		}

		++objectIdx;
	}

	objectIdx = (uint16_t)g_objScanStart;
	while ((uint32_t)objectIdx < g_regionStaticObjectSlotEnd) {
		ObjectRecord* object;

		object = &g_objectTable[objectIdx];
		if (object->objectType != OBJ_None &&
			(g_modelTypeTable[(uint16_t)object->objectType].flags & 2u) != 0 &&
			paifight_ObjectMatchesTargetPairKeepFirst(objectIdx, target1Type, target1, targetOrMode,
													  target2Type, target2) &&
			objectIdx != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx &&
			pai_IsObjectTargetable(objectIdx) &&
			((g_paiContext.aiTargetSearchFlags & 4u) == 0 ||
			 (uint8_t)pai_IsObjectWithinCurrentOrderRange(objectIdx))) {
			++candidateCount;
		}

		++objectIdx;
	}

	if (candidateCount == 0) {
		return -1;
	}

	bestObjIdx = 0xffffu;
	bestRangeScore = UINT32_MAX;
	if (!strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "homing1pln")) {
		bestRangeScore = 0x20000u;
	}

	objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
	while ((uint32_t)objectIdx < g_activeRegionCraftObjectSlotEnd) {
		ObjectRecord* object;

		object = &g_objectTable[objectIdx];
		if (object->objectType != OBJ_None &&
			object->flightGroupIdx != g_paiContext.curOrderCoord.fields.flightGroupIdx &&
			paifight_ObjectMatchesTargetPair(objectIdx, target1Type, target1, targetOrMode, target2Type,
											 target2) &&
			objectIdx != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx &&
			pai_IsObjectTargetable(objectIdx) && paifight_AttackTargetPassesLiveFilter(object) &&
			((g_paiContext.aiTargetSearchFlags & 4u) == 0 ||
			 (uint8_t)pai_IsObjectWithinCurrentOrderRange(objectIdx)) &&
			((g_paiContext.aiTargetSearchFlags & 1u) == 0 ||
			 (uint16_t)paifight_TargetHasAttackCapacity(objectIdx, (uint16_t)candidateCount))) {
			uint32_t rangeScore;

			if ((g_paiContext.aiTargetSearchFlags & 0x60u) != 0) {
				rangeScore =
					(uint32_t)collide_roughdistance3d(object->world_x - g_paiContext.aiTargetSearchOriginX,
													  object->world_y - g_paiContext.aiTargetSearchOriginY,
													  object->world_z - g_paiContext.aiTargetSearchOriginZ);
				g_targetRangeScore = (int)rangeScore;
			} else {
				pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, objectIdx);
				rangeScore = (uint32_t)g_targetRangeScore;
			}

			if (rangeScore <= 0x4000u || (uint8_t)Object_HasActiveDecoyBeam(objectIdx) != 1) {
				uint16_t objectType;

				rangeScore = (uint32_t)g_targetRangeScore;
				objectType = object->objectType;
				if (objectType != OBJ_RebelPilot && objectType != OBJ_ImperialPilot &&
					objectType != OBJ_CivilianPilot && rangeScore < bestRangeScore) {
					uint32_t bestObjectIdxValue;

					bestObjectIdxValue = objectIdx;
					bestRangeScore = rangeScore;
					bestObjIdx = bestObjectIdxValue;
				}
			}
		}

		++objectIdx;
	}

	{
		unsigned int objectIdxWide;

		objectIdx = (uint16_t)g_objScanStart;
		for (objectIdxWide = objectIdx; objectIdxWide < g_regionStaticObjectSlotEnd;
			 objectIdxWide = ++objectIdx) {
			ObjectRecord* object;

			object = &g_objectTable[objectIdxWide];
			if (object->objectType != OBJ_None &&
				(g_modelTypeTable[(uint16_t)object->objectType].flags & 2u) != 0 &&
				paifight_ObjectMatchesTargetPairKeepFirst(objectIdx, target1Type, target1, targetOrMode,
														  target2Type, target2) &&
				objectIdx != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx &&
				pai_IsObjectTargetable(objectIdxWide) &&
				((g_paiContext.aiTargetSearchFlags & 4u) == 0 ||
				 (uint8_t)pai_IsObjectWithinCurrentOrderRange(objectIdx)) &&
				((g_paiContext.aiTargetSearchFlags & 1u) == 0 ||
				 (uint16_t)paifight_TargetHasAttackCapacity(objectIdx, (uint16_t)candidateCount))) {
				pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, objectIdxWide);
				if ((uint32_t)g_targetRangeScore < bestRangeScore) {
					uint32_t bestObjectIdxValue;

					bestObjectIdxValue = objectIdx;
					bestRangeScore = (uint32_t)g_targetRangeScore;
					bestObjIdx = bestObjectIdxValue;
				}
			}
		}
	}

	return (int16_t)bestObjIdx;
}

// FUNCTION: XWA 0x4A5730
int16_t paifight_FindAttackOrderTargetFromOrder(uint8_t orderSlot, uint8_t regionIdx) {
	unsigned int orderIndex;
	int16_t result;

	orderIndex = orderSlot + 4 * regionIdx;
	result = paifight_FindNearestAttackOrderTarget(
		(MissionTriggerVariableType)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target1Type,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target1,
		(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target1OrTarget2,
		(MissionTriggerVariableType)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target2Type,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target2);
	if (result != -1) {
		return result;
	}

	return paifight_FindNearestAttackOrderTarget(
		(MissionTriggerVariableType)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.secondaryTargetTypes[XWA_ORDER_TARGET_3],
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.secondaryTargets[XWA_ORDER_TARGET_3],
		(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.target3OrTarget4,
		(MissionTriggerVariableType)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.secondaryTargetTypes[XWA_ORDER_TARGET_4],
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.secondaryTargets[XWA_ORDER_TARGET_4]);
}

// FUNCTION: XWA 0x4AB680
char paifight_scanforplayertargettypeorder(void) {
	uint16_t targetObjIdx;

	targetObjIdx = (uint16_t)paifight_FindNearestAttackOrderTarget(
		TRIGVAR_SHIP_TYPE, g_curCraft->playerCommandCraftTypeFilter, 0, TRIGVAR_TEAM,
		g_curCraft->playerCommandTeamFilter);
	if (targetObjIdx != 0xffffu) {
		g_paiContext.aiController->targetObjIdx = targetObjIdx;
		g_paiContext.aiController->targetSignature = g_objectTable[targetObjIdx].objectSignature;
		g_paiContext.aiController->hasLiveTarget = 1;
		g_paiContext.aiController->candidateTargetIdx = targetObjIdx;
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4A6AF0
bool paifight_OrderSlotCanTarget(uint8_t orderSlot, uint8_t regionIdx) {
	const XwaOrder* order;
	uint8_t planId;
	const char* planName;
	int16_t targetObjIdx;

	order = &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				 .fg.orders[4 * regionIdx + orderSlot];
	planId = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order->order]];
	planName = g_planTable[planId].name;

	if (strcmp(planName, "disableldr1pln") == 0) {
		g_paiContext.aiRequireLiveOrderTarget = 1;
	} else {
		g_paiContext.aiRequireLiveOrderTarget = 0;
	}
	g_paiContext.aiTargetSearchFlags = 7;

	if (strcmp(planName, "capfreeldr1pln") == 0 || strcmp(planName, "disableldr1pln") == 0 ||
		strcmp(planName, "followtarget1pln") == 0 || strcmp(planName, "kamikaze1pln") == 0) {
		targetObjIdx = paifight_FindAttackOrderTargetFromOrder(orderSlot, regionIdx);
		if (targetObjIdx == -1) {
			return false;
		}
		return true;
	}

	if (strcmp(planName, "capescortersldr1pln") == 0) {
		targetObjIdx = (int16_t)paifight_FindEscortLeaderTargetFromOrder(orderSlot, regionIdx);
		if ((uint16_t)targetObjIdx == 0xffffu) {
			return false;
		}
		return true;
	}

	if (strcmp(planName, "inspectldr1pln") == 0) {
		targetObjIdx = paifight_FindInspectOrderTargetFromOrder(orderSlot, regionIdx);
		if (targetObjIdx == -1) {
			return false;
		}
		return true;
	}

	targetObjIdx = paifight_FindAttackerOfOrderTargetFromOrder(orderSlot, regionIdx);
	if (targetObjIdx == -1) {
		return false;
	}
	return true;
}

// FUNCTION: XWA 0x4AB2B0
int16_t paifight_searchforclosestingroup(uint16_t target1Type, uint16_t target1, int16_t targetRelationOp,
										 uint16_t target2Type, uint16_t target2) {
	int fgIdx;
	uint32_t bestScore;
	uint16_t bestObjIdx;

	if ((uint16_t)target1Type == 0 && (uint16_t)target2Type == 0) {
		return -1;
	}

	bestScore = UINT32_MAX;
	bestObjIdx = UINT16_MAX;
	for (fgIdx = 0; (uint16_t)fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		int16_t matchesTarget1;
		int16_t matchesTarget2;
		uint16_t objIdx;

		matchesTarget1 = Mission_FlightGroupMatchesTriggerVariable((uint16_t)fgIdx, target1Type, target1);
		matchesTarget2 = Mission_FlightGroupMatchesTriggerVariable((uint16_t)fgIdx, target2Type, target2);
		if (targetRelationOp == 1) {
			matchesTarget1 |= matchesTarget2;
		} else {
			matchesTarget1 &= matchesTarget2;
		}

		if (!matchesTarget1) {
			continue;
		}

		objIdx = (uint16_t)g_activeRegionObjectSlotStart;
		while ((uint32_t)objIdx < g_activeRegionCraftObjectSlotEnd) {
			if (g_objectTable[objIdx].objectType != 0 &&
				g_objectTable[objIdx].flightGroupIdx == (uint16_t)fgIdx) {
				pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, objIdx);
				if ((uint32_t)g_targetRangeScore < bestScore) {
					bestScore = (uint32_t)g_targetRangeScore;
					bestObjIdx = (uint16_t)(uint8_t)objIdx;
					g_aiEscortCandidateFgIdx = (uint8_t)fgIdx;
				}
			}
			++objIdx;
		}

		objIdx = (uint16_t)g_objScanStart;
		while ((uint32_t)objIdx < g_regionStaticObjectSlotEnd) {
			if (g_objectTable[objIdx].objectType != 0 &&
				g_objectTable[objIdx].flightGroupIdx == (uint16_t)fgIdx) {
				pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, objIdx);
				if ((uint32_t)g_targetRangeScore < bestScore) {
					bestScore = (uint32_t)g_targetRangeScore;
					bestObjIdx = (uint16_t)(uint8_t)objIdx;
					g_aiEscortCandidateFgIdx = (uint8_t)fgIdx;
				}
			}
			++objIdx;
		}
	}

	return bestObjIdx;
}

// FUNCTION: XWA 0x4A5EA0
uint16_t paifight_FindNearestEscortLeaderTarget(uint16_t fgTarget1Type, uint16_t fgTarget1,
												int16_t targetOrMode, uint16_t fgTarget2Type,
												uint16_t fgTarget2) {
	uint32_t bestScore;
	uint16_t bestObjIdx;
	int fgIdx;

	if ((uint16_t)fgTarget1Type == 0 && (uint16_t)fgTarget2Type == 0) {
		return 0xffffu;
	}

	bestObjIdx = 0xffffu;
	bestScore = UINT32_MAX;
	fgIdx = 0;
	if ((int16_t)g_missionHeader.numFlightGroups > 0) {
		do {
			int matchesTarget1;
			int matchesTarget2;

			matchesTarget1 = Mission_FlightGroupMatchesTriggerVariable(
				(uint16_t)fgIdx, (MissionTriggerVariableType)fgTarget1Type, fgTarget1);
			matchesTarget2 = Mission_FlightGroupMatchesTriggerVariable(
				(uint16_t)fgIdx, (MissionTriggerVariableType)fgTarget2Type, fgTarget2);
			if (targetOrMode == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if ((uint16_t)matchesTarget1) {
				uint16_t objIdx;

				objIdx = (uint16_t)g_activeRegionObjectSlotStart;
				while ((uint32_t)objIdx < g_activeRegionCraftObjectSlotEnd) {
					if (g_objectTable[objIdx].objectType != OBJ_None) {
						ObjectRecord* obj;
						AiController* effectiveController;

						obj = &g_objectTable[objIdx];
						effectiveController = pai_GetEffectiveAIController(obj->mobj->pCraft);
						if (strcmp(g_planTable[effectiveController->currentPlanId].name, "escortldr1pln") ==
								0 &&
							(uint8_t)effectiveController->escortTargetFG == (uint16_t)fgIdx &&
							objIdx != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx &&
							pai_IsObjectTargetable(objIdx) &&
							((g_paiContext.aiTargetSearchFlags & 4u) == 0 ||
							 (uint8_t)pai_IsObjectWithinCurrentOrderRange(objIdx)) &&
							((g_paiContext.aiTargetSearchFlags & 1u) == 0 ||
							 (uint16_t)paifight_TargetHasAttackCapacity(objIdx, 0xffu))) {
							pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, objIdx);
							if ((uint32_t)g_targetRangeScore < bestScore) {
								bestScore = (uint32_t)g_targetRangeScore;
								bestObjIdx = objIdx;
							}
						}
					}

					++objIdx;
				}
			}

			++fgIdx;
		} while ((uint16_t)fgIdx < (int16_t)g_missionHeader.numFlightGroups);
	}

	if (bestObjIdx != 0xffffu) {
		g_paiContext.aiController->targetObjIdx = bestObjIdx;
		g_paiContext.aiController->targetSignature = g_objectTable[bestObjIdx].objectSignature;
		g_paiContext.aiController->hasLiveTarget = 1;
	}

	return bestObjIdx;
}

// FUNCTION: XWA 0x4A5DD0
uint16_t paifight_FindEscortLeaderTargetFromOrder(uint8_t orderSlot, uint8_t regionIdx) {
	unsigned int orderIndex;
	uint16_t target1Type;
	uint16_t target1;
	int16_t targetOrMode;
	uint16_t target2Type;
	uint16_t target2;
	uint16_t result;

	orderIndex = orderSlot + 4 * regionIdx;
	target2 =
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target2;
	target2Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .target2Type;
	targetOrMode = (int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					   .fg.orders[orderIndex]
					   .target1OrTarget2;
	target1 =
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target1;
	target1Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .target1Type;
	result = paifight_FindNearestEscortLeaderTarget(target1Type, target1, targetOrMode, target2Type, target2);
	if (result != 0xffffu) {
		return result;
	}

	target2 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				  .fg.orders[orderIndex]
				  .secondaryTargets[XWA_ORDER_TARGET_4];
	target2Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .secondaryTargetTypes[XWA_ORDER_TARGET_4];
	targetOrMode = (int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					   .fg.orders[orderIndex]
					   .target3OrTarget4;
	target1 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				  .fg.orders[orderIndex]
				  .secondaryTargets[XWA_ORDER_TARGET_3];
	target1Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .secondaryTargetTypes[XWA_ORDER_TARGET_3];
	return paifight_FindNearestEscortLeaderTarget(target1Type, target1, targetOrMode, target2Type, target2);
}

// FUNCTION: XWA 0x4A5410
char paifight_scanfortargetorder(void) {
	uint16_t candidateTargetIdx;
	uint8_t planId;
	uint16_t targetObjIdx;

	if (g_paiContext.aiController->maneuverMode == g_paiContext.aiPlanInitialManeuverId) {
		candidateTargetIdx = g_paiContext.aiController->candidateTargetIdx;
		if (candidateTargetIdx != 0xffffu && candidateTargetIdx != 251u) {
			if (pai_IsObjectTargetable(candidateTargetIdx)) {
				g_paiContext.aiController->targetObjIdx = candidateTargetIdx;
				g_paiContext.aiController->targetSignature =
					g_objectTable[candidateTargetIdx].objectSignature;
				g_paiContext.aiController->hasLiveTarget = 1;
				return 1;
			}
			g_paiContext.aiController->candidateTargetIdx = 0xffffu;
		}

		planId = g_paiContext.aiController->currentPlanId;
		if (strcmp(g_planTable[planId].name, "disableldr1pln") == 0) {
			g_paiContext.aiRequireLiveOrderTarget = 1;
		} else {
			g_paiContext.aiRequireLiveOrderTarget = 0;
		}
		g_paiContext.aiTargetSearchFlags = 7;

		if (strcmp(g_planTable[planId].name, "capfreeldr1pln") == 0 ||
			strcmp(g_planTable[planId].name, "disableldr1pln") == 0 ||
			strcmp(g_planTable[planId].name, "homing1pln") == 0 ||
			strcmp(g_planTable[planId].name, "followtarget1pln") == 0 ||
			strcmp(g_planTable[planId].name, "kamikaze1pln") == 0) {
			targetObjIdx = (uint16_t)paifight_FindAttackOrderTargetFromOrder(
				g_paiContext.curOrderCoord.fields.orderSlot, g_paiContext.curOrderCoord.fields.regionIdx);
		} else if (strcmp(g_planTable[planId].name, "capescortersldr1pln") == 0) {
			targetObjIdx = paifight_FindEscortLeaderTargetFromOrder(
				g_paiContext.curOrderCoord.fields.orderSlot, g_paiContext.curOrderCoord.fields.regionIdx);
		} else if (strcmp(g_planTable[planId].name, "inspectldr1pln") == 0) {
			targetObjIdx = (uint16_t)paifight_FindInspectOrderTargetFromOrder(
				g_paiContext.curOrderCoord.fields.orderSlot, g_paiContext.curOrderCoord.fields.regionIdx);
		} else {
			targetObjIdx = (uint16_t)paifight_FindAttackerOfOrderTargetFromOrder(
				g_paiContext.curOrderCoord.fields.orderSlot, g_paiContext.curOrderCoord.fields.regionIdx);
		}

		if (targetObjIdx != 0xffffu) {
			g_paiContext.aiController->targetObjIdx = targetObjIdx;
			g_paiContext.aiController->targetSignature = g_objectTable[targetObjIdx].objectSignature;
			g_paiContext.aiController->hasLiveTarget = 1;
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4AB1B0
char paifight_checkescortorder(void) {
	uint16_t target1Type;
	uint16_t target1;
	int16_t targetRelationOp;
	uint16_t target2Type;
	uint16_t target2;
	unsigned int orderIndex;
	unsigned int fallbackOrderIndex;

	g_paiContext.aiController->escortTargetFG = -1;

	orderIndex =
		g_paiContext.curOrderCoord.fields.orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx;
	target2 =
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target2;
	target2Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .target2Type;
	targetRelationOp = (int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						   .fg.orders[orderIndex]
						   .target1OrTarget2;
	target1 =
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target1;
	target1Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .target1Type;
	if (paifight_searchforclosestingroup(target1Type, target1, targetRelationOp, target2Type, target2) !=
		-1) {
		g_paiContext.aiController->escortTargetFG = (char)g_aiEscortCandidateFgIdx;
	} else {
		fallbackOrderIndex =
			g_paiContext.curOrderCoord.fields.orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx;
		target2 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[fallbackOrderIndex]
					  .secondaryTargets[XWA_ORDER_TARGET_4];
		target2Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						  .fg.orders[fallbackOrderIndex]
						  .secondaryTargetTypes[XWA_ORDER_TARGET_4];
		targetRelationOp = (int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							   .fg.orders[fallbackOrderIndex]
							   .target3OrTarget4;
		target1 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[fallbackOrderIndex]
					  .secondaryTargets[XWA_ORDER_TARGET_3];
		target1Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						  .fg.orders[fallbackOrderIndex]
						  .secondaryTargetTypes[XWA_ORDER_TARGET_3];
		if (paifight_searchforclosestingroup(target1Type, target1, targetRelationOp, target2Type, target2) !=
			-1) {
			g_paiContext.aiController->escortTargetFG = (char)g_aiEscortCandidateFgIdx;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4AAAA0
char paifight_coverleaderorder(void) {
	ObjectRecord* leaderObject;
	CraftData* leaderCraft;
	uint16_t lastAttackerObjIdx;
	int alreadyCovered;
	unsigned int slotCursor;
	unsigned int slotObjIdx;

	if ((uint16_t)g_paiContext.aiLeaderObjIdx != 0xffu) {
		leaderObject = &g_objectTable[(uint16_t)g_paiContext.aiLeaderObjIdx];
		leaderCraft = leaderObject->mobj->pCraft;
		if (leaderCraft != NULL) {
			lastAttackerObjIdx = leaderCraft->lastAttackerObjIdx;
			if (lastAttackerObjIdx != 0xffffu && g_objectTable[lastAttackerObjIdx].objectType != OBJ_None &&
				leaderCraft->objectKind == 0) {
				if ((g_missionFlightGroups[g_objectTable[g_paiContext.aiObjIdx].flightGroupIdx]
							 .playerOwnerIdx == -1 ||
					 lastAttackerObjIdx != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx) &&
					(g_objectTable[lastAttackerObjIdx].genusId == GENUS_Fighter ||
					 g_objectTable[lastAttackerObjIdx].genusId == GENUS_Transport)) {
					alreadyCovered = 0;
					slotCursor = g_activeRegionObjectSlotStart;
					slotObjIdx = (uint16_t)slotCursor;
					if (slotObjIdx < g_activeRegionCraftObjectSlotEnd) {
						do {
							if (slotObjIdx != g_paiContext.aiObjIdx &&
								g_objectTable[slotObjIdx].objectType != OBJ_None) {
								ObjectRecord* candidateObject;

								candidateObject = &g_objectTable[slotObjIdx];
								if (candidateObject->flightGroupIdx ==
									g_paiContext.curOrderCoord.fields.flightGroupIdx) {
									CraftData* candidateCraft;

									candidateCraft = candidateObject->mobj->pCraft;
									if (candidateCraft != NULL &&
										pai_GetEffectiveAIController(candidateCraft)->targetObjIdx ==
											lastAttackerObjIdx) {
										alreadyCovered = 1;
									}
								}
							}

							++slotCursor;
							slotObjIdx = (uint16_t)slotCursor;
						} while (slotObjIdx < g_activeRegionCraftObjectSlotEnd);
					}

					if (!alreadyCovered) {
						g_paiContext.aiController->targetObjIdx = lastAttackerObjIdx;
						g_paiContext.aiController->targetSignature =
							g_objectTable[lastAttackerObjIdx].objectSignature;
						g_paiContext.aiController->hasLiveTarget = 0;
						return 1;
					}
				}
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4A7350
char paifight_escorttargetorder(void) {
	if (g_paiContext.aiController->maneuverMode == g_paiContext.aiPlanInitialManeuverId) {
		uint16_t candidateTargetIdx;
		uint16_t bestTargetObjIdx;
		uint16_t escortTargetFG;
		unsigned int rangeLimit;
		unsigned int bestRange;
		unsigned int slotCursor;
		unsigned int slotObjIdx;

		candidateTargetIdx = g_paiContext.aiController->candidateTargetIdx;
		bestTargetObjIdx = 0xffffu;
		if (candidateTargetIdx != 0xffffu && candidateTargetIdx != 251u) {
			unsigned int candidateTargetObjIdx;

			candidateTargetObjIdx = candidateTargetIdx;
			if (pai_IsObjectTargetable(candidateTargetObjIdx)) {
				g_paiContext.aiController->targetObjIdx = candidateTargetIdx;
				g_paiContext.aiController->targetSignature =
					g_objectTable[candidateTargetObjIdx].objectSignature;
				g_paiContext.aiController->hasLiveTarget = 1;
				return 1;
			}

			g_paiContext.aiController->candidateTargetIdx = 0xffffu;
		}

		escortTargetFG = (uint8_t)g_paiContext.aiController->escortTargetFG;
		switch (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg
					.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							g_paiContext.curOrderCoord.fields.orderSlot]
					.variable2) {
			case 0:
				rangeLimit = 0x40000u;
				break;

			case 1:
				rangeLimit = 0x18000u;
				break;

			case 2:
				rangeLimit = 0u;
				break;

			case 3:
				rangeLimit = 0x60000u;
				break;

#ifdef XWA_MODERN
			default:
				rangeLimit = 0u;
				break;
#endif
		}

		bestRange = 0xffffffffu;
		for (slotCursor = g_activeRegionObjectSlotStart, slotObjIdx = (uint16_t)slotCursor;
			 slotObjIdx < g_activeRegionCraftObjectSlotEnd; slotObjIdx = (uint16_t)++slotCursor) {
			if (g_objectTable[slotObjIdx].objectType != OBJ_None && slotObjIdx != g_paiContext.aiObjIdx) {
				AiController* effectiveController;
				uint16_t targetObjIdx;
				uint8_t maneuverMode;
				int16_t isAttackingEscortTarget;

				effectiveController = pai_GetEffectiveAIController(g_objectTable[slotObjIdx].mobj->pCraft);
				targetObjIdx = effectiveController->targetObjIdx;
				maneuverMode = effectiveController->maneuverMode;
				isAttackingEscortTarget = 0;
				if ((maneuverMode == 12 || maneuverMode == 23 || maneuverMode == 11) &&
					targetObjIdx < 0x8000u && targetObjIdx != 0xffffu) {
					ObjectRecord* targetObject;

					targetObject = &g_objectTable[targetObjIdx];
					if (targetObject->objectType != OBJ_None) {
						uint16_t targetFlightGroupIdx;

						targetFlightGroupIdx = targetObject->flightGroupIdx;
						if (targetFlightGroupIdx == escortTargetFG) {
							isAttackingEscortTarget = 1;
						}
					}
				}

				if (isAttackingEscortTarget &&
					(uint16_t)paifight_TargetHasAttackCapacity(slotCursor, 0xffu)) {
					unsigned int rangeScore;

					pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, slotObjIdx);
					rangeScore = (unsigned int)g_targetRangeScore;
					if (rangeScore < bestRange && rangeScore < rangeLimit) {
						bestRange = rangeScore;
						bestTargetObjIdx = slotCursor;
					}
				}
			}
		}

		if (bestTargetObjIdx != 0xffffu) {
			g_paiContext.aiController->targetObjIdx = bestTargetObjIdx;
			g_paiContext.aiController->targetSignature = g_objectTable[bestTargetObjIdx].objectSignature;
			g_paiContext.aiController->hasLiveTarget = 1;
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4AAC20
char paifight_followleadatkorder(void) {
	AiController* leaderAi;
	PlanRecord* plan;
	int leaderObjIdx;
	int leaderPlayerOwnerIdx;
	uint16_t leaderManeuverMode;
	uint16_t targetObjIdx;
	uint32_t craftSlotEnd;

	leaderObjIdx = g_paiContext.aiLeaderObjIdx;
	leaderPlayerOwnerIdx = g_objectTable[leaderObjIdx].playerOwnerIdx;
	leaderAi = pai_GetEffectiveAIController(g_paiContext.aiTargetCraft);
	leaderManeuverMode = leaderAi->maneuverMode;
	plan = &g_planTable[g_paiContext.aiController->currentPlanId];
	if (strcmp(plan->name, "disableldr1pln") == 0) {
		g_paiContext.aiRequireLiveOrderTarget = 1;
	} else {
		g_paiContext.aiRequireLiveOrderTarget = 0;
	}

	if (leaderManeuverMode == 12 || leaderManeuverMode == 23 || leaderPlayerOwnerIdx != -1) {

		targetObjIdx = g_paiContext.aiController->candidateTargetIdx;
		if (targetObjIdx != 0xffffu && targetObjIdx != 251u) {
			if (pai_IsObjectTargetable(targetObjIdx)) {
				if (leaderPlayerOwnerIdx == -1) {
					g_paiContext.aiController->targetObjIdx = targetObjIdx;
					g_paiContext.aiController->targetSignature = g_objectTable[targetObjIdx].objectSignature;
					g_paiContext.aiController->hasLiveTarget = 1;
					return 1;
				}

				pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, targetObjIdx);
				if ((uint32_t)g_targetRangeScore <= 0x50000u) {
					g_paiContext.aiController->targetObjIdx = targetObjIdx;
					g_paiContext.aiController->targetSignature = g_objectTable[targetObjIdx].objectSignature;
					g_paiContext.aiController->hasLiveTarget = 1;
					return 1;
				}
				return 0;
			}

			g_paiContext.aiController->candidateTargetIdx = 0xffffu;
		}

		craftSlotEnd = g_activeRegionCraftObjectSlotEnd;
		if (leaderPlayerOwnerIdx == -1) {
			targetObjIdx = leaderAi->targetObjIdx;
		} else {
			uint16_t scanObjIdx;

			targetObjIdx = 0xffffu;
			scanObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
			while ((uint32_t)scanObjIdx < craftSlotEnd) {
				if (g_objectTable[scanObjIdx].objectType != OBJ_None) {
					CraftData* craft;

					craft = g_objectTable[scanObjIdx].mobj->pCraft;
					if ((!g_paiContext.aiRequireLiveOrderTarget || craft->workingSubsystems) &&
						scanObjIdx != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx &&
						g_objectTable[craft->lastAttackerObjIdx].playerOwnerIdx == leaderPlayerOwnerIdx) {
						if (Object_IsHostileToTeam(scanObjIdx, g_objectTable[leaderObjIdx].mobj->team)) {
							targetObjIdx = scanObjIdx;
						}
						craftSlotEnd = g_activeRegionCraftObjectSlotEnd;
					}
				}

				++scanObjIdx;
			}
		}

		if (targetObjIdx == 0xffffu) {
			return 0;
		}

		if (g_objectTable[targetObjIdx].mobj != NULL) {
			uint8_t targetFlightGroupIdx;
			uint16_t candidateObjIdx;
			uint16_t scanCounter;

			if (g_objectTable[targetObjIdx].mobj->pCraft == NULL) {
				return 0;
			}

			targetFlightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
			candidateObjIdx = (uint16_t)(targetObjIdx + g_curCraft->waveNumber);
			if ((uint32_t)candidateObjIdx >= craftSlotEnd) {
				candidateObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
			}

			scanCounter = (uint16_t)g_activeRegionObjectSlotStart;
			if ((uint32_t)scanCounter >= craftSlotEnd) {
				return 0;
			}

			while (1) {
				ObjectRecord* candidateObject;
				CraftData* candidateCraft;

				candidateObject = &g_objectTable[candidateObjIdx];
				candidateCraft = candidateObject->mobj->pCraft;
				if (candidateCraft != NULL &&
					(!g_paiContext.aiRequireLiveOrderTarget || candidateCraft->workingSubsystems) &&
					scanCounter != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx) {
					if (candidateObject->objectType != OBJ_None &&
						candidateObject->flightGroupIdx == targetFlightGroupIdx &&
						pai_IsObjectTargetableNearCurrentPoint(g_paiContext.aiObjIdx, candidateObjIdx, 1)) {
						if ((strcmp(plan->name, "capfreeldr1pln") != 0 &&
							 strcmp(plan->name, "disableldr1pln") != 0 &&
							 strcmp(plan->name, "kamikaze1pln") != 0) ||
							pai_CurrentOrderTargetsMatchObject(candidateObjIdx)) {
							g_paiContext.aiController->targetObjIdx = candidateObjIdx;
							g_paiContext.aiController->targetSignature =
								g_objectTable[candidateObjIdx].objectSignature;
							g_paiContext.aiController->hasLiveTarget = 1;
							return 1;
						}
					}
				}

				++candidateObjIdx;
				if ((uint32_t)candidateObjIdx >= craftSlotEnd) {
					candidateObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
				}

				++scanCounter;
				if ((uint32_t)scanCounter >= craftSlotEnd) {
					return 0;
				}
			}
		} else {
			uint8_t targetFlightGroupIdx;
			uint16_t candidateObjIdx;
			uint16_t scanCount;

			targetFlightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
			candidateObjIdx = (uint16_t)(targetObjIdx + 1u);
			if ((uint32_t)candidateObjIdx >= g_regionStaticObjectSlotEnd) {
				candidateObjIdx = (uint16_t)g_objScanStart;
			}

			scanCount = (uint16_t)g_objScanStart;
			if ((uint32_t)scanCount >= g_regionStaticObjectSlotEnd) {
				return 0;
			}

			while (1) {
				if ((!g_paiContext.aiRequireLiveOrderTarget ||
					 g_objectTable[candidateObjIdx].typeSpecificWord) &&
					candidateObjIdx != (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx) {
					ObjectRecord* candidateObject;

					candidateObject = &g_objectTable[candidateObjIdx];
					if (candidateObject->objectType != OBJ_None &&
						candidateObject->flightGroupIdx == targetFlightGroupIdx) {
						if (pai_IsObjectTargetableNearCurrentPoint(g_paiContext.aiObjIdx, candidateObjIdx,
																   1) &&
							pai_CurrentOrderTargetsMatchObject(candidateObjIdx)) {
							g_paiContext.aiController->targetObjIdx = candidateObjIdx;
							g_paiContext.aiController->targetSignature =
								g_objectTable[candidateObjIdx].objectSignature;
							g_paiContext.aiController->hasLiveTarget = 1;
							return 1;
						}
					}

					++candidateObjIdx;
					if ((uint32_t)candidateObjIdx >= g_regionStaticObjectSlotEnd) {
						candidateObjIdx = (uint16_t)g_objScanStart;
					}
				}

				++scanCount;
				if ((uint32_t)scanCount >= g_regionStaticObjectSlotEnd) {
					return 0;
				}
			}
		}
	}
	return 0;
}

static __inline CraftData* paifight_AssignGunnerTurretTarget(unsigned int laserSlot, int16_t targetObjIdx) {
	CraftData* craft;

	g_curCraft->warheadData[laserSlot].turretTargetObjIdx = targetObjIdx;
	g_curCraft->warheadData[laserSlot].turretRetargetCooldownTimer =
		(int16_t)(g_curCraft->warheadData[laserSlot].turretRetargetCooldownTimer + 472);
	craft = g_curCraft;
	if (g_curCraft->warheadData[laserSlot].turretRotBucket <= 0) {
		g_curCraft->warheadData[laserSlot].turretRotBucket = (int16_t)(11 * (laserSlot & 7u));
		craft = g_curCraft;
	}
	return craft;
}

// FUNCTION: XWA 0x4A98B0
char paifight_gunneroffenseorder(void) {
	CraftData* craft;
	struct {
		uint16_t laserSlot;
		int candidateSetsNeedBuild;
	} state;
	uint16_t hasTurretMount;
	int groupIdx;
	const uint8_t* laserGroupMountType;

	craft = g_curCraft;
	if (craft->objectKind == 3 || craft->workingSubsystems == 0 ||
		g_missionFlightGroups[g_objectTable[g_paiContext.aiObjIdx].flightGroupIdx].fg.status1 == 14 ||
		g_missionFlightGroups[g_objectTable[g_paiContext.aiObjIdx].flightGroupIdx].fg.status2 == 14) {
		return 0;
	}

	hasTurretMount = 0;
	groupIdx = 3;
	laserGroupMountType = g_modelDefs[craft->modelIndex].laserGroupMountType;
	do {
		if (*laserGroupMountType >= 4u) {
			hasTurretMount = 1;
		}
		++laserGroupMountType;
		--groupIdx;
	} while (groupIdx != 0);
	if (!hasTurretMount) {
		return 0;
	}

	g_paiContext.aiRequireLiveOrderTarget = 0;
	state.candidateSetsNeedBuild = 1;
	if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "starshipprotectpln") != 0 &&
		strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "starshipescortpln") != 0) {
		g_paiContext.aiRequireLiveOrderTarget =
			strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "starshipdisablepln") == 0 ||
			strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "disableldr1pln") == 0;
	}

	state.laserSlot = 0;
	if ((uint8_t)craft->laserSlotCount > 0) {
		do {
			if (craft->warheadData[state.laserSlot].weaponType >= 4u &&
				craft->warheadData[state.laserSlot].turretTargetObjIdx == -1 &&
				craft->warheadData[state.laserSlot].turretRetargetCooldownTimer <= 0) {
				CraftData* currentCraft;
				int originX;
				int originY;
				int originZ;
				unsigned int searchFlags;

				craft->warheadData[state.laserSlot].turretRetargetCooldownTimer =
					(int16_t)(craft->warheadData[state.laserSlot].turretRetargetCooldownTimer + 236);
				currentCraft = g_curCraft;
				searchFlags = currentCraft->warheadData[state.laserSlot].weaponType != 4u ? 0x50u : 0x30u;
				g_paiContext.aiTargetSearchFlags = (uint8_t)searchFlags;
				g_paiContext.aiTargetSearchRangeLimit =
					(uint32_t)g_modelDefs[currentCraft->modelIndex]
						.laserGroupFireRange[currentCraft->warheadData[state.laserSlot].weaponGroupIdx];
				if (g_paiContext.aiTargetSearchRangeLimit == 0) {
					g_paiContext.aiTargetSearchRangeLimit = 0x10000u;
				}

				if (state.candidateSetsNeedBuild == 1) {
					paifight_BuildGunnerTargetCandidateSet(
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target1Type,
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target1,
						(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target1OrTarget2,
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target2Type,
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target2,
						0);
					paifight_BuildGunnerTargetCandidateSet(
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.secondaryTargetTypes[XWA_ORDER_TARGET_3],
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.secondaryTargets[XWA_ORDER_TARGET_3],
						(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target3OrTarget4,
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.secondaryTargetTypes[XWA_ORDER_TARGET_4],
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.secondaryTargets[XWA_ORDER_TARGET_4],
						1u);
					currentCraft = g_curCraft;
					state.candidateSetsNeedBuild = 0;
				}

				originX = g_paiContext.aiCurrentPointX;
				originY = g_paiContext.aiCurrentPointY;
				originZ = g_paiContext.aiCurrentPointZ;
				g_paifightSearchOriginX = originX;
				g_paifightSearchOriginY = originY;
				g_paifightSearchOriginZ = originZ;
				if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
					g_paiContext.aiTargetSearchOriginX = originX;
					g_paiContext.aiTargetSearchOriginY = originY;
					g_paiContext.aiTargetSearchOriginZ = originZ;
				} else {
					pai_calcrotatedpoint(
						&g_objectTable[g_paiContext.aiObjIdx],
						g_modelDefs[currentCraft->modelIndex].weaponHardpoints[state.laserSlot].x,
						g_modelDefs[currentCraft->modelIndex].weaponHardpoints[state.laserSlot].z,
						g_modelDefs[currentCraft->modelIndex].weaponHardpoints[state.laserSlot].y);
					if (g_objectTable[g_paiContext.aiObjIdx].objectType == OBJ_ImperialStarDestroyer2) {
						g_rotatedX <<= 1;
						g_rotatedY <<= 1;
						g_rotatedZ <<= 1;
					}

					originX = g_paifightSearchOriginX + g_rotatedX;
					originY = g_paifightSearchOriginY + g_rotatedY;
					originZ = g_paifightSearchOriginZ + g_rotatedZ;
					currentCraft = g_curCraft;
					g_paifightSearchOriginX = originX;
					g_paiContext.aiTargetSearchOriginX = originX;
					g_paifightSearchOriginY = originY;
					g_paiContext.aiTargetSearchOriginY = originY;
					g_paifightSearchOriginZ = originZ;
					g_paiContext.aiTargetSearchOriginZ = originZ;
				}

				g_collisionSegmentStartWorldY = originY;
				g_collisionSegmentStartWorldX = originX;
				g_collisionSegmentStartWorldZ = originZ;
				currentCraft->warheadData[state.laserSlot].count = 0;

				if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
						   "starshipprotectpln") != 0 &&
					strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "starshipescortpln") !=
						0) {
					int16_t targetObjIdx;

					targetObjIdx = paifight_FindNearestGunnerTargetInCandidateSet(
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target1Type,
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target1,
						(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target1OrTarget2,
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target2Type,
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.target2,
						0);
					if (targetObjIdx != -1) {
						craft = paifight_AssignGunnerTurretTarget(state.laserSlot, targetObjIdx);
						if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
								   "starshipdisablepln") == 0 ||
							strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
								   "disableldr1pln") == 0) {
							craft->warheadData[state.laserSlot].count = 1;
						}
					} else {
						targetObjIdx = paifight_FindNearestGunnerTargetInCandidateSet(
							g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								.fg
								.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										g_paiContext.curOrderCoord.fields.orderSlot]
								.secondaryTargetTypes[XWA_ORDER_TARGET_3],
							g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								.fg
								.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										g_paiContext.curOrderCoord.fields.orderSlot]
								.secondaryTargets[XWA_ORDER_TARGET_3],
							(int16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								.fg
								.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										g_paiContext.curOrderCoord.fields.orderSlot]
								.target3OrTarget4,
							g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								.fg
								.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										g_paiContext.curOrderCoord.fields.orderSlot]
								.secondaryTargetTypes[XWA_ORDER_TARGET_4],
							g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								.fg
								.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										g_paiContext.curOrderCoord.fields.orderSlot]
								.secondaryTargets[XWA_ORDER_TARGET_4],
							1);
						if (targetObjIdx != -1) {
							craft = paifight_AssignGunnerTurretTarget(state.laserSlot, targetObjIdx);
							if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
									   "starshipdisablepln") == 0 ||
								strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
									   "disableldr1pln") == 0) {
								craft->warheadData[state.laserSlot].count = 1;
							}
						}
					}
				} else {
					int16_t attackerObjIdx;

					attackerObjIdx = paifight_FindAttackerOfOrderTargetFromOrder(
						g_paiContext.curOrderCoord.fields.orderSlot,
						g_paiContext.curOrderCoord.fields.regionIdx);
					if (attackerObjIdx != -1) {
						craft = paifight_AssignGunnerTurretTarget(state.laserSlot, attackerObjIdx);
					}
				}

				craft = g_curCraft;
			}

			++state.laserSlot;
		} while (state.laserSlot < craft->laserSlotCount);
	}

	return 0;
}

// FUNCTION: XWA 0x4A8520
char paifight_missiledefenseorder(void) {
	CraftData* craft;
	int launcherIdx;

	craft = g_curCraft;
	if (craft->objectKind == 3 || craft->workingSubsystems == 0 || craft->weaponFireInhibitTimer != 0 ||
		g_objectTable[g_paiContext.aiObjIdx].genusId == 0) {
		return 0;
	}

	launcherIdx = 0;
	if ((uint8_t)craft->warheadLauncherCount > 0) {
		do {
			uint16_t currentLauncherIdx;
			uint16_t firstSlot;
			int warheadSlot;

			currentLauncherIdx = (uint16_t)launcherIdx;
			firstSlot = g_modelDefs[craft->modelIndex].warheadLauncherFirstSlot[currentLauncherIdx];
			warheadSlot = (uint16_t)firstSlot;
			while (warheadSlot <
				   g_modelDefs[craft->modelIndex].warheadLauncherLastSlot[currentLauncherIdx] + 1) {
				uint16_t bestTargetObjIdx;
				uint32_t bestRangeScore;
				uint16_t objectIdx;

				if (craft->componentState
						[g_modelDefs[craft->modelIndex].weaponHardpoints[warheadSlot].meshIdx] == 0) {
					if ((craft->warheadData[warheadSlot].projectileTypeId == OBJ_WarheadMissile ||
						 craft->warheadData[warheadSlot].projectileTypeId == OBJ_WarheadAdvancedMissile) &&
						craft->warheadData[warheadSlot].count != 0 &&
						craft->warheadData[warheadSlot].turretRotBucket <= 0) {

						craft->warheadData[warheadSlot].turretTargetObjIdx = -1;
						g_paifightSearchOriginX = g_paiContext.aiCurrentPointX;
						g_paifightSearchOriginY = g_paiContext.aiCurrentPointY;
						g_paifightSearchOriginZ = g_paiContext.aiCurrentPointZ;
						pai_calcrotatedpoint(
							&g_objectTable[g_paiContext.aiObjIdx],
							g_modelDefs[g_curCraft->modelIndex].weaponHardpoints[warheadSlot].x,
							g_modelDefs[g_curCraft->modelIndex].weaponHardpoints[warheadSlot].z,
							g_modelDefs[g_curCraft->modelIndex].weaponHardpoints[warheadSlot].y);
						if (g_objectTable[g_paiContext.aiObjIdx].objectType == OBJ_ImperialStarDestroyer2) {
							g_rotatedX <<= 1;
							g_rotatedY <<= 1;
							g_rotatedZ <<= 1;
						}
						g_paifightSearchOriginX += g_rotatedX;
						g_paifightSearchOriginY += g_rotatedY;
						g_paifightSearchOriginZ += g_rotatedZ;

						bestTargetObjIdx = 0xffffu;
						bestRangeScore = 0x40000u;
						for (objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
							 (uint32_t)objectIdx < g_activeRegionCraftObjectSlotEnd; ++objectIdx) {
							CraftData* candidateCraft;
							AiController* candidateAi;

							if (g_objectTable[objectIdx].objectType == OBJ_None) {
								continue;
							}

							candidateCraft = g_objectTable[objectIdx].mobj->pCraft;
							candidateAi = pai_GetEffectiveAIController(candidateCraft);
							if (!(((uint32_t)candidateAi->targetObjIdx == g_paiContext.aiObjIdx &&
								   (candidateAi->maneuverMode == 12 || candidateAi->maneuverMode == 23)) ||
								  candidateCraft->lastAttackerObjIdx == objectIdx ||
								  candidateCraft->aiFlight.threatObjIdx == objectIdx)) {
								int playerOwnerIdx;

								playerOwnerIdx = g_objectTable[objectIdx].playerOwnerIdx;
								if (playerOwnerIdx == -1) {
									continue;
								}
								if (Object_IsHostileToTeam(
										objectIdx, g_objectTable[g_paiContext.aiObjIdx].mobj->team) != 1) {
									continue;
								}
								if ((uint16_t)g_players[playerOwnerIdx].targetSubState < 5u ||
									(uint32_t)(uint16_t)g_players[playerOwnerIdx].currentTargetObjectIdx !=
										g_paiContext.aiObjIdx) {
									continue;
								}
							}

							if (strcmp(g_planTable[candidateAi->currentPlanId].name, "inspectldr1pln") == 0) {
								continue;
							}
							if (!pai_IsObjectTargetable(objectIdx)) {
								continue;
							}

							{
								uint16_t homingProjectileCount;
								int projectileObjIdx;

								homingProjectileCount = 0;
								for (projectileObjIdx = g_projectileObjectSlotStart;
									 (uint16_t)projectileObjIdx < g_projectileObjectSlotEnd;
									 ++projectileObjIdx) {
									ObjectRecord* projectileObj;

									projectileObj = &g_objectTable[(uint16_t)projectileObjIdx];
									if (projectileObj->objectType != OBJ_None &&
										projectileObjIdx != objectIdx) {
										WarheadGuidanceState* guidance;

										guidance = projectileObj->mobj->pWarheadGuidance;
										if (guidance->homingTier != 0 &&
											guidance->targetObjIdx == objectIdx) {
											++homingProjectileCount;
										}
									}
								}
								if (homingProjectileCount >= 2u) {
									continue;
								}
							}

							{
								uint32_t rangeScore;

								rangeScore = (uint32_t)collide_roughdistance3d(
									g_objectTable[objectIdx].world_x - g_paifightSearchOriginX,
									g_objectTable[objectIdx].world_y - g_paifightSearchOriginY,
									g_objectTable[objectIdx].world_z - g_paifightSearchOriginZ);
								g_targetRangeScore = (int)rangeScore;
								if (rangeScore < bestRangeScore) {
									bestRangeScore = rangeScore;
									bestTargetObjIdx = objectIdx;
								}
							}
						}

						if (bestTargetObjIdx != 0xffffu) {
							g_curCraft->warheadData[warheadSlot].turretTargetObjIdx = bestTargetObjIdx;
						}

						craft = g_curCraft;
						if (g_curCraft->warheadData[warheadSlot].turretTargetObjIdx != -1) {
							uint16_t savedTargetObjIdx;
							int projectileObjIdx;

							savedTargetObjIdx = g_paiContext.aiController->targetObjIdx;
							g_paiContext.aiController->targetObjIdx =
								(uint16_t)g_curCraft->warheadData[warheadSlot].turretTargetObjIdx;
							g_paiContext.aiController->targetComponent =
								paifight_SelectTargetComponentMesh(g_paiContext.aiController->targetObjIdx);
							projectileObjIdx = laser_firemissile(
								g_paiContext.aiObjIdx, warheadSlot,
								g_curCraft->warheadData[warheadSlot].projectileTypeId, 0xffffu);
							if (projectileObjIdx != 0xffff) {
								g_objectTable[projectileObjIdx].mobj->pWarheadGuidance->homingTier =
									(uint8_t)((GameRand() & 3u) + 3u);
								g_curCraft->warheadData[warheadSlot].turretRotBucket = 4720;
							}
							g_paiContext.aiController->targetObjIdx = savedTargetObjIdx;
							craft = g_curCraft;
						}
					}
				}
				++firstSlot;
				warheadSlot = (uint16_t)firstSlot;
			}
			++launcherIdx;
		} while ((uint16_t)launcherIdx < (uint8_t)craft->warheadLauncherCount);
	}

	return 0;
}

// FUNCTION: XWA 0x4AA6A0
void paifight_BuildGunnerTargetCandidateSet(uint16_t target1Type, uint16_t target1, int16_t target1OrTarget2,
											uint16_t target2Type, uint16_t target2,
											uint16_t candidateSetIdx) {
	uint16_t objectIdx;

	objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
	while ((uint32_t)objectIdx < g_activeRegionCraftObjectSlotEnd) {
		g_paifightGunnerTargetCandidateSet[2u * objectIdx + candidateSetIdx] = 0;
		if (g_objectTable[objectIdx].objectType != OBJ_None) {
			int matchesTarget1;
			int matchesTarget2;

			matchesTarget1 = Mission_ObjectMatchesTriggerVariable(objectIdx, target1Type, target1);
			matchesTarget2 = Mission_ObjectMatchesTriggerVariable(objectIdx, target2Type, target2);
			if (target1OrTarget2 == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if ((uint16_t)matchesTarget1) {
				if ((!g_paiContext.aiRequireLiveOrderTarget ||
					 g_objectTable[objectIdx].mobj->pCraft->workingSubsystems) &&
					pai_IsObjectTargetable(objectIdx)) {
					g_paifightGunnerTargetCandidateSet[2u * objectIdx + candidateSetIdx] = 1;
				}
			}
		}

		++objectIdx;
	}
}

static __inline uint32_t paifight_ApplyTurretTargetRangePenalty(uint16_t targetObjIdx, uint32_t rangeScore) {
	int laserSlot;

	laserSlot = 0;
	while (laserSlot < g_curCraft->laserSlotCount) {
		if (g_curCraft->warheadData[laserSlot].turretTargetObjIdx == (int16_t)targetObjIdx) {
			rangeScore += 0x8000u;
			g_targetRangeScore = (int)rangeScore;
		}
		++laserSlot;
	}

	return rangeScore;
}

static __inline int paifight_IsWarheadClassProjectile(ObjectTypeId objectType) {
#ifdef XWA_MODERN
	return g_projectileWarheadClassByType[(uint16_t)objectType - OBJ_LaserRebel] != 0;
#else
	return ((const uint8_t*)&g_projectileDamageByType[14])[(uint16_t)objectType] != 0;
#endif
}

static __inline void paifight_SetGunnerSelfDefenseSearchOrigin(unsigned int laserSlot) {
	g_paifightSearchOriginY = g_paiContext.aiCurrentPointY;
	g_paifightSearchOriginX = g_paiContext.aiCurrentPointX;
	g_paifightSearchOriginZ = g_paiContext.aiCurrentPointZ;
	if (!g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
		pai_calcrotatedpoint(&g_objectTable[g_paiContext.aiObjIdx],
							 g_modelDefs[g_curCraft->modelIndex].weaponHardpoints[laserSlot].x,
							 g_modelDefs[g_curCraft->modelIndex].weaponHardpoints[laserSlot].z,
							 g_modelDefs[g_curCraft->modelIndex].weaponHardpoints[laserSlot].y);
		if (g_objectTable[g_paiContext.aiObjIdx].objectType == OBJ_ImperialStarDestroyer2) {
			g_rotatedX <<= 1;
			g_rotatedY <<= 1;
			g_rotatedZ <<= 1;
		}

		g_paifightSearchOriginX += g_rotatedX;
		g_paifightSearchOriginY += g_rotatedY;
		g_paifightSearchOriginZ += g_rotatedZ;
		g_collisionSegmentStartWorldX = g_paifightSearchOriginX;
		g_collisionSegmentStartWorldY = g_paifightSearchOriginY;
		g_collisionSegmentStartWorldZ = g_paifightSearchOriginZ;
	}
}

static __inline int paifight_HasClearTurretSelfDefenseShotYXZ(uint16_t targetObjIdx) {
	if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
		return 1;
	}

	Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
	g_collisionProbeWorldX = worldlocx;
	g_collisionProbeWorldY = worldlocy;
	g_collisionProbeWorldZ = worldlocz;
	g_collisionStagedModelProbe = 1;
	if (collide_CheckSweptModelCollision((uint16_t)g_paiContext.aiObjIdx, (uint16_t)g_paiContext.aiObjIdx)) {
		g_collisionStagedModelProbe = 0;
		return 0;
	}
	g_collisionStagedModelProbe = 0;
	return 1;
}

static __inline int paifight_HasClearTurretSelfDefenseShotXYZ(uint16_t targetObjIdx) {
	if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
		return 1;
	}

	Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
	g_collisionProbeWorldY = worldlocy;
	g_collisionProbeWorldX = worldlocx;
	g_collisionProbeWorldZ = worldlocz;
	g_collisionStagedModelProbe = 1;
	if (collide_CheckSweptModelCollision((uint16_t)g_paiContext.aiObjIdx, (uint16_t)g_paiContext.aiObjIdx)) {
		g_collisionStagedModelProbe = 0;
		return 0;
	}
	g_collisionStagedModelProbe = 0;
	return 1;
}

static __inline int paifight_HasClearTurretSelfDefenseShotZXY(uint16_t targetObjIdx) {
	if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
		return 1;
	}

	Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
	g_collisionProbeWorldX = worldlocx;
	g_collisionProbeWorldY = worldlocy;
	g_collisionProbeWorldZ = worldlocz;
	g_collisionStagedModelProbe = 1;
	if (collide_CheckSweptModelCollision((uint16_t)g_paiContext.aiObjIdx, (uint16_t)g_paiContext.aiObjIdx)) {
		g_collisionStagedModelProbe = 0;
		return 0;
	}
	g_collisionStagedModelProbe = 0;
	return 1;
}

static __inline int paifight_GunnerSelfDefenseTargetIsAlreadyCovered(uint16_t targetObjIdx) {
	WarheadInventoryEntry* weapon;
	unsigned int coveredCount;
	unsigned int slotsRemaining;

	coveredCount = 0;
	slotsRemaining = g_curCraft->laserSlotCount;
	if (slotsRemaining > 0) {
		weapon = g_curCraft->warheadData;
		do {
			if (weapon->turretTargetObjIdx == (int16_t)targetObjIdx) {
				++coveredCount;
			}
			++weapon;
			--slotsRemaining;
		} while (slotsRemaining != 0);
	}

	return coveredCount != 0;
}

static __inline int paifight_IsCraftAttackingSelf(uint16_t candidateObjIdx, CraftData* candidateCraft) {
	AiController* candidateAi;
	int laserSlot;
	char isAttackingSelf;

	candidateAi = pai_GetEffectiveAIController(candidateCraft);
	isAttackingSelf = 0;
	if (candidateAi->targetObjIdx == g_paiContext.aiObjIdx) {
		if (candidateAi->maneuverMode == 12 || candidateAi->maneuverMode == 23) {
			if (strcmp(g_planTable[candidateAi->currentPlanId].name, "inspectldr1pln") != 0) {
				isAttackingSelf = 1;
			}
		}
	}

	if (!isAttackingSelf) {
		for (laserSlot = 0; laserSlot < candidateCraft->laserSlotCount; ++laserSlot) {
			WarheadInventoryEntry* weapon;

			weapon = &candidateCraft->warheadData[laserSlot];
			if ((uint16_t)weapon->turretTargetObjIdx == g_paiContext.aiObjIdx && weapon->weaponType >= 4u) {
				isAttackingSelf = 1;
				break;
			}
		}
	}

	(void)candidateObjIdx;
	return isAttackingSelf;
}

// FUNCTION: XWA 0x4A75B0
char paifight_fightershootorder(void) {
	uint16_t targetObjIdx;
	uint32_t targetComponent;
	uint32_t maxLaserRange;
	PlanRecord* plan;
	bool isDisablePlan;
	uint16_t yawDelta;
	uint16_t pitchDelta;
	uint16_t aimThreshold;
	uint16_t laserLinkMode;
	uint8_t laserLinkModeHi;
	uint8_t classLinkMode;
	ModelGenusId targetGenus;
	int16_t desiredWarheadClass;
	int16_t maxWarheadShots;
	uint16_t maxComponentHits;
	uint32_t fireRangeLimit;
	char acceptAnyWarhead;
	char hasUsableWarhead;
	CraftData* targetCraft;
	uint32_t durabilityScore;
	uint32_t pendingWarheadDamage;
	int16_t pendingIonPulseScore;
	int componentHitCount;
	int homingWarheadCount;
	uint16_t cannonClassIdx;
	uint32_t projectileObjIdx;
	uint16_t warheadLauncherCount;
	uint16_t launcherIdx;
	uint16_t warheadSlot;

	if (g_curCraft->workingSubsystems == 0) {
		return 0;
	}

	targetObjIdx = g_paiContext.aiController->targetObjIdx;
	targetComponent = g_paiContext.aiController->targetComponent;
	if (!pai_IsObjectTargetable(targetObjIdx)) {
		goto clear_laser_link_modes;
	}

	maxLaserRange = g_aiFighterShootMaxRangeBySkill[g_paiContext.aiSkillTier];
	plan = &g_planTable[g_paiContext.aiController->currentPlanId];
	if (strcmp(plan->name, "disableldr1pln") == 0) {
		isDisablePlan = true;
	} else {
		isDisablePlan =
			strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "playerdisableldr2pln") == 0;
	}

	if ((uint32_t)targetObjIdx >= g_regionMainObjectSlotStart && targetObjIdx < g_regionMainObjectSlotEnd) {
		ObjectRecord* targetObj;
		MobileObject* targetMobj;

		targetObj = &g_objectTable[targetObjIdx];
		targetMobj = targetObj->mobj;
		if (targetMobj != NULL) {
			if (targetMobj->speed > 25u) {
				uint16_t targetYawDelta;

				targetYawDelta = (uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw - targetObj->yaw);
				if (targetYawDelta >= 0x8000u) {
					targetYawDelta = (uint16_t)(0u - targetYawDelta);
				}
				if (targetYawDelta < 0x2000u) {
					maxLaserRange -= 0x4000u;
				} else if (targetYawDelta < 0x5000u) {
					maxLaserRange -= 0x2000u;
				}
			}
			if (targetObj->genusId == GENUS_Platform || targetObj->genusId == GENUS_Starship) {
				maxLaserRange += 0x6000u;
			}
		}
	}

	pai_CalcAnglesToAimPoint();
	yawDelta = (uint16_t)(trig2_xyangle - g_objectTable[g_paiContext.aiObjIdx].yaw);
	if (yawDelta >= 0x8000u) {
		yawDelta = (uint16_t)(0u - yawDelta);
	}
	pitchDelta = (uint16_t)(targetPitch - g_objectTable[g_paiContext.aiObjIdx].pitch);
	if (pitchDelta >= 0x8000u) {
		pitchDelta = (uint16_t)(0u - pitchDelta);
	}

	laserLinkModeHi = 0;
	aimThreshold = g_missionHeader.body.hangar != XWA_HANGAR_DEATHSTAR ? 0x0800u : 0x3000u;
	if (yawDelta >= aimThreshold || pitchDelta >= aimThreshold ||
		(uint32_t)trig2_polardistance >= maxLaserRange) {
		laserLinkMode = 0;
	} else {
		if ((int)trig2_polardistance < 0x2000) {
			laserLinkMode = 3;
		} else {
			laserLinkMode = (uint16_t)(((int)trig2_polardistance < 0x4000) + 1);
		}
		laserLinkModeHi = g_aiFighterShootLinkSlot3ValueBySkill[g_paiContext.aiSkillTier];
	}

	g_curCraft->laserConvergeLevel = 0;
	if (g_objectTable[g_paiContext.aiObjIdx].genusId == GENUS_Fighter ||
		g_objectTable[g_paiContext.aiObjIdx].objectType == OBJ_EscortShuttle) {
		ModelGenusId targetLaserGenus;

		targetLaserGenus = g_objectTable[targetObjIdx].genusId;
		switch (targetLaserGenus) {
			case GENUS_Fighter:
			case GENUS_Transport:
			case GENUS_Utility:
			case GENUS_Mine:
			case GENUS_SatelliteBuoy:
			case GENUS_PilotDroid:
			case GENUS_WeaponEmplacement: {
				uint8_t convergeMode;

				convergeMode = g_modelDefs[g_curCraft->modelIndex].laserConvergeMode;
				if (convergeMode == 1) {
					g_curCraft->laserConvergeLevel = 2;
				} else if (convergeMode == 2) {
					g_curCraft->laserConvergeLevel = 4;
				}
				break;
			}
			default:
				break;
		}
	}

	if (g_curCraft->cannonClassCount != 0) {
		uint16_t remainingCannonClasses;

		cannonClassIdx = 0;
		remainingCannonClasses = g_curCraft->cannonClassCount;
		do {
			if (laserLinkMode == 0) {
				classLinkMode = 0;
			} else if (g_curCraft->laserProjectileTypeId[cannonClassIdx] != OBJ_LaserIon) {
				if (!g_paiContext.aiController->hasLiveTarget || !isDisablePlan) {
					classLinkMode = (uint8_t)laserLinkMode;
				} else {
					classLinkMode = 0;
					if (targetObjIdx >= (uint16_t)g_activeRegionObjectSlotStart &&
						(uint32_t)targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
						uint32_t pendingShieldDamage;

						pendingShieldDamage = 0;
						for (projectileObjIdx = (uint16_t)g_projectileObjectSlotStart;
							 projectileObjIdx < (uint16_t)g_projectileObjectSlotEnd; ++projectileObjIdx) {
							ObjectRecord* projectileObj;

							projectileObj = &g_objectTable[projectileObjIdx];
							if (projectileObj->objectType != OBJ_None &&
								(projectileObj->genusId == GENUS_PlayerProjectile ||
								 projectileObj->genusId == GENUS_NpcProjectile) &&
								projectileObj->mobj->pWarheadGuidance->targetObjIdx == targetObjIdx) {
								uint32_t damage;
								ModelGenusId pendingTargetGenus;

								pendingTargetGenus = g_objectTable[targetObjIdx].genusId;
								damage =
									(uint32_t)g_projectileDamageByType[(uint16_t)projectileObj->objectType -
																	   OBJ_LaserRebel];
								if (pendingTargetGenus == GENUS_Starship ||
									pendingTargetGenus == GENUS_Platform) {
									damage >>= 4;
								}
								if (pendingTargetGenus == GENUS_Freighter ||
									pendingTargetGenus == GENUS_Container) {
									damage >>= 2;
								}
								pendingShieldDamage += damage;
							}
						}

						if (g_objectTable[targetObjIdx].mobj->pCraft->shieldFront != 0 &&
							pendingShieldDamage <
								(uint32_t)g_objectTable[targetObjIdx].mobj->pCraft->shieldFront) {
							classLinkMode = (uint8_t)laserLinkMode;
							laserLinkModeHi = 1;
						}
					}
				}
			} else {
				classLinkMode = 0;
				if (g_paiContext.aiController->hasLiveTarget) {
					if (isDisablePlan) {
						if (g_paiContext.aiController->hasLiveTarget != 1) {
							goto set_laser_link_mode;
						}
					} else if (g_missionHeader.body.hangar != XWA_HANGAR_SKIRMISH) {
						goto set_laser_link_mode;
					}
				}
				if (targetObjIdx >= (uint16_t)g_activeRegionObjectSlotStart &&
					(uint32_t)targetObjIdx < g_activeRegionCraftObjectSlotEnd &&
					g_objectTable[targetObjIdx].mobj->pCraft->shieldFront != 0) {
					classLinkMode = (uint8_t)laserLinkMode;
				}
			}

		set_laser_link_mode:
			g_curCraft->laserLinkMode[cannonClassIdx] = classLinkMode;
			g_curCraft->laserLinkMode[cannonClassIdx + 3] = laserLinkModeHi;
			++cannonClassIdx;
			--remainingCannonClasses;
		} while (remainingCannonClasses != 0);
	}

	if (targetObjIdx < (uint16_t)g_activeRegionObjectSlotStart ||
		(uint32_t)targetObjIdx >= g_activeRegionCraftObjectSlotEnd ||
		Object_HasActiveDecoyBeam(targetObjIdx)) {
		return 0;
	}

	targetGenus = g_objectTable[targetObjIdx].genusId;
	if (targetGenus == GENUS_Starship || targetGenus == GENUS_Platform || targetGenus == GENUS_Freighter ||
		targetGenus == GENUS_Container || targetGenus == GENUS_WeaponEmplacement ||
		(targetGenus == GENUS_Transport && g_missionFormatVersion >= 14)) {
		desiredWarheadClass = 2;
		maxWarheadShots = 6;
		maxComponentHits = 16;
		fireRangeLimit = g_paiContext.aiSkillTier != 2 ? 203610u : 244332u;
	} else {
		maxComponentHits = 2;
		desiredWarheadClass = 1;
		maxWarheadShots = 1;
		fireRangeLimit = 101805u;
	}

	acceptAnyWarhead = 0;
	if (strcmp(plan->name, "capfreeldr1pln") == 0) {
		const XwaOrder* order;

		order = &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					 .fg.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot];
		if (order->variable3 == 1) {
			acceptAnyWarhead = 1;
		} else if (order->variable3 == 2) {
			desiredWarheadClass = 0;
		}
	}

	hasUsableWarhead = 0;
	warheadLauncherCount = g_curCraft->warheadLauncherCount;
	for (launcherIdx = 0; launcherIdx < warheadLauncherCount; ++launcherIdx) {
		ObjectTypeId warheadType;

		warheadType = (ObjectTypeId)g_curCraft->warheadSlotTypeIds[launcherIdx];
		if (g_projectileWarheadClassByType[(uint16_t)warheadType - OBJ_LaserRebel] == desiredWarheadClass ||
			acceptAnyWarhead ||
			(g_missionFormatVersion >= 14 && warheadType == OBJ_WarheadMagPulse &&
			 desiredWarheadClass == 2)) {
			uint16_t firstSlot;
			uint16_t lastSlot;

			firstSlot = g_modelDefs[g_curCraft->modelIndex].warheadLauncherFirstSlot[launcherIdx];
			lastSlot = g_modelDefs[g_curCraft->modelIndex].warheadLauncherLastSlot[launcherIdx];
			for (warheadSlot = firstSlot; warheadSlot <= lastSlot; ++warheadSlot) {
				if (g_curCraft->warheadData[warheadSlot].count != 0) {
					hasUsableWarhead = 1;
					break;
				}
			}

			++firstSlot;
			warheadSlot = (uint16_t)firstSlot;
		}
	}
	if (!hasUsableWarhead) {
		g_curCraft->warheadLockTicks = 0;
		return 0;
	}

	{
		uint16_t remainingSystemStrength;

		targetCraft = g_objectTable[targetObjIdx].mobj->pCraft;
		durabilityScore = (uint32_t)targetCraft->shieldFront;
		pendingWarheadDamage = 0;
		pendingIonPulseScore = 0;
		remainingSystemStrength =
			(uint16_t)(g_modelDefs[g_curCraft->modelIndex].systemStrength - g_curCraft->subsystemDamage);
		componentHitCount = 0;
		if (isDisablePlan) {
			for (launcherIdx = 0; launcherIdx < warheadLauncherCount; ++launcherIdx) {
				ObjectTypeId warheadType;

				warheadType = (ObjectTypeId)g_curCraft->warheadSlotTypeIds[launcherIdx];
				if (g_projectileWarheadClassByType[(uint16_t)warheadType - OBJ_LaserRebel] ==
						desiredWarheadClass ||
					(g_missionFormatVersion >= 14 && warheadType == OBJ_WarheadMagPulse &&
					 desiredWarheadClass == 2)) {
					uint32_t damage;

					damage = (uint32_t)g_projectileDamageByType[(uint16_t)warheadType - OBJ_LaserRebel];
					if (targetGenus == GENUS_Starship || targetGenus == GENUS_Platform) {
						damage >>= 4;
					}
					if (targetGenus == GENUS_Freighter || targetGenus == GENUS_Container) {
						damage >>= 2;
					}
					if (warheadType == OBJ_WarheadIonPulse) {
						pendingIonPulseScore = (int16_t)(pendingIonPulseScore + 5);
					} else {
						if (targetGenus <= GENUS_Transport) {
							durabilityScore = 0;
						}
						pendingWarheadDamage += damage;
					}
				}
			}
		} else {
			durabilityScore += (uint32_t)(targetCraft->hullMax - targetCraft->hullDamage);
		}

		for (projectileObjIdx = (uint16_t)g_projectileObjectSlotStart;
			 projectileObjIdx < (uint16_t)g_projectileObjectSlotEnd; ++projectileObjIdx) {
			ObjectRecord* projectileObj;

			projectileObj = &g_objectTable[projectileObjIdx];
			if (projectileObj->objectType != OBJ_None && (projectileObj->genusId == GENUS_PlayerProjectile ||
														  projectileObj->genusId == GENUS_NpcProjectile)) {
				WarheadGuidanceState* guidance;

				guidance = projectileObj->mobj->pWarheadGuidance;
				if (guidance->targetObjIdx == targetObjIdx) {
					uint32_t damage;

					damage = (uint32_t)
						g_projectileDamageByType[(uint16_t)projectileObj->objectType - OBJ_LaserRebel];
					if (targetGenus == GENUS_Starship || targetGenus == GENUS_Platform) {
						damage >>= 4;
					}
					if (targetGenus == GENUS_Freighter || targetGenus == GENUS_Container) {
						damage >>= 2;
					}
					pendingWarheadDamage += damage;
					if (projectileObj->objectType == OBJ_WarheadIonPulse) {
						pendingIonPulseScore = (int16_t)(pendingIonPulseScore + 30);
					}
					if (guidance->targetComponentIdx == targetComponent && targetComponent != 0xffffu &&
						targetCraft->componentHp[targetComponent] != 0xffu) {
						++componentHitCount;
					}
				}
			}
		}

		if (pendingWarheadDamage >= durabilityScore &&
			(!isDisablePlan || pendingIonPulseScore == 0 ||
			 pendingIonPulseScore >= (int16_t)remainingSystemStrength)) {
			g_curCraft->warheadLockTicks =
				(uint16_t)(g_curCraft->warheadLockTicks - (uint16_t)g_paiContext.aiController->thinkInterval);
			goto clamp_negative_lock_ticks;
		}
	}

	homingWarheadCount = 0;
	for (projectileObjIdx = (uint16_t)g_projectileObjectSlotStart;
		 projectileObjIdx < (uint16_t)g_projectileObjectSlotEnd; ++projectileObjIdx) {
		ObjectRecord* projectileObj;

		projectileObj = &g_objectTable[projectileObjIdx];
		if (projectileObj->objectType != OBJ_None &&
			(g_projectileWarheadClassByType[(uint16_t)projectileObj->objectType - OBJ_LaserRebel] ==
				 desiredWarheadClass ||
			 (g_missionFormatVersion >= 14 && projectileObj->objectType == OBJ_WarheadMagPulse &&
			  desiredWarheadClass == 2))) {
			WarheadGuidanceState* guidance;

			guidance = projectileObj->mobj->pWarheadGuidance;
			if (guidance->homingTier != 0 && guidance->targetObjIdx == targetObjIdx) {
				++homingWarheadCount;
				if (g_missionFormatVersion >= 14 && projectileObj->objectType == OBJ_WarheadMagPulse &&
					desiredWarheadClass == 2) {
					homingWarheadCount += 7;
				}
			}
		}
	}

	if ((uint16_t)homingWarheadCount >= maxComponentHits || componentHitCount >= 3 ||
		g_paiContext.aiController->maneuverMode != 23 || g_curCraft->weaponFireInhibitTimer != 0 ||
		g_curCraft->beamEffectAccum[2] != 0 ||
		g_curCraft->aiFlight.maneuverCounter >= (uint16_t)maxWarheadShots) {
		g_curCraft->warheadLockTicks =
			(uint16_t)(g_curCraft->warheadLockTicks - (uint16_t)g_paiContext.aiController->thinkInterval);
		goto clamp_negative_lock_ticks;
	}

	if (yawDelta >= 0x300u || pitchDelta >= 0x300u || (uint32_t)trig2_polardistance >= fireRangeLimit) {
		g_curCraft->warheadLockTicks =
			(uint16_t)(g_curCraft->warheadLockTicks - (uint16_t)g_paiContext.aiController->thinkInterval);
		goto clamp_negative_lock_ticks;
	}

	if (targetGenus == GENUS_Starship || targetGenus == GENUS_Platform || targetGenus == GENUS_Freighter ||
		targetGenus == GENUS_Container || (targetGenus == GENUS_Transport && g_missionFormatVersion >= 14)) {
		if (Object_IsHostileToTeam((uint16_t)g_paiContext.aiObjIdx,
								   (uint16_t)g_players[g_localPlayer].playerIff)) {
			if (!g_fsfxEnemyWarheadAttackCalloutPlayed) {
				fsfx_speakorderack(g_localPlayer, -1, 34, 4, 0xffffu, 0xc000u);
				g_fsfxEnemyWarheadAttackCalloutPlayed = 1;
			}
		} else {
			fsfx_speakorderack(g_localPlayer, g_paiContext.aiObjIdx, 9, -1, 0xffffu, 0xffffu);
		}
	}

	{
		uint16_t lockThreshold;

		lockThreshold = (uint16_t)(472u * (g_paiContext.aiSkillTier + 1u));
		if (g_paiContext.aiSkillTier == 2) {
			g_curCraft->warheadLockTicks =
				(uint16_t)(g_curCraft->warheadLockTicks + (uint16_t)g_paiContext.aiController->thinkInterval);
			if ((uint32_t)g_curCraft->hullDamage >= (uint32_t)g_curCraft->systemDamageHullThreshold) {
				lockThreshold >>= 1;
			}
		} else {
			g_curCraft->warheadLockTicks =
				(uint16_t)(g_curCraft->warheadLockTicks +
						   MATH2_fraction((uint16_t)g_paiContext.aiController->thinkInterval, 0xc000u));
		}

		if ((int16_t)g_curCraft->warheadLockTicks >= (int)lockThreshold) {
			CraftData* firingCraft;
			uint16_t invalidComponent;

			firingCraft = g_curCraft;
			invalidComponent = 0xffffu;
			for (launcherIdx = 0; launcherIdx < firingCraft->warheadLauncherCount; ++launcherIdx) {
				ObjectTypeId warheadType;

				warheadType = (ObjectTypeId)firingCraft->warheadSlotTypeIds[launcherIdx];
				if (firingCraft->warheadLauncherCooldownTicks[launcherIdx] == 0 &&
					(g_projectileWarheadClassByType[(uint16_t)warheadType - OBJ_LaserRebel] ==
						 desiredWarheadClass ||
					 acceptAnyWarhead ||
					 (g_missionFormatVersion >= 14 && warheadType == OBJ_WarheadMagPulse &&
					  desiredWarheadClass == 2 &&
					  (targetObjIdx == invalidComponent ||
					   g_objectTable[targetObjIdx].mobj->pCraft->weaponFireInhibitTimer == 0 ||
					   g_objectTable[targetObjIdx].mobj->pCraft->weaponFireInhibitTimer < 0x49cu)))) {
					uint16_t firstSlot;

					if ((g_paiContext.aiSkillTier == 2 &&
						 (uint32_t)firingCraft->hullDamage >=
							 (uint32_t)firingCraft->systemDamageHullThreshold &&
						 !isDisablePlan) ||
						g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
						firingCraft->warheadLauncherFlags[launcherIdx] = 3;
					} else {
						firstSlot =
							g_modelDefs[firingCraft->modelIndex].warheadLauncherFirstSlot[launcherIdx];
						if (firingCraft->warheadData[firstSlot].count >=
							firingCraft->warheadData[firstSlot + 1].count) {
							firingCraft->warheadLauncherFlags[launcherIdx] = 1;
						} else {
							firingCraft->warheadLauncherFlags[launcherIdx] = 0x81u;
						}
					}

					if (g_paiContext.aiController->targetComponent == invalidComponent) {
						g_paiContext.aiController->targetComponent =
							paifight_SelectTargetComponentMesh(targetObjIdx);
					}
					laser_firerocketsystem(g_paiContext.aiObjIdx, launcherIdx);
					++g_curCraft->aiFlight.maneuverCounter;
					targetCraft->attackedByTeam[g_objectTable[g_paiContext.aiObjIdx].mobj->team] = 1;
					firingCraft = g_curCraft;
				}
			}
		}
	}

clear_laser_link_modes: {
	CraftData* curCraft;

	curCraft = g_curCraft;
	for (cannonClassIdx = 0; cannonClassIdx < curCraft->cannonClassCount; ++cannonClassIdx) {
		curCraft->laserLinkMode[cannonClassIdx] = 0;
		curCraft = g_curCraft;
	}
}
	return 0;

clamp_negative_lock_ticks:
	if ((int16_t)g_curCraft->warheadLockTicks < 0) {
		g_curCraft->warheadLockTicks = 0;
	}
	return 0;
}

// FUNCTION: XWA 0x4A8AA0
char paifight_gunnerselfdefenseorder(void) {
	unsigned int laserSlot;
	unsigned int aiObjIdx;
	CraftData* craft;
	unsigned int projectileObjIdx;
	uint16_t candidateObjIdx;
	uint32_t defenseRangeLimit;
	uint8_t flightGroupIdx;

	g_paiContext.aiRequireLiveOrderTarget = 0;
	flightGroupIdx = g_objectTable[g_paiContext.aiObjIdx].flightGroupIdx;
	if (g_missionFlightGroups[flightGroupIdx].fg.status1 != 14 &&
		g_missionFlightGroups[flightGroupIdx].fg.status2 != 14) {
		craft = g_curCraft;
		for (laserSlot = 0; laserSlot < craft->laserSlotCount; ++laserSlot) {
			unsigned char weaponType;

			weaponType = craft->warheadData[laserSlot].weaponType;
			if (weaponType < 4u || craft->warheadData[laserSlot].turretRetargetCooldownTimer > 0) {
				continue;
			}

			craft->warheadData[laserSlot].turretTargetObjIdx = -1;
			g_curCraft->warheadData[laserSlot].count = 0;
			g_curCraft->warheadData[laserSlot].lastFireMeshIdx = 0xffu;
			g_curCraft->warheadData[laserSlot].lastFireHardpointIdx = 0xffu;
			craft = g_curCraft;
			if (craft->objectKind == 3 || craft->workingSubsystems == 0 ||
				craft->weaponFireInhibitTimer != 0) {
				continue;
			}

			paifight_SetGunnerSelfDefenseSearchOrigin(laserSlot);

			{
				uint16_t lastAttackerObjIdx;

				lastAttackerObjIdx = craft->lastAttackerObjIdx;
				if (pai_IsObjectTargetable(lastAttackerObjIdx)) {
					uint32_t rangeScore;

					rangeScore = (uint32_t)collide_roughdistance3d(
						g_objectTable[lastAttackerObjIdx].world_x - g_paifightSearchOriginX,
						g_objectTable[lastAttackerObjIdx].world_y - g_paifightSearchOriginY,
						g_objectTable[lastAttackerObjIdx].world_z - g_paifightSearchOriginZ);
					g_targetRangeScore = (int)rangeScore;
					defenseRangeLimit =
						(uint32_t)g_modelDefs[craft->modelIndex]
							.laserGroupFireRange[craft->warheadData[laserSlot].weaponGroupIdx];
					if (defenseRangeLimit == 0) {
						defenseRangeLimit = weaponType != 4u ? 0x20000u : 0x10000u;
					}
					if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
						defenseRangeLimit +=
							g_modelTypeTable[(uint16_t)g_objectTable[g_paiContext.aiObjIdx].objectType]
								.maxBoundsExtent;
					}
					if (weaponType != 5u ||
						(g_objectTable[lastAttackerObjIdx].genusId != GENUS_Fighter &&
						 g_objectTable[lastAttackerObjIdx].genusId != GENUS_Transport) ||
						rangeScore >= 0x10000u) {
						rangeScore = paifight_ApplyTurretTargetRangePenalty(lastAttackerObjIdx, rangeScore);
						if (rangeScore < defenseRangeLimit &&
							paifight_HasClearTurretSelfDefenseShotYXZ(lastAttackerObjIdx)) {
							if (!((strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
										  "board2pln") == 0 &&
								   g_paiContext.aiController->targetObjIdx == lastAttackerObjIdx) ||
								  (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
										  "disableldr1pln") == 0 &&
								   g_paiContext.aiController->targetObjIdx == lastAttackerObjIdx &&
								   g_objectTable[g_paiContext.aiController->targetObjIdx]
										   .mobj->pCraft->shieldFront == 0))) {
								paifight_AssignGunnerTurretTarget(laserSlot, (int16_t)lastAttackerObjIdx);
								craft = g_curCraft;
								continue;
							}
						}
					}
				} else {
					g_curCraft->lastAttackerObjIdx = 0xffffu;
					craft = g_curCraft;
				}
			}

			if (weaponType == 4u) {
				uint16_t projectileCursor;
				int16_t bestProjectileObjIdx;

				bestProjectileObjIdx = -1;
				defenseRangeLimit = (uint32_t)g_modelDefs[craft->modelIndex]
										.laserGroupFireRange[craft->warheadData[laserSlot].weaponGroupIdx];
				if (defenseRangeLimit == 0) {
					defenseRangeLimit = 0x10000u;
				}
				if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
					defenseRangeLimit +=
						g_modelTypeTable[(uint16_t)g_objectTable[g_paiContext.aiObjIdx].objectType]
							.maxBoundsExtent;
				}

				projectileCursor = (uint16_t)g_projectileObjectSlotStart;
				for (projectileObjIdx = projectileCursor; projectileObjIdx < g_projectileObjectSlotEnd;
					 projectileObjIdx = ++projectileCursor) {
					ObjectRecord* projectileObj;
					uint32_t rangeScore;

					projectileObj = &g_objectTable[projectileObjIdx];
					if (projectileObj->objectType == OBJ_None || projectileObj->genusId == GENUS_Explosion ||
						!paifight_IsWarheadClassProjectile(projectileObj->objectType) ||
						projectileObj->mobj->pWarheadGuidance->targetObjIdx != g_paiContext.aiObjIdx) {
						continue;
					}

					rangeScore =
						(uint32_t)collide_roughdistance3d(projectileObj->world_x - g_paifightSearchOriginX,
														  projectileObj->world_y - g_paifightSearchOriginY,
														  projectileObj->world_z - g_paifightSearchOriginZ);
					g_targetRangeScore = (int)rangeScore;
					if (rangeScore >= defenseRangeLimit ||
						paifight_GunnerSelfDefenseTargetIsAlreadyCovered(projectileObjIdx)) {
						continue;
					}
					if (!paifight_HasClearTurretSelfDefenseShotXYZ(projectileObjIdx)) {
						continue;
					}

					defenseRangeLimit = (uint32_t)g_targetRangeScore;
					bestProjectileObjIdx = (int16_t)projectileObjIdx;
				}

				if (bestProjectileObjIdx != -1) {
					paifight_AssignGunnerTurretTarget(laserSlot, bestProjectileObjIdx);
					craft = g_curCraft;
					continue;
				}
			}

			{
				uint16_t bestLargeObjIdx;
				uint16_t bestOtherObjIdx;
				uint32_t bestLargeRange;
				uint32_t bestOtherRange;

				bestLargeObjIdx = 0xffffu;
				bestOtherObjIdx = 0xffffu;
				bestLargeRange = (uint32_t)g_modelDefs[craft->modelIndex]
									 .laserGroupFireRange[craft->warheadData[laserSlot].weaponGroupIdx];
				if (bestLargeRange == 0) {
					bestLargeRange = 0x10000u;
				}
				bestOtherRange = (uint32_t)g_modelDefs[craft->modelIndex]
									 .laserGroupFireRange[craft->warheadData[laserSlot].weaponGroupIdx];
				if (bestOtherRange == 0) {
					bestOtherRange = 0x10000u;
				}

#ifndef XWA_MODERN
				if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
					defenseRangeLimit +=
						g_modelTypeTable[(uint16_t)g_objectTable[g_paiContext.aiObjIdx].objectType]
							.maxBoundsExtent;
				}
#endif

				for (candidateObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
					 candidateObjIdx < (uint32_t)g_activeRegionCraftObjectSlotEnd; ++candidateObjIdx) {
					CraftData* candidateCraft;
					uint32_t rangeScore;
					ModelGenusId genusId;

					if (g_objectTable[candidateObjIdx].objectType == OBJ_None) {
						continue;
					}

					candidateCraft = g_objectTable[candidateObjIdx].mobj->pCraft;
					if (!paifight_IsCraftAttackingSelf(candidateObjIdx, candidateCraft)) {
						continue;
					}
					if (!pai_IsObjectTargetable(candidateObjIdx)) {
						continue;
					}

					rangeScore = (uint32_t)collide_roughdistance3d(
						g_objectTable[candidateObjIdx].world_x - g_paifightSearchOriginX,
						g_objectTable[candidateObjIdx].world_y - g_paifightSearchOriginY,
						g_objectTable[candidateObjIdx].world_z - g_paifightSearchOriginZ);
					g_targetRangeScore = (int)rangeScore;
					if (weaponType == 5u &&
						(g_objectTable[candidateObjIdx].genusId == GENUS_Fighter ||
						 g_objectTable[candidateObjIdx].genusId == GENUS_Transport) &&
						rangeScore < 0x10000u) {
						continue;
					}

					craft = g_curCraft;
					if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
						rangeScore = paifight_ApplyTurretTargetRangePenalty(candidateObjIdx, rangeScore);
					}

					genusId = g_objectTable[candidateObjIdx].genusId;
					if (genusId == GENUS_Starship || genusId == GENUS_Platform ||
						genusId == GENUS_Freighter) {
						if (rangeScore < bestLargeRange &&
							paifight_HasClearTurretSelfDefenseShotZXY(candidateObjIdx)) {
							bestLargeRange = (uint32_t)g_targetRangeScore;
							bestLargeObjIdx = candidateObjIdx;
						}
					} else if (rangeScore < bestOtherRange &&
							   paifight_HasClearTurretSelfDefenseShotYXZ(candidateObjIdx)) {
						bestOtherRange = (uint32_t)g_targetRangeScore;
						bestOtherObjIdx = candidateObjIdx;
					}
				}

				if (bestOtherObjIdx == 0xffffu && bestLargeObjIdx == 0xffffu) {
					craft = g_curCraft;
					continue;
				}
				if (weaponType == 4u && bestOtherObjIdx == 0xffffu) {
					bestOtherObjIdx = bestLargeObjIdx;
				}

				if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "disableldr1pln") ==
						0 &&
					g_paiContext.aiController->targetObjIdx == bestOtherObjIdx) {
					MobileObject* targetMobj;

					targetMobj = g_objectTable[g_paiContext.aiController->targetObjIdx].mobj;
					if (targetMobj != NULL && targetMobj->pCraft != NULL &&
						targetMobj->pCraft->shieldFront != 0 && bestOtherObjIdx != 0xffffu) {
						paifight_AssignGunnerTurretTarget(laserSlot, bestOtherObjIdx);
					}
				} else if (bestOtherObjIdx != 0xffffu) {
					paifight_AssignGunnerTurretTarget(laserSlot, bestOtherObjIdx);
				}
				craft = g_curCraft;
			}
		}
	} else {
		craft = g_curCraft;
	}

	aiObjIdx = g_paiContext.aiObjIdx;
	if (g_objectTable[aiObjIdx].genusId != GENUS_Fighter) {
		return 0;
	}
	if (craft->cmTypeId == 0 || craft->cmAmmoCount == 0) {
		return 0;
	}

	{
		unsigned int threatObjIdx;
		uint32_t threatRange;

		threatObjIdx = g_projectileObjectSlotStart;
		threatRange = g_aiWarheadThreatRangeBySkill[g_paiContext.aiSkillTier];
		for (projectileObjIdx = (uint16_t)threatObjIdx;; projectileObjIdx = (uint16_t)++threatObjIdx) {
			ObjectRecord* projectileObj;
			WarheadGuidanceState* guidance;
			uint32_t maxRangeScore;

			if (projectileObjIdx >= g_projectileObjectSlotEnd) {
				return 0;
			}
			projectileObj = &g_objectTable[projectileObjIdx];
			if (projectileObj->objectType == OBJ_None) {
				continue;
			}

			guidance = projectileObj->mobj->pWarheadGuidance;
			if (guidance->homingTier == 0 || guidance->targetObjIdx != aiObjIdx) {
				continue;
			}

			if (projectileObj->objectType == OBJ_WarheadMissile ||
				projectileObj->objectType == OBJ_WarheadAdvancedMissile) {
				maxRangeScore = 3u * threatRange;
			} else {
				maxRangeScore = threatRange;
			}
			if (pai_IsObjectWithinCurrentPointRange(projectileObjIdx, maxRangeScore) == 1) {
				break;
			}
		}
		craft = g_curCraft;
		if ((craft->workingSubsystems & 2u) == 0) {
			return 0;
		}

		if (craft->cmTypeId == 1) {
			if (craft->chaffActiveTimer == 0) {
				craft->chaffActiveTimer = 10;
				if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.status1 !=
						21 &&
					g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.status2 !=
						21) {
					--g_curCraft->cmAmmoCount;
				}
			}
		} else if (craft->cmTypeId == 2) {
			int flareAlreadyTrackingThreat;
			unsigned int flareCursor;

			flareAlreadyTrackingThreat = 0;
			flareCursor = g_projectileObjectSlotStart;
			for (projectileObjIdx = (uint16_t)flareCursor; projectileObjIdx < g_projectileObjectSlotEnd;
				 projectileObjIdx = (uint16_t)++flareCursor) {
				ObjectRecord* projectileObj;

				projectileObj = &g_objectTable[projectileObjIdx];
				if (projectileObj->objectType == OBJ_WarheadFlare) {
					WarheadGuidanceState* guidance;

					guidance = projectileObj->mobj->pWarheadGuidance;
					if (guidance->homingTier != 0 && guidance->targetObjIdx == (uint16_t)threatObjIdx) {
						flareAlreadyTrackingThreat = 1;
						break;
					}
				}
			}

			craft = g_curCraft;
			if (!flareAlreadyTrackingThreat && craft->cmFireCooldownTimer == 0) {
				laser_createcountermeasureprojectile(g_paiContext.aiObjIdx, OBJ_WarheadFlare);
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4AA120
int16_t paifight_FindNearestGunnerTargetInCandidateSet(uint16_t target1Type, uint16_t target1,
													   int16_t target1OrTarget2, uint16_t target2Type,
													   uint16_t target2, int candidateSetIdx) {
	uint16_t bestObjIdx;
	uint32_t bestRangeScore;
	uint16_t objectIdx;

	if ((uint16_t)target1Type == 0 && (uint16_t)target2Type == 0) {
		return -1;
	}

	bestObjIdx = 0xffffu;
	bestRangeScore = g_paiContext.aiTargetSearchRangeLimit;
	if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
		bestRangeScore +=
			(uint32_t)g_modelTypeTable[(uint16_t)g_objectTable[g_paiContext.aiObjIdx].objectType]
				.maxBoundsExtent;
	}

	objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
	while ((uint32_t)objectIdx < g_activeRegionCraftObjectSlotEnd) {
		if (g_paifightGunnerTargetCandidateSet[2u * objectIdx + (uint32_t)candidateSetIdx] != 0) {
			uint32_t rangeScore;

			rangeScore =
				(uint32_t)collide_roughdistance3d(g_objectTable[objectIdx].world_x - g_paifightSearchOriginX,
												  g_objectTable[objectIdx].world_y - g_paifightSearchOriginY,
												  g_objectTable[objectIdx].world_z - g_paifightSearchOriginZ);
			g_targetRangeScore = (int)rangeScore;

			if (g_paiContext.aiTargetSearchFlags != 0x50u ||
				(g_objectTable[objectIdx].genusId != GENUS_Fighter &&
				 g_objectTable[objectIdx].genusId != GENUS_Transport) ||
				rangeScore >= 0x10000u) {
				if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
					WarheadInventoryEntry* weapon;
					int laserSlot;

					laserSlot = 0;
					weapon = g_curCraft->warheadData;
					while (laserSlot < g_curCraft->laserSlotCount) {
						if (weapon->turretTargetObjIdx == (int16_t)objectIdx) {
							rangeScore += 0x8000u;
							g_targetRangeScore = (int)rangeScore;
						}
						++laserSlot;
						++weapon;
					}
				}

				if (rangeScore < bestRangeScore) {
					char hasClearSweep;

					if (!g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
						Mission_ResolveObjectOrMissionPointWorldLoc(objectIdx, 0, 0, 0);
						g_collisionProbeWorldX = worldlocx;
						g_collisionProbeWorldY = worldlocy;
						g_collisionProbeWorldZ = worldlocz;
						g_collisionStagedModelProbe = 1;
						if (collide_CheckSweptModelCollision(g_paiContext.aiObjIdx, g_paiContext.aiObjIdx) ==
							0) {
							hasClearSweep = 1;
						} else {
							hasClearSweep = 0;
						}
						g_collisionStagedModelProbe = 0;
						rangeScore = (uint32_t)g_targetRangeScore;
					} else {
						hasClearSweep = 1;
					}

					if (hasClearSweep) {
						bestRangeScore = rangeScore;
						bestObjIdx = objectIdx;
					}
				}
			}
		}

		++objectIdx;
	}

	objectIdx = (uint16_t)g_objScanStart;
	while ((uint32_t)objectIdx < g_regionStaticObjectSlotEnd) {
		if (g_objectTable[objectIdx].objectType != OBJ_None &&
			(g_modelTypeTable[(uint16_t)g_objectTable[objectIdx].objectType].flags & 2u) != 0) {
			int matchesTarget1;
			int matchesTarget2;
			matchesTarget1 = Mission_ObjectMatchesTriggerVariable(objectIdx, target1Type, target1);
			matchesTarget2 = Mission_ObjectMatchesTriggerVariable(objectIdx, target2Type, target2);
			if (target1OrTarget2 == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if ((uint16_t)matchesTarget1 && pai_IsObjectTargetable(objectIdx)) {
				uint32_t rangeScore;

				Mission_ResolveObjectOrMissionPointWorldLoc(objectIdx, 0, 0, 0);
				rangeScore = (uint32_t)collide_roughdistance3d(worldlocx - g_paifightSearchOriginX,
															   worldlocy - g_paifightSearchOriginY,
															   worldlocz - g_paifightSearchOriginZ);
				g_targetRangeScore = (int)rangeScore;
				if (g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
					WarheadInventoryEntry* weapon;
					int laserSlot;

					laserSlot = 0;
					weapon = g_curCraft->warheadData;
					while (laserSlot < g_curCraft->laserSlotCount) {
						if (weapon->turretTargetObjIdx == (int16_t)objectIdx) {
							rangeScore += 0x8000u;
							g_targetRangeScore = (int)rangeScore;
						}
						++laserSlot;
						++weapon;
					}
				}

				if (rangeScore < bestRangeScore) {
					int hasClearSweep;

					if (!g_paiContext.aiSelfModelUsesExpandedTargetProbe) {
						g_collisionProbeWorldX = worldlocx;
						g_collisionProbeWorldY = worldlocy;
						g_collisionProbeWorldZ = worldlocz;
						hasClearSweep = collide_CheckSweptModelCollision(g_paiContext.aiObjIdx,
																		 g_paiContext.aiObjIdx) == 0;
						rangeScore = (uint32_t)g_targetRangeScore;
					} else {
						hasClearSweep = 1;
					}

					if (hasClearSweep) {
						bestRangeScore = rangeScore;
						bestObjIdx = objectIdx;
					}
				}
			}
		}

		++objectIdx;
	}

	if (!g_paiContext.aiSelfModelUsesExpandedTargetProbe &&
		bestRangeScore > g_paiContext.aiTargetSearchRangeLimit) {
		bestObjIdx = 0xffffu;
	}
	if (bestObjIdx != 0xffffu && g_missionFormatVersion >= 14) {
		CraftData* savedCurCraft;
		uint16_t blockerObjIdx;

		savedCurCraft = g_curCraft;
		blockerObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
		while ((uint32_t)blockerObjIdx < g_activeRegionCraftObjectSlotEnd) {
			if (g_objectTable[blockerObjIdx].objectType != OBJ_None && blockerObjIdx != bestObjIdx &&
				blockerObjIdx != (uint16_t)g_paiContext.aiObjIdx) {
				if (Object_IsFriendlyToTeam(blockerObjIdx, g_objectTable[g_paiContext.aiObjIdx].mobj->team)) {
					ModelGenusId blockerGenus;

					if (g_objectTable[g_paiContext.aiObjIdx].mobj->speed == 0 ||
						((blockerGenus = g_objectTable[blockerObjIdx].genusId) != GENUS_Fighter &&
						 blockerGenus != GENUS_Transport && blockerGenus != GENUS_PilotDroid &&
						 blockerGenus != GENUS_WeaponEmplacement && blockerGenus != GENUS_Utility)) {
						int rangeScore;

						rangeScore = collide_roughdistance3d(
							g_objectTable[blockerObjIdx].world_x - g_paifightSearchOriginX,
							g_objectTable[blockerObjIdx].world_y - g_paifightSearchOriginY,
							g_objectTable[blockerObjIdx].world_z - g_paifightSearchOriginZ);
						g_targetRangeScore = rangeScore;
						g_targetRangeScore =
							rangeScore - g_modelTypeTable[(uint16_t)g_objectTable[blockerObjIdx].objectType]
											 .maxBoundsExtent;
						if ((uint32_t)g_targetRangeScore <= bestRangeScore) {
							g_collisionProbeWorldX = g_objectTable[bestObjIdx].world_x;
							g_collisionProbeWorldY = g_objectTable[bestObjIdx].world_y;
							g_collisionProbeWorldZ = g_objectTable[bestObjIdx].world_z;
							if (collide_CheckSweptModelCollision(blockerObjIdx, blockerObjIdx) != 0) {
								bestObjIdx = 0xffffu;
								break;
							}
						}
					}
				}
			}

			++blockerObjIdx;
		}

		g_curCraft = savedCurCraft;
	}

	return (int16_t)bestObjIdx;
}

// FUNCTION: XWA 0x4A6930
int paifight_TargetHasAttackCapacity(uint16_t targetObjIdx, uint16_t candidateCount) {
	uint16_t activeObjIdx;
	uint16_t attackerCount;
	uint16_t targetLimit;

	attackerCount = 0;
	activeObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
	while (activeObjIdx < g_activeRegionCraftObjectSlotEnd) {
		if (g_objectTable[activeObjIdx].objectType != OBJ_None) {
			CraftData* craft;
			AiController* aiController;

			craft = g_objectTable[activeObjIdx].mobj->pCraft;
			aiController = pai_GetEffectiveAIController(craft);
			if (aiController->targetObjIdx == targetObjIdx && activeObjIdx != targetObjIdx &&
				aiController->targetObjIdx != (uint16_t)craft->playerCommandAvoidTargetObjIdx) {
				uint8_t maneuverMode;

				maneuverMode = aiController->maneuverMode;
				if (maneuverMode == 11 || maneuverMode == 12 || maneuverMode == 23) {
					if (strcmp(g_planTable[aiController->currentPlanId].name, "inspectldr1pln") != 0) {
						++attackerCount;
					}
				}
			}
		}

		++activeObjIdx;
	}

	{
		unsigned int targetObjIdxWide;

		targetObjIdxWide = targetObjIdx;
		if (targetObjIdxWide >= g_activeRegionObjectSlotStart &&
			targetObjIdxWide < g_activeRegionCraftObjectSlotEnd) {
			if (candidateCount > 1u) {
				ModelGenusId genusId;

				genusId = g_objectTable[targetObjIdx].genusId;
				if (genusId == GENUS_Starship || genusId == GENUS_Platform) {
					targetLimit = 6;
				} else if (genusId == GENUS_Freighter || genusId == GENUS_Container) {
					targetLimit = 4;
				} else {
					targetLimit = 2;
				}
			} else {
				targetLimit = candidateCount == 1 ? 100 : 1;
			}
		} else {
			targetLimit = 2;
		}

		if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
			switch (g_flightDifficulty) {
				case 0:
					targetLimit = candidateCount == 1 ? 4 : 2;
					break;
				case 1:
					targetLimit = candidateCount == 1 ? 8 : 3;
					break;
				case 2:
					targetLimit = candidateCount == 1 ? 100 : 4;
					break;
			}
		}
	}

	return attackerCount < targetLimit;
}

// FUNCTION: XWA 0x4AB610
char paifight_CanCountMagPulseAsRocket(uint16_t sourceObjIdx, ObjectTypeId warheadType,
									   int desiredWarheadClass, int ignoreFireInhibit) {
	CraftData* sourceCraft;

	if (g_missionFormatVersion < 14 || (uint16_t)warheadType != OBJ_WarheadMagPulse ||
		(uint16_t)desiredWarheadClass != 2) {
		return 0;
	}

	if ((uint16_t)ignoreFireInhibit || sourceObjIdx == 0xffff) {
		return 1;
	}

	sourceCraft = g_objectTable[sourceObjIdx].mobj->pCraft;
	if (sourceCraft->weaponFireInhibitTimer) {
		if (sourceCraft->weaponFireInhibitTimer < 0x49c) {
			return 1;
		} else {
			return 0;
		}
	}

	return 1;
}
