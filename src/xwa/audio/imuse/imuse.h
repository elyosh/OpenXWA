#ifndef XWA_AUDIO_IMUSE_IMUSE_H
#define XWA_AUDIO_IMUSE_IMUSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef XWA_MODERN
#include "aeron/sync.h"
#endif

typedef int (*ImHostCallback)(void);

typedef struct ImHostServices {
	int version;
	int (*printStatus)(const char* fmt, ...);
	int (*printMessage)(const char* fmt, ...);
	int (*printWarning)(const char* fmt, ...);
	int (*printError)(const char* fmt, ...);
	int (*printDebug)(const char* fmt, ...);
	void (*assertFail)(const char* expr, const char* file, int line);
	int (*registerAtExit)(void (*fn)(void));
	void* (*allocMem)(unsigned int size);
	void (*freeMem)(void* block);
	void* (*reallocMem)(void* block, unsigned int size);
	int (*getTime)(void);
	void* (*openFile)(const char* path, const char* mode);
	int (*closeFile)(void* stream);
	unsigned int (*readFile)(void* stream, void* dst, unsigned int size);
	char* (*readLine)(void* stream, char* dst, int size);
	int (*writeFile)(void* stream, void* src, unsigned int size);
	int (*atEof)(void* stream);
	int (*tellFile)(void* stream);
	int (*seekFile)(void* stream, int offset, int origin);
	int (*getFileSize)(const char* path);
	int (*filePrintf)(void* stream, const char* fmt, ...);
	void* (*allocHandle)(unsigned int size);
	void (*freeHandle)(void* handle);
	void* (*reallocHandle)(void* handle, unsigned int size);
	void* (*lockHandle)(void* handle);
	void (*unlockHandle)(void* handle);
} ImHostServices;

typedef struct ImApiTable {
	int (*init)(ImHostServices* services, struct ImApiTable* api);
	int reserved;
	int (*startup)(void);
	void (*shutdown)(void);
	int (*saveGame)(void (*callback)(void*, int));
	int (*restoreGame)(void (*callback)(void*, int));
	int (*pause)(void);
	int (*resume)(void);
} ImApiTable;

typedef struct ImDispatch ImDispatch;
typedef struct ImStreamSlot ImStreamSlot;
typedef struct ImStreamZone ImStreamZone;

typedef struct ImSoundBuffer {
	void* data;
	int   size;
	int   field_8;
	int   field_C;
} ImSoundBuffer;

typedef void* (*ImSoundAddrFn)(unsigned int soundId);
typedef ImSoundBuffer* (*ImGetSoundBufferFn)(int index);
typedef void (*ImScriptMarkerFn)(char* marker);
typedef int (*ImHostLoadResourceFn)(char* name, int mode);
typedef int (*ImHostFreeResourceFn)(int sound);
typedef int (*ImHostOpenResourceFn)(char* name, int resId);
typedef int (*ImHostCloseResourceFn)(int handle);
typedef int (*ImHostSeekResourceFn)(int resId, int offset, int origin);
typedef int (*ImHostReadResourceFn)(int resId, char* buf, unsigned int len);
typedef void (*ImLogFn)(const char* fmt, ...);

typedef enum ImSoundParam {
	P_NONE         = 0x000,
	P_COUNT        = 0x100,
	P_STATUS       = 0x200,
	P_PARAM_300    = 0x300,
	P_VGROUP       = 0x400,
	P_PRIORITY     = 0x500,
	P_VOLUME       = 0x600,
	P_PAN          = 0x700,
	P_DETUNE       = 0x800,
	P_PITCH        = 0x900,
	P_PARAM_A00    = 0xA00,
	P_IS_STREAMING = 0x1800,
	P_STREAM_SIZE  = 0x1900,
	P_STREAM_POS   = 0x1A00,
} ImSoundParam;

typedef struct ImTrack {
	struct ImTrack* prev;
	struct ImTrack* next;
	ImDispatch*     dispatch;
	int             soundId;
	int             field_10;
	int             volumeGroup;
	int             priority;
	int             volume;
	int             effectiveVolume;
	int             pan;
	int             detune;
	int             transpose;
	int             pitch;
	int             field_34;
	int             hookId;
} ImTrack;

typedef struct ImListNode {
	struct ImListNode* prev;
	struct ImListNode* next;
} ImListNode;

