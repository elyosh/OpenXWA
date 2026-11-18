#include "aeron/main.h"
#include "aeron/aeron.h"
#include "aeron/audio.h"
#include "aeron/debug.h"
#include "aeron/time.h"
#include "host_config.h"
#include "install_check.h"
#include "install_paths.h"
#include "mod_list.h"
#include "mod_update.h"
#include "options/mods_page.h"
#include "options/options_window.h"
#include "options/video_page.h"
#include "setup.h"
#include "ui.h"
#include "window_icon.h"
#include "xwa/config/game_config.h"
#include "xwa_remaster/cockpit_buttons.h"
#include "xwa_remaster/dynamic_cockpit.h"
#include "xwa_remaster/menu_hands.h"
#include "xwa_remaster/vr_hands.h"
#include "xwa_remaster/vr_keyboard.h"
#include "xwa_remaster/xwa_remaster.h"
#include "xwa_runtime/config/option_section.h"
#include "xwa_runtime/input/capture.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/input/keyboard_mapping.h"
#include "xwa_runtime/input/mouse_flight.h"
#include "xwa_runtime/input/vr_keyboard.h"
#include "xwa_runtime/input/vr_stick.h"
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

static AeronWindowMode aeron_window_mode(XwaModernWindowMode mode) {
	switch (mode) {
		case XWA_MODERN_WINDOW_MODE_FULLSCREEN:
			return AERON_WINDOW_FULLSCREEN;
		case XWA_MODERN_WINDOW_MODE_SPAN:
			return AERON_WINDOW_SPAN;
		default:
			return AERON_WINDOW_WINDOWED;
	}
}

static void apply_modern_video_options(const XwaModernVideoOptions* options) {
	if (!options) {
		return;
	}
	Aeron_SetWindowMode(aeron_window_mode(options->window_mode));
	XwaRemaster_SetVideoOptions(options);
}

static int persist_modern_video_options(const XwaModernVideoOptions* options, char* error,
										size_t error_size) {
	return XwaHostConfig_SaveVideoOptions(Aeron_GetVfs(), options, error, error_size);
}

/* The audio options that apply at once. The speaker layout applies at the
 * next start (see configure_modern_audio). */
static void apply_modern_audio_options(const XwaModernAudioOptions* options) {
	AeronAudioSpatialSettings settings;

	if (!options) {
		return;
	}
	memset(&settings, 0, sizeof settings);
	settings.mode = options->output == XWA_MODERN_AUDIO_HEADPHONES ? AERON_AUDIO_OUTPUT_HEADPHONES
																   : AERON_AUDIO_OUTPUT_SPEAKERS;
	settings.air_absorption = options->distance_filter ? 1.0f : 0.0f;
	Aeron_AudioSetSpatialSettings(&settings);
}

static int persist_modern_audio_options(const XwaModernAudioOptions* options, char* error,
										size_t error_size) {
	return XwaHostConfig_SaveAudioOptions(Aeron_GetVfs(), options, error, error_size);
}

/* Sets the output's channel count, which can only change while no sound
 * exists (see XwaModernAudioOptions_OutputChannels). Then applies the other
 * audio options. */
