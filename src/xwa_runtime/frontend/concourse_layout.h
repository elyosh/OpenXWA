#ifndef XWA_RUNTIME_FRONTEND_CONCOURSE_LAYOUT_H
#define XWA_RUNTIME_FRONTEND_CONCOURSE_LAYOUT_H

/* Positions of the frontend's rooms from frontend/concourse/layout.yaml.
 *
 * The recovered rooms place their doors, hit areas and other items at
 * fixed positions. A mod whose art moves them gives the new positions in
 * this file, in design pixels (frontend/frontend.yaml `design`). This module
 * reads the file once, converts the values to 640x480 units, and answers
 * the rooms' queries. Every key is optional: a missing key keeps the stock
 * value the caller passes. A file with an error is logged with its line and
 * ignored as a whole. */

#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A point, such as "doors.tour", in 640x480 units, not rounded. Returns 0
 * and leaves *x and *y as they are when the file doesn't set it. */
int XwaConcourseLayout_Point(const char* key, float* x, float* y);
/* The point at `index` of a list of points, such as "medals.positions". */
int XwaConcourseLayout_PointAt(const char* key, int index, float* x, float* y);
/* A rectangle given as [x, y, width, height], such as
 * "family_room.statistics", in 640x480 units, not rounded. */
int XwaConcourseLayout_Rect(const char* key, float* x, float* y, float* width, float* height);
/* A position across or down the screen, such as "planet.left" or
 * "planet.top", in 640x480 units, not rounded. */
float XwaConcourseLayout_X(const char* key, float stock);
float XwaConcourseLayout_Y(const char* key, float stock);
/* A plain number, such as "family_room.lady_blue_mission". */
int XwaConcourseLayout_Int(const char* key, int stock);
/* A switch, such as "starfield". */
int XwaConcourseLayout_Bool(const char* key, int stock);
/* Whether the loading screen's progress bar grows linearly, as in XWAU
 * (`loading_screen.progress: linear`), instead of by the stock formula. */
int XwaConcourseLayout_LinearProgress(void);
/* The squadron logo position for logo `id` ("briefing.squad_logos"). */
int XwaConcourseLayout_SquadLogo(int id, float* x, float* y);

/* A value in units rounded to the nearest whole unit, as the game's hit
 * tests use it. */
int XwaConcourseLayout_Round(float value);

/* For a draw at the point `key`: sets *x and *y to the whole units the
 * game draws at, and gives the snapshot the rest, so the next sprite record
 * is drawn at the exact position. Returns 0 and changes nothing when the
 * file doesn't set the point. Call it just before the draw. */
int XwaConcourseLayout_DrawPoint(const char* key, int* x, int* y);
/* For a hit test at the point `key`: sets *x and *y to the point rounded
 * to whole units. Returns 0 and changes nothing when the file doesn't set
 * it. */
int XwaConcourseLayout_HitPoint(const char* key, int* x, int* y);

/* Draws `name` at the point `key`, or at (x, y) when the file doesn't set
 * it, as FrontImage_DrawSprite and FrontImage_DrawSpriteOpaque do. */
void XwaConcourseLayout_DrawSprite(const char* name, const char* key, int x, int y);
void XwaConcourseLayout_DrawSpriteOpaque(const char* name, const char* key, int x, int y);
/* Moves a hit rectangle by the point `key` rounded to whole units, or by
 * (x, y) when the file doesn't set it, as FrontendDraw_RectOffsetXY does. */
void XwaConcourseLayout_OffsetRect(FrontendRect* rect, const char* key, int x, int y);

/* Assigns a hit rectangle from [x, y, width, height] at `key`, rounded to
 * whole units, or the stock edges when the file doesn't set it, as
 * FrontendDraw_RectAssign does. */
void XwaConcourseLayout_AssignRect(FrontendRect* rect, const char* key, int left, int top, int right,
								   int bottom);
/* Assigns the stock edges, moved by the point `key` rounded to whole units,
 * minus the stock point (x, y): the rectangle follows the item drawn at the
 * point. */
void XwaConcourseLayout_AssignMovedRect(FrontendRect* rect, const char* key, int x, int y, int left, int top,
										int right, int bottom);

/* The Tour of Duty mission whose completion unlocks an item, such as
 * "family_room.lady_blue_mission" (1-based, `mission` in stock). A value of
 * 0 or less, or 255 or more, means always unlocked. */
int XwaConcourseLayout_UnlockMission(const char* key, int mission);
/* Whether that mission is completed, or the item is always unlocked. */
int XwaConcourseLayout_Unlocked(const char* key, int mission);

/* The medal case position of medal `index` from "medals.positions", in whole
 * units. A list that doesn't reach `index` gives (0, 0), as XWAU does. Leaves
 * *x and *y as they are when the file doesn't set the list. */
void XwaConcourseLayout_MedalPosition(int index, int* x, int* y);

/* Draws `name`, `width` by `height` units, centred on the point at `index`
 * of the list `key`, rounded to whole units, or on (x, y) when the file
 * doesn't set it. */
void XwaConcourseLayout_DrawSpriteCentered(const char* name, const char* key, int index, int x, int y,
										   int width, int height);
/* Moves a rectangle by the offset at `index` of the list `key`, rounded to
 * whole units. Leaves it when the file doesn't set the offset. */
void XwaConcourseLayout_MoveRect(FrontendRect* rect, const char* key, int index);

void XwaConcourseLayout_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
