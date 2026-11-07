#ifndef XWA_MOD_IMPORT_H
#define XWA_MOD_IMPORT_H

/*
 * The mod importer's engine. It imports an XWA Upgrade installation, such as
 * XWAU or a TFTC release, into an OpenXWA mod folder, in one process.
 *
 * The import runs in phases on the calling thread. The models are converted
 * and cooked by a pool of worker threads. Every message of the tool libraries
 * goes to <mod>/import.log, to the log lines of the progress, and to the
 * `echo` callback when there is one.
 *
 * The import stops at the first failure. The importer writes a mod.yaml with
 * the mod format version (xwa_app/mod_format.h) and `complete: false` at the
 * start, and sets `complete: true` when every phase has run. The game doesn't
 * use a mod that isn't complete. The importer only clears a folder that
 * isn't empty when its mod.yaml comes from the importer and `replace` is
 * set. The work files go in <mod>/.import/, which is deleted at the end.
 *
 * With an authoring folder ("Keep authoring files"), the glTF models and the
 * effect library sources are kept there for editing, and the mod is cooked
 * from them. An entry that the user edited since the last import, or that
 * the importer didn't write, is kept as it is.
 */

#include <stdbool.h>
#include <stddef.h>

#include "tool_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaModImportOptions {
	const char* source;      /* the XWA Upgrade installation */
	const char* game;        /* the original game's data folder */
	const char* mod;         /* the mod folder */
	const char* name;        /* the mod's name, for mod.yaml */
	const char* authoring;   /* the authoring folder, or NULL */
	const char* resources;   /* OpenXWA's resource folder, which holds assets/fonts */
	bool replace;            /* clear a mod folder that the importer made */
	int jobs;                /* models converted at once; 0: half the CPU cores */
	int zstd_level;          /* of the models' textures; 0: the cook's default */
	const char* const* only; /* only import these models (lower-case keys) */
	size_t only_count;
	/* Receives each log line as it is written, from any thread. */
	ToolLogSink echo;
	void* echo_user;
} XwaModImportOptions;

typedef enum XwaModImportPhase {
	XWA_MOD_IMPORT_PREPARE,
	XWA_MOD_IMPORT_MODELS,
	XWA_MOD_IMPORT_EFFECTS,
	XWA_MOD_IMPORT_CRAFT,
	XWA_MOD_IMPORT_MISSIONS,
	XWA_MOD_IMPORT_SKYBOXES,
	XWA_MOD_IMPORT_HANGARS,
	XWA_MOD_IMPORT_FRONTEND,
	XWA_MOD_IMPORT_FLIGHT,
	XWA_MOD_IMPORT_COPIES,
	XWA_MOD_IMPORT_FINISH,
	XWA_MOD_IMPORT_PHASE_COUNT,
} XwaModImportPhase;

typedef enum XwaModImportResult {
	XWA_MOD_IMPORT_OK,        /* every phase ran with no failure */
	XWA_MOD_IMPORT_CANCELLED, /* stopped by `cancel` */
	XWA_MOD_IMPORT_STOPPED,   /* stopped by a failure */
} XwaModImportResult;

typedef struct XwaModImportLogRing XwaModImportLogRing;

/* The state of an import, shared with the threads that follow it. The ints
 * are read with Aeron_AtomicLoad. Set `cancel` with Aeron_AtomicStore to stop
 * the import. The rest is only valid once `finished` is set. */
typedef struct XwaModImportProgress {
	volatile int cancel;
	volatile int phase;  /* XwaModImportPhase */
	volatile int done;   /* items done in the phase */
	volatile int total;  /* items in the phase; 0 when it has no count */
	volatile int weight; /* how far the phase is, from 0 to 1000 */
	volatile int failures;
	volatile int warnings;
	volatile int errors;
	volatile int finished;
	volatile int result;                      /* XwaModImportResult */
	int phase_ms[XWA_MOD_IMPORT_PHASE_COUNT]; /* how long each phase took */
	char** failed;                            /* what failed, one line each */
	size_t failed_count;
	char** error_lines; /* the first error lines of the log, in order */
	size_t error_line_count;
	XwaModImportLogRing* log;
} XwaModImportProgress;

/* What XwaModImport_Detect finds in an installation. */
typedef struct XwaModImportInstall {
	char name[256]; /* the name of the mod that it holds */
	size_t models;  /* its .opt files in FlightModels */
} XwaModImportInstall;

typedef enum XwaModImportCheck {
	XWA_MOD_IMPORT_CHECK_OK,
	XWA_MOD_IMPORT_CHECK_REPLACE, /* the mod folder has a mod made by the importer, and `replace` isn't set */
	XWA_MOD_IMPORT_CHECK_INVALID,
} XwaModImportCheck;

/* Whether `source` is an XWA Upgrade installation: XWINGALLIANCE.EXE at its
 * top, FlightModels/ with at least one .opt, and Hooks.ini or
 * XWAUVersion.txt. Fills `info`, or writes the reason into `error`. */
bool XwaModImport_Detect(const char* source, XwaModImportInstall* info, char* error, size_t error_cap);

/* The folder of the mod `name` in the mods folder `mods`: <mods>/<name>, with
 * the name in lower case and other characters than letters, digits, '.' and
 * '-' replaced by '-'. */
bool XwaModImport_ModFolder(const char* mods, const char* name, char* out, size_t cap);

/* Checks the options before an import. Writes the reason into `error` when
 * it doesn't return XWA_MOD_IMPORT_CHECK_OK. */
XwaModImportCheck XwaModImport_Check(const XwaModImportOptions* options, char* error, size_t error_cap);

/* Sets up and frees a progress. */
bool XwaModImport_ProgressInit(XwaModImportProgress* progress);
void XwaModImport_ProgressFree(XwaModImportProgress* progress);

/* Runs the import. Sets `progress->finished` at the end, and returns the
 * result. */
XwaModImportResult XwaModImport_Run(const XwaModImportOptions* options, XwaModImportProgress* progress);

/* The overall progress, from 0 to 1000. */
int XwaModImport_Overall(XwaModImportProgress* progress);

/* The phase's name for a progress line, such as "Converting models". */
const char* XwaModImport_PhaseName(XwaModImportPhase phase);

/* Calls `fn` for each log line kept since line number `*next`, then sets
 * `*next` past the last one. The progress keeps the last 2,000 lines. Can be
 * called from any thread during the import. */
void XwaModImport_ReadLog(XwaModImportProgress* progress, unsigned long long* next, ToolLogSink fn,
						  void* user);

#ifdef __cplusplus
}
#endif

#endif /* XWA_MOD_IMPORT_H */
