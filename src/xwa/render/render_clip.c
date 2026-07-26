#include "xwa/render/renderer_internal.h"

extern const float g_renderZeroFloat;
extern const float g_renderUnitFloat;

// GLOBAL: XWA 0x5A94F8
const float g_renderNegUnitFloat = -1.0f;

static float RenderClip_AbsFloat(float value) {
	if (value < g_renderZeroFloat) {
		return -value;
	}
	return value;
}

static __inline void RenderClip_WriteIntersection(ProjVertex* dst, const ProjVertex* prev,
												  const ProjVertex* cur, float t, int baseIsCur,
												  int subtractDelta) {
	float deltaSx;
	float deltaSy;
	float deltaW;
	float deltaColor0;
	float deltaColor1;
	float deltaColor2;
	float deltaColor3;
	float deltaTu;
	float deltaTv;
	float wDeltaAbs;
	int attrIdx;

	deltaSx = cur->sx - prev->sx;
	deltaSy = cur->sy - prev->sy;
	deltaW = cur->w - prev->w;
	deltaColor0 = cur->litColor[0] - prev->litColor[0];
	deltaColor1 = cur->litColor[1] - prev->litColor[1];
	deltaColor2 = cur->litColor[2] - prev->litColor[2];
	deltaColor3 = cur->litColor[3] - prev->litColor[3];
	deltaTu = cur->tu - prev->tu;
	deltaTv = cur->tv - prev->tv;

	if (baseIsCur) {
		if (subtractDelta) {
			dst->sx = cur->sx - deltaSx * t;
			dst->sy = cur->sy - deltaSy * t;
			dst->w = cur->w - deltaW * t;
			dst->litColor[0] = cur->litColor[0] - deltaColor0 * t;
			dst->litColor[1] = cur->litColor[1] - deltaColor1 * t;
			dst->litColor[2] = cur->litColor[2] - deltaColor2 * t;
			dst->litColor[3] = cur->litColor[3] - deltaColor3 * t;
		} else {
			dst->sx = deltaSx * t + cur->sx;
			dst->sy = deltaSy * t + cur->sy;
			dst->w = deltaW * t + cur->w;
			dst->litColor[0] = deltaColor0 * t + cur->litColor[0];
			dst->litColor[1] = deltaColor1 * t + cur->litColor[1];
			dst->litColor[2] = deltaColor2 * t + cur->litColor[2];
			dst->litColor[3] = deltaColor3 * t + cur->litColor[3];
		}
	} else {
		if (subtractDelta) {
			dst->sx = prev->sx - deltaSx * t;
			dst->sy = prev->sy - deltaSy * t;
			dst->w = prev->w - deltaW * t;
			dst->litColor[0] = prev->litColor[0] - deltaColor0 * t;
			dst->litColor[1] = prev->litColor[1] - deltaColor1 * t;
			dst->litColor[2] = prev->litColor[2] - deltaColor2 * t;
			dst->litColor[3] = prev->litColor[3] - deltaColor3 * t;
		} else {
			dst->sx = deltaSx * t + prev->sx;
			dst->sy = deltaSy * t + prev->sy;
			dst->w = deltaW * t + prev->w;
			dst->litColor[0] = deltaColor0 * t + prev->litColor[0];
			dst->litColor[1] = deltaColor1 * t + prev->litColor[1];
			dst->litColor[2] = deltaColor2 * t + prev->litColor[2];
			dst->litColor[3] = deltaColor3 * t + prev->litColor[3];
		}
	}

	dst->extraLayerUVCount = prev->extraLayerUVCount;
	wDeltaAbs = RenderClip_AbsFloat(deltaW);

	if (wDeltaAbs < 0.0000099999997f) {
		if (baseIsCur) {
			if (subtractDelta) {
				dst->tu = cur->tu - deltaTu * t;
				dst->tv = cur->tv - deltaTv * t;
				for (attrIdx = 0; attrIdx < dst->extraLayerUVCount; ++attrIdx) {
					dst->extraLayerUVs[attrIdx].u =
						cur->extraLayerUVs[attrIdx].u -
						(cur->extraLayerUVs[attrIdx].u - prev->extraLayerUVs[attrIdx].u) * t;
					dst->extraLayerUVs[attrIdx].v =
						cur->extraLayerUVs[attrIdx].v -
						(cur->extraLayerUVs[attrIdx].v - prev->extraLayerUVs[attrIdx].v) * t;
				}
			} else {
				dst->tu = deltaTu * t + cur->tu;
				dst->tv = deltaTv * t + cur->tv;
				for (attrIdx = 0; attrIdx < dst->extraLayerUVCount; ++attrIdx) {
					dst->extraLayerUVs[attrIdx].u =
						(cur->extraLayerUVs[attrIdx].u - prev->extraLayerUVs[attrIdx].u) * t +
						cur->extraLayerUVs[attrIdx].u;
					dst->extraLayerUVs[attrIdx].v =
						(cur->extraLayerUVs[attrIdx].v - prev->extraLayerUVs[attrIdx].v) * t +
						cur->extraLayerUVs[attrIdx].v;
				}
			}
		} else {
			if (subtractDelta) {
				dst->tu = prev->tu - deltaTu * t;
				dst->tv = prev->tv - deltaTv * t;
				for (attrIdx = 0; attrIdx < dst->extraLayerUVCount; ++attrIdx) {
					dst->extraLayerUVs[attrIdx].u =
						prev->extraLayerUVs[attrIdx].u -
						(cur->extraLayerUVs[attrIdx].u - prev->extraLayerUVs[attrIdx].u) * t;
					dst->extraLayerUVs[attrIdx].v =
						prev->extraLayerUVs[attrIdx].v -
						(cur->extraLayerUVs[attrIdx].v - prev->extraLayerUVs[attrIdx].v) * t;
				}
			} else {
				dst->tu = deltaTu * t + prev->tu;
				dst->tv = deltaTv * t + prev->tv;
				for (attrIdx = 0; attrIdx < dst->extraLayerUVCount; ++attrIdx) {
					dst->extraLayerUVs[attrIdx].u =
						(cur->extraLayerUVs[attrIdx].u - prev->extraLayerUVs[attrIdx].u) * t +
						prev->extraLayerUVs[attrIdx].u;
					dst->extraLayerUVs[attrIdx].v =
						(cur->extraLayerUVs[attrIdx].v - prev->extraLayerUVs[attrIdx].v) * t +
						prev->extraLayerUVs[attrIdx].v;
				}
			}
		}
	} else if (baseIsCur) {
		float prevInvW;
		float curInvW;
		float uvT;

		prevInvW = g_projScale / prev->w;
		curInvW = g_projScale / cur->w;
		uvT = (g_projScale / dst->w - curInvW) / (prevInvW - curInvW);
		dst->tu = cur->tu - deltaTu * uvT;
		dst->tv = cur->tv - deltaTv * uvT;
		for (attrIdx = 0; attrIdx < dst->extraLayerUVCount; ++attrIdx) {
			dst->extraLayerUVs[attrIdx].u =
				cur->extraLayerUVs[attrIdx].u -
				(cur->extraLayerUVs[attrIdx].u - prev->extraLayerUVs[attrIdx].u) * uvT;
			dst->extraLayerUVs[attrIdx].v =
				cur->extraLayerUVs[attrIdx].v -
				(cur->extraLayerUVs[attrIdx].v - prev->extraLayerUVs[attrIdx].v) * uvT;
		}
	} else {
		float prevInvW;
		float curInvW;
		float uvT;

		curInvW = g_projScale / cur->w;
		prevInvW = g_projScale / prev->w;
		uvT = (g_projScale / dst->w - prevInvW) / (curInvW - prevInvW);
		dst->tu = deltaTu * uvT + prev->tu;
		dst->tv = deltaTv * uvT + prev->tv;
		for (attrIdx = 0; attrIdx < dst->extraLayerUVCount; ++attrIdx) {
			dst->extraLayerUVs[attrIdx].u =
				(cur->extraLayerUVs[attrIdx].u - prev->extraLayerUVs[attrIdx].u) * uvT +
				prev->extraLayerUVs[attrIdx].u;
			dst->extraLayerUVs[attrIdx].v =
				(cur->extraLayerUVs[attrIdx].v - prev->extraLayerUVs[attrIdx].v) * uvT +
				prev->extraLayerUVs[attrIdx].v;
		}
	}
}

