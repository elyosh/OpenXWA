#include "ui.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aeron/aeron.h"
#include "aeron/dialog.h"
#include "aeron/scene/font_atlas.h"
#include "aeron/scene/ui.h"
#include "aeron/sync.h"
#include "aeron/time.h"
#include "import.h"
#include "install_check.h"
#include "install_paths.h"
#include "mod_list.h"

#define PATH_CAP XWA_INSTALL_PATH_CAPACITY
#define LOG_LINES 2000
/* The height of a row of buttons and the space above it, in reference
 * pixels. */
#define BUTTON_ROWS_REF 90.0f
/* How long the Mods tab waits after the last key in the mods folder field
 * before it lists the mods of the new folder. */
#define MODS_FOLDER_DELAY_US 700000u

typedef enum Screen {
	SCREEN_FORM,
	SCREEN_RUNNING,
	SCREEN_DONE,
} Screen;

/* The field that the folder dialog fills. */
typedef enum Browse {
	BROWSE_SOURCE,
	BROWSE_GAME,
	BROWSE_MODS,
	BROWSE_AUTHORING,
	BROWSE_MODS_LIST, /* the mods folder, from the Mods tab */
} Browse;

struct ImporterUi {
	AeronUiContext* ui;
	AeronFontAtlas font;
	int tab;
	Screen screen;

	/* The form. The mod goes in <mods folder>/<name>. The name follows the
	   installation, and the authoring folder the mod folder, until the user
	   edits them. */
	char source[PATH_CAP];
	char game[PATH_CAP];
	char name[256];
	char mods_folder[PATH_CAP];
	char mod[PATH_CAP];
	char authoring[PATH_CAP];
	char resources[PATH_CAP];
	int keep_authoring;
	int name_edited, authoring_edited;
	/* The mods folder that OpenXWA lists, and whether it has mods that the
	   form's other mods folder would leave out of the list. */
	char saved_mods_folder[PATH_CAP];
	int mods_left_behind;
	int checked; /* the fields were checked since they last changed */
	int source_ok, game_ok;
	XwaModImportCheck check;
	char source_message[512];
	char game_message[512];
	char check_message[512];
	int replace_open;

	XwaInstallPaths config;
	int config_ok;
	char config_message[512];

	AeronFolderDialog* dialog;
	Browse browse;

	/* The import. */
	XwaModImportOptions options;
	XwaModImportProgress progress;
	int progress_ready;
	AeronThread* thread;
	uint64_t start_us, end_us;
	unsigned long long next_line;
	char* lines[LOG_LINES]; /* a ring of the last lines */
	size_t line_start, line_count;
	AeronUiListItem items[LOG_LINES];
	size_t log_selected;
	size_t failure_selected;
	int cancel_open, quit_open, quit_after_import;
	XwaModImportResult result;
	char done_message[512];

	/* The Mods tab. */
	XwaModList mods;
	AeronUiListItem* mod_items;
	unsigned char* mod_imported; /* whether the importer made each mod */
	size_t mod_in_use;
	size_t mod_selected;
	int mods_scanned;
	char mods_message[512];
	char mods_folder_message[512]; /* why the mods folder field isn't used */
	char mods_notice[PATH_CAP + 160]; /* the mod that OpenXWA is set to use is missing */
	uint64_t mods_folder_edit_us;    /* when the field was last typed in, or 0 */
	/* Deleting a mod, on its own thread. */
	int delete_open;
	AeronThread* delete_thread;
	volatile int delete_done, delete_ok;
	char delete_path[PATH_CAP];
	char delete_name[256];
};

/* ---- Configuration ------------------------------------------------------ */

static void load_config(ImporterUi* app) {
	char error[512];
	app->config_ok = XwaInstallPaths_Load(Aeron_GetVfs(), &app->config, error, sizeof error);
	if (app->config_ok)
		app->config_message[0] = '\0';
	else
		snprintf(app->config_message, sizeof app->config_message,
				 "OpenXWA's configuration can't be read, so it can't be changed: %s", error);
	if (!XwaModList_Folder(app->config.mods, app->saved_mods_folder, sizeof app->saved_mods_folder))
		app->saved_mods_folder[0] = '\0';
}

/* Whether the mods folder that OpenXWA lists has mods, and the form has
   another one. */
static int mods_left_behind(const ImporterUi* app) {
	XwaModList list;
	if (!app->saved_mods_folder[0] || XwaModList_SamePath(app->mods_folder, app->saved_mods_folder) ||
		!XwaModList_Scan(app->saved_mods_folder, NULL, &list))
		return 0;
	/* The first entry is the original game. */
	const int found = list.count > 1;
	XwaModList_Free(&list);
	return found;
}

/* Makes OpenXWA list the mods of the mods folder field, if it names a
   folder. */
static void apply_mods_folder(ImporterUi* app) {
	char error[512];
	app->mods_folder_edit_us = 0;
	app->mods_folder_message[0] = '\0';
	if (!app->mods_folder[0]) {
		snprintf(app->mods_folder_message, sizeof app->mods_folder_message,
				 "Choose the folder that has your mods.");
		return;
	}
	if (XwaModList_SamePath(app->mods_folder, app->saved_mods_folder))
		return;
	if (!XwaModList_IsFolder(app->mods_folder)) {
		snprintf(app->mods_folder_message, sizeof app->mods_folder_message, "This folder doesn't exist.");
		return;
	}
	if (!app->config_ok) {
		snprintf(app->mods_folder_message, sizeof app->mods_folder_message,
				 "OpenXWA's configuration can't be read, so the mods folder can't be changed.");
		return;
	}
	if (!XwaInstallPaths_SaveMods(Aeron_GetVfs(), app->mods_folder, error, sizeof error)) {
		snprintf(app->mods_folder_message, sizeof app->mods_folder_message,
				 "The mods folder can't be saved in OpenXWA's configuration: %s", error);
		return;
	}
	load_config(app);
	app->mods_scanned = 0;
	app->mod_selected = SIZE_MAX;
	app->mods_message[0] = '\0';
	/* The Import tab's mod folder follows. */
	app->checked = 0;
}

