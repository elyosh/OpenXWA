#include "xwa/frontend/frontend_wave_stream.h"

#include "aeron/log.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_file_stream.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/util/memory.h"

#include <string.h>

static unsigned int FrontendWaveStream_EnsureBuffer(void);
static int FrontendWaveStream_StartFile(const char* path);

// GLOBAL: XWA 0x9EAA20
char g_pendingVoiceWav[256];
// GLOBAL: XWA 0x783404
int g_waveStreamDataOffset;
// GLOBAL: XWA 0x783408
uint8_t g_waveStreamSavedRefillEnabled;
// GLOBAL: XWA 0x78340C
int g_waveStreamPlayAheadBytes;
// GLOBAL: XWA 0x783410
uint32_t g_waveStreamPlayedBytes;
// GLOBAL: XWA 0x783414
uint8_t g_waveStreamRefillEnabled;
// GLOBAL: XWA 0x783418
uint8_t g_waveStreamDrainAfterEof;
// GLOBAL: XWA 0x78341C
int g_waveStreamWriteOffset;
// GLOBAL: XWA 0x783420
uint8_t g_waveStreamLoopMode;
// GLOBAL: XWA 0x783424
int g_waveStreamReadChunkBytes;
// GLOBAL: XWA 0x783428
int g_waveStreamPlaying;
// GLOBAL: XWA 0x78342C
uint32_t g_waveStreamLastPlayCursor;
// GLOBAL: XWA 0x783430
uint8_t g_waveStreamIsStreaming;
// GLOBAL: XWA 0x783434
int g_waveStreamCurrentPlayCursor;
// GLOBAL: XWA 0x783438
IDirectSoundBuffer* g_waveStreamBuffer;
// GLOBAL: XWA 0x78343C
void* g_waveStreamReadBuffer;
// GLOBAL: XWA 0x783440
char g_waveStreamPauseDepth;

enum {
	WAVE_STREAM_BUFFER_BYTES = 500000,
	WAVE_STREAM_TARGET_AHEAD_BYTES = 250000,
	WAVE_STREAM_MAX_READ_BYTES = 64000,
	WAVE_STREAM_MIN_READ_BYTES = 1000,
};

static __inline void FrontendWaveStream_CopyToLockedRegion(void* dst, const void* src, uint32_t byteCount) {
	memcpy(dst, src, byteCount);
}

