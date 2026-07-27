#include "xwa/audio/imuse/imuse.h"

#ifdef XWA_MODERN
#include "aeron/sync.h"
#include "aeron/time.h"
#endif

#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef XWA_MODERN
short __cdecl _rotr(short value, int shift);
#pragma intrinsic(_rotr)
#endif

typedef struct ImDSBufferDescCompat {
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwBufferBytes;
	uint32_t dwReserved;
	void* lpwfxFormat;
} ImDSBufferDescCompat;

#ifndef XWA_MODERN
#define IM_STDCALL __stdcall
#else
#define IM_STDCALL
#endif

#ifdef XWA_MODERN
typedef uint32_t ImDSDword;
#else
typedef unsigned long ImDSDword;
#endif

typedef int(IM_STDCALL* ImDSReleaseFn)(void* self);
typedef void(IM_STDCALL* ImTimerCallbackFn)(unsigned int timerId, unsigned int msg, uintptr_t user,
											uintptr_t dw1, uintptr_t dw2);
typedef int(IM_STDCALL* ImDSCreateSoundBufferFn)(void* self, const ImDSBufferDescCompat* desc, void** buffer,
												 void* outer);
typedef int(IM_STDCALL* ImDSGetCurrentPositionFn)(void* self, int* playCursor, int* writeCursor);
typedef int(IM_STDCALL* ImDSSetVolumeFn)(void* self, int volume);
typedef int(IM_STDCALL* ImDSLockFn)(void* self, ImDSDword offset, ImDSDword bytes, void** ptr1,
									ImDSDword* bytes1, void** ptr2, ImDSDword* bytes2, ImDSDword flags);
typedef int(IM_STDCALL* ImDSUnlockFn)(void* self, void* ptr1, ImDSDword bytes1, void* ptr2, ImDSDword bytes2);
typedef int(IM_STDCALL* ImDSPlayFn)(void* self, uint32_t reserved1, uint32_t reserved2, uint32_t flags);
typedef int(IM_STDCALL* ImDSStopFn)(void* self);

typedef struct ImDSBufferCompat ImDSBufferCompat;
typedef struct ImDirectSoundCompat ImDirectSoundCompat;

typedef struct ImDirectSoundVtableCompat {
	void* queryInterface;
	void* addRef;
	ImDSReleaseFn release;
	ImDSCreateSoundBufferFn createSoundBuffer;
} ImDirectSoundVtableCompat;

struct ImDirectSoundCompat {
	const ImDirectSoundVtableCompat* vtable;
};

typedef struct ImDSBufferVtableCompat {
	void* queryInterface;
	void* addRef;
	ImDSReleaseFn release;
	void* getCaps;
	ImDSGetCurrentPositionFn getCurrentPosition;
	void* getFormat;
	void* getVolume;
	void* getPan;
	void* getFrequency;
	void* getStatus;
	void* initialize;
	ImDSLockFn lock;
	ImDSPlayFn play;
	void* setCurrentPosition;
	void* setFormat;
	ImDSSetVolumeFn setVolume;
	void* setPan;
	void* setFrequency;
	ImDSStopFn stop;
	ImDSUnlockFn unlock;
} ImDSBufferVtableCompat;

struct ImDSBufferCompat {
	const ImDSBufferVtableCompat* vtable;
};

void* memcpy_0(void* dst, const void* src, size_t size) { return memcpy(dst, src, size); }

// GLOBAL: XWA 0x7AAC00
ImStreamSlot* g_imCurrentStream;
// GLOBAL: XWA 0x7AAC04
int g_imStreamDirty;
// GLOBAL: XWA 0x7AAC08
ImStreamSlot g_imStreamSlots[3];
// GLOBAL: XWA 0x7B0EC0
unsigned int uDelay;
// GLOBAL: XWA 0x7B0EC4
IM_CROSS_THREAD unsigned int g_imTimerTicks;
// GLOBAL: XWA 0x7B0ED0
int g_imDSBlockCount;
// GLOBAL: XWA 0x7B0EF4
int g_imDSPlayCursor;
// GLOBAL: XWA 0x7B0ED8
ImCriticalSection g_imWaveCritSec;
// GLOBAL: XWA 0x7B0EF0
unsigned int uPeriod;
// GLOBAL: XWA 0x7B0EFC
unsigned int uTimerID;
// GLOBAL: XWA 0x7B0F00
void* g_imDirectSound;
// GLOBAL: XWA 0x7B0F04
void* g_imDSoundBuffer;
// GLOBAL: XWA 0x7B0EC8
int g_imDSWriteCursor;
// GLOBAL: XWA 0x7B0ECC
uint32_t g_imDSLockBytes2;
// GLOBAL: XWA 0x7B0F08
IM_CROSS_THREAD int g_imDSPaused;
// GLOBAL: XWA 0x7B0F0C
void* hObject;
// GLOBAL: XWA 0x7B0F14
IM_CROSS_THREAD int g_imWaveCsInitialized;
// GLOBAL: XWA 0x7B0F18
int g_imDSFillToggle;
// GLOBAL: XWA 0x7B0F1C
int g_imDSLocked;
// GLOBAL: XWA 0x7B0F20
void* g_imDSLockPtr1;
// GLOBAL: XWA 0x7B0F24
void* g_imDSLockPtr2;
// GLOBAL: XWA 0x608714
int g_imDSWriteBlock = -1;
// GLOBAL: XWA 0x608718
ImWaveFormatEx g_imOutputWaveFormat = { 1, 2, 22050, 88200, 4, 16, 0 };
// GLOBAL: XWA 0x7AABF4
int g_imJumpZoneSize;
// GLOBAL: XWA 0xB0CE40
unsigned char g_imPanVolTable[17 * 17];
// GLOBAL: XWA 0x7AAEC0
int g_imMixBuffer[4096];
// GLOBAL: XWA 0x7AEEC0
int16_t g_imOutputStage[4096];
// GLOBAL: XWA 0x7B0EF8
uint32_t g_imDSLockBytes1;

static void** ImComVtable(void* object) {
	if (!object) {
		return NULL;
	}
	return *(void***)object;
}

static int ImCallRelease(void* object) {
	void** vtbl = ImComVtable(object);
	if (!vtbl || !vtbl[2]) {
		return 0;
	}
	return ((ImDSReleaseFn)vtbl[2])(object);
}

static int ImCallCreateSoundBuffer(void* directSound, const ImDSBufferDescCompat* desc, void** buffer) {
	void** vtbl = ImComVtable(directSound);
	if (!vtbl || !vtbl[3]) {
		return -1;
	}
	return ((ImDSCreateSoundBufferFn)vtbl[3])(directSound, desc, buffer, NULL);
}

static int ImCallSetVolume(void* buffer, int volume) {
	void** vtbl = ImComVtable(buffer);
	if (!vtbl || !vtbl[15]) {
		return -1;
	}
	return ((ImDSSetVolumeFn)vtbl[15])(buffer, volume);
}

