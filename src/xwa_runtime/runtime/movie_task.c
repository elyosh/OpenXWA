#include "xwa_runtime/runtime/movie_task.h"

#include "aeron/aeron.h"
#include "aeron/video.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_dialog.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_file_stream.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/movie/movie.h"
#include "xwa/util/memory.h"
#include "xwa_runtime/runtime/frontend_task.h"
#include "xwa_runtime/runtime/port.h"
#include "xwa_runtime/runtime/presentation.h"

#include <stdio.h>
#include <string.h>

enum {
	XWA_MOVIE_OVERLAY_WIDTH = 640,
	XWA_MOVIE_OVERLAY_HEIGHT = 480,
	XWA_MOVIE_OVERLAY_PITCH = XWA_MOVIE_OVERLAY_WIDTH * 2,
	XWA_MOVIE_SKIP_GATE_FRAME = 5,
	XWA_MOVIE_SUBTITLE_FPS = 15,
};

typedef struct XwaMovieTaskState {
	AeronVideoPlayer* player;
	char path[512];
	AeronVfsRoot root;
	int is_replacement;
	uint16_t* subtitle_overlay;
	uint64_t subtitle_generation;
	AeronRenderSubmission subtitle_submission;
	int active;
	int complete;
	int result;
	int no_fade;
	int playback_paused;
} XwaMovieTaskState;

static XwaMovieTaskState g_xwaMovieTask;
static int g_xwaMoviePreferOriginal;

void XwaMovieTask_Configure(int preferOriginalMovies) {
	g_xwaMoviePreferOriginal = preferOriginalMovies != 0;
}

/* Finds the movie `name`: a replacement in the mod folder,
 * frontend/movies/<name>.mp4, or the original MOVIES/<name>.SNM. The
 * replacement comes first unless assets.prefer_original_movies is set. */
static int XwaMovieTask_ResolveAsset(const char* name) {
	char original[512];
	char replacement[512];
	int has_original;
	int has_replacement;

	if (snprintf(original, sizeof(original), "MOVIES/%s.SNM", name) >= (int)sizeof(original) ||
		snprintf(replacement, sizeof(replacement), "frontend/movies/%s.mp4", name) >=
			(int)sizeof(replacement)) {
		return 0;
	}
	has_original = AeronVfs_Exists(Aeron_GetVfs(), AERON_VFS_ROOT_ASSET, original);
	has_replacement = AeronVfs_Exists(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, replacement);
	g_xwaMovieTask.is_replacement = has_replacement && (!g_xwaMoviePreferOriginal || !has_original);
	if (g_xwaMovieTask.is_replacement) {
		g_xwaMovieTask.root = AERON_VFS_ROOT_MOD;
		memcpy(g_xwaMovieTask.path, replacement, sizeof(replacement));
		return 1;
	}
	if (!has_original) {
		return 0;
	}
	g_xwaMovieTask.root = AERON_VFS_ROOT_ASSET;
	memcpy(g_xwaMovieTask.path, original, sizeof(original));
	return 1;
}

static void XwaMovieTask_RestoreFrontend(void) {
	if (!g_xwaMovieTask.no_fade) {
		FrontendDisplay_ClearBackBuffer();
		FrontendDisplay_ClearOffscreenSurface();
		FrontendDisplay_PresentFrame();
		FrontendDisplay_ClearBackBuffer();
	}
	g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	FrontendDisplay_EnableOffscreenRestore();
	Movie_FreeSubtitles();
}

static void XwaMovieTask_Finish(int result, int preserve_submission) {
	if (!preserve_submission && g_xwaMovieTask.player != NULL) {
		Aeron_VideoClose(g_xwaMovieTask.player);
		g_xwaMovieTask.player = NULL;
	}
	XwaMovieTask_RestoreFrontend();
	g_xwaMovieTask.result = result;
	g_xwaMovieTask.active = 0;
	g_xwaMovieTask.complete = 1;
}

