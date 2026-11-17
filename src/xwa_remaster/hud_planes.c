#include "xwa_remaster/hud_planes.h"

#include <math.h>
#include <string.h>

static void planes_cross(const float a[3], const float b[3], float out[3]) {
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

static float planes_normalize(float v[3]) {
	const float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (length > 0.0f) {
		v[0] /= length;
		v[1] /= length;
		v[2] /= length;
	}
	return length;
}

/* The right and down axes of a plane that faces along `forward`, a unit
 * vector, and is upright for the head. The axes are right, down, forward:
 * right = down x forward. */
static void planes_axes(const XwaRemasterHudPlanes* planes, const float forward[3], float right[3],
						float down[3]) {
	planes_cross(&planes->head_rows[3], forward, right);
	if (planes_normalize(right) < 1e-4f) {
		/* Straight above or below the head: the head's right axis decides. */
		memcpy(right, &planes->head_rows[0], 3 * sizeof(float));
		planes_cross(forward, right, down);
		planes_normalize(down);
		planes_cross(down, forward, right);
		return;
	}
	planes_cross(forward, right, down);
}

/* Writes the plane with the axes `right`, `down` and `forward`, `distance`
 * away from `origin`. With a distance of 1 and no origin, it is at
 * infinity. */
static void planes_write(const XwaRemasterHudPlanes* planes, const float right[3], const float down[3],
						 const float forward[3], float distance, const float* origin, float out[9]) {
	const float pixel = distance / planes->focal_px;
	for (int axis = 0; axis < 3; axis++) {
		out[axis * 3 + 0] = right[axis] * pixel;
		out[axis * 3 + 1] = down[axis] * pixel;
		out[axis * 3 + 2] = (origin ? origin[axis] : 0.0f) + forward[axis] * distance -
							(planes->center_x * right[axis] + planes->center_y * down[axis]) * pixel;
	}
}

void XwaHudPlanes_AtHeadPixel(const XwaRemasterHudPlanes* planes, float x, float y, float out[9]) {
	const float ex = (x - planes->head_axis_x) / planes->focal_px;
	const float ey = (y - planes->head_axis_y) / planes->focal_px;
	float forward[3], right[3], down[3];
	for (int axis = 0; axis < 3; axis++) {
		forward[axis] =
			planes->head_rows[axis] * ex + planes->head_rows[3 + axis] * ey + planes->head_rows[6 + axis];
	}
	planes_normalize(forward);
	planes_axes(planes, forward, right, down);
	planes_write(planes, right, down, forward, 1.0f, NULL, out);
}

float XwaHudPlanes_AtPoint(const XwaRemasterHudPlanes* planes, const float point[3], float out[9]) {
	float forward[3] = { point[0] - planes->head_pos[0], point[1] - planes->head_pos[1],
						 point[2] - planes->head_pos[2] };
	float right[3], down[3];
	const float distance = planes_normalize(forward);
	if (!(distance > 0.0f)) {
		return 0.0f;
	}
	planes_axes(planes, forward, right, down);
	planes_write(planes, right, down, forward, distance, planes->head_pos, out);
	return distance;
}

void XwaHudPlanes_Scale(const float plane[9], float x, float y, float scale, float out[9]) {
	/* S scales the drawing's pixels about (x, y): S * p = (x, y) +
	 * (p - (x, y)) * scale. The new plane is plane * S. */
	for (int axis = 0; axis < 3; axis++) {
		const float a = plane[axis * 3 + 0], b = plane[axis * 3 + 1], c = plane[axis * 3 + 2];
		out[axis * 3 + 0] = a * scale;
		out[axis * 3 + 1] = b * scale;
		out[axis * 3 + 2] = a * x * (1.0f - scale) + b * y * (1.0f - scale) + c;
	}
}

void XwaHudPlanes_ViewRows(const float view_proj[16], float out[12]) {
	memcpy(&out[0], &view_proj[0], 4 * sizeof(float));
	memcpy(&out[4], &view_proj[4], 4 * sizeof(float));
	memcpy(&out[8], &view_proj[12], 4 * sizeof(float));
}
