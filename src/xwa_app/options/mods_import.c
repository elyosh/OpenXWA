/* The import part of the Mods tab: the form, the progress of an import and
 * its summary. It is only built with the mod importer (XWA_HAS_MOD_IMPORT). */

#include "aeron/aeron.h"
#include "aeron/time.h"
#include "mod_update.h"
#include "options/mods_page_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Checking the form -------------------------------------------------- */

static void fill_options(bool replace) {
	XwaModsPage* p = &g_xwaModsPage;
	p->options = (XwaModImportOptions) {
		.source = p->source,
		.game = p->game,
		.mod = p->staged ? p->staging : p->mod,
		.name = p->name,
		.authoring = p->keep_authoring ? p->authoring : NULL,
		.resources = p->resources,
		.replace = replace,
	};
}

static void check_source(void) {
	XwaModsPage* p = &g_xwaModsPage;
	XwaModImportInstall install;
	char error[256];
	p->source_ok = 0;
	if (!p->source[0]) {
		snprintf(p->source_message, sizeof p->source_message,
				 "Choose the folder where XWAU or TFTC is installed.");
		return;
	}
	if (p->game[0] && XwaModList_SamePath(p->source, p->game)) {
		snprintf(p->source_message, sizeof p->source_message,
				 "This is OpenXWA's game folder. Choose the folder where XWAU or TFTC is installed.");
		return;
	}
	if (!XwaModImport_Detect(p->source, &install, error, sizeof error)) {
		snprintf(p->source_message, sizeof p->source_message, "%s", error);
		return;
	}
	p->source_ok = 1;
	snprintf(p->source_message, sizeof p->source_message, "Found %s, with %zu models.", install.name,
			 install.models);
	if (!p->name_edited)
		snprintf(p->name, sizeof p->name, "%s", install.name);
}

static void check_fields(void) {
	XwaModsPage* p = &g_xwaModsPage;
	check_source();
	/* The mod goes in the mods folder that OpenXWA lists, or in its own
	   folder when it is imported again. */
	p->mod[0] = '\0';
	if (p->again[0])
		snprintf(p->mod, sizeof p->mod, "%s", p->again);
	else if (p->saved_mods_folder[0] && p->name[0])
		XwaModImport_ModFolder(p->saved_mods_folder, p->name, p->mod, sizeof p->mod);
	if (!p->authoring_edited && p->mod[0])
		snprintf(p->authoring, sizeof p->authoring, "%s-authoring", p->mod);
	/* A usable mod keeps working until the new import is complete: the
	   import goes to its staging folder. */
	XwaModEntry existing;
	p->staged = p->mod[0] && XwaModList_Read(p->mod, &existing) && existing.imported &&
				existing.state == XWA_MOD_READY &&
				XwaModUpdate_StagingFolder(p->mod, p->staging, sizeof p->staging);
	p->check = XWA_MOD_IMPORT_CHECK_INVALID;
	p->check_message[0] = '\0';
	if (!p->name[0]) {
		snprintf(p->check_message, sizeof p->check_message, "Enter a name for the mod.");
	} else if (!p->mod[0]) {
		snprintf(p->check_message, sizeof p->check_message, "Choose a mods folder first.");
	} else if (XwaModUpdate_IsMarkedDelete(p->mod)) {
		snprintf(p->check_message, sizeof p->check_message,
				 "This mod is deleted the next time OpenXWA starts. Cancel the delete to import it again.");
	} else if (!p->staged && p->running_mod[0] && XwaModList_SamePath(p->mod, p->running_mod)) {
		/* OpenXWA reads the files of the mod in use. */
		snprintf(p->check_message, sizeof p->check_message,
				 "OpenXWA is using this mod. To import it again, choose another mod and restart OpenXWA.");
	} else if (p->source_ok) {
		/* A staging folder from an import that didn't finish is replaced
		   without a word. The player confirms the replacement of the mod,
		   or of a new import waiting to replace it. */
		fill_options(p->staged);
		p->check = XwaModImport_Check(&p->options, p->check_message, sizeof p->check_message);
		if (p->staged && p->check == XWA_MOD_IMPORT_CHECK_OK) {
			p->check = XWA_MOD_IMPORT_CHECK_REPLACE;
			snprintf(p->check_message, sizeof p->check_message, "%s",
					 XwaModUpdate_HasUpdate(p->mod)
						 ? "A new import of this mod is ready. Importing again replaces it."
						 : "A mod with this name already exists. Importing replaces it when the import is "
						   "complete.");
		}
	}
	p->checked = 1;
}

