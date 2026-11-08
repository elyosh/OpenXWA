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
	 concourse voices, and the sound banks;
   - the cutscene table, FRONTRES/CUTSCENE/CUTSCENE.TXT;
   - the medal files in FRONTRES/MEDALS/: MEDALS.TXT, the missions that give
	 the battle medals; MEDALS.LST, the medal images; and FAMILYAWARDS.TXT,
	 the memento texts.

   A mod can also remove game files. game/files.yaml lists them under
   `removed`, and the game then works as if they didn't exist. Only the
   files that a mod can replace can be removed. A listed file is removed
   even when game/ has a copy of it.

   XwaGameFiles_Load reads game/files.yaml. XwaGameFiles_Find says where to
   read game file `path` from. For XWA_GAME_FILE_MOD, it writes the path of
   the mod's copy into `modPath`. */
typedef enum XwaGameFileSource {
	/* The game folder's file, if it has one. */
	XWA_GAME_FILE_ORIGINAL,
	/* The mod's copy, at `modPath`. */
	XWA_GAME_FILE_MOD,
	/* The mod removes the file. */
	XWA_GAME_FILE_REMOVED,
} XwaGameFileSource;

void XwaGameFiles_Load(void);
XwaGameFileSource XwaGameFiles_Find(AeronVfs* vfs, const char* path, char* modPath, size_t modPathSize);

#ifdef __cplusplus
}
#endif

#endif
