#include "xwa/render/renderer_internal.h"
#include "xwa/render/std3d_device.h"

// GLOBAL: XWA 0x5ABC30
const float g_std3DFrameIncrement = 1.0f;

int Renderer_FlushTextureCacheAndReturnTrue(void) {
	std3D_FlushTextureCache();
	return 1;
}

/* std3D_FreePalettes @0x593E0D and std3D_CreatePaletteForTexture @0x593E7B live in
 * std3d_texture.c with the palette list. */

// FLAGS: /O2 /Og- /Oi-
// FUNCTION: XWA 0x593610
void std3D_LockVBuffer(Std3DVBuffer* pVBuffer) {
	if (pVBuffer->storageType == 1 && pVBuffer->lockCount == 0) {
		struct {
			HRESULT result;
			DDSURFACEDESC desc;
		} lock;

		memset(&lock.desc, 0, sizeof(lock.desc));
		lock.desc.dwSize = sizeof(lock.desc);
		lock.result = ((IDirectDrawSurface*)pVBuffer->ddSurface)
						  ->lpVtbl->Lock((IDirectDrawSurface*)pVBuffer->ddSurface, NULL, &lock.desc, 1, NULL);
		if (lock.result) {
			DebugPrintf("Error %x locking buffer %x, surface %x\n", lock.result, pVBuffer,
						pVBuffer->ddSurface);
			return;
		}
		pVBuffer->pixels = lock.desc.lpSurface;
		pVBuffer->raster.rowPitch = lock.desc.lPitch;
	}

	++pVBuffer->lockCount;
}

// FUNCTION: XWA 0x5936A6
void std3D_UnlockVBuffer(Std3DVBuffer* pVBuffer) {
	if ((unsigned int)pVBuffer->lockCount < 1) {
		DebugPrintf("Unlock Warning: buffer %x, not locked\n", pVBuffer);
		return;
	}

	if (pVBuffer->lockCount == 1 && pVBuffer->storageType == 1) {
		HRESULT result;

		result = ((IDirectDrawSurface*)pVBuffer->ddSurface)
					 ->lpVtbl->Unlock((IDirectDrawSurface*)pVBuffer->ddSurface, pVBuffer->pixels);
		if (result) {
			DebugPrintf("Error %x unlocking buffer %x, surface %x\n", result, pVBuffer, pVBuffer->ddSurface);
			return;
		}
	}

	--pVBuffer->lockCount;
}

// FUNCTION: XWA 0x59372D
void std3D_BlitVBuffer(Std3DVBuffer* pDst, Std3DVBuffer* pSrc, int dstX, int dstY, int srcX, int srcY) {
	uint8_t* srcRow;
	uint8_t* dstRow;
	size_t rowBytes;
	unsigned int row;

	(void)srcX;
	(void)srcY;

	std3D_LockVBuffer(pDst);
	std3D_LockVBuffer(pSrc);

	srcRow = (uint8_t*)pSrc->pixels;
	dstRow =
		(uint8_t*)pDst->pixels + pDst->raster.rowPitch * dstY + ((pDst->raster.bitsPerPixel >> 3) * dstX);
	rowBytes = (size_t)((pSrc->raster.bitsPerPixel >> 3) * pSrc->raster.width);

	for (row = 0; row < pSrc->raster.height; ++row) {
		memcpy(dstRow, srcRow, rowBytes);
		dstRow += pDst->raster.rowPitch;
		srcRow += pSrc->raster.rowPitch;
	}

	std3D_UnlockVBuffer(pDst);
	std3D_UnlockVBuffer(pSrc);
}

// FUNCTION: XWA 0x593E91
void* std3D_GetZBufferSurface(void) { return g_std3DZBufferSurface.surface; }

// FUNCTION: XWA 0x593964
uint16_t* std3D_CopyPaletteToScratch16(const uint16_t* palette, int colorCount) {
	memcpy(g_std3DPaletteScratch16, palette, (size_t)(2 * colorCount));
	return g_std3DPaletteScratch16;
}

// FUNCTION: XWA 0x593985
uint16_t* std3D_ConvertTexTo1555(uint16_t* pSrc565, uint16_t colorKey, int count) {
	int i;
	ColorInfo* srcFmt;
	ColorInfo* dstFmt;

	if (g_pFmtRGBA1555 == g_pFmtRGB565) {
		memcpy(g_texConvBuf1555, pSrc565, (size_t)(2 * count));
	} else {
		srcFmt = &g_pFmtRGB565->colorInfo;
		dstFmt = &g_pFmtRGBA1555->colorInfo;

		for (i = 0; i < count; ++i) {
			uint8_t channel;

			channel = (uint8_t)((pSrc565[i] >> srcFmt->redPosShift) << srcFmt->redPosShiftRight);
			g_texConvBuf1555[i] = (uint16_t)((channel >> dstFmt->redPosShiftRight) << dstFmt->redPosShift);

			channel = (uint8_t)((pSrc565[i] >> srcFmt->greenPosShift) << srcFmt->greenPosShiftRight);
			g_texConvBuf1555[i] |=
				(uint16_t)((channel >> dstFmt->greenPosShiftRight) << dstFmt->greenPosShift);

			channel = (uint8_t)((pSrc565[i] >> srcFmt->bluePosShift) << srcFmt->bluePosShiftRight);
			g_texConvBuf1555[i] |= (uint16_t)((channel >> dstFmt->bluePosShiftRight) << dstFmt->bluePosShift);

			if (i != 0) {
				if (pSrc565[i] == colorKey) {
					channel = 0;
				} else {
					channel = 0xff;
				}
				g_texConvBuf1555[i] |=
					(uint16_t)((channel >> dstFmt->alphaPosShiftRight) << dstFmt->alphaPosShift);
			}
		}
	}

	return g_texConvBuf1555;
}

