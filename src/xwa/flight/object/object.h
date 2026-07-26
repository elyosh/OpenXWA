#ifndef XWA_FLIGHT_OBJECT_OBJECT_H
#define XWA_FLIGHT_OBJECT_OBJECT_H

#include "xwa/assets/object_type.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t Q16Angle;
typedef uint16_t ModelIndex;
typedef uint8_t  ModelGenusId;
typedef int16_t  ObjectIndex;

typedef struct ObjectSlotRange {
	uint32_t next;
	uint32_t end;
} ObjectSlotRange;

enum {
	GENUS_Fighter                = 0,
	GENUS_Transport              = 1,
	GENUS_Utility                = 2,
	GENUS_Freighter              = 3,
	GENUS_Starship               = 4,
	GENUS_Platform               = 5,
	GENUS_PlayerProjectile       = 6,
	GENUS_NpcProjectile          = 7,
	GENUS_Mine                   = 8,
	GENUS_SatelliteBuoy          = 9,
	GENUS_Asteroid               = 10,
	GENUS_Debris                 = 11,
	GENUS_TextureSprite          = 12,
	GENUS_Explosion              = 13,
	GENUS_LargeScenery           = 14,
	GENUS_DeathStarTunnelSegment = 15,
	GENUS_Container              = 17,
	GENUS_PilotDroid             = 18,
	GENUS_WeaponEmplacement      = 19,
	GENUS_Rubble                 = 20,
	GENUS_SalvageJunk            = 21,
};

typedef struct ObjectRecord ObjectRecord;

typedef struct MobileObjectProximityList {
	uint8_t  count;
	int      score[16];
	uint16_t objIdx[16];
	int      overflowScore;
} MobileObjectProximityList;

#pragma pack(push, 1)
typedef struct AiOrderScratch {
	uint8_t completionState[5][4];
	uint8_t goalProgress[5][4];
} AiOrderScratch;

typedef struct AiController {
	char           currentOrderSlot;
	AiOrderScratch orderScratch;
	uint8_t        orderStateFlag;
	uint8_t        pendingPlanId;
	uint8_t        currentPlanId;
	char           waypointIndex;
	char           savedPlanId;
	int            thinkInterval;
	int            thinkTimer;
	int16_t        savedRandSeed;
	uint16_t       targetObjIdx;
	uint16_t       targetSignature;
	uint16_t       targetComponent;
	char           hasLiveTarget;
	int            aimPointX;
	int            aimPointY;
	int            aimPointZ;
	int            maneuverDist;
	int            orbitRadius;
	uint16_t       candidateTargetIdx;
	char           escortTargetFG;
	uint16_t       targetZAngle;
	uint16_t       targetRoll;
	uint16_t       targetXYAngle;
	uint8_t        maneuverMode;
	uint8_t        maneuverPhase;
	int            maneuverTimer;
	int            aiPlanState;
} AiController;

typedef struct AiFlightState {
	uint16_t threatObjIdx;
	uint16_t impactObjIdx;
	uint8_t  goHomeFlag;
	uint8_t  missionAbortedFlag;
	uint8_t  departTimerFlag;
	uint8_t  departClockHours;
	uint8_t  departClockMin;
	uint8_t  departClockSec;
	uint8_t  maneuverCounter;
	uint8_t  reactionTimer;
	uint8_t  reserved0C;
	uint8_t  orderActionCounter;
	uint8_t  orderActionFlag;
	uint8_t  objSignatureCount;
	uint16_t objSignatures[10];
	int16_t  maxSpeedCache;
	int16_t  motionScale;
	char     climbState;
	char     diveState;
	int16_t  pitchRate;
	int16_t  pitchAccel;
	char     headingState;
	char     headingForce;
	uint16_t headingStep;
	int16_t  rollRate;
	uint16_t rollAccel;
	char     enterFlag;
	uint16_t rollStep;
	int16_t  turnRate;
	int16_t  turnAccel;
	char     turnState;
	uint16_t turnStep;
	char     formationType;
	char     separation;
} AiFlightState;

