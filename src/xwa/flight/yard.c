#include "xwa/flight/yard.h"
#include "xwa/flight/hangar.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/assets/string_table.h"
#include "xwa/audio/sound.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_light.h"
#include "xwa/audio/fsfx.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/math/fixed.h"
#include "xwa/math/scalar.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/net_session.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/memory.h"
#include "xwa/util/random.h"
#include "xwa/util/time.h"
#ifdef XWA_MODERN
#include "xwa_runtime/timing/modern_flight_timing.h"
#endif

#include <stdlib.h>
#ifndef XWA_MODERN
#include <stdio.h>
#endif
#include <string.h>

#define YARD_CRAFT_SCORE_TABLE_TAG "YARDCRAFTSCORE"
#define YARD_CRAFT_SCORE_TABLE_FREE_TAG "CRAFTSCORETABLE"
#define YARD_CRAFT_SCORE_TABLES_TAG "CRAFTSCORETABLES"
#define YARD_HIGH_SCORE_TABLE_TAG "HISCORETABLE"

enum {
	YARD_SCORE_CATEGORY_COUNT = 8,
	YARD_SCORE_ENTRIES_PER_CATEGORY = 10,
	YARD_SCORE_PILOT_NAME_COPY_LENGTH = 13,
	YARD_SCORE_PILOT_NAME_SLOT_LENGTH = 14
};

// GLOBAL: XWA 0x7B2320
YardContext g_yardContext;
// GLOBAL: XWA 0x782960
int g_yardRubbleChunkSpawnLimit;
// GLOBAL: XWA 0x9C6F70
int g_yardFinishPlacementResultCode;
// GLOBAL: XWA 0x6005C8
const uint8_t g_yardCheckpointsPerLapByChallengeMode[8] = { 60, 60, 60, 60, 60, 120, 60, 120 };
// GLOBAL: XWA 0x7828DC
int g_yardChallengeLapCount;
// GLOBAL: XWA 0x7828F4
int g_yardStopCheatingMessageShown;
// GLOBAL: XWA 0x9C6FB0
int g_yardFinishPlacementMessagePending;
// GLOBAL: XWA 0x782910
int g_yardCompactorHintShown;
// GLOBAL: XWA 0x7828E4
int g_yardWatchLasersHintShown;
// GLOBAL: XWA 0x782964
int g_yardFinishMessageShown;
// GLOBAL: XWA 0x782970
int g_yardStartMessageShown;
// GLOBAL: XWA 0x78292C
int g_yardAlmostDoneHintShown;
// GLOBAL: XWA 0x782908
int g_yardLastCourseWarningGameTime;
// GLOBAL: XWA 0x782974
int g_yardLastSafeCourseGameTime;
// GLOBAL: XWA 0x7828FC
int g_yardDontStayLongHintShown;
// GLOBAL: XWA 0x782920
int g_yardChallengeEventTimer;
// GLOBAL: XWA 0x78291C
int g_yardR2D2BumpSfxTimer;
#ifdef XWA_MODERN
static __inline int Yard_ModernIsLegacyCadenceDue(void) {
	return !XwaModernFlightTiming_IsHighRate() || XwaModernFlightTiming_IsLegacyCadenceDue();
}
#endif
// GLOBAL: XWA 0x782928
int g_yardShuttleObjIdx;
// GLOBAL: XWA 0x7828F0
int g_yardCourseSide1ToSide2GapDist;
// GLOBAL: XWA 0x7828D8
int g_yardCourseSide2ToSide1GapDist;
// GLOBAL: XWA 0x782904
int g_yardChuteMouthObjIdx;
// GLOBAL: XWA 0x78290C
int g_yardChuteTunnelEndObjIdx;
// GLOBAL: XWA 0x7828E0
int g_yardCourseSide1FinalObjIdx;
// GLOBAL: XWA 0x782900
int g_yardCourseSide2FinalObjIdx;
// GLOBAL: XWA 0x782918
int g_yardSalvageRoomObjIdx;
// GLOBAL: XWA 0x782914
int g_yardSmeltingRoomAsteroidObjIdx;
// GLOBAL: XWA 0x7828F8
int g_yardCentrifugeObjIdx;
// GLOBAL: XWA 0x782930
int g_yardCentrifugeMoltenBlockSpawnWorldZ[4];
// GLOBAL: XWA 0x782940
int g_yardCentrifugeMoltenBlockSpawnWorldY[4];
// GLOBAL: XWA 0x782950
int g_yardCentrifugeMoltenBlockSpawnWorldX[4];
// GLOBAL: XWA 0x78295C
int g_yardContainerGrandeObjIdx;
// GLOBAL: XWA 0x7828E8
int g_yardAccelRingCullAnchorObjIdx;
// GLOBAL: XWA 0x782978
int g_yardAdvancedCourseTubeFirstObjIdx;
// GLOBAL: XWA 0x782968
int g_yardAdvancedCourseTubeLastObjIdx;
// GLOBAL: XWA 0x78296C
int g_yardR2D2ObjIdx;
// GLOBAL: XWA 0x7828EC
int g_yardBuildParentObjIdx;
// GLOBAL: XWA 0x782924
uint8_t g_yardSpawnFlightGroupIdx;
// GLOBAL: XWA 0x5AA108
const float g_yardCentrifugeDamageScale = 0.5f;
// GLOBAL: XWA 0x5AA0EC
const float g_yardTurretLeadTimeScale = 4.0f;
// GLOBAL: XWA 0x6005C0
const uint8_t g_yardCompactorMeshOpenLimits[5] = { 0u, 0x20u, 0x40u, 0x20u, 0x20u };

// FUNCTION: XWA 0x511E40
void Yard_InitChallengeScene(void) {
	uint8_t challengeMode;
	int lapCount;
	int parentObjIdx;
	uint16_t objectIdx;
	uint16_t playerIdx;

	memset(&g_yardContext, 0, sizeof(g_yardContext));
	challengeMode = g_yardChallengeMode;
	if (g_flightPlayerCount == 1) {
		*(int*)g_pilotData.provingGroundsMissionPerPlayer = challengeMode;
	}

	if (challengeMode <= 2u) {
		if (g_flightPlayerCount == 1) {
			lapCount = 2;
		} else {
			lapCount = g_gameConfig.laps + 1;
		}
	} else {
		lapCount = 1;
	}
	g_yardChallengeLapCount = lapCount;

	for (playerIdx = 0; playerIdx < 8u; ++playerIdx) {
		YardPlayerChallengeState* state;
		PlayerData* player;

		state = &g_yardContext.playerChallengeStates[playerIdx];
		player = &g_players[playerIdx];
		state->objectIdx = player->objectIndex;
		state->currentCheckpointIdx = -1;
		state->currentCourseSide = 1;
		state->nextCheckpointIdx = 0;
		state->nextCourseSide = 1;
		state->ringCheckpointHit = 0;
		state->chuteCheckpointHit = 0;
		state->carriedObjectPickedUp = 0;
		state->carriedObjectDelivered = 0;
		state->finished = 0;
		state->finishTimeSeconds = 0;
		state->field_38 = 0;
		state->lapsRemaining = lapCount;
		state->remainingCheckpointCount = lapCount * g_yardCheckpointsPerLapByChallengeMode[challengeMode];
		state->courseState = challengeMode < 3u ? 3 : 0;
		state->recoveryCollisionObjIdx = 0;
		state->recoveryWorldX = 0;
		player->mfd.enabled[1] = 1;
		state->recoveryWorldY = 0;
		player->mfd.enabled[2] = 1;
		state->recoveryWorldZ = 0;
		player->mfd.page[1] = 0;
		state->recoveryYaw = 0;
		player->mfd.page[2] = 1;
		state->recoveryPitch = 0;

		if (!g_useHardware3D) {
			Hud_SetHudEnabled(playerIdx, 1);
			FlightSurface_Lock();
			FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			g_flightFillClipRectFn();
			FlightSurface_Unlock();
			challengeMode = g_yardChallengeMode;
			lapCount = g_yardChallengeLapCount;
		}
	}

	g_yardContext.rubbleChunkStateCount = 0;
	g_yardContext.smeltingJunkStateCount = 0;
	g_yardContext.centrifugeContainerStateCount = 0;
	g_yardCourseSide1FinalObjIdx = 0xffff;
	g_yardCourseSide2FinalObjIdx = 0xffff;
	g_collideSweepAllowUnownedTargets = 1;
	g_collideRicochetDamageScale = 0.25f;
	g_yardStopCheatingMessageShown = 0;
	g_yardFinishPlacementMessagePending = 0;
	g_yardCompactorHintShown = abs(abs(GameRand2() & 0xffff) & 3) == 0;
	g_yardWatchLasersHintShown = abs(abs(GameRand2() & 0xffff) & 3) == 0;
	g_yardDontStayLongHintShown = abs(abs(GameRand2() & 0xffff) & 3) == 0;
	g_yardFinishMessageShown = 0;
	g_yardStartMessageShown = 0;
	g_yardAlmostDoneHintShown = 0;
	g_yardLastCourseWarningGameTime = 0;
	g_yardLastSafeCourseGameTime = 0;
	g_yardContext.countdownSecondsRemaining = 10;
	g_yardChallengeEventTimer = 0;
	g_yardSpawnFlightGroupIdx = (uint8_t)(g_missionHeader.numFlightGroups - 1u);
	g_yardR2D2BumpSfxTimer = 0;
	g_yardR2D2ObjIdx = 0xffff;
	g_yardShuttleObjIdx = 0xffff;

	for (objectIdx = 0; objectIdx < g_objectSlotsPerRegion; ++objectIdx) {
		ObjectRecord* object;

		object = &g_objectTable[objectIdx];
		if (object->objectType == OBJ_R2D2) {
			g_yardR2D2ObjIdx = objectIdx;
		} else if (object->objectType == OBJ_Shuttle) {
			g_yardShuttleObjIdx = objectIdx;
		} else if (object->objectType != OBJ_None && object->flightGroupIdx == g_yardSpawnFlightGroupIdx) {
			object->objectType = OBJ_None;
			if (objectIdx >= g_activeRegionObjectSlotStart && objectIdx < g_activeRegionCraftObjectSlotEnd &&
				object->mobj != NULL && object->mobj->pCraft != NULL) {
				Craft_ClearEffectiveAiObjectLink(object->mobj->pCraft);
			}
		}
	}

	g_missionFlightGroups[g_yardSpawnFlightGroupIdx].playerOwnerIdx = -1;
	memcpy(g_missionFlightGroups[g_yardSpawnFlightGroupIdx].fg.name, "Salv", 4);
	strcpy(&g_missionFlightGroups[g_yardSpawnFlightGroupIdx].fg.name[4], "age Yard");

	if (g_yardChallengeMode < 6u && g_yardR2D2ObjIdx != 0xffff) {
		g_objectTable[g_yardR2D2ObjIdx].objectType = OBJ_None;
		g_yardR2D2ObjIdx = 0xffff;
	}

	switch (g_yardChallengeMode) {
		case 0: {
			uint16_t checkpointIdx;

			parentObjIdx = g_yardBuildParentObjIdx;
			for (checkpointIdx = 0; checkpointIdx < 30u; ++checkpointIdx) {
				YardCourseCheckpointState* checkpoint;
				ObjectTypeId ringType;
				int16_t yaw;
				int16_t pitch;
				ModelIndex modelIndex;
				ObjectRecord* object;

				if (checkpointIdx < 4u) {
					yaw = -256;
					pitch = 0;
				} else if (checkpointIdx < 8u) {
					yaw = -4096;
					pitch = 0;
				} else if (checkpointIdx < 10u) {
					yaw = 2048;
					pitch = -1024;
				} else if (checkpointIdx < 16u) {
					yaw = 256;
					pitch = 1024;
				} else if (checkpointIdx < 20u) {
					yaw = 512;
					pitch = -4096;
				} else if (checkpointIdx < 22u) {
					yaw = -256;
					pitch = 4096;
				} else if (checkpointIdx < 24u) {
					yaw = -4096;
					pitch = 1280;
				} else if (checkpointIdx < 28u) {
					yaw = -4096;
					pitch = -1280;
				} else {
					yaw = 4096;
					pitch = 256;
				}

				ringType = (checkpointIdx % 4) != 0 ? OBJ_AccelRing : OBJ_AccelRing2;
				checkpoint = &g_yardContext.courseSide1Checkpoints[checkpointIdx];
				if (checkpointIdx == 0) {
					checkpoint->objectIdx =
						Yard_SpawnObjectAtWorldPos(ringType, 0, 0, 0, (int16_t)0x8000, (int16_t)0x4000);
					checkpoint->prevObjectIdx = 0xffff;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 0;
				} else {
					checkpoint->objectIdx = Yard_SpawnChildAtMount(ringType, parentObjIdx, 1, yaw, pitch, 1);
					checkpoint->prevObjectIdx = g_yardBuildParentObjIdx;
					g_yardContext.courseSide1Checkpoints[checkpointIdx - 1].nextObjectIdx =
						checkpoint->objectIdx;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 1;
				}
				checkpoint->field04 = 2;
				g_yardBuildParentObjIdx = checkpoint->objectIdx;
				modelIndex = GetModelIndexFromType(ringType);
				pai_RotateLocalVectorToWorldScratch(
					&g_objectTable[g_yardBuildParentObjIdx], g_modelDefs[modelIndex].childMountPoints[3],
					g_modelDefs[modelIndex].childMountPoints[4], g_modelDefs[modelIndex].childMountPoints[5]);
				parentObjIdx = g_yardBuildParentObjIdx;
				object = &g_objectTable[g_yardBuildParentObjIdx];
				checkpoint->checkpointWorldX = g_rotatedX + object->world_x;
				checkpoint->checkpointWorldY = g_rotatedY + object->world_y;
				checkpoint->checkpointWorldZ = g_rotatedZ + object->world_z;
				checkpoint->fireCooldownTicks = 0;
				checkpoint->launcherIdx = 0;
			}
			g_yardCourseSide1FinalObjIdx = parentObjIdx;
			g_yardContext.courseSide1Checkpoints[parentObjIdx].nextObjectIdx = 0xffff;

			for (checkpointIdx = 0; checkpointIdx < 30u; ++checkpointIdx) {
				YardCourseCheckpointState* checkpoint;
				ObjectTypeId ringType;
				int16_t yaw;
				int16_t pitch;
				ModelIndex modelIndex;
				ObjectRecord* object;

				if (checkpointIdx < 8u) {
					yaw = -3184;
					pitch = 1280;
				} else if (checkpointIdx < 10u) {
					yaw = -768;
					pitch = 512;
				} else if (checkpointIdx < 16u) {
					yaw = 4096;
					pitch = -768;
				} else if (checkpointIdx < 20u) {
					yaw = -4608;
					pitch = 0;
				} else if (checkpointIdx < 22u) {
					yaw = -2048;
					pitch = 3840;
				} else if (checkpointIdx < 24u) {
					yaw = -1616;
					pitch = -1280;
				} else if (checkpointIdx < 26u) {
					yaw = 4096;
					pitch = -256;
				} else {
					yaw = -3840;
					pitch = -1600;
				}

				ringType = (checkpointIdx % 4) != 0 ? OBJ_AccelRing : OBJ_AccelRing2;
				checkpoint = &g_yardContext.courseSide2Checkpoints[checkpointIdx];
				checkpoint->objectIdx = Yard_SpawnChildAtMount(ringType, parentObjIdx, 1, yaw, pitch, 1);
				if (checkpointIdx == 0) {
					checkpoint->prevObjectIdx = 0xffff;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 0;
				} else {
					checkpoint->prevObjectIdx = g_yardBuildParentObjIdx;
					g_yardContext.courseSide2Checkpoints[checkpointIdx - 1].nextObjectIdx =
						checkpoint->objectIdx;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 1;
				}
				g_yardBuildParentObjIdx = checkpoint->objectIdx;
				checkpoint->field04 = 2;
				modelIndex = GetModelIndexFromType(ringType);
				pai_RotateLocalVectorToWorldScratch(
					&g_objectTable[g_yardBuildParentObjIdx], g_modelDefs[modelIndex].childMountPoints[3],
					g_modelDefs[modelIndex].childMountPoints[4], g_modelDefs[modelIndex].childMountPoints[5]);
				parentObjIdx = g_yardBuildParentObjIdx;
				object = &g_objectTable[g_yardBuildParentObjIdx];
				checkpoint->checkpointWorldX = g_rotatedX + object->world_x;
				checkpoint->checkpointWorldY = g_rotatedY + object->world_y;
				checkpoint->checkpointWorldZ = g_rotatedZ + object->world_z;
				checkpoint->fireCooldownTicks = 0;
				checkpoint->launcherIdx = 0;
				checkpoint->secondaryLauncherIdx = 3;
			}
			g_yardCourseSide2FinalObjIdx = parentObjIdx;
			g_yardContext.courseSide2Checkpoints[parentObjIdx].nextObjectIdx = 0xffff;
			break;
		}

		case 1: {
			uint16_t checkpointIdx;

			parentObjIdx = g_yardBuildParentObjIdx;
			for (checkpointIdx = 0; checkpointIdx < 30u; ++checkpointIdx) {
				YardCourseCheckpointState* checkpoint;
				ObjectTypeId ringType;
				int16_t yaw;
				int16_t pitch;
				ModelIndex modelIndex;
				ObjectRecord* object;

				if (checkpointIdx < 4u) {
					yaw = -256;
					pitch = 0;
				} else if (checkpointIdx < 8u) {
					yaw = -4096;
					pitch = 0;
				} else if (checkpointIdx < 10u) {
					yaw = 2048;
					pitch = -1024;
				} else if (checkpointIdx < 16u) {
					yaw = 256;
					pitch = 1024;
				} else if (checkpointIdx < 20u) {
					yaw = 512;
					pitch = -4096;
				} else if (checkpointIdx < 22u) {
					yaw = -256;
					pitch = 4096;
				} else if (checkpointIdx < 24u) {
					yaw = -4096;
					pitch = 1280;
				} else if (checkpointIdx < 28u) {
					yaw = -4096;
					pitch = -1280;
				} else {
					yaw = 4096;
					pitch = 256;
				}

				ringType = (checkpointIdx % 4) != 0 ? OBJ_AccelRing : OBJ_AccelRing2;
				checkpoint = &g_yardContext.courseSide1Checkpoints[checkpointIdx];
				if (checkpointIdx == 0) {
					checkpoint->objectIdx =
						Yard_SpawnObjectAtWorldPos(ringType, 0, 0, 0, (int16_t)0x8000, (int16_t)0x4000);
					checkpoint->prevObjectIdx = 0xffff;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 0;
				} else {
					checkpoint->objectIdx = Yard_SpawnChildAtMount(ringType, parentObjIdx, 1, yaw, pitch, 1);
					checkpoint->prevObjectIdx = g_yardBuildParentObjIdx;
					g_yardContext.courseSide1Checkpoints[checkpointIdx - 1].nextObjectIdx =
						checkpoint->objectIdx;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 1;
				}
				checkpoint->field04 = 2;
				g_yardBuildParentObjIdx = checkpoint->objectIdx;
				modelIndex = GetModelIndexFromType(ringType);
				pai_RotateLocalVectorToWorldScratch(
					&g_objectTable[g_yardBuildParentObjIdx], g_modelDefs[modelIndex].childMountPoints[3],
					g_modelDefs[modelIndex].childMountPoints[4], g_modelDefs[modelIndex].childMountPoints[5]);
				parentObjIdx = g_yardBuildParentObjIdx;
				object = &g_objectTable[g_yardBuildParentObjIdx];
				checkpoint->checkpointWorldX = g_rotatedX + object->world_x;
				checkpoint->checkpointWorldY = g_rotatedY + object->world_y;
				checkpoint->checkpointWorldZ = g_rotatedZ + object->world_z;
				checkpoint->fireCooldownTicks = 0;
				checkpoint->launcherIdx = 0;
			}
			g_yardCourseSide1FinalObjIdx = parentObjIdx;
			g_yardContext.courseSide1Checkpoints[parentObjIdx].nextObjectIdx = 0xffff;

			for (checkpointIdx = 0; checkpointIdx < 30u; ++checkpointIdx) {
				YardCourseCheckpointState* checkpoint;
				ObjectTypeId ringType;
				int16_t yaw;
				int16_t pitch;
				ModelIndex modelIndex;
				ObjectRecord* object;

				if (checkpointIdx < 8u) {
					yaw = -3184;
					pitch = 1280;
				} else if (checkpointIdx < 10u) {
					yaw = -768;
					pitch = 512;
				} else if (checkpointIdx < 16u) {
					yaw = 4096;
					pitch = -768;
				} else if (checkpointIdx < 20u) {
					yaw = -4608;
					pitch = 0;
				} else if (checkpointIdx < 22u) {
					yaw = -2048;
					pitch = 3840;
				} else if (checkpointIdx < 24u) {
					yaw = -1616;
					pitch = -1280;
				} else if (checkpointIdx < 26u) {
					yaw = 4096;
					pitch = -256;
				} else {
					yaw = -3840;
					pitch = -1600;
				}

				ringType = (checkpointIdx % 4) != 0 ? OBJ_AccelRing : OBJ_AccelRing3;
				checkpoint = &g_yardContext.courseSide2Checkpoints[checkpointIdx];
				checkpoint->objectIdx = Yard_SpawnChildAtMount(ringType, parentObjIdx, 1, yaw, pitch, 1);
				if (checkpointIdx == 0) {
					checkpoint->prevObjectIdx = 0xffff;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 0;
				} else {
					checkpoint->prevObjectIdx = g_yardBuildParentObjIdx;
					g_yardContext.courseSide2Checkpoints[checkpointIdx - 1].nextObjectIdx =
						checkpoint->objectIdx;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 1;
				}
				g_yardBuildParentObjIdx = checkpoint->objectIdx;
				checkpoint->field04 = 2;
				modelIndex = GetModelIndexFromType(ringType);
				pai_RotateLocalVectorToWorldScratch(
					&g_objectTable[g_yardBuildParentObjIdx], g_modelDefs[modelIndex].childMountPoints[3],
					g_modelDefs[modelIndex].childMountPoints[4], g_modelDefs[modelIndex].childMountPoints[5]);
				parentObjIdx = g_yardBuildParentObjIdx;
				object = &g_objectTable[g_yardBuildParentObjIdx];
				checkpoint->checkpointWorldX = g_rotatedX + object->world_x;
				checkpoint->checkpointWorldY = g_rotatedY + object->world_y;
				checkpoint->checkpointWorldZ = g_rotatedZ + object->world_z;
				checkpoint->fireCooldownTicks = 0;
				checkpoint->launcherIdx = 0;
				checkpoint->secondaryLauncherIdx = 3;
			}
			g_yardCourseSide2FinalObjIdx = parentObjIdx;
			g_yardContext.courseSide2Checkpoints[parentObjIdx].nextObjectIdx = 0xffff;
			break;
		}

		case 2: {
			uint16_t checkpointIdx;

			parentObjIdx = g_yardBuildParentObjIdx;
			for (checkpointIdx = 0; checkpointIdx < 30u; ++checkpointIdx) {
				YardCourseCheckpointState* checkpoint;
				ObjectTypeId ringType;
				int16_t yaw;
				int16_t pitch;
				ModelIndex modelIndex;
				ObjectRecord* object;

				if (checkpointIdx < 4u) {
					yaw = 864;
					pitch = 0;
				} else if (checkpointIdx < 10u) {
					yaw = 1872;
					pitch = 0;
				} else if (checkpointIdx < 20u) {
					yaw = 1584;
					pitch = 0;
				} else if (checkpointIdx < 27u) {
					yaw = 2192;
					pitch = 0;
				} else {
					yaw = 768;
					pitch = 544;
				}

				ringType = (checkpointIdx % 4) != 0 ? OBJ_AccelRing : OBJ_AccelRing2;
				checkpoint = &g_yardContext.courseSide1Checkpoints[checkpointIdx];
				if (checkpointIdx == 0) {
					checkpoint->objectIdx =
						Yard_SpawnObjectAtWorldPos(ringType, 0, 0, 0, (int16_t)0x8000, (int16_t)0x4000);
					checkpoint->prevObjectIdx = 0xffff;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 0;
				} else {
					checkpoint->objectIdx = Yard_SpawnChildAtMount(ringType, parentObjIdx, 1, yaw, pitch, 1);
					checkpoint->prevObjectIdx = g_yardBuildParentObjIdx;
					g_yardContext.courseSide1Checkpoints[checkpointIdx - 1].nextObjectIdx =
						checkpoint->objectIdx;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 1;
				}
				checkpoint->field04 = 2;
				g_yardBuildParentObjIdx = checkpoint->objectIdx;
				modelIndex = GetModelIndexFromType(ringType);
				pai_RotateLocalVectorToWorldScratch(
					&g_objectTable[g_yardBuildParentObjIdx], g_modelDefs[modelIndex].childMountPoints[3],
					g_modelDefs[modelIndex].childMountPoints[4], g_modelDefs[modelIndex].childMountPoints[5]);
				parentObjIdx = g_yardBuildParentObjIdx;
				object = &g_objectTable[g_yardBuildParentObjIdx];
				checkpoint->checkpointWorldX = g_rotatedX + object->world_x;
				checkpoint->checkpointWorldY = g_rotatedY + object->world_y;
				checkpoint->checkpointWorldZ = g_rotatedZ + object->world_z;
				checkpoint->fireCooldownTicks = 0;
				checkpoint->launcherIdx = 0;
			}
			g_yardCourseSide1FinalObjIdx = parentObjIdx;
			g_yardContext.courseSide1Checkpoints[parentObjIdx].nextObjectIdx = 0xffff;

			parentObjIdx = Yard_SpawnChildAtMount(OBJ_Asteroid03, parentObjIdx, 1, 0, 0, 1);
			g_yardBuildParentObjIdx = parentObjIdx;
			g_yardAccelRingCullAnchorObjIdx = parentObjIdx;

			for (checkpointIdx = 0; checkpointIdx < 30u; ++checkpointIdx) {
				YardCourseCheckpointState* checkpoint;
				ObjectTypeId ringType;
				int16_t yaw;
				int16_t pitch;
				ModelIndex modelIndex;
				ObjectRecord* object;

				if (checkpointIdx < 4u) {
					yaw = -1536;
					pitch = -512;
				} else if (checkpointIdx < 27u) {
					yaw = -1829;
					pitch = 0;
				} else {
					yaw = -1888;
					pitch = 768;
				}

				ringType = (checkpointIdx % 4) != 0 ? OBJ_AccelRing : OBJ_AccelRing3;
				checkpoint = &g_yardContext.courseSide2Checkpoints[checkpointIdx];
				checkpoint->objectIdx = Yard_SpawnChildAtMount(ringType, parentObjIdx, 1, yaw, pitch, 1);
				if (checkpointIdx == 0) {
					checkpoint->prevObjectIdx = 0xffff;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 0;
				} else {
					checkpoint->prevObjectIdx = g_yardBuildParentObjIdx;
					g_yardContext.courseSide2Checkpoints[checkpointIdx - 1].nextObjectIdx =
						checkpoint->objectIdx;
					g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 1;
				}
				g_yardBuildParentObjIdx = checkpoint->objectIdx;
				checkpoint->field04 = 2;
				modelIndex = GetModelIndexFromType(ringType);
				pai_RotateLocalVectorToWorldScratch(
					&g_objectTable[g_yardBuildParentObjIdx], g_modelDefs[modelIndex].childMountPoints[3],
					g_modelDefs[modelIndex].childMountPoints[4], g_modelDefs[modelIndex].childMountPoints[5]);
				parentObjIdx = g_yardBuildParentObjIdx;
				object = &g_objectTable[g_yardBuildParentObjIdx];
				checkpoint->checkpointWorldX = g_rotatedX + object->world_x;
				checkpoint->checkpointWorldY = g_rotatedY + object->world_y;
				checkpoint->checkpointWorldZ = g_rotatedZ + object->world_z;
				checkpoint->fireCooldownTicks = 0;
				checkpoint->launcherIdx = 0;
				checkpoint->secondaryLauncherIdx = 3;
			}
			g_yardCourseSide2FinalObjIdx = parentObjIdx;
			g_yardContext.courseSide2Checkpoints[parentObjIdx].nextObjectIdx = 0xffff;
			break;
		}

		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			Yard_BuildAdvancedChallengeCourse();
			break;

		default:
			break;
	}

	if (g_yardChallengeMode >= 6u && g_yardR2D2ObjIdx != 0xffff) {
		int parentType;
		ModelIndex modelIndex;
		int worldX;
		int worldY;
		int worldZ;

		switch (g_yardChallengeMode) {
			case 6:
				parentObjIdx = g_yardContext.smeltingRoomObjIdx;
				modelIndex = GetModelIndexFromType(OBJ_SmeltingRoom);
				break;

			case 7:
				parentObjIdx = g_yardContainerGrandeObjIdx;
				modelIndex = GetModelIndexFromType(OBJ_ContainerGrandePG);
				break;

			default:
				parentType = playerIdx;
				parentObjIdx = parentType;
				modelIndex = (ModelIndex)parentType;
				break;
		}

		FVIEW_calcrotatemove(g_objectTable[parentObjIdx].pitch, g_objectTable[parentObjIdx].yaw,
							 &g_objectTable[parentObjIdx]);
		FVIEW_calcrotateorient(g_objectTable[parentObjIdx].roll, g_objectTable[parentObjIdx].angleD,
							   &g_objectTable[parentObjIdx]);
		pai_RotateLocalVectorToWorldScratch(
			&g_objectTable[parentObjIdx], g_modelDefs[modelIndex].meshAttachData[8],
			g_modelDefs[modelIndex].meshAttachData[9], g_modelDefs[modelIndex].meshAttachData[10]);
		worldX = g_rotatedX + g_objectTable[parentObjIdx].world_x;
		worldZ = g_rotatedZ + g_objectTable[parentObjIdx].world_z;
		worldY = g_rotatedY + g_objectTable[parentObjIdx].world_y;
		g_objectTable[g_yardR2D2ObjIdx].yaw = g_objectTable[parentObjIdx].yaw;
		g_objectTable[g_yardR2D2ObjIdx].pitch = g_objectTable[parentObjIdx].pitch;
		g_objectTable[g_yardR2D2ObjIdx].world_x = worldX;
		g_objectTable[g_yardR2D2ObjIdx].world_y = worldY;
		g_objectTable[g_yardR2D2ObjIdx].world_z =
			ModelBounds_GetSizeZ((uint16_t)g_objectTable[g_yardR2D2ObjIdx].objectType) / 2 + worldZ + 3;
		g_objectTable[g_yardR2D2ObjIdx].mobj->prevWorldX = g_objectTable[g_yardR2D2ObjIdx].world_x;
		g_objectTable[g_yardR2D2ObjIdx].mobj->prevWorldY = g_objectTable[g_yardR2D2ObjIdx].world_y;
		g_objectTable[g_yardR2D2ObjIdx].mobj->prevWorldZ = g_objectTable[g_yardR2D2ObjIdx].world_z;
		FVIEW_calcrotatemove(g_objectTable[g_yardR2D2ObjIdx].pitch, g_objectTable[g_yardR2D2ObjIdx].yaw,
							 &g_objectTable[g_yardR2D2ObjIdx]);
		FVIEW_calcrotateorient(g_objectTable[g_yardR2D2ObjIdx].roll, g_objectTable[g_yardR2D2ObjIdx].angleD,
							   &g_objectTable[g_yardR2D2ObjIdx]);
	}

	for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
		switch (g_yardChallengeMode) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
				Player_SetTarget((int16_t)g_yardContext.courseSide1Checkpoints[0].objectIdx, playerIdx);
				break;

			case 6:
			case 7:
				Player_SetTarget((int16_t)g_yardR2D2ObjIdx, playerIdx);
				break;

			default:
				break;
		}
	}

	if (g_yardChallengeMode <= 2u) {
		FlightLight_AddDirectionalLight(0, 53247, 20479, 0.5f, 1.0f, 1.0f, 1.0f);
	}

	g_yardCourseSide1ToSide2GapDist = 0;
	g_yardCourseSide2ToSide1GapDist = 0;
	if (g_yardChallengeMode == 2u) {
		g_yardCourseSide1ToSide2GapDist =
			collide_roughdistance3d(g_objectTable[g_yardContext.courseSide2Checkpoints[0].objectIdx].world_x -
										g_objectTable[g_yardCourseSide1FinalObjIdx].world_x,
									g_objectTable[g_yardContext.courseSide2Checkpoints[0].objectIdx].world_y -
										g_objectTable[g_yardCourseSide1FinalObjIdx].world_y,
									g_objectTable[g_yardContext.courseSide2Checkpoints[0].objectIdx].world_z -
										g_objectTable[g_yardCourseSide1FinalObjIdx].world_z);
		g_yardCourseSide2ToSide1GapDist =
			collide_roughdistance3d(g_objectTable[g_yardContext.courseSide1Checkpoints[0].objectIdx].world_x -
										g_objectTable[g_yardCourseSide2FinalObjIdx].world_x,
									g_objectTable[g_yardContext.courseSide1Checkpoints[0].objectIdx].world_y -
										g_objectTable[g_yardCourseSide2FinalObjIdx].world_y,
									g_objectTable[g_yardContext.courseSide1Checkpoints[0].objectIdx].world_z -
										g_objectTable[g_yardCourseSide2FinalObjIdx].world_z);
	}

	if (g_yardChallengeMode >= 3u) {
		unsigned int dockIdx;

		for (dockIdx = 0; dockIdx < 3u; ++dockIdx) {
			const Vec3i* dockPoint;
			ModelIndex modelIndex;
			ObjectRecord* centrifugeObj;

			modelIndex = GetModelIndexFromType(OBJ_Centrifuge);
			/* The three molten-block launch positions use dock points 0, 2, and 4. */
			dockPoint = &g_modelDefs[modelIndex].dockPoints[dockIdx * 2u];
			pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardCentrifugeObjIdx], dockPoint->x,
												dockPoint->z, dockPoint->y);
			centrifugeObj = &g_objectTable[g_yardCentrifugeObjIdx];
			g_yardCentrifugeMoltenBlockSpawnWorldX[dockIdx] = g_rotatedX + centrifugeObj->world_x;
			g_yardCentrifugeMoltenBlockSpawnWorldY[dockIdx] = g_rotatedY + centrifugeObj->world_y;
			g_yardCentrifugeMoltenBlockSpawnWorldZ[dockIdx] = g_rotatedZ + centrifugeObj->world_z;
		}
	}

	ModelMesh_BuildObjectTypeMeshCache();
}

