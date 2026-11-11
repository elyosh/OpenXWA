#include "host_config.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "aeron/numeric.h"
#include "controller_config.h"
#include "keyboard_config.h"
#include "xwa_runtime/config/modern_camera_options.h"
#include "xwa_runtime/config/modern_camera_shake_options.h"
#include "xwa_runtime/config/modern_speed_effect_options.h"
#include "xwa_runtime/config/modern_sun_options.h"
#include "xwa_runtime/config/modern_target_readout_options.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int host_config_error(char* error, size_t error_size, const char* message, const char* detail) {
	if (error && error_size) {
		snprintf(error, error_size, message, detail ? detail : "");
	}
	return 0;
}

static int host_config_simulation_step(const AeronConfigFile* config, int* out, char* error,
									   size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, "flight.simulation_step_ticks");
	int64_t value;

	if (!node) {
		*out = 1;
		return 1;
	}
	value = AeronConfigNode_Int(node, 0);
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || (value != 1 && value != 4 && value != 8)) {
		return host_config_error(error, error_size,
								 "invalid 'flight.simulation_step_ticks': expected integer %s", "1, 4, or 8");
	}
	*out = (int)value;
	return 1;
}

static int host_config_model_smoothing(const AeronConfigFile* config, int required, float* out, char* error,
									   size_t error_size) {
	const char* key = "models.smooth_angle_degrees";
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	double value;
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 key);
		}
		return 1;
	}
	value = AeronConfigNode_Float(node, NAN);
	if (!isfinite(value) || value < 0.0 || value > 180.0) {
		return host_config_error(error, error_size,
								 "invalid 'models.smooth_angle_degrees': expected numeric value %s",
								 "from 0 through 180");
	}
	*out = (float)value;
	return 1;
}

static int host_config_opt_emissive_strength(const AeronConfigFile* config, int required, float* out,
											 char* error, size_t error_size) {
	const char* key = "models.opt_emissive_strength";
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	double value;
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 key);
		}
		return 1;
	}
	value = AeronConfigNode_Float(node, NAN);
	if (!isfinite(value) || value < 0.0 || value > FLT_MAX) {
		return host_config_error(error, error_size,
								 "invalid 'models.opt_emissive_strength': expected numeric value %s",
								 "greater than or equal to 0");
	}
	*out = (float)value;
	return 1;
}

static int host_config_opt_projectile_emissive_strength(const AeronConfigFile* config, int required,
														float* out, char* error, size_t error_size) {
	const char* key = "models.opt_projectile_emissive_strength";
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	double value;
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 key);
		}
		return 1;
	}
	value = AeronConfigNode_Float(node, NAN);
	if (!isfinite(value) || value < 0.0 || value > FLT_MAX) {
		return host_config_error(
			error, error_size, "invalid 'models.opt_projectile_emissive_strength': expected numeric value %s",
			"greater than or equal to 0");
	}
	*out = (float)value;
	return 1;
}

static int host_config_engine_emissive_strength(const AeronConfigFile* config, int required, float* out,
												char* error, size_t error_size) {
	const char* key = "models.engine_emissive_strength";
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	double value;
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 key);
		}
		return 1;
	}
	value = AeronConfigNode_Float(node, NAN);
	if (!isfinite(value) || value < 0.0 || value > FLT_MAX) {
		return host_config_error(error, error_size,
								 "invalid 'models.engine_emissive_strength': expected numeric value %s",
								 "greater than or equal to 0");
	}
	*out = (float)value;
	return 1;
}

static int host_config_force_opt(const AeronConfigFile* config, int required, int* out, char* error,
								 size_t error_size) {
	const char* key = "models.force_opt";
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 key);
		}
		return 1;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		return host_config_error(error, error_size, "invalid 'models.force_opt': expected %s", "boolean");
	}
	*out = AeronConfigNode_Bool(node, 0);
	return 1;
}

/* assets.prefer_original_2d and assets.prefer_original_movies. */
static int host_config_prefer_original(const AeronConfigFile* config, int required, const char* key, int* out,
									   char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 key);
		}
		return 1;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		return host_config_error(error, error_size, "invalid '%s': expected boolean", key);
	}
	*out = AeronConfigNode_Bool(node, 0);
	return 1;
}

static int host_config_environment_maps(const AeronConfigFile* config, int required, XwaHostConfig* out,
										char* error, size_t error_size) {
	const char* enabled_key = "environment_maps.enabled";
	const char* manifest_key = "environment_maps.manifest";
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, enabled_key);
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 enabled_key);
		}
	} else if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		return host_config_error(error, error_size, "invalid 'environment_maps.enabled': expected %s",
								 "boolean");
	} else {
		out->environment_maps_enabled = AeronConfigNode_Bool(node, 0);
	}
	node = AeronConfigFile_GetNode(config, manifest_key);
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 manifest_key);
		}
		return 1;
	}
	const char* manifest = AeronConfigNode_String(node, NULL);
	if (!manifest || !manifest[0] || strlen(manifest) >= sizeof out->environment_maps_manifest) {
		return host_config_error(error, error_size, "invalid 'environment_maps.manifest': expected %s",
								 "a non-empty path shorter than 256 bytes");
	}
	snprintf(out->environment_maps_manifest, sizeof out->environment_maps_manifest, "%s", manifest);
	return 1;
}

static int host_config_sun_max_radiance(const AeronConfigFile* config, int required, float* out, char* error,
										size_t error_size) {
	const char* key = "environment_maps.sun_max_radiance";
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	double value;
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 key);
		}
		return 1;
	}
	value = AeronConfigNode_Float(node, NAN);
	if (!isfinite(value) || value < 1.0 || value > 60000.0) {
		return host_config_error(error, error_size,
								 "invalid 'environment_maps.sun_max_radiance': expected numeric value %s",
								 "from 1 to 60000");
	}
	*out = (float)value;
	return 1;
}

