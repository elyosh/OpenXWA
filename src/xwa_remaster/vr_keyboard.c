#include "xwa_remaster/vr_keyboard.h"

#include "aeron/aeron.h"
#include "aeron/image.h"
#include "aeron/scene/draw_list2d.h"
#include "aeron/xr.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/text.h"
#include "xwa_runtime/input/vr_keyboard.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The width of the keyboard's image in pixels. */
#define KEYBOARD_IMAGE_WIDTH 1056
/* Where the keyboard is, from the rig's origin: in front of the player and
 * below the virtual screen, tilted so that it faces the head. */
#define KEYBOARD_DOWN_M 0.35f
#define KEYBOARD_FORWARD_M 0.50f
#define KEYBOARD_TILT_DEG 35.0f
/* A held key repeats after the first time, then at the second rate. */
#define KEYBOARD_REPEAT_FIRST_US 400000u
#define KEYBOARD_REPEAT_NEXT_US 83000u
#define KEYBOARD_TICK_ENTER_MS 10u
#define KEYBOARD_TICK_PRESS_MS 20u
#define KEYBOARD_TICK_STRENGTH 0x3000u

/* A sticky modifier: off, on for the next key, or locked on. */
enum { STICKY_OFF = 0, STICKY_ONCE, STICKY_LOCKED };
enum { MODIFIER_SHIFT = 0, MODIFIER_CTRL, MODIFIER_ALT, MODIFIER_COUNT };

/* A press is sent over several frames. The game's code reads the state of
 * the keys once a frame, so a modifier goes down one frame before its key,
 * and a key stays down for a frame at least. */
typedef enum KeyStroke {
	STROKE_NONE = 0,
	STROKE_MODIFIERS_DOWN,
	STROKE_KEY_DOWN,
	STROKE_HELD,
	STROKE_KEY_UP,
	STROKE_MODIFIERS_UP,
} KeyStroke;

static struct {
	int haptics;
	AeronDrawList2D* list;
	/* The layout's picture, when it has one. */
	AeronTexture* image;
	int image_tried;
	unsigned layout_generation;
	struct {
		uint64_t frame; /* the input frame of the last test */
		int on_panel;
		float x, y; /* in the image's pixels */
		int key;    /* the key under the ray, or -1 */
		int trigger_down;
	} hands[2];
	uint8_t sticky[MODIFIER_COUNT];
	KeyStroke stroke;
	int stroke_key;
	int stroke_hand;
	uint8_t stroke_modifiers; /* AERON_KEY_MOD_* */
	int stroke_repeated;
	uint64_t stroke_repeat_us;
} g_keyboard = { .haptics = 1, .hands = { { .key = -1 }, { .key = -1 } } };

static const AeronKey kModifierKeys[MODIFIER_COUNT] = { AERON_KEY_LSHIFT, AERON_KEY_LCTRL, AERON_KEY_LALT };
static const uint8_t kModifierBits[MODIFIER_COUNT] = { AERON_KEY_MOD_SHIFT, AERON_KEY_MOD_CTRL,
													   AERON_KEY_MOD_ALT };

void XwaRemasterVrKeyboard_SetHaptics(int enabled) { g_keyboard.haptics = enabled != 0; }

static int keyboard_hand_fresh(int hand) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	return hand >= 0 && hand < 2 && input && g_keyboard.hands[hand].frame != 0 &&
		   input->frame_id - g_keyboard.hands[hand].frame <= 1;
}

int XwaRemasterVrKeyboard_HandOn(int hand) {
	return keyboard_hand_fresh(hand) && g_keyboard.hands[hand].on_panel;
}

int XwaRemasterVrKeyboard_HandPoint(int hand, float point_m[3]) {
	return XwaRemasterVrKeyboard_HandOn(hand) &&
		   Aeron_XrPanelPoint(g_keyboard.hands[hand].x, g_keyboard.hands[hand].y, point_m);
}

/* The sticky modifier that a key is, or -1. */
static int keyboard_modifier(AeronKey key) {
	switch (key) {
		case AERON_KEY_LSHIFT:
		case AERON_KEY_RSHIFT:
			return MODIFIER_SHIFT;
		case AERON_KEY_LCTRL:
		case AERON_KEY_RCTRL:
			return MODIFIER_CTRL;
		case AERON_KEY_LALT:
		case AERON_KEY_RALT:
			return MODIFIER_ALT;
		default:
			return -1;
	}
}

