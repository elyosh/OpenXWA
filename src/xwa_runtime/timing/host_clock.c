#include "xwa_runtime/timing/host_clock.h"

#include "xwa/flight/flight.h"
#include "xwa/util/time.h"
#include "xwa_runtime/runtime/film_benchmark.h"

enum { XWA_LEGACY_TIMER_QUANTUM_US = 15625 };

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

uint64_t XwaTime_GetLegacyTimerIntervalUs(uint32_t intervalMs) {
	uint64_t requestedUs;

	requestedUs = (uint64_t)intervalMs * 1000u;
	/* Preserve the coarse GetTickCount deadline observed by the original while
	   allowing the host scheduler to use Aeron's precise clock. */
	return ((requestedUs + XWA_LEGACY_TIMER_QUANTUM_US - 1u) / XWA_LEGACY_TIMER_QUANTUM_US) *
		   XWA_LEGACY_TIMER_QUANTUM_US;
}

uint32_t XwaTime_RandomSeedClock(void) {
	return XwaFilmBenchmark_IsPlaying() ? (uint32_t)g_filmStepInputTimestamp : XwaTime_GetElapsedTicks();
}

uint32_t timeGetTime(void) { return XwaTime_GetElapsedTicks(); }

uint32_t GetTickCount(void) { return XwaTime_GetElapsedTicks(); }
