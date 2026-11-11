#include "xwa_runtime/input/controller_mapping.h"

#include "aeron/debug.h"
#include "aeron/log.h"
#include "xwa/flight/player/player.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa_runtime/input/capture.h"
#include "xwa_runtime/runtime/flight_pause_task.h"
#include "xwa_runtime/runtime/flight_task.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { ACTION_QUEUE_CAPACITY = 256 };

typedef struct ControllerInstance {
	uint32_t id;
	int model;
	bool down[XWA_CONTROLLER_BINDING_CAP], armed[XWA_CONTROLLER_BINDING_CAP],
		dispatched[XWA_CONTROLLER_BINDING_CAP];
	bool primed;
} ControllerInstance;

static struct {
	XwaControllerOptions options, pending;
	ControllerInstance instances[AERON_CONTROLLER_MAX];
	uint32_t analog[XWA_CONTROLLER_MODEL_CAP];
	bool pending_options, suspended, present, throttle_valid;
	bool present_reported, presence_changed;
	bool resume;
	int axes[3];
	uint16_t holds[XWA_INPUT_ACTION_COUNT], throttle;
	uint32_t throttle_instance, generation;
	uint32_t rumble_instance;
	bool rumble_changed, rumble_enabled;
	uint16_t queue[ACTION_QUEUE_CAPACITY];
	unsigned read, write;
	uint64_t frame;
} g_controller;

static void XwaControllerMapping_QueueKey(uint16_t key) {
	unsigned next = (g_controller.write + 1) % ACTION_QUEUE_CAPACITY;
	if (next == g_controller.read) {
		Aeron_LogWarn("xwa.input", "Controller command queue is full");
		return;
	}
	g_controller.queue[g_controller.write] = key;
	g_controller.write = next;
}

/* Commands reach the game only in flight, after loading. While the local
 * player types a message they are dropped: the typing code would add their
 * key codes to the message. */
static bool XwaControllerMapping_CommandsAllowed(void) {
	return XwaFlightTask_IsActive() && !FlightLoading_AreFrontendSurfacesAttached() &&
		   !XwaFlightTask_IsHoldingLoadingScreen() && (unsigned)g_localPlayer < 8 &&
		   g_players[g_localPlayer].msgTypeId == 0;
}

static void XwaControllerMapping_DispatchAction(XwaInputAction action) {
	if (!XwaControllerMapping_CommandsAllowed())
		return;
	if (XwaInputActions_Kind(action) == XWA_INPUT_ACTION_KIND_PAUSE && XwaFlightPauseTask_IsActive()) {
		g_controller.resume = true;
		return;
	}
	XwaControllerMapping_QueueKey(XwaInputActions_Key(action));
}

/* Held, look and roll actions count their presses, so another device's
 * release doesn't end them. A held action sends its key once, on the first
 * press, as a joystick button did in FlightInput_Read(). There is no release
 * command. */
static void XwaControllerMapping_Dispatch(XwaInputAction action, bool pressed) {
	const XwaInputActionKind kind = XwaInputActions_Kind(action);
	if (kind == XWA_INPUT_ACTION_KIND_HELD || kind == XWA_INPUT_ACTION_KIND_LOOK ||
		kind == XWA_INPUT_ACTION_KIND_ROLL) {
		if (!pressed) {
			if (g_controller.holds[action])
				--g_controller.holds[action];
			return;
		}
		if (g_controller.holds[action]++ || kind != XWA_INPUT_ACTION_KIND_HELD)
			return;
	}
	if (pressed)
		XwaControllerMapping_DispatchAction(action);
}

const AeronControllerSnapshot* XwaControllerMapping_Resolve(const XwaControllerModel* model,
															const AeronInputSnapshot* input,
															uint32_t preferred) {
	const AeronControllerSnapshot* best = NULL;
	if (!input)
		return NULL;
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		const AeronControllerSnapshot* d = &input->controllers[i];
		if (!d->connected || strcmp(model->guid, d->guid) || d->kind != model->kind)
			continue;
		if (d->instance_id == preferred)
			return d;
		if (!best || d->instance_id < best->instance_id)
			best = d;
	}
	return best;
}

static void ReleaseInstance(ControllerInstance* state) {
	if (state->id) {
		const XwaControllerProfile* p = &g_controller.options.models[state->model].profile;
		for (size_t i = 0; i < p->binding_count; ++i)
			if (state->dispatched[i])
				XwaControllerMapping_Dispatch(p->bindings[i].action, false);
	}
	memset(state, 0, sizeof *state);
}

