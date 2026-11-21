#ifndef XWA_RUNTIME_MODELS_PROJECTILES_H
#define XWA_RUNTIME_MODELS_PROJECTILES_H

/* The projectile tables of a mod.
 *
 * The `projectiles` section of flight/flight.yaml changes the game's
 * projectile tables (projectile_tables.inc) for some weapon types. Each key
 * is a weapon type name, with any of these keys: power, speed_mglt,
 * life_seconds, life_fraction, launch_offset_m, warhead_class, score, side,
 * side_model and homing_profile. The `homing_profiles` section changes rows
 * of the homing tables: each key is a row from 0 to 5, with `turn_rates`
 * and `speed_rates`, 7 numbers each. The mod documentation describes the
 * keys.
 *
 * The values replace the original game's values. So they apply to every
 * craft, and a weapon profile's stats still win over them, as they win over
 * the original values. */

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the two sections and writes them into the tables. Called once at
   startup. */
void XwaProjectiles_Init(void);

#ifdef __cplusplus
}
#endif

#endif
