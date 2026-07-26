#ifndef XWA_FRONTEND_FRONTEND_DISPLAY_H
#define XWA_FRONTEND_FRONTEND_DISPLAY_H

#include "aeron/render.h"
#include "aeron/surface.h"
#include "xwa_runtime/compat/directx/ddraw.h"
#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FrontendPaletteEntry {
	unsigned char peRed;
	unsigned char peGreen;
	unsigned char peBlue;
	unsigned char peFlags;
} FrontendPaletteEntry;

typedef struct DisplayDriverEntry {
	char name[256];
	void* guid;
	unsigned char supports16bpp[6];
	unsigned char supports8bpp[6];
} DisplayDriverEntry;

typedef union FrontendDisplayBackBufferLock {
	int word;
	unsigned char value;
} FrontendDisplayBackBufferLock;

#ifndef XWA_MODERN
typedef int(__stdcall* FrontendDisplaySetCursorPosFunc)(int x, int y);
#else
typedef int (*FrontendDisplaySetCursorPosFunc)(int x, int y);
#endif

extern int g_displayBpp;
extern unsigned char g_pixelFormat555;
extern int g_pixelFormatCode;
extern int g_flightRenderToFrontend;
extern int g_frameCounter;
extern int g_frameIntervalMs;
extern FrontendDisplayBackBufferLock g_backBufferLocked;
extern int g_surfaceClearColor;
extern uint8_t g_escapeCloseEnabled;
extern uint8_t g_presentFrameReady;
extern int g_frontendDisplayReactivated;
extern int g_backBufferPitch;
extern int g_offscreenSurfacePitch;
extern int g_frontSurfacePitch;
extern int g_presentBlitX;
extern int g_presentBlitY;
extern unsigned char g_offscreenRestoreEnabled;
extern FrontendPaletteEntry g_displayPalette[256];
extern const unsigned int g_colorDistLUT[256];
/* DirectDraw device + surfaces (shim COM objects; single source of truth for the
 * frontend surface set, distinct from the flight surfaces). */
extern IDirectDraw* g_directDraw;
extern IDirectDraw* g_directDrawPrimary;
extern IDirectDrawSurface* g_primarySurface;
extern IDirectDrawSurface* g_frontSurface;
extern IDirectDrawSurface* g_backBufferSurface;
extern IDirectDrawSurface* g_offscreenSurface;
extern IDirectDrawPalette* g_ddPalette;
extern DDSURFACEDESC g_backBufferDesc;
extern void* g_hWnd;
extern unsigned char* g_offscreenBackupBuffer;
extern void* g_frontendDisplayLifecycleScratch;
extern FrontendDisplaySetCursorPosFunc g_SetCursorPos;

int FrontendDisplay_InitSurfaces(void);
void FrontendDisplay_FreeSurfaces(void);
void FrontendDisplay_DisableEscapeClose(void);
void FrontendDisplay_ClearPresentFrameReady(void);
int FrontendDisplay_SetSurfaceClearColor(int color);
unsigned char* FrontendDisplay_LockSurfaceForFlight(void);
int FrontendDisplay_UnlockSurfaceForFlight(void);
unsigned char* FrontendDisplay_LockBackBuffer(void);
int FrontendDisplay_UnlockBackBuffer(void);
int FrontendDisplay_ClearBackBuffer(void);
void FrontendDisplay_ClearOffscreenSurface(void);
int FrontendDisplay_LockOffscreenSurface(void);
int FrontendDisplay_UnlockOffscreenSurface(int saveToBackup);
int FrontendDisplay_EnableOffscreenRestore(void);
int FrontendDisplay_DisableOffscreenRestore(void);
int FrontendDisplay_SaveBackBuffer(void);
int FrontendDisplay_RestoreBackBuffer(void);
int FrontendDisplay_CopyFrontToBackBuffer(void);
int FrontendDisplay_BlitOffscreenToFront(void);
int FrontendDisplay_SwitchDriver(int driverId);
int FrontendDisplay_SelectDriver(int driverIndex);
int FrontendDisplay_SelectActiveDirectDrawDevice(void);
int FrontendDisplay_SelectPrimaryDirectDrawDevice(void);
int FrontendDisplay_SetWndProcMode(unsigned int mode);
int FrontendDisplay_IsSecondaryDirectDrawActive(void);
int FrontendDisplay_RestorePrimaryDriver(void);
int FrontendDisplay_ReinitSurfaces(void);
int FrontendDisplay_ReleaseSurfaces(void);
int FrontendDisplay_ReleaseSurfacesForFlight(void);
int FrontendDisplay_AttachExternalSurfaces(IDirectDrawSurface* primarySurface,
										   IDirectDrawSurface* frontSurface,
										   IDirectDrawSurface* backBufferSurface,
										   IDirectDrawSurface* offscreenSurface, int surfaceWidth,
										   int surfaceHeight);
int FrontendDisplay_DetachExternalSurfaces(void);
int FrontendDisplay_SetPalette(void);
int FrontendDisplay_PresentFrame(void);
void FrontendDisplay_FlipDirectDrawToGDISurface(void);
int FrontendDisplay_ShowGameMessageBox(const char* message);
HRESULT FrontendDisplay_RestoreLostSurfaces(void);
IDirectDrawPalette* FrontendDisplay_LoadPalette(IDirectDraw* pDD, const char* lpName);
IDirectDraw* FrontendDisplay_GetDirectDraw(void);
void FrontendDisplay_GetScreenClipRect(FrontendRect* outRect);
void FrontendDisplay_ResetScreenClipRect(void);
void FrontendDisplay_SetScreenClipRect640x480(const FrontendRect* src);
int FrontendDisplay_GetReactivatedFlag(void);
int FrontendDisplay_SetDirtyRectBlitEnabled(unsigned char enabled);
int FrontendDisplay_SetFrameRate(int fps);
void FrontendDisplay_SetReactivatedFlag(int value);
void FrontendDisplay_ClearReactivatedFlag(void);
int FrontendDisplay_GetFrameCounter(void);
AeronPixelFormat FrontendDisplay_GetPixelFormat(void);
int FrontendDisplay_GetPixelFormat555(void);
int Display_IsPixelFormat555(void);
int FrontendDisplay_GetDrawSurfacePitch(void);
int FrontendDisplay_GetFrontendOrFlightDrawPitch(void);
int FrontendDisplay_PackRGB(unsigned char r, unsigned char g, unsigned char b);
DisplayDriverEntry* FrontendDisplay_GetDriverTable(unsigned int* outDriverCount);
int FrontendDisplay_DriverSupportsResolutionBpp(unsigned int driverIndex, int resolutionMode, int bpp);

#ifdef __cplusplus
}
#endif

#endif