// FUNCTION: XWA 0x593B31
uint16_t* std3D_ConvertTexTo4444(uint16_t* pSrc565, const uint8_t* pAlpha, int count) {
	int i;
	ColorInfo* srcFmt;
	ColorInfo* dstFmt;

	if (g_pFmtRGBA4444 == g_pFmtRGB565) {
		memcpy(g_texConvBuf4444, pSrc565, (size_t)(2 * count));
	} else {
		srcFmt = &g_pFmtRGB565->colorInfo;
		dstFmt = &g_pFmtRGBA4444->colorInfo;

		for (i = 0; i < count; ++i) {
			uint8_t channel;

			channel = (uint8_t)((pSrc565[i] >> srcFmt->redPosShift) << srcFmt->redPosShiftRight);
			g_texConvBuf4444[i] = (uint16_t)((channel >> dstFmt->redPosShiftRight) << dstFmt->redPosShift);

			channel = (uint8_t)((pSrc565[i] >> srcFmt->greenPosShift) << srcFmt->greenPosShiftRight);
			g_texConvBuf4444[i] |=
				(uint16_t)((channel >> dstFmt->greenPosShiftRight) << dstFmt->greenPosShift);

			channel = (uint8_t)((pSrc565[i] >> srcFmt->bluePosShift) << srcFmt->bluePosShiftRight);
			g_texConvBuf4444[i] |= (uint16_t)((channel >> dstFmt->bluePosShiftRight) << dstFmt->bluePosShift);

			if (i != 0 && pAlpha != NULL) {
				channel = pAlpha[i];
				g_texConvBuf4444[i] |=
					(uint16_t)((channel >> dstFmt->alphaPosShiftRight) << dstFmt->alphaPosShift);
			}
		}
	}

	return g_texConvBuf4444;
}

// FUNCTION: XWA 0x597DBE
static void std3D_CacheListAppend(Std3DTexCacheNode* node) {
	if (g_pTexCacheHead == NULL) {
		g_pTexCacheTail = node;
		g_pTexCacheHead = g_pTexCacheTail;
		node->pPrev = NULL;
		node->pNext = NULL;
	} else {
		g_pTexCacheTail->pNext = node;
		node->pPrev = g_pTexCacheTail;
		node->pNext = NULL;
		g_pTexCacheTail = node;
	}
	++g_texCacheCount;
	g_pStd3DCurDevice->availableMemory -= node->byteSize;
}

// FUNCTION: XWA 0x597E5F
static void std3D_CacheListRemove(Std3DTexCacheNode* node) {
	if (node == g_pTexCacheHead) {
		g_pTexCacheHead = node->pNext;
		if (g_pTexCacheHead) {
			g_pTexCacheHead->pPrev = NULL;
			if (!g_pTexCacheHead->pNext) {
				g_pTexCacheTail = g_pTexCacheHead;
			}
		} else {
			g_pTexCacheTail = NULL;
		}
	} else if (node == g_pTexCacheTail) {
		g_pTexCacheTail = node->pPrev;
		g_pTexCacheTail->pNext = NULL;
	} else {
		node->pPrev->pNext = node->pNext;
		node->pNext->pPrev = node->pPrev;
	}

	node->pPrev = NULL;
	node->pNext = NULL;
	--g_texCacheCount;
	g_pStd3DCurDevice->availableMemory += node->byteSize;
}

