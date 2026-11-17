#ifndef XWA_REMASTER_HUD_PLANES_H
#define XWA_REMASTER_HUD_PLANES_H

#ifdef __cplusplus
extern "C" {
#endif

struct XwaRemasterFlightView;

/* The planes of a headset's HUD.
 *
 * In a headset, the part of the HUD that points at things outside the
 * cockpit (the reticle, the target boxes, the target arrow and their text)
 * is drawn on planes in 3D, straight into each eye image
 * (AeronDrawList_AddPlane, AeronDrawList_RenderPlanes). A plane is a 3x3
 * matrix, row-major, that takes a pixel (x, y, 1) of a drawing to a
 * direction, for a plane at infinity, or to a point. Both are in the space
 * that the flight views' view-projection matrices project.
 *
 * Every plane's drawing has the pixels of the HUD's own image, as a camera
 * on the plane's axis would see it: `focal_px` pixels for a tangent of 1,
 * and the axis at (`center_x`, `center_y`). The HUD's sizes all come from
 * that image's height, so they are right on every plane. */
typedef struct XwaRemasterHudPlanes {
	float focal_px;
	float center_x, center_y;
	/* The plane of the seat's HUD view, at infinity: the reticle group is
	 * laid out in that view. */
	float seat_plane[9];
	/* The size of the reticle group, about the reticle's centre. */
	float reticle_scale;
	/* Pixels of an eye image for one pixel of a drawing, at the middle of
	 * the eye image. Text on a plane uses a font atlas made for its size in
	 * the eye image, which this gives. */
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
} XwaRemasterHudPlanes;

/* The plane, at infinity, of a drawing whose centre is seen at the pixel
 * (x, y) of the head's view. It is upright for the head. */
void XwaHudPlanes_AtHeadPixel(const XwaRemasterHudPlanes* planes, float x, float y, float out[9]);

/* The plane of a drawing whose centre is at `point`, facing the head and
 * upright for it. One pixel of the drawing covers the same angle as at
 * infinity. Returns the distance from the head to the point, or 0, with no
 * plane, when the point is at the head. */
float XwaHudPlanes_AtPoint(const XwaRemasterHudPlanes* planes, const float point[3], float out[9]);

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