typedef struct WarheadInventoryEntry {
	uint16_t projectileTypeId;
	uint8_t  weaponType;
	uint8_t  weaponGroupIdx;
	uint8_t  laserCharge;
	uint8_t  count;
	int16_t  turretRotBucket;
	uint8_t  lastFireMeshIdx;
	uint8_t  lastFireHardpointIdx;
	int16_t  turretTargetObjIdx;
	int16_t  turretRetargetCooldownTimer;
} WarheadInventoryEntry;

typedef struct TurretAimState {
	uint16_t effectiveAiObjectSignature;
	Q16Angle aimAngleA[2];
	Q16Angle aimAngleB[2];
	float    aimAccumA[2];
	float    aimAccumB[2];
} TurretAimState;

typedef struct WarheadGuidanceState {
	int8_t   sourcePlayerIdx;
	uint8_t  homingTier;
	uint16_t targetComponentIdx;
	uint16_t targetObjIdx;
	uint16_t targetSignature;
	uint16_t minSpeed;
} WarheadGuidanceState;

typedef struct MobileObjectCharData {
	uint16_t     skillValue;
	uint8_t      reserved02[2];
	AiController aiController;
	uint8_t      reserved6A[8];
} MobileObjectCharData;

typedef struct CraftData {
	int32_t               craftIndexInGroup;
	ModelIndex            modelIndex;
	int                   leader_obj_idx;
	uint8_t               aiLinkResolving;
	uint8_t               objectKind;
	uint8_t               missionAccountingDone;
	uint16_t              skillValue;
	int16_t               breakupPitchRate;
	int16_t               breakupYawRate;
	unsigned int          beamEffectAccum[5];
	uint8_t               sFoilState;
	AiController          aiController;
	uint16_t              carriedObjectIndex;
	uint16_t              carrierObjIdx;
	uint16_t              lastReleasedObjectIdx;
	uint16_t              releaseClearTimer;
	uint16_t              linkedPrevObjectIdx;
	uint16_t              nextLinkObjectIdx;
	uint16_t              linkSequenceIndex;
	uint16_t              lastAttackerObjIdx;
	uint16_t              lastHitTimestamp;
	AiFlightState         aiFlight;
	uint8_t               waveNumber;
	uint8_t               followFormationSlot;
	int32_t               pushAccumX;
	int32_t               pushAccumY;
	int32_t               pushAccumZ;
	uint16_t              throttleSpeed;
	uint16_t              engineOutputScale;
	int16_t               commandedSpeed;
	uint16_t              slamActive;
	int                   hullDamage;
	int32_t               systemDamageHullThreshold;
	int                   hullMax;
	uint16_t              subsystemDamage;
	int32_t               lastSystemHitTime;
	uint8_t               systemHitFlag;
	int32_t               damageReceivedTotal;
	int32_t               damageReceivedByPlayerOwnedCraft;
	int32_t               damageFromCollision;
	int32_t               damageFromStarship;
	int32_t               damageFromMine;
	int32_t               damageFromFlightGroupAmount[8];
	char                  damageFromFlightGroupIdx[8];
	int32_t               damageFromPlayer[8];
	int32_t               damageFromAiSkill[6];
	uint16_t              installedHudFeatureMask;
	uint16_t              activeHudFeatureMask;
	uint16_t              systemFlags;
	uint16_t              workingSubsystems;
	uint16_t              weaponFireInhibitTimer;
	uint8_t               unusedMissionFlag189;
	uint8_t               notDisabledAccountingSuppress;
	char                  wasCaptured;
	char                  attackedByTeam[10];
	uint8_t               iffVisibility[10];
	uint8_t               cargoIndex;
	uint8_t               boardingState;
	int                   shieldFront;
	int                   shieldRear;
	uint8_t               shieldRedirect;
	uint8_t               shieldDistribMode;
	uint8_t               cannonClassCount;
	uint8_t               laserRedirect;
	uint8_t               laserSlotCount;
	uint8_t               laserConvergeLevel;
	uint16_t              laserProjectileTypeId[3];
	uint8_t               laserLinkMode[6];
	uint8_t               laserLinkNextSlot[3];
	uint16_t              laserFireCooldownTicks[3];
	unsigned int          laserLastFireTimestamp[3];
	uint8_t               warheadLauncherCount;
	uint16_t              warheadSlotTypeIds[2];
	uint8_t               warheadLauncherFlags[2];
	uint16_t              warheadLauncherCooldownTicks[2];
	uint16_t              warheadLockTicks;
	uint8_t               beamTypeId;
	uint8_t               beamLevel;
	uint16_t              beamPresent;
	uint8_t               beamActive;
	int16_t               beamTimer;
	int16_t               beamTargetObjIdx;
	uint8_t               cmTypeId;
	uint8_t               cmAmmoCount;
	uint16_t              chaffActiveTimer;
	uint16_t              cmFireCooldownTimer;
	uint16_t              laserShotsFiredCount;
	uint16_t              laserHitsScoredCount;
	uint16_t              ionShotsFiredCount;
	uint16_t              ionHitsScoredCount;
	uint8_t               warheadsFiredCount;
	uint8_t               warheadHitsScoredCount;
	uint8_t               systemDisplaySlotBySystem[11];
	uint16_t              systemHealth[11];
	uint16_t              systemTimer[11];
	uint8_t               componentState[50];
	uint8_t               meshRotation[50];
	uint8_t               componentHp[50];
	int16_t               playerCommandAvoidTargetObjIdx;
	uint8_t               followPlayerMode;
	uint8_t               followPlayerIdx;
	uint16_t              playerCommandCraftTypeFilter;
	uint8_t               playerCommandTeamFilter;
	uint8_t               savedCurrentPlan;
	uint8_t               savedPendingPlan;
	uint16_t              followTimer;
	uint8_t               engineEmitterHealth[16];
	WarheadInventoryEntry warheadData[16];
	TurretAimState        turretAim;
	char                  specialCargoName[20];
	uint8_t               reserved3ED[8];
	ObjectRecord*         effectiveAiObjectLink;
} CraftData;
#pragma pack(pop)

