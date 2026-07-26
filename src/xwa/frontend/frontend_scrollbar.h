#ifndef XWA_FRONTEND_FRONTEND_SCROLLBAR_H
#define XWA_FRONTEND_FRONTEND_SCROLLBAR_H

#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_scrollableControlCount;
extern int g_frontendFirstVisibleLine;
extern int g_scrollableControlIds[32];

int Frontend_ResetScrollableControls(void);
int Frontend_RegisterScrollableControl(int controlId);
int Frontend_CycleScrollableFocus(void);
int FrontendScrollbar_SaveState(void);
int FrontendScrollbar_RestoreState(void);
int FrontendScrollbar_Draw(FrontendRect* rect, int currentIndex, int itemCount, int minIndex, int pageSize,
						   int color, int controlId);

#ifdef __cplusplus
}
#endif

#endif
