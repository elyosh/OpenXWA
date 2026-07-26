#include "xwa/flight/flight_display.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/object_type.h"
#include "xwa/audio/sound.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/film.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/input/dinput.h"
#include "xwa/net/net.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/time.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef XWA_MODERN
__declspec(dllimport) int __cdecl wsprintfA(char* buffer, const char* format, ...);
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* outputString);
__declspec(dllimport) int __stdcall MessageBoxA(void* hwnd, const char* text, const char* caption,
												unsigned int type);
// GLOBAL: XWA 0x5A926C
int(__cdecl* g_wsprintfA)(char* buffer, const char* format, ...) = wsprintfA;
// GLOBAL: XWA 0x5A9284
int(__stdcall* g_MessageBoxA)(void* hwnd, const char* text, const char* caption,
							  unsigned int type) = MessageBoxA;
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#define FLIGHT_OUTPUT_DEBUG_STRING g_OutputDebugStringA
#else
#include <stdarg.h>
static int wsprintfA(char* buffer, const char* format, ...) {
	va_list args;
	int result;

	va_start(args, format);
	result = vsnprintf(buffer, 256, format, args);
	va_end(args);
	return result;
}

static void OutputDebugStringA(const char* outputString) { DebugPrintf("%s", outputString); }
#define FLIGHT_OUTPUT_DEBUG_STRING OutputDebugStringA

static int MessageBoxA(void* hwnd, const char* text, const char* caption, unsigned int type) {
	(void)hwnd;
	(void)type;
	DebugPrintf("%s: %s", caption, text);
	return 0;
}
#endif

// GLOBAL: XWA 0x91AE64
int g_flightPrimaryPitch;
// GLOBAL: XWA 0x7733CC
int g_surfaceLockCount;
// GLOBAL: XWA 0x773340
bool g_flightLockBackBufferForHudDraw;
// GLOBAL: XWA 0x8C28DB
uint8_t g_flightDisplaySurfacesActive;
// GLOBAL: XWA 0x7827D0
int g_flightAlertBoxVerticalOffset;
// GLOBAL: XWA 0x7828D0
int16_t* g_flightAlertBoxSavedPixels;
// GLOBAL: XWA 0x7828D4
int g_flightAlertBoxSavedBytes;
// GLOBAL: XWA 0x6000EC
void* g_swFramebufferBase = (void*)0xA0000;
// GLOBAL: XWA 0x6000F0
int g_flightSurfaceViewport480ByteSpan;
// GLOBAL: XWA 0x6002BC
void* g_flightSwFramebufferBase = (void*)0xA0000;
// GLOBAL: XWA 0x7D4B78
float g_flightSwPointSpriteScale;
// GLOBAL: XWA 0x80ACF4
void* g_flightSwRotSpriteDestBuffer;
// GLOBAL: XWA 0x7CA3C0
RgbTriplet g_swPalette[256];
// GLOBAL: XWA 0x91AB76
uint8_t g_paletteDirtyFlags;
// GLOBAL: XWA 0x7F90A0
int g_flightLineOffsetTable[1200];
// GLOBAL: XWA 0x74C2E0
int g_flightAltLineOffsetTable[1200];
// GLOBAL: XWA 0x74C2B8
int g_flightAltLinePitch;
// GLOBAL: XWA 0x74D5A4
int* g_flightLinePitchPtr = &g_surfacePitch;
// GLOBAL: XWA 0x74D5A0
int* g_flightLineBufferTable = g_flightLineOffsetTable;
// GLOBAL: XWA 0x74C278
uint16_t g_flightFillRectBottom = 0xffff;
// GLOBAL: XWA 0x74C27C
uint16_t g_flightFillRectRight = 0xffff;
// GLOBAL: XWA 0x74C280
uint16_t g_flightFillRectLeft = 0xffff;
// GLOBAL: XWA 0x74C284
uint16_t g_flightFillRectTop = 0xffff;

/* Per-row pixel counter shared by FlightSw_SaveScreenRect /
 * FlightSw_RestoreScreenRect while copying a rectangle row. */
// GLOBAL: XWA 0x8C28D8
uint16_t g_savedRowPixelsRemaining;

/* Chunk size (bytes) used by FlightSw_InitLineBuffer to zero the software
 * framebuffer one block at a time. */
// GLOBAL: XWA 0x6002A8
unsigned int g_swFramebufferClearChunkSize = 0xF000;

/* Signed (dx,dy) offset tables selected as the active software-clear run
 * pattern. FlightSw_InitLineBuffer switches to the second table on a valid
 * resolution mode. */
// GLOBAL: XWA 0x5BA840
signed char g_flightSwFramebufferClearRun0[24] = {
	-1, 1, -2, 1, -2, 0, -2, -1, -1, -1, 1, -1, 2, -1, 2, 0, 2, 1, 1, 1, 0, 0, 0, 0,
};
// GLOBAL: XWA 0x5BA858
signed char g_flightSwFramebufferClearRun1[24] = {
	-1, 2, -2, 2, -2, 1, -2, 0, -2, -1, -1, -1, 1, -1, 2, -1, 2, 0, 2, 1, 2, 2, 1, 2,
};
// GLOBAL: XWA 0x5BA870
signed char* g_flightSwFramebufferClearRunPtr = g_flightSwFramebufferClearRun0;
// GLOBAL: XWA 0x5BA874
int g_flightSwFramebufferClearRunCount = 10;

/* In-flight frontend-overlay temp surfaces. Created at the flight resolution
 * whenever the 2D frontend must draw over the flight display (loading/ready
 * screens and the options/film-name modals), then released. Shared across those
 * paths, matching the original's backBufferSurface/offscreenSurface globals. */
// GLOBAL: XWA 0x773350
IDirectDrawSurface* g_flightOverlayBackBufferSurface;
// GLOBAL: XWA 0x773354
IDirectDrawSurface* g_flightOverlayOffscreenSurface;
// GLOBAL: XWA 0x773344
IDirectDrawSurface* g_flightPrimarySurface;
// GLOBAL: XWA 0x773348  (g_flightBackBuffer; the std3D render target)
IDirectDrawSurface* g_flightBackBufferSurface;
// GLOBAL: XWA 0x77334C
IDirectDrawSurface* g_flightOffscreenSurface;
// GLOBAL: XWA 0x773358
IDirectDraw* g_flightDirectDraw;
// GLOBAL: XWA 0x77331C
uint8_t g_flightSoftwareFallback;
// GLOBAL: XWA 0x7FBB60
uint8_t g_flightDisplayCoopLevelFallbackUsed;
// GLOBAL: XWA 0x91AE68
int g_displayMemoryType;
// GLOBAL: XWA 0x91AD60
char g_flightDisplayDebugMsgBuffer[256];
// GLOBAL: XWA 0x7733D0
uint32_t g_flightFlickerLastSyncTimeMs;
// GLOBAL: XWA 0x7722D0
int g_flightFlickerRefreshRateScale;
// GLOBAL: XWA 0x5FFE00
int g_flightFlickerPhaseWindow;
static uint8_t g_flightFrontendModalSavedSound3DEnabled;
static int g_flightFrontendModalActive;

enum { DISPLAY_MEMORY_SYSTEM = 1, DISPLAY_MEMORY_VIDEO = 2 };

// GLOBAL: XWA 0x5BA808
static const int8_t g_radarTargetMarkerShapeOffsets[20] = {
	-1, 1, -2, 1, -2, 0, -2, -1, -1, -1, 1, -1, 2, -1, 2, 0, 2, 1, 1, 1,
};
/* Software mode draws the radar target marker directly into the radar scope and
   restores these covered pixels on the next update. */
// GLOBAL: XWA 0x74C288
static uint16_t g_radarTargetMarkerBackgroundSavedPixels[10];
// GLOBAL: XWA 0x5A9E24
const float g_radarTargetMarkerBaseSize = 256.0f;

// GLOBAL: XWA 0x5FFCD0
const uint16_t g_debrisDensityByGraphicsDetail[4] = { 2, 3, 5, 7 };
// GLOBAL: XWA 0x5FFCD8
const uint16_t g_starDensityByGraphicsDetail[4] = { 4, 3, 2, 1 };
// GLOBAL: XWA 0x5FFCE0
const uint16_t g_backdropsByGraphicsDetail[4] = { 0, 0, 0, 1 };
// GLOBAL: XWA 0x5FFCE8
const uint16_t g_debrisByGraphicsDetail[4] = { 0, 0, 1, 1 };
// GLOBAL: XWA 0x5FFCF0
const uint16_t g_localLightsByGraphicsDetail[4] = { 0, 0, 1, 2 };
// GLOBAL: XWA 0x5FFCF8
const uint16_t g_specularByGraphicsDetail[4] = { 0, 0, 0, 1 };
// GLOBAL: XWA 0x5FFD00
const uint16_t g_dirLightingByGraphicsDetail[4] = { 1, 1, 1, 1 };
// GLOBAL: XWA 0x5FFD08
const uint16_t g_hitEffectsByGraphicsDetail[4] = { 0, 0, 1, 1 };
// GLOBAL: XWA 0x5FFD10
const uint16_t g_particleEffectsByGraphicsDetail[4] = { 0, 0, 1, 1 };
// GLOBAL: XWA 0x5FFD18
const uint16_t g_trailsByGraphicsDetail[4] = { 0, 0, 1, 1 };
// GLOBAL: XWA 0x5FFD20
const uint16_t g_engineGlowByGraphicsDetail[4] = { 0, 0, 1, 1 };
// GLOBAL: XWA 0x5FFD28
const uint16_t g_lensFlareByGraphicsDetail[4] = { 0, 0, 1, 1 };
// GLOBAL: XWA 0x5FFD30
const uint16_t g_yardLodByGraphicsDetail[4] = { 3, 8, 12, 16 };

static void FlightDisplay_ClearSurfaceToBlack(IDirectDrawSurface* surface);

/* Open the in-flight frontend modal surface set. Mirrors the surface handling of
 * the original blocking modals (RunRestrictedOptionsModal @0x50C740,
 * FlightFilm_RunNamePrompt @0x50CA90): create two temp offscreen surfaces at the
 * flight resolution, COLORFILL-clear the flight primary/back buffer and both temp
 * surfaces, then hand the real flight primary/back buffer plus the two temp
 * surfaces to the frontend as its external surface set. The original then drove a
 * blocking modal loop; the port keeps the surfaces attached and pumps the modal
 * non-blocking across ticks (see FlightDisplay_PumpFrontendModal), detaching in
 * FlightDisplay_DetachFrontendModalSurfaces. */
static int FlightDisplay_AttachFrontendModalSurfaces(void) {
	DDSURFACEDESC desc;

	if (g_surfaceWidth <= 0 || g_surfaceHeight <= 0) {
		return 0;
	}

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = 108;
	desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	desc.ddsCaps.dwCaps =
		g_useHardware3D ? (DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY) : DDSCAPS_OFFSCREENPLAIN;
	desc.dwHeight = g_surfaceHeight;
	desc.dwWidth = g_surfaceWidth;
	if (g_flightDirectDraw->lpVtbl->CreateSurface(g_flightDirectDraw, &desc, &g_flightOverlayOffscreenSurface,
												  NULL)) {
		return 0;
	}
	if (g_flightDirectDraw->lpVtbl->CreateSurface(g_flightDirectDraw, &desc,
												  &g_flightOverlayBackBufferSurface, NULL)) {
		if (g_flightOverlayOffscreenSurface) {
			g_flightOverlayOffscreenSurface->lpVtbl->Release(g_flightOverlayOffscreenSurface);
			g_flightOverlayOffscreenSurface = NULL;
		}
		return 0;
	}

	FlightDisplay_ClearSurfaceToBlack(g_flightPrimarySurface);
	FlightDisplay_ClearSurfaceToBlack(g_flightBackBufferSurface);
	FlightDisplay_ClearSurfaceToBlack(g_flightOverlayBackBufferSurface);
	FlightDisplay_ClearSurfaceToBlack(g_flightOverlayOffscreenSurface);
	FrontendDisplay_AttachExternalSurfaces(g_flightPrimarySurface, g_flightBackBufferSurface,
										   g_flightOverlayBackBufferSurface, g_flightOverlayOffscreenSurface,
										   g_surfaceWidth, g_surfaceHeight);

	g_flightFrontendModalSavedSound3DEnabled = g_sound3DEnabled;
	g_sound3DEnabled = 0;
	g_flightFrontendModalActive = 1;
	return 1;
}

