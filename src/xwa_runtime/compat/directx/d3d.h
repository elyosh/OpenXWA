#ifndef XWA_RUNTIME_COMPAT_DIRECTX_D3D_H
#define XWA_RUNTIME_COMPAT_DIRECTX_D3D_H

#include "xwa_runtime/compat/directx/ddraw.h"
#include "xwa_runtime/compat/directx/dx_win_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Curated Direct3D (DX5, IDirect3DDevice v1 execute-buffer) ABI for the shim.
 * Byte-exact against $MSVC_ROOT/INCLUDE/{D3D,D3DTYPES,D3DCAPS}.H.
 * Stdint style matches ddraw.h. */

typedef float    D3DVALUE;
typedef uint32_t D3DCOLOR;
typedef uint32_t D3DTEXTUREHANDLE;
typedef uint32_t D3DMATERIALHANDLE;

/* --- structs ------------------------------------------------------------- */

/* Pre-transformed, lit vertex (screen space + RHW). Owned here; renderer.h
 * includes this header rather than defining its own copy. */
typedef struct D3DTLVERTEX {
	D3DVALUE sx; /* screen x */
	D3DVALUE sy;
	D3DVALUE sz;
	D3DVALUE rhw; /* reciprocal homogeneous w */
	D3DCOLOR color;
	D3DCOLOR specular;
	D3DVALUE tu;
	D3DVALUE tv;
} D3DTLVERTEX;
XWA_DX_ASSERT(d3dtlvertex_chk, sizeof(D3DTLVERTEX) == 32);

typedef struct D3DVIEWPORT {
	uint32_t dwSize;
	uint32_t dwX;
	uint32_t dwY;
	uint32_t dwWidth;
	uint32_t dwHeight;
	D3DVALUE dvScaleX;
	D3DVALUE dvScaleY;
	D3DVALUE dvMaxX;
	D3DVALUE dvMaxY;
	D3DVALUE dvMinZ;
	D3DVALUE dvMaxZ;
} D3DVIEWPORT;
XWA_DX_ASSERT(d3dviewport_chk, sizeof(D3DVIEWPORT) == 44);

typedef struct D3DRECT {
	int32_t x1;
	int32_t y1;
	int32_t x2;
	int32_t y2;
} D3DRECT;

typedef struct D3DSTATUS {
	uint32_t dwFlags;
	uint32_t dwStatus;
	D3DRECT  drExtent;
} D3DSTATUS;

/* Execute-buffer instruction header: bOpcode, bSize, then wCount data units. */
typedef struct D3DINSTRUCTION {
	uint8_t  bOpcode;
	uint8_t  bSize;
	uint16_t wCount;
} D3DINSTRUCTION;
XWA_DX_ASSERT(d3dinstruction_chk, sizeof(D3DINSTRUCTION) == 4);

typedef struct D3DTRIANGLE {
	uint16_t v1;
	uint16_t v2;
	uint16_t v3;
	uint16_t wFlags;
} D3DTRIANGLE;
XWA_DX_ASSERT(d3dtriangle_chk, sizeof(D3DTRIANGLE) == 8);

/* D3DOP_STATERENDER data unit: {render-state token, value}. */
typedef struct D3DSTATE {
	uint32_t dwState; /* D3DRENDERSTATETYPE (or transform/light) */
	uint32_t dwArg;   /* value (also read as D3DVALUE) */
} D3DSTATE;
XWA_DX_ASSERT(d3dstate_chk, sizeof(D3DSTATE) == 8);

typedef struct D3DPROCESSVERTICES {
	uint32_t dwFlags;
	uint16_t wStart;
	uint16_t wDest;
	uint32_t dwCount;
	uint32_t dwReserved;
} D3DPROCESSVERTICES;
XWA_DX_ASSERT(d3dprocessvertices_chk, sizeof(D3DPROCESSVERTICES) == 16);

