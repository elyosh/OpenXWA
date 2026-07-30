/* DirectInput 5/3 compatibility shim for the system keyboard and mouse, backed by
 * the Aeron input snapshot. The recovered dinput.c calls the original DirectInput
 * COM methods (CreateDevice / SetDataFormat / SetProperty / Acquire / GetDeviceState
 * / GetDeviceData); this file services them from Aeron_InputSnapshot().
 *
 * Capture model: host pumps sample the Aeron snapshot into the devices once per
 * host frame (the shim's analog of DirectInput's asynchronous capture),
 * translating key_down into the immediate DIK state and key_pressed/key_released
 * into buffered make/break events. Device reads (GetDeviceState/GetDeviceData) are
 * pure reads of that captured state, so no buffered key edge is missed on frames the
 * fixed-step flight loop skips. A frame_id guard keeps the pump idempotent. */

#include "xwa_runtime/compat/directx/dinput.h"

#include "aeron/aeron.h"
#include "aeron/input.h"
#include "xwa_runtime/input/controller_mapping.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* GUID_SysKeyboard {6F1D2B61-D5A0-11CF-BFC7-444553540000},
 * GUID_SysMouse    {6F1D2B60-D5A0-11CF-BFC7-444553540000}. */
// GLOBAL: XWA 0x5AABA0
const DxGuid GUID_SysKeyboard = {
	0x6F1D2B61, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 }
};
// GLOBAL: XWA 0x5AAB90
const DxGuid GUID_SysMouse = {
	0x6F1D2B60, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 }
};

/* Content is unused by the shim (device type is fixed by the CreateDevice GUID); the
 * recovered code only needs a stable non-null pointer to hand to SetDataFormat. */
// GLOBAL: XWA 0x51CE00
const DIDATAFORMAT c_dfDIKeyboard = { (uint32_t)sizeof(DIDATAFORMAT), 8, 0x2, 256, 256, NULL };
// GLOBAL: XWA 0x51BDE0
const DIDATAFORMAT c_dfDIMouse = { (uint32_t)sizeof(DIDATAFORMAT), 8, 0x2, 16, 7, NULL };
/* Force-feedback joystick data format; the FF shim keys off the device, not this. */
const DIDATAFORMAT c_dfDIJoystick = { (uint32_t)sizeof(DIDATAFORMAT), 16, 0x1, 80, 44, NULL };

/* --- SDL(Aeron) scancode -> DIK scancode ---------------------------------- */

/* Maps an Aeron (SDL-scancode) key id to a DirectInput DIK scancode, or 0 if none. */
static unsigned int DInputShim_AeronKeyToDik(int key) {
	static const unsigned char letters[26] = {
		0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
		0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c,
	};
	static const unsigned char keypadDigits[10] = {
		0x52, 0x4f, 0x50, 0x51, 0x4b, 0x4c, 0x4d, 0x47, 0x48, 0x49
	};

	if (key >= AERON_KEY_A && key < AERON_KEY_A + 26) {
		return letters[key - AERON_KEY_A];
	}
	if (key >= AERON_KEY_1 && key < AERON_KEY_1 + 9) {
		return 0x02u + (unsigned int)(key - AERON_KEY_1);
	}
	if (key == AERON_KEY_1 + 9) {
		return 0x0bu;
	}
	if (key >= AERON_KEY_F1 && key < AERON_KEY_F1 + 10) {
		return 0x3bu + (unsigned int)(key - AERON_KEY_F1);
	}
	if (key == AERON_KEY_F1 + 10) {
		return 0x57u;
	}
	if (key == AERON_KEY_F1 + 11) {
		return 0x58u;
	}
	if (key >= AERON_KEY_KP_1 && key <= AERON_KEY_KP_9) {
		return keypadDigits[key - AERON_KEY_KP_1 + 1];
	}

	switch (key) {
		case AERON_KEY_ESCAPE:
			return 0x01u;
		case AERON_KEY_MINUS:
			return 0x0cu;
		case AERON_KEY_EQUALS:
			return 0x0du;
		case AERON_KEY_BACKSPACE:
			return 0x0eu;
		case AERON_KEY_TAB:
			return 0x0fu;
		case AERON_KEY_LEFTBRACKET:
			return 0x1au;
		case AERON_KEY_RIGHTBRACKET:
			return 0x1bu;
		case AERON_KEY_RETURN:
			return 0x1cu;
		case AERON_KEY_LCTRL:
			return 0x1du;
		case AERON_KEY_SEMICOLON:
			return 0x27u;
		case AERON_KEY_APOSTROPHE:
			return 0x28u;
		case AERON_KEY_GRAVE:
			return 0x29u;
		case AERON_KEY_LSHIFT:
			return 0x2au;
		case AERON_KEY_BACKSLASH:
			return 0x2bu;
		case AERON_KEY_COMMA:
			return 0x33u;
		case AERON_KEY_PERIOD:
			return 0x34u;
		case AERON_KEY_SLASH:
			return 0x35u;
		case AERON_KEY_RSHIFT:
			return 0x36u;
		case AERON_KEY_LALT:
			return 0x38u;
		case AERON_KEY_SPACE:
			return 0x39u;
		case AERON_KEY_CAPSLOCK:
			return 0x3au;
		case AERON_KEY_PRINTSCREEN:
			return 0xb7u;
		case AERON_KEY_SCROLLLOCK:
			return 0x46u;
		case AERON_KEY_PAUSE:
			return 0xc5u;
		case AERON_KEY_INSERT:
			return 0xd2u;
		case AERON_KEY_HOME:
			return 0xc7u;
		case AERON_KEY_PAGEUP:
			return 0xc9u;
		case AERON_KEY_DELETE:
			return 0xd3u;
		case AERON_KEY_END:
			return 0xcfu;
		case AERON_KEY_PAGEDOWN:
			return 0xd1u;
		case AERON_KEY_RIGHT:
			return 0xcdu;
		case AERON_KEY_LEFT:
			return 0xcbu;
		case AERON_KEY_DOWN:
			return 0xd0u;
		case AERON_KEY_UP:
			return 0xc8u;
		case AERON_KEY_KP_DIVIDE:
			return 0xb5u;
		case AERON_KEY_KP_MULTIPLY:
			return 0x37u;
		case AERON_KEY_KP_MINUS:
			return 0x4au;
		case AERON_KEY_KP_PLUS:
			return 0x4eu;
		case AERON_KEY_KP_ENTER:
			return 0x9cu;
		case AERON_KEY_KP_0:
			return keypadDigits[0];
		case AERON_KEY_KP_PERIOD:
			return 0x53u;
		case AERON_KEY_RCTRL:
			return 0x9du;
		case AERON_KEY_RALT:
			return 0xb8u;
		default:
			return 0;
	}
}