/* ---- Checking the form -------------------------------------------------- */

static void fill_options(ImporterUi* app, bool replace) {
	app->options = (XwaModImportOptions) {
		.source = app->source,
		.game = app->game,
		.mod = app->mod,
		.name = app->name,
		.authoring = app->keep_authoring ? app->authoring : NULL,
		.resources = app->resources,
		.replace = replace,
	};
}

static void check_source(ImporterUi* app) {
	XwaModImportInstall install;
	char error[256];
	app->source_ok = 0;
	if (!app->source[0]) {
		snprintf(app->source_message, sizeof app->source_message,
				 "Choose the folder where XWAU or TFTC is installed.");
		return;
	}
	if (!XwaModImport_Detect(app->source, &install, error, sizeof error)) {
		snprintf(app->source_message, sizeof app->source_message, "%s", error);
		return;
	}
	app->source_ok = 1;
	snprintf(app->source_message, sizeof app->source_message, "Found %s, with %zu models.", install.name,
			 install.models);
	if (!app->name_edited)
		snprintf(app->name, sizeof app->name, "%s", install.name);
}

static void check_game(ImporterUi* app) {
	char normalized[PATH_CAP], error[512];
	app->game_ok = 0;
	if (!app->game[0]) {
		snprintf(app->game_message, sizeof app->game_message, "Choose your original X-Wing Alliance folder.");
		return;
	}
	if (!XwaInstall_ValidateGameData(Aeron_GetVfs(), app->game, normalized, sizeof normalized, error,
									 sizeof error)) {
		snprintf(app->game_message, sizeof app->game_message,
				 "This isn't a complete X-Wing Alliance folder. %s", error);
		return;
	}
	app->game_ok = 1;
	app->game_message[0] = '\0';
	/* OpenXWA then uses the same folder. */
	if (app->config_ok && strcmp(app->game, app->config.game_data) != 0) {
		if (XwaInstallPaths_SaveGameData(Aeron_GetVfs(), app->game, error, sizeof error))
			snprintf(app->config.game_data, sizeof app->config.game_data, "%s", app->game);
		else
			snprintf(app->game_message, sizeof app->game_message, "%s", error);
	}
}

static void check_fields(ImporterUi* app) {
	check_source(app);
	check_game(app);
	app->mod[0] = '\0';
	if (app->mods_folder[0] && app->name[0])
		XwaModImport_ModFolder(app->mods_folder, app->name, app->mod, sizeof app->mod);
	if (!app->authoring_edited && app->mod[0])
		snprintf(app->authoring, sizeof app->authoring, "%s-authoring", app->mod);
	app->mods_left_behind = mods_left_behind(app);
	app->check = XWA_MOD_IMPORT_CHECK_INVALID;
	app->check_message[0] = '\0';
	if (!app->name[0]) {
		snprintf(app->check_message, sizeof app->check_message, "Enter a name for the mod.");
	} else if (!app->mods_folder[0]) {
		snprintf(app->check_message, sizeof app->check_message, "Choose a mods folder.");
	} else if (app->source_ok && app->game_ok) {
		fill_options(app, false);
		app->check = XwaModImport_Check(&app->options, app->check_message, sizeof app->check_message);
	}
	app->checked = 1;
}

/* ---- The folder dialog -------------------------------------------------- */

static char* browse_field(ImporterUi* app, Browse browse) {
	return browse == BROWSE_SOURCE ? app->source
		   : browse == BROWSE_GAME ? app->game
		   : browse == BROWSE_MODS || browse == BROWSE_MODS_LIST ? app->mods_folder
								   : app->authoring;
}

static void open_dialog(ImporterUi* app, Browse browse, const char* title) {
	if (app->dialog)
		return;
	const AeronFolderDialogOptions options = {
		.title = title,
		.accept_label = "Choose",
		.cancel_label = "Cancel",
		.initial_path = browse_field(app, browse)[0] ? browse_field(app, browse) : NULL,
	};
	app->dialog = Aeron_ShowOpenFolderDialog(&options);
	app->browse = browse;
}

static void poll_dialog(ImporterUi* app) {
	char path[PATH_CAP], error[256];
	if (!app->dialog)
		return;
	const AeronFolderDialogStatus status =
		Aeron_PollFolderDialog(app->dialog, path, sizeof path, error, sizeof error);
	if (status == AERON_FOLDER_DIALOG_WAITING)
		return;
	Aeron_DestroyFolderDialog(app->dialog);
	app->dialog = NULL;
	if (status == AERON_FOLDER_DIALOG_SELECTED) {
		snprintf(browse_field(app, app->browse), PATH_CAP, "%s", path);
		app->authoring_edited |= app->browse == BROWSE_AUTHORING;
		app->checked = 0;
		if (app->browse == BROWSE_MODS_LIST)
			apply_mods_folder(app);
	} else if (status == AERON_FOLDER_DIALOG_ERROR) {
		snprintf(app->check_message, sizeof app->check_message, "The folder dialog failed: %s", error);
	}
}

