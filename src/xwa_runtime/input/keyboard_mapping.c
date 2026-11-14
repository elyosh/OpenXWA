#include "xwa_runtime/input/keyboard_mapping.h"

#include "aeron/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct KeyboardPress {
	XwaInputAction action;
	bool down;
	bool ignored;
} KeyboardPress;

/* Hold counts are per physical press, so two keys bound to one action keep
 * it held until both are released. The pending bits keep a press that is
 * released before the next read visible to that read. */
static struct {
	uint16_t actions[AERON_KEY_COUNT][16];
	KeyboardPress pressed[AERON_KEY_COUNT];
	bool debug_available;
	bool enabled;
	uint16_t holds[2];
	uint16_t look_holds[XWA_INPUT_LOOK_COUNT];
	uint16_t roll_holds[XWA_INPUT_ROLL_COUNT];
	/* XWA key codes go above 0xFF (KEY_CTRL_L, KEY_CONSOLE_TOGGLE...). */
	uint16_t queue[256];
	unsigned read, write;
	uint16_t pending;
	uint16_t observed;
	uint8_t look_pending;
	uint8_t look_observed;
	uint8_t roll_pending;
	uint8_t roll_observed;
} g_keyboard;

void XwaKeyboardMapping_SetPolicy(bool debug_available) { g_keyboard.debug_available = debug_available; }

XwaKeyboardShortcut XwaKeyboardMapping_Shortcut(AeronKeyChord source) {
	const bool ctrl = (source.modifiers & AERON_KEY_MOD_CTRL) != 0;
	const bool alt = (source.modifiers & AERON_KEY_MOD_ALT) != 0;
	const bool gui = (source.modifiers & AERON_KEY_MOD_GUI) != 0;
	if (source.key == AERON_KEY_ESCAPE)
		return XWA_KEYBOARD_SHORTCUT_OPTIONS;
	if (source.key == AERON_KEY_GRAVE && g_keyboard.debug_available)
		return XWA_KEYBOARD_SHORTCUT_DEBUG;
	if (source.key == AERON_KEY_F1 + 4 && source.modifiers == AERON_KEY_MOD_CTRL)
		return XWA_KEYBOARD_SHORTCUT_RENDERER;
	if (source.key == AERON_KEY_F1 + 1 && source.modifiers == AERON_KEY_MOD_CTRL)
		return XWA_KEYBOARD_SHORTCUT_SPLIT_VIEW;
	if (source.key == AERON_KEY_A + ('m' - 'a') && ctrl && alt)
		return XWA_KEYBOARD_SHORTCUT_MOUSE;
	if (source.key == AERON_KEY_A + ('l' - 'a') && ctrl && alt)
		return XWA_KEYBOARD_SHORTCUT_MOUSE_LOOK;
	if (source.key == AERON_KEY_A + ('h' - 'a') && ctrl && !alt)
		return XWA_KEYBOARD_SHORTCUT_HOLOGRAMS;
	if (source.key == AERON_KEY_A + ('p' - 'a') && gui)
		return XWA_KEYBOARD_SHORTCUT_HOST_PAUSE;
#if defined(__APPLE__)
	if (source.key == AERON_KEY_A + ('f' - 'a') && gui && ctrl)
		return XWA_KEYBOARD_SHORTCUT_FULLSCREEN;
#else
	if ((source.key == AERON_KEY_RETURN || source.key == AERON_KEY_KP_ENTER) && alt)
		return XWA_KEYBOARD_SHORTCUT_FULLSCREEN;
#endif
	return XWA_KEYBOARD_SHORTCUT_NONE;
}

int XwaKeyboardMapping_Trigger(const AeronInputSnapshot* input, XwaKeyboardShortcut shortcut) {
	if (!input || !input->has_focus || input->key_events_overflow)
		return -1;
	for (uint16_t i = 0; i < input->key_event_count; ++i) {
		const AeronKeyEvent* event = &input->key_events[i];
		if (event->down && !event->repeat && XwaKeyboardMapping_Shortcut(event->chord) == shortcut)
			return event->chord.key;
	}
	return -1;
}

bool XwaKeyboardMapping_SourceValid(AeronKeyChord source) {
	return source.key > 0 && source.key < AERON_KEY_COUNT && source.modifiers < 16 &&
		   AeronKey_Name((AeronKey)source.key)[0] &&
		   (!AeronKey_Modifier((AeronKey)source.key) || source.modifiers == 0) &&
		   XwaKeyboardMapping_Shortcut(source) == XWA_KEYBOARD_SHORTCUT_NONE;
}

#if defined(__APPLE__)
#define GUI_MODIFIER_LABEL "Command+"
#else
#define GUI_MODIFIER_LABEL "Super+"
#endif

