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
#include "host_config.h"
#include "import.h"
#include "mod_list.h"
#include "setup.h"

#define PATH_CAP XWA_HOST_CONFIG_PATH_CAPACITY
#define LOG_LINES 2000
/* The height of a row of buttons and the space above it, in reference
 * pixels. */
#define BUTTON_ROWS_REF 90.0f

typedef enum Screen {
	SCREEN_FORM,
	SCREEN_RUNNING,
	SCREEN_DONE,
} Screen;

/* The field that the folder dialog fills. */
typedef enum Browse {
	BROWSE_SOURCE,
	BROWSE_GAME,
	BROWSE_MOD,
	BROWSE_AUTHORING,
} Browse;

struct ImporterUi {
	AeronUiContext* ui;
	AeronFontAtlas font;
	int tab;
	Screen screen;

	/* The form. The mod folder follows the name, and the authoring folder
	   the mod folder, until the user edits them. */
	char source[PATH_CAP];
	char game[PATH_CAP];
	char name[256];
	char mod[PATH_CAP];
	char authoring[PATH_CAP];
	char resources[PATH_CAP];
	int keep_authoring;
	int name_edited, mod_edited, authoring_edited;
	int checked; /* the fields were checked since they last changed */
	int source_ok, game_ok;
	XwaModImportCheck check;
	char source_message[512];
	char game_message[512];
	char check_message[512];
	int replace_open;

	XwaHostConfig config;
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
	char (*mod_labels)[320];
	AeronUiListItem* mod_items;
	size_t mod_in_use;
	size_t mod_selected;
	int mods_scanned;
	char mods_message[512];
};

/* ---- Configuration ------------------------------------------------------ */

static void load_config(ImporterUi* app) {
	char error[512];
	app->config_ok = XwaHostConfig_Load(Aeron_GetVfs(), &app->config, error, sizeof error);
	if (app->config_ok)
		app->config_message[0] = '\0';
	else
		snprintf(app->config_message, sizeof app->config_message,
				 "OpenXWA's configuration can't be read, so it can't be changed: %s", error);
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
		snprintf(app->source_message, sizeof app->source_message, "Choose the folder of the installation.");
		return;
	}
	if (!XwaModImport_Detect(app->source, &install, error, sizeof error)) {
		snprintf(app->source_message, sizeof app->source_message, "%s", error);
		return;
	}
	app->source_ok = 1;
	snprintf(app->source_message, sizeof app->source_message, "%s - %zu models", install.name,
			 install.models);
	if (!app->name_edited)
		snprintf(app->name, sizeof app->name, "%s", install.name);
}

static void check_game(ImporterUi* app) {
	char normalized[PATH_CAP], error[512], exe[PATH_CAP];
	app->game_ok = 0;
	if (!app->game[0]) {
		snprintf(app->game_message, sizeof app->game_message, "Choose the original game's data folder.");
		return;
	}
	if (!XwaSetup_ValidateGameData(Aeron_GetVfs(), app->game, normalized, sizeof normalized, error,
								   sizeof error)) {
		snprintf(app->game_message, sizeof app->game_message, "%s", error);
		return;
	}
	if (!XwaModImport_FindGameExe(app->game, exe, sizeof exe)) {
		snprintf(app->game_message, sizeof app->game_message,
				 "XWINGALLIANCE.EXE isn't in this folder or in its ALLIANCE folder.");
		return;
	}
	app->game_ok = 1;
	app->game_message[0] = '\0';
	/* OpenXWA then uses the same folder. */
	if (app->config_ok && strcmp(app->game, app->config.game_data_path) != 0) {
		if (XwaHostConfig_SaveGameDataPath(Aeron_GetVfs(), app->game, error, sizeof error))
			snprintf(app->config.game_data_path, sizeof app->config.game_data_path, "%s", app->game);
		else
			snprintf(app->game_message, sizeof app->game_message, "%s", error);
	}
}

