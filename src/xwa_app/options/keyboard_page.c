#include "options/keyboard_page.h"
#include <stdio.h>
#include <string.h>

void XwaKeyboardSettings_Open(XwaKeyboardSettings* settings, const XwaModernInputOptions* options) {
	memset(settings, 0, sizeof *settings);
	settings->original = settings->draft = options->keyboard;
	XwaBindingsEditor_Init(&settings->editor);
}

void XwaKeyboardSettings_CancelCapture(XwaKeyboardSettings* settings, AeronUiContext* ui) {
	AeronUi_CancelKeyboardCapture(ui);
	settings->editor.binding_modal_open = 0;
	settings->conflict_open = settings->restore_open = 0;
}

static void Changed(XwaKeyboardSettings* settings) {
	XwaKeyboardMapping_Sort(&settings->draft);
	settings->dirty =
		settings->restore_defaults || !XwaKeyboardMapping_Equal(&settings->original, &settings->draft);
	settings->restore_defaults = false;
	settings->editor.binding_selected = SIZE_MAX;
	settings->error[0] = 0;
}

static const char* ReservedShortcutMessage(XwaKeyboardShortcut shortcut) {
	switch (shortcut) {
		case XWA_KEYBOARD_SHORTCUT_OPTIONS:
			return "Escape opens the options and can't be changed.";
		case XWA_KEYBOARD_SHORTCUT_DEBUG:
			return "The grave accent key opens the debug menu and can't be changed.";
		case XWA_KEYBOARD_SHORTCUT_RENDERER:
			return "Ctrl+F5 switches between modern and classic graphics and can't be changed.";
		case XWA_KEYBOARD_SHORTCUT_SPLIT_VIEW:
			return "Ctrl+F2 shows modern and classic graphics side by side and can't be changed.";
		case XWA_KEYBOARD_SHORTCUT_MOUSE:
			return "Ctrl+Alt+M releases or captures the mouse and can't be changed.";
		case XWA_KEYBOARD_SHORTCUT_MOUSE_LOOK:
			return "Ctrl+Alt+L turns cockpit mouse look on or off and can't be changed.";
		case XWA_KEYBOARD_SHORTCUT_HOLOGRAMS:
			return "Ctrl+H turns the cockpit holograms on or off and can't be changed.";
#if defined(__APPLE__)
		case XWA_KEYBOARD_SHORTCUT_HOST_PAUSE:
			return "Command+P pauses the game and can't be changed.";
		case XWA_KEYBOARD_SHORTCUT_FULLSCREEN:
			return "Command+Ctrl+F switches fullscreen and can't be changed.";
#else
		case XWA_KEYBOARD_SHORTCUT_HOST_PAUSE:
			return "Super+P pauses the game and can't be changed.";
		case XWA_KEYBOARD_SHORTCUT_FULLSCREEN:
			return "Alt+Return switches fullscreen and can't be changed.";
#endif
		default:
			return NULL;
	}
}

static bool Captured(XwaKeyboardSettings* settings, AeronUiContext* ui, const char* label,
					 const char* display, AeronKeyChord* chord) {
	if (AeronUi_KeyboardCapture(ui, label, display, chord) != AERON_UI_KEYBOARD_CAPTURE_CAPTURED)
		return false;
	const char* reserved = ReservedShortcutMessage(XwaKeyboardMapping_Shortcut(*chord));
	if (reserved) {
		snprintf(settings->error, sizeof settings->error, "%s", reserved);
		return false;
	}
	if (!XwaKeyboardMapping_SourceValid(*chord)) {
		snprintf(settings->error, sizeof settings->error, "This key combination is not supported.");
		return false;
	}
	settings->error[0] = 0;
	return true;
}

