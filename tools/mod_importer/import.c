#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_time.h>

#include "aeron/sync.h"
#include "aeron/time.h"
#include "bake_xwau.h"
#include "bake_xwau_flight.h"
#include "effect_library.h"
#include "env_bake.h"
#include "envmap.h"
#include "import_internal.h"
#include "material_checks.h"
#include "stock_tables.h"
#include "xwa_app/mod_format.h"
#include "xwau_awards.h"
#include "xwau_craft.h"
#include "xwau_cube_faces.h"
#include "xwau_flight_settings.h"
#include "xwau_game_files.h"
#include "xwau_gunner_seats.h"
#include "xwau_hangars.h"
#include "xwau_library.h"
#include "xwau_mission_files.h"
#include "xwau_missions.h"
#include "xwau_objects.h"
#include "xwau_skyboxes.h"
#include "xwau_sound_banks.h"
#include "xwau_text.h"

#define MOD_YAML_HEADER "# Made by the OpenXWA mod importer."

/* How much of the overall progress each phase takes, in percent: the share
 * of each phase in the times of TFTC-R and XWAU2025 imports. */
static const int kPhaseWeights[XWA_MOD_IMPORT_PHASE_COUNT] = {
	[XWA_MOD_IMPORT_MODELS] = 19,  [XWA_MOD_IMPORT_EFFECTS] = 20,  [XWA_MOD_IMPORT_CRAFT] = 1,
	[XWA_MOD_IMPORT_MISSIONS] = 1, [XWA_MOD_IMPORT_SKYBOXES] = 21, [XWA_MOD_IMPORT_FRONTEND] = 10,
	[XWA_MOD_IMPORT_FLIGHT] = 27,  [XWA_MOD_IMPORT_COPIES] = 1,
};

static const char* const kPhaseNames[XWA_MOD_IMPORT_PHASE_COUNT] = {
	[XWA_MOD_IMPORT_PREPARE] = "Preparing",
	[XWA_MOD_IMPORT_MODELS] = "Converting models",
	[XWA_MOD_IMPORT_EFFECTS] = "Converting effects",
	[XWA_MOD_IMPORT_CRAFT] = "Importing craft settings",
	[XWA_MOD_IMPORT_MISSIONS] = "Importing missions",
	[XWA_MOD_IMPORT_SKYBOXES] = "Converting skyboxes",
	[XWA_MOD_IMPORT_HANGARS] = "Importing hangars",
	[XWA_MOD_IMPORT_FRONTEND] = "Converting menu art",
	[XWA_MOD_IMPORT_FLIGHT] = "Converting flight art",
	[XWA_MOD_IMPORT_COPIES] = "Copying files",
	[XWA_MOD_IMPORT_FINISH] = "Finishing",
};

const char* XwaModImport_PhaseName(XwaModImportPhase phase) {
	return phase >= 0 && phase < XWA_MOD_IMPORT_PHASE_COUNT ? kPhaseNames[phase] : "";
}

int XwaModImport_Overall(XwaModImportProgress* progress) {
	if (Aeron_AtomicLoad(&progress->finished))
		return 1000;
	const int phase = Aeron_AtomicLoad(&progress->phase);
	int done = 0;
	for (int i = 0; i < phase && i < XWA_MOD_IMPORT_PHASE_COUNT; ++i)
		done += kPhaseWeights[i] * 10;
	if (phase >= 0 && phase < XWA_MOD_IMPORT_PHASE_COUNT)
		done += kPhaseWeights[phase] * Aeron_AtomicLoad(&progress->weight) / 100;
	return done < 1000 ? done : 1000;
}

/* ---- Shared helpers ----------------------------------------------------- */

bool Import_Cancelled(ImportContext* ctx) { return Aeron_AtomicLoad(&ctx->progress->cancel) != 0; }

bool Import_ShouldStop(ImportContext* ctx) {
	return Import_Cancelled(ctx) || Aeron_AtomicLoad(&ctx->progress->failures) != 0 || ReportFatalMessage();
}

static void end_phase(ImportContext* ctx) {
	const int phase = Aeron_AtomicLoad(&ctx->progress->phase);
	const uint64_t now = Aeron_NowUs();
	ctx->progress->phase_ms[phase] += (int)((now - ctx->phase_start) / 1000);
	ctx->phase_start = now;
}

void Import_BeginPhase(ImportContext* ctx, XwaModImportPhase phase, int total) {
	end_phase(ctx);
	Aeron_AtomicStore(&ctx->progress->done, 0);
	Aeron_AtomicStore(&ctx->progress->total, total);
	Aeron_AtomicStore(&ctx->progress->weight, 0);
	Aeron_AtomicStore(&ctx->progress->phase, phase);
	ToolLog_Write(TOOL_LOG_INFO, "== %s", kPhaseNames[phase]);
}

void Import_Advance(ImportContext* ctx, int items, int weight) {
	const int done = Aeron_AtomicAdd(&ctx->progress->done, items) + items;
	const int total = Aeron_AtomicLoad(&ctx->progress->total);
	if (weight < 0)
		weight = total > 0 ? (int)((long long)done * 1000 / total) : 0;
	Aeron_AtomicStore(&ctx->progress->weight, weight < 1000 ? weight : 1000);
}

