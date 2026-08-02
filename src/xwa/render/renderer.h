#ifndef XWA_RENDER_RENDERER_H
#define XWA_RENDER_RENDERER_H

#include "xwa/assets/opt_model.h"
#include "xwa/assets/sprite_texture.h"
#include "xwa_runtime/compat/directx/d3d.h"
#include "xwa/math/vec3f.h"
#include "xwa/math/vec3i.h"
#include "xwa/flight/object/object.h"
#include "xwa/util/color.h"
#include "xwa/util/memory.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Matrix3x3 {
	float m[9];
} Matrix3x3;

typedef enum MeshClipFlags {
	MESH_CLIP_RIGHT = 1,
	MESH_CLIP_LEFT = 2,
	MESH_CLIP_BOTTOM = 4,
	MESH_CLIP_TOP = 8,
	MESH_CLIP_NEAR = 16,
	MESH_CLIP_FULLY_INSIDE = 32,
} MeshClipFlags;

typedef enum Std3DRenderStateFlags {
	STD3D_RS_FOG_ENABLE = 0x000040,
	STD3D_RS_TEXTURE_MAG_LINEAR = 0x000080,
	STD3D_RS_TEXTURE_MIN_LINEAR = 0x000100,
	STD3D_RS_ALPHA_BLEND = 0x000200,
	STD3D_RS_TEXTURE_MODULATE_ALPHA = 0x000400,
	STD3D_RS_Z_COMPARE_ENABLE = 0x000800,
	STD3D_RS_Z_WRITE_ENABLE = 0x001000,
	STD3D_RS_TEXTURE_ADDRESS_CLAMP = 0x002000,
	STD3D_RS_MONO_DISABLE = 0x008000,
	STD3D_RS_Z_COMPARE_PREFER_EQUAL = 0x010000,
	STD3D_RS_TEXTURE_BLEND_DECAL = 0x040000,
	STD3D_RS_ALPHA_TEST_DISABLE = 0x080000,
} Std3DRenderStateFlags;

struct MeshDescriptor;
typedef struct OptEngineGlow OptEngineGlow;
typedef struct EngineGlowKnockoutMark EngineGlowKnockoutMark;
typedef struct D3DInfoNode D3DInfoNode;
typedef struct Std3DDevice Std3DDevice;
typedef struct Std3DTexCacheNode Std3DTexCacheNode;
typedef struct Std3DTextureSurface Std3DTextureSurface;
typedef struct ObjectTrailPoint ObjectTrailPoint;
typedef struct ObjectTrailEmitter ObjectTrailEmitter;
typedef struct Std3DZBufferSurfaceBlock {
	IDirectDrawSurface* surface;
	int unused04;
	DDSURFACEDESC desc;
} Std3DZBufferSurfaceBlock;
/* ParticleEffect is fully defined in xwa/render/effects.h; ObjectRenderState
 * only stores a pointer to it, so a forward declaration suffices here. */
typedef struct ParticleEffect ParticleEffect;
typedef void (*FlightInitLineBufferFn)(void);
typedef int (*FlightDebugPrintFn)(const char* format, ...);
typedef char (*FlightResetPaletteFn)(void);
typedef void (*FlightSetPaletteRangeFn)(RgbTriplet* rgbTriples, int startIdx, uint16_t count);
typedef char* (*FlightGetPaletteFn)(char* dst768);
typedef void (*FlightSetPaletteFn)(RgbTriplet* srcRgb);
typedef int (*FlightComputePixelOffsetFn)(int x, int y);
typedef int (*FlightBlitSpriteFn)(uint8_t* rleData, int16_t x, int16_t y, int endMarker, int mirror);
typedef int (*FlightBlitSpriteFadedFn)(uint8_t* rleData, int16_t x, int16_t y, int endMarker,
									   char paletteShift, int16_t fadeAmount);
typedef uint8_t (*FlightDrawCharFn)(uint8_t ch);
typedef int (*FlightFillClipRectFn)(void);
typedef int16_t (*FlightFillRectClippedFn)(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
										   uint16_t borderThickness);
typedef int (*FlightSaveScreenRectFn)(int16_t* dst, int srcX, int srcY, int width, int height);
typedef void (*FlightRestoreScreenRectFn)(const int16_t* src, int dstX, int dstY, int width, int height);
typedef void (*FlightDrawPointArrayFn)(uint16_t* points, uint16_t count);
typedef uint16_t* (*FlightDrawPointArrayMaskedFn)(uint16_t* points, uint16_t count);
typedef int (*FlightDrawPixelFn)(uint16_t x, uint16_t y, char colorIdx);
typedef void (*FlightDrawRadarTargetMarkerFn)(void);
typedef int (*FlightRestoreRadarTargetMarkerFn)(void);
typedef void (*FlightDrawLineFn)(int x1, int y1, int x2, int y2, uint8_t colorIdx);
typedef struct WorldRectRecord {
	uint16_t modelType;
	Vec3i worldDirQ20;
	Vec3i viewDirQ20;
	int stripSegmentCount;
	int stripSegmentsPerFrame;
	Vec3i* stripCoords;
	int stripHalfHeight;
	float colorR;
	float colorG;
	float colorB;
	float intensity;
	int side;
	uint16_t angularScale;
	uint8_t drawFlags;
	uint8_t flags;
	uint8_t frame;
} WorldRectRecord;

typedef struct DeathStarTunnelLaserRegionState {
	int16_t enabled;
	int16_t shotActive;
	int16_t alternateDelayPhase;
	int16_t beamLightActive;
	int warmupTicks;
	int holdTicks;
	int travelTicks;
	int shotStartGameTime;
	int targetObjIdx;
	int laserObjIdx;
	int firstShotDelayTicks;
	int repeatShotDelayTicks;
	WorldRectRecord* emitterRect;
	WorldRectRecord* beamSpriteRect;
	int16_t gap30;
	int emitterOffsetX;
	int emitterOffsetY;
	int emitterOffsetZ;
	int targetFlightGroupIds[4];
	int targetFlightGroupCount;
	int nextTargetFlightGroupIndex;
	int beamStartX;
	int beamStartY;
	int beamStartZ;
	int pointLightX;
	int pointLightY;
	int pointLightZ;
	float remainingDistance;
} DeathStarTunnelLaserRegionState;

typedef struct OptTexCoord {
	float u;
	float v;
} OptTexCoord;

typedef struct FaceTextureGradients {
	Vec3f gradient0;
	Vec3f gradient1;
} FaceTextureGradients;

typedef struct FaceRecord {
	int vertexIdx[4];
	int edgeIdx[4];
	int uvIdx[4];
	int normalIdx[4];
} FaceRecord;

typedef struct MeshExtraTexturePass {
	float uvScale;
	uint32_t color;
	Std3DTextureSurface* texture;
} MeshExtraTexturePass;

typedef struct MeshExtraTextureLayer {
	int passCount;
	MeshExtraTexturePass passes[2];
	OptTexCoord* texCoords;
	uint16_t* faceEnabled;
} MeshExtraTextureLayer;

#pragma pack(push, 1)
typedef struct GlowMarkVertexProjection {
	float planeDistance;
	float planeSide;
	uint8_t clipMask;
} GlowMarkVertexProjection;
#pragma pack(pop)

typedef char glow_mark_vertex_projection_size[(sizeof(GlowMarkVertexProjection) == 0x09) ? 1 : -1];

typedef struct ObjectMeshTextureLayerBlock {
	uint16_t facePatchCount;
	Vec3f* meshVertexArrays[16];
	MeshExtraTextureLayer facePatches[16];
	Vec3f center;
	Vec3f normal;
	float planeD;
	Vec3f uAxis;
	Vec3f vAxis;
	uint8_t gap694[4];
	uint16_t objectIndex;
	uint16_t poolIndex;
	int currentFrame;
	uint16_t active;
	uint16_t modelType;
	uint16_t persistentUntilCleared;
	int effectParam;
	uint8_t gap716[4];
	float baseScale;
	uint8_t gap724[4];
	struct ObjectMeshTextureLayerBlock* freeNext;
	struct ObjectMeshTextureLayerBlock* prevActive;
	struct ObjectMeshTextureLayerBlock* nextActive;
} ObjectMeshTextureLayerBlock;