static int ImCallLock(void* buffer, ImDSDword offset, ImDSDword bytes, void** ptr1, ImDSDword* bytes1,
					  void** ptr2, ImDSDword* bytes2, ImDSDword flags) {
	void** vtbl = ImComVtable(buffer);
	if (!vtbl || !vtbl[11]) {
		return -1;
	}
	return ((ImDSLockFn)vtbl[11])(buffer, offset, bytes, ptr1, bytes1, ptr2, bytes2, flags);
}

static int ImCallUnlock(void* buffer, void* ptr1, ImDSDword bytes1, void* ptr2, ImDSDword bytes2) {
	void** vtbl = ImComVtable(buffer);
	if (!vtbl || !vtbl[19]) {
		return -1;
	}
	return ((ImDSUnlockFn)vtbl[19])(buffer, ptr1, bytes1, ptr2, bytes2);
}

static int ImCallStop(void* buffer) {
	void** vtbl = ImComVtable(buffer);
	if (!vtbl || !vtbl[18]) {
		return -1;
	}
	return ((ImDSStopFn)vtbl[18])(buffer);
}

#ifndef XWA_MODERN
typedef void(IM_STDCALL* ImCriticalSectionFn)(ImCriticalSection* critSec);
typedef int(IM_STDCALL* ImCloseHandleFn)(void* handle);
typedef unsigned int(IM_STDCALL* ImTimeGetDevCapsFn)(ImTimeCaps* caps, unsigned int size);
typedef unsigned int(IM_STDCALL* ImTimePeriodFn)(unsigned int period);
typedef unsigned int(IM_STDCALL* ImTimeSetEventFn)(unsigned int delay, unsigned int resolution,
												   ImTimerCallbackFn callback, uintptr_t user,
												   unsigned int eventFlags);
typedef unsigned int(IM_STDCALL* ImTimeGetTimeFn)(void);
ImCriticalSectionFn InitializeCriticalSection;
ImCriticalSectionFn EnterCriticalSection;
ImCriticalSectionFn LeaveCriticalSection;
ImCriticalSectionFn DeleteCriticalSection;
// GLOBAL: XWA 0x5A912C
ImCloseHandleFn CloseHandle;
// GLOBAL: XWA 0x5A929C
ImTimeGetDevCapsFn timeGetDevCaps;
// GLOBAL: XWA 0x5A92B0
ImTimePeriodFn timeBeginPeriod;
// GLOBAL: XWA 0x5A92BC
ImTimePeriodFn timeEndPeriod;
// GLOBAL: XWA 0x5A92A0
ImTimeSetEventFn timeSetEvent;
// GLOBAL: XWA 0x5A92C8
ImTimePeriodFn timeKillEvent;
// GLOBAL: XWA 0x5A92AC
ImTimeGetTimeFn timeGetTime;
#define ImInitializeCriticalSection InitializeCriticalSection
#define ImEnterCriticalSection EnterCriticalSection
#define ImLeaveCriticalSection LeaveCriticalSection
#define ImDeleteCriticalSection DeleteCriticalSection
#define ImCloseHandle CloseHandle
#define ImTimeGetDevCaps timeGetDevCaps
#define ImTimeBeginPeriod timeBeginPeriod
#define ImTimeEndPeriod timeEndPeriod
#define ImTimeSetEvent timeSetEvent
#define ImTimeKillEvent timeKillEvent
#define ImTimeGetTime timeGetTime
#else
void ImPlatformCsInit(ImCriticalSection* critSec) { critSec->mutex = Aeron_MutexCreate(); }

void ImPlatformCsEnter(ImCriticalSection* critSec) { Aeron_MutexLock((AeronMutex*)critSec->mutex); }

void ImPlatformCsLeave(ImCriticalSection* critSec) { Aeron_MutexUnlock((AeronMutex*)critSec->mutex); }

void ImPlatformCsDelete(ImCriticalSection* critSec) {
	Aeron_MutexDestroy((AeronMutex*)critSec->mutex);
	critSec->mutex = NULL;
}

#define ImInitializeCriticalSection ImPlatformCsInit
#define ImEnterCriticalSection ImPlatformCsEnter
#define ImLeaveCriticalSection ImPlatformCsLeave
#define ImDeleteCriticalSection ImPlatformCsDelete

static int ImCloseHandle(void* handle) {
	(void)handle;
	return 1;
}

/* Resolves to uPeriod = 50 in ImWaveOutStart, hence uDelay = 20 ms. */
static int ImTimeGetDevCaps(ImTimeCaps* caps, unsigned int size) {
	(void)size;
	caps->wPeriodMin = 1;
	caps->wPeriodMax = 100;
	return 0;
}

/* Aeron timers are already high resolution; no global timer period to adjust. */
static unsigned int ImTimeBeginPeriod(unsigned int period) {
	(void)period;
	return 0;
}

static unsigned int ImTimeEndPeriod(unsigned int period) {
	(void)period;
	return 0;
}

/* iMUSE installs a single timer, and ImTimerCallback ignores every argument, so
   the ids are passed as zero. */
static ImTimerCallbackFn g_imTimerCallback;

static void ImTimerTrampoline(void* user) {
	(void)user;
	if (g_imTimerCallback) {
		g_imTimerCallback(0, 0, 15, 0, 0);
	}
}

static unsigned int ImTimeSetEvent(unsigned int delay, unsigned int resolution, ImTimerCallbackFn callback,
								   uintptr_t user, unsigned int eventFlags) {
	(void)resolution;
	(void)user;
	(void)eventFlags;
	g_imTimerCallback = callback;
	return (unsigned int)Aeron_TimerCreate(delay, ImTimerTrampoline, NULL);
}

static unsigned int ImTimeKillEvent(unsigned int timerId) {
	Aeron_TimerDestroy((AeronTimer)timerId);
	return 0;
}

static unsigned int ImTimeGetTime(void) { return (unsigned int)(Aeron_NowUs() / 1000u); }
#endif

// FLAGS: /O2 /Og- /Oi-
// FUNCTION: XWA 0x58504A
int ImGetMusicStreamStatus(int* outBufSize, int* outRefillThreshold, int* outFill, int* outEof) {
	int soundId;

	soundId = 0;
	soundId = ImGetNextSound(soundId);
	while (soundId) {
		if (ImGetParam(soundId, P_IS_STREAMING) == 1 &&
			(ImGetParam(soundId, P_VGROUP) == 3 || ImGetParam(soundId, P_VGROUP) == 4)) {
			ImQueryStream(soundId, outBufSize, outRefillThreshold, outFill, outEof);
			return 1;
		}
		soundId = ImGetNextSound(soundId);
	}
	return 0;
}

// FUNCTION: XWA 0x584FBE
int ImFillStreamsWhileMusicCritical(int extraCount) {
	int i;

	if (g_imRunning && ImIsMusicCritical()) {
		do {
			ImProcessStreams();
		} while (ImIsMusicCritical());

		for (i = 0; i < extraCount; ++i) {
			ImProcessStreams();
		}
		if (extraCount > 0) {
			return extraCount;
		}
	}
	return 0;
}

