#include "xwa/frontend/model_preview.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif

#include "xwa/assets/file_io.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_texture.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight_light.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/util/memory.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// GLOBAL: XWA 0x7825F8
ObjectRecord g_modelPreviewObject;
// GLOBAL: XWA 0x782790
uint16_t g_modelPreviewAngleD;
// GLOBAL: XWA 0x781F18
float g_modelPreviewBoundsMaxZ;
// GLOBAL: XWA 0x781F1C
float g_modelPreviewBoundsMinZ;
// GLOBAL: XWA 0x781F20
float g_modelPreviewBoundsMaxX;
// GLOBAL: XWA 0x781F28
float g_modelPreviewBoundsMinX;
// GLOBAL: XWA 0x781F38
float g_modelPreviewBoundsMaxY;
// GLOBAL: XWA 0x7821E0
float g_modelPreviewBoundsMinY;
// GLOBAL: XWA 0x7827A0
double g_modelPreviewUnscaledMaxExtent;
// GLOBAL: XWA 0x7825E8
int g_modelPreviewTexture237Loaded;
// GLOBAL: XWA 0x7825F0
double g_modelPreviewScaleFactor;
// GLOBAL: XWA 0x781E98
char g_modelPreviewOptFileName[128];
// GLOBAL: XWA 0x782628
MobileObject g_modelPreviewMobileObject;
// GLOBAL: XWA 0x7821E8
CraftData g_modelPreviewCraftData;
// GLOBAL: XWA 0x782794
OptimizedPolyObject* g_modelPreviewOpt;
extern const float flt_5AA080;
extern const float flt_5AA088;
extern const double g_degreesToQ16Scale;
// GLOBAL: XWA 0x5AA030
const double g_modelPreviewBaseExtent = 500.0;
// GLOBAL: XWA 0x5AA038
const double g_modelPreviewLowAspectRatio = 2.0;
// GLOBAL: XWA 0x5AA040
const double g_modelPreviewLowAspectScale = 1.5;
// GLOBAL: XWA 0x5AA048
const double g_modelPreviewHighAspectRatio = 5.0;
// GLOBAL: XWA 0x5AA050
const double g_modelPreviewMediumAspectScale = 1.8;
// GLOBAL: XWA 0x5AA058
const double g_modelPreviewHighAspectScale = 2.1;
// GLOBAL: XWA 0x5AA060
const double g_modelPreviewDefaultLightLengthSquared = 3.0;
// GLOBAL: XWA 0x5AA068
const double g_modelPreviewInvLengthNumerator = 1.0;
// GLOBAL: XWA 0x5AA070
const double g_modelPreviewQ15Scale = 32767.0;
// GLOBAL: XWA 0x5AA078
const double g_modelPreviewNegativeOne = -1.0;
// GLOBAL: XWA 0x5AA098
const double g_modelPreviewMetersScale = 1600.0;
// GLOBAL: XWA 0x5AA0A0
const double g_modelPreviewQ16Scale = 0.0000152587890625;

// FUNCTION: XWA 0x50F700
void ModelPreview_ScaleOptNodeTree(OptNode* node, OptimizedPolyObject* opt, double scale) {
	OptNode* resolvedNode;
	int i;

	resolvedNode = OptModel_ResolveNodeRef(node, opt);
	if (!resolvedNode) {
		return;
	}

	switch (resolvedNode->nodeType) {
		case OPT_FACEGROUP: {
			int count;
			float* childScales;

			count = resolvedNode->childCount;
			childScales = (float*)resolvedNode->param2;
			if (count > 0) {
				do {
					*childScales = (float)(*childScales / scale);
					++childScales;
					--count;
				} while (count);
			}
			break;
		}
		case OPT_MESHVERTS: {
			int count;
			float* vertices;

			count = (int)resolvedNode->param1;
			vertices = (float*)resolvedNode->param2;
			if (count > 0) {
				do {
					vertices[0] = (float)(vertices[0] * scale);
					vertices[1] = (float)(vertices[1] * scale);
					vertices[2] = (float)(vertices[2] * scale);
					vertices += 3;
					--count;
				} while (count);
			}
			break;
		}
		case OPT_FACEDATA: {
			int count;
			float* points;

			count = (int)resolvedNode->param1;
			points = (float*)((uint8_t*)resolvedNode->param2 + 76 * count + 4);
			if (count > 0) {
				do {
					points[0] = (float)(points[0] * scale);
					points[1] = (float)(points[1] * scale);
					points[2] = (float)(points[2] * scale);
					points += 3;
					points[0] = (float)(points[0] * scale);
					points[1] = (float)(points[1] * scale);
					points[2] = (float)(points[2] * scale);
					points += 3;
					--count;
				} while (count);
			}
			break;
		}
	}

	for (i = 0; i < resolvedNode->childCount; ++i) {
		ModelPreview_ScaleOptNodeTree(resolvedNode->pChildren[i], opt, scale);
	}
}

