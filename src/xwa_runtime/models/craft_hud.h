#ifndef XWA_RUNTIME_MODELS_CRAFT_HUD_H
#define XWA_RUNTIME_MODELS_CRAFT_HUD_H

/* Reads each craft's HUD settings and the hull and map icon lists from the
 * mod folder.
 *
 * flight/hud.yaml replaces the game's icon tables. Each list gives the image
 * id for each object type:
 *
 *   version: 1
 *   hull_icons: [0, 2600, ...]
 *   map_icons: [0, 100, ...]
 *
 * The `hud` group of a model settings file holds the craft's own HUD settings.
 * All keys are optional:
 *
 *   hud:
 *     reticle: {5: 135, 6: 136}
 *     colors: {inner: 0xff00ff00, border: 0xff008000}
 *
 * `reticle` maps a reticle slot to the image of group 12000 that it draws,
 * given as a 1-based position in the group. `colors` sets the HUD's inner and
 * border colours, as ARGB.
 *
 * The game reads hud.yaml once at startup. Without the files, the game uses
 * its own tables and colours. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

enum { XWA_CRAFT_HUD_RETICLE_SLOTS = 23 };

/* Reads flight/hud.yaml. */
void XwaCraftHud_Init(void);
/* Drops the previous mission's HUD settings. Called by the model settings
   reader. */
void XwaCraftHud_ClearModelSettings(void);
/* Reads the `hud` group of `objectType`'s model settings file, or does
   nothing when `group` is NULL. `path` is the file, for the log. Called by
   the model settings reader. */
void XwaCraftHud_ReadModelSettings(unsigned int objectType, const char* path,
								   const struct AeronConfigNode* group);

/* Returns the image id of the hull icon for `objectType`. With a
   `hull_icons` list, it returns the list's value, or 0 for a type past the
   end of the list. Without a list, it returns `stockId`. */
uint16_t XwaCraftHud_HullIcon(unsigned int objectType, uint16_t stockId);
/* Works like XwaCraftHud_HullIcon, for the map icons and the `map_icons`
   list. */
uint16_t XwaCraftHud_MapIcon(unsigned int objectType, uint16_t stockId);
/* Replaces colors[0] (inner) and colors[1] (border) with the colours that
   the model settings file of `objectType`'s model sets. A colour that the file doesn't set
   stays the same. */
void XwaCraftHud_Colors(unsigned int objectType, uint32_t colors[2]);
/* Returns the image that reticle slot `slot` draws for `objectType`, as a
   1-based position in group 12000. Returns 0 when the craft doesn't change
   that slot. */
int XwaCraftHud_Reticle(unsigned int objectType, int slot);

#ifdef __cplusplus
}
#endif

#endif
