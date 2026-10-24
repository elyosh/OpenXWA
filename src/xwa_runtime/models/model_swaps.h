#ifndef XWA_RUNTIME_MODELS_MODEL_SWAPS_H
#define XWA_RUNTIME_MODELS_MODEL_SWAPS_H

/* Model swaps: a mission can load another model in place of one, for every
 * object that loads it, in the `model_swaps` section of its mission file, keyed
 * by model key:
 *
 *   model_swaps:
 *     superstardestroyer: superstardestroyerrecentered
 *     millenniumfalcon2cockpit: minifalconcockpit
 *
 * The target must be a GLB of the mod folder (flight/models/<key>.glb).
 * OptModel_LoadHandle loads the target instead of the source, so the model
 * def, collision, hardpoints, skins and the HD renderer's mesh all come from
 * the target. The tech room gets no swaps. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the swaps of mission `missionPath` (for example
   "missions\1b7m2w.tie"), replacing the previous mission's. A target
   without a GLB is logged and skipped. Without a file, nothing is
   swapped. */
void XwaModelSwaps_LoadMission(const char* missionPath);

/* Writes the file to load instead of `fileName` (such as
   "FlightModels\SuperStarDestroyer.opt") into `out` and returns 1, or
   returns 0 when the mission doesn't swap it. */
int XwaModelSwaps_FileName(const char* fileName, char* out, size_t outSize);

/* Writes the key of the model that object type `objectType` loads in this
   mission to `key` (XWA_MODEL_SOURCE_KEY_MAX bytes): the swap's target,
   else the type's own key. Returns 0 when the type has no key. */
int XwaModelSwaps_TypeKey(unsigned int objectType, char* key);

/* A number that changes whenever the swaps can change: when a mission's
   swaps are read, and when the tech room turns them off or on. Code that
   keeps values by object type reads them again when it changes. */
int XwaModelSwaps_State(void);

/* The key that the mission swaps for `key`, or NULL when `key` is no
   swap's target. Checks that depend on the object type, such as the float
   hardpoints of the Super Star Destroyer, use it. */
const char* XwaModelSwaps_SourceOf(const char* key);

#ifdef __cplusplus
}
#endif

#endif