static void list_failed(ImportContext* ctx, const char* format, va_list args) {
	XwaModImportProgress* progress = ctx->progress;
	char text[1024];
	vsnprintf(text, sizeof text, format, args);
	char* copy = StrDup(text);
	char** grown = copy ? realloc(progress->failed, (progress->failed_count + 1) * sizeof *grown) : NULL;
	if (!grown) {
		free(copy);
		return;
	}
	progress->failed = grown;
	progress->failed[progress->failed_count++] = copy;
}

void Import_Failed(ImportContext* ctx, const char* format, ...) {
	va_list args;
	va_start(args, format);
	ToolLog_WriteV(TOOL_LOG_ERROR, format, args);
	va_end(args);
	va_start(args, format);
	list_failed(ctx, format, args);
	va_end(args);
	Aeron_AtomicAdd(&ctx->progress->failures, 1);
}

void Import_ListFailed(ImportContext* ctx, const char* format, ...) {
	va_list args;
	va_start(args, format);
	list_failed(ctx, format, args);
	va_end(args);
}

bool Import_CopyFile(const char* from, const char* to) {
	FILE* in = fopen(from, "rb");
	FILE* out = in ? fopen(to, "wb") : NULL;
	bool ok = out != NULL;
	enum { CHUNK = 1 << 20 };
	char* buffer = ok ? malloc(CHUNK) : NULL;
	ok = buffer != NULL;
	size_t length;
	while (ok && (length = fread(buffer, 1, CHUNK, in)) > 0)
		ok = fwrite(buffer, 1, length, out) == length;
	ok = ok && !ferror(in);
	free(buffer);
	if (in)
		fclose(in);
	if (out && fclose(out) != 0)
		ok = false;
	if (!ok && out)
		remove(to);
	return ok;
}

bool Import_Rename(const char* from, const char* to) { return rename(from, to) == 0; }

bool Import_FindNoCase(const char* dir, const char* name, bool want_dir, char* out, size_t cap) {
	if (!PathFormat(out, cap, "%s/%s", dir, name))
		return false;
	if (want_dir ? IsDir(out) : IsFile(out))
		return true;
	size_t count = 0;
	char** names = ListDir(dir, true, &count);
	bool found = false;
	for (size_t i = 0; !found && i < count; ++i)
		found = strcasecmp(names[i], name) == 0 && PathFormat(out, cap, "%s/%s", dir, names[i]) &&
				(want_dir ? IsDir(out) : IsFile(out));
	FreeList(names, count);
	if (!found)
		out[0] = '\0';
	return found;
}

/* ---- Detection and checks ----------------------------------------------- */

/* Line `number` (from 1) of the file `name` in folder `dir`, trimmed.
 * Returns false when it is missing or empty. */
static bool read_line(const char* dir, const char* name, int number, char* out, size_t cap) {
	char path[IMPORT_PATH_CAP];
	size_t size = 0;
	char* text = Import_FindNoCase(dir, name, false, path, sizeof path) ? ReadWholeFile(path, &size) : NULL;
	if (!text)
		return false;
	const char* line = text;
	if (strncmp(line, "\xEF\xBB\xBF", 3) == 0)
		line += 3;
	for (int i = 1; i < number && line; ++i) {
		line = strchr(line, '\n');
		line = line ? line + 1 : NULL;
	}
	size_t length = 0;
	if (line) {
		while (*line == ' ' || *line == '\t')
			++line;
		length = strcspn(line, "\r\n");
		while (length && isspace((unsigned char)line[length - 1]))
			--length;
		snprintf(out, cap, "%.*s", (int)length, line);
	}
	free(text);
	return length > 0;
}

/* The number of .opt files in the FlightModels folder of `dir`. */
static size_t count_opts(const char* dir) {
	char models[IMPORT_PATH_CAP];
	if (!Import_FindNoCase(dir, "FlightModels", true, models, sizeof models))
		return 0;
	size_t count = 0, opts = 0;
	char** names = ListDir(models, false, &count);
	for (size_t i = 0; i < count; ++i)
		opts += EndsWithNoCase(names[i], ".opt");
	FreeList(names, count);
	return opts;
}

bool XwaModImport_Detect(const char* source, XwaModImportInstall* info, char* error, size_t error_cap) {
	char path[IMPORT_PATH_CAP];
	memset(info, 0, sizeof *info);
	if (!source || !source[0] || !IsDir(source)) {
		snprintf(error, error_cap, "This folder doesn't exist.");
		return false;
	}
	info->models = count_opts(source);
	if (!Import_FindNoCase(source, "XWINGALLIANCE.EXE", false, path, sizeof path) || !info->models ||
		(!Import_FindNoCase(source, "Hooks.ini", false, path, sizeof path) &&
		 !Import_FindNoCase(source, "XWAUVersion.txt", false, path, sizeof path))) {
		snprintf(error, error_cap,
				 "This isn't an XWAU or TFTC installation. Choose the folder that has XWINGALLIANCE.EXE and "
				 "the FlightModels folder.");
		return false;
	}
	if (!read_line(source, "TFTCVersion.txt", 1, info->name, sizeof info->name) &&
		!read_line(source, "mod-version-XWAU-XWAU.txt", 2, info->name, sizeof info->name) &&
		!read_line(source, "XWAUVersion.txt", 1, info->name, sizeof info->name)) {
		size_t length = strlen(source);
		while (length > 1 && (source[length - 1] == '/' || source[length - 1] == '\\'))
			--length;
		size_t start = length;
		while (start && source[start - 1] != '/' && source[start - 1] != '\\')
			--start;
		snprintf(info->name, sizeof info->name, "%.*s", (int)(length - start), source + start);
	}
	return true;
}

