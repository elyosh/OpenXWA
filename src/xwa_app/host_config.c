#include "host_config.h"

#include "aeron/config_file.h"
#include "aeron/log.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

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

static int host_config_gamepad_button_source(const AeronConfigFile* config, const char* key, int required,
											 int* out, char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	const char* value;
	AeronGamepadButton button;

	if (!node) {
		return host_config_input_missing(required, key, error, error_size);
	}
	value = AeronConfigNode_String(node, NULL);
	if (value && strcmp(value, "none") == 0) {
		*out = -1;
		return 1;
	}
	button = Aeron_GamepadButtonFromName(value);
	if (button >= AERON_GAMEPAD_BUTTON_COUNT) {
		return host_config_error(error, error_size, "invalid input setting '%s'", key);
	}
	*out = (int)button;
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
		if (!host_config_gamepad_button_source(config, key, required, &controller->gamepad.buttons[i], error,
											   error_size)) {
			return 0;
		}
		snprintf(key, sizeof(key), "input.controller.joystick.buttons.%d", i + 1);
		if (!host_config_raw_source(config, key, required, AERON_CONTROLLER_BUTTON_MAX,
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

static int host_yaml_scalar_equals(const yaml_node_t* node, const char* value) {
	const size_t length = strlen(value);
	return node && node->type == YAML_SCALAR_NODE && node->data.scalar.length == length &&
		   memcmp(node->data.scalar.value, value, length) == 0;
}

static int host_yaml_mapping_value(const yaml_document_t* document, int mapping_id, const char* key,
								   size_t* pair_index) {
	yaml_node_t* mapping = yaml_document_get_node((yaml_document_t*)document, mapping_id);
	yaml_node_pair_t* pair;

	if (!mapping || mapping->type != YAML_MAPPING_NODE) {
		return 0;
	}
	for (pair = mapping->data.mapping.pairs.start; pair < mapping->data.mapping.pairs.top; ++pair) {
		if (host_yaml_scalar_equals(yaml_document_get_node((yaml_document_t*)document, pair->key), key)) {
			if (pair_index) {
				*pair_index = (size_t)(pair - mapping->data.mapping.pairs.start);
			}
			return pair->value;
		}
	}
	return 0;
}

static int host_yaml_add_scalar(yaml_document_t* document, const char* value, yaml_scalar_style_t style) {
	return yaml_document_add_scalar(document, (yaml_char_t*)YAML_STR_TAG, (const yaml_char_t*)value,
									(int)strlen(value), style);
}

static int host_yaml_get_or_add_mapping(yaml_document_t* document, int parent_id, const char* key) {
	int mapping_id;

	mapping_id = host_yaml_mapping_value(document, parent_id, key, NULL);
	if (mapping_id) {
		yaml_node_t* mapping = yaml_document_get_node(document, mapping_id);
		return mapping && mapping->type == YAML_MAPPING_NODE ? mapping_id : 0;
	}
	mapping_id = yaml_document_add_mapping(document, (yaml_char_t*)YAML_MAP_TAG, YAML_BLOCK_MAPPING_STYLE);
	if (mapping_id) {
		const int key_id = host_yaml_add_scalar(document, key, YAML_PLAIN_SCALAR_STYLE);
		if (key_id && yaml_document_append_mapping_pair(document, parent_id, key_id, mapping_id)) {
			return mapping_id;
		}
	}
	return 0;
}

static int host_yaml_set_scalar(yaml_document_t* document, int mapping_id, const char* key, const char* value,
								yaml_scalar_style_t style) {
	size_t pair_index = 0;
	const int existing_value_id = host_yaml_mapping_value(document, mapping_id, key, &pair_index);
	const int value_id = host_yaml_add_scalar(document, value, style);

	if (!value_id) {
		return 0;
	}
	if (existing_value_id) {
		yaml_node_t* mapping = yaml_document_get_node(document, mapping_id);
		mapping->data.mapping.pairs.start[pair_index].value = value_id;
		return 1;
	}
	{
		const int key_id = host_yaml_add_scalar(document, key, YAML_PLAIN_SCALAR_STYLE);
		return key_id && yaml_document_append_mapping_pair(document, mapping_id, key_id, value_id);
	}
}

static int host_yaml_set_game_data(yaml_document_t* document, const char* path) {
	yaml_node_t* root = yaml_document_get_root_node(document);
	int paths_id;

	if (!root || root->type != YAML_MAPPING_NODE) {
		return 0;
	}
	paths_id = host_yaml_get_or_add_mapping(document, 1, "paths");
	return paths_id &&
		   host_yaml_set_scalar(document, paths_id, "game_data", path, YAML_SINGLE_QUOTED_SCALAR_STYLE);
}

static int host_yaml_set_video_options(yaml_document_t* document, const XwaModernVideoOptions* options) {
	static const char* const window_mode_names[] = { "windowed", "fullscreen" };
	static const char* const quality_names[] = { "off", "low", "high" };
	static const char* const fsr_names[] = { "off", "performance", "balanced", "quality", "native_aa" };
	static const char* const msaa_names[] = { "off", "2x", "4x", "8x" };
	static const char* const sdr_gamma_names[] = { "2.2", "2.4", "srgb" };
	static const char* const paper_white_names[] = { "auto", "100", "150", "200", "250", "300", "400" };
	yaml_node_t* root = yaml_document_get_root_node(document);
	int video_id;

	if (!root || root->type != YAML_MAPPING_NODE || !options ||
		options->window_mode < XWA_MODERN_WINDOW_MODE_WINDOWED ||
		options->window_mode > XWA_MODERN_WINDOW_MODE_FULLSCREEN ||
		options->ssao_quality < XWA_MODERN_SSAO_OFF || options->ssao_quality > XWA_MODERN_SSAO_HIGH ||
		options->fsr_upscaling < XWA_MODERN_FSR_OFF || options->fsr_upscaling > XWA_MODERN_FSR_NATIVE_AA ||
		options->msaa < XWA_MODERN_MSAA_OFF || options->msaa > XWA_MODERN_MSAA_8X ||
		(options->fsr_upscaling != XWA_MODERN_FSR_OFF && options->msaa != XWA_MODERN_MSAA_OFF) ||
		options->motion_blur_quality < XWA_MODERN_MOTION_BLUR_OFF ||
		options->motion_blur_quality > XWA_MODERN_MOTION_BLUR_HIGH ||
		options->sdr_gamma < XWA_MODERN_SDR_GAMMA_2_2 || options->sdr_gamma > XWA_MODERN_SDR_GAMMA_SRGB ||
		options->paper_white < XWA_MODERN_PAPER_WHITE_AUTO ||
		options->paper_white > XWA_MODERN_PAPER_WHITE_400) {
		return 0;
	}
	video_id = host_yaml_get_or_add_mapping(document, 1, "video");
	return video_id &&
		   host_yaml_set_scalar(document, video_id, "window_mode", window_mode_names[options->window_mode],
								YAML_SINGLE_QUOTED_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, video_id, "ssao_quality", quality_names[options->ssao_quality],
								YAML_SINGLE_QUOTED_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, video_id, "fsr_upscaling", fsr_names[options->fsr_upscaling],
								YAML_SINGLE_QUOTED_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, video_id, "msaa", msaa_names[options->msaa],
								YAML_SINGLE_QUOTED_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, video_id, "motion_blur_quality",
								quality_names[options->motion_blur_quality],
								YAML_SINGLE_QUOTED_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, video_id, "hdr_output", options->hdr_output ? "true" : "false",
								YAML_PLAIN_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, video_id, "sdr_content_gamma", sdr_gamma_names[options->sdr_gamma],
								YAML_SINGLE_QUOTED_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, video_id, "paper_white_nits",
								paper_white_names[options->paper_white], YAML_SINGLE_QUOTED_SCALAR_STYLE);
}

static int host_yaml_create_document(yaml_document_t* document) {
	int root_id;
	int version_key_id;
	int version_value_id;

	if (!yaml_document_initialize(document, NULL, NULL, NULL, 1, 1)) {
		return 0;
	}
	root_id = yaml_document_add_mapping(document, (yaml_char_t*)YAML_MAP_TAG, YAML_BLOCK_MAPPING_STYLE);
	version_key_id = host_yaml_add_scalar(document, "version", YAML_PLAIN_SCALAR_STYLE);
	version_value_id = host_yaml_add_scalar(document, "1", YAML_PLAIN_SCALAR_STYLE);
	if (!root_id || !version_key_id || !version_value_id ||
		!yaml_document_append_mapping_pair(document, root_id, version_key_id, version_value_id)) {
		yaml_document_delete(document);
		return 0;
	}
	return 1;
}

static int host_yaml_load_document(AeronVfs* vfs, yaml_document_t* document) {
	uint8_t* data = NULL;
	size_t data_size = 0;
	yaml_parser_t parser;
	yaml_document_t extra_document;
	int document_loaded = 0;
	int loaded = 0;

	memset(document, 0, sizeof *document);
	memset(&extra_document, 0, sizeof extra_document);
	if (!AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_USER, "config.yaml", 1024 * 1024, &data, &data_size) ||
		!yaml_parser_initialize(&parser)) {
		free(data);
		return 0;
	}
	yaml_parser_set_input_string(&parser, data, data_size);
	if (yaml_parser_load(&parser, document)) {
		document_loaded = 1;
	}
	if (document_loaded && yaml_parser_load(&parser, &extra_document)) {
		yaml_node_t* root = yaml_document_get_root_node(document);
		if (root && root->type == YAML_MAPPING_NODE && !yaml_document_get_root_node(&extra_document)) {
			loaded = 1;
		}
	}
	if (document_loaded && !loaded) {
		yaml_document_delete(document);
	}
	yaml_document_delete(&extra_document);
	yaml_parser_delete(&parser);
	free(data);
	return loaded;
}

static int host_yaml_write(void* data, unsigned char* buffer, size_t size) {
	AeronFile* file = (AeronFile*)data;
	return AeronVfs_Write(file, buffer, size, NULL);
}

static int host_yaml_save_document(AeronVfs* vfs, yaml_document_t* document, char* error, size_t error_size) {
	static const char* path = "config.yaml";
	static const char* temporary_path = "config.yaml.tmp";
	yaml_emitter_t emitter;
	AeronFile* file = NULL;
	int emitter_initialized = 0;
	int document_owned = 1;
	int ok = 0;

	if (!AeronVfs_Open(vfs, AERON_VFS_ROOT_USER, temporary_path, AERON_VFS_WRITE, &file) ||
		!yaml_emitter_initialize(&emitter)) {
		goto cleanup;
	}
	emitter_initialized = 1;
	yaml_emitter_set_output(&emitter, host_yaml_write, file);
	yaml_emitter_set_indent(&emitter, 2);
	yaml_emitter_set_unicode(&emitter, 1);
	if (!yaml_emitter_open(&emitter)) {
		goto cleanup;
	}
	document_owned = 0;
	if (!yaml_emitter_dump(&emitter, document) || !yaml_emitter_close(&emitter) || !AeronVfs_Flush(file)) {
		goto cleanup;
	}
	{
		const int close_ok = AeronVfs_Close(file);
		file = NULL;
		if (!close_ok) {
			goto cleanup;
		}
	}
	if (!AeronVfs_Rename(vfs, AERON_VFS_ROOT_USER, temporary_path, path)) {
		goto cleanup;
	}
	ok = 1;

cleanup:
	if (file) {
		AeronVfs_Close(file);
	}
	if (emitter_initialized) {
		yaml_emitter_delete(&emitter);
	}
	if (document_owned) {
		yaml_document_delete(document);
	}
	if (!ok) {
		AeronVfs_Remove(vfs, AERON_VFS_ROOT_USER, temporary_path);
		host_config_error(error, error_size, "could not save user configuration: %s", path);
	}
	return ok;
}

static int host_yaml_prepare_user_document(AeronVfs* vfs, yaml_document_t* document, char* error,
										   size_t error_size) {
	static const char* path = "config.yaml";

	if (AeronVfs_Exists(vfs, AERON_VFS_ROOT_USER, path)) {
		if (host_yaml_load_document(vfs, document)) {
			return 1;
		}
		return host_config_error(error, error_size, "could not update user configuration: %s", path);
	}
	if (host_yaml_create_document(document)) {
		return 1;
	}
	return host_config_error(error, error_size, "could not create user configuration: %s", path);
}

int XwaHostConfig_SaveGameDataPath(AeronVfs* vfs, const char* game_data_path, char* error,
								   size_t error_size) {
	yaml_document_t document;

	if (!vfs || !game_data_path || !game_data_path[0] ||
		strlen(game_data_path) >= XWA_HOST_CONFIG_PATH_CAPACITY) {
		return host_config_error(error, error_size, "cannot save invalid path to %s", "config.yaml");
	}
	if (!host_yaml_prepare_user_document(vfs, &document, error, error_size)) {
		return 0;
	}
	if (!host_yaml_set_game_data(&document, game_data_path)) {
		yaml_document_delete(&document);
		return host_config_error(error, error_size, "could not update user configuration: %s", "config.yaml");
	}
	return host_yaml_save_document(vfs, &document, error, error_size);
}

static int host_yaml_set_sequence_node(yaml_document_t* document, int mapping_id, const char* key,
									   int sequence_id) {
	size_t pair_index = 0;
	const int existing_value_id = host_yaml_mapping_value(document, mapping_id, key, &pair_index);

	if (!sequence_id) {
		return 0;
	}
	if (existing_value_id) {
		yaml_node_t* mapping = yaml_document_get_node(document, mapping_id);
		mapping->data.mapping.pairs.start[pair_index].value = sequence_id;
		return 1;
	}
	{
		const int key_id = host_yaml_add_scalar(document, key, YAML_PLAIN_SCALAR_STYLE);
		return key_id && yaml_document_append_mapping_pair(document, mapping_id, key_id, sequence_id);
	}
}

static int host_yaml_set_controller_axis(yaml_document_t* document, int axes_id, const char* name,
										 const XwaControllerAxisBinding* binding, int gamepad) {
	int axis_id = host_yaml_get_or_add_mapping(document, axes_id, name);
	char source_text[16];
	char deadzone_text[32];
	const char* source_name;

	if (!axis_id) {
		return 0;
	}
	if (binding->source < 0) {
		source_name = "none";
	} else if (gamepad) {
		source_name = Aeron_GamepadAxisName((AeronGamepadAxis)binding->source);
		if (!source_name) {
			return 0;
		}
	} else {
		snprintf(source_text, sizeof(source_text), "%d", binding->source);
		source_name = source_text;
	}
	snprintf(deadzone_text, sizeof(deadzone_text), "%.6g", (double)binding->deadzone);
	return host_yaml_set_scalar(document, axis_id, "source", source_name,
								gamepad || binding->source < 0 ? YAML_SINGLE_QUOTED_SCALAR_STYLE
															   : YAML_PLAIN_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, axis_id, "invert", binding->invert ? "true" : "false",
								YAML_PLAIN_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, axis_id, "deadzone", deadzone_text, YAML_PLAIN_SCALAR_STYLE);
}

static int host_yaml_set_controller_actions(yaml_document_t* document, int profile_id,
											const uint16_t actions[XWA_CONTROLLER_ACTION_COUNT]);

static int host_yaml_set_controller_profile(yaml_document_t* document, int controller_id,
											const char* profile_name, const XwaControllerProfile* profile,
											int gamepad) {
	static const char* const axis_names[XWA_CONTROLLER_LOGICAL_AXIS_COUNT] = { "yaw", "pitch", "throttle",
																			   "roll" };
	int profile_id;
	int axes_id;
	int buttons_id;
	int pov_ok;
	int i;

	profile_id = host_yaml_get_or_add_mapping(document, controller_id, profile_name);
	axes_id = profile_id ? host_yaml_get_or_add_mapping(document, profile_id, "axes") : 0;
	buttons_id = profile_id ? host_yaml_get_or_add_mapping(document, profile_id, "buttons") : 0;
	if (!axes_id || !buttons_id) {
		return 0;
	}
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++i) {
		if (!host_yaml_set_controller_axis(document, axes_id, axis_names[i], &profile->axes[i], gamepad)) {
			return 0;
		}
	}
	for (i = 0; i < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++i) {
		char key[8];
		char value[16];
		const char* source_name;
		yaml_scalar_style_t style;

		snprintf(key, sizeof(key), "%d", i + 1);
		if (profile->buttons[i] < 0) {
			source_name = "none";
			style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
		} else if (gamepad) {
			source_name = Aeron_GamepadButtonName((AeronGamepadButton)profile->buttons[i]);
			style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
			if (!source_name) {
				return 0;
			}
		} else {
			snprintf(value, sizeof(value), "%d", profile->buttons[i]);
			source_name = value;
			style = YAML_PLAIN_SCALAR_STYLE;
		}
		if (!host_yaml_set_scalar(document, buttons_id, key, source_name, style)) {
			return 0;
		}
	}
	if (gamepad) {
		pov_ok = host_yaml_set_scalar(document, profile_id, "pov", profile->pov_source ? "dpad" : "none",
									  YAML_SINGLE_QUOTED_SCALAR_STYLE);
	} else {
		char value[16];
		const char* text = "none";
		yaml_scalar_style_t style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
		if (profile->pov_source >= 0) {
			snprintf(value, sizeof(value), "%d", profile->pov_source);
			text = value;
			style = YAML_PLAIN_SCALAR_STYLE;
		}
		pov_ok = host_yaml_set_scalar(document, profile_id, "pov_hat", text, style);
	}
	return pov_ok && host_yaml_set_controller_actions(document, profile_id, profile->actions);
}

static int host_yaml_set_controller_actions(yaml_document_t* document, int profile_id,
											const uint16_t actions[XWA_CONTROLLER_ACTION_COUNT]) {
	int sequence_id;
	int i;

	sequence_id = yaml_document_add_sequence(document, (yaml_char_t*)YAML_SEQ_TAG, YAML_BLOCK_SEQUENCE_STYLE);
	if (!sequence_id) {
		return 0;
	}
	for (i = 0; i < XWA_CONTROLLER_ACTION_COUNT; ++i) {
		char value[16];
		int value_id;

		snprintf(value, sizeof(value), "%u", (unsigned int)actions[i]);
		value_id = host_yaml_add_scalar(document, value, YAML_PLAIN_SCALAR_STYLE);
		if (!value_id || !yaml_document_append_sequence_item(document, sequence_id, value_id)) {
			return 0;
		}
	}
	return host_yaml_set_sequence_node(document, profile_id, "actions", sequence_id);
}

static int host_yaml_set_input_options(yaml_document_t* document, const XwaModernInputOptions* options) {
	static const char* const mode_names[] = { "position", "rate" };
	yaml_node_t* root = yaml_document_get_root_node(document);
	char sensitivity_text[16];
	char ordinal_text[16];
	char rumble_strength_text[16];
	int input_id;
	int controller_id;
	int device_id;

	if (!root || root->type != YAML_MAPPING_NODE || !XwaModernInputOptions_Validate(options)) {
		return 0;
	}
	snprintf(sensitivity_text, sizeof sensitivity_text, "%d", options->mouse_sensitivity);
	input_id = host_yaml_get_or_add_mapping(document, 1, "input");
	controller_id = input_id ? host_yaml_get_or_add_mapping(document, input_id, "controller") : 0;
	device_id = controller_id ? host_yaml_get_or_add_mapping(document, controller_id, "device") : 0;
	snprintf(ordinal_text, sizeof(ordinal_text), "%d", options->controller.device.ordinal);
	snprintf(rumble_strength_text, sizeof(rumble_strength_text), "%d", options->controller.rumble_strength);
	return input_id && controller_id && device_id &&
		   host_yaml_set_scalar(document, input_id, "mouse_flight",
								options->mouse_flight_enabled ? "true" : "false", YAML_PLAIN_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, input_id, "mouse_mode", mode_names[options->mouse_mode],
								YAML_SINGLE_QUOTED_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, input_id, "mouse_sensitivity", sensitivity_text,
								YAML_PLAIN_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, input_id, "mouse_invert_y",
								options->mouse_invert_y ? "true" : "false", YAML_PLAIN_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, device_id, "guid", options->controller.device.guid,
								YAML_SINGLE_QUOTED_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, device_id, "path", options->controller.device.path,
								YAML_SINGLE_QUOTED_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, device_id, "ordinal", ordinal_text, YAML_PLAIN_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, controller_id, "roll_enabled",
								options->controller.roll_enabled ? "true" : "false",
								YAML_PLAIN_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, controller_id, "rumble_enabled",
								options->controller.rumble_enabled ? "true" : "false",
								YAML_PLAIN_SCALAR_STYLE) &&
		   host_yaml_set_scalar(document, controller_id, "rumble_strength", rumble_strength_text,
								YAML_PLAIN_SCALAR_STYLE) &&
		   host_yaml_set_controller_profile(document, controller_id, "gamepad", &options->controller.gamepad,
											1) &&
		   host_yaml_set_controller_profile(document, controller_id, "joystick",
											&options->controller.joystick, 0);
}

int XwaHostConfig_SaveInputOptions(AeronVfs* vfs, const XwaModernInputOptions* options, char* error,
								   size_t error_size) {
	yaml_document_t document;

	if (!vfs || !options) {
		return host_config_error(error, error_size, "cannot save invalid input settings to %s",
								 "config.yaml");
	}
	if (!host_yaml_prepare_user_document(vfs, &document, error, error_size)) {
		return 0;
	}
	if (!host_yaml_set_input_options(&document, options)) {
		yaml_document_delete(&document);
		return host_config_error(error, error_size, "could not update user configuration: %s", "config.yaml");
	}
	return host_yaml_save_document(vfs, &document, error, error_size);
}

int XwaHostConfig_SaveVideoOptions(AeronVfs* vfs, const XwaModernVideoOptions* options, char* error,
								   size_t error_size) {
	yaml_document_t document;

	if (!vfs || !options) {
		return host_config_error(error, error_size, "cannot save invalid video settings to %s",
								 "config.yaml");
	}
	if (!host_yaml_prepare_user_document(vfs, &document, error, error_size)) {
		return 0;
	}
	if (!host_yaml_set_video_options(&document, options)) {
		yaml_document_delete(&document);
		return host_config_error(error, error_size, "could not update user configuration: %s", "config.yaml");
	}
	return host_yaml_save_document(vfs, &document, error, error_size);
}
