#include "xwa/frontend/frontend_file_stream.h"

#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_wave_stream.h"
#include "xwa/util/memory.h"
#include "xwa/util/time.h"

#include <stdio.h>
#include <string.h>

typedef enum FrontendFileStreamState {
	FFS_OPEN_PENDING = 0,
	FFS_MARK_READING_STARTED = 1,
	FFS_READING = 2,
	FFS_WAIT_BELOW_HALF_BUFFER = 3,
	FFS_THROTTLED = 4,
	FFS_INACTIVE = 5,
} FrontendFileStreamState;

// GLOBAL: XWA 0x783940
int g_frontendFileStreamState[2];
// GLOBAL: XWA 0x7838C8
int g_frontendFileStreamChunkCount[2];
// GLOBAL: XWA 0x7838D0
int g_frontendFileStreamWriteChunkIdx[2];
// GLOBAL: XWA 0x7838D8
int g_frontendFileStreamReadChunkIdx[2];
// GLOBAL: XWA 0x7838E0
int g_frontendFileStreamReadDelayMs[2];
// GLOBAL: XWA 0x7838E8
int g_frontendFileStreamDefaultDelayMs;
// GLOBAL: XWA 0x7838F0
int g_frontendFileStreamCapacityBytes[2];
// GLOBAL: XWA 0x7838F8
int g_frontendFileStreamField38F8[2];
// GLOBAL: XWA 0x783908
int g_frontendFileStreamPreloadChunkCount[2];
// GLOBAL: XWA 0x783930
int g_frontendFileStreamReadOffset[2];
// GLOBAL: XWA 0x783910
int g_frontendFileStreamServiceSlot;
// GLOBAL: XWA 0x783920
int g_frontendFileStreamStateStartTime[2];
// GLOBAL: XWA 0x783928
int g_frontendFileStreamHalfCapacityBytes[2];
// GLOBAL: XWA 0x783948
uint8_t g_frontendFileStreamSlotEnabled[2];
// GLOBAL: XWA 0x604270
uint8_t g_frontendFileStreamFastRead[2] = { 0, 1 };
// GLOBAL: XWA 0x783900
XwaFile* g_frontendFileStreamFiles[2];
// GLOBAL: XWA 0x604268
char g_frontendFileStreamDriveLetter[3];
// GLOBAL: XWA 0x783938
void** g_frontendFileStreamChunkPtrs[2];
// GLOBAL: XWA 0x9F4B28
FrontendFileStreamRequest* g_frontendFileStreamCurrentRequest[2];
// GLOBAL: XWA 0x9F4B30
FrontendFileStreamRequest* g_frontendFileStreamQueueHead[2];
// GLOBAL: XWA 0x783950
int g_frontendFileStreamNormalDelayMs[2];
// GLOBAL: XWA 0x7838C0
int g_frontendFileStreamLastSlotSwitchTime;

// FUNCTION: XWA 0x55ED10
int FrontendFileStream_InitSlotBuffer(int slot, int totalBytes, int initialBufferedBytes) {
	(void)slot;
	(void)totalBytes;
	(void)initialBufferedBytes;

	/* TODO: Reimplement FrontendFileStream_InitSlotBuffer @ 0x55ED10. */
	return 1;
}

// FUNCTION: XWA 0x55F120
void FrontendFileStream_FreeSlot(int slot) {
	(void)slot;

	/* TODO: Reimplement FrontendFileStream_FreeSlot @ 0x55F120. */
}