// FUNCTION: XWA 0x50F820
void ModelPreview_AccumulateOptNodeBounds(OptNode* node, OptimizedPolyObject* opt) {
	OptNode* resolvedNode;
	int i;

	resolvedNode = OptModel_ResolveNodeRef(node, opt);
	if (!resolvedNode) {
		return;
	}

	if (resolvedNode->nodeType == OPT_MESHVERTS) {
		float* vertices;
		int count;

		vertices = (float*)resolvedNode->param2;
		if (resolvedNode->param1 > 0) {
			count = (int)resolvedNode->param1;
			do {
				if (g_modelPreviewBoundsMaxX < vertices[0]) {
					g_modelPreviewBoundsMaxX = vertices[0];
				}
				if (g_modelPreviewBoundsMinX > vertices[0]) {
					g_modelPreviewBoundsMinX = vertices[0];
				}
				if (g_modelPreviewBoundsMaxY < vertices[1]) {
					g_modelPreviewBoundsMaxY = vertices[1];
				}
				if (g_modelPreviewBoundsMinY > vertices[1]) {
					g_modelPreviewBoundsMinY = vertices[1];
				}
				if (g_modelPreviewBoundsMaxZ < vertices[2]) {
					g_modelPreviewBoundsMaxZ = vertices[2];
				}
				if (g_modelPreviewBoundsMinZ > vertices[2]) {
					g_modelPreviewBoundsMinZ = vertices[2];
				}
				vertices += 3;
				--count;
			} while (count);
		}
	}

	for (i = 0; i < resolvedNode->childCount; ++i) {
		ModelPreview_AccumulateOptNodeBounds(resolvedNode->pChildren[i], opt);
	}
}

// FUNCTION: XWA 0x50F920
double ModelPreview_ComputeOptBoundsExtent(OptimizedPolyObject* opt, int axisSelector) {
	double result;
	int i;

	g_modelPreviewBoundsMaxX = 0.0f;
	g_modelPreviewBoundsMinX = 0.0f;
	g_modelPreviewBoundsMaxY = 0.0f;
	g_modelPreviewBoundsMinY = 0.0f;
	g_modelPreviewBoundsMaxZ = 0.0f;
	g_modelPreviewBoundsMinZ = 0.0f;

	for (i = 0; i < opt->rootNodeCount; ++i) {
		ModelPreview_AccumulateOptNodeBounds(opt->rootNodes[i], opt);
	}

	g_modelPreviewBoundsMaxX = g_modelPreviewBoundsMaxX - g_modelPreviewBoundsMinX;
	g_modelPreviewBoundsMaxY = g_modelPreviewBoundsMaxY - g_modelPreviewBoundsMinY;
	g_modelPreviewBoundsMaxZ = g_modelPreviewBoundsMaxZ - g_modelPreviewBoundsMinZ;

	if (axisSelector == 0) {
		if (g_modelPreviewBoundsMaxX > g_modelPreviewBoundsMaxY &&
			g_modelPreviewBoundsMaxX > g_modelPreviewBoundsMaxZ) {
			return g_modelPreviewBoundsMaxX;
		}
		if (g_modelPreviewBoundsMaxY > g_modelPreviewBoundsMaxX &&
			g_modelPreviewBoundsMaxY > g_modelPreviewBoundsMaxZ) {
			return g_modelPreviewBoundsMaxY;
		}
		return g_modelPreviewBoundsMaxZ;
	}

#ifdef XWA_MODERN
	result = g_modelPreviewBoundsMaxX;
#endif
	if (axisSelector == 1) {
		result = g_modelPreviewBoundsMaxX;
	}
	if (axisSelector == 2) {
		result = g_modelPreviewBoundsMaxY;
	}
	if (axisSelector == 3) {
		result = g_modelPreviewBoundsMaxZ;
	}

	return result;
}

// FUNCTION: XWA 0x50EC00
int ModelPreview_FreeResources(void) {
	int savedUseHardware3D;

	if (g_modelPreviewTexture237Loaded) {
		if (g_frontendD3DInitialized) {
			savedUseHardware3D = g_useHardware3D;
			g_useHardware3D = 1;
			FeDiskIo_FreeTexturesForType(237);
			g_useHardware3D = savedUseHardware3D;
			g_modelPreviewTexture237Loaded = 0;
		}
	}
	if (g_loadedModels.byObjectType[0]) {
		OptModel_FreeHandle(g_loadedModels.byObjectType[0]);
		g_loadedModels.byObjectType[0] = 0;
	}
	return 1;
}

// FUNCTION: XWA 0x50FC50
int ModelPreview_FreeTexture237(void) {
	int savedUseHardware3D;
	if (g_useHardware3D && g_frontendD3DInitialized) {
		Renderer_FlushTextureCacheAndReturnTrue();
	}
	if (g_modelPreviewTexture237Loaded && g_frontendD3DInitialized) {
		savedUseHardware3D = g_useHardware3D;
		g_useHardware3D = 1;
		FeDiskIo_FreeTexturesForType(237);
		g_useHardware3D = savedUseHardware3D;
		g_modelPreviewTexture237Loaded = 0;
	}
	return 1;
}

// FUNCTION: XWA 0x50FBE0
int ModelPreview_LoadTexture237(void) {
	int savedUseHardware3D;

	if (g_modelPreviewTexture237Loaded) {
		return 0;
	}
	if (!g_frontendD3DInitialized) {
		return 0;
	}
	savedUseHardware3D = g_useHardware3D;
	g_useHardware3D = 1;
	g_loadingModel = 1;
	FeDiskIo_LoadTexturesForType(237);
	g_useHardware3D = savedUseHardware3D;
	g_loadingModel = 0;
	g_modelPreviewTexture237Loaded = 1;
	return 1;
}

