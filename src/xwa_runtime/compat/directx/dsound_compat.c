#include "xwa_runtime/compat/directx/dsound_compat.h"

#include "aeron/audio.h"
#include "aeron/log.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* DirectSound -> Aeron compatibility shim. See dsound_compat.h.
 *
 * Objects begin with an lpVtbl pointer so recovered code can call methods
 * through the real DirectSound ABI indices (both the typed vtbls in sound.h /
 * frontend_sound.c and the numeric-index calls in imsound.c resolve here).
 *
 * Scope: device + static secondary buffers are fully implemented (frontend UI
 * and flight 2D SFX). 3D buffers/listener (flight positional) and streaming
 * buffers (iMUSE music) are wired in their own phases; the extension points are
 * marked with TODO below. */

#define DS_OK 0
#define DS_FAIL (-1)

enum { DS_DSBPLAY_LOOPING = DSBPLAY_LOOPING_XWA };

/* Full DirectSound vtable layouts (port calling convention is the platform
 * default; the original ABI was __stdcall). */
typedef struct DSBufferVtbl {
	int (*QueryInterface)(void* self, const void* iid, void** out);            /* 0 */
	int (*AddRef)(void* self);                                                 /* 1 */
	int (*Release)(void* self);                                                /* 2 */
	int (*GetCaps)(void* self, void* caps);                                    /* 3 */
	int (*GetCurrentPosition)(void* self, uint32_t* play, uint32_t* write);    /* 4 */
	int (*GetFormat)(void* self, void* fmt, uint32_t size, uint32_t* written); /* 5 */
	int (*GetVolume)(void* self, int32_t* volume);                             /* 6 */
	int (*GetPan)(void* self, int32_t* pan);                                   /* 7 */
	int (*GetFrequency)(void* self, uint32_t* frequency);                      /* 8 */
	int (*GetStatus)(void* self, uint32_t* status);                            /* 9 */
	int (*Initialize)(void* self, void* device, const void* desc);             /* 10 */
	int (*Lock)(void* self, uint32_t offset, uint32_t bytes, void** p1, uint32_t* b1, void** p2, uint32_t* b2,
				uint32_t flags);                                                    /* 11 */
	int (*Play)(void* self, uint32_t reserved1, uint32_t priority, uint32_t flags); /* 12 */
	int (*SetCurrentPosition)(void* self, uint32_t position);                       /* 13 */
	int (*SetFormat)(void* self, const void* fmt);                                  /* 14 */
	int (*SetVolume)(void* self, int32_t volume);                                   /* 15 */
	int (*SetPan)(void* self, int32_t pan);                                         /* 16 */
	int (*SetFrequency)(void* self, uint32_t frequency);                            /* 17 */
	int (*Stop)(void* self);                                                        /* 18 */
	int (*Unlock)(void* self, void* p1, uint32_t b1, void* p2, uint32_t b2);        /* 19 */
	int (*Restore)(void* self);                                                     /* 20 */
} DSBufferVtbl;

typedef struct DSDeviceVtbl {
	int (*QueryInterface)(void* self, const void* iid, void** out);                             /* 0 */
	int (*AddRef)(void* self);                                                                  /* 1 */
	int (*Release)(void* self);                                                                 /* 2 */
	int (*CreateSoundBuffer)(void* self, const DSBufferDesc* desc, void** buffer, void* outer); /* 3 */
	int (*GetCaps)(void* self, void* caps);                                                     /* 4 */
	int (*DuplicateSoundBuffer)(void* self, void* source, void** duplicate);                    /* 5 */
	int (*SetCooperativeLevel)(void* self, void* hwnd, uint32_t level);                         /* 6 */
	int (*Compact)(void* self);                                                                 /* 7 */
	int (*GetSpeakerConfig)(void* self, uint32_t* config);                                      /* 8 */
	int (*SetSpeakerConfig)(void* self, uint32_t config);                                       /* 9 */
	int (*Initialize)(void* self, const void* guid);                                            /* 10 */
} DSDeviceVtbl;

