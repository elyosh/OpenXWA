#include "xwa/frontend/frontend_draw.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif

#include <string.h>

#include "xwa/frontend/frontend_display.h"

// GLOBAL: XWA 0x9F60D4
unsigned char* g_drawSurfacePtr = 0;
// GLOBAL: XWA 0x9F6FFA
int g_drawSurfacePitch = 0;
// GLOBAL: XWA 0x7830A8
int g_savedDrawSurfacePitch = 0;
// GLOBAL: XWA 0x9F708A
int g_clipMinX = 0;
// GLOBAL: XWA 0x9F708E
int g_clipMaxX = 639;
// GLOBAL: XWA 0x9F7092
int g_clipMinY = 0;
// GLOBAL: XWA 0x9F7096
int g_clipMaxY = 479;
// GLOBAL: XWA 0x9F79AA
FrontendRect g_dirtyRects[FRONTEND_DRAW_MAX_DIRTY_RECTS];
// GLOBAL: XWA 0x9F7AEA
int g_dirtyRectCount = 0;
// GLOBAL: XWA 0x9F7AEE
int g_dirtyRectOverflow = 0;

static __inline int FrontendDraw_Shl12(int value) { return value * 4096; }

static __inline int FrontendDraw_Sar12(int value) { return value >> 12; }

static __inline void FrontendDraw_WritePixel(unsigned char* dst, int pixelShift, int color) {
	switch (pixelShift) {
		case 0:
			*dst = (unsigned char)color;
			break;

		case 1:
			*(unsigned short*)dst = (unsigned short)color;
			break;
	}
}

static __inline void FrontendDraw_FillRun(unsigned char* dst, int pixelShift, int count, int color) {
	if (pixelShift) {
		if (pixelShift == 1 && count > 0) {
			unsigned short* wordDst;

			wordDst = (unsigned short*)dst;
			while (count) {
				*wordDst = (unsigned short)color;
				++wordDst;
				--count;
			}
		}
	} else {
		while (count) {
			*dst = (unsigned char)color;
			++dst;
			--count;
		}
	}
}

static __inline void FrontendDraw_FillLineRun(unsigned char* dst, int pixelShift, int count, int color) {
	if (pixelShift) {
		if (pixelShift == 1 && count > 0) {
			unsigned short* wordDst;

			wordDst = (unsigned short*)dst;
			do {
				*wordDst++ = (unsigned short)color;
				--count;
			} while (count);
		}
	} else {
		while (count) {
			*dst = (unsigned char)color;
			++dst;
			--count;
		}
	}
}

static __inline unsigned int FrontendDraw_ReadU16(const unsigned char* ptr) {
	return *(const unsigned short*)ptr;
}

static __inline void FrontendDraw_WriteU16(unsigned char* ptr, unsigned int value) {
	*(unsigned short*)ptr = (unsigned short)value;
}

static __inline void FrontendDraw_FillAARun(unsigned char* dst, int pixelShift, int count,
											unsigned int color) {
	switch (pixelShift) {
		case 1:
			if (g_pixelFormat555) {
				FrontendDraw_WriteU16(dst - 2, ((color >> 1) & 0x3defu) +
												   ((FrontendDraw_ReadU16(dst - 2) >> 1) & 0x3defu));
			} else {
				FrontendDraw_WriteU16(dst - 2, ((color >> 1) & 0x7befu) +
												   ((FrontendDraw_ReadU16(dst - 2) >> 1) & 0x7befu));
			}
			FrontendDraw_FillRun(dst, pixelShift, count, (int)color);
			if (g_pixelFormat555) {
				FrontendDraw_WriteU16(dst + 2 * count,
									  ((color >> 1) & 0x3defu) +
										  ((FrontendDraw_ReadU16(dst + 2 * count) >> 1) & 0x3defu));
			} else {
				FrontendDraw_WriteU16(dst + 2 * count,
									  ((color >> 1) & 0x7befu) +
										  ((FrontendDraw_ReadU16(dst + 2 * count) >> 1) & 0x7befu));
			}
			break;

		case 0:
			while (count) {
				*dst = (unsigned char)color;
				++dst;
				--count;
			}
			break;
	}
}

