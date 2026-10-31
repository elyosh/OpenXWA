#ifndef XWA_RUNTIME_CAMERA_EXTERIOR_HUD_H
#define XWA_RUNTIME_CAMERA_EXTERIOR_HUD_H

/* The HUD in the exterior view. The original game hides most of the HUD
 * there. With the enhanced camera, the exterior view of the player's own
 * craft shows the full HUD, as in the cockpit.
 *
 * This only changes what is drawn. The player's hudEnabled keeps the
 * original game's value, which is off in the exterior view. The simulation
 * reads hudEnabled for the command menu and MFD keys, and films and
 * multiplayer play these keys back, so they must work the same with the
 * feature on or off. So the exterior HUD follows the HUD choice that the
 * player made in the cockpit (savedHudEnabled). The HUD key and the MFD keys
 * still only work in the cockpit. */

#ifdef __cplusplus
extern "C" {
#endif

/* Whether the full HUD is shown in `playerIdx`'s exterior view. It needs
   hardware rendering, the exterior view of the player's own craft, and the
   player's HUD on. */
int XwaExteriorHud_Shown(int playerIdx);

/* Whether `playerIdx`'s exterior view hides the HUD, as in the original
   game. False when the exterior HUD is shown. */
int XwaExteriorHud_HidesHud(int playerIdx);

/* Whether the HUD is drawn: the player's hudEnabled, or the exterior HUD. */
int XwaExteriorHud_HudVisible(int playerIdx);

#ifdef __cplusplus
}
#endif

#endif
