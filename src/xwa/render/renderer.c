#include "xwa/render/renderer.h"

#include "xwa/render/effects.h"

#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_texture.h"
#include "xwa/assets/model_type.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_light.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/player/player.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/random.h"

#include <math.h>
#include <string.h>

// GLOBAL: XWA 0x77330C
int g_useHardware3D = 1;
// GLOBAL: XWA 0x7CA220
uint8_t g_palettePackedMode;
// GLOBAL: XWA 0x6002C0
uint8_t g_flightGraphicsDetailPreset;
// GLOBAL: XWA 0x60036C
int g_debrisDensityLevel;
// GLOBAL: XWA 0x5FFD8C
uint8_t g_usePalettizedTextures;
// GLOBAL: XWA 0x773310
int g_frontendD3DInitialized;
// GLOBAL: XWA 0x77333C
int g_loadingModel;
// GLOBAL: XWA 0x80811C
int g_projOffsetY;
// GLOBAL: XWA 0x7B33C0
float g_projOffsetYf;
// GLOBAL: XWA 0x600288
float g_lodDistanceScale;
// GLOBAL: XWA 0x60028C
float g_mipLodScale;
// GLOBAL: XWA 0x5AA0A8
int g_sw3dMipmapEnabled = 1;
// GLOBAL: XWA 0x78283C
uint8_t g_flightSurfaceAlreadyLocked;
// GLOBAL: XWA 0x600290
int g_localLightsLevel;
// GLOBAL: XWA 0x600294
int g_specularEnabled;
// GLOBAL: XWA 0x60029C
int g_keepFullResTextures;
// GLOBAL: XWA 0x6002A0
int g_explosionResLevel;
// GLOBAL: XWA 0x6002A4
uint8_t g_flightColorEscapeBypassChar = 0xfb;
// GLOBAL: XWA 0x600298
int g_dirLightingEnabled;
// GLOBAL: XWA 0x5FFD98
uint8_t g_hitEffectsEnabled;
// GLOBAL: XWA 0x5FFD90
uint8_t g_particleEffectsEnabled;
// GLOBAL: XWA 0x5FFD94
uint8_t g_trailsEnabled;
// GLOBAL: XWA 0x7827CC
int g_flightRenderModeId;
// GLOBAL: XWA 0x8052B8
uint16_t g_flightVpWidth;
// GLOBAL: XWA 0x7B33BC
uint16_t g_flightVpHeight;
// GLOBAL: XWA 0x7FBB72
uint16_t g_flightVpMaxX;
// GLOBAL: XWA 0x7FFB60
uint16_t g_flightVpMaxY;
// GLOBAL: XWA 0x910DE8
uint16_t g_flightVpCenterX;
// GLOBAL: XWA 0x7B6FEE
uint16_t g_flightVpCenterY;
// GLOBAL: XWA 0x5AA080
const float flt_5AA080 = 0.2f;
// GLOBAL: XWA 0x5AA088
const float flt_5AA088 = 0.000030518509f;
// GLOBAL: XWA 0x5A9AAC
const float g_flightViewportProjectionFactor = 0.2f;
// GLOBAL: XWA 0x5AA090
const double g_degreesToQ16Scale = 182.04444444444445;
// GLOBAL: XWA 0x8C1600
float g_flightVpCenterXf;
// GLOBAL: XWA 0x80ACF8
float g_flightVpCenterYf;
// GLOBAL: XWA 0x91AB60
int g_flightVpProjScaleX;
// GLOBAL: XWA 0x7D5244
int g_flightVpX;
// GLOBAL: XWA 0x7CA354
int g_flightVpY;
// GLOBAL: XWA 0x8C28D4
int g_flightVpBaseOffset;
// GLOBAL: XWA 0x693774
FlightViewportSaveState g_savedFlightViewport;
// GLOBAL: XWA 0x5B630C
int16_t g_maskBufferOffset;
// GLOBAL: XWA 0x5FFDC0
void* g_surfacePixels;
// GLOBAL: XWA 0x80DC58
int g_surfacePitch;
// GLOBAL: XWA 0x6002B0
int g_surfaceWidth;
// GLOBAL: XWA 0x6002B4
int g_surfaceHeight;
// GLOBAL: XWA 0x7CA3A8
unsigned int g_screenWidth;
// GLOBAL: XWA 0x7D4B6C
unsigned int g_screenHeight;
// GLOBAL: XWA 0x91AD34
int width;
// GLOBAL: XWA 0x91AD3C
int height;
// GLOBAL: XWA 0x91AB6C
int g_projScaleInt;
// GLOBAL: XWA 0x7B33CC
int g_projScaleHalfInt;
// GLOBAL: XWA 0x80DC54
uint8_t perspShift;
// GLOBAL: XWA 0x8C1CE8
uint16_t g_projAspectY;
// GLOBAL: XWA 0x8B94BC
float g_projScaleDiv512;
// GLOBAL: XWA 0x8B94CC
float g_projScale;
// GLOBAL: XWA 0x7CAB3A
int16_t g_sceneBillboardQueueCount;
// GLOBAL: XWA 0x60E520
SceneBillboardQueueEntry g_sceneBillboardQueue[32];
// GLOBAL: XWA 0x693770
int16_t g_lensFlareQueueCount;
// GLOBAL: XWA 0x6935A0
LensFlareSource g_lensFlareQueue[4];
// GLOBAL: XWA 0x7B33B8
int g_renderFlags;
// GLOBAL: XWA 0x7D4F84
int viewX;
// GLOBAL: XWA 0x7D4F88
int viewY;
// GLOBAL: XWA 0x7D4F8C
int viewZ;
// GLOBAL: XWA 0x8D42B8
int g_camRelWorldX;
// GLOBAL: XWA 0x8D42BC
int g_camRelWorldY;
// GLOBAL: XWA 0x8D42B4
int g_camRelWorldZ;
// GLOBAL: XWA 0x7D4BD0
int g_curModelMaxExtent;
// GLOBAL: XWA 0x7827A8
int g_modelPreviewModelType;
// GLOBAL: XWA 0x7B33C4
ObjectRecord* g_objectTable;
// GLOBAL: XWA 0x91ACC0
Vec3f g_modelPreviewViewDelta;
// GLOBAL: XWA 0x91AE80
int g_drawingOwnCraft;
// GLOBAL: XWA 0x91AD00
Vec3f g_modelPreviewNegViewDelta;
// GLOBAL: XWA 0x91AD0C
Matrix3x3 g_modelPreviewObjectViewMatrix;
// GLOBAL: XWA 0x91ACCC
Matrix3x3 mat;

