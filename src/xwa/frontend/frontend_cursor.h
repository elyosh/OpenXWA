#ifndef XWA_FRONTEND_FRONTEND_CURSOR_H
#define XWA_FRONTEND_FRONTEND_CURSOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FrontendCursorBitmap {
	unsigned char pixels[100];
} FrontendCursorBitmap;

extern int g_mouseX;
extern int g_mouseY;
extern int g_cursorPrevDrawX;
extern int g_cursorPrevDrawY;
extern unsigned char g_cursorVisible;
extern FrontendCursorBitmap g_cursorDefaultMask;
extern unsigned char g_cursorDefaultSaveBuf[200];
extern unsigned char* g_cursorMaskPixels;
extern unsigned char* g_cursorSaveBuf;
extern int g_cursorWidth;
extern int g_cursorHeight;
extern int g_cursorPrevDrawWidth;
extern int g_cursorPrevDrawHeight;
extern char g_cursorSpriteName[64];
extern char g_cursorLabelText[256];
extern const FrontendCursorBitmap g_defaultCursorBitmap;

void FrontendCursor_Show(void);
void FrontendCursor_Hide(void);
int FrontendCursor_IsVisible(void);
int FrontendCursor_SetLabel(const void* text);
int* FrontendCursor_GetPos(int* outX, int* outY);
int FrontendCursor_SetPos(int x, int y);
int FrontendCursor_HideOsCursor(void);
int FrontendCursor_ShowOsCursor(void);
void FrontendCursor_Init(void);
int FrontendCursor_LoadResources(void);
int FrontendCursor_FreeResources(void);
int FrontendCursor_SetImageFromResourceName(char* resourceName, void* saveBuf);
int FrontendCursor_SetImageResourceForCurrentTheme(char* name, void* outBuffer);
void FrontendCursor_Draw(void);

#ifdef __cplusplus
}
#endif

#endif
