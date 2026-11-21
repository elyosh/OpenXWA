#ifndef XWA_REMASTER_SCENE_LIGHTS_H
#define XWA_REMASTER_SCENE_LIGHTS_H

/* The directional lights of an HD scene, as illuminance in W/m^2.
 *
 * The snapshot keeps the game's directional lights as the game has them
 * (XwaDirLight): a colour and a brightness with no unit. The HD renderer
 * reads them once, with XwaRemaster_ReadGameDirLights, which converts them
 * into illuminance with the classic light calibration. Lights that OpenXWA
 * defines itself (an environment's lights, the hangar's ceiling light, the
 * hyperspace key light) are authored in W/m^2 and are used as they are. */

#include <stdint.h>

#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaRemasterDirLight {
	float world_dir[3]; /* from the surface toward the light, world space, unit */
	float color[3];     /* illuminance in W/m^2, per channel */
	/* The backdrop the game made the light from: its object type and its
	 * position in the snapshot's backdrops, or 0 and -1. */
	uint16_t source_backdrop_object_type;
	int16_t source_backdrop_index;
} XwaRemasterDirLight;

/* The classic light calibration (remaster/config.yaml
 * classic_lighting.illuminance): the illuminance in W/m^2 of a classic light
 * of brightness 1. It converts every classic light, directional and point.
 * The debug inspector changes it; Configure also sets the value the
 * inspector resets to. */
float XwaRemaster_ClassicLightIlluminance(void);
void XwaRemaster_SetClassicLightIlluminance(float illuminance);
void XwaRemaster_ConfigureClassicLightIlluminance(float illuminance);
float XwaRemaster_ClassicLightIlluminanceDefault(void);

/* The radiance in W/m^2/sr of a classic image value of 1
 * (classic_lighting.image_radiance): a backdrop texel, an emissive texel, a
 * cockpit screen. These images have no unit. The value doesn't depend on the
 * light calibration, so the lit models can be brighter than the images
 * without making the images too bright. The debug inspector changes it;
 * Configure also sets the value the inspector resets to. */
float XwaRemaster_ClassicRadiance(void);
void XwaRemaster_SetClassicRadiance(float radiance);
void XwaRemaster_ConfigureClassicRadiance(float radiance);
float XwaRemaster_ClassicRadianceDefault(void);

/* Reads the game's directional lights as illuminance into `out`, which
 * holds XWA_SNAP_MAX_DIR_LIGHTS lights, and returns their count.
 *
 * The colour is decoded from sRGB, and so is a brightness up to 1, then they
 * are multiplied by the classic light calibration. The mission authors
 * chose these values on the classic renderer, which multiplies colour,
 * brightness and texture in gamma space. So a light of brightness 0.3 looks
 * like 0.07 of a light of brightness 1, not 0.3.
 *
 * A brightness above 1 multiplies the light as it is. The classic renderer
 * can't show more than full brightness, so these values only made more of
 * the surface fully lit there. Decoding them from sRGB too would make a
 * brightness of 15.5 (the original 1B2M5G) 640 times a brightness of 1. */
uint32_t XwaRemaster_ReadGameDirLights(const XwaDirLight* lights, uint32_t count, XwaRemasterDirLight* out);

#ifdef __cplusplus
}
#endif

#endif
