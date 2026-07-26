#ifndef XWA_RUNTIME_MODERN_FLIGHT_TIMING_H
#define XWA_RUNTIME_MODERN_FLIGHT_TIMING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XwaModernFlightTimingReason {
	XWA_MODERN_TIMING_CONFIGURED,
	XWA_MODERN_TIMING_MULTIPLAYER
} XwaModernFlightTimingReason;

typedef struct XwaModernAiCadence {
	uint16_t elapsed_ticks;
	uint8_t due;
} XwaModernAiCadence;

void XwaModernFlightTiming_Configure(int requested_step_ticks);
void XwaModernFlightTiming_BeginSession(int player_count);
void XwaModernFlightTiming_EndSession(void);
int XwaModernFlightTiming_StepTicks(void);
int XwaModernFlightTiming_HangarStepTicks(void);
int XwaModernFlightTiming_IsHighRate(void);

void XwaModernFlightTiming_BeginAdvance(uint16_t elapsed_ticks);
int XwaModernFlightTiming_IsLegacyCadenceDue(void);
XwaModernAiCadence XwaModernFlightTiming_BeginAiAdvance(uint16_t elapsed_ticks);
int XwaModernFlightTiming_AdvanceTransientAnimation(uint16_t elapsed_ticks);
int XwaModernFlightTiming_AdvanceGlowMarkAnimation(uint16_t elapsed_ticks);

#ifdef __cplusplus
}
#endif

#endif