typedef struct GlowMarkPlaneGeometry {
	Vec3f center;
	Vec3f normal;
	Vec3f uAxis;
	Vec3f vAxis;
} GlowMarkPlaneGeometry;

typedef struct GlowMarkPlaneScratch {
	Vec3f center;
	Vec3f normal;
	Vec3f uAxis;
} GlowMarkPlaneScratch;

typedef struct GlowMarkSegmentGeometry {
	int startWorldX;
	int startWorldY;
	int startWorldZ;
	int endWorldX;
	int endWorldY;
	int endWorldZ;
	uint8_t reserved18[24];
} GlowMarkSegmentGeometry;

typedef union GlowMarkRequestGeometry {
	GlowMarkPlaneGeometry plane;
	GlowMarkSegmentGeometry segment;
	float values[12];
} GlowMarkRequestGeometry;

#pragma pack(push, 1)
typedef struct GlowMarkRequest {
	int objectIndex;
	int worldSegmentMode;
	uint16_t effectParam;
	GlowMarkRequestGeometry geom;
	uint16_t deriveUvAxesFromReference;
	uint16_t modelType;
	uint8_t persistentUntilCleared;
	float scaleU;
	float scaleV;
	struct GlowMarkRequest* next;
} GlowMarkRequest;
#pragma pack(pop)

typedef char
	glow_mark_request_size_check[(sizeof(GlowMarkRequest) == 0x47 + sizeof(GlowMarkRequest*)) ? 1 : -1];
typedef char glow_mark_request_geom_offset_check[(offsetof(GlowMarkRequest, geom) == 0x0A) ? 1 : -1];
typedef char glow_mark_request_next_offset_check[(offsetof(GlowMarkRequest, next) == 0x47) ? 1 : -1];

typedef struct ObjectRenderState {
	int drawnThisFrame;
	ParticleEffect* particleEffects;
	ObjectTrailEmitter* trailHead;
	GlowMarkRequest* pendingGlowMarks;
	ObjectMeshTextureLayerBlock* glowMarkTail;
	EngineGlowKnockoutMark* engineGlowKnockouts;
} ObjectRenderState;

struct EngineGlowKnockoutMark {
	uint16_t objectIndex;
	uint16_t modelIndex;
	uint8_t emitterIndex;
	ObjectMeshTextureLayerBlock* blastMark;
	EngineGlowKnockoutMark* next;
};

struct ObjectTrailPoint {
	Vec3f world;
	int spawnTime;
	float ageFade;
	float texV;
	ObjectTrailPoint* next;
#ifdef XWA_MODERN
	/* Exact shadow of `world`, captured before the original float cast. */
	int32_t preciseWorld[3];
#endif
};

struct ObjectTrailEmitter {
	int lifetimeTicks;
	float ageRate;
	uint16_t objectIndex;
	uint16_t sourceModelExtent;
	ObjectTrailPoint* pointHead;
	float ribbonWidth;
	int lastUpdateTime;
	float startAlphaBias;
	float alphaFadeStart;
	float alphaFadeRate;
	uint16_t animFrameCount;
	uint16_t animModelType;
	int animTicksAccum;
	float animRateScale;
	int forwardOffset;
	unsigned int argbColor;
	TexLevel* curTexLevel;
	uint32_t textureModelType;
	float texVRate;
	unsigned int renderFlags;
	int trailKind;
	uint16_t pointCount;
	ObjectTrailEmitter* next;
	uint8_t gap52[8];
};

typedef struct RenderObjectListEntry {
	int sortDepth;
	int objectIdx;
	struct RenderObjectListEntry* next;
	int viewX;
	int viewY;
	int viewZ;
	int cullFlags;
	int projectedRadius;
} RenderObjectListEntry;

/* Capacity of the per-frame visible-object render list (VISIBLEOBJECTS). */
#define RENDER_OBJECT_LIST_CAPACITY 1664

typedef struct FlightTexQuad {
	int screenX;
	int screenY;
	int depthZ;
	int16_t rotationAngle;
	uint16_t screenSize;
} FlightTexQuad;

#pragma pack(push, 1)
typedef struct SceneBillboardQueueEntry {
	uint16_t objectOrTypeIndex;
	uint16_t objectType;
	int16_t frame;
	int16_t screenSize;
	int16_t screenX;
	int16_t screenY;
	int32_t depthZ;
	int16_t rotationAngle;
} SceneBillboardQueueEntry;
#pragma pack(pop)

typedef char scene_billboard_queue_entry_size_check[(sizeof(SceneBillboardQueueEntry) == 0x12) ? 1 : -1];

typedef struct LensFlareSource {
	int argbColor;
	FlightTexQuad quads[7];
} LensFlareSource;

enum {
	XWA_BACKDROP_REGION_COUNT = 5,
	XWA_BACKDROP_RECORDS_PER_REGION = 32,
};

typedef struct ModelTextureOverrideSlot {
	uint16_t modelType;
	OptNode* textureNode;
} ModelTextureOverrideSlot;

typedef struct SceneEdge {
	int yEnd;
	int yStart;
	float x;
	float lightIntensity;
	float dxdy;
	float dLightIntensityDy;
	void* pClipVert;
} SceneEdge;

typedef struct SceneFace {
	int faceId;
	int faceIndex;
	struct SceneMesh* pMesh;
	int packed;
	int nearClipState;
	float gradients[9];
	float spanLightIntensityDx;
	SceneEdge* pScanEdge;
	void* pPhongData;
	int yTop;
	int yBot;
	float maxVertW;
	float minVertW;
	SceneEdge* edges[5];
	int edgeCount;
	struct SceneSpan** pSpans;
	int mipLevel;
} SceneFace;

typedef struct SceneSpan {
	struct SceneSpan* next;
	int startX;
	int endX;
	float startLightIntensity;
	float dLightIntensityDx;
	SceneFace* pFace;
} SceneSpan;

typedef struct SoftwareLightSample {
	int stamp;
	float intensity;
	float rowDelta;
} SoftwareLightSample;

typedef union Sw3dFloatInt {
	float asFloat;
	int asInt;
} Sw3dFloatInt;

typedef struct ProjVertex {
	float sx;
	float sy;
	float w;
	float lightIntensity;
	float litColor[4];
	float tu;
	float tv;
	OptTexCoord extraLayerUVs[16];
	int extraLayerUVCount;
} ProjVertex;

typedef struct SceneMesh {
	ObjectRecord* pObject;
	float rotAngle;
	Vec3f viewPos;
	float viewOrient[9];
	Vec3f pos;
	float orient[9];
	int nodeFlags[4];
	int vertexCount;
	Vec3f* pModelVerts;
	OptTexCoord* pUVs;
	Vec3f* pVertNormals;
	int field_136;
	int faceCount;
	int edgeCount;
	Vec3f* pFaceNormals;
	FaceTextureGradients* pFaceTexturing;
	int field_156;
	FaceRecord* pFaceGeom;
	intptr_t field_164;
	OptTextureData* pMaterial;
	void* pTexels;
	void* pPalette;
	intptr_t alphaFlag;
	D3DInfoNode* pTexture;
	int faceBaseIndex;
	int vertBaseIndex;
	int edgeBaseIndex;
	int visFaceCount;
	int projVertCursor;
	int clippedEdgeCount;
	struct MeshDescriptor* pMeshDescriptor;
	int textureLayerCount;
	MeshExtraTextureLayer* layers[16];
} SceneMesh;

/* D3DTLVERTEX is defined in the Direct3D compatibility header (included above). */

typedef struct Std3DViewportRect {
	int x;
	int y;
	int width;
	int height;
} Std3DViewportRect;

