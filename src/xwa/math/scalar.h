#ifndef XWA_MATH_SCALAR_H
#define XWA_MATH_SCALAR_H

#include <stdint.h>

static inline int Xwa_Abs32(int value) {
	int32_t signMask;

	signMask = (int32_t)value >> 31;
	return (int)(((uint32_t)value ^ (uint32_t)signMask) - (uint32_t)signMask);
}

static inline int Xwa_IsProjectedCoordSigned16(int value) {
	int32_t highWord;

	highWord = (int32_t)((uint32_t)value & 0xffff0000u);
	return highWord <= 0 && highWord >= -65536;
}

// FUNCTION: XWA 0x441EB0
void Math_SetFpuSinglePrecisionMode(void);

#endif
