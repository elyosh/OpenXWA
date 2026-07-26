#include "xwa/render/std3d_device.h"

#include "xwa_runtime/compat/middleware_crt.h"
#include "xwa/flight/flight.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"

#include <string.h>

/* std3D texture-surface creation. Builds a system-memory DirectDraw source
 * surface in the destination texel format, converts the source raster into it,
 * and wraps it as an IDirect3DTexture (surf->pSrcTexture). std3D_CacheTextureSurface
 * later creates the video-memory destination and Loads the source into it.
 * Recovered from std3D_CreateMipSurface @0x5960BE, std3D_CopyTextureFromSource
 * @0x596F89, std3D_AllocVBuffer @0x5934E9, std3D_FreeVBuffer @0x59357D,
 * std3D_GetOrCreatePalette @0x593CCC. */

// GLOBAL: XWA 0x608880
unsigned int g_std3DMinTextureWidth;
// GLOBAL: XWA 0x608884
unsigned int g_std3DMinTextureHeight;
// GLOBAL: XWA 0x7B1D48
int g_texSurfaceCount;

// TODO: separately tracked dependency, not yet matched.
// FUNCTION: XWA 0x5960B4
int std3D_GetTextureSurfaceCount(void) { return g_texSurfaceCount; }
// GLOBAL: XWA 0x7B1D40
Std3DPaletteNode* g_pPaletteListHead;
// GLOBAL: XWA 0x7B1D44
Std3DPaletteNode* g_pPaletteListTail;
/* DirectDraw PALETTEENTRY layout for CreatePalette. */
typedef struct Std3DPaletteEntry {
	uint8_t peRed;
	uint8_t peGreen;
	uint8_t peBlue;
	uint8_t peFlags;
} Std3DPaletteEntry;
// GLOBAL: XWA 0x7B15C8
Std3DPaletteEntry g_paletteBuildBuf[256];

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x5934E9
Std3DVBuffer* std3D_AllocVBuffer(Std3DRasterInfo* pRasterInfo, int reserved1, int reserved2, int reserved3) {
	Std3DVBuffer* vbuffer = (Std3DVBuffer*)Memory_AllocTagged("VBUFFER", sizeof(Std3DVBuffer));

	(void)reserved1;
	(void)reserved2;
	(void)reserved3;

	((void* (*)(void*, int, size_t))memset)(vbuffer, 0, sizeof(Std3DVBuffer));
	vbuffer->storageType = 0;
	memcpy_0(&vbuffer->raster, pRasterInfo, sizeof(vbuffer->raster));
	vbuffer->pixels = Memory_AllocTagged("VBUFFERPIXELS", (pRasterInfo->width * pRasterInfo->height) *
															  (pRasterInfo->bitsPerPixel >> 3));
	vbuffer->raster.rowPitch = pRasterInfo->width * (pRasterInfo->bitsPerPixel >> 3);
	return vbuffer;
}

// FUNCTION: XWA 0x59357D
void std3D_FreeVBuffer(Std3DVBuffer* pVBuffer) {
	if (pVBuffer->storageType == 1) {
		if (pVBuffer->ddSurface) {
			IDirectDrawSurface* surface = (IDirectDrawSurface*)pVBuffer->ddSurface;
			g_std3DReleaseRefCount = (int)surface->lpVtbl->Release(surface);
			if (g_std3DReleaseRefCount) {
				DebugPrintf("DX object not released properly, refcount: %d\n", g_std3DReleaseRefCount);
			}
		}
	} else {
		Memory_FreeTagged("VBUFFERPIXELS", pVBuffer->pixels);
	}
	memset(pVBuffer, 0, sizeof(Std3DVBuffer));
	Memory_FreeTagged("VBUFFER", pVBuffer);
}

