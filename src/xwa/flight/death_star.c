#include "xwa/flight/death_star.h"

#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/audio/sound.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/object/damage.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_light.h"
#include "xwa/audio/fsfx.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/input/dinput.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/film.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/random.h"
#include "xwa/util/time.h"
#ifdef XWA_MODERN
#include "xwa_runtime/timing/modern_flight_timing.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XWA_MODERN
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
#else
static void OutputDebugStringA(const char* outputString) { DebugPrintf("%s", outputString); }
#endif

// GLOBAL: XWA 0x631CE8
DeathStarObjectPointTable g_deathStarObjectPointTables[53];
// GLOBAL: XWA 0x631B98
DeathStarFollowChainSlot g_deathStarFollowChainSlots[10];
// GLOBAL: XWA 0x631880
int g_deathStarPathSampleWriteIdx;
// GLOBAL: XWA 0x631884
int g_deathStarPathSampleLastTime;
// GLOBAL: XWA 0x631888
DeathStarPathSample g_deathStarPathSamples[30];
// GLOBAL: XWA 0x631B60
int g_deathStarFollowRefreshPending;
// GLOBAL: XWA 0x631C60
int g_deathStarFollowLeaderExtentX4;
// GLOBAL: XWA 0x634318
int g_deathStarFollowLeaderObjectType;
// GLOBAL: XWA 0x6343A0
int g_deathStarFollowChainLastValidateTime;
// GLOBAL: XWA 0x9E967C
int g_deathStarTunnelFilmStateReserved0;
// GLOBAL: XWA 0x631CA8
int g_deathStarTunnelTimer;
// GLOBAL: XWA 0x634370
int g_deathStarPlayerObjIdx;
// GLOBAL: XWA 0x63439C
uint16_t g_deathStarLastGeneratedRandomSegmentType;
// GLOBAL: XWA 0x631B6C
DeathStarSegmentIndex g_deathStarCurrentSegmentIdx;
// GLOBAL: XWA 0x6343B0
int g_deathStarSegmentSetIdx;
// GLOBAL: XWA 0x631B7C
uint16_t g_deathStarEntranceProximityArmed;
// GLOBAL: XWA 0x63437C
int g_deathStarEntranceTransitionState;
// GLOBAL: XWA 0x6343B8
int g_deathStarEntranceTransitionTimer;
// GLOBAL: XWA 0x631CB4
uint8_t g_deathStarReactorCoreRoomFgIdx;
// GLOBAL: XWA 0x631B80
uint8_t g_deathStarTankPipeBlueFgIdx;
// GLOBAL: XWA 0x6343AC
uint8_t g_deathStarDefaultScriptedObjectFgIdx;
// GLOBAL: XWA 0x6343C0
uint8_t g_deathStarGeneratedObjectFgIdx;
// GLOBAL: XWA 0x631B74
int g_deathStarTripodGunFgIdx;
// GLOBAL: XWA 0x6343B4
uint8_t g_deathStarFocusChamberFgIdx;
// GLOBAL: XWA 0x63436C
uint8_t g_deathStarTankLightsFgIdx;
// GLOBAL: XWA 0x63430C
int g_deathStarBentTubeGrayFgIdx;
// GLOBAL: XWA 0x634364
uint8_t g_deathStarBentTubeRedFgIdx;
// GLOBAL: XWA 0x634378
uint8_t g_deathStarTankPipeRedFgIdx;
// GLOBAL: XWA 0x631B5C
uint8_t g_deathStarFocusLensFgIdx;
// GLOBAL: XWA 0x6343C4
int g_deathStarBentTubeBlueFgIdx;
// GLOBAL: XWA 0x631B84
uint8_t g_deathStarSegmentChildInitialHitCount;
// GLOBAL: XWA 0x634338
uint8_t g_deathStarRandomChildObjectLimit;
// GLOBAL: XWA 0x631B68
uint8_t g_deathStarActiveSegmentPlaceholderFgIdx;
// GLOBAL: XWA 0x631CC0
uint16_t g_deathStarActiveSegmentCount;
// GLOBAL: XWA 0x634320
uint16_t g_deathStarActiveSegmentIdx[10];
// GLOBAL: XWA 0x631C70
int g_deathStarActiveSegmentObjIdx[10];
// GLOBAL: XWA 0x631B78
uint16_t g_deathStarLaserChamberSegmentIdx;
// GLOBAL: XWA 0x634314
int g_deathStarLaserChamberSegmentSetIdx;
// GLOBAL: XWA 0x631CC4
int g_deathStarLaserFireTimer;
// GLOBAL: XWA 0x634398
int g_deathStarLaserCooldownTimer;
// GLOBAL: XWA 0x631CD4
int g_deathStarLaserChamberX;
// GLOBAL: XWA 0x631CD0
int g_deathStarLaserChamberY;
// GLOBAL: XWA 0x631CCC
int g_deathStarLaserChamberZ;
// GLOBAL: XWA 0x634384
int g_deathStarLaserChamberDirX;
// GLOBAL: XWA 0x634380
int g_deathStarLaserChamberDirY;
// GLOBAL: XWA 0x634388
int g_deathStarLaserChamberDirZ;
// GLOBAL: XWA 0x634394
int g_deathStarLaserEffectSlotCount;
// GLOBAL: XWA 0x9E9640
DeathStarLaserEffectSlot g_deathStarLaserEffectSlots[10];
// GLOBAL: XWA 0x6343A4
int g_deathStarLaserPowerSourceObjIdx;
// GLOBAL: XWA 0x634348
int g_deathStarPowerSourceLinkedFocusLensObjIdx;
// GLOBAL: XWA 0x5B1670
unsigned int g_deathStarLaserGlowExtent;
// GLOBAL: XWA 0x631B70
int g_deathStarFollowLeaderObjIdx;
// GLOBAL: XWA 0x634390
int g_deathStarFollowBaseDesiredSpacing;
// GLOBAL: XWA 0x63187C
int g_deathStarAccelChamberLightTimer;
// GLOBAL: XWA 0x631B64
int g_deathStarAccelChamberContainerSpawnInterval;
// GLOBAL: XWA 0x631C68
int g_deathStarAccelChamberLastContainerSpawnTime;
// GLOBAL: XWA 0x634308
uint8_t g_deathStarAccelChamberContainersCleared;
// GLOBAL: XWA 0x6343BC
Q16Angle g_deathStarAccelChamberPitchOffset;
// GLOBAL: XWA 0x6343A8
int g_deathStarContainerCollisionLightTimer;
// GLOBAL: XWA 0x631C98
int g_deathStarFocusChamberObjIdx;
// GLOBAL: XWA 0x634310
DeathStarSegmentIndex g_deathStarFocusChamberSegmentIdx;
// GLOBAL: XWA 0x631C64
int g_deathStarTripodGunObjIdx;
// GLOBAL: XWA 0x634304
int g_deathStarTripodGunsActivated;
// GLOBAL: XWA 0x631CBC
int g_deathStarReactorCoreRoomObjIdx;
// GLOBAL: XWA 0x631B58
int g_deathStarReactorCoreFgIdx;
// GLOBAL: XWA 0x631B88
float g_deathStarReactorCoreDriftDirZ;
// GLOBAL: XWA 0x631B8C
float g_deathStarReactorCoreDriftDirX;
// GLOBAL: XWA 0x631B90
float g_deathStarReactorCoreDriftDirY;
// GLOBAL: XWA 0x631C9C
int g_deathStarReactorExplosionOriginZ;
// GLOBAL: XWA 0x631CA0
int g_deathStarReactorExplosionOriginY;
// GLOBAL: XWA 0x631CA4
int g_deathStarReactorExplosionOriginX;
// GLOBAL: XWA 0x631CAC
float g_deathStarReactorShockwaveDistance;
// GLOBAL: XWA 0x631CB0
int g_deathStarReactorShockwaveSpeed;
// GLOBAL: XWA 0x631CB8
DeathStarSegmentIndex g_deathStarReactorCoreRoomSegmentIdx;
// GLOBAL: XWA 0x631CC8
int g_deathStarReactorCoreObjIdx;
// GLOBAL: XWA 0x634368
int g_deathStarReactorCylinderObjIdx;
// GLOBAL: XWA 0x631CDC
int g_deathStarReactorDestructionTimer;
// GLOBAL: XWA 0x631CE0
int g_deathStarReactorReservedFilmState;
// GLOBAL: XWA 0x631CE4
int g_deathStarReactorCylinderAnimTimer;
#ifdef XWA_MODERN
static int g_modernDeathStarReactorRandomDriftX;
static int g_modernDeathStarReactorRandomDriftY;
static int g_modernDeathStarReactorRandomDriftZ;
static int g_modernDeathStarReactorRandomDriftRemainderX;
static int g_modernDeathStarReactorRandomDriftRemainderY;
static int g_modernDeathStarReactorRandomDriftRemainderZ;
static int g_modernDeathStarReactorCylinderYawRemainder;
static int g_modernDeathStarReactorCoreDriftSpeedRemainder;
static int g_modernDeathStarReactorCorePitchRemainder;
static double g_modernDeathStarReactorCoreDirectedDriftRemainderX;
static double g_modernDeathStarReactorCoreDirectedDriftRemainderY;
static double g_modernDeathStarReactorCoreDirectedDriftRemainderZ;
static uint16_t g_modernDeathStarShockwaveLightValue;

static int DeathStar_ModernScaleWithRemainder(int value, int elapsedTicks, int divisor, int* remainder) {
	int numerator;
	int scaled;

	numerator = value * elapsedTicks + *remainder;
	scaled = numerator / divisor;
	*remainder = numerator % divisor;
	return scaled;
}
#endif
// GLOBAL: XWA 0x634300
int g_deathStarReactorExplosionSpawnCount;
// GLOBAL: XWA 0x634334
uint16_t g_deathStarReactorCoreDriftSpeed;
// GLOBAL: XWA 0x634350
int g_deathStarReactorShockwaveObjIdx[5];
// GLOBAL: XWA 0x634374
int g_deathStarReactorAssaultCraftObjIdx;
// GLOBAL: XWA 0x63438C
uint8_t g_deathStarReactorAssaultFgIdx;
// GLOBAL: XWA 0x634340
int g_deathStarLoopSfxVolume[2];
// GLOBAL: XWA 0x631878
uint8_t g_deathStarLoopSfxActive[2];

#define DEATH_STAR_SEGMENT_POINT_KIND_SPAWN 23
#define DEATH_STAR_SEGMENT_POINT_KIND_ATTACH 24
#define DEATH_STAR_SEGMENT_FLAG_REDIRECT_CHILDREN 0x200u
#define DEATH_STAR_CRAFT_KIND_NORMAL 0
#define DEATH_STAR_CRAFT_KIND_ACCEL_CHAMBER_PULL 8

#if defined(__GNUC__) || defined(__clang__)
#define DEATH_STAR_ALIGN_POINTER_TABLE __attribute__((aligned(sizeof(void*))))
#else
#define DEATH_STAR_ALIGN_POINTER_TABLE
#endif

// GLOBAL: XWA 0x5B1668
int g_deathStarLoopSfxSlotByChannel[2] = { 165, 164 };

static const uint8_t g_deathStarLaserTargetGenusDispatch[GENUS_WeaponEmplacement + 1] = {
	0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
};

// GLOBAL: XWA 0x5A93B8
const float g_deathStarRicochetDamageDifficultyScale = 0.40000001f;
// GLOBAL: XWA 0x5A93BC
const float g_deathStarRicochetDamageDifficultyBias = -0.30000001f;

// GLOBAL: XWA 0x5A93E0
const float g_deathStarTripodGunActivationDistanceScale = 0.22220001f;

// GLOBAL: XWA 0x5B2668
const float g_deathStarSegmentDepthProjScale[28] = {
	2048.0f, 512.0f, 138.0f, 2048.0f, 138.0f, 138.0f, 138.0f, 138.0f, 138.0f, 138.0f,
	138.0f,  138.0f, 138.0f, 138.0f,  138.0f, 138.0f, 138.0f, 138.0f, 138.0f, 138.0f,
	138.0f,  138.0f, 138.0f, 138.0f,  138.0f, 138.0f, 138.0f, 138.0f,
};

typedef struct DeathStarSegmentSetInit {
	uint16_t count;
	Q16Angle baseYaw;
	Q16Angle basePitch;
	ObjectTypeId fixedSegmentObjectType;
	DeathStarSegmentRule* rules;
} DeathStarSegmentSetInit;

typedef struct DeathStarSegmentRuleCallbackInit {
	uint8_t segmentSetIdx;
	uint8_t ruleIdx;
	DeathStarSegmentUpdateFn updateFn;
} DeathStarSegmentRuleCallbackInit;

#pragma pack(push, 1)
typedef struct DeathStarScriptedObjectDef {
	uint16_t scriptId;
	uint16_t objectType;
	uint32_t modelOriginPointKind;
	Q16Angle yaw;
	Q16Angle pitch;
	uint16_t modelOriginPointIdx;
	int16_t segmentIdx;
	uint16_t segmentPointIdx;
	uint16_t allocationMode;
	uint16_t segmentSetIdx;
	uint16_t spawnedObjIdx;
	uint16_t segmentPointKind;
	uint8_t reserved1A[6];
} DeathStarScriptedObjectDef;
#pragma pack(pop)

typedef char death_star_scripted_object_def_size[(sizeof(DeathStarScriptedObjectDef) == 0x20) ? 1 : -1];

void DeathStar_UpdateEntranceSegment(int16_t activeSegmentSlotIdx, int16_t currentSegmentSlotIdx);
void DeathStar_ClearAccelChamberContainersAtSegment(int16_t currentSegmentIdx, int16_t triggerSegmentIdx);
void DeathStar_UpdateAccelChamberSegment(int16_t activeSegmentSlotIdx, int16_t currentSegmentSlotIdx);
void DeathStar_PullObjectTowardAccelChamber(int chamberObjIdx, ObjectRecord* targetObj);
void DeathStar_SteerObjectAnglesToward(Q16Angle targetYaw, Q16Angle targetPitch, ObjectRecord* object,
									   int distance, int rangeScale);
void DeathStar_EnableFollowOverrideAtSegment(int16_t currentSegmentIdx, int16_t triggerSegmentIdx);
void DeathStar_UpdateContainerCollisions(int16_t activeSegmentSlotIdx, int16_t currentSegmentSlotIdx);
void DeathStar_UpdateContainerCollision(int16_t activeSegmentSlotIdx, int containerObjIdx);
void DeathStar_ActivateTripodGunsNearFocusChamber(int16_t currentSegmentIdx, int16_t triggerSegmentIdx);
void DeathStar_UpdateReactorCylinderAndFollowMode(int16_t currentSegmentIdx, int16_t triggerSegmentIdx);
void DeathStar_UpdateLaserChamberFiring(int16_t currentSegmentIdx, int16_t triggerSegmentIdx);
void DeathStar_UpdateLaserEffectSegments(int extendBeam);
void DeathStar_ProjectPlayerOntoLaserChamberAxis(int* outX, int* outY, int* outZ);
void DeathStar_UpdateTunnelExitSegment(int16_t currentSegmentIdx, int16_t triggerSegmentIdx);

