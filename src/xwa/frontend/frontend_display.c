#include "xwa/frontend/frontend_display.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif

#include "aeron/aeron.h"

#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/model_texture.h"
#include "xwa/audio/cd_audio.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/flight_display.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/util/memory.h"
#include "xwa/xwa_options.h"
#ifdef XWA_MODERN
#include "xwa_runtime/runtime/presentation.h"
#endif

#include <string.h>

#ifndef XWA_MODERN
__declspec(dllimport) int __stdcall SetCursorPos(int X, int Y);
// GLOBAL: XWA 0x5A9274
FrontendDisplaySetCursorPosFunc g_SetCursorPos = SetCursorPos;
#else
static int FrontendDisplay_SetCursorPos(int x, int y) {
	int presentationX;
	int presentationY;
	XwaPresentation_FromClassic(x, y, &presentationX, &presentationY);
	return Aeron_WarpMouseLogical(presentationX, presentationY);
}
FrontendDisplaySetCursorPosFunc g_SetCursorPos = FrontendDisplay_SetCursorPos;
#endif

enum {
	FRONTEND_DISPLAY_WIDTH = 640,
	FRONTEND_DISPLAY_HEIGHT = 480,
	FRONTEND_DISPLAY_COLOR_KEY = 0x2000,
	FRONTEND_DISPLAY_SURFACE_FLAG = AERON_SURFACE_CPU_LOCKABLE | AERON_SURFACE_OFFSCREEN,
};

// GLOBAL: XWA 0x9F700A
int g_displayBpp = 0;
// GLOBAL: XWA 0x9F700E
unsigned char g_pixelFormat555;
// GLOBAL: XWA 0x5FFDB0
int g_pixelFormatCode = 565;
// GLOBAL: XWA 0x5FFD9C
int g_flightRenderToFrontend;
// GLOBAL: XWA 0xA21441
int g_frameCounter = 0;
// GLOBAL: XWA 0xA2143D
int g_frameIntervalMs = 1000 / 24;
// GLOBAL: XWA 0x9F6FF4
FrontendDisplayBackBufferLock g_backBufferLocked = { 0 };
// GLOBAL: XWA 0x9F6FF6
int g_surfaceClearColor = 0;
// GLOBAL: XWA 0x9F6F87
uint8_t g_escapeCloseEnabled = 0;
// GLOBAL: XWA 0x9F6FF5
uint8_t g_presentFrameReady = 0;
// GLOBAL: XWA 0x9F7046
int g_frontendDisplayReactivated;
// GLOBAL: XWA 0x9F6FFE
int g_backBufferPitch = 0;
// GLOBAL: XWA 0x9F7002
int g_offscreenSurfacePitch = 0;
// GLOBAL: XWA 0x9F7006
int g_frontSurfacePitch = 0;
// GLOBAL: XWA 0x9F7012
int g_presentBlitX;
// GLOBAL: XWA 0x9F7016
int g_presentBlitY;
// GLOBAL: XWA 0x9F700F
unsigned char g_offscreenRestoreEnabled = 0;
// GLOBAL: XWA 0x9F7AF6
FrontendPaletteEntry g_displayPalette[256];
// GLOBAL: XWA 0x9F7026
IDirectDraw* g_directDrawPrimary = 0;
// GLOBAL: XWA 0x9F7022
IDirectDraw* g_directDrawSecondary = 0;
// GLOBAL: XWA 0x9F702A
IDirectDraw* g_directDraw = 0;
// GLOBAL: XWA 0x9F7032
IDirectDrawSurface* g_primarySurface = 0;
// GLOBAL: XWA 0x9F702E
IDirectDrawSurface* g_offscreenSurface = 0;
// GLOBAL: XWA 0x9F7036
IDirectDrawSurface* g_backBufferSurface = 0;
// GLOBAL: XWA 0x9F703A
IDirectDrawSurface* g_frontSurface = 0;
// GLOBAL: XWA 0x9F703E
IDirectDrawSurface* g_unusedFrontendSurfaceAlias = 0;
// GLOBAL: XWA 0x9F7AF2
IDirectDrawPalette* g_ddPalette = 0;
// GLOBAL: XWA 0x9F6F88
DDSURFACEDESC g_backBufferDesc;
// GLOBAL: XWA 0x9F709A
unsigned char* g_offscreenBackupBuffer = 0;
// GLOBAL: XWA 0x9F65E9
void* g_frontendDisplayLifecycleScratch;
// GLOBAL: XWA 0x783008
static int g_frontendDisplaySavedPixelFormat555;
// GLOBAL: XWA 0x9F7011
static unsigned char g_dirtyRectBlitEnabled;
// GLOBAL: XWA 0x9F701A
void* g_hWnd = 0;
// GLOBAL: XWA 0x9F709E
static int g_restoreOffscreenOverlayAfterActivate = 0;
// GLOBAL: XWA 0x9F7EF6
static int g_paletteNeedsSet = 0;
// GLOBAL: XWA 0x9F701E
static unsigned int g_frontendDisplayWndProcMode;
// GLOBAL: XWA 0x9F70A2
DisplayDriverEntry g_displayDriverTable[1] = {
	{
		"Aeron D3D Renderer",
		0,
		{ 1, 1, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1, 1 },
	},
};
// GLOBAL: XWA 0x9F7922
unsigned int g_displayDriverCount = 1;
// GLOBAL: XWA 0x9F79A6
unsigned int g_selectedDisplayDriver = 0;
// GLOBAL: XWA 0x9F7010
unsigned char g_secondaryDirectDrawActive;
// GLOBAL: XWA 0x603960
const unsigned int g_colorDistLUT[256] = {
	0,     1,     4,     9,     16,    25,    36,    49,    64,    81,    100,   121,   144,   169,   196,
	225,   256,   289,   324,   361,   400,   441,   484,   529,   576,   625,   676,   729,   784,   841,
	900,   961,   1024,  1089,  1156,  1225,  1296,  1369,  1444,  1521,  1600,  1681,  1764,  1849,  1936,
	2025,  2116,  2209,  2304,  2401,  2500,  2601,  2704,  2809,  2916,  3025,  3136,  3249,  3364,  3481,
	3600,  3721,  3844,  3969,  4096,  4225,  4356,  4489,  4624,  4761,  4900,  5041,  5184,  5329,  5476,
	5625,  5776,  5929,  6084,  6241,  6400,  6561,  6724,  6889,  7056,  7225,  7396,  7569,  7744,  7921,
	8100,  8281,  8464,  8649,  8836,  9025,  9216,  9409,  9604,  9801,  10000, 10201, 10404, 10609, 10816,
	11025, 11236, 11449, 11664, 11881, 12100, 12321, 12544, 12769, 12996, 13225, 13456, 13689, 13924, 14161,
	14400, 14641, 14884, 15129, 15376, 15625, 15876, 16129, 16384, 16641, 16900, 17161, 17424, 17689, 17956,
	18225, 18496, 18769, 19044, 19321, 19600, 19881, 20164, 20449, 20736, 21025, 21316, 21609, 21904, 22201,
	22500, 22801, 23104, 23409, 23716, 24025, 24336, 24649, 24964, 25281, 25600, 25921, 26244, 26569, 26896,
	27225, 27556, 27889, 28224, 28561, 28900, 29241, 29584, 29929, 30276, 30625, 30976, 31329, 31684, 32041,
	32400, 32761, 33124, 33489, 33856, 34225, 34596, 34969, 35344, 35721, 36100, 36481, 36864, 37249, 37636,
	38025, 38416, 38809, 39204, 39601, 40000, 40401, 40804, 41209, 41616, 42025, 42436, 42849, 43264, 43681,
	44100, 44521, 44944, 45369, 45796, 46225, 46656, 47089, 47524, 47961, 48400, 48841, 49284, 49729, 50176,
	50625, 51076, 51529, 51984, 52441, 52900, 53361, 53824, 54289, 54756, 55225, 55696, 56169, 56644, 57121,
	57600, 58081, 58564, 59049, 59536, 60025, 60516, 61009, 61504, 62001, 62500, 63001, 63504, 64009, 64516,
	65025
};