typedef struct DSBuffer {
	const DSBufferVtbl* lpVtbl;
	int refcount;

	int rate;
	int channels;
	int bits;
	uint32_t capacity; /* dwBufferBytes */
	uint32_t flags;
	int is_primary;

	uint8_t* staging; /* lazily allocated capacity bytes for static PCM */

	AeronClip clip;
	int owns_clip;

	int volume_mb;      /* 0 == full volume */
	int pan_mb;         /* 0 == centered */
	uint32_t frequency; /* 0 == native rate */

	AeronVoice voice;
	int looping;

	int is3d; /* play positionally (DirectSound3D, mode != DISABLE) */
	float pos3[3];
	float vel3[3];
	float min_dist;
	float max_dist;

	int is_streaming; /* backed by an Aeron ring source instead of a clip */
	AeronRing ring;
	uint8_t* ring_base;
} DSBuffer;

typedef struct DSDevice {
	const DSDeviceVtbl* lpVtbl;
	int refcount;
} DSDevice;

/* --- unit conversions ---------------------------------------------------- */

static float DSoundCompat_GainFromMillibels(int millibels) {
	if (millibels <= -10000) {
		return 0.0f;
	}
	if (millibels >= 0) {
		return 1.0f;
	}
	return powf(10.0f, (float)millibels / 2000.0f);
}

static float DSoundCompat_PanFromMillibels(int millibels) {
	if (millibels < -10000) {
		millibels = -10000;
	}
	if (millibels > 10000) {
		millibels = 10000;
	}
	return (float)millibels / 10000.0f;
}

static float DSoundCompat_Pitch(const DSBuffer* buffer) {
	if (buffer->frequency == 0 || buffer->rate <= 0) {
		return 1.0f;
	}
	return (float)buffer->frequency / (float)buffer->rate;
}

/* --- DirectSound3D sub-interfaces ----------------------------------------- */

// GLOBAL: XWA 0x5AAEF0
const DSCompatGuid IID_IDirectSound3DBuffer = {
	0x279AFA86, 0x4981, 0x11CE, { 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60 }
};
// GLOBAL: XWA 0x5AAEE0
const DSCompatGuid IID_IDirectSound3DListener = {
	0x279AFA84, 0x4981, 0x11CE, { 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60 }
};

#define DS3DMODE_DISABLE 2u

static int DSoundCompat_GuidEqual(const void* a, const DSCompatGuid* b) {
	return a != NULL && memcmp(a, b, sizeof(DSCompatGuid)) == 0;
}

/* IDirectSound3DBuffer: positional parameters for one secondary buffer. A thin
 * wrapper that forwards to its owning DSBuffer and that buffer's live Aeron
 * voice. Vtable indices match the DirectSound3D ABI (only the methods the
 * recovered flight code calls are populated). */
typedef struct DS3DBuffer DS3DBuffer;
typedef struct DS3DBufferVtbl {
	int (*QueryInterface)(void*, const void*, void**);               /* 0 */
	int (*AddRef)(void*);                                            /* 1 */
	int (*Release)(void*);                                           /* 2 */
	int (*GetAllParameters)(void*, void*);                           /* 3 */
	int (*GetConeAngles)(void*, void*, void*);                       /* 4 */
	int (*GetConeOrientation)(void*, void*);                         /* 5 */
	int (*GetConeOutsideVolume)(void*, void*);                       /* 6 */
	int (*GetMaxDistance)(void*, float*);                            /* 7 */
	int (*GetMinDistance)(void*, float*);                            /* 8 */
	int (*GetMode)(void*, uint32_t*);                                /* 9 */
	int (*GetPosition)(void*, void*);                                /* 10 */
	int (*GetVelocity)(void*, void*);                                /* 11 */
	int (*SetAllParameters)(void*, const void*, uint32_t);           /* 12 */
	int (*SetConeAngles)(void*, uint32_t, uint32_t, uint32_t);       /* 13 */
	int (*SetConeOrientation)(void*, float, float, float, uint32_t); /* 14 */
	int (*SetConeOutsideVolume)(void*, int32_t, uint32_t);           /* 15 */
	int (*SetMaxDistance)(void*, float, uint32_t);                   /* 16 */
	int (*SetMinDistance)(void*, float, uint32_t);                   /* 17 */
	int (*SetMode)(void*, uint32_t, uint32_t);                       /* 18 */
	int (*SetPosition)(void*, float, float, float, uint32_t);        /* 19 */
	int (*SetVelocity)(void*, float, float, float, uint32_t);        /* 20 */
} DS3DBufferVtbl;

