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
	char game_command_line[1024];
} XwaLaunchOptions;

int XwaLaunchOptions_Parse(int argc, char** argv, XwaLaunchOptions* out, char* error, size_t error_size);

/* Applies a candidate asset root and validates the staged original-data
 * layout. Selecting its ALLIANCE install child is normalized to the parent. */
int XwaSetup_ValidateGameData(AeronVfs* vfs, const char* candidate, char* normalized,
							  size_t normalized_capacity, char* error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