typedef struct D3DEXECUTEDATA {
	uint32_t  dwSize;
	uint32_t  dwVertexOffset;
	uint32_t  dwVertexCount;
	uint32_t  dwInstructionOffset;
	uint32_t  dwInstructionLength;
	uint32_t  dwHVertexOffset;
	D3DSTATUS dsStatus;
} D3DEXECUTEDATA;
XWA_DX_ASSERT(d3dexecdata_chk, sizeof(D3DEXECUTEDATA) == 48);

typedef struct D3DEXECUTEBUFFERDESC {
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwCaps;
	uint32_t dwBufferSize;
	void*    lpData;
} D3DEXECUTEBUFFERDESC;
XWA_DX_ASSERT32(d3dexecbufdesc_chk, sizeof(D3DEXECUTEBUFFERDESC) == 20);

/* --- enums / flags ------------------------------------------------------- */

enum {
	D3DOP_POINT           = 1,
	D3DOP_LINE            = 2,
	D3DOP_TRIANGLE        = 3,
	D3DOP_MATRIXLOAD      = 4,
	D3DOP_MATRIXMULTIPLY  = 5,
	D3DOP_STATETRANSFORM  = 6,
	D3DOP_STATELIGHT      = 7,
	D3DOP_STATERENDER     = 8,
	D3DOP_PROCESSVERTICES = 9,
	D3DOP_TEXTURELOAD     = 10,
	D3DOP_EXIT            = 11,
	D3DOP_BRANCHFORWARD   = 12,
	D3DOP_SPAN            = 13,
	D3DOP_SETSTATUS       = 14
};

enum {
	D3DPROCESSVERTICES_TRANSFORMLIGHT = 0x00000000,
	D3DPROCESSVERTICES_TRANSFORM      = 0x00000001,
	D3DPROCESSVERTICES_COPY           = 0x00000002,
	D3DPROCESSVERTICES_OPMASK         = 0x00000007
};

enum { D3DEXECUTE_CLIPPED = 0x00000001, D3DEXECUTE_UNCLIPPED = 0x00000002 };

enum {
	D3DTRIFLAG_EDGEENABLE1 = 0x00000100,
	D3DTRIFLAG_EDGEENABLE2 = 0x00000200,
	D3DTRIFLAG_EDGEENABLE3 = 0x00000400
};

/* D3DRENDERSTATETYPE tokens XWA emits. */
typedef enum D3DRENDERSTATETYPE {
	D3DRENDERSTATE_TEXTUREHANDLE   = 1,
	D3DRENDERSTATE_ANTIALIAS       = 2,
	D3DRENDERSTATE_TEXTUREADDRESS  = 3,
	D3DRENDERSTATE_TEXTUREPERSPECTIVE = 4,
	D3DRENDERSTATE_WRAPU           = 5,
	D3DRENDERSTATE_WRAPV           = 6,
	D3DRENDERSTATE_ZENABLE         = 7,
	D3DRENDERSTATE_FILLMODE        = 8,
	D3DRENDERSTATE_SHADEMODE       = 9,
	D3DRENDERSTATE_MONOENABLE      = 11,
	D3DRENDERSTATE_ZWRITEENABLE    = 14,
	D3DRENDERSTATE_ALPHATESTENABLE = 15,
	D3DRENDERSTATE_TEXTUREMAG      = 17,
	D3DRENDERSTATE_TEXTUREMIN      = 18,
	D3DRENDERSTATE_SRCBLEND        = 19,
	D3DRENDERSTATE_DESTBLEND       = 20,
	D3DRENDERSTATE_TEXTUREMAPBLEND = 21,
	D3DRENDERSTATE_CULLMODE        = 22,
	D3DRENDERSTATE_ZFUNC           = 23,
	D3DRENDERSTATE_ALPHAFUNC       = 25,
	D3DRENDERSTATE_DITHERENABLE    = 26,
	D3DRENDERSTATE_BLENDENABLE     = 27,
	D3DRENDERSTATE_FOGENABLE       = 28,
	D3DRENDERSTATE_SPECULARENABLE  = 29,
	D3DRENDERSTATE_SUBPIXEL        = 31,
	D3DRENDERSTATE_SUBPIXELX       = 32,
	D3DRENDERSTATE_STIPPLEDALPHA   = 33,
	D3DRENDERSTATE_FOGCOLOR        = 34,
	D3DRENDERSTATE_FOGTABLEMODE    = 35,
	D3DRENDERSTATE_FOGTABLESTART   = 36,
	D3DRENDERSTATE_FOGTABLEEND     = 37
} D3DRENDERSTATETYPE;