typedef struct Std3DRenderTri {
	int v0;
	int v1;
	int v2;
	int flags;
	Std3DTexCacheNode* texture;
} Std3DRenderTri;

/* Source raster descriptor. The tail (sourceType..alphaPosShiftRight) is the
 * embedded ColorInfo of the source pixels: sourceType == ColorInfo.colorMode,
 * bitsPerPixel == ColorInfo.bpp, followed by the per-channel bit widths, left
 * bit positions, and normalize-to-8-bit right shifts. The std3D texture
 * converters only read width/height/tileFactor/rowPitch/sourceType/bitsPerPixel;
 * the channel fields are populated by callers for completeness. */
typedef struct Std3DRasterInfo {
	uint32_t width;
	uint32_t height;
	uint32_t tileFactor;
	uint32_t rowPitch;
	uint32_t unk10;
	uint32_t sourceType;
	uint32_t bitsPerPixel;
	uint32_t redBPP;
	uint32_t greenBPP;
	uint32_t blueBPP;
	uint32_t redPosShift;
	uint32_t greenPosShift;
	uint32_t bluePosShift;
	uint32_t redPosShiftRight;
	uint32_t greenPosShiftRight;
	uint32_t bluePosShiftRight;
	uint32_t alphaBPP;
	uint32_t alphaPosShift;
	uint32_t alphaPosShiftRight;
} Std3DRasterInfo;

typedef struct Std3DVBuffer {
	int storageType;
	int lockCount;
	int unk08;
	Std3DRasterInfo raster;
	int unk58;
	void* pixels;
	uint32_t transparentColor;
	void* ddSurface;
	uint8_t reserved68[112];
} Std3DVBuffer;

typedef struct RenderBatch {
	D3DTLVERTEX verts[384];
	int vertexCount;
	Std3DRenderTri tris[384];
	int triCount;
	struct RenderBatch* next;
} RenderBatch;

struct Std3DTexCacheNode {
	void* pCachedTexture;
	void* pCachedSurface;
	uint8_t ddsd[108];
	uint32_t texHandle;
	int8_t bCached;
	uint8_t gap_79[3];
	uint32_t texWidth;
	uint32_t texHeight;
	uint32_t byteSize;
	uint32_t cacheFrameTag;
	struct Std3DTexCacheNode* pPrev;
	struct Std3DTexCacheNode* pNext;
};

struct Std3DTextureSurface {
	int8_t bAllocated;
	uint8_t gap_1[3];
	void* pSrcSurface;
	void* pSrcTexture;
	void* paletteHandle;
	Std3DTexCacheNode cacheNode;
	uint8_t bHardwareMipmap;
	uint8_t gap_A5[3];
	uint32_t mipLevelIndex;
	uint32_t mipLevelCount;
};

typedef struct Std3DDeviceCaps {
	int8_t bHardware;
	int8_t bTexturePerspective;
	int8_t bHasZBuffer;
	int8_t bColorKeyTexture;
	int8_t bAlphaTexture;
	int8_t bStippledShade;
	uint8_t bAlphaBlend;
	int8_t bSquareOnlyTexture;
	int8_t bClampSupported;
	int8_t bLinearFilter;
	uint8_t gap_A[6];
	uint32_t colorModelFlags;
	uint32_t renderBitDepthMask;
	uint32_t zCmpCapsMask;
	uint32_t minTextureWidth;
	uint32_t minTextureHeight;
	uint32_t maxTextureWidth;
	uint32_t maxTextureHeight;
	uint32_t maxBufferSize;
	uint32_t maxVertexCount;
	uint8_t mipmapCapLevel;
	uint8_t gap_35[3];
} Std3DDeviceCaps;

struct Std3DDevice {
	Std3DDeviceCaps caps;
	char deviceName[128];
	char deviceDescription[128];
	uint32_t totalMemory;
	uint32_t availableMemory;
	uint8_t d3dDesc[252];
	uint8_t guid[16];
};

struct D3DInfoNode {
	int textureId;
	uint16_t refCount;
	OptTextureData textureData;
	uint8_t hasAlphaData;
	int mipLevelCount;
	Std3DTextureSurface* baseMipSurfaces[6];
	Std3DTextureSurface* lightmapMipSurfaces[6];
	struct D3DInfoNode* next;
	struct D3DInfoNode* prev;
#ifdef XWA_MODERN
	uint32_t bridgeRefId;
#endif
};

#define XWA_D3DINFO_POOL_COUNT 1624

typedef struct FlightViewportSaveState {
	uint16_t viewportX;
	uint16_t pad02;
	uint16_t viewportY;
	uint16_t pad06;
	int camMatR0_X;
	int camMatR1_X;
	int camMatR0_Y;
	int camMatR1_Y;
	int camMatR2_X;
	int camMatR0_Z;
	int camMatR1_Z;
	int camMatR2_Y;
	int camMatR2_Z;
	uint16_t baseOffset;
	uint16_t pad2E;
	uint16_t height;
	uint16_t pad32;
	uint16_t width;
	uint16_t pad36;
} FlightViewportSaveState;

typedef enum ViewportCullFlags {
	VIEWPORT_CULL_NONE = 0,
	VIEWPORT_CULL_RIGHT = 1,
	VIEWPORT_CULL_LEFT = 2,
	VIEWPORT_CULL_TOP = 4,
	VIEWPORT_CULL_BOTTOM = 8,
	VIEWPORT_CULL_NEAR = 16,
	VIEWPORT_CULL_INSIDE = 32,
} ViewportCullFlags;

typedef enum RenderSceneClipFlags {
	RENDER_SCENE_CLIP_FORCE_FULL = 64,
} RenderSceneClipFlags;

