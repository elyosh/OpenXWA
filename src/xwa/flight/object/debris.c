#include "xwa/flight/object/debris.h"

#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/flight.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/object/object.h"
#include "xwa/render/renderer.h"
#include "xwa/util/random.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

// GLOBAL: XWA 0x60E470
uint16_t g_rubbleModelClassCounts[4];
// GLOBAL: XWA 0x60E478
Vec3i g_rubbleModelBoundsSize[XWA_RUBBLE_MODEL_COUNT];
// GLOBAL: XWA 0x60E508
uint32_t g_rubbleVolumeClassThresholds[XWA_RUBBLE_VOLUME_CLASS_COUNT];
// GLOBAL: XWA 0x60E514
uint32_t g_rubbleModelMaxVolume;
// GLOBAL: XWA 0x60E518
uint32_t g_rubbleModelMinVolume;
// GLOBAL: XWA 0x60E768
uint8_t g_rubbleModelUsedInSpawn[XWA_RUBBLE_MODEL_COUNT];
// GLOBAL: XWA 0x60E778
uint16_t g_rubbleModelClassIndices[XWA_RUBBLE_VOLUME_CLASS_COUNT][XWA_RUBBLE_MODEL_COUNT];
// GLOBAL: XWA 0x60E7C0
uint8_t g_rubbleModelIsSmall[XWA_RUBBLE_MODEL_COUNT];
// GLOBAL: XWA 0x60E7D0
uint32_t g_rubbleModelVolume[XWA_RUBBLE_MODEL_COUNT];
// GLOBAL: XWA 0x60E800
uint32_t g_rubbleSpawnVolumeBudget;
// GLOBAL: XWA 0x60E804
uint32_t g_rubbleSpawnRemainingCount;

static __inline int Debris_RandomScatterOffset(void) {
	int offset;

	offset = (int16_t)GameRand();
	offset &= 0x80ff;
	if (offset & 0x8000) {
		offset = -(offset & 0x7fff);
	}

	return offset;
}

static __inline void Debris_ResetRubbleUsedInSpawn(void) {
	memset(g_rubbleModelUsedInSpawn, 0, sizeof(g_rubbleModelUsedInSpawn));
}

