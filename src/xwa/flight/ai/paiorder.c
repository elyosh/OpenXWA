#include "xwa/flight/ai/ai_internal.h"

// GLOBAL: XWA 0x5B7670
const unsigned int g_aiStillAttackLastAttackerRangeBySkill[4] = { 0x8000, 0xc000, 0xe000, 0 };

// GLOBAL: XWA 0x5B7680
const unsigned int g_aiAttackerSearchRangeBySkill[4] = { 0x2000, 0x3000, 0x4000, 0 };

// GLOBAL: XWA 0x5B7690
const unsigned int g_aiWarheadThreatRangeBySkill[4] = { 0x0800, 0x1000, 0x1800, 0 };

// GLOBAL: XWA 0x5B76A8
const uint8_t g_aiUnderAttackFrontManeuverChoices[4] = { 13, 14, 15, 3 };

// GLOBAL: XWA 0x5B76B0
const uint8_t g_aiUnderAttackSideRearManeuverChoices[8] = { 1, 15, 1, 1, 32, 32, 4, 32 };

// GLOBAL: XWA 0x5B76B8
PaiManeuverFunc g_orderTable[66] = {
	paiorder_nullhandler,                   /*  0: 0x4B8A30 */
	paiorder_updatecourseorder,             /*  1: 0x4B8A40 */
	paiorder_underattackorder,              /*  2: 0x4B8A60 */
	paiorder_stillattackorder,              /*  3: 0x4B8EB0 */
	paiorder_flyhomeorder,                  /*  4: 0x4B8F70 */
	paifight_fightershootorder,             /*  5: 0x4A75B0 */
	paifight_gunnerselfdefenseorder,        /*  6: 0x4A8AA0 */
	paifight_gunneroffenseorder,            /*  7: 0x4A98B0 */
	paifight_missiledefenseorder,           /*  8: 0x4A8520 */
	paifight_scanfortargetorder,            /*  9: 0x4A5410 */
	paiorder_waitrunorder,                  /* 10: 0x4B9B00 */
	paiorder_breakofforder,                 /* 11: 0x4B9B50 */
	paiorder_leaderdeadorder,               /* 12: 0x4BA340 */
	paifight_coverleaderorder,              /* 13: 0x4AAAA0 */
	paifight_followleadatkorder,            /* 14: 0x4AAC20 */
	paiorder_abortmissionorder,             /* 15: 0x4B9E40 */
	paiorder_ontailorder,                   /* 16: 0x4BA530 */
	paiorder_alwaysorder,                   /* 17: 0x4BA600 */
	paifight_checkescortorder,              /* 18: 0x4AB1B0 */
	paiorder_leadergohomeorder,             /* 19: 0x4BA610 */
	paiorder_hyperspaceorder,               /* 20: 0x4BA6F0 */
	paiorder_enterhangarorder,              /* 21: 0x4B9220 */
	paiorder_mothershiporder,               /* 22: 0x4BA910 */
	paifight_escorttargetorder,             /* 23: 0x4A7350 */
	paiorder_lookforcrafttoboardorder,      /* 24: 0x4BA970 */
	paiorder_abortboardorder,               /* 25: 0x4BAA40 */
	paiorder_returnboardorder,              /* 26: 0x4BAE10 */
	paiorder_awaitboardorder,               /* 27: 0x4BAB60 */
	paiorder_makedisabledorder,             /* 28: 0x4BAD50 */
	paiorder_returnboardorder,              /* 29: 0x4BAE10 */
	paiorder_rocketsonboardorder,           /* 30: 0x4BAE30 */
	paiorder_avoidhitorder,                 /* 31: 0x4BB000 */
	paiorder_waitforallreturnorder,         /* 32: 0x4BB530 */
	paiorder_waitforallcreateorder,         /* 33: 0x4BB640 */
	paiorder_evasiveorder,                  /* 34: 0x4BB6F0 */
	paiorder_targetfromplayerorder,         /* 35: 0x4BB740 */
	paiorder_avoidstarshiporder,            /* 36: 0x4BB7C0 */
	paiorder_checkhyperorder,               /* 37: 0x4BB910 */
	paiorder_stopgohomeorder,               /* 38: 0x4BB940 */
	paiorder_completegohomeorder,           /* 39: 0x4BBD50 */
	paiorder_completegootherorder,          /* 40: 0x4BBF70 */
	paiorder_completefolloworder,           /* 41: 0x4BC7C0 */
	paiorder_waitgootherorder,              /* 42: 0x4BC120 */
	paiorder_orderswitchorder,              /* 43: 0x4BC400 */
	paiorder_killselforder,                 /* 44: 0x4BC890 */
	paiorder_dropoffdestorder,              /* 45: 0x4BC930 */
	paiorder_abortmotherwaitorder,          /* 46: 0x4BCA30 */
	paiorder_checkconditionalorder,         /* 47: 0x4BC030 */
	paiorder_transfercargoorder,            /* 48: 0x4BCC40 */
	paiorder_selfcaptureorder,              /* 49: 0x4BCCA0 */
	paiorder_checkreleaseorder,             /* 50: 0x4BCDB0 */
	paiorder_checkdeliverorder,             /* 51: 0x4BCE70 */
	paiorder_changesidesorder,              /* 52: 0x4BD130 */
	paiorder_startoverorder,                /* 53: 0x4BD250 */
	paiorder_disappearorder,                /* 54: 0x4BD340 */
	paiorder_commandfromplayerorder,        /* 55: 0x4BD380 */
	paiorder_resumemissionorder,            /* 56: 0x4BDA00 */
	paifight_scanforplayertargettypeorder,  /* 57: 0x4AB680 */
	paiorder_inspectedorder,                /* 58: 0x4BDA30 */
	paifight_scanforplayerinspecttypeorder, /* 59: 0x4AB700 */
	paiorder_componentgoneorder,            /* 60: 0x4BDD40 */
	paiorder_repaironeselforder,            /* 61: 0x4BDD90 */
	paiorder_flyhomeotherregionorder,       /* 62: 0x4BDDF0 */
	paiorder_pickedupobjectorder,           /* 63: 0x4BE290 */
	paiorder_lookforparkorder,              /* 64: 0x4BE2B0 */
	paiorder_playergoneorder,               /* 65: 0x4BE560 */
};

// FUNCTION: XWA 0x4B8A30
char paiorder_nullhandler(void) { return 0; }

// FUNCTION: XWA 0x4BA600
char paiorder_alwaysorder(void) { return 1; }

// FUNCTION: XWA 0x4B8A40
char paiorder_updatecourseorder(void) {
	g_aiCourseOrderManeuverMode = g_aiCourseOrderManeuverTable[g_paiContext.aiController->maneuverMode];
	return g_aiCourseOrderManeuverMode();
}

// FUNCTION: XWA 0x4BCB70
void paiorder_selectorderslot(uint8_t orderSlot) {
	unsigned int order;

	g_paiContext.curOrderCoord.fields.orderSlot = orderSlot;
	g_paiContext.aiController->currentOrderSlot = (char)orderSlot;

	order = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx + orderSlot]
				.order;

	g_paiContext.aiController->currentPlanId =
		g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];

	if (g_paiContext.aiLeaderObjIdx == -1) {
		g_paiContext.nullPlanId = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
	} else {
		g_paiContext.nullPlanId = g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
	}

	if (g_paiContext.aiController->orderScratch
			.completionState[g_paiContext.curOrderCoord.fields.regionIdx][orderSlot] == 1) {
		g_paiContext.aiController->orderStateFlag = 1;
	} else {
		g_paiContext.aiController->orderStateFlag = 0;
	}
	g_paiContext.aiController->waypointIndex = 0;
	g_paiContext.aiController->candidateTargetIdx = 0xffff;

	return;
}

// FUNCTION: XWA 0x4BD250
char paiorder_startoverorder(void) {
	int orderSlotIndex;
	int remainingSlots;

	orderSlotIndex = 0;
	remainingSlots = 4;
	do {
		AiController* aiController;
		uint8_t completionState;
		uint8_t* goalProgressRow;

		aiController = g_paiContext.aiController;
		completionState = aiController->orderScratch
							  .completionState[g_paiContext.curOrderCoord.fields.regionIdx][orderSlotIndex];
		if (completionState == 3) {
			aiController->orderScratch
				.completionState[g_paiContext.curOrderCoord.fields.regionIdx][orderSlotIndex] = 0;
			goalProgressRow = g_paiContext.aiController->orderScratch
								  .goalProgress[g_paiContext.curOrderCoord.fields.regionIdx];
			goalProgressRow[orderSlotIndex] = 0;
		}
		if (completionState == 4) {
			aiController = g_paiContext.aiController;
			aiController->orderScratch
				.completionState[g_paiContext.curOrderCoord.fields.regionIdx][orderSlotIndex] = 1;
			goalProgressRow = g_paiContext.aiController->orderScratch
								  .goalProgress[g_paiContext.curOrderCoord.fields.regionIdx];
			goalProgressRow[orderSlotIndex] = 0;
		}
		++orderSlotIndex;
		--remainingSlots;
	} while (remainingSlots != 0);

	++g_paiContext.aiController->orderScratch.goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
														  [g_paiContext.curOrderCoord.fields.orderSlot];

	{
		uint8_t nextOrderSlot;

		for (nextOrderSlot = 0; nextOrderSlot < 4u; ++nextOrderSlot) {
			if (g_paiContext.aiController->orderScratch
					.completionState[g_paiContext.curOrderCoord.fields.regionIdx][nextOrderSlot] == 0) {
				paiorder_selectorderslot(nextOrderSlot);
				return 1;
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BE290
char paiorder_pickedupobjectorder(void) {
	if (g_curCraft->carriedObjectIndex != 0xffff) {
		return 1;
	} else {
		return 0;
	}
}

// FUNCTION: XWA 0x4BB910
char paiorder_checkhyperorder(void) {
	if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.departMethod == 1) {
		return 1;
	} else {
		return 0;
	}
}

// FUNCTION: XWA 0x4BDD40
char paiorder_componentgoneorder(void) {
	if (g_paiContext.aiController->targetObjIdx != 0xffff &&
		g_paiContext.aiController->targetComponent != 0xffff &&
		g_objectTable[g_paiContext.aiController->targetObjIdx]
				.mobj->pCraft->componentHp[g_paiContext.aiController->targetComponent] == 0) {
		return 1;
	}

	return 0;
}

static __inline uint16_t paiorder_FindMothershipObjectAnyRegion(const uint8_t* mothershipFlightGroupIdx) {
	uint32_t regionSlotBase;
	uint32_t scannedRegionCount;
	uint32_t craftSlotsPerRegion;
	uint32_t regionSlotsPerRegion;
	int regionObjByteStride;
	ObjectRecord* regionObj;

	regionSlotBase = 0;
	scannedRegionCount = 0;
	if ((uint32_t)g_activeMissionRegionCount > 0u) {
		craftSlotsPerRegion = g_craftObjectSlotsTotal / g_missionRegionCount;
		regionSlotsPerRegion = g_regionObjectSlotEnd / g_missionRegionCount;
		regionObjByteStride = (int)sizeof(ObjectRecord) * ((int)g_regionObjectSlotEnd / g_missionRegionCount);
		regionObj = g_objectTable;

		do {
			uint32_t objIdx;
			uint32_t regionCraftEnd;
			ObjectRecord* obj;

			objIdx = regionSlotBase;
			regionCraftEnd = regionSlotBase + craftSlotsPerRegion;
			obj = regionObj;
			while (objIdx < regionCraftEnd) {
				if (obj->objectType != 0) {
					CraftData* craft;
					uint8_t objectKind;

					craft = obj->mobj->pCraft;
					objectKind = craft->objectKind;
					if (objectKind != GENUS_Starship && objectKind != GENUS_Freighter &&
						obj->flightGroupIdx == *mothershipFlightGroupIdx && craft->leader_obj_idx == -1) {
						return (uint16_t)objIdx;
					}
				}
				++objIdx;
				++obj;
			}
			++scannedRegionCount;
			regionSlotBase += regionSlotsPerRegion;
			regionObj = (ObjectRecord*)((char*)regionObj + regionObjByteStride);
		} while (scannedRegionCount < (uint32_t)g_activeMissionRegionCount);
	}

	return 0xffffu;
}

static __inline char paiorder_FlyHomeViaMothership(uint16_t mothershipObjIdx) {
	uint8_t currentRegion;
	uint8_t mothershipRegion;
	uint32_t routeRegion;
	uint32_t firstOtherRegion;
	uint32_t secondOtherRegion;
	uint32_t regionIdx;
	uint8_t canRouteViaFirst;
	uint8_t canRouteViaSecond;

	if (mothershipObjIdx == 0xffffu) {
		return 0;
	}

	mothershipRegion = g_objectTable[mothershipObjIdx].regionIdx;
	currentRegion = g_paiContext.curOrderCoord.fields.regionIdx;
	if (mothershipRegion == currentRegion) {
		return 0;
	}

	if (g_missionRegionHyperPoints.departureRoutePointValid[currentRegion][mothershipRegion]) {
		routeRegion = mothershipRegion;
	} else {
		firstOtherRegion = currentRegion;
		secondOtherRegion = mothershipRegion;
		for (regionIdx = 0; regionIdx < (uint32_t)g_activeMissionRegionCount; ++regionIdx) {
			if (regionIdx != mothershipRegion && regionIdx != currentRegion &&
				firstOtherRegion == currentRegion) {
				firstOtherRegion = regionIdx;
			} else if (regionIdx != mothershipRegion && regionIdx != currentRegion &&
					   secondOtherRegion == mothershipRegion) {
				secondOtherRegion = regionIdx;
			}
		}

		canRouteViaFirst =
			g_missionRegionHyperPoints.departureRoutePointValid[currentRegion][firstOtherRegion];
		if (canRouteViaFirst &&
			g_missionRegionHyperPoints.departureRoutePointValid[firstOtherRegion][mothershipRegion]) {
			routeRegion = firstOtherRegion;
		} else {
			canRouteViaSecond =
				g_missionRegionHyperPoints.departureRoutePointValid[currentRegion][secondOtherRegion];
			if (canRouteViaSecond &&
				g_missionRegionHyperPoints.departureRoutePointValid[secondOtherRegion][mothershipRegion]) {
				routeRegion = secondOtherRegion;
			} else if (canRouteViaFirst &&
					   g_missionRegionHyperPoints
						   .departureRoutePointValid[firstOtherRegion][secondOtherRegion] &&
					   g_missionRegionHyperPoints
						   .departureRoutePointValid[secondOtherRegion][mothershipRegion]) {
				routeRegion = firstOtherRegion;
			} else if (canRouteViaSecond &&
					   g_missionRegionHyperPoints
						   .departureRoutePointValid[secondOtherRegion][firstOtherRegion] &&
					   g_missionRegionHyperPoints
						   .departureRoutePointValid[firstOtherRegion][mothershipRegion]) {
				routeRegion = secondOtherRegion;
			} else {
				return 0;
			}
		}
	}

	g_paiContext.aiController->aimPointX =
		g_missionRegionHyperPoints.departureRoutePoint[currentRegion][routeRegion].x;
	g_paiContext.aiController->aimPointY =
		g_missionRegionHyperPoints
			.departureRoutePoint[g_paiContext.curOrderCoord.fields.regionIdx][routeRegion]
			.y;
	g_paiContext.aiController->aimPointZ =
		g_missionRegionHyperPoints
			.departureRoutePoint[g_paiContext.curOrderCoord.fields.regionIdx][routeRegion]
			.z;
	g_paiContext.aiController->targetObjIdx = routeRegion;
	g_paiContext.aiController->currentPlanId = (uint8_t)pai_findplanbyname("homeviahyperspacepln");

	return 1;
}

// FUNCTION: XWA 0x4BDDF0
char paiorder_flyhomeotherregionorder(void) {
	uint16_t mothershipObjIdx;
	uint16_t flightGroupIdx;

	if (!g_modelDefs[g_curCraft->modelIndex].hasHyperdrive) {
		return 0;
	}

	mothershipObjIdx = 0xffffu;

	if (g_curCraft->wasCaptured) {
		flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
		if (g_missionFlightGroups[flightGroupIdx].fg.capturedDepartViaMothership != 0) {
			mothershipObjIdx = paiorder_FindMothershipObjectAnyRegion(
				&g_missionFlightGroups[flightGroupIdx].fg.capturedDepartureMothership);
		}
		return paiorder_FlyHomeViaMothership(mothershipObjIdx);
	}

	flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
	if (g_missionFlightGroups[flightGroupIdx].fg.departMethod == 1) {
		mothershipObjIdx = paiorder_FindMothershipObjectAnyRegion(
			&g_missionFlightGroups[flightGroupIdx].fg.departureMothership);
	}

	if (mothershipObjIdx == 0xffffu &&
		g_missionFlightGroups[flightGroupIdx].fg.alternateMothershipUsed == 1) {
		uint32_t regionSlotBase;
		uint32_t scannedRegionCount;
		uint32_t craftSlotsPerRegion;
		uint32_t regionSlotsPerRegion;
		int regionObjByteStride;
		ObjectRecord* regionObj;

		regionSlotBase = 0;
		scannedRegionCount = 0;
		if ((uint32_t)g_activeMissionRegionCount > 0u) {
			craftSlotsPerRegion = g_craftObjectSlotsTotal / g_missionRegionCount;
			regionSlotsPerRegion = g_regionObjectSlotEnd / g_missionRegionCount;
			regionObjByteStride =
				(int)sizeof(ObjectRecord) * ((int)g_regionObjectSlotEnd / g_missionRegionCount);
			regionObj = g_objectTable;

			do {
				uint32_t objIdx;
				uint32_t regionCraftEnd;
				ObjectRecord* obj;

				objIdx = regionSlotBase;
				regionCraftEnd = regionSlotBase + craftSlotsPerRegion;
				obj = regionObj;
				while (objIdx < regionCraftEnd) {
					if (obj->objectType != 0) {
						CraftData* craft;
						uint8_t objectKind;

						craft = obj->mobj->pCraft;
						objectKind = craft->objectKind;
						if (objectKind != GENUS_Starship && objectKind != GENUS_Freighter &&
							obj->flightGroupIdx ==
								g_missionFlightGroups[flightGroupIdx].fg.alternateMothership &&
							craft->leader_obj_idx == -1) {
							return paiorder_FlyHomeViaMothership((uint16_t)objIdx);
						}
					}
					++objIdx;
					++obj;
				}
				++scannedRegionCount;
				regionSlotBase += regionSlotsPerRegion;
				regionObj = (ObjectRecord*)((char*)regionObj + regionObjByteStride);
			} while (scannedRegionCount < (uint32_t)g_activeMissionRegionCount);
		}

		return paiorder_FlyHomeViaMothership(0xffffu);
	}

	return paiorder_FlyHomeViaMothership(mothershipObjIdx);
}

// FUNCTION: XWA 0x4BA910
char paiorder_mothershiporder(void) {
	ModelGenusId genusId;

	genusId = g_objectTable[g_paiContext.aiObjIdx].genusId;
	if (genusId == GENUS_WeaponEmplacement || genusId == GENUS_Container || genusId == GENUS_SatelliteBuoy ||
		genusId == GENUS_Platform) {
		return 0;
	}

	if (g_curCraft->leader_obj_idx == -1) {
		if (g_paiContext.aiController->targetObjIdx == 0x8003u) {
			return 1;
		}
		return 0;
	}

	if (pai_GetEffectiveAIController(g_paiContext.aiTargetCraft)->targetObjIdx == 0x8003u) {
		return 1;
	}
	return 0;
}

// FUNCTION: XWA 0x4B8F70
char paiorder_flyhomeorder(void) {
	uint16_t flightGroupIdx;
	uint16_t mothershipObjIdx;

	g_curCraft->aiFlight.separation = 1;
	mothershipObjIdx = 0xffffu;

	if (g_curCraft->wasCaptured) {
		flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
		if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.capturedDepartViaMothership != 0) {
			mothershipObjIdx = (uint16_t)pai_FindMothershipObject(
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.capturedDepartureMothership);
			flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
		}
	} else {
		flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
		if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.departMethod == 1) {
			mothershipObjIdx = (uint16_t)pai_FindMothershipObject(
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.departureMothership);
			flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
		}
		if (mothershipObjIdx == 0xffffu &&
			g_missionFlightGroups[flightGroupIdx].fg.alternateMothershipUsed == 1) {
			mothershipObjIdx = (uint16_t)pai_FindMothershipObject(
				g_missionFlightGroups[flightGroupIdx].fg.alternateMothership);
			flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
		}
	}

	if (mothershipObjIdx != 0xffffu) {
		if (g_objectTable[g_paiContext.aiObjIdx].genusId != GENUS_PilotDroid &&
			g_missionFlightGroups[g_objectTable[mothershipObjIdx].flightGroupIdx].playerOwnerIdx != -1) {
			mothershipObjIdx = 0xffffu;
		}

		if (mothershipObjIdx != 0xffffu) {
			g_paiContext.aiController->targetObjIdx = mothershipObjIdx;
			g_paiContext.aiController->targetSignature = g_objectTable[mothershipObjIdx].objectSignature;
			g_paiContext.aiController->hasLiveTarget = 0;
			{
				CraftData* mothershipCraft;
				uint16_t modelIndex;

				mothershipCraft = g_objectTable[mothershipObjIdx].mobj->pCraft;
				modelIndex = mothershipCraft->modelIndex;
				pai_RotateLocalVectorToWorldScratch(
					&g_objectTable[mothershipObjIdx], g_modelDefs[modelIndex].meshAttachData[8],
					g_modelDefs[modelIndex].meshAttachData[9], g_modelDefs[modelIndex].meshAttachData[10]);
				g_paiContext.aiController->aimPointX = g_objectTable[mothershipObjIdx].world_x + g_rotatedX;
				g_paiContext.aiController->aimPointY = g_objectTable[mothershipObjIdx].world_y + g_rotatedY;
				g_paiContext.aiController->aimPointZ = g_objectTable[mothershipObjIdx].world_z + g_rotatedZ;
			}

			pai_CalcAnglesToAimPoint();
			if (trig2_polardistance < 0x10000) {
				paiman_setspeed((int)g_paiContext.aiObjIdx, 150u);
			}
			if (trig2_polardistance < 0x8000) {
				paiman_setspeed((int)g_paiContext.aiObjIdx, 100u);
			}
			if (trig2_polardistance < 0x4000) {
				paiman_setspeed((int)g_paiContext.aiObjIdx, 75u);
			}

			if (trig2_polardistance < 2048) {
				return 1;
			}
			return 0;
		}
	}

	if (g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_HYPER].enabled != 0) {
		g_paiContext.aiController->targetObjIdx = 0x8003u;
	} else {
		g_paiContext.aiController->targetObjIdx = 0x8000u;
	}
	g_paiContext.aiController->targetSignature = 0;
	g_paiContext.aiController->hasLiveTarget = 0;
	pai_UpdateAimPointFromOrderTarget();
	return 0;
}

