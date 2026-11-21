#ifndef XWA_REMASTER_VR_KEYBOARD_H
#define XWA_REMASTER_VR_KEYBOARD_H

/* Draws the virtual keyboard of VR and types with it. The layout and the
 * open state are the runtime's (xwa_runtime/input/vr_keyboard.h).
 *
 * The keyboard is a flat panel in the headset, in front of the player and
 * below the virtual screen. Each hand points at it with its own ray. The
 * key under a ray is highlighted, and the hand's trigger presses it. A
 * press is sent to the game as the same key events that a real keyboard
 * gives, so every text field works with it. */

#include "xwa_remaster/assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Whether a hand gets a short haptic tick when it enters or presses a key. */
void XwaRemasterVrKeyboard_SetHaptics(int enabled);

/* Reads the hands, sends the keys and draws the keyboard for this frame.
 * Call it once for each frame, outside any render pass. It does nothing
 * while the keyboard is closed or no VR session runs. */
void XwaRemasterVrKeyboard_Frame(XwaRemasterAssets* assets);

/* Whether the hand points at the keyboard: its trigger then presses keys
 * and must not reach the bindings. `hand` is 0 for the left hand, 1 for
 * the right one. */
int XwaRemasterVrKeyboard_HandOn(int hand);
/* Where the hand's ray meets the keyboard, in the rig's space, in metres.
 * Returns 0 when the hand doesn't point at the keyboard. */
int XwaRemasterVrKeyboard_HandPoint(int hand, float point_m[3]);

void XwaRemasterVrKeyboard_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
