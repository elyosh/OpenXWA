#include "xwa/assets/model_texture.h"

#include "xwa/flight/fediskio.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/sprite_resource.h"
#include "xwa/assets/sprite_texture.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/net/net.h"
#include "xwa/render/renderer.h"
#include "xwa/render/std3d_device.h"
#include "xwa/util/byte_order.h"
#include "xwa/util/color.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"

#include <string.h>

// GLOBAL: XWA 0x5B5F68
static const uint8_t g_defaultWhiteTextureRgb24[192] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
#ifdef XWA_MODERN
static uint8_t g_defaultWhiteTextureBuildScratch[64 + 8192];
#endif
// GLOBAL: XWA 0x68CA78
static ModelTextureDefaultTextureDesc g_defaultWhiteTextureDesc;

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

// GLOBAL: XWA 0x68CA90
static ModelTextureDefaultTextureData g_defaultWhiteTextureData;
// GLOBAL: XWA 0x68EAF8
static ModelTextureDefaultTextureDesc* g_defaultWhiteTextureDescPtr;
// FUNCTION: XWA 0x44A5A0
void ModelTexture_CacheHyperspaceTunnelFrames(void) {
	uint16_t frame;

	for (frame = 0; frame < g_modelTypeTable[OBJ_AnimationTextureGroup3051].frameCount; ++frame) {
		FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup3051, frame, 256);
		if (g_modelTypeTable[OBJ_AnimationTextureGroup3051].curTexLevel != NULL) {
			std3D_AddToTextureCache(
				(Std3DTextureSurface*)g_modelTypeTable[OBJ_AnimationTextureGroup3051].curTexLevel->image);
		}
	}
}

// FUNCTION: XWA 0x44A600
void ModelTexture_FilterHardwarePalette(uint16_t* palette) {
	uint16_t* currentPalette;
	uint16_t* replacementPalette;
	uint16_t* basePalette;
	int clearedCount;
	int firstClearedIndex;
	int i;
	int plane;
	uint16_t shiftedBaseColor;
	uint16_t shiftedPlaneColor;

	if (g_keepFullResTextures != 2) {
		palette[2304] = 0;
		return;
	}

	firstClearedIndex = -1;
	i = 0;
	clearedCount = 0;
	basePalette = palette;
	currentPalette = basePalette;
	replacementPalette = basePalette + 2560;

	do {
		uint16_t baseColor;

		baseColor = *currentPalette;
		shiftedBaseColor = baseColor;
		shiftedBaseColor >>= 6;

		if ((baseColor & 0x1f) * (baseColor & 0x1f) + (shiftedBaseColor & 0x1f) * (shiftedBaseColor & 0x1f) +
				((shiftedBaseColor >> 5) & 0x1f) * ((shiftedBaseColor >> 5) & 0x1f) <
			32) {
			++clearedCount;
			*currentPalette = 0;
			if (firstClearedIndex == -1) {
				firstClearedIndex = i;
			}
		} else {
			plane = 1;
			currentPalette += 256;
			while (1) {
				uint16_t planeColor;
				int planeBlueDelta;
				int planeGreenDelta;
				int planeRedDelta;

				planeColor = *currentPalette;
				shiftedBaseColor = baseColor;
				shiftedPlaneColor = planeColor;
				shiftedBaseColor >>= 6;
				shiftedPlaneColor >>= 6;
				planeBlueDelta = (planeColor & 0x1f) - (baseColor & 0x1f);
				planeGreenDelta = (shiftedPlaneColor & 0x1f) - (shiftedBaseColor & 0x1f);
				planeRedDelta = ((shiftedPlaneColor >> 5) & 0x1f) - ((shiftedBaseColor >> 5) & 0x1f);
				if (planeBlueDelta * planeBlueDelta + planeGreenDelta * planeGreenDelta +
						planeRedDelta * planeRedDelta >
					16) {
					break;
				}
				currentPalette += 256;
				++plane;
				if (plane >= 7) {
					currentPalette = &basePalette[i];
					break;
				}
			}

			if (plane != 7) {
				++clearedCount;
				currentPalette = &basePalette[i];
				*currentPalette = 0;
				if (firstClearedIndex == -1) {
					firstClearedIndex = i;
				}
			}
			if (plane == 7) {
				*currentPalette = *replacementPalette;
			}
		}

		++i;
		++replacementPalette;
		currentPalette = &basePalette[i];
	} while (i < 256);

	if (clearedCount != 0 && clearedCount >= 256) {
		clearedCount = 0;
	}
	basePalette[256] = (uint16_t)firstClearedIndex;
	basePalette[2304] = (uint16_t)clearedCount;
}

