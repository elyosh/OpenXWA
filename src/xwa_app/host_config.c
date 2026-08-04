#include "host_config.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "aeron/numeric.h"
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

static int host_config_optional_path(const AeronConfigFile* config, const char* key, char* out,
									 size_t capacity, char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	const char* value;

	if (!node) {
		out[0] = '\0';
		return 1;
	}
	value = AeronConfigNode_String(node, NULL);
	if (!value) {
		return host_config_error(error, error_size, "invalid path setting '%s'", key);
	}
	if (!value[0]) {
		out[0] = '\0';
		return 1;
	}
	if (strlen(value) >= capacity) {
		return host_config_error(error, error_size, "configured path is too long: '%s'", key);
	}
	snprintf(out, capacity, "%s", value);
	return 1;
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

static int host_config_prefer_original_2d(const AeronConfigFile* config, int required, int* out, char* error,
										  size_t error_size) {
	const char* key = "assets.prefer_original_2d";
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	if (!node) {
		if (required) {
			return host_config_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									 key);
		}
		return 1;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		return host_config_error(error, error_size, "invalid 'assets.prefer_original_2d': expected %s",
								 "boolean");
	}
	*out = AeronConfigNode_Bool(node, 0);
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
		   host_config_prefer_original_2d(config, required, &out->prefer_original_2d, error, error_size);
}

