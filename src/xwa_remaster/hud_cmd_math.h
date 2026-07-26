#ifndef XWA_REMASTER_HUD_CMD_MATH_H
#define XWA_REMASTER_HUD_CMD_MATH_H

#include <stdint.h>

/* Reconstruct an orthonormal world-to-eye basis from the classic CMD
 * camera's near-orthonormal Q15 rows, preserving its handedness. */
void XwaRemasterHudCmdMath_OrthonormalRows(const int16_t rows_q15[9], float rows[9]);

/* PiP-local positions. The integer subtraction happens before float
 * conversion, so a shared large world origin cannot perturb the result. */
void XwaRemasterHudCmdMath_ObjectEye(const float rows[9], float camera_distance,
									 const int32_t target_world[3], const int32_t object_world[3],
									 float eye[3]);
void XwaRemasterHudCmdMath_CameraMinusObject(const float rows[9], float camera_distance,
											 const int32_t target_world[3], const int32_t object_world[3],
											 float delta[3]);

#endif