/* --- device shim ---------------------------------------------------------- */

#define DINPUT_SHIM_BUFFER_CAP 128 /* buffered-event ring; original DIPROP_BUFFERSIZE was 32 */

typedef enum DInputDeviceKind { DINPUT_DEV_KEYBOARD, DINPUT_DEV_MOUSE } DInputDeviceKind;

typedef struct DInputDeviceShim {
	const IDirectInputDeviceAVtbl* lpVtbl;
	int refcount;
	DInputDeviceKind kind;
	int acquired;
	uint64_t sampled_frame; /* frame_id of the last sample */

	/* keyboard */
	uint8_t dik[256];                                  /* immediate DIK state (0x80 = down) */
	DIDEVICEOBJECTDATA buffer[DINPUT_SHIM_BUFFER_CAP]; /* buffered make/break events */
	uint32_t buf_head;
	uint32_t buf_tail;
	uint32_t sequence;

	/* mouse (relative axes accumulate until read, buttons are level) */
	DIMOUSESTATE mouse;
	/* Fractional remainder of the float Aeron deltas carried across
	 * frames, so slow trackpad motion (sub-integer per-frame deltas)
	 * still accumulates into the integer mickeys. */
	float mouse_carry_x;
	float mouse_carry_y;
} DInputDeviceShim;

typedef struct DInputShim {
	const IDirectInputAVtbl* lpVtbl;
	int refcount;
} DInputShim;

/* The single keyboard/mouse device instances, tracked so the host input pumps can
 * capture input edges every host frame -- the recovered flight loop polls at a fixed
 * step slower than the host frame rate, so on-read sampling alone would miss the
 * one-frame key_pressed/key_released edges on frames the game does not poll. */
static DInputDeviceShim* g_shimKeyboardDevice;
static DInputDeviceShim* g_shimMouseDevice;

static void DInputShim_RingPush(DInputDeviceShim* d, uint32_t ofs, uint32_t data) {
	uint32_t next = (d->buf_head + 1u) % DINPUT_SHIM_BUFFER_CAP;
	if (next == d->buf_tail) {
		/* Full: drop the oldest event (matches DI dropping on buffer overflow). */
		d->buf_tail = (d->buf_tail + 1u) % DINPUT_SHIM_BUFFER_CAP;
	}
	d->buffer[d->buf_head].dwOfs = ofs;
	d->buffer[d->buf_head].dwData = data;
	d->buffer[d->buf_head].dwTimeStamp = 0;
	d->buffer[d->buf_head].dwSequence = d->sequence++;
	d->buf_head = next;
}

static uint32_t DInputShim_RingCount(const DInputDeviceShim* d) {
	return (d->buf_head + DINPUT_SHIM_BUFFER_CAP - d->buf_tail) % DINPUT_SHIM_BUFFER_CAP;
}

/* Sample the current Aeron snapshot into this device once per frame. */
static void DInputShim_Sample(DInputDeviceShim* d, int suppress_keyboard_presses) {
	const AeronInputSnapshot* in = Aeron_InputSnapshot();
	int k;
	int focus;

	if (!in || in->frame_id == d->sampled_frame) {
		return;
	}
	d->sampled_frame = in->frame_id;
	focus = in->has_focus;

	if (d->kind == DINPUT_DEV_KEYBOARD) {
		memset(d->dik, 0, sizeof(d->dik));
		for (k = 0; k < AERON_KEY_COUNT; ++k) {
			unsigned int dik = DInputShim_AeronKeyToDik(k);
			if (dik == 0) {
				continue;
			}
			if (focus && in->key_down[k]) {
				d->dik[dik] = 0x80;
			}
			/* Buffered make/break edges. A make is suppressed without focus; a break
			 * is always delivered so held keys release when focus is lost. */
			if (focus && in->key_pressed[k] && !suppress_keyboard_presses) {
				DInputShim_RingPush(d, dik, 0x80);
			}
			if (in->key_released[k]) {
				DInputShim_RingPush(d, dik, 0x00);
			}
		}
	} else {
		/* Only a captured pointer belongs to the flight mouse: while the capture is
		 * released to the OS, motion over the window must not steer the ship. */
		int active = focus && in->mouse.inside_content && Aeron_RelativeMouseMode();
		/* Relative axes accumulate until a GetDeviceState read consumes
		 * them. Aeron reports raw float pointer deltas (DirectInput
		 * mickeys were raw counts too); floor with a fractional carry so
		 * slow trackpad motion isn't truncated away. */
		d->mouse_carry_x += active ? in->mouse.relative_x : 0.0f;
		d->mouse_carry_y += active ? in->mouse.relative_y : 0.0f;
		{
			int ix = (int)floorf(d->mouse_carry_x);
			int iy = (int)floorf(d->mouse_carry_y);
			d->mouse_carry_x -= (float)ix;
			d->mouse_carry_y -= (float)iy;
			d->mouse.lX += ix;
			d->mouse.lY += iy;
		}
		d->mouse.lZ += active ? in->mouse.wheel_y : 0;
		{
			uint32_t b = active ? in->mouse.buttons : 0u;
			d->mouse.rgbButtons[0] = (b & AERON_MOUSE_BUTTON_LEFT) ? 0x80 : 0;
			d->mouse.rgbButtons[1] = (b & AERON_MOUSE_BUTTON_RIGHT) ? 0x80 : 0;
			d->mouse.rgbButtons[2] = (b & AERON_MOUSE_BUTTON_MIDDLE) ? 0x80 : 0;
			d->mouse.rgbButtons[3] = (b & AERON_MOUSE_BUTTON_X1) ? 0x80 : 0;
		}
	}
}

