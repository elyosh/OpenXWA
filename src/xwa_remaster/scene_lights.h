#ifndef XWA_REMASTER_SCENE_LIGHTS_H
#define XWA_REMASTER_SCENE_LIGHTS_H

/* The directional lights of an HD scene, in linear light.
 *
 * The snapshot keeps the game's directional lights as the game has them
 * (XwaDirLight): a colour and a brightness. The HD renderer reads them once,
 * with XwaRemaster_ReadGameDirLights, and works on linear lights from then
 * on. Lights that OpenXWA defines itself (an environment's lights, the
 * hangar's ceiling light, the hyperspace key light) are linear from the
 * start. */

#include <stdint.h>

#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaRemasterDirLight {
	float world_dir[3]; /* from the surface toward the light, world space, unit */
	/* Linear light. The game's white light of brightness 1 is 1; the scene
	 * scales it by the lighting calibration (sun_illuminance_scale). */
	float color[3];
	/* The backdrop the game made the light from: its object type and its
	 * position in the snapshot's backdrops, or 0 and -1. */
	uint16_t source_backdrop_object_type;
	int16_t source_backdrop_index;
} XwaRemasterDirLight;

/* Reads the game's directional lights as linear lights into `out`, which
 * holds XWA_SNAP_MAX_DIR_LIGHTS lights, and returns their count.
 *
 * A game light's colour times its brightness is decoded as one sRGB value.
 * The mission authors chose these values on the classic renderer, which
 * multiplies colour, brightness and texture in gamma space. So a light of
 * brightness 0.3 looks like 0.07 of a light of brightness 1, not 0.3. The
 * curve continues above 1. */
uint32_t XwaRemaster_ReadGameDirLights(const XwaDirLight* lights, uint32_t count, XwaRemasterDirLight* out);

#ifdef __cplusplus
}
#endif

#endif