// FUNCTION: XWA 0x444030
void RenderClip_ClipPolyTop(int prevVert, int curVert, ProjVertex* vertBuf) {
	ProjVertex* prev;
	ProjVertex* cur;
	float prevSy;
	float curSy;
	float deltaSy;
	int outIdx;

	prev = &vertBuf[prevVert];
	cur = &vertBuf[curVert];
	prevSy = prev->sy;
	curSy = cur->sy;
	deltaSy = curSy - prevSy;

	if (prevSy < 0.0f) {
		if (curSy < 0.0f) {
			return;
		}

		g_clipOccurred = 1;
		outIdx = g_clipVertCursor++;
		if (-prevSy >= curSy) {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, curSy / deltaSy, 1, 1);
		} else {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, -prevSy / deltaSy, 0, 0);
		}

		vertBuf[outIdx].sy = 0.0f;
		g_clipIdxB[g_clipCountB++] = outIdx;
		g_clipIdxB[g_clipCountB++] = curVert;
		return;
	}

	if (curSy < 0.0f) {
		g_clipOccurred = 1;
		outIdx = g_clipVertCursor++;
		if (prevSy >= -curSy) {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, -curSy / deltaSy, 1, 0);
		} else {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, prevSy / deltaSy, 0, 1);
		}

		vertBuf[outIdx].sy = 0.0f;
		g_clipIdxB[g_clipCountB++] = outIdx;
		return;
	}

	g_clipIdxB[g_clipCountB++] = curVert;
}

