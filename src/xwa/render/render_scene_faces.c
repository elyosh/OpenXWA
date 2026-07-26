#include "xwa/flight/fediskio.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer_internal.h"

#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"

#ifndef XWA_MODERN
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* outputString);
#else
static void OutputDebugStringA(const char* outputString) { DebugPrintf("%s", outputString); }
#endif

// GLOBAL: XWA 0x5A94C0
const float g_renderMinD3DDepth = 0.000015259022f;
// GLOBAL: XWA 0x5A94C8
const float g_renderDistantDepth = 100000.0f;
// GLOBAL: XWA 0x5A94CC
const float g_renderNegDistantDepth = -100000.0f;
// GLOBAL: XWA 0x5A94D0
const float g_renderZeroFloat = 0.0f;
// GLOBAL: XWA 0x5A94D4
const float g_renderRoughDistanceScale = 0.29409999f;
// GLOBAL: XWA 0x5A94D8
const float g_renderUnitFloat = 1.0f;
// GLOBAL: XWA 0x5A94DC
const float g_renderPointLightFacingThreshold = -0.30000001f;
// GLOBAL: XWA 0x5A94E0
const float g_renderHalfFloat = 0.5f;
// GLOBAL: XWA 0x5A94E4
const float g_renderOverbrightBleedScale = 0.1f;
// GLOBAL: XWA 0x5A94F0
const float g_renderNegHalfFloat = -0.5f;
// GLOBAL: XWA 0x5A9500
const double g_vertexColorAlphaScale = 255.0;
// GLOBAL: XWA 0x5A9508
const float g_vertexColorRgbScale = 320.0f;
// GLOBAL: XWA 0x5A950C
const float g_argbAlphaToUnitScale = 2.3374372e-10f;
// GLOBAL: XWA 0x5A9510
const float g_argbRedToUnitScale = 0.000000059838392f;
// GLOBAL: XWA 0x5A9514
const float g_argbGreenToUnitScale = 0.000015318628f;
// GLOBAL: XWA 0x5A9518
const float g_argbBlueToUnitScale = 0.0039215689f;
// GLOBAL: XWA 0x5A951C
const float g_renderBillboardNearCullZ = 10.0f;
// GLOBAL: XWA 0x5A9544
const float g_renderTrailScreenScaleBase = 256.0f;
// GLOBAL: XWA 0x5A954C
const float g_renderProjectionDepthBias = -1.5f;
// FUNCTION: XWA 0x4487E0
int RenderScene_EmitFlightVertex(int vertIdx, ProjVertex* verts) {
	ProjVertex* vert;
	volatile float tu;
	float sx;
	float sy;
	float w;
	volatile float tv;
	float depth;
	int alpha;
	int red;
	int green;
	int blue;
	int emittedIdx;
	float scaledColor;

	vert = &verts[vertIdx];
	tu = vert->tu;
	sx = vert->sx;
	sy = vert->sy;
	w = vert->w;
	tv = vert->tv;
	if (w < g_renderZeroFloat) {
		w = g_projScale;
	}

	depth = w * g_depthProjScale;
	depth = depth / (depth + g_projScale);
	if (depth < g_renderMinD3DDepth) {
		depth = g_renderMinD3DDepth;
	}
	if (g_std3DZCmpMode == 2) {
		depth = g_renderUnitFloat - depth;
	}

	g_flightVertexBuffer[g_d3dVertexCount].sx = sx + g_flightVpOriginX;
	g_flightVertexBuffer[g_d3dVertexCount].sy = sy + g_flightVpOriginY;
	g_flightVertexBuffer[g_d3dVertexCount].sz = depth;
	g_flightVertexBuffer[g_d3dVertexCount].rhw = w;
	g_flightVertexBuffer[g_d3dVertexCount].tu = tu;
	g_flightVertexBuffer[g_d3dVertexCount].tv = tv;

	alpha = (int)(vert->litColor[0] * g_vertexColorAlphaScale);
	if (g_capVertexAlpha && alpha == 255) {
		alpha = 254;
	}

	scaledColor = vert->litColor[1] * g_vertexColorRgbScale;
	red = (int)scaledColor;
	red += 48;
	if (red > 255) {
		red = 255;
	}
	scaledColor = vert->litColor[2] * g_vertexColorRgbScale;
	green = (int)scaledColor;
	green += 48;
	if (green > 255) {
		green = 255;
	}
	scaledColor = vert->litColor[3] * g_vertexColorRgbScale;
	blue = (int)scaledColor;
	blue += 48;
	if (blue > 255) {
		blue = 255;
	}
	g_flightVertexBuffer[g_d3dVertexCount].color =
		((uint32_t)alpha << 24) | ((uint32_t)red << 16) | ((uint32_t)green << 8) | (uint32_t)blue;
	g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
	emittedIdx = g_d3dVertexCount++;
	return emittedIdx;
}

// FUNCTION: XWA 0x4507F0
int RenderScene_EmitClippedFaceD3DVertex(int projVertexIdx, ProjVertex* projVerts) {
	ProjVertex* vert;
	volatile float tu;
	float sx;
	float sy;
	float w;
	volatile float tv;
	float depth;
	int emittedIdx;

	vert = &projVerts[projVertexIdx];
	sx = vert->sx;
	sy = vert->sy;
	w = vert->w;
	tu = vert->tu;
	tv = vert->tv;
	if (w < g_renderZeroFloat) {
		w = g_projScale;
	}

	depth = w * g_depthProjScale;
	depth = depth / (depth + g_projScale);
	if (depth < g_renderMinD3DDepth) {
		depth = g_renderMinD3DDepth;
	}
	if (g_std3DZCmpMode == 2) {
		depth = g_renderUnitFloat - depth;
	}

	g_flightVertexBuffer[g_d3dVertexCount].sx = sx + g_flightVpOriginX;
	g_flightVertexBuffer[g_d3dVertexCount].sy = sy + g_flightVpOriginY;
	g_flightVertexBuffer[g_d3dVertexCount].sz = depth;
	g_flightVertexBuffer[g_d3dVertexCount].rhw = w;
	g_flightVertexBuffer[g_d3dVertexCount].tu = tu;
	g_flightVertexBuffer[g_d3dVertexCount].tv = tv;
	g_flightVertexBuffer[g_d3dVertexCount].color = 0xC0000008u;
	g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
	emittedIdx = g_d3dVertexCount++;
	return emittedIdx;
}

// FUNCTION: XWA 0x4489C0
int RenderScene_AppendProjectedVertexToBatch(RenderBatch* batch, int projVertIdx,
											 const ProjVertex* projVerts) {
	const ProjVertex* vert;
	float sx;
	float sy;
	float w;
	float tu;
	float tv;
	float depth;
	int alpha;
	int red;
	int green;
	int blue;
	int appendedIdx;

	vert = &projVerts[projVertIdx];
	tu = vert->tu;
	sx = vert->sx;
	sy = vert->sy;
	w = vert->w;
	tv = vert->tv;
	if (w < 0.0f) {
		w = g_projScale;
	}

	depth = w * g_depthProjScale / (w * g_depthProjScale + g_projScale);
	if (depth < 0.000015259022f) {
		depth = 0.000015259022f;
	}
	if (g_std3DZCmpMode == 2) {
		depth = 1.0f - depth;
	}

	batch->verts[batch->vertexCount].sx = sx + g_flightVpOriginX;
	batch->verts[batch->vertexCount].sy = sy + g_flightVpOriginY;
	batch->verts[batch->vertexCount].sz = depth;
	batch->verts[batch->vertexCount].rhw = w;
	batch->verts[batch->vertexCount].tu = tu;
	batch->verts[batch->vertexCount].tv = tv;

	alpha = (int)(vert->litColor[0] * 255.0);
	if (g_capVertexAlpha && alpha == 255) {
		alpha = 254;
	}

	red = 48 + (int)(320.0f * vert->litColor[1]);
	if (red > 255) {
		red = 255;
	}
	green = 48 + (int)(320.0f * vert->litColor[2]);
	if (green > 255) {
		green = 255;
	}
	blue = 48 + (int)(320.0f * vert->litColor[3]);
	if (blue > 255) {
		blue = 255;
	}
	batch->verts[batch->vertexCount].color =
		((uint32_t)alpha << 24) | ((uint32_t)red << 16) | ((uint32_t)green << 8) | (uint32_t)blue;
	batch->verts[batch->vertexCount].specular = 0;
	appendedIdx = batch->vertexCount;
	batch->vertexCount = appendedIdx + 1;
	return appendedIdx;
}

// FUNCTION: XWA 0x44FCA0
void RenderBatch_AllocMeshPassBatches(void) {
	g_meshDeferredBatch = RenderBatch_Alloc();
	g_meshPass2Batch = RenderBatch_Alloc();
	g_meshMultiTexBatch = RenderBatch_Alloc();
}

