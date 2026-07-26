#ifndef XWA_FRONTEND_FRONTEND_WAVE_STREAM_H
#define XWA_FRONTEND_FRONTEND_WAVE_STREAM_H

#include "xwa/audio/sound.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern char g_pendingVoiceWav[256];
extern int g_waveStreamDataOffset;
extern uint8_t g_waveStreamSavedRefillEnabled;
extern int g_waveStreamPlayAheadBytes;
extern uint32_t g_waveStreamPlayedBytes;
extern uint8_t g_waveStreamRefillEnabled;
extern uint8_t g_waveStreamDrainAfterEof;
extern int g_waveStreamWriteOffset;
extern uint8_t g_waveStreamLoopMode;
extern int g_waveStreamReadChunkBytes;
extern int g_waveStreamPlaying;
extern uint32_t g_waveStreamLastPlayCursor;
extern uint8_t g_waveStreamIsStreaming;
extern int g_waveStreamCurrentPlayCursor;
extern IDirectSoundBuffer* g_waveStreamBuffer;
extern void* g_waveStreamReadBuffer;
extern char g_waveStreamPauseDepth;

int FrontendWaveStream_PlayWaveFile(char* fileName, int loopMode, char driveSelect);
uint32_t FrontendWaveStream_Update(int unused);
void FrontendWaveStream_RefillBuffer(uint32_t bytesAvailableBeforePlayCursor);
int FrontendWaveStream_IsPaused(void);
void FrontendWaveStream_Pause(void);
void FrontendWaveStream_Resume(void);
int FrontendWaveStream_IsPlaying(void);
void FrontendWaveStream_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
