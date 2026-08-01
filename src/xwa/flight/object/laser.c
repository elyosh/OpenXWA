#include "xwa/flight/object/laser.h"

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
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight.h"
#include "xwa/audio/fsfx.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/input/dinput.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/math/fixed.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/random.h"
#ifdef XWA_MODERN
#include "xwa_runtime/timing/modern_flight_timing.h"
#endif

#include <math.h>
#include <stddef.h>
#include <string.h>

// GLOBAL: XWA 0x5B6330
const uint16_t g_projectileLifetimeSecondsByObjectType[OBJ_LaserImperialDS + 1] = {
	0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0,  0,  0,   0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0,  0,  0,   0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0,  0,  0,   0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0,  0,  0,   0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0,  0,  0,   0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0,  0,  0,   0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0,  0,  0,   0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0,  0,  0,   0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0,  0,  0,   0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 1, 1, 1, 1, 2, 40, 25, 3, 3, 5, 45, 25, 120, 90, 25, 40, 3, 5, 0, 0, 3, 4, 3, 3, 4, 10
};

// GLOBAL: XWA 0x5B6368
const uint16_t g_projectileLifetimeFracQ16ByObjectType[OBJ_LaserImperialDS + 1] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0,      0, 0,      0,      0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0,      0, 0,      0,      0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0,      0, 0,      0,      0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0,      0, 0,      0,      0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0,      0, 0,      0,      0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0,      0, 0,      0,      0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0,      0, 0,      0,      0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0,      0, 0,      0,      0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0,      0, 0,      0,      0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0,      0, 0,      0,      0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      0, 0, 0,      0, 0x8000, 0, 0x8000, 0x8000, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x8000, 0, 0, 0x8000, 0
};

// GLOBAL: XWA 0x5B6560
const uint16_t g_projectileLifetimeSecondsByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1] = {
	1, 1, 1, 1, 1, 2, 40, 25, 3, 3, 5, 45, 25, 120, 90, 25, 40, 3, 5, 10, 0, 3, 4, 3, 3, 4, 10
};

// GLOBAL: XWA 0x5B6598
const uint16_t g_projectileLifetimeFracQ16ByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1] = {
	0, 0x8000, 0, 0x8000, 0x8000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x8000, 0, 0, 0x8000, 0
};

// GLOBAL: XWA 0x5B6528
const uint16_t g_projectileSpeedByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1] = {
	2000, 2000, 1800, 1800, 1400, 1600, 250, 500,  1000, 900,  400,  300, 600, 50,
	100,  600,  300,  1400, 500,  225,  0,   1200, 1000, 1250, 1100, 900, 0
};

#ifndef XWA_MODERN
/* The original executable addresses compact projectile tables through biased object-type bases. */
extern const uint16_t g_projectileSpeedByObjectType_BiasedBase[];
extern const uint8_t g_projectileRequiredIffByObjectType_BiasedBase[];
extern const uint16_t g_projectileAlternateIffTypeByObjectType_BiasedBase[];
#endif

// GLOBAL: XWA 0x5B64B8
const int g_projectileDamageByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1] = {
	250,   500,  200,  400, 200, 400,  10000, 3000, 1500, 1600, 800,  15000, 6000,   65000,
	35000, 3000, 6000, 600, 500, 2000, 0,     750,  1000, 750,  1000, 1200,  1000000
};

// GLOBAL: XWA 0x5B6608
const uint8_t g_projectileWarheadClassByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1] = {
	0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 2, 1, 2, 2, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0
};

// GLOBAL: XWA 0x5B63F8 (biased-base; only the warhead object-type window carries data)
const uint16_t g_warheadProjectilePointValueByObjectType[OBJ_LaserImperialDS + 1] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 9, 6, 0, 0, 0, 15, 15, 45, 24, 9, 9, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0
};

// GLOBAL: XWA 0x5B6634
const uint16_t g_warheadProjectilePointValue[11] = {
	9, 6, 0, 0, 0, 15, 15, 45, 24, 9, 9,
};

// GLOBAL: XWA 0x5B6658 (biased-base; only the warhead object-type window carries data)
const uint8_t g_projectileHomingProfileBaseByObjectType[OBJ_LaserImperialDS + 1] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 14, 21, 28, 14, 14, 0, 35, 0, 0, 0, 0, 0, 0, 0, 0
};

// GLOBAL: XWA 0x5B6790 (7-entry rows: row base 0/7/14/21/28/35 + homing tier)
const uint16_t g_projectileHomingTurnRateByProfile[44] = {
	0,    1024, 2048, 3072,  5120,  7168,  9216, 0,    512,   1024,  2048,  3072,  4608, 6144, 0,
	2048, 4096, 5120, 10240, 14336, 18432, 0,    32,   64,    80,    96,    112,   128,  0,    512,
	1024, 1280, 1536, 1792,  2048,  0,     4096, 8192, 12288, 16384, 20480, 24576, 0,    0,
};

// GLOBAL: XWA 0x5B67E8 (rows at base 0 and base 7)
const uint16_t g_projectileHomingSpeedAdjustRateByProfile[12] = {
	0, 50, 100, 200, 300, 400, 500, 0, 25, 50, 100, 150,
};

// GLOBAL: XWA 0x5B65D0
const int16_t g_projectileLaunchOffsetByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1] = {
	921, 921, 921, 921, 921, 921, 512, 512, 921, 921, 921, 512, 512, 48,
	512, 512, 512, 921, 256, 256, 0,   921, 921, 921, 921, 921, 921
};

// GLOBAL: XWA 0x5B6660
const uint8_t g_projectileRequiredIffByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1] = {
	0,    0,    1,    1, 0xff, 0xff, 0xff, 0xff, 0, 1, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0, 0xff, 0xff, 0xff, 0,    0, 1, 1,    1,    1
};

// GLOBAL: XWA 0x5B6680
const uint16_t g_projectileAlternateIffTypeByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1] = {
	OBJ_LaserImperial,
	OBJ_LaserImperialTurbo,
	OBJ_LaserRebel,
	OBJ_LaserRebelTurbo,
	OBJ_LaserIon,
	OBJ_LaserIonTurbo,
	OBJ_WarheadTorpedo,
	OBJ_WarheadMissile,
	OBJ_WarheadLaser2,
	OBJ_WarheadLaser1,
	OBJ_WarheadIon,
	OBJ_WarheadAdvancedTorpedo,
	OBJ_WarheadAdvancedMissile,
	OBJ_WarheadSpaceBomb,
	OBJ_WarheadRocket,
	OBJ_WarheadMagPulse,
	OBJ_WarheadIonPulse,
	OBJ_LaserImperialTurbo_303,
	OBJ_WarheadFlare,
	OBJ_SparkTextureGroup3002_299,
	OBJ_ChaffTextureGroup5000,
	OBJ_LaserImperialTurbo_304,
	OBJ_LaserImperialTurbo_305,
	OBJ_WarheadLaser3,
	OBJ_LaserRebelTurbo_301,
	OBJ_LaserRebelTurbo_302,
	OBJ_LaserImperialDS
};

// GLOBAL: XWA 0x5FE738
const uint16_t g_warheadLeadErrorModulusByAiLevel[6] = { 15, 10, 6, 5, 2, 0 };
// GLOBAL: XWA 0x5FE748
const uint16_t g_warheadFireChanceThresholdByAiLevel[6] = {
	0xc000, 0xa000, 0x9000, 0x8000, 0x6000, 0x4000,
};
// GLOBAL: XWA 0x5FE758
const uint8_t g_projectileRequiredIffByShooterIff[8] = { 0, 1, 0, 1, 1, 0, 0, 0 };

// FUNCTION: XWA 0x490EB0
uint16_t laser_GetProjectileLifetimeTicks(ObjectTypeId projectileObjectType) {
	uint16_t wholeSecondsTicks;

	wholeSecondsTicks =
		(uint16_t)(236u * g_projectileLifetimeSecondsByType[projectileObjectType - OBJ_LaserRebel]);
	wholeSecondsTicks =
		(uint16_t)(wholeSecondsTicks +
				   MATH2_fraction(g_projectileLifetimeFracQ16ByType[projectileObjectType - OBJ_LaserRebel],
								  236u));
	return wholeSecondsTicks;
}

static __inline int laser_IsLargeLockTargetGenus(ModelGenusId genusId) {
	return genusId == GENUS_Starship || genusId == GENUS_Platform || genusId == GENUS_Container ||
		   genusId == GENUS_Freighter;
}

static __inline void laser_UpdatePlayerWarheadLock(uint16_t objectIdx, int playerIdx) {
	uint16_t currentTargetObjIdx;
	uint16_t loadedWarheadCount;
	ModelIndex modelIndex;

	if (g_players[playerIdx].selectedWeaponMode == 0) {
		return;
	}

#ifdef XWA_MODERN
	loadedWarheadCount = 0;
#endif
	if ((int16_t)GetModelIndexFromType(g_objectTable[objectIdx].objectType) != -1) {
		uint16_t firstSlot;

		modelIndex = (ModelIndex)GetModelIndexFromType(g_objectTable[objectIdx].objectType);
		firstSlot = g_modelDefs[modelIndex].warheadLauncherFirstSlot[g_players[playerIdx].selectedWarhead];
		loadedWarheadCount =
			g_curCraft->warheadData[firstSlot + 1u].count + g_curCraft->warheadData[firstSlot].count;
	}

	currentTargetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
	if (currentTargetObjIdx == 0xffffu || loadedWarheadCount == 0) {
		g_players[playerIdx].missileLockState = 0;
		g_curCraft->warheadLockTicks = 0;
		return;
	}

	{
		uint32_t lockRange;
		uint16_t lockThreshold;
		int lockTicks;
		unsigned int targetingScore;

		if (g_objectTable[currentTargetObjIdx].genusId == GENUS_Starship ||
			g_objectTable[currentTargetObjIdx].genusId == GENUS_Platform) {
			Object_DirectionAndDistanceToMeshCenter(objectIdx, currentTargetObjIdx,
													(uint16_t)g_players[playerIdx].selectedTargetComponent);
		} else {
			pai_ObjectRefDirectionToObjectRef(objectIdx, currentTargetObjIdx);
		}

		lockRange = 101805u;
		if (currentTargetObjIdx >= g_activeRegionObjectSlotStart &&
			currentTargetObjIdx < g_activeRegionCraftObjectSlotEnd) {
			if (g_objectTable[currentTargetObjIdx].genusId == GENUS_Starship ||
				g_objectTable[currentTargetObjIdx].genusId == GENUS_Platform ||
				g_objectTable[currentTargetObjIdx].genusId == GENUS_Container ||
				g_objectTable[currentTargetObjIdx].genusId == GENUS_Freighter) {
				lockRange = 244332u;
			} else if (g_objectTable[currentTargetObjIdx].genusId == GENUS_Transport) {
				lockRange = 162888u;
			}
		}

		if (g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].genusId == GENUS_Starship ||
			g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].genusId == GENUS_Platform) {
			targetingScore = (uint16_t)Targeting_ScoreCandidate(
				currentTargetObjIdx, 0, playerIdx, (uint16_t)g_players[playerIdx].selectedTargetComponent);
		} else {
			targetingScore = (uint16_t)Targeting_ScoreCandidate(currentTargetObjIdx, 0, playerIdx, 0xffffu);
		}

		if ((uint32_t)trig2_polardistance < lockRange && targetingScore) {
			g_curCraft->warheadLockTicks =
				(uint16_t)(g_curCraft->warheadLockTicks + (uint16_t)g_elapsedTicks);
			if ((int16_t)g_curCraft->warheadLockTicks < 0) {
				g_curCraft->warheadLockTicks = 708;
			}

			if (currentTargetObjIdx >= g_activeRegionObjectSlotStart &&
				currentTargetObjIdx < g_activeRegionCraftObjectSlotEnd) {
				CraftData* targetCraft;

				targetCraft = g_objectTable[currentTargetObjIdx].mobj->pCraft;
				if (targetCraft->cmTypeId == 1 && targetCraft->chaffActiveTimer != 0) {
					g_curCraft->warheadLockTicks =
						(uint16_t)(g_curCraft->warheadLockTicks - ((uint16_t)g_elapsedTicks >> 1));
				}
			}

			{
				ModelIndex missileBoatModelIndex;

				missileBoatModelIndex = (ModelIndex)GetModelIndexFromType(OBJ_MissileBoat);
				modelIndex = (ModelIndex)GetModelIndexFromType(g_objectTable[objectIdx].objectType);
				lockThreshold = (uint16_t)(modelIndex != missileBoatModelIndex ? 708u : 354u);
			}
			lockTicks = (int16_t)g_curCraft->warheadLockTicks;
			if (lockTicks >= (int)lockThreshold) {
				g_players[playerIdx].missileLockState = 3;
			} else {
				g_players[playerIdx].missileLockState = 2;
			}

			if (playerIdx == g_localPlayer && !g_fsfxEnemyFighterAttackCalloutPlayed &&
				lockTicks >= (int)(lockThreshold >> 1) && lockTicks < (int)lockThreshold &&
				laser_IsLargeLockTargetGenus(g_objectTable[currentTargetObjIdx].genusId)) {
				unsigned int scanObjIdx;

				for (scanObjIdx = g_activeRegionObjectSlotStart;
					 (uint16_t)scanObjIdx < g_activeRegionCraftObjectSlotEnd; ++scanObjIdx) {
					ObjectRecord* scanObj;
					MobileObject* scanMobj;
					CraftData* scanCraft;
					uint16_t groupIdx;

					scanObj = &g_objectTable[(uint16_t)scanObjIdx];
					if (scanObj->genusId != GENUS_Fighter) {
						continue;
					}

					scanMobj = scanObj->mobj;
					if (scanMobj == NULL) {
						continue;
					}

					scanCraft = scanMobj->pCraft;
					if (scanCraft == NULL || scanCraft->aiController.targetObjIdx != playerIdx) {
						continue;
					}

					for (groupIdx = 0; groupIdx < scanCraft->cannonClassCount; ++groupIdx) {
						if (scanCraft->laserLinkMode[groupIdx] != 0) {
							fsfx_speakorderack(g_localPlayer, -1, 34, 3, 0xffffu, 0xffffu);
							g_fsfxEnemyFighterAttackCalloutPlayed = 1;
							break;
						}
					}
				}
			}
			return;
		}

		if ((int16_t)g_curCraft->warheadLockTicks > 0) {
			g_curCraft->warheadLockTicks =
				(uint16_t)(g_curCraft->warheadLockTicks - ((uint16_t)g_elapsedTicks >> 1) -
						   (uint16_t)g_elapsedTicks);
			if ((int16_t)g_curCraft->warheadLockTicks < 0) {
				g_curCraft->warheadLockTicks = 0;
			}
		}
		g_players[playerIdx].missileLockState = 0;
	}
}

static __inline void laser_UpdatePlayerBeamWeapon(int playerIdx) {
	uint16_t beamTargetObjIdx;

	beamTargetObjIdx = 0xffffu;
	if ((g_curCraft->workingSubsystems & 0x100u) != 0 && g_curCraft->beamActive != 0 &&
		g_curCraft->beamTypeId != 0 && g_players[playerIdx].regionSessionId == 0) {
		if (g_players[playerIdx].beamFireCooldownTimer == 0) {
			int16_t beamPresent;

			g_players[playerIdx].beamFireCooldownTimer = 59;
			beamPresent = (int16_t)(g_curCraft->beamPresent - 125);
			if (beamPresent < 0) {
				beamPresent = 0;
			}
			g_curCraft->beamPresent = (uint16_t)beamPresent;
			if (beamPresent == 0 && g_curCraft->beamActive != 0) {
				g_curCraft->beamActive = 0;
				g_curCraft->beamTimer = 0;
				if (playerIdx == g_localPlayer) {
					msg_emitInFlightMessage(g_curCraft->beamTypeId + 307, g_localPlayer);
				}
			}
		}

		if (g_players[playerIdx].currentTargetObjectIdx != 0xffffu) {
			uint16_t currentTargetObjIdx;

			currentTargetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
			if (currentTargetObjIdx >= g_activeRegionObjectSlotStart &&
				currentTargetObjIdx < g_activeRegionCraftObjectSlotEnd &&
				g_objectTable[currentTargetObjIdx].mobj->pCraft->objectKind == 0 &&
				Targeting_ScoreCandidate(currentTargetObjIdx, 0, playerIdx, 0xffffu) &&
				(uint32_t)g_targetRangeScore < 0x20000u) {
				beamTargetObjIdx = currentTargetObjIdx;
			}
		}

		if (beamTargetObjIdx != 0xffffu) {
			CraftData* targetCraft;

			targetCraft = g_objectTable[(uint16_t)beamTargetObjIdx].mobj->pCraft;
			if (g_curCraft->beamTypeId == 1 || g_curCraft->beamTypeId == 2) {
				if (targetCraft->cmTypeId == 1 && targetCraft->chaffActiveTimer != 0) {
					if (playerIdx == g_localPlayer && Hud_GetSystemMessagePaneState() != MSG_BEAM_DISRUPTED) {
						msg_emitInFlightMessage(MSG_BEAM_DISRUPTED, playerIdx);
					}
					beamTargetObjIdx = 0xffffu;
				} else {
					targetCraft->beamEffectAccum[g_curCraft->beamTypeId] += (uint16_t)g_curCraft->beamTimer;
				}
			}
		}

		if (playerIdx == g_localPlayer) {
			g_localBeamTargetObjIdx = beamTargetObjIdx;
			fsfx_UpdateBeamSystemLoop(1, playerIdx);
		}
	} else if (playerIdx == g_localPlayer) {
		g_localBeamTargetObjIdx = beamTargetObjIdx;
		if ((g_curCraft->systemFlags & 0x100u) != 0) {
			fsfx_UpdateBeamSystemLoop(0, playerIdx);
		}
	}
}