struct DS3DBuffer {
	const DS3DBufferVtbl* lpVtbl;
	int refcount;
	DSBuffer* owner;
};

static int DS3DBuffer_AddRef(void* self) { return ++((DS3DBuffer*)self)->refcount; }

static int DS3DBuffer_Release(void* self) {
	DS3DBuffer* b = (DS3DBuffer*)self;
	if (--b->refcount > 0) {
		return b->refcount;
	}
	free(b);
	return 0;
}

static int DS3DBuffer_SetMaxDistance(void* self, float dist, uint32_t apply) {
	(void)apply;
	((DS3DBuffer*)self)->owner->max_dist = dist;
	return DS_OK;
}

static int DS3DBuffer_SetMinDistance(void* self, float dist, uint32_t apply) {
	(void)apply;
	((DS3DBuffer*)self)->owner->min_dist = dist;
	return DS_OK;
}

static int DS3DBuffer_SetMode(void* self, uint32_t mode, uint32_t apply) {
	(void)apply;
	((DS3DBuffer*)self)->owner->is3d = (mode != DS3DMODE_DISABLE);
	return DS_OK;
}

static int DS3DBuffer_SetPosition(void* self, float x, float y, float z, uint32_t apply) {
	(void)apply;
	DSBuffer* owner = ((DS3DBuffer*)self)->owner;
	owner->pos3[0] = x;
	owner->pos3[1] = y;
	owner->pos3[2] = z;
	owner->is3d = 1;
	if (owner->voice) {
		float pos[3] = { x, y, z };
		Aeron_AudioVoiceSet3DPosition(owner->voice, pos);
	}
	return DS_OK;
}

static int DS3DBuffer_SetVelocity(void* self, float x, float y, float z, uint32_t apply) {
	(void)apply;
	DSBuffer* owner = ((DS3DBuffer*)self)->owner;
	owner->vel3[0] = x;
	owner->vel3[1] = y;
	owner->vel3[2] = z;
	if (owner->voice) {
		float vel[3] = { x, y, z };
		Aeron_AudioVoiceSet3DVelocity(owner->voice, vel);
	}
	return DS_OK;
}

static const DS3DBufferVtbl g_ds3d_buffer_vtbl = {
	0,
	DS3DBuffer_AddRef,
	DS3DBuffer_Release,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	DS3DBuffer_SetMaxDistance,
	DS3DBuffer_SetMinDistance,
	DS3DBuffer_SetMode,
	DS3DBuffer_SetPosition,
	DS3DBuffer_SetVelocity,
};

static int DSoundCompat_QueryInterface3DBuffer(DSBuffer* owner, void** out) {
	DS3DBuffer* wrapper = (DS3DBuffer*)calloc(1, sizeof(DS3DBuffer));
	if (!wrapper) {
		*out = NULL;
		return DS_FAIL;
	}
	wrapper->lpVtbl = &g_ds3d_buffer_vtbl;
	wrapper->refcount = 1;
	wrapper->owner = owner;
	*out = wrapper;
	return DS_OK;
}

/* IDirectSound3DListener: a single shared listener obtained from the primary
 * buffer. Accumulates position/orientation/velocity and the distance model,
 * pushing them to the Aeron mixer on CommitDeferredSettings. */
typedef struct DS3DListener {
	const void** lpVtbl;
	int refcount;
	float pos[3];
	float front[3];
	float top[3];
	float vel[3];
	float distance_factor;
	float rolloff_factor;
	float doppler_factor;
} DS3DListener;

static DS3DListener g_ds3d_listener;

static int DS3DListener_AddRef(void* self) { return ++((DS3DListener*)self)->refcount; }
static int DS3DListener_Release(void* self) { return --((DS3DListener*)self)->refcount; }

static void DS3DListener_PushDistanceModel(DS3DListener* l) {
	Aeron_AudioSetDistanceModel(l->distance_factor, l->rolloff_factor, l->doppler_factor);
}

static int DS3DListener_SetDistanceFactor(void* self, float value, uint32_t apply) {
	(void)apply;
	DS3DListener* l = (DS3DListener*)self;
	l->distance_factor = value;
	DS3DListener_PushDistanceModel(l);
	return DS_OK;
}

