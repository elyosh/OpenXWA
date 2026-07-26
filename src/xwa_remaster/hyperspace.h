#ifndef XWA_REMASTER_HYPERSPACE_H
#define XWA_REMASTER_HYPERSPACE_H

#include "aeron/render.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaRemasterHyperspace XwaRemasterHyperspace;

XwaRemasterHyperspace* XwaRemasterHyperspace_Create(void);
void XwaRemasterHyperspace_Destroy(XwaRemasterHyperspace* hyperspace);

/* Resolves any DAT frames and uploads the state-derived streak geometry.
 * Must run before AeronScene_Render opens its render passes. */
int XwaRemasterHyperspace_Prepare(XwaRemasterHyperspace* hyperspace, AeronCommandBuffer* command_buffer,
								  const XwaSnapshot* snapshot, XwaRemasterAssets* assets,
								  const float view_proj[16], const float camera_rows[9], int rt_w, int rt_h);

/* AeronScene BEFORE_OPAQUE hook. The tunnel/flash is drawn first and
 * additive streak quads follow, leaving the normal attachment untouched. */
void XwaRemasterHyperspace_Draw(AeronCommandBuffer* command_buffer, AeronRenderPass* render_pass, int rt_w,
								int rt_h, void* user);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_HYPERSPACE_H */
