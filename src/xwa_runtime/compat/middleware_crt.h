#ifndef XWA_RUNTIME_COMPAT_MIDDLEWARE_CRT_H
#define XWA_RUNTIME_COMPAT_MIDDLEWARE_CRT_H

#include <string.h>

/* The iMUSE and std3D middleware were linked as separate units, each carrying
 * its own private copy of the CRT mem routines. Their memcpy is a distinct copy
 * from the game's: IDA `_memcpy_0` @0x59d7f0 versus the game's `_memcpy`
 * @0x59a9a0 (the two bodies are identical, only their link placement differs).
 *
 * Middleware translation units therefore call `memcpy_0` so the byte-matching
 * build resolves the correct copy. The modern port build has no such split, so
 * `memcpy_0` collapses to the real `memcpy`. */
#if defined(_MSC_VER) && !defined(XWA_MODERN)
void* memcpy_0(void* dst, const void* src, size_t size);
#else
#define memcpy_0 memcpy
#endif

#endif /* XWA_RUNTIME_COMPAT_MIDDLEWARE_CRT_H */
