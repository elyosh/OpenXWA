#include "xwa/render/renderer_internal.h"

// GLOBAL: XWA 0x5A9400
const float g_renderQuadUnitFloat = 1.0f;

// FUNCTION: XWA 0x44BDB0
void RenderQuad_SubmitClippedTriangle(ProjVertex* verts3, Std3DTexCacheNode* cacheNode, float depthZ,
									  int nearFlag) {
	int i;
	int inputCount;
	int topCount;
	int bottomCount;
	int leftCount;
	int prevVert;

	if (nearFlag) {
		for (i = 0; i < g_clipCountA; ++i) {
			g_clipIdxB[i] = g_clipIdxA[i];
		}
		g_clipCountB = g_clipCountA;
		i = 0;
#ifndef XWA_MODERN
		prevVert = g_clipIdxB[g_clipCountB - 1];
#endif
		g_clipCountA = 0;
		if (g_clipCountB > 0) {
#ifdef XWA_MODERN
			prevVert = g_clipIdxB[g_clipCountB - 1];
#endif
			do {
				int curVert;

				curVert = g_clipIdxB[i];
				RenderClip_ClipPolyNear(prevVert, curVert, verts3);
				prevVert = curVert;
				++i;
			} while (i < g_clipCountB);
		}
	}

	inputCount = g_clipCountA;
	i = 0;
	g_clipCountB = 0;
#ifndef XWA_MODERN
	prevVert = g_clipIdxA[inputCount - 1];
#endif
	if (inputCount > 0) {
#ifdef XWA_MODERN
		prevVert = g_clipIdxA[inputCount - 1];
#endif
		do {
			int curVert;

			curVert = g_clipIdxA[i];
			RenderClip_ClipPolyTop(prevVert, curVert, verts3);
			prevVert = curVert;
			++i;
		} while (i < g_clipCountA);
	}
	topCount = g_clipCountB;

	i = 0;
	g_clipCountA = 0;
#ifndef XWA_MODERN
	prevVert = g_clipIdxB[topCount - 1];
#endif
	if (topCount > 0) {
#ifdef XWA_MODERN
		prevVert = g_clipIdxB[topCount - 1];
#endif
		do {
			int curVert;

			curVert = g_clipIdxB[i];
			RenderClip_ClipPolyBottom(prevVert, curVert, verts3);
			prevVert = curVert;
			++i;
		} while (i < g_clipCountB);
	}
	bottomCount = g_clipCountA;

	i = 0;
	g_clipCountB = 0;
#ifndef XWA_MODERN
	prevVert = g_clipIdxA[bottomCount - 1];
#endif
	if (bottomCount > 0) {
#ifdef XWA_MODERN
		prevVert = g_clipIdxA[bottomCount - 1];
#endif
		do {
			int curVert;

			curVert = g_clipIdxA[i];
			RenderClip_ClipPolyLeft(prevVert, curVert, verts3);
			prevVert = curVert;
			++i;
		} while (i < g_clipCountA);
	}
	leftCount = g_clipCountB;

	i = 0;
	g_clipCountA = 0;
#ifndef XWA_MODERN
	prevVert = g_clipIdxB[leftCount - 1];
#endif
	if (leftCount > 0) {
#ifdef XWA_MODERN
		prevVert = g_clipIdxB[leftCount - 1];
#endif
		do {
			int curVert;

			curVert = g_clipIdxB[i];
			RenderClip_ClipPolyRight(prevVert, curVert, verts3);
			prevVert = curVert;
			++i;
		} while (i < g_clipCountB);
	}

	if (g_clipCountA >= 3) {
		int vertexCount;

		vertexCount = g_clipCountA;
		if (vertexCount + g_d3dVertexCount > g_maxBatchVerts ||
			vertexCount + g_d3dIndexCount > g_maxBatchTris) {
			std3D_LockExecuteBuffer();
			std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
			std3D_BeginInstructions();
			std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
			std3D_ExecuteBuffer();
			g_d3dIndexCount = 0;
			g_d3dVertexCount = 0;
			vertexCount = g_clipCountA;
		}

		if (g_capVertexAlpha) {
			g_capVertexAlpha = 0;
		}

		if (vertexCount > 0) {
			i = 0;
			do {
				ProjVertex* vert;
				float screenY;
				float texU;
				float texV;
#ifdef XWA_MODERN
				uint32_t color;
#else
				int color;
#endif

				vert = &verts3[g_clipIdxA[i]];
				screenY = vert->sy;
				texU = vert->tu;
				texV = vert->tv;
				color = (int)(vert->litColor[0] * g_vertexColorAlphaScale);
				color = color * 256 + (int)(vert->litColor[1] * g_vertexColorAlphaScale);
				color = color * 256 | (int)(vert->litColor[2] * g_vertexColorAlphaScale);
				color = color * 256 | (int)(vert->litColor[3] * g_vertexColorAlphaScale);
				{
					D3DTLVERTEX* outVert;
					float screenX;

					screenX = vert->sx;
					outVert = &g_flightVertexBuffer[g_d3dVertexCount];
					screenX += g_flightVpOriginX;
					outVert->sx = screenX;
				}
				g_flightVertexBuffer[g_d3dVertexCount].sy = screenY + g_flightVpOriginY;
				g_flightVertexBuffer[g_d3dVertexCount].sz = depthZ;
				g_flightVertexBuffer[g_d3dVertexCount].rhw = depthZ;
				g_flightVertexBuffer[g_d3dVertexCount].tu = texU;
				g_flightVertexBuffer[g_d3dVertexCount].tv = texV;
				g_flightVertexBuffer[g_d3dVertexCount].color = color;
				g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
				g_clipIdxA[i] = g_d3dVertexCount++;
				++i;
			} while (i < g_clipCountA);
		}

		for (i = 2; i < g_clipCountA; ++i) {
			g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
			g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[i - 1];
			g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[i];
			g_triBuffer[g_d3dIndexCount].texture = cacheNode;
			g_triBuffer[g_d3dIndexCount].flags = g_d3dRenderStateGlowQuad;
			++g_d3dIndexCount;
		}
	}
}

