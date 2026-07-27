#include "xwa/audio/imuse/imuse.h"

#include "aeron/log.h"
#include "xwa_runtime/compat/middleware_crt.h"
#include "xwa/util/memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0x784C78
ImMcmpStream* g_imResHandles[5];
// GLOBAL: XWA 0x784C90
ImSoundBuffer g_imSoundBuffers[3];
// GLOBAL: XWA 0x784CC0
ImCriticalSection g_imMcmpCritSec;
// GLOBAL: XWA 0x784CD8
ImMcmpStream g_imMcmpStreams[32];
// GLOBAL: XWA 0x785F58
int g_imSavedMasterVolume;
// GLOBAL: XWA 0x785F68
int g_imMcmpStreamCursor;
// GLOBAL: XWA 0x785F6C
int g_imMcmpCsInitialized;
// GLOBAL: XWA 0x788E50
ImTrack g_imTrackPool[16];
// GLOBAL: XWA 0x786074
int g_imTriggerClockAccum;
// GLOBAL: XWA 0x786078
int g_imVolDuckClockAccum;
// GLOBAL: XWA 0x789220
int g_imEnginePaused;
// GLOBAL: XWA 0x789238
ImMarkerTrigger g_imMarkerTriggers[8];
// GLOBAL: XWA 0x789BD8
ImTrigger g_imTriggers[8];
// GLOBAL: XWA 0x789D58
char g_imMarkerScratch[256];
// GLOBAL: XWA 0x789E58
int g_imTriggersActive;
// GLOBAL: XWA 0x789E5C
int g_imMarkerDepth;
// GLOBAL: XWA 0x789E60
char g_imEmptyMarker[8];
// GLOBAL: XWA 0x789E68
int g_imFadesActive;
// GLOBAL: XWA 0x789E70
ImFade g_imFades[16];
// GLOBAL: XWA 0x78A170
int* g_imLargeBufFlags;
// GLOBAL: XWA 0x78A174
char* g_imLargeBufBase;
// GLOBAL: XWA 0x78A178
int g_imDpSwitchBufSize;
// GLOBAL: XWA 0x78A17C
char* g_imSmallBufBase;
// GLOBAL: XWA 0x78A180
int g_imPredictTrackHook;
// GLOBAL: XWA 0x78A190
ImStreamZone g_imStreamZones[50];
// GLOBAL: XWA 0x78A640
int* g_imSmallBufFlags;
// GLOBAL: XWA 0x7AABF0
void* g_imDispatchBuffer;
// GLOBAL: XWA 0x78A648
ImDispatch g_imDispatchPool[16];
// GLOBAL: XWA 0xB0CF64
int g_imMaxTracks[7];
// GLOBAL: XWA 0xB0CF80
ImScriptHost g_imScriptHost;
// GLOBAL: XWA 0xB0CF9C
void* g_imDirectSoundDevice;

// GLOBAL: XWA 0x605680
int g_imLastHeartbeatMs = -1;
// GLOBAL: XWA 0x605684
IM_CROSS_THREAD int g_imBusyCount = 1;
// GLOBAL: XWA 0x784C70
ImHostServices* g_imHostServicesPtr;
// GLOBAL: XWA 0x60539C
int g_imSoundBufferSize = 528000;
// GLOBAL: XWA 0x6053A0
int g_imSoundBufferField8 = 44000;
// GLOBAL: XWA 0x6053A4
int g_imSoundBufferFieldC = 352000;
// GLOBAL: XWA 0x6053A8
int g_imLargeBufSize = 350000;
// GLOBAL: XWA 0x6053AC
int g_imLargeBufCount = 1;
// GLOBAL: XWA 0x6053B0
int g_imSmallBufSize = 44100;
// GLOBAL: XWA 0x6053B4
int g_imSmallBufCount = 4;
// GLOBAL: XWA 0x785F64
int g_imScriptInitParam8Unused = 300000;
// GLOBAL: XWA 0x6053BC
int g_imTrackPoolSize = 8;
// GLOBAL: XWA 0x6053C0
int g_imScriptEnableConfig = 1;
// GLOBAL: XWA 0x785F60
int g_imRunning;
// GLOBAL: XWA 0x78607C
int g_imInitialized;
// GLOBAL: XWA 0x786080
IM_CROSS_THREAD int g_imHeartbeatGuard;
// GLOBAL: XWA 0x786070
int g_imPauseCount;
// GLOBAL: XWA 0x785F70
char g_imLogBuf[256];

#ifdef XWA_MODERN
/* Managed MCMP streams share the ImMcmpStream type with cast raw-file
   handles. Pointer equality is defined for both kinds of handle. */
static int ImMcmpIsManagedStream(const ImMcmpStream* stream) {
	size_t i;

	for (i = 0; i < sizeof(g_imMcmpStreams) / sizeof(g_imMcmpStreams[0]); ++i) {
		if (stream == &g_imMcmpStreams[i]) {
			return 1;
		}
	}
	return 0;
}
#endif

// FUNCTION: XWA 0x585211
int ImInit(ImHostServices* hostIoCallbacks, ImApiTable* outApiTable) {
	g_imHostServicesPtr = hostIoCallbacks;
	outApiTable->init = ImInit;
	outApiTable->reserved = 0;
	outApiTable->startup = ImStartup;
	outApiTable->shutdown = ImShutdown;
	outApiTable->saveGame = ImSaveGame;
	outApiTable->restoreGame = ImRestoreGame;
	outApiTable->pause = ImPause;
	outApiTable->resume = ImResume;
	ImLog("iMUSE activated!\n");
	return 0;
}

// FUNCTION: XWA 0x58632C
static int ImMcmpBlockCmp(const void* key, const void* elem) {
	unsigned int offset;
	const unsigned int* block;

	offset = (unsigned int)(uintptr_t)key;
	block = (const unsigned int*)elem;
	if (offset >= block[0] && offset < block[1]) {
		return 0;
	}
	if (offset < block[0]) {
		return -1;
	}
	return 1;
}

// FUNCTION: XWA 0x588B7D
int ImMcmpDecodedSize(const unsigned char* mcmp) {
	const unsigned char* block;
	uint16_t blockCount;
	int totalSize;

	if (memcmp(mcmp, "MCMP", 4u)) {
		return 0;
	}

	blockCount = (uint16_t)ImReadBE16(mcmp + 4);
	totalSize = 0;
	block = mcmp + 7;
	while (blockCount--) {
		totalSize += ImReadBE32(block);
		block += 9;
	}
	return totalSize;
}

#ifndef XWA_MODERN
#pragma function(memcmp)
#endif
// FUNCTION: XWA 0x588C36
int ImDecodeMcmpBlock(char* dst, char* mcmp) {
	struct {
		int size;
		int dataSize;
		char state[8];
	} decode;

	if (memcmp(mcmp, "MCMP", 4u)) {
		return 1;
	}

#ifndef XWA_MODERN
	decode.size =
		(int)(((*(unsigned int*)(mcmp + 7)) >> 24) | (((*(unsigned int*)(mcmp + 7)) >> 8) & 0xff00) |
			  (((*(unsigned int*)(mcmp + 7)) & 0xff00) << 8) | ((*(unsigned int*)(mcmp + 7)) << 24));
#else
	decode.size =
		(int)(((unsigned int)(unsigned char)mcmp[7] << 24) | ((unsigned int)(unsigned char)mcmp[8] << 16) |
			  ((unsigned int)(unsigned char)mcmp[9] << 8) | (unsigned int)(unsigned char)mcmp[10]);
#endif
	memcpy_0(dst, mcmp + 36, (size_t)decode.size);
	ImParseSoundHeader(dst, NULL, NULL, NULL, NULL, NULL, &decode.dataSize, NULL, NULL);
	ImVimaResetState((ImVimaState*)decode.state);
	ImVimaDecodeBlock((ImVimaState*)decode.state, (int16_t*)(dst + decode.size), mcmp + decode.size + 36,
					  decode.dataSize);
	return 0;
}
#ifndef XWA_MODERN
#pragma intrinsic(memcmp)
#endif

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x58527C
int ImStartup(void) {
	ImLog("iMUSE startup!\n");
	if (!g_imDirectSoundDevice) {
		ImLog("No sound device given.\n");
		return 0;
	}

	memset(g_imResHandles, 0, sizeof(g_imResHandles));
	g_imScriptHost.fn0 = ImScriptHostStub0;
	g_imScriptHost.fn1 = ImScriptHostStub1;
	g_imScriptHost.getBuffer = ImGetSoundBuffer;
	g_imScriptHost.seekResource = ImHostSeekResource;
	g_imScriptHost.readResource = ImHostReadResource;
	g_imScriptHost.log = ImLog;

	if (ImInitSubsystems() == -1) {
		ImLog("iMuse failed to initialize");
		return 1;
	}

	if (!ImAllocSoundBuffer(2, g_imSoundBufferSize, g_imSoundBufferField8, g_imSoundBufferFieldC)) {
		ImLog("Unable to allocate music buffer");
	}

	ImInitializeScript(ImHostLoadResource, ImHostFreeResource, ImHostOpenResource, ImHostCloseResource, 0, 0,
					   g_imScriptEnableConfig, g_imScriptInitParam8Unused);
	g_imRunning = 1;
	return 0;
}