/* Reads a strength from 0 to 100 at `key`. A missing key is an error only
 * when `required`. */
static int host_config_strength(const AeronConfigFile* config, int required, const char* key, float* out,
								char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 key);
		}
		return 1;
	}
	const double value = AeronConfigNode_Float(node, NAN);
	if (!isfinite(value) || value < 0.0 || value > 100.0) {
		return host_config_error(error, error_size, "invalid '%s': expected a number from 0 to 100", key);
	}
	*out = (float)value;
	return 1;
}

static int host_config_captured_lighting(const AeronConfigFile* config, int required, XwaHostConfig* out,
										 char* error, size_t error_size) {
	const char* enabled_key = "environment_maps.captured_lighting.enabled";
	const char* size_key = "environment_maps.captured_lighting.size";
	const char* samples_key = "environment_maps.captured_lighting.specular_samples";
	const char* fill_key = "environment_maps.captured_lighting.ambient_fill";
	const AeronConfigNode* section = AeronConfigFile_GetNode(config, "environment_maps.captured_lighting");
	if (section && AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		return host_config_error(error, error_size, "'%s' must be a mapping",
								 "environment_maps.captured_lighting");
	}
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, enabled_key);
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 enabled_key);
		}
	} else if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		return host_config_error(error, error_size, "invalid '%s': expected a boolean", enabled_key);
	} else {
		out->captured_lighting_enabled = AeronConfigNode_Bool(node, 0);
	}
	node = AeronConfigFile_GetNode(config, size_key);
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 size_key);
		}
	} else {
		const int64_t size =
			AeronConfigNode_Type(node) == AERON_CONFIG_INT ? AeronConfigNode_Int(node, 0) : 0;
		if (size != 64 && size != 128 && size != 256 && size != 512) {
			return host_config_error(error, error_size, "invalid '%s': expected 64, 128, 256 or 512",
									 size_key);
		}
		out->captured_lighting_size = (int)size;
	}
	node = AeronConfigFile_GetNode(config, samples_key);
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 samples_key);
		}
	} else {
		const int64_t samples =
			AeronConfigNode_Type(node) == AERON_CONFIG_INT ? AeronConfigNode_Int(node, 0) : 0;
		if (samples < 1 || samples > 1024) {
			return host_config_error(error, error_size, "invalid '%s': expected an integer from 1 to 1024",
									 samples_key);
		}
		out->captured_lighting_specular_samples = (int)samples;
	}
	if (!host_config_strength(config, required, "environment_maps.captured_lighting.diffuse_strength",
							  &out->captured_lighting_diffuse_strength, error, error_size) ||
		!host_config_strength(config, required, "environment_maps.captured_lighting.specular_strength",
							  &out->captured_lighting_specular_strength, error, error_size)) {
		return 0;
	}
	/* ambient_fill is optional, also in the shipped configuration. */
	node = AeronConfigFile_GetNode(config, fill_key);
	if (node) {
		float fill[3];
		int valid = AeronConfigNode_SequenceCount(node) == 3;
		for (size_t i = 0; valid && i < 3; i++) {
			const double value = AeronConfigNode_Float(AeronConfigNode_SequenceGet(node, i), NAN);
			valid = isfinite(value) && value >= 0.0 && value <= 100.0;
			fill[i] = (float)value;
		}
		if (!valid) {
			return host_config_error(error, error_size, "invalid '%s': expected three numbers from 0 to 100",
									 fill_key);
		}
		memcpy(out->captured_lighting_ambient_fill, fill, sizeof fill);
		out->captured_lighting_has_ambient_fill = 1;
	}
	return 1;
}

/* Reads the option sections: from the shipped file when `required` is set,
 * else from the player's file. */
static int host_config_option_sections(const AeronConfigFile* config, int required, char* error,
									   size_t error_size) {
	XwaOptionSection* const sections[] = {
		XwaModernCameraOptions_Section(),
		XwaModernCameraShakeOptions_Section(),
		XwaModernSunOptions_Section(),
		XwaModernSpeedEffectOptions_Section(),
		XwaModernTargetReadoutOptions_Section(),
	};

	for (size_t i = 0; i < sizeof sections / sizeof sections[0]; ++i) {
		if (!XwaOptionSection_ReadConfig(sections[i], config, required, error, error_size)) {
			return 0;
		}
	}
	return 1;
}

static int host_config_remaster_options(const AeronConfigFile* config, int required, XwaHostConfig* out,
										char* error, size_t error_size) {
	return host_config_model_smoothing(config, required, &out->model_smooth_angle_degrees, error,
									   error_size) &&
		   host_config_opt_emissive_strength(config, required, &out->model_opt_emissive_strength, error,
											 error_size) &&
		   host_config_opt_projectile_emissive_strength(
			   config, required, &out->model_opt_projectile_emissive_strength, error, error_size) &&
		   host_config_engine_emissive_strength(config, required, &out->model_engine_emissive_strength, error,
												error_size) &&
		   host_config_force_opt(config, required, &out->force_opt_models, error, error_size) &&
		   host_config_prefer_original(config, required, "assets.prefer_original_2d",
									   &out->prefer_original_2d, error, error_size) &&
		   host_config_prefer_original(config, required, "assets.prefer_original_movies",
									   &out->prefer_original_movies, error, error_size) &&
		   host_config_environment_maps(config, required, out, error, error_size) &&
		   host_config_sun_max_radiance(config, required, &out->environment_maps_sun_max_radiance, error,
										error_size) &&
		   host_config_captured_lighting(config, required, out, error, error_size);
}

static int host_config_shipped_input(const AeronConfigFile* config, XwaModernInputOptions* out,
									 XwaControllerProfile* gamepad_defaults, char* error, size_t error_size);