typedef char craft_data_ai_heading_state_offset
	[(offsetof(CraftData, aiFlight) + offsetof(AiFlightState, headingState) == 0xCE) ? 1 : -1];
typedef char craft_data_ai_enter_flag_offset
	[(offsetof(CraftData, aiFlight) + offsetof(AiFlightState, enterFlag) == 0xD6) ? 1 : -1];
typedef char craft_data_ai_turn_state_offset
	[(offsetof(CraftData, aiFlight) + offsetof(AiFlightState, turnState) == 0xDD) ? 1 : -1];
typedef char craft_data_throttle_speed_offset[(offsetof(CraftData, throttleSpeed) == 0xF0) ? 1 : -1];
typedef char
	craft_data_laser_projectile_type_offset[(offsetof(CraftData, laserProjectileTypeId) == 0x1B0) ? 1 : -1];
typedef char craft_data_laser_link_mode_offset[(offsetof(CraftData, laserLinkMode) == 0x1B6) ? 1 : -1];
typedef char
	craft_data_laser_link_next_slot_offset[(offsetof(CraftData, laserLinkNextSlot) == 0x1BC) ? 1 : -1];
typedef char
	craft_data_laser_fire_cooldown_offset[(offsetof(CraftData, laserFireCooldownTicks) == 0x1BF) ? 1 : -1];
typedef char
	craft_data_warhead_launcher_flags_offset[(offsetof(CraftData, warheadLauncherFlags) == 0x1D6) ? 1 : -1];
typedef char craft_data_warhead_launcher_cooldown_offset
	[(offsetof(CraftData, warheadLauncherCooldownTicks) == 0x1D8) ? 1 : -1];
