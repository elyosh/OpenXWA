#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aeron/log.h"
#include "aeron/sync.h"
#include "import_internal.h"

#define RING_LINES 2000
#define ERROR_LINES 100

struct XwaModImportLogRing {
	AeronMutex* mutex;
	FILE* file; /* import.log */
	char* lines[RING_LINES];
	ToolLogLevel levels[RING_LINES];
	unsigned long long count; /* lines written so far */
	XwaModImportProgress* progress;
	ToolLogSink echo;
	void* echo_user;
	/* The import thread's log sink before the import. */
	AeronLogSink previous_sink;
	void* previous_sink_user;
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
	XwaModImportProgress* progress = ring->progress;
	if (level == TOOL_LOG_ERROR && progress->error_line_count < ERROR_LINES) {
		char** grown = realloc(progress->error_lines, (progress->error_line_count + 1) * sizeof *grown);
		if (grown) {
			progress->error_lines = grown;
			if ((grown[progress->error_line_count] = StrDup(line)))
				progress->error_line_count++;
		}
	}
	if (ring->echo)
		ring->echo(ring->echo_user, level, line);
	Aeron_MutexUnlock(ring->mutex);
}

/* Gets the lines that the import's threads log through SDL. The flight
   model code, for example, calls SDL_Log directly. The worker threads that
   the import starts get this sink too. It is called under SDL's log lock,
   and sink() then takes the ring's mutex, so code that holds that mutex must
   not log. */
static void thread_sink(void* user, AeronLogLevel level, const char* message) {
	(void)user;
	ToolLog_Write(level >= AERON_LOG_ERROR  ? TOOL_LOG_ERROR
				  : level == AERON_LOG_WARN ? TOOL_LOG_WARNING
											: TOOL_LOG_INFO,
				  "%s", message);
}

void ImportLog_Open(XwaModImportProgress* progress, const XwaModImportOptions* options) {
	XwaModImportLogRing* ring = progress->log;
	ring->progress = progress;
	ring->echo = options->echo;
	ring->echo_user = options->echo_user;
	ToolLog_SetSink(sink, ring);
	Aeron_LogGetThreadSink(&ring->previous_sink, &ring->previous_sink_user);
	if (!Aeron_LogSetThreadSink(thread_sink, NULL))
		ToolLog_Write(TOOL_LOG_WARNING, "The engine's messages can't be added to this log.");
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
	Aeron_LogSetThreadSink(ring->previous_sink, ring->previous_sink_user);
	ToolLog_SetSink(NULL, NULL);
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
