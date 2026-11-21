#include "xwa_runtime/input/vr_stick.h"

#include "aeron/aeron.h"
#include "aeron/debug.h"
#include "aeron/xr.h"
#include "xwa/flight/flight_display.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa_runtime/input/capture.h"
#include "xwa_runtime/runtime/flight_pause_task.h"
#include "xwa_runtime/runtime/flight_task.h"
#include "xwa_runtime/runtime/movie_task.h"

#include <math.h>
#include <string.h>

/* The hand's rotation that gives the full deflection, in tilt mode. */
#define VR_STICK_TILT_RAD (30.0f * 3.14159265f / 180.0f)
#define VR_STICK_TWIST_RAD (40.0f * 3.14159265f / 180.0f)
/* The hand's movement that gives the full deflection, in slide mode. */
#define VR_STICK_SLIDE_M 0.12f
/* The part of the travel around the centre that gives nothing. */
#define VR_STICK_DEADZONE 0.10f
/* How long the stick takes to return to the centre when it is released. */
#define VR_STICK_RETURN_US 100000u
/* The hand's movement that takes the throttle from stopped to full. */
#define VR_THROTTLE_RANGE_M 0.15f

static struct {
	XwaModernInputOptions options;
	/* The mode in use: off, tilt or slide. */
	XwaModernVrStickMode mode;
	int stick_hand;
	/* -1 when the virtual throttle is off. */
	int throttle_hand;
	struct {
		int held;
		AeronViewPose reference;
		/* Yaw, pitch and roll, from -1 to 1. */
		float value[3];
		/* The values when the hand let go, and when it did. */
		float released[3];
		uint64_t released_us;
		int returning;
		int fire;
	} stick;
	struct {
		int held;
		AeronViewPose reference;
		/* How far the lever moved since the press: 1 is the whole range. */
		float travel;
		uint32_t generation;
		/* The craft's throttle at the press, and the grab it belongs to. */
		uint16_t base;
		uint32_t base_generation;
	} throttle;
	int rumble_hand;
} g_vrStick = { .mode = XWA_MODERN_VR_STICK_OFF, .stick_hand = 1, .throttle_hand = -1, .rumble_hand = -1 };

void XwaVrStick_SetOptions(const XwaModernInputOptions* options) {
	if (options) {
		g_vrStick.options = *options;
	}
}

