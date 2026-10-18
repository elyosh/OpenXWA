#include "xwa/frontend/frontend_sound.h"
#include "aeron/compat/dsound.h"
#include "aeron/compat/win_types.h"
#include "aeron/log.h"
#include "xwa/assets/file_io.h"
#include "xwa/audio/music.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_file_stream.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/frontend_wave_stream.h"
#include "xwa/util/memory.h"
#include "xwa/util/time.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef XWA_MODERN
int __stdcall A3D_CreateDirectSound(const DSCompatGuid* deviceGuid, void** outDevice, void* outerUnknown);
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#endif

enum { DSERR_BUFFERLOST = (int)0x88780096u };

// GLOBAL: XWA 0x9F7F0F
void* g_frontendDirectSound;
// GLOBAL: XWA 0x9F7F13
IDirectSoundBuffer* g_frontendPrimarySoundBuffer;
// GLOBAL: XWA 0x782FE8
void* g_waveFileTempBuffer;
// GLOBAL: XWA 0x9F7F17
FrontendSoundBufferRecord* g_frontendSoundBuffers;
// GLOBAL: XWA 0x9F7F1F
int g_frontendSoundBufferCount;
// GLOBAL: XWA 0x9F7F23
int g_frontendActiveVoiceCount;
// GLOBAL: XWA 0x9F7F27
int g_frontendSoundPlaySerial;
// GLOBAL: XWA 0x9F7F1B
FrontendSoundVoice* g_frontendSoundVoices;

// FUNCTION: XWA 0x55D9B0
int FrontendSound_LoadList(char* fileName) {
	XwaFile* stream;
	char soundFileName[256];
	char soundName[256];

	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "r");
	if (stream == NULL) {
		return 0;
	}

#ifdef XWA_MODERN
	{
		char line[256];

		if (File_ReadLine(stream, line, 255)) {
			while (File_ReadLine(stream, line, sizeof(line))) {
				int parsedCount;

				parsedCount = sscanf(line, "%255s %255s", soundFileName, soundName);
				if (parsedCount == EOF) {
					continue;
				}
				if (parsedCount != 2) {
					File_Close(stream);
					return 0;
				}
				FrontendSound_LoadSound(AERON_VFS_ROOT_ASSET, soundFileName, soundName);
			}
		}
	}
#else
	if (fgets(soundFileName, 255, (FILE*)stream) != NULL) {
		while (stream != NULL) {
			int parsedCount;

			parsedCount = fscanf((FILE*)stream, "%s %s\n", soundFileName, soundName);
			if (parsedCount == EOF) {
				break;
			}
			if (parsedCount != 2) {
				File_Close(stream);
				return 0;
			}
			FrontendSound_LoadSound(soundFileName, soundName);
		}
	}
#endif

	File_Close(stream);
	return 1;
}

// FUNCTION: XWA 0x55DA60
int FrontendSound_UnloadList(char* fileName) {
	XwaFile* stream;
	char soundFileName[256];
	char soundName[256];

	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "r");
	if (stream == NULL) {
		return 0;
	}

#ifdef XWA_MODERN
	{
		char line[256];

		if (File_ReadLine(stream, line, 255)) {
			while (File_ReadLine(stream, line, sizeof(line))) {
				int parsedCount;

				parsedCount = sscanf(line, "%255s %255s", soundFileName, soundName);
				if (parsedCount == EOF) {
					continue;
				}
				if (parsedCount != 2) {
					File_Close(stream);
					return 0;
				}
				FrontendSound_UnloadBufferByName(soundName);
			}
		}
	}
#else
	if (fgets(soundFileName, 255, (FILE*)stream) != NULL) {
		while (stream != NULL) {
			int parsedCount;

			parsedCount = fscanf((FILE*)stream, "%s %s\n", soundFileName, soundName);
			if (parsedCount == EOF) {
				break;
			}
			if (parsedCount != 2) {
				File_Close(stream);
				return 0;
			}
			FrontendSound_UnloadBufferByName(soundName);
		}
	}
#endif

	File_Close(stream);
	return 1;
}

// FUNCTION: XWA 0x538620
void* FrontendSound_GetDirectSound(void) { return g_frontendDirectSound; }

// FUNCTION: XWA 0x539550
void DirectSound_StopBuffer(void* buffer) {
	void** vtbl;

	if (buffer == 0) {
		return;
	}

	vtbl = *(void***)buffer;
	((int(AERON_DXAPI*)(void*))vtbl[18])(buffer);
}

// FUNCTION: XWA 0x539560
int DirectSound_LockBuffer(IDirectSoundBuffer* buffer, uint32_t offset, uint32_t bytes, void** audioPtr1,
						   uint32_t* audioBytes1, void** audioPtr2, uint32_t* audioBytes2) {
	void** vtbl;

	if (buffer != NULL) {
		vtbl = *(void***)buffer;
		if (((int(AERON_DXAPI*)(IDirectSoundBuffer*, uint32_t, uint32_t, void**, uint32_t*, void**, uint32_t*,
								uint32_t))vtbl[11])(buffer, offset, bytes, audioPtr1, audioBytes1, audioPtr2,
													audioBytes2, 0) >= 0) {
			return 1;
		}
	}
	return 0;
}

