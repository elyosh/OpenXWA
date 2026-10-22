#ifndef XWA_RUNTIME_MODELS_WARHEAD_POOLS_H
#define XWA_RUNTIME_MODELS_WARHEAD_POOLS_H

/* The warheads that a flight group has in its hangar for the whole mission.
 * They come from the `warheads` key of the flight group's entry in the
 * mission file (mission_tie.h):
 *
 *   flight_groups:
 *     0: { name: "Alpha", craft: 5, warheads: { missiles: 80, torpedoes: 40 } }
 *
 * The keys are the warhead types of the mission's flight groups: space_bombs,
 * heavy_rockets, missiles, torpedoes, advanced_missiles, advanced_torpedoes,
 * mag_pulses, ion_pulses, advanced_mag_pulses and cluster_bombs. The last two
 * load missiles in the game. A warhead type without a value has no limit.
 * The flight group's double and half warhead statuses double or halve the
 * values.
 *
 * The pool doesn't pay for the warheads that a craft starts with, but it
 * limits them. Each launcher rack gets at most what is left divided by the
 * number of racks of the launcher. This applies to every craft of the flight
 * group, AI included.
 *
 * In the hangar, every warhead that goes into a player's rack comes out of
 * the pool: the full racks when the player loads warheads from the menu, and
 * each warhead that the service adds after landing. The same limit by rack
 * applies. The hangar menu shows what is left. The pool only goes down in
 * single-player flights, because the hangar runs on each player's machine
 * alone. The pools start again at each mission load, so a restart or a film
 * starts with full pools.
 *
 * The pools change the simulation. */

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;
struct XwaFlightGroup;

/* Starts a mission with no pools. Called by mission_tie.c. */
void XwaWarheadPools_Clear(void);
/* Reads the `warheads` map of flight group `fgIdx`, `fg`, whose entry has
   the key `fgKey` in the mission file `path`. Called by mission_tie.c.
   Returns the number of values read. */
int XwaWarheadPools_ReadFlightGroup(const char* path, const char* fgKey, int fgIdx,
									const struct XwaFlightGroup* fg, const struct AeronConfigNode* warheads);

/* Returns the warheads of type `warheadType` (the index of a mission's
   flight group warhead, from 1 to 10) left in the pool of flight group
   `fgIdx`, or -1 when there is no limit. */
int XwaWarheadPools_Left(int fgIdx, int warheadType);
/* Returns `count`, the warheads of type `warheadType` for one of the `racks`
   racks of a launcher of a craft of flight group `fgIdx`, limited to `left`
   divided by `racks`. `left` is a value of XwaWarheadPools_Left. */
int XwaWarheadPools_Limit(int left, int racks, int count);
/* Takes `count` warheads of type `warheadType` from the pool of flight group
   `fgIdx`, in single-player flights. */
void XwaWarheadPools_Take(int fgIdx, int warheadType, int count);

#ifdef __cplusplus
}
#endif

#endif
