#ifndef XWA_RUNTIME_MODERN_PILOT_PROFILES_SCREEN_H
#define XWA_RUNTIME_MODERN_PILOT_PROFILES_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Draws and updates the OpenXWA pilot profile management screen.
 * Returns nonzero when the caller should return to the configuration menu. */
int XwaModernPilotProfilesScreen_Update(int menu_center_x, int* cursor_row);
void XwaModernPilotProfilesScreen_Leave(void);
int XwaModernPilotProfilesScreen_TakeActiveChanged(void);

#ifdef __cplusplus
}
#endif

#endif