typedef struct ImSoundFile {
	struct ImSoundFile* prev;
	struct ImSoundFile* next;
	int                 sound;
	char                name[32];
	int                 field_2C;
	int                 refCount;
} ImSoundFile;

struct ImDispatch {
	ImTrack* track;
	int      fmtA;
	int      fmtB;
	int      fmtC;
	int      atRegionStart;
	int      currentOffset;
	int      regionEndOffset;
	char     map[8192];
	ImStreamSlot* stream;
	int      streamSize;
	ImStreamZone* pendingJumpZones;
	int      streamStarved;
	void*    jumpBuffer;
	int      fadeReadOffset;
	int      fadeBytesAvailable;
	int      fadeBitsPerSample;
	int      fadeSampleRate;
	int      fadeChannels;
	int      jumpFadeHoldMask;
	int      jumpFadeHoldFrames;
	int      crossfadeQ16Pos;
	int      crossfadeQ16Step;
	int      jumpInProgress;
};

typedef struct ImFade {
	int active;
	int soundId;
	int param;
	int currentValue;
	int ticksRemaining;
	int totalTicks;
	int stepValue;
	int errorStep;
	int errorAccum;
	int errorSign;
} ImFade;

typedef struct ImMarkerTrigger {
	int  soundId;
	char marker[256];
	int  command;
	int  params[10];
	int  onceFlag;
} ImMarkerTrigger;

typedef struct ImTrigger {
	int timer;
	int command;
	int params[10];
} ImTrigger;

struct ImStreamSlot {
	int bufId;
	int filePos;
	int fileSize;
	int soundId;
	char* bufBase;
	int bufSize;
	int maxChunk;
	int refillThreshold;
	int maxRead;
	int writeCur;
	int readCur;
	int eof;
};

struct ImStreamZone {
	struct ImStreamZone* prev;
	struct ImStreamZone* next;
	int                  inUse;
	int                  streamOffset;
	int                  byteCount;
	int                  isPrefetch;
};

typedef struct ImMapChunk {
	int tag;
	int size;
} ImMapChunk;

typedef struct ImMapEvent {
	int tag;
	int size;
	int sourceOffset;
	int destOffset;
	int hookId;
	int fadeMs;
} ImMapEvent;

typedef struct ImVimaState {
	char    stepIndex[2];
	int16_t predictor[2];
} ImVimaState;

typedef struct ImMcmpStream {
	void*        file;
	int          totalSize;
	int          offset;
	int          blockCount;
	char*        readText;
	char**       blockCodec;
	int*         blockDecodedSize;
	int*         blockStartOffset;
	int*         blockCompressedSize;
	int*         blockFileOffset;
	int          cachedBlockIndex;
	int          segmentCapacity;
	void*        segmentData;
	ImVimaState  vimaState;
	char         path[80];
	char         padding_8A[2];
	void*        readBuffer;
	unsigned int readBufferCapacity;
} ImMcmpStream;

typedef struct ImScriptHost {
	ImSoundAddrFn fn0;
	ImSoundAddrFn fn1;
	ImScriptMarkerFn onSoundEnd;
	ImHostSeekResourceFn seekResource;
	ImHostReadResourceFn readResource;
	ImGetSoundBufferFn getBuffer;
	ImLogFn log;
} ImScriptHost;

typedef struct ImScriptEntry {
	char filename[32];
	int  transType;
	int  id;
	char label[24];
	int  group;
	int  hook;
	int  fadeMs;
} ImScriptEntry;

typedef struct ImWaveFormatEx {
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	uint16_t cbSize;
} ImWaveFormatEx;

typedef struct ImTimeCaps {
	unsigned int wPeriodMin;
	unsigned int wPeriodMax;
} ImTimeCaps;

typedef struct ImCriticalSection {
#ifdef XWA_MODERN
	void* mutex; /* AeronMutex* */
#else
	void*    debugInfo;
	int32_t  lockCount;
	int32_t  recursionCount;
	void*    owningThread;
	void*    lockSemaphore;
	uintptr_t spinCount;
#endif
} ImCriticalSection;

#ifdef XWA_MODERN
/* Backs the wave, nav and mcmp critical sections. Enter/Leave on an
   uninitialised section are no-ops. */