// FUNCTION: XWA 0x59786E
static char std3D_CacheTextureSurface(Std3DTextureSurface* surf) {
	Std3DTexCacheNode* node;
	Std3DTexCacheNode* next;
	D3DTEXTUREHANDLE handle;
	uint32_t needed;
	HRESULT hr;
	uint32_t freed;
	IDirectDrawSurface* destSurface;
	IDirect3DTexture* destTexture;

	destSurface = NULL;
	destTexture = NULL;

	if ((signed char)surf->cacheNode.bCached) {
		DebugPrintf("texture surface is being cached twice");
	}
	if ((signed char)surf->bHardwareMipmap && surf->mipLevelIndex) {
		DebugPrintf("hardware mipmapped textures other than level 0 are being cached!");
	}

	hr = g_std3DDirectDraw->lpVtbl->CreateSurface(g_std3DDirectDraw, (DDSURFACEDESC*)surf->cacheNode.ddsd,
												  &destSurface, NULL);
	if (hr) {
		if (hr != DX_DDERR_OUTOFVIDEOMEMORY) {
			DebugPrintf("Error %s creating texture surface.\n", std3D_GetD3DErrorString(hr), 0, 0, 0);
			goto fail;
		}

		/* Purge least-recently-used textures until the new one fits, then retry. */
		g_bTexCreateFailed = 1;
		DebugPrintf("Error %s Creating surface.\n", std3D_GetD3DErrorString(hr), 0, 0, 0);
		DebugPrintf("Assuming texture ram overflow - PURGING.\n", 0, 0, 0, 0);
		node = g_pTexCacheHead;
		needed = surf->cacheNode.byteSize;
		while (1) {
			freed = 0;

			while (freed < needed && node && node->cacheFrameTag != (uint32_t)g_std3DOpened) {
				next = node->pNext;
				freed += node->byteSize;
				g_frameBytesPurged += (int)node->byteSize;
				std3D_UncacheTexture(node);
				node = next;
			}
			if (freed < needed) {
				DebugPrintf("WARNING: Scene texture overflow occurred!!!.\n", 0, 0, 0, 0);
				destSurface = NULL;
				g_bTexCacheOverflow = 1;
				goto fail;
			}
			hr = g_std3DDirectDraw->lpVtbl->CreateSurface(
				g_std3DDirectDraw, (DDSURFACEDESC*)surf->cacheNode.ddsd, &destSurface, NULL);
			if (!hr) {
				DebugPrintf("Success adding new texture after purge.\n", 0, 0, 0, 0);
				break;
			}
			if (hr != DX_DDERR_OUTOFVIDEOMEMORY) {
				DebugPrintf("Error %s creating texture surface.\n", std3D_GetD3DErrorString(hr), 0, 0, 0);
				goto fail;
			}
			DebugPrintf("WARNING: Texture cache purging failed initial loop - trying again.\n", 0, 0, 0, 0);
		}
	}

	if (surf->paletteHandle) {
		hr = destSurface->lpVtbl->SetPalette(destSurface, (IDirectDrawPalette*)surf->paletteHandle);
		if (hr) {
			DebugPrintf("Error %s setting hardware texture palette.\n", std3D_GetD3DErrorString(hr), 0, 0, 0);
			goto fail;
		}
	}
	hr = destSurface->lpVtbl->QueryInterface(destSurface, &IID_IDirect3DTexture_Compat, (void**)&destTexture);
	if (hr) {
		DebugPrintf("Error %s creating Direct3D dest texture.\n", std3D_GetD3DErrorString(hr), 0, 0, 0);
		destTexture = NULL;
		goto fail;
	}
	hr = destTexture->lpVtbl->GetHandle(destTexture, g_d3dDevice, &handle);
	if (hr) {
		DebugPrintf("Error %s when getting texture handle.\n", std3D_GetD3DErrorString(hr), 0, 0, 0);
		handle = 0;
		goto fail;
	}
	hr = destTexture->lpVtbl->Load(destTexture, (IDirect3DTexture*)surf->pSrcTexture);
	if (hr) {
		DebugPrintf("Error %s loading Direct3D dest texture from source.\n", std3D_GetD3DErrorString(hr), 0,
					0, 0);
		goto fail;
	}

	surf->cacheNode.pCachedTexture = destTexture;
	destTexture = NULL;
	surf->cacheNode.pCachedSurface = destSurface;
	destSurface = NULL;
	surf->cacheNode.texHandle = handle;
	handle = 0;
	surf->cacheNode.bCached = 1;
	surf->cacheNode.cacheFrameTag = (uint32_t)g_std3DOpened;
	std3D_CacheListAppend(&surf->cacheNode);
	g_frameBytesCached += (int)surf->cacheNode.byteSize;
	return 1;

fail:
	if (destTexture) {
		int debugResult;

		g_std3DReleaseRefCount = (int)destTexture->lpVtbl->Release(destTexture);
		if (g_std3DReleaseRefCount != 1) {
			debugResult =
				DebugPrintf("DX object release returned unexpected refcount:", g_std3DReleaseRefCount);
		} else {
			debugResult = 0;
		}
		(void)debugResult;
	}
	if (destSurface) {
		int debugResult;

		g_std3DReleaseRefCount = (int)destSurface->lpVtbl->Release(destSurface);
		if (g_std3DReleaseRefCount) {
			debugResult = DebugPrintf("DX object not released properly, refcount:", g_std3DReleaseRefCount);
		} else {
			debugResult = 0;
		}
		(void)debugResult;
	}
	DebugPrintf("Done error exit from std3D_CacheTextureSurface.\n", 0, 0, 0, 0);
	return 0;
}

// FUNCTION: XWA 0x597784
void std3D_AddToTextureCache(Std3DTextureSurface* surf) {
	Std3DTexCacheNode* node;

	if (surf->cacheNode.bCached) {
		node = &surf->cacheNode;
		node->cacheFrameTag = (uint32_t)g_std3DOpened;
		std3D_CacheListRemove(node);
		std3D_CacheListAppend(node);
		g_frameBytesCached += (int)node->byteSize;
	} else {
		std3D_CacheTextureSurface(surf);
	}
}

// FUNCTION: XWA 0x597CC5
void std3D_UncacheTextureSurface(Std3DTextureSurface* surf) { std3D_UncacheTexture(&surf->cacheNode); }

