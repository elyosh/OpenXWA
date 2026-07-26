#ifndef XWA_FLIGHT_MISSION_MISSION_H
#define XWA_FLIGHT_MISSION_MISSION_H

#include "xwa/frontend/frontend_mission.h"
#include "xwa/math/vec3i.h"
#include "xwa/flight/object/object.h"
#include "xwa/util/memory.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
typedef struct XwaMissionFlightGroup {
	XwaFlightGroup fg;
	int playerOwnerIdx;
} XwaMissionFlightGroup;
#pragma pack(pop)
#else
typedef struct __attribute__((packed)) XwaMissionFlightGroup {
	XwaFlightGroup fg;
	int playerOwnerIdx;
} XwaMissionFlightGroup;
#endif

typedef char xwa_mission_flight_group_size[(sizeof(XwaMissionFlightGroup) == 0xE42) ? 1 : -1];

typedef struct MissionClock {
	uint8_t reserved[3]; // Unused by runtime code; retained for the original eight-byte layout.
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	int16_t subsecondTicks;
} MissionClock;

typedef char mission_clock_size[(sizeof(MissionClock) == 8) ? 1 : -1];

#if defined(_MSC_VER)
#pragma pack(push, 1)
typedef struct MissionRegionHyperPointTables {
	Vec3i arrivalPoint[5][5];
	uint8_t arrivalPointValid[5][5];
	Vec3i departureRoutePoint[5][5];
	uint8_t departureRoutePointValid[5][5];
} MissionRegionHyperPointTables;
#pragma pack(pop)
#else
typedef struct __attribute__((packed)) MissionRegionHyperPointTables {
	Vec3i arrivalPoint[5][5];
	uint8_t arrivalPointValid[5][5];
	Vec3i departureRoutePoint[5][5];
	uint8_t departureRoutePointValid[5][5];
} MissionRegionHyperPointTables;
#endif

typedef char mission_region_hyper_point_tables_size[(sizeof(MissionRegionHyperPointTables) == 650) ? 1 : -1];

typedef enum MissionTriggerVariableType {
	TRIGVAR_NONE = 0,
	TRIGVAR_FLIGHT_GROUP = 1,
	TRIGVAR_SHIP_TYPE = 2,
	TRIGVAR_SHIP_CLASS = 3,
	TRIGVAR_OBJECT_TYPE = 4,
	TRIGVAR_IFF = 5,
	TRIGVAR_SHIP_ORDERS = 6,
	TRIGVAR_CRAFT_WHEN = 7,
	TRIGVAR_GLOBAL_GROUP = 8,
	TRIGVAR_AI_LEVEL = 9,
	TRIGVAR_STATUS = 10,
	TRIGVAR_ALL_CRAFT = 11,
	TRIGVAR_TEAM = 12,
	TRIGVAR_PLAYER_NUM = 13,
	TRIGVAR_BEFORE_TIME = 14,
	TRIGVAR_NOT_FLIGHT_GROUP = 15,
	TRIGVAR_NOT_SHIP_TYPE = 16,
	TRIGVAR_NOT_SHIP_CLASS = 17,
	TRIGVAR_NOT_OBJECT_TYPE = 18,
	TRIGVAR_NOT_IFF = 19,
	TRIGVAR_NOT_GLOBAL_GROUP = 20,
	TRIGVAR_NOT_TEAM = 21,
	TRIGVAR_NOT_PLAYER_NUM = 22,
	TRIGVAR_GLOBAL_UNIT = 23,
	TRIGVAR_NOT_GLOBAL_UNIT = 24,
	TRIGVAR_GLOBAL_CARGO = 25,
	TRIGVAR_NOT_GLOBAL_CARGO = 26,
} MissionTriggerVariableType;

