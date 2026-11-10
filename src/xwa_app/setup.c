#include "setup.h"

#include "mod_list.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
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
	out->film_pause_at = -1;
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
		value = pass_through ? NULL : setup_option_value(argv[i], "--mod");
		if (!pass_through && (value || strcmp(argv[i], "--mod") == 0)) {
			if (!value) {
				if (++i >= argc) {
					return setup_error(error, error_size, "%s requires a path", "--mod");
				}
				value = argv[i];
			}
			if (!setup_copy_path(out->mod_path, sizeof out->mod_path, value, "--mod", error, error_size)) {
				return 0;
			}
			continue;
		}
		value = pass_through ? NULL : setup_option_value(argv[i], "--play-film");
		if (!pass_through && (value || strcmp(argv[i], "--play-film") == 0)) {
			if (!value) {
				if (++i >= argc) {
					return setup_error(error, error_size, "%s requires a film name", "--play-film");
				}
				value = argv[i];
			}
			if (!setup_copy_path(out->play_film, sizeof out->play_film, value, "--play-film", error,
								 error_size)) {
				return 0;
			}
			continue;
		}
		value = pass_through ? NULL : setup_option_value(argv[i], "--benchmark-out");
		if (!pass_through && (value || strcmp(argv[i], "--benchmark-out") == 0)) {
			if (!value) {
				if (++i >= argc) {
					return setup_error(error, error_size, "%s requires a path", "--benchmark-out");
				}
				value = argv[i];
			}
			if (!setup_copy_path(out->benchmark_out, sizeof out->benchmark_out, value, "--benchmark-out",
								 error, error_size)) {
				return 0;
			}
			continue;
		}
		value = pass_through ? NULL : setup_option_value(argv[i], "--film-pause-at");
		if (!pass_through && (value || strcmp(argv[i], "--film-pause-at") == 0)) {
			char* end;
			long ticks;

			if (!value) {
				if (++i >= argc) {
					return setup_error(error, error_size, "%s requires a film time", "--film-pause-at");
				}
				value = argv[i];
			}
			ticks = strtol(value, &end, 10);
			if (!value[0] || *end || ticks < 0 || ticks > INT_MAX) {
				return setup_error(error, error_size, "%s requires a film time in game ticks, 0 or more",
								   "--film-pause-at");
			}
			out->film_pause_at = (int)ticks;
			continue;
		}
		if (!pass_through && strcmp(argv[i], "--film-benchmark") == 0) {
			out->film_benchmark = 1;
			continue;
		}
		if (!pass_through && (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)) {
			out->help = 1;
			continue;
		}
		if (!setup_append_argument(out->game_command_line, sizeof out->game_command_line, argv[i])) {
			return setup_error(error, error_size, "%s", "OpenXWA command line is too long");
		}
	}
	if (out->help) {
		return 1;
	}
	if (out->film_pause_at >= 0 && !out->play_film[0]) {
		return setup_error(error, error_size, "%s requires --play-film", "--film-pause-at");
	}
	if (out->film_benchmark && out->film_pause_at < 0) {
		return setup_error(error, error_size, "%s requires --film-pause-at", "--film-benchmark");
	}
	return 1;
}

const char* XwaLaunchOptions_Help(void) {
	return "Usage: OpenXWA [options] [-- game options]\n"
		   "\n"
		   "  --game-data <folder>      the folder of the installed game's data\n"
		   "  --resource-root <folder>  the folder that holds OpenXWA's resources folder,\n"
		   "                            instead of the application's folder\n"
		   "  --mod <folder>            play with the mod in this folder\n"
		   "  --play-film <name>        play this film of the user folder's film folder,\n"
		   "                            log the frame times, then quit\n"
		   "  --benchmark-out <file>    with --play-film: also write the time of each frame\n"
		   "                            to this CSV file of the user folder\n"
		   "  --film-pause-at <ticks>   with --play-film: fast-forward to this film time,\n"
		   "                            in 4 ms ticks, and freeze there\n"
		   "  --film-benchmark          with --film-pause-at: measure the frozen frame's\n"
		   "                            GPU time, then quit\n"
		   "  -h, --help                print this help and quit\n"
		   "\n"
		   "Other arguments, and everything after --, go to the game's own command line.\n";
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
		return setup_error(error, error_size, "It can't be used: %s", candidate);
	}
	if (!AeronVfs_SetRoot(vfs, AERON_VFS_ROOT_ASSET, candidate) ||
		!AeronVfs_SetRootOptions(vfs, AERON_VFS_ROOT_ASSET, AERON_VFS_ROOT_OPTION_CASE_INSENSITIVE_LOOKUP)) {
		return setup_error(error, error_size, "It can't be used: %s", candidate);
	}
	if (setup_check_required_files(vfs, &missing)) {
		snprintf(normalized, normalized_capacity, "%s", candidate);
		return 1;
	}

	/* A raw CD copy keeps the install files under ALLIANCE; the game expects them
	   merged into the root. Detect it so the failure explains the fix. */
	if (AeronVfs_Exists(vfs, AERON_VFS_ROOT_ASSET, "ALLIANCE/RESDATA.TXT")) {
		return setup_error(error, error_size,
						   "It looks like a copy of the CDs. Copy what is in its ALLIANCE folder into it. "
						   "It has no %s.",
						   missing);
	}

	return setup_error(error, error_size, "It has no %s.", missing);
}

int XwaSetup_SetModRoot(AeronVfs* vfs, const XwaLaunchOptions* launch, const XwaHostConfig* host_config,
						char* selected, size_t selected_capacity, char* notice, size_t notice_size,
						char* error, size_t error_size) {
	const int from_launch = launch->mod_path[0] != 0;
	const char* problem = NULL;
	XwaModEntry mod;
	int length;

	if (notice_size) {
		notice[0] = '\0';
	}
	if (selected_capacity) {
		selected[0] = '\0';
	}
	if (from_launch) {
		length = snprintf(selected, selected_capacity, "%s", launch->mod_path);
	} else if (host_config->use_mod && host_config->mod_path[0]) {
		length = snprintf(selected, selected_capacity, "%s", host_config->mod_path);
	} else {
		return 1;
	}
	if (length < 0 || (size_t)length >= selected_capacity) {
		return setup_error(error, error_size, "could not use mod folder: %s", selected);
	}
	if (!XwaModList_Read(selected, &mod)) {
		snprintf(mod.name, sizeof mod.name, "%s", selected);
		problem = "Its folder has no mod.yaml, so it isn't a mod.";
	} else if (mod.state != XWA_MOD_UNFINISHED || !from_launch) {
		problem = mod.problem[0] ? mod.problem : NULL;
	}
	if (problem && from_launch) {
		snprintf(error, error_size, "--mod %s: %s", selected, problem);
		return 0;
	}
	if (problem) {
		snprintf(notice, notice_size,
				 "The mod \"%s\" can't be used. %s\n\nOpenXWA starts with the original game.", mod.name,
				 problem);
		selected[0] = '\0';
		return 1;
	}
	if (!AeronVfs_SetRoot(vfs, AERON_VFS_ROOT_MOD, selected) ||
		!AeronVfs_SetRootOptions(vfs, AERON_VFS_ROOT_MOD, AERON_VFS_ROOT_OPTION_CASE_INSENSITIVE_LOOKUP)) {
		return setup_error(error, error_size, "could not use mod folder: %s", selected);
	}
	return 1;
}
