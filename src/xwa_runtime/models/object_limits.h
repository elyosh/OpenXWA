#ifndef XWA_RUNTIME_MODELS_OBJECT_LIMITS_H
#define XWA_RUNTIME_MODELS_OBJECT_LIMITS_H

/* A mod's object counts, from the `object_limits` section of
 * flight/flight.yaml:
 *
 *   object_limits:
 *     craft_per_region: 3072
 *     player_projectiles: 128        # for each player
 *     ai_projectiles_per_region: 1024
 *     explosions_per_region: 256     # and as many debris slots
 *     radar_blips: 512               # on each scope
 *
 * A mission's file can have the same section, and its keys replace the ones
 * of flight/flight.yaml for that mission. Every key is optional. Without
 * one, the stock count stays: 96 craft, 12 projectiles for each player, 64
 * projectiles for the AI, 32 explosions and 32 debris objects in each region,
 * and 48 blips on each radar scope. The proving grounds keep their 36 craft
 * slots and 224 salvage slots. radar_blips can be at most
 * HUD_RADAR_MAX_BLIPS.
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