extern int g_useHardware3D;
extern uint8_t g_palettePackedMode;
extern uint16_t g_flightTextPalette[256];
extern uint8_t g_flightColorEscapeBypassChar;
extern uint8_t g_unusedFlightRenderColorByte;
extern uint8_t g_flightGraphicsDetailPreset;
extern int g_debrisDensityLevel;
extern uint8_t g_usePalettizedTextures;
extern int g_frontendD3DInitialized;
extern int g_loadingModel;
extern int g_projOffsetY;
extern float g_projOffsetYf;
extern float g_lodDistanceScale;
extern float g_mipLodScale;
extern int g_sw3dMipmapEnabled;
extern uint8_t g_flightSurfaceAlreadyLocked;
extern int g_localLightsLevel;
extern int g_specularEnabled;
extern int g_keepFullResTextures;
extern int g_explosionResLevel;
extern int g_dirLightingEnabled;
extern uint8_t g_hitEffectsEnabled;
extern uint8_t g_particleEffectsEnabled;
extern uint8_t g_trailsEnabled;
extern int g_flightRenderModeId;
extern uint16_t g_flightVpWidth;
extern uint16_t g_flightVpHeight;
extern uint16_t g_flightVpMaxX;
extern uint16_t g_flightVpMaxY;
extern uint16_t g_flightVpCenterX;
extern uint16_t g_flightVpCenterY;
extern float g_flightVpCenterXf;
extern float g_flightVpCenterYf;
extern int g_flightVpProjScaleX;
extern int g_flightVpX;
extern int g_flightVpY;
extern int g_flightVpBaseOffset;
extern FlightViewportSaveState g_savedFlightViewport;
extern int16_t g_maskBufferOffset;
extern void* g_surfacePixels;
extern unsigned int g_screenHeight;
extern int g_surfacePitch;
extern int g_surfaceWidth;
extern int g_surfaceHeight;
extern unsigned int g_screenWidth;
extern int width;
extern int height;
extern int g_projScaleInt;
extern int g_projScaleHalfInt;
extern uint8_t perspShift;
extern uint16_t g_projAspectY;
extern float g_projScaleDiv512;
extern float g_projScale;
extern int16_t g_sceneBillboardQueueCount;
extern SceneBillboardQueueEntry g_sceneBillboardQueue[32];
extern int16_t g_lensFlareQueueCount;
extern LensFlareSource g_lensFlareQueue[4];
extern int g_renderFlags;
extern int viewX;
extern int viewY;
extern int viewZ;
extern int g_camRelWorldX;
extern int g_camRelWorldY;
extern int g_camRelWorldZ;
extern int g_curModelMaxExtent;
extern int g_modelPreviewModelType;
extern ObjectRecord* g_objectTable;
extern Vec3f g_modelPreviewViewDelta;
extern int g_drawingOwnCraft;
extern Vec3f g_modelPreviewNegViewDelta;
extern Matrix3x3 g_modelPreviewObjectViewMatrix;
extern Matrix3x3 mat;
extern float g_viewMtx00;
extern float g_viewMtx01;
extern float g_viewMtx02;
extern float g_viewMtx10;
extern float g_viewMtx11;
extern float g_viewMtx12;
extern float g_viewMtx20;
extern float g_viewMtx21;
extern float g_viewMtx22;
extern int g_camMatR0_X;
extern int g_camMatR0_Y;
extern int g_camMatR0_Z;
extern int g_camMatR1_X;
extern int g_camMatR1_Y;
extern int g_camMatR1_Z;
extern int g_camMatR2_X;
extern int g_camMatR2_Y;
extern int g_camMatR2_Z;
extern int g_curMatR0_X;
extern int g_curMatR0_Y;
extern int g_curMatR0_Z;
extern int g_curMatR1_X;
extern int g_curMatR1_Y;
extern int g_curMatR1_Z;
extern int g_curMatR2_X;
extern int g_curMatR2_Y;
extern int g_curMatR2_Z;
extern int g_fviewMoveX_Q15;
extern int g_fviewMoveY_Q15;
extern int g_fviewMoveZ_Q15;
extern int g_fviewFwdX_Q15;
extern int g_fviewFwdY_Q15;
extern int g_fviewFwdZ_Q15;
extern int g_fviewSideX_Q15;
extern int g_fviewSideY_Q15;
extern int g_fviewSideZ_Q15;
extern int g_fviewUpX_Q15;
extern int g_fviewUpY_Q15;
extern int g_fviewUpZ_Q15;
extern float g_objViewMatF_R0_X;
extern float g_objViewMatF_R0_Y;
extern float g_objViewMatF_R0_Z;
extern float g_objViewMatF_R1_X;
extern float g_objViewMatF_R1_Y;
extern float g_objViewMatF_R1_Z;
extern float g_objViewMatF_R2_X;
extern float g_objViewMatF_R2_Y;
extern float g_objViewMatF_R2_Z;
extern int g_objViewMat_R0_X;
extern int g_objViewMat_R0_Y;
extern int g_objViewMat_R0_Z;
extern int g_objViewMat_R1_X;
extern int g_objViewMat_R1_Y;
extern int g_objViewMat_R1_Z;
extern int g_objViewMat_R2_X;
extern int g_objViewMat_R2_Y;
extern int g_objViewMat_R2_Z;
extern int g_projectedFaceTraceCount;
extern int g_projectedFaceTraceX[8000];
extern int g_projectedFaceTraceY[8000];
extern int g_modelVertCapacity;
extern int g_modelEdgeCapacity;
extern int g_currentRenderMode;
extern int g_std3DStartScenePending;
extern int g_d3dIndexCount;
extern int g_d3dVertexCount;
extern int g_d3dVertexAlphaStateResetSlot;
extern int g_capVertexAlpha;
extern float g_flightVpOriginX;
extern float g_flightVpOriginY;
extern int g_maxBatchVerts;
extern int g_maxBatchTris;
extern D3DTLVERTEX* g_flightVertexBuffer;
extern Std3DRenderTri* g_triBuffer;
extern uint8_t* g_sceneSpanData;
extern uint16_t g_sceneSpanDataHandle;
extern int g_sceneSpanDataMax;
extern uint8_t* g_sceneSpanPtrList;
extern uint16_t g_sceneSpanPtrListHandle;
extern int g_missionRegionCount;
extern WorldRectRecord g_backdropRecordsByRegion[XWA_BACKDROP_REGION_COUNT][XWA_BACKDROP_RECORDS_PER_REGION];
extern int g_backdropCountByRegion[XWA_BACKDROP_REGION_COUNT];
extern SceneFace* g_visFaceList;
extern uint16_t g_visFaceListHandle;
extern uint8_t* g_sceneSclEdgeList;
extern uint16_t g_sceneSclEdgeListHandle;
extern uint8_t* g_scanlineSpanHeads;
extern uint16_t g_scanlineSpanHeadsHandle;
extern ProjVertex* g_projVertList;
extern uint16_t g_projVertListHandle;
extern int g_projVertCount;
extern int g_projVertMax;
extern SceneMesh* g_meshQueue;
extern uint16_t g_meshQueueHandle;
extern int g_meshQueueMax;
extern int g_sceneFaceMax;
extern int g_sceneEdgeCursor;
extern ObjectRenderState* g_objRenderState;
extern GlowMarkRequest g_glowMarkRequestPool[64];
extern int g_glowMarkRequestCount;
extern int g_glowMarkSavedCollisionSegmentStartX;
extern int g_glowMarkSavedCollisionSegmentStartY;
extern int g_glowMarkSavedCollisionSegmentStartZ;
extern int g_glowMarkSavedCollisionProbeWorldX;
extern int g_glowMarkSavedCollisionProbeWorldY;
extern int g_glowMarkSavedCollisionProbeWorldZ;
extern int g_glowMarkSavedCollisionSweepStartX;
extern int g_glowMarkSavedCollisionSweepStartY;
extern int g_glowMarkSavedCollisionSweepStartZ;
extern int g_glowMarkSavedCollisionSweepEndX;
extern int g_glowMarkSavedCollisionSweepEndY;
extern int g_glowMarkSavedCollisionSweepEndZ;
extern int g_glowMarkSavedCollisionHitOffsetX;
extern int g_glowMarkSavedCollisionHitOffsetY;
extern int g_glowMarkSavedCollisionHitOffsetZ;
extern int g_glowMarkSegmentStartWorld[3];
extern int g_glowMarkSegmentEndWorld[3];
extern GlowMarkPlaneScratch g_glowMarkPlaneScratch;
extern Vec3f* g_glowMarkScratchNormalVec;
extern Vec3f* g_glowMarkScratchUAxisRefVec;
extern uint16_t g_glowMarkWorldSegmentMode;
extern int g_glowMarkTraversalActive;
extern float g_glowMarkInvScaleU;
extern float g_glowMarkInvScaleV;
extern int g_glowMarkMeshVertexCount;
extern GlowMarkVertexProjection g_glowMarkVertexProj[512];
extern int g_glowMarkMeshOrdinal;
extern float g_glowMarkMeshRotationAngle;
extern float* g_glowMarkMeshVertexArray;
extern uint8_t* g_shieldGlowFrameScales;
extern uint8_t* g_hullLightFrameScales;
extern uint8_t* g_magElectFrameScales;
extern uint8_t* g_ionElectFrameScales;
extern uint8_t* g_blastMarkFrameScales;
extern uint8_t* g_tractorBeamFrameScales;
extern ObjectMeshTextureLayerBlock g_glowMarkPatchPool[24];
extern ObjectMeshTextureLayerBlock* g_glowMarkFreeList;
extern int g_glowMarkMaxActiveIndex;
extern ObjectMeshTextureLayerBlock* g_blastMarkFreeList;
extern ObjectMeshTextureLayerBlock g_blastMarkPatchPool[32];
extern int g_blastMarkMaxActiveIndex;
extern uint8_t* g_pSceneSpanDataCur;
extern uint8_t* g_pSceneSpanDataEnd;
extern int g_sceneSpanPtrMax;
extern int g_sceneSpanPtrAvail;
extern int g_phongSlotIndex;
extern int g_visFaceCount;
extern int g_visFaceDrawStartIndex;
extern int g_meshQueueIndex;
extern int g_phongSlotStride;
extern uint8_t* g_scenePhongData;
extern uint16_t g_scenePhongDataHandle;
extern int g_sw3dLightSampleBlockSize;
extern int g_sw3dLightSampleBlockShift;
extern int g_sw3dLightSampleBlockMask;
extern float g_renderUnitScale;
extern const char g_renderSoftwareRenderingMessage[];
extern const char g_renderCockpitMaskErrorMessage[];
extern float g_sw3dSpanLengthReciprocal[70];
extern int g_sw3dTextureShiftBySizeDiv16[60];
extern SceneFace* g_sw3dCurrentFace;
extern int g_sw3dCurrentScanY;
extern int g_sw3dScanlineByteOffset;
extern float g_sw3dLightSampleSubrowLerpT;
extern float g_sw3dLightSampleRowsToNextBlockFloat;
extern float g_sw3dLightSampleSubrowFloat;
extern int g_sw3dCurrentLightSampleCacheStamp;
extern int g_sw3dLightSampleCacheSceneStampBase;
extern uint16_t g_sw3dFpuControlWordScratch;
extern SceneFace g_sw3dCockpitMaskSentinelFace;
extern float g_sw3dCockpitMaskSentinelMaxX;
extern float g_sw3dCockpitMaskSentinelMaxY;
extern float g_sw3dCockpitMaskSentinelStartX;
extern float g_sw3dCockpitMaskSentinelEndX;
extern float g_sw3dCockpitMaskSentinelDepthZ;
extern uint32_t g_renderSceneVirtualProtectOldProtect;
extern SceneMesh* g_sw3dSpanSceneMesh;
extern float g_sw3dSpanTextureWidthFloat;
extern float g_sw3dSpanTextureHeightFloat;
extern int g_sw3dSpanTextureWidthShift;
extern int g_sw3dSpanTextureHeightShift;
extern uint8_t* g_sw3dSpanShadeTable;
extern uint8_t* g_sw3dSpanTexels;
extern int g_sw3dSpanTexelMask;
extern int g_sw3dSpanShadeDitherAccum;
extern int g_sw3dShadeDitherInitialByScanlineParity[2];
extern int g_sw3dSpanStartX;
extern int g_sw3dSpanLength;
extern Sw3dFloatInt g_sw3dSpanStepUQ8;
extern Sw3dFloatInt g_sw3dSpanStepVQ8;
extern Sw3dFloatInt g_sw3dSpanShadeQ8;
extern Sw3dFloatInt g_sw3dSpanShadeStepQ8;
extern Sw3dFloatInt g_sw3dSpanUQ8;
extern Sw3dFloatInt g_sw3dSpanVQ8;
extern const Sw3dFloatInt g_sw3dTexCoordBiasByShift[12];
extern const Sw3dFloatInt g_sw3dFloatToIntRoundBias;
extern const float g_sw3dOneFloat;
extern const float g_sw3dLightSampleInvBlockSize;
extern const float g_sw3dLightSampleBlockSizeFloat;
extern const float g_sw3dLightIntensityToShadeScale;
extern Vec3f g_meshEyePos;
extern float g_fixedCullDir[4];
extern const float g_meshRotationToRadians;
extern const float g_cockpitPanPositionScale;
extern const double g_q16AngleToRadians;
extern const float g_turretRotationToRadians;
extern uint8_t g_bBackdropMeshMode;
extern int g_curLayerId;
extern int g_faceIdCounter;
extern void* g_curTextureDesc;
/* XWA_MODERN: runtime palette pointer tracked alongside g_curTextureDesc. */
extern void* g_curTexturePalette;
extern intptr_t g_curMeshFlags;
extern intptr_t g_curVertNormals;
extern uint16_t g_curTextureId;
extern uint8_t g_bindMeshTextures;
extern ModelTextureOverrideSlot g_modelTextureOverrideSlots[32];
extern uint16_t g_modelTextureOverrideNextSlot;
extern int g_forcedLodLevel;
extern int g_cockpitViewActive;
extern float g_curRotAngle;
extern Vec3f* g_curRotScale;
extern Vec3f* g_unusedCockpitRotScaleType21Data;
extern Vec3f* g_unusedCockpitRotScaleType22Data;
extern int g_curMeshType;
extern int g_nodeSwitchIndex;
extern intptr_t g_modelNodeWalkUnusedScratch0;
extern intptr_t g_modelNodeWalkUnusedScratch1;
extern intptr_t g_modelNodeWalkUnusedScratch2;
extern int g_curVertexCount;
extern int g_bwingBridgeMeshIndexCache;
extern int g_approxDist;
extern int regionIdx;
extern CraftData* g_curCraft;
extern uint8_t g_filmPlaybackMode;
extern uint8_t g_filmOverlayActive;
extern uint8_t g_flightConfPowerVr;
extern int g_bilinearEnabled;
extern int g_flightSwRotSpriteSpanRunsEnabled;
extern int g_renderObjectListCount;
/* Resolution-mode scale shared by flight HUD geometry, layout, spacing,
 * reticles, gauges, and hardware font metrics. It is not a framebuffer scale. */
