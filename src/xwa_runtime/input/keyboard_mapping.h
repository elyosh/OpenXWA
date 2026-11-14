#ifndef XWA_KEYBOARD_MAPPING_H
#define XWA_KEYBOARD_MAPPING_H

#include "aeron/input.h"
#include "xwa_runtime/input/actions.h"
#include <stddef.h>

#define XWA_KEYBOARD_BINDING_CAP 256

typedef struct XwaKeyboardBinding {
	AeronKeyChord source;
	XwaInputAction action;
} XwaKeyboardBinding;

typedef struct XwaKeyboardBindings {
	XwaKeyboardBinding bindings[XWA_KEYBOARD_BINDING_CAP];
	size_t count;
} XwaKeyboardBindings;

/* Key chords that the host keeps. They can't be bound. */
typedef enum XwaKeyboardShortcut {
	XWA_KEYBOARD_SHORTCUT_NONE,
	XWA_KEYBOARD_SHORTCUT_OPTIONS,    /* Escape: the options datapad, AeronUi cancel */
	XWA_KEYBOARD_SHORTCUT_DEBUG,      /* Grave, when the debug UI is available */
	XWA_KEYBOARD_SHORTCUT_RENDERER,   /* Ctrl+F5: modern or classic renderer */
	XWA_KEYBOARD_SHORTCUT_SPLIT_VIEW, /* Ctrl+F2: split view */
	XWA_KEYBOARD_SHORTCUT_MOUSE,      /* Ctrl+Alt+M: release or recapture the mouse */
	XWA_KEYBOARD_SHORTCUT_MOUSE_LOOK, /* Ctrl+Alt+L: cockpit mouse look */
	XWA_KEYBOARD_SHORTCUT_HOLOGRAMS,  /* Ctrl+H without Alt: Dynamic Cockpit holograms */
	XWA_KEYBOARD_SHORTCUT_HOST_PAUSE, /* GUI+P */
	XWA_KEYBOARD_SHORTCUT_FULLSCREEN, /* GUI+Ctrl+F on macOS, Alt+Return elsewhere */
} XwaKeyboardShortcut;

void XwaKeyboardMapping_SetPolicy(bool debug_available);
int XwaKeyboardMapping_Trigger(const AeronInputSnapshot* input, XwaKeyboardShortcut shortcut);
XwaKeyboardShortcut XwaKeyboardMapping_Shortcut(AeronKeyChord source);
bool XwaKeyboardMapping_SourceValid(AeronKeyChord source);
void XwaKeyboardMapping_FormatSource(char* text, size_t capacity, AeronKeyChord source);
size_t XwaKeyboardMapping_Find(const XwaKeyboardBindings* profile, AeronKeyChord source);
bool XwaKeyboardMapping_Equal(const XwaKeyboardBindings* a, const XwaKeyboardBindings* b);
void XwaKeyboardMapping_Sort(XwaKeyboardBindings* profile);
void XwaKeyboardMapping_Remove(XwaKeyboardBindings* profile, size_t index);
void XwaKeyboardMapping_Install(const XwaKeyboardBindings* profile);
void XwaKeyboardMapping_Suspend(void);
void XwaKeyboardMapping_Enable(bool enabled, const AeronInputSnapshot* input);
void XwaKeyboardMapping_BeginFrame(const AeronInputSnapshot* input);
void XwaKeyboardMapping_Event(const AeronKeyEvent* event, bool suppressed);
uint16_t XwaKeyboardMapping_ReadKey(void);
uint16_t XwaKeyboardMapping_ReadButtons(void);
/* Whether a look action (XwaInputLook) or a roll action (XwaInputRoll) is
 * held, or was pressed since the previous frame. */
bool XwaKeyboardMapping_LookHeld(XwaInputLook look);
bool XwaKeyboardMapping_RollHeld(XwaInputRoll side);

#endif
