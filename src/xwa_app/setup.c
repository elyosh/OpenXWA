#include "setup.h"

#include "aeron/paths.h"
#include "mod_list.h"
#include "mod_update.h"

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
		value = pass_through ? NULL : setup_option_value(argv[i], "--record-audio");
		if (!pass_through && (value || strcmp(argv[i], "--record-audio") == 0)) {
			if (!value) {
				if (++i >= argc) {
					return setup_error(error, error_size, "%s requires a path", "--record-audio");
				}
				value = argv[i];
			}
			if (!setup_copy_path(out->record_audio, sizeof out->record_audio, value, "--record-audio", error,
								 error_size)) {
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
		value = pass_through ? NULL : setup_option_value(argv[i], "--film-capture");
		if (!pass_through && (value || strcmp(argv[i], "--film-capture") == 0)) {
			if (!value) {
				if (++i >= argc) {
					return setup_error(error, error_size, "%s requires a file name", "--film-capture");
				}
				value = argv[i];
			}
			if (!setup_copy_path(out->film_capture, sizeof out->film_capture, value, "--film-capture", error,
								 error_size)) {
				return 0;
			}
			continue;
		}
		value = pass_through ? NULL : setup_option_value(argv[i], "--render-size");
		if (!pass_through && (value || strcmp(argv[i], "--render-size") == 0)) {
			char* end;
			long width;
			long height = 0;

			if (!value) {
				if (++i >= argc) {
					return setup_error(error, error_size, "%s requires a size such as 1920x1080", "--render-size");
				}
				value = argv[i];
			}
			width = strtol(value, &end, 10);
			if (end != value && (*end == 'x' || *end == 'X')) {
				const char* rest = end + 1;
				height = strtol(rest, &end, 10);
				if (end == rest) {
					height = 0;
				}
			}
			if (*end || width < 320 || height < 240 || width > 16384 || height > 16384 || (width & 1) ||
				(height & 1)) {
				return setup_error(error, error_size,
								   "%s requires a size such as 1920x1080, with even numbers from 320x240 to "
								   "16384x16384",
								   "--render-size");
			}
			out->render_width = (int)width;
			out->render_height = (int)height;
			continue;
		}
		value = pass_through ? NULL : setup_option_value(argv[i], "--flight-views");
		if (!pass_through && (value || strcmp(argv[i], "--flight-views") == 0)) {
			if (!value) {
				if (++i >= argc) {
					return setup_error(error, error_size, "%s requires surround or stereo", "--flight-views");
				}
				value = argv[i];
			}
			if (strcmp(value, "surround") == 0) {
				out->flight_views = 1;
			} else if (strcmp(value, "stereo") == 0) {
				out->flight_views = 2;
			} else {
				return setup_error(error, error_size, "%s requires surround or stereo", "--flight-views");
			}
			continue;
		}
		if (!pass_through && strcmp(argv[i], "--vr") == 0) {
			out->vr = 1;
			continue;
		}
		if (!pass_through && strcmp(argv[i], "--no-vr") == 0) {
			out->vr = -1;
			continue;
		}
		if (!pass_through && strcmp(argv[i], "--classic-2d-sound") == 0) {
			out->classic_2d_sound = 1;
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
	if (out->record_audio[0] && !out->play_film[0]) {
		return setup_error(error, error_size, "%s requires --play-film", "--record-audio");
	}
	if (out->record_audio[0] && out->film_pause_at >= 0) {
		return setup_error(error, error_size, "%s can't be used with --film-pause-at", "--record-audio");
	}
	if (out->film_pause_at >= 0 && !out->play_film[0]) {
		return setup_error(error, error_size, "%s requires --play-film", "--film-pause-at");
	}
	if (out->film_capture[0] && out->film_pause_at < 0) {
		return setup_error(error, error_size, "%s requires --film-pause-at", "--film-capture");
	}
	if (out->film_benchmark && out->film_pause_at < 0) {
		return setup_error(error, error_size, "%s requires --film-pause-at", "--film-benchmark");
	}
	return 1;
}

static int relaunch_path(XwaRelaunchArgs* out, const char* option, const char* path, char* storage,
						 size_t capacity, char* error, size_t error_size) {
	if (!path[0]) {
		return 1;
	}
	if (!Aeron_AbsolutePath(path, storage, capacity)) {
		return setup_error(error, error_size, "%s can't be made absolute", option);
	}
	out->args[out->count++] = option;
	out->args[out->count++] = storage;
	return 1;
}

int XwaLaunchOptions_RelaunchArgs(const XwaLaunchOptions* launch, int skip_intro, XwaRelaunchArgs* out,
								  char* error, size_t error_size) {
	if (!launch || !out) {
		return setup_error(error, error_size, "%s", "invalid relaunch options");
	}
	memset(out, 0, sizeof *out);
	if (!relaunch_path(out, "--game-data", launch->game_data_path, out->game_data, sizeof out->game_data,
					   error, error_size) ||
		!relaunch_path(out, "--resource-root", launch->resource_root, out->resource_root,
					   sizeof out->resource_root, error, error_size) ||
		!relaunch_path(out, "--mod", launch->mod_path, out->mod, sizeof out->mod, error, error_size)) {
		return 0;
	}
	if (launch->render_width > 0) {
		snprintf(out->render_size, sizeof out->render_size, "%dx%d", launch->render_width,
				 launch->render_height);
		out->args[out->count++] = "--render-size";
		out->args[out->count++] = out->render_size;
	}
	if (launch->flight_views) {
		out->args[out->count++] = "--flight-views";
		out->args[out->count++] = launch->flight_views == 1 ? "surround" : "stereo";
	}
	if (launch->vr) {
		out->args[out->count++] = launch->vr > 0 ? "--vr" : "--no-vr";
	}
	/* The parser joins the game's arguments with spaces, so one argument gives
	   it the same text back, quotes included. */
	snprintf(out->game_command_line, sizeof out->game_command_line, "%s", launch->game_command_line);
	if (skip_intro && !strstr(out->game_command_line, "skipintro") &&
		!setup_append_argument(out->game_command_line, sizeof out->game_command_line, "skipintro")) {
		return setup_error(error, error_size, "%s", "OpenXWA command line is too long");
	}
	if (out->game_command_line[0]) {
		out->args[out->count++] = "--";
		out->args[out->count++] = out->game_command_line;
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
		   "  --record-audio <file>     with --play-film: record the film's sound to this\n"
		   "                            WAV file of the user folder\n"
		   "  --film-pause-at <ticks>   with --play-film: fast-forward to this film time,\n"
		   "                            in 4 ms ticks, and freeze there\n"
		   "  --film-capture <name>     with --film-pause-at: save the frozen frame to\n"
		   "                            <name>.png, and the frame before the tonemap to\n"
		   "                            <name>.hdr, in the user folder's capture folder,\n"
		   "                            then quit\n"
		   "  --film-benchmark          with --film-pause-at: measure the frozen frame's\n"
		   "                            GPU time, then quit\n"
		   "  --classic-2d-sound        play this run with the game's 3D sound off, as the\n"
		   "                            original game did by default\n"
		   "  --render-size <W>x<H>     render the 3D view at this size in pixels,\n"
		   "                            whatever the window's size, and fit it in the\n"
		   "                            window\n"
		   "  --flight-views <kind>     for tests: draw the flight scene of this run as\n"
		   "                            three surround screens (surround), or as the two\n"
		   "                            eyes of a headset side by side (stereo)\n"
		   "  --vr                      start in VR, and stop with a message when VR\n"
		   "                            can't start\n"
		   "  --no-vr                   start without VR, whatever the settings say\n"
		   "  -h, --help                print this help and quit\n"
		   "\n"
		   "Other arguments, and everything after --, go to the game's own command line.\n";
}

int XwaSetup_SetModRoot(AeronVfs* vfs, const XwaLaunchOptions* launch, const XwaInstallPaths* paths,
						char* selected, size_t selected_capacity, char* notice, size_t notice_size,
						char* error, size_t error_size) {
	const int from_launch = launch->mod_path[0] != 0;
	const char* problem = NULL;
	const char* advice = NULL;
	char mods_folder[XWA_INSTALL_PATH_CAPACITY];
	XwaModEntry mod;
	XwaModUpdateResult update;
	int length;

	if (notice_size) {
		notice[0] = '\0';
	}
	if (selected_capacity) {
		selected[0] = '\0';
	}
	/* Before any mod is read: the mods deleted while OpenXWA used them go to
	   the trash, and the new imports made while OpenXWA used their mods
	   replace the old ones. */
	if (XwaModList_Folder(paths->mods, mods_folder, sizeof mods_folder)) {
		XwaModUpdate_DeletePending(mods_folder, paths->mod);
		XwaModUpdate_ApplyPending(mods_folder, paths->mod);
	}
	if (from_launch) {
		length = snprintf(selected, selected_capacity, "%s", launch->mod_path);
	} else if (paths->use_mod && paths->mod[0]) {
		length = snprintf(selected, selected_capacity, "%s", paths->mod);
	} else {
		return 1;
	}
	if (length < 0 || (size_t)length >= selected_capacity) {
		return setup_error(error, error_size, "could not use mod folder: %s", selected);
	}
	/* A new import made while OpenXWA used the mod takes its place. */
	update = XwaModUpdate_Apply(selected);
	if (!XwaModList_Read(selected, &mod)) {
		snprintf(mod.name, sizeof mod.name, "%s", selected);
		problem = "Its folder is missing, or has no mod.yaml.";
		advice = "Choose another mod in OpenXWA Options > Mods.";
	} else if (mod.state != XWA_MOD_UNFINISHED || !from_launch) {
		problem = mod.problem[0] ? mod.problem : NULL;
		advice = XwaModList_Advice(&mod, XWA_MOD_ADVICE_STARTUP);
	}
	if (problem && from_launch) {
		snprintf(error, error_size, "--mod %s: %s", selected, problem);
		return 0;
	}
	if (problem) {
		snprintf(notice, notice_size,
				 "The mod \"%s\" can't be used. %s%s%s\n\nOpenXWA starts with the original game.", mod.name,
				 problem, advice ? " " : "", advice ? advice : "");
		selected[0] = '\0';
		return 1;
	}
	if (update == XWA_MOD_UPDATE_FAILED) {
		snprintf(notice, notice_size,
				 "The new import of \"%s\" couldn't replace the old one, because a program is using its folder:\n"
				 "%s\n\nOpenXWA starts with the old one, and tries again the next time it starts.",
				 mod.name, selected);
	}
	if (!AeronVfs_SetRoot(vfs, AERON_VFS_ROOT_MOD, selected) ||
		!AeronVfs_SetRootOptions(vfs, AERON_VFS_ROOT_MOD, AERON_VFS_ROOT_OPTION_CASE_INSENSITIVE_LOOKUP)) {
		return setup_error(error, error_size, "could not use mod folder: %s", selected);
	}
	return 1;
}
