#ifndef XWA_RUNTIME_FLIGHT_TASK_H
#define XWA_RUNTIME_FLIGHT_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
