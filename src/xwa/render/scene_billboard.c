#include "xwa/flight/fediskio.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer_internal.h"

#include "xwa/flight/flight.h"
#include "xwa/math/fixed.h"
#include "xwa/util/memory.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot_hud.h"
#else
#include <windows.h>
#endif

#include <math.h>

// FUNCTION: XWA 0x4D21C0
int SceneBillboard_ComputeProjectedSize(int depthZ, uint16_t modelMaxExtent, int baseScreenSize,
										int clampTo1024) {
	int scaledDepth;

#ifdef XWA_MODERN
	scaledDepth = depthZ;
	if (scaledDepth < 0) {
		scaledDepth = (int)(0u - (uint32_t)scaledDepth);
	}
#else
	scaledDepth = depthZ;
	if (scaledDepth < 0) {
		scaledDepth = -scaledDepth;
	}
#endif
	scaledDepth >>= 8;
	if (scaledDepth != 0) {
		scaledDepth = modelMaxExtent / scaledDepth;
	}
	{
#ifdef XWA_MODERN
		int result = (baseScreenSize & 0xffff) * scaledDepth;
#else
		int result = (baseScreenSize & 0xffff) * scaledDepth;
#endif

		scaledDepth = clampTo1024;
		result >>= 8;
		if (scaledDepth && result > 1024) {
			return 1024;
		}
		return result;
	}
}

// FUNCTION: XWA 0x424C00
int DeathStar_ComputeScaledProjectedBillboardSize(int depthZ, unsigned int modelMaxExtent,
												  uint16_t baseScreenSize) {
	if (depthZ < 0) {
		depthZ = (int)(0u - (uint32_t)depthZ);
	}

	return (int)(((double)modelMaxExtent * (double)baseScreenSize / (double)depthZ) *
				 g_deathStarTunnelBillboardScale);
}

// FUNCTION: XWA 0x4530A0
void DeathStar_CacheModelNodeTextures(OptNode* node, int firstFrame, int lastFrame) {
	int childIndex;

	if (node->nodeType == OPT_TEXTURE_REF) {
		D3DInfoNode* d3dInfo;
		int frame;

		d3dInfo = (D3DInfoNode*)node->param2;
		for (frame = firstFrame; frame <= lastFrame; ++frame) {
			if (d3dInfo->baseMipSurfaces[frame] != NULL) {
				std3D_AddToTextureCache(d3dInfo->baseMipSurfaces[frame]);
			}
			if (d3dInfo->lightmapMipSurfaces[frame] != NULL && d3dInfo->lightmapMipSurfaces[0] != NULL) {
				std3D_AddToTextureCache(d3dInfo->lightmapMipSurfaces[frame]);
			}
		}
	}

	for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
		if (node->pChildren[childIndex] != NULL) {
			DeathStar_CacheModelNodeTextures(node->pChildren[childIndex], firstFrame, lastFrame);
		}
	}

	return;
}

// FUNCTION: XWA 0x453130
void DeathStar_PreloadSegmentObjectTextures(ObjectRecord* object, int firstFrame, int lastFrame) {
	int result;
	MemoryHandle modelHandle;
	OptimizedPolyObject* model;
	int cacheFirstFrame;
	int cacheLastFrame;
	int rootIndex;

	result = object->objectType;
	modelHandle = g_loadedModels.byObjectType[result];
	if (modelHandle == 0 || (g_modelTypeTable[result].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);
	if (g_hwMipmapFilter) {
		cacheLastFrame = 0;
		cacheFirstFrame = 0;
	} else {
		cacheLastFrame = lastFrame;
		cacheFirstFrame = firstFrame;
	}

	result = model->rootNodeCount;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* rootNode;

		rootNode = model->rootNodes[rootIndex];
		if (rootNode->nodeType != OPT_TEXTURE_REF) {
			DeathStar_CacheModelNodeTextures(rootNode, cacheFirstFrame, cacheLastFrame);
		}
	}
}

// FUNCTION: XWA 0x42BD20
void Backdrop_DrawModelTexQuadAtScreen(ObjectTypeId modelType, int screenX, int screenY, int16_t angle,
									   int16_t screenScale) {
	FlightTexQuad quad;

	quad.screenY = screenY;
	g_camRelWorldZ = quad.depthZ = 0x100000;
	quad.rotationAngle = angle;
	quad.screenX = screenX;
	viewZ = 0x7FFFFFFF;
	quad.screenSize = (uint16_t)screenScale;
	RenderQuad_DrawModelTexture(modelType, &quad, -1);
}

// Projects a camera-space coordinate to the screen: (value << perspShift + g_projScaleHalfInt) / z,
// saturated to 0x7FFFFF00 when the 64-bit quotient would not fit in 32 bits.
static inline int Backdrop_ProjectDivSaturated(int value, int z) {
#ifndef XWA_MODERN
	__asm {
		mov eax, value
		mov cl, perspShift
		xor edx, edx
		shld edx, eax, cl
		shl eax, cl
		add eax, g_projScaleHalfInt
		adc edx, 0
		cmp edx, z
		jb short project_div
		mov eax, 0x7FFFFF00
		jmp short project_done
	project_div:
		div z
	project_done:
		mov value, eax
	}
	return value;
#else
	uint64_t numerator;

	numerator = ((uint64_t)(uint32_t)value << (perspShift & 0x1f)) + (uint32_t)g_projScaleHalfInt;
	if ((uint32_t)(numerator >> 32) >= (uint32_t)z) {
		return 0x7fffff00;
	}
	return (int)(uint32_t)(numerator / (uint32_t)z);
#endif
}

// FUNCTION: XWA 0x407330
void Backdrop_ProjectAndDrawScreenQuad(int viewX, int viewY, int viewZ, int16_t angle, ObjectTypeId modelType,
									   int16_t screenScale) {
	int projectedX;
	int projectedY;

	if (viewX < 0) {
		if (-viewX > viewZ) {
			return;
		}
		viewX = -viewX;
		viewX = Backdrop_ProjectDivSaturated(viewX, viewZ);
		projectedX = -viewX;
	} else {
		if (viewX > viewZ) {
			return;
		}
		viewX = Backdrop_ProjectDivSaturated(viewX, viewZ);
		projectedX = viewX;
	}

	if (viewY < 0) {
		if (-viewY > viewZ) {
			return;
		}
		viewX = -viewY;
		viewX = Backdrop_ProjectDivSaturated(viewX, viewZ);
		projectedY = -viewX;
	} else {
		if (viewY > viewZ) {
			return;
		}
		viewX = viewY;
		viewX = Backdrop_ProjectDivSaturated(viewX, viewZ);
		projectedY = viewX;
	}

	Backdrop_DrawModelTexQuadAtScreen((uint16_t)modelType, g_flightVpCenterX + projectedX,
									  g_flightVpHeight - projectedY - g_projOffsetY - g_flightVpCenterY,
									  angle, screenScale);
}

static __inline int Backdrop_ScaleAxisComponent(int extent, int axisComponent) {
	return Xwa_Q15MulReuseFirstSlot(axisComponent, extent);
}

static __inline int Backdrop_ComputeTextureAxisExtent(uint16_t texSize, uint16_t screenScale) {
	uint32_t shiftedSize;
	uint32_t extentBase;

	shiftedSize = (uint32_t)texSize << (9 - perspShift);
	extentBase = shiftedSize + (shiftedSize >> 1);
	return (int)((uint32_t)screenScale * extentBase >> 8);
}

// FUNCTION: XWA 0x407480
void Backdrop_DrawHardwareAxisQuad(ObjectTypeId modelType, int viewX, int viewY, int viewZ,
								   int halfWidthAxisX, int halfWidthAxisY, int halfWidthAxisZ,
								   int halfHeightAxisX, int halfHeightAxisY, int halfHeightAxisZ,
								   uint16_t screenScale) {
	TexLevel* curTexLevel;
	int halfWidthPixels;
	int halfHeightPixels;
	int corners[12];
	int quadDrawFlags;
	int* corner;
	int cornerCount;

	curTexLevel = g_modelTypeTable[(uint16_t)modelType].curTexLevel;
	if ((g_modelTypeTable[(uint16_t)modelType].assetFlags &
		 (MODEL_TYPE_ASSET_TEXTURE_DRAW | MODEL_TYPE_ASSET_TEXTURE_READY)) == 0 ||
		curTexLevel == NULL) {
		return;
	}

	halfHeightPixels = Backdrop_ComputeTextureAxisExtent(curTexLevel->height, screenScale);
	halfWidthPixels = Backdrop_ComputeTextureAxisExtent(curTexLevel->width, screenScale);

	halfWidthAxisX = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisX);
	halfWidthAxisY = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisY);
	halfWidthAxisZ = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisZ);

	halfHeightAxisX = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisX);
	halfHeightAxisY = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisY);
	halfHeightAxisZ = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisZ);

	corner = corners;
	cornerCount = 4;
	do {
		corner[0] = viewX;
		corner[1] = viewY;
		corner[2] = viewZ;
		corner += 3;
	} while (--cornerCount != 0);

	corners[0] += halfHeightAxisX + halfWidthAxisX;
	corners[1] += halfWidthAxisY + halfHeightAxisY;
	corners[2] += halfWidthAxisZ + halfHeightAxisZ;
	corners[3] += halfHeightAxisX - halfWidthAxisX;
	corners[4] += halfHeightAxisY - halfWidthAxisY;
	corners[5] += halfHeightAxisZ - halfWidthAxisZ;
	corners[6] -= halfHeightAxisX + halfWidthAxisX;
	corners[7] -= halfWidthAxisY + halfHeightAxisY;
	corners[8] -= halfWidthAxisZ + halfHeightAxisZ;
	corners[9] += halfWidthAxisX - halfHeightAxisX;
	corners[10] += halfWidthAxisY - halfHeightAxisY;
	corners[11] += halfWidthAxisZ - halfHeightAxisZ;

	quadDrawFlags = 0x612;
	if (g_bilinearEnabled) {
		quadDrawFlags = 0x792;
	}

	curTexLevel->argbColor = -1;
	RenderQuad_DrawTextured3D(corners, curTexLevel, quadDrawFlags);
}

static __inline int RenderBillboard_ClipScreenAlignedQuad(ProjVertex* vertBuf) {
	int clippedCount;
	int prevVert;
	int curVert;
	int i;

	g_clipCountB = 0;
	clippedCount = g_clipCountA;
	if (clippedCount > 0) {
		prevVert = g_clipIdxA[clippedCount - 1];
		for (i = 0; i < g_clipCountA; ++i) {
			curVert = g_clipIdxA[i];
			RenderClip_ClipPolyTop(prevVert, curVert, vertBuf);
			prevVert = curVert;
		}
		clippedCount = g_clipCountB;
	}

	g_clipCountA = 0;
	if (clippedCount > 0) {
		prevVert = g_clipIdxB[clippedCount - 1];
		for (i = 0; i < g_clipCountB; ++i) {
			curVert = g_clipIdxB[i];
			RenderClip_ClipPolyBottom(prevVert, curVert, vertBuf);
			prevVert = curVert;
		}
		clippedCount = g_clipCountA;
	}

	g_clipCountB = 0;
	if (clippedCount > 0) {
		prevVert = g_clipIdxA[clippedCount - 1];
		for (i = 0; i < g_clipCountA; ++i) {
			curVert = g_clipIdxA[i];
			RenderClip_ClipPolyLeft(prevVert, curVert, vertBuf);
			prevVert = curVert;
		}
		clippedCount = g_clipCountB;
	}

	g_clipCountA = 0;
	if (clippedCount > 0) {
		prevVert = g_clipIdxB[clippedCount - 1];
		for (i = 0; i < g_clipCountB; ++i) {
			curVert = g_clipIdxB[i];
			RenderClip_ClipPolyRight(prevVert, curVert, vertBuf);
			prevVert = curVert;
		}
		clippedCount = g_clipCountA;
	}

	return clippedCount;
}

static __inline int RenderBillboard_ClipTrailQuad(ProjVertex* vertBuf, int clipNear) {
	int i;

	if (clipNear) {
		for (i = 0; i < g_clipCountA; ++i) {
			g_clipIdxB[i] = g_clipIdxA[i];
		}
		g_clipCountB = g_clipCountA;

		g_clipCountA = 0;
		if (g_clipCountB > 0) {
			int prevVert;

			prevVert = g_clipIdxB[g_clipCountB - 1];
			for (i = 0; i < g_clipCountB; ++i) {
				int curVert;

				curVert = g_clipIdxB[i];
				RenderClip_ClipPolyNear(prevVert, curVert, vertBuf);
				prevVert = curVert;
			}
		}
	}

	return RenderBillboard_ClipScreenAlignedQuad(vertBuf);
}

static __inline void RenderBillboard_EmitClippedTrailQuad(ProjVertex* vertBuf, Std3DTexCacheNode* cacheNode,
														  ObjectTrailEmitter* trail) {
	int i;
	ProjVertex* alphaVert;

	alphaVert = vertBuf;
	for (i = 0; i < g_clipCountA; ++i) {
		ProjVertex* vert;
		float screenY;
		float texU;
		float texV;
		float rhw;
		float depth;
		int alpha;
		uint32_t alphaMask;

		vert = &vertBuf[g_clipIdxA[i]];
		screenY = vert->sy;
		texU = vert->tu;
		texV = vert->tv;
		rhw = vert->w;
		g_flightVertexBuffer[g_d3dVertexCount].sx = vert->sx + g_flightVpOriginX;
		g_flightVertexBuffer[g_d3dVertexCount].sy = screenY + g_flightVpOriginY;
		depth = rhw * g_depthProjScale / (rhw * g_depthProjScale + g_projScale);
		if (depth < g_renderMinD3DDepth) {
			depth = g_renderMinD3DDepth;
		}
		if (g_std3DZCmpMode == 2) {
			depth = g_renderUnitFloat - depth;
		}

		g_flightVertexBuffer[g_d3dVertexCount].sz = depth;
		g_flightVertexBuffer[g_d3dVertexCount].rhw = rhw;
		g_flightVertexBuffer[g_d3dVertexCount].tu = texU;
		g_flightVertexBuffer[g_d3dVertexCount].tv = texV;
		alpha = (int)(alphaVert->litColor[0] * g_vertexColorAlphaScale);
		alphaMask = ((uint32_t)alpha << 24) + 0x00ffffffu;
		++alphaVert;
		g_flightVertexBuffer[g_d3dVertexCount].color = trail->argbColor & alphaMask;
		g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
		g_clipIdxA[i] = g_d3dVertexCount++;
	}

	for (i = 2; i < g_clipCountA; ++i) {
		g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
		g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[i - 1];
		g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[i];
		g_triBuffer[g_d3dIndexCount].texture = cacheNode;
		g_triBuffer[g_d3dIndexCount].flags = (int)trail->renderFlags;
		++g_d3dIndexCount;
	}
}

static __inline uint32_t RenderBillboard_GetTrailTextureModelKey(const ObjectTrailEmitter* trail) {
	return trail->textureModelType;
}

