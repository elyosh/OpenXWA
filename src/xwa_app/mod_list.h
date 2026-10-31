#ifndef XWA_APP_MOD_LIST_H
#define XWA_APP_MOD_LIST_H

/* The mods that OpenXWA can use:
 *   - the original game, with no mod;
 *   - each <game data>/mods/<folder> that has a mod.yaml;
 *   - the configured mod folder (paths.mod), when it is somewhere else.
 *
 * A mod.yaml gives the mod's name and whether its import finished:
 *
 *   name: "TFTC Reimagined 1.4"
 *   complete: true
 *
 * A configured folder without one is named after the folder and counts as
 * complete. */

#include "host_config.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaModEntry {
	char name[256];
	/* The mod folder. Empty for the original game. */
	char path[XWA_HOST_CONFIG_PATH_CAPACITY];
	/* 0 when its import didn't finish: it can't be chosen. */
	int complete;
} XwaModEntry;

typedef struct XwaModList {
	XwaModEntry* entries;
	size_t count;
} XwaModList;

/* Lists the mods. The original game comes first, then the mods of
 * <game data>/mods by folder name, then the configured mod.
 * `configured_mod` may be NULL or empty. Returns 0 when out of memory. */
int XwaModList_Scan(const char* game_data, const char* configured_mod, XwaModList* out);
void XwaModList_Free(XwaModList* list);

/* Whether two folder paths are the same, ignoring a final separator. */
int XwaModList_SamePath(const char* a, const char* b);

#ifdef __cplusplus
}
#endif

#endif
