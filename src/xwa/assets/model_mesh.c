#include "xwa/assets/model_mesh.h"

#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"
#include "xwa/flight/fediskio.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/math/fixed.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/player/player.h"
#include "xwa/util/memory.h"
#include "xwa/util/random.h"

#include <string.h>

// GLOBAL: XWA 0x692804
int g_optHardpointSearchIndex;
// GLOBAL: XWA 0x692808
static int g_optEngineGlowSearchIndex;

typedef struct ModelMeshNodeSearchScratch {
	int bestTextureArea;
	int field_4;
	int field_8;
} ModelMeshNodeSearchScratch;

// GLOBAL: XWA 0x80DA48
int g_rotatedX;
// GLOBAL: XWA 0x80DA44
int g_rotatedY;
// GLOBAL: XWA 0x80DB64
int g_rotatedZ;
// GLOBAL: XWA 0x690524
ModelMeshNodeSearchScratch g_optModelNodeSearchScratch;
// GLOBAL: XWA 0x68EB00
OptNode* g_modelTextureSearchBestNode;

// GLOBAL: XWA 0x8D9760
ObjectTypeMeshCache g_objectTypeMeshCache[OBJ_Count];
// GLOBAL: XWA 0x9107A0
ObjectTypeMeshCache g_cockpitModelMeshCache;
// GLOBAL: XWA 0x808160
ObjectTypeMeshCache g_exteriorModelMeshCache;
// GLOBAL: XWA 0x5A9A44
const float flt_5A9A44 = 0.0f;

// FUNCTION: XWA 0x485190
__inline int ModelMesh_GetCount(int modelSlot) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	int meshCount;
	OptNode* firstRootNode;

	modelHandle = g_loadedModels.byObjectType[modelSlot];
	if (!modelHandle) {
		return 0;
	}

	if (!g_flightRenderToFrontend && !(g_modelTypeTable[modelSlot].assetFlags & 1)) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	meshCount = model->rootNodeCount;
	firstRootNode = model->rootNodes[0];
	if (firstRootNode->nodeType == OPT_TEXTURE || firstRootNode->nodeType == OPT_TEXTURE_REF) {
		--meshCount;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelSlot]);
	if (meshCount > 50) {
		meshCount = 50;
	}

	return meshCount;
}

// FUNCTION: XWA 0x488960
int ModelMesh_GetObjectTypeMeshCount(int objectType) {
	if (objectType < OBJ_Count) {
		return g_objectTypeMeshCache[objectType].meshCount;
	}

	return ModelMesh_GetCount(objectType);
}

// FUNCTION: XWA 0x488A50
MeshDescriptor* ModelMesh_GetObjectTypeMeshDescriptor(int objectType, int meshIndex) {
	int localObjectType;

	if (g_cockpitViewActive && g_cockpitModel != 0) {
		localObjectType = g_objectTable[g_players[g_localPlayer].objectIndex].objectType;
		if (localObjectType == objectType) {
			if (g_cockpitModelMeshCache.meshCount == 0) {
				return NULL;
			}
			if (meshIndex > g_cockpitModelMeshCache.meshCount) {
				meshIndex = g_cockpitModelMeshCache.meshCount - 1;
			}
			return g_cockpitModelMeshCache.meshDescriptors[meshIndex];
		}
	}

	if (g_drawingOwnCraft && g_exteriorModel != 0) {
		localObjectType = g_objectTable[g_players[g_localPlayer].objectIndex].objectType;
		if (localObjectType == objectType) {
			if (g_exteriorModelMeshCache.meshCount == 0) {
				return NULL;
			}
			if (meshIndex > g_exteriorModelMeshCache.meshCount) {
				meshIndex = g_exteriorModelMeshCache.meshCount - 1;
			}
			return g_exteriorModelMeshCache.meshDescriptors[meshIndex];
		}
	}

	if (objectType < OBJ_Count) {
		if (meshIndex >= 0 && g_objectTypeMeshCache[objectType].meshCount != 0) {
			int meshCount = g_objectTypeMeshCache[objectType].meshCount;
			if (meshIndex >= meshCount) {
				meshIndex = meshCount - 1;
			}
			return g_objectTypeMeshCache[objectType].meshDescriptors[meshIndex];
		}

		return NULL;
	}

	return ModelMesh_GetDescriptor(objectType, meshIndex);
}

// FUNCTION: XWA 0x4883D0
void ModelMesh_BuildObjectTypeMeshCache(void) {
	int objectType;

	for (objectType = 0; objectType < OBJ_Count; ++objectType) {
		uint16_t modelHandle;
		int meshCount;
		int meshIndex;
		ObjectTypeMeshCache* cache;

		modelHandle = g_loadedModels.byObjectType[objectType];
		meshCount = 0;

		if (modelHandle != 0 && (g_flightRenderToFrontend || (g_modelTypeTable[objectType].assetFlags &
															  MODEL_TYPE_ASSET_MODEL_LOADED) != 0)) {
			OptimizedPolyObject* model;
			OptNodeType firstRootType;

			model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
			OptModel_AdjustOptimizedPolyObjectPointers(model);

			meshCount = model->rootNodeCount;
			firstRootType = model->rootNodes[0]->nodeType;
			if (firstRootType == OPT_TEXTURE || firstRootType == OPT_TEXTURE_REF) {
				--meshCount;
			}

			Memory_UnlockHandle(modelHandle);
			if (meshCount > 50) {
				meshCount = 50;
			}
		}

		cache = &g_objectTypeMeshCache[objectType];
		cache->meshCount = meshCount;
		for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
			cache->meshTypes[meshIndex] = ModelMesh_GetType(objectType, meshIndex);
			cache->meshDescriptors[meshIndex] = ModelMesh_GetDescriptor(objectType, meshIndex);
		}
	}
}

