#ifndef XWA_APP_OPTIONS_CONTROLLER_PAGE_H
#define XWA_APP_OPTIONS_CONTROLLER_PAGE_H

#include <stdbool.h>
#include <stddef.h>

#include "aeron/scene/ui.h"
#include "options/bindings_editor.h"
#include "xwa_runtime/config/modern_input_options.h"

#define XWA_CONTROLLER_SETTINGS_ERROR_CAPACITY 512

typedef struct XwaControllerSettings {
	XwaBindingsEditor editor;
	XwaControllerOptions original;
	XwaControllerOptions draft;
	XwaControllerProfile unconfigured;
	char selected_guid[33];
	uint32_t selected_instance;
	AeronControllerKind active_kind;
	char conflict_text[512];
	bool capacity_warned;
	int page;
	int axis;
	XwaInputAxis pending_axis;
	int pending_axis_source;
	AeronControllerDigitalSource pending_digital;
	XwaInputAction conflicting_action;
	uint32_t active_instance;
	int axis_conflict_open;
	int binding_conflict_open;
	int restore_modal_open;
	bool dirty;
	char error[XWA_CONTROLLER_SETTINGS_ERROR_CAPACITY];
} XwaControllerSettings;

void XwaControllerSettings_Discover(XwaControllerSettings* settings, AeronUiContext* ui,
									const AeronInputSnapshot* input, const XwaControllerProfile* defaults);
void XwaControllerSettings_Open(XwaControllerSettings* settings, const XwaModernInputOptions* options);
void XwaControllerSettings_Draw(XwaControllerSettings* settings, AeronUiContext* ui,
								const AeronInputSnapshot* input);
void XwaControllerSettings_DrawModals(XwaControllerSettings* settings, AeronUiContext* ui,
									  const AeronInputSnapshot* input);
bool XwaControllerSettings_Commit(XwaControllerSettings* settings, char* error, size_t error_capacity);
void XwaControllerSettings_CancelCapture(XwaControllerSettings* settings, AeronUiContext* ui);

#endif
