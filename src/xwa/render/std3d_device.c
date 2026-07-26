#include "xwa/render/std3d_device.h"

#include "xwa_runtime/compat/middleware_crt.h"
#include "xwa/flight/flight.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"

#include <string.h>

/* --- device-layer globals ------------------------------------------------ */

// GLOBAL: XWA 0x7B1D14
IDirectDraw* g_std3DDirectDraw;
// GLOBAL: XWA 0x7B1D34  (IDirectDraw2; the shim resolves it to the same device)
IDirectDraw* g_std3DDirectDraw2;
// GLOBAL: XWA 0x7B15B8
IDirect3D* g_lpD3D;
// GLOBAL: XWA 0x7B1180
IDirect3DDevice* g_d3dDevice;
// GLOBAL: XWA 0x7B15BC
IDirect3DViewport* g_d3dViewport;
// GLOBAL: XWA 0x7B1D20
IDirect3DExecuteBuffer* g_d3dExecuteBuffer;
// GLOBAL: XWA 0x7B1D18
IDirectDrawSurface* g_lpRenderSurface;
// GLOBAL: XWA 0xB0D8A0
Std3DDevice g_std3DDevices[6];
// GLOBAL: XWA 0x7B1CDC
int g_std3DNumDevices;
// GLOBAL: XWA 0xB0CFA8
int g_std3DCurDeviceIdx;
// GLOBAL: XWA 0xB0CFAC
int g_bFmtExactRGB565;
// GLOBAL: XWA 0xB0E81C
int g_bFmtExactRGBA1555;
// GLOBAL: XWA 0xB0CFA0
int g_bFmtExactRGBA4444;
// GLOBAL: XWA 0xB0D0E0
Std3DTexFmt g_std3DTextureFormats[12];
// GLOBAL: XWA 0x7B1CE4
int g_std3DNumTexFormats;
// GLOBAL: XWA 0xB0E6C0
ColorInfo g_cfRGB565;
// GLOBAL: XWA 0xB0E700
ColorInfo g_cfRGBA1555;
// GLOBAL: XWA 0xB0E680
ColorInfo g_cfRGBA4444;
// GLOBAL: XWA 0xB0CFC0
ColorInfo g_cfPal8;
// GLOBAL: XWA 0x7B1CD4
unsigned int g_std3DCapFlags;
// GLOBAL: XWA 0x60889C
char g_std3DZBufferEnabled;
// GLOBAL: XWA 0x5ABC40
const float g_std3DViewportScaleDivisor = 2.0f;
// GLOBAL: XWA 0x7B1D3C
char g_std3DDeviceOpen;
// GLOBAL: XWA 0x7B1D38
char g_std3DStartupDone;
// GLOBAL: XWA 0x7B1D4C
char g_std3DDeviceEmptyString0[4] = "";
// GLOBAL: XWA 0x7B1D50
char g_std3DDeviceEmptyString1[4] = "";
// GLOBAL: XWA 0x7B1D24
unsigned int g_std3DExecBufSize;
// GLOBAL: XWA 0x7B1390
D3DEXECUTEBUFFERDESC g_d3dExecBufDesc;
// GLOBAL: XWA 0x7B1CCC  (execute-buffer write cursor)
uint8_t* g_d3dWritePtr;
// GLOBAL: XWA 0x7B138C
uint8_t* g_d3dExecBufBase;
// GLOBAL: XWA 0x7B1CD0
D3DINSTRUCTION* g_d3dInstrStart;
// GLOBAL: XWA 0x7B1D1C
int g_std3DReleaseRefCount;
// GLOBAL: XWA 0x7B1D10  (software vertex buffer; unused on the hardware path)
Std3DVBuffer* g_pStd3DVBuffer;
// GLOBAL: XWA 0xB0E6F8
int g_bPal8Available;
// GLOBAL: XWA 0x7B1388
int g_std3DFogColorRed8;
// GLOBAL: XWA 0x7B15C0
int g_std3DFogColorGreen8;
// GLOBAL: XWA 0x7B112C
int g_std3DFogColorBlue8;
// GLOBAL: XWA 0x7B117C
int g_std3DFogTableStartBits;
// GLOBAL: XWA 0x7B1128
int g_std3DFogTableEndBits;
// GLOBAL: XWA 0xB0D028
int g_fmtIdxRGB565;
// GLOBAL: XWA 0xB0E818
int g_fmtIdxRGBA1555;
// GLOBAL: XWA 0xB0CFA4
int g_fmtIdxRGBA4444;
// GLOBAL: XWA 0xB0E820
int g_fmtIdxPal8;
// GLOBAL: XWA 0x7B15A8
int g_unusedStd3DStartupState0;
// GLOBAL: XWA 0x7B15AC
int g_unusedStd3DStartupState1;
// GLOBAL: XWA 0x7B15B0
int g_unusedStd3DStartupState2;
// GLOBAL: XWA 0x7B15B4
uint8_t g_unusedStd3DStartupStateFlag;

/* The render target the device draws into (std3D_BuildRenderTargetDesc @0x593F44
 * fills it; CreateZBuffer copies its 76-byte prefix). */
typedef struct Std3DRenderTargetDesc {
	int width;
	int height;
	int sizeBytes;
	int pitch;
	int widthPixels;
	ColorInfo colorInfo;
} Std3DRenderTargetDesc;

// GLOBAL: XWA 0x7B1130
Std3DRenderTargetDesc g_std3DRenderTargetDesc;
// GLOBAL: XWA 0x7B1CC8
Std3DRenderTargetDesc* g_pStd3DRenderTarget = &g_std3DRenderTargetDesc;

typedef struct Std3DZBufferRasterInfo {
	int storageType;
	int lockCount;
	int bVideoMemory;
	Std3DRasterInfo raster;
	int unk58;
	void* pixels;
} Std3DZBufferRasterInfo;

// GLOBAL: XWA 0xB0E740
Std3DZBufferRasterInfo g_std3DZBufferRasterInfo;
// GLOBAL: XWA 0x7B1D30
Std3DZBufferSurfaceBlock* g_pStd3DZBufferSurfaceBlock;

// GLOBAL: XWA 0x60A8EC
const char g_std3DZBufferCreateSurfaceError[] = "Error %s when creating zBuffer DDraw surface.";
// GLOBAL: XWA 0x60A91C
const char g_std3DZBufferAttachError[] = "Error %s when attaching zbuffer to backbuffer.";
// GLOBAL: XWA 0x60A978
const char g_std3DZBufferGetDescError[] = "Error %s when getting zbuffer surface description.";
// GLOBAL: XWA 0x60A9D8
const char g_std3DVideoMemoryText[] = "video";
// GLOBAL: XWA 0x60A9E0
const char g_std3DSystemMemoryText[] = "system";
// GLOBAL: XWA 0x60A9E8
const char g_std3DZBufferMemoryMessage[] = "ZBuffer in %s memory.";

/* Enumerated formats selected for each target format (originally Std3DTexFmt*;
 * the port had diverged g_pFmtRGB565 to a 4-field ModelTextureHardwareFormat,
 * now reconciled). */
// GLOBAL: XWA 0x7B1CE8
Std3DTexFmt* g_pFmtRGB565;
// GLOBAL: XWA 0x7B1CEC
Std3DTexFmt* g_pFmtRGBA1555;
// GLOBAL: XWA 0x7B1CF0
Std3DTexFmt* g_pFmtRGBA4444;
// GLOBAL: XWA 0x7B1CF4
Std3DTexFmt* g_pFmtPal8;

/* --- startup / caps helpers ---------------------------------------------- */

// FLAGS: /O2 /Og- /Oi-
// FUNCTION: XWA 0x593F44
static void std3D_BuildRenderTargetDesc(unsigned int w, unsigned int h, int pitch) {
	memset(&g_std3DRenderTargetDesc, 0, sizeof(g_std3DRenderTargetDesc));
	g_pStd3DRenderTarget = &g_std3DRenderTargetDesc;
	g_pStd3DRenderTarget->width = (int)w;
	g_pStd3DRenderTarget->height = (int)h;
	g_pStd3DRenderTarget->pitch = pitch;
	g_pStd3DRenderTarget->widthPixels = pitch / 2;
	g_pStd3DRenderTarget->sizeBytes = g_pStd3DRenderTarget->pitch * g_pStd3DRenderTarget->height;
	g_pStd3DRenderTarget->colorInfo.colorMode = STDCOLOR_RGB;
	g_pStd3DRenderTarget->colorInfo.bpp = 16;
	g_pStd3DRenderTarget->colorInfo.redBPP = 5;
	g_pStd3DRenderTarget->colorInfo.greenBPP = 6;
	g_pStd3DRenderTarget->colorInfo.blueBPP = 5;
	g_pStd3DRenderTarget->colorInfo.redPosShift = 11;
	g_pStd3DRenderTarget->colorInfo.greenPosShift = 5;
	g_pStd3DRenderTarget->colorInfo.bluePosShift = 0;
	g_pStd3DRenderTarget->colorInfo.redPosShiftRight = 3;
	g_pStd3DRenderTarget->colorInfo.greenPosShiftRight = 2;
	g_pStd3DRenderTarget->colorInfo.bluePosShiftRight = 3;
	g_pStd3DRenderTarget->colorInfo.alphaBPP = 0;
	g_pStd3DRenderTarget->colorInfo.alphaPosShift = 0;
	g_pStd3DRenderTarget->colorInfo.alphaPosShiftRight = 0;
}

// FUNCTION: XWA 0x593F1D
static void std3D_ResetUnusedStartupState(int value0, int value1, int value2, int flag, int unused0,
										  int unused1, int unused2) {
	/* The original call site supplies seven values, but this retail helper stores four. */
	(void)unused0;
	(void)unused1;
	(void)unused2;
	g_unusedStd3DStartupState0 = value0;
	g_unusedStd3DStartupState1 = value1;
	g_unusedStd3DStartupState2 = value2;
	g_unusedStd3DStartupStateFlag = (uint8_t)flag;
}

typedef struct Std3DErrorString {
	HRESULT result;
	const char* text;
} Std3DErrorString;

/* DirectX 5 HRESULT names used by the original renderer. The compatibility
 * headers intentionally expose only the few results needed by control flow, so
 * this diagnostic table keeps the remaining legacy values local. */
