#ifndef XWA_RUNTIME_COMPAT_DIRECTX_DDRAW_H
#define XWA_RUNTIME_COMPAT_DIRECTX_DDRAW_H

#include "xwa_runtime/compat/directx/dx_win_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Curated DirectDraw ABI for the compatibility shim, and the single home for the
 * DirectDraw structs the rest of the port shares (assets/model_texture.h includes
 * this header rather than redefining them). Interfaces, structs, flags, and
 * vtable indices are byte-exact against $MSVC_ROOT/INCLUDE/
 * DDRAW.H so recovered code recompiles identically. Only methods XWA actually
 * calls get real prototypes; unused vtable slots are `void*` pads that keep
 * every method's ABI index correct. */

/* Byte-exact size can only equal the 32-bit original when pointers are 32-bit
 * (the matching build). Structs with interior pointers are asserted only there;
 * pointer-free structs are asserted unconditionally. */
#if UINTPTR_MAX == 0xFFFFFFFFu
#define XWA_DX_ASSERT32(name, expr) typedef char name[(expr) ? 1 : -1]
#else
#define XWA_DX_ASSERT32(name, expr) typedef char name[1] /* layout differs on 64-bit port */
#endif
#define XWA_DX_ASSERT(name, expr) typedef char name[(expr) ? 1 : -1]

/* --- shared DirectDraw structs ------------------------------------------- */

typedef struct DDCOLORKEY {
	uint32_t dwColorSpaceLowValue;
	uint32_t dwColorSpaceHighValue;
} DDCOLORKEY;

typedef struct DDSCAPS {
	uint32_t dwCaps;
} DDSCAPS;

/* Driver capabilities (380 bytes). Only dwCaps/dwCaps2/dwCKeyCaps are read by
 * recovered code; the remainder is padded. */
typedef struct DDCAPS {
	uint32_t dwSize;
	uint32_t dwCaps;
	uint32_t dwCaps2;
	uint32_t dwCKeyCaps;
	uint32_t reserved[91];
} DDCAPS;
XWA_DX_ASSERT(ddcaps_chk, sizeof(DDCAPS) == 380);

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc11-extensions"
#endif

typedef struct DDPIXELFORMAT {
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwFourCC;
	union {
		uint32_t dwRGBBitCount;
		uint32_t dwYUVBitCount;
		uint32_t dwZBufferBitDepth;
		uint32_t dwAlphaBitDepth;
		uint32_t dwLuminanceBitCount;
		uint32_t dwBumpBitCount;
	};
	union {
		uint32_t dwRBitMask;
		uint32_t dwYBitMask;
		uint32_t dwStencilBitDepth;
		uint32_t dwLuminanceBitMask;
		uint32_t dwBumpDuBitMask;
	};
	union {
		uint32_t dwGBitMask;
		uint32_t dwUBitMask;
		uint32_t dwZBitMask;
		uint32_t dwBumpDvBitMask;
	};
	union {
		uint32_t dwBBitMask;
		uint32_t dwVBitMask;
		uint32_t dwStencilBitMask;
		uint32_t dwBumpLuminanceBitMask;
	};
	union {
		uint32_t dwRGBAlphaBitMask;
		uint32_t dwYUVAlphaBitMask;
		uint32_t dwLuminanceAlphaBitMask;
		uint32_t dwRGBZBitMask;
		uint32_t dwYUVZBitMask;
	};
} DDPIXELFORMAT;

typedef struct DDSURFACEDESC {
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwHeight;
	uint32_t dwWidth;
	union {
		int32_t  lPitch;
		uint32_t dwLinearSize;
	};
	uint32_t dwBackBufferCount;
	union {
		uint32_t dwMipMapCount;
		uint32_t dwZBufferBitDepth;
		uint32_t dwRefreshRate;
	};
	uint32_t      dwAlphaBitDepth;
	uint32_t      dwReserved;
	void*         lpSurface;
	DDCOLORKEY    ddckCKDestOverlay;
	DDCOLORKEY    ddckCKDestBlt;
	DDCOLORKEY    ddckCKSrcOverlay;
	DDCOLORKEY    ddckCKSrcBlt;
	DDPIXELFORMAT ddpfPixelFormat;
	DDSCAPS       ddsCaps;
} DDSURFACEDESC;
typedef DDSURFACEDESC* LPDDSURFACEDESC;

/* Blt effect block. Union members XWA never uses are plain uint32_t so the struct
 * is pointer-free and 100 bytes on 32- and 64-bit alike; XWA reads only dwSize
 * and the dwFillColor/dwFillDepth union. */