static int DS3DListener_SetDopplerFactor(void* self, float value, uint32_t apply) {
	(void)apply;
	DS3DListener* l = (DS3DListener*)self;
	l->doppler_factor = value;
	DS3DListener_PushDistanceModel(l);
	return DS_OK;
}

static int DS3DListener_SetOrientation(void* self, float fx, float fy, float fz, float tx, float ty, float tz,
									   uint32_t apply) {
	(void)apply;
	DS3DListener* l = (DS3DListener*)self;
	l->front[0] = fx;
	l->front[1] = fy;
	l->front[2] = fz;
	l->top[0] = tx;
	l->top[1] = ty;
	l->top[2] = tz;
	return DS_OK;
}

static int DS3DListener_SetPosition(void* self, float x, float y, float z, uint32_t apply) {
	(void)apply;
	DS3DListener* l = (DS3DListener*)self;
	l->pos[0] = x;
	l->pos[1] = y;
	l->pos[2] = z;
	return DS_OK;
}

static int DS3DListener_SetVelocity(void* self, float x, float y, float z, uint32_t apply) {
	(void)apply;
	DS3DListener* l = (DS3DListener*)self;
	l->vel[0] = x;
	l->vel[1] = y;
	l->vel[2] = z;
	return DS_OK;
}

static int DS3DListener_CommitDeferredSettings(void* self) {
	DS3DListener* l = (DS3DListener*)self;
	AeronAudioListener listener;
	memcpy(listener.pos, l->pos, sizeof(listener.pos));
	memcpy(listener.front, l->front, sizeof(listener.front));
	memcpy(listener.top, l->top, sizeof(listener.top));
	memcpy(listener.vel, l->vel, sizeof(listener.vel));
	Aeron_AudioSetListener(&listener);
	return DS_OK;
}

static const void* g_ds3d_listener_vtbl[] = {
	NULL,                              /* 0 QueryInterface */
	(const void*)DS3DListener_AddRef,  /* 1 */
	(const void*)DS3DListener_Release, /* 2 */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,                                             /* 3..9 getters */
	NULL,                                             /* 10 SetAllParameters */
	(const void*)DS3DListener_SetDistanceFactor,      /* 11 */
	(const void*)DS3DListener_SetDopplerFactor,       /* 12 */
	(const void*)DS3DListener_SetOrientation,         /* 13 */
	(const void*)DS3DListener_SetPosition,            /* 14 */
	NULL,                                             /* 15 SetRolloffFactor */
	(const void*)DS3DListener_SetVelocity,            /* 16 */
	(const void*)DS3DListener_CommitDeferredSettings, /* 17 */
};

static int DSoundCompat_QueryInterface3DListener(void** out) {
	g_ds3d_listener.lpVtbl = g_ds3d_listener_vtbl;
	g_ds3d_listener.refcount = 1;
	g_ds3d_listener.distance_factor = 1.0f;
	g_ds3d_listener.rolloff_factor = 1.0f;
	g_ds3d_listener.doppler_factor = 1.0f;
	*out = &g_ds3d_listener;
	return DS_OK;
}

/* --- buffer methods ------------------------------------------------------ */

static int DSoundBuffer_QueryInterface(void* self, const void* iid, void** out) {
	if (!out) {
		return DS_FAIL;
	}
	if (DSoundCompat_GuidEqual(iid, &IID_IDirectSound3DBuffer)) {
		return DSoundCompat_QueryInterface3DBuffer((DSBuffer*)self, out);
	}
	if (DSoundCompat_GuidEqual(iid, &IID_IDirectSound3DListener)) {
		/* The recovered code obtains the single listener from the primary buffer. */
		return DSoundCompat_QueryInterface3DListener(out);
	}
	*out = NULL;
	return DS_FAIL;
}

static int DSoundBuffer_AddRef(void* self) {
	DSBuffer* b = (DSBuffer*)self;
	return ++b->refcount;
}

