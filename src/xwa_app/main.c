#include "aeron/main.h"
#include "aeron/aeron.h"
#include "aeron/debug.h"
#include "aeron/time.h"
#include "host_config.h"
#include "install_check.h"
#include "install_paths.h"
#include "mod_list.h"
#include "options/options_window.h"
#include "setup.h"
#include "ui.h"
#include "window_icon.h"
#include "xwa/config/game_config.h"
#include "xwa_remaster/dynamic_cockpit.h"
#include "xwa_remaster/xwa_remaster.h"
#include "xwa_runtime/config/modern_mods.h"
#include "xwa_runtime/config/option_section.h"
#include "xwa_runtime/input/capture.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/input/keyboard_mapping.h"
#include "xwa_runtime/input/mouse_flight.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/runtime/film_benchmark.h"
#include "xwa_runtime/runtime/film_state.h"
#include "xwa_runtime/runtime/movie_task.h"
#include "xwa_runtime/runtime/port.h"
#include "xwa_runtime/runtime/presentation.h"
#include "xwa_runtime/timing/modern_flight_timing.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	XWA_SETUP_CHOOSE_FOLDER = 1,
	XWA_SETUP_TRY_AGAIN = 2,
	XWA_SETUP_QUIT = 3,
};

static uint64_t presentation_interval_us(void) {
	double rate = Aeron_PresentationRate();
	if (!(rate >= 1.0 && rate <= 1000.0))
		rate = 60.0;
	return (uint64_t)(1000000.0 / rate + 0.5);
}

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

/* Whether a host shortcut was pressed this frame. Its key stays suppressed
 * until release, so the game never sees it. */
static int host_shortcut_pressed(const AeronInputSnapshot* in, XwaKeyboardShortcut shortcut) {
	/* The options window gets the keys while it captures a binding. */
	const int key = XwaOptionsWindow_CapturesKeyboard() ? -1 : XwaKeyboardMapping_Trigger(in, shortcut);
	if (key < 0) {
		return 0;
	}
	XwaInput_SuppressKey(key);
	return 1;
}

/* Window-mode changes route through the video options module so the apply
 * callback, the options screen, and config persistence stay in agreement. */
static void set_window_mode_option(XwaModernWindowMode mode) {
	XwaModernVideoOptions options;
	XwaModernVideoOptions_Get(&options);
	if (options.window_mode != mode) {
		options.window_mode = mode;
		XwaModernVideoOptions_Set(&options);
	}
}

static void apply_modern_input_options(const XwaModernInputOptions* options) {
	/* Installing bindings releases the keys held, so only new bindings are
	 * installed. */
	static XwaKeyboardBindings installed_keyboard;
	static int keyboard_installed;

	if (!options) {
		return;
	}
	XwaMouseFlight_SetOptions(options);
	/* Installed at the next host frame. */
	XwaControllerMapping_SetOptions(&options->controllers);
	if (!keyboard_installed || !XwaKeyboardMapping_Equal(&installed_keyboard, &options->keyboard)) {
		installed_keyboard = options->keyboard;
		keyboard_installed = 1;
		XwaKeyboardMapping_Install(&options->keyboard);
	}
	XwaControllerMapping_SetRumbleEnabled(options->rumble_enabled);
	Config_ApplyModernInputOptions(options);
}

/* Each host frame, new gamepad models get the shipped gamepad defaults, and
 * the connected models move to the front of the list, which keeps it in the
 * order of last use. While the options window is open, it adds the new
 * gamepads in its draft. */
static void discover_controllers(const AeronInputSnapshot* in) {
	static char previous_error[512];
	static XwaModernInputOptions current;
	static XwaModernInputOptions candidate;
	char error[512] = { 0 };
	int ok;

	if (XwaOptionsWindow_Open()) {
		return;
	}
	XwaModernInputOptions_Get(&current);
	candidate = current;
	XwaControllerOptions_MoveConnectedFirst(&candidate.controllers, in);
	ok = XwaControllerOptions_InitializeGamepads(
		&candidate.controllers, XwaModernInputOptions_GamepadDefaults(), in, error, sizeof error);
	if (!XwaControllerOptions_Equals(&candidate.controllers, &current.controllers) &&
		!XwaModernInputOptions_Set(&candidate)) {
		snprintf(error, sizeof error, "%s", "the new controller could not be added");
		ok = 0;
	}
	if (!ok && strcmp(previous_error, error) != 0) {
		Aeron_LogWarn("xwa.input", "%s", error);
	}
	snprintf(previous_error, sizeof previous_error, "%s", ok ? "" : error);
}