static int host_config_input_options(const AeronConfigFile* config, int required, XwaModernInputOptions* out,
									 char* error, size_t error_size);

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
	valid = host_config_remaster_options(config, 1, out, error, error_size) &&
			host_config_input_options(config, 1, &out->input_options, error, error_size);
	if (valid) {
		out->input_defaults = out->input_options;
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

static int host_config_input_float(const AeronConfigFile* config, const char* key, int required,
								   double min_value, double max_value, float* out, char* error,
								   size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	double value;

	if (!node) {
		return host_config_input_missing(required, key, error, error_size);
	}
	value = AeronConfigNode_Float(node, NAN);
	/* Affected builds wrote optional user values with the active locale's
	 * decimal comma. Keep canonical YAML strict while accepting that output. */
	if (!isfinite(value) && !required) {
		const char* text = AeronConfigNode_String(node, NULL);
		char normalized[64];
		char* comma;
		size_t length;

		if (text) {
			length = strlen(text);
			comma = strchr(text, ',');
			if (length > 0 && length < sizeof(normalized) && comma && comma != text && comma[1] != '\0' &&
				!strchr(text, '.') && !strchr(comma + 1, ',')) {
				memcpy(normalized, text, length + 1);
				normalized[comma - text] = '.';
				(void)Aeron_ParseAsciiDouble(normalized, length, &value);
			}
		}
	}
	if (!isfinite(value) || value < min_value || value > max_value) {
		return host_config_error(error, error_size, "invalid input setting '%s'", key);
	}
	*out = (float)value;
	return 1;
}

static int host_config_input_string(const AeronConfigFile* config, const char* key, int required, char* out,
									size_t capacity, char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	const char* value;

	if (!node) {
		return host_config_input_missing(required, key, error, error_size);
	}
	value = AeronConfigNode_String(node, NULL);
	if (!value || strlen(value) >= capacity) {
		return host_config_error(error, error_size, "invalid input setting '%s'", key);
	}
	snprintf(out, capacity, "%s", value);
	return 1;
}

static int host_config_gamepad_axis_source(const AeronConfigFile* config, const char* key, int required,
										   int* out, char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	const char* value;
	AeronGamepadAxis axis;

	if (!node) {
		return host_config_input_missing(required, key, error, error_size);
	}
	value = AeronConfigNode_String(node, NULL);
	if (value && strcmp(value, "none") == 0) {
		*out = -1;
		return 1;
	}
	axis = Aeron_GamepadAxisFromName(value);
	if (axis >= AERON_GAMEPAD_AXIS_COUNT) {
		return host_config_error(error, error_size, "invalid input setting '%s'", key);
	}
	*out = (int)axis;
	return 1;
}

static int host_config_controller_digital_binding(const AeronConfigFile* config, const char* key,
												  int required, int gamepad,
												  AeronControllerDigitalSource* out, char* error,
												  size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	const AeronConfigNode* axis_node;
	const AeronConfigNode* direction_node;
	const AeronConfigNode* threshold_node;
	const char* value;
	int source;

	if (!node) {
		return host_config_input_missing(required, key, error, error_size);
	}
	out->threshold = XWA_CONTROLLER_DIGITAL_THRESHOLD_DEFAULT;
	if (AeronConfigNode_Type(node) == AERON_CONFIG_MAP) {
		axis_node = AeronConfigNode_MapGet(node, "axis");
		direction_node = AeronConfigNode_MapGet(node, "direction");
		threshold_node = AeronConfigNode_MapGet(node, "threshold");
		if (!axis_node) {
			return host_config_error(error, error_size, "invalid input setting '%s'", key);
		}
		if (gamepad) {
			AeronGamepadAxis axis = Aeron_GamepadAxisFromName(AeronConfigNode_String(axis_node, NULL));
			if (axis >= AERON_GAMEPAD_AXIS_COUNT) {
				return host_config_error(error, error_size, "invalid input setting '%s'", key);
			}
			source = (int)axis;
		} else {
			const int64_t axis = AeronConfigNode_Int(axis_node, -1);
			if (AeronConfigNode_Type(axis_node) != AERON_CONFIG_INT || axis < 0 ||
				axis >= AERON_CONTROLLER_AXIS_MAX) {
				return host_config_error(error, error_size, "invalid input setting '%s'", key);
			}
			source = (int)axis;
		}
		value = direction_node ? AeronConfigNode_String(direction_node, NULL) : "positive";
		if (value && strcmp(value, "positive") == 0) {
			out->kind = AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE;
		} else if (value && strcmp(value, "negative") == 0) {
			out->kind = AERON_CONTROLLER_DIGITAL_AXIS_NEGATIVE;
		} else {
			return host_config_error(error, error_size, "invalid input setting '%s'", key);
		}
		if (threshold_node) {
			const double threshold = AeronConfigNode_Float(threshold_node, NAN);
			if (!isfinite(threshold) || threshold <= 0.0 || threshold > 1.0) {
				return host_config_error(error, error_size, "invalid input setting '%s'", key);
			}
			out->threshold = (float)threshold;
		}
		out->index = (uint8_t)source;
		return 1;
	}
	value = AeronConfigNode_String(node, NULL);
	if (value && strcmp(value, "none") == 0) {
		out->kind = AERON_CONTROLLER_DIGITAL_NONE;
		out->index = 0;
		return 1;
	}
	if (gamepad) {
		const AeronGamepadButton button = Aeron_GamepadButtonFromName(value);
		if (button >= AERON_GAMEPAD_BUTTON_COUNT) {
			return host_config_error(error, error_size, "invalid input setting '%s'", key);
		}
		source = (int)button;
	} else {
		const int64_t button = AeronConfigNode_Int(node, -1);
		if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || button < 0 ||
			button >= AERON_CONTROLLER_BUTTON_MAX) {
			return host_config_error(error, error_size, "invalid input setting '%s'", key);
		}
		source = (int)button;
	}
	out->kind = AERON_CONTROLLER_DIGITAL_BUTTON;
	out->index = (uint8_t)source;
	return 1;
}

static int host_config_raw_source(const AeronConfigFile* config, const char* key, int required, int maximum,
								  int* out, char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	const char* text;
	int64_t value;

	if (!node) {
		return host_config_input_missing(required, key, error, error_size);
	}
	text = AeronConfigNode_String(node, NULL);
	if (text && strcmp(text, "none") == 0) {
		*out = -1;
		return 1;
	}
	value = AeronConfigNode_Int(node, -1);
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 0 || value >= maximum) {
		return host_config_error(error, error_size, "invalid input setting '%s'", key);
	}
	*out = (int)value;
	return 1;
}