static int keyboard_repeats(AeronKey key) {
	return key == AERON_KEY_BACKSPACE || key == AERON_KEY_DELETE || key == AERON_KEY_LEFT ||
		   key == AERON_KEY_RIGHT || key == AERON_KEY_UP || key == AERON_KEY_DOWN;
}

static void keyboard_tick(int hand, uint32_t duration_ms) {
	if (g_keyboard.haptics) {
		Aeron_RumbleController(hand ? AERON_VR_HAND_INSTANCE_RIGHT : AERON_VR_HAND_INSTANCE_LEFT,
							   KEYBOARD_TICK_STRENGTH, KEYBOARD_TICK_STRENGTH, duration_ms);
	}
}

static int keyboard_trigger_down(const AeronInputSnapshot* input, int hand) {
	const uint32_t id = hand ? AERON_VR_HAND_INSTANCE_RIGHT : AERON_VR_HAND_INSTANCE_LEFT;
	for (int i = 0; i < AERON_CONTROLLER_MAX; i++) {
		const AeronControllerSnapshot* device = &input->controllers[i];
		if (device->connected && device->kind == AERON_CONTROLLER_KIND_VR_HAND && device->instance_id == id) {
			return Aeron_ControllerButtonBit(device->raw_buttons, AERON_VR_HAND_BUTTON_TRIGGER);
		}
	}
	return 0;
}

static void keyboard_send_modifiers(int down) {
	for (int m = 0; m < MODIFIER_COUNT; m++) {
		if (g_keyboard.stroke_modifiers & kModifierBits[m]) {
			Aeron_InjectKey(kModifierKeys[m], down, 0, down ? g_keyboard.stroke_modifiers : 0, NULL);
		}
	}
}

static void keyboard_send_key(const XwaVrKeyboardLayout* layout, int down) {
	const XwaVrKeyboardKey* key = &layout->keys[g_keyboard.stroke_key];
	/* With Ctrl or Alt, a key types nothing. */
	const char* text = (g_keyboard.stroke_modifiers & (AERON_KEY_MOD_CTRL | AERON_KEY_MOD_ALT)) ? NULL
					   : (g_keyboard.stroke_modifiers & AERON_KEY_MOD_SHIFT)                    ? key->shifted
																								: key->text;
	Aeron_InjectKey(key->key, down, 0, g_keyboard.stroke_modifiers, down ? text : NULL);
}

/* Ends the stroke at once, as when the keyboard closes with a key down. */
static void keyboard_abort_stroke(const XwaVrKeyboardLayout* layout) {
	if (layout && (g_keyboard.stroke == STROKE_HELD || g_keyboard.stroke == STROKE_KEY_UP)) {
		keyboard_send_key(layout, 0);
	}
	if (g_keyboard.stroke != STROKE_NONE && g_keyboard.stroke != STROKE_MODIFIERS_DOWN) {
		keyboard_send_modifiers(0);
	}
	g_keyboard.stroke = STROKE_NONE;
}

/* Advances the stroke by one frame. */
static void keyboard_step_stroke(const XwaVrKeyboardLayout* layout) {
	const uint64_t now = Aeron_NowUs();
	const XwaVrKeyboardKey* key =
		g_keyboard.stroke != STROKE_NONE ? &layout->keys[g_keyboard.stroke_key] : NULL;
	switch (g_keyboard.stroke) {
		case STROKE_MODIFIERS_DOWN:
			keyboard_send_modifiers(1);
			g_keyboard.stroke = STROKE_KEY_DOWN;
			break;
		case STROKE_KEY_DOWN:
			keyboard_send_key(layout, 1);
			g_keyboard.stroke = STROKE_HELD;
			g_keyboard.stroke_repeat_us =
				now + (g_keyboard.stroke_repeated ? KEYBOARD_REPEAT_NEXT_US : KEYBOARD_REPEAT_FIRST_US);
			break;
		case STROKE_HELD:
			if (!g_keyboard.hands[g_keyboard.stroke_hand].trigger_down) {
				keyboard_send_key(layout, 0);
				g_keyboard.stroke = STROKE_MODIFIERS_UP;
			} else if (keyboard_repeats(key->key) && now >= g_keyboard.stroke_repeat_us) {
				/* A repeat is a release, then a press in the next frame. */
				keyboard_send_key(layout, 0);
				g_keyboard.stroke_repeated = 1;
				g_keyboard.stroke = STROKE_KEY_DOWN;
			}
			break;
		case STROKE_MODIFIERS_UP:
			keyboard_send_modifiers(0);
			for (int m = 0; m < MODIFIER_COUNT; m++) {
				if (g_keyboard.sticky[m] == STICKY_ONCE) {
					g_keyboard.sticky[m] = STICKY_OFF;
				}
			}
			if (key->key == AERON_KEY_RETURN || key->key == AERON_KEY_ESCAPE) {
				XwaVrKeyboard_Done();
			}
			g_keyboard.stroke = STROKE_NONE;
			break;
		default:
			break;
	}
}

