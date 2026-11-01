#ifndef XWA_MOD_IMPORT_INTERNAL_H
#define XWA_MOD_IMPORT_INTERNAL_H

/* What the parts of the import engine share. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"
#include "import.h"
#include "util.h"

#define IMPORT_PATH_CAP 4096
#define IMPORT_KEY_CAP 256
/* How the messages of the effect library builder start. */
#define IMPORT_LIBRARY_TAG "[aeron_gltf_cook] library"

/* One import. */
typedef struct ImportContext {
	const XwaModImportOptions* options;
	XwaModImportProgress* progress;
	char work[IMPORT_PATH_CAP]; /* <mod>/.import */
	char game_exe[IMPORT_PATH_CAP];
	int cores;
	uint64_t phase_start; /* in microseconds */
} ImportContext;

/* Whether the import was cancelled. */
bool Import_Cancelled(ImportContext* ctx);

/* Whether the import should stop: it was cancelled, or something failed. */
bool Import_ShouldStop(ImportContext* ctx);

/* Starts a phase with `total` items (0: no count). */
void Import_BeginPhase(ImportContext* ctx, XwaModImportPhase phase, int total);

/* Counts `items` more items done, and sets how far the phase is, from 0 to
 * 1000. A negative `weight` takes it from the item count. */
void Import_Advance(ImportContext* ctx, int items, int weight);

/* Counts a failure and logs it as an error. The import stops at the next
 * check, as the mod would be broken. The text is also listed in the summary.
 * Only the thread that runs the import calls it. */
UTIL_PRINTF(2, 3) void Import_Failed(ImportContext* ctx, const char* format, ...);

/* Lists a failure that a worker already counted and logged. */
UTIL_PRINTF(2, 3) void Import_ListFailed(ImportContext* ctx, const char* format, ...);

/* Copies file `from` to `to`, replacing it. Reports a failure. */
bool Import_CopyFile(const char* from, const char* to);

/* Moves `from` to `to`, which must not exist. */
bool Import_Rename(const char* from, const char* to);

/* Finds the entry `name` of folder `dir` in any case. Returns false when the
 * folder has no such file (`want_dir` false) or folder (`want_dir` true). */
bool Import_FindNoCase(const char* dir, const char* name, bool want_dir, char* out, size_t cap);

/* ---- The log ------------------------------------------------------------ */

XwaModImportLogRing* ImportLog_CreateRing(void);
void ImportLog_FreeRing(XwaModImportLogRing* ring);

/* Sends the tool log and SDL's log to the ring and to the echo callback of
 * the options. */
void ImportLog_Open(XwaModImportProgress* progress, const XwaModImportOptions* options);
/* Also writes the log to file `path`. */
bool ImportLog_SetFile(XwaModImportProgress* progress, const char* path);
void ImportLog_Close(XwaModImportProgress* progress);

/* ---- The authoring folder ----------------------------------------------- */

/* The manifest of an authoring folder, import.json. For each entry that the
 * importer wrote (models/<key>, effects/<library>, effects/effects.yaml), it
 * lists the entry's files with their size and modification time. */
typedef struct ImportAuthoring {
	char folder[IMPORT_PATH_CAP];
	cJSON* entries; /* rel -> { file: [size, time in microseconds] } */
} ImportAuthoring;

typedef enum ImportAuthoringState {
	IMPORT_AUTHORING_MISSING,  /* the entry isn't there: import it */
	IMPORT_AUTHORING_IMPORTED, /* as the importer wrote it: replace it */
	IMPORT_AUTHORING_EDITED,   /* changed since the importer wrote it: keep it */
	IMPORT_AUTHORING_AUTHORED, /* the importer didn't write it: keep it */
} ImportAuthoringState;

/* Reads <folder>/import.json, or starts an empty manifest. */
bool ImportAuthoring_Load(ImportAuthoring* authoring, const char* folder);
void ImportAuthoring_Free(ImportAuthoring* authoring);

/* The state of entry `rel`. Only reads the manifest, so workers can call it
 * at the same time. */
ImportAuthoringState ImportAuthoring_State(const ImportAuthoring* authoring, const char* rel);

/* Replaces entry `rel` with the file or folder `staged`, and returns the list
 * of its files for ImportAuthoring_Set. Returns NULL on failure, reported. */
cJSON* ImportAuthoring_Place(const ImportAuthoring* authoring, const char* staged, const char* rel);

/* Records the files of entry `rel`, which takes `files`. */
void ImportAuthoring_Set(ImportAuthoring* authoring, const char* rel, cJSON* files);

/* Writes <folder>/import.json. */
bool ImportAuthoring_Save(const ImportAuthoring* authoring);

/* Whether a file is left out of an entry: the folder files of the OS, and
 * the swap, lock and backup files of editors. */
bool ImportAuthoring_Ignored(const char* name);

/* ---- The phases --------------------------------------------------------- */

/* A model to import. */
typedef struct ImportModel {
	char key[IMPORT_KEY_CAP];  /* the lower-case name of the model */
	char base[IMPORT_KEY_CAP]; /* the OPT's name, without its extension */
	char* opt;                 /* NULL for a model that only the authoring folder has */
	long long size;            /* the OPT's size */
	bool cooked;
	ImportAuthoringState state;
	cJSON* files; /* the files that the worker placed in the authoring folder */
	char* gltf;   /* the .gltf that the material check reads */
	char* failed; /* why it failed, or NULL */
	double seconds;
} ImportModel;

/* Lists the models of the installation and, with an authoring folder, the
 * other models there, sorted by key. Stops the import when it returns
 * false. */
bool ImportModels_List(ImportContext* ctx, ImportModel** models, size_t* count);
void ImportModels_Free(ImportModel* models, size_t count);

/* Converts and cooks the models on the workers. Returns false when the
 * import has to stop. */
bool ImportModels_Run(ImportContext* ctx, ImportModel* models, size_t count,
					  const ImportAuthoring* authoring);

/* Makes the effect libraries that the models' .gltf files `gltfs` name in
 * memory, and builds them into folder `out`, on worker threads. Sets the
 * phase's total to the libraries and `checks` more items. Failures are
 * reported. */
void ImportEffects_Build(ImportContext* ctx, const char* const* gltfs, size_t count, const char* out,
						 size_t checks);

/* Copies the videos, sounds, movies and fonts that the mod's files name. */
void ImportCopies_Run(ImportContext* ctx);

#endif /* XWA_MOD_IMPORT_INTERNAL_H */