// FUNCTION: XWA 0x494770
int SetFlightViewport(unsigned int viewportWidth, unsigned int viewportHeight, int unused,
					  unsigned int baseOffset) {
	unsigned int vpWidth;
	unsigned int vpHeight;
	unsigned int vpBaseOffset;

	(void)unused;

	if (g_flightRenderModeId == 160) {
		vpWidth = viewportWidth >> 1;
		vpHeight = viewportHeight >> 1;
		vpBaseOffset = baseOffset + 120u * (unsigned int)g_surfacePitch + 160u;
	} else {
		vpWidth = viewportWidth;
		vpHeight = viewportHeight;
		vpBaseOffset = baseOffset;
	}

	g_flightVpHeight = (int)vpHeight;
	g_flightVpMaxX = (int)vpWidth - 1;
	g_flightVpMaxY = (int)vpHeight - 1;
	g_flightVpWidth = (int)vpWidth;
	g_flightVpCenterX = (int)(vpWidth >> 1);
	g_flightVpCenterY = (int)(vpHeight >> 1);
	g_flightVpBaseOffset = (int)vpBaseOffset;
	g_flightVpY = (int)(vpBaseOffset / (unsigned int)g_surfacePitch);
	g_flightVpX =
		(int)((vpBaseOffset % (unsigned int)g_surfacePitch) / (unsigned int)g_flight16bppBytesPerPixel);
	g_flightVpCenterXf = (float)(uint16_t)g_flightVpCenterX;
	g_flightVpCenterYf = (float)(uint16_t)g_flightVpCenterY;
	g_flightVpProjScaleX = (int)(g_flightVpCenterXf * 0.2f);
	return g_flightVpProjScaleX;
}

// FUNCTION: XWA 0x494850
unsigned int PushFlightViewport(unsigned int width, unsigned int height, int unused,
								unsigned int baseOffset) {
	unsigned int remainder;

	(void)unused;

	g_savedFlightViewport.width = (uint16_t)g_flightVpWidth;
	g_savedFlightViewport.height = (uint16_t)g_flightVpHeight;
	g_savedFlightViewport.baseOffset = (uint16_t)g_flightVpBaseOffset;
	g_savedFlightViewport.viewportY = (uint16_t)g_flightVpY;
	g_savedFlightViewport.viewportX = (uint16_t)g_flightVpX;
	g_savedFlightViewport.camMatR0_X = g_camMatR0_X;
	g_savedFlightViewport.camMatR1_X = g_camMatR1_X;
	g_savedFlightViewport.camMatR2_X = g_camMatR2_X;
	g_savedFlightViewport.camMatR0_Y = g_camMatR0_Y;
	g_savedFlightViewport.camMatR1_Y = g_camMatR1_Y;
	g_savedFlightViewport.camMatR2_Y = g_camMatR2_Y;
	g_savedFlightViewport.camMatR0_Z = g_camMatR0_Z;
	g_savedFlightViewport.camMatR1_Z = g_camMatR1_Z;
	g_savedFlightViewport.camMatR2_Z = g_camMatR2_Z;
#ifdef XWA_MODERN
	FVIEW_SaveRenderCameraStateForViewport();
#endif

	g_flightVpWidth = width;
	g_flightVpMaxX = (int)width - 1;
	g_flightVpCenterX = g_flightVpWidth >> 1;
	g_flightVpProjScaleX =
		(int)((g_flightVpCenterXf = (float)g_flightVpCenterX) * g_flightViewportProjectionFactor);
	g_flightVpHeight = height;
	g_flightVpMaxY = (int)height - 1;
	g_flightVpCenterY = g_flightVpHeight >> 1;
	g_flightVpCenterYf = (float)g_flightVpCenterY;
	g_flightVpBaseOffset = (int)baseOffset;
	g_maskBufferOffset = (int16_t)0xe000u;
	g_flightVpY = (int)(baseOffset / (unsigned int)g_surfacePitch);
	remainder = baseOffset % (unsigned int)g_surfacePitch;
	g_flightVpX = (int)(remainder / (unsigned int)g_flight16bppBytesPerPixel);
	return (unsigned int)g_flightVpX;
}

