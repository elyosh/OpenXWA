#ifndef XWA_APP_MODS_PAGE_INTERNAL_H
#define XWA_APP_MODS_PAGE_INTERNAL_H

/* The state that the Mods tab's two files share: mods_page.c (the list of
 * mods) and mods_import.c (the import form, its progress and its summary).
 * mods_import.c is only built with the mod importer (XWA_HAS_MOD_IMPORT). */

#include "aeron/dialog.h"
#include "aeron/scene/ui.h"
#include "aeron/sync.h"
#include "install_paths.h"
#include "mod_list.h"

#if XWA_HAS_MOD_IMPORT
#include "import.h"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define XWA_MODS_PATH_CAP XWA_INSTALL_PATH_CAPACITY
#define XWA_MODS_LOG_LINES 2000
/* The height of a row of buttons and the space above it, in reference
 * pixels. */
#define XWA_MODS_BUTTON_ROWS_REF 90.0f

typedef enum XwaModsView {
	XWA_MODS_VIEW_LIST,
	XWA_MODS_VIEW_FORM,
	XWA_MODS_VIEW_RUNNING,
	XWA_MODS_VIEW_DONE,
} XwaModsView;

/* The field that the folder dialog fills. */
/* What the list knows of each mod, besides its XwaModEntry. */
enum {
	XWA_MODS_IMPORTED = 1u << 0, /* the importer made it */
	XWA_MODS_UPDATE = 1u << 1,   /* a new import waits in its staging folder */
	XWA_MODS_MARKED = 1u << 2,   /* it is deleted at the next start */
};

/* What the delete thread deletes. */
typedef enum XwaModsDelete {
	XWA_MODS_DELETE_MOD,    /* the mod, and its staging folder */
	XWA_MODS_DELETE_UPDATE, /* only its staging folder */
} XwaModsDelete;

typedef enum XwaModsBrowse {
	XWA_MODS_BROWSE_SOURCE,
	XWA_MODS_BROWSE_AUTHORING,
	XWA_MODS_BROWSE_MODS_LIST, /* the mods folder, from the list */
} XwaModsBrowse;

typedef struct XwaModsPage {
	AeronUiContext* ui;
	XwaModsView view;

	/* From XwaModsPage_Init. */
	char game[XWA_MODS_PATH_CAP];
	char running_mod[XWA_MODS_PATH_CAP];
	bool launch_override;
	char resources[XWA_MODS_PATH_CAP];

	XwaInstallPaths config;
	int config_ok;
	char config_message[512];
	/* The mods folder that OpenXWA lists. */
	char saved_mods_folder[XWA_MODS_PATH_CAP];
	/* The mods folder of the list and of the form. */
	char mods_folder[XWA_MODS_PATH_CAP];

	AeronFolderDialog* dialog;
	XwaModsBrowse browse;

	/* The form. The mod goes in <the mods folder that OpenXWA lists>/<name>.
	   The name follows the installation, and the authoring folder the mod
	   folder, until the player edits them. */
	char source[XWA_MODS_PATH_CAP];
	/* The folder of the mod that Import Again imports again, or "". The
	   import then goes there whatever the name. */
	char again[XWA_MODS_PATH_CAP];
	/* The mod folder holds a usable mod that the importer made, so the
	   import goes to its staging folder, and replaces it when it is
	   complete. */
	bool staged;
	char staging[XWA_MODS_PATH_CAP];
	char name[256];
	char mod[XWA_MODS_PATH_CAP];
	char authoring[XWA_MODS_PATH_CAP];
	int keep_authoring;
	int name_edited, authoring_edited;
	int checked; /* the fields were checked since they last changed */
	int source_ok;
	char source_message[512];
	char check_message[512];
	int replace_open;

#if XWA_HAS_MOD_IMPORT
	/* The import. */
	XwaModImportCheck check;
	XwaModImportOptions options;
	XwaModImportProgress progress;
	int progress_ready;
	XwaModImportResult result;
#endif
	AeronThread* thread;
	uint64_t start_us, end_us;
	unsigned long long next_line;
	char* lines[XWA_MODS_LOG_LINES]; /* a ring of the last lines */
	size_t line_start, line_count;
	AeronUiListItem items[XWA_MODS_LOG_LINES];
	size_t log_selected;
	size_t failure_selected;
	int cancel_open, quit_open, quit_after_import;
	char done_message[512];
	int done_restart; /* done_message asks for a restart that Restart Now does */

	/* The list. */
	XwaModList mods;
	AeronUiListItem* mod_items;
	unsigned char* mod_flags; /* XWA_MODS_IMPORTED and the others, for each mod */
	size_t mod_chosen;        /* the mod of config.yaml */
	size_t mod_next;          /* the mod that the next start uses: mod_chosen, or the original game */
	size_t mod_running;       /* the mod in use */
	size_t mod_selected;
	int mods_scanned;
	char mods_message[512];
	char mods_folder_message[512];             /* why the mods folder field isn't used */
	char mods_notice[XWA_MODS_PATH_CAP + 160]; /* the mod that OpenXWA is set to use is missing */
	uint64_t mods_folder_edit_us;              /* when the field was last typed in, or 0 */
	/* Deleting a mod, on its own thread. */
	int delete_open;
	AeronThread* delete_thread;
	volatile int delete_done, delete_ok;
	XwaModsDelete delete_kind;
	int quit_after_delete;
	char delete_path[XWA_MODS_PATH_CAP];
	char delete_name[256];
	/* Restart Now. The dialog, which asks after Use This Mod and confirms
	   the button in a flight, the question after Use This Mod (or ""), the
	   request that main.c takes, and why it failed. */
	int restart_open;
	char restart_question[320];
	bool restart_requested;
	char restart_message[256];
} XwaModsPage;

extern XwaModsPage g_xwaModsPage;

/* mods_page.c */
void XwaModsPage_LoadConfig(void);
/* Makes OpenXWA list the mods of the mods folder field, if it names a
   folder. */
void XwaModsPage_ApplyModsFolder(void);
/* Saves `path` as the mod that OpenXWA uses at its next start, or the
   original game when it is empty. Writes the error into `message` when it
   can't, and empties it when it can. */
bool XwaModsPage_UseMod(const char* path, char* message, size_t capacity);
/* A path field with a Browse button. Returns nonzero when it changed. */
int XwaModsPage_PathRow(const char* label, XwaModsBrowse browse, const char* title);
/* The height left for the tab's content above the window's Close button. */
float XwaModsPage_ContentHeight(void);
/* The Restart Now button, under a line that asks for a restart. */
void XwaModsPage_RestartButton(void);

#if XWA_HAS_MOD_IMPORT
/* mods_import.c */
void XwaModsImport_Shutdown(void);
/* Checks the form after a change, and joins the import thread once the
   import has finished. */
void XwaModsImport_Update(void);
/* Draws the form, the progress or the summary. */
void XwaModsImport_Draw(void);
void XwaModsImport_DrawModals(void);
/* Opens the import form: empty for a new mod, or filled from `entry` to
   import that mod again. */
void XwaModsImport_OpenForm(const XwaModEntry* entry);
/* Opens the Quit modal while an import runs. */
void XwaModsImport_RequestQuit(void);
/* Asks a running import to stop. */
void XwaModsImport_Cancel(void);
#endif

#endif /* XWA_APP_MODS_PAGE_INTERNAL_H */
