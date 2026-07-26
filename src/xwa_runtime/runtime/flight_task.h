#ifndef XWA_RUNTIME_FLIGHT_TASK_H
#define XWA_RUNTIME_FLIGHT_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int      XwaFlightTask_Init(char* missionCmdLine, const char* filmFilePath);
void     XwaFlightTask_Tick(void);
int      XwaFlightTask_Shutdown(void);
int      XwaFlightTask_IsActive(void);
int      XwaFlightTask_IsComplete(void);
int      XwaFlightTask_GetResult(void);
uint64_t XwaFlightTask_NextWakeDeadlineUs(void);

#ifdef __cplusplus
}
#endif

#endif
