#ifndef XWA_RUNTIME_MODELS_OBJECT_LIMITS_H
#define XWA_RUNTIME_MODELS_OBJECT_LIMITS_H

/* A mod's object counts, from the `object_limits` section of its flight
 * settings, which a mission file's section of the same name can replace
 * key by key. Without a key, the stock count stays. The proving grounds
 * keep their 224 salvage slots and their 36 craft slots, unless the mod has
 * hangar settings, because a hangar map can need more than 36 slots.
 * radar_blips can be at most HUD_RADAR_MAX_BLIPS.
 *
 * Each player searches its own block of player_projectiles slots for its
 * shots. Warheads use the slots from the ninth one of the block, as in the
 * stock game.
 *
 * The counts must fit in the object table, which has at most
 * XWA_MAX_OBJECTS slots. When they don't, they are lowered, with a log:
 * first the craft slots, down to 96, then the AI's projectiles, down to 64,
 * then the explosions, down to 32, then the player's projectiles, down to 12.
 *
 * The counts change which slot each object gets, so they change the
 * simulation. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the `object_limits` section of flight/flight.yaml. Called once at
   startup. */
void XwaObjectLimits_Init(void);

/* Reads the counts of mission `missionPath` (for example
   "missions\1b1m5fg.tie") over the ones of flight/flight.yaml, and lowers
   them to fit a table of `regions` regions for `players` players.
   Mission_Init calls it before it sizes the object table. */
void XwaObjectLimits_LoadMission(const char* missionPath, uint32_t regions, uint32_t players,
								 int provingGrounds);

/* The counts of the mission that Mission_Init loaded last. */
uint32_t XwaObjectLimits_CraftPerRegion(void);
uint32_t XwaObjectLimits_PlayerProjectiles(void);
uint32_t XwaObjectLimits_AiProjectilesPerRegion(void);
uint32_t XwaObjectLimits_ExplosionsPerRegion(void);
uint32_t XwaObjectLimits_RadarBlips(void);

#ifdef __cplusplus
}
#endif

#endif