static int host_config_load_shipped_config(AeronVfs* vfs, XwaHostConfig* out, char* error,
										   size_t error_size) {
	static const char* path = "remaster/config.yaml";
	AeronConfigFile* config = NULL;
	int valid;

	if (!AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_RESOURCE, path, &config)) {
		return host_config_error(error, error_size,
								 "required shipped configuration unavailable or invalid: %s", path);
	}
	if (AeronConfigNode_Type(AeronConfigFile_Root(config)) != AERON_CONFIG_MAP) {
		AeronConfigFile_Destroy(config);
		return host_config_error(error, error_size, "shipped configuration root must be a mapping: %s", path);
	}
	valid =
		host_config_remaster_options(config, 1, out, error, error_size) &&
		host_config_shipped_input(config, &out->input_defaults, &out->gamepad_defaults, error, error_size) &&
		host_config_option_sections(config, 1, error, error_size);
	if (valid) {
		out->input_options = out->input_defaults;
	}
	AeronConfigFile_Destroy(config);
	return valid;
}

static int host_config_named_value(const AeronConfigFile* config, const char* key, const char* const* names,
								   size_t name_count, int* out, unsigned int override_bit,
								   unsigned int* override_mask, char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	const char* value;
	size_t index;

	if (!node) {
		return 1;
	}
	if (AeronConfigNode_Type(node) == AERON_CONFIG_BOOL && !AeronConfigNode_Bool(node, 1) && name_count > 0 &&
		strcmp(names[0], "off") == 0) {
		*out = 0;
		*override_mask |= override_bit;
		return 1;
	}
	value = AeronConfigNode_String(node, NULL);
	if (!value) {
		return host_config_error(error, error_size, "invalid video setting '%s'", key);
	}
	for (index = 0; index < name_count; ++index) {
		if (strcmp(value, names[index]) == 0) {
			*out = (int)index;
			*override_mask |= override_bit;
			return 1;
		}
	}
	return host_config_error(error, error_size, "invalid video setting '%s'", key);
}

static int host_config_video_options(const AeronConfigFile* config, XwaModernVideoOptions* out,
									 unsigned int* override_mask, char* error, size_t error_size) {
	static const char* const window_mode_names[] = { "windowed", "fullscreen" };
	static const char* const quality_names[] = { "off", "low", "high" };
	static const char* const shadow_quality_names[] = { "standard", "high" };
	static const char* const fsr_names[] = { "off", "performance", "balanced", "quality", "native_aa" };
	static const char* const msaa_names[] = { "off", "2x", "4x", "8x" };
	const char* hdr_key = "video.hdr_output";
	const AeronConfigNode* hdr_node;
	int value;

	*override_mask = 0;
	value = 0;
	if (!host_config_named_value(config, "video.window_mode", window_mode_names,
								 sizeof window_mode_names / sizeof window_mode_names[0], &value,
								 XWA_MODERN_VIDEO_OVERRIDE_WINDOW_MODE, override_mask, error, error_size)) {
		return 0;
	}
	out->window_mode = (XwaModernWindowMode)value;

	value = 0;
	if (!host_config_named_value(config, "video.ssao_quality", quality_names,
								 sizeof quality_names / sizeof quality_names[0], &value,
								 XWA_MODERN_VIDEO_OVERRIDE_SSAO, override_mask, error, error_size)) {
		return 0;
	}
	out->ssao_quality = (XwaModernSsaoQuality)value;

	value = 0;
	if (!host_config_named_value(config, "video.shadow_quality", shadow_quality_names,
								 sizeof shadow_quality_names / sizeof shadow_quality_names[0], &value,
								 XWA_MODERN_VIDEO_OVERRIDE_SHADOW_QUALITY, override_mask, error,
								 error_size)) {
		return 0;
	}
	out->shadow_quality = (XwaModernShadowQuality)value;

	value = 0;
	if (!host_config_named_value(config, "video.fsr_upscaling", fsr_names,
								 sizeof fsr_names / sizeof fsr_names[0], &value,
								 XWA_MODERN_VIDEO_OVERRIDE_FSR, override_mask, error, error_size)) {
		return 0;
	}
	out->fsr_upscaling = (XwaModernFsrUpscaling)value;

	value = 0;
	if (!host_config_named_value(config, "video.msaa", msaa_names, sizeof msaa_names / sizeof msaa_names[0],
								 &value, XWA_MODERN_VIDEO_OVERRIDE_MSAA, override_mask, error, error_size)) {
		return 0;
	}
	out->msaa = (XwaModernMsaa)value;
	if (out->fsr_upscaling != XWA_MODERN_FSR_OFF && out->msaa != XWA_MODERN_MSAA_OFF) {
		return host_config_error(error, error_size, "%s",
								 "video.fsr_upscaling and video.msaa cannot both be enabled");
	}

	value = 0;
	if (!host_config_named_value(config, "video.motion_blur_quality", quality_names,
								 sizeof quality_names / sizeof quality_names[0], &value,
								 XWA_MODERN_VIDEO_OVERRIDE_MOTION_BLUR, override_mask, error, error_size)) {
		return 0;
	}
	out->motion_blur_quality = (XwaModernMotionBlurQuality)value;
	{
		const char* amount_key = "video.motion_blur_amount";
		const AeronConfigNode* amount_node = AeronConfigFile_GetNode(config, amount_key);
		if (amount_node) {
			const double amount = AeronConfigNode_Float(amount_node, NAN);
			if (!isfinite(amount) || amount < 0.0 || amount > 1.0) {
				return host_config_error(error, error_size,
										 "invalid 'video.motion_blur_amount': expected numeric value %s",
										 "from 0 through 1");
			}
			out->motion_blur_amount = (float)amount;
			*override_mask |= XWA_MODERN_VIDEO_OVERRIDE_MOTION_BLUR_AMOUNT;
		}
	}
	{
		const char* hud_scale_key = "video.hud_scale";
		const AeronConfigNode* hud_scale_node = AeronConfigFile_GetNode(config, hud_scale_key);
		if (hud_scale_node) {
			const double hud_scale = AeronConfigNode_Float(hud_scale_node, NAN);
			if (!isfinite(hud_scale) || hud_scale < XWA_MODERN_HUD_SCALE_MIN ||
				hud_scale > XWA_MODERN_HUD_SCALE_MAX) {
				return host_config_error(error, error_size, "invalid 'video.hud_scale': expected a number %s",
										 "from 0.5 to 2");
			}
			out->hud_scale = (float)hud_scale;
			*override_mask |= XWA_MODERN_VIDEO_OVERRIDE_HUD_SCALE;
		}
	}

	hdr_node = AeronConfigFile_GetNode(config, hdr_key);
	if (hdr_node) {
		if (AeronConfigNode_Type(hdr_node) != AERON_CONFIG_BOOL) {
			return host_config_error(error, error_size, "invalid video setting '%s'", hdr_key);
		}
		out->hdr_output = AeronConfigNode_Bool(hdr_node, 0);
		*override_mask |= XWA_MODERN_VIDEO_OVERRIDE_HDR;
	}

	/* video.sdr_content_gamma accepts 'srgb', '2.2', or '2.4'. Unquoted
	 * 2.2/2.4 parse as YAML floats, so both scalar shapes are accepted for
	 * hand edits. Ignored on Apple, where the platform behavior stays
	 * piecewise. */
	{
		const char* gamma_key = "video.sdr_content_gamma";
		const AeronConfigNode* gamma_node = AeronConfigFile_GetNode(config, gamma_key);
		if (gamma_node) {
			const char* text = AeronConfigNode_String(gamma_node, NULL);
			if (text) {
				if (strcmp(text, "srgb") == 0) {
					out->sdr_gamma = XWA_MODERN_SDR_GAMMA_SRGB;
				} else if (strcmp(text, "2.2") == 0) {
					out->sdr_gamma = XWA_MODERN_SDR_GAMMA_2_2;
				} else if (strcmp(text, "2.4") == 0) {
					out->sdr_gamma = XWA_MODERN_SDR_GAMMA_2_4;
				} else {
					return host_config_error(error, error_size, "invalid video setting '%s'", gamma_key);
				}
			} else if (AeronConfigNode_Type(gamma_node) == AERON_CONFIG_FLOAT) {
				const double gamma = AeronConfigNode_Float(gamma_node, 0.0);
				if (gamma > 2.19 && gamma < 2.21) {
					out->sdr_gamma = XWA_MODERN_SDR_GAMMA_2_2;
				} else if (gamma > 2.39 && gamma < 2.41) {
					out->sdr_gamma = XWA_MODERN_SDR_GAMMA_2_4;
				} else {
					return host_config_error(error, error_size, "invalid video setting '%s'", gamma_key);
				}
			} else {
				return host_config_error(error, error_size, "invalid video setting '%s'", gamma_key);
			}
			*override_mask |= XWA_MODERN_VIDEO_OVERRIDE_SDR_GAMMA;
		}
	}

	/* video.paper_white_nits accepts 'auto' or one of 100/150/200/250/300/400
	 * (quoted or bare numeric scalar). Ignored on Apple, where EDR reference
	 * white follows the system brightness. */
	{
		static const int paper_white_nits[] = { 0, 100, 150, 200, 250, 300, 400 };
		const char* white_key = "video.paper_white_nits";
		const AeronConfigNode* white_node = AeronConfigFile_GetNode(config, white_key);
		if (white_node) {
			const char* text = AeronConfigNode_String(white_node, NULL);
			long nits = -1;
			size_t index;
			if (text) {
				if (strcmp(text, "auto") == 0) {
					nits = 0;
				} else {
					char* end = NULL;
					nits = strtol(text, &end, 10);
					if (!end || *end != '\0') {
						nits = -1;
					}
				}
			} else if (AeronConfigNode_Type(white_node) == AERON_CONFIG_INT) {
				nits = (long)AeronConfigNode_Int(white_node, -1);
			}
			for (index = 0; index < sizeof paper_white_nits / sizeof paper_white_nits[0]; ++index) {
				if (nits == paper_white_nits[index]) {
					out->paper_white = (XwaModernPaperWhite)index;
					*override_mask |= XWA_MODERN_VIDEO_OVERRIDE_PAPER_WHITE;
					break;
				}
			}
			if (index >= sizeof paper_white_nits / sizeof paper_white_nits[0]) {
				return host_config_error(error, error_size, "invalid video setting '%s'", white_key);
			}
		}
	}
	return 1;
}

static int host_config_input_missing(int required, const char* key, char* error, size_t error_size) {
	return !required ||
		   host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'", key);
}

static int host_config_input_bool(const AeronConfigFile* config, const char* key, int required, int* out,
								  char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);

	if (!node) {
		return host_config_input_missing(required, key, error, error_size);
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		return host_config_error(error, error_size, "invalid input setting '%s'", key);
	}
	*out = AeronConfigNode_Bool(node, 0);
	return 1;
}

static int host_config_input_int(const AeronConfigFile* config, const char* key, int required, int min_value,
								 int max_value, int* out, char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	int64_t value;

	if (!node) {
		return host_config_input_missing(required, key, error, error_size);
	}
	value = AeronConfigNode_Int(node, 0);
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < min_value || value > max_value) {
		return host_config_error(error, error_size, "invalid input setting '%s'", key);
	}
	*out = (int)value;
	return 1;
}