static DeathStarSegmentRule g_deathStarSegmentRules0[] DEATH_STAR_ALIGN_POINTER_TABLE = {
	{ 0, 352, 0x0047u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 1, 366, 0x0003u, 0x0800u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 2, 369, 0x0003u, 0x0000u, 0xf800u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 3, 373, 0x0103u, 0xf800u, 0x0800u, 0, 0, 0, 7, 0, 0, 0, 0, NULL },
	{ 4, 356, 0x0103u, 0x0500u, 0xf900u, 0, 0, 0, 9, 0, 0, 0, 0, NULL },
	{ 5, 357, 0x0183u, 0x0000u, 0x0000u, 0, 4, 1, 7, 0, 0, 0, 0, NULL },
	{ 6, 370, 0x0183u, 0xe000u, 0x0000u, 0, 4, 2, 7, 0, 0, 0, 0, NULL },
	{ -3, 369, 0x0400u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 1, 3, NULL },
	{ 0, 0, 0x0000u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
};

static DeathStarSegmentRule g_deathStarSegmentRules1[] DEATH_STAR_ALIGN_POINTER_TABLE = {
	{ 0, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, -6, 0, 0, NULL },
	{ 1, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, -5, 0, 0, NULL },
	{ 2, 369, 0x0600u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, -4, 0, -4, NULL },
	{ 3, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, -3, 0, 0, NULL },
	{ 4, 369, 0x0300u, 0x0000u, 0x0000u, 0, 0, 0, 9, 0, -2, 0, 0, NULL },
	{ 5, 369, 0x0300u, 0x0000u, 0x0000u, 0, 0, 0, 7, 0, -1, 0, 0, NULL },
	{ 6, 349, 0x0001u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 7, 354, 0x0003u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 8, 349, 0x008bu, 0x0000u, 0x0000u, 1, 7, 1, 0, 0, 0, 0, 0, NULL },
	{ 9, 369, 0x018au, 0x0000u, 0x0000u, 0, 8, 0, 7, 0, 0, 0, 0, NULL },
	{ 10, 369, 0x0000u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -9, 369, 0x0030u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -8, 369, 0x0030u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -7, 355, 0x0131u, 0x0000u, 0x0000u, 0, 0, 0, 9, 0, 0, 0, 0, NULL },
	{ -6, 362, 0x0403u, 0x2000u, 0x0000u, 0, 0, 0, 0, 0, 0, 3, 1, NULL },
	{ -5, 363, 0x0003u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -4, 359, 0x0483u, 0xd000u, 0x0000u, 0, -7, 2, 0, 0, 0, 2, 1, NULL },
	{ -3, 360, 0x0003u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -2, 363, 0x0081u, 0x0000u, 0x0000u, 0, -5, 1, 0, 0, 0, 0, 0, NULL },
	{ -1, 360, 0x0081u, 0x0000u, 0x0000u, 0, -3, 1, 0, 0, 0, 0, 0, NULL },
	{ 0, 0, 0x0000u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
};

static DeathStarSegmentRule g_deathStarSegmentRules2[] DEATH_STAR_ALIGN_POINTER_TABLE = {
	{ 0, 369, 0x0600u, 0x0000u, 0x0000u, 0, 0, 0, 0, 1, -7, 1, -7, NULL },
	{ 1, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 1, -4, 0, 0, NULL },
	{ 2, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 1, -3, 0, 0, NULL },
	{ 3, 369, 0x0300u, 0x0000u, 0x0000u, 0, 0, 0, 7, 1, -1, 0, 0, NULL },
	{ -4, 369, 0x0100u, 0x0000u, 0x0000u, 0, 0, 0, 7, 0, 0, 0, 0, NULL },
	{ -3, 369, 0x0010u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -2, 361, 0x0011u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -1, 350, 0x0003u, 0x0000u, 0xc800u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 0, 0, 0x0000u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
};

static DeathStarSegmentRule g_deathStarSegmentRules3[] DEATH_STAR_ALIGN_POINTER_TABLE = {
	{ 0, 369, 0x0600u, 0x0000u, 0x0000u, 0, 0, 0, 0, 1, -7, 1, -7, NULL },
	{ 1, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 1, -6, 0, 0, NULL },
	{ 2, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 1, -5, 0, 0, NULL },
	{ 3, 369, 0x0300u, 0x0000u, 0x0000u, 0, 0, 0, 7, 1, -2, 0, 0, NULL },
	{ -15, 369, 0x0010u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -14, 369, 0x0010u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -13, 369, 0x0010u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -12, 351, 0x0011u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -11, 369, 0x0002u, 0x4000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -10, 369, 0x0020u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -6, 369, 0x0100u, 0x0000u, 0x0000u, 0, 0, 0, 7, 0, 0, 0, 0, NULL },
	{ -5, 369, 0x0110u, 0x0000u, 0x0000u, 0, 0, 0, 7, 0, 0, 0, 0, NULL },
	{ -4, 364, 0x0103u, 0x0000u, 0x0000u, 0, 0, 0, 7, 0, 0, 0, 0, NULL },
	{ -3, 366, 0x0031u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -2, 356, 0x049bu, 0x0800u, 0x0000u, 2, -3, 1, 0, 0, 0, 4, 3, NULL },
	{ -1, 369, 0x0089u, 0x0000u, 0x0000u, 0, -2, 0, 0, 0, 0, 0, 0, NULL },
	{ 0, 0, 0x0000u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
};

static DeathStarSegmentRule g_deathStarSegmentRules4[] DEATH_STAR_ALIGN_POINTER_TABLE = {
	{ 0, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 3, -5, 0, 0, NULL },
	{ 1, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 3, -4, 0, 0, NULL },
	{ 2, 369, 0x0600u, 0x0000u, 0x0000u, 0, 0, 0, 0, 3, -3, 3, -3, NULL },
	{ 3, 369, 0x0300u, 0x0000u, 0x0000u, 0, 0, 0, 9, 3, -2, 0, 0, NULL },
	{ 4, 359, 0x0581u, 0x0000u, 0x0000u, 0, 3, 1, 7, 0, 0, 5, 3, NULL },
	{ 5, 360, 0x0003u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 6, 360, 0x0001u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 7, 369, 0x0300u, 0x0000u, 0x0000u, 0, 0, 0, 9, 3, -1, 0, 0, NULL },
	{ -3, 369, 0x0010u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -2, 369, 0x0010u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -1, 353, 0x0011u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 0, 0, 0x0000u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
};

static DeathStarSegmentRule g_deathStarSegmentRules5[] DEATH_STAR_ALIGN_POINTER_TABLE = {
	{ 0, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 4, 7, 0, 0, NULL },
	{ 1, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 4, 6, 0, 0, NULL },
	{ 2, 369, 0x0600u, 0x0000u, 0x0000u, 0, 0, 0, 0, 4, 3, 4, 3, NULL },
	{ 3, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 4, 4, 0, 0, NULL },
	{ 4, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 4, 5, 0, 0, NULL },
	{ -6, 369, 0x0100u, 0x0000u, 0x0000u, 0, 0, 0, 9, 0, 0, 0, 0, NULL },
	{ -5, 369, 0x0100u, 0x0000u, 0x0000u, 0, 0, 0, 9, 0, 0, 0, 0, NULL },
	{ -4, 361, 0x0503u, 0x0000u, 0x0000u, 0, 0, 0, 7, 0, 0, 6, 2, NULL },
	{ -3, 362, 0x0001u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -2, 363, 0x0003u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -1, 363, 0x0001u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 0, 0, 0x0000u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
};

static DeathStarSegmentRule g_deathStarSegmentRules6[] DEATH_STAR_ALIGN_POINTER_TABLE = {
	{ 0, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 5, -6, 0, 0, NULL },
	{ 1, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 5, -5, 0, 0, NULL },
	{ 2, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 5, -4, 0, 0, NULL },
	{ 3, 369, 0x0700u, 0x0000u, 0x0000u, 0, 0, 0, 7, 5, -3, 5, -3, NULL },
	{ 4, 369, 0x0300u, 0x0000u, 0x0000u, 0, 0, 0, 9, 5, -2, 0, 0, NULL },
	{ 5, 369, 0x0300u, 0x0000u, 0x0000u, 0, 0, 0, 9, 5, -1, 0, 0, NULL },
	{ -5, 369, 0x0100u, 0x0000u, 0x0000u, 0, 0, 0, 9, 0, 0, 0, 0, NULL },
	{ -4, 369, 0x0100u, 0x0000u, 0x0000u, 0, 0, 0, 9, 0, 0, 0, 0, NULL },
	{ -3, 364, 0x0503u, 0x0000u, 0x0000u, 0, 0, 0, 7, 0, 0, 7, 2, NULL },
	{ -2, 369, 0x0001u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -1, 366, 0x0001u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ 0, 0, 0x0000u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
};

static DeathStarSegmentRule g_deathStarSegmentRules7[] DEATH_STAR_ALIGN_POINTER_TABLE = {
	{ 0, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 6, -5, 0, 0, NULL },
	{ 1, 369, 0x0200u, 0x0000u, 0x0000u, 0, 0, 0, 0, 6, -4, 0, 0, NULL },
	{ 2, 369, 0x0600u, 0x0000u, 0x0000u, 0, 0, 0, 0, 6, -3, 6, -3, NULL },
	{ 3, 369, 0x0300u, 0x0000u, 0x0000u, 0, 0, 0, 7, 6, -2, 0, 0, NULL },
	{ 4, 369, 0x0300u, 0x0000u, 0x0000u, 0, 0, 0, 7, 6, -1, 0, 0, NULL },
	{ -8, 369, 0x0010u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -7, 369, 0x0010u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
	{ -6, 356, 0x008bu, 0x0000u, 0x0000u, 1, -7, 1, 0, 0, 0, 0, 0, NULL },
	{ -5, 357, 0x0083u, 0xf800u, 0x0000u, 0, -6, 2, 0, 0, 0, 0, 0, NULL },
	{ -4, 373, 0x0081u, 0xfa00u, 0x0600u, 1, -6, 0, 0, 0, 0, 0, 0, NULL },
	{ -3, 369, 0x0081u, 0x0800u, 0xf800u, 1, -4, 0, 0, 0, 0, 0, 0, NULL },
	{ -2, 366, 0x0081u, 0x0000u, 0x0800u, 1, -3, 0, 0, 0, 0, 0, 0, NULL },
	{ -1, 352, 0x0081u, 0xf800u, 0x0000u, 1, -2, 0, 0, 0, 0, 0, 0, NULL },
	{ 0, 0, 0x0000u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
};

static const DeathStarSegmentSetInit g_deathStarSegmentSetInitializers[8] = {
	{ 20, 0x0000u, 0x0000u, 0, g_deathStarSegmentRules0 },
	{ 25, 0x0000u, 0x0000u, 0, g_deathStarSegmentRules1 },
	{ 15, 0x0000u, 0x0000u, 360, g_deathStarSegmentRules2 },
	{ 30, 0x0000u, 0x0000u, 363, g_deathStarSegmentRules3 },
	{ 15, 0x0000u, 0x0000u, 0, g_deathStarSegmentRules4 },
	{ 17, 0x0000u, 0x0000u, 360, g_deathStarSegmentRules5 },
	{ 17, 0x0000u, 0x0000u, 363, g_deathStarSegmentRules6 },
	{ 17, 0x0000u, 0x0000u, 0, g_deathStarSegmentRules7 },
};

// GLOBAL: XWA 0x5B25D8
DeathStarSegmentSet g_deathStarSegmentSets[8] = {
	{ NULL, 20, 0x0000u, 0x0000u, 0, g_deathStarSegmentRules0 },
	{ NULL, 25, 0x0000u, 0x0000u, 0, g_deathStarSegmentRules1 },
	{ NULL, 15, 0x0000u, 0x0000u, 360, g_deathStarSegmentRules2 },
	{ NULL, 30, 0x0000u, 0x0000u, 363, g_deathStarSegmentRules3 },
	{ NULL, 15, 0x0000u, 0x0000u, 0, g_deathStarSegmentRules4 },
	{ NULL, 17, 0x0000u, 0x0000u, 360, g_deathStarSegmentRules5 },
	{ NULL, 17, 0x0000u, 0x0000u, 363, g_deathStarSegmentRules6 },
	{ NULL, 17, 0x0000u, 0x0000u, 0, g_deathStarSegmentRules7 },
};

static const DeathStarSegmentRuleCallbackInit g_deathStarSegmentRuleCallbackInitializers[] = {
	{ 0, 0, DeathStar_UpdateEntranceSegment },
	{ 1, 3, DeathStar_ClearAccelChamberContainersAtSegment },
	{ 1, 6, DeathStar_UpdateAccelChamberSegment },
	{ 1, 7, DeathStar_EnableFollowOverrideAtSegment },
	{ 1, 8, DeathStar_UpdateContainerCollisions },
	{ 1, 9, DeathStar_EnableFollowOverrideAtSegment },
	{ 1, 10, DeathStar_ClearAccelChamberContainersAtSegment },
	{ 2, 7, DeathStar_UpdateLaserChamberFiring },
	{ 3, 6, DeathStar_EnableFollowOverrideAtSegment },
	{ 3, 7, DeathStar_ActivateTripodGunsNearFocusChamber },
	{ 3, 8, DeathStar_EnableFollowOverrideAtSegment },
	{ 4, 10, DeathStar_UpdateReactorCylinderAndFollowMode },
	{ 7, 12, DeathStar_UpdateTunnelExitSegment },
};

// GLOBAL: XWA 0x5B23B8
static DeathStarScriptedObjectDef g_deathStarScriptedObjectDefs[] = {
	{ 0, OBJ_DSReactorCoreRoom, 24u, 0x0000u, 0x0000u, 0, -1, 1, 1, 4, 0, 24u, { 0 } },
	{ 1, OBJ_DSReactorCore, 24u, 0x4000u, 0x0000u, 0, 0, 1, 1, 8, 0, 24u, { 0 } },
	{ 2, OBJ_DSReactorCylinder, 24u, 0x0000u, 0x0000u, 0, 0, 2, 1, 8, 0, 24u, { 0 } },
	{ 3, OBJ_DSTankwlights, 24u, 0x0000u, 0x0000u, 0, -1, 2, 0, 7, 0, 24u, { 0 } },
	{ 4, OBJ_DSBentTubeGray, 24u, 0x0000u, 0x0000u, 0, 0, 2, 0, 0, 0, 24u, { 0 } },
	{ 5, OBJ_DSBentTubeRed, 24u, 0x0000u, 0x0000u, 0, 4, 3, 0, 0, 0, 24u, { 0 } },
	{ 6, OBJ_DSTankwPipeRed01, 24u, 0x0000u, 0x0000u, 0, 6, 2, 0, 1, 0, 24u, { 0 } },
	{ 7, OBJ_DSTankwPipeBlue01, 24u, 0x0000u, 0x0000u, 0, -7, 3, 0, 1, 0, 24u, { 0 } },
	{ 8, OBJ_DSBentTubeBlue01, 24u, 0x0000u, 0x0000u, 0, 0, 2, 0, 0, 0, 24u, { 0 } },
	{ 9, OBJ_DSBigTubeExit, 24u, 0x0000u, 0x0000u, 0, -12, 0, 0, 3, 0, 24u, { 0 } },
	{ 10, OBJ_DSTripodGun, 24u, 0x0000u, 0x0000u, 0, -12, 2, 1, 3, 0, 24u, { 0 } },
	{ 11, OBJ_DS3rdRoom, 0u, 0x0000u, 0x0000u, 0, -12, 0, 1, 3, 0, 0u, { 0 } },
	{ 12, OBJ_DSBigTubeEntrance, 24u, 0x0000u, 0x0000u, 0, -11, 0, 0, 3, 0, 24u, { 0 } },
	{ 13, OBJ_DSFocusLens, 24u, 0x0000u, 0x0000u, 0, -1, 1, 1, 2, 0, 24u, { 0 } },
	{ 14, OBJ_DSFocusLens, 24u, 0x0000u, 0x0000u, 0, -1, 2, 1, 2, 0, 24u, { 0 } },
	{ 14, OBJ_DSFocusLens, 24u, 0x0000u, 0x0000u, 0, -1, 3, 1, 2, 0, 24u, { 0 } },
	{ 0, OBJ_None, 0u, 0x0000u, 0x0000u, 0, 0, 0, 0, 0, 0, 0u, { 0 } },
};

static void DeathStar_InstallSegmentRuleCallbacks(void) {
	size_t i;

	for (i = 0; i < sizeof(g_deathStarSegmentRuleCallbackInitializers) /
						sizeof(g_deathStarSegmentRuleCallbackInitializers[0]);
		 ++i) {
		const DeathStarSegmentRuleCallbackInit* init;

		init = &g_deathStarSegmentRuleCallbackInitializers[i];
		g_deathStarSegmentSetInitializers[init->segmentSetIdx].rules[init->ruleIdx].updateFn = init->updateFn;
	}
}

typedef enum DeathStarSegmentUpdateFnId {
	DEATH_STAR_SEGMENT_UPDATE_NONE = 0,
	DEATH_STAR_SEGMENT_UPDATE_ENTRANCE,
	DEATH_STAR_SEGMENT_UPDATE_CLEAR_ACCEL_CONTAINERS,
	DEATH_STAR_SEGMENT_UPDATE_ACCEL_CHAMBER,
	DEATH_STAR_SEGMENT_UPDATE_ENABLE_FOLLOW_OVERRIDE,
	DEATH_STAR_SEGMENT_UPDATE_CONTAINER_COLLISIONS,
	DEATH_STAR_SEGMENT_UPDATE_ACTIVATE_TRIPOD_GUNS,
	DEATH_STAR_SEGMENT_UPDATE_REACTOR_CYLINDER_AND_FOLLOW,
	DEATH_STAR_SEGMENT_UPDATE_LASER_CHAMBER,
	DEATH_STAR_SEGMENT_UPDATE_TUNNEL_EXIT,
} DeathStarSegmentUpdateFnId;

#pragma pack(push, 1)
typedef struct DeathStarFilmSegmentSetState {
	uint16_t count;
	Q16Angle baseYaw;
	Q16Angle basePitch;
	ObjectTypeId fixedSegmentObjectType;
} DeathStarFilmSegmentSetState;

typedef struct DeathStarFilmSegmentDefState {
	uint16_t objectType;
	uint32_t flags;
	int worldX;
	int worldY;
	int worldZ;
	Q16Angle yaw;
	Q16Angle pitch;
	uint16_t nextSegmentSet;
	int16_t nextSegmentIdx;
	uint16_t activeSegmentCount;
	uint16_t updateFnId;
	DeathStarChildObjectRef childObjects[10];
} DeathStarFilmSegmentDefState;
#pragma pack(pop)

static uint16_t DeathStar_GetSegmentUpdateFnId(DeathStarSegmentUpdateFn updateFn) {
	if (updateFn == NULL) {
		return DEATH_STAR_SEGMENT_UPDATE_NONE;
	}
	if (updateFn == DeathStar_UpdateEntranceSegment) {
		return DEATH_STAR_SEGMENT_UPDATE_ENTRANCE;
	}
	if (updateFn == DeathStar_ClearAccelChamberContainersAtSegment) {
		return DEATH_STAR_SEGMENT_UPDATE_CLEAR_ACCEL_CONTAINERS;
	}
	if (updateFn == DeathStar_UpdateAccelChamberSegment) {
		return DEATH_STAR_SEGMENT_UPDATE_ACCEL_CHAMBER;
	}
	if (updateFn == DeathStar_EnableFollowOverrideAtSegment) {
		return DEATH_STAR_SEGMENT_UPDATE_ENABLE_FOLLOW_OVERRIDE;
	}
	if (updateFn == DeathStar_UpdateContainerCollisions) {
		return DEATH_STAR_SEGMENT_UPDATE_CONTAINER_COLLISIONS;
	}
	if (updateFn == DeathStar_ActivateTripodGunsNearFocusChamber) {
		return DEATH_STAR_SEGMENT_UPDATE_ACTIVATE_TRIPOD_GUNS;
	}
	if (updateFn == DeathStar_UpdateReactorCylinderAndFollowMode) {
		return DEATH_STAR_SEGMENT_UPDATE_REACTOR_CYLINDER_AND_FOLLOW;
	}
	if (updateFn == DeathStar_UpdateLaserChamberFiring) {
		return DEATH_STAR_SEGMENT_UPDATE_LASER_CHAMBER;
	}
	if (updateFn == DeathStar_UpdateTunnelExitSegment) {
		return DEATH_STAR_SEGMENT_UPDATE_TUNNEL_EXIT;
	}
	return DEATH_STAR_SEGMENT_UPDATE_NONE;
}

static DeathStarSegmentUpdateFn DeathStar_GetSegmentUpdateFnById(uint16_t updateFnId) {
	switch (updateFnId) {
		case DEATH_STAR_SEGMENT_UPDATE_ENTRANCE:
			return DeathStar_UpdateEntranceSegment;
		case DEATH_STAR_SEGMENT_UPDATE_CLEAR_ACCEL_CONTAINERS:
			return DeathStar_ClearAccelChamberContainersAtSegment;
		case DEATH_STAR_SEGMENT_UPDATE_ACCEL_CHAMBER:
			return DeathStar_UpdateAccelChamberSegment;
		case DEATH_STAR_SEGMENT_UPDATE_ENABLE_FOLLOW_OVERRIDE:
			return DeathStar_EnableFollowOverrideAtSegment;
		case DEATH_STAR_SEGMENT_UPDATE_CONTAINER_COLLISIONS:
			return DeathStar_UpdateContainerCollisions;
		case DEATH_STAR_SEGMENT_UPDATE_ACTIVATE_TRIPOD_GUNS:
			return DeathStar_ActivateTripodGunsNearFocusChamber;
		case DEATH_STAR_SEGMENT_UPDATE_REACTOR_CYLINDER_AND_FOLLOW:
			return DeathStar_UpdateReactorCylinderAndFollowMode;
		case DEATH_STAR_SEGMENT_UPDATE_LASER_CHAMBER:
			return DeathStar_UpdateLaserChamberFiring;
		case DEATH_STAR_SEGMENT_UPDATE_TUNNEL_EXIT:
			return DeathStar_UpdateTunnelExitSegment;
		default:
			return NULL;
	}
}

// FUNCTION: XWA 0x423730
void DeathStar_Init(void) {
	uint8_t flightGroupIdx;
	float ricochetDamageScale;
	int meshIdx;
	int modelType;
	uint32_t objIdx;

	Math_SeedRandom((uint16_t)timeGetTime());

	g_deathStarPlayerObjIdx = g_players[g_localPlayer].objectIndex;
	g_deathStarReactorCoreObjIdx = 0xffff;
	g_deathStarReactorCylinderObjIdx = 0xffff;
	g_deathStarRandomChildObjectLimit = (uint8_t)(2 * ((uint8_t)g_flightDifficulty + 2));
	g_deathStarLastGeneratedRandomSegmentType = OBJ_None;
	g_deathStarSegmentChildInitialHitCount = (uint8_t)(3 * (uint8_t)g_flightDifficulty + 1);
	g_deathStarLoopSfxVolume[0] = 0;
	g_deathStarLoopSfxVolume[1] = 0;
	ricochetDamageScale = (float)g_flightDifficulty;
	g_collideRicochetDamageScale = ricochetDamageScale * g_deathStarRicochetDamageDifficultyScale -
								   g_deathStarRicochetDamageDifficultyBias;
	flightGroupIdx = (uint8_t)g_missionHeader.numFlightGroups;
	--flightGroupIdx;
	g_deathStarReactorCoreRoomFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarActiveSegmentPlaceholderFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarDefaultScriptedObjectFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarGeneratedObjectFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarReactorCoreFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarTripodGunFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarFocusChamberFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarTankLightsFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarBentTubeGrayFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarBentTubeRedFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarTankPipeRedFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarFocusLensFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarTankPipeBlueFgIdx = flightGroupIdx;
	--flightGroupIdx;
	g_deathStarBentTubeBlueFgIdx = flightGroupIdx;
	g_deathStarTunnelTimer = 0;
	g_deathStarReactorAssaultFgIdx = 1;
	memset(g_deathStarLoopSfxActive, 0, sizeof(g_deathStarLoopSfxActive));

	for (objIdx = 0; objIdx < g_objectTableSlotCount; ++objIdx) {
		ObjectRecord* object;

		object = &g_objectTable[objIdx];
		if (object->objectType != OBJ_None) {
			flightGroupIdx = object->flightGroupIdx;
			if (flightGroupIdx == g_deathStarReactorCoreRoomFgIdx ||
				flightGroupIdx == g_deathStarActiveSegmentPlaceholderFgIdx ||
				flightGroupIdx == g_deathStarDefaultScriptedObjectFgIdx ||
				flightGroupIdx == g_deathStarGeneratedObjectFgIdx ||
				flightGroupIdx == g_deathStarReactorCoreFgIdx ||
				flightGroupIdx == g_deathStarTripodGunFgIdx ||
				flightGroupIdx == g_deathStarFocusChamberFgIdx ||
				flightGroupIdx == g_deathStarTankLightsFgIdx ||
				flightGroupIdx == g_deathStarBentTubeGrayFgIdx ||
				flightGroupIdx == g_deathStarBentTubeRedFgIdx ||
				flightGroupIdx == g_deathStarTankPipeRedFgIdx ||
				flightGroupIdx == g_deathStarFocusLensFgIdx ||
				flightGroupIdx == g_deathStarTankPipeBlueFgIdx ||
				flightGroupIdx == g_deathStarBentTubeBlueFgIdx) {
				object->objectType = OBJ_None;
			}
			if (object->flightGroupIdx == g_deathStarReactorAssaultFgIdx) {
				g_deathStarReactorAssaultCraftObjIdx = (int)objIdx;
			}
		}
	}

	meshIdx = 0;
	g_collideSweepAllowUnownedTargets = 1;
	memset(g_deathStarObjectPointTables, 0, sizeof(g_deathStarObjectPointTables));

	{
		int modelTypeCount;

		modelType = OBJ_DSReactorCore;
		modelTypeCount = OBJ_ChuteMouth - OBJ_DSReactorCore;
		do {
			DeathStarObjectPointTable* pointTable;
			uint16_t meshCount;

			pointTable = &g_deathStarObjectPointTables[modelType - OBJ_DSReactorCore];
			meshCount = (uint16_t)ModelMesh_GetCount(modelType);
			pointTable->attachPointCount = 0;
			pointTable->spawnPointCount = 0;
			if (meshCount > 0u) {
				int meshCountRemaining;

				meshCountRemaining = meshCount;
				do {
					uint16_t hardpointCount;

					hardpointCount = (uint16_t)ModelMesh_CountHardpoints(modelType, meshIdx);
					if (hardpointCount != 0 && hardpointCount > 0u) {
						int hardpointCountRemaining;
						int hardpointIdx;

						OptHardpointType outType;
						int outX;
						int outY;
						int outZ;

						hardpointIdx = 0;
						hardpointCountRemaining = hardpointCount;
						do {
							ModelMesh_GetHardpoint(modelType, meshIdx, hardpointIdx, &outType, &outX, &outY,
												   &outZ);
							switch (outType) {
								case OPT_HARDPOINT_AccStart:
									if (pointTable->spawnPointCount < 10u) {
										pointTable->spawnPoints[pointTable->spawnPointCount].side = outX;
										pointTable->spawnPoints[pointTable->spawnPointCount].forward = outY;
										pointTable->spawnPoints[pointTable->spawnPointCount].up = outZ;
										++pointTable->spawnPointCount;
									}
									break;
								case OPT_HARDPOINT_AccEnd:
									if (pointTable->attachPointCount < 5u) {
										pointTable->attachPoints[pointTable->attachPointCount].side = outX;
										pointTable->attachPoints[pointTable->attachPointCount].forward = outY;
										pointTable->attachPoints[pointTable->attachPointCount].up = outZ;
										++pointTable->attachPointCount;
									}
									break;
								default:
									break;
							}
							++hardpointIdx;
						} while (--hardpointCountRemaining != 0);
					}
					++meshIdx;
				} while (--meshCountRemaining != 0);
				meshIdx = 0;
			}
			++modelType;
		} while (--modelTypeCount != 0);
	}

	DeathStar_BuildSegmentSets();
	DeathStar_SpawnScriptedObjects();
	g_deathStarSegmentSetIdx = meshIdx;
	DeathStar_LoadActiveSegments();
	g_objectTable[g_deathStarPlayerObjIdx].mobj->collisionObjIdx = g_deathStarActiveSegmentObjIdx[0];
	g_deathStarCurrentSegmentIdx.value = (uint16_t)meshIdx;
	{
		uint16_t depthScaleIdx;

		depthScaleIdx = g_deathStarSegmentSets[g_deathStarSegmentSetIdx].segments[0].objectType;
		depthScaleIdx = (uint16_t)(depthScaleIdx - OBJ_DSReactorCoreRoom);
		RenderScene_SetDepthProjectionScale(g_deathStarSegmentDepthProjScale[depthScaleIdx]);
	}
	DeathStar_InitFollowOverrideState();
	DeathStar_PositionPlayerAndFollowersAtStart();
	DeathStar_InitAccelChamberState();
	DeathStar_InitZeroGStormtrooperWaypoints();
	DeathStar_InitReactorAssaultState();
	DeathStar_InitLaserChamber();
}

// FUNCTION: XWA 0x423B30
void DeathStar_Shutdown(void) {
	DeathStarSegmentSet* segmentSet;
	int segmentSetCount;

	segmentSet = g_deathStarSegmentSets;
	segmentSetCount = 8;
	do {
		if (segmentSet->segments != NULL) {
			Memory_FreeTagged("DSSEGMENT", segmentSet->segments);
			segmentSet->segments = NULL;
		}
		++segmentSet;
	} while (--segmentSetCount != 0);

	g_objectTable[g_deathStarReactorCoreRoomObjIdx].typeSpecificWord = 0;
	DeathStar_UpdateLoopingObjectSfxVolume(0, (unsigned int)g_deathStarReactorCoreRoomObjIdx, 0);
	DeathStar_UpdateLoopingObjectSfxVolume(1, (unsigned int)g_deathStarReactorCoreRoomObjIdx, 1);

	g_collideSweepAllowUnownedTargets = 0;
	g_collideRicochetDamageScale = 1.0f;
	RenderScene_ResetDepthProjectionScale();
}

// FUNCTION: XWA 0x423BC0
// Per-tick Death Star tunnel update. Tracks the player's current tunnel segment from
// collisions (focus chamber / reactor-core room override it), switches segment sets and
// the active-segment window when the segment changes (rebuilding child objects and
// resetting proximity), records a periodic player path-history sample, validates the
// follow-chain slots, runs each active segment's per-tick callback, and advances the
// reactor destruction sequence.
void DeathStarTunnel_Update(void) {
	DeathStarSegmentIndex newSegmentIdx;
	DeathStarSegmentIndex playerSegmentIdx;
	int collisionObjIdx;
	int activeSlotIdx;
	int searchSlot;

	g_deathStarTunnelTimer += (uint16_t)g_elapsedTicks;
	newSegmentIdx.paddedValue = g_deathStarCurrentSegmentIdx.paddedValue;
	playerSegmentIdx.paddedValue = newSegmentIdx.paddedValue;
	collisionObjIdx = g_objectTable[g_deathStarPlayerObjIdx].mobj->collisionObjIdx;
	if (collisionObjIdx == g_deathStarFocusChamberObjIdx) {
		playerSegmentIdx.paddedValue = g_deathStarFocusChamberSegmentIdx.paddedValue;
		newSegmentIdx.paddedValue = playerSegmentIdx.paddedValue;
	}
	if (collisionObjIdx == g_deathStarReactorCoreRoomObjIdx ||
		collisionObjIdx == g_deathStarReactorCoreObjIdx) {
		playerSegmentIdx.paddedValue = g_deathStarReactorCoreRoomSegmentIdx.paddedValue;
		newSegmentIdx.paddedValue = playerSegmentIdx.paddedValue;
	}

	searchSlot = 0;
	activeSlotIdx = 0;
	for (searchSlot = 0; (uint16_t)searchSlot < (uint16_t)g_deathStarActiveSegmentCount; ++searchSlot) {
		if (g_deathStarActiveSegmentObjIdx[(uint16_t)searchSlot] == collisionObjIdx) {
			activeSlotIdx = searchSlot;
			playerSegmentIdx.value = g_deathStarActiveSegmentIdx[(uint16_t)searchSlot];
			newSegmentIdx.paddedValue = playerSegmentIdx.paddedValue;
			break;
		}
	}
	if ((uint16_t)searchSlot == (uint16_t)g_deathStarActiveSegmentCount) {
		int j;
		for (j = 0; (uint16_t)j < (uint16_t)g_deathStarActiveSegmentCount; ++j) {
			if (newSegmentIdx.value == g_deathStarActiveSegmentIdx[j]) {
				activeSlotIdx = j;
				break;
			}
		}
	}

	if (newSegmentIdx.value != g_deathStarCurrentSegmentIdx.value) {
		unsigned int segmentSet = g_deathStarSegmentSetIdx;
		int16_t centerSegmentIdx = (int16_t)newSegmentIdx.value;
		uint16_t oldSegmentSet;
		uint16_t oldCount;
		uint16_t newActiveCount;
		DeathStarSegmentDef* segDef;
		unsigned int slot;
		uint16_t oldActiveSegmentIdx[10];

		g_deathStarCurrentSegmentIdx.value = newSegmentIdx.value;
		if ((uint16_t)g_deathStarActiveSegmentCount) {
			uint16_t activePos;
			for (activePos = 0; activePos < (uint16_t)g_deathStarActiveSegmentCount; ++activePos) {
				DeathStarSegmentDef* segments = g_deathStarSegmentSets[segmentSet].segments;
				int segIdx = g_deathStarActiveSegmentIdx[activePos];
				uint32_t flags = segments[segIdx].flags;
				DeathStarChildObjectRef* childObjects = segments[segIdx].childObjects;
				int k;
				if (flags & 0x200) {
					uint16_t redirectedSegmentIdx = childObjects->angleByteOffsets;
					childObjects = g_deathStarSegmentSets[childObjects->objectType]
									   .segments[redirectedSegmentIdx]
									   .childObjects;
				}
				for (k = 0; k < 10; ++k) {
					uint16_t objectIdx = childObjects[k].objectIdx;
#ifdef XWA_MODERN
					if (childObjects[k].objectType && objectIdx != 0xFFFF) {
						ObjectRecord* obj = &g_objectTable[objectIdx];
#else
					ObjectRecord* obj = &g_objectTable[objectIdx];
					if (childObjects[k].objectType && objectIdx != 0xFFFF) {
#endif
						if (obj->objectType != childObjects[k].objectType ||
							obj->objectSignature != childObjects[k].objectSignature) {
							childObjects[k].objectType = 0;
							segmentSet = g_deathStarSegmentSetIdx;
						}
					}
				}
			}
			newSegmentIdx.paddedValue = playerSegmentIdx.paddedValue;
		}

		oldSegmentSet = (uint16_t)segmentSet;
		segDef = &g_deathStarSegmentSets[segmentSet].segments[(int16_t)newSegmentIdx.value];
		if (segDef->flags & 0x400) {
			segmentSet = segDef->nextSegmentSet;
			g_deathStarSegmentSetIdx = segmentSet;
			centerSegmentIdx = segDef->nextSegmentIdx;
		}

		oldCount = (uint16_t)g_deathStarActiveSegmentCount;
		newActiveCount = g_deathStarSegmentSets[segmentSet].segments[centerSegmentIdx].activeSegmentCount;
		if (newActiveCount != (uint16_t)g_deathStarActiveSegmentCount)
			DeathStar_ResizeActiveSegmentSlots(newActiveCount);
		memcpy(oldActiveSegmentIdx, g_deathStarActiveSegmentIdx, sizeof(oldActiveSegmentIdx));
		DeathStar_UpdateActiveSegmentWindow(centerSegmentIdx);
		DeathStar_RebuildSegmentChildObjects(oldActiveSegmentIdx, oldSegmentSet, oldCount);
		if (oldSegmentSet != g_deathStarSegmentSetIdx)
			g_deathStarCurrentSegmentIdx.value = (uint16_t)centerSegmentIdx;

		for (slot = g_activeRegionObjectSlotStart; (uint16_t)slot < g_activeRegionCraftObjectSlotEnd;
			 ++slot) {
			if (g_objectTable[(uint16_t)slot].objectType)
				collide_ResetObjectProximityForSlot((uint16_t)slot);
		}

		RenderScene_SetDepthProjectionScale(g_deathStarSegmentDepthProjScale[(
			uint16_t)(g_deathStarSegmentSets[g_deathStarSegmentSetIdx].segments[centerSegmentIdx].objectType -
					  OBJ_DSReactorCoreRoom)]);
		if (g_deathStarSegmentSetIdx == 1 && centerSegmentIdx == g_deathStarSegmentSets[1].count - 8 &&
			g_deathStarLaserPowerSourceObjIdx == 0xFFFF)
			DeathStar_SpawnLaserPowerSourceObject();

		{
			int slotPos;
			for (slotPos = 0; (uint16_t)slotPos < (uint16_t)g_deathStarActiveSegmentCount; ++slotPos) {
				if (g_deathStarActiveSegmentIdx[(uint16_t)slotPos] == playerSegmentIdx.value) {
					activeSlotIdx = slotPos;
					break;
				}
			}
		}
	}

	{
		int prevIdx = g_deathStarPathSampleWriteIdx - 1;
		ObjectRecord* leader;
		unsigned int dist;
		if (prevIdx < 0)
			prevIdx += 0x1E;
		leader = &g_objectTable[g_deathStarFollowLeaderObjIdx];
		dist = collide_roughdistance3d(leader->world_x - g_deathStarPathSamples[prevIdx].worldX,
									   leader->world_y - g_deathStarPathSamples[prevIdx].worldY,
									   leader->world_z - g_deathStarPathSamples[prevIdx].worldZ);
		if (dist >= 0x1F4) {
			int dt = g_deathStarTunnelTimer - g_deathStarPathSampleLastTime;
			if ((unsigned int)dt > 0x32) {
				DeathStarPathSample* s = &g_deathStarPathSamples[g_deathStarPathSampleWriteIdx];
				s->worldX = leader->world_x;
				s->worldY = leader->world_y;
				s->worldZ = leader->world_z;
				s->yaw = leader->yaw;
				s->pitch = leader->pitch;
				s->roll = leader->roll;
				s->tacticalIndex = leader->mobj->speed;
				s->elapsedTicksSincePrev = dt;
				g_deathStarPathSampleLastTime = g_deathStarTunnelTimer;
				++g_deathStarPathSampleWriteIdx;
				if (g_deathStarPathSampleWriteIdx == 30)
					g_deathStarPathSampleWriteIdx = 0;
			}
		}
	}

	if ((unsigned int)(g_deathStarTunnelTimer - g_deathStarFollowChainLastValidateTime) > 0x3E8) {
		int i;
		DeathStarFollowChainSlot* slot;
		g_deathStarFollowChainLastValidateTime = g_deathStarTunnelTimer;
		i = 0;
		slot = g_deathStarFollowChainSlots;
		do {
			int objIdx = slot->objectIdx;
			if (objIdx != 0xFFFF) {
				if (g_objectTable[objIdx].objectType == OBJ_None ||
					g_objectTable[objIdx].objectSignature != slot->objectSignature)
					DeathStar_RemoveFollowChainSlot(i);
			}
			++slot;
			++i;
		} while ((intptr_t)slot < (intptr_t)&g_deathStarFollowLeaderExtentX4);
	}

	{
		int i;
		for (i = 0; (uint16_t)i < (uint16_t)g_deathStarActiveSegmentCount; ++i) {
			int segmentSetIdx = g_deathStarSegmentSetIdx;
			int segmentIdx = g_deathStarActiveSegmentIdx[(uint16_t)i];
			DeathStarSegmentUpdateFn updateFn =
				g_deathStarSegmentSets[segmentSetIdx].segments[segmentIdx].updateFn;
			if (updateFn)
				updateFn((int16_t)i, (int16_t)activeSlotIdx);
		}
	}

	if (g_deathStarReactorDestructionTimer)
		DeathStarTunnel_UpdateReactorDestructionSequence((int16_t)activeSlotIdx);
}
// FUNCTION: XWA 0x4240D0
int DeathStar_HandlePowerNodeHit(unsigned int sourceObjIdx, unsigned int powerNodeObjIdx,
								 unsigned int hitComponentId) {
	int result;

	(void)hitComponentId;

	if (g_objectTable[powerNodeObjIdx].flightGroupIdx == g_deathStarGeneratedObjectFgIdx) {
		result = OBJ_SparkTextureGroup3000;
		if (g_objectTable[sourceObjIdx].genusId == GENUS_PlayerProjectile ||
			g_objectTable[sourceObjIdx].genusId == GENUS_NpcProjectile) {
#ifdef XWA_MODERN
			int warheadClass = laser_GetProjectileWarheadClass(g_objectTable[sourceObjIdx].objectType);

			if (warheadClass < 0) {
				return result;
			}
			if (warheadClass == 0) {
#else
			if (g_projectileWarheadClassByType[(uint16_t)g_objectTable[sourceObjIdx].objectType -
											 OBJ_LaserRebel] == 0) {
#endif
				--g_objectTable[powerNodeObjIdx].typeSpecificWord;
			} else if (g_objectTable[powerNodeObjIdx].typeSpecificWord > 50u) {
				g_objectTable[powerNodeObjIdx].typeSpecificWord =
					(uint16_t)(g_objectTable[powerNodeObjIdx].typeSpecificWord - 50u);
			} else {
				g_objectTable[powerNodeObjIdx].typeSpecificWord = 0;
			}

			if (g_objectTable[powerNodeObjIdx].typeSpecificWord <= 0u) {
				g_objectTable[powerNodeObjIdx].objectType = OBJ_None;
				result = (uint16_t)GameRand() % 5 + OBJ_ExplosionTextureGroup2000;
				g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][0] += 10;
			}
		}
		return result;
	}

	result = 0;
	if (g_objectTable[powerNodeObjIdx].objectType != OBJ_DSLaserPowerSource) {
		ObjectRecord* sourceObject = &g_objectTable[sourceObjIdx];
		uint16_t sourceObjectType = sourceObject->objectType;
		if (sourceObjectType == OBJ_DSLaserInternal) {
			return sourceObject->typeSpecificWord == 1 ? OBJ_ExplosionTextureGroup2002 : 0;
		}
		if (sourceObjectType == OBJ_LaserIon || sourceObjectType == OBJ_LaserIonTurbo) {
			return OBJ_SparkTextureGroup3001;
		}
		return OBJ_SparkTextureGroup3000;
	}

	if (g_objectTable[sourceObjIdx].genusId == GENUS_PlayerProjectile ||
		g_objectTable[sourceObjIdx].genusId == GENUS_NpcProjectile) {
#ifdef XWA_MODERN
		int warheadClass = laser_GetProjectileWarheadClass(g_objectTable[sourceObjIdx].objectType);

		if (warheadClass < 0) {
			return result;
		}
		if (warheadClass == 0) {
#else
		if (g_projectileWarheadClassByType[(uint16_t)g_objectTable[sourceObjIdx].objectType -
										 OBJ_LaserRebel] == 0) {
#endif
			--g_objectTable[powerNodeObjIdx].typeSpecificWord;
		} else {
			g_objectTable[powerNodeObjIdx].typeSpecificWord = 0;
		}

		if (g_objectTable[powerNodeObjIdx].typeSpecificWord <= 0u &&
			g_deathStarPowerSourceLinkedFocusLensObjIdx != 0xffff) {
			CraftData* linkedCraft = g_objectTable[g_deathStarPowerSourceLinkedFocusLensObjIdx].mobj->pCraft;
			linkedCraft->hullDamage += ((unsigned int)linkedCraft->hullMax >> 1) + 1u;
			g_deathStarPowerSourceLinkedFocusLensObjIdx = 0xffff;
			g_objectTable[powerNodeObjIdx].objectType = OBJ_None;
			result = (uint16_t)GameRand() % 5 + OBJ_ExplosionTextureGroup2000;
			if (g_useHardware3D) {
				g_objectTable[sourceObjIdx].mobj->instanceExtent *= 8;
				return result;
			}
			g_objectTable[sourceObjIdx].mobj->instanceExtent *= 4;
		}
	}
	return result;
}

// FUNCTION: XWA 0x4242E0
// Handle a projectile hit on a Death Star reactor object (reactor core, cylinder, or
// focus lens). Warhead hits damage the reactor core's two components and, once both are
// down, start the reactor destruction timer and clear the tripod gun / focus chamber.
// The focus lens deflects internal laser shots once half-damaged; projectile-genus hits
// are converted into an impact effect in place.
void DeathStar_HandleReactorHit(unsigned int projectileObjIdx, unsigned int reactorObjIdx,
								unsigned int hitComponentId) {
	uint16_t objectType = g_objectTable[projectileObjIdx].objectType;
	int result;

	result = 271;
	if (objectType == OBJ_LaserIon || objectType == OBJ_LaserIonTurbo)
		result = 272;

	switch (g_objectTable[reactorObjIdx].objectType) {
		case OBJ_DSReactorCore: {
			int effect;
			switch (objectType) {
				case OBJ_WarheadTorpedo:
				case OBJ_WarheadMissile:
				case OBJ_WarheadAdvancedTorpedo:
				case OBJ_WarheadAdvancedMissile:
				case OBJ_WarheadSpaceBomb:
				case OBJ_WarheadRocket: {
					CraftData* savedCurCraft;
					CraftData* reactorCraft;

					effect = 266;
					g_missionFlightGroups[(uint8_t)g_deathStarReactorCoreFgIdx].fg.status2 = 0;
					g_missionFlightGroups[(uint8_t)g_deathStarReactorCoreFgIdx].fg.status1 = 0;
					savedCurCraft = g_curCraft;
					reactorCraft = g_objectTable[reactorObjIdx].mobj->pCraft;
					g_curCraft = reactorCraft;
					switch ((uint16_t)hitComponentId) {
						case 1:
							Craft_DamageComponent(g_deathStarReactorCoreObjIdx, hitComponentId + 1, 0x100u,
												  g_objectTable[projectileObjIdx].mobj->sourceObjIdx);
							if (!reactorCraft->componentHp[1]) {
								collide_damagecraft(g_deathStarReactorCoreObjIdx, 0xFFFFu, 0xFFFFFFFDu, 0x20u,
													0);
								fsfx_PlaySound(169, g_deathStarReactorCoreObjIdx, g_localPlayer);
							}
							effect = 270;
							break;
						case 2:
							if (!reactorCraft->componentHp[1]) {
								Craft_DamageComponent(g_deathStarReactorCoreObjIdx, hitComponentId + 1,
													  0x100u,
													  g_objectTable[projectileObjIdx].mobj->sourceObjIdx);
							}
							break;
					}
					g_curCraft = savedCurCraft;
					g_missionFlightGroups[(uint8_t)g_deathStarReactorCoreFgIdx].fg.status2 = 20;
					g_missionFlightGroups[(uint8_t)g_deathStarReactorCoreFgIdx].fg.status1 = 20;
					if (!reactorCraft->componentHp[2]) {
						CraftData* coreCraft;
						unsigned int destructionTime;
						fsfx_PlaySound(170, g_deathStarReactorCoreObjIdx, g_localPlayer);
						coreCraft = g_objectTable[g_deathStarReactorCoreObjIdx].mobj->pCraft;
						coreCraft->hullDamage = coreCraft->hullMax - 1;
						g_objectTable[g_deathStarReactorCoreObjIdx].mobj->lifetimeTimer = 236000;
						destructionTime = (uint16_t)g_elapsedTicks;
						g_deathStarReactorDestructionTimer = destructionTime;
						if (!destructionTime)
							g_deathStarReactorDestructionTimer = 1;
						DeathStar_SpawnReactorDebrisGirders();
						if (g_deathStarTripodGunObjIdx != 0xFFFF)
							g_objectTable[g_deathStarTripodGunObjIdx].objectType = OBJ_None;
						if (g_deathStarFocusChamberObjIdx != 0xFFFF)
							g_objectTable[g_deathStarFocusChamberObjIdx].objectType = OBJ_None;
					}
					break;
				}
				default:
					effect = (objectType == OBJ_LaserIon || objectType == OBJ_LaserIonTurbo) ? 272 : 271;
					break;
			}
			result = effect;
			break;
		}
		case OBJ_DSReactorCylinder:
			if (g_deathStarReactorCoreObjIdx)
				result =
					g_objectTable[g_deathStarReactorCoreObjIdx].mobj->pCraft->componentHp[1] != 0 ? 0x110 : 0;
			break;
		case OBJ_DSFocusLens:
			if (objectType == OBJ_DSLaserInternal) {
				result = 0;
				{
					CraftData* lensCraft = g_objectTable[reactorObjIdx].mobj->pCraft;
					if ((unsigned int)lensCraft->hullDamage >= ((unsigned int)lensCraft->hullMax >> 1)) {
						int r;
						Q16Angle pitch;
						r = GameRand() & 0x83FF;
						if (r & 0x8000)
							g_objectTable[projectileObjIdx].yaw += r & 0x7FFF;
						else
							g_objectTable[projectileObjIdx].yaw -= r & 0x7FFF;
						r = GameRand() & 0x83FF;
						if (r & 0x8000)
							g_objectTable[projectileObjIdx].pitch += r & 0x7FFF;
						else
							g_objectTable[projectileObjIdx].pitch -= r & 0x7FFF;
						pitch = g_objectTable[projectileObjIdx].pitch;
						if (pitch >= 0x8000u) {
							g_objectTable[projectileObjIdx].pitch = -pitch;
							g_objectTable[projectileObjIdx].yaw += 0x8000;
						}
						g_objectTable[projectileObjIdx].mobj->orientMatrixDirty = 1;
						g_objectTable[projectileObjIdx].mobj->moveVectorDirty = 1;
						g_objectTable[projectileObjIdx].typeSpecificWord = 1;
					}
				}
			} else {
				result = 271;
				{
					CraftData* lensCraft = g_objectTable[reactorObjIdx].mobj->pCraft;
					unsigned int hullDamage = lensCraft->hullDamage;
					if (hullDamage <= ((unsigned int)lensCraft->hullMax >> 1)) {
						MobileObject* pm = g_objectTable[projectileObjIdx].mobj;
						unsigned int dmg =
							pm ? (unsigned int)pm->damageAmount
							   : g_modelTypeTable[(uint16_t)g_objectTable[projectileObjIdx].objectType]
									 .maxBoundsExtent;
						lensCraft->hullDamage = hullDamage + dmg;
						if ((unsigned int)g_objectTable[reactorObjIdx].mobj->pCraft->hullDamage >
							((unsigned int)g_objectTable[reactorObjIdx].mobj->pCraft->hullMax >> 1))
							g_objectTable[reactorObjIdx].mobj->nodeSwitchIndex = 1;
					}
				}
			}
			break;
		default:
			if (objectType == OBJ_DSLaserInternal)
				result = (g_objectTable[projectileObjIdx].typeSpecificWord != 1) ? 0 : 266;
			break;
	}

	if ((uint16_t)result) {
		ModelGenusId genusId = g_objectTable[projectileObjIdx].genusId;
		if (genusId == GENUS_PlayerProjectile || genusId == GENUS_NpcProjectile) {
			collide_ConvertObjectToExplosion(projectileObjIdx, (ObjectTypeId)result, 1);
			g_objectTable[projectileObjIdx].world_x = g_collisionSegmentStartWorldX + g_collisionHitOffsetX;
			g_objectTable[projectileObjIdx].world_y = g_collisionSegmentStartWorldY + g_collisionHitOffsetY;
			g_objectTable[projectileObjIdx].world_z = g_collisionSegmentStartWorldZ + g_collisionHitOffsetZ;
			g_objectTable[projectileObjIdx].mobj->speed = 0;
			g_objectTable[projectileObjIdx].mobj->speedRemainder = 0;
		}
	}
}

// FUNCTION: XWA 0x424820
void DeathStar_UpdateFollowOverrideCraft(int objectIdx) {
	int followSlotIdx;

	for (followSlotIdx = 0; followSlotIdx < 10; ++followSlotIdx) {
		if (g_deathStarFollowChainSlots[followSlotIdx].objectIdx == objectIdx &&
			g_deathStarFollowChainSlots[followSlotIdx].objectSignature ==
				g_objectTable[objectIdx].objectSignature) {
			break;
		}
	}

	if (followSlotIdx < 10) {
		DeathStarFollowChainSlot* slot = &g_deathStarFollowChainSlots[followSlotIdx];
		double spacingRatio;
		int elapsed;
		int accumTime;
		int sampleIdx;
		int sampleIdxA;
		int sampleIdxB;
		int refreshTimer;

		{
			double desiredSpacing = (double)slot->desiredSpacing;

			spacingRatio = (double)slot->pathDistance / desiredSpacing;
		}
		if (spacingRatio > 8.0) {
			spacingRatio = 8.0;
		}

		elapsed = (int)(uint16_t)g_elapsedTicks;
		slot->pathDistance += elapsed - (int)((double)elapsed * spacingRatio);
		if (slot->pathDistance < 0) {
			slot->pathDistance = 0;
		}
		if ((uint16_t)g_elapsedTicks > 200u) {
			slot->pathDistance += elapsed;
		}

		accumTime = g_deathStarTunnelTimer - g_deathStarPathSampleLastTime;
		sampleIdx = g_deathStarPathSampleWriteIdx - 1;
		if (sampleIdx < 0) {
			sampleIdx += 30;
		}

		sampleIdxA = sampleIdx;
		sampleIdxB = sampleIdx;
		if (sampleIdx != g_deathStarPathSampleWriteIdx) {
			do {
				sampleIdxA = sampleIdxB - 1;
				if (sampleIdxA < 0) {
					sampleIdxA += 30;
				}
				accumTime += g_deathStarPathSamples[sampleIdxB].elapsedTicksSincePrev;
				if (accumTime > slot->pathDistance) {
					break;
				}
				sampleIdxB = sampleIdxA;
			} while (sampleIdxA != g_deathStarPathSampleWriteIdx);
		}

		{
			unsigned int ticksIntoSample = (unsigned int)(accumTime - slot->pathDistance);

			DeathStar_InterpolateFollowCraftOnPath(objectIdx, sampleIdxA, sampleIdxB, ticksIntoSample, slot);
		}
		DeathStar_ApplyFollowSeparationOffset(objectIdx, followSlotIdx);

		if (g_paiContext.aiController->targetObjIdx != 0xffff) {
			g_paiContext.aiController->aiPlanState -= (uint16_t)g_elapsedTicks;
			if (g_paiContext.aiController->aiPlanState <= 0) {
				g_paiContext.aiController->targetComponent = 0xffff;
				g_paiContext.aiController->aimPointX =
					g_objectTable[g_paiContext.aiController->targetObjIdx].world_x;
				g_paiContext.aiController->aimPointY =
					g_objectTable[g_paiContext.aiController->targetObjIdx].world_y;
				g_paiContext.aiController->aimPointZ =
					g_objectTable[g_paiContext.aiController->targetObjIdx].world_z;
				g_paiContext.aiController->hasLiveTarget = 0;
				paifight_fightershootorder();
				g_paiContext.aiController->aiPlanState =
					20 * g_deathStarPathSamples[sampleIdxA].tacticalIndex;
			}
		}

		refreshTimer = slot->refreshTimer;
		if (refreshTimer != 0) {
			refreshTimer -= (uint16_t)g_elapsedTicks;
			slot->refreshTimer = refreshTimer;
			if (refreshTimer <= 0) {
				slot->refreshTimer = 0;
				paiman_RefreshDeathStarPlayerFollow(slot->objectIdx, g_localPlayer);
				g_objectTable[slot->objectIdx].mobj->speed = 80;

				if (followSlotIdx < 9) {
					int refreshSlotIdx;

#ifdef XWA_MODERN
					memmove(slot, &g_deathStarFollowChainSlots[followSlotIdx + 1],
							(size_t)(9 - followSlotIdx) * sizeof(*slot));
#else
					memcpy(slot, &g_deathStarFollowChainSlots[followSlotIdx + 1],
						   (size_t)(9 - followSlotIdx) * sizeof(*slot));
#endif
					refreshSlotIdx = followSlotIdx;
					while (refreshSlotIdx < 9) {
						if (g_deathStarFollowChainSlots[refreshSlotIdx].objectIdx != 0xFFFF &&
							g_objectTable[g_deathStarFollowChainSlots[refreshSlotIdx].objectIdx].objectType !=
								OBJ_None) {
							DeathStar_UpdateFollowChainSlot(refreshSlotIdx);
						}
						++refreshSlotIdx;
					}
				}

				g_deathStarFollowChainSlots[9].objectIdx = 0xffff;
			}
		}
	}

	if (followSlotIdx == 10) {
		const DeathStarPathSample* targetSample;
		ObjectRecord* object;
		int deltaX;
		int deltaY;
		int deltaZ;

		object = &g_objectTable[objectIdx];
		targetSample = &g_deathStarPathSamples[g_deathStarPathSampleWriteIdx];
		deltaX = targetSample->worldX - object->world_x;
		deltaY = targetSample->worldY - object->world_y;
		deltaZ = targetSample->worldZ - object->world_z;

		if (collide_roughdistance3d(deltaX, deltaY, deltaZ) < 800) {
			object->mobj->pCraft->objectKind = DEATH_STAR_CRAFT_KIND_NORMAL;
			DeathStar_AddFollowChainSlot(objectIdx);
			return;
		}

		trig2_ctop(deltaX, deltaY, deltaZ);
		object->yaw = trig2_xyangle;
		object->pitch = targetPitch;
		object->mobj->orientMatrixDirty = 1;
		object->mobj->moveVectorDirty = 1;
		object->mobj->speed =
			3 *
			g_modelDefs[(uint16_t)g_modelTypeTable[(uint16_t)g_deathStarFollowLeaderObjectType].modelIndex]
				.maxSpeed;
		object->mobj->pCraft->objectKind = 9;
	}
}

static void DeathStar_WriteSegmentSetFilmState(void) {
	int segmentSetIdx;
	uint16_t segmentIdx;

	for (segmentSetIdx = 0; segmentSetIdx < 8; ++segmentSetIdx) {
		const DeathStarSegmentSet* segmentSet;
		DeathStarFilmSegmentSetState state;

		segmentSet = &g_deathStarSegmentSets[segmentSetIdx];
		state.count = segmentSet->count;
		state.baseYaw = segmentSet->baseYaw;
		state.basePitch = segmentSet->basePitch;
		state.fixedSegmentObjectType = segmentSet->fixedSegmentObjectType;
		Film_WriteBytesBuffered(&state, sizeof(state));
	}

	for (segmentSetIdx = 0; segmentSetIdx < 8; ++segmentSetIdx) {
		const DeathStarSegmentSet* segmentSet;

		segmentSet = &g_deathStarSegmentSets[segmentSetIdx];
		for (segmentIdx = 0; segmentIdx < segmentSet->count; ++segmentIdx) {
			const DeathStarSegmentDef* segment;
			DeathStarFilmSegmentDefState state;

			segment = &segmentSet->segments[segmentIdx];
			state.objectType = segment->objectType;
			state.flags = segment->flags;
			state.worldX = segment->worldX;
			state.worldY = segment->worldY;
			state.worldZ = segment->worldZ;
			state.yaw = segment->yaw;
			state.pitch = segment->pitch;
			state.nextSegmentSet = segment->nextSegmentSet;
			state.nextSegmentIdx = segment->nextSegmentIdx;
			state.activeSegmentCount = segment->activeSegmentCount;
			state.updateFnId = DeathStar_GetSegmentUpdateFnId(segment->updateFn);
			memcpy(state.childObjects, segment->childObjects, sizeof(state.childObjects));
			Film_WriteBytesBuffered(&state, sizeof(state));
		}
	}
}

static void DeathStar_ReadSegmentSetFilmState(void) {
	DeathStarFilmSegmentSetState setStates[8];
	int segmentSetIdx;
	uint16_t segmentIdx;

	for (segmentSetIdx = 0; segmentSetIdx < 8; ++segmentSetIdx) {
		Film_ReadBytes(&setStates[segmentSetIdx], sizeof(setStates[segmentSetIdx]));
	}

	for (segmentSetIdx = 0; segmentSetIdx < 8; ++segmentSetIdx) {
		DeathStarSegmentSet* segmentSet;
		const DeathStarFilmSegmentSetState* state;

		segmentSet = &g_deathStarSegmentSets[segmentSetIdx];
		state = &setStates[segmentSetIdx];
		if (segmentSet->segments == NULL || segmentSet->count != state->count) {
			if (segmentSet->segments != NULL) {
				Memory_FreeTagged("DSSEGMENT", segmentSet->segments);
			}
			segmentSet->segments = NULL;
			if (state->count != 0) {
				segmentSet->segments = (DeathStarSegmentDef*)Memory_AllocTagged(
					"DSSEGMENT", sizeof(DeathStarSegmentDef) * state->count);
			}
		}

		segmentSet->count = state->count;
		segmentSet->baseYaw = state->baseYaw;
		segmentSet->basePitch = state->basePitch;
		segmentSet->fixedSegmentObjectType = state->fixedSegmentObjectType;
		segmentSet->rules = g_deathStarSegmentSetInitializers[segmentSetIdx].rules;
	}
	DeathStar_InstallSegmentRuleCallbacks();

	for (segmentSetIdx = 0; segmentSetIdx < 8; ++segmentSetIdx) {
		DeathStarSegmentSet* segmentSet;

		segmentSet = &g_deathStarSegmentSets[segmentSetIdx];
		for (segmentIdx = 0; segmentIdx < segmentSet->count; ++segmentIdx) {
			DeathStarFilmSegmentDefState state;
			DeathStarSegmentDef* segment;

			Film_ReadBytes(&state, sizeof(state));
			segment = &segmentSet->segments[segmentIdx];
			segment->objectType = state.objectType;
			segment->flags = state.flags;
			segment->worldX = state.worldX;
			segment->worldY = state.worldY;
			segment->worldZ = state.worldZ;
			segment->yaw = state.yaw;
			segment->pitch = state.pitch;
			segment->nextSegmentSet = state.nextSegmentSet;
			segment->nextSegmentIdx = state.nextSegmentIdx;
			segment->activeSegmentCount = state.activeSegmentCount;
			segment->updateFn = DeathStar_GetSegmentUpdateFnById(state.updateFnId);
			memcpy(segment->childObjects, state.childObjects, sizeof(segment->childObjects));
		}
	}
}

static void DeathStar_WriteMissionWaypointFilmState(void) {
	int flightGroupIdx;

	Film_WriteBytesBuffered(
		&g_missionFlightGroups[g_deathStarBentTubeGrayFgIdx].fg.missionPoints[XWA_FG_POINT_START_1],
		sizeof(XwaWaypoint));
	Film_WriteBytesBuffered(
		&g_missionFlightGroups[g_deathStarBentTubeBlueFgIdx].fg.missionPoints[XWA_FG_POINT_START_1],
		sizeof(XwaWaypoint));

	for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups; ++flightGroupIdx) {
		XwaFlightGroup* fg;

		fg = &g_missionFlightGroups[flightGroupIdx].fg;
		if (g_objectTypeTables.craftTypeToObjectType[fg->craftType] == OBJ_ZeroGStormtrooper) {
			Film_WriteBytesBuffered(&fg->orders[0].waypoints[0], sizeof(XwaWaypoint));
			Film_WriteBytesBuffered(&fg->orders[0].waypoints[1], sizeof(XwaWaypoint));
		}
	}

	Film_WriteBytesBuffered(&g_missionFlightGroups[g_deathStarReactorAssaultFgIdx].fg.orders[3].waypoints[0],
							sizeof(XwaWaypoint));
	Film_WriteBytesBuffered(&g_missionFlightGroups[g_deathStarReactorAssaultFgIdx].fg.orders[1].waypoints[0],
							sizeof(XwaWaypoint) * 8u);
	Film_WriteBytesBuffered(
		&g_missionFlightGroups[g_deathStarReactorAssaultFgIdx].fg.missionPoints[XWA_FG_POINT_HYPER],
		sizeof(XwaWaypoint));
}

static void DeathStar_ReadMissionWaypointFilmState(void) {
	int flightGroupIdx;

	Film_ReadBytes(
		&g_missionFlightGroups[g_deathStarBentTubeGrayFgIdx].fg.missionPoints[XWA_FG_POINT_START_1],
		sizeof(XwaWaypoint));
	Film_ReadBytes(
		&g_missionFlightGroups[g_deathStarBentTubeBlueFgIdx].fg.missionPoints[XWA_FG_POINT_START_1],
		sizeof(XwaWaypoint));

	for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups; ++flightGroupIdx) {
		XwaFlightGroup* fg;

		fg = &g_missionFlightGroups[flightGroupIdx].fg;
		if (g_objectTypeTables.craftTypeToObjectType[fg->craftType] == OBJ_ZeroGStormtrooper) {
			Film_ReadBytes(&fg->orders[0].waypoints[0], sizeof(XwaWaypoint));
			Film_ReadBytes(&fg->orders[0].waypoints[1], sizeof(XwaWaypoint));
		}
	}

	Film_ReadBytes(&g_missionFlightGroups[g_deathStarReactorAssaultFgIdx].fg.orders[3].waypoints[0],
				   sizeof(XwaWaypoint));
	Film_ReadBytes(&g_missionFlightGroups[g_deathStarReactorAssaultFgIdx].fg.orders[1].waypoints[0],
				   sizeof(XwaWaypoint) * 8u);
	Film_ReadBytes(
		&g_missionFlightGroups[g_deathStarReactorAssaultFgIdx].fg.missionPoints[XWA_FG_POINT_HYPER],
		sizeof(XwaWaypoint));
}

// FUNCTION: XWA 0x424C50
void DeathStar_WriteFilmStateBlock(void) {
#define DEATH_STAR_WRITE_FIELD(field) Film_WriteBytesBuffered(&(field), sizeof(field))
#define DEATH_STAR_WRITE_ARRAY(array) Film_WriteBytesBuffered((array), sizeof(array))

	DEATH_STAR_WRITE_FIELD(g_deathStarTunnelFilmStateReserved0);
	DEATH_STAR_WRITE_FIELD(g_deathStarTunnelTimer);
	DEATH_STAR_WRITE_FIELD(g_deathStarCurrentSegmentIdx.value);
	DEATH_STAR_WRITE_FIELD(g_deathStarSegmentSetIdx);
	DEATH_STAR_WRITE_ARRAY(g_deathStarActiveSegmentObjIdx);
	DEATH_STAR_WRITE_ARRAY(g_deathStarActiveSegmentIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarActiveSegmentCount);
	DEATH_STAR_WRITE_ARRAY(g_deathStarObjectPointTables);
	DEATH_STAR_WRITE_FIELD(g_deathStarPlayerObjIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorAssaultCraftObjIdx);
	DEATH_STAR_WRITE_ARRAY(g_deathStarLoopSfxActive);
	DEATH_STAR_WRITE_ARRAY(g_deathStarLoopSfxVolume);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCoreRoomFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarActiveSegmentPlaceholderFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarDefaultScriptedObjectFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarGeneratedObjectFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCoreFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarTripodGunFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarFocusChamberFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarTankLightsFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarBentTubeGrayFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarBentTubeRedFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarTankPipeRedFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarFocusLensFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarTankPipeBlueFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarBentTubeBlueFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorAssaultFgIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarRandomChildObjectLimit);
	DEATH_STAR_WRITE_FIELD(g_deathStarSegmentChildInitialHitCount);
	DEATH_STAR_WRITE_FIELD(g_deathStarPathSampleWriteIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarPathSampleLastTime);
	DEATH_STAR_WRITE_ARRAY(g_deathStarPathSamples);
	DEATH_STAR_WRITE_FIELD(g_deathStarFollowLeaderExtentX4);
	DEATH_STAR_WRITE_ARRAY(g_deathStarFollowChainSlots);
	DEATH_STAR_WRITE_FIELD(g_deathStarFollowRefreshPending);
	DEATH_STAR_WRITE_FIELD(g_deathStarFollowChainLastValidateTime);
	DEATH_STAR_WRITE_FIELD(g_deathStarFollowLeaderObjIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarFollowLeaderObjectType);
	DEATH_STAR_WRITE_FIELD(g_deathStarLastGeneratedRandomSegmentType);
	DEATH_STAR_WRITE_FIELD(g_deathStarFollowBaseDesiredSpacing);
	DEATH_STAR_WRITE_FIELD(g_deathStarEntranceTransitionState);
	DEATH_STAR_WRITE_FIELD(g_deathStarEntranceProximityArmed);
	DEATH_STAR_WRITE_FIELD(g_deathStarEntranceTransitionTimer);
	DEATH_STAR_WRITE_FIELD(g_deathStarAccelChamberLightTimer);
	DEATH_STAR_WRITE_FIELD(g_deathStarContainerCollisionLightTimer);
	DEATH_STAR_WRITE_FIELD(g_deathStarAccelChamberLastContainerSpawnTime);
	DEATH_STAR_WRITE_FIELD(g_deathStarAccelChamberPitchOffset);
	DEATH_STAR_WRITE_FIELD(g_deathStarAccelChamberContainersCleared);
	DEATH_STAR_WRITE_FIELD(g_deathStarAccelChamberContainerSpawnInterval);
	DEATH_STAR_WRITE_FIELD(g_deathStarFocusChamberObjIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarFocusChamberSegmentIdx.value);
	DEATH_STAR_WRITE_FIELD(g_deathStarTripodGunObjIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarTripodGunsActivated);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCoreRoomObjIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCoreObjIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCylinderObjIdx);
	DEATH_STAR_WRITE_ARRAY(g_deathStarReactorShockwaveObjIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCylinderAnimTimer);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorReservedFilmState);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCoreDriftSpeed);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCoreRoomSegmentIdx.value);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorDestructionTimer);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCoreDriftDirX);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCoreDriftDirY);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorCoreDriftDirZ);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorExplosionOriginX);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorExplosionOriginY);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorExplosionOriginZ);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorShockwaveSpeed);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorShockwaveDistance);
	DEATH_STAR_WRITE_FIELD(g_deathStarReactorExplosionSpawnCount);
	DEATH_STAR_WRITE_FIELD(g_deathStarTunnelBillboardScale);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserCooldownTimer);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserFireTimer);
	DEATH_STAR_WRITE_ARRAY(g_deathStarLaserEffectSlots);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserEffectSlotCount);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserChamberX);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserChamberY);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserChamberZ);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserChamberDirX);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserChamberDirY);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserChamberDirZ);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserPowerSourceObjIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarPowerSourceLinkedFocusLensObjIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserGlowExtent);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserChamberSegmentSetIdx);
	DEATH_STAR_WRITE_FIELD(g_deathStarLaserChamberSegmentIdx);
	DeathStar_WriteSegmentSetFilmState();
	DeathStar_WriteMissionWaypointFilmState();