// FUNCTION: XWA 0x593CCC
void* std3D_GetOrCreatePalette(uint16_t* srcPalette565) {
	uint8_t color;

	{
		HRESULT result;
		Std3DPaletteNode* node;
		IDirectDrawPalette* ddPalette;
		int i;

		node = NULL;
		node = g_pPaletteListHead;
		while (node) {
			if (node->srcPalette565 == srcPalette565) {
				return node;
			}
			node = node->next;
		}

		node = (Std3DPaletteNode*)Memory_AllocTagged("D3DPALETTE8", sizeof(Std3DPaletteNode));
		node->srcPalette565 = srcPalette565;
		for (i = 0; i < 256; ++i) {
			color = (uint8_t)(8 * (srcPalette565[i] >> 11));
			g_paletteBuildBuf[i].peRed = color;
			color = (uint8_t)(4 * (srcPalette565[i] >> 5));
			g_paletteBuildBuf[i].peGreen = color;
			color = (uint8_t)(8 * srcPalette565[i]);
			g_paletteBuildBuf[i].peBlue = color;
			g_paletteBuildBuf[i].peFlags = 0;
		}
		result = g_std3DDirectDraw->lpVtbl->CreatePalette(g_std3DDirectDraw, 68, g_paletteBuildBuf,
														  &ddPalette, NULL);
		if (result != 0) {
			Memory_FreeTagged("D3DPALETTE8", node);
			return NULL;
		}
		node->ddPalette = ddPalette;
		node->next = g_pPaletteListHead;
		g_pPaletteListHead = node;
		return node;
	}
}

// FUNCTION: XWA 0x593E7B
void std3D_CreatePaletteForTexture(uint16_t* srcPalette565) {
	g_pPaletteListTail = (Std3DPaletteNode*)std3D_GetOrCreatePalette(srcPalette565);
}

// FUNCTION: XWA 0x593E0D
void std3D_FreePalettes(void) {
	Std3DPaletteNode* node;
	Std3DPaletteNode* next;

	node = g_pPaletteListHead;
	while (node) {
		next = node->next;
		if (node->ddPalette) {
			g_std3DReleaseRefCount = (int)node->ddPalette->lpVtbl->Release(node->ddPalette);
		}
		Memory_FreeTagged("D3DPALETTE8", node);
		node = next;
	}
	g_pPaletteListHead = NULL;
	g_pPaletteListTail = NULL;
}