static void configure_modern_audio(void) {
	static const int layout_channels[] = { 2, 6, 8 };
	XwaModernAudioOptions options;

	XwaModernAudioOptions_Get(&options);
	const int device = Aeron_AudioDeviceChannels();
	const int channels = XwaModernAudioOptions_OutputChannels(&options, device);
	if (options.output == XWA_MODERN_AUDIO_SPEAKERS && channels < layout_channels[options.speaker_layout]) {
		Aeron_LogInfo("xwa.audio", "the audio device has %d channels, so the output has %d instead of %d",
					  device, channels, layout_channels[options.speaker_layout]);
	}
	if (!Aeron_AudioSetOutputChannels(channels)) {
		Aeron_LogWarn("xwa.audio", "the sound output stays at %d channels instead of %d",
					  Aeron_AudioOutputChannels(), channels);
	}
	XwaModernAudioOptions_Configure(apply_modern_audio_options, persist_modern_audio_options);
	apply_modern_audio_options(&options);
	Aeron_LogInfo("xwa.config", "audio: %s, %d output channels, distance filtering %s",
				  options.output == XWA_MODERN_AUDIO_HEADPHONES ? "headphones" : "speakers",
				  Aeron_AudioOutputChannels(), options.distance_filter ? "on" : "off");
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

/* Returns 1 when a key bound to the host action `action` was pressed, 2 when
 * a controller button was, 0 otherwise. */
static int host_action_pressed(const AeronInputSnapshot* in, XwaInputAction action) {
	const int button = XwaControllerMapping_TakeHostAction(action);
	/* The options window gets the keys while it captures a binding. */
	const int key = XwaOptionsWindow_CapturesKeyboard() ? -1 : XwaKeyboardMapping_ActionTrigger(in, action);
	if (key >= 0) {
		XwaInput_SuppressKey(key);
		return 1;
	}
	return button ? 2 : 0;
}

/* Window mode changes go through the video options module, so the window,
 * the Video tab of OpenXWA Options and config.yaml always agree. */
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
	XwaVrStick_SetOptions(options);
	/* Installed at the next host frame. */
	XwaControllerMapping_SetOptions(&options->controllers);
	if (!keyboard_installed || !XwaKeyboardMapping_Equal(&installed_keyboard, &options->keyboard)) {
		installed_keyboard = options->keyboard;
		keyboard_installed = 1;
		XwaKeyboardMapping_Install(&options->keyboard);
	}
	XwaControllerMapping_SetRumbleEnabled(options->rumble_enabled);
	XwaRemasterVrHands_SetEnabled(options->vr_show_hands);
	XwaVrHands_SetUseMod(options->vr_mod_hands);
	XwaVrKeyboard_SetUseMod(options->vr_mod_keyboard);
	XwaRemasterMenuHands_SetControllers(options->vr_menu_controllers);
	XwaRemasterCockpitButtons_SetOptions(options->vr_cockpit_buttons, options->vr_button_ray_m,
										 options->rumble_enabled);
	XwaRemasterVrKeyboard_SetHaptics(options->rumble_enabled);
	Config_ApplyModernInputOptions(options);
}

/* Each host frame, new gamepads and VR hands get a model with the shipped
 * defaults, and the connected models move to the front of the list, which
 * keeps it in the order of last use. While the options window is open, it
 * adds the new devices in its draft. */
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
	ok = XwaControllerOptions_InitializeDefaults(
		&candidate.controllers, XwaModernInputOptions_ControllerDefaults(), in, error, sizeof error);
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

/* The fake VR hand of a launch option, as the remaster takes it. */
static XwaRemasterStereoHand stereo_hand_option(const XwaLaunchStereoHand* from) {
	XwaRemasterStereoHand to = { .set = from->set, .buttons = from->buttons };
	memcpy(to.position_m, from->position_m, sizeof to.position_m);
	memcpy(to.angles_deg, from->angles_deg, sizeof to.angles_deg);
	return to;
}

/* Keeps the bindings away from the inputs of a hand that something else
 * uses. While a hand points at the virtual screen, its trigger and squeeze
 * click there and its thumbstick scrolls. The virtual stick and throttle use
 * the squeeze, and the trigger of the hand that holds the stick. A hand that
 * points at a cockpit button presses it with its trigger. */