/* A path field with a Browse button. Returns nonzero when it changed. */
static int path_row(ImporterUi* app, const char* label, Browse browse, const char* title) {
	const uint32_t result = AeronUi_InputTextWithAction(app->ui, label, browse_field(app, browse), PATH_CAP,
														AERON_UI_INPUT_TEXT_NONE, "Browse...");
	if (result & AERON_UI_INPUT_TEXT_ACTION_ACTIVATED)
		open_dialog(app, browse, title);
	return (result & AERON_UI_INPUT_TEXT_CHANGED) != 0;
}

/* ---- The import --------------------------------------------------------- */

static void add_log_line(void* user, ToolLogLevel level, const char* line) {
	ImporterUi* app = user;
	(void)level;
	char* copy = malloc(strlen(line) + 1);
	if (!copy)
		return;
	strcpy(copy, line);
	if (app->line_count == LOG_LINES) {
		free(app->lines[app->line_start]);
		app->lines[app->line_start] = copy;
		app->line_start = (app->line_start + 1) % LOG_LINES;
	} else {
		app->lines[(app->line_start + app->line_count++) % LOG_LINES] = copy;
	}
}

static void clear_log(ImporterUi* app) {
	for (size_t i = 0; i < LOG_LINES; ++i) {
		free(app->lines[i]);
		app->lines[i] = NULL;
	}
	app->line_start = app->line_count = 0;
	app->next_line = 0;
	app->log_selected = SIZE_MAX;
}

static int import_thread(void* user) {
	ImporterUi* app = user;
	XwaModImport_Run(&app->options, &app->progress);
	return 0;
}

static void start_import(ImporterUi* app, bool replace) {
	char error[512];
	/* OpenXWA then lists the mods of this folder. */
	if (app->config_ok && !XwaModList_SamePath(app->config.mods, app->mods_folder)) {
		if (!XwaInstallPaths_SaveMods(Aeron_GetVfs(), app->mods_folder, error, sizeof error)) {
			snprintf(app->check_message, sizeof app->check_message,
					 "The mods folder can't be saved in OpenXWA's configuration: %s", error);
			return;
		}
		load_config(app);
		app->mods_left_behind = 0;
		app->mods_scanned = 0;
	}
	fill_options(app, replace);
	if (app->progress_ready)
		XwaModImport_ProgressFree(&app->progress);
	app->progress_ready = XwaModImport_ProgressInit(&app->progress);
	clear_log(app);
	app->failure_selected = SIZE_MAX;
	app->done_message[0] = '\0';
	app->start_us = Aeron_NowUs();
	app->thread = app->progress_ready ? Aeron_ThreadCreate("mod import", import_thread, app) : NULL;
	if (!app->thread) {
		snprintf(app->check_message, sizeof app->check_message, "The import can't start.");
		return;
	}
	app->screen = SCREEN_RUNNING;
}

static void use_mod(ImporterUi* app, const char* path, char* message, size_t capacity) {
	char error[512];
	/* The original game keeps paths.mod, so the mod stays in the list. */
	const int saved = path[0]
						  ? XwaInstallPaths_SaveMod(Aeron_GetVfs(), path, 1, error, sizeof error)
						  : XwaInstallPaths_SaveMod(Aeron_GetVfs(), app->config.mod, 0, error, sizeof error);
	if (saved) {
		snprintf(message, capacity, "OpenXWA will use %s the next time it starts.",
				 path[0] ? "this mod" : "the original game");
		load_config(app);
	} else {
		snprintf(message, capacity, "%s", error);
	}
	app->mods_scanned = 0;
}

/* Joins the import thread once the import has finished. */
static void finish_import(ImporterUi* app) {
	if (!app->thread || !Aeron_AtomicLoad(&app->progress.finished))
		return;
	Aeron_ThreadJoin(app->thread);
	app->thread = NULL;
	app->end_us = Aeron_NowUs();
	app->result = (XwaModImportResult)Aeron_AtomicLoad(&app->progress.result);
	XwaModImport_ReadLog(&app->progress, &app->next_line, add_log_line, app);
	/* A mod that was just imported is the one that the user wants to play. */
	if (app->result == XWA_MOD_IMPORT_OK) {
		if (app->config_ok)
			use_mod(app, app->mod, app->done_message, sizeof app->done_message);
		else
			snprintf(app->done_message, sizeof app->done_message,
					 "OpenXWA's configuration can't be changed, so choose this mod in OpenXWA's mod list.");
	}
	app->screen = SCREEN_DONE;
	app->cancel_open = 0;
	app->checked = 0;
}

static void format_time(char* out, size_t capacity, uint64_t us) {
	const unsigned seconds = (unsigned)(us / 1000000u);
	if (seconds >= 3600)
		snprintf(out, capacity, "%uh %02um %02us", seconds / 3600, seconds / 60 % 60, seconds % 60);
	else
		snprintf(out, capacity, "%um %02us", seconds / 60, seconds % 60);
}

/* Formats the estimated time left, in whole minutes, so that it doesn't
   change on every frame. `us` is -1 when there is no estimate yet. */
static void format_remaining(char* out, size_t capacity, long long us) {
	const long long minutes = (us + 30000000) / 60000000;
	if (us < 0)
		snprintf(out, capacity, "estimating");
	else if (minutes < 1)
		snprintf(out, capacity, "less than a minute");
	else if (minutes < 60)
		snprintf(out, capacity, "about %lld min", minutes);
	else
		snprintf(out, capacity, "about %lldh %02lldm", minutes / 60, minutes % 60);
}

