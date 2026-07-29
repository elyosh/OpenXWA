#include "xwa_runtime/input/mouse_flight.h"

#include "aeron/aeron.h"
#include "aeron/input.h"
#include "aeron/time.h"

#include <math.h>
#include <stdint.h>

/* Rate-mode gains, TIE Fighter parity: TIE scales mouse deltas by 128 (X) and
 * 64 (Y) against the joystick's 120/50 axis scaling, with a x4 pixel-to-mickey
 * factor at a ~60 FPS cadence. XWA's input frame stores pre-scale int8 axes,
 * so the pixels-to-axis gains are 4*128/120 and 4*64/50, normalized to a
 * 60 FPS reference frame so deflection tracks mouse velocity independent of
 * the host frame rate. */
#define MOUSE_FLIGHT_GAIN_X (4.0f * 128.0f / 120.0f)
#define MOUSE_FLIGHT_GAIN_Y (4.0f * 64.0f / 50.0f)
#define MOUSE_FLIGHT_REFERENCE_FRAME_US 16667.0f
#define MOUSE_FLIGHT_MIN_FRAME_US 1000
#define MOUSE_FLIGHT_MAX_FRAME_US 100000
/* Rate mode: overflow past the +/-127 clamp is banked in the signed carry and
 * drained at full deflection, bounded to ~0.15 s of pegged deflection so a
 * fast flick cannot queue a long uncommanded turn. */
#define MOUSE_FLIGHT_MAX_BACKLOG (127.0f * 9.0f)
/* Position mode: axis units per pixel of mouse travel at the default
 * sensitivity notch (full deflection after ~256 px). */
#define MOUSE_FLIGHT_POSITION_GAIN (127.0f / 256.0f)
/* Right-button tap window: release inside it emits the target-in-sight tap
 * action, roll-lock engages only after it. Matches the recovered 59-sim-tick
 * window Flight_UpdateEntity uses for joystick button 2. */
#define MOUSE_FLIGHT_TAP_US 250000u

static struct {
	XwaModernInputOptions options;
	uint64_t pumped_frame;
	/* Time of the last drain; 0 = no drain since (re)activation. */
	uint64_t drain_time_us;
	/* Motion accumulated by the per-host-frame pump, awaiting a game read. */
	float pending_x;
	float pending_y;
	/* Sub-unit remainders carried across drains so slow trackpad motion still
	 * registers instead of truncating to zero every read (rate mode). */
	float carry_x;
	float carry_y;
	/* Position mode: the held virtual-stick deflection, in axis units. */
	float stick_x;
	float stick_y;
	float stick_r;
	int axis_x;
	int axis_y;
	int axis_r;
	int buttons;
	/* Right button held past the tap window: mouse X drives roll, yaw/pitch
	 * suppressed. */
	int roll_lock;
	int drained_roll_lock;
	uint64_t rmb_press_time_us;
	int rmb_down;
	/* A right-button tap was released inside the tap window and awaits its
	 * target-in-sight action key. */
	int target_tap;
	int active;
} g_mouseFlight;

/* Doubling steps (1/16x..16x): raw relative deltas vary by more than an
 * order of magnitude between touchpads and high-DPI mice. */
static const float k_mouseFlightSensitivityScale[XWA_MODERN_MOUSE_SENSITIVITY_MAX] = {
	0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f,
};

/* Joystick button bits in the recovered binding-table order (LEFT, -,
 * MIDDLE, X1; Aeron's own bit order differs). The right button is not
 * forwarded: it is the roll-lock/tap modifier, and forwarding it would
 * engage the joystick button 2 yaw/roll swap binding. */
static int XwaMouseFlight_ButtonsFromAeron(uint32_t buttons) {
	int mask = 0;

	if (buttons & AERON_MOUSE_BUTTON_LEFT) {
		mask |= 1 << 0;
	}
	if (buttons & AERON_MOUSE_BUTTON_MIDDLE) {
		mask |= 1 << 2;
	}
	if (buttons & AERON_MOUSE_BUTTON_X1) {
		mask |= 1 << 3;
	}
	return mask;
}

