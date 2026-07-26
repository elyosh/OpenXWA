#ifndef XWA_ASSETS_MODEL_MESH_H
#define XWA_ASSETS_MODEL_MESH_H

#include "xwa/assets/opt_model.h"
#include "xwa/render/renderer.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int g_rotatedX;
extern int g_rotatedY;
extern int g_rotatedZ;
extern const uint8_t g_meshTypeComponentMaxHp[32];

typedef enum MeshType {
	MESH_Default = 0,
	MESH_MainHull = 1,
	MESH_Wing = 2,
	MESH_Fuselage = 3,
	MESH_GunTurret = 4,
	MESH_SmallGun = 5,
	MESH_Engine = 6,
	MESH_Bridge = 7,
	MESH_ShieldGenerator = 8,
	MESH_EnergyGenerator = 9,
	MESH_Launcher = 10,
	MESH_CommunicationSystem = 11,
	MESH_BeamSystem = 12,
	MESH_CommandSystem = 13,
	MESH_DockingPlatform = 14,
	MESH_LandingPlatform = 15,
	MESH_Hangar = 16,
	MESH_CargoPod = 17,
	MESH_MiscHull = 18,
	MESH_Antenna = 19,
	MESH_RotaryWing = 20,
	MESH_RotaryGunTurret = 21,
	MESH_RotaryLauncher = 22,
	MESH_RotaryCommSystem = 23,
	MESH_RotaryBeamSystem = 24,
	MESH_RotaryCommandSystem = 25,
	MESH_Hatch = 26,
	MESH_Custom = 27,
	MESH_WeaponSystem1 = 28,
	MESH_WeaponSystem2 = 29,
	MESH_PowerRegenerator = 30,
	MESH_Reactor = 31,
} MeshType;

typedef enum MeshExplosionTypeFlags {
	MESH_EXPLOSION_TYPE1 = 1,
	MESH_EXPLOSION_TYPE2 = 2,
	MESH_EXPLOSION_TYPE3 = 4,
} MeshExplosionTypeFlags;

struct ModelFloatHardpoint;

typedef enum OptHardpointType {
	OPT_HARDPOINT_None = 0,
	OPT_HARDPOINT_RebelLaser = 1,
	OPT_HARDPOINT_TurboRebelLaser = 2,
	OPT_HARDPOINT_EmpireLaser = 3,
	OPT_HARDPOINT_TurboEmpireLaser = 4,
	OPT_HARDPOINT_IonCannon = 5,
	OPT_HARDPOINT_TurboIonCannon = 6,
	OPT_HARDPOINT_Torpedo = 7,
	OPT_HARDPOINT_Missile = 8,
	OPT_HARDPOINT_SuperRebelLaser = 9,
	OPT_HARDPOINT_SuperEmpireLaser = 10,
	OPT_HARDPOINT_SuperIonCannon = 11,
	OPT_HARDPOINT_SuperTorpedo = 12,
	OPT_HARDPOINT_SuperMissile = 13,
	OPT_HARDPOINT_DumbBomb = 14,
	OPT_HARDPOINT_FiredBomb = 15,
	OPT_HARDPOINT_MagPulse = 16,
	OPT_HARDPOINT_TurboMagPulse = 17,
	OPT_HARDPOINT_SuperMagPulse = 18,
	OPT_HARDPOINT_Gunner = 19,
	OPT_HARDPOINT_CockpitSparks = 20,
	OPT_HARDPOINT_DockingPoint = 21,
	OPT_HARDPOINT_Towing = 22,
	OPT_HARDPOINT_AccStart = 23,
	OPT_HARDPOINT_AccEnd = 24,
	OPT_HARDPOINT_InsideHangar = 25,
	OPT_HARDPOINT_OutsideHangar = 26,
	OPT_HARDPOINT_DockFromBig = 27,
	OPT_HARDPOINT_DockFromSmall = 28,
	OPT_HARDPOINT_DockToBig = 29,
	OPT_HARDPOINT_DockToSmall = 30,
	OPT_HARDPOINT_Cockpit = 31,
	OPT_HARDPOINT_EngineGlow = 32,
	OPT_HARDPOINT_Custom1 = 33,
	OPT_HARDPOINT_Custom2 = 34,
	OPT_HARDPOINT_Custom3 = 35,
	OPT_HARDPOINT_Custom4 = 36,
	OPT_HARDPOINT_Custom5 = 37,
	OPT_HARDPOINT_Custom6 = 38,
	OPT_HARDPOINT_JammingPoint = 39,
} OptHardpointType;