// FUNCTION: XWA 0x55EEA0
int FrontendFileStream_ServiceSlots(void) {
	int tickCount;
	int slot;
	FrontendFileStreamRequest* request;

	tickCount = (int)GetTickCount();
	slot = g_frontendFileStreamServiceSlot;

	switch (g_frontendFileStreamState[g_frontendFileStreamServiceSlot]) {
		case FFS_OPEN_PENDING:
			request = g_frontendFileStreamCurrentRequest[g_frontendFileStreamServiceSlot];
			if (request == NULL) {
				goto switch_slot;
			}
			if (FrontendFileStream_OpenQueuedFile(
					g_frontendFileStreamServiceSlot,
					g_frontendFileStreamCurrentRequest[g_frontendFileStreamServiceSlot])) {
				request->startChunkIndex = g_frontendFileStreamWriteChunkIdx[g_frontendFileStreamServiceSlot];
				g_frontendFileStreamState[g_frontendFileStreamServiceSlot] = FFS_MARK_READING_STARTED;
				request->eof = 0;
				request->fileOpened = 1;
			} else {
				FrontendFileStreamRequest* nextRequest;

				nextRequest = request->next;
				FrontendFileStream_UnlinkAndFreeRequest(
					g_frontendFileStreamServiceSlot,
					g_frontendFileStreamCurrentRequest[g_frontendFileStreamServiceSlot]);
				g_frontendFileStreamCurrentRequest[g_frontendFileStreamServiceSlot] = nextRequest;
			}
			break;

		case FFS_MARK_READING_STARTED:
			g_frontendFileStreamStateStartTime[g_frontendFileStreamServiceSlot] = tickCount;
			g_frontendFileStreamState[slot] = FFS_READING;
			FrontendWaveStream_Update(0);
			return 0;

		case FFS_READING:
			if ((int)(tickCount - g_frontendFileStreamStateStartTime[g_frontendFileStreamServiceSlot]) <=
				g_frontendFileStreamReadDelayMs[g_frontendFileStreamServiceSlot]) {
				if ((int)(tickCount - g_frontendFileStreamLastSlotSwitchTime) > 1000) {
					goto switch_slot;
				}
			} else {
				if (FrontendFileStream_ReadNextChunk() == 1) {
					g_frontendFileStreamStateStartTime[g_frontendFileStreamServiceSlot] = (int)GetTickCount();
					FrontendWaveStream_Update(0);
					return 0;
				}
				g_frontendFileStreamState[g_frontendFileStreamServiceSlot] = FFS_WAIT_BELOW_HALF_BUFFER;
				g_frontendFileStreamServiceSlot = g_frontendFileStreamServiceSlot + 1;
				g_frontendFileStreamLastSlotSwitchTime = (int)GetTickCount();
				if (g_frontendFileStreamServiceSlot > 1) {
					g_frontendFileStreamServiceSlot = 0;
					FrontendWaveStream_Update(0);
					return 0;
				}
			}
			break;

		case FFS_WAIT_BELOW_HALF_BUFFER:
			if (g_frontendFileStreamField38F8[g_frontendFileStreamServiceSlot] >=
				g_frontendFileStreamHalfCapacityBytes[g_frontendFileStreamServiceSlot]) {
				goto switch_slot;
			}
			g_frontendFileStreamStateStartTime[g_frontendFileStreamServiceSlot] = tickCount;
			g_frontendFileStreamReadDelayMs[slot] =
				2 * ((g_frontendFileStreamNormalDelayMs[slot] << 15) / 0x2000);
			g_frontendFileStreamState[slot] = FFS_THROTTLED;
			FrontendWaveStream_Update(0);
			return 0;

		case FFS_THROTTLED:
			if ((int)(tickCount - g_frontendFileStreamStateStartTime[g_frontendFileStreamServiceSlot]) <=
				g_frontendFileStreamReadDelayMs[g_frontendFileStreamServiceSlot]) {
				File_Seek(g_frontendFileStreamFiles[g_frontendFileStreamServiceSlot], 0, SEEK_CUR);
				FrontendWaveStream_Update(0);
				return 0;
			}
			g_frontendFileStreamStateStartTime[g_frontendFileStreamServiceSlot] = tickCount;
			if (g_frontendFileStreamFastRead[slot]) {
				g_frontendFileStreamReadDelayMs[slot] = 3;
			} else {
				g_frontendFileStreamReadDelayMs[slot] = g_frontendFileStreamNormalDelayMs[slot];
			}
			g_frontendFileStreamState[slot] = FFS_READING;
			FrontendWaveStream_Update(0);
			return 0;

		case FFS_INACTIVE:
			++g_frontendFileStreamServiceSlot;
			g_frontendFileStreamLastSlotSwitchTime = tickCount;
			if (g_frontendFileStreamServiceSlot > 1) {
				g_frontendFileStreamServiceSlot = 0;
			}
			break;

		default:
			break;
	}

	FrontendWaveStream_Update(0);
	return 0;

switch_slot:
	++g_frontendFileStreamServiceSlot;
	g_frontendFileStreamLastSlotSwitchTime = tickCount;
	if (g_frontendFileStreamServiceSlot > 1) {
		g_frontendFileStreamServiceSlot = 0;
		FrontendWaveStream_Update(0);
		return 0;
	}
	FrontendWaveStream_Update(0);
	return 0;
}