static int host_config_controller_axes(const AeronConfigFile* config, int required,
									   XwaControllerOptions* controller, char* error, size_t error_size) {
	static const char* const axis_names[XWA_CONTROLLER_LOGICAL_AXIS_COUNT] = { "yaw", "pitch", "throttle",
																			   "roll" };
	char key[128];
	int i;

	for (i = 0; i < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++i) {
		snprintf(key, sizeof(key), "input.controller.gamepad.axes.%s.source", axis_names[i]);
		if (!host_config_gamepad_axis_source(config, key, required, &controller->gamepad.axes[i].source,
											 error, error_size)) {
			return 0;
		}
		snprintf(key, sizeof(key), "input.controller.gamepad.axes.%s.invert", axis_names[i]);
		if (!host_config_input_bool(config, key, required, &controller->gamepad.axes[i].invert, error,
									error_size)) {
			return 0;
		}
		snprintf(key, sizeof(key), "input.controller.gamepad.axes.%s.deadzone", axis_names[i]);
		if (!host_config_input_float(config, key, required, 0.0, 1.0, &controller->gamepad.axes[i].deadzone,
									 error, error_size)) {
			return 0;
		}
		snprintf(key, sizeof(key), "input.controller.joystick.axes.%s.source", axis_names[i]);
		if (!host_config_raw_source(config, key, required, AERON_CONTROLLER_AXIS_MAX,
									&controller->joystick.axes[i].source, error, error_size)) {
			return 0;
		}
		snprintf(key, sizeof(key), "input.controller.joystick.axes.%s.invert", axis_names[i]);
		if (!host_config_input_bool(config, key, required, &controller->joystick.axes[i].invert, error,
									error_size)) {
			return 0;
		}
		snprintf(key, sizeof(key), "input.controller.joystick.axes.%s.deadzone", axis_names[i]);
		if (!host_config_input_float(config, key, required, 0.0, 1.0, &controller->joystick.axes[i].deadzone,
									 error, error_size)) {
			return 0;
		}
	}
	return 1;
}

static int host_config_controller_buttons(const AeronConfigFile* config, int required,
										  XwaControllerOptions* controller, char* error, size_t error_size) {
	char key[128];
	int i;

	for (i = 0; i < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++i) {
		snprintf(key, sizeof(key), "input.controller.gamepad.buttons.%d", i + 1);
		if (!host_config_controller_digital_binding(config, key, required, 1, &controller->gamepad.buttons[i],
													error, error_size)) {
			return 0;
		}
		snprintf(key, sizeof(key), "input.controller.joystick.buttons.%d", i + 1);
		if (!host_config_controller_digital_binding(config, key, required, 0,
													&controller->joystick.buttons[i], error, error_size)) {
			return 0;
		}
	}
	return 1;
}

static int host_config_controller_pov(const AeronConfigFile* config, int required,
									  XwaControllerOptions* controller, char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, "input.controller.gamepad.pov");
	const char* value;

	if (!node) {
		if (!host_config_input_missing(required, "input.controller.gamepad.pov", error, error_size)) {
			return 0;
		}
	} else {
		value = AeronConfigNode_String(node, NULL);
		if (value && strcmp(value, "dpad") == 0) {
			controller->gamepad.pov_source = 1;
		} else if (value && strcmp(value, "none") == 0) {
			controller->gamepad.pov_source = 0;
		} else {
			return host_config_error(error, error_size, "invalid input setting '%s'",
									 "input.controller.gamepad.pov");
		}
	}
	return host_config_raw_source(config, "input.controller.joystick.pov_hat", required,
								  AERON_CONTROLLER_HAT_MAX, &controller->joystick.pov_source, error,
								  error_size);
}

static int host_config_controller_actions(const AeronConfigFile* config, int required,
										  const char* profile_name, XwaControllerProfile* profile,
										  char* error, size_t error_size) {
	char key[96];
	const AeronConfigNode* node;
	size_t count;
	int i;

	snprintf(key, sizeof(key), "input.controller.%s.actions", profile_name);
	node = AeronConfigFile_GetNode(config, key);
	if (!node) {
		return host_config_input_missing(required, key, error, error_size);
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE) {
		return host_config_error(error, error_size, "invalid input setting '%s'", key);
	}
	count = AeronConfigNode_SequenceCount(node);
	if (count != XWA_CONTROLLER_ACTION_COUNT) {
		return host_config_error(error, error_size, "invalid input setting '%s'", key);
	}
	for (i = 0; i < XWA_CONTROLLER_ACTION_COUNT; ++i) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, (size_t)i);
		int64_t value = AeronConfigNode_Int(item, -1);
		if (AeronConfigNode_Type(item) != AERON_CONFIG_INT || value < 0 || value > UINT16_MAX) {
			return host_config_error(error, error_size, "invalid input setting '%s'", key);
		}
		profile->actions[i] = (uint16_t)value;
	}
	return 1;
}