static HRESULT XWA_DXAPI DInputDevice_QueryInterface(IDirectInputDeviceA* self, DxRefIid iid, void** out) {
	(void)self;
	(void)iid;
	if (out) {
		*out = NULL;
	}
	return DX_E_NOTIMPL;
}

static uint32_t XWA_DXAPI DInputDevice_AddRef(IDirectInputDeviceA* self) {
	return (uint32_t)++((DInputDeviceShim*)self)->refcount;
}

static uint32_t XWA_DXAPI DInputDevice_Release(IDirectInputDeviceA* self) {
	DInputDeviceShim* d = (DInputDeviceShim*)self;
	if (--d->refcount > 0) {
		return (uint32_t)d->refcount;
	}
	if (g_shimKeyboardDevice == d) {
		g_shimKeyboardDevice = NULL;
	}
	if (g_shimMouseDevice == d) {
		g_shimMouseDevice = NULL;
	}
	free(d);
	return 0;
}

static HRESULT XWA_DXAPI DInputDevice_SetProperty(IDirectInputDeviceA* self, const DxGuid* prop,
												  const DIPROPHEADER* header) {
	(void)self;
	(void)prop;
	(void)header;
	/* DIPROP_BUFFERSIZE and friends: the shim's ring is fixed-size, so accept and
	 * ignore. */
	return DI_OK;
}

static HRESULT XWA_DXAPI DInputDevice_Acquire(IDirectInputDeviceA* self) {
	DInputDeviceShim* d = (DInputDeviceShim*)self;
	if (d->acquired) {
		return DI_NOEFFECT; /* S_FALSE, matching DirectInput */
	}
	d->acquired = 1;
	d->sampled_frame = 0; /* force a fresh sample after (re)acquire */
	return DI_OK;
}

static HRESULT XWA_DXAPI DInputDevice_Unacquire(IDirectInputDeviceA* self) {
	((DInputDeviceShim*)self)->acquired = 0;
	return DI_OK;
}

static HRESULT XWA_DXAPI DInputDevice_GetDeviceState(IDirectInputDeviceA* self, uint32_t cbData,
													 void* lpvData) {
	DInputDeviceShim* d = (DInputDeviceShim*)self;
	if (!d->acquired) {
		return DIERR_INPUTLOST;
	}
	if (!lpvData) {
		return DX_E_INVALIDARG;
	}

	if (d->kind == DINPUT_DEV_KEYBOARD) {
		uint32_t n = cbData < sizeof(d->dik) ? cbData : (uint32_t)sizeof(d->dik);
		memcpy(lpvData, d->dik, n);
		return DI_OK;
	}
	if (cbData >= sizeof(DIMOUSESTATE)) {
		memcpy(lpvData, &d->mouse, sizeof(DIMOUSESTATE));
		/* Relative axes report the delta since the previous read. */
		d->mouse.lX = 0;
		d->mouse.lY = 0;
		d->mouse.lZ = 0;
		return DI_OK;
	}
	return DX_E_INVALIDARG;
}

static HRESULT XWA_DXAPI DInputDevice_GetDeviceData(IDirectInputDeviceA* self, uint32_t cbObjectData,
													DIDEVICEOBJECTDATA* rgdod, uint32_t* pdwInOut,
													uint32_t dwFlags) {
	DInputDeviceShim* d = (DInputDeviceShim*)self;
	uint32_t want;
	uint32_t got = 0;

	if (!d->acquired) {
		return DIERR_INPUTLOST;
	}
	if (!pdwInOut || (cbObjectData != 0 && cbObjectData != (uint32_t)sizeof(DIDEVICEOBJECTDATA))) {
		return DX_E_INVALIDARG;
	}

	if (rgdod == NULL) {
		/* Flush: discard up to *pdwInOut buffered events (INFINITE = all) and report
		 * how many were removed. */
		uint32_t avail = DInputShim_RingCount(d);
		uint32_t flush = (*pdwInOut == 0xFFFFFFFFu || *pdwInOut > avail) ? avail : *pdwInOut;
		d->buf_tail = (d->buf_tail + flush) % DINPUT_SHIM_BUFFER_CAP;
		*pdwInOut = flush;
		return DI_OK;
	}

	want = *pdwInOut;
	while (got < want && d->buf_tail != d->buf_head) {
		rgdod[got] = d->buffer[d->buf_tail];
		if ((dwFlags & DIGDD_PEEK) == 0) {
			d->buf_tail = (d->buf_tail + 1u) % DINPUT_SHIM_BUFFER_CAP;
		}
		++got;
	}
	*pdwInOut = got;
	return DI_OK;
}

static HRESULT XWA_DXAPI DInputDevice_SetDataFormat(IDirectInputDeviceA* self, const DIDATAFORMAT* fmt) {
	(void)self;
	(void)fmt;
	return DI_OK;
}

