#ifndef XWA_AUDIO_IMUSE_IMHOST_H
#define XWA_AUDIO_IMUSE_IMHOST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "xwa/assets/file_io.h"

int ImHost_PrintStatus(const char* fmt, ...);
int ImHost_PrintMessage(const char* fmt, ...);
int ImHost_PrintWarning(const char* fmt, ...);
int ImHost_PrintError(const char* fmt, ...);
int ImHost_PrintDebug(const char* fmt, ...);
void ImHost_AssertFail(const char* expr, const char* file, int line);
int ImHost_RegisterAtExit(void (*fn)(void));
void* ImHost_AllocMem(unsigned int size);
void ImHost_FreeMem(void* block);
void* ImHost_ReallocMem(void* block, unsigned int size);
void* ImHost_LockHandle(void* handle);
int ImHost_GetTime(void);
XwaFile* ImHost_OpenFile(const char* path, const char* mode);
int ImHost_CloseFile(XwaFile* stream);
unsigned int ImHost_ReadFile(XwaFile* stream, void* dst, unsigned int size);
char* ImHost_ReadLine(XwaFile* stream, char* dst, int size);
unsigned int ImHost_WriteFile(XwaFile* stream, void* src, unsigned int size);
int ImHost_TellFile(XwaFile* stream);
int ImHost_SeekFile(XwaFile* stream, int offset, int origin);
int ImHost_AtEof(XwaFile* stream);
int ImHost_GetFileSize(const char* path);
int ImHost_FilePrintf(XwaFile* stream, const char* fmt, ...);
void* ImHost_AllocHandle(unsigned int size);
void ImHost_FreeHandle(void* handle);
void* ImHost_ReallocHandle(void* handle, unsigned int size);
void ImHost_UnlockHandle(void* handle);

#ifdef __cplusplus
}
#endif

#endif
