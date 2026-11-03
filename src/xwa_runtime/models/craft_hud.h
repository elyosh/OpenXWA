#ifndef XWA_RUNTIME_MODELS_CRAFT_HUD_H
#define XWA_RUNTIME_MODELS_CRAFT_HUD_H

/* Reads each craft's HUD settings and the hull and map icons from the mod
 * folder.
 *
 * flight/hud.yaml changes the game's icon tables. Each map gives object types
 * an icon, as an image number counted from 1, or `none` for no icon. Hull
 * icons are in image group 26000 and map icons in group 14800. An object type
 * that a map doesn't list keeps its stock icon.
 *
 *   hull_icons: {xwing: 1, slot_33: 49, ywing: none}
 *   map_icons: {xwing: 1}
 *
 * The `hud` group of a model settings file holds the craft's own HUD
 * settings. All keys are optional:
 *
 *   hud:
 *     reticle:
 *       laser: 135
 *       laser_in_range: 136
 *       threats:
 *         laser_mode: {laser: 141, turret: 142, beam: 143, warhead: 144}
 *     colors: {inner: 0xff00ff00, border: 0xff008000}
 *
 * `reticle` sets which image of HUD image group 12000 each part of the
 * reticle draws, as a number counted from 1. The parts are:
 *   - laser and laser_in_range: the laser reticle, and the overlay drawn
 *     when the target is in range;
 *   - warhead and warhead_lock: the warhead reticle, and the lock overlay;
 *   - cannon_ready and cannon: the pip of each cannon when it is ready, and
 *     when it isn't;
 *   - threats.laser_mode and threats.warhead_mode: the threat indicators in
 *     each weapon mode. Each mode has laser (incoming laser fire), turret (a
 *     turret aiming at the player), beam and warhead (an incoming warhead).
 * `colors` sets the HUD's inner and border colours, as ARGB.
 *
 * The `target_display` group of hud.yaml sets how the target display shows
 * the target:
 *
 *   target_display:
 *     style: edges            # or shaded, the default
 *     edges:
 *       luminance: [0.33, 0.5, 0.16]
 *       contrast: 3.0
 *       colors: iff           # or friend_foe
 *       iff: [[0.01, 0.45, 0.01], ...]   # six colours, by IFF 0 to 5
 *       friend: [0.01, 0.32, 0.01]
 *       foe: [0.32, 0.01, 0.01]
 *       other: [0.01, 0.01, 0.21]
 *
 * `shaded` shows the lit model. `edges` shows only its edges: a Sobel filter
 * on the luminance of the shaded display, in sRGB values, weighted by
 * `luminance` and scaled by `contrast`. The edges take the colour of the
 * target's IFF, or the friend or foe colour with `colors: friend_foe`.
 * `other` is the colour of a target without an IFF colour. Colours are
 * linear. With `edges`, every key is required, a 6-pixel border stays empty,
 * and the display is empty while the target is dying.
 *
 * The game reads hud.yaml once at startup. Without the files, the game uses
 * its own tables and colours. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

enum { XWA_CRAFT_HUD_RETICLE_SLOTS = 23, XWA_TARGET_DISPLAY_IFF_COLORS = 6 };

typedef enum XwaTargetDisplayStyle {
	XWA_TARGET_DISPLAY_SHADED = 0,
	XWA_TARGET_DISPLAY_EDGES,
} XwaTargetDisplayStyle;

/* The `target_display` group of hud.yaml. Colours are linear. */
typedef struct XwaTargetDisplay {
	XwaTargetDisplayStyle style;
	float luminance[3];
	float contrast;
	int friendFoe; /* `colors: friend_foe` */
	float iff[XWA_TARGET_DISPLAY_IFF_COLORS][3];
	float friendColor[3];
	float foeColor[3];
	float otherColor[3];
} XwaTargetDisplay;

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

/* Returns the image id of the hull icon for `objectType`, or 0 for no icon.
   When `hull_icons` doesn't list the object type, returns `stockId`. */
uint16_t XwaCraftHud_HullIcon(unsigned int objectType, uint16_t stockId);
/* Works like XwaCraftHud_HullIcon, for the map icons and the `map_icons`
   list. */
uint16_t XwaCraftHud_MapIcon(unsigned int objectType, uint16_t stockId);
/* Replaces colors[0] (inner) and colors[1] (border) with the colours that
   the model settings file of `objectType`'s model sets. A colour that the
   file doesn't set stays the same. */
void XwaCraftHud_Colors(unsigned int objectType, uint32_t colors[2]);
/* Returns the image that reticle slot `slot` draws for `objectType`, as a
   1-based position in group 12000. Returns 0 when the craft doesn't change
   that slot. */
int XwaCraftHud_Reticle(unsigned int objectType, int slot);
/* The `target_display` group of hud.yaml, or the default: shaded. */
const XwaTargetDisplay* XwaCraftHud_TargetDisplay(void);

#ifdef __cplusplus
}
#endif

#endif
