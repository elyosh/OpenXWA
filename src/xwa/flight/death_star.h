#ifndef XWA_FLIGHT_DEATH_STAR_H
#define XWA_FLIGHT_DEATH_STAR_H

#include "xwa/assets/object_type.h"
#include "xwa/flight/object/object.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*DeathStarSegmentUpdateFn)(int16_t activeSegmentSlotIdx, int16_t currentSegmentSlotIdx);

typedef union DeathStarSegmentIndex {
	uint16_t value;
	uint32_t paddedValue;
} DeathStarSegmentIndex;

#pragma pack(push, 1)
typedef struct DeathStarChildObjectRef {
	uint16_t objectType;
	uint16_t angleByteOffsets;
	uint16_t objectIdx;
	uint16_t objectSignature;
} DeathStarChildObjectRef;

typedef struct DeathStarSegmentDef {
	uint16_t                  objectType;
	uint32_t                  flags;
	int                       worldX;
	int                       worldY;
	int                       worldZ;
	Q16Angle                  yaw;
	Q16Angle                  pitch;
	uint16_t                  nextSegmentSet;
	int16_t                   nextSegmentIdx;
	uint16_t                  activeSegmentCount;
	DeathStarSegmentUpdateFn  updateFn;
	DeathStarChildObjectRef   childObjects[10];
} DeathStarSegmentDef;

typedef struct DeathStarSegmentRule {
	int16_t                  segmentIdx;
	uint16_t                 objectType;
	uint32_t                 flags;
	Q16Angle                 yaw;
	Q16Angle                 pitch;
	uint16_t                 attachPointIdx;
	int16_t                  parentSegmentIdx;
	uint16_t                 parentPointKind;
	uint16_t                 activeSegmentCount;
	uint16_t                 redirectSegmentSet;
	int16_t                  redirectSegmentIdx;
	uint16_t                 nextSegmentSet;
	int16_t                  nextSegmentIdx;
	DeathStarSegmentUpdateFn updateFn;
} DeathStarSegmentRule;

typedef struct DeathStarSegmentSet {
	DeathStarSegmentDef*  segments;
	uint16_t              count;
	Q16Angle              baseYaw;
	Q16Angle              basePitch;
	uint16_t              fixedSegmentObjectType; /* ObjectTypeId */
	DeathStarSegmentRule* rules;
} DeathStarSegmentSet;

typedef struct DeathStarSegmentLocalPoint {
	int side;
	int up;
	int forward;
} DeathStarSegmentLocalPoint;

typedef struct DeathStarObjectPointTable {
	uint16_t                   attachPointCount;
	DeathStarSegmentLocalPoint attachPoints[5];
	uint16_t                   spawnPointCount;
	DeathStarSegmentLocalPoint spawnPoints[10];
} DeathStarObjectPointTable;

typedef struct DeathStarFollowChainSlot {
	int      objectIdx;
	uint16_t objectSignature;
	uint16_t reserved06;
	int      pathDistance;
	int      desiredSpacing;
	int      refreshTimer;
} DeathStarFollowChainSlot;

typedef struct DeathStarLaserEffectSlot {
	int      objectIdx;
	uint16_t objectSignature;
} DeathStarLaserEffectSlot;

typedef struct DeathStarPathSample {
	int      worldX;
	int      worldY;
	int      worldZ;
	Q16Angle yaw;
	Q16Angle pitch;
	Q16Angle roll;
	uint16_t tacticalIndex;
	int      elapsedTicksSincePrev;
} DeathStarPathSample;

typedef struct DeathStarPathHistory {
	int                 sampleWriteIdx;
	int                 sampleLastTime;
	DeathStarPathSample samples[30];
} DeathStarPathHistory;
#pragma pack(pop)