// FUNCTION: XWA 0x50E5C0
char ModelPreview_LoadModel(const char* modelName, int objectType) {
	char baseName[256];
	char fileName[256];
	char* dot;
	XwaFile* stream;
	uint16_t handle;
	double maxExtent;
	double xExtent;
	double xyToZRatio;
	double lightX;
	double lightY;
	double lightZ;
	int i;
	OptimizedPolyObject* opt;
	int rootNodeCount;
	int savedUseHardware3D;

	if (g_modelPreviewTexture237Loaded && g_frontendD3DInitialized) {
		savedUseHardware3D = g_useHardware3D;
		g_useHardware3D = 1;
		FeDiskIo_FreeTexturesForType(237);
		g_useHardware3D = savedUseHardware3D;
		g_modelPreviewTexture237Loaded = 0;
	}
	if (!g_modelPreviewTexture237Loaded && g_frontendD3DInitialized) {
		savedUseHardware3D = g_useHardware3D;
		g_useHardware3D = 1;
		g_loadingModel = 1;
		FeDiskIo_LoadTexturesForType(237);
		g_loadingModel = 0;
		g_useHardware3D = savedUseHardware3D;
		g_modelPreviewTexture237Loaded = 1;
	}

	g_projOffsetY = 0;
	g_projOffsetYf = 0.0f;
	g_lodDistanceScale = 1.0f;
	g_players[g_localPlayer].viewState.savedTargetX = 0;
	g_players[g_localPlayer].viewState.savedTargetY = -1280;
	g_players[g_localPlayer].viewState.savedTargetZ = 0;
	g_players[g_localPlayer].viewState.viewRoll = 0;
	g_players[g_localPlayer].viewState.viewPitch = 0x4000;
	g_players[g_localPlayer].viewState.viewYaw = 0;
	g_mipLodScale = 1.0f;
	g_localLightsLevel = 1;
	g_specularEnabled = 1;
	g_keepFullResTextures = 1;
	g_dirLightingEnabled = 1;

	if (!g_modelPreviewOpt) {
		g_loadedModels.byObjectType[0] = 0;
	}

	strcpy(baseName, modelName);
	dot = baseName;
	if (*dot != '.') {
		do {
			++dot;
		} while (*dot != '.');
	}
	*dot = '\0';

	strcpy(fileName, baseName);
	strcat(fileName, ".opt");
#ifdef XWA_MODERN
	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
#else
	File_OpenGlobalStream(fileName, "rb", 0, 0);
	stream = g_stream;
#endif
	if (g_useHardware3D && g_frontendD3DInitialized) {
		Renderer_FlushTextureCacheAndReturnTrue();
	}
	if (!stream) {
		return 0;
	}
#ifdef XWA_MODERN
	File_Close(stream);
#else
	fclose((FILE*)stream);
#endif

	g_loadingModel = 1;
	if (g_loadedModels.byObjectType[0]) {
		OptModel_FreeHandle(g_loadedModels.byObjectType[0]);
		g_loadedModels.byObjectType[0] = 0;
	}
	handle = OptModel_LoadHandle(fileName);
	if (!handle) {
		g_loadingModel = 0;
		return 0;
	}
	g_loadedModels.byObjectType[0] = handle;
	strcpy(g_modelPreviewOptFileName, baseName);
	strcat(g_modelPreviewOptFileName, ".opt");

	g_modelPreviewModelType = g_objectTypeTables.craftTypeToObjectType[objectType];
	g_modelPreviewOpt = (OptimizedPolyObject*)Memory_LockHandle(handle);
	if (!g_modelPreviewOpt) {
		OptModel_FreeHandle(handle);
		g_loadedModels.byObjectType[0] = 0;
		g_loadingModel = 0;
		return 0;
	}
	OptModel_AdjustOptimizedPolyObjectPointers(g_modelPreviewOpt);

	maxExtent = ModelPreview_ComputeOptBoundsExtent(g_modelPreviewOpt, 0);
	g_modelPreviewUnscaledMaxExtent = maxExtent;
	xExtent = ModelPreview_ComputeOptBoundsExtent(g_modelPreviewOpt, 1);
	{
		double yExtent;
		double zExtent;

		yExtent = ModelPreview_ComputeOptBoundsExtent(g_modelPreviewOpt, 2);
		zExtent = ModelPreview_ComputeOptBoundsExtent(g_modelPreviewOpt, 3);
		if (yExtent > xExtent) {
			xExtent = yExtent;
		}
		xyToZRatio = xExtent / zExtent;
	}
	{
		double scale;

		g_modelPreviewScaleFactor = g_modelPreviewBaseExtent / maxExtent;
		if (xyToZRatio < g_modelPreviewLowAspectRatio) {
			scale = g_modelPreviewScaleFactor;
			scale *= g_modelPreviewLowAspectScale;
		} else if (xyToZRatio < g_modelPreviewHighAspectRatio) {
			scale = g_modelPreviewScaleFactor;
			scale *= g_modelPreviewMediumAspectScale;
		} else {
			scale = g_modelPreviewScaleFactor;
			scale *= g_modelPreviewHighAspectScale;
		}
		scale = (g_modelPreviewScaleFactor = scale);
		opt = g_modelPreviewOpt;
		for (i = 0; i < opt->rootNodeCount; ++i) {
			ModelPreview_ScaleOptNodeTree(opt->rootNodes[i], opt, scale);
		}
	}

	ModelBounds_ClearCache();
	FeDiskIo_BuildModelDef((uint16_t)g_modelTypeTable[g_modelPreviewModelType].modelIndex,
						   (uint16_t)g_modelPreviewModelType);

	memset(&g_modelPreviewMobileObject, 0, sizeof(g_modelPreviewMobileObject));
	g_modelPreviewObject.objectType = OBJ_None;
	memset(&g_modelPreviewCraftData, 0, sizeof(g_modelPreviewCraftData));
	g_modelPreviewObject.mobj = &g_modelPreviewMobileObject;
	g_modelPreviewMobileObject.pCraft = &g_modelPreviewCraftData;
	g_modelPreviewObject.world_x = 0;
	g_modelPreviewObject.world_y = 0;
	g_modelPreviewObject.world_z = 0;
	g_modelPreviewObject.pitch = 0;
	g_modelPreviewObject.yaw = 0;
	g_modelPreviewObject.roll = 0;
	g_modelPreviewAngleD = 0;

	g_projOffsetY = 0;
	g_projOffsetYf = 0.0f;
	g_lodDistanceScale = 1.0f;
	g_mipLodScale = 1.0f;
	g_localLightsLevel = 1;
	g_specularEnabled = 1;
	g_keepFullResTextures = 1;
	g_dirLightingEnabled = 1;
	g_players[g_localPlayer].viewState.savedTargetX = 0;
	g_players[g_localPlayer].viewState.savedTargetY = -1280;
	g_players[g_localPlayer].viewState.savedTargetZ = 0;
	g_players[g_localPlayer].viewState.viewRoll = 0;
	g_players[g_localPlayer].viewState.viewPitch = 0x4000;
	g_players[g_localPlayer].viewState.viewYaw = 0;

	lightY = g_modelPreviewInvLengthNumerator / sqrt(g_modelPreviewDefaultLightLengthSquared);
	lightZ = g_modelPreviewInvLengthNumerator / sqrt(g_modelPreviewDefaultLightLengthSquared);
	lightX = g_modelPreviewInvLengthNumerator / sqrt(g_modelPreviewDefaultLightLengthSquared);
	FlightLight_ClearDirectionalLights();
	FlightLight_AddDirectionalLight((int)(lightX * g_modelPreviewQ15Scale),
									(int)(lightY * g_modelPreviewNegativeOne * g_modelPreviewQ15Scale),
									(int)(lightZ * g_modelPreviewQ15Scale), 0.80000001f, 1.0f, 1.0f, 1.0f);

	if (g_frontendD3DInitialized && g_modelTypeTable[g_modelPreviewModelType].modelIndex != -1) {
		OptNode* firstRootNode;

		opt = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[0]);
		OptModel_AdjustOptimizedPolyObjectPointers(opt);
		rootNodeCount = opt->rootNodeCount;
		firstRootNode = opt->rootNodes[0];
		if (firstRootNode->nodeType == OPT_TEXTURE || firstRootNode->nodeType == OPT_TEXTURE_REF) {
			--rootNodeCount;
		}
		Memory_UnlockHandle(g_loadedModels.byObjectType[0]);
		if (rootNodeCount > 50) {
			rootNodeCount = 50;
		}
		for (i = 0; i < rootNodeCount; ++i) {
			int glowIndex;
			int glowCount;

			glowCount = ModelMesh_CountEngineGlows(g_modelPreviewModelType, i);
			glowIndex = 0;
			glowCount &= 0xffff;
			for (; glowIndex < glowCount; ++glowIndex) {
				OptEngineGlow* engineGlowParam;
				float* glowValue;

				engineGlowParam = ModelMesh_GetEngineGlowParam(g_modelPreviewModelType, i, glowIndex);
				glowValue = &engineGlowParam->position.x;
				*glowValue *= g_modelPreviewScaleFactor;
				glowValue = &engineGlowParam->position.y;
				*glowValue *= g_modelPreviewScaleFactor;
				glowValue = &engineGlowParam->position.z;
				*glowValue *= g_modelPreviewScaleFactor;
				glowValue = &engineGlowParam->dimensions.x;
				*glowValue *= g_modelPreviewScaleFactor;
				glowValue = &engineGlowParam->dimensions.y;
				*glowValue *= g_modelPreviewScaleFactor;
			}
		}
	}

	g_loadingModel = 0;
	return 1;
}

