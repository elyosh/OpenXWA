#include "xwa_remaster/hud_cmd_math.h"

#include <math.h>

static void rows_to_quat(const float m[9], float q[4]) {
	const float trace = m[0] + m[4] + m[8];
	if (trace > 0.0f) {
		const float s = sqrtf(trace + 1.0f) * 2.0f;
		q[0] = 0.25f * s;
		q[1] = (m[7] - m[5]) / s;
		q[2] = (m[2] - m[6]) / s;
		q[3] = (m[3] - m[1]) / s;
	} else if (m[0] > m[4] && m[0] > m[8]) {
		const float s = sqrtf(1.0f + m[0] - m[4] - m[8]) * 2.0f;
		q[0] = (m[7] - m[5]) / s;
		q[1] = 0.25f * s;
		q[2] = (m[1] + m[3]) / s;
		q[3] = (m[2] + m[6]) / s;
	} else if (m[4] > m[8]) {
		const float s = sqrtf(1.0f + m[4] - m[0] - m[8]) * 2.0f;
		q[0] = (m[2] - m[6]) / s;
		q[1] = (m[1] + m[3]) / s;
		q[2] = 0.25f * s;
		q[3] = (m[5] + m[7]) / s;
	} else {
		const float s = sqrtf(1.0f + m[8] - m[0] - m[4]) * 2.0f;
		q[0] = (m[3] - m[1]) / s;
		q[1] = (m[2] + m[6]) / s;
		q[2] = (m[5] + m[7]) / s;
		q[3] = 0.25f * s;
	}
	const float length_squared = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
	if (length_squared > 0.0f) {
		const float inverse_length = 1.0f / sqrtf(length_squared);
		for (int i = 0; i < 4; i++)
			q[i] *= inverse_length;
	} else {
		q[0] = 1.0f;
		q[1] = q[2] = q[3] = 0.0f;
	}
}

void XwaRemasterHudCmdMath_OrthonormalRows(const int16_t rows_q15[9], float rows[9]) {
	float source[9];
	for (int i = 0; i < 9; i++)
		source[i] = (float)rows_q15[i] * (1.0f / 32768.0f);
	/* Hud_PointCamera's normal camera builder produces a proper rotation,
	 * but its docked/carried-target shortcut directly remaps target axes
	 * into a reflected world-to-eye basis. A quaternion can represent only
	 * det=+1 rotations. Temporarily flip row 2 for a reflected input, then
	 * restore it after quaternion orthonormalization; this removes Q15
	 * scale/skew without changing the classic basis handedness. */
	const float determinant = source[0] * (source[4] * source[8] - source[5] * source[7]) -
							  source[1] * (source[3] * source[8] - source[5] * source[6]) +
							  source[2] * (source[3] * source[7] - source[4] * source[6]);
	const int reflected = determinant < 0.0f;
	if (reflected) {
		source[6] = -source[6];
		source[7] = -source[7];
		source[8] = -source[8];
	}
	float q[4];
	rows_to_quat(source, q);
	const float w = q[0], x = q[1], y = q[2], z = q[3];
	const float xx = x * x, yy = y * y, zz = z * z;
	const float xy = x * y, xz = x * z, yz = y * z;
	const float wx = w * x, wy = w * y, wz = w * z;
	rows[0] = 1.0f - 2.0f * (yy + zz);
	rows[1] = 2.0f * (xy - wz);
	rows[2] = 2.0f * (xz + wy);
	rows[3] = 2.0f * (xy + wz);
	rows[4] = 1.0f - 2.0f * (xx + zz);
	rows[5] = 2.0f * (yz - wx);
	rows[6] = 2.0f * (xz - wy);
	rows[7] = 2.0f * (yz + wx);
	rows[8] = 1.0f - 2.0f * (xx + yy);
	if (reflected) {
		rows[6] = -rows[6];
		rows[7] = -rows[7];
		rows[8] = -rows[8];
	}
}

void XwaRemasterHudCmdMath_ObjectEye(const float rows[9], float camera_distance,
									 const int32_t target_world[3], const int32_t object_world[3],
									 float eye[3]) {
	const double relative[3] = { (double)object_world[0] - (double)target_world[0],
								 (double)object_world[1] - (double)target_world[1],
								 (double)object_world[2] - (double)target_world[2] };
	for (int r = 0; r < 3; r++) {
		eye[r] = (float)((double)rows[r * 3] * relative[0] + (double)rows[r * 3 + 1] * relative[1] +
						 (double)rows[r * 3 + 2] * relative[2]);
	}
	eye[2] += camera_distance;
}

void XwaRemasterHudCmdMath_CameraMinusObject(const float rows[9], float camera_distance,
											 const int32_t target_world[3], const int32_t object_world[3],
											 float delta[3]) {
	for (int axis = 0; axis < 3; axis++) {
		delta[axis] = (float)((double)target_world[axis] - (double)object_world[axis] -
							  (double)camera_distance * (double)rows[6 + axis]);
	}
}
