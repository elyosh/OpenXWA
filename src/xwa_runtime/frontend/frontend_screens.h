#ifndef XWA_RUNTIME_FRONTEND_FRONTEND_SCREENS_H
#define XWA_RUNTIME_FRONTEND_FRONTEND_SCREENS_H

/* The frontend's screens and their layouts.
 *
 * Each screen is an update function on the screen stack (g_screenStates).
 * Each screen has a name, such as "concourse". A view inside a screen has a
 * dotted name, such as "family_room.rank". The `layouts` table of
 * frontend/frontend.yaml sets the layout of each screen to classic, wide or
 * fill:
 *
 *   layouts:
 *     default: wide
 *     datapad: fill
 *
 * A view that the table doesn't list uses its screen's layout, and a screen
 * that it doesn't list uses `default`. A dialog that it doesn't list uses the
 * layout of the screen below it. Without the table, every screen is classic. */

#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Called by FrontendScreen_RunFrame before it runs the top screen. */
void XwaFrontendScreens_NoteRun(void);
/* The number of frontend frames run so far, across all screens. It is never
 * reset. */
unsigned XwaFrontendScreens_FrameCount(void);

/* The layout of the screen that runs this tick, for the view it shows now.
 * The loading screen isn't on the stack; its name is "loading". */
XwaFrontendLayout XwaFrontendScreens_Layout(void);

/* The factor for text widths, so that text measured in 640x480 units
 * matches the text drawn with its normal shape: 0.75 on a wide 16:9 screen,
 * 1 elsewhere. */
float XwaFrontendScreens_TextWidthScale(void);

/* The text width scale in 1/256 units: 192 on a wide 16:9 screen, else 256.
 * On a wide screen a 640x480 unit is wider than it is tall, but text keeps
 * its shape. So the text functions measure and advance text by its width on
 * screen, and keep the fraction of a unit for the glyph records. Box sizes,
 * centring and wrapping then fit the drawn text. At 256 the results match the
 * original game. */
int XwaFrontendScreens_TextScale256(void);

void XwaFrontendScreens_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
