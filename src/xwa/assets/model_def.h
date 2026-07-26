#ifndef XWA_ASSETS_MODEL_DEF_H
#define XWA_ASSETS_MODEL_DEF_H

#include "xwa/assets/model_mesh.h"
#include "xwa/assets/opt_model.h"
#include "xwa/math/vec3i.h"
#include "xwa/flight/object/object.h"
#include "xwa/util/memory.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { XWA_MODEL_DEF_COUNT = 265 };

typedef struct ModelWeaponHardpoint {
	int16_t x;
	int16_t z;
	int16_t y;
	uint8_t alternateMeshHardpointIdx;
	uint8_t meshIdx;
} ModelWeaponHardpoint;

#pragma pack(push, 1)
typedef struct CockpitSparkHardpoint {
	float localX;
	float localZ;
	float localY;
	uint8_t gap0C[12];
	uint8_t hardpointIndex;
} CockpitSparkHardpoint;
typedef char cockpit_spark_hardpoint_size[(sizeof(CockpitSparkHardpoint) == 0x19) ? 1 : -1];

typedef struct ModelFloatHardpoint {
	int x;
	int z;
	int negY;
	uint8_t componentIndex;
} ModelFloatHardpoint;
#pragma pack(pop)

typedef struct ModelDef {
	char* nameLong;
	char* nameAlt;
	uint16_t craftPointValue;
	uint16_t ratingWeight;
	uint8_t hasHyperdrive;
	uint8_t laserConvergeMode;
	uint8_t hasShields;
	int shieldStrength;
	uint8_t field_13;
	uint8_t gap_14;
	int hullStrength;
	int systemDamageHullThreshold;
	uint16_t systemStrength;
	uint8_t componentMaxHp;
	uint16_t maxSpeed;
	uint16_t accelRate;
	uint16_t decelRate;
	int16_t yawRate;
	uint16_t autoBankFactor;
	int16_t rollRate;
	int16_t pitchRate;
	uint16_t maxTumbleAngle;
	uint16_t maxPushRate;
	char name[256];
	uint16_t laserGroupWeaponType[3];
	uint8_t laserGroupFirstSlot[3];
	uint8_t laserGroupLastSlot[3];
	uint8_t laserGroupSlotCount[3];
	uint8_t laserGroupMountType[3];
	int laserGroupFireRange[3];
	uint16_t laserGroupFireCooldownTicks[3];
	uint16_t warheadLauncherType[2];
	uint8_t warheadLauncherFirstSlot[2];
	uint8_t warheadLauncherLastSlot[2];
	uint8_t warheadLauncherSlotCount[2];
	uint8_t warheadLauncherValue[2];
	ModelWeaponHardpoint weaponHardpoints[16];
	OptEngineGlow* engineGlows[16];
	uint8_t engineGlowMeshIdx[16];
	uint8_t engineGlowCount;
	uint8_t countermeasureCount;
	int gunsightOffsetY;
	int16_t primaryHardpointY;
	int16_t primaryHardpointZ;
	int16_t primaryHardpointX;
	int16_t turretSeatPosY[2];
	int16_t turretSeatPosZ[2];
	int16_t turretSeatPosX[2];
	int16_t turretMountAngleB[2];
	int16_t turretMountAngleA[2];
	uint16_t turretModelIndex[2];
	int16_t turretAimLimitA[2];
	int16_t turretAimLimitB[2];
	int meshAttachData[11];
	uint16_t boundSizeShift;
	uint16_t boundSizeX;
	uint16_t boundSizeZ;
	uint16_t boundSizeY;
	uint16_t dockPointCount;
	Vec3i dockPoints[9];
	int childMountPoints[6];
	uint8_t floatHardpointCount;
	uint8_t auxHardpointCount;
	Vec3i auxHardpoints[8];
	uint8_t jammingPointCount;
	Vec3i jammingPoints[8];
} ModelDef;

extern ModelDef g_modelDefs[XWA_MODEL_DEF_COUNT];
extern MemoryHandle g_modelFloatHardpointDataHandles[XWA_LOADED_MODEL_COUNT];
extern OptEngineGlow* g_cockpitEngineGlows[16];
extern uint8_t g_cockpitEngineGlowMeshIdx[16];
extern uint8_t g_cockpitEngineGlowCount;
extern CockpitSparkHardpoint g_cockpitSparkHardpoints[16];
extern uint8_t g_cockpitSparkHardpointCount;
extern OptEngineGlow* g_exteriorEngineGlows[16];
extern uint8_t g_exteriorEngineGlowMeshIdx[16];
extern uint8_t g_exteriorEngineGlowCount;
extern OptEngineGlow* g_spaceBombEngineGlows[16];
extern uint8_t g_spaceBombEngineGlowMeshIdx[16];
extern uint8_t g_spaceBombEngineGlowCount;
extern uint8_t g_unusedSpaceBombEngineGlowInitByte;

int ModelDef_IsFloatingHardpointModel(uint16_t loadedModelSlot);
int ComputeCraftCombatRating(ObjectTypeId objectType);

#ifdef __cplusplus
}
#endif

#endif