// FUNCTION: XWA 0x519470
int Yard_SteerTrackedObjectTowardPoint(int objectIdx, int targetX, int targetY, int targetZ, int deltaTicks) {
	int dz;
	Q16Angle pitch;
	Q16Angle yaw;
	Q16Angle savedPitch;
	Q16Angle targetYaw;
	Q16Angle targetAimPitch;
	int dx;
	int dy;
	int yawDelta;
	int pitchDelta;
	int absYawDelta;
	int absPitchDelta;
	int yawStep;
	int pitchStep;
	uint16_t speedDelta;

	dz = targetZ;
	yaw = g_objectTable[objectIdx].yaw;
	pitch = g_objectTable[objectIdx].pitch;
	savedPitch = pitch;
	dx = g_objectTable[objectIdx].world_x;
	dy = g_objectTable[objectIdx].world_y;
	dz -= g_objectTable[objectIdx].world_z;
	dy = targetY - dy;
	dx = targetX - dx;

	trig2_ctop(dx, dy, dz);
	targetYaw = trig2_xyangle;
	targetAimPitch = targetPitch;
	yawDelta = (int)yaw - (int)targetYaw;
	pitchDelta = (int)pitch - (int)targetAimPitch;

	if (yawDelta != 0) {
		absYawDelta = abs(yawDelta);
		if (absYawDelta > 0x4000) {
			yawStep = 36 * deltaTicks;
		} else {
			yawStep = 18 * deltaTicks;
		}
		if (yawStep > absYawDelta) {
			yawStep = absYawDelta;
		}
		if (yawDelta > 0) {
			yawStep = -yawStep;
		}
		if (absYawDelta >= 0x8000) {
			yawStep = -yawStep;
		}
		g_objectTable[objectIdx].yaw = (Q16Angle)(yaw + yawStep);
	}
	if (pitchDelta != 0) {
		absPitchDelta = abs(pitchDelta);
		if (absPitchDelta > 0x4000) {
			pitchStep = 36 * deltaTicks;
		} else {
			pitchStep = 18 * deltaTicks;
		}
		if (pitchStep > absPitchDelta) {
			pitchStep = absPitchDelta;
		}
		if (pitchDelta > 0) {
			if (absPitchDelta < 0x8000) {
				pitchStep = -pitchStep;
			}
		}
		if (absPitchDelta >= 0x8000) {
			pitchStep = -pitchStep;
		}
		g_objectTable[objectIdx].pitch = (Q16Angle)(savedPitch + pitchStep);
	}

	if (yawDelta != 0 || pitchDelta != 0) {
		FVIEW_calcrotatemove(g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
							 &g_objectTable[objectIdx]);
		FVIEW_calcrotateorient(g_objectTable[objectIdx].roll, g_objectTable[objectIdx].angleD,
							   &g_objectTable[objectIdx]);
	}

	speedDelta = (uint16_t)deltaTicks;
	if (abs(yawDelta) > 0x1200 || abs(pitchDelta) > 0x1200 ||
		g_objectTable[objectIdx].mobj->velocityOverrideSpeed > 10u) {
		g_objectTable[objectIdx].mobj->speed = (uint16_t)(g_objectTable[objectIdx].mobj->speed - speedDelta);
		if ((int16_t)g_objectTable[objectIdx].mobj->speed < 0) {
			g_objectTable[objectIdx].mobj->speed = 0;
		}
	} else {
		g_objectTable[objectIdx].mobj->speed = (uint16_t)(g_objectTable[objectIdx].mobj->speed + speedDelta);
		if (g_objectTable[objectIdx].mobj->speed > 100u) {
			g_objectTable[objectIdx].mobj->speed = 100;
		}
	}

	g_approxDist = collide_roughdistance3d(dx, dy, dz);
	return g_approxDist;
}

// FUNCTION: XWA 0x514790
int Yard_SpawnObjectAtWorldPos(ObjectTypeId objectType, int worldX, int worldY, int worldZ, Q16Angle yaw,
							   Q16Angle pitch) {
	uint16_t objIdx;
	MobileObject* mobj;
	int componentIdx;
	int meshIdx;
	int meshCount;
	int systemIdx;
	int remainingCount;
	int randSeed;
	uint16_t objectTypeId;

	objectTypeId = (uint16_t)objectType;
	if (g_loadedModels.byObjectType[objectTypeId] == 0) {
		Hangar_LoadCraftModelByType(objectType);
		Math_SetFpuSinglePrecisionMode();
		if (g_loadedModels.byObjectType[objectTypeId] == 0) {
			return 0xffff;
		}
	}

	objIdx = Object_AllocSlotForGenus(g_modelTypeTable[objectTypeId].genusId);
	if ((uint16_t)objIdx == 0xffffu) {
		return 0xffff;
	}

	g_objectTable[objIdx].regionIdx = g_players[g_localPlayer].regionIndex;
	g_objectTable[objIdx].objectType = objectType;
	g_objectTable[objIdx].objectSignature = 1;
	g_objectTable[objIdx].genusId = g_modelTypeTable[objectTypeId].genusId;
	g_objectTable[objIdx].flightGroupIdx = g_yardSpawnFlightGroupIdx;
	g_objectTable[objIdx].roll = 0;
	g_objectTable[objIdx].angleD = 0;
	g_objectTable[objIdx].yaw = (Q16Angle)yaw;
	g_objectTable[objIdx].pitch = (Q16Angle)pitch;
	g_objectTable[objIdx].playerOwnerIdx = -1;
	g_objectTable[objIdx].world_x = worldX;
	g_objectTable[objIdx].world_y = worldY;
	g_objectTable[objIdx].world_z = worldZ;

	mobj = g_objectTable[objIdx].mobj;
	if (mobj != NULL) {
		mobj->state = g_modelTypeTable[objectTypeId].familyId;
		mobj->rollImpulseRate = 0;
		mobj->speed = 0;
		mobj->speedRemainder = 0;
		mobj->damageAmount = 0x7fff;
		mobj->lifetimeTimer = 0;
		mobj->framesAlive = 0;
		mobj->sourceObjIdx = 0;
		mobj->sourceObjectType = 0;
		mobj->iff = 2;
		g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
		mobj->moveVectorDirty = 1;
		mobj->velocityOverrideSpeed = 0;
		mobj->velocityOverrideElapsed = 0;
		mobj->velocityOverrideActive = 0;
		mobj->velocityOverrideDuration = 0;
		mobj->velocityOverrideDirX = 0;
		mobj->velocityOverrideDirY = 0;
		mobj->velocityOverrideDirZ = 0;
		mobj->spinRate = 0;
		mobj->spinRateFrac = 0;
		mobj->spinAngleQ16 = 0;
		mobj->spinAxisX = 0.0f;
		mobj->spinAxisY = 0.0f;
		mobj->spinAxisZ = 0.0f;
		mobj->nodeSwitchIndex = 0;

		g_curCraft = mobj->pCraft;
		if (g_curCraft != NULL) {
			g_curCraft->aiController.targetComponent = 0xffffu;
			g_curCraft->carriedObjectIndex = 0xffffu;
			g_curCraft->carrierObjIdx = 0xffffu;
			g_curCraft->lastReleasedObjectIdx = 0xffffu;
			g_curCraft->releaseClearTimer = 0;
			g_curCraft->linkedPrevObjectIdx = 0xffffu;
			g_curCraft->nextLinkObjectIdx = 0xffffu;
			g_curCraft->linkSequenceIndex = 0;
			g_curCraft->aiFlight.impactObjIdx = 0xffffu;
			g_curCraft->aiFlight.goHomeFlag = 0;
			g_curCraft->aiFlight.missionAbortedFlag = 0;
			g_curCraft->aiFlight.departTimerFlag = 0;
			g_curCraft->aiFlight.departClockHours = 0;
			g_curCraft->aiFlight.departClockMin = 0;
			g_curCraft->aiFlight.departClockSec = 0;
			g_curCraft->aiFlight.reactionTimer = 0;
			g_curCraft->commandedSpeed = 0;
			g_curCraft->aiFlight.reserved0C = 0;
			g_curCraft->aiFlight.orderActionCounter = 0;
			g_curCraft->aiFlight.orderActionFlag = 0;
			g_curCraft->aiFlight.objSignatureCount = 0;
			g_curCraft->aiController.escortTargetFG = -1;
			g_curCraft->aiController.currentOrderSlot = 0;
			g_curCraft->aiController.orderStateFlag = 0;
			g_curCraft->aiController.targetObjIdx = 0xffffu;
			g_curCraft->aiController.candidateTargetIdx = 0xffffu;
			g_curCraft->aiController.targetSignature = 0;
			g_curCraft->aiController.hasLiveTarget = 0;
			g_curCraft->aiController.targetComponent = 0xffffu;
			g_curCraft->aiController.escortTargetFG = -1;
			g_curCraft->aiController.aimPointX = 0;
			g_curCraft->aiController.aimPointY = 0;
			g_curCraft->aiController.aimPointZ = 0;
			g_curCraft->aiController.maneuverDist = 0;
			g_curCraft->aiController.orbitRadius = 0;
			g_curCraft->aiController.targetZAngle = 0x4000u;
			g_curCraft->aiController.targetRoll = 0;
			g_curCraft->aiController.targetXYAngle = 0;
			g_curCraft->aiController.waypointIndex = 0;
			g_curCraft->aiController.savedPlanId = 0;
			g_curCraft->aiController.thinkInterval = 708;
			randSeed = GameRand() ^ 0xbeef;
			g_curCraft->aiController.savedRandSeed = (int16_t)randSeed;
			g_curCraft->aiController.maneuverMode = 0;
			g_curCraft->aiController.maneuverPhase = 0;
			g_curCraft->aiController.maneuverTimer = 0;

			for (componentIdx = 0, remainingCount = 50; remainingCount != 0;
				 ++componentIdx, --remainingCount) {
				g_curCraft->componentState[componentIdx] = 0;
				g_curCraft->meshRotation[componentIdx] = 0;
				g_curCraft->componentHp[componentIdx] = 0xffu;
			}

			meshCount = ModelMesh_GetObjectTypeMeshCount((ObjectTypeId)objectTypeId);
			for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
				MeshType meshType;

				meshType = ModelMesh_GetObjectTypeMeshType((ObjectTypeId)objectTypeId, meshIdx);
				switch (meshType) {
					case MESH_Wing:
						g_curCraft->componentHp[meshIdx] = 20;
						g_curCraft->componentState[meshIdx] = 0;
						break;
					case MESH_SmallGun:
					case MESH_BeamSystem:
						g_curCraft->componentHp[meshIdx] = 2;
						g_curCraft->componentState[meshIdx] = 0;
						break;
				}
			}

			g_curCraft->hullMax = 0x7fff;
			g_curCraft->systemDamageHullThreshold = 0x7fff;
			g_curCraft->hullDamage = 0;
			g_curCraft->unusedMissionFlag189 = 0;
			g_curCraft->attackedByTeam[0] = 0;
			g_curCraft->notDisabledAccountingSuppress = 0;
			g_curCraft->wasCaptured = 0;
			g_curCraft->sFoilState = 0;
			for (systemIdx = 0, remainingCount = 5; remainingCount != 0; ++systemIdx, --remainingCount) {
				g_curCraft->beamEffectAccum[systemIdx] = 0;
			}
			g_curCraft->engineOutputScale = 0xffffu;
			for (systemIdx = 0, remainingCount = 10; remainingCount != 0; ++systemIdx, --remainingCount) {
				g_curCraft->systemHealth[systemIdx] = 100;
			}
			if (objectTypeId == OBJ_AccelRing2 || objectTypeId == OBJ_AccelRing3) {
				g_curCraft->systemFlags = 0x10u;
			} else {
				g_curCraft->systemFlags = 0;
			}
			g_curCraft->workingSubsystems = g_curCraft->systemFlags;
			g_curCraft->shieldDistribMode = 0;
			g_curCraft->shieldFront = 0;
			g_curCraft->shieldRear = 0;
			g_curCraft->effectiveAiObjectLink = NULL;

			FlightObject_InitMeshAnimationDefaults(objIdx);
			if ((g_curCraft->systemFlags & 0x10u) != 0) {
				ModelIndex modelIndex;
				uint16_t laserSlotCount;
				int initGroupIdx;
				uint16_t groupIdx;

				g_curCraft->cannonClassCount = 0;
				laserSlotCount = 0;
				for (initGroupIdx = 0, remainingCount = 3; remainingCount != 0;
					 ++initGroupIdx, --remainingCount) {
					g_curCraft->laserProjectileTypeId[initGroupIdx] = 0;
					g_curCraft->laserLinkMode[initGroupIdx] = 0;
					g_curCraft->laserLinkMode[initGroupIdx + 3] = 0;
					g_curCraft->laserLinkNextSlot[initGroupIdx] = 0;
					g_curCraft->laserFireCooldownTicks[initGroupIdx] = 0;
					g_curCraft->laserLastFireTimestamp[initGroupIdx] = 0;
				}

				modelIndex = (ModelIndex)GetModelIndexFromType((ObjectTypeId)objectTypeId);
				g_curCraft->modelIndex = modelIndex;
				for (groupIdx = 0; groupIdx < 3; ++groupIdx) {
					g_curCraft->laserProjectileTypeId[groupIdx] =
						g_modelDefs[modelIndex].laserGroupWeaponType[groupIdx];
					if (g_curCraft->laserProjectileTypeId[groupIdx] != 0) {
						uint16_t firstSlot;
						uint16_t lastSlot;
						uint16_t slotIdx;

						laserSlotCount += g_modelDefs[modelIndex].laserGroupSlotCount[groupIdx];
						firstSlot = g_modelDefs[modelIndex].laserGroupFirstSlot[groupIdx];
						lastSlot = g_modelDefs[modelIndex].laserGroupLastSlot[groupIdx];
						if (g_modelDefs[modelIndex].laserGroupMountType[groupIdx] == 1u ||
							g_modelDefs[modelIndex].laserGroupMountType[groupIdx] == 2u) {
							++g_curCraft->cannonClassCount;
							g_curCraft->laserLinkNextSlot[groupIdx] = firstSlot;
						}

						if (firstSlot <= lastSlot) {
							for (slotIdx = firstSlot; slotIdx <= lastSlot; ++slotIdx) {
								if (g_modelDefs[modelIndex].laserGroupMountType[groupIdx] == 1u ||
									g_modelDefs[modelIndex].laserGroupMountType[groupIdx] == 2u) {
									g_curCraft->warheadData[slotIdx].projectileTypeId =
										g_curCraft->laserProjectileTypeId[groupIdx];
									g_curCraft->warheadData[slotIdx].weaponType =
										g_modelDefs[modelIndex].laserGroupMountType[groupIdx];
								} else {
									g_curCraft->warheadData[slotIdx].projectileTypeId =
										g_curCraft->laserProjectileTypeId[groupIdx];
									g_curCraft->warheadData[slotIdx].weaponType = 4;
								}
								g_curCraft->warheadData[slotIdx].laserCharge = 127;
								g_curCraft->warheadData[slotIdx].count = 0;
								g_curCraft->warheadData[slotIdx].lastFireMeshIdx = 0xffu;
								g_curCraft->warheadData[slotIdx].lastFireHardpointIdx = 0xffu;
								g_curCraft->warheadData[slotIdx].weaponGroupIdx = (uint8_t)groupIdx;
								g_curCraft->warheadData[slotIdx].turretTargetObjIdx = -1;
							}
						}
					}
				}
				g_curCraft->laserConvergeLevel = 0;
				g_curCraft->laserRedirect = 2;
				g_curCraft->laserSlotCount = (uint8_t)laserSlotCount;
			}
		}

		g_objectTable[objIdx].mobj->prevWorldX = g_objectTable[objIdx].world_x;
		g_objectTable[objIdx].mobj->prevWorldY = g_objectTable[objIdx].world_y;
		g_objectTable[objIdx].mobj->prevWorldZ = g_objectTable[objIdx].world_z;
		FVIEW_calcrotatemove(g_objectTable[objIdx].pitch, g_objectTable[objIdx].yaw, &g_objectTable[objIdx]);
		FVIEW_calcrotateorient(g_objectTable[objIdx].roll, g_objectTable[objIdx].angleD,
							   &g_objectTable[objIdx]);
	}

#ifdef XWA_MODERN
	g_objectTable[objIdx].typeSpecificByte[0] = 0;
	g_objectTable[objIdx].typeSpecificByte[1] = 0;
#else
	for (componentIdx = 0; componentIdx < 2; ++componentIdx) {
		g_objectTable[objIdx].typeSpecificByte[componentIdx] = 0;
	}
#endif
	collide_ResetObjectProximityForSlot(objIdx);
	return objIdx;
}

// FUNCTION: XWA 0x51B1A0
int Yard_SpawnJunkBlockAtChamberDock(void) {
	int slotIdx;
	int* stateValue;
	ModelIndex compactorModelIdx;
	int objIdx;
	YardTrackedObjectState* state;

	slotIdx = 0;
	if (g_yardContext.smeltingJunkStateCount > 0) {
		state = g_yardContext.smeltingJunkStates;
		while (slotIdx < g_yardContext.smeltingJunkStateCount && state->state != 0) {
			++slotIdx;
			++state;
		}
	}

	if (slotIdx < g_yardContext.smeltingJunkStateCount ||
		(g_yardContext.smeltingJunkStateCount < 9 && (slotIdx = g_yardContext.smeltingJunkStateCount, 1))) {
		compactorModelIdx = GetModelIndexFromType(OBJ_Compactor);
		pai_RotateLocalVectorToWorldScratch(
			&g_objectTable[g_yardContext.compactorObjIdx], g_modelDefs[compactorModelIdx].dockPoints[1].x,
			g_modelDefs[compactorModelIdx].dockPoints[1].z, g_modelDefs[compactorModelIdx].dockPoints[1].y);

		objIdx = Yard_SpawnObjectAtWorldPos(
			OBJ_JunkBlock, g_rotatedX + g_objectTable[g_yardContext.compactorObjIdx].world_x,
			g_rotatedY + g_objectTable[g_yardContext.compactorObjIdx].world_y,
			g_rotatedZ + g_objectTable[g_yardContext.compactorObjIdx].world_z, 0xc000, 0x4000);

		g_yardContext.smeltingJunkStates[(uint16_t)slotIdx].objectIdx = objIdx;
		if (objIdx != 0xffff) {
			stateValue = &g_yardContext.smeltingJunkStates[(uint16_t)slotIdx].state;
			*stateValue = 1;
			if (slotIdx == g_yardContext.smeltingJunkStateCount) {
				++g_yardContext.smeltingJunkStateCount;
			}
		}

		objIdx = g_yardContext.smeltingJunkStates[(uint16_t)slotIdx].objectIdx;
	} else {
		objIdx = 0xffff;
	}

	return objIdx;
}

// FUNCTION: XWA 0x518530
void Yard_UpdateCompactorCycle(int deltaTicks) {
	ObjectRecord* compactorObj;
	CraftData* compactorCraft;
	unsigned int sourceObjIdx;
	int tickRemainder;
	int elapsedStepTicks;
	int stepCount;
	uint16_t meshCount;
	uint16_t meshIdx;
	uint8_t meshRotation;
	int slotIdx;

	sourceObjIdx = g_yardContext.compactorObjIdx;
	compactorObj = &g_objectTable[sourceObjIdx];
	compactorCraft = compactorObj->mobj->pCraft;
	tickRemainder = deltaTicks + g_yardContext.compactorTickAccumulator;
	elapsedStepTicks = 0;
	slotIdx = 0;
	g_yardContext.compactorTickAccumulator += deltaTicks;
	if (g_yardContext.compactorTickAccumulator >= 50) {
		do {
			tickRemainder -= 50;
			elapsedStepTicks += 50;
		} while (tickRemainder >= 50);
		g_yardContext.compactorTickAccumulator = tickRemainder;
	}

	if (elapsedStepTicks <= 0) {
		return;
	}

	stepCount = elapsedStepTicks / 50;
	switch (g_yardContext.compactorCycleState) {
		case 0:
#ifdef XWA_MODERN
			if (XwaModernFlightTiming_IsHighRate()) {
				g_yardContext.compactorPauseTimer -= 8 * stepCount;
			} else {
				g_yardContext.compactorPauseTimer -= deltaTicks;
			}
#else
			g_yardContext.compactorPauseTimer -= deltaTicks;
#endif
			if (g_yardContext.compactorPauseTimer <= 0) {
				g_yardContext.compactorPauseTimer = 0;
				for (; slotIdx < g_yardContext.rubbleChunkStateCount; ++slotIdx) {
					if (g_yardContext.rubbleChunkStates[slotIdx].state == 3) {
						g_yardContext.rubbleChunkStates[slotIdx].state = 0;
						g_objectTable[g_yardContext.rubbleChunkStates[slotIdx].objectIdx].objectType =
							OBJ_None;
					}
				}

				if (g_yardChallengeMode > 3u) {
					Yard_SpawnJunkBlockAtChamberDock();
				}
				g_yardContext.compactorCycleState = 1;
				fsfx_PlaySound(180, sourceObjIdx, (unsigned int)g_localPlayer);
			}
			break;

		case 1:
			meshCount = (uint16_t)ModelMesh_GetObjectTypeMeshCount(compactorObj->objectType);
			if (meshCount > 1u) {
				for (meshIdx = 1; meshIdx < meshCount; ++meshIdx) {
					if (compactorCraft->meshRotation[meshIdx] < g_yardCompactorMeshOpenLimits[meshIdx]) {
						compactorCraft->meshRotation[meshIdx] += (uint8_t)stepCount;
						if (compactorCraft->meshRotation[meshIdx] > g_yardCompactorMeshOpenLimits[meshIdx]) {
							compactorCraft->meshRotation[meshIdx] = g_yardCompactorMeshOpenLimits[meshIdx];
						}
					}
				}
			}

			if (compactorCraft->meshRotation[2] == g_yardCompactorMeshOpenLimits[2]) {
				g_yardContext.compactorCycleState = 2;
				fsfx_PlaySound(182, sourceObjIdx, (unsigned int)g_localPlayer);
			}
			break;

		case 2: {
			int crushedCount;

			crushedCount = 0;
			for (slotIdx = 0; slotIdx < g_yardContext.rubbleChunkStateCount; ++slotIdx) {
				if (g_yardContext.rubbleChunkStates[slotIdx].state == 3) {
					++crushedCount;
				}
			}
			if (crushedCount >= 7 && g_yardContext.smeltingJunkStateCount < 10) {
				g_yardContext.compactorCycleState = 3;
				fsfx_PlaySound(181, sourceObjIdx, (unsigned int)g_localPlayer);
			}
			break;
		}

		case 3:
			meshCount = (uint16_t)ModelMesh_GetObjectTypeMeshCount(compactorObj->objectType);
			if (meshCount > 1u) {
				for (meshIdx = 1; meshIdx < meshCount; ++meshIdx) {
					meshRotation = compactorCraft->meshRotation[meshIdx];
					if (meshRotation > 0u) {
						meshRotation = (uint8_t)(meshRotation - stepCount);
						compactorCraft->meshRotation[meshIdx] = meshRotation;
						if (meshRotation > g_yardCompactorMeshOpenLimits[meshIdx]) {
							compactorCraft->meshRotation[meshIdx] = 0;
						}
					}
				}
			}

			// Preserve the original boolean-before-modulo expression.
			if (((uint16_t)GameRand2() == 0) % 20 != 0) {
				switch ((GameRand2() & 0xffff) % 4) {
					case 0:
						(void)fsfx_PlaySound(176, sourceObjIdx, (unsigned int)g_localPlayer);
						break;
					case 1:
						(void)fsfx_PlaySound(177, sourceObjIdx, (unsigned int)g_localPlayer);
						break;
					case 2:
						(void)fsfx_PlaySound(178, sourceObjIdx, (unsigned int)g_localPlayer);
						break;
					case 3:
						(void)fsfx_PlaySound(179, sourceObjIdx, (unsigned int)g_localPlayer);
						break;
				}
			}

			if (compactorCraft->meshRotation[2] == 0) {
				g_yardContext.compactorCycleState = 0;
				(void)fsfx_PlaySound(182, sourceObjIdx, (unsigned int)g_localPlayer);
				fsfx_PlaySound(185, sourceObjIdx, (unsigned int)g_localPlayer);
				g_yardContext.compactorPauseTimer = 200;
			}
			break;

		default:
			break;
	}
}

// FUNCTION: XWA 0x51B010
int Yard_SpawnRubbleChunkAtWorldPos(int rubbleSlot, int worldX, int worldY, int worldZ) {
	int yaw;
	int objIdx;

	yaw = (GameRand() & 0xffff) % 0x2000 + 0x7000;
	objIdx =
		Yard_SpawnObjectAtWorldPos((ObjectTypeId)(OBJ_Junk01 + (GameRand() & 0xffff) % 9), worldX, worldY,
								   worldZ, (int16_t)yaw, (int16_t)((GameRand() & 0xffff) % 0x2000 + 0x3000));

	g_yardContext.rubbleChunkStates[rubbleSlot].objectIdx = objIdx;
	if (objIdx != 0xffff) {
		g_yardContext.rubbleChunkStates[rubbleSlot].objectIdx = objIdx;
		g_yardContext.rubbleChunkStates[rubbleSlot].state = 1;

		g_objectTable[objIdx].mobj->velocityOverrideSpeed = (uint16_t)((GameRand() & 0xffff) % 80 + 30);
		g_objectTable[objIdx].mobj->velocityOverrideElapsed = 0;
		g_objectTable[objIdx].mobj->velocityOverrideActive = 1;
		g_objectTable[objIdx].mobj->velocityOverrideDuration = 0;
		g_objectTable[objIdx].mobj->velocityOverrideDirX = g_objectTable[objIdx].mobj->cachedFwdX;
		g_objectTable[objIdx].mobj->velocityOverrideDirY = g_objectTable[objIdx].mobj->cachedFwdY;
		g_objectTable[objIdx].mobj->velocityOverrideDirZ = g_objectTable[objIdx].mobj->cachedFwdZ;
		MobileObject_SetRandomSpinAxis(objIdx);
		g_objectTable[objIdx].mobj->spinRate = (int16_t)((GameRand() & 0xffff) % 20000 - 10000);
		g_objectTable[objIdx].mobj->spinRateFrac = 0;
	}

	return objIdx;
}