// FUNCTION: XWA 0x596F89
static char std3D_CopyTextureFromSource(IDirectDrawSurface* pSurface, Std3DVBuffer* pVBuffer,
										const uint8_t* alphaPlane, Std3DTextureFormatMode textureFormatMode,
										unsigned int dstWidth, unsigned int dstHeight, unsigned int srcWidth,
										unsigned int srcHeight, Std3DTexFmt* dstFormat,
										ColorInfo* srcColorInfo) {
	DDSURFACEDESC desc;
	unsigned int x;
	unsigned int y;
	uint16_t* src16;
	uint16_t* dst;
	const uint8_t* alpha;
	uint8_t color;
	uint16_t value;

	((void* (*)(void*, int, size_t))memset)(&desc, 0, sizeof(desc));
	desc.dwSize = 108;
	{
		int result;

		result = pSurface->lpVtbl->Lock(pSurface, NULL, &desc, 1, NULL);
		if (result) {
			DebugPrintf("Error %s when locking the DDSurface source buffer.\n",
						std3D_GetD3DErrorString(result), 0, 0, 0);
			return 0;
		}

		switch ((int)pVBuffer->raster.sourceType) {
			case 0: {
				uint8_t* src;

				std3D_LockVBuffer(pVBuffer);
				if (textureFormatMode == STD3D_TEXFMT_PAL8) {
					uint8_t* dst8;

					for (y = 0; y < dstHeight; ++y) {
						src = (uint8_t*)pVBuffer->pixels + y * pVBuffer->raster.rowPitch;
						dst8 = (uint8_t*)desc.lpSurface + desc.lPitch * y;
						for (x = 0; x < dstWidth; ++x) {
							*dst8++ = *src++;
						}
					}
				} else if (textureFormatMode == STD3D_TEXFMT_RGBA1555) {
					if (g_pStd3DCurDevice->caps.bAlphaTexture) {
						for (y = 0; y < dstHeight; ++y) {
							src = (uint8_t*)pVBuffer->pixels + y * pVBuffer->raster.rowPitch;
							dst = (uint16_t*)((uint8_t*)desc.lpSurface + desc.lPitch * y);
							for (x = 0; x < dstWidth; ++x) {
								*dst++ = g_texConvBuf1555[*src++];
							}
						}
					} else {
						for (y = 0; y < dstHeight; ++y) {
							src = (uint8_t*)pVBuffer->pixels + y * pVBuffer->raster.rowPitch;
							dst = (uint16_t*)((uint8_t*)desc.lpSurface + desc.lPitch * y);
							for (x = 0; x < dstWidth; ++x) {
								*dst++ = g_std3DPaletteScratch16[*src++];
							}
						}
					}
				} else if (textureFormatMode == STD3D_TEXFMT_RGBA4444) {
					if (alphaPlane) {
						uint8_t alphaValue;
						uint16_t texel;

						for (y = 0; y < dstHeight; ++y) {
							src = (uint8_t*)pVBuffer->pixels + y * pVBuffer->raster.rowPitch;
							dst = (uint16_t*)((uint8_t*)desc.lpSurface + desc.lPitch * y);
							if (y >= srcHeight) {
								alpha = alphaPlane + (y % srcHeight) * srcWidth;
							} else {
								alpha = alphaPlane + y * srcWidth;
							}
							for (x = 0; x < dstWidth; ++x) {
								texel = g_texConvBuf4444[*src++];
								if (x >= srcWidth) {
									alpha -= srcWidth;
								}
								alphaValue = *alpha++;
								texel |=
									(uint16_t)((alphaValue >> g_pFmtRGBA4444->colorInfo.alphaPosShiftRight)
											   << g_pFmtRGBA4444->colorInfo.alphaPosShift);
								*dst++ = texel;
							}
						}
					} else {
						for (y = 0; y < dstHeight; ++y) {
							src = (uint8_t*)pVBuffer->pixels + y * pVBuffer->raster.rowPitch;
							dst = (uint16_t*)((uint8_t*)desc.lpSurface + desc.lPitch * y);
							for (x = 0; x < dstWidth; ++x) {
								*dst++ = g_texConvBuf4444[*src++];
							}
						}
					}
				} else {
					for (y = 0; y < dstHeight; ++y) {
						src = (uint8_t*)pVBuffer->pixels + y * pVBuffer->raster.rowPitch;
						dst = (uint16_t*)((uint8_t*)desc.lpSurface + desc.lPitch * y);
						for (x = 0; x < dstWidth; ++x) {
							*dst++ = g_std3DPaletteScratch16[*src++];
						}
					}
				}
				std3D_UnlockVBuffer(pVBuffer);
				break;
			}

			case 1:
			case 2: {
				Std3DTexFmt* format;

				format = dstFormat;

				std3D_LockVBuffer(pVBuffer);
				for (y = 0; y < dstHeight; ++y) {
					src16 = (uint16_t*)((uint8_t*)pVBuffer->pixels + y * pVBuffer->raster.rowPitch);
					dst = (uint16_t*)((uint8_t*)desc.lpSurface + desc.lPitch * y);
					for (x = 0; x < dstWidth; ++x) {
						color = (uint8_t)((src16[x] >> srcColorInfo->redPosShift)
										  << srcColorInfo->redPosShiftRight);
						value = (uint16_t)((color >> format->colorInfo.redPosShiftRight)
										   << format->colorInfo.redPosShift);
						color = (uint8_t)((src16[x] >> srcColorInfo->greenPosShift)
										  << srcColorInfo->greenPosShiftRight);
						value |= (uint16_t)((color >> format->colorInfo.greenPosShiftRight)
											<< format->colorInfo.greenPosShift);
						color = (uint8_t)((src16[x] >> srcColorInfo->bluePosShift)
										  << srcColorInfo->bluePosShiftRight);
						value |= (uint16_t)((color >> format->colorInfo.bluePosShiftRight)
											<< format->colorInfo.bluePosShift);
						if (textureFormatMode == STD3D_TEXFMT_RGBA1555) {
							if (src16[x] == pVBuffer->transparentColor) {
								color = 0;
							} else {
								color = 0xff;
							}
						} else {
							color = (uint8_t)((src16[x] >> srcColorInfo->alphaPosShift)
											  << srcColorInfo->alphaPosShiftRight);
						}
						value |= (uint16_t)((color >> format->colorInfo.alphaPosShiftRight)
											<< format->colorInfo.alphaPosShift);
						*dst++ = value;
					}
				}
				std3D_UnlockVBuffer(pVBuffer);
				break;
			}

			default:
				DebugPrintf("unknown sourcebuf type in std3D_CopyTextureFromSource:",
							pVBuffer->raster.sourceType);
				break;
		}

		result = pSurface->lpVtbl->Unlock(pSurface, NULL);
		if (result) {
			DebugPrintf("Error %s when unlocking the DDSurface source buffer.\n",
						std3D_GetD3DErrorString(result), 0, 0, 0);
			return 0;
		}
	}
	return 1;
}

