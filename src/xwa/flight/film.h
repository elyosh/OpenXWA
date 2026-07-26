#ifndef XWA_FLIGHT_FILM_H
#define XWA_FLIGHT_FILM_H

#include "xwa/util/memory.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int Film_ReadBytes(void* dst, int size);
int Film_WriteBytesBuffered(const void* src, size_t size);
int Film_SeekPastHeaderAndMissionName(void);
int Film_SeekToEmbeddedMissionData(void);
int Film_SeekPastEmbeddedMissionData(void);
int Film_FlushWriteBuffer(void);
int FilmNamePrompt_Update(int frameCounter);
char* FilmNamePrompt_Run(void);

extern uint8_t* g_filmWriteBuffer;
extern MemoryHandle g_filmWriteBufferHandle;
extern int      g_filmWriteBufferSize;
extern int      g_filmWriteBufferedBytes;
extern int      g_filmNamePromptOverwriteConfirm;
extern char     g_filmNamePromptName[128];

#ifdef __cplusplus
}
#endif

#endif
