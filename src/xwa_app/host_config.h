#ifndef XWA_APP_HOST_CONFIG_H
#define XWA_APP_HOST_CONFIG_H

#include "aeron/aeron.h"
#include "aeron/vfs.h"
#include "install_paths.h"
#include "setup.h"
#include "xwa_runtime/config/modern_audio_options.h"
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/config/modern_video_options.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XWA_HOST_CONFIG_MANIFEST_CAPACITY 256

typedef struct XwaHostConfig {
	XwaInstallPaths paths;
	int flight_simulation_step_ticks;
	float model_smooth_angle_degrees;
	float model_opt_emissive_strength;
	float model_opt_projectile_emissive_strength;
	float model_engine_emissive_strength;
	int force_opt_models;
	int prefer_original_2d;
	int prefer_original_movies;
	int environment_maps_enabled;
	/* Relative to the mod folder. */
	char environment_maps_manifest[XWA_HOST_CONFIG_MANIFEST_CAPACITY];
	/* Largest radiance a sun disc is drawn with. */
	float environment_maps_sun_max_radiance;
	/* environment_maps.captured_lighting */
	int captured_lighting_enabled;
	int captured_lighting_size;
	int captured_lighting_specular_samples;
	float captured_lighting_diffuse_strength;
	float captured_lighting_specular_strength;
	int captured_lighting_has_ambient_fill;
	float captured_lighting_ambient_fill[3];
	XwaModernVideoOptions video_options;
	unsigned int video_options_override_mask;
	XwaModernInputOptions input_defaults;
	/* input.gamepad_defaults, input.vr_left_defaults and
	   input.vr_right_defaults: the profiles that new models get. */
	XwaControllerDefaults controller_defaults;
	XwaModernInputOptions input_options;
	/* The user file had version 1 input settings, upgraded in memory. The
	 * input options must be saved. */
	int input_upgraded;
} XwaHostConfig;

/* Reads the VR settings that the game starts with, before Aeron creates the
 * window and the GPU device: the mode and the eye images' scale from the
 * video section of the user's config.yaml, and the virtual screen from the
 * shipped remaster/config.yaml. --vr and --no-vr win over the file's mode.
 * A missing or bad file gives "off" and reports nothing: XwaHostConfig_Load
 * reports the problem later. */
void XwaHostConfig_LoadStartup(AeronVfs* vfs, const XwaLaunchOptions* launch, AeronStartupOptions* options);
int XwaHostConfig_Load(AeronVfs* vfs, XwaHostConfig* out, char* error, size_t error_size);
int XwaHostConfig_SaveAudioOptions(AeronVfs* vfs, const XwaModernAudioOptions* options, char* error,
								   size_t error_size);
int XwaHostConfig_SaveVideoOptions(AeronVfs* vfs, const XwaModernVideoOptions* options, char* error,
								   size_t error_size);
int XwaHostConfig_SaveInputOptions(AeronVfs* vfs, const XwaModernInputOptions* options, char* error,
								   size_t error_size);
/* Saves one switch, such as "camera.enabled", and no other key. */
int XwaHostConfig_SaveSwitch(AeronVfs* vfs, const char* key, int value, char* error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