// FUNCTION: XWA 0x485220
OptNode* ModelMesh_FindFirstMeshVertsNode(OptNode* node) {
	int i;

	if (!node) {
		return 0;
	}

	if (node->nodeType == OPT_MESHVERTS) {
		return node;
	}

	for (i = 0; i < node->childCount; ++i) {
		OptNode* found;

		if (!node->pChildren[i]) {
			continue;
		}

		found = ModelMesh_FindFirstMeshVertsNode(node->pChildren[i]);
		if (found) {
			return found;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x485270
OptNode* ModelMesh_FindFirstVertexNormalsNode(OptNode* node) {
	int i;

	if (!node) {
		return 0;
	}

	if (node->nodeType == OPT_VERTNORMALS) {
		return node;
	}

	for (i = 0; i < node->childCount; ++i) {
		OptNode* found;

		if (!node->pChildren[i]) {
			continue;
		}

		found = ModelMesh_FindFirstVertexNormalsNode(node->pChildren[i]);
		if (found) {
			return found;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x488270
void ModelMesh_GetVertexNormalsData(int modelType, int meshIndex, Vec3f** outNormals) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* rootNode;
	OptNode* normalsNode;
	int childIndex;

	*outNormals = 0;
	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}

	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	rootNode = rootNodes[meshIndex];
	if (rootNode) {
		if (rootNode->nodeType != OPT_VERTNORMALS) {
			childIndex = 0;
			if (childIndex < rootNode->childCount) {
				while (1) {
					normalsNode = rootNode->pChildren[childIndex];
					if (normalsNode) {
						normalsNode = ModelMesh_FindFirstVertexNormalsNode(normalsNode);
					}

					if (!normalsNode) {
						++childIndex;
						if (childIndex < rootNode->childCount) {
							continue;
						}
						rootNode = 0;
					} else {
						rootNode = normalsNode;
					}
					break;
				}
			} else {
				rootNode = 0;
			}
		}
	} else {
		rootNode = 0;
	}

#ifdef XWA_MODERN
	*outNormals = rootNode ? (Vec3f*)rootNode->param2 : 0;
#else
	*outNormals = (Vec3f*)rootNode->param2;
#endif
}

// FUNCTION: XWA 0x485960
// `model` is the owning OptimizedPolyObject; the original threads it through the
// recursion but never reads it (a vestigial search-context parameter).
MeshDescriptor* ModelMesh_FindDescriptorNodeRecursive(OptNode* node, OptimizedPolyObject* model) {
	int i;

	if (!node) {
		return 0;
	}

	if (node->nodeType == OPT_MESHDESC) {
		return (MeshDescriptor*)node->param2;
	}

	for (i = 0; i < node->childCount; ++i) {
		MeshDescriptor* descriptor;

		if (!node->pChildren[i]) {
			continue;
		}

		descriptor = ModelMesh_FindDescriptorNodeRecursive(node->pChildren[i], model);
		if (descriptor) {
			return descriptor;
		}
	}

	return 0;
}

// Inline entry frame for the root-level descriptor search: resolves the node
// itself (mesh-descriptor shortcut) and its immediate children, delegating the
// deeper descent to ModelMesh_FindDescriptorNodeRecursive. The original inlines
// this frame at callers that cannot early-return in their own body (e.g. those
// that unlock the model handle on the way out).
static __inline MeshDescriptor* ModelMesh_FindRootDescriptor(OptNode* rootNode, OptimizedPolyObject* model) {
	int i;

	if (!rootNode) {
		return 0;
	}

	if (rootNode->nodeType == OPT_MESHDESC) {
		return (MeshDescriptor*)rootNode->param2;
	}

	for (i = 0; i < rootNode->childCount; ++i) {
		OptNode* childNode;
		OptNode** children;
		MeshDescriptor* descriptor;

		children = rootNode->pChildren;
		childNode = children[i];
		if (!childNode) {
			continue;
		}

		descriptor = ModelMesh_FindDescriptorNodeRecursive(childNode, model);
		if (descriptor) {
			return descriptor;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x488DD0
OptNode* ModelMesh_FindHullTexNode(OptNode* node) {
	int textureArea;
	int i;

	if (g_useHardware3D) {
		if (node->nodeType == OPT_TEXTURE_REF) {
			D3DInfoNode* textureInfo;

			textureInfo = (D3DInfoNode*)node->param2;
			textureArea = textureInfo->textureData.width * textureInfo->textureData.height;
			if (textureArea > g_optModelNodeSearchScratch.bestTextureArea) {
				g_optModelNodeSearchScratch.bestTextureArea = textureArea;
				g_modelTextureSearchBestNode = node;
			}
		}
	} else if (node->nodeType == OPT_TEXTURE) {
		textureArea = ((OptTextureData*)node->param2)->width * ((OptTextureData*)node->param2)->height;
		if (textureArea > g_optModelNodeSearchScratch.bestTextureArea) {
			g_optModelNodeSearchScratch.bestTextureArea = textureArea;
			g_modelTextureSearchBestNode = node;
		}
	}

	for (i = 0; i < node->childCount; ++i) {
		if (node->pChildren[i]) {
			ModelMesh_FindHullTexNode(node->pChildren[i]);
		}
	}

	return g_modelTextureSearchBestNode;
}

// FUNCTION: XWA 0x488CA0
int16_t ModelMesh_AssignDebrisTexSlot(ObjectTypeId modelType, uint16_t slotId) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	OptNode* bestTextureNode;
	int rootIdx;

	modelHandle = g_loadedModels.byObjectType[modelType];
	if (!modelHandle) {
		return -1;
	}
	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return -1;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	g_optModelNodeSearchScratch.field_8 = 0;
	g_optModelNodeSearchScratch.field_4 = 0;
	g_optModelNodeSearchScratch.bestTextureArea = 0;
	g_modelTextureSearchBestNode = 0;
	bestTextureNode = 0;

	for (rootIdx = 0; rootIdx < model->rootNodeCount; ++rootIdx) {
		OptNode* rootNode;
		OptNodeType nodeType;
		MeshDescriptor* meshDesc;

		rootNode = model->rootNodes[rootIdx];
		nodeType = rootNode->nodeType;
		if (nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		meshDesc = 0;
		if (nodeType == OPT_MESHDESC) {
			meshDesc = (MeshDescriptor*)rootNode->param2;
		} else {
			int childIdx;

			for (childIdx = 0; childIdx < rootNode->childCount; ++childIdx) {
				OptNode* childNode;

				childNode = rootNode->pChildren[childIdx];
				if (childNode != 0) {
					meshDesc = ModelMesh_FindDescriptorNodeRecursive(childNode, model);
					if (meshDesc != 0) {
						break;
					}
				}
			}
		}

		if (meshDesc != 0 && meshDesc->meshType == MESH_MainHull) {
			bestTextureNode = ModelMesh_FindHullTexNode(rootNode);
		}
	}

	if (bestTextureNode == 0) {
		return -1;
	}

	g_modelTextureOverrideSlots[slotId].textureNode = bestTextureNode;
	g_modelTextureOverrideSlots[slotId].modelType = (uint16_t)modelType;
	return (int16_t)slotId;
}

// FUNCTION: XWA 0x488B60
int16_t ModelMesh_AllocDebrisTexSlot(ObjectTypeId modelType) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	OptNode* bestTextureNode;
	int rootIdx;

	modelHandle = g_loadedModels.byObjectType[modelType];
	if (!modelHandle) {
		return -1;
	}
	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return -1;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	g_optModelNodeSearchScratch.field_8 = 0;
	g_optModelNodeSearchScratch.field_4 = 0;
	g_optModelNodeSearchScratch.bestTextureArea = 0;
	g_modelTextureSearchBestNode = 0;
	bestTextureNode = 0;

	for (rootIdx = 0; rootIdx < model->rootNodeCount; ++rootIdx) {
		OptNode* rootNode;
		OptNodeType nodeType;
		MeshDescriptor* meshDesc;

		rootNode = model->rootNodes[rootIdx];
		nodeType = rootNode->nodeType;
		if (nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		meshDesc = ModelMesh_FindRootDescriptor(rootNode, NULL);

		if (meshDesc != 0 && meshDesc->meshType == MESH_MainHull) {
			bestTextureNode = ModelMesh_FindHullTexNode(rootNode);
		}
	}

	if (bestTextureNode == 0) {
		return -1;
	}

	if (g_modelTextureOverrideNextSlot >= 32u) {
		g_modelTextureOverrideNextSlot = 0;
	}

	g_modelTextureOverrideSlots[g_modelTextureOverrideNextSlot].textureNode = bestTextureNode;
	g_modelTextureOverrideSlots[g_modelTextureOverrideNextSlot].modelType = (uint16_t)modelType;
	++g_modelTextureOverrideNextSlot;
	return (int16_t)(g_modelTextureOverrideNextSlot - 1u);
}

// FUNCTION: XWA 0x487FD0
int ModelMesh_FindNearestLiveFloatHardpoint(int modelType, int localX, int localY, int localZ,
											int excludedCount, int* excludedHardpointIndices,
											ModelFloatHardpoint* hardpoints, CraftData* craft) {
	int floatHardpointCount;
	int markCount;
	int bestHardpointIndex;
	Vec3f target;
	float bestDistanceSq;
	float distanceSq;
	int hardpointIndex;
	uint8_t excludedHardpointMap[256];

	target.x = (float)localX;
	target.y = (float)localY;
	target.z = (float)localZ;

	floatHardpointCount = g_modelDefs[(uint16_t)g_modelTypeTable[modelType].modelIndex].floatHardpointCount;
	markCount = excludedCount;
	if (markCount >= floatHardpointCount) {
		markCount = floatHardpointCount - 1;
	}

	memset(excludedHardpointMap, 0, (size_t)floatHardpointCount);
	if (markCount > 0) {
		int excludedIndex;

		for (excludedIndex = 0; excludedIndex < markCount; ++excludedIndex) {
			excludedHardpointMap[excludedHardpointIndices[excludedIndex]] = 1;
		}
	}

	for (bestHardpointIndex = 0; bestHardpointIndex < floatHardpointCount; ++bestHardpointIndex) {
		if (craft->componentHp[hardpoints[bestHardpointIndex].componentIndex] &&
			!excludedHardpointMap[bestHardpointIndex]) {
			break;
		}
	}

	if (bestHardpointIndex == floatHardpointCount) {
		return -1;
	}

	hardpoints += bestHardpointIndex;
	hardpointIndex = bestHardpointIndex + 1;
	bestDistanceSq = ((float)hardpoints->x - target.x) * ((float)hardpoints->x - target.x) +
					 ((float)hardpoints->negY - target.y) * ((float)hardpoints->negY - target.y) +
					 ((float)hardpoints->z - target.z) * ((float)hardpoints->z - target.z);

	if (hardpointIndex != floatHardpointCount) {
		for (++hardpoints; hardpointIndex < floatHardpointCount; ++hardpoints, ++hardpointIndex) {
			if (!craft->componentHp[hardpoints->componentIndex] || excludedHardpointMap[hardpointIndex]) {
				continue;
			}

			distanceSq = ((float)hardpoints->x - target.x) * ((float)hardpoints->x - target.x) +
						 ((float)hardpoints->negY - target.y) * ((float)hardpoints->negY - target.y) +
						 ((float)hardpoints->z - target.z) * ((float)hardpoints->z - target.z);
			if (distanceSq < bestDistanceSq) {
				bestDistanceSq = distanceSq;
				bestHardpointIndex = hardpointIndex;
			}
		}
	}

	return bestHardpointIndex;
}

// FUNCTION: XWA 0x4859B0
MeshDescriptor* ModelMesh_GetDescriptor(int modelSlot, int meshIndex) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* root;
	MeshDescriptor* descriptor;

	modelHandle = g_loadedModels.byObjectType[modelSlot];
	if (!modelHandle) {
		return 0;
	}

	if (meshIndex < 0) {
		return 0;
	}

	if (!g_flightRenderToFrontend && !(g_modelTypeTable[modelSlot].assetFlags & 1)) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	Memory_UnlockHandle(g_loadedModels.byObjectType[modelSlot]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	root = rootNodes[meshIndex];
	if (!root) {
		return 0;
	}

	if (root->nodeType == OPT_MESHDESC) {
		return (MeshDescriptor*)root->param2;
	}

	descriptor = 0;
	for (meshIndex = 0; meshIndex < root->childCount; ++meshIndex) {
		if (!root->pChildren[meshIndex]) {
			continue;
		}

		descriptor = ModelMesh_FindDescriptorNodeRecursive(root->pChildren[meshIndex], model);
		if (descriptor) {
			return descriptor;
		}
	}

	return 0;
}

static __inline OptNode* ModelMesh_FindRootVerticesNode(OptNode* rootNode) {
	int childIndex;

	if (rootNode == 0) {
		return 0;
	}
	if (rootNode->nodeType == OPT_MESHVERTS) {
		return rootNode;
	}

	for (childIndex = 0; childIndex < rootNode->childCount; ++childIndex) {
		OptNode* verticesNode;

		if (rootNode->pChildren[childIndex] == 0) {
			continue;
		}

		verticesNode = ModelMesh_FindFirstMeshVertsNode(rootNode->pChildren[childIndex]);
		if (verticesNode != 0) {
			return verticesNode;
		}
	}

	return 0;
}

static __inline OptNode* ModelMesh_FindVerticesForMesh(OptimizedPolyObject* model, int meshIndex) {
	OptNode** rootNodes;

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	return ModelMesh_FindRootVerticesNode(rootNodes[meshIndex]);
}

// FUNCTION: XWA 0x488190
OptNode* ModelMesh_GetVerticesData(int modelType, int meshIndex, Vec3f** outVertices, int* outVertexCount) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* rootNode;
	OptNode* childNode;
	int childIndex;
	int vertexCount;

	*outVertices = 0;
	*outVertexCount = 0;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	rootNode = rootNodes[0];
	if (rootNode->nodeType == OPT_TEXTURE || rootNode->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	rootNode = rootNodes[meshIndex];
	if (rootNode) {
		if (rootNode->nodeType != OPT_MESHVERTS) {
			childIndex = 0;
			if (childIndex < rootNode->childCount) {
				while (1) {
					childNode = rootNode->pChildren[childIndex];
					if (childNode) {
						childNode = ModelMesh_FindFirstMeshVertsNode(childNode);
					}

					if (!childNode) {
						++childIndex;
						if (childIndex < rootNode->childCount) {
							continue;
						}
						rootNode = 0;
					} else {
						rootNode = childNode;
					}
					break;
				}
			} else {
				rootNode = 0;
			}
		}
	} else {
		rootNode = 0;
	}

#ifdef XWA_MODERN
	if (rootNode == 0) {
		return 0;
	}
#endif

	*outVertices = (Vec3f*)rootNode->param2;

	vertexCount = (int)rootNode->param1;
	if (vertexCount > 2) {
		vertexCount -= 2;
	}
	if (vertexCount > 256) {
		vertexCount = 256;
	}
	*outVertexCount = vertexCount;

	return rootNode;
}

static __inline float ModelMesh_BoxAxisGap(float point, float minBound, float maxBound) {
	if (point > maxBound) {
		return point - maxBound;
	}
	if (point < minBound) {
		return minBound - point;
	}
	return flt_5A9A44;
}

// FUNCTION: XWA 0x485B50
int ModelMesh_GetVertexX(int modelType, int meshIndex, int vertexIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* verticesNode;
	int childIndex;
	Vec3f* vertices;
	int vertexCount;
	int result;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	verticesNode = rootNodes[0];
	if (verticesNode->nodeType == OPT_TEXTURE || verticesNode->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	verticesNode = rootNodes[meshIndex];
	if (verticesNode) {
		if (verticesNode->nodeType != OPT_MESHVERTS) {
			childIndex = 0;
			if (childIndex < verticesNode->childCount) {
				while (1) {
					OptNode* childNode;

					childNode = verticesNode->pChildren[childIndex];
					if (childNode) {
						childNode = ModelMesh_FindFirstMeshVertsNode(childNode);
					}

					if (!childNode) {
						++childIndex;
						if (childIndex < verticesNode->childCount) {
							continue;
						}
						verticesNode = 0;
					} else {
						verticesNode = childNode;
					}
					break;
				}
			} else {
				verticesNode = 0;
			}
		}
	} else {
		verticesNode = 0;
	}

	vertices = (Vec3f*)verticesNode->param2;
	vertexCount = (int)verticesNode->param1;
	if (vertexIndex >= vertexCount) {
		vertexIndex = vertexCount - 1;
	}
	result = (int)vertices[vertexIndex].x;

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x485C20
int ModelMesh_GetVertexY(int modelType, int meshIndex, int vertexIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* verticesNode;
	int childIndex;
	Vec3f* vertices;
	int vertexCount;
	int result;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	verticesNode = rootNodes[0];
	if (verticesNode->nodeType == OPT_TEXTURE || verticesNode->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	verticesNode = rootNodes[meshIndex];
	if (verticesNode) {
		if (verticesNode->nodeType != OPT_MESHVERTS) {
			childIndex = 0;
			if (childIndex < verticesNode->childCount) {
				while (1) {
					OptNode* childNode;

					childNode = verticesNode->pChildren[childIndex];
					if (childNode) {
						childNode = ModelMesh_FindFirstMeshVertsNode(childNode);
					}

					if (!childNode) {
						++childIndex;
						if (childIndex < verticesNode->childCount) {
							continue;
						}
						verticesNode = 0;
					} else {
						verticesNode = childNode;
					}
					break;
				}
			} else {
				verticesNode = 0;
			}
		}
	} else {
		verticesNode = 0;
	}

	vertices = (Vec3f*)verticesNode->param2;
	vertexCount = (int)verticesNode->param1;
	if (vertexIndex >= vertexCount) {
		vertexIndex = vertexCount - 1;
	}
	result = (int)vertices[vertexIndex].y;

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x485CF0
int ModelMesh_GetVertexZ(int modelType, int meshIndex, int vertexIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* verticesNode;
	int childIndex;
	Vec3f* vertices;
	int vertexCount;
	int result;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	verticesNode = rootNodes[0];
	if (verticesNode->nodeType == OPT_TEXTURE || verticesNode->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	verticesNode = rootNodes[meshIndex];
	if (verticesNode) {
		if (verticesNode->nodeType != OPT_MESHVERTS) {
			childIndex = 0;
			if (childIndex < verticesNode->childCount) {
				while (1) {
					OptNode* childNode;

					childNode = verticesNode->pChildren[childIndex];
					if (childNode) {
						childNode = ModelMesh_FindFirstMeshVertsNode(childNode);
					}

					if (!childNode) {
						++childIndex;
						if (childIndex < verticesNode->childCount) {
							continue;
						}
						verticesNode = 0;
					} else {
						verticesNode = childNode;
					}
					break;
				}
			} else {
				verticesNode = 0;
			}
		}
	} else {
		verticesNode = 0;
	}

#ifdef XWA_MODERN
	if (verticesNode == 0) {
		Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
		return 0;
	}
#endif

	vertices = (Vec3f*)verticesNode->param2;
	vertexCount = (int)verticesNode->param1;
	if (vertexIndex >= vertexCount) {
		vertexIndex = vertexCount - 1;
	}
	result = (int)vertices[vertexIndex].z;

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x4884D0
int ModelMesh_PickRandomVertex(int modelType, int meshIndex, Vec3f** outVertex) {
	OptimizedPolyObject* model;
	OptNode* verticesNode;
	Vec3f* vertices;
	int vertexCount;
	int vertexIndex;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	{
		OptNode** rootNodes;

		rootNodes = model->rootNodes;
		verticesNode = rootNodes[0];
		if (verticesNode->nodeType == OPT_TEXTURE || verticesNode->nodeType == OPT_TEXTURE_REF) {
			++meshIndex;
		}
		if (meshIndex >= model->rootNodeCount) {
			meshIndex = model->rootNodeCount - 1;
		}
		verticesNode = rootNodes[meshIndex];
		verticesNode = ModelMesh_FindRootVerticesNode(verticesNode);
	}
	vertexCount = (int)verticesNode->param1;
	vertexIndex = (uint16_t)GameRand2() % vertexCount;
	vertices = (Vec3f*)verticesNode->param2;
	if (vertexIndex >= vertexCount) {
		vertexIndex = vertexCount - 1;
	}

	*outVertex = &vertices[vertexIndex];

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return 1;
}

// FUNCTION: XWA 0x487D20
int ModelMesh_FindNearestVertexForPoint(int modelType, int localX, int localY, int localZ, int meshIndex,
										int excludedCount, int* excludedVertexIndices,
										uint8_t* vertexComponentMap, CraftData* craft) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	OptNode* verticesNode;
	Vec3f* vertices;
	int vertexCount;
	int excludedLimit;
	uint8_t excluded[256];
	int nearestIndex;
	float pointX;
	float pointY;
	float pointZ;
	float nearestDistSq;
	int vertexIndex;

	pointX = (float)localX;
	pointY = (float)localY;
	pointZ = (float)localZ;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	modelHandle = g_loadedModels.byObjectType[modelType];
	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);
	verticesNode = ModelMesh_FindVerticesForMesh(model, meshIndex);
	vertices = (Vec3f*)verticesNode->param2;
	vertexCount = (int)verticesNode->param1;
	if (vertexCount > 2) {
		vertexCount -= 2;
	}
	if (vertexCount > 256) {
		vertexCount = 256;
	}

	excludedLimit = excludedCount;
	if (excludedLimit >= vertexCount) {
		excludedLimit = vertexCount - 1;
	}

	Memory_UnlockHandle(modelHandle);

	memset(excluded, 0, (size_t)vertexCount);
	if (excludedLimit > 0) {
		int i;

		for (i = 0; i < excludedLimit; ++i) {
			excluded[excludedVertexIndices[i]] = 1;
		}
	}

	if (vertexComponentMap != NULL) {
		nearestIndex = 0;
		while (nearestIndex < vertexCount) {
			uint8_t component = vertexComponentMap[nearestIndex];
			if (component != 0 && craft->componentHp[component & 0x7f] != 0 && excluded[nearestIndex] == 0) {
				break;
			}
			++nearestIndex;
		}
	} else {
		nearestIndex = 0;
	}

	if (nearestIndex == vertexCount) {
		return -1;
	}

	{
		float dx;
		float dy;
		float dz;

		dx = vertices[nearestIndex].x - pointX;
		dy = vertices[nearestIndex].y - pointY;
		dz = vertices[nearestIndex].z - pointZ;
		nearestDistSq = dx * dx + dy * dy + dz * dz;
	}

	for (vertexIndex = nearestIndex + 1; vertexIndex < vertexCount; ++vertexIndex) {
		if (vertexComponentMap == NULL || vertexComponentMap[vertexIndex] == 0 ||
			(craft->componentHp[vertexComponentMap[vertexIndex] & 0x7f] != 0 && excluded[vertexIndex] == 0)) {
			float dx;
			float dy;
			float dz;
			float distSq;

			dx = vertices[vertexIndex].x - pointX;
			dy = vertices[vertexIndex].y - pointY;
			dz = vertices[vertexIndex].z - pointZ;
			distSq = dx * dx + dy * dy + dz * dz;
			if (distSq < nearestDistSq) {
				nearestDistSq = distSq;
				nearestIndex = vertexIndex;
			}
		}
	}

	return nearestIndex;
}

// FUNCTION: XWA 0x485DC0
int ModelMesh_GetCenterX(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int result;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		result = (int)descriptor->center.x;
	} else {
		result = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x485E90
int ModelMesh_GetCenterY(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int result;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		result = (int)descriptor->center.y;
	} else {
		result = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x485F60
int ModelMesh_GetCenterZ(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int result;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		result = (int)descriptor->center.z;
	} else {
		result = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x4866F0
int ModelMesh_GetComponentFocusX(int objectType, int componentIdx) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int value;

	if (componentIdx < 0) {
		return 0;
	}
	if ((g_modelTypeTable[objectType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[objectType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	{
		OptNode* firstRootNode = rootNodes[0];

		if (firstRootNode->nodeType == OPT_TEXTURE || firstRootNode->nodeType == OPT_TEXTURE_REF) {
			++componentIdx;
		}
	}
	if (componentIdx >= model->rootNodeCount) {
		componentIdx = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[componentIdx], model);
	if (descriptor != 0) {
		if (descriptor->targetId) {
			value = (int)descriptor->target.x;
		} else {
			value = (int)descriptor->center.x;
		}
	} else {
		value = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[objectType]);
	return value;
}

// FUNCTION: XWA 0x4867D0
int ModelMesh_GetComponentFocusY(int objectType, int componentIdx) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int value;

	if (componentIdx < 0) {
		return 0;
	}
	if ((g_modelTypeTable[objectType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[objectType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	{
		OptNode* firstRootNode = rootNodes[0];

		if (firstRootNode->nodeType == OPT_TEXTURE || firstRootNode->nodeType == OPT_TEXTURE_REF) {
			++componentIdx;
		}
	}
	if (componentIdx >= model->rootNodeCount) {
		componentIdx = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[componentIdx], model);
	if (descriptor != 0) {
		if (descriptor->targetId) {
			value = (int)descriptor->target.y;
		} else {
			value = (int)descriptor->center.y;
		}
	} else {
		value = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[objectType]);
	return value;
}

// FUNCTION: XWA 0x4868B0
int ModelMesh_GetComponentFocusZ(int objectType, int componentIdx) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int value;

	if (componentIdx < 0) {
		return 0;
	}
	if ((g_modelTypeTable[objectType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[objectType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	{
		OptNode* firstRootNode = rootNodes[0];

		if (firstRootNode->nodeType == OPT_TEXTURE || firstRootNode->nodeType == OPT_TEXTURE_REF) {
			++componentIdx;
		}
	}
	if (componentIdx >= model->rootNodeCount) {
		componentIdx = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[componentIdx], model);
	if (descriptor != 0) {
		if (descriptor->targetId) {
			value = (int)descriptor->target.z;
		} else {
			value = (int)descriptor->center.z;
		}
	} else {
		value = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[objectType]);
	return value;
}

// FUNCTION: XWA 0x486990
int ModelMesh_GetComponentMaxExtent(int objectType, int componentIdx) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* firstRootNode;
	MeshDescriptor* descriptor;
	int extentX;
	int extentY;
	int extentZ;

	if (componentIdx < 0) {
		return 0;
	}
	if ((g_modelTypeTable[objectType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[objectType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	firstRootNode = rootNodes[0];
	if (firstRootNode->nodeType == OPT_TEXTURE || firstRootNode->nodeType == OPT_TEXTURE_REF) {
		++componentIdx;
	}
	if (componentIdx >= model->rootNodeCount) {
		componentIdx = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[componentIdx], model);
	if (descriptor != 0) {
		extentX = (int)descriptor->span.x;
		extentY = (int)descriptor->span.y;
		extentZ = (int)descriptor->span.z;

		if (extentY >= extentX && extentY >= extentZ) {
			extentX = extentY;
		} else if (extentZ >= extentX && extentZ >= extentY) {
			extentX = extentZ;
		}
	} else {
		extentX = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[objectType]);
	return extentX;
}

// FUNCTION: XWA 0x486030
int ModelMesh_GetBoundsMinX(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int result;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		result = (int)descriptor->boxMin.x;
	} else {
		result = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x486100
int ModelMesh_GetBoundsMinY(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int result;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		result = (int)descriptor->boxMin.y;
	} else {
		result = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x4861D0
int ModelMesh_GetBoundsMinZ(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int result;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		result = (int)descriptor->boxMin.z;
	} else {
		result = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x4862A0
int ModelMesh_GetBoundsMaxX(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int result;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		result = (int)descriptor->boxMax.x;
	} else {
		result = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x486370
int ModelMesh_GetBoundsMaxY(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int result;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		result = (int)descriptor->boxMax.y;
	} else {
		result = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x486440
int ModelMesh_GetBoundsMaxZ(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int result;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		result = (int)descriptor->boxMax.z;
	} else {
		result = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x486510
int ModelMesh_GetBoundsSizeX(int modelType, int meshIndex) {
	return ModelMesh_GetBoundsMaxX(modelType, meshIndex) - ModelMesh_GetBoundsMinX(modelType, meshIndex);
}

// FUNCTION: XWA 0x486540
int ModelMesh_GetBoundsSizeY(int modelType, int meshIndex) {
	return ModelMesh_GetBoundsMaxY(modelType, meshIndex) - ModelMesh_GetBoundsMinY(modelType, meshIndex);
}

// FUNCTION: XWA 0x486570
int ModelMesh_GetBoundsSizeZ(int modelType, int meshIndex) {
	return ModelMesh_GetBoundsMaxZ(modelType, meshIndex) - ModelMesh_GetBoundsMinZ(modelType, meshIndex);
}

// FUNCTION: XWA 0x4865A0
int ModelMesh_GetBoundsVolume(int modelType, int meshIndex) {
	int sizeX = ModelMesh_GetBoundsMaxX(modelType, meshIndex) - ModelMesh_GetBoundsMinX(modelType, meshIndex);
	int sizeY = ModelMesh_GetBoundsMaxY(modelType, meshIndex) - ModelMesh_GetBoundsMinY(modelType, meshIndex);
	int maxZ = ModelMesh_GetBoundsMaxZ(modelType, meshIndex);
	int sizeZ = maxZ - ModelMesh_GetBoundsMinZ(modelType, meshIndex);
	return sizeX * sizeY * sizeZ;
}

// FUNCTION: XWA 0x487B20
int ModelMesh_FindNearestByBounds(int modelType, int localX, int localY, int localZ) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	float pointX;
	float pointY;
	float pointZ;
	float bestDistance;
	int bestRootIndex;
	int rootIndex;

	pointX = (float)localX;
	pointY = (float)localY;
	pointZ = (float)localZ;
	bestDistance = 2147483600.0f;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	modelHandle = g_loadedModels.byObjectType[modelType];
	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	bestRootIndex = localY;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* rootNode;
		MeshDescriptor* descriptor;
		float distance;
		float axisGap;

		rootNode = model->rootNodes[rootIndex];
		if (rootNode->nodeType == OPT_TEXTURE || rootNode->nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		descriptor = ModelMesh_FindRootDescriptor(rootNode, model);
		if (descriptor == NULL) {
			continue;
		}

		distance = ModelMesh_BoxAxisGap(pointX, descriptor->boxMin.x, descriptor->boxMax.x);
		axisGap = ModelMesh_BoxAxisGap(pointY, descriptor->boxMin.y, descriptor->boxMax.y);
		if (axisGap > distance) {
			distance = axisGap;
		}
		axisGap = ModelMesh_BoxAxisGap(pointZ, descriptor->boxMin.z, descriptor->boxMax.z);
		if (axisGap > distance) {
			distance = axisGap;
		}

		if (distance < bestDistance) {
			bestRootIndex = rootIndex;
			bestDistance = distance;
			if (distance == 0.0f) {
				break;
			}
		}
	}

	if (model->rootNodes[0]->nodeType == OPT_TEXTURE || model->rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		--bestRootIndex;
	}

	Memory_UnlockHandle(modelHandle);
	return bestRootIndex;
}

// FUNCTION: XWA 0x4873B0
int ModelMesh_FindNearestLiveMainHullByBounds(int modelType, int localX, int localY, int localZ,
											  CraftData* craft) {
	OptimizedPolyObject* model;
	float bestDistance;
	Vec3f point;
	int bestRootIndex;
	int rootIndex;

	point.x = (float)localX;
	point.y = (float)localY;
	point.z = (float)localZ;
	bestDistance = 2147483600.0f;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	bestRootIndex = 0;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* rootNode;
		OptNodeType nodeType;
		MeshDescriptor* descriptor;
		uint8_t* vertexComponentMap;
		int vertexCount;
		int vertexIndex;
		int componentMapMeshIndex;
		float distance;
		float axisGap;

		rootNode = model->rootNodes[rootIndex];
		nodeType = rootNode->nodeType;
		if (nodeType == OPT_TEXTURE || nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		descriptor = ModelMesh_FindRootDescriptor(rootNode, model);

		if (descriptor == NULL || descriptor->meshType != MESH_MainHull) {
			continue;
		}

		componentMapMeshIndex = rootIndex;
		if (model->rootNodes[0]->nodeType == OPT_TEXTURE || rootNode->nodeType == OPT_TEXTURE_REF) {
			--componentMapMeshIndex;
		}
		vertexComponentMap = FeDiskIo_GetMeshVertexComponentMap(modelType, componentMapMeshIndex);

		rootNode = ModelMesh_FindRootVerticesNode(rootNode);

		vertexCount = (int)rootNode->param1;
		if (vertexCount > 2) {
			vertexCount -= 2;
		}
		if (vertexCount > 256) {
			vertexCount = 256;
		}

		for (vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
			uint8_t component = vertexComponentMap[vertexIndex];

			if (component != 0 && craft->componentHp[component & 0x7f] != 0) {
				break;
			}
		}
		if (vertexIndex == vertexCount) {
			continue;
		}

		if (point.x > descriptor->boxMax.x) {
			distance = point.x - descriptor->boxMax.x;
		} else if (point.x < descriptor->boxMin.x) {
			distance = descriptor->boxMin.x - point.x;
		} else {
			distance = flt_5A9A44;
		}
		if (point.y > descriptor->boxMax.y) {
			axisGap = point.y - descriptor->boxMax.y;
		} else if (point.y < descriptor->boxMin.y) {
			axisGap = descriptor->boxMin.y - point.y;
		} else {
			axisGap = flt_5A9A44;
		}
		if (axisGap > distance) {
			distance = axisGap;
		}
		if (point.z > descriptor->boxMax.z) {
			axisGap = point.z - descriptor->boxMax.z;
		} else if (point.z < descriptor->boxMin.z) {
			axisGap = descriptor->boxMin.z - point.z;
		} else {
			axisGap = flt_5A9A44;
		}
		if (axisGap > distance) {
			distance = axisGap;
		}

		if (distance < bestDistance) {
			bestDistance = distance;
			bestRootIndex = rootIndex;
			if (distance == flt_5A9A44) {
				break;
			}
		}
	}

	if (model->rootNodes[0]->nodeType == OPT_TEXTURE || model->rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		--bestRootIndex;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return bestRootIndex;
}

// FUNCTION: XWA 0x487680
int ModelMesh_FindFloatHardpointComponent(int modelType, int localX, int localY, int localZ) {
	OptimizedPolyObject* model;
	Vec3f point;
	float bestDistanceSq;
	int bestRootIndex;
	int rootIndex;

	point.x = (float)localX;
	point.y = (float)localY;
	point.z = (float)localZ;
	bestDistanceSq = 2147483600.0f;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	bestRootIndex = 0;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* rootNode;
		OptNodeType nodeType;
		MeshDescriptor* descriptor;

		rootNode = model->rootNodes[rootIndex];
		nodeType = rootNode->nodeType;
		if (nodeType == OPT_TEXTURE || nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		descriptor = ModelMesh_FindRootDescriptor(rootNode, model);

		if (descriptor != NULL && descriptor->meshType == MESH_MainHull) {
			bestRootIndex = rootIndex;
			break;
		}
	}

	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* rootNode;
		OptNodeType nodeType;
		MeshDescriptor* descriptor;
		float dx;
		float dy;
		float dz;
		float distanceSq;

		rootNode = model->rootNodes[rootIndex];
		nodeType = rootNode->nodeType;
		if (nodeType == OPT_TEXTURE || nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		descriptor = ModelMesh_FindRootDescriptor(rootNode, model);

		if (descriptor == NULL) {
			continue;
		}
		if (descriptor->meshType != MESH_WeaponSystem1 && descriptor->meshType != MESH_WeaponSystem2) {
			continue;
		}

		if (point.x > descriptor->boxMax.x) {
			dx = point.x - descriptor->boxMax.x;
		} else if (point.x < descriptor->boxMin.x) {
			dx = descriptor->boxMin.x - point.x;
		} else {
			dx = flt_5A9A44;
		}
		distanceSq = dx * dx;
		if (point.y > descriptor->boxMax.y) {
			dy = point.y - descriptor->boxMax.y;
		} else if (point.y < descriptor->boxMin.y) {
			dy = descriptor->boxMin.y - point.y;
		} else {
			dy = flt_5A9A44;
		}
		distanceSq += dy * dy;
		if (point.z > descriptor->boxMax.z) {
			dz = point.z - descriptor->boxMax.z;
		} else if (point.z < descriptor->boxMin.z) {
			dz = descriptor->boxMin.z - point.z;
		} else {
			dz = flt_5A9A44;
		}
		distanceSq += dz * dz;
		if (distanceSq < bestDistanceSq) {
			bestDistanceSq = distanceSq;
			bestRootIndex = rootIndex;
			if (distanceSq == flt_5A9A44) {
				break;
			}
		}
	}

	{
		OptNode* firstRootNode;

		firstRootNode = model->rootNodes[0];
		if (firstRootNode->nodeType == OPT_TEXTURE || firstRootNode->nodeType == OPT_TEXTURE_REF) {
			--bestRootIndex;
		}
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return bestRootIndex;
}

// FUNCTION: XWA 0x4878F0
int ModelMesh_FindNearestLiveComponentByType(int modelType, MeshType meshTypeFilter, int localX, int localY,
											 int localZ, CraftData* craft) {
	OptimizedPolyObject* model;
	Vec3f point;
	float bestDistanceSq;
	int rootIndex;

	point.x = (float)localX;
	point.y = (float)localY;
	point.z = (float)localZ;
	bestDistanceSq = 4.611686e18f;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	localX = -1;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* rootNode;
		MeshDescriptor* descriptor;
		OptNodeType nodeType;
		int childIndex;
		int componentIndex;
		float dx;
		float dy;
		float dz;
		float distanceSq;

		rootNode = model->rootNodes[rootIndex];
		nodeType = rootNode->nodeType;
		if (nodeType == OPT_TEXTURE || nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		descriptor = ModelMesh_FindRootDescriptor(rootNode, model);

		if (descriptor == 0) {
			continue;
		}
		if (meshTypeFilter != MESH_Default && descriptor->meshType != meshTypeFilter) {
			continue;
		}

		if (model->rootNodes[0]->nodeType == OPT_TEXTURE || rootNode->nodeType == OPT_TEXTURE_REF) {
			componentIndex = rootIndex - 1;
		} else {
			componentIndex = rootIndex;
		}
		if (craft->componentHp[componentIndex] == 0) {
			continue;
		}

		if (point.x > descriptor->boxMax.x) {
			dx = point.x - descriptor->boxMax.x;
		} else if (point.x < descriptor->boxMin.x) {
			dx = descriptor->boxMin.x - point.x;
		} else {
			dx = flt_5A9A44;
		}
		distanceSq = dx * dx;
		if (point.y > descriptor->boxMax.y) {
			dy = point.y - descriptor->boxMax.y;
		} else if (point.y < descriptor->boxMin.y) {
			dy = descriptor->boxMin.y - point.y;
		} else {
			dy = flt_5A9A44;
		}
		distanceSq += dy * dy;
		if (point.z > descriptor->boxMax.z) {
			dz = point.z - descriptor->boxMax.z;
		} else if (point.z < descriptor->boxMin.z) {
			dz = descriptor->boxMin.z - point.z;
		} else {
			dz = flt_5A9A44;
		}
		distanceSq += dz * dz;
		if (distanceSq < bestDistanceSq) {
			bestDistanceSq = distanceSq;
			localX = rootIndex;
			if (distanceSq == flt_5A9A44) {
				break;
			}
		}
	}

	{
		OptNode* firstRootNode;

		firstRootNode = model->rootNodes[0];
		if ((firstRootNode->nodeType == OPT_TEXTURE || firstRootNode->nodeType == OPT_TEXTURE_REF) &&
			localX != -1) {
			--localX;
		}
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return localX;
}

// FUNCTION: XWA 0x4872D0
int ModelMesh_HasFuselage(int modelType) {
	OptimizedPolyObject* model;
	int rootIndex;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* rootNode;
		MeshDescriptor* descriptor;

		rootNode = model->rootNodes[rootIndex];
		if (rootNode == NULL || rootNode->nodeType == OPT_TEXTURE || rootNode->nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		descriptor = ModelMesh_FindRootDescriptor(rootNode, model);
		if (descriptor != NULL && descriptor->meshType == MESH_Fuselage) {
			Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
			return 1;
		}
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return 0;
}

// FUNCTION: XWA 0x486620
int ModelMesh_GetTargetId(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* firstRootNode;
	MeshDescriptor* descriptor;
	int targetId;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	firstRootNode = rootNodes[0];
	if (firstRootNode->nodeType == OPT_TEXTURE || firstRootNode->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		targetId = descriptor->targetId;
	} else {
		targetId = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return targetId;
}

// FUNCTION: XWA 0x486A90
int ModelMesh_IsObjectTypeMeshDamageable(int objectType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int flags;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[objectType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[objectType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		flags = descriptor->explosionType & MESH_EXPLOSION_TYPE2;
	} else {
		flags = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[objectType]);
	return flags;
}

// FUNCTION: XWA 0x486B60
int ModelMesh_HasExplosionType1(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	int flags;

	if (meshIndex < 0) {
		return 0;
	}

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor != 0) {
		flags = descriptor->explosionType & MESH_EXPLOSION_TYPE1;
	} else {
		flags = 0;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return flags;
}

// FUNCTION: XWA 0x485A70
MeshType ModelMesh_GetType(int modelSlot, int meshIndex) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	MeshDescriptor* descriptor;
	MeshType meshType;

	modelHandle = g_loadedModels.byObjectType[modelSlot];
	if (!modelHandle) {
		return MESH_Default;
	}

	if (meshIndex < 0) {
		return MESH_Default;
	}

	if (!g_flightRenderToFrontend && !(g_modelTypeTable[modelSlot].assetFlags & 1)) {
		return MESH_Default;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	descriptor = ModelMesh_FindRootDescriptor(rootNodes[meshIndex], model);
	if (descriptor) {
		meshType = descriptor->meshType;
	} else {
		meshType = MESH_Default;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelSlot]);
	return meshType;
}

// FUNCTION: XWA 0x488A00
MeshType ModelMesh_GetObjectTypeMeshType(int objectType, int meshIndex) {
	int meshCount;

	if (objectType < OBJ_Count) {
		if (meshIndex < 0) {
			return MESH_Default;
		}

		meshCount = g_objectTypeMeshCache[objectType].meshCount;
		if (meshIndex >= meshCount) {
			meshIndex = meshCount - 1;
		}
		return g_objectTypeMeshCache[objectType].meshTypes[meshIndex];
	}

	return ModelMesh_GetType(objectType, meshIndex);
}

// FUNCTION: XWA 0x486D60
int ModelMesh_CountHardpointNodesRecursive(OptNode* node, OptimizedPolyObject* optBase) {
	OptNode* resolvedNode;
	int count;
	int i;

	resolvedNode = OptModel_ResolveNodeRef(node, optBase);
	if (!resolvedNode) {
		return 0;
	}

	count = 0;
	if (resolvedNode->nodeType == OPT_HARDPOINT) {
		count = 1;
	}

	for (i = 0; i < resolvedNode->childCount; ++i) {
		count += ModelMesh_CountHardpointNodesRecursive(resolvedNode->pChildren[i], optBase);
	}

	return count;
}

// FUNCTION: XWA 0x486DC0
int ModelMesh_CountHardpoints(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* rootNode;
	int count;
	int childIndex;

	if (!g_flightRenderToFrontend && !(g_modelTypeTable[modelType].assetFlags & 1)) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	rootNode = OptModel_ResolveNodeRef(rootNodes[meshIndex], model);
	if (!rootNode) {
		count = 0;
	} else {
		count = 0;
		if (rootNode->nodeType == OPT_HARDPOINT) {
			count = 1;
		}
		for (childIndex = 0; childIndex < rootNode->childCount; ++childIndex) {
			count += ModelMesh_CountHardpointNodesRecursive(rootNode->pChildren[childIndex], model);
		}
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return count;
}

// FUNCTION: XWA 0x486E90
int ModelMesh_GetAlternateHardpointIndex(int modelType, int meshIndex, int hardpointIndex) {
	(void)modelType;
	(void)meshIndex;

	return hardpointIndex;
}

// FUNCTION: XWA 0x486CF0
OptNode* ModelMesh_FindNthHardpointNodeRecursive(OptNode* node, OptimizedPolyObject* optBase,
												 int hardpointIndex) {
	OptNode* resolvedNode;
	int i;

	resolvedNode = OptModel_ResolveNodeRef(node, optBase);
	if (!resolvedNode) {
		return 0;
	}

	if (resolvedNode->nodeType == OPT_HARDPOINT) {
		if (g_optHardpointSearchIndex == hardpointIndex) {
			return resolvedNode;
		}
		++g_optHardpointSearchIndex;
	}

	for (i = 0; i < resolvedNode->childCount; ++i) {
		OptNode* hardpointNode;

		hardpointNode =
			ModelMesh_FindNthHardpointNodeRecursive(resolvedNode->pChildren[i], optBase, hardpointIndex);
		if (hardpointNode) {
			return hardpointNode;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x486EA0
int ModelMesh_GetHardpointX(int modelType, int meshIndex, int hardpointIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* rootNode;
	OptNode* hardpointNode;
	int result;
	int childIndex;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	rootNode = rootNodes[0];
	if (rootNode->nodeType == OPT_TEXTURE || rootNode->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	rootNode = rootNodes[meshIndex];
	g_optHardpointSearchIndex = 0;
	rootNode = OptModel_ResolveNodeRef(rootNode, model);
	hardpointNode = rootNode;
	if (rootNode != 0) {
		switch (rootNode->nodeType) {
			case OPT_HARDPOINT:
				if (g_optHardpointSearchIndex == hardpointIndex) {
					hardpointNode = rootNode;
					break;
				}
				++g_optHardpointSearchIndex;
				/* fall through */

			default:
				childIndex = 0;
				hardpointNode = 0;
				for (; childIndex < rootNode->childCount; ++childIndex) {
					hardpointNode = ModelMesh_FindNthHardpointNodeRecursive(rootNode->pChildren[childIndex],
																			model, hardpointIndex);
					if (hardpointNode != 0) {
						break;
					}
				}
				break;
		}
	}
	if (hardpointNode == 0) {
		result = 0;
	} else {
		const OptHardpoint* hardpoint;

		hardpoint = (const OptHardpoint*)hardpointNode->param2;
		result = (int)hardpoint->position.x;
	}

	{
		uint16_t modelHandle;

		modelHandle = g_loadedModels.byObjectType[modelType];
		Memory_UnlockHandle(modelHandle);
	}
	return result;
}

// FUNCTION: XWA 0x486F90
int ModelMesh_GetHardpointY(int modelType, int meshIndex, int hardpointIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* rootNode;
	OptNode* hardpointNode;
	int result;
	int childIndex;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	rootNode = rootNodes[0];
	if (rootNode->nodeType == OPT_TEXTURE || rootNode->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	rootNode = rootNodes[meshIndex];
	g_optHardpointSearchIndex = 0;
	rootNode = OptModel_ResolveNodeRef(rootNode, model);
	hardpointNode = rootNode;
	if (rootNode != 0) {
		switch (rootNode->nodeType) {
			case OPT_HARDPOINT:
				if (g_optHardpointSearchIndex == hardpointIndex) {
					hardpointNode = rootNode;
					break;
				}
				++g_optHardpointSearchIndex;
				/* fall through */

			default:
				childIndex = 0;
				hardpointNode = 0;
				for (; childIndex < rootNode->childCount; ++childIndex) {
					hardpointNode = ModelMesh_FindNthHardpointNodeRecursive(rootNode->pChildren[childIndex],
																			model, hardpointIndex);
					if (hardpointNode != 0) {
						break;
					}
				}
				break;
		}
	}
	if (hardpointNode == 0) {
		result = 0;
	} else {
		const OptHardpoint* hardpoint;

		hardpoint = (const OptHardpoint*)hardpointNode->param2;
		result = (int)hardpoint->position.y;
	}

	{
		uint16_t modelHandle;

		modelHandle = g_loadedModels.byObjectType[modelType];
		Memory_UnlockHandle(modelHandle);
	}
	return -result;
}

// FUNCTION: XWA 0x487090
int ModelMesh_GetHardpointZ(int modelType, int meshIndex, int hardpointIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* rootNode;
	OptNode* hardpointNode;
	int result;
	int childIndex;

	if ((g_modelTypeTable[modelType].assetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	rootNode = rootNodes[0];
	if (rootNode->nodeType == OPT_TEXTURE || rootNode->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	rootNode = rootNodes[meshIndex];
	g_optHardpointSearchIndex = 0;
	rootNode = OptModel_ResolveNodeRef(rootNode, model);
	hardpointNode = rootNode;
	if (rootNode != 0) {
		switch (rootNode->nodeType) {
			case OPT_HARDPOINT:
				if (g_optHardpointSearchIndex == hardpointIndex) {
					hardpointNode = rootNode;
					break;
				}
				++g_optHardpointSearchIndex;
				/* fall through */

			default:
				childIndex = 0;
				hardpointNode = 0;
				for (; childIndex < rootNode->childCount; ++childIndex) {
					hardpointNode = ModelMesh_FindNthHardpointNodeRecursive(rootNode->pChildren[childIndex],
																			model, hardpointIndex);
					if (hardpointNode != 0) {
						break;
					}
				}
				break;
		}
	}
	if (hardpointNode == 0) {
		result = 0;
	} else {
		const OptHardpoint* hardpoint;

		hardpoint = (const OptHardpoint*)hardpointNode->param2;
		result = (int)hardpoint->position.z;
	}

	{
		uint16_t modelHandle;

		modelHandle = g_loadedModels.byObjectType[modelType];
		Memory_UnlockHandle(modelHandle);
	}
	return result;
}

// FUNCTION: XWA 0x487180
void ModelMesh_GetHardpoint(int modelType, int meshIndex, int hardpointIndex, OptHardpointType* outType,
							int* outX, int* outY, int* outZ) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* rootNode;
	OptNode* hardpointNode;
	int childIndex;

	if (!g_flightRenderToFrontend && !(g_modelTypeTable[modelType].assetFlags & 1)) {
		return;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	g_optHardpointSearchIndex = 0;
	rootNode = OptModel_ResolveNodeRef(rootNodes[meshIndex], model);
	hardpointNode = 0;
	if (rootNode) {
		if (rootNode->nodeType == OPT_HARDPOINT) {
			if (g_optHardpointSearchIndex == hardpointIndex) {
				hardpointNode = rootNode;
			} else {
				++g_optHardpointSearchIndex;
			}
		}

		for (childIndex = 0; !hardpointNode && childIndex < rootNode->childCount; ++childIndex) {
			hardpointNode = ModelMesh_FindNthHardpointNodeRecursive(rootNode->pChildren[childIndex], model,
																	hardpointIndex);
		}
	}

	if (!hardpointNode) {
		*outType = OPT_HARDPOINT_None;
		*outX = 0;
		*outY = 0;
		*outZ = 0;
	} else {
		const OptHardpoint* hardpoint;

		hardpoint = (const OptHardpoint*)hardpointNode->param2;
		*outType = hardpoint->hardpointType;
		*outX = (int)hardpoint->position.x;
		*outY = -(int)hardpoint->position.y;
		*outZ = (int)hardpoint->position.z;
	}

	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
}

// FUNCTION: XWA 0x488620
int ModelMesh_CountEngineGlowNodesRecursive(OptNode* node, OptimizedPolyObject* model) {
	OptNode* resolvedNode;
	int count;
	int i;

	resolvedNode = OptModel_ResolveNodeRef(node, model);
	if (!resolvedNode) {
		return 0;
	}

	count = 0;
	if (resolvedNode->nodeType == OPT_ENGINEGLOW) {
		count = 1;
	}

	for (i = 0; i < resolvedNode->childCount; ++i) {
		count += ModelMesh_CountEngineGlowNodesRecursive(resolvedNode->pChildren[i], model);
	}

	return count;
}

// FUNCTION: XWA 0x488680
int ModelMesh_CountEngineGlows(int modelType, int meshIndex) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* rootNode;
	int count;
	int childIndex;

	if (g_flightRenderToFrontend) {
		modelHandle = g_loadedModels.byObjectType[0];
		if (!modelHandle) {
			return 0;
		}
		model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	} else {
		if (!(g_modelTypeTable[modelType].assetFlags & 1)) {
			return 0;
		}
		modelHandle = g_loadedModels.byObjectType[modelType];
		model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	}

	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	rootNode = OptModel_ResolveNodeRef(rootNodes[meshIndex], model);
	if (!rootNode) {
		count = 0;
	} else {
		count = 0;
		if (rootNode->nodeType == OPT_ENGINEGLOW) {
			count = 1;
		}
		for (childIndex = 0; childIndex < rootNode->childCount; ++childIndex) {
			count += ModelMesh_CountEngineGlowNodesRecursive(rootNode->pChildren[childIndex], model);
		}
	}

	if (g_flightRenderToFrontend) {
		Memory_UnlockHandle(g_loadedModels.byObjectType[0]);
	} else {
		Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	}

	return count;
}

// FUNCTION: XWA 0x4885B0
OptNode* ModelMesh_FindNthEngineGlowNodeRecursive(OptNode* node, OptimizedPolyObject* model,
												  int engineGlowIndex) {
	OptNode* resolvedNode;
	int i;

	resolvedNode = OptModel_ResolveNodeRef(node, model);
	if (!resolvedNode) {
		return 0;
	}

	if (resolvedNode->nodeType == OPT_ENGINEGLOW) {
		if (g_optEngineGlowSearchIndex == engineGlowIndex) {
			return resolvedNode;
		}
		++g_optEngineGlowSearchIndex;
	}

	for (i = 0; i < resolvedNode->childCount; ++i) {
		OptNode* engineGlowNode;

		engineGlowNode =
			ModelMesh_FindNthEngineGlowNodeRecursive(resolvedNode->pChildren[i], model, engineGlowIndex);
		if (engineGlowNode) {
			return engineGlowNode;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x488790
OptEngineGlow* ModelMesh_GetEngineGlowParam(int modelType, int meshIndex, int engineGlowIndex) {
	uint16_t modelHandle;
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* rootNode;
	OptNode* engineGlowNode;
	OptEngineGlow* param;
	int childIndex;

	param = 0;
	if (g_flightRenderToFrontend) {
		modelHandle = g_loadedModels.byObjectType[0];
		if (modelHandle) {
			model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);

			OptModel_AdjustOptimizedPolyObjectPointers(model);

			rootNodes = model->rootNodes;
			if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
				++meshIndex;
			}
			if (meshIndex >= model->rootNodeCount) {
				meshIndex = model->rootNodeCount - 1;
			}

			rootNode = rootNodes[meshIndex];
			g_optEngineGlowSearchIndex = 0;
			rootNode = OptModel_ResolveNodeRef(rootNode, model);
			if (!rootNode) {
				engineGlowNode = 0;
			} else {
				if (rootNode->nodeType == OPT_ENGINEGLOW) {
					if (g_optEngineGlowSearchIndex == engineGlowIndex) {
						engineGlowNode = rootNode;
					} else {
						++g_optEngineGlowSearchIndex;
					}
				}

				for (childIndex = 0; childIndex < rootNode->childCount; ++childIndex) {
					engineGlowNode = ModelMesh_FindNthEngineGlowNodeRecursive(rootNode->pChildren[childIndex],
																			  model, engineGlowIndex);
					if (engineGlowNode) {
						break;
					}
				}
			}

			if (engineGlowNode) {
				param = (OptEngineGlow*)engineGlowNode->param2;
			}

			Memory_UnlockHandle(g_loadedModels.byObjectType[0]);
			return param;
		}
	} else if (g_modelTypeTable[modelType].assetFlags & 1) {
		modelHandle = g_loadedModels.byObjectType[modelType];
		model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);

		OptModel_AdjustOptimizedPolyObjectPointers(model);

		rootNodes = model->rootNodes;
		if (rootNodes[0]->nodeType == OPT_TEXTURE || rootNodes[0]->nodeType == OPT_TEXTURE_REF) {
			++meshIndex;
		}
		if (meshIndex >= model->rootNodeCount) {
			meshIndex = model->rootNodeCount - 1;
		}

		rootNode = rootNodes[meshIndex];
		g_optEngineGlowSearchIndex = 0;
		rootNode = OptModel_ResolveNodeRef(rootNode, model);
		if (!rootNode) {
			engineGlowNode = 0;
		} else {
			if (rootNode->nodeType == OPT_ENGINEGLOW) {
				if (g_optEngineGlowSearchIndex == engineGlowIndex) {
					engineGlowNode = rootNode;
				} else {
					++g_optEngineGlowSearchIndex;
				}
			}

			for (childIndex = 0; childIndex < rootNode->childCount; ++childIndex) {
				engineGlowNode = ModelMesh_FindNthEngineGlowNodeRecursive(rootNode->pChildren[childIndex],
																		  model, engineGlowIndex);
				if (engineGlowNode) {
					break;
				}
			}
		}

		if (engineGlowNode) {
			param = (OptEngineGlow*)engineGlowNode->param2;
		}

		Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	}
	return param;
}

// FUNCTION: XWA 0x488330
int ModelMesh_FindBridgeIndex(OptimizedPolyObject* model) {
	int rootIndex;
	int meshIndex;

	meshIndex = 0;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* rootNode;
		OptNodeType nodeType;
		MeshDescriptor* descriptor;

		rootNode = model->rootNodes[rootIndex];
		nodeType = rootNode->nodeType;
		if (nodeType == OPT_TEXTURE || nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		if (!rootNode) {
			descriptor = 0;
		} else if (nodeType == OPT_MESHDESC) {
			descriptor = (MeshDescriptor*)rootNode->param2;
		} else {
			int childIndex;

			childIndex = 0;
			if (rootNode->childCount <= 0) {
				descriptor = 0;
			} else {
				while (1) {
					OptNode* childNode;

					childNode = rootNode->pChildren[childIndex];
					if (childNode) {
						descriptor = ModelMesh_FindDescriptorNodeRecursive(childNode, model);
						if (descriptor) {
							break;
						}
					}

					++childIndex;
					if (childIndex >= rootNode->childCount) {
						descriptor = 0;
						break;
					}
				}
			}
		}

		if (descriptor && descriptor->meshType == MESH_Bridge) {
			break;
		}
		++meshIndex;
	}

	if (rootIndex < model->rootNodeCount) {
		return meshIndex;
	}
	return -1;
}

// FUNCTION: XWA 0x4852C0
OptNode* ModelMesh_FindFirstRotScaleNode(OptNode* node) {
	int childIndex;

	if (!node) {
		return 0;
	}
	if (node->nodeType == OPT_ROTSCALE) {
		return node;
	}

	childIndex = 0;
	if (node->childCount <= 0) {
		return 0;
	}
	while (1) {
		if (node->pChildren[childIndex]) {
			OptNode* result;

			result = ModelMesh_FindFirstRotScaleNode(node->pChildren[childIndex]);
			if (result) {
				return result;
			}
		}

		++childIndex;
		if (childIndex >= node->childCount) {
			return 0;
		}
	}
}

// FUNCTION: XWA 0x486C30
OptRotationScale* ModelMesh_GetRotScaleData(int modelType, int meshIndex) {
	OptimizedPolyObject* model;
	OptNode** rootNodes;
	OptNode* rootNode;
	OptRotationScale* result;
	int childIndex;

	if (!(g_modelTypeTable[modelType].assetFlags & 1)) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	rootNodes = model->rootNodes;
	rootNode = rootNodes[0];
	if (rootNode->nodeType == OPT_TEXTURE || rootNode->nodeType == OPT_TEXTURE_REF) {
		++meshIndex;
	}
	if (meshIndex >= model->rootNodeCount) {
		meshIndex = model->rootNodeCount - 1;
	}

	rootNode = rootNodes[meshIndex];
	if (rootNode) {
		if (rootNode->nodeType != OPT_ROTSCALE) {
			OptNode* foundNode;

			childIndex = 0;
			if (childIndex < rootNode->childCount) {
				while (1) {
					foundNode = rootNode->pChildren[childIndex];
					if (foundNode) {
						foundNode = ModelMesh_FindFirstRotScaleNode(foundNode);
					}

					if (!foundNode) {
						++childIndex;
						if (childIndex < rootNode->childCount) {
							continue;
						}
						rootNode = 0;
					} else {
						rootNode = foundNode;
					}
					break;
				}
			} else {
				rootNode = 0;
			}
		}
	} else {
		rootNode = 0;
	}

	if (rootNode) {
		result = (OptRotationScale*)rootNode->param2;
	} else {
		result = 0;
	}
	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
	return result;
}

// FUNCTION: XWA 0x4406B0
void ModelMesh_ApplyAnimatedMeshRotationToPoint(int angleQ16, unsigned int modelType, unsigned int meshIdx,
												int localX, int localY, int localZ) {
	OptRotationScale* rotScale;
	int axisX;
	int axisY;
	int axisZ;
	int16_t cosAngle;
	int16_t sinAngle;
	int m00;
	int m01;
	int m02;
	int m10;
	int m11;
	int m12;
	int m20;
	int m21;
	int m22;
	int pointX;
	int pointY;
	int pointZ;
	int transformedX;
	int transformedY;
	int transformedZ;

	g_rotatedX = localX;
	g_rotatedY = localY;
	g_rotatedZ = localZ;

	rotScale = ModelMesh_GetRotScaleData((int)modelType, (int)meshIdx);
	if (!rotScale) {
		return;
	}

	axisX = (int)rotScale->rotationAxis.x;
	axisY = (int)rotScale->rotationAxis.y;
	axisZ = (int)rotScale->rotationAxis.z;
	cosAngle = trig2_getsignedcos(angleQ16);
	sinAngle = trig2_getsignedsin(angleQ16);

	if (cosAngle >= 0) {
		int oneMinusCos;

		oneMinusCos = 0x7fff - cosAngle;
		m00 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) +
										  oneMinusCos * Xwa_Q15MulReuseFirstSlot(axisX, axisX));
		m01 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15MulReuseFirstSlot(axisZ, sinAngle) * 32768) +
										  oneMinusCos * Xwa_Q15MulReuseFirstSlot(axisY, axisX));
		m02 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15MulReuseFirstSlot(axisY, sinAngle)) +
										  oneMinusCos * Xwa_Q15MulReuseFirstSlot(axisZ, axisX));
		m10 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15MulReuseFirstSlot(axisZ, sinAngle)) +
										  oneMinusCos * Xwa_Q15MulReuseFirstSlot(axisY, axisX));
		m11 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) +
										  oneMinusCos * Xwa_Q15MulReuseFirstSlot(axisY, axisY));
		m12 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15MulReuseFirstSlot(axisX, sinAngle) * 32768) +
										  oneMinusCos * Xwa_Q15MulReuseFirstSlot(axisZ, axisY));
		m20 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15MulReuseFirstSlot(axisY, sinAngle) * 32768) +
										  oneMinusCos * Xwa_Q15MulReuseFirstSlot(axisZ, axisX));
		m21 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15MulReuseFirstSlot(axisX, sinAngle)) +
										  oneMinusCos * Xwa_Q15MulReuseFirstSlot(axisZ, axisY));
		m22 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) +
										  oneMinusCos * Xwa_Q15MulReuseFirstSlot(axisZ, axisZ));
	} else {
		int absCos;

		absCos = -cosAngle;
		m00 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) + axisX * axisX +
										  absCos * Xwa_Q15MulReuseFirstSlot(axisX, axisX));
		m01 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15MulReuseFirstSlot(axisZ, sinAngle) * 32768) +
										  axisY * axisX + absCos * Xwa_Q15MulReuseFirstSlot(axisY, axisX));
		m02 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15MulReuseFirstSlot(axisY, sinAngle)) +
										  axisZ * axisX + absCos * Xwa_Q15MulReuseFirstSlot(axisZ, axisX));
		m10 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15MulReuseFirstSlot(axisZ, sinAngle)) +
										  axisY * axisX + absCos * Xwa_Q15MulReuseFirstSlot(axisY, axisX));
		m11 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) + axisY * axisY +
										  absCos * Xwa_Q15MulReuseFirstSlot(axisY, axisY));
		m12 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15MulReuseFirstSlot(axisX, sinAngle) * 32768) +
										  axisZ * axisY + absCos * Xwa_Q15MulReuseFirstSlot(axisZ, axisY));
		m20 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15MulReuseFirstSlot(axisY, sinAngle) * 32768) +
										  axisZ * axisX + absCos * Xwa_Q15MulReuseFirstSlot(axisZ, axisX));
		m21 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15MulReuseFirstSlot(axisX, sinAngle)) +
										  axisZ * axisY + absCos * Xwa_Q15MulReuseFirstSlot(axisZ, axisY));
		m22 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) + axisZ * axisZ +
										  absCos * Xwa_Q15MulReuseFirstSlot(axisZ, axisZ));
	}

	pointX = localX - (int)rotScale->pivot.x;
	pointY = localY + (int)rotScale->pivot.y;
	pointZ = localZ - (int)rotScale->pivot.z;

	transformedX = Xwa_WrappedMulAdd3Q15(m00, pointX, m10, pointY, m20, pointZ);
	transformedY = Xwa_WrappedMulAdd3Q15(m01, pointX, m11, pointY, m21, pointZ);
	transformedZ = Xwa_WrappedMulAdd3Q15(m02, pointX, m12, pointY, m22, pointZ);

	g_rotatedZ = (int)rotScale->pivot.z + transformedZ;
	g_rotatedY = transformedY - (int)rotScale->pivot.y;
	g_rotatedX = (int)rotScale->pivot.x + transformedX;
}