void ImPlatformCsInit(ImCriticalSection* critSec);
void ImPlatformCsEnter(ImCriticalSection* critSec);
void ImPlatformCsLeave(ImCriticalSection* critSec);
void ImPlatformCsDelete(ImCriticalSection* critSec);

/* Marks globals shared between the game thread and the iMUSE timer thread. */
#define IM_CROSS_THREAD volatile

#define IM_ATOMIC_LOAD(v) Aeron_AtomicLoad((volatile int*)&(v))
#define IM_ATOMIC_STORE(v, x) Aeron_AtomicStore((volatile int*)&(v), (int)(x))
#define IM_ATOMIC_INC(v) ((void)Aeron_AtomicAdd((volatile int*)&(v), 1))
#define IM_ATOMIC_DEC(v) ((void)Aeron_AtomicAdd((volatile int*)&(v), -1))
#else
#define IM_CROSS_THREAD
#define IM_ATOMIC_LOAD(v) (v)
#define IM_ATOMIC_STORE(v, x) ((void)((v) = (x)))
#define IM_ATOMIC_INC(v) ((void)++(v))
#define IM_ATOMIC_DEC(v) ((void)--(v))
#endif

extern IM_CROSS_THREAD int g_imBusyCount;
extern void*    g_imDirectSoundDevice;
extern int      g_imRunning;
extern int      g_imInitialized;
extern int      g_imSeqEndPending;
extern int      g_imGroupVolume[16];
extern int      g_imGainTable[16];
extern ImTrack* g_imActivePlayers;
extern char     g_imLogBuf[256];
extern int      g_imScriptInitialized;
extern ImHostServices* g_imHostServicesPtr;
extern ImSoundBuffer g_imSoundBuffers[3];
extern ImCriticalSection g_imMcmpCritSec;
extern ImMcmpStream  g_imMcmpStreams[32];
extern int           g_imMcmpStreamCursor;
extern int           g_imMcmpCsInitialized;
extern ImMcmpStream* g_imResHandles[5];
extern ImScriptHost  g_imScriptHost;
extern int           g_imScriptEnableConfig;
extern int           g_imScriptInitParam8Unused;
extern int           g_imScriptEnabled;
extern int           g_imScriptInitArg6;
extern int           g_imScriptInitArg7;
extern int           g_imCurMusicState;
extern int           g_imCurMusicSeq;
extern int           g_imPendingMusicSeq;
extern int           g_imScriptAttribs[58];
extern const ImScriptEntry g_imStateTable[29];
extern const ImScriptEntry g_imSeqTable[23];
extern int           g_imSoundBufferSize;
extern int           g_imSoundBufferField8;
extern int           g_imSoundBufferFieldC;
extern int           g_imSavedMasterVolume;
extern int           g_imLargeBufSize;
extern int           g_imLargeBufCount;
extern int           g_imSmallBufSize;
extern int           g_imSmallBufCount;
extern int           g_imTrackPoolSize;
extern int           g_imMaxTracks[7];
extern ImTrack       g_imTrackPool[16];
extern int           g_imEnginePaused;
extern int           g_imLastHeartbeatMs;
extern IM_CROSS_THREAD int g_imHeartbeatGuard;
extern int           g_imPauseCount;
extern int           g_imTriggerClockAccum;
extern int           g_imVolDuckClockAccum;
extern ImFade        g_imFades[16];
extern int           g_imFadesActive;
extern ImMarkerTrigger g_imMarkerTriggers[8];
extern ImTrigger       g_imTriggers[8];
extern char            g_imMarkerScratch[256];
extern int             g_imTriggersActive;
extern int             g_imMarkerDepth;
extern char            g_imEmptyMarker[8];
extern void*        g_imDispatchBuffer;
extern int*         g_imLargeBufFlags;
extern char*        g_imLargeBufBase;
extern int          g_imDpSwitchBufSize;
extern char*        g_imSmallBufBase;
extern int*         g_imSmallBufFlags;
extern ImStreamZone g_imStreamZones[50];
extern ImDispatch   g_imDispatchPool[16];
extern int          g_imMusicJumpsEnabled;
extern int          g_imStatStreamRefillThreshold;
extern int          g_imNavEventReady;
extern int          g_imStatStreamFill;
extern int          g_imStatStreamEof;
extern int          g_imNavBufSize;
extern ImCriticalSection g_imNavCritSec;
extern int          g_imStatStreamSize;
extern int          g_imNavCsInitialized;
extern ImStreamSlot g_imStreamSlots[3];
extern ImStreamSlot* g_imCurrentStream;
extern int          g_imStreamDirty;
extern ImHostLoadResourceFn g_imHostLoadSound;
extern ImHostFreeResourceFn g_imHostFreeSound;
extern ImHostOpenResourceFn g_imHostOpenStream;
extern ImHostCloseResourceFn g_imHostCloseStream;
extern ImSoundFile g_imSoundFiles[10];
extern ImSoundFile* g_imLoadedSoundList;
extern ImSoundFile* g_imStreamSoundList;
extern int          g_imFilelistReady;
extern unsigned char g_imPanVolTable[17 * 17];
extern int           g_imMixBuffer[4096];
extern ImWaveFormatEx g_imOutputWaveFormat;
extern int          g_imRenderRate;
extern unsigned int g_imRenderFrameLen;
extern int          g_imRenderFramesPending;
extern void*        g_imRenderWritePtr[4];
extern int          g_imDSWriteBlock;
extern int          g_imDSWriteCursor;
extern uint32_t     g_imDSLockBytes2;
extern unsigned int uDelay;
extern IM_CROSS_THREAD unsigned int g_imTimerTicks;
extern int          g_imDSBlockCount;
extern ImCriticalSection g_imWaveCritSec;
extern int          g_imDSPlayCursor;
extern uint32_t     g_imDSLockBytes1;
extern unsigned int uPeriod;
extern unsigned int uTimerID;
extern void*        g_imDirectSound;
extern void*        g_imDSoundBuffer;
extern IM_CROSS_THREAD int g_imDSPaused;
extern void*        hObject;
extern IM_CROSS_THREAD int g_imWaveCsInitialized;
extern int          g_imDSFillToggle;
extern int          g_imDSLocked;
extern void*        g_imDSLockPtr1;
extern void*        g_imDSLockPtr2;
extern int16_t      g_imOutputStage[4096];
extern int          g_imJumpZoneSize;
extern int          g_imPredictTrackHook;

