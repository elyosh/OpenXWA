#ifndef XWA_RUNTIME_MODERN_SUN_OPTIONS_H
#define XWA_RUNTIME_MODERN_SUN_OPTIONS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The sun options: the procedural sun and its lens flare. They come from
 * three places, and each one overrides the one before it:
 *   1. the `suns` section of the shipped remaster/config.yaml, which has
 *      every value, with both effects off;
 *   2. the `suns` section of the mod's flight/flight.yaml:
 *
 *        suns:
 *          procedural_sun: true
 *          procedural_flare: true
 *          flare_intensity: 3
 *
 *   3. the `suns` section of the player's config.yaml.
 * A key that a file leaves out keeps the value before it. The keys that
 * describe the look rather than a choice, such as brightness, aren't
 * read from the player's file. */
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

/* A key of a `suns` section. A switch is an int set from true or false. A
   number is a float from `min` to `max`. `offset` is the field's offset in
   XwaModernSunOptions. `player` is 0 for a key that the player's file can't
   set. */
typedef struct XwaModernSunOptionKey {
	const char* name;
	int is_switch;
	int player;
	float min;
	float max;
	size_t offset;
} XwaModernSunOptionKey;

/* The keys, in the order of the shipped file. In an override mask, bit `i`
   stands for key `i`. */
const XwaModernSunOptionKey* XwaModernSunOptions_Keys(size_t* count);

/* `shipped` holds the shipped values. `player` holds the player's values for
   the keys that `override_mask` has. */
void XwaModernSunOptions_Configure(const XwaModernSunOptions* shipped, const XwaModernSunOptions* player,
								   unsigned int override_mask);

/* Reads the `suns` section of the mod's flight/flight.yaml, which replaces
   the shipped values. Called once at startup. */
void XwaModernSunOptions_LoadMod(void);

/* The values in use: the mod's, or the shipped ones without a mod, with the
   player's values for the keys the player has set. */
void XwaModernSunOptions_Get(XwaModernSunOptions* out);

#ifdef __cplusplus
}
#endif

#endif