// FUNCTION: XWA 0x44C1C0
void RenderBillboard_DrawSpriteFacingCamera(ParticleEffect* effect, unsigned int renderFlags) {
	ParticleEffectTemplate* effectDef;
	ParticleRecord* particle;
	Std3DTexCacheNode* cacheNode;
	TexLevel* curTexLevel;
	float uScale;
	float vScale;
	ProjVertex vertBuf[40];

	effectDef = effect->def;
	particle = effect->particles;
	uScale = 1.0f;
	cacheNode = NULL;
	vScale = g_renderUnitFloat;
	{
		float centerRelX;
		float centerRelY;
		float centerRelZ;

		centerRelX = effect->world.x - (float)g_players[g_localPlayer].viewState.savedTargetX;
		centerRelY = effect->world.y - (float)g_players[g_localPlayer].viewState.savedTargetY;
		centerRelZ = effect->world.z - (float)g_players[g_localPlayer].viewState.savedTargetZ;
		if (TRANSFM2_ViewTransformZ(centerRelX, centerRelY, centerRelZ) < g_renderBillboardNearCullZ) {
			return;
		}
	}

	if (effect->textureFrameCount == 0) {
		curTexLevel = effectDef->staticTexLevel;
		if (curTexLevel->image != NULL) {
			Std3DTextureSurface* textureSurface;

			textureSurface = (Std3DTextureSurface*)curTexLevel->image;
			cacheNode = &textureSurface->cacheNode;
			std3D_AddToTextureCache(textureSurface);
			if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
				int squareWidth;
				int squareHeight;

				uScale = 1.0f;
				vScale = g_renderUnitFloat;
				squareWidth = curTexLevel->width;
				squareHeight = curTexLevel->height;
				if (squareWidth > squareHeight) {
					do {
						vScale *= g_renderHalfFloat;
						squareHeight <<= 1;
					} while (squareHeight < squareWidth);
				} else if (squareWidth < squareHeight) {
					do {
						uScale *= g_renderHalfFloat;
						squareWidth <<= 1;
					} while (squareWidth < squareHeight);
				}
			}
		}
	}

	while (particle != NULL) {
		float relX;
		float relY;
		float relZ;
		float viewXf;
		float viewYf;
		float viewZf;
		float rhw;
		float halfWidth;
		float halfHeight;
		float screenX;
		float screenY;

		if (effect->textureFrameCount != 0) {
			FeDiskIo_SelectTextureFrame(effect->textureModelType,
										(uint16_t)(((int)((particle->ageTicks * effect->textureAnimRate) *
														  effectDef->colorDeltaScale) %
													effect->textureFrameCount) +
												   1),
										256);
			curTexLevel = g_modelTypeTable[effect->textureModelType].curTexLevel;
			if (curTexLevel->image != NULL) {
				Std3DTextureSurface* textureSurface;

				textureSurface = (Std3DTextureSurface*)curTexLevel->image;
				cacheNode = &textureSurface->cacheNode;
				std3D_AddToTextureCache(textureSurface);
				if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
					int squareWidth;
					int squareHeight;

					uScale = 1.0f;
					vScale = g_renderUnitFloat;
					squareWidth = curTexLevel->width;
					squareHeight = curTexLevel->height;
					if (squareWidth > squareHeight) {
						do {
							vScale *= g_renderHalfFloat;
							squareHeight <<= 1;
						} while (squareHeight < squareWidth);
					} else if (squareWidth < squareHeight) {
						do {
							uScale *= g_renderHalfFloat;
							squareWidth <<= 1;
						} while (squareWidth < squareHeight);
					}
				}
			}
		}

		relX = particle->world.x - (float)g_players[g_localPlayer].viewState.savedTargetX;
		relY = particle->world.y - (float)g_players[g_localPlayer].viewState.savedTargetY;
		relZ = particle->world.z - (float)g_players[g_localPlayer].viewState.savedTargetZ;
		viewXf = (float)TRANSFM2_ViewTransformX(relX, relY, relZ);
		viewYf = (float)TRANSFM2_ViewTransformY(relX, relY, relZ);
		if ((viewZf = (float)TRANSFM2_ViewTransformZ(relX, relY, relZ)) > g_renderUnitFloat) {
			viewZf = g_renderUnitFloat / viewZf;
			rhw = g_projScale * viewZf;
			halfWidth = (float)(curTexLevel->width << curTexLevel->shift) * viewZf * particle->size *
						(float)effectDef->billboardScale;
			halfHeight = (float)(curTexLevel->height << curTexLevel->shift) * viewZf * particle->size *
						 (float)effectDef->billboardScale;
			screenX = viewXf * rhw + g_flightVpCenterXf;
			screenY = viewYf * rhw + g_projOffsetYf + g_flightVpCenterYf;

			g_clipCountA = 4;
			g_clipVertCursor = 4;
			g_clipIdxA[0] = 0;
			g_clipIdxA[1] = 1;
			g_clipIdxA[2] = 2;
			g_clipIdxA[3] = 3;

			vertBuf[0].sx = screenX + halfWidth;
			vertBuf[0].sy = screenY + halfHeight;
			vertBuf[0].w = rhw;
#ifdef XWA_MODERN
			vertBuf[0].lightIntensity = g_renderZeroFloat;
#endif
			vertBuf[0].litColor[0] = g_renderUnitFloat;
			vertBuf[0].litColor[1] = g_renderZeroFloat;
			vertBuf[0].litColor[2] = g_renderZeroFloat;
			vertBuf[0].litColor[3] = g_renderZeroFloat;
			vertBuf[0].tu = g_currentQuadTexCoords[0].u * uScale;
			vertBuf[0].tv = g_currentQuadTexCoords[0].v * vScale;
			vertBuf[0].extraLayerUVCount = 0;

			vertBuf[1].sx = screenX - halfWidth;
			vertBuf[1].sy = screenY + halfHeight;
			vertBuf[1].w = rhw;
#ifdef XWA_MODERN
			vertBuf[1].lightIntensity = g_renderZeroFloat;
#endif
			vertBuf[1].litColor[0] = g_renderUnitFloat;
			vertBuf[1].litColor[1] = g_renderZeroFloat;
			vertBuf[1].litColor[2] = g_renderZeroFloat;
			vertBuf[1].litColor[3] = g_renderZeroFloat;
			vertBuf[1].tu = g_currentQuadTexCoords[1].u * uScale;
			vertBuf[1].tv = g_currentQuadTexCoords[1].v * vScale;
			vertBuf[1].extraLayerUVCount = 0;

			vertBuf[2].sx = screenX - halfWidth;
			vertBuf[2].sy = screenY - halfHeight;
			vertBuf[2].w = rhw;
#ifdef XWA_MODERN
			vertBuf[2].lightIntensity = g_renderZeroFloat;
#endif
			vertBuf[2].litColor[0] = g_renderUnitFloat;
			vertBuf[2].litColor[1] = g_renderZeroFloat;
			vertBuf[2].litColor[2] = g_renderZeroFloat;
			vertBuf[2].litColor[3] = g_renderZeroFloat;
			vertBuf[2].tu = g_currentQuadTexCoords[2].u * uScale;
			vertBuf[2].tv = g_currentQuadTexCoords[2].v * vScale;
			vertBuf[2].extraLayerUVCount = 0;

			vertBuf[3].sx = screenX + halfWidth;
			vertBuf[3].sy = screenY - halfHeight;
			vertBuf[3].w = rhw;
#ifdef XWA_MODERN
			vertBuf[3].lightIntensity = g_renderZeroFloat;
#endif
			vertBuf[3].litColor[0] = g_renderUnitFloat;
			vertBuf[3].litColor[1] = g_renderZeroFloat;
			vertBuf[3].litColor[2] = g_renderZeroFloat;
			vertBuf[3].litColor[3] = g_renderZeroFloat;
			vertBuf[3].tu = g_currentQuadTexCoords[3].u * uScale;
			vertBuf[3].tv = g_currentQuadTexCoords[3].v * vScale;
			vertBuf[3].extraLayerUVCount = 0;

			RenderBillboard_ClipScreenAlignedQuad(vertBuf);

			if (g_clipCountA > 2) {
				int i;

				if (g_clipCountA + g_d3dVertexCount > g_maxBatchVerts ||
					g_clipCountA + g_d3dIndexCount > g_maxBatchTris) {
					std3D_LockExecuteBuffer();
					std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
					std3D_BeginInstructions();
					std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
					std3D_ExecuteBuffer();
					g_d3dIndexCount = 0;
					g_d3dVertexCount = 0;
				}

				for (i = 0; i < g_clipCountA; ++i) {
					int vertIndex;
					float screenY;
					float texU;
					float texV;
					float vertW;
					float depth;

					vertIndex = g_clipIdxA[i];
					screenY = vertBuf[vertIndex].sy;
					texU = vertBuf[vertIndex].tu;
					texV = vertBuf[vertIndex].tv;
					vertW = vertBuf[vertIndex].w;
					g_flightVertexBuffer[g_d3dVertexCount].sx = vertBuf[vertIndex].sx + g_flightVpOriginX;
					g_flightVertexBuffer[g_d3dVertexCount].sy = screenY + g_flightVpOriginY;
					depth = vertW * g_depthProjScale / (vertW * g_depthProjScale + g_projScale);
					if (depth < g_renderMinD3DDepth) {
						depth = g_renderMinD3DDepth;
					}
					if (g_std3DZCmpMode == 2) {
						depth = g_renderUnitFloat - depth;
					}

					g_flightVertexBuffer[g_d3dVertexCount].sz = depth;
					g_flightVertexBuffer[g_d3dVertexCount].rhw = vertW;
					g_flightVertexBuffer[g_d3dVertexCount].tu = texU;
					g_flightVertexBuffer[g_d3dVertexCount].tv = texV;
					g_flightVertexBuffer[g_d3dVertexCount].color = particle->argbColor;
					g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
					g_clipIdxA[i] = g_d3dVertexCount++;
				}

				for (i = 2; i < g_clipCountA; ++i) {
					g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
					g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[i - 1];
					g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[i];
					g_triBuffer[g_d3dIndexCount].texture = cacheNode;
					g_triBuffer[g_d3dIndexCount].flags = (int)renderFlags;
					++g_d3dIndexCount;
				}
			}
		}

		particle = particle->next;
	}
}

// FUNCTION: XWA 0x44CAC0
void RenderBillboard_DrawOriented(ParticleEffect* effect, unsigned int renderFlags) {
	ParticleEffectTemplate* effectDef;
	ParticleRecord* particle;
	ObjectRecord* object;
	MobileObject** mobjRef;
	Vec3f centerRel;
	float uScale;
	float vScale;
	Std3DTexCacheNode* cacheNode;
	TexLevel* curTexLevel;
	Matrix3x3 orient;
	float axisAngle[4];
	Matrix3x3 spin;

	effectDef = effect->def;
	particle = effect->particles;
	object = &g_objectTable[effect->objectIdx];
	mobjRef = &g_objectTable[effect->objectIdx].mobj;
	cacheNode = NULL;
	uScale = 1.0f;
	vScale = 1.0f;

	orient.m[0] = (float)(*mobjRef)->cachedSideX * 0.000030518509f;
	orient.m[1] = (float)(*mobjRef)->cachedSideY * 0.000030518509f;
	orient.m[2] = (float)(*mobjRef)->cachedSideZ * 0.000030518509f;
	orient.m[3] = -((float)(*mobjRef)->cachedFwdX * 0.000030518509f);
	orient.m[4] = -((float)(*mobjRef)->cachedFwdY * 0.000030518509f);
	orient.m[5] = -((float)(*mobjRef)->cachedFwdZ * 0.000030518509f);
	orient.m[6] = (float)(*mobjRef)->cachedUpX * 0.000030518509f;
	orient.m[7] = (float)(*mobjRef)->cachedUpY * 0.000030518509f;
	orient.m[8] = (float)(*mobjRef)->cachedUpZ * 0.000030518509f;

	centerRel = effect->localOffset;
	if ((*mobjRef)->spinAngleQ16 != 0) {
		float renderOffsetX;
		float renderOffsetY;
		float renderOffsetZ;

		renderOffsetX = (float)(*mobjRef)->renderOffsetX;
		renderOffsetY = (float)(*mobjRef)->renderOffsetY;
		renderOffsetZ = (float)(*mobjRef)->renderOffsetZ;
		centerRel.x -= renderOffsetX;
		centerRel.y -= renderOffsetY;
		centerRel.z -= renderOffsetZ;
		axisAngle[0] = (*mobjRef)->spinAxisX;
		axisAngle[1] = (*mobjRef)->spinAxisY;
		axisAngle[2] = (*mobjRef)->spinAxisZ;
		axisAngle[3] = (float)-((double)(*mobjRef)->spinAngleQ16 * 0.00009587379924285257);
		Math3D_BuildAxisAngleMatrix(&spin, axisAngle);
		Math3D_RotateVec3(&centerRel, &spin);
		centerRel.x += renderOffsetX;
		centerRel.y += renderOffsetY;
		centerRel.z += renderOffsetZ;
		Math3D_RotateVec3(&centerRel, &orient);
		Math3D_MulMatrix3x3(&orient, &spin);
	} else {
		Math3D_RotateVec3(&centerRel, &orient);
	}

	{
		float cameraPanDeltaZ;
		float relYBase;
		float relZBase;

		cameraPanDeltaZ = (float)g_players[g_localPlayer].viewState.cameraPanDeltaZ;
		relYBase = -g_players[g_localPlayer].hardpointWorldY -
				   (float)g_players[g_localPlayer].viewState.cameraPanDeltaY * 0.0625f;
		relZBase = -g_players[g_localPlayer].hardpointWorldZ;
		centerRel.x = -g_players[g_localPlayer].hardpointWorldX -
					  (float)g_players[g_localPlayer].viewState.cameraPanDeltaX * 0.0625f + centerRel.x;
		centerRel.y = relYBase + centerRel.y;
		centerRel.z = relZBase - cameraPanDeltaZ * 0.0625f + centerRel.z;
	}
	if (TRANSFM2_ViewTransformZ(centerRel.x, centerRel.y, centerRel.z) < g_renderUnitFloat) {
		return;
	}

	if (effect->textureFrameCount == 0) {
		curTexLevel = effectDef->staticTexLevel;
		if (curTexLevel->image != NULL) {
			Std3DTextureSurface* textureSurface;

			textureSurface = (Std3DTextureSurface*)curTexLevel->image;
			cacheNode = &textureSurface->cacheNode;
			std3D_AddToTextureCache(textureSurface);
			if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
				int squareWidth;
				int squareHeight;

				vScale = g_renderUnitFloat;
				uScale = 1.0f;
				squareWidth = curTexLevel->width;
				squareHeight = curTexLevel->height;
				if (squareWidth > squareHeight) {
					do {
						vScale *= g_renderHalfFloat;
						squareHeight <<= 1;
					} while (squareHeight < squareWidth);
				} else if (squareWidth < squareHeight) {
					do {
						uScale *= g_renderHalfFloat;
						squareWidth <<= 1;
					} while (squareWidth < squareHeight);
				}
			}
		}
	}

	while (particle != NULL) {
		ProjVertex vertBuf[40];
		Vec3f particleOffset;
		float relX;
		float relY;
		float relZ;
		float viewXf;
		float viewYf;
		float viewZf;
		float invViewZ;
		float rhw;
		float billboardScale;
		float halfWidth;
		float halfHeight;
		float screenX;
		float screenY;
		int clippedCount;

		if (effect->textureFrameCount != 0) {
			FeDiskIo_SelectTextureFrame(
				effect->textureModelType,
				(uint16_t)(((uint16_t)(int)((float)particle->ageTicks * effectDef->colorDeltaScale *
											effect->textureAnimRate) %
							effect->textureFrameCount) +
						   1),
				256);
			curTexLevel = g_modelTypeTable[effect->textureModelType].curTexLevel;
			if (curTexLevel->image != NULL) {
				Std3DTextureSurface* textureSurface;

				textureSurface = (Std3DTextureSurface*)curTexLevel->image;
				cacheNode = &textureSurface->cacheNode;
				std3D_AddToTextureCache(textureSurface);
				if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
					int squareWidth;
					int squareHeight;

					vScale = g_renderUnitFloat;
					uScale = 1.0f;
					squareWidth = curTexLevel->width;
					squareHeight = curTexLevel->height;
					if (squareWidth > squareHeight) {
						do {
							vScale *= g_renderHalfFloat;
							squareHeight <<= 1;
						} while (squareHeight < squareWidth);
					} else if (squareWidth < squareHeight) {
						do {
							uScale *= g_renderHalfFloat;
							squareWidth <<= 1;
						} while (squareWidth < squareHeight);
					}
				}
			}
		}

		particleOffset = particle->world;
		Math3D_RotateVec3(&particleOffset, &orient);
		particleOffset.x = centerRel.x + particleOffset.x;
		particleOffset.y = centerRel.y + particleOffset.y;
		relY = particleOffset.y;
		particleOffset.z = centerRel.z + particleOffset.z;
		relX = particleOffset.x;
		relZ = particleOffset.z;
		viewXf = (float)TRANSFM2_ViewTransformX(relX, relY, relZ);
		viewYf = (float)TRANSFM2_ViewTransformY(relX, relY, relZ);
		viewZf = (float)TRANSFM2_ViewTransformZ(relX, relY, relZ);
		if (viewZf > g_renderUnitFloat) {
			invViewZ = g_renderUnitFloat / viewZf;
			rhw = g_projScale * invViewZ;
			billboardScale = (float)effectDef->billboardScale;
			halfWidth = (float)(curTexLevel->width << curTexLevel->shift) * invViewZ * particle->size *
						billboardScale;
			halfHeight = (float)(curTexLevel->height << curTexLevel->shift) * invViewZ * particle->size *
						 billboardScale;
			screenX = viewXf * rhw + g_flightVpCenterXf;
			screenY = viewYf * rhw + g_projOffsetYf + g_flightVpCenterYf;

			g_clipCountA = 4;
			g_clipVertCursor = 4;
			g_clipIdxA[0] = 0;
			g_clipIdxA[1] = 1;
			g_clipIdxA[2] = 2;
			g_clipIdxA[3] = 3;

			vertBuf[0].sx = screenX + halfWidth;
			vertBuf[0].sy = screenY + halfHeight;
			vertBuf[0].w = rhw;
			vertBuf[0].lightIntensity = g_renderZeroFloat;
			vertBuf[0].litColor[0] = g_renderUnitFloat;
			vertBuf[0].litColor[1] = g_renderZeroFloat;
			vertBuf[0].litColor[2] = g_renderZeroFloat;
			vertBuf[0].litColor[3] = g_renderZeroFloat;
			vertBuf[0].tu = g_currentQuadTexCoords[0].u * uScale;
			vertBuf[0].tv = g_currentQuadTexCoords[0].v * vScale;
			vertBuf[0].extraLayerUVCount = 0;

			vertBuf[1].sx = screenX - halfWidth;
			vertBuf[1].sy = screenY + halfHeight;
			vertBuf[1].w = rhw;
			vertBuf[1].lightIntensity = g_renderZeroFloat;
			vertBuf[1].litColor[0] = g_renderUnitFloat;
			vertBuf[1].litColor[1] = g_renderZeroFloat;
			vertBuf[1].litColor[2] = g_renderZeroFloat;
			vertBuf[1].litColor[3] = g_renderZeroFloat;
			vertBuf[1].tu = g_currentQuadTexCoords[1].u * uScale;
			vertBuf[1].tv = g_currentQuadTexCoords[1].v * vScale;
			vertBuf[1].extraLayerUVCount = 0;

			vertBuf[2].sx = screenX - halfWidth;
			vertBuf[2].sy = screenY - halfHeight;
			vertBuf[2].w = rhw;
			vertBuf[2].lightIntensity = g_renderZeroFloat;
			vertBuf[2].litColor[0] = g_renderUnitFloat;
			vertBuf[2].litColor[1] = g_renderZeroFloat;
			vertBuf[2].litColor[2] = g_renderZeroFloat;
			vertBuf[2].litColor[3] = g_renderZeroFloat;
			vertBuf[2].tu = g_currentQuadTexCoords[2].u * uScale;
			vertBuf[2].tv = g_currentQuadTexCoords[2].v * vScale;
			vertBuf[2].extraLayerUVCount = 0;

			vertBuf[3].sx = screenX + halfWidth;
			vertBuf[3].sy = screenY - halfHeight;
			vertBuf[3].w = rhw;
			vertBuf[3].lightIntensity = g_renderZeroFloat;
			vertBuf[3].litColor[0] = g_renderUnitFloat;
			vertBuf[3].litColor[1] = g_renderZeroFloat;
			vertBuf[3].litColor[2] = g_renderZeroFloat;
			vertBuf[3].litColor[3] = g_renderZeroFloat;
			vertBuf[3].tu = g_currentQuadTexCoords[3].u * uScale;
			vertBuf[3].tv = g_currentQuadTexCoords[3].v * vScale;
			vertBuf[3].extraLayerUVCount = 0;

			g_clipCountB = 0;
			clippedCount = 0;
			{
				int prevVert;
				int i;

#ifdef XWA_MODERN
				prevVert = 0;
				if (g_clipCountA > 0) {
					prevVert = g_clipIdxA[g_clipCountA - 1];
				}
#else
				prevVert = g_clipIdxA[g_clipCountA - 1];
#endif
				if (g_clipCountA > 0) {
					for (i = 0; i < g_clipCountA; ++i) {
						int curVert;

						curVert = g_clipIdxA[i];
						RenderClip_ClipPolyTop(prevVert, curVert, vertBuf);
						prevVert = curVert;
					}
					clippedCount = g_clipCountB;
				}

				g_clipCountA = 0;
#ifdef XWA_MODERN
				prevVert = 0;
				if (clippedCount > 0) {
					prevVert = g_clipIdxB[clippedCount - 1];
				}
#else
				prevVert = g_clipIdxB[clippedCount - 1];
#endif
				if (clippedCount > 0) {
					for (i = 0; i < g_clipCountB; ++i) {
						int curVert;

						curVert = g_clipIdxB[i];
						RenderClip_ClipPolyBottom(prevVert, curVert, vertBuf);
						prevVert = curVert;
					}
					clippedCount = g_clipCountA;
				}

				g_clipCountB = 0;
#ifdef XWA_MODERN
				prevVert = 0;
				if (clippedCount > 0) {
					prevVert = g_clipIdxA[clippedCount - 1];
				}
#else
				prevVert = g_clipIdxA[clippedCount - 1];
#endif
				if (clippedCount > 0) {
					for (i = 0; i < g_clipCountA; ++i) {
						int curVert;

						curVert = g_clipIdxA[i];
						RenderClip_ClipPolyLeft(prevVert, curVert, vertBuf);
						prevVert = curVert;
					}
					clippedCount = g_clipCountB;
				}

				g_clipCountA = 0;
#ifdef XWA_MODERN
				prevVert = 0;
				if (clippedCount > 0) {
					prevVert = g_clipIdxB[clippedCount - 1];
				}
#else
				prevVert = g_clipIdxB[clippedCount - 1];
#endif
				if (clippedCount > 0) {
					for (i = 0; i < g_clipCountB; ++i) {
						int curVert;

						curVert = g_clipIdxB[i];
						RenderClip_ClipPolyRight(prevVert, curVert, vertBuf);
						prevVert = curVert;
					}
					clippedCount = g_clipCountA;
				}
			}

			if (g_clipCountA > 2) {
				int i;

				if (g_clipCountA + g_d3dVertexCount > g_maxBatchVerts ||
					g_clipCountA + g_d3dIndexCount > g_maxBatchTris) {
					std3D_LockExecuteBuffer();
					std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
					std3D_BeginInstructions();
					std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
					std3D_ExecuteBuffer();
					clippedCount = g_clipCountA;
					g_d3dIndexCount = 0;
					g_d3dVertexCount = 0;
				}

				for (i = 0; i < g_clipCountA; ++i) {
					ProjVertex* vert;
					float vertSy;
					float vertW;
					float vertTu;
					float vertTv;

					vert = &vertBuf[g_clipIdxA[i]];
					vertSy = vert->sy;
					vertTu = vert->tu;
					vertTv = vert->tv;
					vertW = vert->w;
					g_flightVertexBuffer[g_d3dVertexCount].sx = vert->sx + g_flightVpOriginX;
					g_flightVertexBuffer[g_d3dVertexCount].sy = vertSy + g_flightVpOriginY;
					g_flightVertexBuffer[g_d3dVertexCount].sz = vertW;
					g_flightVertexBuffer[g_d3dVertexCount].rhw = vertW;
					g_flightVertexBuffer[g_d3dVertexCount].tu = vertTu;
					g_flightVertexBuffer[g_d3dVertexCount].tv = vertTv;
					g_flightVertexBuffer[g_d3dVertexCount].color = particle->argbColor;
					g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
					g_clipIdxA[i] = g_d3dVertexCount++;
				}

				for (i = 2; i < g_clipCountA; ++i) {
					g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
					g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[i - 1];
					g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[i];
					g_triBuffer[g_d3dIndexCount].texture = cacheNode;
					g_triBuffer[g_d3dIndexCount].flags = (int)renderFlags;
					++g_d3dIndexCount;
				}
			}
		}

		particle = particle->next;
	}
}

