#include "xwa/flight/ai/ai_internal.h"

#include "aeron/log.h"

// GLOBAL: XWA 0x7CA1A0
PaiContext g_paiContext;

// ---- AI trace (debug instrumentation) -------------------------------------
// Runtime tracing of one flight group's complete AI state machine: plan
// activations, order-driven plan transitions, and per-tick maneuver/phase/
// distance. Gated by flight-group NAME so the craft is followed through every
// plan/maneuver it runs -- including incorrect ones (a wrong plan would still
// be logged).
//
// OFF by default. To use: set g_paiTrace = 1 (e.g. from a debugger or a
// temporary edit) and point g_paiTraceFgName at the craft under investigation,
// then filter the log on category "xwa.ai.trace". Retained diagnostic; has no
// counterpart in the original binary.
int g_paiTrace = 0;

// Flight group to trace, by name (only consulted when g_paiTrace != 0).
static const char* g_paiTraceFgName = "Selu";

static int pai_TraceThisCraft(void) {
	if (!g_paiTrace) {
		return 0;
	}
	return strcmp(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.name,
				  g_paiTraceFgName) == 0;
}

// Manhattan magnitude; used only to compare distance/offset orders of magnitude
// in the trace, not for game logic.
static int pai_TraceAbsSum(int dx, int dy, int dz) {
	return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + (dz < 0 ? -dz : dz);
}

void pai_TraceManeuverTick(char maneuverResult) {
	static uint32_t lastObjIdx = 0xffffffffu;
	static uint8_t lastPlan = 0xffu;
	static uint8_t lastMode = 0xffu;
	static int lastPhase = -1;
	static int lastDistBucket = -1;
	AiController* controller;
	int distBucket;

	if (!pai_TraceThisCraft()) {
		return;
	}
	controller = g_paiContext.aiController;

	// Collapse runs of identical state to one line per change. Distance is
	// bucketed (~2048 units) so a steady approach still emits periodic progress.
	distBucket = (int)((uint32_t)trig2_polardistance >> 11);
	if (g_paiContext.aiObjIdx == lastObjIdx && controller->pendingPlanId == lastPlan &&
		controller->maneuverMode == lastMode && controller->maneuverPhase == lastPhase &&
		distBucket == lastDistBucket) {
		return;
	}
	lastObjIdx = g_paiContext.aiObjIdx;
	lastPlan = controller->pendingPlanId;
	lastMode = controller->maneuverMode;
	lastPhase = controller->maneuverPhase;
	lastDistBucket = distBucket;

	{
		ObjectRecord* self = &g_objectTable[g_paiContext.aiObjIdx];
		uint16_t tgtIdx = controller->targetObjIdx;
		uint16_t yawErr = (uint16_t)(controller->targetXYAngle - self->yaw);
		int tgtDist = -1; // ship -> real target object
		int aimOff = -1;  // aim point -> real target object (offset applied by maneuver)

		if (tgtIdx < 0x8000u && g_objectTable[tgtIdx].objectType != OBJ_None) {
			ObjectRecord* tgt = &g_objectTable[tgtIdx];
			tgtDist = pai_TraceAbsSum(tgt->world_x - self->world_x, tgt->world_y - self->world_y,
									  tgt->world_z - self->world_z);
			aimOff =
				pai_TraceAbsSum(controller->aimPointX - tgt->world_x, controller->aimPointY - tgt->world_y,
								controller->aimPointZ - tgt->world_z);
		}

		Aeron_Log("xwa.ai.trace",
				  "TICK fg=%u(%s) obj=%u plan='%s' mode=%u phase=%d distAim=%d tgtDist=%d aimOff=%d "
				  "yaw=0x%04x tgtYaw=0x%04x yawErr=0x%04x throttle=%u speed=%u mScale=0x%04x "
				  "turnRate=%d turnStep=0x%04x turnAccel=0x%04x turnState=%d ret=%d",
				  (unsigned)g_paiContext.curOrderCoord.fields.flightGroupIdx,
				  g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.name,
				  (unsigned)g_paiContext.aiObjIdx, g_planTable[controller->pendingPlanId].name,
				  (unsigned)controller->maneuverMode, (int)controller->maneuverPhase,
				  (int)trig2_polardistance, tgtDist, aimOff, (unsigned)self->yaw,
				  (unsigned)controller->targetXYAngle, (unsigned)yawErr, (unsigned)g_curCraft->throttleSpeed,
				  (unsigned)(self->mobj ? self->mobj->speed : 0),
				  (unsigned)(uint16_t)g_curCraft->aiFlight.motionScale, (int)g_curCraft->aiFlight.turnRate,
				  (unsigned)(uint16_t)g_curCraft->aiFlight.turnStep,
				  (unsigned)(uint16_t)g_curCraft->aiFlight.turnAccel, (int)g_curCraft->aiFlight.turnState,
				  (int)maneuverResult);
	}
}

// GLOBAL: XWA 0x7B4BF8
int g_targetRangeScore;

// GLOBAL: XWA 0x7B33B0
uint8_t g_aiEscortCandidateFgIdx;

// GLOBAL: XWA 0x5B76A0
const uint8_t g_aiThreatBearingClassByOctant[8] = { 0, 1, 1, 2, 2, 1, 1, 0 };

// GLOBAL: XWA 0x5B0EC0
const uint16_t g_aiSkillValueQ16ByLevel[8] = { 0, 0x4000, 0x8000, 0xc000, 0xffff, 0xffff, 0, 0 };

// GLOBAL: XWA 0x5B0ED0
const uint16_t g_aiThinkIntervalByGroupAI[8] = { 708, 472, 236, 118, 59, 29, 0, 0 };

void pai_SetFlightGroupFormation(int flightGroupIdx, int formationType, int separation);
int16_t pai_FindMothershipObjectAnyRegion(int16_t mothershipFlightGroupIdx);
bool pai_IsStaticBoardingPlanId(uint16_t planId);

// FUNCTION: XWA 0x4A2C50
void pai_SetFlightGroupFormation(int flightGroupIdx, int formationType, int separation) {
	unsigned int objIdx;

	objIdx = g_activeRegionObjectSlotStart;
	if (objIdx >= g_activeRegionCraftObjectSlotEnd) {
		return;
	}

	do {
		if (g_objectTable[objIdx].objectType != OBJ_None &&
			g_objectTable[objIdx].flightGroupIdx == flightGroupIdx) {
			CraftData* craft;

			craft = g_objectTable[objIdx].mobj->pCraft;
			craft->aiFlight.formationType = formationType;
			craft->aiFlight.separation = separation;
		}
		++objIdx;
	} while (objIdx < g_activeRegionCraftObjectSlotEnd);
}