// FUNCTION: XWA 0x50C640
static unsigned char FrontendDisplay_Init3DDeviceForSurface(IDirectDrawSurface* renderSurface) {
	g_frontendD3DInitialized = 0;
	if (renderSurface == 0) {
		return 0;
	}

	g_flightDirectDraw = FrontendDisplay_GetDirectDraw();
	width = FRONTEND_DISPLAY_WIDTH;
	g_surfaceWidth = FRONTEND_DISPLAY_WIDTH;
	g_useHardware3D = 1;
	height = FRONTEND_DISPLAY_HEIGHT;
	g_surfaceHeight = FRONTEND_DISPLAY_HEIGHT;
	g_pixelFormatCode = 565;
	g_flightPrimaryPitch = FrontendDisplay_GetDrawSurfacePitch();
	g_surfacePitch = g_flightPrimaryPitch;
	{
		signed char pixelFormatAdjustment = (signed char)-!!FrontendDisplay_GetPixelFormat555();
		pixelFormatAdjustment &= -10;
		g_pixelFormatCode = 565 + pixelFormatAdjustment;
	}

	/* Frontend render-quality defaults + render-state presets. In the original
	 * these globals are configured before Frontend3D_InitDeviceForSurface and the
	 * presets are the tail of Renderer_InitD3DDevice; the port factors both into
	 * Renderer_InitFrontendHardwareSettings, so it must run before the device is
	 * created for the presets to pick up g_bilinearEnabled. */
#ifdef XWA_MODERN
	Renderer_InitFrontendHardwareSettings();
#endif

	/* Create the std3D device on the frontend render surface. std3D QIs the shim
	 * surface for its IDirect3DDevice; on any failure Renderer_InitD3DDevice clears
	 * g_useHardware3D and the frontend falls back to the software 2D path. */
	Renderer_InitD3DDevice(g_flightDirectDraw, renderSurface);
	if (!g_useHardware3D) {
		return 0;
	}

	g_frontendD3DInitialized = 1;
	g_useHardware3D = Config_GetSinglePlayerHardware3D();
	RenderBatch_AllocMeshPassBatches();
	return 1;
}

static int Frontend3D_ShutdownDevice(void) {
	if (g_frontendD3DInitialized) {
		g_useHardware3D = 1;
		RenderBatch_FreeDataPoolsThunk();
		D3DInfo_ReleaseAll();
		std3D_Close(0);
		std3D_Shutdown();
		g_useHardware3D = 0;
		g_frontendD3DInitialized = 0;
	}
	return 1;
}

AeronPixelFormat FrontendDisplay_GetPixelFormat(void) {
	if (g_displayBpp == 8) {
		return AERON_PIXEL_FORMAT_INDEX8;
	}

	if (g_displayBpp == 16) {
		return g_pixelFormat555 ? AERON_PIXEL_FORMAT_RGB555 : AERON_PIXEL_FORMAT_RGB565;
	}

	return AERON_PIXEL_FORMAT_UNKNOWN;
}

// FUNCTION: XWA 0x57E7C0
static HRESULT FrontendDisplay_SetSurfaceColorKey(IDirectDrawSurface* surface, uint32_t colorKey) {
	DDCOLORKEY key;
	key.dwColorSpaceLowValue = colorKey;
	key.dwColorSpaceHighValue = colorKey;
	return surface->lpVtbl->SetColorKey(surface, DDCKEY_SRCBLT, &key);
}

// FUNCTION: XWA 0x53E120
HRESULT FrontendDisplay_RestoreLostSurfaces(void) {
	HRESULT result;

	result = g_primarySurface->lpVtbl->Restore(g_primarySurface);
	if (!result) {
		if (g_optNoFullscreen || g_noPageFlip) {
			return g_backBufferSurface->lpVtbl->Restore(g_backBufferSurface);
		}
		return g_offscreenSurface->lpVtbl->Restore(g_offscreenSurface);
	}
	return result;
}

/* Report a DirectDraw bring-up failure stage. The original popped a Win32 message
 * box; the port logs and returns 0 (failure). */
static int FrontendDisplay_ReportDirectDrawInitFailure(void* hwnd, int stage) {
	(void)hwnd;
	(void)stage;
	return 0;
}

// FUNCTION: XWA 0x57E5A0
/* Builds g_displayPalette (default 3-3-2 ramp; original also read a BMP resource
 * via Win32 - dropped for the port) and creates the DirectDraw palette. */
IDirectDrawPalette* FrontendDisplay_LoadPalette(IDirectDraw* pDD, const char* lpName) {
	FrontendPaletteEntry table[256];
	IDirectDrawPalette* palette = 0;
	int i;
	(void)lpName;

	for (i = 0; i < 256; ++i) {
		table[i].peRed = (unsigned char)(255 * ((i >> 5) & 7) / 7);
		table[i].peGreen = (unsigned char)(255 * ((i >> 2) & 7) / 7);
		table[i].peBlue = (unsigned char)(255 * (i & 3) / 3);
		table[i].peFlags = 0;
	}

	pDD->lpVtbl->CreatePalette(pDD, DDPCAPS_8BIT | DDPCAPS_ALLOW256, table, &palette, NULL);
	if (palette) {
		memcpy(g_displayPalette, table, sizeof(g_displayPalette));
		g_displayPalette[0].peRed = 0;
		g_displayPalette[0].peGreen = 0;
		g_displayPalette[0].peBlue = 0;
		g_displayPalette[255].peRed = 0xff;
		g_displayPalette[255].peGreen = 0xff;
		g_displayPalette[255].peBlue = 0xff;
	}
	return palette;
}

