#ifndef XWA_RENDER_STD3D_DEVICE_H
#define XWA_RENDER_STD3D_DEVICE_H

/* std3D hardware-renderer device layer: enumerates a Direct3D device through the
 * DirectDraw/Direct3D compatibility shim, creates the device, viewport, z-buffer
 * and execute buffer, and enumerates texture formats. Recovered from the original
 * std3D_* functions (0x594063, 0x59453F, 0x598C92, 0x598E49, ...). */

#include "xwa_runtime/compat/directx/d3d.h"
#include "xwa_runtime/compat/directx/ddraw.h"
#include "xwa/render/renderer.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tColorMode {
	STDCOLOR_PAL  = 0,
	STDCOLOR_RGB  = 1,
	STDCOLOR_RGBA = 2,
} tColorMode;

/* Color-channel layout of a texture format (ColorInfo, 56 bytes). */
typedef struct ColorInfo {
	tColorMode colorMode;
	int        bpp;
	int        redBPP;
	int        greenBPP;
	int        blueBPP;
	int        redPosShift;
	int        greenPosShift;
	int        bluePosShift;
	int        redPosShiftRight;
	int        greenPosShiftRight;
	int        bluePosShiftRight;
	int        alphaBPP;
	int        alphaPosShift;
	int        alphaPosShiftRight;
} ColorInfo;

/* An enumerated texture format (Std3DTexFmt, 164 bytes). */
typedef struct Std3DTexFmt {
	ColorInfo     colorInfo;
	DDSURFACEDESC ddsd;
} Std3DTexFmt;

/* Direct3D device objects + execute-buffer write cursor (set up by the device
 * chain; used by the std3D execute-buffer builders in renderer_std3d.c). */
extern IDirectDraw*            g_std3DDirectDraw;
extern IDirectDraw*            g_std3DDirectDraw2;
extern IDirect3D*             g_lpD3D;
extern IDirectDrawSurface*     g_lpRenderSurface;
extern int                    g_std3DReleaseRefCount;
extern char                   g_std3DDeviceOpen;
extern char                   g_std3DStartupDone;
extern IDirect3DDevice*        g_d3dDevice;
extern IDirect3DViewport*      g_d3dViewport;
extern IDirect3DExecuteBuffer* g_d3dExecuteBuffer;
extern D3DEXECUTEBUFFERDESC    g_d3dExecBufDesc;
extern uint8_t*                g_d3dWritePtr;
extern uint8_t*                g_d3dExecBufBase;
extern D3DINSTRUCTION*         g_d3dInstrStart;

/* Source color-format descriptors used when converting to each texel format. */
extern ColorInfo g_cfRGB565;
extern ColorInfo g_cfRGBA1555;
extern ColorInfo g_cfRGBA4444;
extern ColorInfo g_cfPal8;

/* Enumerated formats chosen for each target format (set by std3D_CreateDevice). */
extern Std3DTexFmt* g_pFmtRGB565;
extern Std3DTexFmt* g_pFmtRGBA1555;
extern Std3DTexFmt* g_pFmtRGBA4444;
extern Std3DTexFmt* g_pFmtPal8;

/* Fog parameters emitted by std3D_SetRenderState when fog is enabled. */
extern int g_std3DFogColorRed8;
extern int g_std3DFogColorGreen8;
extern int g_std3DFogColorBlue8;
extern int g_std3DFogTableStartBits;
extern int g_std3DFogTableEndBits;

/* Destination texture format selected for a source surface. */
typedef enum Std3DTextureFormatMode {
	STD3D_TEXFMT_RGB565   = 0,
	STD3D_TEXFMT_RGBA1555 = 1,
	STD3D_TEXFMT_RGBA4444 = 2,
	STD3D_TEXFMT_PAL8     = 3,
} Std3DTextureFormatMode;

/* Cached DirectDraw palette for an 8-bit source palette (std3D_GetOrCreatePalette). */
typedef struct Std3DPaletteNode {
	uint16_t*                srcPalette565;
	IDirectDrawPalette*      ddPalette;
	struct Std3DPaletteNode* next;
} Std3DPaletteNode;

extern unsigned int      g_std3DMinTextureWidth;
extern unsigned int      g_std3DMinTextureHeight;
extern int               g_texSurfaceCount;
int                      std3D_GetTextureSurfaceCount(void);
extern Std3DPaletteNode* g_pPaletteListHead;
extern Std3DPaletteNode* g_pPaletteListTail;

/* Texture-surface creation (std3d_texture.c). apSrcAlphaPlanes carries one alpha
 * plane pointer per mip level (NULL when the source has no separate alpha). */
struct Std3DVBuffer;
char std3D_CreateMipSurface(struct Std3DTextureSurface** apOutSurfaces, struct Std3DVBuffer** apSrcVBuffers,
							Std3DTextureFormatMode textureFormatMode, void** apSrcAlphaPlanes, int mipLevelCount,
							char useHardwareMipmaps);
struct Std3DVBuffer* std3D_AllocVBuffer(struct Std3DRasterInfo* pRasterInfo, int reserved1, int reserved2,
										int reserved3);
void                 std3D_FreeVBuffer(struct Std3DVBuffer* pVBuffer);
void*                std3D_GetOrCreatePalette(uint16_t* srcPalette565);
void                 std3D_CreatePaletteForTexture(uint16_t* srcPalette565);
void                 std3D_FreePalettes(void);

/* Maps a Direct3D HRESULT to a descriptive string for "Error %s ..." logs. */
const char*  std3D_GetD3DErrorString(HRESULT result);

/* Device layer entry points (called by Renderer_InitD3DDevice). */
char         std3D_Startup(IDirectDraw* lpDD, IDirectDrawSurface* lpRenderSurface);
unsigned int std3D_SelectBestDevice(const Std3DDeviceCaps* requiredCaps);
int          std3D_CreateDevice(unsigned int deviceIdx, char bUseZBuffer);
char         std3D_CreateViewport(unsigned int width, unsigned int height);
char         std3D_CreateZBuffer(int width, int height);
void         std3D_DestroyDevice(void);
int          std3D_PackRenderBitDepths(unsigned int renderBitDepth);
int          std3D_PackZCmpCaps(unsigned int zCmpCaps);

#ifdef __cplusplus
}
#endif

#endif
