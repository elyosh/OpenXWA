#include "xwa_runtime/input/vr_keyboard.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/flight/player/player.h"
#include "xwa_runtime/runtime/flight_task.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VR_KEYBOARD_MOD_PATH "frontend/vr_keyboard.yaml"
#define VR_KEYBOARD_GAME_PATH "assets/vr_keyboard.yaml"
/* The gap between two keys, as a part of a key of width 1. */
#define VR_KEYBOARD_GAP 0.08f
/* How long a text field of the frontend counts after it was last drawn. */
#define VR_KEYBOARD_FIELD_US 250000u

static struct {
	int loaded;
	int valid;
	int use_mod;
	unsigned generation;
	XwaVrKeyboardLayout layout;
	/* When XwaVrKeyboard_NoteTextField was last called. */
	uint64_t field_us;
	int ui_text_edit;
	/* The player opened it, or closed one that a text field had opened. */
	int opened_by_hand;
	int closed_by_hand;
} g_vrKeyboard = { .use_mod = 1 };

static const char* const kColorNames[XWA_VR_KEYBOARD_COLOR_COUNT] = { "key",  "action", "confirm",
																	  "dark", "text",   "hover" };

static void VrKeyboard_Defaults(XwaVrKeyboardLayout* layout) {
	static const float colors[XWA_VR_KEYBOARD_COLOR_COUNT][4] = {
		{ 74 / 255.0f, 90 / 255.0f, 190 / 255.0f, 1.0f },
		{ 176 / 255.0f, 52 / 255.0f, 74 / 255.0f, 1.0f },
		{ 56 / 255.0f, 140 / 255.0f, 96 / 255.0f, 1.0f },
		{ 36 / 255.0f, 72 / 255.0f, 100 / 255.0f, 1.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		{ 1.0f, 1.0f, 1.0f, 60 / 255.0f },
	};
	memset(layout, 0, sizeof *layout);
	layout->width_m = 0.40f;
	layout->font_size = 16;
	memcpy(layout->colors, colors, sizeof colors);
}

static int VrKeyboard_Error(const char* path, const AeronConfigNode* node, const char* message) {
	Aeron_LogError("xwa.input", "%s:%d: %s", path, node ? AeronConfigNode_Line(node) : 0, message);
	return 0;
}

/* Reads a number, or returns 0. */
static int VrKeyboard_Number(const AeronConfigNode* node, float* out) {
	const AeronConfigNodeType type = AeronConfigNode_Type(node);
	if (!node || (type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT)) {
		return 0;
	}
	*out = (float)AeronConfigNode_Float(node, 0.0);
	return isfinite(*out);
}

static int VrKeyboard_ReadColors(const char* path, const AeronConfigNode* colors,
								 XwaVrKeyboardLayout* layout) {
	if (!colors) {
		return 1;
	}
	if (AeronConfigNode_Type(colors) != AERON_CONFIG_MAP) {
		return VrKeyboard_Error(path, colors, "colors must be a map");
	}
	for (size_t i = 0; i < AeronConfigNode_MapCount(colors); i++) {
		const char* name = AeronConfigNode_MapKeyAt(colors, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(colors, i);
		const size_t count = AeronConfigNode_SequenceCount(value);
		int color = -1;
		for (int c = 0; name && c < XWA_VR_KEYBOARD_COLOR_COUNT; c++) {
			if (strcmp(name, kColorNames[c]) == 0) {
				color = c;
			}
		}
		if (color < 0 || AeronConfigNode_Type(value) != AERON_CONFIG_SEQUENCE || (count != 3 && count != 4)) {
			return VrKeyboard_Error(path, value,
									"a colour is one of key, action, confirm, dark, text and hover, with "
									"[red, green, blue] or [red, green, blue, alpha] from 0 to 255");
		}
		layout->colors[color][3] = 1.0f;
		for (size_t channel = 0; channel < count; channel++) {
			float number;
			if (!VrKeyboard_Number(AeronConfigNode_SequenceGet(value, channel), &number) || number < 0.0f ||
				number > 255.0f) {
				return VrKeyboard_Error(path, value, "a colour's values are numbers from 0 to 255");
			}
			layout->colors[color][channel] = number / 255.0f;
		}
	}
	return 1;
}

/* Reads what a key's entry says, without its place. */
static int VrKeyboard_ReadKey(const char* path, const AeronConfigNode* entry, XwaVrKeyboardKey* key) {
	const char* name = AeronConfigNode_String(AeronConfigNode_MapGet(entry, "key"), NULL);
	const char* text = AeronConfigNode_String(AeronConfigNode_MapGet(entry, "char"), NULL);
	const char* shifted = AeronConfigNode_String(AeronConfigNode_MapGet(entry, "shifted"), NULL);
	const char* label = AeronConfigNode_String(AeronConfigNode_MapGet(entry, "label"), NULL);
	const char* color = AeronConfigNode_String(AeronConfigNode_MapGet(entry, "color"), NULL);
	memset(key, 0, sizeof *key);
	if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP || !name || !AeronKey_FromName(name, &key->key)) {
		return VrKeyboard_Error(path, entry, "a key is a map with `key`, the name of a keyboard key");
	}
	if ((text && strlen(text) >= sizeof key->text) || (shifted && strlen(shifted) >= sizeof key->shifted) ||
		(label && strlen(label) >= sizeof key->label)) {
		return VrKeyboard_Error(path, entry, "a key's char, shifted or label is too long");
	}
	snprintf(key->text, sizeof key->text, "%s", text ? text : "");
	snprintf(key->shifted, sizeof key->shifted, "%s", shifted ? shifted : key->text);
	/* Without a label, a key shows what it types with Shift, or its name. */
	snprintf(key->label, sizeof key->label, "%.15s", label ? label : key->shifted[0] ? key->shifted : name);
	key->color = XWA_VR_KEYBOARD_COLOR_KEY;
	for (int c = 0; color && c <= XWA_VR_KEYBOARD_COLOR_DARK; c++) {
		if (strcmp(color, kColorNames[c]) == 0) {
			key->color = (uint8_t)c;
		}
	}
	return 1;
}

/* `rows`: lists of keys, each with a width `w` in keys (1 when left out). */
static int VrKeyboard_ReadRows(const char* path, const AeronConfigNode* rows, XwaVrKeyboardLayout* layout) {
	const size_t row_count = AeronConfigNode_SequenceCount(rows);
	float widest = 0.0f;
	if (AeronConfigNode_Type(rows) != AERON_CONFIG_SEQUENCE || row_count == 0) {
		return VrKeyboard_Error(path, rows, "rows must be a list of rows of keys");
	}
	for (size_t r = 0; r < row_count; r++) {
		const AeronConfigNode* row = AeronConfigNode_SequenceGet(rows, r);
		float x = 0.0f;
		if (AeronConfigNode_Type(row) != AERON_CONFIG_SEQUENCE) {
			return VrKeyboard_Error(path, row, "a row must be a list of keys");
		}
		for (size_t k = 0; k < AeronConfigNode_SequenceCount(row); k++) {
			const AeronConfigNode* entry = AeronConfigNode_SequenceGet(row, k);
			const AeronConfigNode* width = AeronConfigNode_MapGet(entry, "w");
			float w = 1.0f;
			if (layout->key_count >= XWA_VR_KEYBOARD_MAX_KEYS) {
				return VrKeyboard_Error(path, entry, "the keyboard has too many keys");
			}
			XwaVrKeyboardKey* key = &layout->keys[layout->key_count];
			if (!VrKeyboard_ReadKey(path, entry, key)) {
				return 0;
			}
			if (width && (!VrKeyboard_Number(width, &w) || w < 0.25f || w > 20.0f)) {
				return VrKeyboard_Error(path, width, "a key's w is a number from 0.25 to 20");
			}
			/* In key units for now. */
			key->x = x + VR_KEYBOARD_GAP * 0.5f;
			key->y = (float)r + VR_KEYBOARD_GAP * 0.5f;
			key->w = w - VR_KEYBOARD_GAP;
			key->h = 1.0f - VR_KEYBOARD_GAP;
			x += w;
			layout->key_count++;
		}
		if (x > widest) {
			widest = x;
		}
	}
	if (layout->key_count == 0) {
		return VrKeyboard_Error(path, rows, "the keyboard has no keys");
	}
	for (int k = 0; k < layout->key_count; k++) {
		XwaVrKeyboardKey* key = &layout->keys[k];
		key->x /= widest;
		key->w /= widest;
		key->y /= (float)row_count;
		key->h /= (float)row_count;
	}
	layout->aspect = (float)row_count / widest;
	return 1;
}

/* `image` with `image_size` and `keys`: a picture, and each key's rectangle
 * in its pixels. */
static int VrKeyboard_ReadImage(const char* path, const AeronConfigNode* root, XwaVrKeyboardLayout* layout) {
	const char* image = AeronConfigNode_String(AeronConfigNode_MapGet(root, "image"), NULL);
	const AeronConfigNode* size = AeronConfigNode_MapGet(root, "image_size");
	const AeronConfigNode* keys = AeronConfigNode_MapGet(root, "keys");
	float width, height;
	if (!image || !image[0] || strlen(image) >= sizeof layout->image || strstr(image, "..")) {
		return VrKeyboard_Error(path, root, "image must be the path of a PNG file");
	}
	if (AeronConfigNode_SequenceCount(size) != 2 ||
		!VrKeyboard_Number(AeronConfigNode_SequenceGet(size, 0), &width) ||
		!VrKeyboard_Number(AeronConfigNode_SequenceGet(size, 1), &height) || width < 1.0f || height < 1.0f) {
		return VrKeyboard_Error(path, size ? size : root, "image_size must be [width, height] in pixels");
	}
	if (AeronConfigNode_Type(keys) != AERON_CONFIG_SEQUENCE) {
		return VrKeyboard_Error(path, keys ? keys : root, "a keyboard with an image needs keys, a list");
	}
	for (size_t k = 0; k < AeronConfigNode_SequenceCount(keys); k++) {
		const AeronConfigNode* entry = AeronConfigNode_SequenceGet(keys, k);
		const AeronConfigNode* rect = AeronConfigNode_MapGet(entry, "rect");
		float corners[4];
		if (layout->key_count >= XWA_VR_KEYBOARD_MAX_KEYS) {
			return VrKeyboard_Error(path, entry, "the keyboard has too many keys");
		}
		XwaVrKeyboardKey* key = &layout->keys[layout->key_count];
		if (!VrKeyboard_ReadKey(path, entry, key)) {
			return 0;
		}
		for (size_t c = 0; c < 4; c++) {
			if (AeronConfigNode_SequenceCount(rect) != 4 ||
				!VrKeyboard_Number(AeronConfigNode_SequenceGet(rect, c), &corners[c])) {
				return VrKeyboard_Error(path, entry,
										"a key of an image needs rect: [x0, y0, x1, y1] in pixels");
			}
		}
		key->x = fminf(corners[0], corners[2]) / width;
		key->y = fminf(corners[1], corners[3]) / height;
		key->w = fabsf(corners[2] - corners[0]) / width;
		key->h = fabsf(corners[3] - corners[1]) / height;
		layout->key_count++;
	}
	snprintf(layout->image, sizeof layout->image, "%s", image);
	layout->aspect = height / width;
	return 1;
}

static int VrKeyboard_Load(AeronVfsRoot root, const char* path, XwaVrKeyboardLayout* layout) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	/* A missing file isn't an error: the next place is tried. */
	if (!AeronVfs_Exists(Aeron_GetVfs(), root, path)) {
		return 0;
	}
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), root, path, &file, &error)) {
		Aeron_LogError("xwa.input", "%s:%d: %s", path, error.line, error.message);
		return 0;
	}
	const AeronConfigNode* top = AeronConfigFile_Root(file);
	const AeronConfigNode* width = AeronConfigNode_MapGet(top, "width_m");
	const AeronConfigNode* font = AeronConfigNode_MapGet(top, "font_size");
	const AeronConfigNode* image = AeronConfigNode_MapGet(top, "image");
	float number;
	int ok = AeronConfigNode_Type(top) == AERON_CONFIG_MAP ||
			 VrKeyboard_Error(path, top, "the file must be a map");
	VrKeyboard_Defaults(layout);
	layout->from_mod = root == AERON_VFS_ROOT_MOD;
	if (ok && width) {
		ok = VrKeyboard_Number(width, &layout->width_m) && layout->width_m >= 0.1f && layout->width_m <= 1.5f
				 ? 1
				 : VrKeyboard_Error(path, width, "width_m is a number from 0.1 to 1.5");
	}
	if (ok && font) {
		ok = VrKeyboard_Number(font, &number) && (number == 16.0f || number == 12.0f || number == 10.0f)
				 ? 1
				 : VrKeyboard_Error(path, font, "font_size is 16, 12 or 10");
		layout->font_size = (int)number;
	}
	ok = ok && VrKeyboard_ReadColors(path, AeronConfigNode_MapGet(top, "colors"), layout);
	if (ok) {
		ok = image && AeronConfigNode_Type(image) == AERON_CONFIG_STRING
				 ? VrKeyboard_ReadImage(path, top, layout)
				 : VrKeyboard_ReadRows(path, AeronConfigNode_MapGet(top, "rows"), layout);
	}
	AeronConfigFile_Destroy(file);
	return ok;
}