// FUNCTION: XWA 0x540370
int FrontendDisplay_InitSurfaces(void) {
	DDSCAPS caps;
	DDSURFACEDESC desc;
	void* hwnd = g_hWnd;
	IDirectDrawPalette* palette;

	if (g_directDraw->lpVtbl->SetCooperativeLevel(g_directDraw, hwnd,
												  DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWMODEX)) {
		return FrontendDisplay_ReportDirectDrawInitFailure(hwnd, 1);
	}
	if (g_directDraw->lpVtbl->SetDisplayMode(g_directDraw, 640, 480, (uint32_t)g_displayBpp)) {
		return FrontendDisplay_ReportDirectDrawInitFailure(hwnd, 2);
	}

	if (g_optNoFullscreen) {
		memset(&desc, 0, sizeof(desc));
		desc.dwSize = 108;
		desc.dwFlags = DDSD_CAPS;
		desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	} else {
		if (g_noPageFlip) {
		}
		memset(&desc, 0, sizeof(desc));
		desc.dwSize = 108;
		desc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_3DDEVICE;
		desc.dwBackBufferCount = 1;
	}
	if (g_directDraw->lpVtbl->CreateSurface(g_directDraw, &desc, &g_primarySurface, NULL)) {
		return FrontendDisplay_ReportDirectDrawInitFailure(hwnd, 3);
	}
	g_primarySurface->lpVtbl->GetSurfaceDesc(g_primarySurface, &desc);
	g_pixelFormat555 = (desc.ddpfPixelFormat.dwGBitMask & 0x400) == 0;

	if (g_optNoFullscreen) {
		memset(&desc, 0, sizeof(desc));
		desc.dwSize = 108;
		desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
		desc.dwWidth = 640;
		desc.dwHeight = 480;
		if (!g_directDraw->lpVtbl->CreateSurface(g_directDraw, &desc, &g_frontSurface, NULL)) {
			g_frontSurface->lpVtbl->GetSurfaceDesc(g_frontSurface, &desc);
			g_frontSurfacePitch = desc.lPitch;
		}
	} else {
		caps.dwCaps = DDSCAPS_BACKBUFFER;
		if (!g_primarySurface->lpVtbl->GetAttachedSurface(g_primarySurface, &caps, &g_frontSurface)) {
			g_frontSurface->lpVtbl->GetSurfaceDesc(g_frontSurface, &desc);
			g_frontSurfacePitch = desc.lPitch;
		}
	}

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = 108;
	desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	desc.dwWidth = 640;
	desc.dwHeight = 480;
	if (g_directDraw->lpVtbl->CreateSurface(g_directDraw, &desc, &g_backBufferSurface, NULL)) {
		return FrontendDisplay_ReportDirectDrawInitFailure(hwnd, 4);
	}
	FrontendDisplay_SetSurfaceColorKey(g_backBufferSurface, 0x2000);
	FrontendDisplay_Init3DDeviceForSurface(g_frontSurface);
	g_unusedFrontendSurfaceAlias = g_frontSurface;
	if (Config_GetSinglePlayerHardware3D() && !g_frontSurface) {
		Config_SetSinglePlayerHardware3D(0);
	}
	g_backBufferSurface->lpVtbl->GetSurfaceDesc(g_backBufferSurface, &desc);
	g_backBufferPitch = desc.lPitch;

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = 108;
	desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	if (g_optNoFullscreen || (desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN, g_noPageFlip)) {
		desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	}
	desc.dwWidth = 640;
	desc.dwHeight = 480;
	if (g_directDraw->lpVtbl->CreateSurface(g_directDraw, &desc, &g_offscreenSurface, NULL)) {
		return FrontendDisplay_ReportDirectDrawInitFailure(hwnd, 5);
	}
	g_offscreenSurface->lpVtbl->GetSurfaceDesc(g_offscreenSurface, &desc);
	g_offscreenSurfacePitch = desc.lPitch;

	palette = FrontendDisplay_LoadPalette(g_directDraw, NULL);
	g_ddPalette = palette;
	if (g_optNoFullscreen || g_noPageFlip) {
		if (palette && g_displayBpp == 8) {
			g_primarySurface->lpVtbl->SetPalette(g_primarySurface, palette);
			FrontendDisplay_SetPalette();
		}
		if (g_optNoFullscreen &&
			g_directDraw->lpVtbl->SetCooperativeLevel(g_directDraw, hwnd, DDSCL_NORMAL)) {
			return FrontendDisplay_ReportDirectDrawInitFailure(hwnd, 1);
		}
	} else {
		if (palette && g_displayBpp == 8) {
			g_primarySurface->lpVtbl->SetPalette(g_primarySurface, palette);
		}
	}

	g_textColorCodes[0] = 0xffff;
	g_textColorCodes[1] = FrontendDisplay_PackRGB(0x60, 0x80, 0xff);
	g_textColorCodes[2] = FrontendDisplay_PackRGB(0xff, 0, 0);
	g_textColorCodes[3] = FrontendDisplay_PackRGB(0xff, 0xff, 0);
	g_textColorCodes[4] = FrontendDisplay_PackRGB(0x32, 0x32, 0xff);
	g_textColorCodes[5] = FrontendDisplay_PackRGB(0x80, 0x80, 0xff);
	g_SetCursorPos(0, 0);
	FrontendDisplay_ClearBackBuffer();
	FrontendDisplay_ClearOffscreenSurface();
	FrontendDisplay_PresentFrame();
	return 1;
}

void FrontendDisplay_FreeSurfaces(void) {
	if (g_backBufferLocked.value) {
		FrontendDisplay_UnlockBackBuffer();
	}
	Frontend3D_ShutdownDevice();
	if (g_offscreenSurface) {
		g_offscreenSurface->lpVtbl->Release(g_offscreenSurface);
	}
	if (g_backBufferSurface) {
		g_backBufferSurface->lpVtbl->Release(g_backBufferSurface);
	}
	if (g_frontSurface) {
		g_frontSurface->lpVtbl->Release(g_frontSurface);
	}
	if (g_primarySurface) {
		g_primarySurface->lpVtbl->Release(g_primarySurface);
	}
	if (g_ddPalette) {
		g_ddPalette->lpVtbl->Release(g_ddPalette);
	}
	Mem_Free(g_offscreenBackupBuffer);
	g_offscreenSurface = 0;
	g_backBufferSurface = 0;
	g_frontSurface = 0;
	g_primarySurface = 0;
	g_ddPalette = 0;
	g_offscreenBackupBuffer = 0;
	g_drawSurfacePtr = 0;
	g_drawSurfacePitch = 0;
	g_savedDrawSurfacePitch = 0;
	g_backBufferPitch = 0;
	g_offscreenSurfacePitch = 0;
	g_frontSurfacePitch = 0;
	g_backBufferLocked.value = 0;
}

// FUNCTION: XWA 0x53F800
void FrontendDisplay_DisableEscapeClose(void) { g_escapeCloseEnabled = 0; }

// FUNCTION: XWA 0x53F5B0
void FrontendDisplay_ClearPresentFrameReady(void) { g_presentFrameReady = 0; }

// FUNCTION: XWA 0x53F5C0
int FrontendDisplay_SetSurfaceClearColor(int color) {
	g_surfaceClearColor = color;
	return color;
}

// FUNCTION: XWA 0x540050
IDirectDraw* FrontendDisplay_GetDirectDraw(void) { return g_directDraw; }

// FUNCTION: XWA 0x53FB60
unsigned char* FrontendDisplay_LockSurfaceForFlight(void) {
	HRESULT hr;
	DDSURFACEDESC desc;

	if (g_frontSurface) {
		if (Config_GetSinglePlayerHardware3D()) {
			memset(&desc, 0, sizeof(desc));
			desc.dwSize = 108;
			do {
				while (1) {
					hr = g_frontSurface->lpVtbl->Lock(g_frontSurface, NULL, &desc, 0, NULL);
					if (hr != DX_DDERR_SURFACELOST) {
						break;
					}
					g_frontSurface->lpVtbl->Restore(g_frontSurface);
				}
			} while (hr == DX_DDERR_WASSTILLDRAWING || hr == (HRESULT)0x887601AE ||
					 hr == (HRESULT)0x887601B8);
			return (unsigned char*)desc.lpSurface;
		}
	}
	return g_drawSurfacePtr;
}

// FUNCTION: XWA 0x53FBE0
int FrontendDisplay_UnlockSurfaceForFlight(void) {
	if (g_frontSurface && Config_GetSinglePlayerHardware3D() == 1) {
		g_frontSurface->lpVtbl->Unlock(g_frontSurface, NULL);
	}
	return 1;
}