typedef struct MissionFgRuntimeStats {
	uint8_t hasArrived;
	uint8_t wavesRemaining;
	uint8_t arrivalDelayPending;
	uint8_t arrivalEnabled;
	int16_t arrivalDelayTimer;
	uint16_t currentMissionPointRef;
	uint16_t spawnedCraftCount;
	uint16_t outcomeCount[33];
	uint8_t specialCargoOutcome[33];
	uint8_t teamInspected[10];
	uint8_t teamSpecialCargoInspected[10];
	uint8_t teamUninspectedLost[10];
	uint8_t teamSpecialCargoUninspectedLost[10];
	uint8_t teamPartiallyInspected[10];
	uint8_t teamSpecialCargoPartiallyInspected[10];
	uint8_t teamPartialInspectLost[10];
	uint8_t teamSpecialCargoPartialInspectLost[10];
	uint8_t teamCondition44Count[10];
	uint8_t teamCondition44SpecialCargo[10];
	uint8_t teamCondition44OtherTeamCount[10];
	uint8_t teamCondition44OtherTeamSpecialCargo[10];
	uint8_t teamEventExtra[4][10];
	uint8_t goalState[80];
	uint8_t tailEventCounts[21];
} MissionFgRuntimeStats;

typedef enum TeamGoalKind {
	TEAM_GOAL_PRIMARY = 0,
	TEAM_GOAL_SECONDARY = 1,
	TEAM_GOAL_BONUS = 2,
	TEAM_GOAL_KIND_COUNT = 3,
} TeamGoalKind;

typedef enum TeamScoreKind {
	TEAM_SCORE_BONUS_TENTHS = 0,
	TEAM_SCORE_MISSION = 1,
	TEAM_SCORE_KIND_COUNT = 2,
} TeamScoreKind;

typedef enum TeamKillStatKind {
	TEAM_KILL_STAT_FULL = 0,
	TEAM_KILL_STAT_SHARED = 1,
	TEAM_KILL_STAT_ASSIST = 2,
	TEAM_KILL_STAT_LOSS = 3,
	TEAM_KILL_STAT_COUNT = 4,
} TeamKillStatKind;

typedef enum TeamFgCounterKind {
	TEAM_FG_COUNTER_INSPECTED = 0,
	TEAM_FG_COUNTER_TRANSFER = 1,
	TEAM_FG_COUNTER_COUNT = 2,
} TeamFgCounterKind;

typedef enum GoalTriggerCounterKind {
	GOAL_TRIGGER_COUNTER_CURRENT = 0,
	GOAL_TRIGGER_COUNTER_TOTAL = 1,
	GOAL_TRIGGER_COUNTER_COUNT = 2,
} GoalTriggerCounterKind;

typedef struct MissionFlightRuntimeState {
	int teamScores[TEAM_SCORE_KIND_COUNT][10];
	uint16_t teamKillStats[TEAM_KILL_STAT_COUNT][10];
	uint16_t teamFgCounters[TEAM_FG_COUNTER_COUNT][10][192];
	uint8_t teamFgDesignationCode[10][192];
	uint8_t globalPrimaryGoalStatus;
	uint16_t globalGoalStatusUnused;
	uint8_t globalBonusGoalStatus;
	uint8_t teamGlobalGoalState[10][TEAM_GOAL_KIND_COUNT];
	uint8_t teamGoalStatus[10][TEAM_GOAL_KIND_COUNT];
	uint16_t globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_COUNT][10][3][4];
	int teamMissionCompletionTimeSeconds[10];
	uint8_t teamHasCountableCraft[10];
	uint8_t teamActiveGoalSequence[10];
	uint8_t teamReinforcementCalled[10];
} MissionFlightRuntimeState;