// FUNCTION: XWA 0x4949B0
int PopFlightViewport(void) {
	g_camMatR0_X = g_savedFlightViewport.camMatR0_X;
	g_camMatR1_X = g_savedFlightViewport.camMatR1_X;
	g_camMatR2_X = g_savedFlightViewport.camMatR2_X;
	g_camMatR0_Y = g_savedFlightViewport.camMatR0_Y;
	g_camMatR1_Y = g_savedFlightViewport.camMatR1_Y;
	g_camMatR2_Y = g_savedFlightViewport.camMatR2_Y;
	g_camMatR0_Z = g_savedFlightViewport.camMatR0_Z;
	g_camMatR1_Z = g_savedFlightViewport.camMatR1_Z;
	g_camMatR2_Z = g_savedFlightViewport.camMatR2_Z;
#ifdef XWA_MODERN
	FVIEW_RestoreRenderCameraStateForViewport();
#endif

	g_flightVpWidth = g_savedFlightViewport.width;
	g_flightVpMaxX = g_savedFlightViewport.width - 1;
	g_flightVpCenterX = g_savedFlightViewport.width >> 1;
	g_flightVpProjScaleX =
		(int)((g_flightVpCenterXf = (float)(uint16_t)g_flightVpCenterX) * g_flightViewportProjectionFactor);

	g_flightVpHeight = g_savedFlightViewport.height;
	g_maskBufferOffset = (int16_t)0xc000u;
	g_flightVpMaxY = g_savedFlightViewport.height - 1;
	g_flightVpCenterY = g_savedFlightViewport.height >> 1;
	g_flightVpCenterYf = (float)(uint16_t)g_flightVpCenterY;

	g_flightVpBaseOffset = g_savedFlightViewport.baseOffset;
	g_flightVpY = g_savedFlightViewport.viewportY;
	g_flightVpX = g_savedFlightViewport.viewportX;
	return g_flightVpBaseOffset;
}
// GLOBAL: XWA 0x7B4BEC
float g_viewMtx00;
// GLOBAL: XWA 0x7B6FF8
float g_viewMtx01;
// GLOBAL: XWA 0x7B33DC
float g_viewMtx02;
// GLOBAL: XWA 0x7B4BE8
float g_viewMtx10;
// GLOBAL: XWA 0x7B6FF0
float g_viewMtx11;
// GLOBAL: XWA 0x7B33D8
float g_viewMtx12;
// GLOBAL: XWA 0x7B4BF4
float g_viewMtx20;
// GLOBAL: XWA 0x7B33D4
float g_viewMtx21;
// GLOBAL: XWA 0x7B4BE4
float g_viewMtx22;
// GLOBAL: XWA 0x8D93E4
int g_camMatR0_X;
// GLOBAL: XWA 0x8D93FC
int g_camMatR0_Y;
// GLOBAL: XWA 0x8D93C4
int g_camMatR0_Z;
// GLOBAL: XWA 0x8D93E0
int g_camMatR1_X;
// GLOBAL: XWA 0x8D93F8
int g_camMatR1_Y;
// GLOBAL: XWA 0x8D93C0
int g_camMatR1_Z;
// GLOBAL: XWA 0x8D93F0
int g_camMatR2_X;
// GLOBAL: XWA 0x8D6BB0
int g_camMatR2_Y;
// GLOBAL: XWA 0x8D93CC
int g_camMatR2_Z;
// GLOBAL: XWA 0x8D93D8
int g_curMatR0_X;
// GLOBAL: XWA 0x8D93C8
int g_curMatR0_Y;
// GLOBAL: XWA 0x8D93D0
int g_curMatR0_Z;
// GLOBAL: XWA 0x8D9400
int g_curMatR1_X;
// GLOBAL: XWA 0x8D93E8
int g_curMatR1_Y;
// GLOBAL: XWA 0x8D93F4
int g_curMatR1_Z;
// GLOBAL: XWA 0x910934
int g_curMatR2_X;
// GLOBAL: XWA 0x910938
int g_curMatR2_Y;
// GLOBAL: XWA 0x910944
int g_curMatR2_Z;
// GLOBAL: XWA 0x7D4BD4
int g_fviewMoveX_Q15;
// GLOBAL: XWA 0x7D4BB8
int g_fviewMoveY_Q15;
// GLOBAL: XWA 0x7D4BBC
int g_fviewMoveZ_Q15;
// GLOBAL: XWA 0x7FFB70
int g_fviewFwdX_Q15;
// GLOBAL: XWA 0x7FFB64
int g_fviewFwdY_Q15;
// GLOBAL: XWA 0x7FFA48
int g_fviewFwdZ_Q15;
// GLOBAL: XWA 0x7D4BB4
int g_fviewSideX_Q15;
// GLOBAL: XWA 0x7D4BB0
int g_fviewSideY_Q15;
// GLOBAL: XWA 0x7D4BC0
int g_fviewSideZ_Q15;
// GLOBAL: XWA 0x7D4C60
int g_fviewUpX_Q15;
// GLOBAL: XWA 0x7D4C5C
int g_fviewUpY_Q15;
// GLOBAL: XWA 0x7D4F80
int g_fviewUpZ_Q15;
// GLOBAL: XWA 0x7D4C00
PlayerViewState g_filmOverlayViewState;
// GLOBAL: XWA 0x8D4260
PlayerViewState g_savedPlayerViewStateForPlaybackCamera;
// GLOBAL: XWA 0x7C9DA8
float g_objViewMatF_R0_X;
// GLOBAL: XWA 0x7C9DAC
float g_objViewMatF_R0_Y;
// GLOBAL: XWA 0x7C9DB4
float g_objViewMatF_R0_Z;
// GLOBAL: XWA 0x7CA15C
float g_objViewMatF_R1_X;
// GLOBAL: XWA 0x7CA160
float g_objViewMatF_R1_Y;
// GLOBAL: XWA 0x7CA164
float g_objViewMatF_R1_Z;
// GLOBAL: XWA 0x7CA168
float g_objViewMatF_R2_X;
// GLOBAL: XWA 0x7CA170
float g_objViewMatF_R2_Y;
// GLOBAL: XWA 0x7CA178
float g_objViewMatF_R2_Z;
// GLOBAL: XWA 0x808140
int g_objViewMat_R0_X;
// GLOBAL: XWA 0x808150
int g_objViewMat_R0_Y;
// GLOBAL: XWA 0x80814C
int g_objViewMat_R0_Z;
// GLOBAL: XWA 0x808144
int g_objViewMat_R1_X;
// GLOBAL: XWA 0x808130
int g_objViewMat_R1_Y;
// GLOBAL: XWA 0x80812C
int g_objViewMat_R1_Z;
// GLOBAL: XWA 0x808128
int g_objViewMat_R2_X;
// GLOBAL: XWA 0x80813C
int g_objViewMat_R2_Y;
// GLOBAL: XWA 0x808138
int g_objViewMat_R2_Z;
// GLOBAL: XWA 0x9C6740
int g_projectedFaceTraceCount;
// GLOBAL: XWA 0x9B6D20
int g_projectedFaceTraceX[8000];
// GLOBAL: XWA 0x9BEA20
int g_projectedFaceTraceY[8000];
// GLOBAL: XWA 0x7B13A8
uint16_t g_std3DPaletteScratch16[256];
// GLOBAL: XWA 0x7B1188
uint16_t g_texConvBuf1555[256];
// GLOBAL: XWA 0x7B0F28
uint16_t g_texConvBuf4444[256];
// GLOBAL: XWA 0x693594
int g_currentRenderMode;
// GLOBAL: XWA 0x5B46B8
int g_std3DStartScenePending = 1;
// GLOBAL: XWA 0x686ABC
int g_d3dIndexCount;
// GLOBAL: XWA 0x6628E0
int g_d3dVertexCount;
// GLOBAL: XWA 0x686B20
int g_d3dVertexAlphaStateResetSlot;
// GLOBAL: XWA 0x5B4690
int g_capVertexAlpha;
// GLOBAL: XWA 0x686ACC
float g_flightVpOriginX;
// GLOBAL: XWA 0x64D1AC
float g_flightVpOriginY;
// GLOBAL: XWA 0x662848
int g_maxBatchVerts;
// GLOBAL: XWA 0x63D100
int g_maxBatchTris;
// GLOBAL: XWA 0x64D1A8
D3DTLVERTEX* g_flightVertexBuffer;
// GLOBAL: XWA 0x66283C
Std3DRenderTri* g_triBuffer;
// GLOBAL: XWA 0x9B6C80
uint8_t* g_sceneSpanData;
// GLOBAL: XWA 0x9B6C84
uint16_t g_sceneSpanDataHandle;
// GLOBAL: XWA 0x9B6C86
int g_sceneSpanDataMax;
// GLOBAL: XWA 0x9B6C92
uint8_t* g_sceneSpanPtrList;
// GLOBAL: XWA 0x9B6C96
uint16_t g_sceneSpanPtrListHandle;
// GLOBAL: XWA 0x8D9624
int g_missionRegionCount;
// GLOBAL: XWA 0x808300
WorldRectRecord g_backdropRecordsByRegion[XWA_BACKDROP_REGION_COUNT][XWA_BACKDROP_RECORDS_PER_REGION];
// GLOBAL: XWA 0x80ACE0
int g_backdropCountByRegion[XWA_BACKDROP_REGION_COUNT];
// GLOBAL: XWA 0x9B6C8A
uint8_t* g_pSceneSpanDataCur;
// GLOBAL: XWA 0x9B6C8E
uint8_t* g_pSceneSpanDataEnd;
// GLOBAL: XWA 0x9B6C98
int g_sceneSpanPtrMax;
// GLOBAL: XWA 0x9B6C9C
int g_sceneSpanPtrAvail;
// GLOBAL: XWA 0x9B6CA6
int g_phongSlotIndex;
// GLOBAL: XWA 0x9B6CB0
int g_visFaceCount;
// GLOBAL: XWA 0x9B6CB4
int g_visFaceDrawStartIndex;
// GLOBAL: XWA 0x9B6D02
int g_meshQueueIndex;
// GLOBAL: XWA 0x9B6C6C
int g_phongSlotStride;
// GLOBAL: XWA 0x9B6CA0
uint8_t* g_scenePhongData;
// GLOBAL: XWA 0x9B6CA4
uint16_t g_scenePhongDataHandle;
// GLOBAL: XWA 0x5A9A4C
int g_sw3dLightSampleBlockSize = 16;
// GLOBAL: XWA 0x5A9A50
int g_sw3dLightSampleBlockShift = 4;
// GLOBAL: XWA 0x5A9A54
int g_sw3dLightSampleBlockMask = 15;
// GLOBAL: XWA 0x5A9A68
float g_renderUnitScale = 1.0f;
// GLOBAL: XWA 0x5A99E8
const float g_sw3dZeroFloat = 0.0f;
// GLOBAL: XWA 0x5A99F0
const float g_sw3dUnitFloat = 1.0f;
// GLOBAL: XWA 0x5A9A00
const float g_sw3dNegUnitFloat = -1.0f;
// GLOBAL: XWA 0x5A9A24
const float g_sw3dLaserScale = 100.0f;
// GLOBAL: XWA 0x5A9A28
const float g_modelNodeAxisScale = 0.000030517578f;
// GLOBAL: XWA 0x5A9A2C
const float g_modelNodeHalfAngleScale = 0.5f;
// GLOBAL: XWA 0x5B646C
const char g_renderSoftwareRenderingMessage[] = "RIGGED FOR SOFTWARE RENDERING";
// GLOBAL: XWA 0x5B648C
const char g_renderCockpitMaskErrorMessage[] = "Mask Data error in InitializeScene()\n";
// GLOBAL: XWA 0x5B5E40
float g_sw3dSpanLengthReciprocal[70] = {
	1.0f,         1.0f,         1.0f / 2.0f,  1.0f / 3.0f,  1.0f / 4.0f,  1.0f / 5.0f,  1.0f / 6.0f,
	1.0f / 7.0f,  1.0f / 8.0f,  1.0f / 9.0f,  1.0f / 10.0f, 1.0f / 11.0f, 1.0f / 12.0f, 1.0f / 13.0f,
	1.0f / 14.0f, 1.0f / 15.0f, 1.0f / 16.0f, 1.0f / 17.0f, 1.0f / 18.0f, 1.0f / 19.0f, 1.0f / 20.0f,
	1.0f / 21.0f, 1.0f / 22.0f, 1.0f / 23.0f, 1.0f / 24.0f, 1.0f / 25.0f, 1.0f / 26.0f, 1.0f / 27.0f,
	1.0f / 28.0f, 1.0f / 29.0f, 1.0f / 30.0f, 1.0f / 31.0f, 1.0f / 32.0f, 1.0f / 33.0f, 1.0f / 34.0f,
	1.0f / 35.0f, 1.0f / 36.0f, 1.0f / 37.0f, 1.0f / 38.0f, 1.0f / 39.0f, 1.0f / 40.0f, 1.0f / 41.0f,
	1.0f / 42.0f, 1.0f / 43.0f, 1.0f / 44.0f, 1.0f / 45.0f, 1.0f / 46.0f, 1.0f / 47.0f, 1.0f / 48.0f,
	1.0f / 49.0f, 1.0f / 50.0f, 1.0f / 51.0f, 1.0f / 52.0f, 1.0f / 53.0f, 1.0f / 54.0f, 1.0f / 55.0f,
	1.0f / 56.0f, 1.0f / 57.0f, 1.0f / 58.0f, 1.0f / 59.0f, 1.0f / 60.0f, 1.0f / 61.0f, 1.0f / 62.0f,
	1.0f / 63.0f, 1.0f / 64.0f, 1.0f / 65.0f, 1.0f / 66.0f, 1.0f / 67.0f, 1.0f / 68.0f, 1.0f / 69.0f,
};
// GLOBAL: XWA 0x5B6208
int g_sw3dTextureShiftBySizeDiv16[60] = {
	3, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
};
// GLOBAL: XWA 0x692810
SceneFace* g_sw3dCurrentFace;
// GLOBAL: XWA 0x692858
int g_sw3dCurrentScanY;
// GLOBAL: XWA 0x69281C
int g_sw3dScanlineByteOffset;
// GLOBAL: XWA 0x692824
float g_sw3dLightSampleSubrowLerpT;
// GLOBAL: XWA 0x692834
float g_sw3dLightSampleRowsToNextBlockFloat;
// GLOBAL: XWA 0x692838
float g_sw3dLightSampleSubrowFloat;
// GLOBAL: XWA 0x692864
int g_sw3dCurrentLightSampleCacheStamp;
// GLOBAL: XWA 0x6934EC
int g_sw3dLightSampleCacheSceneStampBase;
// GLOBAL: XWA 0x692820
uint16_t g_sw3dFpuControlWordScratch;
// GLOBAL: XWA 0x6934F0
SceneFace g_sw3dCockpitMaskSentinelFace;
// GLOBAL: XWA 0x69353C
float g_sw3dCockpitMaskSentinelMaxX;
// GLOBAL: XWA 0x693540
float g_sw3dCockpitMaskSentinelMaxY;
// GLOBAL: XWA 0x69351C
float g_sw3dCockpitMaskSentinelStartX;
// GLOBAL: XWA 0x693520
float g_sw3dCockpitMaskSentinelEndX;
// GLOBAL: XWA 0x693524
float g_sw3dCockpitMaskSentinelDepthZ;
// GLOBAL: XWA 0x693598
uint32_t g_renderSceneVirtualProtectOldProtect;
// GLOBAL: XWA 0x693568
SceneMesh* g_sw3dSpanSceneMesh;
// GLOBAL: XWA 0x69356C
float g_sw3dSpanTextureWidthFloat;
// GLOBAL: XWA 0x693570
float g_sw3dSpanTextureHeightFloat;
// GLOBAL: XWA 0x693574
int g_sw3dSpanTextureWidthShift;
// GLOBAL: XWA 0x693578
int g_sw3dSpanTextureHeightShift;
// GLOBAL: XWA 0x69357C
uint8_t* g_sw3dSpanShadeTable;
// GLOBAL: XWA 0x693580
uint8_t* g_sw3dSpanTexels;
// GLOBAL: XWA 0x693584
int g_sw3dSpanTexelMask;
// GLOBAL: XWA 0x692814
int g_sw3dSpanShadeDitherAccum;
// GLOBAL: XWA 0x5B6200
int g_sw3dShadeDitherInitialByScanlineParity[2] = { 0, 128 };
// GLOBAL: XWA 0x692860
int g_sw3dSpanStartX;
// GLOBAL: XWA 0x69285C
int g_sw3dSpanLength;
// GLOBAL: XWA 0x693590
Sw3dFloatInt g_sw3dSpanStepUQ8;
// GLOBAL: XWA 0x692852
Sw3dFloatInt g_sw3dSpanStepVQ8;
// GLOBAL: XWA 0x69358C
Sw3dFloatInt g_sw3dSpanShadeQ8;
// GLOBAL: XWA 0x69284C
Sw3dFloatInt g_sw3dSpanShadeStepQ8;
// GLOBAL: XWA 0x692830
Sw3dFloatInt g_sw3dSpanUQ8;
// GLOBAL: XWA 0x69283E
Sw3dFloatInt g_sw3dSpanVQ8;
// GLOBAL: XWA 0x5B6338
const Sw3dFloatInt g_sw3dTexCoordBiasByShift[12] = {
	{ 49152.0f }, { 24576.0f }, { 12288.0f }, { 6144.0f }, { 3072.0f }, { 1536.0f },
	{ 768.0f },   { 384.0f },   { 192.0f },   { 96.0f },   { 48.0f },   { 24.0f },
};
// GLOBAL: XWA 0x5B6318
const Sw3dFloatInt g_sw3dFloatToIntRoundBias = { 12582912.0f };
// GLOBAL: XWA 0x5B6310
const float g_sw3dOneFloat = 1.0f;
// GLOBAL: XWA 0x5A9A58
const float g_sw3dLightSampleInvBlockSize = 1.0f / 16.0f;
// GLOBAL: XWA 0x5A9A5C
const float g_sw3dLightSampleBlockSizeFloat = 16.0f;
// GLOBAL: XWA 0x5B6314
const float g_sw3dLightIntensityToShadeScale = 15.0f;
// GLOBAL: XWA 0x5A9A04
const float g_meshRotationToRadians = 0.024543673f;
// GLOBAL: XWA 0x5A9A10
const float g_cockpitPanPositionScale = 0.0625f;
// GLOBAL: XWA 0x5A9A18
const double g_q16AngleToRadians = 0.00009587379924285257;
// GLOBAL: XWA 0x5A9A20
const float g_turretRotationToRadians = 0.000095873722f;
// GLOBAL: XWA 0x9C6730
Vec3f g_meshEyePos;
// GLOBAL: XWA 0x5B5F58
float g_fixedCullDir[4] = { 0.0f, 0.0f, -1.0f, 0.0f };
// GLOBAL: XWA 0x68EAEC
uint8_t g_bBackdropMeshMode;
// GLOBAL: XWA 0x68EAF4
int g_curLayerId;
// GLOBAL: XWA 0x91AE88
int g_faceIdCounter;
// GLOBAL: XWA 0x68CA60
void* g_curTextureDesc;
// GLOBAL: XWA 0x68CA30
intptr_t g_curMeshFlags;
// GLOBAL: XWA 0x68CA70
intptr_t g_curVertNormals;
// GLOBAL: XWA 0x68CA68
uint16_t g_curTextureId;
// GLOBAL: XWA 0x770ED0
uint8_t g_bindMeshTextures;
// GLOBAL: XWA 0x9106C0
ModelTextureOverrideSlot g_modelTextureOverrideSlots[32];
// GLOBAL: XWA 0x782824
uint16_t g_modelTextureOverrideNextSlot;
// GLOBAL: XWA 0x7827C4
int g_forcedLodLevel;
// GLOBAL: XWA 0x91AE7C
int g_cockpitViewActive;
// GLOBAL: XWA 0x9C6744
float g_curRotAngle;
// GLOBAL: XWA 0x9C6720
Vec3f* g_curRotScale;
// GLOBAL: XWA 0x9B6D10
Vec3f* g_unusedCockpitRotScaleType21Data;
// GLOBAL: XWA 0x9C673C
Vec3f* g_unusedCockpitRotScaleType22Data;
// GLOBAL: XWA 0x68EAE8
int g_curMeshType;
// GLOBAL: XWA 0x7827C0
int g_nodeSwitchIndex;
// GLOBAL: XWA 0x68C978
intptr_t g_modelNodeWalkUnusedScratch0;
// GLOBAL: XWA 0x68CA5C
intptr_t g_modelNodeWalkUnusedScratch1;
// GLOBAL: XWA 0x68CA64
intptr_t g_modelNodeWalkUnusedScratch2;
// GLOBAL: XWA 0x68CA6C
int g_curVertexCount;
// GLOBAL: XWA 0x5B6028
int g_bwingBridgeMeshIndexCache = -1;
// GLOBAL: XWA 0x7CA17C
int g_approxDist;
// GLOBAL: XWA 0x8C1CD8
int regionIdx;
// GLOBAL: XWA 0x910DFC
CraftData* g_curCraft;
// GLOBAL: XWA 0x80DB68
uint8_t g_filmPlaybackMode;
// GLOBAL: XWA 0x8C163E
uint8_t g_filmOverlayActive;
// GLOBAL: XWA 0x7B1D00
uint8_t g_flightConfPowerVr;
// GLOBAL: XWA 0x5FFD88
int g_bilinearEnabled = 1;
// GLOBAL: XWA 0x6003E4
int g_flightSwRotSpriteSpanRunsEnabled = 1;
// GLOBAL: XWA 0x7CA350
int g_renderObjectListCount;
/* Shared flight-HUD geometry/layout/font scale selected by resolution mode. */
// GLOBAL: XWA 0x6002B8
float g_flightHudScaleFactor = 1.0f;
// GLOBAL: XWA 0x631CD8
float g_deathStarTunnelBillboardScale;
// GLOBAL: XWA 0x686B00
int g_drawSceneEffects;
// GLOBAL: XWA 0x9EA900
int g_frameBytesPurged;
// GLOBAL: XWA 0x9EA96C
int g_frameTriCount;
// GLOBAL: XWA 0x9EA980
int g_frameStateChanges;
// GLOBAL: XWA 0x9EA984
int g_frameTexSwitches;
// GLOBAL: XWA 0x9EA990
int g_frameBytesCached;
// GLOBAL: XWA 0x9EAA00
int g_frameVertCount;
// GLOBAL: XWA 0x7B1D28
int g_d3dBufVertCount;
// GLOBAL: XWA 0x7B1D2C
int g_std3DExecBufTriCount;
// GLOBAL: XWA 0x8082F4
FlightInitLineBufferFn g_flightInitLineBufferFn;
// GLOBAL: XWA 0x7D4BA0
FlightDebugPrintFn g_flightDebugPrintFn;
// GLOBAL: XWA 0x7D4B7C
FlightResetPaletteFn g_flightResetPaletteFn;
// GLOBAL: XWA 0x80AD24
FlightSetPaletteRangeFn g_flightSetPaletteRangeFn;
// GLOBAL: XWA 0x910E04
FlightGetPaletteFn g_flightGetPaletteFn;
// GLOBAL: XWA 0x8D6BA8
FlightSetPaletteFn g_flightSetPaletteFn;
// GLOBAL: XWA 0x7D4B90
FlightComputePixelOffsetFn g_flightComputePixelOffsetFn;
// GLOBAL: XWA 0x80B608
FlightBlitSpriteFn g_flightBlitSpriteFn;
// GLOBAL: XWA 0x7FFD7C
FlightBlitSpriteFadedFn g_flightBlitSpriteFadedFn;
// GLOBAL: XWA 0x7C9DB0
FlightDrawCharFn g_flightDrawCharFn;
// GLOBAL: XWA 0x7FFD70
FlightFillClipRectFn g_flightFillClipRectFn;
// GLOBAL: XWA 0x8C1CBC
FlightFillRectClippedFn g_flightFillRectClippedFn;
// GLOBAL: XWA 0x80DB6C
FlightSaveScreenRectFn g_flightSaveScreenRectFn;
// GLOBAL: XWA 0x9109C0
FlightRestoreScreenRectFn g_flightRestoreScreenRectFn;
// GLOBAL: XWA 0x7B6FFC
FlightDrawPointArrayFn g_flightDrawPointArrayFn;
// GLOBAL: XWA 0x7FFB6C
FlightDrawPointArrayMaskedFn g_flightDrawPointArrayMaskedFn;
// GLOBAL: XWA 0x8B94B4
FlightDrawPixelFn g_flightDrawPixelFn;
// GLOBAL: XWA 0x8BF37C
FlightDrawRadarTargetMarkerFn g_flightDrawRadarTargetMarkerFn;
// GLOBAL: XWA 0x7B33C8
FlightRestoreRadarTargetMarkerFn g_flightRestoreRadarTargetMarkerFn;
// GLOBAL: XWA 0x7F9084
FlightDrawLineFn g_flightDrawLineFn;
// GLOBAL: XWA 0x608894
int g_std3DOpened = 1;
// GLOBAL: XWA 0x608898
intptr_t g_d3dCurTexture = 1;
// GLOBAL: XWA 0x7B1CF8
int g_d3dMaxVerts;
// GLOBAL: XWA 0xB0D0C0
Std3DViewportRect g_std3DQuadRect;
// GLOBAL: XWA 0xB0D040
D3DTLVERTEX g_std3DQuadVerts[4];
Std3DRenderTri g_std3DQuadTris[2];
// GLOBAL: XWA 0x7B1CD8
Std3DRenderStateFlags g_d3dStateFlags;
// GLOBAL: XWA 0x608C68
int g_d3dTexFilterPoint = 1;
// GLOBAL: XWA 0x608C6C
int g_d3dTexFilterLinear = 2;
// GLOBAL: XWA 0x7B1D04
int g_texCacheCount;
// GLOBAL: XWA 0x7B1D08
Std3DTexCacheNode* g_pTexCacheHead;
// GLOBAL: XWA 0x7B1D0C
Std3DTexCacheNode* g_pTexCacheTail;
// GLOBAL: XWA 0x686B24
D3DInfoNode* g_d3dInfoFreeListHead;
// GLOBAL: XWA 0x686B28
D3DInfoNode* g_d3dInfoListHead;
// GLOBAL: XWA 0x686B2C
int g_d3dInfoActiveCount;
// GLOBAL: XWA 0x662968
D3DInfoNode g_d3dInfoPool[XWA_D3DINFO_POOL_COUNT];
#ifdef XWA_MODERN
uint32_t g_nextD3DInfoBridgeRefId = 1;
#endif
// GLOBAL: XWA 0x9EA970
float g_totalTexSwitches;
// GLOBAL: XWA 0x9EA974
float g_totalTris;
// GLOBAL: XWA 0x9EA97C
float g_totalVerts;
// GLOBAL: XWA 0x9EA988
float g_totalBytesPurged;
// GLOBAL: XWA 0x9EA998
float g_totalFrames;
// GLOBAL: XWA 0x9EA99C
float g_totalBytesCached;
// GLOBAL: XWA 0x9EA9FC
float g_totalStateChanges;
// GLOBAL: XWA 0x9EA978
uint8_t g_bTexCacheOverflow;
// GLOBAL: XWA 0x9EA98C
uint8_t g_bTexCreateFailed;
// GLOBAL: XWA 0x686AF4
uint8_t g_flightRenderStatsDumpRequested;
// GLOBAL: XWA 0x9CF730
int g_flightRenderStatTexMemUsedBytes;
// GLOBAL: XWA 0x9CF734
int g_flightRenderStatVertCount;
// GLOBAL: XWA 0x9CF738
int g_flightRenderStatStateChanges;
// GLOBAL: XWA 0x9CF740
int g_flightRenderStatTriCount;
// GLOBAL: XWA 0x9CF744
int g_flightRenderStatBytesPurged;
// GLOBAL: XWA 0x9CF748
uint8_t g_flightRenderStatTexCacheOverflow;
// GLOBAL: XWA 0x9CF749
uint8_t g_flightRenderStatTexCreateFailed;
// GLOBAL: XWA 0x9CF74C
int g_flightRenderStatTexSwitches;
// GLOBAL: XWA 0x9CF750
int g_flightRenderStatBytesCached;
// GLOBAL: XWA 0x7827FC
float g_fpsSampleHistory[5];
// GLOBAL: XWA 0x5B4650
OptTexCoord g_defaultQuadTexCoords[4] = {
	{ 1.0f, 1.0f },
	{ 0.0f, 1.0f },
	{ 0.0f, 0.0f },
	{ 1.0f, 0.0f },
};
// GLOBAL: XWA 0x5B52A0
OptTexCoord g_mfdFrameQuadTexCoords[4] = {
	{ 0.0f, 1.0f },
	{ 1.0f, 1.0f },
	{ 1.0f, 0.0f },
	{ 0.0f, 0.0f },
};
// GLOBAL: XWA 0x9EA8E0
OptTexCoord g_backdropStripTexCoords[4];
// GLOBAL: XWA 0x9CF73C
OptTexCoord* g_currentQuadTexCoords = g_defaultQuadTexCoords;
static RenderObjectListEntry g_renderObjectListEntryStorage[1664];
// GLOBAL: XWA 0x8C1638
RenderObjectListEntry* g_renderObjectListEntries = g_renderObjectListEntryStorage;
// GLOBAL: XWA 0x7D4B74
RenderObjectListEntry* g_renderListHead;
// GLOBAL: XWA 0x8C1A80
DeathStarTunnelLaserRegionState g_deathStarTunnelLaserRegions[5];
// GLOBAL: XWA 0x5FF538
const int flareSpriteOrColor[6] = {
	0x08ffffff, 0x16ffffff, 0x32ffffff, 0x32ffffff, 0x16ffffff, 0x16ffffff,
};
// GLOBAL: XWA 0x68C980
uint8_t g_meshClipCornerOutcodes[8];
// GLOBAL: XWA 0x68C990
float g_meshClipScreenX[8];
// GLOBAL: XWA 0x68C9B0
float g_meshClipScreenY[8];
// GLOBAL: XWA 0x68C9D0
Vec3f g_meshClipBoxCornersView[8];
// GLOBAL: XWA 0x68CA38
float g_meshClipInvZ[8];
// GLOBAL: XWA 0x9B6C68
float g_invProjScale;
// GLOBAL: XWA 0x5B46B4
float g_depthProjScale = 2048.0f;
// GLOBAL: XWA 0x686B34
float g_renderProjectionDepthOverrideZ;
// GLOBAL: XWA 0x6626F4
float g_renderProjectionClampY0;
// GLOBAL: XWA 0x63D194
float g_renderProjectionClampY1;
// GLOBAL: XWA 0x662814
float g_renderProjectionClampX0;
// GLOBAL: XWA 0x64D1A4
float g_renderProjectionClampX1;
// GLOBAL: XWA 0x686B38
int16_t g_renderProjectionYClampEnabled;
// GLOBAL: XWA 0xB0CFF8
int g_std3DZCmpMode = 16;
// GLOBAL: XWA 0xB0E7A4
Std3DZBufferSurfaceBlock g_std3DZBufferSurface;
// GLOBAL: XWA 0x773314
uint8_t g_hwMipmapFilter;
// GLOBAL: XWA 0x9B6CAA
SceneFace* g_visFaceList;
// GLOBAL: XWA 0x9B6CAE
uint16_t g_visFaceListHandle;
// GLOBAL: XWA 0x9B6CEC
uint8_t* g_sceneSclEdgeList;
// GLOBAL: XWA 0x9B6CF0
uint16_t g_sceneSclEdgeListHandle;
// GLOBAL: XWA 0x9B6CF2
uint8_t* g_scanlineSpanHeads;
// GLOBAL: XWA 0x9B6CF6
uint16_t g_scanlineSpanHeadsHandle;
// GLOBAL: XWA 0x9B6CBC
ProjVertex* g_projVertList;
// GLOBAL: XWA 0x9B6CC0
uint16_t g_projVertListHandle;
// GLOBAL: XWA 0x9B6CC2
int g_projVertCount;
// GLOBAL: XWA 0x9B6CC6
int g_projVertMax;
// GLOBAL: XWA 0x9B6CF8
SceneMesh* g_meshQueue;
// GLOBAL: XWA 0x9B6CFC
uint16_t g_meshQueueHandle;
// GLOBAL: XWA 0x9B6CFE
int g_meshQueueMax;
// GLOBAL: XWA 0x9B6CB8
int g_sceneFaceMax;
// GLOBAL: XWA 0x9B6CD0
int g_sceneEdgeCursor;
// GLOBAL: XWA 0x782854
ObjectRenderState* g_objRenderState;
// GLOBAL: XWA 0x761E70
GlowMarkRequest g_glowMarkRequestPool[64];
// GLOBAL: XWA 0x91AEE4
int g_glowMarkRequestCount;
// GLOBAL: XWA 0x763130
int g_glowMarkSavedCollisionSegmentStartX;
// GLOBAL: XWA 0x763134
int g_glowMarkSavedCollisionSegmentStartY;
// GLOBAL: XWA 0x763138
int g_glowMarkSavedCollisionSegmentStartZ;
// GLOBAL: XWA 0x76D34C
int g_glowMarkSavedCollisionProbeWorldX;
// GLOBAL: XWA 0x76D350
int g_glowMarkSavedCollisionProbeWorldY;
// GLOBAL: XWA 0x76D354
int g_glowMarkSavedCollisionProbeWorldZ;
// GLOBAL: XWA 0x76E564
int g_glowMarkSavedCollisionSweepStartX;
// GLOBAL: XWA 0x76E56C
int g_glowMarkSavedCollisionSweepStartY;
// GLOBAL: XWA 0x76E57C
int g_glowMarkSavedCollisionSweepStartZ;
// GLOBAL: XWA 0x76D32C
int g_glowMarkSavedCollisionSweepEndX;
// GLOBAL: XWA 0x76D330
int g_glowMarkSavedCollisionSweepEndY;
// GLOBAL: XWA 0x76D328
int g_glowMarkSavedCollisionSweepEndZ;
// GLOBAL: XWA 0x76D340
int g_glowMarkSavedCollisionHitOffsetX;
// GLOBAL: XWA 0x76E560
int g_glowMarkSavedCollisionHitOffsetY;
// GLOBAL: XWA 0x76D358
int g_glowMarkSavedCollisionHitOffsetZ;
// GLOBAL: XWA 0x91AEA0
int g_glowMarkSegmentStartWorld[3];
// GLOBAL: XWA 0x91AE90
int g_glowMarkSegmentEndWorld[3];
// GLOBAL: XWA 0x91AEC0
GlowMarkPlaneScratch g_glowMarkPlaneScratch;
// GLOBAL: XWA 0x91AE84
Vec3f* g_glowMarkScratchNormalVec = &g_glowMarkPlaneScratch.normal;
// GLOBAL: XWA 0x91AEAC
Vec3f* g_glowMarkScratchUAxisRefVec = &g_glowMarkPlaneScratch.uAxis;
// GLOBAL: XWA 0x91AEE8
uint16_t g_glowMarkWorldSegmentMode;
// GLOBAL: XWA 0x76E580
int g_glowMarkTraversalActive;
// GLOBAL: XWA 0x76D338
float g_glowMarkInvScaleU;
// GLOBAL: XWA 0x76D33C
float g_glowMarkInvScaleV;
// GLOBAL: XWA 0x76D348
int g_glowMarkMeshVertexCount;
// GLOBAL: XWA 0x76D360
GlowMarkVertexProjection g_glowMarkVertexProj[512];
// GLOBAL: XWA 0x76E568
int g_glowMarkMeshOrdinal;
// GLOBAL: XWA 0x76E574
float g_glowMarkMeshRotationAngle;
// GLOBAL: XWA 0x76E588
float* g_glowMarkMeshVertexArray;
// GLOBAL: XWA 0x76E58C
uint8_t* g_shieldGlowFrameScales;
// GLOBAL: XWA 0x76E590
uint8_t* g_hullLightFrameScales;
// GLOBAL: XWA 0x76E594
uint8_t* g_magElectFrameScales;
// GLOBAL: XWA 0x76E598
uint8_t* g_ionElectFrameScales;
// GLOBAL: XWA 0x76E59C
uint8_t* g_blastMarkFrameScales;
// GLOBAL: XWA 0x76E5A0
uint8_t* g_tractorBeamFrameScales;
// GLOBAL: XWA 0x763140
ObjectMeshTextureLayerBlock g_glowMarkPatchPool[24];
// GLOBAL: XWA 0x76D344
ObjectMeshTextureLayerBlock* g_glowMarkFreeList;
// GLOBAL: XWA 0x76E578
int g_glowMarkMaxActiveIndex;
// GLOBAL: XWA 0x7676A0
ObjectMeshTextureLayerBlock* g_blastMarkFreeList;
// GLOBAL: XWA 0x7676A8
ObjectMeshTextureLayerBlock g_blastMarkPatchPool[32];
// GLOBAL: XWA 0x76D334
int g_blastMarkMaxActiveIndex;
// GLOBAL: XWA 0x63D110
int g_clipIdxAStorage[32];
// GLOBAL: XWA 0x6628E8
int g_clipIdxBStorage[32];
// GLOBAL: XWA 0x64D1A0
int* g_clipIdxA = g_clipIdxAStorage;
// GLOBAL: XWA 0x63D198
int* g_clipIdxB = g_clipIdxBStorage;
// GLOBAL: XWA 0x662820
int g_clipCountA;
// GLOBAL: XWA 0x686AB0
int g_clipCountB;
// GLOBAL: XWA 0x686AB4
int g_clipVertCursor;
// GLOBAL: XWA 0x686AFC
int g_clipOccurred;
// GLOBAL: XWA 0x662840
int g_clipInputProjVertEndIndex;
// GLOBAL: XWA 0x662844
int g_unusedD3DMaxVertsClamped384;
// GLOBAL: XWA 0x63D0F0
float g_clipCurW;
// GLOBAL: XWA 0x63D0F8
float g_clipDeltaTu;
// GLOBAL: XWA 0x63D0FC
float g_clipDeltaTv;
// GLOBAL: XWA 0x6626F8
float g_clipPrevColor0;
// GLOBAL: XWA 0x6626FC
float g_clipCurColor0;
// GLOBAL: XWA 0x662700
float g_clipPrevColor3;
// GLOBAL: XWA 0x662704
float g_clipCurColor3;
// GLOBAL: XWA 0x662708
float g_clipAttrPrev0[16];
// GLOBAL: XWA 0x662748
float g_clipAttrCur0[16];
// GLOBAL: XWA 0x662788
float g_clipPrevColor2;
// GLOBAL: XWA 0x66278C
float g_clipCurColor2;
// GLOBAL: XWA 0x662790
float g_clipAttrPrev1[16];
// GLOBAL: XWA 0x6627D0
float g_clipAttrCur1[16];
// GLOBAL: XWA 0x662810
float g_clipScreenDenom;
// GLOBAL: XWA 0x662818
float g_clipPrevColor1;
// GLOBAL: XWA 0x66281C
float g_clipCurColor1;
// GLOBAL: XWA 0x66284C
float g_clipPrevTu;
// GLOBAL: XWA 0x662850
float g_clipCurTu;
// GLOBAL: XWA 0x662854
float g_clipPrevTv;
// GLOBAL: XWA 0x662858
float g_clipCurTv;
// GLOBAL: XWA 0x662860
float g_clipAttrDelta0[16];
// GLOBAL: XWA 0x686AC0
float g_clipDeltaColor0;
// GLOBAL: XWA 0x686AC4
float g_clipDeltaColor3;
// GLOBAL: XWA 0x686AC8
float g_clipDeltaColor2;
// GLOBAL: XWA 0x686AD0
float g_clipDeltaColor1;
// GLOBAL: XWA 0x686AD4
float g_clipDeltaSx;
// GLOBAL: XWA 0x686AD8
float g_clipDeltaSy;
// GLOBAL: XWA 0x686ADC
float g_clipNearDenom;
// GLOBAL: XWA 0x686AE0
float g_clipPrevSx;
// GLOBAL: XWA 0x686AE4
float g_clipCurSx;
// GLOBAL: XWA 0x686AE8
float g_clipPrevSy;
// GLOBAL: XWA 0x686AEC
float g_clipCurSy;
// GLOBAL: XWA 0x686AF0
float g_clipPrevW;
// GLOBAL: XWA 0x6628A0
float g_clipAttrDelta1[16];
// GLOBAL: XWA 0x7FFD74
int g_meshRenderFlags;
// GLOBAL: XWA 0x686AF8
int16_t g_renderSceneForceDefaultTexture;
// GLOBAL: XWA 0x63D190
int g_d3dRenderStatePreset0_unused;
// GLOBAL: XWA 0x662838
int g_d3dRenderStateUntexturedFace;
// GLOBAL: XWA 0x63D19C
int g_d3dRenderStateTexturedMesh;
// GLOBAL: XWA 0x66285C
int g_d3dRenderStateDeferredAlphaMesh;
// GLOBAL: XWA 0x662824
int g_d3dRenderStateMeshPass2;
// GLOBAL: XWA 0x6626F0
int g_d3dRenderStateMultiTextureMesh;
// GLOBAL: XWA 0x662828
int g_d3dRenderStateGlowQuad;
// GLOBAL: XWA 0x63D104
int g_d3dRenderStatePreset7_unused;
// GLOBAL: XWA 0x686B0C
RenderBatch* g_meshDeferredBatch;
// GLOBAL: XWA 0x686B14
RenderBatch* g_meshPass2Batch;
// GLOBAL: XWA 0x686B10
RenderBatch* g_meshMultiTexBatch;
// GLOBAL: XWA 0x7B4C00
XwaMissionHeader g_missionHeader;

