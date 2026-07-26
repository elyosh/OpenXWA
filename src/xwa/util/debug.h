#ifndef XWA_UTIL_DEBUG_H
#define XWA_UTIL_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Declared with unspecified arguments: the retail binary has zero-argument call
   sites (the body compiles out to a bare `ret`), so callers may invoke it with
   or without a format string. */
#ifdef XWA_MODERN
int DebugPrintf(const char* format, ...);
#else
int DebugPrintf();
#endif

// The retail binary dispatches channel debug output through a function-pointer
// global (the call sites compile to `call dword ptr [g_debugPrintfChannel]`),
// so it is modelled as a pointer here rather than a direct function. It
// defaults to a no-op, matching the retail build's suppressed debug output.
extern int (*DebugPrintfChannel)(int channelFlags, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