extern XwaMissionHeader g_missionHeader;
extern XwaMissionFlightGroup g_missionFlightGroups[192];
extern XwaGlobalGoal g_missionGlobalGoals[10][7];
extern MissionFgRuntimeStats g_missionFgStats[192];
extern XwaTeam g_missionTeams[10];
extern XwaMessage g_missionMessages[64];
extern MissionFlightRuntimeState g_missionFlightRuntimeState;
extern int g_connectedPlayerCount;
extern int g_maxConnectedPlayerCountThisMission;
extern MissionClock g_missionElapsedClock;
extern MissionClock g_missionCountdownClock;
extern uint8_t g_missionTimeLimitActive;
extern uint8_t g_flightMissionEndPending;
extern uint8_t g_teamVictoryTimeLimitMinutes;
extern uint8_t g_teamVictoryTimeLimitStarted;
extern uint8_t g_missionStateByte8053E6;
extern uint16_t g_unusedMissionInitStateWord0;
extern uint16_t g_unusedMissionInitStateWord1;
extern uint16_t g_unusedMissionInitStateWord2;
extern uint16_t g_unusedMissionInitStateWord3;
extern uint16_t g_unusedMissionInitStateWord4;
extern uint16_t g_unusedMissionInitStateWord5;
extern int g_unusedMissionInitStateDword805406;
extern uint8_t g_missionRandomVariationEnabled;
extern uint8_t g_aiOpponentsEnabled;
extern uint8_t g_playerFlightGroupWaveMode;
extern int g_missionFormatVersion;
extern uint16_t g_missionConditionTotalCount;
extern uint16_t g_missionConditionCurrentCount;
extern uint8_t g_missionMessageTriggered[64];
extern int g_missionMessageDelayCountdown[64];
extern int g_preparedSpawnMissionX;
extern int g_preparedSpawnMissionY;
extern int g_preparedSpawnMissionZ;
extern uint16_t g_preparedSpawnYawByte;
extern uint16_t g_preparedSpawnPitchByte;
extern uint16_t g_preparedSpawnRollByte;
extern uint8_t g_spawnTeamId;
extern uint8_t g_spawnIff;
extern uint8_t g_spawnFormationSpacing;
extern int g_spawnLeaderObjIdx;
extern uint8_t g_spawnRegionIdx;
extern uint8_t g_spawnGroupAI;
extern Q16Angle g_spawnYaw;
extern uint8_t g_spawnOutOfHyperspaceFlag;
extern uint8_t g_spawnStatus1;
extern uint8_t g_spawnLinkedObjectFlag;
extern uint8_t g_spawnFromMothershipFlag;
extern uint8_t g_spawnUseExactPosition;
extern Q16Angle g_spawnPitch;
extern uint16_t g_spawnCraftOrdinal;
extern uint16_t g_spawnObjectType;
extern uint8_t g_spawnObjectKind;
extern uint8_t g_spawnStatus2;
extern int g_spawnWorldX;
extern int g_spawnWorldY;
extern int g_spawnWorldZ;
extern ModelGenusId g_spawnGenusId;
extern uint8_t g_spawnLastAssignedIff;
extern uint8_t g_spawnFormation;
extern uint8_t g_initialSpawnBindPlayerCraftSlots;
extern uint16_t g_currentFlightGroupIdx;
extern int16_t g_escapePodPilotFlightGroupIdx;
extern int g_missionGlobalUnitCraftCount[41];
extern int g_activeMissionRegionCount;
extern MissionRegionHyperPointTables g_missionRegionHyperPoints;
extern int worldlocx;
extern int worldlocy;
extern int worldlocz;
extern MemoryHandle g_missionFgOverrideStringHandles[192][8][3];
extern MemoryHandle g_missionOrderStringHandles[192][4][4];
extern MemoryHandle g_globalGoalOverrideStringHandles[10][7][4][3];
extern MemoryHandle g_objectTableHandle;
extern MemoryHandle g_mobileObjectPoolHandle;
extern MemoryHandle g_mobileObjectCharDataHandle;
extern MemoryHandle g_craftDataPoolHandle;
extern MemoryHandle g_warheadGuidancePoolHandle;
extern const uint8_t g_genusConvert[12];
extern const uint8_t g_familyConvert[4];

int Mission_DecodeOrderTime(uint8_t encodedTime);
int Mission_GameTimeToSeconds(uint8_t hours, uint8_t minutes, uint8_t seconds);
int Mission_GetElapsedClockSeconds(void);
uint16_t Mission_Init(char* fileName);
int Mission_LoadFile(char* fileName);
void Mission_UpdateLogic(void);
uint16_t Mission_FlightGroupMatchesTriggerVariable(uint16_t fgIdx, uint16_t variableType, uint16_t variable);
int Mission_ObjectMatchesTriggerVariable(uint16_t objectIdx, uint16_t variableType, uint16_t variable);
int Mission_ObjectMatchesTriggerVariableEx(uint16_t objectIdx, uint16_t variableType, uint16_t variable,
											uint16_t contextFgIdx);
