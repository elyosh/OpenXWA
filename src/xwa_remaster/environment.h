#ifndef XWA_REMASTER_ENVIRONMENT_H
#define XWA_REMASTER_ENVIRONMENT_H

/*
 * HD authored environments: XWA mapping and per-frame selection.
 *
 * The manifest (host option environment_maps.manifest, relative to the mod
 * folder) holds Aeron's shared `environments:` map and an `xwa:` section.
 * `xwa.missions` maps each (mission path, region) to an environment
 * reference, and the optional `xwa.default` is the reference of every
 * region without an entry. A region whose value is `none` has no
 * environment, even when there is a default.
 *
 * A region's environment replaces the ambient with its fill and image-based
 * lighting. If the environment has a sky (`radiance`), it also replaces the
 * classic sky, backdrops and backdrop flares. If it has
 * `directional_lights`, they replace the directional lights. A reference
 * with `backdrops: keep` draws the mission's backdrops over the sky instead,
 * except the hidden ones (XwaRemasterEnvironment_BackdropHidden). It keeps
 * the backdrop flares and lights. With `hidden_backdrop_lights: remove`, it
 * leaves out the lights of hidden backdrops that are not suns.
 * `backdrop_light_scale` multiplies the lights of backdrops that are not
 * suns (planets, nebulae), hidden or not, for missions made for a renderer
 * that shows these lights weaker. A mission or
 * region without an entry uses the default environment. Without a default,
 * and in Death Star mode, it uses the classic HD path (the Death Star
 * behaviour in Death Star mode). In a hangar, the selection uses the region
 * the player entered the hangar from, and the flight renderer uses only
 * the sky of that environment.
 *
 * Environments are prepared through Aeron's environment library at the HD
 * asset synchronization point. When the snapshot's normalized mission key
 * changes, the environments of all the mission's regions, the default
 * included, are prepared. A
 * region uses its environment only after it is prepared and available. A model's
 * probes are stored in its GLB (AeronSceneMesh.probes), and the flight
 * renderer places them on the objects that use that model.
 *
 * The selection is resolved from snapshot state alone, once per flight
 * frame, before any sky, backdrop, directional or lens-flare decision. It
 * never loads files or allocates.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aeron/render.h"
#include "aeron/scene/environment.h"
#include "aeron/scene/scene3d.h"
#include "aeron/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct XwaSnapshot;

typedef enum XwaEnvironmentMode {
	XWA_ENV_MODE_DISABLED,   /* feature disabled: current sky and ambient */
	XWA_ENV_MODE_HYPERSPACE, /* dynamic hyperspace diffuse environment */
	XWA_ENV_MODE_DEATH_STAR, /* Death Star region without an environment: current behaviour */
	XWA_ENV_MODE_AUTHORED,   /* the region's environment: its lighting, and its sky when it has one */
	XWA_ENV_MODE_CLASSIC,    /* complete classic path */
} XwaEnvironmentMode;

typedef enum XwaEnvironmentFallback {
	XWA_ENV_FALLBACK_NONE,
	XWA_ENV_FALLBACK_NO_MISSION,       /* snapshot carries no mission path */
	XWA_ENV_FALLBACK_INVALID_MISSION,  /* mission path cannot be normalized */
	XWA_ENV_FALLBACK_NO_MISSION_ENTRY, /* mission not in the mapping */
	XWA_ENV_FALLBACK_NO_REGION_ENTRY,  /* region not in the mission's mapping */
	XWA_ENV_FALLBACK_NOT_READY,        /* mapped environment not prepared or unavailable */
	XWA_ENV_FALLBACK_AUTHORED_OFF,     /* authored environments switched off in the debug tool */
	XWA_ENV_FALLBACK_REGION_NONE,      /* the region's value is `none` */
} XwaEnvironmentFallback;

/* What a sky does with the mission's backdrops (the reference's `backdrops`). */
typedef enum XwaEnvironmentBackdrops {
	XWA_ENV_BACKDROPS_REPLACE, /* the sky replaces the backdrops, their lights and their flares */
	XWA_ENV_BACKDROPS_KEEP,    /* the backdrops are drawn over the sky, except the hidden ones */
} XwaEnvironmentBackdrops;