void ImUpdate(void);
int ImGetMusicStreamStatus(int* outBufSize, int* outRefillThreshold, int* outFill, int* outEof);
int ImFillStreamsWhileMusicCritical(int extraCount);
int ImIsMusicCritical(void);
void ImFloodMusicBuffer(void);
int ImIsMusicBufferFull(void);
void ImSetScriptEnableConfig(int enabled);
void ImSetScriptInitParam8(int value);
void ImSetDirectSoundDevice(void* directSound);
int ImInit(ImHostServices* hostIoCallbacks, ImApiTable* outApiTable);
int ImStartup(void);
void ImShutdown(void);
int ImSaveGame(void (*callback)(void*, int));
int ImRestoreGame(void (*callback)(void*, int));
int ImSaveState(char* buffer, int bufSize);
int ImRestoreState(char* buffer);
int ImInitSubsystems(void);
int ImTeardownSubsystems(void);
int ImShutdownSubsystems(void);
int ImInitializeScript(ImHostLoadResourceFn loadFn, ImHostFreeResourceFn freeFn,
					   ImHostOpenResourceFn openStreamFn, ImHostCloseResourceFn closeStreamFn, int arg6,
					   int arg7, int enabled, int unused);
void* ImScriptHostStub0(unsigned int soundId);
void* ImScriptHostStub1(unsigned int soundId);
ImSoundBuffer* ImGetSoundBuffer(int index);
int ImHostLoadResource(char* name, int mode);
int ImHostFreeResource(int sound);
int ImHostOpenResource(char* name, int resId);
int ImHostCloseResource(int handle);
int ImHostSeekResource(int resId, int offset, int origin);
int ImHostReadResource(int resId, char* buf, unsigned int len);
void* ImResAddr(unsigned int soundId);
void* ImMapSoundAddr(unsigned int soundId);
int ImResSeek(unsigned int soundId, int offset, int origin);
int ImResRead(unsigned int soundId, char* dst, unsigned int len);
ImSoundBuffer* ImResBufInfo(int soundId);
int ImDecodeMcmpBlock(char* dst, char* mcmp);
int ImMcmpDecodedSize(const unsigned char* mcmp);
unsigned int ImMcmpRead(ImMcmpStream* stream, char* dst, unsigned int count);
int ImMcmpClose(ImMcmpStream* stream);
int ImMcmpTell(ImMcmpStream* stream);
int ImMcmpSeek(ImMcmpStream* stream, int offset, int origin);
int ImMcmpOffsetToBlock(ImMcmpStream* stream, unsigned int offset);
unsigned int ImReadBE16(const unsigned char* p);
int ImReadBE32(const unsigned char* p);
void ImWriteBE32(unsigned char* p, int value);
unsigned char* ImFindChunk(unsigned char* blob, const char* tag, int occurrence);
char* ImParseSoundHeader(char* src, int* pFormat, int* pSampleRate, int* pBitsPerSample, int* pChannels,
						 int* pField6, int* pDataSize, void** pSyncData, int* pSyncSize);
