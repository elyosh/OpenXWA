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
/* How much Sound_FlushQueuedEffects lowers the priority of the playing
   sounds at game time `game_time`. The original flushes once per flight
   frame, and its flight frames are 8 ticks apart, so a playing sound loses 1
   every 8 ticks and new sounds can take its slot. With unlocked timing,
   flight frames come faster, and sounds would lose their slots sooner,
   depending on the frame rate. So in unlocked timing this counts the 8-tick
   steps of game time since the last call, and carries the rest over; in
   compatibility timing, it is 1, as in the original. */
int XwaModernFlightTiming_SoundPriorityDecay(int game_time);
/* Uses `step_ticks` for the rest of the flight session: a film plays with
   the step that it was recorded with. Values outside 1 to 8 are ignored. */
void XwaModernFlightTiming_UseFilmStepTicks(int step_ticks);
/* Films recorded by the original game don't save a timing step. Their steps
   ran the original code, so they play in compatibility timing, whatever the
   player's setting. */
void XwaModernFlightTiming_UseOriginalFilmTiming(void);
int XwaModernFlightTiming_HangarStepTicks(void);
int XwaModernFlightTiming_IsHighRate(void);

void XwaModernFlightTiming_BeginAdvance(uint16_t elapsed_ticks);
/* Starts the cadences again from 0, then begins the advance of the current
   step. Film recording and playback call it at the same step, so that the
   cadences run on the same steps in both. */
void XwaModernFlightTiming_RestartCadence(uint16_t elapsed_ticks);
int XwaModernFlightTiming_IsLegacyCadenceDue(void);
XwaModernAiCadence XwaModernFlightTiming_BeginAiAdvance(uint16_t elapsed_ticks);
int XwaModernFlightTiming_AdvanceTransientAnimation(uint16_t elapsed_ticks);
int XwaModernFlightTiming_AdvanceGlowMarkAnimation(uint16_t elapsed_ticks);
int XwaModernFlightTiming_ScaleStepTicks(int step_start_time, uint16_t elapsed_ticks, uint32_t rate,
										 uint32_t divisor);

#ifdef __cplusplus
}
#endif

#endif
