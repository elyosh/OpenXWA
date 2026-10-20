#ifndef XWA_ASSETS_MODEL_BOUNDS_H
#define XWA_ASSETS_MODEL_BOUNDS_H

#include "xwa/render/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Nonzero once a object type's bounds are computed, per object type. */
extern int g_modelBoundsCached[];

void ModelBounds_EnsureCached(int objectType);
int ModelBounds_GetMaxExtent(int objectType);
int ModelBounds_GetMinY(int objectType);
int ModelBounds_GetMinZ(int objectType);
int ModelBounds_GetMaxY(int objectType);
int ModelBounds_GetMaxZ(int objectType);
Vec3f* ModelBounds_GetMinVector(int objectType);
Vec3f* ModelBounds_GetMaxVector(int objectType);
int ModelBounds_GetSizeX(int objectType);
int ModelBounds_GetSizeY(int objectType);
int ModelBounds_GetSizeZ(int objectType);
double ModelBounds_ComputeMaxMinExtentRatio(int objectType);
int ModelBounds_ClearCache(void);

#ifdef __cplusplus
}
#endif

#endif