/* ---- The Import tab ----------------------------------------------------- */

static void draw_form(ImporterUi* app) {
	AeronUiContext* ui = app->ui;
	if (app->config_message[0])
		AeronUi_Error(ui, app->config_message);
	AeronUi_Header(ui, "Installations");
	AeronUi_Help(ui,
				 "The XWAU or TFTC installation to import, and your original X-Wing Alliance. They must be "
				 "two different folders.");
	if (path_row(app, "XWAU or TFTC", BROWSE_SOURCE, "Choose the XWAU or TFTC installation"))
		app->checked = 0;
	if (app->source_ok)
		AeronUi_Label(ui, app->source_message);
	else if (!app->source[0])
		AeronUi_Help(ui, app->source_message);
	else
		AeronUi_Error(ui, app->source_message);
	if (path_row(app, "Original game", BROWSE_GAME, "Choose the original X-Wing Alliance folder"))
		app->checked = 0;
	if (!app->game[0])
		AeronUi_Help(ui, app->game_message);
	else if (app->game_message[0])
		AeronUi_Error(ui, app->game_message);
	else if (app->config_ok)
		AeronUi_Help(ui, "OpenXWA uses this folder too.");

	AeronUi_Spacer(ui, 8.0f);
	AeronUi_Header(ui, "Mod");
	if (AeronUi_InputText(ui, "Mod name", app->name, sizeof app->name, AERON_UI_INPUT_TEXT_NONE)) {
		app->name_edited = 1;
		app->checked = 0;
	}
	if (path_row(app, "Mods folder", BROWSE_MODS, "Choose the mods folder"))
		app->checked = 0;
	AeronUi_Help(ui, "Where your mods are saved. OpenXWA lists the mods in this folder.");
	if (app->mods_left_behind)
		AeronUi_Error(ui, "OpenXWA will only list the mods in the new folder. Move your mods there to keep "
						  "them in the list.");
	if (app->mod[0]) {
		char text[PATH_CAP + 64];
		snprintf(text, sizeof text, "This mod is saved in %s.", app->mod);
		AeronUi_Help(ui, text);
	}
	if (AeronUi_Toggle(ui, "Keep authoring files", &app->keep_authoring))
		app->checked = 0;
	AeronUi_Help(ui,
				 "For modders. Also saves the models as glTF files, and the effect images, so you can edit "
				 "them in Blender. This needs about as much disk space as the mod itself.");
	if (app->keep_authoring &&
		path_row(app, "Authoring folder", BROWSE_AUTHORING, "Choose the authoring folder")) {
		app->authoring_edited = 1;
		app->checked = 0;
	}
	if (app->check == XWA_MOD_IMPORT_CHECK_REPLACE)
		AeronUi_Help(ui, app->check_message);
	else if (app->check_message[0])
		AeronUi_Error(ui, app->check_message);

	AeronUi_Spacer(ui, fmaxf(0.0f, AeronUi_AvailableHeight(ui) - BUTTON_ROWS_REF));
	AeronUi_Separator(ui);
	AeronUi_BeginColumns(ui, 2, NULL);
	if (AeronUi_Button(ui, "Quit"))
		Aeron_RequestQuit();
	AeronUi_NextColumn(ui);
	const int ready = app->checked && app->source_ok && app->game_ok &&
					  app->check != XWA_MOD_IMPORT_CHECK_INVALID && !app->dialog && !app->delete_thread;
	if (AeronUi_ButtonEnabled(ui, "Import", ready)) {
		if (app->check == XWA_MOD_IMPORT_CHECK_REPLACE)
			app->replace_open = 1;
		else
			start_import(app, false);
	}
	AeronUi_EndColumns(ui);
}

static void draw_running(ImporterUi* app) {
	AeronUiContext* ui = app->ui;
	char line[512], elapsed[64];
	XwaModImport_ReadLog(&app->progress, &app->next_line, add_log_line, app);
	const XwaModImportPhase phase = (XwaModImportPhase)Aeron_AtomicLoad(&app->progress.phase);
	const int done = Aeron_AtomicLoad(&app->progress.done);
	const int total = Aeron_AtomicLoad(&app->progress.total);
	if (total > 0)
		snprintf(line, sizeof line, "%s - %d of %d", XwaModImport_PhaseName(phase), done, total);
	else
		snprintf(line, sizeof line, "%s", XwaModImport_PhaseName(phase));
	AeronUi_Header(ui, line);
	AeronUi_PercentageMeter(ui, "Progress", (float)XwaModImport_Overall(&app->progress) / 1000.0f);
	const uint64_t elapsed_us = Aeron_NowUs() - app->start_us;
	format_time(elapsed, sizeof elapsed, elapsed_us);
	snprintf(line, sizeof line, "Elapsed: %s", elapsed);
	AeronUi_Label(ui, line);
	format_remaining(elapsed, sizeof elapsed, XwaModImport_Remaining(&app->progress, elapsed_us));
	snprintf(line, sizeof line, "Time left: %s", elapsed);
	AeronUi_Label(ui, line);
	const int cancelling = Aeron_AtomicLoad(&app->progress.cancel) != 0;
	if (cancelling)
		AeronUi_Help(ui, app->quit_after_import
							 ? "Stopping the import, then quitting. The current step finishes "
							   "first."
							 : "Cancelling the import. The current step finishes first.");

	for (size_t i = 0; i < app->line_count; ++i)
		app->items[i] = (AeronUiListItem) { .id = i, .label = app->lines[(app->line_start + i) % LOG_LINES] };
	const float log_height = fmaxf(120.0f, AeronUi_AvailableHeight(ui) - BUTTON_ROWS_REF);
	AeronUi_ListBoxEx(ui, "Log", app->items, app->line_count, &app->log_selected, log_height,
					  AERON_UI_LIST_FOLLOW);
	AeronUi_Separator(ui);
	if (AeronUi_ButtonEnabled(ui, "Cancel", !cancelling))
		app->cancel_open = 1;
}