// FUNCTION: XWA 0x44AFE0
void RenderQuad_DrawTextured3D(const int* corners, TexLevel* texLevel, int d3dFlags) {
	Std3DTextureSurface* textureSurface;
	ProjVertex vertBuf[40];
	const int* corner;
	ProjVertex* vert;
	OptTexCoord* texCoord;
	float uScale;
	float vScale;
	int nearFlag;
	int i;
	int inputCount;
	int topCount;
	int bottomCount;
	int leftCount;
	int prevVert;

	textureSurface = (Std3DTextureSurface*)texLevel->image;
	g_clipCountA = 4;
	g_clipVertCursor = 4;
	g_clipIdxA[0] = 0;
	nearFlag = 0;
	uScale = g_renderUnitFloat;
	g_clipIdxA[1] = 1;
	vScale = g_renderUnitFloat;
	g_clipIdxA[2] = 2;
	g_clipIdxA[3] = 3;
	if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
		int squareWidth;
		int squareHeight;

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

	corner = corners + 2;
	vert = vertBuf;
	texCoord = g_currentQuadTexCoords;
	i = 4;
	do {
		float* projectedW;

		projectedW = &vert->w;
		if (corner[0] < 1) {
			nearFlag = 1;
			*projectedW = (float)(corner[0] - 1);
			vert->sx = (float)corner[-2];
			vert->sy = (float)corner[-1];
		} else {
			*projectedW = g_projScale / (float)corner[0];
			vert->sx = (float)corner[-2] * *projectedW + g_flightVpCenterXf;
			vert->sy = (float)corner[-1] * *projectedW + g_projOffsetYf + g_flightVpCenterYf;
		}
		vert->litColor[0] = 1.0f;
		vert->litColor[1] = 0.0f;
		vert->litColor[2] = 0.0f;
		vert->litColor[3] = 0.0f;
		vert->tu = texCoord->u * uScale;
		vert->tv = texCoord->v * vScale;
		vert->extraLayerUVCount = 0;
		corner += 3;
		++texCoord;
		++vert;
	} while (--i != 0);

	if (nearFlag) {
		for (i = 0; i < g_clipCountA; ++i) {
			g_clipIdxB[i] = g_clipIdxA[i];
		}
		g_clipCountB = g_clipCountA;
		i = 0;
#ifndef XWA_MODERN
		prevVert = g_clipIdxB[g_clipCountB - 1];
#endif
		g_clipCountA = 0;
		if (g_clipCountB > 0) {
#ifdef XWA_MODERN
			prevVert = g_clipIdxB[g_clipCountB - 1];
#endif
			do {
				int curVert;

				curVert = g_clipIdxB[i];
				RenderClip_ClipPolyNear(prevVert, curVert, vertBuf);
				prevVert = curVert;
				++i;
			} while (i < g_clipCountB);
		}
	}

	inputCount = g_clipCountA;
	i = 0;
	g_clipCountB = 0;
#ifndef XWA_MODERN
	prevVert = g_clipIdxA[inputCount - 1];
#endif
	if (inputCount > 0) {
#ifdef XWA_MODERN
		prevVert = g_clipIdxA[inputCount - 1];
#endif
		do {
			int curVert;

			curVert = g_clipIdxA[i];
			RenderClip_ClipPolyTop(prevVert, curVert, vertBuf);
			prevVert = curVert;
			++i;
		} while (i < g_clipCountA);
	}
	topCount = g_clipCountB;

	i = 0;
	g_clipCountA = 0;
#ifndef XWA_MODERN
	prevVert = g_clipIdxB[topCount - 1];
#endif
	if (topCount > 0) {
#ifdef XWA_MODERN
		prevVert = g_clipIdxB[topCount - 1];
#endif
		do {
			int curVert;

			curVert = g_clipIdxB[i];
			RenderClip_ClipPolyBottom(prevVert, curVert, vertBuf);
			prevVert = curVert;
			++i;
		} while (i < g_clipCountB);
	}
	bottomCount = g_clipCountA;

	i = 0;
	g_clipCountB = 0;
#ifndef XWA_MODERN
	prevVert = g_clipIdxA[bottomCount - 1];
#endif
	if (bottomCount > 0) {
#ifdef XWA_MODERN
		prevVert = g_clipIdxA[bottomCount - 1];
#endif
		do {
			int curVert;

			curVert = g_clipIdxA[i];
			RenderClip_ClipPolyLeft(prevVert, curVert, vertBuf);
			prevVert = curVert;
			++i;
		} while (i < g_clipCountA);
	}
	leftCount = g_clipCountB;

	i = 0;
	g_clipCountA = 0;
#ifndef XWA_MODERN
	prevVert = g_clipIdxB[leftCount - 1];
#endif
	if (leftCount > 0) {
#ifdef XWA_MODERN
		prevVert = g_clipIdxB[leftCount - 1];
#endif
		do {
			int curVert;

			curVert = g_clipIdxB[i];
			RenderClip_ClipPolyRight(prevVert, curVert, vertBuf);
			prevVert = curVert;
			++i;
		} while (i < g_clipCountB);
	}

	if (g_clipCountA >= 3) {
		int vertexCount;

		std3D_AddToTextureCache((Std3DTextureSurface*)texLevel->image);
		vertexCount = g_clipCountA;
		if (vertexCount + g_d3dVertexCount > g_maxBatchVerts ||
			vertexCount + g_d3dIndexCount > g_maxBatchTris) {
			std3D_LockExecuteBuffer();
			std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
			std3D_BeginInstructions();
			std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
			std3D_ExecuteBuffer();
			g_d3dIndexCount = 0;
			g_d3dVertexCount = 0;
			vertexCount = g_clipCountA;
		}

		if (g_capVertexAlpha) {
			uint32_t argbColor;

			argbColor = (uint32_t)texLevel->argbColor;
			if (argbColor > 0xff000000u) {
				texLevel->argbColor = (int32_t)(argbColor & 0xfeffffffu);
				vertexCount = g_clipCountA;
			}
			g_capVertexAlpha = 0;
		}

		for (i = 0; i < g_clipCountA; ++i) {
			int vertIdx;
			float screenY;
			float texU;
			float texV;
			float w;
			float depth;

			vertIdx = g_clipIdxA[i];
			screenY = vertBuf[vertIdx].sy;
			texU = vertBuf[vertIdx].tu;
			texV = vertBuf[vertIdx].tv;
			w = vertBuf[vertIdx].w;
			g_flightVertexBuffer[g_d3dVertexCount].sx = vertBuf[vertIdx].sx + g_flightVpOriginX;
			g_flightVertexBuffer[g_d3dVertexCount].sy = screenY + g_flightVpOriginY;
			depth = w * g_depthProjScale;
			depth = depth / (depth + g_projScale);
			if (depth < g_renderMinD3DDepth) {
				depth = g_renderMinD3DDepth;
			}
			if (g_std3DZCmpMode == 2) {
				depth = g_renderUnitFloat - depth;
			}

			g_flightVertexBuffer[g_d3dVertexCount].sz = depth;
			g_flightVertexBuffer[g_d3dVertexCount].rhw = w;
			g_flightVertexBuffer[g_d3dVertexCount].tu = texU;
			g_flightVertexBuffer[g_d3dVertexCount].tv = texV;
			g_flightVertexBuffer[g_d3dVertexCount].color = (uint32_t)texLevel->argbColor;
			g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
			g_clipIdxA[i] = g_d3dVertexCount++;
		}

		for (i = 2; i < g_clipCountA; ++i) {
			g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
			g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[i - 1];
			g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[i];
			g_triBuffer[g_d3dIndexCount].texture = &textureSurface->cacheNode;
			g_triBuffer[g_d3dIndexCount].flags = d3dFlags;
			++g_d3dIndexCount;
		}
	}
}