// FUNCTION: XWA 0x51AEA0
void Yard_UpdatePeriodicRubbleSpawner(int deltaTicks) {
	uint16_t slotIdx;
	int spawnRadius;
	int spawnHeight;
	int worldX;
	int worldY;
	int worldZ;

	g_yardContext.rubbleSpawnTickAccumulator += deltaTicks;
	if (g_yardContext.rubbleSpawnTickAccumulator < 3000) {
		return;
	}

	g_yardContext.rubbleSpawnTickAccumulator -= 3000;
	slotIdx = 0;
	if (g_yardContext.rubbleChunkStateCount > 0) {
		while (slotIdx < g_yardContext.rubbleChunkStateCount &&
			   g_yardContext.rubbleChunkStates[slotIdx].state != 0) {
			++slotIdx;
		}
	}

	if (slotIdx >= g_yardContext.rubbleChunkStateCount) {
		if (g_yardContext.rubbleChunkStateCount >= g_yardRubbleChunkSpawnLimit - 1) {
			return;
		}
		slotIdx = (uint16_t)g_yardContext.rubbleChunkStateCount;
	}

	spawnRadius = ModelBounds_GetSizeX(g_objectTable[g_yardChuteTunnelEndObjIdx].objectType) / 3;
	spawnHeight = ModelBounds_GetSizeY(g_objectTable[g_yardChuteMouthObjIdx].objectType);
	worldX = g_objectTable[g_yardChuteMouthObjIdx].world_x + 2 * ((uint16_t)GameRand() % spawnRadius) -
			 spawnRadius;
	worldY = g_objectTable[g_yardChuteMouthObjIdx].world_y - ((uint16_t)GameRand() % spawnHeight);
	worldZ = g_objectTable[g_yardChuteMouthObjIdx].world_z + 2 * ((uint16_t)GameRand() % spawnRadius) -
			 spawnRadius;

	if (Yard_SpawnRubbleChunkAtWorldPos(slotIdx, worldX, worldY, worldZ) != 0xffff) {
		g_yardContext.rubbleChunkStates[slotIdx].state = 1;
		if (slotIdx >= g_yardContext.rubbleChunkStateCount) {
			++g_yardContext.rubbleChunkStateCount;
		}
	}
}

// FUNCTION: XWA 0x514440
int Yard_SpawnChildAtMount(ObjectTypeId childType, int parentObjIdx, int mountSelector, int yawArg,
						   int pitchArg, int angleMode) {
	uint16_t parentType;
	ModelIndex parentModelIdx;
	int mountLocalSide;
	int mountLocalUp;
	int mountLocalFwd;
	int parentWorldX;
	int parentMountWorldX;
	int parentMountWorldY;
	int parentMountWorldZ;
	int parentWorldY;
	int parentWorldZ;
	Q16Angle yaw;
	Q16Angle pitch;
	int childObjIdx;
	ModelIndex childModelIdx;
	int childMountWorldX;
	int childMountWorldY;
	int childMountWorldZ;

	parentType = g_objectTable[parentObjIdx].objectType;
	parentModelIdx = GetModelIndexFromType(parentType);

	switch (mountSelector) {
		case 0:
			mountLocalSide = g_modelDefs[parentModelIdx].childMountPoints[3];
			mountLocalUp = g_modelDefs[parentModelIdx].childMountPoints[4];
			mountLocalFwd = g_modelDefs[parentModelIdx].childMountPoints[5];
			break;

		case 1:
			mountLocalSide = g_modelDefs[parentModelIdx].childMountPoints[0];
			mountLocalUp = g_modelDefs[parentModelIdx].childMountPoints[1];
			mountLocalFwd = g_modelDefs[parentModelIdx].childMountPoints[2];
			break;

		case 2:
		case 3:
			mountLocalSide = g_modelDefs[parentModelIdx].meshAttachData[5];
			mountLocalUp = g_modelDefs[parentModelIdx].meshAttachData[6];
			mountLocalFwd = g_modelDefs[parentModelIdx].meshAttachData[7];
			break;

		default:
			mountLocalSide = mountSelector;
			mountLocalUp = mountSelector;
			mountLocalFwd = parentObjIdx;
			break;
	}

	pai_RotateLocalVectorToWorldScratch(&g_objectTable[parentObjIdx], mountLocalSide, mountLocalUp,
										mountLocalFwd);
	parentMountWorldX = g_objectTable[parentObjIdx].world_x + g_rotatedX;
	parentWorldX = g_objectTable[parentObjIdx].world_x;
	parentMountWorldY = g_objectTable[parentObjIdx].world_y + g_rotatedY;
	parentWorldY = g_objectTable[parentObjIdx].world_y;
	parentMountWorldZ = g_objectTable[parentObjIdx].world_z + g_rotatedZ;
	parentWorldZ = g_objectTable[parentObjIdx].world_z;

	switch (parentType) {
		case OBJ_Compactor:
			yaw = g_objectTable[parentObjIdx].yaw - 0x4000;
			pitch = g_objectTable[parentObjIdx].pitch;
			break;

		case OBJ_SmeltingRoom:
			if (mountSelector == 1) {
				yaw = g_objectTable[parentObjIdx].yaw + 0x4000;
			} else {
				yaw = g_objectTable[parentObjIdx].yaw;
			}
			pitch = g_objectTable[parentObjIdx].pitch;
			break;

		case OBJ_SRTubeUP:
			pitch = g_objectTable[parentObjIdx].pitch - 0x2000;
			yaw = g_objectTable[parentObjIdx].yaw;
			break;

		case OBJ_SRTubeDown:
			pitch = g_objectTable[parentObjIdx].pitch + 0x2000;
			yaw = g_objectTable[parentObjIdx].yaw;
			break;

		case OBJ_SRTubeLH:
			yaw = g_objectTable[parentObjIdx].yaw - 0x2000;
			pitch = g_objectTable[parentObjIdx].pitch;
			break;

		case OBJ_SRTubeRH:
			yaw = g_objectTable[parentObjIdx].yaw + 0x2000;
			pitch = g_objectTable[parentObjIdx].pitch;
			break;

		default:
			yaw = g_objectTable[parentObjIdx].yaw;
			pitch = g_objectTable[parentObjIdx].pitch;
			break;
	}

	switch (angleMode) {
		case 0:
			yaw = yawArg;
			pitch = pitchArg;
			break;

		case 1:
			yaw += yawArg;
			pitch += pitchArg;
			break;
	}

	childObjIdx = Yard_SpawnObjectAtWorldPos(childType, parentWorldX, parentWorldY, parentWorldZ, yaw, pitch);
	childModelIdx = GetModelIndexFromType(childType);
	switch (mountSelector) {
		case 0:
		case 3:
			mountLocalSide = g_modelDefs[childModelIdx].childMountPoints[0];
			mountLocalUp = g_modelDefs[childModelIdx].childMountPoints[1];
			mountLocalFwd = g_modelDefs[childModelIdx].childMountPoints[2];
			break;

		case 1:
		case 2:
			mountLocalSide = g_modelDefs[childModelIdx].childMountPoints[3];
			mountLocalUp = g_modelDefs[childModelIdx].childMountPoints[4];
			mountLocalFwd = g_modelDefs[childModelIdx].childMountPoints[5];
			break;

		default:
			break;
	}

	pai_RotateLocalVectorToWorldScratch(&g_objectTable[childObjIdx], mountLocalSide, mountLocalUp,
										mountLocalFwd);
	childMountWorldX = parentMountWorldX - g_rotatedX;
	childMountWorldY = parentMountWorldY - g_rotatedY;
	childMountWorldZ = parentMountWorldZ - g_rotatedZ;

	g_objectTable[childObjIdx].mobj->prevWorldX = childMountWorldX;
	g_objectTable[childObjIdx].world_x = childMountWorldX;
	g_objectTable[childObjIdx].mobj->prevWorldY = childMountWorldY;
	g_objectTable[childObjIdx].world_y = childMountWorldY;
	g_objectTable[childObjIdx].mobj->prevWorldZ = childMountWorldZ;
	g_objectTable[childObjIdx].world_z = childMountWorldZ;

	FVIEW_calcrotatemove(g_objectTable[childObjIdx].pitch, g_objectTable[childObjIdx].yaw,
						 &g_objectTable[childObjIdx]);
	FVIEW_calcrotateorient(g_objectTable[childObjIdx].roll, g_objectTable[childObjIdx].angleD,
						   &g_objectTable[childObjIdx]);
	return childObjIdx;
}

// FUNCTION: XWA 0x518870
void Yard_UpdateSmeltingJunkStates(int deltaTicks) {
	uint16_t stateIdx;

	stateIdx = 0;
	if (g_yardContext.smeltingJunkStateCount <= 0) {
		return;
	}

	do {
		int state;
		int objectIdx;

		state = g_yardContext.smeltingJunkStates[stateIdx].state;
		if (state != 0) {
			objectIdx = g_yardContext.smeltingJunkStates[stateIdx].objectIdx;
			switch (state) {
				case 1:
					if (g_yardContext.compactorCycleState == 2) {
						g_objectTable[objectIdx].mobj->velocityOverrideActive = 1;
						g_objectTable[objectIdx].mobj->velocityOverrideSpeed = 1;
						g_objectTable[objectIdx].mobj->velocityOverrideElapsed = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideDirX = -32767;
						g_objectTable[objectIdx].mobj->velocityOverrideDirY = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideDirZ = 0;
						g_yardContext.smeltingJunkStates[stateIdx].state = 2;
					}
					break;

				case 2: {
					ObjectRecord* objects;
					ObjectRecord* compactorObj;
					int compactorObjIdx;

					g_objectTable[objectIdx].mobj->velocityOverrideSpeed =
						(uint16_t)(g_objectTable[objectIdx].mobj->velocityOverrideSpeed + deltaTicks);
					if (g_objectTable[objectIdx].mobj->velocityOverrideSpeed > 30u) {
						g_objectTable[objectIdx].mobj->velocityOverrideSpeed = 30;
						g_objectTable[objectIdx].mobj->velocityOverrideElapsed = 0;
					}

					objects = g_objectTable;
					compactorObjIdx = g_yardContext.compactorObjIdx;
					g_collisionProbeWorldX = objects[objectIdx].world_x;
					g_collisionProbeWorldY = objects[objectIdx].world_y;
					g_collisionProbeWorldZ = objects[objectIdx].world_z;
					g_collisionSegmentStartWorldX = objects[objectIdx].mobj->prevWorldX;
					g_collisionSegmentStartWorldY = objects[objectIdx].mobj->prevWorldY;
					g_collisionSegmentStartWorldZ = objects[objectIdx].mobj->prevWorldZ;
					compactorObj = &objects[compactorObjIdx];
					g_collisionSweepEndX = compactorObj->world_x;
					g_collisionSweepEndY = compactorObj->world_y;
					g_collisionSweepEndZ = compactorObj->world_z;
					g_collisionSweepStartX = compactorObj->mobj->prevWorldX;
					g_collisionSweepStartY = compactorObj->mobj->prevWorldY;
					g_collisionSweepStartZ = compactorObj->mobj->prevWorldZ;
					if ((uint16_t)collide_lasercraftcollide(objectIdx, compactorObjIdx) != 0) {
						g_objectTable[objectIdx].mobj->velocityOverrideDirX = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideDirY = 32767;
						g_objectTable[objectIdx].mobj->velocityOverrideDirZ = 0;
						g_objectTable[objectIdx].world_x = g_objectTable[objectIdx].mobj->prevWorldX;
						g_objectTable[objectIdx].world_y = g_objectTable[objectIdx].mobj->prevWorldY;
						g_objectTable[objectIdx].world_z = g_objectTable[objectIdx].mobj->prevWorldZ;
						g_yardContext.smeltingJunkStates[stateIdx].state = 3;
					}
					break;
				}

				case 3: {
					ObjectRecord* object;
					int* worldY;
					MobileObject** mobj;
					ObjectRecord* compactorObj;

					object = &g_objectTable[objectIdx];
					worldY = &object->world_y;
					g_collisionProbeWorldX = object->world_x;
					g_collisionProbeWorldY = *worldY;
					g_collisionProbeWorldZ = object->world_z;
					mobj = &object->mobj;
					g_collisionSegmentStartWorldX = (*mobj)->prevWorldX;
					g_collisionSegmentStartWorldY = (*mobj)->prevWorldY;
					g_collisionSegmentStartWorldZ = (*mobj)->prevWorldZ;
					compactorObj = &g_objectTable[g_yardContext.compactorObjIdx];
					g_collisionSweepEndX = compactorObj->world_x;
					g_collisionSweepEndY = compactorObj->world_y;
					g_collisionSweepEndZ = compactorObj->world_z;
					g_collisionSweepStartX = compactorObj->mobj->prevWorldX;
					g_collisionSweepStartY = compactorObj->mobj->prevWorldY;
					g_collisionSweepStartZ = compactorObj->mobj->prevWorldZ;
					if (*worldY > -131472) {
						(*mobj)->velocityOverrideDirX = -32767;
						g_objectTable[objectIdx].mobj->velocityOverrideDirY = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideDirZ = 0;
						g_objectTable[objectIdx].world_x = g_objectTable[objectIdx].mobj->prevWorldX;
						g_objectTable[objectIdx].world_y = g_objectTable[objectIdx].mobj->prevWorldY;
						g_objectTable[objectIdx].world_z = g_objectTable[objectIdx].mobj->prevWorldZ;
						g_objectTable[objectIdx].mobj->velocityOverrideActive = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideSpeed = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideElapsed = 0;
						g_yardContext.smeltingJunkStates[stateIdx].state = 5;
						g_yardContext.smeltingJunkStates[stateIdx].waypointIdx = 0;
					}
					break;
				}

				case 5:
					g_approxDist = Yard_SteerTrackedObjectTowardPoint(
						objectIdx,
						g_yardContext
							.courseSide1Checkpoints[g_yardContext.smeltingJunkStates[stateIdx].waypointIdx]
							.checkpointWorldX,
						g_yardContext
							.courseSide1Checkpoints[g_yardContext.smeltingJunkStates[stateIdx].waypointIdx]
							.checkpointWorldY,
						g_yardContext
							.courseSide1Checkpoints[g_yardContext.smeltingJunkStates[stateIdx].waypointIdx]
							.checkpointWorldZ,
						deltaTicks);
					if ((unsigned int)g_approxDist < 0x320u &&
						++g_yardContext.smeltingJunkStates[stateIdx].waypointIdx >= 30u) {
						g_yardContext.smeltingJunkStates[stateIdx].state = 6;
					}
					break;

				case 6: {
					ModelIndex smeltingModelIdx;

					smeltingModelIdx = GetModelIndexFromType(OBJ_SmeltingRoom);
					pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardContext.smeltingRoomObjIdx],
														g_modelDefs[smeltingModelIdx].dockPoints[0].x,
														g_modelDefs[smeltingModelIdx].dockPoints[0].z,
														g_modelDefs[smeltingModelIdx].dockPoints[0].y);
					g_approxDist = Yard_SteerTrackedObjectTowardPoint(
						objectIdx, g_rotatedX + g_objectTable[g_yardContext.smeltingRoomObjIdx].world_x,
						g_rotatedY + g_objectTable[g_yardContext.smeltingRoomObjIdx].world_y,
						g_rotatedZ + g_objectTable[g_yardContext.smeltingRoomObjIdx].world_z, deltaTicks);
					if ((unsigned int)g_approxDist < 0x320u) {
						g_yardContext.smeltingJunkStates[stateIdx].state = 7;
					}
					break;
				}

				case 7: {
					ModelIndex smeltingModelIdx;

					smeltingModelIdx = GetModelIndexFromType(OBJ_SmeltingRoom);
					pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardContext.smeltingRoomObjIdx],
														g_modelDefs[smeltingModelIdx].dockPoints[1].x,
														g_modelDefs[smeltingModelIdx].dockPoints[1].z,
														g_modelDefs[smeltingModelIdx].dockPoints[1].y);
					g_approxDist = Yard_SteerTrackedObjectTowardPoint(
						objectIdx, g_rotatedX + g_objectTable[g_yardContext.smeltingRoomObjIdx].world_x,
						g_rotatedY + g_objectTable[g_yardContext.smeltingRoomObjIdx].world_y,
						g_rotatedZ + g_objectTable[g_yardContext.smeltingRoomObjIdx].world_z, deltaTicks);
					if ((unsigned int)g_approxDist < 0x320u &&
						++g_yardContext.smeltingJunkStates[stateIdx].waypointIdx >= 30u) {
						g_yardContext.smeltingJunkStates[stateIdx].state = 8;
					}
					break;
				}

				case 8: {
					MobileObject* mobj;

					mobj = g_objectTable[objectIdx].mobj;
					if (mobj->speed > 0u) {
						mobj->speed = 0;
					}
					g_yardContext.smeltingJunkStates[stateIdx].state = 0;
					g_objectTable[g_yardContext.smeltingJunkStates[stateIdx].objectIdx].objectType = OBJ_None;
					break;
				}

				default:
					break;
			}
		}

		++stateIdx;
	} while (stateIdx < g_yardContext.smeltingJunkStateCount);
}

// FUNCTION: XWA 0x518E70
void Yard_UpdateCentrifugeContainerStates(int deltaTicks) {
	uint16_t stateIdx;

	stateIdx = 0;
	if (g_yardContext.centrifugeContainerStateCount <= 0) {
		return;
	}

	do {
		int objectIdx;

		if (g_yardContext.centrifugeContainerStates[stateIdx].state != 0) {
			objectIdx = g_yardContext.centrifugeContainerStates[stateIdx].objectIdx;
			switch (g_yardContext.centrifugeContainerStates[stateIdx].state) {
				case 1:
#ifdef XWA_MODERN
					if (Yard_ModernIsLegacyCadenceDue()) {
#endif
						++g_objectTable[objectIdx].mobj->velocityOverrideSpeed;
						if (g_objectTable[objectIdx].mobj->velocityOverrideSpeed > 10u) {
							g_objectTable[objectIdx].mobj->velocityOverrideSpeed = 10;
						}
#ifdef XWA_MODERN
					}
#endif

					g_collisionProbeWorldX = g_objectTable[objectIdx].world_x;
					g_collisionProbeWorldY = g_objectTable[objectIdx].world_y;
					g_collisionProbeWorldZ = g_objectTable[objectIdx].world_z;
					g_collisionSegmentStartWorldX = g_objectTable[objectIdx].mobj->prevWorldX;
					g_collisionSegmentStartWorldY = g_objectTable[objectIdx].mobj->prevWorldY;
					g_collisionSegmentStartWorldZ = g_objectTable[objectIdx].mobj->prevWorldZ;

					g_collisionSweepEndX = g_objectTable[g_yardCentrifugeObjIdx].world_x;
					g_collisionSweepEndY = g_objectTable[g_yardCentrifugeObjIdx].world_y;
					g_collisionSweepEndZ = g_objectTable[g_yardCentrifugeObjIdx].world_z;
					g_collisionSweepStartX = g_objectTable[g_yardCentrifugeObjIdx].mobj->prevWorldX;
					g_collisionSweepStartY = g_objectTable[g_yardCentrifugeObjIdx].mobj->prevWorldY;
					g_collisionSweepStartZ = g_objectTable[g_yardCentrifugeObjIdx].mobj->prevWorldZ;

					if ((uint16_t)collide_lasercraftcollide(objectIdx, g_yardCentrifugeObjIdx) == 14u) {
						g_objectTable[objectIdx].mobj->velocityOverrideDirX = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideDirY = 32767;
						g_objectTable[objectIdx].mobj->velocityOverrideDirZ = 0;
						g_objectTable[objectIdx].world_x = g_objectTable[objectIdx].mobj->prevWorldX;
						g_objectTable[objectIdx].world_y = g_objectTable[objectIdx].mobj->prevWorldY;
						g_objectTable[objectIdx].world_z = g_objectTable[objectIdx].mobj->prevWorldZ;
						g_yardContext.centrifugeContainerStates[stateIdx].state = 2;
					}
					break;

				case 2: {
					ModelIndex centrifugeModelIdx;

#ifdef XWA_MODERN
					if (Yard_ModernIsLegacyCadenceDue()) {
#endif
						++g_objectTable[objectIdx].mobj->velocityOverrideSpeed;
						if (g_objectTable[objectIdx].mobj->velocityOverrideSpeed > 40u) {
							g_objectTable[objectIdx].mobj->velocityOverrideSpeed = 40;
							g_objectTable[objectIdx].mobj->velocityOverrideElapsed = 0;
						}
#ifdef XWA_MODERN
					}
#endif

					centrifugeModelIdx = GetModelIndexFromType(OBJ_Centrifuge);
					pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardCentrifugeObjIdx],
														g_modelDefs[centrifugeModelIdx].meshAttachData[8],
														g_modelDefs[centrifugeModelIdx].meshAttachData[9],
														g_modelDefs[centrifugeModelIdx].meshAttachData[10]);
					g_approxDist =
						collide_roughdistance3d(g_rotatedX + g_objectTable[g_yardCentrifugeObjIdx].world_x -
													g_objectTable[objectIdx].world_x,
												g_rotatedY + g_objectTable[g_yardCentrifugeObjIdx].world_y -
													g_objectTable[objectIdx].world_y,
												g_rotatedZ + g_objectTable[g_yardCentrifugeObjIdx].world_z -
													g_objectTable[objectIdx].world_z);
					if ((unsigned int)g_approxDist < 0x1F40u) {
						g_objectTable[objectIdx].mobj->velocityOverrideDuration = 12;
						g_yardContext.centrifugeContainerStates[stateIdx].state = 3;
						g_yardContext.centrifugeContainerStates[stateIdx].waypointIdx = 0;
						g_objectTable[objectIdx].mobj->nodeSwitchIndex = 1;
					}
					break;
				}

				case 3:
					g_approxDist = Yard_SteerTrackedObjectTowardPoint(
						objectIdx,
						g_yardContext
							.courseSide2Checkpoints[g_yardContext.centrifugeContainerStates[stateIdx]
														.waypointIdx]
							.checkpointWorldX,
						g_yardContext
							.courseSide2Checkpoints[g_yardContext.centrifugeContainerStates[stateIdx]
														.waypointIdx]
							.checkpointWorldY,
						g_yardContext
							.courseSide2Checkpoints[g_yardContext.centrifugeContainerStates[stateIdx]
														.waypointIdx]
							.checkpointWorldZ,
						deltaTicks);
					if ((unsigned int)g_approxDist < 0x320u) {
						++g_yardContext.centrifugeContainerStates[stateIdx].waypointIdx;
						if (g_yardContext.centrifugeContainerStates[stateIdx].waypointIdx == 10u) {
							g_objectTable[objectIdx].mobj->nodeSwitchIndex = 2;
						} else if (g_yardContext.centrifugeContainerStates[stateIdx].waypointIdx == 20u) {
							g_objectTable[objectIdx].mobj->nodeSwitchIndex = 3;
						}
						if (g_yardContext.centrifugeContainerStates[stateIdx].waypointIdx >= 30u) {
							g_yardContext.centrifugeContainerStates[stateIdx].state = 4;
						}
					}
					break;

				case 4: {
					ModelIndex containerModelIdx;

					containerModelIdx = GetModelIndexFromType(OBJ_ContainerGrandePG);
					pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardContainerGrandeObjIdx],
														g_modelDefs[containerModelIdx].dockPoints[0].x,
														g_modelDefs[containerModelIdx].dockPoints[0].z,
														g_modelDefs[containerModelIdx].dockPoints[0].y);
					g_approxDist = Yard_SteerTrackedObjectTowardPoint(
						objectIdx, g_rotatedX + g_objectTable[g_yardContainerGrandeObjIdx].world_x,
						g_rotatedY + g_objectTable[g_yardContainerGrandeObjIdx].world_y,
						g_rotatedZ + g_objectTable[g_yardContainerGrandeObjIdx].world_z, deltaTicks);
					if ((unsigned int)g_approxDist < 0x320u) {
						g_yardContext.centrifugeContainerStates[stateIdx].state = 7;
						g_objectTable[objectIdx].mobj->speed = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideSpeed = 1;
						g_objectTable[objectIdx].mobj->velocityOverrideElapsed = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideDuration = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideDirX = -4096;
						g_objectTable[objectIdx].mobj->velocityOverrideDirY = -2304;
						g_objectTable[objectIdx].mobj->velocityOverrideDirZ = 0;
						g_objectTable[objectIdx].mobj->velocityOverrideActive = 1;
						fsfx_PlaySound(192, (uint16_t)objectIdx, (unsigned int)g_localPlayer);
					}
					break;
				}

				case 7: {
					ModelIndex containerModelIdx;

#ifdef XWA_MODERN
					if (Yard_ModernIsLegacyCadenceDue()) {
#endif
						g_objectTable[objectIdx].mobj->velocityOverrideSpeed =
							(uint16_t)(g_objectTable[objectIdx].mobj->velocityOverrideSpeed * 2u);
						if (g_objectTable[objectIdx].mobj->velocityOverrideSpeed > 1000u) {
							g_objectTable[objectIdx].mobj->velocityOverrideSpeed = 1000;
						}
#ifdef XWA_MODERN
					}
#endif

					containerModelIdx = GetModelIndexFromType(OBJ_ContainerGrandePG);
					pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardContainerGrandeObjIdx],
														g_modelDefs[containerModelIdx].dockPoints[1].x,
														g_modelDefs[containerModelIdx].dockPoints[1].z,
														g_modelDefs[containerModelIdx].dockPoints[1].y);
					if (g_rotatedX + g_objectTable[g_yardContainerGrandeObjIdx].world_x >
						g_objectTable[objectIdx].world_x) {
						g_yardContext.centrifugeContainerStates[stateIdx].state = 0;
						g_objectTable[g_yardContext.centrifugeContainerStates[stateIdx].objectIdx]
							.objectType = OBJ_None;
						fsfx_PlaySound(189, (uint16_t)objectIdx, (unsigned int)g_localPlayer);
					}
					break;
				}

				default:
					break;
			}
		}

		++stateIdx;
	} while (stateIdx < g_yardContext.centrifugeContainerStateCount);
}

// FUNCTION: XWA 0x51ACE0
// Handle a player-owned craft colliding with Yard challenge geometry: slow the craft
// on impact, damage/bounce it off solid course pieces, and play course-specific SFX
// for smelting tubes and the centrifuge. Returns 1 when the caller should fall back
// to generic ricochet handling, 0 when this routine already resolved the impact.
int Yard_HandlePlayerChallengeObjectCollision(unsigned int playerObjIdx, unsigned int targetObjIdx,
											  uint16_t hitPartIdx) {
	ObjectRecord* targetObj;
	int objectType;

	g_objectTable[playerObjIdx].mobj->speed = 3 * g_objectTable[playerObjIdx].mobj->speed / 4;

	objectType = (uint16_t)g_objectTable[targetObjIdx].objectType;
	targetObj = &g_objectTable[targetObjIdx];

	if (objectType > 0x3B) {
		if (objectType <= OBJ_AccelRing2) {
			if (objectType >= OBJ_ChuteMouth)
				return 1;
			if (objectType != 65)
				objectType = OBJ_None;
		} else {
			switch (objectType) {
				case OBJ_SRTubeNOBend:
				case OBJ_SRTubeUP:
				case OBJ_SRTubeDown:
				case OBJ_SRTubeLH:
				case OBJ_SRTubeRH:
					if (g_players[g_localPlayer].objectIndex == (int)playerObjIdx &&
						g_yardContext.playerChallengeStates[g_localPlayer].carriedObjectPickedUp &&
						g_yardR2D2BumpSfxTimer > 944) {
						fsfx_PlaySound(124, playerObjIdx, g_localPlayer);
						g_yardR2D2BumpSfxTimer = 0;
					}
					if (!hitPartIdx) {
						fsfx_PlaySound(188, playerObjIdx, g_localPlayer);
						collide_damagecraft(playerObjIdx, 0xFFFFu, 0xFFFFFFFEu, 0x50u, 0);
					}
					return 1;
				case OBJ_SmeltingRoom:
					if (hitPartIdx == 9) {
						fsfx_PlaySound(188, playerObjIdx, g_localPlayer);
						collide_damagecraft(playerObjIdx, 0xFFFFu, 0xFFFFFFFEu, 0x50u, 0);
					}
					return 1;
				case OBJ_Centrifuge: {
					float damageAmount;

					objectType = hitPartIdx;
					if (objectType >= 3 && (objectType <= 4 || objectType == 12)) {
						damageAmount = g_yardContext.centrifugeMechanisms[0].angularVelocity;
						damageAmount *= g_yardCentrifugeDamageScale;
						collide_damagecraft(playerObjIdx, 0xFFFFu, 0xFFFFFFFEu,
											(unsigned int)(int64_t)damageAmount, 0);
					}
					return 1;
				}
				case OBJ_AccelRing3:
				case OBJ_ContainerGrandePG:
				case OBJ_JunkBlock:
				case OBJ_MoltenBlock:
					return 1;
				case OBJ_Junk01:
				case OBJ_Junk02:
				case OBJ_Junk03:
				case OBJ_Junk04:
				case OBJ_Junk05:
				case OBJ_Junk06:
				case OBJ_Junk07:
				case OBJ_Junk08:
				case OBJ_Junk09:
				case OBJ_Junk10:
					objectType = OBJ_CorellianTransport2;
					break;
				default:
					objectType = OBJ_None;
					break;
			}
		}
	}

	if (objectType < OBJ_CorellianTransport2 && objectType != 50 && targetObj->genusId > GENUS_Transport)
		return 1;
	collide_damagecraft(playerObjIdx, 0xFFFFu, 0xFFFFFFFEu, 0x28u, 0);
	collide_applyCraftImpactBounce(playerObjIdx, targetObjIdx);
	return 0;
}

