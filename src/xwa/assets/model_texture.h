#ifndef XWA_ASSETS_MODEL_TEXTURE_H
#define XWA_ASSETS_MODEL_TEXTURE_H

#include "xwa/assets/sprite_texture.h"
#include "xwa_runtime/compat/directx/ddraw.h"

#include "xwa/assets/opt_model.h"

#include <stddef.h>
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

/* White fallback texture, shaped like a loaded OPT texture image: the renderer aliases
 * the head as OptTextureData and reads texels at head + sizeof(OptTextureData). The
 * legacy head stores the palette pointer in its first field (the paletteAddress alias),
 * which cannot hold a native pointer; XWA_MODERN uses data.shadeTable instead. */
#ifndef XWA_MODERN
#pragma pack(push, 1)
#endif
typedef struct ModelTextureDefaultTextureData {
	uint8_t baseTexels[64];
	uint8_t mipTexels[21];
	uint16_t shadeTable[4096];
} ModelTextureDefaultTextureData;
#ifndef XWA_MODERN
#pragma pack(pop)
#endif

typedef struct ModelTextureDefaultTexture {
#ifdef XWA_MODERN
	OptTextureData desc;
#else
	uint16_t* shadeTable;
	int field04;
	int baseTexelCount;
	int mipTexelCount;
	int height;
	int width;
#endif
	ModelTextureDefaultTextureData data;
} ModelTextureDefaultTexture;

typedef char model_texture_white_texels_offset[(offsetof(ModelTextureDefaultTexture, data) ==
												sizeof(OptTextureData))
												   ? 1
												   : -1];

struct Std3DTextureSurface;

/* DDCOLORKEY, DDSCAPS, DDPIXELFORMAT, DDSURFACEDESC, and LPDDSURFACEDESC now live
 * in the DirectDraw compatibility header (single source of truth, shared with the
 * ddraw/d3d shim); included at the top of this file. */

void ModelTexture_CacheHyperspaceTunnelFrames(void);
void ModelTexture_FilterHardwarePalette(uint16_t* palette);
int ModelTexture_IsHardwareFormat555(void);
void ModelTexture_BuildPalettedShadeTable(uint8_t* dst, const uint8_t* rgb24, int width, int height);
ModelTextureDefaultTexture* ModelTexture_GetDefaultWhiteTexture(void);
void std3D_DeleteTextureSurface(struct Std3DTextureSurface* surface);

#ifdef __cplusplus
}
#endif

#endif