// FUNCTION: XWA 0x44A5F0
int ModelTexture_IsHardwareFormat555(void) { return g_pFmtRGB565->colorInfo.greenBPP == 5; }

// FUNCTION: XWA 0x480580
void ModelTexture_BuildPalettedShadeTable(uint8_t* dst, const uint8_t* rgb24, int width, int height) {
	int texelCount;
	unsigned int paletteCount;
	RgbTriplet targetRgb[2];
	int remaining;
	uint8_t nearestIndex;
	RgbTriplet palette[256];
	const uint8_t* srcRgb;
	uint8_t* dstTexel;
	uint8_t* shadeBase;
	RgbTriplet* paletteEntry;

	texelCount = width * height;
	palette[0].r = (uint8_t)(rgb24[2] >> 3);
	srcRgb = rgb24 + 3;
	paletteCount = 1;
	palette[0].g = (uint8_t)(rgb24[1] >> 3);
	palette[0].b = (uint8_t)(rgb24[0] >> 3);
	dst[0] = 0;
	dstTexel = dst + 1;
	if ((unsigned int)texelCount > 1) {
		remaining = texelCount - 1;
		do {
			targetRgb[0].r = (uint8_t)(srcRgb[2] >> 3);
			targetRgb[0].g = (uint8_t)(srcRgb[1] >> 3);
			targetRgb[0].b = (uint8_t)(srcRgb[0] >> 3);
			nearestIndex = (uint8_t)Color_FindNearestRgbTripletIndex(targetRgb, palette, 0, paletteCount);
			if ((palette[nearestIndex].r != targetRgb[0].r || palette[nearestIndex].g != targetRgb[0].g ||
				 palette[nearestIndex].b != targetRgb[0].b) &&
				paletteCount < 256) {
				nearestIndex = (uint8_t)paletteCount++;
				palette[nearestIndex].r = targetRgb[0].r;
				palette[nearestIndex].g = targetRgb[0].g;
				palette[nearestIndex].b = targetRgb[0].b;
			}
			*dstTexel++ = nearestIndex;
			srcRgb += 3;
			--remaining;
		} while (remaining != 0);
	}

	shadeBase = dst + texelCount;
	paletteEntry = palette;
	remaining = 256;
	do {
		unsigned int shade;
		uint16_t* shadeDst;

		shade = 0;
		shadeDst = (uint16_t*)shadeBase;
		do {
			unsigned int blue;
			uint16_t packed;

			if (shade < 8) {
				targetRgb[0].r =
					(uint8_t)(((((paletteEntry->r << 8) * shade) >> 4) + (paletteEntry->r << 7)) >> 8);
				blue = (((paletteEntry->b << 8) * shade) >> 4) + (paletteEntry->b << 7);
				targetRgb[0].g =
					(uint8_t)(((((paletteEntry->g << 8) * shade) >> 4) + (paletteEntry->g << 7)) >> 8);
			} else {
				int brightShade;

				brightShade = shade - 8;
				targetRgb[0].r = (uint8_t)((((((31 - paletteEntry->r) * brightShade) << 8) >> 3) +
											(paletteEntry->r << 8)) >>
										   8);
				blue = ((((31 - paletteEntry->b) * brightShade) << 8) >> 3) + (paletteEntry->b << 8);
				targetRgb[0].g = (uint8_t)((((((31 - paletteEntry->g) * brightShade) << 8) >> 3) +
											(paletteEntry->g << 8)) >>
										   8);
			}

			packed = (uint16_t)(((targetRgb[0].g + 32 * targetRgb[0].r) << 6) + ((blue >> 8) & 0xff));
			*shadeDst = packed;
			++shade;
			shadeDst += 256;
		} while (shade < 16);
		shadeBase += 2;
		++paletteEntry;
		--remaining;
	} while (remaining != 0);
}