static __inline void laser_UpdateAiPowerRedirects(uint16_t objectIdx, int* shieldRechargeRate) {
	if (g_objectTable[objectIdx].genusId == GENUS_Fighter) {
		AiController* ai;
		uint16_t groupAI;

		ai = pai_GetEffectiveAIController(g_curCraft);
		g_curCraft->shieldRedirect = 2;
		g_curCraft->laserRedirect = 2;
		if ((g_curCraft->systemFlags & 1u) != 0) {
			int shieldMax;

			groupAI = g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.groupAI;
			if (ai->maneuverMode == 32) {
				if (groupAI == 5) {
					g_curCraft->shieldRedirect = 0;
				} else if (groupAI == 4 || groupAI == 3) {
					g_curCraft->shieldRedirect = 1;
				} else {
					g_curCraft->shieldRedirect = 2;
				}
				g_curCraft->laserRedirect = 3;
			}

			shieldMax = 4 * g_modelDefs[g_objectTable[objectIdx].mobj->pCraft->modelIndex].shieldStrength;
			if (g_curCraft->shieldFront < shieldMax) {
				int chargeBudget;
				int16_t totalLaserCharge;
				uint16_t drainSlotIdx;
				unsigned int budgetTicks;
				int shieldStep;

				if (g_curCraft->laserRedirect == 2) {
					g_curCraft->laserRedirect = 4;
				}
				if (g_curCraft->shieldFront <= 0) {
					if (g_curCraft->shieldRedirect == 2) {
						g_curCraft->shieldRedirect = 4;
					}
					if (groupAI < 2u) {
						chargeBudget = 250;
					} else {
						chargeBudget = groupAI < 3u ? 500 : 10000;
					}
				} else {
					if (groupAI < 2u) {
						chargeBudget = 0;
					} else {
						uint16_t mask;

						if (groupAI == 5) {
							mask = 1;
						} else if (groupAI == 4) {
							mask = 3;
						} else {
							mask = groupAI == 3 ? 7 : 15;
						}
						chargeBudget = 100 * ((g_missionElapsedClock.seconds & mask) == mask);
					}
				}

				totalLaserCharge = 0;
				for (drainSlotIdx = 0; drainSlotIdx < g_curCraft->laserSlotCount; ++drainSlotIdx) {
					if ((int8_t)g_curCraft->warheadData[drainSlotIdx].laserCharge > 0) {
						totalLaserCharge =
							(int16_t)(totalLaserCharge +
									  (int8_t)g_curCraft->warheadData[drainSlotIdx].laserCharge);
					}
				}

				shieldStep = g_objectTable[objectIdx].objectType == OBJ_MissileBoat ? 32 : 4;
				for (drainSlotIdx = 0, budgetTicks = 0;
					 totalLaserCharge != 0 && budgetTicks < (uint16_t)chargeBudget; ++budgetTicks) {
					if ((int8_t)g_curCraft->warheadData[drainSlotIdx].laserCharge > 0) {
						--totalLaserCharge;
						g_curCraft->warheadData[drainSlotIdx].laserCharge =
							(uint8_t)(g_curCraft->warheadData[drainSlotIdx].laserCharge - 1u);
						g_curCraft->shieldFront += shieldStep;
						if (g_curCraft->shieldFront >= shieldMax) {
							totalLaserCharge = 0;
						}
					}
					++drainSlotIdx;
					if (drainSlotIdx >= g_curCraft->laserSlotCount) {
						drainSlotIdx = 0;
					}
				}
			}
		}

		if (g_curCraft->laserRedirect == 2) {
			int16_t totalCharge;
			uint16_t chargedSlotCount;
			uint16_t slotIdx;

			totalCharge = 0;
			chargedSlotCount = 0;
			for (slotIdx = 0; slotIdx < g_curCraft->laserSlotCount; ++slotIdx) {
				if (g_curCraft->warheadData[slotIdx].weaponType != 0) {
					totalCharge =
						(int16_t)(totalCharge + (int8_t)g_curCraft->warheadData[slotIdx].laserCharge);
					++chargedSlotCount;
				}
			}
			if (chargedSlotCount != 0) {
				int average;

				average = totalCharge / (int)chargedSlotCount;
				if ((int16_t)average < 32) {
					g_curCraft->laserRedirect = 4;
				} else {
					g_curCraft->laserRedirect = (uint8_t)(((int16_t)average < 96) + 2);
				}
			}
		}

		*shieldRechargeRate = 20;
		return;
	}

	g_curCraft->shieldRedirect = 2;
	g_curCraft->laserRedirect = 2;
	{
		int liveShieldGenerators;

		liveShieldGenerators = 0;
		if (g_flightDifficulty >= 1u && g_objectTable[objectIdx].genusId == GENUS_Starship) {
			if (g_objectTable[objectIdx].objectType == OBJ_Interdictor2 ||
				g_objectTable[objectIdx].objectType == OBJ_VictoryStarDestroyer2 ||
				g_objectTable[objectIdx].objectType == OBJ_VictoryStarDestroyer2_149 ||
				g_objectTable[objectIdx].objectType == OBJ_ImperialStarDestroyer2 ||
				g_objectTable[objectIdx].objectType == OBJ_ImperialStarDestroyer2_150 ||
				g_objectTable[objectIdx].objectType == OBJ_SuperStarDestroyer) {
				int meshCount;
				int meshIdx;

				meshCount = ModelMesh_GetObjectTypeMeshCount(g_objectTable[objectIdx].objectType);
				for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
					if (ModelMesh_GetObjectTypeMeshType(g_objectTable[objectIdx].objectType, meshIdx) ==
							MESH_ShieldGenerator &&
						g_curCraft->componentHp[meshIdx] != 0) {
						++liveShieldGenerators;
					}
				}
			} else {
				liveShieldGenerators = 1;
			}
			g_curCraft->shieldRedirect = 3;
		}
		if (g_objectTable[objectIdx].playerOwnerIdx == -1) {
			*shieldRechargeRate = 5 * liveShieldGenerators;
		} else {
			*shieldRechargeRate = 20;
		}
	}
}

// FUNCTION: XWA 0x48FC00
void laser_weaponsfire(void) {
	char doPeriodicPowerUpdate;
	uint16_t objectIdx;

	doPeriodicPowerUpdate = 0;
	if (g_flightGlobalCountdownTimers[10] == 0) {
		if (regionIdx == g_activeMissionRegionCount - 1) {
			g_flightGlobalCountdownTimers[10] = 236;
		}
		doPeriodicPowerUpdate = 1;
	}

	{
		uint16_t clearObjIdx;

		for (clearObjIdx = g_activeRegionObjectSlotStart; clearObjIdx < g_activeRegionCraftObjectSlotEnd;
			 ++clearObjIdx) {
			if (g_objectTable[clearObjIdx].objectType == OBJ_None) {
				continue;
			}
			if (g_objectTable[clearObjIdx].mobj->state == 0) {
				memset(g_objectTable[clearObjIdx].mobj->pCraft->beamEffectAccum, 0,
					   sizeof(g_objectTable[clearObjIdx].mobj->pCraft->beamEffectAccum));
			}
		}
	}

	for (objectIdx = g_activeRegionObjectSlotStart; (uint16_t)objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		int shieldRechargeRate;

		if (g_objectTable[objectIdx].objectType == OBJ_None) {
			continue;
		}
		if (g_objectTable[objectIdx].mobj->state != 0) {
			continue;
		}

		if (g_objectTable[objectIdx].playerOwnerIdx != -1) {
			int playerIdx;
			unsigned int scanObjIdx;

			g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
			playerIdx = g_objectTable[objectIdx].playerOwnerIdx;
			laser_UpdatePlayerWarheadLock(objectIdx, playerIdx);
			laser_UpdatePlayerBeamWeapon(playerIdx);

			shieldRechargeRate = 20;
			for (scanObjIdx = g_activeRegionObjectSlotStart;
				 (uint16_t)scanObjIdx < g_activeRegionCraftObjectSlotEnd; ++scanObjIdx) {
				if (g_missionHeader.body.hangar != XWA_HANGAR_DEATHSTAR) {
					if (g_objectTable[(uint16_t)scanObjIdx].objectType != OBJ_None &&
						(g_objectTable[(uint16_t)scanObjIdx].genusId == GENUS_Starship ||
						 g_objectTable[(uint16_t)scanObjIdx].genusId == GENUS_Platform) &&
						Object_IsHostileToTeam(objectIdx, g_objectTable[(uint16_t)scanObjIdx].mobj->team)) {
						collide_ApplyHostileProximityWeaponDisruption(objectIdx, (uint16_t)scanObjIdx);
					}
				}
			}
		} else {
			if (!doPeriodicPowerUpdate) {
				continue;
			}
			g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
			laser_UpdateAiPowerRedirects(objectIdx, &shieldRechargeRate);
		}

		if (doPeriodicPowerUpdate) {
			g_curCraft = g_objectTable[objectIdx].mobj->pCraft;

			if (g_objectTable[objectIdx].objectType == OBJ_YWing) {
				shieldRechargeRate *= 2;
			}

			if ((g_curCraft->workingSubsystems & 1u) != 0 && shieldRechargeRate != 0) {
				int shieldDelta;

				shieldDelta = shieldRechargeRate * ((int)g_curCraft->shieldRedirect - 2);
				if (shieldDelta != 0) {
					if (g_curCraft->shieldDistribMode == 0) {
						int maxShield;

						g_curCraft->shieldFront += shieldDelta;
						maxShield = 2 * g_modelDefs[g_curCraft->modelIndex].shieldStrength;
						if (g_curCraft->shieldFront < 0) {
							g_curCraft->shieldFront = 0;
						}
						if (g_curCraft->shieldFront > maxShield) {
							g_curCraft->shieldFront = maxShield;
						}
					} else if (g_curCraft->shieldDistribMode == 2) {
						int maxShield;

						g_curCraft->shieldRear += shieldDelta;
						maxShield = 2 * g_modelDefs[g_curCraft->modelIndex].shieldStrength;
						if (g_curCraft->shieldRear < 0) {
							g_curCraft->shieldRear = 0;
						}
						if (g_curCraft->shieldRear > maxShield) {
							g_curCraft->shieldRear = maxShield;
						}
					} else {
						int maxShield;

						g_curCraft->shieldFront += shieldDelta / 2;
						maxShield = 2 * g_modelDefs[g_curCraft->modelIndex].shieldStrength;
						if (g_curCraft->shieldFront < 0) {
							g_curCraft->shieldFront = 0;
						}
						if (g_curCraft->shieldFront > maxShield) {
							g_curCraft->shieldFront = maxShield;
						}
						g_curCraft->shieldRear += shieldDelta / 2;
						maxShield = 2 * g_modelDefs[g_curCraft->modelIndex].shieldStrength;
						if (g_curCraft->shieldRear < 0) {
							g_curCraft->shieldRear = 0;
						}
						if (g_curCraft->shieldRear > maxShield) {
							g_curCraft->shieldRear = maxShield;
						}
					}
				}
			}

			if ((g_curCraft->workingSubsystems & 0x10u) != 0) {
				uint16_t slotIdx;

				for (slotIdx = 0; slotIdx < g_curCraft->laserSlotCount; ++slotIdx) {
					if (g_curCraft->warheadData[slotIdx].weaponType != 0 &&
						g_curCraft->warheadData[slotIdx].weaponType < 4u) {
						int16_t chargeBasis;
						int16_t chargeDelta;

						chargeBasis = (int16_t)((int)g_curCraft->laserRedirect - 2);
						if (g_curCraft->slamActive != 0) {
							chargeBasis -= 4;
						}
						chargeDelta = (int16_t)(2 * chargeBasis);
						if (g_objectTable[objectIdx].objectType == OBJ_TIEFighter ||
							g_objectTable[objectIdx].objectType == OBJ_TIEBomber) {
							chargeDelta = (int16_t)(3 * chargeBasis);
						}
						g_curCraft->warheadData[slotIdx].laserCharge =
							(uint8_t)(g_curCraft->warheadData[slotIdx].laserCharge + chargeDelta);
						if (chargeDelta < 0 && (int8_t)g_curCraft->warheadData[slotIdx].laserCharge < 0) {
							g_curCraft->warheadData[slotIdx].laserCharge = 0;
						}
						if (chargeDelta > 0 && (int8_t)g_curCraft->warheadData[slotIdx].laserCharge < 0) {
							g_curCraft->warheadData[slotIdx].laserCharge = 127;
						}
					}
				}
			}

			if (g_curCraft->slamActive != 0) {
				int anyLaserCharge;
				uint16_t slotIdx;

				anyLaserCharge = 0;
				for (slotIdx = 0; slotIdx < g_curCraft->laserSlotCount; ++slotIdx) {
					if ((int8_t)g_curCraft->warheadData[slotIdx].laserCharge > 0) {
						anyLaserCharge = 1;
					}
				}
				if (!anyLaserCharge) {
					g_curCraft->slamActive = 0;
					msg_emitInFlightMessage(MSG_OVERDRIVE_OFF, g_localPlayer);
					fsfx_PlaySound(130, 0xffffu, (unsigned int)g_localPlayer);
				}
			}

			if ((g_curCraft->workingSubsystems & 0x100u) != 0) {
				int16_t beamPresent;

				beamPresent = (int16_t)(g_curCraft->beamPresent + 125 * ((int)g_curCraft->beamLevel - 2));
				if (beamPresent < 0) {
					beamPresent = 0;
				}
				if (beamPresent > 9999) {
					beamPresent = 9999;
				}
				g_curCraft->beamPresent = (uint16_t)beamPresent;
				if (beamPresent == 0 && g_curCraft->beamActive != 0) {
					g_curCraft->beamActive = 0;
					g_curCraft->beamTimer = 0;
					if (g_objectTable[objectIdx].playerOwnerIdx == g_localPlayer) {
						msg_emitInFlightMessage(g_curCraft->beamTypeId + 307, g_localPlayer);
					}
				}
			}

			if (g_curCraft->chaffActiveTimer != 0) {
				g_curCraft->chaffActiveTimer = (uint16_t)(g_curCraft->chaffActiveTimer - 1u);
				if (g_curCraft->cmTypeId == 1 && g_curCraft->chaffActiveTimer == 0 &&
					g_objectTable[objectIdx].playerOwnerIdx != -1) {
					msg_emitInFlightMessage(MSG_CHAFF_DONE, g_objectTable[objectIdx].playerOwnerIdx);
				}
			}
		}
	}

	for (objectIdx = g_activeRegionObjectSlotStart; (uint16_t)objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		if (g_objectTable[objectIdx].objectType == OBJ_None) {
			continue;
		}
		if (g_objectTable[objectIdx].mobj->state != 0) {
			continue;
		}

		g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
		if (g_curCraft->objectKind != 4 && g_curCraft->objectKind != 3 &&
			g_curCraft->weaponFireInhibitTimer == 0) {
			uint16_t slotIdx;
			uint16_t launcherIdx;

			if (g_curCraft->beamEffectAccum[2] == 0) {
				uint16_t groupIdx;

				for (groupIdx = 0; groupIdx < g_curCraft->cannonClassCount; ++groupIdx) {
					int16_t cooldown;

					cooldown = (int16_t)g_curCraft->laserFireCooldownTicks[groupIdx];
					if (cooldown != 0) {
						cooldown = (int16_t)(cooldown - (int16_t)g_elapsedTicks);
						if (cooldown < 0) {
							cooldown = 0;
						}
						g_curCraft->laserFireCooldownTicks[groupIdx] = (uint16_t)cooldown;
					}

					if (g_objectTable[objectIdx].playerOwnerIdx == -1 && cooldown < (int16_t)g_elapsedTicks &&
						g_curCraft->laserLinkMode[groupIdx] != 0) {
						if ((g_curCraft->workingSubsystems & 0x10u) != 0 && g_curCraft->objectKind == 0) {
							laser_firelasersystem(objectIdx, groupIdx, -1, 1u);
						}
						--g_curCraft->laserLinkMode[groupIdx + 3];
						g_curCraft->laserFireCooldownTicks[groupIdx] =
							(uint16_t)(g_curCraft->laserFireCooldownTicks[groupIdx] +
									   2u * (uint16_t)g_elapsedTicks);
						g_curCraft->laserLastFireTimestamp[groupIdx] += 2u * (uint16_t)g_elapsedTicks;
						if (g_curCraft->laserLinkMode[groupIdx + 3] == 0) {
							g_curCraft->laserLinkMode[groupIdx] = 0;
						}
					}
				}
			}

			for (slotIdx = 0; slotIdx < g_curCraft->laserSlotCount; ++slotIdx) {
				if (g_curCraft->warheadData[slotIdx].weaponType >= 4u &&
					g_curCraft->warheadData[slotIdx].turretTargetObjIdx != -1) {
					laser_firewarheadlauncher(objectIdx, slotIdx,
											  (uint16_t)g_curCraft->warheadData[slotIdx].turretTargetObjIdx);
				}
			}

			for (launcherIdx = 0; launcherIdx < g_curCraft->warheadLauncherCount; ++launcherIdx) {
				int16_t cooldown;

				cooldown = (int16_t)g_curCraft->warheadLauncherCooldownTicks[launcherIdx];
				if (cooldown != 0) {
					cooldown = (int16_t)(cooldown - (int16_t)g_elapsedTicks);
					if (cooldown < 0) {
						cooldown = 0;
					}
					g_curCraft->warheadLauncherCooldownTicks[launcherIdx] = (uint16_t)cooldown;
				}
			}
		}
	}

	for (objectIdx = g_objScanStart; (uint16_t)objectIdx < g_regionStaticObjectSlotEnd; ++objectIdx) {
		if (g_objectTable[objectIdx].objectType != OBJ_None &&
			g_objectTable[objectIdx].genusId == GENUS_Mine) {
			laser_UpdateMineWeaponFire(objectIdx);
		}
	}
}

static __inline int laser_ObjectTypeUsesExpandedTargetProbe(ObjectTypeId objectType) {
	return (g_modelTypeTable[objectType].flags & MODEL_TYPE_FLAG_EXPANDED_TARGET_PROBE) != 0;
}