bool XwaModImport_ModFolder(const char* mods, const char* name, char* out, size_t cap) {
	char folder[256];
	LowerCopy(folder, sizeof folder, name);
	for (char* c = folder; *c; ++c)
		if (!isalnum((unsigned char)*c) && *c != '.' && *c != '-')
			*c = '-';
	const size_t end = strlen(mods);
	const bool separator = end && (mods[end - 1] == '/' || mods[end - 1] == '\\');
	return PathFormat(out, cap, "%s%s%s", mods, separator ? "" : "/", folder[0] ? folder : "mod");
}

typedef enum FolderState {
	FOLDER_EMPTY,    /* missing, or with only the files that the OS adds */
	FOLDER_IMPORTED, /* holds a mod made by the importer */
	FOLDER_OTHER,
} FolderState;

static FolderState folder_state(const char* dir) {
	char path[IMPORT_PATH_CAP];
	size_t count = 0;
	char** names = ListDir(dir, false, &count);
	bool empty = true;
	for (size_t i = 0; empty && i < count; ++i)
		empty = strcmp(names[i], ".DS_Store") == 0 || strcmp(names[i], "Thumbs.db") == 0 ||
				strcmp(names[i], "desktop.ini") == 0 || strncmp(names[i], "._", 2) == 0;
	FreeList(names, count);
	if (empty)
		return FOLDER_EMPTY;
	size_t size = 0;
	char* text = PathFormat(path, sizeof path, "%s/mod.yaml", dir) ? ReadWholeFile(path, &size) : NULL;
	bool ours = text && strncmp(text, MOD_YAML_HEADER, strlen(MOD_YAML_HEADER)) == 0 &&
				strchr("\r\n", text[strlen(MOD_YAML_HEADER)]);
	free(text);
	return ours ? FOLDER_IMPORTED : FOLDER_OTHER;
}

/* Whether `path` is `dir` or inside it. */
static bool inside(const char* path, const char* dir) {
	size_t length = strlen(dir);
	while (length > 1 && dir[length - 1] == '/')
		--length;
	return strncmp(path, dir, length) == 0 && (path[length] == '\0' || path[length] == '/');
}

XwaModImportCheck XwaModImport_Check(const XwaModImportOptions* options, char* error, size_t error_cap) {
	XwaModImportInstall install;
	char path[IMPORT_PATH_CAP], detect[256];
	if (!XwaModImport_Detect(options->source, &install, detect, sizeof detect)) {
		snprintf(error, error_cap, "Installation: %s", detect);
		return XWA_MOD_IMPORT_CHECK_INVALID;
	}
	if (!options->game || !IsDir(options->game) ||
		!Import_FindNoCase(options->game, "FlightModels", true, path, sizeof path)) {
		snprintf(error, error_cap, "Original game: this isn't an X-Wing Alliance folder.");
		return XWA_MOD_IMPORT_CHECK_INVALID;
	}
	if (!options->resources || !PathFormat(path, sizeof path, "%s/assets/fonts", options->resources) ||
		!IsDir(path)) {
		snprintf(error, error_cap,
				 "The importer's resources/assets/fonts folder is missing. Reinstall OpenXWA.");
		return XWA_MOD_IMPORT_CHECK_INVALID;
	}
	if (!options->mod || !options->mod[0]) {
		snprintf(error, error_cap, "Choose a mod folder.");
		return XWA_MOD_IMPORT_CHECK_INVALID;
	}
	if (options->authoring &&
		(inside(options->authoring, options->mod) || inside(options->mod, options->authoring))) {
		snprintf(error, error_cap,
				 "The authoring folder and the mod folder must be separate: one can't be inside the other.");
		return XWA_MOD_IMPORT_CHECK_INVALID;
	}
	if (IsFile(options->mod)) {
		snprintf(error, error_cap, "The mod folder is a file. Choose a folder.");
		return XWA_MOD_IMPORT_CHECK_INVALID;
	}
	switch (folder_state(options->mod)) {
		case FOLDER_EMPTY:
			return XWA_MOD_IMPORT_CHECK_OK;
		case FOLDER_IMPORTED:
			if (options->replace)
				return XWA_MOD_IMPORT_CHECK_OK;
			snprintf(error, error_cap,
					 "The mod folder has a mod made by the importer. Importing replaces it.");
			return XWA_MOD_IMPORT_CHECK_REPLACE;
		case FOLDER_OTHER:
			break;
	}
	snprintf(error, error_cap,
			 "The mod folder already has other files in it. Choose another mod name or mods folder.");
	return XWA_MOD_IMPORT_CHECK_INVALID;
}

/* ---- Progress ----------------------------------------------------------- */

bool XwaModImport_ProgressInit(XwaModImportProgress* progress) {
	memset(progress, 0, sizeof *progress);
	progress->log = ImportLog_CreateRing();
	return progress->log != NULL;
}

void XwaModImport_ProgressFree(XwaModImportProgress* progress) {
	for (size_t i = 0; i < progress->failed_count; ++i)
		free(progress->failed[i]);
	free(progress->failed);
	for (size_t i = 0; i < progress->error_line_count; ++i)
		free(progress->error_lines[i]);
	free(progress->error_lines);
	ImportLog_FreeRing(progress->log);
	memset(progress, 0, sizeof *progress);
}