/* The subtitles next to the movie that plays: MOVIES/<name>.sub for an
 * original, frontend/movies/<name>.sub in the mod folder for a replacement.
 * A replacement never takes the original's subtitles, which are timed for the
 * original and may repeat text that the replacement shows in its picture. */
static int XwaMovieTask_LoadSubtitleTrack(void) {
	return Movie_LoadSubtitles(g_xwaMovieTask.root, g_xwaMovieTask.path);
}

int XwaMovieTask_Begin(const char* name, int noFade) {
	AeronVideoOpenDesc desc;

	if (name == NULL || name[0] == '\0' || g_xwaMovieTask.active) {
		return 0;
	}
	XwaMovieTask_ReapFinished();
	g_movieSkipRequested = 0;
	g_movieFrameNumber = 0;
	g_xwaMovieTask.complete = 0;
	g_xwaMovieTask.result = 0;
	g_xwaMovieTask.playback_paused = 0;
	if (!XwaMovieTask_ResolveAsset(name)) {
		Aeron_LogError("xwa.movie", "Movie asset '%s' was not found", name);
		return 0;
	}

	if (g_xwaMovieTask.subtitle_overlay == NULL) {
		g_xwaMovieTask.subtitle_overlay =
			(uint16_t*)Mem_Alloc(XWA_MOVIE_OVERLAY_PITCH * XWA_MOVIE_OVERLAY_HEIGHT);
		if (g_xwaMovieTask.subtitle_overlay == NULL) {
			return 0;
		}
	}
	(void)XwaMovieTask_LoadSubtitleTrack();
	g_xwaMovieTask.no_fade = noFade != 0;
	if (!g_xwaMovieTask.no_fade) {
		/* The modern surface bridge presents a cleared back buffer as the
		 * original front-surface black transition. */
		FrontendDisplay_ClearBackBuffer();
		FrontendDisplay_PresentFrame();
		FrontendDisplay_ClearBackBuffer();
	}
	FrontendDisplay_DisableOffscreenRestore();
	FrontendDisplay_UnlockBackBuffer();
	FrontendText_ResetGlyphScratch();

	memset(&desc, 0, sizeof(desc));
	desc.vfs = Aeron_GetVfs();
	desc.root = g_xwaMovieTask.root;
	desc.path = g_xwaMovieTask.path;
	desc.autoplay = 1;
	desc.gain = (float)g_gameConfig.sfxDatapadVolume / 10.0f;
	g_xwaMovieTask.player = Aeron_VideoOpen(&desc);
	if (g_xwaMovieTask.player == NULL) {
		XwaMovieTask_RestoreFrontend();
		return 0;
	}
	g_xwaMovieTask.active = 1;
	Aeron_LogInfo("xwa.movie", "Playing '%s' from %s (%s)", name, g_xwaMovieTask.path,
				  g_xwaMovieTask.is_replacement ? "mod" : "original");
	return 1;
}

void XwaMovieTask_ReapFinished(void) {
	if (!g_xwaMovieTask.active && g_xwaMovieTask.player != NULL) {
		Aeron_VideoClose(g_xwaMovieTask.player);
		g_xwaMovieTask.player = NULL;
	}
}

/* The frame number that subtitles and the skip guard count in. SUB files
 * count frames at the original movies' 15 fps. An original counts its own
 * frames; a replacement, whose frame rate may differ, counts its playback
 * position at 15 fps. */
static int XwaMovieTask_CurrentFrameNumber(void) {
	int64_t position_us;

	if (!g_xwaMovieTask.is_replacement) {
		return (int)Aeron_VideoGetPresentedFrameIndex(g_xwaMovieTask.player);
	}
	position_us = Aeron_VideoGetPositionUs(g_xwaMovieTask.player);
	return position_us > 0 ? (int)(position_us * XWA_MOVIE_SUBTITLE_FPS / 1000000) : 0;
}