// FUNCTION: XWA 0x558C90
void FrontendDraw_RectAssign(FrontendRect* rc, int left, int top, int right, int bottom) {
	rc->left = left;
	rc->top = top;
	rc->right = right;
	rc->bottom = bottom;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x558CB0
void FrontendDraw_RectCopy(FrontendRect* dst, const FrontendRect* src) {
	dst->left = src->left;
	dst->top = src->top;
	dst->right = src->right;
	dst->bottom = src->bottom;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x558CD0
void FrontendDraw_RectOffsetXY(FrontendRect* rect, int dx, int dy) {
	rect->left += dx;
	rect->right += dx;
	rect->top += dy;
	rect->bottom += dy;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x558CF0
void FrontendDraw_RectInsetXY(FrontendRect* rect, int dx, int dy) {
	rect->left += dx;
	rect->right -= dx;
	rect->top += dy;
	rect->bottom -= dy;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x558D10
int FrontendDraw_RectClipToBounds(FrontendRect* rect) {
	int left;
	int clipMinX;
	int result;
	int right;
	int clipMaxX;
	int top;
	int clipMinY;
	int clipMaxY;

	left = rect->left;
	clipMinX = g_clipMinX;
	result = 0;
	if (rect->left < g_clipMinX) {
		right = rect->right;
		left = g_clipMinX;
		rect->left = g_clipMinX;
		if (right < clipMinX) {
			rect->right = clipMinX - 1;
		}
		result = 1;
	}

	clipMaxX = g_clipMaxX;
	if (rect->right > g_clipMaxX) {
		rect->right = g_clipMaxX;
		if (left > clipMaxX) {
			rect->left = clipMaxX + 1;
		}
		result |= 4u;
	}

	top = rect->top;
	clipMinY = g_clipMinY;
	if (top < g_clipMinY) {
		top = g_clipMinY;
		rect->top = g_clipMinY;
		if (rect->bottom < clipMinY) {
			rect->bottom = top - 1;
		}
		result |= 2u;
	}

	clipMaxY = g_clipMaxY;
	if (rect->bottom > g_clipMaxY) {
		rect->bottom = g_clipMaxY;
		if (top > clipMaxY) {
			rect->top = clipMaxY + 1;
		}
		return result | 8;
	}

	return result;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5592E0
int FrontendDraw_PointInRect(FrontendRect* rect, int x, int y) {
	return x >= rect->left && x <= rect->right && y >= rect->top && y <= rect->bottom;
}

// FUNCTION: XWA 0x5593B0
int FrontendDraw_ForceFullScreenPresent(void) {
	g_dirtyRectOverflow = 1;
	return 1;
}

// FUNCTION: XWA 0x559310
int FrontendDraw_AddDirtyRect(FrontendRect* rect) {
	FrontendRect dst;

	if ((unsigned int)g_dirtyRectCount >= FRONTEND_DRAW_MAX_DIRTY_RECTS) {
		g_dirtyRectOverflow = 1;
		return 0;
	}

	FrontendDraw_RectCopy(&dst, rect);
	FrontendDraw_RectClipToBounds(&dst);
	++dst.right;
	++dst.bottom;
	dst.left &= 0xfffffffe;
	++dst.right;
	dst.right &= 0xfffffffe;
	dst.top &= 0xfffffffe;
	++dst.bottom;
	dst.bottom &= 0xfffffffe;
	FrontendDraw_RectCopy(&g_dirtyRects[g_dirtyRectCount], &dst);
	++g_dirtyRectCount;
	return 1;
}

// FUNCTION: XWA 0x5420D0
int FrontendDraw_BeginExternalSurface(unsigned char* pixels, int pitch) {
#ifdef XWA_MODERN
	/* Remaster snapshot observer: subsequent 2D records route to the
	 * external/scratch surface (briefing-map source etc.). */
	XwaSnapshot_SetEmitTarget(XWA_EMIT_TARGET_EXTERNAL);
#endif
	FrontendDisplay_UnlockBackBuffer();
	g_drawSurfacePtr = pixels;
	g_savedDrawSurfacePitch = g_drawSurfacePitch;
	g_drawSurfacePitch = pitch;
	return 1;
}

// FUNCTION: XWA 0x542100
int FrontendDraw_EndExternalSurface(void) {
#ifdef XWA_MODERN
	XwaSnapshot_SetEmitTarget(XWA_EMIT_TARGET_MAIN);
#endif
	g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	g_drawSurfacePitch = g_savedDrawSurfacePitch;
	return 1;
}

// FUNCTION: XWA 0x543490
void FrontendDraw_HorizontalLineClipped(int x0, int x1, int y, int color) {
#ifdef XWA_MODERN
	/* Remaster snapshot observer. */
	XwaSnapshot_EmitPaint(XWA_PAINT_HLINE, x0, y, x1, y, 0, 0, (uint32_t)color);
#endif

	int pixelShift;
	unsigned char* row;
	unsigned char* dst;
	int count;
	int wordCount;

	pixelShift = g_displayBpp >> 4;
	if (y < g_clipMinY || y > g_clipMaxY) {
		return;
	}

	if (x0 <= x1) {
		if (x1 > g_clipMaxX) {
			x1 = g_clipMaxX;
			if (x1 < x0) {
				return;
			}
		}

		if (x0 < g_clipMinX) {
			x0 = g_clipMinX;
			if (x1 < x0) {
				return;
			}
		}

		row = &g_drawSurfacePtr[g_drawSurfacePitch * y];
		dst = &row[x0 << pixelShift];
		switch (pixelShift) {
			case 0:
				count = x1;
				count -= x0;
				goto fillBytes;

			case 1:
				count = x1 - x0 + 1;
				color = (unsigned short)color;
				if (count <= 0) {
					return;
				}

				wordCount = count;
				do {
					dst[0] = (unsigned char)color;
					dst[1] = (unsigned char)(color >> 8);
					dst += 2;
					--wordCount;
				} while (wordCount);
		}
		return;
	}

	if (x1 < g_clipMinX) {
		x1 = g_clipMinX;
		if (x1 >= x0) {
			return;
		}
	}

	if (x0 > g_clipMaxX) {
		x0 = g_clipMaxX;
	}

	if (x1 >= x0) {
		return;
	}

	row = &g_drawSurfacePtr[g_drawSurfacePitch * y];
	dst = &row[x1 << pixelShift];
	switch (pixelShift) {
		case 0:
			count = x0;
			count -= x1;
			goto fillBytes;

		case 1:
			count = x0 - x1 + 1;
			color = (unsigned short)color;
			if (count <= 0) {
				return;
			}

			wordCount = count;
			do {
				dst[0] = (unsigned char)color;
				dst[1] = (unsigned char)(color >> 8);
				dst += 2;
				--wordCount;
			} while (wordCount);
	}
	return;

fillBytes:
	++count;
	color = (unsigned short)color;
	memset(dst, color, count);
}

// FUNCTION: XWA 0x5435E0
void FrontendDraw_VerticalLineClipped(int y0, int y1, int x, int color) {
#ifdef XWA_MODERN
	/* Remaster snapshot observer. */
	XwaSnapshot_EmitPaint(XWA_PAINT_VLINE, x, y0, x, y1, 0, 0, (uint32_t)color);
#endif

	int pixelShift;
	unsigned char* dst;
	int count;
	int startY;
	int endY;

	if (x < g_clipMinX || x > g_clipMaxX) {
		return;
	}

	startY = y0;
	endY = y1;
	pixelShift = g_displayBpp >> 4;
	if (startY <= endY) {
		if (endY > g_clipMaxY) {
			endY = g_clipMaxY;
			if (startY > endY) {
				return;
			}
		}

		if (startY < g_clipMinY) {
			startY = g_clipMinY;
			if (startY > endY) {
				return;
			}
		}

		dst = &g_drawSurfacePtr[g_drawSurfacePitch * startY + (x << pixelShift)];
		switch (pixelShift) {
			case 0:
				if (startY > endY) {
					return;
				}

				count = endY - startY + 1;
				do {
					*dst = (unsigned char)color;
					dst += g_drawSurfacePitch;
					--count;
				} while (count);
				return;

			case 1:
				if (startY > endY) {
					return;
				}

				count = endY - startY + 1;
				do {
					*(unsigned short*)dst = (unsigned short)color;
					dst += 2 * (g_drawSurfacePitch >> 1);
					--count;
				} while (count);
				return;
		}
		return;
	}

	if (endY < g_clipMinY) {
		endY = g_clipMinY;
		if (startY <= endY) {
			return;
		}
	}

	if (startY > g_clipMaxY) {
		startY = g_clipMaxY;
		if (startY <= endY) {
			return;
		}
	}

	dst = &g_drawSurfacePtr[g_drawSurfacePitch * endY + (x << pixelShift)];
	switch (pixelShift) {
		case 0:
			if (endY > startY) {
				return;
			}

			count = startY - endY + 1;
			do {
				*dst = (unsigned char)color;
				dst += g_drawSurfacePitch;
				--count;
			} while (count);
			return;

		case 1:
			if (endY > startY) {
				return;
			}

			count = startY - endY + 1;
			do {
				*(unsigned short*)dst = (unsigned short)color;
				dst += 2 * (g_drawSurfacePitch >> 1);
				--count;
			} while (count);
			return;
	}
}

// FUNCTION: XWA 0x542C90
void FrontendDraw_Line(int x0, int y0, int x1, int y1, int color) {
#ifdef XWA_MODERN
	/* Remaster snapshot observer. */
	XwaSnapshot_EmitPaint(XWA_PAINT_LINE, x0, y0, x1, y1, 0, 0, (uint32_t)color);
#endif

	int x;
	int yStart;
	int yEnd;
	int endX;
	int oldY;
	int error;
	int run;
	unsigned char* dst;
	unsigned char clipped;

	yStart = y0;
	yEnd = y1;
	error = 2048;
	clipped = 0;
	if (y1 == y0) {
		FrontendDraw_HorizontalLineClipped(x0, x1, y0, color);
		return;
	}

	x = x0;
	if (x0 == x1) {
		FrontendDraw_VerticalLineClipped(y0, y1, x0, color);
		return;
	}

	y0 = g_displayBpp >> 4;
	if (yStart < y1 && x0 < x1) {
		int fracStep;
		int rows;

		x0 = FrontendDraw_Shl12(x1 - x0 + 1) / (y1 - yStart + 1);
		if (x < g_clipMinX) {
			yStart += FrontendDraw_Shl12(g_clipMinX - x) / x0;
			x = g_clipMinX;
			clipped = 1;
		}
		if (yStart < g_clipMinY) {
			x += FrontendDraw_Sar12(x0 * (g_clipMinY - yStart));
			yStart = g_clipMinY;
			clipped = 1;
		}
		if (x1 <= g_clipMaxX) {
			endX = x1;
		} else {
			yEnd += FrontendDraw_Shl12(g_clipMaxX - x1) / x0;
			endX = g_clipMaxX;
			clipped = 1;
		}
		if (yEnd > g_clipMaxY) {
			oldY = yEnd;
			yEnd = g_clipMaxY;
			endX -= FrontendDraw_Sar12(x0 * (oldY - yEnd));
			clipped = 1;
		}
		if (clipped) {
			if (endX < x || yEnd < yStart) {
				return;
			}
			x0 = FrontendDraw_Shl12(endX - x + 1) / (yEnd - yStart + 1);
		}
		fracStep = x0 & 0xfff;
		x0 = FrontendDraw_Sar12(x0);
		dst = &g_drawSurfacePtr[g_drawSurfacePitch * yStart + (x << y0)];
		rows = yEnd - yStart + 1;
		if (x0 != 0) {
			while (rows > 0) {
				run = x0;
				error += fracStep;
				if (error >= 4096) {
					run = x0 + 1;
					error -= 4096;
				}
				FrontendDraw_FillLineRun(dst, y0, run, color);
				dst += g_drawSurfacePitch + (run << y0);
				--rows;
			}
		} else {
			error = 2048;
			while (rows > 0) {
				FrontendDraw_WritePixel(dst, y0, color);
				error += fracStep;
				if (error >= 4096) {
					error -= 4096;
					dst += y0 + 1;
				}
				dst += g_drawSurfacePitch;
				--rows;
			}
		}
		return;
	}

	if (yStart < y1 && x1 < x0) {
		int fracStep;
		int rows;

		x0 = FrontendDraw_Shl12(x0 - x1 + 1) / (y1 - yStart + 1);
		if (x > g_clipMaxX) {
			yStart += FrontendDraw_Shl12(x - g_clipMaxX) / x0;
			x = g_clipMaxX;
			clipped = 1;
		}
		if (yStart < g_clipMinY) {
			x -= FrontendDraw_Sar12(x0 * (g_clipMinY - yStart));
			yStart = g_clipMinY;
			clipped = 1;
		}
		if (x1 < g_clipMinX) {
			yEnd += FrontendDraw_Shl12(x1 - g_clipMinX) / x0;
			endX = g_clipMinX;
			x1 = g_clipMinX;
			clipped = 1;
		} else {
			endX = x1;
		}
		if (yEnd > g_clipMaxY) {
			oldY = yEnd;
			yEnd = g_clipMaxY;
			endX += FrontendDraw_Sar12(x0 * (oldY - yEnd));
			x1 = endX;
			clipped = 1;
		}
		if (clipped) {
			if (x < endX || yEnd < yStart) {
				return;
			}
			x0 = FrontendDraw_Shl12(x - endX + 1) / (yEnd - yStart + 1);
		}
		fracStep = x0 & 0xfff;
		x0 = FrontendDraw_Sar12(x0);
		dst = &g_drawSurfacePtr[g_drawSurfacePitch * yEnd + (x1 << y0)];
		rows = yEnd - yStart + 1;
		if (x0 != 0) {
			while (rows > 0) {
				run = x0;
				error += fracStep;
				if (error >= 4096) {
					run = x0 + 1;
					error -= 4096;
				}
				FrontendDraw_FillLineRun(dst, y0, run, color);
				dst += (run << y0) - g_drawSurfacePitch;
				--rows;
			}
		} else {
			error = 2048;
			while (rows > 0) {
				FrontendDraw_WritePixel(dst, y0, color);
				error += fracStep;
				if (error >= 4096) {
					error -= 4096;
					dst += y0 + 1;
				}
				dst -= g_drawSurfacePitch;
				--rows;
			}
		}
		return;
	}

	if (y1 < yStart && x0 < x1) {
		int fracStep;
		int rows;

		x0 = FrontendDraw_Shl12(x1 - x0 + 1) / (yStart - y1 + 1);
		if (x < g_clipMinX) {
			yStart += FrontendDraw_Shl12(x - g_clipMinX) / x0;
			x = g_clipMinX;
			clipped = 1;
		}
		if (yStart > g_clipMaxY) {
			x += FrontendDraw_Sar12(x0 * (yStart - g_clipMaxY));
			yStart = g_clipMaxY;
			clipped = 1;
		}
		if (x1 <= g_clipMaxX) {
			endX = x1;
		} else {
			yEnd += FrontendDraw_Shl12(x1 - g_clipMaxX) / x0;
			endX = g_clipMaxX;
			clipped = 1;
		}
		if (yEnd < g_clipMinY) {
			endX -= FrontendDraw_Sar12(x0 * (g_clipMinY - yEnd));
			yEnd = g_clipMinY;
			clipped = 1;
		}
		if (clipped) {
			if (endX < x || yStart < yEnd) {
				return;
			}
			x0 = FrontendDraw_Shl12(endX - x + 1) / (yStart - yEnd + 1);
		}
		fracStep = x0 & 0xfff;
		x0 = FrontendDraw_Sar12(x0);
		dst = &g_drawSurfacePtr[g_drawSurfacePitch * yStart + (x << y0)];
		rows = yStart - yEnd + 1;
		if (x0 != 0) {
			while (rows > 0) {
				run = x0;
				error += fracStep;
				if (error >= 4096) {
					run = x0 + 1;
					error -= 4096;
				}
				FrontendDraw_FillLineRun(dst, y0, run, color);
				dst += (run << y0) - g_drawSurfacePitch;
				--rows;
			}
		} else {
			error = 2048;
			while (rows > 0) {
				FrontendDraw_WritePixel(dst, y0, color);
				error += fracStep;
				if (error >= 4096) {
					error -= 4096;
					dst += y0 + 1;
				}
				dst -= g_drawSurfacePitch;
				--rows;
			}
		}
		return;
	}

	{
		int fracStep;
		int rows;

		x0 = FrontendDraw_Shl12(x0 - x1 + 1) / (yStart - y1 + 1);
		if (x > g_clipMaxX) {
			yStart += FrontendDraw_Shl12(g_clipMaxX - x) / x0;
			x = g_clipMaxX;
			clipped = 1;
		}
		if (yStart > g_clipMaxY) {
			x -= FrontendDraw_Sar12(x0 * (yStart - g_clipMaxY));
			yStart = g_clipMaxY;
			clipped = 1;
		}
		if (x1 < g_clipMinX) {
			yEnd += FrontendDraw_Shl12(g_clipMinX - x1) / x0;
			endX = g_clipMinX;
			x1 = g_clipMinX;
			clipped = 1;
		} else {
			endX = x1;
		}
		if (yEnd < g_clipMinY) {
			endX += FrontendDraw_Sar12(x0 * (g_clipMinY - yEnd));
			yEnd = g_clipMinY;
			x1 = endX;
			clipped = 1;
		}
		if (clipped) {
			if (x < endX || yStart < yEnd) {
				return;
			}
			x0 = FrontendDraw_Shl12(x - endX + 1) / (yStart - yEnd + 1);
		}
		fracStep = x0 & 0xfff;
		x0 = FrontendDraw_Sar12(x0);
		dst = &g_drawSurfacePtr[g_drawSurfacePitch * yEnd + (x1 << y0)];
		rows = yStart - yEnd + 1;
		if (x0 != 0) {
			while (rows > 0) {
				run = x0;
				error += fracStep;
				if (error >= 4096) {
					run = x0 + 1;
					error -= 4096;
				}
				FrontendDraw_FillLineRun(dst, y0, run, color);
				dst += g_drawSurfacePitch + (run << y0);
				--rows;
			}
		} else {
			error = 2048;
			while (rows > 0) {
				FrontendDraw_WritePixel(dst, y0, color);
				error += fracStep;
				if (error >= 4096) {
					error -= 4096;
					dst += y0 + 1;
				}
				dst += g_drawSurfacePitch;
				--rows;
			}
		}
	}
}

// FUNCTION: XWA 0x542120
void FrontendDraw_LineAntialiased(int x0, int y0, int x1, int y1, unsigned int color) {
#ifdef XWA_MODERN
	/* Remaster snapshot observer. */
	XwaSnapshot_EmitPaint(XWA_PAINT_LINE_AA, x0, y0, x1, y1, 0, 0, color);
#endif

	int x;
	int yStart;
	int yEnd;
	int pixelShift;
	int step;
	unsigned char clipped;
	int endX;
	int oldY;
	int fracStep;
	unsigned char* dst;
	int rows;
	int error;
	int run;

	yStart = y0;
	yEnd = y1;
	error = 2048;
	clipped = 0;
	if (y0 == y1) {
		FrontendDraw_HorizontalLineClipped(x0, x1, y0, (int)color);
		return;
	}

	if (x0 == x1) {
		FrontendDraw_VerticalLineClipped(y0, y1, x0, (int)color);
		return;
	}

	x = x0;
	pixelShift = g_displayBpp >> 4;
	if (y1 > yStart && x0 < x1) {
		step = FrontendDraw_Shl12(x1 - x0 + 1) / (y1 - yStart + 1);
		x0 = step;
		if (x < g_clipMinX) {
			yStart += FrontendDraw_Shl12(g_clipMinX - x) / x0;
			x = g_clipMinX;
			clipped = 1;
		}
		if (yStart < g_clipMinY) {
			x += FrontendDraw_Sar12(x0 * (g_clipMinY - yStart));
			yStart = g_clipMinY;
			clipped = 1;
		}
		if (x1 > g_clipMaxX) {
			yEnd += FrontendDraw_Shl12(g_clipMaxX - x1) / x0;
			x1 = g_clipMaxX;
			clipped = 1;
		}
		endX = x1;
		if (yEnd > g_clipMaxY) {
			oldY = yEnd;
			yEnd = g_clipMaxY;
			endX = x1 - FrontendDraw_Sar12(x0 * (oldY - g_clipMaxY));
			clipped = 1;
		}
		if (clipped) {
			if (endX < x || yEnd < yStart) {
				return;
			}
			x0 = FrontendDraw_Shl12(endX - x + 1) / (yEnd - yStart + 1);
		}
		fracStep = x0 & 0xfff;
		x0 = FrontendDraw_Sar12(x0);
		dst = &g_drawSurfacePtr[g_drawSurfacePitch * yStart + (x << pixelShift)];
		rows = yEnd - yStart + 1;
		if (x0 != 0) {
			while (rows > 0) {
				run = x0;
				error += fracStep;
				if (error >= 4096) {
					run = x0 + 1;
					error -= 4096;
				}
				FrontendDraw_FillAARun(dst, pixelShift, run, color);
				dst += g_drawSurfacePitch + (run << pixelShift);
				--rows;
			}
		} else {
			error = 2048;
			while (rows > 0) {
				switch (pixelShift) {
					case 1:
						*(unsigned short*)dst = (unsigned short)color;
						if (g_pixelFormat555) {
							*(unsigned short*)(dst + 2) =
								(unsigned short)(((color >> 1) & 0x3defu) +
												 ((*(unsigned short*)(dst + 2) >> 1) & 0x3defu));
						} else {
							*(unsigned short*)(dst + 2) =
								(unsigned short)(((color >> 1) & 0x7befu) +
												 ((*(unsigned short*)(dst + 2) >> 1) & 0x7befu));
						}
						break;

					case 0:
						*dst = (unsigned char)color;
						break;
				}
				error += fracStep;
				if (error >= 4096) {
					error -= 4096;
					dst += pixelShift + 1;
				}
				dst += g_drawSurfacePitch;
				--rows;
			}
		}
		return;
	}

	if (y1 > yStart && x1 < x0) {
		step = FrontendDraw_Shl12(x0 - x1 + 1) / (y1 - yStart + 1);
		x0 = step;
		if (x > g_clipMaxX) {
			yStart += FrontendDraw_Shl12(x - g_clipMaxX) / x0;
			x = g_clipMaxX;
			clipped = 1;
		}
		if (yStart < g_clipMinY) {
			x -= FrontendDraw_Sar12(x0 * (g_clipMinY - yStart));
			yStart = g_clipMinY;
			clipped = 1;
		}
		endX = x1;
		if (x1 < g_clipMinX) {
			yEnd += FrontendDraw_Shl12(x1 - g_clipMinX) / x0;
			x1 = g_clipMinX;
			endX = g_clipMinX;
			clipped = 1;
		}
		if (yEnd > g_clipMaxY) {
			endX += FrontendDraw_Sar12(x0 * (yEnd - g_clipMaxY));
			yEnd = g_clipMaxY;
			x1 = endX;
			clipped = 1;
		}
		if (clipped) {
			if (x < endX || yEnd < yStart) {
				return;
			}
			x0 = FrontendDraw_Shl12(x - endX + 1) / (yEnd - yStart + 1);
		}
		fracStep = x0 & 0xfff;
		x0 = FrontendDraw_Sar12(x0);
		dst = &g_drawSurfacePtr[g_drawSurfacePitch * yEnd + (x1 << pixelShift)];
		rows = yEnd - yStart + 1;
		if (x0 != 0) {
			while (rows > 0) {
				run = x0;
				error += fracStep;
				if (error >= 4096) {
					run = x0 + 1;
					error -= 4096;
				}
				FrontendDraw_FillAARun(dst, pixelShift, run, color);
				dst += (run << pixelShift) - g_drawSurfacePitch;
				--rows;
			}
		} else {
			error = 2048;
			while (rows > 0) {
				switch (pixelShift) {
					case 1:
						*(unsigned short*)dst = (unsigned short)color;
						if (g_pixelFormat555) {
							*(unsigned short*)(dst + 2) =
								(unsigned short)(((color >> 1) & 0x3defu) +
												 ((*(unsigned short*)(dst + 2) >> 1) & 0x3defu));
						} else {
							*(unsigned short*)(dst + 2) =
								(unsigned short)(((color >> 1) & 0x7befu) +
												 ((*(unsigned short*)(dst + 2) >> 1) & 0x7befu));
						}
						break;

					case 0:
						*dst = (unsigned char)color;
						break;
				}
				error += fracStep;
				if (error >= 4096) {
					error -= 4096;
					dst += pixelShift + 1;
				}
				dst -= g_drawSurfacePitch;
				--rows;
			}
		}
		return;
	}

	if (yEnd < yStart && x0 < x1) {
		step = FrontendDraw_Shl12(x1 - x0 + 1) / (yStart - y1 + 1);
		if (x < g_clipMinX) {
			yStart += FrontendDraw_Shl12(x - g_clipMinX) / step;
			x = g_clipMinX;
			clipped = 1;
		}
		if (yStart > g_clipMaxY) {
			x += FrontendDraw_Sar12(step * (yStart - g_clipMaxY));
			yStart = g_clipMaxY;
			clipped = 1;
		}
		if (x1 > g_clipMaxX) {
			yEnd += FrontendDraw_Shl12(x1 - g_clipMaxX) / step;
			x1 = g_clipMaxX;
			clipped = 1;
		}
		endX = x1;
		if (yEnd < g_clipMinY) {
			oldY = g_clipMinY - yEnd;
			yEnd = g_clipMinY;
			endX = x1 - FrontendDraw_Sar12(step * oldY);
			clipped = 1;
		}
		if (clipped) {
			if (endX < x || yStart < yEnd) {
				return;
			}
			step = FrontendDraw_Shl12(endX - x + 1) / (yStart - yEnd + 1);
		}
		fracStep = step & 0xfff;
		x0 = FrontendDraw_Sar12(step);
		dst = &g_drawSurfacePtr[g_drawSurfacePitch * yStart + (x << pixelShift)];
		rows = yStart - yEnd + 1;
		if (x0 != 0) {
			while (rows > 0) {
				run = x0;
				error += fracStep;
				if (error >= 4096) {
					run = x0 + 1;
					error -= 4096;
				}
				FrontendDraw_FillAARun(dst, pixelShift, run, color);
				dst += (run << pixelShift) - g_drawSurfacePitch;
				--rows;
			}
		} else {
			error = 2048;
			while (rows > 0) {
				switch (pixelShift) {
					case 1:
						*(unsigned short*)dst = (unsigned short)color;
						if (g_pixelFormat555) {
							*(unsigned short*)(dst + 2) =
								(unsigned short)(((color >> 1) & 0x3defu) +
												 ((*(unsigned short*)(dst + 2) >> 1) & 0x3defu));
						} else {
							*(unsigned short*)(dst + 2) =
								(unsigned short)(((color >> 1) & 0x7befu) +
												 ((*(unsigned short*)(dst + 2) >> 1) & 0x7befu));
						}
						break;

					case 0:
						*dst = (unsigned char)color;
						break;
				}
				error += fracStep;
				if (error >= 4096) {
					error -= 4096;
					dst += pixelShift + 1;
				}
				dst -= g_drawSurfacePitch;
				--rows;
			}
		}
	} else {
		step = FrontendDraw_Shl12(x0 - x1 + 1) / (yStart - y1 + 1);
		x0 = step;
		if (x > g_clipMaxX) {
			yStart += FrontendDraw_Shl12(g_clipMaxX - x) / x0;
			x = g_clipMaxX;
			clipped = 1;
		}
		if (yStart > g_clipMaxY) {
			x -= FrontendDraw_Sar12(x0 * (yStart - g_clipMaxY));
			yStart = g_clipMaxY;
			clipped = 1;
		}
		endX = x1;
		if (x1 < g_clipMinX) {
			yEnd += FrontendDraw_Shl12(g_clipMinX - x1) / x0;
			endX = g_clipMinX;
			x1 = g_clipMinX;
			clipped = 1;
		}
		if (yEnd < g_clipMinY) {
			oldY = g_clipMinY - yEnd;
			yEnd = g_clipMinY;
			endX += FrontendDraw_Sar12(x0 * oldY);
			x1 = endX;
			clipped = 1;
		}
		if (clipped) {
			if (x < endX || yStart < yEnd) {
				return;
			}
			x0 = FrontendDraw_Shl12(x - endX + 1) / (yStart - yEnd + 1);
		}
		fracStep = x0 & 0xfff;
		x0 = FrontendDraw_Sar12(x0);
		dst = &g_drawSurfacePtr[g_drawSurfacePitch * yEnd + (x1 << pixelShift)];
		rows = yStart - yEnd + 1;
		if (x0 != 0) {
			while (rows > 0) {
				run = x0;
				error += fracStep;
				if (error >= 4096) {
					run = x0 + 1;
					error -= 4096;
				}
				FrontendDraw_FillAARun(dst, pixelShift, run, color);
				dst += g_drawSurfacePitch + (run << pixelShift);
				--rows;
			}
		} else {
			error = 2048;
			while (rows > 0) {
				switch (pixelShift) {
					case 1:
						*(unsigned short*)dst = (unsigned short)color;
						if (g_pixelFormat555) {
							*(unsigned short*)(dst + 2) =
								(unsigned short)(((color >> 1) & 0x3defu) +
												 ((*(unsigned short*)(dst + 2) >> 1) & 0x3defu));
						} else {
							*(unsigned short*)(dst + 2) =
								(unsigned short)(((color >> 1) & 0x7befu) +
												 ((*(unsigned short*)(dst + 2) >> 1) & 0x7befu));
						}
						break;

					case 0:
						*dst = (unsigned char)color;
						break;
				}
				error += fracStep;
				if (error >= 4096) {
					error -= 4096;
					dst += pixelShift + 1;
				}
				dst += g_drawSurfacePitch;
				--rows;
			}
		}
	}
}

// FUNCTION: XWA 0x558D90
void FrontendDraw_FillRectTranslucent(FrontendRect* rect, int dx, int dy, unsigned int color) {
#ifdef XWA_MODERN
	/* Remaster snapshot observer. */
	XwaSnapshot_EmitPaint(XWA_PAINT_FILL_TRANSLUCENT, rect->left, rect->top, rect->right, rect->bottom, dx,
						  dy, color);
#endif

	FrontendRect dst;
	int width;
	int pitch;
	int bottomEnd;
	int rows;
	int col;
	unsigned int halfColor;
	unsigned int blend;
	unsigned char* ptr;
	unsigned char* pixel;

	if (rect->right > rect->left && rect->bottom > rect->top) {
		FrontendDraw_RectCopy(&dst, rect);
		FrontendDraw_RectOffsetXY(&dst, dx, dy);
		if (dst.left <= 640 && dst.right >= 0 && dst.top <= 480 && dst.bottom >= 0) {
			FrontendDraw_RectClipToBounds(&dst);
			bottomEnd = dst.bottom + 1;
			width = dst.right - dst.left + 1;
			pitch = g_drawSurfacePitch;
			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					ptr = &g_drawSurfacePtr[2 * dst.left + dst.top * pitch];
					if (g_pixelFormat555) {
						halfColor = (color >> 1) & 0x3defu;
						if (dst.top < bottomEnd) {
							rows = bottomEnd - dst.top;
							do {
								if (width > 0) {
									pixel = ptr;
									col = width;
									do {
										blend = ((FrontendDraw_ReadU16(pixel) >> 1) & 0x3defu) + halfColor;
										FrontendDraw_WriteU16(pixel, blend);
										pixel += 2;
										--col;
									} while (col);
								}
								ptr += 2 * (pitch >> 1);
								--rows;
							} while (rows);
						}
					} else {
						halfColor = (color >> 1) & 0x7befu;
						if (dst.top < bottomEnd) {
							rows = bottomEnd - dst.top;
							do {
								if (width > 0) {
									pixel = ptr;
									col = width;
									do {
										blend = ((FrontendDraw_ReadU16(pixel) >> 1) & 0x7befu) + halfColor;
										FrontendDraw_WriteU16(pixel, blend);
										pixel += 2;
										--col;
									} while (col);
								}
								ptr += 2 * (pitch >> 1);
								--rows;
							} while (rows);
						}
					}
				}
			} else {
				ptr = &g_drawSurfacePtr[dst.left + dst.top * pitch];
				if (dst.top < bottomEnd) {
					rows = bottomEnd - dst.top;
					do {
						memset(ptr, (int)color, width);
						ptr += pitch;
						--rows;
					} while (rows);
				}
			}
		}
	}
}

// FUNCTION: XWA 0x5590D0
void FrontendDraw_RectOutline(FrontendRect* rect, int dx, int dy, int color) {
#ifdef XWA_MODERN
	/* Remaster snapshot observer. */
	XwaSnapshot_EmitPaint(XWA_PAINT_RECT_OUTLINE, rect->left, rect->top, rect->right, rect->bottom, dx, dy,
						  (uint32_t)color);
#endif
	FrontendRect original;
	FrontendRect dst;
	int drawTopEdge;
	int drawBottomEdge;
	int drawLeftEdge;
	int drawRightEdge;
	int top;
	int bottom;
	int width;
	int pitch;
	unsigned char* ptr;
	int middleTop;
	int rows;

	drawTopEdge = 1;
	drawBottomEdge = 1;
	drawLeftEdge = 1;
	drawRightEdge = 1;
	if (rect->right > rect->left && rect->bottom > rect->top) {
		FrontendDraw_RectCopy(&dst, rect);
		FrontendDraw_RectOffsetXY(&dst, dx, dy);
		if (dst.left <= 640 && dst.right >= 0 && dst.top <= 480 && dst.bottom >= 0) {
			FrontendDraw_RectCopy(&original, &dst);
			FrontendDraw_RectClipToBounds(&dst);
			if (dst.left != original.left) {
				drawLeftEdge = 0;
			}
			if (dst.right != original.right) {
				drawRightEdge = 0;
			}
			top = dst.top;
			if (dst.top != original.top) {
				drawTopEdge = 0;
			}
			bottom = dst.bottom;
			if (dst.bottom != original.bottom) {
				drawBottomEdge = 0;
			}
			width = dst.right - dst.left + 1;
			pitch = g_drawSurfacePitch;
			if (g_displayBpp == 8) {
				ptr = &g_drawSurfacePtr[dst.left + dst.top * g_drawSurfacePitch];
				if (drawTopEdge) {
					FrontendDraw_FillRun(ptr, 0, width, color);
					top = dst.top;
					ptr += pitch;
					bottom = dst.bottom;
				}
				middleTop = top + 1;
				if (middleTop < bottom) {
					rows = bottom - middleTop;
					do {
						if (drawLeftEdge) {
							*ptr = (unsigned char)color;
						}
						if (drawRightEdge) {
							ptr[width - 1] = (unsigned char)color;
						}
						ptr += pitch;
						--rows;
					} while (rows);
				}
				if (drawBottomEdge) {
					FrontendDraw_FillRun(ptr, 0, width, color);
				}
			} else if (g_displayBpp == 16) {
				ptr = &g_drawSurfacePtr[2 * dst.left + dst.top * g_drawSurfacePitch];
				if (drawTopEdge) {
					FrontendDraw_FillRun(ptr, 1, width, color);
					top = dst.top;
					ptr += pitch;
				}
				middleTop = top + 1;
				if (middleTop < bottom) {
					rows = bottom - middleTop;
					do {
						if (drawLeftEdge) {
							FrontendDraw_WriteU16(ptr, (unsigned short)color);
						}
						if (drawRightEdge) {
							FrontendDraw_WriteU16(&ptr[2 * width - 2], (unsigned short)color);
						}
						ptr += pitch;
						--rows;
					} while (rows);
				}
				if (drawBottomEdge && width > 0) {
					FrontendDraw_FillRun(ptr, 1, width, color);
				}
			}
		}
	}
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x558F60
void FrontendDraw_Rect(FrontendRect* rect, int dx, int dy, int color, int filled) {
	FrontendRect dst;

	if (!filled) {
		FrontendDraw_RectOutline(rect, dx, dy, color);
		return;
	}

#ifdef XWA_MODERN
	/* Remaster snapshot observer (filled path only; the unfilled path
	 * emits through RectOutline above). */
	XwaSnapshot_EmitPaint(XWA_PAINT_FILL_RECT, rect->left, rect->top, rect->right, rect->bottom, dx, dy,
						  (uint32_t)color);
#endif
	if (rect->right > rect->left && rect->bottom > rect->top) {
		FrontendDraw_RectCopy(&dst, rect);
		FrontendDraw_RectOffsetXY(&dst, dx, dy);
		if (dst.left <= 640 && dst.right >= 0 && dst.top <= 480 && dst.bottom >= 0) {
			int bottomEnd;
			int width;
			int pitch;

			FrontendDraw_RectClipToBounds(&dst);
			bottomEnd = dst.bottom + 1;
			width = dst.right - dst.left + 1;
			pitch = g_drawSurfacePitch;
			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					unsigned char* ptr;

					ptr = &g_drawSurfacePtr[2 * dst.left + dst.top * g_drawSurfacePitch];
					if (dst.top < bottomEnd) {
						int rows;

						rows = bottomEnd - dst.top;
						do {
							FrontendDraw_FillRun(ptr, 1, width, color);
							ptr += pitch;
							--rows;
						} while (rows);
					}
				}
			} else {
				unsigned char* ptr;

				ptr = &g_drawSurfacePtr[dst.left + dst.top * pitch];
				if (dst.top < bottomEnd) {
					int rows;

					rows = bottomEnd - dst.top;
					do {
						memset(ptr, color, (size_t)width);
						ptr += pitch;
						--rows;
					} while (rows);
				}
			}
		}
	}
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif
