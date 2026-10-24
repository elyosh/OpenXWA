#ifndef XWA_RUNTIME_FILM_BENCHMARK_H
#define XWA_RUNTIME_FILM_BENCHMARK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Plays a film from the command line and measures each host frame, to
   compare builds on a run that repeats (--play-film). Films replay the
   simulation from recorded inputs, so each playback runs the same battle.

   The game starts as with skipintro, plays the film as the film room does,
   and quits when the film ends, where the film room would play it again. It
   quits with exit code 0, or 1 when the film doesn't exist or there is no
   active pilot. While the film plays, each host frame
   gets a record. At the end, the log gets a summary of each measure, and the
   CSV file, when one is asked for, gets the records. */

/* `filmName` is a film of the user folder's film folder, with or without
   .flm; empty for a normal start. `csvPath`, when not empty, is a file of the
   user folder. Call it before XwaPort_Init. */
void XwaFilmBenchmark_Configure(const char* filmName, const char* csvPath);
int XwaFilmBenchmark_IsRequested(void);

/* The first screen when a film is asked for: it starts the film's flight. */
int XwaFilmBenchmark_StartScreen(int frameCounter);
/* Whether the film benchmark's film is playing. A film then plays only
   once, instead of again from the start. */
int XwaFilmBenchmark_IsPlaying(void);
/* The flight ended. When it was the film's, logs the summary and asks the
   game to quit. */
void XwaFilmBenchmark_FlightEnded(void);
int XwaFilmBenchmark_ShouldQuit(void);
int XwaFilmBenchmark_ExitCode(void);

/* The times of this host frame, in microseconds: the flight task's tick when
   it ran a flight frame (`flightFrameRan`), and the snapshot capture and
   commit when it ran. */
void XwaFilmBenchmark_NoteFlight(int flightFrameRan, uint64_t flightUs);
void XwaFilmBenchmark_NoteCapture(uint64_t captureUs);
/* Ends the host frame's record with the remaster's frame and the whole host
   frame without the wait. */
void XwaFilmBenchmark_NoteHostFrame(uint64_t remasterUs, uint64_t hostFrameUs);

#ifdef __cplusplus
}
#endif

#endif
