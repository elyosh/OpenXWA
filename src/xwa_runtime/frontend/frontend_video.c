#include "xwa_runtime/frontend/frontend_video.h"

#include "aeron/aeron.h"
#include "aeron/log.h"
#include "xwa_runtime/frontend/frontend_screens.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <stdio.h>
#include <string.h>

/* The frontend shows only a few videos at once, so each player keeps 3
 * decoded frames and uses 2 decoding threads. */
#define XWA_FRONTEND_VIDEO_QUEUE_FRAMES 3
#define XWA_FRONTEND_VIDEO_THREADS 2

typedef struct XwaFrontendVideoSlot {
	AeronVideoPlayer* player;
	uint16_t serial;
	int closed; /* released at the start of the next tick */
} XwaFrontendVideoSlot;

static XwaFrontendVideoSlot g_videoSlots[XWA_FRONTEND_VIDEO_SLOTS];

/* A video that replaces an image, while the game draws it. */
typedef struct XwaFrontendImageVideo {
	char path[160]; /* empty when unused */
	int slot;       /* -1 when the video can't be opened */
	unsigned frame; /* the frontend frame that drew it last */
} XwaFrontendImageVideo;

static XwaFrontendImageVideo g_imageVideos[XWA_FRONTEND_VIDEO_SLOTS];

int XwaFrontendVideo_Open(AeronVfsRoot root, const char* path, int loop, int play, float fps) {
	AeronVideoOpenDesc desc;
	int slot = 0;

	while (slot < XWA_FRONTEND_VIDEO_SLOTS && g_videoSlots[slot].player) {
		slot++;
	}
	if (slot == XWA_FRONTEND_VIDEO_SLOTS) {
		Aeron_LogError("xwa.frontend", "%s: no free video slot; at most %d videos can play at once", path,
					   XWA_FRONTEND_VIDEO_SLOTS);
		return -1;
	}
	memset(&desc, 0, sizeof(desc));
	desc.vfs = Aeron_GetVfs();
	desc.root = root;
	desc.path = path;
	desc.autoplay = play;
	desc.loop = loop;
	desc.video_queue_frames = XWA_FRONTEND_VIDEO_QUEUE_FRAMES;
	desc.alpha = AERON_VIDEO_ALPHA_PREMULTIPLIED;
	desc.no_audio = 1;
	desc.max_decode_threads = XWA_FRONTEND_VIDEO_THREADS;
	desc.frame_rate = fps;
	g_videoSlots[slot].player = Aeron_VideoOpen(&desc);
	if (!g_videoSlots[slot].player) {
		Aeron_LogError("xwa.frontend", "%s: cannot open the video", path);
		return -1;
	}
	g_videoSlots[slot].serial++;
	g_videoSlots[slot].closed = 0;
	return slot;
}

static XwaFrontendVideoSlot* video_slot(int slot) {
	return slot >= 0 && slot < XWA_FRONTEND_VIDEO_SLOTS && g_videoSlots[slot].player ? &g_videoSlots[slot]
																					 : NULL;
}

AeronVideoPlayer* XwaFrontendVideo_Player(int slot) {
	XwaFrontendVideoSlot* s = video_slot(slot);
	return s && !s->closed ? s->player : NULL;
}

void XwaFrontendVideo_Close(int slot) {
	XwaFrontendVideoSlot* s = video_slot(slot);
	if (s && !s->closed) {
		Aeron_VideoPause(s->player);
		s->closed = 1;
	}
}

void XwaFrontendVideo_Draw(int slot, float x, float y, float width, float height) {
	XwaFrontendVideoSlot* s = video_slot(slot);
	if (s && !s->closed) {
		XwaSnapshot_EmitVideo(slot, s->serial, x, y, width, height);
	}
}

AeronTexture* XwaFrontendVideo_Texture(int slot, unsigned serial, AeronCommandBuffer* cmd) {
	XwaFrontendVideoSlot* s = video_slot(slot);
	return s && s->serial == (uint16_t)serial ? Aeron_VideoTexture(s->player, cmd) : NULL;
}

static void image_video_close(XwaFrontendImageVideo* video) {
	XwaFrontendVideo_Close(video->slot);
	video->path[0] = '\0';
	video->slot = -1;
}

void XwaFrontendVideo_DrawImage(const char* path, float fps, float x, float y, float width, float height) {
	XwaFrontendImageVideo* video = NULL;
	AeronVideoPlayer* player;

	for (int i = 0; i < XWA_FRONTEND_VIDEO_SLOTS && !video; i++) {
		if (strcmp(g_imageVideos[i].path, path) == 0) {
			video = &g_imageVideos[i];
		}
	}
	for (int i = 0; i < XWA_FRONTEND_VIDEO_SLOTS && !video; i++) {
		if (!g_imageVideos[i].path[0]) {
			video = &g_imageVideos[i];
			snprintf(video->path, sizeof(video->path), "%s", path);
			/* A video that can't be opened keeps slot -1 until it stops being
			   drawn, so the error is logged only once. */
			video->slot = XwaFrontendVideo_Open(AERON_VFS_ROOT_MOD, path, 1, 1, fps);
		}
	}
	if (!video) {
		return;
	}
	video->frame = XwaFrontendScreens_FrameCount();
	player = XwaFrontendVideo_Player(video->slot);
	if (player) {
		Aeron_VideoUpdate(player);
		XwaFrontendVideo_Draw(video->slot, x, y, width, height);
	}
}

void XwaFrontendVideo_FreeImage(const char* path) {
	for (int i = 0; i < XWA_FRONTEND_VIDEO_SLOTS; i++) {
		if (g_imageVideos[i].path[0] && strcmp(g_imageVideos[i].path, path) == 0) {
			image_video_close(&g_imageVideos[i]);
		}
	}
}

void XwaFrontendVideo_StartTick(void) {
	const unsigned frame = XwaFrontendScreens_FrameCount();
	for (int i = 0; i < XWA_FRONTEND_VIDEO_SLOTS; i++) {
		if (g_videoSlots[i].player && g_videoSlots[i].closed) {
			Aeron_VideoClose(g_videoSlots[i].player);
			g_videoSlots[i].player = NULL;
			g_videoSlots[i].closed = 0;
		}
	}
	for (int i = 0; i < XWA_FRONTEND_VIDEO_SLOTS; i++) {
		if (g_imageVideos[i].path[0] && g_imageVideos[i].frame != frame) {
			image_video_close(&g_imageVideos[i]);
		}
	}
}

void XwaFrontendVideo_Shutdown(void) {
	memset(g_imageVideos, 0, sizeof(g_imageVideos));
	for (int i = 0; i < XWA_FRONTEND_VIDEO_SLOTS; i++) {
		Aeron_VideoClose(g_videoSlots[i].player);
		g_videoSlots[i].player = NULL;
		g_videoSlots[i].closed = 0;
	}
}