/* ---- The import --------------------------------------------------------- */

/* Copies a log line. XwaModImport_ReadLog calls it while it holds the log's
   lock, so it must not log. */
static void add_log_line(void* user, ToolLogLevel level, const char* line) {
	XwaModsPage* p = user;
	(void)level;
	char* copy = malloc(strlen(line) + 1);
	if (!copy)
		return;
	strcpy(copy, line);
	if (p->line_count == XWA_MODS_LOG_LINES) {
		free(p->lines[p->line_start]);
		p->lines[p->line_start] = copy;
		p->line_start = (p->line_start + 1) % XWA_MODS_LOG_LINES;
	} else {
		p->lines[(p->line_start + p->line_count++) % XWA_MODS_LOG_LINES] = copy;
	}
}

static void clear_log(void) {
	XwaModsPage* p = &g_xwaModsPage;
	for (size_t i = 0; i < XWA_MODS_LOG_LINES; ++i) {
		free(p->lines[i]);
		p->lines[i] = NULL;
	}
	p->line_start = p->line_count = 0;
	p->next_line = 0;
	p->log_selected = SIZE_MAX;
}

static int import_thread(void* user) {
	XwaModsPage* p = user;
	XwaModImport_Run(&p->options, &p->progress);
	return 0;
}

static void start_import(bool replace) {
	XwaModsPage* p = &g_xwaModsPage;
	fill_options(replace || p->staged);
	if (p->progress_ready)
		XwaModImport_ProgressFree(&p->progress);
	p->progress_ready = XwaModImport_ProgressInit(&p->progress);
	clear_log();
	p->failure_selected = SIZE_MAX;
	p->done_message[0] = '\0';
	p->done_restart = 0;
	p->start_us = Aeron_NowUs();
	p->thread = p->progress_ready ? Aeron_ThreadCreate("mod import", import_thread, p) : NULL;
	if (!p->thread) {
		snprintf(p->check_message, sizeof p->check_message, "The import couldn't start.");
		return;
	}
	p->view = XWA_MODS_VIEW_RUNNING;
}