// FUNCTION: XWA 0x585009
int ImIsMusicCritical(void) {
	struct ImMusicStreamStatus {
		int bufSize;
		int fill;
		int refillThreshold;
		int eof;
	} status;

	if (ImGetMusicStreamStatus(&status.bufSize, &status.refillThreshold, &status.fill, &status.eof)) {
		if (status.eof || status.fill > status.refillThreshold) {
			return 0;
		}
		return 1;
	}
	return 0;
}

// FUNCTION: XWA 0x5850E6
void ImFloodMusicBuffer(void) {
	if (!g_imRunning) {
		return;
	}

	while (!ImIsMusicBufferFull()) {
		ImProcessStreams();
	}
}

// FUNCTION: XWA 0x585106
int ImIsMusicBufferFull(void) {
	struct ImMusicStreamStatus {
		int bufSize;
		int fill;
		int refillThreshold;
		int eof;
	} status;

	if (ImGetMusicStreamStatus(&status.bufSize, &status.refillThreshold, &status.fill, &status.eof)) {
		if (status.eof || status.fill > status.bufSize - g_imSoundBufferField8) {
			return 1;
		}
		return 0;
	}
	return 1;
}

// FUNCTION: XWA 0x585150
void ImUpdate(void) {
	if (g_imRunning) {
		ImProcessStreams();
		ImRefreshScript();
	}
}

// FUNCTION: XWA 0x5851EA
void ImSetScriptEnableConfig(int enabled) { g_imScriptEnableConfig = enabled; }

// FUNCTION: XWA 0x5851F7
void ImSetScriptInitParam8(int value) { g_imScriptInitParam8Unused = value; }

// FUNCTION: XWA 0x585204
void ImSetDirectSoundDevice(void* directSound) { g_imDirectSoundDevice = directSound; }

// FUNCTION: XWA 0x58DDA6
int ImIsValidSoundId(unsigned int soundId) { return soundId != 0 && soundId < 0xfffffff0u; }

// FUNCTION: XWA 0x5874BB
void ImHeartbeat(void) {
	unsigned int nowMs;
	int soundId;
	unsigned int targetGroup4Volume;
	unsigned int currentGroup3Volume;
	unsigned int currentGroup4Volume;
	unsigned int steppedGroup4Volume;
	int nextGroup4Volume;

	nowMs = ImTimeGetTime();
	if (g_imHeartbeatGuard || IM_ATOMIC_LOAD(g_imBusyCount) ||
		(g_imLastHeartbeatMs >= 0 && nowMs - (unsigned int)g_imLastHeartbeatMs < 20u)) {
		return;
	}

	g_imLastHeartbeatMs = (int)nowMs;
	g_imHeartbeatGuard = 1;
	if (!IM_ATOMIC_LOAD(g_imBusyCount)) {
		ImRenderFrame();
	}

	if (g_imPauseCount) {
		g_imHeartbeatGuard = 0;
		return;
	}

	g_imTriggerClockAccum += 20000;
	while ((unsigned int)g_imTriggerClockAccum >= 20000u) {
		g_imTriggerClockAccum -= 20000;
		ImProcessFades();
		ImProcessTriggers();
	}

	g_imVolDuckClockAccum += 20000;
	while ((unsigned int)g_imVolDuckClockAccum >= 100000u) {
		g_imVolDuckClockAccum -= 100000;
		targetGroup4Volume = (unsigned int)ImSetGroupVol(3u, -1);
		for (soundId = ImGetNextSound(0); soundId; soundId = ImGetNextSound(soundId)) {
			if (ImGetParam(soundId, P_VGROUP) == 2) {
				targetGroup4Volume = (75 * targetGroup4Volume) >> 7;
				break;
			}
		}

		currentGroup4Volume = (unsigned int)ImSetGroupVol(4u, -1);
		currentGroup3Volume = (unsigned int)ImSetGroupVol(3u, -1);
		if (currentGroup4Volume > targetGroup4Volume) {
			steppedGroup4Volume = currentGroup4Volume - 18;
			if (steppedGroup4Volume > targetGroup4Volume) {
				if (steppedGroup4Volume >= currentGroup3Volume) {
					nextGroup4Volume = (int)currentGroup3Volume;
				} else {
					nextGroup4Volume = (int)steppedGroup4Volume;
				}
			} else {
				if (targetGroup4Volume >= currentGroup3Volume) {
					nextGroup4Volume = (int)currentGroup3Volume;
				} else {
					nextGroup4Volume = (int)targetGroup4Volume;
				}
			}
			ImSetGroupVol(4u, nextGroup4Volume);
		} else if (currentGroup4Volume < targetGroup4Volume) {
			steppedGroup4Volume = currentGroup4Volume + 1;
			if (steppedGroup4Volume < targetGroup4Volume) {
				if (steppedGroup4Volume <= currentGroup3Volume) {
					nextGroup4Volume = (int)currentGroup3Volume;
				} else {
					nextGroup4Volume = (int)steppedGroup4Volume;
				}
			} else {
				if (targetGroup4Volume <= currentGroup3Volume) {
					nextGroup4Volume = (int)currentGroup3Volume;
				} else {
					nextGroup4Volume = (int)targetGroup4Volume;
				}
			}
			ImSetGroupVol(4u, nextGroup4Volume);
		}
	}

	g_imHeartbeatGuard = 0;
}

// FUNCTION: XWA 0x590510
int ImStreamerInit(void) {
	int i;

	for (i = 0; i < 3; ++i) {
		g_imStreamSlots[i].bufId = 0;
	}
	g_imCurrentStream = 0;
	return 0;
}

// FUNCTION: XWA 0x59054E
ImStreamSlot* ImStreamOpen(unsigned int bufId, int soundId, unsigned int maxRead) {
	ImSoundBuffer* bufInfo;
	ImStreamSlot* slot;
	int i;

	bufInfo = ImResBufInfo(soundId);
	if (!bufInfo) {
		ImLog("ERR: streamer couldn't get buf info...");
		return NULL;
	}

	if (maxRead >= (unsigned int)(bufInfo->size >> 2)) {
		ImLog("ERR: maxRead too big for buf...");
		return NULL;
	}

	slot = g_imStreamSlots;
	for (i = 0; i < 3; ++i) {
		if (slot->bufId && slot->soundId == soundId) {
			ImLog("ERR: stream bufID %lu already in use...", soundId);
			return NULL;
		}
		++slot;
	}

	slot = g_imStreamSlots;
	for (i = 0; i < 3; ++i) {
		if (!slot->bufId) {
			slot->bufId = (int)bufId;
			slot->filePos = 0;
			slot->fileSize = ImResSeek(bufId, 0, 2);
			slot->soundId = soundId;
			slot->bufBase = (char*)bufInfo->data;
			slot->bufSize = bufInfo->size - (int)(maxRead + 4);
			slot->maxChunk = bufInfo->field_8;
			slot->refillThreshold = bufInfo->field_C;
			slot->maxRead = (int)maxRead;
			slot->writeCur = 0;
			slot->readCur = 0;
			slot->eof = 0;
			return slot;
		}
		++slot;
	}

	ImLog("ERR: no spare streams...");
	return NULL;
}

