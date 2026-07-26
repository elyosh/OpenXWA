#include "xwa_runtime/timing/host_clock.h"

#include "xwa/util/time.h"

static uint64_t g_xwaElapsedUs;

void XwaTime_Reset(void) {
	g_xwaElapsedUs = 0;
	g_gameTime = 0;
}

void XwaTime_AdvanceHostClock(int32_t delta_us) {
	if (delta_us > 0) {
		g_xwaElapsedUs += (uint32_t)delta_us;
	}
}

uint64_t XwaTime_GetElapsedUs(void) { return g_xwaElapsedUs; }

uint32_t XwaTime_GetElapsedTicks(void) { return (uint32_t)(g_xwaElapsedUs / 1000u); }

uint32_t timeGetTime(void) { return XwaTime_GetElapsedTicks(); }

uint32_t GetTickCount(void) { return XwaTime_GetElapsedTicks(); }
