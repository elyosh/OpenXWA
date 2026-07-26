#include "xwa/flight/object/object.h"

#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/object/damage.h"
#include "xwa/flight/object/debris.h"
#include "xwa/flight/flight.h"
#include "xwa/audio/fsfx.h"
#include "xwa/input/dinput.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/math/fixed.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/util/debug.h"
#include "xwa/util/random.h"
#include "xwa/util/time.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

// GLOBAL: XWA 0x5A9AB0
const float g_explosionCloudSpacingNumerator = 2.0f;
// GLOBAL: XWA 0x5A9300
const float g_randomSpinAxisZero = 0.0f;
// GLOBAL: XWA 0x5A9328
const float g_randomSpinAxisScale = 0.000030517578f;

// GLOBAL: XWA 0x7CA3B4
uint32_t g_objScanStart;
// GLOBAL: XWA 0x8B94D0
uint32_t g_localTransientSlotStart;
// GLOBAL: XWA 0x7B33B4
uint32_t g_localTransientSlotEnd;
// GLOBAL: XWA 0x8BF378
uint32_t g_activeRegionObjectSlotStart;
// GLOBAL: XWA 0x7CA3B8
uint32_t g_activeRegionCraftObjectSlotEnd;
// GLOBAL: XWA 0x917E64
uint32_t g_regionStaticObjectSlotEnd;
// GLOBAL: XWA 0x7D5240
uint32_t g_regionObjectSlotEnd;
// GLOBAL: XWA 0x80B610
uint32_t g_objectSlotsPerRegion;
// GLOBAL: XWA 0x917E40
uint32_t g_regionMainObjectSlotStart;
// GLOBAL: XWA 0x8BF380
uint32_t g_regionMainObjectSlotEnd;
// GLOBAL: XWA 0x91ACA0
uint32_t g_regionMainObjectSlotsTotal;
// GLOBAL: XWA 0x8BF370
uint32_t g_regionStaticObjectSlotsTotal;
// GLOBAL: XWA 0x7FFD80
uint32_t g_objectTableSlotCount;
// GLOBAL: XWA 0x8B94C8
uint32_t g_mainObjectSlotsPerRegion;
// GLOBAL: XWA 0x8C1CE4
uint32_t g_craftObjectSlotsTotal;
// GLOBAL: XWA 0x7D4B94
uint32_t g_craftObjectSlotsPerRegion;
// GLOBAL: XWA 0x7CA3AC
uint32_t g_projectileObjectSlotsTotal;
// GLOBAL: XWA 0x91AB7C
uint32_t g_mobileObjectCharDataCount;
// GLOBAL: XWA 0x917E58
uint32_t g_localDebrisObjectSlotsTotal;
// GLOBAL: XWA 0x91AB78
uint32_t g_localEffectObjectSlotsTotal;
// GLOBAL: XWA 0x8D9628
uint32_t g_projectileObjectSlotStart;
// GLOBAL: XWA 0x8BF368
uint32_t g_projectileObjectSlotEnd;
// GLOBAL: XWA 0x8052AC
uint32_t g_projectileObjectSlotsPerRegion;
// GLOBAL: XWA 0x7D4C58
uint32_t g_sharedPlayerProjectileSlotsPerRegion;
// GLOBAL: XWA 0x7D4B80
uint32_t g_playerProjectileSlotsTotal;
// GLOBAL: XWA 0x80B618
uint32_t g_salvageJunkObjectSlotStart;
// GLOBAL: XWA 0x8D4440
uint32_t g_salvageJunkObjectSlotEnd;
// GLOBAL: XWA 0x8BF35C
uint32_t g_salvageJunkObjectSlotsTotal;
// GLOBAL: XWA 0x917E50
uint32_t g_salvageJunkObjectSlotsPerRegion;
// GLOBAL: XWA 0x8C1CCC
uint32_t g_debrisObjectSlotStart;
// GLOBAL: XWA 0x808148
uint32_t g_debrisObjectSlotEnd;
// GLOBAL: XWA 0x910E08
uint32_t g_debrisObjectSlotsTotal;
// GLOBAL: XWA 0x8BF398
uint32_t g_explosionObjectSlotStart;
// GLOBAL: XWA 0x8C1CE0
uint32_t g_explosionObjectSlotEnd;
// GLOBAL: XWA 0x910DF0
uint32_t g_explosionObjectSlotsTotal;
// GLOBAL: XWA 0x8D42B0
uint32_t g_localDebrisSlotEnd;
// GLOBAL: XWA 0x910E0C
uint32_t g_mobileObjectCharDataSlotStart;
// GLOBAL: XWA 0x8B8E04
uint32_t g_mobileObjectCharDataSlotEnd;
// GLOBAL: XWA 0x917E48
uint32_t g_localEffectSlotStart;
// GLOBAL: XWA 0x7D4B66
uint16_t g_localDebrisRecycleSlotCursor;
// GLOBAL: XWA 0x91B400
ObjectSlotRange g_objectSlotRangeByGenus[22];
// GLOBAL: XWA 0x8052A0
MobileObject* g_mobileObjectPoolBase;
// GLOBAL: XWA 0x9106A0
CraftData* g_craftDataPoolBase;
// GLOBAL: XWA 0x7D4C54
WarheadGuidanceState* g_warheadGuidancePoolBase;
// GLOBAL: XWA 0x782820
MobileObjectCharData* g_mobileObjectCharDataPool;
// GLOBAL: XWA 0x805404
uint16_t g_nextObjectSignature;

// FUNCTION: XWA 0x41E960
uint16_t Object_FindFreeMissionSlot(void) {
	uint32_t slot;

	for (slot = g_objScanStart; slot < g_regionStaticObjectSlotEnd; ++slot) {
		if (g_objectTable[slot].objectType == 0) {
			if (g_flightPlayerCount > 1 || g_filmRecording || g_filmPlaybackMode) {
				g_objectTable[slot].objectSignature = 1;
			}

			return (uint16_t)slot;
		}
	}

	return 0xffffu;
}

// FUNCTION: XWA 0x41E720
uint16_t Object_AllocSlotForGenus(uint16_t genusId) {
	uint16_t rangeEnd;
	uint16_t candidateObjIdx;

	rangeEnd = (uint16_t)g_objectSlotRangeByGenus[genusId].end;
	candidateObjIdx = (uint16_t)g_objectSlotRangeByGenus[genusId].next;

	while (candidateObjIdx < rangeEnd) {
		if (g_objectTable[candidateObjIdx].objectType == 0) {
			g_objectTable[candidateObjIdx].mobj->sourceObjIdx = -1;
			g_objectTable[candidateObjIdx].mobj->instanceExtent = 0;
			break;
		}
		++candidateObjIdx;
	}

	if (candidateObjIdx < rangeEnd) {
		if (g_flightPlayerCount > 1 || g_filmRecording || g_filmPlaybackMode) {
			g_objectTable[candidateObjIdx].objectSignature = 1;
		}

		collide_ResetObjectProximityForSlot(candidateObjIdx);
		return candidateObjIdx;
	}

	return 0xffffu;
}