unsigned int ImCalcConvertedSize(int outFormat, unsigned int rate, unsigned int bits, unsigned int channels,
								 char* src);
int ImEncodeSoundToMcmp(char* dst, char* src);
int ImBuildSyncChunk(char* dst, char* pcmData, unsigned int sampleRate, unsigned char codeBaseA,
					 unsigned char codeBaseB, int windowDivisor, int bits, int channels, int alignFlag,
					 unsigned int pcmSize);
int ImBuildMap(void* dst, int formatType, unsigned int sampleRate, unsigned int bitsPerSample,
			   unsigned int channels, unsigned int dataSize, void* syncChunk);
ImMcmpStream* ImResFopen(char* path, const char* mode);
void ImVimaResetState(ImVimaState* state);
unsigned int ImVimaEncode(ImVimaState* state, unsigned char* dst, int16_t* samples, unsigned int byteCount,
						  unsigned int channelCount);
unsigned int ImVimaEncodeBlock(ImVimaState* state, unsigned char* dst, int16_t* samples, unsigned int sampleCount,
							   unsigned int channelCount, int nativeEndian, int continueState);
int ImVimaEncodeStats(unsigned char* dst, int16_t* samples, int byteCount, FILE* statsFile);
void ImVimaDecodeBlock(ImVimaState* state, int16_t* dst, char* src, unsigned int sampleCount);
unsigned int ImDecodeWvsm(int16_t* dst, unsigned char* src, int byteCount);
void ImVimaDecodeAdpcm(ImVimaState* state, int16_t* dst, unsigned char* src, int sampleCount,
					   unsigned int channelCount, int resetFlag);
void* ImAllocSoundBuffer(int index, int size, int field8, int fieldC);
int ImSetVolume(unsigned int group, unsigned int value);
int ImStartSound(unsigned int soundId, int priority);
int ImStartStream(int soundId, int priority, int param);
int ImSwitchStream(int curSoundId, int newSoundId, int fadeMs, unsigned int switchFlagsLow,
				   unsigned int switchFlagsHigh);
int ImCommandDispatch(int command, ...);
int ImIncBusyCount(void);
void ImDecBusyCount(void);
int ImGroupsInit(void);
int ImSetGroupVol(unsigned int group, int value);
int ImGetGroupGain(unsigned int group);
int ImApplyVolumes(void);
int ImGetNextSound(int prevSoundId);
unsigned int ImNextSoundId(unsigned int prevSoundId);
int ImGetHook(int soundId);
int ImGetHookCore(int soundId);
int ImSetHook(int soundId, int hookId);
int ImSetHookCore(int soundId, unsigned int hookId);
int ImGetFadeOutGain(ImDispatch* dispatch);
int ImGetFadeInGain(ImDispatch* dispatch, int delta);
int ImDpFreeStream(ImTrack* track);
void ImRenderPlayer(ImTrack* track, unsigned int frames, unsigned int outRate);
void ImRenderFrame(void);
int ImProcessStreams(void);
void ImFreeTrack(ImTrack* track);
ImTrack* ImAllocTrack(int priority);
int ImStartSoundCore(int soundId, int priority, int param);
int ImStopAllSoundsCore(void);
int ImStopSoundCore(int soundId);
int ImStopSound(int soundId);
int ImStopAllSounds(void);
int ImSaveTracks(void* buf, unsigned int bufSize);
int ImRestoreTracks(char* src);
int ImFadesInit(void);
int ImTriggersInit(void);
int ImTracksInit(void);
int ImWaveInit(void);
int ImWaveTerminate(void);
int ImMixerInit(void);
int ImMixerTerminate(void);
int ImClearMixBuffer(void);
int ImDownmixOutput(int16_t* dst, int frames);
int16_t* ImMixSource(int16_t* src, int count, int srcRate, int channels, int doMix, unsigned int dstFrames,
					 int dstSample, int volume, int pan);
