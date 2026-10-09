#ifndef XWA_RUNTIME_MODELS_SKINS_H
#define XWA_RUNTIME_MODELS_SKINS_H

/* XWAU skins: which skin each model shows for each markings value.
 *
 * A converted model's GLB carries its skins as material variants and the
 * variant each markings value shows by default. A mission may select others
 * in the `skins` section of its remaster file (mission_file.h), keyed by
 * GLB name and markings value:
 *
 *   skins:
 *     awing:
 *       0: "0|default_2,ace"
 *
 * The renderer creates each skinned model's mesh with only the variants its
 * objects use. This module tells it, through the snapshot, the mission's
 * selections and the markings values each model's objects use: every flight
 * group's, the player cockpit and exterior with their flight group's, and
 * the frontend previews'. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct XwaFlightGroup;
struct XwaSnapshot;

/* Reads the skin selections of mission `missionPath` (NULL for none),
   replacing the previous mission's, and forgets the flight groups. */
void XwaSkins_LoadMission(const char* missionPath);

/* Records the mission's flight groups once their markings are final:
   `flightGroups` is the first one, `stride` the distance between two and
   `count` their number. */
void XwaSkins_SetFlightGroups(const struct XwaFlightGroup* flightGroups, size_t stride, int count);

/* Exports the selections and the markings values used into `snapshot`,
   after its loaded models, cockpit and previews; skin_generation changes
   when they do. */
void XwaSkins_Capture(struct XwaSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