static void draw_done(ImporterUi* app) {
	AeronUiContext* ui = app->ui;
	static const char* const results[] = {
		[XWA_MOD_IMPORT_OK] = "The import finished.",
		[XWA_MOD_IMPORT_CANCELLED] = "The import was cancelled.",
		[XWA_MOD_IMPORT_STOPPED] = "The import stopped because of an error.",
	};
	char line[PATH_CAP + 64], time[64];
	AeronUi_Header(ui, results[app->result]);
	format_time(time, sizeof time, app->end_us - app->start_us);
	snprintf(line, sizeof line, "Time: %s", time);
	AeronUi_Label(ui, line);
	snprintf(line, sizeof line, "Mod folder: %s", app->mod);
	AeronUi_Label(ui, line);
	const int usable = app->result == XWA_MOD_IMPORT_OK;
	if (app->result == XWA_MOD_IMPORT_CANCELLED)
		AeronUi_Help(ui, "The mod can't be used yet. Import it again into the same folder to finish it.");
	else if (!usable)
		AeronUi_Help(ui, "The mod can't be used. Fix the problem shown below, then import it again into the "
						 "same folder.");
	AeronUi_Help(ui, "The full log is in import.log in the mod folder.");

	AeronUi_BeginColumns(ui, 2, (const float[]) { -480.0f, 1.0f });
	for (int i = 0; i < XWA_MOD_IMPORT_PHASE_COUNT; ++i)
		if (app->progress.phase_ms[i])
			AeronUi_Label(ui, XwaModImport_PhaseName((XwaModImportPhase)i));
	AeronUi_NextColumn(ui);
	for (int i = 0; i < XWA_MOD_IMPORT_PHASE_COUNT; ++i)
		if (app->progress.phase_ms[i]) {
			snprintf(time, sizeof time, "%.1f s", app->progress.phase_ms[i] / 1000.0);
			AeronUi_Label(ui, time);
		}
	AeronUi_EndColumns(ui);

	if (app->result == XWA_MOD_IMPORT_STOPPED && app->progress.error_line_count) {
		AeronUi_Header(ui, "What went wrong");
		AeronUi_Help(ui, "The first message usually explains the problem.");
		const size_t count =
			app->progress.error_line_count < LOG_LINES ? app->progress.error_line_count : LOG_LINES;
		for (size_t i = 0; i < count; ++i)
			app->items[i] = (AeronUiListItem) { .id = i, .label = app->progress.error_lines[i] };
		AeronUi_ListBox(ui, "Errors", app->items, count, &app->failure_selected,
						fmaxf(120.0f, AeronUi_AvailableHeight(ui) - BUTTON_ROWS_REF - 60.0f));
	}
	if (app->done_message[0])
		AeronUi_Help(ui, app->done_message);

	AeronUi_Spacer(ui, fmaxf(0.0f, AeronUi_AvailableHeight(ui) - BUTTON_ROWS_REF));
	AeronUi_Separator(ui);
	AeronUi_BeginColumns(ui, 2, NULL);
	if (AeronUi_Button(ui, "Import Another"))
		app->screen = SCREEN_FORM;
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Quit"))
		Aeron_RequestQuit();
	AeronUi_EndColumns(ui);
}

/* ---- The Mods tab ------------------------------------------------------- */

/* The short status at the right of a mod's row. The details below the list
   say the whole problem. */
static const char* mod_status(const XwaModEntry* entry, int in_use) {
	switch (entry->state) {
	case XWA_MOD_UNFINISHED:
		return "Unfinished";
	case XWA_MOD_OTHER_VERSION:
		return "Other version";
	case XWA_MOD_UNREADABLE:
		return "Broken mod.yaml";
	case XWA_MOD_READY:
		break;
	}
	return in_use ? "In use" : NULL;
}

static void scan_mods(ImporterUi* app) {
	char current[PATH_CAP] = "";
	XwaModList_Free(&app->mods);
	free(app->mod_items);
	free(app->mod_imported);
	app->mod_items = NULL;
	app->mod_imported = NULL;
	app->mod_in_use = SIZE_MAX;
	app->mods_notice[0] = '\0';
	app->mods_scanned = 1;
	if (!XwaModList_Scan(app->saved_mods_folder, app->config.mod, &app->mods)) {
		snprintf(app->mods_message, sizeof app->mods_message, "The mods can't be listed. Try Refresh.");
		return;
	}
	app->mod_items = calloc(app->mods.count, sizeof *app->mod_items);
	app->mod_imported = calloc(app->mods.count, sizeof *app->mod_imported);
	if (!app->mod_items || !app->mod_imported) {
		free(app->mod_items);
		free(app->mod_imported);
		app->mod_items = NULL;
		app->mod_imported = NULL;
		XwaModList_Free(&app->mods);
		return;
	}
	/* The mod that OpenXWA uses, as XwaSetup_SetModRoot picks it without
	   --mod. */
	if (app->config_ok && app->config.use_mod)
		snprintf(current, sizeof current, "%s", app->config.mod);
	for (size_t i = 0; i < app->mods.count; ++i) {
		const XwaModEntry* entry = &app->mods.entries[i];
		const int in_use =
			app->config_ok &&
			(entry->path[0] ? current[0] && XwaModList_SamePath(entry->path, current) : !current[0]);
		if (in_use)
			app->mod_in_use = i;
		app->mod_imported[i] = entry->path[0] && XwaModImport_IsImported(entry->path);
		app->mod_items[i] = (AeronUiListItem) {
			.id = i,
			.label = entry->name,
			.detail = mod_status(entry, in_use),
			.flags = entry->problem[0] ? AERON_UI_LIST_ITEM_DIMMED : AERON_UI_LIST_ITEM_NONE,
		};
	}
	/* The list leaves out a configured mod without a mod.yaml, such as one
	   whose folder was deleted. */
	if (current[0] && app->mod_in_use == SIZE_MAX)
		snprintf(app->mods_notice, sizeof app->mods_notice,
				 "OpenXWA is set to use the mod in %s, but that folder has no mod. OpenXWA starts with the "
				 "original game until you choose another mod.",
				 current);
	if (app->mod_selected >= app->mods.count)
		app->mod_selected = SIZE_MAX;
}

