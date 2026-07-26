#ifndef XWA_FRONTEND_FRONTEND_FILE_STREAM_H
#define XWA_FRONTEND_FRONTEND_FILE_STREAM_H

#include "xwa/assets/file_io.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
typedef struct FrontendFileStreamRequest {
	char path[256];
	int startChunkIndex;
	uint32_t bufferedBytes;
	uint8_t eof;
	uint8_t fileOpened;
	struct FrontendFileStreamRequest* next;
} FrontendFileStreamRequest;
#pragma pack(pop)

typedef char
	frontend_file_stream_request_next_offset[(offsetof(FrontendFileStreamRequest, next) == 0x10a) ? 1 : -1];

extern int g_frontendFileStreamState[2];
extern int g_frontendFileStreamChunkCount[2];
extern int g_frontendFileStreamWriteChunkIdx[2];
extern int g_frontendFileStreamReadChunkIdx[2];
extern int g_frontendFileStreamReadDelayMs[2];
extern int g_frontendFileStreamDefaultDelayMs;
extern int g_frontendFileStreamCapacityBytes[2];
extern int g_frontendFileStreamField38F8[2];
extern int g_frontendFileStreamPreloadChunkCount[2];
extern int g_frontendFileStreamServiceSlot;
extern int g_frontendFileStreamStateStartTime[2];
extern int g_frontendFileStreamHalfCapacityBytes[2];
extern int g_frontendFileStreamReadOffset[2];
extern uint8_t g_frontendFileStreamFastRead[2];
extern uint8_t g_frontendFileStreamSlotEnabled[2];
extern XwaFile* g_frontendFileStreamFiles[2];
extern char g_frontendFileStreamDriveLetter[3];
extern void** g_frontendFileStreamChunkPtrs[2];
extern FrontendFileStreamRequest* g_frontendFileStreamCurrentRequest[2];
extern FrontendFileStreamRequest* g_frontendFileStreamQueueHead[2];
extern int g_frontendFileStreamNormalDelayMs[2];
extern int g_frontendFileStreamLastSlotSwitchTime;

int FrontendFileStream_InitSlotBuffer(int slot, int totalBytes, int initialBufferedBytes);
int FrontendFileStream_SetSlotDriveAndFastRead(int slot, char fastRead, char driveLetter);
void FrontendFileStream_FreeSlot(int slot);
int FrontendFileStream_ServiceSlots(void);
unsigned int FrontendFileStream_ReadBytes(int slot, void* dst, int dstOffset, unsigned int byteCount,
										  int blockUntilAvailable);
int FrontendFileStream_QueueFile(int slot, const char* path);
int FrontendFileStream_PopHead(int slot);
int FrontendFileStream_RotateLoopQueue(int slot);
int FrontendFileStream_PrimeFromQueuedFile(int slot, const char* pathPrefix);
int FrontendFileStream_OpenQueuedFile(int slot, FrontendFileStreamRequest* request);
void* FrontendFileStream_ReserveWriteChunk(void);
int FrontendFileStream_ReadNextChunk(void);
void FrontendFileStream_AppendRequest(int slot, FrontendFileStreamRequest* request);
void FrontendFileStream_UnlinkAndFreeRequest(int slot, FrontendFileStreamRequest* request);

#ifdef __cplusplus
}
#endif

#endif