// FUNCTION: XWA 0x51AA80
// Yard-mode collision hook for a source/target pair. Player-owned sources delegate to
// the player-craft handler; otherwise it suppresses the generic ricochet pass for
// fixed course geometry (return 0) and plays debris/R2D2 bump SFX. Returns 1 when the
// generic collision pass should continue.
int Yard_HandleChallengeObjectCollision(unsigned int sourceObjIdx, unsigned int targetObjIdx,
										int16_t hitPartPlusOne) {
	ObjectRecord* src;

	if (sourceObjIdx == targetObjIdx)
		return 0;
	src = &g_objectTable[sourceObjIdx];
	if (src->playerOwnerIdx != -1)
		return Yard_HandlePlayerChallengeObjectCollision(sourceObjIdx, targetObjIdx,
														 (uint16_t)(hitPartPlusOne - 1));

	switch (src->objectType) {
		case OBJ_R2D2:
			if (!g_flightSimSideEffectsSuppressed && g_yardR2D2BumpSfxTimer > 944) {
				fsfx_PlaySound(124, 0xFFFFu, g_localPlayer);
				g_yardR2D2BumpSfxTimer = 0;
			}
			return 1;
		case OBJ_ChuteMouth:
		case OBJ_ChuteTunnel:
		case OBJ_SalvageRoom:
		case OBJ_Compactor:
		case OBJ_AccelRing:
		case OBJ_AccelRing2:
		case OBJ_SmeltingRoom:
		case OBJ_SRTubeNOBend:
		case OBJ_SRTubeUP:
		case OBJ_SRTubeDown:
		case OBJ_SRTubeLH:
		case OBJ_SRTubeRH:
		case OBJ_Centrifuge:
		case OBJ_AccelRing3:
		case OBJ_ContainerGrandePG:
		case OBJ_Asteroid01:
		case OBJ_Asteroid02:
		case OBJ_Asteroid03:
		case OBJ_JunkBlock:
		case OBJ_MoltenBlock:
			return 0;
		case OBJ_Junk01:
		case OBJ_Junk02:
		case OBJ_Junk03:
		case OBJ_Junk04:
		case OBJ_Junk05:
		case OBJ_Junk06:
		case OBJ_Junk07:
		case OBJ_Junk08:
		case OBJ_Junk09:
		case OBJ_Junk10: {
			uint16_t tt = (uint16_t)g_objectTable[targetObjIdx].objectType;
			if ((tt >= OBJ_Junk01 && tt <= OBJ_Junk10) || g_flightSimSideEffectsSuppressed)
				return 1;
			switch (GameRand2() & 3) {
				case 0:
					fsfx_PlaySound(172, sourceObjIdx, g_localPlayer);
					break;
				case 1:
					fsfx_PlaySound(173, sourceObjIdx, g_localPlayer);
					break;
				case 2:
					fsfx_PlaySound(174, sourceObjIdx, g_localPlayer);
					break;
				case 3:
					fsfx_PlaySound(175, sourceObjIdx, g_localPlayer);
					break;
			}
			return 1;
		}
		default:
			return 1;
	}
}

static __inline uint16_t Yard_CentrifugeSpinMeshIndex(int mechanismIdx) {
	switch (mechanismIdx) {
		case 0:
			return 3;
		case 1:
			return 4;
		case 2:
			return 12;
		default:
			return 12;
	}
}

static __inline uint16_t Yard_CentrifugeDoorMeshIndex(int mechanismIdx) {
	switch (mechanismIdx) {
		case 0:
			return 8;
		case 1:
			return 7;
		case 2:
			return 6;
		default:
			return 6;
	}
}

static __inline int Yard_CentrifugeCrossedMeshHalfTurn(uint8_t oldRotation, uint8_t newRotation) {
	return ((oldRotation & 0x80u) + (newRotation & 0x80u)) == 0x80u;
}

// FUNCTION: XWA 0x519D60
void Yard_UpdateCentrifugeMechanisms(int deltaTicks) {
	int sourceObjIdx;
	CraftData* centrifugeCraft;
	int result;
	int elapsedStepTicks;
	int stepCount;
	int mechanismIdx;

	sourceObjIdx = g_yardCentrifugeObjIdx;
	centrifugeCraft = g_objectTable[g_yardCentrifugeObjIdx].mobj->pCraft;
	result = deltaTicks + g_yardContext.centrifugeMechanismTickRemainder;
	elapsedStepTicks = 0;
	g_yardContext.centrifugeMechanismTickRemainder += deltaTicks;
	if (g_yardContext.centrifugeMechanismTickRemainder >= 10) {
		do {
			result -= 10;
			elapsedStepTicks += 10;
		} while (result >= 10);
		g_yardContext.centrifugeMechanismTickRemainder = result;
	}

	if (elapsedStepTicks <= 0) {
		return;
	}

	stepCount = elapsedStepTicks / 10;
	for (mechanismIdx = 0; mechanismIdx < 3; ++mechanismIdx) {
		unsigned int localPlayer;

		localPlayer = (unsigned int)g_localPlayer;

		switch (g_yardContext.centrifugeMechanisms[mechanismIdx].state) {
			case 0:
				g_yardContext.centrifugeMechanisms[mechanismIdx].delayTicks -= stepCount;
				if (g_yardContext.centrifugeMechanisms[mechanismIdx].delayTicks <= 0) {
					g_yardContext.centrifugeMechanisms[mechanismIdx].delayTicks = 0;
					++g_yardContext.centrifugeMechanisms[mechanismIdx].cycleParity;
					if ((unsigned int)g_yardContext.centrifugeMechanisms[mechanismIdx].cycleParity > 1u) {
						g_yardContext.centrifugeMechanisms[mechanismIdx].cycleParity = 0;
					}
					g_yardContext.centrifugeMechanisms[mechanismIdx].state = 1;
					if (g_yardContext.playerChallengeStates[localPlayer].courseState == 6) {
						(void)fsfx_PlaySound(187, sourceObjIdx, localPlayer);
					}
				}
				break;

			case 1: {
				uint16_t meshIdx;
				uint8_t rotation;

				meshIdx = Yard_CentrifugeDoorMeshIndex(mechanismIdx);
				rotation = (uint8_t)(centrifugeCraft->meshRotation[meshIdx] + stepCount);
				centrifugeCraft->meshRotation[meshIdx] = rotation;
				if (rotation >= 0x36u) {
					centrifugeCraft->meshRotation[meshIdx] = 0x36u;
					g_yardContext.centrifugeMechanisms[mechanismIdx].state = 2;
				}
				break;
			}

			case 2: {
				uint16_t meshIdx;
				uint8_t rotation;

				meshIdx = Yard_CentrifugeSpinMeshIndex(mechanismIdx);
				g_yardContext.centrifugeMechanisms[mechanismIdx].angularAccel =
					g_yardContext.centrifugeMechanisms[mechanismIdx].angularAccel -
					(float)stepCount * -0.005f;
				g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity =
					g_yardContext.centrifugeMechanisms[mechanismIdx].angularAccel * (float)stepCount +
					g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity;
				g_yardContext.centrifugeMechanisms[mechanismIdx].meshRotationAccum =
					(float)stepCount * g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity +
					g_yardContext.centrifugeMechanisms[mechanismIdx].meshRotationAccum;
				rotation = (uint8_t)(int)(g_yardContext.centrifugeMechanisms[mechanismIdx].meshRotationAccum *
										  0.00390625f);
				centrifugeCraft->meshRotation[meshIdx] = rotation;

				if (g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity >=
					800.0f - (float)(10 * mechanismIdx)) {
					g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity = 800.0f;
					g_yardContext.centrifugeMechanisms[mechanismIdx].angularAccel = 0.0f;
					g_yardContext.centrifugeMechanisms[mechanismIdx].delayTicks = 250;
					g_yardContext.centrifugeMechanisms[mechanismIdx].state = 3;
				}

				if (!g_flightSimSideEffectsSuppressed) {
					localPlayer = (unsigned int)g_localPlayer;
					if (g_yardContext.playerChallengeStates[localPlayer].courseState == 6 &&
						g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity > 400.0f &&
						((rotation & 0x80u) + ((uint8_t)deltaTicks & 0x80u)) == 0x80u) {
						(void)fsfx_PlaySound(191, sourceObjIdx, localPlayer);
					}
				}
				break;
			}

			case 3: {
				uint16_t meshIdx;
				int oldRotation;
				uint8_t rotation;

				meshIdx = Yard_CentrifugeSpinMeshIndex(mechanismIdx);
				g_yardContext.centrifugeMechanisms[mechanismIdx].meshRotationAccum =
					(float)stepCount * g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity +
					g_yardContext.centrifugeMechanisms[mechanismIdx].meshRotationAccum;
				oldRotation = centrifugeCraft->meshRotation[meshIdx];
				rotation = (uint8_t)(int)(g_yardContext.centrifugeMechanisms[mechanismIdx].meshRotationAccum *
										  0.00390625f);
				centrifugeCraft->meshRotation[meshIdx] = rotation;

				if (!g_flightSimSideEffectsSuppressed) {
					localPlayer = (unsigned int)g_localPlayer;
					if (g_yardContext.playerChallengeStates[localPlayer].courseState == 6 &&
						Yard_CentrifugeCrossedMeshHalfTurn(oldRotation, rotation)) {
						(void)fsfx_PlaySound(191, sourceObjIdx, localPlayer);
					}
				}

				g_yardContext.centrifugeMechanisms[mechanismIdx].delayTicks -= stepCount;
				if (g_yardContext.centrifugeMechanisms[mechanismIdx].delayTicks <= 0) {
					g_yardContext.centrifugeMechanisms[mechanismIdx].delayTicks = 0;
					g_yardContext.centrifugeMechanisms[mechanismIdx].state = 4;
				}
				break;
			}

			case 4: {
				uint16_t meshIdx;
				int oldRotation;
				uint8_t rotation;

				meshIdx = Yard_CentrifugeSpinMeshIndex(mechanismIdx);
				oldRotation = centrifugeCraft->meshRotation[meshIdx];
				g_yardContext.centrifugeMechanisms[mechanismIdx].angularAccel =
					g_yardContext.centrifugeMechanisms[mechanismIdx].angularAccel -
					(float)stepCount * -0.005f;
				g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity =
					g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity -
					g_yardContext.centrifugeMechanisms[mechanismIdx].angularAccel * (float)stepCount;
				if ((unsigned int)oldRotation <= 0xe6u) {
					if (g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity < 200.0f) {
						g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity = 200.0f;
					}
				} else if (g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity < 50.0f) {
					g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity = 50.0f;
				}

				g_yardContext.centrifugeMechanisms[mechanismIdx].meshRotationAccum =
					(float)stepCount * g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity +
					g_yardContext.centrifugeMechanisms[mechanismIdx].meshRotationAccum;
				rotation = (uint8_t)(int)(g_yardContext.centrifugeMechanisms[mechanismIdx].meshRotationAccum *
										  0.00390625f);
				centrifugeCraft->meshRotation[meshIdx] = rotation;

				if (!g_flightSimSideEffectsSuppressed) {
					localPlayer = (unsigned int)g_localPlayer;
					if (g_yardContext.playerChallengeStates[localPlayer].courseState == 6 &&
						g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity > 400.0f &&
						Yard_CentrifugeCrossedMeshHalfTurn(oldRotation, rotation)) {
						(void)fsfx_PlaySound(191, sourceObjIdx, localPlayer);
					}
				}

				if (g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity > 50.0f ||
					centrifugeCraft->meshRotation[meshIdx] != 0) {
					break;
				}

				g_yardContext.centrifugeMechanisms[mechanismIdx].angularVelocity = 0.0f;
				g_yardContext.centrifugeMechanisms[mechanismIdx].angularAccel = 0.0f;
				g_yardContext.centrifugeMechanisms[mechanismIdx].meshRotationAccum = 0.0f;
				g_yardContext.centrifugeMechanisms[mechanismIdx].delayTicks = 250;
				g_yardContext.centrifugeMechanisms[mechanismIdx].state = 5;
				localPlayer = (unsigned int)g_localPlayer;
				if (g_yardContext.playerChallengeStates[localPlayer].courseState == 6) {
					(void)fsfx_PlaySound(187, sourceObjIdx, localPlayer);
					(void)fsfx_PlaySound(189, sourceObjIdx, (unsigned int)g_localPlayer);
				}

				{
					int slotIdx;

					slotIdx = 0;
					while (slotIdx < g_yardContext.centrifugeContainerStateCount &&
						   g_yardContext.centrifugeContainerStates[slotIdx].state != 0) {
						++slotIdx;
					}
					if (slotIdx >= g_yardContext.centrifugeContainerStateCount) {
						if (g_yardContext.centrifugeContainerStateCount >= 19) {
							break;
						}
						slotIdx = g_yardContext.centrifugeContainerStateCount;
					}

					g_yardContext.centrifugeContainerStates[(uint16_t)slotIdx].objectIdx =
						Yard_SpawnObjectAtWorldPos(OBJ_MoltenBlock,
												   g_yardCentrifugeMoltenBlockSpawnWorldX[mechanismIdx],
												   g_yardCentrifugeMoltenBlockSpawnWorldY[mechanismIdx],
												   g_yardCentrifugeMoltenBlockSpawnWorldZ[mechanismIdx],
												   (int16_t)0xc000, (int16_t)0x4000);
					if (g_yardContext.centrifugeContainerStates[(uint16_t)slotIdx].objectIdx != 0xffff) {
						g_yardContext.centrifugeContainerStates[(uint16_t)slotIdx].state = 1;
						if (slotIdx == g_yardContext.centrifugeContainerStateCount) {
							++g_yardContext.centrifugeContainerStateCount;
						}
						g_objectTable[g_yardContext.centrifugeContainerStates[(uint16_t)slotIdx].objectIdx]
							.mobj->velocityOverrideActive = 1;
						g_objectTable[g_yardContext.centrifugeContainerStates[(uint16_t)slotIdx].objectIdx]
							.mobj->velocityOverrideSpeed = 1;
						g_objectTable[g_yardContext.centrifugeContainerStates[(uint16_t)slotIdx].objectIdx]
							.mobj->velocityOverrideElapsed = 1;
						g_objectTable[g_yardContext.centrifugeContainerStates[(uint16_t)slotIdx].objectIdx]
							.mobj->velocityOverrideDirX = 0;
						g_objectTable[g_yardContext.centrifugeContainerStates[(uint16_t)slotIdx].objectIdx]
							.mobj->velocityOverrideDirY = 0;
						g_objectTable[g_yardContext.centrifugeContainerStates[(uint16_t)slotIdx].objectIdx]
							.mobj->velocityOverrideDirZ = 0x7fff;
					}
				}
				break;
			}

			case 5: {
				uint16_t meshIdx;
				uint8_t rotation;

				meshIdx = Yard_CentrifugeDoorMeshIndex(mechanismIdx);
				rotation = (uint8_t)(centrifugeCraft->meshRotation[meshIdx] - stepCount);
				centrifugeCraft->meshRotation[meshIdx] = rotation;
				if ((int8_t)rotation > 0) {
					break;
				}

				centrifugeCraft->meshRotation[meshIdx] = 0;
				g_yardContext.centrifugeMechanisms[mechanismIdx].state = 0;
				localPlayer = (unsigned int)g_localPlayer;
				if (g_yardContext.playerChallengeStates[localPlayer].courseState == 6) {
					switch (abs(abs(GameRand2() & 0xffff) & 1)) {
						case 0:
							(void)fsfx_PlaySound(183, sourceObjIdx, localPlayer);
							break;
						case 1:
							(void)fsfx_PlaySound(184, sourceObjIdx, (unsigned int)g_localPlayer);
							break;
					}
				}
				break;
			}

			default:
				break;
		}
	}
}

// FUNCTION: XWA 0x517EE0
void Yard_UpdateRubbleChunkMotion(int deltaTicks) {
	ModelIndex compactorModelIdx;
	int dock0WorldX;
	int dock0WorldY;
	int dock0WorldZ;
	int dock2WorldX;
	int dock2WorldY;
	int dock2WorldZ;
	uint16_t slotIdx;

	compactorModelIdx = GetModelIndexFromType(OBJ_Compactor);
	pai_RotateLocalVectorToWorldScratch(
		&g_objectTable[g_yardContext.compactorObjIdx], g_modelDefs[compactorModelIdx].dockPoints[0].x,
		g_modelDefs[compactorModelIdx].dockPoints[0].z, g_modelDefs[compactorModelIdx].dockPoints[0].y);
	dock0WorldX = g_rotatedX + g_objectTable[g_yardContext.compactorObjIdx].world_x;
	dock0WorldY = g_rotatedY + g_objectTable[g_yardContext.compactorObjIdx].world_y;
	dock0WorldZ = g_rotatedZ + g_objectTable[g_yardContext.compactorObjIdx].world_z;

	pai_RotateLocalVectorToWorldScratch(
		&g_objectTable[g_yardContext.compactorObjIdx], g_modelDefs[compactorModelIdx].dockPoints[2].x,
		g_modelDefs[compactorModelIdx].dockPoints[2].z, g_modelDefs[compactorModelIdx].dockPoints[2].y);
	dock2WorldX = g_rotatedX + g_objectTable[g_yardContext.compactorObjIdx].world_x;
	dock2WorldY = g_rotatedY + g_objectTable[g_yardContext.compactorObjIdx].world_y;
	dock2WorldZ = g_rotatedZ + g_objectTable[g_yardContext.compactorObjIdx].world_z;

	for (slotIdx = 0; slotIdx < g_yardContext.rubbleChunkStateCount; ++slotIdx) {
		int objectIdx;
		ObjectTypeId objectType;

		if (g_yardContext.rubbleChunkStates[slotIdx].state == 0) {
			continue;
		}

		objectIdx = g_yardContext.rubbleChunkStates[slotIdx].objectIdx;
		objectType = g_objectTable[objectIdx].objectType;
		if ((uint16_t)objectType < (uint16_t)OBJ_Junk01 || (uint16_t)objectType > (uint16_t)OBJ_Junk10) {
			g_yardContext.rubbleChunkStates[slotIdx].state = 0;
			continue;
		}

		switch (g_yardContext.rubbleChunkStates[slotIdx].state) {
			case 1: {
				Q16Angle yaw;
				Q16Angle pitch;

				trig2_ctop(g_objectTable[objectIdx].mobj->velocityOverrideDirX,
						   g_objectTable[objectIdx].mobj->velocityOverrideDirY,
						   g_objectTable[objectIdx].mobj->velocityOverrideDirZ);
				yaw = trig2_xyangle;
				pitch = targetPitch;
				if (trig2_xyangle > 0xa000u) {
					yaw -= deltaTicks;
				} else if (trig2_xyangle < 0x6000u) {
					yaw += deltaTicks;
				}
				if (targetPitch > 0x6000u) {
					pitch -= deltaTicks;
				} else if (targetPitch < 0x2000u) {
					pitch += deltaTicks;
				}

				trig2_rho = 0x7fff;
				trig2_theta = (uint16_t)yaw;
				trig2_phi = (uint16_t)pitch;
				trig2_ptoc3dim();
				g_objectTable[objectIdx].mobj->velocityOverrideDirX = (int16_t)trig2_yoffset;
				g_objectTable[objectIdx].mobj->velocityOverrideDirY = (int16_t)trig2_xoffset;
				g_objectTable[objectIdx].mobj->velocityOverrideDirZ = (int16_t)trig2_zoffset;
				if (g_objectTable[objectIdx].world_y < -120000) {
					g_yardContext.rubbleChunkStates[slotIdx].state = 2;
				}
				break;
			}
			case 2: {
				ModelIndex modelIdx;
				MobileObject* rubbleMobj;
				Q16Angle currentYaw;
				Q16Angle currentPitch;
				int worldX;
				int worldY;
				int worldZ;
				int targetX;
				int targetY;
				int targetZ;
				int dx;
				int dy;
				int dz;
				Q16Angle desiredYaw;
				Q16Angle desiredPitch;
				unsigned int distance;
				int turnRate;

				rubbleMobj = g_objectTable[objectIdx].mobj;
				trig2_ctop(rubbleMobj->velocityOverrideDirX, rubbleMobj->velocityOverrideDirY,
						   rubbleMobj->velocityOverrideDirZ);
				currentPitch = targetPitch;
				currentYaw = trig2_xyangle;
				worldX = g_objectTable[objectIdx].world_x;
				worldY = g_objectTable[objectIdx].world_y;
				worldZ = g_objectTable[objectIdx].world_z;

				modelIdx = GetModelIndexFromType(OBJ_Compactor);
				pai_RotateLocalVectorToWorldScratch(
					&g_objectTable[g_yardContext.compactorObjIdx], g_modelDefs[modelIdx].dockPoints[1].x,
					g_modelDefs[modelIdx].dockPoints[1].z, g_modelDefs[modelIdx].dockPoints[1].y);
				targetX = g_rotatedX + g_objectTable[g_yardContext.compactorObjIdx].world_x +
						  rubbleMobj->renderOffsetX % 80;
				targetY = g_rotatedY + g_objectTable[g_yardContext.compactorObjIdx].world_y +
						  rubbleMobj->renderOffsetZ % 140 + rubbleMobj->renderOffsetY % 80;
				targetZ = g_rotatedZ + g_objectTable[g_yardContext.compactorObjIdx].world_z;
				dx = targetX - worldX;
				dy = targetY - worldY;
				dz = targetZ - worldZ;
				trig2_ctop(dx, dy, dz);
				desiredYaw = trig2_xyangle;
				desiredPitch = targetPitch;
				distance = (unsigned int)collide_roughdistance3d(dx, dy, dz);
				turnRate = distance > 0x1388u ? 15 : 200;

				if (g_yardContext.compactorCycleState == 2 && worldX > dock0WorldX) {
					if ((int16_t)(currentYaw - (int)desiredYaw) > 0) {
						currentYaw -= deltaTicks * turnRate;
					} else if ((int16_t)(currentYaw - (int)desiredYaw) < 0) {
						currentYaw += deltaTicks * turnRate;
					}
					if ((int16_t)(currentPitch - (int)desiredPitch) > 0) {
						currentPitch -= deltaTicks * turnRate;
					} else if ((int16_t)(currentPitch - (int)desiredPitch) < 0) {
						currentPitch += deltaTicks * turnRate;
					}
				} else {
					if ((int16_t)(currentYaw - (int)desiredYaw) > 0) {
						currentYaw += deltaTicks * turnRate;
					} else if ((int16_t)(currentYaw - (int)desiredYaw) < 0) {
						currentYaw -= deltaTicks * turnRate;
					}
					if ((int16_t)(currentPitch - (int)desiredPitch) > 0) {
						currentPitch += deltaTicks * turnRate;
					} else if ((int16_t)(currentPitch - (int)desiredPitch) < 0) {
						currentPitch -= deltaTicks * turnRate;
					}
				}

				trig2_rho = 0x7fff;
				trig2_theta = (uint16_t)currentYaw;
				trig2_phi = (uint16_t)currentPitch;
				trig2_ptoc3dim();
				g_objectTable[objectIdx].mobj->velocityOverrideDirX = (int16_t)trig2_yoffset;
				g_objectTable[objectIdx].mobj->velocityOverrideDirY = (int16_t)trig2_xoffset;
				g_objectTable[objectIdx].mobj->velocityOverrideDirZ = (int16_t)trig2_zoffset;
				if (distance < 0x1f4u) {
					g_yardContext.rubbleChunkStates[slotIdx].state = 3;
					g_objectTable[objectIdx].mobj->velocityOverrideActive = 0;
					g_objectTable[objectIdx].mobj->velocityOverrideSpeed = 0;
					g_objectTable[objectIdx].mobj->velocityOverrideElapsed = 0;
					g_objectTable[objectIdx].mobj->spinRate = 0;
					g_objectTable[objectIdx].mobj->spinRateFrac = 0;
				}
				break;
			}
			case 3: {
				if (g_objectTable[objectIdx].world_x <= dock0WorldX ||
					g_objectTable[objectIdx].world_x >= dock2WorldX ||
					g_objectTable[objectIdx].world_y <= dock0WorldY ||
					g_objectTable[objectIdx].world_y >= dock2WorldY ||
					g_objectTable[objectIdx].world_z <= dock2WorldZ ||
					g_objectTable[objectIdx].world_z >= dock0WorldZ) {
					g_yardContext.rubbleChunkStates[slotIdx].state = 2;
					g_objectTable[objectIdx].mobj->velocityOverrideActive = 1;
					g_objectTable[objectIdx].mobj->velocityOverrideSpeed = 100;
					g_objectTable[objectIdx].mobj->velocityOverrideElapsed = 0;
				}
				break;
			}
		}
	}
}

// FUNCTION: XWA 0x51BD20
YardCraftScoreTable* Yard_AllocCraftScoreTable(void) {
	YardCraftScoreTable* table;
	int categoryIdx;

	table = (YardCraftScoreTable*)Memory_AllocTagged(YARD_CRAFT_SCORE_TABLE_FREE_TAG,
													 sizeof(YardCraftScoreTable));
	if (table) {
		table->objectType = 0;
		memset(table->pilotNames, 0, sizeof(table->pilotNames));
		for (categoryIdx = 0; categoryIdx < YARD_SCORE_CATEGORY_COUNT; ++categoryIdx) {
			int entryIdx;

			for (entryIdx = 0; entryIdx < YARD_SCORE_ENTRIES_PER_CATEGORY; ++entryIdx) {
				table->scores[categoryIdx][entryIdx] = 3600;
			}
		}
	}

	return table;
}

// FUNCTION: XWA 0x51BCF0
#ifndef XWA_MODERN
__inline
#endif
	YardCraftScoreTable* Yard_FindCraftScoreTableByObjectType(ObjectTypeId objectType,
															  YardHighScoreTable* table) {
	int i;

	for (i = 0; i < table->count; ++i) {
		YardCraftScoreTable* craftTable;

		craftTable = table->craftTables[i];
		if (craftTable->objectType == (int)objectType) {
			return craftTable;
		}
	}

	return NULL;
}

// FUNCTION: XWA 0x51B7F0
int Yard_InsertCraftHighScore(YardHighScoreTable* table, int categoryIdx, int objectType,
							  const char* pilotName, int score) {
	YardCraftScoreTable* craftTable;
	int rank;

	if (categoryIdx < 0 || categoryIdx > YARD_SCORE_CATEGORY_COUNT - 1) {
		return 0;
	}

	craftTable = Yard_FindCraftScoreTableByObjectType((ObjectTypeId)objectType, table);

	if (craftTable != NULL) {
		rank = 0;
		while (1) {
			if (score < craftTable->scores[categoryIdx][rank]) {
				break;
			}
			++rank;
			if (rank >= YARD_SCORE_ENTRIES_PER_CATEGORY) {
				return 0;
			}
		}

		if (rank < YARD_SCORE_ENTRIES_PER_CATEGORY - 1) {
			int entryIdx;

			for (entryIdx = YARD_SCORE_ENTRIES_PER_CATEGORY - 1; entryIdx > rank; --entryIdx) {
				craftTable->scores[categoryIdx][entryIdx] = craftTable->scores[categoryIdx][entryIdx - 1];
				strncpy(craftTable->pilotNames[categoryIdx][entryIdx],
						craftTable->pilotNames[categoryIdx][entryIdx - 1], YARD_SCORE_PILOT_NAME_SLOT_LENGTH);
			}
		}

		strncpy(craftTable->pilotNames[categoryIdx][rank], pilotName, YARD_SCORE_PILOT_NAME_COPY_LENGTH);
		craftTable->pilotNames[categoryIdx][rank][YARD_SCORE_PILOT_NAME_COPY_LENGTH] = '\0';
		craftTable->scores[categoryIdx][rank] = score;
		return rank + 1;
	} else {
		YardCraftScoreTable* newCraftTable;
		YardCraftScoreTable** resizedTables;

		newCraftTable = Yard_AllocCraftScoreTable();
		if (newCraftTable == NULL) {
			return 0;
		}

		resizedTables = (YardCraftScoreTable**)Memory_ReallocTagged(
			YARD_CRAFT_SCORE_TABLES_TAG, table->craftTables,
			(size_t)(table->count + 1) * sizeof(table->craftTables[0]));
		if (resizedTables != NULL) {
			table->craftTables = resizedTables;
			table->craftTables[table->count] = newCraftTable;
			++table->count;
			newCraftTable->objectType = objectType;

			strncpy(newCraftTable->pilotNames[categoryIdx][0], pilotName, YARD_SCORE_PILOT_NAME_COPY_LENGTH);
			newCraftTable->pilotNames[categoryIdx][0][YARD_SCORE_PILOT_NAME_COPY_LENGTH] = '\0';
			newCraftTable->scores[categoryIdx][0] = score;
			return 1;
		}

		Memory_FreeTagged(YARD_CRAFT_SCORE_TABLE_FREE_TAG, newCraftTable);
		return 0;
	}
}