static int DSoundBuffer_Release(void* self) {
	DSBuffer* b = (DSBuffer*)self;
	if (--b->refcount > 0) {
		return b->refcount;
	}
	if (b->voice) {
		Aeron_AudioVoiceStop(b->voice);
	}
	if (b->is_streaming && b->ring) {
		Aeron_AudioRingClose(b->ring);
	}
	if (b->owns_clip && b->clip) {
		Aeron_AudioClipDestroy(b->clip);
	}
	free(b->staging);
	free(b);
	return 0;
}

static int DSoundBuffer_GetCaps(void* self, void* caps) {
	(void)self;
	(void)caps;
	return DS_OK;
}

static int DSoundBuffer_GetCurrentPosition(void* self, uint32_t* play, uint32_t* write) {
	DSBuffer* b = (DSBuffer*)self;
	uint32_t cursor = 0;
	if (b->is_streaming) {
		cursor = (uint32_t)Aeron_AudioRingPlayCursorBytes(b->ring);
		{
			static int n;
			if (++n % 200 == 0) {
				Aeron_LogVerbose("xwa.audio", "shim ring playCursor=%u playing=%d", cursor,
						  Aeron_AudioRingIsPlaying(b->ring));
			}
		}
	}
	if (play) {
		*play = cursor;
	}
	if (write) {
		*write = cursor;
	}
	return DS_OK;
}

static int DSoundBuffer_GetFormat(void* self, void* fmt, uint32_t size, uint32_t* written) {
	DSBuffer* b = (DSBuffer*)self;
	DSWaveFormat wf;

	if (fmt && size >= sizeof(DSWaveFormat)) {
		wf.wFormatTag = 1; /* WAVE_FORMAT_PCM */
		wf.nChannels = (uint16_t)b->channels;
		wf.nSamplesPerSec = (uint32_t)b->rate;
		wf.wBitsPerSample = (uint16_t)b->bits;
		wf.nBlockAlign = (uint16_t)(b->channels * (b->bits / 8));
		wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
		wf.cbSize = 0;
		memcpy(fmt, &wf, sizeof(wf));
	}
	if (written) {
		*written = sizeof(DSWaveFormat);
	}
	return DS_OK;
}

static int DSoundBuffer_GetVolume(void* self, int32_t* volume) {
	DSBuffer* b = (DSBuffer*)self;
	if (volume) {
		*volume = b->volume_mb;
	}
	return DS_OK;
}

static int DSoundBuffer_GetPan(void* self, int32_t* pan) {
	DSBuffer* b = (DSBuffer*)self;
	if (pan) {
		*pan = b->pan_mb;
	}
	return DS_OK;
}

static int DSoundBuffer_GetFrequency(void* self, uint32_t* frequency) {
	DSBuffer* b = (DSBuffer*)self;
	if (frequency) {
		*frequency = b->frequency ? b->frequency : (uint32_t)b->rate;
	}
	return DS_OK;
}

static int DSoundBuffer_GetStatus(void* self, uint32_t* status) {
	DSBuffer* b = (DSBuffer*)self;
	uint32_t s = 0;
	int playing = b->is_streaming ? Aeron_AudioRingIsPlaying(b->ring)
								  : (b->voice && Aeron_AudioVoiceIsPlaying(b->voice));
	if (playing) {
		s = DSBSTATUS_PLAYING_XWA | (b->looping ? DSBSTATUS_LOOPING_XWA : 0u);
	}
	if (status) {
		*status = s;
	}
	return DS_OK;
}

static int DSoundBuffer_Initialize(void* self, void* device, const void* desc) {
	(void)self;
	(void)device;
	(void)desc;
	return DS_OK;
}