static int XwaMovieTask_LocalSkipRequested(void) {
	const AeronInputSnapshot* input;

	if (g_movieFrameNumber <= XWA_MOVIE_SKIP_GATE_FRAME) {
		Keyboard_FlushCharBuffer();
		FrontendMouse_ClearClicks();
		return 0;
	}
	input = Aeron_InputSnapshot();
	if ((input != NULL &&
		 (input->key_pressed[AERON_KEY_ESCAPE] || input->key_pressed[AERON_KEY_SPACE] ||
		  input->key_pressed[AERON_KEY_RETURN] || input->key_pressed[AERON_KEY_BACKSPACE] ||
		  (input->mouse.released_buttons & (AERON_MOUSE_BUTTON_LEFT | AERON_MOUSE_BUTTON_RIGHT)) != 0)) ||
		FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick() || Keyboard_BufferContains(27) ||
		Keyboard_BufferContains(32) || Keyboard_BufferContains(13) || Keyboard_BufferContains(8)) {
		Keyboard_FlushCharBuffer();
		FrontendMouse_ClearClicks();
		return 1;
	}
	return 0;
}

static void XwaMovieTask_SubmitSubtitles(void) {
	AeronPixelLayerDesc layer;
	unsigned char* saved_draw_surface;
	int saved_pitch;
	FrontendRect saved_clip;
	XwaPresentationRect safe;

	g_xwaMovieTask.subtitle_submission = 0;
	if (g_movieSubtitles == NULL || g_movieSubtitleCurrentIndex >= g_movieSubtitleCount) {
		return;
	}
	memset(g_xwaMovieTask.subtitle_overlay, 0, XWA_MOVIE_OVERLAY_PITCH * XWA_MOVIE_OVERLAY_HEIGHT);
	saved_draw_surface = g_drawSurfacePtr;
	saved_pitch = g_drawSurfacePitch;
	FrontendDisplay_GetScreenClipRect(&saved_clip);
	FrontendDisplay_ResetScreenClipRect();
	g_drawSurfacePtr = (unsigned char*)g_xwaMovieTask.subtitle_overlay;
	g_drawSurfacePitch = XWA_MOVIE_OVERLAY_PITCH;
	(void)Movie_DrawSubtitlesForCurrentFrame();
	g_drawSurfacePtr = saved_draw_surface;
	g_drawSurfacePitch = saved_pitch;
	FrontendDisplay_SetScreenClipRect640x480(&saved_clip);

	safe = XwaPresentation_ClassicSafeFrame();
	memset(&layer, 0, sizeof(layer));
	layer.frame.pixels = g_xwaMovieTask.subtitle_overlay;
	layer.frame.width = XWA_MOVIE_OVERLAY_WIDTH;
	layer.frame.height = XWA_MOVIE_OVERLAY_HEIGHT;
	layer.frame.pitch = XWA_MOVIE_OVERLAY_PITCH;
	layer.frame.format = FrontendDisplay_GetPixelFormat();
	layer.frame.color_space = AERON_COLOR_SPACE_SRGB;
	layer.frame.generation = ++g_xwaMovieTask.subtitle_generation;
	layer.logical_rect = (AeronRectI) { safe.x, safe.y, safe.width, safe.height };
	layer.blend_mode = AERON_LAYER_BLEND_ALPHA;
	layer.sampling = AERON_PIXEL_SAMPLING_SHARP_BILINEAR;
	layer.preserve_encoded_values = 1;
	layer.color_key_enabled = 1;
	layer.color_key = 0;
	g_xwaMovieTask.subtitle_submission = Aeron_SubmitPixelLayer(&layer);
	if (!g_xwaMovieTask.subtitle_submission) {
		Aeron_RequestFatalRendererError("movie subtitle presentation");
	}
}

static int XwaMovieTask_SubmitCurrent(void) {
	AeronVideoPresentDesc present;
	XwaPresentationRect frame;

	frame = XwaPresentation_Frame();
	memset(&present, 0, sizeof(present));
	present.bounds = (AeronRectI) { frame.x, frame.y, frame.width, frame.height };
	present.scale_mode = AERON_VIDEO_SCALE_CONTAIN;
	present.blend_mode = AERON_LAYER_BLEND_OPAQUE;
	if (!Aeron_VideoSubmit(g_xwaMovieTask.player, &present)) {
		return 0;
	}
	XwaMovieTask_SubmitSubtitles();
	return 1;
}