// FUNCTION: XWA 0x4A1D80
void pai_UpdateAllCraftAI(void) {
	uint32_t objIdx;

	if (g_provingGroundsModeActive == 1) {
		return;
	}

	GameRand_SavePrimarySeed();

	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
		if (g_objectTable[objIdx].objectType != OBJ_None && g_objectTable[objIdx].mobj->state == 0) {
			AiController* aiController;
			int playerOwnerIdx;

			g_curCraft = g_objectTable[objIdx].mobj->pCraft;
			aiController = pai_GetEffectiveAIController(g_curCraft);

			if (g_curCraft->objectKind != GENUS_Freighter && g_curCraft->objectKind != GENUS_Starship &&
				aiController->thinkTimer <= 0) {
				playerOwnerIdx = g_objectTable[objIdx].playerOwnerIdx;
				if (playerOwnerIdx != -1 && !g_players[playerOwnerIdx].aiControlledFlag) {
					if (g_players[playerOwnerIdx].currentSeatIdx == 0 &&
						g_players[playerOwnerIdx].turretAutoFireState == 1 &&
						g_objectTable[objIdx].genusId == GENUS_Transport) {
						char hasFreeTurretSlot;
						int slotIdx;

						hasFreeTurretSlot = 0;
						for (slotIdx = 0; slotIdx < g_curCraft->laserSlotCount; ++slotIdx) {
							WarheadInventoryEntry* weapon;

							weapon = &g_curCraft->warheadData[slotIdx];
							if (weapon->weaponType >= 4u && (uint16_t)weapon->turretTargetObjIdx == 0xffffu) {
								hasFreeTurretSlot = 1;
								break;
							}
						}

						if (hasFreeTurretSlot) {
							g_curCraft->lastAttackerObjIdx = 0xffffu;
							pai_setupcraftcontext((int)objIdx);
							paifight_gunnerselfdefenseorder();
						}
					}
				} else {
					pai_setupcraftcontext((int)objIdx);
					Math_SeedRandom((uint16_t)aiController->savedRandSeed);
					pai_ProcessPlan();
					aiController->savedRandSeed = (int16_t)GameRand_GetPrimarySeed();
				}

				aiController->thinkTimer += aiController->thinkInterval;
				if (playerOwnerIdx != -1 && g_players[playerOwnerIdx].aiControlledFlag) {
					aiController->thinkTimer = 0;
				}
			}
		}
	}

	GameRand_RestorePrimarySeed();
}

// FUNCTION: XWA 0x4A22C0
void pai_ProcessPlan(void) {
	uint32_t objIdx;
	AiController* aiController;
	uint8_t orderId;

	aiController = pai_GetEffectiveAIController(g_curCraft);
	objIdx = g_paiContext.aiObjIdx;

	if (g_objectTable[objIdx].playerOwnerIdx != -1 &&
		strcmp(g_planTable[aiController->currentPlanId].name, "escortldr1pln") == 0) {
		paifight_checkescortorder();
		objIdx = g_paiContext.aiObjIdx;
	}

	while ((orderId = *g_paiContext.planCursor++) != 0) {
		uint8_t* nextPlanPtr;

		objIdx = g_paiContext.aiObjIdx;
		if (g_objectTable[objIdx].objectType == OBJ_None) {
			break;
		}

		if (g_orderTable[orderId]()) {
			nextPlanPtr = g_paiContext.planCursor;
			if (strcmp(g_planTable[*nextPlanPtr].name, "nullpln") != 0) {
#ifdef XWA_MODERN
				if (pai_TraceThisCraft()) {
					Aeron_Log("xwa.ai.trace", "TRANSITION fg=%u(%s) obj=%u order=%u '%s' -> '%s'",
							  (unsigned)g_paiContext.curOrderCoord.fields.flightGroupIdx,
							  g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.name,
							  (unsigned)g_paiContext.aiObjIdx, (unsigned)orderId,
							  g_planTable[aiController->pendingPlanId].name, g_planTable[*nextPlanPtr].name);
				}
#endif
				if (strcmp(g_planTable[*nextPlanPtr].name, "variablepln") == 0) {
					aiController->pendingPlanId = g_paiContext.nullPlanId;
				} else {
					aiController->pendingPlanId = *nextPlanPtr;
				}
				pai_setupcraftcontext((int)g_paiContext.aiObjIdx);
				pai_ApplyPendingPlanTargetAndManeuver(g_paiContext.aiObjIdx);
				return;
			}
		}

		++g_paiContext.planCursor;
	}

	return;
}

// FUNCTION: XWA 0x4A2BE0
void pai_UpdateAimPointFromOrderTarget(void) {
	Mission_ResolveObjectOrMissionPointWorldLoc(
		g_paiContext.aiController->targetObjIdx, g_objectTable[g_paiContext.aiObjIdx].flightGroupIdx,
		g_paiContext.curOrderCoord.fields.regionIdx, g_paiContext.curOrderCoord.fields.orderSlot);
	g_paiContext.aiController->aimPointX = worldlocx;
	g_paiContext.aiController->aimPointY = worldlocy;
	g_paiContext.aiController->aimPointZ = worldlocz;
}

// FUNCTION: XWA 0x4A1F30
int pai_ApplyPendingPlanTargetAndManeuver(unsigned int objectIdx) {
	AiController* effectiveController;
	uint8_t* planData;
	unsigned int objIdx;
	uint16_t targetToken;
	uint8_t maneuverToken;

	effectiveController = pai_GetEffectiveAIController(g_curCraft);
	objIdx = objectIdx;
	planData = g_planDataPtrs[effectiveController->pendingPlanId];
	targetToken = *planData++;

	if (targetToken != 0xffu) {
		if (targetToken == 0xfdu) {
			if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.missionPoints[XWA_FG_POINT_CAPTURE_HYPER]
					.enabled != 0) {
				effectiveController->targetObjIdx = 0x8002u;
			} else {
				effectiveController->targetObjIdx = 0x8000u;
			}
			effectiveController->targetSignature = 0;
			effectiveController->hasLiveTarget = 0;
		} else if (targetToken == 0xfeu) {
			char targetHandled;

			targetHandled = 0;
			if (strcmp(g_planTable[effectiveController->pendingPlanId].name, "hyperspacepln") == 0) {
				if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg
						.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot]
						.waypoints[(uint8_t)effectiveController->waypointIndex]
						.enabled != 0) {
					effectiveController->targetSignature = 0;
					effectiveController->targetObjIdx =
						(uint16_t)(0x8004u + (uint8_t)effectiveController->waypointIndex);
				} else {
					int worldZ;

					effectiveController->aimPointX = g_objectTable[objIdx].world_x;
					effectiveController->aimPointY = g_objectTable[objIdx].world_y;
					worldZ = g_objectTable[objIdx].world_z;
					effectiveController->targetObjIdx = 0xffffu;
					effectiveController->aimPointZ = worldZ;
					effectiveController->targetSignature = 0;
				}
				effectiveController->hasLiveTarget = 0;
				targetHandled = 1;
			}

			if (!targetHandled) {
				if (g_curCraft->wasCaptured &&
					g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg.missionPoints[XWA_FG_POINT_CAPTURE_HYPER]
							.enabled != 0) {
					effectiveController->targetObjIdx = 0x8002u;
				} else if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							   .fg.missionPoints[XWA_FG_POINT_HYPER]
							   .enabled != 0) {
					effectiveController->targetObjIdx = 0x8003u;
				} else {
					effectiveController->targetObjIdx = 0x8000u;
				}
				effectiveController->targetSignature = 0;
				effectiveController->hasLiveTarget = 0;
			}
		} else if (targetToken == 0xf9u) {
			effectiveController->targetObjIdx = 0x8002u;
			effectiveController->targetSignature = 0;
			effectiveController->hasLiveTarget = 0;
		} else {
			if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg
					.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							g_paiContext.curOrderCoord.fields.orderSlot]
					.waypoints[(uint8_t)effectiveController->waypointIndex]
					.enabled != 0) {
				effectiveController->targetObjIdx =
					(uint16_t)(0x8004u + (uint8_t)effectiveController->waypointIndex);
			} else if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						   .fg
						   .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								   g_paiContext.curOrderCoord.fields.orderSlot]
						   .waypoints[0]
						   .enabled != 0) {
				effectiveController->targetObjIdx = 0x8004u;
			} else {
				effectiveController->targetObjIdx = 0x8000u;
			}
			effectiveController->targetSignature = 0;
			effectiveController->hasLiveTarget = 0;
		}

		if (effectiveController->targetObjIdx != 0xffffu) {
			Mission_ResolveObjectOrMissionPointWorldLoc(
				g_paiContext.aiController->targetObjIdx, g_objectTable[g_paiContext.aiObjIdx].flightGroupIdx,
				g_paiContext.curOrderCoord.fields.regionIdx, g_paiContext.curOrderCoord.fields.orderSlot);
			g_paiContext.aiController->aimPointX = worldlocx;
			g_paiContext.aiController->aimPointY = worldlocy;
			g_paiContext.aiController->aimPointZ = worldlocz;
		}
	}

	effectiveController->aiPlanState = 0;
	maneuverToken = *planData;
	if (maneuverToken != 0xffu) {
		if (maneuverToken == 6u && (g_objectTable[objIdx].genusId == GENUS_WeaponEmplacement ||
									g_objectTable[objIdx].genusId == GENUS_SatelliteBuoy)) {
			if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg
					.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							g_paiContext.curOrderCoord.fields.orderSlot]
					.waypoints[0]
					.enabled != 0) {
				effectiveController->maneuverMode = 34;
			} else {
				effectiveController->maneuverMode = 6;
			}
		} else {
			effectiveController->maneuverMode = maneuverToken;
		}
		paiman_initmaneuver();
	}

	g_curCraft->lastAttackerObjIdx = 0xffffu;
	g_curCraft->lastHitTimestamp = 0;
	g_curCraft->aiFlight.threatObjIdx = 0xffffu;
	effectiveController->thinkTimer =
		(unsigned int)(effectiveController->thinkInterval * (int)(objIdx & 7u)) >> 3;

	return 0xffff;
}

