#ifndef XWA_FRONTEND_FILM_ROOM_H
#define XWA_FRONTEND_FILM_ROOM_H

#include "xwa/frontend/frontend_file_list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FilmRoomPendingAction {
	FILM_ROOM_ACTION_NONE = 0,
	FILM_ROOM_ACTION_EXIT_CONCOURSE = 1,
	FILM_ROOM_ACTION_PLAY_SELECTED_FILM = 2,
} FilmRoomPendingAction;

extern int g_filmFeatureEnabled;
extern int g_filmRoomListScrollOffset;
extern FilmRoomPendingAction g_filmRoomPendingAction;
extern int g_filmRoomSelectedFilmIndex;
extern FrontFilenameList* g_filmRoomFilmList;

int FilmRoom_UpdateRightBarAnimation(void);
int FilmRoom_DrawFilmList(int frameCounter);
int FilmRoom_DrawLoadDeleteButtons(void);
int FilmRoom_Update(int frameCounter);
int FilmRoom_Exit(int frameCounter);

#ifdef __cplusplus
}
#endif

#endif