static Std3DDevice g_defaultStd3DDevice = {
	{
		1, 1, 1, 1, 1, 0, 1, 0, 1, 1, { 0 }, 0, 0, 0, 0, 0, 4096, 4096, 0x10000, 256, 6, { 0 },
	},
};

// GLOBAL: XWA 0x7B1CE0
Std3DDevice* g_pStd3DCurDevice = &g_defaultStd3DDevice;

// GLOBAL: XWA 0x9B6CCE
uint16_t g_sceneEdgeListHandle;
// GLOBAL: XWA 0x9B6CD4
int g_sceneEdgeMax;
// GLOBAL: XWA 0x9B6CCA
void* g_sceneEdgeList;
// GLOBAL: XWA 0x9B6CDC
uint16_t g_vertexRemapHandle;
// GLOBAL: XWA 0x9B6CDE
int g_vertexRemapCapacity;
// GLOBAL: XWA 0x9B6CD8
int* g_vertexRemap;
// GLOBAL: XWA 0x9B6CE6
uint16_t g_sceneEdgeFlagsHandle;
// GLOBAL: XWA 0x9B6CE8
int g_sceneEdgeFlagsCapacity;
// GLOBAL: XWA 0x9B6CE2
void* g_sceneEdgeFlags;

// GLOBAL: XWA 0x5B4670
const int g_d3dRenderStateDefaultFlags[8] = {
	0x00009a13, 0x00018a13, 0x00008e13, 0x00009813, 0x00018a13, 0x0001ae13, 0x00008e12, 0x00089613,
};

