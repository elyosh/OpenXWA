#ifndef XWA_ASSETS_MODEL_TEXTURE_H
#define XWA_ASSETS_MODEL_TEXTURE_H

#include "xwa/assets/sprite_texture.h"
#include "xwa_runtime/compat/directx/ddraw.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODEL_TEXTURE_SPECIESTMINFO_TAG "SPECIESTMINFO"
#define MODEL_TEXTURE_RESOURCEITEM_TAG "RESOURCEITEM"

enum {
	MODEL_TEXTURE_PAIR_MODEL_TYPE = 487,
	MODEL_TEXTURE_PAIR_LOAD_TYPE = 488,
	MODEL_TEXTURE_FONT_MODEL_TYPE = 418,
	MODEL_TEXTURE_SPRITE_HEADER_SIZE = 18,
	MODEL_TEXTURE_SPRITE_WIDTH_OFFSET = 2,
	MODEL_TEXTURE_SPRITE_HEIGHT_OFFSET = 4,
	MODEL_TEXTURE_SPRITE_PIXEL_DATA_SIZE_OFFSET = 14,
};

typedef struct ModelTextureDefaultTextureDesc {
	uint16_t* shadeTable;
	int field04;
	int baseTexelCount;
	int mipTexelCount;
	int height;
	int width;
} ModelTextureDefaultTextureDesc;

struct Std3DTextureSurface;

/* DDCOLORKEY, DDSCAPS, DDPIXELFORMAT, DDSURFACEDESC, and LPDDSURFACEDESC now live
 * in the DirectDraw compatibility header (single source of truth, shared with the
 * ddraw/d3d shim); included at the top of this file. */

void ModelTexture_CacheHyperspaceTunnelFrames(void);
void ModelTexture_FilterHardwarePalette(uint16_t* palette);
int ModelTexture_IsHardwareFormat555(void);
void ModelTexture_BuildPalettedShadeTable(uint8_t* dst, const uint8_t* rgb24, int width, int height);
ModelTextureDefaultTextureDesc* ModelTexture_GetDefaultWhiteTexture(void);
void std3D_DeleteTextureSurface(struct Std3DTextureSurface* surface);

#ifdef __cplusplus
}
#endif

#endif
