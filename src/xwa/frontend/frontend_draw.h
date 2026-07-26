#ifndef XWA_FRONTEND_FRONTEND_DRAW_H
#define XWA_FRONTEND_FRONTEND_DRAW_H

#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	FRONTEND_DRAW_MAX_DIRTY_RECTS = 20,
};

extern unsigned char* g_drawSurfacePtr;
extern int g_drawSurfacePitch;
extern int g_savedDrawSurfacePitch;
extern int g_clipMinX;
extern int g_clipMaxX;
extern int g_clipMinY;
extern int g_clipMaxY;
extern FrontendRect g_dirtyRects[FRONTEND_DRAW_MAX_DIRTY_RECTS];
extern int g_dirtyRectCount;
extern int g_dirtyRectOverflow;

void FrontendDraw_RectAssign(FrontendRect* rc, int left, int top, int right, int bottom);
void FrontendDraw_RectCopy(FrontendRect* dst, const FrontendRect* src);
void FrontendDraw_RectOffsetXY(FrontendRect* rect, int dx, int dy);
void FrontendDraw_RectInsetXY(FrontendRect* rect, int dx, int dy);
int FrontendDraw_RectClipToBounds(FrontendRect* rect);
int FrontendDraw_PointInRect(FrontendRect* rect, int x, int y);
int FrontendDraw_ForceFullScreenPresent(void);
int FrontendDraw_AddDirtyRect(FrontendRect* rect);
int FrontendDraw_BeginExternalSurface(unsigned char* pixels, int pitch);
int FrontendDraw_EndExternalSurface(void);
void FrontendDraw_HorizontalLineClipped(int x0, int x1, int y, int color);
void FrontendDraw_VerticalLineClipped(int y0, int y1, int x, int color);
void FrontendDraw_Line(int x0, int y0, int x1, int y1, int color);
void FrontendDraw_LineAntialiased(int x0, int y0, int x1, int y1, unsigned int color);
void FrontendDraw_FillRectTranslucent(FrontendRect* rect, int dx, int dy, unsigned int color);
void FrontendDraw_RectOutline(FrontendRect* rect, int dx, int dy, int color);
void FrontendDraw_Rect(FrontendRect* rect, int dx, int dy, int color, int filled);

#ifdef __cplusplus
}
#endif

#endif