// FUNCTION: XWA 0x55F800
unsigned int FrontendFileStream_ReadBytes(int slot, void* dst, int dstOffset, unsigned int byteCount,
										  int blockUntilAvailable) {
	unsigned int remaining;
	FrontendFileStreamRequest* request;
	unsigned int bytesReturned;
	int copiedBytes;
	void** chunkPtrs;

	if (!g_frontendFileStreamSlotEnabled[slot]) {
		return 0;
	}

	remaining = byteCount;
	if (byteCount > (unsigned int)g_frontendFileStreamCapacityBytes[slot]) {
		return 0;
	}

	request = g_frontendFileStreamQueueHead[slot];
	if (blockUntilAvailable && byteCount > request->bufferedBytes) {
		do {
			if (request->eof == 1) {
				break;
			}
			FrontendFileStream_ServiceSlots();
		} while (byteCount > request->bufferedBytes);
	}

	if (byteCount > request->bufferedBytes) {
		if (request->eof != 1) {
			return 0xffffffffu;
		}
		remaining = request->bufferedBytes;
	}

	bytesReturned = remaining;
	copiedBytes = 0;
	chunkPtrs = g_frontendFileStreamChunkPtrs[slot];
	while (remaining != 0) {
		unsigned int copyBytes;
		unsigned int newReadOffset;

		copyBytes = 0x2000u - (unsigned int)g_frontendFileStreamReadOffset[slot];
		if (copyBytes > remaining) {
			copyBytes = remaining;
		}
		memcpy((char*)dst + dstOffset + copiedBytes,
			   (char*)chunkPtrs[g_frontendFileStreamReadChunkIdx[slot]] +
				   g_frontendFileStreamReadOffset[slot],
			   copyBytes);

		newReadOffset = copyBytes + (unsigned int)g_frontendFileStreamReadOffset[slot];
		copiedBytes += (int)copyBytes;
		g_frontendFileStreamReadOffset[slot] = (int)newReadOffset;
		if (newReadOffset >= 0x2000u) {
			int nextReadChunkIdx;

			g_frontendFileStreamReadOffset[slot] = 0;
			nextReadChunkIdx = g_frontendFileStreamReadChunkIdx[slot] + 1;
			g_frontendFileStreamReadChunkIdx[slot] = nextReadChunkIdx;
			if (nextReadChunkIdx >= g_frontendFileStreamChunkCount[slot]) {
				g_frontendFileStreamReadChunkIdx[slot] = 0;
			}
		}
		remaining -= copyBytes;
		request->bufferedBytes -= copyBytes;
	}

	return bytesReturned;
}