static int host_config_input_scalars(const AeronConfigFile* config, int required, XwaModernInputOptions* out,
									 char* error, size_t error_size) {
	const AeronConfigNode* node;

	if (!host_config_input_bool(config, "input.mouse_flight", required, &out->mouse_flight_enabled, error,
								error_size) ||
		!host_config_input_int(config, "input.mouse_sensitivity", required, XWA_MODERN_MOUSE_SENSITIVITY_MIN,
							   XWA_MODERN_MOUSE_SENSITIVITY_MAX, &out->mouse_sensitivity, error,
							   error_size) ||
		!host_config_input_bool(config, "input.mouse_invert_y", required, &out->mouse_invert_y, error,
								error_size) ||
		!host_config_input_bool(config, "input.rumble_enabled", required, &out->rumble_enabled, error,
								error_size) ||
		!host_config_input_int(config, "input.rumble_strength", required, XWA_MODERN_RUMBLE_STRENGTH_MIN,
							   XWA_MODERN_RUMBLE_STRENGTH_MAX, &out->rumble_strength, error, error_size)) {
		return 0;
	}

	node = AeronConfigFile_GetNode(config, "input.mouse_mode");
	if (!node) {
		if (!host_config_input_missing(required, "input.mouse_mode", error, error_size)) {
			return 0;
		}
	} else {
		const char* value = AeronConfigNode_String(node, NULL);
		if (value && strcmp(value, "position") == 0) {
			out->mouse_mode = XWA_MODERN_MOUSE_MODE_POSITION;
		} else if (value && strcmp(value, "rate") == 0) {
			out->mouse_mode = XWA_MODERN_MOUSE_MODE_RATE;
		} else {
			return host_config_error(error, error_size, "invalid 'input.mouse_mode': expected %s",
									 "'position' or 'rate'");
		}
	}
	return 1;
}

static int host_config_validate_input(const XwaModernInputOptions* options, const char* path, char* error,
									  size_t error_size) {
	char detail[256];

	if (!XwaModernInputOptions_Validate(options, detail, sizeof detail)) {
		if (error && error_size) {
			snprintf(error, error_size, "invalid input configuration in %s: %s", path, detail);
		}
		return 0;
	}
	return 1;
}

/* The input section of the shipped file. Every key is required. */
static int host_config_shipped_input(const AeronConfigFile* config, XwaModernInputOptions* out,
									 XwaControllerProfile* gamepad_defaults, char* error, size_t error_size) {
	memset(out, 0, sizeof *out);
	return host_config_input_scalars(config, 1, out, error, error_size) &&
		   XwaControllerConfig_Parse(config, &out->controllers, error, error_size) &&
		   XwaControllerConfig_ReadProfile(config, "input.gamepad_defaults", AERON_CONTROLLER_KIND_GAMEPAD,
										   gamepad_defaults, error, error_size) &&
		   XwaKeyboardConfig_Read(config, &out->keyboard, error, error_size) &&
		   host_config_validate_input(out, "remaster/config.yaml", error, error_size);
}

/* The input section of the user file, over the shipped values: the scalar
 * keys override one by one, input.controllers replaces the shipped list, and
 * input.keyboard overrides action by action. */
static int host_config_user_input(const AeronConfigFile* config, const XwaModernInputOptions* defaults,
								  XwaModernInputOptions* out, char* error, size_t error_size) {
	*out = *defaults;
	if (!host_config_input_scalars(config, 0, out, error, error_size)) {
		return 0;
	}
	if (AeronConfigFile_Has(config, "input.controllers") &&
		!XwaControllerConfig_Parse(config, &out->controllers, error, error_size)) {
		return 0;
	}
	if (!XwaKeyboardConfig_Resolve(&defaults->keyboard, config, &out->keyboard, error, error_size)) {
		return 0;
	}
	return host_config_validate_input(out, "config.yaml", error, error_size);
}

/* Version 1 saved one shared gamepad profile and one shared joystick profile
 * under input.controller. They can't be tied to a device model, so they are
 * dropped and controllers start again from the defaults. Their rumble
 * settings move to input.rumble_enabled and input.rumble_strength. This
 * changes only the loaded document; the next input save writes the file. */
static int host_config_upgrade_input(AeronConfigFile* config, int* upgraded, char* error, size_t error_size) {
	static const char* const keys[] = { "rumble_enabled", "rumble_strength" };
	const AeronConfigNode* version = AeronConfigFile_GetNode(config, "version");
	AeronConfigError detail = { 0 };
	size_t i;

	*upgraded = 0;
	if ((version && AeronConfigNode_Int(version, 1) >= 2) ||
		!AeronConfigFile_Has(config, "input.controller")) {
		return 1;
	}
	for (i = 0; i < sizeof keys / sizeof keys[0]; ++i) {
		char old_key[64];
		char new_key[64];
		const AeronConfigNode* node;
		int ok = 1;

		snprintf(old_key, sizeof old_key, "input.controller.%s", keys[i]);
		snprintf(new_key, sizeof new_key, "input.%s", keys[i]);
		node = AeronConfigFile_GetNode(config, old_key);
		if (!node || AeronConfigFile_Has(config, new_key)) {
			continue;
		}
		if (AeronConfigNode_Type(node) == AERON_CONFIG_BOOL) {
			ok = AeronConfigFile_SetBool(config, new_key, AeronConfigNode_Bool(node, 0), &detail);
		} else if (AeronConfigNode_Type(node) == AERON_CONFIG_INT) {
			ok = AeronConfigFile_SetInt(config, new_key, AeronConfigNode_Int(node, 0), &detail);
		}
		if (!ok) {
			return host_config_error(error, error_size, "could not upgrade the input settings: %s",
									 detail.message);
		}
	}
	if (!AeronConfigFile_Remove(config, "input.controller", &detail) ||
		!AeronConfigFile_SetInt(config, "version", XWA_CONFIG_VERSION, &detail)) {
		return host_config_error(error, error_size, "could not upgrade the input settings: %s",
								 detail.message);
	}
	*upgraded = 1;
	Aeron_LogInfo("xwa.config", "controller settings were reset: each controller model now has its own "
								"profile, and gamepads get the default layout");
	return 1;
}

