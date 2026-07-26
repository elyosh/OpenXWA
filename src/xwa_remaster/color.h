#ifndef XWA_REMASTER_COLOR_H
#define XWA_REMASTER_COLOR_H

#include <math.h>

/* Normalized sRGB-encoded channel -> linear-light channel. */
static inline float XwaRemaster_SrgbToLinear(float value) {
	return value <= 0.04045f ? value / 12.92f : powf((value + 0.055f) / 1.055f, 2.4f);
}

#endif