// FUNCTION: XWA 0x4051B0
bool Debris_InitRubbleModelTables(void) {
	bool allRubbleModelsAvailable;
	int rubbleIdx;

	allRubbleModelsAvailable = true;
	if (g_flightPlayerCount > 1) {
		g_debrisDensityLevel = 0;
	} else {
		int debrisDensity;

		debrisDensity = g_gameConfig.debrisDensity[0];
		g_debrisDensityLevel = debrisDensity;
		if (debrisDensity < 0) {
			g_debrisDensityLevel = 0;
		} else if (debrisDensity > 7) {
			g_debrisDensityLevel = 7;
		}
	}

	g_rubbleModelMinVolume = UINT32_MAX;
	g_rubbleModelMaxVolume = 0;
	for (rubbleIdx = 0; rubbleIdx < XWA_RUBBLE_MODEL_COUNT; ++rubbleIdx) {
		int objectType;
		uint32_t volume;

		objectType = OBJ_Rubble01 + rubbleIdx;
		if (g_loadedModels.byObjectType[objectType] == 0) {
			g_rubbleModelBoundsSize[rubbleIdx].x = 0;
			g_rubbleModelBoundsSize[rubbleIdx].y = 0;
			g_rubbleModelBoundsSize[rubbleIdx].z = 0;
			g_rubbleModelVolume[rubbleIdx] = 0;
			allRubbleModelsAvailable = false;
		} else {
			g_rubbleModelBoundsSize[rubbleIdx].x = ModelBounds_GetSizeX(objectType);
			g_rubbleModelBoundsSize[rubbleIdx].y = ModelBounds_GetSizeY(objectType);
			g_rubbleModelBoundsSize[rubbleIdx].z = ModelBounds_GetSizeZ(objectType);
			volume = (uint32_t)g_rubbleModelBoundsSize[rubbleIdx].y *
					 (uint32_t)g_rubbleModelBoundsSize[rubbleIdx].z;
			volume *= (uint32_t)g_rubbleModelBoundsSize[rubbleIdx].x;
			g_rubbleModelVolume[rubbleIdx] = volume;
			if (volume < g_rubbleModelMinVolume) {
				g_rubbleModelMinVolume = volume;
			}
			if (volume > g_rubbleModelMaxVolume) {
				g_rubbleModelMaxVolume = volume;
			}
		}
	}

	{
		uint32_t smallThreshold;

		smallThreshold = g_rubbleModelMinVolume * 3u;
		for (rubbleIdx = 0; rubbleIdx < XWA_RUBBLE_MODEL_COUNT; ++rubbleIdx) {
			if (g_rubbleModelVolume[rubbleIdx] < smallThreshold) {
				g_rubbleModelIsSmall[rubbleIdx] = 1;
			} else {
				g_rubbleModelIsSmall[rubbleIdx] = 0;
			}
		}
	}

	{
		uint32_t* volumeSlot;
		uint32_t largeThreshold;
		uint32_t mediumThreshold;
		int classRubbleIdx;

		largeThreshold = g_rubbleModelMaxVolume - g_rubbleModelMaxVolume / 10u;
		mediumThreshold = g_rubbleModelMaxVolume / 5u;
		g_rubbleVolumeClassThresholds[2] = largeThreshold;
		memset(g_rubbleModelClassCounts, 0,
			   XWA_RUBBLE_VOLUME_CLASS_COUNT * sizeof(g_rubbleModelClassCounts[0]));
		mediumThreshold *= 2u;
		classRubbleIdx = 0;
		g_rubbleVolumeClassThresholds[1] = mediumThreshold;
		g_rubbleVolumeClassThresholds[0] = 0;
		volumeSlot = g_rubbleModelVolume;

		for (; (intptr_t)volumeSlot < (intptr_t)&g_rubbleModelVolume[XWA_RUBBLE_MODEL_COUNT];
			 ++volumeSlot, ++classRubbleIdx) {
			uint16_t modelIndex;
			uint32_t volume;

			modelIndex = (uint16_t)classRubbleIdx;
			volume = *volumeSlot;
			if (volume >= g_rubbleVolumeClassThresholds[2]) {
				g_rubbleModelClassIndices[2][g_rubbleModelClassCounts[2]++] = modelIndex;
			} else if (volume >= mediumThreshold) {
				g_rubbleModelClassIndices[1][g_rubbleModelClassCounts[1]++] = modelIndex;
			} else {
				g_rubbleModelClassIndices[0][g_rubbleModelClassCounts[0]++] = modelIndex;
			}
		}
	}

	return allRubbleModelsAvailable;
}

// FUNCTION: XWA 0x405390
void Debris_PositionFragment(int fragmentObjIdx, int sourceObjIdx, int meshIndex) {
	int centerX;
	int centerY;
	int centerZ;
	int rotatedX;
	int rotatedY;
	int rotatedZ;
	int scatterX;
	int scatterY;
	int scatterZ;

	centerX = ModelMesh_GetCenterX(g_objectTable[sourceObjIdx].objectType, meshIndex);
	centerY = ModelMesh_GetCenterY(g_objectTable[sourceObjIdx].objectType, meshIndex);
	centerZ = ModelMesh_GetCenterZ(g_objectTable[sourceObjIdx].objectType, meshIndex);
	if ((centerX | centerY | centerZ) == 0) {
		centerX = Debris_RandomScatterOffset();
		centerY = Debris_RandomScatterOffset();
		centerZ = Debris_RandomScatterOffset();
	}

	pai_RotateLocalVectorToWorldScratch(&g_objectTable[sourceObjIdx], centerX, centerZ, -centerY);
	rotatedX = g_rotatedX;
	rotatedY = g_rotatedY;
	rotatedZ = g_rotatedZ;
	g_objectTable[fragmentObjIdx].world_x += rotatedX;
	g_objectTable[fragmentObjIdx].world_y += rotatedY;
	g_objectTable[fragmentObjIdx].world_z += rotatedZ;

	scatterX = Debris_RandomScatterOffset();
	scatterY = Debris_RandomScatterOffset();
	scatterZ = Debris_RandomScatterOffset();
	g_objectTable[fragmentObjIdx].world_x += scatterX;
	g_objectTable[fragmentObjIdx].world_y += scatterY;
	g_objectTable[fragmentObjIdx].world_z += scatterZ;

	g_objectTable[fragmentObjIdx].mobj->prevWorldX = g_objectTable[fragmentObjIdx].world_x;
	g_objectTable[fragmentObjIdx].mobj->prevWorldY = g_objectTable[fragmentObjIdx].world_y;
	g_objectTable[fragmentObjIdx].mobj->prevWorldZ = g_objectTable[fragmentObjIdx].world_z;

	trig2_ctop(scatterX, scatterY, scatterZ);
	g_objectTable[fragmentObjIdx].yaw = trig2_xyangle;
	g_objectTable[fragmentObjIdx].pitch = targetPitch;
}

