#ifndef XWA_FRONTEND_FRONTEND_ESCAPE_H
#define XWA_FRONTEND_FRONTEND_ESCAPE_H

#ifdef __cplusplus
extern "C" {
#endif

int Frontend_SavePersistentState(void);
int Frontend_HandleEscapeQuit(int sessionContext);

#ifdef __cplusplus
}
#endif

#endif