// FUNCTION: XWA 0x404290
int FlightObject_SpawnEscapePodOrPilot(int sourceObjIdx) {
	CraftData* craft;
	ObjectTypeId objectType;
	uint16_t objectTypeId;
	int objectGenus;
	int objIdx;
	int16_t fgIdx;
	ModelIndex modelIndex;
	int i;
	uint16_t yawDelta;
	uint16_t pitchDelta;

	if (g_flightSimSideEffectsSuppressed) {
		return 0xffffu;
	}
	if (g_flightPlayerCount > 1) {
		return 0xffffu;
	}

	if (g_objectTable[sourceObjIdx].playerOwnerIdx != -1) {
		return 0xffffu;
	}

	if (g_objectTable[sourceObjIdx].genusId == 0) {
		int sourceIff;

		if (!g_flightConfNoPilot) {
			sourceIff = (uint8_t)g_objectTable[sourceObjIdx].mobj->iff;
			switch (sourceIff) {
				case 0:
					objectType = OBJ_RebelPilot;
					break;
				case 1:
					objectType = OBJ_ImperialPilot;
					break;
				default:
					objectType = OBJ_CivilianPilot;
					break;
			}
			objectGenus = GENUS_PilotDroid;
		} else {
			return 0xffffu;
		}
	} else {
		objectType = (ObjectTypeId)((GameRand() & 1u) + OBJ_EscapePod);
		objectGenus = GENUS_Utility;
		DebugPrintfChannel(512, "Ejecting escape pod..\n");
	}

	if ((int16_t)g_missionHeader.numFlightGroups + 2 >= 192) {
		return 0xffffu;
	}

	objectTypeId = (uint16_t)objectType;
	objIdx = Object_AllocSlotForGenus(g_modelTypeTable[objectTypeId].genusId);
	if (objIdx == 0xffffu) {
		return 0xffffu;
	}

	fgIdx = g_escapePodPilotFlightGroupIdx;
	if (fgIdx == -1) {
		int i;

		fgIdx = g_escapePodPilotFlightGroupIdx = g_missionHeader.numFlightGroups + 2;
		memset(&g_missionFlightGroups[fgIdx], 0, sizeof(g_missionFlightGroups[fgIdx]));
		g_missionFlightGroups[fgIdx].fg.cargo[0] = 0;
		g_missionFlightGroups[fgIdx].fg.specialCargo[0] = 0;
		g_missionFlightGroups[fgIdx].fg.globalCargoIndex = 0xffu;
		g_missionFlightGroups[fgIdx].fg.globalSpecialCargoIndex = 0xffu;
		g_missionFlightGroups[fgIdx].playerOwnerIdx = -1;
		g_missionFlightGroups[fgIdx].fg.iff =
			g_missionFlightGroups[g_objectTable[sourceObjIdx].flightGroupIdx].fg.iff;
		g_missionFlightGroups[fgIdx].fg.team =
			g_missionFlightGroups[g_objectTable[sourceObjIdx].flightGroupIdx].fg.team;
		for (i = 0; i < XWA_CRAFT_TYPE_REVERSE_SEARCH_COUNT; ++i) {
			if (g_objectTypeTables.craftTypeToObjectType[i] == objectTypeId) {
				g_missionFlightGroups[fgIdx].fg.craftType = (uint8_t)i;
				break;
			}
		}
		g_missionFlightGroups[fgIdx].fg.departMethod = 0;
		g_missionFlightGroups[fgIdx].fg.missionPoints[XWA_FG_POINT_HYPER].enabled = 1;
		if (i == XWA_CRAFT_TYPE_REVERSE_SEARCH_COUNT) {
			g_missionFlightGroups[fgIdx].fg.craftType = 0;
		}
	}

	++g_missionFlightGroups[fgIdx].fg.numberOfCraft;

	modelIndex = (ModelIndex)GetModelIndexFromType(objectTypeId);

	g_objectTable[objIdx].flightGroupIdx = (uint8_t)g_escapePodPilotFlightGroupIdx;
	g_objectTable[objIdx].playerOwnerIdx = -1;
	g_objectTable[objIdx].regionIdx = g_objectTable[sourceObjIdx].regionIdx;
	g_objectTable[objIdx].objectType = objectTypeId;
	g_objectTable[objIdx].genusId = objectGenus;
	g_objectTable[objIdx].objectSignature = g_nextObjectSignature++;
	if (g_nextObjectSignature == 0) {
		g_nextObjectSignature = 2;
	}

	g_objectTable[objIdx].mobj->state = g_modelTypeTable[objectTypeId].familyId;
	g_objectTable[objIdx].mobj->instanceExtent = g_modelTypeTable[objectTypeId].maxBoundsExtent;
	g_objectTable[objIdx].world_x = g_objectTable[sourceObjIdx].world_x;
	g_objectTable[objIdx].world_y = g_objectTable[sourceObjIdx].world_y;
	g_objectTable[objIdx].world_z = g_objectTable[sourceObjIdx].world_z;
	g_objectTable[objIdx].mobj->prevWorldX = g_objectTable[objIdx].world_x;
	g_objectTable[objIdx].mobj->prevWorldY = g_objectTable[objIdx].world_y;
	g_objectTable[objIdx].mobj->prevWorldZ = g_objectTable[objIdx].world_z;
	collide_ResetObjectProximityForSlot(objIdx);
	g_objectTable[objIdx].mobj->damageAmount = g_modelTypeTable[objectTypeId].maxBoundsExtent;
	g_objectTable[objIdx].mobj->lifetimeTimer = 0;
	g_objectTable[objIdx].mobj->framesAlive = 0;
	g_objectTable[objIdx].mobj->sourceObjIdx = -1;
	g_objectTable[objIdx].mobj->sourceObjectType = objectTypeId;
	g_objectTable[objIdx].mobj->ejectionSpawnCount = 0;
	g_objectTable[objIdx].mobj->iff = g_objectTable[sourceObjIdx].mobj->iff;
	g_objectTable[objIdx].mobj->team = g_objectTable[sourceObjIdx].mobj->team;
	g_objectTable[objIdx].mobj->nodeSwitchIndex = g_objectTable[sourceObjIdx].mobj->nodeSwitchIndex;
	g_objectTable[objIdx].mobj->moveVectorDirty = 1;
	g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
	craft = g_objectTable[objIdx].mobj->pCraft;
	g_objectTable[objIdx].mobj->rollImpulseRate = (int16_t)(GameRand() & 0x1ff);
	g_objectTable[objIdx].mobj->spinRate = 0;
	g_objectTable[objIdx].mobj->spinRateFrac = 0;
	g_objectTable[objIdx].mobj->spinAngleQ16 = 0;
	g_objectTable[objIdx].mobj->velocityOverrideActive = 0;
	g_objectTable[objIdx].mobj->speed = (uint16_t)(g_objectTable[sourceObjIdx].mobj->speed + 80);
	g_objectTable[objIdx].mobj->speedRemainder = 0;

	g_objectTable[objIdx].yaw = g_objectTable[sourceObjIdx].yaw;
	g_objectTable[objIdx].pitch = g_objectTable[sourceObjIdx].pitch;
	g_objectTable[objIdx].roll = g_objectTable[sourceObjIdx].roll;
	g_objectTable[objIdx].angleD = g_objectTable[sourceObjIdx].angleD;
	yawDelta = (GameRand() & 0x2fff) + 256;
	pitchDelta = (GameRand() & 0x2fff) + 256;
	if (GameRand() & 1) {
		yawDelta = -yawDelta;
	}
	if (GameRand() & 1) {
		pitchDelta = -pitchDelta;
	}
	g_objectTable[objIdx].yaw = (uint16_t)(g_objectTable[objIdx].yaw + 0x8000u);
	g_objectTable[objIdx].yaw = (uint16_t)(g_objectTable[objIdx].yaw + (uint16_t)yawDelta);
	g_objectTable[objIdx].pitch = (uint16_t)(0x8000u - g_objectTable[objIdx].pitch);
	g_objectTable[objIdx].pitch = (uint16_t)(g_objectTable[objIdx].pitch + (uint16_t)pitchDelta);
	if (g_objectTable[objIdx].pitch >= 0x8000u) {
		g_objectTable[objIdx].pitch = (uint16_t)(0u - g_objectTable[objIdx].pitch);
		g_objectTable[objIdx].yaw = (uint16_t)(g_objectTable[objIdx].yaw + 0x8000u);
	}
	g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[objIdx].mobj->moveVectorDirty = 1;
	g_objectTable[objIdx].mobj->speed =
		(uint16_t)(g_objectTable[objIdx].mobj->speed + (uint16_t)((GameRand() & 0x1f) + 64));

	craft->effectiveAiObjectLink = NULL;
	craft->turretAim.effectiveAiObjectSignature = 0;
	craft->modelIndex = modelIndex;
	craft->craftIndexInGroup = 0;
	craft->leader_obj_idx = -1;
	craft->aiLinkResolving = 0;
	craft->objectKind = 0;
	craft->missionAccountingDone = 0;
	craft->skillValue = 0;
	craft->breakupPitchRate = 0;
	craft->breakupYawRate = 0;
	memset(craft->beamEffectAccum, 0, sizeof(craft->beamEffectAccum));
	craft->sFoilState = 0;

	craft->aiController.currentOrderSlot = 0;
	craft->aiController.orderStateFlag = 0;
	craft->aiController.targetObjIdx = 0xffffu;
	craft->aiController.candidateTargetIdx = 0xffffu;
	craft->aiController.targetSignature = 0;
	craft->aiController.hasLiveTarget = 0;
	craft->carriedObjectIndex = 0xffffu;
	craft->carrierObjIdx = 0xffffu;
	craft->lastReleasedObjectIdx = 0xffffu;
	craft->releaseClearTimer = 0;
	craft->linkedPrevObjectIdx = 0xffffu;
	craft->nextLinkObjectIdx = 0xffffu;
	craft->linkSequenceIndex = 0;
	craft->aiFlight.impactObjIdx = 0xffffu;
	craft->aiFlight.goHomeFlag = 0;
	craft->aiFlight.missionAbortedFlag = 0;
	craft->aiFlight.departTimerFlag = 0;
	craft->aiFlight.departClockHours = 0;
	craft->aiFlight.departClockMin = 0;
	craft->aiFlight.departClockSec = 0;
	craft->aiFlight.reactionTimer = 0;
	craft->commandedSpeed = 0;
	craft->aiFlight.reserved0C = 0;
	craft->aiFlight.orderActionCounter = 0;
	craft->aiFlight.orderActionFlag = 0;
	craft->aiFlight.objSignatureCount = 0;
	craft->aiController.targetComponent = 0xffffu;
	craft->aiController.escortTargetFG = -1;
	craft->aiController.aimPointX = 0;
	craft->aiController.aimPointY = 0;
	craft->aiController.aimPointZ = 0;
	craft->aiController.maneuverDist = 0;
	craft->aiController.orbitRadius = 0;
	craft->aiController.targetZAngle = 0x4000u;
	craft->aiController.targetRoll = 0;
	craft->aiController.targetXYAngle = 0;
	craft->aiController.waypointIndex = 0;
	craft->aiController.savedPlanId = 0;
	craft->aiController.thinkInterval = 708;
	{
		int randomSeed;

		randomSeed = GameRand();
		randomSeed ^= 0xbeef;
		craft->aiController.savedRandSeed = (int16_t)randomSeed;
	}
	craft->aiController.maneuverMode = 0;
	craft->aiController.maneuverPhase = 0;
	craft->aiController.maneuverTimer = 0;

	{
		const char* cargoName;
		char* cargoDestination;
		int remaining;

		if (craft->waveNumber ==
			g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.specialCargoCraft) {
			cargoName = g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.specialCargo;
		} else {
			cargoName = g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.cargo;
		}
		cargoDestination = craft->specialCargoName;
		remaining = 16;
		do {
			*cargoDestination++ = *cargoName++;
		} while (--remaining != 0);
	}

	craft->cargoIndex = 0xffu;
	craft->boardingState = 0;
	craft->systemFlags = 1023;
	craft->installedHudFeatureMask = 0x1fffu;
	craft->aiFlight.enterFlag = 0;
	craft->aiFlight.headingState = 0;
	craft->aiFlight.turnState = 0;
	craft->aiFlight.climbState = 0;
	craft->aiFlight.diveState = 0;
	craft->aiFlight.headingForce = 0;
	craft->aiFlight.motionScale = -1;
	craft->aiFlight.rollAccel = -1;
	craft->aiFlight.pitchAccel = -1;
	craft->aiFlight.turnAccel = -1;
	craft->aiFlight.rollRate = g_modelDefs[modelIndex].rollRate;
	craft->aiFlight.pitchRate = g_modelDefs[modelIndex].pitchRate;
	craft->aiFlight.turnRate = g_modelDefs[modelIndex].yawRate;
	craft->aiFlight.maxSpeedCache = (int16_t)g_modelDefs[modelIndex].maxSpeed;
	craft->hullMax = g_modelDefs[modelIndex].hullStrength;
	craft->systemDamageHullThreshold = g_modelDefs[modelIndex].systemDamageHullThreshold;
	craft->hullDamage = 0;
	craft->subsystemDamage = 0;
	craft->lastSystemHitTime = 0;
	craft->systemHitFlag = 0;
	craft->damageReceivedTotal = 0;
	craft->damageReceivedByPlayerOwnedCraft = 0;
	craft->damageFromCollision = 0;
	craft->damageFromStarship = 0;
	craft->damageFromMine = 0;
	memset(craft->damageFromPlayer, 0, sizeof(craft->damageFromPlayer));
	for (i = 0; i < 8; ++i) {
		craft->damageFromFlightGroupAmount[i] = 0;
		craft->damageFromFlightGroupIdx[i] = -1;
	}
	memset(craft->attackedByTeam, 0, sizeof(craft->attackedByTeam));
	memset(craft->damageFromAiSkill, 0, sizeof(craft->damageFromAiSkill));
	craft->weaponFireInhibitTimer = 0;
	craft->unusedMissionFlag189 = 0;
	craft->notDisabledAccountingSuppress = 0;
	craft->wasCaptured = 0;
	memset(craft->iffVisibility, 0xff, sizeof(craft->iffVisibility));

	if (!g_modelDefs[modelIndex].hasHyperdrive) {
		craft->systemFlags ^= 0x80u;
	}
	craft->shieldFront = g_modelDefs[modelIndex].shieldStrength;
	craft->shieldFront += g_modelDefs[modelIndex].shieldStrength;
	craft->shieldRear = 0;
	craft->shieldDistribMode = 0;
	if (!g_modelDefs[modelIndex].hasShields) {
		craft->systemFlags ^= 1u;
		craft->shieldFront = 0;
		craft->shieldRear = 0;
		craft->installedHudFeatureMask ^= 0x800u;
	}

	craft->beamTypeId = 0;
	craft->beamLevel = 2;
	craft->beamPresent = 0;
	craft->beamActive = 0;
	craft->beamTimer = 0;
	craft->beamTargetObjIdx = -1;
	craft->systemFlags ^= 0x100u;
	craft->installedHudFeatureMask ^= 0x1010u;
	craft->cmTypeId = 0;
	craft->cmAmmoCount = 0;
	craft->systemFlags ^= 2u;
	craft->chaffActiveTimer = 0;
	craft->cmFireCooldownTimer = 0;
	craft->workingSubsystems = craft->systemFlags;
	craft->activeHudFeatureMask = craft->installedHudFeatureMask;

	for (i = 0; i < 2; ++i) {
		g_objectTable[objIdx].typeSpecificByte[i] = 0;
	}
	{
		int componentIndex;
		int remainingComponents;

		componentIndex = 0;
		remainingComponents = 50;
		do {
			craft->componentState[componentIndex] = 0;
			craft->meshRotation[componentIndex] = 0;
			craft->componentHp[componentIndex] = 0xffu;
			++componentIndex;
		} while (--remainingComponents != 0);
	}
	{
		uint16_t meshIndex;

		meshIndex = 0;
		if (ModelMesh_GetObjectTypeMeshCount(objectTypeId) > 0) {
			MeshType meshType;

			do {
				meshType = ModelMesh_GetObjectTypeMeshType(objectTypeId, meshIndex);
				if (ModelMesh_IsObjectTypeMeshDamageable(objectTypeId, meshIndex)) {
					craft->componentHp[meshIndex] = g_meshTypeComponentMaxHp[meshType];
				}
				++meshIndex;
			} while (meshIndex < ModelMesh_GetObjectTypeMeshCount(objectTypeId));
		}
	}

	g_objectTable[objIdx].mobj->simStateTimestamp = 0;
	g_objectTable[objIdx].typeSpecificWord = craft->workingSubsystems;

	craft->aiController.pendingPlanId = 0;
	craft->aiController.currentPlanId = 0;
	{
		uint8_t* completionState;
		uint8_t* goalProgress;
		int remainingRows;

		completionState = craft->aiController.orderScratch.completionState[0];
		goalProgress = craft->aiController.orderScratch.goalProgress[0];
		remainingRows = 5;
		do {
			int remainingColumns;

			remainingColumns = 4;
			do {
				*completionState++ = 0;
				*goalProgress++ = 0;
			} while (--remainingColumns != 0);
		} while (--remainingRows != 0);
	}
	craft->aiController.aiPlanState = 0;

	craft->lastAttackerObjIdx = 0xffffu;
	craft->lastHitTimestamp = 0;
	craft->aiFlight.threatObjIdx = 0xffffu;
	craft->aiFlight.maneuverCounter = 0;
	memset(craft->aiFlight.objSignatures, 0xff, sizeof(craft->aiFlight.objSignatures));
	craft->aiFlight.headingStep = 0xffffu;
	craft->aiFlight.rollStep = 0xffffu;
	craft->aiFlight.turnStep = 0xffffu;
	craft->engineOutputScale = 0xffffu;
	craft->aiFlight.formationType = 0;
	craft->aiFlight.separation = 0;
	craft->waveNumber = 0;
	craft->pushAccumX = 0;
	craft->pushAccumY = 0;
	craft->pushAccumZ = 0;
	craft->throttleSpeed = 0x2000u;
	craft->slamActive = 0;
	craft->shieldRedirect = 2;
	craft->cannonClassCount = 0;
	craft->laserRedirect = 0;
	craft->laserSlotCount = 0;
	{
		int laserIndex;
		int remainingLaserSlots;

		laserIndex = 0;
		remainingLaserSlots = 3;
		do {
			craft->laserProjectileTypeId[laserIndex] = 0;
			craft->laserLinkMode[laserIndex] = 0;
			craft->laserLinkMode[laserIndex + 3] = 0;
			craft->laserLinkNextSlot[laserIndex] = 0;
			craft->laserFireCooldownTicks[laserIndex] = 0;
			craft->laserLastFireTimestamp[laserIndex] = 0;
			++laserIndex;
		} while (--remainingLaserSlots != 0);
	}
	craft->warheadLauncherCount = 0;
	craft->warheadLockTicks = 0;
	{
		int warheadIndex;
		int remainingWarheadSlots;

		warheadIndex = 0;
		remainingWarheadSlots = 2;
		do {
			craft->warheadSlotTypeIds[warheadIndex] = 0;
			craft->warheadLauncherFlags[warheadIndex] = 0;
			craft->warheadLauncherCooldownTicks[warheadIndex] = 0;
			++warheadIndex;
		} while (--remainingWarheadSlots != 0);
	}
	craft->laserShotsFiredCount = 0;
	craft->laserHitsScoredCount = 0;
	craft->ionShotsFiredCount = 0;
	craft->ionHitsScoredCount = 0;
	craft->warheadsFiredCount = 0;
	craft->warheadHitsScoredCount = 0;
	for (i = 0; i < 11; ++i) {
		craft->systemDisplaySlotBySystem[i] = (uint8_t)i;
		craft->systemHealth[i] = 100;
		craft->systemTimer[i] = 0;
	}
	craft->playerCommandAvoidTargetObjIdx = -1;
	craft->aiController.pendingPlanId = 0;
	craft->aiController.currentPlanId = 0;
	{
		int row;
		int col;

		for (row = 0; row < 5; ++row) {
			for (col = 0; col < 4; ++col) {
				if (col == 0) {
					craft->aiController.orderScratch.completionState[row][0] = 0;
				} else {
					craft->aiController.orderScratch.completionState[row][col] = 0;
				}
				if (col != 0) {
					craft->aiController.orderScratch.completionState[row][col] = 2;
				}
				craft->aiController.orderScratch.goalProgress[row][col] = 0;
			}
		}
	}

	switch (objectTypeId) {
		case OBJ_RebelPilot:
		case OBJ_ImperialPilot:
		case OBJ_CivilianPilot: {
			g_objectTable[objIdx].yaw = g_objectTable[sourceObjIdx].yaw;
			g_objectTable[objIdx].pitch = (uint16_t)(g_objectTable[sourceObjIdx].pitch - 0x4000u);
			if (g_objectTable[objIdx].pitch & 0x8000u) {
				g_objectTable[objIdx].pitch = (uint16_t)(0u - g_objectTable[objIdx].pitch);
				g_objectTable[objIdx].yaw = (uint16_t)(g_objectTable[objIdx].yaw + 0x8000u);
			}
			craft->aiFlight.maxSpeedCache = 64;
			g_objectTable[objIdx].mobj->speed = 80;
			g_objectTable[objIdx].mobj->damageAmount = 50;
			break;
		}
		case OBJ_EscapePod:
		case OBJ_EscapePodA: {
			CraftData* savedCurCraft;

			craft->aiController.currentPlanId = g_builtinPlanIdByNameIndex[45];
			craft->aiController.pendingPlanId = g_builtinPlanIdByNameIndex[45];
			savedCurCraft = g_curCraft;
			g_curCraft = craft;
			pai_setupcraftcontext(objIdx);
			pai_ApplyPendingPlanTargetAndManeuver(objIdx);
			craft->aiController.aimPointX = g_objectTable[objIdx].world_x;
			craft->aiController.aimPointY = g_objectTable[objIdx].world_y;
			craft->aiController.aimPointZ = g_objectTable[objIdx].world_z;
			g_curCraft = savedCurCraft;
			break;
		}
		default:
			break;
	}

	return objIdx;
}