// FUNCTION: XWA 0x405590
void Debris_SpawnObjectFragments(int sourceObjIdx, int sourceMeshIdx) {
	uint16_t sourceObjectType;

	if (g_flightSimSideEffectsSuppressed || g_flightPlayerCount > 1) {
		return;
	}

	sourceObjectType = g_objectTable[sourceObjIdx].objectType;
	if (g_objectTable[sourceObjIdx].mobj->orientMatrixDirty) {
		FVIEW_SetObjectTransform(g_objectTable[sourceObjIdx].roll, g_objectTable[sourceObjIdx].pitch,
								 g_objectTable[sourceObjIdx].yaw, g_objectTable[sourceObjIdx].angleD,
								 &g_objectTable[sourceObjIdx]);
	}

	if (g_objectTable[sourceObjIdx].objectType >= (uint16_t)OBJ_Rubble01 &&
		g_objectTable[sourceObjIdx].objectType <= (uint16_t)OBJ_Rubble12) {
		unsigned int fragmentObjIdx;
		int fragmentsForRubble;
		uint32_t spawnVolume;

		if (!g_rubbleModelIsSmall[g_objectTable[sourceObjIdx].objectType - OBJ_Rubble01]) {
			fragmentsForRubble = g_debrisDensityLevel - 3;
			spawnVolume = g_rubbleModelVolume[g_objectTable[sourceObjIdx].objectType - OBJ_Rubble01] - 1u;
			if (fragmentsForRubble < 2) {
				fragmentsForRubble = 2;
			}
			g_rubbleSpawnVolumeBudget = spawnVolume;
			g_rubbleSpawnRemainingCount = (uint32_t)fragmentsForRubble;
			Debris_ResetRubbleUsedInSpawn();

			fragmentObjIdx = (uint16_t)Debris_AllocFragmentFromVolumeBudget(sourceObjIdx, -1);
			while (fragmentObjIdx != 0xffffu) {
				g_objectTable[fragmentObjIdx].mobj->speed =
					(uint16_t)(g_objectTable[fragmentObjIdx].mobj->speed + 32u);
				g_objectTable[fragmentObjIdx].mobj->lifetimeTimer = (uint16_t)GameRand() % 944 + 472;
				fragmentObjIdx = (uint16_t)Debris_AllocFragmentFromVolumeBudget(sourceObjIdx, -1);
			}
		}
		return;
	}

	{
		int modelType;
		int16_t sourceModelMarker;
		unsigned int meshCount;
		uint32_t meshIdx;
		int densityLevel;

		modelType = sourceObjectType;
		sourceModelMarker = ModelMesh_AllocDebrisTexSlot((ObjectTypeId)modelType);
		if ((uint16_t)sourceModelMarker == 0xffffu) {
			return;
		}

		meshCount = (unsigned int)ModelMesh_GetObjectTypeMeshCount(modelType);
		meshIdx = 0;
		if (meshIdx < meshCount) {
			densityLevel = g_debrisDensityLevel;
			do {
				uint32_t fragmentsForMesh;

				fragmentsForMesh = 1;
				if (sourceMeshIdx != -1) {
					meshIdx = (uint32_t)sourceMeshIdx;
				} else {
					bool skipMeshForDensity;

					skipMeshForDensity = 0;
					switch (densityLevel) {
						case 3:
							fragmentsForMesh = 2;
							/* Fall through: density levels 2 and 3 skip alternate meshes. */
						case 2:
							if (meshIdx & 1u) {
								skipMeshForDensity = 1;
							}
							break;
						case 0:
							if (meshIdx & 3u) {
								skipMeshForDensity = 1;
							}
							break;
						case 1:
							if (meshIdx % 3u) {
								skipMeshForDensity = 1;
							}
							densityLevel = g_debrisDensityLevel;
							break;
						default:
							break;
					}
					if (skipMeshForDensity) {
						++meshIdx;
						continue;
					}
				}

				{
					unsigned int fragmentObjIdx;

					if (densityLevel >= 4) {
						fragmentsForMesh = (uint32_t)(densityLevel - 3);
					}

					g_rubbleSpawnVolumeBudget = ModelMesh_GetBoundsVolume(modelType, (int)meshIdx);
					Debris_ResetRubbleUsedInSpawn();
					g_rubbleSpawnRemainingCount = fragmentsForMesh;

					fragmentObjIdx =
						(uint16_t)Debris_AllocFragmentFromVolumeBudget(sourceObjIdx, (int)meshIdx);
					if (fragmentObjIdx != 0xffffu) {
						uint16_t fragmentSignature;

						fragmentSignature = (uint16_t)(0xfffeu - (uint16_t)sourceModelMarker);
						do {
							int yawDelta;
							int pitchDelta;

							Debris_PositionFragment(fragmentObjIdx, sourceObjIdx, (int)meshIdx);
							g_objectTable[fragmentObjIdx].objectSignature = fragmentSignature;
							yawDelta = (GameRand() & 0x0bff) + 1024;
							pitchDelta = (GameRand() & 0x0bff) + 1024;
							if (GameRand() & 0x1000) {
								yawDelta = -yawDelta;
							}
							if (GameRand() & 0x1000) {
								pitchDelta = -pitchDelta;
							}
							g_objectTable[fragmentObjIdx].yaw =
								(uint16_t)(g_objectTable[fragmentObjIdx].yaw + (uint16_t)yawDelta);
							g_objectTable[fragmentObjIdx].pitch =
								(uint16_t)(g_objectTable[fragmentObjIdx].pitch + (uint16_t)pitchDelta);
							if (g_objectTable[fragmentObjIdx].pitch >= 0x8000u) {
								g_objectTable[fragmentObjIdx].pitch =
									(uint16_t)(0u - g_objectTable[fragmentObjIdx].pitch);
								g_objectTable[fragmentObjIdx].yaw =
									(uint16_t)(g_objectTable[fragmentObjIdx].yaw + 0x8000u);
							}
							g_objectTable[fragmentObjIdx].roll = GameRand();
							g_objectTable[fragmentObjIdx].angleD = GameRand();
							g_objectTable[fragmentObjIdx].mobj->orientMatrixDirty = 1;
							g_objectTable[fragmentObjIdx].mobj->moveVectorDirty = 1;
							fragmentObjIdx =
								(uint16_t)Debris_AllocFragmentFromVolumeBudget(sourceObjIdx, (int)meshIdx);
						} while (fragmentObjIdx != 0xffffu);
					}

					if (sourceMeshIdx != -1) {
						return;
					}
					densityLevel = g_debrisDensityLevel;
				}
				++meshIdx;
			} while (meshIdx < (uint32_t)meshCount);
		}

		return;
	}
}

