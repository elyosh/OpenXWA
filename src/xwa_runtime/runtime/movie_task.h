#ifndef XWA_RUNTIME_MOVIE_TASK_H
#define XWA_RUNTIME_MOVIE_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Rooms that play a movie on entry, each of which a mod can choose in
   frontend/movies/movies.yaml. */
typedef enum XwaMovieRoom {
	XWA_MOVIE_ROOM_FILM,
	XWA_MOVIE_ROOM_SINGLE_PLAYER_POD,
	XWA_MOVIE_ROOM_MULTIPLAYER_POD,
	XWA_MOVIE_ROOM_COUNT
} XwaMovieRoom;

/* Called before XwaPort_Init. A nonzero value plays the original movies even
   when the mod folder has replacements. */
void XwaMovieTask_Configure(int preferOriginalMovies);
int XwaMovieTask_Begin(const char* name, int noFade);
/* Plays `<mission>_<name>` for the mission selected in the mission list,
   such as "1B1M7G_ceremony", when that movie exists, otherwise `name`. */
int XwaMovieTask_BeginForMission(const char* name, int noFade);
/* The movie that `room` plays: the one movies.yaml names, else "pod" as in
   the original game. */
const char* XwaMovieTask_RoomMovie(XwaMovieRoom room);
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