static __inline void FrontendWaveStream_FillSilence(void* dst, uint32_t byteCount) {
	memset(dst, 0x80, byteCount);
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x558B60
static unsigned int FrontendWaveStream_EnsureBuffer(void) {
	unsigned int initialWriteOffset = 0xFFFFFFFFu;

	if (FrontendSound_GetDirectSound() == NULL) {
		return 0;
	}
	if (g_waveStreamBuffer == NULL) {
		initialWriteOffset = DirectSound_CreateStreamingWaveBuffer(
			FrontendSound_GetDirectSound(), &g_waveStreamBuffer, WAVE_STREAM_BUFFER_BYTES,
			(uint32_t*)&g_waveStreamDataOffset, 1);
		g_waveStreamPlaying = 0;
		g_waveStreamRefillEnabled = 0;
		g_waveStreamSavedRefillEnabled = 0;
		g_waveStreamWriteOffset = 0;
	}
	if (g_waveStreamReadBuffer == NULL) {
		g_waveStreamReadBuffer = Mem_Alloc(WAVE_STREAM_MAX_READ_BYTES);
	}
	return initialWriteOffset;
}

// FUNCTION: XWA 0x5584A0
static int FrontendWaveStream_StartFile(const char* path) {
	unsigned int initialWriteOffset;
	int bufferBytes;
	void* audioPtr1;
	void* audioPtr2;
	uint32_t audioBytes1;
	uint32_t audioBytes2;

	if (FrontendFileStream_QueueFile(1, path)) {
		if (!g_waveStreamLoopMode || FrontendFileStream_QueueFile(1, path)) {
			if (FrontendFileStream_PrimeFromQueuedFile(1, path)) {
				initialWriteOffset = FrontendWaveStream_EnsureBuffer();
				g_waveStreamWriteOffset = (int)initialWriteOffset;
				g_waveStreamPlayedBytes = 0;
				g_waveStreamLastPlayCursor = 0;
				if (initialWriteOffset == 0xFFFFFFFFu) {
					FrontendWaveStream_Shutdown();
					return 0;
				}

				bufferBytes = WAVE_STREAM_BUFFER_BYTES;
				/* Pad the rest of the ring with silence so playback never reads uninitialized
				   data before the refill loop fills it with audio. */
				if (DirectSound_LockBuffer(g_waveStreamBuffer, initialWriteOffset,
										   (uint32_t)bufferBytes - initialWriteOffset, &audioPtr1,
										   &audioBytes1, &audioPtr2, &audioBytes2)) {
					if (audioPtr1) {
						FrontendWaveStream_FillSilence(audioPtr1, audioBytes1);
					}
					if (audioPtr2) {
						FrontendWaveStream_FillSilence(audioPtr2, audioBytes2);
					}
					DirectSound_UnlockBuffer(g_waveStreamBuffer, audioPtr1, audioBytes1, audioPtr2,
											 audioBytes2);

					g_waveStreamRefillEnabled = 1;
					g_waveStreamDrainAfterEof = 0;
					while (g_waveStreamWriteOffset < WAVE_STREAM_TARGET_AHEAD_BYTES) {
						int chunk = WAVE_STREAM_TARGET_AHEAD_BYTES - g_waveStreamWriteOffset;
						unsigned int bytesRead;

						if (chunk >= WAVE_STREAM_MAX_READ_BYTES) {
							chunk = WAVE_STREAM_MAX_READ_BYTES;
						}
						bytesRead = FrontendFileStream_ReadBytes(1, g_waveStreamReadBuffer, 0,
																 (unsigned int)chunk, 1);
						if (bytesRead != 0xFFFFFFFFu) {
							if (bytesRead == 0) {
								break;
							}
							if (DirectSound_LockBuffer(g_waveStreamBuffer, (uint32_t)g_waveStreamWriteOffset,
													   bytesRead, &audioPtr1, &audioBytes1, &audioPtr2,
													   &audioBytes2)) {
								char* readBuffer = (char*)g_waveStreamReadBuffer;
								char* secondReadBuffer = (char*)g_waveStreamReadBuffer;

								if (audioPtr1) {
									if (readBuffer) {
										memcpy(audioPtr1, readBuffer, audioBytes1);
										g_waveStreamWriteOffset += (int)audioBytes1;
										if (g_waveStreamWriteOffset >= bufferBytes) {
											g_waveStreamWriteOffset -= bufferBytes;
										}
									}
								}
								if (audioPtr2) {
									if (secondReadBuffer) {
										memcpy(audioPtr2, secondReadBuffer + audioBytes1, audioBytes2);
										g_waveStreamWriteOffset = (int)audioBytes2;
									}
								}
								DirectSound_UnlockBuffer(g_waveStreamBuffer, audioPtr1, audioBytes1,
														 audioPtr2, audioBytes2);
							}
						}
					}

					DirectSound_PlayBuffer(g_waveStreamBuffer, 0, 1,
										   (g_gameConfig.sfxDatapadVolume && 0x7F) ? 0x7F : 0);
					g_waveStreamPlaying = 1;
					g_waveStreamPlayAheadBytes = WAVE_STREAM_TARGET_AHEAD_BYTES;
					return 1;
				}
				FrontendWaveStream_Shutdown();
				return 0;
			}
			if (g_waveStreamLoopMode) {
				FrontendFileStream_PopHead(1);
			}
		}
		FrontendFileStream_PopHead(1);
	}
	return 0;
}

// FUNCTION: XWA 0x5582E0
int FrontendWaveStream_PlayWaveFile(char* fileName, int loopMode, char driveSelect) {
	XwaFile* stream;
	int fileSize;
	int result;
#ifndef XWA_MODERN
	char filePath[256];
#endif

	if (FrontendSound_GetDirectSound() == NULL) {
		return 0;
	}
	result = 0;
	FrontendWaveStream_Shutdown();
	g_waveStreamPauseDepth = 0;
	g_waveStreamLoopMode = (uint8_t)loopMode;
	if (fileName[0] != '\0') {

#ifdef XWA_MODERN
		/* The port resolves install-relative paths through the asset VFS. */
		(void)driveSelect;
		stream = FrontendSound_OpenAsset(fileName, "rb");
		if (stream == NULL) {
			Aeron_Log("xwa.audio", "wave stream file not found: %s", fileName);
		}
#else
		if (driveSelect == 1) {
			if (File_GetInstallDriveLetter() == '\0') {
				return 0;
			}
			filePath[0] = File_GetInstallDriveLetter();
			filePath[1] = ':';
			filePath[2] = '\0';
			FrontendFileStream_SetSlotDriveAndFastRead(1, 1, File_GetInstallDriveLetter());
		} else {
			if (File_GetCdDriveLetter() == '\0') {
				return 0;
			}
			filePath[0] = File_GetCdDriveLetter();
			filePath[1] = ':';
			filePath[2] = '\\';
			filePath[3] = '\0';
			FrontendFileStream_SetSlotDriveAndFastRead(1, (char)driveSelect, File_GetCdDriveLetter());
		}
		strcat(filePath, fileName);
		stream = (File_Open)(filePath, "rb");
#endif
		if (stream != NULL) {
			File_Seek(stream, 0, 2);
			fileSize = File_Tell(stream);
			File_Close(stream);

			if (fileSize <= 251000) {
				int loop;
				int volume;

#ifdef XWA_MODERN
				DirectSound_LoadWaveBufferIntoPtr(&g_waveStreamBuffer, fileName, 0);
#else
				DirectSound_LoadWaveBufferIntoPtr(&g_waveStreamBuffer, filePath, 0);
#endif
				volume = g_gameConfig.sfxDatapadVolume != 0 ? 0x7F : 0;
				loop = g_waveStreamLoopMode;
				g_waveStreamRefillEnabled = 0;
				g_waveStreamIsStreaming = 0;
				DirectSound_PlayBuffer(g_waveStreamBuffer, 0, loop, volume);
				g_waveStreamPlaying = 1;
				result = 1;
			} else {
				g_waveStreamIsStreaming = 1;
#ifdef XWA_MODERN
				result = FrontendWaveStream_StartFile(fileName);
#else
				result = FrontendWaveStream_StartFile(filePath + 2);
#endif
			}
		}
	}
	return result;
}

// FUNCTION: XWA 0x558760
uint32_t FrontendWaveStream_Update(int unused) {
	uint32_t status;

	(void)unused;

	if (FrontendSound_GetDirectSound() == NULL) {
		return 0;
	}
	if (g_waveStreamBuffer != NULL) {
		if (g_waveStreamIsStreaming) {
			int playCursor;
			int bytesAvailableBeforePlayCursor;
			int lastPlayCursor;
			int readChunkBytes;

			playCursor = (int)DirectSound_GetPlayCursor(g_waveStreamBuffer);
			bytesAvailableBeforePlayCursor = playCursor - g_waveStreamWriteOffset;
			g_waveStreamCurrentPlayCursor = playCursor;
			if (bytesAvailableBeforePlayCursor <= 0) {
				bytesAvailableBeforePlayCursor += WAVE_STREAM_BUFFER_BYTES;
			}

			lastPlayCursor = g_waveStreamLastPlayCursor;
			g_waveStreamPlayedBytes += (uint32_t)(playCursor - lastPlayCursor);
			if (playCursor < lastPlayCursor) {
				g_waveStreamPlayedBytes += WAVE_STREAM_BUFFER_BYTES;
			}
			g_waveStreamLastPlayCursor = (uint32_t)playCursor;

			if (g_waveStreamDrainAfterEof == 1 &&
				bytesAvailableBeforePlayCursor < g_waveStreamPlayAheadBytes) {
				DirectSound_StopBuffer(g_waveStreamBuffer);
				g_waveStreamPlaying = 0;
				g_waveStreamDrainAfterEof = 0;
			}

			if (g_waveStreamRefillEnabled == 1) {
				readChunkBytes = (bytesAvailableBeforePlayCursor - WAVE_STREAM_TARGET_AHEAD_BYTES) & ~3;
				g_waveStreamReadChunkBytes = readChunkBytes;
				if (readChunkBytes > WAVE_STREAM_MAX_READ_BYTES) {
					readChunkBytes = WAVE_STREAM_MAX_READ_BYTES;
					g_waveStreamReadChunkBytes = WAVE_STREAM_MAX_READ_BYTES;
				}
				if (readChunkBytes < WAVE_STREAM_MIN_READ_BYTES) {
					readChunkBytes = WAVE_STREAM_MIN_READ_BYTES;
					g_waveStreamReadChunkBytes = WAVE_STREAM_MIN_READ_BYTES;
				}
				if (bytesAvailableBeforePlayCursor > readChunkBytes) {
					FrontendWaveStream_RefillBuffer((uint32_t)bytesAvailableBeforePlayCursor);
				}
			}

			g_waveStreamPlayAheadBytes = bytesAvailableBeforePlayCursor;
		} else if (!g_waveStreamPauseDepth &&
				   (g_waveStreamBuffer->lpVtbl->GetStatus(g_waveStreamBuffer, &status) ||
					(status & 5u) == 0)) {
			g_waveStreamPlaying = 0;
		}
	}
	return g_waveStreamPlayedBytes;
}

// FUNCTION: XWA 0x558890
void FrontendWaveStream_RefillBuffer(uint32_t bytesAvailableBeforePlayCursor) {
	void* audioPtr1;
	void* audioPtr2;
	uint32_t audioBytes1;
	uint32_t audioBytes2;
	int result;

	result = (int)FrontendFileStream_ReadBytes(1, g_waveStreamReadBuffer, 0,
											   (unsigned int)g_waveStreamReadChunkBytes, 0);
	if (result != -1) {
		if (result != 0) {
			result =
				DirectSound_LockBuffer(g_waveStreamBuffer, (uint32_t)g_waveStreamWriteOffset,
									   (uint32_t)result, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2);
			if (result) {
				char* firstReadBuffer = (char*)g_waveStreamReadBuffer;
				char* secondReadBuffer = (char*)g_waveStreamReadBuffer;

				if (audioPtr1 != NULL && firstReadBuffer != NULL) {
					FrontendWaveStream_CopyToLockedRegion(audioPtr1, firstReadBuffer, audioBytes1);
					g_waveStreamWriteOffset += (int)audioBytes1;
					if (g_waveStreamWriteOffset >= WAVE_STREAM_BUFFER_BYTES) {
						g_waveStreamWriteOffset -= WAVE_STREAM_BUFFER_BYTES;
					}
				}
				if (audioPtr2 != NULL && secondReadBuffer != NULL) {
					FrontendWaveStream_CopyToLockedRegion(audioPtr2, secondReadBuffer + audioBytes1,
														  audioBytes2);
					g_waveStreamWriteOffset = (int)audioBytes2;
				}
				DirectSound_UnlockBuffer(g_waveStreamBuffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);
			}
		} else if (g_waveStreamLoopMode) {
			FrontendFileStream_RotateLoopQueue(1);
			do {
				result = (int)FrontendFileStream_ReadBytes(1, g_waveStreamReadBuffer, 0,
														   (unsigned int)g_waveStreamDataOffset, 0);
			} while (result == -1);
		} else {
			result = DirectSound_LockBuffer(g_waveStreamBuffer, (uint32_t)g_waveStreamWriteOffset,
											bytesAvailableBeforePlayCursor, &audioPtr1, &audioBytes1,
											&audioPtr2, &audioBytes2);
			if (result) {
				if (audioPtr1 != NULL) {
					FrontendWaveStream_FillSilence(audioPtr1, audioBytes1);
				}
				if (audioPtr2 != NULL) {
					FrontendWaveStream_FillSilence(audioPtr2, audioBytes2);
				}
				DirectSound_UnlockBuffer(g_waveStreamBuffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);
			}
			g_waveStreamDrainAfterEof = 1;
			g_waveStreamRefillEnabled = 0;
		}
	}
}

// FUNCTION: XWA 0x558A80
int FrontendWaveStream_IsPaused(void) { return g_waveStreamPauseDepth != 0; }

// FUNCTION: XWA 0x558A90
void FrontendWaveStream_Pause(void) {
	if (FrontendSound_GetDirectSound() == NULL) {
		return;
	}
	if (g_waveStreamPauseDepth == 0) {
		if (g_waveStreamBuffer != NULL && g_waveStreamPlaying == 1) {
			DirectSound_StopBuffer(g_waveStreamBuffer);
			g_waveStreamCurrentPlayCursor = (int)DirectSound_GetPlayCursor(g_waveStreamBuffer);
		}
		g_waveStreamSavedRefillEnabled = g_waveStreamRefillEnabled;
		g_waveStreamRefillEnabled = 0;
	}
	++g_waveStreamPauseDepth;
}

// FUNCTION: XWA 0x558AF0
void FrontendWaveStream_Resume(void) {
	if (FrontendSound_GetDirectSound() == NULL) {
		return;
	}
	--g_waveStreamPauseDepth;
	if (g_waveStreamPauseDepth == 0) {
		if (g_waveStreamBuffer != NULL && g_waveStreamPlaying == 1) {
			int volume = g_gameConfig.sfxDatapadVolume != 0 ? 0x7F : 0;

			DirectSound_PlayBuffer(g_waveStreamBuffer, (uint32_t)g_waveStreamCurrentPlayCursor,
								   g_waveStreamLoopMode || g_waveStreamIsStreaming, volume);
		}
		g_waveStreamRefillEnabled = g_waveStreamSavedRefillEnabled;
	}
}

// FUNCTION: XWA 0x558C70
int FrontendWaveStream_IsPlaying(void) {
	if (FrontendSound_GetDirectSound() == NULL) {
		return 0;
	}
	return g_waveStreamPlaying & 0xFF;
}

// FUNCTION: XWA 0x558BE0
void FrontendWaveStream_Shutdown(void) {
	g_waveStreamWriteOffset = 0;
	g_waveStreamRefillEnabled = 0;
	g_waveStreamLoopMode = 0;
	g_waveStreamDrainAfterEof = 0;
	FrontendFileStream_PopHead(0);
	FrontendFileStream_PopHead(0);
	FrontendFileStream_PopHead(1);
	FrontendFileStream_PopHead(1);
	if (g_waveStreamBuffer != NULL) {
		DirectSound_StopBuffer(g_waveStreamBuffer);
		g_waveStreamPlaying = 0;
		DirectSound_ReleaseBufferPtr(&g_waveStreamBuffer);
		g_waveStreamBuffer = NULL;
	}
	if (g_waveStreamReadBuffer != NULL) {
		Mem_Free(g_waveStreamReadBuffer);
		g_waveStreamReadBuffer = NULL;
	}
}
