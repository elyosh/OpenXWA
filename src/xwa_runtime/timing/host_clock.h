#ifndef XWA_RUNTIME_HOST_CLOCK_H
#define XWA_RUNTIME_HOST_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void XwaTime_Reset(void);
void XwaTime_AdvanceHostClock(int32_t delta_us);
uint64_t XwaTime_GetElapsedUs(void);
/* Returns host elapsed milliseconds, matching the unit of Win32 GetTickCount. */
uint32_t XwaTime_GetElapsedTicks(void);

#ifdef __cplusplus
}
#endif

#endif
