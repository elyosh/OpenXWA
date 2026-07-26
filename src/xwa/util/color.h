#ifndef XWA_UTIL_COLOR_H
#define XWA_UTIL_COLOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RgbTriplet {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} RgbTriplet;

unsigned int Color_FindNearestRgbTripletIndex(const RgbTriplet* targetRgb, const RgbTriplet* palette,
											  unsigned int startIndex, unsigned int endIndex);

#ifdef __cplusplus
}
#endif

#endif
