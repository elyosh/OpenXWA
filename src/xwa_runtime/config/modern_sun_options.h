#ifndef XWA_RUNTIME_MODERN_SUN_OPTIONS_H
#define XWA_RUNTIME_MODERN_SUN_OPTIONS_H

#include "xwa_runtime/config/option_section.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The sun options: the procedural sun and its lens flare. They are read
 * from the `suns` section of the shipped remaster/config.yaml, the mod's
 * flight/flight.yaml and the player's config.yaml (option_section.h). The
 * shipped file turns both effects off. The player's file can't set the keys
 * that describe the look, such as brightness. The HD renderer reads the
 * flare's shape, `flare_elements`, from the shipped settings and the mod's. */
typedef struct XwaModernSunOptions {
	/* Backdrop suns draw a procedural disc and corona in place of their
	   image. */
	int procedural_sun;
	/* The sun that casts the shadows draws a procedural lens flare in place
	   of the classic image train. */
	int procedural_flare;
	/* The flare's strength. */
	float flare_intensity;
	/* The brightness of the sun's disc and rays, as a multiple of the
	   radiance of a white texel of a classic image, like the emissive
	   strengths of explosions and engine glows. */
	float brightness;
	/* The light added in the sun's core, as a multiple of the disc's
	   radiance. It makes the sun glow through the bloom. */
	float core_brightness;
} XwaModernSunOptions;

XwaOptionSection* XwaModernSunOptions_Section(void);
/* The values in use. */
void XwaModernSunOptions_Get(XwaModernSunOptions* out);

#ifdef __cplusplus
}
#endif

#endif
