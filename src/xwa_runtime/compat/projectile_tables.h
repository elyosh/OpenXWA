#ifndef XWA_RUNTIME_COMPAT_PROJECTILE_TABLES_H
#define XWA_RUNTIME_COMPAT_PROJECTILE_TABLES_H

/* The projectile tables, for object types that aren't weapon types.
 *
 * When a warhead's lifetime runs out, Object_UpdateLifetimeAndMovement turns
 * it into an explosion or a spark (types 266 to 271). The movement code that
 * follows in the same step still uses the projectile's genus, so the
 * explosion goes through the warhead homing step once. That step reads the
 * homing profile base with the explosion's type. In the original, that
 * reads the unreferenced data before the table. In C, it would be
 * undefined behavior, so the port calls this function instead. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* g_projectileHomingProfileBaseByType for a weapon type, and what the
   original reads for types 264 to 279. Returns 0 for other types, which
   never reach the homing step. */
uint8_t XwaProjectileTables_HomingProfileBase(unsigned int objectType);

#ifdef __cplusplus
}
#endif

#endif