// FUNCTION: XWA 0x44D6C0
void RenderBillboard_DrawStretched(ParticleEffect* effect, unsigned int renderFlags) {
	ParticleEffectTemplate* effectDef;
	ParticleRecord* particle;
	TexLevel* curTexLevel;
	Std3DTexCacheNode* cacheNode;
	float uScale;
	float vScale;
	float centerRelX;
	float centerRelY;
	float centerRelZ;

	effectDef = effect->def;
	particle = effect->particles;
	cacheNode = NULL;
	uScale = g_renderUnitFloat;
	vScale = 1.0f;

	centerRelX = effect->world.x - (float)g_players[g_localPlayer].viewState.savedTargetX;
	centerRelY = effect->world.y - (float)g_players[g_localPlayer].viewState.savedTargetY;
	centerRelZ = effect->world.z - (float)g_players[g_localPlayer].viewState.savedTargetZ;
	if (TRANSFM2_ViewTransformZ(centerRelX, centerRelY, centerRelZ) < g_renderUnitFloat) {
		return;
	}

	if (effect->textureFrameCount == 0) {
		curTexLevel = effectDef->staticTexLevel;
		if (curTexLevel->image != NULL) {
			Std3DTextureSurface* textureSurface;

			textureSurface = (Std3DTextureSurface*)curTexLevel->image;
			cacheNode = &textureSurface->cacheNode;
			std3D_AddToTextureCache(textureSurface);
			if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
				int squareWidth;
				int squareHeight;

				vScale = g_renderUnitFloat;
				uScale = 1.0f;
				squareWidth = curTexLevel->width;
				squareHeight = curTexLevel->height;
				if (squareWidth > squareHeight) {
					do {
						vScale *= g_renderHalfFloat;
						squareHeight <<= 1;
					} while (squareHeight < squareWidth);
				} else if (squareWidth < squareHeight) {
					do {
						uScale *= g_renderHalfFloat;
						squareWidth <<= 1;
					} while (squareWidth < squareHeight);
				}
			}
		}
	}

	while (particle != NULL) {
		ProjVertex vertBuf[40];
		float relX;
		float relY;
		float relZ;
		float viewXf;
		float viewYf;
		float viewZf;
		float screenX0;
		float screenY0;
		float thickness;
		float stretchRelX;
		float stretchRelY;
		float stretchRelZ;
		float stretchViewX;
		float stretchViewY;
		float stretchViewZ;
		float screenX1;
		float screenY1;
		float rhw1;
		float angle;
		float sinWidth;
		float cosWidth;
		float negSinWidth;
		float negCosWidth;
		unsigned int clippedEndpointCount;

		if (effect->textureFrameCount != 0) {
			FeDiskIo_SelectTextureFrame(
				effect->textureModelType,
				(uint16_t)(((uint16_t)(int)(effectDef->colorDeltaScale * effect->textureAnimRate *
											particle->ageTicks) %
							effect->textureFrameCount) +
						   1),
				256);
			curTexLevel = g_modelTypeTable[effect->textureModelType].curTexLevel;
			if (curTexLevel->image != NULL) {
				Std3DTextureSurface* textureSurface;

				textureSurface = (Std3DTextureSurface*)curTexLevel->image;
				cacheNode = &textureSurface->cacheNode;
				std3D_AddToTextureCache(textureSurface);
				if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
					int squareWidth;
					int squareHeight;

					uScale = 1.0f;
					vScale = g_renderUnitFloat;
					squareWidth = curTexLevel->width;
					squareHeight = curTexLevel->height;
					if (squareWidth > squareHeight) {
						do {
							vScale *= g_renderHalfFloat;
							squareHeight <<= 1;
						} while (squareHeight < squareWidth);
					} else if (squareWidth < squareHeight) {
						do {
							uScale *= g_renderHalfFloat;
							squareWidth <<= 1;
						} while (squareWidth < squareHeight);
					}
				}
			}
		}

		clippedEndpointCount = 0;
		relX = particle->world.x - (float)g_players[g_localPlayer].viewState.savedTargetX;
		relY = particle->world.y - (float)g_players[g_localPlayer].viewState.savedTargetY;
		relZ = particle->world.z - (float)g_players[g_localPlayer].viewState.savedTargetZ;
		viewXf = (float)TRANSFM2_ViewTransformX(relX, relY, relZ);
		viewYf = (float)TRANSFM2_ViewTransformY(relX, relY, relZ);
		viewZf = (float)TRANSFM2_ViewTransformZ(relX, relY, relZ);
		if (viewZf < g_renderUnitFloat) {
			clippedEndpointCount = 1;
			screenX0 = viewXf;
			screenY0 = viewYf;
			viewZf -= g_renderUnitFloat;
			thickness = (float)(curTexLevel->width << curTexLevel->shift) * particle->size *
							(float)effect->def->billboardScale -
						g_renderNegHalfFloat;
		} else {
			viewZf = g_renderUnitFloat / viewZf;
			thickness = (float)(curTexLevel->width << curTexLevel->shift) * particle->size *
							(float)effect->def->billboardScale * viewZf -
						g_renderNegHalfFloat;
			viewZf *= g_projScale;
			screenX0 = viewXf * viewZf + g_flightVpCenterXf;
			screenY0 = viewYf * viewZf + g_projOffsetYf;
			screenY0 += g_flightVpCenterYf;
		}

		stretchRelX = particle->world.x - particle->vel.x * g_renderHalfFloat -
					  (float)g_players[g_localPlayer].viewState.savedTargetX;
		stretchRelY = particle->world.y - particle->vel.y * g_renderHalfFloat -
					  (float)g_players[g_localPlayer].viewState.savedTargetY;
		stretchRelZ = particle->world.z - particle->vel.z * g_renderHalfFloat -
					  (float)g_players[g_localPlayer].viewState.savedTargetZ;
		stretchViewX = (float)TRANSFM2_ViewTransformX(stretchRelX, stretchRelY, stretchRelZ);
		stretchViewY = (float)TRANSFM2_ViewTransformY(stretchRelX, stretchRelY, stretchRelZ);
		stretchViewZ = (float)TRANSFM2_ViewTransformZ(stretchRelX, stretchRelY, stretchRelZ);
		if (stretchViewZ < g_renderUnitFloat) {
			rhw1 = stretchViewZ - g_renderUnitFloat;
			screenX1 = stretchViewX;
			screenY1 = stretchViewY;
			++clippedEndpointCount;
		} else {
			rhw1 = g_projScale / stretchViewZ;
			screenX1 = stretchViewX * rhw1 + g_flightVpCenterXf;
			screenY1 = stretchViewY * rhw1 + g_projOffsetYf;
			screenY1 += g_flightVpCenterYf;
		}

		angle = (float)atan2((double)(screenY0 - screenY1), (double)(screenX1 - screenX0));
		cosWidth = (float)cos((double)angle) * thickness;
		sinWidth = (float)sin((double)angle) * thickness;
		negCosWidth = -cosWidth;
		negSinWidth = -sinWidth;

		if (clippedEndpointCount < 1) {
			int clippedCount;

			vertBuf[0].sx = screenX0 + sinWidth;
			g_clipCountA = 4;
			g_clipVertCursor = 4;
			g_clipIdxA[0] = 0;
			g_clipIdxA[1] = 1;
			g_clipIdxA[2] = 2;
			g_clipIdxA[3] = 3;

			vertBuf[0].sy = screenY0 + cosWidth;
			vertBuf[0].litColor[1] = g_renderZeroFloat;
			vertBuf[0].litColor[2] = g_renderZeroFloat;
			vertBuf[0].litColor[3] = g_renderZeroFloat;
			vertBuf[0].litColor[0] = g_renderUnitFloat;
			vertBuf[0].tu = g_currentQuadTexCoords[0].u * uScale;
			vertBuf[0].tv = g_currentQuadTexCoords[0].v * vScale;
			vertBuf[0].w = viewZf;
			vertBuf[0].extraLayerUVCount = 0;

			vertBuf[1].sx = screenX0 + negSinWidth;
			vertBuf[1].sy = screenY0 + negCosWidth;
			vertBuf[1].litColor[1] = g_renderZeroFloat;
			vertBuf[1].litColor[2] = g_renderZeroFloat;
			vertBuf[1].litColor[3] = g_renderZeroFloat;
			vertBuf[1].litColor[0] = g_renderUnitFloat;
			vertBuf[1].tu = g_currentQuadTexCoords[1].u * uScale;
			vertBuf[1].tv = g_currentQuadTexCoords[1].v * vScale;
			vertBuf[1].w = viewZf;
			vertBuf[1].extraLayerUVCount = 0;

			vertBuf[2].sx = screenX1 + negSinWidth;
			vertBuf[2].sy = screenY1 + negCosWidth;
			vertBuf[2].litColor[1] = g_renderZeroFloat;
			vertBuf[2].litColor[2] = g_renderZeroFloat;
			vertBuf[2].litColor[3] = g_renderZeroFloat;
			vertBuf[2].litColor[0] = g_renderUnitFloat;
			vertBuf[2].tu = g_currentQuadTexCoords[2].u * uScale;
			vertBuf[2].tv = g_currentQuadTexCoords[2].v * vScale;
			vertBuf[2].w = rhw1;
			vertBuf[2].extraLayerUVCount = 0;

			vertBuf[3].sx = screenX1 + sinWidth;
			vertBuf[3].sy = screenY1 + cosWidth;
			vertBuf[3].litColor[1] = g_renderZeroFloat;
			vertBuf[3].litColor[2] = g_renderZeroFloat;
			vertBuf[3].litColor[3] = g_renderZeroFloat;
			vertBuf[3].litColor[0] = g_renderUnitFloat;
			vertBuf[3].tu = g_currentQuadTexCoords[3].u * uScale;
			vertBuf[3].tv = g_currentQuadTexCoords[3].v * vScale;
			vertBuf[3].w = rhw1;
			vertBuf[3].extraLayerUVCount = 0;

			clippedCount = RenderBillboard_ClipScreenAlignedQuad(vertBuf);
			if (g_clipCountA > 2) {
				int i;

				if (g_clipCountA + g_d3dVertexCount > g_maxBatchVerts ||
					g_clipCountA + g_d3dIndexCount > g_maxBatchTris) {
					std3D_LockExecuteBuffer();
					std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
					std3D_BeginInstructions();
					std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
					std3D_ExecuteBuffer();
					clippedCount = g_clipCountA;
					g_d3dIndexCount = 0;
					g_d3dVertexCount = 0;
				}

				for (i = 0; i < g_clipCountA; ++i) {
					int vertIndex;
					float vertSy;
					float vertTu;
					float vertTv;
					float vertW;
					float depth;

					vertIndex = g_clipIdxA[i];
					vertSy = vertBuf[vertIndex].sy;
					vertTu = vertBuf[vertIndex].tu;
					vertTv = vertBuf[vertIndex].tv;
					vertW = vertBuf[vertIndex].w;
					g_flightVertexBuffer[g_d3dVertexCount].sx = vertBuf[vertIndex].sx + g_flightVpOriginX;
					g_flightVertexBuffer[g_d3dVertexCount].sy = vertSy + g_flightVpOriginY;
					depth = vertW * g_depthProjScale;
					depth = depth / (depth + g_projScale);
					if (depth < g_renderMinD3DDepth) {
						depth = g_renderMinD3DDepth;
					}
					if (g_std3DZCmpMode == 2) {
						depth = g_renderUnitFloat - depth;
					}
					g_flightVertexBuffer[g_d3dVertexCount].sz = depth;
					g_flightVertexBuffer[g_d3dVertexCount].rhw = vertW;
					g_flightVertexBuffer[g_d3dVertexCount].tu = vertTu;
					g_flightVertexBuffer[g_d3dVertexCount].tv = vertTv;
					g_flightVertexBuffer[g_d3dVertexCount].color = particle->argbColor;
					g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
					g_clipIdxA[i] = g_d3dVertexCount++;
				}

				for (i = 2; i < g_clipCountA; ++i) {
					g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
					g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[i - 1];
					g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[i];
					g_triBuffer[g_d3dIndexCount].texture = cacheNode;
					g_triBuffer[g_d3dIndexCount].flags = (int)renderFlags;
					++g_d3dIndexCount;
				}
			}
		}

		particle = particle->next;
	}
}