#ifndef XWA_MODERN
#pragma function(memcpy)
#endif
// FUNCTION: XWA 0x480460
ModelTextureDefaultTextureDesc* ModelTexture_GetDefaultWhiteTexture(void) {
	if (g_defaultWhiteTextureDescPtr != NULL) {
		return g_defaultWhiteTextureDescPtr;
	}

	g_defaultWhiteTextureDesc.field04 = 0;
	g_defaultWhiteTextureDescPtr = &g_defaultWhiteTextureDesc;
	g_defaultWhiteTextureDesc.shadeTable = g_defaultWhiteTextureData.shadeTable;
	g_defaultWhiteTextureDesc.baseTexelCount = 64;
	g_defaultWhiteTextureDesc.mipTexelCount = 85;
	g_defaultWhiteTextureDesc.width = 8;
	g_defaultWhiteTextureDesc.height = 8;

#ifdef XWA_MODERN
	ModelTexture_BuildPalettedShadeTable(g_defaultWhiteTextureBuildScratch, g_defaultWhiteTextureRgb24, 8, 8);
	memcpy(g_defaultWhiteTextureData.baseTexels, g_defaultWhiteTextureBuildScratch,
		   sizeof(g_defaultWhiteTextureData.baseTexels));
	memcpy(g_defaultWhiteTextureData.shadeTable, g_defaultWhiteTextureBuildScratch + 64,
		   sizeof(g_defaultWhiteTextureData.shadeTable));
#else
	ModelTexture_BuildPalettedShadeTable(g_defaultWhiteTextureData.baseTexels, g_defaultWhiteTextureRgb24, 8,
										 8);
	memcpy(g_defaultWhiteTextureData.shadeTable, g_defaultWhiteTextureData.mipTexels,
		   sizeof(g_defaultWhiteTextureData.shadeTable));
#endif

	memset(g_defaultWhiteTextureData.mipTexels, g_defaultWhiteTextureData.baseTexels[0],
		   sizeof(g_defaultWhiteTextureData.mipTexels));

	if ((g_useHardware3D ? ModelTexture_IsHardwareFormat555() : g_pixelFormatCode == 555) != 0) {
		int remaining;
		uint16_t* shade;

		remaining = 4096;
		shade = g_defaultWhiteTextureData.shadeTable;
		do {
			uint16_t color;
			int blue;
			int green;
			int red;

			color = *shade++;
			blue = color & 0x1f;
			color >>= 5;
			green = color & 0x3f;
			red = (color >> 6) & 0x1f;
			shade[-1] = (uint16_t)((red << 10) | (green << 5) | blue);
		} while (--remaining != 0);
	}

	return g_defaultWhiteTextureDescPtr;
}
#ifndef XWA_MODERN
#pragma intrinsic(memcpy)
#endif

#ifndef XWA_MODERN
#pragma optimize("g", off)
#endif
// FUNCTION: XWA 0x596EA2
void std3D_DeleteTextureSurface(Std3DTextureSurface* surf) {
	/* The original logs but does not bail on a NULL surface; every caller guards
	 * the pointer, so the subsequent field access is reached only for a live
	 * surface. */
	if (surf == NULL) {
		DebugPrintf("no texture surface in std3D_DeleteTextureSurface()");
	}
	if (surf->bAllocated == 1) {
		std3D_UncacheTextureSurface(surf);
		if (surf->pSrcTexture != NULL) {
			int debugResult;

			g_std3DReleaseRefCount = (int)((IDirect3DTexture*)surf->pSrcTexture)
										 ->lpVtbl->Release((IDirect3DTexture*)surf->pSrcTexture);
			if (g_std3DReleaseRefCount != 1) {
				debugResult =
					DebugPrintf("DX object release returned unexpected refcount:", g_std3DReleaseRefCount);
			} else {
				debugResult = 0;
			}
			(void)debugResult;
		}
		if (surf->pSrcSurface != NULL) {
			int debugResult;

			g_std3DReleaseRefCount = (int)((IDirectDrawSurface*)surf->pSrcSurface)
										 ->lpVtbl->Release((IDirectDrawSurface*)surf->pSrcSurface);
			if (g_std3DReleaseRefCount != 0) {
				debugResult =
					DebugPrintf("DX object not released properly, refcount:", g_std3DReleaseRefCount);
			} else {
				debugResult = 0;
			}
			(void)debugResult;
		}
		Memory_FreeTagged("T3DTEXTSURFACE", surf);
		--g_texSurfaceCount;
	}
}
#ifndef XWA_MODERN
#pragma optimize("g", on)
#endif
