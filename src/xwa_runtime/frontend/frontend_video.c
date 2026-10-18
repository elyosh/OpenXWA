#include "xwa_runtime/frontend/frontend_video.h"

#include "aeron/aeron.h"
#include "aeron/log.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <string.h>

/* The frontend draws at most a few videos at a time, so the players are
 * small: 3 decoded frames each, and 2 decoding threads. */
#define XWA_FRONTEND_VIDEO_QUEUE_FRAMES 3
#define XWA_FRONTEND_VIDEO_THREADS 2

typedef struct XwaFrontendVideoSlot {
	AeronVideoPlayer* player;
	uint16_t serial;
	int closed; /* released at the start of the next tick */
} XwaFrontendVideoSlot;

static XwaFrontendVideoSlot g_videoSlots[XWA_FRONTEND_VIDEO_SLOTS];

int XwaFrontendVideo_Open(AeronVfsRoot root, const char* path, int loop, int play, float fps) {
	AeronVideoOpenDesc desc;
	int slot = 0;

	while (slot < XWA_FRONTEND_VIDEO_SLOTS && g_videoSlots[slot].player) {
		slot++;
	}
	if (slot == XWA_FRONTEND_VIDEO_SLOTS) {
		Aeron_LogError("xwa.frontend", "%s: no free video slot; at most %d videos play at a time", path,
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
		Aeron_LogError("xwa.frontend", "%s: the video can't be opened", path);
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

void XwaFrontendVideo_ReleaseClosed(void) {
	for (int i = 0; i < XWA_FRONTEND_VIDEO_SLOTS; i++) {
		if (g_videoSlots[i].player && g_videoSlots[i].closed) {
			Aeron_VideoClose(g_videoSlots[i].player);
			g_videoSlots[i].player = NULL;
			g_videoSlots[i].closed = 0;
		}
	}
}

void XwaFrontendVideo_Shutdown(void) {
	for (int i = 0; i < XWA_FRONTEND_VIDEO_SLOTS; i++) {
		Aeron_VideoClose(g_videoSlots[i].player);
		g_videoSlots[i].player = NULL;
		g_videoSlots[i].closed = 0;
	}
}