/* ---- Prepare ------------------------------------------------------------ */

/* Writes `text` as a YAML double-quoted string. */
static void yaml_quoted(FILE* f, const char* text) {
	fputc('"', f);
	for (const char* c = text; *c; ++c) {
		if (*c == '"' || *c == '\\')
			fputc('\\', f);
		if ((unsigned char)*c >= 0x20)
			fputc(*c, f);
	}
	fputc('"', f);
}

static bool write_mod_yaml(ImportContext* ctx, bool complete) {
	const XwaModImportOptions* options = ctx->options;
	char path[IMPORT_PATH_CAP];
	SDL_Time now;
	SDL_DateTime date = { 0 };
	if (SDL_GetCurrentTime(&now))
		SDL_TimeToDateTime(now, &date, true);
	FILE* f = PathFormat(path, sizeof path, "%s/mod.yaml", options->mod) ? fopen(path, "w") : NULL;
	if (!f)
		return false;
	fprintf(f, MOD_YAML_HEADER "\nversion: %d\nname: ", XWA_MOD_FORMAT_VERSION);
	yaml_quoted(f, options->name && options->name[0] ? options->name : "Mod");
	fprintf(f, "\ncomplete: %s\nsource: ", complete ? "true" : "false");
	yaml_quoted(f, options->source);
	fprintf(f, "\nimported: \"%04d-%02d-%02d\"\n", date.year, date.month, date.day);
	return fclose(f) == 0;
}

/* Deletes what the mod folder holds. */
static bool clear_folder(const char* dir) {
	size_t count = 0;
	char** names = ListDir(dir, false, &count);
	bool ok = true;
	for (size_t i = 0; i < count; ++i) {
		char path[IMPORT_PATH_CAP];
		ok = PathFormat(path, sizeof path, "%s/%s", dir, names[i]) && RemoveTree(path) && ok;
	}
	FreeList(names, count);
	return ok;
}

static bool prepare(ImportContext* ctx) {
	const XwaModImportOptions* options = ctx->options;
	char error[512], path[IMPORT_PATH_CAP];
	const XwaModImportCheck check = XwaModImport_Check(options, error, sizeof error);
	if (check != XWA_MOD_IMPORT_CHECK_OK) {
		Import_Failed(ctx, "%s", error);
		return false;
	}
	if (folder_state(options->mod) == FOLDER_IMPORTED) {
		ToolLog_Write(TOOL_LOG_INFO, "Deleting the mod in %s", options->mod);
		if (!clear_folder(options->mod)) {
			Import_Failed(ctx, "The old mod in %s can't be deleted.", options->mod);
			return false;
		}
	}
	if (!MakeDirs(options->mod) || !write_mod_yaml(ctx, false)) {
		Import_Failed(ctx, "The mod folder %s can't be written to.", options->mod);
		return false;
	}
	if (!PathFormat(path, sizeof path, "%s/import.log", options->mod) ||
		!ImportLog_SetFile(ctx->progress, path))
		ToolLog_Write(TOOL_LOG_WARNING, "Can't write %s", path);
	if (!PathFormat(ctx->work, sizeof ctx->work, "%s/.import", options->mod) || !RemoveTree(ctx->work) ||
		!MakeDirs(ctx->work)) {
		Import_Failed(ctx, "The folder %s can't be created.", ctx->work);
		return false;
	}
	ToolLog_Write(TOOL_LOG_INFO, "Importing %s into %s", options->source, options->mod);
	return true;
}

/* ---- Effect libraries --------------------------------------------------- */

/* Places the library sources and effects.yaml that were written into
 * <work>/effects in the authoring folder, except the ones that the user
 * changed there. */
static void place_libraries(ImportContext* ctx, ImportAuthoring* authoring) {
	char effects[IMPORT_PATH_CAP];
	size_t count = 0;
	char** names =
		PathFormat(effects, sizeof effects, "%s/effects", ctx->work) ? ListDir(effects, true, &count) : NULL;
	for (size_t i = 0; i < count; ++i) {
		char staged[IMPORT_PATH_CAP], rel[IMPORT_PATH_CAP];
		if (!PathFormat(staged, sizeof staged, "%s/%s", effects, names[i]) ||
			!PathFormat(rel, sizeof rel, "effects/%s", names[i]))
			continue;
		const ImportAuthoringState state = ImportAuthoring_State(authoring, rel);
		if (state == IMPORT_AUTHORING_EDITED)
			ToolLog_Write(TOOL_LOG_DETAIL, "%s: edited since its import, kept", rel);
		else if (state == IMPORT_AUTHORING_AUTHORED)
			ToolLog_Write(TOOL_LOG_DETAIL, "%s: not made by the importer, kept", rel);
		else {
			cJSON* files = ImportAuthoring_Place(authoring, staged, rel);
			if (files)
				ImportAuthoring_Set(authoring, rel, files);
			else
				Import_Failed(ctx, "%s can't be placed in the authoring folder.", rel);
		}
	}
	FreeList(names, count);
	ImportAuthoring_Save(authoring);
}

/* Builds the effect libraries from the models' .gltf files, then checks the
 * materials of every cooked model against them. */
/* Builds the libraries from their sources in the authoring folder, after
 * writing there the ones that the imported models name. */