// FUNCTION: XWA 0x50EC70
int ModelPreview_RenderViewport(int x, int y, int width, int height, void* softwareSurface, int softwarePitch,
								unsigned int softwareHeight) {
	int savedLocalLightsLevel;
	int savedUseHardware3D;
	double viewMtx00;
	double viewMtx10;
	double viewMtx20;
	double viewMtx01;
	double viewMtx11;
	double viewMtx21;
	double viewMtx02;
	double viewMtx12;
	double viewMtx22;
	float objMatR0X;
	float objMatR0Y;
	float objMatR0Z;
	float objMatR1X;
	float objMatR1Y;
	float objMatR1Z;
	float objMatR2X;
	float objMatR2Y;
	float objMatR2Z;

	if (!g_loadedModels.byObjectType[0]) {
		return 0;
	}

	if (x < 0) {
		width += x;
		x = 0;
	}
	if (y < 0) {
		height += y;
	}
	if (x >= 1600) {
		return 0;
	}
	if (y >= 1200) {
		return 0;
	}
	if (height + y > 1200) {
		height = 1200 - y;
	}
	if (width + x > 1600) {
		width = 1600 - x;
	}

	g_flightVpWidth = width;
	g_flightVpMaxX = width - 1;
	g_flightVpCenterX = width / 2;
	g_flightVpProjScaleX = (int)((g_flightVpCenterXf = (float)g_flightVpCenterX) * flt_5AA080);
	g_flightVpMaxY = height - 1;
	g_flightVpHeight = height;
	g_flightVpCenterY = height / 2;
	g_flightVpY = y;
	g_flightVpX = x;
	g_flightVpCenterYf = (float)(uint16_t)g_flightVpCenterY;
	g_surfacePitch = FrontendDisplay_GetFrontendOrFlightDrawPitch();
	g_flightVpBaseOffset = x + g_surfacePitch * y;
	g_screenHeight = 480;
	g_projScaleInt = 512;
	g_projScaleHalfInt = 256;
	perspShift = 9;
	g_projAspectY = 0;
	g_projScale = 512.0f;
	g_sceneBillboardQueueCount = 0;

	FVIEW_BuildCameraOrient(g_players[g_localPlayer].viewState.viewRoll,
							g_players[g_localPlayer].viewState.viewPitch,
							g_players[g_localPlayer].viewState.viewYaw, 0, 0, 0, NULL, -1);
	FVIEW_SetObjectTransform(g_modelPreviewObject.roll, g_modelPreviewObject.pitch, g_modelPreviewObject.yaw,
							 g_modelPreviewAngleD, 0);

	g_modelPreviewViewDelta.x =
		(float)(g_modelPreviewObject.world_x - g_players[g_localPlayer].viewState.savedTargetX);

	g_modelPreviewViewDelta.y =
		(float)(g_modelPreviewObject.world_y - g_players[g_localPlayer].viewState.savedTargetY);
	g_modelPreviewViewDelta.z =
		(float)(g_modelPreviewObject.world_z - g_players[g_localPlayer].viewState.savedTargetZ);

	mat.m[0] = (float)(viewMtx00 = g_viewMtx00);
	mat.m[1] = (float)(viewMtx10 = g_viewMtx10);
	mat.m[2] = (float)(viewMtx20 = g_viewMtx20);
	mat.m[3] = (float)(viewMtx01 = g_viewMtx01);
	mat.m[4] = (float)(viewMtx11 = g_viewMtx11);
	mat.m[5] = (float)(viewMtx21 = g_viewMtx21);
	mat.m[6] = (float)(viewMtx02 = g_viewMtx02);
	mat.m[7] = (float)(viewMtx12 = g_viewMtx12);
	mat.m[8] = (float)(viewMtx22 = g_viewMtx22);
	Math3D_RotateVec3(&g_modelPreviewViewDelta, &mat);

	objMatR1X = g_objViewMatF_R1_X;
	objMatR0X = g_objViewMatF_R0_X;
	objMatR2X = g_objViewMatF_R2_X;
	g_modelPreviewNegViewDelta.x = -g_modelPreviewViewDelta.x;
	objMatR0Y = g_objViewMatF_R0_Y;
	g_modelPreviewNegViewDelta.y = -g_modelPreviewViewDelta.y;
	objMatR1Y = g_objViewMatF_R1_Y;
	g_modelPreviewNegViewDelta.z = -g_modelPreviewViewDelta.z;
	objMatR2Y = g_objViewMatF_R2_Y;
	g_modelPreviewObjectViewMatrix.m[1] = (float)objMatR1X;
	objMatR0Z = g_objViewMatF_R0_Z;
	g_modelPreviewObjectViewMatrix.m[0] = (float)objMatR0X;
	objMatR1Z = g_objViewMatF_R1_Z;
	g_modelPreviewObjectViewMatrix.m[2] = (float)objMatR2X;
	objMatR2Z = g_objViewMatF_R2_Z;
	g_modelPreviewObjectViewMatrix.m[3] = (float)objMatR0Y;
	g_modelPreviewObjectViewMatrix.m[4] = (float)objMatR1Y;
	g_modelPreviewObjectViewMatrix.m[5] = (float)objMatR2Y;
	g_modelPreviewObjectViewMatrix.m[6] = (float)objMatR0Z;
	g_modelPreviewObjectViewMatrix.m[7] = (float)objMatR1Z;
	g_modelPreviewObjectViewMatrix.m[8] = (float)objMatR2Z;
	mat.m[0] = g_objViewMatF_R0_X;
	mat.m[1] = g_objViewMatF_R0_Y;
	mat.m[2] = g_objViewMatF_R0_Z;
	mat.m[3] = g_objViewMatF_R1_X;
	mat.m[4] = g_objViewMatF_R1_Y;
	mat.m[5] = g_objViewMatF_R1_Z;
	mat.m[6] = g_objViewMatF_R2_X;
	mat.m[7] = g_objViewMatF_R2_Y;
	mat.m[8] = g_objViewMatF_R2_Z;
	Math3D_RotateVec3(&g_modelPreviewNegViewDelta, &g_modelPreviewObjectViewMatrix);

#ifdef XWA_MODERN
	{
		/* Remaster snapshot observer — after FVIEW_SetObjectTransform
		 * and the view rotation so the record carries the engine's
		 * resolved object->eye basis + eye-space position. */
		XwaModelPreview snapPreview;
		memset(&snapPreview, 0, sizeof snapPreview);
		snapPreview.wireframe = 0;
		snapPreview.pitch = g_modelPreviewObject.pitch;
		snapPreview.yaw = g_modelPreviewObject.yaw;
		snapPreview.roll = g_modelPreviewObject.roll;
		snapPreview.angle_d = g_modelPreviewAngleD;
		{
			/* Basename only — path components would truncate long
			 * names (CorellianTransport2Exterior.opt). */
			const char* base = g_modelPreviewOptFileName;
			for (const char* c = g_modelPreviewOptFileName; *c; c++) {
				if (*c == '\\' || *c == '/') {
					base = c + 1;
				}
			}
			snprintf(snapPreview.opt_name, sizeof snapPreview.opt_name, "%s", base);
		}
		snapPreview.world_x = g_modelPreviewObject.world_x;
		snapPreview.world_y = g_modelPreviewObject.world_y;
		snapPreview.world_z = g_modelPreviewObject.world_z;
		snapPreview.node_switch_index = g_nodeSwitchIndex;
		snapPreview.dst_x = (int16_t)x;
		snapPreview.dst_y = (int16_t)y;
		snapPreview.dst_w = (int16_t)width;
		snapPreview.dst_h = (int16_t)height;
		snapPreview.line_color = 0;
		snapPreview.obj_basis[0] = g_objViewMatF_R0_X;
		snapPreview.obj_basis[1] = g_objViewMatF_R0_Y;
		snapPreview.obj_basis[2] = g_objViewMatF_R0_Z;
		snapPreview.obj_basis[3] = g_objViewMatF_R1_X;
		snapPreview.obj_basis[4] = g_objViewMatF_R1_Y;
		snapPreview.obj_basis[5] = g_objViewMatF_R1_Z;
		snapPreview.obj_basis[6] = g_objViewMatF_R2_X;
		snapPreview.obj_basis[7] = g_objViewMatF_R2_Y;
		snapPreview.obj_basis[8] = g_objViewMatF_R2_Z;
		snapPreview.eye_delta[0] = g_modelPreviewViewDelta.x;
		snapPreview.eye_delta[1] = g_modelPreviewViewDelta.y;
		snapPreview.eye_delta[2] = g_modelPreviewViewDelta.z;
		snapPreview.model_scale = (float)g_modelPreviewScaleFactor;
		/* This view's camera (world->eye), for rotating the WORLD-space
		 * dir_lights channel into the eye space the basis/delta above
		 * live in. Same g_camMat the wireframe path consumes below. */
		snapPreview.cam_rows[0] = (float)g_camMatR0_X * (1.0f / 32768.0f);
		snapPreview.cam_rows[1] = (float)g_camMatR0_Y * (1.0f / 32768.0f);
		snapPreview.cam_rows[2] = (float)g_camMatR0_Z * (1.0f / 32768.0f);
		snapPreview.cam_rows[3] = (float)g_camMatR1_X * (1.0f / 32768.0f);
		snapPreview.cam_rows[4] = (float)g_camMatR1_Y * (1.0f / 32768.0f);
		snapPreview.cam_rows[5] = (float)g_camMatR1_Z * (1.0f / 32768.0f);
		snapPreview.cam_rows[6] = (float)g_camMatR2_X * (1.0f / 32768.0f);
		snapPreview.cam_rows[7] = (float)g_camMatR2_Y * (1.0f / 32768.0f);
		snapPreview.cam_rows[8] = (float)g_camMatR2_Z * (1.0f / 32768.0f);
		XwaSnapshot_EmitModelPreview(&snapPreview);
	}
#endif

	savedUseHardware3D = g_useHardware3D;
	if (softwareSurface) {
		g_useHardware3D = 0;
	}
	if (g_useHardware3D && g_frontendD3DInitialized) {
		std3D_ClearZBuffer();
	}

	viewZ = (int)g_modelPreviewViewDelta.z;
	g_modelPreviewObject.mobj->nodeSwitchIndex = (uint8_t)g_nodeSwitchIndex;
	g_modelPreviewObject.mobj->orientMatrixDirty = 1;
	g_modelPreviewObject.mobj->moveVectorDirty = 1;
	savedLocalLightsLevel = g_localLightsLevel;
	g_localLightsLevel = 0;

	RenderScene_Initialize(1);
	if (g_useHardware3D && g_frontendD3DInitialized) {
		FlightLight_SetupObjectLighting(&g_modelPreviewObject);
	}
	g_renderFlags = 64;
	RenderScene_DrawObjectModel(&g_modelPreviewObject);
	g_objectTable = &g_modelPreviewObject;
	if (g_useHardware3D && g_frontendD3DInitialized) {
		EngineGlow_RenderSceneGlows();
	}
	g_objectTable = 0;

	if (!softwareSurface) {
		if (g_useHardware3D && g_frontendD3DInitialized) {
			RenderScene_EffectsPass();
			RenderScene_End3D();
		} else {
			RenderScene_DrawVisibleFaces();
		}
	} else if (g_useHardware3D && g_frontendD3DInitialized) {
		RenderScene_EffectsPass();
		RenderScene_End3D();
	} else {
		sw3d_DrawVisibleFacesToSurface(softwareSurface, softwarePitch, softwareHeight);
	}

	g_useHardware3D = savedUseHardware3D;
	g_localLightsLevel = savedLocalLightsLevel;
	return 1;
}

