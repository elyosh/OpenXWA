#ifndef XWA_REMASTER_ENVIRONMENT_H
#define XWA_REMASTER_ENVIRONMENT_H

/*
 * HD authored environments: XWA mapping and per-frame selection.
 *
 * The manifest (environment_maps.manifest, under the remaster bake root)
 * holds Aeron's shared `packages:` map and an `xwa:` section that maps the
 * hangar and each (mission path, region) to a package reference. A mission
 * or region without an entry keeps the classic HD path.
 *
 * The selection is resolved from snapshot state alone, once per flight
 * frame, before any sky, backdrop, directional or lens-flare decision. It
 * never loads files or allocates.
 */

#include <stddef.h>
#include <stdint.h>

#include "aeron/config_file.h"
#include "aeron/scene/environment.h"

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
} XwaEnvironmentFallback;

typedef struct XwaEnvironmentSelection {
	XwaEnvironmentMode mode;
	XwaEnvironmentFallback fallback;
	/* Hangar or region reference; NULL when the mode uses no package. */
	const AeronEnvironmentReference* reference;
	uint8_t region;
} XwaEnvironmentSelection;

#define XWA_ENV_MISSION_KEY_MAX 128

/* Reads environment_maps.* from the remaster configuration and, when
 * enabled, parses the required manifest. Returns 0 on any error, logged
 * with the source path and YAML line. */
int XwaRemasterEnvironment_Init(AeronVfs* vfs, const AeronConfigFile* config, const char* config_path);
void XwaRemasterEnvironment_Shutdown(void);

int XwaRemasterEnvironment_Enabled(void);

/* Normalizes a mission path into a lookup key: forward slashes, lower-case
 * ASCII, empty and "." components dropped, ".." rejected, relative
 * directories kept. Returns 0 when the path is empty, rejected or does not
 * fit `size`. */
int XwaRemasterEnvironment_NormalizeMissionKey(const char* path, char* out, size_t size);

/* Resolves the environment selection for one flight frame. Never loads or
 * allocates; logs when the selection changes. */
void XwaRemasterEnvironment_Resolve(const struct XwaSnapshot* snap, XwaEnvironmentSelection* out);

const char* XwaRemasterEnvironment_ModeName(XwaEnvironmentMode mode);
const char* XwaRemasterEnvironment_FallbackName(XwaEnvironmentFallback fallback);

#ifdef __cplusplus
}
#endif

#endif
