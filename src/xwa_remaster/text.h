#ifndef XWA_REMASTER_TEXT_H
#define XWA_REMASTER_TEXT_H

#include "aeron/scene/draw_list2d.h"
#include "xwa_runtime/runtime/presentation.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Adds one captured frontend glyph in a target that represents the classic
 * 640x480 coordinate space. The target dimensions select the output scale. */
int XwaRemasterText_AddFrontendGlyph(AeronDrawList2D* list, XwaRemasterAssets* assets,
									 const XwaGlyph2D* glyph, int target_width, int target_height);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_TEXT_H */