#ifndef XWA_MODERN
#pragma optimize("", off)
#endif
// FUNCTION: XWA 0x593E9B
char std3D_SetMipmapFilter(char filterType) {
	if (filterType > (char)g_pStd3DCurDevice->caps.mipmapCapLevel) {
		filterType = g_pStd3DCurDevice->caps.mipmapCapLevel;
	}

	switch (filterType) {
		case 0:
			g_d3dTexFilterPoint = 1;
			g_d3dTexFilterLinear = 2;
			break;
		case 1:
			g_d3dTexFilterPoint = 3;
			g_d3dTexFilterLinear = 4;
			break;
		case 2:
			g_d3dTexFilterPoint = 3;
			g_d3dTexFilterLinear = 6;
			break;
		default:
			break;
	}
	return filterType;
}
#ifndef XWA_MODERN
#pragma optimize("", on)
#endif

void Renderer_InitHardwareMaxQualitySettings(void) {
	/* Port replacement for the config-to-render-global setup in Flight_Main @
	   0x50AC9A..0x50AEFF, fixed to the highest-quality hardware profile. */
	g_useHardware3D = 1;
	g_bilinearEnabled = 1;
	g_hwMipmapFilter = std3D_SetMipmapFilter(2);
	g_lodDistanceScale = 0.4f;
	g_mipLodScale = 0.94736844f;
	g_keepFullResTextures = 2;
	g_localLightsLevel = 2;
	g_specularEnabled = 1;
	g_dirLightingEnabled = 1;
	g_forcedLodLevel = 0;
	Renderer_InitD3DRenderStatePresets();
}

void Renderer_InitFrontendHardwareSettings(void) {
	/* Original frontend render globals before Flight_Main applies flight graphics config. */
	g_useHardware3D = 1;
	g_bilinearEnabled = 1;
	g_hwMipmapFilter = std3D_SetMipmapFilter(2);
	g_keepFullResTextures = 1;
	g_localLightsLevel = 1;
	g_specularEnabled = 1;
	g_dirLightingEnabled = 1;
	g_forcedLodLevel = 0;
	Renderer_InitD3DRenderStatePresets();
}

// FUNCTION: XWA 0x4531D0
int16_t Renderer_CanUseBilinearFiltering(void) {
	if (g_useHardware3D && g_pStd3DCurDevice) {
		return g_pStd3DCurDevice->caps.bLinearFilter;
	}
	return 1;
}