// FUNCTION: XWA 0x597CD9
void std3D_UncacheTexture(Std3DTexCacheNode* node) {
	if (!node->bCached) {
		return;
	}

	node->texHandle = 0;
	if (node->pCachedTexture) {
		int debugResult;

		g_std3DReleaseRefCount = (int)((IDirect3DTexture*)node->pCachedTexture)
									 ->lpVtbl->Release((IDirect3DTexture*)node->pCachedTexture);
		if (g_std3DReleaseRefCount != 1) {
			debugResult =
				DebugPrintf("DX object release returned unexpected refcount:", g_std3DReleaseRefCount);
		} else {
			debugResult = 0;
		}
		(void)debugResult;
		node->pCachedTexture = NULL;
	}
	if (node->pCachedSurface) {
		int debugResult;

		g_std3DReleaseRefCount = (int)((IDirectDrawSurface*)node->pCachedSurface)
									 ->lpVtbl->Release((IDirectDrawSurface*)node->pCachedSurface);
		if (g_std3DReleaseRefCount != 0) {
			debugResult = DebugPrintf("DX object not released properly, refcount:", g_std3DReleaseRefCount);
		} else {
			debugResult = 0;
		}
		(void)debugResult;
		node->pCachedSurface = NULL;
	}
	std3D_CacheListRemove(node);
	node->bCached = 0;
	node->cacheFrameTag = 0;
}

// FUNCTION: XWA 0x5977ED
void std3D_FlushTextureCache(void) {
	Std3DTexCacheNode* next;

	{
		Std3DTexCacheNode* node;

		node = g_pTexCacheHead;
		while (node) {
			next = node->pNext;
			std3D_UncacheTexture(node);
			node = next;
		}
	}
	if (g_pTexCacheHead || g_pTexCacheTail || g_texCacheCount) {
		DebugPrintf("texture cache not cleared in std3D_FlushTextureCache()");
	}
	g_std3DOpened = 1;
	g_pStd3DCurDevice->availableMemory = g_pStd3DCurDevice->totalMemory;
}