typedef struct DDBLTFX {
	uint32_t dwSize;
	uint32_t dwDDFX;
	uint32_t dwROP;
	uint32_t dwDDROP;
	uint32_t dwRotationAngle;
	uint32_t dwZBufferOpCode;
	uint32_t dwZBufferLow;
	uint32_t dwZBufferHigh;
	uint32_t dwZBufferBaseDest;
	uint32_t dwZDestConstBitDepth;
	uint32_t dwZDestConst;
	uint32_t dwZSrcConstBitDepth;
	uint32_t dwZSrcConst;
	uint32_t dwAlphaEdgeBlendBitDepth;
	uint32_t dwAlphaEdgeBlend;
	uint32_t dwReserved;
	uint32_t dwAlphaDestConstBitDepth;
	uint32_t dwAlphaDestConst;
	uint32_t dwAlphaSrcConstBitDepth;
	uint32_t dwAlphaSrcConst;
	union {
		uint32_t dwFillColor; /* COLORFILL */
		uint32_t dwFillDepth; /* DEPTHFILL (same slot) */
	};
	DDCOLORKEY ddckDestColorkey;
	DDCOLORKEY ddckSrcColorkey;
} DDBLTFX;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

XWA_DX_ASSERT(ddckey_chk, sizeof(DDCOLORKEY) == 8);
XWA_DX_ASSERT(ddscaps_chk, sizeof(DDSCAPS) == 4);
XWA_DX_ASSERT(ddpf_chk, sizeof(DDPIXELFORMAT) == 32);
XWA_DX_ASSERT(ddbltfx_chk, sizeof(DDBLTFX) == 100);
XWA_DX_ASSERT32(ddsd_chk, sizeof(DDSURFACEDESC) == 108);

/* --- flags (subset XWA uses) --------------------------------------------- */

enum {
	DDSD_CAPS            = 0x00000001,
	DDSD_HEIGHT          = 0x00000002,
	DDSD_WIDTH           = 0x00000004,
	DDSD_PITCH           = 0x00000008,
	DDSD_BACKBUFFERCOUNT = 0x00000020,
	DDSD_MIPMAPCOUNT     = 0x00020000,
	DDSD_PIXELFORMAT     = 0x00001000,

	DDSCAPS_BACKBUFFER     = 0x00000004,
	DDSCAPS_MIPMAP         = 0x00400000,
	DDSCAPS_COMPLEX        = 0x00000008,
	DDSCAPS_FLIP           = 0x00000010,
	DDSCAPS_OFFSCREENPLAIN = 0x00000040,
	DDSCAPS_PRIMARYSURFACE = 0x00000200,
	DDSCAPS_SYSTEMMEMORY   = 0x00000800,
	DDSCAPS_TEXTURE        = 0x00001000,
	DDSCAPS_3DDEVICE       = 0x00002000,
	DDSCAPS_VIDEOMEMORY    = 0x00004000,
	DDSCAPS_ZBUFFER        = 0x00020000,
	DDSCAPS_MODEX          = 0x00200000,
	DDSCAPS_ALLOCONLOAD    = 0x04000000,

	DDPF_ALPHAPIXELS     = 0x00000001,
	DDPF_PALETTEINDEXED8 = 0x00000020,
	DDPF_RGB             = 0x00000040,

	DDBLT_COLORFILL = 0x00000400,
	DDBLT_KEYSRC    = 0x00008000,
	DDBLT_WAIT      = 0x01000000,
	DDBLT_DEPTHFILL = 0x02000000,

	DDBLTFAST_NOCOLORKEY  = 0x00000000,
	DDBLTFAST_SRCCOLORKEY = 0x00000001,
	DDBLTFAST_WAIT        = 0x00000010,

	DDSCL_FULLSCREEN  = 0x00000001,
	DDSCL_ALLOWREBOOT = 0x00000002,
	DDSCL_NORMAL      = 0x00000008,
	DDSCL_EXCLUSIVE   = 0x00000010,
	DDSCL_ALLOWMODEX  = 0x00000040,

	DDCKEY_SRCBLT = 0x00000008,

	DDLOCK_SURFACEMEMORYPTR = 0x00000000,
	DDLOCK_WAIT             = 0x00000001,
	DDLOCK_NOSYSLOCK        = 0x00000800,

	DDPCAPS_8BIT     = 0x00000004,
	DDPCAPS_ALLOW256 = 0x00000040
};

/* --- interfaces ---------------------------------------------------------- */

typedef struct IDirectDraw        IDirectDraw;
typedef struct IDirectDrawSurface IDirectDrawSurface;
typedef struct IDirectDrawPalette IDirectDrawPalette;

