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

/* The place of `point` on `screen`: its depth along the screen's axis, and
 * how far outside the screen's edges it is, as a tangent. 0 is on the screen.
 * Returns 0 when the point isn't in front of the screen. */
static int planes_on_screen(const XwaRemasterHudPlanes* planes, const XwaRemasterHudScreen* screen,
							const float point[3], float* depth, float* outside) {
	const float d[3] = { point[0] - planes->head_pos[0], point[1] - planes->head_pos[1],
						 point[2] - planes->head_pos[2] };
	const float x = screen->rows[0] * d[0] + screen->rows[1] * d[1] + screen->rows[2] * d[2];
	const float y = screen->rows[3] * d[0] + screen->rows[4] * d[1] + screen->rows[5] * d[2];
	const float z = screen->rows[6] * d[0] + screen->rows[7] * d[1] + screen->rows[8] * d[2];
	if (!(z > 0.0f)) {
		return 0;
	}
	const float tx = x / z, ty = y / z;
	float over = 0.0f;
	over = fmaxf(over, screen->tan_left - tx);
	over = fmaxf(over, tx - screen->tan_right);
	over = fmaxf(over, screen->tan_top - ty);
	over = fmaxf(over, ty - screen->tan_bottom);
	*depth = z;
	*outside = over;
	return 1;
}

/* Returns the screen that shows `point`, or the nearest one that the point
 * is in front of, with the point's depth on it. -1 when there is none. */
static int planes_nearest_screen(const XwaRemasterHudPlanes* planes, const float point[3], float* depth,
								 float* outside) {
	int best = -1;
	for (int i = 0; i < planes->screen_count; i++) {
		float z, over;
		if (planes_on_screen(planes, &planes->screens[i], point, &z, &over) &&
			(best < 0 || over < *outside)) {
			best = i;
			*depth = z;
			*outside = over;
		}
	}
	return best;
}

int XwaHudPlanes_ScreenAt(const XwaRemasterHudPlanes* planes, const float point[3]) {
	float depth = 0.0f, outside = 0.0f;
	const int screen = planes_nearest_screen(planes, point, &depth, &outside);
	return screen >= 0 && outside <= 0.0f ? screen : -1;
}

float XwaHudPlanes_AtPoint(const XwaRemasterHudPlanes* planes, const float point[3], float out[9]) {
	if (planes->screen_count > 0) {
		float depth = 0.0f, outside = 0.0f;
		const int index = planes_nearest_screen(planes, point, &depth, &outside);
		if (index < 0) {
			return 0.0f;
		}
		const XwaRemasterHudScreen* screen = &planes->screens[index];
		const float pixel_x = depth / screen->focal_x_px;
		const float pixel_y = depth / screen->focal_y_px;
		/* The drawing moves by less than a pixel, so that its pixels are the
		 * screen's: text then stays sharp. */
		float d[3], at[2];
		for (int axis = 0; axis < 3; axis++) {
			d[axis] = point[axis] - planes->head_pos[axis];
		}
		for (int row = 0; row < 2; row++) {
			const float* r = &screen->rows[row * 3];
			const float tangent = (r[0] * d[0] + r[1] * d[1] + r[2] * d[2]) / depth;
			const float pixel = row == 0 ? (tangent - screen->tan_left) * screen->focal_x_px
										 : (tangent - screen->tan_top) * screen->focal_y_px;
			const float offset = pixel - (row == 0 ? planes->center_x : planes->center_y);
			at[row] = (row == 0 ? planes->center_x : planes->center_y) + offset - roundf(offset);
		}
		for (int axis = 0; axis < 3; axis++) {
			out[axis * 3 + 0] = screen->rows[axis] * pixel_x;
			out[axis * 3 + 1] = screen->rows[3 + axis] * pixel_y;
			out[axis * 3 + 2] = point[axis] - at[0] * out[axis * 3 + 0] - at[1] * out[axis * 3 + 1];
		}
		return depth;
	}
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
