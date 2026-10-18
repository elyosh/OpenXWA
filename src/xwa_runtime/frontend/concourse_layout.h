#ifndef XWA_RUNTIME_FRONTEND_CONCOURSE_LAYOUT_H
#define XWA_RUNTIME_FRONTEND_CONCOURSE_LAYOUT_H

/* Room positions from frontend/concourse/layout.yaml.
 *
 * The game draws the rooms' doors, hit areas and other items at fixed
 * positions. A mod whose art moves them sets the new positions in this file,
 * in design pixels (frontend/frontend.yaml `design`). The file is read once
 * and the values are converted to 640x480 units. Keys are optional: a missing
 * key keeps the stock value that the caller passes. If the file has an
 * error, the error is logged with its line and the whole file is ignored. */

#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A point, such as "doors.tour", in 640x480 units, not rounded. Returns 0
 * and leaves *x and *y unchanged when the file doesn't set it. */
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
/* Whether the loading screen's progress bar grows linearly
 * (`loading_screen.progress: linear`) instead of by the stock formula. */
int XwaConcourseLayout_LinearProgress(void);
/* The squadron logo position for logo `id` ("briefing.squad_logos"). */
int XwaConcourseLayout_SquadLogo(int id, float* x, float* y);

/* Rounds a value to the nearest whole unit, as hit tests need. */
int XwaConcourseLayout_Round(float value);

/* For a draw at the point `key`: sets *x and *y to the point rounded down to
 * whole units, and passes the fraction to the snapshot so the next sprite is
 * drawn at the exact position. Call it just before the draw. Returns 0 and
 * changes nothing when the file doesn't set the point. */
int XwaConcourseLayout_DrawPoint(const char* key, int* x, int* y);
/* For a hit test at the point `key`: sets *x and *y to the point rounded
 * to whole units. Returns 0 and changes nothing when the file doesn't set
 * it. */
int XwaConcourseLayout_HitPoint(const char* key, int* x, int* y);

/* FrontImage_DrawSprite and FrontImage_DrawSpriteOpaque at the point `key`,
 * or at (x, y) when the file doesn't set it. */
void XwaConcourseLayout_DrawSprite(const char* name, const char* key, int x, int y);
void XwaConcourseLayout_DrawSpriteOpaque(const char* name, const char* key, int x, int y);
/* FrontendDraw_RectOffsetXY by the point `key` rounded to whole units, or by
 * (x, y) when the file doesn't set it. */
void XwaConcourseLayout_OffsetRect(FrontendRect* rect, const char* key, int x, int y);

/* FrontendDraw_RectAssign with the rectangle [x, y, width, height] at `key`,
 * rounded to whole units, or with the stock edges when the file doesn't set
 * it. */
void XwaConcourseLayout_AssignRect(FrontendRect* rect, const char* key, int left, int top, int right,
								   int bottom);
/* Assigns the stock edges, then moves the rectangle by the point `key`
 * (rounded to whole units) minus the stock point (x, y). This keeps the
 * rectangle on the item drawn at that point. */
void XwaConcourseLayout_AssignMovedRect(FrontendRect* rect, const char* key, int x, int y, int left, int top,
										int right, int bottom);

/* The Tour of Duty mission that unlocks an item when completed, such as
 * "family_room.lady_blue_mission". Missions start at 1, and `mission` is the
 * stock value. A value of 0 or less, or 255 or more, means always unlocked
 * and returns 0. */
int XwaConcourseLayout_UnlockMission(const char* key, int mission);
/* Whether the item is unlocked: its mission is completed or it has none. */
int XwaConcourseLayout_Unlocked(const char* key, int mission);

/* The medal case position of medal `index` from "medals.positions", in whole
 * units. If the list is too short for `index`, the position is (0, 0). Leaves
 * *x and *y unchanged when the file doesn't set the list. */
void XwaConcourseLayout_MedalPosition(int index, int* x, int* y);

/* Draws `name`, `width` by `height` units, centred on the point at `index`
 * of the list `key`, rounded to whole units, or on (x, y) when the file
 * doesn't set it. */
void XwaConcourseLayout_DrawSpriteCentered(const char* name, const char* key, int index, int x, int y,
										   int width, int height);
/* Moves a rectangle by the offset at `index` of the list `key`, rounded to
 * whole units. Does nothing when the file doesn't set the offset. */
void XwaConcourseLayout_MoveRect(FrontendRect* rect, const char* key, int index);

void XwaConcourseLayout_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