#undef DEATH_STAR_WRITE_ARRAY
#undef DEATH_STAR_WRITE_FIELD
}

// FUNCTION: XWA 0x425490
void DeathStar_ReadFilmStateBlock(void) {
#define DEATH_STAR_READ_FIELD(field) Film_ReadBytes(&(field), sizeof(field))
#define DEATH_STAR_READ_ARRAY(array) Film_ReadBytes((array), sizeof(array))

	DEATH_STAR_READ_FIELD(g_deathStarTunnelFilmStateReserved0);
	DEATH_STAR_READ_FIELD(g_deathStarTunnelTimer);
	DEATH_STAR_READ_FIELD(g_deathStarCurrentSegmentIdx.value);
	DEATH_STAR_READ_FIELD(g_deathStarSegmentSetIdx);
	DEATH_STAR_READ_ARRAY(g_deathStarActiveSegmentObjIdx);
	DEATH_STAR_READ_ARRAY(g_deathStarActiveSegmentIdx);
	DEATH_STAR_READ_FIELD(g_deathStarActiveSegmentCount);
	DEATH_STAR_READ_ARRAY(g_deathStarObjectPointTables);
	DEATH_STAR_READ_FIELD(g_deathStarPlayerObjIdx);
	DEATH_STAR_READ_FIELD(g_deathStarReactorAssaultCraftObjIdx);
	DEATH_STAR_READ_ARRAY(g_deathStarLoopSfxActive);
	DEATH_STAR_READ_ARRAY(g_deathStarLoopSfxVolume);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCoreRoomFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarActiveSegmentPlaceholderFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarDefaultScriptedObjectFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarGeneratedObjectFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCoreFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarTripodGunFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarFocusChamberFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarTankLightsFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarBentTubeGrayFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarBentTubeRedFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarTankPipeRedFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarFocusLensFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarTankPipeBlueFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarBentTubeBlueFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarReactorAssaultFgIdx);
	DEATH_STAR_READ_FIELD(g_deathStarRandomChildObjectLimit);
	DEATH_STAR_READ_FIELD(g_deathStarSegmentChildInitialHitCount);
	DEATH_STAR_READ_FIELD(g_deathStarPathSampleWriteIdx);
	DEATH_STAR_READ_FIELD(g_deathStarPathSampleLastTime);
	DEATH_STAR_READ_ARRAY(g_deathStarPathSamples);
	DEATH_STAR_READ_FIELD(g_deathStarFollowLeaderExtentX4);
	DEATH_STAR_READ_ARRAY(g_deathStarFollowChainSlots);
	DEATH_STAR_READ_FIELD(g_deathStarFollowRefreshPending);
	DEATH_STAR_READ_FIELD(g_deathStarFollowChainLastValidateTime);
	DEATH_STAR_READ_FIELD(g_deathStarFollowLeaderObjIdx);
	DEATH_STAR_READ_FIELD(g_deathStarFollowLeaderObjectType);
	DEATH_STAR_READ_FIELD(g_deathStarLastGeneratedRandomSegmentType);
	DEATH_STAR_READ_FIELD(g_deathStarFollowBaseDesiredSpacing);
	DEATH_STAR_READ_FIELD(g_deathStarEntranceTransitionState);
	DEATH_STAR_READ_FIELD(g_deathStarEntranceProximityArmed);
	DEATH_STAR_READ_FIELD(g_deathStarEntranceTransitionTimer);
	DEATH_STAR_READ_FIELD(g_deathStarAccelChamberLightTimer);
	DEATH_STAR_READ_FIELD(g_deathStarContainerCollisionLightTimer);
	DEATH_STAR_READ_FIELD(g_deathStarAccelChamberLastContainerSpawnTime);
	DEATH_STAR_READ_FIELD(g_deathStarAccelChamberPitchOffset);
	DEATH_STAR_READ_FIELD(g_deathStarAccelChamberContainersCleared);
	DEATH_STAR_READ_FIELD(g_deathStarAccelChamberContainerSpawnInterval);
	DEATH_STAR_READ_FIELD(g_deathStarFocusChamberObjIdx);
	DEATH_STAR_READ_FIELD(g_deathStarFocusChamberSegmentIdx.value);
	DEATH_STAR_READ_FIELD(g_deathStarTripodGunObjIdx);
	DEATH_STAR_READ_FIELD(g_deathStarTripodGunsActivated);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCoreRoomObjIdx);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCoreObjIdx);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCylinderObjIdx);
	DEATH_STAR_READ_ARRAY(g_deathStarReactorShockwaveObjIdx);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCylinderAnimTimer);
	DEATH_STAR_READ_FIELD(g_deathStarReactorReservedFilmState);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCoreDriftSpeed);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCoreRoomSegmentIdx.value);
	DEATH_STAR_READ_FIELD(g_deathStarReactorDestructionTimer);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCoreDriftDirX);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCoreDriftDirY);
	DEATH_STAR_READ_FIELD(g_deathStarReactorCoreDriftDirZ);
	DEATH_STAR_READ_FIELD(g_deathStarReactorExplosionOriginX);
	DEATH_STAR_READ_FIELD(g_deathStarReactorExplosionOriginY);
	DEATH_STAR_READ_FIELD(g_deathStarReactorExplosionOriginZ);
	DEATH_STAR_READ_FIELD(g_deathStarReactorShockwaveSpeed);
	DEATH_STAR_READ_FIELD(g_deathStarReactorShockwaveDistance);
	DEATH_STAR_READ_FIELD(g_deathStarReactorExplosionSpawnCount);
	DEATH_STAR_READ_FIELD(g_deathStarTunnelBillboardScale);
	DEATH_STAR_READ_FIELD(g_deathStarLaserCooldownTimer);
	DEATH_STAR_READ_FIELD(g_deathStarLaserFireTimer);
	DEATH_STAR_READ_ARRAY(g_deathStarLaserEffectSlots);
	DEATH_STAR_READ_FIELD(g_deathStarLaserEffectSlotCount);
	DEATH_STAR_READ_FIELD(g_deathStarLaserChamberX);
	DEATH_STAR_READ_FIELD(g_deathStarLaserChamberY);
	DEATH_STAR_READ_FIELD(g_deathStarLaserChamberZ);
	DEATH_STAR_READ_FIELD(g_deathStarLaserChamberDirX);
	DEATH_STAR_READ_FIELD(g_deathStarLaserChamberDirY);
	DEATH_STAR_READ_FIELD(g_deathStarLaserChamberDirZ);
	DEATH_STAR_READ_FIELD(g_deathStarLaserPowerSourceObjIdx);
	DEATH_STAR_READ_FIELD(g_deathStarPowerSourceLinkedFocusLensObjIdx);
	DEATH_STAR_READ_FIELD(g_deathStarLaserGlowExtent);
	DEATH_STAR_READ_FIELD(g_deathStarLaserChamberSegmentSetIdx);
	DEATH_STAR_READ_FIELD(g_deathStarLaserChamberSegmentIdx);
	DeathStar_ReadSegmentSetFilmState();
	DeathStar_ReadMissionWaypointFilmState();

#undef DEATH_STAR_READ_ARRAY
#undef DEATH_STAR_READ_FIELD
}

