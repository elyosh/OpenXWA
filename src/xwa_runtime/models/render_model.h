#ifndef XWA_RUNTIME_MODELS_RENDER_MODEL_H
#define XWA_RUNTIME_MODELS_RENDER_MODEL_H

/* Modern port: hands the HD renderer the flight model the simulation parsed
 * for a GLB replacement, so each GLB is read and parsed once. Implemented by
 * the OPT synthesizer. */

#include "aeron/asset/flight_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The flight model the simulation parsed for a loaded replacement `key`, to
   build the HD mesh from (render data, render BVH, component mesh types and
   rotations, engine glows, bounds). NULL when no loaded model for `key`
   still holds it. Owned by the synthesizer; valid until
   XwaRenderModel_Release or the model's release. */
const AeronFlightModel* XwaRenderModel_Find(const char* key);
/* Frees the render model of every loaded replacement for `key`, once the HD
   renderer holds a mesh for it. */
void XwaRenderModel_Release(const char* key);
/* Converts the names in the cockpit screens of `model` to the game's
   numbers: the source of each rectangle, and the mask of HUD areas that the
   screens hide. Logs unknown names with `label` and the material. A
   rectangle with an unknown source shows nothing. Call it after each GLB
   parse, before building a mesh from the model. */
void XwaRenderModel_ResolveScreens(AeronFlightModel* model, const char* label);

#ifdef __cplusplus
}
#endif

#endif