// FUNCTION: XWA 0x44E160
void RenderBillboard_DrawOrientedStretched(ParticleEffect* effect, unsigned int renderFlags) {
	ParticleEffectTemplate* effectDef;
	ParticleRecord* particle;
	MobileObject** mobjRef;
	Matrix3x3 orient;
	Vec3f centerRel;
	float axisAngle[4];
	Matrix3x3 spin;
	TexLevel* curTexLevel;
	Std3DTexCacheNode* cacheNode;
	float uScale;
	float vScale;

	effectDef = effect->def;
	particle = effect->particles;
	cacheNode = NULL;
	uScale = 1.0f;
	vScale = 1.0f;

	if (effect->textureFrameCount == 0) {
		curTexLevel = effectDef->staticTexLevel;
		if (curTexLevel->image != NULL) {
			Std3DTextureSurface* textureSurface;

			textureSurface = (Std3DTextureSurface*)curTexLevel->image;
			cacheNode = &textureSurface->cacheNode;
			std3D_AddToTextureCache(textureSurface);
			if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
				int squareWidth;
				int squareHeight;

				vScale = g_renderUnitFloat;
				uScale = 1.0f;
				squareWidth = curTexLevel->width;
				squareHeight = curTexLevel->height;
				if (squareWidth > squareHeight) {
					do {
						vScale *= g_renderHalfFloat;
						squareHeight <<= 1;
					} while (squareHeight < squareWidth);
				} else if (squareWidth < squareHeight) {
					do {
						uScale *= g_renderHalfFloat;
						squareWidth <<= 1;
					} while (squareWidth < squareHeight);
				}
			}
		}
	}

	mobjRef = &g_objectTable[effect->objectIdx].mobj;
	orient.m[0] = (float)(*mobjRef)->cachedSideX * 0.000030518509f;
	orient.m[1] = (float)(*mobjRef)->cachedSideY * 0.000030518509f;
	orient.m[2] = (float)(*mobjRef)->cachedSideZ * 0.000030518509f;
	orient.m[3] = -((float)(*mobjRef)->cachedFwdX * 0.000030518509f);
	orient.m[4] = -((float)(*mobjRef)->cachedFwdY * 0.000030518509f);
	orient.m[5] = -((float)(*mobjRef)->cachedFwdZ * 0.000030518509f);
	orient.m[6] = (float)(*mobjRef)->cachedUpX * 0.000030518509f;
	orient.m[7] = (float)(*mobjRef)->cachedUpY * 0.000030518509f;
	orient.m[8] = (float)(*mobjRef)->cachedUpZ * 0.000030518509f;

	centerRel = effect->localOffset;
	if ((*mobjRef)->spinAngleQ16 != 0) {
		float renderOffsetX;
		float renderOffsetY;
		float renderOffsetZ;

		renderOffsetX = (float)(*mobjRef)->renderOffsetX;
		renderOffsetY = (float)(*mobjRef)->renderOffsetY;
		renderOffsetZ = (float)(*mobjRef)->renderOffsetZ;
		centerRel.x -= renderOffsetX;
		centerRel.y -= renderOffsetY;
		centerRel.z -= renderOffsetZ;
		axisAngle[0] = (*mobjRef)->spinAxisX;
		axisAngle[1] = (*mobjRef)->spinAxisY;
		axisAngle[2] = (*mobjRef)->spinAxisZ;
		axisAngle[3] = (float)-((double)(*mobjRef)->spinAngleQ16 * 0.00009587379924285257);
		Math3D_BuildAxisAngleMatrix(&spin, axisAngle);
		Math3D_RotateVec3(&centerRel, &spin);
		centerRel.x += renderOffsetX;
		centerRel.y += renderOffsetY;
		centerRel.z += renderOffsetZ;
		Math3D_RotateVec3(&centerRel, &orient);
		Math3D_MulMatrix3x3(&orient, &spin);
	} else {
		Math3D_RotateVec3(&centerRel, &orient);
	}

	{
		float cameraPanDeltaZ;
		float relYBase;
		float relZBase;

		cameraPanDeltaZ = (float)g_players[g_localPlayer].viewState.cameraPanDeltaZ;
		relYBase = -g_players[g_localPlayer].hardpointWorldY -
				   (float)g_players[g_localPlayer].viewState.cameraPanDeltaY * 0.0625f;
		relZBase = -g_players[g_localPlayer].hardpointWorldZ;
		centerRel.x = -g_players[g_localPlayer].hardpointWorldX -
					  (float)g_players[g_localPlayer].viewState.cameraPanDeltaX * 0.0625f + centerRel.x;
		centerRel.y = relYBase + centerRel.y;
		centerRel.z = relZBase - cameraPanDeltaZ * 0.0625f + centerRel.z;
	}
	if (TRANSFM2_ViewTransformZ(centerRel.x, centerRel.y, centerRel.z) < g_renderUnitFloat) {
		return;
	}

	while (particle != NULL) {
		ProjVertex vertBuf[40];
		Vec3f particleOffset;
		float viewXf;
		float viewYf;
		float viewZf;
		float screenX0;
		float screenY0;
		float rhw0;
		float thickness;
		float screenX1;
		float screenY1;
		float rhw1;
		float angle;
		float cosWidth;
		float sinWidth;
		float negCosWidth;
		float negSinWidth;
		int clippedEndpointCount;

		if (effect->textureFrameCount != 0) {
			float frameScale;
			int frame;

			frameScale = effectDef->colorDeltaScale * effect->textureAnimRate;
			frame = (int)(frameScale * particle->ageTicks);
			frame = (frame % effect->textureFrameCount) + 1;
			FeDiskIo_SelectTextureFrame(effect->textureModelType, (uint16_t)frame, 256);
			curTexLevel = g_modelTypeTable[effect->textureModelType].curTexLevel;
			if (curTexLevel->image != NULL) {
				Std3DTextureSurface* textureSurface;

				textureSurface = (Std3DTextureSurface*)curTexLevel->image;
				cacheNode = &textureSurface->cacheNode;
				std3D_AddToTextureCache(textureSurface);
				if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
					int squareWidth;
					int squareHeight;

					vScale = g_renderUnitFloat;
					uScale = 1.0f;
					squareWidth = curTexLevel->width;
					squareHeight = curTexLevel->height;
					if (squareWidth > squareHeight) {
						do {
							vScale *= g_renderHalfFloat;
							squareHeight <<= 1;
						} while (squareHeight < squareWidth);
					} else if (squareWidth < squareHeight) {
						do {
							uScale *= g_renderHalfFloat;
							squareWidth <<= 1;
						} while (squareWidth < squareHeight);
					}
				}
			}
		}

		clippedEndpointCount = 0;
		particleOffset.x = particle->world.x - particle->vel.x * g_renderOverbrightBleedScale;
		particleOffset.y = particle->world.y - particle->vel.y * g_renderOverbrightBleedScale;
		particleOffset.z = particle->world.z - particle->vel.z * g_renderOverbrightBleedScale;
		Math3D_RotateVec3(&particleOffset, &orient);
		screenX0 = centerRel.x + particleOffset.x;
		screenY0 = centerRel.y + particleOffset.y;
		rhw0 = centerRel.z + particleOffset.z;
		viewXf = (float)TRANSFM2_ViewTransformX(screenX0, screenY0, rhw0);
		viewYf = (float)TRANSFM2_ViewTransformY(screenX0, screenY0, rhw0);
		viewZf = (float)TRANSFM2_ViewTransformZ(screenX0, screenY0, rhw0);
		if (viewZf < g_renderUnitFloat) {
			clippedEndpointCount = 1;
			screenX0 = viewXf;
			screenY0 = viewYf;
			rhw0 = viewZf - g_renderUnitFloat;
			thickness = g_renderHalfFloat - (float)(curTexLevel->width << curTexLevel->shift) *
												particle->size * (float)effect->def->billboardScale * -4.0f;
		} else {
			thickness = g_renderUnitFloat / viewZf;
			rhw0 = g_projScale * thickness;
			thickness = g_renderHalfFloat - (float)(curTexLevel->width << curTexLevel->shift) *
												particle->size * (float)effect->def->billboardScale *
												thickness * -4.0f;
			screenX0 = viewXf * rhw0 + g_flightVpCenterXf;
			screenY0 = viewYf * rhw0 + (float)g_projOffsetY + g_flightVpCenterYf;
		}

		particleOffset.x = particle->world.x;
		particleOffset.y = particle->world.y;
		particleOffset.z = particle->world.z;
		Math3D_RotateVec3(&particleOffset, &orient);
		screenX1 = centerRel.x + particleOffset.x;
		screenY1 = centerRel.y + particleOffset.y;
		rhw1 = centerRel.z + particleOffset.z;
		viewXf = (float)TRANSFM2_ViewTransformX(screenX1, screenY1, rhw1);
		viewYf = (float)TRANSFM2_ViewTransformY(screenX1, screenY1, rhw1);
		viewZf = (float)TRANSFM2_ViewTransformZ(screenX1, screenY1, rhw1);
		if (viewZf < g_renderUnitFloat) {
			rhw1 = viewZf - g_renderUnitFloat;
			screenX1 = viewXf;
			screenY1 = viewYf;
			++clippedEndpointCount;
		} else {
			rhw1 = g_projScale / viewZf;
			screenX1 = viewXf * rhw1 + g_flightVpCenterXf;
			screenY1 = viewYf * rhw1 + g_projOffsetYf + g_flightVpCenterYf;
		}

		angle = (float)atan2((double)(screenY0 - screenY1), (double)(screenX0 - screenX1));
		cosWidth = (float)cos((double)angle);
		sinWidth = (float)sin((double)angle);
		cosWidth *= thickness;
		if (cosWidth < g_renderUnitFloat) {
			cosWidth = 1.0f;
		}
		negCosWidth = -cosWidth;
		sinWidth *= thickness;
		if (sinWidth < g_renderUnitFloat) {
			sinWidth = g_renderUnitFloat;
		}
		negSinWidth = -sinWidth;

		if ((unsigned int)clippedEndpointCount < 1u) {
			int clippedCount;

			g_clipCountA = 4;
			g_clipVertCursor = 4;
			g_clipIdxA[0] = 0;
			g_clipIdxA[1] = 1;
			g_clipIdxA[2] = 2;
			g_clipIdxA[3] = 3;

			vertBuf[0].sx = screenX0 + sinWidth;
			vertBuf[0].sy = screenY0 + cosWidth;
			vertBuf[0].litColor[1] = g_renderUnitFloat;
			vertBuf[0].litColor[2] = g_renderUnitFloat;
			vertBuf[0].litColor[3] = g_renderUnitFloat;
			vertBuf[0].litColor[0] = g_renderUnitFloat;
			vertBuf[0].tu = g_currentQuadTexCoords[0].u * uScale;
			vertBuf[0].tv = g_currentQuadTexCoords[0].v * vScale;
			vertBuf[0].w = rhw0;
			vertBuf[0].extraLayerUVCount = 0;

			vertBuf[1].sx = screenX0 + negSinWidth;
			vertBuf[1].sy = screenY0 + negCosWidth;
			vertBuf[1].litColor[1] = g_renderUnitFloat;
			vertBuf[1].litColor[2] = g_renderUnitFloat;
			vertBuf[1].litColor[3] = g_renderUnitFloat;
			vertBuf[1].litColor[0] = g_renderUnitFloat;
			vertBuf[1].tu = g_currentQuadTexCoords[1].u * uScale;
			vertBuf[1].tv = g_currentQuadTexCoords[1].v * vScale;
			vertBuf[1].w = rhw0;
			vertBuf[1].extraLayerUVCount = 0;

			vertBuf[2].sx = screenX1 + negSinWidth;
			vertBuf[2].sy = screenY1 + negCosWidth;
			vertBuf[2].litColor[1] = g_renderUnitFloat;
			vertBuf[2].litColor[2] = g_renderUnitFloat;
			vertBuf[2].litColor[3] = g_renderUnitFloat;
			vertBuf[2].litColor[0] = g_renderUnitFloat;
			vertBuf[2].tu = g_currentQuadTexCoords[2].u * uScale;
			vertBuf[2].tv = g_currentQuadTexCoords[2].v * vScale;
			vertBuf[2].w = rhw1;
			vertBuf[2].extraLayerUVCount = 0;

			vertBuf[3].sx = screenX1 + sinWidth;
			vertBuf[3].sy = screenY1 + cosWidth;
			vertBuf[3].litColor[1] = 0.8f;
			vertBuf[3].litColor[2] = 0.2f;
			vertBuf[3].litColor[3] = 0.0f;
			vertBuf[3].litColor[0] = 1.0f;
			vertBuf[3].tu = g_currentQuadTexCoords[3].u * uScale;
			vertBuf[3].tv = g_currentQuadTexCoords[3].v * vScale;
			vertBuf[3].w = rhw1;
			vertBuf[3].extraLayerUVCount = 0;

			g_clipCountB = 0;
			clippedCount = 0;
			{
				int prevVert;
				int i;

#ifdef XWA_MODERN
				prevVert = 0;
				if (g_clipCountA > 0) {
					prevVert = g_clipIdxA[g_clipCountA - 1];
				}
#else
				prevVert = g_clipIdxA[g_clipCountA - 1];
#endif
				if (g_clipCountA > 0) {
					for (i = 0; i < g_clipCountA; ++i) {
						int curVert;

						curVert = g_clipIdxA[i];
						RenderClip_ClipPolyTop(prevVert, curVert, vertBuf);
						prevVert = curVert;
					}
					clippedCount = g_clipCountB;
				}
			}

			g_clipCountA = 0;
			{
				int prevVert;
				int i;

#ifdef XWA_MODERN
				prevVert = 0;
				if (clippedCount > 0) {
					prevVert = g_clipIdxB[clippedCount - 1];
				}
#else
				prevVert = g_clipIdxB[clippedCount - 1];
#endif
				if (clippedCount > 0) {
					for (i = 0; i < g_clipCountB; ++i) {
						int curVert;

						curVert = g_clipIdxB[i];
						RenderClip_ClipPolyBottom(prevVert, curVert, vertBuf);
						prevVert = curVert;
					}
					clippedCount = g_clipCountA;
				}
			}

			g_clipCountB = 0;
			if (clippedCount > 0) {
				int prevVert;
				int i;

				prevVert = g_clipIdxA[clippedCount - 1];
				for (i = 0; i < g_clipCountA; ++i) {
					int curVert;

					curVert = g_clipIdxA[i];
					RenderClip_ClipPolyLeft(prevVert, curVert, vertBuf);
					prevVert = curVert;
				}
				clippedCount = g_clipCountB;
			}

			g_clipCountA = 0;
			if (clippedCount > 0) {
				int prevVert;
				int i;

				prevVert = g_clipIdxB[clippedCount - 1];
				for (i = 0; i < g_clipCountB; ++i) {
					int curVert;

					curVert = g_clipIdxB[i];
					RenderClip_ClipPolyRight(prevVert, curVert, vertBuf);
					prevVert = curVert;
				}
				clippedCount = g_clipCountA;
			}

			if (g_clipCountA > 2) {
				if (effect->textureFrameCount != 0) {
					float frameScale;
					int frame;

					frameScale = effectDef->colorDeltaScale * effect->textureAnimRate;
					frame = (int)(frameScale * particle->ageTicks);
					frame = (frame % effect->textureFrameCount) + 1;
					FeDiskIo_SelectTextureFrame(OBJ_ParticleTextureGroup22003, (uint16_t)frame, 256);
					curTexLevel = g_modelTypeTable[OBJ_ParticleTextureGroup22003].curTexLevel;
					if (curTexLevel->image != NULL) {
						std3D_AddToTextureCache((Std3DTextureSurface*)curTexLevel->image);
						if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
							int squareWidth;
							int squareHeight;

							vScale = g_renderUnitFloat;
							uScale = 1.0f;
							squareWidth = curTexLevel->width;
							squareHeight = curTexLevel->height;
							if (squareWidth > squareHeight) {
								do {
									vScale *= g_renderHalfFloat;
									squareHeight <<= 1;
								} while (squareHeight < squareWidth);
							} else {
								while (squareWidth < squareHeight) {
									squareWidth <<= 1;
									uScale *= g_renderHalfFloat;
								}
							}
						}
					}
				}

				if (g_clipCountA + g_d3dVertexCount > g_maxBatchVerts ||
					g_clipCountA + g_d3dIndexCount > g_maxBatchTris) {
					std3D_LockExecuteBuffer();
					std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
					std3D_BeginInstructions();
					std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
					std3D_ExecuteBuffer();
					clippedCount = g_clipCountA;
					g_d3dIndexCount = 0;
					g_d3dVertexCount = 0;
				}

				{
					int i;
					ProjVertex* colorVert;

					colorVert = vertBuf;
					for (i = 0; i < g_clipCountA; ++i) {
						int vertIndex;
						float vertSy;
						float vertTu;
						float vertTv;
						float vertW;
						float depth;
#ifdef XWA_MODERN
						uint32_t alpha;
						uint32_t red;
						uint32_t green;
						uint32_t blue;
#else
						int alpha;
						int red;
						int green;
						int blue;
#endif

						vertIndex = g_clipIdxA[i];
						vertSy = vertBuf[vertIndex].sy;
						vertTu = vertBuf[vertIndex].tu;
						vertTv = vertBuf[vertIndex].tv;
						vertW = vertBuf[vertIndex].w;
						g_flightVertexBuffer[g_d3dVertexCount].sx = vertBuf[vertIndex].sx + g_flightVpOriginX;
						g_flightVertexBuffer[g_d3dVertexCount].sy = vertSy + g_flightVpOriginY;
						depth = vertW * g_depthProjScale;
						depth = depth / (depth + g_projScale);
						if (depth < g_renderMinD3DDepth) {
							depth = g_renderMinD3DDepth;
						}
						if (g_std3DZCmpMode == 2) {
							depth = g_renderUnitFloat - depth;
						}
						g_flightVertexBuffer[g_d3dVertexCount].sz = depth;
						g_flightVertexBuffer[g_d3dVertexCount].rhw = vertW;
						g_flightVertexBuffer[g_d3dVertexCount].tu = vertTu;
						g_flightVertexBuffer[g_d3dVertexCount].tv = vertTv;
						alpha = (int)(colorVert->litColor[0] * g_vertexColorAlphaScale);
						red = (int)(colorVert->litColor[1] * 255.0f);
						green = (int)(colorVert->litColor[2] * 255.0f);
						blue = (int)(colorVert->litColor[3] * 255.0f);
						++colorVert;
						g_flightVertexBuffer[g_d3dVertexCount].color =
							(uint32_t)(blue | ((green | ((red | (alpha << 8)) << 8)) << 8));
						g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
						g_clipIdxA[i] = g_d3dVertexCount++;
					}

					for (i = 2; i < g_clipCountA; ++i) {
						g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
						g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[i - 1];
						g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[i];
						g_triBuffer[g_d3dIndexCount].texture = cacheNode;
						g_triBuffer[g_d3dIndexCount].flags = (int)renderFlags;
						++g_d3dIndexCount;
					}
				}
			}
		}

		particle = particle->next;
	}
}

