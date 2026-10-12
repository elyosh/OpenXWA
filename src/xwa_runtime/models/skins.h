#ifndef XWA_RUNTIME_MODELS_SKINS_H
#define XWA_RUNTIME_MODELS_SKINS_H

/* Which look each model shows for each markings value.
 *
 * A model's GLB names, in its markings list, the material variant each
 * markings value shows. A mission may select others in the `skins` section
 * of its remaster file (mission_file.h), keyed by GLB name and markings
 * value; null selects the model's own materials:
 *
 *   skins:
 *     awing:
 *       0: "0|default_2,ace"
 *
 * The renderer creates each model's mesh with only the variants its objects
 * use. This module tells it, through the snapshot, the mission's selections
 * and the markings values used: every flight group's, the player cockpit
 * and exterior with their flight group's, the frontend previews', and those
 * registered during the mission (hangar objects, the objects whose markings
 * game code changes). An object shown with markings nothing registered is
 * registered then (logged for a GLB), and its mesh is created again. Uses
 * last until the next mission is loaded. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct XwaFlightGroup;
struct XwaSnapshot;

/* Reads the skin selections of mission `missionPath` (NULL for none),
   replacing the previous mission's, and forgets the flight groups and
   uses. */
void XwaSkins_LoadMission(const char* missionPath);

/* Records the mission's flight groups once their markings are final:
   `flightGroups` is the first one, `stride` the distance between two and
   `count` their number. */
void XwaSkins_SetFlightGroups(const struct XwaFlightGroup* flightGroups, size_t stride, int count);

/* Registers, for the rest of the mission, that objects of `objectType` show
   `markings`, or every markings value (game code changes them). */
void XwaSkins_UseMarkings(uint16_t objectType, int markings);
void XwaSkins_UseAllMarkings(uint16_t objectType);

/* Exports the selections and the markings values used into `snapshot`,
   after its loaded models, cockpit and previews; skin_generation changes
   when they do. */
void XwaSkins_Capture(struct XwaSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
