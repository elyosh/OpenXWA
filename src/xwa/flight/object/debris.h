#ifndef XWA_FLIGHT_OBJECT_DEBRIS_H
#define XWA_FLIGHT_OBJECT_DEBRIS_H

#include "xwa/math/vec3i.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	XWA_RUBBLE_MODEL_COUNT = 12,
	XWA_RUBBLE_VOLUME_CLASS_COUNT = 3,
};

extern uint16_t g_rubbleModelClassCounts[4];
extern Vec3i    g_rubbleModelBoundsSize[XWA_RUBBLE_MODEL_COUNT];
extern uint32_t g_rubbleVolumeClassThresholds[XWA_RUBBLE_VOLUME_CLASS_COUNT];
extern uint32_t g_rubbleModelMaxVolume;
extern uint32_t g_rubbleModelMinVolume;
extern uint8_t  g_rubbleModelUsedInSpawn[XWA_RUBBLE_MODEL_COUNT];
extern uint16_t g_rubbleModelClassIndices[XWA_RUBBLE_VOLUME_CLASS_COUNT][XWA_RUBBLE_MODEL_COUNT];
extern uint8_t  g_rubbleModelIsSmall[XWA_RUBBLE_MODEL_COUNT];
extern uint32_t g_rubbleModelVolume[XWA_RUBBLE_MODEL_COUNT];
extern uint32_t g_rubbleSpawnVolumeBudget;
extern uint32_t g_rubbleSpawnRemainingCount;

bool Debris_InitRubbleModelTables(void);
void Debris_PositionFragment(int fragmentObjIdx, int sourceObjIdx, int meshIndex);
void Debris_SpawnObjectFragments(int sourceObjIdx, int sourceMeshIdx);
int16_t Debris_AllocFragmentFromVolumeBudget(int sourceObjIdx, int sourceMeshIdx);
int  Debris_AllocRubbleObject(int sourceObjIdx);

#ifdef __cplusplus
}
#endif

#endif