int XwaHostConfig_Load(AeronVfs* vfs, XwaHostConfig* out, char* error, size_t error_size) {
	static const char* path = "config.yaml";
	AeronConfigFile* config = NULL;
	if (!vfs || !out) {
		return host_config_error(error, error_size, "cannot load %s", path);
	}
	memset(out, 0, sizeof *out);
	out->flight_simulation_step_ticks = 1;
	out->paths.use_mod = 1;
	if (!host_config_load_shipped_config(vfs, out, error, error_size)) {
		return 0;
	}
	if (!AeronVfs_Exists(vfs, AERON_VFS_ROOT_USER, path)) {
		return 1;
	}
	if (!AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_USER, path, &config)) {
		return host_config_error(error, error_size, "user configuration is invalid: %s", path);
	}
	if (AeronConfigNode_Type(AeronConfigFile_Root(config)) != AERON_CONFIG_MAP) {
		AeronConfigFile_Destroy(config);
		return host_config_error(error, error_size, "user configuration root must be a mapping: %s", path);
	}
	{
		const AeronConfigNode* models = AeronConfigFile_GetNode(config, "models");
		const AeronConfigNode* assets = AeronConfigFile_GetNode(config, "assets");
		const AeronConfigNode* environment_maps = AeronConfigFile_GetNode(config, "environment_maps");
		const AeronConfigNode* video = AeronConfigFile_GetNode(config, "video");
		const AeronConfigNode* input = AeronConfigFile_GetNode(config, "input");
		if (!XwaInstallPaths_CheckVersion(config, path, error, error_size)) {
			AeronConfigFile_Destroy(config);
			return 0;
		}
		if (models && AeronConfigNode_Type(models) != AERON_CONFIG_MAP) {
			AeronConfigFile_Destroy(config);
			return host_config_error(error, error_size, "'models' must be a mapping in %s", path);
		}
		if (assets && AeronConfigNode_Type(assets) != AERON_CONFIG_MAP) {
			AeronConfigFile_Destroy(config);
			return host_config_error(error, error_size, "'assets' must be a mapping in %s", path);
		}
		if (environment_maps && AeronConfigNode_Type(environment_maps) != AERON_CONFIG_MAP) {
			AeronConfigFile_Destroy(config);
			return host_config_error(error, error_size, "'environment_maps' must be a mapping in %s", path);
		}
		if (video && AeronConfigNode_Type(video) != AERON_CONFIG_MAP) {
			AeronConfigFile_Destroy(config);
			return host_config_error(error, error_size, "'video' must be a mapping in %s", path);
		}
		if (input && AeronConfigNode_Type(input) != AERON_CONFIG_MAP) {
			AeronConfigFile_Destroy(config);
			return host_config_error(error, error_size, "'input' must be a mapping in %s", path);
		}
	}
	if (!host_config_upgrade_input(config, &out->input_upgraded, error, error_size)) {
		AeronConfigFile_Destroy(config);
		return 0;
	}
	if (AeronConfigFile_Has(config, "input.gamepad_defaults")) {
		Aeron_LogWarn("xwa.config",
					  "'input.gamepad_defaults' in %s is ignored; it is read from the shipped file", path);
	}
	if (AeronConfigFile_GetNode(config, "paths.resources")) {
		Aeron_LogWarn("xwa.config",
					  "deprecated setting 'paths.resources' is ignored; resources are application-owned");
	}
	const int valid =
		XwaInstallPaths_Read(config, &out->paths, error, error_size) &&
		host_config_simulation_step(config, &out->flight_simulation_step_ticks, error, error_size) &&
		host_config_remaster_options(config, 0, out, error, error_size) &&
		host_config_video_options(config, &out->video_options, &out->video_options_override_mask, error,
								  error_size) &&
		host_config_user_input(config, &out->input_defaults, &out->input_options, error, error_size) &&
		host_config_option_sections(config, 0, error, error_size);
	AeronConfigFile_Destroy(config);
	return valid;
}

static int host_config_prepare_user_document(AeronVfs* vfs, AeronConfigFile** document, char* error,
											 size_t error_size) {
	AeronConfigError config_error = { 0 };

	if (AeronVfs_Exists(vfs, AERON_VFS_ROOT_USER, "config.yaml")) {
		if (AeronConfigFile_LoadYamlEx(vfs, AERON_VFS_ROOT_USER, "config.yaml", document, &config_error)) {
			return 1;
		}
		return host_config_error(error, error_size, "could not update user configuration: %s",
								 config_error.message);
	}
	if (!AeronConfigFile_CreateMap(AERON_VFS_ROOT_USER, "config.yaml", document, &config_error) ||
		!AeronConfigFile_SetInt(*document, "version", XWA_CONFIG_VERSION, &config_error)) {
		AeronConfigFile_Destroy(*document);
		*document = NULL;
		return host_config_error(error, error_size, "could not create user configuration: %s",
								 config_error.message);
	}
	return 1;
}

static int host_config_save_user_document(AeronVfs* vfs, AeronConfigFile* document, char* error,
										  size_t error_size) {
	AeronConfigError config_error = { 0 };
	int saved = AeronConfigFile_SaveYaml(vfs, document, &config_error);

	AeronConfigFile_Destroy(document);
	if (!saved) {
		return host_config_error(error, error_size, "could not save user configuration: %s",
								 config_error.message);
	}
	return 1;
}