// FUNCTION: XWA 0x4A2CC0
void pai_ObjectRefDirectionToObjectRef(unsigned int fromRef, unsigned int toRef) {
	int targetX;
	int targetY;
	int targetZ;

	Mission_ResolveObjectOrMissionPointWorldLoc(toRef, 0, 0, 0);
	targetX = worldlocx;
	targetY = worldlocy;
	targetZ = worldlocz;

	Mission_ResolveObjectOrMissionPointWorldLoc(fromRef, 0, 0, 0);
	trig2_ctop(targetX - worldlocx, targetY - worldlocy, targetZ - worldlocz);
}

// FUNCTION: XWA 0x4A2D30
void pai_ObjectRefUpdateApproxRangeScore(unsigned int fromRef, unsigned int toRef) {
	int deltaX;
	int deltaY;
	int deltaZ;
	unsigned int xyScore;

	Mission_ResolveObjectOrMissionPointWorldLoc(fromRef, 0, 0, 0);
	deltaX = worldlocx;
	deltaY = worldlocy;
	deltaZ = worldlocz;

	Mission_ResolveObjectOrMissionPointWorldLoc(toRef, 0, 0, 0);
	deltaX -= worldlocx;
	deltaY -= worldlocy;
	deltaZ -= worldlocz;

	if (deltaX < 0) {
		deltaX = -deltaX;
	}
	if (deltaY < 0) {
		deltaY = -deltaY;
	}
	if (deltaZ < 0) {
		deltaZ = -deltaZ;
	}

	if (deltaX > deltaY) {
		xyScore = (unsigned int)(deltaX + (deltaY >> 1));
	} else {
		xyScore = (unsigned int)(deltaY + (deltaX >> 1));
	}

	if (xyScore > (unsigned int)deltaZ) {
		deltaZ >>= 1;
	} else {
		xyScore >>= 1;
	}
	g_targetRangeScore = (int)(xyScore + (unsigned int)deltaZ);
}

// FUNCTION: XWA 0x4A3660
void pai_CalcAnglesToAimPoint(void) {
	trig2_ctop(g_paiContext.aiController->aimPointX - g_objectTable[g_paiContext.aiObjIdx].world_x,
			   g_paiContext.aiController->aimPointY - g_objectTable[g_paiContext.aiObjIdx].world_y,
			   g_paiContext.aiController->aimPointZ - g_objectTable[g_paiContext.aiObjIdx].world_z);
}

// FUNCTION: XWA 0x4A50C0
// Returns the Q16 skill magnitude as UNSIGNED, matching the original
// (unsigned __int16). Signed return makes skill >= 0x8000 negative, which turns
// the `(skill >> 1)` turn-step formulas into arithmetic shifts and roughly
// halves the AI turn rate at high skill (craft cannot pull onto a target).
uint16_t pai_GetEffectiveSkillValue(CraftData* craft) {
	ObjectRecord* linkedObject;

	linkedObject = craft->effectiveAiObjectLink;
	if (!linkedObject) {
		return craft->skillValue;
	}

	if (linkedObject->objectSignature != craft->turretAim.effectiveAiObjectSignature) {
		craft->effectiveAiObjectLink = NULL;
		return craft->skillValue;
	}

	if (linkedObject->mobj && linkedObject->mobj->pCharData) {
		return linkedObject->mobj->pCharData->skillValue;
	}

	return craft->skillValue;
}

// FUNCTION: XWA 0x4A5040
AiController* pai_GetEffectiveAIController(CraftData* craft) {
	ObjectRecord* linkedObject;
	AiController* controller;

	if (!craft) {
		return NULL;
	}

	linkedObject = craft->effectiveAiObjectLink;
	if (!linkedObject) {
		return &craft->aiController;
	}

	if (linkedObject->objectSignature != craft->turretAim.effectiveAiObjectSignature) {
		craft->effectiveAiObjectLink = NULL;
		return &craft->aiController;
	}

	if (!linkedObject->mobj) {
		return &craft->aiController;
	}

	if (linkedObject->mobj->pCharData) {
		return &linkedObject->mobj->pCharData->aiController;
	}

	if (!linkedObject->mobj->pCraft || linkedObject->mobj->pCraft->aiLinkResolving) {
		return &craft->aiController;
	}

	craft->aiLinkResolving = 1;
	controller = pai_GetEffectiveAIController(linkedObject->mobj->pCraft);
	craft->aiLinkResolving = 0;
	return controller;
}