// FUNCTION: XWA 0x444DF0
void RenderClip_ClipPolyBottom(int prevVert, int curVert, ProjVertex* vertBuf) {
	ProjVertex* prev;
	ProjVertex* cur;
	float prevSy;
	float curSy;
	float maxY;
	float deltaSy;
	int outIdx;

	prev = &vertBuf[prevVert];
	cur = &vertBuf[curVert];
	prevSy = prev->sy;
	curSy = cur->sy;
	maxY = (float)g_flightVpMaxY;
	deltaSy = curSy - prevSy;

	if (prevSy > maxY) {
		if (curSy > maxY) {
			return;
		}

		g_clipOccurred = 1;
		outIdx = g_clipVertCursor++;
		if (prevSy - maxY >= maxY - curSy) {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, (maxY - curSy) / deltaSy, 1, 0);
		} else {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, (prevSy - maxY) / deltaSy, 0, 1);
		}

		vertBuf[outIdx].sy = maxY;
		g_clipIdxA[g_clipCountA++] = outIdx;
		g_clipIdxA[g_clipCountA++] = curVert;
		return;
	}

	if (curSy > maxY) {
		g_clipOccurred = 1;
		outIdx = g_clipVertCursor++;
		if (maxY - prevSy >= curSy - maxY) {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, (curSy - maxY) / deltaSy, 1, 1);
		} else {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, (maxY - prevSy) / deltaSy, 0, 0);
		}

		vertBuf[outIdx].sy = maxY;
		g_clipIdxA[g_clipCountA++] = outIdx;
		return;
	}

	g_clipIdxA[g_clipCountA++] = curVert;
}

// FUNCTION: XWA 0x445C00
void RenderClip_ClipPolyLeft(int prevVert, int curVert, ProjVertex* vertBuf) {
	ProjVertex* prev;
	ProjVertex* cur;
	float prevSx;
	float curSx;
	float deltaSx;
	int outIdx;

	prev = &vertBuf[prevVert];
	cur = &vertBuf[curVert];
	prevSx = prev->sx;
	curSx = cur->sx;
	deltaSx = curSx - prevSx;

	if (prevSx < 0.0f) {
		if (curSx < 0.0f) {
			return;
		}

		g_clipOccurred = 1;
		outIdx = g_clipVertCursor++;
		if (-prevSx >= curSx) {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, curSx / deltaSx, 1, 1);
		} else {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, -prevSx / deltaSx, 0, 0);
		}

		vertBuf[outIdx].sx = 0.0f;
		g_clipIdxB[g_clipCountB++] = outIdx;
		g_clipIdxB[g_clipCountB++] = curVert;
		return;
	}

	if (curSx < 0.0f) {
		g_clipOccurred = 1;
		outIdx = g_clipVertCursor++;
		if (prevSx >= -curSx) {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, -curSx / deltaSx, 1, 0);
		} else {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, prevSx / deltaSx, 0, 1);
		}

		vertBuf[outIdx].sx = 0.0f;
		g_clipIdxB[g_clipCountB++] = outIdx;
		return;
	}

	g_clipIdxB[g_clipCountB++] = curVert;
}