// FUNCTION: XWA 0x51B990
YardHighScoreTable* Yard_LoadHighScoreTable(void) {
	enum {
		YARD_HIGH_SCORE_MAGIC = 0x13de3c1f,
		YARD_HIGH_SCORE_CATEGORY_COUNT = 8,
		YARD_HIGH_SCORE_ENTRIES_PER_CATEGORY = 10
	};

	YardHighScoreTable* table;
#ifdef XWA_MODERN
	XwaFile* stream;
#else
	FILE* stream;
#endif
	int value;

	table = (YardHighScoreTable*)Memory_AllocTagged(YARD_HIGH_SCORE_TABLE_TAG, sizeof(YardHighScoreTable));
	if (table != NULL) {
		table->count = 0;
		table->craftTables = NULL;
	}

	if (table != NULL) {
#ifdef XWA_MODERN
		stream = File_Open(AERON_VFS_ROOT_USER, "xwahs.tbl", "rb");
#else
		stream = fopen("xwahs.tbl", "rb");
#endif
		if (stream != NULL) {
#ifdef XWA_MODERN
			value = 0;
#endif
#ifdef XWA_MODERN
			File_ReadCount(stream, &value, sizeof(value));
#else
			fread(&value, sizeof(value), 1, stream);
#endif
			if (value != YARD_HIGH_SCORE_MAGIC) {
				Memory_FreeTagged(YARD_HIGH_SCORE_TABLE_TAG, table);
#ifdef XWA_MODERN
				File_Close(stream);
#else
				fclose(stream);
#endif
				return NULL;
			}

#ifdef XWA_MODERN
			File_ReadCount(stream, &value, sizeof(value));
			File_ReadCount(stream, &table->count, sizeof(table->count));
#else
			fread(&value, sizeof(value), 1, stream);
			fread(table, sizeof(table->count), 1, stream);
#endif

			if (table->craftTables != NULL) {
				Memory_FreeTagged(YARD_CRAFT_SCORE_TABLES_TAG, table->craftTables);
			}

			if (table->count != 0) {
				table->craftTables = (YardCraftScoreTable**)Memory_AllocTagged(
					YARD_CRAFT_SCORE_TABLES_TAG, (size_t)table->count * sizeof(table->craftTables[0]));
				if (table->craftTables != NULL) {
					value = 0;
					while (value < table->count) {
						YardCraftScoreTable* craftTable;

						craftTable = Yard_AllocCraftScoreTable();
						if (craftTable == NULL) {
							table->count = value;
							break;
						}
						table->craftTables[value] = craftTable;
						++value;
					}

					for (value = 0; value < table->count; ++value) {
						YardCraftScoreTable* craftTable;
						int categoryIdx;

						craftTable = table->craftTables[value];
#ifdef XWA_MODERN
						File_ReadCount(stream, &craftTable->objectType, sizeof(craftTable->objectType));
#else
						fread(&craftTable->objectType, sizeof(craftTable->objectType), 1, stream);
#endif
						for (categoryIdx = 0; categoryIdx < YARD_HIGH_SCORE_CATEGORY_COUNT; ++categoryIdx) {
							int entryIdx;

							for (entryIdx = 0; entryIdx < YARD_HIGH_SCORE_ENTRIES_PER_CATEGORY; ++entryIdx) {
#ifdef XWA_MODERN
								File_ReadCount(stream, craftTable->pilotNames[categoryIdx][entryIdx],
											   sizeof(craftTable->pilotNames[categoryIdx][entryIdx]));
								File_ReadCount(stream, &craftTable->scores[categoryIdx][entryIdx],
											   sizeof(craftTable->scores[categoryIdx][entryIdx]));
#else
								int charIdx;

								for (charIdx = 0; charIdx < YARD_SCORE_PILOT_NAME_SLOT_LENGTH; ++charIdx) {
									fread(&craftTable->pilotNames[categoryIdx][entryIdx][charIdx], 1, 1,
										  stream);
								}
								fread(&craftTable->scores[categoryIdx][entryIdx],
									  sizeof(craftTable->scores[categoryIdx][entryIdx]), 1, stream);
#endif
							}
						}
					}
				}
			}

#ifdef XWA_MODERN
			File_Close(stream);
#else
			fclose(stream);
#endif
			return table;
		}

		Memory_FreeTagged(YARD_HIGH_SCORE_TABLE_TAG, table);
	}

	return NULL;
}

// FUNCTION: XWA 0x51B6E0
YardHighScoreTable* Yard_LoadOrCreateHighScoreTable(void) {
	YardHighScoreTable* table;

	table = Yard_LoadHighScoreTable();
	if (table == NULL) {
		table =
			(YardHighScoreTable*)Memory_AllocTagged(YARD_HIGH_SCORE_TABLE_TAG, sizeof(YardHighScoreTable));
		if (table != NULL) {
			table->count = 0;
			table->craftTables = NULL;
		}
	}

	return table;
}

// FUNCTION: XWA 0x51BB80
int Yard_SaveHighScoreTable(YardHighScoreTable* table) {
	enum {
		YARD_HIGH_SCORE_MAGIC = 0x13de3c1f,
		YARD_HIGH_SCORE_VERSION = 1,
		YARD_HIGH_SCORE_CATEGORY_COUNT = 8,
		YARD_HIGH_SCORE_ENTRIES_PER_CATEGORY = 10
	};

#ifdef XWA_MODERN
	XwaFile* stream;
#else
	FILE* stream;
#endif

#ifdef XWA_MODERN
	stream = File_Open(AERON_VFS_ROOT_USER, "xwahs.tbl", "wb");
#else
	stream = fopen("xwahs.tbl", "wb");
#endif
	if (stream != NULL) {
		int value;

		value = YARD_HIGH_SCORE_MAGIC;
#ifdef XWA_MODERN
		if (!File_WriteCount(stream, &value, sizeof(value))) {
			File_Close(stream);
#else
		if (fwrite(&value, sizeof(value), 1, stream) == 0) {
			fclose(stream);
#endif
			return 0;
		}

		value = YARD_HIGH_SCORE_VERSION;
#ifdef XWA_MODERN
		if (!File_WriteCount(stream, &value, sizeof(value))) {
			File_Close(stream);
#else
		if (fwrite(&value, sizeof(value), 1, stream) == 0) {
			fclose(stream);
#endif
			return 0;
		}

#ifdef XWA_MODERN
		if (!File_WriteCount(stream, &table->count, sizeof(table->count))) {
			File_Close(stream);
#else
		if (fwrite(table, sizeof(table->count), 1, stream) == 0) {
			fclose(stream);
#endif
			return 0;
		}

		for (value = 0; value < table->count; ++value) {
			YardCraftScoreTable* craftTable;
			int categoryIdx;

			craftTable = table->craftTables[value];
#ifdef XWA_MODERN
			File_WriteCount(stream, &craftTable->objectType, sizeof(craftTable->objectType));
#else
			fwrite(&craftTable->objectType, sizeof(craftTable->objectType), 1, stream);
#endif

			for (categoryIdx = 0; categoryIdx < YARD_HIGH_SCORE_CATEGORY_COUNT; ++categoryIdx) {
				int entryIdx;

				for (entryIdx = 0; entryIdx < YARD_HIGH_SCORE_ENTRIES_PER_CATEGORY; ++entryIdx) {
#ifdef XWA_MODERN
					File_WriteCount(stream, craftTable->pilotNames[categoryIdx][entryIdx],
									sizeof(craftTable->pilotNames[categoryIdx][entryIdx]));
					File_WriteCount(stream, &craftTable->scores[categoryIdx][entryIdx],
									sizeof(craftTable->scores[categoryIdx][entryIdx]));
#else
					int charIdx;

					for (charIdx = 0; charIdx < YARD_SCORE_PILOT_NAME_SLOT_LENGTH; ++charIdx) {
						fwrite(&craftTable->pilotNames[categoryIdx][entryIdx][charIdx], 1, 1, stream);
					}
					fwrite(&craftTable->scores[categoryIdx][entryIdx],
						   sizeof(craftTable->scores[categoryIdx][entryIdx]), 1, stream);
#endif
				}
			}
		}

#ifdef XWA_MODERN
		File_Close(stream);
#else
		fclose(stream);
#endif
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x51B710
int Yard_FreeHighScoreTable(YardHighScoreTable* table) {
	int i;

	if (table == NULL) {
		return 0;
	}

	if (Yard_SaveHighScoreTable(table)) {
		for (i = 0; i < table->count; ++i) {
			if (table->craftTables[i] != NULL) {
				Memory_FreeTagged(YARD_CRAFT_SCORE_TABLE_FREE_TAG, table->craftTables[i]);
			}
		}

		if (table->craftTables != NULL) {
			Memory_FreeTagged(YARD_CRAFT_SCORE_TABLES_TAG, table->craftTables);
			table->craftTables = NULL;
		}

		table->count = 0;
		Memory_FreeTagged(YARD_HIGH_SCORE_TABLE_TAG, table);
		return 1;
	}

	for (i = 0; i < table->count; ++i) {
		if (table->craftTables[i] != NULL) {
			Memory_FreeTagged(YARD_CRAFT_SCORE_TABLE_FREE_TAG, table->craftTables[i]);
		}
	}

	if (table->craftTables != NULL) {
		Memory_FreeTagged(YARD_CRAFT_SCORE_TABLES_TAG, table->craftTables);
		table->craftTables = NULL;
	}

	table->count = 0;
	Memory_FreeTagged(YARD_HIGH_SCORE_TABLE_TAG, table);
	return 0;
}

// FUNCTION: XWA 0x519690
void Yard_UpdateSmeltingRoomTurrets(int deltaTicks) {
	int result;
	int elapsedStepTicks;
	int stepCount;
	int smeltingObjIdx;
	CraftData* smeltingCraft;

	smeltingObjIdx = g_yardContext.smeltingRoomObjIdx;
	smeltingCraft = g_objectTable[smeltingObjIdx].mobj->pCraft;
	result = deltaTicks + g_yardContext.smeltingRoomTickAccumulator;
	elapsedStepTicks = 0;
	g_yardContext.smeltingRoomTickAccumulator += deltaTicks;
	if (result >= 40) {
		do {
			result -= 40;
			elapsedStepTicks += 40;
		} while (result >= 40);
		g_yardContext.smeltingRoomTickAccumulator = result;
	}

	if (elapsedStepTicks <= 0) {
		return;
	}

	stepCount = elapsedStepTicks / 40;
	result = g_yardContext.smeltingRoomMode;
	switch (result) {
		case 0:
			return;

		case 1: {
			uint16_t mesh;

			(void)ModelMesh_GetObjectTypeMeshCount(g_objectTable[smeltingObjIdx].objectType);
			for (mesh = 3; mesh <= 8; ++mesh) {
				smeltingCraft->meshRotation[mesh] =
					(uint8_t)(smeltingCraft->meshRotation[mesh] + stepCount * (10 - mesh));
			}
			break;
		}

		case 2: {
			uint16_t mesh;

			(void)ModelMesh_GetObjectTypeMeshCount(g_objectTable[smeltingObjIdx].objectType);
			for (mesh = 0; mesh < 6; ++mesh) {
				smeltingCraft->meshRotation[mesh + 3] =
					(uint8_t)(smeltingCraft->meshRotation[mesh + 3] + stepCount);
			}
			break;
		}
	}

	{
		int shotCount;
		int worldX;
		int worldY;
		int worldZ;

		shotCount = 2;
		do {
			CraftData* turretCraft;
			int projectileObjIdx;

			result = ++g_yardContext.smeltingRoomTurretMeshIdx;
			if (result > 8) {
				result = 3;
				g_yardContext.smeltingRoomTurretMeshIdx = result;
			}

			turretCraft = g_objectTable[smeltingObjIdx].mobj->pCraft;
			if (result <= 5) {
				if (turretCraft->componentHp[2] == 0) {
					continue;
				}
			} else if (turretCraft->componentHp[10] == 0) {
				continue;
			}

			projectileObjIdx = (uint16_t)Object_AllocSlotForGenus(GENUS_NpcProjectile);
			if (projectileObjIdx != 0xffff) {
				ModelIndex smeltingModelIdx;
				int hardpointSlot;
				WarheadGuidanceState* guidance;
				int targetObjIdx;
				int playerIdx;
				int playerObjIdx;

				hardpointSlot = g_yardContext.smeltingRoomTurretMeshIdx - 3;
				smeltingModelIdx = GetModelIndexFromType(OBJ_SmeltingRoom);

				ModelMesh_ApplyAnimatedMeshRotationToPoint(
					smeltingCraft->meshRotation[g_yardContext.smeltingRoomTurretMeshIdx] << 8,
					OBJ_SmeltingRoom, (unsigned int)g_yardContext.smeltingRoomTurretMeshIdx,
					g_modelDefs[smeltingModelIdx].weaponHardpoints[hardpointSlot].x,
					g_modelDefs[smeltingModelIdx].weaponHardpoints[hardpointSlot].y,
					g_modelDefs[smeltingModelIdx].weaponHardpoints[hardpointSlot].z);
				worldZ = g_rotatedZ;
				worldX = g_rotatedX;
				pai_RotateLocalVectorToWorldScratch(&g_objectTable[smeltingObjIdx], (int16_t)worldX,
													(int16_t)worldZ, (int16_t)g_rotatedY);
				worldX = g_rotatedX + g_objectTable[smeltingObjIdx].world_x;
				worldY = g_rotatedY + g_objectTable[smeltingObjIdx].world_y;
				worldZ = g_rotatedZ + g_objectTable[smeltingObjIdx].world_z;

				g_objectTable[projectileObjIdx].mobj->state = 1;
				g_objectTable[projectileObjIdx].genusId = GENUS_NpcProjectile;
				g_objectTable[projectileObjIdx].mobj->iff = g_objectTable[smeltingObjIdx].mobj->iff;
				g_objectTable[projectileObjIdx].objectType = OBJ_LaserRebelTurbo_301;
				g_objectTable[projectileObjIdx].mobj->framesAlive = 1;
				g_objectTable[projectileObjIdx].mobj->sourceObjIdx = (uint16_t)smeltingObjIdx;
				g_objectTable[projectileObjIdx].mobj->sourceObjectType =
					g_objectTable[smeltingObjIdx].objectType;
				g_objectTable[projectileObjIdx].yaw = g_objectTable[smeltingObjIdx].yaw;
				g_objectTable[projectileObjIdx].pitch =
					(uint16_t)(g_objectTable[smeltingObjIdx].pitch + 0x4000u);

				targetObjIdx = 0xffff;
				for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
					PlayerData* player;

					player = &g_players[playerIdx];
					if (!player->connectedFlag) {
						continue;
					}

					playerObjIdx = player->objectIndex;
					if (playerObjIdx == 0xffff) {
						continue;
					}

					g_approxDist = collide_roughdistance3d(g_objectTable[playerObjIdx].world_x - worldX,
														   g_objectTable[playerObjIdx].world_y - worldY,
														   g_objectTable[playerObjIdx].world_z - worldZ);
					if ((unsigned int)g_approxDist < 0x1c00u) {
						int16_t pitchDelta;

						trig2_ctop(g_objectTable[playerObjIdx].world_x - worldX,
								   g_objectTable[playerObjIdx].world_y - worldY,
								   g_objectTable[playerObjIdx].world_z - worldZ);
						pitchDelta = (int16_t)(targetPitch + 0x8000u);
						if (abs(pitchDelta) < 0x1100) {
							targetObjIdx = playerObjIdx;
						}
						break;
					}
				}

				if (targetObjIdx != 0xffff) {
					MobileObject* targetMobj;
					uint16_t speed;

					targetMobj = g_objectTable[targetObjIdx].mobj;
					speed = targetMobj->speed;
					if (speed != 0) {
						int timeToImpact;
						int motionScale;
						int leadX;
						int leadY;
						int leadZ;

						timeToImpact = (int)((double)(unsigned int)g_approxDist /
											 (g_yardTurretLeadTimeScale / g_simStepScale));
						motionScale = ((uint16_t)g_elapsedTicks * ((4660 * (int)speed + 128) >> 8)) / 236;
						motionScale = (uint16_t)motionScale;

#ifdef XWA_MODERN
						leadX = (int)((uint32_t)g_objectTable[playerObjIdx].world_x +
									  (uint32_t)timeToImpact *
										  (uint32_t)Xwa_Q15Mul(targetMobj->moveX, motionScale));
						leadY = (int)((uint32_t)g_objectTable[playerObjIdx].world_y +
									  (uint32_t)timeToImpact *
										  (uint32_t)Xwa_Q15MulReuseFirstSlot(targetMobj->moveY, motionScale));
						leadZ = (int)((uint32_t)g_objectTable[playerObjIdx].world_z +
									  (uint32_t)timeToImpact *
										  (uint32_t)Xwa_Q15MulReuseFirstSlot(targetMobj->moveZ, motionScale));
#else
						leadX = g_objectTable[playerObjIdx].world_x +
								timeToImpact * Xwa_Q15Mul(targetMobj->moveX, motionScale);
						leadY = g_objectTable[playerObjIdx].world_y +
								timeToImpact * Xwa_Q15MulReuseFirstSlot(targetMobj->moveY, motionScale);
						leadZ = g_objectTable[playerObjIdx].world_z +
								timeToImpact * Xwa_Q15MulReuseFirstSlot(targetMobj->moveZ, motionScale);
#endif
						trig2_ctop(leadX - worldX, leadY - worldY, leadZ - worldZ);
						g_objectTable[projectileObjIdx].yaw = trig2_xyangle;
					} else {
						g_objectTable[projectileObjIdx].yaw = trig2_xyangle;
					}
					g_objectTable[projectileObjIdx].pitch = targetPitch;
				}

				g_objectTable[projectileObjIdx].roll = 0;
				g_objectTable[projectileObjIdx].angleD = 0;
				g_objectTable[projectileObjIdx].mobj->orientMatrixDirty = 1;
				g_objectTable[projectileObjIdx].mobj->moveVectorDirty = 1;
				g_objectTable[projectileObjIdx].mobj->speed = 400;
				g_objectTable[projectileObjIdx].mobj->damageAmount =
					g_projectileDamageByType[OBJ_LaserRebelTurbo_301 - OBJ_LaserRebel];
				g_objectTable[projectileObjIdx].mobj->lifetimeTimer = 236;
				FVIEW_calcrotatemove(g_objectTable[projectileObjIdx].pitch,
									 g_objectTable[projectileObjIdx].yaw, &g_objectTable[projectileObjIdx]);
				g_objectTable[projectileObjIdx].mobj->prevWorldX = worldX;
				g_objectTable[projectileObjIdx].mobj->prevWorldY = worldY;
				g_objectTable[projectileObjIdx].mobj->prevWorldZ = worldZ;
				g_objectTable[projectileObjIdx].world_x = worldX;
				g_objectTable[projectileObjIdx].world_y = worldY;
				g_objectTable[projectileObjIdx].world_z = worldZ;

				guidance = g_objectTable[projectileObjIdx].mobj->pWarheadGuidance;
				guidance->homingTier = 0;
				if (targetObjIdx != 0xffff) {
					guidance->targetObjIdx = (uint16_t)targetObjIdx;
				} else {
					guidance->targetObjIdx = 0xffffu;
				}
				guidance->targetSignature = 0;
				guidance->minSpeed = g_objectTable[projectileObjIdx].mobj->speed;
				guidance->sourcePlayerIdx = -1;

				smeltingCraft->warheadData[hardpointSlot].lastFireMeshIdx =
					(uint8_t)g_yardContext.smeltingRoomTurretMeshIdx;
				smeltingCraft->warheadData[hardpointSlot].lastFireHardpointIdx =
					g_modelDefs[smeltingModelIdx].weaponHardpoints[hardpointSlot].alternateMeshHardpointIdx;
				fsfx_triggerweaponsfx((unsigned int)projectileObjIdx, (unsigned int)g_localPlayer);
			}
		} while (--shotCount != 0);
	}
}

// FUNCTION: XWA 0x51A670
void Yard_UpdateAccelRingLaunchers(int deltaTicks) {
	int checkpointIdx;

	for (checkpointIdx = 0; checkpointIdx < 29; ++checkpointIdx) {
		int launcherObjIdx;
		ObjectTypeId objectType;
		int playerIdx;

		launcherObjIdx = g_yardContext.courseSide2Checkpoints[checkpointIdx].objectIdx;
		objectType = g_objectTable[launcherObjIdx].objectType;
		switch (objectType) {
			case OBJ_AccelRing:
				continue;

			case OBJ_AccelRing2: {
				CraftData* craft;

				craft = g_objectTable[launcherObjIdx].mobj->pCraft;
				g_curCraft = craft;
				if (craft->componentHp[4] == 0) {
					if (g_flightPlayerCount > 1 && craft->aiController.aiPlanState == 0) {
						craft->componentHp[4] = 2;
						g_curCraft->componentState[4] = 0;
						for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
							g_curCraft->damageFromPlayer[playerIdx] = 0;
						}
					}
					continue;
				}
				break;
			}

			case OBJ_AccelRing3: {
				CraftData* craft;

				craft = g_objectTable[launcherObjIdx].mobj->pCraft;
				g_curCraft = craft;
				if (craft->componentHp[4] == 0) {
					if (g_flightPlayerCount > 1 && craft->aiController.aiPlanState == 0) {
						craft->componentHp[4] = 2;
						g_curCraft->componentState[4] = 0;
						for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
							g_curCraft->damageFromPlayer[playerIdx] = 0;
						}
					}
					continue;
				}
				break;
			}

			default:
				break;
		}

		g_yardContext.courseSide2Checkpoints[checkpointIdx].fireCooldownTicks -= deltaTicks;
		if (g_yardContext.courseSide2Checkpoints[checkpointIdx].fireCooldownTicks > 0) {
			continue;
		}

		g_yardContext.courseSide2Checkpoints[checkpointIdx].fireCooldownTicks = 0;
		g_yardContext.courseSide2Checkpoints[checkpointIdx].targetObjIdx = 0xffff;
		g_yardContext.courseSide2Checkpoints[checkpointIdx].targetDistance = 0;
		for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
			PlayerData* player;
			int playerObjIdx;
			unsigned int distance;

			player = &g_players[playerIdx];
			if (!player->connectedFlag) {
				continue;
			}
			playerObjIdx = player->objectIndex;
			if (playerObjIdx == 0xffff) {
				continue;
			}

			distance = collide_roughdistance3d(
				g_yardContext.courseSide2Checkpoints[checkpointIdx + 1].checkpointWorldX -
					g_objectTable[playerObjIdx].world_x,
				g_yardContext.courseSide2Checkpoints[checkpointIdx + 1].checkpointWorldY -
					g_objectTable[playerObjIdx].world_y,
				g_yardContext.courseSide2Checkpoints[checkpointIdx + 1].checkpointWorldZ -
					g_objectTable[playerObjIdx].world_z);
			g_approxDist = (int)distance;
			if (distance < 0x1f40u &&
				(g_yardContext.courseSide2Checkpoints[checkpointIdx].targetObjIdx == 0xffff ||
				 (unsigned int)g_yardContext.courseSide2Checkpoints[checkpointIdx].targetDistance >=
					 distance)) {
				g_yardContext.courseSide2Checkpoints[checkpointIdx].targetObjIdx = playerObjIdx;
				g_yardContext.courseSide2Checkpoints[checkpointIdx].targetDistance = (int)distance;
			}
		}

		if (g_yardContext.courseSide2Checkpoints[checkpointIdx].targetObjIdx != 0xffff) {
			g_curCraft = g_objectTable[launcherObjIdx].mobj->pCraft;
			laser_firewarheadlauncher(
				(uint16_t)launcherObjIdx, g_yardContext.courseSide2Checkpoints[checkpointIdx].launcherIdx,
				(uint16_t)g_yardContext.courseSide2Checkpoints[checkpointIdx].targetObjIdx);
			++g_yardContext.courseSide2Checkpoints[checkpointIdx].launcherIdx;
			if (g_yardContext.courseSide2Checkpoints[checkpointIdx].launcherIdx >= 3u) {
				g_yardContext.courseSide2Checkpoints[checkpointIdx].launcherIdx = 0;
			}
			g_yardContext.courseSide2Checkpoints[checkpointIdx].fireCooldownTicks = 100;
		}

		if (g_objectTable[launcherObjIdx].objectType == OBJ_AccelRing3) {
			for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
				PlayerData* player;
				int playerObjIdx;
				unsigned int distance;

				player = &g_players[playerIdx];
				if (!player->connectedFlag) {
					continue;
				}
				playerObjIdx = player->objectIndex;
				if (playerObjIdx == 0xffff) {
					continue;
				}

				distance = collide_roughdistance3d(
					g_yardContext.courseSide2Checkpoints[checkpointIdx].checkpointWorldX -
						g_objectTable[playerObjIdx].world_x,
					g_yardContext.courseSide2Checkpoints[checkpointIdx].checkpointWorldY -
						g_objectTable[playerObjIdx].world_y,
					g_yardContext.courseSide2Checkpoints[checkpointIdx].checkpointWorldZ -
						g_objectTable[playerObjIdx].world_z);
				g_approxDist = (int)distance;
				if (distance < 0x1f40u &&
					(g_yardContext.courseSide2Checkpoints[checkpointIdx].targetObjIdx == 0xffff ||
					 (unsigned int)g_yardContext.courseSide2Checkpoints[checkpointIdx].targetDistance >=
						 distance)) {
					g_yardContext.courseSide2Checkpoints[checkpointIdx].targetObjIdx = playerObjIdx;
					g_yardContext.courseSide2Checkpoints[checkpointIdx].targetDistance = (int)distance;
				}
			}

			if (g_yardContext.courseSide2Checkpoints[checkpointIdx].targetObjIdx != 0xffff) {
				g_curCraft = g_objectTable[launcherObjIdx].mobj->pCraft;
				laser_firewarheadlauncher(
					(uint16_t)launcherObjIdx,
					g_yardContext.courseSide2Checkpoints[checkpointIdx].secondaryLauncherIdx,
					(uint16_t)g_yardContext.courseSide2Checkpoints[checkpointIdx].targetObjIdx);
				++g_yardContext.courseSide2Checkpoints[checkpointIdx].secondaryLauncherIdx;
				if (g_yardContext.courseSide2Checkpoints[checkpointIdx].secondaryLauncherIdx >= 5u) {
					g_yardContext.courseSide2Checkpoints[checkpointIdx].secondaryLauncherIdx = 3;
				}
				g_yardContext.courseSide2Checkpoints[checkpointIdx].fireCooldownTicks = 100;
			}
		}
	}
}

// FUNCTION: XWA 0x51A4B0
void Yard_UpdateSecondaryAccelRingLaunchers(int deltaTicks) {
	int checkpointIdx;

	for (checkpointIdx = 0; checkpointIdx < 29; ++checkpointIdx) {
		int launcherObjIdx;
		int playerIdx;

		launcherObjIdx = g_yardContext.courseSide1Checkpoints[checkpointIdx].objectIdx;
		if (g_objectTable[launcherObjIdx].objectType == OBJ_AccelRing) {
			continue;
		}

		g_curCraft = g_objectTable[launcherObjIdx].mobj->pCraft;
		if (g_curCraft->componentHp[4] == 0) {
			if (g_flightPlayerCount > 1 && g_curCraft->aiController.aiPlanState == 0) {
				g_curCraft->componentHp[4] = 2;
				g_curCraft->componentState[4] = 0;
				for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
					g_curCraft->damageFromPlayer[playerIdx] = 0;
				}
			}
			continue;
		}

		g_yardContext.courseSide1Checkpoints[checkpointIdx].fireCooldownTicks -= deltaTicks;
		if (g_yardContext.courseSide1Checkpoints[checkpointIdx].fireCooldownTicks > 0) {
			continue;
		}

		g_yardContext.courseSide1Checkpoints[checkpointIdx].fireCooldownTicks = 0;
		g_yardContext.courseSide1Checkpoints[checkpointIdx].targetObjIdx = 0xffff;
		g_yardContext.courseSide1Checkpoints[checkpointIdx].targetDistance = 0;
		for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
			PlayerData* player;
			int playerObjIdx;
			ObjectRecord* playerObj;
			unsigned int distance;

			player = &g_players[playerIdx];
			if (!player->connectedFlag) {
				continue;
			}
			playerObjIdx = player->objectIndex;
			if (playerObjIdx == 0xffff) {
				continue;
			}

			playerObj = &g_objectTable[playerObjIdx];
			if (g_objectTable[playerObjIdx].mobj->framesAlive <= 3u) {
				continue;
			}

			distance = collide_roughdistance3d(
				g_yardContext.courseSide1Checkpoints[checkpointIdx + 1].checkpointWorldX - playerObj->world_x,
				g_yardContext.courseSide1Checkpoints[checkpointIdx + 1].checkpointWorldY - playerObj->world_y,
				g_yardContext.courseSide1Checkpoints[checkpointIdx + 1].checkpointWorldZ -
					playerObj->world_z);
			g_approxDist = (int)distance;
			if (distance < 0x1f40u &&
				(g_yardContext.courseSide1Checkpoints[checkpointIdx].targetObjIdx == 0xffff ||
				 (unsigned int)g_yardContext.courseSide1Checkpoints[checkpointIdx].targetDistance >=
					 distance)) {
				g_yardContext.courseSide1Checkpoints[checkpointIdx].targetObjIdx = playerObjIdx;
				g_yardContext.courseSide1Checkpoints[checkpointIdx].targetDistance = (int)distance;
			}
		}

		if (g_yardContext.courseSide1Checkpoints[checkpointIdx].targetObjIdx != 0xffff) {
			laser_firewarheadlauncher(
				launcherObjIdx, g_yardContext.courseSide1Checkpoints[checkpointIdx].launcherIdx,
				(uint16_t)g_yardContext.courseSide1Checkpoints[checkpointIdx].targetObjIdx);
			++g_yardContext.courseSide1Checkpoints[checkpointIdx].launcherIdx;
			if (g_yardContext.courseSide1Checkpoints[checkpointIdx].launcherIdx >=
				g_curCraft->laserSlotCount) {
				g_yardContext.courseSide1Checkpoints[checkpointIdx].launcherIdx = 0;
			}
			g_yardContext.courseSide1Checkpoints[checkpointIdx].fireCooldownTicks = 100;
		}
	}
}