static void XwaMouseFlight_ResetAxes(void) {
	g_mouseFlight.pending_x = 0.0f;
	g_mouseFlight.pending_y = 0.0f;
	g_mouseFlight.carry_x = 0.0f;
	g_mouseFlight.carry_y = 0.0f;
	g_mouseFlight.stick_x = 0.0f;
	g_mouseFlight.stick_y = 0.0f;
	g_mouseFlight.stick_r = 0.0f;
	g_mouseFlight.axis_x = 0;
	g_mouseFlight.axis_y = 0;
	g_mouseFlight.axis_r = 0;
	g_mouseFlight.buttons = 0;
	g_mouseFlight.roll_lock = 0;
	g_mouseFlight.rmb_down = 0;
	g_mouseFlight.target_tap = 0;
	g_mouseFlight.drain_time_us = 0;
	g_mouseFlight.active = 0;
}

void XwaMouseFlight_SetOptions(const XwaModernInputOptions* options) {
	int mode_changed;

	if (!options) {
		return;
	}
	mode_changed = options->mouse_mode != g_mouseFlight.options.mouse_mode;
	g_mouseFlight.options = *options;
	if (!g_mouseFlight.options.mouse_flight_enabled || mode_changed) {
		XwaMouseFlight_ResetAxes();
	}
}

void XwaMouseFlight_Suspend(void) {
	g_mouseFlight.pending_x = 0.0f;
	g_mouseFlight.pending_y = 0.0f;
	g_mouseFlight.carry_x = 0.0f;
	g_mouseFlight.carry_y = 0.0f;
	g_mouseFlight.stick_x = 0.0f;
	g_mouseFlight.stick_y = 0.0f;
	g_mouseFlight.stick_r = 0.0f;
	g_mouseFlight.axis_x = 0;
	g_mouseFlight.axis_y = 0;
	g_mouseFlight.axis_r = 0;
	g_mouseFlight.target_tap = 0;
}

static int XwaMouseFlight_TakeAxis(float* carry, float value) {
	int units;

	*carry += value;
	if (*carry > MOUSE_FLIGHT_MAX_BACKLOG) {
		*carry = MOUSE_FLIGHT_MAX_BACKLOG;
	} else if (*carry < -MOUSE_FLIGHT_MAX_BACKLOG) {
		*carry = -MOUSE_FLIGHT_MAX_BACKLOG;
	}
	units = (int)floorf(*carry);
	if (units > 127) {
		units = 127;
	} else if (units < -127) {
		units = -127;
	}
	*carry -= (float)units;
	return units;
}

void XwaMouseFlight_Pump(void) {
	const AeronInputSnapshot* in = Aeron_InputSnapshot();

	if (!g_mouseFlight.options.mouse_flight_enabled || !in) {
		XwaMouseFlight_ResetAxes();
		return;
	}
	if (in->frame_id == g_mouseFlight.pumped_frame) {
		return;
	}
	g_mouseFlight.pumped_frame = in->frame_id;
	/* Only a captured pointer belongs to flight controls: while the capture is
	 * released to the OS, motion over the window must not steer the ship. */
	if (!(in->has_focus && in->mouse.inside_content && Aeron_RelativeMouseMode())) {
		XwaMouseFlight_ResetAxes();
		return;
	}
	g_mouseFlight.active = 1;
	g_mouseFlight.pending_x += in->mouse.relative_x;
	g_mouseFlight.pending_y += in->mouse.relative_y;
	g_mouseFlight.buttons = XwaMouseFlight_ButtonsFromAeron(in->mouse.buttons);
	{
		const int rmb = (in->mouse.buttons & AERON_MOUSE_BUTTON_RIGHT) != 0;
		const uint64_t now = Aeron_NowUs();

		if (rmb && !g_mouseFlight.rmb_down) {
			g_mouseFlight.rmb_press_time_us = now;
		} else if (!rmb && g_mouseFlight.rmb_down &&
				   now - g_mouseFlight.rmb_press_time_us < MOUSE_FLIGHT_TAP_US) {
			g_mouseFlight.target_tap = 1;
		}
		g_mouseFlight.rmb_down = rmb;
		g_mouseFlight.roll_lock = rmb && now - g_mouseFlight.rmb_press_time_us >= MOUSE_FLIGHT_TAP_US;
	}
}

