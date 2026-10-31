#ifndef XWA_RUNTIME_MODERN_MODS_H
#define XWA_RUNTIME_MODERN_MODS_H

/* The mod that OpenXWA uses, as the options show it. The host gives the
 * list of mods and the one running. A new choice is saved by the host's
 * callback on Flush, and takes effect the next time OpenXWA starts, since
 * the mod folder is bound before any data loads. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaModernModEntry {
	const char* name;
	const char* path; /* empty for the original game */
	int complete;     /* 0 when its import didn't finish: it can't be chosen */
} XwaModernModEntry;

/* Saves the choice: `path` is empty for the original game. */
typedef int (*XwaModernModsPersistFn)(const char* path, char* error, size_t error_size);

/* Copies the entries. `running` is the entry of the mod in use, or SIZE_MAX.
 * With `launch_override`, the mod was set by the --mod option and can't be
 * changed. */
void XwaModernMods_Configure(const XwaModernModEntry* entries, size_t count, size_t running,
							 int launch_override, XwaModernModsPersistFn persist);
size_t XwaModernMods_Count(void);
const XwaModernModEntry* XwaModernMods_Entry(size_t index);
/* The chosen entry, or SIZE_MAX. */
size_t XwaModernMods_Get(void);
/* The entry of the mod in use, or SIZE_MAX. */
size_t XwaModernMods_Running(void);
int XwaModernMods_LaunchOverride(void);
/* Chooses entry `index`, which must be complete. */
int XwaModernMods_Set(size_t index);
int XwaModernMods_Flush(void);

#ifdef __cplusplus
}
#endif

#endif