// FUNCTION: XWA 0x4A2480
void pai_setupcraftcontext(int objIdx) {
	AiController* aiController;
	CraftData* selfCraft;
	ObjectRecord* linkedObject;
	uint16_t skillValue;

	g_paiContext.aiObjIdx = (uint32_t)objIdx;
	g_paiContext.aiSelfObjRecord = &g_objectTable[objIdx];
	g_paiContext.aiSelfMobj = g_paiContext.aiSelfObjRecord->mobj;
	g_paiContext.aiSelfCraft = g_paiContext.aiSelfObjRecord->mobj->pCraft;
	g_paiContext.aiLeaderObjIdx = g_paiContext.aiSelfCraft->leader_obj_idx;

	aiController = pai_GetEffectiveAIController(g_paiContext.aiSelfCraft);
	g_paiContext.aiController = aiController;

	if (g_paiContext.aiLeaderObjIdx == -1) {
		g_paiContext.aiTargetCraft = g_objectTable[objIdx].mobj->pCraft;
	} else {
		g_paiContext.aiTargetCraft = g_objectTable[g_paiContext.aiLeaderObjIdx].mobj->pCraft;
	}

	g_paiContext.curOrderCoord.fields.flightGroupIdx = g_objectTable[objIdx].flightGroupIdx;
	g_paiContext.curOrderCoord.fields.regionIdx = g_objectTable[objIdx].regionIdx;
	g_paiContext.curOrderCoord.fields.orderSlot = (uint8_t)aiController->currentOrderSlot;

	Mission_ResolveObjectOrMissionPointWorldLoc(g_paiContext.aiObjIdx, 0, 0, 0);
	g_paiContext.aiCurrentPointX = worldlocx;
	g_paiContext.aiCurrentPointY = worldlocy;
	g_paiContext.aiCurrentPointZ = worldlocz;

	selfCraft = g_paiContext.aiSelfCraft;
	linkedObject = selfCraft->effectiveAiObjectLink;
	if (!linkedObject) {
		skillValue = selfCraft->skillValue;
	} else if (linkedObject->objectSignature != selfCraft->turretAim.effectiveAiObjectSignature) {
		selfCraft->effectiveAiObjectLink = NULL;
		skillValue = selfCraft->skillValue;
	} else {
		MobileObject* linkedMobile;

		linkedMobile = linkedObject->mobj;
		if (linkedMobile) {
			MobileObjectCharData* linkedCharData;

			linkedCharData = linkedMobile->pCharData;
			if (linkedCharData) {
				skillValue = linkedCharData->skillValue;
			} else {
				skillValue = selfCraft->skillValue;
			}
		} else {
			skillValue = selfCraft->skillValue;
		}
	}

	if (skillValue < 0x8000u) {
		skillValue = 0;
	} else if (skillValue < 0xc000u) {
		skillValue = 1;
	} else {
		skillValue = 2;
	}
	g_paiContext.aiSkillTier = skillValue;

	g_paiContext.planCursor = g_planDataPtrs[aiController->pendingPlanId] + 1;
	g_paiContext.aiPlanInitialManeuverId = *g_paiContext.planCursor++;
	g_paiContext.aiRequireLiveOrderTarget = 0;
	g_paiContext.nullPlanId = (uint8_t)pai_findplanbyname("nullpln");
	g_paiContext.aiSelfModelUsesExpandedTargetProbe =
		(uint8_t)((g_modelTypeTable[g_objectTable[g_paiContext.aiObjIdx].objectType].flags &
				   MODEL_TYPE_FLAG_EXPANDED_TARGET_PROBE) != 0);
}

// FUNCTION: XWA 0x4A5210
bool pai_IsBoardingPlanId(uint16_t planId) {
	const char* planName;

	planName = g_planTable[planId].name;
	if (!strcmp(planName, "boardtogivepln") || !strcmp(planName, "boardtotakepln") ||
		!strcmp(planName, "boardtoexchangepln") || !strcmp(planName, "boardtocapturepln") ||
		!strcmp(planName, "boardtodestroypln") || !strcmp(planName, "boardtopickuppln") ||
		!strcmp(planName, "boardtocontactpln") || !strcmp(planName, "boardtorepairpln")) {
		return true;
	}
	return false;
}

// FUNCTION: XWA 0x4A4C70
bool pai_IsBoardingPlanCompleteForOrderSlot(uint16_t planId, uint8_t orderSlot, uint8_t regionIdx) {
	bool complete;
	bool boardingPlan;
	const char* planName;

	complete = false;
	planName = g_planTable[planId].name;
	boardingPlan = strcmp(planName, "boardtogivepln") == 0 || strcmp(planName, "boardtotakepln") == 0 ||
				   strcmp(planName, "boardtoexchangepln") == 0 ||
				   strcmp(planName, "boardtocapturepln") == 0 || strcmp(planName, "boardtodestroypln") == 0 ||
				   strcmp(planName, "boardtopickuppln") == 0 || strcmp(planName, "boardtocontactpln") == 0 ||
				   strcmp(planName, "boardtorepairpln") == 0;

	if (boardingPlan) {
		if (!paifight_OrderSlotHasRemainingTargets(orderSlot, regionIdx) &&
			!paifight_OrderSlotHasFutureTargets(orderSlot, regionIdx)) {
			complete = true;
		}
	}

	return complete;
}

