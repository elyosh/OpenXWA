#include "aeron/aeron.h"
#include "host_config.h"
#include "setup.h"
#include "window_icon.h"
#include "xwa_remaster/xwa_remaster.h"
#include "xwa_runtime/runtime/port.h"
#include "xwa_runtime/runtime/presentation.h"
#include "xwa_runtime/timing/modern_flight_timing.h"

#include <stdio.h>
#include <string.h>

enum {
	XWA_SETUP_CHOOSE_FOLDER = 1,
	XWA_SETUP_TRY_AGAIN = 2,
	XWA_SETUP_QUIT = 3,
};

static void apply_modern_video_options(const XwaModernVideoOptions* options) {
	if (!options) {
		return;
	}
	Aeron_SetFullscreen(options->window_mode == XWA_MODERN_WINDOW_MODE_FULLSCREEN);
	XwaRemaster_SetVideoOptions(options);
}

static int persist_modern_video_options(const XwaModernVideoOptions* options, char* error,
										size_t error_size) {
	return XwaHostConfig_SaveVideoOptions(Aeron_GetVfs(), options, error, error_size);
}

static int show_first_launch_prompt(int* cancelled, char* error, size_t error_size) {
	static const AeronMessageBoxButton buttons[] = {
		{ XWA_SETUP_CHOOSE_FOLDER, "Choose Game Folder", 1, 0 },
		{ XWA_SETUP_QUIT, "Quit", 0, 1 },
	};
	const AeronMessageBoxOptions options = {
		.kind = AERON_MESSAGE_BOX_INFORMATION,
		.title = "Set up OpenXWA",
		.message = "OpenXWA requires data files from an original X-Wing Alliance installation.\n\n"
				   "Select a folder containing the merged contents of CD1 and CD2:\n\n"
				   "    ALLIANCE\n"
				   "    FLIGHTMODELS\n"
				   "    MISSIONS\n"
				   "    MOVIES\n"
				   "    RESDATA\n"
				   "    WAVE\n\n"
				   "The selected location will be validated and remembered.",
		.buttons = buttons,
		.button_count = sizeof buttons / sizeof buttons[0],
	};
	int selected_button = -1;

	if (!Aeron_ShowMessageBox(&options, &selected_button)) {
		snprintf(error, error_size, "Could not show the first-launch setup prompt.");
		return 0;
	}
	if (selected_button != XWA_SETUP_CHOOSE_FOLDER) {
		*cancelled = 1;
		return 0;
	}
	return 1;
}

static int select_game_data(AeronVfs* vfs, char* selected, size_t selected_capacity, int* cancelled,
							char* error, size_t error_size) {
	const AeronFolderDialogOptions options = {
		.title = "Select your original X-Wing Alliance game directory",
		.accept_label = "Choose folder",
		.cancel_label = "Quit",
	};

	*cancelled = 0;
	for (;;) {
		AeronFolderDialog* dialog = Aeron_ShowOpenFolderDialog(&options);
		AeronFolderDialogStatus status;
		char candidate[XWA_HOST_CONFIG_PATH_CAPACITY];
		char dialog_error[256];
		char validation_error[512];

		if (!dialog) {
			snprintf(error, error_size, "Could not open the folder picker.");
			return 0;
		}
		do {
				Aeron_BeginFrame();
				status = Aeron_PollFolderDialog(dialog, candidate, sizeof candidate, dialog_error,
												sizeof dialog_error);
				if (!Aeron_Present()) {
					snprintf(error, error_size, "Renderer failure while displaying the folder picker: %s",
							 Aeron_RenderLastError());
					Aeron_DestroyFolderDialog(dialog);
					return 0;
				}
			if (status == AERON_FOLDER_DIALOG_WAITING) {
				Aeron_WaitForNextFrame(Aeron_NowUs() + 16667);
			}
		} while (status == AERON_FOLDER_DIALOG_WAITING);
		Aeron_DestroyFolderDialog(dialog);

		if (Aeron_QuitRequested()) {
			*cancelled = 1;
			return 0;
		}
		if (status == AERON_FOLDER_DIALOG_CANCELLED) {
			*cancelled = 1;
			return 0;
		}
		if (status == AERON_FOLDER_DIALOG_ERROR) {
			snprintf(error, error_size, "Folder picker failed: %s",
					 dialog_error[0] ? dialog_error : "unknown platform error");
			return 0;
		}
		if (XwaSetup_ValidateGameData(vfs, candidate, selected, selected_capacity, validation_error,
									  sizeof validation_error)) {
			return 1;
		}
		{
			static const AeronMessageBoxButton buttons[] = {
				{ XWA_SETUP_CHOOSE_FOLDER, "Choose Another Folder", 1, 0 },
				{ XWA_SETUP_QUIT, "Quit", 0, 1 },
			};
			char message[1536];
			int selected_button = -1;
			const AeronMessageBoxOptions message_options = {
				.kind = AERON_MESSAGE_BOX_WARNING,
				.title = "Invalid X-Wing Alliance game directory",
				.message = message,
				.buttons = buttons,
				.button_count = sizeof buttons / sizeof buttons[0],
			};

			snprintf(message, sizeof message,
					 "The selected folder is not a complete X-Wing Alliance data directory.\n\n"
					 "Selected:\n%s\n\n"
					 "%s\n\n"
					 "Select the folder containing ALLIANCE, FLIGHTMODELS, and MISSIONS.",
					 candidate, validation_error);
			if (!Aeron_ShowMessageBox(&message_options, &selected_button)) {
				snprintf(error, error_size, "Could not show the game-data validation result.");
				return 0;
			}
			if (selected_button != XWA_SETUP_CHOOSE_FOLDER) {
				*cancelled = 1;
				return 0;
			}
		}
	}
}