static void keyboard_press(const XwaVrKeyboardLayout* layout, int hand, int index) {
	const int modifier = keyboard_modifier(layout->keys[index].key);
	keyboard_tick(hand, KEYBOARD_TICK_PRESS_MS);
	if (modifier >= 0) {
		/* Once for the next key, twice to lock, a third time to release. */
		g_keyboard.sticky[modifier] = (uint8_t)((g_keyboard.sticky[modifier] + 1) % 3);
		return;
	}
	/* One key at a time: a press during another key's stroke is dropped. */
	if (g_keyboard.stroke != STROKE_NONE) {
		return;
	}
	g_keyboard.stroke_key = index;
	g_keyboard.stroke_hand = hand;
	g_keyboard.stroke_repeated = 0;
	g_keyboard.stroke_modifiers = 0;
	for (int m = 0; m < MODIFIER_COUNT; m++) {
		if (g_keyboard.sticky[m] != STICKY_OFF) {
			g_keyboard.stroke_modifiers |= kModifierBits[m];
		}
	}
	g_keyboard.stroke = g_keyboard.stroke_modifiers ? STROKE_MODIFIERS_DOWN : STROKE_KEY_DOWN;
}

static void keyboard_rgba(const float srgb[4], float scale, float out[4]) {
	for (int c = 0; c < 3; c++) {
		float value = srgb[c] * scale;
		value = value > 1.0f ? 1.0f : value;
		out[c] = XwaRemaster_SrgbToLinear(value) * srgb[3];
	}
	out[3] = srgb[3];
}

/* Loads the layout's picture once. */
static void keyboard_load_image(const XwaVrKeyboardLayout* layout) {
	if (g_keyboard.layout_generation != layout->generation) {
		/* Another layout was read: its picture, if any, is another one. */
		g_keyboard.layout_generation = layout->generation;
		if (g_keyboard.image) {
			Aeron_DestroyTexture(g_keyboard.image);
			g_keyboard.image = NULL;
		}
		g_keyboard.image_tried = 0;
	}
	if (g_keyboard.image_tried || !layout->image[0]) {
		return;
	}
	g_keyboard.image_tried = 1;
	char path[192];
	int width = 0, height = 0;
	snprintf(path, sizeof path, "%s/%s", layout->from_mod ? "frontend" : "assets", layout->image);
	uint8_t* pixels = Aeron_ImageReadPngRgba8(Aeron_GetVfs(),
											  layout->from_mod ? AERON_VFS_ROOT_MOD : AERON_VFS_ROOT_RESOURCE,
											  path, &width, &height);
	if (!pixels) {
		Aeron_LogError("xwa.remaster", "VR keyboard: the picture %s can't be read", path);
		return;
	}
	/* The draw list blends colours that are premultiplied by alpha. */
	for (int i = 0; i < width * height; i++) {
		uint8_t* pixel = &pixels[i * 4];
		for (int c = 0; c < 3; c++) {
			pixel[c] = (uint8_t)((pixel[c] * pixel[3] + 127) / 255);
		}
	}
	g_keyboard.image = Aeron_CreateTexture(&(AeronTextureDesc) {
		.width = width,
		.height = height,
		.format = AERON_TEXTURE_FORMAT_RGBA8_SRGB,
		.usage = AERON_TEXTURE_USAGE_SAMPLED | AERON_TEXTURE_USAGE_TRANSFER_DST,
		.debug_name = "xwa.vr_keyboard",
	});
	if (g_keyboard.image &&
		!Aeron_UploadTextureData(&(AeronTextureUploadDesc) { .texture = g_keyboard.image,
															 .width = width,
															 .height = height,
															 .raw_data = pixels,
															 .raw_size = (uint32_t)(width * height * 4) })) {
		Aeron_DestroyTexture(g_keyboard.image);
		g_keyboard.image = NULL;
	}
	free(pixels);
}