typedef struct MeshDescriptor {
	MeshType meshType;
	MeshExplosionTypeFlags explosionType;
	Vec3f span;
	Vec3f center;
	Vec3f boxMin;
	Vec3f boxMax;
	int targetId;
	Vec3f target;
} MeshDescriptor;

typedef struct OptHardpoint {
	OptHardpointType hardpointType;
	Vec3f position;
} OptHardpoint;

typedef struct ObjectTypeMeshCache {
	int meshCount;
	MeshType meshTypes[50];
	MeshDescriptor* meshDescriptors[50];
} ObjectTypeMeshCache;

typedef struct OptRotationScale {
	Vec3f pivot;
	Vec3f rotationAxis;
	Vec3f directionAxis;
	Vec3f upAxis;
} OptRotationScale;

struct OptEngineGlow {
	int isDisabled;
	uint32_t coreColor;
	uint32_t outerColor;
	Vec3f dimensions;
	Vec3f position;
	Vec3f lookAxis;
	Vec3f upAxis;
	Vec3f rightAxis;
};

int ModelMesh_GetCount(int modelSlot);
int ModelMesh_GetObjectTypeMeshCount(int objectType);
void ModelMesh_BuildObjectTypeMeshCache(void);
OptNode* ModelMesh_FindFirstMeshVertsNode(OptNode* node);
OptNode* ModelMesh_FindFirstVertexNormalsNode(OptNode* node);
void ModelMesh_GetVertexNormalsData(int modelType, int meshIndex, Vec3f** outNormals);
OptNode* ModelMesh_GetVerticesData(int modelType, int meshIndex, Vec3f** outVertices, int* outVertexCount);
OptNode* ModelMesh_FindFirstRotScaleNode(OptNode* node);
MeshDescriptor* ModelMesh_FindDescriptorNodeRecursive(OptNode* node, OptimizedPolyObject* model);
OptNode* ModelMesh_FindHullTexNode(OptNode* node);
int16_t ModelMesh_AssignDebrisTexSlot(ObjectTypeId modelType, uint16_t slotId);
int16_t ModelMesh_AllocDebrisTexSlot(ObjectTypeId modelType);
int ModelMesh_FindNearestLiveFloatHardpoint(int modelType, int localX, int localY, int localZ,
											int excludedCount, int* excludedHardpointIndices,
											struct ModelFloatHardpoint* hardpoints, CraftData* craft);
int ModelMesh_FindNearestVertexForPoint(int modelType, int localX, int localY, int localZ, int meshIndex,
										int excludedCount, int* excludedVertexIndices,
										uint8_t* vertexComponentMap, CraftData* craft);
OptRotationScale* ModelMesh_GetRotScaleData(int modelType, int meshIndex);
void ModelMesh_ApplyAnimatedMeshRotationToPoint(int angleQ16, unsigned int modelType, unsigned int meshIdx,
												int localX, int localY, int localZ);
