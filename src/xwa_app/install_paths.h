#ifndef XWA_APP_INSTALL_PATHS_H
#define XWA_APP_INSTALL_PATHS_H

/* The paths.* keys of the user's config.yaml: the game data folder and the
 * mods. The game and the mod importer both read and change them. */

#include "aeron/config_file.h"
#include "aeron/vfs.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XWA_INSTALL_PATH_CAPACITY 1024

/* The newest config.yaml version this build reads and writes. Older
 * versions are accepted; newer ones are refused. */
#define XWA_CONFIG_VERSION 1

typedef struct XwaInstallPaths {
	/* paths.game_data. Empty when not set. */
	char game_data[XWA_INSTALL_PATH_CAPACITY];
	/* paths.mod. Empty when not set. */
	char mod[XWA_INSTALL_PATH_CAPACITY];
	/* paths.mods: the folder that holds the mods. Empty when not set. */
	char mods[XWA_INSTALL_PATH_CAPACITY];
	/* paths.use_mod: 0 runs the original game with no mod. 1 by default. */
	int use_mod;
} XwaInstallPaths;

/* Fails when `config` has a version that this build can't read. A file
 * without a version is accepted. `path` names the file in the error. */
int XwaInstallPaths_CheckVersion(const AeronConfigFile* config, const char* path, char* error,
								 size_t error_size);

/* Reads the paths.* keys of an already loaded user configuration. Missing
 * keys keep their defaults: empty paths and use_mod 1. */
int XwaInstallPaths_Read(const AeronConfigFile* config, XwaInstallPaths* out, char* error, size_t error_size);

/* Reads the paths.* keys of the user's config.yaml, without the shipped
 * defaults. A missing file gives the defaults. */
int XwaInstallPaths_Load(AeronVfs* vfs, XwaInstallPaths* out, char* error, size_t error_size);

/* Each save rereads config.yaml, changes only its own keys and writes the
 * file. A missing file is created. */
int XwaInstallPaths_SaveGameData(AeronVfs* vfs, const char* game_data, char* error, size_t error_size);
int XwaInstallPaths_SaveMods(AeronVfs* vfs, const char* mods, char* error, size_t error_size);
/* Saves paths.mod, removed when `mod` is NULL or empty, and paths.use_mod. */
int XwaInstallPaths_SaveMod(AeronVfs* vfs, const char* mod, int use_mod, char* error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
