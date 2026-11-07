#ifndef XWA_RUNTIME_FRONTEND_ROOM_ANIMS_H
#define XWA_RUNTIME_FRONTEND_ROOM_ANIMS_H

/* Room animations: videos that play over a room's art, such as the ships
 * that pass the concourse windows. The mod documentation describes the keys.
 *
 * The animations come in groups, and each group plays one animation at a
 * time. When the player enters the room, each group picks a random animation
 * among those allowed in the current mission. After each play, the group
 * picks again or plays the same one again, after a random wait. Groups are
 * drawn in file order. A file with an error is ignored as a whole. */

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
