#ifndef XWA_REMASTER_HUD_READOUT_H
#define XWA_REMASTER_HUD_READOUT_H

/* The target readout: the target's name, cargo, distance and damage bars
 * next to its box, and the component line under the component's box
 * (modern_target_readout_options.h). The boxes module works out the bars and
 * the text module makes the glyphs. This module draws them in their own list,
 * after the target boxes. With hidden_within_m above 0, the list is hidden
 * where the scene's depth is nearer than that, such as behind the cockpit. */

#include "aeron/render.h"
#include "aeron/scene/scene3d.h"

#ifdef __cplusplus
extern "C" {
#endif

int XwaRemasterHudReadout_Init(void);
void XwaRemasterHudReadout_Shutdown(void);
/* Fills the list from the prepared boxes and text. Call it after
   XwaRemasterHudText_PrepareDrawList, outside any pass. */
void XwaRemasterHudReadout_PrepareDrawList(AeronCommandBuffer* cmd, int target_w, int target_h);
/* `scene` is the flight scene, already rendered this frame. Its depth
   covers the whole target. */
void XwaRemasterHudReadout_Render(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
								  int target_w, int target_h, AeronScene3D* scene);

#ifdef __cplusplus
}
#endif

#endif
