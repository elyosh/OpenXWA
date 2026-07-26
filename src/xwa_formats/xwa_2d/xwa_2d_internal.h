#ifndef XWA_2D_INTERNAL_H
#define XWA_2D_INTERNAL_H

#include "xwa_2d.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline uint16_t xwa2d_u16(const uint8_t* p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }

static inline int16_t xwa2d_i16(const uint8_t* p) { return (int16_t)xwa2d_u16(p); }

static inline uint32_t xwa2d_u32(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline int32_t xwa2d_i32(const uint8_t* p) { return (int32_t)xwa2d_u32(p); }

static inline int xwa2d_fail(char* error, size_t error_size, const char* format, ...) {
	if (error && error_size) {
		va_list args;
		va_start(args, format);
		vsnprintf(error, error_size, format, args);
		va_end(args);
	}
	return 0;
}

int xwa2d_append_frame(Xwa2dFrameSet* set, Xwa2dFrame* frame);
uint8_t* xwa2d_decode_indexed(const uint8_t* pixels, size_t pixel_size, int compressed, int width, int height,
							  int bounds_right, int bounds_bottom, const uint8_t palette[1024]);
int Xwa2d_DecodeIndexedFrame(const uint8_t* pixels, size_t pixel_size, int compressed, int width, int height,
							 int bounds_right, int bounds_bottom, const uint8_t palette[1024],
							 Xwa2dFrame* out, char* error, size_t error_size);
int Xwa2d_DecodeDatSprite(const uint8_t* record, size_t record_size, Xwa2dFrame* out, char* error,
						  size_t error_size);

#endif