static int DSoundBuffer_Lock(void* self, uint32_t offset, uint32_t bytes, void** p1, uint32_t* b1, void** p2,
							 uint32_t* b2, uint32_t flags) {
	(void)flags;
	DSBuffer* b = (DSBuffer*)self;
	if (b->is_primary) {
		return DS_FAIL;
	}
	if (b->is_streaming) {
		if (offset > b->capacity) {
			return DS_FAIL;
		}
		if (!b->staging) {
			b->staging = (uint8_t*)calloc(1, b->capacity ? b->capacity : 1);
			if (!b->staging) {
				return DS_FAIL;
			}
		}
		uint32_t avail = b->capacity - offset;
		uint32_t first = bytes <= avail ? bytes : avail;
		if (p1) {
			*p1 = b->staging + offset;
		}
		if (b1) {
			*b1 = first;
		}
		if (bytes > first) { /* wrap to the ring start */
			if (p2) {
				*p2 = b->staging;
			}
			if (b2) {
				*b2 = bytes - first;
			}
		} else {
			if (p2) {
				*p2 = NULL;
			}
			if (b2) {
				*b2 = 0;
			}
		}
		return DS_OK;
	}
	if (!b->staging) {
		b->staging = (uint8_t*)calloc(1, b->capacity ? b->capacity : 1);
		if (!b->staging) {
			return DS_FAIL;
		}
	}
	if (offset > b->capacity) {
		return DS_FAIL;
	}
	uint32_t avail = b->capacity - offset;
	if (bytes > avail) {
		bytes = avail;
	}
	if (p1) {
		*p1 = b->staging + offset;
	}
	if (b1) {
		*b1 = bytes;
	}
	/* Static buffers never wrap. */
	if (p2) {
		*p2 = NULL;
	}
	if (b2) {
		*b2 = 0;
	}
	return DS_OK;
}

/* (Re)builds the Aeron clip from the staged PCM after a write. */
static void DSoundBuffer_RebuildClip(DSBuffer* b) {
	if (!b->staging || b->capacity == 0 || b->channels <= 0 || b->bits <= 0) {
		return;
	}
	int block_align = b->channels * (b->bits / 8);
	if (block_align <= 0) {
		return;
	}
	size_t frames = b->capacity / (uint32_t)block_align;
	AeronPcmFormat fmt = b->bits == 8 ? AERON_PCM_U8 : AERON_PCM_S16;

	if (b->owns_clip && b->clip) {
		Aeron_AudioClipDestroy(b->clip);
	}
	b->clip = Aeron_AudioClipCreate(b->staging, frames, b->rate, b->channels, fmt);
	b->owns_clip = 1;
}

static int DSoundBuffer_Unlock(void* self, void* p1, uint32_t b1, void* p2, uint32_t b2) {
	DSBuffer* b = (DSBuffer*)self;
	if (b->is_streaming) {
		if (p1 && b1) {
			uint8_t* ptr = (uint8_t*)p1;
			if (!b->staging || ptr < b->staging || ptr > b->staging + b->capacity) {
				return DS_FAIL;
			}
			if (!Aeron_AudioRingWrite(b->ring, (size_t)(ptr - b->staging), p1, b1)) {
				return DS_FAIL;
			}
		}
		if (p2 && b2) {
			uint8_t* ptr = (uint8_t*)p2;
			if (!b->staging || ptr < b->staging || ptr > b->staging + b->capacity) {
				return DS_FAIL;
			}
			if (!Aeron_AudioRingWrite(b->ring, (size_t)(ptr - b->staging), p2, b2)) {
				return DS_FAIL;
			}
		}
	} else if (!b->is_primary) {
		DSoundBuffer_RebuildClip(b);
	}
	return DS_OK;
}

static int DSoundBuffer_Play(void* self, uint32_t reserved1, uint32_t priority, uint32_t flags) {
	(void)reserved1;
	(void)priority;
	DSBuffer* b = (DSBuffer*)self;
	if (b->is_streaming) {
		b->looping = (flags & DS_DSBPLAY_LOOPING) != 0;
		Aeron_AudioRingPlay(b->ring, b->looping);
		Aeron_LogVerbose("xwa.audio", "shim ring play looping=%d cap=%u rate=%d", b->looping, b->capacity, b->rate);
		return DS_OK;
	}
	if (b->is_primary || !b->clip) {
		return DS_OK;
	}
	if (b->voice) {
		Aeron_AudioVoiceStop(b->voice);
		b->voice = 0;
	}
	b->looping = (flags & DS_DSBPLAY_LOOPING) != 0;
	float gain = DSoundCompat_GainFromMillibels(b->volume_mb);
	float pitch = DSoundCompat_Pitch(b);
	if (b->is3d) {
		b->voice =
			Aeron_AudioVoicePlay3D(b->clip, gain, pitch, b->looping, b->pos3, b->min_dist, b->max_dist);
	} else {
		b->voice =
			Aeron_AudioVoicePlay(b->clip, gain, DSoundCompat_PanFromMillibels(b->pan_mb), pitch, b->looping);
	}
	return DS_OK;
}

