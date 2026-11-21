#include "xwa_remaster/cockpit_buttons.h"

#include "aeron/aeron.h"
#include "aeron/scene/draw_list2d.h"
#include "aeron/scene/mesh.h"
#include "aeron/xr.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/text.h"
#include "xwa_remaster/vr_hands.h"
#include "xwa_runtime/input/actions.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/input/vr_stick.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The haptic ticks: when the ray enters a button, and when it is pressed. */
#define BUTTON_TICK_ENTER_MS 10u
#define BUTTON_TICK_PRESS_MS 20u
#define BUTTON_TICK_STRENGTH 0x3000u
/* The models whose unknown action names were already reported. */
#define BUTTON_CHECKED_MODELS 8

typedef struct ButtonHand {
	/* The input frame that Update last tested the hand in. */
	uint64_t frame;
	/* The ray meets a material that has buttons, at this point of the seat
	 * camera's eye space. The cockpit doesn't move in that space, so the
	 * point is still right when the next frame's HUD is prepared. */
	int on_panel;
	float seat_point[3];
	/* The button under the ray: its model and index, or -1. */
	const AeronSceneMesh* mesh;
	int button;
	XwaInputAction action;
	char label[AERON_GLTF_BUTTON_NAME_MAX];
	int trigger_down;
	/* The action that the trigger holds, and the mapping's press number. */
	XwaInputAction held;
	uint32_t held_press;
} ButtonHand;

static struct {
	int enabled;
	float ray_m;
	int haptics;
	ButtonHand hands[2];
	const AeronSceneMesh* checked[BUTTON_CHECKED_MODELS];
	uint32_t checked_next;
	AeronDrawList2D* list;
	int list_has_markers;
} g_buttons = { .enabled = 1, .ray_m = 1.5f, .haptics = 1 };

void XwaRemasterCockpitButtons_SetOptions(int enabled, float ray_m, int haptics) {
	g_buttons.enabled = enabled != 0;
	g_buttons.ray_m = ray_m > 0.0f ? ray_m : 1.5f;
	g_buttons.haptics = haptics != 0;
}

/* Whether Update tested the hand in this input frame or the one before. The
 * bindings' mask is set before the frame's scene is built, so it reads the
 * last frame's result. */
static int button_hand_fresh(const ButtonHand* hand) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	return input && hand->frame != 0 && input->frame_id - hand->frame <= 1;
}

int XwaRemasterCockpitButtons_OnPanel(int hand) {
	return hand >= 0 && hand < 2 && button_hand_fresh(&g_buttons.hands[hand]) &&
		   g_buttons.hands[hand].on_panel;
}

int XwaRemasterCockpitButtons_OnButton(int hand) {
	return XwaRemasterCockpitButtons_OnPanel(hand) && g_buttons.hands[hand].button >= 0;
}

/* Reports, once for each model, the buttons whose action the game doesn't
 * know. Such a button does nothing. */
static void button_check_model(const AeronSceneMesh* mesh) {
	for (int i = 0; i < BUTTON_CHECKED_MODELS; i++) {
		if (g_buttons.checked[i] == mesh) {
			return;
		}
	}
	g_buttons.checked[g_buttons.checked_next++ % BUTTON_CHECKED_MODELS] = mesh;
	for (uint32_t b = 0; b < mesh->button_count; b++) {
		if (XwaInputActions_FromName(mesh->buttons[b].action) == XWA_INPUT_ACTION_NONE) {
			Aeron_LogWarn("xwa.remaster", "cockpit buttons: the action '%s' of a button isn't known",
						  mesh->buttons[b].action);
		}
	}
}

