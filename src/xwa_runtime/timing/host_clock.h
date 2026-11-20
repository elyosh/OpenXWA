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
uint64_t XwaTime_GetLegacyTimerIntervalUs(uint32_t intervalMs);
/* The clock that seeds the game's cosmetic random numbers (Math_SeedRandom).
   It is the real clock, except for a film played with --play-film: it is
   then the film's time, so that every run shows the same frames. */
uint32_t XwaTime_RandomSeedClock(void);

#ifdef __cplusplus
}
#endif

#endif