// FUNCTION: XWA 0x41E8C0
uint16_t Object_AllocLocalTransientSlot(void) {
	uint16_t candidateObjIdx;
	uint16_t rangeEnd;

	if (regionIdx == g_players[g_localPlayer].regionIndex) {
		candidateObjIdx = (uint16_t)g_localEffectSlotStart;
		rangeEnd = (uint16_t)g_localTransientSlotEnd;
		while (candidateObjIdx < rangeEnd) {
			if (g_objectTable[candidateObjIdx].objectType == 0) {
				g_objectTable[candidateObjIdx].mobj->sourceObjIdx = -1;
				g_objectTable[candidateObjIdx].mobj->instanceExtent = 0;
				break;
			}
			++candidateObjIdx;
		}

		if (candidateObjIdx < rangeEnd) {
			collide_ResetObjectProximityForSlot(candidateObjIdx);
			return candidateObjIdx;
		}
	}

	return 0xffffu;
}

// FUNCTION: XWA 0x41EDF0
void Object_CopyStatePreservingStorage(unsigned int dstObjIdx, unsigned int srcObjIdx) {
	CraftData* savedCraft;
	WarheadGuidanceState* savedGuidance;
	MobileObjectCharData* savedCharData;
	MobileObject* savedMobj;

	if (g_objectTable[dstObjIdx].mobj->pCraft != NULL && g_objectTable[srcObjIdx].mobj->pCraft != NULL) {
		memcpy(g_objectTable[dstObjIdx].mobj->pCraft, g_objectTable[srcObjIdx].mobj->pCraft,
			   sizeof(CraftData));
	}
	if (g_objectTable[dstObjIdx].mobj->pWarheadGuidance != NULL &&
		g_objectTable[srcObjIdx].mobj->pWarheadGuidance != NULL) {
		memcpy(g_objectTable[dstObjIdx].mobj->pWarheadGuidance,
			   g_objectTable[srcObjIdx].mobj->pWarheadGuidance, 10);
	}
	if (g_objectTable[dstObjIdx].mobj->pCharData != NULL &&
		g_objectTable[srcObjIdx].mobj->pCharData != NULL) {
		memcpy(g_objectTable[dstObjIdx].mobj->pCharData, g_objectTable[srcObjIdx].mobj->pCharData,
			   sizeof(MobileObjectCharData));
	}

	if (g_objectTable[dstObjIdx].mobj != NULL && g_objectTable[srcObjIdx].mobj != NULL) {
		savedCraft = g_objectTable[dstObjIdx].mobj->pCraft;
		savedGuidance = g_objectTable[dstObjIdx].mobj->pWarheadGuidance;
		savedCharData = g_objectTable[dstObjIdx].mobj->pCharData;
		memcpy(g_objectTable[dstObjIdx].mobj, g_objectTable[srcObjIdx].mobj, sizeof(MobileObject));
		g_objectTable[dstObjIdx].mobj->pCraft = savedCraft;
		g_objectTable[dstObjIdx].mobj->pWarheadGuidance = savedGuidance;
		g_objectTable[dstObjIdx].mobj->pCharData = savedCharData;
	}

	savedMobj = g_objectTable[dstObjIdx].mobj;
	memcpy(&g_objectTable[dstObjIdx], &g_objectTable[srcObjIdx], sizeof(ObjectRecord));
	g_objectTable[dstObjIdx].mobj = savedMobj;

	collide_ResetObjectProximityForSlot((uint16_t)dstObjIdx);
}

// FUNCTION: XWA 0x41DF10
uint16_t Object_SpawnDetachedComponent(uint16_t sourceObjIdx, uint8_t componentIdx) {
	uint16_t candidateObjIdx;
	uint16_t rangeEnd;
	uint16_t objectIdx;
	MobileObject* mobj;

	candidateObjIdx = (uint16_t)g_objectSlotRangeByGenus[GENUS_Debris].next;
	rangeEnd = (uint16_t)g_objectSlotRangeByGenus[GENUS_Debris].end;
	while (candidateObjIdx < rangeEnd) {
		if (g_objectTable[candidateObjIdx].objectType == 0) {
			g_objectTable[candidateObjIdx].mobj->sourceObjIdx = -1;
			g_objectTable[candidateObjIdx].mobj->instanceExtent = 0;
			break;
		}
		++candidateObjIdx;
	}

	if (candidateObjIdx < rangeEnd) {
		if (g_flightPlayerCount > 1 || g_filmRecording || g_filmPlaybackMode) {
			g_objectTable[candidateObjIdx].objectSignature = 1;
		}
		collide_ResetObjectProximityForSlot(candidateObjIdx);
		objectIdx = candidateObjIdx;
	} else {
		objectIdx = 0xffffu;
	}

	if (objectIdx == 0xffffu) {
		return 0xffffu;
	}

	Object_CopyStatePreservingStorage(objectIdx, sourceObjIdx);

	mobj = g_objectTable[objectIdx].mobj;
	mobj->state = 3;
	g_objectTable[objectIdx].genusId = GENUS_Debris;
	g_objectTable[objectIdx].mobj->instanceExtent =
		g_modelTypeTable[(uint16_t)g_objectTable[sourceObjIdx].objectType].maxBoundsExtent;
	g_objectTable[objectIdx].objectType = OBJ_NoAsset_222;
	g_objectTable[objectIdx].mobj->sourceObjectType = g_objectTable[sourceObjIdx].objectType;
	g_objectTable[objectIdx].playerOwnerIdx = -1;
	g_objectTable[objectIdx].mobj->framesAlive = 0;
	g_objectTable[objectIdx].mobj->lifetimeTimer = 236 * ((GameRand() & 7) + 4);
	g_objectTable[objectIdx].objectSignature = g_nextObjectSignature++;
	if (g_nextObjectSignature == 0) {
		g_nextObjectSignature = 2;
	}
	g_objectTable[objectIdx].typeSpecificByte[0] = (uint8_t)(2u * componentIdx);
	g_objectTable[objectIdx].typeSpecificByte[1] = 0;
	return objectIdx;
}