// FUNCTION: XWA 0x4B9220
char paiorder_enterhangarorder(void) {
	uint16_t mothershipObjIdx;
	uint16_t outcomeId;
	uint16_t speed;
	int stopDistance;
	int sFoilDistance;
	unsigned int objIdx;
	unsigned int distance;

	g_curCraft->aiFlight.separation = 1;
	g_paiContext.aiController->thinkInterval = 29;

	if (g_curCraft->wasCaptured) {
		mothershipObjIdx = (uint16_t)pai_FindMothershipObject(
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.capturedDepartureMothership);
		outcomeId = 20;
	} else {
		mothershipObjIdx = (uint16_t)pai_FindMothershipObject(
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.departureMothership);
		outcomeId = 18;
		if (mothershipObjIdx == 0xffffu &&
			g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.alternateMothershipUsed == 1) {
			mothershipObjIdx = (uint16_t)pai_FindMothershipObject(
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.alternateMothership);
			outcomeId = 19;
		}
	}

	if (mothershipObjIdx != 0xffffu) {
		CraftData* mothershipCraft;
		int modelIndex;

		g_paiContext.aiController->targetObjIdx = mothershipObjIdx;
		g_paiContext.aiController->targetSignature = g_objectTable[mothershipObjIdx].objectSignature;
		g_paiContext.aiController->hasLiveTarget = 0;

		mothershipCraft = g_objectTable[mothershipObjIdx].mobj->pCraft;
		modelIndex = mothershipCraft->modelIndex;
		pai_RotateLocalVectorToWorldScratch(
			&g_objectTable[mothershipObjIdx], g_modelDefs[modelIndex].meshAttachData[5],
			g_modelDefs[modelIndex].meshAttachData[6], g_modelDefs[modelIndex].meshAttachData[7]);
		g_paiContext.aiController->aimPointX = g_objectTable[mothershipObjIdx].world_x + g_rotatedX;
		g_paiContext.aiController->aimPointY = g_objectTable[mothershipObjIdx].world_y + g_rotatedY;
		g_paiContext.aiController->aimPointZ = g_objectTable[mothershipObjIdx].world_z + g_rotatedZ;
	}

	pai_CalcAnglesToAimPoint();
	if (mothershipObjIdx != 0xffffu) {
		speed = g_objectTable[mothershipObjIdx].mobj->speed;
		if (speed >= 25) {
			paiman_setspeed((int)g_paiContext.aiObjIdx, (unsigned int)(speed + 25));
		} else {
			paiman_setspeed((int)g_paiContext.aiObjIdx, 40u);
		}

		objIdx = g_paiContext.aiObjIdx;
		distance = trig2_polardistance;
		stopDistance = g_objectTable[g_paiContext.aiObjIdx].genusId != GENUS_Starship ? 512 : 1024;
		sFoilDistance = stopDistance + 4096;

		if ((int)trig2_polardistance < sFoilDistance) {
			uint8_t sFoilState;

			sFoilState = g_paiContext.aiSelfCraft->sFoilState;
			if (sFoilState != 2) {
				g_paiContext.aiSelfCraft->sFoilState = (uint8_t)(sFoilState | 3u);
				distance = trig2_polardistance;
				objIdx = g_paiContext.aiObjIdx;
			}
		}

		if (g_objectTable[objIdx].playerOwnerIdx != -1 && (int)distance < 2048) {
			paiman_setpower((int)objIdx, 0);
			return 0;
		}

		if ((int)distance < stopDistance) {
			unsigned int scanObjIdx;
			unsigned int savedScanObjIdx;
			uint16_t flightGroupIdx;
			CraftData* craft;

			scanObjIdx = g_activeRegionObjectSlotStart;
			savedScanObjIdx = scanObjIdx;
			while ((uint16_t)scanObjIdx < g_activeRegionCraftObjectSlotEnd) {
				uint16_t scanSlot;

				scanSlot = (uint16_t)scanObjIdx;
				if (g_objectTable[scanSlot].objectType != OBJ_None &&
					g_objectTable[scanSlot].flightGroupIdx ==
						g_paiContext.curOrderCoord.fields.flightGroupIdx) {
					AiController* aiController;

					craft = g_objectTable[scanSlot].mobj->pCraft;
					aiController = pai_GetEffectiveAIController(craft);
					if (craft->leader_obj_idx != -1 && aiController->maneuverMode == 10 &&
						g_objectTable[scanSlot].playerOwnerIdx == -1) {
						if (!craft->wasCaptured) {
							if (!aiController->orderStateFlag &&
								(craft->aiFlight.goHomeFlag ||
								 (!craft->aiFlight.missionAbortedFlag && !craft->aiFlight.departTimerFlag))) {
								uint16_t completeFgIdx;
								int specialCargoFlag;

								completeFgIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
								specialCargoFlag = 0;
								++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
									  .outcomeCount[16];
								if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.specialCargoCraft == craft->waveNumber) {
									g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.specialCargoOutcome[16] = 1;
									specialCargoFlag = 1;
								}
								Mission_ApplyTeamGoalScoreAllEnabledTeams(12, completeFgIdx,
																		  specialCargoFlag);
							}
							if (!craft->wasCaptured && aiController->orderStateFlag == 1 &&
								!craft->aiFlight.missionAbortedFlag && !craft->aiFlight.departTimerFlag) {
								++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
									  .outcomeCount[27];
								if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.specialCargoCraft == craft->waveNumber) {
									g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.specialCargoOutcome[27] = 1;
								}
							}
						}
						msg_emitCraftMessage((uint16_t)savedScanObjIdx, craft, 149);
						Mission_RecordCraftOutcome((uint16_t)savedScanObjIdx,
												   g_paiContext.curOrderCoord.fields.flightGroupIdx,
												   outcomeId);
						g_objectTable[scanSlot].objectType = OBJ_None;
						Craft_ClearEffectiveAiObjectLink(craft);
					}
				}
				++scanObjIdx;
				savedScanObjIdx = scanObjIdx;
			}

			craft = g_curCraft;
			if (!craft->wasCaptured) {
				if (!g_paiContext.aiController->orderStateFlag &&
					(craft->aiFlight.goHomeFlag ||
					 (!craft->aiFlight.missionAbortedFlag && !craft->aiFlight.departTimerFlag))) {
					uint16_t completeFgIdx;
					int specialCargoFlag;

					completeFgIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
					specialCargoFlag = 0;
					++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx].outcomeCount[16];
					if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg.specialCargoCraft == craft->waveNumber) {
						g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.specialCargoOutcome[16] = 1;
						specialCargoFlag = 1;
					}
					Mission_ApplyTeamGoalScoreAllEnabledTeams(12, completeFgIdx, specialCargoFlag);
					craft = g_curCraft;
				}
				if (!craft->wasCaptured && g_paiContext.aiController->orderStateFlag == 1 &&
					!craft->aiFlight.missionAbortedFlag && !craft->aiFlight.departTimerFlag) {
					++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx].outcomeCount[27];
					if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg.specialCargoCraft == craft->waveNumber) {
						g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.specialCargoOutcome[27] = 1;
					}
				}
			}

			flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
			if (craft->wasCaptured) {
				int team;
				unsigned int otherTeam;
				int specialCargoFlag;

				team = g_objectTable[g_paiContext.aiObjIdx].mobj->team;
				specialCargoFlag = 0;
				++g_missionFgStats[flightGroupIdx].teamCondition44Count[team];
				if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
					++g_missionFgStats[flightGroupIdx].teamCondition44SpecialCargo[team];
					specialCargoFlag = 1;
				}
				Mission_ApplyTeamGoalScoreForTeam(44, flightGroupIdx, specialCargoFlag, team);
				craft = g_curCraft;
				flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
				for (otherTeam = 0; otherTeam < 10u; ++otherTeam) {
					if (otherTeam != (unsigned int)team &&
						otherTeam != g_missionFlightGroups[flightGroupIdx].fg.team) {
						++g_missionFgStats[flightGroupIdx].teamCondition44OtherTeamCount[otherTeam];
						if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
							g_missionFgStats[flightGroupIdx].teamCondition44OtherTeamSpecialCargo[otherTeam] =
								1;
						}
					}
				}
			}

			msg_emitCraftMessage(g_paiContext.aiObjIdx, craft, 149);
			Mission_RecordCraftOutcome(g_paiContext.aiObjIdx,
									   g_paiContext.curOrderCoord.fields.flightGroupIdx, outcomeId);
			g_objectTable[g_paiContext.aiObjIdx].objectType = OBJ_None;
			Craft_ClearEffectiveAiObjectLink(g_curCraft);

			if (g_curCraft->carriedObjectIndex != 0xffffu) {
				uint16_t carriedObjectIdx;
				uint16_t carriedFlightGroupIdx;
				MobileObject* carriedMobj;
				CraftData* carriedCraft;

				carriedObjectIdx = g_curCraft->carriedObjectIndex;
				carriedMobj = g_objectTable[carriedObjectIdx].mobj;
				if (carriedMobj != NULL) {
					uint16_t outcomeFlightGroupIdx;
					uint8_t* specialCargoCraft;

					carriedFlightGroupIdx = g_objectTable[carriedObjectIdx].flightGroupIdx;
					Mission_RecordCraftOutcome(carriedObjectIdx, carriedFlightGroupIdx, outcomeId);
					carriedMobj = g_objectTable[carriedObjectIdx].mobj;
					carriedCraft = carriedMobj->pCraft;
					if (carriedCraft->wasCaptured) {
						uint8_t team;
						uint8_t otherTeam;
						int specialCargoFlag;

						team = carriedMobj->team;
						outcomeFlightGroupIdx = carriedFlightGroupIdx;
						++g_missionFgStats[carriedFlightGroupIdx].teamCondition44Count[team];
						specialCargoCraft =
							&g_missionFlightGroups[carriedFlightGroupIdx].fg.specialCargoCraft;
						if (*specialCargoCraft == carriedCraft->waveNumber) {
							++g_missionFgStats[carriedFlightGroupIdx].teamCondition44SpecialCargo[team];
							specialCargoFlag = 1;
						} else {
							specialCargoFlag = 0;
						}
						Mission_ApplyTeamGoalScoreForTeam(44, carriedFlightGroupIdx, specialCargoFlag, team);
						for (otherTeam = 0; otherTeam < 10u; ++otherTeam) {
							if (otherTeam != team &&
								otherTeam != g_missionFlightGroups[carriedFlightGroupIdx].fg.team) {
								++g_missionFgStats[carriedFlightGroupIdx]
									  .teamCondition44OtherTeamCount[otherTeam];
								if (*specialCargoCraft == carriedCraft->waveNumber) {
									g_missionFgStats[carriedFlightGroupIdx]
										.teamCondition44OtherTeamSpecialCargo[otherTeam] = 1;
								}
							}
						}
						++g_missionFgStats[outcomeFlightGroupIdx].outcomeCount[24];
						if (*specialCargoCraft == carriedCraft->waveNumber) {
							g_missionFgStats[outcomeFlightGroupIdx].specialCargoOutcome[24] = 1;
						}
					} else {
						int specialCargoFlag;

						specialCargoFlag = 0;
						outcomeFlightGroupIdx = carriedFlightGroupIdx;
						++g_missionFgStats[outcomeFlightGroupIdx].outcomeCount[23];
						specialCargoCraft =
							&g_missionFlightGroups[carriedFlightGroupIdx].fg.specialCargoCraft;
						if (*specialCargoCraft == carriedCraft->waveNumber) {
							g_missionFgStats[outcomeFlightGroupIdx].specialCargoOutcome[23] = 1;
							specialCargoFlag = 1;
						}
						Mission_ApplyTeamGoalScoreAllEnabledTeams(46, carriedFlightGroupIdx,
																  specialCargoFlag);
					}
					++g_missionFgStats[outcomeFlightGroupIdx].outcomeCount[28];
					if (*specialCargoCraft == carriedCraft->waveNumber) {
						g_missionFgStats[outcomeFlightGroupIdx].specialCargoOutcome[28] = 1;
					}
					g_objectTable[carriedObjectIdx].objectType = OBJ_None;
					Craft_ClearEffectiveAiObjectLink(carriedCraft);
				}
			}
		}
	} else {
		paiman_setspeed((int)g_paiContext.aiObjIdx, 35u);
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4BDD90
char paiorder_repaironeselforder(void) {
	if (!g_paiContext.aiController->maneuverTimer) {
		if (!g_curCraft->workingSubsystems) {
			g_curCraft->workingSubsystems = g_curCraft->systemFlags;
			g_curCraft->subsystemDamage = 0;
		}

		g_objectTable[g_paiContext.aiObjIdx].mobj->lifetimeTimer = 0;
	}

	return 0;
}

// FUNCTION: XWA 0x4BE560
char paiorder_playergoneorder(void) {
	int objectIndex;

	objectIndex = g_players[g_curCraft->followPlayerIdx].objectIndex;
	if (objectIndex == 0xffff) {
		return 0;
	}

	if (g_objectTable[objectIndex].objectType == OBJ_None) {
		return 1;
	}

	if (g_objectTable[objectIndex].regionIdx != g_objectTable[g_paiContext.aiObjIdx].regionIdx) {
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4BA530
char paiorder_ontailorder(void) {
	if (g_paiContext.aiController->maneuverMode == g_paiContext.aiPlanInitialManeuverId &&
		g_curCraft->lastAttackerObjIdx != 0xffffu) {
		uint16_t relBearing;

		pai_ObjectRefDirectionToObjectRef(g_paiContext.aiObjIdx, g_curCraft->lastAttackerObjIdx);
		relBearing = (uint16_t)(trig2_xyangle - g_objectTable[g_paiContext.aiObjIdx].yaw);
		if (g_aiThreatBearingClassByOctant[relBearing >> 13] == 2) {
			uint16_t choice;
			uint8_t maneuverMode;

			choice = (uint16_t)(GameRand() & 3);
			if (choice == 0) {
				maneuverMode = 1;
			} else if (choice == 1) {
				maneuverMode = 13;
			} else if (choice == 2) {
				maneuverMode = 4;
			} else {
				maneuverMode = 14;
			}
			g_paiContext.aiController->maneuverMode = maneuverMode;
			paiman_initmaneuver();
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BB7C0
char paiorder_avoidstarshiporder(void) {
	CraftData* savedCraft;
	uint16_t collisionObjIdx;

	if (g_paiContext.aiController->maneuverMode != 28) {
		savedCraft = g_curCraft;
		collisionObjIdx = collide_craftstarshipcollision(g_paiContext.aiObjIdx, 4);
		g_curCraft = savedCraft;
		if (collisionObjIdx != 0xffffu && g_paiContext.aiObjIdx != collisionObjIdx &&
			savedCraft->carriedObjectIndex != collisionObjIdx) {
			if ((savedCraft->waveNumber & 1u) != 0) {
				g_paiContext.aiController->targetXYAngle =
					(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw - 0x7000u);
			} else {
				g_paiContext.aiController->targetXYAngle =
					(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw + 0x7000u);
			}
			if (g_paiContext.aiController->targetZAngle > 0x4000u) {
				g_paiContext.aiController->targetZAngle =
					(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].pitch - 0x4000u);
			} else {
				g_paiContext.aiController->targetZAngle =
					(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].pitch + 0x4000u);
			}

			g_paiContext.aiController->maneuverMode = 28;
			paiman_initmaneuver();
			if (collisionObjIdx >= g_objScanStart && collisionObjIdx < g_regionStaticObjectSlotEnd) {
				g_paiContext.aiController->aiPlanState = 236 * ((GameRand() & 3) + 3);
			}
		}

	} else if (g_paiContext.aiController->aiPlanState == 0) {
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4B8A60
char paiorder_underattackorder(void) {
	uint8_t bearingClass;
	uint16_t randValue;
	uint8_t maneuverMode;
	uint16_t scanObjIdx;

	if ((g_objectTable[g_paiContext.aiObjIdx].genusId != GENUS_Fighter &&
		 g_objectTable[g_paiContext.aiObjIdx].genusId != GENUS_Transport) ||
		g_paiContext.aiController->maneuverMode != g_paiContext.aiPlanInitialManeuverId) {
		return 0;
	}

	if (g_curCraft->lastAttackerObjIdx == 0xffffu) {
		unsigned int warheadRange;

		scanObjIdx = g_projectileObjectSlotStart;
		warheadRange = g_aiWarheadThreatRangeBySkill[g_paiContext.aiSkillTier];
		while (scanObjIdx < g_projectileObjectSlotEnd) {
			if (g_objectTable[scanObjIdx].objectType != OBJ_None) {
				WarheadGuidanceState* guidance;
				unsigned int maxRangeScore;

				guidance = g_objectTable[scanObjIdx].mobj->pWarheadGuidance;
				if (guidance->homingTier != 0 && guidance->targetObjIdx == g_paiContext.aiObjIdx) {
					if (g_objectTable[scanObjIdx].objectType == OBJ_WarheadMissile) {
						maxRangeScore = 3u * warheadRange;
					} else {
						maxRangeScore = warheadRange;
					}
					if (pai_IsObjectWithinCurrentPointRange(scanObjIdx, maxRangeScore) == 1) {
						g_curCraft->lastAttackerObjIdx = scanObjIdx;
						pai_ObjectRefDirectionToObjectRef(g_paiContext.aiObjIdx, scanObjIdx);
						bearingClass = g_aiThreatBearingClassByOctant
							[(uint16_t)(trig2_xyangle - g_objectTable[g_paiContext.aiObjIdx].yaw) >> 13];
						if (bearingClass == 0) {
							g_paiContext.aiController->maneuverMode = 24;
						} else {
							g_paiContext.aiController->maneuverMode = 1;
						}
						paiman_initmaneuver();

						if (g_curCraft->cmTypeId == 0 || g_curCraft->cmAmmoCount == 0) {
							return 0;
						}
						if (g_curCraft->cmTypeId == 1 && (g_curCraft->workingSubsystems & 2u) != 0) {
							g_curCraft->chaffActiveTimer = (uint16_t)(g_curCraft->chaffActiveTimer + 10u);
							if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.status1 != 21 &&
								g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg.status2 != 21) {
								--g_curCraft->cmAmmoCount;
							}
							return 0;
						}
						if (g_curCraft->cmFireCooldownTimer == 0) {
							laser_createcountermeasureprojectile(g_paiContext.aiObjIdx, OBJ_WarheadFlare);
						}
						return 0;
					}
				}
			}
			++scanObjIdx;
		}
		{
			unsigned int attackerSearchRange;

			attackerSearchRange = g_aiAttackerSearchRangeBySkill[g_paiContext.aiSkillTier];
			if (g_curCraft->lastAttackerObjIdx == 0xffffu) {
				uint16_t objectIdx;

				for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
					 ++objectIdx) {
					if (g_objectTable[objectIdx].playerOwnerIdx == -1 &&
						g_objectTable[objectIdx].objectType != OBJ_None) {
						if (Object_IsHostileToTeam(objectIdx,
												   g_objectTable[g_paiContext.aiObjIdx].mobj->team)) {
							if (g_curCraft->objectKind == 0 &&
								g_objectTable[objectIdx].genusId == GENUS_Fighter) {
								if (pai_IsObjectWithinCurrentPointRange(objectIdx, attackerSearchRange) ==
									1) {
									uint16_t yawDelta;
									uint16_t pitchDelta;

									pai_ObjectRefDirectionToObjectRef(objectIdx, g_paiContext.aiObjIdx);
									yawDelta = (uint16_t)(trig2_xyangle - g_objectTable[objectIdx].yaw);
									if (yawDelta >= 0x8000u) {
										yawDelta = (uint16_t)(0u - yawDelta);
									}
									pitchDelta = (uint16_t)(targetPitch - g_objectTable[objectIdx].pitch);
									if (pitchDelta >= 0x8000u) {
										pitchDelta = (uint16_t)(0u - pitchDelta);
									}
									if (yawDelta < 0x2000u && pitchDelta < 0x2000u) {
										g_curCraft->lastAttackerObjIdx = objectIdx;
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (g_curCraft->lastAttackerObjIdx == 0xffffu) {
		return 0;
	}

	pai_ObjectRefDirectionToObjectRef(g_paiContext.aiObjIdx, g_curCraft->lastAttackerObjIdx);
	bearingClass =
		g_aiThreatBearingClassByOctant[(uint16_t)(trig2_xyangle - g_objectTable[g_paiContext.aiObjIdx].yaw) >>
									   13];
	{
		uint16_t selfMaxSpeed;
		uint16_t attackerMaxSpeed;
		unsigned int attackerObjIdx;
		MobileObject* attackerMobj;

		selfMaxSpeed = g_modelDefs[g_curCraft->modelIndex].maxSpeed;
		attackerObjIdx = (unsigned int)(int16_t)g_curCraft->lastAttackerObjIdx & 0xffffu;
		attackerMobj = g_objectTable[attackerObjIdx].mobj;
		if (attackerMobj->state == 0) {
			attackerMaxSpeed = g_modelDefs[attackerMobj->pCraft->modelIndex].maxSpeed;
		} else {
			attackerMaxSpeed = 900;
		}

		randValue = GameRand();
		if (bearingClass == 1) {
			if (trig2_polardistance < 0x2000 && randValue < 0x4000u) {
				maneuverMode = 1;
			} else {
				maneuverMode = g_aiUnderAttackFrontManeuverChoices[randValue & 3u];
			}
		} else if (bearingClass == 0) {
			if (randValue > 0x8000u) {
				maneuverMode = 9;
			} else {
				maneuverMode = g_aiUnderAttackFrontManeuverChoices[randValue & 3u];
			}
		} else {
			if (selfMaxSpeed < attackerMaxSpeed || trig2_polardistance <= 0x8000) {
				maneuverMode = g_aiUnderAttackSideRearManeuverChoices[randValue & 7u];
			} else {
				maneuverMode = 16;
			}
		}
	}

	g_paiContext.aiController->maneuverMode = maneuverMode;
	paiman_initmaneuver();
	return 0;
}

// FUNCTION: XWA 0x4BB000
char paiorder_avoidhitorder(void) {
	ModelGenusId selfGenus;
	unsigned int scanObjIdx;

	selfGenus = g_objectTable[g_paiContext.aiObjIdx].genusId;
	if (selfGenus == GENUS_Freighter || selfGenus == GENUS_Container || selfGenus == GENUS_Starship ||
		g_paiContext.aiController->maneuverMode != g_paiContext.aiPlanInitialManeuverId) {
		return 0;
	}

	if (g_curCraft->lastAttackerObjIdx == 0xffffu) {
		unsigned int warheadRange;

		warheadRange = g_aiWarheadThreatRangeBySkill[g_paiContext.aiSkillTier];
		for (scanObjIdx = g_projectileObjectSlotStart; scanObjIdx < g_projectileObjectSlotEnd; ++scanObjIdx) {
			if (g_objectTable[scanObjIdx].objectType != OBJ_None) {
				WarheadGuidanceState* guidance;

				guidance = g_objectTable[scanObjIdx].mobj->pWarheadGuidance;
				if (guidance->homingTier != 0 && guidance->targetObjIdx == (uint16_t)g_paiContext.aiObjIdx) {
					unsigned int maxRangeScore;

					if (g_objectTable[scanObjIdx].objectType == OBJ_WarheadMissile ||
						g_objectTable[scanObjIdx].objectType == OBJ_WarheadAdvancedMissile) {
						maxRangeScore = 3u * warheadRange;
					} else {
						maxRangeScore = warheadRange;
					}

					if (pai_IsObjectWithinCurrentPointRange(scanObjIdx, maxRangeScore) == 1) {
						g_curCraft->lastAttackerObjIdx = (uint16_t)scanObjIdx;
						goto handleIncomingWarhead;
					}
				}
			}
		}
	}

	if (g_curCraft->lastAttackerObjIdx == 0xffffu) {
		unsigned int attackerSearchRange;

		attackerSearchRange = g_aiAttackerSearchRangeBySkill[g_paiContext.aiSkillTier];
		for (scanObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
			 scanObjIdx < g_activeRegionCraftObjectSlotEnd; ++scanObjIdx) {
			if (g_objectTable[scanObjIdx].objectType != OBJ_None &&
				Object_IsHostileToTeam((uint16_t)scanObjIdx,
									   g_objectTable[g_paiContext.aiObjIdx].mobj->team) &&
				g_curCraft->objectKind == 0 && g_objectTable[scanObjIdx].genusId == GENUS_Fighter &&
				pai_IsObjectWithinCurrentPointRange(scanObjIdx, attackerSearchRange) == 1) {
				uint16_t yawDelta;
				uint16_t pitchDelta;

				pai_ObjectRefDirectionToObjectRef(scanObjIdx, g_paiContext.aiObjIdx);
				yawDelta = (uint16_t)(trig2_xyangle - g_objectTable[scanObjIdx].yaw);
				if (yawDelta >= 0x8000u) {
					yawDelta = (uint16_t)(0u - yawDelta);
				}
				pitchDelta = (uint16_t)(targetPitch - g_objectTable[scanObjIdx].pitch);
				if (pitchDelta >= 0x8000u) {
					pitchDelta = (uint16_t)(0u - pitchDelta);
				}
				if (yawDelta < 0x2000u && pitchDelta < 0x2000u) {
					g_curCraft->lastAttackerObjIdx = (uint16_t)scanObjIdx;
					break;
				}
			}
		}
	}

	if (g_curCraft->lastAttackerObjIdx != 0xffffu &&
		g_objectTable[g_paiContext.aiObjIdx].genusId != GENUS_Utility) {
		CraftData* craft;
		uint16_t speedScale;
		int16_t push;

		craft = g_curCraft;
		if (((craft->workingSubsystems & 1u) != 0 && craft->shieldFront < 500) ||
			(uint32_t)craft->hullDamage >= (uint32_t)craft->systemDamageHullThreshold) {
			pai_ObjectRefDirectionToObjectRef(craft->lastAttackerObjIdx, g_paiContext.aiObjIdx);
			if (trig2_polardistance < 0x8000 && g_curCraft->cmTypeId == 2 && g_curCraft->cmAmmoCount != 0 &&
				g_curCraft->cmFireCooldownTimer == 0) {
				laser_createcountermeasureprojectile(g_paiContext.aiObjIdx, OBJ_WarheadFlare);
			}
			if (g_objectTable[g_curCraft->lastAttackerObjIdx].playerOwnerIdx != -1) {
				g_paiContext.aiController->maneuverMode = 32;
				paiman_initmaneuver();
				paiman_setpower((int)g_paiContext.aiObjIdx, 0xffffu);
			}
			craft = g_curCraft;
		}

		speedScale = (uint16_t)MATH2_divide(g_objectTable[g_paiContext.aiObjIdx].mobj->speed,
											(uint16_t)craft->aiFlight.maxSpeedCache);

		push = (int16_t)MATH2_fraction((uint16_t)((GameRand() & 0xffu) + 256u), speedScale);
		if ((((uint16_t)GameRand() >> 8) & 0x80u) != 0) {
			push = (int16_t)-push;
		}
		g_curCraft->pushAccumX = push;

		push = (int16_t)MATH2_fraction((uint16_t)((GameRand() & 0xffu) + 256u), speedScale);
		if ((((uint16_t)GameRand() >> 8) & 0x80u) != 0) {
			push = (int16_t)-push;
		}
		g_curCraft->pushAccumY = push;

		push = (int16_t)MATH2_fraction((uint16_t)((GameRand() & 0xffu) + 256u), speedScale);
		if ((((uint16_t)GameRand() >> 8) & 0x80u) != 0) {
			push = (int16_t)-push;
		}
		g_curCraft->pushAccumZ = push;
	}

	return 0;

handleIncomingWarhead: {
	uint8_t cmTypeId;

	pai_ObjectRefDirectionToObjectRef(g_paiContext.aiObjIdx, (uint16_t)scanObjIdx);
	if (g_aiThreatBearingClassByOctant[(uint16_t)(trig2_xyangle - g_objectTable[g_paiContext.aiObjIdx].yaw) >>
									   13] == 0) {
		g_paiContext.aiController->maneuverMode = 1;
	} else {
		g_paiContext.aiController->maneuverMode = 32;
	}
	paiman_initmaneuver();

	if (g_curCraft->cmAmmoCount == 0) {
		return 0;
	}
	cmTypeId = g_curCraft->cmTypeId;
	if (cmTypeId == 1 && (g_curCraft->workingSubsystems & 2u) != 0) {
		if (g_curCraft->chaffActiveTimer == 0) {
			g_curCraft->chaffActiveTimer = 10;
			if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.status1 != 21 &&
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.status2 != 21) {
				--g_curCraft->cmAmmoCount;
			}
		}
		return 0;
	}
	if (cmTypeId != 2 || g_curCraft->cmFireCooldownTimer != 0) {
		return 0;
	}
	laser_createcountermeasureprojectile(g_paiContext.aiObjIdx, OBJ_WarheadFlare);
	return 0;
}
}

// FUNCTION: XWA 0x4BAE30
char paiorder_rocketsonboardorder(void) {
	unsigned int targetObjIdx;
	int desiredWarheadClass;
	char acceptAnyWarhead;
	CraftData* craft;
	uint16_t launcherIdx;

	targetObjIdx = g_paiContext.aiController->targetObjIdx;
	if ((uint16_t)targetObjIdx >= g_activeRegionObjectSlotStart &&
		targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
		ModelGenusId genusId;

		genusId = g_objectTable[targetObjIdx].genusId;
		if (genusId == GENUS_Freighter || genusId == GENUS_Platform || genusId == GENUS_Starship ||
			genusId == GENUS_Container || (genusId == GENUS_Transport && g_missionFormatVersion >= 14)) {
			desiredWarheadClass = 2;
		} else {
			desiredWarheadClass = 1;
		}
	} else {
		desiredWarheadClass = 1;
	}

	acceptAnyWarhead = 0;
	if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "capfreeldr1pln") == 0) {
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

	craft = g_curCraft;
	if (craft->warheadLauncherCount == 0) {
		return 0;
	}

	for (launcherIdx = 0; launcherIdx < craft->warheadLauncherCount; ++launcherIdx) {
		ObjectTypeId warheadType;

		warheadType = (ObjectTypeId)craft->warheadSlotTypeIds[launcherIdx];
		if (g_projectileWarheadClassByType[(uint16_t)warheadType - OBJ_LaserRebel] == desiredWarheadClass ||
			acceptAnyWarhead ||
			paifight_CanCountMagPulseAsRocket(0xffffu, warheadType, desiredWarheadClass, 1)) {
			const ModelDef* modelDef;
			uint16_t firstSlot;
			uint16_t lastSlot;
			uint16_t weaponSlot;

			modelDef = &g_modelDefs[craft->modelIndex];
			lastSlot = modelDef->warheadLauncherLastSlot[launcherIdx];
			firstSlot = modelDef->warheadLauncherFirstSlot[launcherIdx];
			weaponSlot = firstSlot;
			while (weaponSlot <= lastSlot) {
				if (craft->warheadData[weaponSlot].count != 0) {
					return 1;
				}
				++weaponSlot;
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BA970
char paiorder_lookforcrafttoboardorder(void) {
	uint16_t candidateTargetIdx;
	uint16_t targetObjIdx;

	candidateTargetIdx = g_paiContext.aiController->candidateTargetIdx;
	if (candidateTargetIdx != 0xffffu && candidateTargetIdx != 251u) {
		if (pai_IsObjectTargetable(candidateTargetIdx)) {
			g_paiContext.aiController->targetObjIdx = candidateTargetIdx;
			g_paiContext.aiController->targetSignature = g_objectTable[candidateTargetIdx].objectSignature;
			g_paiContext.aiController->hasLiveTarget = 1;
			return 1;
		}
		g_paiContext.aiController->candidateTargetIdx = 0xffffu;
	}

	targetObjIdx = (uint16_t)pai_FindBoardingTargetFromOrder(g_paiContext.curOrderCoord.fields.orderSlot,
															 g_paiContext.curOrderCoord.fields.regionIdx);
	if (targetObjIdx != 0xffffu) {
		g_paiContext.aiController->targetObjIdx = targetObjIdx;
		g_paiContext.aiController->targetSignature = g_objectTable[targetObjIdx].objectSignature;
		g_paiContext.aiController->hasLiveTarget = 1;
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4BAB60
char paiorder_awaitboardorder(void) {
	uint8_t engineIndex;
	uint16_t engineGlowCount;

	if (g_curCraft->boardingState != 2 && g_curCraft->boardingState != 3) {
		return 0;
	}

	++g_paiContext.aiController->orderScratch.goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
														  [g_paiContext.curOrderCoord.fields.orderSlot];

	if (g_curCraft->aiFlight.orderActionCounter >=
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.variable1) {
		g_curCraft->workingSubsystems = g_curCraft->systemFlags;
		g_curCraft->subsystemDamage = 0;

		if (g_curCraft->aiFlight.orderActionCounter ==
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg
					.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							g_paiContext.curOrderCoord.fields.orderSlot]
					.variable1 &&
			strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "disabledpln") == 0) {
			msg_emitCraftMessage((uint16_t)g_paiContext.aiObjIdx, g_curCraft, 146);
		}

		engineGlowCount = g_modelDefs[g_curCraft->modelIndex].engineGlowCount;
		for (engineIndex = 0; engineIndex < engineGlowCount; ++engineIndex) {
			g_curCraft->engineEmitterHealth[engineIndex] = g_modelDefs[g_curCraft->modelIndex].componentMaxHp;
		}

		if (g_useHardware3D) {
			GlowMark_ClearEngineKnockoutBlastMarks(g_paiContext.aiObjIdx);
		}

		g_curCraft->engineOutputScale = 0xffffu;
		return 0;
	}

	g_curCraft->boardingState = 0;
	return 0;
}

// FUNCTION: XWA 0x4BAE10
char paiorder_returnboardorder(void) {
	pai_CalcAnglesToAimPoint();
	if (trig2_polardistance < 0x4000) {
		return 1;
	}
	return 0;
}

// FUNCTION: XWA 0x4BAA40
char paiorder_abortboardorder(void) {
	int16_t abortOrder;
	char maneuverPhase;
	uint16_t targetObjIdx;

	abortOrder = 0;
	maneuverPhase = g_paiContext.aiController->maneuverPhase;

	if ((uint8_t)maneuverPhase < 3u) {
		ObjectRecord* targetObj;
		MobileObject* targetMobj;

		targetObjIdx = g_paiContext.aiController->targetObjIdx;
		targetMobj = g_objectTable[targetObjIdx].mobj;
		targetObj = &g_objectTable[targetObjIdx];
		if (targetMobj != NULL) {
			if (targetObj->objectType == OBJ_None) {
				abortOrder = 1;
			}
			if (targetMobj->state == 5) {
				abortOrder = 1;
			}
		} else if (targetObj->objectType == OBJ_None) {
			abortOrder = 1;
		}

		if (g_curCraft->workingSubsystems == 0) {
			abortOrder = 1;
		}
		if (g_paiContext.aiController->targetSignature != targetObj->objectSignature) {
			abortOrder = 1;
		}
	}

	if (maneuverPhase == 1 || maneuverPhase == 2) {
		int playerOwnerIdx;
		ObjectRecord* targetObj;

		playerOwnerIdx = g_objectTable[targetObjIdx].playerOwnerIdx;
		targetObj = &g_objectTable[targetObjIdx];
		if (playerOwnerIdx != -1) {
			if (targetObj->mobj->speed != 0) {
				abortOrder = 1;
			}
		}
	}

	if (abortOrder) {
		g_curCraft->pushAccumZ = 0;
		g_curCraft->pushAccumY = 0;
		g_curCraft->pushAccumX = 0;
		g_paiContext.aiController->targetObjIdx = 0x8000u;
		g_paiContext.aiController->targetSignature = 0;
		g_paiContext.aiController->hasLiveTarget = 0;
		pai_UpdateAimPointFromOrderTarget();
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4B9E40
char paiorder_abortmissionorder(void) {
	enum {
		OUTCOME_ABORTED = 21,
		OUTCOME_DEPARTED = 22,
	};

	uint8_t genusId;
	uint8_t abortTrigger;
	int shouldAbort;
	int abortReasonMsg;
	unsigned int teamIdx;

	if (g_curCraft->aiFlight.maxSpeedCache == 0) {
		return 0;
	}

	genusId = g_objectTable[g_paiContext.aiObjIdx].genusId;
	if (genusId == GENUS_WeaponEmplacement || genusId == GENUS_SatelliteBuoy) {
		return 0;
	}

	if (g_curCraft->workingSubsystems == 0) {
		return 0;
	}

	abortTrigger = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.abortTrigger;
	shouldAbort = 0;

	switch (abortTrigger) {
		case 1:
			if ((g_curCraft->workingSubsystems & 1u) == 0) {
				shouldAbort = 1;
			}
			if ((genusId == GENUS_Freighter || genusId == GENUS_Starship) &&
				(uint32_t)g_curCraft->shieldRear + (uint32_t)g_curCraft->shieldFront == 0) {
				shouldAbort = 1;
			}
			abortReasonMsg = MSG_ABORT_SHIELDS_0;
			break;

		case 6: {
			if ((uint32_t)g_curCraft->shieldRear + (uint32_t)g_curCraft->shieldFront <=
				MATH2_fraction((uint16_t)Craft_GetObjectMaxShield((unsigned short)g_paiContext.aiObjIdx),
							   0x8000u)) {
				shouldAbort = 1;
			}
			abortReasonMsg = MSG_ABORT_SHIELDS_50;
			break;
		}

		case 7: {
			if ((uint32_t)g_curCraft->shieldRear + (uint32_t)g_curCraft->shieldFront <=
				MATH2_fraction((uint16_t)Craft_GetObjectMaxShield((unsigned short)g_paiContext.aiObjIdx),
							   0x4000u)) {
				shouldAbort = 1;
			}
			abortReasonMsg = MSG_ABORT_SHIELDS_25;
			break;
		}

		case 2:
			if ((g_curCraft->workingSubsystems & 0x10u) == 0) {
				shouldAbort = 1;
			}
			abortReasonMsg = MSG_ABORT_WARHEADS_OUT;
			break;

		case 3: {
			int launcherIdx;
			int remainingLaunchers;
			uint16_t* warheadSlotTypeId;
			uint16_t hasWarheads;

			if ((g_curCraft->workingSubsystems & 8u) == 0) {
				shouldAbort = 1;
			}

			hasWarheads = 0;
			if (g_curCraft->warheadLauncherCount > 0u) {
				launcherIdx = 0;
				remainingLaunchers = g_curCraft->warheadLauncherCount;
				warheadSlotTypeId = g_curCraft->warheadSlotTypeIds;
				do {
					if (*warheadSlotTypeId != 0) {
						unsigned int firstSlot;
						unsigned int lastSlot;

						firstSlot = g_modelDefs[g_curCraft->modelIndex].warheadLauncherFirstSlot[launcherIdx];
						lastSlot = g_modelDefs[g_curCraft->modelIndex].warheadLauncherLastSlot[launcherIdx];
						if (firstSlot <= lastSlot) {
							int slotCount;
							WarheadInventoryEntry* warhead;

							slotCount = (int)(lastSlot - firstSlot + 1);
							warhead = &g_curCraft->warheadData[firstSlot];
							do {
								if (warhead->count != 0) {
									hasWarheads = 1;
								}
								++warhead;
								--slotCount;
							} while (slotCount != 0);
						}
					}
					++launcherIdx;
					++warheadSlotTypeId;
					--remainingLaunchers;
				} while (remainingLaunchers != 0);
			}
			if (!hasWarheads) {
				shouldAbort = 1;
			}
			abortReasonMsg = MSG_ABORT_WARHEADS_OUT;
			break;
		}

		case 8:
			if ((uint32_t)g_curCraft->hullDamage >=
				MATH2_longfraction((uint32_t)g_curCraft->hullMax, 0x4000u)) {
				shouldAbort = 1;
			}
			abortReasonMsg = MSG_ABORT_HULL_75;
			break;

		case 4:
			if ((uint32_t)g_curCraft->hullDamage >=
				MATH2_longfraction((uint32_t)g_curCraft->hullMax, 0x8000u)) {
				shouldAbort = 1;
			}
			abortReasonMsg = MSG_ABORT_HULL_50;
			break;

		case 9:
			if ((uint32_t)g_curCraft->hullDamage >=
				MATH2_longfraction((uint32_t)g_curCraft->hullMax, 0xc000u)) {
				shouldAbort = 1;
			}
			abortReasonMsg = MSG_ABORT_HULL_25;
			break;

		case 5:
			teamIdx = 0;
			do {
				if (g_curCraft->attackedByTeam[teamIdx] != 0) {
					shouldAbort = 1;
				}
				++teamIdx;
			} while (teamIdx < 10);
			abortReasonMsg = MSG_ABORT_UNDER_ATTACK;
			break;

		default:
			break;
	}

	if (!shouldAbort) {
		return 0;
	}

	if (!g_curCraft->aiFlight.missionAbortedFlag) {
		++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx].outcomeCount[OUTCOME_ABORTED];
		if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.specialCargoCraft ==
			g_curCraft->waveNumber) {
			g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.specialCargoOutcome[OUTCOME_ABORTED] = 1;
		}

		if (g_curCraft->aiFlight.departTimerFlag) {
			--g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				  .outcomeCount[OUTCOME_DEPARTED];
			if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.specialCargoCraft == g_curCraft->waveNumber) {
				g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.specialCargoOutcome[OUTCOME_DEPARTED] = 0;
			}
			g_curCraft->aiFlight.departTimerFlag = 0;
		}

		fsfx_SpeakTacticalOfficerEvent(4, 77, g_paiContext.aiObjIdx, 0xffffu);

		if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].playerOwnerIdx != -1) {
			g_msgSenderIff = (uint8_t)g_objectTable[g_paiContext.aiObjIdx].mobj->iff;
			msg_addMessagePtr(
				0, (const char*)&g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg);
			g_msgArgTable[1] = (uint16_t)Hud_MissionFG_GetCraftNumberIfShown(
				g_paiContext.curOrderCoord.fields.flightGroupIdx, g_curCraft);
			g_msgArgTable[2] = abortReasonMsg;
			msg_emitInFlightMessage(
				MSG_WINGMAN_ABORT,
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].playerOwnerIdx);
		}

		{
			const XwaOrder* order;
			int orderIndex;
			uint8_t planNameIndex;
			uint8_t planId;

			orderIndex =
				g_paiContext.curOrderCoord.fields.orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx;
			order = &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						 .fg.orders[orderIndex];
			planNameIndex = g_orderLeaderBuiltinPlanNameIndex[order->order];
			planId = g_builtinPlanIdByNameIndex[planNameIndex];
			if (strcmp(g_planTable[planId].name, "waitforboardpln") == 0 &&
				g_curCraft->subsystemDamage == 0) {
				g_curCraft->workingSubsystems = g_curCraft->systemFlags;
			}
		}
	}

	g_curCraft->aiFlight.missionAbortedFlag = 1;
	return shouldAbort;
}

// FUNCTION: XWA 0x4BAD50
char paiorder_makedisabledorder(void) {
	uint16_t flightGroupIdx;

	if (g_curCraft->workingSubsystems != 0) {
		g_curCraft->workingSubsystems = 0;

		flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
		++g_missionFgStats[flightGroupIdx].outcomeCount[14];
		if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == g_curCraft->waveNumber) {
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[14] = 1;
		}

		if (g_missionFlightGroups[flightGroupIdx]
				.fg
				.orders[g_paiContext.curOrderCoord.fields.orderSlot +
						4 * g_paiContext.curOrderCoord.fields.regionIdx]
				.order == 44) {
			g_curCraft->shieldFront = 0;
			return 0;
		}

		msg_emitCraftMessage((uint16_t)g_paiContext.aiObjIdx, g_curCraft, 145);
	}

	return 0;
}

// FUNCTION: XWA 0x4BCCA0
char paiorder_selfcaptureorder(void) {
	uint16_t flightGroupIdx;
	uint8_t newIff;

	flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
	newIff = g_missionFlightGroups[flightGroupIdx]
				 .fg
				 .orders[g_paiContext.curOrderCoord.fields.orderSlot +
						 4 * g_paiContext.curOrderCoord.fields.regionIdx]
				 .variable1;

	if ((uint8_t)g_objectTable[g_paiContext.aiObjIdx].mobj->iff != newIff) {
		++g_missionFgStats[flightGroupIdx].outcomeCount[6];
		if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == g_curCraft->waveNumber) {
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[6] = 1;
		}

		g_objectTable[g_paiContext.aiObjIdx].mobj->iff = (int8_t)newIff;
		g_curCraft->workingSubsystems = g_curCraft->systemFlags;
		g_curCraft->subsystemDamage = 0;
		g_curCraft->objectKind = 0;
		g_curCraft->lastAttackerObjIdx = 0xffffu;
		g_curCraft->wasCaptured = 1;
		msg_emitCraftMessage((uint16_t)g_paiContext.aiObjIdx, g_curCraft, 147);
	}

	if (g_curCraft->aiFlight.maxSpeedCache == 0) {
		return 0;
	}
	return 1;
}

// FUNCTION: XWA 0x4BCC40
char paiorder_transfercargoorder(void) {
	if (g_curCraft->cargoIndex != 0xffu) {
		uint16_t targetObjIdx;

		targetObjIdx = (uint16_t)paifight_FindAttackOrderTargetFromOrder(
			g_paiContext.curOrderCoord.fields.orderSlot, g_paiContext.curOrderCoord.fields.regionIdx);
		if (targetObjIdx != 0xffffu) {
			MobileObject* mobj;

			mobj = g_objectTable[targetObjIdx].mobj;
			if (mobj != NULL) {
				mobj->pCraft->cargoIndex = g_curCraft->cargoIndex;
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BE2B0
char paiorder_lookforparkorder(void) {
	unsigned int targetObjIdx;
	unsigned int targetModelIndex;
	unsigned int dockPointCount;
	int selectedDockPoint;
	uint32_t bestRange;
	unsigned int dockPointIdx;

	g_paiContext.aiRequireLiveOrderTarget = 0;
	g_paiContext.aiTargetSearchFlags = 7;

	targetObjIdx = (uint16_t)paifight_FindNearestAttackOrderTarget(
		(MissionTriggerVariableType)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.target1Type,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.target1,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.target1OrTarget2,
		(MissionTriggerVariableType)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.target2Type,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.target2);
	if (targetObjIdx != 0xffffu) {
		Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
		targetModelIndex =
			(uint16_t)g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].modelIndex;
		bestRange = UINT32_MAX;
		dockPointCount = (uint16_t)g_modelDefs[targetModelIndex].dockPointCount;
		selectedDockPoint = 0xffff;

		for (dockPointIdx = 0; dockPointIdx < dockPointCount; ++dockPointIdx) {
			int dockWorldX;
			int dockWorldY;
			int dockWorldZ;
			int occupied;
			uint32_t scanObjIdx;

			dockWorldX = worldlocx + g_modelDefs[targetModelIndex].dockPoints[dockPointIdx].x;
			dockWorldY = worldlocy + g_modelDefs[targetModelIndex].dockPoints[dockPointIdx].y;
			dockWorldZ = worldlocz + g_modelDefs[targetModelIndex].dockPoints[dockPointIdx].z;
			g_targetRangeScore =
				collide_roughdistance3d(dockWorldX - g_objectTable[g_paiContext.aiObjIdx].world_x,
										dockWorldY - g_objectTable[g_paiContext.aiObjIdx].world_y,
										dockWorldZ - g_objectTable[g_paiContext.aiObjIdx].world_z);

			if ((uint32_t)g_targetRangeScore >= bestRange) {
				continue;
			}

			occupied = 0;
			for (scanObjIdx = g_activeRegionObjectSlotStart; scanObjIdx < g_activeRegionCraftObjectSlotEnd;
				 ++scanObjIdx) {
				AiController* effectiveController;

				if (g_objectTable[scanObjIdx].objectType == OBJ_None || scanObjIdx == g_paiContext.aiObjIdx) {
					continue;
				}

				effectiveController = pai_GetEffectiveAIController(g_objectTable[scanObjIdx].mobj->pCraft);
				if (dockWorldX == effectiveController->aimPointX &&
					dockWorldY == effectiveController->aimPointY &&
					dockWorldZ == effectiveController->aimPointZ) {
					occupied = 1;
					break;
				}
			}

			if (!occupied) {
				selectedDockPoint = dockPointIdx;
				bestRange = (uint32_t)g_targetRangeScore;
			}
		}

		if (selectedDockPoint != 0xffff) {
			g_paiContext.aiController->targetObjIdx = targetObjIdx;
			g_paiContext.aiController->aimPointX =
				worldlocx + g_modelDefs[targetModelIndex].dockPoints[selectedDockPoint].x;
			g_paiContext.aiController->aimPointY =
				worldlocy + g_modelDefs[targetModelIndex].dockPoints[selectedDockPoint].y;
			g_paiContext.aiController->aimPointZ =
				worldlocz + g_modelDefs[targetModelIndex].dockPoints[selectedDockPoint].z;
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BB740
char paiorder_targetfromplayerorder(void) {
	if (g_paiContext.aiController->candidateTargetIdx != 0xffffu &&
		g_paiContext.aiController->candidateTargetIdx != 251u) {
		if (pai_IsObjectTargetable(g_paiContext.aiController->candidateTargetIdx)) {
			if (g_paiContext.aiController->candidateTargetIdx != g_paiContext.aiController->targetObjIdx) {
				g_paiContext.aiController->targetObjIdx = g_paiContext.aiController->candidateTargetIdx;
				g_paiContext.aiController->targetSignature =
					g_objectTable[g_paiContext.aiController->targetObjIdx].objectSignature;
				g_paiContext.aiController->hasLiveTarget = 1;
			}
		} else {
			g_paiContext.aiController->candidateTargetIdx = 0xffffu;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BD340
char paiorder_disappearorder(void) {
	Mission_RecordCraftOutcome((uint16_t)g_paiContext.aiObjIdx,
							   g_paiContext.curOrderCoord.fields.flightGroupIdx, 17u);
	g_objectTable[g_paiContext.aiObjIdx].objectType = OBJ_None;
	return 0;
}

// FUNCTION: XWA 0x4BD380
char paiorder_commandfromplayerorder(void) {
	CraftData* aiSelfCraft;
	uint16_t followTimer;

	aiSelfCraft = g_paiContext.aiSelfCraft;
	if (g_paiContext.aiSelfCraft->followPlayerMode != 2) {
		return 0;
	}

	followTimer = g_paiContext.aiSelfCraft->followTimer;
	if (followTimer >= 60u) {
		g_paiContext.aiSelfCraft->followPlayerMode = 1;
		g_paiContext.aiController->targetObjIdx = 0xffffu;
		g_paiContext.aiController->targetSignature = 0;
		g_paiContext.aiController->hasLiveTarget = 0;
		g_paiContext.aiController->currentPlanId = (uint8_t)pai_findplanbyname("deathstarfollowpln");
		g_paiContext.nullPlanId = g_paiContext.aiController->currentPlanId;
		return 1;
	}

	if (followTimer >= 50u) {
		if (g_paiContext.aiController->candidateTargetIdx != 0xffffu &&
			pai_IsObjectTargetable(g_paiContext.aiController->candidateTargetIdx)) {
			switch ((uint8_t)(g_paiContext.aiSelfCraft->followTimer - 50u)) {
				case 1:
					g_paiContext.aiController->currentPlanId =
						(uint8_t)pai_findplanbyname("playerboardtorepairpln");
					break;
				case 2:
					g_paiContext.aiController->currentPlanId =
						(uint8_t)pai_findplanbyname("playerboardtocapturepln");
					break;
				case 3:
					g_paiContext.aiController->currentPlanId =
						(uint8_t)pai_findplanbyname("playerboardtopickuppln");
					break;
				case 4:
					g_paiContext.aiController->currentPlanId =
						(uint8_t)pai_findplanbyname("playerboardtodestroypln");
					break;
				default:
					break;
			}

			g_paiContext.aiController->targetObjIdx = g_paiContext.aiController->candidateTargetIdx;
			g_paiContext.aiController->targetSignature =
				g_objectTable[g_paiContext.aiController->candidateTargetIdx].objectSignature;
			g_paiContext.aiController->hasLiveTarget = 0;
			g_paiContext.nullPlanId = (uint8_t)pai_findplanbyname("playerboard2pln");
			g_paiContext.aiSelfCraft->followPlayerMode = 1;
			return 1;
		}

		return 0;
	}

	if (followTimer >= 40u) {
		return 0;
	}

	if (followTimer >= 30u) {
		uint8_t planId;

		planId = (uint8_t)pai_findplanbyname("playerfollowpln");
		if (g_paiContext.aiController->currentPlanId != planId) {
			g_paiContext.aiSelfCraft->followPlayerMode = 1;
			g_paiContext.aiController->currentPlanId = planId;
			g_paiContext.nullPlanId = planId;
			return 1;
		}

		return 0;
	}

	if (followTimer >= 20u || followTimer < 10u) {
		return 0;
	}

	switch ((uint8_t)(aiSelfCraft->followTimer - 10u)) {
		case 0: {
			if (g_paiContext.aiController->candidateTargetIdx == 0xffffu ||
				!pai_IsObjectTargetable(g_paiContext.aiController->candidateTargetIdx)) {
				g_paiContext.aiSelfCraft->followPlayerMode = 0;
				return 0;
			}

			g_paiContext.aiSelfCraft->followPlayerMode = 1;
			g_paiContext.aiController->targetObjIdx = g_paiContext.aiController->candidateTargetIdx;
			g_paiContext.aiController->targetSignature =
				g_objectTable[g_paiContext.aiController->candidateTargetIdx].objectSignature;
			g_paiContext.aiController->hasLiveTarget = 0;
			g_paiContext.aiController->currentPlanId = (uint8_t)pai_findplanbyname("playercapldr2pln");
			g_paiContext.nullPlanId = g_paiContext.aiController->currentPlanId;
			return 1;
		}

		case 1: {
			if (g_paiContext.aiController->candidateTargetIdx == 0xffffu ||
				!pai_IsObjectTargetable(g_paiContext.aiController->candidateTargetIdx)) {
				g_paiContext.aiSelfCraft->followPlayerMode = 0;
				return 0;
			}

			g_paiContext.aiSelfCraft->followPlayerMode = 1;
			g_paiContext.aiController->targetObjIdx = g_paiContext.aiController->candidateTargetIdx;
			g_paiContext.aiController->targetSignature =
				g_objectTable[g_paiContext.aiController->candidateTargetIdx].objectSignature;
			g_paiContext.aiController->hasLiveTarget = 0;
			g_paiContext.aiController->currentPlanId = (uint8_t)pai_findplanbyname("playercapldr2pln");
			g_paiContext.nullPlanId = g_paiContext.aiController->currentPlanId;
			return 1;
		}

		case 2:
			g_paiContext.aiSelfCraft->followPlayerMode = 1;
			g_paiContext.aiController->targetObjIdx = 0xffffu;
			g_paiContext.aiController->targetSignature = 0;
			g_paiContext.aiController->hasLiveTarget = 0;
			g_paiContext.aiController->currentPlanId = (uint8_t)pai_findplanbyname("playercapldr1pln");
			g_paiContext.nullPlanId = g_paiContext.aiController->currentPlanId;
			return 1;

		case 3: {
			if (g_paiContext.aiController->candidateTargetIdx == 0xffffu ||
				!pai_IsObjectTargetable(g_paiContext.aiController->candidateTargetIdx)) {
				g_paiContext.aiSelfCraft->followPlayerMode = 0;
				return 0;
			}

			g_paiContext.aiSelfCraft->followPlayerMode = 1;
			g_paiContext.aiController->targetObjIdx = g_paiContext.aiController->candidateTargetIdx;
			g_paiContext.aiController->targetSignature =
				g_objectTable[g_paiContext.aiController->candidateTargetIdx].objectSignature;
			g_paiContext.aiController->hasLiveTarget = 1;
			g_paiContext.aiController->currentPlanId = (uint8_t)pai_findplanbyname("playerdisableldr2pln");
			g_paiContext.nullPlanId = g_paiContext.aiController->currentPlanId;
			return 1;
		}

		case 4:
			g_paiContext.aiSelfCraft->followPlayerMode = 1;
			g_paiContext.aiController->targetObjIdx = 0xffffu;
			g_paiContext.aiController->targetSignature = 0;
			g_paiContext.aiController->currentPlanId = (uint8_t)pai_findplanbyname("playerinspectldr1pln");
			g_paiContext.nullPlanId = g_paiContext.aiController->currentPlanId;
			return 1;

		case 5: {
			const char* pendingPlanName;

			pendingPlanName = g_planTable[g_paiContext.aiController->pendingPlanId].name;
			if (strcmp(pendingPlanName, "craftwaitforgopln") == 0 ||
				strcmp(pendingPlanName, "intohyperspacepln") == 0 ||
				strcmp(pendingPlanName, "outofhyperspacepln") == 0) {
				return 0;
			}

			g_paiContext.aiSelfCraft->followPlayerMode = 1;
			g_paiContext.aiController->currentPlanId = (uint8_t)pai_findplanbyname("craftwaitforgopln");
			g_paiContext.nullPlanId = g_paiContext.aiController->currentPlanId;
			return 1;
		}

		case 6:
			g_paiContext.aiSelfCraft->followPlayerMode = 0;
			g_paiContext.aiController->targetObjIdx = 0xffffu;
			g_paiContext.aiController->targetSignature = 0;
			g_paiContext.aiController->currentPlanId = (uint8_t)pai_findplanbyname("resumemissionpln");
			g_paiContext.nullPlanId = g_paiContext.aiController->currentPlanId;
			return 1;

		case 7: {
			const char* pendingPlanName;

			pendingPlanName = g_planTable[g_paiContext.aiController->pendingPlanId].name;
			if (strcmp(pendingPlanName, "flyhomeevadepln") == 0 ||
				strcmp(pendingPlanName, "starshipintohyperpln") == 0) {
				return 0;
			}

			if (g_paiContext.aiSelfCraft->aiFlight.missionAbortedFlag == 0) {
				uint8_t* flightGroupIdx;

				flightGroupIdx = &g_objectTable[g_paiContext.aiObjIdx].flightGroupIdx;
				++g_missionFgStats[*flightGroupIdx].outcomeCount[21];
				if (g_missionFlightGroups[*flightGroupIdx].fg.specialCargoCraft == aiSelfCraft->waveNumber) {
					g_missionFgStats[*flightGroupIdx].specialCargoOutcome[21] = 1;
				}
			}

			aiSelfCraft->aiFlight.missionAbortedFlag = 1;
			g_paiContext.aiSelfCraft->followPlayerMode = 0;
			g_paiContext.aiController->targetObjIdx = 0xffffu;
			g_paiContext.aiController->targetSignature = 0;
			if (g_objectTable[g_paiContext.aiObjIdx].genusId == GENUS_Starship) {
				g_paiContext.aiController->currentPlanId =
					(uint8_t)pai_findplanbyname("starshipintohyperpln");
			} else {
				g_paiContext.aiController->currentPlanId = (uint8_t)pai_findplanbyname("flyhomeevadepln");
			}
			g_paiContext.nullPlanId = g_paiContext.aiController->currentPlanId;
			return 1;
		}

		default:
			return 0;
	}
}

// FUNCTION: XWA 0x4BCA30
char paiorder_abortmotherwaitorder(void) {
	char result;

	if (g_paiContext.aiController->targetObjIdx >= g_activeRegionObjectSlotStart &&
		g_paiContext.aiController->targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
		return 0;
	}

	if (!g_modelDefs[g_curCraft->modelIndex].hasHyperdrive) {
		return 0;
	}

	result = 0;
	if (g_curCraft->wasCaptured) {
		if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.capturedDepartViaMothership != 0) {
			uint16_t mothershipFgIdx;

			mothershipFgIdx = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								  .fg.capturedDepartureMothership;
			if (g_missionFgStats[mothershipFgIdx].outcomeCount[0] ==
				g_missionFgStats[mothershipFgIdx].outcomeCount[1]) {
				result = 1;
			}
		}
	} else {
		char departureMothershipReady;
		char alternateMothershipReady;

		departureMothershipReady = 1;
		alternateMothershipReady = 1;
		if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.departMethod == 1) {
			uint16_t mothershipFgIdx;

			mothershipFgIdx = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								  .fg.departureMothership;
			if (g_missionFgStats[mothershipFgIdx].outcomeCount[0] !=
				g_missionFgStats[mothershipFgIdx].outcomeCount[1]) {
				departureMothershipReady = 0;
			}
		}
		if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg.alternateMothershipUsed == 1) {
			uint16_t mothershipFgIdx;

			mothershipFgIdx = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								  .fg.alternateMothership;
			if (g_missionFgStats[mothershipFgIdx].outcomeCount[0] !=
				g_missionFgStats[mothershipFgIdx].outcomeCount[1]) {
				alternateMothershipReady = 0;
			}
		}

		result = (char)(departureMothershipReady & alternateMothershipReady);
	}

	return result;
}

// FUNCTION: XWA 0x4BB940
char paiorder_stopgohomeorder(void) {
	int departNow;

	if ((g_curCraft->aiFlight.maxSpeedCache != 0 &&
		 g_objectTable[g_paiContext.aiObjIdx].genusId != GENUS_WeaponEmplacement &&
		 g_objectTable[g_paiContext.aiObjIdx].genusId != GENUS_SatelliteBuoy &&
		 g_curCraft->workingSubsystems != 0) ||
		strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "hyperbuoypln") == 0) {
		departNow = 0;
		if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.departureClockMin +
					g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg.departureClockSec !=
				0 &&
			(g_missionElapsedClock.minutes >
				 g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					 .fg.departureClockMin ||
			 (g_missionElapsedClock.minutes ==
				  g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.departureClockMin &&
			  g_missionElapsedClock.seconds >=
				  g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.departureClockSec))) {
			departNow = 1;
		}

		if (!g_curCraft->aiFlight.departTimerFlag) {
			if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg.departure.triggers[0]
						.condition != 0 ||
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg.departure.triggers[1]
						.condition != 0) {
				if ((Mission_EvaluateTriggerPair(
						 &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							  .fg.departure,
						 0) &
					 1) != 0) {
					departNow = 1;
				}
			}

			if (departNow == 1) {
				g_curCraft->aiFlight.departClockHours = g_missionElapsedClock.hours;
				g_curCraft->aiFlight.departClockMin = g_missionElapsedClock.minutes;
				g_curCraft->aiFlight.departClockSec = g_missionElapsedClock.seconds;

				if (!g_curCraft->aiFlight.departTimerFlag) {
					++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx].outcomeCount[22];
					if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg.specialCargoCraft == g_curCraft->waveNumber) {
						g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.specialCargoOutcome[22] = 1;
					}
				}
				g_curCraft->aiFlight.departTimerFlag = 1;
			}
		}

		if (g_curCraft->aiFlight.departTimerFlag == 1) {
			CraftData* departureCraft;
			uint32_t departureDelaySeconds;
			int elapsedFinalSeconds;
			uint32_t elapsedSeconds;

			departureCraft = g_curCraft;
			departureDelaySeconds =
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.departureDelaySeconds +
				60u * g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						  .fg.departureDelayMinutes;
			elapsedSeconds = 60u * (g_missionElapsedClock.minutes + 60u * g_missionElapsedClock.hours);
			elapsedSeconds -= 3600u * departureCraft->aiFlight.departClockHours;
			elapsedSeconds -= departureCraft->aiFlight.departClockSec;
			departureCraft = g_curCraft;
			elapsedFinalSeconds = (int)elapsedSeconds - 60 * departureCraft->aiFlight.departClockMin;
			elapsedFinalSeconds += g_missionElapsedClock.seconds;
			if (departureDelaySeconds == 0 || (uint32_t)elapsedFinalSeconds >= departureDelaySeconds) {
				if (g_objectTable[g_paiContext.aiObjIdx].genusId != GENUS_SatelliteBuoy) {
					fsfx_SpeakTacticalOfficerEvent(4, 77, g_paiContext.aiObjIdx, 0xffffu);
					g_msgSenderIff = (uint8_t)g_objectTable[g_paiContext.aiObjIdx].mobj->iff;
					msg_addMessagePtr(0, g_modelDefs[g_curCraft->modelIndex].nameLong);
					msg_addMessagePtr(
						1,
						(const char*)&g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg);
					if (!Hud_MissionFG_GetCraftNumberIfShown(g_paiContext.curOrderCoord.fields.flightGroupIdx,
															 g_curCraft)) {
						msg_emitInFlightMessage(MSG_SINGLE_WITHDRAW, g_localPlayer);
					} else {
						msg_emitInFlightMessage(MSG_MANY_WITHDRAW, g_localPlayer);
					}
				}

				if (strcmp(
						g_planTable
							[g_builtinPlanIdByNameIndex
								 [g_orderLeaderBuiltinPlanNameIndex
									  [g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										   .fg
										   .orders[g_paiContext.curOrderCoord.fields.orderSlot +
												   4 * g_paiContext.curOrderCoord.fields.regionIdx]
										   .order]]]
								.name,
						"waitforboardpln") == 0 &&
					!g_curCraft->subsystemDamage) {
					g_curCraft->workingSubsystems = g_curCraft->systemFlags;
				}

				return 1;
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BC030
char paiorder_checkconditionalorder(void) {
	uint8_t orderSlot;
	uint8_t stopOrderSlot;
	uint8_t completionState;

	completionState =
		g_paiContext.aiController->orderScratch.completionState[g_paiContext.curOrderCoord.fields.regionIdx]
															   [g_paiContext.curOrderCoord.fields.orderSlot];
	if (completionState == 0 || completionState == 3) {
		stopOrderSlot = 4;
	} else if (g_paiContext.curOrderCoord.fields.orderSlot == 1) {
		return 0;
	} else {
		stopOrderSlot = g_paiContext.curOrderCoord.fields.orderSlot;
	}

	orderSlot = 1;
	if (stopOrderSlot <= 1u) {
		return 0;
	}

	while (orderSlot < stopOrderSlot) {
		if (g_paiContext.aiController->orderScratch
				.completionState[g_paiContext.curOrderCoord.fields.regionIdx][orderSlot] == 1) {
			int triggerCondition;

			triggerCondition =
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.skipTriggers[orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx]
					.triggers[0]
					.condition;
			if (triggerCondition == 0) {
				triggerCondition =
					g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg.skipTriggers[orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx]
						.triggers[1]
						.condition;
			}
			if (triggerCondition != 0) {
				if ((Mission_EvaluateTriggerPair(
						 &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							  .fg.skipTriggers[orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx],
						 0) &
					 1) != 0) {
					paiorder_selectorderslot(orderSlot);
					return 1;
				}
			}
		}

		++orderSlot;
	}

	return 0;
}

// FUNCTION: XWA 0x4BBD50
char paiorder_completegohomeorder(void) {
	uint8_t completionState;

	completionState =
		g_paiContext.aiController->orderScratch.completionState[g_paiContext.curOrderCoord.fields.regionIdx]
															   [g_paiContext.curOrderCoord.fields.orderSlot];
	if (completionState == 0 || completionState == 1) {
		if (pai_IsPlanCompleteForOrderSlot(g_paiContext.aiController->currentPlanId,
										   g_paiContext.curOrderCoord.fields.orderSlot,
										   g_paiContext.curOrderCoord.fields.regionIdx)) {
			if (completionState == 0) {
				g_paiContext.aiController->orderScratch
					.completionState[g_paiContext.curOrderCoord.fields.regionIdx]
									[g_paiContext.curOrderCoord.fields.orderSlot] = 3;
			} else {
				g_paiContext.aiController->orderScratch
					.completionState[g_paiContext.curOrderCoord.fields.regionIdx]
									[g_paiContext.curOrderCoord.fields.orderSlot] = 4;
			}
		} else if (pai_IsBoardingPlanCompleteForOrderSlot(g_paiContext.aiController->currentPlanId,
														  g_paiContext.curOrderCoord.fields.orderSlot,
														  g_paiContext.curOrderCoord.fields.regionIdx)) {
			if (completionState == 0) {
				g_paiContext.aiController->orderScratch
					.completionState[g_paiContext.curOrderCoord.fields.regionIdx]
									[g_paiContext.curOrderCoord.fields.orderSlot] = 5;
			} else {
				g_paiContext.aiController->orderScratch
					.completionState[g_paiContext.curOrderCoord.fields.regionIdx]
									[g_paiContext.curOrderCoord.fields.orderSlot] = 6;
			}
		}
	}

	completionState =
		g_paiContext.aiController->orderScratch.completionState[g_paiContext.curOrderCoord.fields.regionIdx]
															   [g_paiContext.curOrderCoord.fields.orderSlot];
	if (completionState == 0 || completionState == 1 || completionState == 2) {
		return 0;
	}

	{
		int completedGoHomeCount;
		int completedBoardingCount;
		uint8_t orderSlot;

		completedGoHomeCount = 0;
		completedBoardingCount = 0;
		for (orderSlot = 0; orderSlot < 4u; ++orderSlot) {
			completionState =
				g_paiContext.aiController->orderScratch
					.completionState[0][orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx];
			if (completionState == 0) {
				return 0;
			}
			if (completionState == 1) {
				if ((g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							 .fg.skipTriggers[orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx]
							 .triggers[0]
							 .condition != 0 ||
					 g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							 .fg.skipTriggers[orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx]
							 .triggers[1]
							 .condition != 0)) {
					const XwaTriggerPair* skipTriggers;

					skipTriggers =
						&g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							 .fg.skipTriggers[orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx];
					if ((Mission_EvaluateTriggerPair(skipTriggers, 0) & 1) != 0) {
						return 0;
					}
				}
			}
			if (completionState == 3) {
				++completedGoHomeCount;
			}
			if (completionState == 5) {
				++completedBoardingCount;
			}
		}

		if (completedGoHomeCount != 0 && completedBoardingCount == 0 && !g_curCraft->aiFlight.goHomeFlag) {
			g_curCraft->aiFlight.goHomeFlag = 1;
		}
	}

	if (g_curCraft->aiFlight.maxSpeedCache != 0) {
		ModelGenusId genusId;

		genusId = g_objectTable[g_paiContext.aiObjIdx].genusId;
		if (genusId != GENUS_WeaponEmplacement && genusId != GENUS_SatelliteBuoy) {
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BC120
char paiorder_waitgootherorder(void) {
	PaiOrderCoord orderCoord;
	volatile int regionOrderBase;
	uint8_t orderSlot;

	orderSlot = g_paiContext.curOrderCoord.fields.orderSlot;
	regionOrderBase = 4 * g_paiContext.curOrderCoord.fields.regionIdx;
	if (g_paiContext.aiController->orderScratch.completionState[0][regionOrderBase + orderSlot] == 1) {
		return 0;
	}

	++orderSlot;
	if (orderSlot >= 4u) {
		return 0;
	}

	orderCoord = g_paiContext.curOrderCoord;
	while (orderSlot < 4u) {
		int orderIndex;

		orderIndex = regionOrderBase + orderSlot;
		if (g_paiContext.aiController->orderScratch.completionState[0][orderIndex] == 0) {
			const XwaOrder* order;
			uint8_t orderId;
			uint8_t planNameIndex;
			uint8_t planId;
			const char* planName;

			order = &g_missionFlightGroups[orderCoord.fields.flightGroupIdx].fg.orders[orderIndex];
			orderId = order->order;
			planNameIndex = g_orderLeaderBuiltinPlanNameIndex[orderId];
			planId = g_builtinPlanIdByNameIndex[planNameIndex];
			planName = g_planTable[planId].name;
			if (strcmp(planName, "capfreeldr1pln") == 0 || strcmp(planName, "capescortersldr1pln") == 0 ||
				strcmp(planName, "caprespondldr1pln") == 0 || strcmp(planName, "disableldr1pln") == 0 ||
				strcmp(planName, "inspectldr1pln") == 0 || strcmp(planName, "escortldr1pln") == 0 ||
				strcmp(planName, "followtarget1pln") == 0 ||
				(g_missionHeader.body.missionType == XWA_MISSION_TYPE_SKIRMISH &&
				 (strcmp(planName, "capfreeldr1pln") == 0 || strcmp(planName, "hyperspacepln") == 0))) {
				paiorder_selectorderslot(orderSlot);
				return 1;
			}
		}

		++orderSlot;
	}

	return 0;
}

// FUNCTION: XWA 0x4BBF70
char paiorder_completegootherorder(void) {
	unsigned int regionIdx;
	uint8_t orderSlot;
	uint8_t completionState;

	regionIdx = g_paiContext.curOrderCoord.fields.regionIdx;
	orderSlot = g_paiContext.curOrderCoord.fields.orderSlot;
	completionState =
		g_paiContext.aiController->orderScratch.completionState[g_paiContext.curOrderCoord.fields.regionIdx]
															   [g_paiContext.curOrderCoord.fields.orderSlot];
	if (completionState == 3) {
		++orderSlot;
	} else if (completionState == 4) {
		orderSlot = 0;
	} else {
		return 0;
	}

	while (orderSlot < 4) {
		if (g_paiContext.aiController->orderScratch.completionState[regionIdx][orderSlot] == 0) {
			paiorder_selectorderslot(orderSlot);
			return 1;
		}
		++orderSlot;
	}

	if (completionState == 3) {
		orderSlot = 0;
		while (orderSlot < 4) {
			if (g_paiContext.aiController->orderScratch.completionState[regionIdx][orderSlot] == 0) {
				paiorder_selectorderslot(orderSlot);
				return 1;
			}
			++orderSlot;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BC400
char paiorder_orderswitchorder(void) {
	uint8_t orderSlot;
	uint16_t found;

	if (g_paiContext.curOrderCoord.fields.orderSlot == 0 ||
		g_paiContext.aiController->orderScratch
				.completionState[g_paiContext.curOrderCoord.fields.regionIdx]
								[g_paiContext.curOrderCoord.fields.orderSlot] == 1) {
		return 0;
	}

	orderSlot = 0;
	found = 0;
	while (orderSlot < g_paiContext.curOrderCoord.fields.orderSlot) {
		if (found) {
			break;
		}
		if (g_paiContext.aiController->orderScratch
				.completionState[g_paiContext.curOrderCoord.fields.regionIdx][orderSlot] == 0) {
			const XwaOrder* order;
			const char* planName;
			uint8_t planNameIndex;
			uint16_t planId;

			order = &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						 .fg.orders[orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx];
			planNameIndex = g_orderLeaderBuiltinPlanNameIndex[order->order];
			planId = g_builtinPlanIdByNameIndex[planNameIndex];
			planName = g_planTable[planId].name;

			if (strcmp(planName, "capfreeldr1pln") == 0 || strcmp(planName, "caprespondldr1pln") == 0 ||
				strcmp(planName, "capescortersldr1pln") == 0 || strcmp(planName, "followtarget1pln") == 0 ||
				strcmp(planName, "disableldr1pln") == 0) {
				if (paifight_OrderSlotCanTarget(orderSlot, g_paiContext.curOrderCoord.fields.regionIdx)) {
					found = 1;
				}
			} else if ((strcmp(planName, "boardtogivepln") == 0 || strcmp(planName, "boardtotakepln") == 0 ||
						strcmp(planName, "boardtoexchangepln") == 0 ||
						strcmp(planName, "boardtocapturepln") == 0 ||
						strcmp(planName, "boardtodestroypln") == 0 ||
						strcmp(planName, "boardtocontactpln") == 0 ||
						strcmp(planName, "boardtorepairpln") == 0) &&
					   pai_OrderSlotCanBoardTarget(orderSlot, g_paiContext.curOrderCoord.fields.regionIdx)) {
				found = 1;
			}
		}

		++orderSlot;
	}

	if (found) {
		--orderSlot;
		paiorder_selectorderslot(orderSlot);
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4BC7C0
char paiorder_completefolloworder(void) {
	AiController* effectiveTargetAiController;
	uint16_t flightGroupIdx;

	effectiveTargetAiController = pai_GetEffectiveAIController(g_paiContext.aiTargetCraft);

	if (g_paiContext.aiLeaderObjIdx != -1) {
		if (g_paiContext.aiTargetCraft->aiFlight.goHomeFlag == 1 && g_curCraft->aiFlight.goHomeFlag == 0) {
			g_curCraft->aiFlight.goHomeFlag = 1;
		}

		if (g_paiContext.aiTargetCraft->aiFlight.departTimerFlag == 1 &&
			g_curCraft->aiFlight.departTimerFlag == 0) {
			g_curCraft->aiFlight.departTimerFlag = 1;
			flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
			++g_missionFgStats[flightGroupIdx].outcomeCount[22];
			if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == g_curCraft->waveNumber) {
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[22] = 1;
			}
		}
	}

	if ((char)g_paiContext.curOrderCoord.fields.orderSlot != effectiveTargetAiController->currentOrderSlot) {
		paiorder_selectorderslot(effectiveTargetAiController->currentOrderSlot);
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4BA610
char paiorder_leadergohomeorder(void) {
	AiController* effectiveTargetAiController;
	PlanRecord* pendingPlan;
	bool result;

	effectiveTargetAiController = pai_GetEffectiveAIController(g_paiContext.aiTargetCraft);
	pendingPlan = &g_planTable[effectiveTargetAiController->pendingPlanId];
	result = strcmp(pendingPlan->name, "flyhomepln") == 0 ||
			 strcmp(pendingPlan->name, "homeviahyperspacepln") == 0 ||
			 strcmp(pendingPlan->name, "flyhomeevadepln") == 0;
	return result;
}

// FUNCTION: XWA 0x4BA340
char paiorder_leaderdeadorder(void) {
	uint32_t leaderObjIdx;
	CraftData* leaderCraft;
	AiController* leaderAiController;
	char leaderDead;

	leaderObjIdx = (uint32_t)g_curCraft->leader_obj_idx;
	if (leaderObjIdx == 0xffffffffu) {
		return 0;
	}

	if (leaderObjIdx >= g_activeRegionCraftObjectSlotEnd) {
		return 0;
	}

	leaderCraft = g_objectTable[leaderObjIdx].mobj->pCraft;
	leaderAiController = pai_GetEffectiveAIController(leaderCraft);
	leaderDead = 0;
	if (g_objectTable[leaderObjIdx].objectType == OBJ_None) {
		leaderDead = 1;
	}
	if (g_objectTable[leaderObjIdx].flightGroupIdx != g_objectTable[g_paiContext.aiObjIdx].flightGroupIdx) {
		leaderDead = 1;
	}
	if (leaderCraft->objectKind == 3 || leaderCraft->objectKind == 4) {
		leaderDead = 1;
	}
	if (leaderCraft->aiFlight.missionAbortedFlag != 0) {
		leaderDead = 1;
	}
	if (g_objectTable[leaderObjIdx].playerOwnerIdx != -1) {
		leaderDead = 1;
	}

	if (leaderDead) {
		if (g_activeRegionObjectSlotStart < g_activeRegionCraftObjectSlotEnd) {
			uint32_t objIdx;

			objIdx = g_activeRegionObjectSlotStart;
			do {
				CraftData* craft;
				AiController* aiController;

				craft = g_objectTable[objIdx].mobj->pCraft;
				aiController = pai_GetEffectiveAIController(craft);
				if (g_objectTable[objIdx].objectType != OBJ_None &&
					(uint16_t)g_objectTable[objIdx].flightGroupIdx ==
						g_paiContext.curOrderCoord.fields.flightGroupIdx) {
					if (objIdx == g_paiContext.aiObjIdx) {
						craft->leader_obj_idx = -1;
						craft->aiFlight.separation = leaderCraft->aiFlight.separation;
						aiController->waypointIndex = leaderAiController->waypointIndex;
						aiController->targetObjIdx = leaderAiController->targetObjIdx;
						aiController->targetSignature = leaderAiController->targetSignature;
						aiController->hasLiveTarget = leaderAiController->hasLiveTarget;
						pai_UpdateAimPointFromOrderTarget();
					} else {
						craft->leader_obj_idx = (int)g_paiContext.aiObjIdx;
					}
				}
				++objIdx;
			} while (objIdx < g_activeRegionCraftObjectSlotEnd);
		}
	} else {
		AiController* effectiveTargetAiController;
		PlanRecord* pendingPlan;

		effectiveTargetAiController = pai_GetEffectiveAIController(g_paiContext.aiTargetCraft);
		pendingPlan = &g_planTable[effectiveTargetAiController->pendingPlanId];
		if (strcmp(pendingPlan->name, "enterhangarpln") == 0) {
			g_paiContext.aiController->thinkInterval = 59;
		}
	}

	return leaderDead;
}

// FUNCTION: XWA 0x4B9B50
char paiorder_breakofforder(void) {
	uint16_t currentPlanId;
	uint16_t targetObjIdx;
	uint16_t checkObjIdx;

	targetObjIdx = g_paiContext.aiController->targetObjIdx;
	if (targetObjIdx == (uint16_t)g_curCraft->playerCommandAvoidTargetObjIdx) {
		g_paiContext.aiController->targetObjIdx = 0xffffu;
		g_paiContext.aiController->targetSignature = 0;
		g_paiContext.aiController->hasLiveTarget = 0;
		g_paiContext.aiController->candidateTargetIdx = 0xffffu;
		return 1;
	}

	if (!pai_IsObjectTargetable(targetObjIdx)) {
		g_paiContext.aiController->targetObjIdx = 0xffffu;
		g_paiContext.aiController->targetSignature = 0;
		g_paiContext.aiController->hasLiveTarget = 0;
		g_paiContext.aiController->candidateTargetIdx = 0xffffu;
		return 1;
	}

	checkObjIdx = targetObjIdx;
	if (g_paiContext.aiController->targetSignature != g_objectTable[targetObjIdx].objectSignature) {
		g_paiContext.aiController->targetObjIdx = 0xffffu;
		g_paiContext.aiController->targetSignature = 0;
		g_paiContext.aiController->hasLiveTarget = 0;
		g_paiContext.aiController->candidateTargetIdx = 0xffffu;
		return 1;
	}

	currentPlanId = g_paiContext.aiController->currentPlanId;
	if (strcmp(g_planTable[currentPlanId].name, "disableldr1pln") == 0 ||
		strcmp(g_planTable[currentPlanId].name, "playerdisableldr2pln") == 0) {
		if ((uint32_t)targetObjIdx >= g_activeRegionObjectSlotStart &&
			(uint32_t)targetObjIdx < g_activeRegionCraftObjectSlotEnd &&
			!g_objectTable[targetObjIdx].mobj->pCraft->workingSubsystems) {
			g_paiContext.aiController->targetObjIdx = 0xffffu;
			g_paiContext.aiController->targetSignature = 0;
			g_paiContext.aiController->candidateTargetIdx = 0xffffu;
			return 1;
		}
	}

	if (g_objectTable[targetObjIdx].playerOwnerIdx != -1 &&
		(uint32_t)targetObjIdx >= g_activeRegionObjectSlotStart &&
		(uint32_t)targetObjIdx < g_activeRegionCraftObjectSlotEnd &&
		Object_HasActiveDecoyBeam(checkObjIdx) == 1) {
		pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, targetObjIdx);
		if ((uint32_t)g_targetRangeScore > 0x4000u) {
			g_paiContext.aiController->targetObjIdx = 0xffffu;
			g_paiContext.aiController->targetSignature = 0;
			g_paiContext.aiController->hasLiveTarget = 0;
			g_paiContext.aiController->candidateTargetIdx = 0xffffu;
			return 1;
		}
	}

	if (g_missionFormatVersion < 14) {
		return 0;
	}

	{
		uint16_t targetStatus;
		MobileObject* mobj;

		targetStatus = g_objectTable[targetObjIdx].typeSpecificWord;
		mobj = g_objectTable[targetObjIdx].mobj;
		if (mobj != NULL && mobj->pCraft != NULL) {
			targetStatus = mobj->pCraft->workingSubsystems;
		}

		if (targetStatus != 0 || checkObjIdx == g_paiContext.aiController->candidateTargetIdx ||
			pai_CurrentOrderTargetsMatchObject(checkObjIdx)) {
			return 0;
		}
	}

	g_paiContext.aiController->targetObjIdx = 0xffffu;
	g_paiContext.aiController->targetSignature = 0;
	g_paiContext.aiController->hasLiveTarget = 0;
	g_paiContext.aiController->candidateTargetIdx = 0xffffu;
	return 1;
}

// FUNCTION: XWA 0x4B8EB0
char paiorder_stillattackorder(void) {
	unsigned int lastAttackerObjIdx;
	WarheadGuidanceState* guidance;

	if (g_paiContext.aiController->maneuverMode == g_paiContext.aiPlanInitialManeuverId ||
		g_curCraft->lastAttackerObjIdx == 0xffffu) {
		return 0;
	}

	lastAttackerObjIdx = g_curCraft->lastAttackerObjIdx;
	if (lastAttackerObjIdx < g_projectileObjectSlotStart || lastAttackerObjIdx >= g_projectileObjectSlotEnd) {
		if (!pai_IsObjectWithinCurrentPointRange(
				lastAttackerObjIdx, g_aiStillAttackLastAttackerRangeBySkill[g_paiContext.aiSkillTier])) {
			g_curCraft->lastAttackerObjIdx = 0xffffu;
			return 1;
		}
	} else {
		guidance = g_objectTable[lastAttackerObjIdx].mobj->pWarheadGuidance;
		if (g_objectTable[lastAttackerObjIdx].objectType == OBJ_None ||
			guidance->targetObjIdx != g_paiContext.aiObjIdx) {
			g_curCraft->lastAttackerObjIdx = 0xffffu;
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BB6F0
char paiorder_evasiveorder(void) {
	if (g_paiContext.aiController->maneuverMode != g_paiContext.aiPlanInitialManeuverId ||
		g_paiContext.aiController->candidateTargetIdx != 251) {
		return 0;
	}

	g_paiContext.aiController->targetObjIdx = 0xffff;
	g_paiContext.aiController->targetSignature = 0;
	g_paiContext.aiController->hasLiveTarget = 0;
	g_paiContext.aiController->candidateTargetIdx = 0xffff;
	return 1;
}

// FUNCTION: XWA 0x4BDA00
char paiorder_resumemissionorder(void) {
	g_paiContext.aiController->currentPlanId = g_paiContext.aiSelfCraft->savedCurrentPlan;
	g_paiContext.nullPlanId = g_paiContext.aiSelfCraft->savedCurrentPlan;
	return 1;
}

// FUNCTION: XWA 0x4B9B00
char paiorder_waitrunorder(void) {
	if ((uint16_t)g_paiContext.aiController->maneuverMode == g_paiContext.aiPlanInitialManeuverId) {
		uint16_t skillValue;

		skillValue = pai_GetEffectiveSkillValue(g_curCraft);
		if (pai_IsObjectWithinCurrentPointRange(g_paiContext.aiController->targetObjIdx,
												(unsigned int)skillValue + 0x20000u) == 1) {
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BA6F0
char paiorder_hyperspaceorder(void) {
	if (g_curCraft->leader_obj_idx == -1 || g_curCraft->aiFlight.missionAbortedFlag != 0 ||
		strcmp(g_planTable[g_paiContext.aiController->pendingPlanId].name, "flyhomeevadepln") == 0) {
		if (g_modelDefs[g_curCraft->modelIndex].hasHyperdrive != 0) {
			if (!g_curCraft->wasCaptured) {
				uint8_t departMethod;

				departMethod =
					g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.departMethod;
				if (departMethod == 0 || departMethod == 2) {
					return 1;
				}
			} else if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						   .fg.capturedDepartViaMothership == 0) {
				return 1;
			}
		}
		return 0;
	}

	if (g_paiContext.aiTargetCraft->objectKind == 5) {
		{
			AiController* effectiveTargetController;

			effectiveTargetController = pai_GetEffectiveAIController(g_paiContext.aiTargetCraft);
			if (strcmp(g_planTable[effectiveTargetController->currentPlanId].name, "homeviahyperspacepln") ==
				0) {
				g_paiContext.aiController->pendingPlanId =
					(uint8_t)pai_findplanbyname("homeviahyperspacepln");
				g_paiContext.aiController->currentPlanId = g_paiContext.aiController->pendingPlanId;
				g_paiContext.aiController->targetObjIdx = effectiveTargetController->targetObjIdx;
			} else {
				g_paiContext.aiController->pendingPlanId = (uint8_t)pai_findplanbyname("intohyperspacepln");
			}
		}

		g_curCraft->objectKind = 5;
		g_curCraft->aiFlight.enterFlag = 0;
		g_curCraft->aiFlight.headingState = 0;
		g_curCraft->aiFlight.turnState = 0;
		g_paiContext.aiController->maneuverMode = 21;
		g_curCraft->pushAccumX = 0;
		g_curCraft->pushAccumY = 0;
		g_curCraft->pushAccumZ = 0;
		g_paiContext.aiController->maneuverPhase = 1;
		g_paiContext.aiController->aiPlanState = 944;
		g_paiContext.aiController->maneuverTimer = 2360;
		paiman_setpower((int)g_paiContext.aiObjIdx, 0xffffu);
	}
	return 0;
}

// FUNCTION: XWA 0x4BC890
char paiorder_killselforder(void) {
	int lifetimeSeconds;

	if (g_objectTable[g_paiContext.aiObjIdx].mobj->lifetimeTimer == 0) {
		lifetimeSeconds =
			Mission_DecodeOrderTime(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg
										.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
												g_paiContext.curOrderCoord.fields.orderSlot]
										.variable1);
		if (lifetimeSeconds == 0) {
			lifetimeSeconds = (GameRand() & 3) + 2;
		}

		g_objectTable[g_paiContext.aiObjIdx].mobj->lifetimeTimer = 236 * lifetimeSeconds;
	}

	return 0;
}

// FUNCTION: XWA 0x4BB640
char paiorder_waitforallcreateorder(void) {
	uint16_t fgIdx;

	for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		if ((g_missionFgStats[fgIdx].arrivalEnabled || g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) &&
			fgIdx != g_paiContext.curOrderCoord.fields.flightGroupIdx &&
			g_missionFlightGroups[fgIdx].fg.arrivalMethod != 0 &&
			g_missionFlightGroups[fgIdx].fg.arrivalMothership ==
				g_paiContext.curOrderCoord.fields.flightGroupIdx &&
			(!g_missionFgStats[fgIdx].hasArrived || g_missionFgStats[fgIdx].wavesRemaining)) {
			return 0;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x4BB530
char paiorder_waitforallreturnorder(void) {
	uint16_t fgIdx;

	for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		if ((g_missionFgStats[fgIdx].arrivalEnabled || g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) &&
			fgIdx != g_paiContext.curOrderCoord.fields.flightGroupIdx &&
			g_missionFlightGroups[fgIdx].fg.departMethod != 0 &&
			g_missionFlightGroups[fgIdx].fg.departMethod != 2 &&
			g_missionFlightGroups[fgIdx].fg.departureMothership ==
				g_paiContext.curOrderCoord.fields.flightGroupIdx) {
			uint16_t objIdx;

			if (!g_missionFgStats[fgIdx].hasArrived || g_missionFgStats[fgIdx].wavesRemaining) {
				return 0;
			}

			objIdx = (uint16_t)g_activeRegionObjectSlotStart;
			while (objIdx < g_activeRegionCraftObjectSlotEnd) {
				if (g_objectTable[objIdx].objectType != OBJ_None &&
					g_objectTable[objIdx].flightGroupIdx == fgIdx) {
					return 0;
				}
				++objIdx;
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x4BCE70
char paiorder_checkdeliverorder(void) {
	int orderIndex;
	unsigned int targetFgIdx;
	uint32_t objIdx;
	unsigned int nearestObjScore;
	unsigned int nearestObjIdx;

	if (g_paiContext.aiController->orderScratch
			.goalProgress[0][g_paiContext.curOrderCoord.fields.orderSlot +
							 4 * g_paiContext.curOrderCoord.fields.regionIdx] != 0) {

		orderIndex =
			g_paiContext.curOrderCoord.fields.orderSlot + 4 * g_paiContext.curOrderCoord.fields.regionIdx;
		targetFgIdx = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						  .fg.orders[orderIndex]
						  .variable1;
		nearestObjScore = 0xffffffffu;
		nearestObjIdx = 0xffffu;

		for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
			ObjectRecord* obj;

			obj = &g_objectTable[objIdx];
			if (obj->objectType != OBJ_None && obj->flightGroupIdx == targetFgIdx) {
				pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, objIdx);
				if ((unsigned int)g_targetRangeScore < nearestObjScore) {
					nearestObjScore = (unsigned int)g_targetRangeScore;
					nearestObjIdx = objIdx;
				}
			}
		}

		if (nearestObjIdx != 0xffffu) {
			ObjectRecord* targetObj;
			unsigned int modelIndex;
			unsigned int dockIdx;
			unsigned int dockPointCount;
			unsigned int selectedDockScore;
			unsigned int selectedDockIdx;

			Mission_ResolveObjectOrMissionPointWorldLoc(nearestObjIdx, 0, 0, 0);
			targetObj = &g_objectTable[nearestObjIdx];
			modelIndex = (uint16_t)g_modelTypeTable[targetObj->objectType].modelIndex;
			selectedDockScore = 0xffffffffu;
			dockIdx = 0;
			dockPointCount = g_modelDefs[modelIndex].dockPointCount;
			selectedDockIdx = 0xffffu;

			if (dockPointCount > 0) {
				const Vec3i* dockPoint = g_modelDefs[modelIndex].dockPoints;

				do {
					int dockWorldX;
					int dockWorldY;
					int dockWorldZ;

					dockWorldY = worldlocy + dockPoint->y;
					dockWorldX = worldlocx + dockPoint->x;
					dockWorldZ = worldlocz + dockPoint->z;
					g_targetRangeScore =
						collide_roughdistance3d(dockWorldX - g_objectTable[g_paiContext.aiObjIdx].world_x,
												dockWorldY - g_objectTable[g_paiContext.aiObjIdx].world_y,
												dockWorldZ - g_objectTable[g_paiContext.aiObjIdx].world_z);

					if ((unsigned int)g_targetRangeScore < selectedDockScore) {
						int occupied = 0;

						for (objIdx = g_activeRegionObjectSlotStart;
							 objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
							ObjectRecord* obj;

							obj = &g_objectTable[objIdx];
							if (obj->objectType != OBJ_None && objIdx != g_paiContext.aiObjIdx) {
								AiController* aiController;

								aiController = pai_GetEffectiveAIController(obj->mobj->pCraft);
								if (dockWorldX == aiController->aimPointX &&
									dockWorldY == aiController->aimPointY &&
									dockWorldZ == aiController->aimPointZ) {
									occupied = 1;
									break;
								}
							}
						}

						if (!occupied) {
							selectedDockIdx = dockIdx;
							selectedDockScore = (unsigned int)g_targetRangeScore;
						}
					}

					++dockIdx;
					++dockPoint;
				} while (dockIdx < dockPointCount);
			}

			g_paiContext.aiController->aimPointX =
				worldlocx + g_modelDefs[modelIndex].dockPoints[selectedDockIdx].x;
			g_paiContext.aiController->aimPointY =
				worldlocy + g_modelDefs[modelIndex].dockPoints[selectedDockIdx].y;
			g_paiContext.aiController->aimPointZ =
				worldlocz + g_modelDefs[modelIndex].dockPoints[selectedDockIdx].z;
		}

		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4BC930
char paiorder_dropoffdestorder(void) {
	int targetFgIdx;

	targetFgIdx = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg
					  .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							  g_paiContext.curOrderCoord.fields.orderSlot]
					  .variable2;

	if (g_missionFgStats[targetFgIdx].outcomeCount[1] == 0) {
		Mission_ResolveFormationSlotWorldLoc(targetFgIdx, 0, 0xffffu);
		g_paiContext.aiController->aimPointX = worldlocx;
		g_paiContext.aiController->aimPointY = worldlocy;
		g_paiContext.aiController->aimPointZ = worldlocz + 932;
		pai_CalcAnglesToAimPoint();

		if ((int)trig2_polardistance < 0x4000) {
			paiman_setpower((int)g_paiContext.aiObjIdx, 0xc000u);
		}
		if ((int)trig2_polardistance < 4096) {
			paiman_setpower((int)g_paiContext.aiObjIdx, 0x6000u);
		}
		if ((int)trig2_polardistance < 2048) {
			g_paiContext.aiController->waypointIndex = 0;
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BCDB0
char paiorder_checkreleaseorder(void) {
	int orderIndex;

	orderIndex =
		4 * g_paiContext.curOrderCoord.fields.regionIdx + g_paiContext.curOrderCoord.fields.orderSlot;
	if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg.orders[orderIndex]
			.waypoints[0]
			.enabled == 0) {
		g_paiContext.aiController->aimPointX = g_objectTable[g_paiContext.aiObjIdx].world_x;
		g_paiContext.aiController->aimPointY = g_objectTable[g_paiContext.aiObjIdx].world_y;
		g_paiContext.aiController->aimPointZ = g_objectTable[g_paiContext.aiObjIdx].world_z;
		return 1;
	}

	if (g_paiContext.aiController->orderScratch.goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
															[g_paiContext.curOrderCoord.fields.orderSlot] !=
		0) {
		return 1;
	} else {
		return 0;
	}
}

// FUNCTION: XWA 0x4BD130
char paiorder_changesidesorder(void) {
	uint16_t fgIndex;
	uint32_t orderCoord;
	uint8_t newIff;
	uint8_t newTeam;
	uint32_t objectIdx;

	orderCoord = g_paiContext.curOrderCoord.raw;
	fgIndex = (uint16_t)orderCoord;
	newIff = g_missionFlightGroups[fgIndex]
				 .fg
				 .orders[g_paiContext.curOrderCoord.fields.orderSlot +
						 4 * g_paiContext.curOrderCoord.fields.regionIdx]
				 .variable1;
	newTeam = g_missionFlightGroups[fgIndex]
				  .fg
				  .orders[g_paiContext.curOrderCoord.fields.orderSlot +
						  4 * g_paiContext.curOrderCoord.fields.regionIdx]
				  .variable2;
	for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		if (g_objectTable[objectIdx].objectType != OBJ_None &&
			g_objectTable[objectIdx].flightGroupIdx == fgIndex) {
			g_objectTable[objectIdx].mobj->iff = (int8_t)newIff;
			g_objectTable[objectIdx].mobj->team = newTeam;
			orderCoord = g_paiContext.curOrderCoord.raw;
			fgIndex = (uint16_t)orderCoord;
			++g_missionFgStats[fgIndex].outcomeCount[32];
			if (g_missionFlightGroups[fgIndex].fg.specialCargoCraft ==
				g_objectTable[objectIdx].mobj->pCraft->waveNumber) {
				g_missionFgStats[fgIndex].specialCargoOutcome[32] = 1;
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BDA30
char paiorder_inspectedorder(void) {
	AiController* aiController;
	ObjectRecord* objectTable;
	uint16_t targetObjIdx;
	int8_t* iffVisibility;
	int inspectorTeam;

	aiController = g_paiContext.aiController;
	if (!((aiController->maneuverMode == 12 && aiController->maneuverPhase == 1 &&
		   aiController->maneuverTimer > 472) ||
		  aiController->targetObjIdx == 0xffffu)) {
		targetObjIdx = aiController->targetObjIdx;
		objectTable = g_objectTable;
		inspectorTeam = (int8_t)objectTable[g_paiContext.aiObjIdx].mobj->team;
		iffVisibility = (int8_t*)&objectTable[targetObjIdx].mobj->pCraft->iffVisibility[inspectorTeam];
		if (*iffVisibility > 0) {
			aiController->targetObjIdx = 0xffffu;
			g_paiContext.aiController->targetSignature = 0;
			g_paiContext.aiController->hasLiveTarget = 0;
			return 1;
		}

		if (targetObjIdx >= g_activeRegionObjectSlotStart &&
			targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
			pai_ObjectRefDirectionToObjectRef(g_paiContext.aiObjIdx, targetObjIdx);
			paiorder_UpdateInspectionVisibility(g_paiContext.aiObjIdx, targetObjIdx,
												(unsigned int)trig2_polardistance);
			aiController = g_paiContext.aiController;
		}

		if (*iffVisibility > 0) {
			aiController->targetObjIdx = 0xffffu;
			g_paiContext.aiController->targetSignature = 0;
			g_paiContext.aiController->hasLiveTarget = 0;
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BDB30
char paiorder_UpdateInspectionVisibility(unsigned int inspectorObjIdx, unsigned int targetObjIdx,
										 unsigned int range) {
	MobileObject* targetMobj;
	CraftData* targetCraft;
	int inspectorTeam;
	unsigned int inspectionRadius;

	targetMobj = g_objectTable[targetObjIdx].mobj;
	if (targetMobj == NULL) {
		return 0;
	}

	targetCraft = targetMobj->pCraft;
	if (targetCraft == NULL) {
		return 0;
	}

	inspectorTeam = (int8_t)g_objectTable[inspectorObjIdx].mobj->team;
	if ((int8_t)targetCraft->iffVisibility[inspectorTeam] > 0) {
		return 0;
	}

	inspectionRadius =
		4u * (unsigned int)g_modelTypeTable[g_objectTable[targetObjIdx].objectType].maxBoundsExtent;
	if (inspectionRadius < 0x2800u) {
		inspectionRadius = 0x2800u;
	}
	if (inspectionRadius > 0xa000u) {
		inspectionRadius = 0xa000u;
	}

	if (range < inspectionRadius) {
		int8_t maxVisibility;
		int remainingTeams;
		int flightGroupIdx;

		maxVisibility = 0;
		remainingTeams = 10;
		do {
			if ((int8_t)targetCraft->iffVisibility[10 - remainingTeams] > maxVisibility) {
				maxVisibility = (int8_t)targetCraft->iffVisibility[10 - remainingTeams];
			}
		} while (--remainingTeams != 0);

		targetCraft->iffVisibility[inspectorTeam] = (uint8_t)(maxVisibility + 1);
		++g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_INSPECTED][inspectorTeam]
													[g_objectTable[targetObjIdx].flightGroupIdx];
		flightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
		++g_missionFgStats[flightGroupIdx].outcomeCount[8];
		++g_missionFgStats[flightGroupIdx].teamInspected[inspectorTeam];

		if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == targetCraft->waveNumber) {
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[8] = 1;
			++g_missionFgStats[flightGroupIdx].teamSpecialCargoInspected[inspectorTeam];
		}

		if (inspectorTeam == (uint16_t)g_players[g_localPlayer].playerIff &&
			targetObjIdx == (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx) {
			msg_emitInFlightMessage(MSG_TARGET_INSPECT_CMP, g_localPlayer);
			fsfx_PlaySound(SFX_TARGET_INSPECT_CMP, g_players[g_localPlayer].objectIndex, g_localPlayer);
		}

		return 1;
	}

	if ((unsigned int)trig2_polardistance < 6u * inspectionRadius) {
		int flightGroupIdx;

		targetCraft->iffVisibility[inspectorTeam] = 0;
		flightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
		++g_missionFgStats[flightGroupIdx].outcomeCount[29];
		++g_missionFgStats[flightGroupIdx].teamPartiallyInspected[inspectorTeam];

		if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == targetCraft->waveNumber) {
			g_missionFgStats[flightGroupIdx].specialCargoOutcome[29] = 1;
			++g_missionFgStats[flightGroupIdx].teamSpecialCargoPartiallyInspected[inspectorTeam];
		}

		return 1;
	}

	return 0;
}
