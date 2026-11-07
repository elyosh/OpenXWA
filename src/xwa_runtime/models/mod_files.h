#ifndef XWA_RUNTIME_MODELS_MOD_FILES_H
#define XWA_RUNTIME_MODELS_MOD_FILES_H

/* Loads the YAML files of the mod folder. A file that the mod doesn't have
 * is fine. A file that it has but that can't be read, or that isn't a map,
 * stops the game with a fatal error, because the mod would otherwise play
 * without its settings. */

#include "aeron/config_file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XwaModFileResult {
	/* The mod has no such file. */
	XWA_MOD_FILE_MISSING,
	/* The file is loaded, and its root is a map. */
	XWA_MOD_FILE_LOADED,
	/* The file can't be used. The game is stopping. */
	XWA_MOD_FILE_FAILED,
} XwaModFileResult;

/* Loads the mod file `path` into `*out`, which is NULL unless the result is
   XWA_MOD_FILE_LOADED. With `allowEmpty`, an empty file is loaded and its
   root is NULL. Without it, an empty file isn't a map. Only the first fatal
   error shows a message. */
XwaModFileResult XwaModFiles_Load(const char* path, int allowEmpty, AeronConfigFile** out);

#ifdef __cplusplus
}
#endif

#endif