extern float g_flightHudScaleFactor;
extern float g_deathStarTunnelBillboardScale;
extern OptTexCoord g_defaultQuadTexCoords[4];
extern OptTexCoord g_mfdFrameQuadTexCoords[4];
extern OptTexCoord g_backdropStripTexCoords[4];
extern OptTexCoord* g_currentQuadTexCoords;
extern RenderObjectListEntry* g_renderObjectListEntries;
extern RenderObjectListEntry* g_renderListHead;
extern DeathStarTunnelLaserRegionState g_deathStarTunnelLaserRegions[5];
extern const int flareSpriteOrColor[6];
extern uint8_t g_meshClipCornerOutcodes[8];
extern float g_meshClipScreenX[8];
extern float g_meshClipScreenY[8];
extern Vec3f g_meshClipBoxCornersView[8];
extern float g_meshClipInvZ[8];
extern float g_invProjScale;
extern float g_depthProjScale;
extern float g_renderProjectionDepthOverrideZ;
extern float g_renderProjectionClampY0;
extern float g_renderProjectionClampY1;
extern float g_renderProjectionClampX0;
extern float g_renderProjectionClampX1;
extern int16_t g_renderProjectionYClampEnabled;
extern int g_hardpointOriginOffset[3];
extern int g_std3DZCmpMode;
extern Std3DZBufferSurfaceBlock g_std3DZBufferSurface;
extern uint8_t g_hwMipmapFilter;
extern int* g_clipIdxA;
extern int* g_clipIdxB;
extern int g_clipIdxAStorage[32];
extern int g_clipIdxBStorage[32];
extern int g_clipCountA;
extern int g_clipCountB;
extern int g_clipVertCursor;
extern int g_clipOccurred;
extern int g_clipInputProjVertEndIndex;
extern int g_unusedD3DMaxVertsClamped384;
extern float g_clipCurW;
extern float g_clipDeltaTu;
extern float g_clipDeltaTv;
extern float g_clipPrevColor0;
extern float g_clipCurColor0;
extern float g_clipPrevColor3;
extern float g_clipCurColor3;
extern float g_clipAttrPrev0[16];
extern float g_clipAttrCur0[16];
extern float g_clipPrevColor2;
extern float g_clipCurColor2;
extern float g_clipAttrPrev1[16];
extern float g_clipAttrCur1[16];
extern float g_clipScreenDenom;
extern float g_clipPrevColor1;
extern float g_clipCurColor1;
extern float g_clipPrevTu;
extern float g_clipCurTu;
extern float g_clipPrevTv;
extern float g_clipCurTv;
extern float g_clipAttrDelta0[16];
extern float g_clipDeltaColor0;
extern float g_clipDeltaColor3;
extern float g_clipDeltaColor2;
extern float g_clipDeltaColor1;
extern float g_clipDeltaSx;
extern float g_clipDeltaSy;
extern float g_clipNearDenom;
extern float g_clipPrevSx;
extern float g_clipCurSx;
extern float g_clipPrevSy;
extern float g_clipCurSy;
extern float g_clipPrevW;
extern float g_clipAttrDelta1[16];
extern Std3DDevice* g_pStd3DCurDevice;
extern int g_texCacheCount;
extern Std3DTexCacheNode* g_pTexCacheHead;
extern Std3DTexCacheNode* g_pTexCacheTail;
extern D3DInfoNode* g_d3dInfoFreeListHead;
extern D3DInfoNode* g_d3dInfoListHead;
extern int g_d3dInfoActiveCount;
extern D3DInfoNode g_d3dInfoPool[XWA_D3DINFO_POOL_COUNT];
#ifdef XWA_MODERN
extern uint32_t g_nextD3DInfoBridgeRefId;
#endif
extern int g_meshRenderFlags;
extern int16_t g_renderSceneForceDefaultTexture;
extern int g_d3dRenderStatePreset0_unused;
extern int g_d3dRenderStateUntexturedFace;
extern int g_d3dRenderStateTexturedMesh;
extern int g_d3dRenderStateDeferredAlphaMesh;
extern int g_d3dRenderStateMeshPass2;
extern int g_d3dRenderStateMultiTextureMesh;
extern int g_d3dRenderStateGlowQuad;
extern int g_d3dRenderStatePreset7_unused;
extern const int g_d3dRenderStateDefaultFlags[8];
extern RenderBatch* g_meshDeferredBatch;
extern RenderBatch* g_meshPass2Batch;
extern RenderBatch* g_meshMultiTexBatch;
extern FlightInitLineBufferFn g_flightInitLineBufferFn;
extern FlightDebugPrintFn g_flightDebugPrintFn;
extern FlightResetPaletteFn g_flightResetPaletteFn;
extern FlightSetPaletteRangeFn g_flightSetPaletteRangeFn;
extern FlightGetPaletteFn g_flightGetPaletteFn;
extern FlightSetPaletteFn g_flightSetPaletteFn;
extern RgbTriplet g_swPalette[256];
extern uint8_t g_paletteDirtyFlags;
extern FlightComputePixelOffsetFn g_flightComputePixelOffsetFn;
extern FlightBlitSpriteFn g_flightBlitSpriteFn;
extern FlightBlitSpriteFadedFn g_flightBlitSpriteFadedFn;
extern FlightDrawCharFn g_flightDrawCharFn;
extern FlightFillClipRectFn g_flightFillClipRectFn;
extern FlightFillRectClippedFn g_flightFillRectClippedFn;
extern FlightSaveScreenRectFn g_flightSaveScreenRectFn;
extern FlightRestoreScreenRectFn g_flightRestoreScreenRectFn;
extern FlightDrawPointArrayFn g_flightDrawPointArrayFn;
extern FlightDrawPointArrayMaskedFn g_flightDrawPointArrayMaskedFn;
extern FlightDrawPixelFn g_flightDrawPixelFn;
extern FlightDrawRadarTargetMarkerFn g_flightDrawRadarTargetMarkerFn;
extern FlightRestoreRadarTargetMarkerFn g_flightRestoreRadarTargetMarkerFn;
extern FlightDrawLineFn g_flightDrawLineFn;