// FUNCTION: XWA 0x4469D0
void RenderClip_ClipPolyRight(int prevVert, int curVert, ProjVertex* vertBuf) {
	ProjVertex* prev;
	ProjVertex* cur;
	float prevSx;
	float curSx;
	float maxX;
	float deltaSx;
	int outIdx;

	prev = &vertBuf[prevVert];
	cur = &vertBuf[curVert];
	prevSx = prev->sx;
	curSx = cur->sx;
	maxX = (float)g_flightVpMaxX;
	deltaSx = curSx - prevSx;

	if (prevSx > maxX) {
		if (curSx > maxX) {
			return;
		}

		g_clipOccurred = 1;
		outIdx = g_clipVertCursor++;
		if (prevSx - maxX >= maxX - curSx) {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, (maxX - curSx) / deltaSx, 1, 0);
		} else {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, (prevSx - maxX) / deltaSx, 0, 1);
		}

		vertBuf[outIdx].sx = maxX;
		g_clipIdxA[g_clipCountA++] = outIdx;
		g_clipIdxA[g_clipCountA++] = curVert;
		return;
	}

	if (curSx > maxX) {
		g_clipOccurred = 1;
		outIdx = g_clipVertCursor++;
		if (maxX - prevSx >= curSx - maxX) {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, (curSx - maxX) / deltaSx, 1, 1);
		} else {
			RenderClip_WriteIntersection(&vertBuf[outIdx], prev, cur, (maxX - prevSx) / deltaSx, 0, 0);
		}

		vertBuf[outIdx].sx = maxX;
		g_clipIdxA[g_clipCountA++] = outIdx;
		return;
	}

	g_clipIdxA[g_clipCountA++] = curVert;
}

