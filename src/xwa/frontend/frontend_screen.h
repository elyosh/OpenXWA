#ifndef XWA_FRONTEND_FRONTEND_SCREEN_H
#define XWA_FRONTEND_FRONTEND_SCREEN_H

#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	FRONTEND_SCREEN_MAX_STACK = 10,
};

typedef enum FrontendScreenModalStatus {
	FRONTEND_SCREEN_MODAL_INACTIVE = -1,
	FRONTEND_SCREEN_MODAL_RUNNING = 0,
	FRONTEND_SCREEN_MODAL_DONE = 1,
	FRONTEND_SCREEN_MODAL_QUIT = 2,
	FRONTEND_SCREEN_MODAL_FAILED = 3,
} FrontendScreenModalStatus;

typedef int (*FrontendScreenUpdateFn)(int frameCounter);
typedef int (*FrontendScreenExitFn)(int frameCounter);
typedef void (*FrontendScreenModalCleanupFn)(void);

typedef struct FrontendScreenState {
	ImageResource savedImage;
	FrontendRect savedRect;
	FrontendRect savedClipRect;
	FrontendScreenUpdateFn updateFn;
	FrontendScreenExitFn exitFn;
	int savedFrameCounter;
} FrontendScreenState;

extern FrontendScreenState g_screenStates[FRONTEND_SCREEN_MAX_STACK];
extern int g_screenStackTop;
extern int g_screenCallbacksDirty;
extern int g_modalScreenActive;
extern int g_modalScreenDepth;
extern FrontendScreenModalStatus g_modalScreenStatus;
extern FrontendScreenUpdateFn g_pendingScreenUpdateFn;
extern FrontendRect g_pendingScreenRect;

void FrontendScreen_SetCallbacks(FrontendScreenUpdateFn updateFn, FrontendScreenExitFn exitFn);
int FrontendScreen_QueuePush(FrontendScreenUpdateFn updateFn, FrontendRect* screenRect);
int FrontendScreen_RunModal(FrontendScreenUpdateFn updateFn, FrontendRect* screenRect);
int FrontendScreen_BeginModal(FrontendScreenUpdateFn updateFn, FrontendRect* screenRect);
int FrontendScreen_BeginModalWithCleanup(FrontendScreenUpdateFn updateFn, FrontendRect* screenRect,
										 FrontendScreenModalCleanupFn cleanupFn);
FrontendScreenModalStatus FrontendScreen_TickModal(void);
void FrontendScreen_EndModal(void);
int FrontendScreen_IsModalActive(void);
FrontendScreenModalStatus FrontendScreen_GetModalStatus(void);
int FrontendScreen_PushState(FrontendScreenUpdateFn updateFn, FrontendRect* screenRect);
void FrontendScreen_PopState(void);
int FrontendScreen_RunFrame(void);

#ifdef __cplusplus
}
#endif

#endif
