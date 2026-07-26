#ifndef XWA_REMASTER_FRONTEND_H
#define XWA_REMASTER_FRONTEND_H

/*
 * Frontend 2D reconstruction driver: replays the snapshot's
 * draws/paints/glyphs/surface events/model previews onto persistent
 * HD render targets. One scene driver among peers — the per-frame
 * entry, view-mode state and layer submission live in xwa_remaster.c.
 */

#include "aeron/render.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reconstruct `snap` (a FRONTEND-scene tick) and return the composed
 * frame texture (borrowed; persistent output RT). NULL on setup
 * failure. `cmd` must have no open pass; `assets` is borrowed for the
 * call. */
AeronTexture* XwaRemasterFrontend_Render(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
										 XwaRemasterAssets* assets);

void XwaRemasterFrontend_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_FRONTEND_H */