// FUNCTION: XWA 0x5906D8
int ImFreeStream(ImStreamSlot* slot) {
	slot->bufId = 0;
	if (g_imCurrentStream == slot) {
		g_imCurrentStream = 0;
	}
	return 0;
}

// FUNCTION: XWA 0x5906FD
int ImProcessStreamSwitches(void) {
	int i;
	ImStreamSlot* first;
	ImStreamSlot* second;

	first = NULL;
	second = NULL;
	ImIncBusyCount();
	ImServiceStreamJumps();

	for (i = 0; i < 3; ++i) {
		if (g_imStreamSlots[i].bufId && !g_imStreamSlots[i].eof) {
			if (!first) {
				first = &g_imStreamSlots[i];
			} else if (!second) {
				second = &g_imStreamSlots[i];
			} else {
				ImLog("ERR: three streams in use...");
			}
		}
	}

	if (first && second) {
		int firstLow;
		int secondLow;

		firstLow = (unsigned int)ImGetStreamFill(first) < (unsigned int)first->refillThreshold;
		secondLow = (unsigned int)ImGetStreamFill(second) < (unsigned int)second->refillThreshold;

		if (firstLow && secondLow) {
			if (g_imCurrentStream == first) {
				ImStreamRefill(first);
				ImStreamRefill(second);
			} else {
				ImStreamRefill(second);
				ImStreamRefill(first);
			}
		} else if (firstLow) {
			ImStreamRefill(first);
		} else if (secondLow) {
			ImStreamRefill(second);
		} else if (g_imCurrentStream == first) {
			ImStreamRefill(first);
		} else {
			ImStreamRefill(second);
		}
	} else if (first) {
		ImStreamRefill(first);
	} else if (second) {
		ImStreamRefill(second);
	}

	ImDecBusyCount();
	return 0;
}

// FUNCTION: XWA 0x590AB0
int ImStreamGetOffset(ImStreamSlot* stream) { return stream->filePos; }

// FUNCTION: XWA 0x590ABB
int ImGetStreamFill(ImStreamSlot* stream) {
	ImStreamSlot* slot;
	int fill;

	slot = stream;
	fill = slot->writeCur - slot->readCur;
	if (fill < 0) {
		fill += slot->bufSize;
	}
	return fill;
}

// FUNCTION: XWA 0x590B57
int ImGetStreamStatus(ImStreamSlot* stream, int* outBufSize, int* outRefillThreshold, int* outFill,
					  int* outEof) {
	ImStreamSlot* slot;
	slot = stream;

	ImServiceStreamJumps();
	*outBufSize = slot->bufSize;
	*outRefillThreshold = slot->refillThreshold;
	*outFill = ImGetStreamFill(stream);
	*outEof = slot->eof;
	return 0;
}

// FUNCTION: XWA 0x5909F2
int ImStreamConsume(ImStreamSlot* stream, unsigned int count) {
	ImStreamSlot* slot;

	slot = stream;
	g_imStreamDirty = 1;
	if (count > (unsigned int)ImGetStreamFill(stream)) {
		return -1;
	}

	slot->readCur += (int)count;
	if ((unsigned int)slot->readCur >= (unsigned int)slot->bufSize) {
		slot->readCur -= slot->bufSize;
	}
	return 0;
}

// FUNCTION: XWA 0x590A51
int ImStreamSetAvail(ImStreamSlot* stream, unsigned int count) {
	g_imStreamDirty = 1;
	if (count > (unsigned int)ImGetStreamFill(stream)) {
		return -1;
	}

	stream->writeCur = stream->readCur + (int)count;
	if ((unsigned int)stream->writeCur >= (unsigned int)stream->bufSize) {
		stream->writeCur -= stream->bufSize;
	}
	return 0;
}

// FUNCTION: XWA 0x59032B
void* ImAllocFadeBuf(unsigned int* pSize) {
	int i;

	if (*pSize > (unsigned int)g_imLargeBufSize) {
		ImLog("WARNING: requested fade too large (%lu)...", *pSize);
		*pSize = (unsigned int)g_imLargeBufSize;
	}

	if (*pSize > (unsigned int)g_imSmallBufSize) {
		for (i = 0; i < g_imLargeBufCount; ++i) {
			if (!g_imLargeBufFlags[i]) {
				g_imLargeBufFlags[i] = 1;
				return g_imLargeBufBase + g_imLargeBufSize * i;
			}
		}
		ImLog("ERR: couldn't allocate large fade buf...");
		*pSize = (unsigned int)g_imSmallBufSize;
	}

	for (i = 0; i < g_imSmallBufCount; ++i) {
		if (!g_imSmallBufFlags[i]) {
			g_imSmallBufFlags[i] = 1;
			return g_imSmallBufBase + g_imSmallBufSize * i;
		}
	}

	ImLog("ERR: couldn't allocate small fade buf...");
	return NULL;
}

// FUNCTION: XWA 0x590434
void ImFreeFadeBuf(void* buf) {
	int i;

	for (i = 0; i < g_imLargeBufCount; ++i) {
		if (buf == g_imLargeBufBase + i * g_imLargeBufSize) {
			if (!g_imLargeBufFlags[i]) {
				ImLog("ERR: redundant large fade buf de-allocation...");
			}
			g_imLargeBufFlags[i] = 0;
			return;
		}
	}

	for (i = 0; i < g_imSmallBufCount; ++i) {
		if (buf == g_imSmallBufBase + i * g_imSmallBufSize) {
			if (!g_imSmallBufFlags[i]) {
				ImLog("ERR: redundant small fade buf de-allocation...");
			}
			g_imSmallBufFlags[i] = 0;
			return;
		}
	}

	ImLog("ERR: couldn't find fade buf to de-allocate...");
}

// FUNCTION: XWA 0x5900F2
void ImCommitJump(ImDispatch* dispatch, ImStreamZone* zone) {
	ImStreamZone* cur;

	if (!zone->next) {
		return;
	}

	{
		int count = zone->byteCount;

		cur = dispatch->pendingJumpZones;
		while (cur != zone) {
			count += cur->byteCount;
			cur = cur->next;
		}
		ImStreamSetAvail(dispatch->stream, (unsigned int)count);
	}
	while (zone->next) {
		zone->next->inUse = 0;
		ImListRemove2((ImListNode**)&zone->next, (ImListNode*)zone->next);
	}

	ImStreamSeek(dispatch->stream, dispatch->track->soundId, zone->streamOffset + zone->byteCount);
}

// FUNCTION: XWA 0x590950
void* ImStreamPeek(ImStreamSlot* stream, int offset, unsigned int len) {
	int readOffset;
	int tailBytes;

	if ((uint32_t)offset + len > (uint32_t)ImGetStreamFill(stream) || len > (unsigned int)stream->maxRead) {
		return 0;
	}

	readOffset = stream->readCur + offset;
	if (readOffset >= stream->bufSize) {
		readOffset -= stream->bufSize;
	}

	tailBytes = stream->bufSize - readOffset;
	if ((uint32_t)tailBytes < len) {
		memcpy(stream->bufBase + stream->bufSize, stream->bufBase, len - (uint32_t)tailBytes);
	}
	return stream->bufBase + readOffset;
}