// FUNCTION: XWA 0x55F220
int FrontendFileStream_QueueFile(int slot, const char* path) {
	FrontendFileStreamRequest* request;

	if (!g_frontendFileStreamSlotEnabled[slot]) {
		return 0;
	}

	request = (FrontendFileStreamRequest*)Mem_Alloc(sizeof(*request));
	if (request == NULL) {
		return 0;
	}

	if (strlen(path) > 0x100u) {
		return 0;
	}

	strcpy(request->path, path);
	request->startChunkIndex = 0;
	request->bufferedBytes = 0;
	request->eof = 0;
	request->fileOpened = 0;
	request->next = NULL;
	FrontendFileStream_AppendRequest(slot, request);
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x55F310
int FrontendFileStream_RotateLoopQueue(int slot) {
	FrontendFileStreamRequest* request;
	char path[256];
	char pathPrefix[256];

	if (!g_frontendFileStreamSlotEnabled[slot]) {
		return 0;
	}

	request = g_frontendFileStreamQueueHead[slot];
	if (request != NULL) {
		FrontendFileStreamRequest* next;

		strcpy(path, request->path);
		next = request->next;
		if (next != NULL) {
			strcpy(pathPrefix, next->path);
		} else {
			strcpy(pathPrefix, path);
		}

		FrontendFileStream_UnlinkAndFreeRequest(slot, request);
		g_frontendFileStreamQueueHead[slot] = next;
	}

	FrontendFileStream_QueueFile(slot, path);
	FrontendFileStream_PrimeFromQueuedFile(slot, pathPrefix);
	return 1;
}

// FUNCTION: XWA 0x55F410
int FrontendFileStream_PrimeFromQueuedFile(int slot, const char* pathPrefix) {
	FrontendFileStreamRequest* request;
	uint8_t keepScanning;
	void** chunkPtrs;

	if (!g_frontendFileStreamSlotEnabled[slot]) {
		return 0;
	}

	request = g_frontendFileStreamQueueHead[slot];
	keepScanning = 1;
	if (request != NULL) {
		do {
			if (!keepScanning) {
				break;
			}

			if (request == g_frontendFileStreamCurrentRequest[slot]) {
				keepScanning = 0;
			}
			if (strncmp(pathPrefix, request->path, strlen(pathPrefix)) == 0) {
				break;
			}

			request = request->next;
		} while (request != NULL);

		if (request == NULL) {
			return 0;
		}

		if (g_frontendFileStreamQueueHead[slot] != request) {
			FrontendFileStreamRequest* head;

			head = g_frontendFileStreamQueueHead[slot];
			do {
				FrontendFileStreamRequest* next;

				next = head->next;
				FrontendFileStream_UnlinkAndFreeRequest(slot, head);
				head = next;
			} while (head != request);
		}

		if (request->eof == 1) {
			g_frontendFileStreamReadChunkIdx[slot] = request->startChunkIndex;
			g_frontendFileStreamReadOffset[slot] = 0;
			return 1;
		}

		if (!request->fileOpened) {
			if (!FrontendFileStream_OpenQueuedFile(slot, request)) {
				return 0;
			}

			g_frontendFileStreamState[slot] = 1;
			request->startChunkIndex = g_frontendFileStreamWriteChunkIdx[slot];
			request->eof = 0;
			request->fileOpened = 1;
		}

		g_frontendFileStreamReadChunkIdx[slot] = request->startChunkIndex;
		g_frontendFileStreamReadOffset[slot] = 0;
		chunkPtrs = g_frontendFileStreamChunkPtrs[slot];
		while ((request->bufferedBytes >> 13) < (uint32_t)g_frontendFileStreamPreloadChunkCount[slot]) {
			void* writeChunk;
			size_t bytesRead;

			if (request->eof == 1) {
				break;
			}

			writeChunk = chunkPtrs[g_frontendFileStreamWriteChunkIdx[slot]];
#ifdef XWA_MODERN
			/* Frame-driven port time does not advance during this preload loop. */
			bytesRead = File_ReadPartial(g_frontendFileStreamFiles[slot], writeChunk, 0x2000);
#else
			{
				int elapsedTicks;

				for (elapsedTicks = 0; elapsedTicks < g_frontendFileStreamReadDelayMs[slot];
					 elapsedTicks += (int)GetTickCount()) {
				}
				bytesRead = fread(writeChunk, 1, 0x2000, g_frontendFileStreamFiles[slot]);
			}
#endif
			request->bufferedBytes += (uint32_t)bytesRead;
			if (bytesRead != 0x2000) {
				request->eof = 1;
			}

			++g_frontendFileStreamWriteChunkIdx[slot];
			if (g_frontendFileStreamWriteChunkIdx[slot] >= g_frontendFileStreamChunkCount[slot]) {
				g_frontendFileStreamWriteChunkIdx[slot] = 0;
			}
		}

		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x55F610
int FrontendFileStream_OpenQueuedFile(int slot, FrontendFileStreamRequest* request) {
	if (!g_frontendFileStreamSlotEnabled[slot]) {
		return 0;
	}

	if (g_frontendFileStreamFiles[slot] != NULL) {
		File_Close(g_frontendFileStreamFiles[slot]);
	}

	/* Resolve install-relative ("wave\\...") paths the same way the rest of the
	   sound subsystem does (ALLIANCE install subtree, then the CD-image tree). */
	g_frontendFileStreamFiles[slot] = FrontendSound_OpenAsset(request->path, "rb");
	return g_frontendFileStreamFiles[slot] != NULL;
}

// FUNCTION: XWA 0x55F2D0
int FrontendFileStream_PopHead(int slot) {
	FrontendFileStreamRequest* head;

	if (!g_frontendFileStreamSlotEnabled[slot]) {
		return 0;
	}
	head = g_frontendFileStreamQueueHead[slot];
	if (head != NULL) {
		FrontendFileStreamRequest* next = head->next;
		FrontendFileStream_UnlinkAndFreeRequest(slot, head);
		g_frontendFileStreamQueueHead[slot] = next;
	}
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x55F6F0
void* FrontendFileStream_ReserveWriteChunk(void) {
	int slot;
	int oldWriteChunkIdx;
	int nextWriteChunkIdx;

	slot = g_frontendFileStreamServiceSlot;
	oldWriteChunkIdx = g_frontendFileStreamWriteChunkIdx[slot];
	nextWriteChunkIdx = oldWriteChunkIdx + 1;
	g_frontendFileStreamWriteChunkIdx[slot] = nextWriteChunkIdx;
	if (nextWriteChunkIdx >= g_frontendFileStreamChunkCount[slot]) {
		g_frontendFileStreamWriteChunkIdx[slot] = 0;
	}

	if (g_frontendFileStreamWriteChunkIdx[slot] == g_frontendFileStreamReadChunkIdx[slot]) {
		g_frontendFileStreamWriteChunkIdx[slot] = oldWriteChunkIdx;
		return 0;
	}

	if (g_frontendFileStreamChunkPtrs[slot] != 0) {
		return g_frontendFileStreamChunkPtrs[slot][oldWriteChunkIdx];
	}
	return 0;
}

// FUNCTION: XWA 0x55F750
int FrontendFileStream_ReadNextChunk(void) {
	void* chunk;
	size_t bytesRead;
	FrontendFileStreamRequest* request;
	XwaFile* stream;

	if (!g_frontendFileStreamSlotEnabled[g_frontendFileStreamServiceSlot]) {
		return 0;
	}

	chunk = FrontendFileStream_ReserveWriteChunk();
	if (chunk == NULL) {
		return 0;
	}

#ifdef XWA_MODERN
	bytesRead = File_ReadPartial(g_frontendFileStreamFiles[g_frontendFileStreamServiceSlot], chunk, 0x2000);
#else
	bytesRead = fread(chunk, 1, 0x2000, g_frontendFileStreamFiles[g_frontendFileStreamServiceSlot]);
#endif
	request = g_frontendFileStreamCurrentRequest[g_frontendFileStreamServiceSlot];
	request->bufferedBytes += (uint32_t)bytesRead;
	if (bytesRead != 0x2000) {
		g_frontendFileStreamState[g_frontendFileStreamServiceSlot] = 0;
		request->eof = 1;
		stream = g_frontendFileStreamFiles[g_frontendFileStreamServiceSlot];
		g_frontendFileStreamCurrentRequest[g_frontendFileStreamServiceSlot] = request->next;
#ifdef XWA_MODERN
		File_Close(stream);
#else
		fclose(stream);
#endif
		g_frontendFileStreamFiles[g_frontendFileStreamServiceSlot] = NULL;
	}

	return 1;
}

// FUNCTION: XWA 0x55F970
void FrontendFileStream_AppendRequest(int slot, FrontendFileStreamRequest* request) {
	FrontendFileStreamRequest* cursor;
	uint8_t keepSearching;

	cursor = g_frontendFileStreamQueueHead[slot];
	if (cursor == 0) {
		g_frontendFileStreamQueueHead[slot] = request;
		g_frontendFileStreamCurrentRequest[slot] = request;
		return;
	}

	keepSearching = 1;
	while (keepSearching && cursor != 0) {
		if (cursor->next == 0) {
			cursor->next = request;
			keepSearching = 0;
		}
		cursor = cursor->next;
	}

	if (g_frontendFileStreamCurrentRequest[slot] == 0) {
		g_frontendFileStreamCurrentRequest[slot] = request;
	}
}

// FUNCTION: XWA 0x55F9E0
void FrontendFileStream_UnlinkAndFreeRequest(int slot, FrontendFileStreamRequest* request) {
	FrontendFileStreamRequest* next;
	FrontendFileStreamRequest* cursor;
	int keepSearching;

	next = request->next;
	cursor = g_frontendFileStreamQueueHead[slot];
	keepSearching = 1;
	Mem_Free(request);

	if (request == g_frontendFileStreamCurrentRequest[slot]) {
		g_frontendFileStreamCurrentRequest[slot] = next;
		g_frontendFileStreamState[slot] = 0;
	}

	if (request == g_frontendFileStreamQueueHead[slot]) {
		g_frontendFileStreamQueueHead[slot] = next;
		return;
	}

	while (keepSearching) {
		if (!cursor) {
			break;
		}
		if (cursor->next == request) {
			cursor->next = next;
			keepSearching = 0;
		}
		cursor = cursor->next;
	}
}
