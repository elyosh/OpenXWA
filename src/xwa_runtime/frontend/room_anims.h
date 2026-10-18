#ifndef XWA_RUNTIME_FRONTEND_ROOM_ANIMS_H
#define XWA_RUNTIME_FRONTEND_ROOM_ANIMS_H

/* Room animations from frontend/animations.yaml.
 *
 * These are videos played over a room's art, such as the ships passing the
 * concourse windows. The file lists groups of animations:
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
 *           fps: 24             # optional: frames per second; the file's rate by default
 *           missions: [7, 8]    # optional: all, a list, or {except: [...]}
 *
 * Each group plays one animation at a time. When the room is entered, each
 * group picks a random animation from those allowed in the current mission.
 * After each play, the group picks again (`repeat: random`, the default) or
 * plays the same one again (`repeat: same`). Before each play it waits for a
 * random time within `delay`, which can also be a single number of seconds.
 * By default there is no wait. Without `size`, a video is drawn at its pixel
 * size divided by `scale` (1 by default). Groups are drawn in file order. If
 * the file has an error, the error is logged with its line and the whole file
 * is ignored. */

#ifdef __cplusplus
extern "C" {
#endif

/* Draws the animations of `room`, such as "concourse". Call it at the point in
 * the room's drawing where the animations belong. `frame` is the room's frame
 * counter. At 0 the room has just been entered and every group starts again.
 * Groups also start again the first time the room is drawn after
 * XwaRoomAnims_Leave. */
void XwaRoomAnims_Draw(const char* room, int frame);
/* Stops the animations of `room`. Called when the room is left. */
void XwaRoomAnims_Leave(const char* room);

void XwaRoomAnims_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