// FUNCTION: XWA 0x59089D
void* ImStreamGet(ImStreamSlot* stream, unsigned int count) {
	char* result;
	int tailBytes;

	if (count > (unsigned int)ImGetStreamFill(stream) || count > (unsigned int)stream->maxRead) {
		return 0;
	}

	tailBytes = stream->bufSize - stream->readCur;
	if ((uint32_t)tailBytes < count) {
		memcpy(stream->bufBase + stream->bufSize, stream->bufBase, count - (uint32_t)tailBytes);
	}

	result = stream->bufBase + stream->readCur;
	stream->readCur += (int)count;
	if ((unsigned int)stream->readCur >= (unsigned int)stream->bufSize) {
		stream->readCur -= stream->bufSize;
	}
	return result;
}

// FUNCTION: XWA 0x590AEF
int ImStreamGetFileSize(ImStreamSlot* stream) { return stream->fileSize; }

// FUNCTION: XWA 0x590B03
int ImStreamSeek(ImStreamSlot* slot, int bufId, int filePos) {
	ImStreamSlot* stream;

	stream = slot;
	g_imStreamDirty = 1;
	stream->bufId = bufId;
	stream->filePos = filePos;
	stream->fileSize = 0;
	stream->eof = 0;
	if (g_imCurrentStream == stream) {
		g_imCurrentStream = 0;
	}
	return 0;
}

// FUNCTION: XWA 0x590B9E
int ImFeedStream(ImStreamSlot* stream, char* src, int count, int feedFlag) {
	int freeBytes;
	int overflowBytes;
	int chunk;

	freeBytes = stream->readCur - stream->writeCur;
	if (freeBytes <= 0) {
		freeBytes += stream->bufSize;
	}
	freeBytes -= 4;

	if (count > freeBytes) {
		ImLog("ERR: FeedStream() buffer overflow...");
		overflowBytes = count - freeBytes;
		ImStreamConsume(stream, (unsigned int)(12 - overflowBytes % 12 + overflowBytes));
	}

	while (count > 0) {
		if (count >= stream->bufSize - stream->writeCur) {
			chunk = stream->bufSize - stream->writeCur;
		} else {
			chunk = count;
		}

		memcpy(stream->bufBase + stream->writeCur, src, (size_t)chunk);
		count -= chunk;
		src += chunk;
		stream->filePos += chunk;
		stream->writeCur += chunk;
		if ((unsigned int)stream->writeCur >= (unsigned int)stream->bufSize) {
			stream->writeCur -= stream->bufSize;
		}
	}

	stream->eof = feedFlag;
	return 0;
}

// FUNCTION: XWA 0x590CD2
int ImStreamRefill(ImStreamSlot* stream) {
	int freeBytes;
	int refillBytes;
	int fileRemaining;
	int readRemaining;
	int chunk;
	int actual;

	if (!stream->fileSize) {
		stream->fileSize = ImResSeek((unsigned int)stream->bufId, 0, 2);
	}

	freeBytes = stream->readCur - stream->writeCur;
	if (freeBytes <= 0) {
		freeBytes += stream->bufSize;
	}
	freeBytes -= 4;

	if (stream->maxChunk < freeBytes) {
		refillBytes = stream->maxChunk;
	} else {
		refillBytes = freeBytes;
	}

	fileRemaining = stream->fileSize - stream->filePos;
	if (refillBytes < fileRemaining) {
		readRemaining = refillBytes;
	} else {
		readRemaining = fileRemaining;
	}
	if (fileRemaining <= 0) {
		stream->eof = 1;
	}

	while (readRemaining > 0) {
		if (readRemaining < stream->bufSize - stream->writeCur) {
			chunk = readRemaining;
		} else {
			chunk = stream->bufSize - stream->writeCur;
		}

		if (ImResSeek((unsigned int)stream->bufId, stream->filePos, 0) != stream->filePos) {
			ImLog("ERR: Invalid seek in streamer...");
			stream->eof = 1;
			return 0;
		}

		g_imStreamDirty = 0;
		ImDecBusyCount();
		actual =
			ImResRead((unsigned int)stream->bufId, stream->bufBase + stream->writeCur, (unsigned int)chunk);
		ImIncBusyCount();
		if (g_imStreamDirty) {
			return 0;
		}

		readRemaining -= actual;
		stream->filePos += actual;
		g_imCurrentStream = stream;
		stream->writeCur += actual;
		if ((unsigned int)stream->writeCur >= (unsigned int)stream->bufSize) {
			stream->writeCur -= stream->bufSize;
		}

		if (actual != chunk) {
			ImLog("ERR: unable to load correct amount (req=%lu,act=%lu)...", chunk, actual);
			g_imCurrentStream = 0;
			return 0;
		}
	}
	return 0;
}

// FUNCTION: XWA 0x591ADE
int16_t* ImMixSource(int16_t* src, int count, int srcRate, int channels, int doMix, unsigned int dstFrames,
					 int dstSample, int volume, int pan) {
	int gainL;
	int panIndex;
	unsigned int volumeRow;
	int gainR;
	int16_t* result;

	result = (int16_t*)g_imMixBuffer;
	if (!src || !count || srcRate != 16 || volume < 1) {
		return result;
	}

	if (channels == 1) {
		volumeRow = ((unsigned int)volume >> 3) + 1;
		if (volumeRow >= 17) {
			volumeRow = 16;
		}
		panIndex = (pan >> 3) - 8;
		if (pan > 64) {
			++panIndex;
		}
		gainL = g_imPanVolTable[17 * volumeRow + 8 - panIndex];
		gainR = g_imPanVolTable[17 * volumeRow + 8 + panIndex];
		if (doMix) {
			ImMixMono(src, count, dstFrames, dstSample, gainL, gainR);
			return result;
		}
		ImMixMonoNative(src, count, dstFrames, dstSample, gainL, gainR);
		return result;
	}

	if (doMix) {
		ImMixStereo(src, count, dstFrames, dstSample, volume + 1);
		return result;
	}
	ImMixStereoNative(src, count, dstFrames, dstSample, volume + 1);
	return result;
}

