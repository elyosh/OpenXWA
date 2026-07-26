#include "xwa/assets/model_def.h"

#include "aeron/log.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/frontend/frontend_display.h"

#include "model_def_static_data.inc"

// GLOBAL: XWA 0x9E9080
MemoryHandle g_modelFloatHardpointDataHandles[XWA_LOADED_MODEL_COUNT];
// GLOBAL: XWA 0x7FFB80
OptEngineGlow* g_cockpitEngineGlows[16];
// GLOBAL: XWA 0x7FFBC0
uint8_t g_cockpitEngineGlowMeshIdx[16];
// GLOBAL: XWA 0x7FFBD0
uint8_t g_cockpitEngineGlowCount;
// GLOBAL: XWA 0x7FFBD1
CockpitSparkHardpoint g_cockpitSparkHardpoints[16];
// GLOBAL: XWA 0x7FFD61
uint8_t g_cockpitSparkHardpointCount;
// GLOBAL: XWA 0x910960
OptEngineGlow* g_exteriorEngineGlows[16];
// GLOBAL: XWA 0x9109A0
uint8_t g_exteriorEngineGlowMeshIdx[16];
// GLOBAL: XWA 0x9109B0
uint8_t g_exteriorEngineGlowCount;
// GLOBAL: XWA 0x8D5760
OptEngineGlow* g_spaceBombEngineGlows[16];
// GLOBAL: XWA 0x8D57A0
uint8_t g_spaceBombEngineGlowMeshIdx[16];
// GLOBAL: XWA 0x8D57B0
uint8_t g_spaceBombEngineGlowCount;
// GLOBAL: XWA 0x8D5941
uint8_t g_unusedSpaceBombEngineGlowInitByte;

static int ModelDef_GetProjectileDamage(ObjectTypeId objectType) {
	switch (objectType) {
		case OBJ_LaserRebel:
			return 250;
		case OBJ_LaserRebelTurbo:
			return 500;
		case OBJ_LaserImperial:
			return 200;
		case OBJ_LaserImperialTurbo:
			return 400;
		case OBJ_LaserIon:
			return 200;
		case OBJ_LaserIonTurbo:
			return 400;
		case OBJ_WarheadTorpedo:
			return 10000;
		case OBJ_WarheadMissile:
			return 3000;
		case OBJ_WarheadLaser1:
			return 1500;
		case OBJ_WarheadLaser2:
			return 1600;
		case OBJ_WarheadIon:
			return 800;
		case OBJ_WarheadAdvancedTorpedo:
			return 15000;
		case OBJ_WarheadAdvancedMissile:
			return 6000;
		case OBJ_WarheadSpaceBomb:
			return 65000;
		case OBJ_WarheadRocket:
			return 35000;
		case OBJ_WarheadMagPulse:
			return 3000;
		case OBJ_WarheadIonPulse:
			return 6000;
		case OBJ_WarheadLaser3:
			return 600;
		case OBJ_WarheadFlare:
			return 500;
		default:
			return 0;
	}
}

int ModelDef_IsFloatingHardpointModel(uint16_t loadedModelSlot) {
	return (g_modelTypeTable[loadedModelSlot].flags & 0x0800u) != 0;
}

// FUNCTION: XWA 0x4DD160
int ComputeCraftCombatRating(ObjectTypeId objectType) {
	int modelIndex;
	ModelDef* modelDef;
	ModelGenusId genusId;
	int shieldScore;
	int durabilityScore;
	int maneuverScore;
	int speedScore;
	int weaponScore;
	int rating;
	int group;

	if ((uint16_t)objectType >= OBJ_Count) {
		modelIndex = -1;
	} else {
		modelIndex = g_modelTypeTable[(uint16_t)objectType].modelIndex;
	}
	if (modelIndex == -1) {
		return 5;
	}

	modelDef = &g_modelDefs[modelIndex];
	if (modelDef->hasShields) {
		shieldScore = modelDef->shieldStrength / 100;
	} else {
		shieldScore = 10;
	}

	durabilityScore = (shieldScore + modelDef->hullStrength / 100) / 2;
	genusId = g_modelTypeTable[objectType].genusId;
	if (genusId == GENUS_Starship || genusId == GENUS_Platform) {
		durabilityScore *= 16;
	} else if (genusId == GENUS_Freighter || genusId == GENUS_Container) {
		durabilityScore *= 4;
	}

	maneuverScore = (((uint16_t)modelDef->pitchRate >> 8) + ((uint16_t)modelDef->yawRate >> 8) +
					 ((uint16_t)modelDef->rollRate >> 8)) /
					3;
	speedScore = modelDef->maxSpeed >> 1;

	weaponScore = 0;
	if (ModelDef_IsFloatingHardpointModel((uint16_t)objectType)) {
		for (group = 0; group < 3; ++group) {
			if (modelDef->laserGroupSlotCount[group]) {
				weaponScore +=
					modelDef->laserGroupSlotCount[group] * (modelDef->laserGroupFireRange[group] / 0x10000) *
					ModelDef_GetProjectileDamage((ObjectTypeId)modelDef->laserGroupWeaponType[group]) / 10;
			}
		}
	} else {
		for (group = 0; group < 3; ++group) {
			if (modelDef->laserGroupSlotCount[group]) {
				int fireRange;
				int score;

				fireRange = modelDef->laserGroupFireRange[group];
				if (!fireRange) {
					fireRange = 0x10000;
				}
				score = ModelDef_GetProjectileDamage((ObjectTypeId)modelDef->laserGroupWeaponType[group]) *
						modelDef->laserGroupSlotCount[group] * (fireRange / 0x10000) / 10;
				if (modelDef->laserGroupMountType[group] == 4) {
					score *= 2;
				}
				weaponScore += score;
			}
		}
	}

	rating = (int)((double)(durabilityScore + maneuverScore + speedScore + weaponScore) * 0.25 * 0.727);
	if (genusId == GENUS_Starship || genusId == GENUS_Platform) {
		rating = 50 * ((rating + 25) / 50);
		if (!rating) {
			return 50;
		}
		return rating;
	}

	rating = 5 * ((rating + 2) / 5);
	if (!rating) {
		return 5;
	}
	return rating;
}