static void check_fields(ImporterUi* app) {
	check_source(app);
	check_game(app);
	if (!app->mod_edited && app->game_ok && app->name[0])
		XwaModImport_DefaultModFolder(app->game, app->name, app->mod, sizeof app->mod);
	if (!app->authoring_edited && app->mod[0])
		snprintf(app->authoring, sizeof app->authoring, "%s-authoring", app->mod);
	app->check = XWA_MOD_IMPORT_CHECK_INVALID;
	app->check_message[0] = '\0';
	if (app->source_ok && app->game_ok) {
		fill_options(app, false);
		app->check = XwaModImport_Check(&app->options, app->check_message, sizeof app->check_message);
	}
	app->checked = 1;
}

/* ---- The folder dialog -------------------------------------------------- */

static char* browse_field(ImporterUi* app, Browse browse) {
	return browse == BROWSE_SOURCE ? app->source
		   : browse == BROWSE_GAME ? app->game
		   : browse == BROWSE_MOD  ? app->mod
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
		app->mod_edited |= app->browse == BROWSE_MOD;
		app->authoring_edited |= app->browse == BROWSE_AUTHORING;
		app->checked = 0;
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

/* Joins the import thread once the import has finished. */
static void finish_import(ImporterUi* app) {
	if (!app->thread || !Aeron_AtomicLoad(&app->progress.finished))
		return;
	Aeron_ThreadJoin(app->thread);
	app->thread = NULL;
	app->end_us = Aeron_NowUs();
	app->result = (XwaModImportResult)Aeron_AtomicLoad(&app->progress.result);
	XwaModImport_ReadLog(&app->progress, &app->next_line, add_log_line, app);
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

static void use_mod(ImporterUi* app, const char* path, char* message, size_t capacity) {
	char error[512];
	/* The original game keeps paths.mod, so the mod stays in the list. */
	const int saved =
		path[0] ? XwaHostConfig_SaveModPath(Aeron_GetVfs(), path, 1, error, sizeof error)
				: XwaHostConfig_SaveModPath(Aeron_GetVfs(), app->config.mod_path, 0, error, sizeof error);
	if (saved) {
		snprintf(message, capacity, "OpenXWA will use %s the next time it starts.",
				 path[0] ? "this mod" : "the original game");
		load_config(app);
	} else {
		snprintf(message, capacity, "%s", error);
	}
	app->mods_scanned = 0;
}

/* ---- The Import tab ----------------------------------------------------- */

static void draw_form(ImporterUi* app) {
	AeronUiContext* ui = app->ui;
	if (app->config_message[0])
		AeronUi_Error(ui, app->config_message);
	AeronUi_Header(ui, "Installation");
	AeronUi_Help(ui, "An X-Wing Alliance Upgrade installation, such as XWAU or a TFTC release.");
	if (path_row(app, "Installation", BROWSE_SOURCE, "Choose the XWA Upgrade installation"))
		app->checked = 0;
	if (app->source_ok)
		AeronUi_Label(ui, app->source_message);
	else if (!app->source[0])
		AeronUi_Help(ui, app->source_message);
	else
		AeronUi_Error(ui, app->source_message);
	if (path_row(app, "Original game", BROWSE_GAME, "Choose the original X-Wing Alliance game folder"))
		app->checked = 0;
	if (!app->game[0])
		AeronUi_Help(ui, app->game_message);
	else if (app->game_message[0])
		AeronUi_Error(ui, app->game_message);

	AeronUi_Spacer(ui, 8.0f);
	AeronUi_Header(ui, "Mod");
	if (AeronUi_InputText(ui, "Mod name", app->name, sizeof app->name, AERON_UI_INPUT_TEXT_NONE)) {
		app->name_edited = 1;
		app->checked = 0;
	}
	if (path_row(app, "Mod folder", BROWSE_MOD, "Choose the mod folder")) {
		app->mod_edited = 1;
		app->checked = 0;
	}
	if (AeronUi_Toggle(ui, "Keep authoring files", &app->keep_authoring))
		app->checked = 0;
	AeronUi_Help(ui,
				 "Keeps the glTF models and the effect images, for editing in Blender. Needs about as much "
				 "space again as the mod.");
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
					  app->check != XWA_MOD_IMPORT_CHECK_INVALID && !app->dialog;
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
	format_time(elapsed, sizeof elapsed, Aeron_NowUs() - app->start_us);
	snprintf(line, sizeof line, "Elapsed: %s", elapsed);
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
		[XWA_MOD_IMPORT_FAILURES] = "The import finished, but some items failed.",
		[XWA_MOD_IMPORT_CANCELLED] = "The import was cancelled.",
		[XWA_MOD_IMPORT_STOPPED] = "The import stopped.",
	};
	char line[PATH_CAP + 64], time[64];
	AeronUi_Header(ui, results[app->result]);
	format_time(time, sizeof time, app->end_us - app->start_us);
	snprintf(line, sizeof line, "Time: %s. Failures: %d. Warnings: %d.", time,
			 Aeron_AtomicLoad(&app->progress.failures), Aeron_AtomicLoad(&app->progress.warnings));
	AeronUi_Label(ui, line);
	snprintf(line, sizeof line, "Mod folder: %s", app->mod);
	AeronUi_Label(ui, line);
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

	if (app->progress.failed_count) {
		AeronUi_Header(ui, "Failures");
		const size_t count = app->progress.failed_count < LOG_LINES ? app->progress.failed_count : LOG_LINES;
		for (size_t i = 0; i < count; ++i)
			app->items[i] = (AeronUiListItem) { .id = i, .label = app->progress.failed[i] };
		AeronUi_ListBox(ui, "Failures", app->items, count, &app->failure_selected,
						fmaxf(120.0f, AeronUi_AvailableHeight(ui) - BUTTON_ROWS_REF - 60.0f));
	}
	if (app->done_message[0])
		AeronUi_Help(ui, app->done_message);

	AeronUi_Spacer(ui, fmaxf(0.0f, AeronUi_AvailableHeight(ui) - BUTTON_ROWS_REF));
	AeronUi_Separator(ui);
	AeronUi_BeginColumns(ui, 3, NULL);
	const int usable = app->result == XWA_MOD_IMPORT_OK || app->result == XWA_MOD_IMPORT_FAILURES;
	if (AeronUi_ButtonEnabled(ui, "Use This Mod in OpenXWA", usable && app->config_ok))
		use_mod(app, app->mod, app->done_message, sizeof app->done_message);
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Import Another"))
		app->screen = SCREEN_FORM;
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Quit"))
		Aeron_RequestQuit();
	AeronUi_EndColumns(ui);
}

/* ---- The Mods tab ------------------------------------------------------- */

/* Whether two folder paths are the same, ignoring a final separator. */
static int same_path(const char* a, const char* b) {
	size_t length_a = strlen(a), length_b = strlen(b);
	while (length_a > 1 && (a[length_a - 1] == '/' || a[length_a - 1] == '\\'))
		--length_a;
	while (length_b > 1 && (b[length_b - 1] == '/' || b[length_b - 1] == '\\'))
		--length_b;
	return length_a == length_b && strncmp(a, b, length_a) == 0;
}

static void scan_mods(ImporterUi* app) {
	char current[PATH_CAP] = "";
	XwaModList_Free(&app->mods);
	free(app->mod_labels);
	free(app->mod_items);
	app->mod_labels = NULL;
	app->mod_items = NULL;
	app->mod_in_use = SIZE_MAX;
	app->mods_scanned = 1;
	if (!XwaModList_Scan(app->config.game_data_path, app->config.mod_path, &app->mods)) {
		snprintf(app->mods_message, sizeof app->mods_message, "The mods can't be listed.");
		return;
	}
	app->mod_labels = calloc(app->mods.count, sizeof *app->mod_labels);
	app->mod_items = calloc(app->mods.count, sizeof *app->mod_items);
	if (!app->mod_labels || !app->mod_items) {
		XwaModList_Free(&app->mods);
		return;
	}
	/* The mod that OpenXWA uses, as XwaSetup_SetModRoot picks it without
	   --mod. */
	if (app->config_ok && app->config.use_mod) {
		if (app->config.mod_path[0])
			snprintf(current, sizeof current, "%s", app->config.mod_path);
		else if (app->config.game_data_path[0])
			snprintf(current, sizeof current, "%s/mod", app->config.game_data_path);
	}
	for (size_t i = 0; i < app->mods.count; ++i) {
		const XwaModEntry* entry = &app->mods.entries[i];
		const int in_use = app->config_ok && (entry->path[0] ? current[0] && same_path(entry->path, current)
															 : !app->config.use_mod);
		if (in_use)
			app->mod_in_use = i;
		snprintf(app->mod_labels[i], sizeof app->mod_labels[i], "%s%s%s", entry->name,
				 entry->complete ? "" : " (incomplete)", in_use ? " (in use)" : "");
		app->mod_items[i] = (AeronUiListItem) {
			.id = i,
			.label = app->mod_labels[i],
			.detail = entry->path,
			.flags = entry->complete ? AERON_UI_LIST_ITEM_NONE : AERON_UI_LIST_ITEM_DISABLED,
		};
	}
	if (app->mod_selected >= app->mods.count)
		app->mod_selected = SIZE_MAX;
}

static void draw_mods(ImporterUi* app) {
	AeronUiContext* ui = app->ui;
	if (!app->mods_scanned)
		scan_mods(app);
	if (app->config_message[0])
		AeronUi_Error(ui, app->config_message);
	AeronUi_Help(ui, "The mod that OpenXWA uses. A change takes effect the next time OpenXWA starts.");
	AeronUi_ListBox(ui, "Mods", app->mod_items, app->mod_items ? app->mods.count : 0, &app->mod_selected,
					fmaxf(120.0f, AeronUi_AvailableHeight(ui) - BUTTON_ROWS_REF - 60.0f));
	if (app->mods_message[0])
		AeronUi_Help(ui, app->mods_message);
	AeronUi_Spacer(ui, fmaxf(0.0f, AeronUi_AvailableHeight(ui) - BUTTON_ROWS_REF));
	AeronUi_Separator(ui);
	AeronUi_BeginColumns(ui, 2, NULL);
	const int selectable = app->config_ok && app->mod_selected < app->mods.count &&
						   app->mod_selected != app->mod_in_use &&
						   app->mods.entries[app->mod_selected].complete;
	if (AeronUi_ButtonEnabled(ui, "Use in OpenXWA", selectable))
		use_mod(app, app->mods.entries[app->mod_selected].path, app->mods_message, sizeof app->mods_message);
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Refresh")) {
		app->mods_message[0] = '\0';
		app->mods_scanned = 0;
	}
	AeronUi_EndColumns(ui);
}