void ImMixMonoNative(int16_t* src, int srcLen, unsigned int dstLen, int dstFrameOff, int gainL, int gainR);
void ImMixStereoNative(int16_t* src, int srcLen, unsigned int dstLen, int dstFrameOff, int gain);
void ImMixMono(int16_t* src, int srcLen, unsigned int dstLen, int dstFrameOff, int gainL, int gainR);
void ImMixStereo(int16_t* src, int srcLen, unsigned int dstLen, int dstFrameOff, int gain);
int ImGetPlayBlock(void);
void ImServiceDSBuffer(void** outPtr, unsigned int* outLen, int* outRate);
int ImDispatchInit(void);
int ImDispatchTerminate(void);
ImDispatch* ImGetDispatch(int trackIdx);
ImStreamZone* ImAllocStreamZone(void);
int ImStreamerInit(void);
ImStreamSlot* ImStreamOpen(unsigned int bufId, int soundId, unsigned int maxRead);
int ImWaveOutStart(void);
void ImWaveOutStop(void);
void ImCreateSoundBuffer(int blockCount);
#ifndef XWA_MODERN
void __stdcall ImTimerCallback(unsigned int timerId, unsigned int msg, uintptr_t user, uintptr_t dw1, uintptr_t dw2);
#else
void ImTimerCallback(unsigned int timerId, unsigned int msg, uintptr_t user, uintptr_t dw1, uintptr_t dw2);
#endif
void ImHeartbeat(void);
int ImStreamGetOffset(ImStreamSlot* stream);
int ImStreamGetFileSize(ImStreamSlot* stream);
int ImGetStreamFill(ImStreamSlot* stream);
int ImGetStreamStatus(ImStreamSlot* stream, int* outBufSize, int* outRefillThreshold, int* outFill, int* outEof);
int ImProcessStreamSwitches(void);
void* ImStreamPeek(ImStreamSlot* stream, int offset, unsigned int len);
void* ImStreamGet(ImStreamSlot* stream, unsigned int count);
int ImStreamConsume(ImStreamSlot* stream, unsigned int count);
int ImStreamSetAvail(ImStreamSlot* stream, unsigned int count);
int ImFeedStream(ImStreamSlot* stream, char* src, int count, int feedFlag);
int ImStreamRefill(ImStreamSlot* stream);
void* ImAllocFadeBuf(unsigned int* pSize);
void ImFreeFadeBuf(void* buf);
void ImCommitJump(ImDispatch* dispatch, ImStreamZone* zone);
int ImListAdd(ImListNode** head, ImListNode* node);
int ImListAdd2(ImListNode** head, ImListNode* node);
int ImListRemove(ImListNode** head, ImListNode* node);
int ImListRemove2(ImListNode** head, ImListNode* node);
int ImMatchJumpHook(int* trackHook, int hookId);
int ImSyncLookup(char* syncChunk, unsigned int position, unsigned char* outCodeHi,
				 unsigned char* outCodeLo);