static void quat_multiply(const float a[4], const float b[4], float out[4]) {
	out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
	out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
	out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
	out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

static void quat_rotate(const float q[4], const float v[3], float out[3]) {
	const float vq[4] = { v[0], v[1], v[2], 0.0f };
	const float conjugate[4] = { -q[0], -q[1], -q[2], q[3] };
	float t[4], r[4];
	quat_multiply(q, vq, t);
	quat_multiply(t, conjugate, r);
	out[0] = r[0];
	out[1] = r[1];
	out[2] = r[2];
}

static float dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

/* Removes the dead zone and rescales the rest of the travel, as the mapping
 * does for a stick. */
static float vr_stick_deadzone(float value) {
	const float magnitude = fabsf(value);
	if (magnitude <= VR_STICK_DEADZONE) {
		return 0.0f;
	}
	float scaled = (magnitude - VR_STICK_DEADZONE) / (1.0f - VR_STICK_DEADZONE);
	if (scaled > 1.0f) {
		scaled = 1.0f;
	}
	return value < 0.0f ? -scaled : scaled;
}

/* The hand is seen as the handle of a stick. The handle's axis is the grip
 * pose's -Z, which goes through the closed fingers, from the little finger
 * to the thumb. The rotation since the press is split in two: the twist
 * around the handle, and the tilt that moves the handle. The tilt is then
 * split along the seat's axes: forward and back, left and right.
 *
 * The rig's axes are X to the right, Y up and Z to the back. */
void XwaVrStick_Deflection(XwaModernVrStickMode mode, float range, const AeronViewPose* reference,
						   const AeronViewPose* now, float out[3]) {
	static const float handle_local[3] = { 0.0f, 0.0f, -1.0f };
	static const float right[3] = { 1.0f, 0.0f, 0.0f };
	const float* q0 = reference->orientation;
	const float inverse[4] = { -q0[0], -q0[1], -q0[2], q0[3] };
	float delta[4], handle[3], side[3], across[3];
	float tilt[3] = { 0.0f, 0.0f, 0.0f };

	if (!(range > 0.0f)) {
		range = 1.0f;
	}
	/* The rotation since the press, in the rig's space. */
	quat_multiply(now->orientation, inverse, delta);
	if (delta[3] < 0.0f) {
		for (int i = 0; i < 4; i++) {
			delta[i] = -delta[i];
		}
	}
	quat_rotate(q0, handle_local, handle);

	/* The twist is the part of the rotation around the handle. What is left
	 * is the tilt, as a rotation vector. */
	const float along = dot3(delta, handle);
	const float twist_length = sqrtf(along * along + delta[3] * delta[3]);
	float twist_angle = 0.0f;
	if (twist_length > 1e-6f) {
		const float twist_inverse[4] = { -handle[0] * along / twist_length, -handle[1] * along / twist_length,
										 -handle[2] * along / twist_length, delta[3] / twist_length };
		float swing[4];
		twist_angle = 2.0f * atan2f(along, delta[3]);
		quat_multiply(delta, twist_inverse, swing);
		const float length = sqrtf(dot3(swing, swing));
		if (length > 1e-6f) {
			const float angle = 2.0f * atan2f(length, fabsf(swing[3]));
			const float sign = swing[3] < 0.0f ? -1.0f : 1.0f;
			for (int i = 0; i < 3; i++) {
				tilt[i] = sign * swing[i] / length * angle;
			}
		}
	}

	/* `side` points to the player's right, across the handle. A rotation
	 * around it tilts the handle forward or back. `across` is the third
	 * axis: a rotation around it tilts the handle left or right. */
	float lean = dot3(right, handle);
	for (int i = 0; i < 3; i++) {
		side[i] = right[i] - lean * handle[i];
	}
	float side_length = sqrtf(dot3(side, side));
	if (side_length < 0.2f) {
		/* The handle lies across the seat. The palm's axis is used. */
		float palm[3];
		quat_rotate(q0, right, palm);
		lean = dot3(palm, handle);
		for (int i = 0; i < 3; i++) {
			side[i] = palm[i] - lean * handle[i];
		}
		side_length = sqrtf(dot3(side, side));
	}
	for (int i = 0; i < 3; i++) {
		side[i] /= side_length;
	}
	across[0] = handle[1] * side[2] - handle[2] * side[1];
	across[1] = handle[2] * side[0] - handle[0] * side[2];
	across[2] = handle[0] * side[1] - handle[1] * side[0];

	float yaw, pitch;
	if (mode == XWA_MODERN_VR_STICK_SLIDE) {
		/* The hand moves right to turn right, and back to pull up. */
		yaw = (now->position_m[0] - reference->position_m[0]) / (VR_STICK_SLIDE_M * range);
		pitch = (now->position_m[2] - reference->position_m[2]) / (VR_STICK_SLIDE_M * range);
	} else {
		yaw = dot3(tilt, across) / (VR_STICK_TILT_RAD * range);
		pitch = dot3(tilt, side) / (VR_STICK_TILT_RAD * range);
	}
	/* Seen from the thumb's side, a twist to the right rolls to the right. */
	const float roll = -twist_angle / (VR_STICK_TWIST_RAD * range);
	out[0] = vr_stick_deadzone(yaw);
	out[1] = vr_stick_deadzone(pitch);
	out[2] = vr_stick_deadzone(roll);
}

/* The hand's entry in the input snapshot, or NULL. */
static const AeronControllerSnapshot* vr_stick_controller(const AeronInputSnapshot* input, int hand) {
	const uint32_t id = hand ? AERON_VR_HAND_INSTANCE_RIGHT : AERON_VR_HAND_INSTANCE_LEFT;
	for (int i = 0; i < AERON_CONTROLLER_MAX; i++) {
		const AeronControllerSnapshot* device = &input->controllers[i];
		if (device->connected && device->kind == AERON_CONTROLLER_KIND_VR_HAND && device->instance_id == id) {
			return device;
		}
	}
	return NULL;
}

static void vr_stick_release(uint64_t now) {
	if (g_vrStick.stick.held) {
		memcpy(g_vrStick.stick.released, g_vrStick.stick.value, sizeof g_vrStick.stick.released);
		g_vrStick.stick.released_us = now;
		g_vrStick.stick.returning = 1;
	}
	g_vrStick.stick.held = 0;
	g_vrStick.stick.fire = 0;
}

void XwaVrStick_Update(const AeronInputSnapshot* input) {
	AeronXrHand hands[2];
	const uint64_t now = Aeron_NowUs();
	const int has_hands = input && Aeron_XrHands(hands);

	g_vrStick.mode = g_vrStick.options.vr_stick;
	g_vrStick.stick_hand = g_vrStick.options.vr_stick_hand != 0;
	g_vrStick.throttle_hand = g_vrStick.mode != XWA_MODERN_VR_STICK_OFF && g_vrStick.options.vr_throttle
								  ? !g_vrStick.stick_hand
								  : -1;
	if (g_vrStick.mode == XWA_MODERN_VR_STICK_OFF) {
		g_vrStick.rumble_hand = -1;
	}

	/* The hands only hold the stick and the throttle in flight, while the
	 * player flies: the same conditions as for the bindings. */
	const int flying = has_hands && g_vrStick.mode != XWA_MODERN_VR_STICK_OFF && input->has_focus &&
					   !XwaInput_IsCaptured() && !Aeron_DebugUiVisible() && XwaFlightTask_IsActive() &&
					   !FlightLoading_AreFrontendSurfacesAttached() &&
					   !XwaFlightTask_IsHoldingLoadingScreen() && !XwaFlightPauseTask_IsActive() &&
					   !FlightDisplay_IsFrontendModalActive() && !XwaMovieTask_IsActive();
	int squeezed[2] = { 0, 0 };
	const AeronControllerSnapshot* devices[2] = { NULL, NULL };
	for (int hand = 0; hand < 2 && flying; hand++) {
		devices[hand] = vr_stick_controller(input, hand);
		/* A hand that points at the virtual screen clicks with its squeeze. */
		squeezed[hand] = devices[hand] && hands[hand].active && hand != Aeron_XrPointerHand() &&
						 Aeron_ControllerButtonBit(devices[hand]->raw_buttons, AERON_VR_HAND_BUTTON_SQUEEZE);
	}

	const int stick = g_vrStick.stick_hand;
	if (squeezed[stick]) {
		if (!g_vrStick.stick.held) {
			g_vrStick.stick.held = 1;
			g_vrStick.stick.returning = 0;
			g_vrStick.stick.reference = hands[stick].grip;
			g_vrStick.rumble_hand = stick;
		}
		XwaVrStick_Deflection(g_vrStick.mode, g_vrStick.options.vr_stick_range, &g_vrStick.stick.reference,
							  &hands[stick].grip, g_vrStick.stick.value);
		g_vrStick.stick.fire =
			Aeron_ControllerButtonBit(devices[stick]->raw_buttons, AERON_VR_HAND_BUTTON_TRIGGER);
	} else {
		vr_stick_release(now);
		if (g_vrStick.stick.returning) {
			const uint64_t elapsed = now - g_vrStick.stick.released_us;
			const float left =
				elapsed < VR_STICK_RETURN_US ? 1.0f - (float)elapsed / VR_STICK_RETURN_US : 0.0f;
			for (int i = 0; i < 3; i++) {
				g_vrStick.stick.value[i] = g_vrStick.stick.released[i] * left;
			}
			g_vrStick.stick.returning = left > 0.0f;
		} else {
			memset(g_vrStick.stick.value, 0, sizeof g_vrStick.stick.value);
		}
	}

	const int throttle = g_vrStick.throttle_hand;
	if (throttle >= 0 && squeezed[throttle]) {
		if (!g_vrStick.throttle.held) {
			g_vrStick.throttle.held = 1;
			g_vrStick.throttle.reference = hands[throttle].grip;
			++g_vrStick.throttle.generation;
		}
		/* Forward is -Z in the rig's space. */
		g_vrStick.throttle.travel =
			(g_vrStick.throttle.reference.position_m[2] - hands[throttle].grip.position_m[2]) /
			VR_THROTTLE_RANGE_M;
	} else {
		g_vrStick.throttle.held = 0;
	}
}

int XwaVrStick_Held(int hand) {
	return (g_vrStick.stick.held && hand == g_vrStick.stick_hand) ||
		   (g_vrStick.throttle.held && hand == g_vrStick.throttle_hand);
}

void XwaVrStick_SourceMask(int hand, uint32_t* buttons, uint32_t* axes) {
	if (g_vrStick.mode == XWA_MODERN_VR_STICK_OFF ||
		(hand != g_vrStick.stick_hand && hand != g_vrStick.throttle_hand)) {
		return;
	}
	*buttons |= 1u << AERON_VR_HAND_BUTTON_SQUEEZE;
	*axes |= 1u << AERON_VR_HAND_AXIS_SQUEEZE;
	/* The trigger of the hand that holds the stick always fires. */
	if (g_vrStick.stick.held && hand == g_vrStick.stick_hand) {
		*buttons |= 1u << AERON_VR_HAND_BUTTON_TRIGGER;
		*axes |= 1u << AERON_VR_HAND_AXIS_TRIGGER;
	}
}

int XwaVrStick_Sample(int* axisX, int* axisY, int* axisR) {
	*axisX = (int)lroundf(g_vrStick.stick.value[0] * 127.0f);
	*axisY = (int)lroundf(g_vrStick.stick.value[1] * 127.0f);
	*axisR = (int)lroundf(g_vrStick.stick.value[2] * 127.0f);
	return g_vrStick.stick.held || g_vrStick.stick.returning;
}

int XwaVrStick_ButtonsMask(void) { return g_vrStick.stick.held && g_vrStick.stick.fire ? 1 : 0; }

int XwaVrStick_ThrottleSample(uint16_t craft_throttle, uint16_t* position, uint32_t* generation) {
	if (!g_vrStick.throttle.held) {
		return 0;
	}
	if (g_vrStick.throttle.base_generation != g_vrStick.throttle.generation) {
		g_vrStick.throttle.base_generation = g_vrStick.throttle.generation;
		g_vrStick.throttle.base = craft_throttle;
	}
	float value = (float)g_vrStick.throttle.base + g_vrStick.throttle.travel * (float)UINT16_MAX;
	if (value < 0.0f) {
		value = 0.0f;
	}
	if (value > (float)UINT16_MAX) {
		value = (float)UINT16_MAX;
	}
	*position = (uint16_t)lroundf(value);
	/* The top bit keeps these numbers apart from the mapping's. */
	*generation = 0x80000000u | g_vrStick.throttle.generation;
	return 1;
}

int XwaVrStick_RumbleHand(void) { return g_vrStick.rumble_hand; }
