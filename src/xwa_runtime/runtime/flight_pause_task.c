#include "xwa_runtime/runtime/flight_pause_task.h"

#include "xwa/audio/fsfx.h"
#include "xwa/audio/music.h"
#include "xwa/audio/sound.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_text.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/input/dinput.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/time.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/input/flight_controls.h"
#include "xwa_runtime/input/mouse_flight.h"

typedef enum XwaFlightPausePhase {
	XWA_FLIGHT_PAUSE_INACTIVE,
	XWA_FLIGHT_PAUSE_REQUESTED,
	XWA_FLIGHT_PAUSE_WAITING,
} XwaFlightPausePhase;

static XwaFlightPausePhase g_flightPausePhase;
static unsigned int g_flightPausePlayerIdx;

void XwaFlightPauseTask_Request(unsigned int playerIdx) {
	if (g_flightPausePhase != XWA_FLIGHT_PAUSE_INACTIVE || g_flightPlayerCount != 1) {
		return;
	}
	if (g_filmPlaybackMode || g_filmRecording) {
		g_actionKey = KEY_NONE;
		if (g_filmRecording) {
			msg_emitInFlightMessage(MSG_FILM_NO_PAUSE, (int)playerIdx);
			fsfx_PlaySound(63, 0xffffu, playerIdx);
		}
		return;
	}

	/* Finish the current simulation frame before taking ownership. No input or
	 * render callback can then observe a partially updated world during pause. */
	g_flightPausePlayerIdx = playerIdx;
	g_flightPausePhase = XWA_FLIGHT_PAUSE_REQUESTED;
	g_actionKey = KEY_NONE;
}

static void XwaFlightPauseTask_Begin(void) {
	g_inputTimestamp += Time_GetFrameDelta();
	fsfx_PlaySound(68, 0xffffu, g_flightPausePlayerIdx);
	msg_emitInFlightMessage(MSG_MISSION_PAUSED, (int)g_flightPausePlayerIdx);
	if (g_useHardware3D) {
		RenderScene_Initialize(1);
		FlightText_FlushQueue();
		RenderScene_DrawVisibleFaces();
	} else {
		FlightSw_SetRenderTarget(NULL, 0, 0, 0);
		FlightSurface_Lock();
		FlightText_SetClipRect(0, 0, g_screenWidth, g_screenHeight);
		g_flightFillClipRectFn();
		FlightSurface_Unlock();
		Hud_BlitSoftwareHudTextPanes();
	}
	FlightDisplay_Flip();
	Sound_StopAllInstances();
	Music_PauseIfInitialized();
	XwaFlightControls_Recover();
	g_flightPausePhase = XWA_FLIGHT_PAUSE_WAITING;
}

int XwaFlightPauseTask_Tick(void) {
	if (g_flightPausePhase == XWA_FLIGHT_PAUSE_INACTIVE) {
		return 0;
	}
	if (g_flightPausePhase == XWA_FLIGHT_PAUSE_REQUESTED) {
		XwaFlightPauseTask_Begin();
		return 1;
	}

	/* Host time and relative mouse motion continue arriving while simulation
	 * is suspended. Discard them, including on the resume frame. */
	Time_GetFrameDelta();
	XwaMouseFlight_Suspend();
	if (g_flightConfDirectInput) {
		DInput_PollMouseState();
	}
	/* Any key resumes, and so does a controller pause binding. */
	if ((FlightInput_HasKeyReady() && FlightInput_GetNextKey() != 0) || XwaControllerMapping_TakeResume()) {
		XwaFlightControls_Recover();
		Music_ResumeIfInitialized();
		msg_emitInFlightMessage(MSG_MISSION_RESUMED, (int)g_flightPausePlayerIdx);
		g_actionKey = KEY_NONE;
		sub_4D4640();
		g_flightPausePhase = XWA_FLIGHT_PAUSE_INACTIVE;
	}
	return 1;
}

int XwaFlightPauseTask_IsActive(void) { return g_flightPausePhase != XWA_FLIGHT_PAUSE_INACTIVE; }

void XwaFlightPauseTask_Shutdown(void) {
	if (g_flightPausePhase == XWA_FLIGHT_PAUSE_WAITING) {
		Music_ResumeIfInitialized();
	}
	g_flightPausePhase = XWA_FLIGHT_PAUSE_INACTIVE;
}