typedef char craft_data_warhead_lock_ticks_offset[(offsetof(CraftData, warheadLockTicks) == 0x1DC) ? 1 : -1];
typedef char craft_data_component_state_offset[(offsetof(CraftData, componentState) == 0x22E) ? 1 : -1];
typedef char craft_data_mesh_rotation_offset[(offsetof(CraftData, meshRotation) == 0x260) ? 1 : -1];
typedef char craft_data_component_hp_offset[(offsetof(CraftData, componentHp) == 0x292) ? 1 : -1];

typedef struct MobileObject {
	uint8_t                   state;
	uint8_t                   motionFlags;
	int                       instanceExtent;
	int                       simStateTimestamp;
	int                       prevWorldX;
	int                       prevWorldY;
	int                       prevWorldZ;
	MobileObjectProximityList proximityList;
	int16_t                   rollImpulseRate;
	int16_t                   spinRate;
	uint16_t                  spinRateFrac;
	uint16_t                  spinDecelRate;
	int16_t                   spinAngleQ16;
	uint16_t                  speed;
	uint16_t                  speedRemainder;
	int                       damageAmount;
	unsigned int              lifetimeTimer;
	uint16_t                  framesAlive;
	int16_t                   sourceObjIdx;
	uint16_t                  sourceObjectType;
	int8_t                    iff;
	uint8_t                   team;
	uint8_t                   nodeSwitchIndex;
	uint16_t                  ejectionSpawnCount;
	int                       collisionObjIdx;
	uint8_t                   velocityOverrideActive;
	uint16_t                  velocityOverrideSpeed;
	uint16_t                  velocityOverrideElapsed;
	uint16_t                  velocityOverrideDuration;
	int16_t                   velocityOverrideDirX;
	int16_t                   velocityOverrideDirY;
	int16_t                   velocityOverrideDirZ;
	int16_t                   renderOffsetX;
	int16_t                   renderOffsetY;
	int16_t                   renderOffsetZ;
	float                     spinAxisX;
	float                     spinAxisY;
	float                     spinAxisZ;
	char                      moveVectorDirty;
	int16_t                   moveX;
	int16_t                   moveY;
	int16_t                   moveZ;
	uint8_t                   orientMatrixDirty;
	int16_t                   cachedFwdX;
	int16_t                   cachedFwdY;
	int16_t                   cachedFwdZ;
	int16_t                   cachedSideX;
	int16_t                   cachedSideY;
	int16_t                   cachedSideZ;
	int16_t                   cachedUpX;
	int16_t                   cachedUpY;
	int16_t                   cachedUpZ;
	WarheadGuidanceState*     pWarheadGuidance;
	CraftData*                pCraft;
	MobileObjectCharData*     pCharData;
} MobileObject;

struct ObjectRecord {
	uint16_t      objectSignature;
	uint16_t      objectType; /* ObjectTypeId values; stored as 16-bit in the original layout */
	ModelGenusId  genusId;
	uint8_t       flightGroupIdx;
	uint8_t       regionIdx;
	int           world_x;
	int           world_y;
	int           world_z;
	Q16Angle      yaw;
	Q16Angle      pitch;
	Q16Angle      roll;
	Q16Angle      angleD;
	uint16_t      typeSpecificWord;
	uint8_t       typeSpecificByte[2];
	int           playerOwnerIdx;
	MobileObject* mobj;
};