// FUNCTION: XWA 0x50F160
int ModelPreview_RenderWireframeViewport(int x, int y, int width, int height, int lineColor,
										 unsigned char* softwareSurface, int softwarePitch,
										 int softwareHeight) {
	int savedLocalLightsLevel;
	int traceIndex;

	if (!g_loadedModels.byObjectType[0]) {
		return 0;
	}

	if (x < 0) {
		width += x;
		x = 0;
	}
	if (y < 0) {
		height += y;
	}

	if (!softwareSurface) {
		if (x >= 1600) {
			return 0;
		}
		if (y >= 1200) {
			return 0;
		}
		if (height + y > 1200) {
			height = 1200 - y;
		}
		if (width + x > 1600) {
			width = 1600 - x;
		}
	} else {
		if (x >= softwarePitch) {
			return 0;
		}
		if (y >= softwareHeight) {
			return 0;
		}
		if (height + y > softwareHeight) {
			height = softwareHeight - y;
		}
		if (width + x > (softwarePitch >> 1)) {
			width = (softwarePitch >> 1) - x;
		}
	}
#ifdef XWA_MODERN
	{
		/* Remaster snapshot observer. */
		XwaModelPreview snapPreview;
		memset(&snapPreview, 0, sizeof snapPreview);
		snapPreview.wireframe = 1;
		snapPreview.pitch = g_modelPreviewObject.pitch;
		snapPreview.yaw = g_modelPreviewObject.yaw;
		snapPreview.roll = g_modelPreviewObject.roll;
		snapPreview.angle_d = g_modelPreviewAngleD;
		snprintf(snapPreview.opt_name, sizeof snapPreview.opt_name, "%s", g_modelPreviewOptFileName);
		snapPreview.world_x = g_modelPreviewObject.world_x;
		snapPreview.world_y = g_modelPreviewObject.world_y;
		snapPreview.world_z = g_modelPreviewObject.world_z;
		snapPreview.node_switch_index = g_nodeSwitchIndex;
		snapPreview.dst_x = (int16_t)x;
		snapPreview.dst_y = (int16_t)y;
		snapPreview.dst_w = (int16_t)width;
		snapPreview.dst_h = (int16_t)height;
		snapPreview.line_color = (uint32_t)lineColor;
		XwaSnapshot_EmitModelPreview(&snapPreview);
	}
#endif

	g_flightVpWidth = width;
	g_flightVpMaxX = width - 1;
	g_flightVpCenterX = width / 2;
	g_flightVpMaxY = height - 1;
	g_flightVpCenterY = height / 2;
	g_flightVpCenterYf = (float)(uint16_t)g_flightVpCenterY;
	g_flightVpHeight = height;
	traceIndex = 0;
	g_flightVpY = y;
	g_flightVpX = x;
	g_flightVpCenterXf = (float)(uint16_t)g_flightVpCenterX;
	if (!softwareSurface) {
		g_flightVpBaseOffset = x + g_surfacePitch * y;
	} else {
		g_flightVpBaseOffset = x + y * softwarePitch;
	}

	g_projScaleInt = 512;
	g_projScaleHalfInt = 256;
	perspShift = 9;
	g_projAspectY = (uint16_t)traceIndex;
	FVIEW_BuildCameraOrient(g_players[g_localPlayer].viewState.viewRoll,
							g_players[g_localPlayer].viewState.viewPitch,
							g_players[g_localPlayer].viewState.viewYaw, 0, 0, 0, NULL, -1);
	FVIEW_SetObjectTransform(g_modelPreviewObject.roll, g_modelPreviewObject.pitch, g_modelPreviewObject.yaw,
							 g_modelPreviewAngleD, 0);

	g_modelPreviewViewDelta.x =
		(float)(g_modelPreviewObject.world_x - g_players[g_localPlayer].viewState.savedTargetX);
	g_modelPreviewViewDelta.y =
		(float)(g_modelPreviewObject.world_y - g_players[g_localPlayer].viewState.savedTargetY);
	g_modelPreviewViewDelta.z =
		(float)(g_modelPreviewObject.world_z - g_players[g_localPlayer].viewState.savedTargetZ);

	mat.m[0] = (float)g_camMatR0_X * flt_5AA088;
	mat.m[1] = (float)g_camMatR1_X * flt_5AA088;
	mat.m[2] = (float)g_camMatR2_X * flt_5AA088;
	mat.m[3] = (float)g_camMatR0_Y * flt_5AA088;
	mat.m[4] = (float)g_camMatR1_Y * flt_5AA088;
	mat.m[5] = (float)g_camMatR2_Y * flt_5AA088;
	mat.m[6] = (float)g_camMatR0_Z * flt_5AA088;
	mat.m[7] = (float)g_camMatR1_Z * flt_5AA088;
	mat.m[8] = (float)g_camMatR2_Z * flt_5AA088;
	Math3D_RotateVec3(&g_modelPreviewViewDelta, &mat);

	{
		float objMatR0X;
		float objMatR0Y;
		float objMatR0Z;
		float objMatR1X;
		float objMatR1Y;
		float objMatR1Z;
		float objMatR2Y;
		float objMatR2Z;

		objMatR0X = (float)g_objViewMat_R0_X * flt_5AA088;
		objMatR0Y = (float)g_objViewMat_R0_Y * flt_5AA088;
		mat.m[0] = objMatR0X;
		mat.m[1] = objMatR0Y;
		objMatR0Z = (float)g_objViewMat_R0_Z * flt_5AA088;
		mat.m[2] = objMatR0Z;
		objMatR1X = (float)g_objViewMat_R1_X * flt_5AA088;
		objMatR1Y = (float)g_objViewMat_R1_Y * flt_5AA088;
		objMatR1Z = (float)g_objViewMat_R1_Z * flt_5AA088;
		mat.m[3] = objMatR1X;
		mat.m[4] = objMatR1Y;
		mat.m[5] = objMatR1Z;
		mat.m[6] = (float)g_objViewMat_R2_X * flt_5AA088;
		objMatR2Y = (float)g_objViewMat_R2_Y * flt_5AA088;
		objMatR2Z = (float)g_objViewMat_R2_Z * flt_5AA088;

		g_modelPreviewNegViewDelta.x = -g_modelPreviewViewDelta.x;
		g_modelPreviewNegViewDelta.y = -g_modelPreviewViewDelta.y;
		g_modelPreviewNegViewDelta.z = -g_modelPreviewViewDelta.z;
		g_modelPreviewObjectViewMatrix.m[0] = objMatR0X;
		mat.m[7] = objMatR2Y;
		g_modelPreviewObjectViewMatrix.m[1] = objMatR1X;
		g_modelPreviewObjectViewMatrix.m[2] = mat.m[6];
		mat.m[8] = objMatR2Z;
		g_modelPreviewObjectViewMatrix.m[3] = objMatR0Y;
		g_modelPreviewObjectViewMatrix.m[4] = objMatR1Y;
		g_modelPreviewObjectViewMatrix.m[5] = objMatR2Y;
		g_modelPreviewObjectViewMatrix.m[6] = objMatR0Z;
		g_modelPreviewObjectViewMatrix.m[7] = objMatR1Z;
		g_modelPreviewObjectViewMatrix.m[8] = objMatR2Z;
	}
	Math3D_RotateVec3(&g_modelPreviewNegViewDelta, &g_modelPreviewObjectViewMatrix);

	viewZ = (int)g_modelPreviewViewDelta.z;
	g_modelPreviewObject.mobj->nodeSwitchIndex = (uint8_t)g_nodeSwitchIndex;
	savedLocalLightsLevel = g_localLightsLevel;
	g_localLightsLevel = traceIndex;
	RenderScene_Initialize(1);
	RenderScene_ProjectPreviewWireframeModel(&g_modelPreviewObject);

	if (softwareSurface) {
		FrontendDraw_BeginExternalSurface(softwareSurface, softwarePitch);
	}

	{
		int pointCount;
		int pointsX[5];
		int pointsY[5];

		pointCount = traceIndex;
		for (traceIndex = 0; traceIndex < g_projectedFaceTraceCount; ++traceIndex) {
			if (g_projectedFaceTraceX[traceIndex] || g_projectedFaceTraceY[traceIndex]) {
				if (pointCount < 5) {
					pointsX[pointCount] = g_projectedFaceTraceX[traceIndex];
					pointsY[pointCount] = g_projectedFaceTraceY[traceIndex];
					++pointCount;
				}
			} else {
				int fromIndex;

				for (fromIndex = 0; fromIndex < pointCount; ++fromIndex) {
					int toIndex;

					for (toIndex = 0; toIndex < pointCount; ++toIndex) {
						if (toIndex != fromIndex) {
							FrontendDraw_Line(
								g_flightVpX + pointsX[fromIndex], g_flightVpY + pointsY[fromIndex],
								g_flightVpX + pointsX[toIndex], g_flightVpY + pointsY[toIndex], lineColor);
						}
					}
				}
				pointCount = 0;
			}
		}
	}

	if (softwareSurface) {
		FrontendDraw_EndExternalSurface();
	}

	g_localLightsLevel = savedLocalLightsLevel;
	return 1;
}