// FUNCTION: XWA 0x594E6C
void std3D_StartScene(void) {
	HRESULT result;

	result = g_d3dDevice->lpVtbl->BeginScene(g_d3dDevice);
	if (result) {
		DebugPrintf("Error %s beginning scene.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
	}
	g_frameTriCount = 0;
	g_frameVertCount = 0;
	g_frameTexSwitches = 0;
	g_frameStateChanges = 0;
	g_frameBytesCached = 0;
	g_frameBytesPurged = 0;
	g_bTexCreateFailed = 0;
	g_bTexCacheOverflow = 0;
}

// FUNCTION: XWA 0x594EF8
void std3D_EndScene(void) {
	HRESULT result;

	result = g_d3dDevice->lpVtbl->EndScene(g_d3dDevice);
	if (result) {
		DebugPrintf("Error %s ending scene.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
	}
	g_totalTris += (float)(unsigned int)g_frameTriCount;
	g_totalVerts += (float)(unsigned int)g_frameVertCount;
	g_totalTexSwitches += (float)(unsigned int)g_frameTexSwitches;
	g_totalStateChanges += (float)(unsigned int)g_frameStateChanges;
	g_totalBytesCached += (float)(unsigned int)g_frameBytesCached;
	g_totalBytesPurged += (float)(unsigned int)g_frameBytesPurged;
	g_totalFrames += g_std3DFrameIncrement;
}

// FUNCTION: XWA 0x595006
char std3D_LockExecuteBuffer(void) {
	HRESULT result;

	g_d3dBufVertCount = 0;
	g_std3DExecBufTriCount = 0;
	++g_std3DOpened;
	g_d3dCurTexture = 1;
	result = g_d3dExecuteBuffer->lpVtbl->Lock(g_d3dExecuteBuffer, &g_d3dExecBufDesc);
	if (result) {
		DebugPrintf("Error %s locking D3D Execute buffer.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
		return 0;
	}
	g_d3dExecBufBase = (uint8_t*)g_d3dExecBufDesc.lpData;
	g_d3dWritePtr = g_d3dExecBufBase;
	return 1;
}

// FUNCTION: XWA 0x595095
char std3D_AddVertices(D3DTLVERTEX* verts, int count) {
	g_frameVertCount += count;
	if ((unsigned int)count + (unsigned int)g_d3dBufVertCount > (unsigned int)g_d3dMaxVerts) {
		return 0;
	}
	if (g_d3dWritePtr != (uint8_t*)verts) {
		memcpy(g_d3dWritePtr, verts, (size_t)(32 * count));
	}
	g_d3dWritePtr += 32 * count;
	g_d3dBufVertCount += count;
	return 1;
}

// FUNCTION: XWA 0x595106
char std3D_BeginInstructions(void) {
	g_d3dInstrStart = (D3DINSTRUCTION*)g_d3dWritePtr;
	g_d3dWritePtr[0] = D3DOP_PROCESSVERTICES;
	g_d3dWritePtr[1] = 16;
	*(uint16_t*)(g_d3dWritePtr + 2) = 1;
	g_d3dWritePtr += 4;
	*(uint32_t*)(g_d3dWritePtr + 0) = D3DPROCESSVERTICES_COPY;
	*(uint16_t*)(g_d3dWritePtr + 4) = 0;
	*(uint16_t*)(g_d3dWritePtr + 6) = 0;
	*(uint32_t*)(g_d3dWritePtr + 8) = (uint32_t)g_d3dBufVertCount;
	*(uint32_t*)(g_d3dWritePtr + 12) = 0;
	g_d3dWritePtr += 16;
	return 1;
}

// FUNCTION: XWA 0x595191
char std3D_AddTriangles(Std3DRenderTri* tris, unsigned int count) {
	int flags;
	unsigned int runCount;
	unsigned int cursor;
	Std3DRenderTri* tri;
	Std3DRenderTri* scanTri;
	unsigned int i;
	unsigned int processed;

	g_frameTriCount += count;
	scanTri = tris;
	cursor = 0;
	while (cursor < count) {
		i = 0;
		if (scanTri->texture == NULL) {
			tri = scanTri;
			flags = tri->flags;
			processed = cursor;
			while (processed < count && tri->texture == NULL && tri->flags == flags) {
				++i;
				++processed;
				++tri;
			}
			std3D_SetRenderState((unsigned int)flags);
			if (g_d3dCurTexture) {
				g_d3dWritePtr[0] = D3DOP_STATERENDER;
				g_d3dWritePtr[1] = 8;
				*(uint16_t*)(g_d3dWritePtr + 2) = 1;
				g_d3dWritePtr += 4;
				*(uint32_t*)(g_d3dWritePtr + 0) = D3DRENDERSTATE_TEXTUREHANDLE;
				*(uint32_t*)(g_d3dWritePtr + 4) = 0;
				g_d3dWritePtr += 8;
				g_d3dCurTexture = 0;
			}
			g_d3dWritePtr[0] = D3DOP_TRIANGLE;
			g_d3dWritePtr[1] = 8;
			*(uint16_t*)(g_d3dWritePtr + 2) = (uint16_t)i;
			g_d3dWritePtr += 4;
			for (runCount = 0; runCount < i; ++runCount) {
				*(uint16_t*)(g_d3dWritePtr + 0) = (uint16_t)scanTri->v0;
				*(uint16_t*)(g_d3dWritePtr + 2) = (uint16_t)scanTri->v1;
				*(uint16_t*)(g_d3dWritePtr + 4) = (uint16_t)scanTri->v2;
				*(uint16_t*)(g_d3dWritePtr + 6) =
					D3DTRIFLAG_EDGEENABLE1 | D3DTRIFLAG_EDGEENABLE2 | D3DTRIFLAG_EDGEENABLE3;
				g_d3dWritePtr += 8;
				++scanTri;
			}
			cursor += i;
		} else {
			Std3DTexCacheNode* texture;

			tri = scanTri;
			flags = tri->flags;
			processed = cursor;
			texture = scanTri->texture;
			while (processed < count && tri->texture == texture && tri->flags == flags) {
				++i;
				++processed;
				++tri;
			}
			std3D_SetRenderState((unsigned int)flags);
			if ((intptr_t)scanTri->texture != g_d3dCurTexture) {
				g_d3dWritePtr[0] = D3DOP_STATERENDER;
				g_d3dWritePtr[1] = 8;
				*(uint16_t*)(g_d3dWritePtr + 2) = 1;
				g_d3dWritePtr += 4;
				*(uint32_t*)(g_d3dWritePtr + 0) = D3DRENDERSTATE_TEXTUREHANDLE;
				*(uint32_t*)(g_d3dWritePtr + 4) = texture->texHandle;
				g_d3dWritePtr += 8;
				g_d3dCurTexture = (intptr_t)texture;
				++g_frameTexSwitches;
			}
			g_d3dWritePtr[0] = D3DOP_TRIANGLE;
			g_d3dWritePtr[1] = 8;
			*(uint16_t*)(g_d3dWritePtr + 2) = (uint16_t)i;
			g_d3dWritePtr += 4;
			for (runCount = 0; runCount < i; ++runCount) {
				*(uint16_t*)(g_d3dWritePtr + 0) = (uint16_t)scanTri->v0;
				*(uint16_t*)(g_d3dWritePtr + 2) = (uint16_t)scanTri->v1;
				*(uint16_t*)(g_d3dWritePtr + 4) = (uint16_t)scanTri->v2;
				*(uint16_t*)(g_d3dWritePtr + 6) =
					D3DTRIFLAG_EDGEENABLE1 | D3DTRIFLAG_EDGEENABLE2 | D3DTRIFLAG_EDGEENABLE3;
				g_d3dWritePtr += 8;
				++scanTri;
			}
			cursor += i;
		}
	}
	g_std3DExecBufTriCount += (int)count;
	return 1;
}

// FUNCTION: XWA 0x5954D6
char std3D_ExecuteBuffer(void) {
	D3DEXECUTEDATA execData;

	g_d3dWritePtr[0] = D3DOP_EXIT;
	g_d3dWritePtr[1] = 0;
	*(uint16_t*)(g_d3dWritePtr + 2) = 0;
	g_d3dWritePtr += 4;

	{
		HRESULT result;

		result = g_d3dExecuteBuffer->lpVtbl->Unlock(g_d3dExecuteBuffer);
		if (result) {
			DebugPrintf("Error %s unlocking D3D Execute buffer.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
			return 0;
		}
		memset(&execData, 0, sizeof(execData));
		execData.dwSize = 48;
		execData.dwVertexCount = (uint32_t)g_d3dBufVertCount;
		execData.dwInstructionOffset = (uint32_t)((uint8_t*)g_d3dInstrStart - g_d3dExecBufBase);
		execData.dwInstructionLength = (uint32_t)(g_d3dWritePtr - (uint8_t*)g_d3dInstrStart);
		g_d3dExecuteBuffer->lpVtbl->SetExecuteData(g_d3dExecuteBuffer, &execData);
		result = g_d3dDevice->lpVtbl->Execute(g_d3dDevice, g_d3dExecuteBuffer, g_d3dViewport,
											  D3DEXECUTE_UNCLIPPED);
		if (result) {
			DebugPrintf("Error %s executing buffer.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
			return 0;
		}
	}
	return 1;
}

// FUNCTION: XWA 0x598396
int std3D_BuildViewportQuad(const Std3DViewportRect* rect) {
	if (rect == NULL) {
		return 0;
	}

	memcpy(&g_std3DQuadRect, rect, sizeof(g_std3DQuadRect));
	memset(g_std3DQuadVerts, 0, sizeof(g_std3DQuadVerts));
	g_std3DQuadVerts[0].sx = (float)g_std3DQuadRect.x;
	g_std3DQuadVerts[0].sy = (float)g_std3DQuadRect.y;
	g_std3DQuadVerts[1].sx = (float)(g_std3DQuadRect.x + g_std3DQuadRect.width);
	g_std3DQuadVerts[1].sy = (float)g_std3DQuadRect.y;
	g_std3DQuadVerts[2].sx = (float)(g_std3DQuadRect.x + g_std3DQuadRect.width);
	g_std3DQuadVerts[2].sy = (float)(g_std3DQuadRect.y + g_std3DQuadRect.height);
	g_std3DQuadVerts[3].sx = (float)g_std3DQuadRect.x;
	g_std3DQuadVerts[3].sy = (float)(g_std3DQuadRect.y + g_std3DQuadRect.height);

	memset(g_std3DQuadTris, 0, sizeof(g_std3DQuadTris));
	g_std3DQuadTris[0].v0 = 0;
	g_std3DQuadTris[0].v1 = 1;
	g_std3DQuadTris[0].v2 = 2;
	g_std3DQuadTris[0].flags = 0x8200;
	g_std3DQuadTris[1].v0 = 0;
	g_std3DQuadTris[1].v1 = 2;
	g_std3DQuadTris[1].v2 = 3;
	g_std3DQuadTris[1].flags = 0x8200;
	return g_std3DQuadRect.y + g_std3DQuadRect.height;
}

// FUNCTION: XWA 0x599D95
int std3D_MapZCmpFunc(int zCmpCapsMask, int bPreferOrEqual) {
	int value;

	value = 0;
	if ((zCmpCapsMask & 1) != 0) {
		value |= 1;
	}
	if ((zCmpCapsMask & 4) != 0) {
		value |= 3;
	}
	if ((zCmpCapsMask & 2) != 0) {
		if (bPreferOrEqual) {
			value |= 4;
		} else {
			value |= 2;
		}
	}
	if ((zCmpCapsMask & 8) != 0) {
		value |= 4;
	}
	if ((zCmpCapsMask & 0x10) != 0) {
		if (bPreferOrEqual) {
			value |= 7;
		} else {
			value |= 5;
		}
	}
	if ((zCmpCapsMask & 0x20) != 0) {
		value |= 6;
	}
	if ((zCmpCapsMask & 0x40) != 0) {
		value |= 7;
	}
	if ((zCmpCapsMask & 0x80) != 0) {
		value |= 8;
	}
	return value;
}

/* Emit a D3DOP_STATERENDER instruction header for `count` {state,value} pairs. */
static __inline void std3D_EmitStateRenderHeader(unsigned int count) {
	g_d3dWritePtr[0] = D3DOP_STATERENDER;
	g_d3dWritePtr[1] = 8;
	*(uint16_t*)(g_d3dWritePtr + 2) = (uint16_t)count;
	g_d3dWritePtr += 4;
}

static __inline void std3D_EmitState(uint32_t state, uint32_t value) {
	*(uint32_t*)(g_d3dWritePtr + 0) = state;
	*(uint32_t*)(g_d3dWritePtr + 4) = value;
	g_d3dWritePtr += 8;
}

// FUNCTION: XWA 0x5955F0
void std3D_SetRenderState(Std3DRenderStateFlags flags) {
	if (g_d3dStateFlags != flags) {
		++g_frameStateChanges;

		if ((g_d3dStateFlags & STD3D_RS_TEXTURE_ADDRESS_CLAMP) != (flags & STD3D_RS_TEXTURE_ADDRESS_CLAMP)) {
			std3D_EmitStateRenderHeader(1);
			if (flags & STD3D_RS_TEXTURE_ADDRESS_CLAMP) {
				std3D_EmitState(D3DRENDERSTATE_TEXTUREADDRESS, 3u); /* CLAMP */
			} else {
				std3D_EmitState(D3DRENDERSTATE_TEXTUREADDRESS, 1u); /* WRAP */
			}
		}
		if ((g_d3dStateFlags & STD3D_RS_MONO_DISABLE) != (flags & STD3D_RS_MONO_DISABLE)) {
			std3D_EmitStateRenderHeader(1);
			if (flags & STD3D_RS_MONO_DISABLE) {
				std3D_EmitState(D3DRENDERSTATE_MONOENABLE, 0u);
			} else {
				std3D_EmitState(D3DRENDERSTATE_MONOENABLE, 1u);
			}
		}
		if ((g_d3dStateFlags & STD3D_RS_ALPHA_TEST_DISABLE) != (flags & STD3D_RS_ALPHA_TEST_DISABLE)) {
			std3D_EmitStateRenderHeader(2);
			if (flags & STD3D_RS_ALPHA_TEST_DISABLE) {
				std3D_EmitState(D3DRENDERSTATE_ALPHATESTENABLE, 0u);
				std3D_EmitState(D3DRENDERSTATE_ALPHAFUNC, 6u); /* D3DCMP_NOTEQUAL */
			} else {
				std3D_EmitState(D3DRENDERSTATE_ALPHATESTENABLE, 1u);
				std3D_EmitState(D3DRENDERSTATE_ALPHAFUNC, 6u); /* D3DCMP_NOTEQUAL */
			}
		}
		if ((g_d3dStateFlags & (STD3D_RS_ALPHA_BLEND | STD3D_RS_TEXTURE_MODULATE_ALPHA)) !=
				(flags & (STD3D_RS_ALPHA_BLEND | STD3D_RS_TEXTURE_MODULATE_ALPHA)) ||
			(g_d3dStateFlags & STD3D_RS_TEXTURE_BLEND_DECAL) != (flags & STD3D_RS_TEXTURE_BLEND_DECAL)) {
			if ((flags & (STD3D_RS_ALPHA_BLEND | STD3D_RS_TEXTURE_MODULATE_ALPHA)) != 0 ||
				(flags & STD3D_RS_TEXTURE_BLEND_DECAL) != 0) {
				std3D_EmitStateRenderHeader(4);
				std3D_EmitState(D3DRENDERSTATE_SRCBLEND, 5u);  /* SRCALPHA */
				std3D_EmitState(D3DRENDERSTATE_DESTBLEND, 6u); /* INVSRCALPHA */
				if (flags & STD3D_RS_TEXTURE_BLEND_DECAL) {
					std3D_EmitState(D3DRENDERSTATE_TEXTUREMAPBLEND, 1u); /* DECAL */
				} else if (flags & STD3D_RS_TEXTURE_MODULATE_ALPHA) {
					std3D_EmitState(D3DRENDERSTATE_TEXTUREMAPBLEND, 4u); /* MODULATEALPHA */
				} else {
					std3D_EmitState(D3DRENDERSTATE_TEXTUREMAPBLEND, 2u); /* MODULATE */
				}
				std3D_EmitState(D3DRENDERSTATE_BLENDENABLE, 1u);
			} else {
				std3D_EmitStateRenderHeader(4);
				std3D_EmitState(D3DRENDERSTATE_SRCBLEND, 2u);  /* ONE */
				std3D_EmitState(D3DRENDERSTATE_DESTBLEND, 1u); /* ZERO */
				if (flags & STD3D_RS_TEXTURE_BLEND_DECAL) {
					std3D_EmitState(D3DRENDERSTATE_TEXTUREMAPBLEND, 1u); /* DECAL */
				} else if (flags & STD3D_RS_TEXTURE_MODULATE_ALPHA) {
					std3D_EmitState(D3DRENDERSTATE_TEXTUREMAPBLEND, 4u); /* MODULATEALPHA */
				} else {
					std3D_EmitState(D3DRENDERSTATE_TEXTUREMAPBLEND, 2u); /* MODULATE */
				}
				std3D_EmitState(D3DRENDERSTATE_BLENDENABLE, 0u);
			}
		}
		if ((g_d3dStateFlags & (STD3D_RS_Z_COMPARE_ENABLE | STD3D_RS_Z_WRITE_ENABLE)) !=
			(flags & (STD3D_RS_Z_COMPARE_ENABLE | STD3D_RS_Z_WRITE_ENABLE))) {
			std3D_EmitStateRenderHeader(2);
			if (flags & STD3D_RS_Z_COMPARE_ENABLE) {
				*(uint32_t*)(g_d3dWritePtr + 0) = D3DRENDERSTATE_ZFUNC;
				*(uint32_t*)(g_d3dWritePtr + 4) =
					(uint32_t)std3D_MapZCmpFunc(g_std3DZCmpMode, flags & STD3D_RS_Z_COMPARE_PREFER_EQUAL);
				g_d3dWritePtr += 8;
			} else {
				std3D_EmitState(D3DRENDERSTATE_ZFUNC, 8u); /* D3DCMP_ALWAYS */
			}
			if (flags & STD3D_RS_Z_WRITE_ENABLE) {
				std3D_EmitState(D3DRENDERSTATE_ZWRITEENABLE, 1u);
			} else {
				std3D_EmitState(D3DRENDERSTATE_ZWRITEENABLE, 0u);
			}
		}
		if ((g_d3dStateFlags & (STD3D_RS_TEXTURE_MAG_LINEAR | STD3D_RS_TEXTURE_MIN_LINEAR)) !=
			(flags & (STD3D_RS_TEXTURE_MAG_LINEAR | STD3D_RS_TEXTURE_MIN_LINEAR))) {
			int filter;
			std3D_EmitStateRenderHeader(2);
			*(uint32_t*)(g_d3dWritePtr + 0) = D3DRENDERSTATE_TEXTUREMAG;
			*(uint32_t*)(g_d3dWritePtr + 4) = (uint32_t)(((flags & STD3D_RS_TEXTURE_MAG_LINEAR) != 0) + 1);
			g_d3dWritePtr += 8;
			*(uint32_t*)(g_d3dWritePtr + 0) = D3DRENDERSTATE_TEXTUREMIN;
			if (flags & STD3D_RS_TEXTURE_MIN_LINEAR) {
				filter = g_d3dTexFilterLinear;
			} else {
				filter = g_d3dTexFilterPoint;
			}
			*(uint32_t*)(g_d3dWritePtr + 4) = (uint32_t)filter;
			g_d3dWritePtr += 8;
		}
		if ((g_d3dStateFlags & STD3D_RS_FOG_ENABLE) != (flags & STD3D_RS_FOG_ENABLE)) {
			if (flags & STD3D_RS_FOG_ENABLE) {
				std3D_EmitStateRenderHeader(5);
				std3D_EmitState(D3DRENDERSTATE_FOGENABLE, 1u);
				*(uint32_t*)(g_d3dWritePtr + 0) = D3DRENDERSTATE_FOGCOLOR;
				*(uint32_t*)(g_d3dWritePtr + 4) =
					(uint32_t)((g_std3DFogColorRed8 << 16) | (g_std3DFogColorGreen8 << 8) |
							   g_std3DFogColorBlue8);
				g_d3dWritePtr += 8;
				std3D_EmitState(D3DRENDERSTATE_FOGTABLEMODE, 3u);
				*(uint32_t*)(g_d3dWritePtr + 0) = D3DRENDERSTATE_FOGTABLESTART;
				*(uint32_t*)(g_d3dWritePtr + 4) = (uint32_t)g_std3DFogTableStartBits;
				g_d3dWritePtr += 8;
				*(uint32_t*)(g_d3dWritePtr + 0) = D3DRENDERSTATE_FOGTABLEEND;
				*(uint32_t*)(g_d3dWritePtr + 4) = (uint32_t)g_std3DFogTableEndBits;
				g_d3dWritePtr += 8;
			} else {
				std3D_EmitStateRenderHeader(1);
				std3D_EmitState(D3DRENDERSTATE_FOGENABLE, 0u);
			}
		}

		g_d3dStateFlags = flags;
	}
}

// FUNCTION: XWA 0x597F9F
char std3D_ClearZBuffer(void) {
	DDBLTFX fx;
	HRESULT hr;

	memset(&fx, 0, sizeof(fx));
	fx.dwSize = 100;
	if (g_std3DZCmpMode == 16) {
		fx.dwFillDepth = 0;
	} else {
		fx.dwFillDepth = 0xFFFF;
	}
	{
		D3DRECT rect;

		rect.x1 = g_std3DQuadRect.x;
		rect.y1 = g_std3DQuadRect.y;
		rect.x2 = g_std3DQuadRect.x + g_std3DQuadRect.width;
		rect.y2 = g_std3DQuadRect.y + g_std3DQuadRect.height;
		while (1) {
			hr = g_std3DZBufferSurface.surface->lpVtbl->Blt(g_std3DZBufferSurface.surface, &rect, NULL, NULL,
															DDBLT_DEPTHFILL | DDBLT_WAIT, &fx);
			if (!hr) {
				break;
			}
			if (hr == DX_DDERR_SURFACELOST) {
				hr = g_std3DZBufferSurface.surface->lpVtbl->Restore(g_std3DZBufferSurface.surface);
			}
			if (hr) {
				DebugPrintf("Error %s clearing zbuffer.\n", std3D_GetD3DErrorString(hr), 0, 0, 0);
				return 0;
			}
		}
	}
	return 1;
}

// FUNCTION: XWA 0x594AE9
void std3D_Close(int checkForLeakedTextures) {
	if (!g_std3DDeviceOpen) {
		DebugPrintf("Error: Multiple Closes Attempted.\n", 0, 0, 0, 0);
	} else {
		if (!checkForLeakedTextures && std3D_GetTextureSurfaceCount()) {
			DebugPrintf("Textures still allocated at std3D_Close() call:", std3D_GetTextureSurfaceCount());
			DebugPrintf("Some textures have not been freed in std3D_Close()");
		}
		std3D_FlushTextureCache();
		std3D_FreePalettes();
		std3D_DestroyDevice();
		DebugPrintf("std3D closed.\n", 0, 0, 0, 0);
		g_std3DDeviceOpen = 0;
	}
}

// FUNCTION: XWA 0x594D02
void std3D_Shutdown(void) {
	if (g_lpD3D) {
		int debugResult;

		g_std3DReleaseRefCount = (int)g_lpD3D->lpVtbl->Release(g_lpD3D);
		if (!g_std3DReleaseRefCount) {
			debugResult = DebugPrintf("DX object released too early, refcount is 0");
		} else {
			debugResult = 0;
		}
		g_lpD3D = NULL;
	}
	if (g_std3DDirectDraw2) {
		int debugResult;

		g_std3DReleaseRefCount = (int)g_std3DDirectDraw2->lpVtbl->Release(g_std3DDirectDraw2);
		if (g_std3DReleaseRefCount) {
			debugResult = DebugPrintf("DX object not released properly, refcount:", g_std3DReleaseRefCount);
		} else {
			debugResult = 0;
		}
		g_std3DDirectDraw2 = NULL;
	}
	g_std3DDirectDraw = NULL;
	g_lpRenderSurface = NULL;
	DebugPrintf("Shutdown Succeeded.\n", 0, 0, 0, 0);
	g_std3DStartupDone = 0;
}

/* Renderer_InitD3DDevice @0x441EE0 lives in std3d_device.c with the device chain. */
