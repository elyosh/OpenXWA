#ifndef XWA_REMASTER_SUNS_H
#define XWA_REMASTER_SUNS_H

#include "aeron/config_file.h"
#include "aeron/scene/scene3d.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The procedural lens flare of the sun that casts the shadows
 * (AeronScene_SetLensFlare), with the elements of the `suns` section. */

/* Reads `suns.flare_elements` from the shipped file, then from the mod's
 * flight/flight.yaml, whose list replaces the shipped one. A wrong mod list
 * is logged and the shipped one is kept. Returns 0 when the shipped list is
 * missing or wrong. */
int XwaRemasterSuns_LoadFlareElements(const AeronConfigFile* shipped, const char* shipped_path);

/* Gives a scene the flare elements, and builds its flare pipelines when
 * `procedural_flare` is set. Does nothing again for the same scene: a new
 * scene has a new `scene_serial`. */
void XwaRemasterSuns_PrepareScene(AeronScene3D* scene, uint64_t scene_serial, int procedural_flare);

/* The tangent of the angular radius of a procedural star's disc, from the
 * view-space quads its sun is drawn on, whose uvs go from 0 to 1 across the
 * sun. Returns 0 without quads. */
int XwaRemasterSuns_StarDiscRadius(const float (*corners)[4][3], const float (*uvs)[4][2], uint32_t count,
								   float* out_tangent);

/* Sets this frame's flare, toward `direction` (world axes) from the camera
 * at `camera_local`, with a disc of the angular radius whose tangent is
 * `tangent`. `tint` is sRGB. Call after AeronScene_Begin. */
void XwaRemasterSuns_SetFlare(AeronScene3D* scene, const float camera_local[3], const float direction[3],
							  float tangent, const float tint[3], float intensity);

#ifdef __cplusplus
}
#endif

#endif