// FUNCTION: XWA 0x41E0F0
uint16_t Object_SpawnEffectFragment(uint16_t sourceObjIdx) {
	uint16_t candidateObjIdx;
	uint16_t rangeEnd;
	uint16_t objectIdx;
	MobileObject* sourceMobj;

	candidateObjIdx = (uint16_t)g_objectSlotRangeByGenus[GENUS_Explosion].next;
	rangeEnd = (uint16_t)g_objectSlotRangeByGenus[GENUS_Explosion].end;
	while (candidateObjIdx < rangeEnd) {
		if (g_objectTable[candidateObjIdx].objectType == 0) {
			g_objectTable[candidateObjIdx].mobj->sourceObjIdx = -1;
			g_objectTable[candidateObjIdx].mobj->instanceExtent = 0;
			break;
		}
		++candidateObjIdx;
	}

	if (candidateObjIdx < rangeEnd) {
		if (g_flightPlayerCount > 1 || g_filmRecording || g_filmPlaybackMode) {
			g_objectTable[candidateObjIdx].objectSignature = 1;
		}
		collide_ResetObjectProximityForSlot(candidateObjIdx);
		objectIdx = candidateObjIdx;
	} else {
		objectIdx = 0xffffu;
	}

	if (objectIdx == 0xffffu) {
		return 0xffffu;
	}

	Object_CopyStatePreservingStorage(objectIdx, sourceObjIdx);

	g_objectTable[objectIdx].mobj->state = 5;
	g_objectTable[objectIdx].genusId = GENUS_Explosion;
	g_objectTable[objectIdx].objectType = (uint16_t)((GameRand() & 1) + OBJ_SparkTextureGroup3002);
	g_objectTable[objectIdx].mobj->instanceExtent =
		g_modelTypeTable[g_objectTable[objectIdx].objectType].maxBoundsExtent;
	g_objectTable[objectIdx].mobj->sourceObjectType = g_objectTable[sourceObjIdx].objectType;
	g_objectTable[objectIdx].playerOwnerIdx = -1;

	sourceMobj = g_objectTable[sourceObjIdx].mobj;
	if (sourceMobj != NULL && sourceMobj->velocityOverrideActive) {
		trig2_ctop(sourceMobj->velocityOverrideDirX, sourceMobj->velocityOverrideDirY,
				   sourceMobj->velocityOverrideDirZ);
		g_objectTable[objectIdx].yaw = trig2_xyangle;
		g_objectTable[objectIdx].pitch = targetPitch;
	}

	{
		int yawDelta;
		int pitchDelta;

		yawDelta = (GameRand() & 0x7ff) + 0x100;
		pitchDelta = (GameRand() & 0x7ff) + 0x100;
		if (GameRand() & 1) {
			yawDelta = -yawDelta;
		}
		if (GameRand() & 1) {
			pitchDelta = -pitchDelta;
		}

		g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + yawDelta);
		g_objectTable[objectIdx].pitch = (uint16_t)(g_objectTable[objectIdx].pitch + pitchDelta);
	}
	if (g_objectTable[objectIdx].pitch >= 0x8000u) {
		g_objectTable[objectIdx].pitch = (uint16_t)(0u - g_objectTable[objectIdx].pitch);
		g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + 0x8000u);
	}

	g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
	g_objectTable[objectIdx].mobj->speed =
		(uint16_t)(g_objectTable[objectIdx].mobj->speed + (GameRand() & 0xff) + 50);
	g_objectTable[objectIdx].mobj->framesAlive = 0;
	g_objectTable[objectIdx].mobj->lifetimeTimer = 236 * ((GameRand() & 3) + 1);
	g_objectTable[objectIdx].typeSpecificByte[0] = 1;
	return objectIdx;
}

// FUNCTION: XWA 0x41E3B0
uint16_t Object_SpawnLocalEffectFragment(uint16_t sourceObjIdx) {
	uint16_t candidateObjIdx;
	uint16_t rangeEnd;
	uint16_t objectIdx;
	int16_t yawDelta;
	int16_t pitchDelta;
	uint16_t moveDistance;

	if (regionIdx == g_players[g_localPlayer].regionIndex) {
		candidateObjIdx = (uint16_t)g_localEffectSlotStart;
		rangeEnd = (uint16_t)g_localTransientSlotEnd;
		while (candidateObjIdx < rangeEnd) {
			if (g_objectTable[candidateObjIdx].objectType == 0) {
				g_objectTable[candidateObjIdx].mobj->sourceObjIdx = -1;
				g_objectTable[candidateObjIdx].mobj->instanceExtent = 0;
				break;
			}
			++candidateObjIdx;
		}

		if (candidateObjIdx < rangeEnd) {
			collide_ResetObjectProximityForSlot(candidateObjIdx);
			objectIdx = candidateObjIdx;
		} else {
			objectIdx = 0xffffu;
		}
	} else {
		objectIdx = 0xffffu;
	}

	if (objectIdx == 0xffffu) {
		return 0xffffu;
	}

	Object_CopyStatePreservingStorage(objectIdx, sourceObjIdx);

	g_objectTable[objectIdx].mobj->state = 5;
	g_objectTable[objectIdx].genusId = GENUS_Explosion;
	g_objectTable[objectIdx].objectType = OBJ_ChaffTextureGroup5000;
	g_objectTable[objectIdx].mobj->instanceExtent =
		g_modelTypeTable[OBJ_ChaffTextureGroup5000].maxBoundsExtent;
	g_objectTable[objectIdx].mobj->sourceObjectType = g_objectTable[sourceObjIdx].objectType;
	g_objectTable[objectIdx].playerOwnerIdx = -1;

	yawDelta = (GameRand2() & 0x1fff) + 0x100;
	pitchDelta = (GameRand2() & 0x1fff) + 0x100;
	if (GameRand2() & 1) {
		yawDelta = -yawDelta;
	}
	if (GameRand2() & 1) {
		pitchDelta = -pitchDelta;
	}

	g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + 0x8000u);
	g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + (uint16_t)yawDelta);
	g_objectTable[objectIdx].pitch = (uint16_t)(0x8000u - g_objectTable[objectIdx].pitch);
	g_objectTable[objectIdx].pitch = (uint16_t)(g_objectTable[objectIdx].pitch + (uint16_t)pitchDelta);
	if (g_objectTable[objectIdx].pitch >= 0x8000u) {
		g_objectTable[objectIdx].pitch = (uint16_t)(0u - g_objectTable[objectIdx].pitch);
		g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + 0x8000u);
	}

	g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
	g_objectTable[objectIdx].mobj->speed = (uint16_t)((GameRand2() & 0x0f) + 35);
	g_objectTable[objectIdx].mobj->framesAlive = 0;
	g_objectTable[objectIdx].mobj->lifetimeTimer = (GameRand2() & 3) + 39;
	g_objectTable[objectIdx].typeSpecificByte[0] = 1;

	{
		ObjectRecord* object;

		object = &g_objectTable[objectIdx];
		object->mobj->prevWorldX = object->world_x;
		object->mobj->prevWorldY = object->world_y;
		object->mobj->prevWorldZ = object->world_z;

		moveDistance = (uint16_t)MATH2_mphconvert((int16_t)(object->mobj->speed + 75), g_simStepScale);
		if (moveDistance != 0) {
			if (object->mobj->moveVectorDirty) {
				FVIEW_calcrotatemove((int16_t)object->pitch, (int16_t)object->yaw, object);
			}
			trig2_xmovedist = Xwa_Q15MulReuseFirstSlot(object->mobj->moveX, (int)moveDistance);
			trig2_ymovedist = Xwa_Q15MulReuseFirstSlot(object->mobj->moveY, (int)moveDistance);
			{
				int zMove;

				zMove = Xwa_Q15MulReuseFirstSlot(object->mobj->moveZ, (int)moveDistance);
				trig2_xmovedist <<= 2;
				trig2_ymovedist <<= 2;
				trig2_zmovedist = zMove * 4;
			}
			Object_AddTrigMoveDeltaAndClampWorldPosition(object);
		}
	}

	return objectIdx;
}

// FUNCTION: XWA 0x495020
int Object_SpawnExplosionSpriteCloud(ObjectRecord* sourceObj, int sourceGenus) {
	int stepX;
	int stepY;
	int stepZ;
	int startX;
	int startY;
	int startZ;
	unsigned int spriteCount;
	unsigned int remaining;
	unsigned int objectIdx;
	unsigned int modelIndex;
	(void)sourceGenus;

	modelIndex = (ModelIndex)GetModelIndexFromType(sourceObj->objectType);
	stepX = sourceObj->world_x;
	stepY = sourceObj->world_y;
	stepZ = sourceObj->world_z;
	startX = 0;
	startY = 0;
	startZ = 0;
	spriteCount = 1;
	remaining = 1;

	if (sourceObj->genusId == GENUS_Starship && modelIndex != 0xffffu) {
		int offsetX;
		int offsetY;
		int offsetZ;
		float stepScale;

		offsetX = Xwa_Q15MulReuseFirstSlot(ModelBounds_GetMinY((uint16_t)sourceObj->objectType),
										   sourceObj->mobj->cachedFwdX);
		offsetY = Xwa_Q15MulReuseFirstSlot(ModelBounds_GetMinY((uint16_t)sourceObj->objectType),
										   sourceObj->mobj->cachedFwdY);
		offsetZ = Xwa_Q15MulReuseFirstSlot(ModelBounds_GetMinY((uint16_t)sourceObj->objectType),
										   sourceObj->mobj->cachedFwdZ);
		startX = sourceObj->world_x - offsetX;
		startY = sourceObj->world_y - offsetY;
		startZ = sourceObj->world_z - offsetZ;
		if (sourceObj->objectType == OBJ_SuperStarDestroyer) {
			spriteCount = 6;
		} else {
			spriteCount = (GameRand() & 3u) + 1u;
		}
		stepScale = g_explosionCloudSpacingNumerator / (spriteCount + 1u);
		stepX = (int)(offsetX * stepScale);
		stepY = (int)(offsetY * stepScale);
		stepZ = (int)(offsetZ * stepScale);
		remaining = spriteCount;
	}

	if (remaining > 0u) {
		int curX;
		int curY;
		int curZ;

		curZ = startZ + stepZ;
		curY = startY + stepY;
		curX = startX + stepX;
		do {
			objectIdx = Object_AllocSlotForGenus(GENUS_Explosion);
			if (objectIdx == 0xffffu) {
				unsigned int rangeStart;
				unsigned int rangeEnd;

				rangeStart = g_objectSlotRangeByGenus[GENUS_Explosion].next;
				rangeEnd = g_objectSlotRangeByGenus[GENUS_Explosion].end;
				objectIdx = rangeStart;
				if (objectIdx < rangeEnd) {
					while (objectIdx < rangeEnd &&
						   g_objectTable[objectIdx].objectType == OBJ_ExplosionTextureGroup2006) {
						++objectIdx;
					}
				}
				if (objectIdx == rangeEnd) {
					objectIdx = rangeStart;
				}
			}

			g_objectTable[objectIdx].objectType = OBJ_ExplosionTextureGroup2006;
			g_objectTable[objectIdx].world_x = curX;
			g_objectTable[objectIdx].world_y = curY;
			g_objectTable[objectIdx].world_z = curZ;
			g_objectTable[objectIdx].genusId = GENUS_Explosion;
			g_objectTable[objectIdx].mobj->state = 5;
			if (sourceObj->objectType == OBJ_SuperStarDestroyer) {
				g_objectTable[objectIdx].typeSpecificByte[0] = 1;
			} else {
				g_objectTable[objectIdx].typeSpecificByte[0] = (uint8_t)(2 * (GameRand() & 3u) + 1);
			}
			g_objectTable[objectIdx].mobj->framesAlive = 0;
			g_objectTable[objectIdx].mobj->lifetimeTimer = 0;
			g_objectTable[objectIdx].typeSpecificWord =
				(uint16_t)(ModelBounds_GetMaxExtent((uint16_t)sourceObj->objectType) / 2);
			if (sourceObj->objectType == OBJ_SuperStarDestroyer) {
				g_objectTable[objectIdx].mobj->instanceExtent = 0xfffff;
			} else {
				g_objectTable[objectIdx].mobj->instanceExtent =
					g_modelTypeTable[(uint16_t)sourceObj->objectType].maxBoundsExtent / spriteCount;
			}
			g_objectTable[objectIdx].mobj->sourceObjIdx = -1;
			g_objectTable[objectIdx].mobj->sourceObjectType = sourceObj->objectType;
			g_objectTable[objectIdx].pitch = sourceObj->pitch;
			g_objectTable[objectIdx].yaw = sourceObj->yaw;
			g_objectTable[objectIdx].roll = GameRand();
			g_objectTable[objectIdx].angleD = 0;
			g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
			g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objectIdx].mobj->spinRate = 0;
			g_objectTable[objectIdx].mobj->spinAngleQ16 = 0;

			if (sourceObj->mobj) {
				if (sourceObj->mobj->velocityOverrideActive) {
					trig2_ctop(sourceObj->mobj->velocityOverrideDirX, sourceObj->mobj->velocityOverrideDirY,
							   sourceObj->mobj->velocityOverrideDirZ);
					g_objectTable[objectIdx].yaw = trig2_xyangle;
					g_objectTable[objectIdx].pitch = targetPitch;
					g_objectTable[objectIdx].mobj->speed = sourceObj->mobj->velocityOverrideSpeed;
				}
			} else {
				g_objectTable[objectIdx].mobj->speed = 0;
			}

			curY += stepY;
			curX += stepX;
			curZ += stepZ;
			--remaining;
		} while (remaining != 0u);
	} else {
		objectIdx = spriteCount;
	}

	return fsfx_PlaySound(24, objectIdx, (unsigned int)g_localPlayer);
}