static void XwaControllerMapping_Install(const XwaControllerOptions* options) {
	static const XwaControllerOptions empty = { 0 };
	if (!options)
		options = &empty;
	char error[128];
	if (!XwaControllerOptions_Validate(options, error, sizeof error)) {
		Aeron_LogWarn("xwa.input", "%s", error);
		return;
	}
	if (XwaControllerOptions_Equals(options, &g_controller.options))
		return;
	uint32_t analog[XWA_CONTROLLER_MODEL_CAP] = { 0 };
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		ControllerInstance* state = &g_controller.instances[i];
		if (!state->id)
			continue;
		const XwaControllerModel* old = &g_controller.options.models[state->model];
		int next = XwaControllerOptions_FindModel(options, old->guid);
		if (next < 0 || old->kind != options->models[next].kind ||
			!XwaControllerOptions_ProfileEqual(&old->profile, &options->models[next].profile))
			ReleaseInstance(state);
	}
	for (size_t i = 0; i < options->count; ++i) {
		int old = XwaControllerOptions_FindModel(&g_controller.options, options->models[i].guid);
		if (old >= 0 && options->models[i].kind == g_controller.options.models[old].kind)
			analog[i] = g_controller.analog[old];
	}
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		ControllerInstance* state = &g_controller.instances[i];
		if (state->id)
			state->model =
				XwaControllerOptions_FindModel(options, g_controller.options.models[state->model].guid);
	}
	/* Only changes to the effective throttle binding invalidate its movement baseline. */
	for (size_t i = 0; i < g_controller.options.count; ++i) {
		const XwaControllerModel* old = &g_controller.options.models[i];
		const XwaInputAxisBinding* a = &old->profile.mapping.axes[XWA_INPUT_AXIS_THROTTLE];
		if (a->source < 0)
			continue;
		int n = XwaControllerOptions_FindModel(options, old->guid);
		if (n < 0 || old->kind != options->models[n].kind ||
			a->source != options->models[n].profile.mapping.axes[XWA_INPUT_AXIS_THROTTLE].source ||
			a->invert != options->models[n].profile.mapping.axes[XWA_INPUT_AXIS_THROTTLE].invert)
			++g_controller.generation;
	}
	g_controller.options = *options;
	memcpy(g_controller.analog, analog, sizeof analog);
}

void XwaControllerMapping_Init(const XwaControllerOptions* options) {
	memset(&g_controller, 0, sizeof g_controller);
	g_controller.frame = UINT64_MAX;
	g_controller.rumble_enabled = true;
	XwaControllerMapping_Install(options);
}

void XwaControllerMapping_SetOptions(const XwaControllerOptions* options) {
	char error[128];
	if (!XwaControllerOptions_Validate(options, error, sizeof error)) {
		Aeron_LogWarn("xwa.input", "%s", error);
		return;
	}
	g_controller.pending = *options;
	g_controller.pending_options = true;
}

void XwaControllerMapping_ApplyPending(void) {
	if (!g_controller.pending_options)
		return;
	XwaControllerMapping_Install(&g_controller.pending);
	g_controller.pending_options = false;
}

const XwaControllerOptions* XwaControllerMapping_Options(void) { return &g_controller.options; }

void XwaControllerMapping_Suspend(void) {
	if (g_controller.suspended)
		return;
	/* Drop the entire route without dispatching synthetic gameplay releases. */
	memset(g_controller.instances, 0, sizeof g_controller.instances);
	memset(g_controller.holds, 0, sizeof g_controller.holds);
	memset(g_controller.axes, 0, sizeof g_controller.axes);
	g_controller.read = g_controller.write = 0;
	g_controller.resume = false;
	g_controller.throttle_valid = false;
	g_controller.suspended = true;
	++g_controller.generation;
}

static void XwaControllerMapping_Resume(void) { g_controller.suspended = false; }

void XwaControllerMapping_Shutdown(void) {
	if (g_controller.rumble_instance)
		Aeron_RumbleController(g_controller.rumble_instance, 0, 0, 0);
	memset(&g_controller, 0, sizeof g_controller);
}