void XwaMovieTask_Tick(void) {
	const AeronInputSnapshot* input;
	AeronVideoState state;

	if (!g_xwaMovieTask.active || g_xwaMovieTask.player == NULL) {
		return;
	}
	FrontendFileStream_ServiceSlots();
	XwaFrontendTask_ServiceFrameSystems();
	input = Aeron_InputSnapshot();
	if (input != NULL && !input->has_focus && XwaPort_EverHadFocus()) {
		if (!g_xwaMovieTask.playback_paused) {
			Aeron_VideoPause(g_xwaMovieTask.player);
			g_xwaMovieTask.playback_paused = 1;
		}
	} else if (g_xwaMovieTask.playback_paused) {
		Aeron_VideoPlay(g_xwaMovieTask.player);
		g_xwaMovieTask.playback_paused = 0;
	}

	Aeron_VideoUpdate(g_xwaMovieTask.player);
	g_movieFrameNumber = XwaMovieTask_CurrentFrameNumber();

	if (FrontendDialog_HasNetworkDismissPacket() || XwaMovieTask_LocalSkipRequested()) {
		XwaMovieTask_Stop(1);
	}

	state = Aeron_VideoGetState(g_xwaMovieTask.player);
	if (state == AERON_VIDEO_ERROR) {
		Aeron_LogError("xwa.movie", "%s", Aeron_VideoGetError(g_xwaMovieTask.player));
		XwaMovieTask_Finish(0, 0);
		return;
	}
	if (state == AERON_VIDEO_ENDED) {
		const int submitted = g_xwaMovieTask.no_fade ? XwaMovieTask_SubmitCurrent() : 0;
		XwaMovieTask_Finish(1, submitted);
		return;
	}
	(void)XwaMovieTask_SubmitCurrent();
}

void XwaMovieTask_PausedFrame(void) {
	if (!g_xwaMovieTask.active || g_xwaMovieTask.player == NULL) {
		return;
	}
	if (!g_xwaMovieTask.playback_paused) {
		Aeron_VideoPause(g_xwaMovieTask.player);
		g_xwaMovieTask.playback_paused = 1;
	}
	(void)XwaMovieTask_SubmitCurrent();
}

void XwaMovieTask_SuppressClassicSubtitles(void) {
	Aeron_CancelRenderSubmission(g_xwaMovieTask.subtitle_submission);
	g_xwaMovieTask.subtitle_submission = 0;
}

void XwaMovieTask_Stop(int skipped) {
	if (!g_xwaMovieTask.active || g_xwaMovieTask.player == NULL) {
		return;
	}
	if (skipped) {
		g_movieSkipRequested = 1;
	}
	Aeron_VideoStop(g_xwaMovieTask.player);
}

int XwaMovieTask_IsActive(void) { return g_xwaMovieTask.active; }

int XwaMovieTask_IsComplete(void) { return g_xwaMovieTask.complete; }

int XwaMovieTask_GetResult(void) { return g_xwaMovieTask.result; }

uint64_t XwaMovieTask_NextWakeDelayUs(void) {
	uint64_t delayUs;

	if (!g_xwaMovieTask.active || g_xwaMovieTask.player == NULL) {
		return 0;
	}
	return Aeron_VideoGetNextWakeDelayUs(g_xwaMovieTask.player, &delayUs) ? delayUs : 10000u;
}

void XwaMovieTask_Shutdown(void) {
	if (g_xwaMovieTask.player != NULL) {
		Aeron_VideoClose(g_xwaMovieTask.player);
		g_xwaMovieTask.player = NULL;
	}
	Movie_FreeSubtitles();
	if (g_xwaMovieTask.subtitle_overlay != NULL) {
		Mem_Free(g_xwaMovieTask.subtitle_overlay);
		g_xwaMovieTask.subtitle_overlay = NULL;
	}
	memset(&g_xwaMovieTask, 0, sizeof(g_xwaMovieTask));
}
