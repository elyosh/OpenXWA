#ifndef XWA_REMASTER_SKINS_H
#define XWA_REMASTER_SKINS_H

/* Markings on the renderer side: the variant a model's mesh shows for each
 * markings value, from the snapshot's skin selections and uses
 * (xwa_runtime/models/skins.h). The mesh's variant index i is markings value
 * i (AeronSceneMeshInstance.variant = node_switch):
 *   - a used markings value shows the mission's selection for the model's
 *     GLB, else its markings list entry (Aeron_GltfMarkingsVariant); a
 *     selected name the GLB doesn't have falls back to the list, logged;
 *   - an unused one shows the own materials, and its variant's textures
 *     are not loaded;
 *   - a model no object is known to use uses markings 0, which objects
 *     start with.
 * Models without variants keep a single variant index. */

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
   or NULL when it has no variants (or out of memory). */
XwaRemasterSkinModel* XwaRemasterSkins_Capture(const AeronGltfModel* model, const char* key);
void XwaRemasterSkins_Free(XwaRemasterSkinModel* skin);

/* The variant each markings value shows, for `snapshot`'s selections and
   uses, up to the highest markings value used (0 when none is); returns
   their count. */
uint32_t XwaRemasterSkins_MarkingsVariants(const XwaRemasterSkinModel* skin, const XwaSnapshot* snapshot,
										   uint32_t variants[XWA_REMASTER_MAX_MARKINGS]);
/* A hash of `count` markings variants, to tell when a mesh's change. */
uint64_t XwaRemasterSkins_MarkingsVariantsHash(const uint32_t* variants, uint32_t count);

/* Misses: an instance whose markings value its mesh was not created for
   shows the highest one's variant; the first per mesh is logged, to find
   markings values the uses missed. */
void XwaRemasterSkins_NoteMesh(const AeronSceneMesh* mesh, const char* key, uint32_t markings_count);
void XwaRemasterSkins_ForgetMesh(const AeronSceneMesh* mesh);
void XwaRemasterSkins_CheckMarkings(const AeronSceneMesh* mesh, uint32_t markings);

#ifdef __cplusplus
}
#endif

#endif