static void LogUnavailableControls(const XwaControllerModel* model, const AeronControllerSnapshot* device) {
	int missing = 0;
	for (int axis = 0; axis < XWA_INPUT_AXIS_COUNT; ++axis) {
		int source = model->profile.mapping.axes[axis].source;
		if (source >= 0 && !Aeron_ControllerAxisAvailable(device, source))
			++missing;
	}
	for (size_t i = 0; i < model->profile.binding_count; ++i) {
		const AeronControllerDigitalSource* source = &model->profile.bindings[i].source;
		bool available = false;
		if (source->kind == AERON_CONTROLLER_DIGITAL_BUTTON)
			available = model->kind == AERON_CONTROLLER_KIND_GAMEPAD
							? (device->gamepad_available_buttons & (1u << source->index)) != 0
							: source->index < device->button_count;
		else if (source->kind == AERON_CONTROLLER_DIGITAL_HAT)
			available = source->index < device->hat_count;
		else
			available = Aeron_ControllerAxisAvailable(device, source->index);
		if (!available)
			++missing;
	}
	if (missing || device->controls_truncated)
		Aeron_LogWarn("xwa.input", "controller '%s': %d unavailable configured controls%s", device->name,
					  missing, device->controls_truncated ? ", hardware controls truncated" : "");
}

static void SampleDigital(ControllerInstance* state, const AeronControllerSnapshot* device) {
	const XwaControllerModel* model = &g_controller.options.models[state->model];
	const XwaControllerProfile* profile = &model->profile;
	if (!state->primed)
		LogUnavailableControls(model, device);
	for (size_t i = 0; i < profile->binding_count; ++i) {
		bool down = Aeron_ControllerDigitalSourceDown(device, &profile->bindings[i].source, state->down[i]);
		if (!state->primed)
			state->armed[i] = !down;
		else if (!down) {
			if (state->dispatched[i])
				XwaControllerMapping_Dispatch(profile->bindings[i].action, false);
			state->dispatched[i] = false;
			state->armed[i] = true;
		} else if (state->armed[i] && !state->down[i]) {
			XwaControllerMapping_Dispatch(profile->bindings[i].action, true);
			state->dispatched[i] = true;
		}
		state->down[i] = down;
	}
	state->primed = true;
}

uint16_t XwaControllerMapping_ThrottlePosition(int16_t raw, AeronControllerKind kind, int source,
											   bool invert) {
	const uint32_t end_tolerance = (uint32_t)ceilf(0.5f * UINT16_MAX / 100.0f);
	const uint32_t range = UINT16_MAX - 2 * end_tolerance;
	uint32_t position = (int32_t)raw + 32768;
	if (kind == AERON_CONTROLLER_KIND_GAMEPAD && source >= AERON_GAMEPAD_AXIS_LEFT_TRIGGER)
		position = ((uint32_t)(raw < 0 ? 0 : raw) * UINT16_MAX + 16383u) / 32767u;
	if (invert)
		position = UINT16_MAX - position;
	/* Saturate both endpoints and scale the remaining travel continuously. */
	if (position <= end_tolerance)
		return 0;
	if (position >= UINT16_MAX - end_tolerance)
		return UINT16_MAX;
	return (uint16_t)(((position - end_tolerance) * UINT16_MAX + range / 2) / range);
}

static void SampleAnalog(size_t index, const AeronControllerSnapshot* device) {
	const XwaControllerModel* model = &g_controller.options.models[index];
	for (int axis = 0; axis < XWA_INPUT_AXIS_COUNT; ++axis) {
		XwaInputAxisBinding binding = model->profile.mapping.axes[axis];
		if (!Aeron_ControllerAxisAvailable(device, binding.source))
			continue;
		int16_t raw = Aeron_ControllerAxisValue(device, binding.source);
		if (axis == XWA_INPUT_AXIS_THROTTLE) {
			g_controller.throttle =
				XwaControllerMapping_ThrottlePosition(raw, model->kind, binding.source, binding.invert);
			g_controller.throttle_valid = true;
			if (g_controller.throttle_instance != device->instance_id) {
				g_controller.throttle_instance = device->instance_id;
				++g_controller.generation;
			}
		} else {
			float normalized = (float)raw / 32768;
			int value = fabsf(normalized) > binding.deadzone ? (int)(normalized * 127) : 0;
			g_controller.axes[axis] =
				XwaControllerOptions_EffectiveAxisInvert(model->kind, (XwaInputAxis)axis, binding.invert)
					? -value
					: value;
		}
	}
}

