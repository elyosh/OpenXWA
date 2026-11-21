#include "xwa_runtime/compat/projectile_tables.h"

#include "xwa/assets/object_type.h"
#include "xwa/flight/object/laser.h"

#include <stdint.h>

/* Modern port module; no original counterpart. */

enum {
	/* The first type of k_homingProfileBaseBefore. */
	HOMING_BEFORE_FIRST_TYPE = OBJ_ExplosionTextureGroup2000,
};

/* The bytes before g_projectileHomingProfileBaseByType (0x5B6760 to
   0x5B676F), read for types 264 to 279. */
static const uint8_t k_homingProfileBaseBefore[OBJ_LaserRebel - HOMING_BEFORE_FIRST_TYPE] = {
	21, 4, 21, 3, 21, 2, 21, 1, 21, 0, 0, 0, 0, 0, 0, 0,
};

uint8_t XwaProjectileTables_HomingProfileBase(unsigned int objectType) {
	if (objectType >= OBJ_LaserRebel && objectType <= OBJ_NoAsset_307) {
		return g_projectileHomingProfileBaseByType[objectType - OBJ_LaserRebel];
	}
	if (objectType >= HOMING_BEFORE_FIRST_TYPE && objectType < OBJ_LaserRebel) {
		return k_homingProfileBaseBefore[objectType - HOMING_BEFORE_FIRST_TYPE];
	}
	return 0;
}
