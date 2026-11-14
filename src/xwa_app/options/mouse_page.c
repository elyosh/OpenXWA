#include "options/mouse_page.h"

#include "options/options_window.h"
#include "xwa_runtime/config/modern_input_options.h"

/* The mouse options apply at once and are saved on Close. */
void XwaMousePage_Draw(AeronUiContext* ui, const AeronInputSnapshot* input) {
	static const char* const modes[] = { "Virtual Stick", "Direct" };
	static XwaModernInputOptions options;
	(void)input;
	XwaModernInputOptions_Get(&options);
	int mode = (int)options.mouse_mode;
	AeronUi_Header(ui, "Mouse Flight Control");
	bool changed = AeronUi_Toggle(ui, "Mouse Flight Control", &options.mouse_flight_enabled);
	if (AeronUi_Selector(ui, "Control Mode", &mode, modes, 2)) {
		options.mouse_mode = (XwaModernMouseMode)mode;
		changed = true;
	}
	if (options.mouse_mode == XWA_MODERN_MOUSE_MODE_RATE)
		AeronUi_Help(ui, "Move the mouse to turn. Stop moving to stop turning.");
	else
		AeronUi_Help(ui, "Mouse movement deflects a virtual stick. Move it back to center to stop turning.");
	changed |= AeronUi_SliderInt(ui, "Mouse Sensitivity", &options.mouse_sensitivity,
								 XWA_MODERN_MOUSE_SENSITIVITY_MIN, XWA_MODERN_MOUSE_SENSITIVITY_MAX, 1, "%d");
	changed |= AeronUi_Toggle(ui, "Invert Mouse Y", &options.mouse_invert_y);
	AeronUi_Help(ui, "Left button: fire.");
	AeronUi_Help(ui, "Right button: tap to target the craft in sights, hold to roll.");
	AeronUi_Help(ui, "Middle button: target the nearest enemy fighter.");
	AeronUi_Help(ui, "Side button: padlock view.");
	AeronUi_Help(ui,
				 "Ctrl+Alt+M releases or captures the pointer. Click in the flight view to recapture it.");
	if (AeronUi_Button(ui, "Restore Mouse Defaults")) {
		static XwaModernInputOptions defaults;
		XwaModernInputOptions_GetDefaults(&defaults);
		options.mouse_flight_enabled = defaults.mouse_flight_enabled;
		options.mouse_mode = defaults.mouse_mode;
		options.mouse_sensitivity = defaults.mouse_sensitivity;
		options.mouse_invert_y = defaults.mouse_invert_y;
		changed = true;
	}
	if (changed && !XwaModernInputOptions_Set(&options))
		XwaOptionsWindow_ReportError("The mouse options are invalid.");
}
