#ifndef XWA_APP_OPTIONS_BINDINGS_EDITOR_H
#define XWA_APP_OPTIONS_BINDINGS_EDITOR_H

#include "aeron/scene/ui.h"
#include "xwa_runtime/input/actions.h"

typedef struct XwaBindingsEditor {
	int category;
	size_t action_selected;
	size_t binding_selected;
	XwaInputAction selected_action;
	int binding_modal_open;
} XwaBindingsEditor;

void XwaBindingsEditor_Init(XwaBindingsEditor* editor);
void XwaBindingsEditor_Select(XwaBindingsEditor* editor, XwaInputAction action, bool open_modal);
void XwaBindingsEditor_Category(XwaBindingsEditor* editor, AeronUiContext* ui);
void XwaBindingsEditor_Actions(XwaBindingsEditor* editor, AeronUiContext* ui, const AeronUiListItem* items,
							   size_t count, float trailing_height);
bool XwaBindingsEditor_BeginDetail(XwaBindingsEditor* editor, AeronUiContext* ui);
void XwaBindingsEditor_List(XwaBindingsEditor* editor, AeronUiContext* ui, const AeronUiListItem* items,
							size_t count, const char* empty_text);
bool XwaBindingsEditor_Remove(AeronUiContext* ui);
void XwaBindingsEditor_EndDetail(XwaBindingsEditor* editor, AeronUiContext* ui);
/* Returns true only when Replace is chosen. The dialog owns its open flag. */
bool XwaBindingsEditor_Conflict(AeronUiContext* ui, int* open, const char* source, XwaInputAction previous,
								XwaInputAction replacement);
#endif