// FUNCTION: XWA 0x425DA0
void DeathStar_InitFollowOverrideState(void) {
	int sampleIdx;
	int sampleWorldY;
	uint16_t slotIdx;
	uint32_t objectIdx;

	memset(&g_deathStarPathSampleWriteIdx, 0,
		   sizeof(g_deathStarPathSampleWriteIdx) + sizeof(g_deathStarPathSampleLastTime) +
			   sizeof(g_deathStarPathSamples));

	g_deathStarPathSampleLastTime = g_deathStarTunnelTimer;
	g_deathStarFollowChainLastValidateTime = g_deathStarTunnelTimer;
	g_deathStarPathSampleWriteIdx = 0;
	g_deathStarFollowLeaderObjIdx = g_deathStarPlayerObjIdx;
	sampleWorldY = 0;
	g_deathStarFollowLeaderObjectType = g_objectTable[g_deathStarPlayerObjIdx].objectType;
	g_deathStarFollowLeaderExtentX4 = 4 * g_modelTypeTable[g_deathStarFollowLeaderObjectType].maxBoundsExtent;
	g_deathStarFollowBaseDesiredSpacing = 30 * (int)g_flightDifficulty + 110;

	for (sampleIdx = 29; sampleIdx >= 0; --sampleIdx) {
		DeathStarPathSample* sample;

		sampleWorldY -= 500;
		sample = &g_deathStarPathSamples[sampleIdx];
		sample->elapsedTicksSincePrev = 50;
		sample->worldX = 0;
		sample->worldY = sampleWorldY;
		sample->worldZ = 0;
	}

	for (slotIdx = 0; slotIdx < 10; ++slotIdx) {
		g_deathStarFollowChainSlots[slotIdx].objectIdx = 0xffff;
	}

	objectIdx = g_activeRegionObjectSlotStart;
	g_deathStarFollowRefreshPending = 0;
	while (objectIdx < g_activeRegionCraftObjectSlotEnd) {
		if (g_objectTable[objectIdx].objectType != OBJ_None &&
			objectIdx != (unsigned int)g_deathStarFollowLeaderObjIdx &&
			(g_objectTable[objectIdx].genusId == GENUS_Fighter ||
			 g_objectTable[objectIdx].genusId == GENUS_Transport)) {
			paiman_BeginPlayerFollowOverride(objectIdx, g_localPlayer);
		}
		++objectIdx;
	}

	g_deathStarFollowRefreshPending = 1;
}

// FUNCTION: XWA 0x425ED0
void DeathStar_SpawnScriptedObjects(void) {
	DeathStarScriptedObjectDef* def;
	const uint16_t mobileAllocationMode = 1;

	def = g_deathStarScriptedObjectDefs;
	if (def->objectType == OBJ_None) {
		return;
	}

	do {
		unsigned int objIdx;
		ObjectRecord* obj;
		DeathStarSegmentDef* segment;
		DeathStarSegmentDef relativeSegment;
		DeathStarSegmentDef modelOriginSegment;
		int byteIdx;
		int offsetX;
		int offsetY;
		int offsetZ;
		int16_t segmentIdx;
		uint16_t segmentSetIdx;

		if (def->allocationMode == mobileAllocationMode) {
			objIdx = Object_AllocSlotForGenus(g_modelTypeTable[def->objectType].genusId);
		} else {
			objIdx = Object_FindFreeMissionSlot();
		}
		if (objIdx == 0xffffu) {
			++def;
			continue;
		}

		def->spawnedObjIdx = (uint16_t)objIdx;
		obj = &g_objectTable[objIdx];
		obj->flightGroupIdx = g_deathStarDefaultScriptedObjectFgIdx;
		obj->regionIdx = 0;
		obj->roll = 0;
		obj->angleD = 0;
		obj->typeSpecificWord = 0;
		for (byteIdx = 0; byteIdx < 2; ++byteIdx) {
			obj->typeSpecificByte[byteIdx] = 0;
		}
		obj->playerOwnerIdx = -1;
		if (def->allocationMode == mobileAllocationMode) {
			DeathStar_InitMobileObjectForType(def->objectType, objIdx);
		} else {
			obj->mobj = NULL;
		}

		{
			uint16_t objectType;

			objectType = def->objectType;
			obj->objectType = objectType;
			obj->genusId = g_modelTypeTable[objectType].genusId;
		}
		obj->yaw = def->yaw;
		obj->pitch = def->pitch;

		segmentSetIdx = def->segmentSetIdx;
		segmentIdx = def->segmentIdx;
		if (segmentSetIdx == 8) {
			const ObjectRecord* sourceObj;

			sourceObj = &g_objectTable[(int16_t)g_deathStarScriptedObjectDefs[segmentIdx].spawnedObjIdx];
			relativeSegment.objectType = sourceObj->objectType;
			relativeSegment.yaw = sourceObj->yaw;
			relativeSegment.pitch = sourceObj->pitch;
			relativeSegment.worldX = sourceObj->world_x;
			relativeSegment.worldY = sourceObj->world_y;
			relativeSegment.worldZ = sourceObj->world_z;
			segment = &relativeSegment;
		} else {
			if (segmentIdx < 0) {
				segmentIdx = (int16_t)(segmentIdx + g_deathStarSegmentSets[segmentSetIdx].count);
			}
			segment = &g_deathStarSegmentSets[def->segmentSetIdx].segments[segmentIdx];
		}

		DeathStar_ComputeSegmentPointOffset(segment, def->segmentPointIdx, (int)def->segmentPointKind,
											&offsetX, &offsetY, &offsetZ);
		obj->world_x = segment->worldX + offsetX;
		obj->world_y = segment->worldY + offsetY;
		obj->world_z = segment->worldZ + offsetZ;
		obj->yaw = (Q16Angle)(obj->yaw + segment->yaw);
		obj->pitch = (Q16Angle)(obj->pitch + segment->pitch);

		if (def->objectType >= OBJ_DSReactorCore && def->objectType < OBJ_ChuteMouth) {
			modelOriginSegment.objectType = def->objectType;
			modelOriginSegment.pitch = obj->pitch;
			modelOriginSegment.yaw = obj->yaw;
			DeathStar_ComputeSegmentPointOffset(&modelOriginSegment, def->modelOriginPointIdx,
												(int)def->modelOriginPointKind, &offsetX, &offsetY, &offsetZ);
			obj->world_x -= offsetX;
			obj->world_y -= offsetY;
			obj->world_z -= offsetZ;
		}

		switch (def->objectType) {
			case OBJ_DS3rdRoom:
				obj->flightGroupIdx = g_deathStarFocusChamberFgIdx;
				g_deathStarFocusChamberObjIdx = (int16_t)def->spawnedObjIdx;
				g_deathStarFocusChamberSegmentIdx.value = (uint16_t)def->segmentIdx;
				if (def->segmentIdx < 0) {
					g_deathStarFocusChamberSegmentIdx.value =
						(uint16_t)(g_deathStarFocusChamberSegmentIdx.value +
								   g_deathStarSegmentSets[def->segmentSetIdx].count);
				}
				break;

			case OBJ_DSTripodGun: {
				const int spawnedObjIdx = (int16_t)def->spawnedObjIdx;

				g_currentFlightGroupIdx = (uint8_t)g_deathStarTripodGunFgIdx;
				g_spawnGenusId = g_modelTypeTable[OBJ_DSTripodGun].genusId;
				g_deathStarTripodGunObjIdx = spawnedObjIdx;
				g_spawnStatus1 = g_missionFlightGroups[(uint8_t)g_deathStarTripodGunFgIdx].fg.status1;
				g_spawnStatus2 = g_missionFlightGroups[(uint8_t)g_deathStarTripodGunFgIdx].fg.status2;
				Mission_InitFlightGroupObjectSlot(OBJ_DSTripodGun, (ObjectIndex)spawnedObjIdx);
				pai_setupcraftcontext(spawnedObjIdx);
				pai_ApplyPendingPlanTargetAndManeuver((unsigned int)spawnedObjIdx);
				g_objectTable[spawnedObjIdx].mobj->ejectionSpawnCount = 0;
				break;
			}

			case OBJ_DSTankwlights:
				obj->flightGroupIdx = g_deathStarTankLightsFgIdx;
				break;

			case OBJ_DSBentTubeGray: {
				g_missionFlightGroups[(uint8_t)g_deathStarBentTubeGrayFgIdx]
					.fg.missionPoints[XWA_FG_POINT_START_1]
					.x = (int16_t)(obj->world_x >> 8);
				g_missionFlightGroups[(uint8_t)g_deathStarBentTubeGrayFgIdx]
					.fg.missionPoints[XWA_FG_POINT_START_1]
					.y = (int16_t)((-obj->world_y) >> 8);
				g_missionFlightGroups[(uint8_t)g_deathStarBentTubeGrayFgIdx]
					.fg.missionPoints[XWA_FG_POINT_START_1]
					.z = (int16_t)(obj->world_x >> 8);
				obj->objectType = OBJ_None;
				break;
			}

			case OBJ_DSBentTubeRed:
				obj->flightGroupIdx = g_deathStarBentTubeRedFgIdx;
				break;

			case OBJ_DSTankwPipeRed01:
				obj->flightGroupIdx = g_deathStarTankPipeRedFgIdx;
				break;

			case OBJ_DSTankwPipeBlue01:
				obj->flightGroupIdx = g_deathStarTankPipeBlueFgIdx;
				break;

			case OBJ_DSBentTubeBlue01: {
				g_missionFlightGroups[(uint8_t)g_deathStarBentTubeBlueFgIdx]
					.fg.missionPoints[XWA_FG_POINT_START_1]
					.x = (int16_t)(obj->world_x >> 8);
				g_missionFlightGroups[(uint8_t)g_deathStarBentTubeBlueFgIdx]
					.fg.missionPoints[XWA_FG_POINT_START_1]
					.y = (int16_t)((-obj->world_y) >> 8);
				g_missionFlightGroups[(uint8_t)g_deathStarBentTubeBlueFgIdx]
					.fg.missionPoints[XWA_FG_POINT_START_1]
					.z = (int16_t)(obj->world_x >> 8);
				obj->objectType = OBJ_None;
				break;
			}

			case OBJ_DSReactorCoreRoom:
				g_deathStarReactorCoreRoomObjIdx = (int16_t)def->spawnedObjIdx;
				obj->flightGroupIdx = g_deathStarReactorCoreRoomFgIdx;
				break;

			case OBJ_DSReactorCore: {
				const int spawnedObjIdx = (int16_t)def->spawnedObjIdx;

				g_currentFlightGroupIdx = (uint8_t)g_deathStarReactorCoreFgIdx;
				g_spawnGenusId = g_modelTypeTable[OBJ_DSReactorCore].genusId;
				g_deathStarReactorCoreObjIdx = spawnedObjIdx;
				g_spawnStatus1 = g_missionFlightGroups[(uint8_t)g_deathStarReactorCoreFgIdx].fg.status1;
				g_spawnStatus2 = g_missionFlightGroups[(uint8_t)g_deathStarReactorCoreFgIdx].fg.status2;
				Mission_InitFlightGroupObjectSlot(OBJ_DSReactorCore, (ObjectIndex)spawnedObjIdx);
				pai_setupcraftcontext(spawnedObjIdx);
				pai_ApplyPendingPlanTargetAndManeuver((unsigned int)spawnedObjIdx);
				g_objectTable[spawnedObjIdx].mobj->ejectionSpawnCount = 0;
				break;
			}

			case OBJ_DSReactorCylinder:
				if (g_useHardware3D) {
					g_deathStarReactorCylinderObjIdx = (int16_t)def->spawnedObjIdx;
					g_objectTable[(int16_t)def->spawnedObjIdx].mobj->nodeSwitchIndex = 0;
				} else {
					g_objectTable[(int16_t)def->spawnedObjIdx].objectType = OBJ_None;
				}
				break;

			case OBJ_DSFocusLens:
				obj->mobj->pCraft->shieldFront = 0;
				obj->mobj->pCraft->shieldRear = 0;
				obj->flightGroupIdx = g_deathStarFocusLensFgIdx;
				obj->objectSignature = 0xffffu;
				if (def->scriptId == 14) {
					g_deathStarPowerSourceLinkedFocusLensObjIdx = (int16_t)def->spawnedObjIdx;
				}
				break;

			default:
				break;
		}
		++def;
	} while (def->objectType != OBJ_None);
}

// FUNCTION: XWA 0x4263F0
int DeathStar_BuildSegmentSets(void) {
	int ordered;
	int segmentSetIdx;
	DeathStarSegmentSet* segmentSet;

#ifdef XWA_MODERN
	DeathStar_InstallSegmentRuleCallbacks();
#endif
	segmentSet = g_deathStarSegmentSets;
	for (segmentSetIdx = 8; segmentSetIdx != 0; --segmentSetIdx, ++segmentSet) {
		uint16_t segmentCount;

		segmentSet->segments = NULL;
		segmentCount = segmentSet->count;
		if (segmentCount != 0) {
			segmentSet->segments = (DeathStarSegmentDef*)Memory_AllocTagged(
				"DSSEGMENT", sizeof(DeathStarSegmentDef) * segmentCount);
		}
	}

	do {
		for (segmentSetIdx = 0; (uint16_t)segmentSetIdx < 8u; ++segmentSetIdx) {
			int randomSign;
			uint16_t segmentIdx;

			if (g_deathStarSegmentSets[segmentSetIdx].segments == NULL) {
				continue;
			}

			for (segmentIdx = 0; segmentIdx < g_deathStarSegmentSets[segmentSetIdx].count; ++segmentIdx) {
				DeathStarSegmentDef* segment;
				const DeathStarSegmentRule* rule;

				{
					int yawDelta;
					int pitchDelta;

					segment = &g_deathStarSegmentSets[segmentSetIdx].segments[segmentIdx];
					if (g_deathStarSegmentSets[segmentSetIdx].fixedSegmentObjectType == OBJ_None) {
						do {
							segment->objectType = (uint16_t)(365 + (uint16_t)GameRand() % 11);
						} while (segment->objectType == g_deathStarLastGeneratedRandomSegmentType);
					} else {
						segment->objectType =
							(uint16_t)g_deathStarSegmentSets[segmentSetIdx].fixedSegmentObjectType;
					}

					segment->flags = 0;
					randomSign = GameRand();
					yawDelta = (GameRand() & 0xffff) % 1024 + 1024;
					if (((randomSign >> 8) & 1) != 0) {
						yawDelta = -yawDelta;
					}
					randomSign = GameRand();
					pitchDelta = (GameRand() & 0xffff) % 1024 + 1024;
					if (((randomSign >> 8) & 1) != 0) {
						pitchDelta = -pitchDelta;
					}
					segment->yaw = (Q16Angle)(g_deathStarSegmentSets[segmentSetIdx].baseYaw + yawDelta);
					segment->pitch = (Q16Angle)(g_deathStarSegmentSets[segmentSetIdx].basePitch + pitchDelta);
					segment->activeSegmentCount = 5;
					segment->worldX = 0;
					segment->worldY = (int)segmentIdx - 1;
					segment->worldZ = 1;
					rule = g_deathStarSegmentSets[segmentSetIdx].rules;
					segment->updateFn = NULL;
				}

				if (rule != NULL && rule->objectType != OBJ_None) {
					for (; rule->objectType != OBJ_None; ++rule) {
						int16_t ruleSegmentIdx;

						ruleSegmentIdx = rule->segmentIdx;
						if (ruleSegmentIdx < 0) {
							ruleSegmentIdx =
								(int16_t)(ruleSegmentIdx + g_deathStarSegmentSets[segmentSetIdx].count);
						}
						if ((int)segmentIdx < ruleSegmentIdx) {
							break;
						}
						if ((int)segmentIdx <= ruleSegmentIdx) {
							goto found_rule;
						}
					}
				}
				rule = NULL;

			found_rule:
				if (rule != NULL) {
					segment = &g_deathStarSegmentSets[segmentSetIdx].segments[segmentIdx];
					if ((rule->flags & DEATH_STAR_SEGMENT_FLAG_REDIRECT_CHILDREN) != 0) {
						DeathStar_ResolveSegmentRedirect(rule, segment);
					}
					segment->flags = rule->flags;
					if ((rule->flags & 0x1u) != 0) {
						segment->objectType = rule->objectType;
					}
					if ((rule->flags & 0x2u) != 0) {
						segment->yaw = rule->yaw;
						segment->pitch = rule->pitch;
					}
					if ((rule->flags & 0x80u) != 0) {
						segment->worldX = rule->attachPointIdx;
						segment->worldY = rule->parentSegmentIdx;
						if (segment->worldY < 0) {
							segment->worldY += g_deathStarSegmentSets[segmentSetIdx].count;
						}
						segment->worldZ = rule->parentPointKind;
					}
					if ((rule->flags & 0x100u) != 0) {
						segment->activeSegmentCount = rule->activeSegmentCount;
					}
					if ((rule->flags & 0x400u) != 0) {
						segment->nextSegmentSet = rule->nextSegmentSet;
						segment->nextSegmentIdx = rule->nextSegmentIdx;
						if (segment->nextSegmentIdx < 0) {
							segment->nextSegmentIdx =
								(int16_t)(segment->nextSegmentIdx +
										  g_deathStarSegmentSets[segment->nextSegmentSet].count);
						}
					}
					segment->updateFn = rule->updateFn;
				}

				if ((g_deathStarSegmentSets[segmentSetIdx].segments[segmentIdx].flags &
					 DEATH_STAR_SEGMENT_FLAG_REDIRECT_CHILDREN) == 0) {
					DeathStarSegmentDef* segment;
					const DeathStarSegmentDef* relativeTo;
					int parentX;
					int parentY;
					int parentZ;
					int attachX;
					int attachY;
					int attachZ;

					segment = &g_deathStarSegmentSets[segmentSetIdx].segments[segmentIdx];
					relativeTo = NULL;
					if ((segment->flags & 0x46u) != 0x46u) {
						relativeTo = &g_deathStarSegmentSets[segmentSetIdx].segments[segment->worldY];
					}

					if ((segment->flags & 0x6u) == 0x6u) {
						segment->pitch = (Q16Angle)(segment->pitch + 0x4000u);
					} else {
						DeathStar_ApplySegmentOrientationFlags(segment, relativeTo);
					}

					if ((segment->flags & 0x46u) == 0x46u) {
						parentX = 0;
						parentY = 0;
						parentZ = 0;
					} else {
						DeathStar_ComputeSegmentPointOffset(relativeTo, (uint16_t)segment->worldZ,
															DEATH_STAR_SEGMENT_POINT_KIND_ATTACH, &parentX,
															&parentY, &parentZ);
						parentX += relativeTo->worldX;
						parentY += relativeTo->worldY;
						parentZ += relativeTo->worldZ;
					}

					DeathStar_ComputeSegmentPointOffset(segment, (uint16_t)segment->worldX,
														DEATH_STAR_SEGMENT_POINT_KIND_ATTACH, &attachX,
														&attachY, &attachZ);
					segment->worldX = parentX - attachX;
					segment->worldY = parentY - attachY;
					segment->worldZ = parentZ - attachZ;
					DeathStar_PopulateRandomSegmentChildren((int16_t)segmentSetIdx, segment);
				}

				g_deathStarLastGeneratedRandomSegmentType =
					g_deathStarSegmentSets[segmentSetIdx].segments[segmentIdx].objectType;
			}
		}

		ordered = DeathStar_AreSegmentSetStartDistancesOrdered();
	} while (!ordered);

	return ordered;
}

// FUNCTION: XWA 0x426800
int DeathStar_AreSegmentSetStartDistancesOrdered(void) {
	DeathStarSegmentDef* segment1;
	DeathStarSegmentDef* segment3;
	DeathStarSegmentDef* segment4;
	int distance1;
	int distance3;
	int distance4;

	segment1 = &g_deathStarSegmentSets[1].segments[g_deathStarSegmentSets[1].count - 5];
	distance1 = collide_roughdistance3d(segment1->worldX, segment1->worldY, segment1->worldZ);

	segment3 = &g_deathStarSegmentSets[3].segments[g_deathStarSegmentSets[3].count - 12];
	distance3 = collide_roughdistance3d(segment3->worldX, segment3->worldY, segment3->worldZ);

	segment4 = &g_deathStarSegmentSets[4].segments[g_deathStarSegmentSets[4].count - 1];
	distance4 = collide_roughdistance3d(segment4->worldX, segment4->worldY, segment4->worldZ);

	return distance1 < distance3 && distance3 < distance4;
}

// FUNCTION: XWA 0x4268C0
void DeathStar_InitMobileObjectForType(uint16_t objectType, unsigned int objIdx) {
	MobileObject* mobj;
	int objectTypeIndex;
	uint16_t modelIndex;
	CraftData* craft;

	mobj = g_objectTable[objIdx].mobj;
	objectTypeIndex = (uint16_t)objectType;

	mobj->state = g_modelTypeTable[objectTypeIndex].familyId;
	mobj->motionFlags = 0;
	mobj->instanceExtent = g_modelTypeTable[objectTypeIndex].maxBoundsExtent;
	mobj->simStateTimestamp = 0;
	mobj->prevWorldX = g_objectTable[objIdx].world_x;
	mobj->prevWorldY = g_objectTable[objIdx].world_y;
	mobj->prevWorldZ = g_objectTable[objIdx].world_z;
	mobj->rollImpulseRate = 0;
	mobj->spinRate = 0;
	mobj->spinRateFrac = 0;
	mobj->spinDecelRate = 0;
	mobj->spinAngleQ16 = 0;
	mobj->speed = 0;
	mobj->speedRemainder = 0;
	mobj->damageAmount = g_modelTypeTable[objectTypeIndex].maxBoundsExtent;
	mobj->iff = 2;
	mobj->team = 2;
	mobj->lifetimeTimer = 0;
	mobj->framesAlive = 0;
	mobj->sourceObjIdx = -1;
	mobj->sourceObjectType = 0;
	mobj->nodeSwitchIndex = 0;
	mobj->ejectionSpawnCount = 0;
	mobj->collisionObjIdx = 0xffff;
	mobj->velocityOverrideActive = 0;
	mobj->moveVectorDirty = 1;
	mobj->orientMatrixDirty = 1;

	modelIndex = g_modelTypeTable[objectTypeIndex].modelIndex;
	if (modelIndex != 0xffffu) {
		craft = mobj->pCraft;
		memset(craft, 0, sizeof(*craft));
		craft->modelIndex = (uint16_t)modelIndex;
		craft->leader_obj_idx = -1;
		craft->workingSubsystems = 0x03ffu;
		craft->aiFlight.maxSpeedCache = g_modelDefs[(uint16_t)modelIndex].maxSpeed;
		craft->hullMax = g_modelDefs[(uint16_t)modelIndex].hullStrength;
		craft->systemDamageHullThreshold = g_modelDefs[(uint16_t)modelIndex].systemDamageHullThreshold;
		craft->shieldFront = 2 * g_modelDefs[(uint16_t)modelIndex].shieldStrength;
		memset(craft->componentHp, 0xff, sizeof(craft->componentHp));
		craft->aiController.pendingPlanId = 0;
		craft->aiController.currentPlanId = 0;
		craft->engineOutputScale = 0xffffu;
		craft->objectKind = 0;
		craft->carrierObjIdx = 0xffffu;
		craft->carriedObjectIndex = 0xffffu;
		craft->aiController.targetComponent = 0xffffu;
		craft->lastReleasedObjectIdx = 0xffffu;
		craft->linkedPrevObjectIdx = 0xffffu;
		craft->nextLinkObjectIdx = 0xffffu;
		craft->aiFlight.impactObjIdx = 0xffffu;
		craft->aiController.escortTargetFG = -1;
		collide_ResetObjectProximityForSlot((uint16_t)objIdx);
	}
}

// FUNCTION: XWA 0x426A90
DeathStarSegmentDef* DeathStar_ResolveSegmentRedirect(const DeathStarSegmentRule* rule,
													  DeathStarSegmentDef* outDef) {
	const DeathStarSegmentRule* currentRule;
	DeathStarSegmentDef* resolvedSegment;
	uint16_t resolvedSegmentSet;
	int16_t resolvedSegmentIdx;
	int resolved;

	currentRule = rule;
	resolved = 0;
	do {
		resolvedSegmentIdx = currentRule->redirectSegmentIdx;
		resolvedSegmentSet = currentRule->redirectSegmentSet;
		if (resolvedSegmentIdx < 0) {
			resolvedSegmentIdx =
				(int16_t)(resolvedSegmentIdx + g_deathStarSegmentSets[(int16_t)resolvedSegmentSet].count);
		}

		resolvedSegment = &g_deathStarSegmentSets[(int16_t)resolvedSegmentSet].segments[resolvedSegmentIdx];
		if ((resolvedSegment->flags & 0x200u) != 0) {
			currentRule = g_deathStarSegmentSets[resolvedSegmentSet].rules;
			if (currentRule != 0) {
				if (currentRule->objectType != 0) {
					int resolvedSegmentNumber;

					resolvedSegmentNumber = (uint16_t)resolvedSegmentIdx;
					for (;;) {
						int16_t candidateSegmentIdx;

						candidateSegmentIdx = currentRule->segmentIdx;
						if (candidateSegmentIdx < 0) {
							candidateSegmentIdx = (int16_t)(candidateSegmentIdx +
															g_deathStarSegmentSets[resolvedSegmentSet].count);
						}
						if (resolvedSegmentNumber < candidateSegmentIdx) {
							currentRule = 0;
							break;
						}
						if (resolvedSegmentNumber <= candidateSegmentIdx) {
							break;
						}
						++currentRule;
						if (currentRule->objectType == 0) {
							currentRule = 0;
							break;
						}
					}
				} else {
					currentRule = 0;
				}
			}
		} else {
			resolved = 1;
		}
	} while ((int16_t)resolved == 0);

	*outDef = *resolvedSegment;
	outDef->flags = rule->flags;
	outDef->childObjects[0].objectType = (uint16_t)resolvedSegmentSet;
	outDef->childObjects[0].angleByteOffsets = (uint16_t)resolvedSegmentIdx;
	return outDef;
}

// FUNCTION: XWA 0x426B80
void DeathStar_ApplySegmentOrientationFlags(DeathStarSegmentDef* segment,
											const DeathStarSegmentDef* relativeTo) {
	uint32_t flags;
	Q16Angle yaw;
	Q16Angle pitch;

	flags = segment->flags;
	yaw = relativeTo->yaw;
	pitch = relativeTo->pitch;
	if ((flags & 0x8u) != 0) {
		yaw = (Q16Angle)(yaw + 0x8000u);
		pitch = (Q16Angle)(0x8000u - pitch);
	}

	segment->yaw = (Q16Angle)(segment->yaw + yaw);
	segment->pitch = (Q16Angle)(segment->pitch + pitch);

	if ((flags & 0x10u) != 0) {
		pitch = (Q16Angle)(relativeTo->pitch - 0x4000u);
		if (pitch < 0x1000u || pitch > 0xf000u) {
			pitch = 0;
		} else if (pitch < 0x8000u) {
			pitch = (Q16Angle)(pitch - 0x1000u);
		} else {
			pitch = (Q16Angle)(pitch + 0x1000u);
		}
		segment->pitch = (Q16Angle)(pitch + 0x4000u);
	}

	if ((flags & 0x20u) != 0) {
		if (yaw < 0x0800u || yaw > 0xf800u) {
			yaw = 0;
		} else if (yaw < 0x8000u) {
			yaw = (Q16Angle)(yaw - 0x0800u);
		} else {
			yaw = (Q16Angle)(yaw + 0x0800u);
		}
		segment->yaw = yaw;
	}
}

// FUNCTION: XWA 0x426C30
void DeathStar_ComputeSegmentPointOffset(const DeathStarSegmentDef* segmentDef, uint16_t pointIdx,
										 int pointKind, int* outX, int* outY, int* outZ) {
	const DeathStarObjectPointTable* pointTable;
	const DeathStarSegmentLocalPoint* point;
	ObjectRecord objRecord;

	pointTable = &g_deathStarObjectPointTables[segmentDef->objectType - OBJ_DSReactorCore];
	point = 0;
	switch (pointKind) {
		case DEATH_STAR_SEGMENT_POINT_KIND_SPAWN:
			if (pointIdx < pointTable->spawnPointCount) {
				point = &pointTable->spawnPoints[pointIdx];
			}
			break;

		case DEATH_STAR_SEGMENT_POINT_KIND_ATTACH:
			if (pointIdx < pointTable->attachPointCount) {
				point = &pointTable->attachPoints[pointIdx];
			}
			break;
	}

	if (point == 0) {
		*outZ = 0;
		*outY = 0;
		*outX = 0;
		return;
	}

	objRecord.yaw = segmentDef->yaw;
	objRecord.pitch = segmentDef->pitch;
	objRecord.roll = 0;
	objRecord.angleD = 0;
	objRecord.mobj = 0;

	pai_RotateLocalVectorToWorldScratchMaybeStatic(&objRecord, point->side, point->up, point->forward);
	*outX = g_rotatedX;
	*outY = g_rotatedY;
	*outZ = g_rotatedZ;
}

// FUNCTION: XWA 0x426D20
void DeathStar_PopulateRandomSegmentChildren(int16_t segmentSetIdx, DeathStarSegmentDef* segment) {
	const DeathStarObjectPointTable* pointTable;
	uint16_t childSlotLimit;

	memset(segment->childObjects, 0, sizeof(segment->childObjects));

	switch (segment->objectType) {
		case OBJ_DSReactorCore:
		case OBJ_DSReactorCoreRoom:
		case OBJ_DS3rdRoom:
			return;

		case OBJ_DSAngleTube:
			if (segmentSetIdx == 7) {
				return;
			}
			break;

		default:
			break;
	}

	pointTable = &g_deathStarObjectPointTables[segment->objectType - OBJ_DSReactorCore];
	childSlotLimit = pointTable->spawnPointCount;
	childSlotLimit = childSlotLimit >= g_deathStarRandomChildObjectLimit ? g_deathStarRandomChildObjectLimit
																		 : childSlotLimit;
	while (childSlotLimit-- != 0) {
		uint16_t emptySlotOrdinal;
		uint16_t childSlot;

		emptySlotOrdinal = (uint16_t)((int)(uint16_t)GameRand() % (int)((uint16_t)childSlotLimit + 1u));
		childSlot = 0;
		while (childSlot < 10) {
			if (!segment->childObjects[childSlot].objectType) {
				if (!emptySlotOrdinal) {
					segment->childObjects[childSlot].objectType =
						(uint16_t)(OBJ_DSTankwlights + ((int)(uint16_t)GameRand() % 21));
					segment->childObjects[childSlot].objectIdx = 0xffffu;
					if (pointTable->spawnPoints[childSlot].up > 0) {
						segment->childObjects[childSlot].angleByteOffsets = 0x0080u;
					}
					break;
				}
				--emptySlotOrdinal;
			}
			++childSlot;
		}
	}
}

