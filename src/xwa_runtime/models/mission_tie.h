#ifndef XWA_RUNTIME_MODELS_MISSION_TIE_H
#define XWA_RUNTIME_MODELS_MISSION_TIE_H

/* The [mission_tie] values of a mod's mission, from its mission file,
 * flight/missions/<mission>.yaml in the mod folder:
 *
 *   flight_groups:
 *     1: { name: "Red", craft: 1, markings: 4 }
 *     5: { name: "Gold", craft: 2, iff: 2, pilot_voice: "Rebel_Pilot3" }
 *
 * Each entry names a flight group by its position in the mission's .TIE,
 * with the optional `name` and `craft` checks of mission_file.h. Its keys are
 * optional: `markings` and `iff` are numbers from 0 to 255, and
 * `pilot_voice` is the name of the pilot's voice, up to 19 characters. */

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
   values. Without a file, nothing changes. */
void XwaMissionTie_LoadMission(const char* missionPath, struct XwaFlightGroup* flightGroups, size_t stride,
							   int count);

#ifdef __cplusplus
}
#endif

#endif
