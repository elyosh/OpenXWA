#include "xwa_runtime/timing/modern_flight_timing.h"

#include "aeron/log.h"

#include <stdint.h>

enum {
	XWA_MODERN_UNLOCKED_STEP_TICKS = 1,
	XWA_MODERN_HANGAR_STEP_TICKS = 4,
	XWA_MODERN_COMPATIBILITY_STEP_TICKS = 8,
};

static int g_requestedStepTicks = XWA_MODERN_UNLOCKED_STEP_TICKS;
static int g_effectiveStepTicks = XWA_MODERN_COMPATIBILITY_STEP_TICKS;
static XwaModernFlightTimingReason g_timingReason = XWA_MODERN_TIMING_CONFIGURED;
static uint16_t g_pendingLegacyCadenceTicks;
static int g_legacyCadenceDue;
static uint16_t g_pendingAiTicks;
static uint16_t g_pendingTransientAnimationTicks;
static uint16_t g_pendingGlowMarkAnimationTicks;

static const char* XwaModernFlightTiming_ReasonName(XwaModernFlightTimingReason reason) {
	switch (reason) {
		case XWA_MODERN_TIMING_MULTIPLAYER:
			return "multiplayer";
		case XWA_MODERN_TIMING_CONFIGURED:
		default:
			return "configured";
	}
}

static uint16_t XwaModernFlightTiming_SaturatingAdd(uint16_t accumulated, uint16_t elapsed_ticks) {
	const uint32_t sum = (uint32_t)accumulated + elapsed_ticks;
	return sum > UINT16_MAX ? UINT16_MAX : (uint16_t)sum;
}

static void XwaModernFlightTiming_ResetAccumulators(void) {
	g_pendingLegacyCadenceTicks = 0;
	g_legacyCadenceDue = 0;
	g_pendingAiTicks = 0;
	g_pendingTransientAnimationTicks = 0;
	g_pendingGlowMarkAnimationTicks = 0;
}

void XwaModernFlightTiming_Configure(int requested_step_ticks) {
	g_requestedStepTicks = requested_step_ticks;
}

void XwaModernFlightTiming_BeginSession(int player_count) {
	XwaModernFlightTiming_ResetAccumulators();

	if (player_count != 1) {
		g_effectiveStepTicks = XWA_MODERN_COMPATIBILITY_STEP_TICKS;
		g_timingReason = XWA_MODERN_TIMING_MULTIPLAYER;
	} else {
		g_effectiveStepTicks = g_requestedStepTicks;
		g_timingReason = XWA_MODERN_TIMING_CONFIGURED;
	}

	Aeron_Log("xwa.flight.timing", "session requested=%d effective=%d reason=%s", g_requestedStepTicks,
			  g_effectiveStepTicks, XwaModernFlightTiming_ReasonName(g_timingReason));
}

void XwaModernFlightTiming_EndSession(void) {
	g_effectiveStepTicks = XWA_MODERN_COMPATIBILITY_STEP_TICKS;
	g_timingReason = XWA_MODERN_TIMING_CONFIGURED;
	XwaModernFlightTiming_ResetAccumulators();
}

int XwaModernFlightTiming_StepTicks(void) { return g_effectiveStepTicks; }

int XwaModernFlightTiming_HangarStepTicks(void) {
	return XwaModernFlightTiming_IsHighRate() ? XWA_MODERN_HANGAR_STEP_TICKS : g_effectiveStepTicks;
}

int XwaModernFlightTiming_IsHighRate(void) {
	return g_effectiveStepTicks < XWA_MODERN_COMPATIBILITY_STEP_TICKS;
}

void XwaModernFlightTiming_BeginAdvance(uint16_t elapsed_ticks) {
	if (!XwaModernFlightTiming_IsHighRate()) {
		g_pendingLegacyCadenceTicks = 0;
		g_legacyCadenceDue = 1;
		return;
	}

	g_pendingLegacyCadenceTicks =
		XwaModernFlightTiming_SaturatingAdd(g_pendingLegacyCadenceTicks, elapsed_ticks);
	if (g_pendingLegacyCadenceTicks >= XWA_MODERN_COMPATIBILITY_STEP_TICKS) {
		g_pendingLegacyCadenceTicks -= XWA_MODERN_COMPATIBILITY_STEP_TICKS;
		g_legacyCadenceDue = 1;
	} else {
		g_legacyCadenceDue = 0;
	}
}

int XwaModernFlightTiming_IsLegacyCadenceDue(void) { return g_legacyCadenceDue; }

XwaModernAiCadence XwaModernFlightTiming_BeginAiAdvance(uint16_t elapsed_ticks) {
	XwaModernAiCadence cadence;

	if (!XwaModernFlightTiming_IsHighRate()) {
		cadence.elapsed_ticks = elapsed_ticks;
		cadence.due = 1;
		return cadence;
	}

	g_pendingAiTicks = XwaModernFlightTiming_SaturatingAdd(g_pendingAiTicks, elapsed_ticks);
	if (g_pendingAiTicks >= XWA_MODERN_COMPATIBILITY_STEP_TICKS) {
		cadence.elapsed_ticks = XWA_MODERN_COMPATIBILITY_STEP_TICKS;
		cadence.due = 1;
		g_pendingAiTicks -= XWA_MODERN_COMPATIBILITY_STEP_TICKS;
	} else {
		cadence.elapsed_ticks = 0;
		cadence.due = 0;
	}
	return cadence;
}

int XwaModernFlightTiming_AdvanceTransientAnimation(uint16_t elapsed_ticks) {
	if (!XwaModernFlightTiming_IsHighRate()) {
		return 1;
	}

	g_pendingTransientAnimationTicks =
		XwaModernFlightTiming_SaturatingAdd(g_pendingTransientAnimationTicks, elapsed_ticks);
	if (g_pendingTransientAnimationTicks < XWA_MODERN_COMPATIBILITY_STEP_TICKS) {
		return 0;
	}
	g_pendingTransientAnimationTicks -= XWA_MODERN_COMPATIBILITY_STEP_TICKS;
	return 1;
}

int XwaModernFlightTiming_AdvanceGlowMarkAnimation(uint16_t elapsed_ticks) {
	if (!XwaModernFlightTiming_IsHighRate()) {
		return 1;
	}

	g_pendingGlowMarkAnimationTicks =
		XwaModernFlightTiming_SaturatingAdd(g_pendingGlowMarkAnimationTicks, elapsed_ticks);
	if (g_pendingGlowMarkAnimationTicks < XWA_MODERN_COMPATIBILITY_STEP_TICKS) {
		return 0;
	}
	g_pendingGlowMarkAnimationTicks -= XWA_MODERN_COMPATIBILITY_STEP_TICKS;
	return 1;
}