static void FlightDisplay_DetachFrontendModalSurfaces(void) {
	if (!g_flightFrontendModalActive) {
		return;
	}

	g_sound3DEnabled = g_flightFrontendModalSavedSound3DEnabled;
	FrontendDisplay_DetachExternalSurfaces();
	FlightDisplay_ClearSurfaceToBlack(g_flightPrimarySurface);
	FlightDisplay_ClearSurfaceToBlack(g_flightBackBufferSurface);
	if (g_flightOverlayBackBufferSurface) {
		FlightDisplay_ClearSurfaceToBlack(g_flightOverlayBackBufferSurface);
		g_flightOverlayBackBufferSurface->lpVtbl->Release(g_flightOverlayBackBufferSurface);
		g_flightOverlayBackBufferSurface = NULL;
	}
	if (g_flightOverlayOffscreenSurface) {
		g_flightOverlayOffscreenSurface->lpVtbl->Release(g_flightOverlayOffscreenSurface);
		g_flightOverlayOffscreenSurface = NULL;
	}
	g_flightFrontendModalActive = 0;
	DInput_DrainKeyboardEvents();
}

static FrontendScreenModalStatus FlightDisplay_PumpFrontendModalFrame(void) {
	FrontendScreenModalStatus modalStatus;

	/* Draw-then-present: tick the modal (which renders into the frontend back
	   buffer), then PresentFrame composites and submits it via the shim. The
	   flight sim is paused while the modal is up, so it submits nothing and no
	   render-submission clearing is needed. */
	modalStatus = FrontendScreen_GetModalStatus();
	if (modalStatus == FRONTEND_SCREEN_MODAL_RUNNING) {
		modalStatus = FrontendScreen_TickModal();
	}
	FrontendDisplay_PresentFrame();
	return modalStatus;
}

int FlightDisplay_PumpFrontendModal(void) {
	FrontendScreenModalStatus modalStatus;

	if (!g_flightFrontendModalActive) {
		return 1;
	}

	modalStatus = FlightDisplay_PumpFrontendModalFrame();
	return modalStatus == FRONTEND_SCREEN_MODAL_DONE || modalStatus == FRONTEND_SCREEN_MODAL_QUIT ||
		   modalStatus == FRONTEND_SCREEN_MODAL_FAILED || modalStatus == FRONTEND_SCREEN_MODAL_INACTIVE;
}

static int FlightDisplay_TickFrontendModal(void) {
	FrontendScreenModalStatus modalStatus;

	modalStatus = FlightDisplay_PumpFrontendModalFrame();
	if (modalStatus == FRONTEND_SCREEN_MODAL_DONE || modalStatus == FRONTEND_SCREEN_MODAL_QUIT ||
		modalStatus == FRONTEND_SCREEN_MODAL_FAILED) {
		FrontendScreen_EndModal();
		/* A completed nested dialog resumes the outer flight options modal. */
		return !FrontendScreen_IsModalActive();
	}

	return modalStatus == FRONTEND_SCREEN_MODAL_INACTIVE;
}

int FlightDisplay_IsFrontendModalActive(void) { return g_flightFrontendModalActive; }

/* Port glue (no original counterpart). The original releases the flight
 * surfaces inline in the teardown tail of Flight_Main @0x50A7E0 (0x50B451-
 * 0x50B493); the port's task shell has no Flight_Main, so it needs a standalone
 * teardown entry point. Releases the flight shim COM surfaces in the original's
 * order (offscreen, back buffer, primary). Eliminated once Flight_Main is
 * reverted and the teardown moves inline there. */
void FlightDisplay_FreeSurfaces(void) {
	if (g_flightFrontendModalActive) {
		FlightDisplay_DetachFrontendModalSurfaces();
	}

	if ((int)(intptr_t)g_flightOffscreenSurface) {
		g_flightOffscreenSurface->lpVtbl->Release(g_flightOffscreenSurface);
		g_flightOffscreenSurface = 0;
	}
	if (g_flightBackBufferSurface) {
		g_flightBackBufferSurface->lpVtbl->Release(g_flightBackBufferSurface);
		g_flightBackBufferSurface = 0;
	}
	if (g_flightPrimarySurface) {
		g_flightPrimarySurface->lpVtbl->Release(g_flightPrimarySurface);
		g_flightPrimarySurface = 0;
	}
}

/* Clear a flight surface to black via DDBLT_COLORFILL, restoring the primary on
 * DDERR_SURFACELOST and retrying on DDERR_WASSTILLDRAWING. */
static void FlightDisplay_ClearSurfaceToBlack(IDirectDrawSurface* surface) {
	DDBLTFX fx;
	HRESULT hr;

	fx.dwSize = 100;
	fx.dwFillColor = 0;
	do {
		hr = surface->lpVtbl->Blt(surface, NULL, NULL, NULL, DDBLT_COLORFILL, &fx);
	} while (
		hr &&
		(hr != DX_DDERR_SURFACELOST || !g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface)) &&
		hr == DX_DDERR_WASSTILLDRAWING);
}

static __inline char FlightDisplay_ReportInitFailure(int errorCode) {
#ifndef XWA_MODERN
	g_wsprintfA(g_flightDisplayDebugMsgBuffer, "___CleanupAndExit  err = %d\n", errorCode);
#else
	wsprintfA(g_flightDisplayDebugMsgBuffer, "___CleanupAndExit  err = %d\n", errorCode);
#endif
	FLIGHT_OUTPUT_DEBUG_STRING(g_flightDisplayDebugMsgBuffer);
	if (g_flightOffscreenSurface) {
		g_flightOffscreenSurface->lpVtbl->Release(g_flightOffscreenSurface);
		g_flightOffscreenSurface = NULL;
	}
	if (g_flightBackBufferSurface) {
		if (g_flightFullscreen) {
			g_flightBackBufferSurface->lpVtbl->Release(g_flightBackBufferSurface);
		} else {
			g_flightBackBufferSurface->lpVtbl->Release(g_flightBackBufferSurface);
		}
		g_flightBackBufferSurface = NULL;
	}
	if (g_flightPrimarySurface) {
		g_flightPrimarySurface->lpVtbl->Release(g_flightPrimarySurface);
		g_flightPrimarySurface = NULL;
	}
	NetSession_Shutdown();
#ifndef XWA_MODERN
	g_MessageBoxA(NULL, "Game could not start", "ERROR", 0);
#else
	MessageBoxA(NULL, "Game could not start", "ERROR", 0);
#endif
	return 0;
}