static int host_config_controller_options(const AeronConfigFile* config, int required,
										  XwaControllerOptions* controller, char* error, size_t error_size) {
	if (!host_config_input_string(config, "input.controller.device.guid", required, controller->device.guid,
								  sizeof(controller->device.guid), error, error_size) ||
		!host_config_input_string(config, "input.controller.device.path", required, controller->device.path,
								  sizeof(controller->device.path), error, error_size) ||
		!host_config_input_int(config, "input.controller.device.ordinal", required, 0,
							   XWA_CONTROLLER_DEVICE_ORDINAL_MAX, &controller->device.ordinal, error,
							   error_size)) {
		return 0;
	}
	return host_config_input_bool(config, "input.controller.roll_enabled", required,
								  &controller->roll_enabled, error, error_size) &&
		   host_config_input_bool(config, "input.controller.rumble_enabled", required,
								  &controller->rumble_enabled, error, error_size) &&
		   host_config_input_int(config, "input.controller.rumble_strength", required,
								 XWA_CONTROLLER_RUMBLE_STRENGTH_MIN, XWA_CONTROLLER_RUMBLE_STRENGTH_MAX,
								 &controller->rumble_strength, error, error_size) &&
		   host_config_controller_axes(config, required, controller, error, error_size) &&
		   host_config_controller_buttons(config, required, controller, error, error_size) &&
		   host_config_controller_pov(config, required, controller, error, error_size) &&
		   host_config_controller_actions(config, required, "gamepad", &controller->gamepad, error,
										  error_size) &&
		   host_config_controller_actions(config, required, "joystick", &controller->joystick, error,
										  error_size);
}

