#ifndef XWA_APP_SETUP_H
#define XWA_APP_SETUP_H

#include "aeron/vfs.h"
#include "install_paths.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A fake VR hand for a test run. */
typedef struct XwaLaunchStereoHand {
	/* The option was given. */
	int set;
	/* Position in metres from the head: right, up and forward. */
	float position_m[3];
	/* Yaw, pitch and roll in degrees, as for --head-pose. */
	float angles_deg[3];
	/* One bit for each AeronVrHandButton. */
	unsigned buttons;
} XwaLaunchStereoHand;

typedef struct XwaLaunchOptions {
	char game_data_path[XWA_INSTALL_PATH_CAPACITY];
	char resource_root[XWA_INSTALL_PATH_CAPACITY];
	char mod_path[XWA_INSTALL_PATH_CAPACITY];
	/* --play-film: a film of the user folder's film folder to play, after
	   which the game quits. --benchmark-out: the CSV file, in the user
	   folder, that gets its frame times. */
	char play_film[256];
	char benchmark_out[XWA_INSTALL_PATH_CAPACITY];
	/* --record-audio: the WAV file, in the user folder, that gets the film's
	   sound. */
	char record_audio[XWA_INSTALL_PATH_CAPACITY];
	/* --film-pause-at: the film time, in game ticks, where the film freezes,
	   or -1. --film-capture: the file name, in the user folder, for the
	   capture of the frozen frame. --film-benchmark: measure the GPU time of
	   the frozen frame, then quit. */
	int film_pause_at;
	char film_capture[XWA_INSTALL_PATH_CAPACITY];
	int film_benchmark;
	/* --classic-2d-sound: the game's 3D sound is off for this run, as in the
	   stock game by default. */
	int classic_2d_sound;
	/* --render-size: the fixed size of the 3D render, in pixels, or 0 to
	   follow the window. */
	int render_width;
	int render_height;
	/* --flight-views: draw the flight scene for this run as a surround
	   setup (1) or as the two eyes of a headset (2), or 0. */
	int flight_views;
	/* --head-pose: the test head's yaw, pitch and roll in degrees, then its
	   position in metres. Positive is right, up and clockwise, then right, up
	   and forward. It is the head of --flight-views stereo, or it moves the
	   one view. */
	float head_pose[6];
	int head_pose_set;
	/* --stereo-hand-left and --stereo-hand-right: a VR hand that doesn't
	   move, for --flight-views stereo. Index 0 is the left hand.
	   --stereo-hand-buttons: the buttons that each hand holds down. */
	XwaLaunchStereoHand stereo_hands[2];
	/* --vr: start in VR (1). --no-vr: start without VR (-1). 0 follows
	   video.vr_mode of the configuration. */
	int vr;
	/* --help or -h: print the launch options and quit. */
	int help;
	char game_command_line[1024];
} XwaLaunchOptions;

int XwaLaunchOptions_Parse(int argc, char** argv, XwaLaunchOptions* out, char* error, size_t error_size);

/* The arguments of a new start of OpenXWA, without the program. */
typedef struct XwaRelaunchArgs {
	char game_data[XWA_INSTALL_PATH_CAPACITY];
	char resource_root[XWA_INSTALL_PATH_CAPACITY];
	char mod[XWA_INSTALL_PATH_CAPACITY];
	char render_size[32];
	char head_pose[128];
	char stereo_hands[2][128];
	char stereo_hand_buttons[128];
	/* The game's arguments of XwaLaunchOptions, and " skipintro". */
	char game_command_line[1024 + sizeof " skipintro"];
	const char* args[23];
	int count;
} XwaRelaunchArgs;

/* Builds the arguments of a new start with the same launch options as
 * `launch`. The paths are made absolute, since the new start can be in another
 * folder (on macOS, it starts in "/"). The film options are dropped: they are
 * for one test run. The game's arguments are passed after "--", and
 * `skip_intro` adds "skipintro" to them. Returns 0 with `error` set when a
 * path can't be made absolute or is too long. */
int XwaLaunchOptions_RelaunchArgs(const XwaLaunchOptions* launch, int skip_intro, XwaRelaunchArgs* out,
								  char* error, size_t error_size);

/* The text that --help prints: every launch option, one line each. */
const char* XwaLaunchOptions_Help(void);

/* Sets the mod root to the --mod folder, or else to paths.mod when
 * paths.use_mod is true. Copies the folder it uses to `selected`. Without a
 * mod, `selected` is empty, the mod root stays unset and the game uses only
 * its own files.
 *
 * The mod folder must have a mod.yaml with the current mod format version.
 * The mod must also be complete, unless it comes from --mod.
 *   - A --mod folder that doesn't pass these checks is an error.
 *   - A paths.mod folder that doesn't pass them isn't used. The game starts
 *     with its own files, and `notice` explains why.
 * Otherwise, `notice` is empty. */
int XwaSetup_SetModRoot(AeronVfs* vfs, const XwaLaunchOptions* launch, const XwaInstallPaths* paths,
						char* selected, size_t selected_capacity, char* notice, size_t notice_size,
						char* error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