static int resolve_game_data(const XwaLaunchOptions* launch, const XwaHostConfig* host_config, char* selected,
							 size_t selected_capacity, int* cancelled, char* error, size_t error_size) {
	AeronVfs* vfs = Aeron_GetVfs();

	*cancelled = 0;
	if (launch->game_data_path[0]) {
		return XwaSetup_ValidateGameData(vfs, launch->game_data_path, selected, selected_capacity, error,
										 error_size);
	}
	if (host_config->game_data_path[0]) {
		if (XwaSetup_ValidateGameData(vfs, host_config->game_data_path, selected, selected_capacity, error,
									  error_size)) {
			return 1;
		}
		Aeron_LogWarn("xwa.config", "saved game-data path is no longer valid: %s", error);
		for (;;) {
			static const AeronMessageBoxButton buttons[] = {
				{ XWA_SETUP_TRY_AGAIN, "Try Again", 1, 0 },
				{ XWA_SETUP_CHOOSE_FOLDER, "Choose Another Folder", 0, 0 },
				{ XWA_SETUP_QUIT, "Quit", 0, 1 },
			};
			char message[1536];
			int selected_button = -1;
			const AeronMessageBoxOptions options = {
				.kind = AERON_MESSAGE_BOX_WARNING,
				.title = "Original X-Wing Alliance game data not found",
				.message = message,
				.buttons = buttons,
				.button_count = sizeof buttons / sizeof buttons[0],
			};

			snprintf(message, sizeof message,
					 "The previously configured game-data folder is no longer available or "
					 "does not contain the required files.\n\n"
					 "Configured folder:\n%s\n\n"
					 "%s\n\n"
					 "The folder may have moved, or its drive may be disconnected.",
					 host_config->game_data_path, error);
			if (!Aeron_ShowMessageBox(&options, &selected_button)) {
				snprintf(error, error_size, "Could not show the saved game-data error.");
				return 0;
			}
			if (selected_button == XWA_SETUP_TRY_AGAIN) {
				if (XwaSetup_ValidateGameData(vfs, host_config->game_data_path, selected, selected_capacity,
											  error, error_size)) {
					return 1;
				}
				continue;
			}
			if (selected_button == XWA_SETUP_CHOOSE_FOLDER) {
				return select_game_data(vfs, selected, selected_capacity, cancelled, error, error_size);
			}
			*cancelled = 1;
			return 0;
		}
	}
	if (!show_first_launch_prompt(cancelled, error, error_size)) {
		return 0;
	}
	return select_game_data(vfs, selected, selected_capacity, cancelled, error, error_size);
}