MeshDescriptor* ModelMesh_GetDescriptor(int modelSlot, int meshIndex);
MeshDescriptor* ModelMesh_GetObjectTypeMeshDescriptor(int objectType, int meshIndex);
int ModelMesh_GetVertexX(int modelType, int meshIndex, int vertexIndex);
int ModelMesh_GetVertexY(int modelType, int meshIndex, int vertexIndex);
int ModelMesh_GetVertexZ(int modelType, int meshIndex, int vertexIndex);
int ModelMesh_PickRandomVertex(int modelType, int meshIndex, Vec3f** outVertex);
int ModelMesh_GetCenterX(int modelType, int meshIndex);
int ModelMesh_GetCenterY(int modelType, int meshIndex);
int ModelMesh_GetCenterZ(int modelType, int meshIndex);
int ModelMesh_GetComponentFocusX(int objectType, int componentIdx);
int ModelMesh_GetComponentFocusY(int objectType, int componentIdx);
int ModelMesh_GetComponentFocusZ(int objectType, int componentIdx);
int ModelMesh_GetComponentMaxExtent(int objectType, int componentIdx);
int ModelMesh_GetBoundsMinX(int modelType, int meshIndex);
int ModelMesh_GetBoundsMinY(int modelType, int meshIndex);
int ModelMesh_GetBoundsMinZ(int modelType, int meshIndex);
int ModelMesh_GetBoundsMaxX(int modelType, int meshIndex);
int ModelMesh_GetBoundsMaxY(int modelType, int meshIndex);
int ModelMesh_GetBoundsMaxZ(int modelType, int meshIndex);
int ModelMesh_GetBoundsSizeX(int modelType, int meshIndex);
int ModelMesh_GetBoundsSizeY(int modelType, int meshIndex);
int ModelMesh_GetBoundsSizeZ(int modelType, int meshIndex);
int ModelMesh_GetBoundsVolume(int modelType, int meshIndex);
int ModelMesh_FindNearestByBounds(int modelType, int localX, int localY, int localZ);
int ModelMesh_FindNearestLiveMainHullByBounds(int modelType, int localX, int localY, int localZ,
											  CraftData* craft);
int ModelMesh_FindFloatHardpointComponent(int modelType, int localX, int localY, int localZ);
int ModelMesh_FindNearestLiveComponentByType(int modelType, MeshType meshTypeFilter, int localX, int localY,
											 int localZ, CraftData* craft);
int ModelMesh_HasFuselage(int modelType);
int ModelMesh_GetTargetId(int modelType, int meshIndex);
MeshType ModelMesh_GetType(int modelSlot, int meshIndex);
MeshType ModelMesh_GetObjectTypeMeshType(int objectType, int meshIndex);
int ModelMesh_IsObjectTypeMeshDamageable(int objectType, int meshIndex);
int ModelMesh_HasExplosionType1(int modelType, int meshIndex);
int ModelMesh_CountHardpointNodesRecursive(OptNode* node, OptimizedPolyObject* optBase);
int ModelMesh_CountHardpoints(int modelType, int meshIndex);
int ModelMesh_GetAlternateHardpointIndex(int modelType, int meshIndex, int hardpointIndex);
OptNode* ModelMesh_FindNthHardpointNodeRecursive(OptNode* node, OptimizedPolyObject* optBase,
												 int hardpointIndex);
int ModelMesh_GetHardpointX(int modelType, int meshIndex, int hardpointIndex);
int ModelMesh_GetHardpointY(int modelType, int meshIndex, int hardpointIndex);
int ModelMesh_GetHardpointZ(int modelType, int meshIndex, int hardpointIndex);
void ModelMesh_GetHardpoint(int modelType, int meshIndex, int hardpointIndex, OptHardpointType* outType,
							int* outX, int* outY, int* outZ);
int ModelMesh_CountEngineGlowNodesRecursive(OptNode* node, OptimizedPolyObject* model);
int ModelMesh_CountEngineGlows(int modelType, int meshIndex);
OptNode* ModelMesh_FindNthEngineGlowNodeRecursive(OptNode* node, OptimizedPolyObject* model,
												  int engineGlowIndex);
OptEngineGlow* ModelMesh_GetEngineGlowParam(int modelType, int meshIndex, int engineGlowIndex);
int ModelMesh_FindBridgeIndex(OptimizedPolyObject* model);

extern ObjectTypeMeshCache g_objectTypeMeshCache[OBJ_Count];
extern ObjectTypeMeshCache g_cockpitModelMeshCache;
extern ObjectTypeMeshCache g_exteriorModelMeshCache;

#ifdef __cplusplus
}
#endif

#endif