// FUNCTION: XWA 0x44F070
void RenderBillboard_DrawObjectTrail(ObjectTrailEmitter* trail) {
	ObjectTrailPoint* pointHead;
	float* prevWorld;
	float* nextWorld;
	Std3DTexCacheNode* cacheNode;
	float uScale;
	float vScale;
	float prevScreenX;
	float prevScreenY;
	float prevW;
	float prevPlusX;
	float prevPlusY;
	float prevMinusX;
	float prevMinusY;
	float prevTexV;
	float prevAlpha;
	int prevBehind;
	float trailScreenScale;
	float relX;
	float relY;
	float relZ;
	float viewX;
	float viewY;
	float viewZ;

	pointHead = trail->pointHead;
	cacheNode = NULL;
	uScale = 1.0f;
	vScale = 1.0f;

	{
		relX = pointHead->world.x - (float)g_players[g_localPlayer].viewState.savedTargetX;
		relY = pointHead->world.y - (float)g_players[g_localPlayer].viewState.savedTargetY;
		relZ = pointHead->world.z - (float)g_players[g_localPlayer].viewState.savedTargetZ;
		viewX = (float)TRANSFM2_ViewTransformX(relX, relY, relZ);
		viewY = (float)TRANSFM2_ViewTransformY(relX, relY, relZ);
		viewZ = (float)TRANSFM2_ViewTransformZ(relX, relY, relZ);
		if (viewZ < g_renderUnitFloat) {
			prevBehind = 1;
			prevScreenX = viewX;
			prevScreenY = viewY;
			prevW = viewZ - g_renderUnitFloat;
		} else {
			prevBehind = 0;
			prevW = (g_renderUnitFloat / viewZ) * g_projScale;
			prevScreenX = viewX * prevW + g_flightVpCenterXf;
			prevScreenY = viewY * prevW + g_projOffsetYf;
			prevScreenY = prevScreenY + g_flightVpCenterYf;
		}
		trailScreenScale = g_renderTrailScreenScaleBase / viewZ;
	}

	if (RenderBillboard_GetTrailTextureModelKey(trail) != 0) {
		if (trail->animFrameCount != 0) {
			float frameTickScale;

			frameTickScale = (float)trail->animTicksAccum * trail->animRateScale;
			FeDiskIo_SelectTextureFrame(
				trail->animModelType,
				(uint16_t)(((uint16_t)(int)(frameTickScale * trail->ageRate) % trail->animFrameCount) + 1),
				256);
		} else {
			FeDiskIo_SelectTextureFrame(trail->textureModelType, 1u, (int)trailScreenScale);
		}

		trail->curTexLevel = g_modelTypeTable[RenderBillboard_GetTrailTextureModelKey(trail)].curTexLevel;
		if (trail->curTexLevel != NULL && trail->curTexLevel->image != NULL) {
			Std3DTextureSurface* textureSurface;

			textureSurface = (Std3DTextureSurface*)trail->curTexLevel->image;
			cacheNode = &textureSurface->cacheNode;
			std3D_AddToTextureCache(textureSurface);
			if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
				TexLevel* texLevel;
				int squareWidth;
				int squareHeight;

				texLevel = trail->curTexLevel;
				uScale = 1.0f;
				vScale = g_renderUnitFloat;
				squareWidth = texLevel->width;
				squareHeight = texLevel->height;
				if (squareWidth > squareHeight) {
					do {
						vScale *= g_renderHalfFloat;
						squareHeight <<= 1;
					} while (squareHeight < squareWidth);
				} else if (squareWidth < squareHeight) {
					do {
						uScale *= g_renderHalfFloat;
						squareWidth <<= 1;
					} while (squareWidth < squareHeight);
				}
			}
		}
	}

	prevWorld = &pointHead->world.x;
	nextWorld = (float*)pointHead->next;
	prevPlusX = -9999.0f;

	if ((uint16_t)trail->pointCount + g_d3dVertexCount > g_maxBatchVerts ||
		trail->pointCount + g_d3dIndexCount > g_maxBatchTris) {
		std3D_LockExecuteBuffer();
		std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
		std3D_BeginInstructions();
		std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
		std3D_ExecuteBuffer();
		g_d3dIndexCount = 0;
		g_d3dVertexCount = 0;
	}

	while (nextWorld != NULL) {
		ProjVertex vertBuf[40];
		float nextScreenX;
		float nextScreenY;
		float nextW;
		float nextTexV;
		float nextAlpha;
		float widthFactor;
		float angle;
		float angleCos;
		float angleSin;
		float nextPlusX;
		float nextPlusY;
		float nextMinusX;
		float nextMinusY;
		int nextBehind;

		if (nextWorld[4] < g_renderUnitFloat) {
			float nextAge;

			nextAge = nextWorld[4];
			relX = nextWorld[0] - (float)g_players[g_localPlayer].viewState.savedTargetX;
			relY = nextWorld[1] - (float)g_players[g_localPlayer].viewState.savedTargetY;
			relZ = nextWorld[2] - (float)g_players[g_localPlayer].viewState.savedTargetZ;
			if (nextAge < trail->alphaFadeStart) {
				nextAlpha = 1.0f;
			} else {
				nextAlpha = g_renderUnitFloat - (nextAge - trail->alphaFadeStart) * trail->alphaFadeRate;
			}
			widthFactor = trail->startAlphaBias * nextAge - g_renderNegUnitFloat;
		} else {
			relX = prevWorld[0] - (float)g_players[g_localPlayer].viewState.savedTargetX +
				   (nextWorld[0] - prevWorld[0]) * (g_renderUnitFloat / nextWorld[4]);
			relY = prevWorld[1] - (float)g_players[g_localPlayer].viewState.savedTargetY +
				   (nextWorld[1] - prevWorld[1]) * (g_renderUnitFloat / nextWorld[4]);
			relZ = prevWorld[2] - (float)g_players[g_localPlayer].viewState.savedTargetZ +
				   (nextWorld[2] - prevWorld[2]) * (g_renderUnitFloat / nextWorld[4]);
			if (g_renderUnitFloat < trail->alphaFadeStart) {
				nextAlpha = 1.0f;
			} else {
				nextAlpha =
					g_renderUnitFloat - (g_renderUnitFloat - trail->alphaFadeStart) * trail->alphaFadeRate;
			}
			widthFactor = trail->startAlphaBias - g_renderNegUnitFloat;
		}

		viewX = (float)TRANSFM2_ViewTransformX(relX, relY, relZ);
		viewY = (float)TRANSFM2_ViewTransformY(relX, relY, relZ);
		viewZ = (float)TRANSFM2_ViewTransformZ(relX, relY, relZ);
		nextTexV = nextWorld[5];
		if (viewZ < g_renderUnitFloat) {
			nextBehind = 1;
			nextScreenX = viewX;
			nextScreenY = viewY;
			nextW = viewZ - g_renderUnitFloat;
		} else {
			nextBehind = 0;
			nextW = g_projScale / viewZ;
			nextScreenX = viewX * nextW + g_flightVpCenterXf;
			nextScreenY = viewY * nextW + g_projOffsetYf;
			nextScreenY = nextScreenY + g_flightVpCenterYf;
		}

		angle = (float)atan2((double)(prevScreenY - nextScreenY), (double)(nextScreenX - prevScreenX));
		angleCos = (float)cos((double)angle);
		angleSin = (float)sin((double)angle);
		widthFactor = widthFactor * trailScreenScale * trail->ribbonWidth;
		nextPlusY = angleCos * widthFactor;
		nextPlusX = angleSin * widthFactor;
		nextMinusY = -nextPlusY;
		nextMinusX = -nextPlusX;

		if (prevPlusX == -9999.0f) {
			if (g_renderZeroFloat < trail->alphaFadeStart) {
				prevAlpha = 1.0f;
			} else {
				prevAlpha = g_renderUnitFloat - (-trail->alphaFadeStart * trail->alphaFadeRate);
			}
			widthFactor = trailScreenScale * trail->ribbonWidth;
			prevPlusY = angleCos * widthFactor;
			prevPlusX = angleSin * widthFactor;
			prevMinusY = -prevPlusY;
			prevTexV = 0.0f;
			prevMinusX = -prevPlusX;
		}

		if (!(prevBehind && nextBehind)) {
			g_clipCountA = 4;
			g_clipVertCursor = 4;
			g_clipIdxA[0] = 0;
			g_clipIdxA[1] = 1;
			g_clipIdxA[2] = 2;
			g_clipIdxA[3] = 3;

			vertBuf[0].sx = prevScreenX + prevPlusX;
			vertBuf[0].sy = prevScreenY + prevPlusY;
			vertBuf[0].litColor[1] = g_renderZeroFloat;
			vertBuf[0].litColor[2] = g_renderZeroFloat;
			vertBuf[0].litColor[3] = g_renderZeroFloat;
			vertBuf[0].litColor[0] = prevAlpha;
			vertBuf[0].w = prevW;
			vertBuf[1].sx = prevScreenX + prevMinusX;
			vertBuf[1].sy = prevScreenY + prevMinusY;
			vertBuf[0].tu = g_currentQuadTexCoords[0].u * uScale;
			vertBuf[0].tv = prevTexV * vScale;
			vertBuf[1].litColor[1] = g_renderZeroFloat;
			vertBuf[0].extraLayerUVCount = 0;
			vertBuf[1].litColor[2] = g_renderZeroFloat;
			vertBuf[1].litColor[3] = g_renderZeroFloat;
			vertBuf[1].litColor[0] = prevAlpha;
			vertBuf[1].tu = g_currentQuadTexCoords[1].u * uScale;
			vertBuf[1].tv = prevTexV * vScale;
			vertBuf[2].sx = nextScreenX + nextMinusX;
			vertBuf[2].sy = nextScreenY + nextMinusY;
			vertBuf[1].w = prevW;
			vertBuf[1].extraLayerUVCount = 0;
			vertBuf[2].litColor[1] = g_renderZeroFloat;
			vertBuf[2].litColor[2] = g_renderZeroFloat;
			vertBuf[2].litColor[3] = g_renderZeroFloat;
			vertBuf[2].litColor[0] = nextAlpha;
			vertBuf[2].tu = g_currentQuadTexCoords[2].u * uScale;
			vertBuf[2].tv = nextTexV * vScale;
			vertBuf[2].extraLayerUVCount = 0;
			vertBuf[2].w = nextW;

			vertBuf[3].sx = nextScreenX + nextPlusX;
			vertBuf[3].sy = nextScreenY + nextPlusY;
			vertBuf[3].litColor[1] = g_renderZeroFloat;
			vertBuf[3].litColor[2] = g_renderZeroFloat;
			vertBuf[3].litColor[3] = g_renderZeroFloat;
			vertBuf[3].litColor[0] = nextAlpha;
			vertBuf[3].tu = g_currentQuadTexCoords[3].u * uScale;
			vertBuf[3].tv = nextTexV * vScale;
			vertBuf[3].w = nextW;
			vertBuf[3].extraLayerUVCount = 0;

			if (RenderBillboard_ClipTrailQuad(vertBuf, prevBehind || nextBehind) > 2) {
				RenderBillboard_EmitClippedTrailQuad(vertBuf, cacheNode, trail);
			}
		}

		prevScreenX = nextScreenX;
		prevScreenY = nextScreenY;
		prevW = nextW;
		prevPlusX = nextPlusX;
		prevPlusY = nextPlusY;
		prevMinusX = nextMinusX;
		prevMinusY = nextMinusY;
		prevAlpha = nextAlpha;
		prevTexV = nextTexV;
		prevBehind = nextBehind;
		prevWorld = nextWorld;
		nextWorld = *(float**)&nextWorld[6];
	}
}

// FUNCTION: XWA 0x407670
void Backdrop_DrawCoordinateStrip(WorldRectRecord* worldRect) {
	int frame;
	TexLevel* curTexLevel;
	int quadDrawFlags;
	int stripHalfHeight;
	int corners[12];
	int segmentIndex;
	int segmentInFrame;
	int coordIndex;

	worldRect->viewDirQ20.x =
		TRANSFM2_CamMatDotRow0(worldRect->worldDirQ20.x, worldRect->worldDirQ20.y, worldRect->worldDirQ20.z);
	worldRect->viewDirQ20.y =
		TRANSFM2_CamMatDotRow1(worldRect->worldDirQ20.x, worldRect->worldDirQ20.y, worldRect->worldDirQ20.z);
	worldRect->viewDirQ20.z =
		TRANSFM2_CamMatDotRow2(worldRect->worldDirQ20.x, worldRect->worldDirQ20.y, worldRect->worldDirQ20.z);

	if ((worldRect->flags & 1u) != 0) {
		frame = worldRect->frame;
	} else {
		frame = 1;
	}

	if (g_modelTypeTable[(uint16_t)worldRect->modelType].texLevels != NULL) {
		FeDiskIo_SelectTextureFrame((uint16_t)worldRect->modelType, (uint16_t)frame, worldRect->angularScale);
	}

	curTexLevel = g_modelTypeTable[(uint16_t)worldRect->modelType].curTexLevel;
	if (curTexLevel == NULL) {
		return;
	}

	quadDrawFlags = 0x2612;
	if (g_bilinearEnabled) {
		quadDrawFlags = 0x2792;
	}
	curTexLevel->argbColor = -1;

	stripHalfHeight = worldRect->stripHalfHeight;
	corners[0] = TRANSFM2_CamMatDotRow0(worldRect->stripCoords[0].x, worldRect->stripCoords[0].y,
										worldRect->stripCoords[0].z + stripHalfHeight);
	corners[1] = TRANSFM2_CamMatDotRow1(worldRect->stripCoords[0].x, worldRect->stripCoords[0].y,
										worldRect->stripCoords[0].z + stripHalfHeight);
	corners[2] = TRANSFM2_CamMatDotRow2(worldRect->stripCoords[0].x, worldRect->stripCoords[0].y,
										worldRect->stripCoords[0].z + stripHalfHeight);
	corners[3] = TRANSFM2_CamMatDotRow0(worldRect->stripCoords[1].x, worldRect->stripCoords[1].y,
										worldRect->stripCoords[1].z + stripHalfHeight);
	corners[4] = TRANSFM2_CamMatDotRow1(worldRect->stripCoords[1].x, worldRect->stripCoords[1].y,
										worldRect->stripCoords[1].z + stripHalfHeight);
	corners[5] = TRANSFM2_CamMatDotRow2(worldRect->stripCoords[1].x, worldRect->stripCoords[1].y,
										worldRect->stripCoords[1].z + stripHalfHeight);
	corners[6] = TRANSFM2_CamMatDotRow0(worldRect->stripCoords[1].x, worldRect->stripCoords[1].y,
										worldRect->stripCoords[1].z - stripHalfHeight);
	corners[7] = TRANSFM2_CamMatDotRow1(worldRect->stripCoords[1].x, worldRect->stripCoords[1].y,
										worldRect->stripCoords[1].z - stripHalfHeight);
	corners[8] = TRANSFM2_CamMatDotRow2(worldRect->stripCoords[1].x, worldRect->stripCoords[1].y,
										worldRect->stripCoords[1].z - stripHalfHeight);
	corners[9] = TRANSFM2_CamMatDotRow0(worldRect->stripCoords[0].x, worldRect->stripCoords[0].y,
										worldRect->stripCoords[0].z - stripHalfHeight);
	corners[10] = TRANSFM2_CamMatDotRow1(worldRect->stripCoords[0].x, worldRect->stripCoords[0].y,
										 worldRect->stripCoords[0].z - stripHalfHeight);
	corners[11] = TRANSFM2_CamMatDotRow2(worldRect->stripCoords[0].x, worldRect->stripCoords[0].y,
										 worldRect->stripCoords[0].z - stripHalfHeight);

	g_backdropStripTexCoords[0].u = 0.0f;
	g_backdropStripTexCoords[0].v = 0.0f;
	g_backdropStripTexCoords[1].u = g_renderUnitFloat / (float)worldRect->stripSegmentsPerFrame;
	g_backdropStripTexCoords[1].v = 0.0f;
	g_backdropStripTexCoords[2].u = g_renderUnitFloat / (float)worldRect->stripSegmentsPerFrame;
	g_backdropStripTexCoords[2].v = 1.0f;
	g_backdropStripTexCoords[3].u = 0.0f;
	g_backdropStripTexCoords[3].v = 1.0f;
	g_currentQuadTexCoords = g_backdropStripTexCoords;
	RenderQuad_DrawTextured3D(corners, curTexLevel, quadDrawFlags);

	segmentIndex = 2;
	segmentInFrame = segmentIndex;
	if (segmentIndex <= worldRect->stripSegmentCount) {
		coordIndex = 2;
		do {
			int stripSegmentsPerFrame;

			stripSegmentsPerFrame = worldRect->stripSegmentsPerFrame;
			if (segmentInFrame > stripSegmentsPerFrame) {
				g_backdropStripTexCoords[3].u = 0.0f;
				g_backdropStripTexCoords[0].u = 0.0f;
				segmentInFrame -= stripSegmentsPerFrame;
				++frame;
				FeDiskIo_SelectTextureFrame((uint16_t)worldRect->modelType, (uint16_t)frame,
											worldRect->angularScale);
				if (g_modelTypeTable[(uint16_t)worldRect->modelType].curTexLevel->height !=
					curTexLevel->height) {
					stripHalfHeight = g_modelTypeTable[(uint16_t)worldRect->modelType].curTexLevel->height *
									  stripHalfHeight / curTexLevel->height;
					corners[0] = TRANSFM2_CamMatDotRow0(
						worldRect->stripCoords[coordIndex - 1].x, worldRect->stripCoords[coordIndex - 1].y,
						worldRect->stripCoords[coordIndex - 1].z + stripHalfHeight);
					corners[1] = TRANSFM2_CamMatDotRow1(
						worldRect->stripCoords[coordIndex - 1].x, worldRect->stripCoords[coordIndex - 1].y,
						worldRect->stripCoords[coordIndex - 1].z + stripHalfHeight);
					corners[2] = TRANSFM2_CamMatDotRow2(
						worldRect->stripCoords[coordIndex - 1].x, worldRect->stripCoords[coordIndex - 1].y,
						worldRect->stripCoords[coordIndex - 1].z + stripHalfHeight);
					corners[9] = TRANSFM2_CamMatDotRow0(
						worldRect->stripCoords[coordIndex - 1].x, worldRect->stripCoords[coordIndex - 1].y,
						worldRect->stripCoords[coordIndex - 1].z - stripHalfHeight);
					corners[10] = TRANSFM2_CamMatDotRow1(
						worldRect->stripCoords[coordIndex - 1].x, worldRect->stripCoords[coordIndex - 1].y,
						worldRect->stripCoords[coordIndex - 1].z - stripHalfHeight);
					corners[11] = TRANSFM2_CamMatDotRow2(
						worldRect->stripCoords[coordIndex - 1].x, worldRect->stripCoords[coordIndex - 1].y,
						worldRect->stripCoords[coordIndex - 1].z - stripHalfHeight);
				} else {
					corners[0] = corners[3];
					corners[1] = corners[4];
					corners[2] = corners[5];
					corners[9] = corners[6];
					corners[10] = corners[7];
					corners[11] = corners[8];
				}
				curTexLevel = g_modelTypeTable[(uint16_t)worldRect->modelType].curTexLevel;
			} else {
				g_backdropStripTexCoords[3].u = g_backdropStripTexCoords[1].u;
				g_backdropStripTexCoords[0].u = g_backdropStripTexCoords[1].u;
				corners[0] = corners[3];
				corners[1] = corners[4];
				corners[2] = corners[5];
				corners[9] = corners[6];
				corners[10] = corners[7];
				corners[11] = corners[8];
			}

			g_backdropStripTexCoords[2].u = (float)segmentInFrame / (float)worldRect->stripSegmentsPerFrame;
			g_backdropStripTexCoords[1].u = g_backdropStripTexCoords[2].u;

			corners[3] = TRANSFM2_CamMatDotRow0(worldRect->stripCoords[coordIndex].x,
												worldRect->stripCoords[coordIndex].y,
												worldRect->stripCoords[coordIndex].z + stripHalfHeight);
			corners[4] = TRANSFM2_CamMatDotRow1(worldRect->stripCoords[coordIndex].x,
												worldRect->stripCoords[coordIndex].y,
												worldRect->stripCoords[coordIndex].z + stripHalfHeight);
			corners[5] = TRANSFM2_CamMatDotRow2(worldRect->stripCoords[coordIndex].x,
												worldRect->stripCoords[coordIndex].y,
												worldRect->stripCoords[coordIndex].z + stripHalfHeight);
			corners[6] = TRANSFM2_CamMatDotRow0(worldRect->stripCoords[coordIndex].x,
												worldRect->stripCoords[coordIndex].y,
												worldRect->stripCoords[coordIndex].z - stripHalfHeight);
			corners[7] = TRANSFM2_CamMatDotRow1(worldRect->stripCoords[coordIndex].x,
												worldRect->stripCoords[coordIndex].y,
												worldRect->stripCoords[coordIndex].z - stripHalfHeight);
			corners[8] = TRANSFM2_CamMatDotRow2(worldRect->stripCoords[coordIndex].x,
												worldRect->stripCoords[coordIndex].y,
												worldRect->stripCoords[coordIndex].z - stripHalfHeight);

			RenderQuad_DrawTextured3D(corners, curTexLevel, quadDrawFlags);
			++coordIndex;
			++segmentInFrame;
		} while (++segmentIndex <= worldRect->stripSegmentCount);
	}

	g_currentQuadTexCoords = g_defaultQuadTexCoords;
}

