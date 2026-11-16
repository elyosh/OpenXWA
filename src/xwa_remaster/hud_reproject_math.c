#include "xwa_remaster/hud_reproject_math.h"

#include <math.h>

/* Writes the inverse of the row-major 3x3 matrix `m`. Returns 0 when it has
 * none. */
static int reproject_invert3(const float m[9], float out[9]) {
	const double a = m[0], b = m[1], c = m[2];
	const double d = m[3], e = m[4], f = m[5];
	const double g = m[6], h = m[7], i = m[8];
	const double co0 = e * i - f * h;
	const double co1 = f * g - d * i;
	const double co2 = d * h - e * g;
	const double det = a * co0 + b * co1 + c * co2;
	const double size = fabs(a) + fabs(b) + fabs(c) + fabs(d) + fabs(e) + fabs(f) + fabs(g) + fabs(h) + fabs(i);
	if (fabs(det) <= 1e-6 * size * size * size) {
		return 0;
	}
	const double inv = 1.0 / det;
	out[0] = (float)(co0 * inv);
	out[1] = (float)((c * h - b * i) * inv);
	out[2] = (float)((b * f - c * e) * inv);
	out[3] = (float)(co1 * inv);
	out[4] = (float)((a * i - c * g) * inv);
	out[5] = (float)((c * d - a * f) * inv);
	out[6] = (float)(co2 * inv);
	out[7] = (float)((b * g - a * h) * inv);
	out[8] = (float)((a * e - b * d) * inv);
	return 1;
}

static void reproject_mul3(const float a[9], const float b[9], float out[9]) {
	for (int row = 0; row < 3; row++) {
		for (int col = 0; col < 3; col++) {
			out[row * 3 + col] =
				a[row * 3 + 0] * b[0 * 3 + col] + a[row * 3 + 1] * b[1 * 3 + col] + a[row * 3 + 2] * b[2 * 3 + col];
		}
	}
}

/* The part of a view-projection matrix that projects a direction: the clip
 * X, Y and W of a point at infinity. */
static void reproject_direction_part(const float view_proj[16], float out[9]) {
	static const int rows[3] = { 0, 1, 3 };
	for (int row = 0; row < 3; row++) {
		for (int col = 0; col < 3; col++) {
			out[row * 3 + col] = view_proj[rows[row] * 4 + col];
		}
	}
}

int XwaHudReproject_Infinity(const float eye_view_proj[16], const float hud_view_proj[16], float out[9]) {
	float eye[9], hud[9], eye_inverse[9];
	reproject_direction_part(eye_view_proj, eye);
	reproject_direction_part(hud_view_proj, hud);
	/* The eye pixel's direction, then that direction in the HUD's view. */
	if (!reproject_invert3(eye, eye_inverse)) {
		return 0;
	}
	reproject_mul3(hud, eye_inverse, out);
	return 1;
}

int XwaHudReproject_Panel(const float eye_view_proj[16], const float center[3], const float half_right[3],
						  const float half_up[3], float out[9]) {
	/* The panel point (u, v) is center + u * half_right + v * half_up. Its
	 * clip X, Y and W are a matrix times (u, v, 1). The inverse of that
	 * matrix takes the eye pixel back to (u, v, 1) / W. */
	static const int rows[3] = { 0, 1, 3 };
	float panel_to_clip[9];
	for (int row = 0; row < 3; row++) {
		const float* m = &eye_view_proj[rows[row] * 4];
		panel_to_clip[row * 3 + 0] = m[0] * half_right[0] + m[1] * half_right[1] + m[2] * half_right[2];
		panel_to_clip[row * 3 + 1] = m[0] * half_up[0] + m[1] * half_up[1] + m[2] * half_up[2];
		panel_to_clip[row * 3 + 2] = m[0] * center[0] + m[1] * center[1] + m[2] * center[2] + m[3];
	}
	return reproject_invert3(panel_to_clip, out);
}
