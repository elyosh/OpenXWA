#ifndef XWA_FLIGHT_FLIGHT_DISPLAY_H
#define XWA_FLIGHT_FLIGHT_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "xwa_runtime/compat/directx/ddraw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Flight DirectDraw device + presented surfaces (shim COM objects). Cross-module
 * globals in the original: the loading/ready path (flight_loading.c) creates its
 * overlay surfaces against this device and presents onto the flight primary/back
 * buffer. */
extern IDirectDraw*        g_flightDirectDraw;
extern IDirectDrawSurface* g_flightPrimarySurface;
extern IDirectDrawSurface* g_flightBackBufferSurface;
/* Shared in-flight frontend-overlay temp surfaces (0x773350 / 0x773354). Created
 * and released by both the loading/ready screens (flight_loading.c) and the
 * in-flight modals (flight_display.c). */
extern IDirectDrawSurface* g_flightOverlayBackBufferSurface;
extern IDirectDrawSurface* g_flightOverlayOffscreenSurface;

extern int   g_flightSurfaceViewport480ByteSpan;
extern int   g_flightPrimaryPitch;
extern int   g_surfaceLockCount;
extern bool   g_flightLockBackBufferForHudDraw;
extern uint8_t g_flightDisplaySurfacesActive;
extern int   g_flightAlertBoxVerticalOffset;
extern int16_t* g_flightAlertBoxSavedPixels;
extern int   g_flightAlertBoxSavedBytes;
extern void* g_swFramebufferBase;
extern void* g_flightSwFramebufferBase;
extern float g_flightSwPointSpriteScale;
extern void* g_flightSwRotSpriteDestBuffer;
extern int   g_flightLineOffsetTable[1200];
extern int   g_flightAltLineOffsetTable[1200];
extern int   g_flightAltLinePitch;
extern int*  g_flightLinePitchPtr;
extern int*  g_flightLineBufferTable;

char  FlightDisplay_Init(void);
void  FlightDisplay_FreeSurfaces(void);
int   FlightDisplay_IsFrontendModalActive(void);
int   FlightDisplay_PumpFrontendModal(void);
int   FlightDisplay_RunRestrictedOptionsModal(void);
char* FlightFilm_RunNamePrompt(void);
int   FlightFilm_SaveTempRecordingWithPrompt(void);
int   FlightRender_InstallCallbacks(uint8_t initialGraphicsDetailPreset);
void  Flight_ApplyGraphicsDetailPreset(uint16_t graphicsDetailPreset);
int   FlightDisplay_GetPrimarySurfacePitch(void);
int   FlightDisplay_Flip(void);
int   FlightDisplay_BlitRenderSurface(void);
int   FlightSurface_Lock(void);
int   FlightSurface_Unlock(void);
void  FlightSurface_ClearToBlack(void);
void  FlightAlert_SaveBoxBackground(void);
void  FlightAlert_RestoreBoxBackground(void);
void  FlightAlert_DrawBox(int verticalMode, char* line1, char* line2, uint8_t bgColor);
int   FlightScreenshot_Capture(void);
int   FlightSurface_GetLockCount(void);
void* FlightSurface_GetSoftwareFramebufferBase(void);
int   FlightSurface_SetViewport480ByteSpan(int byteSpan);
void* FlightSurface_SetSoftwareFramebufferBase(void* framebufferBase);
void  FlightSw_InitLineBuffer(void);
void  FlightSw_SetRenderTarget(void* surface, int width, unsigned int height, int pitchBytes);
void  FlightSw_SetRotatedSpriteDestBuffer(void* destBuffer);
void  Blit16ToFlightSurface(const void* srcPixels, uint16_t colorKeyPaletteIndex,
							uint16_t srcX, uint16_t srcY, uint16_t dstX, uint16_t dstY,
							uint16_t width, uint16_t height, uint16_t srcPitchBytes);

#ifdef __cplusplus
}
#endif

#endif
