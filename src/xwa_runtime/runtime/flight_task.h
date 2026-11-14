#ifndef XWA_RUNTIME_FLIGHT_TASK_H
#define XWA_RUNTIME_FLIGHT_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OpenXWA's flight screen. Flight always uses the 1600x1200 slot of the
   resolution table, changed to a 1920x1080 screen with a HUD scale of 1.44
   and a projection of 1152 (1080 x 1.0666, which keeps the vertical field of
   view of the stock modes). The modern renderer takes only the HUD layout,
   the text sizes and the cockpit view offset from this screen, so they are
   the same for every player. See docs/hud_scale_plan.md.

   The CMD view's camera distance fits the target to the HUD, so the CMD
   needs a projection that matches the HUD scale. In the stock modes the
   projection is about 640 times the HUD scale, but 1152 is 800 times 1.44,
   which makes the target 25% too large. So the remaster draws the CMD with
   XWA_FLIGHT_SCREEN_CMD_PROJ_SCALE, the projection of the stock 1152x864
   mode, which also has a HUD scale of 1.44. */
enum {
	XWA_FLIGHT_SCREEN_WIDTH = 1920,
	XWA_FLIGHT_SCREEN_HEIGHT = 1080,
	XWA_FLIGHT_SCREEN_PROJ_SCALE = 1152,
	XWA_FLIGHT_SCREEN_CMD_PROJ_SCALE = 922,
};
#define XWA_FLIGHT_SCREEN_HUD_SCALE 1.44f

int XwaFlightTask_Init(char* missionCmdLine, const char* filmFilePath);
void XwaFlightTask_Tick(void);
int XwaFlightTask_Shutdown(void);
int XwaFlightTask_IsActive(void);
int XwaFlightTask_IsComplete(void);
/* True while the loading screen is kept visible after loading ends, to meet
 * the minimum loading-screen time. The task runs no frames during this time,
 * and presentation keeps showing the last loading image. */
int XwaFlightTask_IsHoldingLoadingScreen(void);
int XwaFlightTask_GetResult(void);
uint64_t XwaFlightTask_NextWakeDelayUs(void);
/* The number of flight frames that the task ran in this session. */
uint32_t XwaFlightTask_FramesRun(void);
/* Runs the next frame no sooner than `delayUs` from now. Film playback uses
 * it to play the recorded steps at their real speed. */
void XwaFlightTask_DelayNextFrame(uint64_t delayUs);
/* Runs the next flight frame of a playing film now, without waiting for its
   wake time. Returns 0, and runs nothing, outside a film's flight frames: a
   pause, a modal, the end of the mission. */
int XwaFlightTask_RunFrameNow(void);
/* After frames run with XwaFlightTask_RunFrameNow, puts the game time and the
   next wake time back in line with the film, as unpausing a film does. */
void XwaFlightTask_EndFilmFastForward(void);

#ifdef __cplusplus
}
#endif

#endif