/* Joins the import thread once the import has finished. */
static void finish_import(void) {
	XwaModsPage* p = &g_xwaModsPage;
	if (!p->thread || !Aeron_AtomicLoad(&p->progress.finished))
		return;
	Aeron_ThreadJoin(p->thread);
	p->thread = NULL;
	p->end_us = Aeron_NowUs();
	p->result = (XwaModImportResult)Aeron_AtomicLoad(&p->progress.result);
	XwaModImport_ReadLog(&p->progress, &p->next_line, add_log_line, p);
	/* A mod that was just imported is the one that the player wants to
	   play. */
	const bool running = p->running_mod[0] && XwaModList_SamePath(p->mod, p->running_mod);
	if (p->result == XWA_MOD_IMPORT_OK && p->staged && !running &&
		XwaModUpdate_Apply(p->mod) == XWA_MOD_UPDATE_FAILED) {
		/* The list tries again each time it lists the mods. */
		snprintf(p->done_message, sizeof p->done_message,
				 "The new import is ready, but a program is using the folder of the old one. Close it, then "
				 "press Refresh.");
	} else if (p->result == XWA_MOD_IMPORT_OK) {
		if (!p->config_ok)
			snprintf(p->done_message, sizeof p->done_message,
					 "The mod is ready, but OpenXWA can't read config.yaml to select it.");
		else if (XwaModsPage_UseMod(p->mod, p->done_message, sizeof p->done_message)) {
			/* A restart keeps --mod, so it only plays this mod if --mod
			   chose it. */
			p->done_restart = (p->staged && running) || !p->launch_override;
			snprintf(p->done_message, sizeof p->done_message, "%s",
					 p->staged && running ? "Restart OpenXWA to play with the new import."
					 : p->launch_override ? "OpenXWA was started with --mod. Start it without --mod to play "
											"with this mod."
										  : "Restart OpenXWA to play with this mod.");
		}
	}
	p->view = XWA_MODS_VIEW_DONE;
	p->cancel_open = 0;
	p->checked = 0;
	p->mods_scanned = 0;
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

/* ---- The views ---------------------------------------------------------- */

static void draw_form(void) {
	XwaModsPage* p = &g_xwaModsPage;
	AeronUiContext* ui = p->ui;
	char text[XWA_MODS_PATH_CAP + 64];
	if (AeronUi_BeginScroll(ui, "Import form",
							fmaxf(1.0f, XwaModsPage_ContentHeight() - XWA_MODS_BUTTON_ROWS_REF))) {
		if (p->config_message[0])
			AeronUi_Error(ui, p->config_message);
		AeronUi_Header(ui, "Installation");
		if (XwaModsPage_PathRow("XWAU or TFTC Folder", XWA_MODS_BROWSE_SOURCE,
								"Choose the XWAU or TFTC Folder"))
			p->checked = 0;
		if (p->source_ok)
			AeronUi_Label(ui, p->source_message);
		else if (!p->source[0])
			AeronUi_Help(ui, p->source_message);
		else
			AeronUi_Error(ui, p->source_message);

		AeronUi_Spacer(ui, 8.0f);
		AeronUi_Header(ui, "Mod");
		if (AeronUi_InputText(ui, "Mod Name", p->name, sizeof p->name, AERON_UI_INPUT_TEXT_NONE)) {
			p->name_edited = 1;
			p->checked = 0;
		}
		if (p->mod[0]) {
			snprintf(text, sizeof text, "Saved in %s.", p->mod);
			AeronUi_Help(ui, text);
		}
		if (p->staged)
			AeronUi_Help(ui, "The new import is made beside the current one, so it needs as much free space "
							 "as the mod.");
		if (AeronUi_Toggle(ui, "Keep Authoring Files", &p->keep_authoring))
			p->checked = 0;
		AeronUi_Help(ui,
					 "For modders: also saves the models as glTF files and the effect images, to edit them "
					 "in Blender. This needs about as much disk space as the mod.");
		if (p->keep_authoring && XwaModsPage_PathRow("Authoring Folder", XWA_MODS_BROWSE_AUTHORING,
													 "Choose the Authoring Folder")) {
			p->authoring_edited = 1;
			p->checked = 0;
		}
		if (p->check == XWA_MOD_IMPORT_CHECK_REPLACE)
			AeronUi_Help(ui, p->check_message);
		else if (p->check_message[0])
			AeronUi_Error(ui, p->check_message);
		AeronUi_EndScroll(ui);
	}

	AeronUi_Separator(ui);
	AeronUi_BeginColumns(ui, 2, NULL);
	if (AeronUi_ButtonEnabled(ui, "Back", !p->dialog))
		p->view = XWA_MODS_VIEW_LIST;
	AeronUi_NextColumn(ui);
	const int ready = p->checked && p->source_ok && p->check != XWA_MOD_IMPORT_CHECK_INVALID && !p->dialog &&
					  !p->delete_thread;
	if (AeronUi_ButtonEnabled(ui, "Import", ready)) {
		if (p->check == XWA_MOD_IMPORT_CHECK_REPLACE)
			p->replace_open = 1;
		else
			start_import(false);
	}
	AeronUi_EndColumns(ui);
}

static void draw_running(void) {
	XwaModsPage* p = &g_xwaModsPage;
	AeronUiContext* ui = p->ui;
	char line[512], elapsed[64];
	XwaModImport_ReadLog(&p->progress, &p->next_line, add_log_line, p);
	const XwaModImportPhase phase = (XwaModImportPhase)Aeron_AtomicLoad(&p->progress.phase);
	const int done = Aeron_AtomicLoad(&p->progress.done);
	const int total = Aeron_AtomicLoad(&p->progress.total);
	if (total > 0)
		snprintf(line, sizeof line, "%s - %d of %d", XwaModImport_PhaseName(phase), done, total);
	else
		snprintf(line, sizeof line, "%s", XwaModImport_PhaseName(phase));
	AeronUi_Header(ui, line);
	AeronUi_PercentageMeter(ui, "Progress", (float)XwaModImport_Overall(&p->progress) / 1000.0f);
	const uint64_t elapsed_us = Aeron_NowUs() - p->start_us;
	format_time(elapsed, sizeof elapsed, elapsed_us);
	snprintf(line, sizeof line, "Elapsed: %s", elapsed);
	AeronUi_Label(ui, line);
	format_remaining(elapsed, sizeof elapsed, XwaModImport_Remaining(&p->progress, elapsed_us));
	snprintf(line, sizeof line, "Time left: %s", elapsed);
	AeronUi_Label(ui, line);
	const int cancelling = Aeron_AtomicLoad(&p->progress.cancel) != 0;
	if (cancelling)
		AeronUi_Help(ui, p->quit_after_import
							 ? "Stopping the import, then quitting. The current step finishes first."
							 : "Cancelling the import. The current step finishes first.");

	for (size_t i = 0; i < p->line_count; ++i)
		p->items[i] =
			(AeronUiListItem) { .id = i, .label = p->lines[(p->line_start + i) % XWA_MODS_LOG_LINES] };
	const float log_height = fmaxf(120.0f, XwaModsPage_ContentHeight() - XWA_MODS_BUTTON_ROWS_REF);
	AeronUi_ListBoxEx(ui, "Log", p->items, p->line_count, &p->log_selected, log_height, AERON_UI_LIST_FOLLOW);
	AeronUi_Separator(ui);
	if (AeronUi_ButtonEnabled(ui, "Cancel", !cancelling))
		p->cancel_open = 1;
}

static void draw_done(void) {
	XwaModsPage* p = &g_xwaModsPage;
	AeronUiContext* ui = p->ui;
	static const char* const results[] = {
		[XWA_MOD_IMPORT_OK] = "Import Finished",
		[XWA_MOD_IMPORT_CANCELLED] = "Import Cancelled",
		[XWA_MOD_IMPORT_STOPPED] = "Import Stopped",
	};
	char line[XWA_MODS_PATH_CAP + 64], time[64];
	AeronUi_Header(ui, results[p->result]);
	format_time(time, sizeof time, p->end_us - p->start_us);
	snprintf(line, sizeof line, "Time: %s", time);
	AeronUi_Label(ui, line);
	/* Help text wraps, and a folder can be longer than the window. */
	snprintf(line, sizeof line, "Folder: %s", p->mod);
	AeronUi_Help(ui, line);
	if (p->result != XWA_MOD_IMPORT_OK && p->staged)
		AeronUi_Help(ui, p->result == XWA_MOD_IMPORT_CANCELLED
							 ? "The new import isn't finished. The mod stays as it was."
							 : "The import stopped because of an error, and the mod stays as it was. Fix the "
							   "problem below, then import the mod again.");
	else if (p->result == XWA_MOD_IMPORT_CANCELLED)
		AeronUi_Help(ui, "The mod isn't finished. Import it again to finish it.");
	else if (p->result != XWA_MOD_IMPORT_OK)
		AeronUi_Help(ui, "The import stopped because of an error. Fix the problem below, then import the mod "
						 "again.");
	AeronUi_Help(ui, "The full log is in import.log in the mod folder.");

	if (p->result == XWA_MOD_IMPORT_STOPPED && p->progress.error_line_count) {
		AeronUi_Header(ui, "What Went Wrong");
		AeronUi_Help(ui, "The first message usually explains the problem.");
		const size_t count = p->progress.error_line_count < XWA_MODS_LOG_LINES ? p->progress.error_line_count
																			   : XWA_MODS_LOG_LINES;
		for (size_t i = 0; i < count; ++i)
			p->items[i] = (AeronUiListItem) { .id = i, .label = p->progress.error_lines[i] };
		AeronUi_ListBox(ui, "Errors", p->items, count, &p->failure_selected,
						fmaxf(120.0f, XwaModsPage_ContentHeight() - XWA_MODS_BUTTON_ROWS_REF - 60.0f));
	}
	if (p->done_message[0])
		AeronUi_Error(ui, p->done_message);
	if (p->done_restart)
		XwaModsPage_RestartButton();

	AeronUi_Spacer(ui, fmaxf(0.0f, XwaModsPage_ContentHeight() - XWA_MODS_BUTTON_ROWS_REF));
	AeronUi_Separator(ui);
	AeronUi_BeginColumns(ui, 2, NULL);
	if (AeronUi_Button(ui, "Back to Mods"))
		p->view = XWA_MODS_VIEW_LIST;
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Import Another"))
		XwaModsImport_OpenForm(NULL);
	AeronUi_EndColumns(ui);
}

/* ---- The part of the page ----------------------------------------------- */

void XwaModsImport_Shutdown(void) {
	XwaModsPage* p = &g_xwaModsPage;
	if (p->thread) {
		Aeron_AtomicStore(&p->progress.cancel, 1);
		Aeron_ThreadJoin(p->thread);
		p->thread = NULL;
	}
	if (p->progress_ready)
		XwaModImport_ProgressFree(&p->progress);
	p->progress_ready = 0;
	clear_log();
}

void XwaModsImport_Update(void) {
	XwaModsPage* p = &g_xwaModsPage;
	finish_import();
	if (p->quit_after_import && p->view != XWA_MODS_VIEW_RUNNING) {
		p->quit_after_import = 0;
		Aeron_RequestQuit();
	}
	if (p->view == XWA_MODS_VIEW_FORM && !p->checked)
		check_fields();
}

void XwaModsImport_Draw(void) {
	switch (g_xwaModsPage.view) {
		case XWA_MODS_VIEW_FORM:
			draw_form();
			break;
		case XWA_MODS_VIEW_RUNNING:
			draw_running();
			break;
		case XWA_MODS_VIEW_DONE:
			draw_done();
			break;
		case XWA_MODS_VIEW_LIST:
			break;
	}
}

void XwaModsImport_DrawModals(void) {
	XwaModsPage* p = &g_xwaModsPage;
	AeronUiContext* ui = p->ui;
	const AeronUiWindowDesc desc = { .width_ref = 900.0f, .centered = 1 };
	char text[XWA_MODS_PATH_CAP + 512];
	if (p->replace_open && AeronUi_BeginModal(ui, "Replace the Mod", &p->replace_open, &desc)) {
		const bool running = p->running_mod[0] && XwaModList_SamePath(p->mod, p->running_mod);
		if (!p->staged)
			snprintf(text, sizeof text, "%s already exists. Importing it again replaces it.", p->name);
		else if (running)
			snprintf(text, sizeof text,
					 "%s already exists. OpenXWA is using it, so the new import replaces it the next time "
					 "OpenXWA starts.",
					 p->name);
		else
			snprintf(text, sizeof text, "%s already exists. The new import replaces it when it is complete.",
					 p->name);
		AeronUi_Help(ui, text);
		AeronUi_BeginColumns(ui, 2, NULL);
		if (AeronUi_Button(ui, "Cancel"))
			p->replace_open = 0;
		AeronUi_NextColumn(ui);
		if (AeronUi_Button(ui, "Replace")) {
			p->replace_open = 0;
			start_import(true);
		}
		AeronUi_EndColumns(ui);
		AeronUi_EndModal(ui);
	}
	if (p->cancel_open && AeronUi_BeginModal(ui, "Cancel the Import", &p->cancel_open, &desc)) {
		AeronUi_Help(ui, "The mod won't work until you import it again.");
		AeronUi_BeginColumns(ui, 2, NULL);
		if (AeronUi_Button(ui, "Keep Importing"))
			p->cancel_open = 0;
		AeronUi_NextColumn(ui);
		if (AeronUi_Button(ui, "Cancel the Import")) {
			p->cancel_open = 0;
			Aeron_AtomicStore(&p->progress.cancel, 1);
		}
		AeronUi_EndColumns(ui);
		AeronUi_EndModal(ui);
	}
	if (p->quit_open && AeronUi_BeginModal(ui, "Quit", &p->quit_open, &desc)) {
		AeronUi_Help(ui,
					 "An import is running. If you quit now, the mod won't work until you import it again.");
		AeronUi_BeginColumns(ui, 2, NULL);
		if (AeronUi_Button(ui, "Keep Importing"))
			p->quit_open = 0;
		AeronUi_NextColumn(ui);
		if (AeronUi_Button(ui, "Stop and Quit")) {
			p->quit_open = 0;
			p->quit_after_import = 1;
			Aeron_AtomicStore(&p->progress.cancel, 1);
		}
		AeronUi_EndColumns(ui);
		AeronUi_EndModal(ui);
	}
}

void XwaModsImport_Cancel(void) {
	XwaModsPage* p = &g_xwaModsPage;
	if (p->thread) {
		p->cancel_open = 0;
		Aeron_AtomicStore(&p->progress.cancel, 1);
	}
}

void XwaModsImport_OpenForm(const XwaModEntry* entry) {
	XwaModsPage* p = &g_xwaModsPage;
	if (entry) {
		snprintf(p->again, sizeof p->again, "%s", entry->path);
		snprintf(p->source, sizeof p->source, "%s", entry->source);
		snprintf(p->name, sizeof p->name, "%s", entry->name);
		snprintf(p->authoring, sizeof p->authoring, "%s", entry->authoring);
		p->name_edited = 1;
		p->keep_authoring = p->authoring_edited = entry->authoring[0] != '\0';
	} else if (p->again[0]) {
		/* After Import Again, a new mod starts from an empty form. */
		p->again[0] = p->source[0] = p->name[0] = p->authoring[0] = '\0';
		p->name_edited = p->keep_authoring = p->authoring_edited = 0;
	}
	p->view = XWA_MODS_VIEW_FORM;
	p->checked = 0;
}

void XwaModsImport_RequestQuit(void) {
	XwaModsPage* p = &g_xwaModsPage;
	p->quit_open = !p->quit_after_import;
}
