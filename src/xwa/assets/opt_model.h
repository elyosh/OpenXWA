#ifndef XWA_ASSETS_OPT_MODEL_H
#define XWA_ASSETS_OPT_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OptNodeType {
	OPT_NULL = -1,
	OPT_GROUP = 0,
	OPT_FACEDATA = 1,
	OPT_TYPE_2 = 2,
	OPT_MESHVERTS = 3,
	OPT_TYPE_4 = 4,
	OPT_TYPE_5 = 5,
	OPT_TYPE_6 = 6,
	OPT_NODEREF = 7,
	OPT_TYPE_8 = 8,
	OPT_TYPE_9 = 9,
	OPT_VERTNORMALS = 11,
	OPT_TYPE_12 = 12,
	OPT_TEXCOORDS = 13,
	OPT_TYPE_14 = 14,
	OPT_TEXTURE = 20,
	OPT_FACEGROUP = 21,
	OPT_HARDPOINT = 22,
	OPT_ROTSCALE = 23,
	OPT_NODESWITCH = 24,
	OPT_MESHDESC = 25,
	OPT_TEXALPHA = 26,
	OPT_TEXTURE_REF = 27,
	OPT_ENGINEGLOW = 28,
	/* Game-internal node types in the IDB; meaning still unverified. */
	OPT_TYPE_10 = 10,
	OPT_TYPE_18 = 18,
	OPT_TYPE_19 = 19,
	/* FaceData variants in the IDB. */
	OPT_FACEDATA_15 = 15,
	OPT_FACEDATA_16 = 16,
	OPT_FACEDATA_17 = 17,
} OptNodeType;

typedef struct OptNode {
	char* pName;
	OptNodeType nodeType;
	int childCount;
	struct OptNode** pChildren;
	intptr_t param1;
	void* param2;
} OptNode;

typedef struct OptimizedPolyObject {
	void* selfMarker;
	uint16_t reserved;
	int rootNodeCount;
	OptNode** rootNodes;
} OptimizedPolyObject;

typedef struct OptTextureData {
	uint32_t paletteAddress;
	int32_t paletteType;
	int32_t textureSize;
	int32_t dataSize;
	int32_t width;
	int32_t height;
} OptTextureData;

#define XWA_LOADED_MODEL_COUNT 557

typedef union LoadedModelHandleTable {
	uint16_t byObjectType[XWA_LOADED_MODEL_COUNT];
} LoadedModelHandleTable;

extern LoadedModelHandleTable g_loadedModels;
extern uint16_t g_cockpitModel;
extern uint16_t g_exteriorModel;

void OptModel_AdjustOptimizedNodePointers(OptNode* node, intptr_t base);
unsigned int OptModel_GetSerializedNodeSize(OptNode* node, int* parentState, int texturePageCount);
int OptModel_ComputeProcessedModelSize(OptimizedPolyObject* model, int texturePageCount);
void OptModel_AdjustNodePointersRecursive(OptNode* node, intptr_t base, const void* threshold,
										  intptr_t delta);
void OptModel_AdjustOptimizedPolyObjectPatchPointers(OptimizedPolyObject* model, const void* threshold,
													 int delta);
uint16_t OptModel_LoadFileToHandle(const char* fileName, int* outVersion);
void OptModel_DeleteBytes(OptimizedPolyObject* model, int modelSize, void* cutPoint, int cutSize);
OptNode* OptModel_FindNodeByName(OptNode* node, const char* name);
OptNode* OptModel_ResolveNodeRef(OptNode* refNode, OptimizedPolyObject* model);
int OptModel_FreeHandle(uint16_t modelHandle);
uint16_t OptModel_LoadHandle(const char* fileName);
void D3DInfo_InitPool(void);
void OptModel_AdjustOptimizedPolyObjectPointers(OptimizedPolyObject* model);
void OptModel_ScaleTexturePaletteBrightness16Bpp(float brightnessScale, const uint16_t* srcPalette,
												 uint16_t* dstPalette, int entryCount, int isRgb555);
void OptModel_PrepareTexturePalette(uint16_t* palette);
int OptModel_PrepareTextures(OptNode* node, OptimizedPolyObject* model, int modelSize);
int OptModel_ReplaceTextureNodesWithRefsRecursive(OptNode* node, OptimizedPolyObject* model,
												  const intptr_t* textureIds, int textureRefIndex,
												  int* modelSize);
void OptModel_ResolveTextureRefsRecursive(OptNode* node, OptimizedPolyObject* model, int* modelSize);
int OptModel_BuildHardwareData(OptimizedPolyObject* model, int modelSize);
void OptModel_ReleaseD3DInfoRecursive(OptNode* node);

#ifdef __cplusplus
}
#endif

#endif
