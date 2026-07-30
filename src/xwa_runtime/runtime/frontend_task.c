#include "xwa_runtime/runtime/frontend_task.h"

#include "aeron/aeron.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/concourse.h"
#include "xwa/frontend/frontend_bootstrap.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_file_stream.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/net_transport.h"
#include "xwa/util/memory.h"
#include "xwa/xwa_options.h"

#include <string.h>

enum {
	XWA_FRONTEND_FPS = 24,
	XWA_FRONTEND_BPP = 16,
};

static uint64_t g_xwaFrontendNextFrameUs;
static int g_xwaFrontendShouldQuit;

/* Installs the callback state that original FrontendDisplay_Init @ 0x53ED60 wrote into
   g_screenStates[0] before entering FrontendDisplay_RunMainLoop. */
static void XwaFrontendTask_SetInitialCallbacks(FrontendScreenUpdateFn updateFn,
												FrontendScreenExitFn exitFn) {
	memset(g_screenStates, 0, sizeof(g_screenStates));
	g_screenStackTop = 0;
	g_screenCallbacksDirty = 0;
	g_pendingScreenUpdateFn = 0;
	g_modalScreenActive = 0;
	g_modalScreenDepth = 0;
	g_modalScreenStatus = FRONTEND_SCREEN_MODAL_INACTIVE;
	g_screenStates[0].updateFn = updateFn;
	g_screenStates[0].exitFn = exitFn;
	g_frameCounter = 0;
}

/* TODO: Port the rest of FrontendDisplay_Shutdown @ 0x53E160 (fonts, screen-stack images,
   resource tables, string table, surface/device release). */
void XwaFrontendTask_Shutdown(void) { FrontendSound_ShutdownDirectSound(); }

/* Tick-model replacement for the non-returning original FrontendDisplay_Init @ 0x53ED60
   entry into FrontendDisplay_RunMainLoop @ 0x53E760. */
int XwaFrontendTask_Init(void) {
	FrontendScreenUpdateFn updateFn;
	FrontendScreenExitFn exitFn;
	int (*modeInitFn)(void);

	/* Allocate the frontend sound-buffer/voice tables (original FrontendDisplay_Init @ 0x53ED60) and
	   bring up the DirectSound->Aeron device (original FrontendSound_InitDirectSound @ 0x538100, invoked
	   from FrontendDisplay_InitMainWindow during boot). Allocation sizes use sizeof so the wider 64-bit
	   pointer in each record is accounted for instead of the original 32-bit byte counts. */
	if (g_frontendSoundBuffers == NULL) {
		g_frontendSoundBuffers =
			(FrontendSoundBufferRecord*)Mem_Alloc(128 * sizeof(FrontendSoundBufferRecord));
	}
	if (g_frontendSoundVoices == NULL) {
		g_frontendSoundVoices =
			(FrontendSoundVoice*)Mem_Alloc(FRONTEND_SOUND_VOICE_COUNT * sizeof(FrontendSoundVoice));
	}
	if (g_frontendSoundBuffers != NULL && g_frontendSoundVoices != NULL) {
		FrontendSound_InitDirectSound(0);
	}

	/* TODO: Recover remaining FrontendDisplay_Init startup side effects that are not yet covered by
	   Aeron/static storage replacements. */
	Config_Load();

	if (g_optSkipIntro || g_optIsHost || g_optIsClient) {
		updateFn = Concourse_Update;
		exitFn = Concourse_Exit;
		modeInitFn = Frontend_LoadResources;
	} else {
		updateFn = FrontendBootstrap_RunIntroAndEnterConcourse;
		exitFn = 0;
		modeInitFn = FrontendBootstrap_InitMode;
	}

	g_displayBpp = XWA_FRONTEND_BPP;
	FrontendDisplay_SetFrameRate(XWA_FRONTEND_FPS);
	/* Bring up the DirectDraw device before the surfaces. The original does this in
	   FrontendDisplay_InitMainWindow @0x53EB30 (DirectDrawCreate -> g_directDrawPrimary,
	   then g_directDraw = g_directDrawPrimary); its Win32 window creation is owned by
	   Aeron in the port, so only the device bring-up is reproduced here. */
	if (DirectDrawCreate_Compat(NULL, &g_directDrawPrimary, NULL)) {
		XwaFrontendTask_Shutdown();
		return 0;
	}
	g_directDraw = g_directDrawPrimary;
	/* Enumerate WinMM joysticks (original FrontendDisplay_InitMainWindow also calls this). */
	Joystick_InitDevices();
	if (!FrontendDisplay_InitSurfaces()) {
		XwaFrontendTask_Shutdown();
		return 0;
	}
	if (FrontendText_LoadFont(20) != 1) {
		FrontendDisplay_FreeSurfaces();
		XwaFrontendTask_Shutdown();
		return 0;
	}
	FrontendCursor_Init();
	FrontendCursor_HideOsCursor();

	XwaFrontendTask_SetInitialCallbacks(updateFn, exitFn);
	if (modeInitFn != 0 && modeInitFn()) {
		FrontendDisplay_FreeSurfaces();
		XwaFrontendTask_Shutdown();
		return 0;
	}

	g_xwaFrontendShouldQuit = 0;
	g_xwaFrontendNextFrameUs = Aeron_NowUs();
	return 1;
}