// GLOBAL: XWA 0x6088A0
static const Std3DErrorString g_std3DErrorStringTable[] = {
	{ (HRESULT)0x00000000u, "D3D_OK" },
	{ (HRESULT)0x887602BCu, "D3DERR_BADMAJORVERSION" },
	{ (HRESULT)0x887602BDu, "D3DERR_BADMINORVERSION" },
	{ (HRESULT)0x887602C7u, "D3DERR_EXECUTE_DESTROY_FAILED" },
	{ (HRESULT)0x887602C8u, "D3DERR_EXECUTE_LOCK_FAILED" },
	{ (HRESULT)0x887602C9u, "D3DERR_EXECUTE_UNLOCK_FAILED" },
	{ (HRESULT)0x887602CAu, "D3DERR_EXECUTE_LOCKED" },
	{ (HRESULT)0x887602CBu, "D3DERR_EXECUTE_NOT_LOCKED" },
	{ (HRESULT)0x887602CDu, "D3DERR_EXECUTE_CLIPPED_FAILED" },
	{ (HRESULT)0x887602D1u, "D3DERR_TEXTURE_CREATE_FAILED" },
	{ (HRESULT)0x887602D2u, "D3DERR_TEXTURE_DESTROY_FAILED" },
	{ (HRESULT)0x887602D3u, "D3DERR_TEXTURE_LOCK_FAILED" },
	{ (HRESULT)0x887602D4u, "D3DERR_TEXTURE_UNLOCK_FAILED" },
	{ (HRESULT)0x887602D5u, "D3DERR_TEXTURE_LOAD_FAILED" },
	{ (HRESULT)0x887602D6u, "D3DERR_TEXTURE_SWAP_FAILED" },
	{ (HRESULT)0x887602D7u, "D3DERR_TEXTURE_LOCKED" },
	{ (HRESULT)0x887602D8u, "D3DERR_TEXTURE_NOT_LOCKED" },
	{ (HRESULT)0x887602D9u, "D3DERR_TEXTURE_GETSURF_FAILED" },
	{ (HRESULT)0x887602DBu, "D3DERR_MATRIX_DESTROY_FAILED" },
	{ (HRESULT)0x887602DCu, "D3DERR_MATRIX_SETDATA_FAILED" },
	{ (HRESULT)0x887602DDu, "D3DERR_MATRIX_GETDATA_FAILED" },
	{ (HRESULT)0x887602DEu, "D3DERR_SETVIEWPORTDATA_FAILED" },
	{ (HRESULT)0x887602E5u, "D3DERR_MATERIAL_DESTROY_FAILED" },
	{ (HRESULT)0x887602E6u, "D3DERR_MATERIAL_SETDATA_FAILED" },
	{ (HRESULT)0x887602E7u, "D3DERR_MATERIAL_GETDATA_FAILED" },
	{ (HRESULT)0x887602F9u, "D3DERR_SCENE_NOT_IN_SCENE" },
	{ (HRESULT)0x887602FAu, "D3DERR_SCENE_BEGIN_FAILED" },
	{ (HRESULT)0x887602FBu, "D3DERR_SCENE_END_FAILED" },
	{ (HRESULT)0x00000000u, "DD_OK" },
	{ (HRESULT)0x88760005u, "DDERR_ALREADYINITIALIZED" },
	{ (HRESULT)0x8876000Au, "DDERR_CANNOTATTACHSURFACE" },
	{ (HRESULT)0x88760014u, "DDERR_CANNOTDETACHSURFACE" },
	{ (HRESULT)0x88760028u, "DDERR_CURRENTLYNOTAVAIL" },
	{ (HRESULT)0x88760037u, "DDERR_EXCEPTION" },
	{ (HRESULT)0x80004005u, "DDERR_GENERIC" },
	{ (HRESULT)0x8876005Au, "DDERR_HEIGHTALIGN" },
	{ (HRESULT)0x8876005Fu, "DDERR_INCOMPATIBLEPRIMARY" },
	{ (HRESULT)0x88760064u, "DDERR_INVALIDCAPS" },
	{ (HRESULT)0x8876006Eu, "DDERR_INVALIDCLIPLIST" },
	{ (HRESULT)0x88760078u, "DDERR_INVALIDMODE" },
	{ (HRESULT)0x88760082u, "DDERR_INVALIDOBJECT" },
	{ (HRESULT)0x80070057u, "DDERR_INVALIDPARAMS" },
	{ (HRESULT)0x88760091u, "DDERR_INVALIDPIXELFORMAT" },
	{ (HRESULT)0x88760096u, "DDERR_INVALIDRECT" },
	{ (HRESULT)0x887600A0u, "DDERR_LOCKEDSURFACES" },
	{ (HRESULT)0x887600AAu, "DDERR_NO3D" },
	{ (HRESULT)0x887600B4u, "DDERR_NOALPHAHW" },
	{ (HRESULT)0x887600CDu, "DDERR_NOCLIPLIST" },
	{ (HRESULT)0x887600D2u, "DDERR_NOCOLORCONVHW" },
	{ (HRESULT)0x887600D4u, "DDERR_NOCOOPERATIVELEVELSET" },
	{ (HRESULT)0x887600D7u, "DDERR_NOCOLORKEY" },
	{ (HRESULT)0x887600DCu, "DDERR_NOCOLORKEYHW" },
	{ (HRESULT)0x887600DEu, "DDERR_NODIRECTDRAWSUPPORT" },
	{ (HRESULT)0x887600E1u, "DDERR_NOEXCLUSIVEMODE" },
	{ (HRESULT)0x887600E6u, "DDERR_NOFLIPHW" },
	{ (HRESULT)0x887600F0u, "DDERR_NOGDI" },
	{ (HRESULT)0x887600FAu, "DDERR_NOMIRRORHW" },
	{ (HRESULT)0x887600FFu, "DDERR_NOTFOUND" },
	{ (HRESULT)0x88760104u, "DDERR_NOOVERLAYHW" },
	{ (HRESULT)0x88760118u, "DDERR_NORASTEROPHW" },
	{ (HRESULT)0x88760122u, "DDERR_NOROTATIONHW" },
	{ (HRESULT)0x88760136u, "DDERR_NOSTRETCHHW" },
	{ (HRESULT)0x8876013Cu, "DDERR_NOT4BITCOLOR" },
	{ (HRESULT)0x8876013Du, "DDERR_NOT4BITCOLORINDEX" },
	{ (HRESULT)0x88760140u, "DDERR_NOT8BITCOLOR" },
	{ (HRESULT)0x8876014Au, "DDERR_NOTEXTUREHW" },
	{ (HRESULT)0x8876014Fu, "DDERR_NOVSYNCHW" },
	{ (HRESULT)0x88760154u, "DDERR_NOZBUFFERHW" },
	{ (HRESULT)0x8876015Eu, "DDERR_NOZOVERLAYHW" },
	{ (HRESULT)0x88760168u, "DDERR_OUTOFCAPS" },
	{ (HRESULT)0x8007000Eu, "DDERR_OUTOFMEMORY" },
	{ (HRESULT)0x8876017Cu, "DDERR_OUTOFVIDEOMEMORY" },
	{ (HRESULT)0x8876017Eu, "DDERR_OVERLAYCANTCLIP" },
	{ (HRESULT)0x88760180u, "DDERR_OVERLAYCOLORKEYONLYONEACTIVE" },
	{ (HRESULT)0x88760183u, "DDERR_PALETTEBUSY" },
	{ (HRESULT)0x88760190u, "DDERR_COLORKEYNOTSET" },
	{ (HRESULT)0x8876019Au, "DDERR_SURFACEALREADYATTACHED" },
	{ (HRESULT)0x887601A4u, "DDERR_SURFACEALREADYDEPENDENT" },
	{ (HRESULT)0x887601AEu, "DDERR_SURFACEBUSY" },
	{ (HRESULT)0x887601B8u, "DDERR_SURFACEISOBSCURED" },
	{ (HRESULT)0x887601C2u, "DDERR_SURFACELOST" },
	{ (HRESULT)0x887601CCu, "DDERR_SURFACENOTATTACHED" },
	{ (HRESULT)0x887601D6u, "DDERR_TOOBIGHEIGHT" },
	{ (HRESULT)0x887601E0u, "DDERR_TOOBIGSIZE" },
	{ (HRESULT)0x887601EAu, "DDERR_TOOBIGWIDTH" },
	{ (HRESULT)0x80004001u, "DDERR_UNSUPPORTED" },
	{ (HRESULT)0x887601FEu, "DDERR_UNSUPPORTEDFORMAT" },
	{ (HRESULT)0x88760208u, "DDERR_UNSUPPORTEDMASK" },
	{ (HRESULT)0x88760219u, "DDERR_VERTICALBLANKINPROGRESS" },
	{ (HRESULT)0x8876021Cu, "DDERR_WASSTILLDRAWING" },
	{ (HRESULT)0x88760230u, "DDERR_XALIGN" },
	{ (HRESULT)0x88760231u, "DDERR_INVALIDDIRECTDRAWGUID" },
	{ (HRESULT)0x88760232u, "DDERR_DIRECTDRAWALREADYCREATED" },
	{ (HRESULT)0x88760233u, "DDERR_NODIRECTDRAWHW" },
	{ (HRESULT)0x88760234u, "DDERR_PRIMARYSURFACEALREADYEXISTS" },
	{ (HRESULT)0x88760235u, "DDERR_NOEMULATION" },
	{ (HRESULT)0x88760236u, "DDERR_REGIONTOOSMALL" },
	{ (HRESULT)0x88760237u, "DDERR_CLIPPERISUSINGHWND" },
	{ (HRESULT)0x88760238u, "DDERR_NOCLIPPERATTACHED" },
	{ (HRESULT)0x88760239u, "DDERR_NOHWND" },
	{ (HRESULT)0x8876023Au, "DDERR_HWNDSUBCLASSED" },
	{ (HRESULT)0x8876023Bu, "DDERR_HWNDALREADYSET" },
	{ (HRESULT)0x8876023Cu, "DDERR_NOPALETTEATTACHED" },
	{ (HRESULT)0x8876023Du, "DDERR_NOPALETTEHW" },
	{ (HRESULT)0x8876023Eu, "DDERR_BLTFASTCANTCLIP" },
	{ (HRESULT)0x8876023Fu, "DDERR_NOBLTHW" },
	{ (HRESULT)0x88760240u, "DDERR_NODDROPSHW" },
	{ (HRESULT)0x88760241u, "DDERR_OVERLAYNOTVISIBLE" },
	{ (HRESULT)0x88760242u, "DDERR_NOOVERLAYDEST" },
	{ (HRESULT)0x88760243u, "DDERR_INVALIDPOSITION" },
	{ (HRESULT)0x88760244u, "DDERR_NOTAOVERLAYSURFACE" },
	{ (HRESULT)0x88760245u, "DDERR_EXCLUSIVEMODEALREADYSET" },
	{ (HRESULT)0x88760246u, "DDERR_NOTFLIPPABLE" },
	{ (HRESULT)0x88760247u, "DDERR_CANTDUPLICATE" },
	{ (HRESULT)0x88760248u, "DDERR_NOTLOCKED" },
	{ (HRESULT)0x88760249u, "DDERR_CANTCREATEDC" },
	{ (HRESULT)0x8876024Au, "DDERR_NODC" },
	{ (HRESULT)0x8876024Bu, "DDERR_WRONGMODE" },
	{ (HRESULT)0x8876024Cu, "DDERR_IMPLICITLYCREATED" },
	{ (HRESULT)0x8876024Du, "DDERR_NOTPALETTIZED" },
	{ (HRESULT)0x8876024Eu, "DDERR_UNSUPPORTEDMODE" },
};