// FUNCTION: XWA 0x405FE0
void Backdrop_RenderCurrentRegion(void) {
	uint8_t hangar;
	int regionIndex;
	unsigned int recordIdx;
	int screenScale;
	uint16_t modelType;
	TexLevel* curTexLevel;
	int viewX;
	int viewY;
	int viewZ;
	int halfWidthPixels;
	int halfHeightPixels;
	int halfWidthAxisX;
	int halfWidthAxisY;
	int halfWidthAxisZ;
	int halfHeightAxisX;
	int halfHeightAxisY;
	int halfHeightAxisZ;
	int quadDrawFlags;
	int* corner;
	int cornerCount;

	hangar = g_missionHeader.body.hangar;
	if (hangar == XWA_HANGAR_DEATHSTAR) {
		return;
	}

	regionIndex = g_players[g_localPlayer].regionIndex;
	for (recordIdx = 0; recordIdx < (unsigned int)g_backdropCountByRegion[regionIndex]; ++recordIdx) {
		WorldRectRecord* record;
		WorldRectRecord* beamSprite;
		int flatRecordIdx;

		flatRecordIdx = regionIndex * XWA_BACKDROP_RECORDS_PER_REGION + (int)recordIdx;
		g_backdropRecordsByRegion[0][flatRecordIdx].viewDirQ20.z = -1;
		if (g_backdropsEnabled == 0 && g_backdropRecordsByRegion[0][flatRecordIdx].drawFlags != 0) {
			continue;
		}

		beamSprite = g_deathStarTunnelLaserRegions[regionIndex].beamSpriteRect;
		if (beamSprite != NULL && beamSprite == &g_backdropRecordsByRegion[0][flatRecordIdx] &&
			g_deathStarTunnelLaserRegions[regionIndex].shotActive == 0) {
			continue;
		}

		(void)TRANSFM2_CamMatDotRow0(g_backdropRecordsByRegion[0][flatRecordIdx].worldDirQ20.x,
									 g_backdropRecordsByRegion[0][flatRecordIdx].worldDirQ20.y,
									 g_backdropRecordsByRegion[0][flatRecordIdx].worldDirQ20.z);
		(void)TRANSFM2_CamMatDotRow1(g_backdropRecordsByRegion[0][flatRecordIdx].worldDirQ20.x,
									 g_backdropRecordsByRegion[0][flatRecordIdx].worldDirQ20.y,
									 g_backdropRecordsByRegion[0][flatRecordIdx].worldDirQ20.z);
		(void)TRANSFM2_CamMatDotRow2(g_backdropRecordsByRegion[0][flatRecordIdx].worldDirQ20.x,
									 g_backdropRecordsByRegion[0][flatRecordIdx].worldDirQ20.y,
									 g_backdropRecordsByRegion[0][flatRecordIdx].worldDirQ20.z);

		if ((g_backdropRecordsByRegion[0][flatRecordIdx].flags & 1u) != 0) {
			FeDiskIo_SelectTextureFrame((uint16_t)g_backdropRecordsByRegion[0][flatRecordIdx].modelType,
										g_backdropRecordsByRegion[0][flatRecordIdx].frame,
										g_backdropRecordsByRegion[0][flatRecordIdx].angularScale);
		}

		if ((g_backdropRecordsByRegion[0][flatRecordIdx].flags & 2u) != 0) {
			if (g_useHardware3D) {
				Backdrop_DrawCoordinateStrip(&g_backdropRecordsByRegion[0][flatRecordIdx]);
			}
			continue;
		}

		switch (g_backdropRecordsByRegion[0][flatRecordIdx].side) {
			case 0:
				if (g_useHardware3D) {
					Backdrop_DrawCoordinateStrip(&g_backdropRecordsByRegion[0][flatRecordIdx]);
				} else if (g_camMatR2_Y > 0) {
					int16_t angle;

					record = &g_backdropRecordsByRegion[0][flatRecordIdx];
					angle = (int16_t)-trig2_arctan(g_camMatR1_X, g_camMatR0_X);
					screenScale = record->angularScale;
					halfHeightAxisZ = g_camMatR2_Z;
					{
						int worldX;
						int worldY;
						int worldZ;

						worldZ = record->worldDirQ20.z;
						worldY = record->worldDirQ20.y;
						halfHeightAxisY = g_camMatR1_Z;
						halfWidthAxisY = g_camMatR1_X;
						halfHeightAxisX = g_camMatR0_Z;
						halfWidthAxisZ = g_camMatR2_X;
						worldX = record->worldDirQ20.x;
						halfWidthAxisX = g_camMatR0_X;
						modelType = record->modelType;

						record->viewDirQ20.x = TRANSFM2_CamMatDotRow0(worldX, worldY, worldZ);
						record->viewDirQ20.y = TRANSFM2_CamMatDotRow1(worldX, worldY, worldZ);
						record->viewDirQ20.z = TRANSFM2_CamMatDotRow2(worldX, worldY, worldZ);
					}
					if (g_useHardware3D) {
						int corners[12];

						viewX = record->viewDirQ20.x >> 9;
						viewY = record->viewDirQ20.y >> 9;
						viewZ = record->viewDirQ20.z >> 9;

						curTexLevel = g_modelTypeTable[(uint16_t)modelType].curTexLevel;
						if ((g_modelTypeTable[(uint16_t)modelType].assetFlags &
							 (MODEL_TYPE_ASSET_TEXTURE_DRAW | MODEL_TYPE_ASSET_TEXTURE_READY)) != 0 &&
							curTexLevel != NULL) {
							halfHeightPixels =
								Backdrop_ComputeTextureAxisExtent(curTexLevel->height, screenScale);
							halfWidthPixels =
								Backdrop_ComputeTextureAxisExtent(curTexLevel->width, screenScale);

							halfWidthAxisX = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisX);
							halfWidthAxisY = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisY);
							halfWidthAxisZ = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisZ);
							halfHeightAxisX = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisX);
							halfHeightAxisY = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisY);
							halfHeightAxisZ = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisZ);

							corner = corners;
							cornerCount = 4;
							do {
								corner[0] = viewX;
								corner[1] = viewY;
								corner[2] = viewZ;
								corner += 3;
							} while (--cornerCount != 0);

							corners[0] += halfHeightAxisX + halfWidthAxisX;
							corners[1] += halfWidthAxisY + halfHeightAxisY;
							corners[2] += halfWidthAxisZ + halfHeightAxisZ;
							corners[3] += halfHeightAxisX - halfWidthAxisX;
							corners[4] += halfHeightAxisY - halfWidthAxisY;
							corners[5] += halfHeightAxisZ - halfWidthAxisZ;
							corners[6] -= halfHeightAxisX + halfWidthAxisX;
							corners[7] -= halfWidthAxisY + halfHeightAxisY;
							corners[8] -= halfWidthAxisZ + halfHeightAxisZ;
							corners[9] += halfWidthAxisX - halfHeightAxisX;
							corners[10] += halfWidthAxisY - halfHeightAxisY;
							corners[11] += halfWidthAxisZ - halfHeightAxisZ;

							quadDrawFlags = 0x612;
							if (g_bilinearEnabled) {
								quadDrawFlags = 0x792;
							}

							curTexLevel->argbColor = -1;
							RenderQuad_DrawTextured3D(corners, curTexLevel, quadDrawFlags);
						}
					} else {
						Backdrop_ProjectAndDrawScreenQuad(
							record->viewDirQ20.x >> 9, record->viewDirQ20.y >> 9, record->viewDirQ20.z >> 9,
							angle, modelType, screenScale);
					}
				}
				break;

			case 1:
				if (g_useHardware3D) {
					Backdrop_DrawCoordinateStrip(&g_backdropRecordsByRegion[0][flatRecordIdx]);
				} else if (g_camMatR2_Y < 0) {
					int16_t angle;

					record = &g_backdropRecordsByRegion[0][flatRecordIdx];
					angle = (int16_t)(0x8000 - trig2_arctan(g_camMatR1_X, g_camMatR0_X));
					modelType = record->modelType;
					screenScale = record->angularScale;
					halfWidthAxisX = g_camMatR0_X;
					halfWidthAxisY = g_camMatR1_X;
					halfWidthAxisZ = g_camMatR2_X;
					halfHeightAxisX = g_camMatR0_Z;
					halfHeightAxisY = g_camMatR1_Z;
					halfHeightAxisZ = g_camMatR2_Z;
					{
						int worldX = record->worldDirQ20.x;
						int worldY = record->worldDirQ20.y;
						int worldZ = record->worldDirQ20.z;

						record->viewDirQ20.x = TRANSFM2_CamMatDotRow0(worldX, worldY, worldZ);
						record->viewDirQ20.y = TRANSFM2_CamMatDotRow1(worldX, worldY, worldZ);
						record->viewDirQ20.z = TRANSFM2_CamMatDotRow2(worldX, worldY, worldZ);
					}
					if (g_useHardware3D) {
						int corners[12];

						viewX = record->viewDirQ20.x >> 9;
						viewY = record->viewDirQ20.y >> 9;
						viewZ = record->viewDirQ20.z >> 9;

						curTexLevel = g_modelTypeTable[(uint16_t)modelType].curTexLevel;
						if ((g_modelTypeTable[(uint16_t)modelType].assetFlags &
							 (MODEL_TYPE_ASSET_TEXTURE_DRAW | MODEL_TYPE_ASSET_TEXTURE_READY)) != 0 &&
							curTexLevel != NULL) {
							halfHeightPixels =
								Backdrop_ComputeTextureAxisExtent(curTexLevel->height, screenScale);
							halfWidthPixels =
								Backdrop_ComputeTextureAxisExtent(curTexLevel->width, screenScale);

							halfWidthAxisX = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisX);
							halfWidthAxisY = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisY);
							halfWidthAxisZ = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisZ);
							halfHeightAxisX = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisX);
							halfHeightAxisY = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisY);
							halfHeightAxisZ = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisZ);

							corner = corners;
							cornerCount = 4;
							do {
								corner[0] = viewX;
								corner[1] = viewY;
								corner[2] = viewZ;
								corner += 3;
							} while (--cornerCount != 0);

							corners[0] += halfHeightAxisX + halfWidthAxisX;
							corners[1] += halfWidthAxisY + halfHeightAxisY;
							corners[2] += halfWidthAxisZ + halfHeightAxisZ;
							corners[3] += halfHeightAxisX - halfWidthAxisX;
							corners[4] += halfHeightAxisY - halfWidthAxisY;
							corners[5] += halfHeightAxisZ - halfWidthAxisZ;
							corners[6] -= halfHeightAxisX + halfWidthAxisX;
							corners[7] -= halfWidthAxisY + halfHeightAxisY;
							corners[8] -= halfWidthAxisZ + halfHeightAxisZ;
							corners[9] += halfWidthAxisX - halfHeightAxisX;
							corners[10] += halfWidthAxisY - halfHeightAxisY;
							corners[11] += halfWidthAxisZ - halfHeightAxisZ;

							quadDrawFlags = 0x612;
							if (g_bilinearEnabled) {
								quadDrawFlags = 0x792;
							}

							curTexLevel->argbColor = -1;
							RenderQuad_DrawTextured3D(corners, curTexLevel, quadDrawFlags);
						}
					} else {
						Backdrop_ProjectAndDrawScreenQuad(
							record->viewDirQ20.x >> 9, record->viewDirQ20.y >> 9, record->viewDirQ20.z >> 9,
							angle, modelType, screenScale);
					}
				}
				break;

			case 2:
				if (g_useHardware3D) {
					Backdrop_DrawCoordinateStrip(&g_backdropRecordsByRegion[0][flatRecordIdx]);
				} else if (g_camMatR2_X < 0) {
					int16_t angle;

					record = &g_backdropRecordsByRegion[0][flatRecordIdx];
					angle = (int16_t)-trig2_arctan(g_camMatR1_Y, g_camMatR0_Y);
					screenScale = record->angularScale;
					halfHeightAxisZ = g_camMatR2_Z;
					{
						int worldX;
						int worldY;
						int worldZ;

						worldZ = record->worldDirQ20.z;
						worldY = record->worldDirQ20.y;
						halfWidthAxisY = g_camMatR1_Z;
						halfHeightAxisX = g_camMatR2_Y;
						halfWidthAxisZ = g_camMatR0_Z;
						halfHeightAxisY = g_camMatR1_Y;
						worldX = record->worldDirQ20.x;
						halfWidthAxisX = g_camMatR0_Y;
						modelType = record->modelType;

						record->viewDirQ20.x = TRANSFM2_CamMatDotRow0(worldX, worldY, worldZ);
						record->viewDirQ20.y = TRANSFM2_CamMatDotRow1(worldX, worldY, worldZ);
						record->viewDirQ20.z = TRANSFM2_CamMatDotRow2(worldX, worldY, worldZ);
					}
					if (g_useHardware3D) {
						int corners[12];

						viewX = record->viewDirQ20.x >> 9;
						viewY = record->viewDirQ20.y >> 9;
						viewZ = record->viewDirQ20.z >> 9;

						curTexLevel = g_modelTypeTable[(uint16_t)modelType].curTexLevel;
						if ((g_modelTypeTable[(uint16_t)modelType].assetFlags &
							 (MODEL_TYPE_ASSET_TEXTURE_DRAW | MODEL_TYPE_ASSET_TEXTURE_READY)) != 0 &&
							curTexLevel != NULL) {
							halfHeightPixels =
								Backdrop_ComputeTextureAxisExtent(curTexLevel->height, screenScale);
							halfWidthPixels =
								Backdrop_ComputeTextureAxisExtent(curTexLevel->width, screenScale);

							halfWidthAxisX = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisX);
							halfWidthAxisY = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisY);
							halfWidthAxisZ = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisZ);
							halfHeightAxisX = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisX);
							halfHeightAxisY = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisY);
							halfHeightAxisZ = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisZ);

							corner = corners;
							cornerCount = 4;
							do {
								corner[0] = viewX;
								corner[1] = viewY;
								corner[2] = viewZ;
								corner += 3;
							} while (--cornerCount != 0);

							corners[0] += halfHeightAxisX + halfWidthAxisX;
							corners[1] += halfWidthAxisY + halfHeightAxisY;
							corners[2] += halfWidthAxisZ + halfHeightAxisZ;
							corners[3] += halfHeightAxisX - halfWidthAxisX;
							corners[4] += halfHeightAxisY - halfWidthAxisY;
							corners[5] += halfHeightAxisZ - halfWidthAxisZ;
							corners[6] -= halfHeightAxisX + halfWidthAxisX;
							corners[7] -= halfWidthAxisY + halfHeightAxisY;
							corners[8] -= halfWidthAxisZ + halfHeightAxisZ;
							corners[9] += halfWidthAxisX - halfHeightAxisX;
							corners[10] += halfWidthAxisY - halfHeightAxisY;
							corners[11] += halfWidthAxisZ - halfHeightAxisZ;

							quadDrawFlags = 0x612;
							if (g_bilinearEnabled) {
								quadDrawFlags = 0x792;
							}

							curTexLevel->argbColor = -1;
							RenderQuad_DrawTextured3D(corners, curTexLevel, quadDrawFlags);
						}
					} else {
						Backdrop_ProjectAndDrawScreenQuad(
							record->viewDirQ20.x >> 9, record->viewDirQ20.y >> 9, record->viewDirQ20.z >> 9,
							angle, modelType, screenScale);
					}
				}
				break;

			case 3:
				if (g_useHardware3D) {
					Backdrop_DrawCoordinateStrip(&g_backdropRecordsByRegion[0][flatRecordIdx]);
				} else if (g_camMatR2_X > 0) {
					int16_t angle;

					record = &g_backdropRecordsByRegion[0][flatRecordIdx];
					angle = (int16_t)(0x8000 - trig2_arctan(g_camMatR1_Y, g_camMatR0_Y));
					screenScale = record->angularScale;
					halfHeightAxisZ = g_camMatR2_Z;
					{
						int worldX;
						int worldY;
						int worldZ;

						worldZ = record->worldDirQ20.z;
						worldY = record->worldDirQ20.y;
						halfHeightAxisY = g_camMatR1_Z;
						halfWidthAxisY = g_camMatR1_Y;
						halfHeightAxisX = g_camMatR0_Z;
						halfWidthAxisZ = g_camMatR2_Y;
						worldX = record->worldDirQ20.x;
						halfWidthAxisX = g_camMatR0_Y;
						modelType = record->modelType;

						record->viewDirQ20.x = TRANSFM2_CamMatDotRow0(worldX, worldY, worldZ);
						record->viewDirQ20.y = TRANSFM2_CamMatDotRow1(worldX, worldY, worldZ);
						record->viewDirQ20.z = TRANSFM2_CamMatDotRow2(worldX, worldY, worldZ);
					}
					if (g_useHardware3D) {
						Backdrop_DrawHardwareAxisQuad(
							modelType, record->viewDirQ20.x >> 9, record->viewDirQ20.y >> 9,
							record->viewDirQ20.z >> 9, halfWidthAxisX, halfWidthAxisY, halfWidthAxisZ,
							halfHeightAxisX, halfHeightAxisY, halfHeightAxisZ, screenScale);
					} else {
						int projectedX;
						int projectedY;
						int projectionInput;
						uint32_t projectionHigh;

						viewX = record->viewDirQ20.x >> 9;
						viewY = record->viewDirQ20.y >> 9;
						viewZ = record->viewDirQ20.z >> 9;

						if (viewX < 0) {
							projectionInput = -viewX;
							if (projectionInput > viewZ) {
								break;
							} else {
								projectionHigh = (uint32_t)projectionInput >> (32 - perspShift);
								projectionInput = (int)((uint32_t)projectionInput << perspShift);
								projectionInput += g_projScaleHalfInt;
								if ((uint32_t)projectionInput < (uint32_t)g_projScaleHalfInt) {
									++projectionHigh;
								}
								if (projectionHigh < (uint32_t)viewZ) {
									projectionInput = (int)((uint32_t)projectionInput / (uint32_t)viewZ);
								} else {
									projectionInput = 0x7fffff00;
								}
								projectedX = -projectionInput;
							}
						} else if (viewX > viewZ) {
							break;
						} else {
							projectionHigh = (uint32_t)viewX >> (32 - perspShift);
							projectionInput = (int)((uint32_t)viewX << perspShift);
							projectionInput += g_projScaleHalfInt;
							if ((uint32_t)projectionInput < (uint32_t)g_projScaleHalfInt) {
								++projectionHigh;
							}
							if (projectionHigh < (uint32_t)viewZ) {
								projectedX = (int)((uint32_t)projectionInput / (uint32_t)viewZ);
							} else {
								projectedX = 0x7fffff00;
							}
						}

						if (viewY < 0) {
							projectionInput = -viewY;
							if (projectionInput > viewZ) {
								break;
							} else {
								projectionHigh = (uint32_t)projectionInput >> (32 - perspShift);
								projectionInput = (int)((uint32_t)projectionInput << perspShift);
								projectionInput += g_projScaleHalfInt;
								if ((uint32_t)projectionInput < (uint32_t)g_projScaleHalfInt) {
									++projectionHigh;
								}
								if (projectionHigh < (uint32_t)viewZ) {
									projectionInput = (int)((uint32_t)projectionInput / (uint32_t)viewZ);
								} else {
									projectionInput = 0x7fffff00;
								}
								projectedY = -projectionInput;
							}
						} else if (viewY > viewZ) {
							break;
						} else {
							projectionHigh = (uint32_t)viewY >> (32 - perspShift);
							projectionInput = (int)((uint32_t)viewY << perspShift);
							projectionInput += g_projScaleHalfInt;
							if ((uint32_t)projectionInput < (uint32_t)g_projScaleHalfInt) {
								++projectionHigh;
							}
							if (projectionHigh < (uint32_t)viewZ) {
								projectedY = (int)((uint32_t)projectionInput / (uint32_t)viewZ);
							} else {
								projectedY = 0x7fffff00;
							}
						}

						Backdrop_DrawModelTexQuadAtScreen((uint16_t)modelType, g_flightVpCenterX + projectedX,
														  g_flightVpHeight - projectedY - g_projOffsetY -
															  g_flightVpCenterY,
														  angle, screenScale);
					}
				}
				break;

			case 4:
				if (g_camMatR2_Z > 0) {
					int16_t angle;

					record = &g_backdropRecordsByRegion[0][flatRecordIdx];
					angle = (int16_t)-trig2_arctan(g_camMatR1_X, g_camMatR0_X);
					modelType = record->modelType;
					screenScale = record->angularScale;
					halfWidthAxisX = g_camMatR0_Y;
					halfWidthAxisY = g_camMatR1_Y;
					halfWidthAxisZ = g_camMatR2_Y;
					halfHeightAxisX = g_camMatR0_X;
					halfHeightAxisY = g_camMatR1_X;
					halfHeightAxisZ = g_camMatR2_X;
					{
						int worldX = record->worldDirQ20.x;
						int worldY = record->worldDirQ20.y;
						int worldZ = record->worldDirQ20.z;

						record->viewDirQ20.x = TRANSFM2_CamMatDotRow0(worldX, worldY, worldZ);
						record->viewDirQ20.y = TRANSFM2_CamMatDotRow1(worldX, worldY, worldZ);
						record->viewDirQ20.z = TRANSFM2_CamMatDotRow2(worldX, worldY, worldZ);
					}
					if (g_useHardware3D) {
						int corners[12];

						viewX = record->viewDirQ20.x >> 9;
						viewY = record->viewDirQ20.y >> 9;
						viewZ = record->viewDirQ20.z >> 9;

						curTexLevel = g_modelTypeTable[(uint16_t)modelType].curTexLevel;
						if ((g_modelTypeTable[(uint16_t)modelType].assetFlags &
							 (MODEL_TYPE_ASSET_TEXTURE_DRAW | MODEL_TYPE_ASSET_TEXTURE_READY)) != 0 &&
							curTexLevel != NULL) {
							halfHeightPixels =
								Backdrop_ComputeTextureAxisExtent(curTexLevel->height, screenScale);
							halfWidthPixels =
								Backdrop_ComputeTextureAxisExtent(curTexLevel->width, screenScale);

							halfWidthAxisX = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisX);
							halfWidthAxisY = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisY);
							halfWidthAxisZ = Backdrop_ScaleAxisComponent(halfWidthPixels, halfWidthAxisZ);
							halfHeightAxisX = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisX);
							halfHeightAxisY = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisY);
							halfHeightAxisZ = Backdrop_ScaleAxisComponent(halfHeightPixels, halfHeightAxisZ);

							corner = corners;
							cornerCount = 4;
							do {
								corner[0] = viewX;
								corner[1] = viewY;
								corner[2] = viewZ;
								corner += 3;
							} while (--cornerCount != 0);

							corners[0] += halfHeightAxisX + halfWidthAxisX;
							corners[1] += halfWidthAxisY + halfHeightAxisY;
							corners[2] += halfWidthAxisZ + halfHeightAxisZ;
							corners[3] += halfHeightAxisX - halfWidthAxisX;
							corners[4] += halfHeightAxisY - halfWidthAxisY;
							corners[5] += halfHeightAxisZ - halfWidthAxisZ;
							corners[6] -= halfHeightAxisX + halfWidthAxisX;
							corners[7] -= halfWidthAxisY + halfHeightAxisY;
							corners[8] -= halfWidthAxisZ + halfHeightAxisZ;
							corners[9] += halfWidthAxisX - halfHeightAxisX;
							corners[10] += halfWidthAxisY - halfHeightAxisY;
							corners[11] += halfWidthAxisZ - halfHeightAxisZ;

							quadDrawFlags = 0x612;
							if (g_bilinearEnabled) {
								quadDrawFlags = 0x792;
							}

							curTexLevel->argbColor = -1;
							RenderQuad_DrawTextured3D(corners, curTexLevel, quadDrawFlags);
						}
					} else {
						Backdrop_ProjectAndDrawScreenQuad(
							record->viewDirQ20.x >> 9, record->viewDirQ20.y >> 9, record->viewDirQ20.z >> 9,
							angle, modelType, screenScale);
					}
				}
				break;

			case 5:
				if (g_camMatR2_Z < 0) {
					int16_t angle;

					record = &g_backdropRecordsByRegion[0][flatRecordIdx];
					angle = (int16_t)-trig2_arctan(g_camMatR1_X, g_camMatR0_X);
					modelType = record->modelType;
					screenScale = record->angularScale;
					halfWidthAxisX = g_camMatR0_Y;
					halfWidthAxisY = g_camMatR1_Y;
					halfWidthAxisZ = g_camMatR2_Y;
					halfHeightAxisX = g_camMatR0_X;
					halfHeightAxisY = g_camMatR1_X;
					halfHeightAxisZ = g_camMatR2_X;
					{
						int worldX = record->worldDirQ20.x;
						int worldY = record->worldDirQ20.y;
						int worldZ = record->worldDirQ20.z;

						record->viewDirQ20.x = TRANSFM2_CamMatDotRow0(worldX, worldY, worldZ);
						record->viewDirQ20.y = TRANSFM2_CamMatDotRow1(worldX, worldY, worldZ);
						record->viewDirQ20.z = TRANSFM2_CamMatDotRow2(worldX, worldY, worldZ);
					}
					if (g_useHardware3D) {
						Backdrop_DrawHardwareAxisQuad(
							modelType, record->viewDirQ20.x >> 9, record->viewDirQ20.y >> 9,
							record->viewDirQ20.z >> 9, halfWidthAxisX, halfWidthAxisY, halfWidthAxisZ,
							halfHeightAxisX, halfHeightAxisY, halfHeightAxisZ, screenScale);
					} else {
						int projectedX;
						int projectedY;
						int projectionInput;
						uint32_t projectionHigh;

						viewX = record->viewDirQ20.x >> 9;
						viewY = record->viewDirQ20.y >> 9;
						viewZ = record->viewDirQ20.z >> 9;

						if (viewX < 0) {
							projectionInput = -viewX;
							if (projectionInput > viewZ) {
								break;
							} else {
								projectionHigh = (uint32_t)projectionInput >> (32 - perspShift);
								projectionInput = (int)((uint32_t)projectionInput << perspShift);
								projectionInput += g_projScaleHalfInt;
								if ((uint32_t)projectionInput < (uint32_t)g_projScaleHalfInt) {
									++projectionHigh;
								}
								if (projectionHigh < (uint32_t)viewZ) {
									projectionInput = (int)((uint32_t)projectionInput / (uint32_t)viewZ);
								} else {
									projectionInput = 0x7fffff00;
								}
								projectedX = -projectionInput;
							}
						} else if (viewX > viewZ) {
							break;
						} else {
							projectionHigh = (uint32_t)viewX >> (32 - perspShift);
							projectionInput = (int)((uint32_t)viewX << perspShift);
							projectionInput += g_projScaleHalfInt;
							if ((uint32_t)projectionInput < (uint32_t)g_projScaleHalfInt) {
								++projectionHigh;
							}
							if (projectionHigh < (uint32_t)viewZ) {
								projectedX = (int)((uint32_t)projectionInput / (uint32_t)viewZ);
							} else {
								projectedX = 0x7fffff00;
							}
						}

						if (viewY < 0) {
							projectionInput = -viewY;
							if (projectionInput > viewZ) {
								break;
							} else {
								projectionHigh = (uint32_t)projectionInput >> (32 - perspShift);
								projectionInput = (int)((uint32_t)projectionInput << perspShift);
								projectionInput += g_projScaleHalfInt;
								if ((uint32_t)projectionInput < (uint32_t)g_projScaleHalfInt) {
									++projectionHigh;
								}
								if (projectionHigh < (uint32_t)viewZ) {
									projectionInput = (int)((uint32_t)projectionInput / (uint32_t)viewZ);
								} else {
									projectionInput = 0x7fffff00;
								}
								projectedY = -projectionInput;
							}
						} else if (viewY > viewZ) {
							break;
						} else {
							projectionHigh = (uint32_t)viewY >> (32 - perspShift);
							projectionInput = (int)((uint32_t)viewY << perspShift);
							projectionInput += g_projScaleHalfInt;
							if ((uint32_t)projectionInput < (uint32_t)g_projScaleHalfInt) {
								++projectionHigh;
							}
							if (projectionHigh < (uint32_t)viewZ) {
								projectedY = (int)((uint32_t)projectionInput / (uint32_t)viewZ);
							} else {
								projectedY = 0x7fffff00;
							}
						}

						Backdrop_DrawModelTexQuadAtScreen((uint16_t)modelType, g_flightVpCenterX + projectedX,
														  g_flightVpHeight - projectedY - g_projOffsetY -
															  g_flightVpCenterY,
														  angle, screenScale);
					}
				}
				break;

			default:
				break;
		}
	}
}

