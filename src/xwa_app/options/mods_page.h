#ifndef XWA_APP_MODS_PAGE_H
#define XWA_APP_MODS_PAGE_H

/*
 * The Mods tab of the OpenXWA Options window. It lists the mods, chooses the
 * one that OpenXWA uses at its next start, and deletes the mods that the
 * importer made. With the mod importer (XWA_HAS_MOD_IMPORT), it also imports
 * an XWA Upgrade installation into a new mod. The import and a delete run on
 * their own threads.
 */

#include "aeron/scene/ui.h"

#include <stdbool.h>

typedef struct XwaModsPageSetup {
	const char* game_data;   /* the game data folder in use */
	const char* running_mod; /* the folder of the mod in use, or "" */
	bool launch_override;    /* the mod comes from --mod */
} XwaModsPageSetup;

void XwaModsPage_Init(AeronUiContext* ui, const XwaModsPageSetup* setup);
/* Stops a running import, waits for it and for a delete, and frees the
 * page. */
void XwaModsPage_Shutdown(void);
/* Call when the tab becomes visible: it reads the configuration and lists
 * the mods again. */
void XwaModsPage_Open(void);
/* Call each frame while the options window is open, whatever the tab, before
 * the window is drawn. It follows the folder dialog, the import and a
 * delete. */
void XwaModsPage_Update(void);
/* Draws the tab, inside the options window's tab bar. */
void XwaModsPage_Draw(void);
/* Draws the tab's modals, after the window's content, whatever the tab. */
void XwaModsPage_DrawModals(void);
/* Whether an import or a delete runs. The options window can't be closed
 * then. */
bool XwaModsPage_Busy(void);
/* The line that tells the player why the window can't be closed, or NULL
 * when it can. */
const char* XwaModsPage_BusyMessage(void);
/* Cancels a running import, such as when the datapad closes under the
 * window. A delete can't stop halfway, so it goes on. */
void XwaModsPage_CancelImport(void);
/* Asks to quit OpenXWA. During an import, a modal asks whether to stop it.
 * During a delete, OpenXWA quits when it ends. Returns whether the page took
 * the request; otherwise the caller can quit at once. */
bool XwaModsPage_RequestQuit(void);
/* Whether the player pressed Restart Now since the last call. The caller
 * then restarts OpenXWA. */
bool XwaModsPage_TakeRestartRequest(void);
/* Tells the player that OpenXWA can't restart by itself. */
void XwaModsPage_RestartFailed(void);

#endif
