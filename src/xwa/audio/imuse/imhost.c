#include "xwa/audio/imuse/imhost.h"

#include "aeron/log.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/time.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// GLOBAL: XWA 0x693860
static char g_imHostLogScratch[256];

#ifdef XWA_MODERN
static AeronVfsRoot ImHost_FileRootForMode(const char* mode) {
	if (mode != NULL && (mode[0] == 'w' || mode[0] == 'a')) {
		return AERON_VFS_ROOT_USER;
	}
	return AERON_VFS_ROOT_ASSET;
}
#endif

// FUNCTION: XWA 0x49A630
int ImHost_PrintStatus(const char* fmt, ...) {
	va_list args;
	int result;

	va_start(args, fmt);
	result = vsprintf(g_imHostLogScratch, fmt, args);
	va_end(args);
	DebugPrintfChannel(0x10, "iMUSE status: %s\n", g_imHostLogScratch);
	return result;
}

// FUNCTION: XWA 0x49A670
int ImHost_PrintMessage(const char* fmt, ...) {
	va_list args;
	int result;

	va_start(args, fmt);
	result = vsprintf(g_imHostLogScratch, fmt, args);
	va_end(args);
	DebugPrintfChannel(0x10, "iMUSE message: %s\n", g_imHostLogScratch);
	return result;
}

// FUNCTION: XWA 0x49A6B0
int ImHost_PrintWarning(const char* fmt, ...) {
	va_list args;
	int result;

	va_start(args, fmt);
	result = vsprintf(g_imHostLogScratch, fmt, args);
	va_end(args);
	DebugPrintfChannel(0x10, "iMUSE warning: %s\n", g_imHostLogScratch);
	return result;
}

// FUNCTION: XWA 0x49A6F0
int ImHost_PrintError(const char* fmt, ...) {
	va_list args;
	int result;

	va_start(args, fmt);
	result = vsprintf(g_imHostLogScratch, fmt, args);
	va_end(args);
	DebugPrintfChannel(4, "iMUSE error: %s\n", g_imHostLogScratch);
	return result;
}

// FUNCTION: XWA 0x49A730
int ImHost_PrintDebug(const char* fmt, ...) {
	va_list args;
	int result;

	va_start(args, fmt);
	result = vsprintf(g_imHostLogScratch, fmt, args);
	va_end(args);
	DebugPrintfChannel(0x10, "iMUSE debug: %s\n", g_imHostLogScratch);
	return result;
}

// FUNCTION: XWA 0x49A770
void ImHost_AssertFail(const char* expr, const char* file, int line) {
	sprintf(g_imHostLogScratch, "ASSERT! (%s:%d) %s", file, line, expr);
	DebugPrintfChannel(4, "iMUSE assert: %s\n", g_imHostLogScratch);
	exit(1);
}

// FUNCTION: XWA 0x49A7C0
int ImHost_RegisterAtExit(void (*fn)(void)) { return atexit(fn); }

// FUNCTION: XWA 0x49A7D0
void* ImHost_AllocMem(unsigned int size) {
	void* block;

	block = Memory_AllocTagged("IMUSEALLOCMEM", (size_t)size);
	DebugPrintfChannel(8, "iMUSE malloc: Size %d to Ptr %d.\n", size, block);
	return block;
}

// FUNCTION: XWA 0x49A800
void ImHost_FreeMem(void* block) {
	DebugPrintfChannel(8, "iMUSE free: Ptr %d.\n", block);
	Memory_FreeTagged("IMUSEALLOCMEM", block);
}

// FUNCTION: XWA 0x49A830
void* ImHost_ReallocMem(void* block, unsigned int size) {
	void* newBlock;

	newBlock = Memory_ReallocTagged("IMUSEALLOCMEM", block, (size_t)size);
	DebugPrintfChannel(8, "iMUSE realloc: Size %d, Ptr %d to Ptr %d.\n", size, block, newBlock);
	return newBlock;
}

// FUNCTION: XWA 0x49AA20
// Port handles are direct heap pointers (ImHost_AllocHandle), so locking is the
// identity. Typed void*(void*) rather than the original's int(int): on 32-bit
// those were ABI-identical, but on 64-bit int(int) would truncate the pointer.
void* ImHost_LockHandle(void* handle) { return handle; }

// FUNCTION: XWA 0x49A870
int ImHost_GetTime(void) { return g_gameTime; }

// FUNCTION: XWA 0x49A880
XwaFile* ImHost_OpenFile(const char* path, const char* mode) {
#ifdef XWA_MODERN
	XwaFile* stream = File_OpenAsset(ImHost_FileRootForMode(mode), path, mode);
	Aeron_LogVerbose("xwa.music", "iMUSE open '%s' (%s) -> %s", path, mode, stream ? "ok" : "FAIL");
	return stream;
#else
	return (XwaFile*)fopen(path, mode);
#endif
}