static int host_config_input_options(const AeronConfigFile* config, int required, XwaModernInputOptions* out,
									 char* error, size_t error_size) {
	const AeronConfigNode* node;

	if (!host_config_input_bool(config, "input.mouse_flight", required, &out->mouse_flight_enabled, error,
								error_size) ||
		!host_config_input_int(config, "input.mouse_sensitivity", required, XWA_MODERN_MOUSE_SENSITIVITY_MIN,
							   XWA_MODERN_MOUSE_SENSITIVITY_MAX, &out->mouse_sensitivity, error,
							   error_size) ||
		!host_config_input_bool(config, "input.mouse_invert_y", required, &out->mouse_invert_y, error,
								error_size) ||
		!host_config_controller_options(config, required, &out->controller, error, error_size)) {
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
	if (!XwaModernInputOptions_Validate(out)) {
		return host_config_error(error, error_size, "invalid input configuration in %s",
								 required ? "remaster/config.yaml" : "config.yaml");
	}
	return 1;
}

static int host_config_validate_input_maps(const AeronConfigFile* config, char* error, size_t error_size) {
	static const char* const map_paths[] = {
		"input.controller",
		"input.controller.device",
		"input.controller.gamepad",
		"input.controller.gamepad.axes",
		"input.controller.gamepad.axes.yaw",
		"input.controller.gamepad.axes.pitch",
		"input.controller.gamepad.axes.throttle",
		"input.controller.gamepad.axes.roll",
		"input.controller.gamepad.buttons",
		"input.controller.joystick",
		"input.controller.joystick.axes",
		"input.controller.joystick.axes.yaw",
		"input.controller.joystick.axes.pitch",
		"input.controller.joystick.axes.throttle",
		"input.controller.joystick.axes.roll",
		"input.controller.joystick.buttons",
	};
	size_t i;

	for (i = 0; i < sizeof(map_paths) / sizeof(map_paths[0]); ++i) {
		const AeronConfigNode* node = AeronConfigFile_GetNode(config, map_paths[i]);
		if (node && AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
			return host_config_error(error, error_size, "'%s' must be a mapping", map_paths[i]);
		}
	}
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
		const AeronConfigNode* version = AeronConfigFile_GetNode(config, "version");
		const AeronConfigNode* paths = AeronConfigFile_GetNode(config, "paths");
		const AeronConfigNode* models = AeronConfigFile_GetNode(config, "models");
		const AeronConfigNode* assets = AeronConfigFile_GetNode(config, "assets");
		const AeronConfigNode* video = AeronConfigFile_GetNode(config, "video");
		const AeronConfigNode* input = AeronConfigFile_GetNode(config, "input");
		if (version &&
			(AeronConfigNode_Type(version) != AERON_CONFIG_INT || AeronConfigNode_Int(version, 0) != 1)) {
			AeronConfigFile_Destroy(config);
			return host_config_error(error, error_size, "unsupported configuration version in %s", path);
		}
		if (paths && AeronConfigNode_Type(paths) != AERON_CONFIG_MAP) {
			AeronConfigFile_Destroy(config);
			return host_config_error(error, error_size, "'paths' must be a mapping in %s", path);
		}
		if (models && AeronConfigNode_Type(models) != AERON_CONFIG_MAP) {
			AeronConfigFile_Destroy(config);
			return host_config_error(error, error_size, "'models' must be a mapping in %s", path);
		}
		if (assets && AeronConfigNode_Type(assets) != AERON_CONFIG_MAP) {
			AeronConfigFile_Destroy(config);
			return host_config_error(error, error_size, "'assets' must be a mapping in %s", path);
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
	if (AeronConfigFile_GetNode(config, "paths.resources")) {
		Aeron_LogWarn("xwa.config",
					  "deprecated setting 'paths.resources' is ignored; resources are application-owned");
	}
	const int valid =
		host_config_optional_path(config, "paths.game_data", out->game_data_path, sizeof out->game_data_path,
								  error, error_size) &&
		host_config_simulation_step(config, &out->flight_simulation_step_ticks, error, error_size) &&
		host_config_remaster_options(config, 0, out, error, error_size) &&
		host_config_video_options(config, &out->video_options, &out->video_options_override_mask, error,
								  error_size) &&
		host_config_validate_input_maps(config, error, error_size) &&
		host_config_input_options(config, 0, &out->input_options, error, error_size);
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
		!AeronConfigFile_SetInt(*document, "version", 1, &config_error)) {
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
		options->shadow_quality > XWA_MODERN_SHADOW_HIGH ||
		options->fsr_upscaling < XWA_MODERN_FSR_OFF || options->fsr_upscaling > XWA_MODERN_FSR_NATIVE_AA ||
		options->msaa < XWA_MODERN_MSAA_OFF || options->msaa > XWA_MODERN_MSAA_8X ||
		(options->fsr_upscaling != XWA_MODERN_FSR_OFF && options->msaa != XWA_MODERN_MSAA_OFF) ||
		options->motion_blur_quality < XWA_MODERN_MOTION_BLUR_OFF ||
		options->motion_blur_quality > XWA_MODERN_MOTION_BLUR_HIGH ||
		!isfinite(options->motion_blur_amount) || options->motion_blur_amount < 0.0f ||
		options->motion_blur_amount > 1.0f || options->sdr_gamma < XWA_MODERN_SDR_GAMMA_2_2 ||
		options->sdr_gamma > XWA_MODERN_SDR_GAMMA_SRGB ||
		options->paper_white < XWA_MODERN_PAPER_WHITE_AUTO ||
		options->paper_white > XWA_MODERN_PAPER_WHITE_400) {
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
									 paper_white_names[options->paper_white], error);
}

int XwaHostConfig_SaveGameDataPath(AeronVfs* vfs, const char* game_data_path, char* error,
								   size_t error_size) {
	AeronConfigFile* document = NULL;
	AeronConfigError config_error = { 0 };

	if (!vfs || !game_data_path || !game_data_path[0] ||
		strlen(game_data_path) >= XWA_HOST_CONFIG_PATH_CAPACITY) {
		return host_config_error(error, error_size, "cannot save invalid path to %s", "config.yaml");
	}
	if (!host_config_prepare_user_document(vfs, &document, error, error_size)) {
		return 0;
	}
	if (!AeronConfigFile_SetString(document, "paths.game_data", game_data_path, &config_error)) {
		AeronConfigFile_Destroy(document);
		return host_config_error(error, error_size, "could not update user configuration: %s",
								 config_error.message);
	}
	return host_config_save_user_document(vfs, document, error, error_size);
}

static int host_config_set_controller_axis(AeronConfigFile* document, const char* profile_name,
										   const char* name, const XwaControllerAxisBinding* binding,
										   int gamepad, AeronConfigError* error) {
	char path[128];
	const char* source_name;

	if (binding->source < 0) {
		source_name = "none";
	} else if (gamepad) {
		source_name = Aeron_GamepadAxisName((AeronGamepadAxis)binding->source);
		if (!source_name) {
			return 0;
		}
	}
	snprintf(path, sizeof path, "input.controller.%s.axes.%s.source", profile_name, name);
	if (gamepad || binding->source < 0) {
		if (!AeronConfigFile_SetString(document, path, source_name, error)) {
			return 0;
		}
	} else if (!AeronConfigFile_SetInt(document, path, binding->source, error)) {
		return 0;
	}
	snprintf(path, sizeof path, "input.controller.%s.axes.%s.invert", profile_name, name);
	if (!AeronConfigFile_SetBool(document, path, binding->invert, error)) {
		return 0;
	}
	snprintf(path, sizeof path, "input.controller.%s.axes.%s.deadzone", profile_name, name);
	return AeronConfigFile_SetFloat(document, path, binding->deadzone, error);
}

static int host_config_set_controller_button(AeronConfigFile* document, const char* profile_name,
											 const char* key, const AeronControllerDigitalSource* binding,
											 int gamepad, AeronConfigError* error) {
	char path[128];
	const char* source_name;

	snprintf(path, sizeof path, "input.controller.%s.buttons.%s", profile_name, key);

	if (binding->kind == AERON_CONTROLLER_DIGITAL_NONE) {
		return AeronConfigFile_SetString(document, path, "none", error);
	}
	if (binding->kind == AERON_CONTROLLER_DIGITAL_BUTTON) {
		if (gamepad) {
			source_name = Aeron_GamepadButtonName((AeronGamepadButton)binding->index);
			if (!source_name) {
				return 0;
			}
			return AeronConfigFile_SetString(document, path, source_name, error);
		} else {
			return AeronConfigFile_SetInt(document, path, binding->index, error);
		}
	}
	if (binding->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE ||
		binding->kind == AERON_CONTROLLER_DIGITAL_AXIS_NEGATIVE) {
		AeronConfigValue axis;
		AeronConfigValue direction = { .type = AERON_CONFIG_STRING };
		AeronConfigValue threshold = { .type = AERON_CONFIG_FLOAT };
		AeronConfigMapValue entries[3];
		AeronConfigValue value = { .type = AERON_CONFIG_MAP };

		if (gamepad) {
			source_name = Aeron_GamepadAxisName((AeronGamepadAxis)binding->index);
			if (!source_name) {
				return 0;
			}
			axis.type = AERON_CONFIG_STRING;
			axis.value.string_value = source_name;
		} else {
			axis.type = AERON_CONFIG_INT;
			axis.value.int_value = binding->index;
		}
		direction.value.string_value =
			binding->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE ? "positive" : "negative";
		threshold.value.float_value = binding->threshold;
		entries[0] = (AeronConfigMapValue) { "axis", &axis };
		entries[1] = (AeronConfigMapValue) { "direction", &direction };
		entries[2] = (AeronConfigMapValue) { "threshold", &threshold };
		value.value.map.entries = entries;
		value.value.map.count = 3;
		return AeronConfigFile_SetValue(document, path, &value, error);
	}
	return 0;
}

static int host_config_set_controller_actions(AeronConfigFile* document, const char* profile_name,
											  const uint16_t actions[XWA_CONTROLLER_ACTION_COUNT],
											  AeronConfigError* error) {
	AeronConfigValue items[XWA_CONTROLLER_ACTION_COUNT];
	AeronConfigValue sequence = { .type = AERON_CONFIG_SEQUENCE };
	char path[96];
	int i;

	for (i = 0; i < XWA_CONTROLLER_ACTION_COUNT; ++i) {
		items[i].type = AERON_CONFIG_INT;
		items[i].value.int_value = actions[i];
	}
	sequence.value.sequence.values = items;
	sequence.value.sequence.count = XWA_CONTROLLER_ACTION_COUNT;
	snprintf(path, sizeof path, "input.controller.%s.actions", profile_name);
	return AeronConfigFile_SetValue(document, path, &sequence, error);
}

static int host_config_set_controller_profile(AeronConfigFile* document, const char* profile_name,
											  const XwaControllerProfile* profile, int gamepad,
											  AeronConfigError* error) {
	static const char* const axis_names[XWA_CONTROLLER_LOGICAL_AXIS_COUNT] = { "yaw", "pitch", "throttle",
																			   "roll" };
	char path[96];
	int i;

	for (i = 0; i < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++i) {
		if (!host_config_set_controller_axis(document, profile_name, axis_names[i], &profile->axes[i],
											 gamepad, error)) {
			return 0;
		}
	}
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++i) {
		char key[8];

		snprintf(key, sizeof(key), "%d", i + 1);
		if (!host_config_set_controller_button(document, profile_name, key, &profile->buttons[i], gamepad,
											   error)) {
			return 0;
		}
	}
	if (gamepad) {
		snprintf(path, sizeof path, "input.controller.%s.pov", profile_name);
		if (!AeronConfigFile_SetString(document, path, profile->pov_source ? "dpad" : "none", error)) {
			return 0;
		}
	} else {
		snprintf(path, sizeof path, "input.controller.%s.pov_hat", profile_name);
		if (profile->pov_source >= 0) {
			if (!AeronConfigFile_SetInt(document, path, profile->pov_source, error)) {
				return 0;
			}
		} else if (!AeronConfigFile_SetString(document, path, "none", error)) {
			return 0;
		}
	}
	return host_config_set_controller_actions(document, profile_name, profile->actions, error);
}

