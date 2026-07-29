#ifndef XWA_RUNTIME_MODERN_INPUT_OPTIONS_SCREEN_H
#define XWA_RUNTIME_MODERN_INPUT_OPTIONS_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Draws and updates the original-widget OpenXWA input options screen.
 * Returns nonzero when the caller should return to the parent controller menu. */
int XwaModernInputOptionsScreen_Update(int menu_center_x, int* cursor_row);

#ifdef __cplusplus
}
#endif

#endif
