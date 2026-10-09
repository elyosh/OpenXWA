#ifndef XWA_REMASTER_SKINS_H
#define XWA_REMASTER_SKINS_H

/* XWAU skins on the renderer side: the variant a skinned model's mesh shows
 * for each markings value, from the snapshot's skin selections and uses
 * (xwa_runtime/models/skins.h). The mesh's variant index i is markings value
 * i (AeronSceneMeshInstance.variant = node_switch):
 *   - a used markings value shows the mission's selection for the model's
 *     GLB, else the GLB's default for it (aeronSkinMarkings); a selected
 *     name the GLB doesn't have falls back to the default, logged once;
 *   - a markings value without a skin, and any unused one, shows the asset
 *     paint state min(m, aeronAssetVariants - 1);
 *   - a model no object is known to use (hangar craft, for example) uses
 *     markings 0, which objects start with.
 * Only models with variant groups get markings variants; the others keep
 * their asset variants (AeronScene_MeshCreate). */

#include <stdint.h>

#include "aeron/scene/gltf_mesh.h"
#include "aeron/scene/mesh.h"
#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XWA_REMASTER_MAX_MARKINGS 256

typedef struct XwaRemasterSkinModel XwaRemasterSkinModel;

/* What choosing markings variants needs from model `key`'s GLB render data,
   or NULL when it has no variant groups (or out of memory). */
XwaRemasterSkinModel* XwaRemasterSkins_Capture(const AeronGltfModel* model, const char* key);
void XwaRemasterSkins_Free(XwaRemasterSkinModel* skin);

/* The variant each markings value shows, for `snapshot`'s selections and
   uses, up to the highest markings value used (0 when none is); returns
   their count. */
uint32_t XwaRemasterSkins_MarkingsVariants(const XwaRemasterSkinModel* skin, const XwaSnapshot* snapshot,
										   uint32_t variants[XWA_REMASTER_MAX_MARKINGS]);
/* A hash of `count` markings variants, to tell when a mesh's change. */
uint64_t XwaRemasterSkins_MarkingsVariantsHash(const uint32_t* variants, uint32_t count);

/* Misses: an instance whose markings value its skinned mesh was not created
   for shows the highest one's variant; the first per mesh is logged, to find
   markings values the uses missed. */
void XwaRemasterSkins_NoteMesh(const AeronSceneMesh* mesh, const char* key, uint32_t markings_count);
void XwaRemasterSkins_ForgetMesh(const AeronSceneMesh* mesh);
void XwaRemasterSkins_CheckMarkings(const AeronSceneMesh* mesh, uint32_t markings);

#ifdef __cplusplus
}
#endif

#endif
