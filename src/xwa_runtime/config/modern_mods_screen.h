#ifndef XWA_RUNTIME_MODERN_MODS_SCREEN_H
#define XWA_RUNTIME_MODERN_MODS_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Draws and updates the screen that chooses the mod OpenXWA uses.
 * Returns nonzero when the caller should return to the configuration menu. */
int XwaModernModsScreen_Update(int menu_center_x, int* cursor_row);

#ifdef __cplusplus
}
#endif

#endif
