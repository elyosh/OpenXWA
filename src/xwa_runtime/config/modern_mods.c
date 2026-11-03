#include "xwa_runtime/config/modern_mods.h"

#include "aeron/log.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static struct {
	XwaModernModEntry* entries;
	size_t count;
	size_t chosen;
	/* The last chosen entry without a problem. Flush saves this one. */
	size_t usable;
	size_t running;
	int launch_override;
	XwaModernModsPersistFn persist;
	int dirty;
} g_modernMods;

static char* XwaModernMods_CopyString(const char* text) {
	const size_t length = strlen(text ? text : "");
	char* copy = (char*)malloc(length + 1);
	if (copy) {
		memcpy(copy, text ? text : "", length + 1);
	}
	return copy;
}

static void XwaModernMods_Free(void) {
	for (size_t i = 0; i < g_modernMods.count; ++i) {
		free((char*)g_modernMods.entries[i].name);
		free((char*)g_modernMods.entries[i].path);
		free((char*)g_modernMods.entries[i].problem);
	}
	free(g_modernMods.entries);
	memset(&g_modernMods, 0, sizeof g_modernMods);
	g_modernMods.chosen = SIZE_MAX;
	g_modernMods.usable = SIZE_MAX;
	g_modernMods.running = SIZE_MAX;
}

void XwaModernMods_Configure(const XwaModernModEntry* entries, size_t count, size_t running,
							 int launch_override, XwaModernModsPersistFn persist) {
	XwaModernMods_Free();
	g_modernMods.entries = count ? (XwaModernModEntry*)calloc(count, sizeof *g_modernMods.entries) : NULL;
	if (count && !g_modernMods.entries) {
		Aeron_LogError("xwa.config", "out of memory for the mod list");
		return;
	}
	for (size_t i = 0; i < count; ++i) {
		XwaModernModEntry* entry = &g_modernMods.entries[i];
		entry->name = XwaModernMods_CopyString(entries[i].name);
		entry->path = XwaModernMods_CopyString(entries[i].path);
		entry->problem = entries[i].problem ? XwaModernMods_CopyString(entries[i].problem) : NULL;
		g_modernMods.count = i + 1;
		if (!entry->name || !entry->path || (entries[i].problem && !entry->problem)) {
			Aeron_LogError("xwa.config", "out of memory for the mod list");
			XwaModernMods_Free();
			return;
		}
	}
	g_modernMods.running = running < count ? running : SIZE_MAX;
	g_modernMods.chosen = g_modernMods.running;
	g_modernMods.usable = g_modernMods.running;
	g_modernMods.launch_override = launch_override != 0;
	g_modernMods.persist = persist;
}

size_t XwaModernMods_Count(void) { return g_modernMods.count; }

const XwaModernModEntry* XwaModernMods_Entry(size_t index) {
	return index < g_modernMods.count ? &g_modernMods.entries[index] : NULL;
}

size_t XwaModernMods_Get(void) { return g_modernMods.chosen; }

size_t XwaModernMods_Running(void) { return g_modernMods.running; }

int XwaModernMods_LaunchOverride(void) { return g_modernMods.launch_override; }

int XwaModernMods_Set(size_t index) {
	if (g_modernMods.launch_override || index >= g_modernMods.count) {
		return 0;
	}
	g_modernMods.chosen = index;
	if (!g_modernMods.entries[index].problem && index != g_modernMods.usable) {
		g_modernMods.usable = index;
		g_modernMods.dirty = 1;
	}
	return 1;
}

int XwaModernMods_Flush(void) {
	char error[512];

	g_modernMods.chosen = g_modernMods.usable;
	if (!g_modernMods.dirty || g_modernMods.usable >= g_modernMods.count) {
		return 1;
	}
	if (!g_modernMods.persist) {
		Aeron_LogWarn("xwa.config", "the mod choice changed but no persistence callback is registered");
		return 0;
	}
	error[0] = '\0';
	if (!g_modernMods.persist(g_modernMods.entries[g_modernMods.usable].path, error, sizeof error)) {
		Aeron_LogError("xwa.config", "%s",
					   error[0] ? error : "could not save the mod choice to user configuration");
		return 0;
	}
	g_modernMods.dirty = 0;
	return 1;
}