// FUNCTION: XWA 0x4A4080
bool pai_IsPlanCompleteForOrderSlot(uint16_t planId, uint8_t orderSlot, uint8_t regionIdx) {
	bool complete;
	const char* planName;
	int orderIndex;
	uint8_t variable1;

	complete = false;
	planName = g_planTable[planId].name;

	if (strcmp(planName, "formldr1pln") == 0 || strcmp(planName, "formevadeldr1pln") == 0 ||
		strcmp(planName, "starshipformpln") == 0 || strcmp(planName, "rendezvous1pln") == 0 ||
		strcmp(planName, "disabledpln") == 0 || strcmp(planName, "startoverpln") == 0 ||
		strcmp(planName, "waitforboardpln") == 0) {
		orderIndex = orderSlot + 4 * regionIdx;
		variable1 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg.orders[orderIndex]
						.variable1;
		if (variable1 != 0 &&
			g_paiContext.aiController->orderScratch.goalProgress[regionIdx][orderSlot] >= variable1) {
			complete = true;
		}
	} else if (strcmp(planName, "capfreeldr1pln") != 0 && strcmp(planName, "capescortersldr1pln") != 0 &&
			   strcmp(planName, "caprespondldr1pln") != 0 && strcmp(planName, "escortldr1pln") != 0 &&
			   strcmp(planName, "disableldr1pln") != 0 && strcmp(planName, "inspectldr1pln") != 0 &&
			   strcmp(planName, "followtarget1pln") != 0 && strcmp(planName, "starshipprotectpln") != 0 &&
			   strcmp(planName, "starshipattackpln") != 0 && strcmp(planName, "starshipdisablepln") != 0) {
		if (strcmp(planName, "boardtogivepln") == 0 || strcmp(planName, "boardtotakepln") == 0 ||
			strcmp(planName, "boardtoexchangepln") == 0 || strcmp(planName, "boardtocapturepln") == 0 ||
			strcmp(planName, "boardtodestroypln") == 0 || strcmp(planName, "boardtopickuppln") == 0 ||
			strcmp(planName, "boardtocontactpln") == 0 || strcmp(planName, "boardtorepairpln") == 0) {
			if (g_paiContext.aiController->orderScratch.goalProgress[regionIdx][orderSlot] >=
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.orders[4 * regionIdx + orderSlot]
					.variable2) {
				complete = true;
			}
		} else if (strcmp(planName, "hyperspacepln") == 0) {
			if (g_paiContext.aiController->orderScratch.goalProgress[regionIdx][orderSlot] >=
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg.orders[4 * regionIdx + orderSlot]
						.variable3 +
					1) {
				complete = true;
			}
		} else if (strcmp(planName, "dropoffldr1pln") == 0) {
			if (g_paiContext.aiController->orderScratch.goalProgress[regionIdx][orderSlot] >=
				(unsigned int)
					g_missionFgStats[g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										 .fg.orders[4 * regionIdx + orderSlot]
										 .variable2]
						.outcomeCount[0]) {
				complete = true;
			}
		} else if (strcmp(planName, "waitpln") == 0) {
			if (g_paiContext.aiController->maneuverTimer == 0) {
				complete = true;
			}
		} else if (strcmp(planName, "starshipwaitreturnpln") == 0) {
			if (paiorder_waitforallreturnorder()) {
				complete = true;
			}
		} else if (strcmp(planName, "starshipwaitcreatepln") == 0) {
			if (paiorder_waitforallcreateorder()) {
				complete = true;
			}
		} else if (strcmp(planName, "transfercargopln") != 0 && strcmp(planName, "changesidespln") != 0 &&
				   strcmp(planName, "selfdestroypln") != 0) {
			if (strcmp(planName, "orbitpln") == 0) {
				orderIndex = orderSlot + 4 * regionIdx;
				variable1 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								.fg.orders[orderIndex]
								.variable1;
				if (variable1 != 0 &&
					g_paiContext.aiController->orderScratch.goalProgress[regionIdx][orderSlot] >= variable1) {
					complete = true;
				}
			} else if (strcmp(planName, "release1pln") == 0 || strcmp(planName, "deliverpln") == 0) {
				if (g_paiContext.aiController->maneuverTimer == 0 &&
					g_paiContext.aiController->orderScratch.goalProgress[regionIdx][orderSlot] != 0) {
					complete = true;
				}
			} else if (strcmp(planName, "backuppln") == 0) {
				if (g_paiContext.aiController->aiPlanState == 0) {
					complete = true;
				}
			} else if (strcmp(planName, "repaironeselfpln") == 0) {
				if (g_curCraft->workingSubsystems == g_curCraft->systemFlags &&
					g_paiContext.aiController->maneuverTimer == 0) {
					complete = true;
				}
			} else if (strcmp(planName, "park1pln") == 0) {
				if (g_paiContext.aiController->maneuverMode == 37 &&
					g_paiContext.aiController->maneuverPhase == 5) {
					complete = true;
				}
			} else if (strcmp(planName, "workon1pln") == 0 &&
					   g_paiContext.aiController->orderScratch.goalProgress[regionIdx][orderSlot] >=
						   g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							   .fg.orders[4 * regionIdx + orderSlot]
							   .variable3) {
				complete = true;
			}
		} else {
			complete = true;
		}
	} else if (!paifight_OrderSlotHasRemainingTargets(orderSlot, regionIdx) &&
			   !paifight_OrderSlotHasFutureTargets(orderSlot, regionIdx)) {
		complete = true;
	}

	return complete;
}

// FUNCTION: XWA 0x4A2640
int16_t pai_FindMothershipObject(int16_t mothershipFlightGroupIdx) {
	uint32_t objIdx;
	CraftData* craft;
	uint8_t objectKind;

	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
		if (g_objectTable[objIdx].objectType != 0) {
			craft = g_objectTable[objIdx].mobj->pCraft;
			objectKind = craft->objectKind;
			if (objectKind != 4 && objectKind != 3 &&
				(uint16_t)g_objectTable[objIdx].flightGroupIdx == (uint16_t)mothershipFlightGroupIdx &&
				craft->leader_obj_idx == -1) {
				return (int16_t)objIdx;
			}
		}
	}

	return (int16_t)(objIdx | 0xffffu);
}

int16_t pai_FindMothershipObjectAnyRegion(int16_t mothershipFlightGroupIdx) {
	uint32_t regionIdx;
	uint32_t regionSlotBase;
	uint32_t craftSlotsPerRegion;
	uint32_t regionSlotsPerRegion;

	if (g_activeMissionRegionCount == 0) {
		return -1;
	}

	regionSlotBase = 0;
	craftSlotsPerRegion = g_craftObjectSlotsTotal / (uint32_t)g_missionRegionCount;
	regionSlotsPerRegion = g_regionObjectSlotEnd / (uint32_t)g_missionRegionCount;

	for (regionIdx = 0; regionIdx < (uint32_t)g_activeMissionRegionCount; ++regionIdx) {
		uint32_t objIdx;
		uint32_t regionCraftEnd;
		ObjectRecord* obj;

		objIdx = regionSlotBase;
		regionCraftEnd = regionSlotBase + craftSlotsPerRegion;
		obj = &g_objectTable[objIdx];
		while (objIdx < regionCraftEnd) {
			if (obj->objectType != 0) {
				CraftData* craft;
				uint8_t objectKind;

				craft = obj->mobj->pCraft;
				objectKind = craft->objectKind;
				if (objectKind != GENUS_Starship && objectKind != GENUS_Freighter &&
					(uint16_t)obj->flightGroupIdx == (uint16_t)mothershipFlightGroupIdx &&
					craft->leader_obj_idx == -1) {
					return (int16_t)objIdx;
				}
			}

			++objIdx;
			++obj;
		}

		regionSlotBase += regionSlotsPerRegion;
	}

	return -1;
}

// The order address is derived from g_paiContext, which the calls below may
// mutate, so the original re-reads it for each target rather than caching a
// pointer. Target pairs are combined bitwise (the matcher returns 0/1), and the
// match flags and result are 16-bit, matching the original codegen.
#define PAI_CUR_ORDER                                                                                        \
	(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]                                 \
		 .fg.orders[g_paiContext.curOrderCoord.fields.orderSlot +                                            \
					4 * g_paiContext.curOrderCoord.fields.regionIdx])

// FUNCTION: XWA 0x4A4E80
int16_t pai_CurrentOrderTargetsMatchObject(uint16_t objIdx) {
	int16_t matchesTarget1;
	int16_t matchesTarget2;
	int16_t matchesTarget3;
	int16_t matchesTarget4;

	matchesTarget1 = Mission_ObjectMatchesTriggerVariable(
		objIdx, (MissionTriggerVariableType)PAI_CUR_ORDER.target1Type, PAI_CUR_ORDER.target1);
	matchesTarget2 = Mission_ObjectMatchesTriggerVariable(
		objIdx, (MissionTriggerVariableType)PAI_CUR_ORDER.target2Type, PAI_CUR_ORDER.target2);
	if (PAI_CUR_ORDER.target1OrTarget2 == 1) {
		matchesTarget1 |= matchesTarget2;
	} else {
		matchesTarget1 &= matchesTarget2;
	}

	matchesTarget3 = Mission_ObjectMatchesTriggerVariable(
		objIdx, (MissionTriggerVariableType)PAI_CUR_ORDER.secondaryTargetTypes[XWA_ORDER_TARGET_3],
		PAI_CUR_ORDER.secondaryTargets[XWA_ORDER_TARGET_3]);
	matchesTarget4 = Mission_ObjectMatchesTriggerVariable(
		objIdx, (MissionTriggerVariableType)PAI_CUR_ORDER.secondaryTargetTypes[XWA_ORDER_TARGET_4],
		PAI_CUR_ORDER.secondaryTargets[XWA_ORDER_TARGET_4]);
	if (PAI_CUR_ORDER.target3OrTarget4 == 1) {
		matchesTarget3 |= matchesTarget4;
	} else {
		matchesTarget3 &= matchesTarget4;
	}

	if (matchesTarget1 || matchesTarget3) {
		return 1;
	}
	return 0;
}
#undef PAI_CUR_ORDER

