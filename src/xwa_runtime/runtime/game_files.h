#ifndef XWA_RUNTIME_GAME_FILES_H
#define XWA_RUNTIME_GAME_FILES_H

#include "aeron/vfs.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The size of the path that XwaGameFiles_Find writes. */
#define XWA_GAME_FILES_PATH_SIZE 300

/* The mod folder's game/ folder holds files with the same paths as in the
   game folder. A file there replaces the game's file with the same path,
   found in any case. Only these files are looked for there:
   - the mission files, MISSIONS/, MELEE/ and COMBAT/<mission>.TIE;
   - the mission lists, <same folders>/MISSION.LST, and MISSIONS/EMAIL.TXT;
   - the sounds, WAVE/...: the mission voices, the briefing, debriefing and
	 concourse voices, and the sound banks (xwa_runtime/audio/sound_banks.h);
   - the cutscene table, FRONTRES/CUTSCENE/CUTSCENE.TXT.

   Returns 1 when the mod folder has a copy of game file `path`, and writes
   the copy's path in the mod folder into `modPath`. Returns 0 otherwise. */
int XwaGameFiles_Find(AeronVfs* vfs, const char* path, char* modPath, size_t modPathSize);

#ifdef __cplusplus
}
#endif

#endif
