#ifndef XWA_APP_MOD_UPDATE_H
#define XWA_APP_MOD_UPDATE_H

/*
 * Changes to a mod folder that wait until OpenXWA doesn't read its files: a
 * new import of the mod, and a delete.
 *
 * A new import of a usable mod goes to its staging folder,
 * <parent>/.openxwa-update/<folder name>. XwaModUpdate_Apply then swaps the
 * folders: the old mod goes to <parent>/.openxwa-trash/<folder name>, and the
 * staging folder takes its place. A mod marked for deletion has a
 * .openxwa-delete file in its folder, and XwaModUpdate_DeletePending moves
 * it to the trash folder. The trash is deleted in the background
 * (XwaModUpdate_StartCleanup).
 *
 * Only folders whose mod.yaml comes from the importer are moved or deleted.
 * Each step leaves a state that the next call recognizes, so a swap that
 * stops halfway is finished or undone at the next start.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XwaModUpdateResult {
	XWA_MOD_UPDATE_NONE,    /* no update to apply */
	XWA_MOD_UPDATE_APPLIED, /* the new import replaced the old one */
	XWA_MOD_UPDATE_FAILED,  /* a folder couldn't be renamed, such as while a program uses it */
} XwaModUpdateResult;

/* The staging folder of mod folder `mod`. */
bool XwaModUpdate_StagingFolder(const char* mod, char* out, size_t capacity);
/* Whether `mod` has a staging folder with a complete import for this
 * version of OpenXWA. */
bool XwaModUpdate_HasUpdate(const char* mod);
/* Whether `mod` has a staging folder, complete or not. */
bool XwaModUpdate_HasStaging(const char* mod);

/* Swaps `mod` and its staging folder when the staging folder has a complete
 * import, or finishes or undoes a swap that stopped halfway. Call it before
 * OpenXWA reads the files of `mod`. When a rename fails, such as while a
 * program uses the folder, nothing changes and a later call tries again. The
 * old mod stays in the trash folder until the cleanup deletes it. */
XwaModUpdateResult XwaModUpdate_Apply(const char* mod);

/* Marks `mod` for deletion at the next start, or removes the mark. */
bool XwaModUpdate_MarkDelete(const char* mod, bool marked);
bool XwaModUpdate_IsMarkedDelete(const char* mod);
/* Moves each mod marked for deletion to its trash folder, with its staging
 * folder: the folders at the top of `mods_folder`, and `configured_mod`
 * (paths.mod). Call it at startup, before any mod is read. */
void XwaModUpdate_DeletePending(const char* mods_folder, const char* configured_mod);
/* Calls XwaModUpdate_Apply on each mod with a staging folder in
 * `mods_folder`, and on `configured_mod` (paths.mod). Call it at startup,
 * after XwaModUpdate_DeletePending and before any mod is read. This applies
 * a new import made while its mod was in use, even when another mod is
 * selected now. */
void XwaModUpdate_ApplyPending(const char* mods_folder, const char* configured_mod);

/* Deletes folder `dir` if its mod.yaml comes from the importer, or if it
 * holds no files. mod.yaml is deleted last, so a folder that is only partly
 * deleted is still recognized. Stops between files when `stop` is set, and
 * then returns false. */
bool XwaModUpdate_DeleteFolder(const char* dir, volatile int* stop);

/* Deletes the trash folders beside `mods_folder`'s mods and beside
 * `configured_mod`, on a thread of its own. */
void XwaModUpdate_StartCleanup(const char* mods_folder, const char* configured_mod);
/* Stops the cleanup between two files and waits for it. The next start
 * finishes it. */
void XwaModUpdate_StopCleanup(void);

#ifdef __cplusplus
}
#endif

#endif
