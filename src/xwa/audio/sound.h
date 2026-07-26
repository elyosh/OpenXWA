#ifndef XWA_AUDIO_SOUND_H
#define XWA_AUDIO_SOUND_H

#include "xwa_runtime/compat/directx/dsound_compat.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DirectSound COM methods use the __stdcall convention (callee cleans the
 * stack). Only the MSVC matching build needs the exact convention; the port
 * bridges these interfaces and does not issue raw COM calls. */
#ifndef XWA_MODERN
#define XWA_STDCALL __stdcall
#else
#define XWA_STDCALL
#endif

typedef struct IDirectSoundBuffer   IDirectSoundBuffer;
typedef struct IDirectSound3DBuffer IDirectSound3DBuffer;

/* Runtime-only sound definition. The original stored 32-bit DirectSound
 * pointers in 4-byte fields; the port stores native pointers, so the struct is
 * wider on 64-bit hosts. It is never serialized, so the exact layout is free. */
#pragma pack(push, 1)
typedef struct SoundEffectDef {
	char                  name[64];
	char                  fileName[128];
	IDirectSoundBuffer*   buffer;
	IDirectSound3DBuffer* buffer3D;
	uint8_t               currentPriority;
} SoundEffectDef;
#pragma pack(pop)

typedef struct IDirectSoundBufferVtbl {
	int (XWA_STDCALL* QueryInterface)(IDirectSoundBuffer* self, const void* iid, void** out);
	void* AddRef;
	int (XWA_STDCALL* Release)(IDirectSoundBuffer* self);
	int (XWA_STDCALL* GetCaps)(IDirectSoundBuffer* self, DSBufferCaps* caps);
	int (XWA_STDCALL* GetCurrentPosition)(IDirectSoundBuffer* self, uint32_t* playCursor,
										 uint32_t* writeCursor);
	int (XWA_STDCALL* GetFormat)(IDirectSoundBuffer* self, void* format, uint32_t size, uint32_t* written);
	void* GetVolume;
	void* GetPan;
	void* GetFrequency;
	int (XWA_STDCALL* GetStatus)(IDirectSoundBuffer* self, uint32_t* status);
	void* Initialize;
	int (XWA_STDCALL* Lock)(IDirectSoundBuffer* self, uint32_t offset, uint32_t bytes, void** audioPtr1,
							  uint32_t* audioBytes1, void** audioPtr2, uint32_t* audioBytes2, uint32_t flags);
	int (XWA_STDCALL* Play)(IDirectSoundBuffer* self, uint32_t reserved1, uint32_t priority, uint32_t flags);
	int (XWA_STDCALL* SetCurrentPosition)(IDirectSoundBuffer* self, uint32_t position);
	int (XWA_STDCALL* SetFormat)(IDirectSoundBuffer* self, const DSWaveFormat* format);
	int (XWA_STDCALL* SetVolume)(IDirectSoundBuffer* self, int volume);
	int (XWA_STDCALL* SetPan)(IDirectSoundBuffer* self, int pan);
	int (XWA_STDCALL* SetFrequency)(IDirectSoundBuffer* self, int frequency);
	int (XWA_STDCALL* Stop)(IDirectSoundBuffer* self);
	int (XWA_STDCALL* Unlock)(IDirectSoundBuffer* self, void* audioPtr1, uint32_t audioBytes1,
								void* audioPtr2, uint32_t audioBytes2);
	int (XWA_STDCALL* Restore)(IDirectSoundBuffer* self);
} IDirectSoundBufferVtbl;

struct IDirectSoundBuffer {
	const IDirectSoundBufferVtbl* lpVtbl;
};

