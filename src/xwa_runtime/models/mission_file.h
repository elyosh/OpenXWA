#ifndef XWA_RUNTIME_MODELS_MISSION_FILE_H
#define XWA_RUNTIME_MODELS_MISSION_FILE_H

/* A mission's remaster file: remaster/missions/<mission path without its
 * extension>.yaml, lower case, which holds the selections the game reads at
 * mission load (object profiles, skins):
 *
 *   version: 1
 *   object_profiles: ...
 *   skins: ...
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigFile;

/* Loads the file of mission `missionPath` (for example
   "missions\1b1m5fg.tie"). Returns 1 with *out set (free it with
   AeronConfigFile_Destroy), 0 when the mission has no file, -1 when the file
   is invalid or lacks "version: 1" (logged). `path` receives the file's
   path for diagnostics. */
int XwaMissionFile_Load(const char* missionPath, struct AeronConfigFile** out, char* path, size_t pathSize);

#ifdef __cplusplus
}
#endif

#endif