// FUNCTION: XWA 0x50FA50
int ModelPreview_SetWhiteDirectionalLight(int dx, int dy, int dz) {
	double lightX;
	double lightY;
	double lightZ;
	double invLength;

	lightY = (double)-dy;
	lightX = (double)dx;
	lightZ = (double)dz;
	invLength = g_modelPreviewInvLengthNumerator / sqrt(lightX * lightX + lightY * lightY + lightZ * lightZ);

	FlightLight_ClearDirectionalLights();
	return FlightLight_AddDirectionalLight((int)(invLength * lightX * g_modelPreviewQ15Scale),
										   (int)(invLength * lightY * g_modelPreviewQ15Scale),
										   (int)(invLength * lightZ * g_modelPreviewQ15Scale), 0.80000001f,
										   1.0f, 1.0f, 1.0f);
}

// FUNCTION: XWA 0x50FB10
void ModelPreview_SetObjectEulerDegrees(float pitchDeg, float yawDeg, float rollDeg) {
	g_modelPreviewObject.pitch = (uint16_t)(int64_t)(pitchDeg * g_degreesToQ16Scale);
	g_modelPreviewObject.yaw = (uint16_t)(int64_t)(yawDeg * g_degreesToQ16Scale);
	g_modelPreviewObject.roll = (uint16_t)(int64_t)(rollDeg * g_degreesToQ16Scale);
}