static float XwaMouseFlight_ClampStick(float value) {
	if (value > 127.0f) {
		return 127.0f;
	}
	if (value < -127.0f) {
		return -127.0f;
	}
	return value;
}

static int XwaMouseFlight_StickAxis(float value) { return (int)floorf(value + 0.5f); }

/* Position mode: mouse displacement moves a held virtual-stick deflection.
 * While roll-lock is held, X motion moves a transient roll deflection and the
 * yaw/pitch stick is frozen; roll recenters when the button is released. */
static void XwaMouseFlight_SamplePosition(float sensitivity) {
	const float gain = MOUSE_FLIGHT_POSITION_GAIN * sensitivity;
	float delta_y = g_mouseFlight.pending_y * gain;

	if (g_mouseFlight.options.mouse_invert_y) {
		delta_y = -delta_y;
	}
	if (g_mouseFlight.roll_lock != g_mouseFlight.drained_roll_lock) {
		g_mouseFlight.drained_roll_lock = g_mouseFlight.roll_lock;
		g_mouseFlight.stick_r = 0.0f;
	}
	if (g_mouseFlight.roll_lock) {
		g_mouseFlight.stick_r =
			XwaMouseFlight_ClampStick(g_mouseFlight.stick_r + g_mouseFlight.pending_x * gain);
	} else {
		g_mouseFlight.stick_r = 0.0f;
		g_mouseFlight.stick_x =
			XwaMouseFlight_ClampStick(g_mouseFlight.stick_x + g_mouseFlight.pending_x * gain);
		g_mouseFlight.stick_y = XwaMouseFlight_ClampStick(g_mouseFlight.stick_y + delta_y);
	}
	g_mouseFlight.axis_x = XwaMouseFlight_StickAxis(g_mouseFlight.stick_x);
	g_mouseFlight.axis_y = XwaMouseFlight_StickAxis(g_mouseFlight.stick_y);
	g_mouseFlight.axis_r = XwaMouseFlight_StickAxis(g_mouseFlight.stick_r);
}

/* Rate mode (TIE Fighter scheme): motion since the last read, normalized by
 * the read interval, is this frame's deflection. */
static void XwaMouseFlight_SampleRate(float sensitivity, uint64_t interval_us) {
	const float rate_scale = MOUSE_FLIGHT_REFERENCE_FRAME_US / (float)interval_us;
	int axis_from_x;
	int axis_from_y;
	float delta_y;

	/* Entering or leaving roll-lock rebinds what the X motion means; drop any
	 * banked motion so it cannot bleed into the other axis. */
	if (g_mouseFlight.roll_lock != g_mouseFlight.drained_roll_lock) {
		g_mouseFlight.drained_roll_lock = g_mouseFlight.roll_lock;
		g_mouseFlight.carry_x = 0.0f;
		g_mouseFlight.carry_y = 0.0f;
	}
	axis_from_x = XwaMouseFlight_TakeAxis(&g_mouseFlight.carry_x, g_mouseFlight.pending_x * rate_scale *
																	  sensitivity * MOUSE_FLIGHT_GAIN_X);
	delta_y = g_mouseFlight.pending_y * rate_scale * sensitivity * MOUSE_FLIGHT_GAIN_Y;
	if (g_mouseFlight.options.mouse_invert_y) {
		delta_y = -delta_y;
	}
	axis_from_y = XwaMouseFlight_TakeAxis(&g_mouseFlight.carry_y, delta_y);
	if (g_mouseFlight.roll_lock) {
		/* TIE roll-lock: X motion rolls, everything else is suppressed. */
		g_mouseFlight.axis_x = 0;
		g_mouseFlight.axis_y = 0;
		g_mouseFlight.axis_r = axis_from_x;
	} else {
		g_mouseFlight.axis_x = axis_from_x;
		g_mouseFlight.axis_y = axis_from_y;
		g_mouseFlight.axis_r = 0;
	}
}