static int persist_modern_input_options(const XwaModernInputOptions* options, char* error,
										size_t error_size) {
	return XwaHostConfig_SaveInputOptions(Aeron_GetVfs(), options, error, error_size);
}

static int persist_option_switch(const char* key, int value, char* error, size_t error_size) {
	return XwaHostConfig_SaveSwitch(Aeron_GetVfs(), key, value, error, error_size);
}

/* paths.mod. It is kept when the original game is chosen, so the mod stays
 * in the list. */
static char s_configured_mod[XWA_INSTALL_PATH_CAPACITY];

static int persist_mod_choice(const char* path, char* error, size_t error_size) {
	if (!path[0]) {
		return XwaInstallPaths_SaveMod(Aeron_GetVfs(), s_configured_mod, 0, error, error_size);
	}
	return XwaInstallPaths_SaveMod(Aeron_GetVfs(), path, 1, error, error_size);
}

/* Gives the options the mods and the one in use. A mod from --mod is listed
 * too, so it shows as the one in use. */
static void configure_mods(const XwaLaunchOptions* launch, const XwaHostConfig* host_config,
						   const char* selected_mod) {
	XwaModList list;
	XwaModernModEntry* entries;
	const char* configured = launch->mod_path[0] ? selected_mod : host_config->paths.mod;
	char mods_folder[XWA_INSTALL_PATH_CAPACITY] = "";
	size_t running = SIZE_MAX;

	snprintf(s_configured_mod, sizeof s_configured_mod, "%s", host_config->paths.mod);
	XwaModList_Folder(host_config->paths.mods, mods_folder, sizeof mods_folder);
	if (!XwaModList_Scan(mods_folder, configured, &list)) {
		Aeron_LogError("xwa.config", "the mods can't be listed");
		return;
	}
	entries = (XwaModernModEntry*)calloc(list.count ? list.count : 1, sizeof *entries);
	if (!entries) {
		XwaModList_Free(&list);
		return;
	}
	for (size_t i = 0; i < list.count; ++i) {
		entries[i].name = list.entries[i].name;
		entries[i].path = list.entries[i].path;
		entries[i].problem = list.entries[i].problem[0] ? list.entries[i].problem : NULL;
		if (XwaModList_SamePath(list.entries[i].path, selected_mod)) {
			running = i;
		}
	}
	XwaModernMods_Configure(entries, list.count, running, launch->mod_path[0] != 0, persist_mod_choice);
	Aeron_LogInfo("xwa.config", "mods: %zu listed, including the original game", list.count);
	free(entries);
	XwaModList_Free(&list);
}

/* Shows a setup message about a problem that the player can fix. Unlike
 * Aeron_FatalError, it doesn't ask for the logs. */
static void show_setup_message(AeronMessageBoxKind kind, const char* message) {
	static const AeronMessageBoxButton buttons[] = {
		{ 0, "OK", 1, 1 },
	};
	const AeronMessageBoxOptions options = {
		.kind = kind,
		.title = "OpenXWA",
		.message = message,
		.buttons = buttons,
		.button_count = sizeof buttons / sizeof buttons[0],
	};

	Aeron_ShowMessageBox(&options, NULL);
}

static void show_setup_error(const char* message) { show_setup_message(AERON_MESSAGE_BOX_ERROR, message); }

