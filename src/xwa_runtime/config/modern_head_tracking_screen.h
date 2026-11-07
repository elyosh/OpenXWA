#ifndef XWA_RUNTIME_MODERN_HEAD_TRACKING_SCREEN_H
#define XWA_RUNTIME_MODERN_HEAD_TRACKING_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 on Back/Escape, 0 while the head-tracking page remains open. */
int XwaModernHeadTrackingScreen_Update(int menu_center_x, int* cursor_row);

#ifdef __cplusplus
}
#endif

#endif