// FUNCTION: XWA 0x50BC20
char FlightDisplay_Init(void) {
	DDSCAPS caps;
	DDSURFACEDESC desc;
	DDBLTFX bltFx;
	DDCAPS ddcaps;
	char err;
	HRESULT displayResult;
	HRESULT bltResult;
	IDirectDrawSurface* clearSurface;

	g_flightSoftwareFallback = 0;
	g_flightDirectDraw = FrontendDisplay_GetDirectDraw();
	g_flightDisplayCoopLevelFallbackUsed = 0;

	/* Acquire an exclusive cooperative level (0x851 = EXCLUSIVE|FULLSCREEN|
	   ALLOWMODEX|FPUSETUP, falling back to 0x51 = EXCLUSIVE|FULLSCREEN|ALLOWREBOOT). */
	if (g_flightFullscreen) {
		displayResult = g_flightDirectDraw->lpVtbl->SetCooperativeLevel(g_flightDirectDraw, hwnd, 0x851);
		if (displayResult) {
			displayResult = g_flightDirectDraw->lpVtbl->SetCooperativeLevel(g_flightDirectDraw, hwnd, 0x51);
			if (displayResult) {
				return FlightDisplay_ReportInitFailure(2);
			}
			g_flightDisplayCoopLevelFallbackUsed = 1;
		}
	} else {
		displayResult = g_flightDirectDraw->lpVtbl->SetCooperativeLevel(g_flightDirectDraw, hwnd, 0x851);
		if (displayResult) {
			displayResult = g_flightDirectDraw->lpVtbl->SetCooperativeLevel(g_flightDirectDraw, hwnd, 0x51);
			if (displayResult) {
				return FlightDisplay_ReportInitFailure(2);
			}
			g_flightDisplayCoopLevelFallbackUsed = 1;
		}
	}

	if (g_flightFullscreen) {
		displayResult = g_flightDirectDraw->lpVtbl->SetDisplayMode(g_flightDirectDraw, width, height,
																   8 * g_flight16bppBytesPerPixel);
		if (displayResult) {
			/* Resolution fallback chain: 320x240 -> 512x384 -> 640x480. */
			if (width == 320) {
				width = 512;
				height = 384;
				displayResult = g_flightDirectDraw->lpVtbl->SetDisplayMode(g_flightDirectDraw, 512, 384,
																		   8 * g_flight16bppBytesPerPixel);
				if (displayResult) {
					width = 640;
					height = 480;
					displayResult = g_flightDirectDraw->lpVtbl->SetDisplayMode(
						g_flightDirectDraw, 640, 480, 8 * g_flight16bppBytesPerPixel);
					if (displayResult) {
						width = 320;
						height = 240;
					}
				}
			} else if (width == 512) {
				width = 640;
				height = 480;
				displayResult = g_flightDirectDraw->lpVtbl->SetDisplayMode(g_flightDirectDraw, 640, 480,
																		   8 * g_flight16bppBytesPerPixel);
				if (displayResult) {
					width = 512;
					height = 384;
				}
				if (displayResult) {
#ifndef XWA_MODERN
					g_wsprintfA(g_flightDisplayDebugMsgBuffer, "___CleanupAndExit  err = %d\n", 3);
#else
					wsprintfA(g_flightDisplayDebugMsgBuffer, "___CleanupAndExit  err = %d\n", 3);
#endif
					FLIGHT_OUTPUT_DEBUG_STRING(g_flightDisplayDebugMsgBuffer);
					if (g_flightOffscreenSurface) {
						g_flightOffscreenSurface->lpVtbl->Release(g_flightOffscreenSurface);
					}
					if (g_flightOffscreenSurface) {
						g_flightOffscreenSurface = NULL;
					}
					if (g_flightBackBufferSurface) {
						if (g_flightFullscreen) {
							g_flightBackBufferSurface->lpVtbl->Release(g_flightBackBufferSurface);
						} else {
							g_flightBackBufferSurface->lpVtbl->Release(g_flightBackBufferSurface);
						}
						g_flightBackBufferSurface = NULL;
					}
					if (g_flightPrimarySurface) {
						g_flightPrimarySurface->lpVtbl->Release(g_flightPrimarySurface);
						g_flightPrimarySurface = NULL;
					}
					NetSession_Shutdown();
#ifndef XWA_MODERN
					g_MessageBoxA(NULL, "Game could not start", "ERROR", 0);
#else
					MessageBoxA(NULL, "Game could not start", "ERROR", 0);
#endif
					return 0;
				}
			}
		}
	} else {
		displayResult = g_flightDirectDraw->lpVtbl->SetDisplayMode(g_flightDirectDraw, width, height,
																   8 * g_flight16bppBytesPerPixel);
	}
	if (displayResult) {
		return FlightDisplay_ReportInitFailure(3);
	}

	memset(&ddcaps, 0, sizeof(ddcaps));
	g_displayMemoryType = DISPLAY_MEMORY_SYSTEM;
	ddcaps.dwSize = 380;
	displayResult = g_flightDirectDraw->lpVtbl->GetCaps(g_flightDirectDraw, &ddcaps, NULL);
	if (!displayResult && (ddcaps.dwCaps & 0x400000) != 0 && (ddcaps.dwCKeyCaps & 1) != 0 &&
		(ddcaps.dwCKeyCaps & 0x200) == 0) {
		g_displayMemoryType = DISPLAY_MEMORY_VIDEO;
	}

	if (g_flightFullscreen) {
		/* Fullscreen: flip-chain primary (page flip) or plain primary. */
		memset(&desc, 0, sizeof(desc));
		desc.dwSize = 108;
		if (g_flightPageFlip) {
			desc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
			desc.ddsCaps.dwCaps = 0x218; /* PRIMARY|FLIP|COMPLEX */
			desc.dwBackBufferCount = 1;
			if (g_useHardware3D) {
				desc.ddsCaps.dwCaps = 0x2218;
			}
		} else {
			desc.dwFlags = DDSD_CAPS;
			desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
			if (g_useHardware3D) {
				desc.ddsCaps.dwCaps = 0x2200;
			}
		}
		displayResult = g_flightDirectDraw->lpVtbl->CreateSurface(g_flightDirectDraw, &desc,
																  &g_flightPrimarySurface, NULL);
		if (displayResult) {
			return FlightDisplay_ReportInitFailure(4);
		}
		memset(&desc, 0, sizeof(desc));
		g_pixelFormatCode = 565;
		desc.dwSize = 108;
		g_flightPrimarySurface->lpVtbl->GetSurfaceDesc(g_flightPrimarySurface, &desc);
		g_flightPrimaryPitch = desc.lPitch;
		g_surfacePitch = desc.lPitch;
		if (desc.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8) {
			DebugPrintf("8-bit screen surface!");
		} else if (desc.ddpfPixelFormat.dwFlags & DDPF_RGB) {
			g_pixelFormatCode = (desc.ddpfPixelFormat.dwGBitMask & 0x400) != 0 ? 565 : 555;
		}
		clearSurface = g_flightPrimarySurface;
		bltFx.dwSize = 100;
		bltFx.dwFillColor = 0;
		do {
			bltResult = clearSurface->lpVtbl->Blt(clearSurface, NULL, NULL, NULL, DDBLT_COLORFILL, &bltFx);
		} while (bltResult &&
				 (bltResult != DX_DDERR_SURFACELOST ||
				  !(displayResult = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface))) &&
				 bltResult == DX_DDERR_WASSTILLDRAWING);
		if (g_flightPageFlip) {
			caps.dwCaps = DDSCAPS_BACKBUFFER;
			displayResult = g_flightPrimarySurface->lpVtbl->GetAttachedSurface(g_flightPrimarySurface, &caps,
																			   &g_flightBackBufferSurface);
			if (displayResult) {
				return FlightDisplay_ReportInitFailure(5);
			}
			clearSurface = g_flightBackBufferSurface;
			bltFx.dwSize = 100;
			bltFx.dwFillColor = 0;
			do {
				bltResult =
					clearSurface->lpVtbl->Blt(clearSurface, NULL, NULL, NULL, DDBLT_COLORFILL, &bltFx);
			} while (bltResult &&
					 (bltResult != DX_DDERR_SURFACELOST ||
					  !(displayResult = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface))) &&
					 bltResult == DX_DDERR_WASSTILLDRAWING);
			if (g_flightOffscreenSurface) {
				clearSurface = g_flightOffscreenSurface;
				bltFx.dwSize = 100;
				bltFx.dwFillColor = 0;
				do {
					bltResult =
						clearSurface->lpVtbl->Blt(clearSurface, NULL, NULL, NULL, DDBLT_COLORFILL, &bltFx);
				} while (
					bltResult &&
					(bltResult != DX_DDERR_SURFACELOST ||
					 !(displayResult = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface))) &&
					bltResult == DX_DDERR_WASSTILLDRAWING);
			}
		} else {
			g_flightBackBufferSurface = g_flightPrimarySurface;
			g_flightPrimarySurface->lpVtbl->AddRef(g_flightPrimarySurface);
		}
	} else {
		/* Windowed: plain primary + separate back/offscreen. */
		memset(&desc, 0, sizeof(desc));
		desc.dwSize = 108;
		desc.dwFlags = DDSD_CAPS;
		desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		displayResult = g_flightDirectDraw->lpVtbl->CreateSurface(g_flightDirectDraw, &desc,
																  &g_flightPrimarySurface, NULL);
		if (displayResult) {
			return FlightDisplay_ReportInitFailure(4);
		}
		memset(&desc, 0, sizeof(desc));
		desc.dwSize = 108;
		g_flightPrimarySurface->lpVtbl->GetSurfaceDesc(g_flightPrimarySurface, &desc);
		g_flightPrimaryPitch = desc.lPitch;
		if (desc.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8) {
			DebugPrintf("8-bit desktop mode not supported.");
		} else if (desc.ddpfPixelFormat.dwFlags & DDPF_RGB) {
			g_pixelFormatCode = (desc.ddpfPixelFormat.dwGBitMask & 0x400) != 0 ? 565 : 555;
		} else {
			DebugPrintf("non 16-bit desktop mode not supported.");
		}
		desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		if (g_useHardware3D) {
			desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;
		}
		desc.dwWidth = g_surfaceWidth;
		desc.dwHeight = g_surfaceHeight;
		displayResult = g_flightDirectDraw->lpVtbl->CreateSurface(g_flightDirectDraw, &desc,
																  &g_flightBackBufferSurface, NULL);
		if (displayResult) {
			return FlightDisplay_ReportInitFailure(5);
		}
		clearSurface = g_flightBackBufferSurface;
		bltFx.dwSize = 100;
		bltFx.dwFillColor = 0;
		do {
			bltResult = clearSurface->lpVtbl->Blt(clearSurface, NULL, NULL, NULL, DDBLT_COLORFILL, &bltFx);
		} while (bltResult &&
				 (bltResult != DX_DDERR_SURFACELOST ||
				  !(displayResult = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface))) &&
				 bltResult == DX_DDERR_WASSTILLDRAWING);
	}

	if (g_useHardware3D) {
		Renderer_InitD3DDevice(g_flightDirectDraw, g_flightBackBufferSurface);
		if (!g_useHardware3D) {
			/* Hardware device init dropped to software: release and re-init at 640x480. */
			if (g_flightOffscreenSurface) {
				g_flightOffscreenSurface->lpVtbl->Release(g_flightOffscreenSurface);
				g_flightOffscreenSurface = NULL;
			}
			if (g_flightBackBufferSurface) {
				if (g_flightFullscreen) {
					g_flightBackBufferSurface->lpVtbl->Release(g_flightBackBufferSurface);
				} else {
					g_flightBackBufferSurface->lpVtbl->Release(g_flightBackBufferSurface);
				}
				g_flightBackBufferSurface = NULL;
			}
			if (g_flightPrimarySurface) {
				g_flightPrimarySurface->lpVtbl->Release(g_flightPrimarySurface);
				g_flightPrimarySurface = NULL;
			}
			width = 640;
			height = 480;
			g_surfaceWidth = 640;
			g_surfaceHeight = 480;
			g_flightResolutionMode = 0;
			g_useHardware3D = 0;
			g_renderTargetWidth = 640;
			err = FlightDisplay_Init();
			g_flightSoftwareFallback = 1;
			return err;
		}
	} else {
		/* Software path: create the offscreen render surface. */
		desc.dwWidth = g_surfaceWidth;
		desc.dwHeight = g_surfaceHeight;
		desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		displayResult = g_flightDirectDraw->lpVtbl->CreateSurface(g_flightDirectDraw, &desc,
																  &g_flightOffscreenSurface, NULL);
		if (displayResult) {
			return FlightDisplay_ReportInitFailure(6);
		}
		if (g_flightOffscreenSurface) {
			clearSurface = g_flightOffscreenSurface;
			bltFx.dwSize = 100;
			bltFx.dwFillColor = 0;
			do {
				bltResult =
					clearSurface->lpVtbl->Blt(clearSurface, NULL, NULL, NULL, DDBLT_COLORFILL, &bltFx);
			} while (bltResult &&
					 (bltResult != DX_DDERR_SURFACELOST ||
					  !(displayResult = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface))) &&
					 bltResult == DX_DDERR_WASSTILLDRAWING);
		}
	}

	if (!g_flightFullscreen) {
		/* Return to a windowed cooperative level for the Blt-present path. */
		displayResult = g_flightDirectDraw->lpVtbl->SetCooperativeLevel(g_flightDirectDraw, hwnd, 0x808);
		if (displayResult) {
			displayResult = g_flightDirectDraw->lpVtbl->SetCooperativeLevel(g_flightDirectDraw, hwnd, 8);
			if (displayResult) {
				return FlightDisplay_ReportInitFailure(7);
			}
			g_flightDisplayCoopLevelFallbackUsed = 1;
		}
	}
	return 1;
}

// FUNCTION: XWA 0x50C740
int FlightDisplay_RunRestrictedOptionsModal(void) {
	int finished;
	int result;

	if (!g_flightFrontendModalActive) {
		DInput_DrainKeyboardEvents();
		if (!FlightDisplay_AttachFrontendModalSurfaces()) {
			return 0;
		}
		result = Config_RunRestrictedOptionsModal();
		if (FrontendScreen_GetModalStatus() == FRONTEND_SCREEN_MODAL_FAILED) {
			FlightDisplay_DetachFrontendModalSurfaces();
			return result;
		}
	}

	finished = FlightDisplay_TickFrontendModal();
	result = Config_RunRestrictedOptionsModal();
	if (!finished && FrontendScreen_GetModalStatus() != FRONTEND_SCREEN_MODAL_INACTIVE) {
		return 0;
	}

	FlightDisplay_DetachFrontendModalSurfaces();
	return result;
}

// FUNCTION: XWA 0x50CA90
char* FlightFilm_RunNamePrompt(void) {
	int finished;
	char* filmName;

	if (!g_flightFrontendModalActive) {
		DInput_DrainKeyboardEvents();
		if (!FlightDisplay_AttachFrontendModalSurfaces()) {
			return NULL;
		}
		filmName = FilmNamePrompt_Run();
		if (FrontendScreen_GetModalStatus() == FRONTEND_SCREEN_MODAL_FAILED) {
			FlightDisplay_DetachFrontendModalSurfaces();
			return filmName;
		}
	}

	finished = FlightDisplay_TickFrontendModal();
	filmName = FilmNamePrompt_Run();
	if (!finished && FrontendScreen_GetModalStatus() != FRONTEND_SCREEN_MODAL_INACTIVE) {
		return NULL;
	}

	FlightDisplay_DetachFrontendModalSurfaces();
	return filmName;
}

// FUNCTION: XWA 0x50A420
int FlightFilm_SaveTempRecordingWithPrompt(void) {
	char filmPath[256];
	char* filmName;

	filmName = FlightFilm_RunNamePrompt();
	if (filmName == NULL) {
		return 0;
	}
	if (filmName[0] == '\0') {
		return 0;
	}

	sprintf(filmPath, "film\\%s.flm", filmName);
#ifdef XWA_MODERN
	File_Remove(AERON_VFS_ROOT_USER, filmPath);
	File_Rename(AERON_VFS_ROOT_USER, "film\\tempfilm.tmp", filmPath);
#else
	remove(filmPath);
	rename("film\\tempfilm.tmp", filmPath);
#endif
	return 1;
}

// FUNCTION: XWA 0x5048B0
void Flight_ApplyGraphicsDetailPreset(uint16_t graphicsDetailPreset) {
	g_gameConfig.yardLod[g_flightPlayerCount > 1] = (uint8_t)g_yardLodByGraphicsDetail[graphicsDetailPreset];
	g_debrisDensityLevel = g_debrisDensityByGraphicsDetail[graphicsDetailPreset];
	g_starDensity = g_starDensityByGraphicsDetail[graphicsDetailPreset];
	g_backdropsEnabled = (uint8_t)g_backdropsByGraphicsDetail[graphicsDetailPreset];
	g_debrisEnabled = (uint8_t)g_debrisByGraphicsDetail[graphicsDetailPreset];

	if ((uint16_t)Renderer_IsTextureClampSupported() &&
		(uint8_t)g_hitEffectsByGraphicsDetail[graphicsDetailPreset]) {
		g_hitEffectsEnabled = 1;
	} else {
		g_hitEffectsEnabled = 0;
	}

	g_particleEffectsEnabled = (uint8_t)g_particleEffectsByGraphicsDetail[graphicsDetailPreset];
	g_trailsEnabled = (uint8_t)g_trailsByGraphicsDetail[graphicsDetailPreset];
	g_localLightsLevel = g_localLightsByGraphicsDetail[graphicsDetailPreset];

	if (!g_gameConfig.use3dHardware[g_flightPlayerCount > 1]) {
		g_specularEnabled = g_specularByGraphicsDetail[graphicsDetailPreset];
	} else {
		g_specularEnabled = 0;
	}

	g_dirLightingEnabled = g_dirLightingByGraphicsDetail[graphicsDetailPreset];
	g_gameConfig.debrisDensity[g_flightPlayerCount > 1] = (uint8_t)g_debrisDensityLevel;

	switch (g_starDensity) {
		case 0:
		case 1:
			g_gameConfig.starDensity[g_flightPlayerCount > 1] = 0;
			break;
		case 2:
		case 3:
			g_gameConfig.starDensity[g_flightPlayerCount > 1] = 1;
			break;
		case 4:
			g_gameConfig.starDensity[g_flightPlayerCount > 1] = 2;
			break;
		default:
			break;
	}

	g_gameConfig.backdrop[g_flightPlayerCount > 1] = (uint8_t)g_backdropsEnabled;
	g_gameConfig.debris[g_flightPlayerCount > 1] = (uint8_t)g_debrisEnabled;
	g_gameConfig.hitEffects[g_flightPlayerCount > 1] = (uint8_t)g_hitEffectsEnabled;
	g_gameConfig.particleEffects[g_flightPlayerCount > 1] = (uint8_t)g_particleEffectsEnabled;
	g_gameConfig.trails[g_flightPlayerCount > 1] = (uint8_t)g_trailsEnabled;
	g_gameConfig.engineGlow[g_flightPlayerCount > 1] =
		(uint8_t)g_engineGlowByGraphicsDetail[graphicsDetailPreset];
	g_gameConfig.lensFlare[NetSession_GetPlayerCount() > 1] =
		(uint8_t)g_lensFlareByGraphicsDetail[graphicsDetailPreset];
	g_gameConfig.localLights[g_flightPlayerCount > 1] = (uint8_t)g_localLightsLevel;
	g_gameConfig.specular[g_flightPlayerCount > 1] = (uint8_t)g_specularEnabled;
	g_gameConfig.diffuse[g_flightPlayerCount > 1] = (uint8_t)g_dirLightingEnabled;
}