int Mission_EvaluateCondition(const XwaTrigger* trigger, uint8_t includeDepartedAsDestroyed,
									  uint16_t teamOrVariable);
int Mission_EvaluateTriggerPair(const XwaTriggerPair* triggerPair, char includePending);
void Mission_SetActiveRegionObjectRanges(int regionIdx);
int Team_IsHostileToTeam(int otherTeamIdx, int teamIdx);
int Team_IsFriendlyToTeam(int otherTeamIdx, int teamIdx);
int Mission_SyncPilotNetworkPlayersToSessionSlots(void);
void Mission_RecordPlayerCraftLossAttribution(int attackerPlayerIdx, int victimObjIdx, int contributionTier);
void Mission_CreditDestructionDamageContributors(uint16_t sourceObjIdx, uint16_t victimObjIdx);
void Mission_CreditPlayerKillContribution(uint16_t victimObjIdx, int specialCargoFlag, int contributionTier,
										  unsigned int playerIdx, int creditedOwnerIdx, int victimRating);
int Mission_CreditTeamKillContribution(uint16_t victimObjIdx, int specialCargoFlag, int contributionTier,
									   int teamIdx);
int Mission_RecordPlayerCraftLoss(unsigned int objIdx, int allowPendingDamageCredit);
void Mission_RecordProjectileHitStats(unsigned int projectileObjIdx, char awardWarheadPoints);
void Mission_CloseUnavailableFlightGroupAccounting(unsigned int flightGroupIdx);
void Mission_RecordCraftOutcome(uint16_t objIdx, uint16_t flightGroupIdx, uint16_t outcomeId);
void Mission_ApplyTeamGoalScoreAllEnabledTeams(int16_t eventCondition, uint16_t flightGroupIdx,
											   int specialCargoFlag);
void Mission_ApplyTeamGoalScoreForTeam(int eventCondition, uint16_t flightGroupIdx, int specialCargoFlag,
									   uint8_t teamIdx);
int Mission_ApplyFlightGroupGoalScore(int16_t eventCondition, uint16_t flightGroupIdx, int playerIdx,
									  uint16_t scoreDivisor, int specialCargoFlag, int teamIdx);
int Mission_ComputeCraftLoadoutPointValue(int craftType, int warheadType, int beamType,
										  int countermeasureType);
unsigned int Mission_ComputeCraftPointValue(int objIdx);
char Mission_ShouldApplyEndMissionPenalty(unsigned int playerIdx);
ObjectIndex Mission_InitFlightGroupObjectSlot(uint16_t objectTypeOverride, uint16_t existingObjIdx);
uint16_t Mission_SpawnPreparedObject(uint16_t flightGroupIdx, uint16_t genusId, ObjectTypeId objectType);
void Mission_SpawnFlightGroupStaticObjects(uint16_t instanceFilter);
short Mission_SpawnFlightGroupWaveCraft(uint16_t instanceFilter);
void Mission_ProcessFlightGroupWaveCompletion(uint16_t flightGroupIdx);
void Mission_UpdateFlightGroupArrivals(void);
void Mission_InitFlightRuntimeState(void);
void Mission_CreateRegionMarkerObjects(int worldX, int worldY, int worldZ);
int Mission_FreeOverrideStringHandles(void);
void Mission_FreeObjectStorageHandles(void);
void Mission_ResolveObjectOrMissionPointWorldLoc(unsigned int objOrMissionPointRef, int flightGroupIdx,
												 int orderIdx, int waypointSetIdx);
void Mission_ResolveFormationSlotWorldLoc(uint16_t flightGroupIdx, uint16_t formationSlotIdx,
										  uint16_t basisObjIdx);
short Mission_StartFlightGroupArrival(uint16_t instanceFilter);
int Mission_ApplyStringLocalization(char* fileName);

#ifdef __cplusplus
}
#endif

#endif