// FUNCTION: XWA 0x4477E0
void RenderClip_ClipPolyNear(int prevVert, int curVert, ProjVertex* vertBuf) {
	int extraLayerUVCount;
	int outIdx;
	int attrIdx;
	ProjVertex* prev;
	ProjVertex* cur;
	OptTexCoord* prevUv;
	OptTexCoord* curUv;
	OptTexCoord* outUv;
	double curTv;

	extraLayerUVCount = vertBuf[prevVert].extraLayerUVCount;
	prev = &vertBuf[prevVert];
	g_clipPrevW = prev->w;
	g_clipCurW = vertBuf[curVert].w;
	cur = &vertBuf[curVert];

	if (g_clipPrevW < g_renderZeroFloat) {
		if (g_clipCurW < g_renderZeroFloat) {
			return;
		}

		g_clipOccurred = 1;
		outIdx = g_clipVertCursor++;

		g_clipCurW = g_projScale / g_clipCurW;
		g_clipPrevSx = prev->sx;
		g_clipCurSx = cur->sx;
		g_clipPrevSy = prev->sy;
		g_clipCurSy = cur->sy;
		g_clipPrevTu = prev->tu;
		g_clipCurTu = cur->tu;
		g_clipPrevTv = prev->tv;
		curTv = cur->tv;

		g_clipCurSx = (g_clipCurSx - g_flightVpCenterXf) * g_clipCurW;
		g_clipCurSy = (g_clipCurSy - (g_projOffsetYf + g_flightVpCenterYf)) * g_clipCurW;
		g_clipCurSx = g_clipCurSx * g_invProjScale;
		g_clipCurSy = g_clipCurSy * g_invProjScale;
		g_clipCurTv = (float)curTv;
		g_clipDeltaSx = g_clipCurSx - g_clipPrevSx;
		g_clipDeltaSy = g_clipCurSy - g_clipPrevSy;
		g_clipNearDenom = g_clipCurW - g_clipPrevW - g_renderUnitFloat;

		g_clipPrevColor0 = prev->litColor[0];
		g_clipCurColor0 = cur->litColor[0];
		g_clipPrevColor1 = prev->litColor[1];
		g_clipCurColor1 = cur->litColor[1];
		g_clipPrevColor2 = prev->litColor[2];
		g_clipCurColor2 = cur->litColor[2];
		g_clipPrevColor3 = prev->litColor[3];
		g_clipCurColor3 = cur->litColor[3];

		g_clipDeltaColor0 = g_clipCurColor0 - g_clipPrevColor0;
		g_clipDeltaColor1 = g_clipCurColor1 - g_clipPrevColor1;
		g_clipDeltaColor2 = g_clipCurColor2 - g_clipPrevColor2;
		g_clipDeltaColor3 = g_clipCurColor3 - g_clipPrevColor3;
		g_clipDeltaTu = g_clipCurTu - g_clipPrevTu;
		g_clipPrevW = g_clipPrevW / g_clipNearDenom;
		g_clipDeltaTv = g_clipCurTv - g_clipPrevTv;

		vertBuf[outIdx].litColor[0] = g_clipPrevColor0 - g_clipPrevW * g_clipDeltaColor0;
		vertBuf[outIdx].litColor[1] = g_clipPrevColor1 - g_clipPrevW * g_clipDeltaColor1;
		vertBuf[outIdx].litColor[3] = g_clipPrevColor2 - g_clipPrevW * g_clipDeltaColor2;
		vertBuf[outIdx].litColor[2] = g_clipPrevColor3 - g_clipPrevW * g_clipDeltaColor3;
		vertBuf[outIdx].sx = g_clipPrevSx - g_clipPrevW * g_clipDeltaSx;
		vertBuf[outIdx].sy = g_clipPrevSy - g_clipPrevW * g_clipDeltaSy;
		vertBuf[outIdx].tu = g_clipPrevTu - g_clipDeltaTu * g_clipPrevW;
		vertBuf[outIdx].tv = g_clipPrevTv - g_clipDeltaTv * g_clipPrevW;
		vertBuf[outIdx].extraLayerUVCount = prev->extraLayerUVCount;

		prevUv = prev->extraLayerUVs;
		curUv = cur->extraLayerUVs;
		outUv = vertBuf[outIdx].extraLayerUVs;

		for (attrIdx = 0; attrIdx < extraLayerUVCount; ++attrIdx) {
			float delta0;

			g_clipAttrPrev0[attrIdx] = prevUv->u;
			g_clipAttrCur0[attrIdx] = curUv->u;
			delta0 = g_clipAttrCur0[attrIdx] - g_clipAttrPrev0[attrIdx];
			g_clipAttrPrev1[attrIdx] = prevUv->v;
			g_clipAttrCur1[attrIdx] = curUv->v;
			g_clipAttrDelta0[attrIdx] = delta0;
			g_clipAttrDelta1[attrIdx] = g_clipAttrCur1[attrIdx] - g_clipAttrPrev1[attrIdx];
			outUv->u = g_clipAttrPrev0[attrIdx] - g_clipPrevW * delta0;
			outUv->v = g_clipAttrPrev1[attrIdx] - g_clipPrevW * g_clipAttrDelta1[attrIdx];
			++prevUv;
			++curUv;
			++outUv;
		}

		g_clipPrevW = g_projScale;
		vertBuf[outIdx].w = g_clipPrevW;
		vertBuf[outIdx].sx = g_clipPrevW * vertBuf[outIdx].sx;
		vertBuf[outIdx].sy = g_clipPrevW * vertBuf[outIdx].sy;
		vertBuf[outIdx].sx = g_flightVpCenterXf + vertBuf[outIdx].sx;
		vertBuf[outIdx].sy = g_projOffsetYf + g_flightVpCenterYf + vertBuf[outIdx].sy;
		g_clipIdxA[g_clipCountA++] = outIdx;
		g_clipIdxA[g_clipCountA++] = curVert;
		return;
	}

	if (g_clipCurW < g_renderZeroFloat) {
		g_clipOccurred = 1;
		outIdx = g_clipVertCursor++;

		g_clipPrevW = g_projScale / g_clipPrevW;
		g_clipPrevSx = prev->sx;
		g_clipCurSx = cur->sx;
		g_clipPrevSy = prev->sy;
		g_clipCurSy = cur->sy;
		g_clipPrevTu = prev->tu;
		g_clipCurTu = cur->tu;
		g_clipPrevTv = prev->tv;
		curTv = cur->tv;

		g_clipPrevSx = (g_clipPrevSx - (float)g_flightVpCenterX) * g_clipPrevW;
		g_clipPrevSy = (g_clipPrevSy - ((float)g_flightVpCenterY + g_projOffsetYf)) * g_clipPrevW;
		g_clipPrevSx = g_clipPrevSx * g_invProjScale;
		g_clipPrevSy = g_clipPrevSy * g_invProjScale;
		g_clipCurTv = (float)curTv;
		g_clipDeltaSx = g_clipCurSx - g_clipPrevSx;
		g_clipDeltaSy = g_clipCurSy - g_clipPrevSy;
		g_clipNearDenom = g_clipCurW - g_clipPrevW - g_renderNegUnitFloat;

		g_clipPrevColor0 = prev->litColor[0];
		g_clipCurColor0 = cur->litColor[0];
		g_clipPrevColor1 = prev->litColor[1];
		g_clipCurColor1 = cur->litColor[1];
		g_clipPrevColor2 = prev->litColor[2];
		g_clipCurColor2 = cur->litColor[2];
		g_clipPrevColor3 = prev->litColor[3];
		g_clipCurColor3 = cur->litColor[3];

		g_clipDeltaColor0 = g_clipCurColor0 - g_clipPrevColor0;
		g_clipDeltaColor1 = g_clipCurColor1 - g_clipPrevColor1;
		g_clipDeltaColor2 = g_clipCurColor2 - g_clipPrevColor2;
		g_clipCurW = g_clipCurW / g_clipNearDenom;
		g_clipDeltaColor3 = g_clipCurColor3 - g_clipPrevColor3;
		g_clipDeltaTu = g_clipCurTu - g_clipPrevTu;
		g_clipDeltaTv = g_clipCurTv - g_clipPrevTv;

		vertBuf[outIdx].litColor[0] = g_clipCurColor0 - g_clipCurW * g_clipDeltaColor0;
		vertBuf[outIdx].litColor[1] = g_clipCurColor1 - g_clipCurW * g_clipDeltaColor1;
		vertBuf[outIdx].litColor[2] = g_clipCurColor2 - g_clipCurW * g_clipDeltaColor2;
		vertBuf[outIdx].litColor[3] = g_clipCurColor3 - g_clipCurW * g_clipDeltaColor3;
		vertBuf[outIdx].sx = g_clipCurSx - g_clipCurW * g_clipDeltaSx;
		vertBuf[outIdx].sy = g_clipCurSy - g_clipCurW * g_clipDeltaSy;
		vertBuf[outIdx].tu = g_clipCurTu - g_clipDeltaTu * g_clipCurW;
		vertBuf[outIdx].tv = g_clipCurTv - g_clipDeltaTv * g_clipCurW;
		vertBuf[outIdx].extraLayerUVCount = prev->extraLayerUVCount;

		prevUv = prev->extraLayerUVs;
		curUv = cur->extraLayerUVs;
		outUv = vertBuf[outIdx].extraLayerUVs;

		for (attrIdx = 0; attrIdx < extraLayerUVCount; ++attrIdx) {
			float delta0;

			g_clipAttrPrev0[attrIdx] = prevUv->u;
			g_clipAttrCur0[attrIdx] = curUv->u;
			delta0 = g_clipAttrCur0[attrIdx] - g_clipAttrPrev0[attrIdx];
			g_clipAttrPrev1[attrIdx] = prevUv->v;
			g_clipAttrCur1[attrIdx] = curUv->v;
			g_clipAttrDelta0[attrIdx] = delta0;
			g_clipAttrDelta1[attrIdx] = g_clipAttrCur1[attrIdx] - g_clipAttrPrev1[attrIdx];
			outUv->u = g_clipAttrCur0[attrIdx] - g_clipCurW * delta0;
			outUv->v = g_clipAttrCur1[attrIdx] - g_clipCurW * g_clipAttrDelta1[attrIdx];
			++prevUv;
			++curUv;
			++outUv;
		}

		g_clipCurW = g_projScale;
		vertBuf[outIdx].w = g_clipCurW;
		vertBuf[outIdx].sx = g_clipCurW * vertBuf[outIdx].sx;
		vertBuf[outIdx].sy = g_clipCurW * vertBuf[outIdx].sy;
		vertBuf[outIdx].sx = (float)g_flightVpCenterX + vertBuf[outIdx].sx;
		vertBuf[outIdx].sy = (float)(g_projOffsetY + g_flightVpCenterY) + vertBuf[outIdx].sy;
		g_clipIdxA[g_clipCountA++] = outIdx;
		return;
	}

	g_clipIdxA[g_clipCountA++] = curVert;
}