// FUNCTION: XWA 0x4348C0
int FlightRender_InstallCallbacks(uint8_t initialGraphicsDetailPreset) {
	int result;

	g_palettePackedMode = 2;
	g_flightInitLineBufferFn = FlightSw_InitLineBuffer;
	g_flightDebugPrintFn = DebugPrintf;
	g_flightResetPaletteFn = FlightPalette_Reset;
	g_flightSetPaletteRangeFn = FlightPalette_SetRange;
	g_flightGetPaletteFn = FlightPalette_GetFull;
	g_flightSetPaletteFn = FlightPalette_SetFull;
	g_flightComputePixelOffsetFn = FlightSw_ComputePixelOffset;
	g_flightBlitSpriteFn = FlightSw_BlitSpriteRle;
	g_flightBlitSpriteFadedFn = FlightSw_BlitSpriteRleFaded;
	g_flightDrawCharFn = FlightText_DrawHardwareGlyph;
	if (!g_useHardware3D) {
		g_flightDrawCharFn = FlightText_DrawSoftwareGlyph;
	}
	g_flightFillClipRectFn = FlightSw_FillClipRect;
	g_flightFillRectClippedFn = FlightSw_FillRectClipped;
	g_flightSaveScreenRectFn = FlightSw_SaveScreenRect;
	g_flightRestoreScreenRectFn = FlightSw_RestoreScreenRect;
	g_flightDrawPointArrayFn = FlightSw_DrawPointArray;
	g_flightDrawPointArrayMaskedFn = FlightSw_DrawPointArrayMasked;
	g_flightDrawPixelFn = FlightSw_DrawPixel;
	g_flightDrawRadarTargetMarkerFn = FlightSw_DrawRadarTargetMarker;
	g_flightRestoreRadarTargetMarkerFn = FlightSw_RestoreRadarTargetMarker;
	g_flightDrawLineFn = FlightSw_DrawLine;

	result = SetFlightViewport((unsigned int)g_screenWidth, (unsigned int)g_screenHeight, 1, 0);
	g_flightGraphicsDetailPreset = initialGraphicsDetailPreset;
	return result;
}

// FUNCTION: XWA 0x50D050
int FlightDisplay_GetPrimarySurfacePitch(void) { return g_flightPrimaryPitch; }

// FUNCTION: XWA 0x50D780
int FlightDisplay_Flip(void) {
	HRESULT result;
	HRESULT flipResult;
	HRESULT bltResult;
	DDBLTFX fx;
	uint32_t dst[4];
	uint32_t src[4];
	int32_t vblank;
	int count;

	if (g_flightPageFlip) {
		/* Optional flicker sync (flicker.txt) waits for the vertical blank;
		   GetVerticalBlankStatus returns in-vblank immediately under Aeron. */
		if (g_flightConfFlicker) {
			if (!g_flightFlickerLastSyncTimeMs) {
				if (!g_flightDirectDraw->lpVtbl->GetVerticalBlankStatus(g_flightDirectDraw, &vblank)) {
					while (vblank &&
						   !g_flightDirectDraw->lpVtbl->GetVerticalBlankStatus(g_flightDirectDraw, &vblank)) {
					}
					while (!vblank &&
						   !g_flightDirectDraw->lpVtbl->GetVerticalBlankStatus(g_flightDirectDraw, &vblank)) {
					}
				}
				g_flightFlickerLastSyncTimeMs = timeGetTime();
				if (g_flightDirectDraw->lpVtbl->GetMonitorFrequency(
						g_flightDirectDraw, (uint32_t*)&g_flightFlickerRefreshRateScale)) {
					count = 100;
					do {
						while (vblank && !g_flightDirectDraw->lpVtbl->GetVerticalBlankStatus(
											 g_flightDirectDraw, &vblank)) {
						}
						while (!vblank && !g_flightDirectDraw->lpVtbl->GetVerticalBlankStatus(
											  g_flightDirectDraw, &vblank)) {
						}
						--count;
					} while (count);
					g_flightFlickerRefreshRateScale =
						10000000 / (int)(timeGetTime() - g_flightFlickerLastSyncTimeMs);
				}
			} else {
				int phase =
					(int)(g_flightFlickerRefreshRateScale * (timeGetTime() - g_flightFlickerLastSyncTimeMs)) %
					100000;
				if ((phase < g_flightFlickerPhaseWindow || phase > 100000 - g_flightFlickerPhaseWindow) &&
					!g_flightDirectDraw->lpVtbl->GetVerticalBlankStatus(g_flightDirectDraw, &vblank) &&
					!vblank) {
					while (!vblank &&
						   !g_flightDirectDraw->lpVtbl->GetVerticalBlankStatus(g_flightDirectDraw, &vblank)) {
					}
					g_flightFlickerLastSyncTimeMs = timeGetTime();
				}
			}
		}

		result = g_flightPrimarySurface->lpVtbl->Flip(g_flightPrimarySurface, NULL, 1);
		if (result == (HRESULT)-2005532447) { /* DDERR_NOEXCLUSIVEMODE: re-acquire the mode */
			if (g_flightDirectDraw->lpVtbl->SetCooperativeLevel(g_flightDirectDraw, hwnd, 0x851)) {
				if (g_flightDirectDraw->lpVtbl->SetCooperativeLevel(g_flightDirectDraw, hwnd, 0x51)) {
					/* original debug break on double failure */
				}
				g_flightDisplayCoopLevelFallbackUsed = 1;
			}
			flipResult = g_flightPrimarySurface->lpVtbl->Flip(g_flightPrimarySurface, NULL, 1);
			if (flipResult == DX_DDERR_SURFACELOST) {
				g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface);
				g_flightBackBufferSurface->lpVtbl->Restore(g_flightBackBufferSurface);
				if (std3D_GetZBufferSurface()) {
					((IDirectDrawSurface*)std3D_GetZBufferSurface())
						->lpVtbl->Restore((IDirectDrawSurface*)std3D_GetZBufferSurface());
				}
				flipResult = g_flightPrimarySurface->lpVtbl->Flip(g_flightPrimarySurface, NULL, 1);
			}
			if (flipResult) {
				/* original debug break on failure */
			}
			result = g_flightDirectDraw->lpVtbl->SetCooperativeLevel(g_flightDirectDraw, hwnd, 0x808);
			if (result) {
				result = g_flightDirectDraw->lpVtbl->SetCooperativeLevel(g_flightDirectDraw, hwnd, 8);
				if (result) {
					/* original debug break on double failure */
				}
				g_flightDisplayCoopLevelFallbackUsed = 1;
			}
		}
		if (result == DX_DDERR_SURFACELOST) {
			return g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface);
		}
	} else {
		/* Non-page-flip: SRCCOPY Blt the back buffer to the centered primary. */
		memset(&fx, 0, sizeof(fx));
		fx.dwSize = 100;
		fx.dwROP = 0x00CC0020; /* SRCCOPY */
		do {
			dst[0] = (uint32_t)(width - g_surfaceWidth) >> 1;
			dst[1] = (uint32_t)(height - g_surfaceHeight) >> 1;
			dst[2] = g_surfaceWidth + dst[0];
			dst[3] = g_surfaceHeight + dst[1];
			src[0] = 0;
			src[1] = 0;
			src[2] = g_surfaceWidth;
			src[3] = g_surfaceHeight;
			bltResult = g_flightPrimarySurface->lpVtbl->Blt(g_flightPrimarySurface, dst,
															g_flightBackBufferSurface, src, 0x20000, &fx);
			result = bltResult;
		} while (bltResult &&
				 (bltResult != DX_DDERR_SURFACELOST ||
				  !(result = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface))) &&
				 bltResult == DX_DDERR_WASSTILLDRAWING);
	}
	return result;
}

// FUNCTION: XWA 0x50DAA0
int FlightDisplay_BlitRenderSurface(void) {
	HRESULT result;
	HRESULT bltResult;
	int32_t dst[4];
	int32_t src[4];
	DDBLTFX fx;

	result = (HRESULT)(intptr_t)g_flightOffscreenSurface;
	if (result) {
		/* DirectDraw uses distinct paths for page-flipped and blitted displays. */
		if (g_flightPageFlip) {
			memset(&fx, 0, sizeof(fx));
			fx.dwSize = 100;
			fx.dwROP = 0x00CC0020; /* SRCCOPY */
			do {
				dst[0] = ((unsigned int)(width - g_surfaceWidth)) >> 1;
				dst[1] = ((unsigned int)(height - g_surfaceHeight)) >> 1;
				dst[2] = g_surfaceWidth + dst[0];
				dst[3] = g_surfaceHeight + dst[1];
				src[0] = 0;
				src[1] = 0;
				src[2] = g_surfaceWidth;
				src[3] = g_surfaceHeight;
				bltResult = g_flightBackBufferSurface->lpVtbl->Blt(
					g_flightBackBufferSurface, dst, g_flightOffscreenSurface, src, 0x20000, &fx);
				result = bltResult;
			} while (bltResult &&
					 (bltResult != DX_DDERR_SURFACELOST ||
					  !(result = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface))) &&
					 bltResult == DX_DDERR_WASSTILLDRAWING);
		} else {
			memset(&fx, 0, sizeof(fx));
			fx.dwSize = 100;
			fx.dwROP = 0x00CC0020; /* SRCCOPY */
			do {
				dst[0] = ((unsigned int)(width - g_surfaceWidth)) >> 1;
				dst[1] = ((unsigned int)(height - g_surfaceHeight)) >> 1;
				dst[2] = g_surfaceWidth + dst[0];
				dst[3] = g_surfaceHeight + dst[1];
				src[0] = 0;
				src[1] = 0;
				src[2] = g_surfaceWidth;
				src[3] = g_surfaceHeight;
				bltResult = g_flightBackBufferSurface->lpVtbl->Blt(
					g_flightBackBufferSurface, dst, g_flightOffscreenSurface, src, 0x20000, &fx);
				result = bltResult;
			} while (bltResult &&
					 (bltResult != DX_DDERR_SURFACELOST ||
					  !(result = g_flightPrimarySurface->lpVtbl->Restore(g_flightPrimarySurface))) &&
					 bltResult == DX_DDERR_WASSTILLDRAWING);
		}
	}
	return result;
}

// FUNCTION: XWA 0x4CC8E0
void FlightSurface_ClearToBlack(void) {
	FlightSurface_Lock();
	FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
	g_flightTextBgColor = 0;
	g_flightFillClipRectFn();
	FlightSurface_Unlock();
}