static int DSoundBuffer_SetCurrentPosition(void* self, uint32_t position) {
	(void)self;
	(void)position;
	/* New voices always start at the clip head, matching SetCurrentPosition(0). */
	return DS_OK;
}

static int DSoundBuffer_SetFormat(void* self, const void* fmt) {
	(void)self;
	(void)fmt;
	/* Primary-buffer format is fixed by the Aeron device. */
	return DS_OK;
}

static int DSoundBuffer_SetVolume(void* self, int32_t volume) {
	DSBuffer* b = (DSBuffer*)self;
	b->volume_mb = volume;
	if (b->is_streaming) {
		Aeron_AudioRingSetGain(b->ring, DSoundCompat_GainFromMillibels(volume));
	} else if (b->voice) {
		Aeron_AudioVoiceSetGain(b->voice, DSoundCompat_GainFromMillibels(volume));
	}
	return DS_OK;
}

static int DSoundBuffer_SetPan(void* self, int32_t pan) {
	DSBuffer* b = (DSBuffer*)self;
	b->pan_mb = pan;
	if (b->voice) {
		Aeron_AudioVoiceSetPan(b->voice, DSoundCompat_PanFromMillibels(pan));
	}
	return DS_OK;
}

static int DSoundBuffer_SetFrequency(void* self, uint32_t frequency) {
	DSBuffer* b = (DSBuffer*)self;
	b->frequency = frequency;
	if (b->voice) {
		Aeron_AudioVoiceSetPitch(b->voice, DSoundCompat_Pitch(b));
	}
	return DS_OK;
}

static int DSoundBuffer_Stop(void* self) {
	DSBuffer* b = (DSBuffer*)self;
	if (b->is_streaming) {
		Aeron_AudioRingStop(b->ring);
		return DS_OK;
	}
	if (b->voice) {
		Aeron_AudioVoiceStop(b->voice);
		b->voice = 0;
	}
	return DS_OK;
}

static int DSoundBuffer_Restore(void* self) {
	(void)self;
	return DS_OK;
}

static const DSBufferVtbl g_ds_buffer_vtbl = {
	DSoundBuffer_QueryInterface,
	DSoundBuffer_AddRef,
	DSoundBuffer_Release,
	DSoundBuffer_GetCaps,
	DSoundBuffer_GetCurrentPosition,
	DSoundBuffer_GetFormat,
	DSoundBuffer_GetVolume,
	DSoundBuffer_GetPan,
	DSoundBuffer_GetFrequency,
	DSoundBuffer_GetStatus,
	DSoundBuffer_Initialize,
	DSoundBuffer_Lock,
	DSoundBuffer_Play,
	DSoundBuffer_SetCurrentPosition,
	DSoundBuffer_SetFormat,
	DSoundBuffer_SetVolume,
	DSoundBuffer_SetPan,
	DSoundBuffer_SetFrequency,
	DSoundBuffer_Stop,
	DSoundBuffer_Unlock,
	DSoundBuffer_Restore,
};

static DSBuffer* DSoundCompat_AllocBuffer(void) {
	DSBuffer* b = (DSBuffer*)calloc(1, sizeof(DSBuffer));
	if (b) {
		b->lpVtbl = &g_ds_buffer_vtbl;
		b->refcount = 1;
	}
	return b;
}

/* --- device methods ------------------------------------------------------ */

static int DSoundDevice_QueryInterface(void* self, const void* iid, void** out) {
	(void)self;
	(void)iid;
	/* TODO(flight 3D): return an IDirectSound3DListener shim. */
	if (out) {
		*out = NULL;
	}
	return DS_FAIL;
}

static int DSoundDevice_AddRef(void* self) {
	DSDevice* d = (DSDevice*)self;
	return ++d->refcount;
}

static int DSoundDevice_Release(void* self) {
	DSDevice* d = (DSDevice*)self;
	if (--d->refcount > 0) {
		return d->refcount;
	}
	free(d);
	return 0;
}