extern DeathStarSegmentSet g_deathStarSegmentSets[8];
extern DeathStarObjectPointTable g_deathStarObjectPointTables[53];
extern DeathStarFollowChainSlot g_deathStarFollowChainSlots[10];
extern DeathStarPathHistory g_deathStarPathHistory;
extern int g_deathStarFollowRefreshPending;
extern int g_deathStarFollowLeaderExtentX4;
extern int g_deathStarFollowLeaderObjectType;
extern int g_deathStarFollowChainLastValidateTime;
extern int g_deathStarTunnelFilmStateReserved0;
extern int g_deathStarTunnelTimer;
extern int g_deathStarPlayerObjIdx;
extern uint16_t g_deathStarLastGeneratedRandomSegmentType;
extern DeathStarSegmentIndex g_deathStarCurrentSegmentIdx;
extern int g_deathStarSegmentSetIdx;
extern uint16_t g_deathStarEntranceProximityArmed;
extern int      g_deathStarEntranceTransitionState;
extern int      g_deathStarEntranceTransitionTimer;
extern uint8_t g_deathStarReactorCoreRoomFgIdx;
extern uint8_t g_deathStarTankPipeBlueFgIdx;
extern uint8_t g_deathStarDefaultScriptedObjectFgIdx;
extern uint8_t g_deathStarGeneratedObjectFgIdx;
extern int     g_deathStarTripodGunFgIdx;
extern uint8_t g_deathStarFocusChamberFgIdx;
extern uint8_t g_deathStarTankLightsFgIdx;
extern int     g_deathStarBentTubeGrayFgIdx;
extern uint8_t g_deathStarBentTubeRedFgIdx;
extern uint8_t g_deathStarTankPipeRedFgIdx;
extern uint8_t g_deathStarFocusLensFgIdx;
extern int     g_deathStarBentTubeBlueFgIdx;
extern uint8_t g_deathStarSegmentChildInitialHitCount;
extern uint8_t g_deathStarRandomChildObjectLimit;
extern uint8_t g_deathStarActiveSegmentPlaceholderFgIdx;
extern uint16_t g_deathStarActiveSegmentCount;
extern uint16_t g_deathStarActiveSegmentIdx[10];
extern int      g_deathStarActiveSegmentObjIdx[10];
extern uint16_t g_deathStarLaserChamberSegmentIdx;
extern int     g_deathStarLaserChamberSegmentSetIdx;
extern int     g_deathStarLaserFireTimer;
extern int     g_deathStarLaserCooldownTimer;
extern int     g_deathStarLaserChamberX;
extern int     g_deathStarLaserChamberY;
extern int     g_deathStarLaserChamberZ;
extern int     g_deathStarLaserChamberDirX;
extern int     g_deathStarLaserChamberDirY;
extern int     g_deathStarLaserChamberDirZ;
extern int     g_deathStarLaserEffectSlotCount;
extern DeathStarLaserEffectSlot g_deathStarLaserEffectSlots[10];
extern int     g_deathStarLaserPowerSourceObjIdx;
extern int     g_deathStarPowerSourceLinkedFocusLensObjIdx;
extern unsigned int g_deathStarLaserGlowExtent;
extern int     g_deathStarFollowLeaderObjIdx;
extern int     g_deathStarFollowBaseDesiredSpacing;
extern int     g_deathStarAccelChamberLightTimer;
extern int     g_deathStarAccelChamberContainerSpawnInterval;
extern int     g_deathStarAccelChamberLastContainerSpawnTime;
extern uint8_t g_deathStarAccelChamberContainersCleared;
extern Q16Angle g_deathStarAccelChamberPitchOffset;
extern int     g_deathStarContainerCollisionLightTimer;
extern int     g_deathStarFocusChamberObjIdx;
extern DeathStarSegmentIndex g_deathStarFocusChamberSegmentIdx;
extern int     g_deathStarTripodGunObjIdx;
extern int     g_deathStarTripodGunsActivated;
extern int     g_deathStarReactorCoreRoomObjIdx;
extern int     g_deathStarReactorCoreFgIdx;
extern float   g_deathStarReactorCoreDriftDirZ;
extern float   g_deathStarReactorCoreDriftDirX;
extern float   g_deathStarReactorCoreDriftDirY;
extern int     g_deathStarReactorExplosionOriginZ;
extern int     g_deathStarReactorExplosionOriginY;
extern int     g_deathStarReactorExplosionOriginX;
extern float   g_deathStarReactorShockwaveDistance;
extern int     g_deathStarReactorShockwaveSpeed;
extern DeathStarSegmentIndex g_deathStarReactorCoreRoomSegmentIdx;
extern int      g_deathStarReactorCoreObjIdx;
extern int     g_deathStarReactorCylinderObjIdx;
extern int     g_deathStarReactorDestructionTimer;
extern int     g_deathStarReactorReservedFilmState;
extern int     g_deathStarReactorCylinderAnimTimer;
extern int     g_deathStarReactorExplosionSpawnCount;
extern uint16_t g_deathStarReactorCoreDriftSpeed;
extern int     g_deathStarReactorShockwaveObjIdx[5];
extern int     g_deathStarReactorAssaultCraftObjIdx;
extern uint8_t g_deathStarReactorAssaultFgIdx;
extern int     g_deathStarLoopSfxVolume[2];
extern uint8_t g_deathStarLoopSfxActive[2];

