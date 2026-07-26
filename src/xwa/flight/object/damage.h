#ifndef XWA_FLIGHT_OBJECT_DAMAGE_H
#define XWA_FLIGHT_OBJECT_DAMAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int16_t g_damageMfdLastSelectedSystemId;
extern int16_t g_damageMfdDamagedSystemCountCached;
extern int16_t g_damageMfdSelectedRowY;
extern int16_t g_damageMfdRightPaneRedrawPending;
extern int16_t g_damageMfdForceAllSystemsDamaged;
extern int16_t g_damageMfdSelectedSystemId;
extern const uint16_t g_subsystemRepairDuration[10];
extern const uint8_t g_subsystemMessageArgById[10];
extern const uint16_t g_subsystemFailureHudMaskByRandomSlot[16];

int16_t Damage_FindAdjacentDamagedSystem(int16_t currentSystemIdx, uint16_t directionStep);
void Damage_DisplayMfdPage(int mfdSide, void* mfdTexPixels);
unsigned int Craft_DamageComponent(uint16_t objIdx, int componentId, unsigned int damageAmount,
								   uint16_t sourceObjIdx);
void Damage_QueueCraftBillboards(uint16_t objectIndex);
void Craft_DetachDamageableComponent(uint16_t sourceObjIdx, int detachAll, uint16_t componentIdx);
unsigned int Craft_SpawnExplosionObjectAtMesh(uint16_t sourceObjIdx, uint16_t meshIdx, int instanceExtent,
											  int useRandomVertex);
void Craft_SpawnMainHullExplosionEffects(uint16_t sourceObjIdx, uint16_t forceAtFirstHullMesh);

#ifdef __cplusplus
}
#endif

#endif
