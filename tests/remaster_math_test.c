/*
 * Tests the pure math of the remaster: the matrices that place the HUD in a
 * headset's eye images (src/xwa_remaster/hud_reproject_math.c).
 *
 * Built as xwa_remaster_math_test. It needs no window and no GPU. It prints
 * each check and exits with a non-zero status when a check fails.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "xwa_remaster/hud_reproject_math.h"

#define TOLERANCE 1e-4f
#define PI 3.14159265358979f

static int g_failures;

static void check(int ok, const char* what) {
	printf("%s %s\n", ok ? "ok" : "FAILED", what);
	g_failures += !ok;
}

static int near(float a, float b) { return fabsf(a - b) <= TOLERANCE * (1.0f + fabsf(b)); }

/* A view-projection matrix as AeronScene_ComputeViewProj writes it:
 * row-major, for column vectors. Eye space is X right, Y down, Z forward.
 * `rows` are the world-to-eye rows and `pos` is the eye's position. */
static void view_proj(const float rows[9], const float pos[3], float h_half, float v_half, float offset_x,
					  float offset_y, float out[16]) {
	float view[16] = { 0 }, proj[16] = { 0 };
	for (int row = 0; row < 3; row++) {
		for (int col = 0; col < 3; col++)
			view[row * 4 + col] = rows[row * 3 + col];
		view[row * 4 + 3] = -(rows[row * 3 + 0] * pos[0] + rows[row * 3 + 1] * pos[1] + rows[row * 3 + 2] * pos[2]);
	}
	view[15] = 1.0f;
	proj[0] = 1.0f / tanf(h_half);
	proj[2] = offset_x;
	proj[5] = -1.0f / tanf(v_half);
	proj[6] = offset_y;
	proj[11] = 0.1f;
	proj[14] = 1.0f;
	for (int row = 0; row < 4; row++)
		for (int col = 0; col < 4; col++)
			out[row * 4 + col] = proj[row * 4 + 0] * view[0 * 4 + col] + proj[row * 4 + 1] * view[1 * 4 + col] +
								 proj[row * 4 + 2] * view[2 * 4 + col] + proj[row * 4 + 3] * view[3 * 4 + col];
}

/* The eye rows of a view turned right by `yaw` and then down by `pitch`. */
static void turned_rows(float yaw, float pitch, float out[9]) {
	const float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
	const float rows[9] = {
		cy,       0.0f, -sy,      /* right */
		-sp * sy, cp,   -sp * cy, /* down */
		cp * sy,  sp,   cp * cy,  /* forward */
	};
	memcpy(out, rows, sizeof rows);
}

/* Projects a world point. Returns 0 when it is behind the view. */
static int project(const float m[16], const float p[3], float w, float out[2]) {
	const float x = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3] * w;
	const float y = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7] * w;
	const float cw = m[12] * p[0] + m[13] * p[1] + m[14] * p[2] + m[15] * w;
	if (cw <= 0.0f)
		return 0;
	out[0] = x / cw;
	out[1] = y / cw;
	return 1;
}

/* Applies a reprojection matrix to an eye pixel. Returns 0 when the HUD
 * image is behind the eye. */
static int reproject(const float m[9], const float eye[2], float out[2]) {
	const float w = m[6] * eye[0] + m[7] * eye[1] + m[8];
	if (w <= 0.0f)
		return 0;
	out[0] = (m[0] * eye[0] + m[1] * eye[1] + m[2]) / w;
	out[1] = (m[3] * eye[0] + m[4] * eye[1] + m[5]) / w;
	return 1;
}

