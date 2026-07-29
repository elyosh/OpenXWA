#ifndef XWA_RUNTIME_MODERN_CONTROLLER_OPTIONS_SCREEN_H
#define XWA_RUNTIME_MODERN_CONTROLLER_OPTIONS_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XwaModernControllerScreenResult {
	XWA_MODERN_CONTROLLER_SCREEN_STAY = 0,
	XWA_MODERN_CONTROLLER_SCREEN_BACK,
	XWA_MODERN_CONTROLLER_SCREEN_AXES,
	XWA_MODERN_CONTROLLER_SCREEN_BUTTONS,
} XwaModernControllerScreenResult;

XwaModernControllerScreenResult XwaModernControllerOptionsScreen_Update(int menu_center_x, int* cursor_row);
int XwaModernControllerAxesScreen_Update(int menu_center_x, int* cursor_row);
int XwaModernControllerButtonsScreen_Update(int menu_center_x, int* cursor_row);
void XwaModernControllerOptionsScreen_ResetCapture(void);
void XwaModernControllerOptionsScreen_Leave(void);

#ifdef __cplusplus
}
#endif

#endif