static uint32_t XwaControllerMapping_FindRumbleInstance(const AeronInputSnapshot* input) {
	static const XwaInputAxis owners[] = { XWA_INPUT_AXIS_PITCH, XWA_INPUT_AXIS_YAW };
	for (size_t i = 0; i < sizeof owners / sizeof owners[0]; ++i)
		for (size_t model = 0; model < g_controller.options.count; ++model)
			if (g_controller.options.models[model].profile.mapping.axes[owners[i]].source >= 0 &&
				g_controller.analog[model] && Aeron_ControllerHasRumble(g_controller.analog[model]))
				return g_controller.analog[model];
	for (size_t model = 0; model < g_controller.options.count; ++model) {
		const XwaControllerModel* m = &g_controller.options.models[model];
		uint32_t best = 0;
		if (m->kind != AERON_CONTROLLER_KIND_GAMEPAD)
			continue;
		for (int j = 0; j < AERON_CONTROLLER_MAX; ++j) {
			const AeronControllerSnapshot* d = &input->controllers[j];
			if (d->connected && d->has_rumble && !strcmp(d->guid, m->guid) && d->kind == m->kind &&
				(!best || d->instance_id < best))
				best = d->instance_id;
		}
		if (best)
			return best;
	}
	return 0;
}

void XwaControllerMapping_Update(const AeronInputSnapshot* input) {
	if (!input || input->frame_id == g_controller.frame)
		return;
	g_controller.frame = input->frame_id;
	if (!input->has_focus || XwaInput_IsCaptured() || Aeron_DebugUiVisible())
		XwaControllerMapping_Suspend();
	else
		XwaControllerMapping_Resume();
	g_controller.present = false;
	g_controller.throttle_valid = false;
	memset(g_controller.axes, 0, sizeof g_controller.axes);
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		ControllerInstance* state = &g_controller.instances[i];
		bool found = false;
		for (int j = 0; j < AERON_CONTROLLER_MAX && state->id; ++j) {
			const AeronControllerSnapshot* d = &input->controllers[j];
			const XwaControllerModel* m = &g_controller.options.models[state->model];
			if (d->connected && d->instance_id == state->id && !strcmp(d->guid, m->guid) &&
				d->kind == m->kind)
				found = true;
		}
		if (!found)
			ReleaseInstance(state);
	}

	for (size_t model = 0; model < g_controller.options.count; ++model) {
		const AeronControllerSnapshot* analog = XwaControllerMapping_Resolve(
			&g_controller.options.models[model], input, g_controller.analog[model]);
		g_controller.analog[model] = analog ? analog->instance_id : 0;
		if (analog) {
			g_controller.present = true;
			if (!g_controller.suspended)
				SampleAnalog(model, analog);
		}
		if (g_controller.suspended)
			continue;
		uint32_t previous = 0;
		for (int n = 0; n < AERON_CONTROLLER_MAX; ++n) {
			const AeronControllerSnapshot* d = NULL;
			for (int j = 0; j < AERON_CONTROLLER_MAX; ++j) {
				const AeronControllerSnapshot* candidate = &input->controllers[j];
				if (candidate->connected && candidate->instance_id > previous &&
					!strcmp(candidate->guid, g_controller.options.models[model].guid) &&
					candidate->kind == g_controller.options.models[model].kind &&
					(!d || candidate->instance_id < d->instance_id))
					d = candidate;
			}
			if (!d)
				break;
			previous = d->instance_id;
			ControllerInstance* state = NULL;
			for (int j = 0; j < AERON_CONTROLLER_MAX; ++j)
				if (g_controller.instances[j].id == d->instance_id)
					state = &g_controller.instances[j];
			if (!state)
				for (int j = 0; j < AERON_CONTROLLER_MAX; ++j)
					if (!g_controller.instances[j].id) {
						state = &g_controller.instances[j];
						state->id = d->instance_id;
						state->model = (int)model;
						break;
					}
			if (state)
				SampleDigital(state, d);
		}
	}
	if (!g_controller.throttle_valid)
		g_controller.throttle_instance = 0;
	if (g_controller.present != g_controller.present_reported) {
		g_controller.present_reported = g_controller.present;
		g_controller.presence_changed = true;
	}
	uint32_t rumble = XwaControllerMapping_FindRumbleInstance(input);
	if (rumble != g_controller.rumble_instance) {
		if (g_controller.rumble_instance)
			Aeron_RumbleController(g_controller.rumble_instance, 0, 0, 0);
		Aeron_LogInfo("xwa.input", "Rumble controller changed from %u to %u", g_controller.rumble_instance,
					  rumble);
		g_controller.rumble_instance = rumble;
		g_controller.rumble_changed = true;
	}
}