// FUNCTION: XWA 0x426E50
void DeathStar_LoadActiveSegments(void) {
	uint16_t segmentSlotIdx;

	g_deathStarActiveSegmentCount = 0;
	DeathStar_ResizeActiveSegmentSlots(5);

	segmentSlotIdx = 0;
	if (g_deathStarActiveSegmentCount > segmentSlotIdx) {
		do {
		DeathStarSegmentDef* segment;
		DeathStarChildObjectRef* childObjects;
		int objectIdx;
		uint16_t childIdx;

		segment = &g_deathStarSegmentSets[g_deathStarSegmentSetIdx].segments[segmentSlotIdx];
		objectIdx = g_deathStarActiveSegmentObjIdx[segmentSlotIdx];
		g_objectTable[objectIdx].objectType = (ObjectTypeId)segment->objectType;
		g_objectTable[objectIdx].world_x = segment->worldX;
		g_objectTable[objectIdx].world_y = segment->worldY;
		g_objectTable[objectIdx].world_z = segment->worldZ;
		g_objectTable[objectIdx].yaw = segment->yaw;
		g_objectTable[objectIdx].pitch = segment->pitch;

		childObjects = segment->childObjects;
		if ((segment->flags & DEATH_STAR_SEGMENT_FLAG_REDIRECT_CHILDREN) != 0) {
			uint16_t redirectedSegmentSetIdx;
			uint16_t redirectedSegmentIdx;

			redirectedSegmentSetIdx = childObjects->objectType;
			redirectedSegmentIdx = childObjects->angleByteOffsets;
			childObjects =
				g_deathStarSegmentSets[redirectedSegmentSetIdx].segments[redirectedSegmentIdx].childObjects;
		}

		for (childIdx = 0; childIdx < 10u; ++childIdx) {
			if (childObjects->objectType != OBJ_None) {
				DeathStar_SpawnSegmentChildObject(childObjects, childIdx, segment);
			}
		}

		g_deathStarActiveSegmentIdx[segmentSlotIdx] = segmentSlotIdx;
		++segmentSlotIdx;
		} while (segmentSlotIdx < g_deathStarActiveSegmentCount);
	}

	{
		uint16_t activeCount;
		uint16_t slotIdx;

		activeCount = g_deathStarActiveSegmentCount;
		for (slotIdx = activeCount - 2; slotIdx != 0; --slotIdx) {
			DeathStar_PreloadSegmentObjectTextures(&g_objectTable[g_deathStarActiveSegmentObjIdx[slotIdx]], 0,
												   4);
		}
	}
}

// FUNCTION: XWA 0x426FB0
void DeathStar_ResizeActiveSegmentSlots(uint16_t activeSegmentCount) {
	if (activeSegmentCount < (uint16_t)g_deathStarActiveSegmentCount) {
		uint16_t slotIdx;

		slotIdx = activeSegmentCount;
		do {
			g_objectTable[g_deathStarActiveSegmentObjIdx[slotIdx]].objectType = OBJ_None;
			++slotIdx;
		} while (slotIdx < (uint16_t)g_deathStarActiveSegmentCount);
	}

	if (activeSegmentCount > (uint16_t)g_deathStarActiveSegmentCount) {
		uint16_t slotIdx;

		slotIdx = (uint16_t)g_deathStarActiveSegmentCount;
		while (slotIdx < activeSegmentCount) {
			uint16_t objectIdx;
			int byteIdx;

			objectIdx = Object_FindFreeMissionSlot();
			g_deathStarActiveSegmentObjIdx[slotIdx] = objectIdx;
			g_objectTable[objectIdx].objectType = OBJ_DSTriRoomEntrance;
			g_objectTable[objectIdx].flightGroupIdx = g_deathStarActiveSegmentPlaceholderFgIdx;
			g_objectTable[objectIdx].genusId = GENUS_DeathStarTunnelSegment;
			g_objectTable[objectIdx].regionIdx = 0;
			g_objectTable[objectIdx].roll = 0;
			g_objectTable[objectIdx].angleD = 0;
			g_objectTable[objectIdx].typeSpecificWord = 0;
			for (byteIdx = 0; byteIdx < 2; ++byteIdx) {
				g_objectTable[objectIdx].typeSpecificByte[byteIdx] = 0;
			}
			g_objectTable[objectIdx].playerOwnerIdx = -1;
			g_objectTable[objectIdx].mobj = 0;
			++slotIdx;
		}
	}

	g_deathStarActiveSegmentCount = activeSegmentCount;
}

// FUNCTION: XWA 0x4270D0
void DeathStar_UpdateActiveSegmentWindow(int16_t centerSegmentIdx) {
	uint16_t activeSegmentCount;
	uint16_t halfActiveSegmentCount;
	int objectIdx;
	int slotIdx;
	int segmentSetIdx;
	int16_t startSegmentIdx;

	activeSegmentCount = (uint16_t)g_deathStarActiveSegmentCount;
	halfActiveSegmentCount = activeSegmentCount >> 1;
	startSegmentIdx = (int16_t)(centerSegmentIdx - halfActiveSegmentCount);
	segmentSetIdx = g_deathStarSegmentSetIdx;

	if (segmentSetIdx == 1 && centerSegmentIdx == g_deathStarSegmentSets[1].count - 7) {
		startSegmentIdx = (int16_t)(centerSegmentIdx - 2);
	}
	if (startSegmentIdx < 0) {
		startSegmentIdx = 0;
	}

	if (startSegmentIdx > g_deathStarSegmentSets[segmentSetIdx].count - activeSegmentCount) {
		startSegmentIdx = g_deathStarSegmentSets[segmentSetIdx].count - activeSegmentCount;
	}

	slotIdx = 0;
	while ((uint16_t)slotIdx < (uint16_t)g_deathStarActiveSegmentCount) {
		const DeathStarSegmentDef* segment;

		segmentSetIdx = g_deathStarSegmentSetIdx;
		segment = &g_deathStarSegmentSets[segmentSetIdx].segments[(uint16_t)slotIdx + startSegmentIdx];
		objectIdx = g_deathStarActiveSegmentObjIdx[(uint16_t)slotIdx];

		g_objectTable[objectIdx].objectType = (ObjectTypeId)segment->objectType;
		g_objectTable[objectIdx].world_x = segment->worldX;
		g_objectTable[objectIdx].world_y = segment->worldY;
		g_objectTable[objectIdx].world_z = segment->worldZ;
		g_objectTable[objectIdx].yaw = segment->yaw;
		g_objectTable[objectIdx].pitch = segment->pitch;
		g_deathStarActiveSegmentIdx[(uint16_t)slotIdx] = slotIdx + startSegmentIdx;

		++slotIdx;
	}
}

// FUNCTION: XWA 0x4271F0
void DeathStar_RebuildSegmentChildObjects(uint16_t* oldActiveSegmentIdx, uint16_t oldSegmentSetIdx,
										  uint16_t oldActiveSegmentCount) {
	uint16_t activeSlotIdx;

	if (oldActiveSegmentCount > 0) {
		int oldSetIdx;
		uint16_t* oldSegmentIdx;
		int oldSlotsRemaining;

		oldSetIdx = oldSegmentSetIdx;
		oldSegmentIdx = oldActiveSegmentIdx;
		oldSlotsRemaining = oldActiveSegmentCount;
		do {
			uint16_t stillActive;

			stillActive = 0;
			if (oldSetIdx == g_deathStarSegmentSetIdx) {
				uint16_t activeSegmentCount;
				uint16_t scanIdx;

				activeSegmentCount = (uint16_t)g_deathStarActiveSegmentCount;
				for (scanIdx = 0; scanIdx < activeSegmentCount; ++scanIdx) {
					if (*oldSegmentIdx == g_deathStarActiveSegmentIdx[scanIdx]) {
						stillActive = 1;
						break;
					}
				}
			}

			if (!stillActive) {
				DeathStarSegmentDef* segment;
				DeathStarChildObjectRef* childObjects;
				uint16_t childIdx;

				segment = &g_deathStarSegmentSets[oldSetIdx].segments[*oldSegmentIdx];
				childObjects = segment->childObjects;
				if ((segment->flags & DEATH_STAR_SEGMENT_FLAG_REDIRECT_CHILDREN) != 0) {
					uint16_t redirectedSegmentSetIdx;
					uint16_t redirectedSegmentIdx;

					redirectedSegmentSetIdx = childObjects[0].objectType;
					redirectedSegmentIdx = childObjects[0].angleByteOffsets;
					childObjects = g_deathStarSegmentSets[redirectedSegmentSetIdx]
									   .segments[redirectedSegmentIdx]
									   .childObjects;
				}

				for (childIdx = 0; childIdx < 10u; ++childIdx) {
					DeathStarChildObjectRef* child;

					child = &childObjects[childIdx];
					if (child->objectType != OBJ_None && child->objectIdx != 0xffffu) {
						g_objectTable[child->objectIdx].objectType = OBJ_None;
						child->objectIdx = 0xffffu;
					}
				}
			}
			++oldSegmentIdx;
			--oldSlotsRemaining;
		} while (oldSlotsRemaining != 0);
	}

	for (activeSlotIdx = 0; activeSlotIdx < (uint16_t)g_deathStarActiveSegmentCount; ++activeSlotIdx) {
		uint16_t alreadyActive;

		alreadyActive = 0;
		if (oldSegmentSetIdx == g_deathStarSegmentSetIdx) {
			uint16_t scanIdx;

			for (scanIdx = 0; scanIdx < oldActiveSegmentCount; ++scanIdx) {
				if (g_deathStarActiveSegmentIdx[activeSlotIdx] == oldActiveSegmentIdx[scanIdx]) {
					alreadyActive = 1;
					break;
				}
			}
		}

		if (!alreadyActive) {
			DeathStarSegmentDef* segment;
			DeathStarChildObjectRef* childObjects;
			int segmentSetIdx;
			uint16_t childIdx;

			segmentSetIdx = g_deathStarSegmentSetIdx;
			segment =
				&g_deathStarSegmentSets[segmentSetIdx].segments[g_deathStarActiveSegmentIdx[activeSlotIdx]];
			childObjects = segment->childObjects;
			if ((segment->flags & DEATH_STAR_SEGMENT_FLAG_REDIRECT_CHILDREN) != 0) {
				uint16_t redirectedSegmentSetIdx;
				uint16_t redirectedSegmentIdx;

				redirectedSegmentSetIdx = childObjects[0].objectType;
				redirectedSegmentIdx = childObjects[0].angleByteOffsets;
				childObjects = g_deathStarSegmentSets[redirectedSegmentSetIdx]
								   .segments[redirectedSegmentIdx]
								   .childObjects;
			}

			for (childIdx = 0; childIdx < 10u; ++childIdx) {
				if (childObjects[childIdx].objectType != OBJ_None) {
					DeathStar_SpawnSegmentChildObject(childObjects, childIdx, segment);
				}
			}
		}
	}
}

// FUNCTION: XWA 0x427410
void DeathStar_SpawnSegmentChildObject(DeathStarChildObjectRef* childList, uint16_t childIdx,
									   DeathStarSegmentDef* parentSegment) {
	DeathStarChildObjectRef* child;
	int objectIdx;

	objectIdx = Object_FindFreeMissionSlot();
	child = &childList[childIdx];
	child->objectIdx = objectIdx;
	if (objectIdx != 0xffffu) {
		ObjectRecord* object;
		int byteIdx;
		int offsetX;
		int offsetY;
		int offsetZ;

		object = &g_objectTable[objectIdx];
		object->flightGroupIdx = g_deathStarGeneratedObjectFgIdx;
		object->regionIdx = 0;
		object->roll = 0;
		object->angleD = 0;
		object->typeSpecificWord = g_deathStarSegmentChildInitialHitCount;
		for (byteIdx = 0; byteIdx < 2; ++byteIdx) {
			object->typeSpecificByte[byteIdx] = 0;
		}
		object->playerOwnerIdx = -1;
		object->mobj = 0;

		object->objectSignature = g_nextObjectSignature;
		++g_nextObjectSignature;
		if (g_nextObjectSignature == 0) {
			g_nextObjectSignature = 2;
		}
		child->objectSignature = object->objectSignature;

		object->objectType = (ObjectTypeId)child->objectType;
		object->genusId = (ModelGenusId)g_modelTypeTable[(uint16_t)object->objectType].genusId;
		object->yaw = parentSegment->yaw;
		object->pitch = parentSegment->pitch;
		object->yaw = (Q16Angle)(object->yaw + (child->angleByteOffsets & 0xff00u));
		object->pitch = (Q16Angle)(object->pitch + ((child->angleByteOffsets & 0x00ffu) << 8));

		DeathStar_ComputeSegmentPointOffset(parentSegment, childIdx, DEATH_STAR_SEGMENT_POINT_KIND_SPAWN,
											&offsetX, &offsetY, &offsetZ);
		object->world_x = parentSegment->worldX + offsetX;
		object->world_y = parentSegment->worldY + offsetY;
		object->world_z = parentSegment->worldZ + offsetZ;

		if ((uint16_t)object->objectType >= OBJ_DSReactorCore &&
			(uint16_t)object->objectType < OBJ_ChuteMouth) {
			DeathStarSegmentDef childSegment;

			childSegment.objectType = (uint16_t)object->objectType;
			childSegment.yaw = object->yaw;
			childSegment.pitch = object->pitch;
			DeathStar_ComputeSegmentPointOffset(&childSegment, 0, DEATH_STAR_SEGMENT_POINT_KIND_SPAWN,
												&offsetX, &offsetY, &offsetZ);
			object->world_x -= offsetX;
			object->world_y -= offsetY;
			object->world_z -= offsetZ;
		}
	}
}

// FUNCTION: XWA 0x4275B0
void DeathStar_UpdateFollowChainSlot(int followSlotIdx) {
	DeathStarFollowChainSlot* slot;
	ObjectRecord* object;
	CraftData* craft;
	int targetObjIdx;
	int leaderObjIdx;
	int extentSum;
	int desiredSpacing;

	slot = &g_deathStarFollowChainSlots[followSlotIdx];
	leaderObjIdx = g_deathStarFollowLeaderObjIdx;
	targetObjIdx = leaderObjIdx;
	object = &g_objectTable[slot->objectIdx];
	craft = object->mobj->pCraft;

	if (followSlotIdx != 0) {
		targetObjIdx = g_deathStarFollowChainSlots[followSlotIdx - 1].objectIdx;
	}

	slot->desiredSpacing = g_deathStarFollowBaseDesiredSpacing;
	extentSum = (int)g_modelTypeTable[object->objectType].maxBoundsExtent +
				(int)g_modelTypeTable[g_objectTable[targetObjIdx].objectType].maxBoundsExtent;
	desiredSpacing = g_deathStarFollowBaseDesiredSpacing - (int)((double)extentSum * -0.039999999);
	slot->desiredSpacing = desiredSpacing;
	if (targetObjIdx != leaderObjIdx) {
		slot->desiredSpacing = desiredSpacing + g_deathStarFollowChainSlots[followSlotIdx - 1].desiredSpacing;
	}

	if (object->mobj->team == g_objectTable[targetObjIdx].mobj->team) {
		craft->aiController.targetObjIdx = 0xffffu;
	} else {
		craft->aiController.targetObjIdx = (uint16_t)targetObjIdx;
		craft->aiController.aiPlanState = 2000;
		slot->desiredSpacing += 50;
	}
}

// FUNCTION: XWA 0x4276C0
void DeathStar_RefreshFollowOverrideCandidates(void) {
	ObjectRecord* objectTable;
	unsigned int objectIdx;
	int followSlotIdx;

	if (g_deathStarFollowRefreshPending == 0) {
		return;
	}

	objectTable = g_objectTable;
	for (followSlotIdx = 0; followSlotIdx < 10; ++followSlotIdx) {
		DeathStarFollowChainSlot* slot = &g_deathStarFollowChainSlots[followSlotIdx];

		if (slot->objectIdx != 0xffff && objectTable[slot->objectIdx].objectType != OBJ_None) {
			slot->refreshTimer = slot->pathDistance;
		}
	}

	objectIdx = g_activeRegionObjectSlotStart;
	if (objectIdx < g_activeRegionCraftObjectSlotEnd) {
		do {
			ObjectRecord* object;

			object = &objectTable[objectIdx];
			if (object->objectType != OBJ_None && objectIdx != (unsigned int)g_deathStarFollowLeaderObjIdx &&
				(object->genusId == GENUS_Fighter || object->genusId == GENUS_Transport)) {
				int followSlotIdx;

				for (followSlotIdx = 0; followSlotIdx < 10; ++followSlotIdx) {
					if (objectIdx == (unsigned int)g_deathStarFollowChainSlots[followSlotIdx].objectIdx) {
						break;
					}
				}

				if (followSlotIdx == 10) {
					paiman_RefreshDeathStarPlayerFollow((int)objectIdx, g_localPlayer);
					objectTable = g_objectTable;
				}
			}
			++objectIdx;
		} while (objectIdx < g_activeRegionCraftObjectSlotEnd);
	}

	g_deathStarFollowRefreshPending = 0;
}

// FUNCTION: XWA 0x427790
void DeathStar_RemoveFollowChainSlot(int followSlotIdx) {
	DeathStarFollowChainSlot* slot;

	slot = &g_deathStarFollowChainSlots[followSlotIdx];
	g_objectTable[slot->objectIdx].mobj->speed = 80;

	if (followSlotIdx < 9) {
		int refreshSlotIdx;

#ifdef XWA_MODERN
		memmove(slot, slot + 1, (size_t)(9 - followSlotIdx) * sizeof(*slot));
#else
		memcpy(slot, &g_deathStarFollowChainSlots[followSlotIdx + 1],
			   (size_t)(9 - followSlotIdx) * sizeof(*slot));
#endif
		refreshSlotIdx = followSlotIdx;
		while (refreshSlotIdx < 9) {
			if (g_deathStarFollowChainSlots[refreshSlotIdx].objectIdx != 0xffff &&
				g_objectTable[g_deathStarFollowChainSlots[refreshSlotIdx].objectIdx].objectType != OBJ_None) {
				DeathStar_UpdateFollowChainSlot(refreshSlotIdx);
			}
			++refreshSlotIdx;
		}
	}

	g_deathStarFollowChainSlots[9].objectIdx = 0xffff;
}

// FUNCTION: XWA 0x427840
char DeathStar_InterpolateFollowCraftOnPath(int objectIdx, int sampleIdxA, int sampleIdxB,
											unsigned int ticksIntoSample, DeathStarFollowChainSlot* slot) {
	ObjectRecord* object;
	MobileObject* mobj;
	const DeathStarPathSample* sampleA;
	const DeathStarPathSample* sampleB;
	double fraction;

	object = &g_objectTable[objectIdx];
	sampleA = &g_deathStarPathSamples[sampleIdxA];
	sampleB = &g_deathStarPathSamples[sampleIdxB];

	object->world_x = sampleA->worldX;
	object->world_y = sampleA->worldY;
	object->world_z = sampleA->worldZ;
	object->yaw = sampleA->yaw;
	object->pitch = sampleA->pitch;
	object->roll = sampleA->roll;

	fraction = (double)ticksIntoSample / (double)(unsigned int)sampleB->elapsedTicksSincePrev;

	object->world_x += (int)((double)(sampleB->worldX - sampleA->worldX) * fraction);
	object->world_y += (int)((double)(sampleB->worldY - sampleA->worldY) * fraction);
	object->world_z += (int)((double)(sampleB->worldZ - sampleA->worldZ) * fraction);

	mobj = object->mobj;
	mobj->speed = 0;

	slot->reserved06 = (uint16_t)(int)((double)-(sampleA->tacticalIndex - sampleB->tacticalIndex) * fraction);
	{
		int16_t angleDelta = (int16_t)(sampleB->yaw - sampleA->yaw);
		double signedAngleDelta;

		if (angleDelta < 0) {
			signedAngleDelta = -(double)-angleDelta;
		} else {
			signedAngleDelta = (double)angleDelta;
		}
		object->yaw = (Q16Angle)(object->yaw + (int)(signedAngleDelta * fraction));
	}
	{
		int16_t angleDelta = (int16_t)(sampleB->pitch - sampleA->pitch);
		double signedAngleDelta;

		if (angleDelta < 0) {
			signedAngleDelta = -(double)-angleDelta;
		} else {
			signedAngleDelta = (double)angleDelta;
		}
		object->pitch = (Q16Angle)(object->pitch + (int)(signedAngleDelta * fraction));
	}
	{
		int16_t angleDelta = (int16_t)(sampleB->roll - sampleA->roll);
		double signedAngleDelta;

		if (angleDelta < 0) {
			signedAngleDelta = -(double)-angleDelta;
		} else {
			signedAngleDelta = (double)angleDelta;
		}
		object->roll = (Q16Angle)(object->roll + (int)(signedAngleDelta * fraction));
	}

	object->mobj->orientMatrixDirty = 1;
	object->mobj->moveVectorDirty = 1;
	return 1;
}

// FUNCTION: XWA 0x427A10
void DeathStar_AddFollowChainSlot(int objectIdx) {
	int followSlotIdx;
	int pathDistance;
	int sampleIdx;
	DeathStarFollowChainSlot* slot;

	followSlotIdx = 0;
	slot = g_deathStarFollowChainSlots;
	for (; (intptr_t)slot < (intptr_t)&g_deathStarFollowChainSlots[10]; ++followSlotIdx) {
		if (slot->objectIdx == 0xffff) {
			slot->objectIdx = objectIdx;
			slot->objectSignature = g_objectTable[objectIdx].objectSignature;
			pathDistance = g_deathStarTunnelTimer - g_deathStarPathSampleLastTime;

			for (sampleIdx = 0; sampleIdx < 30; ++sampleIdx) {
				if (sampleIdx != g_deathStarPathSampleWriteIdx) {
					pathDistance += g_deathStarPathSamples[sampleIdx].elapsedTicksSincePrev;
				}
			}

			slot->pathDistance = pathDistance;
			DeathStar_UpdateFollowChainSlot(followSlotIdx);
			slot->refreshTimer = 0;
			return;
		}
		++slot;
	}
}

// FUNCTION: XWA 0x427A90
void DeathStar_PositionPlayerAndFollowersAtStart(void) {
	ObjectRecord* playerObj;
	unsigned int objectIdx;
	int followerYOffset;

	g_deathStarEntranceProximityArmed = 0;

	playerObj = &g_objectTable[g_deathStarPlayerObjIdx];

	playerObj->mobj->pCraft->throttleSpeed = 0xffffu;
	playerObj->mobj->speed =
		(uint16_t)((double)(uint16_t)playerObj->mobj->pCraft->aiFlight.maxSpeedCache * 3.3);

	g_deathStarEntranceTransitionState = 0;
	g_deathStarEntranceTransitionTimer = 1;

	playerObj->yaw = 0;
	playerObj->world_y -= 35000;
	playerObj->pitch = 0x4000u;
	playerObj->roll = 0x1000u;
	playerObj->mobj->pCraft->objectKind = 9;

	followerYOffset = 0;
	objectIdx = g_activeRegionObjectSlotStart;
	if (objectIdx < g_activeRegionCraftObjectSlotEnd) {
		do {
			if (g_objectTable[objectIdx].objectType != OBJ_None &&
				objectIdx != (unsigned int)g_deathStarFollowLeaderObjIdx &&
				(g_objectTable[objectIdx].genusId == GENUS_Fighter ||
				 g_objectTable[objectIdx].genusId == GENUS_Transport)) {
				followerYOffset += 3000;
				g_objectTable[objectIdx].world_x = g_objectTable[g_deathStarPlayerObjIdx].world_x;
				g_objectTable[objectIdx].world_y =
					g_objectTable[g_deathStarPlayerObjIdx].world_y - followerYOffset;
				g_objectTable[objectIdx].world_z = g_objectTable[g_deathStarPlayerObjIdx].world_z;
				g_objectTable[objectIdx].yaw = 0;
				g_objectTable[objectIdx].pitch = 0x4000u;
				g_objectTable[objectIdx].roll = 0x1000u;
			}
			++objectIdx;
		} while (objectIdx < g_activeRegionCraftObjectSlotEnd);
	}
}

// FUNCTION: XWA 0x427C10
void DeathStar_InitAccelChamberState(void) {
	int meshIdx;
	int accEndCount;
	int spawnIntervalReduction;
	int secondEndY;
	int secondEndZ;
	int thirdEndY;
	int thirdEndZ;

	meshIdx = 0;
	g_deathStarContainerCollisionLightTimer = 0;
	g_deathStarAccelChamberLightTimer = 0;
	g_deathStarAccelChamberLastContainerSpawnTime = 0;
	g_deathStarAccelChamberContainersCleared = 1;
	secondEndY = secondEndZ = thirdEndY = thirdEndZ = 0;
	spawnIntervalReduction = 5 * (int)g_flightDifficulty;
	g_deathStarAccelChamberContainerSpawnInterval = 1200 - 40 * spawnIntervalReduction;
	accEndCount = 0;
	g_deathStarAccelChamberPitchOffset = 0;
	{
		uint16_t meshCount;

		meshCount = (uint16_t)ModelMesh_GetCount(OBJ_DSAccelChamber);
		if (meshCount > 0u) {
			int meshCountRemaining;

			meshCountRemaining = meshCount;
			do {
				uint16_t hardpointCount;

				hardpointCount = (uint16_t)ModelMesh_CountHardpoints(OBJ_DSAccelChamber, meshIdx);
				if (hardpointCount != 0 && hardpointCount > 0u) {
					int hardpointCountRemaining;
					int hardpointIdx;
					OptHardpointType hardpointType;
					int hardpointX;
					int hardpointY;
					int hardpointZ;

					hardpointIdx = 0;
					hardpointCountRemaining = hardpointCount;
					do {
						ModelMesh_GetHardpoint(OBJ_DSAccelChamber, meshIdx, hardpointIdx, &hardpointType,
											   &hardpointX, &hardpointY, &hardpointZ);
						if (hardpointType == OPT_HARDPOINT_AccEnd) {
							if (accEndCount == 2) {
								secondEndY = hardpointY;
								secondEndZ = hardpointZ;
							}
							if (accEndCount == 3) {
								thirdEndY = hardpointY;
								thirdEndZ = hardpointZ;
							}
							++accEndCount;
						}
						++hardpointIdx;
					} while (--hardpointCountRemaining != 0);
				}
				++meshIdx;
			} while (--meshCountRemaining != 0);
		}
	}

	if (accEndCount != 0) {
		g_deathStarAccelChamberPitchOffset -= trig2_arctan(thirdEndZ - secondEndZ, thirdEndY - secondEndY);
	}
}

// FUNCTION: XWA 0x427D40
void DeathStar_InitZeroGStormtrooperWaypoints(void) {
	enum {
		DEATH_STAR_ZERO_G_SEGMENT_SET = 3,
		DEATH_STAR_ZERO_G_ORDER_SLOT = 0,
	};

	DeathStarSegmentDef* segment;
	uint16_t segmentIdx;
	uint16_t flightGroupIdx;
	int offsetX;
	int offsetY;
	int offsetZ;
	XwaWaypoint* waypoint;

	segment = NULL;
	for (segmentIdx = 0; segmentIdx < g_deathStarSegmentSets[DEATH_STAR_ZERO_G_SEGMENT_SET].count;
		 ++segmentIdx) {
		segment = &g_deathStarSegmentSets[DEATH_STAR_ZERO_G_SEGMENT_SET].segments[segmentIdx];
		if (segment->objectType == OBJ_DS3rdRoom) {
			break;
		}
	}

	if (segment == NULL) {
		return;
	}

	for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups; ++flightGroupIdx) {
		if (g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[flightGroupIdx].fg.craftType] ==
			OBJ_ZeroGStormtrooper) {
			DeathStar_ComputeSegmentPointOffset(segment, 0, DEATH_STAR_SEGMENT_POINT_KIND_SPAWN, &offsetX,
												&offsetY, &offsetZ);
			waypoint =
				&g_missionFlightGroups[flightGroupIdx].fg.orders[DEATH_STAR_ZERO_G_ORDER_SLOT].waypoints[0];
			waypoint->x = (int16_t)((segment->worldX + offsetX) >> 8);
			waypoint->y = (int16_t)(-(segment->worldY + offsetY) >> 8);
			waypoint->z = (int16_t)((segment->worldZ + offsetZ) >> 8);

			DeathStar_ComputeSegmentPointOffset(segment, 1, DEATH_STAR_SEGMENT_POINT_KIND_SPAWN, &offsetX,
												&offsetY, &offsetZ);
			waypoint =
				&g_missionFlightGroups[flightGroupIdx].fg.orders[DEATH_STAR_ZERO_G_ORDER_SLOT].waypoints[1];
			waypoint->x = (int16_t)((segment->worldX + offsetX) >> 8);
			waypoint->y = (int16_t)(-(segment->worldY + offsetY) >> 8);
			waypoint->z = (int16_t)((segment->worldZ + offsetZ) >> 8);
		}
	}

	segment->objectType = OBJ_DSAccelTube;
	segment->worldZ = 0;
	segment->worldY = 0;
	segment->worldX = 0;
	g_modelTypeTable[OBJ_DSTripodGun].flags &= (uint16_t)~0x0003u;
	g_deathStarTripodGunsActivated = 0;
}

