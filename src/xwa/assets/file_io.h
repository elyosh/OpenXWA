#ifndef XWA_ASSETS_FILE_IO_H
#define XWA_ASSETS_FILE_IO_H

#include "aeron/vfs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef AeronFile XwaFile;
typedef AeronVfsEntry XwaFileEntry;

typedef int (*FileGlobCallback)(void* userdata, const XwaFileEntry* entry);

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif

#ifndef SEEK_END
#define SEEK_END 2
#endif

extern int g_openFileCount;
extern char g_installPath[256];
extern char g_startupParentPath[256];
extern char FileName[256];
extern char g_cdDriveLetter;
extern XwaFile* g_stream;
extern int16_t g_fileReadAbortFlag;
extern int g_fileFatalErrorPending;
extern int g_fileFatalExitCode;
extern AeronVfsRoot g_lastOpenedFileRoot;

void File_SetVfs(AeronVfs* vfs);

#ifdef XWA_MODERN
XwaFile* File_Open(AeronVfsRoot root, const char* fileName, const char* mode);
#else
XwaFile* File_Open(const char* fileName, const char* mode);
#ifndef XWA_FILE_IO_IMPLEMENTATION
#define File_Open(root, fileName, mode) File_Open(fileName, mode)
#endif
#endif
int File_Close(XwaFile* stream);
int File_Seek(XwaFile* stream, int offset, int16_t origin);
int File_Tell(XwaFile* stream);
int File_GetSize(XwaFile* stream);
int File_Flush(XwaFile* stream);
int File_Glob(AeronVfsRoot root, const char* wildcard, uint32_t flags, FileGlobCallback callback,
			  void* userdata);
int File_Remove(AeronVfsRoot root, const char* fileName);
int File_Rename(AeronVfsRoot root, const char* oldFileName, const char* newFileName);
void File_PrintFatalMessageAndExit(char* message, int exitCode);
int File_HasFatalError(void);
int File_GetFatalExitCode(void);
char File_GetCdDriveLetter(void);
char File_GetInstallDriveLetter(void);
int File_CheckGameCdPresent(int diskNum);
int File_DetectGameAndCdPaths(const char* probeFilePath);

bool File_ReadLine(XwaFile* stream, char* buffer, size_t bufferSize);
bool File_ReadByte(XwaFile* stream, void* buffer);
bool File_ReadWord(XwaFile* stream, void* buffer);
bool File_ReadDword(XwaFile* stream, void* buffer);
int16_t File_ReadCount(XwaFile* stream, void* buffer, size_t count);
size_t File_ReadPartial(XwaFile* stream, void* buffer, size_t count);

bool File_WriteByte(XwaFile* stream, int value);
bool File_WriteWord(XwaFile* stream, int value);
bool File_WriteDword(XwaFile* stream, int value);
int16_t File_WriteCount(XwaFile* stream, const void* buffer, size_t count);
size_t File_WritePartial(XwaFile* stream, const void* buffer, size_t count);

#ifdef __cplusplus
}
#endif

#endif
