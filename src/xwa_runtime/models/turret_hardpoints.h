#ifndef XWA_RUNTIME_MODELS_TURRET_HARDPOINTS_H
#define XWA_RUNTIME_MODELS_TURRET_HARDPOINTS_H

/* XWAU's rules for the hardpoints of gun turret meshes. The rule
 * gun_turret_hardpoints in flight/flight.yaml turns them on. A gun turret
 * mesh is a mesh of type gun_turret, rotary_gun_turret, small_gun,
 * weapon_system_1 or weapon_system_2.
 *
 * With the rule:
 *   - when a starship's model loads and the model has gun turret meshes,
 *     each float hardpoint belongs to the gun turret mesh that has it, or to
 *     the nearest gun turret mesh. In the stock game, it belongs to the
 *     nearest weapon system mesh, or to the main hull;
 *   - the hardpoints of gun turret meshes aren't copied to the other side
 *     of the model;
 *   - a starship fires from the nearest live float hardpoint that faces the
 *     target. A hardpoint faces the target when it isn't on a gun turret
 *     mesh, or when its mesh looks toward the target. The test uses the sign
 *     of a dot product in double precision, so it gives the same result on
 *     every platform.
 *
 * The rule changes the simulation. */

#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 when `meshType` is a gun turret mesh. */
int XwaTurretHardpoints_IsTurretMesh(MeshType meshType);
/* Returns the mesh of object type `objectType` that the float hardpoint at
   (`x`, `negY`, `z`) belongs to. It takes the place of
   ModelMesh_FindFloatHardpointComponent. */
int XwaTurretHardpoints_FloatHardpointComponent(int objectType, int x, int negY, int z);
/* Returns the live float hardpoint of `hardpoints` that is nearest to the
   target at (`localX`, `localY`, `localZ`) and faces it, or -1. It takes the
   place of ModelMesh_FindNearestLiveFloatHardpoint, with the same
   arguments. */
int XwaTurretHardpoints_FindNearestLive(int objectType, int localX, int localY, int localZ, int excludedCount,
										int* excludedHardpointIndices, ModelFloatHardpoint* hardpoints,
										CraftData* craft);

#ifdef __cplusplus
}
#endif

#endif