// FUNCTION: XWA 0x591CC4
void ImMixMonoNative(int16_t* src, int srcLen, unsigned int dstLen, int dstFrameOff, int gainL, int gainR) {
	uint32_t step;
	{
		int* dst;

		dst = &g_imMixBuffer[2 * dstFrameOff];
		step = ((uint32_t)srcLen << 16) / dstLen;
		switch (step) {
			case 0x10000u:
				while (dstLen--) {
					*dst++ += gainL * src[0];
					*dst++ += gainR * *src++;
				}
				break;

			case 0x8000u:
				srcLen -= 1;
				while (srcLen--) {
					*dst++ += gainL * src[0];
					*dst++ += gainR * src[0];
					*dst++ += gainL * ((src[0] + src[2]) >> 1);
					*dst++ += gainR * ((src[0] + src[2]) >> 1);
					++src;
				}
				*dst++ += gainL * src[0];
				*dst++ += gainR * src[0];
				*dst++ += gainL * src[0];
				*dst++ += gainR * src[0];
				break;

			case 0x20000u:
				while (dstLen--) {
					*dst++ += gainL * src[0];
					*dst++ += gainR * src[0];
					src += 2;
				}
				break;

			default: {
				uint32_t frac;

				frac = 0;
				while (dstLen--) {
					*dst++ += gainL * src[0];
					*dst++ += gainR * src[0];
					frac += step;
					src += (int32_t)frac >> 16;
					frac = (uint16_t)frac;
				}
			} break;
		}
	}
}

// FUNCTION: XWA 0x591FA2
void ImMixStereoNative(int16_t* src, int srcLen, unsigned int dstLen, int dstFrameOff, int gain) {
	uint32_t step;
	{
		int* dst;

		dst = &g_imMixBuffer[2 * dstFrameOff];
		step = ((uint32_t)srcLen << 16) / dstLen;
		switch (step) {
			case 0x10000u:
				dstLen *= 2;
				while (dstLen--) {
					*dst++ += gain * *src++;
				}
				break;

			case 0x8000u:
				srcLen -= 1;
				while (srcLen--) {
					*dst++ += gain * src[0];
					*dst++ += gain * src[1];
					*dst++ += gain * ((src[0] + src[2]) >> 1);
					++src;
					*dst++ += gain * ((src[0] + src[2]) >> 1);
					++src;
				}
				*dst++ += gain * src[0];
				*dst++ += gain * src[1];
				*dst++ += gain * src[0];
				*dst++ += gain * src[1];
				break;

			case 0x20000u:
				while (dstLen--) {
					*dst++ += gain * *src++;
					*dst++ += gain * *src++;
					src += 2;
				}
				break;

			default: {
				uint32_t frac;

				frac = 0;
				step *= 2;
				while (dstLen--) {
					*dst++ += gain * src[0];
					*dst++ += gain * src[1];
					frac += step;
					src += (int32_t)frac >> 16;
					frac = (uint16_t)frac;
				}
			} break;
		}
	}
}

static __inline int16_t ImSwapSigned16(int16_t value) {
	int16_t result;

#ifndef XWA_MODERN
	result = _rotr(value, 8);
#else
	{
		uint16_t raw;

		raw = (uint16_t)value;
		result = (int16_t)((raw >> 8) | (raw << 8));
	}
#endif
	return result;
}

static __inline int16_t ImSwapSigned16At(const int16_t* value) {
	int16_t sample;

	sample = *value;
#ifndef XWA_MODERN
	return _rotr(sample, 8);
#else
	return ImSwapSigned16(sample);
#endif
}

typedef struct ImMixResampler {
	int* dst;
	uint32_t step;
} ImMixResampler;

// FUNCTION: XWA 0x592292
void ImMixMono(int16_t* src, int srcLen, unsigned int dstLen, int dstFrameOff, int gainL, int gainR) {
	ImMixResampler mixer;

	mixer.dst = &g_imMixBuffer[2 * dstFrameOff];
	mixer.step = ((uint32_t)srcLen << 16) / dstLen;
	switch (mixer.step) {
		case 0x10000u:
			while (dstLen--) {
				int16_t sample;

				sample = ImSwapSigned16At(src);
				*mixer.dst++ += gainL * sample;
				*mixer.dst++ += gainR * sample;
				++src;
			}
			break;

		case 0x8000u:
			srcLen -= 1;
			while (srcLen--) {
				int16_t sample;
				int16_t interp;

				sample = ImSwapSigned16At(src);
				interp = (sample + ImSwapSigned16(src[2])) >> 1;
				*mixer.dst++ += gainL * sample;
				*mixer.dst++ += gainR * sample;
				*mixer.dst++ += gainL * interp;
				*mixer.dst++ += gainR * interp;
				++src;
			}
			{
				int16_t sample;

				sample = ImSwapSigned16At(src);
				*mixer.dst++ += gainL * sample;
				*mixer.dst++ += gainR * sample;
				*mixer.dst++ += gainL * sample;
				*mixer.dst++ += gainR * sample;
			}
			break;

		case 0x20000u:
			while (dstLen--) {
				int16_t sample;

				sample = ImSwapSigned16At(src);
				*mixer.dst++ += gainL * sample;
				*mixer.dst++ += gainR * sample;
				src += 2;
			}
			break;

		default: {
			uint32_t frac;

			frac = 0;
			while (dstLen--) {
				int16_t sample;

				sample = ImSwapSigned16At(src);
				*mixer.dst++ += gainL * sample;
				*mixer.dst++ += gainR * sample;
				frac += mixer.step;
				src += (int32_t)frac >> 16;
				frac = (uint16_t)frac;
			}
			break;
		}
	}
}

// FUNCTION: XWA 0x5925E3
void ImMixStereo(int16_t* src, int srcLen, unsigned int dstLen, int dstFrameOff, int gain) {
	int* dst;
	uint32_t step;

	dst = &g_imMixBuffer[2 * dstFrameOff];
	step = ((uint32_t)srcLen << 16) / dstLen;
	switch (step) {
		case 0x10000u:
			dstLen *= 2;
			while (dstLen--) {
				int16_t left;

				left = ImSwapSigned16At(src);
				*dst += left * gain;
				++dst;
				++src;
			}
			break;

		case 0x8000u:
			srcLen -= 1;
			while (srcLen--) {
				int16_t interpRight;
				int16_t interpLeft;
				int16_t right;
				int16_t left;

				left = ImSwapSigned16At(src);
				right = ImSwapSigned16At(src + 1);
				interpLeft = (int16_t)((left + ImSwapSigned16At(src + 2)) >> 1);
				interpRight = (int16_t)((right + ImSwapSigned16At(src + 3)) >> 1);
				dst[0] += left * gain;
				++dst;
				dst[0] += right * gain;
				++dst;
				dst[0] += interpLeft * gain;
				++dst;
				dst[0] += interpRight * gain;
				++dst;
				src += 2;
			}
			{
				int16_t right;
				int16_t left;

				left = ImSwapSigned16At(src);
				right = ImSwapSigned16At(src + 1);
				dst[0] += left * gain;
				++dst;
				dst[0] += right * gain;
				++dst;
				dst[0] += left * gain;
				++dst;
				dst[0] += right * gain;
				++dst;
			}
			break;

		case 0x20000u:
			while (dstLen--) {
				int16_t sample;

				sample = ImSwapSigned16At(src);
				dst[0] += sample * gain;
				++dst;
				src += 1;
				sample = ImSwapSigned16At(src);
				dst[0] += sample * gain;
				++dst;
				src += 3;
			}
			break;

		default: {
			uint32_t frac;

			frac = 0;
			step *= 2;
			while (dstLen--) {
				int16_t right;
				int16_t left;

				left = ImSwapSigned16At(src);
				right = ImSwapSigned16At(src + 1);
				dst[0] += left * gain;
				++dst;
				dst[0] += right * gain;
				++dst;
				frac += step;
				src += (int32_t)frac >> 16;
				frac = (uint16_t)frac;
			}
			break;
		}
	}
}