static void build_sources(ImportContext* ctx, ImportAuthoring* authoring, const char* out, size_t cooked) {
	const XwaModImportOptions* options = ctx->options;
	char sources[IMPORT_PATH_CAP], path[IMPORT_PATH_CAP], library[IMPORT_PATH_CAP];
	if (!PathFormat(sources, sizeof sources, "%s/effects", options->authoring))
		return;
	/* Every model of the authoring folder is read, as it is the project that
	 * the mod comes from. */
	const char* const inputs[] = { options->authoring };
	if (!XwauLibrary_Rebuild(options->source, ctx->work, inputs, 1))
		Import_Failed(ctx, "The effect libraries can't be written.");
	else
		place_libraries(ctx, authoring);

	/* The authoring folder keeps the libraries of earlier imports, so they
	 * are built even when this import couldn't write its own. */
	size_t names_count = 0;
	char** names = ListDir(sources, true, &names_count);
	size_t libraries = 0;
	for (size_t i = 0; i < names_count; ++i)
		libraries += PathFormat(library, sizeof library, "%s/%s/%s.yaml", sources, names[i], names[i]) &&
					 IsFile(library);
	Aeron_AtomicStore(&ctx->progress->total, (int)(libraries + cooked));

	for (size_t i = 0; i < names_count && !Import_ShouldStop(ctx); ++i) {
		if (!PathFormat(library, sizeof library, "%s/%s/%s.yaml", sources, names[i], names[i]) ||
			!IsFile(library))
			continue;
		if (EffectLibrary_BuildLibrary(IMPORT_LIBRARY_TAG, library, NULL, out) == EFFECT_LIBRARY_FAILED)
			Import_Failed(ctx, "The effect library %s couldn't be built.", names[i]);
		Import_Advance(ctx, 1, -1);
	}
	FreeList(names, names_count);
	if (PathFormat(path, sizeof path, "%s/effects.yaml", sources) && IsFile(path)) {
		char target[IMPORT_PATH_CAP];
		if (!PathFormat(target, sizeof target, "%s/effects.yaml", out) || !Import_CopyFile(path, target))
			Import_Failed(ctx, "%s can't be copied.", path);
	}
}

/* Builds the effect libraries, then checks the materials of every cooked
 * model against them. Without an authoring folder, the libraries are made
 * in memory from the models' .gltf files and built straight into the mod. */
static void effects(ImportContext* ctx, ImportModel* models, size_t count, ImportAuthoring* authoring) {
	const XwaModImportOptions* options = ctx->options;
	char out[IMPORT_PATH_CAP], path[IMPORT_PATH_CAP];
	if (!PathFormat(out, sizeof out, "%s/flight/effects", options->mod) || !MakeDirs(out)) {
		Import_Failed(ctx, "The folder %s can't be created.", out);
		return;
	}
	size_t cooked = 0;
	for (size_t i = 0; i < count; ++i)
		cooked += models[i].cooked;
	if (authoring) {
		build_sources(ctx, authoring, out, cooked);
	} else {
		const char** inputs = calloc(count + 1, sizeof *inputs);
		size_t input_count = 0;
		for (size_t i = 0; inputs && i < count; ++i)
			if (models[i].gltf)
				inputs[input_count++] = models[i].gltf;
		if (inputs)
			ImportEffects_Build(ctx, inputs, input_count, out, cooked);
		else
			Import_Failed(ctx, "The computer ran out of memory.");
		free(inputs);
	}

	/* A model whose materials name what the libraries don't have isn't
	 * kept, as its cook would have failed. */
	for (size_t i = 0; i < count && !Import_ShouldStop(ctx); ++i) {
		ImportModel* model = &models[i];
		if (!model->cooked)
			continue;
		if (!aeron_gltf_cook_check_materials(model->gltf, out, NULL)) {
			if (PathFormat(path, sizeof path, "%s/flight/models/%s.glb", options->mod, model->key))
				remove(path);
			model->cooked = false;
			Import_Failed(ctx, "The model %s uses materials that the effect libraries don't have.",
						  model->key);
		}
		Import_Advance(ctx, 1, -1);
	}
}

/* ---- The other phases --------------------------------------------------- */

/* The craft settings, object and craft types, texts and sound banks. */
static void craft(ImportContext* ctx) {
	const XwaModImportOptions* options = ctx->options;
	char flight[IMPORT_PATH_CAP], strings[IMPORT_PATH_CAP], game_files[IMPORT_PATH_CAP];
	if (!PathFormat(flight, sizeof flight, "%s/flight", options->mod) ||
		!PathFormat(strings, sizeof strings, "%s/text/strings.yaml", options->mod) ||
		!PathFormat(game_files, sizeof game_files, "%s/game", options->mod))
		return;
	XwauGameTables* stock = ImportStockTables_Create(options->game);
	if (!stock) {
		Import_Failed(ctx, "The original game's ship lists couldn't be read.");
		return;
	}
	XwauGameTables* mod = XwauGameTables_Load(options->source);
	/* Rename the object types first, because every mod file uses their
	   names. */
	if (!XwauObjects_Rename(options->source, options->game, stock, mod, &ctx->names)) {
		XwauGameTables_Free(mod);
		XwauGameTables_Free(stock);
		Import_Failed(ctx, "The computer ran out of memory.");
		return;
	}
	XwauLines cockpits = { 0 };
	XwauTextIds moved = { 0 };
	bool ok = XwauCraft_Write(options->source, flight, &ctx->names, &ctx->stock_values, &cockpits);
	ok = XwauObjects_Write(options->source, options->game, stock, mod, &ctx->names, flight, &moved) && ok;
	XwauGunnerSeats_CheckCockpits(options->source, flight, &cockpits);
	XwauGameTables_Free(mod);
	XwauGameTables_Free(stock);
	XwauLines_Free(&cockpits);
	ok = XwauText_Write(options->source, options->game, strings, &moved) && ok;
	ok = XwauSoundBanks_Write(options->source, &ctx->names, flight, game_files) && ok;
	XwauTextIds_Free(&moved);
	if (!ok)
		Import_Failed(ctx, "The craft settings couldn't be imported.");
}