static int delete_thread(void* user) {
	ImporterUi* app = user;
	Aeron_AtomicStore(&app->delete_ok, XwaModImport_Delete(app->delete_path));
	Aeron_AtomicStore(&app->delete_done, 1);
	return 0;
}

static void start_delete(ImporterUi* app) {
	const XwaModEntry* entry = &app->mods.entries[app->mod_selected];
	snprintf(app->delete_path, sizeof app->delete_path, "%s", entry->path);
	snprintf(app->delete_name, sizeof app->delete_name, "%s", entry->name);
	Aeron_AtomicStore(&app->delete_done, 0);
	Aeron_AtomicStore(&app->delete_ok, 0);
	app->delete_thread = Aeron_ThreadCreate("mod delete", delete_thread, app);
	if (app->delete_thread)
		snprintf(app->mods_message, sizeof app->mods_message, "Deleting %s...", app->delete_name);
	else
		snprintf(app->mods_message, sizeof app->mods_message, "%s can't be deleted.", app->delete_name);
}

/* Joins the delete thread once it has finished. */
static void finish_delete(ImporterUi* app) {
	char error[512];
	if (!app->delete_thread || !Aeron_AtomicLoad(&app->delete_done))
		return;
	Aeron_ThreadJoin(app->delete_thread);
	app->delete_thread = NULL;
	if (!Aeron_AtomicLoad(&app->delete_ok)) {
		snprintf(app->mods_message, sizeof app->mods_message,
				 "Some files of %s can't be deleted. Close OpenXWA if it is running and try again, or delete "
				 "the folder yourself with Show Folder.",
				 app->delete_name);
	} else if (app->config_ok && XwaModList_SamePath(app->config.mod, app->delete_path)) {
		/* OpenXWA would warn that the mod is missing at every start. */
		if (XwaInstallPaths_SaveMod(Aeron_GetVfs(), "", 0, error, sizeof error)) {
			snprintf(app->mods_message, sizeof app->mods_message,
					 "%s was deleted. OpenXWA will start with the original game.", app->delete_name);
			load_config(app);
		} else {
			snprintf(app->mods_message, sizeof app->mods_message, "%s was deleted, but %s", app->delete_name,
					 error);
		}
	} else {
		snprintf(app->mods_message, sizeof app->mods_message, "%s was deleted.", app->delete_name);
	}
	app->mods_scanned = 0;
	app->mod_selected = SIZE_MAX;
	/* The Import tab checks the mod folder again. */
	app->checked = 0;
}