static void keyboard_draw(const XwaVrKeyboardLayout* layout, XwaRemasterAssets* assets,
						  AeronRenderTarget* target, int width, int height) {
	AeronCommandBuffer* cmd = Aeron_AcquireCommandBuffer();
	if (!cmd) {
		return;
	}
	if (!g_keyboard.list) {
		g_keyboard.list = AeronDrawList_Create(512);
	}
	if (!g_keyboard.list) {
		Aeron_CancelCommandBuffer(cmd);
		return;
	}
	keyboard_load_image(layout);
	const int tier = layout->font_size == 16 ? 0 : layout->font_size == 12 ? 1 : 2;
	XwaRemasterAssets_PrepareFlightFonts(assets, cmd);
	const AeronFontAtlas* font = XwaRemasterAssets_FlightFont(assets, tier, NULL);
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	AeronDrawList_Begin(g_keyboard.list, target, width, height, AERON_DRAWLIST2D_CLEAR, clear);
	if (g_keyboard.image) {
		AeronDrawList_AddSprite(g_keyboard.list, &(AeronDrawList2DSprite) {
													 .texture = g_keyboard.image,
													 .src_u1 = 1.0f,
													 .src_v1 = 1.0f,
													 .dst_w = (float)width,
													 .dst_h = (float)height,
													 .tint = { 1.0f, 1.0f, 1.0f, 1.0f },
													 .blend = AERON_BLIT2D_BLEND_PMA,
													 .filter = AERON_BLIT2D_FILTER_LINEAR,
												 });
	} else {
		AeronDrawList_AddRRect(g_keyboard.list, &(AeronDrawList2DRRect) {
													.dst_w = (float)width,
													.dst_h = (float)height,
													.radius_px = 18.0f,
													.soft_px = 1.0f,
													.fill_top = { 0.012f, 0.016f, 0.028f, 0.88f },
													.fill_bottom = { 0.006f, 0.008f, 0.014f, 0.88f },
												});
	}
	uint8_t stroke_modifiers = 0;
	for (int m = 0; m < MODIFIER_COUNT; m++) {
		stroke_modifiers |= g_keyboard.sticky[m] != STICKY_OFF ? kModifierBits[m] : 0;
	}
	for (int k = 0; k < layout->key_count; k++) {
		const XwaVrKeyboardKey* key = &layout->keys[k];
		const float x = key->x * (float)width, y = key->y * (float)height;
		const float w = key->w * (float)width, h = key->h * (float)height;
		const int modifier = keyboard_modifier(key->key);
		const int hovered = XwaRemasterVrKeyboard_HandOn(0) && g_keyboard.hands[0].key == k
								? 1
								: XwaRemasterVrKeyboard_HandOn(1) && g_keyboard.hands[1].key == k;
		const int down = (g_keyboard.stroke != STROKE_NONE && g_keyboard.stroke_key == k) ||
						 (modifier >= 0 && g_keyboard.sticky[modifier] != STICKY_OFF);
		if (!g_keyboard.image) {
			AeronDrawList2DRRect rect = {
				.dst_x = x,
				.dst_y = y,
				.dst_w = w,
				.dst_h = h,
				.radius_px = h * 0.16f,
				.border_px = modifier >= 0 && g_keyboard.sticky[modifier] == STICKY_LOCKED ? 4.0f : 2.0f,
				.soft_px = 1.0f,
			};
			keyboard_rgba(layout->colors[key->color], down ? 1.5f : 1.12f, rect.fill_top);
			keyboard_rgba(layout->colors[key->color], down ? 1.25f : 0.82f, rect.fill_bottom);
			keyboard_rgba(layout->colors[down ? XWA_VR_KEYBOARD_COLOR_TEXT : key->color], down ? 1.0f : 0.5f,
						  rect.border);
			AeronDrawList_AddRRect(g_keyboard.list, &rect);
			/* A key shows what the next press types. */
			const char* label = key->label;
			if (key->text[0] && key->shifted[0] && strcmp(key->text, key->shifted) != 0 &&
				strcmp(key->label, key->shifted) == 0) {
				label = (stroke_modifiers & AERON_KEY_MOD_SHIFT) ? key->shifted : key->text;
			}
			if (font && label[0] && label[0] != ' ') {
				/* Long names get smaller letters, to stay inside the key. */
				float size = h * 0.44f;
				const float text_width = XwaRemasterText_MeasureFlightString(font, NULL, label, size);
				if (text_width > w * 0.86f && text_width > 0.0f) {
					size *= w * 0.86f / text_width;
				}
				const float text_color[4] = { layout->colors[XWA_VR_KEYBOARD_COLOR_TEXT][0],
											  layout->colors[XWA_VR_KEYBOARD_COLOR_TEXT][1],
											  layout->colors[XWA_VR_KEYBOARD_COLOR_TEXT][2], 1.0f };
				const uint32_t argb = 0xff000000u | ((uint32_t)lroundf(text_color[0] * 255.0f) << 16) |
									  ((uint32_t)lroundf(text_color[1] * 255.0f) << 8) |
									  (uint32_t)lroundf(text_color[2] * 255.0f);
				XwaRemasterText_AddFlightString(g_keyboard.list, font, NULL, label, x + w * 0.5f,
												y + (h - size) * 0.5f, size, XWA_REMASTER_TEXT_ALIGN_CENTER,
												argb, NULL);
			}
		}
		if (hovered || (g_keyboard.image && down)) {
			AeronDrawList2DRRect highlight = {
				.dst_x = x,
				.dst_y = y,
				.dst_w = w,
				.dst_h = h,
				.radius_px = h * 0.16f,
				.soft_px = 1.0f,
			};
			keyboard_rgba(layout->colors[XWA_VR_KEYBOARD_COLOR_HOVER], 1.0f, highlight.fill_top);
			memcpy(highlight.fill_bottom, highlight.fill_top, sizeof highlight.fill_bottom);
			AeronDrawList_AddRRect(g_keyboard.list, &highlight);
		}
	}
	AeronDrawList_Render(g_keyboard.list, cmd);
	if (!Aeron_SubmitCommandBuffer(cmd)) {
		Aeron_LogError("xwa.remaster", "VR keyboard: the keyboard couldn't be drawn");
	}
}