static int host_config_set_video_options(AeronConfigFile* document, const XwaModernVideoOptions* options,
										 AeronConfigError* error) {
	static const char* const window_mode_names[] = { "windowed", "fullscreen" };
	static const char* const quality_names[] = { "off", "low", "high" };
	static const char* const shadow_quality_names[] = { "standard", "high" };
	static const char* const fsr_names[] = { "off", "performance", "balanced", "quality", "native_aa" };
	static const char* const msaa_names[] = { "off", "2x", "4x", "8x" };
	static const char* const sdr_gamma_names[] = { "2.2", "2.4", "srgb" };
	static const char* const paper_white_names[] = { "auto", "100", "150", "200", "250", "300", "400" };

	if (!options || options->window_mode < XWA_MODERN_WINDOW_MODE_WINDOWED ||
		options->window_mode > XWA_MODERN_WINDOW_MODE_FULLSCREEN ||
		options->ssao_quality < XWA_MODERN_SSAO_OFF || options->ssao_quality > XWA_MODERN_SSAO_HIGH ||
		options->shadow_quality < XWA_MODERN_SHADOW_STANDARD ||
		options->shadow_quality > XWA_MODERN_SHADOW_HIGH || options->fsr_upscaling < XWA_MODERN_FSR_OFF ||
		options->fsr_upscaling > XWA_MODERN_FSR_NATIVE_AA || options->msaa < XWA_MODERN_MSAA_OFF ||
		options->msaa > XWA_MODERN_MSAA_8X ||
		(options->fsr_upscaling != XWA_MODERN_FSR_OFF && options->msaa != XWA_MODERN_MSAA_OFF) ||
		options->motion_blur_quality < XWA_MODERN_MOTION_BLUR_OFF ||
		options->motion_blur_quality > XWA_MODERN_MOTION_BLUR_HIGH ||
		!isfinite(options->motion_blur_amount) || options->motion_blur_amount < 0.0f ||
		options->motion_blur_amount > 1.0f || options->sdr_gamma < XWA_MODERN_SDR_GAMMA_2_2 ||
		options->sdr_gamma > XWA_MODERN_SDR_GAMMA_SRGB ||
		options->paper_white < XWA_MODERN_PAPER_WHITE_AUTO ||
		options->paper_white > XWA_MODERN_PAPER_WHITE_400 || !isfinite(options->hud_scale) ||
		options->hud_scale < XWA_MODERN_HUD_SCALE_MIN || options->hud_scale > XWA_MODERN_HUD_SCALE_MAX) {
		return 0;
	}
	return AeronConfigFile_SetString(document, "video.window_mode", window_mode_names[options->window_mode],
									 error) &&
		   AeronConfigFile_SetString(document, "video.ssao_quality", quality_names[options->ssao_quality],
									 error) &&
		   AeronConfigFile_SetString(document, "video.shadow_quality",
									 shadow_quality_names[options->shadow_quality], error) &&
		   AeronConfigFile_SetString(document, "video.fsr_upscaling", fsr_names[options->fsr_upscaling],
									 error) &&
		   AeronConfigFile_SetString(document, "video.msaa", msaa_names[options->msaa], error) &&
		   AeronConfigFile_SetString(document, "video.motion_blur_quality",
									 quality_names[options->motion_blur_quality], error) &&
		   AeronConfigFile_SetFloat(document, "video.motion_blur_amount", options->motion_blur_amount,
									error) &&
		   AeronConfigFile_SetBool(document, "video.hdr_output", options->hdr_output, error) &&
		   AeronConfigFile_SetString(document, "video.sdr_content_gamma", sdr_gamma_names[options->sdr_gamma],
									 error) &&
		   AeronConfigFile_SetString(document, "video.paper_white_nits",
									 paper_white_names[options->paper_white], error) &&
		   AeronConfigFile_SetFloat(document, "video.hud_scale", options->hud_scale, error);
}

static int host_config_remove_key(AeronConfigFile* document, const char* key, AeronConfigError* error) {
	return !AeronConfigFile_Has(document, key) || AeronConfigFile_Remove(document, key, error);
}

/* The scalar input keys are written only when they differ from the shipped
 * value, and removed when they are the same. */
static int host_config_set_input_bool(AeronConfigFile* document, const char* key, int value, int shipped,
									  AeronConfigError* error) {
	return (value != 0) == (shipped != 0) ? host_config_remove_key(document, key, error)
										  : AeronConfigFile_SetBool(document, key, value != 0, error);
}

static int host_config_set_input_int(AeronConfigFile* document, const char* key, int value, int shipped,
									 AeronConfigError* error) {
	return value == shipped ? host_config_remove_key(document, key, error)
							: AeronConfigFile_SetInt(document, key, value, error);
}

static int host_config_set_input_options(AeronConfigFile* document, const XwaModernInputOptions* options,
										 const XwaModernInputOptions* defaults, AeronConfigError* error) {
	static const char* const mode_names[] = { "position", "rate" };
	int ok;

	ok = host_config_set_input_bool(document, "input.mouse_flight", options->mouse_flight_enabled,
									defaults->mouse_flight_enabled, error) &&
		 host_config_set_input_int(document, "input.mouse_sensitivity", options->mouse_sensitivity,
								   defaults->mouse_sensitivity, error) &&
		 host_config_set_input_bool(document, "input.mouse_invert_y", options->mouse_invert_y,
									defaults->mouse_invert_y, error) &&
		 host_config_set_input_bool(document, "input.rumble_enabled", options->rumble_enabled,
									defaults->rumble_enabled, error) &&
		 host_config_set_input_int(document, "input.rumble_strength", options->rumble_strength,
								   defaults->rumble_strength, error);
	if (ok) {
		ok = options->mouse_mode == defaults->mouse_mode
				 ? host_config_remove_key(document, "input.mouse_mode", error)
				 : AeronConfigFile_SetString(document, "input.mouse_mode", mode_names[options->mouse_mode],
											 error);
	}
	/* input.controllers replaces the shipped list as a whole. */
	if (ok) {
		ok = XwaControllerOptions_Equals(&options->controllers, &defaults->controllers)
				 ? host_config_remove_key(document, "input.controllers", error)
				 : XwaControllerConfig_Write(document, &options->controllers, error);
	}
	/* input.keyboard lists every action, with empty lists for the ones that
	 * have no key, so removed defaults stay removed. */
	if (ok) {
		ok = XwaKeyboardMapping_Equal(&options->keyboard, &defaults->keyboard)
				 ? host_config_remove_key(document, "input.keyboard", error)
				 : XwaKeyboardConfig_Write(document, &options->keyboard, error);
	}
	/* The version 1 input.controller section is replaced by the keys above. */
	return ok && host_config_remove_key(document, "input.controller", error) &&
		   AeronConfigFile_SetInt(document, "version", XWA_CONFIG_VERSION, error);
}