// FUNCTION: XWA 0x407C10
void Backdrop_BuildCoordinateBuffers(void) {
	int regionIdx;

	if (g_missionRegionCount <= 0) {
		return;
	}

	for (regionIdx = 0; regionIdx < g_missionRegionCount; ++regionIdx) {
		unsigned int recordIdx;

		for (recordIdx = 0; recordIdx < (unsigned int)g_backdropCountByRegion[regionIdx]; ++recordIdx) {
			WorldRectRecord* record;
			uint8_t* recordFrame;

			record = &g_backdropRecordsByRegion[regionIdx][recordIdx];
			recordFrame = &record->frame;
			if ((record->flags & 1u) == 0) {
				continue;
			}
			if ((uint16_t)record->modelType == OBJ_DeathStarFireTextureGroup6250_Sprite000 ||
				(uint16_t)record->modelType == OBJ_DeathStarFireTextureGroup6251) {
				if (*recordFrame < 1) {
					*recordFrame = 1;
				}
			} else {
				uint8_t usedFrameBitmap[100];
				ObjectTypeId modelType;
				TexLevel* texLevels;
				int scanRegionIdx;
				int frameIdx;

				memset(usedFrameBitmap, 0, sizeof(usedFrameBitmap));
				modelType = record->modelType;
				for (scanRegionIdx = 0; scanRegionIdx < g_missionRegionCount; ++scanRegionIdx) {
					unsigned int scanRecordIdx;

					for (scanRecordIdx = 0;
						 scanRecordIdx < (unsigned int)g_backdropCountByRegion[scanRegionIdx];
						 ++scanRecordIdx) {
						WorldRectRecord* scanRecord;
						uint8_t* scanFrame;

						scanRecord = &g_backdropRecordsByRegion[scanRegionIdx][scanRecordIdx];
						scanFrame = &scanRecord->frame;
						if ((uint16_t)scanRecord->modelType != (uint16_t)modelType) {
							continue;
						}
						if (*scanFrame < 1) {
							*scanFrame = 1;
						}
						if (*scanFrame <= 100) {
							usedFrameBitmap[*scanFrame - 1] = 1;
						}
					}
				}

				texLevels = g_modelTypeTable[(uint16_t)modelType].texLevels;
				if (texLevels != NULL) {
					for (frameIdx = 0; frameIdx < g_modelTypeTable[(uint16_t)modelType].frameCount;
						 ++frameIdx) {
						if (usedFrameBitmap[frameIdx] == 0 && texLevels[frameIdx].image != NULL) {
							if (g_useHardware3D) {
								std3D_DeleteTextureSurface(texLevels[frameIdx].image);
							} else {
								Memory_FreeTagged("RESOURCEITEM", texLevels[frameIdx].image);
							}
							texLevels[frameIdx].image = NULL;
						}
					}
				}
			}
		}
	}

	for (regionIdx = 0; regionIdx < g_missionRegionCount; ++regionIdx) {
		unsigned int recordIdx;

		for (recordIdx = 0; recordIdx < (unsigned int)g_backdropCountByRegion[regionIdx]; ++recordIdx) {
			WorldRectRecord* record;
			TexLevel* curTexLevel;
			int dirX;
			int dirY;
			int dirZ;
			int textureWidth;
			int textureHeight;
			int angularScale;
			float dirRadius;
			int stripArcWidth;
			int stripArcHeight;
			int stripSegmentCount;
			int imageFrameCount;
			int stripHalfHeight;
			int stripSegmentsPerFrame;
			Vec3i* stripCoords;
			int coordCount;
			double initialSin;
			double initialCos;
			double baseX;
			double baseY;
			int currentX;
			int currentY;
			Vec3i* coord;
			int coordIndex;
			int step;

			record = &g_backdropRecordsByRegion[regionIdx][recordIdx];
			if (record->side == 4 || record->side == 5) {
				record->stripSegmentCount = 0;
				record->stripCoords = NULL;
				continue;
			}

			if (g_modelTypeTable[(uint16_t)record->modelType].texLevels != NULL) {
				if ((record->flags & 1u) != 0) {
					FeDiskIo_SelectTextureFrame((uint16_t)record->modelType, record->frame, 256);
				} else {
					FeDiskIo_SelectTextureFrame((uint16_t)record->modelType, 1u, 256);
				}
			}

			curTexLevel = g_modelTypeTable[(uint16_t)record->modelType].curTexLevel;
			if (curTexLevel == NULL) {
				continue;
			}

			textureWidth = curTexLevel->width << 9;
			textureHeight = curTexLevel->height << 9;
			angularScale = record->angularScale;
			stripArcWidth = (angularScale * (textureWidth + (textureWidth >> 1))) >> 8;
			stripArcHeight = (angularScale * (textureHeight + (textureHeight >> 1))) >> 8;
			dirX = record->worldDirQ20.x;
			dirY = record->worldDirQ20.y;
			dirZ = record->worldDirQ20.z;

			trig2_ctop(dirX, dirY, dirZ);
			dirRadius = (float)(int)trig2_polardistance;
			if ((record->flags & 2u) != 0) {
				double arcBase;
				double textureAspectRatio;

				textureAspectRatio =
					(double)g_modelTypeTable[(uint16_t)record->modelType].curTexLevel->height /
					(double)g_modelTypeTable[(uint16_t)record->modelType].curTexLevel->width;
				arcBase = (double)record->angularScale * 0.00390625 * (double)dirRadius;
				stripArcWidth = (int)(arcBase * 3.141592653589793);
				stripArcHeight = (int)(textureAspectRatio * arcBase * 3.141592653589793);
			}

			stripSegmentCount = stripArcWidth >> 17;
			if (stripSegmentCount < 1) {
				stripSegmentCount = 1;
			} else if (stripSegmentCount > 16) {
				stripSegmentCount = 16;
			}

			if (g_modelTypeTable[(uint16_t)record->modelType].texLevels != NULL) {
				imageFrameCount = 0;
				do {
					FeDiskIo_SelectTextureFrame((uint16_t)record->modelType, (uint16_t)(imageFrameCount + 1),
												256);
					++imageFrameCount;
				} while (g_modelTypeTable[(uint16_t)record->modelType].curTexLevel != NULL);
				--imageFrameCount;
			} else {
				record->flags &= (uint8_t)~1u;
				record->frame = 0;
				if (g_modelTypeTable[(uint16_t)record->modelType].curTexLevel == NULL) {
					DebugPrintfChannel(1024, "No image for bg [%d][%d].\n", regionIdx, recordIdx);
					record->stripSegmentCount = 0;
					record->stripCoords = NULL;
					continue;
				}
				imageFrameCount = 1;
			}

			if (imageFrameCount == 0) {
				DebugPrintfChannel(1024, "0 imagecount for bg [%d][%d].\n", regionIdx, recordIdx);
				record->stripSegmentCount = 0;
				record->stripCoords = NULL;
				continue;
			}

			if ((record->flags & 1u) != 0) {
				if (record->frame > imageFrameCount) {
					record->frame = 1;
				}
				imageFrameCount = 1;
			}

			stripHalfHeight = stripArcHeight / imageFrameCount;
			if (stripSegmentCount % imageFrameCount != 0) {
				if (24 % imageFrameCount == 0) {
					stripSegmentCount = 24;
				} else {
					stripSegmentCount = 4 * imageFrameCount;
				}
			}
			stripSegmentsPerFrame = stripSegmentCount / imageFrameCount;
			stripCoords = (Vec3i*)Memory_AllocTagged("WORLDRECTCOORDS",
													 sizeof(*stripCoords) * (size_t)(stripSegmentCount + 1));

			initialSin = sin((double)stripArcWidth / (double)dirRadius);
			initialCos = cos((double)stripArcWidth / (double)dirRadius);
			currentX = (int)((double)dirX * initialCos - (double)-dirY * initialSin);
			currentY = (int)((double)dirY * initialCos - initialSin * (double)dirX);
			baseX = (double)currentX;
			baseY = (double)currentY;

			coordCount = stripSegmentCount + 1;
			step = 1;
			coord = &stripCoords[coordCount];
			for (coordIndex = stripSegmentCount; coordIndex >= 0; --coordIndex) {
				int angleNumerator;
				double angleSin;
				double angleCos;

				--coord;
				if (stripArcWidth > 0x400000) {
					angleNumerator = (step * (stripArcWidth >> 7) / stripSegmentCount) << 8;
				} else {
					angleNumerator = (stripArcWidth * 2) * step / stripSegmentCount;
				}
				coord->x = currentX;
				coord->y = currentY;
				coord->z = dirZ;
				angleSin = sin((double)angleNumerator / (double)dirRadius);
				angleCos = cos((double)angleNumerator / (double)dirRadius);
				currentX = (int)(angleCos * baseX - angleSin * baseY);
				currentY = (int)(angleCos * baseY + angleSin * baseX);
				++step;
			}

			record->stripSegmentCount = stripSegmentCount;
			record->stripSegmentsPerFrame = stripSegmentsPerFrame;
			record->stripCoords = stripCoords;
			record->stripHalfHeight = stripHalfHeight;

			if ((uint16_t)record->modelType == OBJ_DeathStarFireTextureGroup6250_Sprite000) {
				DeathStarTunnelLaserRegionState* laser;
				WorldRectRecord* emitter;
				WorldRectRecord* beamSprite;
				int laserDirX;
				int laserDirY;
				int laserDirZ;
				double laserDirRadius;
				uint16_t laserAngularScale;
				double laserAngle;
				double laserAngleCos;
				double laserAngleSin;
				int laserNewX;
				int laserNewY;
				int laserNewZ;

				laser = &g_deathStarTunnelLaserRegions[regionIdx];
				emitter = laser->emitterRect;
				laserDirX = emitter->worldDirQ20.x;
				laserDirY = emitter->worldDirQ20.y;
				laserDirZ = emitter->worldDirQ20.z;
				trig2_ctop(laserDirX, laserDirY, laserDirZ);
				laserDirRadius = (double)(int)trig2_polardistance;

				FeDiskIo_SelectTextureFrame(OBJ_DeathStarFireTextureGroup6250_Sprite000, 1u, 256);
				laserAngularScale = emitter->angularScale;
				laserAngle =
					(double)(laserAngularScale * g_modelTypeTable[OBJ_DeathStarFireTextureGroup6250_Sprite000]
													 .curTexLevel->width) *
					-0.800000011920929 / laserDirRadius;
				laserAngleCos = cos(laserAngle);
				laserAngleSin = sin(laserAngle);

				laserNewX = (int)((double)laserDirX * laserAngleCos - (double)-laserDirY * laserAngleSin);
				laserNewY = (int)((double)laserDirY * laserAngleCos - laserAngleSin * (double)laserDirX);
				laserNewZ =
					laserDirZ - (int)((float)(laserAngularScale *
											  g_modelTypeTable[OBJ_DeathStarFireTextureGroup6250_Sprite000]
												  .curTexLevel->height) *
									  -1.15f);

				beamSprite = laser->beamSpriteRect;
				beamSprite->worldDirQ20.x = laserNewX;
				beamSprite->worldDirQ20.y = laserNewY;
				beamSprite->worldDirQ20.z = laserNewZ;
				beamSprite->angularScale =
					(uint16_t)(beamSprite->angularScale - (int)((float)beamSprite->angularScale * -0.5f));
				laser->emitterOffsetX = laserNewX;
				laser->emitterOffsetY = laserNewY;
				laser->emitterOffsetZ = laserNewZ;
			}
		}
	}
}