const XwaVrKeyboardLayout* XwaVrKeyboard_Layout(void) {
	if (!g_vrKeyboard.loaded) {
		g_vrKeyboard.loaded = 1;
		g_vrKeyboard.valid =
			(g_vrKeyboard.use_mod &&
			 VrKeyboard_Load(AERON_VFS_ROOT_MOD, VR_KEYBOARD_MOD_PATH, &g_vrKeyboard.layout)) ||
			VrKeyboard_Load(AERON_VFS_ROOT_RESOURCE, VR_KEYBOARD_GAME_PATH, &g_vrKeyboard.layout);
		g_vrKeyboard.layout.generation = ++g_vrKeyboard.generation;
		if (!g_vrKeyboard.valid) {
			Aeron_LogError("xwa.input", "the VR keyboard has no layout, so it can't be shown");
		}
	}
	return g_vrKeyboard.valid ? &g_vrKeyboard.layout : NULL;
}

void XwaVrKeyboard_SetUseMod(int use_mod) {
	if (g_vrKeyboard.use_mod != (use_mod != 0)) {
		g_vrKeyboard.use_mod = use_mod != 0;
		/* The layout is read again when it is next needed. */
		g_vrKeyboard.loaded = 0;
	}
}

void XwaVrKeyboard_NoteTextField(void) { g_vrKeyboard.field_us = Aeron_NowUs(); }