/* Device capability descriptor enumerated via IDirect3D::EnumDevices and read by
 * std3D_EnumDevicesCallback @0x5991CC. Exact DX5 layout (D3DDEVICEDESC = 252
 * bytes, D3DPRIMCAPS = 56); the caps sub-structs the recovered code does not read
 * by field are byte-padded. */
typedef struct D3DPRIMCAPS {
	uint32_t dwSize;
	uint32_t dwMiscCaps;
	uint32_t dwRasterCaps;
	uint32_t dwZCmpCaps;
	uint32_t dwSrcBlendCaps;
	uint32_t dwDestBlendCaps;
	uint32_t dwAlphaCmpCaps;
	uint32_t dwShadeCaps;
	uint32_t dwTextureCaps;
	uint32_t dwTextureFilterCaps;
	uint32_t dwTextureBlendCaps;
	uint32_t dwTextureAddressCaps;
	uint32_t dwStippleWidth;
	uint32_t dwStippleHeight;
} D3DPRIMCAPS;
XWA_DX_ASSERT(d3dprimcaps_chk, sizeof(D3DPRIMCAPS) == 56);

typedef struct D3DDEVICEDESC {
	uint32_t    dwSize;
	uint32_t    dwFlags;
	uint32_t    dcmColorModel;
	uint32_t    dwDevCaps;
	uint8_t     dtcTransformCaps[8];
	int32_t     bClipping;
	uint8_t     dlcLightingCaps[16];
	D3DPRIMCAPS dpcLineCaps;
	D3DPRIMCAPS dpcTriCaps;
	uint32_t    dwDeviceRenderBitDepth;
	uint32_t    dwDeviceZBufferBitDepth;
	uint32_t    dwMaxBufferSize;
	uint32_t    dwMaxVertexCount;
	uint32_t    dwMinTextureWidth;
	uint32_t    dwMinTextureHeight;
	uint32_t    dwMaxTextureWidth;
	uint32_t    dwMaxTextureHeight;
	uint32_t    dwMinStippleWidth;
	uint32_t    dwMaxStippleWidth;
	uint32_t    dwMinStippleHeight;
	uint32_t    dwMaxStippleHeight;
	uint32_t    dwMaxTextureRepeat;
	uint32_t    dwMaxTextureAspectRatio;
	uint32_t    dwMaxAnisotropy;
	float       dvGuardBandLeft;
	float       dvGuardBandTop;
	float       dvGuardBandRight;
	float       dvGuardBandBottom;
	float       dvExtentsAdjust;
	uint32_t    dwStencilCaps;
	uint32_t    dwFVFCaps;
	uint32_t    dwTextureOpCaps;
	uint16_t    wMaxTextureBlendStages;
	uint16_t    wMaxSimultaneousTextures;
} D3DDEVICEDESC;
XWA_DX_ASSERT(d3ddevicedesc_chk, sizeof(D3DDEVICEDESC) == 252);

/* --- interfaces ---------------------------------------------------------- */

typedef struct IDirect3D              IDirect3D;
typedef struct IDirect3DDevice        IDirect3DDevice;
typedef struct IDirect3DViewport      IDirect3DViewport;
typedef struct IDirect3DExecuteBuffer IDirect3DExecuteBuffer;
typedef struct IDirect3DTexture       IDirect3DTexture;

