#ifndef XWA_RUNTIME_MODELS_MISSION_FILE_H
#define XWA_RUNTIME_MODELS_MISSION_FILE_H

/* A mission's file in the mod folder: flight/missions/<mission path without its
 * extension>.yaml, lower case, which holds what the game reads at mission
 * load:
 *
 *   version: 1
 *   resdata: ...           (mission_art.h)
 *   settings: ...          (mission_settings.h)
 *   flight_groups: ...     (mission_tie.h)
 *   object_profiles: ...   (object_profiles.h)
 *   skins: ...             (skins.h)
 *   articulations: ...     (articulation.h)
 *   hangar: ...            (hangars.h)
 *   model_swaps: ...       (model_swaps.h)
 *   craft_names: ...       (craft_names.h)
 *   concourse: ...         (frontend/concourse_planet.h)
 *   headlights: true       (headlights.h)
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigFile;
struct AeronConfigNode;
struct XwaFlightGroup;

/* Loads the file of mission `missionPath` (for example
   "missions\1b1m5fg.tie"). Returns 1 with *out set (free it with
   AeronConfigFile_Destroy), 0 when the mission has no file, -1 when the file
   is invalid or lacks "version: 1" (logged). `path` receives the file's
   path for diagnostics. */
int XwaMissionFile_Load(const char* missionPath, struct AeronConfigFile** out, char* path, size_t pathSize);

/* The flight-group index of entry `key` of a `flight_groups` map, or -1
   (logged) when `key` is not an index, `entry` is not a map, the mission has
   no such flight group or the entry's optional `name` and `craft` checks do
   not match it. `flightGroups` is the loaded mission's first flight group,
   `stride` the distance between two of them and `count` their number. */
int XwaMissionFile_FlightGroup(const char* path, const char* key, const struct AeronConfigNode* entry,
							   const struct XwaFlightGroup* flightGroups, size_t stride, int count);

#ifdef __cplusplus
}
#endif

#endif