int ImGetMap(ImDispatch* dispatch);
int ImConvertMap(char* src, char* dst);
int ImNavigateMap(ImDispatch* dispatch);
void ImServiceStreamJumps(void);
void ImPredictStream(ImDispatch* dispatch);
void ImPrepareToJump(ImDispatch* dispatch, ImStreamZone* zone, ImMapEvent* jumpEvent, int force);
ImMapEvent* ImFindJump(ImMapChunk* region, ImStreamZone* zone, int* trackHook);
ImMapEvent* ImGetNextMapEvent(ImMapChunk* map, int offset, ImMapEvent* prevEvent);
int ImDispatchStartStream(ImTrack* track, int streamSize);
int ImDpSwitchStream(int curSoundId, int newSoundId, unsigned int fadeMs, uint64_t switchFlags);
int ImDeferCommandCore(int delayTicks, int command, ...);
int ImDeferCommand(int delayTicks, int command, ...);
void ImLog(const char* fmt, ...);
int ImPrintf(char* format, ...);
int ImSaveScript(int* buf, int bufSize);
int ImRestoreScript(int* buf);
int ImRefreshScript(void);
int ImSetState(int state);
int ImSetSequence(int seqId);
int ImSetCuePoint(int cuePoint);
int ImSetAttribute(int index, int value);
int ImSetParam(int soundId, ImSoundParam param, int value);
int ImGetParam(int soundId, ImSoundParam param);
int ImFadeParam(int soundId, ImSoundParam param, int targetValue, int timeMs);
int ImGetStreamingSoundPos(void);
int ImPause(void);
int ImResume(void);
void ImPauseEngine(void);
void ImResumeEngine(void);
int ImPauseRefInc(void);
int ImResumeRefDec(void);
int ImIsValidSoundId(unsigned int soundId);
int ImFeedSound(int soundId, void* src, int len, int feedFlag);
int ImFeedSoundCore(int soundId, void* src, int len, int feedFlag);
int ImQueryStream(int soundId, int* outBufSize, int* outRefillThreshold, int* outFill, int* outEof);
int ImQueryStreamCore(int soundId, int* outBufSize, int* outRefillThreshold, int* outFill, int* outEof);
int ImTrSetParam(int soundId, ImSoundParam param, int value);
int ImTrGetParam(int soundId, ImSoundParam param);
int ImSaveFades(void* buf, unsigned int bufSize);
int ImRestoreFades(void* buf);
int ImFadeParamCore(int soundId, ImSoundParam param, int targetValue, int timeMs);
int ImCancelFade(int soundId, int param);
void ImProcessFades(void);
int ImSaveTriggers(void* buf, unsigned int bufSize);
int ImRestoreTriggers(void* buf);
int ImSetTrigger(int soundId, char* marker, int command, ...);
int ImSetTriggerCore(int soundId, char* marker, int command, ...);
int ImCheckTrigger(int soundId, char* marker, int command);
int ImCheckTriggerCore(int soundId, char* marker, int command);
int ImCountPendingStarts(int soundId);
void ImTgProcessMarker(int soundId, char* markerText);
void ImProcessTriggers(void);
void ImFireMarkerTrigger(ImMarkerTrigger* trigger, char* marker);
void ImFireTimedTrigger(ImTrigger* trigger);
int ImClearTrigger(int soundId, char* marker, int command);
int ImClearTriggerCore(int soundId, char* marker, int command);
int ImSaveDispatch(void* buf, unsigned int bufSize);
int ImRestoreDispatch(void* buf);
int ImRestoreDispatchStreams(void);
int ImFreeStream(ImStreamSlot* slot);
int ImStreamSeek(ImStreamSlot* slot, int bufId, int filePos);
int ImScriptCommand(int opcode, ...);
void ImScriptOnSoundEnd(char* marker);
int ImInitFilelist(ImHostLoadResourceFn loadFn, ImHostFreeResourceFn freeFn,
				   ImHostOpenResourceFn openStreamFn, ImHostCloseResourceFn closeStreamFn);
int ImFindSound(char* name);
int ImLoadSound(char* name, int mode);
int ImOpenSound(char* name, int mode);
ImSoundFile* ImFlushSounds(void);
void ImUnloadSound(int soundId);
void ImUnloadAll(void);
void ImCloseSound(int soundId);
void ImCloseAll(void);
int ImSaveFilelist(int* buf, unsigned int bufSize);
int ImRestoreFilelist(int* buf);
void ImScriptStopAll(void);
int ImScriptHandleInit(ImScriptHost* host, ImHostLoadResourceFn loadFn, ImHostFreeResourceFn freeFn,
					   ImHostOpenResourceFn openStreamFn, ImHostCloseResourceFn closeStreamFn, int arg6,
					   int arg7, int enabled);
int ImScriptHandleTerminate(void* host);
int ImScriptHandleSave(int* buf, int bufSize);
int ImScriptHandleRestore(int* buf);
int ImScriptHandleRefresh(void);
void ImScriptProcessTransition(ImScriptEntry* entry, int entryIndex, int isSequence);
int ImScriptSetState(int stateIdOrRoom);
int ImScriptSetSequence(int seqId);
int ImScriptSetCuePoint(void);
int ImScriptSetAttribute(unsigned int attrIndex, int value);

#ifdef __cplusplus
}
#endif

#endif