// FUNCTION: XWA 0x5133E0
int Yard_BuildAdvancedChallengeCourse(void) {
	uint16_t checkpointIdx;
	int i;
	int rubbleIdx;

	rubbleIdx = 0;
	g_yardChuteMouthObjIdx = Yard_SpawnObjectAtWorldPos(OBJ_ChuteMouth, 0, 0, 0, 0, 0x4000);
	g_yardBuildParentObjIdx = g_yardChuteMouthObjIdx;
	for (i = 0; i < 4; ++i) {
		g_yardBuildParentObjIdx =
			Yard_SpawnChildAtMount(OBJ_ChuteTunnel, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	}
	g_yardChuteTunnelEndObjIdx = g_yardBuildParentObjIdx;

	g_yardAccelRingCullAnchorObjIdx =
		Yard_SpawnChildAtMount(OBJ_Asteroid01, g_yardChuteMouthObjIdx, 0, 0, 0, 1);
	g_yardSalvageRoomObjIdx = Yard_SpawnChildAtMount(OBJ_SalvageRoom, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = g_yardSalvageRoomObjIdx;
	g_yardContext.compactorObjIdx =
		Yard_SpawnChildAtMount(OBJ_Compactor, g_yardSalvageRoomObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = g_yardContext.compactorObjIdx;
	g_yardContext.compactorCycleState = 1;
	g_yardContext.compactorPauseTimer = 0;
	g_yardContext.compactorTickAccumulator = 0;
	if (g_yardChallengeMode > 3u) {
		Yard_SpawnJunkBlockAtChamberDock();
	}

	checkpointIdx = 0;
	while (checkpointIdx < 30) {
		YardCourseCheckpointState* checkpoint;
		ObjectTypeId ringType;
		int16_t yaw;
		int16_t pitch;

		if (checkpointIdx < 2) {
			yaw = -1280;
			pitch = yaw;
		} else if (checkpointIdx < 4) {
			yaw = 0x2000;
			pitch = 1024;
		} else if (checkpointIdx < 6) {
			yaw = 256;
			pitch = yaw;
		} else if (checkpointIdx < 8) {
			yaw = -512;
			pitch = -1024;
		} else if (checkpointIdx < 10) {
			yaw = -256;
			pitch = yaw;
		} else if (checkpointIdx < 14) {
			yaw = -4096;
			pitch = 1280;
		} else if (checkpointIdx < 18) {
			yaw = 4096;
			pitch = -1280;
		} else if (checkpointIdx < 20) {
			yaw = -4608;
			pitch = 4096;
		} else if (checkpointIdx < 24) {
			yaw = 2048;
			pitch = yaw;
		} else if (checkpointIdx < 26) {
			yaw = 4608;
			pitch = -4096;
		} else {
			yaw = -1024;
			pitch = yaw;
		}
		ringType = (checkpointIdx % 4) != 0 ? OBJ_AccelRing : OBJ_AccelRing2;
		checkpoint = &g_yardContext.courseSide1Checkpoints[checkpointIdx];
		checkpoint->objectIdx = Yard_SpawnChildAtMount(ringType, g_yardBuildParentObjIdx, 1, yaw, pitch, 1);
		g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = (uint8_t)checkpointIdx;
		if (checkpointIdx == 0) {
			checkpoint->prevObjectIdx = 0xffff;
		} else {
			checkpoint->prevObjectIdx = g_yardBuildParentObjIdx;
			g_yardContext.courseSide1Checkpoints[checkpointIdx - 1].nextObjectIdx = checkpoint->objectIdx;
		}
		g_yardBuildParentObjIdx = checkpoint->objectIdx;
		{
			ModelIndex modelIndex;
			ObjectRecord* object;

			modelIndex = GetModelIndexFromType(ringType);
			pai_RotateLocalVectorToWorldScratch(
				&g_objectTable[g_yardBuildParentObjIdx], g_modelDefs[modelIndex].childMountPoints[3],
				g_modelDefs[modelIndex].childMountPoints[4], g_modelDefs[modelIndex].childMountPoints[5]);

			object = &g_objectTable[g_yardBuildParentObjIdx];
			checkpoint->checkpointWorldX = g_rotatedX + object->world_x;
			checkpoint->checkpointWorldY = g_rotatedY + object->world_y;
			checkpoint->checkpointWorldZ = g_rotatedZ + object->world_z;
			checkpoint->fireCooldownTicks = 0;
			checkpoint->launcherIdx = 0;
		}
		checkpoint->secondaryLauncherIdx = 0;
		++checkpointIdx;
	}
	g_yardCourseSide1FinalObjIdx = g_yardBuildParentObjIdx;
	g_yardContext.courseSide1Checkpoints[29].nextObjectIdx = 0xffff;

	g_yardContext.smeltingRoomObjIdx =
		Yard_SpawnChildAtMount(OBJ_SmeltingRoom, g_yardBuildParentObjIdx, 1, 0, 0x4000, 0);
	g_yardBuildParentObjIdx = g_yardContext.smeltingRoomObjIdx;
	g_yardContext.smeltingRoomMode = 1;
	g_yardContext.smeltingRoomTickAccumulator = 0;
	g_yardContext.smeltingRoomTurretMeshIdx = 3;
	g_yardSmeltingRoomAsteroidObjIdx =
		Yard_SpawnChildAtMount(OBJ_Asteroid02, g_yardContext.smeltingRoomObjIdx, 3, 0, 0x4000, 0);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardAdvancedCourseTubeFirstObjIdx = g_yardBuildParentObjIdx;
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeLH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeRH, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeDown, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeUP, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = Yard_SpawnChildAtMount(OBJ_SRTubeNOBend, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardAdvancedCourseTubeLastObjIdx = g_yardBuildParentObjIdx;

	g_yardCentrifugeObjIdx = Yard_SpawnChildAtMount(OBJ_Centrifuge, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = g_yardCentrifugeObjIdx;
	g_yardContext.centrifugeMechanismTickRemainder = 0;
	g_yardContext.centrifugeMechanisms[0].state = 0;
	g_yardContext.centrifugeMechanisms[0].cycleParity = 0;
	g_yardContext.centrifugeMechanisms[0].angularAccel = 0.0f;
	g_yardContext.centrifugeMechanisms[0].angularVelocity = 0.0f;
	g_yardContext.centrifugeMechanisms[0].delayTicks = 5;
	g_yardContext.centrifugeMechanisms[0].meshRotationAccum = 0.0f;
	g_yardContext.centrifugeMechanisms[1].state = 3;
	g_yardContext.centrifugeMechanisms[1].cycleParity = 0;
	g_yardContext.centrifugeMechanisms[1].angularAccel = 0.0f;
	g_yardContext.centrifugeMechanisms[1].angularVelocity = 4.0f;
	g_yardContext.centrifugeMechanisms[1].delayTicks = 2;
	g_yardContext.centrifugeMechanisms[1].meshRotationAccum = 8192.0f;
	g_objectTable[g_yardCentrifugeObjIdx].mobj->pCraft->meshRotation[4] = 32;
	g_yardContext.centrifugeMechanisms[2].state = 2;
	g_yardContext.centrifugeMechanisms[2].cycleParity = 0;
	g_yardContext.centrifugeMechanisms[2].angularAccel = 0.0f;
	g_yardContext.centrifugeMechanisms[2].angularVelocity = 0.0f;
	g_yardContext.centrifugeMechanisms[2].delayTicks = 0;
	g_yardContext.centrifugeMechanisms[2].meshRotationAccum = 0.0f;

	checkpointIdx = 0;
	while (checkpointIdx < 30) {
		YardCourseCheckpointState* checkpoint;
		ObjectTypeId ringType;
		int16_t yaw;
		int16_t pitch;

		if (checkpointIdx < 2) {
			yaw = 256;
			pitch = yaw;
		} else if (checkpointIdx < 6) {
			yaw = -4096;
			pitch = 1280;
		} else if (checkpointIdx < 10) {
			yaw = 4096;
			pitch = -1280;
		} else if (checkpointIdx < 14) {
			yaw = 6144;
			pitch = 5120;
		} else if (checkpointIdx < 18) {
			yaw = -6144;
			pitch = -5120;
		} else if (checkpointIdx < 22) {
			yaw = -6656;
			pitch = 4096;
		} else if (checkpointIdx < 26) {
			yaw = 6656;
			pitch = -4096;
		} else {
			yaw = -1280;
			pitch = yaw;
		}
		ringType = (checkpointIdx % 4) != 0 ? OBJ_AccelRing : OBJ_AccelRing3;
		checkpoint = &g_yardContext.courseSide2Checkpoints[checkpointIdx];
		checkpoint->objectIdx = Yard_SpawnChildAtMount(ringType, g_yardBuildParentObjIdx, 1, yaw, pitch, 1);
		if (checkpointIdx == 0) {
			checkpoint->prevObjectIdx = 0xffff;
			g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 0;
		} else {
			checkpoint->prevObjectIdx = g_yardBuildParentObjIdx;
			g_yardContext.courseSide2Checkpoints[checkpointIdx - 1].nextObjectIdx = checkpoint->objectIdx;
			g_objectTable[checkpoint->objectIdx].mobj->nodeSwitchIndex = 1;
		}
		g_yardBuildParentObjIdx = checkpoint->objectIdx;
		{
			ModelIndex modelIndex;
			ObjectRecord* object;

			modelIndex = GetModelIndexFromType(ringType);
			pai_RotateLocalVectorToWorldScratch(
				&g_objectTable[g_yardBuildParentObjIdx], g_modelDefs[modelIndex].childMountPoints[3],
				g_modelDefs[modelIndex].childMountPoints[4], g_modelDefs[modelIndex].childMountPoints[5]);

			object = &g_objectTable[g_yardBuildParentObjIdx];
			checkpoint->checkpointWorldX = g_rotatedX + object->world_x;
			checkpoint->checkpointWorldY = g_rotatedY + object->world_y;
			checkpoint->checkpointWorldZ = g_rotatedZ + object->world_z;
			checkpoint->fireCooldownTicks = 0;
			checkpoint->launcherIdx = 0;
		}
		checkpoint->secondaryLauncherIdx = 3;
		++checkpointIdx;
	}
	g_yardCourseSide2FinalObjIdx = g_yardBuildParentObjIdx;
	g_yardContext.courseSide2Checkpoints[29].nextObjectIdx = 0xffff;

	g_yardContainerGrandeObjIdx =
		Yard_SpawnChildAtMount(OBJ_ContainerGrandePG, g_yardBuildParentObjIdx, 1, 0, 0, 1);
	g_yardBuildParentObjIdx = g_yardContainerGrandeObjIdx;
	if (g_flightPlayerCount >= 6) {
		g_yardRubbleChunkSpawnLimit = 5;
	} else {
		g_yardRubbleChunkSpawnLimit = g_flightPlayerCount < 4 ? 20 : 10;
	}
	g_yardContext.rubbleSpawnTickAccumulator = 0;

	if (g_yardChallengeMode > 3u) {
		int halfSpawnExtent;
		int heightExtent;

		halfSpawnExtent =
			ModelBounds_GetSizeX((uint16_t)g_objectTable[g_yardChuteTunnelEndObjIdx].objectType) / 3;
		heightExtent = ModelBounds_GetSizeY((uint16_t)g_objectTable[g_yardChuteTunnelEndObjIdx].objectType);
		if (g_yardRubbleChunkSpawnLimit > 0) {
			for (; rubbleIdx < g_yardRubbleChunkSpawnLimit; ++rubbleIdx) {
				int worldX;
				int worldY;
				int worldZ;

				worldX = g_objectTable[g_yardChuteMouthObjIdx].world_x +
						 (uint16_t)GameRand() % (2 * halfSpawnExtent) - halfSpawnExtent;
				worldY = g_objectTable[g_yardChuteMouthObjIdx].world_y -
						 heightExtent * (((uint16_t)GameRand() >> 4) % 5);
				worldY -= (uint16_t)GameRand() % heightExtent;
				worldZ = g_objectTable[g_yardChuteMouthObjIdx].world_z +
						 (uint16_t)GameRand() % (2 * halfSpawnExtent) - halfSpawnExtent;
				g_yardBuildParentObjIdx = Yard_SpawnRubbleChunkAtWorldPos(rubbleIdx, worldX, worldY, worldZ);
				if (g_yardBuildParentObjIdx == 0xffff) {
					g_yardBuildParentObjIdx = g_yardContext.rubbleChunkStates[rubbleIdx].objectIdx;
					return rubbleIdx;
				}
				++g_yardContext.rubbleChunkStateCount;
			}
			return g_yardRubbleChunkSpawnLimit;
		}
	}

	return g_yardChallengeMode;
}

// FUNCTION: XWA 0x51B420
void Yard_TargetCurrentObjective(unsigned int playerIdx) {
	int useCheckpointTarget;
	int playerObjIdx;

	useCheckpointTarget = 1;
	playerObjIdx = g_players[playerIdx].objectIndex;
	if (playerObjIdx != 0xffff && g_yardChallengeMode >= 6u) {
		int carriedObjectIdx;

		carriedObjectIdx = g_objectTable[playerObjIdx].mobj->pCraft->carriedObjectIndex;
		if (carriedObjectIdx != 0xffff && g_objectTable[carriedObjectIdx].objectType == OBJ_R2D2) {
			if (g_players[playerIdx].currentTargetObjectIdx == 0xffffu ||
				g_objectTable[g_players[playerIdx].currentTargetObjectIdx].objectType != OBJ_Shuttle) {
				uint16_t objIdx;

				objIdx = 0;
				while (objIdx < g_objectSlotsPerRegion) {
					if (g_objectTable[objIdx].objectType == OBJ_Shuttle) {
						Player_SetTarget(objIdx, playerIdx);
						useCheckpointTarget = 0;
					}
					++objIdx;
				}
			}
		} else if (g_players[playerIdx].currentTargetObjectIdx == 0xffffu ||
				   g_objectTable[g_players[playerIdx].currentTargetObjectIdx].objectType != OBJ_R2D2) {
			uint16_t objIdx;

			objIdx = 0;
			while (objIdx < g_objectSlotsPerRegion) {
				if (g_objectTable[objIdx].objectType == OBJ_R2D2) {
					Player_SetTarget(objIdx, playerIdx);
					useCheckpointTarget = 0;
				}
				++objIdx;
			}
		}
	}

	if (useCheckpointTarget) {
		int nextCheckpointIdx;

		nextCheckpointIdx = g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx;
		if (nextCheckpointIdx >= 0) {
			int nextCourseSide;

			nextCourseSide = g_yardContext.playerChallengeStates[playerIdx].nextCourseSide;
			if (nextCourseSide == 1) {
				Player_SetTarget(g_yardContext.courseSide1Checkpoints[nextCheckpointIdx].objectIdx,
								 playerIdx);
			} else {
				Player_SetTarget(g_yardContext.courseSide2Checkpoints[nextCheckpointIdx].objectIdx,
								 playerIdx);
			}
		}
	}
}

// FUNCTION: XWA 0x51B5C0
void Yard_HandleR2D2CarrierLaserHit(unsigned int projectileObjIdx, unsigned int targetObjIdx) {
	int sourceObjIdx;
	int shooterPlayerIdx;
	int carriedObjectIdx;

	if (projectileObjIdx == 0xffffu || targetObjIdx == 0xffffu) {
		return;
	}

	sourceObjIdx = (uint16_t)g_objectTable[projectileObjIdx].mobj->sourceObjIdx;
	if (sourceObjIdx == 0xffff) {
		return;
	}

	shooterPlayerIdx = g_objectTable[sourceObjIdx].playerOwnerIdx;
	if (g_objectTable[targetObjIdx].playerOwnerIdx == -1 || shooterPlayerIdx == -1) {
		return;
	}

	g_objectTable[targetObjIdx].mobj->speed = (uint16_t)((2 * g_objectTable[targetObjIdx].mobj->speed) / 3);
	carriedObjectIdx = g_objectTable[targetObjIdx].mobj->pCraft->carriedObjectIndex;
	if (carriedObjectIdx != 0xffff && g_objectTable[carriedObjectIdx].objectType == OBJ_R2D2) {
		Player_ReleaseCarriedObject((unsigned int)g_objectTable[targetObjIdx].playerOwnerIdx);
		Player_SetTarget(carriedObjectIdx, (unsigned int)shooterPlayerIdx);
		Player_HandlePickupCommand((unsigned int)shooterPlayerIdx);
		fsfx_PlaySound(124, 0xffffu, g_localPlayer);
	}
}

// FUNCTION: XWA 0x51B6C0
void Yard_PickUpR2D2Objective(unsigned int playerIdx) {
	Player_SetTarget((uint16_t)g_yardR2D2ObjIdx, playerIdx);
	Player_HandlePickupCommand(playerIdx);
}

// FUNCTION: XWA 0x51B2B0
int Yard_SavePlayerRecoveryState(int objectIdx) {
	MobileObject* mobj;
	int playerOwnerIdx;
	int previousWorldX;
	int previousWorldY;
	int previousWorldZ;
	int recoveryWorldX;
	int recoveryWorldY;
	int recoveryWorldZ;
	int recoveryCollisionObjIdx;
	Q16Angle recoveryYaw;
	Q16Angle recoveryPitch;

	mobj = g_objectTable[objectIdx].mobj;
	recoveryCollisionObjIdx = mobj->collisionObjIdx;
	previousWorldX = mobj->prevWorldX;
	previousWorldY = mobj->prevWorldY;
	previousWorldZ = mobj->prevWorldZ;
	recoveryWorldX = previousWorldX;
	recoveryWorldY = previousWorldY;
	recoveryWorldZ = previousWorldZ;
	recoveryYaw = g_objectTable[objectIdx].yaw;
	recoveryPitch = g_objectTable[objectIdx].pitch;

	if (objectIdx == 0xffff) {
		return false;
	}
	if (mobj == NULL) {
		return false;
	}

	playerOwnerIdx = g_objectTable[objectIdx].playerOwnerIdx;

	if (g_yardContext.playerChallengeStates[playerOwnerIdx].courseState == 3 ||
		g_yardContext.playerChallengeStates[playerOwnerIdx].courseState == 7) {
		int currentCheckpointIdx;

		currentCheckpointIdx = g_yardContext.playerChallengeStates[playerOwnerIdx].currentCheckpointIdx;
		if (currentCheckpointIdx >= 0 &&
			g_yardContext.playerChallengeStates[playerOwnerIdx].currentCourseSide ==
				g_yardContext.playerChallengeStates[playerOwnerIdx].nextCourseSide) {
			if (g_yardContext.playerChallengeStates[playerOwnerIdx].currentCourseSide == 1) {
				recoveryWorldX = g_yardContext.courseSide1Checkpoints[currentCheckpointIdx].checkpointWorldX;
				recoveryWorldY = g_yardContext.courseSide1Checkpoints[currentCheckpointIdx].checkpointWorldY;
				recoveryWorldZ = g_yardContext.courseSide1Checkpoints[currentCheckpointIdx].checkpointWorldZ;
			} else {
				recoveryWorldX = g_yardContext.courseSide2Checkpoints[currentCheckpointIdx].checkpointWorldX;
				recoveryWorldY = g_yardContext.courseSide2Checkpoints[currentCheckpointIdx].checkpointWorldY;
				recoveryWorldZ = g_yardContext.courseSide2Checkpoints[currentCheckpointIdx].checkpointWorldZ;
			}
		}
	}

	g_approxDist = collide_roughdistance3d(recoveryWorldX - previousWorldX, recoveryWorldY - previousWorldY,
										   recoveryWorldZ - previousWorldZ);
	if ((uint32_t)g_approxDist > 0xc000u) {
		recoveryWorldX = g_objectTable[objectIdx].mobj->prevWorldX;
		recoveryWorldY = g_objectTable[objectIdx].mobj->prevWorldY;
		recoveryWorldZ = g_objectTable[objectIdx].mobj->prevWorldZ;
	}

	g_yardContext.playerChallengeStates[playerOwnerIdx].recoveryCollisionObjIdx = recoveryCollisionObjIdx;
	g_yardContext.playerChallengeStates[playerOwnerIdx].recoveryWorldX = recoveryWorldX;
	g_yardContext.playerChallengeStates[playerOwnerIdx].recoveryWorldY = recoveryWorldY;
	g_yardContext.playerChallengeStates[playerOwnerIdx].recoveryWorldZ = recoveryWorldZ;
	g_yardContext.playerChallengeStates[playerOwnerIdx].recoveryYaw = recoveryYaw;
	g_yardContext.playerChallengeStates[playerOwnerIdx].recoveryPitch = recoveryPitch;
	return true;
}

// FUNCTION: XWA 0x517B50
int Yard_IsObjectTypeVisibleForCurrentCourseState(ObjectTypeId objectType) {
	if (objectType < OBJ_ChuteMouth || objectType > OBJ_MoltenBlock || g_yardChallengeMode < 3) {
		return true;
	}

	switch (g_yardContext.playerChallengeStates[g_localPlayer].courseState) {
		case 0:
			switch (objectType) {
				case OBJ_ChuteMouth:
				case OBJ_ChuteTunnel:
				case OBJ_SalvageRoom:
				case OBJ_Asteroid01:
				case OBJ_Junk01:
				case OBJ_Junk02:
				case OBJ_Junk03:
				case OBJ_Junk04:
				case OBJ_Junk05:
				case OBJ_Junk06:
				case OBJ_Junk07:
				case OBJ_Junk08:
				case OBJ_Junk09:
				case OBJ_Junk10:
					return true;
				default:
					break;
			}
			break;

		case 1:
			switch (objectType) {
				case OBJ_ChuteMouth:
				case OBJ_ChuteTunnel:
				case OBJ_SalvageRoom:
				case OBJ_Compactor:
				case OBJ_Junk01:
				case OBJ_Junk02:
				case OBJ_Junk03:
				case OBJ_Junk04:
				case OBJ_Junk05:
				case OBJ_Junk06:
				case OBJ_Junk07:
				case OBJ_Junk08:
				case OBJ_Junk09:
				case OBJ_Junk10:
				case OBJ_JunkBlock:
					return true;
				default:
					break;
			}
			break;

		case 2:
			switch (objectType) {
				case OBJ_SalvageRoom:
				case OBJ_Compactor:
				case OBJ_AccelRing:
				case OBJ_AccelRing2:
				case OBJ_Asteroid02:
				case OBJ_Junk01:
				case OBJ_Junk02:
				case OBJ_Junk03:
				case OBJ_Junk04:
				case OBJ_Junk05:
				case OBJ_Junk06:
				case OBJ_Junk07:
				case OBJ_Junk08:
				case OBJ_Junk09:
				case OBJ_Junk10:
				case OBJ_JunkBlock:
					return true;
				default:
					break;
			}
			break;

		case 3:
			switch (objectType) {
				case OBJ_Compactor:
				case OBJ_AccelRing:
				case OBJ_AccelRing2:
				case OBJ_SmeltingRoom:
				case OBJ_ContainerGrandePG:
				case OBJ_Asteroid01:
				case OBJ_Asteroid02:
				case OBJ_JunkBlock:
					return true;
				case OBJ_AccelRing3:
					if (g_yardChallengeMode <= 1 || g_gameConfig.yardLod[g_flightPlayerCount > 1] > 10) {
						return true;
					}
					break;
				default:
					break;
			}
			break;

		case 4:
			switch (objectType) {
				case OBJ_AccelRing:
				case OBJ_AccelRing2:
				case OBJ_SmeltingRoom:
				case OBJ_SRTubeNOBend:
				case OBJ_SRTubeUP:
				case OBJ_SRTubeDown:
				case OBJ_SRTubeRH:
				case OBJ_Asteroid01:
				case OBJ_JunkBlock:
					return true;
				default:
					break;
			}
			break;

		case 5:
			if (objectType >= OBJ_SmeltingRoom && objectType <= OBJ_Centrifuge) {
				return true;
			}
			break;

		case 6:
			switch (objectType) {
				case OBJ_AccelRing:
				case OBJ_SRTubeNOBend:
				case OBJ_Centrifuge:
				case OBJ_AccelRing3:
				case OBJ_MoltenBlock:
					return true;
				case OBJ_SRTubeUP:
				case OBJ_SRTubeDown:
				case OBJ_Asteroid02:
					if (g_gameConfig.yardLod[g_flightPlayerCount > 1] > 2) {
						return true;
					}
					break;
				case OBJ_ContainerGrandePG:
					if (g_gameConfig.yardLod[g_flightPlayerCount > 1] > 10) {
						return true;
					}
					break;
				default:
					break;
			}
			break;

		case 7:
			switch (objectType) {
				case OBJ_AccelRing:
				case OBJ_Centrifuge:
				case OBJ_AccelRing3:
				case OBJ_ContainerGrandePG:
				case OBJ_Asteroid02:
				case OBJ_MoltenBlock:
					return true;
				case OBJ_AccelRing2:
				case OBJ_Asteroid01:
					if (g_yardChallengeMode <= 1 || g_gameConfig.yardLod[g_flightPlayerCount > 1] > 10) {
						return true;
					}
					break;
				default:
					break;
			}
			break;

		case 8:
			switch (objectType) {
				case OBJ_AccelRing:
				case OBJ_Centrifuge:
				case OBJ_AccelRing3:
				case OBJ_ContainerGrandePG:
				case OBJ_Asteroid02:
				case OBJ_MoltenBlock:
					return true;
				case OBJ_Asteroid01:
					if (g_gameConfig.yardLod[g_flightPlayerCount > 1] > 10) {
						return true;
					}
					break;
				default:
					break;
			}
			break;

		default:
			break;
	}

	return false;
}

static __inline int Yard_AbsObjectIndexDelta(unsigned int leftObjIdx, unsigned int rightObjIdx) {
#ifdef XWA_MODERN
	return Xwa_Abs32((int)(leftObjIdx - rightObjIdx));
#else
	return abs((int)(leftObjIdx - rightObjIdx));
#endif
}

// FUNCTION: XWA 0x5174F0
int Yard_ShouldRenderChallengeObject(unsigned int objectIdx) {
	uint16_t objectType;
	int playerObjIdx;
	int localPlayer;
	int playerCount;
	unsigned int collisionObjIdx;
	uint16_t collisionObjectType;

	objectType = g_objectTable[objectIdx].objectType;
	if (!Yard_IsObjectTypeVisibleForCurrentCourseState(objectType)) {
		return false;
	}

	localPlayer = g_localPlayer;
	playerObjIdx = g_players[localPlayer].objectIndex;
	switch (objectType) {
		case OBJ_AccelRing:
		case OBJ_AccelRing2:
		case OBJ_AccelRing3:
		case OBJ_Junk01:
		case OBJ_Junk02:
		case OBJ_Junk03:
		case OBJ_Junk04:
		case OBJ_Junk05:
		case OBJ_Junk06:
		case OBJ_Junk07:
		case OBJ_Junk08:
		case OBJ_Junk09:
		case OBJ_Junk10:
		case OBJ_JunkBlock:
		case OBJ_MoltenBlock:
			playerCount = g_flightPlayerCount;
			if (g_gameConfig.yardLod[playerCount > 1] < 0x14u) {
				unsigned int approxDist = collide_roughdistance3d(
					g_objectTable[playerObjIdx].world_x - g_objectTable[objectIdx].world_x,
					g_objectTable[playerObjIdx].world_y - g_objectTable[objectIdx].world_y,
					g_objectTable[playerObjIdx].world_z - g_objectTable[objectIdx].world_z);
				playerCount = g_flightPlayerCount;
				g_approxDist = (int)approxDist;
				if (approxDist > 20480u * (unsigned int)g_gameConfig.yardLod[playerCount > 1] + 0x8000u) {
					return false;
				}
				localPlayer = g_localPlayer;
			}
			break;

		default:
			playerCount = g_flightPlayerCount;
			break;
	}

	collisionObjIdx = g_objectTable[playerObjIdx].mobj->collisionObjIdx;
#ifdef XWA_MODERN
	collisionObjectType = OBJ_None;
#endif
	if (collisionObjIdx != 0xffffu) {
		collisionObjectType = g_objectTable[collisionObjIdx].objectType;
	}

	switch (g_yardContext.playerChallengeStates[localPlayer].courseState) {
		case 0:
			if (objectType == OBJ_SalvageRoom && g_gameConfig.yardLod[playerCount > 1] < 0x0au) {
				return false;
			}
			break;

		case 1: {
			ObjectTypeId collisionType;

			collisionType = (ObjectTypeId)collisionObjectType;
			if (collisionType >= OBJ_ChuteMouth && collisionType <= OBJ_ChuteTunnel &&
				objectType == OBJ_Compactor) {
				return false;
			}
			break;
		}

		case 4:
			switch (objectType) {
				case OBJ_AccelRing:
				case OBJ_AccelRing2: {
					ModelIndex smeltingModel;

					smeltingModel = GetModelIndexFromType(OBJ_SmeltingRoom);
					pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardContext.smeltingRoomObjIdx],
														g_modelDefs[smeltingModel].meshAttachData[5],
														g_modelDefs[smeltingModel].meshAttachData[6],
														g_modelDefs[smeltingModel].meshAttachData[7]);
					g_approxDist = collide_roughdistance3d(
						g_objectTable[playerObjIdx].world_x -
							g_objectTable[g_yardContext.smeltingRoomObjIdx].world_x - g_rotatedX,
						g_objectTable[playerObjIdx].world_y -
							g_objectTable[g_yardContext.smeltingRoomObjIdx].world_y - g_rotatedY,
						g_objectTable[playerObjIdx].world_z -
							g_objectTable[g_yardContext.smeltingRoomObjIdx].world_z - g_rotatedZ);
					if ((unsigned int)g_approxDist > 0x4800u) {
						return false;
					}
					break;
				}

				case OBJ_SRTubeNOBend:
				case OBJ_SRTubeUP:
				case OBJ_SRTubeDown:
				case OBJ_SRTubeLH:
				case OBJ_SRTubeRH: {
					ModelIndex smeltingModel;

					if (objectIdx > (unsigned int)(g_yardAdvancedCourseTubeFirstObjIdx + 4)) {
						return false;
					}
					smeltingModel = GetModelIndexFromType(OBJ_SmeltingRoom);
					pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardContext.smeltingRoomObjIdx],
														g_modelDefs[smeltingModel].childMountPoints[0],
														g_modelDefs[smeltingModel].childMountPoints[1],
														g_modelDefs[smeltingModel].childMountPoints[2]);
					g_approxDist = collide_roughdistance3d(
						g_objectTable[playerObjIdx].world_x -
							g_objectTable[g_yardContext.smeltingRoomObjIdx].world_x - g_rotatedX,
						g_objectTable[playerObjIdx].world_y -
							g_objectTable[g_yardContext.smeltingRoomObjIdx].world_y - g_rotatedY,
						g_objectTable[playerObjIdx].world_z -
							g_objectTable[g_yardContext.smeltingRoomObjIdx].world_z - g_rotatedZ);
					if ((unsigned int)g_approxDist > 0x8000u) {
						return false;
					}
					break;
				}

				default:
					break;
			}
			break;

		case 5:
			switch (objectType) {
				case OBJ_SmeltingRoom:
					if (objectIdx > (unsigned int)(g_yardAdvancedCourseTubeFirstObjIdx + 4)) {
						return false;
					}
					break;

				case OBJ_SRTubeNOBend:
				case OBJ_SRTubeUP:
				case OBJ_SRTubeDown:
				case OBJ_SRTubeLH:
				case OBJ_SRTubeRH:
					if (g_gameConfig.yardLod[playerCount > 1] < 0x0au) {
						if (Yard_AbsObjectIndexDelta(objectIdx, collisionObjIdx) >
							g_gameConfig.yardLod[playerCount > 1] / 3 + 4) {
							return false;
						}
					} else {
						if (Yard_AbsObjectIndexDelta(objectIdx, collisionObjIdx) > 8) {
							return false;
						}
					}
					break;

				case OBJ_Centrifuge:
					if (collisionObjIdx < (unsigned int)(g_yardAdvancedCourseTubeLastObjIdx - 4)) {
						return false;
					}
					break;

				default:
					break;
			}
			break;

		case 6:
			switch (objectType) {
				case OBJ_SRTubeNOBend:
				case OBJ_SRTubeUP:
				case OBJ_SRTubeDown:
				case OBJ_SRTubeLH:
				case OBJ_SRTubeRH:
					if (objectIdx < (unsigned int)(g_yardAdvancedCourseTubeLastObjIdx - 4)) {
						return false;
					}
					break;

				case OBJ_AccelRing:
				case OBJ_AccelRing3:
				case OBJ_Asteroid02: {
					ModelIndex centrifugeModel;

					centrifugeModel = GetModelIndexFromType(OBJ_Centrifuge);
					pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardCentrifugeObjIdx],
														g_modelDefs[centrifugeModel].meshAttachData[8],
														g_modelDefs[centrifugeModel].meshAttachData[9],
														g_modelDefs[centrifugeModel].meshAttachData[10]);
					g_approxDist = collide_roughdistance3d(
						g_objectTable[playerObjIdx].world_x - g_objectTable[g_yardCentrifugeObjIdx].world_x -
							g_rotatedX,
						g_objectTable[playerObjIdx].world_y - g_objectTable[g_yardCentrifugeObjIdx].world_y -
							g_rotatedY,
						g_objectTable[playerObjIdx].world_z - g_objectTable[g_yardCentrifugeObjIdx].world_z -
							g_rotatedZ);
					if (objectType == OBJ_Asteroid02 && (unsigned int)g_approxDist > 0x4800u) {
						return false;
					}
					if ((unsigned int)g_approxDist >
						((unsigned int)g_gameConfig.yardLod[g_flightPlayerCount > 1] << 12)) {
						return false;
					}
					break;
				}

				default:
					break;
			}
			break;

		case 9:
		case 10: {
			ObjectTypeId renderObjectType;

			renderObjectType = (ObjectTypeId)objectType;
			if (renderObjectType < OBJ_AccelRing ||
				(renderObjectType > OBJ_AccelRing2 && renderObjectType != OBJ_AccelRing3)) {
				break;
			}
			g_approxDist = collide_roughdistance3d(
				g_objectTable[playerObjIdx].world_x - g_objectTable[g_yardAccelRingCullAnchorObjIdx].world_x,
				g_objectTable[playerObjIdx].world_y - g_objectTable[g_yardAccelRingCullAnchorObjIdx].world_y,
				g_objectTable[playerObjIdx].world_z - g_objectTable[g_yardAccelRingCullAnchorObjIdx].world_z);
			if ((unsigned int)g_approxDist < 0xb400u) {
				return false;
			}
			break;
		}

		default:
			break;
	}

	return true;
}