/* Inverts an instance's transform: rotation, scale and translation. */
static int button_invert(const float m[16], float out[12]) {
	const float a[3] = { m[0], m[1], m[2] }, b[3] = { m[4], m[5], m[6] }, c[3] = { m[8], m[9], m[10] };
	const float bc[3] = { b[1] * c[2] - b[2] * c[1], b[2] * c[0] - b[0] * c[2], b[0] * c[1] - b[1] * c[0] };
	const float ca[3] = { c[1] * a[2] - c[2] * a[1], c[2] * a[0] - c[0] * a[2], c[0] * a[1] - c[1] * a[0] };
	const float ab[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
	const float determinant = a[0] * bc[0] + a[1] * bc[1] + a[2] * bc[2];
	if (!(fabsf(determinant) > 1e-20f)) {
		return 0;
	}
	for (int row = 0; row < 3; row++) {
		out[row * 4 + 0] = bc[row] / determinant;
		out[row * 4 + 1] = ca[row] / determinant;
		out[row * 4 + 2] = ab[row] / determinant;
		out[row * 4 + 3] = -(out[row * 4 + 0] * m[3] + out[row * 4 + 1] * m[7] + out[row * 4 + 2] * m[11]);
	}
	return 1;
}

/* Tests a ray of the scene against an instance. `distance` is the nearest
 * hit so far, in scene units, and is lowered when the instance is nearer. */
static int button_pick_instance(const AeronSceneMeshInstance* instance, const float origin[3],
								const float direction[3], float* distance, AeronSceneMeshPick* pick) {
	float inverse[12], local_origin[3], local_direction[3];
	if (!instance || !instance->mesh || !instance->mesh->button_count ||
		!button_invert(instance->transform, inverse)) {
		return 0;
	}
	button_check_model(instance->mesh);
	for (int row = 0; row < 3; row++) {
		const float* r = &inverse[row * 4];
		local_origin[row] = r[0] * origin[0] + r[1] * origin[1] + r[2] * origin[2] + r[3];
		/* Not normalised: a distance along the ray stays in scene units. */
		local_direction[row] = r[0] * direction[0] + r[1] * direction[1] + r[2] * direction[2];
	}
	if (!AeronSceneMesh_Pick(instance->mesh, instance->mesh_table, instance->variant,
							 instance->hidden_tag_groups, local_origin, local_direction, *distance, pick)) {
		return 0;
	}
	*distance = pick->distance;
	return 1;
}

static void button_tick(int hand, uint32_t duration_ms) {
	if (g_buttons.haptics) {
		Aeron_RumbleController(hand ? AERON_VR_HAND_INSTANCE_RIGHT : AERON_VR_HAND_INSTANCE_LEFT,
							   BUTTON_TICK_STRENGTH, BUTTON_TICK_STRENGTH, duration_ms);
	}
}

static void button_release(ButtonHand* state) {
	if (state->held != XWA_INPUT_ACTION_NONE) {
		XwaControllerMapping_ReleaseAction(state->held, state->held_press);
		state->held = XWA_INPUT_ACTION_NONE;
	}
}

/* AERON_XR_INPUT_LOG=1 in the environment also logs the button that a hand
 * points at and each press. */
static int button_log(void) {
	static int log = -1;
	if (log < 0) {
		const char* value = getenv("AERON_XR_INPUT_LOG");
		log = value && value[0] && value[0] != '0';
	}
	return log;
}

/* Whether the hand's trigger is pulled, from its entry in the snapshot. */
static int button_trigger_down(const AeronInputSnapshot* input, int hand) {
	const uint32_t id = hand ? AERON_VR_HAND_INSTANCE_RIGHT : AERON_VR_HAND_INSTANCE_LEFT;
	for (int i = 0; i < AERON_CONTROLLER_MAX; i++) {
		const AeronControllerSnapshot* device = &input->controllers[i];
		if (device->connected && device->kind == AERON_CONTROLLER_KIND_VR_HAND && device->instance_id == id) {
			return Aeron_ControllerButtonBit(device->raw_buttons, AERON_VR_HAND_BUTTON_TRIGGER);
		}
	}
	return 0;
}

void XwaRemasterCockpitButtons_Update(const AeronSceneMeshInstance* cockpit, const float seat_rows[9],
									  const float seat_pos[3]) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	if (!input) {
		return;
	}
	for (int hand = 0; hand < 2; hand++) {
		ButtonHand* state = &g_buttons.hands[hand];
		const int was_fresh = button_hand_fresh(state);
		const AeronSceneMesh* previous_mesh = was_fresh ? state->mesh : NULL;
		const int previous_button = was_fresh ? state->button : -1;
		const int was_down = was_fresh && state->trigger_down;
		float origin[3], direction[3], units_per_m = 1.0f;

		state->frame = input->frame_id;
		state->on_panel = 0;
		state->mesh = NULL;
		state->button = -1;
		state->action = XWA_INPUT_ACTION_NONE;
		state->trigger_down = button_trigger_down(input, hand);
		if (!state->trigger_down) {
			button_release(state);
		}
		/* A hand that holds the stick or the throttle fires and isn't tested.
		 * Neither is a hand that points at the virtual screen. */
		if (!g_buttons.enabled || XwaVrStick_Held(hand) || hand == Aeron_XrPointerHand() ||
			!XwaRemasterVrHands_Ray(hand, seat_rows, seat_pos, origin, direction, &units_per_m)) {
			continue;
		}
		float distance = g_buttons.ray_m * units_per_m;
		AeronSceneMeshPick pick;
		const AeronSceneMesh* mesh = NULL;
		if (button_pick_instance(cockpit, origin, direction, &distance, &pick)) {
			mesh = cockpit->mesh;
		}
		/* The other hand's model can have buttons too, on its wrist. */
		AeronSceneMeshInstance other;
		AeronSceneMeshPick other_pick;
		if (XwaRemasterVrHands_Instance(!hand, seat_rows, seat_pos, &other) &&
			button_pick_instance(&other, origin, direction, &distance, &other_pick)) {
			mesh = other.mesh;
			pick = other_pick;
		}
		if (!mesh || !pick.has_buttons) {
			continue;
		}
		state->on_panel = 1;
		const float hit[3] = { origin[0] + direction[0] * distance - seat_pos[0],
							   origin[1] + direction[1] * distance - seat_pos[1],
							   origin[2] + direction[2] * distance - seat_pos[2] };
		for (int row = 0; row < 3; row++) {
			state->seat_point[row] = seat_rows[row * 3 + 0] * hit[0] + seat_rows[row * 3 + 1] * hit[1] +
									 seat_rows[row * 3 + 2] * hit[2];
		}
		if (pick.button < 0) {
			continue;
		}
		const AeronGltfButton* button = &mesh->buttons[pick.button];
		const XwaInputAction action = XwaInputActions_FromName(button->action);
		if (action == XWA_INPUT_ACTION_NONE) {
			continue;
		}
		state->mesh = mesh;
		state->button = pick.button;
		state->action = action;
		snprintf(state->label, sizeof state->label, "%s",
				 button->label[0] ? button->label : XwaInputActions_DisplayName(action));
		if (mesh != previous_mesh || pick.button != previous_button) {
			button_tick(hand, BUTTON_TICK_ENTER_MS);
			if (button_log()) {
				Aeron_LogInfo("xwa.remaster", "cockpit buttons: the %s hand is on '%s' (%s)",
							  hand ? "right" : "left", state->label, button->action);
			}
		}
		if (state->trigger_down && !was_down) {
			button_release(state);
			if (XwaInputActions_Kind(action) == XWA_INPUT_ACTION_KIND_HOST) {
				XwaControllerMapping_PressHostAction(action);
			} else {
				state->held_press = XwaControllerMapping_PressAction(action);
				state->held = action;
			}
			button_tick(hand, BUTTON_TICK_PRESS_MS);
			if (button_log()) {
				Aeron_LogInfo("xwa.remaster", "cockpit buttons: the %s hand pressed '%s'",
							  hand ? "right" : "left", state->label);
			}
		}
	}
}

