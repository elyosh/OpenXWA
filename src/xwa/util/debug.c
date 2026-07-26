#include "xwa/util/debug.h"

// FUNCTION: XWA 0x50A490
int DebugPrintf(const char* format, ...) {
	(void)format;
	return 0;
}

// Default no-op target for the DebugPrintfChannel pointer. The retail build
// suppresses channel debug output; the indirect dispatch shape (a call through
// a function-pointer global) is what the original call sites compile to.
static int DebugPrintfChannel_NoOp(int channelFlags, const char* format, ...) {
	(void)channelFlags;
	(void)format;
	return 0;
}

// GLOBAL: XWA 0x8D5758
int (*DebugPrintfChannel)(int channelFlags, const char* format, ...) = DebugPrintfChannel_NoOp;