// FUNCTION: XWA 0x41E7F0
void Object_ClearSlotState(uint32_t objIdx) {
	if (objIdx == 0xffffu) {
		return;
	}

	memset(&g_objectTable[objIdx], 0, offsetof(ObjectRecord, mobj));
	g_objectTable[objIdx].playerOwnerIdx = -1;

	if (g_objectTable[objIdx].mobj == NULL) {
		return;
	}

	memset(g_objectTable[objIdx].mobj, 0, offsetof(MobileObject, moveVectorDirty));

	if (g_objectTable[objIdx].mobj->pCraft != NULL) {
		memset(g_objectTable[objIdx].mobj->pCraft, 0,
			   offsetof(CraftData, warheadData[14].count) +
				   sizeof(g_objectTable[objIdx].mobj->pCraft->warheadData[14].count));
		g_objectTable[objIdx].mobj->iff = -1;
	}

	if (g_objectTable[objIdx].mobj->pWarheadGuidance != NULL) {
		memset(g_objectTable[objIdx].mobj->pWarheadGuidance, 0,
			   sizeof(*g_objectTable[objIdx].mobj->pWarheadGuidance));
	}

	if (g_objectTable[objIdx].mobj->pCharData != NULL) {
		memset(g_objectTable[objIdx].mobj->pCharData, 0, sizeof(*g_objectTable[objIdx].mobj->pCharData));
	}
}

// FUNCTION: XWA 0x505EB0
char Object_HasActiveDecoyBeam(uint16_t objIdx) {
	CraftData* craft;

	if (objIdx == 0xffffu || objIdx >= g_activeRegionCraftObjectSlotEnd) {
		return 0;
	}

	if (g_objectTable[objIdx].playerOwnerIdx == -1) {
		return 0;
	}

	craft = g_objectTable[objIdx].mobj->pCraft;
	if (craft == NULL) {
		return 0;
	}

	if ((craft->workingSubsystems & 0x100u) == 0 || craft->beamActive == 0 || craft->beamTypeId != 3 ||
		craft->beamTimer == 0) {
		return 0;
	}

	return 1;
}

// FUNCTION: XWA 0x4050A0
void MobileObject_SetRandomSpinAxis(int objIdx) {
	float axisX;
	float axisY;
	float axisZ;
	float length;

	axisX = (float)(int16_t)GameRand() * g_randomSpinAxisScale;
	axisY = (float)(int16_t)GameRand() * g_randomSpinAxisScale;
	axisZ = (float)(int16_t)GameRand() * g_randomSpinAxisScale;
	length = (float)sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ);

	if (length == g_randomSpinAxisZero) {
		axisX = g_randomSpinAxisZero;
		axisY = 0.0f;
		axisZ = 1.0f;
	} else {
		axisX /= length;
		axisY /= length;
		axisZ /= length;
	}

	g_objectTable[objIdx].mobj->spinAxisX = axisX;
	g_objectTable[objIdx].mobj->spinAxisY = axisY;
	g_objectTable[objIdx].mobj->spinAxisZ = axisZ;
}

// FUNCTION: XWA 0x497590
ObjectRecord* Object_AddTrigMoveDeltaAndClampWorldPosition(ObjectRecord* obj) {
	obj->world_x += trig2_xmovedist;
	if (obj->world_x < -0x01000000) {
		obj->world_x = -0x01000000;
	}
	if (obj->world_x > 0x01000000) {
		obj->world_x = 0x01000000;
	}

	obj->world_y += trig2_ymovedist;
	if (obj->world_y < -0x01000000) {
		obj->world_y = -0x01000000;
	}
	if (obj->world_y > 0x01000000) {
		obj->world_y = 0x01000000;
	}

	obj->world_z += trig2_zmovedist;
	if (obj->world_z < -0x01000000) {
		obj->world_z = -0x01000000;
	}
	if (obj->world_z > 0x01000000) {
		obj->world_z = 0x01000000;
	}

	return obj;
}

// FUNCTION: XWA 0x505D40
int Object_IsHostileToTeam(uint16_t objectIdx, int teamIdx) {
	ObjectRecord* objectTable;
	ObjectRecord* obj;
	MobileObject* mobj;
	int objectTeam;

	objectTable = g_objectTable;
	obj = &g_objectTable[objectIdx];
	mobj = obj->mobj;
	if (mobj != NULL) {
		objectTeam = mobj->team;
	} else {
		objectTeam = g_missionFlightGroups[objectTable[objectIdx].flightGroupIdx].fg.team;
	}

	if (objectTeam == teamIdx) {
		return 0;
	}

	return g_missionTeams[teamIdx].allies[objectTeam] == 0;
}

// FUNCTION: XWA 0x505DF0
int Object_IsFriendlyToTeam(uint16_t objectIdx, int teamIdx) {
	ObjectRecord* objectTable;
	ObjectRecord* obj;
	MobileObject* mobj;
	int objectTeam;

	objectTable = g_objectTable;
	obj = &g_objectTable[objectIdx];
	mobj = obj->mobj;
	if (mobj != NULL) {
		objectTeam = mobj->team;
	} else {
		objectTeam = g_missionFlightGroups[objectTable[objectIdx].flightGroupIdx].fg.team;
	}

	if (objectTeam == teamIdx) {
		return 1;
	}

	return g_missionTeams[teamIdx].allies[objectTeam] == 1;
}

// FUNCTION: XWA 0x5041C0
unsigned int Object_DirectionAndDistanceToMeshCenter(uint16_t fromObjIdx, uint16_t targetObjIdx,
													 unsigned int meshIdx) {
	int fromWorldX;
	int fromWorldY;
	int fromWorldZ;
	int targetWorldX;
	int targetWorldY;
	int targetWorldZ;
	uint16_t targetObjectType;
	int dx;
	int dy;
	int dz;

	fromWorldX = g_objectTable[fromObjIdx].world_x;
	fromWorldY = g_objectTable[fromObjIdx].world_y;
	fromWorldZ = g_objectTable[fromObjIdx].world_z;

	Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
	targetWorldX = worldlocx;
	targetWorldY = worldlocy;
	targetWorldZ = worldlocz;

	targetObjectType = g_objectTable[targetObjIdx].objectType;
	g_rotatedX = ModelMesh_GetCenterX(targetObjectType, (int)meshIdx);
	g_rotatedY = ModelMesh_GetCenterZ(targetObjectType, (int)meshIdx);
	g_rotatedZ = -ModelMesh_GetCenterY(targetObjectType, (int)meshIdx);
	pai_RotateLocalVectorToWorldScratch(&g_objectTable[targetObjIdx], g_rotatedX, g_rotatedY, g_rotatedZ);

	dx = g_rotatedX + targetWorldX - fromWorldX;
	dy = g_rotatedY + targetWorldY - fromWorldY;
	dz = g_rotatedZ + targetWorldZ - fromWorldZ;
	trig2_ctop(dx, dy, dz);
	return (unsigned int)collide_roughdistance3d(dx, dy, dz);
}

// Sets trig2_x/y/zmovedist from a mobile object's active velocity override. Mirrors the inline
// override-movement blocks in Object_UpdateLifetimeAndMovement (craft, space bomb, salvage paths).
static __inline void Object_ComputeVelocityOverrideMove(ObjectRecord* obj) {
	int speedQ16 = (int)((uint32_t)obj->mobj->velocityOverrideSpeed << 16);
	int zComponent;
	trig2_xmovedist =
		24 *
		((int)((uint16_t)g_elapsedTicks * (uint32_t)Xwa_Q15Mul(speedQ16, obj->mobj->velocityOverrideDirX)) /
		 236);
	trig2_ymovedist =
		24 *
		((int)((uint16_t)g_elapsedTicks * (uint32_t)Xwa_Q15Mul(speedQ16, obj->mobj->velocityOverrideDirY)) /
		 236);
	zComponent = Xwa_Q15Mul(speedQ16, obj->mobj->velocityOverrideDirZ);
	trig2_xmovedist >>= 16;
	trig2_ymovedist >>= 16;
	trig2_zmovedist = (24 * (((int)(uint16_t)g_elapsedTicks * zComponent) / 236)) >> 16;
}

// Decays an active velocity override's speed and sub-tick fraction by one sim step, clearing it
// when it runs out. Fixed-point borrow logic preserved exactly from the original.
static __inline void Object_DecayVelocityOverride(MobileObject* mobj) {
	uint32_t prod = (uint32_t)(uint16_t)g_elapsedTicks * mobj->velocityOverrideDuration;
	uint32_t q = prod / 236;
	uint32_t scaled = (prod + 65300u * q) << 16;
	uint16_t newSpeed = (uint16_t)(mobj->velocityOverrideSpeed - q);
	uint16_t newElapsed;
	if (scaled / 236u <= mobj->velocityOverrideElapsed) {
		newElapsed = 0;
	} else {
		newElapsed = (uint16_t)(mobj->velocityOverrideElapsed - scaled / 236u - 1);
		newSpeed = (uint16_t)(newSpeed - 1);
	}
	if (newSpeed & 0x8000) {
		newSpeed = 0;
		newElapsed = 0;
		mobj->velocityOverrideActive = 0;
	}
	mobj->velocityOverrideSpeed = newSpeed;
	mobj->velocityOverrideElapsed = newElapsed;
}