extern uint32_t              g_objScanStart;
extern uint32_t              g_localTransientSlotStart;
extern uint32_t              g_localTransientSlotEnd;
extern uint32_t              g_activeRegionObjectSlotStart;
extern uint32_t              g_activeRegionCraftObjectSlotEnd;
extern uint32_t              g_regionStaticObjectSlotEnd;
extern uint32_t              g_regionObjectSlotEnd;
extern uint32_t              g_objectSlotsPerRegion;
extern uint32_t              g_regionMainObjectSlotStart;
extern uint32_t              g_regionMainObjectSlotEnd;
extern uint32_t              g_regionMainObjectSlotsTotal;
extern uint32_t              g_regionStaticObjectSlotsTotal;
extern uint32_t              g_objectTableSlotCount;
extern uint32_t              g_mainObjectSlotsPerRegion;
extern uint32_t              g_craftObjectSlotsTotal;
extern uint32_t              g_craftObjectSlotsPerRegion;
extern uint32_t              g_projectileObjectSlotsTotal;
extern uint32_t              g_mobileObjectCharDataCount;
extern uint32_t              g_localDebrisObjectSlotsTotal;
extern uint32_t              g_localEffectObjectSlotsTotal;
extern uint32_t              g_projectileObjectSlotStart;
extern uint32_t              g_projectileObjectSlotEnd;
extern uint32_t              g_projectileObjectSlotsPerRegion;
extern uint32_t              g_sharedPlayerProjectileSlotsPerRegion;
extern uint32_t              g_playerProjectileSlotsTotal;
extern uint32_t              g_salvageJunkObjectSlotStart;
extern uint32_t              g_salvageJunkObjectSlotEnd;
extern uint32_t              g_salvageJunkObjectSlotsTotal;
extern uint32_t              g_salvageJunkObjectSlotsPerRegion;
extern uint32_t              g_debrisObjectSlotStart;
extern uint32_t              g_debrisObjectSlotEnd;
extern uint32_t              g_debrisObjectSlotsTotal;
extern uint32_t              g_explosionObjectSlotStart;
extern uint32_t              g_explosionObjectSlotEnd;
extern uint32_t              g_explosionObjectSlotsTotal;
extern uint32_t              g_localDebrisSlotEnd;
extern uint32_t              g_mobileObjectCharDataSlotStart;
extern uint32_t              g_mobileObjectCharDataSlotEnd;
extern uint32_t              g_localEffectSlotStart;
extern uint16_t              g_localDebrisRecycleSlotCursor;
extern ObjectSlotRange       g_objectSlotRangeByGenus[22];
extern ObjectRecord*         g_objectTable;
extern MobileObject*         g_mobileObjectPoolBase;
extern CraftData*            g_craftDataPoolBase;
extern WarheadGuidanceState* g_warheadGuidancePoolBase;
extern MobileObjectCharData* g_mobileObjectCharDataPool;
extern uint16_t              g_nextObjectSignature;

uint16_t      Object_FindFreeMissionSlot(void);
uint16_t      Object_AllocSlotForGenus(uint16_t genusId);
uint16_t      Object_AllocLocalTransientSlot(void);
void          Object_CopyStatePreservingStorage(unsigned int dstObjIdx, unsigned int srcObjIdx);
uint16_t      Object_SpawnDetachedComponent(uint16_t sourceObjIdx, uint8_t componentIdx);
int           FlightObject_SpawnEscapePodOrPilot(int sourceObjIdx);
uint16_t      Object_SpawnEffectFragment(uint16_t sourceObjIdx);
uint16_t      Object_SpawnLocalEffectFragment(uint16_t sourceObjIdx);
int           Object_SpawnExplosionSpriteCloud(ObjectRecord* sourceObj, int sourceGenus);
void          Object_ClearSlotState(uint32_t objIdx);
char          Object_HasActiveDecoyBeam(uint16_t objIdx);
void          Object_UpdateLifetimeAndMovement(void);
void          MobileObject_SetRandomSpinAxis(int objIdx);
ObjectRecord* Object_AddTrigMoveDeltaAndClampWorldPosition(ObjectRecord* obj);
int           Object_IsHostileToTeam(uint16_t objectIdx, int teamIdx);
int           Object_IsFriendlyToTeam(uint16_t objectIdx, int teamIdx);
unsigned int  Object_DirectionAndDistanceToMeshCenter(uint16_t fromObjIdx, uint16_t targetObjIdx,
													  unsigned int meshIdx);

#ifdef __cplusplus
}
#endif

#endif
