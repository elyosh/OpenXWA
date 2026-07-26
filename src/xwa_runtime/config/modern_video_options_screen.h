#ifndef XWA_RUNTIME_MODERN_VIDEO_OPTIONS_SCREEN_H
#define XWA_RUNTIME_MODERN_VIDEO_OPTIONS_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Draws and updates the original-widget OpenXWA video options screen.
 * Returns nonzero when the caller should return to the parent video menu. */
int XwaModernVideoOptionsScreen_Update(int menu_center_x, int* cursor_row);

#ifdef __cplusplus
}
#endif

#endif