/* ---- Modals ------------------------------------------------------------- */

static void draw_modals(ImporterUi* app) {
	AeronUiContext* ui = app->ui;
	const AeronUiWindowDesc desc = { .width_ref = 900.0f, .centered = 1 };
	char text[PATH_CAP + 128];
	if (app->replace_open && AeronUi_BeginModal(ui, "Replace the Mod", &app->replace_open, &desc)) {
		snprintf(text, sizeof text, "%s has a mod made by the importer. Importing deletes it first.",
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
	if (app->cancel_open && AeronUi_BeginModal(ui, "Cancel the Import", &app->cancel_open, &desc)) {
		AeronUi_Help(ui, "The mod folder is left incomplete. Importing into it again replaces it.");
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
		AeronUi_Help(ui, "An import is running. Quitting stops it and leaves the mod folder incomplete.");
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
		if (app->tab != previous)
			app->mods_scanned = 0;
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
		snprintf(error, error_capacity, "The font can't be uploaded.");
		free(app);
		return NULL;
	}
	if (!AeronFontAtlas_LoadVfs(&app->font, command, Aeron_GetVfs(), AERON_VFS_ROOT_RESOURCE,
								"assets/fonts/menu", 64u * 1024u * 1024u)) {
		Aeron_CancelCommandBuffer(command);
		snprintf(error, error_capacity, "The font assets/fonts/menu.{fnt,png} is missing from %s.",
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
	snprintf(app->resources, sizeof app->resources, "%s", Aeron_ResourceRoot());
	load_config(app);
	if (app->config_ok)
		snprintf(app->game, sizeof app->game, "%s", app->config.game_data_path);
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
	if (app->dialog)
		Aeron_DestroyFolderDialog(app->dialog);
	if (app->progress_ready)
		XwaModImport_ProgressFree(&app->progress);
	clear_log(app);
	XwaModList_Free(&app->mods);
	free(app->mod_labels);
	free(app->mod_items);
	AeronUi_Destroy(app->ui);
	AeronFontAtlas_Release(&app->font);
	free(app);
}
