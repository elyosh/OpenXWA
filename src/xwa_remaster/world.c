#include "xwa_remaster/world.h"

void XwaRemasterWorld_LocalI32(const int32_t origin[3], const int32_t world[3], float out[3]) {
	for (int i = 0; i < 3; i++)
		out[i] = (float)((int64_t)world[i] - (int64_t)origin[i]);
}

void XwaRemasterWorld_DeltaI32(const int32_t a[3], const int32_t b[3], float out_a_minus_b[3]) {
	for (int i = 0; i < 3; i++)
		out_a_minus_b[i] = (float)((int64_t)a[i] - (int64_t)b[i]);
}

void XwaRemasterWorld_LocalPoint(const int32_t origin[3], const int32_t base[3], const float offset[3],
								 float out[3]) {
	for (int i = 0; i < 3; i++)
		out[i] = (float)((double)((int64_t)base[i] - (int64_t)origin[i]) + (double)offset[i]);
}