// FUNCTION: XWA 0x585583
void ImShutdown(void) {
	int i;

	ImLog("iMUSE shutdown!\n");
	if (!g_imDirectSoundDevice) {
		return;
	}
	if (!g_imRunning) {
		return;
	}

	g_imRunning = 0;
	for (i = 0; i < 3; ++i) {
		if (g_imSoundBuffers[i].data) {
			Memory_FreeTagged("IMSOUNDBUFFER", g_imSoundBuffers[i].data);
			g_imSoundBuffers[i].data = NULL;
		}
	}
	ImShutdownSubsystems();
}

// FUNCTION: XWA 0x585610
int ImSaveGame(void (*callback)(void*, int)) {
	char* buffer;
	int savedSize;

	buffer = (char*)Memory_AllocTagged("IMSAVEBUFFER", 0x20000u);
	if (!buffer) {
		return 1;
	}

	savedSize = ImSaveState(buffer, 0x20000);
	callback(&savedSize, 4);
	callback(buffer, savedSize);

	savedSize = ImSaveScript((int*)buffer, 0x20000);
	callback(&savedSize, 4);
	callback(buffer, savedSize);

	Memory_FreeTagged("IMSAVEBUFFER", buffer);
	buffer = NULL;
	return 0;
}

// FUNCTION: XWA 0x5856B2
int ImRestoreGame(void (*callback)(void*, int)) {
	char* buffer;
	int chunkSizeAndReserved[2];

	buffer = (char*)Memory_AllocTagged("IMSAVEBUFFER", 0x20000u);
	chunkSizeAndReserved[1] = 0;
	if (!buffer) {
		return 1;
	}

	ImStopAllSounds();
	callback(chunkSizeAndReserved, 4);
	callback(buffer, chunkSizeAndReserved[0]);
	if (!ImRestoreState(buffer)) {
		Memory_FreeTagged("IMSAVEBUFFER", buffer);
		buffer = NULL;
		return 1;
	}

	callback(chunkSizeAndReserved, 4);
	callback(buffer, chunkSizeAndReserved[0]);
	if (!ImRestoreScript((int*)buffer)) {
		Memory_FreeTagged("IMSAVEBUFFER", buffer);
		buffer = NULL;
		return 1;
	}

	Memory_FreeTagged("IMSAVEBUFFER", buffer);
	buffer = NULL;
	return 0;
}

// FUNCTION: XWA 0x58537E
void* ImScriptHostStub1(unsigned int soundId) {
	(void)soundId;
	return NULL;
}

// FUNCTION: XWA 0x585385
void* ImScriptHostStub0(unsigned int soundId) {
	(void)soundId;
	return NULL;
}

// FUNCTION: XWA 0x58538C
void* ImAllocSoundBuffer(int index, int size, int field8, int fieldC) {
	ImSoundBuffer* soundBuffer;

	soundBuffer = &g_imSoundBuffers[index];
	soundBuffer->data = Memory_AllocTagged("IMSOUNDBUFFER", (size_t)size);
	soundBuffer->size = size;
	soundBuffer->field_8 = field8;
	soundBuffer->field_C = fieldC;
	return soundBuffer->data;
}

// FUNCTION: XWA 0x5853D8
ImSoundBuffer* ImGetSoundBuffer(int index) { return &g_imSoundBuffers[index]; }

// FUNCTION: XWA 0x5853E8
int ImHostLoadResource(char* name, int mode) {
	(void)name;
	(void)mode;
	return 0;
}

// FUNCTION: XWA 0x5853EF
int ImHostFreeResource(int sound) {
	(void)sound;
	return 0;
}

// FUNCTION: XWA 0x5853F6
int ImHostOpenResource(char* name, int resId) {
	int slot;

	if (resId) {
		if (resId >= 10000 && resId < 10005) {
			g_imResHandles[resId - 10000] = ImResFopen(name, "rb");
		}
		ImLog("ResFopen returns %p preferredNumber %d\n",
			  (resId >= 10000 && resId < 10005) ? (void*)g_imResHandles[resId - 10000] : NULL, resId);
		return resId;
	}

	for (slot = 0; slot < 5; ++slot) {
		if (!g_imResHandles[slot]) {
			g_imResHandles[slot] = ImResFopen(name, "rb");
			if (g_imResHandles[slot]) {
				return slot + 10000;
			}
			return 0;
		}
	}

	ImLog("ihost_open unable to find free slot");
	return 0;
}

// FUNCTION: XWA 0x584F80
int ImHostCloseResource(int handle) {
	ImMcmpStream* stream;

	stream = g_imResHandles[handle - 10000];
	if (stream) {
		ImMcmpClose(stream);
		g_imResHandles[handle - 10000] = NULL;
		return 0;
	}
	return 1;
}

// FUNCTION: XWA 0x5854FB
int ImHostSeekResource(int resId, int offset, int origin) {
	ImMcmpStream* stream;

	if (resId >= 10000 && resId < 10005) {
		stream = g_imResHandles[resId - 10000];
		if (stream && !ImMcmpSeek(stream, offset, origin)) {
			return ImMcmpTell(stream);
		}
	}
	return 0;
}

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x585550
int ImHostReadResource(int resId, char* buf, unsigned int len) {
	ImMcmpStream* stream;

	stream = g_imResHandles[resId - 10000];
	if (!stream) {
		return 0;
	}
	return ImMcmpRead(stream, buf, len);
}

// FUNCTION: XWA 0x58DC40
void* ImResAddr(unsigned int soundId) {
	ImSoundAddrFn soundAddrFunc;

	soundAddrFunc = g_imScriptHost.fn1;
	if (ImIsValidSoundId(soundId) && soundAddrFunc) {
		return soundAddrFunc(soundId);
	}

	ImLog("ERR: soundAddrFunc failure in files.c...");
	return NULL;
}

// FUNCTION: XWA 0x58DC89
void* ImMapSoundAddr(unsigned int soundId) {
	ImSoundAddrFn mapAddrFunc;
	void* soundAddr;

	mapAddrFunc = g_imScriptHost.fn0;
	if (ImIsValidSoundId(soundId) && mapAddrFunc) {
		soundAddr = mapAddrFunc(soundId);
		return soundAddr;
	}

	ImLog("ERR: mapAddrFunc failure in files.c...");
	return NULL;
}

// FUNCTION: XWA 0x58DDCD
int ImResSeek(unsigned int soundId, int offset, int origin) {
	ImHostSeekResourceFn seekFunc;

	seekFunc = g_imScriptHost.seekResource;
	if (ImIsValidSoundId(soundId) && seekFunc) {
		return seekFunc((int)soundId, offset, origin);
	}

	ImLog("ERR: seekFunc failure in files.c...");
	return 0;
}

// FUNCTION: XWA 0x58DE16
int ImResRead(unsigned int soundId, char* dst, unsigned int len) {
	ImHostReadResourceFn readFunc;

	readFunc = g_imScriptHost.readResource;
	if (ImIsValidSoundId(soundId) && readFunc) {
		return readFunc((int)soundId, dst, len);
	}

	ImLog("ERR: readFunc failure in files.c...");
	return 0;
}

// FUNCTION: XWA 0x58DE5F
ImSoundBuffer* ImResBufInfo(int soundId) {
	ImGetSoundBufferFn bufInfoFunc;

	bufInfoFunc = g_imScriptHost.getBuffer;
	if (soundId && bufInfoFunc) {
		return bufInfoFunc(soundId);
	}

	ImLog("ERR: bufInfoFunc failure in files.c...");
	return NULL;
}