static void test_infinity(void) {
	const float origin[3] = { 0.0f, 0.0f, 0.0f };
	const float seat_rows[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	float       hud[16], eye[16], m[9];
	view_proj(seat_rows, origin, 0.6f, 0.47f, 0.0f, 0.0f, hud);

	check(XwaHudReproject_Infinity(hud, hud, m) && near(m[0], m[8]) && near(m[4], m[8]) && m[8] > 0.0f &&
			  near(m[1], 0) && near(m[2], 0) && near(m[3], 0) && near(m[5], 0) && near(m[6], 0) &&
			  near(m[7], 0),
		  "infinity: the HUD's own view gives the identity");

	/* An eye turned and moved away from the seat, with an off-centre
	 * projection as a headset's eye has. */
	float       eye_rows[9];
	const float eye_pos[3] = { 0.03f, -0.1f, 0.2f };
	turned_rows(25.0f * PI / 180.0f, -10.0f * PI / 180.0f, eye_rows);
	view_proj(eye_rows, eye_pos, 0.8f, 0.85f, 0.12f, -0.05f, eye);
	check(XwaHudReproject_Infinity(eye, hud, m), "infinity: a turned eye has a matrix");

	/* A direction: the eye pixel that shows it goes to the HUD pixel that
	 * shows it. The eye's position doesn't matter. */
	const float direction[3] = { 0.25f, -0.1f, 1.0f };
	float       in_eye[2], in_hud[2], out[2];
	check(project(eye, direction, 0.0f, in_eye) && project(hud, direction, 0.0f, in_hud) &&
			  reproject(m, in_eye, out) && near(out[0], in_hud[0]) && near(out[1], in_hud[1]),
		  "infinity: a direction lands on the same HUD pixel from a turned eye");

	/* A direction behind the seat has no HUD pixel. */
	const float behind[3] = { 1.0f, 0.0f, -0.2f };
	float       back_rows[9], back[16], in_back[2];
	turned_rows(100.0f * PI / 180.0f, 0.0f, back_rows);
	view_proj(back_rows, origin, 0.8f, 0.8f, 0.0f, 0.0f, back);
	check(XwaHudReproject_Infinity(back, hud, m) && project(back, behind, 0.0f, in_back) &&
			  !reproject(m, in_back, out),
		  "infinity: a direction behind the HUD's view has no HUD pixel");
}

static void test_panel(void) {
	/* A panel 0.8 in front of the seat, 1.2 wide and 0.9 high. */
	const float center[3] = { 0.0f, 0.0f, 0.8f };
	const float half_right[3] = { 0.6f, 0.0f, 0.0f };
	const float half_up[3] = { 0.0f, -0.45f, 0.0f }; /* Y is down */
	float       eye_rows[9], eye[16], m[9];
	const float eye_pos[3] = { 0.032f, 0.01f, -0.05f };
	turned_rows(-8.0f * PI / 180.0f, 5.0f * PI / 180.0f, eye_rows);
	view_proj(eye_rows, eye_pos, 0.8f, 0.85f, -0.1f, 0.0f, eye);
	check(XwaHudReproject_Panel(eye, center, half_right, half_up, m), "panel: an eye in front has a matrix");

	/* The panel point (u, v), seen by the eye, goes back to (u, v). */
	const float uv[3][2] = { { 0.0f, 0.0f }, { 1.0f, 1.0f }, { -0.4f, 0.7f } };
	int         ok = 1;
	for (int i = 0; i < 3; i++) {
		float point[3], in_eye[2], out[2];
		for (int axis = 0; axis < 3; axis++)
			point[axis] = center[axis] + uv[i][0] * half_right[axis] + uv[i][1] * half_up[axis];
		ok = ok && project(eye, point, 1.0f, in_eye) && reproject(m, in_eye, out) && near(out[0], uv[i][0]) &&
			 near(out[1], uv[i][1]);
	}
	check(ok, "panel: the centre, a corner and another point come back");

	/* The two eyes see the panel's centre in different pixels: it has a
	 * depth. A point at infinity would be in the same pixel. */
	float       other[16], in_a[2], in_b[2];
	const float other_pos[3] = { -0.032f, 0.01f, -0.05f };
	view_proj(eye_rows, other_pos, 0.8f, 0.85f, -0.1f, 0.0f, other);
	check(project(eye, center, 1.0f, in_a) && project(other, center, 1.0f, in_b) &&
			  fabsf(in_a[0] - in_b[0]) > 0.05f,
		  "panel: the two eyes see it at different places");

	/* An eye that looks away from the panel doesn't see it. */
	float away_rows[9], away[16], pixel[2] = { 0.0f, 0.0f }, out[2];
	turned_rows(PI, 0.0f, away_rows);
	view_proj(away_rows, eye_pos, 0.8f, 0.85f, 0.0f, 0.0f, away);
	check(XwaHudReproject_Panel(away, center, half_right, half_up, m) && !reproject(m, pixel, out),
		  "panel: an eye that looks away sees no panel");

	/* An eye in the panel's plane has no matrix. */
	const float in_plane[3] = { 2.0f, 0.0f, 0.8f };
	float       flat[16];
	view_proj(eye_rows, in_plane, 0.8f, 0.85f, 0.0f, 0.0f, flat);
	check(!XwaHudReproject_Panel(flat, center, half_right, half_up, m), "panel: an eye in its plane has no matrix");
}

int main(void) {
	test_infinity();
	test_panel();
	printf(g_failures ? "%d checks FAILED\n" : "all checks passed\n", g_failures);
	return g_failures ? 1 : 0;
}
