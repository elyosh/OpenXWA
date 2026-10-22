#ifndef XWA_RUNTIME_MODELS_MISSION_TIE_H
#define XWA_RUNTIME_MODELS_MISSION_TIE_H

/* The [mission_tie] values of a mod's mission, from its mission file,
 * flight/missions/<mission>.yaml in the mod folder:
 *
 *   flight_groups:
 *     1: { name: "Red", craft: 1, markings: 4 }
 *     5: { name: "Gold", craft: 2, iff: 2, pilot_voice: "Rebel_Pilot3" }
 *     26: { name: "Red", craft: 1, craft_markings: { 0: 0, 1: 1 } }
 *     0: { name: "Alpha", craft: 5, warheads: { missiles: 80 } }
 *
 * Each entry names a flight group by its position in the mission's .TIE,
 * with the optional `name` and `craft` checks of mission_file.h. Its keys are
 * optional: `markings` and `iff` are numbers from 0 to 255, and
 * `pilot_voice` is the name of the pilot's voice, up to 19 characters.
 * `craft_markings` gives single craft their own markings, from 0 to 255. Its
 * keys are the craft's number in its wave, from 0, so the same craft of
 * every wave gets them. `warheads` gives the flight group's warhead pools
 * (XwaWarheadPools_ReadFlightGroup). */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct XwaFlightGroup;

/* Sets the markings, IFF and pilot voice of the flight groups of mission
   `missionPath` (for example "missions\1b1m5fg.tie") from its mission file.
   `flightGroups` is the loaded mission's first flight group, `stride` the
   distance between two of them and `count` their number. Called by
   Mission_Init right after the .TIE is read, so every later reader sees the
   values. Without a file, nothing changes and no craft has markings of its
   own. */
void XwaMissionTie_LoadMission(const char* missionPath, struct XwaFlightGroup* flightGroups, size_t stride,
							   int count);

/* The markings that the mission gives craft `craft` (its number in its wave)
   of flight group `flightGroupIdx`, or -1. Mission_InitFlightGroupObjectSlot
   sets them on each new craft, after the object profile is applied, so the
   object profile uses the flight group's markings. */
int XwaMissionTie_CraftMarkings(int flightGroupIdx, int craft);

/* Registers the markings of single craft with the skins module. Called
   after XwaSkins_SetFlightGroups. */
void XwaMissionTie_UseSkins(void);

#ifdef __cplusplus
}
#endif

#endif