#define XWA_ENV_BACKDROP_LIST_MAX 64

/* The backdrop settings of a reference. Images are `group << 16 | image id`
 * (DAT group and image). */
typedef struct XwaEnvironmentBackdropRule {
	XwaEnvironmentBackdrops mode;
	bool hide_all; /* hide_backdrops: all */
	bool show_all; /* show_backdrops: all */
	/* Set by hidden_backdrop_lights: remove. Hidden backdrops that are not
	 * suns then make no light. */
	bool remove_hidden_lights;
	/* backdrop_light_scale: multiplies the linear light of backdrops that are
	 * not suns. 1 unless set. */
	float light_scale;
	uint8_t hide_count;
	uint8_t show_count;
	uint32_t hide[XWA_ENV_BACKDROP_LIST_MAX];
	uint32_t show[XWA_ENV_BACKDROP_LIST_MAX];
} XwaEnvironmentBackdropRule;

typedef struct XwaEnvironmentSelection {
	XwaEnvironmentMode mode;
	XwaEnvironmentFallback fallback;
	/* Region reference; NULL when the mode uses no environment. */
	const AeronEnvironmentReference* reference;
	/* Prepared environment for the reference; NULL unless the mode uses one
	 * and it is available. */
	const AeronPreparedEnvironment* prepared;
	uint8_t region; /* flight_camera.mission_region */
	/* The reference is xwa.default: the region has no entry of its own. */
	bool is_default;
	/* The reference's backdrop settings; NULL without a reference. */
	const XwaEnvironmentBackdropRule* backdrops;
} XwaEnvironmentSelection;

#define XWA_ENV_MISSION_KEY_MAX 128

/* Sets the host options environment_maps.enabled, .manifest and
 * .sun_max_radiance (shipped configuration, optionally overridden by the
 * user configuration). The manifest path is copied. Call before
 * XwaRemasterEnvironment_Init. */
void XwaRemasterEnvironment_Configure(int enabled, const char* manifest, float sun_max_radiance);

/* When enabled, parses the manifest. Without a manifest there are no
 * authored environments, and every region uses the classic path. Returns 0
 * when the manifest is invalid, and logs the source path and YAML line. */
int XwaRemasterEnvironment_Init(AeronVfs* vfs);
void XwaRemasterEnvironment_Shutdown(void);

int XwaRemasterEnvironment_Enabled(void);

/* Debug switch for authored environments (on by default). When off, all
 * regions use the classic path, for visual comparison and for profiling
 * the cost of the environments. Environments stay prepared, so toggling it
 * loads nothing. */
void XwaRemasterEnvironment_SetAuthoredEnvironments(int enabled);
int XwaRemasterEnvironment_AuthoredEnvironments(void);

/* Creates the environment library. Failures are logged, and the regions then
 * use the classic path. Returns 0 only on invalid arguments. */
int XwaRemasterEnvironment_PrepareProcess(AeronCommandBuffer* cmd);

/* Nonzero when the snapshot's mission key differs from the prepared one, or
 * a debug reload is pending. */
int XwaRemasterEnvironment_NeedsPrepare(const struct XwaSnapshot* snap);
/* Prepares the mission's regions, or runs a pending reload, with `cmd`.
 * Failures are logged, and the affected regions use the classic path. */
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

/* The normalized mission key whose environments are prepared; "" when none. */
const char* XwaRemasterEnvironment_PreparedMissionKey(void);

/* Whether the selection draws a sky with `backdrops: keep`, with the
 * mission's backdrops drawn over it. */
bool XwaRemasterEnvironment_KeepsBackdrops(const XwaEnvironmentSelection* selection);

/* Whether backdrop image (`group`, `image`) is hidden under `rule` in
 * `keep` mode. An image is hidden when it is in `hide` or `hide_all` is
 * set, unless it is in `show` or `show_all` is set. */
bool XwaRemasterEnvironment_BackdropHidden(const XwaEnvironmentBackdropRule* rule, int group, int image);

const char* XwaRemasterEnvironment_ModeName(XwaEnvironmentMode mode);
const char* XwaRemasterEnvironment_FallbackName(XwaEnvironmentFallback fallback);

#ifdef __cplusplus
}
#endif

#endif