// FUNCTION: XWA 0x450310
int RenderScene_ClipAndEmitFace(SceneMesh* mesh) {
	ProjVertex* vertBuf;
	SceneFace* currentFace;
	int* emitCache;

	emitCache = (int*)g_sceneEdgeList;
	vertBuf = &g_projVertList[mesh->vertBaseIndex];
	g_clipInputProjVertEndIndex = mesh->vertBaseIndex + mesh->projVertCursor;
	g_clipVertCursor = g_clipInputProjVertEndIndex;
	currentFace = &g_visFaceList[mesh->faceBaseIndex];

	{
		int cacheIter;

		for (cacheIter = 0; cacheIter < g_clipInputProjVertEndIndex; ++cacheIter) {
			emitCache[cacheIter] = -1;
		}
	}

	{
		int faceIter;
		int result;

		result = mesh->visFaceCount;
		faceIter = 0;
		if (result > 0) {
			for (; faceIter < mesh->visFaceCount; result = ++faceIter) {
				FaceRecord* faceRecord;
				SceneFace* face;
				int cornerCount;
				int remainingCorners;
				int clippedCount;
				int prevIdx;
				int i;

				face = currentFace;
				faceRecord = &mesh->pFaceGeom[face->faceIndex];
				++currentFace;
				cornerCount = (faceRecord->edgeIdx[3] != -1) + 3;
				g_clipCountA = cornerCount;

				remainingCorners = cornerCount;
				i = 0;
				while (remainingCorners > 0) {
					ProjVertex* srcVert;
					int projIdx;

					projIdx = g_vertexRemap[faceRecord->vertexIdx[i]];
					g_clipIdxA[i] = projIdx;
					srcVert = &vertBuf[g_clipIdxA[i]];
					if (srcVert->tu != mesh->pUVs[faceRecord->uvIdx[i]].u ||
						srcVert->tv != mesh->pUVs[faceRecord->uvIdx[i]].v) {
						ProjVertex* dstVert;
						int outIdx;

						outIdx = g_clipVertCursor++;
						dstVert = &vertBuf[outIdx];

						dstVert->sx = srcVert->sx;
						dstVert->sy = srcVert->sy;
						dstVert->litColor[0] = srcVert->litColor[0];
						dstVert->litColor[1] = srcVert->litColor[1];
						dstVert->litColor[2] = srcVert->litColor[2];
						dstVert->litColor[3] = srcVert->litColor[3];
						dstVert->w = srcVert->w;
						dstVert->tu = mesh->pUVs[faceRecord->uvIdx[i]].u;
						dstVert->tv = mesh->pUVs[faceRecord->uvIdx[i]].v;
						dstVert->extraLayerUVCount = srcVert->extraLayerUVCount;
						g_clipIdxA[i] = outIdx;
					}

					++i;
					--remainingCorners;
				}

				clippedCount = g_clipCountA;
				if (face->nearClipState == -1) {
					i = 0;
					if (clippedCount > 0) {
						do {
							++i;
							g_clipIdxB[i - 1] = g_clipIdxA[i - 1];
						} while (i < g_clipCountA);
					}

					clippedCount = g_clipCountA;
					g_clipCountB = clippedCount;
					i = 0;
#ifdef XWA_MODERN
					if (clippedCount > 0) {
						prevIdx = g_clipIdxB[g_clipCountA - 1];
						g_clipCountA = 0;
#else
					prevIdx = g_clipIdxB[g_clipCountA - 1];
					g_clipCountA = 0;
					if (clippedCount > 0) {
#endif
						for (i = 0; i < g_clipCountB; ++i) {
							int curIdx;

							curIdx = g_clipIdxB[i];
							RenderClip_ClipPolyNear(prevIdx, curIdx, vertBuf);
							prevIdx = curIdx;
						}
					}

					if (g_clipCountA < 2) {
						continue;
					}
				}

				face->nearClipState = g_flightVpHeight;

				g_clipCountB = 0;
				i = 0;
#ifdef XWA_MODERN
				if (g_clipCountA > 0) {
					prevIdx = g_clipIdxA[g_clipCountA - 1];
#else
				prevIdx = g_clipIdxA[g_clipCountA - 1];
				if (g_clipCountA > 0) {
#endif
					do {
						int curIdx;

						curIdx = g_clipIdxA[i];
						RenderClip_ClipPolyTop(prevIdx, curIdx, vertBuf);
						++i;
						prevIdx = curIdx;
					} while (i < g_clipCountA);
				}
				clippedCount = g_clipCountB;

				g_clipCountA = 0;
				i = 0;
#ifdef XWA_MODERN
				if (clippedCount > 0) {
					prevIdx = g_clipIdxB[clippedCount - 1];
#else
				prevIdx = g_clipIdxB[clippedCount - 1];
				if (clippedCount > 0) {
#endif
					do {
						int curIdx;

						curIdx = g_clipIdxB[i];
						RenderClip_ClipPolyBottom(prevIdx, curIdx, vertBuf);
						++i;
						prevIdx = curIdx;
					} while (i < g_clipCountB);
				}
				clippedCount = g_clipCountA;

				g_clipCountB = 0;
				i = 0;
#ifdef XWA_MODERN
				if (clippedCount > 0) {
					prevIdx = g_clipIdxA[clippedCount - 1];
#else
				prevIdx = g_clipIdxA[clippedCount - 1];
				if (clippedCount > 0) {
#endif
					do {
						int curIdx;

						curIdx = g_clipIdxA[i];
						RenderClip_ClipPolyLeft(prevIdx, curIdx, vertBuf);
						++i;
						prevIdx = curIdx;
					} while (i < g_clipCountA);
				}
				clippedCount = g_clipCountB;

				g_clipCountA = 0;
				i = 0;
#ifdef XWA_MODERN
				if (clippedCount > 0) {
					prevIdx = g_clipIdxB[clippedCount - 1];
#else
				prevIdx = g_clipIdxB[clippedCount - 1];
				if (clippedCount > 0) {
#endif
					do {
						int curIdx;

						curIdx = g_clipIdxB[i];
						RenderClip_ClipPolyRight(prevIdx, curIdx, vertBuf);
						++i;
						prevIdx = curIdx;
					} while (i < g_clipCountB);
				}

				i = 0;
				if (g_clipCountA > 0) {
					do {
						int projIdx;

						g_clipIdxB[i] = g_clipIdxA[i];
						projIdx = g_clipIdxA[i];
						if (projIdx >= g_clipInputProjVertEndIndex) {
							g_clipIdxA[i] = RenderScene_EmitClippedFaceD3DVertex(projIdx, vertBuf);
						} else {
							if (emitCache[projIdx] == -1) {
								emitCache[g_clipIdxA[i]] =
									RenderScene_EmitClippedFaceD3DVertex(projIdx, vertBuf);
							}
							g_clipIdxA[i] = emitCache[g_clipIdxA[i]];
						}
						++i;
					} while (i < g_clipCountA);
				}

				if (g_clipCountA > 2) {
					for (i = 2; i < g_clipCountA; ++i) {
						g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
						g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[i - 1];
						g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[i];
						g_triBuffer[g_d3dIndexCount].texture = NULL;
						g_triBuffer[g_d3dIndexCount].flags = g_d3dRenderStateUntexturedFace;
						if (g_capVertexAlpha) {
							g_capVertexAlpha = 0;
						}
						++g_d3dIndexCount;
					}
				}
			}
		}

		return result;
	}
}

void RenderScene_EnsureMeshBatches(void) {
	if (g_meshDeferredBatch == NULL) {
		g_meshDeferredBatch = RenderBatch_Alloc();
	}
	if (g_meshPass2Batch == NULL) {
		g_meshPass2Batch = RenderBatch_Alloc();
	}
	if (g_meshMultiTexBatch == NULL) {
		g_meshMultiTexBatch = RenderBatch_Alloc();
	}
}

// FUNCTION: XWA 0x4483C0
RenderBatch* RenderScene_FlushDeferredMeshBatches(void) {
	RenderBatch* nextBatch;

	do {
		if (g_meshPass2Batch->triCount) {
			std3D_LockExecuteBuffer();
			std3D_AddVertices(g_meshPass2Batch->verts, g_meshPass2Batch->vertexCount);
			std3D_BeginInstructions();
			std3D_AddTriangles(g_meshPass2Batch->tris, (unsigned int)g_meshPass2Batch->triCount);
			std3D_ExecuteBuffer();
		}

		nextBatch = g_meshPass2Batch->next;
		RenderBatch_Free(g_meshPass2Batch);
		g_meshPass2Batch = nextBatch;
	} while (g_meshPass2Batch != NULL);
	g_meshPass2Batch = RenderBatch_Alloc();

	do {
		if (g_meshDeferredBatch->triCount) {
			std3D_LockExecuteBuffer();
			std3D_AddVertices(g_meshDeferredBatch->verts, g_meshDeferredBatch->vertexCount);
			std3D_BeginInstructions();
			std3D_AddTriangles(g_meshDeferredBatch->tris, (unsigned int)g_meshDeferredBatch->triCount);
			std3D_ExecuteBuffer();
		}

		nextBatch = g_meshDeferredBatch->next;
		RenderBatch_Free(g_meshDeferredBatch);
		g_meshDeferredBatch = nextBatch;
	} while (g_meshDeferredBatch != NULL);
	g_meshDeferredBatch = RenderBatch_Alloc();

	do {
		if (g_meshMultiTexBatch->triCount) {
			std3D_LockExecuteBuffer();
			std3D_AddVertices(g_meshMultiTexBatch->verts, g_meshMultiTexBatch->vertexCount);
			std3D_BeginInstructions();
			std3D_AddTriangles(g_meshMultiTexBatch->tris, (unsigned int)g_meshMultiTexBatch->triCount);
			std3D_ExecuteBuffer();
		}

		nextBatch = g_meshMultiTexBatch->next;
		RenderBatch_Free(g_meshMultiTexBatch);
		g_meshMultiTexBatch = nextBatch;
	} while (g_meshMultiTexBatch != NULL);
	g_meshMultiTexBatch = RenderBatch_Alloc();

	return g_meshMultiTexBatch;
}

// FUNCTION: XWA 0x442F70
void sw3d_RasterizeMeshFaces(SceneMesh* mesh) {
	ProjVertex* vertBuf;
	SceneFace* face;
	int* emitCache;
	Std3DTextureSurface* lastCachedSurface;
	Std3DTexCacheNode* primaryTexture;
	Std3DTexCacheNode* pass2Texture;
	int faceIter;

	lastCachedSurface = NULL;
	emitCache = (int*)g_sceneEdgeList;
	vertBuf = &g_projVertList[mesh->vertBaseIndex];
	g_clipInputProjVertEndIndex = mesh->vertBaseIndex + mesh->projVertCursor;
	g_clipVertCursor = g_clipInputProjVertEndIndex;
	face = &g_visFaceList[mesh->faceBaseIndex];
	for (faceIter = 0; faceIter < g_clipInputProjVertEndIndex; ++faceIter) {
		emitCache[faceIter] = -1;
	}

	for (faceIter = 0; faceIter < mesh->visFaceCount; ++faceIter, ++face) {
		FaceRecord* faceRecord;
		int cornerCount;
		int cornerIdx;
		int clippedCount;
		int prevIdx;
		int i;
#ifdef XWA_MODERN
		int cornerVerts[4];
		int savedNearClipState;
		int faceClipVertBase;
		int splitPassCount;
		int splitPass;
#endif

		faceRecord = &mesh->pFaceGeom[face->faceIndex];
		cornerCount = (faceRecord->edgeIdx[3] != -1) + 3;
		if (-face->nearClipState == cornerCount) {
			continue;
		}

		g_clipCountA = cornerCount;
		if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
			float uvScale;
			int width;
			int height;
			int scaledWidth;
			int scaledHeight;

			uvScale = g_renderUnitFloat;
			width = mesh->pMaterial->width;
			height = mesh->pMaterial->height;
			scaledWidth = width;
			scaledHeight = height;
			if (width > height) {
				do {
					uvScale *= g_renderHalfFloat;
					scaledHeight <<= 1;
				} while (scaledHeight < width);
				scaledHeight = height;
			} else if (width < height) {
				do {
					uvScale *= g_renderHalfFloat;
					scaledWidth <<= 1;
				} while (scaledWidth < height);
				scaledWidth = width;
			}

			for (cornerIdx = 0; cornerIdx < cornerCount; ++cornerIdx) {
				float u;
				float v;
				ProjVertex* srcVert;

				u = mesh->pUVs[faceRecord->uvIdx[cornerIdx]].u;
				v = mesh->pUVs[faceRecord->uvIdx[cornerIdx]].v;
				if (scaledWidth > scaledHeight) {
					v = uvScale * v;
				} else if (scaledWidth < scaledHeight) {
					u = uvScale * u;
				}

				g_clipIdxA[cornerIdx] = g_vertexRemap[faceRecord->vertexIdx[cornerIdx]];
				srcVert = &vertBuf[g_clipIdxA[cornerIdx]];
				if (u != srcVert->tu || srcVert->tv != v) {
					ProjVertex* dstVert;
					int outIdx;

					outIdx = g_clipVertCursor++;
					dstVert = &vertBuf[outIdx];
					dstVert->sx = srcVert->sx;
					dstVert->sy = srcVert->sy;
					dstVert->litColor[0] = srcVert->litColor[0];
					dstVert->litColor[1] = srcVert->litColor[1];
					dstVert->litColor[2] = srcVert->litColor[2];
					dstVert->litColor[3] = srcVert->litColor[3];
					dstVert->w = srcVert->w;
					dstVert->tu = u;
					dstVert->tv = v;
					dstVert->extraLayerUVCount = srcVert->extraLayerUVCount;
					for (i = 0; i < dstVert->extraLayerUVCount; ++i) {
						dstVert->extraLayerUVs[i] = srcVert->extraLayerUVs[i];
					}
					g_clipIdxA[cornerIdx] = outIdx;
				}
			}
			clippedCount = g_clipCountA;
		} else {
			for (cornerIdx = 0; cornerIdx < cornerCount; ++cornerIdx) {
				ProjVertex* srcVert;
				OptTexCoord* uv;

				g_clipIdxA[cornerIdx] = g_vertexRemap[faceRecord->vertexIdx[cornerIdx]];
				srcVert = &vertBuf[g_clipIdxA[cornerIdx]];
				uv = &mesh->pUVs[faceRecord->uvIdx[cornerIdx]];
				if (srcVert->tu != uv->u || srcVert->tv != uv->v) {
					ProjVertex* dstVert;
					int outIdx;

					outIdx = g_clipVertCursor++;
					dstVert = &vertBuf[outIdx];
					dstVert->sx = srcVert->sx;
					dstVert->sy = srcVert->sy;
					dstVert->litColor[0] = srcVert->litColor[0];
					dstVert->litColor[1] = srcVert->litColor[1];
					dstVert->litColor[2] = srcVert->litColor[2];
					dstVert->litColor[3] = srcVert->litColor[3];
					dstVert->w = srcVert->w;
					dstVert->tu = uv->u;
					dstVert->tv = uv->v;
					dstVert->extraLayerUVCount = srcVert->extraLayerUVCount;
					for (i = 0; i < dstVert->extraLayerUVCount; ++i) {
						dstVert->extraLayerUVs[i] = srcVert->extraLayerUVs[i];
					}
					g_clipIdxA[cornerIdx] = outIdx;
				}
			}
			clippedCount = g_clipCountA;
		}

#ifdef XWA_MODERN
		/* Texture-wobble fix (documented deviation): clip quads as two fixed
		 * triangles (0,1,2) and (0,2,3) instead of one polygon. The original
		 * clips the whole quad and fans from the clipped polygon's first
		 * vertex, so the triangulation changes as the clip boundary moves
		 * across the viewport. Many OPT quads (hangar walls in particular)
		 * have trapezoid UV mappings that are not affine-consistent, and for
		 * those the interpolated texture interior depends on the
		 * triangulation, which makes textures warp heavily while panning.
		 * A fixed split keeps each triangle's mapping projectively stable
		 * under clipping and matches the unclipped fan exactly. The clip
		 * vertex cursor is rewound per pass so the split never allocates
		 * more scratch vertices than the original single-polygon clip. */
		for (i = 0; i < cornerCount; ++i) {
			cornerVerts[i] = g_clipIdxA[i];
		}
		savedNearClipState = face->nearClipState;
		faceClipVertBase = g_clipVertCursor;
		splitPassCount = (cornerCount == 4) ? 2 : 1;
		for (splitPass = 0; splitPass < splitPassCount; ++splitPass) {
			g_clipVertCursor = faceClipVertBase;
			if (splitPassCount == 2) {
				g_clipIdxA[0] = cornerVerts[0];
				g_clipIdxA[1] = cornerVerts[splitPass + 1];
				g_clipIdxA[2] = cornerVerts[splitPass + 2];
				g_clipCountA = 3;
			} else {
				for (i = 0; i < cornerCount; ++i) {
					g_clipIdxA[i] = cornerVerts[i];
				}
				g_clipCountA = cornerCount;
			}
			clippedCount = g_clipCountA;

			if (savedNearClipState < 0) {
#else
		if (face->nearClipState < 0) {
#endif
				for (i = 0; i < clippedCount; ++i) {
					g_clipIdxB[i] = g_clipIdxA[i];
				}
				g_clipCountB = clippedCount;
				g_clipCountA = 0;
				if (g_clipCountB > 0) {
					prevIdx = g_clipIdxB[g_clipCountB - 1];
					for (i = 0; i < g_clipCountB; ++i) {
						int curIdx;

						curIdx = g_clipIdxB[i];
						RenderClip_ClipPolyNear(prevIdx, curIdx, vertBuf);
						prevIdx = curIdx;
					}
				}
				if (g_clipCountA < 2) {
					continue;
				}
			}

			face->nearClipState = g_flightVpHeight;
			if ((g_meshRenderFlags & 0x20) == 0) {
				g_clipOccurred = 0;
				g_clipCountB = 0;
#ifdef XWA_MODERN
				if (g_clipCountA > 0) {
#endif
					prevIdx = g_clipIdxA[g_clipCountA - 1];
					for (i = 0; i < g_clipCountA; ++i) {
						int curIdx;

						curIdx = g_clipIdxA[i];
						RenderClip_ClipPolyTop(prevIdx, curIdx, vertBuf);
						prevIdx = curIdx;
					}
#ifdef XWA_MODERN
				}
#endif

				g_clipCountA = 0;
#ifdef XWA_MODERN
				if (g_clipCountB > 0) {
#endif
					prevIdx = g_clipIdxB[g_clipCountB - 1];
					for (i = 0; i < g_clipCountB; ++i) {
						int curIdx;

						curIdx = g_clipIdxB[i];
						RenderClip_ClipPolyBottom(prevIdx, curIdx, vertBuf);
						prevIdx = curIdx;
					}
#ifdef XWA_MODERN
				}
#endif

				g_clipCountB = 0;
#ifdef XWA_MODERN
				if (g_clipCountA > 0) {
#endif
					prevIdx = g_clipIdxA[g_clipCountA - 1];
					for (i = 0; i < g_clipCountA; ++i) {
						int curIdx;

						curIdx = g_clipIdxA[i];
						RenderClip_ClipPolyLeft(prevIdx, curIdx, vertBuf);
						prevIdx = curIdx;
					}
#ifdef XWA_MODERN
				}
#endif

				g_clipCountA = 0;
#ifdef XWA_MODERN
				if (g_clipCountB > 0) {
#endif
					prevIdx = g_clipIdxB[g_clipCountB - 1];
					for (i = 0; i < g_clipCountB; ++i) {
						int curIdx;

						curIdx = g_clipIdxB[i];
						RenderClip_ClipPolyRight(prevIdx, curIdx, vertBuf);
						prevIdx = curIdx;
					}
#ifdef XWA_MODERN
				}
#endif
			}

			clippedCount = g_clipCountA;
			if (mesh->alphaFlag) {
				for (i = 0; i < clippedCount; ++i) {
					int projIdx;

					g_clipIdxB[i] = g_clipIdxA[i];
					projIdx = g_clipIdxA[i];
					if (projIdx >= g_clipInputProjVertEndIndex) {
						g_clipIdxA[i] =
							RenderScene_AppendProjectedVertexToBatch(g_meshDeferredBatch, projIdx, vertBuf);
					} else {
						if (emitCache[projIdx] == -1) {
							emitCache[projIdx] = RenderScene_AppendProjectedVertexToBatch(g_meshDeferredBatch,
																						  projIdx, vertBuf);
						}
						g_clipIdxA[i] = emitCache[projIdx];
					}
				}
			} else {
				for (i = 0; i < clippedCount; ++i) {
					int projIdx;

					g_clipIdxB[i] = g_clipIdxA[i];
					projIdx = g_clipIdxA[i];
					if (projIdx >= g_clipInputProjVertEndIndex) {
						g_clipIdxA[i] = RenderScene_EmitFlightVertex(projIdx, vertBuf);
					} else {
						if (emitCache[projIdx] == -1) {
							emitCache[projIdx] = RenderScene_EmitFlightVertex(projIdx, vertBuf);
						}
						g_clipIdxA[i] = emitCache[projIdx];
					}
				}
			}

			if (clippedCount > 2) {
				int mipLevel;
				int flags;

				mipLevel = 0;
				if (mesh->pTexture == NULL || g_renderSceneForceDefaultTexture) {
					TexLevel* texLevel;

					FeDiskIo_SelectTextureFrame(0xEDu, 2u, 256);
					texLevel = g_modelTypeTable[237].curTexLevel;
					std3D_AddToTextureCache((Std3DTextureSurface*)texLevel->image);
					primaryTexture = &((Std3DTextureSurface*)texLevel->image)->cacheNode;
					pass2Texture = NULL;
				} else {
					D3DInfoNode* textureInfo;
					Std3DTextureSurface* baseSurface;
					int width;
					int height;
					int lod;

					textureInfo = mesh->pTexture;
					width = mesh->pMaterial->width;
					height = mesh->pMaterial->height;
					if (!g_hwMipmapFilter && width * height == mesh->pMaterial->textureSize) {
						lod = (int)((float)face->mipLevel * g_mipLodScale);
						while (lod > 256) {
							if (width == 8 || height == 8) {
								break;
							}
							width >>= 1;
							height >>= 1;
							lod >>= 2;
							++mipLevel;
						}
					}
					baseSurface = textureInfo->baseMipSurfaces[mipLevel];
					if (baseSurface != lastCachedSurface) {
						Std3DTextureSurface* pass2Surface;

						lastCachedSurface = baseSurface;
						if (mipLevel >= textureInfo->mipLevelCount) {
							OutputDebugStringA("TooManyMipMaps\n");
						}
						std3D_AddToTextureCache(baseSurface);
						primaryTexture = &baseSurface->cacheNode;
						pass2Surface = textureInfo->lightmapMipSurfaces[mipLevel];
						if (pass2Surface != NULL) {
							std3D_AddToTextureCache(pass2Surface);
							pass2Texture = &pass2Surface->cacheNode;
						} else {
							pass2Texture = NULL;
						}
					}
				}

				flags = g_d3dRenderStateTexturedMesh;
				if (mesh->alphaFlag) {
					for (i = 2; i < g_clipCountA; ++i) {
						g_meshDeferredBatch->tris[g_meshDeferredBatch->triCount].v0 = g_clipIdxA[0];
						g_meshDeferredBatch->tris[g_meshDeferredBatch->triCount].v1 = g_clipIdxA[i - 1];
						g_meshDeferredBatch->tris[g_meshDeferredBatch->triCount].v2 = g_clipIdxA[i];
						g_meshDeferredBatch->tris[g_meshDeferredBatch->triCount].texture = primaryTexture;
						g_meshDeferredBatch->tris[g_meshDeferredBatch->triCount].flags =
							g_d3dRenderStateDeferredAlphaMesh;
						if (g_capVertexAlpha) {
							g_capVertexAlpha = 0;
						}
						++g_meshDeferredBatch->triCount;
					}
				} else {
					for (i = 2; i < g_clipCountA; ++i) {
						g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
						g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[i - 1];
						g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[i];
						g_triBuffer[g_d3dIndexCount].texture = primaryTexture;
						g_triBuffer[g_d3dIndexCount].flags = flags;
						if (g_capVertexAlpha) {
							g_triBuffer[g_d3dIndexCount].flags += 512;
							g_capVertexAlpha = 0;
						}
						++g_d3dIndexCount;
					}
				}

				if (pass2Texture != NULL) {
					RenderBatch* batch;
					int baseVertex;

					batch = g_meshPass2Batch;
					if (g_clipCountA + batch->vertexCount > g_maxBatchVerts) {
						batch = RenderBatch_Alloc();
						batch->next = g_meshPass2Batch;
						g_meshPass2Batch = batch;
					}
					if (mesh->alphaFlag) {
						baseVertex = batch->vertexCount;
						for (i = 0; i < g_clipCountA; ++i) {
							batch->verts[baseVertex + i] = g_meshDeferredBatch->verts[g_clipIdxA[i]];
							batch->verts[baseVertex + i].color = UINT32_MAX;
						}
						batch->vertexCount = baseVertex + g_clipCountA;
						for (i = 2; i < g_clipCountA; ++i) {
							g_meshPass2Batch->tris[g_meshPass2Batch->triCount].v0 = baseVertex;
							g_meshPass2Batch->tris[g_meshPass2Batch->triCount].v1 = baseVertex + i - 1;
							g_meshPass2Batch->tris[g_meshPass2Batch->triCount].v2 = baseVertex + i;
							g_meshPass2Batch->tris[g_meshPass2Batch->triCount].texture = pass2Texture;
							g_meshPass2Batch->tris[g_meshPass2Batch->triCount].flags =
								g_d3dRenderStateMeshPass2;
							++g_meshPass2Batch->triCount;
						}
					} else {
						baseVertex = batch->vertexCount;
						for (i = 0; i < g_clipCountA; ++i) {
							batch->verts[baseVertex + i] = g_flightVertexBuffer[g_clipIdxA[i]];
							batch->verts[baseVertex + i].color = UINT32_MAX;
						}
						batch->vertexCount = baseVertex + g_clipCountA;
						for (i = 2; i < g_clipCountA; ++i) {
							g_meshPass2Batch->tris[g_meshPass2Batch->triCount].v0 = baseVertex;
							g_meshPass2Batch->tris[g_meshPass2Batch->triCount].v1 = baseVertex + i - 1;
							g_meshPass2Batch->tris[g_meshPass2Batch->triCount].v2 = baseVertex + i;
							g_meshPass2Batch->tris[g_meshPass2Batch->triCount].texture = pass2Texture;
							g_meshPass2Batch->tris[g_meshPass2Batch->triCount].flags =
								g_d3dRenderStateMeshPass2;
							++g_meshPass2Batch->triCount;
						}
					}
				}

				for (i = 0; i < mesh->textureLayerCount; ++i) {
					MeshExtraTextureLayer* layer;
					int passIdx;

					layer = mesh->layers[i];
					for (passIdx = 0; passIdx < layer->passCount; ++passIdx) {
						Std3DTextureSurface* layerSurface;
						int baseVertex;
						int k;

						layerSurface = layer->passes[passIdx].texture;
						if (g_clipCountA + g_meshMultiTexBatch->vertexCount > g_maxBatchVerts) {
							RenderBatch* batch;

							batch = RenderBatch_Alloc();
							batch->next = g_meshMultiTexBatch;
							g_meshMultiTexBatch = batch;
						}
						if (!layer->faceEnabled[face->faceId] || layerSurface == NULL) {
							continue;
						}

						std3D_AddToTextureCache(layerSurface);
						baseVertex = g_meshMultiTexBatch->vertexCount;
						for (k = 0; k < g_clipCountA; ++k) {
							OptTexCoord* extraUv;

							g_meshMultiTexBatch->verts[baseVertex + k] = g_flightVertexBuffer[g_clipIdxA[k]];
							g_meshMultiTexBatch->verts[baseVertex + k].color = layer->passes[passIdx].color;
							extraUv = &vertBuf[g_clipIdxB[k]].extraLayerUVs[i];
							g_meshMultiTexBatch->verts[baseVertex + k].tu =
								extraUv->u * layer->passes[passIdx].uvScale + 0.5f;
							g_meshMultiTexBatch->verts[baseVertex + k].tv =
								extraUv->v * layer->passes[passIdx].uvScale + 0.5f;
						}
						g_meshMultiTexBatch->vertexCount = baseVertex + g_clipCountA;
						for (k = 2; k < g_clipCountA; ++k) {
							g_meshMultiTexBatch->tris[g_meshMultiTexBatch->triCount].v0 = baseVertex;
							g_meshMultiTexBatch->tris[g_meshMultiTexBatch->triCount].v1 = baseVertex + k - 1;
							g_meshMultiTexBatch->tris[g_meshMultiTexBatch->triCount].v2 = baseVertex + k;
							g_meshMultiTexBatch->tris[g_meshMultiTexBatch->triCount].texture =
								&layerSurface->cacheNode;
							g_meshMultiTexBatch->tris[g_meshMultiTexBatch->triCount].flags =
								g_d3dRenderStateMultiTextureMesh;
							++g_meshMultiTexBatch->triCount;
						}
					}
				}
			}
#ifdef XWA_MODERN
		}
		g_clipVertCursor = faceClipVertBase;
#endif
	}

	if (!g_pStd3DCurDevice->caps.bAlphaTexture && g_meshPass2Batch->triCount) {
		do {
			RenderBatch* nextBatch;

			std3D_LockExecuteBuffer();
			std3D_AddVertices(g_meshPass2Batch->verts, g_meshPass2Batch->vertexCount);
			std3D_BeginInstructions();
			std3D_AddTriangles(g_meshPass2Batch->tris, (unsigned int)g_meshPass2Batch->triCount);
			std3D_ExecuteBuffer();
			nextBatch = g_meshPass2Batch->next;
			RenderBatch_Free(g_meshPass2Batch);
			g_meshPass2Batch = nextBatch;
		} while (g_meshPass2Batch != NULL);
		g_meshPass2Batch = RenderBatch_Alloc();
	}

	if (g_meshMultiTexBatch->vertexCount) {
		std3D_LockExecuteBuffer();
		std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
		std3D_BeginInstructions();
		std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
		std3D_ExecuteBuffer();
		g_d3dIndexCount = 0;
		g_d3dVertexCount = 0;
		if (g_meshPass2Batch != NULL && g_meshPass2Batch->vertexCount) {
			do {
				RenderBatch* nextBatch;

				std3D_LockExecuteBuffer();
				std3D_AddVertices(g_meshPass2Batch->verts, g_meshPass2Batch->vertexCount);
				std3D_BeginInstructions();
				std3D_AddTriangles(g_meshPass2Batch->tris, (unsigned int)g_meshPass2Batch->triCount);
				std3D_ExecuteBuffer();
				nextBatch = g_meshPass2Batch->next;
				RenderBatch_Free(g_meshPass2Batch);
				g_meshPass2Batch = nextBatch;
			} while (g_meshPass2Batch != NULL);
			g_meshPass2Batch = RenderBatch_Alloc();
		}
		do {
			RenderBatch* nextBatch;

			std3D_LockExecuteBuffer();
			std3D_AddVertices(g_meshMultiTexBatch->verts, g_meshMultiTexBatch->vertexCount);
			std3D_BeginInstructions();
			std3D_AddTriangles(g_meshMultiTexBatch->tris, (unsigned int)g_meshMultiTexBatch->triCount);
			std3D_ExecuteBuffer();
			nextBatch = g_meshMultiTexBatch->next;
			RenderBatch_Free(g_meshMultiTexBatch);
			g_meshMultiTexBatch = nextBatch;
		} while (g_meshMultiTexBatch != NULL);
		g_meshMultiTexBatch = RenderBatch_Alloc();
	}
}

// FUNCTION: XWA 0x489690
char RenderScene_DrawVisibleFaces(void) {
	int faceIter;

	FlightLight_ResetSoftwareFaceSampleCache();
	if (g_useHardware3D) {
		return RenderScene_EffectsPass();
	}

	if (!g_flightSurfaceAlreadyLocked) {
		FlightSurface_Lock();
	}

	g_sw3dSpanSceneMesh = NULL;
	faceIter = g_visFaceDrawStartIndex;
	while (faceIter < g_visFaceCount) {
		SceneFace* face;
		int mipTexelOffset;
		int texWidth;
		int texHeight;
		float texHeightFloat;
		int scanlineByteOffset;
		uint32_t scanY;
		float scanlineViewZBase;
		int spanOffset;
		SceneEdge scanEdgeScratch;

		mipTexelOffset = 0;
		g_sw3dCurrentFace = &g_visFaceList[faceIter];
		g_sw3dCurrentScanY = g_sw3dCurrentFace->yTop;
		g_sw3dCurrentFace->pScanEdge = &scanEdgeScratch;
		face = g_sw3dCurrentFace;
		texWidth = face->pMesh->pMaterial->width;
		texHeight = face->pMesh->pMaterial->height;

		if (g_sw3dMipmapEnabled && texWidth * texHeight == face->pMesh->pMaterial->textureSize) {
			int lod;

			lod = (int)((float)face->mipLevel * g_mipLodScale);
			if (lod > 256) {
				while (lod > 256) {
					if (texWidth == 8 || texHeight == 8) {
						break;
					}
					lod >>= 2;
					mipTexelOffset += texWidth * texHeight;
					texWidth >>= 1;
					texHeight >>= 1;
				}
			}
		}

		g_sw3dSpanTextureWidthShift = g_sw3dTextureShiftBySizeDiv16[texWidth >> 4];
		texHeightFloat = (float)texHeight;
		g_sw3dSpanTextureWidthFloat = (float)texWidth;
		g_sw3dSpanTextureHeightFloat = texHeightFloat;
		g_sw3dSpanTextureHeightShift = g_sw3dTextureShiftBySizeDiv16[texHeight >> 4];
		g_sw3dSpanTexelMask = texWidth * texHeight - 1;
		g_sw3dSpanShadeTable = (uint8_t*)face->pMesh->pPalette;
		g_sw3dSpanTexels = (uint8_t*)face->pMesh->pTexels + mipTexelOffset;
		g_sw3dSpanSceneMesh = face->pMesh;
		scanlineViewZBase = (float)(uint32_t)g_sw3dCurrentScanY * face->gradients[7] + face->gradients[8];
		scanlineByteOffset =
			g_flight16bppBytesPerPixel * g_flightVpX + g_surfacePitch * (g_sw3dCurrentScanY + g_flightVpY);
		g_sw3dScanlineByteOffset = scanlineByteOffset;

		if (scanY < (uint32_t)face->yBot) {
			spanOffset = 0;
			do {
				SceneSpan* span;

				span = *(SceneSpan**)((uint8_t*)face->pSpans + spanOffset);
				if (span == NULL) {
					scanlineViewZBase += face->gradients[7];
					scanlineByteOffset += g_surfacePitch;
					g_sw3dScanlineByteOffset = scanlineByteOffset;
				} else {
					int sampleSubrow;
					SceneSpan* clipSpan;
					int startX;
					int drawStartX;
					int endX;

					sampleSubrow = (int)(scanY & (uint32_t)g_sw3dLightSampleBlockMask);
					if (sampleSubrow != 0) {
						g_sw3dLightSampleSubrowFloat = (float)(uint32_t)sampleSubrow;
						g_sw3dLightSampleRowsToNextBlockFloat =
							(float)(uint32_t)(g_sw3dLightSampleBlockSize - sampleSubrow);
						g_sw3dLightSampleSubrowLerpT =
							g_sw3dSpanLengthReciprocal[g_sw3dLightSampleBlockSize] *
							g_sw3dLightSampleSubrowFloat;
					} else {
						g_sw3dLightSampleSubrowLerpT = 0.0f;
						g_sw3dLightSampleSubrowFloat = 0.0f;
						g_sw3dLightSampleRowsToNextBlockFloat = (float)(uint32_t)g_sw3dLightSampleBlockSize;
					}

					g_sw3dCurrentLightSampleCacheStamp =
						g_sw3dLightSampleCacheSceneStampBase + (scanY >> g_sw3dLightSampleBlockShift);

					startX = span->startX;
					clipSpan = span->next;
					endX = span->endX;
					scanEdgeScratch.lightIntensity = span->startLightIntensity;
					drawStartX = startX;
					scanEdgeScratch.x = (float)drawStartX;
					face->spanLightIntensityDx = span->dLightIntensityDx;

					if (clipSpan == NULL) {
						sw3d_DrawTexturedShadeSpan(startX, endX,
												   g_sw3dCurrentFace->gradients[6] * (float)drawStartX +
													   scanlineViewZBase);
					} else {
						int clipStartX;

						clipStartX = clipSpan->startX;
						if (clipStartX < endX) {
							while (1) {
								if (clipStartX > startX) {
									sw3d_DrawTexturedShadeSpan(startX, clipStartX,
															   (float)drawStartX *
																	   g_sw3dCurrentFace->gradients[6] +
																   scanlineViewZBase);
									startX = clipSpan->endX;
									endX = span->endX;
									drawStartX = startX;
									if (startX >= endX) {
										goto next_scanline;
									}
								} else {
									int clipEndX;

									clipEndX = clipSpan->endX;
									if (startX < clipEndX) {
										startX = clipEndX;
										drawStartX = startX;
										if (startX >= endX) {
											goto next_scanline;
										}
									}
								}

								clipSpan = clipSpan->next;
								if (clipSpan != NULL) {
									clipStartX = clipSpan->startX;
									if (clipStartX < endX) {
										continue;
									}
								}
								break;
							}
						}
						if (startX < endX) {
							sw3d_DrawTexturedShadeSpan(startX, endX,
													   (float)drawStartX * g_sw3dCurrentFace->gradients[6] +
														   scanlineViewZBase);
						}
					}

				next_scanline:
					face = g_sw3dCurrentFace;
					scanlineViewZBase += face->gradients[7];
					scanY = (uint32_t)g_sw3dCurrentScanY;
					g_sw3dScanlineByteOffset += g_surfacePitch;
				}

				g_sw3dCurrentScanY = (int)++scanY;
				spanOffset += 4;
			} while (scanY < (uint32_t)face->yBot);
		}

		++faceIter;
	}

	if (!g_flightSurfaceAlreadyLocked) {
		return (char)FlightSurface_Unlock();
	}
	return (char)g_flightSurfaceAlreadyLocked;
}

// FUNCTION: XWA 0x489B20
void sw3d_DrawVisibleFacesToSurface(void* surfacePixels, int surfacePitchBytes, unsigned int surfaceHeight) {
	int faceIter;

	FlightLight_ResetSoftwareFaceSampleCache();
	FlightSw_SetRenderTarget(surfacePixels, surfacePitchBytes >> 1, surfaceHeight, surfacePitchBytes);
	g_sw3dSpanSceneMesh = NULL;
	faceIter = g_visFaceDrawStartIndex;
	while (faceIter < g_visFaceCount) {
		int mipTexelOffset;
		int texWidth;
		int texHeight;
		SceneFace* face;
		OptTextureData* material;
		uint32_t scanY;
		float scanlineViewZBase;
		SceneEdge scanEdgeScratch;

		mipTexelOffset = 0;
		g_sw3dCurrentFace = &g_visFaceList[faceIter];
		g_sw3dCurrentScanY = g_sw3dCurrentFace->yTop;
		g_sw3dCurrentFace->pScanEdge = &scanEdgeScratch;
		face = g_sw3dCurrentFace;
		material = face->pMesh->pMaterial;
		texWidth = material->width;
		texHeight = material->height;

		if (g_sw3dMipmapEnabled && texWidth * texHeight == material->textureSize) {
			int lod;

			lod = (int)((float)face->mipLevel * g_mipLodScale);
			if (lod > 256) {
				while (lod > 256) {
					if (texWidth == 8 || texHeight == 8) {
						break;
					}
					lod >>= 2;
					mipTexelOffset += texWidth * texHeight;
					texWidth >>= 1;
					texHeight >>= 1;
				}
			}
		}

		g_sw3dSpanTextureWidthFloat = (float)texWidth;
		g_sw3dSpanTextureHeightFloat = (float)texHeight;
		g_sw3dSpanTextureWidthShift = g_sw3dTextureShiftBySizeDiv16[texWidth >> 4];
		g_sw3dSpanTextureHeightShift = g_sw3dTextureShiftBySizeDiv16[texHeight >> 4];
		g_sw3dSpanTexelMask = texWidth * texHeight - 1;
		g_sw3dSpanShadeTable = (uint8_t*)face->pMesh->pPalette;
		g_sw3dSpanTexels = (uint8_t*)face->pMesh->pTexels + mipTexelOffset;
		scanY = (uint32_t)g_sw3dCurrentScanY;
		g_sw3dSpanSceneMesh = face->pMesh;
		scanlineViewZBase = (float)scanY * face->gradients[7] + face->gradients[8];
		g_sw3dScanlineByteOffset =
			g_flight16bppBytesPerPixel * g_flightVpX + g_flightVpY * FlightSw_GetLinePitch();
		{
			int linePitch;
			int scanlineByteOffset;

			linePitch = FlightSw_GetLinePitch();
			scanY = (uint32_t)g_sw3dCurrentScanY;
			scanlineByteOffset = g_sw3dCurrentScanY * linePitch + g_sw3dScanlineByteOffset;
			g_sw3dScanlineByteOffset = scanlineByteOffset;
		}

		if (scanY < (uint32_t)g_sw3dCurrentFace->yBot) {
			int scanlineIndex;

			scanlineIndex = 0;
			do {
				SceneSpan* span;
				float scanlineViewZStep;

				span = g_sw3dCurrentFace->pSpans[scanlineIndex];
				if (span == NULL) {
					scanlineViewZStep = g_sw3dCurrentFace->gradients[7];
				} else {
					int sampleSubrow;
					SceneSpan* clipSpan;
					int startX;
					int drawStartX;
					int endX;

					sampleSubrow = (int)(scanY & (uint32_t)g_sw3dLightSampleBlockMask);
					if (sampleSubrow != 0) {
						g_sw3dLightSampleSubrowFloat = (float)(uint32_t)sampleSubrow;
						g_sw3dLightSampleRowsToNextBlockFloat =
							(float)(uint32_t)(g_sw3dLightSampleBlockSize - sampleSubrow);
						g_sw3dLightSampleSubrowLerpT =
							g_sw3dSpanLengthReciprocal[g_sw3dLightSampleBlockSize] *
							g_sw3dLightSampleSubrowFloat;
					} else {
						g_sw3dLightSampleSubrowLerpT = 0.0f;
						g_sw3dLightSampleSubrowFloat = 0.0f;
						g_sw3dLightSampleRowsToNextBlockFloat = (float)(uint32_t)g_sw3dLightSampleBlockSize;
					}

					g_sw3dCurrentLightSampleCacheStamp =
						g_sw3dLightSampleCacheSceneStampBase + (scanY >> g_sw3dLightSampleBlockShift);

					startX = span->startX;
					clipSpan = span->next;
					endX = span->endX;
					scanEdgeScratch.lightIntensity = span->startLightIntensity;
					drawStartX = startX;
					scanEdgeScratch.x = (float)drawStartX;
					g_sw3dCurrentFace->spanLightIntensityDx = span->dLightIntensityDx;

					if (clipSpan == NULL) {
						sw3d_DrawTexturedShadeSpan(startX, endX,
												   (float)drawStartX * g_sw3dCurrentFace->gradients[6] +
													   scanlineViewZBase);
					} else {
						int clipStartX;

						clipStartX = clipSpan->startX;
						if (clipStartX < endX) {
							while (1) {
								if (clipStartX > startX) {
									sw3d_DrawTexturedShadeSpan(startX, clipStartX,
															   (float)drawStartX *
																	   g_sw3dCurrentFace->gradients[6] +
																   scanlineViewZBase);
									startX = clipSpan->endX;
									endX = span->endX;
									drawStartX = startX;
									if (startX >= endX) {
										break;
									}
								} else if (startX < clipSpan->endX) {
									startX = clipSpan->endX;
									drawStartX = startX;
									if (startX >= endX) {
										break;
									}
								}

								clipSpan = clipSpan->next;
								if (clipSpan != NULL) {
									clipStartX = clipSpan->startX;
									if (clipStartX < endX) {
										continue;
									}
								}
								break;
							}
						}

						if (startX < endX) {
							sw3d_DrawTexturedShadeSpan(startX, endX,
													   (float)drawStartX * g_sw3dCurrentFace->gradients[6] +
														   scanlineViewZBase);
						}
					}
					scanlineViewZStep = g_sw3dCurrentFace->gradients[7];
				}

				scanlineViewZBase += scanlineViewZStep;
				g_sw3dScanlineByteOffset += FlightSw_GetLinePitch();
				scanY = (uint32_t)g_sw3dCurrentScanY;
				++scanlineIndex;
				g_sw3dCurrentScanY = (int)++scanY;
			} while (scanY < (uint32_t)g_sw3dCurrentFace->yBot);
		}

		++faceIter;
	}
}

void sw3d_DrawTexturedShadeSpanKernel_w3_h3(void);
void sw3d_DrawTexturedShadeSpanKernel_w3_h4(void);
void sw3d_DrawTexturedShadeSpanKernel_w3_h5(void);
void sw3d_DrawTexturedShadeSpanKernel_w3_h6(void);
void sw3d_DrawTexturedShadeSpanKernel_w3_h7(void);
void sw3d_DrawTexturedShadeSpanKernel_w3_h8(void);
void sw3d_DrawTexturedShadeSpanKernel_w4_h3(void);
void sw3d_DrawTexturedShadeSpanKernel_w4_h4(void);
void sw3d_DrawTexturedShadeSpanKernel_w4_h5(void);
void sw3d_DrawTexturedShadeSpanKernel_w4_h6(void);
void sw3d_DrawTexturedShadeSpanKernel_w4_h7(void);
void sw3d_DrawTexturedShadeSpanKernel_w4_h8(void);
void sw3d_DrawTexturedShadeSpanKernel_w5_h3(void);
void sw3d_DrawTexturedShadeSpanKernel_w5_h4(void);
void sw3d_DrawTexturedShadeSpanKernel_w5_h5(void);
void sw3d_DrawTexturedShadeSpanKernel_w5_h6(void);
void sw3d_DrawTexturedShadeSpanKernel_w5_h7(void);
void sw3d_DrawTexturedShadeSpanKernel_w5_h8(void);
void sw3d_DrawTexturedShadeSpanKernel_w6_h3(void);
void sw3d_DrawTexturedShadeSpanKernel_w6_h4(void);
void sw3d_DrawTexturedShadeSpanKernel_w6_h5(void);
void sw3d_DrawTexturedShadeSpanKernel_w6_h6(void);
void sw3d_DrawTexturedShadeSpanKernel_w6_h7(void);
void sw3d_DrawTexturedShadeSpanKernel_w6_h8(void);
void sw3d_DrawTexturedShadeSpanKernel_w7_h3(void);
void sw3d_DrawTexturedShadeSpanKernel_w7_h4(void);
void sw3d_DrawTexturedShadeSpanKernel_w7_h5(void);
void sw3d_DrawTexturedShadeSpanKernel_w7_h6(void);
void sw3d_DrawTexturedShadeSpanKernel_w7_h7(void);
void sw3d_DrawTexturedShadeSpanKernel_w7_h8(void);
void sw3d_DrawTexturedShadeSpanKernel_w8_h3(void);
void sw3d_DrawTexturedShadeSpanKernel_w8_h4(void);
void sw3d_DrawTexturedShadeSpanKernel_w8_h5(void);
void sw3d_DrawTexturedShadeSpanKernel_w8_h6(void);
void sw3d_DrawTexturedShadeSpanKernel_w8_h7(void);
void sw3d_DrawTexturedShadeSpanKernel_w8_h8(void);
void sw3d_DrawTexturedShadeSpanGeneric(void);

static __inline void sw3d_UpdateLightSampleAtBoundary(SoftwareLightSample* sample, int screenX, int screenY,
													  float viewZ) {
	float sampleIntensity;
	int stampDelta;

	stampDelta = g_sw3dCurrentLightSampleCacheStamp - sample->stamp;
	if (stampDelta == 0) {
		return;
	}

	if (stampDelta != 1) {
		sample->stamp = g_sw3dCurrentLightSampleCacheStamp;
		sample->intensity =
			FlightLight_ComputeSoftwareFaceSampleIntensity(g_sw3dCurrentFace, screenX, screenY, viewZ);
		sampleIntensity = FlightLight_ComputeSoftwareFaceSampleIntensity(
			g_sw3dCurrentFace, screenX, screenY + g_sw3dLightSampleBlockSize, viewZ);
		sample->rowDelta = sampleIntensity - sample->intensity;
	} else {
		sample->stamp = g_sw3dCurrentLightSampleCacheStamp;
		sampleIntensity = FlightLight_ComputeSoftwareFaceSampleIntensity(
			g_sw3dCurrentFace, screenX, screenY + g_sw3dLightSampleBlockSize, viewZ);
		sample->intensity += sample->rowDelta;
		sample->rowDelta = sampleIntensity - sample->intensity;
	}
}

static __inline void sw3d_DrawTexturedShadeSpanKernel(void) {
	switch (g_sw3dSpanTextureWidthShift) {
		case 3:
			switch (g_sw3dSpanTextureHeightShift) {
				case 3:
					sw3d_DrawTexturedShadeSpanKernel_w3_h3();
					break;
				case 4:
					sw3d_DrawTexturedShadeSpanKernel_w3_h4();
					break;
				case 5:
					sw3d_DrawTexturedShadeSpanKernel_w3_h5();
					break;
				case 6:
					sw3d_DrawTexturedShadeSpanKernel_w3_h6();
					break;
				case 7:
					sw3d_DrawTexturedShadeSpanKernel_w3_h7();
					break;
				case 8:
					sw3d_DrawTexturedShadeSpanKernel_w3_h8();
					break;
				default:
					sw3d_DrawTexturedShadeSpanGeneric();
					break;
			}
			break;
		case 4:
			switch (g_sw3dSpanTextureHeightShift) {
				case 3:
					sw3d_DrawTexturedShadeSpanKernel_w4_h3();
					break;
				case 4:
					sw3d_DrawTexturedShadeSpanKernel_w4_h4();
					break;
				case 5:
					sw3d_DrawTexturedShadeSpanKernel_w4_h5();
					break;
				case 6:
					sw3d_DrawTexturedShadeSpanKernel_w4_h6();
					break;
				case 7:
					sw3d_DrawTexturedShadeSpanKernel_w4_h7();
					break;
				case 8:
					sw3d_DrawTexturedShadeSpanKernel_w4_h8();
					break;
				default:
					sw3d_DrawTexturedShadeSpanGeneric();
					break;
			}
			break;
		case 5:
			switch (g_sw3dSpanTextureHeightShift) {
				case 3:
					sw3d_DrawTexturedShadeSpanKernel_w5_h3();
					break;
				case 4:
					sw3d_DrawTexturedShadeSpanKernel_w5_h4();
					break;
				case 5:
					sw3d_DrawTexturedShadeSpanKernel_w5_h5();
					break;
				case 6:
					sw3d_DrawTexturedShadeSpanKernel_w5_h6();
					break;
				case 7:
					sw3d_DrawTexturedShadeSpanKernel_w5_h7();
					break;
				case 8:
					sw3d_DrawTexturedShadeSpanKernel_w5_h8();
					break;
				default:
					sw3d_DrawTexturedShadeSpanGeneric();
					break;
			}
			break;
		case 6:
			switch (g_sw3dSpanTextureHeightShift) {
				case 3:
					sw3d_DrawTexturedShadeSpanKernel_w6_h3();
					break;
				case 4:
					sw3d_DrawTexturedShadeSpanKernel_w6_h4();
					break;
				case 5:
					sw3d_DrawTexturedShadeSpanKernel_w6_h5();
					break;
				case 6:
					sw3d_DrawTexturedShadeSpanKernel_w6_h6();
					break;
				case 7:
					sw3d_DrawTexturedShadeSpanKernel_w6_h7();
					break;
				case 8:
					sw3d_DrawTexturedShadeSpanKernel_w6_h8();
					break;
				default:
					sw3d_DrawTexturedShadeSpanGeneric();
					break;
			}
			break;
		case 7:
			switch (g_sw3dSpanTextureHeightShift) {
				case 3:
					sw3d_DrawTexturedShadeSpanKernel_w7_h3();
					break;
				case 4:
					sw3d_DrawTexturedShadeSpanKernel_w7_h4();
					break;
				case 5:
					sw3d_DrawTexturedShadeSpanKernel_w7_h5();
					break;
				case 6:
					sw3d_DrawTexturedShadeSpanKernel_w7_h6();
					break;
				case 7:
					sw3d_DrawTexturedShadeSpanKernel_w7_h7();
					break;
				case 8:
					sw3d_DrawTexturedShadeSpanKernel_w7_h8();
					break;
				default:
					sw3d_DrawTexturedShadeSpanGeneric();
					break;
			}
			break;
		case 8:
			switch (g_sw3dSpanTextureHeightShift) {
				case 3:
					sw3d_DrawTexturedShadeSpanKernel_w8_h3();
					break;
				case 4:
					sw3d_DrawTexturedShadeSpanKernel_w8_h4();
					break;
				case 5:
					sw3d_DrawTexturedShadeSpanKernel_w8_h5();
					break;
				case 6:
					sw3d_DrawTexturedShadeSpanKernel_w8_h6();
					break;
				case 7:
					sw3d_DrawTexturedShadeSpanKernel_w8_h7();
					break;
				case 8:
					sw3d_DrawTexturedShadeSpanKernel_w8_h8();
					break;
				default:
					sw3d_DrawTexturedShadeSpanGeneric();
					break;
			}
			break;
		default:
			sw3d_DrawTexturedShadeSpanGeneric();
			break;
	}
}

// FUNCTION: XWA 0x48AE60
void sw3d_DrawTexturedShadeSpan(int startX, int endX, float startViewZ) {
	SceneFace* face;
	SoftwareLightSample* lightSamples;
	SoftwareLightSample* leftSample;
	SoftwareLightSample* rightSample;
	Sw3dFloatInt endShade;
	Sw3dFloatInt nextU;
	Sw3dFloatInt nextV;
	float uAtY;
	float vAtY;
	float* uGradient;
	float* vGradient;
	float viewZAtY;
	float uNumerator;
	float vNumerator;
	float inverseViewZ;
	float u;
	float v;
	float rightU;
	float rightV;
	float rightLight;
	float leftLight;
	float lightIntensity;
	float lightIntensityAtEnd;
	float lightIntensityBlockStep;
	float uNumeratorBlockStep;
	float vNumeratorBlockStep;
	float viewZBlockStep;
	float boundaryViewZ;
	int startBlock;
	int endBlock;
	int block;
	int boundaryX;
	int blockStartX;
	int blockStartY;
	int withinBlockX;
	int withinBlockY;
	int shadeStep;
	int stampDelta;

	face = g_sw3dCurrentFace;
	lightSamples = (SoftwareLightSample*)face->pPhongData;
	viewZAtY = (float)(unsigned int)g_sw3dCurrentScanY * g_sw3dCurrentFace->gradients[7];
	viewZAtY += g_sw3dCurrentFace->gradients[8];
	uGradient = &face->gradients[0];
	vGradient = &face->gradients[3];
	uAtY = (float)g_sw3dCurrentScanY * uGradient[1];
	vAtY = (float)g_sw3dCurrentScanY * vGradient[1];
	uAtY += uGradient[2];
	vAtY += vGradient[2];
	uNumerator = (float)startX * uGradient[0] + uAtY;
	vNumerator = (float)startX * vGradient[0] + vAtY;
	inverseViewZ = g_sw3dOneFloat / startViewZ;
	lightIntensity =
		((float)startX - face->pScanEdge->x) * face->spanLightIntensityDx + face->pScanEdge->lightIntensity;

	g_sw3dSpanShadeDitherAccum = g_sw3dShadeDitherInitialByScanlineParity[g_sw3dCurrentScanY & 1];
	startBlock = startX >> g_sw3dLightSampleBlockShift;
	endBlock = (endX - 1) >> g_sw3dLightSampleBlockShift;
	g_sw3dSpanStartX = startX;
	leftSample = &lightSamples[startBlock];
	withinBlockX = startX & g_sw3dLightSampleBlockMask;
	withinBlockY = g_sw3dCurrentScanY & g_sw3dLightSampleBlockMask;
	blockStartX = startX - withinBlockX;
	blockStartY = g_sw3dCurrentScanY - withinBlockY;

	stampDelta = g_sw3dCurrentLightSampleCacheStamp - leftSample->stamp;
	if (stampDelta != 0) {
		if (stampDelta != 1) {
			leftSample->stamp = g_sw3dCurrentLightSampleCacheStamp;
			leftSample->intensity = FlightLight_ComputeSoftwareFaceSampleIntensity(
				face, blockStartX, blockStartY,
				startViewZ - (float)withinBlockX * face->gradients[6] -
					g_sw3dLightSampleSubrowFloat * face->gradients[7]);
		} else {
			leftSample->stamp++;
			leftSample->intensity += leftSample->rowDelta;
		}
		leftSample->rowDelta = FlightLight_ComputeSoftwareFaceSampleIntensity(
								   face, blockStartX, blockStartY + g_sw3dLightSampleBlockSize,
								   startViewZ - (float)withinBlockX * face->gradients[6] +
									   g_sw3dLightSampleRowsToNextBlockFloat * face->gradients[7]) -
							   leftSample->intensity;
	}

	leftLight = leftSample->intensity + g_sw3dLightSampleSubrowLerpT * leftSample->rowDelta;
	u = inverseViewZ * uNumerator;
	v = inverseViewZ * vNumerator;

	boundaryX = (startBlock + 1) << g_sw3dLightSampleBlockShift;
	g_sw3dSpanLength = boundaryX - g_sw3dSpanStartX;
	uNumerator = (float)boundaryX * face->gradients[0] + uAtY;
	vNumerator = (float)boundaryX * face->gradients[3] + vAtY;
	boundaryViewZ = (float)boundaryX * face->gradients[6] + viewZAtY;
	block = startBlock + 1;
	inverseViewZ = g_sw3dOneFloat / boundaryViewZ;
	rightSample = &lightSamples[block];
	sw3d_UpdateLightSampleAtBoundary(rightSample, boundaryX, blockStartY, boundaryViewZ);
	rightLight = rightSample->intensity + g_sw3dLightSampleSubrowLerpT * rightSample->rowDelta;
	leftLight += (rightLight - leftLight) * ((float)withinBlockX * g_sw3dLightSampleInvBlockSize);
	rightU = inverseViewZ * uNumerator;
	rightV = inverseViewZ * vNumerator;

	g_sw3dSpanStepUQ8.asFloat = (rightU - u) * g_sw3dSpanLengthReciprocal[g_sw3dSpanLength] +
								g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureWidthShift].asFloat;
	g_sw3dSpanStepVQ8.asFloat = (rightV - v) * g_sw3dSpanLengthReciprocal[g_sw3dSpanLength] +
								g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureHeightShift].asFloat;
	g_sw3dSpanStepUQ8.asInt -= g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureWidthShift].asInt;
	g_sw3dSpanStepVQ8.asInt -= g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureHeightShift].asInt;

	if ((unsigned int)block > (unsigned int)endBlock) {
		g_sw3dSpanLength = endX - g_sw3dSpanStartX;
	} else {
		uNumeratorBlockStep = face->gradients[0] * g_sw3dLightSampleBlockSizeFloat;
		vNumeratorBlockStep = face->gradients[3] * g_sw3dLightSampleBlockSizeFloat;
		viewZBlockStep = face->gradients[6] * g_sw3dLightSampleBlockSizeFloat;
	}

	lightIntensityAtEnd = (float)g_sw3dSpanLength * face->spanLightIntensityDx + lightIntensity;
	lightIntensityBlockStep = face->spanLightIntensityDx * g_sw3dLightSampleBlockSizeFloat;
	g_sw3dSpanShadeQ8.asFloat = (leftLight + lightIntensity) * g_sw3dLightIntensityToShadeScale +
								g_sw3dTexCoordBiasByShift[0].asFloat;
	endShade.asFloat = (rightLight + lightIntensityAtEnd) * g_sw3dLightIntensityToShadeScale +
					   g_sw3dTexCoordBiasByShift[0].asFloat;
	g_sw3dSpanShadeQ8.asInt -= g_sw3dTexCoordBiasByShift[0].asInt;
	if (g_sw3dSpanShadeQ8.asInt < 0) {
		g_sw3dSpanShadeQ8.asInt = 0;
	}
	if (g_sw3dSpanShadeQ8.asInt > 0xEFF) {
		g_sw3dSpanShadeQ8.asInt = 0xEFF;
	}
	endShade.asInt -= g_sw3dTexCoordBiasByShift[0].asInt;
	if (endShade.asInt < 0) {
		endShade.asInt = 0;
	}
	if (endShade.asInt > 0xEFF) {
		endShade.asInt = 0xEFF;
	}
	shadeStep = endShade.asInt - g_sw3dSpanShadeQ8.asInt;
	if (shadeStep < 0) {
		shadeStep += g_sw3dLightSampleBlockSize;
	}
	g_sw3dSpanShadeStepQ8.asFloat =
		(float)shadeStep * g_sw3dSpanLengthReciprocal[g_sw3dSpanLength] + g_sw3dFloatToIntRoundBias.asFloat;
	g_sw3dSpanShadeStepQ8.asInt -= g_sw3dFloatToIntRoundBias.asInt;

	g_sw3dSpanUQ8.asFloat = u + g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureWidthShift].asFloat;
	nextU.asFloat = rightU + g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureWidthShift].asFloat;
	g_sw3dSpanVQ8.asFloat = v + g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureHeightShift].asFloat;
	nextV.asFloat = rightV + g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureHeightShift].asFloat;
	g_sw3dSpanUQ8.asInt -= g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureWidthShift].asInt;
	nextU.asInt -= g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureWidthShift].asInt;
	g_sw3dSpanVQ8.asInt -= g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureHeightShift].asInt;
	nextV.asInt -= g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureHeightShift].asInt;

	while (1) {
		if (block <= endBlock) {
			boundaryViewZ += viewZBlockStep;
			inverseViewZ = g_sw3dOneFloat / boundaryViewZ;
		}

		sw3d_DrawTexturedShadeSpanKernel();
		if (block > endBlock) {
			break;
		}

		uNumerator += uNumeratorBlockStep;
		vNumerator += vNumeratorBlockStep;
		g_sw3dSpanStartX += g_sw3dSpanLength;
		boundaryX += g_sw3dLightSampleBlockSize;
		if (block == endBlock) {
			g_sw3dSpanLength = endX - g_sw3dSpanStartX;
		} else {
			g_sw3dSpanLength = g_sw3dLightSampleBlockSize;
		}
		block++;
		rightSample++;
		sw3d_UpdateLightSampleAtBoundary(rightSample, boundaryX, blockStartY, boundaryViewZ);
		rightU = inverseViewZ * uNumerator;
		rightV = inverseViewZ * vNumerator;
		rightLight = rightSample->intensity + g_sw3dLightSampleSubrowLerpT * rightSample->rowDelta;
		lightIntensityAtEnd += lightIntensityBlockStep;

		g_sw3dSpanShadeQ8.asInt = endShade.asInt;
		endShade.asFloat = (rightLight + lightIntensityAtEnd) * g_sw3dLightIntensityToShadeScale +
						   g_sw3dTexCoordBiasByShift[0].asFloat;
		endShade.asInt -= g_sw3dTexCoordBiasByShift[0].asInt;
		if (endShade.asInt < 0) {
			endShade.asInt = 0;
		}
		if (endShade.asInt > 0xEFF) {
			endShade.asInt = 0xEFF;
		}
		shadeStep = endShade.asInt - g_sw3dSpanShadeQ8.asInt;
		if (shadeStep < 0) {
			shadeStep += g_sw3dLightSampleBlockSize;
		}
		g_sw3dSpanShadeStepQ8.asInt = shadeStep >> g_sw3dLightSampleBlockShift;

		g_sw3dSpanUQ8.asInt = nextU.asInt;
		g_sw3dSpanVQ8.asInt = nextV.asInt;
		nextU.asFloat = rightU + g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureWidthShift].asFloat;
		nextV.asFloat = rightV + g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureHeightShift].asFloat;
		nextU.asInt -= g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureWidthShift].asInt;
		nextV.asInt -= g_sw3dTexCoordBiasByShift[g_sw3dSpanTextureHeightShift].asInt;
		g_sw3dSpanStepUQ8.asInt = (nextU.asInt - g_sw3dSpanUQ8.asInt) >> g_sw3dLightSampleBlockShift;
		g_sw3dSpanStepVQ8.asInt = (nextV.asInt - g_sw3dSpanVQ8.asInt) >> g_sw3dLightSampleBlockShift;
	}
}