// FUNCTION: XWA 0x511A90
void FlightAlert_SaveBoxBackground(void) {
	unsigned int boxX;
	unsigned int boxY;
	unsigned int boxWidth;
	int boxTextHeight;
	int boxHeight;
	int pixelCount;
	int byteCount;

	FlightText_SetFontTier(0);
	boxWidth = ((unsigned int)(g_surfaceWidth - g_flightRenderModeId) >> 1) + 2u;
	boxX = ((unsigned int)(g_surfaceWidth + g_flightRenderModeId) >> 1) -
		   (unsigned int)((int)((unsigned int)(g_surfaceWidth - g_flightRenderModeId) >> 1) / 2) - 1u;
	boxTextHeight = 5 * g_flightFontLineHeight;
	boxHeight = boxTextHeight + 2;
	boxY = ((unsigned int)(g_surfaceHeight + g_flightAlertBoxVerticalOffset) >> 1) -
		   (unsigned int)(boxTextHeight / 2) - 1u;

	pixelCount = (int)boxWidth * boxHeight;
	byteCount = pixelCount * g_flight16bppBytesPerPixel;
	if (g_flightAlertBoxSavedPixels == NULL) {
		g_flightAlertBoxSavedPixels = (int16_t*)Memory_AllocTagged("ALERTBOXBUFFER", (size_t)byteCount);
		if (g_flightAlertBoxSavedPixels == NULL) {
			return;
		}
		g_flightAlertBoxSavedBytes = byteCount;
	} else if (g_flightAlertBoxSavedBytes < byteCount) {
		Memory_FreeTagged("ALERTBOXBUFFER", g_flightAlertBoxSavedPixels);
		g_flightAlertBoxSavedPixels = (int16_t*)Memory_AllocTagged("ALERTBOXBUFFER", (size_t)byteCount);
		if (g_flightAlertBoxSavedPixels == NULL) {
			return;
		}
		g_flightAlertBoxSavedBytes = byteCount;
	}

	FlightDisplay_Flip();
	FlightSurface_Lock();
	g_flightSaveScreenRectFn(g_flightAlertBoxSavedPixels, (uint16_t)boxX, (uint16_t)boxY,
							 (int16_t)(uint16_t)boxWidth, (uint16_t)boxHeight);
	FlightSurface_Unlock();
	FlightDisplay_Flip();
}

// FUNCTION: XWA 0x511BE0
void FlightAlert_RestoreBoxBackground(void) {
	int boxX;
	int boxY;
	unsigned int boxWidth;
	int boxHeight;

	FlightText_SetFontTier(0);
	boxWidth = (unsigned int)(g_surfaceWidth - g_flightRenderModeId) >> 1;
	boxHeight = 5 * g_flightFontLineHeight;
	boxX = (int)((unsigned int)(g_surfaceWidth + g_flightRenderModeId) >> 1) - (int)boxWidth / 2 - 1;
	boxWidth += 2u;
	boxY = (int)((unsigned int)(g_flightAlertBoxVerticalOffset + g_surfaceHeight) >> 1) - boxHeight / 2 - 1;
	boxHeight += 2;

	if (g_flightAlertBoxSavedPixels != NULL) {
		FlightDisplay_Flip();
		FlightSurface_Lock();
		g_flightRestoreScreenRectFn(g_flightAlertBoxSavedPixels, (uint16_t)boxX, (uint16_t)boxY,
									(uint16_t)boxWidth, (uint16_t)boxHeight);
		FlightSurface_Unlock();
		FlightDisplay_Flip();
	}
}

// FUNCTION: XWA 0x511C90
void FlightAlert_DrawBox(int verticalMode, char* line1, char* line2, uint8_t bgColor) {
	int boxWidth;
	unsigned int boxX;
	int boxHeight;
	unsigned int boxY;
	int textLine;
	int lineHeight;

	FlightText_SetFontTier(0);
	boxWidth = (int)((unsigned int)(g_surfaceWidth - g_flightRenderModeId) >> 1);
	boxX = ((unsigned int)(g_surfaceWidth + g_flightRenderModeId) >> 1) - (unsigned int)(boxWidth / 2);
	boxHeight = 5 * g_flightFontLineHeight;
	boxY = ((unsigned int)(g_flightAlertBoxVerticalOffset + g_surfaceHeight) >> 1) -
		   (unsigned int)(boxHeight / 2);

	if (g_flightAlertBoxSavedPixels == NULL) {
		return;
	}

	FlightText_SetColor(0x2fu);
	FlightText_SetBackgroundColor((uint32_t)bgColor + 2u);
	g_flightTextShadowEnabled = 1;
	FlightText_SetShadowColor(0x2cu);
	FlightDisplay_Flip();
	FlightSurface_Lock();

	if (verticalMode == 1) {
		FlightText_SetClipRect((int16_t)(boxX - 1u), (int16_t)(boxY - 1u),
							   (uint16_t)(boxX + (unsigned int)boxWidth + 1u),
							   (uint16_t)(boxY + (unsigned int)boxHeight + 1u));
		g_flightFillClipRectFn();
		verticalMode = 0;
	}

	FlightText_SetBackgroundColor(bgColor);
	boxHeight += (int)boxY;
	boxWidth += (int)boxX;
	FlightText_SetClipRect((int16_t)boxX, (int16_t)boxY, (uint16_t)boxWidth, (uint16_t)boxHeight);
	g_flightFillClipRectFn();

	if (verticalMode == 0) {
		verticalMode = 1;
	}
	textLine = verticalMode;

	lineHeight = g_flightFontLineHeight;
	FlightText_SetCursor((int16_t)(boxX + lineHeight), (int16_t)(boxY + textLine * lineHeight));
	FlightText_DrawStringCentered(line1);

	if (line2 != NULL) {
		lineHeight = g_flightFontLineHeight;
		FlightText_SetCursor((int16_t)(boxX + lineHeight), (int16_t)(boxY + (textLine + 1) * lineHeight));
		FlightText_DrawStringCentered(line2);
	}

	FlightSurface_Unlock();
	if (g_useHardware3D) {
		RenderScene_Initialize(1);
		FlightText_FlushQueue();
		RenderScene_DrawVisibleFaces();
	}
	FlightDisplay_Flip();
}