// Applies the movement scratch delta directly in the update loop, where the original code inlined
// the world-bound clamp rather than calling Object_AddTrigMoveDeltaAndClampWorldPosition.
static __inline void Object_ApplyTrigMoveDeltaAndClampWorldPosition(ObjectRecord* obj) {
	obj->world_x += trig2_xmovedist;
	if (obj->world_x < -0x01000000) {
		obj->world_x = -0x01000000;
	}
	if (obj->world_x > 0x01000000) {
		obj->world_x = 0x01000000;
	}

	obj->world_y += trig2_ymovedist;
	if (obj->world_y < -0x01000000) {
		obj->world_y = -0x01000000;
	}
	if (obj->world_y > 0x01000000) {
		obj->world_y = 0x01000000;
	}

	obj->world_z += trig2_zmovedist;
	if (obj->world_z < -0x01000000) {
		obj->world_z = -0x01000000;
	}
	if (obj->world_z > 0x01000000) {
		obj->world_z = 0x01000000;
	}
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x495460
// Per-step object lifetime and movement update over the active object range (or a single-object
// override). Handles lifetime expiry, explosion/debris conversion, scoring, roll impulse / spin /
// breakup rotation, forward movement, push accumulators, carried/linked object positioning, and
// projectile/warhead homing guidance. Temporarily repurposes g_elapsedTicks / g_simStepScale as
// the per-object time delta, restoring the per-step values on return.
void Object_UpdateLifetimeAndMovement(void) {
	uint16_t objIdx = (uint16_t)g_regionMainObjectSlotStart;
	unsigned int savedSimStepScale = g_simStepScale;
	uint16_t elapsedTicks = g_elapsedTicks;
	uint16_t savedElapsedTicks = g_elapsedTicks;
	int singleObjDone = 0;

	if (objIdx >= g_regionMainObjectSlotEnd) {
		g_simStepScale = (uint16_t)savedSimStepScale;
		g_elapsedTicks = savedElapsedTicks;
		return;
	}

	for (;;) {
		int singleIdx = g_singleObjectUpdateOverrideIdx;
		ObjectRecord* obj;
		uint16_t objectType;
		MobileObject* mobj;
		int genus;
		uint32_t lifetimeTimer;
		int16_t rollImpulseRate;
		CraftData* pCraft;
		MobileObject* fm;
		int fwdScalar;
		if (singleIdx != -1) {
			if (singleObjDone) {
				break;
			}
			objIdx = (uint16_t)singleIdx;
			singleObjDone = 1;
		}

		obj = &g_objectTable[objIdx];
		objectType = obj->objectType;

		// Skip the chute-mouth..asteroid03 special object range entirely.
		if (objectType >= OBJ_ChuteMouth) {
			if (objectType <= OBJ_Asteroid03) {
				goto advance;
			}
			elapsedTicks = savedElapsedTicks;
			singleIdx = g_singleObjectUpdateOverrideIdx;
		}

		g_elapsedTicks = elapsedTicks;
		g_simStepScale = (uint16_t)savedSimStepScale;

		mobj = obj->mobj;
		if (mobj && mobj->simStateTimestamp) {
			uint16_t delta = (uint16_t)(elapsedTicks + g_gameTime - (uint16_t)mobj->simStateTimestamp);
			g_elapsedTicks = delta;
			if (delta == 0) {
				if (singleIdx == -1 && obj->playerOwnerIdx != -1) {
					mobj->prevWorldX = obj->world_x;
					mobj->prevWorldY = obj->world_y;
					mobj->prevWorldZ = obj->world_z;
				}
				goto advance;
			}
			g_simStepScale = 236 / delta;
			if (g_simStepScale == 0) {
				g_simStepScale = 1;
			}
			mobj->simStateTimestamp += delta;
		}

		if (obj->objectType == OBJ_None) {
			goto advance;
		}

		genus = obj->genusId; // captured genus, used for movement dispatch
		lifetimeTimer = (uint32_t)obj->mobj->lifetimeTimer;

		if (lifetimeTimer == 0) {
			goto movement;
		} else {
			uint32_t newLifetime = lifetimeTimer - (uint16_t)g_elapsedTicks;
			if (newLifetime > lifetimeTimer) {
				newLifetime = 0;
			}
			obj->mobj->lifetimeTimer = (int)newLifetime;

			// Warning explosion cloud when crossing the 300-tick threshold.
			if (newLifetime <= 0x12C && lifetimeTimer > 0x12C) {
				switch (genus) {
					case GENUS_Transport:
					case GENUS_Freighter:
					case GENUS_Starship:
					case GENUS_Platform:
					case GENUS_Container:
						Object_SpawnExplosionSpriteCloud(obj, genus);
						break;
					default:
						break;
				}
			}

			if (newLifetime != 0) {
				goto movement;
			} else {
				MobileObject* em;

				// Lifetime expired this step.
				if (genus == GENUS_Transport || genus == GENUS_Freighter || genus == GENUS_Starship ||
					genus == GENUS_Platform || genus == GENUS_Rubble) {
					Debris_SpawnObjectFragments(objIdx, -1);
				}

				em = obj->mobj;
				if (em->velocityOverrideActive) {
					em->moveX = em->velocityOverrideDirX;
					obj->mobj->moveY = obj->mobj->velocityOverrideDirY;
					obj->mobj->moveZ = obj->mobj->velocityOverrideDirZ;
					obj->mobj->velocityOverrideActive = 0;
					trig2_ctop(obj->mobj->moveX, obj->mobj->moveY, obj->mobj->moveZ);
					obj->yaw = trig2_xyangle;
					obj->pitch = targetPitch;
				}

				switch (genus) {
					case GENUS_Utility:
					case GENUS_SatelliteBuoy:
					case GENUS_PilotDroid:
					case GENUS_WeaponEmplacement:
						if (!ModelMesh_HasFuselage(obj->objectType)) {
							Craft_SpawnMainHullExplosionEffects(objIdx, 1);
						} else {
							collide_ApplyDefaultProximityDamage(
								objIdx, g_objectTable[objIdx].mobj->damageAmount, objIdx);
						}
						collide_ConvertObjectToExplosion(objIdx, (ObjectTypeId)((GameRand() & 1) + 267), 0);
						g_objectTable[objIdx].mobj->instanceExtent =
							(unsigned int)g_objectTable[objIdx].mobj->instanceExtent >> 2;
						fsfx_PlaySound(35, objIdx, g_localPlayer);
						ForceFeedback_PlayProximityEffectForObject(0, objIdx);
						Mission_RecordCraftOutcome(objIdx, g_objectTable[objIdx].flightGroupIdx, 2);
						goto movement;

					case GENUS_Rubble:
						collide_ConvertObjectToExplosion(objIdx, (ObjectTypeId)((GameRand() & 1) + 267), 0);
						g_objectTable[objIdx].mobj->instanceExtent =
							(unsigned int)g_objectTable[objIdx].mobj->instanceExtent >> 1;
						fsfx_PlaySound(fsfx_PickRandomSmallExplosionSfx(), objIdx, g_localPlayer);
						ForceFeedback_PlayProximityEffectForObject(1, objIdx);
						goto movement;

					case GENUS_Transport:
					case GENUS_Freighter:
					case GENUS_Container:
						if (!ModelMesh_HasFuselage(obj->objectType)) {
							Craft_SpawnMainHullExplosionEffects(objIdx, 1);
						} else {
							collide_ApplyDefaultProximityDamage(
								objIdx, g_objectTable[objIdx].mobj->damageAmount, objIdx);
						}
						collide_ConvertObjectToExplosion(objIdx, (ObjectTypeId)((GameRand() & 3) + 264), 0);
						g_objectTable[objIdx].mobj->instanceExtent =
							(unsigned int)g_objectTable[objIdx].mobj->instanceExtent >> 3;
						fsfx_PlaySound(37, objIdx, g_localPlayer);
						ForceFeedback_PlayProximityEffectForObject(0, objIdx);
						Mission_RecordCraftOutcome(objIdx, g_objectTable[objIdx].flightGroupIdx, 2);
						goto movement;

					case GENUS_Starship:
					case GENUS_Platform:
						if (!ModelMesh_HasFuselage(obj->objectType)) {
							Craft_SpawnMainHullExplosionEffects(objIdx, 1);
						} else {
							collide_ApplyDefaultProximityDamage(
								objIdx, g_objectTable[objIdx].mobj->damageAmount, objIdx);
						}
						collide_ConvertObjectToExplosion(objIdx, (ObjectTypeId)((GameRand() & 3) + 264), 0);
						g_objectTable[objIdx].mobj->instanceExtent =
							(unsigned int)g_objectTable[objIdx].mobj->instanceExtent >> 3;
						fsfx_PlaySound((GameRand() & 1) + 38, objIdx, g_localPlayer);
						ForceFeedback_PlayProximityEffectForObject(0, objIdx);
						Mission_RecordCraftOutcome(objIdx, g_objectTable[objIdx].flightGroupIdx, 2);
						goto movement;

					case GENUS_Fighter:
						Craft_DetachDamageableComponent(objIdx, 1, 0xFFFF);
						collide_ApplyDefaultProximityDamage(
							objIdx, (unsigned int)g_objectTable[objIdx].mobj->damageAmount >> 2, objIdx);
						collide_ConvertObjectToExplosion(objIdx, (ObjectTypeId)((GameRand() & 3) + 264), 0);
						fsfx_PlaySound(36, objIdx, g_localPlayer);
						ForceFeedback_PlayProximityEffectForObject(0, objIdx);
						Mission_RecordCraftOutcome(objIdx, g_objectTable[objIdx].flightGroupIdx, 2);
						goto movement;

					case GENUS_Debris:
						collide_ApplyDefaultProximityDamage(objIdx, 0x64, objIdx);
						collide_ConvertObjectToExplosion(objIdx, (ObjectTypeId)((GameRand() & 3) + 264), 1);
						goto movement;

					case GENUS_PlayerProjectile:
					case GENUS_NpcProjectile: {
						uint16_t projType = obj->objectType;
#ifdef XWA_MODERN
						if (g_projectileWarheadClassByType[projType - OBJ_LaserRebel]) {
#else
						if (((const uint8_t*)&g_projectileDamageByType[14])[projType]) {
#endif
							if (projType == OBJ_WarheadSpaceBomb || projType == OBJ_WarheadRocket) {
								collide_ApplyDefaultProximityDamage(
									objIdx, (unsigned int)g_objectTable[objIdx].mobj->damageAmount >> 2,
									0xFFFF);
							}
							collide_ApplyDefaultProximityDamage(
								objIdx, g_objectTable[objIdx].mobj->damageAmount, 0xFFFF);
							collide_ConvertObjectToExplosion(objIdx, (ObjectTypeId)((GameRand() & 3) + 266),
															 1);
							if (obj->mobj->sourceObjIdx != -1) {
								int ownerPlayer =
									g_objectTable[(uint16_t)obj->mobj->sourceObjIdx].playerOwnerIdx;
								if (ownerPlayer != -1) {
									g_players[ownerPlayer].missionStats.missionScore -=
										g_warheadProjectilePointValueByObjectType[obj->objectType];
								}
							}
							goto movement;
						} else if (projType != OBJ_LaserRebel && projType != OBJ_LaserRebelTurbo &&
								   projType != OBJ_LaserImperial && projType != OBJ_LaserImperialTurbo &&
								   projType != OBJ_LaserImperialDS && projType != OBJ_LaserIon &&
								   projType != OBJ_LaserIonTurbo && projType != OBJ_WarheadIon &&
								   projType != OBJ_ChaffTextureGroup5000) {
							collide_ApplyDefaultProximityDamage(
								objIdx, g_objectTable[objIdx].mobj->damageAmount, 0xFFFF);
							collide_ConvertObjectToExplosion(objIdx, OBJ_SparkTextureGroup3000, 1);
							goto movement;
						} else {
							obj->objectType = OBJ_None;
						}
						goto advance;
					}

					default:
						obj->objectType = OBJ_None;
						goto advance;
				}
			}
		}

	movement:
		// --- Movement (LABEL_55) ---
		if (g_singleObjectUpdateOverrideIdx == -1) {
			obj->mobj->prevWorldX = obj->world_x;
			obj->mobj->prevWorldY = obj->world_y;
			obj->mobj->prevWorldZ = obj->world_z;
		}

		if (obj->playerOwnerIdx != -1) {
			// Player view: decay the camera pan / rotation deltas toward zero.
			PlayerViewState* vs = &g_players[obj->playerOwnerIdx].viewState;

			if (vs->cameraPanDeltaX) {
				if (vs->cameraPanDeltaX < 0) {
					vs->cameraPanDeltaX += ((uint16_t)g_elapsedTicks << 6) / 236;
					if (vs->cameraPanDeltaX > 0) {
						vs->cameraPanDeltaX = 0;
					}
				} else {
					vs->cameraPanDeltaX -= ((uint16_t)g_elapsedTicks << 6) / 236;
					if (vs->cameraPanDeltaX < 0) {
						vs->cameraPanDeltaX = 0;
					}
				}
			}
			if (vs->cameraPanDeltaY) {
				if (vs->cameraPanDeltaY < 0) {
					vs->cameraPanDeltaY += ((uint16_t)g_elapsedTicks << 6) / 236;
					if (vs->cameraPanDeltaY > 0) {
						vs->cameraPanDeltaY = 0;
					}
				} else {
					vs->cameraPanDeltaY -= ((uint16_t)g_elapsedTicks << 6) / 236;
					if (vs->cameraPanDeltaY < 0) {
						vs->cameraPanDeltaY = 0;
					}
				}
			}
			if (vs->cameraPanDeltaZ) {
				if (vs->cameraPanDeltaZ < 0) {
					vs->cameraPanDeltaZ += ((uint16_t)g_elapsedTicks << 6) / 236;
					if (vs->cameraPanDeltaZ > 0) {
						vs->cameraPanDeltaZ = 0;
					}
				} else {
					vs->cameraPanDeltaZ -= ((uint16_t)g_elapsedTicks << 6) / 236;
					if (vs->cameraPanDeltaZ < 0) {
						vs->cameraPanDeltaZ = 0;
					}
				}
			}

			if (vs->cameraPitchDelta) {
				vs->cameraPitchDelta =
					(uint16_t)(vs->cameraPitchDelta - (((uint16_t)g_elapsedTicks << 12) / 236));
			}
			if (vs->cameraYawDelta) {
				vs->cameraYawDelta =
					(uint16_t)(vs->cameraYawDelta - (((uint16_t)g_elapsedTicks << 12) / 236));
			}
			if (vs->cameraRollDelta) {
				vs->cameraRollDelta =
					(uint16_t)(vs->cameraRollDelta - (((uint16_t)g_elapsedTicks << 12) / 236));
			}
			if (vs->field_32) {
				vs->field_32 = (uint16_t)(vs->field_32 - (((uint16_t)g_elapsedTicks << 12) / 236));
			}
		}

		// Roll impulse decay / accumulated roll (LABEL_85).
		rollImpulseRate = obj->mobj->rollImpulseRate;
		if (rollImpulseRate) {
			if (objIdx < g_activeRegionObjectSlotStart || objIdx >= g_activeRegionCraftObjectSlotEnd) {
				if (obj->genusId == GENUS_SalvageJunk) {
					if (rollImpulseRate < 0) {
						int rollDecay = ((uint16_t)g_elapsedTicks << 12) / 236;
						obj->mobj->rollImpulseRate = (int16_t)(rollImpulseRate + rollDecay);
						if (obj->mobj->rollImpulseRate >= 0) {
							obj->mobj->rollImpulseRate = 0;
							rollImpulseRate = 0;
						}
					} else {
						int rollDecay = ((uint16_t)g_elapsedTicks << 12) / 236;
						obj->mobj->rollImpulseRate = (int16_t)(rollImpulseRate - rollDecay);
						if (obj->mobj->rollImpulseRate <= 0) {
							rollImpulseRate = 0;
							obj->mobj->rollImpulseRate = 0;
						}
					}
				}
			} else {
				CraftData* impactCraft = g_objectTable[objIdx].mobj->pCraft;
				if (impactCraft->aiFlight.impactObjIdx != 0xFFFF) {
					if (rollImpulseRate < 0) {
						int rollDecay = ((uint16_t)g_elapsedTicks << 12) / 236;
						obj->mobj->rollImpulseRate = (int16_t)(rollImpulseRate + rollDecay);
						if (obj->mobj->rollImpulseRate >= 0) {
							rollImpulseRate = 0;
							obj->mobj->rollImpulseRate = 0;
							impactCraft->aiFlight.impactObjIdx = 0xFFFF;
						}
					} else {
						int rollDecay = ((uint16_t)g_elapsedTicks << 12) / 236;
						obj->mobj->rollImpulseRate = (int16_t)(rollImpulseRate - rollDecay);
						if (obj->mobj->rollImpulseRate <= 0) {
							rollImpulseRate = 0;
							obj->mobj->rollImpulseRate = 0;
							impactCraft->aiFlight.impactObjIdx = 0xFFFF;
						}
					}
				}
			}
			obj->roll = (uint16_t)(obj->roll + 4 * ((uint16_t)g_elapsedTicks * rollImpulseRate / 236));
			obj->mobj->orientMatrixDirty = 1;
		} else {
			CraftData* craftClear = g_objectTable[objIdx].mobj->pCraft;
			if (craftClear) {
				craftClear->aiFlight.impactObjIdx = 0xFFFF;
			}
		}

		if (g_modelTypeTable[obj->objectType].flags & MODEL_TYPE_FLAG_YAW_UPDATES_ANGLE_D) {
			obj->angleD = (uint16_t)(obj->angleD + (((uint16_t)g_elapsedTicks << 12) / 236));
			obj->mobj->orientMatrixDirty = 1;
		}

		pCraft = obj->mobj->pCraft;
		if (pCraft) {
			// Breakup yaw/pitch tumble for damaged craft.
			int16_t breakupYawRate = pCraft->breakupYawRate;
			int16_t breakupPitchRate;
			if (breakupYawRate) {
				int yawAdd = (uint16_t)g_elapsedTicks * breakupYawRate / 236;
				obj->yaw = (uint16_t)(obj->yaw + yawAdd);
				if (breakupYawRate < 0) {
					if (breakupYawRate < -24576) {
						obj->yaw = (uint16_t)(obj->yaw + yawAdd);
					}
				} else {
					if (breakupYawRate > 24576) {
						obj->yaw = (uint16_t)(obj->yaw + yawAdd);
					}
				}
				obj->mobj->orientMatrixDirty = 1;
			}
			breakupPitchRate = obj->mobj->pCraft->breakupPitchRate;
			if (breakupPitchRate) {
				int pitchAdd = (uint16_t)g_elapsedTicks * breakupPitchRate / 236;
				obj->pitch = (uint16_t)(obj->pitch + pitchAdd);
				if (breakupPitchRate < 0) {
					if (breakupPitchRate < -24576) {
						obj->yaw = (uint16_t)(obj->yaw + pitchAdd);
					}
				} else {
					if (breakupPitchRate > 24576) {
						obj->yaw = (uint16_t)(obj->yaw + pitchAdd);
					}
				}
				obj->mobj->orientMatrixDirty = 1;
			}
		}

		// Spin (free-tumbling objects) (LABEL_120).
		{
			MobileObject* sm = obj->mobj;
			int16_t spinRate = sm->spinRate;
			if (spinRate) {
				int16_t sr2;
				int extra;
				MobileObject* dm;
				uint32_t v54;
				uint32_t q;
				uint32_t v55;
				int16_t curSpin;
				int neg;
				int absSpin;
				uint16_t newSpin16;
				uint16_t frac;

				sm->spinAngleQ16 += (int16_t)(spinRate * (int)(uint16_t)g_elapsedTicks / 236);
				sr2 = obj->mobj->spinRate;
				extra = 0;
				if (sr2 < 0) {
					if (sr2 < -24576) {
						extra = sr2 + 24576;
					}
				} else {
					if (sr2 > 24576) {
						extra = sr2 - 24576;
					}
				}
				if (extra) {
					obj->mobj->spinAngleQ16 +=
						(int16_t)((16 * ((int)(uint16_t)g_elapsedTicks * extra)) / 236);
				}
				obj->mobj->orientMatrixDirty = 1;

				// Spin decel with sub-tick fractional borrow.
				dm = obj->mobj;
				v54 = (uint32_t)(uint16_t)g_elapsedTicks * dm->spinDecelRate;
				q = v54 / 236;
				v55 = ((v54 + 65300u * q) << 16) / 236u;
				curSpin = dm->spinRate;
				neg = 0;
				absSpin = curSpin;
				if (curSpin < 0) {
					neg = 1;
					absSpin = -curSpin;
				}
				newSpin16 = (uint16_t)(absSpin - (int)q);
				frac = dm->spinRateFrac;
				if (v55 > frac) {
					frac = (uint16_t)(frac - v55 - 1);
					newSpin16 = (uint16_t)(newSpin16 - 1);
				}
				if (newSpin16 & 0x8000) {
					newSpin16 = 0;
					frac = 0;
				}
				dm->spinRate = (int16_t)newSpin16;
				if (neg) {
					dm->spinRate = (int16_t)(-(int16_t)newSpin16);
				}
				dm->spinRateFrac = frac;
			}
		}

		// Forward-move scalar for this step (LABEL_137).
		fm = obj->mobj;
		if (fm->speed) {
			fwdScalar = (uint16_t)g_elapsedTicks * ((4660 * fm->speed + 128) >> 8) / 236;
		} else {
			fwdScalar = 0;
		}
		if (obj->playerOwnerIdx != -1) {
			uint8_t inputDisabledFlag = g_players[obj->playerOwnerIdx].inputDisabledFlag;
			if (inputDisabledFlag && inputDisabledFlag != 5) {
				fwdScalar = 0;
			}
		}

		switch (genus) {
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
			case GENUS_WeaponEmplacement: {
				CraftData* mc = fm->pCraft;
				if (mc->nextLinkObjectIdx == 0xFFFF) {
					AiController* aiCtrl = pai_GetEffectiveAIController(mc);
					MobileObject* vm;
					MobileObject* nm;
					FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
					if (obj->mobj->orientMatrixDirty) {
						FVIEW_calcrotateorient(obj->roll, obj->angleD, obj);
					}
					trig2_xmovedist = 0;
					trig2_ymovedist = 0;
					trig2_zmovedist = 0;
					vm = obj->mobj;
					if (vm->velocityOverrideActive) {
						Object_ComputeVelocityOverrideMove(obj);
						if (genus != GENUS_LargeScenery || obj->objectType == OBJ_MoltenBlock) {
							Object_DecayVelocityOverride(obj->mobj);
						}
					}
					nm = obj->mobj;
					if (!nm->velocityOverrideActive || nm->velocityOverrideDuration) {
						trig2_xmovedist += Xwa_Q15Mul((int16_t)nm->moveX, (uint16_t)fwdScalar);
						trig2_ymovedist += Xwa_Q15Mul((int16_t)obj->mobj->moveY, (uint16_t)fwdScalar);
						trig2_zmovedist += Xwa_Q15Mul((int16_t)obj->mobj->moveZ, (uint16_t)fwdScalar);
					}

					if (mc->workingSubsystems) {
						// Lateral/forward push accumulators (collision shove, AI evasion).
						int maxPushRate;
						int pax;
						int pay;
						int paz;
						if (obj->playerOwnerIdx == -1) {
							switch (aiCtrl->maneuverMode) {
								case 0x12:
								case 0x23:
									maxPushRate = 500;
									break;
								case 0x1E:
									maxPushRate = 750;
									break;
								case 0x22:
									maxPushRate = 5000;
									break;
								default:
									maxPushRate = g_modelDefs[mc->modelIndex].maxPushRate;
									break;
							}
						} else {
							maxPushRate = 1000;
						}

						pax = mc->pushAccumX;
						if (pax) {
							int clamp;
							int step;
							if (pax >= -maxPushRate) {
								clamp = (pax <= maxPushRate) ? pax / 2 : maxPushRate;
							} else {
								clamp = -maxPushRate;
							}
							step = (uint16_t)g_elapsedTicks * (int16_t)clamp / 236;
							if (!step) {
								step = ((int16_t)clamp <= 0) ? -1 : 1;
							}
							mc->pushAccumX = pax - step;
							trig2_xmovedist += step;
						}
						pay = mc->pushAccumY;
						if (pay) {
							int clamp;
							int step;
							if (pay >= -maxPushRate) {
								clamp = (pay <= maxPushRate) ? pay / 2 : maxPushRate;
							} else {
								clamp = -maxPushRate;
							}
							step = (uint16_t)g_elapsedTicks * (int16_t)clamp / 236;
							if (!step) {
								step = ((int16_t)clamp <= 0) ? -1 : 1;
							}
							mc->pushAccumY = pay - step;
							trig2_ymovedist += step;
						}
						paz = mc->pushAccumZ;
						if (paz) {
							int clamp = maxPushRate;
							int step;
							if (paz >= -maxPushRate) {
								if (paz <= maxPushRate) {
									clamp = paz / 2;
								}
							} else {
								clamp = -maxPushRate;
							}
							step = (uint16_t)g_elapsedTicks * (int16_t)clamp / 236;
							if (!step) {
								step = ((int16_t)clamp <= 0) ? -1 : 1;
							}
							if (aiCtrl->maneuverMode != 34) {
								mc->pushAccumZ = paz - step;
							}
							trig2_zmovedist += step;
						}
					}

					Object_ApplyTrigMoveDeltaAndClampWorldPosition(obj);

					// Position a carried object relative to this craft.
					if (mc->carriedObjectIndex != 0xFFFF) {
						int carriedIdx = mc->carriedObjectIndex;
						MobileObject* carriedMobj = g_objectTable[carriedIdx].mobj;
						if (carriedMobj) {
							int carriedModelIndex = carriedMobj->pCraft->modelIndex;
							int childOffset;
							int parentOffset;
							if (g_objectTable[carriedIdx].genusId > GENUS_Utility) {
								if (g_objectTable[objIdx].genusId > GENUS_Utility) {
									childOffset =
										g_modelDefs[carriedMobj->pCraft->modelIndex].meshAttachData[2];
								} else {
									childOffset =
										g_modelDefs[carriedMobj->pCraft->modelIndex].meshAttachData[1];
								}
								parentOffset = g_modelDefs[mc->modelIndex].meshAttachData[4];
							} else {
								childOffset = g_modelDefs[carriedMobj->pCraft->modelIndex].meshAttachData[1];
								parentOffset = g_modelDefs[mc->modelIndex].meshAttachData[3];
							}
							pai_RotateLocalVectorToWorldScratch(
								obj, 0, parentOffset - childOffset,
								g_modelDefs[carriedModelIndex].meshAttachData[0]);
							g_objectTable[carriedIdx].world_x = obj->world_x + g_rotatedX;
							g_objectTable[carriedIdx].world_y = obj->world_y + g_rotatedY;
							g_objectTable[carriedIdx].world_z = obj->world_z + g_rotatedZ;
						}
					}

					// Position the chain of linked objects (e.g. multi-segment craft).
					if (mc->nextLinkObjectIdx == 0xFFFF) {
						uint16_t linkedPrev = mc->linkedPrevObjectIdx;
						CraftData* parentCraft = mc;
						ObjectRecord* curLink = obj;
						while (linkedPrev != 0xFFFF) {
							int prevIdx = linkedPrev;
							CraftData* prevCraft = g_objectTable[prevIdx].mobj->pCraft;
							ModelIndex prevModelIndex = prevCraft->modelIndex;
							int mountX;
							int mountY;
							int mountZ;
							ObjectRecord* prevObj;
							pai_RotateLocalVectorToWorldScratch(
								curLink, (int16_t)g_modelDefs[parentCraft->modelIndex].childMountPoints[0],
								(int16_t)g_modelDefs[parentCraft->modelIndex].childMountPoints[1],
								(int16_t)g_modelDefs[parentCraft->modelIndex].childMountPoints[2]);
							mountX = g_rotatedX;
							mountY = g_rotatedY;
							mountZ = g_rotatedZ;
							prevObj = &g_objectTable[prevIdx];
							pai_RotateLocalVectorToWorldScratch(
								prevObj, (int16_t)g_modelDefs[prevModelIndex].childMountPoints[3],
								(int16_t)g_modelDefs[prevModelIndex].childMountPoints[4],
								(int16_t)g_modelDefs[prevModelIndex].childMountPoints[5]);
							prevObj->world_x = mountX + curLink->world_x - g_rotatedX;
							prevObj->world_y = mountY + curLink->world_y - g_rotatedY;
							prevObj->world_z = mountZ + curLink->world_z - g_rotatedZ;
							parentCraft = prevCraft;
							linkedPrev = prevCraft->linkedPrevObjectIdx;
							curLink = prevObj;
						}
					}
				}
				goto advance;
			}

			case GENUS_PlayerProjectile:
			case GENUS_NpcProjectile: {
				WarheadGuidanceState* pWG = fm->pWarheadGuidance;
				int homingTier;
				uint16_t tgtIdx;

				if (obj->objectType == OBJ_WarheadSpaceBomb) {
					if (fm->framesAlive < 2) {
						// Launch frames: ride the velocity override, then also advance by forward speed
						// (no speed ramp during launch).
						Object_ComputeVelocityOverrideMove(obj);
						Object_ApplyTrigMoveDeltaAndClampWorldPosition(obj);
						trig2_xmovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveX, (uint16_t)fwdScalar);
						trig2_ymovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveY, (uint16_t)fwdScalar);
						trig2_zmovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveZ, (uint16_t)fwdScalar);
						Object_ApplyTrigMoveDeltaAndClampWorldPosition(obj);
						goto advance;
					}
					if (fm->velocityOverrideActive) {
						// Boost phase: ramp speed toward the guidance minimum, then forward move.
						if (fm->velocityOverrideActive) {
							uint16_t speed = fm->speed;
							if (speed < pWG->minSpeed) {
								fm->speed =
									(uint16_t)(speed + (uint16_t)g_elapsedTicks *
														   g_projectileSpeedByType[OBJ_WarheadSpaceBomb -
																				   OBJ_LaserRebel] /
														   236);
							} else {
								fm->velocityOverrideActive = 0;
							}
						}
						trig2_xmovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveX, (uint16_t)fwdScalar);
						trig2_ymovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveY, (uint16_t)fwdScalar);
						trig2_zmovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveZ, (uint16_t)fwdScalar);
						Object_ApplyTrigMoveDeltaAndClampWorldPosition(obj);
						goto advance;
					}
				}

				homingTier = pWG->homingTier;
				if (homingTier) {
					tgtIdx = pWG->targetObjIdx;
					if (tgtIdx == 0xFFFF) {
						goto projectile_forward_move;
					}
					if (tgtIdx >= g_regionMainObjectSlotStart && tgtIdx < g_regionMainObjectSlotEnd) {
						ObjectRecord* tgtObj = &g_objectTable[tgtIdx];
						if (tgtObj->objectType == OBJ_None ||
							tgtObj->objectSignature != pWG->targetSignature) {
							goto projectile_detonate;
						}
					}

					{
						uint16_t tgtComp = pWG->targetComponentIdx;
						uint16_t profile =
							(uint8_t)g_projectileHomingProfileBaseByObjectType[obj->objectType] + homingTier;
						int targetIsActiveBeam = 0;

						if (tgtIdx >= g_activeRegionObjectSlotStart &&
							tgtIdx < g_activeRegionCraftObjectSlotEnd) {
							CraftData* tgtCraft = g_objectTable[tgtIdx].mobj->pCraft;
							if (tgtCraft->beamTypeId == 3 && tgtCraft->beamActive) {
								targetIsActiveBeam = 1;
							}
							if (tgtComp != 0xFFFF && tgtCraft->componentHp[tgtComp] == 0) {
								goto projectile_detonate;
							}
						}

						if (!targetIsActiveBeam) {
							uint16_t yaw;
							uint16_t yawDeltaU;
							int16_t yawDelta;
							int turnStep;
							int absYaw;
							uint16_t pitch;
							uint16_t pitchDeltaU;
							int16_t pitchDelta;
							int pitchStep;
							int absPitch;

							// Resolve aim point toward the target (or a specific mesh component).
							Mission_ResolveObjectOrMissionPointWorldLoc(tgtIdx, 0, 0, 0);
							if (tgtIdx < g_activeRegionObjectSlotStart ||
								tgtIdx >= g_activeRegionCraftObjectSlotEnd ||
								g_objectTable[tgtIdx].mobj->state) {
								g_rotatedX = worldlocx;
								g_rotatedY = worldlocy;
								g_rotatedZ = worldlocz;
							} else {
								int centerX;
								int centerZ;
								int negCenterY;
								if (tgtComp != 0xFFFF) {
									negCenterY =
										-ModelMesh_GetCenterY(g_objectTable[tgtIdx].objectType, tgtComp);
									centerZ = ModelMesh_GetCenterZ(g_objectTable[tgtIdx].objectType, tgtComp);
									centerX = ModelMesh_GetCenterX(g_objectTable[tgtIdx].objectType, tgtComp);
								} else {
									negCenterY = -ModelMesh_GetCenterY(g_objectTable[tgtIdx].objectType, 0);
									centerZ = ModelMesh_GetCenterZ(g_objectTable[tgtIdx].objectType, 0);
									centerX = ModelMesh_GetCenterX(g_objectTable[tgtIdx].objectType, 0);
								}
								pai_RotateLocalVectorToWorldScratch(&g_objectTable[tgtIdx], centerX, centerZ,
																	negCenterY);
								g_rotatedX += worldlocx;
								g_rotatedY += worldlocy;
								g_rotatedZ += worldlocz;
							}
							Mission_ResolveObjectOrMissionPointWorldLoc(objIdx, 0, 0, 0);
							g_rotatedX -= worldlocx;
							g_rotatedY -= worldlocy;
							g_rotatedZ -= worldlocz;
							trig2_ctop(g_rotatedX, g_rotatedY, g_rotatedZ);

							// Yaw homing.
							yaw = obj->yaw;
							yawDeltaU = (uint16_t)(trig2_xyangle - yaw);
							yawDelta = (int16_t)yawDeltaU;
							turnStep =
								g_projectileHomingTurnRateByProfile[profile] * (uint16_t)g_elapsedTicks / 236;
							absYaw = (yawDelta >= 0) ? yawDelta : -yawDelta;
							if (absYaw <= (int)(uint16_t)turnStep) {
								obj->yaw = trig2_xyangle;
								if (obj->mobj->speed < pWG->minSpeed) {
									obj->mobj->speed =
										(uint16_t)(obj->mobj->speed +
												   (uint16_t)g_elapsedTicks *
													   g_projectileHomingSpeedAdjustRateByProfile[profile] /
													   236);
								}
							} else {
								int step = (uint16_t)turnStep;
								uint16_t spd;
								if (yawDelta < 0) {
									step = -step;
								}
								obj->yaw = (uint16_t)(yaw + step);
								spd = obj->mobj->speed;
								if (spd > 0xC8) {
									obj->mobj->speed =
										(uint16_t)(spd -
												   (uint16_t)g_elapsedTicks *
													   g_projectileHomingSpeedAdjustRateByProfile[profile] /
													   236);
									if (obj->mobj->speed < 0xC8) {
										obj->mobj->speed = 200;
									}
								}
							}

							// Pitch homing.
							pitch = obj->pitch;
							pitchDeltaU = (uint16_t)(targetPitch - pitch);
							pitchDelta = (int16_t)pitchDeltaU;
							pitchStep =
								g_projectileHomingTurnRateByProfile[profile] * (uint16_t)g_elapsedTicks / 236;
							absPitch = (pitchDelta >= 0) ? pitchDelta : -pitchDelta;
							if (absPitch <= (int)(uint16_t)pitchStep) {
								obj->pitch = targetPitch;
							} else {
								if (pitchDelta < 0) {
									pitchStep = -pitchStep;
								}
								obj->pitch = (uint16_t)(pitch + pitchStep);
							}

							obj->mobj->orientMatrixDirty = 1;
							obj->mobj->moveVectorDirty = 1;
							FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
							obj->mobj->moveX = (int16_t)g_fviewMoveX_Q15;
							obj->mobj->moveY = (int16_t)g_fviewMoveY_Q15;
							obj->mobj->moveZ = (int16_t)g_fviewMoveZ_Q15;
						}
					}
				}

			projectile_forward_move:
				if (obj->mobj->moveVectorDirty) {
					FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
				}
				trig2_xmovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveX, (uint16_t)fwdScalar);
				trig2_ymovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveY, (uint16_t)fwdScalar);
				trig2_zmovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveZ, (uint16_t)fwdScalar);
				Object_ApplyTrigMoveDeltaAndClampWorldPosition(obj);
				goto advance;

			projectile_detonate:
				collide_ApplyDefaultProximityDamage(objIdx, g_objectTable[objIdx].mobj->damageAmount, 0xFFFF);
				collide_ConvertObjectToExplosion(objIdx, (ObjectTypeId)((GameRand() & 3) + 266), 1);
				goto advance;
			}

			case GENUS_Debris:
			case GENUS_Explosion:
			case GENUS_Rubble:
				if (fm->moveVectorDirty) {
					FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
				}
				trig2_xmovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveX, (uint16_t)fwdScalar);
				trig2_ymovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveY, (uint16_t)fwdScalar);
				trig2_zmovedist = Xwa_Q15Mul((int16_t)obj->mobj->moveZ, (uint16_t)fwdScalar);
				Object_ApplyTrigMoveDeltaAndClampWorldPosition(obj);
				goto advance;

			case GENUS_SalvageJunk: {
				MobileObject* svm;
				MobileObject* snm;
				trig2_xmovedist = 0;
				trig2_ymovedist = 0;
				trig2_zmovedist = 0;
				FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
				if (obj->mobj->orientMatrixDirty) {
					FVIEW_calcrotateorient(obj->roll, obj->angleD, obj);
				}
				svm = obj->mobj;
				if (svm->velocityOverrideActive) {
					Object_ComputeVelocityOverrideMove(obj);
					if (obj->objectType == OBJ_MoltenBlock) {
						Object_DecayVelocityOverride(obj->mobj);
					}
				}
				snm = obj->mobj;
				if (!snm->velocityOverrideActive || snm->velocityOverrideDuration) {
					trig2_xmovedist += Xwa_Q15Mul((int16_t)snm->moveX, (uint16_t)fwdScalar);
					trig2_ymovedist += Xwa_Q15Mul((int16_t)obj->mobj->moveY, (uint16_t)fwdScalar);
					trig2_zmovedist += Xwa_Q15Mul((int16_t)obj->mobj->moveZ, (uint16_t)fwdScalar);
				}
				Object_ApplyTrigMoveDeltaAndClampWorldPosition(obj);
				goto advance;
			}

			default:
				goto advance;
		}

	advance:
		if ((uint16_t)(objIdx + 1) >= g_regionMainObjectSlotEnd) {
			break;
		}
		objIdx = (uint16_t)(objIdx + 1);
		elapsedTicks = savedElapsedTicks;
	}

	g_simStepScale = (uint16_t)savedSimStepScale;
	g_elapsedTicks = savedElapsedTicks;
}
