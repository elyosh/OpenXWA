/*
 * xwa_mod_import imports an XWA Upgrade installation, such as XWAU or a TFTC
 * release, into an OpenXWA mod folder. It is the command-line form of the
 * OpenXWA Mod Importer, for development.
 *
 * Usage:
 *   xwa_mod_import --source DIR --mod DIR [--game DIR] [--name TEXT]
 *                  [--authoring DIR] [--replace] [--details]
 *                  [--jobs N] [--zstd-level N] [--only KEY]...
 *
 *   --source DIR     the installation
 *   --mod DIR        the mod folder. If it isn't empty, it must hold a mod
 *                    made by the importer, and --replace must be given
 *   --game DIR       the original game's data folder (default: paths.game_data
 *                    in OpenXWA's config.yaml)
 *   --name TEXT      the mod's name (default: the installation's own name)
 *   --authoring DIR  keep the glTF models and the effect library sources in
 *                    DIR, and cook the mod from them. Entries edited there
 *                    since the last import are kept
 *   --replace        replace the mod in the mod folder
 *   --details        also log the details of the work, such as a line for
 *                    each converted model
 *   --jobs N         models converted at once (default: half the CPU cores)
 *   --zstd-level N   zstd level of the models' textures, 1 to 22 (default 9)
 *   --only KEY       only import this model, named by its OPT name in lower
 *                    case (repeatable)
 *
 * The log goes to stderr, and to <mod>/import.log. Ctrl+C cancels the
 * import. The import stops at the first failure. The exit code is 0 when the
 * import finished, 1 when it stopped or was cancelled, and 2 for a usage
 * error.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include "aeron/sync.h"
#include "import.h"
#include "tool_log.h"
#include "util.h"
#include "yaml_json.h"

static int usage(void) {
	fprintf(stderr, "usage: xwa_mod_import --source DIR --mod DIR [--game DIR] [--name TEXT]\n"
					"                      [--authoring DIR] [--replace] [--details]\n"
					"                      [--jobs N] [--zstd-level N] [--only KEY]...\n");
	return 2;
}

static void echo(void* user, ToolLogLevel level, const char* line) {
	(void)user;
	(void)level;
	fputs(line, stderr);
	fputc('\n', stderr);
}

static XwaModImportProgress g_progress;

static void interrupted(int signal_number) {
	(void)signal_number;
	Aeron_AtomicStore(&g_progress.cancel, 1);
	/* A second Ctrl+C ends the program. */
	signal(SIGINT, SIG_DFL);
}

/* Reads paths.game_data from OpenXWA's config.yaml into `out`. */
static bool configured_game(char* out, size_t cap) {
	char path[4096], err[512];
	char* user = SDL_GetPrefPath("TotallyOpen", "OpenXWA");
	if (!user || !PathFormat(path, sizeof path, "%sconfig.yaml", user)) {
		SDL_free(user);
		return false;
	}
	SDL_free(user);
	cJSON* config = IsFile(path) ? YamlJson_Load(path, err, sizeof err) : NULL;
	const cJSON* game =
		cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(config, "paths"), "game_data");
	const bool found =
		cJSON_IsString(game) && game->valuestring[0] && PathFormat(out, cap, "%s", game->valuestring);
	cJSON_Delete(config);
	return found;
}

/* OpenXWA's resource folder: the one beside the program, else the one in
 * the source tree. */
static bool resource_folder(char* out, size_t cap) {
	char fonts[4096];
	const char* base = SDL_GetBasePath();
	if (base && PathFormat(out, cap, "%sresources", base) &&
		PathFormat(fonts, sizeof fonts, "%s/assets/fonts", out) && IsDir(fonts))
		return true;
	return PathFormat(out, cap, "%s", XWA_MOD_IMPORT_RESOURCES);
}

static bool parse_int(const char* text, int low, int high, int* out) {
	char* end = NULL;
	const long value = text ? strtol(text, &end, 10) : 0;
	if (!text || end == text || *end || value < low || value > high)
		return false;
	*out = (int)value;
	return true;
}

int main(int argc, char** argv) {
	XwaModImportOptions options = { .echo = echo };
	char game[4096], resources[4096], name[256];
	char** only = calloc((size_t)argc, sizeof *only);
	size_t only_count = 0;
	if (!only)
		return 1;
	for (int i = 1; i < argc; ++i) {
		const char* arg = argv[i];
		const char* value = i + 1 < argc ? argv[i + 1] : NULL;
		if (strcmp(arg, "--replace") == 0) {
			options.replace = true;
			continue;
		}
		if (strcmp(arg, "--details") == 0) {
			ToolLog_SetDetails(1);
			continue;
		}
		if (!value) {
			fprintf(stderr, "xwa_mod_import: %s needs a value\n", arg);
			return usage();
		}
		++i;
		if (strcmp(arg, "--source") == 0)
			options.source = value;
		else if (strcmp(arg, "--mod") == 0)
			options.mod = value;
		else if (strcmp(arg, "--game") == 0)
			options.game = value;
		else if (strcmp(arg, "--name") == 0)
			options.name = value;
		else if (strcmp(arg, "--authoring") == 0)
			options.authoring = value;
		else if (strcmp(arg, "--jobs") == 0 && parse_int(value, 1, 256, &options.jobs))
			continue;
		else if (strcmp(arg, "--zstd-level") == 0 && parse_int(value, 1, 22, &options.zstd_level))
			continue;
		else if (strcmp(arg, "--only") == 0) {
			if (!(only[only_count] = StrDup(value)))
				return 1;
			LowerInPlace(only[only_count++]);
		} else {
			fprintf(stderr, "xwa_mod_import: bad option %s %s\n", arg, value);
			return usage();
		}
	}
	options.only = (const char* const*)only;
	options.only_count = only_count;
	if (!options.source || !options.mod)
		return usage();
	if (!options.game) {
		if (!configured_game(game, sizeof game)) {
			fprintf(stderr, "xwa_mod_import: give --game, as OpenXWA's config.yaml has no paths.game_data\n");
			return 2;
		}
		options.game = game;
	}
	if (!resource_folder(resources, sizeof resources))
		return 1;
	options.resources = resources;

	XwaModImportInstall install;
	char error[512];
	if (!XwaModImport_Detect(options.source, &install, error, sizeof error)) {
		fprintf(stderr, "xwa_mod_import: %s: %s\n", options.source, error);
		return 2;
	}
	if (!options.name) {
		snprintf(name, sizeof name, "%s", install.name);
		options.name = name;
	}
	const XwaModImportCheck check = XwaModImport_Check(&options, error, sizeof error);
	if (check != XWA_MOD_IMPORT_CHECK_OK) {
		fprintf(stderr, "xwa_mod_import: %s%s\n", error,
				check == XWA_MOD_IMPORT_CHECK_REPLACE ? " Give --replace to replace it." : "");
		return 2;
	}

	if (!XwaModImport_ProgressInit(&g_progress))
		return 1;
	fprintf(stderr, "xwa_mod_import: %s, %zu models\n", install.name, install.models);
	signal(SIGINT, interrupted);
	const XwaModImportResult result = XwaModImport_Run(&options, &g_progress);
	signal(SIGINT, SIG_DFL);
	XwaModImport_ProgressFree(&g_progress);
	for (size_t i = 0; i < only_count; ++i)
		free(only[i]);
	free(only);
	return result == XWA_MOD_IMPORT_OK ? 0 : 1;
}
