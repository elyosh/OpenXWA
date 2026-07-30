#include "setup.h"

#include <stdio.h>
#include <string.h>

static int setup_error(char* error, size_t error_size, const char* format, const char* detail) {
	if (error && error_size) {
		snprintf(error, error_size, format, detail ? detail : "");
	}
	return 0;
}

static int setup_copy_path(char* destination, size_t capacity, const char* path, const char* option,
						   char* error, size_t error_size) {
	if (!path || !path[0]) {
		return setup_error(error, error_size, "%s requires a non-empty path", option);
	}
	if (strlen(path) >= capacity) {
		return setup_error(error, error_size, "%s path is too long", option);
	}
	snprintf(destination, capacity, "%s", path);
	return 1;
}

static int setup_append_argument(char* command_line, size_t capacity, const char* argument) {
	const size_t used = strlen(command_line);
	const size_t length = strlen(argument);

	if (used + length + (used != 0) + 1 > capacity) {
		return 0;
	}
	if (used != 0) {
		command_line[used] = ' ';
		command_line[used + 1] = '\0';
	}
	strcat(command_line, argument);
	return 1;
}

static const char* setup_option_value(const char* argument, const char* option) {
	const size_t length = strlen(option);
	if (strncmp(argument, option, length) == 0 && argument[length] == '=') {
		return argument + length + 1;
	}
	return NULL;
}

int XwaLaunchOptions_Parse(int argc, char** argv, XwaLaunchOptions* out, char* error, size_t error_size) {
	int pass_through = 0;
	int i;

	if (!out) {
		return setup_error(error, error_size, "%s", "invalid launch options");
	}
	memset(out, 0, sizeof *out);
	for (i = 1; i < argc; ++i) {
		const char* value;
		if (!pass_through && strcmp(argv[i], "--") == 0) {
			pass_through = 1;
			continue;
		}
		value = pass_through ? NULL : setup_option_value(argv[i], "--game-data");
		if (!pass_through && (value || strcmp(argv[i], "--game-data") == 0)) {
			if (!value) {
				if (++i >= argc) {
					return setup_error(error, error_size, "%s requires a path", "--game-data");
				}
				value = argv[i];
			}
			if (!setup_copy_path(out->game_data_path, sizeof out->game_data_path, value, "--game-data", error,
								 error_size)) {
				return 0;
			}
			continue;
		}
		value = pass_through ? NULL : setup_option_value(argv[i], "--resource-root");
		if (!pass_through && (value || strcmp(argv[i], "--resource-root") == 0)) {
			if (!value) {
				if (++i >= argc) {
					return setup_error(error, error_size, "%s requires a path", "--resource-root");
				}
				value = argv[i];
			}
			if (!setup_copy_path(out->resource_root, sizeof out->resource_root, value, "--resource-root",
								 error, error_size)) {
				return 0;
			}
			continue;
		}
		if (!setup_append_argument(out->game_command_line, sizeof out->game_command_line, argv[i])) {
			return setup_error(error, error_size, "%s", "OpenXWA command line is too long");
		}
	}
	return 1;
}

static int setup_check_required_files(AeronVfs* vfs, const char** missing) {
	/* Canonical install layout: the CD ALLIANCE directory contents merged into the
	   root, as the original installer laid them out. RESDATA.TXT marks the merged
	   ALLIANCE content; the movie/wave entries are unique to CD1 and CD2 so a
	   one-disc tree cannot pass validation. */
	static const char* required[] = {
		"RESDATA.TXT",         "FLIGHTMODELS/SPACECRAFT0.LST", "MISSIONS/MISSION.LST",
		"MOVIES/PROLOGUE.SNM", "MOVIES/BATTLE1.SNM",           "WAVE/FRONTEND/B1M1/N010101.WAV",
	};
	size_t i;

	for (i = 0; i < sizeof required / sizeof required[0]; ++i) {
		if (!AeronVfs_Exists(vfs, AERON_VFS_ROOT_ASSET, required[i])) {
			*missing = required[i];
			return 0;
		}
	}
	return 1;
}

int XwaSetup_ValidateGameData(AeronVfs* vfs, const char* candidate, char* normalized,
							  size_t normalized_capacity, char* error, size_t error_size) {
	const char* missing = NULL;

	if (!vfs || !candidate || !candidate[0] || !normalized || strlen(candidate) >= normalized_capacity) {
		return setup_error(error, error_size, "invalid game-data directory: %s", candidate);
	}
	if (!AeronVfs_SetRoot(vfs, AERON_VFS_ROOT_ASSET, candidate) ||
		!AeronVfs_SetRootOptions(vfs, AERON_VFS_ROOT_ASSET,
								 AERON_VFS_ROOT_OPTION_CASE_INSENSITIVE_READ_LOOKUP)) {
		return setup_error(error, error_size, "could not use game-data directory: %s", candidate);
	}
	if (setup_check_required_files(vfs, &missing)) {
		snprintf(normalized, normalized_capacity, "%s", candidate);
		return 1;
	}

	/* A raw CD copy keeps the install files under ALLIANCE; the game expects them
	   merged into the root. Detect it so the failure explains the fix. */
	if (AeronVfs_Exists(vfs, AERON_VFS_ROOT_ASSET, "ALLIANCE/RESDATA.TXT")) {
		return setup_error(error, error_size,
						   "this looks like an unmerged CD copy: copy the contents of its ALLIANCE "
						   "directory into the selected folder (missing '%s')",
						   missing);
	}

	return setup_error(error, error_size, "selected directory is missing '%s'", missing);
}
