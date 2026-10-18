#ifndef XWA_RUNTIME_FRONTEND_FRONTEND_SCREENS_H
#define XWA_RUNTIME_FRONTEND_FRONTEND_SCREENS_H

/* The frontend's screens and their layouts.
 *
 * Each screen is an update function on the screen stack (g_screenStates).
 * This module gives each one a stable name, such as "concourse" or
 * "family_room.rank" for a view inside a screen, and takes its layout from
 * the `layouts` table of frontend/frontend.yaml:
 *
 *   layouts:
 *     default: wide
 *     datapad: fill
 *
 * A view falls back to its screen, and a screen to `default`. A dialog
 * pushed over a screen takes that screen's layout unless the table lists
 * it. Without the table, every screen is classic. */

#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Called by FrontendScreen_RunFrame before it runs the top screen. */
void XwaFrontendScreens_NoteRun(void);

/* The layout of the screen that runs this tick, with its view as it is now.
 * The loading screen, which isn't on the stack, is "loading". */
XwaFrontendLayout XwaFrontendScreens_Layout(void);

/* What the game multiplies text widths by, so that text measured in 640x480
 * units matches the text drawn at its own shape: 0.75 in a wide 16:9
 * screen, 1 elsewhere. */
float XwaFrontendScreens_TextWidthScale(void);

void XwaFrontendScreens_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