// FUNCTION: XWA 0x5395A0
void DirectSound_UnlockBuffer(IDirectSoundBuffer* buffer, void* audioPtr1, uint32_t audioBytes1,
							  void* audioPtr2, uint32_t audioBytes2) {
	void** vtbl;

	if (buffer != NULL) {
		vtbl = *(void***)buffer;
		((int(AERON_DXAPI*)(IDirectSoundBuffer*, void*, uint32_t, void*, uint32_t))vtbl[19])(
			buffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);
	}
}

// FUNCTION: XWA 0x5395D0
uint32_t DirectSound_GetPlayCursor(IDirectSoundBuffer* buffer) {
	uint32_t playCursor;

	playCursor = 0;
	if (buffer != NULL) {
#ifdef XWA_MODERN
		uint32_t writeCursor;

		buffer->lpVtbl->GetCurrentPosition(buffer, &playCursor, &writeCursor);
#else
		buffer->lpVtbl->GetCurrentPosition(buffer, &playCursor, (uint32_t*)&buffer);
#endif
	}
	return playCursor;
}

// FUNCTION: XWA 0x5383D0
int FrontendSound_ReleaseForFlight(void) {
	/* TODO: Reimplement FrontendSound_ReleaseForFlight @ 0x5383D0. */
	return 1;
}

// FUNCTION: XWA 0x538460
int FrontendSound_RecreateAfterFlight(void* hwnd) {
	(void)hwnd;

	/* TODO: Reimplement FrontendSound_RecreateAfterFlight @ 0x538460. */
	return 1;
}

// FUNCTION: XWA 0x538630
#ifdef XWA_MODERN
int FrontendSound_LoadSound(AeronVfsRoot root, char* fileName, char* soundName) {
	return FrontendSound_LoadSoundFile(root, fileName, soundName, 0);
}
#else
int FrontendSound_LoadSound(char* fileName, char* soundName) {
	return FrontendSound_LoadSoundFile(fileName, soundName, 0);
}
#endif

