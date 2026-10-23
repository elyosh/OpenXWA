#include "xwa_runtime/models/turret_hardpoints.h"

#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/flight/object/object.h"

#include <string.h>

/* Modern port module; no original counterpart. */

int XwaTurretHardpoints_IsTurretMesh(MeshType meshType) {
	return meshType == MESH_GunTurret || meshType == MESH_RotaryGunTurret || meshType == MESH_SmallGun ||
		   meshType == MESH_WeaponSystem1 || meshType == MESH_WeaponSystem2;
}

/* Returns 1 when the model of `objectType` has a gun turret mesh. */
static int TurretHardpoints_HasTurretMesh(int objectType) {
	const int meshCount = ModelMesh_GetCount(objectType);
	int meshIdx;

	for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
		if (XwaTurretHardpoints_IsTurretMesh(ModelMesh_GetType(objectType, meshIdx))) {
			return 1;
		}
	}
	return 0;
}

/* Returns the distance from `point` to the box from `min` to `max` along
   one axis, in float32 as in the hook. */
static float TurretHardpoints_BoxOffset(float point, float min, float max) {
	if (point > max) {
		return point - max;
	}
	if (point < min) {
		return min - point;
	}
	return 0.0f;
}

int XwaTurretHardpoints_FloatHardpointComponent(int objectType, int x, int negY, int z) {
	float bestDistance = 2.14748365E+09f;
	int bestMeshIdx = -1;
	int meshCount;
	int meshIdx;

	if (!TurretHardpoints_HasTurretMesh(objectType)) {
		return ModelMesh_FindFloatHardpointComponent(objectType, x, negY, z);
	}
	meshCount = ModelMesh_GetCount(objectType);
	for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
		const MeshDescriptor* descriptor;
		int hardpointCount;
		int hardpointIdx;
		float distance;
		float dx;
		float dy;
		float dz;

		if (!XwaTurretHardpoints_IsTurretMesh(ModelMesh_GetType(objectType, meshIdx))) {
			continue;
		}
		descriptor = ModelMesh_GetDescriptor(objectType, meshIdx);
		if (descriptor == NULL) {
			continue;
		}
		/* The hardpoint is one of the mesh's own hardpoints. */
		hardpointCount = ModelMesh_CountHardpoints(objectType, meshIdx);
		for (hardpointIdx = 0; hardpointIdx < hardpointCount; ++hardpointIdx) {
			OptHardpointType type;
			int hardpointX;
			int hardpointY;
			int hardpointZ;

			ModelMesh_GetHardpoint(objectType, meshIdx, hardpointIdx, &type, &hardpointX, &hardpointY,
								   &hardpointZ);
			if (x == hardpointX && negY == -hardpointY && z == hardpointZ) {
				return meshIdx;
			}
		}
		dx = TurretHardpoints_BoxOffset((float)x, descriptor->boxMin.x, descriptor->boxMax.x);
		dy = TurretHardpoints_BoxOffset((float)negY, descriptor->boxMin.y, descriptor->boxMax.y);
		dz = TurretHardpoints_BoxOffset((float)z, descriptor->boxMin.z, descriptor->boxMax.z);
		distance = dx * dx + dy * dy + dz * dz;
		if (distance < bestDistance) {
			bestDistance = distance;
			bestMeshIdx = meshIdx;
			if (bestDistance == 0.0f) {
				break;
			}
		}
	}
	return bestMeshIdx;
}

/* Returns 1 when float hardpoint `hardpoint` can fire at the target at
   (`localX`, `localY`, `localZ`): it isn't on a gun turret mesh, or its mesh
   looks toward the target. */
static int TurretHardpoints_FacesTarget(int objectType, const ModelFloatHardpoint* hardpoint, int localX,
										int localY, int localZ) {
	const OptRotationScale* rotation;
	double dot;

	if (!XwaTurretHardpoints_IsTurretMesh(ModelMesh_GetType(objectType, hardpoint->componentIndex))) {
		return 1;
	}
	rotation = ModelMesh_GetRotScaleData(objectType, hardpoint->componentIndex);
	if (rotation == NULL) {
		return 1;
	}
	/* The hook first normalizes both vectors in float32. This doesn't change
	   the sign. */
	dot = ((double)localX - hardpoint->x) * rotation->rotationAxis.x +
		  ((double)localY - hardpoint->negY) * rotation->rotationAxis.y +
		  ((double)localZ - hardpoint->z) * rotation->rotationAxis.z;
	return dot > 0.0;
}

int XwaTurretHardpoints_FindNearestLive(int objectType, int localX, int localY, int localZ, int excludedCount,
										int* excludedHardpointIndices, ModelFloatHardpoint* hardpoints,
										CraftData* craft) {
	uint8_t excluded[256];
	int hardpointCount;
	int markCount;
	int best;
	int index;
	float bestDistance;

	hardpointCount = g_modelDefs[(uint16_t)g_objectTypes[objectType].modelDefIndex].floatHardpointCount;
	markCount = excludedCount;
	if (markCount >= hardpointCount) {
		markCount = hardpointCount - 1;
	}
	memset(excluded, 0, sizeof excluded);
	for (index = 0; index < markCount; ++index) {
		excluded[(uint8_t)excludedHardpointIndices[index]] = 1;
	}
	best = -1;
	bestDistance = 0.0f;
	for (index = 0; index < hardpointCount; ++index) {
		const ModelFloatHardpoint* hardpoint = &hardpoints[index];
		float distance;

		if (craft->componentHp[hardpoint->componentIndex] == 0 || excluded[index] ||
			!TurretHardpoints_FacesTarget(objectType, hardpoint, localX, localY, localZ)) {
			continue;
		}
		/* The same float32 distance as the stock search. */
		distance = ((float)hardpoint->x - (float)localX) * ((float)hardpoint->x - (float)localX) +
				   ((float)hardpoint->negY - (float)localY) * ((float)hardpoint->negY - (float)localY) +
				   ((float)hardpoint->z - (float)localZ) * ((float)hardpoint->z - (float)localZ);
		if (best < 0 || distance < bestDistance) {
			best = index;
			bestDistance = distance;
		}
	}
	return best;
}