// FUNCTION: XWA 0x5960BE
char std3D_CreateMipSurface(Std3DTextureSurface** apOutSurfaces, Std3DVBuffer** apSrcVBuffers,
							Std3DTextureFormatMode textureFormatMode, void** apSrcAlphaPlanes,
							int mipLevelCount, char useHardwareMipmaps) {
	IDirect3DTexture* d3dTextures[6];
	{
		unsigned int scaledHeight;
		DDSURFACEDESC surfaceDesc;
		unsigned int dstHeight;
		{
			Std3DTextureSurface* outSurface;
			{
				IDirectDrawSurface* createdSurfaces[6];
				Std3DVBuffer* scaledVBuffer;
				unsigned int totalTextureBytes;
				Std3DTexFmt* texFmt;
				ColorInfo* srcColorInfo;
				IDirectDrawPalette* ddPalette;
				int mipLevel;
				unsigned int dstWidth;
				unsigned int srcHeight;
				HRESULT result;

				createdSurfaces[0] = NULL;
				createdSurfaces[1] = NULL;
				createdSurfaces[2] = NULL;
				createdSurfaces[3] = NULL;
				createdSurfaces[4] = NULL;
				createdSurfaces[5] = NULL;
				d3dTextures[0] = NULL;
				d3dTextures[1] = NULL;
				d3dTextures[2] = NULL;
				d3dTextures[3] = NULL;
				d3dTextures[4] = NULL;
				d3dTextures[5] = NULL;
				scaledVBuffer = NULL;
				totalTextureBytes = 0;
				texFmt = NULL;
				srcColorInfo = NULL;
				ddPalette = NULL;

				if (textureFormatMode == STD3D_TEXFMT_RGBA4444) {
					texFmt = g_pFmtRGBA4444;
					srcColorInfo = &g_cfRGBA4444;
				} else if (textureFormatMode == STD3D_TEXFMT_RGB565) {
					texFmt = g_pFmtRGB565;
					srcColorInfo = &g_cfRGB565;
				} else if (textureFormatMode == STD3D_TEXFMT_PAL8) {
					texFmt = g_pFmtPal8;
					srcColorInfo = &g_cfPal8;
					ddPalette = (IDirectDrawPalette*)g_pPaletteListTail->ddPalette;
					if (!ddPalette) {
						DebugPrintf("No palette for 8-bit texture!\n", 0, 0, 0, 0);
						goto fail;
					}
				} else {
					texFmt = g_pFmtRGBA1555;
					srcColorInfo = &g_cfRGB565;
				}

				for (mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
					unsigned int srcWidth;

					dstWidth = srcWidth = apSrcVBuffers[mipLevel]->raster.width >= 1
											  ? (apSrcVBuffers[mipLevel]->raster.width <= 0x100
													 ? apSrcVBuffers[mipLevel]->raster.width
													 : 256)
											  : 1;
					dstHeight = srcHeight = apSrcVBuffers[mipLevel]->raster.height >= 1
												? (apSrcVBuffers[mipLevel]->raster.height <= 0x100
													   ? apSrcVBuffers[mipLevel]->raster.height
													   : 256)
												: 1;

					/* Scale-up path: pad below the device minimum, or square out for a
					 * square-only device, by tiling the source. */
					if (dstWidth < g_std3DMinTextureWidth || dstHeight < g_std3DMinTextureHeight ||
						(g_pStd3DCurDevice->caps.bSquareOnlyTexture && dstWidth != dstHeight)) {
						Std3DRasterInfo rasterInfo;
						unsigned int scaledWidth;
						unsigned int xTileCount;
						unsigned int yTileCount;
						unsigned int tileX;
						unsigned int tileY;

						memcpy_0(&rasterInfo, &apSrcVBuffers[mipLevel]->raster, sizeof(rasterInfo));
						if (g_pStd3DCurDevice->caps.bSquareOnlyTexture && dstWidth != dstHeight) {
							scaledWidth = dstWidth > dstHeight ? dstWidth : dstHeight;
							scaledHeight = scaledWidth;
						} else {
							scaledWidth =
								g_std3DMinTextureWidth > dstWidth ? g_std3DMinTextureWidth : dstWidth;
							scaledHeight =
								g_std3DMinTextureHeight > dstHeight ? g_std3DMinTextureHeight : dstHeight;
						}
						xTileCount = (unsigned int)((double)scaledWidth / (double)(int)dstWidth + 0.5);
						yTileCount = (unsigned int)((double)scaledHeight / (double)(int)dstHeight + 0.5);
						rasterInfo.width *= xTileCount;
						rasterInfo.height *= yTileCount;
						rasterInfo.tileFactor = xTileCount * yTileCount;
						scaledVBuffer = std3D_AllocVBuffer(&rasterInfo, 0, 0, 0);
						for (tileY = 0; tileY < yTileCount; ++tileY) {
							for (tileX = 0; tileX < xTileCount; ++tileX) {
								std3D_BlitVBuffer(scaledVBuffer, apSrcVBuffers[mipLevel],
												  (int)(dstWidth * tileX), (int)(dstHeight * tileY), 0, 1);
							}
						}
						dstWidth = rasterInfo.width;
						dstHeight = rasterInfo.height;
						apSrcVBuffers[mipLevel] = scaledVBuffer;
					}

					memcpy_0(&surfaceDesc, &texFmt->ddsd, sizeof(surfaceDesc));
					/* Original allocates 0xB0 (176) bytes; use sizeof so the wider 64-bit
					 * pointers in Std3DTextureSurface/Std3DTexCacheNode are accounted for. */
					outSurface = (Std3DTextureSurface*)Memory_AllocTagged("T3DTEXTSURFACE",
																		  sizeof(Std3DTextureSurface));
					((void* (*)(void*, int, size_t))memset)(outSurface, 0, sizeof(Std3DTextureSurface));
					apOutSurfaces[mipLevel] = outSurface;
					outSurface->mipLevelIndex = (uint32_t)mipLevel;
					outSurface->mipLevelCount = (uint32_t)mipLevelCount;

					if (useHardwareMipmaps) {
						/* One complex surface owns the whole mip chain; the remaining levels are
						 * reached with GetAttachedSurface. Only built at level 0. */
						if (mipLevel == 0) {
							DDSCAPS attachedCaps;
							IDirectDrawSurface* parent = NULL;
							int mipIndex;

							surfaceDesc.dwSize = 108;
							surfaceDesc.dwFlags =
								DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT;
							surfaceDesc.ddsCaps.dwCaps =
								DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY | DDSCAPS_MIPMAP | DDSCAPS_COMPLEX;
							surfaceDesc.dwWidth = dstWidth;
							surfaceDesc.dwHeight = dstHeight;
							surfaceDesc.dwMipMapCount = (uint32_t)mipLevelCount;
							surfaceDesc.lPitch = (int)apSrcVBuffers[0]->raster.rowPitch;
							result = g_std3DDirectDraw->lpVtbl->CreateSurface(g_std3DDirectDraw, &surfaceDesc,
																			  &createdSurfaces[0], NULL);
							if (result) {
								DebugPrintf("Error %s creating DirectDraw hardware mip surface.\n",
											std3D_GetD3DErrorString(result), 0, 0, 0);
								createdSurfaces[0] = NULL;
								goto fail;
							}
							((void* (*)(void*, int, size_t))memset)(&attachedCaps, 0, sizeof(attachedCaps));
							attachedCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY | DDSCAPS_COMPLEX;
							parent = createdSurfaces[0];
							for (mipIndex = 1; mipIndex < mipLevelCount; ++mipIndex) {
								int debugResult;

								result = parent->lpVtbl->GetAttachedSurface(parent, &attachedCaps,
																			&createdSurfaces[mipIndex]);
								if (result) {
									DebugPrintf("Error %s retrieving DirectDraw hardware mip surface. \n",
												std3D_GetD3DErrorString(result), 0, 0, 0);
									goto fail;
								}
								g_std3DReleaseRefCount = (int)createdSurfaces[mipIndex]->lpVtbl->Release(
									createdSurfaces[mipIndex]);
								if (!g_std3DReleaseRefCount) {
									debugResult = DebugPrintf("DX object released too early, refcount is 0");
								} else {
									debugResult = 0;
								}
								(void)debugResult;
								parent = createdSurfaces[mipIndex];
							}
						}
						outSurface->bHardwareMipmap = 1;
					} else {
						/* System-memory source surface in the destination texel format. The RIVA
						 * quirk path (riva.txt) requests a single-level mipmap-capable surface. */
						surfaceDesc.dwSize = 108;
						if (g_FlightConfRivaTxt) {
							surfaceDesc.dwFlags = 0x21007; /* CAPS|HEIGHT|WIDTH|PIXELFORMAT|MIPMAPCOUNT */
							surfaceDesc.ddsCaps.dwCaps = 0x401808; /* MIPMAP|TEXTURE|SYSTEMMEMORY|COMPLEX */
							surfaceDesc.dwMipMapCount = 1;
						} else {
							surfaceDesc.dwFlags = 0x1007; /* CAPS|HEIGHT|WIDTH|PIXELFORMAT */
							surfaceDesc.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;
						}
						surfaceDesc.dwWidth = dstWidth;
						surfaceDesc.dwHeight = dstHeight;
						surfaceDesc.lPitch = (int)apSrcVBuffers[mipLevel]->raster.rowPitch;
						result = g_std3DDirectDraw->lpVtbl->CreateSurface(g_std3DDirectDraw, &surfaceDesc,
																		  &createdSurfaces[mipLevel], NULL);
						if (result) {
							DebugPrintf("Error %s when creating the DirectDraw source surface.\n",
										std3D_GetD3DErrorString(result), 0, 0, 0);
							createdSurfaces[mipLevel] = NULL;
							goto fail;
						}
						outSurface->bHardwareMipmap = 0;
					}

					if (textureFormatMode == STD3D_TEXFMT_PAL8) {
						result = createdSurfaces[mipLevel]->lpVtbl->SetPalette(createdSurfaces[mipLevel],
																			   ddPalette);
						if (result) {
							DebugPrintf("Error %s setting texture palette.\n",
										std3D_GetD3DErrorString(result), 0, 0, 0);
							goto fail;
						}
					}

					if (!std3D_CopyTextureFromSource(createdSurfaces[mipLevel], apSrcVBuffers[mipLevel],
													 (const uint8_t*)apSrcAlphaPlanes[mipLevel],
													 textureFormatMode, dstWidth, dstHeight, srcWidth,
													 srcHeight, texFmt, srcColorInfo)) {
						DebugPrintf("Failed to copy texture source surface.\n", 0, 0, 0, 0);
						goto fail;
					}

					if (textureFormatMode == STD3D_TEXFMT_RGBA1555 &&
						!g_pStd3DCurDevice->caps.bAlphaTexture) {
						DDCOLORKEY colorKey;

						switch (apSrcVBuffers[mipLevel]->raster.sourceType) {
							case 0:
								colorKey.dwColorSpaceLowValue = g_std3DPaletteScratch16[0];
								colorKey.dwColorSpaceHighValue = g_std3DPaletteScratch16[0];
								break;

							case 1:
								colorKey.dwColorSpaceLowValue = apSrcVBuffers[mipLevel]->transparentColor;
								colorKey.dwColorSpaceHighValue = apSrcVBuffers[mipLevel]->transparentColor;
								break;
						}
						createdSurfaces[mipLevel]->lpVtbl->SetColorKey(createdSurfaces[mipLevel],
																	   DDCKEY_SRCBLT, &colorKey);
					}

					if (scaledVBuffer) {
						std3D_FreeVBuffer(scaledVBuffer);
						scaledVBuffer = NULL;
					}

					/* Only level 0 owns the Direct3D texture for a hardware mip chain. */
					if (!useHardwareMipmaps || mipLevel == 0) {
						result = createdSurfaces[mipLevel]->lpVtbl->QueryInterface(
							createdSurfaces[mipLevel], &IID_IDirect3DTexture_Compat,
							(void**)&d3dTextures[mipLevel]);
						if (result) {
							DebugPrintf("Error %s creating Direct3D source texture.\n",
										std3D_GetD3DErrorString(result), 0, 0, 0);
							goto fail;
						}
					}
					result = createdSurfaces[mipLevel]->lpVtbl->GetSurfaceDesc(createdSurfaces[mipLevel],
																			   &surfaceDesc);
					if (result) {
						DebugPrintf("Error %s get surface description.\n", std3D_GetD3DErrorString(result), 0,
									0, 0);
						goto fail;
					}
					/* Caps of the cached (video-memory) destination surface. */
					if (useHardwareMipmaps || g_FlightConfRivaTxt) {
						surfaceDesc.dwFlags = 0x21007; /* CAPS|HEIGHT|WIDTH|PIXELFORMAT|MIPMAPCOUNT */
						surfaceDesc.ddsCaps.dwCaps = DDSCAPS_ALLOCONLOAD | DDSCAPS_MIPMAP |
													 DDSCAPS_VIDEOMEMORY | DDSCAPS_TEXTURE | DDSCAPS_COMPLEX;
					} else {
						surfaceDesc.dwFlags = 0x1007; /* CAPS|HEIGHT|WIDTH|PIXELFORMAT */
						surfaceDesc.ddsCaps.dwCaps =
							DDSCAPS_ALLOCONLOAD | DDSCAPS_VIDEOMEMORY | DDSCAPS_TEXTURE;
					}

					if (!useHardwareMipmaps || mipLevel == 0) {
						outSurface->pSrcSurface = createdSurfaces[mipLevel];
						outSurface->pSrcTexture = d3dTextures[mipLevel];
						outSurface->paletteHandle = ddPalette;
					}
					memcpy_0(&outSurface->cacheNode.ddsd, &surfaceDesc, sizeof(outSurface->cacheNode.ddsd));
					outSurface->cacheNode.texWidth = apSrcVBuffers[mipLevel]->raster.width;
					outSurface->cacheNode.texHeight = apSrcVBuffers[mipLevel]->raster.height;
					outSurface->cacheNode.byteSize =
						apSrcVBuffers[mipLevel]->raster.width * apSrcVBuffers[mipLevel]->raster.height * 2;
					totalTextureBytes += outSurface->cacheNode.byteSize;
					outSurface->cacheNode.bCached = 0;
					outSurface->bAllocated = 1;
				}

				if (useHardwareMipmaps && apOutSurfaces[0]) {
					apOutSurfaces[0]->cacheNode.byteSize = totalTextureBytes;
				}
				g_texSurfaceCount += mipLevelCount;
				return 1;

			fail:
				if (scaledVBuffer) {
					std3D_FreeVBuffer(scaledVBuffer);
					scaledVBuffer = NULL;
				}
				for (mipLevel = 0; mipLevel < 6; ++mipLevel) {
					if (d3dTextures[mipLevel]) {
						int debugResult;

						g_std3DReleaseRefCount =
							(int)d3dTextures[mipLevel]->lpVtbl->Release(d3dTextures[mipLevel]);
						if (g_std3DReleaseRefCount != 1) {
							debugResult = DebugPrintf("DX object release returned unexpected refcount:",
													  g_std3DReleaseRefCount);
						} else {
							debugResult = 0;
						}
						(void)debugResult;
						d3dTextures[mipLevel] = NULL;
					}
					if (createdSurfaces[mipLevel]) {
						/* Attached hardware-mip levels (>0) are owned by level 0's chain. */
						if (!useHardwareMipmaps || mipLevel == 0) {
							int debugResult;

							g_std3DReleaseRefCount =
								(int)createdSurfaces[mipLevel]->lpVtbl->Release(createdSurfaces[mipLevel]);
							if (g_std3DReleaseRefCount) {
								debugResult = DebugPrintf("DX object not released properly, refcount:",
														  g_std3DReleaseRefCount);
							} else {
								debugResult = 0;
							}
							(void)debugResult;
						}
						createdSurfaces[mipLevel] = NULL;
					}
					if (apOutSurfaces[mipLevel]) {
						Memory_FreeTagged("T3DTEXTSURFACE", apOutSurfaces[mipLevel]);
						apOutSurfaces[mipLevel] = NULL;
					}
				}
				return 0;
			}
		}
	}
}