static int host_config_compare_controller_binding(const void* left, const void* right) {
	const XwaInputActionBinding* a = left;
	const XwaInputActionBinding* b = right;

	if (a->action != b->action) {
		return (int)a->action - (int)b->action;
	}
	if (a->source.kind != b->source.kind) {
		return (int)a->source.kind - (int)b->source.kind;
	}
	if (a->source.index != b->source.index) {
		return (int)a->source.index - (int)b->source.index;
	}
	return (int)a->source.hat_direction - (int)b->source.hat_direction;
}

/* The written file lists controller bindings in action order, so compare
 * the bindings in that order. */
static void host_config_sort_controller_bindings(XwaControllerOptions* options) {
	size_t i;

	for (i = 0; i < options->count; ++i) {
		XwaControllerProfile* profile = &options->models[i].profile;
		qsort(profile->bindings, profile->binding_count, sizeof profile->bindings[0],
			  host_config_compare_controller_binding);
	}
}

int XwaHostConfig_SaveInputOptions(AeronVfs* vfs, const XwaModernInputOptions* options, char* error,
								   size_t error_size) {
	/* Each copy holds every controller and keyboard binding. */
	static XwaModernInputOptions defaults;
	static XwaModernInputOptions expected;
	static XwaModernInputOptions saved;
	XwaControllerProfile gamepad_defaults;
	AeronConfigFile* shipped = NULL;
	AeronConfigFile* document = NULL;
	AeronConfigError config_error = { 0 };
	char detail[512];
	int valid;

	if (!vfs || !options) {
		return host_config_error(error, error_size, "cannot save invalid input settings to %s",
								 "config.yaml");
	}
	if (!AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_RESOURCE, "remaster/config.yaml", &shipped)) {
		return host_config_error(error, error_size,
								 "required shipped configuration unavailable or invalid: %s",
								 "remaster/config.yaml");
	}
	valid = host_config_shipped_input(shipped, &defaults, &gamepad_defaults, error, error_size);
	AeronConfigFile_Destroy(shipped);
	if (!valid) {
		return 0;
	}
	if (!host_config_prepare_user_document(vfs, &document, error, error_size)) {
		return 0;
	}
	if (!host_config_set_input_options(document, options, &defaults, &config_error)) {
		AeronConfigFile_Destroy(document);
		return host_config_error(error, error_size, "could not update user configuration: %s",
								 config_error.message[0] ? config_error.message : "invalid input settings");
	}
	/* Read the updated document back as the next start will, and write it
	 * only if it gives the same settings. */
	expected = *options;
	XwaKeyboardMapping_Sort(&expected.keyboard);
	host_config_sort_controller_bindings(&expected.controllers);
	if (!host_config_user_input(document, &defaults, &saved, detail, sizeof detail)) {
		AeronConfigFile_Destroy(document);
		return host_config_error(error, error_size, "the input settings would not load again: %s", detail);
	}
	host_config_sort_controller_bindings(&saved.controllers);
	if (!XwaModernInputOptions_Equal(&saved, &expected)) {
		AeronConfigFile_Destroy(document);
		return host_config_error(error, error_size, "the input settings would not load again: %s",
								 "the saved values differ");
	}
	return host_config_save_user_document(vfs, document, error, error_size);
}

int XwaHostConfig_SaveVideoOptions(AeronVfs* vfs, const XwaModernVideoOptions* options, char* error,
								   size_t error_size) {
	AeronConfigFile* document = NULL;
	AeronConfigError config_error = { 0 };

	if (!vfs || !options) {
		return host_config_error(error, error_size, "cannot save invalid video settings to %s",
								 "config.yaml");
	}
	if (!host_config_prepare_user_document(vfs, &document, error, error_size)) {
		return 0;
	}
	if (!host_config_set_video_options(document, options, &config_error)) {
		AeronConfigFile_Destroy(document);
		return host_config_error(error, error_size, "could not update user configuration: %s",
								 config_error.message[0] ? config_error.message : "invalid video settings");
	}
	return host_config_save_user_document(vfs, document, error, error_size);
}

int XwaHostConfig_SaveSwitch(AeronVfs* vfs, const char* key, int value, char* error, size_t error_size) {
	AeronConfigFile* document = NULL;
	AeronConfigError config_error = { 0 };

	if (!vfs || !key) {
		return host_config_error(error, error_size, "cannot save a switch to %s", "config.yaml");
	}
	if (!host_config_prepare_user_document(vfs, &document, error, error_size)) {
		return 0;
	}
	if (!AeronConfigFile_SetBool(document, key, value != 0, &config_error)) {
		AeronConfigFile_Destroy(document);
		return host_config_error(error, error_size, "could not update user configuration: %s",
								 config_error.message);
	}
	return host_config_save_user_document(vfs, document, error, error_size);
}