// FUNCTION: XWA 0x53EF80
unsigned char* FrontendDisplay_LockBackBuffer(void) {
	HRESULT hr;

#ifdef XWA_MODERN
	/* Remaster: draw records emitted from here on target the back
	 * buffer (per-frame transients while offscreen restore is on). */
	XwaSnapshot_SetEmitTarget(XWA_EMIT_TARGET_MAIN);
#endif

	if (!g_directDraw) {
		return 0;
	}
	if (!g_backBufferSurface) {
		return 0;
	}
	g_drawSurfacePitch = g_backBufferPitch;
	if (!g_backBufferLocked.value) {
		g_backBufferDesc.dwSize = 108;
		while (1) {
			hr = g_backBufferSurface->lpVtbl->Lock(g_backBufferSurface, NULL, &g_backBufferDesc, 0, NULL);
			if (hr == DX_DDERR_SURFACELOST) {
				g_backBufferSurface->lpVtbl->Restore(g_backBufferSurface);
				continue;
			}
			if (hr != DX_DDERR_WASSTILLDRAWING && hr != (HRESULT)0x887601AE && hr != (HRESULT)0x887601B8) {
				break;
			}
		}
		g_backBufferLocked.value = 1;
	}
	return (unsigned char*)g_backBufferDesc.lpSurface;
}

// FUNCTION: XWA 0x53F010
int FrontendDisplay_UnlockBackBuffer(void) {
	int result;

	result = (int)(intptr_t)g_directDraw;
	if (g_directDraw) {
		result = (int)(intptr_t)g_backBufferSurface;
		if (g_backBufferSurface) {
			result = g_backBufferSurface->lpVtbl->Unlock(g_backBufferSurface, NULL);
			g_backBufferLocked.value = 0;
		}
	}
	return result;
}