uint32_t XwaControllerMapping_AnalogInstance(const char* guid) {
	int i = XwaControllerOptions_FindModel(&g_controller.options, guid);
	return i < 0 ? 0 : g_controller.analog[i];
}

bool XwaControllerMapping_ThrottleSample(uint16_t* position, uint32_t* generation) {
	*position = g_controller.throttle;
	*generation = g_controller.generation;
	return g_controller.throttle_valid;
}

int XwaControllerMapping_Present(void) { return g_controller.present; }

int XwaControllerMapping_ConsumePresenceChange(void) {
	const bool changed = g_controller.presence_changed;
	g_controller.presence_changed = false;
	return changed;
}

int XwaControllerMapping_Axis(XwaInputAxis axis) { return (unsigned)axis < 3 ? g_controller.axes[axis] : 0; }

uint16_t XwaControllerMapping_Modifiers(void) {
	return (g_controller.holds[XWA_INPUT_ACTION_FIRE_WEAPON] ? 1 : 0) |
		   (g_controller.holds[XWA_INPUT_ACTION_TARGET_ROLL_MODIFIER] ? 2 : 0);
}

uint16_t XwaControllerMapping_ReadKey(void) {
	if (g_controller.read == g_controller.write)
		return 0;
	uint16_t key = g_controller.queue[g_controller.read];
	g_controller.read = (g_controller.read + 1) % ACTION_QUEUE_CAPACITY;
	return key;
}

uint16_t XwaControllerMapping_ReadLookKey(void) {
	/* The order of the original default joystick table: up, right, down, left. */
	static const XwaInputAction order[] = { XWA_INPUT_ACTION_LOOK_UP, XWA_INPUT_ACTION_LOOK_RIGHT,
											XWA_INPUT_ACTION_LOOK_DOWN, XWA_INPUT_ACTION_LOOK_LEFT };
	if (!XwaControllerMapping_CommandsAllowed())
		return 0;
	for (size_t i = 0; i < sizeof order / sizeof order[0]; ++i)
		if (g_controller.holds[order[i]])
			return XwaInputActions_Key(order[i]);
	return 0;
}

int XwaControllerMapping_RollHeld(XwaInputRoll side) {
	return side == XWA_INPUT_ROLL_LEFT    ? g_controller.holds[XWA_INPUT_ACTION_ROLL_LEFT] != 0
		   : side == XWA_INPUT_ROLL_RIGHT ? g_controller.holds[XWA_INPUT_ACTION_ROLL_RIGHT] != 0
										  : 0;
}

int XwaControllerMapping_TakeResume(void) {
	const bool resume = g_controller.resume;
	g_controller.resume = false;
	return resume;
}

void XwaControllerMapping_ReleaseCommands(void) {
	g_controller.read = g_controller.write = 0;
	g_controller.resume = false;
	memset(g_controller.holds, 0, sizeof g_controller.holds);
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		ControllerInstance* state = &g_controller.instances[i];
		for (size_t j = 0; j < XWA_CONTROLLER_BINDING_CAP; ++j) {
			state->dispatched[j] = false;
			state->armed[j] = !state->down[j];
		}
	}
}

uint32_t XwaControllerMapping_RumbleInstance(void) { return g_controller.rumble_instance; }

int XwaControllerMapping_ConsumeRumbleChange(void) {
	const bool changed = g_controller.rumble_changed;
	g_controller.rumble_changed = false;
	return changed;
}

void XwaControllerMapping_SetRumbleEnabled(int enabled) { g_controller.rumble_enabled = enabled != 0; }

int XwaControllerMapping_Rumble(uint16_t low_frequency_rumble, uint16_t high_frequency_rumble,
								uint32_t duration_ms) {
	const uint32_t instance = g_controller.rumble_instance;
	const bool stopping = (low_frequency_rumble == 0 && high_frequency_rumble == 0) || duration_ms == 0;
	if (!instance)
		return 0;
	if (!stopping && !g_controller.rumble_enabled)
		return 0;
	return Aeron_RumbleController(instance, low_frequency_rumble, high_frequency_rumble, duration_ms);
}