// FUNCTION: XWA 0x4A5110
int pai_OrderSlotMatchingObjectHasOrderClass(int sourceObjIdx, int orderClass, uint16_t targetObjIdx) {
	uint8_t regionIdx;
	uint8_t orderSlot;

	if (sourceObjIdx == 0xffff) {
		return 0;
	}

	pai_setupcraftcontext(sourceObjIdx);

	for (regionIdx = 0; regionIdx < (uint32_t)g_activeMissionRegionCount; ++regionIdx) {
		for (orderSlot = 0; orderSlot < 3u; ++orderSlot) {
			if (g_orderLeaderBuiltinPlanNameIndex
					[g_missionFlightGroups[g_objectTable[sourceObjIdx].flightGroupIdx]
						 .fg.orders[orderSlot + 4 * regionIdx]
						 .order] == orderClass) {
				g_paiContext.curOrderCoord.fields.orderSlot = orderSlot;
				g_paiContext.curOrderCoord.fields.regionIdx = regionIdx;
				if (pai_CurrentOrderTargetsMatchObject(targetObjIdx)) {
					return 1;
				}
			}
		}
	}

	return 0;
}

bool pai_IsStaticBoardingPlanId(uint16_t planId) {
	const char* planName;

	planName = g_planTable[planId].name;
	return strcmp(planName, "boardtogivepln") == 0 || strcmp(planName, "boardtotakepln") == 0 ||
		   strcmp(planName, "boardtoexchangepln") == 0 || strcmp(planName, "boardtocapturepln") == 0 ||
		   strcmp(planName, "boardtodestroypln") == 0 || strcmp(planName, "boardtocontactpln") == 0 ||
		   strcmp(planName, "boardtorepairpln") == 0;
}

