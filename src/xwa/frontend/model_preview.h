#ifndef XWA_FRONTEND_MODEL_PREVIEW_H
#define XWA_FRONTEND_MODEL_PREVIEW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ModelPreview_FreeResources(void);
int ModelPreview_FreeTexture237(void);
int ModelPreview_LoadTexture237(void);
char ModelPreview_LoadModel(const char* modelName, int objectType);
int  ModelPreview_RenderViewport(int x, int y, int width, int height, void* softwareSurface,
								  int softwarePitch, unsigned int softwareHeight);
int  ModelPreview_RenderWireframeViewport(int x, int y, int width, int height, int lineColor,
										   unsigned char* softwareSurface, int softwarePitch,
										   int softwareHeight);
int  ModelPreview_SetWhiteDirectionalLight(int dx, int dy, int dz);
void    ModelPreview_SetObjectEulerDegrees(float pitchDeg, float yawDeg, float rollDeg);
int     ModelPreview_SetObjectWorldPosition(int x, int y, int z);
int     ModelPreview_SetNodeSwitchIndex(int nodeSwitchIndex);
int64_t ModelPreview_SetObjectAngleDDegrees(float angleDeg);
int     ModelPreview_GetDisplayedSizeMeters(void);
int     Frontend3D_SetRuntimeHardwareEnabled(int enabled);

#ifdef __cplusplus
}
#endif

#endif