static void AddBinding(XwaKeyboardSettings* settings, AeronKeyChord source) {
	size_t existing = XwaKeyboardMapping_Find(&settings->draft, source);
	if (existing != SIZE_MAX) {
		XwaInputAction action = settings->draft.bindings[existing].action;
		if (action == settings->editor.selected_action) {
			/* The Options action lists the fixed Escape key first. */
			size_t position = action == XWA_INPUT_ACTION_OPTIONS_MENU;
			for (size_t i = 0; i < existing; ++i)
				position += settings->draft.bindings[i].action == action;
			settings->editor.binding_selected = position;
		} else {
			settings->pending = source;
			settings->conflicting_action = action;
			settings->conflict_open = 1;
		}
		return;
	}
	if (settings->draft.count == XWA_KEYBOARD_BINDING_CAP) {
		snprintf(settings->error, sizeof settings->error, "Remove a binding before adding another.");
		return;
	}
	settings->draft.bindings[settings->draft.count++] =
		(XwaKeyboardBinding) { source, settings->editor.selected_action };
	Changed(settings);
}

static void DescribeAction(const XwaKeyboardBindings* profile, XwaInputAction action, char* text,
						   size_t capacity) {
	snprintf(text, capacity, "%s", action == XWA_INPUT_ACTION_OPTIONS_MENU ? "Escape (fixed)" : "");
	for (size_t i = 0; i < profile->count; ++i) {
		if (profile->bindings[i].action != action)
			continue;
		char label[128];
		XwaKeyboardMapping_FormatSource(label, sizeof label, profile->bindings[i].source);
		size_t length = strlen(text);
		int written = snprintf(text + length, capacity - length, "%s%s", length ? "   " : "", label);
		if (written < 0 || (size_t)written >= capacity - length)
			break;
	}
	if (!text[0])
		snprintf(text, capacity, "Not Bound");
}

void XwaKeyboardSettings_Draw(XwaKeyboardSettings* settings, AeronUiContext* ui) {
	XwaBindingsEditor_Category(&settings->editor, ui);
	AeronKeyChord source;
	if (Captured(settings, ui, "Find Binding...", "Press to identify", &source)) {
		size_t index = XwaKeyboardMapping_Find(&settings->draft, source);
		if (index == SIZE_MAX)
			snprintf(settings->error, sizeof settings->error, "This key combination is not bound.");
		else
			XwaBindingsEditor_Select(&settings->editor, settings->draft.bindings[index].action, false);
	}
	AeronUi_Spacer(ui, 8.0f);
	AeronUiListItem items[XWA_INPUT_ACTION_COUNT - 1];
	char details[XWA_INPUT_ACTION_COUNT - 1][256];
	size_t count = 0;
	for (int action = XWA_INPUT_ACTION_NONE + 1; action < XWA_INPUT_ACTION_COUNT; ++action) {
		if (!XwaInputActions_KeyboardBindable((XwaInputAction)action) ||
			(int)XwaInputActions_Category((XwaInputAction)action) != settings->editor.category)
			continue;
		DescribeAction(&settings->draft, (XwaInputAction)action, details[count], sizeof details[count]);
		items[count] = (AeronUiListItem) { .id = (uint64_t)action,
										   .label = XwaInputActions_DisplayName((XwaInputAction)action),
										   .detail = details[count] };
		++count;
	}
	float trailing_height = 143.0f;
	if (settings->error[0])
		trailing_height += AeronUi_MeasureHelpHeight(ui, settings->error, 0.0f) + 60.0f;
	XwaBindingsEditor_Actions(&settings->editor, ui, items, count, trailing_height);
	if (settings->error[0]) {
		AeronUi_Error(ui, settings->error);
		if (AeronUi_Button(ui, "Dismiss Error"))
			settings->error[0] = 0;
	}
	if (AeronUi_Button(ui, "Restore Defaults")) {
		AeronUi_CancelKeyboardCapture(ui);
		settings->restore_open = 1;
	}
}

