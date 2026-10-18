#ifndef XWA_RUNTIME_FRONTEND_ROOM_ANIMS_H
#define XWA_RUNTIME_FRONTEND_ROOM_ANIMS_H

/* Room animations from frontend/animations.yaml.
 *
 * Videos played over a room's art, such as the ships passing the concourse
 * windows. The file lists groups of animations:
 *
 *   version: 1
 *   groups:
 *     - room: concourse
 *       repeat: random        # or same
 *       delay: [0, 14.5]      # seconds before each play
 *       animations:
 *         - video: window.webm  # in frontend/animations/
 *           at: [0, 0]          # design pixels
 *           size: [2560, 300]   # optional
 *           fps: 24             # optional: the frames a second, else the file's
 *           missions: [7, 8]    # optional: all, a list, or {except: [...]}
 *
 * One animation per group plays at a time. When the room is entered, each
 * group picks an animation of the current mission at random. After each
 * play, it picks again (`repeat: random`, the default) or plays the same one
 * again (`repeat: same`). Each play waits for a random time in `delay`; there
 * is no wait by default. Without a size, a video is drawn at its pixel size
 * divided by `scale` (1 by default). Groups are drawn in file order. A file
 * with an error is logged with its line and ignored as a whole. */

#ifdef __cplusplus
extern "C" {
#endif

/* Draws the animations of `room`, such as "concourse", where they go between
 * the room's draws. `frame` is the room's frame counter: at 0, the room has
 * just been entered and every group starts again, as it does the first time
 * the room is drawn after XwaRoomAnims_Leave. */
void XwaRoomAnims_Draw(const char* room, int frame);
/* Stops the animations of `room`. Called when the room is left. */
void XwaRoomAnims_Leave(const char* room);

void XwaRoomAnims_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
