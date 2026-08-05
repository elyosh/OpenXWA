#ifndef XWA_RENDER_RENDERER_INTERNAL_H
#define XWA_RENDER_RENDERER_INTERNAL_H

#include "xwa/render/renderer.h"

#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_texture.h"
#include "xwa/assets/model_type.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/flight_text.h"
#include "xwa/flight/flight_light.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/math/fixed.h"
#include "xwa/math/scalar.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/player/player.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/random.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define RENDER_DATAPOOL_TAG "DATAPOOL"

#if defined(__clang__) && defined(_MSC_VER) && _MSC_VER == 1100
// FUNCTION: XWA 0x59AEC0
static __inline void __chkstk(void) {}
#endif

extern const float g_renderMinD3DDepth;
extern const float g_renderDistantDepth;
extern const float g_renderNegDistantDepth;
extern const float g_renderZeroFloat;
extern const float g_renderRoughDistanceScale;
extern const float g_renderUnitFloat;
extern const float g_renderPointLightFacingThreshold;
extern const float g_renderNegUnitFloat;
extern const float g_sw3dZeroFloat;
extern const float g_sw3dUnitFloat;
extern const float g_sw3dNegUnitFloat;
extern const float g_sw3dLaserScale;
extern const float g_modelNodeAxisScale;
extern const float g_modelNodeHalfAngleScale;
extern const float g_sw3dLightZero;
extern const float g_sw3dLightUnit;
extern const float g_renderHalfFloat;
extern const float g_renderOverbrightBleedScale;
extern const float g_renderNegHalfFloat;
extern const double g_vertexColorAlphaScale;
extern const float g_renderTrailScreenScaleBase;
extern const float g_argbAlphaToUnitScale;
extern const float g_argbRedToUnitScale;
extern const float g_argbGreenToUnitScale;
extern const float g_argbBlueToUnitScale;
extern const float g_renderBillboardNearCullZ;
extern const float g_renderProjectionDepthBias;

extern uint16_t g_sceneEdgeListHandle;
extern int      g_sceneEdgeMax;
extern void*    g_sceneEdgeList;
extern uint16_t g_vertexRemapHandle;
extern int      g_vertexRemapCapacity;
extern int*     g_vertexRemap;
extern uint16_t g_sceneEdgeFlagsHandle;
extern int      g_sceneEdgeFlagsCapacity;
extern void*    g_sceneEdgeFlags;

extern uint16_t         g_std3DPaletteScratch16[256];
extern uint16_t         g_texConvBuf1555[256];
extern uint16_t         g_texConvBuf4444[256];
extern XwaMissionHeader g_missionHeader;
extern uint16_t         g_targetAngleScore;

extern int                   g_drawSceneEffects;
extern int                   g_frameBytesPurged;
extern int                   g_frameTriCount;
extern int                   g_frameStateChanges;
extern int                   g_frameTexSwitches;
extern int                   g_frameBytesCached;
extern int                   g_frameVertCount;
extern int                   g_d3dBufVertCount;
extern int                   g_std3DExecBufTriCount;
extern int                   g_std3DOpened;
extern intptr_t              g_d3dCurTexture;
extern int                   g_d3dMaxVerts;
extern Std3DViewportRect     g_std3DQuadRect;
extern D3DTLVERTEX           g_std3DQuadVerts[4];
extern Std3DRenderTri        g_std3DQuadTris[2];
extern Std3DRenderStateFlags g_d3dStateFlags;
extern int                   g_d3dTexFilterPoint;
extern int                   g_d3dTexFilterLinear;
extern float                 g_totalTexSwitches;
extern float                 g_totalTris;
extern float                 g_totalVerts;
extern float                 g_totalBytesPurged;
extern float                 g_totalFrames;
extern float                 g_totalBytesCached;
extern float                 g_totalStateChanges;
extern uint8_t               g_bTexCacheOverflow;
extern uint8_t               g_bTexCreateFailed;
extern uint8_t               g_flightRenderStatsDumpRequested;
extern int                   g_flightRenderStatTexMemUsedBytes;
extern int                   g_flightRenderStatVertCount;
extern int                   g_flightRenderStatStateChanges;
extern int                   g_flightRenderStatTriCount;
extern int                   g_flightRenderStatBytesPurged;
extern uint8_t               g_flightRenderStatTexCacheOverflow;
extern uint8_t               g_flightRenderStatTexCreateFailed;
extern int                   g_flightRenderStatTexSwitches;
extern int                   g_flightRenderStatBytesCached;
extern float                 g_fpsSampleHistory[5];
extern uint8_t               g_flightFontTier;
extern uint8_t               g_flightFontLineHeight;
extern uint8_t               g_flightFontScale;
extern int16_t               g_flightTextReservedState91079E;
extern const uint16_t        g_flightTextDecimalDivisors[6];
extern uint8_t               g_flightFontHasLowercase;
extern uint8_t               g_flightFontHalfHeight;
extern uint16_t              g_flightFontHwScaleDivisor;
extern uint8_t*              g_flightFontGlyphWidthsHw;
extern uint8_t*              g_flightFontGlyphTableSw;
extern uint16_t              g_flightFontGlyphStrideSw;
extern FlightTextGlyphBitmapHw*   g_flightActiveFontHwMetadata;
extern uint8_t*              g_flightFontSmallSw;
extern uint8_t*              g_flightFontMediumSw;
extern uint8_t               g_font0Widths[256];
extern uint8_t               g_font1Widths[256];
extern uint8_t               g_font2Widths[256];
extern uint8_t               g_flightTextBgColor;
extern uint8_t               g_flightTextColorIndex;
extern uint8_t               g_flightTextShadowEnabled;
extern uint8_t               g_flightTextShadowColor;
extern uint32_t              g_flightTextColorHwArgb;
extern uint32_t              g_flightTextShadowHwArgb;
extern int16_t               g_flightTextRenderOffsetX;
extern int16_t               g_flightTextRenderOffsetY;
extern uint16_t              g_flightWordWrapEnabled;
extern uint16_t              g_flightClearLineBgEnabled;
extern uint16_t              g_flightTextPalette[256];
extern int16_t               g_flightCursorX;
extern int16_t               g_flightCursorY;
extern int16_t               g_flightClipTop;
extern int16_t               g_flightClipLeft;
extern int16_t               g_flightClipBottom;
extern int16_t               g_flightClipRight;
extern uint8_t               g_flightColorEscapeBypassChar;
extern const uint8_t         g_flightCharToColorLut[32];

