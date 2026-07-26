#ifndef XWA_MATH_FIXED_H
#define XWA_MATH_FIXED_H

#include <stdint.h>

static inline int Xwa_Q15Mul(int a, int b) {
#ifndef XWA_MODERN
	int result;
	/* Match the original fixed-point macro: preserve edx and store the result before restoring it. */
	__asm {
		push edx
		mov eax, a
		imul b
		shrd eax, edx, 15
		mov result, eax
		pop edx
	}
	return result;
#else
	return (int)(((int64_t)a * (int64_t)b) >> 15);
#endif
}

static inline int Xwa_Q15MulReuseFirstSlot(int a, int b) {
#ifndef XWA_MODERN
	__asm {
		push edx
		mov eax, a
		imul b
		shrd eax, edx, 15
		mov a, eax
		pop edx
	}
	return a;
#else
	return (int)(((int64_t)a * (int64_t)b) >> 15);
#endif
}

static inline int Xwa_Q15MulInline(int a, int b) {
#ifndef XWA_MODERN
	int result;
	__asm {
		mov eax, a
		imul b
		shrd eax, edx, 15
		mov result, eax
	}
	return result;
#else
	return (int)(((int64_t)a * (int64_t)b) >> 15);
#endif
}

static inline int Xwa_Dot3Q15Inline(int rowX, int rowY, int rowZ, int x, int y, int z) {
#ifndef XWA_MODERN
	int result;
	__asm {
		mov eax, x
		imul rowX
		shrd eax, edx, 15
		mov ebx, eax
		mov eax, y
		imul rowY
		shrd eax, edx, 15
		add ebx, eax
		mov eax, z
		imul rowZ
		shrd eax, edx, 15
		add eax, ebx
		mov result, eax
	}
	return result;
#else
	return Xwa_Q15MulInline(rowX, x) + Xwa_Q15MulInline(rowY, y) + Xwa_Q15MulInline(rowZ, z);
#endif
}

static inline int Xwa_Dot3Q15ReuseXSlot(int rowX, int rowY, int rowZ, int x, int y, int z) {
#ifndef XWA_MODERN
	__asm {
		mov eax, x
		imul rowX
		shrd eax, edx, 15
		mov ebx, eax
		mov eax, y
		imul rowY
		shrd eax, edx, 15
		add ebx, eax
		mov eax, z
		imul rowZ
		shrd eax, edx, 15
		add eax, ebx
		mov x, eax
	}
	return x;
#else
	return Xwa_Q15MulInline(rowX, x) + Xwa_Q15MulInline(rowY, y) + Xwa_Q15MulInline(rowZ, z);
#endif
}

static inline int Xwa_SaturateQ30ToQ15(int64_t value) {
	if (value >= 0x40000000) {
		value = 0x3fffffff;
	}
	if (value <= -1073741824) {
		value = -1073676288;
	}
	return (int)(value >> 15);
}

static inline int Xwa_SaturateWrappedQ30ToQ15(int32_t value) {
	if (value >= 0x40000000) {
		value = 0x3fffffff;
	}
	if (value <= (int32_t)0xc0000000u) {
		value = (int32_t)0xc0010000u;
	}
	return (int)(value >> 15);
}

static inline int Xwa_WrappedMulAdd3Q15(int a0, int b0, int a1, int b1, int a2, int b2) {
	uint32_t acc;

	acc = (uint32_t)((int64_t)a0 * b0);
	acc += (uint32_t)((int64_t)a1 * b1);
	acc += (uint32_t)((int64_t)a2 * b2);
	return Xwa_SaturateWrappedQ30ToQ15((int32_t)acc);
}

static inline int Xwa_Dot3Q15Wrapped(int rowX, int rowY, int rowZ, int x, int y, int z) {
	return Xwa_WrappedMulAdd3Q15(rowX, x, rowY, y, rowZ, z);
}

#endif
