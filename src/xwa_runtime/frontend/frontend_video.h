#ifndef XWA_RUNTIME_FRONTEND_FRONTEND_VIDEO_H
#define XWA_RUNTIME_FRONTEND_FRONTEND_VIDEO_H

/* Video players for the frontend's draw list.
 *
 * A frontend module, such as the room animations, opens a video in a slot,
 * updates its player each frame, and draws it with XwaFrontendVideo_Draw,
 * which adds a video record to the snapshot. The renderer then takes the
 * slot's current frame as a texture.
 *
 * The videos have no sound and play in real time. Their alpha is kept and
 * premultiplied, like the frontend's other images. */

#include "aeron/video.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XWA_FRONTEND_VIDEO_SLOTS 16

/* Opens `path` under `root` in a free slot. With `loop`, the video restarts
 * at its end with no empty frame in between. With `play`, it plays as soon as
 * it is ready; otherwise it waits for Aeron_VideoPlay. A nonzero `fps`
 * overrides the file's frame rate. Returns the slot, or -1 when the file
 * can't be opened or every slot is in use. */
int XwaFrontendVideo_Open(AeronVfsRoot root, const char* path, int loop, int play, float fps);
/* The player in `slot`, or NULL. */
AeronVideoPlayer* XwaFrontendVideo_Player(int slot);
/* Closes the player in `slot`. The player is paused at once and released at
 * the start of the next tick, because the renderer may still draw the frame
 * recorded this tick. */
void XwaFrontendVideo_Close(int slot);
/* Adds a video record for the current frame of `slot`, stretched over x, y,
 * width, height in 640x480 units. */
void XwaFrontendVideo_Draw(int slot, float x, float y, float width, float height);

/* For the renderer: a texture with the current frame of `slot`. Returns NULL
 * when no frame is ready, or when the slot now holds a different player than
 * the one the record's `serial` names. If the frame changed, the upload is
 * recorded on `cmd`, which must have no open pass. */
AeronTexture* XwaFrontendVideo_Texture(int slot, unsigned serial, AeronCommandBuffer* cmd);

/* Draws a video that replaces an image (see frontend_geometry.h), looping,
 * stretched over x, y, width, height in 640x480 units. `path` is the video's
 * path in the mod folder. The video opens the first time it is drawn. It
 * closes when a frontend frame passes without drawing it, or when the game
 * frees the image (XwaFrontendVideo_FreeImage). */
void XwaFrontendVideo_DrawImage(const char* path, float fps, float x, float y, float width, float height);
/* Closes the video at `path` when the game frees the image it replaces. */
void XwaFrontendVideo_FreeImage(const char* path);

/* Called at the start of each tick. Releases the players closed during the
 * last tick, and closes the image videos that the last frontend frame didn't
 * draw. */
void XwaFrontendVideo_StartTick(void);
void XwaFrontendVideo_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