// FUNCTION: XWA 0x50FB60
int ModelPreview_SetObjectWorldPosition(int x, int y, int z) {
	g_modelPreviewObject.world_x = x;
	g_modelPreviewObject.world_y = y;
	g_modelPreviewObject.world_z = z;

	return x;
}

// FUNCTION: XWA 0x50FB50
int ModelPreview_SetNodeSwitchIndex(int nodeSwitchIndex) {
	g_nodeSwitchIndex = nodeSwitchIndex;
	return nodeSwitchIndex;
}

// FUNCTION: XWA 0x50FB80
int64_t ModelPreview_SetObjectAngleDDegrees(float angleDeg) {
	int64_t result;

	result = (int64_t)(angleDeg * g_degreesToQ16Scale);
	g_modelPreviewAngleD = (uint16_t)result;

	return result;
}

// FUNCTION: XWA 0x50FBA0
int ModelPreview_GetDisplayedSizeMeters(void) {
	double displayedSize;

	displayedSize = g_modelPreviewUnscaledMaxExtent;
	displayedSize *= g_modelPreviewMetersScale;
	displayedSize *= g_modelPreviewQ16Scale;
	return (int)displayedSize;
}

// FUNCTION: XWA 0x50FBC0
int Frontend3D_SetRuntimeHardwareEnabled(int enabled) {
	g_useHardware3D = g_frontendD3DInitialized ? enabled : 0;
	return 1;
}