// FUNCTION: XWA 0x538650
#ifdef XWA_MODERN
int FrontendSound_LoadSoundFile(AeronVfsRoot root, char* fileName, char* soundName, int create3DFlags) {
#else
int FrontendSound_LoadSoundFile(char* fileName, char* soundName, int create3DFlags) {
#endif
	FrontendSoundBufferRecord record;
	IDirectSoundBuffer* buffer;
	int wasBackBufferLocked;

	if (fileName[0] == '\0' || soundName[0] == '\0' || g_frontendSoundBufferCount >= 128 ||
		g_frontendDirectSound == 0) {
		return 0;
	}

	if (FrontendSound_FindBufferByName(soundName) != -1) {
		return 1;
	}

	wasBackBufferLocked = g_backBufferLocked.word & 0xff;
	FrontendDisplay_UnlockBackBuffer();
#ifdef XWA_MODERN
	buffer = DirectSound_LoadWaveBuffer(g_frontendDirectSound, root, fileName, create3DFlags);
#else
	buffer = DirectSound_LoadWaveBuffer(g_frontendDirectSound, fileName, create3DFlags);
#endif
	record.buffer = buffer;
	if (buffer == 0) {
		if (wasBackBufferLocked) {
			g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
		}
		return 0;
	}

	buffer->lpVtbl->SetCurrentPosition(buffer, 0);
	strncpy(record.name, soundName, sizeof(record.name));
	record.name[63] = '\0';
	strncpy(record.fileName, fileName, sizeof(record.fileName));
	record.fileName[191] = '\0';
	record.priority = 0;
	FrontendSound_InsertSortedBuffer(&record);

	if (wasBackBufferLocked) {
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	}

	return record.buffer != 0;
}

// FUNCTION: XWA 0x539320
static int DirectSound_FindFormatAndDataChunks(void* riffData, DSWaveFormat** outFormat, void** outSamples,
											   unsigned int* outSampleBytes) {
	enum {
		RIFF_MAGIC = 0x46464952u, /* 'RIFF' */
		WAVE_MAGIC = 0x45564157u, /* 'WAVE' */
		FMT_CHUNK = 0x20746D66u,  /* 'fmt ' */
		DATA_CHUNK = 0x61746164u  /* 'data' */
	};
	uint32_t* cursor = (uint32_t*)riffData;
	uint32_t riffId;
	uint32_t riffSize;
	uint32_t waveId;
	uint32_t* end;

	if (outFormat) {
		*outFormat = NULL;
	}
	if (outSamples) {
		*outSamples = NULL;
	}
	if (outSampleBytes) {
		*outSampleBytes = 0;
	}

	riffId = *cursor++;
	riffSize = *cursor++;
	waveId = *cursor++;
	if (riffId != RIFF_MAGIC || waveId != WAVE_MAGIC) {
		return 0;
	}

	end = (uint32_t*)((char*)cursor + riffSize - 4);
	while (cursor < end) {
		uint32_t chunkId = *cursor++;
		uint32_t chunkSize = *cursor++;
		void* chunkData = cursor;

		switch (chunkId) {
			case DATA_CHUNK:
				if ((outSamples && !*outSamples) || (outSampleBytes && !*outSampleBytes)) {
					if (outSamples) {
						*outSamples = chunkData;
					}
					if (outSampleBytes) {
						*outSampleBytes = chunkSize;
					}
					if (!outFormat || *outFormat) {
						return 1;
					}
				}
				break;

			case FMT_CHUNK:
				if (outFormat && !*outFormat) {
					if (chunkSize < 0xE) {
						return 0;
					}
					*outFormat = (DSWaveFormat*)chunkData;
					if ((!outSamples || *outSamples) && (!outSampleBytes || *outSampleBytes)) {
						return 1;
					}
				}
				break;
		}

		cursor = (uint32_t*)((char*)cursor + ((chunkSize + 1) & 0xFFFFFFFEu));
	}

	return 0;
}

// FUNCTION: XWA 0x539190
#ifdef XWA_MODERN
static int DirectSound_LoadFileAndFindAudioData(int unused, AeronVfsRoot root, char* fileName,
												DSWaveFormat** outFormat, void** outSamples,
												unsigned int* outSampleBytes) {
#else
static int DirectSound_LoadFileAndFindAudioData(int unused, char* fileName, DSWaveFormat** outFormat,
												void** outSamples, unsigned int* outSampleBytes) {
#endif
	XwaFile* stream;
	void* fileData;
	size_t size;

#ifdef XWA_MODERN
	(void)unused;
#endif

#ifdef XWA_MODERN
	stream = File_Open(root, fileName, "rb");
#else
	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
#endif
	if (stream == NULL) {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.audio", "sound file not found: %s", fileName);
#endif
		return 0;
	}

	File_Seek(stream, 0, 2);
	size = (size_t)File_Tell(stream);
	File_Seek(stream, 0, 0);

	if (g_waveFileTempBuffer) {
		Mem_Free(g_waveFileTempBuffer);
		g_waveFileTempBuffer = NULL;
	}
	fileData = g_waveFileTempBuffer = Mem_Alloc(size);
	if (g_waveFileTempBuffer != NULL && File_ReadCount(stream, g_waveFileTempBuffer, size) &&
		DirectSound_FindFormatAndDataChunks(fileData, outFormat, outSamples, outSampleBytes)) {
		File_Close(stream);
		return 1;
	}

#ifdef XWA_MODERN
	Aeron_LogError("xwa.audio", "could not read/parse WAV: %s", fileName);
#endif
	File_Close(stream);
	return 0;
}

// FUNCTION: XWA 0x539260
static int DirectSound_CopyWaveDataToBuffer(IDirectSoundBuffer* buffer, const void* sampleData,
											unsigned int sampleBytes) {
	void* audioPtr1;
	uint32_t audioBytes1;
	void* audioPtr2;
	uint32_t audioBytes2;

	if (buffer == NULL || sampleData == NULL || sampleBytes == 0) {
		return 0;
	}
	if (buffer->lpVtbl->Lock(buffer, 0, sampleBytes, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2, 0) <
		0) {
		return 0;
	}

	memcpy(audioPtr1, sampleData, audioBytes1);
	if (audioBytes2 != 0) {
		memcpy(audioPtr2, (const char*)sampleData + audioBytes1, audioBytes2);
	}
	buffer->lpVtbl->Unlock(buffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);
	return 1;
}

// FUNCTION: XWA 0x539000
#ifdef XWA_MODERN
IDirectSoundBuffer* DirectSound_LoadWaveBuffer(void* directSound, AeronVfsRoot root, char* fileName,
											   int create3DFlags) {
#else
IDirectSoundBuffer* DirectSound_LoadWaveBuffer(void* directSound, char* fileName, int create3DFlags) {
#endif
	IDirectSound* device = (IDirectSound*)directSound;
	DSBufferDesc desc;
	DSWaveFormat* format = NULL;
	void* sampleData = NULL;
	unsigned int sampleBytes = 0;
	IDirectSoundBuffer* buffer = NULL;

	if (g_waveFileTempBuffer) {
		Mem_Free(g_waveFileTempBuffer);
		g_waveFileTempBuffer = NULL;
	}

#ifdef XWA_MODERN
	if (DirectSound_LoadFileAndFindAudioData(0, root, fileName, &format, &sampleData, &sampleBytes)) {
#else
	if (DirectSound_LoadFileAndFindAudioData(0, fileName, &format, &sampleData, &sampleBytes)) {
#endif
		memset(&desc, 0, sizeof(desc));
		desc.dwSize = 20;
		if (g_sound3DEnabled) {
			desc.dwFlags = create3DFlags != 0 ? 131250u : 131258u;
		} else {
			desc.dwFlags = create3DFlags != 0 ? 194u : 234u;
		}
		desc.dwBufferBytes = sampleBytes;
		desc.lpwfxFormat = format;

		if (device->lpVtbl->CreateSoundBuffer(device, &desc, &buffer, NULL) >= 0) {
			if (DirectSound_CopyWaveDataToBuffer(buffer, sampleData, sampleBytes)) {
				goto done;
			}
			buffer->lpVtbl->Release(buffer);
		}
		buffer = NULL;
	}

done:
	if (g_waveFileTempBuffer) {
		Mem_Free(g_waveFileTempBuffer);
		g_waveFileTempBuffer = NULL;
	}
	return buffer;
}

// FUNCTION: XWA 0x539740
IDirectSoundBuffer* DirectSound_LoadWaveBufferIntoPtr(IDirectSoundBuffer** outBuffer, char* fileName,
													  int create3DFlags) {
#ifdef XWA_MODERN
	IDirectSoundBuffer* buffer =
		DirectSound_LoadWaveBuffer(g_frontendDirectSound, AERON_VFS_ROOT_ASSET, fileName, create3DFlags);
#else
	IDirectSoundBuffer* buffer = DirectSound_LoadWaveBuffer(g_frontendDirectSound, fileName, create3DFlags);
#endif
	*outBuffer = buffer;
	return buffer;
}

// FUNCTION: XWA 0x539500
void DirectSound_PlayBuffer(IDirectSoundBuffer* buffer, uint32_t position, int loop, int volume0To127) {
	if (buffer == NULL) {
		return;
	}
	buffer->lpVtbl->SetCurrentPosition(buffer, position);
	{
		int volume = DirectSound_VolumeToMillibels(volume0To127);

		buffer->lpVtbl->SetVolume(buffer, volume);
	}
	buffer->lpVtbl->SetPan(buffer, 0);
	if (loop) {
		loop = 1;
	}
	buffer->lpVtbl->Play(buffer, 0, 0, (uint32_t)loop);
}

// FUNCTION: XWA 0x5394E0
void DirectSound_ReleaseBufferPtr(IDirectSoundBuffer** bufferPtr) {
	if (bufferPtr != NULL && *bufferPtr != NULL) {
		(*bufferPtr)->lpVtbl->Release(*bufferPtr);
		*bufferPtr = NULL;
	}
}

// FUNCTION: XWA 0x539420
int DirectSound_CreateWaveBuffer(void* directSound, IDirectSoundBuffer** outBuffer, uint32_t bufferBytes,
								 DSWaveFormat* format, int create3DFlags) {
	IDirectSound* device = (IDirectSound*)directSound;
	DSBufferDesc desc;
	DSWaveFormat defaultFormat;
	int result;

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = 20;
	desc.dwFlags = create3DFlags != 0 ? 194u : 234u;
	desc.dwBufferBytes = bufferBytes;
	if (format == NULL) {
#ifdef XWA_MODERN
		memset(&defaultFormat, 0, sizeof(defaultFormat));
#else
		memset(&defaultFormat, 0, sizeof(defaultFormat) - sizeof(defaultFormat.cbSize));
#endif
		defaultFormat.wFormatTag = 1;
		defaultFormat.nChannels = 1;
		defaultFormat.nSamplesPerSec = 22050;
		defaultFormat.nAvgBytesPerSec = 22050;
		defaultFormat.wBitsPerSample = 8;
		defaultFormat.nBlockAlign = 1;
		desc.lpwfxFormat = &defaultFormat;
	} else {
		desc.lpwfxFormat = format;
	}

	result = device->lpVtbl->CreateSoundBuffer(device, &desc, outBuffer, NULL);
	if (result < 0) {
		*outBuffer = NULL;
	}
	return result;
}

// FUNCTION: XWA 0x539600
unsigned int DirectSound_CreateStreamingWaveBuffer(void* directSound, IDirectSoundBuffer** outBuffer,
												   uint32_t bufferBytes, uint32_t* outDataOffset,
												   unsigned int streamHandle) {
	DSWaveFormat* format;
	void* samples;
	unsigned int sampleBytes;
	uint8_t* header;
	unsigned int bytesRead;
	unsigned int initialBytes;

	if (outBuffer == NULL) {
		return 0xFFFFFFFFu;
	}

	header = (uint8_t*)Mem_Alloc(0x5A); /* 90-byte RIFF/WAVE header probe */
	do {
		bytesRead = FrontendFileStream_ReadBytes(streamHandle, header, 0, 0x5A, 1);
	} while (bytesRead == 0xFFFFFFFFu);
	if (bytesRead == 90) {
		DirectSound_FindFormatAndDataChunks(header, &format, &samples, &sampleBytes);
		DirectSound_CreateWaveBuffer(directSound, outBuffer, bufferBytes, format, 0);
		if (*outBuffer == NULL) {
			Mem_Free(header);
			return 0xFFFFFFFFu;
		}

		if (outDataOffset != NULL) {
			*outDataOffset = (uint32_t)((uintptr_t)samples - (uintptr_t)header);
		}
		initialBytes = (unsigned int)((uintptr_t)header - (uintptr_t)samples + 90u);

		if (initialBytes != 0) {
			uint32_t lockBytes1;
			void* lockPtr1;
			void* lockPtr2;
			uint32_t lockBytes2;
			if (((int(AERON_DXAPI*)(IDirectSoundBuffer*, uint32_t, uint32_t, void**, uint32_t*, void**,
									uint32_t*, uint32_t))(*outBuffer)
					 ->lpVtbl->Lock)(*outBuffer, 0, initialBytes, &lockPtr1, &lockBytes1, &lockPtr2,
									 &lockBytes2, 0) >= 0) {
				unsigned int copyBytes = lockBytes1 < initialBytes ? lockBytes1 : initialBytes;
				memcpy(samples, lockPtr1, copyBytes);
				copyBytes = lockBytes1 < initialBytes ? lockBytes1 : initialBytes;
				((int(AERON_DXAPI*)(IDirectSoundBuffer*, void*, uint32_t, void*, uint32_t))(*outBuffer)
					 ->lpVtbl->Unlock)(*outBuffer, lockPtr1, copyBytes, lockPtr2, 0);
				Mem_Free(header);
				return initialBytes;
			}
			initialBytes = 0xFFFFFFFFu;
		}
	} else {
		initialBytes = 0xFFFFFFFFu;
	}

	Mem_Free(header);
	return initialBytes;
}

// FUNCTION: XWA 0x538100
int FrontendSound_InitDirectSound(void* hwnd) {
	DSoundDeviceCaps deviceCaps;
	DSBufferDesc primaryDesc;
	DSWaveFormat primaryFormat;
	int i;
	int use3D;

	if (g_frontendDirectSound == NULL) {
		if (g_frontendSoundVoices == NULL)
			return 0;
		if (g_frontendSoundBuffers == NULL)
			return 0;

		for (i = 0; i < FRONTEND_SOUND_VOICE_COUNT; ++i) {
			g_frontendSoundVoices[i].bufferIndex = -1;
			g_frontendSoundVoices[i].playSerial = 0;
			g_frontendSoundVoices[i].buffer = NULL;
		}
		g_frontendActiveVoiceCount = 0;
		g_frontendSoundBufferCount = 0;
		g_frontendSoundPlaySerial = 0;
		for (i = 0; i < 128; ++i) {
			g_frontendSoundBuffers[i].buffer = NULL;
			g_frontendSoundBuffers[i].name[0] = '\0';
		}

#ifdef XWA_MODERN
		/* The modern backend creates the software DirectSound device. */
		if (DirectSoundCreate(NULL, &g_frontendDirectSound, NULL) != 0)
			return 0;
#else
		if (A3D_CreateDirectSound(NULL, &g_frontendDirectSound, NULL) < 0) {
			g_OutputDebugStringA("A3dCreate failed.\n");
			if (g_frontendDirectSound == NULL && DirectSoundCreate(NULL, &g_frontendDirectSound, NULL) != 0)
				return 0;
		} else {
			g_OutputDebugStringA("A3D Object created successfully.\n");
		}
#endif
		memset(&deviceCaps, 0, sizeof(deviceCaps));
		deviceCaps.dwSize = sizeof(deviceCaps);
		((IDirectSound*)g_frontendDirectSound)->lpVtbl->GetCaps(g_frontendDirectSound, &deviceCaps);

		use3D = deviceCaps.dwMaxHw3DAllBuffers != 0;
		memset(&primaryDesc, 0, sizeof(primaryDesc));
		primaryDesc.dwFlags = use3D ? DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D : DSBCAPS_PRIMARYBUFFER;
		primaryDesc.dwSize = 20;
		primaryDesc.dwBufferBytes = 0;
		primaryDesc.lpwfxFormat = NULL;
		if (((IDirectSound*)g_frontendDirectSound)
				->lpVtbl->CreateSoundBuffer(g_frontendDirectSound, &primaryDesc,
											&g_frontendPrimarySoundBuffer, NULL) < 0) {
			FrontendSound_ShutdownDirectSound();
			return 0;
		}
		if (((IDirectSound*)g_frontendDirectSound)
				->lpVtbl->SetCooperativeLevel(g_frontendDirectSound, hwnd, 2) < 0) {
			FrontendSound_ShutdownDirectSound();
			return 0;
		}

		memset(&primaryFormat, 0, sizeof(primaryFormat));
		primaryFormat.wFormatTag = 1;
		primaryFormat.nChannels = 2;
		primaryFormat.nSamplesPerSec = 22050;
		primaryFormat.wBitsPerSample = 16;
		primaryFormat.nBlockAlign = 4;
		primaryFormat.nAvgBytesPerSec = 88200;
		g_frontendPrimarySoundBuffer->lpVtbl->SetFormat(g_frontendPrimarySoundBuffer, &primaryFormat);
	}
	return 1;
}

// FUNCTION: XWA 0x538300
int FrontendSound_ShutdownDirectSound(void) {
	int i;
#ifndef XWA_MODERN
	uint32_t startTick;
#endif

	if (g_frontendDirectSound == NULL) {
		return 1;
	}

	Music_Shutdown();
#ifndef XWA_MODERN
	startTick = GetTickCount();
	memset(&i, 0, sizeof(i));
	while (i < 1000) {
		i = (int)(GetTickCount() - startTick);
	}
#endif
	FrontendWaveStream_Shutdown();

#ifdef XWA_MODERN
	if (g_frontendPrimarySoundBuffer) {
		g_frontendPrimarySoundBuffer->lpVtbl->Release(g_frontendPrimarySoundBuffer);
	}
#else
	g_frontendPrimarySoundBuffer->lpVtbl->Release(g_frontendPrimarySoundBuffer);
#endif
	((IDirectSound*)g_frontendDirectSound)->lpVtbl->Release(g_frontendDirectSound);
	g_frontendDirectSound = NULL;

	if (g_frontendSoundBuffers) {
		for (i = 0; i < 128; ++i) {
			g_frontendSoundBuffers[i].buffer = NULL;
			g_frontendSoundBuffers[i].name[0] = '\0';
		}
	}
	if (g_frontendSoundVoices) {
		for (i = 0; i < FRONTEND_SOUND_VOICE_COUNT; ++i) {
			g_frontendSoundVoices[i].bufferIndex = -1;
			g_frontendSoundVoices[i].playSerial = 0;
			g_frontendSoundVoices[i].buffer = NULL;
		}
	}
	g_frontendPrimarySoundBuffer = NULL;
	g_frontendSoundPlaySerial = 0;
	return 1;
}

// FUNCTION: XWA 0x5387A0
int FrontendSound_UnloadBufferByName(char* soundName) {
	int bufferIndex;
	int wasBackBufferLocked;

	if (soundName[0] == '\0') {
		return 0;
	}

	bufferIndex = FrontendSound_FindBufferByName(soundName);
	if (bufferIndex == -1) {
		return 0;
	}

	wasBackBufferLocked = g_backBufferLocked.word & 0xff;
	FrontendDisplay_UnlockBackBuffer();
	while (FrontendSound_StopOldestVoiceByName(g_frontendSoundBuffers[bufferIndex].name) == 1) {
	}

	g_frontendSoundBuffers[bufferIndex].buffer->lpVtbl->Release(g_frontendSoundBuffers[bufferIndex].buffer);
	FrontendSound_RemoveBufferRecord(bufferIndex);

	if (wasBackBufferLocked) {
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	}

	return 1;
}

// FUNCTION: XWA 0x538D40
int FrontendSound_GetPlayingCount(char* name) {
	(void)name;

	/* TODO: Reimplement FrontendSound_GetPlayingCount @ 0x538D40. */
	return 0;
}

// FUNCTION: XWA 0x538850
int FrontendSound_PlayUISound(char* soundName, int allowRestartExisting, int loop, int priority,
							  int volume0To127, int pan0To127) {
	int bufferIndex;
	int wasBackBufferLocked;
	FrontendSoundVoice* voices;
	uint32_t bufferStatus;
	int voiceIndex;
	int lowestPriority;
	int i;
	int clampedVolume;
	int clampedPan;
	int loopFlag;
	int playResult;

	if (g_frontendDirectSound == 0) {
		return 0;
	}
	if (soundName[0] == '\0') {
		return 0;
	}

	bufferIndex = FrontendSound_FindBufferByName(soundName);
	if (bufferIndex == -1) {
		return 0;
	}

	wasBackBufferLocked = g_backBufferLocked.word & 0xff;
	FrontendDisplay_UnlockBackBuffer();

	if (g_frontendActiveVoiceCount == FRONTEND_SOUND_VOICE_COUNT) {
		voices = g_frontendSoundVoices;
		for (voiceIndex = 0; voiceIndex < FRONTEND_SOUND_VOICE_COUNT; ++voiceIndex) {
			if (voices[voiceIndex].bufferIndex == -1) {
				continue;
			}
			if (voices[voiceIndex].buffer->lpVtbl->GetStatus(voices[voiceIndex].buffer, &bufferStatus) == 0 &&
				(bufferStatus & 5) != 0) {
				voices = g_frontendSoundVoices;
				continue;
			}
			g_frontendSoundVoices[voiceIndex].buffer->lpVtbl->Release(
				g_frontendSoundVoices[voiceIndex].buffer);
			g_frontendSoundVoices[voiceIndex].bufferIndex = -1;
			g_frontendSoundVoices[voiceIndex].buffer = NULL;
			{
				int activeVoiceCount;

				activeVoiceCount = g_frontendActiveVoiceCount;
				--activeVoiceCount;
				g_frontendActiveVoiceCount = activeVoiceCount;
			}
			break;
		}

		if (voiceIndex == FRONTEND_SOUND_VOICE_COUNT) {
			lowestPriority = priority;
			for (i = 0; i < FRONTEND_SOUND_VOICE_COUNT; ++i, ++voices) {
				int activeBufferIndex;
				int activePriority;

				activeBufferIndex = voices->bufferIndex;
				activePriority = g_frontendSoundBuffers[activeBufferIndex].priority;
				if (activePriority < lowestPriority) {
					voiceIndex = i;
					lowestPriority = activePriority;
				}
			}

			if (voiceIndex != FRONTEND_SOUND_VOICE_COUNT) {
				g_frontendSoundVoices[voiceIndex].buffer->lpVtbl->Stop(
					g_frontendSoundVoices[voiceIndex].buffer);
				g_frontendSoundVoices[voiceIndex].buffer->lpVtbl->Release(
					g_frontendSoundVoices[voiceIndex].buffer);
				g_frontendSoundVoices[voiceIndex].bufferIndex = -1;
				g_frontendSoundVoices[voiceIndex].buffer = NULL;
				--g_frontendActiveVoiceCount;
			} else {
				if (allowRestartExisting) {
					for (i = 0; i < FRONTEND_SOUND_VOICE_COUNT; ++i) {
						if (g_frontendSoundVoices[i].bufferIndex == bufferIndex) {
							g_frontendSoundVoices[i].buffer->lpVtbl->SetCurrentPosition(
								g_frontendSoundVoices[i].buffer, 0);
							g_frontendSoundVoices[i].playSerial = g_frontendSoundPlaySerial++;
							if (wasBackBufferLocked) {
								g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
							}
							return 1;
						}
					}
				}
				if (wasBackBufferLocked) {
					g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				}
				return 0;
			}
		}
	} else {
		for (voiceIndex = 0; voiceIndex < FRONTEND_SOUND_VOICE_COUNT; ++voiceIndex) {
			if (g_frontendSoundVoices[voiceIndex].bufferIndex == -1) {
				break;
			}
		}
	}

	{
		IDirectSoundBuffer* playBuffer;

		((IDirectSound*)g_frontendDirectSound)
			->lpVtbl->DuplicateSoundBuffer((IDirectSound*)g_frontendDirectSound,
										   g_frontendSoundBuffers[bufferIndex].buffer, &playBuffer);
		if (playBuffer == NULL) {
			if (wasBackBufferLocked) {
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
			}
			return 0;
		}

		playBuffer->lpVtbl->SetCurrentPosition(playBuffer, 0);
		clampedVolume = volume0To127;
		if (clampedVolume > 127) {
			clampedVolume = 127;
		} else if (clampedVolume < 0) {
			clampedVolume = 0;
		}
		clampedVolume = DirectSound_VolumeToMillibels(clampedVolume);
		playBuffer->lpVtbl->SetVolume(playBuffer, clampedVolume);

		clampedPan = pan0To127;
		if (clampedPan > 127) {
			clampedPan = 127;
		}
		if (clampedPan < 0) {
			clampedPan = 0;
		}
		playBuffer->lpVtbl->SetPan(playBuffer, 2000 * (clampedPan - 63) / 63);

		loopFlag = loop == 1;
		playResult = playBuffer->lpVtbl->Play(playBuffer, 0, 0, (uint32_t)loopFlag);
		if (playResult == DSERR_BUFFERLOST) {
			playResult = DirectSound_ReloadWaveBuffer(g_frontendSoundBuffers[bufferIndex].buffer,
													  g_frontendSoundBuffers[bufferIndex].fileName);
			if (playResult == 1) {
				playBuffer->lpVtbl->SetCurrentPosition(playBuffer, 0);
				playResult = playBuffer->lpVtbl->Play(playBuffer, 0, 0, (uint32_t)loopFlag);
				if (playResult == 0) {
					g_frontendSoundVoices[voiceIndex].bufferIndex = bufferIndex;
					g_frontendSoundVoices[voiceIndex].buffer = playBuffer;
					g_frontendSoundVoices[voiceIndex].playSerial = g_frontendSoundPlaySerial++;
					++g_frontendActiveVoiceCount;
				} else {
					playResult = 0;
				}
			}
		} else if (playResult == 0) {
			g_frontendSoundVoices[voiceIndex].bufferIndex = bufferIndex;
			g_frontendSoundVoices[voiceIndex].buffer = playBuffer;
			g_frontendSoundVoices[voiceIndex].playSerial = g_frontendSoundPlaySerial++;
			++g_frontendActiveVoiceCount;
			playResult = 1;
		}

		if (wasBackBufferLocked) {
			g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
		}
		return playResult;
	}
}

// FUNCTION: XWA 0x539100
int DirectSound_ReloadWaveBuffer(IDirectSoundBuffer* buffer, char* fileName) {
	(void)buffer;
	(void)fileName;

	/* TODO: Reimplement DirectSound_ReloadWaveBuffer @ 0x539100. */
	return 0;
}

// FUNCTION: XWA 0x538C50
int FrontendSound_StopOldestVoiceByName(char* name) {
	int bufferIndex;
	int oldestSerial;
	int oldestVoiceIndex;
	int voiceIndex;
	IDirectSoundBuffer* buffer;
	int wasBackBufferLocked;
	int stopResult;

	if (g_frontendDirectSound == 0) {
		return 0;
	}
	if (name[0] == '\0') {
		return 0;
	}

	bufferIndex = FrontendSound_FindBufferByName(name);
	if (bufferIndex == -1) {
		return 0;
	}

	oldestVoiceIndex = -1;
	oldestSerial = g_frontendSoundPlaySerial + 1;
	for (voiceIndex = 0; voiceIndex < FRONTEND_SOUND_VOICE_COUNT; ++voiceIndex) {
		if (g_frontendSoundVoices[voiceIndex].bufferIndex == bufferIndex &&
			g_frontendSoundVoices[voiceIndex].playSerial < oldestSerial) {
			oldestSerial = g_frontendSoundVoices[voiceIndex].playSerial;
			oldestVoiceIndex = voiceIndex;
		}
	}

	if (oldestVoiceIndex == -1) {
		return 0;
	}

	buffer = g_frontendSoundVoices[oldestVoiceIndex].buffer;
	if (buffer == 0) {
		return 0;
	}

	wasBackBufferLocked = g_backBufferLocked.word & 0xff;
	FrontendDisplay_UnlockBackBuffer();
	stopResult = buffer->lpVtbl->Stop(buffer);
	buffer->lpVtbl->Release(buffer);

	g_frontendSoundVoices[oldestVoiceIndex].buffer = 0;
	g_frontendSoundVoices[oldestVoiceIndex].bufferIndex = -1;
	--g_frontendActiveVoiceCount;

	if (wasBackBufferLocked) {
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	}

	return stopResult >= 0;
}

// FUNCTION: XWA 0x538F60
int FrontendSound_BinarySearchBufferByName(FrontendSoundBufferRecord* records, int lastIndex, char* name) {
	FrontendSoundBufferRecord* cursor;
	int baseIndex;

	cursor = records;
	baseIndex = 0;
	while (1) {
		int mid;
		int cmp;

		if (lastIndex < 0) {
			break;
		}
		mid = lastIndex >> 1;
		cmp = strncmp(cursor[mid].name, name, sizeof(cursor[mid].name));
		if (cmp == 0) {
			return mid + baseIndex;
		}
		if (lastIndex <= 0) {
			break;
		}
		if (cmp < 0) {
			lastIndex -= mid + 1;
			baseIndex += mid + 1;
			cursor += mid + 1;
		} else {
			lastIndex = mid - 1;
		}
	}

	return -1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x538F40
int FrontendSound_FindBufferByName(char* name) {
	return FrontendSound_BinarySearchBufferByName(g_frontendSoundBuffers, g_frontendSoundBufferCount - 1,
												  name);
}

// FUNCTION: XWA 0x538DE0
void FrontendSound_InsertSortedBuffer(FrontendSoundBufferRecord* record) {
	int insertIndex;
	int voiceIndex;

	insertIndex = 0;
	while (insertIndex < g_frontendSoundBufferCount &&
		   strncmp(record->name, g_frontendSoundBuffers[insertIndex].name, sizeof(record->name)) >= 0) {
		++insertIndex;
	}

	for (voiceIndex = g_frontendSoundBufferCount; voiceIndex > insertIndex; --voiceIndex) {
		g_frontendSoundBuffers[voiceIndex] = g_frontendSoundBuffers[voiceIndex - 1];
	}
	g_frontendSoundBuffers[insertIndex] = *record;

	++g_frontendSoundBufferCount;
	for (voiceIndex = 0; voiceIndex < FRONTEND_SOUND_VOICE_COUNT; ++voiceIndex) {
		if (g_frontendSoundVoices[voiceIndex].bufferIndex >= insertIndex) {
			++g_frontendSoundVoices[voiceIndex].bufferIndex;
		}
	}
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x538EB0
void FrontendSound_RemoveBufferRecord(int bufferIndex) {
	int i;

	if (bufferIndex < 0 || bufferIndex >= g_frontendSoundBufferCount) {
		return;
	}

	for (i = bufferIndex; i < g_frontendSoundBufferCount - 1; ++i) {
		g_frontendSoundBuffers[i] = g_frontendSoundBuffers[i + 1];
	}

	--g_frontendSoundBufferCount;
	for (i = 0; i < FRONTEND_SOUND_VOICE_COUNT; ++i) {
		if (g_frontendSoundVoices[i].bufferIndex > bufferIndex) {
			--g_frontendSoundVoices[i].bufferIndex;
		}
	}
}