int main(int argc, char** argv) {
	AeronConfig config;
	XwaHostConfig host_config;
	XwaLaunchOptions launch;
	char selected_game_data[XWA_HOST_CONFIG_PATH_CAPACITY];
	char config_error[1024];
	XwaModernWindowMode initial_window_mode;
	int exit_code;
	int setup_cancelled;

	if (!XwaLaunchOptions_Parse(argc, argv, &launch, config_error, sizeof config_error)) {
		Aeron_LogError("xwa.config", "%s", config_error);
		return 2;
	}
	memset(&config, 0, sizeof(config));
	config.org_name = "TotallyOpen";
	config.app_name = "OpenXWA";
	config.resource_root = launch.resource_root[0] ? launch.resource_root : NULL;
	config.resource_path = "resources";
	config.shader_path = "shaders";
	config.window_title = "OpenXWA";
	config.window_icon_bmp = xwa_window_icon_bmp;
	config.window_icon_bmp_size = sizeof(xwa_window_icon_bmp);
	config.window_width = 1920;
	config.window_height = 1080;
	config.logical_width = XWA_PRESENTATION_WIDTH;
	config.logical_height = XWA_PRESENTATION_HEIGHT;
	config.presentation_mode = AERON_PRESENTATION_ASPECT_FIT;
	config.clear_color_enabled = 1;
	config.clear_color_rgba[0] = 0.0f;
	config.clear_color_rgba[1] = 0.0f;
	config.clear_color_rgba[2] = 0.0f;
	config.clear_color_rgba[3] = 1.0f;

	if (!Aeron_Init(&config)) {
		return 1;
	}
	if (!XwaHostConfig_Load(Aeron_GetVfs(), &host_config, config_error, sizeof config_error)) {
		char message[1536];
		snprintf(message, sizeof message, "%s\nConfiguration path: %s/config.yaml", config_error,
				 Aeron_UserPath());
		Aeron_LogError("xwa.config", "%s", message);
		Aeron_FatalError("OpenXWA", message);
		Aeron_Shutdown();
		return 1;
	}
	initial_window_mode =
		Aeron_Fullscreen() ? XWA_MODERN_WINDOW_MODE_FULLSCREEN : XWA_MODERN_WINDOW_MODE_WINDOWED;
	if (host_config.video_options_override_mask & XWA_MODERN_VIDEO_OVERRIDE_WINDOW_MODE) {
		initial_window_mode = host_config.video_options.window_mode;
	}
	if (!resolve_game_data(&launch, &host_config, selected_game_data, sizeof selected_game_data,
						   &setup_cancelled, config_error, sizeof config_error)) {
		if (!setup_cancelled) {
			Aeron_LogError("xwa.config", "%s", config_error);
			Aeron_FatalError("OpenXWA", config_error);
		}
		Aeron_Shutdown();
		return setup_cancelled ? 0 : 1;
	}
	if (strcmp(host_config.game_data_path, selected_game_data) != 0 &&
		!XwaHostConfig_SaveGameDataPath(Aeron_GetVfs(), selected_game_data, config_error,
										sizeof config_error)) {
		Aeron_LogError("xwa.config", "%s", config_error);
		Aeron_FatalError("OpenXWA", config_error);
		Aeron_Shutdown();
		return 1;
	}
	Aeron_SetFullscreen(initial_window_mode == XWA_MODERN_WINDOW_MODE_FULLSCREEN);
	Aeron_LogInfo("xwa.config", "game data: %s", selected_game_data);
	Aeron_LogInfo("xwa.config", "resources: %s", Aeron_ResourceRoot());
	Aeron_LogInfo("xwa.config", "flight simulation step: %d ticks", host_config.flight_simulation_step_ticks);
	Aeron_LogInfo("xwa.config", "OPT smoothing angle: %.3g degrees",
			  (double)host_config.model_smooth_angle_degrees);
	Aeron_LogInfo("xwa.config", "OPT emissive strength: %.3g", (double)host_config.model_opt_emissive_strength);
	Aeron_LogInfo("xwa.config", "OPT projectile emissive strength: %.3g",
			  (double)host_config.model_opt_projectile_emissive_strength);
	Aeron_LogInfo("xwa.config", "force OPT models: %s", host_config.force_opt_models ? "yes" : "no");
	Aeron_LogInfo("xwa.config", "prefer original 2D assets: %s", host_config.prefer_original_2d ? "yes" : "no");
	const XwaRemasterInitOptions remaster_options = {
		.opt_smooth_angle_degrees = host_config.model_smooth_angle_degrees,
		.opt_emissive_strength = host_config.model_opt_emissive_strength,
		.opt_projectile_emissive_strength = host_config.model_opt_projectile_emissive_strength,
		.force_opt_models = host_config.force_opt_models,
		.prefer_original_2d = host_config.prefer_original_2d,
		.video_options = host_config.video_options,
		.video_options_override_mask = host_config.video_options_override_mask,
	};
	if (!XwaRemaster_Init(&remaster_options)) {
		Aeron_FatalError("OpenXWA",
						 "Required remaster resources could not be loaded. See the log for details.");
		XwaRemaster_Shutdown();
		Aeron_Shutdown();
		return 1;
	}
	{
		XwaModernVideoOptions effective_video_options;
		XwaRemaster_GetVideoOptions(&effective_video_options);
		effective_video_options.window_mode = initial_window_mode;
		XwaModernVideoOptions_Configure(&effective_video_options, apply_modern_video_options,
										persist_modern_video_options);
	}

	XwaPort_SetCommandLine(launch.game_command_line);
	XwaModernFlightTiming_Configure(host_config.flight_simulation_step_ticks);
	if (!XwaPort_Init()) {
		XwaRemaster_Shutdown();
		Aeron_Shutdown();
		return 1;
	}

	exit_code = 0;
	/* Cmd+P host pause (the TIE shell model): the game tick is skipped
	 * — sim and snapshots freeze — while the HD remaster keeps
	 * rendering the frozen snapshot every frame (debug UI and
	 * motion-blur inspection stay live). The toggle frame also
	 * skips the tick, so the P press edge never reaches the classic
	 * input path (edges pump inside the tick). */
	{
		int paused = 0;
		while (!Aeron_QuitRequested() && !XwaPort_ShouldQuit()) {
			const int32_t delta_us = Aeron_BeginFrame();
			const AeronInputSnapshot* in = Aeron_InputSnapshot();
			int toggled = 0;
			const int gui_mod = in && (in->key_down[AERON_KEY_LGUI] || in->key_down[AERON_KEY_RGUI]);
			if (gui_mod && in->key_pressed[AERON_KEY_A + ('p' - 'a')]) {
				paused = !paused;
				toggled = 1;
				Aeron_AudioSetPaused(paused);
				Aeron_LogInfo("xwa", "%s", paused ? "paused" : "resumed");
			}
			/* Ctrl+Alt+L is a portable alias for the original Scroll Lock cockpit
			 * mouse-look action. Queue the original action so its gameplay gates remain
			 * authoritative, and skip this tick so the chord cannot leak into flight. */
			if (in && (in->key_down[AERON_KEY_LCTRL] || in->key_down[AERON_KEY_RCTRL]) &&
				(in->key_down[AERON_KEY_LALT] || in->key_down[AERON_KEY_RALT]) &&
				in->key_pressed[AERON_KEY_A + ('l' - 'a')] && XwaPort_QueueMouseLookToggle()) {
				toggled = 1;
			}
			/* Ctrl+Alt+M releases the flight mouse capture back to the OS and
			 * re-captures it. Handled here, and skipping the tick like the pause
			 * toggle, so the M edge never reaches the classic input path. */
			if (in && (in->key_down[AERON_KEY_LCTRL] || in->key_down[AERON_KEY_RCTRL]) &&
				(in->key_down[AERON_KEY_LALT] || in->key_down[AERON_KEY_RALT]) &&
				in->key_pressed[AERON_KEY_A + ('m' - 'a')]) {
				XwaPort_ToggleMouseCapture();
				toggled = 1;
			}
			XwaRemaster_BeginFrame(in);
			if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
				break;
			}
			if (paused || toggled) {
				XwaPort_PausedFrame();
			} else {
				XwaPort_Tick(delta_us);
			}
			if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
				break;
			}
			XwaRemaster_Frame(delta_us);
			if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
				break;
			}
			if (!Aeron_Present()) {
				Aeron_RequestFatalRendererError("frame presentation");
				break;
			}
			Aeron_WaitForNextFrame(XwaPort_NextWakeDeadlineUs());
		}
	}

	exit_code = Aeron_FatalErrorRequested() ? 1 : XwaPort_GetExitCode();
	XwaRemaster_Shutdown();
	XwaPort_Shutdown();
	Aeron_Shutdown();

	return exit_code;
}
