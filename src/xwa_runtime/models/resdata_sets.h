#ifndef XWA_RUNTIME_MODELS_RESDATA_SETS_H
#define XWA_RUNTIME_MODELS_RESDATA_SETS_H

/* Art that a mission adds for itself.
 *
 * A mod's mission can add image files (DATs) of its own. When such a file
 * replaces images that other files also have, the bake makes it a set: an
 * atlas for each of its groups, in resdata/sets/<set>/group_<g>.yaml in the
 * mod folder, next to the shared ones in resdata/atlas. The mission's file,
 * flight/missions/<mission>.yaml, lists the files that the mission adds, in
 * its order:
 *
 *   resdata: ["planet2", "planetcustom", "backdrop_special"]
 *
 * A name with a folder in resdata/sets is a set. The others were baked into
 * the shared atlases, so they need nothing more. While the mission plays, a
 * group of one of its sets is drawn from the first set that has it, and the
 * other groups from the shared atlases. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The longest set name, its end included. */
#define XWA_RESDATA_SET_NAME_MAX 32
/* The most groups that a mission's sets give. */
#define XWA_RESDATA_SET_GROUPS_MAX 64

/* Makes the sets of mission `missionPath` (such as "missions\1b0m1fw.tie")
   the active ones. The groups whose set changes are unloaded, so that the
   game builds them again from their new source. Called by Mission_Init,
   before any texture of the mission is loaded. */
void XwaResdataSets_LoadMission(const char* missionPath);

/* The active set that group `group` comes from, or NULL when it comes from
   the shared atlases. */
const char* XwaResdataSets_GroupSet(int group);

/* The groups that come from an active set: `count` returns their number,
   and `at` the group and the set name of entry `index`. The generation
   changes each time the active sets do. */
int XwaResdataSets_Count(void);
int XwaResdataSets_At(int index, int* group, const char** set);
uint64_t XwaResdataSets_Generation(void);

#ifdef __cplusplus
}
#endif

#endif