static __inline void laser_RotateWorldDeltaToObjectLocal(ObjectRecord* obj, int dx, int dy, int dz) {
	int sideX;
	int sideY;
	int sideZ;
	int fwdX;
	int fwdY;
	int fwdZ;
	int upX;
	int upY;
	int upZ;

	sideZ = obj->mobj->cachedSideZ;
	sideY = obj->mobj->cachedSideY;
	sideX = obj->mobj->cachedSideX;
	g_rotatedX = Xwa_Dot3Q15Inline(sideX, sideY, sideZ, dx, dy, dz);
	fwdZ = obj->mobj->cachedFwdZ;
	fwdY = obj->mobj->cachedFwdY;
	fwdX = obj->mobj->cachedFwdX;
	g_rotatedY = -Xwa_Dot3Q15Inline(fwdX, fwdY, fwdZ, dx, dy, dz);
	upZ = obj->mobj->cachedUpZ;
	upY = obj->mobj->cachedUpY;
	upX = obj->mobj->cachedUpX;
	g_rotatedZ = Xwa_Dot3Q15Inline(upX, upY, upZ, dx, dy, dz);
}

static __inline void laser_RotateObjectLocalToWorldOffset(ObjectRecord* obj, int localSide, int localUp,
														  int localFwd) {
	int sideX;
	int sideY;
	int sideZ;
	int upX;
	int upY;
	int upZ;
	int fwdX;
	int fwdY;
	int fwdZ;

	fwdX = obj->mobj->cachedFwdX;
	upX = obj->mobj->cachedUpX;
	sideX = obj->mobj->cachedSideX;
	g_rotatedX = Xwa_Dot3Q15Inline(sideX, upX, fwdX, localSide, localUp, localFwd);
	upY = obj->mobj->cachedUpY;
	fwdY = obj->mobj->cachedFwdY;
	sideY = obj->mobj->cachedSideY;
	g_rotatedY = Xwa_Dot3Q15Inline(sideY, upY, fwdY, localSide, localUp, localFwd);
	fwdZ = obj->mobj->cachedFwdZ;
	upZ = obj->mobj->cachedUpZ;
	sideZ = obj->mobj->cachedSideZ;
	g_rotatedZ = Xwa_Dot3Q15Inline(sideZ, upZ, fwdZ, localSide, localUp, localFwd);
}

static __inline uint16_t laser_ComputeLauncherCooldownTicks(ModelIndex modelIndex, uint16_t launcherIdx,
															int ownerPlayerIdx) {
	uint16_t cooldown;

	cooldown = g_modelDefs[modelIndex]
				   .laserGroupFireCooldownTicks[g_curCraft->warheadData[launcherIdx].weaponGroupIdx];
	if (cooldown == 0) {
		cooldown = (uint16_t)(g_curCraft->warheadData[launcherIdx].weaponType != 4 ? 236u : 118u);
	}
	if (ownerPlayerIdx == -1) {
		uint16_t skill;

		skill = pai_GetEffectiveSkillValue(g_curCraft);
		if (skill < 0x4000u) {
			cooldown = (uint16_t)(4u * cooldown);
		} else if (skill < 0x5555u) {
			cooldown = (uint16_t)(3u * cooldown);
		} else if (skill >= 0xaaaau) {
			cooldown = (uint16_t)(2u * cooldown);
		}
	}
	return cooldown;
}

static __inline uint16_t laser_SelectWarheadLauncherProjectileType(ObjectRecord* ownerObj,
																   ModelIndex modelIndex,
																   uint16_t launcherIdx) {
	uint16_t projectileType;

	if (g_curCraft->warheadData[launcherIdx].count != 0) {
		int isIonLauncherGroup;
		int groupIdx;
		int groupsLeft;

		isIonLauncherGroup = 0;
		for (groupsLeft = 3, groupIdx = 0; groupsLeft != 0; --groupsLeft, ++groupIdx) {
			if (launcherIdx >= g_modelDefs[modelIndex].laserGroupFirstSlot[groupIdx] &&
				launcherIdx <= g_modelDefs[modelIndex].laserGroupLastSlot[groupIdx] &&
				(g_modelDefs[modelIndex].laserGroupWeaponType[groupIdx] == OBJ_WarheadLaser1 ||
				 g_modelDefs[modelIndex].laserGroupWeaponType[groupIdx] == OBJ_WarheadLaser2)) {
				isIonLauncherGroup = 1;
			}
		}
		projectileType = (uint16_t)(OBJ_LaserIon + (isIonLauncherGroup != 0));
	} else {
		uint16_t typeIdx;
		uint8_t requiredIff;

		projectileType = g_curCraft->warheadData[launcherIdx].projectileTypeId;
		typeIdx = (uint16_t)projectileType - OBJ_LaserRebel;
#ifdef XWA_MODERN
		requiredIff = g_projectileRequiredIffByType[typeIdx];
#else
		requiredIff = g_projectileRequiredIffByObjectType_BiasedBase[projectileType];
#endif
		if (requiredIff != 0xffu &&
			g_projectileRequiredIffByShooterIff[(uint8_t)ownerObj->mobj->iff] != requiredIff) {
#ifdef XWA_MODERN
			projectileType = g_projectileAlternateIffTypeByType[typeIdx];
#else
			projectileType = g_projectileAlternateIffTypeByObjectType_BiasedBase[projectileType];
#endif
		}
	}
	return projectileType;
}

static __inline int laser_GetLargeObjectLauncherPoint(ObjectRecord* ownerObj, ModelIndex modelIndex,
													  uint16_t launcherIdx, int* outMeshIdx,
													  uint8_t* outHardpointIdx) {
	int excludedHardpointIndices[256];
	int localSide;
	int localUp;
	int localFwd;

	localSide = 0;
	localUp = 0;
	localFwd = 0;
	*outMeshIdx = 0xffu;
	*outHardpointIdx = 0xffu;

	if (g_modelDefs[modelIndex].floatHardpointCount != 0) {
		ModelFloatHardpoint* floatHardpoints;
		int excludedCount;
		uint8_t hardpointIdx;
		int slotIdx;

		excludedCount = 0;
		for (slotIdx = 0; slotIdx < g_curCraft->laserSlotCount; ++slotIdx) {
			if (slotIdx != launcherIdx) {
				excludedHardpointIndices[excludedCount++] =
					g_curCraft->warheadData[slotIdx].lastFireHardpointIdx;
			}
		}

		floatHardpoints = FeDiskIo_GetMeshFloatHardpoint(ownerObj->objectType, 0);
		hardpointIdx = ModelMesh_FindNearestLiveFloatHardpoint(
			ownerObj->objectType, g_rotatedX, g_rotatedY, g_rotatedZ, excludedCount, excludedHardpointIndices,
			floatHardpoints, g_curCraft);
		if (hardpointIdx == 0xffu) {
			return 0;
		}

		localSide = floatHardpoints[hardpointIdx].x;
		localUp = floatHardpoints[hardpointIdx].z;
		localFwd = -floatHardpoints[hardpointIdx].negY;
		*outMeshIdx = floatHardpoints[hardpointIdx].componentIndex;
		*outHardpointIdx = hardpointIdx;
	} else {
		uint8_t* vertexComponentMap;
		int excludedCount;
		int meshIdx;
		uint16_t meshIndex;
		uint8_t vertexIdx;
		int slotIdx;

		meshIdx = ModelMesh_FindNearestLiveMainHullByBounds(ownerObj->objectType, g_rotatedX, g_rotatedY,
															g_rotatedZ, g_curCraft);
		excludedCount = 0;
		for (slotIdx = 0; slotIdx < g_curCraft->laserSlotCount; ++slotIdx) {
			if ((uint16_t)g_curCraft->warheadData[slotIdx].lastFireMeshIdx == (uint16_t)meshIdx &&
				slotIdx != launcherIdx) {
				excludedHardpointIndices[excludedCount++] =
					g_curCraft->warheadData[slotIdx].lastFireHardpointIdx;
			}
		}

		meshIndex = (uint16_t)meshIdx;
		vertexComponentMap = FeDiskIo_GetMeshVertexComponentMap(ownerObj->objectType, meshIndex);
		vertexIdx = ModelMesh_FindNearestVertexForPoint(
			ownerObj->objectType, g_rotatedX, g_rotatedY, g_rotatedZ, meshIndex, excludedCount,
			excludedHardpointIndices, vertexComponentMap, g_curCraft);
		if (vertexIdx == 0xffu) {
			return 0;
		}

		localSide = ModelMesh_GetVertexX(ownerObj->objectType, meshIndex, vertexIdx);
		localFwd = -ModelMesh_GetVertexY(ownerObj->objectType, meshIndex, vertexIdx);
		localUp = ModelMesh_GetVertexZ(ownerObj->objectType, meshIndex, vertexIdx);
		*outMeshIdx = (uint8_t)meshIndex;
		*outHardpointIdx = vertexIdx;
	}

	laser_RotateObjectLocalToWorldOffset(ownerObj, localSide, localUp, localFwd);
	return 1;
}

static __inline void laser_GetModelHardpointLauncherPoint(ObjectRecord* ownerObj, ModelIndex modelIndex,
														  uint16_t launcherIdx, int initialMeshIdx,
														  uint8_t initialHardpointIdx, int* outMeshIdx,
														  uint8_t* outHardpointIdx) {
	int16_t localSide;
	int16_t localUp;
	int16_t localFwd;
	int meshIdx;
	uint16_t meshIndex;
	uint8_t hardpointIdx;

	meshIdx = initialMeshIdx;
	hardpointIdx = initialHardpointIdx;

	if (hardpointIdx != 0xffu && (((uint16_t)g_missionElapsedClock.subsecondTicks & 1u) != 0)) {
		meshIdx = initialMeshIdx;
		meshIndex = (uint16_t)meshIdx;
		hardpointIdx = initialHardpointIdx;
		if (ownerObj->objectType == OBJ_ImperialStarDestroyer2) {
			localSide = ModelMesh_GetHardpointX(OBJ_ImperialStarDestroyer2, meshIndex, hardpointIdx) >> 1;
			localFwd = ModelMesh_GetHardpointY(ownerObj->objectType, meshIndex, hardpointIdx) >> 1;
			localUp = ModelMesh_GetHardpointZ(ownerObj->objectType, meshIndex, hardpointIdx) >> 1;
		} else {
			localSide = ModelMesh_GetHardpointX(ownerObj->objectType, meshIndex, hardpointIdx);
			localFwd = ModelMesh_GetHardpointY(ownerObj->objectType, meshIndex, hardpointIdx);
			localUp = ModelMesh_GetHardpointZ(ownerObj->objectType, meshIndex, hardpointIdx);
		}
	} else {
		localSide = g_modelDefs[modelIndex].weaponHardpoints[launcherIdx].x;
		localUp = g_modelDefs[modelIndex].weaponHardpoints[launcherIdx].z;
		localFwd = g_modelDefs[modelIndex].weaponHardpoints[launcherIdx].y;
		meshIndex = (uint16_t)meshIdx;
	}

	if (ModelMesh_GetObjectTypeMeshType(ownerObj->objectType, meshIndex) == MESH_RotaryGunTurret) {
		int rotateSide;
		int rotateFwd;
		int rotateUp;

		rotateSide = localSide;
		rotateFwd = localFwd;
		rotateUp = localUp;
		g_rotatedX = rotateSide;
		g_rotatedY = rotateFwd;
		g_rotatedZ = rotateUp;
		if (ownerObj->objectType == OBJ_ImperialStarDestroyer2) {
#ifdef XWA_MODERN
			rotateSide *= 2;
			rotateFwd *= 2;
			rotateUp *= 2;
#else
			rotateSide <<= 1;
			rotateFwd <<= 1;
			rotateUp <<= 1;
#endif
			g_rotatedX = rotateSide;
			g_rotatedY = rotateFwd;
			g_rotatedZ = rotateUp;
		}
		ModelMesh_ApplyAnimatedMeshRotationToPoint((uint16_t)g_curCraft->meshRotation[meshIndex] << 8,
												   ownerObj->objectType, meshIndex, rotateSide, rotateFwd,
												   rotateUp);
		if (ownerObj->objectType == OBJ_ImperialStarDestroyer2) {
			g_rotatedX >>= 1;
			g_rotatedY >>= 1;
			g_rotatedZ >>= 1;
		}
		localSide = g_rotatedX;
		localFwd = g_rotatedY;
		localUp = g_rotatedZ;
	}

	pai_calcrotatedpoint(ownerObj, localSide, localUp, localFwd);
	if (ownerObj->objectType == OBJ_ImperialStarDestroyer2) {
#ifdef XWA_MODERN
		g_rotatedX *= 2;
		g_rotatedY *= 2;
		g_rotatedZ *= 2;
#else
		g_rotatedX <<= 1;
		g_rotatedY <<= 1;
		g_rotatedZ <<= 1;
#endif
	}

	*outMeshIdx = meshIndex;
	*outHardpointIdx = hardpointIdx;
}

static __inline void laser_ApplyWarheadLauncherAimError(ObjectRecord* ownerObj, ObjectRecord* targetObj,
														int16_t* pitch, uint16_t* yaw) {
	int yawError;
	int pitchError;

	if (ownerObj->playerOwnerIdx != -1) {
		MobileObject* targetMobj;

		targetMobj = targetObj->mobj;
		if (targetMobj != NULL && targetMobj->speed != 0) {
			yawError = GameRand() & 0x7f;
			yawError += (uint8_t)GameRand();
		} else {
			yawError = (uint8_t)GameRand();
		}
		if ((uint16_t)GameRand() >= 0x8000u) {
			yawError = -yawError;
		}
		*yaw = (uint16_t)(*yaw + yawError);
		pitchError = (uint8_t)GameRand();
		if ((uint16_t)GameRand() >= 0x8000u) {
			*pitch = (int16_t)(*pitch - pitchError);
			if (((uint16_t)*pitch & 0x8000u) != 0) {
				*pitch = 0;
			}
		} else {
			*pitch = (int16_t)(*pitch + pitchError);
			if (((uint16_t)*pitch & 0x8000u) != 0) {
				*pitch = 0x7fff;
			}
		}
	} else {
		if (targetObj->genusId != GENUS_PlayerProjectile && targetObj->genusId != GENUS_NpcProjectile) {
			return;
		}
		{
			uint16_t fireRoll;
			uint8_t groupAi;

			fireRoll = GameRand();
			groupAi = g_missionFlightGroups[ownerObj->flightGroupIdx].fg.groupAI;
			if (fireRoll >= g_warheadFireChanceThresholdByAiLevel[groupAi]) {
				return;
			}
		}
		yawError = (GameRand() & 0x3f) + 32;
		if ((uint16_t)GameRand() >= 0x8000u) {
			yawError = -yawError;
		}
		*yaw = (uint16_t)(*yaw + yawError);
		pitchError = GameRand() & 0x5f;
		if ((uint16_t)GameRand() >= 0x8000u) {
			*pitch = (int16_t)(*pitch - pitchError);
			if (((uint16_t)*pitch & 0x8000u) != 0) {
				*pitch = 0;
			}
		} else {
			*pitch = (int16_t)(*pitch + pitchError);
			if (((uint16_t)*pitch & 0x8000u) != 0) {
				*pitch = 0x7fff;
			}
		}
	}
}

