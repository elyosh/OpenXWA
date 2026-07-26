#ifndef XWA_REMASTER_WORLD_H
#define XWA_REMASTER_WORLD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convert only after exact integer subtraction. int64 intermediates cover the
 * complete signed-int32 world range without overflow. */
void XwaRemasterWorld_LocalI32(const int32_t origin[3], const int32_t world[3], float out[3]);
void XwaRemasterWorld_DeltaI32(const int32_t a[3], const int32_t b[3], float out_a_minus_b[3]);

/* A precise point represented by an integer world base plus a
 * fractional local offset. */
void XwaRemasterWorld_LocalPoint(const int32_t origin[3], const int32_t base[3], const float offset[3],
								 float out[3]);

#ifdef __cplusplus
}
#endif

#endif