int Renderer_FlushTextureCacheAndReturnTrue(void);
void Renderer_InitD3DDevice(IDirectDraw* pDD, IDirectDrawSurface* renderSurface);
void FVIEW_calcrotatemove(Q16Angle angleA, Q16Angle angleB, ObjectRecord* objRecord);
void FVIEW_transformaxes(int axisX_Q15, int axisY_Q15, int axisZ_Q15, int16_t angleQ16);
void FVIEW_calcrotateorient(Q16Angle angleC, Q16Angle angleD, ObjectRecord* objRecord);
int ComputeHardpointWorldPos(int playerIdx);
void FlightView_RenderCockpitModel(void);
char FlightView_Render(void);
void FlightView_RenderFrame(void);
int FVIEW_BuildCameraOrient(int16_t rollQ16, int16_t pitchQ16, int16_t yawQ16, int angle25Q16,
							int16_t extraPitchQ16, int16_t extraYawQ16, ObjectRecord* objRecord,
							int playerIdx);
void FVIEW_BuildCameraOrientNoTurret(int16_t rollQ16, int16_t pitchQ16, int16_t yawQ16, int angle25Q16,
									 int16_t extraPitchQ16, int16_t extraYawQ16, ObjectRecord* objRecord,
									 int playerIdx);
int FVIEW_ComputeObjectViewMatrix(void);
#ifdef XWA_MODERN
void FVIEW_CopyRenderCameraRows(float rows[9]);
void FVIEW_SaveRenderCameraStateForViewport(void);
void FVIEW_RestoreRenderCameraStateForViewport(void);
#endif
int FlightView_FinishCameraFocusTransition(int playerIdx, uint16_t transitionDuration);
void FlightView_ComputeObjectViewPosition(uint16_t objectIdx);
void USER_calcdeltapitch(int16_t pitchQ16, int16_t negYawQ16, uint16_t shipObjIdx);
void FlightView_RotateViewByInput(int pitchDeltaQ16, int negYawDeltaQ16, int playerIdx);
void FlightView_RotateFilmOverlayFreeCameraByInput(int pitchDeltaQ16, int negYawDeltaQ16, int playerIdx);
void FlightView_UpdatePlaybackCamera(int playerIdx);
void FlightView_UpdatePlayerCamera(int playerIdx);
void FlightView_RenderStartupFrame(void);
char FlightView_IsLensFlareSourceVisible(int worldX, int worldY, int worldZ);
int FlightView_IsObjectSphereVisible(int objectIdx, int sphereRadius);
ViewportCullFlags FlightView_CullObjectSphereToViewport(int objectIdx, int sphereRadius,
														int* projectedRadiusOut);
ViewportCullFlags FlightView_CullWorldSphereToViewport(int worldX, int worldY, int worldZ, int sphereRadius,
													   int* projectedRadiusOut);
int FVIEW_SetObjectTransform(Q16Angle angleC, Q16Angle angleA, Q16Angle angleB, Q16Angle angleD,
							 ObjectRecord* objRecord);
int TRANSFM2_CamMatDotRow0(int x, int y, int z);
int TRANSFM2_CamMatDotRow1(int x, int y, int z);
int TRANSFM2_CamMatDotRow2(int x, int y, int z);
float TRANSFM2_ViewTransformX(float x, float y, float z);
float TRANSFM2_ViewTransformY(float x, float y, float z);
float TRANSFM2_ViewTransformZ(float x, float y, float z);
uint32_t MATH2_longfraction(uint32_t value, uint16_t fracQ16);
uint16_t MATH2_fraction(uint16_t value, uint16_t fracQ16);
uint16_t MATH2_divide(uint16_t numerator, uint16_t denominator);
uint32_t MATH2_percentage(uint32_t numerator, uint32_t denominator);
uint32_t MATH2_mphconvert(int16_t speed, uint16_t divisor);
int32_t MATH2_ABoverC32(int32_t a, int32_t b, int32_t c);
void MATH2_getradarcoord(int relX, int relY, int relZ);
void TRANSFM2_clipobjecteyez(int x, int y, int z);
int TRANSFM2_ProjectScreenX(int viewX, int viewZ);
int TRANSFM2_ProjectStarfieldScreenX(int viewX, int viewZ);
int TRANSFM2_ProjectStarfieldScreenY(int viewY, int viewZ);
int TRANSFM2_ProjectScreenY(int viewY, int viewZ);
void pai_RotateVectorByExplicitAnglesScratch(int localSide, int localUp, int localFwd, Q16Angle yaw,
											 Q16Angle pitch, Q16Angle roll, Q16Angle angleD);