// FUNCTION: XWA 0x408330
void Backdrop_FreeCoordinateBuffers(void) {
	int regionIdx;
	unsigned int recordIdx;

	for (regionIdx = 0; regionIdx < g_missionRegionCount; ++regionIdx) {
		for (recordIdx = 0; recordIdx < (unsigned int)g_backdropCountByRegion[regionIdx]; ++recordIdx) {
			if (g_backdropRecordsByRegion[regionIdx][recordIdx].stripSegmentCount > 0) {
				Memory_FreeTagged("WORLDRECTCOORDS", g_backdropRecordsByRegion[regionIdx][recordIdx].stripCoords);
				g_backdropRecordsByRegion[regionIdx][recordIdx].stripSegmentCount = 0;
				g_backdropRecordsByRegion[regionIdx][recordIdx].stripCoords = NULL;
			}
		}
	}
}

// FUNCTION: XWA 0x42BBA0
void RenderBillboard_DrawRollAlignedObjectModel(uint16_t objectIndex) {
	ObjectRecord* obj;
	int deltaX;
	int deltaY;
	int deltaZ;
	int sideDot;
	int upDot;
	Q16Angle savedRoll;

	obj = &g_objectTable[objectIndex];

	deltaX = g_players[g_localPlayer].viewState.savedTargetX - obj->world_x;
	deltaY = g_players[g_localPlayer].viewState.savedTargetY - obj->world_y;
	deltaZ = g_players[g_localPlayer].viewState.savedTargetZ - obj->world_z;

	if (obj->mobj->orientMatrixDirty) {
		FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
		FVIEW_calcrotateorient(obj->roll, obj->angleD, obj);
	}

	sideDot = Xwa_Dot3Q15Inline(deltaX, deltaY, deltaZ, obj->mobj->cachedSideX, obj->mobj->cachedSideY,
								obj->mobj->cachedSideZ);
	upDot = Xwa_Dot3Q15Inline(deltaX, deltaY, deltaZ, obj->mobj->cachedUpX, obj->mobj->cachedUpY,
							  obj->mobj->cachedUpZ);

	savedRoll = obj->roll;
	obj->roll = (Q16Angle)(obj->roll + trig2_arctan(upDot, sideDot) - 0x4000u);
	obj->mobj->orientMatrixDirty = 1;

	FVIEW_SetObjectTransform(obj->roll, obj->pitch, obj->yaw, obj->angleD, obj);
	RenderScene_DrawObjectModel(obj);

	obj->roll = savedRoll;
	obj->mobj->orientMatrixDirty = 1;
}

// FUNCTION: XWA 0x401000
void SceneBillboard_QueueObjectTextured(uint16_t objectIndex) {
	ObjectRecord* obj;
	ObjectTypeId objectType;
	int modelTypeIndex;
	uint16_t frame;
	int axisX;
	int axisY;
	int absR0Z;
	int absR1Z;
	int16_t rotationAngle;
	int screenX;
	int screenXHigh;
	int screenYProjected;
	int screenYHigh;
	int16_t screenY;
	MobileObject* mobj;
	unsigned int screenSize;
	int depthZ;

	obj = &g_objectTable[objectIndex];
	objectType = obj->objectType;
	if (objectType == OBJ_NoAsset_222) {
		RenderScene_DrawNoAssetSourceModel(obj, obj->typeSpecificByte[0] >> 1);
		return;
	}

	modelTypeIndex = (uint16_t)obj->objectType;
	if (!g_modelTypeTable[modelTypeIndex].texLevels) {
		return;
	}

	frame = obj->typeSpecificByte[0];
	if (viewZ < 0 || frame == 0) {
		return;
	}

	absR0Z = g_objViewMat_R0_Z;
	absR1Z = g_objViewMat_R1_Z;
	if (absR0Z < 0) {
		absR0Z = -absR0Z;
	}
	if (absR1Z < 0) {
		absR1Z = -absR1Z;
	}

	if (absR0Z < absR1Z) {
		axisX = g_objViewMat_R0_X;
		axisY = g_objViewMat_R0_Y;
	} else {
		axisX = g_objViewMat_R1_X;
		axisY = g_objViewMat_R1_Y;
	}

	if (axisX < 0) {
		rotationAngle = (int16_t)trig2_arctan(axisY, -axisX);
	} else {
		rotationAngle = (int16_t)-trig2_arctan(axisY, axisX);
	}

	screenX = TRANSFM2_ProjectScreenX(viewX, viewZ);
	screenXHigh = screenX & (int)0xffff0000u;
	if (screenXHigh > 0 || screenXHigh < (int)0xffff0000u) {
		return;
	}

	screenYProjected = TRANSFM2_ProjectScreenY(viewY, viewZ);
	screenYHigh = screenYProjected & (int)0xffff0000u;
	if (screenYHigh > 0 || screenYHigh < (int)0xffff0000u) {
		return;
	}

	screenY = (int16_t)(g_flightVpHeight - screenYProjected);
	if (obj->objectType == OBJ_ExplosionTextureGroup2006) {
		viewZ -= obj->typeSpecificWord;
		if (viewZ < 0) {
			viewZ = 1;
		}
	}

	mobj = obj->mobj;
	screenSize = ((unsigned int)mobj->instanceExtent << 8) /
				 (unsigned int)g_modelTypeTable[modelTypeIndex].maxBoundsExtent;
	if (obj->objectType < OBJ_DebrisTextureGroup4000 || obj->objectType > OBJ_DebrisTextureGroup4003) {
		screenSize <<= 3;
	}
	if (mobj->sourceObjectType == OBJ_SuperStarDestroyer &&
		obj->objectType == OBJ_ExplosionTextureGroup2006) {
		screenSize = 0xffffu;
	}

	obj->objectType = objectType;
	depthZ = viewZ;
	if ((int16_t)g_sceneBillboardQueueCount < 32) {
		int16_t entryIndex;
		SceneBillboardQueueEntry* entry;

		entryIndex = (int16_t)g_sceneBillboardQueueCount;
		entry = &g_sceneBillboardQueue[entryIndex];
		entry->objectType = g_objectTable[objectIndex].objectType;
		entry->objectOrTypeIndex = objectIndex;
		entry->frame = (int16_t)frame;
		entry->screenSize = (int16_t)screenSize;
		++g_sceneBillboardQueueCount;
		entry->screenX = (int16_t)screenX;
		entry->screenY = screenY;
		entry->depthZ = depthZ;
		entry->rotationAngle = rotationAngle;
	}
	g_objectTable[objectIndex].objectType = objectType;
}

// FUNCTION: XWA 0x401250
void SceneBillboard_QueueProjectedTextured(int objectOrTypeIndex, int frame, int screenSize, int screenX,
										   int screenY, int depthZ, int rotationAngle) {
	if (g_sceneBillboardQueueCount < 32) {
		int entryIndex;
		uint16_t nextCount;

		entryIndex = (int16_t)g_sceneBillboardQueueCount;
		nextCount = (uint16_t)(g_sceneBillboardQueueCount + 1);
		g_sceneBillboardQueue[entryIndex].objectType = g_objectTable[objectOrTypeIndex].objectType;
		g_sceneBillboardQueue[entryIndex].objectOrTypeIndex = objectOrTypeIndex;
		g_sceneBillboardQueue[entryIndex].frame = frame;
		g_sceneBillboardQueue[entryIndex].screenSize = screenSize;
		g_sceneBillboardQueue[entryIndex].screenX = screenX;
		g_sceneBillboardQueue[entryIndex].screenY = screenY;
		g_sceneBillboardQueue[entryIndex].depthZ = depthZ;
		g_sceneBillboardQueue[entryIndex].rotationAngle = rotationAngle;
		g_sceneBillboardQueueCount = (int16_t)nextCount;
	}
}

// FUNCTION: XWA 0x4012F0
void SceneBillboard_RenderQueuedTextured(int drawTargetComponentMarkers) {
	int16_t swapped;
	uint16_t currentTargetObjectIdx;

	swapped = 1;
	while (g_sceneBillboardQueueCount--) {
		SceneBillboardQueueEntry* entry;
		ObjectRecord* objectTable;
		ObjectRecord* obj;
		uint16_t objectType;
		int screenSize;
		int clampTo1024;
		FlightTexQuad quad;

		if (swapped) {
			uint16_t passIndex;
			int sortLimit;

			swapped = 0;
			passIndex = 0;
			sortLimit = g_sceneBillboardQueueCount;
			if (sortLimit > 0) {
				do {
					if (g_sceneBillboardQueue[passIndex].depthZ >
						g_sceneBillboardQueue[passIndex + 1].depthZ) {
						SceneBillboardQueueEntry temp;

						temp = g_sceneBillboardQueue[passIndex];
						g_sceneBillboardQueue[passIndex] = g_sceneBillboardQueue[passIndex + 1];
						g_sceneBillboardQueue[passIndex + 1] = temp;
						swapped = 1;
					}
					++passIndex;
				} while (passIndex < g_sceneBillboardQueueCount);
			}
		}

		entry = &g_sceneBillboardQueue[g_sceneBillboardQueueCount];
		objectType = entry->objectType;
		objectTable = g_objectTable;
		obj = &objectTable[entry->objectOrTypeIndex];

		g_camRelWorldX = obj->world_x - g_players[g_localPlayer].viewState.savedTargetX;
		g_camRelWorldY = obj->world_y - g_players[g_localPlayer].viewState.savedTargetY;
		g_camRelWorldZ = obj->world_z - g_players[g_localPlayer].viewState.savedTargetZ;
		viewZ = entry->depthZ;

		if (objectType == OBJ_DeathStarIITextureGroup17002) {
			screenSize = DeathStar_ComputeScaledProjectedBillboardSize(
				entry->depthZ, g_modelTypeTable[OBJ_DeathStarIITextureGroup17002].maxBoundsExtent,
				(uint16_t)entry->screenSize);
		} else {
			clampTo1024 = 1;
			if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR &&
				objectType == OBJ_ExplosionTextureGroup2006) {
				clampTo1024 = 0;
			}
			screenSize = SceneBillboard_ComputeProjectedSize(
				entry->depthZ, g_modelTypeTable[(uint16_t)objectType].maxBoundsExtent,
				(uint16_t)entry->screenSize, clampTo1024);
		}

		quad.screenX = entry->screenX;
		quad.screenY = entry->screenY;
		quad.depthZ = entry->depthZ;
		quad.rotationAngle = entry->rotationAngle;
		quad.screenSize =
			(uint16_t)FeDiskIo_SelectTextureFrame(objectType, (uint16_t)entry->frame, screenSize);
		RenderQuad_DrawModelTexture(objectType, &quad, -1);
	}

	if ((uint16_t)drawTargetComponentMarkers) {
#ifdef XWA_MODERN
		XwaHudTargetBoxLayer previousTargetBoxLayer;

		previousTargetBoxLayer = XwaSnapshotHud_SetTargetBoxLayer(XWA_HUD_TARGET_BOX_BEFORE_FIXED);
#endif

		currentTargetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
		if (currentTargetObjectIdx != 0xFFFFu) {
			if (g_objectTable[currentTargetObjectIdx].objectType == OBJ_AccelRing2 ||
				g_objectTable[currentTargetObjectIdx].objectType == OBJ_AccelRing3) {
				Targeting_DrawObjectBox(currentTargetObjectIdx, 0xFFFFu, 0x3Bu);
				if (g_objectTable[currentTargetObjectIdx].mobj->pCraft->componentHp[4]) {
					Targeting_DrawObjectBox(currentTargetObjectIdx,
											(uint16_t)g_players[g_localPlayer].selectedTargetComponent,
											0x36u);
				}
			} else if (g_objectTable[currentTargetObjectIdx].genusId == GENUS_Starship ||
					   g_objectTable[currentTargetObjectIdx].genusId == GENUS_Platform ||
					   g_objectTable[currentTargetObjectIdx].genusId == GENUS_Freighter) {
				CraftData* craft;
				int modelType;
				MeshType selectedMeshType;

				Targeting_DrawObjectBox(currentTargetObjectIdx,
										(uint16_t)g_players[g_localPlayer].selectedTargetComponent, 0x3Fu);
				modelType = (uint16_t)g_objectTable[currentTargetObjectIdx].objectType;
				selectedMeshType = ModelMesh_GetObjectTypeMeshType(
					modelType, (uint16_t)g_players[g_localPlayer].selectedTargetComponent);
				if (selectedMeshType == MESH_WeaponSystem1 || selectedMeshType == MESH_WeaponSystem2) {
					int meshCount;
					uint16_t meshIndex;

					meshCount = ModelMesh_GetObjectTypeMeshCount(modelType);
					meshIndex = 0;
					craft = g_objectTable[currentTargetObjectIdx].mobj->pCraft;
					while ((int)(uint16_t)meshIndex < meshCount) {
						if (meshIndex != (uint16_t)g_players[g_localPlayer].selectedTargetComponent &&
							craft->componentHp[meshIndex] &&
							ModelMesh_GetObjectTypeMeshType(modelType, meshIndex) == selectedMeshType) {
							Targeting_DrawObjectBox(currentTargetObjectIdx, meshIndex, 0x36u);
						}
						++meshIndex;
					}
				}
			}
			Targeting_DrawObjectBox(currentTargetObjectIdx, 0xFFFFu, 0x3Bu);
		}
#ifdef XWA_MODERN
		XwaSnapshotHud_SetTargetBoxLayer(previousTargetBoxLayer);
#endif
	}
}