// FUNCTION: XWA 0x4D4650
int FlightScreenshot_Capture(void) {
	char fileName[64];
	uint8_t palette[1024];
	int fileIdx;
	int lockCount;
	int remainingLocks;
	int savedLockBackBufferForHudDraw;
	int result;
	XwaFile* stream;
	int i;

	fileIdx = 0;
	for (;;) {
		sprintf(fileName, "flightscreen%d.bmp", fileIdx);
		stream = File_Open(AERON_VFS_ROOT_USER, fileName, "rb");
		g_stream = stream;
		if (g_stream == NULL) {
			break;
		}
		File_Close(g_stream);
		g_stream = NULL;
		++fileIdx;
	}

	for (i = 0; i < 256; ++i) {
		palette[4 * i + 0] = (uint8_t)(g_swPalette[i].b << 2);
		palette[4 * i + 1] = (uint8_t)(g_swPalette[i].g << 2);
		palette[4 * i + 2] = (uint8_t)(g_swPalette[i].r << 2);
		palette[4 * i + 3] = 0;
	}

	lockCount = FlightSurface_GetLockCount();
	if (lockCount > 0) {
		remainingLocks = lockCount;
		do {
			FlightSurface_Unlock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}

	FlightDisplay_Flip();
	savedLockBackBufferForHudDraw = g_flightLockBackBufferForHudDraw;
	g_flightLockBackBufferForHudDraw = 0;
	FlightSurface_Lock();
	FrontImage_SaveBmpFile(fileName, g_surfacePixels, g_surfaceWidth, g_surfaceHeight, g_surfacePitch,
						   8 * g_flight16bppBytesPerPixel, Display_IsPixelFormat555(), palette);
	result = FlightSurface_Unlock();
	g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

	if (lockCount > 0) {
		do {
			result = FlightSurface_Lock();
			--lockCount;
		} while (lockCount != 0);
	}

	return result;
}

/* Locks `surface` (retrying on DDERR_WASSTILLDRAWING), publishes its pixels to
 * the software framebuffer base and g_surfacePixels, reads its pitch into
 * g_flightPrimaryPitch, optionally centers the 640x480 viewport within it, and
 * sets the 480-line byte span. Inlined by FlightSurface_Lock at each lock site
 * in the original; factored here for clarity. Returns the lock HRESULT (0 = ok). */
static HRESULT FlightSurface_LockAndCenter(IDirectDrawSurface* surface, uint32_t lockFlags, int center) {
	DDSURFACEDESC desc;
	HRESULT hr;
	int offset;

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = 108;
	while (1) {
		hr = surface->lpVtbl->Lock(surface, NULL, &desc, lockFlags, NULL);
		if (!hr) {
			break;
		}
		if (hr != DX_DDERR_WASSTILLDRAWING) {
			return hr;
		}
	}
	FlightSurface_SetSoftwareFramebufferBase(desc.lpSurface);
	g_flightSwFramebufferBase = desc.lpSurface;
	g_surfacePixels = desc.lpSurface;

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = 108;
	surface->lpVtbl->GetSurfaceDesc(surface, &desc);
	g_flightPrimaryPitch = desc.lPitch;
	if (center) {
		offset = desc.lPitch * ((height - g_surfaceHeight) >> 1) +
				 g_flight16bppBytesPerPixel * ((width - g_surfaceWidth) >> 1);
		g_flightSwFramebufferBase = (uint8_t*)g_flightSwFramebufferBase + offset;
		g_surfacePixels = (uint8_t*)g_surfacePixels + offset;
	}
	FlightSurface_SetViewport480ByteSpan(480 * desc.lPitch);
	return 0;
}

// FUNCTION: XWA 0x50D070
int FlightSurface_Lock(void) {
	int result;

	if (g_flightRenderToFrontend == 1) {
		g_flightSwFramebufferBase = FrontendDisplay_LockSurfaceForFlight();
		g_surfacePixels = g_flightSwFramebufferBase;
		result = FrontendDisplay_GetFrontendOrFlightDrawPitch();
		g_surfacePitch = result;
		return result;
	}

	if (g_surfaceLockCount != 0) {
		return ++g_surfaceLockCount;
	}
	g_surfaceLockCount = 1;

	if (!g_flightFullscreen) {
		if (g_flightDisplaySurfacesActive) {
			if (g_useHardware3D || !g_flightLockBackBufferForHudDraw) {
				result = FlightSurface_LockAndCenter(g_flightBackBufferSurface, 2048, 1);
			} else {
				result = FlightSurface_LockAndCenter(g_flightOffscreenSurface, 2048, 0);
			}
			if (result) {
				return result;
			}
			if (g_surfacePitch != g_flightPrimaryPitch) {
				g_surfacePitch = g_flightPrimaryPitch;
				FlightSw_SetRenderTarget(g_flightSwFramebufferBase, g_flightPrimaryPitch, 0x4B0u, -1);
			}
		} else {
			result = FlightSurface_LockAndCenter(g_flightPrimarySurface, 0, 1);
			if (result) {
				return result;
			}
		}
	}

	result = g_flightPageFlip;
	if (!g_flightPageFlip) {
		return result;
	}

	if (g_flightDisplaySurfacesActive) {
		if (g_useHardware3D || !g_flightLockBackBufferForHudDraw) {
			result = FlightSurface_LockAndCenter(g_flightBackBufferSurface, 0, 1);
		} else {
			result = FlightSurface_LockAndCenter(g_flightOffscreenSurface, 0, 0);
		}
		if (result) {
			return result;
		}
		if (g_surfacePitch != g_flightPrimaryPitch) {
			g_surfacePitch = g_flightPrimaryPitch;
			FlightSw_SetRenderTarget(g_flightSwFramebufferBase, g_flightPrimaryPitch, 0x4B0u, -1);
		}
		return g_flightPrimaryPitch;
	}

	return FlightSurface_LockAndCenter(g_flightPrimarySurface, 0, 1);
}

// FUNCTION: XWA 0x50D6A0
int FlightSurface_Unlock(void) {
	IDirectDrawSurface* surface;
	int result;

	if (g_flightRenderToFrontend == 1) {
		return FrontendDisplay_UnlockSurfaceForFlight();
	}

	result = g_surfaceLockCount;
	if (g_surfaceLockCount > 1) {
		return --g_surfaceLockCount;
	}
	if (g_surfaceLockCount < 1) {
		g_surfaceLockCount = 0;
		return result;
	}

	--g_surfaceLockCount;
	if (g_flightPageFlip) {
		if (g_flightDisplaySurfacesActive) {
			surface = (g_useHardware3D || !g_flightLockBackBufferForHudDraw) ? g_flightBackBufferSurface
																			 : g_flightOffscreenSurface;
		} else {
			surface = g_flightPrimarySurface;
		}
		surface->lpVtbl->Unlock(surface, g_surfacePixels);
	}

	result = g_flightFullscreen;
	if (!g_flightFullscreen) {
		if (g_flightDisplaySurfacesActive) {
			if (g_useHardware3D || !g_flightLockBackBufferForHudDraw) {
				result =
					g_flightBackBufferSurface->lpVtbl->Unlock(g_flightBackBufferSurface, g_surfacePixels);
			} else {
				result = g_flightOffscreenSurface->lpVtbl->Unlock(g_flightOffscreenSurface, g_surfacePixels);
			}
		} else {
			result = g_flightPrimarySurface->lpVtbl->Unlock(g_flightPrimarySurface, g_surfacePixels);
		}
	}
	return result;
}

// FUNCTION: XWA 0x50D060
int FlightSurface_GetLockCount(void) { return g_surfaceLockCount; }

// FUNCTION: XWA 0x50E470
void* FlightSurface_GetSoftwareFramebufferBase(void) { return g_swFramebufferBase; }

// FUNCTION: XWA 0x50E450
int FlightSurface_SetViewport480ByteSpan(int byteSpan) {
	g_flightSurfaceViewport480ByteSpan = byteSpan;
	return byteSpan;
}

// FUNCTION: XWA 0x50E460
void* FlightSurface_SetSoftwareFramebufferBase(void* framebufferBase) {
	g_swFramebufferBase = framebufferBase;
	return framebufferBase;
}

// FUNCTION: XWA 0x4D3DE0
void FlightSw_InitLineBuffer(void) {
	unsigned int y;
	unsigned int i;
	unsigned int remainder;

	for (y = 0; y < g_screenHeight; ++y) {
		g_flightLineOffsetTable[y] = y * g_surfacePitch;
	}

	g_flightSwFramebufferBase = FlightSurface_GetSoftwareFramebufferBase();

	if (g_flightResolutionMode >= 0 && g_flightResolutionMode <= 5) {
		for (i = 0; i < g_surfacePitch * g_screenHeight / g_swFramebufferClearChunkSize; ++i) {
			memset(g_flightSwFramebufferBase, 0, g_swFramebufferClearChunkSize);
		}
		remainder = g_surfacePitch * g_screenHeight % g_swFramebufferClearChunkSize;
		if (remainder) {
			memset(g_flightSwFramebufferBase, 0, remainder);
		}
		g_flightSwFramebufferClearRunPtr = g_flightSwFramebufferClearRun1;
		g_flightSwFramebufferClearRunCount = 12;
	}

	g_flightLinePitchPtr = &g_surfacePitch;
	g_flightLineBufferTable = g_flightLineOffsetTable;
}

// FUNCTION: XWA 0x4D3ED0
void FlightSw_SetRenderTarget(void* surface, int width, unsigned int height, int pitchBytes) {
	uint16_t y;

	if (surface == NULL) {
		g_flightSwFramebufferBase = FlightSurface_GetSoftwareFramebufferBase();
		g_flightLinePitchPtr = &g_surfacePitch;
		g_flightLineBufferTable = g_flightLineOffsetTable;
		return;
	}

	g_flightSwFramebufferBase = surface;
	if (pitchBytes == -1) {
		for (y = 0; y < height; ++y) {
			g_flightLineOffsetTable[y] = (int)y * g_surfacePitch;
		}
		g_flightLinePitchPtr = &g_surfacePitch;
		g_flightLineBufferTable = g_flightLineOffsetTable;
		return;
	}

	g_surfacePixels = surface;
	for (y = 0; y < height; ++y) {
		g_flightAltLineOffsetTable[y] = (int)y * g_flight16bppBytesPerPixel * width;
	}
	g_flightAltLinePitch = pitchBytes;
	g_flightLinePitchPtr = &g_flightAltLinePitch;
	g_flightLineBufferTable = g_flightAltLineOffsetTable;
}

// FUNCTION: XWA 0x494760
void FlightSw_SetRotatedSpriteDestBuffer(void* destBuffer) { g_flightSwRotSpriteDestBuffer = destBuffer; }

// FUNCTION: XWA 0x4CF980
char FlightSw_DrawRotatedSpriteQuad(int16_t screenX, int16_t screenY, int16_t screenSize, Sprite* sprite) {
	(void)screenX;
	(void)screenY;
	(void)screenSize;
	(void)sprite;
	/* TODO: Reimplement FlightSw_DrawRotatedSpriteQuad @ 0x4CF980. */
	return 0;
}

// FUNCTION: XWA 0x4CFD60
int FlightSw_PrepareSpriteRotationTables(int16_t rotationAngle, ...) {
	(void)rotationAngle;
	/* TODO: Reimplement FlightSw_PrepareSpriteRotationTables @ 0x4CFD60. */
	return 0;
}

// FUNCTION: XWA 0x4CFE20
char* FlightSw_BuildSpriteTintRemapTables(Sprite* sprite) {
	(void)sprite;
	/* TODO: Reimplement FlightSw_BuildSpriteTintRemapTables @ 0x4CFE20. */
	return NULL;
}

// FUNCTION: XWA 0x494AD0
void Blit16ToFlightSurface(const void* srcPixels, uint16_t colorKeyPaletteIndex, uint16_t srcX, uint16_t srcY,
						   uint16_t dstX, uint16_t dstY, uint16_t blitWidth, uint16_t blitHeight,
						   uint16_t srcPitchBytes) {
	int colorKey;
	uint8_t* dstRow;
	const uint8_t* srcRow;

	colorKey = g_flightTextPalette[colorKeyPaletteIndex];
	dstRow = (uint8_t*)g_flightSwFramebufferBase + dstX * g_flight16bppBytesPerPixel + dstY * g_surfacePitch;
	srcRow = (const uint8_t*)srcPixels + srcX * g_flight16bppBytesPerPixel + srcY * srcPitchBytes;

	if (colorKeyPaletteIndex == 0xffffu) {
		int rowsRemaining;
		int copyWidth;
		int copyHeight;

		copyHeight = blitHeight;
		if (copyHeight <= 0) {
			return;
		}

		rowsRemaining = copyHeight;
		copyWidth = blitWidth;
		do {
			int copyBytes;

			copyBytes = g_flight16bppBytesPerPixel * copyWidth;
			memcpy(dstRow, srcRow, copyBytes);
			dstRow += g_surfacePitch;
			srcRow += srcPitchBytes;
			--rowsRemaining;
		} while (rowsRemaining != 0);
		return;
	}

	{
		int rowsRemaining;
		int copyWidth;
		int copyHeight;

		copyHeight = blitHeight;
		if (copyHeight <= 0) {
			return;
		}

		rowsRemaining = copyHeight;
		copyWidth = blitWidth;
		do {
			if (copyWidth > 0) {
				int pixelsRemaining;

				pixelsRemaining = copyWidth;
				do {
					uint16_t pixel;

					pixel = *(const uint16_t*)srcRow;
					if ((uint16_t)colorKey != pixel) {
						*(uint16_t*)dstRow = pixel;
					}
					srcRow += g_flight16bppBytesPerPixel;
					dstRow += g_flight16bppBytesPerPixel;
					--pixelsRemaining;
				} while (pixelsRemaining != 0);
			}
			{
				int copiedBytes;

				copiedBytes = g_flight16bppBytesPerPixel * copyWidth;
				dstRow += g_surfacePitch - copiedBytes;
				srcRow += srcPitchBytes - copiedBytes;
			}
			--rowsRemaining;
		} while (rowsRemaining != 0);
	}
}

// FUNCTION: XWA 0x4D4500
char FlightPalette_Reset(void) {
	RgbTriplet dstRgb[256];

	FlightPalette_BuildRgbRange(g_swPalette, dstRgb, 0, 256);
	g_paletteDirtyFlags = (uint8_t)(g_paletteDirtyFlags & 0xfeu);
	return (char)g_paletteDirtyFlags;
}

// FUNCTION: XWA 0x4D4070
void FlightPalette_BuildRgbRange(RgbTriplet* srcRgb, RgbTriplet* dstRgb, int startIndex, int count) {
	int remaining;
	RgbTriplet* dst;
	uint8_t* srcByte;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t maxChannel;
	uint8_t minChannel;
	uint8_t saturation6;
	uint8_t hueSector;
	uint8_t hueOffset6;
	uint8_t value6;
	uint8_t lowChannel;
	uint8_t offsetChannel;
	uint8_t inverseOffsetChannel;
	int byteOffset;

	if (g_flightBrightnessScaleQ8 == 256) {
		if (count == 0) {
			return;
		}
#ifdef XWA_MODERN
		{
			RgbTriplet* src;

			/* The original x86 loop uses a 32-bit pointer delta that truncates on 64-bit hosts. */
			src = &srcRgb[startIndex];
			dst = &dstRgb[startIndex];
			remaining = count;
			do {
				dst->r = src->r;
				dst->g = src->g;
				dst->b = src->b;
				++src;
				++dst;
				--remaining;
			} while (remaining != 0);
		}
#else
		{
			int srcByteDelta;
			uint8_t* srcByte;
			uint8_t* dstByte;

			srcByte = (uint8_t*)&srcRgb[startIndex];
			dstByte = &dstRgb[startIndex].g;
			srcByteDelta = (uint8_t*)srcRgb - (uint8_t*)dstRgb;
			remaining = count;
			do {
				dstByte[-1] = srcByte[0];
				srcByte += 3;
				dstByte[0] = dstByte[srcByteDelta];
				dstByte[1] = srcByte[-1];
				dstByte += 3;
				--remaining;
			} while (remaining != 0);
		}
#endif
		return;
	}

	remaining = count;
	--remaining;
	if (remaining == -1) {
		return;
	}

	byteOffset = startIndex + startIndex * 2;
	dst = (RgbTriplet*)((uint8_t*)dstRgb + byteOffset);
	srcByte = (uint8_t*)srcRgb + byteOffset + 2;
	++remaining;
	do {
		r = srcByte[-2];
		g = srcByte[-1];
		b = srcByte[0];

		if (r < g || r < b) {
			if (g < r || g < b) {
				maxChannel = b;
			} else {
				maxChannel = g;
			}
		} else {
			maxChannel = r;
		}

		if (r > g || r > b) {
			if (g > r || g > b) {
				minChannel = b;
			} else {
				minChannel = g;
			}
		} else {
			minChannel = r;
		}

		if (maxChannel != 0) {
			saturation6 = (uint8_t)(63 * (maxChannel - minChannel) / maxChannel);
		} else {
			saturation6 = 0;
		}

		if (saturation6 != 0) {
			if (r == maxChannel) {
				if (g >= b) {
					hueOffset6 = (uint8_t)(63 * (g - b) / (maxChannel - minChannel));
					hueSector = 0;
				} else {
					hueOffset6 = (uint8_t)(63 * (g - b) / (maxChannel - minChannel) + 63);
					hueSector = 5;
				}
			} else if (g == maxChannel) {
				if (b >= r) {
					hueOffset6 = (uint8_t)(63 * (b - r) / (maxChannel - minChannel));
					hueSector = 2;
				} else {
					hueOffset6 = (uint8_t)(63 * (b - r) / (maxChannel - minChannel) + 63);
					hueSector = 1;
				}
			} else if (r >= g) {
				hueOffset6 = (uint8_t)(63 * (r - g) / (maxChannel - minChannel));
				hueSector = 4;
			} else {
				hueOffset6 = (uint8_t)(63 * (r - g) / (maxChannel - minChannel) + 63);
				hueSector = 3;
			}
		}

		value6 = (uint8_t)(((unsigned int)g_flightBrightnessScaleQ8 * maxChannel) >> 8);
		if (value6 > 0x3fu) {
			value6 = 63;
		}

		if (saturation6 != 0) {
			lowChannel = (uint8_t)(value6 * (63 - saturation6) / 63);
			offsetChannel = (uint8_t)(value6 * (63 - saturation6 * hueOffset6 / 63) / 63);
			inverseOffsetChannel = (uint8_t)(value6 * (63 - saturation6 * (63 - hueOffset6) / 63) / 63);

			switch (hueSector) {
				case 0:
					r = value6;
					g = inverseOffsetChannel;
					b = lowChannel;
					break;
				case 1:
					r = offsetChannel;
					g = value6;
					b = lowChannel;
					break;
				case 2:
					r = lowChannel;
					g = value6;
					b = inverseOffsetChannel;
					break;
				case 3:
					r = lowChannel;
					g = offsetChannel;
					b = value6;
					break;
				case 4:
					r = inverseOffsetChannel;
					g = lowChannel;
					b = value6;
					break;
				case 5:
					r = value6;
					g = lowChannel;
					b = offsetChannel;
					break;
				default:
					break;
			}
		} else {
			r = value6;
			g = value6;
			b = value6;
		}

		dst->r = r;
		dst->g = g;
		dst->b = b;
		++dst;
		srcByte += 3;
		--remaining;
	} while (remaining != 0);
}

// FUNCTION: XWA 0x4D4540
void FlightPalette_SetRange(RgbTriplet* rgbTriples, int startIdx, uint16_t count) {
	int endIdx;
	int idx;
	int startMasked;

	idx = startIdx;
	endIdx = (uint16_t)count + (startIdx & 0xffff);
	startMasked = startIdx & 0xffff;
	if (startMasked < endIdx) {
		do {
			g_swPalette[idx & 0xffff].r = rgbTriples->r;
			g_swPalette[idx & 0xffff].g = rgbTriples->g;
			g_swPalette[idx & 0xffff].b = rgbTriples->b;
			++idx;
			++rgbTriples;
		} while ((idx & 0xffff) < endIdx);
	}
	if (g_palettePackedMode == 2) {
		FlightPalette_Build16BppRange(g_swPalette, g_flightTextPalette, startMasked, count);
	}
}

static __inline uint16_t FlightPalette_PackRgb565(const RgbTriplet* colors, uint8_t colorIndex) {
	const RgbTriplet* volatile color;
	uint16_t packedColor;

	color = &colors[colorIndex];
	packedColor = (uint16_t)(((color->r >> 1) << 11) | (color->g << 5) | (color->b >> 1));
	return packedColor;
}

static __inline uint16_t FlightPalette_PackRgb555(uint8_t red, uint8_t green, uint8_t blue) {
	uint16_t bluePart;
	uint16_t redGreen;

	redGreen = (uint16_t)green & 0x7feu;
	bluePart = red & 0x3eu;
	bluePart <<= 5;
	redGreen |= bluePart;
	bluePart = blue >> 1;
	bluePart &= 0x7fffu;
	return (uint16_t)((redGreen << 4) | bluePart);
}

// FUNCTION: XWA 0x4D2210
void FlightPalette_Build16BppRange(RgbTriplet* srcRgb, uint16_t* dst16, int startIndex, int count) {
	int remaining;
	uint8_t* srcByte;
	uint8_t minChannel;
	uint16_t* dst;
	uint8_t saturation6;
	uint8_t maxChannel;
	uint8_t hueSector;
	uint8_t hueOffset6;
	uint8_t value6;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	RgbTriplet colors[1];
	uint8_t colorIndex;
	uint8_t lowChannel;
	uint8_t offsetChannel;
	uint8_t inverseOffsetChannel;

	if (g_flightBrightnessScaleQ8 == 256) {
		int endIndex;

		endIndex = startIndex + count;
		if (startIndex < endIndex) {
			uint8_t* srcGreen;

			dst = &dst16[startIndex];
			remaining = endIndex - startIndex;
			srcGreen = &srcRgb[startIndex].g;
			do {
				if (!Display_IsPixelFormat555()) {
					colors[0].r = srcGreen[-1];
					colors[0].g = srcGreen[0];
					colors[0].b = srcGreen[1];
					colorIndex = 0;
					*dst = FlightPalette_PackRgb565(colors, colorIndex);
				} else {
					*dst = FlightPalette_PackRgb555(srcGreen[-1], srcGreen[0], srcGreen[1]);
				}
				srcGreen += 3;
				++dst;
				--remaining;
			} while (remaining != 0);
		}
		return;
	}

	remaining = count;
	--remaining;
	if (remaining == -1) {
		return;
	}

	srcByte = &srcRgb[startIndex].b;
	dst = &dst16[startIndex];
	++remaining;
	do {
		r = srcByte[-2];
		g = srcByte[-1];
		b = srcByte[0];

		if (r < g || r < b) {
			if (g < r || g < b) {
				maxChannel = b;
			} else {
				maxChannel = g;
			}
		} else {
			maxChannel = r;
		}

		if (r > g || r > b) {
			if (g > r || g > b) {
				minChannel = b;
			} else {
				minChannel = g;
			}
		} else {
			minChannel = r;
		}

		if (maxChannel != 0) {
			saturation6 = (uint8_t)(63 * (maxChannel - minChannel) / maxChannel);
		} else {
			saturation6 = 0;
		}

		if (saturation6 != 0) {
			if (r == maxChannel) {
				if (g >= b) {
					hueOffset6 = (uint8_t)(63 * (g - b) / (maxChannel - minChannel));
					hueSector = 0;
				} else {
					hueOffset6 = (uint8_t)(63 * (g - b) / (maxChannel - minChannel) + 63);
					hueSector = 5;
				}
			} else if (g == maxChannel) {
				if (b >= r) {
					hueOffset6 = (uint8_t)(63 * (b - r) / (maxChannel - minChannel));
					hueSector = 2;
				} else {
					hueOffset6 = (uint8_t)(63 * (b - r) / (maxChannel - minChannel) + 63);
					hueSector = 1;
				}
			} else if (r >= g) {
				hueOffset6 = (uint8_t)(63 * (r - g) / (maxChannel - minChannel));
				hueSector = 4;
			} else {
				hueOffset6 = (uint8_t)(63 * (r - g) / (maxChannel - minChannel) + 63);
				hueSector = 3;
			}
		}

		value6 = (uint8_t)(((unsigned int)g_flightBrightnessScaleQ8 * maxChannel) >> 8);
		if (value6 > 0x3fu) {
			value6 = 63;
		}

		if (saturation6 != 0) {
			lowChannel = (uint8_t)(value6 * (63 - saturation6) / 63);
			offsetChannel = (uint8_t)(value6 * (63 - saturation6 * hueOffset6 / 63) / 63);
			inverseOffsetChannel = (uint8_t)(value6 * (63 - saturation6 * (63 - hueOffset6) / 63) / 63);

			switch (hueSector) {
				case 0:
					r = value6;
					g = inverseOffsetChannel;
					b = lowChannel;
					break;
				case 1:
					r = offsetChannel;
					g = value6;
					b = lowChannel;
					break;
				case 2:
					r = lowChannel;
					g = value6;
					b = inverseOffsetChannel;
					break;
				case 3:
					r = lowChannel;
					g = offsetChannel;
					b = value6;
					break;
				case 4:
					r = inverseOffsetChannel;
					g = lowChannel;
					b = value6;
					break;
				case 5:
					r = value6;
					g = lowChannel;
					b = offsetChannel;
					break;
				default:
					break;
			}
		} else {
			r = value6;
			g = value6;
			b = value6;
		}

		if (!Display_IsPixelFormat555()) {
			colors[0].r = r;
			colors[0].g = g;
			colors[0].b = b;
			colorIndex = 0;
			*dst = FlightPalette_PackRgb565(colors, colorIndex);
		} else {
			*dst = FlightPalette_PackRgb555(r, g, b);
		}
		srcByte += 3;
		++dst;
		--remaining;
	} while (remaining != 0);
}

// FUNCTION: XWA 0x4D45C0
char* FlightPalette_GetFull(char* dst768) {
	char* result;
	int i;

	result = (char*)&g_swPalette[0].g;
	for (i = 0; i < 256; ++i) {
		dst768[0] = *(result - 1);
		dst768[1] = result[0];
		dst768[2] = result[1];
		result += 3;
		dst768 += 3;
	}
	return result;
}

// FUNCTION: XWA 0x4D45F0
void FlightPalette_SetFull(RgbTriplet* srcRgb) {
	RgbTriplet* dst;
	int remaining;

	dst = g_swPalette;
	remaining = 256;
	do {
		dst->r = srcRgb->r;
		dst->g = srcRgb->g;
		dst->b = srcRgb->b;
		++srcRgb;
		++dst;
		--remaining;
	} while (remaining != 0);
	if (g_palettePackedMode == 2) {
		FlightPalette_Build16BppRange(g_swPalette, g_flightTextPalette, 0, 256);
	}
}

// FUNCTION: XWA 0x4D2730
int FlightSw_ComputePixelOffset(int x, int y) {
	return y * FlightSw_GetLinePitch() + x * g_flight16bppBytesPerPixel;
}

// FUNCTION: XWA 0x4D4050
int FlightSw_GetLineBufferAddr(int line) { return g_flightLineBufferTable[line]; }

// FUNCTION: XWA 0x4D4060
int FlightSw_GetLinePitch(void) { return *g_flightLinePitchPtr; }

// FUNCTION: XWA 0x4D2750
int FlightSw_BlitSpriteRle(uint8_t* rleData, int16_t x, int16_t y, int endMarker, int mirror) {
	(void)rleData;
	(void)x;
	(void)y;
	(void)endMarker;
	(void)mirror;
	/* TODO: Reimplement FlightSw_BlitSpriteRle @ 0x4D2750. */
	return 0;
}

// FUNCTION: XWA 0x4D2780
int FlightSw_BlitSpriteRleFaded(uint8_t* rleData, int16_t x, int16_t y, int endMarker, char paletteShift,
								int16_t fadeAmount) {
	(void)rleData;
	(void)x;
	(void)y;
	(void)endMarker;
	(void)paletteShift;
	(void)fadeAmount;
	/* TODO: Reimplement FlightSw_BlitSpriteRleFaded @ 0x4D2780. */
	return 0;
}

// FUNCTION: XWA 0x4D3130
int FlightSw_FillClipRect(void) {
	/* TODO: Reimplement FlightSw_FillClipRect @ 0x4D3130. */
	return 0;
}

// FUNCTION: XWA 0x4D3170
int FlightSw_FillRectOrBorder(uint16_t borderThickness) {
	(void)borderThickness;
	/* TODO: Reimplement FlightSw_FillRectOrBorder @ 0x4D3170. */
	return 0;
}

// FUNCTION: XWA 0x4D33D0
int16_t FlightSw_FillRectClipped(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
								 uint16_t borderThickness) {
	g_flightFillRectLeft = x1;
	g_flightFillRectTop = y1;
	g_flightFillRectRight = x2;
	g_flightFillRectBottom = y2;

	if (x1 < g_flightClipLeft) {
		g_flightFillRectLeft = (uint16_t)g_flightClipLeft;
	}
	if (x2 > g_flightClipRight) {
		g_flightFillRectRight = (uint16_t)g_flightClipRight;
	}
	if (y1 < g_flightClipTop) {
		g_flightFillRectTop = (uint16_t)g_flightClipTop;
	}
	if (y2 > g_flightClipBottom) {
		g_flightFillRectBottom = (uint16_t)g_flightClipBottom;
	}
	if (g_flightFillRectBottom > g_flightFillRectTop && g_flightFillRectRight > g_flightFillRectLeft) {
		FlightSw_FillRectOrBorder(borderThickness);
	}
}

// FUNCTION: XWA 0x4D34A0
int FlightSw_SaveScreenRect(int16_t* dst, int srcX, int srcY, int width, int height) {
	int16_t* dstPtr;
	int16_t* src;
	int16_t pixel;
	int result;
	unsigned int rowsLeft;

	dstPtr = dst;
	result = height;
	rowsLeft = (unsigned int)height;
	if (rowsLeft > 0) {
		do {
			int rowBase = FlightSw_GetLineBufferAddr(srcY) + 2 * srcX;
			src = (int16_t*)(rowBase + 2 * ((uintptr_t)g_flightSwFramebufferBase >> 1));
			g_savedRowPixelsRemaining = width;
			for (; g_savedRowPixelsRemaining > 0; --g_savedRowPixelsRemaining) {
				pixel = *src++;
				*dstPtr++ = pixel;
			}
			++srcY;
			--rowsLeft;
			result = (int)rowsLeft;
		} while (rowsLeft > 0);
	}
	return result;
}

// FUNCTION: XWA 0x4D3520
void FlightSw_RestoreScreenRect(const int16_t* src, int dstX, int dstY, int width, int height) {
	const int16_t* srcPtr;
	int16_t* dstPtr;
	int16_t pixel;
	unsigned int rowsLeft;

	srcPtr = src;
	rowsLeft = (unsigned int)height;
	while (rowsLeft > 0) {
		int rowBase = FlightSw_GetLineBufferAddr(dstY) + 2 * dstX;
		dstPtr = (int16_t*)(rowBase + 2 * ((uintptr_t)g_flightSwFramebufferBase >> 1));
		g_savedRowPixelsRemaining = width;
		while (g_savedRowPixelsRemaining > 0) {
			pixel = *srcPtr++;
			*dstPtr = pixel;
			--g_savedRowPixelsRemaining;
			++dstPtr;
		}
		++dstY;
		--rowsLeft;
	}
}

// FUNCTION: XWA 0x4D35A0
void FlightSw_DrawPointArray(uint16_t* points, uint16_t count) {
	float scaledSize;
	FlightTexQuad quad;
	int remaining;

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	scaledSize = g_flightSwPointSpriteScale;
	scaledSize *= g_radarTargetMarkerBaseSize;
	quad.screenSize = (uint16_t)(int)scaledSize;
	quad.screenSize >>= 2;

	if (count > 0) {
		remaining = count;
		do {
			uint16_t x;
			uint16_t y;

			x = points[0];
			y = points[1];
			if (g_useHardware3D) {
				uint16_t color16;
				int color;
				uint8_t colorIdx;

				colorIdx = *((uint8_t*)points + 4);
				quad.screenX = x;
				quad.screenY = g_screenHeight - y;
				color16 = g_flightTextPalette[colorIdx];
				if (Display_IsPixelFormat555()) {
					color = 8 * ((color16 & 0x1f) + 8 * ((color16 & 0x03e0) + 8 * (color16 & 0x7c00)));
				} else {
					color = 8 * ((color16 & 0x1f) + 4 * ((color16 & 0x07e0) + 8 * (color16 & 0xf800)));
				}
				FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, 0x2fu, 256);
				RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, color | (int)0xff000000u);
			} else {
				uintptr_t alignedFramebufferBase;
				uint8_t colorIdx;
				int pixelOffset;

				pixelOffset = FlightSw_GetLineBufferAddr(y) + 2 * x;
				alignedFramebufferBase = ((uintptr_t)g_flightSwFramebufferBase >> 1) * 2;
				colorIdx = *((uint8_t*)points + 4);
				*(uint16_t*)(alignedFramebufferBase + pixelOffset) = g_flightTextPalette[colorIdx];
			}
			points += 3;
			--remaining;
		} while (remaining != 0);
	}
}