typedef struct IDirectSound3DBufferVtbl {
	void* QueryInterface;     /* 0 */
	void* AddRef;             /* 1 */
	int (XWA_STDCALL* Release)(IDirectSound3DBuffer* self); /* 2 */
	void* GetAllParameters;   /* 3 */
	void* GetConeAngles;      /* 4 */
	void* GetConeOrientation; /* 5 */
	void* GetConeOutsideVolume; /* 6 */
	int (XWA_STDCALL* GetMaxDistance)(IDirectSound3DBuffer* self, float* distance); /* 7 */
	int (XWA_STDCALL* GetMinDistance)(IDirectSound3DBuffer* self, float* distance); /* 8 */
	void* GetMode;            /* 9 */
	void* GetPosition;        /* 10 */
	void* GetVelocity;        /* 11 */
	void* SetAllParameters;   /* 12 */
	void* SetConeAngles;      /* 13 */
	void* SetConeOrientation; /* 14 */
	void* SetConeOutsideVolume; /* 15 */
	int (XWA_STDCALL* SetMaxDistance)(IDirectSound3DBuffer* self, float dist, uint32_t apply); /* 16 */
	int (XWA_STDCALL* SetMinDistance)(IDirectSound3DBuffer* self, float dist, uint32_t apply); /* 17 */
	int (XWA_STDCALL* SetMode)(IDirectSound3DBuffer* self, uint32_t mode, uint32_t apply);     /* 18 */
	int (XWA_STDCALL* SetPosition)(IDirectSound3DBuffer* self, float x, float y, float z, uint32_t apply); /* 19 */
	int (XWA_STDCALL* SetVelocity)(IDirectSound3DBuffer* self, float x, float y, float z, uint32_t apply); /* 20 */
} IDirectSound3DBufferVtbl;

struct IDirectSound3DBuffer {
	const IDirectSound3DBufferVtbl* lpVtbl;
};

typedef struct IDirectSound3DListener IDirectSound3DListener;
typedef struct IDirectSound3DListenerVtbl {
	void* QueryInterface;    /* 0 */
	void* AddRef;            /* 1 */
	int (XWA_STDCALL* Release)(IDirectSound3DListener* self); /* 2 */
	void* GetAllParameters;  /* 3 */
	void* GetDistanceFactor; /* 4 */
	void* GetDopplerFactor;  /* 5 */
	void* GetOrientation;    /* 6 */
	void* GetPosition;       /* 7 */
	void* GetRolloffFactor;  /* 8 */
	void* GetVelocity;       /* 9 */
	void* SetAllParameters;  /* 10 */
	int (XWA_STDCALL* SetDistanceFactor)(IDirectSound3DListener* self, float value, uint32_t apply); /* 11 */
	int (XWA_STDCALL* SetDopplerFactor)(IDirectSound3DListener* self, float value, uint32_t apply);  /* 12 */
	int (XWA_STDCALL* SetOrientation)(IDirectSound3DListener* self, float fx, float fy, float fz, float tx,
									  float ty, float tz, uint32_t apply); /* 13 */
	int (XWA_STDCALL* SetPosition)(IDirectSound3DListener* self, float x, float y, float z, uint32_t apply); /* 14 */
	void* SetRolloffFactor; /* 15 */
	int (XWA_STDCALL* SetVelocity)(IDirectSound3DListener* self, float x, float y, float z, uint32_t apply); /* 16 */
	int (XWA_STDCALL* CommitDeferredSettings)(IDirectSound3DListener* self); /* 17 */
} IDirectSound3DListenerVtbl;

struct IDirectSound3DListener {
	const IDirectSound3DListenerVtbl* lpVtbl;
};

typedef struct IDirectSound IDirectSound;
typedef struct IDirectSoundVtbl {
	void* QueryInterface; /* 0 */
	void* AddRef;         /* 1 */
	int (XWA_STDCALL* Release)(IDirectSound* self); /* 2 */
	int (XWA_STDCALL* CreateSoundBuffer)(IDirectSound* self, const DSBufferDesc* desc,
										 IDirectSoundBuffer** buffer, void* outer); /* 3 */
	int (XWA_STDCALL* GetCaps)(IDirectSound* self, DSoundDeviceCaps* caps); /* 4 */
	int (XWA_STDCALL* DuplicateSoundBuffer)(IDirectSound* self, IDirectSoundBuffer* source,
											IDirectSoundBuffer** duplicate); /* 5 */
	int (XWA_STDCALL* SetCooperativeLevel)(IDirectSound* self, void* hwnd, uint32_t level); /* 6 */
	void* Compact; /* 7 */
	int (XWA_STDCALL* GetSpeakerConfig)(IDirectSound* self, uint32_t* config); /* 8 */
	void* SetSpeakerConfig; /* 9 */
	void* Initialize;       /* 10 */
} IDirectSoundVtbl;

struct IDirectSound {
	const IDirectSoundVtbl* lpVtbl;
};