int pai_calcrotatedpoint(ObjectRecord* objRecord, int16_t sideArg, int16_t upArg, int16_t fwdArg);
int pai_RotateLocalVectorToWorldScratch(ObjectRecord* objRecord, int localSide, int localUp, int localFwd);
void pai_RotateLocalVectorToWorldScratchMaybeStatic(ObjectRecord* objRecord, int localSide, int localUp,
													int localFwd);
float Math3D_Dot3(const float* lhs, const float* rhs);
float Math3D_RotateVec3X(Vec3f* vec, Matrix3x3* matrix);
float Math3D_RotateVec3Y(Vec3f* vec, Matrix3x3* matrix);
float Math3D_RotateVec3Z(Vec3f* vec, Matrix3x3* matrix);
Matrix3x3* Math3D_MulMatrix3x3(Matrix3x3* dst, Matrix3x3* rhs);
Matrix3x3* Math3D_MulMatrix3x3T(Matrix3x3* dst, Matrix3x3* rhs);
Matrix3x3* Math3D_BuildAxisAngleMatrix(Matrix3x3* out, float* axisAngle);
void Math3D_RotateVec3(Vec3f* vec, Matrix3x3* matrix);
void RenderScene_ComputeVertexLighting(SceneMesh* mesh, ProjVertex* outVert, Vec3f* normal, Vec3f* pos,
									   Vec3f* eyePos);
int sw3d_ProjectMeshVerticesDistant(SceneMesh* mesh);
void RenderScene_TransformProjectVertices(SceneMesh* mesh);
SceneFace* RenderScene_TransformFaceTextureGradients(SceneFace* face,
													 const FaceTextureGradients* faceTexGradients,
													 const float* viewPosAndOrient);
int RenderScene_CullMeshFaces(SceneMesh* mesh);
int RenderScene_CullMeshFacesFromView(SceneMesh* mesh);
void GlowMark_AppendObjectMeshTextureLayers(SceneMesh* mesh, ObjectMeshTextureLayerBlock* layerBlocks);
void GlowMark_ClearPendingRequests(void);
ObjectMeshTextureLayerBlock* GlowMark_AllocShieldPatch(uint16_t sourceModelType, int unused);
ObjectMeshTextureLayerBlock* GlowMark_AllocAnimatedPatch(uint16_t effectParamIndex, uint16_t modelType);
void GlowMark_UpdateActivePatches(void);
void GlowMark_EvictOldestBlastMarkPatch(void);
void GlowMark_QueueRandomObjectSurfaceEffect(unsigned int objectIndex, int16_t effectModelType, float scale);
void GlowMark_QueueCraftDamageSurfaceEffects(uint16_t objectIndex);
void GlowMark_BeginMeshFacePatch(ObjectMeshTextureLayerBlock* patch);
void GlowMark_MarkFaceData(ObjectMeshTextureLayerBlock* patch, OptNode* faceDataNode);
void GlowMark_CollectModelFaces(ObjectMeshTextureLayerBlock* patch, OptimizedPolyObject* model,
								OptNode* node);
void GlowMark_InitFrameScalesAndPools(void);
void GlowMark_ShutdownFrameScalesAndPools(void);
char GlowMark_SpawnLocalPlayerHitEffects(void);
void GlowMark_ProcessPendingRequests(uint16_t objectIndex);
int sw3d_ProjectMeshVertices(SceneMesh* mesh);
void RenderScene_DrawMesh(SceneMesh* mesh);
void RenderScene_DrawSceneMesh(SceneMesh* mesh);
int RenderScene_DrawNodeMeshFaces(SceneMesh* mesh);
MeshClipFlags RenderScene_ComputeMeshDescriptorClipFlags(SceneMesh* mesh);
void RenderScene_DrawModelNode(OptimizedPolyObject* model, OptNode* node, SceneMesh* mesh);
void RenderScene_DrawModelNodeHardware(OptimizedPolyObject* model, OptNode* node, SceneMesh* mesh);
void RenderClip_ClipPolyTop(int prevVert, int curVert, ProjVertex* vertBuf);
void RenderClip_ClipPolyBottom(int prevVert, int curVert, ProjVertex* vertBuf);
void RenderClip_ClipPolyLeft(int prevVert, int curVert, ProjVertex* vertBuf);
void RenderClip_ClipPolyRight(int prevVert, int curVert, ProjVertex* vertBuf);
void RenderClip_ClipPolyNear(int prevVert, int curVert, ProjVertex* vertBuf);
int RenderScene_ClipAndEmitFace(SceneMesh* mesh);
int RenderScene_EmitFlightVertex(int vertIdx, ProjVertex* verts);
int RenderScene_EmitClippedFaceD3DVertex(int projVertexIdx, ProjVertex* projVerts);
int RenderScene_AppendProjectedVertexToBatch(RenderBatch* batch, int projVertIdx,
											 const ProjVertex* projVerts);
void RenderBatch_AllocMeshPassBatches(void);
void LensFlare_QueueSource(int argbColor);
RenderBatch* RenderScene_FlushGeometry(void);
RenderBatch* RenderScene_FlushDeferredMeshBatches(void);
void D3DInfo_InitPool(void);
D3DInfoNode* D3DInfo_CreateFromOptTexture(char* textureName, int textureId, OptTextureData* textureData,
										  void* pixelData, void* alphaData, uint16_t* brightPalette,
										  uint16_t* palette);
void D3DInfo_Release(D3DInfoNode* d3dInfo);
void std3D_AddToTextureCache(Std3DTextureSurface* surf);
void std3D_UncacheTextureSurface(Std3DTextureSurface* surf);
void std3D_UncacheTexture(Std3DTexCacheNode* node);
void std3D_FlushTextureCache(void);
void std3D_FreePalettes(void);
void std3D_StartScene(void);
void std3D_EndScene(void);
char std3D_LockExecuteBuffer(void);
char std3D_AddVertices(D3DTLVERTEX* verts, int count);
char std3D_BeginInstructions(void);
char std3D_AddTriangles(Std3DRenderTri* tris, unsigned int count);
char std3D_ExecuteBuffer(void);
void std3D_SetRenderState(Std3DRenderStateFlags flags);
int std3D_MapZCmpFunc(int zCmpCapsMask, int bPreferOrEqual);
void std3D_LockVBuffer(Std3DVBuffer* pVBuffer);
void std3D_UnlockVBuffer(Std3DVBuffer* pVBuffer);
void std3D_BlitVBuffer(Std3DVBuffer* pDst, Std3DVBuffer* pSrc, int dstX, int dstY, int srcX, int srcY);
void sw3d_RasterizeMeshFaces(SceneMesh* mesh);
char RenderScene_DrawVisibleFaces(void);
void sw3d_DrawVisibleFacesToSurface(void* surfacePixels, int surfacePitchBytes, unsigned int surfaceHeight);
void sw3d_DrawTexturedShadeSpan(int startX, int endX, float startViewZ);
char std3D_ClearZBuffer(void);
void* std3D_GetZBufferSurface(void);
int std3D_BuildViewportQuad(const Std3DViewportRect* rect);
void RenderScene_InitHardwareFrame(void);
void RenderScene_Initialize(int resetFlag);
char RenderScene_ClearFrameBuffers(void);
void RenderScene_SetDepthProjectionScale(float depthProjScale);
void RenderScene_ResetDepthProjectionScale(void);
void RenderScene_SetProjectionDepthOverride(float depthZ);
void RenderScene_EnableProjectionYClamp(float clampY0, float clampY1, float clampX0, float clampX1);
void RenderScene_DisableProjectionYClamp(void);
int SetFlightViewport(unsigned int viewportWidth, unsigned int viewportHeight, int unused,
					  unsigned int baseOffset);
