#ifndef XWA_FLIGHT_YARD_H
#define XWA_FLIGHT_YARD_H

#include "xwa/flight/object/object.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define XWA_YARD_PACKED_STRUCT
#else
#define XWA_YARD_PACKED_STRUCT __attribute__((packed))
#endif

typedef struct XWA_YARD_PACKED_STRUCT YardCraftScoreTable {
	char pilotNames[8][10][14];
	int  scores[8][10];
	int  objectType;
} YardCraftScoreTable;

typedef struct XWA_YARD_PACKED_STRUCT YardHighScoreTable {
	int                  count;
	YardCraftScoreTable** craftTables;
} YardHighScoreTable;

typedef struct YardPlayerChallengeState {
	int       objectIdx;
	int       courseState;
	int       score;
	int       currentCheckpointIdx;
	int       currentCourseSide;
	int       nextCheckpointIdx;
	int       nextCourseSide;
	int       ringCheckpointHit;
	int       chuteCheckpointHit;
	int       carriedObjectPickedUp;
	int       carriedObjectDelivered;
	int       finished;
	int       finishTimeSeconds;
	int       penaltyUntilSeconds;
	int       field_38;
	int       remainingCheckpointCount;
	int       field_40;
	int       lapsRemaining;
	int       recoveryCollisionObjIdx;
	int       recoveryWorldX;
	int       recoveryWorldY;
	int       recoveryWorldZ;
	Q16Angle  recoveryYaw;
	Q16Angle  recoveryPitch;
} YardPlayerChallengeState;

typedef struct YardCentrifugeMechanismState {
	int   state;
	int   cycleParity;
	float angularAccel;
	float angularVelocity;
	float meshRotationAccum;
	int   delayTicks;
} YardCentrifugeMechanismState;

typedef struct YardCourseCheckpointState {
	int      objectIdx;
	int      field04;
	int      checkpointWorldX;
	int      checkpointWorldY;
	int      checkpointWorldZ;
	int      nextObjectIdx;
	int      prevObjectIdx;
	int      targetObjIdx;
	int      targetDistance;
	uint16_t launcherIdx;
	uint16_t secondaryLauncherIdx;
	int      fireCooldownTicks;
} YardCourseCheckpointState;

typedef struct XWA_YARD_PACKED_STRUCT YardTrackedObjectState {
	int      objectIdx;
	int      state;
	uint16_t waypointIdx;
} YardTrackedObjectState;

typedef struct YardRubbleChunkState {
	int objectIdx;
	int state;
} YardRubbleChunkState;

typedef struct YardContext {
	YardPlayerChallengeState       playerChallengeStates[8];
	int                            compactorObjIdx;
	int                            compactorCycleState;
	int                            compactorPauseTimer;
	int                            compactorTickAccumulator;
	int                            smeltingRoomObjIdx;
	int                            smeltingRoomMode;
	int                            smeltingRoomTickAccumulator;
	int                            smeltingRoomTurretMeshIdx;
	YardCentrifugeMechanismState   centrifugeMechanisms[3];
	int                            centrifugeMechanismTickRemainder;
	YardRubbleChunkState           rubbleChunkStates[20];
	YardCourseCheckpointState      courseSide1Checkpoints[30];
	YardCourseCheckpointState      courseSide2Checkpoints[30];
	YardTrackedObjectState         smeltingJunkStates[10];
	YardTrackedObjectState         centrifugeContainerStates[20];
	int                            rubbleChunkStateCount;
	int                            smeltingJunkStateCount;
	int                            centrifugeContainerStateCount;
	int                            rubbleSpawnTickAccumulator;
	int                            countdownSecondsRemaining;
} YardContext;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
#undef XWA_YARD_PACKED_STRUCT

typedef char xwa_yard_craft_score_table_size[(sizeof(YardCraftScoreTable) == 0x5A4) ? 1 : -1];
typedef char xwa_yard_craft_score_table_object_type_offset
	[(offsetof(YardCraftScoreTable, objectType) == 0x5A0) ? 1 : -1];
typedef char xwa_yard_high_score_table_craft_tables_offset
	[(offsetof(YardHighScoreTable, craftTables) == 0x4) ? 1 : -1];
typedef char xwa_yard_tracked_object_state_size
	[(sizeof(YardTrackedObjectState) == 0x0A) ? 1 : -1];
typedef char xwa_yard_rubble_chunk_state_size
	[(sizeof(YardRubbleChunkState) == 0x08) ? 1 : -1];
typedef char xwa_yard_context_size[(sizeof(YardContext) == 0x0F7C) ? 1 : -1];
typedef char xwa_yard_context_course_side1_offset
	[(offsetof(YardContext, courseSide1Checkpoints) == 0x03EC) ? 1 : -1];
typedef char xwa_yard_context_course_side2_offset
	[(offsetof(YardContext, courseSide2Checkpoints) == 0x0914) ? 1 : -1];
typedef char xwa_yard_context_centrifuge_container_offset
	[(offsetof(YardContext, centrifugeContainerStates) == 0x0EA0) ? 1 : -1];
typedef char xwa_yard_context_centrifuge_container_count_offset
	[(offsetof(YardContext, centrifugeContainerStateCount) == 0x0F70) ? 1 : -1];
typedef char xwa_yard_context_tail_offset
	[(offsetof(YardContext, countdownSecondsRemaining) == 0x0F78) ? 1 : -1];

