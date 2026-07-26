#ifndef XWA_UTIL_BYTE_ORDER_H
#define XWA_UTIL_BYTE_ORDER_H

#include <stdint.h>

static inline uint16_t ByteOrder_ReadU16Le(const void* data) {
	const unsigned char* bytes = (const unsigned char*)data;

	return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
}

static inline uint32_t ByteOrder_ReadU32Le(const void* data) {
	const unsigned char* bytes = (const unsigned char*)data;

	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
		   ((uint32_t)bytes[3] << 24);
}

static inline int32_t ByteOrder_ReadI32Le(const void* data) { return (int32_t)ByteOrder_ReadU32Le(data); }

static inline void ByteOrder_WriteU16Le(void* data, uint16_t value) {
	unsigned char* bytes = (unsigned char*)data;

	bytes[0] = (unsigned char)(value & 0xffu);
	bytes[1] = (unsigned char)(value >> 8);
}

static inline void ByteOrder_WriteU32Le(void* data, uint32_t value) {
	unsigned char* bytes = (unsigned char*)data;

	bytes[0] = (unsigned char)(value & 0xffu);
	bytes[1] = (unsigned char)((value >> 8) & 0xffu);
	bytes[2] = (unsigned char)((value >> 16) & 0xffu);
	bytes[3] = (unsigned char)((value >> 24) & 0xffu);
}

#endif