static const char* std3D_LookupErrorString(HRESULT result, const Std3DErrorString* table, int count);

// FUNCTION: XWA 0x5944DA
const char* std3D_GetD3DErrorString(HRESULT result) {
	return std3D_LookupErrorString(result, g_std3DErrorStringTable, 121);
}

// FUNCTION: XWA 0x5944F2
static const char* std3D_LookupErrorString(HRESULT result, const Std3DErrorString* table, int count) {
	const char* text;
	int i;

	text = "Unknown Error";
	for (i = 0; i < count; ++i) {
		if (table[i].result == result) {
			text = table[i].text;
			break;
		}
	}

	return text;
}

// FUNCTION: XWA 0x5984BA
char std3D_SetInitialRenderState(void) {
	int blendEnabled;
	uint8_t* base;
	IDirect3DExecuteBuffer* executeBuffer;
	D3DEXECUTEBUFFERDESC desc;
	uint8_t* cursor;
	HRESULT result;
	D3DEXECUTEDATA executeData;
	size_t bufferSize;

	executeBuffer = NULL;
	bufferSize = 0x1000;
	memset(&desc, 0, sizeof(desc));
	desc.dwSize = 20;
	desc.dwFlags = 1;
	desc.dwBufferSize = (uint32_t)bufferSize;
	result = g_d3dDevice->lpVtbl->CreateExecuteBuffer(g_d3dDevice, &desc, &executeBuffer, NULL);
	if (result) {
		DebugPrintf("Error %s creating D3D Execute buffer.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
	}
	result = executeBuffer->lpVtbl->Lock(executeBuffer, &desc);
	if (result) {
		DebugPrintf("Error %s locking D3D Execute buffer.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
	}

	memset(desc.lpData, 0, bufferSize);
	base = (uint8_t*)desc.lpData;
	cursor = (uint8_t*)desc.lpData;

	cursor[0] = D3DOP_STATERENDER;
	cursor[1] = sizeof(D3DSTATE);
	*(uint16_t*)(cursor + 2) = 25;
	cursor += sizeof(D3DINSTRUCTION);

	*(uint32_t*)cursor = D3DRENDERSTATE_TEXTUREPERSPECTIVE;
	*(uint32_t*)(cursor + 4) = (g_std3DCapFlags & 1u) != 0;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_TEXTUREMAG;
	*(uint32_t*)(cursor + 4) = ((g_std3DCapFlags & STD3D_RS_TEXTURE_MAG_LINEAR) != 0) + 1;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_TEXTUREMIN;
	*(uint32_t*)(cursor + 4) = ((g_std3DCapFlags & STD3D_RS_TEXTURE_MIN_LINEAR) != 0) + 1;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_SUBPIXEL;
	*(uint32_t*)(cursor + 4) = (g_std3DCapFlags & 0x10u) != 0;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_SUBPIXELX;
	*(uint32_t*)(cursor + 4) = (g_std3DCapFlags & 0x20u) != 0;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_WRAPU;
	*(uint32_t*)(cursor + 4) = 0;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_WRAPV;
	*(uint32_t*)(cursor + 4) = 0;
	cursor += sizeof(D3DSTATE);

	*(uint32_t*)cursor = D3DRENDERSTATE_BLENDENABLE;
	if ((g_std3DCapFlags & (STD3D_RS_ALPHA_BLEND | STD3D_RS_TEXTURE_MODULATE_ALPHA)) == 0 &&
		(g_std3DCapFlags & STD3D_RS_TEXTURE_BLEND_DECAL) == 0) {
		blendEnabled = 0;
	} else {
		blendEnabled = 1;
	}
	*(uint32_t*)(cursor + 4) = (uint32_t)blendEnabled;
	cursor += sizeof(D3DSTATE);

	if ((g_std3DCapFlags & (STD3D_RS_ALPHA_BLEND | STD3D_RS_TEXTURE_MODULATE_ALPHA)) != 0 ||
		(g_std3DCapFlags & STD3D_RS_TEXTURE_BLEND_DECAL) != 0) {
		if (g_std3DCapFlags & STD3D_RS_TEXTURE_BLEND_DECAL) {
			*(uint32_t*)g_d3dWritePtr = D3DRENDERSTATE_TEXTUREMAPBLEND;
			*(uint32_t*)(g_d3dWritePtr + 4) = 1;
			g_d3dWritePtr += sizeof(D3DSTATE);
		} else {
			if (g_std3DCapFlags & STD3D_RS_TEXTURE_MODULATE_ALPHA) {
				*(uint32_t*)g_d3dWritePtr = D3DRENDERSTATE_TEXTUREMAPBLEND;
				*(uint32_t*)(g_d3dWritePtr + 4) = 4;
				g_d3dWritePtr += sizeof(D3DSTATE);
			} else {
				*(uint32_t*)g_d3dWritePtr = D3DRENDERSTATE_TEXTUREMAPBLEND;
				*(uint32_t*)(g_d3dWritePtr + 4) = 2;
				g_d3dWritePtr += sizeof(D3DSTATE);
			}
		}
		*(uint32_t*)cursor = D3DRENDERSTATE_SRCBLEND;
		*(uint32_t*)(cursor + 4) = 5;
		cursor += sizeof(D3DSTATE);
		*(uint32_t*)cursor = D3DRENDERSTATE_DESTBLEND;
		*(uint32_t*)(cursor + 4) = 6;
		cursor += sizeof(D3DSTATE);
	} else {
		*(uint32_t*)cursor = D3DRENDERSTATE_TEXTUREMAPBLEND;
		*(uint32_t*)(cursor + 4) = 2;
		cursor += sizeof(D3DSTATE);
		*(uint32_t*)cursor = D3DRENDERSTATE_SRCBLEND;
		*(uint32_t*)(cursor + 4) = 2;
		cursor += sizeof(D3DSTATE);
		*(uint32_t*)cursor = D3DRENDERSTATE_DESTBLEND;
		*(uint32_t*)(cursor + 4) = 1;
		cursor += sizeof(D3DSTATE);
	}

	*(uint32_t*)cursor = D3DRENDERSTATE_ALPHATESTENABLE;
	*(uint32_t*)(cursor + 4) = 1;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_ALPHAFUNC;
	*(uint32_t*)(cursor + 4) = 6;
	cursor += sizeof(D3DSTATE);
	if (g_pStd3DCurDevice->caps.bStippledShade) {
		*(uint32_t*)cursor = D3DRENDERSTATE_STIPPLEDALPHA;
		*(uint32_t*)(cursor + 4) = 1;
		cursor += sizeof(D3DSTATE);
	} else {
		*(uint32_t*)cursor = D3DRENDERSTATE_STIPPLEDALPHA;
		*(uint32_t*)(cursor + 4) = 0;
		cursor += sizeof(D3DSTATE);
	}
	*(uint32_t*)cursor = D3DRENDERSTATE_SHADEMODE;
	*(uint32_t*)(cursor + 4) = 2;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_MONOENABLE;
	*(uint32_t*)(cursor + 4) = (g_std3DCapFlags & STD3D_RS_MONO_DISABLE) == 0;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_SPECULARENABLE;
	*(uint32_t*)(cursor + 4) = (g_std3DCapFlags & 4u) != 0;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_FOGENABLE;
	*(uint32_t*)(cursor + 4) = (g_std3DCapFlags & STD3D_RS_FOG_ENABLE) != 0;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_FILLMODE;
	*(uint32_t*)(cursor + 4) = 3;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_DITHERENABLE;
	*(uint32_t*)(cursor + 4) = (g_std3DCapFlags & 2u) != 0;
	cursor += sizeof(D3DSTATE);
	*(uint32_t*)cursor = D3DRENDERSTATE_ANTIALIAS;
	*(uint32_t*)(cursor + 4) = (g_std3DCapFlags & 8u) != 0;
	cursor += sizeof(D3DSTATE);

	if (g_std3DZBufferEnabled) {
		DebugPrintf("Enabling Z buffer render state.\n", 0, 0, 0, 0);
		*(uint32_t*)cursor = D3DRENDERSTATE_ZENABLE;
		*(uint32_t*)(cursor + 4) = 1;
		cursor += sizeof(D3DSTATE);
		*(uint32_t*)cursor = D3DRENDERSTATE_ZWRITEENABLE;
		*(uint32_t*)(cursor + 4) = 1;
		cursor += sizeof(D3DSTATE);
		*(uint32_t*)cursor = D3DRENDERSTATE_ZFUNC;
		*(uint32_t*)(cursor + 4) =
			(uint32_t)std3D_MapZCmpFunc(g_std3DZCmpMode, g_std3DCapFlags & STD3D_RS_Z_COMPARE_PREFER_EQUAL);
		cursor += sizeof(D3DSTATE);
	} else {
		DebugPrintf("Disabling Z buffer render state.\n", 0, 0, 0, 0);
		*(uint32_t*)cursor = D3DRENDERSTATE_ZENABLE;
		*(uint32_t*)(cursor + 4) = 0;
		cursor += sizeof(D3DSTATE);
		*(uint32_t*)cursor = D3DRENDERSTATE_ZWRITEENABLE;
		*(uint32_t*)(cursor + 4) = 0;
		cursor += sizeof(D3DSTATE);
		*(uint32_t*)cursor = D3DRENDERSTATE_ZFUNC;
		*(uint32_t*)(cursor + 4) =
			(uint32_t)std3D_MapZCmpFunc(g_std3DZCmpMode, g_std3DCapFlags & STD3D_RS_Z_COMPARE_PREFER_EQUAL);
		cursor += sizeof(D3DSTATE);
	}
	*(uint32_t*)cursor = D3DRENDERSTATE_CULLMODE;
	*(uint32_t*)(cursor + 4) = 1;
	cursor += sizeof(D3DSTATE);

	cursor[0] = D3DOP_EXIT;
	cursor[1] = 0;
	*(uint16_t*)(cursor + 2) = 0;
	cursor += sizeof(D3DINSTRUCTION);

	result = executeBuffer->lpVtbl->Unlock(executeBuffer);
	if (result) {
		DebugPrintf("Error %s unlocking D3D Execute buffer.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
	}

	memset(&executeData, 0, sizeof(executeData));
	executeData.dwSize = 48;
	executeData.dwInstructionOffset = 0;
	executeData.dwInstructionLength = (uint32_t)(cursor - base);
	executeBuffer->lpVtbl->SetExecuteData(executeBuffer, &executeData);

	result = g_d3dDevice->lpVtbl->BeginScene(g_d3dDevice);
	if (result) {
		DebugPrintf("Error %s beginning scene.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
	}
	result = g_d3dDevice->lpVtbl->Execute(g_d3dDevice, executeBuffer, g_d3dViewport, D3DEXECUTE_UNCLIPPED);
	if (result) {
		DebugPrintf("Error %s executing buffer.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
	}
	result = g_d3dDevice->lpVtbl->EndScene(g_d3dDevice);
	if (result) {
		DebugPrintf("Error %s ending scene.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
	}

	{
		int debugResult;

		g_std3DReleaseRefCount = (int)executeBuffer->lpVtbl->Release(executeBuffer);
		if (g_std3DReleaseRefCount) {
			debugResult = DebugPrintf("DX object not released properly, refcount:", g_std3DReleaseRefCount);
		} else {
			debugResult = 0;
		}
		(void)debugResult;
	}
	g_d3dStateFlags = (Std3DRenderStateFlags)g_std3DCapFlags;
	DebugPrintf("Initial render state set.\n", 0, 0, 0, 0);
	return 1;
}

// FUNCTION: XWA 0x599EA9
void std3D_CheckExactFmtRGB565(void) {
	ColorInfo* actualFormat;
	ColorInfo* expectedFormat;
	int isExact;

	expectedFormat = &g_cfRGB565;
	actualFormat = &g_pFmtRGB565->colorInfo;

	if (expectedFormat->redBPP == actualFormat->redBPP && expectedFormat->blueBPP == actualFormat->blueBPP &&
		expectedFormat->greenBPP == actualFormat->greenBPP &&
		expectedFormat->alphaBPP == actualFormat->alphaBPP &&
		expectedFormat->redPosShift == actualFormat->redPosShift &&
		expectedFormat->bluePosShift == actualFormat->bluePosShift &&
		expectedFormat->greenPosShift == actualFormat->greenPosShift &&
		expectedFormat->alphaPosShift == actualFormat->alphaPosShift &&
		expectedFormat->redPosShiftRight == actualFormat->redPosShiftRight &&
		expectedFormat->bluePosShiftRight == actualFormat->bluePosShiftRight &&
		expectedFormat->greenPosShiftRight == actualFormat->greenPosShiftRight &&
		expectedFormat->alphaPosShiftRight == actualFormat->alphaPosShiftRight) {
		isExact = 1;
	} else {
		isExact = 0;
	}

	g_bFmtExactRGB565 = isExact;
}

// FUNCTION: XWA 0x599F8F
void std3D_CheckExactFmtRGBA1555(void) {
	ColorInfo* actualFormat;
	ColorInfo* expectedFormat;
	int isExact;

	expectedFormat = &g_cfRGBA1555;
	actualFormat = &g_pFmtRGBA1555->colorInfo;

	if (expectedFormat->redBPP == actualFormat->redBPP && expectedFormat->blueBPP == actualFormat->blueBPP &&
		expectedFormat->greenBPP == actualFormat->greenBPP &&
		expectedFormat->alphaBPP == actualFormat->alphaBPP &&
		expectedFormat->redPosShift == actualFormat->redPosShift &&
		expectedFormat->bluePosShift == actualFormat->bluePosShift &&
		expectedFormat->greenPosShift == actualFormat->greenPosShift &&
		expectedFormat->alphaPosShift == actualFormat->alphaPosShift &&
		expectedFormat->redPosShiftRight == actualFormat->redPosShiftRight &&
		expectedFormat->bluePosShiftRight == actualFormat->bluePosShiftRight &&
		expectedFormat->greenPosShiftRight == actualFormat->greenPosShiftRight &&
		expectedFormat->alphaPosShiftRight == actualFormat->alphaPosShiftRight) {
		isExact = 1;
	} else {
		isExact = 0;
	}

	g_bFmtExactRGBA1555 = isExact;
}

// FUNCTION: XWA 0x59A075
void std3D_CheckExactFmtRGBA4444(void) {
	ColorInfo* actualFormat;
	ColorInfo* expectedFormat;
	int isExact;

	expectedFormat = &g_cfRGBA4444;
	actualFormat = &g_pFmtRGBA4444->colorInfo;

	if (expectedFormat->redBPP == actualFormat->redBPP && expectedFormat->blueBPP == actualFormat->blueBPP &&
		expectedFormat->greenBPP == actualFormat->greenBPP &&
		expectedFormat->alphaBPP == actualFormat->alphaBPP &&
		expectedFormat->redPosShift == actualFormat->redPosShift &&
		expectedFormat->bluePosShift == actualFormat->bluePosShift &&
		expectedFormat->greenPosShift == actualFormat->greenPosShift &&
		expectedFormat->alphaPosShift == actualFormat->alphaPosShift &&
		expectedFormat->redPosShiftRight == actualFormat->redPosShiftRight &&
		expectedFormat->bluePosShiftRight == actualFormat->bluePosShiftRight &&
		expectedFormat->greenPosShiftRight == actualFormat->greenPosShiftRight &&
		expectedFormat->alphaPosShiftRight == actualFormat->alphaPosShiftRight) {
		isExact = 1;
	} else {
		isExact = 0;
	}

	g_bFmtExactRGBA4444 = isExact;
}

// TODO: separately tracked dependency, not yet matched.
// FUNCTION: XWA 0x597F62
char std3D_QueryTextureVidMem(uint32_t* pTotal, uint32_t* pFree) {
	DDSCAPS caps;
	HRESULT result;

	caps.dwCaps = DDSCAPS_TEXTURE;
	result = g_std3DDirectDraw2->lpVtbl->GetAvailableVidMem(g_std3DDirectDraw2, &caps, pTotal, pFree);
	if (result) {
		return 0;
	}
	return 1;
}

// FUNCTION: XWA 0x599BAD
int std3D_PackRenderBitDepths(unsigned int ddbdFlags) {
	int v = 0;
	if (ddbdFlags & 0x4000) {
		v |= 1;
	}
	if (ddbdFlags & 0x2000) {
		v |= 2;
	}
	if (ddbdFlags & 0x1000) {
		v |= 4;
	}
	if (ddbdFlags & 0x800) {
		v |= 8;
	}
	if (ddbdFlags & 0x400) {
		v |= 0x10;
	}
	if (ddbdFlags & 0x200) {
		v |= 0x20;
	}
	if (ddbdFlags & 0x100) {
		v |= 0x40;
	}
	return v;
}

// FUNCTION: XWA 0x599CEB
int std3D_PackZCmpCaps(unsigned int d3dpcmpcaps) {
	int v = 0;
	if (d3dpcmpcaps & 1) {
		v |= 1;
	}
	if (d3dpcmpcaps & 4) {
		v |= 4;
	}
	if (d3dpcmpcaps & 2) {
		v |= 2;
	}
	if (d3dpcmpcaps & 8) {
		v |= 8;
	}
	if (d3dpcmpcaps & 0x10) {
		v |= 0x10;
	}
	if (d3dpcmpcaps & 0x20) {
		v |= 0x20;
	}
	if (d3dpcmpcaps & 0x40) {
		v |= 0x40;
	}
	if (d3dpcmpcaps & 0x80) {
		v |= 0x80;
	}
	return v;
}

// FUNCTION: XWA 0x5981B9
static int std3D_FindClosestFormat(const ColorInfo* pMatch, Std3DTexFmt* aFormats, unsigned int numFormats) {
	int bestScore;
	Std3DTexFmt* fmt;
	int bestIdx;
	unsigned int i;
	int score;

	if (!numFormats) {
		return 0;
	}
	score = 0;
	bestScore = 0;
	fmt = aFormats;
	for (bestIdx = 0; (unsigned int)bestIdx < numFormats; ++bestIdx) {
		i = 0;
		if (fmt->colorInfo.colorMode == pMatch->colorMode) {
			++i;
			if (fmt->colorInfo.bpp == pMatch->bpp) {
				++i;
				switch (pMatch->colorMode) {
					case STDCOLOR_RGB:
						if (fmt->colorInfo.redBPP == pMatch->redBPP &&
							fmt->colorInfo.greenBPP == pMatch->greenBPP &&
							fmt->colorInfo.blueBPP == pMatch->blueBPP) {
							++i;
							DebugPrintf("Found a perfect mode match #%d!\n", bestIdx, 0, 0, 0);
							return bestIdx;
						}
						break;
					case STDCOLOR_RGBA:
						if (fmt->colorInfo.colorMode == STDCOLOR_RGBA) {
							++i;
						}
						if (fmt->colorInfo.redBPP == pMatch->redBPP &&
							fmt->colorInfo.greenBPP == pMatch->greenBPP &&
							fmt->colorInfo.blueBPP == pMatch->blueBPP &&
							fmt->colorInfo.alphaBPP == pMatch->alphaBPP) {
							++i;
							DebugPrintf("Found a perfect mode match #%d!\n", bestIdx, 0, 0, 0);
							return bestIdx;
						}
						break;
					case STDCOLOR_PAL:
						++i;
						break;
					default:
						++i;
						DebugPrintf("Found a perfect mode match #%d!\n", bestIdx, 0, 0, 0);
						return bestIdx;
				}
			}
		}
		if ((int)i > score) {
			bestScore = bestIdx;
			score = (int)i;
		}
		++fmt;
	}
	DebugPrintf("Settling for a closest match #%d..\n", bestScore, 0, 0, 0);
	return bestScore;
}

/* --- texture-format enumeration callback --------------------------------- */

// FUNCTION: XWA 0x593330
static int std3D_Log2Floor(int n) {
	int result = 0;
	while (n > 1) {
		n >>= 1;
		++result;
	}
	return result;
}

// FUNCTION: XWA 0x5995F0
static HRESULT XWA_DXAPI std3D_EnumTextureFormats(DDSURFACEDESC* pddsd, void* lParam) {
	Std3DTexFmt* fmt;
	int blueShift;
	uint32_t mask;
	int greenShift;
	int redShift;
	int alphaShift;
	(void)lParam;

	if ((unsigned int)g_std3DNumTexFormats < 12) {
		fmt = &g_std3DTextureFormats[g_std3DNumTexFormats];
		memcpy_0(&fmt->ddsd, pddsd, sizeof(fmt->ddsd));

		if (pddsd->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8) {
			fmt->colorInfo.colorMode = STDCOLOR_PAL;
			fmt->colorInfo.bpp = 8;
			fmt->colorInfo.redPosShift = 0;
			fmt->colorInfo.redPosShiftRight = 0;
			fmt->colorInfo.redBPP = 0;
			fmt->colorInfo.greenPosShift = 0;
			fmt->colorInfo.greenPosShiftRight = 0;
			fmt->colorInfo.greenBPP = 0;
			fmt->colorInfo.bluePosShift = 0;
			fmt->colorInfo.bluePosShiftRight = 0;
			fmt->colorInfo.blueBPP = 0;
			fmt->colorInfo.alphaPosShift = 0;
			fmt->colorInfo.alphaPosShiftRight = 0;
			fmt->colorInfo.alphaBPP = 0;
			DebugPrintf("Found %dbpp palettized tex format.\n", fmt->colorInfo.bpp, 0, 0, 0);
		} else if (pddsd->ddpfPixelFormat.dwFlags & 8) {
			return 1; /* alpha-only format: skip */
		} else if (pddsd->ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS) {
			fmt->colorInfo.colorMode = STDCOLOR_RGBA;
			fmt->colorInfo.bpp = (int)pddsd->ddpfPixelFormat.dwRGBBitCount;

			for (redShift = 0, mask = pddsd->ddpfPixelFormat.dwRBitMask; (mask & 1) == 0;
				 ++redShift, mask >>= 1) {
			}
			fmt->colorInfo.redPosShift = redShift;
			fmt->colorInfo.redPosShiftRight =
				std3D_Log2Floor(0xFF / (pddsd->ddpfPixelFormat.dwRBitMask >> redShift));
			for (redShift = 0; mask & 1; ++redShift, mask >>= 1) {
			}
			fmt->colorInfo.redBPP = redShift;

			for (greenShift = 0, mask = pddsd->ddpfPixelFormat.dwGBitMask; (mask & 1) == 0;
				 ++greenShift, mask >>= 1) {
			}
			fmt->colorInfo.greenPosShift = greenShift;
			fmt->colorInfo.greenPosShiftRight =
				std3D_Log2Floor(0xFF / (pddsd->ddpfPixelFormat.dwGBitMask >> greenShift));
			for (greenShift = 0; mask & 1; ++greenShift, mask >>= 1) {
			}
			fmt->colorInfo.greenBPP = greenShift;

			for (blueShift = 0, mask = pddsd->ddpfPixelFormat.dwBBitMask; (mask & 1) == 0;
				 ++blueShift, mask >>= 1) {
			}
			fmt->colorInfo.bluePosShift = blueShift;
			fmt->colorInfo.bluePosShiftRight =
				std3D_Log2Floor(0xFF / (pddsd->ddpfPixelFormat.dwBBitMask >> blueShift));
			for (blueShift = 0; mask & 1; ++blueShift, mask >>= 1) {
			}
			fmt->colorInfo.blueBPP = blueShift;

			for (alphaShift = 0, mask = pddsd->ddpfPixelFormat.dwRGBAlphaBitMask; (mask & 1) == 0;
				 ++alphaShift, mask >>= 1) {
			}
			fmt->colorInfo.alphaPosShift = alphaShift;
			fmt->colorInfo.alphaPosShiftRight =
				std3D_Log2Floor(0xFF / (pddsd->ddpfPixelFormat.dwRGBAlphaBitMask >> alphaShift));
			for (alphaShift = 0; mask & 1; ++alphaShift, mask >>= 1) {
			}
			fmt->colorInfo.alphaBPP = alphaShift;

			DebugPrintf("Found RGBA tex format (%d:%d:%d:%d).\n", fmt->colorInfo.redBPP,
						fmt->colorInfo.greenBPP, fmt->colorInfo.blueBPP, fmt->colorInfo.alphaBPP);
			++g_std3DNumTexFormats;
			return 1;
		} else {
			fmt->colorInfo.colorMode = STDCOLOR_RGB;
			fmt->colorInfo.bpp = (int)pddsd->ddpfPixelFormat.dwRGBBitCount;

			for (redShift = 0, mask = pddsd->ddpfPixelFormat.dwRBitMask; (mask & 1) == 0;
				 ++redShift, mask >>= 1) {
			}
			fmt->colorInfo.redPosShift = redShift;
			fmt->colorInfo.redPosShiftRight =
				std3D_Log2Floor(0xFF / (pddsd->ddpfPixelFormat.dwRBitMask >> redShift));
			for (redShift = 0; mask & 1; ++redShift, mask >>= 1) {
			}
			fmt->colorInfo.redBPP = redShift;

			for (greenShift = 0, mask = pddsd->ddpfPixelFormat.dwGBitMask; (mask & 1) == 0;
				 ++greenShift, mask >>= 1) {
			}
			fmt->colorInfo.greenPosShift = greenShift;
			fmt->colorInfo.greenPosShiftRight =
				std3D_Log2Floor(0xFF / (pddsd->ddpfPixelFormat.dwGBitMask >> greenShift));
			for (greenShift = 0; mask & 1; ++greenShift, mask >>= 1) {
			}
			fmt->colorInfo.greenBPP = greenShift;

			for (blueShift = 0, mask = pddsd->ddpfPixelFormat.dwBBitMask; (mask & 1) == 0;
				 ++blueShift, mask >>= 1) {
			}
			fmt->colorInfo.bluePosShift = blueShift;
			fmt->colorInfo.bluePosShiftRight =
				std3D_Log2Floor(0xFF / (pddsd->ddpfPixelFormat.dwBBitMask >> blueShift));
			for (blueShift = 0; mask & 1; ++blueShift, mask >>= 1) {
			}
			fmt->colorInfo.blueBPP = blueShift;

			fmt->colorInfo.alphaPosShift = 0;
			fmt->colorInfo.alphaPosShiftRight = 0;
			fmt->colorInfo.alphaBPP = 0;

			DebugPrintf("Found RGB tex format (%d:%d:%d).\n", fmt->colorInfo.redBPP, fmt->colorInfo.greenBPP,
						fmt->colorInfo.blueBPP, 0);
		}
		++g_std3DNumTexFormats;
		return 1;
	}

	DebugPrintf("TEXTURE FORMAT DISCARDED - expand parameter");
	return 0;
}

/* --- device enumeration -------------------------------------------------- */

// FUNCTION: XWA 0x5991CC
static HRESULT XWA_DXAPI std3D_EnumDevicesCallback(DxGuid* lpGuid, char* lpDeviceDesc, char* lpDeviceName,
												   D3DDEVICEDESC* lpHWDesc, D3DDEVICEDESC* lpHELDesc,
												   void* lpUserArg) {
	Std3DDevice* dev;
	(void)lpUserArg;

	if ((unsigned int)g_std3DNumDevices < 6) {
		dev = &g_std3DDevices[g_std3DNumDevices];
		memcpy_0(dev->guid, lpGuid, sizeof(dev->guid));
		strncpy(dev->deviceDescription, lpDeviceDesc, 0x80);
		strncpy(dev->deviceName, lpDeviceName, 0x80);
		if (lpHWDesc->dcmColorModel) {
			dev->caps.bHardware = 1;
			memcpy_0(&dev->d3dDesc, lpHWDesc, sizeof(dev->d3dDesc));
		} else {
			dev->caps.bHardware = 0;
			memcpy_0(&dev->d3dDesc, lpHELDesc, sizeof(dev->d3dDesc));
		}

		dev->caps.colorModelFlags = 0;
		if (((D3DDEVICEDESC*)dev->d3dDesc)->dcmColorModel & 2) {
			dev->caps.colorModelFlags |= 2;
		}
		if (((D3DDEVICEDESC*)dev->d3dDesc)->dcmColorModel & 1) {
			dev->caps.colorModelFlags |= 1;
		}
		dev->caps.bTexturePerspective = (((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwTextureCaps & 1) != 0;
		dev->caps.bHasZBuffer = ((D3DDEVICEDESC*)dev->d3dDesc)->dwDeviceZBufferBitDepth != 0;
		dev->caps.bSquareOnlyTexture = (((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwTextureCaps & 0x20) != 0;
		dev->caps.bClampSupported =
			(((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwTextureAddressCaps & 4) != 0;
		dev->caps.bAlphaTexture = (((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwTextureCaps & 4) != 0;
		dev->caps.bLinearFilter = (((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwTextureFilterCaps & 2) != 0;
		if (((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwShadeCaps & 0x1000) {
			dev->caps.bStippledShade = 0;
		} else if (((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwShadeCaps & 0x2000) {
			dev->caps.bStippledShade = 1;
		} else {
			dev->caps.bStippledShade = 0;
		}
		dev->caps.bAlphaBlend =
			((((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwTextureBlendCaps & 8) != 0 ? 1 : 0) &&
				((((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwShadeCaps & 0x4000) != 0 ? 1 : 0) ||
			dev->caps.bStippledShade;
		dev->caps.bColorKeyTexture = (((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwTextureCaps & 8) != 0;
		dev->caps.renderBitDepthMask =
			std3D_PackRenderBitDepths(((D3DDEVICEDESC*)dev->d3dDesc)->dwDeviceRenderBitDepth);
		dev->caps.zCmpCapsMask = std3D_PackZCmpCaps(((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwZCmpCaps);
		dev->caps.minTextureWidth = 1;
		dev->caps.minTextureHeight = 1;
		dev->caps.maxTextureWidth = 256;
		dev->caps.maxTextureHeight = 256;
		dev->caps.maxBufferSize = ((D3DDEVICEDESC*)dev->d3dDesc)->dwMaxBufferSize;
		dev->caps.maxVertexCount = ((D3DDEVICEDESC*)dev->d3dDesc)->dwMaxVertexCount;

		DebugPrintf("Found |%s|%s|%s|%s| D3D Device\n", dev->caps.bHardware ? "HW" : "SW",
					(dev->caps.colorModelFlags & 1) ? "MONO" : g_std3DDeviceEmptyString1,
					(dev->caps.colorModelFlags & 2) ? "RGB" : g_std3DDeviceEmptyString0,
					dev->caps.bHasZBuffer ? "Z" : "Non-Z");
		DebugPrintf("      |%s|%s|%s| %dbpp\n", dev->caps.bAlphaTexture ? "Alpha" : "No Alpha",
					dev->caps.bStippledShade ? "Stippled" : "Blend",
					dev->caps.bColorKeyTexture ? "Colorkey" : "No Colorkey", dev->caps.renderBitDepthMask);
		DebugPrintf("Description: %s [%s]\n", dev->deviceName, dev->deviceDescription, 0, 0);

		dev->caps.mipmapCapLevel = 0;
		if (((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwTextureFilterCaps & 0x10) {
			dev->caps.mipmapCapLevel = 1;
			if (((D3DDEVICEDESC*)dev->d3dDesc)->dpcTriCaps.dwTextureFilterCaps & 8) {
				dev->caps.mipmapCapLevel = 2;
			}
		}
		++g_std3DNumDevices;
		return 1;
	}
	return 0;
}

// FUNCTION: XWA 0x594063
char std3D_Startup(IDirectDraw* lpDD, IDirectDrawSurface* lpRenderSurface) {
	HRESULT hr;

	g_std3DDirectDraw = lpDD;
	g_lpRenderSurface = lpRenderSurface;
	if (!g_std3DDirectDraw) {
		DebugPrintf("DDraw device not created yet!\n", 0, 0, 0, 0);
	} else {
		g_cfRGB565.colorMode = STDCOLOR_RGB;
		g_cfRGB565.bpp = 16;
		g_cfRGB565.redBPP = 5;
		g_cfRGB565.greenBPP = 6;
		g_cfRGB565.blueBPP = 5;
		g_cfRGB565.alphaBPP = 0;
		g_cfRGB565.redPosShift = 11;
		g_cfRGB565.greenPosShift = 5;
		g_cfRGB565.bluePosShift = 0;
		g_cfRGB565.alphaPosShift = 0;
		g_cfRGB565.redPosShiftRight = 3;
		g_cfRGB565.greenPosShiftRight = 2;
		g_cfRGB565.bluePosShiftRight = 3;
		g_cfRGB565.alphaPosShiftRight = 0;

		g_cfRGBA1555.colorMode = STDCOLOR_RGBA;
		g_cfRGBA1555.bpp = 16;
		g_cfRGBA1555.redBPP = 5;
		g_cfRGBA1555.greenBPP = 5;
		g_cfRGBA1555.blueBPP = 5;
		g_cfRGBA1555.alphaBPP = 1;
		g_cfRGBA1555.redPosShift = 10;
		g_cfRGBA1555.greenPosShift = 5;
		g_cfRGBA1555.bluePosShift = 0;
		g_cfRGBA1555.alphaPosShift = 15;
		g_cfRGBA1555.redPosShiftRight = 3;
		g_cfRGBA1555.greenPosShiftRight = 3;
		g_cfRGBA1555.bluePosShiftRight = 3;
		g_cfRGBA1555.alphaPosShiftRight = 7;

		g_cfRGBA4444.colorMode = STDCOLOR_RGBA;
		g_cfRGBA4444.bpp = 16;
		g_cfRGBA4444.redBPP = 4;
		g_cfRGBA4444.greenBPP = 4;
		g_cfRGBA4444.blueBPP = 4;
		g_cfRGBA4444.alphaBPP = 4;
		g_cfRGBA4444.redPosShift = 8;
		g_cfRGBA4444.greenPosShift = 4;
		g_cfRGBA4444.bluePosShift = 0;
		g_cfRGBA4444.alphaPosShift = 12;
		g_cfRGBA4444.redPosShiftRight = 4;
		g_cfRGBA4444.greenPosShiftRight = 4;
		g_cfRGBA4444.bluePosShiftRight = 4;
		g_cfRGBA4444.alphaPosShiftRight = 4;

		g_cfPal8.colorMode = STDCOLOR_PAL;
		g_cfPal8.bpp = 8;

		g_std3DCapFlags = 0x19B3;
		g_std3DZBufferEnabled = 1;

		DebugPrintf("Creating D3D interface object.\n", 0, 0, 0, 0);
		hr = g_std3DDirectDraw->lpVtbl->QueryInterface(g_std3DDirectDraw, &CLSID_IDirectDraw2,
													   (void**)&g_std3DDirectDraw2);
		if (hr) {
			DebugPrintf("Error %s creating DirectDraw 2 interface object.\n", std3D_GetD3DErrorString(hr), 0,
						0, 0);
		} else {
			hr = g_std3DDirectDraw->lpVtbl->QueryInterface(g_std3DDirectDraw, &CLSID_IDirect3D,
														   (void**)&g_lpD3D);
			if (hr) {
				DebugPrintf("Error %s creating Direct3D interface object.\n", std3D_GetD3DErrorString(hr), 0,
							0, 0);
			} else {
				DebugPrintf("Enumerating D3D devices.\n", 0, 0, 0, 0);
				g_std3DNumDevices = 0;
				hr = g_lpD3D->lpVtbl->EnumDevices(g_lpD3D, std3D_EnumDevicesCallback, NULL);
				if (hr) {
					DebugPrintf("Error %s when enumerating D3D devices.\n", std3D_GetD3DErrorString(hr), 0, 0,
								0);
				} else if (!g_std3DNumDevices) {
					DebugPrintf("No D3D devices found!\n", 0, 0, 0, 0);
				} else {
					DebugPrintf("%d D3D devices found.\n", g_std3DNumDevices, std3D_GetD3DErrorString(hr), 0,
								0);
					g_std3DStartupDone = 1;
					DebugPrintf("Startup Succeeded.\n", 0, 0, 0, 0);
					g_totalTris = 0.0f;
					g_totalVerts = 0.0f;
					g_totalTexSwitches = 0.0f;
					g_totalStateChanges = 0.0f;
					g_totalBytesCached = 0.0f;
					g_totalBytesPurged = 0.0f;
					g_totalFrames = 0.0f;
					return 1;
				}
			}
		}
	}

	if (g_lpD3D) {
		int debugResult;

		g_std3DReleaseRefCount = (int)g_lpD3D->lpVtbl->Release(g_lpD3D);
		if (!g_std3DReleaseRefCount) {
			debugResult = DebugPrintf("DX object released too early, refcount is 0");
		} else {
			debugResult = 0;
		}
		g_lpD3D = NULL;
	}
	if (g_std3DDirectDraw2) {
		int debugResult;

		g_std3DReleaseRefCount = (int)g_std3DDirectDraw2->lpVtbl->Release(g_std3DDirectDraw2);
		if (g_std3DReleaseRefCount) {
			debugResult = DebugPrintf("DX object not released properly, refcount:", g_std3DReleaseRefCount);
		} else {
			debugResult = 0;
		}
		g_std3DDirectDraw2 = NULL;
	}
	g_std3DDirectDraw = NULL;
	g_lpRenderSurface = NULL;
	return 0;
}

// FUNCTION: XWA 0x598089
unsigned int std3D_SelectBestDevice(const Std3DDeviceCaps* requiredCaps) {
	unsigned int selectedIndex;
	unsigned int index;
	int deviceScore;
	int highestScore;

	if (!g_std3DNumDevices) {
		return 0;
	}
	highestScore = 0;
	selectedIndex = 0;
	{
		Std3DDevice* dev;

		dev = g_std3DDevices;
		for (index = 0; index < (unsigned int)g_std3DNumDevices; ++index) {
			deviceScore = 0;
			if (!requiredCaps->bTexturePerspective ||
				dev->caps.bTexturePerspective == requiredCaps->bTexturePerspective) {
				++deviceScore;
				if (!requiredCaps->bHasZBuffer || dev->caps.bHasZBuffer == requiredCaps->bHasZBuffer) {
					++deviceScore;
					if (dev->caps.bHardware == requiredCaps->bHardware) {
						++deviceScore;
						if (requiredCaps->colorModelFlags & dev->caps.colorModelFlags) {
							++deviceScore;
							DebugPrintf("Found a perfect device match #%d!\n", index, 0, 0, 0);
							return index;
						}
					}
				}
			}
			if (deviceScore > highestScore) {
				selectedIndex = index;
				highestScore = deviceScore;
			}
			++dev;
		}
	}
	DebugPrintf("Settling for a closest match #%d..\n", selectedIndex, 0, 0, 0);
	return selectedIndex;
}

// FUNCTION: XWA 0x598C92
char std3D_CreateViewport(unsigned int width, unsigned int height) {
	D3DVIEWPORT vp;
	{
		Std3DViewportRect rect;
		{
			HRESULT result;

			result = g_lpD3D->lpVtbl->CreateViewport(g_lpD3D, &g_d3dViewport, NULL);
			if (result) {
				DebugPrintf("Error %s when creating D3D viewport.\n", std3D_GetD3DErrorString(result), 0, 0,
							0);
				return 0;
			}
			result = g_d3dDevice->lpVtbl->AddViewport(g_d3dDevice, g_d3dViewport);
			if (result) {
				DebugPrintf("Error %s when adding the D3D viewport to the device.\n",
							std3D_GetD3DErrorString(result), 0, 0, 0);
				return 0;
			}
			memset(&vp, 0, sizeof(vp));
			vp.dwSize = 44;
			vp.dwX = vp.dwY = 0;
			vp.dwWidth = width;
			vp.dwHeight = height;
			vp.dvScaleX = (float)vp.dwWidth / g_std3DViewportScaleDivisor;
			vp.dvScaleY = (float)vp.dwHeight / g_std3DViewportScaleDivisor;
			vp.dvMaxX = (float)vp.dwWidth / (g_std3DViewportScaleDivisor * vp.dvScaleX);
			vp.dvMaxY = (float)vp.dwHeight / (g_std3DViewportScaleDivisor * vp.dvScaleY);
			result = g_d3dViewport->lpVtbl->SetViewport(g_d3dViewport, &vp);
			if (result) {
				DebugPrintf("Error %s when creating D3D viewport.\n", std3D_GetD3DErrorString(result), 0, 0,
							0);
				return 0;
			}
			rect.x = 0;
			rect.y = 0;
			rect.width = (int)width;
			rect.height = (int)height;
			std3D_BuildViewportQuad(&rect);
			DebugPrintf("Viewport created successfully.\n", 0, 0, 0, 0);
			return 1;
		}
	}
}

// FUNCTION: XWA 0x598E49
char std3D_CreateZBuffer(int width, int height) {
	HRESULT result;
	unsigned int zBufferBitDepth;

	/* Create the DirectDraw z-buffer surface and attach it to the render surface,
	 * exactly as the original does. Under the shim the render-target backing owns
	 * the real depth target, so the attachment is nominal, but the create /
	 * bit-depth-validate / attach / describe control flow is reproduced. */
	g_std3DZBufferRasterInfo.storageType = 1;
	g_std3DZBufferRasterInfo.bVideoMemory = 0;
	memcpy_0(&g_std3DZBufferRasterInfo.raster, g_pStd3DRenderTarget, sizeof(Std3DRenderTargetDesc));
	g_std3DZBufferRasterInfo.unk58 = 0;
	g_std3DZBufferRasterInfo.pixels = NULL;

	g_pStd3DZBufferSurfaceBlock = &g_std3DZBufferSurface;
	memset(&g_pStd3DZBufferSurfaceBlock->desc, 0, sizeof(g_pStd3DZBufferSurfaceBlock->desc));
	g_pStd3DZBufferSurfaceBlock->desc.dwSize = 108;
	g_pStd3DZBufferSurfaceBlock->desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | 0x40;
	g_pStd3DZBufferSurfaceBlock->desc.ddsCaps.dwCaps = DDSCAPS_ZBUFFER;
	g_pStd3DZBufferSurfaceBlock->desc.dwWidth = (uint32_t)width;
	g_pStd3DZBufferSurfaceBlock->desc.dwHeight = (uint32_t)height;
	if ((char)g_pStd3DCurDevice->caps.bHardware) {
		g_pStd3DZBufferSurfaceBlock->desc.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
	} else {
		g_pStd3DZBufferSurfaceBlock->desc.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
	}

	zBufferBitDepth = ((const D3DDEVICEDESC*)g_pStd3DCurDevice->d3dDesc)->dwDeviceZBufferBitDepth;
	if (zBufferBitDepth & 0x100) {
		g_pStd3DZBufferSurfaceBlock->desc.dwZBufferBitDepth = 32;
	} else if (zBufferBitDepth & 0x400) {
		g_pStd3DZBufferSurfaceBlock->desc.dwZBufferBitDepth = 16;
	} else if (zBufferBitDepth & 0x800) {
		g_pStd3DZBufferSurfaceBlock->desc.dwZBufferBitDepth = 8;
	} else {
		DebugPrintf("Error: unsupported zbuffer bit depth!\n", 0, 0, 0, 0);
		return 0;
	}
	DebugPrintf("ZBuffer depth: %d.\n", g_pStd3DZBufferSurfaceBlock->desc.dwZBufferBitDepth, 0, 0, 0);

	result = g_std3DDirectDraw->lpVtbl->CreateSurface(g_std3DDirectDraw, &g_pStd3DZBufferSurfaceBlock->desc,
													  &g_pStd3DZBufferSurfaceBlock->surface, NULL);
	if (result) {
		DebugPrintf(g_std3DZBufferCreateSurfaceError, std3D_GetD3DErrorString(result), 0, 0, 0);
		return 0;
	}
	result = g_lpRenderSurface->lpVtbl->AddAttachedSurface(g_lpRenderSurface,
														   g_pStd3DZBufferSurfaceBlock->surface);
	if (result) {
		int debugResultAttach;
		DebugPrintf(g_std3DZBufferAttachError, std3D_GetD3DErrorString(result), 0, 0, 0);
		g_std3DReleaseRefCount =
			(int)g_pStd3DZBufferSurfaceBlock->surface->lpVtbl->Release(g_pStd3DZBufferSurfaceBlock->surface);
		if (g_std3DReleaseRefCount) {
			debugResultAttach =
				DebugPrintf("DX object not released properly, refcount:", g_std3DReleaseRefCount);
		} else {
			debugResultAttach = 0;
		}
		g_pStd3DZBufferSurfaceBlock->surface = NULL;
		return 0;
	}
	result = g_pStd3DZBufferSurfaceBlock->surface->lpVtbl->GetSurfaceDesc(
		g_pStd3DZBufferSurfaceBlock->surface, &g_pStd3DZBufferSurfaceBlock->desc);
	if (result) {
		int debugResultDesc;
		DebugPrintf(g_std3DZBufferGetDescError, std3D_GetD3DErrorString(result), 0, 0, 0);
		result = g_lpRenderSurface->lpVtbl->DeleteAttachedSurface(g_lpRenderSurface, 0,
																  g_pStd3DZBufferSurfaceBlock->surface);
		g_std3DReleaseRefCount =
			(int)g_pStd3DZBufferSurfaceBlock->surface->lpVtbl->Release(g_pStd3DZBufferSurfaceBlock->surface);
		if (g_std3DReleaseRefCount) {
			debugResultDesc =
				DebugPrintf("DX object not released properly, refcount:", g_std3DReleaseRefCount);
		} else {
			debugResultDesc = 0;
		}
		g_pStd3DZBufferSurfaceBlock->surface = NULL;
		return 0;
	}
	if (g_pStd3DZBufferSurfaceBlock->desc.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) {
		g_std3DZBufferRasterInfo.bVideoMemory = 1;
	}
	{
		const char* memoryLocation;
		if (g_std3DZBufferRasterInfo.bVideoMemory) {
			memoryLocation = g_std3DVideoMemoryText;
		} else {
			memoryLocation = g_std3DSystemMemoryText;
		}
		DebugPrintf(g_std3DZBufferMemoryMessage, memoryLocation, 0, 0, 0);
	}
	DebugPrintf("ZBuffer created successfully.\n", 0, 0, 0, 0);
	return 1;
}

// FUNCTION: XWA 0x59453F
int std3D_CreateDevice(unsigned int deviceIdx, char bUseZBuffer) {
	int result;
	const char* zCompareText;
	int maxVerts;

	if (g_std3DDeviceOpen) {
		DebugPrintf("Error: Multiple Opens Attempted.\n", 0, 0, 0, 0);
		return 0;
	}
	if (deviceIdx >= (unsigned int)g_std3DNumDevices) {
		return 0;
	}
	g_std3DCurDeviceIdx = (int)deviceIdx;
	g_pStd3DCurDevice = &g_std3DDevices[g_std3DCurDeviceIdx];
	if (bUseZBuffer && g_pStd3DCurDevice->caps.bHasZBuffer && (g_std3DCapFlags & 0x1800) != 0) {
		g_std3DZBufferEnabled = 1;
	} else {
		g_std3DZBufferEnabled = 0;
	}
	if (g_std3DZBufferEnabled) {
		if (!std3D_CreateZBuffer(g_pStd3DRenderTarget->width, g_pStd3DRenderTarget->height)) {
			DebugPrintf("Error creating Z buffer.\n", 0, 0, 0, 0);
			goto fail;
		}
		if (g_pStd3DCurDevice->caps.zCmpCapsMask & 0x10) {
			g_std3DZCmpMode = 0x10;
		} else {
			g_std3DZCmpMode = 0x2;
		}
		if (g_std3DZCmpMode == 0x10) {
			zCompareText = "Greater";
		} else {
			zCompareText = "Less";
		}
		DebugPrintf("Z compare: %s\n", zCompareText, 0, 0, 0);
	}

	DebugPrintf("Creating D3D device #%d.\n", g_std3DCurDeviceIdx, 0, 0, 0);
	result = g_lpRenderSurface->lpVtbl->QueryInterface(g_lpRenderSurface, (DxRefIid)g_pStd3DCurDevice->guid,
													   (void**)&g_d3dDevice);
	if (result) {
		DebugPrintf("Error %s creating Direct3D device.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
		goto fail;
	}

	g_std3DNumTexFormats = 0;
	result = g_d3dDevice->lpVtbl->EnumTextureFormats(g_d3dDevice, (void*)std3D_EnumTextureFormats, NULL);
	if (result) {
		DebugPrintf("Error %s when enumerating D3D device texture formats.\n",
					std3D_GetD3DErrorString(result), 0, 0, 0);
		goto fail;
	}
	if (!g_std3DNumTexFormats) {
		DebugPrintf("Error: no texture formats found.\n", 0, 0, 0, 0);
		goto fail;
	}
	DebugPrintf("%d texture formats found.\n", g_std3DNumTexFormats, std3D_GetD3DErrorString(result), 0, 0);

	if (!std3D_CreateViewport((unsigned int)g_pStd3DRenderTarget->width,
							  (unsigned int)g_pStd3DRenderTarget->height)) {
		DebugPrintf("Error creating viewport.\n", 0, 0, 0, 0);
		goto fail;
	}
	if (!std3D_SetInitialRenderState()) {
		DebugPrintf("Error initializing render state.\n", 0, 0, 0, 0);
		goto fail;
	}
	DebugPrintf("Creating Execute buffer.\n", 0, 0, 0, 0);

	if (!g_pStd3DCurDevice->caps.maxBufferSize) {
		g_std3DExecBufSize = 0x10000;
	} else {
		g_std3DExecBufSize = g_pStd3DCurDevice->caps.maxBufferSize;
	}
	memset(&g_d3dExecBufDesc, 0, sizeof(g_d3dExecBufDesc));
	g_d3dExecBufDesc.dwSize = 20;
	g_d3dExecBufDesc.dwFlags = 1;
	g_d3dExecBufDesc.dwBufferSize = g_std3DExecBufSize;
	/* Max vertices a single execute buffer may hold, clamped to 512 (the original
	 * device limit). std3D_AddVertices rejects a batch that would exceed this. */
	if (!g_pStd3DCurDevice->caps.maxVertexCount) {
		g_d3dMaxVerts = 512;
	} else {
		if (g_pStd3DCurDevice->caps.maxVertexCount < 512) {
			maxVerts = (int)g_pStd3DCurDevice->caps.maxVertexCount;
		} else {
			maxVerts = 512;
		}
		g_d3dMaxVerts = maxVerts;
	}
	DebugPrintf("Execute buffer size: %d.\n", g_std3DExecBufSize, 0, 0, 0);
	DebugPrintf("Max vertices: %d.\n", g_d3dMaxVerts, 0, 0, 0);
	result =
		g_d3dDevice->lpVtbl->CreateExecuteBuffer(g_d3dDevice, &g_d3dExecBufDesc, &g_d3dExecuteBuffer, NULL);
	if (result) {
		DebugPrintf("Error %s creating D3D Execute buffer.\n", std3D_GetD3DErrorString(result), 0, 0, 0);
		goto fail;
	}

	g_std3DOpened = 1;
	if (g_texCacheCount || g_pTexCacheHead || g_pTexCacheTail) {
		DebugPrintf("texture cache reset but not empty!");
	}
	g_texCacheCount = 0;
	g_pTexCacheHead = NULL;
	g_pTexCacheTail = NULL;

	g_fmtIdxRGB565 =
		std3D_FindClosestFormat(&g_cfRGB565, g_std3DTextureFormats, (unsigned int)g_std3DNumTexFormats);
	g_pFmtRGB565 = &g_std3DTextureFormats[g_fmtIdxRGB565];
	std3D_CheckExactFmtRGB565();
	g_fmtIdxRGBA1555 =
		std3D_FindClosestFormat(&g_cfRGBA1555, g_std3DTextureFormats, (unsigned int)g_std3DNumTexFormats);
	g_pFmtRGBA1555 = &g_std3DTextureFormats[g_fmtIdxRGBA1555];
	std3D_CheckExactFmtRGBA1555();
	g_fmtIdxRGBA4444 =
		std3D_FindClosestFormat(&g_cfRGBA4444, g_std3DTextureFormats, (unsigned int)g_std3DNumTexFormats);
	g_pFmtRGBA4444 = &g_std3DTextureFormats[g_fmtIdxRGBA4444];
	std3D_CheckExactFmtRGBA4444();
	g_fmtIdxPal8 =
		std3D_FindClosestFormat(&g_cfPal8, g_std3DTextureFormats, (unsigned int)g_std3DNumTexFormats);
	g_pFmtPal8 = &g_std3DTextureFormats[g_fmtIdxPal8];
	if (g_pFmtPal8->colorInfo.colorMode == STDCOLOR_PAL) {
		g_bPal8Available = 1;
	} else {
		g_bPal8Available = 0;
	}

	std3D_FreePalettes();
	std3D_QueryTextureVidMem(&g_pStd3DCurDevice->totalMemory, &g_pStd3DCurDevice->availableMemory);
	DebugPrintf("Texture Ram  Total: %d bytes  Free: %d bytes.\n", g_pStd3DCurDevice->totalMemory,
				g_pStd3DCurDevice->availableMemory, 0, 0);
	DebugPrintf("Device #%d opened successfully.\n", g_std3DCurDeviceIdx, 0, 0, 0);
	DebugPrintf("std3D opened.\n", 0, 0, 0, 0);
	g_std3DDeviceOpen = 1;
	return 1;

fail:
	std3D_DestroyDevice();
	return 0;
}

// FUNCTION: XWA 0x594B6A
void std3D_DestroyDevice(void) {
	if (g_pStd3DVBuffer) {
		std3D_FreeVBuffer(g_pStd3DVBuffer);
		g_pStd3DVBuffer = NULL;
	}
	if (g_d3dExecuteBuffer) {
		int debugResult;

		g_std3DReleaseRefCount = (int)g_d3dExecuteBuffer->lpVtbl->Release(g_d3dExecuteBuffer);
		if (g_std3DReleaseRefCount) {
			debugResult = DebugPrintf("DX object not released properly, refcount:", g_std3DReleaseRefCount);
		} else {
			debugResult = 0;
		}
		g_d3dExecuteBuffer = NULL;
	}
	if (g_d3dViewport) {
		int debugResult;

		if (g_d3dDevice) {
			g_d3dDevice->lpVtbl->DeleteViewport(g_d3dDevice, g_d3dViewport);
		}
		g_std3DReleaseRefCount = (int)g_d3dViewport->lpVtbl->Release(g_d3dViewport);
		if (g_std3DReleaseRefCount) {
			debugResult = DebugPrintf("DX object not released properly, refcount:", g_std3DReleaseRefCount);
		} else {
			debugResult = 0;
		}
		g_d3dViewport = NULL;
	}
	if (g_d3dDevice) {
		int debugResult;

		g_std3DReleaseRefCount = (int)g_d3dDevice->lpVtbl->Release(g_d3dDevice);
		if (!g_std3DReleaseRefCount) {
			debugResult = DebugPrintf("DX object released too early, refcount is 0");
		} else {
			debugResult = 0;
		}
		g_d3dDevice = NULL;
	}
	if (g_std3DZBufferSurface.surface) {
		if (g_lpRenderSurface) {
			g_lpRenderSurface->lpVtbl->DeleteAttachedSurface(g_lpRenderSurface, 0,
															 g_std3DZBufferSurface.surface);
		} else {
			DebugPrintf("zbuffer not detached because there is no backbuffer");
		}
		while (g_std3DZBufferSurface.surface->lpVtbl->Release(g_std3DZBufferSurface.surface)) {
			/* Release until the attachment ref is gone. */
		}
		g_std3DZBufferSurface.surface = NULL;
	}
}

// FUNCTION: XWA 0x59395A
int std3D_GetPal8Available(void) { return g_bPal8Available; }

#ifndef XWA_MODERN
/* The original compiler emits the 56-byte capability record clear inline. */
#pragma intrinsic(memcpy, memset)
#endif

static inline int16_t Renderer_GetTextureClampSupported(int16_t defaultValue) {
	if (g_useHardware3D && g_pStd3DCurDevice) {
		return g_pStd3DCurDevice->caps.bClampSupported;
	}
	return defaultValue;
}

#ifndef XWA_MODERN
#pragma optimize("g", on)
#endif
// FUNCTION: XWA 0x4531B0
int16_t Renderer_IsTextureClampSupported(void) {
	if (g_useHardware3D && g_pStd3DCurDevice) {
		return g_pStd3DCurDevice->caps.bClampSupported;
	}
	return 1;
}
#ifndef XWA_MODERN
#pragma optimize("g", off)
#endif

static inline void Renderer_BuildD3DRenderStatePresets(void) {
	int filterFlags = g_bilinearEnabled ? 0x180 : 0;

	g_d3dRenderStatePreset0_unused = g_d3dRenderStateDefaultFlags[0] + filterFlags;
	g_d3dRenderStateUntexturedFace = g_d3dRenderStateDefaultFlags[1] + filterFlags;
	g_d3dRenderStateDeferredAlphaMesh = g_d3dRenderStateDefaultFlags[2] + filterFlags;
	g_d3dRenderStateTexturedMesh = g_d3dRenderStateDefaultFlags[3] + filterFlags;
	g_d3dRenderStateMeshPass2 = g_d3dRenderStateDefaultFlags[4] + filterFlags;
	g_d3dRenderStateMultiTextureMesh = g_d3dRenderStateDefaultFlags[5] + filterFlags;
	g_d3dRenderStateGlowQuad = g_d3dRenderStateDefaultFlags[6] + filterFlags;
	g_d3dRenderStatePreset7_unused = g_d3dRenderStateDefaultFlags[7];
}

void Renderer_InitD3DRenderStatePresets(void) {
	/* The modern renderer uses the same presets without opening a Direct3D device. */
	Renderer_BuildD3DRenderStatePresets();
}

#ifndef XWA_MODERN
#pragma optimize("g", on)
#endif
// FUNCTION: XWA 0x441EE0
void Renderer_InitD3DDevice(IDirectDraw* lpDD, IDirectDrawSurface* lpRenderSurface) {
	Std3DDeviceCaps deviceCaps;
	unsigned int deviceIdx;
	int startupResult;
	const int16_t enabled = 1;

	std3D_BuildRenderTargetDesc(width, height, g_surfacePitch);
	std3D_ResetUnusedStartupState(0, 0, 0, 0, 0, 0, 0);
	startupResult = std3D_Startup(lpDD, lpRenderSurface);
	g_useHardware3D = startupResult;
	if (!startupResult) {
		return;
	}
	memset(&deviceCaps, 0, sizeof(deviceCaps));
	deviceCaps.bHardware = enabled;
	deviceCaps.bTexturePerspective = enabled;
	deviceCaps.bHasZBuffer = enabled;
	deviceCaps.colorModelFlags = 2;
	deviceIdx = std3D_SelectBestDevice(&deviceCaps);
	memcpy(&deviceCaps, &g_std3DDevices[deviceIdx].caps, sizeof(deviceCaps));

	if (deviceCaps.bHasZBuffer && deviceCaps.bTexturePerspective && deviceCaps.bHardware) {
		Renderer_BuildD3DRenderStatePresets();

		if (std3D_CreateDevice(deviceIdx, enabled)) {
			if (!std3D_GetZBufferSurface()) {
				DebugPrintf("ERROR! Failed to get HW Zbuffer");
				if (g_d3dInfoActiveCount) {
					while (g_d3dInfoListHead) {
						g_d3dInfoListHead->refCount = enabled;
						D3DInfo_Release(g_d3dInfoListHead);
					}
				}
				std3D_Close(0);
			} else {
				g_hwMipmapFilter = std3D_SetMipmapFilter(g_hwMipmapFilter);
				if (!Renderer_GetTextureClampSupported(enabled) ||
					(g_hitEffectsEnabled = enabled,
					 !g_gameConfig.hitEffects[g_flightPlayerCount > enabled])) {
					g_hitEffectsEnabled = 0;
				}

				if (!g_usePalettizedTextures || !std3D_GetPal8Available()) {
					g_usePalettizedTextures = 0;
				} else {
					g_usePalettizedTextures = enabled;
				}

				g_unusedD3DMaxVertsClamped384 = g_d3dMaxVerts;
				if ((unsigned int)g_d3dMaxVerts >= 384) {
					g_unusedD3DMaxVertsClamped384 = 384;
				}
				g_clipIdxA = g_clipIdxAStorage;
				g_clipIdxB = g_clipIdxBStorage;

				if (g_hwMipmapFilter) {
					DebugPrintf("You are using hardware mipmapping.");
				} else {
					DebugPrintf("You are not using hardware mipmapping.");
				}
				if (g_usePalettizedTextures) {
					DebugPrintf("You are using 8-bit palettized textures for textures with no alpha.");
				} else {
					DebugPrintf("You are using only 16-bit hicolor textures.");
				}
				return;
			}
		}
	} else {
		DebugPrintf("Essential Hardware Feature NOT Supported: Z:%d Tex:%d HW:%d\n", deviceCaps.bHasZBuffer,
					deviceCaps.bTexturePerspective, deviceCaps.bHardware);
	}

	std3D_Shutdown();
	g_useHardware3D = 0;
}
#ifndef XWA_MODERN
#pragma optimize("g", off)
#endif
