#ifndef XWA_RUNTIME_MOVIE_TASK_H
#define XWA_RUNTIME_MOVIE_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Called before XwaPort_Init. A nonzero value plays the original movies even
   when the mod folder has replacements. */
void XwaMovieTask_Configure(int preferOriginalMovies);
int XwaMovieTask_Begin(const char* name, int noFade);
void XwaMovieTask_ReapFinished(void);
void XwaMovieTask_Tick(void);
void XwaMovieTask_PausedFrame(void);
void XwaMovieTask_SuppressClassicSubtitles(void);
void XwaMovieTask_Stop(int skipped);
int XwaMovieTask_IsActive(void);
int XwaMovieTask_IsComplete(void);
int XwaMovieTask_GetResult(void);
uint64_t XwaMovieTask_NextWakeDelayUs(void);
void XwaMovieTask_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