static void Detail(XwaKeyboardSettings* settings, AeronUiContext* ui) {
	if (!XwaBindingsEditor_BeginDetail(&settings->editor, ui))
		return;
	AeronUiListItem items[XWA_KEYBOARD_BINDING_CAP + 1];
	char labels[XWA_KEYBOARD_BINDING_CAP + 1][128];
	size_t count = 0;
	if (settings->editor.selected_action == XWA_INPUT_ACTION_OPTIONS_MENU)
		items[count++] = (AeronUiListItem) { .id = SIZE_MAX, .label = "Escape", .detail = "Fixed shortcut" };
	for (size_t i = 0; i < settings->draft.count; ++i) {
		if (settings->draft.bindings[i].action != settings->editor.selected_action)
			continue;
		XwaKeyboardMapping_FormatSource(labels[count], sizeof labels[count],
										settings->draft.bindings[i].source);
		items[count] = (AeronUiListItem) { .id = i, .label = labels[count] };
		++count;
	}
	XwaBindingsEditor_List(&settings->editor, ui, items, count, "This action has no keyboard binding.");
	if (settings->editor.binding_selected < count &&
		items[settings->editor.binding_selected].id != SIZE_MAX && XwaBindingsEditor_Remove(ui)) {
		XwaKeyboardMapping_Remove(&settings->draft, (size_t)items[settings->editor.binding_selected].id);
		Changed(settings);
	}
	AeronUi_Separator(ui);
	AeronKeyChord source;
	if (Captured(settings, ui, "Add Binding...", "Press to add", &source))
		AddBinding(settings, source);
	if (settings->error[0])
		AeronUi_Error(ui, settings->error);
	XwaBindingsEditor_EndDetail(&settings->editor, ui);
}

static void Restore(XwaKeyboardSettings* settings, AeronUiContext* ui) {
	if (!AeronUi_BeginModal(ui, "Restore Keyboard Defaults", &settings->restore_open, NULL))
		return;
	AeronUi_Help(ui, "Replace all keyboard bindings with the defaults?");
	AeronUi_BeginColumns(ui, 2, NULL);
	if (AeronUi_Button(ui, "Restore")) {
		static XwaModernInputOptions defaults;
		XwaModernInputOptions_GetDefaults(&defaults);
		settings->draft = defaults.keyboard;
		settings->restore_defaults = settings->dirty = true;
		settings->restore_open = 0;
		settings->error[0] = 0;
		XwaBindingsEditor_Init(&settings->editor);
	}
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Cancel"))
		settings->restore_open = 0;
	AeronUi_EndColumns(ui);
	AeronUi_EndModal(ui);
}

void XwaKeyboardSettings_DrawModals(XwaKeyboardSettings* settings, AeronUiContext* ui) {
	if (settings->conflict_open) {
		char label[128];
		XwaKeyboardMapping_FormatSource(label, sizeof label, settings->pending);
		if (XwaBindingsEditor_Conflict(ui, &settings->conflict_open, label, settings->conflicting_action,
									   settings->editor.selected_action)) {
			size_t existing = XwaKeyboardMapping_Find(&settings->draft, settings->pending);
			if (existing != SIZE_MAX) {
				settings->draft.bindings[existing].action = settings->editor.selected_action;
				Changed(settings);
			}
		}
	} else if (settings->restore_open) {
		Restore(settings, ui);
	} else {
		Detail(settings, ui);
	}
}

bool XwaKeyboardSettings_Commit(XwaKeyboardSettings* settings, char* error, size_t capacity) {
	static XwaModernInputOptions options;
	if (!settings->dirty)
		return true;
	XwaModernInputOptions_Get(&options);
	options.keyboard = settings->draft;
	/* The apply callback installs the bindings. */
	if (!XwaModernInputOptions_Set(&options)) {
		if (error && capacity)
			snprintf(error, capacity, "The keyboard bindings are invalid.");
		return false;
	}
	XwaModernInputOptions_Get(&options);
	settings->draft = settings->original = options.keyboard;
	settings->restore_defaults = settings->dirty = false;
	return true;
}
