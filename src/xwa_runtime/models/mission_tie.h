#ifndef XWA_RUNTIME_MODELS_MISSION_TIE_H
#define XWA_RUNTIME_MODELS_MISSION_TIE_H

/* The changes that a mod's mission makes to its flight groups, from the
 * `flight_groups` section of its mission file: markings, the markings of
 * single craft, IFF, pilot voice, warhead pools and weapon profiles. They
 * are applied right after the .TIE file loads, so they change the
 * simulation. Each entry names a flight group by its position in the
 * mission's .TIE file. XwaWarheadPools_ReadFlightGroup reads the warhead
 * pools. */

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