// FUNCTION: XWA 0x44A7B0
int RenderQuad_DrawRotatedSprite(FlightTexQuad* quad, TexLevel* texLevel, int d3dFlags) {
	Std3DTextureSurface* textureSurface;
	ProjVertex vertBuf[40];
	int screenX;
	int baseY;
	float projectedDepth;
	float uScale;
	float vScale;

	textureSurface = (Std3DTextureSurface*)texLevel->image;
	screenX = quad->screenX;
	baseY = g_flightVpHeight - quad->screenY;
	projectedDepth = g_depthProjScale / ((float)quad->depthZ + g_depthProjScale);
	uScale = 1.0f;
	vScale = 1.0f;
	if (projectedDepth < g_renderMinD3DDepth) {
		projectedDepth = g_renderMinD3DDepth;
	}
	if (g_std3DZCmpMode == 2) {
		projectedDepth = g_renderUnitFloat - projectedDepth;
	}

	{
		int angle;
		int halfWidth;
		int halfHeight;
		int cornerX;
		int cornerY;

		angle = quad->rotationAngle;
		halfWidth = (texLevel->width * quad->screenSize) >> 9;
		halfHeight = (texLevel->height * quad->screenSize) >> 9;

		cornerX = trig2_cosinedwordmult(halfWidth, angle) + trig2_sinedwordmult(halfHeight, angle);
		cornerY = trig2_cosinedwordmult(halfHeight, angle) - trig2_sinedwordmult(halfWidth, angle);

		g_clipCountA = 4;
		g_clipVertCursor = 4;
		g_clipIdxA[0] = 0;
		g_clipIdxA[1] = 1;
		g_clipIdxA[2] = 2;
		g_clipIdxA[3] = 3;

		if (g_pStd3DCurDevice->caps.bSquareOnlyTexture) {
			int squareWidth;
			int squareHeight;

			squareWidth = texLevel->width;
			squareHeight = texLevel->height;
			if (squareWidth > squareHeight) {
				do {
					vScale *= 0.5f;
					squareHeight *= 2;
				} while (squareHeight < squareWidth);
			} else if (squareWidth < squareHeight) {
				do {
					uScale *= 0.5f;
					squareWidth *= 2;
				} while (squareWidth < squareHeight);
			}
		}

		vertBuf[0].sx = (float)(screenX + cornerX);
		vertBuf[0].sy = (float)(baseY + cornerY);
		vertBuf[0].w = projectedDepth;
		vertBuf[0].litColor[0] = 1.0f;
		vertBuf[0].litColor[1] = 0.0f;
		vertBuf[0].litColor[2] = 0.0f;
		vertBuf[0].litColor[3] = 0.0f;
		vertBuf[0].tu = g_currentQuadTexCoords[0].u * uScale;
		vertBuf[0].tv = g_currentQuadTexCoords[0].v * vScale;
		vertBuf[0].extraLayerUVCount = 0;

		halfWidth = -halfWidth;
		vertBuf[1].sx = (float)(screenX + trig2_cosinedwordmult(halfWidth, angle) +
								trig2_sinedwordmult(halfHeight, angle));
		vertBuf[1].sy =
			(float)(baseY + trig2_cosinedwordmult(halfHeight, angle) - trig2_sinedwordmult(halfWidth, angle));
		vertBuf[1].w = projectedDepth;
		vertBuf[1].litColor[0] = 1.0f;
		vertBuf[1].litColor[1] = 0.0f;
		vertBuf[1].litColor[2] = 0.0f;
		vertBuf[1].litColor[3] = 0.0f;
		vertBuf[1].tu = g_currentQuadTexCoords[1].u * uScale;
		vertBuf[1].tv = g_currentQuadTexCoords[1].v * vScale;
		vertBuf[1].extraLayerUVCount = 0;

		halfHeight = -halfHeight;
		vertBuf[2].sx = (float)(screenX + trig2_cosinedwordmult(halfWidth, angle) +
								trig2_sinedwordmult(halfHeight, angle));
		vertBuf[2].sy =
			(float)(baseY + trig2_cosinedwordmult(halfHeight, angle) - trig2_sinedwordmult(halfWidth, angle));
		vertBuf[2].w = projectedDepth;
		vertBuf[2].litColor[0] = 1.0f;
		vertBuf[2].litColor[1] = 0.0f;
		vertBuf[2].litColor[2] = 0.0f;
		vertBuf[2].litColor[3] = 0.0f;
		vertBuf[2].tu = g_currentQuadTexCoords[2].u * uScale;
		vertBuf[2].tv = g_currentQuadTexCoords[2].v * vScale;
		vertBuf[2].extraLayerUVCount = 0;

		halfWidth = -halfWidth;
		vertBuf[3].sx = (float)(screenX + trig2_cosinedwordmult(halfWidth, angle) +
								trig2_sinedwordmult(halfHeight, angle));
		vertBuf[3].sy =
			(float)(baseY + trig2_cosinedwordmult(halfHeight, angle) - trig2_sinedwordmult(halfWidth, angle));
		vertBuf[3].w = projectedDepth;
		vertBuf[3].litColor[0] = 1.0f;
		vertBuf[3].litColor[1] = 0.0f;
		vertBuf[3].litColor[2] = 0.0f;
		vertBuf[3].litColor[3] = 0.0f;
		vertBuf[3].tu = g_currentQuadTexCoords[3].u * uScale;
		vertBuf[3].tv = g_currentQuadTexCoords[3].v * vScale;
		vertBuf[3].extraLayerUVCount = 0;
	}

	{
		int i;
		int clippedCount;

		g_clipCountB = 0;
		clippedCount = g_clipCountA;
		if (clippedCount > 0) {
			int prevVert;

			prevVert = g_clipIdxA[clippedCount - 1];
			for (i = 0; i < g_clipCountA; ++i) {
				int curVert;

				curVert = g_clipIdxA[i];
				RenderClip_ClipPolyTop(prevVert, curVert, vertBuf);
				prevVert = curVert;
			}
			clippedCount = g_clipCountB;
		}

		g_clipCountA = 0;
		if (clippedCount > 0) {
			int prevVert;

			prevVert = g_clipIdxB[clippedCount - 1];
			for (i = 0; i < g_clipCountB; ++i) {
				int curVert;

				curVert = g_clipIdxB[i];
				RenderClip_ClipPolyBottom(prevVert, curVert, vertBuf);
				prevVert = curVert;
			}
			clippedCount = g_clipCountA;
		}

		g_clipCountB = 0;
		if (clippedCount > 0) {
			int prevVert;

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

			prevVert = g_clipIdxB[clippedCount - 1];
			for (i = 0; i < g_clipCountB; ++i) {
				int curVert;

				curVert = g_clipIdxB[i];
				RenderClip_ClipPolyRight(prevVert, curVert, vertBuf);
				prevVert = curVert;
			}
			clippedCount = g_clipCountA;
		}
	}

	{
		int result;

		result = 0;
		if (g_clipCountA >= 3) {
			int i;
			int vertexCount;
			uint32_t argbColor;
			float rhw;

			std3D_AddToTextureCache(textureSurface);
			vertexCount = g_clipCountA;
			if (vertexCount + g_d3dVertexCount > g_maxBatchVerts ||
				vertexCount + g_d3dIndexCount > g_maxBatchTris) {
				std3D_LockExecuteBuffer();
				std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
				std3D_BeginInstructions();
				std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
				std3D_ExecuteBuffer();
				g_d3dIndexCount = 0;
				g_d3dVertexCount = 0;
				vertexCount = g_clipCountA;
			}

			if (g_capVertexAlpha) {
				argbColor = (uint32_t)texLevel->argbColor;
				if (argbColor > 0xff000000u) {
					texLevel->argbColor = (int32_t)(argbColor & 0xfeffffffu);
					vertexCount = g_clipCountA;
				}
				g_capVertexAlpha = 0;
			}

			if (g_flightConfPowerVr) {
				rhw = g_projScale / (float)quad->depthZ;
			} else {
				rhw = projectedDepth;
			}

			for (i = 0; i < vertexCount; ++i) {
				ProjVertex* vert;
				float sy;
				float w;
				float tu;
				float tv;

				vert = &vertBuf[g_clipIdxA[i]];
				sy = vert->sy;
				tu = vert->tu;
				tv = vert->tv;
				w = vert->w;
				g_flightVertexBuffer[g_d3dVertexCount].sx = vert->sx + g_flightVpOriginX;
				g_flightVertexBuffer[g_d3dVertexCount].sy = sy + g_flightVpOriginY;
				g_flightVertexBuffer[g_d3dVertexCount].sz = w;
				g_flightVertexBuffer[g_d3dVertexCount].rhw = rhw;
				g_flightVertexBuffer[g_d3dVertexCount].tu = tu;
				g_flightVertexBuffer[g_d3dVertexCount].tv = tv;
				g_flightVertexBuffer[g_d3dVertexCount].color = (uint32_t)texLevel->argbColor;
				g_flightVertexBuffer[g_d3dVertexCount].specular = 0;
				g_clipIdxA[i] = g_d3dVertexCount++;
			}

			result = 2;
			for (i = 2; i < vertexCount; ++i) {
				g_triBuffer[g_d3dIndexCount].v0 = g_clipIdxA[0];
				g_triBuffer[g_d3dIndexCount].v1 = g_clipIdxA[i - 1];
				g_triBuffer[g_d3dIndexCount].v2 = g_clipIdxA[i];
				g_triBuffer[g_d3dIndexCount].texture = &textureSurface->cacheNode;
				g_triBuffer[g_d3dIndexCount].flags = d3dFlags;
				result = i + 1;
				++g_d3dIndexCount;
			}
		}

		return result;
	}
}

