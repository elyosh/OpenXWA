#ifndef XWA_APP_OPTIONS_KEYBOARD_PAGE_H
#define XWA_APP_OPTIONS_KEYBOARD_PAGE_H
#include "options/bindings_editor.h"
#include "xwa_runtime/config/modern_input_options.h"

typedef struct XwaKeyboardSettings {
	XwaKeyboardBindings original;
	XwaKeyboardBindings draft;
	XwaBindingsEditor editor;
	AeronKeyChord pending;
	XwaInputAction conflicting_action;
	int conflict_open;
	int restore_open;
	bool dirty;
	bool restore_defaults;
	char error[512];
} XwaKeyboardSettings;

void XwaKeyboardSettings_Open(XwaKeyboardSettings* settings, const XwaModernInputOptions* options);
void XwaKeyboardSettings_Draw(XwaKeyboardSettings* settings, AeronUiContext* ui);
void XwaKeyboardSettings_DrawModals(XwaKeyboardSettings* settings, AeronUiContext* ui);
void XwaKeyboardSettings_CancelCapture(XwaKeyboardSettings* settings, AeronUiContext* ui);
bool XwaKeyboardSettings_Commit(XwaKeyboardSettings* settings, char* error, size_t capacity);
#endif
