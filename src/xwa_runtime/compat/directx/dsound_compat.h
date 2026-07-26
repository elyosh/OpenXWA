#ifndef XWA_RUNTIME_COMPAT_DIRECTX_DSOUND_COMPAT_H
#define XWA_RUNTIME_COMPAT_DIRECTX_DSOUND_COMPAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Port-owned DirectSound compatibility shim.
 *
 * The recovered XWA audio code (flight SFX, frontend UI sound, and the iMUSE
 * music engine) was ported to call DirectSound COM objects through their
 * vtables. This shim provides those objects with real C vtables laid out at the
 * exact DirectSound ABI indices, backed by the generic Aeron mixer. It is the
 * single replacement for dsound.dll / A3D: recovered code keeps issuing the
 * same vtable calls, and this layer translates them into Aeron clips, voices
 * and streams.
 *
 * HRESULT convention: methods return 0 (DS_OK) on success and a negative value
 * on failure, matching the original `result >= 0` success tests. */

/* WAVEFORMATEX-compatible PCM format descriptor (matches the on-disk WAV
 * `fmt ` chunk layout). */
#pragma pack(push, 1)
typedef struct DSWaveFormat {
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	uint16_t cbSize;
} DSWaveFormat;

/* DSBUFFERDESC-compatible. Mirrors imsound.c's ImDSBufferDescCompat field for
 * field, so both consumers can share the shim's CreateSoundBuffer. */
typedef struct DSBufferDesc {
	uint32_t      dwSize;
	uint32_t      dwFlags;
	uint32_t      dwBufferBytes;
	uint32_t      dwReserved;
	DSWaveFormat* lpwfxFormat;
} DSBufferDesc;

typedef struct DSBufferCaps {
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwBufferBytes;
	uint32_t dwUnlockTransferRate;
	uint32_t dwPlayCpuOverhead;
} DSBufferCaps;

/* DSCAPS-compatible device capability descriptor used by the flight sound
 * engine to determine the number of hardware 3D buffers. */
typedef struct DSoundDeviceCaps {
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwMinSecondarySampleRate;
	uint32_t dwMaxSecondarySampleRate;
	uint32_t dwPrimaryBuffers;
	uint32_t dwMaxHwMixingAllBuffers;
	uint32_t dwFreeHwMixingAllBuffers;
	uint32_t dwMaxHwMixingStaticBuffers;
	uint32_t dwFreeHwMixingStaticBuffers;
	uint32_t dwMaxHwMixingStreamingBuffers;
	uint32_t dwFreeHwMixingStreamingBuffers;
	uint32_t dwMaxHw3DAllBuffers;
	uint32_t dwFreeHw3DAllBuffers;
	uint32_t dwMaxHw3DStaticBuffers;
	uint32_t dwFreeHw3DStaticBuffers;
	uint32_t dwMaxHw3DStreamingBuffers;
	uint32_t dwFreeHw3DStreamingBuffers;
	uint32_t dwTotalHwMemBytes;
	uint32_t dwFreeHwMemBytes;
	uint32_t dwMaxContigFreeHwMemBytes;
	uint32_t dwUnlockTransferRateHwBuffers;
	uint32_t dwPlayCpuOverheadSwBuffers;
	uint32_t dwReserved1;
	uint32_t dwReserved2;
} DSoundDeviceCaps;
#pragma pack(pop)

/* DirectSound buffer capability/playback flags actually used by recovered code. */
enum {
	DSBCAPS_PRIMARYBUFFER_XWA = 0x00000001,
	DSBCAPS_STATIC_XWA        = 0x00000002,
	DSBCAPS_LOCHARDWARE_XWA   = 0x00000004,
	DSBCAPS_CTRL3D_XWA        = 0x00000010,
	DSBCAPS_CTRLFREQUENCY_XWA = 0x00000020,
	DSBCAPS_CTRLPAN_XWA       = 0x00000040,
	DSBCAPS_CTRLVOLUME_XWA    = 0x00000080,
	DSBCAPS_MUTE3DATMAXDISTANCE_XWA = 0x00020000,
	DSBLOCK_ENTIREBUFFER_XWA  = 0x00000002,
	DSBPLAY_LOOPING_XWA       = 0x00000001,
	DSBSTATUS_PLAYING_XWA     = 0x00000001,
	DSBSTATUS_LOOPING_XWA     = 0x00000004,
	/* DirectSound DSBCAPS_GETCURRENTPOSITION2: the buffer's owner polls the play
	 * cursor, i.e. it is used as a streaming buffer. iMUSE already sets this on
	 * its music buffer; the shim uses it to back a buffer with an Aeron ring
	 * instead of a static clip. */
	DSBCAPS_GETCURRENTPOSITION2_XWA = 0x00010000
};

/* COM-style interface id (GUID layout). The recovered flight code passes these
 * to QueryInterface to obtain the DirectSound3D sub-interfaces. */
typedef struct DSCompatGuid {
	uint32_t data1;
	uint16_t data2;
	uint16_t data3;
	uint8_t  data4[8];
} DSCompatGuid;

extern const DSCompatGuid IID_IDirectSound3DBuffer;
extern const DSCompatGuid IID_IDirectSound3DListener;

/* Creates a port-owned IDirectSound shim device backed by the Aeron mixer.
 * Replaces DirectSoundCreate / A3D_CreateDirectSound. Returns >= 0 on success
 * and stores the opaque device pointer (callable through its vtable, e.g.
 * lpVtbl->CreateSoundBuffer) in *outDevice. */
int DSoundCompat_Create(void** outDevice);

#ifdef __cplusplus
}
#endif

#endif
