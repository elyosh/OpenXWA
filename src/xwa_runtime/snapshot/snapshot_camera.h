#ifndef XWA_RUNTIME_SNAPSHOT_CAMERA_H
#define XWA_RUNTIME_SNAPSHOT_CAMERA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The external camera sits behind its focus object, at an offset that the game
 * rounds down to whole world units on each axis. The rounding changes as the
 * camera turns, so the camera jumps by a unit at a time against the object,
 * which is easy to see when the camera is close. These calls keep the part
 * that the rounding dropped, for the local player's camera, so that the
 * renderer can place the camera smoothly. The game's integer position doesn't
 * change. */

/* Drops the kept fraction. Called when the game starts to place `playerIdx`'s
   camera. */
void XwaSnapshotCamera_BeginUpdate(int playerIdx);
/* Keeps the fraction of `playerIdx`'s external camera, which the game has just
   placed `offset` units behind `focus` along the camera's forward axis. */
void XwaSnapshotCamera_NoteExternalCamera(int playerIdx, int focusX, int focusY, int focusZ, double offset);
/* The part of the camera position below whole world units, or zero when
   `position` isn't the position of the camera that the fraction was kept
   for. */
void XwaSnapshotCamera_PositionFraction(const int32_t position[3], float fraction[3]);

/* During every hyperspace phase the game replaces the player's camera with a
 * fixed one, looking along world +Y and moving along it, which its own
 * hyperspace effect needs. The HD renderer draws the world during the jump,
 * so it keeps the camera of ordinary flight instead. */

/* Keeps `playerIdx`'s camera as the game has just placed it, before the
   hyperspace override replaces it, with the seat's eye offset it was placed
   with. The game's hyperspace cockpit draw recomputes that offset later with
   the fixed orientation. */
void XwaSnapshotCamera_NoteHyperspaceOverride(int playerIdx);
/* The kept camera's rows (FVIEW_CopyRenderCameraRows order), position and
   seat eye offset in world units. Returns 0 when the latest camera update had
   no hyperspace override. */
int XwaSnapshotCamera_OrdinaryCamera(float rows[9], int32_t position[3], float eye_offset[3]);

#ifdef __cplusplus
}
#endif

#endif