DeathStarSegmentDef* DeathStar_ResolveSegmentRedirect(const DeathStarSegmentRule* rule,
													  DeathStarSegmentDef* outDef);
void DeathStar_ApplySegmentOrientationFlags(DeathStarSegmentDef* segment,
											const DeathStarSegmentDef* relativeTo);
int  DeathStar_AreSegmentSetStartDistancesOrdered(void);
void DeathStar_PopulateRandomSegmentChildren(int16_t segmentSetIdx,
											 DeathStarSegmentDef* segment);
int  DeathStar_BuildSegmentSets(void);
void DeathStar_SelectLaserTarget(int preferredTargetObjIdx);
void DeathStar_FireLaserAtTarget(void);
void DeathStar_Init(void);
void DeathStar_InitMobileObjectForType(uint16_t objectType, unsigned int objIdx);
void DeathStar_SpawnScriptedObjects(void);
void DeathStar_ComputeSegmentPointOffset(const DeathStarSegmentDef* segmentDef, uint16_t pointIdx,
										 int pointKind, int* outX, int* outY, int* outZ);
void DeathStar_RebuildSegmentChildObjects(uint16_t* oldActiveSegmentIdx,
										  uint16_t oldSegmentSetIdx,
										  uint16_t oldActiveSegmentCount);
void DeathStar_SpawnSegmentChildObject(DeathStarChildObjectRef* childList, uint16_t childIdx,
									  DeathStarSegmentDef* parentSegment);
void DeathStar_LoadActiveSegments(void);
void DeathStar_SpawnLaserPowerSourceObject(void);
unsigned int DeathStarTunnel_SpawnExplosionEffectObject(ObjectTypeId explosionObjectType,
														unsigned int instanceExtent);
void DeathStar_SetOrderWaypointFromSegmentPoint(int objectIdx, uint16_t segmentPointIdx,
												int pointKind, uint8_t flightGroupIdx,
												int orderSlot, int waypointSlot);
void DeathStar_InitReactorAssaultState(void);
void DeathStar_InitZeroGStormtrooperWaypoints(void);
void DeathStar_PositionPlayerAndFollowersAtStart(void);
void DeathStar_ResizeActiveSegmentSlots(uint16_t activeSegmentCount);
void DeathStar_UpdateActiveSegmentWindow(int16_t centerSegmentIdx);
void DeathStar_InitLaserChamber(void);
uint16_t DeathStar_SpawnLaserGlowSegment(void);
uint16_t DeathStar_SpawnLaserInternalSegment(void);
void DeathStar_InitFollowOverrideState(void);
void DeathStar_InitAccelChamberState(void);
void DeathStar_UpdateFollowChainSlot(int followSlotIdx);
void DeathStar_RefreshFollowOverrideCandidates(void);
void DeathStar_RemoveFollowChainSlot(int followSlotIdx);
char DeathStar_InterpolateFollowCraftOnPath(int objectIdx, int sampleIdxA, int sampleIdxB,
											unsigned int ticksIntoSample, DeathStarFollowChainSlot* slot);
void DeathStar_AddFollowChainSlot(int objectIdx);
void DeathStar_ApplyFollowSeparationOffset(int objectIdx, int followSlotIdx);
void DeathStar_UpdateFollowOverrideCraft(int objectIdx);
int  DeathStar_HandlePowerNodeHit(unsigned int sourceObjIdx, unsigned int powerNodeObjIdx,
								  unsigned int hitComponentId);
void DeathStar_HandleReactorHit(unsigned int projectileObjIdx, unsigned int reactorObjIdx,
								unsigned int hitComponentId);
void DeathStarTunnel_UpdateReactorDestructionSequence(int16_t activeSegmentSlotIdx);
void DeathStarTunnel_Update(void);
int  DeathStar_SpawnReactorDebrisGirders(void);
void DeathStar_Shutdown(void);
void DeathStar_UpdateLoopingObjectSfxVolume(int channelIdx, unsigned int sourceObjIdx,
											int useDirectVolume);
void DeathStar_WriteFilmStateBlock(void);
void DeathStar_ReadFilmStateBlock(void);

#ifdef __cplusplus
}
#endif

#endif
