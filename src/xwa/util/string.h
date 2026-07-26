#ifndef XWA_UTIL_STRING_H
#define XWA_UTIL_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

int Xwa_Stricmp(const char* left, const char* right);
int StrCmpI(const char* lhs, const char* rhs);

#ifdef __cplusplus
}
#endif

// Case-insensitive compare matching the CRT _strcmpi the retail binary calls at
// some recovered sites. _strcmpi is the MSVC CRT spelling (so the matching build
// reproduces the original `call __strcmpi`); strcasecmp is the POSIX equivalent
// used by the non-MSVC port build.
#ifdef _MSC_VER
#include <string.h>
#define Xwa_CrtStricmp _strcmpi
#else
#include <strings.h>
#define Xwa_CrtStricmp strcasecmp
#endif

#endif
