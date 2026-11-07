#ifndef XWA_RUNTIME_FRONTEND_SIDE_PANELS_H
#define XWA_RUNTIME_FRONTEND_SIDE_PANELS_H

/* The side panels are the animated images leftbar1 to leftbar5 and
 * rightbar1 to rightbar4. They slide in from the bottom corners of some
 * screens and play the "panelarm" sound.
 *
 * With `side_panels` in frontend/frontend.yaml, a mod can hide them on some
 * screens, for example when its backgrounds already show the button slots.
 * There, the panels aren't drawn and "panelarm" doesn't play. The panels
 * still move, so their buttons appear at the usual time. */

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 if image `name` is a side panel that this screen hides. */
int XwaSidePanels_HidesImage(const char* name);

/* Returns 1 if sound `name` is "panelarm" and this screen hides the side
   panels. */
int XwaSidePanels_MutesSound(const char* name);

void XwaSidePanels_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
