#ifndef XWA_RUNTIME_MODELS_CRAFT_HUD_H
#define XWA_RUNTIME_MODELS_CRAFT_HUD_H

/* HUD settings for each craft, and the hull and map icon lists, read from the
 * mod folder.
 *
 * flight/hud.yaml replaces the game's icon tables. Each list gives the image
 * id for each object type:
 *
 *   version: 1
 *   hull_icons: [0, 2600, ...]
 *   map_icons: [0, 100, ...]
 *
 * flight/craft/<model key>.yaml holds one craft's settings. The model key is
 * the name of the craft's GLB, as model_source.h gives it. All keys are
 * optional:
 *
 *   version: 1
 *   hud:
 *     reticle: {5: 135, 6: 136}
 *     colors: {inner: 0xff00ff00, border: 0xff008000}
 *
 * `reticle` gives, for a reticle slot, the image of group 12000 that it
 * draws, as a 1-based position in the group. `colors` gives the HUD's inner
 * and border colours as ARGB.
 *
 * All files are read once at startup. The craft files are only used with
 * model replacements, because the model keys come from them. Without the
 * files, the game uses its own tables and colours. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { XWA_CRAFT_HUD_RETICLE_SLOTS = 23 };

/* Reads the files. Call it after XwaModelSource_Init. */
void XwaCraftHud_Init(void);

/* The hull icon image id of `objectType`. With a `hull_icons` list, this is
   the list's value, or 0 for a type past its end. Without one, it is
   `stockId`. */
uint16_t XwaCraftHud_HullIcon(unsigned int objectType, uint16_t stockId);
/* The same for the map icons and the `map_icons` list. */
uint16_t XwaCraftHud_MapIcon(unsigned int objectType, uint16_t stockId);
/* Replaces colors[0] (inner) and colors[1] (border) with the colours that
   `objectType`'s craft file sets. The colours it doesn't set don't change. */
void XwaCraftHud_Colors(unsigned int objectType, uint32_t colors[2]);
/* The 1-based position in group 12000 that reticle slot `slot` draws for
   `objectType`, or 0 when the craft doesn't change that slot. */
int XwaCraftHud_Reticle(unsigned int objectType, int slot);

#ifdef __cplusplus
}
#endif

#endif