void XwaVrKeyboard_SetUiTextEdit(int active) { g_vrKeyboard.ui_text_edit = active != 0; }

/* Whether something that takes text is active. */
static int VrKeyboard_FieldActive(void) {
	/* The frontend draws its dialogs at its own rate, which is lower than
	 * the headset's, so the field counts for a while after it was drawn. */
	const int field =
		g_vrKeyboard.field_us != 0 && Aeron_NowUs() - g_vrKeyboard.field_us < VR_KEYBOARD_FIELD_US;
	const int chat =
		XwaFlightTask_IsActive() && (unsigned)g_localPlayer < 8 && g_players[g_localPlayer].msgTypeId != 0;
	return field || g_vrKeyboard.ui_text_edit || chat;
}

void XwaVrKeyboard_Toggle(void) {
	if (XwaVrKeyboard_IsOpen()) {
		g_vrKeyboard.opened_by_hand = 0;
		g_vrKeyboard.closed_by_hand = VrKeyboard_FieldActive();
	} else {
		g_vrKeyboard.opened_by_hand = 1;
		g_vrKeyboard.closed_by_hand = 0;
	}
}

void XwaVrKeyboard_Done(void) { g_vrKeyboard.opened_by_hand = 0; }

int XwaVrKeyboard_IsOpen(void) {
	/* XWA_VR_KEYBOARD_OPEN=1 in the environment opens the keyboard at the
	 * start, to look at it without a text field. */
	static int checked;
	if (!checked) {
		const char* value = getenv("XWA_VR_KEYBOARD_OPEN");
		checked = 1;
		g_vrKeyboard.opened_by_hand = value && value[0] && value[0] != '0';
	}
	const int field = VrKeyboard_FieldActive();
	/* Closing it by hand lasts as long as the field that had opened it. */
	if (!field) {
		g_vrKeyboard.closed_by_hand = 0;
	}
	return g_vrKeyboard.opened_by_hand || (field && !g_vrKeyboard.closed_by_hand);
}