// FUNCTION: XWA 0x51A9B0
int Yard_ShouldSuppressProximityPair(unsigned int ownerObjIdx, unsigned int candidateObjIdx) {
	int candidateType;

	if (g_objectTable[ownerObjIdx].playerOwnerIdx != -1 &&
		g_objectTable[candidateObjIdx].playerOwnerIdx != -1) {
		if (!g_flightCollisionsEnabled) {
			return true;
		}
	}

	switch (g_objectTable[ownerObjIdx].objectType) {
		case OBJ_ChuteMouth:
		case OBJ_ChuteTunnel:
		case OBJ_SalvageRoom:
		case OBJ_Compactor:
		case OBJ_AccelRing:
		case OBJ_AccelRing2:
		case OBJ_SmeltingRoom:
		case OBJ_SRTubeNOBend:
		case OBJ_SRTubeUP:
		case OBJ_SRTubeDown:
		case OBJ_SRTubeLH:
		case OBJ_SRTubeRH:
		case OBJ_Centrifuge:
		case OBJ_AccelRing3:
		case OBJ_ContainerGrandePG:
		case OBJ_Asteroid01:
		case OBJ_Asteroid02:
		case OBJ_Asteroid03:
			return true;

		case OBJ_Junk01:
		case OBJ_Junk02:
		case OBJ_Junk03:
		case OBJ_Junk04:
		case OBJ_Junk05:
		case OBJ_Junk06:
		case OBJ_Junk07:
		case OBJ_Junk08:
		case OBJ_Junk09:
		case OBJ_Junk10:
			candidateType = g_objectTable[candidateObjIdx].objectType;
			if (candidateType < OBJ_AccelRing ||
				(candidateType > OBJ_Asteroid03 && candidateType != OBJ_MoltenBlock)) {
				break;
			}
			/* Fall through. */

		case OBJ_JunkBlock:
		case OBJ_MoltenBlock:
			return true;

		default:
			break;
	}

	return false;
}

