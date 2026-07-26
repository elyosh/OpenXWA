#ifndef XWA_UTIL_MEMORY_H
#define XWA_UTIL_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t MemoryHandle;

enum {
	MEMORY_MAX_HANDLES = 5000,
};

extern int g_handleAllocatorInitialized;
extern int g_handleAllocatedCount;
extern int g_handleAllocatedPeakCount;
extern int g_handleAllocatedTotalBytes;

void* Memory_AllocTagged(const char* tag, size_t size);
void* Memory_CallocTagged(const char* tag, size_t count, size_t size);
void* Memory_ReallocTagged(const char* tag, void* block, size_t size);
void Memory_FreeTagged(const char* tag, void* block);

MemoryHandle Memory_AllocHandle(const char* tag, size_t size);
MemoryHandle Memory_AllocHandleZeroed(const char* tag, size_t size);
MemoryHandle Memory_AllocHandleInternal(const char* tag, int size, int clearFlag);
void Memory_FreeHandle(const char* tag, MemoryHandle handle);
void* Memory_LockHandle(MemoryHandle handle);
int Memory_UnlockHandle(MemoryHandle handle);
int Memory_GetHandleSize(MemoryHandle handle);

void* Mem_Alloc(size_t size);
int Mem_Free(void* block);
void* Mem_Realloc(void* block, size_t size);

#ifdef __cplusplus
}
#endif

#endif
