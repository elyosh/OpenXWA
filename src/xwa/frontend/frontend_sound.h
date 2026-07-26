#ifndef XWA_FRONTEND_FRONTEND_SOUND_H
#define XWA_FRONTEND_FRONTEND_SOUND_H

#include "xwa/assets/file_io.h"
#include "xwa/audio/sound.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
typedef struct FrontendSoundBufferRecord {
	char name[64];
	char fileName[256];
	IDirectSoundBuffer* buffer;
	uint8_t priority;
} FrontendSoundBufferRecord;
#pragma pack(pop)

typedef struct FrontendSoundVoice {
	int bufferIndex;
	int playSerial;
	IDirectSoundBuffer* buffer;
} FrontendSoundVoice;

enum { FRONTEND_SOUND_VOICE_COUNT = 12 };

extern void* g_frontendDirectSound;
extern IDirectSoundBuffer* g_frontendPrimarySoundBuffer;
extern void* g_waveFileTempBuffer;
extern FrontendSoundBufferRecord* g_frontendSoundBuffers;
extern int g_frontendSoundBufferCount;
extern int g_frontendActiveVoiceCount;
extern int g_frontendSoundPlaySerial;
extern FrontendSoundVoice* g_frontendSoundVoices;

int FrontendSound_InitDirectSound(void* hwnd);
int FrontendSound_ShutdownDirectSound(void);
int FrontendSound_LoadList(char* fileName);
int FrontendSound_UnloadList(char* fileName);
void* FrontendSound_GetDirectSound(void);
void DirectSound_StopBuffer(void* buffer);
int DirectSound_LockBuffer(IDirectSoundBuffer* buffer, uint32_t offset, uint32_t bytes, void** audioPtr1,
						   uint32_t* audioBytes1, void** audioPtr2, uint32_t* audioBytes2);
void DirectSound_UnlockBuffer(IDirectSoundBuffer* buffer, void* audioPtr1, uint32_t audioBytes1,
							  void* audioPtr2, uint32_t audioBytes2);
uint32_t DirectSound_GetPlayCursor(IDirectSoundBuffer* buffer);
int FrontendSound_ReleaseForFlight(void);
int FrontendSound_RecreateAfterFlight(void* hwnd);
int FrontendSound_LoadSound(char* fileName, char* soundName);
int FrontendSound_LoadSoundFile(char* fileName, char* soundName, int create3DFlags);
int FrontendSound_UnloadBufferByName(char* soundName);
int FrontendSound_GetPlayingCount(char* name);
int FrontendSound_PlayUISound(char* soundName, int allowRestartExisting, int loop, int priority,
							  int volume0To127, int pan0To127);
int FrontendSound_StopOldestVoiceByName(char* name);
int FrontendSound_FindBufferByName(char* name);
int FrontendSound_BinarySearchBufferByName(FrontendSoundBufferRecord* records, int lastIndex, char* name);
void FrontendSound_InsertSortedBuffer(FrontendSoundBufferRecord* record);
void FrontendSound_RemoveBufferRecord(int bufferIndex);
IDirectSoundBuffer* DirectSound_LoadWaveBuffer(void* directSound, char* fileName, int create3DFlags);
XwaFile* FrontendSound_OpenAsset(const char* fileName, const char* mode);
IDirectSoundBuffer* DirectSound_LoadWaveBufferIntoPtr(IDirectSoundBuffer** outBuffer, char* fileName,
													  int create3DFlags);
void DirectSound_PlayBuffer(IDirectSoundBuffer* buffer, uint32_t position, int loop, int volume0To127);
void DirectSound_ReleaseBufferPtr(IDirectSoundBuffer** bufferPtr);
int DirectSound_CreateWaveBuffer(void* directSound, IDirectSoundBuffer** outBuffer, uint32_t bufferBytes,
								 DSWaveFormat* format, int create3DFlags);
unsigned int DirectSound_CreateStreamingWaveBuffer(void* directSound, IDirectSoundBuffer** outBuffer,
												   uint32_t bufferBytes, uint32_t* outDataOffset,
												   unsigned int streamHandle);

#ifdef __cplusplus
}
#endif

#endif