void XwaKeyboardMapping_FormatSource(char* text, size_t capacity, AeronKeyChord source) {
	snprintf(text, capacity, "%s%s%s%s%s", (source.modifiers & AERON_KEY_MOD_CTRL) ? "Ctrl+" : "",
			 (source.modifiers & AERON_KEY_MOD_ALT) ? "Alt+" : "",
			 (source.modifiers & AERON_KEY_MOD_SHIFT) ? "Shift+" : "",
			 (source.modifiers & AERON_KEY_MOD_GUI) ? GUI_MODIFIER_LABEL : "",
			 AeronKey_Name((AeronKey)source.key));
}

size_t XwaKeyboardMapping_Find(const XwaKeyboardBindings* profile, AeronKeyChord source) {
	for (size_t i = 0; i < profile->count; ++i)
		if (profile->bindings[i].source.key == source.key &&
			profile->bindings[i].source.modifiers == source.modifiers)
			return i;
	return SIZE_MAX;
}

static int BindingCompare(const void* left, const void* right) {
	const XwaKeyboardBinding* a = left;
	const XwaKeyboardBinding* b = right;
	if (a->action != b->action)
		return (int)a->action - (int)b->action;
	if (a->source.key != b->source.key)
		return (int)a->source.key - (int)b->source.key;
	return (int)a->source.modifiers - (int)b->source.modifiers;
}

void XwaKeyboardMapping_Sort(XwaKeyboardBindings* profile) {
	qsort(profile->bindings, profile->count, sizeof profile->bindings[0], BindingCompare);
}

bool XwaKeyboardMapping_Equal(const XwaKeyboardBindings* a, const XwaKeyboardBindings* b) {
	if (a->count != b->count)
		return false;
	for (size_t i = 0; i < a->count; ++i)
		if (BindingCompare(&a->bindings[i], &b->bindings[i]))
			return false;
	return true;
}

void XwaKeyboardMapping_Remove(XwaKeyboardBindings* profile, size_t index) {
	if (index >= profile->count)
		return;
	memmove(&profile->bindings[index], &profile->bindings[index + 1],
			(profile->count - index - 1) * sizeof profile->bindings[0]);
	--profile->count;
}

static uint16_t ButtonBit(XwaInputAction action) {
	return action == XWA_INPUT_ACTION_FIRE_WEAPON            ? 1
		   : action == XWA_INPUT_ACTION_TARGET_ROLL_MODIFIER ? 2
															 : 0;
}

static void Queue(uint16_t key) {
	unsigned next = (g_keyboard.write + 1) % 256;
	if (next == g_keyboard.read) {
		Aeron_LogWarn("xwa.input", "Keyboard command queue is full");
		return;
	}
	g_keyboard.queue[g_keyboard.write] = key;
	g_keyboard.write = next;
}

static void Hold(uint16_t* count, bool down) {
	if (down)
		++*count;
	else if (*count)
		--*count;
}

/* DirectInput has no key repeat, so every call is a physical press or
 * release. Held actions also send their key once on the press, as a joystick
 * button does in FlightInput_Read(). */
static void Dispatch(XwaInputAction action, bool down) {
	switch (XwaInputActions_Kind(action)) {
		case XWA_INPUT_ACTION_KIND_HELD:
			Hold(&g_keyboard.holds[ButtonBit(action) == 1 ? 0 : 1], down);
			break;
		case XWA_INPUT_ACTION_KIND_LOOK:
		case XWA_INPUT_ACTION_KIND_LOOK_CENTER:
			Hold(&g_keyboard.look_holds[XwaInputActions_Look(action)], down);
			break;
		case XWA_INPUT_ACTION_KIND_ROLL:
			Hold(&g_keyboard.roll_holds[XwaInputActions_Roll(action)], down);
			return;
		default:
			break;
	}
	if (down)
		Queue(XwaInputActions_Key(action));
}

void XwaKeyboardMapping_Suspend(void) {
	memset(g_keyboard.pressed, 0, sizeof g_keyboard.pressed);
	memset(g_keyboard.holds, 0, sizeof g_keyboard.holds);
	memset(g_keyboard.look_holds, 0, sizeof g_keyboard.look_holds);
	memset(g_keyboard.roll_holds, 0, sizeof g_keyboard.roll_holds);
	g_keyboard.read = g_keyboard.write = 0;
	g_keyboard.pending = g_keyboard.observed = 0;
	g_keyboard.look_pending = g_keyboard.look_observed = 0;
	g_keyboard.roll_pending = g_keyboard.roll_observed = 0;
	g_keyboard.enabled = false;
}

static void Compile(uint16_t table[AERON_KEY_COUNT][16], const XwaKeyboardBindings* profile) {
	memset(table, 0, sizeof g_keyboard.actions);
	for (size_t i = 0; i < profile->count; ++i) {
		const XwaKeyboardBinding* b = &profile->bindings[i];
		table[b->source.key][b->source.modifiers] = (uint16_t)b->action;
	}
}

