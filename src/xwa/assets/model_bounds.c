#include "xwa/assets/model_bounds.h"

#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/util/memory.h"

// GLOBAL: XWA 0x690530
int g_modelBoundsCached[XWA_LOADED_MODEL_COUNT];
// GLOBAL: XWA 0x68EB08
Vec3f g_modelBoundsMin[XWA_LOADED_MODEL_COUNT];
// GLOBAL: XWA 0x690DE8
Vec3f g_modelBoundsMax[XWA_LOADED_MODEL_COUNT];

// FUNCTION: XWA 0x485310
void ModelBounds_EnsureCached(int modelType) {
	OptimizedPolyObject* model;
	Vec3f minBounds;
	Vec3f maxBounds;
	int i;

	minBounds.x = minBounds.y = minBounds.z = 1073741800.0f;
	maxBounds.x = maxBounds.y = maxBounds.z = -1073741800.0f;

	if (!g_flightRenderToFrontend && !(g_modelTypeTable[modelType].assetFlags & 1)) {
		return;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[modelType]);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	for (i = 0; i < model->rootNodeCount; ++i) {
		OptNode* root;
		MeshDescriptor* descriptor;

		root = model->rootNodes[i];
		if (!root || root->nodeType == OPT_TEXTURE || root->nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		if (root->nodeType == OPT_MESHDESC) {
			descriptor = (MeshDescriptor*)root->param2;
		} else {
			int childIndex;

			childIndex = 0;
			if (root->childCount <= 0) {
				descriptor = 0;
			} else {
				while (1) {
					if (root->pChildren[childIndex]) {
						descriptor =
							ModelMesh_FindDescriptorNodeRecursive(root->pChildren[childIndex], model);
						if (descriptor) {
							break;
						}
					}
					++childIndex;
					if (childIndex >= root->childCount) {
						descriptor = 0;
						break;
					}
				}
			}
		}

		if (descriptor) {
			if (descriptor->boxMin.x < minBounds.x) {
				minBounds.x = descriptor->boxMin.x;
			}
			if (descriptor->boxMin.y < minBounds.y) {
				minBounds.y = descriptor->boxMin.y;
			}
			if (descriptor->boxMin.z < minBounds.z) {
				minBounds.z = descriptor->boxMin.z;
			}
			if (descriptor->boxMax.x > maxBounds.x) {
				maxBounds.x = descriptor->boxMax.x;
			}
			if (descriptor->boxMax.y > maxBounds.y) {
				maxBounds.y = descriptor->boxMax.y;
			}
			if (descriptor->boxMax.z > maxBounds.z) {
				maxBounds.z = descriptor->boxMax.z;
			}
		} else {
			if (root->nodeType != OPT_MESHVERTS) {
				int childIndex;

				childIndex = 0;
				if (root->childCount <= 0) {
					root = 0;
				} else {
					while (1) {
						if (root->pChildren[childIndex]) {
							OptNode* meshVertsNode;

							meshVertsNode = ModelMesh_FindFirstMeshVertsNode(root->pChildren[childIndex]);
							if (meshVertsNode) {
								root = meshVertsNode;
								break;
							}
						}
						++childIndex;
						if (childIndex >= root->childCount) {
							root = 0;
							break;
						}
					}
				}
			}

			if (root) {
				const Vec3f* vertexData = (const Vec3f*)root->param2;
				int vertexCount = root->param1;

				if (vertexCount >= 2) {
					const Vec3f* vertexBounds = vertexData + (vertexCount - 2);
					if (vertexBounds->x < minBounds.x) {
						minBounds.x = vertexBounds->x;
					}
					if (vertexBounds->y < minBounds.y) {
						minBounds.y = vertexBounds->y;
					}
					if (vertexBounds->z < minBounds.z) {
						minBounds.z = vertexBounds->z;
					}
					++vertexBounds;
					if (vertexBounds->x > maxBounds.x) {
						maxBounds.x = vertexBounds->x;
					}
					if (vertexBounds->y > maxBounds.y) {
						maxBounds.y = vertexBounds->y;
					}
					if (vertexBounds->z > maxBounds.z) {
						maxBounds.z = vertexBounds->z;
					}
				}
			}
		}
	}

	g_modelBoundsCached[modelType] = 1;
	g_modelBoundsMin[modelType].x = minBounds.x;
	g_modelBoundsMin[modelType].y = minBounds.y;
	g_modelBoundsMin[modelType].z = minBounds.z;
	g_modelBoundsMax[modelType].x = maxBounds.x;
	g_modelBoundsMax[modelType].y = maxBounds.y;
	g_modelBoundsMax[modelType].z = maxBounds.z;
	Memory_UnlockHandle(g_loadedModels.byObjectType[modelType]);
}

// FUNCTION: XWA 0x4855D0
int ModelBounds_GetMaxExtent(int modelType) {
	double extentX;
	double extentY;
	float extent[3];

	if (!g_modelBoundsCached[modelType]) {
		ModelBounds_EnsureCached(modelType);
	}
	extentY = g_modelBoundsMax[modelType].y;
	extent[1] = (float)(extentY - g_modelBoundsMin[modelType].y);
	extentX = g_modelBoundsMax[modelType].x;
	extentX -= g_modelBoundsMin[modelType].x;
	extent[2] = g_modelBoundsMax[modelType].z - g_modelBoundsMin[modelType].z;

	if (extent[1] >= extentX && extent[1] >= extent[2]) {
		return (int)extent[1];
	}
	if (extent[2] >= extentX && extent[2] >= extent[1]) {
		extentX = extent[2];
	}
	return (int)extentX;
}

// FUNCTION: XWA 0x485680
int ModelBounds_GetMinY(int modelType) {
	if (!g_modelBoundsCached[modelType]) {
		ModelBounds_EnsureCached(modelType);
	}
	return (int)g_modelBoundsMin[modelType].y;
}

// FUNCTION: XWA 0x4856B0
int ModelBounds_GetMinZ(int modelType) {
	if (!g_modelBoundsCached[modelType]) {
		ModelBounds_EnsureCached(modelType);
	}
	return (int)g_modelBoundsMin[modelType].z;
}

// FUNCTION: XWA 0x4856E0
int ModelBounds_GetMaxY(int modelType) {
	if (!g_modelBoundsCached[modelType]) {
		ModelBounds_EnsureCached(modelType);
	}
	return (int)g_modelBoundsMax[modelType].y;
}

// FUNCTION: XWA 0x485710
int ModelBounds_GetMaxZ(int modelType) {
	if (!g_modelBoundsCached[modelType]) {
		ModelBounds_EnsureCached(modelType);
	}
	return (int)g_modelBoundsMax[modelType].z;
}

// FUNCTION: XWA 0x485740
Vec3f* ModelBounds_GetMinVector(int modelType) {
	if (!g_modelBoundsCached[modelType]) {
		ModelBounds_EnsureCached(modelType);
	}
	return &g_modelBoundsMin[modelType];
}

// FUNCTION: XWA 0x485770
Vec3f* ModelBounds_GetMaxVector(int modelType) {
	if (!g_modelBoundsCached[modelType]) {
		ModelBounds_EnsureCached(modelType);
	}
	return &g_modelBoundsMax[modelType];
}

// FUNCTION: XWA 0x4857A0
int ModelBounds_GetSizeX(int modelType) {
	if (!g_modelBoundsCached[modelType]) {
		ModelBounds_EnsureCached(modelType);
	}
	return (int)(g_modelBoundsMax[modelType].x - g_modelBoundsMin[modelType].x);
}

// FUNCTION: XWA 0x4857E0
int ModelBounds_GetSizeY(int modelType) {
	if (!g_modelBoundsCached[modelType]) {
		ModelBounds_EnsureCached(modelType);
	}
	return (int)(g_modelBoundsMax[modelType].y - g_modelBoundsMin[modelType].y);
}

// FUNCTION: XWA 0x485820
int ModelBounds_GetSizeZ(int modelType) {
	if (!g_modelBoundsCached[modelType]) {
		ModelBounds_EnsureCached(modelType);
	}
	return (int)(g_modelBoundsMax[modelType].z - g_modelBoundsMin[modelType].z);
}

// FUNCTION: XWA 0x485860
double ModelBounds_ComputeMaxMinExtentRatio(int modelType) {
	double extentX;
	double extentY;
	double extentZ;
	double ratio;

	g_modelTypeTable[0].assetFlags = 1;
	g_modelBoundsCached[modelType] = 0;
	ModelBounds_EnsureCached(modelType);

	extentZ = g_modelBoundsMax[modelType].z - g_modelBoundsMin[modelType].z;

	if ((extentY = g_modelBoundsMax[modelType].y - g_modelBoundsMin[modelType].y) <
		(extentX = g_modelBoundsMax[modelType].x - g_modelBoundsMin[modelType].x)) {
		if (extentY > extentZ) {
			ratio = extentX / extentZ;
		} else if (extentX > extentZ) {
			ratio = extentX / extentY;
		} else {
			ratio = extentZ / extentY;
		}
	} else if (extentX > extentZ) {
		ratio = extentY / extentZ;
	} else if (extentY > extentZ) {
		ratio = extentY / extentX;
	} else {
		ratio = extentZ / extentX;
	}

	g_modelTypeTable[0].assetFlags = 0;
	return ratio;
}

// FUNCTION: XWA 0x488E50
int ModelBounds_ClearCache(void) {
	int i;

	for (i = 0; i < 512; ++i) {
		g_modelBoundsCached[i] = 0;
	}
	return 1;
}
