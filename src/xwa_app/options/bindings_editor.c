#include "options/bindings_editor.h"
#include <stdio.h>

void XwaBindingsEditor_Init(XwaBindingsEditor* editor) {
	*editor = (XwaBindingsEditor) { .action_selected = SIZE_MAX, .binding_selected = SIZE_MAX };
}

void XwaBindingsEditor_Select(XwaBindingsEditor* editor, XwaInputAction action, bool open_modal) {
	editor->selected_action = action;
	editor->category = XwaInputActions_Category(action);
	editor->action_selected = SIZE_MAX;
	editor->binding_selected = SIZE_MAX;
	if (open_modal)
		editor->binding_modal_open = 1;
}

void XwaBindingsEditor_Category(XwaBindingsEditor* editor, AeronUiContext* ui) {
	static const char* const categories[] = {
		"Weapons", "Targets", "Flight", "View", "HUD", "System", "Comms"
	};
	if (AeronUi_SegmentedSelector(ui, "Action Category", &editor->category, categories,
								  XWA_INPUT_ACTION_CATEGORY_COUNT)) {
		editor->action_selected = SIZE_MAX;
		editor->selected_action = XWA_INPUT_ACTION_NONE;
	}
}

void XwaBindingsEditor_Actions(XwaBindingsEditor* editor, AeronUiContext* ui, const AeronUiListItem* items,
							   size_t count, float trailing_height) {
	if (editor->action_selected == SIZE_MAX && editor->selected_action != XWA_INPUT_ACTION_NONE)
		for (size_t i = 0; i < count; ++i)
			if (items[i].id == (uint64_t)editor->selected_action) {
				editor->action_selected = i;
				break;
			}
	float height = AeronUi_AvailableHeight(ui) - trailing_height;
	if (height < 180.0f)
		height = 180.0f;
	uint32_t result = AeronUi_ListBox(ui, "Actions", items, count, &editor->action_selected, height);
	if ((result & AERON_UI_LIST_ACTIVATED) && editor->action_selected < count)
		XwaBindingsEditor_Select(editor, (XwaInputAction)items[editor->action_selected].id, true);
}

bool XwaBindingsEditor_BeginDetail(XwaBindingsEditor* editor, AeronUiContext* ui) {
	if (!editor->binding_modal_open || editor->selected_action == XWA_INPUT_ACTION_NONE)
		return false;
	return AeronUi_BeginModal(ui, XwaInputActions_DisplayName(editor->selected_action),
							  &editor->binding_modal_open,
							  &(AeronUiWindowDesc) { .width_ref = 720.0f, .centered = 1 }) != 0;
}

void XwaBindingsEditor_List(XwaBindingsEditor* editor, AeronUiContext* ui, const AeronUiListItem* items,
							size_t count, const char* empty_text) {
	AeronUi_Header(ui, "Current Bindings");
	if (count)
		AeronUi_ListBox(ui, "Bindings", items, count, &editor->binding_selected, 180.0f);
	else {
		editor->binding_selected = SIZE_MAX;
		AeronUi_Help(ui, empty_text);
	}
}

bool XwaBindingsEditor_Remove(AeronUiContext* ui) { return AeronUi_Button(ui, "Remove Binding") != 0; }

void XwaBindingsEditor_EndDetail(XwaBindingsEditor* editor, AeronUiContext* ui) {
	if (AeronUi_Button(ui, "Done"))
		editor->binding_modal_open = 0;
	AeronUi_EndModal(ui);
}

bool XwaBindingsEditor_Conflict(AeronUiContext* ui, int* open, const char* source, XwaInputAction previous,
								XwaInputAction replacement) {
	if (!AeronUi_BeginModal(ui, "CONTROL ALREADY BOUND", open, NULL))
		return false;
	char text[512];
	snprintf(text, sizeof text, "%s is assigned to %s. Replace it with %s?", source,
			 XwaInputActions_DisplayName(previous), XwaInputActions_DisplayName(replacement));
	AeronUi_Error(ui, text);
	AeronUi_BeginColumns(ui, 2, NULL);
	bool replace = AeronUi_Button(ui, "Replace") != 0;
	if (replace)
		*open = 0;
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Cancel"))
		*open = 0;
	AeronUi_EndColumns(ui);
	AeronUi_EndModal(ui);
	return replace;
}