/* IDirect3D::EnumDevices callback: __stdcall, receives the device guid, its
 * description/name strings, and the hardware + software (HEL) capability
 * descriptors; returns nonzero to continue enumeration. */
typedef HRESULT(XWA_DXAPI* D3DEnumDevicesCb)(DxGuid*, char*, char*, D3DDEVICEDESC*, D3DDEVICEDESC*, void*);

typedef struct IDirect3DVtbl {
	HRESULT(XWA_DXAPI* QueryInterface)(IDirect3D*, DxRefIid, void**);              /* 0 */
	uint32_t(XWA_DXAPI* AddRef)(IDirect3D*);                                       /* 1 */
	uint32_t(XWA_DXAPI* Release)(IDirect3D*);                                      /* 2 */
	void* Initialize;                                                             /* 3 */
	HRESULT(XWA_DXAPI* EnumDevices)(IDirect3D*, D3DEnumDevicesCb, void*);          /* 4 */
	void* CreateLight;                                                            /* 5 */
	void* CreateMaterial;                                                         /* 6 */
	HRESULT(XWA_DXAPI* CreateViewport)(IDirect3D*, IDirect3DViewport**, void*);   /* 7 */
	void* FindDevice;                                                             /* 8 */
} IDirect3DVtbl;
struct IDirect3D {
	const IDirect3DVtbl* lpVtbl;
};

typedef struct IDirect3DDeviceVtbl {
	HRESULT(XWA_DXAPI* QueryInterface)(IDirect3DDevice*, DxRefIid, void**); /* 0 */
	uint32_t(XWA_DXAPI* AddRef)(IDirect3DDevice*);                          /* 1 */
	uint32_t(XWA_DXAPI* Release)(IDirect3DDevice*);                         /* 2 */
	void* Initialize;                                                       /* 3 */
	HRESULT(XWA_DXAPI* GetCaps)(IDirect3DDevice*, void*, void*);            /* 4 */
	void* SwapTextureHandles;                                               /* 5 */
	HRESULT(XWA_DXAPI* CreateExecuteBuffer)(IDirect3DDevice*, D3DEXECUTEBUFFERDESC*, IDirect3DExecuteBuffer**,
											void*); /* 6 */
	void* GetStats;                                 /* 7 */
	HRESULT(XWA_DXAPI* Execute)(IDirect3DDevice*, IDirect3DExecuteBuffer*, IDirect3DViewport*,
								uint32_t);                                         /* 8 */
	HRESULT(XWA_DXAPI* AddViewport)(IDirect3DDevice*, IDirect3DViewport*);         /* 9 */
	HRESULT(XWA_DXAPI* DeleteViewport)(IDirect3DDevice*, IDirect3DViewport*);      /* 10 */
	void* NextViewport;                                                            /* 11 */
	void* Pick;                                                                    /* 12 */
	void* GetPickRecords;                                                          /* 13 */
	HRESULT(XWA_DXAPI* EnumTextureFormats)(IDirect3DDevice*, void* /*cb*/, void*); /* 14 */
	void* CreateMatrix;                                                            /* 15 */
	void* SetMatrix;                                                               /* 16 */
	void* GetMatrix;                                                               /* 17 */
	void* DeleteMatrix;                                                            /* 18 */
	HRESULT(XWA_DXAPI* BeginScene)(IDirect3DDevice*);                              /* 19 */
	HRESULT(XWA_DXAPI* EndScene)(IDirect3DDevice*);                                /* 20 */
	void* GetDirect3D;                                                             /* 21 */
} IDirect3DDeviceVtbl;
struct IDirect3DDevice {
	const IDirect3DDeviceVtbl* lpVtbl;
};