// FUNCTION: XWA 0x4D36F0
uint16_t* FlightSw_DrawPointArrayMasked(uint16_t* points, uint16_t count) {
	(void)count;
	/* TODO: Reimplement FlightSw_DrawPointArrayMasked @ 0x4D36F0. */
	return points;
}

// FUNCTION: XWA 0x4D2A00
int FlightSw_DrawPixel(uint16_t x, uint16_t y, char colorIdx) {
	uintptr_t alignedFramebufferBase;
	int result;

	result = FlightSw_GetLineBufferAddr(y) + 2 * x;
	alignedFramebufferBase = ((uintptr_t)g_flightSwFramebufferBase >> 1) * 2;
	*(uint16_t*)(alignedFramebufferBase + result) = g_flightTextPalette[(int)colorIdx];
	return result;
}

// FUNCTION: XWA 0x4D3750
void FlightSw_DrawRadarTargetMarker(void) {
	uint16_t shapeOffset;
	int savedPixelIndex;
	int remaining;
	FlightTexQuad quad;

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = 256;

	if (g_radarTargetMarkerDrawX == UINT16_MAX && g_radarTargetMarkerDrawY == UINT16_MAX) {
		return;
	}

	shapeOffset = 0;
	if (g_useHardware3D) {
		float scaledSize;

		scaledSize = g_flightHudScaleFactor;
		scaledSize *= g_radarTargetMarkerBaseSize;
		quad.screenSize = (uint16_t)(int)scaledSize;
		quad.screenX = (uint16_t)g_radarTargetMarkerDrawX;
		quad.screenX += g_radarTargetMarkerShapeOffsets[0] + 1;
		quad.screenY =
			g_screenHeight - g_radarTargetMarkerShapeOffsets[1] - (uint16_t)g_radarTargetMarkerDrawY + 1;
		FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, 0x30u, 256);
		RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, -1);
		return;
	}

	savedPixelIndex = 0;
	remaining = 10;
	do {
		int x;
		int y;
		int offset;

		y = (uint16_t)g_radarTargetMarkerDrawY + g_radarTargetMarkerShapeOffsets[(uint16_t)shapeOffset + 1];
		offset = FlightSw_GetLineBufferAddr(y);
		x = (uint16_t)g_radarTargetMarkerDrawX + g_radarTargetMarkerShapeOffsets[(uint16_t)shapeOffset];
		shapeOffset += 2;
		offset += 2 * x;
		g_radarTargetMarkerBackgroundSavedPixels[(uint16_t)savedPixelIndex++] =
			*(uint16_t*)(offset + 2 * ((uintptr_t)g_flightSwFramebufferBase >> 1));
		*(uint16_t*)(offset + 2 * ((uintptr_t)g_flightSwFramebufferBase >> 1)) = g_flightTextPalette[206];
	} while (--remaining != 0);
}