// FUNCTION: XWA 0x5152F0
// Updates Pilot Proving Ground / Yard challenge progress for all players: countdown audio/messages,
// per-player course-state transitions from collision contacts, checkpoint/lap accounting, out-of-course
// penalties (rewind + speed cap), per-state scoring, finish detection, mission goal completion, and a
// delayed high-score insertion once all relevant players have finished.
void Yard_UpdateChallengeProgressAndScoring(int deltaTicks) {
	int playerIdx;
	unsigned int categoryIdx = g_yardChallengeMode;
	int allPlayersFinished = 1; // cleared as soon as any still-racing player is seen
	int anyFinished = 0;        // v114
	int maxFinishTime = 0;      // Block: latest finish time across players
	int allFinishedGate = 1;    // v1
	int nowSeconds = Mission_GameTimeToSeconds(g_missionElapsedClock.hours, g_missionElapsedClock.minutes,
											   g_missionElapsedClock.seconds);

	(void)deltaTicks;

	// Pre-start countdown audio/messages (only while clock minutes==0 and a fresh second elapsed).
	if (g_yardContext.countdownSecondsRemaining > 0 && !g_missionElapsedClock.minutes &&
		g_missionElapsedClock.seconds > 0 && !g_flightSimSideEffectsSuppressed) {
		g_yardContext.countdownSecondsRemaining -= g_missionElapsedClock.seconds;
		if (g_yardContext.countdownSecondsRemaining < 0) {
			g_yardContext.countdownSecondsRemaining = 0;
		}
		g_missionElapsedClock.seconds = 0;
		if (g_yardContext.countdownSecondsRemaining > 5) {
			msg_emitInFlightMessage(MSG_YARD_COUNTDOWN_GETREADY, g_localPlayer);
		} else {
			if (g_yardContext.countdownSecondsRemaining > 0) {
				fsfx_PlaySound(67, 0xFFFF, g_localPlayer);
			} else {
				int i;

				fsfx_PlaySound(85, 0xFFFF, g_localPlayer);
				for (i = 0; i < g_flightPlayerCount; ++i) {
					Hud_ToggleMfdSide(i, 2);
				}
			}
			switch (g_yardContext.countdownSecondsRemaining) {
				case 5:
					msg_emitInFlightMessage(MSG_YARD_COUNTDOWN_5, g_localPlayer);
					break;
				case 4:
					msg_emitInFlightMessage(MSG_YARD_COUNTDOWN_4, g_localPlayer);
					break;
				case 3:
					msg_emitInFlightMessage(MSG_YARD_COUNTDOWN_3, g_localPlayer);
					break;
				case 2:
					msg_emitInFlightMessage(MSG_YARD_COUNTDOWN_2, g_localPlayer);
					break;
				case 1:
					msg_emitInFlightMessage(MSG_YARD_COUNTDOWN_1, g_localPlayer);
					break;
				case 0:
					msg_emitInFlightMessage(MSG_YARD_COUNTDOWN_GO, g_localPlayer);
					break;
				default:
					break;
			}
		}
	}

	playerIdx = 0;
	if (g_flightPlayerCount > 0) {
		for (; playerIdx < g_flightPlayerCount; ++playerIdx) {
			int objectIndex;
			unsigned int sourceObjIdx;
			int prevCourseState;
			int collisionObjIdx;
			ObjectTypeId objectType;
			int objTypeU;
			int courseStateToSet;
			int penaltyLevel;
			if (!g_players[playerIdx].connectedFlag) {
				continue;
			}
			objectIndex = g_players[playerIdx].objectIndex;
			sourceObjIdx = (unsigned int)objectIndex;
			prevCourseState = g_yardContext.playerChallengeStates[playerIdx].courseState;
			if (objectIndex == 0xFFFF) {
				continue;
			}

			if (g_yardContext.countdownSecondsRemaining > 0) {
				g_objectTable[objectIndex].mobj->speed = 0;
			}

			collisionObjIdx = g_objectTable[objectIndex].mobj->collisionObjIdx;
			if (collisionObjIdx != 0xFFFF) {
				objectType = g_objectTable[collisionObjIdx].objectType;
			} else {
				objectType = OBJ_None;
			}

			if (g_flightSimSideEffectsSuppressed || g_yardStartMessageShown) {
				courseStateToSet = 1;
			} else {
				if (playerIdx == g_localPlayer) {
					int pick = (uint16_t)GameRand2() % 3;
					switch (pick) {
						case 0:
							g_pendingHudMessageVoiceSfxId = 203;
							msg_addMessagePtr(0, g_strYardStrings[15]); // "show us what you can do"
							break;
						case 1:
							g_pendingHudMessageVoiceSfxId = 204;
							msg_addMessagePtr(0, g_strYardStrings[14]); // "give it your best"
							break;
						default:
							break;
					}
					msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
					g_pendingHudMessageVoiceSfxId = 0;
				}
				courseStateToSet = 1;
				g_yardStartMessageShown = 1;
			}
			objTypeU = (uint16_t)objectType;
			// Course-state transition driven by the object the craft is currently touching.
			if (objTypeU > OBJ_SalvageRoom) {
				switch (objTypeU) {
					case OBJ_Compactor:
						g_yardContext.playerChallengeStates[playerIdx].courseState = 2;
						g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx = 0;
						g_yardContext.playerChallengeStates[playerIdx].nextCourseSide = 1;
						g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx = -1;
						g_yardContext.playerChallengeStates[playerIdx].currentCourseSide = 1;
						if (!g_yardCompactorHintShown &&
#ifdef XWA_MODERN
							Yard_ModernIsLegacyCadenceDue() &&
#endif
							(uint16_t)GameRand2() % 10 == 1 && !g_flightSimSideEffectsSuppressed &&
							playerIdx == g_localPlayer && g_yardChallengeMode >= 4 &&
							(g_yardContext.compactorCycleState == 1 ||
							 g_yardContext.compactorCycleState == 3)) {
							if (g_yardChallengeMode == 4) {
								g_pendingHudMessageVoiceSfxId = 212;
								msg_addMessagePtr(0, g_strYardStrings[7]); // "watch the compactor"
								msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
								g_pendingHudMessageVoiceSfxId = 0;
							} else if ((uint16_t)GameRand2() % 2 == 0) {
								g_pendingHudMessageVoiceSfxId = 212;
								msg_addMessagePtr(0, g_strYardStrings[7]); // "watch the compactor"
								msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
								g_pendingHudMessageVoiceSfxId = 0;
							}
							g_yardCompactorHintShown = 1;
						}
						break;
					case OBJ_AccelRing2: {
						int idx = 0;
						while (idx < 30 &&
							   collisionObjIdx != g_yardContext.courseSide1Checkpoints[idx].objectIdx) {
							++idx;
						}
						courseStateToSet = (idx < 30) ? 3 : 7;
						g_yardContext.playerChallengeStates[playerIdx].courseState = courseStateToSet;
						break;
					}
					case OBJ_SmeltingRoom:
						g_yardContext.playerChallengeStates[playerIdx].courseState = 4;
						if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit ||
							g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
							g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx = 29;
							g_yardContext.playerChallengeStates[playerIdx].nextCourseSide = 1;
							g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx = 0;
							g_yardContext.playerChallengeStates[playerIdx].currentCourseSide = 2;
						} else {
							g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx = 0;
							g_yardContext.playerChallengeStates[playerIdx].nextCourseSide = 2;
							g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx = 29;
							g_yardContext.playerChallengeStates[playerIdx].currentCourseSide = 1;
						}
						if (!g_yardWatchLasersHintShown &&
#ifdef XWA_MODERN
							Yard_ModernIsLegacyCadenceDue() &&
#endif
							(uint16_t)GameRand2() % 10 == 1 && !g_flightSimSideEffectsSuppressed &&
							playerIdx == g_localPlayer && g_yardChallengeMode >= 4) {
							if (g_yardChallengeMode == 4 || (uint16_t)GameRand2() % 2 == 0) {
								g_pendingHudMessageVoiceSfxId = 211;
								msg_addMessagePtr(0, g_strYardStrings[6]); // "watch the lasers"
								msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
								g_pendingHudMessageVoiceSfxId = 0;
							}
							g_yardWatchLasersHintShown = 1;
						}
						break;
					case OBJ_SRTubeNOBend:
					case OBJ_SRTubeUP:
					case OBJ_SRTubeDown:
					case OBJ_SRTubeLH:
					case OBJ_SRTubeRH:
						g_yardContext.playerChallengeStates[playerIdx].courseState = 5;
						break;
					case OBJ_Centrifuge:
						g_yardContext.playerChallengeStates[playerIdx].courseState = 6;
						if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit ||
							g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
							g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx = 29;
							g_yardContext.playerChallengeStates[playerIdx].nextCourseSide = 1;
							g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx = 0;
							g_yardContext.playerChallengeStates[playerIdx].currentCourseSide = 2;
						} else {
							g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx = 0;
							g_yardContext.playerChallengeStates[playerIdx].nextCourseSide = 2;
							g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx = 29;
							g_yardContext.playerChallengeStates[playerIdx].currentCourseSide = 1;
						}
						if (!g_flightSimSideEffectsSuppressed && !g_yardDontStayLongHintShown &&
#ifdef XWA_MODERN
							Yard_ModernIsLegacyCadenceDue() &&
#endif
							(uint16_t)GameRand2() % 10 == 1 && playerIdx == g_localPlayer &&
							g_yardChallengeMode >= 4) {
							if (g_yardChallengeMode == 4 || (uint16_t)GameRand2() % 2 == 0) {
								g_pendingHudMessageVoiceSfxId = 214;
								msg_addMessagePtr(0, g_strYardStrings[9]); // "don't stay long"
								msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
								g_pendingHudMessageVoiceSfxId = 0;
							}
							g_yardDontStayLongHintShown = 1;
						}
						break;
					case OBJ_AccelRing3:
						if (g_yardContext.playerChallengeStates[playerIdx].courseState) {
							g_yardContext.playerChallengeStates[playerIdx].courseState = 7;
						}
						break;
					case OBJ_ContainerGrandePG:
						g_yardContext.playerChallengeStates[playerIdx].courseState = 8;
						g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx = 29;
						g_yardContext.playerChallengeStates[playerIdx].nextCourseSide = 2;
						break;
					case OBJ_Asteroid02:
						break;
					case OBJ_Asteroid03:
						if (g_yardContext.playerChallengeStates[playerIdx].courseState == 3) {
							g_approxDist = collide_roughdistance3d(
								g_objectTable[objectIndex].world_x -
									g_objectTable[g_yardCourseSide1FinalObjIdx].world_x,
								g_objectTable[objectIndex].world_y -
									g_objectTable[g_yardCourseSide1FinalObjIdx].world_y,
								g_objectTable[objectIndex].world_z -
									g_objectTable[g_yardCourseSide1FinalObjIdx].world_z);
							if ((unsigned int)g_approxDist <= 0x3E00) {
								g_yardContext.playerChallengeStates[playerIdx].courseState = 9;
								g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx = 0;
								g_yardContext.playerChallengeStates[playerIdx].nextCourseSide = 2;
								Player_SetTarget((int16_t)g_yardContext.courseSide2Checkpoints[0].objectIdx,
												 playerIdx);
							}
						} else if (g_yardContext.playerChallengeStates[playerIdx].courseState == 7) {
							g_approxDist = collide_roughdistance3d(
								g_objectTable[objectIndex].world_x -
									g_objectTable[g_yardCourseSide2FinalObjIdx].world_x,
								g_objectTable[objectIndex].world_y -
									g_objectTable[g_yardCourseSide2FinalObjIdx].world_y,
								g_objectTable[objectIndex].world_z -
									g_objectTable[g_yardCourseSide2FinalObjIdx].world_z);
							if ((unsigned int)g_approxDist <= 0x7C00) {
								g_yardContext.playerChallengeStates[playerIdx].courseState = 10;
								g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx = 0;
								g_yardContext.playerChallengeStates[playerIdx].nextCourseSide = 1;
								Player_SetTarget((int16_t)g_yardContext.courseSide1Checkpoints[0].objectIdx,
												 playerIdx);
							}
						}
						break;
					default:
						break;
				}
			} else if (objTypeU >= OBJ_ChuteMouth) {
				g_yardContext.playerChallengeStates[playerIdx].courseState = courseStateToSet;
			}
			if (objectType == OBJ_Asteroid02 || objectType == OBJ_None) {
				switch (g_yardContext.playerChallengeStates[playerIdx].courseState) {
					case 1:
						g_yardContext.playerChallengeStates[playerIdx].courseState = 0;
						break;
					case 2:
					case 4:
						g_yardContext.playerChallengeStates[playerIdx].courseState = 3;
						break;
					case 6:
					case 8:
						g_yardContext.playerChallengeStates[playerIdx].courseState = 7;
						break;
					default:
						break;
				}
			}

			// Out-of-course detection and penalty (skipped when side effects are suppressed).
			penaltyLevel = 0;
			if (!g_flightSimSideEffectsSuppressed) {
				if (g_yardChallengeMode == 2) {
					int cs = g_yardContext.playerChallengeStates[playerIdx].courseState;
					if (prevCourseState != cs) {
						switch (prevCourseState) {
							case 0:
								if (cs != 3) {
									penaltyLevel = 2;
								}
								break;
							case 3:
								if (cs != 9) {
									penaltyLevel = 2;
								}
								break;
							case 7:
								if (cs != 10) {
									penaltyLevel = 2;
								}
								break;
							case 9:
								if (cs != 7) {
									penaltyLevel = 2;
								}
								break;
							case 10:
								if (cs != 3) {
									penaltyLevel = 2;
								}
								break;
							default:
								break;
						}
					}
				} else if (g_yardChallengeMode > 2 && g_yardChallengeMode <= 7 &&
						   Xwa_Abs32(prevCourseState -
									 g_yardContext.playerChallengeStates[playerIdx].courseState) > 1) {
					penaltyLevel = 2;
				}

				{
					int cs = g_yardContext.playerChallengeStates[playerIdx].courseState;
					if (cs == 3 || cs == 7) {
						int idx;
						unsigned int dist;
						if ((cs == 3 && g_yardContext.playerChallengeStates[playerIdx].nextCourseSide == 1) ||
							(cs == 7 && g_yardContext.playerChallengeStates[playerIdx].nextCourseSide == 2)) {
							idx = g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx;
						} else {
							idx = g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx;
						}
						if (idx < 0) {
							idx = 0;
						} else if (idx >= 30) {
							idx = 29;
						}
						if (g_yardChallengeMode != 2 || idx < 29) {
							int checkpointObjIdx = cs == 3
													   ? g_yardContext.courseSide1Checkpoints[idx].objectIdx
													   : g_yardContext.courseSide2Checkpoints[idx].objectIdx;
							dist = collide_roughdistance3d(
								g_objectTable[objectIndex].world_x - g_objectTable[checkpointObjIdx].world_x,
								g_objectTable[objectIndex].world_y - g_objectTable[checkpointObjIdx].world_y,
								g_objectTable[objectIndex].world_z - g_objectTable[checkpointObjIdx].world_z);
							g_approxDist = (int)dist;
							if (dist > 0x18000) {
								penaltyLevel = 2;
							} else if (dist > 0xC000) {
								penaltyLevel = 1;
							}
						}
					} else if (cs == 0 && g_yardChallengeMode >= 3) {
						unsigned int dist = (unsigned int)collide_roughdistance3d(
							g_objectTable[objectIndex].world_x -
								g_objectTable[g_yardChuteMouthObjIdx].world_x,
							g_objectTable[objectIndex].world_y -
								g_objectTable[g_yardChuteMouthObjIdx].world_y,
							g_objectTable[objectIndex].world_z -
								g_objectTable[g_yardChuteMouthObjIdx].world_z);
						g_approxDist = (int)dist;
						if (dist > 0x1F000) {
							penaltyLevel = 2;
						} else if (dist > 0x19000) {
							penaltyLevel = 1;
						}
					}
				}

				switch (penaltyLevel) {
					case 2: {
						int curIdx;
						int pu;
						if (playerIdx == g_localPlayer) {
							if (!g_yardStopCheatingMessageShown) {
								fsfx_PlaySound(128, sourceObjIdx, g_localPlayer);
								switch ((uint16_t)GameRand2() % 2) {
									case 1:
										g_pendingHudMessageVoiceSfxId = 210;
										msg_addMessagePtr(0, g_strYardStrings[5]); // stop cheating 2
										break;
									case 0:
										g_pendingHudMessageVoiceSfxId = 209;
										msg_addMessagePtr(0, g_strYardStrings[4]); // stop cheating 1
										break;
									default:
										break;
								}
								msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
								g_pendingHudMessageVoiceSfxId = 0;
								if (g_flightPlayerCount == 1) {
									g_yardStopCheatingMessageShown = 1;
								}
							}
							g_yardLastCourseWarningGameTime = g_gameTime;
						}
						// Rewind to last reached checkpoint (or origin).
						curIdx = g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx;
						if (curIdx < 0) {
							g_objectTable[objectIndex].mobj->prevWorldX = 0;
							g_objectTable[objectIndex].world_x = 0;
							g_objectTable[objectIndex].mobj->prevWorldY = 0;
							g_objectTable[objectIndex].world_y = 0;
							g_objectTable[objectIndex].mobj->prevWorldZ = 0;
							g_objectTable[objectIndex].world_z = 0;
							g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit = 0;
						} else {
							int anchorIdx =
								(g_yardContext.playerChallengeStates[playerIdx].currentCourseSide == 1)
									? g_yardContext.courseSide1Checkpoints[curIdx].objectIdx
									: g_yardContext.courseSide2Checkpoints[curIdx].objectIdx;
							g_objectTable[objectIndex].mobj->prevWorldX = g_objectTable[anchorIdx].world_x;
							g_objectTable[objectIndex].world_x = g_objectTable[objectIndex].mobj->prevWorldX;
							g_objectTable[objectIndex].mobj->prevWorldY = g_objectTable[anchorIdx].world_y;
							g_objectTable[objectIndex].world_y = g_objectTable[objectIndex].mobj->prevWorldY;
							g_objectTable[objectIndex].mobj->prevWorldZ = g_objectTable[anchorIdx].world_z;
							g_objectTable[objectIndex].world_z = g_objectTable[objectIndex].mobj->prevWorldZ;
						}
						pu = g_yardContext.playerChallengeStates[playerIdx].penaltyUntilSeconds;
						g_yardContext.playerChallengeStates[playerIdx].penaltyUntilSeconds =
							(pu <= nowSeconds) ? (nowSeconds + 4) : (pu + 4);
						break;
					}
					case 1:
						if (playerIdx == g_localPlayer) {
							fsfx_PlaySound(127, sourceObjIdx, g_localPlayer);
							if (g_yardLastCourseWarningGameTime <= g_yardLastSafeCourseGameTime) {
								g_pendingHudMessageVoiceSfxId = 208;
								msg_addMessagePtr(0, g_strYardStrings[3]); // stay in course
								msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
								g_pendingHudMessageVoiceSfxId = 0;
							}
							g_yardLastCourseWarningGameTime = g_gameTime;
						}
						break;
					default:
						if (playerIdx == g_localPlayer) {
							g_yardLastSafeCourseGameTime = g_gameTime;
						}
						break;
				}
			}

			// Contact-type dispatch: checkpoints, compactor crush, laser tubes, hint zones.
			switch (objTypeU) {
				case OBJ_Compactor:
					if (!g_yardContext.compactorCycleState) {
						ObjectRecord* compObj;
						int loX;
						int loY;
						int loZ;
						ModelIndex compModel2;
						ObjectRecord* compObj2;
						int hiY;
						int hiZ;
						int px;
						ModelIndex compModel = GetModelIndexFromType(OBJ_Compactor);
						pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardContext.compactorObjIdx],
															g_modelDefs[compModel].dockPoints[0].x,
															g_modelDefs[compModel].dockPoints[0].z,
															g_modelDefs[compModel].dockPoints[0].y);
						compObj = &g_objectTable[g_yardContext.compactorObjIdx];
						loX = g_rotatedX + compObj->world_x;
						loY = g_rotatedY + compObj->world_y;
						loZ = g_rotatedZ + compObj->world_z;
						compModel2 = GetModelIndexFromType(OBJ_Compactor);
						pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_yardContext.compactorObjIdx],
															g_modelDefs[compModel2].dockPoints[2].x,
															g_modelDefs[compModel2].dockPoints[2].z,
															g_modelDefs[compModel2].dockPoints[2].y);
						compObj2 = &g_objectTable[g_yardContext.compactorObjIdx];
						hiY = g_rotatedY + compObj2->world_y;
						hiZ = g_rotatedZ + compObj2->world_z;
						px = g_objectTable[objectIndex].world_x;
						if (px > loX &&
							px < g_rotatedX + g_objectTable[g_yardContext.compactorObjIdx].world_x) {
							int py = g_objectTable[objectIndex].world_y;
							if (py > loY && py < hiY) {
								int pz = g_objectTable[objectIndex].world_z;
								if (pz > loZ && pz < hiZ) {
									fsfx_PlaySound(33, sourceObjIdx,
												   g_objectTable[objectIndex].playerOwnerIdx);
									collide_damagecraft(sourceObjIdx, 0xFFFF, 0xFFFFFFFE, 0x96, 0);
								}
							}
						}
					}
					if (!g_flightSimSideEffectsSuppressed && !g_yardAlmostDoneHintShown &&
						g_localPlayer == playerIdx && g_yardChallengeMode == 6) {
						if (g_yardContext.playerChallengeStates[g_localPlayer].carriedObjectPickedUp != 0 &&
#ifdef XWA_MODERN
							Yard_ModernIsLegacyCadenceDue() &&
#endif
							((uint16_t)GameRand2() % 20) == 0) {
							g_pendingHudMessageVoiceSfxId = 207;
							msg_addMessagePtr(0, g_strYardStrings[11]); // almost done
							msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
							g_pendingHudMessageVoiceSfxId = 0;
							g_yardAlmostDoneHintShown = 1;
						}
					}
					break;

				case OBJ_AccelRing:
				case OBJ_AccelRing2:
				case OBJ_AccelRing3: {
					int hitSide;
					int hitIdx;
					int expectedNext;
					int nextIdx;
					int nextSide;
					int newNextIdx;
					int newNextObj;
					int expectedCur;
					int curIdx = g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx;
					if (curIdx == -1) {
						expectedCur = 0xFFFF;
					} else if (g_yardContext.playerChallengeStates[playerIdx].currentCourseSide == 1) {
						expectedCur = g_yardContext.courseSide1Checkpoints[curIdx].objectIdx;
					} else {
						expectedCur = g_yardContext.courseSide2Checkpoints[curIdx].objectIdx;
					}
					if (collisionObjIdx == expectedCur &&
						(g_yardChallengeMode < 5 || curIdx != 29 ||
						 g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx != 29)) {
						break;
					}

					g_objectTable[g_players[playerIdx].objectIndex].mobj->speed += 120;
					if (!g_flightSimSideEffectsSuppressed) {
						fsfx_PlaySound(190, sourceObjIdx, g_objectTable[objectIndex].playerOwnerIdx);
					}

					// Identify which side and index the contacted checkpoint belongs to.
					hitSide = 0;
					hitIdx = 0;
					while (hitIdx < 30 &&
						   collisionObjIdx != g_yardContext.courseSide1Checkpoints[hitIdx].objectIdx) {
						++hitIdx;
					}
					if (hitIdx < 30) {
						hitSide = 1;
					}
					if (hitIdx >= 30) {
						hitIdx = 0;
						while (hitIdx < 30 &&
							   collisionObjIdx != g_yardContext.courseSide2Checkpoints[hitIdx].objectIdx) {
							++hitIdx;
						}
						if (hitIdx < 30) {
							hitSide = 2;
						}
						if (hitIdx >= 30) {
							break;
						}
					}

					// Award a small speed boost if a nearby accel ring gate was shot down by this player.
					if (collisionObjIdx != 0xFFFF) {
						ObjectRecord* ringObj;
						ObjectTypeId finalType;
						int scanIdx = hitIdx;
						int scanSide = hitSide;
						int ringObjIdx = -1;
						int steps = 0;
						for (;;) {
							ObjectTypeId ringType;
							int probeIdx;

#ifdef XWA_MODERN
							probeIdx = (scanIdx >= 30) ? 29 : scanIdx;
#else
							probeIdx = scanIdx;
#endif
							ringObjIdx = (scanSide == 1)
											 ? g_yardContext.courseSide1Checkpoints[probeIdx].objectIdx
											 : g_yardContext.courseSide2Checkpoints[probeIdx].objectIdx;
							ringType = g_objectTable[ringObjIdx].objectType;
							if (ringType == OBJ_AccelRing2) {
								break;
							}
							if (ringType != OBJ_AccelRing3) {
								if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit ||
									g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
									if (++scanIdx >= 30) {
										scanIdx = 0;
										if (++scanSide > 2) {
											scanSide = 1;
										}
									}
								} else if (--scanIdx < 0) {
									--scanSide;
									scanIdx = 30;
									if (scanSide <= 0) {
										scanSide = 2;
									}
								}
								if (++steps < 5) {
									continue;
								}
							}
							break;
						}
						ringObj = &g_objectTable[ringObjIdx];
						finalType = ringObj->objectType;
						if (finalType == OBJ_AccelRing2 || finalType == OBJ_AccelRing3) {
							CraftData* ringCraft = ringObj->mobj->pCraft;
							if (!ringCraft->componentHp[4] && ringCraft->damageFromPlayer[playerIdx]) {
								g_objectTable[g_players[playerIdx].objectIndex].mobj->speed += 60;
							}
						}
					}

					expectedNext = -1;
					nextIdx = g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx;
					if ((unsigned int)nextIdx <= 0x1D) {
						expectedNext = (g_yardContext.playerChallengeStates[playerIdx].nextCourseSide == 1)
										   ? g_yardContext.courseSide1Checkpoints[nextIdx].objectIdx
										   : g_yardContext.courseSide2Checkpoints[nextIdx].objectIdx;
						if (expectedNext != 0xFFFF) {
							g_objectTable[expectedNext].mobj->nodeSwitchIndex = 1;
						}
					}

					if (collisionObjIdx == expectedNext) {
						if (g_yardContext.playerChallengeStates[playerIdx].remainingCheckpointCount > 0) {
							g_yardContext.playerChallengeStates[playerIdx].remainingCheckpointCount -= 1;
						}
						if (!g_flightSimSideEffectsSuppressed && playerIdx == g_localPlayer &&
							g_yardContext.playerChallengeStates[playerIdx].lapsRemaining == 1 &&
							!g_yardAlmostDoneHintShown &&
							g_yardContext.playerChallengeStates[g_localPlayer].remainingCheckpointCount <
								30) {
							switch (g_yardChallengeMode) {
								case 0:
									if (hitSide == 2 && nextIdx > 15 && (uint16_t)GameRand2() % 2 != 0) {
										g_pendingHudMessageVoiceSfxId = 207;
										msg_addMessagePtr(0, g_strYardStrings[11]); // almost done
										msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
										g_pendingHudMessageVoiceSfxId = 0;
										g_yardAlmostDoneHintShown = 1;
									}
									break;
								case 1:
									if (hitSide == 2 && nextIdx > 15 && (uint16_t)GameRand2() % 3 == 0) {
										g_pendingHudMessageVoiceSfxId = 207;
										msg_addMessagePtr(0, g_strYardStrings[11]); // almost done
										msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
										g_pendingHudMessageVoiceSfxId = 0;
										g_yardAlmostDoneHintShown = 1;
									}
									break;
								case 3:
								case 4:
									if (hitSide == 2 && (uint16_t)GameRand2() % 4 == 0) {
										g_pendingHudMessageVoiceSfxId = 207;
										msg_addMessagePtr(0, g_strYardStrings[11]); // almost done
										msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
										g_pendingHudMessageVoiceSfxId = 0;
										g_yardAlmostDoneHintShown = 1;
									}
									break;
								default:
									break;
							}
						}
					} else {
						// Hit an unexpected checkpoint: adjust remaining count and time penalty.
						int nextRef = g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx;
						int penaltyTicks = 0;
						if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit ||
							g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
							if (g_yardContext.playerChallengeStates[playerIdx].nextCourseSide == hitSide &&
								hitIdx < nextRef) {
								g_yardContext.playerChallengeStates[playerIdx].remainingCheckpointCount +=
									hitIdx - nextRef;
								penaltyTicks = 5 * (nextRef - hitIdx);
							}
						} else {
							if (g_yardContext.playerChallengeStates[playerIdx].nextCourseSide != hitSide) {
								nextRef += 30;
							}
							if (hitIdx > nextRef) {
								penaltyTicks = 5 * (hitIdx - nextRef);
								g_yardContext.playerChallengeStates[playerIdx].remainingCheckpointCount +=
									nextRef - hitIdx;
							}
						}
						if (penaltyTicks) {
							int pu = g_yardContext.playerChallengeStates[playerIdx].penaltyUntilSeconds;
							g_yardContext.playerChallengeStates[playerIdx].penaltyUntilSeconds =
								(pu <= nowSeconds) ? (penaltyTicks + nowSeconds) : (pu + penaltyTicks);
						}
					}

					g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx = hitIdx;
					g_yardContext.playerChallengeStates[playerIdx].currentCourseSide = hitSide;
					nextSide = hitSide;
					if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit ||
						g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
						newNextIdx = hitIdx - 1;
						if (newNextIdx < 0) {
							newNextIdx = 29;
							nextSide = 2 - (hitSide != 1);
						}
					} else {
						newNextIdx = hitIdx + 1;
						if (newNextIdx >= 30) {
							newNextIdx = 0;
							nextSide = 2 - (hitSide != 1);
						}
					}
					g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx = newNextIdx;
					g_yardContext.playerChallengeStates[playerIdx].nextCourseSide = nextSide;
					newNextObj = (nextSide == 1) ? g_yardContext.courseSide1Checkpoints[newNextIdx].objectIdx
												 : g_yardContext.courseSide2Checkpoints[newNextIdx].objectIdx;
					if (newNextObj != 0xFFFF) {
						g_objectTable[newNextObj].mobj->nodeSwitchIndex = 0;
						if (g_players[playerIdx].currentTargetObjectIdx != 0xffffu) {
							int targetType =
								(uint16_t)g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx]
									.objectType;
							if (targetType >= 0x17C &&
								((uint16_t)targetType <= (uint16_t)OBJ_AccelRing2 || targetType == 389)) {
								Player_SetTarget((int16_t)newNextObj, playerIdx);
							}
						}
					}
					if (g_yardChallengeMode <= 1) {
						g_yardContext.playerChallengeStates[playerIdx].courseState = (nextSide != 1) ? 7 : 3;
						break;
					}
					if (g_yardChallengeMode == 2) {
						if (nextSide == hitSide) {
							g_yardContext.playerChallengeStates[playerIdx].courseState =
								(nextSide != 1) ? 7 : 3;
							break;
						}
						g_yardContext.playerChallengeStates[playerIdx].courseState =
							(g_yardContext.playerChallengeStates[playerIdx].courseState != 3) + 9;
					}
					break;
				}

				case OBJ_SmeltingRoom:
					if (g_flightSimSideEffectsSuppressed || playerIdx != g_localPlayer) {
						break;
					}
					if (g_yardChallengeMode == 5) {
						if (!g_yardAlmostDoneHintShown &&
							g_yardContext.playerChallengeStates[g_localPlayer].ringCheckpointHit != 0 &&
#ifdef XWA_MODERN
							Yard_ModernIsLegacyCadenceDue() &&
#endif
							((uint16_t)GameRand2() % 20) == 0) {
							g_pendingHudMessageVoiceSfxId = 207;
							msg_addMessagePtr(0, g_strYardStrings[11]); // almost done
							msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
							g_pendingHudMessageVoiceSfxId = 0;
							g_yardAlmostDoneHintShown = 1;
						}
						break;
					}
					if (g_yardChallengeMode != 6) {
						if (g_yardChallengeMode == 7 && !g_yardAlmostDoneHintShown &&
							g_yardContext.playerChallengeStates[g_localPlayer].ringCheckpointHit &&
							g_yardContext.playerChallengeStates[g_localPlayer].carriedObjectPickedUp != 0 &&
#ifdef XWA_MODERN
							Yard_ModernIsLegacyCadenceDue() &&
#endif
							((uint16_t)GameRand2() % 20) == 0) {
							g_pendingHudMessageVoiceSfxId = 207;
							msg_addMessagePtr(0, g_strYardStrings[11]); // almost done
							msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
							g_pendingHudMessageVoiceSfxId = 0;
							g_yardAlmostDoneHintShown = 1;
						}
						break;
					}
					// Challenge mode 6 shares the container-zone reminder below.

				case OBJ_ContainerGrandePG:
					if (objectType == OBJ_SmeltingRoom ||
						(!g_flightSimSideEffectsSuppressed && playerIdx == g_localPlayer &&
						 g_yardChallengeMode == 7)) {
						if (g_yardChallengeEventTimer > 1888 &&
#ifdef XWA_MODERN
							Yard_ModernIsLegacyCadenceDue() &&
#endif
							((uint16_t)GameRand2() % 80) == 0 &&
							!g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
							fsfx_PlaySound(122, sourceObjIdx, g_objectTable[objectIndex].playerOwnerIdx);
							g_yardChallengeEventTimer = 0;
						}
					}
					break;

				case OBJ_SRTubeNOBend:
				case OBJ_SRTubeUP:
				case OBJ_SRTubeDown:
				case OBJ_SRTubeLH:
				case OBJ_SRTubeRH:
					if (g_yardChallengeMode > 3 &&
						g_gameTime > g_players[playerIdx].impactDamageCooldownTime) {
						g_players[playerIdx].impactDamageCooldownTime = g_gameTime + 118;
						collide_damagecraft(sourceObjIdx, 0xFFFF, 0xFFFFFFFE, 0xA, 0);
					}
					break;

				default:
					break;
			}

			// Per-player scoring and finish detection (every contact path above reaches here).
			{
				int score;
				// Apply speed penalty while serving a course penalty.
				if (g_yardContext.playerChallengeStates[playerIdx].penaltyUntilSeconds > nowSeconds &&
					(g_yardChallengeMode < 6 ||
					 g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp)) {
					MobileObject* pm = g_objectTable[g_players[playerIdx].objectIndex].mobj;
					if (pm->speed > 0x14) {
						pm->speed = 20;
					}
				}

				// Compute running challenge score.
				if (g_yardContext.playerChallengeStates[playerIdx].finished) {
					score = 2360000 - g_yardContext.playerChallengeStates[playerIdx].finishTimeSeconds;
				} else {
					int cs;
					score = 100000 * (g_yardChallengeLapCount -
									  g_yardContext.playerChallengeStates[playerIdx].lapsRemaining);
					cs = g_yardContext.playerChallengeStates[playerIdx].courseState;
					if (g_yardChallengeMode == 2) {
						switch (cs) {
							case 3:
								score += 100;
								break;
							case 7:
								score += 300;
								break;
							case 9:
								score += 200;
								break;
							case 10:
								score += 400;
								break;
							default:
								break;
						}
					} else if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit ||
							   g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
						score += 100 * (108 - cs);
					} else {
						score += 100 * cs;
					}
					switch (cs) {
						case 1:
							if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit ||
								g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
								score +=
									g_yardChuteTunnelEndObjIdx - collisionObjIdx - g_yardChuteMouthObjIdx;
							} else {
								score += collisionObjIdx - g_yardChuteMouthObjIdx;
							}
							break;
						case 3:
							if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit ||
								g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
								score +=
									30 - g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx;
							} else {
								score += g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx;
							}
							break;
						case 5:
							if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit ||
								g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
								score += (g_yardAdvancedCourseTubeLastObjIdx - collisionObjIdx) -
										 g_yardAdvancedCourseTubeFirstObjIdx;
							} else {
								score += collisionObjIdx - g_yardAdvancedCourseTubeFirstObjIdx;
							}
							break;
						case 7:
							if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit ||
								g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
								score +=
									30 - g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx;
							} else {
								score += g_yardContext.playerChallengeStates[playerIdx].currentCheckpointIdx;
							}
							break;
						case 9:
							g_approxDist = collide_roughdistance3d(
								g_objectTable[objectIndex].world_x -
									g_objectTable[g_yardContext.courseSide2Checkpoints[0].objectIdx].world_x,
								g_objectTable[objectIndex].world_y -
									g_objectTable[g_yardContext.courseSide2Checkpoints[0].objectIdx].world_y,
								g_objectTable[objectIndex].world_z -
									g_objectTable[g_yardContext.courseSide2Checkpoints[0].objectIdx].world_z);
							score += 100 - 100 * g_approxDist / (unsigned int)g_yardCourseSide1ToSide2GapDist;
							break;
						case 10:
							g_approxDist = collide_roughdistance3d(
								g_objectTable[objectIndex].world_x -
									g_objectTable[g_yardContext.courseSide1Checkpoints[0].objectIdx].world_x,
								g_objectTable[objectIndex].world_y -
									g_objectTable[g_yardContext.courseSide1Checkpoints[0].objectIdx].world_y,
								g_objectTable[objectIndex].world_z -
									g_objectTable[g_yardContext.courseSide1Checkpoints[0].objectIdx].world_z);
							score += 100 - 100 * g_approxDist / (unsigned int)g_yardCourseSide2ToSide1GapDist;
							break;
						default:
							break;
					}
				}
				g_yardContext.playerChallengeStates[playerIdx].score = score;

				// Carry/compactor/laser ambient voice barks (local player only).
				if (playerIdx == g_localPlayer && !g_flightSimSideEffectsSuppressed) {
					int cs;
					if (g_yardChallengeEventTimer > 1888 &&
						g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp &&
#ifdef XWA_MODERN
						Yard_ModernIsLegacyCadenceDue() &&
#endif
						((uint16_t)GameRand2() % 120) == 0) {
						switch (g_yardContext.playerChallengeStates[playerIdx].courseState) {
							case 0:
							case 3:
								fsfx_PlaySound(121, sourceObjIdx, g_objectTable[objectIndex].playerOwnerIdx);
								g_yardChallengeEventTimer = 0;
								break;
							case 2:
							case 4:
							case 5:
							case 6:
								fsfx_PlaySound(122, sourceObjIdx, g_objectTable[objectIndex].playerOwnerIdx);
								g_yardChallengeEventTimer = 0;
								break;
							case 7:
								fsfx_PlaySound(125, sourceObjIdx, g_objectTable[objectIndex].playerOwnerIdx);
								g_yardChallengeEventTimer = 0;
								break;
							default:
								break;
						}
					}
					if (g_yardContext.playerChallengeStates[playerIdx].courseState == 2 &&
						g_yardContext.compactorCycleState == 2 &&
						!Sound_CountPlayingInstances(g_sfxIds[186])) {
						fsfx_PlaySound(186, g_yardContext.compactorObjIdx,
									   g_objectTable[objectIndex].playerOwnerIdx);
					}
					cs = g_yardContext.playerChallengeStates[playerIdx].courseState;
					if (prevCourseState == 5) {
						if (cs != 5) {
							Sound_StopOldestInstance(g_sfxIds[193]);
						}
					} else if (cs == 5 && !Sound_CountPlayingInstances(g_sfxIds[193])) {
						Sound_QueueEffect(g_sfxIds[193], 1, 1, 80, 100, 64, -1,
										  g_objectTable[objectIndex].playerOwnerIdx);
					}
				}

				if (!g_yardContext.playerChallengeStates[playerIdx].finished &&
					!g_flightSimSideEffectsSuppressed) {
					int r2d2;

					allPlayersFinished = 0;
					if (g_yardChallengeMode > 2) {
						if (g_yardContext.playerChallengeStates[playerIdx].courseState == 8) {
							if (!g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit) {
								fsfx_PlaySound(69, sourceObjIdx, g_objectTable[objectIndex].playerOwnerIdx);
							}
							g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit = 1;
							if (g_yardChallengeMode == 5) {
								Player_SetTarget((int16_t)g_yardContext.courseSide2Checkpoints[29].objectIdx,
												 playerIdx);
							} else if (g_yardChallengeMode == 7) {
								Player_SetTarget((int16_t)g_yardR2D2ObjIdx, playerIdx);
							}
						}
					} else if ((collisionObjIdx == g_yardCourseSide2FinalObjIdx ||
								(g_yardContext.playerChallengeStates[playerIdx].nextCourseSide == 1 &&
								 g_yardContext.playerChallengeStates[playerIdx].nextCheckpointIdx <= 4)) &&
							   g_yardContext.playerChallengeStates[playerIdx].remainingCheckpointCount -
									   g_yardCheckpointsPerLapByChallengeMode[g_yardChallengeMode] *
										   (g_yardContext.playerChallengeStates[playerIdx].lapsRemaining -
											1) <
								   g_yardCheckpointsPerLapByChallengeMode[g_yardChallengeMode] >> 1) {
						if (!g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit) {
							fsfx_PlaySound(69, sourceObjIdx, g_objectTable[objectIndex].playerOwnerIdx);
						}
						g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit = 1;
					}
					if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit &&
						objectType == OBJ_ChuteMouth) {
						if (!g_yardContext.playerChallengeStates[playerIdx].chuteCheckpointHit) {
							fsfx_PlaySound(69, sourceObjIdx, g_objectTable[objectIndex].playerOwnerIdx);
						}
						g_yardContext.playerChallengeStates[playerIdx].chuteCheckpointHit = 1;
					}

					r2d2 = g_yardR2D2ObjIdx;
					if (g_yardR2D2ObjIdx != 0xFFFF) {
						if (g_objectTable[objectIndex].mobj->pCraft->carriedObjectIndex == g_yardR2D2ObjIdx) {
							if (!g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
								fsfx_PlaySound(121, sourceObjIdx, g_objectTable[objectIndex].playerOwnerIdx);
								g_yardChallengeEventTimer = 0;
							}
							r2d2 = g_yardR2D2ObjIdx;
							g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp = 1;
						} else {
							g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp = 0;
						}
					}
					if (r2d2 != 0xFFFF &&
						g_yardContext.playerChallengeStates[playerIdx].carriedObjectPickedUp) {
						g_approxDist = collide_roughdistance3d(
							g_objectTable[r2d2].world_x - g_objectTable[g_yardShuttleObjIdx].world_x,
							g_objectTable[r2d2].world_y - g_objectTable[g_yardShuttleObjIdx].world_y,
							g_objectTable[r2d2].world_z - g_objectTable[g_yardShuttleObjIdx].world_z);
						if ((unsigned int)g_approxDist <= 0x2000) {
							if (!g_yardContext.playerChallengeStates[playerIdx].carriedObjectDelivered) {
								fsfx_PlaySound(194, sourceObjIdx, g_objectTable[objectIndex].playerOwnerIdx);
								g_yardChallengeEventTimer = 0;
							}
							g_yardContext.playerChallengeStates[playerIdx].carriedObjectDelivered = 1;
						}
					}

					switch (g_yardChallengeMode) {
						case 0:
						case 1:
						case 2:
							if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit) {
								int laps = g_yardContext.playerChallengeStates[playerIdx].lapsRemaining - 1;
								g_yardContext.playerChallengeStates[playerIdx].lapsRemaining = laps;
								if (laps <= 0) {
									g_yardContext.playerChallengeStates[playerIdx].finished = 1;
									if (g_yardChallengeMode <= 1) {
										g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit = 0;
									}
								} else {
									if (g_yardChallengeMode == 2 && !g_flightSimSideEffectsSuppressed &&
										!g_yardAlmostDoneHintShown && playerIdx == g_localPlayer &&
										laps == 1 &&
										g_yardContext.playerChallengeStates[playerIdx].courseState == 7) {
										g_pendingHudMessageVoiceSfxId = 207;
										msg_addMessagePtr(0, g_strYardStrings[11]); // almost done
										msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
										g_pendingHudMessageVoiceSfxId = 0;
										g_yardAlmostDoneHintShown = 1;
									}
									g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit = 0;
									Player_SetTarget(
										(int16_t)g_yardContext.courseSide1Checkpoints[0].objectIdx,
										playerIdx);
								}
							}
							break;
						case 3:
						case 4:
							if (g_yardContext.playerChallengeStates[playerIdx].ringCheckpointHit) {
								g_yardContext.playerChallengeStates[playerIdx].finished = 1;
							}
							break;
						case 5:
							if (g_yardContext.playerChallengeStates[playerIdx].chuteCheckpointHit) {
								g_yardContext.playerChallengeStates[playerIdx].finished = 1;
							}
							break;
						case 6:
						case 7:
							if (g_yardContext.playerChallengeStates[playerIdx].carriedObjectDelivered) {
								g_yardContext.playerChallengeStates[playerIdx].finished = 1;
							}
							break;
						default:
							break;
					}

					if (g_yardContext.playerChallengeStates[playerIdx].finished) {
						if (!g_yardContext.playerChallengeStates[playerIdx].finishTimeSeconds) {
							int playerIff;
							int finishSec = Mission_GameTimeToSeconds(g_missionElapsedClock.hours,
																	  g_missionElapsedClock.minutes,
																	  g_missionElapsedClock.seconds);
							g_yardContext.playerChallengeStates[playerIdx].finishTimeSeconds = finishSec;
							playerIff = (uint16_t)g_players[playerIdx].playerIff;
							g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] = 1;
							g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerIff] =
								finishSec;
						}
						if (g_localPlayer == playerIdx) {
							fsfx_speakorderack(g_localPlayer, -1, 38, -1, 0xFFFF, 0xFFFF);
						}
					}
				}

				// Finished (already or just now): finish banner + record latest finish time.
				if (g_yardContext.playerChallengeStates[playerIdx].finished &&
					!g_flightSimSideEffectsSuppressed) {
					anyFinished = 1;
					if (playerIdx == g_localPlayer && !g_yardFinishMessageShown) {
						switch ((uint16_t)GameRand2() % 2) {
							case 1:
								g_pendingHudMessageVoiceSfxId = 216;
								msg_addMessagePtr(0, g_strYardStrings[17]); // you finished
								break;
							case 0:
								g_pendingHudMessageVoiceSfxId = 215;
								msg_addMessagePtr(0, g_strYardStrings[16]); // you completed
								break;
							default:
								break;
						}
						msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
						g_pendingHudMessageVoiceSfxId = 0;
						g_yardFinishMessageShown = 1;
					}
					if (maxFinishTime < g_yardContext.playerChallengeStates[playerIdx].finishTimeSeconds) {
						maxFinishTime = g_yardContext.playerChallengeStates[playerIdx].finishTimeSeconds;
					}
				}
			}
		}
		allFinishedGate = allPlayersFinished;
	}

	if (anyFinished && g_yardChallengeMode >= 6) {
		allFinishedGate = 1;
	}
	if (allFinishedGate && !g_flightSimSideEffectsSuppressed) {
		int doneSeconds = Mission_GameTimeToSeconds(
			g_missionElapsedClock.hours, g_missionElapsedClock.minutes, g_missionElapsedClock.seconds);
		if (doneSeconds > maxFinishTime + 3) {
			int slot;
			YardHighScoreTable* table = Yard_LoadOrCreateHighScoreTable();
			int count = g_flightPlayerCount;
			PlayerData* pl = g_players;
			YardPlayerChallengeState* challengeState = g_yardContext.playerChallengeStates;
			for (slot = 0; slot < count; ++slot, ++pl, ++challengeState) {
				ObjectTypeId craftType;
				int inserted;
				int playerIff;
				if (!pl->connectedFlag || pl->objectIndex == 0xFFFF) {
					continue;
				}
				craftType = g_objectTable[pl->objectIndex].objectType;
				if (count == 1) {
					const char* pilotName = NetSession_GetPlayerName(slot);
					inserted = Yard_InsertCraftHighScore(table, categoryIdx, (uint16_t)craftType, pilotName,
														 challengeState->finishTimeSeconds);
				} else {
					inserted = 0;
				}
				if (slot == g_localPlayer) {
					g_yardFinishPlacementMessagePending = 1;
					if (inserted <= 0) {
						YardCraftScoreTable* craftTable =
							Yard_FindCraftScoreTableByObjectType((uint16_t)craftType, table);
						if (craftTable) {
							g_yardFinishPlacementResultCode = (10 * craftTable->scores[categoryIdx][9] / 9 <
															   challengeState->finishTimeSeconds) +
															  1000;
						} else {
							g_yardFinishPlacementResultCode = 1001;
						}
					} else {
						g_yardFinishPlacementResultCode = inserted;
					}
				}
				playerIff = (uint16_t)pl->playerIff;
				g_hangarMissionResolved = 1;
				g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] = 1;
				g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerIff] =
					challengeState->finishTimeSeconds;
				g_flightMissionEndPending = 1;
			}
			Yard_FreeHighScoreTable(table);
		}
	}
}

static __inline int Yard_HasConnectedPlayerInCourseState(int courseState) {
	int i;
	int playerCount = g_flightPlayerCount;

	for (i = 0; i < playerCount; ++i) {
		if (g_players[i].connectedFlag && g_yardContext.playerChallengeStates[i].courseState == courseState) {
			return 1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x515050
// Per-tick update dispatcher for Combat Chamber / Pilot Proving Ground Yard challenges. Accumulates
// the challenge-event and R2-D2 bump SFX timers while flight side effects are enabled, animates the
// node-switching smelting-room / centrifuge / advanced-course-tube meshes while any connected player
// is actively running an advanced course (course state 4, 5, or 6), runs the rubble/junk/compactor/
// smelting/centrifuge/accel-ring helpers selected by the current challenge mode, and finishes with the
// common challenge progress/scoring update. Always returns 0.
int Yard_UpdateChallengeTick(int deltaTicks) {
	if (!g_flightSimSideEffectsSuppressed) {
		g_yardChallengeEventTimer += deltaTicks;
		g_yardR2D2BumpSfxTimer += deltaTicks;
	}

	if (g_yardChallengeMode >= 3u) {
		if (Yard_HasConnectedPlayerInCourseState(4) || Yard_HasConnectedPlayerInCourseState(5) ||
			Yard_HasConnectedPlayerInCourseState(6)) {
			int smeltingRoomNodeSwitch;
			int tubeNodeSwitch;

			smeltingRoomNodeSwitch = g_serverTickTime / 64 % 4;
			g_objectTable[g_yardContext.smeltingRoomObjIdx].mobj->nodeSwitchIndex = smeltingRoomNodeSwitch;

			tubeNodeSwitch = g_serverTickTime / 36 % 4;
			g_objectTable[g_yardCentrifugeObjIdx].mobj->nodeSwitchIndex = tubeNodeSwitch;

			{
				int tubeObjIdx;
				int tubeLastObjIdx;
				unsigned int tubeObjIdxLimitCheck;
				ObjectRecord* objectTable;

				tubeObjIdx = g_yardAdvancedCourseTubeFirstObjIdx;
				tubeLastObjIdx = g_yardAdvancedCourseTubeLastObjIdx;
				tubeObjIdxLimitCheck = (unsigned int)tubeObjIdx;
				if ((unsigned int)tubeObjIdx <= (unsigned int)tubeLastObjIdx) {
					objectTable = g_objectTable;
					do {
						uint16_t objectType = objectTable[tubeObjIdx].objectType;
						if (objectType >= OBJ_SRTubeNOBend && objectType <= OBJ_SRTubeRH) {
							objectTable[tubeObjIdx].mobj->nodeSwitchIndex = tubeNodeSwitch;
							objectTable = g_objectTable;
						}
						++tubeObjIdxLimitCheck;
						++tubeObjIdx;
					} while (tubeObjIdxLimitCheck <= (unsigned int)g_yardAdvancedCourseTubeLastObjIdx);
				}
			}
		}
	}

	if (g_yardChallengeMode > 3u) {
		int i;
		Yard_UpdateRubbleChunkMotion(deltaTicks);
		Yard_UpdateCompactorCycle(deltaTicks);
		Yard_UpdateSmeltingJunkStates(deltaTicks);
		Yard_UpdatePeriodicRubbleSpawner(deltaTicks);

		for (i = 0; i < g_flightPlayerCount; ++i) {
			if (g_players[i].connectedFlag && g_yardContext.playerChallengeStates[i].courseState == 4) {
				Yard_UpdateSmeltingRoomTurrets(deltaTicks);
				break;
			}
		}

		if (g_yardChallengeMode != 6u) {
			Yard_UpdateCentrifugeMechanisms(deltaTicks);
			Yard_UpdateCentrifugeContainerStates(deltaTicks);
		}
	} else if (g_yardChallengeMode == 3u && g_yardContext.compactorCycleState != 2) {
		Yard_UpdateCompactorCycle(deltaTicks);
	}

	switch (g_yardChallengeMode) {
		case 0:
		case 1:
		case 2:
		case 4:
		case 5:
		case 6:
		case 7:
			Yard_UpdateSecondaryAccelRingLaunchers(deltaTicks);
			break;
	}

	switch (g_yardChallengeMode) {
		case 0:
		case 1:
		case 2:
		case 4:
		case 5:
		case 6:
		case 7:
			Yard_UpdateAccelRingLaunchers(deltaTicks);
			break;
	}

	Yard_UpdateChallengeProgressAndScoring(deltaTicks);
	return 0;
}