static HRESULT XWA_DXAPI DInputDevice_SetCooperativeLevel(IDirectInputDeviceA* self, void* hwnd,
														  uint32_t flags) {
	(void)self;
	(void)hwnd;
	(void)flags;
	return DI_OK;
}

/* Methods the recovered keyboard/mouse code never calls. Provided (rather than left
 * NULL) so a stray call fails cleanly instead of jumping through a null slot. */
static HRESULT XWA_DXAPI DInputDevice_GetCapabilities(IDirectInputDeviceA* s, void* a) {
	(void)s;
	(void)a;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputDevice_EnumObjects(IDirectInputDeviceA* s, void* a, void* b, uint32_t c) {
	(void)s;
	(void)a;
	(void)b;
	(void)c;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputDevice_GetProperty(IDirectInputDeviceA* s, const DxGuid* a, DIPROPHEADER* b) {
	(void)s;
	(void)a;
	(void)b;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputDevice_SetEventNotification(IDirectInputDeviceA* s, void* a) {
	(void)s;
	(void)a;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputDevice_GetObjectInfo(IDirectInputDeviceA* s, void* a, uint32_t b, uint32_t c) {
	(void)s;
	(void)a;
	(void)b;
	(void)c;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputDevice_GetDeviceInfo(IDirectInputDeviceA* s, void* a) {
	(void)s;
	(void)a;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputDevice_RunControlPanel(IDirectInputDeviceA* s, void* a, uint32_t b) {
	(void)s;
	(void)a;
	(void)b;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputDevice_Initialize(IDirectInputDeviceA* s, void* a, uint32_t b, DxRefIid c) {
	(void)s;
	(void)a;
	(void)b;
	(void)c;
	return DI_OK;
}

static const IDirectInputDeviceAVtbl g_dinputDeviceVtbl = {
	.QueryInterface = DInputDevice_QueryInterface,
	.AddRef = DInputDevice_AddRef,
	.Release = DInputDevice_Release,
	.GetCapabilities = DInputDevice_GetCapabilities,
	.EnumObjects = DInputDevice_EnumObjects,
	.GetProperty = DInputDevice_GetProperty,
	.SetProperty = DInputDevice_SetProperty,
	.Acquire = DInputDevice_Acquire,
	.Unacquire = DInputDevice_Unacquire,
	.GetDeviceState = DInputDevice_GetDeviceState,
	.GetDeviceData = DInputDevice_GetDeviceData,
	.SetDataFormat = DInputDevice_SetDataFormat,
	.SetEventNotification = DInputDevice_SetEventNotification,
	.SetCooperativeLevel = DInputDevice_SetCooperativeLevel,
	.GetObjectInfo = DInputDevice_GetObjectInfo,
	.GetDeviceInfo = DInputDevice_GetDeviceInfo,
	.RunControlPanel = DInputDevice_RunControlPanel,
	.Initialize = DInputDevice_Initialize,
};

/* --- force feedback (IDirectInputDevice2 / IDirectInputEffect) ------------ *
 * The recovered force-feedback code enumerates a force-feedback device, creates
 * constant-force / periodic / spring effects on it, and starts/stops them. This shim
 * maps that onto Aeron two-motor rumble: constant and periodic effects become rumble
 * pulses whose amplitude is magnitude x effect-gain x device-gain; spring (condition)
 * effects have no rumble analog and are accepted as no-ops. Rumble is a single global
 * state per pad, so overlapping effects are last-writer-wins -- the inherent limit of
 * degrading directional/superimposed force feedback to a rumble motor pair. */

/* {13541C20/23/27-8E33-11D0-9AD0-00A0C9A06E35}: standard DirectInput effect GUIDs. */
const DxGuid GUID_ConstantForce = {
	0x13541C20, 0x8E33, 0x11D0, { 0x9A, 0xD0, 0x00, 0xA0, 0xC9, 0xA0, 0x6E, 0x35 }
};
const DxGuid GUID_Sine = { 0x13541C23, 0x8E33, 0x11D0, { 0x9A, 0xD0, 0x00, 0xA0, 0xC9, 0xA0, 0x6E, 0x35 } };
const DxGuid GUID_Spring = { 0x13541C27, 0x8E33, 0x11D0, { 0x9A, 0xD0, 0x00, 0xA0, 0xC9, 0xA0, 0x6E, 0x35 } };
/* {5944E683-C92E-11CF-BFC7-444553540000}: IID_IDirectInputDevice2A. */
// GLOBAL: XWA 0x5AAAB0
const DxGuid IID_IDirectInputDevice2A = {
	0x5944E683, 0xC92E, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 }
};
/* Synthetic instance GUID the shim reports for its single force-feedback gamepad. */
static const DxGuid GUID_AeronFFGamepad = {
	0xAE401001, 0x0000, 0x0000, { 0x41, 0x45, 0x52, 0x4F, 0x4E, 0x46, 0x46 }
};

/* Force-feedback command flags (SendForceFeedbackCommand); only CONTINUE keeps rumble. */
#define DISFFC_CONTINUE 0x00000008u

typedef enum DInputEffectKind {
	DINPUT_FX_CONSTANT,
	DINPUT_FX_PERIODIC,
	DINPUT_FX_CONDITION,
	DINPUT_FX_OTHER
} DInputEffectKind;

typedef struct DInputFFDeviceShim {
	const IDirectInputDevice2AVtbl* lpVtbl;
	int refcount;
	int acquired;
	uint32_t controller_instance_id; /* Selected Aeron controller, or zero. */
	uint32_t device_gain;            /* DIPROP_FFGAIN, 0..10000 */
	struct DInputEffectShim* active; /* effect currently driving rumble */
} DInputFFDeviceShim;

typedef struct DInputEffectShim {
	const IDirectInputEffectVtbl* lpVtbl;
	int refcount;
	DInputFFDeviceShim* device;
	DInputEffectKind kind;
	uint32_t magnitude; /* 0..10000 base magnitude */
	uint32_t gain;      /* DIEFFECT.dwGain, 0..10000 */
	uint32_t duration;  /* microseconds, or DI_INFINITE */
} DInputEffectShim;

/* Extract the effect's base magnitude (0..10000) from its type-specific parameters. */
static void DInputEffect_ReadMagnitude(DInputEffectShim* fx, const DIEFFECT* eff) {
	if (!eff || !eff->lpvTypeSpecificParams) {
		return;
	}
	if (fx->kind == DINPUT_FX_CONSTANT && eff->cbTypeSpecificParams >= sizeof(DICONSTANTFORCE)) {
		int32_t m = ((const DICONSTANTFORCE*)eff->lpvTypeSpecificParams)->lMagnitude;
		fx->magnitude = (uint32_t)(m < 0 ? -m : m);
	} else if (fx->kind == DINPUT_FX_PERIODIC && eff->cbTypeSpecificParams >= sizeof(DIPERIODIC)) {
		fx->magnitude = ((const DIPERIODIC*)eff->lpvTypeSpecificParams)->dwMagnitude;
	}
	/* Condition (spring) effects carry no scalar magnitude and do not rumble. */
}

/* Start rumble for this effect, scaling magnitude by effect gain and device gain. */
static void DInputEffect_Trigger(DInputEffectShim* fx) {
	DInputFFDeviceShim* dev = fx->device;
	uint32_t amplitude;
	uint32_t motor;
	uint32_t duration_ms;

	if (!dev || dev->controller_instance_id == 0 ||
		dev->controller_instance_id != XwaControllerMapping_SelectedInstanceId() ||
		!XwaControllerMapping_SelectedHasRumble() || fx->kind == DINPUT_FX_CONDITION) {
		return;
	}
	amplitude = fx->magnitude;
	amplitude = amplitude * fx->gain / 10000u;
	amplitude = amplitude * dev->device_gain / 10000u;
	if (amplitude > 10000u) {
		amplitude = 10000u;
	}
	motor = amplitude * 65535u / 10000u;
	duration_ms = (fx->duration == DI_INFINITE) ? DI_INFINITE : fx->duration / 1000u;
	if (duration_ms == 0) {
		duration_ms = 1;
	}
	XwaControllerMapping_Rumble((uint16_t)motor, (uint16_t)motor, duration_ms);
	dev->active = fx;
}

static HRESULT XWA_DXAPI DInputEffect_QueryInterface(IDirectInputEffect* self, DxRefIid iid, void** out) {
	(void)self;
	(void)iid;
	if (out) {
		*out = NULL;
	}
	return DX_E_NOTIMPL;
}
static uint32_t XWA_DXAPI DInputEffect_AddRef(IDirectInputEffect* self) {
	return (uint32_t)++((DInputEffectShim*)self)->refcount;
}
static uint32_t XWA_DXAPI DInputEffect_Release(IDirectInputEffect* self) {
	DInputEffectShim* fx = (DInputEffectShim*)self;
	if (--fx->refcount > 0) {
		return (uint32_t)fx->refcount;
	}
	if (fx->device && fx->device->active == fx) {
		fx->device->active = NULL;
	}
	free(fx);
	return 0;
}
static HRESULT XWA_DXAPI DInputEffect_SetParameters(IDirectInputEffect* self, const DIEFFECT* eff,
													uint32_t flags) {
	DInputEffectShim* fx = (DInputEffectShim*)self;
	if (eff) {
		if (flags & DIEP_GAIN) {
			fx->gain = eff->dwGain;
		}
		if (flags & DIEP_DURATION) {
			fx->duration = eff->dwDuration;
		}
		if (flags & DIEP_TYPESPECIFICPARAMS) {
			DInputEffect_ReadMagnitude(fx, eff);
		}
		/* Direction is dropped: two-motor rumble has no directional component. */
	}
	if (flags & DIEP_START) {
		DInputEffect_Trigger(fx);
	}
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputEffect_Start(IDirectInputEffect* self, uint32_t iterations, uint32_t flags) {
	(void)iterations;
	(void)flags;
	DInputEffect_Trigger((DInputEffectShim*)self);
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputEffect_Stop(IDirectInputEffect* self) {
	DInputEffectShim* fx = (DInputEffectShim*)self;
	if (fx->device && fx->device->controller_instance_id != 0 && fx->device->active == fx) {
		if (fx->device->controller_instance_id == XwaControllerMapping_SelectedInstanceId()) {
			XwaControllerMapping_Rumble(0, 0, 0);
		}
		fx->device->active = NULL;
	}
	return DI_OK;
}
/* Effect methods the recovered code never calls (typed stubs, no NULL slots). */
static HRESULT XWA_DXAPI DInputEffect_Initialize(IDirectInputEffect* s, void* a, uint32_t b, DxRefIid c) {
	(void)s;
	(void)a;
	(void)b;
	(void)c;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputEffect_GetEffectGuid(IDirectInputEffect* s, DxGuid* a) {
	(void)s;
	(void)a;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputEffect_GetParameters(IDirectInputEffect* s, DIEFFECT* a, uint32_t b) {
	(void)s;
	(void)a;
	(void)b;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputEffect_GetEffectStatus(IDirectInputEffect* s, uint32_t* a) {
	(void)s;
	if (a) {
		*a = 0;
	}
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputEffect_Download(IDirectInputEffect* s) {
	(void)s;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputEffect_Unload(IDirectInputEffect* s) {
	(void)s;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputEffect_Escape(IDirectInputEffect* s, void* a) {
	(void)s;
	(void)a;
	return DX_E_NOTIMPL;
}

static const IDirectInputEffectVtbl g_dinputEffectVtbl = {
	.QueryInterface = DInputEffect_QueryInterface,
	.AddRef = DInputEffect_AddRef,
	.Release = DInputEffect_Release,
	.Initialize = DInputEffect_Initialize,
	.GetEffectGuid = DInputEffect_GetEffectGuid,
	.GetParameters = DInputEffect_GetParameters,
	.SetParameters = DInputEffect_SetParameters,
	.Start = DInputEffect_Start,
	.Stop = DInputEffect_Stop,
	.GetEffectStatus = DInputEffect_GetEffectStatus,
	.Download = DInputEffect_Download,
	.Unload = DInputEffect_Unload,
	.Escape = DInputEffect_Escape,
};

/* --- IDirectInputDevice2 (force-feedback device) -------------------------- */

static HRESULT XWA_DXAPI DInputFFDevice_QueryInterface(IDirectInputDevice2A* self, DxRefIid iid, void** out) {
	if (!out) {
		return DX_E_INVALIDARG;
	}
	*out = NULL;
	if (iid && DxGuidEqual(iid, &IID_IDirectInputDevice2A)) {
		++((DInputFFDeviceShim*)self)->refcount;
		*out = self;
		return DI_OK;
	}
	return DX_E_NOTIMPL;
}
static uint32_t XWA_DXAPI DInputFFDevice_AddRef(IDirectInputDevice2A* self) {
	return (uint32_t)++((DInputFFDeviceShim*)self)->refcount;
}
static uint32_t XWA_DXAPI DInputFFDevice_Release(IDirectInputDevice2A* self) {
	DInputFFDeviceShim* dev = (DInputFFDeviceShim*)self;
	if (--dev->refcount > 0) {
		return (uint32_t)dev->refcount;
	}
	free(dev);
	return 0;
}
static HRESULT XWA_DXAPI DInputFFDevice_SetProperty(IDirectInputDevice2A* self, const DxGuid* prop,
													const DIPROPHEADER* header) {
	DInputFFDeviceShim* dev = (DInputFFDeviceShim*)self;
	/* DIPROP_FFGAIN carries the device master gain; DIPROP_AUTOCENTER has no rumble
	 * analog. Both arrive as a MAKEDIPROP pseudo-GUID pointer. */
	if (prop == DINPUT_DIPROP_FFGAIN && header) {
		dev->device_gain = ((const DIPROPDWORD*)header)->dwData;
	}
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_Acquire(IDirectInputDevice2A* self) {
	((DInputFFDeviceShim*)self)->acquired = 1;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_Unacquire(IDirectInputDevice2A* self) {
	((DInputFFDeviceShim*)self)->acquired = 0;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_SetDataFormat(IDirectInputDevice2A* self, const DIDATAFORMAT* fmt) {
	(void)self;
	(void)fmt;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_SetCooperativeLevel(IDirectInputDevice2A* self, void* hwnd,
															uint32_t flags) {
	(void)self;
	(void)hwnd;
	(void)flags;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_CreateEffect(IDirectInputDevice2A* self, DxRefIid guid,
													 const DIEFFECT* eff, IDirectInputEffect** out,
													 void* outer) {
	DInputFFDeviceShim* dev = (DInputFFDeviceShim*)self;
	DInputEffectShim* fx;
	(void)outer;

	if (!out) {
		return DX_E_INVALIDARG;
	}
	*out = NULL;
	fx = (DInputEffectShim*)calloc(1, sizeof(*fx));
	if (!fx) {
		return DX_E_FAIL;
	}
	fx->lpVtbl = &g_dinputEffectVtbl;
	fx->refcount = 1;
	fx->device = dev;
	fx->kind = DINPUT_FX_OTHER;
	fx->gain = 10000;
	fx->duration = DI_INFINITE;
	if (guid) {
		if (DxGuidEqual(guid, &GUID_ConstantForce)) {
			fx->kind = DINPUT_FX_CONSTANT;
		} else if (DxGuidEqual(guid, &GUID_Sine)) {
			fx->kind = DINPUT_FX_PERIODIC;
		} else if (DxGuidEqual(guid, &GUID_Spring)) {
			fx->kind = DINPUT_FX_CONDITION;
		}
	}
	if (eff) {
		fx->gain = eff->dwGain;
		fx->duration = eff->dwDuration;
		DInputEffect_ReadMagnitude(fx, eff);
	}
	*out = (IDirectInputEffect*)fx;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_EnumEffects(IDirectInputDevice2A* self, LPDIENUMEFFECTSCALLBACKA cb,
													void* ctx, uint32_t flags) {
	(void)self;
	(void)cb;
	(void)ctx;
	(void)flags;
	/* The recovered code passes a no-op callback and ignores the result. */
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_GetEffectInfo(IDirectInputDevice2A* self, DIEFFECTINFOA* info,
													  DxRefIid guid) {
	uint32_t size;
	(void)self;

	if (!info) {
		return DX_E_INVALIDARG;
	}
	size = info->dwSize;
	memset(info, 0, sizeof(*info));
	info->dwSize = size;
	if (guid) {
		info->guid = *guid;
	}
	/* Report gain/direction/duration/type-specific params as dynamically settable so the
	 * recovered init treats gain changes as supported. */
	info->dwDynamicParams = DIEP_DURATION | DIEP_GAIN | DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS;
	info->dwStaticParams = info->dwDynamicParams;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_SendForceFeedbackCommand(IDirectInputDevice2A* self,
																 uint32_t command) {
	DInputFFDeviceShim* dev = (DInputFFDeviceShim*)self;
	/* Any command other than CONTINUE (reset/stop/pause/actuators-off) silences rumble. */
	if (command != DISFFC_CONTINUE && dev->controller_instance_id != 0) {
		if (dev->controller_instance_id == XwaControllerMapping_SelectedInstanceId()) {
			XwaControllerMapping_Rumble(0, 0, 0);
		}
		dev->active = NULL;
	}
	return DI_OK;
}
/* Device methods the recovered code never calls (typed stubs, no NULL slots). */
static HRESULT XWA_DXAPI DInputFFDevice_GetCapabilities(IDirectInputDevice2A* s, void* a) {
	(void)s;
	(void)a;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputFFDevice_EnumObjects(IDirectInputDevice2A* s, void* a, void* b, uint32_t c) {
	(void)s;
	(void)a;
	(void)b;
	(void)c;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputFFDevice_GetProperty(IDirectInputDevice2A* s, const DxGuid* a,
													DIPROPHEADER* b) {
	(void)s;
	(void)a;
	(void)b;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputFFDevice_GetDeviceState(IDirectInputDevice2A* s, uint32_t a, void* b) {
	(void)s;
	(void)a;
	(void)b;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputFFDevice_GetDeviceData(IDirectInputDevice2A* s, uint32_t a,
													  DIDEVICEOBJECTDATA* b, uint32_t* c, uint32_t d) {
	(void)s;
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputFFDevice_SetEventNotification(IDirectInputDevice2A* s, void* a) {
	(void)s;
	(void)a;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_GetObjectInfo(IDirectInputDevice2A* s, void* a, uint32_t b,
													  uint32_t c) {
	(void)s;
	(void)a;
	(void)b;
	(void)c;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputFFDevice_GetDeviceInfo(IDirectInputDevice2A* s, void* a) {
	(void)s;
	(void)a;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputFFDevice_RunControlPanel(IDirectInputDevice2A* s, void* a, uint32_t b) {
	(void)s;
	(void)a;
	(void)b;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputFFDevice_Initialize(IDirectInputDevice2A* s, void* a, uint32_t b, DxRefIid c) {
	(void)s;
	(void)a;
	(void)b;
	(void)c;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_GetForceFeedbackState(IDirectInputDevice2A* s, uint32_t* a) {
	(void)s;
	if (a) {
		*a = 0;
	}
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_EnumCreatedEffectObjects(IDirectInputDevice2A* s, void* a, void* b,
																 uint32_t c) {
	(void)s;
	(void)a;
	(void)b;
	(void)c;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputFFDevice_Escape(IDirectInputDevice2A* s, void* a) {
	(void)s;
	(void)a;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInputFFDevice_Poll(IDirectInputDevice2A* s) {
	(void)s;
	return DI_OK;
}
static HRESULT XWA_DXAPI DInputFFDevice_SendDeviceData(IDirectInputDevice2A* s, uint32_t a, const void* b,
													   uint32_t* c, uint32_t d) {
	(void)s;
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	return DX_E_NOTIMPL;
}

static const IDirectInputDevice2AVtbl g_dinputFFDeviceVtbl = {
	.QueryInterface = DInputFFDevice_QueryInterface,
	.AddRef = DInputFFDevice_AddRef,
	.Release = DInputFFDevice_Release,
	.GetCapabilities = DInputFFDevice_GetCapabilities,
	.EnumObjects = DInputFFDevice_EnumObjects,
	.GetProperty = DInputFFDevice_GetProperty,
	.SetProperty = DInputFFDevice_SetProperty,
	.Acquire = DInputFFDevice_Acquire,
	.Unacquire = DInputFFDevice_Unacquire,
	.GetDeviceState = DInputFFDevice_GetDeviceState,
	.GetDeviceData = DInputFFDevice_GetDeviceData,
	.SetDataFormat = DInputFFDevice_SetDataFormat,
	.SetEventNotification = DInputFFDevice_SetEventNotification,
	.SetCooperativeLevel = DInputFFDevice_SetCooperativeLevel,
	.GetObjectInfo = DInputFFDevice_GetObjectInfo,
	.GetDeviceInfo = DInputFFDevice_GetDeviceInfo,
	.RunControlPanel = DInputFFDevice_RunControlPanel,
	.Initialize = DInputFFDevice_Initialize,
	.CreateEffect = DInputFFDevice_CreateEffect,
	.EnumEffects = DInputFFDevice_EnumEffects,
	.GetEffectInfo = DInputFFDevice_GetEffectInfo,
	.GetForceFeedbackState = DInputFFDevice_GetForceFeedbackState,
	.SendForceFeedbackCommand = DInputFFDevice_SendForceFeedbackCommand,
	.EnumCreatedEffectObjects = DInputFFDevice_EnumCreatedEffectObjects,
	.Escape = DInputFFDevice_Escape,
	.Poll = DInputFFDevice_Poll,
	.SendDeviceData = DInputFFDevice_SendDeviceData,
};

static HRESULT DInput_CreateFFDevice(IDirectInputDeviceA** out) {
	DInputFFDeviceShim* dev = (DInputFFDeviceShim*)calloc(1, sizeof(*dev));
	if (!dev) {
		return DX_E_FAIL;
	}
	dev->lpVtbl = &g_dinputFFDeviceVtbl;
	dev->refcount = 1;
	dev->controller_instance_id = XwaControllerMapping_SelectedInstanceId();
	dev->device_gain = 10000;
	/* IDirectInputDevice2A shares the IDirectInputDeviceA prefix; the recovered enum
	 * callback QueryInterface's this to IID_IDirectInputDevice2A before use. */
	*out = (IDirectInputDeviceA*)dev;
	return DI_OK;
}

/* --- IDirectInput --------------------------------------------------------- */

static HRESULT XWA_DXAPI DInput_QueryInterface(IDirectInputA* self, DxRefIid iid, void** out) {
	(void)self;
	(void)iid;
	if (out) {
		*out = NULL;
	}
	return DX_E_NOTIMPL;
}

static uint32_t XWA_DXAPI DInput_AddRef(IDirectInputA* self) {
	return (uint32_t)++((DInputShim*)self)->refcount;
}

static uint32_t XWA_DXAPI DInput_Release(IDirectInputA* self) {
	DInputShim* s = (DInputShim*)self;
	if (--s->refcount > 0) {
		return (uint32_t)s->refcount;
	}
	free(s);
	return 0;
}

static HRESULT XWA_DXAPI DInput_CreateDevice(IDirectInputA* self, const DxGuid* guid,
											 IDirectInputDeviceA** out, void* outer) {
	DInputDeviceShim* d;
	DInputDeviceKind kind;
	(void)self;
	(void)outer;

	if (!out) {
		return DX_E_INVALIDARG;
	}
	*out = NULL;
	if (guid && DxGuidEqual(guid, &GUID_SysKeyboard)) {
		kind = DINPUT_DEV_KEYBOARD;
	} else if (guid && DxGuidEqual(guid, &GUID_SysMouse)) {
		kind = DINPUT_DEV_MOUSE;
	} else if (guid && DxGuidEqual(guid, &GUID_AeronFFGamepad)) {
		/* Force-feedback device: separate IDirectInputDevice2 object over gamepad rumble. */
		return DInput_CreateFFDevice(out);
	} else {
		return DX_E_FAIL; /* other devices are not represented by the shim */
	}

	d = (DInputDeviceShim*)calloc(1, sizeof(*d));
	if (!d) {
		return DX_E_FAIL;
	}
	d->lpVtbl = &g_dinputDeviceVtbl;
	d->refcount = 1;
	d->kind = kind;
	if (kind == DINPUT_DEV_KEYBOARD) {
		g_shimKeyboardDevice = d;
	} else {
		g_shimMouseDevice = d;
	}
	/* Capture the current frame immediately so the device reports valid state on the
	 * first read (DInput_Init reads modifier state right after creation, before the
	 * next per-frame pump). Subsequent frames are captured by DInputShim_Pump. */
	DInputShim_Sample(d, 0);
	*out = (IDirectInputDeviceA*)d;
	return DI_OK;
}

/* Capture the current host frame's input into the keyboard/mouse devices. Called once
 * per host frame so buffered key edges accumulate regardless of the game's polling
 * cadence; the per-frame frame_id guard makes it idempotent with on-read sampling. */
void DInputShim_Pump(void) {
	if (g_shimKeyboardDevice) {
		DInputShim_Sample(g_shimKeyboardDevice, 0);
	}
	if (g_shimMouseDevice) {
		DInputShim_Sample(g_shimMouseDevice, 0);
	}
}

void DInputShim_PumpSuppressed(void) {
	if (g_shimKeyboardDevice) {
		DInputShim_Sample(g_shimKeyboardDevice, 1);
	}
}

static HRESULT XWA_DXAPI DInput_EnumDevices(IDirectInputA* s, uint32_t devType,
											LPDIENUMDEVICESCALLBACKA callback, void* ctx, uint32_t flags) {
	(void)s;
	(void)devType;
	/* Force-feedback enumeration: present the selected rumble-capable controller as a single
	 * force-feedback joystick so the recovered init can create and QueryInterface it. */
	if (callback && (flags & DIEDFL_FORCEFEEDBACK)) {
		if (XwaControllerMapping_SelectedHasRumble()) {
			DIDEVICEINSTANCEA inst;
			memset(&inst, 0, sizeof(inst));
			inst.dwSize = (uint32_t)sizeof(inst);
			inst.guidInstance = GUID_AeronFFGamepad;
			inst.guidProduct = GUID_AeronFFGamepad;
			inst.dwDevType = DIDEVTYPE_JOYSTICK;
			callback(&inst, ctx);
		}
	}
	return DI_OK;
}
static HRESULT XWA_DXAPI DInput_GetDeviceStatus(IDirectInputA* s, const DxGuid* a) {
	(void)s;
	(void)a;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInput_RunControlPanel(IDirectInputA* s, void* a, uint32_t b) {
	(void)s;
	(void)a;
	(void)b;
	return DX_E_NOTIMPL;
}
static HRESULT XWA_DXAPI DInput_Initialize(IDirectInputA* s, void* a, uint32_t b) {
	(void)s;
	(void)a;
	(void)b;
	return DI_OK;
}

static const IDirectInputAVtbl g_dinputVtbl = {
	.QueryInterface = DInput_QueryInterface,
	.AddRef = DInput_AddRef,
	.Release = DInput_Release,
	.CreateDevice = DInput_CreateDevice,
	.EnumDevices = DInput_EnumDevices,
	.GetDeviceStatus = DInput_GetDeviceStatus,
	.RunControlPanel = DInput_RunControlPanel,
	.Initialize = DInput_Initialize,
};

HRESULT XWA_DXAPI DirectInputCreateA(void* hinst, uint32_t version, IDirectInputA** out, void* outer) {
	DInputShim* s;
	(void)hinst;
	(void)version;
	(void)outer;

	if (!out) {
		return DX_E_INVALIDARG;
	}
	*out = NULL;
	s = (DInputShim*)calloc(1, sizeof(*s));
	if (!s) {
		return DX_E_FAIL;
	}
	s->lpVtbl = &g_dinputVtbl;
	s->refcount = 1;
	*out = (IDirectInputA*)s;
	return DI_OK;
}