typedef struct IDirectDrawVtbl {
	HRESULT(XWA_DXAPI* QueryInterface)(IDirectDraw*, DxRefIid, void**); /* 0 */
	uint32_t(XWA_DXAPI* AddRef)(IDirectDraw*);                          /* 1 */
	uint32_t(XWA_DXAPI* Release)(IDirectDraw*);                         /* 2 */
	void* Compact;                                                      /* 3 */
	void* CreateClipper;                                                /* 4 */
	HRESULT(XWA_DXAPI* CreatePalette)(IDirectDraw*, uint32_t, void* /*PALETTEENTRY*/, IDirectDrawPalette**,
									  void*);                                                      /* 5 */
	HRESULT(XWA_DXAPI* CreateSurface)(IDirectDraw*, DDSURFACEDESC*, IDirectDrawSurface**, void*);  /* 6 */
	HRESULT(XWA_DXAPI* DuplicateSurface)(IDirectDraw*, IDirectDrawSurface*, IDirectDrawSurface**); /* 7 */
	void* EnumDisplayModes;                                                                        /* 8 */
	void* EnumSurfaces;                                                                            /* 9 */
	HRESULT(XWA_DXAPI* FlipToGDISurface)(IDirectDraw*);                                           /* 10 */
	HRESULT(XWA_DXAPI* GetCaps)(IDirectDraw*, void* /*DDCAPS*/, void* /*DDCAPS*/);                 /* 11 */
	void* GetDisplayMode;                                                                          /* 12 */
	void* GetFourCCCodes;                                                                          /* 13 */
	void* GetGDISurface;                                                            /* 14 */
	HRESULT(XWA_DXAPI* GetMonitorFrequency)(IDirectDraw*, uint32_t*);               /* 15 */
	HRESULT(XWA_DXAPI* GetScanLine)(IDirectDraw*, uint32_t*);                       /* 16 */
	HRESULT(XWA_DXAPI* GetVerticalBlankStatus)(IDirectDraw*, int32_t*);             /* 17 */
	void* Initialize;                                                                              /* 18 */
	void* RestoreDisplayMode;                                                                      /* 19 */
	HRESULT(XWA_DXAPI* SetCooperativeLevel)(IDirectDraw*, void* /*HWND*/, uint32_t);               /* 20 */
	HRESULT(XWA_DXAPI* SetDisplayMode)(IDirectDraw*, uint32_t, uint32_t, uint32_t);                /* 21 */
	void* WaitForVerticalBlank;                                                                    /* 22 */
	HRESULT(XWA_DXAPI* GetAvailableVidMem)(IDirectDraw*, DDSCAPS*, uint32_t*, uint32_t*);          /* 23 */
} IDirectDrawVtbl;
struct IDirectDraw {
	const IDirectDrawVtbl* lpVtbl;
};

