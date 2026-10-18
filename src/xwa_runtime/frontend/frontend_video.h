#ifndef XWA_RUNTIME_FRONTEND_FRONTEND_VIDEO_H
#define XWA_RUNTIME_FRONTEND_FRONTEND_VIDEO_H

/* Video players for the frontend's draw list.
 *
 * A frontend module, such as the room animations, opens a video in a slot,
 * updates its player each frame, and draws it with XwaFrontendVideo_Draw,
 * which adds a video record to the snapshot. The renderer then takes the
 * slot's current frame as a texture.
 *
 * The videos have no sound and play by the wall clock. They keep their alpha,
 * premultiplied like the frontend's other images. */

#include "aeron/video.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XWA_FRONTEND_VIDEO_SLOTS 16

/* Opens `path` under `root` in a free slot. With `loop`, the video starts
 * again at its end, with no empty frame between the two passes. With `play`,
 * it plays as soon as it is ready; otherwise it waits for Aeron_VideoPlay.
 * A nonzero `fps` shows the frames at that rate instead of the file's.
 * Returns the slot, or -1 when the file can't be opened or every slot is in
 * use. */
int XwaFrontendVideo_Open(AeronVfsRoot root, const char* path, int loop, int play, float fps);
/* The player in `slot`, or NULL. */
AeronVideoPlayer* XwaFrontendVideo_Player(int slot);
/* Closes the player in `slot`. The player is paused now and released at the
 * start of the next tick, because the renderer can still draw the frame that
 * this tick recorded. */
void XwaFrontendVideo_Close(int slot);
/* Adds a video record for the current frame of `slot`, stretched over x, y,
 * width, height in 640x480 units. */
void XwaFrontendVideo_Draw(int slot, float x, float y, float width, float height);

/* For the renderer: a texture with the current frame of `slot`, or NULL when
 * no frame is ready or the slot no longer holds the player that the record
 * named (`serial`). When the frame has changed, the upload is recorded on
 * `cmd`, which must have no open pass. */
AeronTexture* XwaFrontendVideo_Texture(int slot, unsigned serial, AeronCommandBuffer* cmd);

/* Releases the players closed during the last tick. Called at the start of
 * each tick. */
void XwaFrontendVideo_ReleaseClosed(void);
void XwaFrontendVideo_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
