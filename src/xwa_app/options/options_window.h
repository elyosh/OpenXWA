#ifndef XWA_APP_OPTIONS_WINDOW_H
#define XWA_APP_OPTIONS_WINDOW_H
#include "ui.h"

/* The OpenXWA Options window: the Video, Controller, Keyboard and Mouse tabs.
 * The datapad opens it (XwaPort_RequestOptionsWindow). Video and mouse
 * changes apply at once. Controller and keyboard changes apply only on Close.
 * Close saves them all. */
bool XwaOptionsWindow_Init(XwaAppUi* ui, char* error, size_t capacity);
void XwaOptionsWindow_Shutdown(void);
void XwaOptionsWindow_FlushForExit(void);
bool XwaOptionsWindow_Open(void);
bool XwaOptionsWindow_CapturesController(void);
bool XwaOptionsWindow_CapturesKeyboard(void);
void XwaOptionsWindow_Show(void);
void XwaOptionsWindow_RequestClose(void);
bool XwaOptionsWindow_CloseRequested(void);
void XwaOptionsWindow_CompleteClose(void);
void XwaOptionsWindow_ReportError(const char* error);
/* The height that makes a tab's scroll area fill the window down to the
 * Close button. */
float XwaOptionsWindow_ScrollHeight(void);
/* Starts the host frame's input: installs pending controller options,
 * commits on Close, captures the input while the window is open or the game
 * has no focus, and reads the controllers. */
bool XwaOptionsWindow_BeginFrame(const AeronInputSnapshot* input);
/* Opens the window if the datapad asked for it. Returns whether it opened. */
bool XwaOptionsWindow_ConsumeRuntimeRequest(void);
void XwaOptionsWindow_Frame(const AeronInputSnapshot* input, float seconds);
/* Tells the player once that the controller settings from an earlier version
 * were reset. */
void XwaOptionsWindow_ShowControllerResetNotice(void);
#endif