// FUNCTION: XWA 0x49A8A0
int ImHost_CloseFile(XwaFile* stream) {
#ifdef XWA_MODERN
	return File_Close(stream);
#else
	return fclose((FILE*)stream);
#endif
}

// FUNCTION: XWA 0x49A8B0
unsigned int ImHost_ReadFile(XwaFile* stream, void* dst, unsigned int size) {
	return (unsigned int)File_ReadPartial(stream, dst, (size_t)size);
}

// FUNCTION: XWA 0x49A8D0
char* ImHost_ReadLine(XwaFile* stream, char* dst, int size) {
#ifdef XWA_MODERN
	if (size <= 0 || !File_ReadLine(stream, dst, (size_t)size)) {
		return NULL;
	}
	return dst;
#else
	return fgets(dst, size, (FILE*)stream);
#endif
}

// FUNCTION: XWA 0x49A8F0
unsigned int ImHost_WriteFile(XwaFile* stream, void* src, unsigned int size) {
#ifdef XWA_MODERN
	return (unsigned int)File_WritePartial(stream, src, (size_t)size);
#else
	return fwrite(src, 1, size, (FILE*)stream);
#endif
}

// FUNCTION: XWA 0x49A920
int ImHost_TellFile(XwaFile* stream) {
#ifdef XWA_MODERN
	return File_Tell(stream);
#else
	return ftell((FILE*)stream);
#endif
}

// FUNCTION: XWA 0x49A930
int ImHost_SeekFile(XwaFile* stream, int offset, int origin) {
#ifdef XWA_MODERN
	return File_Seek(stream, offset, origin);
#else
	return fseek((FILE*)stream, offset, origin);
#endif
}

// atEof host callback. The original wired the CRT feof directly into the host
// services table (no dedicated XWA function), so there is no original to match.
// iMUSE's music path never calls atEof, so a never-EOF stub is sufficient.
int ImHost_AtEof(XwaFile* stream) {
	(void)stream;
	return 0;
}

// FUNCTION: XWA 0x49AA30 (unlockHandle host callback; the original used a nullsub).
void ImHost_UnlockHandle(void* handle) { (void)handle; }

// FUNCTION: XWA 0x49A950
int ImHost_GetFileSize(const char* path) {
#ifdef XWA_MODERN
	XwaFile* stream;
#else
	FILE* stream;
#endif
	int size;

#ifdef XWA_MODERN
	stream = File_OpenAsset(AERON_VFS_ROOT_ASSET, path, "r");
#else
	stream = fopen(path, "r");
#endif
	if (stream) {
#ifdef XWA_MODERN
		File_Seek(stream, 0, SEEK_END);
		size = File_Tell(stream);
		File_Close(stream);
#else
		fseek(stream, 0, SEEK_END);
		size = ftell(stream);
		fclose(stream);
#endif
		return size;
	}

	return 0;
}

// FUNCTION: XWA 0x49A9A0
int ImHost_FilePrintf(XwaFile* stream, const char* fmt, ...) {
	va_list args;
	va_list argsCopy;
	char stackBuf[512];
	char* heapBuf;
	int count;
	int result;

	va_start(args, fmt);
	va_copy(argsCopy, args);
	count = vsnprintf(stackBuf, sizeof(stackBuf), fmt, args);
	va_end(args);
	if (count < 0) {
		va_end(argsCopy);
		return -1;
	}

	if ((size_t)count < sizeof(stackBuf)) {
		va_end(argsCopy);
		return File_WriteCount(stream, stackBuf, (size_t)count) ? count : -1;
	}

	heapBuf = (char*)Memory_AllocTagged("IMUSEFILEPRINTF", (size_t)count + 1u);
	if (!heapBuf) {
		va_end(argsCopy);
		return -1;
	}

	result = vsnprintf(heapBuf, (size_t)count + 1u, fmt, argsCopy);
	va_end(argsCopy);
	if (result == count && File_WriteCount(stream, heapBuf, (size_t)count)) {
		Memory_FreeTagged("IMUSEFILEPRINTF", heapBuf);
		return count;
	}

	Memory_FreeTagged("IMUSEFILEPRINTF", heapBuf);
	return -1;
}

// FUNCTION: XWA 0x49A9C0
void* ImHost_AllocHandle(unsigned int size) { return Memory_AllocTagged("IMUSEALLOCHANDLE", (size_t)size); }

// FUNCTION: XWA 0x49A9E0
void ImHost_FreeHandle(void* handle) { Memory_FreeTagged("IMUSEALLOCHANDLE", handle); }

// FUNCTION: XWA 0x49AA00
void* ImHost_ReallocHandle(void* handle, unsigned int size) {
	return Memory_ReallocTagged("IMUSEALLOCHANDLE", handle, (size_t)size);
}
