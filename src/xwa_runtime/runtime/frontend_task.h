#ifndef XWA_RUNTIME_FRONTEND_TASK_H
#define XWA_RUNTIME_FRONTEND_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int XwaFrontendTask_Init(void);
void XwaFrontendTask_Tick(void);
void XwaFrontendTask_ServiceFrameSystems(void);
void XwaFrontendTask_Pause(void);
int XwaFrontendTask_ShouldQuit(void);
uint64_t XwaFrontendTask_NextWakeDeadlineUs(void);

#ifdef __cplusplus
}
#endif

#endif
