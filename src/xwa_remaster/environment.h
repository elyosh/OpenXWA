#ifndef XWA_REMASTER_ENVIRONMENT_H
#define XWA_REMASTER_ENVIRONMENT_H

/*
 * HD authored environments: XWA mapping and per-frame selection.
 *
 * The manifest (host option environment_maps.manifest, under the remaster
 * bake root)
 * holds Aeron's shared `packages:` map and an `xwa:` section that maps the
 * hangar and each (mission path, region) to a package reference. A mission
 * or region without an entry keeps the classic HD path.
 *
 * Packages are prepared through Aeron's environment library: the hangar
 * package during process asset preparation, and the hangar plus every
 * region of a mission when the snapshot's normalized mission key changes,
 * at the HD asset synchronization point. A region becomes authored only once
 * its package is prepared and available.
 *
 * The selection is resolved from snapshot state alone, once per flight
 * frame, before any sky, backdrop, directional or lens-flare decision. It
 * never loads files or allocates.
 */

#include <stddef.h>
#include <stdint.h>

#include "aeron/render.h"
#include "aeron/scene/environment.h"
#include "aeron/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct XwaSnapshot;

typedef enum XwaEnvironmentMode {
	XWA_ENV_MODE_DISABLED,   /* feature disabled: current sky and ambient */
	XWA_ENV_MODE_HYPERSPACE, /* dynamic hyperspace diffuse environment */
	XWA_ENV_MODE_DEATH_STAR, /* current behaviour, no authored environment */
	XWA_ENV_MODE_HANGAR,     /* hangar lighting package, or current ambient without one */
	XWA_ENV_MODE_AUTHORED,   /* complete authored package for the region */
	XWA_ENV_MODE_CLASSIC,    /* complete classic path */
} XwaEnvironmentMode;

typedef enum XwaEnvironmentFallback {
	XWA_ENV_FALLBACK_NONE,
	XWA_ENV_FALLBACK_NO_MISSION,       /* snapshot carries no mission path */
	XWA_ENV_FALLBACK_INVALID_MISSION,  /* mission path cannot be normalized */
	XWA_ENV_FALLBACK_NO_MISSION_ENTRY, /* mission not in the mapping */
	XWA_ENV_FALLBACK_NO_REGION_ENTRY,  /* region not in the mission's mapping */
	XWA_ENV_FALLBACK_NO_HANGAR_ENTRY,  /* no hangar package in the mapping */
	XWA_ENV_FALLBACK_NOT_READY,        /* mapped package not prepared or unavailable */
	XWA_ENV_FALLBACK_AUTHORED_OFF,     /* authored environments switched off in the debug tool */
} XwaEnvironmentFallback;

typedef struct XwaEnvironmentSelection {
	XwaEnvironmentMode mode;
	XwaEnvironmentFallback fallback;
	/* Hangar or region reference; NULL when the mode uses no package. */
	const AeronEnvironmentReference* reference;
	/* Prepared package for the reference; NULL unless the mode uses one and
	 * it is available. */
	const AeronEnvironmentPackage* package;
	uint8_t region;
} XwaEnvironmentSelection;

#define XWA_ENV_MISSION_KEY_MAX 128

/* Sets the host options environment_maps.enabled, .manifest and
 * .sun_max_radiance (shipped configuration, optionally overridden by the
 * user configuration). The manifest path is copied. Call before
 * XwaRemasterEnvironment_Init. */
void XwaRemasterEnvironment_Configure(int enabled, const char* manifest, float sun_max_radiance);

/* When enabled, parses the required manifest. Returns 0 on any error,
 * logged with the source path and YAML line. */
int XwaRemasterEnvironment_Init(AeronVfs* vfs);
void XwaRemasterEnvironment_Shutdown(void);

int XwaRemasterEnvironment_Enabled(void);

/* Debug switch for authored environments (on by default): off, the hangar
 * and flight regions use the classic path, for visual comparison and for
 * profiling the environments' cost. Packages stay prepared, so toggling it
 * loads nothing. */
void XwaRemasterEnvironment_SetAuthoredEnvironments(int enabled);
int XwaRemasterEnvironment_AuthoredEnvironments(void);

/* Creates the package library and prepares the hangar package. Failures are
 * logged and leave the classic path; returns 0 only on invalid arguments. */
int XwaRemasterEnvironment_PrepareProcess(AeronCommandBuffer* cmd);

/* Nonzero when the snapshot's mission key differs from the prepared one, or
 * a debug reload is pending. */
int XwaRemasterEnvironment_NeedsPrepare(const struct XwaSnapshot* snap);
/* Prepares the hangar and the mission's regions (or services a reload) with
 * `cmd`. Failures are logged; the affected regions stay classic. */
void XwaRemasterEnvironment_Prepare(AeronCommandBuffer* cmd, const struct XwaSnapshot* snap);

/* Normalizes a mission path into a lookup key: forward slashes, lower-case
 * ASCII, empty and "." components dropped, ".." rejected, relative
 * directories kept. Returns 0 when the path is empty, rejected or does not
 * fit `size`. */
int XwaRemasterEnvironment_NormalizeMissionKey(const char* path, char* out, size_t size);

/* Resolves the environment selection for one flight frame. Never loads or
 * allocates; logs when the selection changes. */
void XwaRemasterEnvironment_Resolve(const struct XwaSnapshot* snap, XwaEnvironmentSelection* out);

/* The most recently resolved selection and its mission path; returns 0
 * before the first resolution. */
int XwaRemasterEnvironment_LastSelection(XwaEnvironmentSelection* out, char* mission, size_t mission_size);

/* The normalized mission key whose packages are prepared; "" when none. */
const char* XwaRemasterEnvironment_PreparedMissionKey(void);

const char* XwaRemasterEnvironment_ModeName(XwaEnvironmentMode mode);
const char* XwaRemasterEnvironment_FallbackName(XwaEnvironmentFallback fallback);

#ifdef __cplusplus
}
#endif

#endif
