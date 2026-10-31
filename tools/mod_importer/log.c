#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_log.h>

#include "aeron/sync.h"
#include "import_internal.h"

#define RING_LINES 2000

struct XwaModImportLogRing {
	AeronMutex* mutex;
	FILE* file; /* import.log */
	char* lines[RING_LINES];
	ToolLogLevel levels[RING_LINES];
	unsigned long long count; /* lines written so far */
	XwaModImportProgress* progress;
	ToolLogSink echo;
	void* echo_user;
	SDL_LogOutputFunction sdl_output; /* SDL's log output before the import */
	void* sdl_user;
};

XwaModImportLogRing* ImportLog_CreateRing(void) {
	XwaModImportLogRing* ring = calloc(1, sizeof *ring);
	if (ring && !(ring->mutex = Aeron_MutexCreate())) {
		free(ring);
		return NULL;
	}
	return ring;
}

void ImportLog_FreeRing(XwaModImportLogRing* ring) {
	if (!ring)
		return;
	for (size_t i = 0; i < RING_LINES; ++i)
		free(ring->lines[i]);
	Aeron_MutexDestroy(ring->mutex);
	free(ring);
}

static void sink(void* user, ToolLogLevel level, const char* line) {
	XwaModImportLogRing* ring = user;
	if (level == TOOL_LOG_WARNING)
		Aeron_AtomicAdd(&ring->progress->warnings, 1);
	else if (level == TOOL_LOG_ERROR)
		Aeron_AtomicAdd(&ring->progress->errors, 1);
	Aeron_MutexLock(ring->mutex);
	if (ring->file) {
		fputs(line, ring->file);
		fputc('\n', ring->file);
		/* The log stays complete if the program crashes. */
		fflush(ring->file);
	}
	const size_t slot = (size_t)(ring->count % RING_LINES);
	free(ring->lines[slot]);
	ring->lines[slot] = StrDup(line);
	ring->levels[slot] = level;
	ring->count++;
	if (ring->echo)
		ring->echo(ring->echo_user, level, line);
	Aeron_MutexUnlock(ring->mutex);
}

/* SDL's log, which the flight model libraries write to. */
static void sdl_output(void* user, int category, SDL_LogPriority priority, const char* message) {
	(void)user;
	(void)category;
	ToolLog_Write(priority >= SDL_LOG_PRIORITY_ERROR  ? TOOL_LOG_ERROR
				  : priority == SDL_LOG_PRIORITY_WARN ? TOOL_LOG_WARNING
													  : TOOL_LOG_INFO,
				  "%s", message);
}

void ImportLog_Open(XwaModImportProgress* progress, const XwaModImportOptions* options) {
	XwaModImportLogRing* ring = progress->log;
	ring->progress = progress;
	ring->echo = options->echo;
	ring->echo_user = options->echo_user;
	SDL_GetLogOutputFunction(&ring->sdl_output, &ring->sdl_user);
	SDL_SetLogOutputFunction(sdl_output, NULL);
	ToolLog_SetSink(sink, ring);
}

bool ImportLog_SetFile(XwaModImportProgress* progress, const char* path) {
	XwaModImportLogRing* ring = progress->log;
	FILE* file = fopen(path, "w");
	Aeron_MutexLock(ring->mutex);
	ring->file = file;
	Aeron_MutexUnlock(ring->mutex);
	return file != NULL;
}

void ImportLog_Close(XwaModImportProgress* progress) {
	XwaModImportLogRing* ring = progress->log;
	ToolLog_SetSink(NULL, NULL);
	SDL_SetLogOutputFunction(ring->sdl_output, ring->sdl_user);
	Aeron_MutexLock(ring->mutex);
	if (ring->file)
		fclose(ring->file);
	ring->file = NULL;
	ring->echo = NULL;
	Aeron_MutexUnlock(ring->mutex);
}

void XwaModImport_ReadLog(XwaModImportProgress* progress, unsigned long long* next, ToolLogSink fn,
						  void* user) {
	XwaModImportLogRing* ring = progress->log;
	Aeron_MutexLock(ring->mutex);
	unsigned long long line = *next;
	if (ring->count > RING_LINES && line < ring->count - RING_LINES)
		line = ring->count - RING_LINES;
	for (; line < ring->count; ++line) {
		const size_t slot = (size_t)(line % RING_LINES);
		if (ring->lines[slot])
			fn(user, ring->levels[slot], ring->lines[slot]);
	}
	*next = ring->count;
	Aeron_MutexUnlock(ring->mutex);
}