// FUNCTION: XWA 0x427EB0
void DeathStar_InitReactorAssaultState(void) {
	enum {
		DEATH_STAR_REACTOR_SEGMENT_SET = 4,
		DEATH_STAR_REACTOR_STATUS_DESTROYED = 20,
		DEATH_STAR_REACTOR_SHOCKWAVE_SPEED = 3600,
	};

	int coreObjIdx;
	DeathStarSegmentDef segmentDef;
	int offsetX;
	int offsetY;
	int offsetZ;
	DeathStarSegmentDef* coreRoomSegment;
	CraftData* craft;

	g_deathStarReactorShockwaveObjIdx[0] = 0xffff;
	g_deathStarReactorShockwaveObjIdx[1] = 0xffff;
	g_deathStarReactorShockwaveObjIdx[2] = 0xffff;
	g_deathStarReactorCoreRoomSegmentIdx.value =
		(uint16_t)(g_deathStarSegmentSets[DEATH_STAR_REACTOR_SEGMENT_SET].count - 1u);
	coreObjIdx = g_deathStarReactorCoreObjIdx;
	g_deathStarReactorShockwaveObjIdx[3] = 0xffff;
	g_deathStarReactorShockwaveObjIdx[4] = 0xffff;
	g_deathStarReactorCylinderAnimTimer = 0;
	g_deathStarReactorReservedFilmState = 0;
	g_deathStarReactorDestructionTimer = 0;
	g_deathStarReactorCoreDriftSpeed = 0;
	g_deathStarReactorCoreDriftDirZ = 0.0f;
	g_deathStarReactorCoreDriftDirY = 0.0f;
	g_deathStarReactorCoreDriftDirX = 0.0f;
#ifdef XWA_MODERN
	g_modernDeathStarReactorRandomDriftX = 0;
	g_modernDeathStarReactorRandomDriftY = 0;
	g_modernDeathStarReactorRandomDriftZ = 0;
	g_modernDeathStarReactorRandomDriftRemainderX = 0;
	g_modernDeathStarReactorRandomDriftRemainderY = 0;
	g_modernDeathStarReactorRandomDriftRemainderZ = 0;
	g_modernDeathStarReactorCylinderYawRemainder = 0;
	g_modernDeathStarReactorCoreDriftSpeedRemainder = 0;
	g_modernDeathStarReactorCorePitchRemainder = 0;
	g_modernDeathStarReactorCoreDirectedDriftRemainderX = 0.0;
	g_modernDeathStarReactorCoreDirectedDriftRemainderY = 0.0;
	g_modernDeathStarReactorCoreDirectedDriftRemainderZ = 0.0;
	g_modernDeathStarShockwaveLightValue = 0;
#endif

	if (coreObjIdx != 0xffffu) {
		ObjectRecord* coreObj;

		coreObj = &g_objectTable[coreObjIdx];
		segmentDef.objectType = coreObj->objectType;
		segmentDef.yaw = coreObj->yaw;
		segmentDef.pitch = coreObj->pitch;
		DeathStar_ComputeSegmentPointOffset(&segmentDef, 1u, DEATH_STAR_SEGMENT_POINT_KIND_ATTACH, &offsetX,
											&offsetY, &offsetZ);

		coreObjIdx = g_deathStarReactorCoreObjIdx;
		coreObj = &g_objectTable[coreObjIdx];
		g_deathStarReactorExplosionOriginX = coreObj->world_x + offsetX;
		g_deathStarReactorExplosionOriginY = coreObj->world_y + offsetY;
		g_deathStarReactorExplosionOriginZ = coreObj->world_z + offsetZ;
		g_deathStarReactorShockwaveSpeed = DEATH_STAR_REACTOR_SHOCKWAVE_SPEED;
		g_deathStarReactorExplosionSpawnCount = 0;
		g_deathStarTunnelBillboardScale = 20.0f;
		g_deathStarReactorShockwaveDistance =
			(float)g_modelTypeTable[(uint16_t)coreObj->objectType].maxBoundsExtent;
		g_missionFlightGroups[(uint8_t)g_deathStarReactorCoreFgIdx].fg.status2 =
			DEATH_STAR_REACTOR_STATUS_DESTROYED;
		g_missionFlightGroups[(uint8_t)g_deathStarReactorCoreFgIdx].fg.status1 =
			DEATH_STAR_REACTOR_STATUS_DESTROYED;
	}

	DeathStar_SetOrderWaypointFromSegmentPoint(coreObjIdx, 0, DEATH_STAR_SEGMENT_POINT_KIND_SPAWN,
											   g_deathStarReactorAssaultFgIdx, 3, 0);
	DeathStar_SetOrderWaypointFromSegmentPoint(g_deathStarReactorCoreObjIdx, 0,
											   DEATH_STAR_SEGMENT_POINT_KIND_SPAWN,
											   g_deathStarReactorAssaultFgIdx, 1, 0);
	DeathStar_SetOrderWaypointFromSegmentPoint(g_deathStarReactorCoreObjIdx, 1u,
											   DEATH_STAR_SEGMENT_POINT_KIND_SPAWN,
											   g_deathStarReactorAssaultFgIdx, 1, 1);
	DeathStar_SetOrderWaypointFromSegmentPoint(g_deathStarReactorCoreObjIdx, 2u,
											   DEATH_STAR_SEGMENT_POINT_KIND_SPAWN,
											   g_deathStarReactorAssaultFgIdx, 1, 2);
	DeathStar_SetOrderWaypointFromSegmentPoint(g_deathStarReactorCoreObjIdx, 3u,
											   DEATH_STAR_SEGMENT_POINT_KIND_SPAWN,
											   g_deathStarReactorAssaultFgIdx, 1, 3);
	DeathStar_SetOrderWaypointFromSegmentPoint(g_deathStarReactorCoreRoomObjIdx, 0,
											   DEATH_STAR_SEGMENT_POINT_KIND_SPAWN,
											   g_deathStarReactorAssaultFgIdx, 1, 4);
	DeathStar_SetOrderWaypointFromSegmentPoint(g_deathStarReactorCoreRoomObjIdx, 1u,
											   DEATH_STAR_SEGMENT_POINT_KIND_SPAWN,
											   g_deathStarReactorAssaultFgIdx, 1, 5);
	DeathStar_SetOrderWaypointFromSegmentPoint(g_deathStarReactorCoreRoomObjIdx, 0,
											   DEATH_STAR_SEGMENT_POINT_KIND_ATTACH,
											   g_deathStarReactorAssaultFgIdx, 1, 6);

	coreRoomSegment = &g_deathStarSegmentSets[DEATH_STAR_REACTOR_SEGMENT_SET]
						   .segments[g_deathStarSegmentSets[DEATH_STAR_REACTOR_SEGMENT_SET].count - 1u];
	{
		uint8_t assaultFgIdx;
		int offsetX;
		int offsetY;
		int offsetZ;

		assaultFgIdx = g_deathStarReactorAssaultFgIdx;
		DeathStar_ComputeSegmentPointOffset(coreRoomSegment, 0, DEATH_STAR_SEGMENT_POINT_KIND_ATTACH,
											&offsetX, &offsetY, &offsetZ);
		g_missionFlightGroups[assaultFgIdx].fg.orders[1].waypoints[7].x =
			(int16_t)((coreRoomSegment->worldX + offsetX) >> 8);
		g_missionFlightGroups[assaultFgIdx].fg.orders[1].waypoints[7].y =
			(int16_t)(-(coreRoomSegment->worldY + offsetY) >> 8);
		g_missionFlightGroups[assaultFgIdx].fg.orders[1].waypoints[7].z =
			(int16_t)((coreRoomSegment->worldZ + offsetZ) >> 8);
	}

	{
		uint8_t assaultFgIdx;
		int offsetX;
		int offsetY;
		int offsetZ;

		assaultFgIdx = g_deathStarReactorAssaultFgIdx;
		DeathStar_ComputeSegmentPointOffset(coreRoomSegment, 0, DEATH_STAR_SEGMENT_POINT_KIND_ATTACH,
											&offsetX, &offsetY, &offsetZ);
		g_missionFlightGroups[assaultFgIdx].fg.missionPoints[XWA_FG_POINT_HYPER].x =
			(int16_t)((coreRoomSegment->worldX + offsetX) >> 8);
		g_missionFlightGroups[assaultFgIdx].fg.missionPoints[XWA_FG_POINT_HYPER].y =
			(int16_t)(-(coreRoomSegment->worldY + offsetY) >> 8);
		g_missionFlightGroups[assaultFgIdx].fg.missionPoints[XWA_FG_POINT_HYPER].z =
			(int16_t)((coreRoomSegment->worldZ + offsetZ) >> 8);
	}

	craft = g_objectTable[g_deathStarReactorAssaultCraftObjIdx].mobj->pCraft;
	craft->warheadData[g_modelDefs[craft->modelIndex].warheadLauncherFirstSlot[0]].count = 1;
	craft->warheadData[g_modelDefs[craft->modelIndex].warheadLauncherLastSlot[0]].count = 1;
}

// FUNCTION: XWA 0x428270
void DeathStar_InitLaserChamber(void) {
	DeathStarSegmentDef* chamberSegment;
	int segmentSetIdx;
	DeathStarSegmentSet* segmentSet;
	int localFwd;

	chamberSegment = NULL;
	g_deathStarLaserCooldownTimer = 1180;
	g_deathStarLaserFireTimer = 0;
	g_deathStarLaserChamberX = 0;
	g_deathStarLaserChamberY = 0;
	g_deathStarLaserChamberZ = 0;
	g_deathStarLaserChamberDirX = 0;
	g_deathStarLaserChamberDirY = 0;
	g_deathStarLaserChamberDirZ = 0;
	g_deathStarLaserEffectSlotCount = 0;
	g_deathStarLaserPowerSourceObjIdx = 0xffff;
	memset(g_deathStarLaserEffectSlots, 0, sizeof(g_deathStarLaserEffectSlots));

	for (segmentSetIdx = 0, segmentSet = g_deathStarSegmentSets; segmentSetIdx < 8;
		 ++segmentSet, ++segmentSetIdx) {
		uint16_t segmentCount;

		segmentCount = segmentSet->count;
		if (segmentCount != 0) {
			int segmentIdx;
			DeathStarSegmentDef* segmentBase;
			DeathStarSegmentDef* segment;

			segmentIdx = 0;
			segmentBase = segmentSet->segments;
			segment = segmentBase;
			while (segmentIdx < (int)segmentCount) {
				if (segment->objectType == OBJ_DSFocusChamber) {
					g_deathStarLaserChamberSegmentSetIdx = segmentSetIdx;
					g_deathStarLaserChamberSegmentIdx = (uint16_t)segmentIdx;
					chamberSegment = &segmentBase[segmentIdx];
					break;
				}

				++segmentIdx;
				++segment;
			}
		}

		if (chamberSegment) {
			break;
		}
	}

	if (!chamberSegment) {
		DebugPrintfChannel(0x200000, "Unable to locate laser chamber.\n");
		return;
	}

	g_deathStarLaserChamberX = chamberSegment->worldX;
	g_deathStarLaserChamberY = chamberSegment->worldY;
	g_deathStarLaserChamberZ = chamberSegment->worldZ;
	DebugPrintfChannel(0x200000, "Found laser chamber at %ld,%ld,%ld.\n", (long)chamberSegment->worldX,
					   (long)chamberSegment->worldY, (long)chamberSegment->worldZ);

	localFwd = ModelBounds_GetSizeY(OBJ_DSFocusChamber) >> 1;
	pai_RotateVectorByExplicitAnglesScratch(0, 0, -localFwd, chamberSegment->yaw, chamberSegment->pitch, 0,
											0);
	{
		int rotatedX;
		int rotatedY;
		int rotatedZ;
		int chamberX;
		int chamberY;
		int chamberZ;

		rotatedX = g_rotatedX;
		rotatedY = g_rotatedY;
		rotatedZ = g_rotatedZ;
		chamberZ = g_deathStarLaserChamberZ;
		chamberY = g_deathStarLaserChamberY;
		chamberX = g_deathStarLaserChamberX;
		chamberX += rotatedX;
		chamberY += rotatedY;
		chamberZ += rotatedZ;
		g_deathStarLaserChamberX = chamberX;
		g_deathStarLaserChamberY = chamberY;
		g_deathStarLaserChamberZ = chamberZ;
	}

	localFwd = -ModelBounds_GetSizeY(OBJ_DSLaserInternal);
	pai_RotateVectorByExplicitAnglesScratch(0, 0, localFwd, chamberSegment->yaw, chamberSegment->pitch, 0, 0);
	g_deathStarLaserChamberDirX = g_rotatedX >> 1;
	g_deathStarLaserChamberDirY = g_rotatedY >> 1;
	g_deathStarLaserChamberDirZ = g_rotatedZ >> 1;
}

// FUNCTION: XWA 0x428440
void DeathStar_UpdateEntranceSegment(int16_t activeSegmentSlotIdx, int16_t currentSegmentSlotIdx) {
	ObjectRecord* playerObj;

	if (activeSegmentSlotIdx != currentSegmentSlotIdx) {
		return;
	}

	if (currentSegmentSlotIdx > 0) {
		g_deathStarEntranceProximityArmed = 1;
	}

	if (g_deathStarEntranceProximityArmed) {
		ObjectRecord* activeSegmentObj;
		int distanceToEntrance;

		playerObj = &g_objectTable[g_deathStarPlayerObjIdx];
		activeSegmentObj = &g_objectTable[g_deathStarActiveSegmentObjIdx[currentSegmentSlotIdx]];
		distanceToEntrance = collide_roughdistance3d(playerObj->world_x - activeSegmentObj->world_x,
													 playerObj->world_y - activeSegmentObj->world_y,
													 playerObj->world_z - activeSegmentObj->world_z);
		if (distanceToEntrance < 27250) {
			g_flightMissionEndPending = 1;
		}
	}

	{
		ObjectRecord* transitionPlayerObj;
		int transitionPlayerObjIdx;

		transitionPlayerObjIdx = g_deathStarPlayerObjIdx;
		transitionPlayerObj = &g_objectTable[transitionPlayerObjIdx];
		switch (g_deathStarEntranceTransitionState) {
			case 0: {
				int maxBoundsExtent;

				g_deathStarEntranceTransitionState = 1;
				Player_StepExtView(g_localPlayer);
				g_deathStarEntranceTransitionTimer = 800;

				maxBoundsExtent =
					g_modelTypeTable[g_objectTable[g_deathStarPlayerObjIdx].objectType].maxBoundsExtent;
				g_players[g_localPlayer].viewState.cameraDistance = maxBoundsExtent / 3 + 2 * maxBoundsExtent;
				g_players[g_localPlayer].viewState.hudAimX = -4097;
				g_players[g_localPlayer].viewState.hudAimY = 30720;
				FlightView_UpdatePlayerCamera(g_localPlayer);
				break;
			}

			case 1: {
				int localPlayerIdx;
				int maxBoundsExtent;

				g_deathStarEntranceTransitionTimer =
					g_deathStarEntranceTransitionTimer - (int)(uint16_t)g_elapsedTicks;
				if (g_deathStarEntranceTransitionTimer < 0) {
					g_deathStarEntranceTransitionTimer = 0;
					transitionPlayerObj->mobj->pCraft->objectKind = DEATH_STAR_CRAFT_KIND_NORMAL;
					Player_StepExtView(g_localPlayer);
					Hud_SetHudViewState(19, g_localPlayer);
					g_deathStarEntranceTransitionState = 2;
					break;
				}

				localPlayerIdx = g_localPlayer;
				maxBoundsExtent =
					g_modelTypeTable[g_objectTable[transitionPlayerObjIdx].objectType].maxBoundsExtent;
				g_players[localPlayerIdx].viewState.cameraDistance =
					maxBoundsExtent / 3 + 2 * g_deathStarEntranceTransitionTimer * maxBoundsExtent / 800;
				if (g_deathStarEntranceTransitionTimer < 500) {
					g_players[localPlayerIdx].viewState.hudAimX =
						(int16_t)(-1 - (g_deathStarEntranceTransitionTimer << 12) / 500);
					g_players[localPlayerIdx].viewState.hudAimY =
						(int16_t)(30720 * g_deathStarEntranceTransitionTimer / 500);
				}
				FlightView_UpdatePlayerCamera(localPlayerIdx);
				break;
			}
		}
	}
}

// FUNCTION: XWA 0x428680
void DeathStar_UpdateAccelChamberSegment(int16_t activeSegmentSlotIdx, int16_t currentSegmentSlotIdx) {
	int chamberObjIdx;
	ObjectRecord* chamberObj;
	unsigned int objectIdx;

	(void)currentSegmentSlotIdx;

	if ((unsigned int)(g_deathStarTunnelTimer - g_deathStarAccelChamberLastContainerSpawnTime) >
		(unsigned int)g_deathStarAccelChamberContainerSpawnInterval) {
		objectIdx = Object_AllocSlotForGenus(g_modelTypeTable[OBJ_DSContainer].genusId);
		if (objectIdx != 0xffffu) {
			ObjectRecord* object;
			MobileObject* mobj;
			int byteIdx;

			chamberObjIdx = g_deathStarActiveSegmentObjIdx[activeSegmentSlotIdx];
			g_deathStarAccelChamberContainersCleared = 0;

			object = &g_objectTable[objectIdx];
			object->objectSignature = g_nextObjectSignature++;
			if (g_nextObjectSignature == 0) {
				g_nextObjectSignature = 2;
			}

			object->flightGroupIdx = g_deathStarGeneratedObjectFgIdx;
			object->objectType = OBJ_DSContainer;
			object->genusId = g_modelTypeTable[OBJ_DSContainer].genusId;
			object->roll = 0;
			object->regionIdx = 0;
			object->typeSpecificWord = 0;
			for (byteIdx = 0; byteIdx < 2; ++byteIdx) {
				object->typeSpecificByte[byteIdx] = 0;
			}
			object->playerOwnerIdx = -1;

			DeathStar_ComputeSegmentPointOffset(
				&g_deathStarSegmentSets[g_deathStarSegmentSetIdx]
					 .segments[g_deathStarActiveSegmentIdx[activeSegmentSlotIdx]],
				2u, DEATH_STAR_SEGMENT_POINT_KIND_ATTACH, &object->world_x, &object->world_y,
				&object->world_z);

			object->world_x += g_objectTable[chamberObjIdx].world_x;
			object->world_y += g_objectTable[chamberObjIdx].world_y;
			object->world_z += g_objectTable[chamberObjIdx].world_z;

			DeathStar_InitMobileObjectForType(OBJ_DSContainer, objectIdx);

			object->mobj->sourceObjIdx = (uint16_t)chamberObjIdx;
			object->mobj->sourceObjectType = OBJ_DSAccelChamber;
			object->yaw = g_objectTable[chamberObjIdx].yaw;
			mobj = object->mobj;
			object->pitch =
				(Q16Angle)(g_objectTable[chamberObjIdx].pitch + g_deathStarAccelChamberPitchOffset);
			mobj->speed = 50;
			object->mobj->speedRemainder = 0;
			object->mobj->pCraft->objectKind = 9;
		}

		g_deathStarAccelChamberLastContainerSpawnTime = g_deathStarTunnelTimer;
	}

	chamberObjIdx = g_deathStarActiveSegmentObjIdx[activeSegmentSlotIdx];
	chamberObj = &g_objectTable[chamberObjIdx];

	chamberObj->typeSpecificWord = 0;
	DeathStar_PullObjectTowardAccelChamber(chamberObjIdx, &g_objectTable[g_deathStarPlayerObjIdx]);

	for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		ObjectRecord* object;

		object = &g_objectTable[objectIdx];
		if (object->objectType == OBJ_DSContainer) {
			DeathStar_PullObjectTowardAccelChamber(chamberObjIdx, object);
		}
	}

	DeathStar_UpdateLoopingObjectSfxVolume(0, (unsigned int)chamberObjIdx, 0);

	if (chamberObj->typeSpecificWord != 0) {
		FlightLight_AppendScenePointLightForObject(chamberObj);
	}

	if (g_deathStarAccelChamberLightTimer != 0) {
		g_deathStarAccelChamberLightTimer -= (int)(uint16_t)g_elapsedTicks;
		if (g_deathStarAccelChamberLightTimer < 0) {
			g_deathStarAccelChamberLightTimer = 0;
		}

		g_objectTable[chamberObjIdx].typeSpecificWord = (uint16_t)(g_deathStarAccelChamberLightTimer << 6);
		FlightLight_AppendScenePointLightForObject(&g_objectTable[chamberObjIdx]);
	}
}

// FUNCTION: XWA 0x428920
void DeathStar_UpdateLoopingObjectSfxVolume(int channelIdx, unsigned int sourceObjIdx, int useDirectVolume) {
	int volume;
	unsigned int priority;
	unsigned int computedVolume;
	uint16_t sourceScale;

	if (useDirectVolume) {
		uint16_t directVolume;

		directVolume = g_objectTable[sourceObjIdx].typeSpecificWord;
		if (directVolume > 127) {
			volume = 127;
		} else {
			volume = directVolume;
		}
	} else {
		computedVolume = fsfx_ComputeSourceVolume(
			sourceObjIdx, (unsigned int)g_deathStarLoopSfxSlotByChannel[channelIdx], &priority);
		sourceScale = g_objectTable[sourceObjIdx].typeSpecificWord;
		volume = 127;
		if (computedVolume * (sourceScale >> 7) <= 127u) {
			volume = (int)(computedVolume * (sourceScale >> 7));
		}
	}

	if (g_deathStarLoopSfxVolume[channelIdx] == volume) {
		return;
	}

	{
		uint8_t isActive = g_deathStarLoopSfxActive[channelIdx];

		if (volume == 0) {
			if (isActive == 1) {
				Sound_StopOldestInstance(g_sfxIds[g_deathStarLoopSfxSlotByChannel[channelIdx]]);
				g_deathStarLoopSfxActive[channelIdx] = 0;
				g_deathStarLoopSfxVolume[channelIdx] = 0;
			}
			return;
		}

		if (isActive == 0) {
			if (Sound_QueueEffect(g_sfxIds[g_deathStarLoopSfxSlotByChannel[channelIdx]], 1, 1, 125, volume,
								  64, -1, sourceObjIdx) != -1) {
				g_deathStarLoopSfxActive[channelIdx] = 1;
				g_deathStarLoopSfxVolume[channelIdx] = volume;
				return;
			}
		} else {
			Sound_SetLatestInstanceVolume(g_sfxIds[g_deathStarLoopSfxSlotByChannel[channelIdx]], volume);
		}
		g_deathStarLoopSfxVolume[channelIdx] = volume;
	}
}

// FUNCTION: XWA 0x428A50
void DeathStar_PullObjectTowardAccelChamber(int chamberObjIdx, ObjectRecord* targetObj) {
	ObjectRecord* chamberObj;
	MobileObject* targetMobj;
	CraftData* targetCraft;
	unsigned int distance;
	int pullSpeed;
	int targetWorldY;
	double deltaX;
	int targetWorldZ;
	double deltaY;
	double deltaZ;
	double exactDistance;
	double pullDistance;
	uint16_t lightIntensity;

	chamberObj = &g_objectTable[chamberObjIdx];
	distance = collide_roughdistance3d(chamberObj->world_x - targetObj->world_x,
									   chamberObj->world_y - targetObj->world_y,
									   chamberObj->world_z - targetObj->world_z);
	if (distance >= 15000u) {
		return;
	}

	targetMobj = targetObj->mobj;
	targetCraft = targetMobj->pCraft;
	if (targetCraft->objectKind == DEATH_STAR_CRAFT_KIND_ACCEL_CHAMBER_PULL) {
		return;
	}

	if (targetObj->yaw == chamberObj->yaw && targetObj->pitch == chamberObj->pitch && distance < 700u) {
		targetMobj->speed = 3000;
		targetObj->mobj->pCraft->objectKind = DEATH_STAR_CRAFT_KIND_ACCEL_CHAMBER_PULL;
		g_deathStarAccelChamberLightTimer = 250;
		fsfx_PlaySound(161, chamberObjIdx, (unsigned int)g_localPlayer);
		return;
	}

	pullSpeed = 1500000u / (((distance >> 4) * (distance >> 4)) + 1);
	if (pullSpeed >= 20) {
		pullSpeed = 20;
	}

	targetWorldY = targetObj->world_y;
	deltaX = (double)(chamberObj->world_x - targetObj->world_x);
	targetWorldZ = targetObj->world_z;
	deltaY = (double)(chamberObj->world_y - targetWorldY);
	deltaZ = (double)(chamberObj->world_z - targetWorldZ);
	exactDistance = sqrt(deltaX * deltaX + deltaZ * deltaZ + deltaY * deltaY);
	pullDistance = (double)((int)(uint16_t)g_elapsedTicks * pullSpeed);

	if (pullDistance > exactDistance) {
		targetObj->world_x = chamberObj->world_x;
		targetObj->world_y = chamberObj->world_y;
		targetObj->world_z = chamberObj->world_z;
	} else {
		double pullFraction;

		pullFraction = pullDistance / exactDistance;
		targetObj->world_x = targetObj->world_x + (int)(pullFraction * deltaX);
		targetObj->world_y = targetWorldY + (int)(pullFraction * deltaY);
		targetObj->world_z = targetWorldZ + (int)(pullFraction * deltaZ);
	}

	lightIntensity = (uint16_t)((30720000u - (distance << 11)) / 15000u + 256u);
	if (lightIntensity > chamberObj->typeSpecificWord) {
		chamberObj->typeSpecificWord = (uint16_t)(lightIntensity | 1u);
	}

	if (distance < 3000u) {
		DeathStar_SteerObjectAnglesToward(chamberObj->yaw, chamberObj->pitch, targetObj, (int)distance, 700);
	}
}

// FUNCTION: XWA 0x428C40
void DeathStar_SteerObjectAnglesToward(Q16Angle targetYaw, Q16Angle targetPitch, ObjectRecord* object,
									   int distance, int rangeScale) {
	unsigned int scaledStep;
	int16_t pitchNegative;
	int16_t yawNegative;
	Q16Angle step;
	Q16Angle currentYaw;
	Q16Angle yawDelta;
	Q16Angle currentPitch;
	Q16Angle pitchDelta;

	scaledStep = 40u * (unsigned int)g_elapsedTicks * distance / (unsigned int)(rangeScale + 1);
	pitchNegative = 0;
	yawNegative = 0;
	step = 0x7fffu;
	if (scaledStep <= 0x7fffu) {
		step = (Q16Angle)scaledStep;
	}
	if (step < 0x200u) {
		step = 0x200u;
	}

	currentYaw = object->yaw;
	yawDelta = (Q16Angle)(targetYaw - currentYaw);
	if (yawDelta > 0x8000u) {
		yawNegative = 1;
		yawDelta = (Q16Angle)-yawDelta;
	}
	if (yawDelta > step) {
		yawDelta = step;
	}
	if (yawNegative) {
		object->yaw = (Q16Angle)(currentYaw - yawDelta);
	} else {
		object->yaw = (Q16Angle)(currentYaw + yawDelta);
	}

	currentPitch = object->pitch;
	pitchDelta = (Q16Angle)(targetPitch - currentPitch);
	if (pitchDelta > 0x8000u) {
		pitchNegative = 1;
		pitchDelta = (Q16Angle)-pitchDelta;
	}
	if (pitchDelta > step) {
		pitchDelta = step;
	}
	if (pitchNegative) {
		object->pitch = (Q16Angle)(currentPitch - pitchDelta);
	} else {
		object->pitch = (Q16Angle)(currentPitch + pitchDelta);
	}

	object->mobj->orientMatrixDirty = 1;
	object->mobj->moveVectorDirty = 1;
}

// FUNCTION: XWA 0x428D10
void DeathStar_UpdateContainerCollisions(int16_t activeSegmentSlotIdx, int16_t currentSegmentSlotIdx) {
	(void)currentSegmentSlotIdx;

	DeathStar_UpdateContainerCollision(activeSegmentSlotIdx, g_deathStarPlayerObjIdx);

	{
		ModelGenusId containerGenus;
		uint32_t objectIdx;

		containerGenus = g_modelTypeTable[OBJ_DSContainer].genusId;
		for (objectIdx = g_objectSlotRangeByGenus[containerGenus].next;
			 objectIdx < g_objectSlotRangeByGenus[containerGenus].end; ++objectIdx) {
			if (g_objectTable[objectIdx].objectType == OBJ_DSContainer) {
				DeathStar_UpdateContainerCollision(activeSegmentSlotIdx, objectIdx);
			}
		}
	}

	if (g_deathStarContainerCollisionLightTimer != 0) {
		ObjectRecord* activeSegmentObj;

		g_deathStarContainerCollisionLightTimer -= (int)(uint16_t)g_elapsedTicks;
		if (g_deathStarContainerCollisionLightTimer < 0) {
			g_deathStarContainerCollisionLightTimer = 0;
		}

		activeSegmentObj = &g_objectTable[g_deathStarActiveSegmentObjIdx[(int16_t)activeSegmentSlotIdx]];
		activeSegmentObj->typeSpecificWord = (uint16_t)(g_deathStarContainerCollisionLightTimer << 5);
		FlightLight_AppendScenePointLightForObject(activeSegmentObj);
	}
}

// FUNCTION: XWA 0x428DD0
void DeathStar_UpdateContainerCollision(int16_t activeSegmentSlotIdx, int containerObjIdx) {
	int segmentObjIdx;
	ObjectRecord* segmentObj;
	ObjectRecord* containerObj;
	unsigned int distanceToSegment;
	int outX;
	int outY;
	int outZ;

	segmentObjIdx = g_deathStarActiveSegmentObjIdx[activeSegmentSlotIdx];
	segmentObj = &g_objectTable[segmentObjIdx];
	containerObj = &g_objectTable[containerObjIdx];
	distanceToSegment = collide_roughdistance3d(segmentObj->world_x - containerObj->world_x,
												segmentObj->world_y - containerObj->world_y,
												segmentObj->world_z - containerObj->world_z);

	if (containerObj->mobj->pCraft->objectKind == DEATH_STAR_CRAFT_KIND_ACCEL_CHAMBER_PULL &&
		distanceToSegment < 5000u && containerObj->mobj->speed > 60u) {
		uint16_t previousSpeed;

		previousSpeed = containerObj->mobj->speed;
		containerObj->mobj->speed = (uint16_t)(3000u * distanceToSegment / 5000u);
		if (containerObj->mobj->speed < 60u || previousSpeed <= containerObj->mobj->speed) {
			containerObj->mobj->speed = 60;
			if (containerObj->objectType == OBJ_DSContainer) {
				containerObj->mobj->pCraft->objectKind = 9;
			} else {
				containerObj->mobj->pCraft->objectKind = DEATH_STAR_CRAFT_KIND_NORMAL;
				if (containerObjIdx == g_deathStarPlayerObjIdx) {
					DeathStar_RefreshFollowOverrideCandidates();
				}
			}
		}

		if (distanceToSegment < 15000u) {
			segmentObj->typeSpecificWord =
				(uint16_t)(((30720000u - (distanceToSegment << 11)) / 15000u + 256u) | 1u);
			FlightLight_AppendScenePointLightForObject(segmentObj);
			g_deathStarContainerCollisionLightTimer = 250;
		}
		fsfx_PlaySound(162, segmentObjIdx, (unsigned int)g_localPlayer);
		return;
	}

	if (containerObj->objectType != OBJ_DSContainer) {
		return;
	}

	DeathStar_ComputeSegmentPointOffset(&g_deathStarSegmentSets[g_deathStarSegmentSetIdx]
											 .segments[g_deathStarActiveSegmentIdx[activeSegmentSlotIdx]],
										3u, DEATH_STAR_SEGMENT_POINT_KIND_ATTACH, &outX, &outY, &outZ);
	{
		unsigned int distanceToExitPoint;

		distanceToExitPoint = collide_roughdistance3d(segmentObj->world_x + outX - containerObj->world_x,
													  segmentObj->world_y + outY - containerObj->world_y,
													  segmentObj->world_z + outZ - containerObj->world_z);
		if (distanceToExitPoint < 1000u) {
			DeathStar_SteerObjectAnglesToward(
				segmentObj->yaw + 0x8000u, 0x8000u - segmentObj->pitch - g_deathStarAccelChamberPitchOffset,
				containerObj, (int)distanceToExitPoint, 1000);
		}
	}

	DeathStar_ComputeSegmentPointOffset(&g_deathStarSegmentSets[g_deathStarSegmentSetIdx]
											 .segments[g_deathStarActiveSegmentIdx[activeSegmentSlotIdx]],
										2u, DEATH_STAR_SEGMENT_POINT_KIND_ATTACH, &outX, &outY, &outZ);
	if ((unsigned int)collide_roughdistance3d(segmentObj->world_x + outX - containerObj->world_x,
											  segmentObj->world_y + outY - containerObj->world_y,
											  segmentObj->world_z + outZ - containerObj->world_z) < 800u) {
		containerObj->objectType = OBJ_None;
	}
}

// FUNCTION: XWA 0x429070
void DeathStar_ClearAccelChamberContainersAtSegment(int16_t currentSegmentIdx, int16_t triggerSegmentIdx) {
	ModelGenusId containerGenus;
	uint16_t objectIdx;
	unsigned int activeSegmentObjIdx;

	if (currentSegmentIdx != triggerSegmentIdx || g_deathStarAccelChamberContainersCleared) {
		return;
	}

	if (g_objectTable[g_deathStarPlayerObjIdx].mobj->pCraft->objectKind ==
		DEATH_STAR_CRAFT_KIND_ACCEL_CHAMBER_PULL) {
		g_objectTable[g_deathStarPlayerObjIdx].mobj->pCraft->objectKind = DEATH_STAR_CRAFT_KIND_NORMAL;
	}

	containerGenus = g_modelTypeTable[OBJ_DSContainer].genusId;
	for (objectIdx = (uint16_t)g_objectSlotRangeByGenus[containerGenus].next;
		 objectIdx < g_objectSlotRangeByGenus[containerGenus].end; ++objectIdx) {
		if (g_objectTable[objectIdx].objectType == OBJ_DSContainer) {
			g_objectTable[objectIdx].objectType = OBJ_None;
		}
	}

	activeSegmentObjIdx = (unsigned int)g_deathStarActiveSegmentObjIdx[currentSegmentIdx];
	g_objectTable[activeSegmentObjIdx].typeSpecificWord = 0;
	DeathStar_UpdateLoopingObjectSfxVolume(0, activeSegmentObjIdx, 0);
	g_deathStarAccelChamberContainersCleared = 1;
}

