#ifndef XWA_ASSETS_MODEL_BOUNDS_H
#define XWA_ASSETS_MODEL_BOUNDS_H

#include "xwa/render/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

void ModelBounds_EnsureCached(int modelType);
int ModelBounds_GetMaxExtent(int modelType);
int ModelBounds_GetMinY(int modelType);
int ModelBounds_GetMinZ(int modelType);
int ModelBounds_GetMaxY(int modelType);
int ModelBounds_GetMaxZ(int modelType);
Vec3f* ModelBounds_GetMinVector(int modelType);
Vec3f* ModelBounds_GetMaxVector(int modelType);
int ModelBounds_GetSizeX(int modelType);
int ModelBounds_GetSizeY(int modelType);
int ModelBounds_GetSizeZ(int modelType);
double ModelBounds_ComputeMaxMinExtentRatio(int modelType);
int ModelBounds_ClearCache(void);

#ifdef __cplusplus
}
#endif

#endif
