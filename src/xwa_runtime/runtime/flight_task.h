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

#ifdef __cplusplus
}
#endif

#endif
