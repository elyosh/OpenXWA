#define XWA_FILE_IO_IMPLEMENTATION
#include "xwa/assets/file_io.h"

#include "aeron/log.h"
#include "xwa/assets/string_table.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/render/renderer_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0x7829C8
int g_openFileCount = 0;
// GLOBAL: XWA 0xABC738
char g_installPath[256] = { 0 };
// GLOBAL: XWA 0xABC838
char g_startupParentPath[256] = { 0 };
// GLOBAL: XWA 0x80DA60
char FileName[256] = { 0 };
// GLOBAL: XWA 0xABC737
char g_cdDriveLetter = 0;
// GLOBAL: XWA 0x910780
XwaFile* g_stream = NULL;
// GLOBAL: XWA 0x7B7000
int16_t g_fileReadAbortFlag = 0;

static AeronVfs* g_fileVfs;
int g_fileFatalErrorPending;
int g_fileFatalExitCode;
AeronVfsRoot g_lastOpenedFileRoot;

static AeronVfsOpenMode File_OpenMode(const char* mode) {
	if (mode == NULL || mode[0] == '\0') {
		return AERON_VFS_READ;
	}

	if (mode[0] == 'w') {
		return strchr(mode, '+') != NULL ? AERON_VFS_WRITE_READ : AERON_VFS_WRITE;
	}

	if (mode[0] == 'a') {
		return AERON_VFS_APPEND;
	}

	if (strchr(mode, '+') != NULL) {
		return AERON_VFS_READ_WRITE;
	}

	return AERON_VFS_READ;
}

static int File_SplitWildcard(const char* wildcard, char* directory, size_t directorySize,
							  const char** pattern) {
	const char* lastSlash;
	const char* cursor;
	size_t directoryLength;

	if (wildcard == NULL || directory == NULL || directorySize == 0 || pattern == NULL) {
		return 0;
	}

	lastSlash = NULL;
	for (cursor = wildcard; *cursor != '\0'; ++cursor) {
		if (*cursor == '/' || *cursor == '\\') {
			lastSlash = cursor;
		}
	}

	if (lastSlash == NULL) {
		if (directorySize < 2) {
			return 0;
		}

		directory[0] = '.';
		directory[1] = '\0';
		*pattern = wildcard;
		return 1;
	}

	directoryLength = (size_t)(lastSlash - wildcard);
	if (directoryLength == 0) {
		directoryLength = 1;
	}

	if (directoryLength + 1 > directorySize) {
		return 0;
	}

	memcpy(directory, wildcard, directoryLength);
	directory[directoryLength] = '\0';
	*pattern = lastSlash + 1;
	return **pattern != '\0';
}

void File_SetVfs(AeronVfs* vfs) { g_fileVfs = vfs; }

