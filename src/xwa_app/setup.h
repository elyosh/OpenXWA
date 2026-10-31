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

/* Sets the mod root to the --mod path, else, unless paths.use_mod is false,
 * the paths.mod setting, else <game data>/mod. Copies the path used to
 * selected, which is empty when paths.use_mod is false: the mod root then
 * stays unset and the game uses its own files only. The folder does not have
 * to exist. */
int XwaSetup_SetModRoot(AeronVfs* vfs, const XwaLaunchOptions* launch, const XwaHostConfig* host_config,
						const char* game_data, char* selected, size_t selected_capacity, char* error,
						size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