YardCraftScoreTable* Yard_AllocCraftScoreTable(void);
YardCraftScoreTable* Yard_FindCraftScoreTableByObjectType(ObjectTypeId objectType, YardHighScoreTable* table);
int Yard_InsertCraftHighScore(YardHighScoreTable* table, int categoryIdx, int objectType,
							  const char* pilotName, int score);
YardHighScoreTable* Yard_LoadHighScoreTable(void);
YardHighScoreTable* Yard_LoadOrCreateHighScoreTable(void);
int Yard_SaveHighScoreTable(YardHighScoreTable* table);
int Yard_FreeHighScoreTable(YardHighScoreTable* table);
void Yard_UpdateChallengeProgressAndScoring(int deltaTicks);
int Yard_SteerTrackedObjectTowardPoint(int objectIdx, int targetX, int targetY, int targetZ,
									   int deltaTicks);
int Yard_SpawnObjectAtWorldPos(ObjectTypeId objectType, int worldX, int worldY, int worldZ,
							   Q16Angle yaw, Q16Angle pitch);
int Yard_SpawnJunkBlockAtChamberDock(void);
void Yard_UpdateCompactorCycle(int deltaTicks);
int Yard_SpawnRubbleChunkAtWorldPos(int rubbleSlot, int worldX, int worldY, int worldZ);
void Yard_UpdatePeriodicRubbleSpawner(int deltaTicks);
int Yard_SpawnChildAtMount(ObjectTypeId childType, int parentObjIdx, int mountSelector, int yawArg,
						   int pitchArg, int angleMode);
void Yard_UpdateSmeltingJunkStates(int deltaTicks);
void Yard_UpdateCentrifugeContainerStates(int deltaTicks);
void Yard_UpdateCentrifugeMechanisms(int deltaTicks);
void Yard_UpdateRubbleChunkMotion(int deltaTicks);
void Yard_UpdateSmeltingRoomTurrets(int deltaTicks);
void Yard_UpdateAccelRingLaunchers(int deltaTicks);
void Yard_UpdateSecondaryAccelRingLaunchers(int deltaTicks);
int Yard_UpdateChallengeTick(int deltaTicks);
void Yard_InitChallengeScene(void);
int Yard_BuildAdvancedChallengeCourse(void);
void Yard_TargetCurrentObjective(unsigned int playerIdx);
void Yard_HandleR2D2CarrierLaserHit(unsigned int projectileObjIdx, unsigned int targetObjIdx);
void Yard_PickUpR2D2Objective(unsigned int playerIdx);
int Yard_SavePlayerRecoveryState(int objectIdx);
int Yard_IsObjectTypeVisibleForCurrentCourseState(ObjectTypeId objectType);
int Yard_ShouldRenderChallengeObject(unsigned int objectIdx);
int Yard_ShouldSuppressProximityPair(unsigned int ownerObjIdx, unsigned int candidateObjIdx);
int Yard_HandlePlayerChallengeObjectCollision(unsigned int playerObjIdx, unsigned int targetObjIdx,
											   uint16_t hitPartIdx);
int Yard_HandleChallengeObjectCollision(unsigned int sourceObjIdx, unsigned int targetObjIdx,
										 int16_t hitPartPlusOne);

extern YardContext g_yardContext;
extern int g_yardRubbleChunkSpawnLimit;
extern int g_yardFinishPlacementResultCode;
extern const uint8_t g_yardCheckpointsPerLapByChallengeMode[8];
extern int g_yardChallengeLapCount;
extern int g_yardStopCheatingMessageShown;
extern int g_yardFinishPlacementMessagePending;
extern int g_yardCompactorHintShown;
extern int g_yardWatchLasersHintShown;
extern int g_yardFinishMessageShown;
extern int g_yardStartMessageShown;
extern int g_yardAlmostDoneHintShown;
extern int g_yardLastCourseWarningGameTime;
extern int g_yardLastSafeCourseGameTime;
extern int g_yardDontStayLongHintShown;
extern int g_yardChallengeEventTimer;
extern int g_yardR2D2BumpSfxTimer;
extern int g_yardShuttleObjIdx;
extern int g_yardCourseSide1ToSide2GapDist;
extern int g_yardCourseSide2ToSide1GapDist;
extern int g_yardChuteMouthObjIdx;
extern int g_yardChuteTunnelEndObjIdx;
extern int g_yardCourseSide1FinalObjIdx;
extern int g_yardCourseSide2FinalObjIdx;
extern int g_yardSalvageRoomObjIdx;
extern int g_yardSmeltingRoomAsteroidObjIdx;
extern int g_yardCentrifugeObjIdx;
extern int g_yardCentrifugeMoltenBlockSpawnWorldZ[4];
extern int g_yardCentrifugeMoltenBlockSpawnWorldY[4];
extern int g_yardCentrifugeMoltenBlockSpawnWorldX[4];
extern int g_yardContainerGrandeObjIdx;
extern int g_yardAccelRingCullAnchorObjIdx;
extern int g_yardAdvancedCourseTubeFirstObjIdx;
extern int g_yardAdvancedCourseTubeLastObjIdx;
extern int g_yardR2D2ObjIdx;
extern int g_yardBuildParentObjIdx;
extern uint8_t g_yardSpawnFlightGroupIdx;

#ifdef __cplusplus
}
#endif

#endif
