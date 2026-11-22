#ifndef XWA_REMASTER_DEBUG_INTERNAL_H
#define XWA_REMASTER_DEBUG_INTERNAL_H

/*
 * Shared by the debug UI files of src/xwa_remaster/debug. C++ only, built
 * only with the debug UI.
 */

#include "aeron/debug.h"

#include <imgui.h>

/* The menus of the menu bar, in order. */
#define XWA_DEBUG_MENU_DISPLAY "Display"
#define XWA_DEBUG_MENU_IMAGE "Image"
#define XWA_DEBUG_MENU_LIGHTING "Lighting"
#define XWA_DEBUG_MENU_MODELS "Models"
#define XWA_DEBUG_MENU_EFFECTS "Effects"
#define XWA_DEBUG_MENU_COCKPIT "Cockpit"
#define XWA_DEBUG_MENU_AUDIO "Audio"
#define XWA_DEBUG_MENU_VIEW "Debug view"
#define XWA_DEBUG_MENU_DIAGNOSTICS "Diagnostics"

/* Adds a window to a menu. `closed`, when set, runs when the window closes. */
void XwaDebug_AddWindow(const char* menu, const char* title, AeronDebugWindowFn draw, float width,
						float height, AeronDebugWindowFn closed = nullptr);

/* A checkbox for an int flag. Returns true when it changed. */
bool XwaDebug_Checkbox(const char* label, int* value);

/* A "Reset" button that puts back the values of remaster/config.yaml. */
bool XwaDebug_ResetButton(const char* label = "Reset");

/* Two-column table rows for read-only values: begin, add rows, end. */
bool XwaDebug_BeginValues(const char* id);
void XwaDebug_Value(const char* name, const char* format, ...) IM_FMTARGS(2);
void XwaDebug_EndValues(void);

/* The source that the dynamic cockpit boxes highlight, or -1. The Dynamic
 * Cockpit window picks it and the Debug view menu shows the boxes. */
extern int g_xwa_debug_dc_highlight;

/* Each file adds its windows. */
void XwaDebugDisplay_Register(void);
void XwaDebugImage_Register(void);
void XwaDebugLighting_Register(void);
void XwaDebugModels_Register(void);
void XwaDebugEffects_Register(void);
void XwaDebugCockpit_Register(void);
void XwaDebugDiagnostics_Register(void);
void XwaDebugView_Register(void);

/* The legend of the false color view, drawn by the Exposure window. */
void XwaDebugView_DrawFalseColorLegend(void);

#endif /* XWA_REMASTER_DEBUG_INTERNAL_H */