// FUNCTION: XWA 0x4D38A0
int FlightSw_RestoreRadarTargetMarker(void) {
	int result;
	uint16_t shapeOffset;
	uint16_t savedPixelIndex;
	int remaining;

	result = 0xffff;
	if ((uint16_t)g_radarTargetMarkerRestoreX == 0xffff && (uint16_t)g_radarTargetMarkerRestoreY == 0xffff) {
		return result;
	}

	for (savedPixelIndex = 0, shapeOffset = 0, remaining = 10; remaining != 0;
		 ++savedPixelIndex, shapeOffset += 2, --remaining) {
		int x;
		int y;
		int shapeOffsetIndex;
		int xOffset;
		uintptr_t alignedFramebufferBase;

		shapeOffsetIndex = (uint16_t)shapeOffset;
		y = (uint16_t)g_radarTargetMarkerRestoreY + g_radarTargetMarkerShapeOffsets[shapeOffsetIndex + 1];
		result = FlightSw_GetLineBufferAddr(y);
		xOffset = g_radarTargetMarkerShapeOffsets[shapeOffsetIndex];
		x = (uint16_t)g_radarTargetMarkerRestoreX + xOffset;
		result += 2 * x;
		alignedFramebufferBase = ((uintptr_t)g_flightSwFramebufferBase >> 1) * 2;
		*(uint16_t*)(alignedFramebufferBase + result) =
			g_radarTargetMarkerBackgroundSavedPixels[(uint16_t)savedPixelIndex];
	}
	return result;
}

// FUNCTION: XWA 0x4D3930
void FlightSw_DrawLine(int x1, int y1, int x2, int y2, uint8_t colorIdx) {
	uint16_t color;
	int deltaX;
	int deltaY;
	uint8_t* pixel;

	color = g_flightTextPalette[colorIdx];

	deltaX = x2 - x1;
	if (deltaX < 0) {
		int swapX;
		int swapY;

		swapX = x1;
		x1 = x2;
		x2 = swapX;
		swapY = y1;
		y1 = y2;
		y2 = swapY;
		deltaX = -deltaX;
	} else if (deltaX == 0) {
		int topY;
		int bottomY;
		int count;

		if (x1 < g_flightClipLeft) {
			return;
		}
		if (x1 >= g_flightClipRight) {
			return;
		}

		topY = y1;
		bottomY = y2;
		if (topY > bottomY) {
			int swapY;

			swapY = topY;
			topY = bottomY;
			bottomY = swapY;
		}
		if (topY < g_flightClipTop) {
			topY = g_flightClipTop;
		}
		if (bottomY >= g_flightClipBottom) {
			bottomY = g_flightClipBottom - 1;
		}

		count = bottomY - topY;
		if (count > 0) {
			pixel = (uint8_t*)g_flightSwFramebufferBase;
			pixel += topY * FlightSw_GetLinePitch();
			pixel += x1 * g_flight16bppBytesPerPixel;
			do {
				*(uint16_t*)pixel = color;
				pixel += FlightSw_GetLinePitch();
				--count;
			} while (count != 0);
		}
		return;
	}

	if (x1 >= g_flightClipRight) {
		return;
	}
	if (x2 < g_flightClipLeft) {
		return;
	}

	deltaY = y2 - y1;
	if (deltaY < 0) {
		deltaY = -deltaY;
		if (y1 < g_flightClipTop) {
			return;
		}
		if (y2 >= g_flightClipBottom) {
			return;
		}

		if (y1 >= g_flightClipBottom) {
			int advance;

			advance = MATH2_ABoverC32(y1 - g_flightClipBottom + 1, deltaX, deltaY);
			x1 += advance;
			if (x1 >= g_flightClipRight) {
				return;
			}
			y1 = g_flightClipBottom - 1;
		}
		if (x1 < g_flightClipLeft) {
			int advance;

			advance = MATH2_ABoverC32(g_flightClipLeft - x1, deltaY, deltaX);
			y1 -= advance;
			if (y1 < g_flightClipTop) {
				return;
			}
			x1 = g_flightClipLeft;
		}
		if (x2 >= g_flightClipRight) {
			x2 = g_flightClipRight - 1;
		}
		if (y2 < g_flightClipTop) {
			y2 = g_flightClipTop;
		}

		pixel = (uint8_t*)g_flightSwFramebufferBase;
		pixel += y1 * FlightSw_GetLinePitch();
		pixel += x1 * g_flight16bppBytesPerPixel;
		if (deltaX >= deltaY) {
			int error;
			int ySteps;
			int xCount;

			error = deltaX >> 1;
			ySteps = y1 - y2 + 1;
			xCount = x2 - x1;
			while (xCount-- != 0) {
				*(uint16_t*)pixel = color;
				pixel += g_flight16bppBytesPerPixel;
				error -= deltaY;
				if (error < 0) {
					error += deltaX;
					--ySteps;
					if (ySteps == 0) {
						return;
					}
					pixel -= FlightSw_GetLinePitch();
				}
			}
		} else {
			int error;
			int xSteps;
			int yCount;

			error = deltaY >> 1;
			xSteps = x2 - x1 + 1;
			yCount = y1 - y2;
			while (yCount-- != 0) {
				*(uint16_t*)pixel = color;
				pixel -= FlightSw_GetLinePitch();
				error -= deltaX;
				if (error < 0) {
					error += deltaY;
					--xSteps;
					if (xSteps == 0) {
						return;
					}
					pixel += g_flight16bppBytesPerPixel;
				}
			}
		}
	} else if (deltaY > 0) {
		if (y1 >= g_flightClipBottom) {
			return;
		}
		if (y2 < g_flightClipTop) {
			return;
		}

		if (y1 < g_flightClipTop) {
			int advance;

			advance = MATH2_ABoverC32(g_flightClipTop - y1, deltaX, deltaY);
			x1 += advance;
			if (x1 >= g_flightClipRight) {
				return;
			}
			y1 = g_flightClipTop;
		}
		if (x1 < g_flightClipLeft) {
			int advance;

			advance = MATH2_ABoverC32(g_flightClipLeft - x1, deltaY, deltaX);
			y1 += advance;
			if (y1 >= g_flightClipBottom) {
				return;
			}
			x1 = g_flightClipLeft;
		}
		if (x2 >= g_flightClipRight) {
			x2 = g_flightClipRight - 1;
		}
		if (y2 >= g_flightClipBottom) {
			y2 = g_flightClipBottom - 1;
		}

		pixel = (uint8_t*)g_flightSwFramebufferBase;
		pixel += y1 * FlightSw_GetLinePitch();
		pixel += x1 * g_flight16bppBytesPerPixel;
		if (deltaX >= deltaY) {
			int error;
			int ySteps;
			int xCount;

			error = deltaX >> 1;
			ySteps = y2 - y1 + 1;
			xCount = x2 - x1;
			while (xCount-- != 0) {
				*(uint16_t*)pixel = color;
				pixel += g_flight16bppBytesPerPixel;
				error -= deltaY;
				if (error < 0) {
					error += deltaX;
					--ySteps;
					if (ySteps == 0) {
						return;
					}
					pixel += FlightSw_GetLinePitch();
				}
			}
		} else {
			int error;
			int xSteps;
			int yCount;

			error = deltaY >> 1;
			xSteps = x2 - x1 + 1;
			yCount = y2 - y1;
			while (yCount-- != 0) {
				*(uint16_t*)pixel = color;
				pixel += FlightSw_GetLinePitch();
				error -= deltaX;
				if (error < 0) {
					error += deltaY;
					--xSteps;
					if (xSteps == 0) {
						return;
					}
					pixel += g_flight16bppBytesPerPixel;
				}
			}
		}
	} else if (y1 >= g_flightClipTop && y1 < g_flightClipBottom) {
		int count;

		if (x1 < g_flightClipLeft) {
			x1 = g_flightClipLeft;
		}
		if (x2 >= g_flightClipRight) {
			x2 = g_flightClipRight - 1;
		}

		count = x2 - x1;
		if (count > 0) {
			pixel = (uint8_t*)g_flightSwFramebufferBase;
			pixel += y1 * FlightSw_GetLinePitch();
			pixel += x1 * g_flight16bppBytesPerPixel;
			do {
				*(uint16_t*)pixel = color;
				pixel += g_flight16bppBytesPerPixel;
				--count;
			} while (count != 0);
		}
	}
}