// FUNCTION: XWA 0x591A30
int ImMixerInit(void) {
	int i;
	int j;
	unsigned char* dst;

	dst = g_imPanVolTable;
	for (i = 0; i < 17; ++i) {
		for (j = 0; j < 17; ++j) {
			*dst++ = (unsigned char)(int)((sin((double)j * 3.1415 / 32.0) * (double)i + 0.5) * 8.0);
		}
	}
	return 0;
}

// FUNCTION: XWA 0x591ABC
int ImMixerTerminate(void) { return 0; }

// FUNCTION: XWA 0x591AC3
int ImClearMixBuffer(void) {
	memset(g_imMixBuffer, 0, sizeof(g_imMixBuffer));
	return 0;
}

// FUNCTION: XWA 0x591C28
int ImDownmixOutput(int16_t* dst, int frames) {
	int initialFrames;
	int16_t* output;
	{
		int* mix;

		mix = g_imMixBuffer;
		output = dst;
		initialFrames = frames;
		if (!g_imMixBuffer || !dst || !frames) {
			return -1;
		}

		frames <<= 1;
		while (frames--) {
			int sample;

			sample = *mix++ >> 7;
			if (sample < -32768) {
				sample = -32768;
			} else if (sample > 32767) {
				sample = 32767;
			}
			*output++ = (int16_t)sample;
		}
		(void)initialFrames;
		return 0;
	}
}

// FUNCTION: XWA 0x59305B
int ImGetPlayBlock(void) {
	if (!((ImDSBufferCompat*)g_imDSoundBuffer)
			 ->vtable->getCurrentPosition(g_imDSoundBuffer, &g_imDSPlayCursor, &g_imDSWriteCursor)) {
		return (g_imDSPlayCursor / 0x2000) % g_imDSBlockCount;
	}
	return -1;
}

// FUNCTION: XWA 0x592C89
void ImServiceDSBuffer(void** outPtr, unsigned int* outLen, int* outRate) {
	int result;
	int playBlock;

	*outLen = 0;
	if (!g_imDSoundBuffer) {
		return;
	}
	if (IM_ATOMIC_LOAD(g_imDSPaused)) {
		return;
	}

	g_imDSFillToggle ^= 1;
	if (!g_imDSFillToggle) {
		if (!g_imDSLocked) {
			return;
		}

		if (g_imDSLockPtr2 && g_imDSLockBytes2) {
			memcpy_0(g_imDSLockPtr1, g_imOutputStage, g_imDSLockBytes1);
			memcpy_0(g_imDSLockPtr2, (char*)g_imOutputStage + g_imDSLockBytes1, g_imDSLockBytes2);
		}
		result = ((ImDSBufferCompat*)g_imDSoundBuffer)
					 ->vtable->unlock(g_imDSoundBuffer, g_imDSLockPtr1, g_imDSLockBytes1, g_imDSLockPtr2,
									  g_imDSLockBytes2);
		g_imDSLocked = 0;
		return;
	}

	playBlock = ImGetPlayBlock();
	if (playBlock == -1 || playBlock == (g_imDSWriteBlock + 1) % g_imDSBlockCount) {
		return;
	}

	if (g_imDSWriteBlock < 0) {
		g_imDSWriteBlock = playBlock;
	}
	g_imDSWriteBlock = (g_imDSWriteBlock + 1) % g_imDSBlockCount;

	result = ((ImDSBufferCompat*)g_imDSoundBuffer)
				 ->vtable->lock(g_imDSoundBuffer, g_imDSWriteBlock << 13, 0x2000, &g_imDSLockPtr1,
								&g_imDSLockBytes1, &g_imDSLockPtr2, &g_imDSLockBytes2, 0);
	if (result) {
		return;
	}

	if (g_imDSLockPtr2 && g_imDSLockBytes2) {
		*outPtr = g_imOutputStage;
		*outLen = 2048;
	} else {
		*outPtr = g_imDSLockPtr1;
		*outLen = g_imDSLockBytes1 >> 2;
	}
	*outRate = 22050;
	g_imDSLocked = 1;
}

// FUNCTION: XWA 0x5929C0
int ImWaveInit(void) {
	IM_ATOMIC_STORE(g_imDSPaused, 1);
	g_imDirectSound = g_imDirectSoundDevice;
	g_imDSWriteBlock = -1;
	ImCreateSoundBuffer(5);
	if (!g_imDSoundBuffer) {
	} else {
		IM_ATOMIC_STORE(g_imDSPaused, 0);
		if (!ImWaveOutStart()) {
		} else {
			return 0;
		}
	}
	ImWaveTerminate();
	return -1;
}

// FUNCTION: XWA 0x592A19
int ImWaveOutStart(void) {
	unsigned int startTime;
	{
		ImTimeCaps caps;
		{
			int retries;

			if (!g_imWaveCsInitialized) {
				ImInitializeCriticalSection(&g_imWaveCritSec);
				g_imWaveCsInitialized = 1;
			}
			if (ImTimeGetDevCaps(&caps, 8)) {
				ImLog("timeGetDevCaps failed. (%d)", 79);
				return 0;
			}

			IM_ATOMIC_STORE(g_imTimerTicks, 0);
			uPeriod = (caps.wPeriodMin > 50 ? caps.wPeriodMin : 50) < caps.wPeriodMax
						  ? (caps.wPeriodMin > 50 ? caps.wPeriodMin : 50)
						  : caps.wPeriodMax;
			uDelay = 1000 / uPeriod;
			if (ImTimeBeginPeriod(uPeriod)) {
				ImLog("timeBeginPeriod failed. (%d)", 89);
			}
			uTimerID = ImTimeSetEvent(uDelay, 0, ImTimerCallback, 15, 1);
			if (!uTimerID) {
				ImLog("timeSetEvent failed. (%d)", 93);
			}

			startTime = ImTimeGetTime();
			IM_ATOMIC_STORE(g_imTimerTicks, 0);
			retries = 3;
			while (retries-- && !IM_ATOMIC_LOAD(g_imTimerTicks)) {
				while (ImTimeGetTime() - startTime < 4000 && !IM_ATOMIC_LOAD(g_imTimerTicks)) {
				}
				if (!IM_ATOMIC_LOAD(g_imTimerTicks)) {
					if (retries == 2) {
						ImLog("iMUSE timer bug encountered, if you get sound this time, please note it in "
							  "the bug "
							  "db.\n");
					}
					ImTimeKillEvent(uTimerID);
					ImTimeEndPeriod(uPeriod);
					if (ImTimeBeginPeriod(uPeriod)) {
						ImLog("timeBeginPeriod failed. (%d)", 109);
					}
					uTimerID = ImTimeSetEvent(uDelay, 0, ImTimerCallback, 15, 1);
					if (!uTimerID) {
						ImLog("timeSetEvent failed. (%d)", 112);
					}
					startTime = ImTimeGetTime();
				}
			}

			if (!IM_ATOMIC_LOAD(g_imTimerTicks)) {
				return 0;
			}

			((ImDSBufferCompat*)g_imDSoundBuffer)->vtable->play(g_imDSoundBuffer, 0, 0, 1);
			return 1;
		}
	}
}