/* The missions' files and settings, the awards, and the skybox list. Returns
 * false when the missions can't be loaded. */
static bool missions(ImportContext* ctx, bool* skyboxes) {
	const XwaModImportOptions* options = ctx->options;
	char game_files[IMPORT_PATH_CAP], flight[IMPORT_PATH_CAP], awards[IMPORT_PATH_CAP], out[IMPORT_PATH_CAP];
	char environments[IMPORT_PATH_CAP], manifest[IMPORT_PATH_CAP], folders[IMPORT_PATH_CAP];
	*skyboxes = false;
	if (!PathFormat(game_files, sizeof game_files, "%s/game", options->mod) ||
		!PathFormat(flight, sizeof flight, "%s/flight/flight.yaml", options->mod) ||
		!PathFormat(awards, sizeof awards, "%s/frontend/awards.yaml", options->mod) ||
		!PathFormat(out, sizeof out, "%s/flight/missions", options->mod) ||
		!PathFormat(environments, sizeof environments, "%s/flight/environments", options->mod) ||
		!PathFormat(manifest, sizeof manifest, "%s/environments.yaml", environments) ||
		!PathFormat(folders, sizeof folders, "%s/skybox_folders.txt", ctx->work))
		return true;
	if (!XwauGameFiles_Copy(options->source, options->game, game_files) ||
		!XwauFlightSettings_Write(options->source, &ctx->names, &ctx->stock_values, flight) ||
		!XwauAwards_Write(options->source, awards)) {
		Import_Failed(ctx, "The mission files couldn't be imported.");
		return true;
	}
	XwauMissions* loaded = XwauMissions_Load(options->source, true);
	if (!loaded) {
		Import_Failed(ctx, "The missions can't be read.");
		return false;
	}
	const int design_scale[2] = { BAKE_XWAU_DESIGN_SCALE_X, BAKE_XWAU_DESIGN_SCALE_Y };
	const bool ok =
		XwauMissionFiles_Write(options->source, loaded, options->game, out, &ctx->names, design_scale);
	XwauMissions_Free(loaded);
	if (!ok) {
		Import_Failed(ctx, "The mission files couldn't be imported.");
		return true;
	}
	if (!MakeDirs(environments) || !XwauSkyboxes_Write(options->source, options->game, manifest, folders))
		Import_Failed(ctx, "The skybox list couldn't be imported.");
	else
		*skyboxes = true;
	return true;
}

enum { SKY_MAX_FACE_SIZE = 4096 };

/* Returns the face size of a baked sky or overlay: the largest power of two
 * that isn't larger than the source faces, up to SKY_MAX_FACE_SIZE. */
static int sky_face_size(int source_size) {
	int size = 16;
	while (size * 2 <= source_size && size < SKY_MAX_FACE_SIZE)
		size *= 2;
	return size;
}

/* Converts each sky and overlay folder of the skybox list. Each line is
 * "sky" or "overlay", the source folder, the environment name ("-" for an
 * overlay) and the texture folder in the mod folder, separated by tabs. */
