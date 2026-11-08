#ifndef XWA_APP_MOD_LIST_H
#define XWA_APP_MOD_LIST_H

/* The list of mods:
 *   - the original game, with no mod;
 *   - each folder in the mods folder (paths.mods) that has a mod file at its
 *     top;
 *   - the configured mod folder (paths.mod), if it is somewhere else and has
 *     a mod file.
 *
 * A folder without a mod file is not a mod. A mod can be used only if its
 * file gives XWA_MOD_FORMAT_VERSION as the version and doesn't mark the mod
 * as incomplete. A mod whose file has `imported` was made by the
 * importer. */

#include "host_config.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XwaModState {
	XWA_MOD_READY,
	XWA_MOD_UNFINISHED,    /* `complete: false`, such as an import that didn't finish */
	XWA_MOD_OTHER_VERSION, /* made for another mod format version */
	XWA_MOD_UNREADABLE,    /* its mod.yaml has an error */
} XwaModState;

typedef struct XwaModEntry {
	char name[256];
	/* The mod folder. Empty for the original game. */
	char path[XWA_HOST_CONFIG_PATH_CAPACITY];
	XwaModState state;
	/* Why the mod can't be used, for the player. Empty when it can. */
	char problem[160];
} XwaModEntry;

/* Reads the mod.yaml of folder `path` into `out`. Returns 0 when the folder
 * has none. A mod.yaml with an error gives XWA_MOD_UNREADABLE. */
int XwaModList_Read(const char* path, XwaModEntry* out);

typedef struct XwaModList {
	XwaModEntry* entries;
	size_t count;
} XwaModList;

/* Writes the mods folder: `configured` (paths.mods) when it is set, else
 * the mods folder in Aeron_LocalDataPath. Needs Aeron to be running. */
int XwaModList_Folder(const char* configured, char* out, size_t capacity);

/* Lists the mods. The original game comes first, then the mods of
 * `mods_folder` by folder name, then the configured mod. `mods_folder` and
 * `configured_mod` may be NULL or empty. Returns 0 when out of memory. */
int XwaModList_Scan(const char* mods_folder, const char* configured_mod, XwaModList* out);
void XwaModList_Free(XwaModList* list);

/* Whether `path` is an existing folder. */
int XwaModList_IsFolder(const char* path);

/* Whether two folder paths are the same, ignoring a final separator. */
int XwaModList_SamePath(const char* a, const char* b);

#ifdef __cplusplus
}
#endif

#endif
