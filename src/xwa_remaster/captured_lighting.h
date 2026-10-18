#ifndef XWA_REMASTER_CAPTURED_LIGHTING_H
#define XWA_REMASTER_CAPTURED_LIGHTING_H

/*
 * Captured lighting builds a diffuse cube and a specular cube on the GPU
 * from what the region shows around the player.
 *
 * It applies in ordinary flight, in a region without an environment and in
 * a region whose sky keeps the mission's backdrops (`backdrops: keep`). It
 * does not apply in the hangar, in Death Star mode, in hyperspace or on the
 * map. The cubes show the sky and every backdrop that is not a sun, minus
 * those the region's rules hide. The sky is the environment's sky without
 * its overlay, or the starfield. Suns are left out because they
 * already light ships as directional lights.
 *
 * Each frame computes a signature of what the cubes would show: the
 * mission, the region, the sky, the hide and show rules, and each
 * backdrop's hidden flag. When it differs from the signature of the current
 * cubes, the cubes are rebuilt in the frame's command buffer, before the
 * main scene, and the frame uses them.
 *
 * A region that draws no backdrop other than suns has nothing to capture:
 * the cubes would show only the sky. It gets no cubes. A region whose sky
 * keeps the backdrops then uses its environment's baked cubes, and a region
 * without an environment keeps the game's lighting.
 */

#include <stdint.h>

#include "aeron/render.h"
#include "aeron/scene/scene3d.h"
#include "xwa_remaster/assets.h"
#include "xwa_remaster/environment.h"
#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The settings under environment_maps.captured_lighting in
 * remaster/config.yaml. */
typedef struct XwaRemasterCapturedLightingOptions {
	int enabled;
	int size;             /* capture and specular face size: 64, 128, 256 or 512 */
	int specular_samples; /* GGX samples per texel: 1 to 1024 */
	/* These strengths apply in regions without an environment. */
	float diffuse_strength;
	float specular_strength;
	/* When set, ambient_fill replaces the game's ambient in regions without
	 * an environment. */
	int has_ambient_fill;
	float ambient_fill[3];
} XwaRemasterCapturedLightingOptions;

void XwaRemasterCapturedLighting_Configure(const XwaRemasterCapturedLightingOptions* options);
/* Releases the face scene and the cubes. */
void XwaRemasterCapturedLighting_Shutdown(void);

/* Builds the cubes into `cmd` when captured lighting applies to the frame
 * and its signature differs from that of the current cubes. Call after the
 * frame's sprite atlas uploads and before the main scene render. */
void XwaRemasterCapturedLighting_Update(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
										XwaRemasterAssets* assets, const XwaEnvironmentSelection* selection);

/* Gets the cubes to bind this frame, with their strengths and world-to-cube
 * matrix. Returns 0 when captured lighting does not apply, or when there are
 * no cubes for the frame's signature. `ambient_fill` gets the configured
 * fill in a region without an environment, or NULL to keep the ambient. */
int XwaRemasterCapturedLighting_Environment(const XwaSnapshot* snap, const XwaEnvironmentSelection* selection,
											AeronScenePbrEnvironment* out, const float** ambient_fill);

/* ---- debug tool --------------------------------------------------------- */

typedef struct XwaRemasterCapturedLightingInfo {
	int enabled;
	int size;
	int applies;            /* to the last frame outside hyperspace and the map */
	int ready;              /* the cubes exist */
	int current;            /* they match that frame's signature */
	int nothing_to_capture; /* that frame's region draws no backdrop other than suns */
	int failed;             /* creating the resources or a build failed */
	uint64_t signature;     /* of the cubes */
	uint32_t build_count;
} XwaRemasterCapturedLightingInfo;

void XwaRemasterCapturedLighting_Info(XwaRemasterCapturedLightingInfo* out);

#ifdef __cplusplus
}
#endif

#endif