static int DSoundDevice_CreateSoundBuffer(void* self, const DSBufferDesc* desc, void** buffer, void* outer) {
	(void)self;
	(void)outer;
	if (!buffer || !desc) {
		return DS_FAIL;
	}
	*buffer = NULL;

	DSBuffer* b = DSoundCompat_AllocBuffer();
	if (!b) {
		return DS_FAIL;
	}
	b->flags = desc->dwFlags;
	b->capacity = desc->dwBufferBytes;

	if (desc->dwFlags & DSBCAPS_PRIMARYBUFFER_XWA) {
		b->is_primary = 1;
	} else if (desc->lpwfxFormat) {
		b->rate = (int)desc->lpwfxFormat->nSamplesPerSec;
		b->channels = desc->lpwfxFormat->nChannels;
		b->bits = desc->lpwfxFormat->wBitsPerSample;
	}

	if (!b->is_primary && (desc->dwFlags & DSBCAPS_GETCURRENTPOSITION2_XWA) && b->channels > 0 &&
		b->bits > 0) {
		b->is_streaming = 1;
		b->ring = Aeron_AudioRingOpen(b->rate, b->channels, b->bits, b->capacity, 1.0f);
		b->ring_base = (uint8_t*)Aeron_AudioRingBase(b->ring);
		if (!b->ring || !b->ring_base) {
			free(b);
			return DS_FAIL;
		}
	}

	*buffer = b;
	return DS_OK;
}

static int DSoundDevice_GetCaps(void* self, void* caps) {
	(void)self;
	(void)caps;
	/* Caller pre-zeroes the DSCAPS struct; leaving it lets the recovered code
	 * pick its non-hardware primary-buffer path. */
	return DS_OK;
}

static int DSoundDevice_DuplicateSoundBuffer(void* self, void* source, void** duplicate) {
	(void)self;
	if (!duplicate || !source) {
		return DS_FAIL;
	}
	*duplicate = NULL;

	DSBuffer* src = (DSBuffer*)source;
	DSBuffer* dup = DSoundCompat_AllocBuffer();
	if (!dup) {
		return DS_FAIL;
	}
	dup->rate = src->rate;
	dup->channels = src->channels;
	dup->bits = src->bits;
	dup->capacity = src->capacity;
	dup->flags = src->flags;
	dup->volume_mb = src->volume_mb;
	dup->pan_mb = src->pan_mb;
	dup->frequency = src->frequency;
	dup->clip = src->clip; /* shared; the template owns the clip lifetime */
	dup->owns_clip = 0;
	dup->min_dist = src->min_dist; /* DirectSound copies 3D params into the duplicate */
	dup->max_dist = src->max_dist;

	*duplicate = dup;
	return DS_OK;
}

static int DSoundDevice_SetCooperativeLevel(void* self, void* hwnd, uint32_t level) {
	(void)self;
	(void)hwnd;
	(void)level;
	return DS_OK;
}

static int DSoundDevice_Compact(void* self) {
	(void)self;
	return DS_OK;
}

static int DSoundDevice_GetSpeakerConfig(void* self, uint32_t* config) {
	(void)self;
	if (config) {
		*config = 0;
	}
	return DS_OK;
}

static int DSoundDevice_SetSpeakerConfig(void* self, uint32_t config) {
	(void)self;
	(void)config;
	return DS_OK;
}

static int DSoundDevice_Initialize(void* self, const void* guid) {
	(void)self;
	(void)guid;
	return DS_OK;
}

static const DSDeviceVtbl g_ds_device_vtbl = {
	DSoundDevice_QueryInterface,      DSoundDevice_AddRef,     DSoundDevice_Release,
	DSoundDevice_CreateSoundBuffer,   DSoundDevice_GetCaps,    DSoundDevice_DuplicateSoundBuffer,
	DSoundDevice_SetCooperativeLevel, DSoundDevice_Compact,    DSoundDevice_GetSpeakerConfig,
	DSoundDevice_SetSpeakerConfig,    DSoundDevice_Initialize,
};

int DSoundCompat_Create(void** outDevice) {
	if (!outDevice) {
		return DS_FAIL;
	}
	*outDevice = NULL;

	DSDevice* device = (DSDevice*)calloc(1, sizeof(DSDevice));
	if (!device) {
		return DS_FAIL;
	}
	device->lpVtbl = &g_ds_device_vtbl;
	device->refcount = 1;
	*outDevice = device;
	return DS_OK;
}
