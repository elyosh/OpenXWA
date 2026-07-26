#ifndef XWA_REMASTER_PREVIEW_H
#define XWA_REMASTER_PREVIEW_H

/*
 * Frontend model-preview PiP: renders one XwaModelPreview record as a
 * true AeronScene3D PBR draw of the cooked ship mesh
 * (<configured asset root>/remaster/models/<opt>.glb — the OPT -> opt2gltf ->
 * aeron_gltf_cook pipeline) into an internal HDR render target. The caller
 * composites the returned texture into the 2D reconstruction at the
 * record's z position. No classic renderer involvement.
 */

#include "aeron/render.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Render preview `p` into the internal RT for `slot` (one per
 * model_previews[] index). `lights`/`light_count` are the snapshot's
 * shared dir_lights channel (the engine's own preview light). Engine
 * glows are STATE-DERIVED from the cooked mesh's own glb extras at the
 * classic preview base scale; `glow_tex` is the resolved
 * LightingEffects frame (NULL = no glows drawn). Returns the slot's
 * tonemapped texture (borrowed; valid until the next render into the
 * same slot); a missing cooked mesh renders an empty transparent PiP.
 * `cmd` must have no open pass. */
AeronTexture* XwaRemasterPreview_Render(AeronCommandBuffer* cmd, const XwaModelPreview* p,
										int slot, const XwaDirLight* lights,
										uint32_t light_count, const XwaAssetRef* glow_tex);

void XwaRemasterPreview_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_PREVIEW_H */