static void draw_mods(ImporterUi* app) {
	AeronUiContext* ui = app->ui;
	const AeronUiTheme* theme = AeronUi_GetTheme(ui);
	if (app->mods_folder_edit_us && Aeron_NowUs() - app->mods_folder_edit_us >= MODS_FOLDER_DELAY_US)
		apply_mods_folder(app);
	if (!app->mods_scanned)
		scan_mods(app);
	if (app->config_message[0])
		AeronUi_Error(ui, app->config_message);
	AeronUi_Help(ui,
				 "Choose the mod that OpenXWA uses. The change takes effect the next time OpenXWA starts.");
	if (path_row(app, "Mods folder", BROWSE_MODS_LIST, "Choose the mods folder")) {
		app->mods_folder_edit_us = Aeron_NowUs();
		app->mods_folder_message[0] = '\0';
		app->checked = 0;
	}
	if (app->mods_folder_message[0])
		AeronUi_Error(ui, app->mods_folder_message);
	else
		AeronUi_Help(ui, "OpenXWA lists the mods in this folder.");
	if (app->mods_notice[0])
		AeronUi_Error(ui, app->mods_notice);

	/* Below the list: the name, folder and problem of the selected mod, and
	   a message. A long folder takes more lines. */
	const float line = AeronUi_MeasureHelpHeight(ui, "X", 0.0f) + theme->item_spacing;
	const float details = theme->row_height + theme->item_spacing + 4.0f * line;
	AeronUi_ListBox(ui, "Mods", app->mod_items, app->mod_items ? app->mods.count : 0, &app->mod_selected,
					fmaxf(120.0f, AeronUi_AvailableHeight(ui) - BUTTON_ROWS_REF - details));
	const XwaModEntry* selected =
		app->mod_items && app->mod_selected < app->mods.count ? &app->mods.entries[app->mod_selected] : NULL;
	/* The original game's folder is the game data folder. */
	const char* folder = !selected ? "" : selected->path[0] ? selected->path : app->config.game_data;
	if (selected) {
		char text[PATH_CAP + 64];
		AeronUi_Label(ui, selected->name);
		if (selected->path[0])
			snprintf(text, sizeof text, "Folder: %s", selected->path);
		else if (folder[0])
			snprintf(text, sizeof text, "The game without a mod, from %s.", folder);
		else
			snprintf(text, sizeof text, "The game without a mod.");
		AeronUi_Help(ui, text);
		if (selected->problem[0])
			AeronUi_Error(ui, selected->problem);
	} else if (app->mod_items) {
		AeronUi_Help(ui, "Select a mod to see its folder.");
	}
	if (app->mods_message[0])
		AeronUi_Help(ui, app->mods_message);

	AeronUi_Spacer(ui, fmaxf(0.0f, AeronUi_AvailableHeight(ui) - BUTTON_ROWS_REF));
	AeronUi_Separator(ui);
	AeronUi_BeginColumns(ui, 4, NULL);
	const int idle = !app->delete_thread;
	const int usable = idle && app->config_ok && selected && app->mod_selected != app->mod_in_use &&
					   selected->state == XWA_MOD_READY;
	if (AeronUi_ButtonEnabled(ui, "Use in OpenXWA", usable))
		use_mod(app, selected->path, app->mods_message, sizeof app->mods_message);
	AeronUi_NextColumn(ui);
	if (AeronUi_ButtonEnabled(ui, "Show Folder", idle && folder[0]) && !Aeron_OpenFolder(folder))
		snprintf(app->mods_message, sizeof app->mods_message, "%s can't be opened.", folder);
	AeronUi_NextColumn(ui);
	/* Only a mod that the importer made, as it can be imported again. No
	   import runs meanwhile, as it could be writing to the folder. */
	const int deletable = idle && !app->thread && selected && app->mod_imported[app->mod_selected];
	if (AeronUi_ButtonEnabled(ui, "Delete...", deletable))
		app->delete_open = 1;
	AeronUi_NextColumn(ui);
	if (AeronUi_ButtonEnabled(ui, "Refresh", idle)) {
		app->mods_message[0] = '\0';
		app->mods_scanned = 0;
	}
	AeronUi_EndColumns(ui);
}

/* ---- Modals ------------------------------------------------------------- */

static void draw_modals(ImporterUi* app) {
	AeronUiContext* ui = app->ui;
	const AeronUiWindowDesc desc = { .width_ref = 900.0f, .centered = 1 };
	char text[PATH_CAP + 512];
	if (app->replace_open && AeronUi_BeginModal(ui, "Replace the Mod", &app->replace_open, &desc)) {
		snprintf(text, sizeof text,
				 "%s already has a mod made by the importer. Importing again deletes it and makes a new one.",
				 app->mod);
		AeronUi_Help(ui, text);
		AeronUi_BeginColumns(ui, 2, NULL);
		if (AeronUi_Button(ui, "Cancel"))
			app->replace_open = 0;
		AeronUi_NextColumn(ui);
		if (AeronUi_Button(ui, "Replace")) {
			app->replace_open = 0;
			start_import(app, true);
		}
		AeronUi_EndColumns(ui);
		AeronUi_EndModal(ui);
	}
	if (app->delete_open && AeronUi_BeginModal(ui, "Delete the Mod", &app->delete_open, &desc)) {
		/* The selection can't change while the modal is open. */
		const XwaModEntry* entry =
			app->mod_selected < app->mods.count ? &app->mods.entries[app->mod_selected] : NULL;
		if (entry) {
			snprintf(text, sizeof text,
					 "This deletes %s and its folder, %s. To get it back, import it again from its XWAU or "
					 "TFTC installation.",
					 entry->name, entry->path);
			AeronUi_Help(ui, text);
			if (app->mod_selected == app->mod_in_use)
				AeronUi_Help(ui, "OpenXWA uses this mod, so it will start with the original game.");
		}
		AeronUi_BeginColumns(ui, 2, NULL);
		if (AeronUi_Button(ui, "Cancel"))
			app->delete_open = 0;
		AeronUi_NextColumn(ui);
		if (AeronUi_ButtonEnabled(ui, "Delete", entry != NULL)) {
			app->delete_open = 0;
			start_delete(app);
		}
		AeronUi_EndColumns(ui);
		AeronUi_EndModal(ui);
	}
	if (app->cancel_open && AeronUi_BeginModal(ui, "Cancel the Import", &app->cancel_open, &desc)) {
		AeronUi_Help(ui, "The mod can't be used until it is imported again into the same folder.");
		AeronUi_BeginColumns(ui, 2, NULL);
		if (AeronUi_Button(ui, "Keep Importing"))
			app->cancel_open = 0;
		AeronUi_NextColumn(ui);
		if (AeronUi_Button(ui, "Cancel the Import")) {
			app->cancel_open = 0;
			Aeron_AtomicStore(&app->progress.cancel, 1);
		}
		AeronUi_EndColumns(ui);
		AeronUi_EndModal(ui);
	}
	if (app->quit_open && AeronUi_BeginModal(ui, "Quit", &app->quit_open, &desc)) {
		AeronUi_Help(ui, "An import is running. If you quit, it stops, and the mod can't be used until it is "
						 "imported again.");
		AeronUi_BeginColumns(ui, 2, NULL);
		if (AeronUi_Button(ui, "Keep Importing"))
			app->quit_open = 0;
		AeronUi_NextColumn(ui);
		if (AeronUi_Button(ui, "Stop and Quit")) {
			app->quit_open = 0;
			app->quit_after_import = 1;
			Aeron_AtomicStore(&app->progress.cancel, 1);
		}
		AeronUi_EndColumns(ui);
		AeronUi_EndModal(ui);
	}
}