unsigned int PushFlightViewport(unsigned int width, unsigned int height, int unused, unsigned int baseOffset);
int PopFlightViewport(void);
void FlightPalette_BuildRgbRange(RgbTriplet* srcRgb, RgbTriplet* dstRgb, int startIndex, int count);
void FlightPalette_Build16BppRange(RgbTriplet* srcRgb, uint16_t* dst16, int startIndex, int count);
char FlightPalette_Reset(void);
void FlightPalette_SetRange(RgbTriplet* rgbTriples, int startIdx, uint16_t count);
char* FlightPalette_GetFull(char* dst768);
void FlightPalette_SetFull(RgbTriplet* srcRgb);
int FlightSw_GetLineBufferAddr(int line);
int FlightSw_GetLinePitch(void);
int FlightSw_ComputePixelOffset(int x, int y);
int FlightSw_BlitSpriteRle(uint8_t* rleData, int16_t x, int16_t y, int endMarker, int mirror);
int FlightSw_BlitSpriteRleFaded(uint8_t* rleData, int16_t x, int16_t y, int endMarker, char paletteShift,
								int16_t fadeAmount);
uint8_t FlightText_DrawHardwareGlyph(uint8_t ch);
uint8_t FlightText_DrawSoftwareGlyph(uint8_t ch);
int FlightSw_FillClipRect(void);
int16_t FlightSw_FillRectClipped(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
								 uint16_t borderThickness);
int FlightSw_SaveScreenRect(int16_t* dst, int srcX, int srcY, int width, int height);
void FlightSw_RestoreScreenRect(const int16_t* src, int dstX, int dstY, int width, int height);
void FlightSw_DrawPointArray(uint16_t* points, uint16_t count);
uint16_t* FlightSw_DrawPointArrayMasked(uint16_t* points, uint16_t count);
int FlightSw_DrawPixel(uint16_t x, uint16_t y, char colorIdx);
void FlightSw_DrawRadarTargetMarker(void);
int FlightSw_RestoreRadarTargetMarker(void);
void FlightSw_DrawLine(int x1, int y1, int x2, int y2, uint8_t colorIdx);
void Renderer_InitD3DRenderStatePresets(void);
void Renderer_InitFrontendHardwareSettings(void);
void Renderer_InitHardwareMaxQualitySettings(void);
char std3D_SetMipmapFilter(char filterType);
int std3D_GetPal8Available(void);
int16_t Renderer_IsTextureClampSupported(void);
int16_t Renderer_CanUseBilinearFiltering(void);
void D3DInfo_ReleaseAll(void);
void std3D_Close(int checkForLeakedTextures);
void std3D_Shutdown(void);
int16_t RenderScene_DrawObjectModel(ObjectRecord* obj);
int RenderScene_DrawObjectModelHardware(ObjectRecord* obj);
void RenderScene_DrawNoAssetSourceModel(ObjectRecord* obj, int nodeSwitchIndex);
void RenderNonCraftSceneObject(uint16_t objectIndex);
void EngineGlow_RenderForObject(unsigned int objIdx);
void EngineGlow_RenderSceneGlows(void);
GlowMarkRequest* GlowMark_QueueRequest(uint16_t objectIndex, int16_t effectParam, int16_t modelType,
									   float scaleU, float scaleV);
void GlowMark_CreateEngineKnockoutBlastMark(unsigned int objectIndex, uint8_t emitterIndex);
void GlowMark_ClearEngineKnockoutBlastMarks(unsigned int objectIndex);
void Backdrop_DrawModelTexQuadAtScreen(ObjectTypeId modelType, int screenX, int screenY, int16_t angle,
									   int16_t screenScale);
void Backdrop_ProjectAndDrawScreenQuad(int viewX, int viewY, int viewZ, int16_t angle, ObjectTypeId modelType,
									   int16_t screenScale);
void Backdrop_DrawHardwareAxisQuad(ObjectTypeId modelType, int viewX, int viewY, int viewZ,
								   int halfWidthAxisX, int halfWidthAxisY, int halfWidthAxisZ,
								   int halfHeightAxisX, int halfHeightAxisY, int halfHeightAxisZ,
								   uint16_t screenScale);
void Backdrop_RenderCurrentRegion(void);
void Backdrop_BuildCoordinateBuffers(void);
void Backdrop_FreeCoordinateBuffers(void);
char RenderQuad_DrawModelTexture(ObjectTypeId modelType, FlightTexQuad* quad, int argbColor);
int FlightSw_PrepareSpriteRotationTables(int16_t rotationAngle, ...);
char* FlightSw_BuildSpriteTintRemapTables(Sprite* sprite);
char FlightSw_DrawRotatedSpriteQuad(int16_t screenX, int16_t screenY, int16_t screenSize, Sprite* sprite);
void RenderBillboard_DrawRollAlignedObjectModel(uint16_t objectIndex);
void SceneBillboard_QueueObjectTextured(uint16_t objectIndex);
int SceneBillboard_ComputeProjectedSize(int depthZ, uint16_t modelMaxExtent, int baseScreenSize,
										int clampTo1024);
int DeathStar_ComputeScaledProjectedBillboardSize(int depthZ, unsigned int modelMaxExtent,
												  uint16_t baseScreenSize);
void DeathStar_CacheModelNodeTextures(OptNode* node, int firstFrame, int lastFrame);
void DeathStar_PreloadSegmentObjectTextures(ObjectRecord* object, int firstFrame, int lastFrame);
void RenderBillboard_DrawSpriteFacingCamera(ParticleEffect* effect, unsigned int renderFlags);
void RenderBillboard_DrawObjectTrail(ObjectTrailEmitter* trail);
void RenderBillboard_DrawOriented(ParticleEffect* effect, unsigned int renderFlags);
void RenderBillboard_DrawOrientedStretched(ParticleEffect* effect, unsigned int renderFlags);
void RenderBillboard_DrawStretched(ParticleEffect* effect, unsigned int renderFlags);
void SceneBillboard_QueueProjectedTextured(int objectOrTypeIndex, int frame, int screenSize, int screenX,
										   int screenY, int depthZ, int rotationAngle);
void SceneBillboard_RenderQueuedTextured(int drawTargetComponentMarkers);
char RenderScene_EffectsPass(void);
void RenderScene_End3D(void);
int RenderScene_ProjectPreviewWireframeModel(ObjectRecord* obj);
void EngineGlow_BuildRectQuadCorners(const OptEngineGlow* glow, int* viewQuad, float scale,
									 const int* unusedNormal, const int* upAxisView,
									 const int* rightAxisView);
void EngineGlow_ExtrudeQuadAlongViewNormal(int* viewQuad, const int* lookAxisView, int depthScaleQ15);
int16_t EngineGlow_BuildProjectedQuad(unsigned int objIdx, const OptEngineGlow* glow, unsigned char meshIdx,
									  int* outViewQuad, float scale);
int EngineGlow_AdjustSuperStarDestroyerDepth(int objIdx, int sortDepth, const OptEngineGlow* glow);
void RenderQuad_SubmitClippedTriangle(ProjVertex* verts3, Std3DTexCacheNode* cacheNode, float depthZ,
									  int nearFlag);
void RenderQuad_DrawTextured3D(const int* corners, TexLevel* texLevel, int d3dFlags);
int RenderQuad_DrawRotatedSprite(FlightTexQuad* quad, TexLevel* texLevel, int d3dFlags);
void RenderQuad_DrawGlow(int* corners5, int depthZ, Std3DTextureSurface* tex, int edgeColor, int centerColor);
void* sw3d_AllocSceneBuffers(void);
void sw3d_InitSceneBuffers(void);
void sw3d_AllocSceneModelLists(void);
uint16_t* std3D_CopyPaletteToScratch16(const uint16_t* palette, int colorCount);
uint16_t* std3D_ConvertTexTo1555(uint16_t* pSrc565, uint16_t colorKey, int count);
uint16_t* std3D_ConvertTexTo4444(uint16_t* pSrc565, const uint8_t* pAlpha, int count);

#ifdef __cplusplus
}
#endif

#endif
