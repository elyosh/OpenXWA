#ifndef XWA_APP_SETUP_H
#define XWA_APP_SETUP_H

#include "aeron/vfs.h"
#include "host_config.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaLaunchOptions {
	char game_data_path[XWA_HOST_CONFIG_PATH_CAPACITY];
	char resource_root[XWA_HOST_CONFIG_PATH_CAPACITY];
	char mod_path[XWA_HOST_CONFIG_PATH_CAPACITY];
	/* --play-film: a film of the user folder's film folder to play, after
	   which the game quits. --benchmark-out: the CSV file, in the user
	   folder, that gets its frame times. */
	char play_film[256];
	char benchmark_out[XWA_HOST_CONFIG_PATH_CAPACITY];
	char game_command_line[1024];
} XwaLaunchOptions;

int XwaLaunchOptions_Parse(int argc, char** argv, XwaLaunchOptions* out, char* error, size_t error_size);

/* Applies a candidate asset root and validates the canonical install layout:
 * an installed game directory with the CD ALLIANCE contents merged into the
 * root, plus the CD data directories (FLIGHTMODELS, MISSIONS, MOVIES, RESDATA,
 * WAVE). An unmerged CD copy is rejected with an explanatory error. */
int XwaSetup_ValidateGameData(AeronVfs* vfs, const char* candidate, char* normalized,
							  size_t normalized_capacity, char* error, size_t error_size);

/* Sets the mod root to the --mod folder, or else to paths.mod when
 * paths.use_mod is true. Copies the folder it uses to `selected`. Without a
 * mod, `selected` is empty, the mod root stays unset and the game uses only
 * its own files.
 *
 * The mod folder must have a mod.yaml with the current mod format version.
 * The mod must also be complete, unless it comes from --mod.
 *   - A --mod folder that doesn't pass these checks is an error.
 *   - A paths.mod folder that doesn't pass them isn't used. The game starts
 *     with its own files, and `notice` explains why.
 * Otherwise, `notice` is empty. */
int XwaSetup_SetModRoot(AeronVfs* vfs, const XwaLaunchOptions* launch, const XwaHostConfig* host_config,
						char* selected, size_t selected_capacity, char* notice, size_t notice_size,
						char* error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