// FUNCTION: XWA 0x58620F
int ImMcmpTell(ImMcmpStream* stream) {
	ImMcmpStream* mcmpStream;

	mcmpStream = stream;
#ifdef XWA_MODERN
	if (!ImMcmpIsManagedStream(mcmpStream)) {
#else
	if (mcmpStream < g_imMcmpStreams || mcmpStream >= &g_imMcmpStreams[32]) {
#endif
		return g_imHostServicesPtr->tellFile(stream);
	}
	return mcmpStream->offset;
}

// FUNCTION: XWA 0x586247
int ImMcmpSeek(ImMcmpStream* stream, int offset, int origin) {
	ImMcmpStream* mcmpStream;

	mcmpStream = stream;
#ifdef XWA_MODERN
	if (!ImMcmpIsManagedStream(mcmpStream)) {
#else
	if (mcmpStream < g_imMcmpStreams || mcmpStream >= &g_imMcmpStreams[32]) {
#endif
		return g_imHostServicesPtr->seekFile(stream, offset, origin);
	}

	switch (origin) {
		case 0:
			mcmpStream->offset = offset;
			break;
		case 1:
			mcmpStream->offset += offset;
			break;
		case 2:
			mcmpStream->offset = mcmpStream->totalSize + offset;
			break;
		default:
			return 1;
	}
	return 0;
}

// FUNCTION: XWA 0x5862D3
int ImMcmpOffsetToBlock(ImMcmpStream* stream, unsigned int offset) {
	int* block;

	if (offset < 0u || offset >= (unsigned int)stream->totalSize) {
		return -1;
	}

	block = (int*)bsearch((const void*)(uintptr_t)offset, stream->blockStartOffset,
						  (size_t)stream->blockCount, sizeof(int), ImMcmpBlockCmp);
	if (!block) {
		return -1;
	}
	return (int)(block - stream->blockStartOffset);
}

#ifndef XWA_MODERN
typedef void(__stdcall* ImMcmpCriticalSectionFn)(ImCriticalSection* critSec);
ImMcmpCriticalSectionFn InitializeCriticalSection;
ImMcmpCriticalSectionFn EnterCriticalSection;
ImMcmpCriticalSectionFn LeaveCriticalSection;
#define ImMcmpInitializeCriticalSection InitializeCriticalSection
#define ImMcmpEnterCriticalSection EnterCriticalSection
#define ImMcmpLeaveCriticalSection LeaveCriticalSection
#else
#define ImMcmpInitializeCriticalSection ImPlatformCsInit
#define ImMcmpEnterCriticalSection ImPlatformCsEnter
#define ImMcmpLeaveCriticalSection ImPlatformCsLeave
#endif

#ifdef XWA_MODERN
static unsigned int ImReadBE32Unaligned(const unsigned char* p) {
	return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
}
#endif

#ifndef XWA_MODERN
#pragma function(memcmp)
#endif

// FUNCTION: XWA 0x5857F6
ImMcmpStream* ImResFopen(char* path, const char* mode) {
	struct ImMcmpOpenContext {
		unsigned int value;
		unsigned char* textCursor;
		ImMcmpStream* stream;
		unsigned int dataFileOffset;
		int totalDecodedSize;
		unsigned char* tableCursor;
		unsigned char* rawTable;
		uint16_t textSize;
		uint16_t textSizePadding;
		int index;
		char magic[4];
		char* layout;
		uint16_t blockCount;
		uint16_t blockCountPadding;
		size_t tableSize;
		void* file;
	} openContext;

	if (*mode == 'w' || *mode == 'a' || mode[1] == '+') {
		openContext.file = g_imHostServicesPtr->openFile(path, mode);
		return (ImMcmpStream*)openContext.file;
	}

	openContext.file = g_imHostServicesPtr->openFile(path, mode);
	if (!openContext.file) {
		return NULL;
	}

	g_imHostServicesPtr->readFile(openContext.file, openContext.magic, 4u);
	if (memcmp(openContext.magic, "MCMP", 4u) != 0) {
		g_imHostServicesPtr->seekFile(openContext.file, 0, 0);
		return (ImMcmpStream*)openContext.file;
	}

	if (!g_imMcmpCsInitialized) {
		ImMcmpInitializeCriticalSection(&g_imMcmpCritSec);
		g_imMcmpCsInitialized = 1;
	}

	ImMcmpEnterCriticalSection(&g_imMcmpCritSec);
	openContext.index = g_imMcmpStreamCursor;
	for (;;) {
		if (g_imMcmpStreams[openContext.index].file && (openContext.index + 1) % 32 != g_imMcmpStreamCursor) {
			openContext.index = (openContext.index + 1) % 32;
			continue;
		}
		break;
	}
	if (g_imMcmpStreams[openContext.index].file) {
		g_imHostServicesPtr->closeFile(openContext.file);
		return NULL;
	}

	openContext.stream = &g_imMcmpStreams[openContext.index];
	g_imMcmpStreamCursor = openContext.index;
	ImMcmpLeaveCriticalSection(&g_imMcmpCritSec);

	openContext.stream->file = openContext.file;
	openContext.stream->offset = 0;
	openContext.stream->readBuffer = NULL;
	openContext.stream->readBufferCapacity = 0;

	g_imHostServicesPtr->readFile(openContext.file, &openContext.blockCount, 2u);
	openContext.blockCount =
		(uint16_t)(((openContext.blockCount & 0xff) << 8) | (openContext.blockCount >> 8));
	openContext.stream->blockCount = openContext.blockCount;

	g_imHostServicesPtr->seekFile(openContext.file, 9 * openContext.blockCount + 6, 0);
	g_imHostServicesPtr->readFile(openContext.file, &openContext.textSize, 2u);
	openContext.textSize = (uint16_t)(((openContext.textSize & 0xff) << 8) | (openContext.textSize >> 8));

	openContext.tableSize = 9 * openContext.blockCount;
	openContext.rawTable = (unsigned char*)Memory_AllocTagged("IMTEMPDATA", openContext.tableSize);
	openContext.tableCursor = openContext.rawTable;
#ifdef XWA_MODERN
	openContext.stream->readText = (char*)Memory_AllocTagged(
		"IMREADTEXT", openContext.textSize + sizeof(char*) * (openContext.blockCount + 1) +
						  sizeof(int) * 4 * (openContext.blockCount + 1));
#else
	openContext.stream->readText =
		(char*)Memory_AllocTagged("IMREADTEXT", openContext.textSize + 20 * (openContext.blockCount + 1));
#endif

	openContext.layout = openContext.stream->readText;
	openContext.layout += openContext.textSize;
	openContext.stream->blockCodec = (char**)openContext.layout;
	openContext.layout += sizeof(char*) * (openContext.blockCount + 1);
	openContext.stream->blockDecodedSize = (int*)openContext.layout;
	openContext.layout += sizeof(int) * (openContext.blockCount + 1);
	openContext.stream->blockStartOffset = (int*)openContext.layout;
	openContext.layout += sizeof(int) * (openContext.blockCount + 1);
	openContext.stream->blockCompressedSize = (int*)openContext.layout;
	openContext.layout += sizeof(int) * (openContext.blockCount + 1);
	openContext.stream->blockFileOffset = (int*)openContext.layout;
	openContext.layout += sizeof(int) * (openContext.blockCount + 1);

	g_imHostServicesPtr->readFile(openContext.file, openContext.stream->readText, openContext.textSize);
	g_imHostServicesPtr->seekFile(openContext.file, 6, 0);
	g_imHostServicesPtr->readFile(openContext.file, openContext.tableCursor,
								  (unsigned int)openContext.tableSize);

	openContext.dataFileOffset = 9 * openContext.blockCount + openContext.textSize + 8;
	openContext.totalDecodedSize = 0;
	for (openContext.index = 0; openContext.index < openContext.blockCount; ++openContext.index) {
		openContext.textCursor = (unsigned char*)openContext.stream->readText;
		openContext.value = *openContext.tableCursor++;
		while (openContext.value--) {
			while (*openContext.textCursor++) {
			}
		}
		openContext.stream->blockCodec[openContext.index] = (char*)openContext.textCursor;

#ifdef XWA_MODERN
		openContext.value = ImReadBE32Unaligned(openContext.tableCursor);
		openContext.tableCursor += 4;
#else
		openContext.value = *(unsigned int*)openContext.tableCursor;
		openContext.tableCursor += 4;
		openContext.value = (openContext.value >> 24) | ((openContext.value >> 8) & 0xff00) |
							((openContext.value & 0xff00) << 8) | (openContext.value << 24);
#endif
		openContext.stream->blockDecodedSize[openContext.index] = (int)openContext.value;
		openContext.stream->blockStartOffset[openContext.index] = openContext.totalDecodedSize;
		openContext.totalDecodedSize += (int)openContext.value;

#ifdef XWA_MODERN
		openContext.value = ImReadBE32Unaligned(openContext.tableCursor);
		openContext.tableCursor += 4;
#else
		openContext.value = *(unsigned int*)openContext.tableCursor;
		openContext.tableCursor += 4;
		openContext.value = (openContext.value >> 24) | ((openContext.value >> 8) & 0xff00) |
							((openContext.value & 0xff00) << 8) | (openContext.value << 24);
#endif
		openContext.stream->blockCompressedSize[openContext.index] = (int)openContext.value;
		openContext.stream->blockFileOffset[openContext.index] = (int)openContext.dataFileOffset;
		openContext.dataFileOffset += openContext.value;
	}

	openContext.stream->blockStartOffset[openContext.blockCount] = openContext.totalDecodedSize;
	openContext.stream->totalSize = openContext.totalDecodedSize;
	openContext.stream->blockFileOffset[openContext.blockCount] = (int)openContext.dataFileOffset;
	Memory_FreeTagged("IMTEMPDATA", openContext.rawTable);
	openContext.rawTable = NULL;
	ImVimaResetState(&openContext.stream->vimaState);
	openContext.stream->cachedBlockIndex = -1;
	openContext.stream->segmentCapacity = 0;
	openContext.stream->segmentData = NULL;
	strncpy(openContext.stream->path, path, 0x50u);
	openContext.stream->path[79] = 0;
	return openContext.stream;
}

#ifndef XWA_MODERN
#pragma intrinsic(memcmp)
#endif

// FUNCTION: XWA 0x585D81
int ImMcmpClose(ImMcmpStream* stream) {
	void* file;

#ifdef XWA_MODERN
	if (!ImMcmpIsManagedStream(stream)) {
#else
	if (stream < g_imMcmpStreams || stream >= &g_imMcmpStreams[32]) {
#endif
		return g_imHostServicesPtr->closeFile(stream);
	}

	file = stream->file;
	if (stream->readBuffer) {
		Memory_FreeTagged("IMREADBUFFER", stream->readBuffer);
		stream->readBuffer = NULL;
	}
	stream->readBufferCapacity = 0;

	if (stream->readText) {
		Memory_FreeTagged("IMREADTEXT", stream->readText);
		stream->readText = NULL;
	}
	if (stream->segmentData) {
		Memory_FreeTagged("IMSEGMENTDATA", stream->segmentData);
	}
	memset(stream, 0, sizeof(*stream));
	g_imHostServicesPtr->closeFile(file);
	return 0;
}

#ifndef XWA_MODERN
#pragma function(memcmp)
#pragma function(memcpy)
#pragma function(strcmp)
#endif
// FUNCTION: XWA 0x585E64
unsigned int ImMcmpRead(ImMcmpStream* stream, char* dst, unsigned int count) {
	unsigned int decodeCopySize;
	unsigned int readCount;
	unsigned int blockOffset;
	unsigned int copySize;
	ImMcmpStream* localStream;
	int firstBlock;
	char* out;
	int lastBlock;
	unsigned int fallbackCount;
	int blockIndex;

	localStream = stream;
	out = dst;
#ifdef XWA_MODERN
	if (!ImMcmpIsManagedStream(localStream)) {
#else
	if (localStream < g_imMcmpStreams || localStream >= &g_imMcmpStreams[32]) {
#endif
		fallbackCount = g_imHostServicesPtr->readFile(stream, dst, count);
		return fallbackCount;
	}

	firstBlock = ImMcmpOffsetToBlock(localStream, (unsigned int)localStream->offset);
	lastBlock = ImMcmpOffsetToBlock(localStream, (unsigned int)(localStream->offset + count - 1));
	if (firstBlock < 0) {
		return 0;
	}
	if (lastBlock < 0) {
		lastBlock = localStream->blockCount - 1;
		count = (unsigned int)(localStream->totalSize - localStream->offset);
	}
	if (count <= 0u) {
		return 0;
	}

	for (blockIndex = firstBlock; blockIndex <= lastBlock; ++blockIndex) {
		if (!localStream->segmentData || blockIndex != localStream->cachedBlockIndex) {
			if (localStream->readBufferCapacity <
				(unsigned int)localStream->blockCompressedSize[blockIndex]) {
				if ((unsigned int)localStream->blockCompressedSize[blockIndex] < 8192u) {
					localStream->readBuffer =
						Memory_ReallocTagged("IMREADBUFFER", localStream->readBuffer, 8192u);
					localStream->readBufferCapacity = 8192u;
				} else {
					localStream->readBuffer =
						Memory_ReallocTagged("IMREADBUFFER", localStream->readBuffer,
											 (size_t)localStream->blockCompressedSize[blockIndex]);
					localStream->readBufferCapacity =
						(unsigned int)localStream->blockCompressedSize[blockIndex];
				}
			}
			g_imHostServicesPtr->seekFile(localStream->file, localStream->blockFileOffset[blockIndex], 0);
			readCount =
				g_imHostServicesPtr->readFile(localStream->file, localStream->readBuffer,
											  (unsigned int)localStream->blockCompressedSize[blockIndex]);
			(void)readCount;
			if ((unsigned int)localStream->segmentCapacity <
				(unsigned int)localStream->blockDecodedSize[blockIndex]) {
				localStream->segmentData =
					Memory_ReallocTagged("IMSEGMENTDATA", localStream->segmentData,
										 (size_t)localStream->blockDecodedSize[blockIndex]);
				localStream->segmentCapacity = localStream->blockDecodedSize[blockIndex];
			}
			if (!memcmp(localStream->blockCodec[blockIndex], "VIMA", 4u)) {
				ImVimaDecodeBlock(&localStream->vimaState, (int16_t*)localStream->segmentData,
								  (char*)localStream->readBuffer,
								  (unsigned int)localStream->blockDecodedSize[blockIndex]);
			} else {
				if ((unsigned int)localStream->blockDecodedSize[blockIndex] <
					(unsigned int)localStream->blockCompressedSize[blockIndex]) {
					decodeCopySize = (unsigned int)localStream->blockDecodedSize[blockIndex];
				} else {
					decodeCopySize = (unsigned int)localStream->blockCompressedSize[blockIndex];
				}
				memcpy(localStream->segmentData, localStream->readBuffer, (size_t)decodeCopySize);
			}
			localStream->cachedBlockIndex = blockIndex;
		}

		if (firstBlock == lastBlock) {
			blockOffset = (unsigned int)(localStream->offset - localStream->blockStartOffset[blockIndex]);
			copySize = count;
		} else if (blockIndex == firstBlock) {
			blockOffset = (unsigned int)(localStream->offset - localStream->blockStartOffset[blockIndex]);
			copySize = (unsigned int)localStream->blockDecodedSize[blockIndex] - blockOffset;
		} else {
			if (blockIndex == lastBlock) {
				blockOffset = 0;
				copySize =
					(unsigned int)(localStream->offset + count - localStream->blockStartOffset[blockIndex]);
			} else {
				blockOffset = 0;
				copySize = (unsigned int)localStream->blockDecodedSize[blockIndex];
			}
		}
		memcpy(out, (char*)localStream->segmentData + blockOffset, (size_t)copySize);
		out += copySize;
	}

	localStream->offset += (int)count;
	return count;
}
#ifndef XWA_MODERN
#pragma intrinsic(memcmp)
#pragma intrinsic(memcpy)
#endif

// FUNCTION: XWA 0x586380
int ImInitializeScript(ImHostLoadResourceFn loadFn, ImHostFreeResourceFn freeFn,
					   ImHostOpenResourceFn openStreamFn, ImHostCloseResourceFn closeStreamFn, int arg6,
					   int arg7, int enabled, int unused) {
	(void)unused;
	return ImScriptCommand(0, &g_imScriptHost, loadFn, freeFn, openStreamFn, closeStreamFn, arg6, arg7,
						   enabled);
}

// FUNCTION: XWA 0x5863BF
int ImSaveScript(int* buf, int bufSize) { return ImScriptCommand(2, buf, bufSize); }

// FUNCTION: XWA 0x5863D6
int ImRestoreScript(int* buf) { return ImScriptCommand(3, buf); }

// FUNCTION: XWA 0x5863E9
int ImRefreshScript(void) { return ImScriptCommand(4); }

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x586450
int ImInitSubsystems(void) {
	if (g_imInitialized) {
		ImLog("ERROR:system already initialized...");
		return -1;
	}

	g_imTriggerClockAccum = 0;
	g_imVolDuckClockAccum = 0;
	if (!ImGroupsInit() && !ImFadesInit() && !ImTriggersInit() && !ImTracksInit()) {
		IM_ATOMIC_STORE(g_imBusyCount, 0);
		g_imHeartbeatGuard = 0;
		g_imPauseCount = 0;
		g_imInitialized = 1;
		ImLog("Initialization complete...");
		return 49;
	} else {
		return -1;
	}
}

// FUNCTION: XWA 0x58636F
void ImLog(const char* fmt, ...) { (void)fmt; }

// FUNCTION: XWA 0x586509
int ImPrintf(char* format, ...) {
	va_list args;

	va_start(args, format);
	vsprintf(g_imLogBuf, format, args);
	va_end(args);

	ImLog(g_imLogBuf);
	return 0;
}

// FUNCTION: XWA 0x5863F8
int ImSetState(int state) { return ImScriptCommand(5, state); }

// FUNCTION: XWA 0x58640B
int ImSetSequence(int seqId) { return ImScriptCommand(6, seqId); }

// FUNCTION: XWA 0x58641E
int ImSetCuePoint(int cuePoint) { return ImScriptCommand(7, cuePoint); }

// FUNCTION: XWA 0x586431
int ImSetAttribute(int index, int value) { return ImScriptCommand(8, index, value); }

// FUNCTION: XWA 0x585799
int ImPause(void) {
	ImPauseRefInc();
	g_imSavedMasterVolume = ImSetVolume(0, 0xffffffffu);
	ImSetVolume(0, 0);
	ImSetAttribute(-1, 0);
	return 0;
}

// FUNCTION: XWA 0x5857CE
int ImResume(void) {
	ImSetVolume(0, (unsigned int)g_imSavedMasterVolume);
	ImResumeRefDec();
	ImSetAttribute(-1, 1);
	return 0;
}

// FUNCTION: XWA 0x5866F3
int ImSetVolume(unsigned int group, unsigned int value) { return ImSetGroupVol(group, (int)value); }

#ifndef XWA_MODERN
#pragma function(memcmp)
#endif
// FUNCTION: XWA 0x586708
int ImStartSound(unsigned int soundId, int priority) {
	int result;
	void* soundAddr;

	result = -1;
	soundAddr = ImMapSoundAddr(soundId);
	if (!soundAddr) {
		ImLog("ERR: null sound addr in StartSound()...");
		return -1;
	}
	if (!memcmp(soundAddr, "iMUS", 4u)) {
		result = ImStartSoundCore((int)soundId, priority, 0);
	}
	return result;
}
#ifndef XWA_MODERN
#pragma intrinsic(memcmp)
#endif

// FUNCTION: XWA 0x586A95
int ImStartStream(int soundId, int priority, int param) {
	int started;

	if (ImIsValidSoundId((unsigned int)soundId)) {
		ImIncBusyCount();
		started = ImStartSoundCore(soundId, priority, param);
		ImDecBusyCount();
		return started;
	}
	return -1;
}

// FUNCTION: XWA 0x586AD6
int ImSwitchStream(int curSoundId, int newSoundId, int fadeMs, unsigned int switchFlagsLow,
				   unsigned int switchFlagsHigh) {
	uint64_t switchFlags;

	switchFlags = ((uint64_t)switchFlagsHigh << 32) | switchFlagsLow;
	return ImDpSwitchStream(curSoundId, newSoundId, fadeMs, switchFlags);
}

// FUNCTION: XWA 0x586B3F
int ImIncBusyCount(void) {
	IM_ATOMIC_INC(g_imBusyCount);
	return IM_ATOMIC_LOAD(g_imBusyCount);
}

// FUNCTION: XWA 0x586B51
void ImDecBusyCount(void) {
	if (IM_ATOMIC_LOAD(g_imBusyCount)) {
		IM_ATOMIC_DEC(g_imBusyCount);
	}
}

// FUNCTION: XWA 0x586B6C
int ImCommandDispatch(int command, ...) {
#ifdef XWA_MODERN
	va_list args;
	int result;
#else
	int* args;
#endif

	if (!g_imInitialized) {
		ImLog("ERROR: iMUSE system not initialized...");
		return -1;
	}

#ifndef XWA_MODERN
	args = &command;
	args += 10;

	switch (command) {
		case 0:
			return ImInitSubsystems();
		case 1:
			return ImShutdownSubsystems();
		case 2:
			return command;
		case 3:
			return ImPauseRefInc();
		case 4:
			return ImResumeRefDec();
		case 5:
			args -= 8;
			return ImSaveState((char*)*args--, *args--);
		case 6:
			args -= 9;
			return ImRestoreState((char*)*args--);
		case 7:
			args -= 8;
			return ImSetVolume((unsigned int)*args--, (unsigned int)*args--);
		case 8:
			args -= 8;
			return ImStartSound((unsigned int)*args--, *args--);
		case 9:
			args -= 9;
			return ImStopSound(*args--);
		case 10:
			return ImStopAllSounds();
		case 11:
			args -= 9;
			return ImGetNextSound(*args--);
		case 12:
			args -= 7;
			return ImSetParam(*args--, (ImSoundParam)*args--, *args--);
		case 13:
			args -= 8;
			return ImGetParam(*args--, (ImSoundParam)*args--);
		case 14:
			args -= 6;
			return ImFadeParam(*args--, (ImSoundParam)*args--, *args--, *args--);
		case 15:
			args -= 8;
			return ImSetHook(*args--, *args--);
		case 16:
			args -= 9;
			return ImGetHook(*args--);
		case 17:
			args += 4;
			return ImSetTrigger(*args--, (char*)*args--, *args--, *args--, *args--, *args--, *args--, *args--,
								*args--, *args--, *args--, *args--, *args--);
		case 18:
			args -= 7;
			return ImCheckTrigger(*args--, (char*)*args--, *args--);
		case 19:
			args -= 7;
			return ImClearTrigger(*args--, (char*)*args--, *args--);
		case 20:
			args += 4;
			return ImDeferCommand(*args--, *args--, *args--, *args--, *args--, *args--, *args--, *args--,
								  *args--, *args--, *args--, *args--);
		case 25:
			args -= 7;
			return ImStartStream(*args--, *args--, *args--);
		case 26:
			args -= 5;
			return ImSwitchStream(*args--, *args--, *args--, *args--, *args--);
		case 27:
			return ImProcessStreams();
		case 28:
			args -= 5;
			return ImQueryStream(*args--, (int*)*args--, (int*)*args--, (int*)*args--, (int*)*args--);
		case 29:
			args -= 6;
			return ImFeedSound(*args--, (void*)*args--, *args--, *args--);
		default:
			ImLog("ERROR:bogus opcode...");
			return -1;
	}
#else
	va_start(args, command);
	switch (command) {
		case 0:
			result = ImInitSubsystems();
			break;
		case 1:
			result = ImShutdownSubsystems();
			break;
		case 2:
			result = command;
			break;
		case 3:
			result = ImPauseRefInc();
			break;
		case 4:
			result = ImResumeRefDec();
			break;
		case 5: {
			char* buffer;
			int bufSize;
			buffer = va_arg(args, char*);
			bufSize = va_arg(args, int);
			result = ImSaveState(buffer, bufSize);
			break;
		}
		case 6:
			result = ImRestoreState(va_arg(args, char*));
			break;
		case 7: {
			unsigned int group;
			unsigned int value;
			group = (unsigned int)va_arg(args, int);
			value = (unsigned int)va_arg(args, int);
			result = ImSetVolume(group, value);
			break;
		}
		case 8: {
			unsigned int soundId;
			int priority;
			soundId = (unsigned int)va_arg(args, int);
			priority = va_arg(args, int);
			result = ImStartSound(soundId, priority);
			break;
		}
		case 9:
			result = ImStopSound(va_arg(args, int));
			break;
		case 10:
			result = ImStopAllSounds();
			break;
		case 11:
			result = ImGetNextSound(va_arg(args, int));
			break;
		case 12: {
			int soundId;
			ImSoundParam param;
			int value;
			soundId = va_arg(args, int);
			param = (ImSoundParam)va_arg(args, int);
			value = va_arg(args, int);
			result = ImSetParam(soundId, param, value);
			break;
		}
		case 13: {
			int soundId;
			ImSoundParam param;
			soundId = va_arg(args, int);
			param = (ImSoundParam)va_arg(args, int);
			result = ImGetParam(soundId, param);
			break;
		}
		case 14: {
			int soundId;
			ImSoundParam param;
			int targetValue;
			int timeMs;
			soundId = va_arg(args, int);
			param = (ImSoundParam)va_arg(args, int);
			targetValue = va_arg(args, int);
			timeMs = va_arg(args, int);
			result = ImFadeParam(soundId, param, targetValue, timeMs);
			break;
		}
		case 15: {
			int soundId;
			int hookId;
			soundId = va_arg(args, int);
			hookId = va_arg(args, int);
			result = ImSetHook(soundId, hookId);
			break;
		}
		case 16:
			result = ImGetHook(va_arg(args, int));
			break;
		case 17: {
			int soundId;
			char* marker;
			int triggerCommand;
			int p1;
			int p2;
			int p3;
			int p4;
			int p5;
			int p6;
			int p7;
			int p8;
			int p9;
			int p10;
			(void)va_arg(args, int);
			soundId = va_arg(args, int);
			marker = va_arg(args, char*);
			triggerCommand = va_arg(args, int);
			p1 = va_arg(args, int);
			p2 = va_arg(args, int);
			p3 = va_arg(args, int);
			p4 = va_arg(args, int);
			p5 = va_arg(args, int);
			p6 = va_arg(args, int);
			p7 = va_arg(args, int);
			p8 = va_arg(args, int);
			p9 = va_arg(args, int);
			p10 = va_arg(args, int);
			result = ImSetTrigger(soundId, marker, triggerCommand, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
			break;
		}
		case 18: {
			int soundId;
			char* marker;
			int triggerCommand;
			soundId = va_arg(args, int);
			marker = va_arg(args, char*);
			triggerCommand = va_arg(args, int);
			result = ImCheckTrigger(soundId, marker, triggerCommand);
			break;
		}
		case 19: {
			int soundId;
			char* marker;
			int triggerCommand;
			soundId = va_arg(args, int);
			marker = va_arg(args, char*);
			triggerCommand = va_arg(args, int);
			result = ImClearTrigger(soundId, marker, triggerCommand);
			break;
		}
		case 20: {
			int delayTicks;
			int deferredCommand;
			int p1;
			int p2;
			int p3;
			int p4;
			int p5;
			int p6;
			int p7;
			int p8;
			int p9;
			int p10;
			(void)va_arg(args, int);
			(void)va_arg(args, int);
			delayTicks = va_arg(args, int);
			deferredCommand = va_arg(args, int);
			p1 = va_arg(args, int);
			p2 = va_arg(args, int);
			p3 = va_arg(args, int);
			p4 = va_arg(args, int);
			p5 = va_arg(args, int);
			p6 = va_arg(args, int);
			p7 = va_arg(args, int);
			p8 = va_arg(args, int);
			p9 = va_arg(args, int);
			p10 = va_arg(args, int);
			result = ImDeferCommand(delayTicks, deferredCommand, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
			break;
		}
		case 25: {
			int soundId;
			int priority;
			int param;
			soundId = va_arg(args, int);
			priority = va_arg(args, int);
			param = va_arg(args, int);
			result = ImStartStream(soundId, priority, param);
			break;
		}
		case 26: {
			int curSoundId;
			int newSoundId;
			int fadeMs;
			unsigned int switchFlagsLow;
			unsigned int switchFlagsHigh;
			curSoundId = va_arg(args, int);
			newSoundId = va_arg(args, int);
			fadeMs = va_arg(args, int);
			switchFlagsLow = (unsigned int)va_arg(args, int);
			switchFlagsHigh = (unsigned int)va_arg(args, int);
			result = ImSwitchStream(curSoundId, newSoundId, fadeMs, switchFlagsLow, switchFlagsHigh);
			break;
		}
		case 27:
			result = ImProcessStreams();
			break;
		case 28: {
			int soundId;
			int* outBufSize;
			int* outRefillThreshold;
			int* outFill;
			int* outEof;
			soundId = va_arg(args, int);
			outBufSize = va_arg(args, int*);
			outRefillThreshold = va_arg(args, int*);
			outFill = va_arg(args, int*);
			outEof = va_arg(args, int*);
			result = ImQueryStream(soundId, outBufSize, outRefillThreshold, outFill, outEof);
			break;
		}
		case 29: {
			int soundId;
			void* src;
			int len;
			int feedFlag;
			soundId = va_arg(args, int);
			src = va_arg(args, void*);
			len = va_arg(args, int);
			feedFlag = va_arg(args, int);
			result = ImFeedSound(soundId, src, len, feedFlag);
			break;
		}
		default:
			ImLog("ERROR:bogus opcode...");
			result = -1;
			break;
	}
	va_end(args);
	return result;
#endif
}

// FUNCTION: XWA 0x58BD00
int ImTracksInit(void) {
	ImTrack* track;

	track = g_imTrackPool;
	ImLog("TRACKS module...");
	g_imMaxTracks[0] = g_imTrackPoolSize;
	g_imEnginePaused = 0;
	g_imActivePlayers = NULL;
	if (ImWaveInit()) {
		return -1;
	}
	if (ImMixerInit()) {
		return -1;
	}
	if (ImDispatchInit()) {
		return -1;
	}
	if (ImStreamerInit()) {
		return -1;
	}

	{
		int trackIdx;

		for (trackIdx = 0; trackIdx < g_imMaxTracks[0]; ++trackIdx) {
			track->prev = NULL;
			track->next = NULL;
			track->dispatch = ImGetDispatch(trackIdx);
			track->dispatch->track = track;
			track->soundId = 0;
			++track;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x58BDDE
int ImTeardownSubsystems(void) {
	ImStopAllSoundsCore();
	if (ImWaveTerminate()) {
		return -1;
	}
	if (ImDispatchTerminate()) {
		return -1;
	}
	if (ImMixerTerminate()) {
		return -1;
	}
	return 0;
}

// FUNCTION: XWA 0x5864E7
int ImShutdownSubsystems(void) {
	if (!g_imInitialized) {
		return -1;
	}
	g_imInitialized = 0;
	return ImTeardownSubsystems();
}

// FUNCTION: XWA 0x5867AE
int ImSetParam(int soundId, ImSoundParam param, int value) { return ImTrSetParam(soundId, param, value); }

// FUNCTION: XWA 0x5867C7
int ImGetParam(int soundId, ImSoundParam param) {
	int result;

	if (!param) {
		return 2;
	}
	if (param == P_STATUS) {
		return ImCountPendingStarts(soundId);
	}

	ImIncBusyCount();
	result = ImTrGetParam(soundId, param);
	ImDecBusyCount();
	if (result < 0 && param == P_COUNT) {
		return 0;
	}
	return result;
}

// FUNCTION: XWA 0x586826
int ImFadeParam(int soundId, ImSoundParam param, int targetValue, int timeMs) {
	return ImFadeParamCore(soundId, param, targetValue, timeMs);
}

// FUNCTION: XWA 0x585168
int ImGetStreamingSoundPos(void) {
	int soundId;

	{
		int prevSoundId;

		prevSoundId = 0;
		soundId = 0;
		prevSoundId = ImGetNextSound(prevSoundId);
		while (prevSoundId) {
			if (ImGetParam(prevSoundId, P_IS_STREAMING) && ImGetParam(prevSoundId, P_STREAM_SIZE) == 2) {
				soundId = prevSoundId;
				break;
			}
			prevSoundId = ImGetNextSound(prevSoundId);
		}
	}
	return ImGetParam(soundId, P_STREAM_POS);
}

// FUNCTION: XWA 0x58BE14
void ImPauseEngine(void) {
	ImIncBusyCount();
	g_imEnginePaused = 1;
	ImDecBusyCount();
}

// FUNCTION: XWA 0x58BE2D
void ImResumeEngine(void) {
	ImIncBusyCount();
	g_imEnginePaused = 0;
	ImDecBusyCount();
}

// FUNCTION: XWA 0x586542
int ImPauseRefInc(void) {
	if (!g_imPauseCount++) {
		ImPauseEngine();
	}
	return g_imPauseCount;
}

// FUNCTION: XWA 0x586569
int ImResumeRefDec(void) {
	--g_imPauseCount;
	if (!g_imPauseCount) {
		ImResumeEngine();
	} else if (g_imPauseCount < 0) {
		g_imPauseCount = 0;
	}
	return g_imPauseCount;
}

// FUNCTION: XWA 0x5865A3
int ImSaveState(char* buffer, int bufSize) {
	int savedSize;
	int chunkSize;

	savedSize = 0;
	if (bufSize < 80000) {
		ImLog("ERR: save buffer too small...");
		return -1;
	}

	*(int*)(buffer + savedSize) = 49;
	savedSize += 4;

	chunkSize = ImSaveFades(buffer + savedSize, (unsigned int)(bufSize - savedSize));
	if (chunkSize < 0) {
		return chunkSize;
	}
	savedSize += chunkSize;

	chunkSize = ImSaveTriggers(buffer + savedSize, (unsigned int)(bufSize - savedSize));
	if (chunkSize < 0) {
		return chunkSize;
	}
	savedSize += chunkSize;

	chunkSize = ImSaveTracks(buffer + savedSize, (unsigned int)(bufSize - savedSize));
	if (chunkSize < 0) {
		return chunkSize;
	}
	savedSize += chunkSize;

	return savedSize;
}

// FUNCTION: XWA 0x586671
int ImRestoreState(char* buffer) {
	int restoredSize;

	restoredSize = 0;
	ImStopAllSounds();
	if (*(int*)(buffer + restoredSize) != 49) {
		ImLog("ERR: restore buffer contains bad data...");
		return -1;
	}

	restoredSize += 4;
	restoredSize += ImRestoreFades(buffer + restoredSize);
	restoredSize += ImRestoreTriggers(buffer + restoredSize);
	restoredSize += ImRestoreTracks(buffer + restoredSize);
	return restoredSize;
}

// FUNCTION: XWA 0x58D762
int ImSaveFades(void* buf, unsigned int bufSize) {
	if (bufSize < sizeof(g_imFades)) {
		return -5;
	}
	memcpy(buf, g_imFades, sizeof(g_imFades));
	return (int)sizeof(g_imFades);
}

// FUNCTION: XWA 0x58D792
int ImRestoreFades(void* buf) {
	memcpy_0(g_imFades, buf, sizeof(g_imFades));
	g_imFadesActive = 1;
	return (int)sizeof(g_imFades);
}

// FUNCTION: XWA 0x58D7BC
int ImFadeParamCore(int soundId, ImSoundParam param, int targetValue, int timeMs) {
	int delta;
	int i;
	ImFade* fade;

	fade = g_imFades;
	if (!soundId || timeMs < 0) {
		return -5;
	}
	if (param != P_PRIORITY && param != P_VOLUME && param != P_PAN && param != P_DETUNE) {
		return -5;
	}

	timeMs = 5 * timeMs / 6;
	ImCancelFade(soundId, param);
	if (!timeMs) {
		if (param == P_VOLUME && !targetValue) {
			ImStopSound(soundId);
		} else {
			ImSetParam(soundId, param, targetValue);
		}
		return 0;
	}

	for (i = 0;; ++i) {
		if (i >= 16) {
			ImLog("ERROR: fd unable to alloc fade...");
			return -6;
		}
		if (!fade->active) {
			break;
		}
		++fade;
	}

	fade->soundId = soundId;
	fade->param = param;
	fade->currentValue = ImGetParam(soundId, param);
	fade->totalTicks = timeMs;
	fade->ticksRemaining = timeMs;
	delta = targetValue - fade->currentValue;
	fade->stepValue = delta / timeMs;
	fade->errorSign = delta >= 0 ? 1 : -1;
	if (delta < 0) {
		delta = -delta;
	}
	fade->errorStep = delta % timeMs;
	fade->errorAccum = 0;
	fade->active = 1;
	g_imFadesActive = 1;
	return 0;
}

// FUNCTION: XWA 0x58D963
int ImCancelFade(int soundId, int param) {
	int i;
	ImFade* fade;

	fade = g_imFades;
	for (i = 0; i < 16; ++i) {
		if (fade->active && fade->soundId == soundId && (fade->param == param || param == -1)) {
			fade->active = 0;
		}
		++fade;
	}
	return i;
}

// FUNCTION: XWA 0x58D9C4
void ImProcessFades(void) {
	int i;
	ImFade* fade;
	int value;

	fade = g_imFades;
	if (g_imFadesActive) {
		g_imFadesActive = 0;
		for (i = 0; i < 16; ++i) {
			if (fade->active) {
				g_imFadesActive = 1;
				--fade->ticksRemaining;
				if (!fade->ticksRemaining) {
					fade->active = 0;
				}

				value = fade->currentValue + fade->stepValue;
				fade->errorAccum += fade->errorStep;
				if ((unsigned int)fade->errorAccum >= (unsigned int)fade->totalTicks) {
					fade->errorAccum -= fade->totalTicks;
					value += fade->errorSign;
				}

				if (value != fade->currentValue) {
					fade->currentValue = value;
					if (!(fade->ticksRemaining % 5)) {
						if (fade->param != P_VOLUME || value) {
							ImSetParam(fade->soundId, (ImSoundParam)fade->param, value);
						} else {
							ImStopSound(fade->soundId);
						}
					}
				}
			}
			++fade;
		}
	}
}

#ifndef XWA_MODERN
#pragma function(memset)
#pragma function(strlen)
#pragma function(strcpy)
#endif

// FUNCTION: XWA 0x58C9F0
int ImTriggersInit(void) {
	memset(g_imMarkerTriggers, 0, sizeof(g_imMarkerTriggers));
	memset(g_imTriggers, 0, sizeof(g_imTriggers));
	g_imTriggersActive = 0;
	g_imMarkerDepth = 0;
	return 0;
}

// FUNCTION: XWA 0x58CA90
int ImRestoreTriggers(void* buf) {
	struct {
		int markerTriggers;
		int triggers;
	} sizes;

	sizes.markerTriggers = (int)sizeof(g_imMarkerTriggers);
	sizes.triggers = (int)sizeof(g_imTriggers);
	memcpy_0(g_imMarkerTriggers, buf, (size_t)sizes.markerTriggers);
	memcpy_0(g_imTriggers, (char*)buf + sizes.markerTriggers, (size_t)sizes.triggers);
	g_imTriggersActive = 1;
	return sizes.markerTriggers + sizes.triggers;
}

// FUNCTION: XWA 0x58CA33
int ImSaveTriggers(void* buf, unsigned int bufSize) {
	unsigned int size;

	size = (unsigned int)(sizeof(g_imMarkerTriggers) + sizeof(g_imTriggers));
	if (bufSize < size) {
		return -5;
	}

	memcpy(buf, g_imMarkerTriggers, sizeof(g_imMarkerTriggers));
	memcpy((char*)buf + sizeof(g_imMarkerTriggers), g_imTriggers, sizeof(g_imTriggers));
	return (int)size;
}

// FUNCTION: XWA 0x58CEEE
int ImDeferCommandCore(int delayTicks, int command, ...) {
	struct {
		ImTrigger* trigger;
#ifdef XWA_MODERN
		va_list args;
#else
		int* args;
#endif
		int j;
		int i;
		int* params;
	} state;

	state.trigger = g_imTriggers;
	if (!delayTicks) {
		return -5;
	}

	for (state.i = 0; state.i < 8; ++state.i, ++state.trigger) {
		if (!state.trigger->timer) {
			state.trigger->command = command;
#ifdef XWA_MODERN
			va_start(state.args, command);
			state.params = state.trigger->params;
			for (state.j = 0; state.j < 10; ++state.j) {
				*state.params++ = va_arg(state.args, int);
			}
			va_end(state.args);
#else
			state.args = &command;
			state.params = state.trigger->params;
			for (state.j = 0; state.j < 10; ++state.j) {
				*state.params++ = *++state.args;
			}
#endif
			state.trigger->timer = delayTicks;
			g_imTriggersActive = 1;
			return 0;
		}
	}

	ImLog("ERR: tr unable to alloc deferred cmd...");
	return -6;
}

// FUNCTION: XWA 0x58699A
int ImDeferCommand(int delayTicks, int command, ...) {
	int i;
	int lastParam;
	va_list args;

	lastParam = 0;
	va_start(args, command);
	for (i = 0; i < 10; ++i) {
		lastParam = va_arg(args, int);
	}
	va_end(args);

	/* Retail code reads the 10th vararg and passes that value in all ten parameter slots. */
	return ImDeferCommandCore(delayTicks, command, lastParam, lastParam, lastParam, lastParam, lastParam,
							  lastParam, lastParam, lastParam, lastParam, lastParam);
}

// FUNCTION: XWA 0x58D02F
int ImCountPendingStarts(int soundId) {
	struct {
		ImTrigger* trigger;
		int i;
		ImMarkerTrigger* markerTrigger;
		int count;
	} pending;

	pending.count = 0;
	pending.markerTrigger = g_imMarkerTriggers;
	pending.trigger = g_imTriggers;

	for (pending.i = 0; pending.i < 8; ++pending.i, ++pending.markerTrigger) {
		if (pending.markerTrigger->soundId) {
			if (pending.markerTrigger->command == 8 && pending.markerTrigger->params[0] == soundId) {
				++pending.count;
			} else if (pending.markerTrigger->command == 26 && pending.markerTrigger->params[1] == soundId) {
				++pending.count;
			}
		}
	}

	for (pending.i = 0; pending.i < 8; ++pending.i, ++pending.trigger) {
		if (pending.trigger->timer) {
			if (pending.trigger->command == 8 && pending.trigger->params[0] == soundId) {
				++pending.count;
			} else if (pending.trigger->command == 26 && pending.trigger->params[1] == soundId) {
				++pending.count;
			}
		}
	}

	return pending.count;
}

// FUNCTION: XWA 0x58CD3C
void ImTgProcessMarker(int soundId, char* markerText) {
	char savedMarker[256];
	int i;
	int triggerIndex;
	ImMarkerTrigger* trigger;

	if (strlen(markerText) >= 256) {
		ImLog("ERR: TgProcessMarker() passed oversize marker string...");
		return;
	}

	strcpy(g_imMarkerScratch, markerText);
	++g_imMarkerDepth;

	for (triggerIndex = 0, trigger = g_imMarkerTriggers; triggerIndex < 8; ++triggerIndex, ++trigger) {
		if (trigger->soundId && soundId == trigger->soundId &&
			(!trigger->marker[0] || !strcmp(g_imMarkerScratch, trigger->marker))) {
			for (i = 0; g_imMarkerScratch[i]; ++i) {
				savedMarker[i] = g_imMarkerScratch[i];
			}
			savedMarker[i] = 0;

			ImFireMarkerTrigger(trigger, g_imMarkerScratch);

			for (i = 0; savedMarker[i]; ++i) {
				g_imMarkerScratch[i] = savedMarker[i];
			}
			g_imMarkerScratch[i] = 0;
		}
	}

	if (!--g_imMarkerDepth) {
		for (triggerIndex = 0, trigger = g_imMarkerTriggers; triggerIndex < 8; ++triggerIndex, ++trigger) {
			if (trigger->onceFlag) {
				trigger->soundId = 0;
			}
		}
	}
}

// FUNCTION: XWA 0x58CFB3
void ImProcessTriggers(void) {
	struct {
		ImTrigger* trigger;
		int i;
	} state;

	state.trigger = g_imTriggers;
	if (g_imTriggersActive == 0) {
		return;
	}

	g_imTriggersActive = 0;
	for (state.i = 0; state.i < 8; ++state.i, ++state.trigger) {
		if (state.trigger->timer) {
			g_imTriggersActive = 1;
			--state.trigger->timer;
			if (!state.trigger->timer) {
				ImFireTimedTrigger(state.trigger);
			}
		}
	}
}

typedef void (*ImRawMarkerCommand)(char* marker, intptr_t p1, intptr_t p2, intptr_t p3, intptr_t p4,
								   intptr_t p5, intptr_t p6, intptr_t p7, intptr_t p8, intptr_t p9,
								   intptr_t p10);
typedef void (*ImRawTimedCommand)(intptr_t p1, intptr_t p2, intptr_t p3, intptr_t p4, intptr_t p5,
								  intptr_t p6, intptr_t p7, intptr_t p8, intptr_t p9, intptr_t p10);

// FUNCTION: XWA 0x58D12E
void ImFireMarkerTrigger(ImMarkerTrigger* trigger, char* marker) {
	intptr_t param;
	uintptr_t callback;

	param = (intptr_t)trigger->params[0];
	trigger->soundId = 0;
	if (trigger->command) {
		if (trigger->command >= 30) {
			((ImRawMarkerCommand)(uintptr_t)(uint32_t)trigger->command)(
				marker, param, param, param, param, param, param, param, param, param, param);
		} else {
			ImCommandDispatch(trigger->command, (int)param, (int)param, (int)param, (int)param, (int)param,
							  (int)param, (int)param, (int)param, (int)param, (int)param, 0, 0, 0, 0);
		}
	} else if (g_imScriptHost.onSoundEnd) {
		callback = (uintptr_t)g_imScriptHost.onSoundEnd;
		((ImRawMarkerCommand)callback)(marker, param, param, param, param, param, param, param, param, param,
									   param);
	} else {
		ImLog("ERR:null callback in InitData struct...");
	}
}

// FUNCTION: XWA 0x58D437
void ImFireTimedTrigger(ImTrigger* trigger) {
	intptr_t param;
	uintptr_t callback;

	param = (intptr_t)trigger->params[0];
	if (trigger->command) {
		if (trigger->command >= 30) {
			((ImRawTimedCommand)(uintptr_t)(uint32_t)trigger->command)(param, param, param, param, param,
																	   param, param, param, param, param);
		} else {
			ImCommandDispatch(trigger->command, (int)param, (int)param, (int)param, (int)param, (int)param,
							  (int)param, (int)param, (int)param, (int)param, (int)param, 0, 0, 0, 0);
		}
	} else if (g_imScriptHost.onSoundEnd) {
		callback = (uintptr_t)g_imScriptHost.onSoundEnd;
		((ImRawMarkerCommand)callback)((char*)param, param, param, param, param, param, param, param, param,
									   param, param);
	} else {
		ImLog("ERR:null callback in InitData struct...");
	}
}

// FUNCTION: XWA 0x58CAE5
int ImSetTriggerCore(int soundId, char* marker, int command, ...) {
	int i;
	int j;
	va_list args;
	ImMarkerTrigger* trigger;

	if (!soundId) {
		return -5;
	}
	if (!marker) {
		marker = g_imEmptyMarker;
	}
	if (strlen(marker) >= 256) {
		ImLog("ERR: attempting to set trig with oversize marker string...");
		return -5;
	}

	trigger = g_imMarkerTriggers;
	for (i = 0; i < 8; ++i) {
		if (!trigger->soundId) {
			trigger->soundId = soundId;
			trigger->onceFlag = 0;
			trigger->command = command;
			strcpy(trigger->marker, marker);
			va_start(args, command);
			for (j = 0; j < 10; ++j) {
				trigger->params[j] = va_arg(args, int);
			}
			va_end(args);
			return 0;
		}
		++trigger;
	}

	ImLog("ERR: tr unable to alloc trigger...");
	return -6;
}

// FUNCTION: XWA 0x586869
int ImSetTrigger(int soundId, char* marker, int command, ...) {
	int i;
	int lastParam;
	va_list args;

	lastParam = 0;
	va_start(args, command);
	for (i = 0; i < 10; ++i) {
		lastParam = va_arg(args, int);
	}
	va_end(args);

	/* Retail code reads the 10th vararg and passes that value in all ten parameter slots. */
	return ImSetTriggerCore(soundId, marker, command, lastParam, lastParam, lastParam, lastParam, lastParam,
							lastParam, lastParam, lastParam, lastParam, lastParam);
}

// FUNCTION: XWA 0x58CC03
int ImCheckTriggerCore(int soundId, char* marker, int command) {
	int count;

	count = 0;
	{
		ImMarkerTrigger* trigger;
		int i;

		trigger = g_imMarkerTriggers;
		for (i = 0; i < 8; ++i, ++trigger) {
			if (trigger->soundId && (soundId == -1 || soundId == trigger->soundId) &&
				(marker == (char*)-1 || !strcmp(marker, trigger->marker)) &&
				(command == -1 || command == trigger->command)) {
				++count;
			}
		}
	}
	return count;
}

// FUNCTION: XWA 0x586968
int ImCheckTrigger(int soundId, char* marker, int command) {
	return ImCheckTriggerCore(soundId, marker, command);
}

// FUNCTION: XWA 0x58CC96
int ImClearTriggerCore(int soundId, char* marker, int command) {
	int i;
	ImMarkerTrigger* trigger;

	trigger = g_imMarkerTriggers;
	for (i = 0; i < 8; ++i, ++trigger) {
		if (trigger->soundId && (soundId == -1 || soundId == trigger->soundId) &&
			(marker == (char*)-1 || !strcmp(marker, trigger->marker)) &&
			(command == -1 || command == trigger->command)) {
			if (!g_imMarkerDepth) {
				trigger->soundId = 0;
			} else {
				trigger->onceFlag = 1;
			}
		}
	}
	return 0;
}

// FUNCTION: XWA 0x586981
int ImClearTrigger(int soundId, char* marker, int command) {
	return ImClearTriggerCore(soundId, marker, command);
}

// FUNCTION: XWA 0x58D730
int ImFadesInit(void) {
	ImLog("FADES module...");
	memset(g_imFades, 0, sizeof(g_imFades));
	g_imFadesActive = 0;
	return 0;
}

// FUNCTION: XWA 0x58DEA0
int ImDispatchInit(void) {
	g_imDispatchBuffer = Memory_AllocTagged(
		"IMDISPBUFFER", (size_t)(g_imSmallBufSize * g_imSmallBufCount + g_imLargeBufSize * g_imLargeBufCount +
								 4 * (g_imLargeBufCount + g_imSmallBufCount)));
	if (!g_imDispatchBuffer) {
		ImLog("ERR:in dispatch allocating buffers...");
		return -1;
	}

	g_imLargeBufBase = (char*)g_imDispatchBuffer;
	g_imSmallBufBase = (char*)g_imDispatchBuffer + g_imLargeBufSize * g_imLargeBufCount;
	g_imLargeBufFlags = (int*)(g_imLargeBufBase + g_imLargeBufSize * g_imLargeBufCount +
							   g_imSmallBufSize * g_imSmallBufCount);
	g_imSmallBufFlags = (int*)(g_imLargeBufBase + g_imLargeBufSize * g_imLargeBufCount +
							   g_imSmallBufSize * g_imSmallBufCount + 4 * g_imLargeBufCount);

	memset(g_imLargeBufFlags, 0, sizeof(int));
	memset(g_imSmallBufFlags, 0, sizeof(int));
	memset(g_imStreamZones, 0, sizeof(g_imStreamZones));
	return 0;
}

// FUNCTION: XWA 0x59029F
ImStreamZone* ImAllocStreamZone(void) {
	int i;
	ImStreamZone* zone;

	zone = g_imStreamZones;
	for (i = 0; i < 50; ++i) {
		if (!zone->inUse) {
			zone->prev = NULL;
			zone->next = NULL;
			zone->inUse = 1;
			zone->streamOffset = 0;
			zone->byteCount = 0;
			zone->isPrefetch = 0;
			return zone;
		}
		++zone;
	}

	ImLog("ERR: out of streamZones...");
	return NULL;
}

// FUNCTION: XWA 0x58DFB9
int ImDispatchTerminate(void) {
	Memory_FreeTagged("IMDISPBUFFER", g_imDispatchBuffer);
	return 0;
}

// FUNCTION: XWA 0x58DFE6
int ImSaveDispatch(void* buf, unsigned int bufSize) {
	if (bufSize < sizeof(g_imDispatchPool)) {
		return -5;
	}
	memcpy(buf, g_imDispatchPool, sizeof(g_imDispatchPool));
	return (int)sizeof(g_imDispatchPool);
}

// FUNCTION: XWA 0x58E016
int ImRestoreDispatch(void* buf) {
	struct {
		ImStreamZone* zone;
		int index;
	} clear;

	clear.zone = g_imStreamZones;
	memcpy_0(g_imDispatchPool, buf, sizeof(g_imDispatchPool));
	for (clear.index = 0; clear.index < g_imLargeBufCount; ++clear.index) {
		g_imLargeBufFlags[clear.index] = 0;
	}
	for (clear.index = 0; clear.index < g_imSmallBufCount; ++clear.index) {
		g_imSmallBufFlags[clear.index] = 0;
	}
	for (clear.index = 0; clear.index < 50; ++clear.index, ++clear.zone) {
		clear.zone->inUse = 0;
	}
	return (int)sizeof(g_imDispatchPool);
}

// FUNCTION: XWA 0x58E0CD
int ImRestoreDispatchStreams(void) {
	int i;
	ImDispatch* dispatch;

	dispatch = g_imDispatchPool;
	for (i = 0; i < g_imMaxTracks[0]; ++i) {
		dispatch->jumpBuffer = NULL;
		if (dispatch->track->soundId && dispatch->stream) {
			dispatch->stream =
				ImStreamOpen((unsigned int)dispatch->track->soundId, dispatch->streamSize, 0x4000u);
			if (dispatch->stream) {
				ImStreamSeek(dispatch->stream, dispatch->track->soundId, dispatch->currentOffset);
				if (dispatch->regionEndOffset) {
					dispatch->pendingJumpZones = ImAllocStreamZone();
					if (dispatch->pendingJumpZones) {
						dispatch->pendingJumpZones->streamOffset = dispatch->currentOffset;
						dispatch->pendingJumpZones->byteCount = 0;
						dispatch->pendingJumpZones->isPrefetch = 0;
					} else {
						ImLog("ERR: unable to alloc zone during restore...");
					}
				}
			} else {
				ImLog("ERR: unable to start stream during restore...");
			}
		}
		++dispatch;
	}
	return 0;
}

// FUNCTION: XWA 0x58DFD3
ImDispatch* ImGetDispatch(int trackIdx) { return &g_imDispatchPool[trackIdx]; }