static void mask_hand_sources(void) {
	const int pointer = Aeron_XrPointerHand();
	for (int hand = 0; hand < 2; hand++) {
		uint32_t buttons = 0;
		uint32_t axes = 0;
		if (hand == pointer) {
			buttons = (1u << AERON_VR_HAND_BUTTON_TRIGGER) | (1u << AERON_VR_HAND_BUTTON_SQUEEZE);
			axes = (1u << AERON_VR_HAND_AXIS_TRIGGER) | (1u << AERON_VR_HAND_AXIS_SQUEEZE) |
				   (1u << AERON_VR_HAND_AXIS_THUMBSTICK_Y);
		}
		XwaVrStick_SourceMask(hand, &buttons, &axes);
		/* On the virtual keyboard, the trigger presses a key. */
		if (XwaRemasterVrKeyboard_HandOn(hand)) {
			buttons |= (1u << AERON_VR_HAND_BUTTON_TRIGGER) | (1u << AERON_VR_HAND_BUTTON_SQUEEZE);
			axes |= (1u << AERON_VR_HAND_AXIS_TRIGGER) | (1u << AERON_VR_HAND_AXIS_SQUEEZE);
		}
		/* On a cockpit button, the trigger presses the button. */
		if (XwaRemasterCockpitButtons_OnButton(hand)) {
			buttons |= 1u << AERON_VR_HAND_BUTTON_TRIGGER;
			axes |= 1u << AERON_VR_HAND_AXIS_TRIGGER;
		}
		XwaControllerMapping_SetSourceMask(hand, buttons, axes);
	}
}

static int persist_modern_input_options(const XwaModernInputOptions* options, char* error,
										size_t error_size) {
	return XwaHostConfig_SaveInputOptions(Aeron_GetVfs(), options, error, error_size);
}

static int persist_option_switch(const char* key, int value, char* error, size_t error_size) {
	return XwaHostConfig_SaveSwitch(Aeron_GetVfs(), key, value, error, error_size);
}

/* Restart Now in the Mods tab. OpenXWA quits as usual, then Aeron_Shutdown
 * starts it again with the same launch options. With a pilot, the new start
 * skips the intro and opens the concourse. */
