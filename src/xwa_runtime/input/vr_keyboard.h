#ifndef XWA_RUNTIME_INPUT_VR_KEYBOARD_H
#define XWA_RUNTIME_INPUT_VR_KEYBOARD_H

#include "aeron/input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The virtual keyboard of VR: its layout, and whether it is open. A player
 * with only the VR controllers types text with it: a pilot's name, a film's
 * name, a chat message. The renderer draws it and sends its keys
 * (xwa_remaster/vr_keyboard.h).
 *
 * The layout is a file: frontend/vr_keyboard.yaml of the mod, or else
 * assets/vr_keyboard.yaml of the game. */

enum {
	XWA_VR_KEYBOARD_MAX_KEYS = 128,
	XWA_VR_KEYBOARD_COLOR_COUNT = 6,
};

/* The colours of a layout, by name in the file. */
typedef enum XwaVrKeyboardColor {
	XWA_VR_KEYBOARD_COLOR_KEY = 0,
	XWA_VR_KEYBOARD_COLOR_ACTION,
	XWA_VR_KEYBOARD_COLOR_CONFIRM,
	XWA_VR_KEYBOARD_COLOR_DARK,
	XWA_VR_KEYBOARD_COLOR_TEXT,
	XWA_VR_KEYBOARD_COLOR_HOVER,
} XwaVrKeyboardColor;

typedef struct XwaVrKeyboardKey {
	AeronKey key;
	/* What is drawn on the key, and the characters it types without and with
	 * Shift. A key that types nothing has empty strings. */
	char label[16];
	char text[8];
	char shifted[8];
	/* Its rectangle on the keyboard, from 0 to 1 across and down. */
	float x, y, w, h;
	uint8_t color; /* XwaVrKeyboardColor */
} XwaVrKeyboardKey;

typedef struct XwaVrKeyboardLayout {
	float width_m;
	/* The keyboard's height for a width of 1. */
	float aspect;
	/* The size of the flight font for the labels: 16, 12 or 10. */
	int font_size;
	/* sRGB, from 0 to 1, with alpha. */
	float colors[XWA_VR_KEYBOARD_COLOR_COUNT][4];
	/* A picture drawn in place of the keys, as a path in the folder that the
	 * layout came from, or "". `from_mod` says which folder. */
	char image[128];
	int from_mod;
	/* Changes each time the layout is read again. */
	unsigned generation;
	int key_count;
	XwaVrKeyboardKey keys[XWA_VR_KEYBOARD_MAX_KEYS];
} XwaVrKeyboardLayout;

/* The layout, read at the first call. NULL when no layout could be read:
 * there is then no keyboard. */
const XwaVrKeyboardLayout* XwaVrKeyboard_Layout(void);

/* Whether the mod's layout is used when the mod has one (the Use the Mod's
 * Keyboard option). When off, the game's own layout is always used. */
void XwaVrKeyboard_SetUseMod(int use_mod);

/* A text field of a frontend dialog is being edited in this frame. The
 * dialogs call it each frame while they are shown. */
void XwaVrKeyboard_NoteTextField(void);
/* Whether a text field of the options window is being edited. */
void XwaVrKeyboard_SetUiTextEdit(int active);
/* The player opens or closes the keyboard (the toggle_vr_keyboard action). */
void XwaVrKeyboard_Toggle(void);
/* Return or Escape was typed: a keyboard that the player opened closes. One
 * that a text field opened stays while the field is there. */
void XwaVrKeyboard_Done(void);
/* Whether the keyboard is open in this frame: a text field is being edited,
 * the local player types a chat message, or the player opened it. */
int XwaVrKeyboard_IsOpen(void);

#ifdef __cplusplus
}
#endif

#endif