// FUNCTION: XWA 0x592C3B
void IM_STDCALL ImTimerCallback(unsigned int timerId, unsigned int msg, uintptr_t user, uintptr_t dw1,
								uintptr_t dw2) {
	(void)timerId;
	(void)msg;
	(void)user;
	(void)dw1;
	(void)dw2;

	if (IM_ATOMIC_LOAD(g_imDSPaused)) {
		return;
	}
	if (!g_imWaveCsInitialized) {
		return;
	}

	ImEnterCriticalSection(&g_imWaveCritSec);
	if (!IM_ATOMIC_LOAD(g_imDSPaused)) {
		ImHeartbeat();
	}
	ImLeaveCriticalSection(&g_imWaveCritSec);
	IM_ATOMIC_INC(g_imTimerTicks);
}

// FUNCTION: XWA 0x592E4F
int ImWaveTerminate(void) {
	IM_ATOMIC_STORE(g_imDSPaused, 1);
	ImWaveOutStop();
	if (hObject) {
		ImCloseHandle(hObject);
		hObject = NULL;
	}
	if (g_imDSoundBuffer) {
#ifndef XWA_MODERN
		((ImDSBufferCompat*)g_imDSoundBuffer)->vtable->release(g_imDSoundBuffer);
#else
		ImCallRelease(g_imDSoundBuffer);
#endif
		g_imDSoundBuffer = NULL;
	}
	return 0;
}

// FUNCTION: XWA 0x592EA9
void ImWaveOutStop(void) {
	if (g_imDSoundBuffer) {
		ImCallStop(g_imDSoundBuffer);
	}
#ifdef XWA_MODERN
	/* DEVIATION: killed outside g_imWaveCritSec, unlike the original. ImTimeKillEvent
	   waits for an in-flight callback, which takes that same section. */
	ImTimeKillEvent(uTimerID);
	ImEnterCriticalSection(&g_imWaveCritSec);
	ImTimeEndPeriod(uPeriod);
	ImLeaveCriticalSection(&g_imWaveCritSec);
#else
	ImEnterCriticalSection(&g_imWaveCritSec);
	ImTimeKillEvent(uTimerID);
	ImTimeEndPeriod(uPeriod);
	ImLeaveCriticalSection(&g_imWaveCritSec);
#endif
	if (g_imWaveCsInitialized) {
		ImDeleteCriticalSection(&g_imWaveCritSec);
		g_imWaveCsInitialized = 0;
	}
}

// FUNCTION: XWA 0x592F0C
void ImCreateSoundBuffer(int blockCount) {
	ImDSBufferDescCompat desc;
	{
		int result;

		if (g_imDSoundBuffer) {
#ifdef XWA_MODERN
			result = ImCallRelease(g_imDSoundBuffer);
#else
			result = ((ImDSBufferCompat*)g_imDSoundBuffer)->vtable->release(g_imDSoundBuffer);
#endif
		}
		g_imDSoundBuffer = NULL;

		if (blockCount < 3) {
			blockCount = 3;
		}
		if (blockCount > 16) {
			blockCount = 16;
		}
		g_imDSBlockCount = blockCount;

		memset(&desc, 0, sizeof(desc));
		desc.dwSize = 20;
		desc.dwFlags = 0x10088;
		desc.dwBufferBytes = (uint32_t)g_imDSBlockCount << 13;
		desc.dwReserved = 0;
		desc.lpwfxFormat = &g_imOutputWaveFormat;

#ifdef XWA_MODERN
		result = ImCallCreateSoundBuffer(g_imDirectSound, &desc, &g_imDSoundBuffer);
#else
		result = ((ImDirectSoundCompat*)g_imDirectSound)
					 ->vtable->createSoundBuffer(g_imDirectSound, &desc, &g_imDSoundBuffer, NULL);
#endif
		if (!result) {
			ImDSDword lockBytes1;
			void* lockPtr1;
			void* lockPtr2;
			ImDSDword lockBytes2;

#ifdef XWA_MODERN
			lockPtr1 = NULL;
			lockPtr2 = NULL;
			lockBytes1 = 0;
			lockBytes2 = 0;
#endif

#ifdef XWA_MODERN
			ImCallSetVolume(g_imDSoundBuffer, 0);
			result = ImCallLock(g_imDSoundBuffer, 0, (uint32_t)g_imDSBlockCount << 13, &lockPtr2, &lockBytes2,
								&lockPtr1, &lockBytes1, 0);
#else
			((ImDSBufferCompat*)g_imDSoundBuffer)->vtable->setVolume(g_imDSoundBuffer, 0);
			result = ((ImDSBufferCompat*)g_imDSoundBuffer)
						 ->vtable->lock(g_imDSoundBuffer, 0, (uint32_t)g_imDSBlockCount << 13, &lockPtr2,
										&lockBytes2, &lockPtr1, &lockBytes1, 0);
#endif
			if (!result) {
				memset(lockPtr2, 0, lockBytes2);
				if (lockPtr1) {
					memset(lockPtr1, 0, lockBytes1);
				}
#ifdef XWA_MODERN
				result = ImCallUnlock(g_imDSoundBuffer, lockPtr2, lockBytes2, lockPtr1, lockBytes1);
#else
				result = ((ImDSBufferCompat*)g_imDSoundBuffer)
							 ->vtable->unlock(g_imDSoundBuffer, lockPtr2, lockBytes2, lockPtr1, lockBytes1);
#endif
			}
		}
	}
}

// FUNCTION: XWA 0x59190C
int ImListAdd(ImListNode** head, ImListNode* node) {
	if (!node || node->prev || node->next) {
		ImPrintf((char*)"iMUSE.C: list arg err when adding...");
		return -5;
	}

	node->next = *head;
	if (*head) {
		(*head)->prev = node;
	}
	node->prev = NULL;
	*head = node;
	return 0;
}

// FUNCTION: XWA 0x59196C
int ImListRemove(ImListNode** head, ImListNode* node) {
	ImListNode* cur = *head;

	if (!node || !*head) {
		ImPrintf((char*)"iMUSE.C: list arg err when removing...");
		return -5;
	}

	while (cur) {
		if (cur == node) {
			break;
		}
		cur = cur->next;
	}
	if (!cur) {
		ImPrintf((char*)"iMUSE.C: item not on list...");
		return -3;
	}

	if (node->next) {
		node->next->prev = node->prev;
	}
	if (node->prev) {
		node->prev->next = node->next;
	} else {
		*head = node->next;
	}
	node->next = NULL;
	node->prev = NULL;
	return 0;
}