// FUNCTION: XWA 0x44B590
void RenderQuad_DrawGlow(int* corners5, int depthZ, Std3DTextureSurface* tex, int edgeColor,
						 int centerColor) {
	Std3DTexCacheNode* cacheNode;
	ProjVertex projected[5];
	ProjVertex triangle[50];
	float depth;
	float vertexDepth;
	float edgeAlpha;
	float edgeRed;
	float edgeGreen;
	float edgeBlue;
	int nearFlag;

	cacheNode = &tex->cacheNode;
	std3D_AddToTextureCache(tex);

	depth = g_depthProjScale / ((float)depthZ + g_depthProjScale);
	if (depth < g_renderMinD3DDepth) {
		depth = g_renderMinD3DDepth;
	}
	if (g_std3DZCmpMode == 2) {
		depth = g_renderUnitFloat - depth;
	}

	nearFlag = 0;

	if (corners5[2] < 1) {
		vertexDepth = (float)corners5[2] - g_renderUnitFloat;
		projected[0].sx = (float)corners5[0];
		projected[0].sy = (float)corners5[1];
		nearFlag = 1;
	} else {
		projected[0].sx = (float)TRANSFM2_ProjectScreenX(corners5[0], corners5[2]);
		projected[0].sy = (float)TRANSFM2_ProjectScreenY(corners5[1], corners5[2]);
		vertexDepth = g_depthProjScale / ((float)corners5[2] + g_depthProjScale);
		if (vertexDepth < g_renderMinD3DDepth) {
			vertexDepth = g_renderMinD3DDepth;
		}
		if (g_std3DZCmpMode == 2) {
			vertexDepth = g_renderUnitFloat - vertexDepth;
		}
	}

	projected[0].w = vertexDepth;
	projected[0].litColor[0] = (float)((uint32_t)(centerColor & 0xff000000u)) * g_argbAlphaToUnitScale;
	projected[0].litColor[1] = (float)((uint32_t)(centerColor & 0x00ff0000u)) * g_argbRedToUnitScale;
	projected[0].litColor[2] = (float)((uint32_t)(centerColor & 0x0000ff00u)) * g_argbGreenToUnitScale;
	projected[0].litColor[3] = (float)((uint32_t)(centerColor & 0x000000ffu)) * g_argbBlueToUnitScale;
	projected[0].tu = 0.5f;
	projected[0].tv = 0.5f;
	projected[0].extraLayerUVCount = 0;

	edgeAlpha = (float)((uint32_t)(edgeColor & 0xff000000u)) * g_argbAlphaToUnitScale;
	edgeRed = (float)((uint32_t)(edgeColor & 0x00ff0000u)) * g_argbRedToUnitScale;
	edgeGreen = (float)((uint32_t)(edgeColor & 0x0000ff00u)) * g_argbGreenToUnitScale;
	edgeBlue = (float)((uint32_t)(edgeColor & 0x000000ffu)) * g_argbBlueToUnitScale;

	if (corners5[5] < 1) {
		vertexDepth = (float)corners5[5] - g_renderUnitFloat;
		projected[1].sx = (float)corners5[3];
		projected[1].sy = (float)corners5[4];
		nearFlag = 1;
	} else {
		projected[1].sx = (float)TRANSFM2_ProjectScreenX(corners5[3], corners5[5]);
		projected[1].sy = (float)TRANSFM2_ProjectScreenY(corners5[4], corners5[5]);
		vertexDepth = g_depthProjScale / ((float)corners5[5] + g_depthProjScale);
		if (vertexDepth < g_renderMinD3DDepth) {
			vertexDepth = g_renderMinD3DDepth;
		}
		if (g_std3DZCmpMode == 2) {
			vertexDepth = g_renderUnitFloat - vertexDepth;
		}
	}
	projected[1].w = vertexDepth;
	projected[1].litColor[0] = edgeAlpha;
	projected[1].litColor[1] = edgeRed;
	projected[1].litColor[2] = edgeGreen;
	projected[1].litColor[3] = edgeBlue;
	projected[1].tu = 0.0f;
	projected[1].tv = 0.0f;
	projected[1].extraLayerUVCount = 0;

	if (corners5[8] < 1) {
		vertexDepth = (float)corners5[8] - g_renderUnitFloat;
		projected[2].sx = (float)corners5[6];
		projected[2].sy = (float)corners5[7];
		nearFlag = 1;
	} else {
		projected[2].sx = (float)TRANSFM2_ProjectScreenX(corners5[6], corners5[8]);
		projected[2].sy = (float)TRANSFM2_ProjectScreenY(corners5[7], corners5[8]);
		vertexDepth = g_depthProjScale / ((float)corners5[8] + g_depthProjScale);
		if (vertexDepth < g_renderMinD3DDepth) {
			vertexDepth = g_renderMinD3DDepth;
		}
		if (g_std3DZCmpMode == 2) {
			vertexDepth = g_renderUnitFloat - vertexDepth;
		}
	}
	projected[2].w = vertexDepth;
	projected[2].litColor[0] = edgeAlpha;
	projected[2].litColor[1] = edgeRed;
	projected[2].litColor[2] = edgeGreen;
	projected[2].litColor[3] = edgeBlue;
	projected[2].tu = 1.0f;
	projected[2].tv = 0.0f;
	projected[2].extraLayerUVCount = 0;

	if (corners5[11] < 1) {
		vertexDepth = (float)corners5[11] - g_renderUnitFloat;
		projected[3].sx = (float)corners5[9];
		projected[3].sy = (float)corners5[10];
		nearFlag = 1;
	} else {
		projected[3].sx = (float)TRANSFM2_ProjectScreenX(corners5[9], corners5[11]);
		projected[3].sy = (float)TRANSFM2_ProjectScreenY(corners5[10], corners5[11]);
		vertexDepth = g_depthProjScale / ((float)corners5[11] + g_depthProjScale);
		if (vertexDepth < g_renderMinD3DDepth) {
			vertexDepth = g_renderMinD3DDepth;
		}
		if (g_std3DZCmpMode == 2) {
			vertexDepth = g_renderUnitFloat - vertexDepth;
		}
	}
	projected[3].w = vertexDepth;
	projected[3].litColor[0] = edgeAlpha;
	projected[3].litColor[1] = edgeRed;
	projected[3].litColor[2] = edgeGreen;
	projected[3].litColor[3] = edgeBlue;
	projected[3].tu = 1.0f;
	projected[3].tv = 1.0f;
	projected[3].extraLayerUVCount = 0;

	if (corners5[14] < 1) {
		vertexDepth = (float)corners5[14] - g_renderUnitFloat;
		projected[4].sx = (float)corners5[12];
		projected[4].sy = (float)corners5[13];
		nearFlag = 1;
	} else {
		projected[4].sx = (float)TRANSFM2_ProjectScreenX(corners5[12], corners5[14]);
		projected[4].sy = (float)TRANSFM2_ProjectScreenY(corners5[13], corners5[14]);
		vertexDepth = g_depthProjScale / ((float)corners5[14] + g_depthProjScale);
		if (vertexDepth < g_renderMinD3DDepth) {
			vertexDepth = g_renderMinD3DDepth;
		}
		if (g_std3DZCmpMode == 2) {
			vertexDepth = g_renderUnitFloat - vertexDepth;
		}
	}
	projected[4].w = vertexDepth;
	projected[4].litColor[0] = edgeAlpha;
	projected[4].litColor[1] = edgeRed;
	projected[4].litColor[2] = edgeGreen;
	projected[4].litColor[3] = edgeBlue;
	projected[4].tu = 0.0f;
	projected[4].tv = 1.0f;
	projected[4].extraLayerUVCount = 0;

	g_clipCountA = 3;
	g_clipVertCursor = 3;
	g_clipIdxA[0] = 0;
	g_clipIdxA[1] = 1;
	g_clipIdxA[2] = 2;
	triangle[0] = projected[0];
	triangle[1] = projected[2];
	triangle[2] = projected[1];
	RenderQuad_SubmitClippedTriangle(triangle, cacheNode, depth, nearFlag);

	g_clipCountA = 3;
	g_clipVertCursor = 3;
	g_clipIdxA[0] = 0;
	g_clipIdxA[1] = 1;
	g_clipIdxA[2] = 2;
	triangle[0] = projected[0];
	triangle[1] = projected[3];
	triangle[2] = projected[2];
	RenderQuad_SubmitClippedTriangle(triangle, cacheNode, depth, nearFlag);

	g_clipCountA = 3;
	g_clipVertCursor = 3;
	g_clipIdxA[0] = 0;
	g_clipIdxA[1] = 1;
	g_clipIdxA[2] = 2;
	triangle[0] = projected[0];
	triangle[1] = projected[4];
	triangle[2] = projected[3];
	RenderQuad_SubmitClippedTriangle(triangle, cacheNode, depth, nearFlag);

	g_clipCountA = 3;
	g_clipVertCursor = 3;
	g_clipIdxA[0] = 0;
	g_clipIdxA[1] = 1;
	g_clipIdxA[2] = 2;
	triangle[0] = projected[0];
	triangle[1] = projected[1];
	triangle[2] = projected[4];
	RenderQuad_SubmitClippedTriangle(triangle, cacheNode, depth, nearFlag);
}