void RenderScene_EnsureMeshBatches(void);
int  RenderScene_AppendMeshFacesNoCull(SceneMesh* mesh);
void RenderScene_SetMeshGapInt(SceneMesh* mesh, int offset, intptr_t value);
void RenderScene_ApplyRotationScale(SceneMesh* mesh, Vec3f* pivot, Vec3f* axis, float angle);
int  sw3d_ProjectPreviewVisibleFaceVertices(SceneMesh* mesh);

void     LensFlare_InitQueue(void);
void     LensFlare_QueueSource(int argbColor);
void     LensFlare_RenderQueuedSources(void);
int      Targeting_GetObjectBoxExtent(int objectIdx);
int      Craft_IsSelectableDamageComponentMesh(ObjectTypeId objectType, int meshIndex);
int16_t  Targeting_ScoreCandidate(uint16_t candidateObjIdx, int16_t mode, int playerIdx, uint16_t subSystemIdx);
void     Targeting_DrawObjectBox(uint16_t objectIdx, uint16_t componentIdx, uint8_t colorIndex);
void     Targeting_DrawSceneObjectBoxes(void);
void     FlightText_FlushQueue(void);
extern char g_flightTextScratchBuffer[256];
void     FlightText_SetScratch(const char* text);
uint16_t FlightText_FormatScratchInt(int value);
void     FlightText_AppendScratchDecimalNumber(uint16_t value, unsigned int width,
											   unsigned int minDigits);
void     FlightText_DrawDecimalNumber(uint16_t value, unsigned int width, unsigned int minDigits);
void     FlightText_DrawString(const char* str);
void     FlightText_DrawStringCentered(const char* str);
void     FlightText_DrawStringRightAligned(const char* str);
uint16_t FlightText_MeasureStringWidth(const char* str);
void     FlightText_TruncateStringToWidth(char* str, unsigned int maxWidth);
void     FlightText_AppendScratchChar(uint8_t ch);
void     FlightText_AppendScratchString(const char* text);
void     FlightText_SetCursor(int x, int y);
int      FlightText_SetRenderOffset(int16_t x, int16_t y);
int16_t  FlightText_SetClipRect(int16_t left, int16_t top, uint16_t right, uint16_t bottom);
uint16_t FlightText_SetWordWrap(uint16_t enabled);
uint16_t FlightText_SetClearLineBackground(uint16_t enabled);
void FlightText_SetBackgroundColor(uint32_t charOrIndex);
void     FlightText_SetColor(unsigned int charOrIndex);
void     FlightText_SetShadowColor(unsigned int charOrIndex);
char     FlightText_SetFontTier(int tier);
uint16_t FlightText_SelectHardwareFontForLineHeight(uint16_t lineHeight);
void     FlightText_SetHardwareGlyphDepth(float depthZ);
uint8_t  FlightText_SetShadowEnabled(uint8_t enabled);

#endif