// FUNCTION: XWA 0x4E1760
void laser_firewarheadlauncher(unsigned int ownerObjIdx, uint16_t launcherIdx, uint16_t targetRef) {
	ModelIndex modelIndex;
	ObjectRecord* ownerObj;
	int meshIdx;
	uint8_t hardpointIdx;
	int launchX;
	int launchY;
	int launchZ;
	int targetX;
	int targetY;
	int targetZ;
	int dx;
	int dy;
	int dz;
	uint16_t projectileType;
	uint16_t yaw;
	int16_t pitch;
	uint16_t projectileObjIdx;
	MobileObject* projectileMobj;

	if (g_curCraft->workingSubsystems == 0) {
		return;
	}

	if (g_objectTable[targetRef].objectType == OBJ_None) {
		g_curCraft->warheadData[launcherIdx].turretTargetObjIdx = -1;
		return;
	}

	modelIndex = g_curCraft->modelIndex;
	ownerObj = &g_objectTable[(uint16_t)ownerObjIdx];

	if (!laser_ObjectTypeUsesExpandedTargetProbe(ownerObj->objectType)) {
		meshIdx = g_modelDefs[modelIndex].weaponHardpoints[launcherIdx].meshIdx;
		hardpointIdx = g_modelDefs[modelIndex].weaponHardpoints[launcherIdx].alternateMeshHardpointIdx;
		if (g_curCraft->componentHp[meshIdx] == 0) {
			return;
		}
	}

	if (g_curCraft->beamEffectAccum[2] >= 0x28000u) {
		return;
	}
	if (g_curCraft->beamEffectAccum[2] >= 0x18000u) {
		if (g_missionElapsedClock.subsecondTicks < 118) {
			g_curCraft->warheadData[launcherIdx].turretRotBucket =
				(int16_t)(g_curCraft->warheadData[launcherIdx].turretRotBucket + (uint16_t)g_elapsedTicks);
			return;
		}
#ifdef XWA_MODERN
		if (XwaModernFlightTiming_IsHighRate() && !XwaModernFlightTiming_IsLegacyCadenceDue()) {
			return;
		}
#endif
		++g_curCraft->warheadData[launcherIdx].turretRotBucket;
	} else if (g_curCraft->beamEffectAccum[2] >= 0x8000u) {
		if (g_missionElapsedClock.subsecondTicks < 118) {
			g_curCraft->warheadData[launcherIdx].turretRotBucket =
				(int16_t)(g_curCraft->warheadData[launcherIdx].turretRotBucket + (uint16_t)g_elapsedTicks);
			return;
		}
#ifdef XWA_MODERN
		if (XwaModernFlightTiming_IsHighRate() && !XwaModernFlightTiming_IsLegacyCadenceDue()) {
			return;
		}
#endif
		g_curCraft->warheadData[launcherIdx].turretRotBucket =
			(int16_t)(g_curCraft->warheadData[launcherIdx].turretRotBucket + 2);
	}
	if (g_curCraft->warheadData[launcherIdx].turretRotBucket > 0) {
		return;
	}

	{
		int ownerPlayerIdx;

		ownerPlayerIdx = ownerObj->playerOwnerIdx;
		if (ownerPlayerIdx != -1) {
			if ((g_curCraft->workingSubsystems & 0x10u) == 0) {
				return;
			}
			if (g_modelDefs[g_curCraft->modelIndex].weaponHardpoints[launcherIdx].z < -64) {
				uint8_t inputDisabledFlag;

				if (g_curCraft->carriedObjectIndex != 0xffffu) {
					return;
				}
				inputDisabledFlag = g_players[ownerPlayerIdx].inputDisabledFlag;
				if (inputDisabledFlag != 0 && inputDisabledFlag != 5) {
					return;
				}
			}
		}

		g_curCraft->warheadData[launcherIdx].turretRotBucket =
			(int16_t)laser_ComputeLauncherCooldownTicks(modelIndex, launcherIdx, ownerPlayerIdx);
	}

	launchX = ownerObj->world_x;
	launchY = ownerObj->world_y;
	launchZ = ownerObj->world_z;
	if (laser_ObjectTypeUsesExpandedTargetProbe(ownerObj->objectType)) {
		Mission_ResolveObjectOrMissionPointWorldLoc(targetRef, 0, 0, 0);
		dx = worldlocx - launchX;
		dy = worldlocy - launchY;
		dz = worldlocz - launchZ;
		if (ownerObj->mobj == NULL) {
			return;
		}
		if (ownerObj->mobj->orientMatrixDirty != 0) {
			FVIEW_calcrotatemove(ownerObj->pitch, ownerObj->yaw, ownerObj);
			FVIEW_calcrotateorient(ownerObj->roll, ownerObj->angleD, ownerObj);
		}
		laser_RotateWorldDeltaToObjectLocal(ownerObj, dx, dy, dz);
		if (!laser_GetLargeObjectLauncherPoint(ownerObj, modelIndex, launcherIdx, &meshIdx, &hardpointIdx)) {
			return;
		}
	} else {
		laser_GetModelHardpointLauncherPoint(ownerObj, modelIndex, launcherIdx, meshIdx, hardpointIdx,
											 &meshIdx, &hardpointIdx);
	}

	launchX += g_rotatedX;
	launchY += g_rotatedY;
	launchZ += g_rotatedZ;

	Mission_ResolveObjectOrMissionPointWorldLoc(targetRef, 0, 0, 0);
	targetX = worldlocx;
	targetY = worldlocy;
	targetZ = worldlocz;
	dx = targetX - launchX;
	dy = targetY - launchY;
	dz = targetZ - launchZ;

	{
		uint32_t rangeLimit;
		uint32_t rangeScore;

		rangeLimit = (uint32_t)g_modelDefs[modelIndex]
						 .laserGroupFireRange[g_curCraft->warheadData[launcherIdx].weaponGroupIdx];
		if (rangeLimit == 0) {
			rangeLimit = (g_curCraft->warheadData[launcherIdx].weaponType != 5) ? 81920u : 163840u;
		}
		rangeScore = (uint32_t)collide_roughdistance3d(dx, dy, dz);
		g_targetRangeScore = (int)rangeScore;
		if (rangeScore > rangeLimit) {
			return;
		}
		if (g_curCraft->warheadData[launcherIdx].weaponType == 5 && rangeScore < 0x4000u) {
			return;
		}
	}

	g_collisionProbeWorldY = targetY;
	g_collisionProbeWorldX = targetX;
	g_collisionSegmentStartWorldY = launchY;
	g_collisionSegmentStartWorldZ = launchZ;
	g_collisionSegmentStartWorldX = launchX;
	g_collisionProbeWorldZ = targetZ;
	g_collisionStagedModelProbe = 1;
	{
		int collisionBlocked;

		if (laser_ObjectTypeUsesExpandedTargetProbe(ownerObj->objectType)) {
			g_collideSweepRejectNearStartHits = 1;
			g_collideSweepSkipSsdMeshOrdinal = meshIdx;
			collisionBlocked = collide_CheckSweptModelCollision(ownerObjIdx, ownerObjIdx);
			g_collideSweepRejectNearStartHits = 0;
		} else {
			collisionBlocked = collide_CheckSweptModelCollision(ownerObjIdx, ownerObjIdx);
		}
		g_collisionStagedModelProbe = 0;
		if (collisionBlocked != 0) {
			return;
		}
	}

	projectileType = laser_SelectWarheadLauncherProjectileType(ownerObj, modelIndex, launcherIdx);
	if (g_objectTable[targetRef].mobj != NULL) {
		MobileObject* targetMobj;
		uint16_t leadFrames;

		targetMobj = g_objectTable[targetRef].mobj;
		if (targetMobj->speed != 0) {
			uint16_t projectileSpeed;
			uint16_t speedDenom;

			trig2_ctop(dx, dy, dz);
#ifdef XWA_MODERN
			projectileSpeed = g_projectileSpeedByType[projectileType - OBJ_LaserRebel];
#else
			projectileSpeed = g_projectileSpeedByObjectType_BiasedBase[projectileType];
#endif
			speedDenom = (uint16_t)(projectileSpeed / 5 + 18 * projectileSpeed);
			speedDenom = (uint16_t)(speedDenom / (uint16_t)g_simStepScale);
			if (speedDenom == 0) {
				speedDenom = 19;
			}
			leadFrames = (uint16_t)((int)trig2_polardistance / speedDenom);
		} else {
			leadFrames = 0;
		}

		{
			uint16_t leadError;
			uint8_t groupAi;

			groupAi = g_missionFlightGroups[ownerObj->flightGroupIdx].fg.groupAI;
			leadError = GameRandRange(g_warheadLeadErrorModulusByAiLevel[groupAi]);
			if ((GameRand() & 1) != 0) {
				leadFrames = (uint16_t)(leadFrames + leadError);
			} else {
				leadFrames = (uint16_t)(leadFrames - leadError);
				if (leadFrames >= 0x8000u) {
					leadFrames = 0;
				}
			}
		}

		targetMobj = g_objectTable[targetRef].mobj;
		targetX += leadFrames * (targetX - targetMobj->prevWorldX);
		targetY += leadFrames * (targetY - targetMobj->prevWorldY);
		targetZ += leadFrames * (targetZ - targetMobj->prevWorldZ);
	}

	trig2_ctop(targetX - launchX, targetY - launchY, targetZ - launchZ);
	pitch = (int16_t)targetPitch;
	yaw = trig2_xyangle;
	laser_ApplyWarheadLauncherAimError(ownerObj, &g_objectTable[targetRef], &pitch, &yaw);

	projectileObjIdx = Object_AllocSlotForGenus(GENUS_NpcProjectile);
	if (projectileObjIdx == 0xffffu) {
		return;
	}

	g_objectTable[projectileObjIdx].objectType = projectileType;
	g_objectTable[projectileObjIdx].genusId = GENUS_NpcProjectile;
	g_objectTable[projectileObjIdx].yaw = yaw;
	g_objectTable[projectileObjIdx].pitch = (uint16_t)pitch;
	g_objectTable[projectileObjIdx].roll = 0;
	g_objectTable[projectileObjIdx].angleD = 0;
	if (g_flightPlayerCount > 1 || g_filmRecording || g_filmPlaybackMode) {
		g_objectTable[projectileObjIdx].objectSignature = 1;
	}

	memset(g_objectTable[projectileObjIdx].mobj, 0, offsetof(MobileObject, pWarheadGuidance));
	g_objectTable[projectileObjIdx].mobj->state = 1;
	g_objectTable[projectileObjIdx].mobj->iff = ownerObj->mobj->iff;
	g_objectTable[projectileObjIdx].mobj->framesAlive = 1;
	g_objectTable[projectileObjIdx].mobj->sourceObjIdx = (int16_t)ownerObjIdx;
	g_objectTable[projectileObjIdx].mobj->sourceObjectType = ownerObj->objectType;
	g_objectTable[projectileObjIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[projectileObjIdx].mobj->moveVectorDirty = 1;
	g_objectTable[projectileObjIdx].mobj->speed = g_projectileSpeedByType[projectileType - OBJ_LaserRebel];
	g_objectTable[projectileObjIdx].mobj->damageAmount =
		g_projectileDamageByType[projectileType - OBJ_LaserRebel];

	{
		uint16_t speedTicks;
		uint16_t wholeSeconds;
		uint16_t frac;

		speedTicks = (uint16_t)(g_projectileSpeedByType[projectileType - OBJ_LaserRebel] / 5 +
								18u * g_projectileSpeedByType[projectileType - OBJ_LaserRebel]);
		wholeSeconds = (uint16_t)((int)trig2_polardistance / speedTicks);
		frac = (uint16_t)((int)trig2_polardistance % speedTicks);
		frac = MATH2_divide(frac, speedTicks);
		g_objectTable[projectileObjIdx].mobj->lifetimeTimer = 236 * wholeSeconds;
		projectileMobj = g_objectTable[projectileObjIdx].mobj;
		projectileMobj->lifetimeTimer += (uint16_t)MATH2_fraction(frac, 236u);
		g_objectTable[projectileObjIdx].mobj->lifetimeTimer += 59;
		projectileMobj = g_objectTable[projectileObjIdx].mobj;
		projectileMobj->lifetimeTimer += GameRand() & 0x1f;
	}

	FVIEW_calcrotatemove(pitch, yaw, &g_objectTable[projectileObjIdx]);
	projectileMobj->prevWorldX = launchX;
	projectileMobj->prevWorldY = launchY;
	projectileMobj->prevWorldZ = launchZ;
	{
		int offsetX;
		int offsetY;
		int offsetZ;

		offsetX = Xwa_Q15MulReuseFirstSlot(g_fviewMoveX_Q15,
										   g_projectileLaunchOffsetByType[projectileType - OBJ_LaserRebel]);
		offsetY = Xwa_Q15MulReuseFirstSlot(g_fviewMoveY_Q15,
										   g_projectileLaunchOffsetByType[projectileType - OBJ_LaserRebel]);
		offsetZ = Xwa_Q15MulReuseFirstSlot(g_fviewMoveZ_Q15,
										   g_projectileLaunchOffsetByType[projectileType - OBJ_LaserRebel]);
		g_objectTable[projectileObjIdx].world_x = launchX + offsetX;
		g_objectTable[projectileObjIdx].world_y = launchY + offsetY;
		g_objectTable[projectileObjIdx].world_z = launchZ + offsetZ;
	}

	{
		WarheadGuidanceState* guidance;
		uint16_t targetSignature;

		guidance = g_objectTable[projectileObjIdx].mobj->pWarheadGuidance;
		guidance->sourcePlayerIdx = (int8_t)ownerObj->playerOwnerIdx;
		guidance->homingTier = 0;
		guidance->targetComponentIdx = 0xffffu;
		guidance->targetObjIdx = targetRef;
		if (targetRef == 0xffffu || targetRef >= 0x8000u) {
			targetSignature = 0;
		} else {
			targetSignature = g_objectTable[targetRef].objectSignature;
		}
		guidance->targetSignature = targetSignature;
		guidance->minSpeed = g_objectTable[projectileObjIdx].mobj->speed;
	}

	g_curCraft->warheadData[launcherIdx].lastFireMeshIdx = (uint8_t)meshIdx;
	g_curCraft->warheadData[launcherIdx].lastFireHardpointIdx = hardpointIdx;
	fsfx_triggerweaponsfx(projectileObjIdx, g_localPlayer);
}

static __inline void laser_GetTurretProjectileHardpoint(ModelIndex firerModelIndex, int turretSeatIdx,
														int playerIdx, int16_t* outSide, int16_t* outUp,
														int16_t* outFwd) {
	uint16_t turretModelType;
	uint16_t meshCount;
	OptRotationScale* gunTurretRotScale;
	OptRotationScale* launcherRotScale;
	OptRotationScale* beamRotScale;
	ModelIndex turretModelIndex;
	ModelDef* turretModelDef;
	uint16_t hardpointSlot;
	uint16_t meshIdx;

	gunTurretRotScale = NULL;
	launcherRotScale = NULL;
	beamRotScale = NULL;
	turretModelType = g_modelDefs[firerModelIndex].turretModelIndex[turretSeatIdx];
	meshCount = (uint16_t)ModelMesh_GetObjectTypeMeshCount(turretModelType);
	for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
		MeshType meshType;

		meshType = ModelMesh_GetObjectTypeMeshType(turretModelType, meshIdx);
		switch (meshType) {
			case MESH_RotaryGunTurret:
				gunTurretRotScale = ModelMesh_GetRotScaleData(turretModelType, meshIdx);
				break;
			case MESH_RotaryLauncher:
				launcherRotScale = ModelMesh_GetRotScaleData(turretModelType, meshIdx);
				break;
			case MESH_RotaryBeamSystem:
				beamRotScale = ModelMesh_GetRotScaleData(turretModelType, meshIdx);
				break;
		}
	}

	++g_players[playerIdx].gunnerHardpointToggle;
	if (g_players[playerIdx].gunnerHardpointToggle > 1u) {
		g_players[playerIdx].gunnerHardpointToggle = 0;
	}
	hardpointSlot = g_players[playerIdx].gunnerHardpointToggle;
	turretModelIndex = (ModelIndex)GetModelIndexFromType((ObjectTypeId)turretModelType);
	turretModelDef = &g_modelDefs[turretModelIndex];
	*outSide = turretModelDef->weaponHardpoints[hardpointSlot].x;
	*outUp = turretModelDef->weaponHardpoints[hardpointSlot].z;
	*outFwd = turretModelDef->weaponHardpoints[hardpointSlot].y;

	if (beamRotScale != NULL && launcherRotScale != NULL) {
		CraftData* turretCraft;
		Matrix3x3 out;
		float axisAngle[4];
		Vec3f vec;

		turretCraft = g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft;
		vec.y = (float)-*outFwd;
		vec.z = (float)*outUp;
		vec.x = (float)*outSide;
		vec.x -= gunTurretRotScale->pivot.x;
		vec.y -= gunTurretRotScale->pivot.y;
		vec.z -= gunTurretRotScale->pivot.z;
		axisAngle[0] = gunTurretRotScale->rotationAxis.x * 0.000030517578f;
		axisAngle[1] = gunTurretRotScale->rotationAxis.y * 0.000030517578f;
		axisAngle[2] = gunTurretRotScale->rotationAxis.z * 0.000030517578f;
		axisAngle[3] = (float)(-(int16_t)turretCraft->turretAim.aimAngleA[turretSeatIdx] * 0.000095873722f);
		Math3D_BuildAxisAngleMatrix(&out, axisAngle);
		Math3D_RotateVec3(&vec, &out);

		vec.x += gunTurretRotScale->pivot.x;
		vec.y += gunTurretRotScale->pivot.y;
		vec.z += gunTurretRotScale->pivot.z;
		vec.x -= beamRotScale->pivot.x;
		vec.y -= beamRotScale->pivot.y;
		vec.z -= beamRotScale->pivot.z;
		axisAngle[0] = beamRotScale->rotationAxis.x * 0.000030517578f;
		axisAngle[1] = beamRotScale->rotationAxis.y * 0.000030517578f;
		axisAngle[2] = beamRotScale->rotationAxis.z * 0.000030517578f;
		axisAngle[3] = (float)((int16_t)turretCraft->turretAim.aimAngleB[turretSeatIdx] * 0.000095873722f);
		Math3D_BuildAxisAngleMatrix(&out, axisAngle);
		Math3D_RotateVec3(&vec, &out);

		vec.x = vec.x + beamRotScale->pivot.x;
		vec.y = vec.y + beamRotScale->pivot.y;
		vec.z = vec.z + beamRotScale->pivot.z;
		if (turretSeatIdx == 1) {
			Matrix3x3 mirror = { { 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f } };

			vec.x -= g_players[playerIdx].hardpointLocalX;
			vec.y -= g_players[playerIdx].hardpointLocalY;
			vec.z -= g_players[playerIdx].hardpointLocalZ;
			Math3D_RotateVec3(&vec, &mirror);
			vec.x += g_players[playerIdx].hardpointLocalX;
			vec.y += g_players[playerIdx].hardpointLocalY;
			vec.z += g_players[playerIdx].hardpointLocalZ;
		}

		*outSide = (int16_t)(int)vec.x;
		*outFwd = (int16_t)(int)-vec.y;
		*outUp = (int16_t)(int)vec.z;
	}
}

