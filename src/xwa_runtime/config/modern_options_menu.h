#ifndef XWA_RUNTIME_MODERN_OPTIONS_MENU_H
#define XWA_RUNTIME_MODERN_OPTIONS_MENU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	XWA_MODERN_MENU_KEY_ENTER = '\r',
	XWA_MODERN_MENU_KEY_ESCAPE = 0x1b,
	XWA_MODERN_MENU_KEY_LEFT = 0x25,
	XWA_MODERN_MENU_KEY_RIGHT = 0x27,
};

typedef struct XwaModernOptionsMenu {
	/* Row helpers advance y and row as they draw. */
	int center_x;
	int y;
	int row;
	int cursor_row;
	char key;
} XwaModernOptionsMenu;

int XwaModernOptionsMenu_Begin(XwaModernOptionsMenu* menu, int center_x, int start_y, int* cursor_row,
							   int row_count);
void XwaModernOptionsMenu_DrawTitle(XwaModernOptionsMenu* menu, const char* title);
int XwaModernOptionsMenu_DrawAction(XwaModernOptionsMenu* menu, const char* text, int button_id,
									int disabled);
/* Returns -1 for previous, 1 for next/activate, or zero. */
int XwaModernOptionsMenu_DrawValue(XwaModernOptionsMenu* menu, const char* label, const char* value,
								   int button_id, int disabled);
int XwaModernOptionsMenu_DrawCycleU8(XwaModernOptionsMenu* menu, uint8_t* value, const char* label,
									 const char* const* value_texts, int option_count, int button_id,
									 int disabled);
int XwaModernOptionsMenu_TakeEscape(XwaModernOptionsMenu* menu);
int XwaModernOptionsMenu_LastRowSelected(const XwaModernOptionsMenu* menu);

#ifdef __cplusplus
}
#endif

#endif
