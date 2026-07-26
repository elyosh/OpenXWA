#include "xwa/util/memory.h"
#include "xwa/util/debug.h"

#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0x781E5C
int g_handleAllocatorInitialized = 0;
// GLOBAL: XWA 0x781E50
int g_handleAllocatedCount = 0;
// GLOBAL: XWA 0x781E54
int g_handleAllocatedPeakCount = 0;
// GLOBAL: XWA 0x781E58
int g_handleAllocatedTotalBytes = 0;

// GLOBAL: XWA 0x778208
int g_handleLockCount[MEMORY_MAX_HANDLES];
// GLOBAL: XWA 0x7733E8
int g_handleSizeTable[MEMORY_MAX_HANDLES];
// GLOBAL: XWA 0x77D030
void* g_handlePtrTable[MEMORY_MAX_HANDLES];

// GLOBAL: XWA 0x5AA020
const int g_handleMask = 0x3fff;

static void Memory_InitHandleAllocatorIfNeeded(void) {
	if (g_handleAllocatorInitialized) {
		return;
	}

	g_handleAllocatedCount = 0;
	g_handleAllocatedPeakCount = 0;
	g_handleAllocatedTotalBytes = 0;
	memset(g_handleLockCount, 0, sizeof(g_handleLockCount));
	memset(g_handleSizeTable, 0, sizeof(g_handleSizeTable));
	memset(g_handlePtrTable, 0, sizeof(g_handlePtrTable));
	g_handleAllocatorInitialized = 1;
}

// FUNCTION: XWA 0x50E070
void* Memory_AllocTagged(const char* tag, size_t size) {
	(void)tag;
	return malloc(size);
}

// FUNCTION: XWA 0x50E080
void* Memory_CallocTagged(const char* tag, size_t count, size_t size) {
	(void)tag;
	return calloc(count, size);
}

// FUNCTION: XWA 0x50E0A0
void* Memory_ReallocTagged(const char* tag, void* block, size_t size) {
	(void)tag;
	return realloc(block, size);
}

// FUNCTION: XWA 0x50E0C0
void Memory_FreeTagged(const char* tag, void* block) {
	free(block);
	if (block == NULL) {
		DebugPrintf("TGFree() was passed a NULL pointer to free block:", tag);
	}
}

// FUNCTION: XWA 0x50E0F0
MemoryHandle Memory_AllocHandle(const char* tag, size_t size) {
	return Memory_AllocHandleInternal(tag, (int)size, 0);
}

// FUNCTION: XWA 0x50E110
MemoryHandle Memory_AllocHandleZeroed(const char* tag, size_t size) {
	return Memory_AllocHandleInternal(tag, (int)size, 1);
}

// FUNCTION: XWA 0x50E130
MemoryHandle Memory_AllocHandleInternal(const char* tag, int size, int clearFlag) {
	int i;
	void* block;

	(void)tag;
	Memory_InitHandleAllocatorIfNeeded();

	if (size <= 0) {
		DebugPrintf("zero or negative size in Alloc_Handle()");
	}

	for (i = 0; i < MEMORY_MAX_HANDLES; ++i) {
		if (g_handlePtrTable[i] == NULL) {
			break;
		}
	}

	if (i == MEMORY_MAX_HANDLES) {
		DebugPrintf("out of handles in Alloc_Handle");
		return 0;
	}

	block = malloc((size_t)size);
	g_handlePtrTable[i] = block;
	if (block == NULL) {
		return 0;
	}

	if (clearFlag) {
		memset(block, 0, (size_t)size);
	}

	++g_handleAllocatedCount;
	if (g_handleAllocatedCount > g_handleAllocatedPeakCount) {
		g_handleAllocatedPeakCount = g_handleAllocatedCount;
	}

	g_handleAllocatedTotalBytes += size;
	g_handleSizeTable[i] = size;
	g_handleLockCount[i] = 0;
	return (MemoryHandle)(i + 1);
}

// FUNCTION: XWA 0x50E240
void Memory_FreeHandle(const char* tag, MemoryHandle handle) {
	int index = (int)handle - 1;
	void* block;
	int result;

	/* The original only warns on a bad handle and still frees the slot. */
	if (handle < 1u || handle > MEMORY_MAX_HANDLES) {
		DebugPrintf("bad handle in Free_Handle()");
	}

	if (g_handlePtrTable[index] == NULL) {
		DebugPrintf("unused handle in Free_Handle()");
	}

	block = g_handlePtrTable[index];
	free(block);
	if (block == NULL) {
		DebugPrintf("TGFree() was passed a NULL pointer to free block:", tag);
	}

	g_handleAllocatedTotalBytes -= g_handleSizeTable[index];
	result = g_handleAllocatedCount - 1;
	g_handlePtrTable[index] = NULL;
	g_handleSizeTable[index] = 0;
	g_handleLockCount[index] = 0;
	g_handleAllocatedCount = result;
}

// FUNCTION: XWA 0x50E2F0
void* Memory_LockHandle(MemoryHandle handle) {
	unsigned short masked;
	int index;

	masked = (unsigned short)(handle & g_handleMask);
	index = masked - 1;

	if (masked < 1 || masked > MEMORY_MAX_HANDLES) {
		DebugPrintf("bad handle in Lock_Handle()");
	}
	if (g_handlePtrTable[index] == NULL) {
		DebugPrintf("unused handle in Lock_Handle()");
	}

	++g_handleLockCount[index];
	return g_handlePtrTable[index];
}

// FUNCTION: XWA 0x50E350
int Memory_UnlockHandle(MemoryHandle handle) {
	unsigned short masked;
	int index;

	if ((unsigned short)handle == 0) {
		return handle;
	}

	masked = (unsigned short)(handle & g_handleMask);
	index = masked - 1;

	if (masked < 1 || masked > MEMORY_MAX_HANDLES) {
		DebugPrintf("bad handle in Unlock_Handle()");
	}
	if (g_handlePtrTable[index] == NULL) {
		DebugPrintf("unused handle in Unlock_Handle()");
	}

	return --g_handleLockCount[index];
}

// FUNCTION: XWA 0x50E3B0
int Memory_GetHandleSize(MemoryHandle handle) {
	handle &= g_handleMask;

	if (handle < 1 || handle > MEMORY_MAX_HANDLES) {
		DebugPrintf("bad handle in GetSizeFromHandle()");
	}
	if (g_handlePtrTable[handle - 1] == NULL) {
		DebugPrintf("unused handle in GetSizeFromHandle()");
	}

	return g_handleSizeTable[handle - 1];
}

// FUNCTION: XWA 0x55D470
void* Mem_Alloc(size_t size) { return malloc(size); }

// FUNCTION: XWA 0x55D480
int Mem_Free(void* block) {
	free(block);
	return 1;
}

// FUNCTION: XWA 0x55D4A0
void* Mem_Realloc(void* block, size_t size) { return realloc(block, size); }