typedef struct IDirectDrawSurfaceVtbl {
	HRESULT(XWA_DXAPI* QueryInterface)(IDirectDrawSurface*, DxRefIid, void**); /* 0 */
	uint32_t(XWA_DXAPI* AddRef)(IDirectDrawSurface*);                          /* 1 */
	uint32_t(XWA_DXAPI* Release)(IDirectDrawSurface*);                         /* 2 */
	HRESULT(XWA_DXAPI* AddAttachedSurface)(IDirectDrawSurface*, IDirectDrawSurface*); /* 3 */
	void* AddOverlayDirtyRect;                                                 /* 4 */
	HRESULT(XWA_DXAPI* Blt)(IDirectDrawSurface*, void* /*RECT*/, IDirectDrawSurface*, void* /*RECT*/,
							uint32_t, DDBLTFX*); /* 5 */
	void* BltBatch;                              /* 6 */
	HRESULT(XWA_DXAPI* BltFast)(IDirectDrawSurface*, uint32_t, uint32_t, IDirectDrawSurface*, void* /*RECT*/,
								uint32_t);                                                         /* 7 */
	HRESULT(XWA_DXAPI* DeleteAttachedSurface)(IDirectDrawSurface*, uint32_t, IDirectDrawSurface*); /* 8 */
	void* EnumAttachedSurfaces;                                                                    /* 9 */
	void* EnumOverlayZOrders;                                                                      /* 10 */
	HRESULT(XWA_DXAPI* Flip)(IDirectDrawSurface*, IDirectDrawSurface*, uint32_t);                  /* 11 */
	HRESULT(XWA_DXAPI* GetAttachedSurface)(IDirectDrawSurface*, DDSCAPS*, IDirectDrawSurface**);   /* 12 */
	void* GetBltStatus;                                                                            /* 13 */
	void* GetCaps;                                                                                 /* 14 */
	void* GetClipper;                                                                              /* 15 */
	HRESULT(XWA_DXAPI* GetColorKey)(IDirectDrawSurface*, uint32_t, DDCOLORKEY*);                   /* 16 */
	void* GetDC;                                                                                   /* 17 */
	HRESULT(XWA_DXAPI* GetFlipStatus)(IDirectDrawSurface*, uint32_t);                              /* 18 */
	void* GetOverlayPosition;                                                                      /* 19 */
	void* GetPalette;                                                                              /* 20 */
	void* GetPixelFormat;                                                                          /* 21 */
	HRESULT(XWA_DXAPI* GetSurfaceDesc)(IDirectDrawSurface*, DDSURFACEDESC*);                       /* 22 */
	void* Initialize;                                                                              /* 23 */
	HRESULT(XWA_DXAPI* IsLost)(IDirectDrawSurface*);                                               /* 24 */
	HRESULT(XWA_DXAPI* Lock)(IDirectDrawSurface*, void* /*RECT*/, DDSURFACEDESC*, uint32_t,
							 void* /*HANDLE*/);                                  /* 25 */
	void* ReleaseDC;                                                             /* 26 */
	HRESULT(XWA_DXAPI* Restore)(IDirectDrawSurface*);                            /* 27 */
	void* SetClipper;                                                            /* 28 */
	HRESULT(XWA_DXAPI* SetColorKey)(IDirectDrawSurface*, uint32_t, DDCOLORKEY*); /* 29 */
	void* SetOverlayPosition;                                                    /* 30 */
	HRESULT(XWA_DXAPI* SetPalette)(IDirectDrawSurface*, IDirectDrawPalette*);    /* 31 */
	HRESULT(XWA_DXAPI* Unlock)(IDirectDrawSurface*, void*);                      /* 32 */
	void* UpdateOverlay;                                                         /* 33 */
	void* UpdateOverlayDisplay;                                                  /* 34 */
	void* UpdateOverlayZOrder;                                                   /* 35 */
} IDirectDrawSurfaceVtbl;
struct IDirectDrawSurface {
	const IDirectDrawSurfaceVtbl* lpVtbl;
};

typedef struct IDirectDrawPaletteVtbl {
	HRESULT(XWA_DXAPI* QueryInterface)(IDirectDrawPalette*, DxRefIid, void**);                /* 0 */
	uint32_t(XWA_DXAPI* AddRef)(IDirectDrawPalette*);                                         /* 1 */
	uint32_t(XWA_DXAPI* Release)(IDirectDrawPalette*);                                        /* 2 */
	void* GetCaps;                                                                            /* 3 */
	HRESULT(XWA_DXAPI* GetEntries)(IDirectDrawPalette*, uint32_t, uint32_t, uint32_t, void*); /* 4 */
	void* Initialize;                                                                         /* 5 */
	HRESULT(XWA_DXAPI* SetEntries)(IDirectDrawPalette*, uint32_t, uint32_t, uint32_t, void*); /* 6 */
} IDirectDrawPaletteVtbl;
struct IDirectDrawPalette {
	const IDirectDrawPaletteVtbl* lpVtbl;
};

/* Single shim entry point; replaces DirectDrawCreate. Returns DX_DD_OK and stores
 * an IDirectDraw callable through its vtable in *out. */
HRESULT XWA_DXAPI DirectDrawCreate(DxGuid* driver, IDirectDraw** out, void* outer);
HRESULT DirectDrawCreate_Compat(DxGuid* driver, IDirectDraw** out, void* outer);

/* Port service (called from the XwaPort_Tick tail, once per host frame):
 * re-submit a required classic frame when the game presented nothing this
 * tick. Pure re-present: no snapshot event or buffer swap. */
void DDrawCompat_ResubmitIfIdle(void);

/* Modern flight presentation policy. Suppression applies only to render-target
 * surfaces: CPU DirectDraw surfaces used by the frontend continue normally. */
void DDrawCompat_SetClassicFlightRenderingSuppressed(int suppressed);
int  DDrawCompat_IsClassicFlightRenderingSuppressed(void);
/* Submit the last complete classic frame even while suppression is active.
 * This is a presentation-only fallback: no PRESENT event or page flip. */
void DDrawCompat_SubmitLastPresented(void);
/* Monotonic count of completed, non-suppressed classic presents. */
uint64_t DDrawCompat_GetClassicFlightFrameSerial(void);

#ifdef __cplusplus
}
#endif

#endif