// FUNCTION: XWA 0x42BD80
char RenderQuad_DrawModelTexture(ObjectTypeId modelType, FlightTexQuad* quad, int argbColor) {
	uint8_t assetFlags;
	int d3dFlags;

	assetFlags = g_modelTypeTable[modelType].assetFlags;
	if (g_modelTypeTable[modelType].curTexLevel == NULL ||
		(assetFlags & (MODEL_TYPE_ASSET_TEXTURE_DRAW | MODEL_TYPE_ASSET_TEXTURE_READY)) == 0) {
		return (char)assetFlags;
	}

	if (g_useHardware3D) {
		TexLevel* curTexLevel;

		curTexLevel = g_modelTypeTable[modelType].curTexLevel;
		d3dFlags = 0x8612;
		if (g_bilinearEnabled &&
			((modelType != OBJ_HudTextureGroup12000 && modelType != OBJ_MapIconTextureGroup14800 &&
			  modelType != OBJ_HullIconTextureGroup26000) ||
			 g_flightHudScaleFactor != g_renderQuadUnitFloat)) {
			d3dFlags = 0x8792;
		}
		if ((g_modelTypeTable[modelType].flags & MODEL_TYPE_FLAG_SINGLE_MIP_LEVEL) == 0) {
			d3dFlags += 0x800;
		}

		curTexLevel->argbColor = argbColor;
		if (modelType >= OBJ_ExplosionTextureGroup2000 && modelType <= OBJ_ExplosionTextureGroup2005) {
			int maxTexDim;
			int maxTexDimQ8;
			int depthZ;

			if (curTexLevel->width > curTexLevel->height) {
				maxTexDim = curTexLevel->width;
			} else {
				maxTexDim = curTexLevel->height;
			}
			maxTexDimQ8 = maxTexDim << 8;
			depthZ = quad->depthZ;
			if (maxTexDimQ8 < depthZ) {
				quad->depthZ = depthZ - maxTexDimQ8;
			}
		}

		return (char)RenderQuad_DrawRotatedSprite(quad, curTexLevel, d3dFlags);
	} else {
		Sprite* sprite;

		g_flightSwRotSpriteSpanRunsEnabled = 1;
		sprite = g_modelTypeTable[modelType].curTexLevel->image;
		FlightSw_PrepareSpriteRotationTables(quad->rotationAngle, 2);
		FlightSw_BuildSpriteTintRemapTables(sprite);
		return FlightSw_DrawRotatedSpriteQuad((int16_t)quad->screenX, (int16_t)quad->screenY,
											  (int16_t)quad->screenSize, sprite);
	}
}
