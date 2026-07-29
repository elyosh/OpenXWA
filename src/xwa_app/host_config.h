#ifndef XWA_APP_HOST_CONFIG_H
#define XWA_APP_HOST_CONFIG_H

#include "aeron/vfs.h"
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/config/modern_video_options.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XWA_HOST_CONFIG_PATH_CAPACITY 1024

typedef struct XwaHostConfig {
	char game_data_path[XWA_HOST_CONFIG_PATH_CAPACITY];
	int flight_simulation_step_ticks;
	float model_smooth_angle_degrees;
	float model_opt_emissive_strength;
	float model_opt_projectile_emissive_strength;
	float model_engine_emissive_strength;
	int force_opt_models;
	int prefer_original_2d;
	XwaModernVideoOptions video_options;
	unsigned int video_options_override_mask;
	XwaModernInputOptions input_options;
} XwaHostConfig;

int XwaHostConfig_Load(AeronVfs* vfs, XwaHostConfig* out, char* error, size_t error_size);
int XwaHostConfig_SaveGameDataPath(AeronVfs* vfs, const char* game_data_path, char* error, size_t error_size);
int XwaHostConfig_SaveVideoOptions(AeronVfs* vfs, const XwaModernVideoOptions* options, char* error,
								   size_t error_size);
int XwaHostConfig_SaveInputOptions(AeronVfs* vfs, const XwaModernInputOptions* options, char* error,
								   size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