void XwaKeyboardMapping_Install(const XwaKeyboardBindings* profile) {
	XwaKeyboardMapping_Suspend();
	Compile(g_keyboard.actions, profile);
}

void XwaKeyboardMapping_Enable(bool enabled, const AeronInputSnapshot* input) {
	if (enabled == g_keyboard.enabled)
		return;
	XwaKeyboardMapping_Suspend();
	g_keyboard.enabled = enabled;
	if (enabled && input)
		for (int key = 0; key < AERON_KEY_COUNT; ++key)
			g_keyboard.pressed[key].ignored = input->key_down[key] != 0;
}

void XwaKeyboardMapping_BeginFrame(const AeronInputSnapshot* input) {
	g_keyboard.pending &= (uint16_t)~g_keyboard.observed;
	g_keyboard.observed = 0;
	g_keyboard.look_pending &= (uint8_t)~g_keyboard.look_observed;
	g_keyboard.look_observed = 0;
	g_keyboard.roll_pending &= (uint8_t)~g_keyboard.roll_observed;
	g_keyboard.roll_observed = 0;
	if (input->key_events_overflow && g_keyboard.enabled) {
		XwaKeyboardMapping_Suspend();
		XwaKeyboardMapping_Enable(true, input);
	}
}

static void MarkPending(XwaInputAction action) {
	const int look = XwaInputActions_Look(action);
	const int roll = XwaInputActions_Roll(action);
	g_keyboard.pending |= ButtonBit(action);
	if (look >= 0)
		g_keyboard.look_pending |= (uint8_t)(1u << look);
	if (roll >= 0)
		g_keyboard.roll_pending |= (uint8_t)(1u << roll);
}

void XwaKeyboardMapping_Event(const AeronKeyEvent* event, bool suppressed) {
	if (!g_keyboard.enabled || event->chord.key >= AERON_KEY_COUNT || event->repeat)
		return;
	KeyboardPress* press = &g_keyboard.pressed[event->chord.key];
	if (!event->down) {
		if (press->down && press->action != XWA_INPUT_ACTION_NONE)
			Dispatch(press->action, false);
		*press = (KeyboardPress) { 0 };
		return;
	}
	if (press->ignored)
		return;
	if (suppressed) {
		if (press->down && press->action != XWA_INPUT_ACTION_NONE)
			Dispatch(press->action, false);
		*press = (KeyboardPress) { .ignored = true };
		return;
	}
	if (press->down)
		return;
	AeronKeyChord chord = event->chord;
	chord.modifiers &= (uint8_t)~AeronKey_Modifier((AeronKey)chord.key);
	press->down = true;
	switch (XwaKeyboardMapping_Shortcut(event->chord)) {
		case XWA_KEYBOARD_SHORTCUT_NONE:
			break;
		case XWA_KEYBOARD_SHORTCUT_OPTIONS:
			/* Escape always opens the options datapad in flight. */
			Queue(XwaInputActions_Key(XWA_INPUT_ACTION_OPTIONS_MENU));
			return;
		default:
			return;
	}
	press->action = (XwaInputAction)g_keyboard.actions[chord.key][chord.modifiers];
	if (press->action == XWA_INPUT_ACTION_NONE)
		return;
	MarkPending(press->action);
	Dispatch(press->action, true);
}

uint16_t XwaKeyboardMapping_ReadKey(void) {
	if (g_keyboard.read == g_keyboard.write)
		return 0;
	uint16_t key = g_keyboard.queue[g_keyboard.read];
	g_keyboard.read = (g_keyboard.read + 1) % 256;
	return key;
}

uint16_t XwaKeyboardMapping_ReadButtons(void) {
	g_keyboard.observed |= g_keyboard.pending;
	return (g_keyboard.holds[0] ? 1 : 0) | (g_keyboard.holds[1] ? 2 : 0) | g_keyboard.pending;
}

bool XwaKeyboardMapping_LookHeld(XwaInputLook look) {
	if ((unsigned)look >= XWA_INPUT_LOOK_COUNT)
		return false;
	const uint8_t bit = (uint8_t)(1u << look);
	g_keyboard.look_observed |= g_keyboard.look_pending & bit;
	return g_keyboard.look_holds[look] || (g_keyboard.look_pending & bit);
}

bool XwaKeyboardMapping_RollHeld(XwaInputRoll side) {
	if ((unsigned)side >= XWA_INPUT_ROLL_COUNT)
		return false;
	const uint8_t bit = (uint8_t)(1u << side);
	g_keyboard.roll_observed |= g_keyboard.roll_pending & bit;
	return g_keyboard.roll_holds[side] || (g_keyboard.roll_pending & bit);
}