int XwaMouseFlight_Sample(void) {
	uint64_t now;
	uint64_t interval_us;
	int position_mode;
	float sensitivity;

	XwaMouseFlight_Pump();
	if (!g_mouseFlight.active) {
		return 0;
	}

	position_mode = g_mouseFlight.options.mouse_mode == XWA_MODERN_MOUSE_MODE_POSITION;
	now = Aeron_NowUs();
	interval_us = now - g_mouseFlight.drain_time_us;
	/* First drain after (re)activation, or return from a long stall: the
	 * transition motion may include a capture warp — drop it. In position
	 * mode the held deflection survives a stall; rate mode has no usable
	 * interval to normalize against and outputs neutral. */
	if (g_mouseFlight.drain_time_us == 0 || interval_us > MOUSE_FLIGHT_MAX_FRAME_US) {
		g_mouseFlight.drain_time_us = now;
		g_mouseFlight.pending_x = 0.0f;
		g_mouseFlight.pending_y = 0.0f;
		g_mouseFlight.carry_x = 0.0f;
		g_mouseFlight.carry_y = 0.0f;
		g_mouseFlight.axis_x = position_mode ? XwaMouseFlight_StickAxis(g_mouseFlight.stick_x) : 0;
		g_mouseFlight.axis_y = position_mode ? XwaMouseFlight_StickAxis(g_mouseFlight.stick_y) : 0;
		g_mouseFlight.axis_r = position_mode ? XwaMouseFlight_StickAxis(g_mouseFlight.stick_r) : 0;
		return 1;
	}
	g_mouseFlight.drain_time_us = now;
	if (interval_us < MOUSE_FLIGHT_MIN_FRAME_US) {
		interval_us = MOUSE_FLIGHT_MIN_FRAME_US;
	}
	sensitivity = k_mouseFlightSensitivityScale[g_mouseFlight.options.mouse_sensitivity -
												XWA_MODERN_MOUSE_SENSITIVITY_MIN];
	if (position_mode) {
		XwaMouseFlight_SamplePosition(sensitivity);
	} else {
		XwaMouseFlight_SampleRate(sensitivity, interval_us);
	}
	g_mouseFlight.pending_x = 0.0f;
	g_mouseFlight.pending_y = 0.0f;
	return 1;
}

void XwaMouseFlight_GetAxes(int* axisX, int* axisY, int* axisR) {
	if (axisX) {
		*axisX = g_mouseFlight.axis_x;
	}
	if (axisY) {
		*axisY = g_mouseFlight.axis_y;
	}
	if (axisR) {
		*axisR = g_mouseFlight.axis_r;
	}
}

int XwaMouseFlight_ButtonsMask(void) { return g_mouseFlight.buttons; }

int XwaMouseFlight_TakeTargetTap(void) {
	const int tap = g_mouseFlight.target_tap;

	g_mouseFlight.target_tap = 0;
	return tap && g_mouseFlight.options.mouse_flight_enabled && g_mouseFlight.active;
}

int XwaMouseFlight_GetHudMarker(int* deflectionX, int* deflectionY) {
	if (!g_mouseFlight.options.mouse_flight_enabled || !g_mouseFlight.active ||
		g_mouseFlight.options.mouse_mode != XWA_MODERN_MOUSE_MODE_POSITION) {
		return 0;
	}
	if (deflectionX) {
		*deflectionX = XwaMouseFlight_StickAxis(g_mouseFlight.stick_x);
	}
	if (deflectionY) {
		*deflectionY = XwaMouseFlight_StickAxis(g_mouseFlight.stick_y);
	}
	return 1;
}
