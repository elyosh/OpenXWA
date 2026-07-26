#ifndef XWA_REMASTER_CUTSCENE_H
#define XWA_REMASTER_CUTSCENE_H

#include "aeron/render.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Renders cutscene glyph records into a transparent texture at the physical
 * size of the classic safe frame. Returns NULL when the frame has no visible
 * subtitle glyphs. */
AeronTexture* XwaRemasterCutscene_Render(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot,
										 XwaRemasterAssets* assets, int presentation_width,
										 int presentation_height);

void XwaRemasterCutscene_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_CUTSCENE_H */
