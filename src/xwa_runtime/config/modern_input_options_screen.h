#ifndef XWA_RUNTIME_MODERN_INPUT_OPTIONS_SCREEN_H
#define XWA_RUNTIME_MODERN_INPUT_OPTIONS_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Draws and updates the modern Game Controller Options screen.
 * Returns 1 for Back or 2 for Controller Setup. */
int XwaModernInputOptionsScreen_Update(int menu_center_x, int* cursor_row);

#ifdef __cplusplus
}
#endif

#endif
