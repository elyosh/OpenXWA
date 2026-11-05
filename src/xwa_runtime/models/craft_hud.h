#ifndef XWA_RUNTIME_MODELS_CRAFT_HUD_H
#define XWA_RUNTIME_MODELS_CRAFT_HUD_H

/* Reads each craft's HUD settings from the `hud` group of a model settings
 * file. From the mod's HUD file, it reads the icon tables, the target display
 * settings, the target box settings and the target readout settings. They set the reticle images, the
 * HUD colours, the hull and map icons, how the target display draws the
 * target, and the target readout next to the target's box. The mod
 * documentation describes the keys.
 *
 * The target readout settings go to XwaModernTargetReadoutOptions_SetMod.
 *
 * Hull icons are images of group 26000, map icons of group 14800 and reticle
 * parts of group 12000. A mod file counts images from 1. An object type that
 * the icon tables don't list keeps its stock icon.
 *
 * The edges style of the target display runs a Sobel filter on the luminance
 * of the shaded display, in sRGB values.
 *
 * The game reads the HUD file once at startup. Without the files, the game
 * uses its own tables and colours. */

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
/* `target_boxes.line_width` of hud.yaml, in points: 1/1080 of the 3D view's
   height. 0, the default, keeps the original game's target boxes. */
float XwaCraftHud_TargetBoxLineWidth(void);

#ifdef __cplusplus
}
#endif

#endif