static void bake_skyboxes(ImportContext* ctx) {
	char path[IMPORT_PATH_CAP];
	size_t size = 0;
	char* text =
		PathFormat(path, sizeof path, "%s/skybox_folders.txt", ctx->work) ? ReadWholeFile(path, &size) : NULL;
	int total = 0, baked = 0;
	for (const char* c = text; c && *c; ++c)
		total += *c == '\n';
	Aeron_AtomicStore(&ctx->progress->total, total);
	const int jobs = ctx->cores < ENVMAP_MAX_JOBS ? ctx->cores : ENVMAP_MAX_JOBS;
	for (char* line = text; line && *line && !Import_ShouldStop(ctx);) {
		char* end = strchr(line, '\n');
		if (end)
			*end = '\0';
		char* fields[4] = { line };
		for (int i = 1; i < 4 && fields[i - 1]; ++i) {
			fields[i] = strchr(fields[i - 1], '\t');
			if (fields[i])
				*fields[i]++ = '\0';
		}
		char textures[IMPORT_PATH_CAP], radiance[IMPORT_PATH_CAP], diffuse[IMPORT_PATH_CAP],
			specular[IMPORT_PATH_CAP], overlay[IMPORT_PATH_CAP];
		if (fields[3] && PathFormat(textures, sizeof textures, "%s/%s", ctx->options->mod, fields[3]) &&
			PathFormat(radiance, sizeof radiance, "%s/sky.ktx2", textures) &&
			PathFormat(diffuse, sizeof diffuse, "%s/diffuse.ktx2", textures) &&
			PathFormat(specular, sizeof specular, "%s/specular.ktx2", textures) &&
			PathFormat(overlay, sizeof overlay, "%s/overlay.ktx2", textures) && MakeDirs(textures)) {
			const bool is_overlay = strcmp(fields[0], "overlay") == 0;
			const char* folder = strrchr(fields[3], '/') ? strrchr(fields[3], '/') + 1 : fields[3];
			/* The bake reads the XWAU faces in its own face layout. */
			XwauCubeFaces faces;
			char err[512];
			const bool read = XwauCubeFaces_Read(fields[1], &faces, err, sizeof err);
			if (!read)
				ToolLog_Write(TOOL_LOG_ERROR, "%s", err);
			const CubeFacePixels pixels = { fields[1],
											faces.size,
											{ faces.rgba[0], faces.rgba[1], faces.rgba[2], faces.rgba[3],
											  faces.rgba[4], faces.rgba[5] } };
			EnvBakeOptions bake;
			EnvBake_DefaultOptions(&bake);
			bake.face_pixels = &pixels;
			bake.radiance_size = sky_face_size(faces.size);
			if (is_overlay) {
				bake.overlay_out = overlay;
			} else {
				bake.name = fields[2];
				bake.radiance_out = radiance;
				bake.diffuse_out = diffuse;
				bake.specular_out = specular;
				bake.jobs = jobs;
			}
			if (read && (is_overlay ? EnvBake_Overlay(&bake) : EnvBake_Sky(&bake)) == ENV_BAKE_OK)
				baked++;
			else
				Import_Failed(ctx,
							  is_overlay ? "The overlay %s couldn't be converted."
										 : "The skybox %s couldn't be converted.",
							  folder);
			if (read)
				XwauCubeFaces_Free(&faces);
		} else if (line[0])
			Import_Failed(ctx, "The skybox list has a line that can't be read: \"%s\"", line);
		Import_Advance(ctx, 1, -1);
		line = end ? end + 1 : NULL;
	}
	free(text);
	ToolLog_Write(TOOL_LOG_INFO, "Converted %d skybox and overlay folders", baked);
}

static void bake_frontend(ImportContext* ctx) {
	BakeSpriteOptions sprites = { .out_root = ctx->options->mod,
								  .write_ktx2 = true,
								  .zstd = true,
								  .scale = true,
								  .bc7_quality = KTX2_BC7_QUALITY_MED,
								  .group_filter = -1 };
	const BakeXwauOptions options = { .sprites = &sprites,
									  .xwau_root = ctx->options->source,
									  .stock_root = ctx->options->game };
	BakeXwauStats stats = { 0 };
	const int ok = BakeXwau_RunFrontend(&options, &stats);
	ToolLog_Write(
		TOOL_LOG_INFO,
		"Frontend art: %d atlases, %d HD frames, %d frames upscaled from standard images, %d files left "
		"to the original game's art, %d failures",
		stats.atlases, stats.hd_frames, stats.sd_frames, stats.unchanged, stats.failures);
	if (!ok)
		Import_Failed(ctx, "The menu art couldn't be converted.");
}

static void bake_flight(ImportContext* ctx) {
	BakeSpriteOptions sprites = { .out_root = ctx->options->mod,
								  .write_ktx2 = true,
								  .zstd = true,
								  .scale = true,
								  .bc7_quality = KTX2_BC7_QUALITY_MED,
								  .group_filter = -1 };
	const BakeXwauFlightOptions options = { .sprites = &sprites,
											.xwau_root = ctx->options->source,
											.planet_max = 0 };
	BakeXwauFlightStats stats = { 0 };
	const int ok = BakeXwau_RunFlight(&options, &stats);
	ToolLog_Write(
		TOOL_LOG_INFO,
		"Flight art: %d atlases with %d images, %d alternative frame sets, %d mission art folders, %d "
		"flight font tiers, %d failures",
		stats.atlases, stats.images, stats.frame_sets, stats.mission_art, stats.font_tiers, stats.failures);
	if (!ok)
		Import_Failed(ctx, "The flight art couldn't be converted.");
}

/* ---- The import --------------------------------------------------------- */

static void summary(ImportContext* ctx, XwaModImportResult result, uint64_t start) {
	XwaModImportProgress* progress = ctx->progress;
	const int seconds = (int)((Aeron_NowUs() - start) / 1000000);
	static const char* const results[] = { "Import finished", "Import cancelled", "Import stopped" };
	ToolLog_Write(TOOL_LOG_INFO, "%s in %dm %02ds: %d failures, %d warnings", results[result], seconds / 60,
				  seconds % 60, Aeron_AtomicLoad(&progress->failures), Aeron_AtomicLoad(&progress->warnings));
	for (int i = 0; i < XWA_MOD_IMPORT_PHASE_COUNT; ++i)
		if (progress->phase_ms[i])
			ToolLog_Write(TOOL_LOG_INFO, "  %s: %.1f s", kPhaseNames[i], progress->phase_ms[i] / 1000.0);
	if (progress->failed_count)
		ToolLog_Write(TOOL_LOG_INFO, "Failed:");
	for (size_t i = 0; i < progress->failed_count; ++i)
		ToolLog_Write(TOOL_LOG_INFO, "  %s", progress->failed[i]);
}