void XwaRemasterVrKeyboard_Frame(XwaRemasterAssets* assets) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	const XwaVrKeyboardLayout* layout =
		input && assets && Aeron_XrSessionRunning() && XwaVrKeyboard_IsOpen() ? XwaVrKeyboard_Layout() : NULL;
	AeronRenderTarget* target = NULL;
	int width = KEYBOARD_IMAGE_WIDTH, height = 0;
	if (layout) {
		const float tilt = -KEYBOARD_TILT_DEG * 3.14159265f / 360.0f;
		height = (int)lroundf((float)width * layout->aspect);
		target = Aeron_XrPanelBegin(&(AeronXrPanelDesc) {
			.width_px = width,
			.height_px = height,
			.width_m = layout->width_m,
			.pose = { .position_m = { 0.0f, -KEYBOARD_DOWN_M, -KEYBOARD_FORWARD_M },
					  .orientation = { sinf(tilt), 0.0f, 0.0f, cosf(tilt) } },
		});
	}
	if (!target) {
		/* Closed, or not shown in this frame: no key stays down. */
		keyboard_abort_stroke(XwaVrKeyboard_Layout());
		g_keyboard.hands[0].frame = g_keyboard.hands[1].frame = 0;
		return;
	}
	for (int hand = 0; hand < 2; hand++) {
		const int was_fresh = keyboard_hand_fresh(hand);
		const int previous_key = was_fresh ? g_keyboard.hands[hand].key : -1;
		const int was_down = was_fresh && g_keyboard.hands[hand].trigger_down;
		float x = 0.0f, y = 0.0f;
		g_keyboard.hands[hand].frame = input->frame_id;
		g_keyboard.hands[hand].trigger_down = keyboard_trigger_down(input, hand);
		g_keyboard.hands[hand].on_panel = Aeron_XrPanelHit(hand, &x, &y);
		g_keyboard.hands[hand].x = x;
		g_keyboard.hands[hand].y = y;
		g_keyboard.hands[hand].key = -1;
		if (!g_keyboard.hands[hand].on_panel) {
			continue;
		}
		const float u = x / (float)width, v = y / (float)height;
		for (int k = 0; k < layout->key_count; k++) {
			const XwaVrKeyboardKey* key = &layout->keys[k];
			if (u >= key->x && u <= key->x + key->w && v >= key->y && v <= key->y + key->h) {
				g_keyboard.hands[hand].key = k;
				break;
			}
		}
		const int key = g_keyboard.hands[hand].key;
		if (key >= 0 && key != previous_key) {
			keyboard_tick(hand, KEYBOARD_TICK_ENTER_MS);
		}
		if (key >= 0 && g_keyboard.hands[hand].trigger_down && !was_down) {
			keyboard_press(layout, hand, key);
		}
	}
	keyboard_step_stroke(layout);
	keyboard_draw(layout, assets, target, width, height);
}

void XwaRemasterVrKeyboard_Shutdown(void) {
	AeronDrawList_Destroy(g_keyboard.list);
	g_keyboard.list = NULL;
	if (g_keyboard.image) {
		Aeron_DestroyTexture(g_keyboard.image);
		g_keyboard.image = NULL;
	}
	g_keyboard.image_tried = 0;
}