/* ---- The window --------------------------------------------------------- */

ImporterUiFrame ImporterUi_Frame(ImporterUi* app, float dt_seconds) {
	static const char* const tabs[] = { "Import", "Mods" };
	if (Aeron_QuitRequested()) {
		if (app->screen != SCREEN_RUNNING)
			return IMPORTER_UI_QUIT;
		/* Ask first: the import has to stop before the program ends. */
		Aeron_ClearQuitRequest();
		app->quit_open = !app->quit_after_import;
	}
	poll_dialog(app);
	finish_import(app);
	finish_delete(app);
	if (app->quit_after_import && app->screen != SCREEN_RUNNING)
		return IMPORTER_UI_QUIT;
	if (app->screen == SCREEN_FORM && !app->checked)
		check_fields(app);

	AeronUi_BeginFrame(app->ui, &(AeronUiFrameDesc) {
									.input = Aeron_InputSnapshot(),
									.dt_seconds = dt_seconds,
								});
	const AeronUiWindowDesc window = {
		.width_ref = 1840.0f,
		.height_ref = 1020.0f,
		.centered = 1,
		.flags = AERON_UI_WINDOW_NO_TITLE,
	};
	if (AeronUi_BeginWindow(app->ui, "OpenXWA Mod Importer", &window)) {
		const int previous = app->tab;
		AeronUi_BeginTabBar(app->ui, "tabs", tabs, 2, &app->tab);
		if (app->tab != previous) {
			app->mods_scanned = 0;
			/* Both tabs show the same mods folder, so an edit in the Import
			   tab changes the list too. */
			if (app->tab == 1)
				apply_mods_folder(app);
		}
		if (app->tab == 1)
			draw_mods(app);
		else if (app->screen == SCREEN_FORM)
			draw_form(app);
		else if (app->screen == SCREEN_RUNNING)
			draw_running(app);
		else
			draw_done(app);
		AeronUi_EndTabBar(app->ui);
		AeronUi_EndWindow(app->ui);
	}
	draw_modals(app);
	AeronUi_EndFrame(app->ui);
	return AeronUi_Submit(app->ui) ? IMPORTER_UI_CONTINUE : IMPORTER_UI_FAILED;
}

/* ---- Setup -------------------------------------------------------------- */

ImporterUi* ImporterUi_Create(char* error, size_t error_capacity) {
	ImporterUi* app = calloc(1, sizeof *app);
	if (!app) {
		snprintf(error, error_capacity, "Out of memory.");
		return NULL;
	}
	AeronCommandBuffer* command = Aeron_AcquireCommandBuffer();
	if (!command) {
		snprintf(error, error_capacity, "The window can't be set up.");
		free(app);
		return NULL;
	}
	if (!AeronFontAtlas_LoadKtx2Vfs(&app->font, command, Aeron_GetVfs(), AERON_VFS_ROOT_RESOURCE,
									"assets/fonts/menu", 64u * 1024u * 1024u)) {
		Aeron_CancelCommandBuffer(command);
		snprintf(error, error_capacity,
				 "The importer's font, assets/fonts/menu.fnt and menu.ktx2, is missing from %s. Reinstall "
				 "OpenXWA.",
				 Aeron_ResourceRoot());
		free(app);
		return NULL;
	}
	if (!Aeron_SubmitCommandBuffer(command) || !(app->ui = AeronUi_Create(NULL))) {
		AeronFontAtlas_Release(&app->font);
		snprintf(error, error_capacity, "The window can't be set up.");
		free(app);
		return NULL;
	}
	AeronUi_SetFonts(app->ui, &(AeronUiFontSet) { .regular = &app->font });
	/* The text is drawn 2 to 5 times smaller than the font atlas. A
	   negative bias reads a larger mip level, which keeps it sharper. */
	AeronUiTheme theme = *AeronUi_GetTheme(app->ui);
	theme.text_mip_bias = -0.75f;
	AeronUi_SetTheme(app->ui, &theme);
	snprintf(app->resources, sizeof app->resources, "%s", Aeron_ResourceRoot());
	load_config(app);
	if (app->config_ok)
		snprintf(app->game, sizeof app->game, "%s", app->config.game_data);
	snprintf(app->mods_folder, sizeof app->mods_folder, "%s", app->saved_mods_folder);
	app->log_selected = SIZE_MAX;
	app->failure_selected = SIZE_MAX;
	app->mod_selected = SIZE_MAX;
	return app;
}

void ImporterUi_Destroy(ImporterUi* app) {
	if (!app)
		return;
	if (app->thread) {
		Aeron_AtomicStore(&app->progress.cancel, 1);
		Aeron_ThreadJoin(app->thread);
	}
	/* A deletion can't stop halfway, so it finishes first. */
	if (app->delete_thread)
		Aeron_ThreadJoin(app->delete_thread);
	if (app->dialog)
		Aeron_DestroyFolderDialog(app->dialog);
	if (app->progress_ready)
		XwaModImport_ProgressFree(&app->progress);
	clear_log(app);
	XwaModList_Free(&app->mods);
	free(app->mod_items);
	free(app->mod_imported);
	AeronUi_Destroy(app->ui);
	AeronFontAtlas_Release(&app->font);
	free(app);
}