/* XwaPort-level replacement for the inactive g_appActive branch of original
   FrontendDisplay_RunMainLoop @ 0x53E760. Aeron already pumped events; keep the
   latest submitted frontend frame visible and prevent deadline catch-up. */
void XwaFrontendTask_Pause(void) {
	/* Aeron retains the last presented frame when a tick submits nothing, so the
	   paused (unfocused) frontend stays on screen with no re-present needed. */
	g_xwaFrontendNextFrameUs = Aeron_NowUs();
}

void XwaFrontendTask_ServiceFrameSystems(void) {
	Joystick_UpdateState(0);
	Joystick_UpdateState(1);
	Net_PumpIncomingPackets();
	Music_Update();
}

/* Tick-model replacement for the active loop body of original
   FrontendDisplay_RunMainLoop @ 0x53E760. */
void XwaFrontendTask_Tick(void) {
	FrontendScreenModalStatus modalStatus;
	int result;

	if (g_xwaFrontendShouldQuit) {
		return;
	}

	FrontendFileStream_ServiceSlots();
	if (Aeron_NowUs() < g_xwaFrontendNextFrameUs) {
		return;
	}

	g_xwaFrontendNextFrameUs = Aeron_NowUs() + (uint64_t)g_frameIntervalMs * 1000u;
	XwaFrontendTask_ServiceFrameSystems();
	/* Original RunMainLoop also handled CDAudio resume/track-end work here. CD audio is intentionally
	   not ported because it is not used by the original game frontend flow. */

	if (FrontendScreen_IsModalActive()) {
		modalStatus = FrontendScreen_TickModal();
		if (modalStatus == FRONTEND_SCREEN_MODAL_DONE) {
			FrontendScreen_EndModal();
		} else if (modalStatus == FRONTEND_SCREEN_MODAL_QUIT || modalStatus == FRONTEND_SCREEN_MODAL_FAILED) {
			g_xwaFrontendShouldQuit = 1;
		}
	} else {
		result = FrontendScreen_RunFrame();
		if (result == 1 || result == 2) {
			g_xwaFrontendShouldQuit = 1;
		}
	}

	/* Draw-then-present, matching the original RunMainLoop @0x53E760: the frame
	   update renders into the back buffer, then PresentFrame composites and
	   submits it via the DirectDraw shim. */
	FrontendFileStream_ServiceSlots();
	FrontendDisplay_PresentFrame();
	FrontendDisplay_ClearPresentFrameReady();
}

int XwaFrontendTask_ShouldQuit(void) { return g_xwaFrontendShouldQuit; }

/* Replaces the original GetTickCount/g_frameIntervalMs throttle in
   FrontendDisplay_RunMainLoop @ 0x53E760 with an Aeron wake deadline. */
uint64_t XwaFrontendTask_NextWakeDeadlineUs(void) { return g_xwaFrontendNextFrameUs; }