// FUNCTION: XWA 0x429150
void DeathStar_EnableFollowOverrideAtSegment(int16_t currentSegmentIdx, int16_t triggerSegmentIdx) {
	unsigned int objectIdx;

	if (currentSegmentIdx == triggerSegmentIdx && g_deathStarFollowRefreshPending != 1) {
		for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
			 ++objectIdx) {
			ObjectRecord* object;

			object = &g_objectTable[objectIdx];
			if (object->objectType != OBJ_None && (int)objectIdx != g_deathStarFollowLeaderObjIdx &&
				(object->genusId == GENUS_Fighter || object->genusId == GENUS_Transport)) {
				paiman_BeginPlayerFollowOverride((int)objectIdx, g_localPlayer);
			}
		}

		g_deathStarFollowRefreshPending = 1;
	}
}

// FUNCTION: XWA 0x4291D0
void DeathStar_ActivateTripodGunsNearFocusChamber(int16_t currentSegmentIdx, int16_t triggerSegmentIdx) {
	ObjectRecord* playerObj;
	ObjectRecord* focusChamberObj;
	int distanceToFocusChamber;
	int activationDistance;

	(void)currentSegmentIdx;
	(void)triggerSegmentIdx;

	playerObj = &g_objectTable[g_deathStarPlayerObjIdx];
	if (playerObj->mobj->collisionObjIdx != g_deathStarFocusChamberObjIdx) {
		return;
	}
	if (g_deathStarTripodGunsActivated) {
		return;
	}

	focusChamberObj = &g_objectTable[g_deathStarFocusChamberObjIdx];
	distanceToFocusChamber = collide_roughdistance3d(playerObj->world_x - focusChamberObj->world_x,
													 playerObj->world_y - focusChamberObj->world_y,
													 playerObj->world_z - focusChamberObj->world_z);

	activationDistance = (int)((float)g_modelTypeTable[OBJ_DS3rdRoom].maxBoundsExtent *
							   g_deathStarTripodGunActivationDistanceScale);
	if (distanceToFocusChamber < activationDistance) {
		DeathStar_RefreshFollowOverrideCandidates();
		g_modelTypeTable[OBJ_DSTripodGun].flags |= 0x0003u;
		g_deathStarTripodGunsActivated = 1;
	}
}

// FUNCTION: XWA 0x429260
void DeathStar_UpdateReactorCylinderAndFollowMode(int16_t currentSegmentIdx, int16_t triggerSegmentIdx) {
	ObjectRecord* objectTable;

	if (g_deathStarReactorCylinderObjIdx != 0xffff) {
		ObjectRecord* cylinderObjectTable;
		int animTimer;

		cylinderObjectTable = g_objectTable;
		if (OBJ_DSReactorCylinder == (cylinderObjectTable + g_deathStarReactorCylinderObjIdx)->objectType) {
#ifdef XWA_MODERN
			if (!XwaModernFlightTiming_IsHighRate() || XwaModernFlightTiming_IsLegacyCadenceDue()) {
#endif
				g_objectTable[g_deathStarReactorCylinderObjIdx].typeSpecificWord =
					(uint16_t)(((uint16_t)GameRand() & 0xffu) + 0x2000u);
#ifdef XWA_MODERN
			}
#endif
			FlightLight_AppendScenePointLightForObject(&g_objectTable[g_deathStarReactorCylinderObjIdx]);

			g_objectTable[g_deathStarReactorCylinderObjIdx].yaw =
				(Q16Angle)(g_objectTable[g_deathStarReactorCylinderObjIdx].yaw +
#ifdef XWA_MODERN
						   (int16_t)(XwaModernFlightTiming_IsHighRate()
										 ? DeathStar_ModernScaleWithRemainder(
											   1000, (uint16_t)g_elapsedTicks, 236,
											   &g_modernDeathStarReactorCylinderYawRemainder)
										 : 1000 * (uint16_t)g_elapsedTicks / 236));
#else
						   (int16_t)(1000 * (uint16_t)g_elapsedTicks / 236));
#endif
			g_objectTable[g_deathStarReactorCylinderObjIdx].mobj->moveVectorDirty = 1;
			g_objectTable[g_deathStarReactorCylinderObjIdx].mobj->orientMatrixDirty = 1;

			animTimer = g_deathStarReactorCylinderAnimTimer - (int)(uint16_t)g_elapsedTicks;
			g_deathStarReactorCylinderAnimTimer = animTimer;
			if (animTimer < 0) {
				g_deathStarReactorCylinderAnimTimer = (uint16_t)GameRand() % 7 + 20;
				++g_objectTable[g_deathStarReactorCylinderObjIdx].mobj->nodeSwitchIndex;

				if (g_objectTable[g_deathStarReactorCylinderObjIdx].mobj->nodeSwitchIndex >= 8u) {
					g_objectTable[g_deathStarReactorCylinderObjIdx].mobj->nodeSwitchIndex = 0;
				}
			}
		}
	}

	objectTable = g_objectTable;
	if (currentSegmentIdx != triggerSegmentIdx) {
		return;
	}

	if (objectTable[g_deathStarPlayerObjIdx].mobj->collisionObjIdx == g_deathStarReactorCoreRoomObjIdx ||
		objectTable[g_deathStarPlayerObjIdx].mobj->collisionObjIdx == g_deathStarReactorCoreObjIdx) {
		DeathStar_RefreshFollowOverrideCandidates();
		return;
	}

	if (g_deathStarFollowRefreshPending != 1) {
		unsigned int objectIdx;

		for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
			 ++objectIdx) {
			ObjectRecord* object;

			object = &objectTable[objectIdx];
			if (object->objectType != OBJ_None && objectIdx != (unsigned int)g_deathStarFollowLeaderObjIdx &&
				(object->genusId == GENUS_Fighter || object->genusId == GENUS_Transport)) {
				paiman_BeginPlayerFollowOverride(objectIdx, g_localPlayer);
				objectTable = g_objectTable;
			}
		}

		g_deathStarFollowRefreshPending = 1;
	}
}

// FUNCTION: XWA 0x429480
// Advance the timed Death Star reactor-core destruction sequence. Below 0x96 ticks it
// only shakes/drifts the core. Otherwise it spawns up to 36 staged explosion objects,
// then drives the expanding shockwave-ring objects along the reactor->player axis with
// proximity damage / point light / looping SFX, shaking and drifting the core. When the
// shockwave passes the player it sets g_flightMissionEndPending; past 0x9F6 ticks it
// destroys the reactor core/cylinder objects and clears the reactor flight-group status.
void DeathStarTunnel_UpdateReactorDestructionSequence(int16_t activeSegmentSlotIdx) {
	unsigned int timer = (uint16_t)g_elapsedTicks + g_deathStarReactorDestructionTimer;
	g_deathStarReactorDestructionTimer = timer;

	if (timer > 0x96) {
		unsigned int spawnCount = g_deathStarReactorExplosionSpawnCount;

		if (spawnCount < 0x24) {
			float dirZ = 0.0f, dirDist = 0.0f, dirY = 0.0f, dirX = 0.0f;
			unsigned int target = (36 * timer - 5400) / 0x960;

			while (1) {
				unsigned int limit = target;
				ObjectTypeId explosionType;
				unsigned int extent;
				int ext270;
				unsigned int newIdx;

				if (target >= 0x24)
					limit = 36;
				if (spawnCount >= limit)
					break;

				if (spawnCount < 0x1E || (spawnCount & 3) != 0) {
					uint16_t r = (uint16_t)GameRand();
					explosionType = (ObjectTypeId)(r % 5 + 264);
					extent = spawnCount / 3 *
							 ((unsigned int)g_modelTypeTable[(uint16_t)explosionType].maxBoundsExtent >> 2);
					ext270 = g_modelTypeTable[270].maxBoundsExtent;
				} else {
					ext270 = g_modelTypeTable[270].maxBoundsExtent;
					explosionType = (ObjectTypeId)270;
					extent = (unsigned int)g_modelTypeTable[270].maxBoundsExtent * spawnCount;
				}
				if (spawnCount == 35) {
					explosionType = (ObjectTypeId)270;
					extent = 70 * ext270;
				}

				newIdx = DeathStarTunnel_SpawnExplosionEffectObject(explosionType, extent);
				if (newIdx == 0xFFFF)
					break;

				if (dirDist == 0.0f) {
					ObjectRecord* player = &g_objectTable[g_deathStarPlayerObjIdx];
					float dx = (float)(player->world_x - g_deathStarReactorExplosionOriginX);
					float dy = (float)(player->world_y - g_deathStarReactorExplosionOriginY);
					float dz = (float)(player->world_z - g_deathStarReactorExplosionOriginZ);
					dirDist = sqrt(dx * dx + dy * dy + dz * dz);
					dirX = dx / dirDist;
					dirY = dy / dirDist;
					dirZ = dz / dirDist;
				}

				g_objectTable[newIdx].world_x = (int)(int64_t)(dirX * g_deathStarReactorShockwaveDistance) +
												(uint16_t)GameRand() % 8000 +
												g_deathStarReactorExplosionOriginX - 4000;
				g_objectTable[newIdx].world_y = (int)(int64_t)(dirY * g_deathStarReactorShockwaveDistance) +
												(uint16_t)GameRand() % 8000 +
												g_deathStarReactorExplosionOriginY - 4000;
				g_objectTable[newIdx].world_z = (int)(int64_t)(dirZ * g_deathStarReactorShockwaveDistance) +
												(uint16_t)GameRand() % 8000 +
												g_deathStarReactorExplosionOriginZ - 4000;
				++g_deathStarReactorExplosionSpawnCount;
				++spawnCount;
			}

			if (g_deathStarReactorExplosionSpawnCount == 36) {
				unsigned int i;
				unsigned int frame = 0;
				for (i = 0; i < 5; ++i) {
					unsigned int newIdx = DeathStarTunnel_SpawnExplosionEffectObject(
						OBJ_DeathStarIITextureGroup17002, g_modelTypeTable[321].maxBoundsExtent);
					g_deathStarReactorShockwaveObjIdx[i] = newIdx;
					if (newIdx != 0xFFFF) {
						g_objectTable[newIdx].typeSpecificByte[0] =
							(uint8_t)(frame % g_modelTypeTable[(uint16_t)g_objectTable[newIdx].objectType]
												  .frameCount);
						g_objectTable[g_deathStarReactorShockwaveObjIdx[i]].typeSpecificWord = 1000;
					}
					frame += 2;
				}
				g_deathStarReactorShockwaveDistance = g_deathStarReactorShockwaveDistance * 0.5f;
			}
		} else {
			ObjectRecord* player;
			float dx, dy, dz, dist, dirX, dirY, dirZ;

			g_deathStarReactorShockwaveDistance += (float)(((unsigned int)(uint16_t)g_elapsedTicks *
															(unsigned int)g_deathStarReactorShockwaveSpeed) /
														   0xECu);
			player = &g_objectTable[g_deathStarPlayerObjIdx];
			dx = (float)(player->world_x - g_deathStarReactorExplosionOriginX);
			dy = (float)(player->world_y - g_deathStarReactorExplosionOriginY);
			dz = (float)(player->world_z - g_deathStarReactorExplosionOriginZ);
			dist = sqrt(dx * dx + dy * dy + dz * dz);
			dirX = dx / dist;
			dirY = dy / dist;
			dirZ = dz / dist;

			if (g_deathStarReactorShockwaveDistance > dist + 10000.0f) {
				g_flightMissionEndPending = 1;
			} else {
				float ringDist = g_deathStarReactorShockwaveDistance;
				unsigned int i;

				for (i = 0; i < 5; ++i) {
					if (g_deathStarReactorShockwaveObjIdx[i] != 0xFFFF) {
						g_objectTable[g_deathStarReactorShockwaveObjIdx[i]].world_x =
							g_deathStarReactorExplosionOriginX + (int)(int64_t)(ringDist * dirX);
						g_objectTable[g_deathStarReactorShockwaveObjIdx[i]].world_y =
							g_deathStarReactorExplosionOriginY + (int)(int64_t)(ringDist * dirY);
						g_objectTable[g_deathStarReactorShockwaveObjIdx[i]].world_z =
							g_deathStarReactorExplosionOriginZ + (int)(int64_t)(ringDist * dirZ);
						ringDist -= 3000.0f;
						collide_ApplyProximityDamageFalloff(g_deathStarReactorShockwaveObjIdx[i],
															2500 * (uint16_t)g_elapsedTicks / 236, 0x4E20u,
															g_deathStarReactorShockwaveObjIdx[i]);
					}
				}

				if (g_deathStarReactorShockwaveObjIdx[0] != 0xFFFF) {
					float diff;
					float intensity;
#ifndef XWA_MODERN
					uint16_t lightValue;

					lightValue = (uint16_t)GameRand();
					g_objectTable[g_deathStarReactorShockwaveObjIdx[0]].typeSpecificWord =
						(uint16_t)((lightValue & 0x7FF) + 3072);
#else
					if (XwaModernFlightTiming_IsHighRate()) {
						if (XwaModernFlightTiming_IsLegacyCadenceDue()) {
							g_modernDeathStarShockwaveLightValue = (uint16_t)GameRand();
						}
						g_objectTable[g_deathStarReactorShockwaveObjIdx[0]].typeSpecificWord =
							(uint16_t)((g_modernDeathStarShockwaveLightValue & 0x7FF) + 3072);
					} else {
						uint16_t lightValue;

						lightValue = (uint16_t)GameRand();
						g_objectTable[g_deathStarReactorShockwaveObjIdx[0]].typeSpecificWord =
							(uint16_t)((lightValue & 0x7FF) + 3072);
					}
#endif
					FlightLight_AppendScenePointLightForObject(
						&g_objectTable[g_deathStarReactorShockwaveObjIdx[0]]);
					if (dist > g_deathStarReactorShockwaveDistance)
						diff = dist - g_deathStarReactorShockwaveDistance;
					else
						diff = g_deathStarReactorShockwaveDistance - dist;
					intensity = 100000.0f - diff;
					if (0.0f > intensity)
						intensity = 0.0f;
					intensity *= 300.0f;
					intensity *= 0.0000099999997f;
					g_objectTable[g_deathStarReactorShockwaveObjIdx[0]].typeSpecificWord =
						(uint16_t)(int64_t)intensity;
					DeathStar_UpdateLoopingObjectSfxVolume(1, g_deathStarReactorShockwaveObjIdx[0], 1);
				}
			}
		}
	}

	if ((unsigned int)g_deathStarReactorDestructionTimer > 0x9F6) {
		if (g_deathStarReactorCoreObjIdx != 0xFFFF) {
			int fgIdx = (uint8_t)g_deathStarReactorCoreFgIdx;
			g_missionFlightGroups[fgIdx].fg.status2 = 0;
			g_missionFlightGroups[fgIdx].fg.status1 = 0;
			collide_damagecraft(g_deathStarReactorCoreObjIdx, 0xFFFFu, 0xFFFFFFFDu, 0x2710u, 0);
			g_objectTable[g_deathStarReactorCoreObjIdx].objectType = OBJ_None;
			if ((uint16_t)g_players[g_localPlayer].currentTargetObjectIdx == g_deathStarReactorCoreObjIdx)
				g_players[g_localPlayer].currentTargetObjectIdx = 0xffffu;
			g_deathStarReactorCoreObjIdx = 0xFFFF;
			if (g_deathStarReactorCylinderObjIdx != 0xFFFF) {
				g_objectTable[g_deathStarReactorCylinderObjIdx].objectType = OBJ_None;
				g_deathStarReactorCylinderObjIdx = 0xFFFF;
			}
			g_dirLightCount = 0;
		}
		return;
	}

	{
		ObjectRecord* core;
		int ticks;
		int driftX, driftY, driftZ;

#ifdef XWA_MODERN
		if (XwaModernFlightTiming_IsHighRate()) {
			if (XwaModernFlightTiming_IsLegacyCadenceDue()) {
				g_modernDeathStarReactorRandomDriftX = (uint16_t)GameRand() % 1500 - 750;
				g_modernDeathStarReactorRandomDriftY = (uint16_t)GameRand() % 1500 - 750;
				g_modernDeathStarReactorRandomDriftZ = (uint16_t)GameRand() % 1500 - 750;
			}
			ticks = (uint16_t)g_elapsedTicks;
			driftX = DeathStar_ModernScaleWithRemainder(g_modernDeathStarReactorRandomDriftX, ticks, 236,
														&g_modernDeathStarReactorRandomDriftRemainderX);
			driftY = DeathStar_ModernScaleWithRemainder(g_modernDeathStarReactorRandomDriftY, ticks, 236,
														&g_modernDeathStarReactorRandomDriftRemainderY);
			driftZ = DeathStar_ModernScaleWithRemainder(g_modernDeathStarReactorRandomDriftZ, ticks, 236,
														&g_modernDeathStarReactorRandomDriftRemainderZ);
		} else {
#endif
			driftX = ((uint16_t)GameRand() % 1500 - 750) * (uint16_t)g_elapsedTicks / 236;
			{
				int randomY = (uint16_t)GameRand() % 1500 - 750;
				uint16_t currentTicks = (uint16_t)g_elapsedTicks;
				ticks = currentTicks;
				driftY = randomY * currentTicks / 236;
			}
			driftZ = ((uint16_t)GameRand() % 1500 - 750) * ticks / 236;
#ifdef XWA_MODERN
		}
#endif
		core = &g_objectTable[g_deathStarReactorCoreObjIdx];

		if ((unsigned int)g_deathStarReactorDestructionTimer > 0x834) {
			float driftDist;
			if (g_deathStarReactorCoreDriftDirZ + g_deathStarReactorCoreDriftDirY +
					g_deathStarReactorCoreDriftDirX ==
				0.0f) {
				pai_RotateLocalVectorToWorldScratchMaybeStatic(
					&g_objectTable[g_deathStarActiveSegmentObjIdx[activeSegmentSlotIdx]], 0, -32767, 0);
				g_deathStarReactorCoreDriftDirX = (float)g_rotatedX * 0.000030518509f;
				g_deathStarReactorCoreDriftDirY = (float)g_rotatedY * 0.000030518509f;
				g_deathStarReactorCoreDriftDirZ = (float)g_rotatedZ * 0.000030518509f;
			}
			ticks = (uint16_t)g_elapsedTicks;
#ifdef XWA_MODERN
			if (XwaModernFlightTiming_IsHighRate()) {
				g_deathStarReactorCoreDriftSpeed += (int16_t)DeathStar_ModernScaleWithRemainder(
					3000, (uint16_t)g_elapsedTicks, 236, &g_modernDeathStarReactorCoreDriftSpeedRemainder);
				driftDist = (float)(uint16_t)g_elapsedTicks * g_deathStarReactorCoreDriftSpeed / 236.0f;
				{
					double move;

					move = driftDist * g_deathStarReactorCoreDriftDirX +
						   g_modernDeathStarReactorCoreDirectedDriftRemainderX;
					driftX = (int)move;
					g_modernDeathStarReactorCoreDirectedDriftRemainderX = move - driftX;
					move = driftDist * g_deathStarReactorCoreDriftDirY +
						   g_modernDeathStarReactorCoreDirectedDriftRemainderY;
					driftY = (int)move;
					g_modernDeathStarReactorCoreDirectedDriftRemainderY = move - driftY;
					move = driftDist * g_deathStarReactorCoreDriftDirZ +
						   g_modernDeathStarReactorCoreDirectedDriftRemainderZ;
					driftZ = (int)move;
					g_modernDeathStarReactorCoreDirectedDriftRemainderZ = move - driftZ;
				}
			} else {
				g_deathStarReactorCoreDriftSpeed += (int16_t)(3000 * (uint16_t)g_elapsedTicks / 236);
				driftDist = (float)((uint16_t)g_elapsedTicks * g_deathStarReactorCoreDriftSpeed / 236);
				driftX = (int)(int64_t)(driftDist * g_deathStarReactorCoreDriftDirX);
				driftY = (int)(int64_t)(driftDist * g_deathStarReactorCoreDriftDirY);
				driftZ = (int)(int64_t)(driftDist * g_deathStarReactorCoreDriftDirZ);
			}
#else
			g_deathStarReactorCoreDriftSpeed += (int16_t)(3000 * (uint16_t)g_elapsedTicks / 236);
			driftDist = (float)((uint16_t)g_elapsedTicks * g_deathStarReactorCoreDriftSpeed / 236);
			driftX = (int)(int64_t)(driftDist * g_deathStarReactorCoreDriftDirX);
			driftY = (int)(int64_t)(driftDist * g_deathStarReactorCoreDriftDirY);
			driftZ = (int)(int64_t)(driftDist * g_deathStarReactorCoreDriftDirZ);
#endif
		}

		if ((unsigned int)g_deathStarReactorDestructionTimer > 0x76C) {
			MobileObject* mobj;

#ifdef XWA_MODERN
			if (XwaModernFlightTiming_IsHighRate()) {
				core->pitch += (Q16Angle)DeathStar_ModernScaleWithRemainder(
					-640, ticks, 236, &g_modernDeathStarReactorCorePitchRemainder);
			} else {
				core->pitch += -640 * ticks / 236;
			}
#else
			core->pitch += -640 * ticks / 236;
#endif
			mobj = core->mobj;
			if (mobj) {
				mobj->orientMatrixDirty = 1;
				core->mobj->moveVectorDirty = 1;
			}
		}

		core->world_x += driftX;
		core->world_y += driftY;
		core->world_z += driftZ;
	}
}

// FUNCTION: XWA 0x429D80
// Allocate a transient explosion/effect object (genus 13), initialize its mobile
// state, play a random explosion SFX + impact force-feedback, and apply radius damage
// scaled from instanceExtent. Returns the new object index, or 0xFFFF if none free.
unsigned int DeathStarTunnel_SpawnExplosionEffectObject(ObjectTypeId explosionObjectType,
														unsigned int instanceExtent) {
	unsigned int objIdx = Object_AllocSlotForGenus(0xDu);
	ObjectRecord* obj;

	if (objIdx == 0xFFFF)
		return 0xFFFF;

	DeathStar_InitMobileObjectForType(explosionObjectType, objIdx);
	obj = &g_objectTable[objIdx];

	obj->objectType = explosionObjectType;
	obj->genusId = 13;
	obj->mobj->state = 5;
	obj->typeSpecificByte[0] = 1;
	obj->mobj->framesAlive = 0;
	obj->mobj->lifetimeTimer = 0;
	obj->mobj->sourceObjIdx = -1;
	obj->mobj->speed = 0;
	obj->mobj->speedRemainder = 0;
	obj->mobj->moveVectorDirty = 1;
	obj->mobj->orientMatrixDirty = 1;
	obj->mobj->instanceExtent = instanceExtent;
	obj->mobj->rollImpulseRate = 0;
	obj->pitch = 0;
	obj->yaw = 0;
	obj->roll = 0;
	obj->angleD = 0;

	fsfx_PlaySound((uint16_t)GameRand() % 3 + 166, objIdx, g_localPlayer);
	ForceFeedback_PlayProximityEffectForObject(1, objIdx);
	collide_ApplyDefaultProximityDamage(objIdx, instanceExtent >> 6, 0xFFFF);
	return objIdx;
}

// FUNCTION: XWA 0x429E90
void DeathStar_UpdateLaserChamberFiring(int16_t currentSegmentIdx, int16_t triggerSegmentIdx) {
	int previousFireTimer;
	int slotIdx;

	(void)currentSegmentIdx;
	(void)triggerSegmentIdx;

	if (g_deathStarLaserCooldownTimer > 0) {
		g_deathStarLaserCooldownTimer -= (int)(uint16_t)g_elapsedTicks;
		DeathStar_UpdateLaserEffectSegments(0);
		return;
	}

	previousFireTimer = g_deathStarLaserFireTimer;
	g_deathStarLaserFireTimer += (int)(uint16_t)g_elapsedTicks;

	if (g_deathStarLaserFireTimer < 944) {
		if (previousFireTimer == 0) {
			int worldX;
			int worldY;
			int worldZ;

			for (slotIdx = 0; slotIdx < g_deathStarLaserEffectSlotCount; ++slotIdx) {
				ObjectRecord* object;

				object = &g_objectTable[g_deathStarLaserEffectSlots[slotIdx].objectIdx];
				if (object->objectType != OBJ_None &&
					object->objectSignature == g_deathStarLaserEffectSlots[slotIdx].objectSignature) {
					object->objectType = OBJ_None;
				}
			}

			g_deathStarLaserEffectSlotCount = 0;
			DeathStar_ProjectPlayerOntoLaserChamberAxis(&worldX, &worldY, &worldZ);
			fsfx_PlaySfxAtWorldPosition(58u, 0.5f, worldX, worldY, worldZ, g_localPlayer);
		}

		DeathStar_UpdateLaserEffectSegments(0);
		return;
	}

	if (g_deathStarLaserFireTimer < 1416) {
		DeathStar_UpdateLaserEffectSegments(1);

		for (slotIdx = 0; slotIdx < g_deathStarLaserEffectSlotCount; ++slotIdx) {
			FlightLight_AppendScenePointLightForObject(
				&g_objectTable[g_deathStarLaserEffectSlots[slotIdx].objectIdx]);
		}
		return;
	}

	if (g_deathStarLaserFireTimer < 2124) {
		DeathStar_UpdateLaserEffectSegments(0);
		return;
	}

	DeathStar_UpdateLaserEffectSegments(0);
	g_deathStarLaserFireTimer = 0;
	g_deathStarLaserCooldownTimer = 4248;
}

#ifndef XWA_MODERN
#pragma function(memcpy)
#define DEATH_STAR_SLOT_MOVE memcpy
#else
#define DEATH_STAR_SLOT_MOVE memmove
#endif
// FUNCTION: XWA 0x42A010
void DeathStar_UpdateLaserEffectSegments(int extendBeam) {
	int slotIdx;
	char spawnedSegments;

	spawnedSegments = 0;
	for (slotIdx = 0; slotIdx < g_deathStarLaserEffectSlotCount; ++slotIdx) {
		ObjectRecord* object;

		object = &g_objectTable[g_deathStarLaserEffectSlots[slotIdx].objectIdx];
		if (object->objectType == OBJ_None ||
			object->objectSignature != g_deathStarLaserEffectSlots[slotIdx].objectSignature) {
			DEATH_STAR_SLOT_MOVE(
				&g_deathStarLaserEffectSlots[slotIdx], &g_deathStarLaserEffectSlots[slotIdx + 1],
				(size_t)(g_deathStarLaserEffectSlotCount - slotIdx - 1) *
					sizeof(g_deathStarLaserEffectSlots[0]));
			--g_deathStarLaserEffectSlotCount;
			--slotIdx;
		}
	}

	if ((uint8_t)extendBeam && g_deathStarLaserEffectSlotCount != 0) {
		unsigned int segmentLength;
		unsigned int beamDistance;

		segmentLength = collide_roughdistance3d(g_deathStarLaserChamberDirX, g_deathStarLaserChamberDirY,
												g_deathStarLaserChamberDirZ);
		beamDistance = collide_roughdistance3d(
			g_objectTable[g_deathStarLaserEffectSlots[g_deathStarLaserEffectSlotCount - 1].objectIdx]
					.world_x -
				g_deathStarLaserChamberX,
			g_objectTable[g_deathStarLaserEffectSlots[g_deathStarLaserEffectSlotCount - 1].objectIdx]
					.world_y -
				g_deathStarLaserChamberY,
			g_objectTable[g_deathStarLaserEffectSlots[g_deathStarLaserEffectSlotCount - 1].objectIdx]
					.world_z -
				g_deathStarLaserChamberZ);
		if (beamDistance >= segmentLength) {
			DeathStar_SpawnLaserGlowSegment();
			DeathStar_SpawnLaserInternalSegment();
			spawnedSegments = 1;
		}
	} else if ((uint8_t)extendBeam) {
		DeathStar_SpawnLaserGlowSegment();
		DeathStar_SpawnLaserInternalSegment();
		spawnedSegments = 1;
	}

	for (slotIdx = 0; slotIdx < g_deathStarLaserEffectSlotCount; ++slotIdx) {
		if (g_objectTable[g_deathStarLaserEffectSlots[slotIdx].objectIdx].objectType ==
			OBJ_LightingEffectTextureGroup1000) {
			g_objectTable[g_deathStarLaserEffectSlots[slotIdx].objectIdx].typeSpecificByte[0] = 1;
		}
	}

	if (g_players[g_localPlayer].objectIndex != 0xffff) {
		int playerObjIdx;
		int playerWorldX;
		int playerWorldY;
		int playerWorldZ;
		int projectedX;
		int projectedY;

		playerObjIdx = g_players[g_localPlayer].objectIndex;
		playerWorldX = g_objectTable[playerObjIdx].world_x;
		playerWorldY = g_objectTable[playerObjIdx].world_y;
		playerWorldZ = g_objectTable[playerObjIdx].world_z;

		DeathStar_ProjectPlayerOntoLaserChamberAxis(&projectedX, &projectedY, &extendBeam);
		if (g_deathStarLaserFireTimer > 944 && g_deathStarLaserFireTimer < 1416) {
			int distanceFromBeam;

			distanceFromBeam = collide_roughdistance3d(playerWorldX - projectedX, playerWorldY - projectedY,
													   playerWorldZ - extendBeam);
			if (distanceFromBeam < 2048) {
				collide_damagecraft(playerObjIdx, 0xffffu, 0xfffffffdu,
									(unsigned int)((2048 - distanceFromBeam) / 16), 0);
			}
		}

		if (spawnedSegments) {
			fsfx_PlaySfxAtWorldPosition(171u, 1.0f, projectedX, projectedY, extendBeam, g_localPlayer);
		}
	}
}
#ifndef XWA_MODERN
#pragma function(memcpy)
#endif

