#ifndef XWA_REMASTER_HUD_PLANES_H
#define XWA_REMASTER_HUD_PLANES_H

#ifdef __cplusplus
extern "C" {
#endif

struct XwaRemasterFlightView;

/* The planes of the HUD's markers, in a headset and on surround screens.
 *
 * There, the part of the HUD that points at things outside the
 * cockpit (the reticle, the target boxes, the target arrow and their text)
 * is drawn on planes in 3D, straight into each view's image
 * (AeronDrawList_AddPlane, AeronDrawList_RenderPlanes). A plane is a 3x3
 * matrix, row-major, that takes a pixel (x, y, 1) of a drawing to a
 * direction, for a plane at infinity, or to a point. Both are in the space
 * that the flight views' view-projection matrices project.
 *
 * Every plane's drawing has the pixels of the HUD's own image, as a camera
 * on the plane's axis would see it: `focal_px` pixels for a tangent of 1,
 * and the axis at (`center_x`, `center_y`). The HUD's sizes all come from
 * that image's height, so they are right on every plane.
 *
 * On surround screens the HUD's image is the centre screen's, and the planes
 * are parallel to the screens (`screens`), so the markers cover whole pixels
 * as a flat HUD does. */
#define XWA_HUD_PLANES_MAX_SCREENS 4

/* One screen of a surround setup. */
typedef struct XwaRemasterHudScreen {
	/* The screen's right, down and forward axes as rows. */
	float rows[9];
	/* The tangents of the screen's edges, seen from the head: left, right,
	 * top and bottom along the right and down axes. */
	float tan_left, tan_right, tan_top, tan_bottom;
	/* The screen's pixels for a tangent of 1 along each axis. */
	float focal_x_px, focal_y_px;
} XwaRemasterHudScreen;

typedef struct XwaRemasterHudPlanes {
	float focal_px;
	float center_x, center_y;
	/* The plane of the seat's HUD view, at infinity: the reticle group is
	 * laid out in that view. */
	float seat_plane[9];
	/* The size of the reticle group, about the reticle's centre. */
	float reticle_scale;
	/* Pixels of an eye image for one pixel of a drawing, at the middle of the
	 * eye image. Text on a plane uses this to pick the font atlas made for its
	 * size in the eye image. */
	float eye_pixel_scale;
	/* The head: where it is, and its right, down and forward axes as rows. */
	float head_pos[3];
	float head_rows[9];
	/* The view that follows the head and covers what both eyes see, with
	 * `focal_px` pixels for a tangent of 1. It has no image: the arrow to a
	 * target out of view is placed in it. (`head_axis_x`, `head_axis_y`) is
	 * its pixel on the head's axis. */
	const struct XwaRemasterFlightView* head_view;
	float head_axis_x, head_axis_y;
	/* The surround screens, or a count of 0 in a headset. With screens, the
	 * head doesn't turn, and a drawing is on a plane parallel to the screen
	 * that shows its centre. */
	int screen_count;
	XwaRemasterHudScreen screens[XWA_HUD_PLANES_MAX_SCREENS];
} XwaRemasterHudPlanes;

/* The plane, at infinity, of a drawing whose centre is seen at the pixel
 * (x, y) of the head's view. It is upright for the head. */
void XwaHudPlanes_AtHeadPixel(const XwaRemasterHudPlanes* planes, float x, float y, float out[9]);

/* The plane of a drawing whose centre is at `point`.
 *
 * In a headset the plane faces the head and is upright for it. One pixel of
 * the drawing covers the same angle as at infinity. Returns the distance from
 * the head to the point, or 0, with no plane, when the point is at the head.
 *
 * With screens, the plane is parallel to the screen that shows the point, or
 * to the nearest one, and one pixel of the drawing is one pixel of that
 * screen. Returns the point's depth along the screen's axis, or 0, with no
 * plane, when the point is behind every screen. */
float XwaHudPlanes_AtPoint(const XwaRemasterHudPlanes* planes, const float point[3], float out[9]);

/* Returns the index of the surround screen that shows `point`, or -1 when no
 * screen shows it or there are no screens. */
int XwaHudPlanes_ScreenAt(const XwaRemasterHudPlanes* planes, const float point[3]);

/* The plane `plane` with its drawing made `scale` times as large about the
 * drawing's pixel (x, y). */
void XwaHudPlanes_Scale(const float plane[9], float x, float y, float scale, float out[9]);

/* The rows x, y and w of a row-major 4x4 view-projection matrix, as
 * AeronDrawList_RenderPlanes takes them. */
void XwaHudPlanes_ViewRows(const float view_proj[16], float out[12]);

#ifdef __cplusplus
}
#endif

#endif