static void button_color(float r, float g, float b, float a, float out[4]) {
	out[0] = XwaRemaster_SrgbToLinear(r) * a;
	out[1] = XwaRemaster_SrgbToLinear(g) * a;
	out[2] = XwaRemaster_SrgbToLinear(b) * a;
	out[3] = a;
}

void XwaRemasterCockpitButtons_PrepareMarkers(AeronCommandBuffer* cmd, XwaRemasterAssets* assets,
											  const XwaRemasterHudPlanes* planes, const float seat_rows[9],
											  const float seat_pos[3], int target_w, int target_h) {
	g_buttons.list_has_markers = 0;
	if (!cmd || target_w <= 0 || target_h <= 0) {
		return;
	}
	if (!g_buttons.list) {
		g_buttons.list = AeronDrawList_Create(256);
		if (!g_buttons.list) {
			return;
		}
	}
	/* The label's font: the middle flight font, at about 1.2 degrees. */
	const float text_px = planes ? planes->focal_px * 0.022f : 0.0f;
	const float dot_px = planes ? fmaxf(planes->focal_px * 0.006f, 3.0f) : 0.0f;
	XwaRemasterFlightFace face;
	int has_face = 0;
	const AeronFontAtlas* font = NULL;
	int any = 0;
	for (int hand = 0; hand < 2 && planes; hand++) {
		any |= XwaRemasterCockpitButtons_OnButton(hand);
	}
	if (any && assets) {
		XwaRemasterText_PrepareFlightFace(cmd, assets, 1, text_px);
		has_face = XwaRemasterText_FlightFace(assets, 1, text_px, &face);
		font = XwaRemasterAssets_FlightFont(assets, 1, NULL);
	}
	AeronDrawList_Begin(g_buttons.list, NULL, target_w, target_h, AERON_DRAWLIST2D_LOAD, NULL);
	for (int hand = 0; hand < 2 && planes && seat_rows && seat_pos; hand++) {
		const ButtonHand* state = &g_buttons.hands[hand];
		if (!XwaRemasterCockpitButtons_OnPanel(hand)) {
			continue;
		}
		float point[3], plane[9];
		for (int axis = 0; axis < 3; axis++) {
			point[axis] = seat_pos[axis] + seat_rows[0 * 3 + axis] * state->seat_point[0] +
						  seat_rows[1 * 3 + axis] * state->seat_point[1] +
						  seat_rows[2 * 3 + axis] * state->seat_point[2];
		}
		if (!(XwaHudPlanes_AtPoint(planes, point, plane) > 0.0f)) {
			continue;
		}
		const int index = AeronDrawList_AddPlane(g_buttons.list, plane, 1.0f);
		if (index < 0) {
			continue;
		}
		AeronDrawList_UsePlane(g_buttons.list, index);
		/* White on a panel, green with a ring on a button, blue while the
		 * trigger is pulled on it. */
		float rgba[4];
		const int on_button = state->button >= 0;
		if (!on_button) {
			button_color(1.0f, 1.0f, 1.0f, 0.9f, rgba);
		} else if (state->trigger_down) {
			button_color(0.3f, 0.6f, 1.0f, 1.0f, rgba);
		} else {
			button_color(0.3f, 1.0f, 0.3f, 1.0f, rgba);
		}
		const float x = planes->center_x, y = planes->center_y;
		AeronDrawList_AddFill(g_buttons.list, x - dot_px * 0.5f, y - dot_px * 0.5f, dot_px, dot_px, rgba,
							  AERON_BLIT2D_BLEND_PMA, NULL);
		if (on_button) {
			const float ring = dot_px * 2.6f;
			AeronDrawList_AddFrame(g_buttons.list, x - ring * 0.5f, y - ring * 0.5f, ring, ring,
								   fmaxf(dot_px * 0.25f, 1.0f), rgba, AERON_BLIT2D_BLEND_PMA, NULL);
			if (font || has_face) {
				XwaRemasterText_AddFlightString(g_buttons.list, font, has_face ? &face : NULL, state->label,
												x, y - ring * 0.5f - text_px * 1.3f, text_px,
												XWA_REMASTER_TEXT_ALIGN_CENTER, 0xffffffffu, NULL);
			}
		}
		g_buttons.list_has_markers = 1;
	}
	(void)AeronDrawList_Prepare(g_buttons.list, cmd);
}

void XwaRemasterCockpitButtons_RenderMarkers(AeronCommandBuffer* cmd, AeronRenderPass* pass,
											 AeronRenderTarget* target, const float view[12]) {
	if (g_buttons.list && g_buttons.list_has_markers && cmd && pass && target && view) {
		AeronDrawList_RenderPlanes(g_buttons.list, cmd, pass, target, view, 1.0f);
	}
}

void XwaRemasterCockpitButtons_Shutdown(void) {
	AeronDrawList_Destroy(g_buttons.list);
	g_buttons.list = NULL;
	g_buttons.list_has_markers = 0;
	memset(g_buttons.hands, 0, sizeof g_buttons.hands);
	memset(g_buttons.checked, 0, sizeof g_buttons.checked);
}