static int show_first_launch_prompt(int* cancelled, char* error, size_t error_size) {
	static const AeronMessageBoxButton buttons[] = {
		{ XWA_SETUP_CHOOSE_FOLDER, "Choose Game Folder", 1, 0 },
		{ XWA_SETUP_QUIT, "Quit", 0, 1 },
	};
	const AeronMessageBoxOptions options = {
		.kind = AERON_MESSAGE_BOX_INFORMATION,
		.title = "Set up OpenXWA",
		.message = "OpenXWA requires data files from an original X-Wing Alliance installation.\n\n"
				   "Select an installed game folder (for example a GOG or Steam install) containing:\n\n"
				   "    FLIGHTMODELS\n"
				   "    FRONTRES\n"
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
		char candidate[XWA_INSTALL_PATH_CAPACITY];
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
				Aeron_WaitForNextFrame(16667);
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
		if (XwaInstall_ValidateGameData(vfs, candidate, selected, selected_capacity, validation_error,
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
					 "Select an installed game folder containing FLIGHTMODELS, MISSIONS, and RESDATA.TXT.",
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
		return XwaInstall_ValidateGameData(vfs, launch->game_data_path, selected, selected_capacity, error,
										   error_size);
	}
	if (host_config->paths.game_data[0]) {
		if (XwaInstall_ValidateGameData(vfs, host_config->paths.game_data, selected, selected_capacity, error,
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
					 host_config->paths.game_data, error);
			if (!Aeron_ShowMessageBox(&options, &selected_button)) {
				snprintf(error, error_size, "Could not show the saved game-data error.");
				return 0;
			}
			if (selected_button == XWA_SETUP_TRY_AGAIN) {
				if (XwaInstall_ValidateGameData(vfs, host_config->paths.game_data, selected,
												selected_capacity, error, error_size)) {
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
	XwaAppUi app_ui;
	char selected_game_data[XWA_INSTALL_PATH_CAPACITY];
	char selected_mod[XWA_INSTALL_PATH_CAPACITY];
	char config_error[1024];
	char mod_notice[1024 + XWA_INSTALL_PATH_CAPACITY];
	XwaModernWindowMode initial_window_mode;
	int exit_code;
	int setup_cancelled;

	if (!XwaLaunchOptions_Parse(argc, argv, &launch, config_error, sizeof config_error)) {
		Aeron_LogError("xwa.config", "%s", config_error);
		return 2;
	}
	if (launch.help) {
		fputs(XwaLaunchOptions_Help(), stdout);
		return 0;
	}
	memset(&config, 0, sizeof(config));
	config.org_name = "TotallyOpen";
	config.app_name = "OpenXWA";
	config.app_version = OPENXWA_VERSION;
	config.resource_root = launch.resource_root[0] ? launch.resource_root : NULL;
	config.resource_path = "resources";
	config.shader_path = "shaders";
	config.window_title = "OpenXWA";
	config.window_icon_bmp = xwa_window_icon_bmp;
	config.window_icon_bmp_size = sizeof(xwa_window_icon_bmp);
	/* Windowed size auto-fits the primary display. The logical frame starts
	 * at the 16:9 default and tracks the window aspect once the main loop
	 * runs (XwaPresentation_SyncToWindow). */
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
		show_setup_error(message);
		Aeron_Shutdown();
		return 1;
	}
	initial_window_mode = XWA_MODERN_WINDOW_MODE_FULLSCREEN;
	if (host_config.video_options_override_mask & XWA_MODERN_VIDEO_OVERRIDE_WINDOW_MODE) {
		initial_window_mode = host_config.video_options.window_mode;
	}
	if (!resolve_game_data(&launch, &host_config, selected_game_data, sizeof selected_game_data,
						   &setup_cancelled, config_error, sizeof config_error)) {
		if (!setup_cancelled) {
			Aeron_LogError("xwa.config", "%s", config_error);
			show_setup_error(config_error);
		}
		Aeron_Shutdown();
		return setup_cancelled ? 0 : 1;
	}
	if (strcmp(host_config.paths.game_data, selected_game_data) != 0 &&
		!XwaInstallPaths_SaveGameData(Aeron_GetVfs(), selected_game_data, config_error,
									  sizeof config_error)) {
		Aeron_LogError("xwa.config", "%s", config_error);
		show_setup_error(config_error);
		Aeron_Shutdown();
		return 1;
	}
	Aeron_LogInfo("xwa.config", "game data: %s", selected_game_data);
	if (!XwaSetup_SetModRoot(Aeron_GetVfs(), &launch, &host_config.paths, selected_mod, sizeof selected_mod,
							 mod_notice, sizeof mod_notice, config_error, sizeof config_error)) {
		Aeron_LogError("xwa.config", "%s", config_error);
		show_setup_error(config_error);
		Aeron_Shutdown();
		return 1;
	}
	if (mod_notice[0]) {
		Aeron_LogWarn("xwa.config", "%s", mod_notice);
		show_setup_message(AERON_MESSAGE_BOX_WARNING, mod_notice);
	}
	Aeron_SetFullscreen(initial_window_mode == XWA_MODERN_WINDOW_MODE_FULLSCREEN);
	/* The setup dialogs above are separate OS windows; on some platforms
	 * (macOS) closing them does not re-key the game window, which would start
	 * the game inside its unfocused pause. Ask for input focus back. */
	Aeron_RaiseWindow();
	if (!selected_mod[0]) {
		Aeron_LogInfo("xwa.config", "mod: none, the original game");
	} else {
		Aeron_LogInfo("xwa.config", "mod: %s", selected_mod);
	}
	configure_mods(&launch, &host_config, selected_mod);
	Aeron_LogInfo("xwa.config", "resources: %s", Aeron_ResourceRoot());
	Aeron_LogInfo("xwa.config", "flight simulation step: %d ticks", host_config.flight_simulation_step_ticks);
	Aeron_LogInfo("xwa.config", "OPT smoothing angle: %.3g degrees",
				  (double)host_config.model_smooth_angle_degrees);
	Aeron_LogInfo("xwa.config", "OPT emissive strength: %.3g",
				  (double)host_config.model_opt_emissive_strength);
	Aeron_LogInfo("xwa.config", "OPT projectile emissive strength: %.3g",
				  (double)host_config.model_opt_projectile_emissive_strength);
	Aeron_LogInfo("xwa.config", "engine emissive strength: %.3g",
				  (double)host_config.model_engine_emissive_strength);
	Aeron_LogInfo("xwa.config", "force OPT models: %s", host_config.force_opt_models ? "yes" : "no");
	Aeron_LogInfo("xwa.config", "prefer original 2D assets: %s",
				  host_config.prefer_original_2d ? "yes" : "no");
	Aeron_LogInfo("xwa.config", "prefer original movies: %s",
				  host_config.prefer_original_movies ? "yes" : "no");
	Aeron_LogInfo("xwa.config", "environment maps: %s (manifest %s, sun max radiance %.4g)",
				  host_config.environment_maps_enabled ? "yes" : "no", host_config.environment_maps_manifest,
				  (double)host_config.environment_maps_sun_max_radiance);
	Aeron_LogInfo("xwa.config", "captured lighting: %s (%d px, %d specular samples)",
				  host_config.captured_lighting_enabled ? "yes" : "no", host_config.captured_lighting_size,
				  host_config.captured_lighting_specular_samples);
	const XwaRemasterInitOptions remaster_options = {
		.opt_smooth_angle_degrees = host_config.model_smooth_angle_degrees,
		.opt_emissive_strength = host_config.model_opt_emissive_strength,
		.opt_projectile_emissive_strength = host_config.model_opt_projectile_emissive_strength,
		.engine_emissive_strength = host_config.model_engine_emissive_strength,
		.prefer_original_2d = host_config.prefer_original_2d,
		.environment_maps_enabled = host_config.environment_maps_enabled,
		.environment_maps_manifest = host_config.environment_maps_manifest,
		.environment_maps_sun_max_radiance = host_config.environment_maps_sun_max_radiance,
		.captured_lighting =
			{
				.enabled = host_config.captured_lighting_enabled,
				.size = host_config.captured_lighting_size,
				.specular_samples = host_config.captured_lighting_specular_samples,
				.diffuse_strength = host_config.captured_lighting_diffuse_strength,
				.specular_strength = host_config.captured_lighting_specular_strength,
				.has_ambient_fill = host_config.captured_lighting_has_ambient_fill,
				.ambient_fill = { host_config.captured_lighting_ambient_fill[0],
								  host_config.captured_lighting_ambient_fill[1],
								  host_config.captured_lighting_ambient_fill[2] },
			},
		.video_options = host_config.video_options,
		.video_options_override_mask = host_config.video_options_override_mask,
		.render_width = launch.render_width,
		.render_height = launch.render_height,
	};
	if (!XwaRemaster_Init(&remaster_options)) {
		Aeron_FatalError("OpenXWA", "Required remaster resources could not be loaded.");
		XwaRemaster_Shutdown();
		Aeron_Shutdown();
		return 1;
	}
	if (!XwaAppUi_Init(&app_ui, config_error, sizeof config_error) ||
		!XwaOptionsWindow_Init(&app_ui, config_error, sizeof config_error)) {
		Aeron_LogError("xwa.options", "%s", config_error);
		Aeron_FatalError("OpenXWA", config_error);
		XwaAppUi_Shutdown(&app_ui);
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
	XwaKeyboardMapping_SetPolicy(Aeron_DebugUiAvailable() != 0);
	XwaControllerMapping_Init(&host_config.input_options.controllers);
	XwaModernInputOptions_Configure(&host_config.input_defaults, &host_config.gamepad_defaults,
									&host_config.input_options, apply_modern_input_options,
									persist_modern_input_options);
	apply_modern_input_options(&host_config.input_options);
	if (host_config.input_upgraded) {
		/* Saved now, so the notice shows only once. */
		XwaModernInputOptions_MarkDirty();
		XwaModernInputOptions_Flush();
		XwaOptionsWindow_ShowControllerResetNotice();
	}
	XwaOptionSection_SetPersist(persist_option_switch);
	Aeron_LogInfo(
		"xwa.config", "mouse flight control: %s (%s mode, sensitivity %d, invert Y: %s)",
		host_config.input_options.mouse_flight_enabled ? "on" : "off",
		host_config.input_options.mouse_mode == XWA_MODERN_MOUSE_MODE_POSITION ? "position" : "rate",
		host_config.input_options.mouse_sensitivity, host_config.input_options.mouse_invert_y ? "yes" : "no");

	XwaPort_SetCommandLine(launch.game_command_line);
	XwaModernFlightTiming_Configure(host_config.flight_simulation_step_ticks);
	XwaFilmState_Configure(selected_mod);
	XwaFilmBenchmark_Configure(launch.play_film, launch.benchmark_out, launch.film_pause_at, launch.film_capture,
							   launch.film_benchmark);
	/* XwaRemaster_Init succeeded above, so the HD renderer is available. */
	XwaModelSource_Configure(!host_config.force_opt_models);
	XwaMovieTask_Configure(host_config.prefer_original_movies);
	if (!XwaPort_Init()) {
		XwaOptionsWindow_Shutdown();
		XwaAppUi_Shutdown(&app_ui);
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
		int last_fullscreen = Aeron_Fullscreen();
		while (!Aeron_QuitRequested() && !XwaPort_ShouldQuit()) {
			const uint64_t frame_start_us = Aeron_NowUs();
			const int32_t delta_us = Aeron_BeginFrame();
			const AeronInputSnapshot* in = Aeron_InputSnapshot();
			int toggled = 0;
			/* With a fixed render size, the 2D frame takes the render's shape,
			   and the presentation fits it in the window. */
			if (launch.render_width > 0) {
				XwaPresentation_SyncToWindow(launch.render_width, launch.render_height);
			} else if (in) {
				XwaPresentation_SyncToWindow(in->window_width, in->window_height);
			}
			/* Reconcile OS-initiated fullscreen transitions (e.g. the macOS
			 * green button) into the stored option. Edge-triggered on the
			 * observed state so an in-flight transition is never fought. */
			const int fullscreen = Aeron_Fullscreen();
			if (fullscreen != last_fullscreen) {
				last_fullscreen = fullscreen;
				set_window_mode_option(fullscreen ? XWA_MODERN_WINDOW_MODE_FULLSCREEN
												  : XWA_MODERN_WINDOW_MODE_WINDOWED);
			}
			discover_controllers(in);
			int window_opened = XwaOptionsWindow_BeginFrame(in);
			XwaPort_SetShortcutsCaptured(XwaOptionsWindow_CapturesKeyboard());
			/* Host shortcuts (keyboard_mapping.h). Their keys can't be bound.
			 * The toggle frame also skips the game tick. Cmd+Ctrl+F on macOS,
			 * Alt+Enter elsewhere. */
			if (host_shortcut_pressed(in, XWA_KEYBOARD_SHORTCUT_FULLSCREEN)) {
				set_window_mode_option(fullscreen ? XWA_MODERN_WINDOW_MODE_WINDOWED
												  : XWA_MODERN_WINDOW_MODE_FULLSCREEN);
				toggled = 1;
			}
			if (host_shortcut_pressed(in, XWA_KEYBOARD_SHORTCUT_HOST_PAUSE)) {
				paused = !paused;
				toggled = 1;
				Aeron_AudioSetPaused(paused);
				Aeron_LogInfo("xwa", "%s", paused ? "paused" : "resumed");
			}
			/* Ctrl+Alt+L is a portable alias for the original Scroll Lock cockpit
			 * mouse-look action. Queue the original action so its gameplay gates remain
			 * authoritative. */
			if (host_shortcut_pressed(in, XWA_KEYBOARD_SHORTCUT_MOUSE_LOOK) &&
				XwaPort_QueueMouseLookToggle()) {
				toggled = 1;
			}
			/* Ctrl+Alt+M releases the flight mouse capture back to the OS and
			 * re-captures it. */
			if (host_shortcut_pressed(in, XWA_KEYBOARD_SHORTCUT_MOUSE)) {
				XwaPort_ToggleMouseCapture();
				toggled = 1;
			}
			/* Ctrl+H toggles the Dynamic Cockpit holograms while a cockpit
			 * with screens is shown. */
			if (XwaRemasterDc_Active() && host_shortcut_pressed(in, XWA_KEYBOARD_SHORTCUT_HOLOGRAMS)) {
				XwaRemasterDc_ToggleHolograms();
				toggled = 1;
			}
			XwaRemaster_BeginFrame(in);
			if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
				break;
			}
			/* A frozen film runs no film steps either (film_benchmark.h). */
			const int film_frozen = XwaFilmBenchmark_IsFrozen();
			if (paused || toggled || film_frozen) {
				XwaPort_PausedFrame();
			} else {
				XwaPort_Tick(delta_us);
			}
			if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
				break;
			}
			window_opened |= XwaOptionsWindow_ConsumeRuntimeRequest();
			const uint64_t remaster_start_us = Aeron_NowUs();
			XwaRemaster_Frame(delta_us);
			const uint64_t remaster_us = Aeron_NowUs() - remaster_start_us;
			if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
				break;
			}
			/* Drawn over the game. The frame that opens it only starts it, so the
			 * input that opened it doesn't reach it. */
			if (!window_opened) {
				XwaOptionsWindow_Frame(in, (float)delta_us / 1000000.0f);
			}
			if (!Aeron_Present()) {
				Aeron_RequestFatalRendererError("frame presentation");
				break;
			}
			XwaFilmBenchmark_NoteHostFrame(remaster_us, Aeron_NowUs() - frame_start_us);
			if (XwaFilmBenchmark_CaptureDue()) {
				XwaFilmBenchmark_CaptureDone(XwaRemaster_CaptureFlightFrame(XwaFilmBenchmark_CaptureName()));
			}
			if (XwaFilmBenchmark_MeasurementDue()) {
				XwaFilmBenchmark_MeasurementDone(XwaRemaster_BenchmarkFlightFrames());
			}
			uint64_t wake_us = presentation_interval_us();
			if (!paused && !film_frozen) {
				const uint64_t task_us = XwaPort_NextWakeDelayUs();
				if (task_us < wake_us)
					wake_us = task_us;
			}
			Aeron_WaitForNextFrame(wake_us);
		}
	}

	exit_code = Aeron_FatalErrorRequested() ? 1 : XwaPort_GetExitCode();
	XwaOptionsWindow_FlushForExit();
	XwaOptionsWindow_Shutdown();
	XwaAppUi_Shutdown(&app_ui);
	XwaRemaster_Shutdown();
	XwaPort_Shutdown();
	Aeron_Shutdown();

	return exit_code;
}