static int host_config_set_input_options(AeronConfigFile* document, const XwaModernInputOptions* options,
										 AeronConfigError* error) {
	static const char* const mode_names[] = { "position", "rate" };

	if (!XwaModernInputOptions_Validate(options)) {
		return 0;
	}
	return AeronConfigFile_SetBool(document, "input.mouse_flight", options->mouse_flight_enabled, error) &&
		   AeronConfigFile_SetString(document, "input.mouse_mode", mode_names[options->mouse_mode], error) &&
		   AeronConfigFile_SetInt(document, "input.mouse_sensitivity", options->mouse_sensitivity, error) &&
		   AeronConfigFile_SetBool(document, "input.mouse_invert_y", options->mouse_invert_y, error) &&
		   AeronConfigFile_SetString(document, "input.controller.device.guid",
									 options->controller.device.guid, error) &&
		   AeronConfigFile_SetString(document, "input.controller.device.path",
									 options->controller.device.path, error) &&
		   AeronConfigFile_SetInt(document, "input.controller.device.ordinal",
								  options->controller.device.ordinal, error) &&
		   AeronConfigFile_SetBool(document, "input.controller.roll_enabled",
								   options->controller.roll_enabled, error) &&
		   AeronConfigFile_SetBool(document, "input.controller.rumble_enabled",
								   options->controller.rumble_enabled, error) &&
		   AeronConfigFile_SetInt(document, "input.controller.rumble_strength",
								  options->controller.rumble_strength, error) &&
		   host_config_set_controller_profile(document, "gamepad", &options->controller.gamepad, 1, error) &&
		   host_config_set_controller_profile(document, "joystick", &options->controller.joystick, 0, error);
}

int XwaHostConfig_SaveInputOptions(AeronVfs* vfs, const XwaModernInputOptions* options, char* error,
								   size_t error_size) {
	AeronConfigFile* document = NULL;
	AeronConfigError config_error = { 0 };

	if (!vfs || !options) {
		return host_config_error(error, error_size, "cannot save invalid input settings to %s",
								 "config.yaml");
	}
	if (!host_config_prepare_user_document(vfs, &document, error, error_size)) {
		return 0;
	}
	if (!host_config_set_input_options(document, options, &config_error)) {
		AeronConfigFile_Destroy(document);
		return host_config_error(error, error_size, "could not update user configuration: %s",
								 config_error.message[0] ? config_error.message : "invalid input settings");
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
