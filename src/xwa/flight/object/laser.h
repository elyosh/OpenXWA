#ifndef XWA_FLIGHT_OBJECT_LASER_H
#define XWA_FLIGHT_OBJECT_LASER_H

#include "xwa/assets/object_type.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The projectile tables, in projectile_tables.inc. A mod can change them at
   startup (xwa_runtime/models/projectiles.h). */
extern int g_projectileDamageByType[XWA_PROJECTILE_TYPE_COUNT];
extern uint16_t g_projectileSpeedByType[XWA_PROJECTILE_TYPE_COUNT];
extern uint16_t g_projectileLifetimeSecondsByType[XWA_PROJECTILE_TYPE_COUNT];
extern uint16_t g_projectileLifetimeFracQ16ByType[XWA_PROJECTILE_TYPE_COUNT];
extern int16_t g_projectileLaunchOffsetByType[XWA_PROJECTILE_TYPE_COUNT];
extern uint8_t g_projectileWarheadClassByType[XWA_PROJECTILE_TYPE_COUNT];
extern uint16_t g_projectileScoreByType[XWA_PROJECTILE_TYPE_COUNT];
extern uint8_t g_projectileRequiredIffByType[XWA_PROJECTILE_TYPE_COUNT];
extern uint16_t g_projectileAlternateIffTypeByType[XWA_PROJECTILE_TYPE_COUNT];
extern uint8_t g_projectileHomingProfileBaseByType[XWA_PROJECTILE_TYPE_COUNT];
extern uint16_t g_projectileHomingTurnRateByProfile[42];
extern uint16_t g_projectileHomingSpeedAdjustRateByProfile[42];

#ifndef XWA_MODERN
/* The original code reads some projectile tables by object type, at the
   table's address minus 280 entries. */
// GLOBAL: XWA 0x5B62F8
extern const uint16_t g_projectileSpeedByObjectType_BiasedBase[];
// GLOBAL: XWA 0x5B6330
extern const uint16_t g_projectileLifetimeSecondsByObjectType_BiasedBase[];
// GLOBAL: XWA 0x5B6368
extern const uint16_t g_projectileLifetimeFracQ16ByObjectType_BiasedBase[];
// GLOBAL: XWA 0x5B63F8
extern const uint16_t g_projectileScoreByObjectType_BiasedBase[];
extern const uint8_t g_projectileRequiredIffByObjectType_BiasedBase[];
extern const uint16_t g_projectileAlternateIffTypeByObjectType_BiasedBase[];
// GLOBAL: XWA 0x5B6658
extern const uint8_t g_projectileHomingProfileBaseByObjectType_BiasedBase[];
#endif

extern const uint16_t g_warheadLeadErrorModulusByAiLevel[6];
extern const uint16_t g_warheadFireChanceThresholdByAiLevel[6];
extern const uint8_t g_projectileRequiredIffByShooterIff[8];

#ifdef XWA_MODERN
/* Returns -1 when objectType cannot index the compact projectile tables. */
static inline int laser_GetProjectileWarheadClass(ObjectTypeId objectType) {
	uint16_t type = (uint16_t)objectType;

	if (type < OBJ_LaserRebel || type > OBJ_NoAsset_307) {
		return -1;
	}

	return g_projectileWarheadClassByType[type - OBJ_LaserRebel];
}
#endif

uint16_t laser_GetProjectileLifetimeTicks(ObjectTypeId projectileObjectType);
void laser_weaponsfire(void);
void laser_UpdateMineWeaponFire(int mineObjIdx);
int laser_createprojectile(unsigned int firerObjIdx, int weaponSlotIdx, ObjectTypeId projectileType,
						   int playerIdx);
void laser_firewarheadlauncher(unsigned int ownerObjIdx, uint16_t launcherIdx, uint16_t targetRef);
int laser_firemissile(int firerObjIdx, int warheadSlot, int projectileType, unsigned int fireMode);
void laser_firelasersystem(unsigned int shooterObjIdx, int laserGroupIdx, int playerIdx,
						   uint16_t updateFireDelay);
void laser_firerocketsystem(int firerObjIdx, unsigned int fireMode);
void laser_fireplayerweapon(int playerIdx);
uint16_t laser_createprojectilefromstatic(uint16_t staticObjIdx, uint16_t shooterObjIdx);
int laser_createcountermeasureprojectile(unsigned int ownerObjIdx, ObjectTypeId projectileObjectType);

#ifdef __cplusplus
}
#endif

#endif
