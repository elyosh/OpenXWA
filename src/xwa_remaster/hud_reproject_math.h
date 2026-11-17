#ifndef XWA_REMASTER_HUD_REPROJECT_MATH_H
#define XWA_REMASTER_HUD_REPROJECT_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

/* The math that places the HUD's panel in a headset's eye images. The fixed
 * group of the HUD is drawn once into a flat image, and each eye shows that
 * image on a flat panel in the cockpit. That is a 3x3 matrix that takes a
 * pixel of the eye image to a point of the HUD image:
 *
 *     (x, y, w) = M * (eye_ndc_x, eye_ndc_y, 1)
 *
 * With w > 0, the HUD point is (x / w, y / w), from -1 to 1 on both axes
 * with Y up, like the eye's own coordinates. With w <= 0 the HUD image is
 * behind the eye. shaders/hud_reproject.frag.hlsl uses the matrix.
 *
 * The view-projection matrix is row-major and takes column vectors, as
 * AeronScene_ComputeViewProj writes it. The matrix is row-major 3x3.
 *
 * The HUD's world group isn't drawn this way: see hud_planes.h. */

/* The HUD image on a flat panel. The panel's centre is `center`, and
 * `half_right` and `half_up` go from the centre to the middle of its right
 * and top edges, all in the space that `eye_view_proj` projects. Returns 0
 * when the eye is in the panel's plane. */
int XwaHudReproject_Panel(const float eye_view_proj[16], const float center[3], const float half_right[3],
						  const float half_up[3], float out[9]);

#ifdef __cplusplus
}
#endif

#endif