/* Runs the phases, and returns how the import ended. */
static XwaModImportResult run_phases(ImportContext* ctx, ImportAuthoring* authoring) {
	const XwaModImportOptions* options = ctx->options;
	ImportModel* models = NULL;
	size_t count = 0;
	if (!prepare(ctx) || (options->authoring && !ImportAuthoring_Load(authoring, options->authoring)) ||
		!ImportModels_List(ctx, &models, &count))
		return XWA_MOD_IMPORT_STOPPED;

	Import_BeginPhase(ctx, XWA_MOD_IMPORT_MODELS, (int)count);
	const bool converted = ImportModels_Run(ctx, models, count, options->authoring ? authoring : NULL);
	for (size_t i = 0; i < count; ++i) {
		if (models[i].failed)
			Import_ListFailed(ctx, "The model %s couldn't be imported: %s.", models[i].key, models[i].failed);
		if (models[i].files) {
			char rel[IMPORT_PATH_CAP];
			if (PathFormat(rel, sizeof rel, "models/%s", models[i].key)) {
				ImportAuthoring_Set(authoring, rel, models[i].files);
				models[i].files = NULL;
			}
		}
	}
	if (options->authoring)
		ImportAuthoring_Save(authoring);
	if (converted && !Import_ShouldStop(ctx)) {
		Import_BeginPhase(ctx, XWA_MOD_IMPORT_EFFECTS, 0);
		effects(ctx, models, count, options->authoring ? authoring : NULL);
	}
	ImportModels_Free(models, count);
	if (!converted)
		return XWA_MOD_IMPORT_STOPPED;

	bool skyboxes = false;
	SetReportProgram("xwau");
	if (!Import_ShouldStop(ctx)) {
		Import_BeginPhase(ctx, XWA_MOD_IMPORT_CRAFT, 0);
		craft(ctx);
	}
	if (!Import_ShouldStop(ctx)) {
		Import_BeginPhase(ctx, XWA_MOD_IMPORT_MISSIONS, 0);
		if (!missions(ctx, &skyboxes))
			return XWA_MOD_IMPORT_STOPPED;
	}
	if (!Import_ShouldStop(ctx)) {
		Import_BeginPhase(ctx, XWA_MOD_IMPORT_SKYBOXES, 0);
		if (skyboxes)
			bake_skyboxes(ctx);
	}
	if (!Import_ShouldStop(ctx)) {
		Import_BeginPhase(ctx, XWA_MOD_IMPORT_HANGARS, 0);
		XwauGameTables* stock = ImportStockTables_Create(options->game);
		if (!stock || !XwauHangars_Write(options->source, stock, &ctx->names, options->game, options->mod))
			Import_Failed(ctx, "The hangars couldn't be imported.");
		XwauGameTables_Free(stock);
	}
	if (!Import_ShouldStop(ctx)) {
		Import_BeginPhase(ctx, XWA_MOD_IMPORT_FRONTEND, 0);
		bake_frontend(ctx);
	}
	if (!Import_ShouldStop(ctx)) {
		Import_BeginPhase(ctx, XWA_MOD_IMPORT_FLIGHT, 0);
		bake_flight(ctx);
	}
	if (!Import_ShouldStop(ctx)) {
		Import_BeginPhase(ctx, XWA_MOD_IMPORT_COPIES, 0);
		ImportCopies_Run(ctx);
	}
	if (Import_ShouldStop(ctx))
		return XWA_MOD_IMPORT_STOPPED;
	Import_BeginPhase(ctx, XWA_MOD_IMPORT_FINISH, 0);
	if (!write_mod_yaml(ctx, true)) {
		Import_Failed(ctx, "%s/mod.yaml can't be written.", options->mod);
		return XWA_MOD_IMPORT_STOPPED;
	}
	return XWA_MOD_IMPORT_OK;
}

XwaModImportResult XwaModImport_Run(const XwaModImportOptions* options, XwaModImportProgress* progress) {
	ImportContext ctx = { .options = options, .progress = progress };
	ImportStockTables_Names(&ctx.names);
	ImportStockTables_Values(&ctx.stock_values);
	ImportAuthoring authoring = { 0 };
	const uint64_t start = Aeron_NowUs();
	ctx.phase_start = start;
	ctx.cores = SDL_GetNumLogicalCPUCores();
	if (ctx.cores < 1)
		ctx.cores = 1;
	ImportLog_Open(progress, options);
	ClearReportFatal();

	XwaModImportResult result = run_phases(&ctx, &authoring);
	if (ReportFatalMessage()) {
		Import_ListFailed(&ctx, "The mod can't be imported as it is: %s.", ReportFatalMessage());
		Aeron_AtomicAdd(&progress->failures, 1);
		result = XWA_MOD_IMPORT_STOPPED;
	}
	if (result == XWA_MOD_IMPORT_OK)
		Import_Advance(&ctx, 0, 1000);
	else if (result == XWA_MOD_IMPORT_STOPPED && Import_Cancelled(&ctx))
		result = XWA_MOD_IMPORT_CANCELLED;
	if (ctx.work[0] && !RemoveTree(ctx.work))
		ToolLog_Write(TOOL_LOG_WARNING, "Can't delete %s", ctx.work);
	ImportAuthoring_Free(&authoring);
	XwauObjects_FreeRenames(&ctx.names);
	end_phase(&ctx);
	ReportUnsupportedSummary();
	summary(&ctx, result, start);
	SetReportProgram(NULL);
	ImportLog_Close(progress);
	Aeron_AtomicStore(&progress->result, result);
	Aeron_AtomicStore(&progress->finished, 1);
	return result;
}
