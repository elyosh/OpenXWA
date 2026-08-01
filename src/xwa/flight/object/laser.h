#ifndef XWA_FLIGHT_OBJECT_LASER_H
#define XWA_FLIGHT_OBJECT_LASER_H

#include "xwa/assets/object_type.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint16_t g_projectileLifetimeSecondsByObjectType[OBJ_LaserImperialDS + 1];
extern const uint16_t g_projectileLifetimeFracQ16ByObjectType[OBJ_LaserImperialDS + 1];
extern const uint16_t g_projectileLifetimeSecondsByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1];
extern const uint16_t g_projectileLifetimeFracQ16ByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1];
extern const uint16_t g_projectileSpeedByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1];
extern const int      g_projectileDamageByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1];
extern const uint8_t  g_projectileWarheadClassByType[OBJ_LaserImperialDS - OBJ_LaserRebel + 1];

#ifdef XWA_MODERN
/* Returns -1 when objectType cannot index the compact projectile tables. */
static inline int laser_GetProjectileWarheadClass(ObjectTypeId objectType) {
	uint16_t type = (uint16_t)objectType;

	if (type < OBJ_LaserRebel || type > OBJ_LaserImperialDS) {
		return -1;
	}

	return g_projectileWarheadClassByType[type - OBJ_LaserRebel];
}
#endif

/* Mission-score point value lost when a player-owned warhead is destroyed, indexed by warhead
   object type. Original is a biased-base table; only the warhead object-type window is populated. */
extern const uint16_t g_warheadProjectilePointValueByObjectType[OBJ_LaserImperialDS + 1];
extern const uint16_t g_warheadProjectilePointValue[11];
/* Per-warhead homing profile base, indexed by warhead object type. Added to the guidance homing
   tier to select a profile row in the homing turn-rate / speed-adjust tables. */
extern const uint8_t  g_projectileHomingProfileBaseByObjectType[OBJ_LaserImperialDS + 1];
/* Homing turn rate (Q16 angle/step scale) per resolved homing profile row. */
extern const uint16_t g_projectileHomingTurnRateByProfile[44];
/* Homing speed adjustment rate per resolved homing profile row. */
extern const uint16_t g_projectileHomingSpeedAdjustRateByProfile[12];

uint16_t laser_GetProjectileLifetimeTicks(ObjectTypeId projectileObjectType);
void     laser_weaponsfire(void);
void     laser_UpdateMineWeaponFire(int mineObjIdx);
int      laser_createprojectile(unsigned int firerObjIdx, int weaponSlotIdx, ObjectTypeId projectileType,
								int playerIdx);
void     laser_firewarheadlauncher(unsigned int ownerObjIdx, uint16_t launcherIdx, uint16_t targetRef);
int      laser_firemissile(int firerObjIdx, int warheadSlot, int projectileType, unsigned int fireMode);
void     laser_firelasersystem(unsigned int shooterObjIdx, int laserGroupIdx, int playerIdx,
							   uint16_t updateFireDelay);
void     laser_firerocketsystem(int firerObjIdx, unsigned int fireMode);
void     laser_fireplayerweapon(int playerIdx);
uint16_t laser_createprojectilefromstatic(uint16_t staticObjIdx, uint16_t shooterObjIdx);
int      laser_createcountermeasureprojectile(unsigned int ownerObjIdx, ObjectTypeId projectileObjectType);

#ifdef __cplusplus
}
#endif

#endif