#pragma pack(push, 1)
typedef struct ActiveSoundInstance {
	int                   soundId;
	int                   sequence;
	int                   priority;
	uint32_t              sourceObjOrPointRef;
	uint16_t              sourceCreationIdx;
	IDirectSoundBuffer*   buffer;
	IDirectSound3DBuffer* buffer3D;
} ActiveSoundInstance;

typedef struct SoundQueueEntry {
	int      soundId;
	int      volume;
	int      pan;
	int      loop;
	int      param2;
	int      priority;
	int      pitch;
	uint32_t sourceObjOrPointRef;
	int      worldX;
	int      worldY;
	int      worldZ;
	uint8_t  hasWorldPosition;
} SoundQueueEntry;
#pragma pack(pop)

/* SoundEffectDef size/offset asserts intentionally dropped: the port widens the
 * buffer pointers to native width, so the original 32-bit byte layout no longer
 * applies (the struct is runtime-only and never serialized). */
typedef char active_sound_instance_buffer_offset[(offsetof(ActiveSoundInstance, buffer) == 0x12) ? 1 : -1];
typedef char sound_queue_entry_size[(sizeof(SoundQueueEntry) == 0x2D) ? 1 : -1];
typedef char sound_queue_entry_priority_offset[(offsetof(SoundQueueEntry, priority) == 0x14) ? 1 : -1];

extern int                 g_sfxIds[2872];
extern const int           g_directSoundVolumeMillibelTable[128];
extern void*               g_directSound;
extern int                 g_maxActiveSounds;
extern int                 g_activeSoundCount;
extern int                 g_nextSoundInstanceSeq;
extern int                 g_soundQueueCount;
extern uint8_t             g_sound3DEnabled;
extern ActiveSoundInstance g_activeSoundInstances[32];
extern SoundQueueEntry     g_soundQueue[5];
extern int                 g_soundCount;
extern SoundEffectDef      g_soundDefs[1536];
extern IDirectSoundBuffer*     g_primarySoundBuffer;
extern IDirectSound3DListener* g_sound3DListener;
extern uint8_t                 g_soundHardware3DBuffersAvailable;
extern uint8_t                 g_flightConfAudio22k;
extern int                     g_sound3DListenerLastGameTime;

uint8_t Sound_Init_Sound_Engine(void* hwnd);
uint8_t Sound_Shutdown_Sound_Engine(void);
uint8_t Sound_LoadEffectEx(const char* fileName, const char* name, int create3DFlags,
							uint16_t minDistanceOrRolloff);
uint8_t Sound_LoadEffect(const char* fileName, const char* name, uint16_t minDistanceOrRolloff);
void Sound_UnloadAllEffects(void);
int  Sound_CountPlayingInstances(int soundId);
int  Sound_FindEffectByName(const char* name);
int  Sound_QueueEffect(int soundId, int param2, int loop, int priority, int volume, int pan, int pitch,
					   unsigned int sourceObjOrPointRef);
uint8_t Sound_QueueEffectAtWorldPosition(int soundId, int param2, int loop, int priority, int volume, int pan,
									 int pitch, int worldX, int worldY, int worldZ);
uint8_t Sound_PlayEffectNow(int soundId, int param2, int loop, int priority, int volume, int pan, int pitch,
							unsigned int sourceObjOrPointRef, uint8_t hasWorldPosition, int worldX, int worldY,
							int worldZ);
uint8_t Sound_DuplicateOrCreateHardwareBuffer(IDirectSoundBuffer* sourceBuffer,
											  IDirectSoundBuffer** outBuffer);
int  Sound_SetEffectCurrentPriority(int soundId, int priority);
uint8_t Sound_SetLatestInstanceFrequency(int soundId, int frequency);
uint8_t Sound_SetLatestInstancePan(int soundId, int pan);
uint8_t Sound_SetLatestInstanceVolume(int soundId, int volume);
uint8_t Sound_StopOldestInstance(int soundId);
uint8_t Sound_FlushQueuedEffects(void);
void Sound_Update3DListenerAndSources(void);
uint8_t Sound_StopAllInstances(void);
int  DirectSound_VolumeToMillibels(int volume0To127);
int DirectSound_ReloadWaveBuffer(IDirectSoundBuffer* buffer, char* fileName);

#ifdef __cplusplus
}
#endif

#endif