// FUNCTION: XWA 0x491EB0
int laser_createprojectile(unsigned int firerObjIdx, int weaponSlotIdx, ObjectTypeId projectileType,
						   int playerIdx) {
	int projectileGenus;
	uint16_t projectileObjIdx;
	ObjectRecord* firerObj;
	CraftData* firerCraft;
	WarheadGuidanceState* guidance;
	ModelIndex firerModelIndex;
	uint16_t firerObjectType;
	int turretSeatIdx;
	int16_t localSide;
	int16_t localUp;
	int16_t localFwd;
	int spawnX;
	int spawnY;
	int spawnZ;
	unsigned int projectileTypeIdx;

	if (playerIdx != -1) {
		uint16_t slotEnd;

		projectileGenus = GENUS_PlayerProjectile;
		projectileObjIdx =
			(uint16_t)(g_objectSlotRangeByGenus[GENUS_PlayerProjectile].next + 12u * (uint32_t)playerIdx);
		slotEnd = (uint16_t)(projectileObjIdx + 12u);
		if (g_projectileWarheadClassByType[projectileType - OBJ_LaserRebel] != 0) {
			projectileObjIdx = (uint16_t)(projectileObjIdx + 8u);
		}

		while (projectileObjIdx < slotEnd) {
			if (g_objectTable[projectileObjIdx].objectType == OBJ_None) {
				g_objectTable[projectileObjIdx].mobj->sourceObjIdx = -1;
				g_objectTable[projectileObjIdx].mobj->instanceExtent = 0;
				break;
			}
			++projectileObjIdx;
		}
		if (projectileObjIdx >= slotEnd) {
			projectileObjIdx = (uint16_t)(g_objectSlotRangeByGenus[GENUS_PlayerProjectile].next +
										  g_playerProjectileSlotsTotal);
			slotEnd = (uint16_t)(projectileObjIdx + g_sharedPlayerProjectileSlotsPerRegion);
			if (projectileObjIdx >= slotEnd) {
				return 0xffff;
			}
			while (projectileObjIdx < slotEnd) {
				if (g_objectTable[projectileObjIdx].objectType == OBJ_None) {
					g_objectTable[projectileObjIdx].mobj->sourceObjIdx = -1;
					g_objectTable[projectileObjIdx].mobj->instanceExtent = 0;
					break;
				}
				++projectileObjIdx;
			}
			if (projectileObjIdx >= slotEnd) {
				return 0xffff;
			}
		}

		collide_ResetObjectProximityForSlot(projectileObjIdx);
		if (g_flightPlayerCount > 1 || g_filmRecording || g_filmPlaybackMode) {
			g_objectTable[projectileObjIdx].objectSignature = 1;
		}
	} else {
		projectileGenus = GENUS_NpcProjectile;
		projectileObjIdx = Object_AllocSlotForGenus(GENUS_NpcProjectile);
	}
	if (projectileObjIdx != 0xffffu) {

		memset(g_objectTable[projectileObjIdx].mobj, 0, offsetof(MobileObject, pWarheadGuidance));
		g_objectTable[projectileObjIdx].mobj->sourceObjIdx = -1;
		g_objectTable[projectileObjIdx].mobj->instanceExtent = 0;

		firerObj = &g_objectTable[firerObjIdx];
		firerObjectType = firerObj->objectType;

		projectileTypeIdx = (unsigned int)(projectileType - OBJ_LaserRebel);
		g_objectTable[projectileObjIdx].mobj->state = 1;
		g_objectTable[projectileObjIdx].genusId = projectileGenus;
		g_objectTable[projectileObjIdx].objectType = projectileType;
		g_objectTable[projectileObjIdx].regionIdx = (uint8_t)regionIdx;
		g_objectTable[projectileObjIdx].typeSpecificWord = 0;
		g_objectTable[projectileObjIdx].mobj->framesAlive = 1;
		g_objectTable[projectileObjIdx].mobj->sourceObjIdx = (int16_t)firerObjIdx;
		g_objectTable[projectileObjIdx].mobj->sourceObjectType = firerObjectType;
		firerModelIndex = (ModelIndex)GetModelIndexFromType(firerObjectType);
		g_objectTable[projectileObjIdx].mobj->iff = firerObj->mobj->iff;
#ifdef XWA_MODERN
		if (playerIdx != -1 && g_players[playerIdx].objectIndex == (int)firerObjIdx) {
#else
		if (g_players[playerIdx].objectIndex == (int)firerObjIdx) {
#endif
			turretSeatIdx = g_players[playerIdx].currentSeatIdx - 1;
		} else {
			turretSeatIdx = -1;
		}

		if (turretSeatIdx == -1) {
			g_objectTable[projectileObjIdx].pitch = firerObj->pitch;
			g_objectTable[projectileObjIdx].roll = firerObj->roll;
			g_objectTable[projectileObjIdx].yaw = firerObj->yaw;
			g_objectTable[projectileObjIdx].angleD = 0;
		} else {
			CraftData* turretCraft;

			turretCraft = g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft;
			g_objectTable[projectileObjIdx].pitch = turretCraft->turretAim.aimAngleA[turretSeatIdx];
			g_objectTable[projectileObjIdx].roll = firerObj->roll;
			g_objectTable[projectileObjIdx].angleD = 0;
			g_objectTable[projectileObjIdx].yaw = turretCraft->turretAim.aimAngleB[turretSeatIdx];
		}

		guidance = g_objectTable[projectileObjIdx].mobj->pWarheadGuidance;
		if (g_objectTable[projectileObjIdx].objectType == OBJ_WarheadSpaceBomb) {
			g_objectTable[projectileObjIdx].mobj->speed = firerObj->mobj->speed;
			guidance->minSpeed = (uint16_t)(g_objectTable[projectileObjIdx].mobj->speed +
											g_projectileSpeedByType[projectileTypeIdx]);
		} else {
			g_objectTable[projectileObjIdx].mobj->speed =
				(uint16_t)(firerObj->mobj->speed + g_projectileSpeedByType[projectileTypeIdx]);
			guidance->minSpeed = g_objectTable[projectileObjIdx].mobj->speed;
		}

		if (firerObj->genusId != GENUS_PilotDroid || g_projectileWarheadClassByType[projectileTypeIdx]) {
			g_objectTable[projectileObjIdx].mobj->damageAmount =
				g_projectileDamageByType[projectileTypeIdx] + firerObj->mobj->speed;
			if ((uint32_t)g_objectTable[projectileObjIdx].mobj->damageAmount <
				(uint32_t)g_projectileDamageByType[projectileTypeIdx]) {
				g_objectTable[projectileObjIdx].mobj->damageAmount =
					g_projectileDamageByType[projectileTypeIdx];
			}
		} else {
			g_objectTable[projectileObjIdx].mobj->damageAmount =
				(uint32_t)g_projectileDamageByType[projectileTypeIdx] >> 2;
		}
		{
			uint16_t lifetimeTimer;

			lifetimeTimer = (uint16_t)(236u * g_projectileLifetimeSecondsByObjectType[projectileType]);
			lifetimeTimer =
				(uint16_t)(lifetimeTimer +
						   MATH2_fraction(g_projectileLifetimeFracQ16ByObjectType[projectileType], 236u));
			g_objectTable[projectileObjIdx].mobj->lifetimeTimer = lifetimeTimer;
		}

		spawnX = firerObj->world_x;
		spawnY = firerObj->world_y;
		spawnZ = firerObj->world_z;
		if (turretSeatIdx == -1) {
			localSide = g_modelDefs[firerModelIndex].weaponHardpoints[(uint16_t)weaponSlotIdx].x;
			localUp = g_modelDefs[firerModelIndex].weaponHardpoints[(uint16_t)weaponSlotIdx].z;
			localFwd = g_modelDefs[firerModelIndex].weaponHardpoints[(uint16_t)weaponSlotIdx].y;
		} else {
			laser_GetTurretProjectileHardpoint(firerModelIndex, turretSeatIdx, playerIdx, &localSide,
											   &localUp, &localFwd);
		}
		pai_calcrotatedpoint(firerObj, localSide, localUp, localFwd);
		if (firerObjectType == OBJ_ImperialStarDestroyer2) {
			g_rotatedX *= 2;
			g_rotatedY *= 2;
			g_rotatedZ *= 2;
		}
		spawnX += g_rotatedX;
		spawnY += g_rotatedY;
		spawnZ += g_rotatedZ;
		g_objectTable[projectileObjIdx].mobj->prevWorldX = spawnX;
		g_objectTable[projectileObjIdx].mobj->prevWorldY = spawnY;
		g_objectTable[projectileObjIdx].mobj->prevWorldZ = spawnZ;
		if (playerIdx != -1) {
			g_objectTable[projectileObjIdx].mobj->simStateTimestamp = g_players[playerIdx].lockstepTimestamp;
		}

		if (g_projectileWarheadClassByType[projectileTypeIdx] &&
			(firerObj->genusId == GENUS_Starship || firerObj->genusId == GENUS_Freighter ||
			 firerObj->genusId == GENUS_Container || firerObj->genusId == GENUS_Platform)) {
			int16_t muzzleOffset;

			muzzleOffset = g_projectileLaunchOffsetByType[projectileTypeIdx];
			if (localUp < 0) {
				g_objectTable[projectileObjIdx].pitch = 0x8000u;
				spawnZ -= muzzleOffset;
			} else {
				g_objectTable[projectileObjIdx].pitch = 0;
				spawnZ += muzzleOffset;
			}
			g_objectTable[projectileObjIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[projectileObjIdx].mobj->moveVectorDirty = 1;
			g_objectTable[projectileObjIdx].world_x = spawnX;
			g_objectTable[projectileObjIdx].world_y = spawnY;
			g_objectTable[projectileObjIdx].world_z = spawnZ;
		} else {
			uint8_t useConvergedAim;
			int convergeDistance;
			int16_t muzzleOffset;

			firerCraft = firerObj->mobj->pCraft;
			if (firerCraft->laserConvergeLevel != 0 &&
				(firerObj->genusId == GENUS_Fighter || firerObj->objectType == OBJ_EscortShuttle)) {
				if (firerCraft->laserConvergeLevel == 4) {
					if (playerIdx != -1) {
						uint16_t currentTargetObjectIdx;

						currentTargetObjectIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
						if (currentTargetObjectIdx != 0xffffu) {
							ObjectRecord* targetObj;

							targetObj = &g_objectTable[currentTargetObjectIdx];
							if (targetObj->genusId == GENUS_Freighter ||
								targetObj->genusId == GENUS_Starship) {
								Object_DirectionAndDistanceToMeshCenter(
									(uint16_t)firerObjIdx, currentTargetObjectIdx,
									(uint16_t)g_players[playerIdx].selectedTargetComponent);
							} else {
								pai_ObjectRefDirectionToObjectRef(currentTargetObjectIdx, firerObjIdx);
							}
							convergeDistance = trig2_polardistance;
							if (convergeDistance < 4096) {
								convergeDistance = 4096;
							}
							if (convergeDistance > 0x10000) {
								convergeDistance = 0x10000;
							}
							useConvergedAim = 1;
							if (g_provingGroundsModeActive && (targetObj->objectType == OBJ_AccelRing ||
															   targetObj->objectType == OBJ_R2D2)) {
								useConvergedAim = 0;
							}
						} else {
							convergeDistance = projectileType;
							useConvergedAim = 0;
						}
					} else {
						AiController* ai;

						ai = pai_GetEffectiveAIController(firerCraft);
						if (ai->targetObjIdx != 0xffffu) {
							trig2_ctop(g_objectTable[firerObjIdx].world_x - ai->aimPointX,
									   g_objectTable[firerObjIdx].world_y - ai->aimPointY,
									   g_objectTable[firerObjIdx].world_z - ai->aimPointZ);
							convergeDistance = trig2_polardistance;
							if (convergeDistance < 4096) {
								convergeDistance = 4096;
							}
							if (convergeDistance > 0x10000) {
								convergeDistance = 0x10000;
							}
							useConvergedAim = 1;
						} else {
							convergeDistance = projectileType;
							useConvergedAim = 0;
						}
					}
				} else {
					convergeDistance =
						g_laserConvergenceDistanceByLevel_BiasedBase[firerCraft->laserConvergeLevel];
					useConvergedAim = 1;
				}
			} else {
				convergeDistance = projectileType;
				useConvergedAim = 0;
			}

			if (useConvergedAim) {
				int aimTargetX;
				int aimTargetY;
				int aimTargetZ;
				float deltaX;
				float deltaY;
				float deltaZ;
				float invLen;
				int16_t moveX;
				int16_t moveY;
				int16_t moveZ;

				aimTargetX = firerObj->world_x + Xwa_Q15Mul(convergeDistance, firerObj->mobj->cachedFwdX);
				aimTargetY = firerObj->world_y + Xwa_Q15Mul(convergeDistance, firerObj->mobj->cachedFwdY);
				aimTargetZ = firerObj->world_z + Xwa_Q15Mul(convergeDistance, firerObj->mobj->cachedFwdZ);
				deltaX = (double)(aimTargetX - spawnX);
				deltaY = (double)(aimTargetY - spawnY);
				deltaZ = (double)(aimTargetZ - spawnZ);
				invLen = 1.0 / sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
				moveX = (int16_t)(int)(invLen * deltaX * 32768.0);
				moveY = (int16_t)(int)(invLen * deltaY * 32768.0);
				moveZ = (int16_t)(int)(invLen * deltaZ * 32768.0);
				g_objectTable[projectileObjIdx].mobj->moveX = moveX;
				g_objectTable[projectileObjIdx].mobj->moveY = moveY;
				g_objectTable[projectileObjIdx].mobj->moveZ = moveZ;
				trig2_ctop(moveX, moveY, moveZ);
				g_objectTable[projectileObjIdx].yaw = trig2_xyangle;
				g_objectTable[projectileObjIdx].pitch = targetPitch;
				g_objectTable[projectileObjIdx].roll = 0;
				g_objectTable[projectileObjIdx].angleD = 0;
				g_objectTable[projectileObjIdx].mobj->moveVectorDirty = 1;
				g_objectTable[projectileObjIdx].mobj->orientMatrixDirty = 1;
			} else if (turretSeatIdx == -1) {
				g_objectTable[projectileObjIdx].mobj->moveX = firerObj->mobj->moveX;
				g_objectTable[projectileObjIdx].mobj->moveY = firerObj->mobj->moveY;
				g_objectTable[projectileObjIdx].mobj->moveZ = firerObj->mobj->moveZ;
				g_objectTable[projectileObjIdx].mobj->cachedSideX = firerObj->mobj->cachedSideX;
				g_objectTable[projectileObjIdx].mobj->cachedSideY = firerObj->mobj->cachedSideY;
				g_objectTable[projectileObjIdx].mobj->cachedSideZ = firerObj->mobj->cachedSideZ;
				g_objectTable[projectileObjIdx].mobj->cachedUpX = firerObj->mobj->cachedUpX;
				g_objectTable[projectileObjIdx].mobj->cachedUpY = firerObj->mobj->cachedUpY;
				g_objectTable[projectileObjIdx].mobj->cachedUpZ = firerObj->mobj->cachedUpZ;
				g_objectTable[projectileObjIdx].mobj->cachedFwdX =
					g_objectTable[projectileObjIdx].mobj->moveX;
				g_objectTable[projectileObjIdx].mobj->cachedFwdY =
					g_objectTable[projectileObjIdx].mobj->moveY;
				g_objectTable[projectileObjIdx].mobj->cachedFwdZ =
					g_objectTable[projectileObjIdx].mobj->moveZ;
				g_objectTable[projectileObjIdx].mobj->orientMatrixDirty = 0;
				g_objectTable[projectileObjIdx].mobj->moveVectorDirty = 0;
			} else {
				g_objectTable[projectileObjIdx].mobj->moveX = g_players[playerIdx].turretCamMat[0];
				g_objectTable[projectileObjIdx].mobj->moveY = g_players[playerIdx].turretCamMat[1];
				g_objectTable[projectileObjIdx].mobj->moveZ = g_players[playerIdx].turretCamMat[2];
				trig2_ctop(g_players[playerIdx].turretCamMat[0], g_players[playerIdx].turretCamMat[1],
						   g_players[playerIdx].turretCamMat[2]);
				g_objectTable[projectileObjIdx].yaw = trig2_xyangle;
				g_objectTable[projectileObjIdx].pitch = targetPitch;
				g_objectTable[projectileObjIdx].roll = 0;
				g_objectTable[projectileObjIdx].angleD = 0;
				g_objectTable[projectileObjIdx].mobj->orientMatrixDirty = 1;
				g_objectTable[projectileObjIdx].mobj->moveVectorDirty = 1;
			}

			{
				int offsetX;
				int offsetY;
				int offsetZ;

				muzzleOffset = g_projectileLaunchOffsetByType[projectileTypeIdx];
				offsetX = Xwa_Q15MulReuseFirstSlot(muzzleOffset, g_objectTable[projectileObjIdx].mobj->moveX);
				offsetY = Xwa_Q15MulReuseFirstSlot(muzzleOffset, g_objectTable[projectileObjIdx].mobj->moveY);
				offsetZ = Xwa_Q15MulReuseFirstSlot(muzzleOffset, g_objectTable[projectileObjIdx].mobj->moveZ);
				g_objectTable[projectileObjIdx].world_x = spawnX + offsetX;
				g_objectTable[projectileObjIdx].world_y = spawnY + offsetY;
				g_objectTable[projectileObjIdx].world_z = spawnZ + offsetZ;
			}
		}

		guidance->homingTier = 0;
		guidance->targetObjIdx = 0xffffu;
		guidance->targetSignature = 0;
		guidance->targetComponentIdx = 0xffffu;
		guidance->sourcePlayerIdx = -1;

		if (g_useHardware3D && g_projectileWarheadClassByType[projectileTypeIdx] &&
			g_players[g_localPlayer].regionIndex == g_objectTable[projectileObjIdx].regionIdx) {
			ObjectTrail_CreateEmitter(projectileObjIdx, projectileType);
		}
		if (projectileType == OBJ_WarheadSpaceBomb) {
			ObjectRecord* projectileObj;

			projectileObj = &g_objectTable[projectileObjIdx];
			projectileObj->mobj->velocityOverrideDirX = (int16_t)-firerObj->mobj->cachedUpX;
			projectileObj->mobj->velocityOverrideDirY = (int16_t)-firerObj->mobj->cachedUpY;
			projectileObj->mobj->velocityOverrideDirZ = (int16_t)-firerObj->mobj->cachedUpZ;
			projectileObj->mobj->velocityOverrideSpeed =
				(uint16_t)(g_projectileSpeedByType[OBJ_WarheadSpaceBomb - OBJ_LaserRebel] >> 2);
			projectileObj->mobj->velocityOverrideElapsed = 0;
			projectileObj->mobj->velocityOverrideActive = 1;
		}

		return projectileObjIdx;
	}
	return 0xffff;
}

// FUNCTION: XWA 0x491B90
int laser_firemissile(int firerObjIdx, int warheadSlot, int projectileType, unsigned int fireMode) {
	int playerOwnerIdx;
	AiController* effectiveAi;
	int projectileObjIdx;
	WarheadGuidanceState* guidance;

	playerOwnerIdx = g_objectTable[firerObjIdx].playerOwnerIdx;
	effectiveAi = pai_GetEffectiveAIController(g_curCraft);
	projectileObjIdx = 0xffff;
	if (g_curCraft->warheadData[warheadSlot].weaponType == 3 &&
		g_curCraft->warheadData[warheadSlot].count > 0) {
		projectileObjIdx = laser_createprojectile((unsigned int)firerObjIdx, warheadSlot,
												  (ObjectTypeId)projectileType, playerOwnerIdx);
		if (projectileObjIdx != 0xffff) {

			++g_curCraft->warheadsFiredCount;
			if ((unsigned int)playerOwnerIdx != 0xffffffffu) {
				++g_players[playerOwnerIdx].warheadsFired;
			}
			fsfx_triggerweaponsfx((unsigned int)projectileObjIdx, (unsigned int)playerOwnerIdx);
			if (g_missionFlightGroups[g_objectTable[firerObjIdx].flightGroupIdx].fg.status1 != 21 &&
				g_missionFlightGroups[g_objectTable[firerObjIdx].flightGroupIdx].fg.status2 != 21) {
				--g_curCraft->warheadData[warheadSlot].count;
			}

			guidance = g_objectTable[projectileObjIdx].mobj->pWarheadGuidance;
			if (fireMode < 2u) {
				int homingTier;

				homingTier = (int16_t)g_curCraft->warheadLockTicks / 236;
				guidance->homingTier = (uint8_t)homingTier;
				if (guidance->homingTier > 6u) {
					guidance->homingTier = 6;
				}
			}

			if (playerOwnerIdx != -1) {
				guidance->targetObjIdx = g_players[playerOwnerIdx].currentTargetObjectIdx;
				guidance->targetComponentIdx = (uint16_t)g_players[playerOwnerIdx].selectedTargetComponent;
				if (g_players[playerOwnerIdx].currentTargetObjectIdx != 0xffffu) {
					guidance->targetSignature =
						g_objectTable[g_players[playerOwnerIdx].currentTargetObjectIdx].objectSignature;
				} else {
					guidance->targetSignature = 0;
				}
			} else {
				guidance->targetObjIdx = effectiveAi->targetObjIdx;
				guidance->targetComponentIdx = effectiveAi->targetComponent;
				if (effectiveAi->targetObjIdx != 0xffffu && effectiveAi->targetObjIdx < 0x8000u) {
					guidance->targetSignature = g_objectTable[effectiveAi->targetObjIdx].objectSignature;
				} else {
					guidance->targetSignature = 0;
				}
			}
			guidance->sourcePlayerIdx = (int8_t)playerOwnerIdx;

			if (guidance->targetObjIdx != 0xffffu) {
				if (g_objectTable[guidance->targetObjIdx].playerOwnerIdx != -1) {
					int targetPlayerIdx;

					targetPlayerIdx = g_objectTable[g_objectTable[(uint16_t)projectileObjIdx]
														.mobj->pWarheadGuidance->targetObjIdx]
										  .playerOwnerIdx;
					if (targetPlayerIdx != -1 && g_players[targetPlayerIdx].pendingActionId == 0) {
						g_players[targetPlayerIdx].pendingActionId = 1;
						g_players[targetPlayerIdx].pendingActionIssuerPlayerIdx = 0xffffu;
						g_players[targetPlayerIdx].pendingActionParam = (int16_t)projectileObjIdx;
						g_players[targetPlayerIdx].pendingActionTimer = 1416;
						if (targetPlayerIdx == g_localPlayer) {
							msg_emitInFlightMessage(MSG_MISSILE_WARNING, targetPlayerIdx);
							fsfx_speakorderack(g_localPlayer, (int16_t)0xffffu, 25, 0,
											   (unsigned int)g_players[g_localPlayer].objectIndex, 0xffffu);
							fsfx_PlaySound(65, 0xffffu, (unsigned int)g_localPlayer);
							g_incomingMissileWarningFlashActive = 1;
						}
					}
				}
			}

			if (fireMode < 2u) {
				unsigned int firstSlot;

				firstSlot = g_modelDefs[g_curCraft->modelIndex].warheadLauncherFirstSlot[fireMode];
				if (g_curCraft->warheadData[firstSlot].count >=
					g_curCraft->warheadData[firstSlot + 1].count) {
					g_curCraft->warheadLauncherFlags[fireMode] &= 0x7fu;
				} else {
					g_curCraft->warheadLauncherFlags[fireMode] |= 0x80u;
				}
			}
		}
	}

	return projectileObjIdx;
}

// FUNCTION: XWA 0x4912C0
void laser_firelasersystem(unsigned int shooterObjIdx, int laserGroupIdx, int playerIdx,
						   uint16_t updateFireDelay) {
	AiController* effectiveAi;
	ModelIndex modelIndex;
	uint16_t firstSlot;
	uint16_t lastSlot;
	int remainingShots;
	uint16_t firedCount;
	uint16_t projectileTypeId;
	uint16_t slotIdx;
	int16_t slotStep;
	int firingPlayerIdx;

	firstSlot = 0;
	lastSlot = 0;
	remainingShots = 0;
	slotStep = 0;
	g_curCraft = g_objectTable[shooterObjIdx].mobj->pCraft;
	effectiveAi = pai_GetEffectiveAIController(g_curCraft);
	modelIndex = g_curCraft->modelIndex;
	if (g_curCraft->sFoilState != 0) {
		if (playerIdx == g_localPlayer) {
			msg_emitInFlightMessage(MSG_SFOILS_NO_FIRE, g_localPlayer);
		}
		return;
	}

	firingPlayerIdx = playerIdx;
	firedCount = 0;
	switch (g_curCraft->laserLinkMode[laserGroupIdx]) {
		case 3:
			slotStep = 1;
			firstSlot = g_modelDefs[modelIndex].laserGroupFirstSlot[laserGroupIdx];
			lastSlot = g_modelDefs[modelIndex].laserGroupLastSlot[laserGroupIdx];
			remainingShots = (uint16_t)(lastSlot - firstSlot + 1);
			break;
		case 0:
		case 1: {
			uint8_t groupFirst;
			uint8_t groupLast;
			uint8_t curSlot;

			groupFirst = g_modelDefs[modelIndex].laserGroupFirstSlot[laserGroupIdx];
			groupLast = g_modelDefs[modelIndex].laserGroupLastSlot[laserGroupIdx];
			if (g_curCraft->laserLinkNextSlot[laserGroupIdx] < groupFirst ||
				g_curCraft->laserLinkNextSlot[laserGroupIdx] > groupLast) {
				g_curCraft->laserLinkNextSlot[laserGroupIdx] = groupFirst;
			}
			curSlot = g_curCraft->laserLinkNextSlot[laserGroupIdx];
			firstSlot = curSlot;
			lastSlot = curSlot;
			g_curCraft->laserLinkNextSlot[laserGroupIdx] = (uint8_t)(curSlot + 1u);
			if (g_curCraft->laserLinkNextSlot[laserGroupIdx] > groupLast) {
				g_curCraft->laserLinkNextSlot[laserGroupIdx] = groupFirst;
			}
			remainingShots = 1;
			slotStep = 1;
			if (firingPlayerIdx != -1 &&
				g_modelDefs[g_curCraft->modelIndex].laserGroupMountType[laserGroupIdx] == 4 &&
				g_modelDefs[g_curCraft->modelIndex].weaponHardpoints[firstSlot].z < -64) {
				if (g_curCraft->carriedObjectIndex != 0xffffu) {
					return;
				}
				if (g_players[firingPlayerIdx].inputDisabledFlag != 0 &&
					g_players[firingPlayerIdx].inputDisabledFlag != 5) {
					return;
				}
			}
			break;
		}
		case 2: {
			uint8_t groupFirst;
			uint8_t groupLast;
			uint8_t curSlot;

			groupFirst = g_modelDefs[modelIndex].laserGroupFirstSlot[laserGroupIdx];
			groupLast = g_modelDefs[modelIndex].laserGroupLastSlot[laserGroupIdx];
			if (g_curCraft->laserLinkNextSlot[laserGroupIdx] < groupFirst ||
				g_curCraft->laserLinkNextSlot[laserGroupIdx] > groupLast) {
				g_curCraft->laserLinkNextSlot[laserGroupIdx] = groupFirst;
			}
			curSlot = g_curCraft->laserLinkNextSlot[laserGroupIdx];
			firstSlot = curSlot;
			g_curCraft->laserLinkNextSlot[laserGroupIdx] = (uint8_t)(curSlot ^ 1u);
			if (g_curCraft->laserLinkNextSlot[laserGroupIdx] > groupLast) {
				g_curCraft->laserLinkNextSlot[laserGroupIdx] = groupFirst;
			}
			lastSlot = groupLast;
			slotStep = 2;
			remainingShots = (groupLast - groupFirst + 1) / 2;
			break;
		}
		default:
			break;
	}

	if (g_objectTable[shooterObjIdx].objectType == OBJ_CorellianTransportGunner) {
		if (g_players[firingPlayerIdx].gunnerHardpointToggle > 0) {
			++firstSlot;
		} else {
			--lastSlot;
		}
	}

	projectileTypeId = updateFireDelay;
	slotIdx = firstSlot;
	while (slotIdx <= lastSlot) {
		if (g_curCraft->warheadData[slotIdx].weaponType != 0) {
			int8_t laserCharge;

			laserCharge = (int8_t)g_curCraft->warheadData[slotIdx].laserCharge;
			if (laserCharge > 0) {
				unsigned int projectileObjIdx;

				projectileTypeId = g_curCraft->warheadData[slotIdx].projectileTypeId;
				if ((projectileTypeId == OBJ_LaserRebel || projectileTypeId == OBJ_LaserImperial ||
					 projectileTypeId == OBJ_LaserIon) &&
					laserCharge >= 64) {
					projectileTypeId = (uint16_t)(projectileTypeId + 1u);
				}
				projectileObjIdx = laser_createprojectile(shooterObjIdx, slotIdx,
														  (ObjectTypeId)projectileTypeId, firingPlayerIdx);
				if (projectileObjIdx != 0xffff) {
					WarheadGuidanceState* guidance;

					if (firingPlayerIdx == g_localPlayer) {
						ForceFeedback_PlayLaserFireEffect();
					}
					if (g_missionFlightGroups[g_objectTable[shooterObjIdx].flightGroupIdx].fg.status1 != 21 &&
						g_missionFlightGroups[g_objectTable[shooterObjIdx].flightGroupIdx].fg.status2 != 21) {
						if (firingPlayerIdx != -1) {
							if (g_objectTable[shooterObjIdx].genusId) {
								if (g_curCraft->warheadData[slotIdx].weaponType < 4u) {
									g_curCraft->warheadData[slotIdx].laserCharge =
										(uint8_t)(g_curCraft->warheadData[slotIdx].laserCharge - 3u);
								}
							} else if (modelIndex == GetModelIndexFromType(OBJ_TIEFighter) ||
									   modelIndex == GetModelIndexFromType(OBJ_TIEBomber)) {
								g_curCraft->warheadData[slotIdx].laserCharge =
									(uint8_t)(g_curCraft->warheadData[slotIdx].laserCharge - 3u);
							} else {
								g_curCraft->warheadData[slotIdx].laserCharge =
									(uint8_t)(g_curCraft->warheadData[slotIdx].laserCharge - 4u);
							}
						} else {
							--g_curCraft->warheadData[slotIdx].laserCharge;
						}
					}

					if ((uint16_t)remainingShots >= 2u) {
						if ((firedCount & 1u) == 0) {
							fsfx_triggerweaponsfx((unsigned int)projectileObjIdx,
												  (unsigned int)g_localPlayer);
						}
					} else if (firedCount < 2u) {
						fsfx_triggerweaponsfx((unsigned int)projectileObjIdx, (unsigned int)g_localPlayer);
					}
					if ((int8_t)g_curCraft->warheadData[slotIdx].laserCharge < 0) {
						g_curCraft->warheadData[slotIdx].laserCharge = 0;
					}

					guidance = g_objectTable[projectileObjIdx].mobj->pWarheadGuidance;
					if (firingPlayerIdx != -1) {
						guidance->targetObjIdx = (uint16_t)g_players[firingPlayerIdx].currentTargetObjectIdx;
						if (g_players[firingPlayerIdx].currentTargetObjectIdx != 0xffffu) {
							guidance->targetSignature =
								g_objectTable[(uint16_t)g_players[firingPlayerIdx].currentTargetObjectIdx]
									.objectSignature;
						} else {
							guidance->targetSignature = 0;
						}
					} else {
						guidance->targetObjIdx = effectiveAi->targetObjIdx;
						if (effectiveAi->targetObjIdx != 0xffffu && effectiveAi->targetObjIdx < 0x8000u) {
							guidance->targetSignature =
								g_objectTable[effectiveAi->targetObjIdx].objectSignature;
						} else {
							guidance->targetSignature = 0;
						}
					}
					guidance->sourcePlayerIdx = (int8_t)firingPlayerIdx;
					++firedCount;
				}
			}
		}

		--remainingShots;
		if ((uint16_t)remainingShots == 0) {
			break;
		}
		slotIdx = (uint16_t)(slotIdx + (uint16_t)slotStep);
	}

	if (projectileTypeId == OBJ_LaserIon || projectileTypeId == OBJ_LaserIonTurbo) {
		g_curCraft->ionShotsFiredCount = (uint16_t)(g_curCraft->ionShotsFiredCount + firedCount);
		if (firingPlayerIdx != -1) {
			uint16_t* statsShotsFired;

			statsShotsFired = &g_players[firingPlayerIdx].missionStats.ionShotsFired;
			*statsShotsFired = (uint16_t)(*statsShotsFired + firedCount);
		}
	} else {
		g_curCraft->laserShotsFiredCount = (uint16_t)(g_curCraft->laserShotsFiredCount + firedCount);
		if (firingPlayerIdx != -1) {
			uint16_t* statsShotsFired;

			statsShotsFired = &g_players[firingPlayerIdx].missionStats.laserShotsFired;
			*statsShotsFired = (uint16_t)(*statsShotsFired + firedCount);
		}
	}

	if ((uint8_t)updateFireDelay != 0) {
		if (firedCount == 0) {
			firedCount = 1;
		}
		if (firingPlayerIdx != -1) {
			g_curCraft->laserFireCooldownTicks[0] =
				(uint16_t)(g_curCraft->laserFireCooldownTicks[0] + 47 * firedCount + 2);
			g_curCraft->laserLastFireTimestamp[0] += 47 * firedCount + 2;
		} else {
			g_curCraft->laserFireCooldownTicks[laserGroupIdx] =
				(uint16_t)(g_curCraft->laserFireCooldownTicks[laserGroupIdx] + 47 * firedCount + 2);
			g_curCraft->laserLastFireTimestamp[laserGroupIdx] += 47 * firedCount + 2;
		}
	}
}

// FUNCTION: XWA 0x4918F0
void laser_firerocketsystem(int firerObjIdx, unsigned int fireMode) {
	uint16_t fireFlags;
	uint16_t firstSlot;
	uint16_t firedCount;
	uint16_t blockedByLoadedSlot;

	g_curCraft = g_objectTable[firerObjIdx].mobj->pCraft;
	fireFlags = g_curCraft->warheadLauncherFlags[fireMode];
	firstSlot = g_modelDefs[g_curCraft->modelIndex].warheadLauncherFirstSlot[fireMode];
	firedCount = 0;
	blockedByLoadedSlot = 0;

	if ((fireFlags & 0x7f) == 3) {
		if (laser_firemissile(firerObjIdx, firstSlot, g_curCraft->warheadSlotTypeIds[fireMode], fireMode) !=
			0xffff) {
			firedCount = 1;
		} else if (g_curCraft->warheadData[firstSlot].count > 0) {
			blockedByLoadedSlot = 1;
		}

		if (laser_firemissile(firerObjIdx, (uint16_t)(firstSlot + 1u),
							  g_curCraft->warheadSlotTypeIds[fireMode], fireMode) != 0xffff) {
			++firedCount;
		} else if (g_curCraft->warheadData[(uint16_t)(firstSlot + 1u)].count > 0) {
			blockedByLoadedSlot = 1;
		}
	} else if ((fireFlags & 0x80) != 0) {
		uint16_t secondSlot;

		secondSlot = (uint16_t)(firstSlot + 1u);
		if (laser_firemissile(firerObjIdx, secondSlot, g_curCraft->warheadSlotTypeIds[fireMode], fireMode) !=
			0xffff) {
			firedCount = 1;
		} else if (g_curCraft->warheadData[secondSlot].count > 0) {
			blockedByLoadedSlot = 1;
		}
	} else {
		if (laser_firemissile(firerObjIdx, firstSlot, g_curCraft->warheadSlotTypeIds[fireMode], fireMode) !=
			0xffff) {
			firedCount = 1;
		} else if (g_curCraft->warheadData[firstSlot].count > 0) {
			blockedByLoadedSlot = 1;
		}
	}

	if (g_objectTable[firerObjIdx].objectType == OBJ_TIEWarheads) {
		g_curCraft->warheadLauncherCooldownTicks[fireMode] =
			(uint16_t)(g_curCraft->warheadLauncherCooldownTicks[fireMode] + 118u);
	} else {
		g_curCraft->warheadLauncherCooldownTicks[fireMode] =
			(uint16_t)(g_curCraft->warheadLauncherCooldownTicks[fireMode] + 472u);
	}

	if (g_objectTable[firerObjIdx].playerOwnerIdx == g_localPlayer && !blockedByLoadedSlot) {
		uint16_t warheadKindIndex;

		warheadKindIndex = (uint16_t)ObjectType_GetWarheadKindIndex(
			(ObjectTypeId)g_curCraft->warheadSlotTypeIds[g_players[g_localPlayer].selectedWarhead]);
		if (firedCount == 0) {
			msg_emitInFlightMessage(warheadKindIndex + 45u, g_localPlayer);
			return;
		}

		ForceFeedback_PlayWarheadFireEffect();
		if (firedCount == 1) {
			msg_emitInFlightMessage(warheadKindIndex + 55u, g_localPlayer);
			return;
		}
		if (firedCount == 2) {
			msg_emitInFlightMessage(warheadKindIndex + 65u, g_localPlayer);
		}
	}
}

// FUNCTION: XWA 0x490EE0
void laser_fireplayerweapon(int playerIdx) {
	int objectIndex;
	CraftData* craft;
	int16_t fireCooldown;
	int lockstepTimestamp;
	int16_t fireGateTicks;
	int16_t currentSeatIdx;

	objectIndex = g_players[playerIdx].objectIndex;
	if (objectIndex == 0xffff) {
		return;
	}

	craft = g_objectTable[objectIndex].mobj->pCraft;
	if (craft->beamEffectAccum[2] != 0 && (craft->cmTypeId != 1 || craft->chaffActiveTimer == 0)) {
		msg_emitInFlightMessage(MSG_WEAPONS_JAMMED, playerIdx);
		return;
	}

	if (g_players[playerIdx].selectedWeaponMode == 0) {
		fireCooldown = (int16_t)craft->laserFireCooldownTicks[0];
		lockstepTimestamp = g_players[playerIdx].lockstepTimestamp;
		if (fireCooldown != 0) {
			if (lockstepTimestamp > (int)craft->laserLastFireTimestamp[0]) {
				fireCooldown = 0;
				craft->laserLastFireTimestamp[0] = lockstepTimestamp;
			} else {
				fireCooldown = (int16_t)(2 * g_elapsedTicks);
			}
		} else {
			craft->laserLastFireTimestamp[0] = lockstepTimestamp;
		}

		currentSeatIdx = g_players[playerIdx].currentSeatIdx;
		if (currentSeatIdx == 2) {
			if (craft->carriedObjectIndex != 0xffffu) {
				msg_emitInFlightMessage(MSG_GUNNER_BLOCKED, playerIdx);
				return;
			}
			if (g_players[playerIdx].inputDisabledFlag != 0 && g_players[playerIdx].inputDisabledFlag != 5) {
				msg_emitInFlightMessage(MSG_GUNNER_DISABLED, playerIdx);
				return;
			}
		}

		fireGateTicks = (int16_t)(g_elapsedTicks + ((uint16_t)g_elapsedTicks >> 1));
		if (fireCooldown >= fireGateTicks) {
			return;
		}

		if ((craft->workingSubsystems & 0x10u) != 0) {
			if (currentSeatIdx == 0) {
				if (craft->cannonClassCount != 0) {
					unsigned int selectedWarhead;

					selectedWarhead = g_players[playerIdx].selectedWarhead;
					if (craft->laserLinkMode[selectedWarhead] == 4) {
						uint8_t savedMode0;
						uint8_t savedMode1;

						savedMode0 = craft->laserLinkMode[0];
						savedMode1 = craft->laserLinkMode[1];
						craft->laserLinkMode[0] = 3;
						craft->laserLinkMode[1] = 3;
						laser_firelasersystem((unsigned int)g_players[playerIdx].objectIndex, 0, playerIdx,
											  1u);
						laser_firelasersystem((unsigned int)g_players[playerIdx].objectIndex, 1, playerIdx,
											  0);
						craft->laserLinkMode[0] = savedMode0;
						craft->laserLinkMode[1] = savedMode1;
					} else {
						laser_firelasersystem((unsigned int)g_players[playerIdx].objectIndex,
											  (int)selectedWarhead, playerIdx, 1u);
					}
				}
			} else {
				uint8_t* laserGroupMountType;
				int laserGroupIdx;
				int remainingGroups;

				laserGroupMountType =
					g_modelDefs[(uint16_t)GetModelIndexFromType(
									g_objectTable[g_players[playerIdx].objectIndex].objectType)]
						.laserGroupMountType;
				laserGroupIdx = 0;
				remainingGroups = 3;
				do {
					if (*laserGroupMountType == 4) {
						laser_firelasersystem((unsigned int)g_players[playerIdx].objectIndex, laserGroupIdx,
											  playerIdx, 1u);
					}
					++laserGroupIdx;
					++laserGroupMountType;
					--remainingGroups;
				} while (remainingGroups != 0);
			}
			if (g_players[playerIdx].turretAutoFireState == 0) {
				if (g_players[playerIdx].selectedWarhead == 0 ||
					craft->laserLinkMode[g_players[playerIdx].selectedWarhead] == 4) {
					uint8_t* laserGroupMountType;
					int laserGroupIdx;
					int remainingGroups;

					laserGroupMountType =
						g_modelDefs[(uint16_t)GetModelIndexFromType(
										g_objectTable[g_players[playerIdx].objectIndex].objectType)]
							.laserGroupMountType;
					laserGroupIdx = 0;
					remainingGroups = 3;
					do {
						if (*laserGroupMountType == 4) {
							laser_firelasersystem((unsigned int)g_players[playerIdx].objectIndex,
												  laserGroupIdx, playerIdx, 0);
						}
						++laserGroupIdx;
						++laserGroupMountType;
						--remainingGroups;
					} while (remainingGroups != 0);
				}
			}
			return;
		}

		if (playerIdx == g_localPlayer) {
			g_msgArgTable[0] = (uint16_t)(g_players[playerIdx].selectedWarhead + 99u);
			g_msgArgTable[1] = 94;
			msg_emitInFlightMessage(MSG_SYSTEMCOND, playerIdx);
		}
		return;
	}

	{
		unsigned int selectedWarhead;

		fireGateTicks = (int16_t)(g_elapsedTicks + ((uint16_t)g_elapsedTicks >> 1));
		selectedWarhead = g_players[playerIdx].selectedWarhead;
		if ((int16_t)craft->warheadLauncherCooldownTicks[selectedWarhead] >= fireGateTicks) {
			return;
		}

		if ((craft->workingSubsystems & 8u) != 0) {
			laser_firerocketsystem(objectIndex, selectedWarhead);
			craft = g_objectTable[objectIndex].mobj->pCraft;
			{
				unsigned int firstSlot;

				firstSlot = g_modelDefs[craft->modelIndex]
								.warheadLauncherFirstSlot[g_players[playerIdx].selectedWarhead];
				if (craft->warheadData[firstSlot + 1u].count + craft->warheadData[firstSlot].count == 0) {
					g_players[playerIdx].selectedWeaponMode = 0;
					g_players[playerIdx].selectedWarhead = 0;
					craft->laserFireCooldownTicks[0] = 118;
					craft->laserLastFireTimestamp[0] =
						(unsigned int)(g_players[playerIdx].lockstepTimestamp + 118);
				}
			}
			return;
		}
	}

	if (playerIdx == g_localPlayer) {
		g_msgArgTable[0] = 101;
		g_msgArgTable[1] = 94;
		msg_emitInFlightMessage(MSG_SYSTEMCOND, playerIdx);
	}
}

// FUNCTION: XWA 0x4E4820
void laser_UpdateMineWeaponFire(int mineObjIdx) {
	int mineX;
	int mineY;
	int mineZ;
	int mineIdx;
	uint16_t flightGroupIdx;
	int regionIdx;
	uint16_t targetRef;
	int targetObjIdx;
	int leadTargetX;
	int leadTargetY;
	int leadTargetZ;
	uint16_t mineType;
	Q16Angle projectileYaw;
	Q16Angle projectilePitch;
	uint16_t offset;
	int distanceValue;
	uint16_t accuracyThreshold;
	uint16_t targetSpeedFrac;

	mineIdx = (uint16_t)mineObjIdx;
	if (g_objectTable[mineIdx].typeSpecificWord == 0) {
		return;
	}

	{
		uint16_t cooldownStep;
		uint8_t cooldown;

		cooldown = g_objectTable[mineIdx].typeSpecificByte[1];
		cooldownStep = (uint16_t)g_elapsedTicks >> 1;
		if ((uint16_t)cooldown > cooldownStep) {
			g_objectTable[mineIdx].typeSpecificByte[1] = (uint8_t)(cooldown - cooldownStep);
			return;
		}
	}
	g_objectTable[mineIdx].typeSpecificByte[1] = (uint8_t)-20;

	mineX = g_paifightSearchOriginX = g_objectTable[mineIdx].world_x;
	mineY = g_paifightSearchOriginY = g_objectTable[mineIdx].world_y;
	mineZ = g_paifightSearchOriginZ = g_objectTable[mineIdx].world_z;

	flightGroupIdx = g_objectTable[mineIdx].flightGroupIdx;
	g_paiContext.aiRequireLiveOrderTarget = (uint8_t)(g_objectTable[mineIdx].objectType == OBJ_Mine2);
	regionIdx = g_objectTable[mineIdx].regionIdx;

	targetRef = paifight_FindNearestMatchingTargetFromOrigin(
		(MissionTriggerVariableType)g_missionFlightGroups[flightGroupIdx]
			.fg.orders[4 * regionIdx]
			.target1Type,
		g_missionFlightGroups[flightGroupIdx].fg.orders[4 * regionIdx].target1,
		(int16_t)g_missionFlightGroups[flightGroupIdx].fg.orders[4 * regionIdx].target1OrTarget2,
		(MissionTriggerVariableType)g_missionFlightGroups[flightGroupIdx]
			.fg.orders[4 * regionIdx]
			.target2Type,
		g_missionFlightGroups[flightGroupIdx].fg.orders[4 * regionIdx].target2, 0);
	if (targetRef == 0xffffu) {
		targetRef = paifight_FindNearestMatchingTargetFromOrigin(
			(MissionTriggerVariableType)g_missionFlightGroups[flightGroupIdx]
				.fg.orders[4 * regionIdx]
				.secondaryTargetTypes[XWA_ORDER_TARGET_3],
			g_missionFlightGroups[flightGroupIdx]
				.fg.orders[4 * regionIdx]
				.secondaryTargets[XWA_ORDER_TARGET_3],
			(int16_t)g_missionFlightGroups[flightGroupIdx].fg.orders[4 * regionIdx].target3OrTarget4,
			(MissionTriggerVariableType)g_missionFlightGroups[flightGroupIdx]
				.fg.orders[4 * regionIdx]
				.secondaryTargetTypes[XWA_ORDER_TARGET_4],
			g_missionFlightGroups[flightGroupIdx]
				.fg.orders[4 * regionIdx]
				.secondaryTargets[XWA_ORDER_TARGET_4],
			0);
		if (targetRef == 0xffffu) {
			return;
		}
	}

	targetObjIdx = targetRef;
	{
		int targetX;
		int targetY;
		int targetZ;

		Mission_ResolveObjectOrMissionPointWorldLoc(targetRef, 0, 0, 0);
		targetX = worldlocx;
		targetY = worldlocy;
		targetZ = worldlocz;
		if ((unsigned int)collide_roughdistance3d(targetX - mineX, targetY - mineY, targetZ - mineZ) >=
			0x10000u) {
			return;
		}

		if (g_objectTable[targetObjIdx].mobj != NULL) {
			int leadFrames;
			uint8_t randomValue;
			MobileObject* targetMobj;

			trig2_ctop(g_objectTable[targetObjIdx].world_x - mineX,
					   g_objectTable[targetObjIdx].world_y - mineY,
					   g_objectTable[targetObjIdx].world_z - mineZ);
			leadFrames = (uint16_t)g_simStepScale * trig2_polardistance;
			trig2_polardistance = leadFrames;
			if (g_objectTable[mineIdx].objectType == OBJ_Mine2) {
				leadFrames >>= 15;
			} else {
				leadFrames >>= 14;
			}
			trig2_polardistance = leadFrames;

			randomValue = GameRand();
			targetMobj = g_objectTable[targetObjIdx].mobj;
			leadFrames = (uint16_t)((randomValue & 3) + leadFrames - 1);
			leadTargetX = g_objectTable[targetObjIdx].world_x +
						  leadFrames * (g_objectTable[targetObjIdx].world_x - targetMobj->prevWorldX);
			leadTargetY = g_objectTable[targetObjIdx].world_y +
						  leadFrames * (g_objectTable[targetObjIdx].world_y - targetMobj->prevWorldY);
			leadTargetZ = g_objectTable[targetObjIdx].world_z +
						  leadFrames * (g_objectTable[targetObjIdx].world_z - targetMobj->prevWorldZ);
		} else {
			leadTargetX = targetX;
			leadTargetY = targetY;
			leadTargetZ = targetZ;
		}
	}

	trig2_ctop(leadTargetX - mineX, leadTargetY - mineY, leadTargetZ - mineZ);
	mineType = g_objectTable[mineIdx].objectType;
	projectilePitch = targetPitch;
	projectileYaw = trig2_xyangle;
	offset = (uint16_t)(mineType > OBJ_Mine2 ? 170 : 150);
	if (targetPitch < 0x2000u) {
		mineZ += offset;
	} else if (targetPitch > 0x6000u) {
		if ((uint16_t)mineType >= OBJ_Mine3) {
			return;
		}
		mineZ -= offset;
	} else if (trig2_xyangle < 0x2000u || trig2_xyangle > 0xe000u) {
		mineY += offset;
	} else if (trig2_xyangle < 0x6000u) {
		mineX += offset;
	} else if (trig2_xyangle < 0xa000u) {
		mineY -= offset;
	} else {
		mineX -= offset;
	}

	distanceValue = trig2_polardistance;
	if (distanceValue >= 0x10000) {
		distanceValue = 0xffff;
	}
	distanceValue = ~distanceValue;
	if (g_objectTable[targetObjIdx].mobj == NULL) {
		targetSpeedFrac = 0xffffu;
	} else if (g_objectTable[targetObjIdx].mobj->speed < 0xbcu) {
		targetSpeedFrac = 0xffffu;
	} else {
		targetSpeedFrac = (uint16_t)(0x5dffu - ((uint16_t)g_objectTable[targetObjIdx].mobj->speed << 7));
	}

	accuracyThreshold = MATH2_fraction((uint16_t)distanceValue, targetSpeedFrac);
	if ((uint16_t)GameRand() > accuracyThreshold) {
		int aimError;

		aimError = ((uint16_t)GameRand() - 0x100) & 0x3ff;
		if ((uint16_t)GameRand() >= 0x8000u) {
			aimError = -aimError;
		}
		projectileYaw += aimError;

		aimError = ((uint16_t)GameRand() - 0x100) & 0x3ff;
		if ((uint16_t)GameRand() >= 0x8000u) {
			projectilePitch -= aimError;
			if ((projectilePitch & 0x8000u) != 0) {
				projectilePitch = 0;
			}
		} else {
			projectilePitch += aimError;
			if ((projectilePitch & 0x8000u) != 0) {
				projectilePitch = 0x7fff;
			}
		}
	}

	{
		ObjectTypeId projectileType;
		uint16_t projectileObjIdx;
		unsigned int projectileTypeIdx;
		WarheadGuidanceState* guidance;

		projectileObjIdx = Object_AllocSlotForGenus(GENUS_NpcProjectile);
		if (projectileObjIdx == 0xffffu) {
			return;
		}

		g_objectTable[projectileObjIdx].mobj->state = 1;
		g_objectTable[projectileObjIdx].genusId = GENUS_NpcProjectile;
		if (g_objectTable[mineIdx].objectType == OBJ_Mine2) {
			projectileType = OBJ_LaserIonTurbo;
		} else if (g_missionFlightGroups[flightGroupIdx].fg.iff == 1 ||
				   g_missionFlightGroups[flightGroupIdx].fg.iff == 4) {
			projectileType = OBJ_LaserImperialTurbo;
		} else {
			projectileType = OBJ_LaserRebelTurbo;
		}

		g_objectTable[projectileObjIdx].objectType = projectileType;
		projectileTypeIdx = (unsigned int)(projectileType - OBJ_LaserRebel);
		g_objectTable[projectileObjIdx].mobj->framesAlive = 1;
		g_objectTable[projectileObjIdx].mobj->sourceObjIdx = (int16_t)mineObjIdx;
		g_objectTable[projectileObjIdx].mobj->sourceObjectType = 0;
		g_objectTable[projectileObjIdx].mobj->iff =
			(int8_t)g_missionFlightGroups[g_objectTable[mineIdx].flightGroupIdx].fg.iff;
		g_objectTable[projectileObjIdx].pitch = (Q16Angle)projectilePitch;
		g_objectTable[projectileObjIdx].roll = 0;
		g_objectTable[projectileObjIdx].angleD = 0;
		g_objectTable[projectileObjIdx].yaw = (Q16Angle)projectileYaw;
		g_objectTable[projectileObjIdx].mobj->orientMatrixDirty = 1;
		g_objectTable[projectileObjIdx].mobj->moveVectorDirty = 1;
		g_objectTable[projectileObjIdx].mobj->speed =
			(uint16_t)(g_projectileSpeedByType[projectileTypeIdx] >> 1);
		g_objectTable[projectileObjIdx].mobj->lifetimeTimer =
			2 * laser_GetProjectileLifetimeTicks(projectileType);
		g_objectTable[projectileObjIdx].mobj->damageAmount = g_projectileDamageByType[projectileTypeIdx];

		FVIEW_calcrotatemove((int16_t)projectilePitch, (int16_t)projectileYaw,
							 &g_objectTable[projectileObjIdx]);
		g_objectTable[projectileObjIdx].mobj->prevWorldX = mineX;
		g_objectTable[projectileObjIdx].mobj->prevWorldY = mineY;
		g_objectTable[projectileObjIdx].mobj->prevWorldZ = mineZ;
		{
			int offsetX;
			int offsetY;
			int offsetZ;

			offsetX = Xwa_Q15Mul(g_fviewMoveX_Q15, g_projectileLaunchOffsetByType[projectileTypeIdx]);
			offsetY =
				Xwa_Q15MulReuseFirstSlot(g_fviewMoveY_Q15, g_projectileLaunchOffsetByType[projectileTypeIdx]);
			offsetZ =
				Xwa_Q15MulReuseFirstSlot(g_fviewMoveZ_Q15, g_projectileLaunchOffsetByType[projectileTypeIdx]);
			g_objectTable[projectileObjIdx].world_x = mineX + offsetX;
			g_objectTable[projectileObjIdx].world_y = mineY + offsetY;
			g_objectTable[projectileObjIdx].world_z = mineZ + offsetZ;
		}

		fsfx_triggerweaponsfx(projectileObjIdx, (unsigned int)g_localPlayer);
		guidance = g_objectTable[projectileObjIdx].mobj->pWarheadGuidance;
		guidance->homingTier = 0;
		guidance->targetObjIdx = targetRef;
		{
			int targetSignature;

			if (targetRef < 0x8000u) {
				targetSignature = g_objectTable[targetObjIdx].objectSignature;
			} else {
				targetSignature = 0;
			}
			guidance->sourcePlayerIdx = -1;
			guidance->targetSignature = (uint16_t)targetSignature;
		}
	}
}

// FUNCTION: XWA 0x492F20
uint16_t laser_createprojectilefromstatic(uint16_t staticObjIdx, uint16_t shooterObjIdx) {
	uint16_t flightGroupIdx;
	ObjectTypeId projectileObjectType;
	uint16_t projectileObjIdx;
	MobileObject* projectileMobj;
	WarheadGuidanceState* guidance;
	int spawnX;
	int spawnY;
	int spawnZ;
	int targetSignature;
	int targetPlayerIdx;

	flightGroupIdx = g_objectTable[staticObjIdx].flightGroupIdx;
	projectileObjectType = (ObjectTypeId)g_warheadTypeIds[g_missionFlightGroups[flightGroupIdx].fg.warhead];
	if (projectileObjectType == OBJ_None) {
		return 0xffffu;
	}

	projectileObjIdx = Object_AllocSlotForGenus(GENUS_NpcProjectile);
	if (projectileObjIdx == 0xffffu) {
		projectileObjIdx = (uint16_t)(g_projectileObjectSlotStart + g_playerProjectileSlotsTotal +
									  g_sharedPlayerProjectileSlotsPerRegion);
		while ((uint32_t)projectileObjIdx < g_projectileObjectSlotEnd) {
			ObjectRecord* scanObj;

			scanObj = &g_objectTable[projectileObjIdx];
#ifdef XWA_MODERN
			if (laser_GetProjectileWarheadClass(scanObj->objectType) == 0 &&
#else
			if (g_projectileWarheadClassByType[scanObj->objectType - OBJ_LaserRebel] == 0 &&
#endif
				scanObj->mobj->team == g_missionFlightGroups[flightGroupIdx].fg.team) {
				break;
			}
			++projectileObjIdx;
		}
	}

	if ((uint32_t)projectileObjIdx == g_projectileObjectSlotEnd) {
		return 0xffffu;
	}

	g_objectTable[projectileObjIdx].mobj->state = 1;
	g_objectTable[projectileObjIdx].genusId = GENUS_NpcProjectile;
	g_objectTable[projectileObjIdx].objectType = projectileObjectType;
	g_objectTable[projectileObjIdx].regionIdx = (uint8_t)regionIdx;
	g_objectTable[projectileObjIdx].mobj->framesAlive = 1;
	g_objectTable[projectileObjIdx].mobj->sourceObjIdx = (int16_t)staticObjIdx;
	g_objectTable[projectileObjIdx].mobj->sourceObjectType = g_objectTable[staticObjIdx].objectType;
	g_objectTable[projectileObjIdx].mobj->iff = (int8_t)g_missionFlightGroups[flightGroupIdx].fg.iff;
	g_objectTable[projectileObjIdx].pitch = 0;
	g_objectTable[projectileObjIdx].roll = 0;
	g_objectTable[projectileObjIdx].angleD = 0;
	g_objectTable[projectileObjIdx].yaw = 0;
	projectileMobj = g_objectTable[projectileObjIdx].mobj;
	guidance = projectileMobj->pWarheadGuidance;
	projectileMobj->speed = g_projectileSpeedByType[projectileObjectType - OBJ_LaserRebel];
	guidance->minSpeed = g_objectTable[projectileObjIdx].mobj->speed;
	g_objectTable[projectileObjIdx].mobj->damageAmount =
		g_projectileDamageByType[projectileObjectType - OBJ_LaserRebel];
	g_objectTable[projectileObjIdx].mobj->lifetimeTimer =
		(int)(uint16_t)(236u * g_projectileLifetimeSecondsByObjectType[projectileObjectType] +
						MATH2_fraction(g_projectileLifetimeFracQ16ByObjectType[projectileObjectType], 236u));

	Mission_ResolveObjectOrMissionPointWorldLoc(staticObjIdx, 0, 0, 0);
	spawnX = worldlocx;
	spawnY = worldlocy;
	spawnZ = worldlocz + 384;
	g_objectTable[projectileObjIdx].mobj->prevWorldX = spawnX;
	g_objectTable[projectileObjIdx].world_x = spawnX;
	g_objectTable[projectileObjIdx].mobj->prevWorldY = spawnY;
	g_objectTable[projectileObjIdx].world_y = spawnY;
	g_objectTable[projectileObjIdx].mobj->prevWorldZ = spawnZ;
	g_objectTable[projectileObjIdx].world_z = spawnZ;

	guidance->homingTier = (uint8_t)(((uint16_t)GameRand() & 3u) + 3u);
	guidance->targetObjIdx = shooterObjIdx;
	if (shooterObjIdx == 0xffffu || shooterObjIdx >= 0x8000u) {
		targetSignature = 0;
	} else {
		targetSignature = g_objectTable[shooterObjIdx].objectSignature;
	}
	guidance->targetSignature = (uint16_t)targetSignature;
	guidance->sourcePlayerIdx = -1;

	if (g_objectTable[shooterObjIdx].playerOwnerIdx != -1) {
		targetPlayerIdx = g_objectTable[g_objectTable[projectileObjIdx].mobj->pWarheadGuidance->targetObjIdx]
							  .playerOwnerIdx;
		if (targetPlayerIdx != -1 && g_players[targetPlayerIdx].pendingActionId == 0) {
			g_players[targetPlayerIdx].pendingActionId = 1;
			g_players[targetPlayerIdx].pendingActionIssuerPlayerIdx = -1;
			g_players[targetPlayerIdx].pendingActionParam = (int16_t)projectileObjIdx;
			g_players[targetPlayerIdx].pendingActionTimer = 1416;
			if (targetPlayerIdx == g_localPlayer) {
				msg_emitInFlightMessage(MSG_MISSILE_WARNING, targetPlayerIdx);
				fsfx_speakorderack(g_localPlayer, -1, 25, 0,
								   (unsigned int)g_players[g_localPlayer].objectIndex, 0xffffu);
				fsfx_PlaySound(65, 0xffffu, (unsigned int)g_localPlayer);
				g_incomingMissileWarningFlashActive = 1;
			}
		}
	}

	return projectileObjIdx;
}

// FUNCTION: XWA 0x4932F0
int laser_createcountermeasureprojectile(unsigned int ownerObjIdx, ObjectTypeId projectileObjectType) {
	ObjectRecord* ownerObj;
	int projectileGenus;
	unsigned int projectileObjIdx;
	ObjectRecord* projectileObj;
	WarheadGuidanceState* guidance;
	CraftData* ownerCraft;
	uint16_t ownerObjectType;

	if (g_objectTable[ownerObjIdx].playerOwnerIdx != -1) {
		uint32_t playerSlotStart;
		uint32_t playerSlotEnd;

		projectileGenus = GENUS_PlayerProjectile;
		playerSlotStart = g_objectSlotRangeByGenus[GENUS_PlayerProjectile].next +
						  12u * (uint32_t)g_objectTable[ownerObjIdx].playerOwnerIdx;
		playerSlotEnd = playerSlotStart + 12u;
		if (g_projectileWarheadClassByType[projectileObjectType - OBJ_LaserRebel] != 0) {
			playerSlotStart += 8u;
		}

		projectileObjIdx = playerSlotStart;
		while ((uint16_t)projectileObjIdx < playerSlotEnd) {
			if (g_objectTable[(uint16_t)projectileObjIdx].objectType == OBJ_None) {
				g_objectTable[(uint16_t)projectileObjIdx].mobj->sourceObjIdx = -1;
				g_objectTable[(uint16_t)projectileObjIdx].mobj->instanceExtent = 0;
				break;
			}
			++projectileObjIdx;
		}
		if ((uint16_t)projectileObjIdx >= playerSlotEnd) {
			playerSlotStart =
				g_objectSlotRangeByGenus[GENUS_PlayerProjectile].next + g_playerProjectileSlotsTotal;
			playerSlotEnd = playerSlotStart + g_sharedPlayerProjectileSlotsPerRegion;
			projectileObjIdx = playerSlotStart;
			while ((uint16_t)projectileObjIdx < playerSlotEnd) {
				if (g_objectTable[(uint16_t)projectileObjIdx].objectType == OBJ_None) {
					g_objectTable[(uint16_t)projectileObjIdx].mobj->sourceObjIdx = -1;
					g_objectTable[(uint16_t)projectileObjIdx].mobj->instanceExtent = 0;
					break;
				}
				++projectileObjIdx;
			}
		}

		if ((uint16_t)projectileObjIdx >= playerSlotEnd) {
			return 0xffff;
		}
		if (g_flightPlayerCount > 1 || g_filmRecording || g_filmPlaybackMode) {
			g_objectTable[(uint16_t)projectileObjIdx].objectSignature = 1;
		}
	} else {
		projectileGenus = GENUS_NpcProjectile;
		projectileObjIdx = Object_AllocSlotForGenus(GENUS_NpcProjectile);
	}

	if ((uint16_t)projectileObjIdx != 0xffffu) {
		int resultObjIdx;

		resultObjIdx = (uint16_t)projectileObjIdx;

		ownerObj = &g_objectTable[ownerObjIdx];
		ownerObjectType = ownerObj->objectType;
		g_objectTable[(uint16_t)projectileObjIdx].mobj->state = 1;
		g_objectTable[(uint16_t)projectileObjIdx].genusId = (ModelGenusId)projectileGenus;
		g_objectTable[(uint16_t)projectileObjIdx].objectType = projectileObjectType;
		g_objectTable[(uint16_t)projectileObjIdx].regionIdx = (uint8_t)regionIdx;
		g_objectTable[(uint16_t)projectileObjIdx].mobj->framesAlive = 1;
		g_objectTable[(uint16_t)projectileObjIdx].mobj->sourceObjIdx = (int16_t)ownerObjIdx;
		g_objectTable[(uint16_t)projectileObjIdx].mobj->sourceObjectType = ownerObjectType;
		(void)GetModelIndexFromType(ownerObjectType);
		g_objectTable[(uint16_t)projectileObjIdx].mobj->iff = ownerObj->mobj->iff;
		g_objectTable[(uint16_t)projectileObjIdx].mobj->team = ownerObj->mobj->team;
		g_objectTable[(uint16_t)projectileObjIdx].pitch = (Q16Angle)(0x8000u - ownerObj->pitch);
		g_objectTable[(uint16_t)projectileObjIdx].roll = ownerObj->roll;
		g_objectTable[(uint16_t)projectileObjIdx].angleD = 0;
		g_objectTable[(uint16_t)projectileObjIdx].yaw = (Q16Angle)(ownerObj->yaw + 0x8000u);
		guidance = g_objectTable[(uint16_t)projectileObjIdx].mobj->pWarheadGuidance;
		g_objectTable[(uint16_t)projectileObjIdx].mobj->speed =
			(uint16_t)(g_projectileSpeedByType[projectileObjectType - OBJ_LaserRebel] >> 1);
		guidance->minSpeed = (uint16_t)(g_projectileSpeedByType[projectileObjectType - OBJ_LaserRebel] +
										ownerObj->mobj->speed);
		g_objectTable[(uint16_t)projectileObjIdx].mobj->damageAmount =
			g_projectileDamageByType[projectileObjectType - OBJ_LaserRebel] + ownerObj->mobj->speed;
		if ((uint32_t)g_objectTable[(uint16_t)projectileObjIdx].mobj->damageAmount <
			(uint32_t)g_projectileDamageByType[projectileObjectType - OBJ_LaserRebel]) {
			g_objectTable[(uint16_t)projectileObjIdx].mobj->damageAmount =
				g_projectileDamageByType[projectileObjectType - OBJ_LaserRebel];
		}
		{
			uint16_t wholeSecondsTicks;

			wholeSecondsTicks =
				(uint16_t)(236u * g_projectileLifetimeSecondsByObjectType[projectileObjectType]);
			wholeSecondsTicks =
				(uint16_t)(wholeSecondsTicks +
						   MATH2_fraction(g_projectileLifetimeFracQ16ByObjectType[projectileObjectType],
										  236u));
			g_objectTable[(uint16_t)projectileObjIdx].mobj->lifetimeTimer = wholeSecondsTicks;
		}
		g_objectTable[(uint16_t)projectileObjIdx].mobj->orientMatrixDirty = 1;
		g_objectTable[(uint16_t)projectileObjIdx].mobj->moveVectorDirty = 1;

		projectileObj = &g_objectTable[(uint16_t)projectileObjIdx];
		projectileObj->mobj->prevWorldX = projectileObj->world_x = ownerObj->world_x;
		projectileObj->mobj->prevWorldY = projectileObj->world_y = ownerObj->world_y;
		projectileObj->mobj->prevWorldZ = projectileObj->world_z = ownerObj->world_z;

		{
			int launchOffset;

			launchOffset = ModelBounds_GetSizeY(projectileObj->objectType);
			launchOffset += ModelBounds_GetMaxY(ownerObjectType);
			FVIEW_calcrotatemove(projectileObj->pitch, projectileObj->yaw, projectileObj);
			trig2_xmovedist = Xwa_Q15Mul(projectileObj->mobj->moveX, (uint16_t)launchOffset);
			trig2_ymovedist = Xwa_Q15Mul(projectileObj->mobj->moveY, (uint16_t)launchOffset);
			trig2_zmovedist = Xwa_Q15Mul(projectileObj->mobj->moveZ, (uint16_t)launchOffset);
			Object_AddTrigMoveDeltaAndClampWorldPosition(projectileObj);
		}

		guidance->homingTier = 0;
		guidance->targetObjIdx = 0xffffu;
		guidance->targetSignature = 0;
		guidance->targetComponentIdx = 0xffffu;
		guidance->sourcePlayerIdx = (int8_t)g_objectTable[ownerObjIdx].playerOwnerIdx;
		ownerCraft = g_objectTable[ownerObjIdx].mobj->pCraft;
		if (g_missionFlightGroups[g_objectTable[ownerObjIdx].flightGroupIdx].fg.status1 != 21 &&
			g_missionFlightGroups[g_objectTable[ownerObjIdx].flightGroupIdx].fg.status2 != 21) {
			--ownerCraft->cmAmmoCount;
		}
		ownerCraft->cmFireCooldownTimer = 472;

		if (projectileObjectType == OBJ_WarheadFlare) {
			uint32_t scanObjIdx;
			uint32_t targetCandidateObjIdx;
			uint32_t targetCandidateDistance;
			uint32_t fallbackFlareTargetObjIdx;
			uint32_t fallbackFlareTargetDistance;
			int validTarget;
			int flareTargetingThisProjectileCount;

			if (g_useHardware3D) {
				Particle_AttachEffectToObject(12, projectileObjIdx, NULL, NULL);
			}

			targetCandidateDistance = UINT32_MAX;
			fallbackFlareTargetDistance = UINT32_MAX;
			targetCandidateObjIdx = 0xffffu;
			fallbackFlareTargetObjIdx = 0xffffu;

			for (scanObjIdx = g_projectileObjectSlotStart; scanObjIdx < g_projectileObjectSlotEnd;
				 ++scanObjIdx) {
				uint16_t scanType;

				scanType = g_objectTable[scanObjIdx].objectType;
				if (scanType == OBJ_None) {
					continue;
				}
				if (g_objectTable[scanObjIdx].mobj->state == 1 &&
#ifdef XWA_MODERN
					laser_GetProjectileWarheadClass((ObjectTypeId)scanType) > 0 &&
#else
					g_projectileWarheadClassByType[scanType - OBJ_LaserRebel] != 0 &&
#endif
					(unsigned int)g_objectTable[scanObjIdx].mobj->pWarheadGuidance->targetObjIdx ==
						ownerObjIdx) {
					uint32_t flareObjIdx;

					flareTargetingThisProjectileCount = 0;
					for (flareObjIdx = g_projectileObjectSlotStart; flareObjIdx < g_projectileObjectSlotEnd;
						 ++flareObjIdx) {
						if (scanType == OBJ_WarheadFlare &&
							(unsigned int)g_objectTable[flareObjIdx].mobj->pWarheadGuidance->targetObjIdx ==
								scanObjIdx) {
							++flareTargetingThisProjectileCount;
						}
					}

					pai_ObjectRefDirectionToObjectRef(ownerObjIdx, scanObjIdx);
					if (flareTargetingThisProjectileCount == 0) {
						if ((uint32_t)trig2_polardistance < targetCandidateDistance) {
							targetCandidateObjIdx = scanObjIdx;
							targetCandidateDistance = (uint32_t)trig2_polardistance;
						}
					} else if ((uint32_t)trig2_polardistance < fallbackFlareTargetDistance) {
						fallbackFlareTargetObjIdx = scanObjIdx;
						fallbackFlareTargetDistance = (uint32_t)trig2_polardistance;
					}
				}
			}

			if ((uint16_t)targetCandidateObjIdx == 0xffffu) {
				for (scanObjIdx = g_activeRegionObjectSlotStart;
					 (uint16_t)scanObjIdx < g_activeRegionCraftObjectSlotEnd; ++scanObjIdx) {
					ObjectRecord* scanObj;
					CraftData* scanCraft;

					scanObj = &g_objectTable[(uint16_t)scanObjIdx];
					if (scanObj->objectType == OBJ_None) {
						continue;
					}
					scanCraft = scanObj->mobj->pCraft;
					if (scanCraft->objectKind != 0) {
						continue;
					}

					validTarget = 0;
					if (scanObj->playerOwnerIdx == -1) {
						if ((unsigned int)pai_GetEffectiveAIController(scanCraft)->targetObjIdx ==
							ownerObjIdx) {
							validTarget = 1;
						}
					} else if (Object_IsHostileToTeam(
								   ownerObjIdx, (uint16_t)g_players[scanObj->playerOwnerIdx].playerIff)) {
						validTarget = 1;
					}

					if (validTarget) {
						pai_ObjectRefDirectionToObjectRef(ownerObjIdx, (uint16_t)scanObjIdx);
						if ((uint32_t)trig2_polardistance < targetCandidateDistance &&
							trig2_polardistance < 0x8000) {
							targetCandidateDistance = (uint32_t)trig2_polardistance;
							targetCandidateObjIdx = scanObjIdx;
						}
					}
				}
			}

			if ((uint16_t)targetCandidateObjIdx == 0xffffu) {
				targetCandidateObjIdx = fallbackFlareTargetObjIdx;
			}
			if ((uint16_t)targetCandidateObjIdx != 0xffffu) {
				guidance->targetObjIdx = (uint16_t)targetCandidateObjIdx;
				guidance->homingTier = 6;
				guidance->targetSignature = g_objectTable[(uint16_t)targetCandidateObjIdx].objectSignature;
			}
		}

		if ((unsigned int)(uint16_t)guidance->targetObjIdx >= g_activeRegionObjectSlotStart &&
			(unsigned int)(uint16_t)guidance->targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
			g_objectTable[(uint16_t)projectileObjIdx].mobj->lifetimeTimer >>= 1;
		}

		if (guidance->targetObjIdx != 0xffffu &&
			g_objectTable[guidance->targetObjIdx].playerOwnerIdx == g_localPlayer) {
			fsfx_QueueVoiceSfx(61, 0, 0, 0, 0xffff, 0xffff);
		}
		if (g_objectTable[ownerObjIdx].playerOwnerIdx != -1) {
			fsfx_triggerweaponsfx((unsigned int)resultObjIdx,
								  (unsigned int)g_objectTable[ownerObjIdx].playerOwnerIdx);
		}
		return resultObjIdx;
	}
	return 0xffff;
}