// FUNCTION: XWA 0x4A36B0
int16_t pai_FindNearestBoardingTarget(uint16_t target1Type, uint16_t target1, int16_t targetOrMode,
									  uint16_t target2Type, uint16_t target2) {
	uint16_t objectIdx;
	uint32_t bestRangeScore;
	uint16_t bestObjIdx;

	objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
	bestObjIdx = 0xffffu;
	bestRangeScore = UINT32_MAX;
	while ((uint32_t)objectIdx < g_activeRegionCraftObjectSlotEnd) {
		if (g_objectTable[objectIdx].objectType != OBJ_None) {
			int matchesTarget1;
			int matchesTarget2;

			matchesTarget1 = Mission_ObjectMatchesTriggerVariable(objectIdx, target1Type, target1);
			matchesTarget2 = Mission_ObjectMatchesTriggerVariable(objectIdx, target2Type, target2);
			if (targetOrMode == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if ((int16_t)matchesTarget1 != 0) {
				CraftData* craft;
				AiController* ai;
				const char* aiPlanName;
				const char* selfPlanName;
				ModelGenusId genusId;
				int16_t eligible;

				craft = g_objectTable[objectIdx].mobj->pCraft;
				ai = pai_GetEffectiveAIController(craft);
				aiPlanName = g_planTable[ai->currentPlanId].name;
				eligible = 0;

				if (strcmp(aiPlanName, "nullpln") == 0 || strcmp(aiPlanName, "stationaryldrpln") == 0 ||
					strcmp(aiPlanName, "stationaryflwpln") == 0) {
					eligible = 1;
				} else {
					genusId = g_objectTable[objectIdx].genusId;
					if (genusId == GENUS_WeaponEmplacement || genusId == GENUS_Container ||
						genusId == GENUS_Platform) {
						eligible = 1;
					} else {
						selfPlanName = g_planTable[g_paiContext.aiController->currentPlanId].name;
						if (strcmp(selfPlanName, "boardtocapturepln") == 0 ||
							strcmp(selfPlanName, "boardtodestroypln") == 0) {
							if (craft->workingSubsystems == 0) {
								eligible = 1;
							}
						} else if (craft->workingSubsystems == 0) {
							eligible = 1;
						} else if (ai->maneuverMode == 19u || ai->maneuverMode == 25u) {
							eligible = 1;
						} else if (strcmp(selfPlanName, "workon1pln") == 0) {
							if (g_objectTable[objectIdx].mobj->speed == 0) {
								eligible = 1;
							}
						}
					}
				}

				if (eligible) {
					uint16_t claimObjIdx;
					int claimCount;

					claimObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
					claimCount = 0;
					while ((uint32_t)claimObjIdx < g_activeRegionCraftObjectSlotEnd) {
						if (g_objectTable[claimObjIdx].objectType != OBJ_None &&
							claimObjIdx != g_paiContext.aiObjIdx) {
							CraftData* claimCraft;
							AiController* claimAi;
							const char* planName;

							claimCraft = g_objectTable[claimObjIdx].mobj->pCraft;
							claimAi = pai_GetEffectiveAIController(claimCraft);
							planName = g_planTable[claimAi->currentPlanId].name;
							if (strcmp(planName, "boardtogivepln") == 0 ||
								strcmp(planName, "boardtotakepln") == 0 ||
								strcmp(planName, "boardtoexchangepln") == 0 ||
								strcmp(planName, "boardtocapturepln") == 0 ||
								strcmp(planName, "boardtodestroypln") == 0 ||
								strcmp(planName, "boardtopickuppln") == 0 ||
								strcmp(planName, "boardtocontactpln") == 0 ||
								strcmp(planName, "boardtorepairpln") == 0) {
								if (claimAi->targetObjIdx == objectIdx ||
									claimCraft->carriedObjectIndex == objectIdx) {
									++claimCount;
								}
							}
						}
						++claimObjIdx;
					}

					{
						uint16_t signatureCount;
						uint16_t signatureIdx;

						signatureCount = g_curCraft->aiFlight.objSignatureCount;
						if (signatureCount > 0) {
							for (signatureIdx = 0; signatureIdx < signatureCount; ++signatureIdx) {
								if (g_objectTable[objectIdx].objectSignature ==
									g_curCraft->aiFlight.objSignatures[signatureIdx]) {
									++claimCount;
								}
							}
						}
					}

					if ((int16_t)claimCount == 0) {
						int selfX;
						int selfY;
						int selfZ;
						int deltaX;
						int deltaY;
						int deltaZ;
						unsigned int xyScore;
						uint32_t rangeScore;

						Mission_ResolveObjectOrMissionPointWorldLoc(g_paiContext.aiObjIdx, 0, 0, 0);
						selfX = worldlocx;
						selfY = worldlocy;
						selfZ = worldlocz;

						Mission_ResolveObjectOrMissionPointWorldLoc(objectIdx, 0, 0, 0);
						deltaX = selfX - worldlocx;
						deltaY = selfY - worldlocy;
						deltaZ = selfZ - worldlocz;

						if (deltaX < 0) {
							deltaX = -deltaX;
						}
						if (deltaY < 0) {
							deltaY = -deltaY;
						}
						if (deltaZ < 0) {
							deltaZ = -deltaZ;
						}

						if (deltaX > deltaY) {
							xyScore = (unsigned int)(deltaX + (deltaY >> 1));
						} else {
							xyScore = (unsigned int)(deltaY + (deltaX >> 1));
						}
						if (xyScore > (unsigned int)deltaZ) {
							deltaZ >>= 1;
						} else {
							xyScore >>= 1;
						}
						xyScore += (unsigned int)deltaZ;
						g_targetRangeScore = (int)xyScore;

						rangeScore = xyScore;
						if (rangeScore < bestRangeScore) {
							bestRangeScore = rangeScore;
							bestObjIdx = objectIdx;
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

			matchesTarget1 = Mission_FlightGroupMatchesTriggerVariable(
				g_objectTable[objectIdx].flightGroupIdx, target1Type, target1);
			matchesTarget2 = Mission_FlightGroupMatchesTriggerVariable(
				g_objectTable[objectIdx].flightGroupIdx, target2Type, target2);
			if (targetOrMode == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if ((int16_t)matchesTarget1 != 0) {
				uint16_t claimObjIdx;
				int claimCount;

				claimObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
				claimCount = 0;
				while ((uint32_t)claimObjIdx < g_activeRegionCraftObjectSlotEnd) {
					if (g_objectTable[claimObjIdx].objectType != OBJ_None &&
						claimObjIdx != g_paiContext.aiObjIdx) {
						CraftData* claimCraft;
						AiController* claimAi;
						const char* planName;

						claimCraft = g_objectTable[claimObjIdx].mobj->pCraft;
						claimAi = pai_GetEffectiveAIController(claimCraft);
						planName = g_planTable[claimAi->currentPlanId].name;
						if (strcmp(planName, "boardtogivepln") == 0 ||
							strcmp(planName, "boardtotakepln") == 0 ||
							strcmp(planName, "boardtoexchangepln") == 0 ||
							strcmp(planName, "boardtocapturepln") == 0 ||
							strcmp(planName, "boardtodestroypln") == 0 ||
							strcmp(planName, "boardtocontactpln") == 0 ||
							strcmp(planName, "boardtorepairpln") == 0) {
							if (claimAi->targetObjIdx == objectIdx) {
								++claimCount;
							}
						}
					}
					++claimObjIdx;
				}

				{
					uint16_t signatureCount;
					uint16_t signatureIdx;

					signatureCount = g_curCraft->aiFlight.objSignatureCount;
					if (signatureCount > 0) {
						for (signatureIdx = 0; signatureIdx < signatureCount; ++signatureIdx) {
							if (g_objectTable[objectIdx].objectSignature ==
								g_curCraft->aiFlight.objSignatures[signatureIdx]) {
								++claimCount;
							}
						}
					}
				}

				if ((int16_t)claimCount == 0) {
					int selfX;
					int selfY;
					int selfZ;
					int deltaX;
					int deltaY;
					int deltaZ;
					unsigned int xyScore;
					uint32_t rangeScore;

					Mission_ResolveObjectOrMissionPointWorldLoc(g_paiContext.aiObjIdx, 0, 0, 0);
					selfX = worldlocx;
					selfY = worldlocy;
					selfZ = worldlocz;

					Mission_ResolveObjectOrMissionPointWorldLoc(objectIdx, 0, 0, 0);
					deltaX = selfX - worldlocx;
					deltaY = selfY - worldlocy;
					deltaZ = selfZ - worldlocz;

					if (deltaX < 0) {
						deltaX = -deltaX;
					}
					if (deltaY < 0) {
						deltaY = -deltaY;
					}
					if (deltaZ < 0) {
						deltaZ = -deltaZ;
					}

					if (deltaX > deltaY) {
						xyScore = (unsigned int)(deltaX + (deltaY >> 1));
					} else {
						xyScore = (unsigned int)(deltaY + (deltaX >> 1));
					}
					if (xyScore > (unsigned int)deltaZ) {
						deltaZ >>= 1;
					} else {
						xyScore >>= 1;
					}
					xyScore += (unsigned int)deltaZ;
					g_targetRangeScore = (int)xyScore;

					rangeScore = xyScore;
					if (rangeScore < bestRangeScore) {
						bestRangeScore = rangeScore;
						bestObjIdx = objectIdx;
					}
				}
			}
		}

		++objectIdx;
	}

	return (int16_t)bestObjIdx;
}

// FUNCTION: XWA 0x4A2A80
int16_t pai_FindBoardingTargetFromOrder(uint8_t orderSlot, uint8_t regionIdx) {
	int orderIndex;
	uint16_t target1Type;
	uint16_t target1;
	int16_t targetOrMode;
	uint16_t target2Type;
	uint16_t target2;
	int16_t targetObjIdx;

	orderIndex = orderSlot + 4 * regionIdx;
	target2 =
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target2;
	target2Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .target2Type;
	targetOrMode = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					   .fg.orders[orderIndex]
					   .target1OrTarget2;
	target1 =
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.orders[orderIndex].target1;
	target1Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .target1Type;
	targetObjIdx = pai_FindNearestBoardingTarget(target1Type, target1, targetOrMode, target2Type, target2);
	if (targetObjIdx == -1) {
		target2 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .secondaryTargets[XWA_ORDER_TARGET_4];
		target2Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						  .fg.orders[orderIndex]
						  .secondaryTargetTypes[XWA_ORDER_TARGET_4];
		targetOrMode = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						   .fg.orders[orderIndex]
						   .target3OrTarget4;
		target1 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					  .fg.orders[orderIndex]
					  .secondaryTargets[XWA_ORDER_TARGET_3];
		target1Type = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						  .fg.orders[orderIndex]
						  .secondaryTargetTypes[XWA_ORDER_TARGET_3];
		targetObjIdx =
			pai_FindNearestBoardingTarget(target1Type, target1, targetOrMode, target2Type, target2);
	}

	return targetObjIdx;
}

// FUNCTION: XWA 0x4A2A60
uint16_t pai_OrderSlotCanBoardTarget(uint8_t orderSlot, uint8_t regionIdx) {
	return pai_FindBoardingTargetFromOrder(orderSlot, regionIdx) != -1;
}

// FUNCTION: XWA 0x4A27A0
int pai_IsObjectTargetable(unsigned int objIdx) {
	CraftData* craft;
	MobileObject* mobj;
	AiController* aiController;
	uint16_t objectType;
	uint16_t objectKind;

	if (objIdx == 0xffffu) {
		return 0;
	}

	mobj = g_objectTable[objIdx].mobj;
	if (mobj != NULL) {
		if (g_objectTable[objIdx].objectType == OBJ_None) {
			return 0;
		}

		craft = mobj->pCraft;
		if (craft != NULL) {
			aiController = pai_GetEffectiveAIController(craft);
			if (aiController->maneuverMode == 21 && aiController->maneuverPhase != 0) {
				return 0;
			}

			objectKind = craft->objectKind;
			if (objectKind == 1 || objectKind == 3 || objectKind == 4) {
				return 0;
			}

			if (craft->beamTypeId == 3 && craft->beamActive != 0) {
				unsigned int sourceObjIdx;
				int targetX;
				int targetY;
				int targetZ;
				int deltaX;
				int deltaY;
				int deltaZ;
				unsigned int xyScore;
				ModelGenusId sourceGenus;

				sourceObjIdx = g_paiContext.aiObjIdx;
				Mission_ResolveObjectOrMissionPointWorldLoc(objIdx, 0, 0, 0);
				targetX = worldlocx;
				targetY = worldlocy;
				targetZ = worldlocz;

				Mission_ResolveObjectOrMissionPointWorldLoc(sourceObjIdx, 0, 0, 0);
				deltaX = targetX - worldlocx;
				deltaY = targetY - worldlocy;
				deltaZ = targetZ - worldlocz;
				if (deltaX < 0) {
					deltaX = -deltaX;
				}
				if (deltaY < 0) {
					deltaY = -deltaY;
				}
				if (deltaZ < 0) {
					deltaZ = -deltaZ;
				}

				if (deltaX > deltaY) {
					xyScore = (unsigned int)(deltaX + (deltaY >> 1));
				} else {
					xyScore = (unsigned int)(deltaY + (deltaX >> 1));
				}

				if (xyScore > (unsigned int)deltaZ) {
					deltaZ >>= 1;
				} else {
					xyScore >>= 1;
				}
				xyScore += (unsigned int)deltaZ;

				g_targetRangeScore = (int)xyScore;
				sourceGenus = g_objectTable[g_paiContext.aiObjIdx].genusId;
				if (sourceGenus == GENUS_Fighter || sourceGenus == GENUS_Transport ||
					sourceGenus == GENUS_PilotDroid || sourceGenus == GENUS_WeaponEmplacement ||
					sourceGenus == GENUS_Utility) {
					if (xyScore > 0x8000u) {
						return 0;
					}
				} else if (xyScore > 0x10000u) {
					return 0;
				}
			}

			objectType = g_objectTable[objIdx].objectType;
			if (objectType == OBJ_RebelPilot || objectType == OBJ_ImperialPilot ||
				objectType == OBJ_CivilianPilot) {
				return 0;
			}

			if (g_objectTable[objIdx].playerOwnerIdx != -1) {
				return 1;
			}

			return (unsigned int)craft->hullDamage <=
				   (unsigned int)g_objectTable[objIdx].mobj->pCraft->hullMax;
		}

		return 1;
	}

	return g_objectTable[objIdx].objectType != OBJ_None;
}

// FUNCTION: XWA 0x4A26B0
int pai_IsObjectTargetableNearCurrentPoint(int unused, unsigned int objIdx, int expandRange) {
	int maxRangeScore;
	ObjectRecord* object;
	int deltaX;
	int deltaY;
	int deltaZ;
	unsigned int xyScore;
	unsigned int rangeScore;
	uint8_t inRange;

	(void)unused;

	if (pai_IsObjectTargetable(objIdx)) {
		maxRangeScore =
			(uint16_t)MATH2_fraction(0x500u, g_aiSkillValueQ16ByLevel[g_paiContext.aiSkillTier]) + 2560;
		if (expandRange) {
			maxRangeScore += (uint16_t)MATH2_fraction((uint16_t)maxRangeScore, 0x5555u);
		}
		maxRangeScore <<= 8;

		object = &g_objectTable[objIdx];
		deltaX = g_paiContext.aiCurrentPointX;
		deltaY = g_paiContext.aiCurrentPointY;
		deltaZ = g_paiContext.aiCurrentPointZ;
		deltaX -= object->world_x;
		deltaY -= object->world_y;
		deltaZ -= object->world_z;
		if (deltaX < 0) {
			deltaX = -deltaX;
		}
		if (deltaY < 0) {
			deltaY = -deltaY;
		}
		if (deltaZ < 0) {
			deltaZ = -deltaZ;
		}

		if (deltaX > deltaY) {
			xyScore = (unsigned int)(deltaX + (deltaY >> 1));
		} else {
			xyScore = (unsigned int)(deltaY + (deltaX >> 1));
		}

		if (xyScore > (unsigned int)deltaZ) {
			deltaZ >>= 1;
		} else {
			xyScore >>= 1;
		}
		rangeScore = xyScore + (unsigned int)deltaZ;

		g_targetRangeScore = (int)rangeScore;
		inRange = (unsigned int)maxRangeScore > rangeScore;
		if (inRange == 1) {
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4A2B50
bool __inline pai_IsObjectWithinCurrentPointRange(unsigned int objIdx, unsigned int maxRangeScore) {
	ObjectRecord* object;
	int deltaX;
	int deltaY;
	int deltaZ;
	unsigned int xyScore;

	object = &g_objectTable[objIdx];
	deltaX = g_paiContext.aiCurrentPointX - object->world_x;
	deltaY = g_paiContext.aiCurrentPointY - object->world_y;
	deltaZ = g_paiContext.aiCurrentPointZ - object->world_z;

	if (deltaX < 0) {
		deltaX = -deltaX;
	}
	if (deltaY < 0) {
		deltaY = -deltaY;
	}
	if (deltaZ < 0) {
		deltaZ = -deltaZ;
	}

	if (deltaX > deltaY) {
		xyScore = (unsigned int)(deltaX + (deltaY >> 1));
	} else {
		xyScore = (unsigned int)(deltaY + (deltaX >> 1));
	}

	if (xyScore > (unsigned int)deltaZ) {
		g_targetRangeScore = (int)(xyScore + ((unsigned int)deltaZ >> 1));
	} else {
		g_targetRangeScore = deltaZ + (int)(xyScore >> 1);
	}

	return maxRangeScore > (unsigned int)g_targetRangeScore;
}

// FUNCTION: XWA 0x4A29A0
uint8_t pai_IsObjectWithinCurrentOrderRange(uint16_t objIdx) {
	unsigned int maxRangeScore;
	int deltaX;
	int deltaZ;
	int deltaY;
	unsigned int xyScore;

	maxRangeScore =
		((uint16_t)MATH2_fraction(0x500u, g_aiSkillValueQ16ByLevel[g_paiContext.aiSkillTier]) + 2560u) << 8;
	deltaX = g_paiContext.aiCurrentPointX - g_objectTable[objIdx].world_x;
	deltaY = g_paiContext.aiCurrentPointY - g_objectTable[objIdx].world_y;
	deltaZ = g_paiContext.aiCurrentPointZ - g_objectTable[objIdx].world_z;

	if (deltaX < 0) {
		deltaX = -deltaX;
	}
	if (deltaY < 0) {
		deltaY = -deltaY;
	}
	if (deltaZ < 0) {
		deltaZ = -deltaZ;
	}

	if (deltaX > deltaY) {
		xyScore = (unsigned int)(deltaX + (deltaY >> 1));
	} else {
		xyScore = (unsigned int)(deltaY + (deltaX >> 1));
	}

	if (xyScore > (unsigned int)deltaZ) {
		deltaZ >>= 1;
	} else {
		xyScore >>= 1;
	}
	g_targetRangeScore = (int)(xyScore + (unsigned int)deltaZ);
	if (maxRangeScore > (unsigned int)g_targetRangeScore) {
		return 1;
	}
	return 0;
}