// FUNCTION: XWA 0x52AD30
#ifdef XWA_MODERN
XwaFile* File_Open(AeronVfsRoot root, const char* fileName, const char* mode) {
#else
XwaFile* File_Open(const char* fileName, const char* mode) {
	AeronVfsRoot root;
#endif
	AeronFile* file;
	AeronVfsOpenMode openMode;

#ifndef XWA_MODERN
	root = AERON_VFS_ROOT_ASSET;
#endif
	if (g_fileVfs == NULL || fileName == NULL || mode == NULL) {
		return NULL;
	}

	openMode = File_OpenMode(mode);
	file = NULL;
	g_lastOpenedFileRoot = root;
	strncpy(FileName, fileName, sizeof(FileName) - 1);
	FileName[sizeof(FileName) - 1] = '\0';

	if (AeronVfs_Open(g_fileVfs, root, fileName, openMode, &file)) {
		++g_openFileCount;
		return file;
	}

	return NULL;
}

// FUNCTION: XWA 0x52ADD0
int File_Close(XwaFile* stream) {
	if (stream == NULL) {
		return 0;
	}

	--g_openFileCount;
	return AeronVfs_Close(stream) ? 0 : -1;
}

// FUNCTION: XWA 0x52ADF0
int File_Seek(XwaFile* stream, int offset, int16_t origin) {
#ifdef XWA_MODERN
	return AeronVfs_Seek(stream, offset, origin) ? 0 : -1;
#else
	return fseek(stream, offset, origin);
#endif
}

// FUNCTION: XWA 0x52AE10
int File_Tell(XwaFile* stream) {
#ifdef XWA_MODERN
	return (int)AeronVfs_Tell(stream);
#else
	return ftell(stream);
#endif
}

// FUNCTION: XWA 0x52AE20
int File_GetSize(XwaFile* stream) { return (int)AeronVfs_GetSize(stream); }

int File_Flush(XwaFile* stream) { return AeronVfs_Flush(stream) ? 0 : -1; }

int File_Glob(AeronVfsRoot root, const char* wildcard, uint32_t flags, FileGlobCallback callback,
			  void* userdata) {
	char directory[512];
	const char* pattern;

	if (g_fileVfs == NULL || callback == NULL ||
		!File_SplitWildcard(wildcard, directory, sizeof(directory), &pattern)) {
		return 0;
	}

	return AeronVfs_Glob(g_fileVfs, root, directory, pattern, flags, callback, userdata);
}

int File_Remove(AeronVfsRoot root, const char* fileName) {
	if (g_fileVfs == NULL || fileName == NULL) {
		return -1;
	}

	return AeronVfs_Remove(g_fileVfs, root, fileName) ? 0 : -1;
}

int File_Rename(AeronVfsRoot root, const char* oldFileName, const char* newFileName) {
	if (g_fileVfs == NULL || oldFileName == NULL || newFileName == NULL) {
		return -1;
	}

	return AeronVfs_Rename(g_fileVfs, root, oldFileName, newFileName) ? 0 : -1;
}

int File_HasFatalError(void) { return g_fileFatalErrorPending; }

int File_GetFatalExitCode(void) { return g_fileFatalExitCode; }

// FUNCTION: XWA 0x52B100
char File_GetCdDriveLetter(void) { return g_cdDriveLetter; }

// FUNCTION: XWA 0x50E5A0
void File_PrintFatalMessageAndExit(char* message, int exitCode) {
	Aeron_LogInfo("xwa.file", "%s", message != NULL ? message : "");
#ifdef XWA_MODERN
	Aeron_FatalError("OpenXWA", message != NULL ? message : "");
#else
	Aeron_FatalError("X-Wing Alliance", message != NULL ? message : "");
#endif
	g_fileFatalExitCode = exitCode;
	g_fileFatalErrorPending = 1;
}

bool File_ReadLine(XwaFile* stream, char* buffer, size_t bufferSize) {
	return AeronVfs_ReadLine(stream, buffer, bufferSize) != 0;
}

// FUNCTION: XWA 0x52AE60
bool File_ReadByte(XwaFile* stream, void* buffer) { return File_ReadCount(stream, buffer, 1); }

// FUNCTION: XWA 0x52AE90
bool File_ReadWord(XwaFile* stream, void* buffer) { return File_ReadCount(stream, buffer, 2); }

// FUNCTION: XWA 0x52AEC0
bool File_ReadDword(XwaFile* stream, void* buffer) { return File_ReadCount(stream, buffer, 4); }

// FUNCTION: XWA 0x52AEF0
int16_t File_ReadCount(XwaFile* stream, void* buffer, size_t count) {
#ifdef XWA_MODERN
	return AeronVfs_Read(stream, buffer, count, NULL) != 0;
#else
	int16_t readFailed;

	readFailed = fread(buffer, 1, count, stream) != count;
	return readFailed == 0;
#endif
}

size_t File_ReadPartial(XwaFile* stream, void* buffer, size_t count) {
	size_t bytesRead;

	bytesRead = 0;
	AeronVfs_Read(stream, buffer, count, &bytesRead);
	return bytesRead;
}

// FUNCTION: XWA 0x52AF20
bool File_WriteByte(XwaFile* stream, int value) {
	unsigned char byteValue = (unsigned char)value;

	return File_WriteCount(stream, &byteValue, 1);
}

// FUNCTION: XWA 0x52AF50
bool File_WriteWord(XwaFile* stream, int value) {
	unsigned char bytes[2];

	bytes[0] = (unsigned char)value;
	bytes[1] = (unsigned char)((unsigned int)value >> 8);
	return File_WriteCount(stream, bytes, sizeof(bytes));
}

// FUNCTION: XWA 0x52AF80
bool File_WriteDword(XwaFile* stream, int value) {
	unsigned char bytes[4];

	bytes[0] = (unsigned char)value;
	bytes[1] = (unsigned char)((unsigned int)value >> 8);
	bytes[2] = (unsigned char)((unsigned int)value >> 16);
	bytes[3] = (unsigned char)((unsigned int)value >> 24);
	return File_WriteCount(stream, bytes, sizeof(bytes));
}

// FUNCTION: XWA 0x52AFB0
int16_t File_WriteCount(XwaFile* stream, const void* buffer, size_t count) {
#ifdef XWA_MODERN
	return AeronVfs_Write(stream, buffer, count, NULL) != 0;
#else
	int16_t writeFailed;

	writeFailed = fwrite(buffer, 1, count, stream) != count;
	return writeFailed == 0;
#endif
}

size_t File_WritePartial(XwaFile* stream, const void* buffer, size_t count) {
	size_t bytesWritten;

	bytesWritten = 0;
	AeronVfs_Write(stream, buffer, count, &bytesWritten);
	return bytesWritten;
}

// FUNCTION: XWA 0x52AFE0
int File_CheckGameCdPresent(int diskNum) {
	(void)diskNum;

	/* TODO: Reimplement File_CheckGameCdPresent @ 0x52AFE0. */
	return 1;
}

// TODO: stub for XWA 0x52B220 — original probes the CD/game install for the given
// file and updates the resolved game/CD paths.
int File_DetectGameAndCdPaths(const char* probeFilePath) {
	(void)probeFilePath;

	return 0;
}
