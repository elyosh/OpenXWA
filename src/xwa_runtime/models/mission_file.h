#ifndef XWA_RUNTIME_MODELS_MISSION_FILE_H
#define XWA_RUNTIME_MODELS_MISSION_FILE_H

/* A mission's file in the mod folder, which holds the mod's changes to one
 * mission. The game reads it each time it loads the mission, and the other
 * modules read their sections from it. */

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
   isn't a map (logged). `path` receives the file's path for diagnostics. */
int XwaMissionFile_Load(const char* missionPath, struct AeronConfigFile** out, char* path, size_t pathSize);

/* Returns the flight group index for entry `key` of a `flight_groups` map.
   Logs a warning and returns -1 when `key` isn't an index, `entry` isn't a
   map, the mission has no such flight group, or the flight group fails the
   entry's optional `name` or `craft` check. `craft` is the object type name
   of the flight group's craft type. `flightGroups` points to the loaded
   mission's first flight group, `stride` is the distance between two flight
   groups and `count` is their number. */
int XwaMissionFile_FlightGroup(const char* path, const char* key, const struct AeronConfigNode* entry,
							   const struct XwaFlightGroup* flightGroups, size_t stride, int count);

#ifdef __cplusplus
}
#endif

#endif
