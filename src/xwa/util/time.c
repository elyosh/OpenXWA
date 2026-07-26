#include "xwa/util/time.h"

// GLOBAL: XWA 0x8B94D4
int g_gameTime;
// GLOBAL: XWA 0x781E64
uint32_t g_lastTickTime;
// GLOBAL: XWA 0x77D028
uint32_t g_simStepLastTickTime;

// FUNCTION: XWA 0x50E400
void Time_ResetFrameDeltaClocks(void) {
	g_lastTickTime = 0;
	g_simStepLastTickTime = 0;
}

// FUNCTION: XWA 0x50E410
uint32_t Time_GetFrameDelta(void) {
	uint32_t time;
	uint32_t lastTickTime;
	uint32_t deltaTicks;

	time = timeGetTime();
	lastTickTime = g_lastTickTime;
	if (lastTickTime == 0) {
		lastTickTime = time;
	}
	deltaTicks = (time - lastTickTime) >> 2;
	g_lastTickTime = lastTickTime + 4 * deltaTicks;
	return deltaTicks;
}

// FUNCTION: XWA 0x50E430
uint32_t Time_GetSimStepDelta(void) {
	uint32_t time;
	uint32_t lastTickTime;
	uint32_t deltaTicks;

	time = timeGetTime();
	lastTickTime = g_simStepLastTickTime;
	if (lastTickTime == 0) {
		lastTickTime = time;
	}
	deltaTicks = (time - lastTickTime) >> 2;
	g_simStepLastTickTime = lastTickTime + 4 * deltaTicks;
	return deltaTicks;
}