static void request_restart(const XwaLaunchOptions* launch) {
	XwaRelaunchArgs args;
	char error[256];

	if (!XwaLaunchOptions_RelaunchArgs(launch, XwaPort_HasPilot(), &args, error, sizeof error)) {
		Aeron_LogError("xwa", "cannot restart: %s", error);
		XwaModsPage_RestartFailed();
		return;
	}
	if (!Aeron_RequestRelaunch(args.args, args.count)) {
		XwaModsPage_RestartFailed();
		return;
	}
	Aeron_LogInfo("xwa", "restarting");
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

/* Aeron asks for the VR settings before it creates the window and the GPU
 * device. */
static void load_startup_options(AeronVfs* vfs, void* userdata, AeronStartupOptions* options) {
	XwaHostConfig_LoadStartup(vfs, (const XwaLaunchOptions*)userdata, options);
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
	config.startup_options = load_startup_options;
	config.startup_userdata = &launch;

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
	Aeron_SetWindowMode(aeron_window_mode(initial_window_mode));
	/* The setup dialogs above are separate OS windows; on some platforms
	 * (macOS) closing them does not re-key the game window, which would start
	 * the game inside its unfocused pause. Ask for input focus back. */
	Aeron_RaiseWindow();
	if (!selected_mod[0]) {
		Aeron_LogInfo("xwa.config", "mod: none, the original game");
	} else {
		Aeron_LogInfo("xwa.config", "mod: %s", selected_mod);
	}
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
		.flight_views = launch.flight_views,
		.head_pose = { launch.head_pose[0], launch.head_pose[1], launch.head_pose[2], launch.head_pose[3],
					   launch.head_pose[4], launch.head_pose[5] },
		.head_pose_set = launch.head_pose_set,
		.stereo_hands = { stereo_hand_option(&launch.stereo_hands[0]),
						  stereo_hand_option(&launch.stereo_hands[1]) },
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
		const XwaModsPageSetup mods_setup = {
			.game_data = selected_game_data,
			.running_mod = selected_mod,
			.launch_override = launch.mod_path[0] != 0,
		};
		XwaModsPage_Init(XwaAppUi_Context(&app_ui), &mods_setup);
	}
	{
		XwaModernVideoOptions effective_video_options;
		XwaRemaster_GetVideoOptions(&effective_video_options);
		effective_video_options.window_mode = initial_window_mode;
		/* The remaster doesn't keep the VR mode: it only applies at startup. */
		effective_video_options.vr_mode =
			(host_config.video_options_override_mask & XWA_MODERN_VIDEO_OVERRIDE_VR_MODE)
				? host_config.video_options.vr_mode
				: XWA_MODERN_VR_OFF;
		/* Nor the scale of the eye images. */
		effective_video_options.vr_render_scale =
			(host_config.video_options_override_mask & XWA_MODERN_VIDEO_OVERRIDE_VR_RENDER_SCALE)
				? host_config.video_options.vr_render_scale
				: 1.0f;
		XwaModernVideoOptions_Configure(&effective_video_options, apply_modern_video_options,
										persist_modern_video_options);
	}
	configure_modern_audio();
	if (launch.classic_2d_sound) {
		XwaModernAudioOptions_SetClassic2DSound(1);
		Aeron_LogInfo("xwa.config", "3D sound is off for this run (--classic-2d-sound)");
	}
	XwaKeyboardMapping_SetPolicy(Aeron_DebugUiAvailable() != 0);
	XwaControllerMapping_Init(&host_config.input_options.controllers);
	XwaModernInputOptions_Configure(&host_config.input_defaults, &host_config.controller_defaults,
									&host_config.input_options, apply_modern_input_options,
									persist_modern_input_options);
	apply_modern_input_options(&host_config.input_options);
	if (host_config.input_upgraded) {
		/* Saved now, so the notice shows only once. */
		XwaModernInputOptions_MarkDirty();
		XwaModernInputOptions_Flush();
		XwaOptionsWindow_ShowControllerResetNotice();
	}
	if (Aeron_XrStartError()) {
		XwaOptionsWindow_ShowVrStartNotice(Aeron_XrStartError());
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
	XwaFilmBenchmark_Configure(launch.play_film, launch.benchmark_out, launch.record_audio,
							   launch.film_pause_at, launch.film_capture, launch.film_benchmark);
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
	/* The mods replaced or deleted at startup are deleted in the
	 * background. */
	{
		char mods_folder[XWA_INSTALL_PATH_CAPACITY];
		if (XwaModList_Folder(host_config.paths.mods, mods_folder, sizeof mods_folder)) {
			XwaModUpdate_StartCleanup(mods_folder, host_config.paths.mod);
		}
	}
	/* Cmd+P host pause (the TIE shell model): the game tick is skipped
	 * — sim and snapshots freeze — while the HD remaster keeps
	 * rendering the frozen snapshot every frame (debug UI and
	 * motion-blur inspection stay live). The toggle frame also
	 * skips the tick, so the P press edge never reaches the classic
	 * input path (edges pump inside the tick). */
	{
		int paused = 0;
		/* The game also pauses while the options window is open, except in a
		 * network session (XwaPort_OptionsWindowPausesGame). */
		int options_paused = 0;
		int headset_paused = 0;
		AeronWindowMode last_window_mode = Aeron_WindowMode();
		while (!Aeron_QuitRequested() && !XwaPort_ShouldQuit()) {
			const uint64_t frame_start_us = Aeron_NowUs();
			const int32_t delta_us = Aeron_BeginFrame();
			/* The film benchmark times the parts of each frame. */
			const int benchmark_frame = XwaFilmBenchmark_IsPlaying();
			const uint64_t begin_us = benchmark_frame ? Aeron_NowUs() - frame_start_us : 0;
			/* The window's close button or Cmd+Q during an import or a delete
			   of the Mods tab: the tab asks first, or quits when the delete
			   ends. */
			if (Aeron_QuitRequested() && XwaModsPage_RequestQuit()) {
				Aeron_ClearQuitRequest();
			}
			const AeronInputSnapshot* in = Aeron_InputSnapshot();
			int toggled = 0;
			/* In VR, the 2D frame takes the shape of the virtual screen: Aeron
			   draws the layers into it, and shows a copy in the window. With
			   a fixed render size, the 2D frame takes the render's shape, and
			   the presentation fits it in the window. */
			int screen_width = 0;
			int screen_height = 0;
			if (Aeron_XrEnabled() && Aeron_GetPresentationPixelSize(&screen_width, &screen_height)) {
				XwaPresentation_SyncToWindow(screen_width, screen_height);
			} else if (launch.render_width > 0) {
				XwaPresentation_SyncToWindow(launch.render_width, launch.render_height);
			} else if (in) {
				XwaPresentation_SyncToWindow(in->window_width, in->window_height);
			}
			/* Reconcile OS-initiated fullscreen transitions (e.g. the macOS
			 * green button) into the stored option. Edge-triggered on the
			 * observed state so an in-flight transition is never fought. */
			const AeronWindowMode window_mode = Aeron_WindowMode();
			const int fullscreen = window_mode == AERON_WINDOW_FULLSCREEN;
			if (window_mode != last_window_mode) {
				last_window_mode = window_mode;
				XwaModernVideoOptions video_options;
				XwaModernVideoOptions_Get(&video_options);
				/* A span that couldn't be applied shows as fullscreen. The
				   option stays on span, so that the window spans the displays
				   again when they allow it. */
				if (window_mode == AERON_WINDOW_SPAN) {
					set_window_mode_option(XWA_MODERN_WINDOW_MODE_SPAN);
				} else if (video_options.window_mode != XWA_MODERN_WINDOW_MODE_SPAN || !fullscreen) {
					set_window_mode_option(fullscreen ? XWA_MODERN_WINDOW_MODE_FULLSCREEN
													  : XWA_MODERN_WINDOW_MODE_WINDOWED);
				}
			}
			discover_controllers(in);
			XwaVrStick_Update(in);
			mask_hand_sources();
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
				Aeron_AudioSetPaused(paused || options_paused || headset_paused);
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
			/* Host actions (actions.h): the keys and buttons bound to them, in
			 * flight and in the menus. A key press also skips the game tick, so
			 * the game doesn't read the key. Recentring makes the headset's
			 * current direction straight ahead; Ctrl+Alt+R by default. */
			XwaControllerMapping_UpdateHostActions(in, XwaPort_InFlight());
			/* Outside flight, a button bound to options_menu is the Escape
			 * key: it does what Escape does in each screen. In flight the
			 * flight bindings handle it. The release follows a frame later. */
			{
				static int escape_down;
				if (escape_down) {
					Aeron_InjectKey(AERON_KEY_ESCAPE, 0, 0, 0, NULL);
					escape_down = 0;
				} else if (XwaControllerMapping_TakeHostAction(XWA_INPUT_ACTION_OPTIONS_MENU)) {
					Aeron_InjectKey(AERON_KEY_ESCAPE, 1, 0, 0, NULL);
					escape_down = 1;
				}
			}
			const int recenter = host_action_pressed(in, XWA_INPUT_ACTION_RECENTER_HEADSET);
			if (recenter && Aeron_XrEnabled()) {
				Aeron_XrRecenter();
				toggled |= recenter == 1;
			}
			/* The Dynamic Cockpit holograms, while a cockpit with screens is
			 * shown; Ctrl+H by default. */
			const int holograms = host_action_pressed(in, XWA_INPUT_ACTION_TOGGLE_HOLOGRAMS);
			if (holograms && XwaRemasterDc_Active()) {
				XwaRemasterDc_ToggleHolograms();
				toggled |= holograms == 1;
			}
			/* The virtual keyboard of VR: a text field of the options window
			 * opens it, and the player can open it for any other field. */
			XwaVrKeyboard_SetUiTextEdit(XwaOptionsWindow_EditsText());
			if (host_action_pressed(in, XWA_INPUT_ACTION_TOGGLE_VR_KEYBOARD) && Aeron_XrEnabled()) {
				XwaVrKeyboard_Toggle();
			}
			XwaRemaster_BeginFrame(in);
			if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
				break;
			}
			/* The window closes in XwaOptionsWindow_BeginFrame, so the tick of
			 * the frame that closes it runs, and the datapad sees it closed. */
			if (XwaPort_OptionsWindowPausesGame() != options_paused) {
				options_paused = !options_paused;
				Aeron_AudioSetPaused(paused || options_paused || headset_paused);
			}
			/* In VR, the game waits while the headset doesn't have the
			   focus: the user took it off, or a system menu covers the
			   game. A network session can't wait for one player. */
			if ((Aeron_XrEnabled() && !Aeron_XrFocused() && !XwaPort_InNetworkSession()) != headset_paused) {
				headset_paused = !headset_paused;
				Aeron_AudioSetPaused(paused || options_paused || headset_paused);
				Aeron_LogInfo("xwa", "%s", headset_paused ? "paused: the headset doesn't have the focus"
														  : "resumed: the headset has the focus");
			}
			/* A frozen film runs no film steps either (film_benchmark.h). */
			const int film_frozen = XwaFilmBenchmark_IsFrozen();
			const uint64_t tick_start_us = benchmark_frame ? Aeron_NowUs() : 0;
			if (paused || toggled || film_frozen || options_paused || headset_paused) {
				XwaPort_PausedFrame();
			} else {
				XwaPort_Tick(delta_us);
			}
			const uint64_t tick_us = benchmark_frame ? Aeron_NowUs() - tick_start_us : 0;
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
			if (XwaModsPage_TakeRestartRequest()) {
				request_restart(&launch);
			}
			if (XwaVideoPage_TakeRestartRequest()) {
				/* The new start reads the VR mode from the saved options. */
				XwaModernVideoOptions_Flush();
				request_restart(&launch);
			}
			const uint64_t present_start_us = benchmark_frame ? Aeron_NowUs() : 0;
			if (!Aeron_Present()) {
				Aeron_RequestFatalRendererError("frame presentation");
				break;
			}
			const uint64_t present_end_us = Aeron_NowUs();
			XwaFilmBenchmark_NoteHostFrame(begin_us, tick_us, remaster_us,
										   benchmark_frame ? present_end_us - present_start_us : 0,
										   present_end_us - frame_start_us);
			/* The capture and the measurement need the frozen frame's
			   render, which can wait for models or for the headset. */
			const int frozen_frame_ready = !XwaRemaster_FrameWaits();
			if (frozen_frame_ready && XwaFilmBenchmark_CaptureDue()) {
				XwaFilmBenchmark_CaptureDone(XwaRemaster_CaptureFlightFrame(XwaFilmBenchmark_CaptureName()));
			}
			if (frozen_frame_ready && XwaFilmBenchmark_MeasurementDue()) {
				XwaFilmBenchmark_MeasurementDone(XwaRemaster_BenchmarkFlightFrames());
			}
			uint64_t wake_us = presentation_interval_us();
			if (!paused && !film_frozen && !options_paused) {
				const uint64_t task_us = XwaPort_NextWakeDelayUs();
				if (task_us < wake_us)
					wake_us = task_us;
			}
			Aeron_WaitForNextFrame(wake_us);
		}
	}

	exit_code = Aeron_FatalErrorRequested() ? 1 : XwaPort_GetExitCode();
	XwaModUpdate_StopCleanup();
	XwaOptionsWindow_FlushForExit();
	XwaOptionsWindow_Shutdown();
	XwaAppUi_Shutdown(&app_ui);
	XwaRemaster_Shutdown();
	XwaPort_Shutdown();
	Aeron_Shutdown();

	return exit_code;
}