// FUNCTION: XWA 0x48F260
void sw3d_DrawTexturedShadeSpanGeneric(void) {
	uint8_t* surfacePixels;
	int pixelOffset;
	int pixelEnd;

	surfacePixels = (uint8_t*)g_surfacePixels;
	pixelOffset = g_sw3dScanlineByteOffset + g_sw3dSpanStartX;
	pixelEnd = pixelOffset + g_sw3dSpanLength;
	while (pixelOffset < pixelEnd) {
		unsigned int shadeAccum;
		int texelIndex;
		uint8_t texel;

		texelIndex = ((g_sw3dSpanVQ8.asInt >> 8) << g_sw3dSpanTextureWidthShift) + (g_sw3dSpanUQ8.asInt >> 8);
		texel = g_sw3dSpanTexels[texelIndex & g_sw3dSpanTexelMask];
		shadeAccum = (unsigned int)(g_sw3dSpanShadeDitherAccum + g_sw3dSpanShadeQ8.asInt);
		g_sw3dSpanShadeDitherAccum = (uint8_t)shadeAccum;
		surfacePixels[pixelOffset++] =
			g_sw3dSpanShadeTable[((shadeAccum >> 8) & 0xF) * 512 + (unsigned int)texel * 2];
		g_sw3dSpanShadeQ8.asInt += g_sw3dSpanShadeStepQ8.asInt;
		g_sw3dSpanUQ8.asInt += g_sw3dSpanStepUQ8.asInt;
		g_sw3dSpanVQ8.asInt += g_sw3dSpanStepVQ8.asInt;
	}
}