// FUNCTION: XWA 0x405910
int16_t Debris_AllocFragmentFromVolumeBudget(int sourceObjIdx, int sourceMeshIdx) {
	int classIdx;
	uint16_t selectedRubbleIdx;
	int fragmentObjIdx;
	uint16_t candidateSlot;
	int sourceObjectIdx;

	if (g_rubbleSpawnVolumeBudget <= g_rubbleModelMinVolume || g_rubbleSpawnRemainingCount == 0) {
		return -1;
	}
	sourceObjectIdx = sourceObjIdx;

	for (classIdx = 2; classIdx >= 0; --classIdx) {
		unsigned int firstCandidateSlot;
		bool foundSuitableModel;

		if (g_rubbleSpawnVolumeBudget < g_rubbleVolumeClassThresholds[classIdx]) {
			continue;
		}

		if (g_rubbleModelClassCounts[classIdx] == 0) {
			continue;
		}

		candidateSlot = (uint16_t)((uint16_t)GameRand() % g_rubbleModelClassCounts[classIdx]);
		firstCandidateSlot = candidateSlot;
		foundSuitableModel = true;
		while (1) {
			uint16_t objectType;
			int rubbleModelIdx;

			if (!g_rubbleModelUsedInSpawn[g_rubbleModelClassIndices[classIdx][candidateSlot]]) {
				objectType = (uint16_t)g_objectTable[sourceObjectIdx].objectType;
				rubbleModelIdx = g_rubbleModelClassIndices[classIdx][candidateSlot];
				if (g_rubbleModelVolume[rubbleModelIdx] <= g_rubbleSpawnVolumeBudget) {
					if (sourceMeshIdx != -1) {
						if ((uint32_t)g_rubbleModelBoundsSize[rubbleModelIdx].x <=
								(uint32_t)ModelMesh_GetBoundsSizeX(objectType, sourceMeshIdx) &&
							(uint32_t)g_rubbleModelBoundsSize[rubbleModelIdx].y <=
								(uint32_t)ModelMesh_GetBoundsSizeY(objectType, sourceMeshIdx) &&
							(uint32_t)g_rubbleModelBoundsSize[rubbleModelIdx].z <=
								(uint32_t)ModelMesh_GetBoundsSizeZ(objectType, sourceMeshIdx)) {
							break;
						}
					} else if ((uint32_t)g_rubbleModelBoundsSize[rubbleModelIdx].x <=
								   (uint32_t)ModelBounds_GetSizeX(objectType) &&
							   (uint32_t)g_rubbleModelBoundsSize[rubbleModelIdx].y <=
								   (uint32_t)ModelBounds_GetSizeY(objectType) &&
							   (uint32_t)g_rubbleModelBoundsSize[rubbleModelIdx].z <=
								   (uint32_t)ModelBounds_GetSizeZ(objectType)) {
						break;
					}
				}
			}

			candidateSlot = (uint16_t)(candidateSlot + 1u);
			if (candidateSlot >= g_rubbleModelClassCounts[classIdx]) {
				candidateSlot = 0;
			}
			if (candidateSlot == firstCandidateSlot) {
				foundSuitableModel = false;
				break;
			}
		}

		if (foundSuitableModel) {
			break;
		}
	}

	if (classIdx == -1) {
		return -1;
	}

	selectedRubbleIdx = g_rubbleModelClassIndices[classIdx][candidateSlot];
	fragmentObjIdx = Debris_AllocRubbleObject(sourceObjectIdx);
	if (fragmentObjIdx == 0xffff) {
		return -1;
	}

	g_objectTable[fragmentObjIdx].objectType = (ObjectTypeId)(OBJ_Rubble01 + selectedRubbleIdx);
	g_objectTable[fragmentObjIdx].objectSignature = g_objectTable[sourceObjectIdx].objectSignature;
	g_rubbleSpawnVolumeBudget -= g_rubbleModelVolume[selectedRubbleIdx];
	--g_rubbleSpawnRemainingCount;
	return (int16_t)fragmentObjIdx;
}