// FUNCTION: XWA 0x53F5D0
int FrontendDisplay_ClearBackBuffer(void) {
	int result;
	int wasLocked;
	FrontendRect rc;
	DDBLTFX fx;

#ifdef XWA_MODERN
	/* Remaster: back-buffer colorfill (the shim's COLORFILL clears only
	 * classic CPU pixels). */
	XwaSnapshot_EmitSurfaceEventAux(XWA_SURFACE_EVENT_BACKBUFFER_CLEAR, 0, 0, 639, 479,
									(int)(uint16_t)g_surfaceClearColor, 0);
#endif

	result = (int)(intptr_t)g_directDraw;
	if (g_directDraw) {
		result = (int)(intptr_t)g_backBufferSurface;
		if (g_backBufferSurface) {
			wasLocked = g_backBufferLocked.value;
			FrontendDisplay_UnlockBackBuffer();
			FrontendDraw_RectAssign(&rc, 0, 0, 640, 480);
			memset(&fx, 0, sizeof(fx));
			fx.dwSize = 100;
			fx.dwFillColor = (uint32_t)g_surfaceClearColor;
			while (1) {
				result = g_backBufferSurface->lpVtbl->Blt(g_backBufferSurface, &rc, NULL, NULL,
														  DDBLT_COLORFILL, &fx);
				if (!result) {
					break;
				}
				if (result == DX_DDERR_SURFACELOST) {
					result = FrontendDisplay_RestoreLostSurfaces();
					if (result) {
						goto relock;
					}
				} else if (result != DX_DDERR_WASSTILLDRAWING) {
					goto relock;
				}
			}
			if (wasLocked) {
				result = (int)(intptr_t)FrontendDisplay_LockBackBuffer();
				g_drawSurfacePtr = (unsigned char*)(intptr_t)result;
			}
			return result;
		relock:
			if (wasLocked) {
				result = (int)(intptr_t)FrontendDisplay_LockBackBuffer();
				g_drawSurfacePtr = (unsigned char*)(intptr_t)result;
			}
		}
	}
	return result;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x53F980
void FrontendDisplay_ClearOffscreenSurface(void) {
	int result;
	int wasLocked;
	FrontendRect rc;
	DDBLTFX fx;

#ifdef XWA_MODERN
	/* Remaster: offscreen-surface colorfill. */
	XwaSnapshot_EmitSurfaceEventAux(XWA_SURFACE_EVENT_OFFSCREEN_CLEAR, 0, 0, 639, 479,
									(int)(uint16_t)g_surfaceClearColor, 0);
#endif

	if (g_directDraw) {
		if (g_offscreenSurface) {
			wasLocked = g_backBufferLocked.word & 0xff;
			FrontendDisplay_UnlockBackBuffer();
			FrontendDraw_RectAssign(&rc, 0, 0, 640, 480);
			memset(&fx, 0, sizeof(fx));
			fx.dwSize = 100;
			fx.dwFillColor = (uint32_t)g_surfaceClearColor;
			while (1) {
				result = g_offscreenSurface->lpVtbl->Blt(g_offscreenSurface, &rc, NULL, NULL, DDBLT_COLORFILL,
														 &fx);
				if (!result) {
					break;
				}
				if (result == DX_DDERR_SURFACELOST) {
					result = FrontendDisplay_RestoreLostSurfaces();
					if (result) {
						goto error_relock;
					}
				} else if (result != DX_DDERR_WASSTILLDRAWING) {
					goto error_relock;
				}
			}
			if (wasLocked) {
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
			}
			return;
		error_relock:
			if (wasLocked) {
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
			}
		}
	}
}

// FUNCTION: XWA 0x53F830
int FrontendDisplay_LockOffscreenSurface(void) {
	HRESULT hr;
	DDSURFACEDESC desc;

#ifdef XWA_MODERN
	/* Remaster: draws now target the persistent offscreen surface. */
	XwaSnapshot_SetEmitTarget(XWA_EMIT_TARGET_OFFSCREEN);
#endif

	if (!g_directDraw) {
		return 0;
	}
	if (!g_offscreenSurface) {
		return 0;
	}
	memset(&desc, 0, sizeof(desc));
	desc.dwSize = 108;
	while (1) {
		hr = g_offscreenSurface->lpVtbl->Lock(g_offscreenSurface, NULL, &desc, 0, NULL);
		if (!hr) {
			break;
		}
		if (hr == DX_DDERR_SURFACELOST) {
			g_offscreenSurface->lpVtbl->Restore(g_offscreenSurface);
		} else if (hr != DX_DDERR_WASSTILLDRAWING) {
			return 0;
		}
	}
	g_drawSurfacePitch = g_offscreenSurfacePitch;
	g_drawSurfacePtr = (unsigned char*)desc.lpSurface;
	return 1;
}

// FUNCTION: XWA 0x53F8D0
int FrontendDisplay_UnlockOffscreenSurface(int saveToBackup) {
	unsigned char* drawSurfacePtr;

#ifdef XWA_MODERN
	/* Remaster: the draw target reverts to the back buffer (the
	 * internal LockBackBuffer call below only fires when it was not
	 * already locked, so tag explicitly). */
	XwaSnapshot_SetEmitTarget(XWA_EMIT_TARGET_MAIN);
#endif

	if (!g_directDraw) {
		return 0;
	}
	if (!g_offscreenSurface) {
		return 0;
	}
	if (g_drawSurfacePtr && g_offscreenBackupBuffer && saveToBackup) {
		memcpy(g_offscreenBackupBuffer, g_drawSurfacePtr,
			   (size_t)FRONTEND_DISPLAY_HEIGHT * (size_t)g_offscreenSurfacePitch);
	}
	g_offscreenSurface->lpVtbl->Unlock(g_offscreenSurface, NULL);
	g_drawSurfacePitch = g_backBufferPitch;
	if (g_backBufferLocked.value) {
		drawSurfacePtr = (unsigned char*)g_backBufferDesc.lpSurface;
	} else {
		drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	}
	g_drawSurfacePtr = drawSurfacePtr;
	return 1;
}

// FUNCTION: XWA 0x53F960
int FrontendDisplay_EnableOffscreenRestore(void) {
	g_offscreenRestoreEnabled = 1;
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x53F970
int FrontendDisplay_DisableOffscreenRestore(void) {
	g_offscreenRestoreEnabled = 0;
	return 0;
}

// FUNCTION: XWA 0x541E90
int FrontendDisplay_SaveBackBuffer(void) {
	unsigned char* src;
	unsigned char* dst;
	int row;
	int wasLocked;

#ifdef XWA_MODERN
	/* Remaster: back -> offscreen promote (composed frame becomes the
	 * persistent screen). */
	XwaSnapshot_EmitSurfaceEvent(XWA_SURFACE_EVENT_BACKBUFFER_SAVE, 0, 0, 639, 479);
#endif

	wasLocked = g_backBufferLocked.value;
	src = FrontendDisplay_LockBackBuffer();
	FrontendDisplay_LockOffscreenSurface();
	dst = g_drawSurfacePtr;
	for (row = 480; row; --row) {
		memcpy(dst, src, 640 * (size_t)(g_displayBpp >> 3));
		dst += g_offscreenSurfacePitch;
		src += g_backBufferPitch;
	}
	FrontendDisplay_UnlockOffscreenSurface(0);
	if (!wasLocked) {
		FrontendDisplay_UnlockBackBuffer();
	}
	return 1;
}

// FUNCTION: XWA 0x541F10
int FrontendDisplay_RestoreBackBuffer(void) {
	unsigned char* dst;
	unsigned char* src;
	int row;
	int wasLocked;

#ifdef XWA_MODERN
	XwaSnapshot_EmitSurfaceEvent(XWA_SURFACE_EVENT_OFFSCREEN_RESTORE, 0, 0, 639, 479);
#endif

	wasLocked = g_backBufferLocked.value;
	dst = FrontendDisplay_LockBackBuffer();
	FrontendDisplay_LockOffscreenSurface();
	src = g_drawSurfacePtr;
	for (row = 480; row; --row) {
		memcpy(dst, src, 640 * (size_t)(g_displayBpp >> 3));
		src += g_offscreenSurfacePitch;
		dst += g_backBufferPitch;
	}
	FrontendDisplay_UnlockOffscreenSurface(0);
	if (!wasLocked) {
		FrontendDisplay_UnlockBackBuffer();
	}
	return 1;
}

// FUNCTION: XWA 0x541F90
int FrontendDisplay_CopyFrontToBackBuffer(void) {
	FrontendRect rc;
	HRESULT hr;
	int wasLocked;

	if (!g_directDraw) {
		return 0;
	}
	if (!g_backBufferSurface) {
		return 0;
	}
	if (!g_frontSurface) {
		return 0;
	}
	wasLocked = g_backBufferLocked.value;
	FrontendDisplay_UnlockBackBuffer();
	FrontendDraw_RectAssign(&rc, 0, 0, 640, 480);
	while (1) {
		hr = g_backBufferSurface->lpVtbl->BltFast(g_backBufferSurface, 0, 0, g_frontSurface, &rc, 0);
		if (!hr) {
			break;
		}
		if (hr == DX_DDERR_SURFACELOST) {
			if (FrontendDisplay_RestoreLostSurfaces()) {
				break;
			}
		} else if (hr != DX_DDERR_WASSTILLDRAWING) {
			break;
		}
	}
	if (wasLocked) {
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	}
	return 1;
}

// FUNCTION: XWA 0x542050
int FrontendDisplay_BlitOffscreenToFront(void) {
	FrontendRect rc;
	HRESULT hr;

	if (!g_directDraw) {
		return 0;
	}
	if (!g_frontSurface) {
		return 0;
	}
	FrontendDraw_RectAssign(&rc, 0, 0, 640, 480);
	while (1) {
		hr = g_frontSurface->lpVtbl->BltFast(g_frontSurface, 0, 0, g_offscreenSurface, &rc, 0);
		if (!hr) {
			break;
		}
		if (hr == DX_DDERR_SURFACELOST) {
			if (FrontendDisplay_RestoreLostSurfaces()) {
				break;
			}
		} else if (hr != DX_DDERR_WASSTILLDRAWING) {
			break;
		}
	}
	return 1;
}

// FUNCTION: XWA 0x5408F0
int FrontendDisplay_SwitchDriver(int driverId) {
	int wasLocked;
	int wasPixelFormat555;

	wasLocked = g_backBufferLocked.word & 0xff;
	FrontendDisplay_UnlockBackBuffer();
	if (Config_GetDisplayDriverIndex(driverId) == 0) {
		if (g_directDraw == g_directDrawSecondary) {
			wasPixelFormat555 = g_pixelFormat555;
			FrontendDisplay_ReleaseSurfaces();
			FrontendDisplay_SelectPrimaryDirectDrawDevice();
			FrontendDisplay_InitSurfaces();
			if (g_pixelFormat555 != wasPixelFormat555) {
				FrontImage_FreeAtlasResources();
				FrontImage_InitAtlasSprites();
				FrontendCursor_FreeResources();
				FrontendCursor_LoadResources();
				FrontImage_RebuildPaletteCache();
			}
		}
	} else if (g_selectedDisplayDriver != (unsigned int)Config_GetDisplayDriverIndex(driverId)) {
		wasPixelFormat555 = g_pixelFormat555;
		FrontendDisplay_ReleaseSurfaces();
		FrontendDisplay_SelectDriver(Config_GetDisplayDriverIndex(driverId));
		FrontendDisplay_SelectActiveDirectDrawDevice();
		FrontendDisplay_InitSurfaces();
		if (g_pixelFormat555 != wasPixelFormat555) {
			FrontImage_FreeAtlasResources();
			FrontImage_InitAtlasSprites();
			FrontendCursor_FreeResources();
			FrontendCursor_LoadResources();
			FrontImage_RebuildPaletteCache();
		}
	} else if (g_directDraw != g_directDrawSecondary) {
		wasPixelFormat555 = g_pixelFormat555;
		FrontendDisplay_ReleaseSurfaces();
		FrontendDisplay_SelectActiveDirectDrawDevice();
		FrontendDisplay_InitSurfaces();
		if (g_pixelFormat555 != wasPixelFormat555) {
			FrontImage_FreeAtlasResources();
			FrontImage_InitAtlasSprites();
			FrontendCursor_FreeResources();
			FrontendCursor_LoadResources();
			FrontImage_RebuildPaletteCache();
		}
	}
	if (wasLocked) {
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	}
	return 1;
}

// FUNCTION: XWA 0x55CCF0
int FrontendDisplay_SelectDriver(int driverIndex) {
	DisplayDriverEntry* driver;
	DxGuid* guid;
	HRESULT result;
	HRESULT success;

	if ((unsigned int)driverIndex < g_displayDriverCount &&
		(unsigned int)driverIndex != g_selectedDisplayDriver) {
		if (g_directDrawSecondary) {
			g_directDrawSecondary->lpVtbl->Release(g_directDrawSecondary);
			g_directDrawSecondary = 0;
			g_secondaryDirectDrawActive = 0;
		}
		driver = &g_displayDriverTable[driverIndex];
		g_selectedDisplayDriver = (unsigned int)driverIndex;
		guid = driver->guid;
		if (guid) {
			success = 0;
			result = DirectDrawCreate(guid, &g_directDrawSecondary, 0);
			if (result != success) {
				g_directDrawSecondary = 0;
				g_secondaryDirectDrawActive = 0;
				return 1;
			}
			g_secondaryDirectDrawActive = 1;
		}
	}
	return 1;
}

// FUNCTION: XWA 0x540890
int FrontendDisplay_SelectActiveDirectDrawDevice(void) {
	if (g_directDrawSecondary) {
		g_directDraw = g_directDrawSecondary;
		return 1;
	}
	g_directDraw = g_directDrawPrimary;
	return 0;
}

// FUNCTION: XWA 0x5408C0
int FrontendDisplay_SelectPrimaryDirectDrawDevice(void) {
	g_directDraw = g_directDrawPrimary;
	return 1;
}

// FUNCTION: XWA 0x540110
int FrontendDisplay_SetWndProcMode(unsigned int mode) {
	g_frontendDisplayWndProcMode = mode & 0xffu;
	return (int)g_frontendDisplayWndProcMode;
}

// FUNCTION: XWA 0x5408D0
int FrontendDisplay_IsSecondaryDirectDrawActive(void) { return g_directDraw == g_directDrawSecondary; }

// FUNCTION: XWA 0x540A00
int FrontendDisplay_RestorePrimaryDriver(void) {
	int wasLocked;
	int wasPixelFormat555;

	wasLocked = g_backBufferLocked.word & 0xff;
	FrontendDisplay_UnlockBackBuffer();
	if (g_directDraw != g_directDrawPrimary) {
		wasPixelFormat555 = g_pixelFormat555;
		FrontendDisplay_ReleaseSurfaces();
		FrontendDisplay_SelectPrimaryDirectDrawDevice();
		FrontendDisplay_InitSurfaces();
		if (g_pixelFormat555 != wasPixelFormat555) {
			FrontImage_FreeAtlasResources();
			FrontImage_InitAtlasSprites();
			FrontendCursor_FreeResources();
			FrontendCursor_LoadResources();
			FrontImage_RebuildPaletteCache();
		}
	}
	if (wasLocked) {
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	}
	return 1;
}

// FUNCTION: XWA 0x540080
int FrontendDisplay_ReinitSurfaces(void) {
	IDirectDraw* directDraw;
	void* hwnd;

	directDraw = g_directDrawSecondary;
	hwnd = g_hWnd;
	if (directDraw) {
		directDraw->lpVtbl->SetCooperativeLevel(directDraw, hwnd, DDSCL_NORMAL);
	}
	FrontendDisplay_InitSurfaces();
	g_SetCursorPos(0, 0);
	FrontendDisplay_ClearOffscreenSurface();
	FrontendDisplay_ClearBackBuffer();
	FrontendDisplay_PresentFrame();
	if (!g_offscreenBackupBuffer) {
		g_offscreenBackupBuffer =
			(unsigned char*)Mem_Alloc(g_offscreenSurfacePitch * FRONTEND_DISPLAY_HEIGHT);
		if (g_offscreenBackupBuffer) {
			memset(g_offscreenBackupBuffer, 0, g_offscreenSurfacePitch * FRONTEND_DISPLAY_HEIGHT);
		}
	}
	return 1;
}

// FUNCTION: XWA 0x5407F0
int FrontendDisplay_ReleaseSurfaces(void) {
	IDirectDraw* directDraw;
	void* hwnd;

	Frontend3D_ShutdownDevice();
	if (g_ddPalette) {
		g_ddPalette->lpVtbl->Release(g_ddPalette);
		g_ddPalette = 0;
	}
	if (g_offscreenSurface) {
		g_offscreenSurface->lpVtbl->Release(g_offscreenSurface);
		g_offscreenSurface = 0;
	}
	if (g_backBufferSurface) {
		g_backBufferSurface->lpVtbl->Release(g_backBufferSurface);
		g_backBufferSurface = 0;
	}
	if (g_optNoFullscreen && g_frontSurface) {
		g_frontSurface->lpVtbl->Release(g_frontSurface);
		g_frontSurface = 0;
	}
	if (g_primarySurface) {
		g_primarySurface->lpVtbl->Release(g_primarySurface);
		g_primarySurface = 0;
		g_frontSurface = 0;
	}
	hwnd = g_hWnd;
	directDraw = g_directDraw;
	directDraw->lpVtbl->SetCooperativeLevel(directDraw, hwnd, DDSCL_NORMAL);
	return 1;
}

// FUNCTION: XWA 0x540060
int FrontendDisplay_ReleaseSurfacesForFlight(void) {
	FrontendDisplay_UnlockBackBuffer();
	CDAudio_CloseDevice();
	FrontendDisplay_ReleaseSurfaces();
	return 1;
}

// FUNCTION: XWA 0x540B00
int FrontendDisplay_AttachExternalSurfaces(IDirectDrawSurface* primarySurface,
										   IDirectDrawSurface* frontSurface,
										   IDirectDrawSurface* backBufferSurface,
										   IDirectDrawSurface* offscreenSurface, int surfaceWidth,
										   int surfaceHeight) {
	DDSURFACEDESC desc;

	g_primarySurface = primarySurface;
	g_backBufferSurface = backBufferSurface;
	g_offscreenSurface = offscreenSurface;
	memset(&desc, 0, sizeof(desc));
	g_frontSurface = frontSurface;
	desc.dwSize = 108;
	g_frontSurface->lpVtbl->GetSurfaceDesc(g_frontSurface, &desc);
	g_frontSurfacePitch = desc.lPitch;
	g_backBufferSurface->lpVtbl->GetSurfaceDesc(g_backBufferSurface, &desc);
	g_backBufferPitch = desc.lPitch;
	g_offscreenSurface->lpVtbl->GetSurfaceDesc(g_offscreenSurface, &desc);
	g_offscreenSurfacePitch = desc.lPitch;
	g_primarySurface->lpVtbl->GetSurfaceDesc(g_primarySurface, &desc);
	g_frontendDisplaySavedPixelFormat555 = (unsigned char)g_pixelFormat555;
	if (((~desc.ddpfPixelFormat.dwGBitMask >> 10) & 1) != (uint32_t)g_pixelFormat555) {
		g_pixelFormat555 = (~desc.ddpfPixelFormat.dwGBitMask & 0x400) != 0;
		FrontImage_BuildAtlasBlendLut();
		FrontImage_RebuildPaletteCache();
	}
	g_drawSurfacePitch = g_backBufferPitch;
	g_presentBlitX = 0;
	g_presentBlitY = 0;
	if (g_offscreenBackupBuffer) {
		Mem_Free(g_offscreenBackupBuffer);
		g_offscreenBackupBuffer = 0;
	}
	g_offscreenBackupBuffer = (unsigned char*)Mem_Alloc(480 * (size_t)g_offscreenSurfacePitch);
	if (g_offscreenBackupBuffer) {
		memset(g_offscreenBackupBuffer, 0, 480 * (size_t)g_offscreenSurfacePitch);
	}
	if (g_frontendDisplayLifecycleScratch) {
		Mem_Free(g_frontendDisplayLifecycleScratch);
		g_frontendDisplayLifecycleScratch = 0;
	}
	g_frontendDisplayLifecycleScratch = Mem_Alloc(307200 * (size_t)(g_displayBpp >> 2));
	g_presentBlitX = (surfaceWidth - 640) >> 1;
	g_presentBlitY = (surfaceHeight - 480) >> 1;
	FrontendDisplay_SetWndProcMode(0);
	return 1;
}

// FUNCTION: XWA 0x540CB0
int FrontendDisplay_DetachExternalSurfaces(void) {
	FrontendDisplay_SetWndProcMode(1);
	g_primarySurface = 0;
	g_backBufferSurface = 0;
	g_frontSurface = 0;
#ifdef XWA_MODERN
	/* The external owner releases this surface immediately after detaching. */
	g_offscreenSurface = 0;
#endif
	g_presentBlitX = 0;
	g_presentBlitY = 0;

	if (g_frontendDisplaySavedPixelFormat555 != (int)g_pixelFormat555) {
		FrontImage_BuildAtlasBlendLut();
		FrontImage_RebuildPaletteCache();
	}
	if (g_offscreenBackupBuffer) {
		Mem_Free(g_offscreenBackupBuffer);
		g_offscreenBackupBuffer = 0;
	}
	if (g_frontendDisplayLifecycleScratch) {
		Mem_Free(g_frontendDisplayLifecycleScratch);
		g_frontendDisplayLifecycleScratch = 0;
	}
	return 1;
}

// FUNCTION: XWA 0x55D190
int FrontendDisplay_SetPalette(void) {
	/* Fullscreen path: push g_displayPalette to the DirectDraw palette. The
	 * original windowed VGA-DAC path (out 0x3C8/0x3C9) is dropped. */
	if (!g_optNoFullscreen) {
		if (g_ddPalette) {
			return g_ddPalette->lpVtbl->SetEntries(g_ddPalette, 0, 0, 256, g_displayPalette);
		}
	}
	return 0;
}

// FUNCTION: XWA 0x53F040
int FrontendDisplay_PresentFrame(void) {
	HRESULT result;
	int count;
	int vblank = 0;
	FrontendRect dst;

	result = (int)(intptr_t)g_directDraw;
	if (!g_directDraw) {
		return result;
	}
	if (g_backBufferLocked.value) {
		FrontendDisplay_UnlockBackBuffer();
	}
	FrontendDraw_RectAssign(&dst, 0, 0, 640, 480);

	if (g_optNoFullscreen) {
		/* Windowed: dirty BltFast(front<-back), then BltFast(primary<-front). */
		if (!g_dirtyRectOverflow && g_dirtyRectBlitEnabled && (count = g_dirtyRectCount) != 0) {
			FrontendDraw_RectCopy(&dst, &g_dirtyRects[g_dirtyRectCount - 1]);
		} else {
			count = 1;
		}
		while (count > 0) {
			while (1) {
				if (g_dirtyRectBlitEnabled == 1) {
					result = g_frontSurface->lpVtbl->BltFast(g_frontSurface, g_presentBlitX + dst.left,
															 g_presentBlitY + dst.top, g_backBufferSurface,
															 &dst, 1);
					dst.top = 0;
				} else {
					result = g_frontSurface->lpVtbl->BltFast(g_frontSurface, g_presentBlitX + dst.left,
															 g_presentBlitY + dst.top, g_backBufferSurface,
															 &dst, 0);
				}
				if (!result) {
					goto win_advance;
				}
				if (result == DX_DDERR_SURFACELOST) {
					break;
				}
				if (result != DX_DDERR_WASSTILLDRAWING) {
					goto win_advance;
				}
			}
			if (FrontendDisplay_RestoreLostSurfaces()) {
			win_advance:
				if (--count == 0) {
					break;
				}
				FrontendDraw_RectCopy(&dst, &g_dirtyRects[count - 1]);
			}
		}
		FrontendDraw_RectAssign(&dst, 0, 0, 640, 480);
		while (1) {
			result = g_primarySurface->lpVtbl->BltFast(g_primarySurface, 0, 0, g_frontSurface, NULL, 0);
			if (!result) {
				goto restore;
			}
			if (result == DX_DDERR_SURFACELOST) {
				if (FrontendDisplay_RestoreLostSurfaces()) {
					goto restore;
				}
			} else if (result != DX_DDERR_WASSTILLDRAWING) {
				goto restore;
			}
		}
	}

	if (g_noPageFlip) {
		/* Fullscreen, no page flip: dirty BltFast(front<-back), then Flip(primary<-front). */
		if (!g_dirtyRectOverflow && g_dirtyRectBlitEnabled && (count = g_dirtyRectCount) != 0) {
			FrontendDraw_RectCopy(&dst, &g_dirtyRects[g_dirtyRectCount - 1]);
		} else {
			count = 1;
		}
		while (count > 0) {
			while (1) {
				if (g_dirtyRectBlitEnabled == 1) {
					result = g_frontSurface->lpVtbl->BltFast(g_frontSurface, g_presentBlitX + dst.left,
															 g_presentBlitY + dst.top, g_backBufferSurface,
															 &dst, 1);
					dst.top = 0;
				} else {
					result = g_frontSurface->lpVtbl->BltFast(g_frontSurface, g_presentBlitX + dst.left,
															 g_presentBlitY + dst.top, g_backBufferSurface,
															 &dst, 0);
				}
				if (!result) {
					goto flip_advance;
				}
				if (result == DX_DDERR_SURFACELOST) {
					break;
				}
				if (result != DX_DDERR_WASSTILLDRAWING) {
					goto flip_advance;
				}
			}
			if (FrontendDisplay_RestoreLostSurfaces()) {
			flip_advance:
				if (--count == 0) {
					break;
				}
				FrontendDraw_RectCopy(&dst, &g_dirtyRects[count - 1]);
			}
		}
		while (1) {
			result = g_primarySurface->lpVtbl->Flip(g_primarySurface, g_frontSurface, 1);
			if (!result) {
				break;
			}
			if (result == DX_DDERR_SURFACELOST) {
				if (FrontendDisplay_RestoreLostSurfaces()) {
					break;
				}
			} else if (result != DX_DDERR_WASSTILLDRAWING) {
				break;
			}
		}
	} else {
		/* Fullscreen page flip: wait for vertical blank, then Flip(primary<-back). */
		while (!g_directDraw->lpVtbl->GetVerticalBlankStatus(g_directDraw, &vblank) && !vblank) {
		}
		if (g_paletteNeedsSet == 1 && g_displayBpp == 8) {
			FrontendDisplay_SetPalette();
			g_paletteNeedsSet = 0;
		}
		while (1) {
			result = g_primarySurface->lpVtbl->Flip(g_primarySurface, g_backBufferSurface, 1);
			if (!result) {
				break;
			}
			if (result == DX_DDERR_SURFACELOST) {
				if (FrontendDisplay_RestoreLostSurfaces()) {
					break;
				}
			} else if (result != DX_DDERR_WASSTILLDRAWING) {
				break;
			}
		}
	}

restore:
	FrontendDraw_RectAssign(&dst, 0, 0, 640, 480);
	if (g_offscreenRestoreEnabled) {
		/* XWA_MODERN note: this per-present restore is the FRAME
		 * BOUNDARY wipe — the presented frame keeps its transients;
		 * the reconstruction models it structurally (output rebuilds
		 * from the persistent RT at the start of every tick), so no
		 * surface event is emitted here. Mid-tick restores
		 * (FrontendDisplay_RestoreBackBuffer) do emit one. */
		if (g_restoreOffscreenOverlayAfterActivate) {
			g_restoreOffscreenOverlayAfterActivate = 0;
			if (g_offscreenBackupBuffer && g_offscreenSurface) {
				DDSURFACEDESC desc;

				memset(&desc, 0, sizeof(desc));
				desc.dwSize = 108;
				while (1) {
					result = g_offscreenSurface->lpVtbl->Lock(g_offscreenSurface, NULL, &desc, 0, NULL);
					if (!result) {
						break;
					}
					if (result == DX_DDERR_SURFACELOST) {
						g_offscreenSurface->lpVtbl->Restore(g_offscreenSurface);
					} else if (result != DX_DDERR_WASSTILLDRAWING) {
						break;
					}
				}
				if (desc.lpSurface) {
					memcpy(desc.lpSurface, g_offscreenBackupBuffer,
						   (size_t)480 * (size_t)g_offscreenSurfacePitch);
					g_offscreenSurface->lpVtbl->Unlock(g_offscreenSurface, NULL);
				}
			}
		}
		if (g_optNoFullscreen || g_noPageFlip) {
			if (g_dirtyRectBlitEnabled && g_frontSurface) {
				while (1) {
					result =
						g_frontSurface->lpVtbl->BltFast(g_frontSurface, 0, 0, g_offscreenSurface, &dst, 0);
					if (!result) {
						break;
					}
					if (result == DX_DDERR_SURFACELOST) {
						if (FrontendDisplay_RestoreLostSurfaces()) {
							goto done;
						}
					} else if (result != DX_DDERR_WASSTILLDRAWING) {
						g_dirtyRectBlitEnabled = 0;
						return result;
					}
				}
			} else {
				result = FrontendDisplay_RestoreBackBuffer();
			}
		} else {
			while (1) {
				result = g_backBufferSurface->lpVtbl->BltFast(g_backBufferSurface, 0, 0, g_offscreenSurface,
															  &dst, 0);
				if (!result) {
					break;
				}
				if (result == DX_DDERR_SURFACELOST) {
					if (FrontendDisplay_RestoreLostSurfaces()) {
						goto done;
					}
				} else if (result != DX_DDERR_WASSTILLDRAWING) {
					g_dirtyRectBlitEnabled = 0;
					return result;
				}
			}
		}
	}
	if (g_presentFrameReady) {
		result = FrontendDisplay_ClearBackBuffer();
	}
done:
	g_dirtyRectBlitEnabled = 0;
	return result;
}

// FUNCTION: XWA 0x540160
void FrontendDisplay_FlipDirectDrawToGDISurface(void) {
	if (g_directDraw) {
		g_directDraw->lpVtbl->FlipToGDISurface(g_directDraw);
	}
}

// FUNCTION: XWA 0x53E710
int FrontendDisplay_ShowGameMessageBox(const char* message) {
	(void)message;

	/* TODO: Reimplement FrontendDisplay_ShowGameMessageBox @ 0x53E710. */
	return 0;
}

// FUNCTION: XWA 0x53F730
void FrontendDisplay_GetScreenClipRect(FrontendRect* outRect) {
	FrontendDraw_RectAssign(outRect, g_clipMinX, g_clipMinY, g_clipMaxX, g_clipMaxY);
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x53F760
void FrontendDisplay_ResetScreenClipRect(void) {
	g_clipMinX = 0;
	g_clipMinY = 0;
	g_clipMaxX = FRONTEND_DISPLAY_WIDTH - 1;
	g_clipMaxY = FRONTEND_DISPLAY_HEIGHT - 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x53F790
void FrontendDisplay_SetScreenClipRect640x480(const FrontendRect* src) {
	FrontendRect rect;

	FrontendDraw_RectCopy(&rect, src);
	if (rect.left < 0) {
		rect.left = 0;
	}

	if (rect.top < 0) {
		rect.top = 0;
	}

	if (rect.right >= 640) {
		rect.right = 639;
	}

	if (rect.bottom >= 480) {
		rect.bottom = 479;
	}

	if (rect.right >= rect.left && rect.bottom >= rect.top) {
		g_clipMinX = rect.left;
		g_clipMinY = rect.top;
		g_clipMaxX = rect.right;
		g_clipMaxY = rect.bottom;
	}
}

/* Reimplements FrontendDisplay_GetReactivatedFlag @ 0x540D30. The flag is set
   when the frontend regains focus and cleared after one screen update slice. */
// FUNCTION: XWA 0x540D30
int FrontendDisplay_GetReactivatedFlag(void) { return g_frontendDisplayReactivated; }

// FUNCTION: XWA 0x540360
int FrontendDisplay_SetDirtyRectBlitEnabled(unsigned char enabled) {
	g_dirtyRectBlitEnabled = enabled;
	return 1;
}

// FUNCTION: XWA 0x53F820
int FrontendDisplay_SetFrameRate(int fps) {
	g_frameIntervalMs = 1000 / fps;
	return g_frameIntervalMs;
}

/* Portable replacement for the WM_ACTIVATEAPP focus-gained write in
   FrontendDisplay_MainWndProc @ 0x53E340. */
void FrontendDisplay_SetReactivatedFlag(int value) { g_frontendDisplayReactivated = value; }

/* Matches the clears in FrontendDisplay_RunMainLoop @ 0x53E760 and
   FrontendDisplay_RunFrame @ 0x53FD00 after the active screen update runs. */
void FrontendDisplay_ClearReactivatedFlag(void) { g_frontendDisplayReactivated = 0; }

// FUNCTION: XWA 0x53F810
int FrontendDisplay_GetFrameCounter(void) { return g_frameCounter; }

// FUNCTION: XWA 0x53FA50
int FrontendDisplay_GetPixelFormat555(void) { return g_pixelFormat555; }

// FUNCTION: XWA 0x50DC50
int Display_IsPixelFormat555(void) {
	if (g_flightRenderToFrontend) {
		if (g_loadingModel && g_useHardware3D) {
			return ModelTexture_IsHardwareFormat555();
		}

		return FrontendDisplay_GetPixelFormat555();
	}

	if (g_loadingModel && g_useHardware3D) {
		return ModelTexture_IsHardwareFormat555();
	}

	return g_pixelFormatCode == 555;
}

// FUNCTION: XWA 0x53FA60
int FrontendDisplay_GetDrawSurfacePitch(void) { return g_drawSurfacePitch; }

// FUNCTION: XWA 0x53FCE0
int FrontendDisplay_GetFrontendOrFlightDrawPitch(void) {
	if (g_frontSurface && Config_GetSinglePlayerHardware3D() == 1) {
		return g_frontSurfacePitch;
	}
	return g_drawSurfacePitch;
}

// FUNCTION: XWA 0x55D270
int FrontendDisplay_PackRGB(unsigned char r, unsigned char g, unsigned char b) {
	int index;
	unsigned int bestDistance;
	int bestIndex;
	FrontendPaletteEntry* entry;

	if (g_displayBpp != 8) {
		if (g_displayBpp == 16) {
			int color;

			if (g_pixelFormat555) {
				color = 32 * (r >> 3) + (g >> 3);
				return (b >> 3) + 32 * color;
			}

			color = 64 * (r >> 3) + (g >> 2);
			return (b >> 3) + 32 * color;
		}

		return g_displayBpp;
	}

	bestDistance = 0x7fffffffu;
	bestIndex = 1;
	entry = &g_displayPalette[1];
	for (index = 1; index < 256; ++entry, ++index) {
		int redDelta;
		int greenDelta;
		int blueDelta;
		unsigned int distance;

		redDelta = (int)entry->peRed - r;
		if (redDelta < 0) {
			redDelta = -redDelta;
		}

		greenDelta = (int)entry->peGreen - g;
		if (greenDelta < 0) {
			greenDelta = -greenDelta;
		}

		blueDelta = (int)entry->peBlue - b;
		if (blueDelta < 0) {
			blueDelta = -blueDelta;
		}

		distance = g_colorDistLUT[blueDelta] + g_colorDistLUT[greenDelta] + g_colorDistLUT[redDelta];
		if (distance == 0) {
			return index;
		}

		if (distance < bestDistance) {
			bestDistance = distance;
			bestIndex = index;
		}
	}

	return bestIndex;
}

// FUNCTION: XWA 0x55CCD0
DisplayDriverEntry* FrontendDisplay_GetDriverTable(unsigned int* outDriverCount) {
	*outDriverCount = g_displayDriverCount;
	return g_displayDriverTable;
}

// FUNCTION: XWA 0x55D070
int FrontendDisplay_DriverSupportsResolutionBpp(unsigned int driverIndex, int resolutionMode, int bpp) {
	if (bpp == 8) {
		if (driverIndex >= g_displayDriverCount) {
			return 0;
		}

		return g_displayDriverTable[driverIndex].supports8bpp[resolutionMode];
	}
	if (bpp == 16) {
		if (driverIndex >= g_displayDriverCount) {
			return 0;
		}

		return g_displayDriverTable[driverIndex].supports16bpp[resolutionMode];
	}

	return 0;
}