/* TODO: Replace these behaviorally-correct generic fallbacks with the original unrolled texture-size kernels.
 */
// FUNCTION: XWA 0x48BA20
void sw3d_DrawTexturedShadeSpanKernel_w3_h3(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48BBB0
void sw3d_DrawTexturedShadeSpanKernel_w3_h4(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48BD40
void sw3d_DrawTexturedShadeSpanKernel_w3_h5(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48BED0
void sw3d_DrawTexturedShadeSpanKernel_w3_h6(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48C060
void sw3d_DrawTexturedShadeSpanKernel_w3_h7(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48C1F0
void sw3d_DrawTexturedShadeSpanKernel_w3_h8(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48C380
void sw3d_DrawTexturedShadeSpanKernel_w4_h3(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48C510
void sw3d_DrawTexturedShadeSpanKernel_w4_h4(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48C6A0
void sw3d_DrawTexturedShadeSpanKernel_w4_h5(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48C830
void sw3d_DrawTexturedShadeSpanKernel_w4_h6(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48C9C0
void sw3d_DrawTexturedShadeSpanKernel_w4_h7(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48CB50
void sw3d_DrawTexturedShadeSpanKernel_w4_h8(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48CCE0
void sw3d_DrawTexturedShadeSpanKernel_w5_h3(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48CE70
void sw3d_DrawTexturedShadeSpanKernel_w5_h4(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48D000
void sw3d_DrawTexturedShadeSpanKernel_w5_h5(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48D190
void sw3d_DrawTexturedShadeSpanKernel_w5_h6(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48D320
void sw3d_DrawTexturedShadeSpanKernel_w5_h7(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48D4B0
void sw3d_DrawTexturedShadeSpanKernel_w5_h8(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48D640
void sw3d_DrawTexturedShadeSpanKernel_w6_h3(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48D7D0
void sw3d_DrawTexturedShadeSpanKernel_w6_h4(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48D960
void sw3d_DrawTexturedShadeSpanKernel_w6_h5(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48DAF0
void sw3d_DrawTexturedShadeSpanKernel_w6_h6(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48DC80
void sw3d_DrawTexturedShadeSpanKernel_w6_h7(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48DE10
void sw3d_DrawTexturedShadeSpanKernel_w6_h8(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48DFA0
void sw3d_DrawTexturedShadeSpanKernel_w7_h3(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48E130
void sw3d_DrawTexturedShadeSpanKernel_w7_h4(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48E2C0
void sw3d_DrawTexturedShadeSpanKernel_w7_h5(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48E450
void sw3d_DrawTexturedShadeSpanKernel_w7_h6(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48E5E0
void sw3d_DrawTexturedShadeSpanKernel_w7_h7(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48E770
void sw3d_DrawTexturedShadeSpanKernel_w7_h8(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48E900
void sw3d_DrawTexturedShadeSpanKernel_w8_h3(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48EA90
void sw3d_DrawTexturedShadeSpanKernel_w8_h4(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48EC20
void sw3d_DrawTexturedShadeSpanKernel_w8_h5(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48EDB0
void sw3d_DrawTexturedShadeSpanKernel_w8_h6(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48EF40
void sw3d_DrawTexturedShadeSpanKernel_w8_h7(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
// FUNCTION: XWA 0x48F0D0
void sw3d_DrawTexturedShadeSpanKernel_w8_h8(void) { sw3d_DrawTexturedShadeSpanGeneric(); }