// FUNCTION: XWA 0x42A270
uint16_t DeathStar_SpawnLaserGlowSegment(void) {
	DeathStarSegmentDef* chamberSegment;
	unsigned int objectIdx;
	ObjectRecord* object;
	MobileObject* mobj;
	int slotIdx;
	DeathStarLaserEffectSlot* previousSlot;
	uint16_t objectSignature;
	int fireTimer;

	chamberSegment = &g_deathStarSegmentSets[g_deathStarLaserChamberSegmentSetIdx]
						  .segments[(int16_t)g_deathStarLaserChamberSegmentIdx];
	slotIdx = 0;

	if (g_deathStarLaserEffectSlotCount == 10) {
		g_objectTable[g_deathStarLaserEffectSlots[0].objectIdx].objectType = OBJ_None;
		DEATH_STAR_SLOT_MOVE(g_deathStarLaserEffectSlots, &g_deathStarLaserEffectSlots[1],
							 9 * sizeof(g_deathStarLaserEffectSlots[0]));
		--g_deathStarLaserEffectSlotCount;
	}

	objectIdx = (uint16_t)Object_AllocSlotForGenus(GENUS_NpcProjectile);
	if (objectIdx == 0xffffu) {
		fireTimer = g_deathStarLaserFireTimer;
		objectIdx = g_elapsedTicks;
		g_deathStarLaserFireTimer = fireTimer - (int)objectIdx;
		return (uint16_t)objectIdx;
	}

	{
		uint16_t nextObjectSignature;

		nextObjectSignature = g_nextObjectSignature;
		object = &g_objectTable[objectIdx];
		object->objectSignature = nextObjectSignature;
		++g_nextObjectSignature;
	}
	if (g_nextObjectSignature == 0) {
		g_nextObjectSignature = 2;
	}

	object->objectType = OBJ_LightingEffectTextureGroup1000;
	object->genusId = GENUS_Explosion;
	object->typeSpecificByte[0] = 1;

	if (g_deathStarLaserEffectSlotCount > slotIdx) {
		previousSlot = &g_deathStarLaserEffectSlots[g_deathStarLaserEffectSlotCount - 1];
		if (g_objectTable[previousSlot->objectIdx].objectType == OBJ_DSLaserInternal) {
			object->world_x = g_objectTable[previousSlot->objectIdx].world_x + g_deathStarLaserChamberDirX;
			object->world_y = g_objectTable[previousSlot->objectIdx].world_y + g_deathStarLaserChamberDirY;
			object->world_z = g_objectTable[previousSlot->objectIdx].world_z + g_deathStarLaserChamberDirZ;
		} else {
			object->world_x = g_objectTable[previousSlot->objectIdx].world_x;
			object->world_y = g_objectTable[previousSlot->objectIdx].world_y;
			object->world_z = g_objectTable[previousSlot->objectIdx].world_z;
		}
	} else {
		object->world_x = g_deathStarLaserChamberX;
		object->world_y = g_deathStarLaserChamberY;
		object->world_z = g_deathStarLaserChamberZ;
	}

	object->yaw = chamberSegment->yaw;
	object->pitch = chamberSegment->pitch;

	object->mobj->prevWorldX = object->world_x;
	mobj = object->mobj;
	mobj->prevWorldY = object->world_y;
	mobj->prevWorldZ = object->world_z;
	mobj->instanceExtent = (uint16_t)g_deathStarLaserGlowExtent;
	mobj->lifetimeTimer = 255;
	mobj->framesAlive = (uint16_t)slotIdx;
	mobj->speedRemainder = (uint16_t)slotIdx;
	mobj->speed = 6000;
	mobj->moveVectorDirty = 1;
	mobj->orientMatrixDirty = 1;

	slotIdx = g_deathStarLaserEffectSlotCount;
	g_deathStarLaserEffectSlots[slotIdx].objectIdx = objectIdx;
	objectSignature = g_objectTable[objectIdx].objectSignature;
	g_deathStarLaserEffectSlots[slotIdx].objectSignature = objectSignature;
	g_deathStarLaserEffectSlotCount = slotIdx + 1;

	return objectSignature;
}

#ifndef XWA_MODERN
#pragma function(memcpy)
#endif
// FUNCTION: XWA 0x42A4B0
uint16_t DeathStar_SpawnLaserInternalSegment(void) {
	DeathStarSegmentDef* chamberSegment;
	unsigned int objectIdx;
	ObjectRecord* object;
	MobileObject* mobj;
	int slotIdx;
	ObjectTypeId laserObjectType;
	DeathStarLaserEffectSlot* previousSlot;
	uint16_t objectSignature;
	int fireTimer;

	chamberSegment = &g_deathStarSegmentSets[g_deathStarLaserChamberSegmentSetIdx]
						  .segments[(int16_t)g_deathStarLaserChamberSegmentIdx];
	slotIdx = 0;

	if (g_deathStarLaserEffectSlotCount == 10) {
		g_objectTable[g_deathStarLaserEffectSlots[0].objectIdx].objectType = OBJ_None;
		DEATH_STAR_SLOT_MOVE(g_deathStarLaserEffectSlots, &g_deathStarLaserEffectSlots[1],
							 9 * sizeof(g_deathStarLaserEffectSlots[0]));
		--g_deathStarLaserEffectSlotCount;
	}

	objectIdx = Object_AllocSlotForGenus(GENUS_NpcProjectile);
	if (objectIdx == 0xffffu) {
		fireTimer = g_deathStarLaserFireTimer;
		objectIdx = g_elapsedTicks;
		g_deathStarLaserFireTimer = fireTimer - (int)objectIdx;
		return (uint16_t)objectIdx;
	}

	object = &g_objectTable[objectIdx];
	object->objectSignature = g_nextObjectSignature++;
	if (g_nextObjectSignature == 0) {
		g_nextObjectSignature = 2;
	}

	laserObjectType = OBJ_DSLaserInternal;
	object->genusId = GENUS_NpcProjectile;
	object->objectType = laserObjectType;

	if (g_deathStarLaserEffectSlotCount > slotIdx) {
		previousSlot = &g_deathStarLaserEffectSlots[g_deathStarLaserEffectSlotCount - 1];
		if (g_objectTable[previousSlot->objectIdx].objectType == laserObjectType) {
			object->world_x = g_objectTable[previousSlot->objectIdx].world_x + g_deathStarLaserChamberDirX;
			object->world_y = g_objectTable[previousSlot->objectIdx].world_y + g_deathStarLaserChamberDirY;
			object->world_z = g_objectTable[previousSlot->objectIdx].world_z + g_deathStarLaserChamberDirZ;
		} else {
			object->world_x = g_objectTable[previousSlot->objectIdx].world_x;
			object->world_y = g_objectTable[previousSlot->objectIdx].world_y;
			object->world_z = g_objectTable[previousSlot->objectIdx].world_z;
		}
	} else {
		object->world_x = g_deathStarLaserChamberX;
		object->world_y = g_deathStarLaserChamberY;
		object->world_z = g_deathStarLaserChamberZ;
	}

	object->yaw = chamberSegment->yaw;
	object->pitch = chamberSegment->pitch;
	object->typeSpecificWord = (uint16_t)slotIdx;

	mobj = object->mobj;
	mobj->prevWorldX = object->world_x;
	mobj->prevWorldY = object->world_y;
	mobj->prevWorldZ = object->world_z;
	mobj->state = 1;
	mobj->damageAmount = 1000000;
	mobj->framesAlive = (uint16_t)slotIdx;
	mobj->lifetimeTimer = 255;
	mobj->speed = 6000;
	mobj->speedRemainder = (uint16_t)slotIdx;
	mobj->moveVectorDirty = 1;
	mobj->orientMatrixDirty = 1;

	slotIdx = g_deathStarLaserEffectSlotCount;
	g_deathStarLaserEffectSlots[slotIdx].objectIdx = objectIdx;
	objectSignature = object->objectSignature;
	g_deathStarLaserEffectSlotCount = slotIdx + 1;
	g_deathStarLaserEffectSlots[slotIdx].objectSignature = objectSignature;

	return objectSignature;
}
#undef DEATH_STAR_SLOT_MOVE

// FUNCTION: XWA 0x42A6F0
void DeathStar_ProjectPlayerOntoLaserChamberAxis(int* outX, int* outY, int* outZ) {
	int playerObjIdx;
	int laserLengthScale;
	int laserLengthScaleSq;
	int dirX;
	int dirY;
	int dirZ;
	int playerDeltaX;
	int playerDeltaY;
	int playerDeltaZ;
	int dot;

	playerObjIdx = g_players[g_localPlayer].objectIndex;
	if (playerObjIdx == 0xffff) {
		return;
	}

	laserLengthScale = ModelBounds_GetSizeY(OBJ_DSLaserInternal) >> 7;
	laserLengthScaleSq = laserLengthScale * laserLengthScale;

	dirX = g_deathStarLaserChamberDirX >> 6;
	dirY = g_deathStarLaserChamberDirY >> 6;
	dirZ = g_deathStarLaserChamberDirZ >> 6;

	playerDeltaX = (g_objectTable[playerObjIdx].world_x - g_deathStarLaserChamberX) >> 6;
	playerDeltaY = (g_objectTable[playerObjIdx].world_y - g_deathStarLaserChamberY) >> 6;
	playerDeltaZ = (g_objectTable[playerObjIdx].world_z - g_deathStarLaserChamberZ) >> 6;
	dot = dirX * playerDeltaX + dirZ * playerDeltaZ + dirY * playerDeltaY;

	*outX = g_deathStarLaserChamberX + ((dirX * dot / laserLengthScaleSq) << 6);
	*outY = g_deathStarLaserChamberY + ((dirY * dot / laserLengthScaleSq) << 6);
	*outZ = g_deathStarLaserChamberZ + ((dirZ * dot / laserLengthScaleSq) << 6);
}

// FUNCTION: XWA 0x42A7E0
void DeathStar_SpawnLaserPowerSourceObject(void) {
	int slot;

	slot = Object_FindFreeMissionSlot();
	g_deathStarLaserPowerSourceObjIdx = slot;
	if (slot != 0xffffu) {
		ObjectRecord* object;
		DeathStarSegmentDef* segment;
		int byteIdx;

		object = &g_objectTable[slot];
		object->flightGroupIdx = g_deathStarTankPipeBlueFgIdx;
		object->regionIdx = 0;
		object->roll = 0;
		object->angleD = 0;
		object->typeSpecificWord = 4;
		for (byteIdx = 0; byteIdx < 2; ++byteIdx) {
			object->typeSpecificByte[byteIdx] = 0;
		}
		object->playerOwnerIdx = -1;
		object->mobj = 0;
		object->objectType = OBJ_DSLaserPowerSource;
		object->genusId = (ModelGenusId)g_modelTypeTable[OBJ_DSLaserPowerSource].genusId;

#ifdef XWA_MODERN
		if (g_deathStarSegmentSets[2].segments == NULL || g_deathStarSegmentSets[2].count == 0) {
			segment = NULL;
		} else {
			segment = &g_deathStarSegmentSets[2].segments[g_deathStarSegmentSets[2].count - 1];
		}
#else
		segment = &g_deathStarSegmentSets[2].segments[g_deathStarSegmentSets[2].count - 1];
#endif
		if (segment != NULL) {
			int offsetX;
			int offsetY;
			int offsetZ;

			DeathStar_ComputeSegmentPointOffset(segment, 4u, DEATH_STAR_SEGMENT_POINT_KIND_ATTACH, &offsetX,
												&offsetY, &offsetZ);
			object->world_x = segment->worldX + offsetX;
			object->world_y = segment->worldY + offsetY;
			object->world_z = segment->worldZ + offsetZ;
			object->yaw = segment->yaw;
			object->pitch = segment->pitch;
		} else {
			object->objectType = OBJ_None;
			g_deathStarLaserPowerSourceObjIdx = 0;
		}
	}
}

// FUNCTION: XWA 0x42A8D0
void DeathStar_UpdateTunnelExitSegment(int16_t currentSegmentIdx, int16_t triggerSegmentIdx) {
	if (currentSegmentIdx == triggerSegmentIdx) {
		ObjectRecord* playerObj;
		ObjectRecord* segmentObj;

		playerObj = &g_objectTable[g_deathStarPlayerObjIdx];
		segmentObj = &g_objectTable[g_deathStarActiveSegmentObjIdx[triggerSegmentIdx]];
		if (collide_roughdistance3d(playerObj->world_x - segmentObj->world_x,
									playerObj->world_y - segmentObj->world_y,
									playerObj->world_z - segmentObj->world_z) < 0x6a72) {
			g_flightMissionEndPending = 1;
		}
	}
}

// FUNCTION: XWA 0x42A940
void DeathStar_SetOrderWaypointFromSegmentPoint(int objectIdx, uint16_t segmentPointIdx, int pointKind,
												uint8_t flightGroupIdx, int orderSlot, int waypointSlot) {
	ObjectRecord* object;
	DeathStarSegmentDef segmentDef;
	int offsetX;
	int offsetY;
	int offsetZ;
	XwaWaypoint* waypoint;

	object = &g_objectTable[objectIdx];
	segmentDef.objectType = object->objectType;
	segmentDef.yaw = object->yaw;
	segmentDef.pitch = object->pitch;
	segmentDef.worldX = object->world_x;
	segmentDef.worldY = object->world_y;
	segmentDef.worldZ = object->world_z;

	DeathStar_ComputeSegmentPointOffset(&segmentDef, segmentPointIdx, pointKind, &offsetX, &offsetY,
										&offsetZ);

	waypoint = &g_missionFlightGroups[flightGroupIdx].fg.orders[orderSlot].waypoints[waypointSlot];
	waypoint->x = (int16_t)((segmentDef.worldX + offsetX) >> 8);
	waypoint->y = (int16_t)(-(segmentDef.worldY + offsetY) >> 8);
	waypoint->z = (int16_t)((segmentDef.worldZ + offsetZ) >> 8);
}

// FUNCTION: XWA 0x42AA30
int DeathStar_SpawnReactorDebrisGirders(void) {
	DeathStarSegmentDef* segment;
	int distance;
	int savedDistance;
	int offsetX;
	int offsetY;
	int offsetZ;
	int result;

	segment = &g_deathStarSegmentSets[3].segments[g_deathStarSegmentSets[3].count - 2];
	DeathStar_ComputeSegmentPointOffset(segment, 4u, DEATH_STAR_SEGMENT_POINT_KIND_ATTACH, &offsetX, &offsetY,
										&offsetZ);

	distance = 0;
	savedDistance = distance;
	do {
		result = Object_FindFreeMissionSlot();
		if (result != 0xffff) {
			int objectIdx;
			int clearIdx;
			int randomYaw;
			int randomPitch;
			int worldZ;
			int* worldX;
			int* worldY;
			Q16Angle* yaw;
			Q16Angle* pitch;

			objectIdx = result;
			g_objectTable[objectIdx].objectType = OBJ_DSGirder;
			g_objectTable[objectIdx].genusId =
				(ModelGenusId)g_modelTypeTable[(uint16_t)g_objectTable[result].objectType].genusId;
			g_objectTable[objectIdx].world_x = segment->worldX + offsetX;
			g_objectTable[objectIdx].world_y = segment->worldY + offsetY;
			g_objectTable[objectIdx].world_z = segment->worldZ + offsetZ;
			g_objectTable[objectIdx].yaw = (Q16Angle)(segment->yaw - 2048);
			g_objectTable[objectIdx].pitch = segment->pitch;
			g_objectTable[objectIdx].flightGroupIdx = g_deathStarGeneratedObjectFgIdx;
			g_objectTable[objectIdx].regionIdx = 0;
			g_objectTable[objectIdx].roll = 0;
			g_objectTable[objectIdx].angleD = 0;
			for (clearIdx = 0; clearIdx < 2; ++clearIdx) {
				g_objectTable[objectIdx].typeSpecificByte[clearIdx] = 0;
			}
			g_objectTable[objectIdx].playerOwnerIdx = -1;
			g_objectTable[objectIdx].mobj = 0;

			worldZ = g_objectTable[objectIdx].world_z;
			worldZ += distance - 900;
			g_objectTable[objectIdx].world_z = worldZ;
			worldX = &g_objectTable[objectIdx].world_x;
			*worldX += 300 - ((uint16_t)GameRand() % 150);
			worldY = &g_objectTable[objectIdx].world_y;
			*worldY += 300 - ((uint16_t)GameRand() % 150);
			yaw = &g_objectTable[objectIdx].yaw;
			randomYaw = abs((int)(uint16_t)GameRand());
			randomYaw &= 0x3fff;
			randomYaw = abs(randomYaw);
			*yaw = (Q16Angle)(*yaw + 0x8000 - randomYaw);
			pitch = &g_objectTable[objectIdx].pitch;
			randomPitch = abs((int)(uint16_t)GameRand());
			randomPitch &= 0x3fff;
			randomPitch = abs(randomPitch);
			*pitch = (Q16Angle)(*pitch + 0x8000 - randomPitch);
			distance = savedDistance;
			g_objectTable[objectIdx].typeSpecificWord = 100;
			result = randomPitch;
		}
		distance += 300;
		savedDistance = distance;
	} while (distance < 2100);

	return result;
}

// FUNCTION: XWA 0x42AC60
void DeathStar_ApplyFollowSeparationOffset(int objectIdx, int followSlotIdx) {
	int previousObjIdx;
	MobileObject* mobj;
	int distance;
	int separation;

	if (followSlotIdx == 0) {
		previousObjIdx = g_deathStarFollowLeaderObjIdx;
	} else {
		previousObjIdx = g_deathStarFollowChainSlots[followSlotIdx - 1].objectIdx;
	}

	if (previousObjIdx == 0xffff) {
		return;
	}

	mobj = g_objectTable[objectIdx].mobj;
	distance =
		collide_roughdistance3d(g_objectTable[objectIdx].world_x - g_objectTable[previousObjIdx].world_x,
								g_objectTable[objectIdx].world_y - g_objectTable[previousObjIdx].world_y,
								g_objectTable[objectIdx].world_z - g_objectTable[previousObjIdx].world_z);
	if (distance >= 1000) {
		return;
	}

	separation = 1000 - distance;
	switch (followSlotIdx & 3) {
		case 0:
			g_objectTable[objectIdx].world_x += separation * mobj->cachedUpX / 0x7fff / 3;
			g_objectTable[objectIdx].world_y += separation * mobj->cachedUpY / 0x7fff / 3;
			g_objectTable[objectIdx].world_z += separation * mobj->cachedUpZ / 0x7fff / 3;
			return;

		case 1:
			g_objectTable[objectIdx].world_x += separation * mobj->cachedSideX / 0x7fff / 2;
			g_objectTable[objectIdx].world_y += separation * mobj->cachedSideY / 0x7fff / 2;
			g_objectTable[objectIdx].world_z += separation * mobj->cachedSideZ / 0x7fff / 2;
			break;

		case 2:
			g_objectTable[objectIdx].world_x += -separation * mobj->cachedUpX / 0x7fff / 3;
			g_objectTable[objectIdx].world_y += -separation * mobj->cachedUpY / 0x7fff / 3;
			g_objectTable[objectIdx].world_z += -separation * mobj->cachedUpZ / 0x7fff / 3;
			return;

		case 3:
			g_objectTable[objectIdx].world_x += -separation * mobj->cachedSideX / 0x7fff / 2;
			g_objectTable[objectIdx].world_y += -separation * mobj->cachedSideY / 0x7fff / 2;
			g_objectTable[objectIdx].world_z += -separation * mobj->cachedSideZ / 0x7fff / 2;
			break;
	}
}

// FUNCTION: XWA 0x493AD0
void DeathStar_SelectLaserTarget(int preferredTargetObjIdx) {
	uint32_t scanObjIdx;
	int bestExtent;
	ObjectRecord* objects;

	bestExtent = -1;
	if (g_deathStarTunnelLaserRegions[regionIdx].shotActive) {
		return;
	}

	if (preferredTargetObjIdx == 0xffff) {
		const uint32_t targetScanEnd = g_activeRegionCraftObjectSlotEnd;
		objects = g_objectTable;
		if (g_deathStarTunnelLaserRegions[regionIdx].targetFlightGroupCount != 0 &&
			g_deathStarTunnelLaserRegions[regionIdx].nextTargetFlightGroupIndex <
				g_deathStarTunnelLaserRegions[regionIdx].targetFlightGroupCount) {
			scanObjIdx = 1;
			do {
				uint32_t targetScanObjIdx;
				const int targetFgIdx = g_deathStarTunnelLaserRegions[regionIdx]
											.targetFlightGroupIds[g_deathStarTunnelLaserRegions[regionIdx]
																	  .nextTargetFlightGroupIndex];

				for (targetScanObjIdx = g_activeRegionObjectSlotStart; targetScanObjIdx < targetScanEnd;
					 ++targetScanObjIdx) {
					if (objects[targetScanObjIdx].flightGroupIdx == targetFgIdx) {
						break;
					}
				}

				if (targetScanObjIdx != targetScanEnd) {
					preferredTargetObjIdx = (int)targetScanObjIdx;
					scanObjIdx = 0;
				}
				++g_deathStarTunnelLaserRegions[regionIdx].nextTargetFlightGroupIndex;
			} while (scanObjIdx != 0 && g_deathStarTunnelLaserRegions[regionIdx].nextTargetFlightGroupIndex <
											g_deathStarTunnelLaserRegions[regionIdx].targetFlightGroupCount);
		}

		if (preferredTargetObjIdx == 0xffff) {
			for (scanObjIdx = g_activeRegionObjectSlotStart; scanObjIdx < targetScanEnd; ++scanObjIdx) {
				int maxBoundsExtent;
				int worldX;
				int* worldY;
				int* worldZ;
				int emitterOffsetX;
				int emitterOffsetY;
				int emitterOffsetZ;
				unsigned int genusId;
				float invDistance;
				float extent;
				float deltaY;
				float deltaZ;
				float deltaX;

				if (objects[scanObjIdx].objectType == OBJ_None) {
					continue;
				}
				if (g_missionFlightGroups[objects[scanObjIdx].flightGroupIdx].fg.iff == 1) {
					continue;
				}

				genusId = objects[scanObjIdx].genusId;
				if (genusId > GENUS_WeaponEmplacement) {
					continue;
				}

				switch (g_deathStarLaserTargetGenusDispatch[genusId]) {
					case 0:
						break;
					case 1:
						break;
					case 2:
						break;
					case 3:
						break;
					default:
						continue;
				}

				maxBoundsExtent =
					(int)g_modelTypeTable[(uint16_t)objects[scanObjIdx].objectType].maxBoundsExtent;
				if (maxBoundsExtent <= bestExtent) {
					continue;
				}

				worldX = objects[scanObjIdx].world_x;
				worldY = &objects[scanObjIdx].world_y;
				worldZ = &objects[scanObjIdx].world_z;
				emitterOffsetX = g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetX;
				emitterOffsetY = g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetY;
				emitterOffsetZ = g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetZ;
				deltaX = (float)((double)worldX - (double)emitterOffsetX);
				deltaY = (float)((double)*worldY - (double)emitterOffsetY);
				deltaZ = (float)((double)*worldZ - (double)emitterOffsetZ);
				extent = (float)maxBoundsExtent;
				invDistance =
					(float)(1.0 / sqrt((double)deltaX * (double)deltaX + (double)deltaY * (double)deltaY +
									   (double)deltaZ * (double)deltaZ));

				g_collisionProbeWorldX = worldX - (int)(invDistance * deltaX * extent);
				g_collisionProbeWorldY = *worldY - (int)(invDistance * deltaY * extent);
				g_collisionProbeWorldZ = *worldZ - (int)(invDistance * deltaZ * extent);
				g_collisionSegmentStartWorldX = emitterOffsetX;
				g_collisionSegmentStartWorldY = emitterOffsetY;
				g_collisionSegmentStartWorldZ = emitterOffsetZ;

				if (collide_CheckSweptModelCollision(scanObjIdx, scanObjIdx) == 0) {
					preferredTargetObjIdx = (int)scanObjIdx;
					bestExtent =
						(int)g_modelTypeTable[(uint16_t)g_objectTable[scanObjIdx].objectType].maxBoundsExtent;
					objects = g_objectTable;
				} else {
					objects = g_objectTable;
				}
			}
		}
	}

	if (preferredTargetObjIdx != 0xffff) {
		const int shotStartGameTime = g_gameTime;
		g_deathStarTunnelLaserRegions[regionIdx].targetObjIdx = preferredTargetObjIdx;
		g_deathStarTunnelLaserRegions[regionIdx].shotStartGameTime = shotStartGameTime;
		g_deathStarTunnelLaserRegions[regionIdx].shotActive = 1;
	}
}

// FUNCTION: XWA 0x493E00
void DeathStar_FireLaserAtTarget(void) {
	enum { DEATH_STAR_LASER_PROJECTILE_IDX = OBJ_LaserImperialDS - OBJ_LaserRebel };

	PlayerData* player;
	int targetObjIdx;
	int savedTargetX;
	int savedTargetY;
	int savedTargetZ;
	float deltaX;
	float deltaY;
	float invDistance;
	float deltaZ;
	int moveX;
	int moveY;
	int moveZ;
	unsigned int laserObjIdx;
	WarheadGuidanceState* guidance;
	char debugMessage[32];

	targetObjIdx = g_deathStarTunnelLaserRegions[regionIdx].targetObjIdx;
	sprintf(debugMessage, "ds %d  %d \n", targetObjIdx, g_objectTable[targetObjIdx].flightGroupIdx);
	OutputDebugStringA(debugMessage);
	if (targetObjIdx == 0xffff) {
		return;
	}

	player = &g_players[g_localPlayer];
	savedTargetX = player->viewState.savedTargetX;
	savedTargetY = player->viewState.savedTargetY;
	savedTargetZ = player->viewState.savedTargetZ;

	g_deathStarTunnelLaserRegions[regionIdx].beamStartX =
		savedTargetX + g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetX;
	g_deathStarTunnelLaserRegions[regionIdx].beamStartY =
		savedTargetY + g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetY;
	g_deathStarTunnelLaserRegions[regionIdx].beamStartZ =
		savedTargetZ + g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetZ;

	deltaX = (float)((double)g_objectTable[targetObjIdx].world_x -
					 (double)(savedTargetX + g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetX));
	deltaY = (float)((double)g_objectTable[targetObjIdx].world_y -
					 (double)(savedTargetY + g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetY));
	deltaZ = (float)((double)g_objectTable[targetObjIdx].world_z -
					 (double)(savedTargetZ + g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetZ));
	g_deathStarTunnelLaserRegions[regionIdx].remainingDistance = (float)sqrt(
		(double)deltaX * (double)deltaX + (double)deltaY * (double)deltaY + (double)deltaZ * (double)deltaZ);
	invDistance = (float)(1.0 / (double)g_deathStarTunnelLaserRegions[regionIdx].remainingDistance);
	deltaX *= invDistance;
	deltaY *= invDistance;
	deltaZ *= invDistance;

	g_deathStarTunnelLaserRegions[regionIdx].pointLightX =
		(int)((g_deathStarTunnelLaserRegions[regionIdx].remainingDistance -
			   (float)g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].maxBoundsExtent) *
				  deltaX +
			  (float)g_deathStarTunnelLaserRegions[regionIdx].beamStartX);
	g_deathStarTunnelLaserRegions[regionIdx].pointLightY =
		(int)((g_deathStarTunnelLaserRegions[regionIdx].remainingDistance -
			   (float)g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].maxBoundsExtent) *
				  deltaY +
			  (float)g_deathStarTunnelLaserRegions[regionIdx].beamStartY);
	g_deathStarTunnelLaserRegions[regionIdx].pointLightZ =
		(int)((g_deathStarTunnelLaserRegions[regionIdx].remainingDistance -
			   (float)g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].maxBoundsExtent) *
				  deltaZ +
			  (float)g_deathStarTunnelLaserRegions[regionIdx].beamStartZ);

	laserObjIdx = Object_AllocSlotForGenus(GENUS_NpcProjectile);
	if (laserObjIdx == 0xffffu) {
		laserObjIdx = g_playerProjectileSlotsTotal + g_sharedPlayerProjectileSlotsPerRegion +
					  g_projectileObjectSlotStart;
		while (laserObjIdx < g_projectileObjectSlotEnd) {
#ifdef XWA_MODERN
			if (laser_GetProjectileWarheadClass(g_objectTable[laserObjIdx].objectType) == 0 &&
#else
			if (g_projectileWarheadClassByType[(uint16_t)g_objectTable[laserObjIdx].objectType -
											 OBJ_LaserRebel] == 0 &&
#endif
				g_objectTable[laserObjIdx].mobj->team == 1) {
				break;
			}
			++laserObjIdx;
		}
	}
	if (laserObjIdx == g_projectileObjectSlotEnd) {
		return;
	}

	g_deathStarTunnelLaserRegions[regionIdx].beamLightActive = 1;
	g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx = (int)laserObjIdx;
	g_objectTable[laserObjIdx].mobj->state = 1;
	g_objectTable[laserObjIdx].genusId = GENUS_NpcProjectile;
	g_objectTable[laserObjIdx].objectType = OBJ_LaserImperialDS;
	g_objectTable[laserObjIdx].regionIdx = (uint8_t)regionIdx;
	g_objectTable[laserObjIdx].mobj->framesAlive = 1;
	g_objectTable[laserObjIdx].mobj->sourceObjIdx = -1;
	g_objectTable[laserObjIdx].mobj->sourceObjectType = OBJ_DeathStarFireTextureGroup6250_Sprite000;
	g_objectTable[laserObjIdx].mobj->iff = 1;
	g_objectTable[laserObjIdx].world_x = g_players[g_localPlayer].viewState.savedTargetX +
										 g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetX;
	g_objectTable[laserObjIdx].world_y = g_players[g_localPlayer].viewState.savedTargetY +
										 g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetY;
	g_objectTable[laserObjIdx].world_z = g_players[g_localPlayer].viewState.savedTargetZ +
										 g_deathStarTunnelLaserRegions[regionIdx].emitterOffsetZ;
	moveX = (int)(deltaX * 32767.0f);
	moveY = (int)(deltaY * 32767.0f);
	moveZ = (int)(deltaZ * 32767.0f);
	g_objectTable[laserObjIdx].mobj->moveX = (int16_t)moveX;
	g_objectTable[laserObjIdx].mobj->moveY = (int16_t)moveY;
	g_objectTable[laserObjIdx].mobj->moveZ = (int16_t)moveZ;

	trig2_ctop((int16_t)moveX, (int16_t)moveY, (int16_t)moveZ);
	g_objectTable[laserObjIdx].yaw = trig2_xyangle;
	g_objectTable[laserObjIdx].pitch = targetPitch;
	g_objectTable[laserObjIdx].roll = 0;
	g_objectTable[laserObjIdx].angleD = 0;
	g_objectTable[laserObjIdx].mobj->moveVectorDirty = 0;
	g_objectTable[laserObjIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[laserObjIdx].mobj->speed = g_projectileSpeedByType[DEATH_STAR_LASER_PROJECTILE_IDX];
	g_objectTable[laserObjIdx].mobj->damageAmount = g_projectileDamageByType[DEATH_STAR_LASER_PROJECTILE_IDX];
	moveX = 236 * *(const int*)&g_projectileLifetimeSecondsByType[DEATH_STAR_LASER_PROJECTILE_IDX];
	moveY = g_projectileLifetimeFracQ16ByType[DEATH_STAR_LASER_PROJECTILE_IDX];
	moveX += MATH2_fraction((uint16_t)moveY, 0x00ecu);
	g_objectTable[laserObjIdx].mobj->lifetimeTimer = (uint16_t)moveX;
	guidance = g_objectTable[laserObjIdx].mobj->pWarheadGuidance;
	guidance->homingTier = 0;
	guidance->targetObjIdx = 0xffffu;
	guidance->targetSignature = 0;
	guidance->targetComponentIdx = 0xffffu;
	guidance->minSpeed = g_projectileSpeedByType[DEATH_STAR_LASER_PROJECTILE_IDX];
	guidance->sourcePlayerIdx = -1;
}
