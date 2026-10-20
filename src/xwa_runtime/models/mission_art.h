#ifndef XWA_RUNTIME_MODELS_MISSION_ART_H
#define XWA_RUNTIME_MODELS_MISSION_ART_H

/* Art that a mission adds for itself.
 *
 * A mod's mission can add image files (DATs) of its own. When such a file
 * replaces images that other files also have, the bake gives it a folder of
 * its own, resdata/mission_art/<name>/ in the mod folder, with an atlas for
 * each of its groups, next to the shared atlases in resdata/atlas. The
 * mission's file, flight/missions/<mission>.yaml, lists the files that the
 * mission adds, in its order:
 *
 *   resdata: ["planet2", "planetcustom", "backdrop_special"]
 *
 * A name with a folder in resdata/mission_art is the mission's own art. The
 * others are already in the shared atlases. While the mission plays, a group
 * is drawn from the first of its art folders that has it, and the other
 * groups from the shared atlases. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The longest art name, its end included. */
#define XWA_MISSION_ART_NAME_MAX 32
/* The most groups that a mission's art gives. */
#define XWA_MISSION_ART_GROUPS_MAX 64

/* Makes the art of mission `missionPath` (such as "missions\1b0m1fw.tie")
   the active art. The groups whose source changes are unloaded, so that the
   game builds them again from their new source. Called by Mission_Init,
   before any texture of the mission is loaded. */
void XwaMissionArt_LoadMission(const char* missionPath);

/* The name of the active art that group `group` comes from, or NULL when it
   comes from the shared atlases. */
const char* XwaMissionArt_GroupSource(int group);

/* The groups that come from the active art: `count` returns their number,
   and `at` the group and the art name of entry `index`. The generation
   changes each time the active art does. */
int XwaMissionArt_Count(void);
int XwaMissionArt_At(int index, int* group, const char** art);
uint64_t XwaMissionArt_Generation(void);

#ifdef __cplusplus
}
#endif

#endif