// FUNCTION: XWA 0x405B90
int Debris_AllocRubbleObject(int sourceObjIdx) {
	unsigned int newObjIdx;
	uint16_t newObjIdxWord;
	float axisX;
	float axisY;
	float axisZ;
	float axisLen;
	int yawJitter;
	int pitchJitter;

	newObjIdxWord = Object_AllocSlotForGenus(g_modelTypeTable[OBJ_Rubble02].genusId);
	newObjIdx = newObjIdxWord;
	if (newObjIdxWord == 0xffffu) {
		return newObjIdx;
	}

	memset(g_objectTable[newObjIdx].mobj, 0, offsetof(MobileObject, pWarheadGuidance));
	memset(g_objectTable[newObjIdx].mobj->pCraft, 0, offsetof(CraftData, effectiveAiObjectLink));

	g_objectTable[newObjIdx].mobj->sourceObjIdx = -1;
	g_objectTable[newObjIdx].mobj->instanceExtent = 0;
	g_objectTable[newObjIdx].objectType = OBJ_Rubble02;
	g_objectTable[newObjIdx].playerOwnerIdx = -1;
	g_objectTable[newObjIdx].regionIdx = g_objectTable[sourceObjIdx].regionIdx;
	g_objectTable[newObjIdx].objectType = OBJ_Rubble02;
	g_objectTable[newObjIdx].genusId = g_modelTypeTable[OBJ_Rubble02].genusId;
	g_objectTable[newObjIdx].objectSignature = 0xffffu;
	g_objectTable[newObjIdx].flightGroupIdx = g_objectTable[sourceObjIdx].flightGroupIdx;
	g_objectTable[newObjIdx].mobj->state = g_modelTypeTable[OBJ_Rubble02].familyId;
	g_objectTable[newObjIdx].mobj->instanceExtent = g_modelTypeTable[OBJ_Rubble02].maxBoundsExtent;
	g_objectTable[newObjIdx].world_x = g_objectTable[sourceObjIdx].world_x;
	g_objectTable[newObjIdx].world_y = g_objectTable[sourceObjIdx].world_y;
	g_objectTable[newObjIdx].world_z = g_objectTable[sourceObjIdx].world_z;
	g_objectTable[newObjIdx].mobj->prevWorldX = g_objectTable[newObjIdx].world_x;
	g_objectTable[newObjIdx].mobj->prevWorldY = g_objectTable[newObjIdx].world_y;
	g_objectTable[newObjIdx].mobj->prevWorldZ = g_objectTable[newObjIdx].world_z;
	collide_ResetObjectProximityForSlot(newObjIdx);
	g_objectTable[newObjIdx].mobj->damageAmount = g_modelTypeTable[OBJ_Rubble02].maxBoundsExtent;
	g_objectTable[newObjIdx].mobj->lifetimeTimer = (uint16_t)GameRand() % 7552 + 1888;
	g_objectTable[newObjIdx].mobj->framesAlive = 0;
	g_objectTable[newObjIdx].mobj->sourceObjIdx = (int16_t)sourceObjIdx;
	g_objectTable[newObjIdx].mobj->sourceObjectType = g_objectTable[sourceObjIdx].objectType;
	g_objectTable[newObjIdx].mobj->rollImpulseRate = 0;
	g_objectTable[newObjIdx].mobj->spinRate = (int16_t)(GameRand() & 0x1fff);
	g_objectTable[newObjIdx].mobj->spinRateFrac = 0;
	g_objectTable[newObjIdx].mobj->spinAngleQ16 = 0;
	g_objectTable[newObjIdx].mobj->velocityOverrideActive = 0;

	axisX = (float)(int16_t)GameRand() * 0.000030517578f;
	axisY = (float)(int16_t)GameRand() * 0.000030517578f;
	axisZ = (float)(int16_t)GameRand() * 0.000030517578f;
	axisLen = (float)sqrt((double)(axisX * axisX + axisY * axisY + axisZ * axisZ));
	if (axisLen == 0.0f) {
		axisX = 0.0f;
		axisY = 0.0f;
		axisZ = 1.0f;
	} else {
		axisX = axisX / axisLen;
		axisY = axisY / axisLen;
		axisZ = axisZ / axisLen;
	}
	g_objectTable[newObjIdx].mobj->spinAxisX = axisX;
	g_objectTable[newObjIdx].mobj->spinAxisY = axisY;
	g_objectTable[newObjIdx].mobj->spinAxisZ = axisZ;
	g_objectTable[newObjIdx].mobj->speed = (uint16_t)((GameRand() & 0x3f) + 96);
	g_objectTable[newObjIdx].mobj->speedRemainder = 0;

	g_objectTable[newObjIdx].yaw = g_objectTable[sourceObjIdx].yaw;
	g_objectTable[newObjIdx].pitch = g_objectTable[sourceObjIdx].pitch;
	g_objectTable[newObjIdx].roll = g_objectTable[sourceObjIdx].roll;
	g_objectTable[newObjIdx].angleD = g_objectTable[sourceObjIdx].angleD;
	yawJitter = (GameRand() & 0x0fff) + 256;
	pitchJitter = (GameRand() & 0x0fff) + 256;
	if (GameRand() & 1) {
		yawJitter = -yawJitter;
	}
	if (GameRand() & 1) {
		pitchJitter = -pitchJitter;
	}
	g_objectTable[newObjIdx].yaw = (uint16_t)(g_objectTable[newObjIdx].yaw + (uint16_t)yawJitter);
	g_objectTable[newObjIdx].pitch = (uint16_t)(g_objectTable[newObjIdx].pitch + (uint16_t)pitchJitter);
	if (g_objectTable[newObjIdx].pitch >= 0x8000u) {
		g_objectTable[newObjIdx].pitch = (uint16_t)(0u - g_objectTable[newObjIdx].pitch);
		g_objectTable[newObjIdx].yaw = (uint16_t)(g_objectTable[newObjIdx].yaw + 0x8000u);
	}
	g_objectTable[newObjIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[newObjIdx].mobj->moveVectorDirty = 1;
	g_objectTable[newObjIdx].typeSpecificWord = 0;

	return newObjIdx;
}