typedef struct IDirect3DViewportVtbl {
	HRESULT(XWA_DXAPI* QueryInterface)(IDirect3DViewport*, DxRefIid, void**); /* 0 */
	uint32_t(XWA_DXAPI* AddRef)(IDirect3DViewport*);                          /* 1 */
	uint32_t(XWA_DXAPI* Release)(IDirect3DViewport*);                         /* 2 */
	void* Initialize;                                                         /* 3 */
	void* GetViewport;                                                        /* 4 */
	HRESULT(XWA_DXAPI* SetViewport)(IDirect3DViewport*, D3DVIEWPORT*);        /* 5 */
	void* TransformVertices;                                                  /* 6 */
	void* LightElements;                                                      /* 7 */
	HRESULT(XWA_DXAPI* SetBackground)(IDirect3DViewport*, D3DMATERIALHANDLE); /* 8 */
	void* GetBackground;                                                      /* 9 */
	void* SetBackgroundDepth;                                                 /* 10 */
	void* GetBackgroundDepth;                                                 /* 11 */
	void* Clear;                                                              /* 12 */
	void* AddLight;                                                           /* 13 */
	void* DeleteLight;                                                        /* 14 */
	void* NextLight;                                                          /* 15 */
} IDirect3DViewportVtbl;
struct IDirect3DViewport {
	const IDirect3DViewportVtbl* lpVtbl;
};

typedef struct IDirect3DExecuteBufferVtbl {
	HRESULT(XWA_DXAPI* QueryInterface)(IDirect3DExecuteBuffer*, DxRefIid, void**); /* 0 */
	uint32_t(XWA_DXAPI* AddRef)(IDirect3DExecuteBuffer*);                          /* 1 */
	uint32_t(XWA_DXAPI* Release)(IDirect3DExecuteBuffer*);                         /* 2 */
	void* Initialize;                                                              /* 3 */
	HRESULT(XWA_DXAPI* Lock)(IDirect3DExecuteBuffer*, D3DEXECUTEBUFFERDESC*);      /* 4 */
	HRESULT(XWA_DXAPI* Unlock)(IDirect3DExecuteBuffer*);                           /* 5 */
	HRESULT(XWA_DXAPI* SetExecuteData)(IDirect3DExecuteBuffer*, D3DEXECUTEDATA*);  /* 6 */
	void* GetExecuteData;                                                          /* 7 */
	void* Validate;                                                                /* 8 */
	void* Optimize;                                                                /* 9 */
} IDirect3DExecuteBufferVtbl;
struct IDirect3DExecuteBuffer {
	const IDirect3DExecuteBufferVtbl* lpVtbl;
};

typedef struct IDirect3DTextureVtbl {
	HRESULT(XWA_DXAPI* QueryInterface)(IDirect3DTexture*, DxRefIid, void**);               /* 0 */
	uint32_t(XWA_DXAPI* AddRef)(IDirect3DTexture*);                                        /* 1 */
	uint32_t(XWA_DXAPI* Release)(IDirect3DTexture*);                                       /* 2 */
	void* Initialize;                                                                      /* 3 */
	HRESULT(XWA_DXAPI* GetHandle)(IDirect3DTexture*, IDirect3DDevice*, D3DTEXTUREHANDLE*); /* 4 */
	void* PaletteChanged;                                                                  /* 5 */
	HRESULT(XWA_DXAPI* Load)(IDirect3DTexture*, IDirect3DTexture*);                        /* 6 */
	void* Unload;                                                                          /* 7 */
} IDirect3DTextureVtbl;
struct IDirect3DTexture {
	const IDirect3DTextureVtbl* lpVtbl;
};

/* Interface ids the recovered code passes to QueryInterface. Defined in the shim.
 * The device guid is chosen at runtime (HAL/RGB); the shim recognizes either. */
extern const DxGuid CLSID_IDirectDraw2;
extern const DxGuid CLSID_IDirect3D;
extern const DxGuid CLSID_IDirect3DTexture;
extern const DxGuid IID_IDirect3DHALDevice_Compat;
extern const DxGuid IID_IDirect3DRGBDevice_Compat;
#define IID_IDirect3DTexture_Compat CLSID_IDirect3DTexture

#ifdef __cplusplus
}
#endif

#endif
