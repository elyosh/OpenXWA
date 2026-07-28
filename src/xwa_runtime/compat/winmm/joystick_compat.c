/* WinMM joystick API shim backed by Aeron gamepads.
 *
 * Serves the recovered joystick code's original joyGetNumDevs / joyGetDevCapsA /
 * joyGetPosEx calls. joyGetPosEx maps an Aeron gamepad into a raw JOYINFOEX (0..65535
 * axes, button bitmask) using the YAML device-layout config (which SDL axis drives
 * which WinMM axis slot, inversion, and an optional deadzone override); the recovered
 * game code then does its original calibration/scaling. The YAML layout lives here --
 * this is the device-abstraction layer -- while axis->function assignment stays in the
 * game. */

#define XWA_WINMM_COMPAT_IMPLEMENTATION
#include "xwa_runtime/compat/winmm/joystick.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/input.h"

#include <string.h>

enum {
	WINMM_JOY_AXIS_X = 0,
	WINMM_JOY_AXIS_Y,
	WINMM_JOY_AXIS_Z,
	WINMM_JOY_AXIS_R,
	WINMM_JOY_AXIS_NONE = -1,
	WINMM_JOY_MAX_BUTTONS = 20,
	WINMM_JOY_AXIS_RANGE = 65535, /* raw WinMM axis span reported to the game */
	WINMM_JOY_AXIS_CENTER = 32768,
};

typedef struct WinmmJoyAxisBinding {
	int target; /* WINMM_JOY_AXIS_* */
	int invert;
} WinmmJoyAxisBinding;

typedef struct WinmmJoyConfig {
	int loaded;
	int device;
	double deadzone;
	WinmmJoyAxisBinding axes[AERON_GAMEPAD_AXIS_COUNT];
	int buttons[AERON_GAMEPAD_BUTTON_COUNT]; /* XWA button number 1..20, 0 = unbound */
} WinmmJoyConfig;

static WinmmJoyConfig g_winmmJoyConfig;
static WinmmJoystickTraceSample g_winmmJoystickTraceSample;
static int g_winmmJoystickTraceValid;

/* --- YAML device-layout config (relocated from the joystick bridge) ------- */

static int Winmm_StringEquals(const char* a, const char* b) {
	if (!a || !b) {
		return 0;
	}
	while (*a && *b) {
		char ca = *a++;
		char cb = *b++;
		if (ca >= 'A' && ca <= 'Z') {
			ca = (char)(ca - 'A' + 'a');
		}
		if (cb >= 'A' && cb <= 'Z') {
			cb = (char)(cb - 'A' + 'a');
		}
		if (ca == '_' || ca == '-' || ca == ' ') {
			--b;
			continue;
		}
		if (cb == '_' || cb == '-' || cb == ' ') {
			--a;
			continue;
		}
		if (ca != cb) {
			return 0;
		}
	}
	while (*a == '_' || *a == '-' || *a == ' ') {
		++a;
	}
	while (*b == '_' || *b == '-' || *b == ' ') {
		++b;
	}
	return *a == '\0' && *b == '\0';
}

static int Winmm_AxisTargetFromName(const char* name) {
	if (Winmm_StringEquals(name, "x")) {
		return WINMM_JOY_AXIS_X;
	}
	if (Winmm_StringEquals(name, "y")) {
		return WINMM_JOY_AXIS_Y;
	}
	if (Winmm_StringEquals(name, "z")) {
		return WINMM_JOY_AXIS_Z;
	}
	if (Winmm_StringEquals(name, "r")) {
		return WINMM_JOY_AXIS_R;
	}
	return WINMM_JOY_AXIS_NONE;
}

static void Winmm_JoyConfigSetDefaults(void) {
	int i;

	memset(&g_winmmJoyConfig, 0, sizeof(g_winmmJoyConfig));
	g_winmmJoyConfig.device = 0;
	g_winmmJoyConfig.deadzone = 0.08;
	for (i = 0; i < AERON_GAMEPAD_AXIS_COUNT; ++i) {
		g_winmmJoyConfig.axes[i].target = WINMM_JOY_AXIS_NONE;
	}

	g_winmmJoyConfig.axes[AERON_GAMEPAD_AXIS_LEFTX].target = WINMM_JOY_AXIS_X;
	g_winmmJoyConfig.axes[AERON_GAMEPAD_AXIS_LEFTY].target = WINMM_JOY_AXIS_Y;
	g_winmmJoyConfig.axes[AERON_GAMEPAD_AXIS_LEFTY].invert = 1;
	g_winmmJoyConfig.axes[AERON_GAMEPAD_AXIS_RIGHTX].target = WINMM_JOY_AXIS_R;
	g_winmmJoyConfig.axes[AERON_GAMEPAD_AXIS_RIGHT_TRIGGER].target = WINMM_JOY_AXIS_Z;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_SOUTH] = 1;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_EAST] = 2;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_WEST] = 3;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_NORTH] = 4;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_LEFT_SHOULDER] = 5;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_RIGHT_SHOULDER] = 6;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_BACK] = 7;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_START] = 8;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_DPAD_UP] = 17;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_DPAD_RIGHT] = 18;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_DPAD_DOWN] = 19;
	g_winmmJoyConfig.buttons[AERON_GAMEPAD_BUTTON_DPAD_LEFT] = 20;
}

static void Winmm_JoyParseAxes(const AeronConfigNode* axesNode) {
	size_t i;
	size_t count;

	if (AeronConfigNode_Type(axesNode) != AERON_CONFIG_MAP) {
		return;
	}
	count = AeronConfigNode_MapCount(axesNode);
	for (i = 0; i < count; ++i) {
		const char* key = AeronConfigNode_MapKeyAt(axesNode, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(axesNode, i);
		AeronGamepadAxis sourceAxis = Aeron_GamepadAxisFromName(key);
		const char* targetName;
		int targetAxis;

		if (sourceAxis >= AERON_GAMEPAD_AXIS_COUNT) {
			Aeron_LogWarn("xwa.input", "Ignoring unknown gamepad axis '%s' in input.yaml", key ? key : "");
			continue;
		}
		targetName = AeronConfigNode_String(AeronConfigNode_MapGet(value, "target"), NULL);
		targetAxis = Winmm_AxisTargetFromName(targetName);
		if (targetAxis == WINMM_JOY_AXIS_NONE) {
			Aeron_LogWarn("xwa.input", "Ignoring axis '%s' with unknown XWA target '%s'", key ? key : "",
					  targetName ? targetName : "");
			continue;
		}
		g_winmmJoyConfig.axes[sourceAxis].target = targetAxis;
		g_winmmJoyConfig.axes[sourceAxis].invert = AeronConfigNode_Bool(
			AeronConfigNode_MapGet(value, "invert"), g_winmmJoyConfig.axes[sourceAxis].invert);
	}
}

static void Winmm_JoyParseButtons(const AeronConfigNode* buttonsNode) {
	size_t i;
	size_t count;

	if (AeronConfigNode_Type(buttonsNode) != AERON_CONFIG_MAP) {
		return;
	}
	count = AeronConfigNode_MapCount(buttonsNode);
	for (i = 0; i < count; ++i) {
		const char* key = AeronConfigNode_MapKeyAt(buttonsNode, i);
		AeronGamepadButton button = Aeron_GamepadButtonFromName(key);
		int64_t targetButton;

		if (button >= AERON_GAMEPAD_BUTTON_COUNT) {
			Aeron_LogWarn("xwa.input", "Ignoring unknown gamepad button '%s' in input.yaml", key ? key : "");
			continue;
		}
		targetButton = AeronConfigNode_Int(AeronConfigNode_MapValueAt(buttonsNode, i), 0);
		if (targetButton < 0 || targetButton > WINMM_JOY_MAX_BUTTONS) {
			Aeron_LogWarn("xwa.input", "Ignoring button '%s' with out-of-range XWA button %lld", key ? key : "",
					  (long long)targetButton);
			continue;
		}
		g_winmmJoyConfig.buttons[button] = (int)targetButton;
	}
}

static AeronConfigFile* Winmm_JoyLoadYamlIfExists(const char* path) {
	AeronConfigFile* config;
	AeronVfs* vfs = Aeron_GetVfs();

	if (AeronVfs_Exists(vfs, AERON_VFS_ROOT_USER, path) &&
		AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_USER, path, &config)) {
		Aeron_LogInfo("xwa.input", "Loaded joystick mapping from user config '%s'", path);
		return config;
	}
	if (AeronVfs_Exists(vfs, AERON_VFS_ROOT_ASSET, path) &&
		AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_ASSET, path, &config)) {
		Aeron_LogInfo("xwa.input", "Loaded joystick mapping from asset config '%s'", path);
		return config;
	}
	return NULL;
}

static void Winmm_JoyLoadConfig(void) {
	AeronConfigFile* config;
	const AeronConfigNode* joystick;

	if (g_winmmJoyConfig.loaded) {
		return;
	}
	Winmm_JoyConfigSetDefaults();
	config = Winmm_JoyLoadYamlIfExists("input.yaml");
	if (config) {
		joystick =
			AeronConfigNode_MapGet(AeronConfigNode_MapGet(AeronConfigFile_Root(config), "xwa"), "joystick");
		if (AeronConfigNode_Type(joystick) == AERON_CONFIG_MAP) {
			g_winmmJoyConfig.device =
				(int)AeronConfigNode_Int(AeronConfigNode_MapGet(joystick, "device"), g_winmmJoyConfig.device);
			g_winmmJoyConfig.deadzone = AeronConfigNode_Float(AeronConfigNode_MapGet(joystick, "deadzone"),
															  g_winmmJoyConfig.deadzone);
			if (g_winmmJoyConfig.deadzone < 0.0) {
				g_winmmJoyConfig.deadzone = 0.0;
			}
			if (g_winmmJoyConfig.deadzone > 1.0) {
				g_winmmJoyConfig.deadzone = 1.0;
			}
			Winmm_JoyParseAxes(AeronConfigNode_MapGet(joystick, "axes"));
			Winmm_JoyParseButtons(AeronConfigNode_MapGet(joystick, "buttons"));
		}
		AeronConfigFile_Destroy(config);
	}
	g_winmmJoyConfig.loaded = 1;
}

/* --- Aeron gamepad -> WinMM ----------------------------------------------- */

/* Returns the connected Aeron gamepad backing WinMM device uJoyID, or NULL. */
static const AeronGamepadSnapshot* Winmm_JoyGamepad(uint32_t uJoyID) {
	const AeronInputSnapshot* input = Aeron_InputSnapshot();

	if (!input || !input->has_focus || uJoyID >= AERON_GAMEPAD_MAX) {
		return NULL;
	}
	if (!input->gamepads[uJoyID].connected) {
		return NULL;
	}
	return &input->gamepads[uJoyID];
}

/* Converts a stick axis (SDL int16, centered) to a raw WinMM axis value, applying the
 * YAML inversion and deadzone (center-snap, so a configured deadzone overrides -- the
 * effective deadzone is the larger of this and the game's built-in ~5%). */
static uint32_t Winmm_JoyStickAxisRaw(int16_t value, int invert) {
	double normalized = value < 0 ? (double)value / 32768.0 : (double)value / 32767.0;
	double magnitude;

	if (invert) {
		normalized = -normalized;
	}
	magnitude = normalized < 0.0 ? -normalized : normalized;
	if (magnitude <= g_winmmJoyConfig.deadzone) {
		return WINMM_JOY_AXIS_CENTER;
	}
	return (uint32_t)(WINMM_JOY_AXIS_CENTER + (int)(normalized * 32767.0));
}

/* Converts a trigger axis (SDL 0..32767) to a raw WinMM axis value spanning the full
 * range; the game calibrates it as a centered axis (its config choice). */
static uint32_t Winmm_JoyTriggerAxisRaw(int16_t value, int invert) {
	double normalized = (double)value / 32767.0;

	if (normalized < 0.0) {
		normalized = 0.0;
	}
	if (normalized > 1.0) {
		normalized = 1.0;
	}
	if (invert) {
		normalized = 1.0 - normalized;
	}
	return (uint32_t)(normalized * (double)WINMM_JOY_AXIS_RANGE);
}

static int Winmm_JoyAxisIsTrigger(int aeronAxis) {
	return aeronAxis == AERON_GAMEPAD_AXIS_LEFT_TRIGGER || aeronAxis == AERON_GAMEPAD_AXIS_RIGHT_TRIGGER;
}

uint32_t XWA_WINMMAPI joyGetNumDevs(void) {
	/* Number of device slots the recovered code probes via joyGetDevCapsA. */
	return AERON_GAMEPAD_MAX;
}

MMRESULT XWA_WINMMAPI joyGetDevCapsA(uint32_t uJoyID, JOYCAPSA* pjc, uint32_t cbjc) {
	const AeronGamepadSnapshot* pad;
	int highestButton = 0;
	int b;

	Winmm_JoyLoadConfig();
	if (!pjc || cbjc < sizeof(JOYCAPSA)) {
		return JOYERR_PARMS;
	}
	pad = Winmm_JoyGamepad(uJoyID);
	if (!pad) {
		return JOYERR_UNPLUGGED;
	}

	memset(pjc, 0, sizeof(*pjc));
	pjc->wXmin = 0;
	pjc->wXmax = WINMM_JOY_AXIS_RANGE;
	pjc->wYmin = 0;
	pjc->wYmax = WINMM_JOY_AXIS_RANGE;
	pjc->wZmin = 0;
	pjc->wZmax = WINMM_JOY_AXIS_RANGE;
	pjc->wRmin = 0;
	pjc->wRmax = WINMM_JOY_AXIS_RANGE;
	/* Report the highest bound XWA button number as the button count. Dpad is mapped
	 * to buttons (not a POV hat), so no HASPOV; report a rudder axis (HASR). */
	for (b = 0; b < AERON_GAMEPAD_BUTTON_COUNT; ++b) {
		if (g_winmmJoyConfig.buttons[b] > highestButton) {
			highestButton = g_winmmJoyConfig.buttons[b];
		}
	}
	pjc->wNumButtons = (uint32_t)highestButton;
	pjc->wMaxButtons = WINMM_JOY_MAX_BUTTONS;
	pjc->wNumAxes = 4;
	pjc->wMaxAxes = 4;
	pjc->wCaps = JOYCAPS_HASZ | JOYCAPS_HASR;
	return JOYERR_NOERROR;
}

MMRESULT XWA_WINMMAPI joyGetPosEx(uint32_t uJoyID, JOYINFOEX* pji) {
	const AeronGamepadSnapshot* pad;
	WinmmJoystickTraceSample trace;
	int a;

	Winmm_JoyLoadConfig();
	if (!pji) {
		return JOYERR_PARMS;
	}
	pad = Winmm_JoyGamepad(uJoyID);
	if (!pad) {
		return JOYERR_UNPLUGGED;
	}

	pji->dwXpos = WINMM_JOY_AXIS_CENTER;
	pji->dwYpos = WINMM_JOY_AXIS_CENTER;
	pji->dwZpos = WINMM_JOY_AXIS_CENTER;
	pji->dwRpos = WINMM_JOY_AXIS_CENTER;
	pji->dwUpos = 0;
	pji->dwVpos = 0;
	pji->dwButtons = 0;
	pji->dwButtonNumber = 0;
	pji->dwPOV = JOY_POVCENTERED;

	memset(&trace, 0, sizeof(trace));
	trace.deviceId = uJoyID;
	trace.sourceAxisX = -1;
	trace.sourceAxisY = -1;
	trace.sourceAxisR = -1;

	/* Axes: each Aeron axis drives the configured WinMM axis slot. */
	for (a = 0; a < AERON_GAMEPAD_AXIS_COUNT; ++a) {
		int target = g_winmmJoyConfig.axes[a].target;
		int invert = g_winmmJoyConfig.axes[a].invert;
		uint32_t raw;

		if (target == WINMM_JOY_AXIS_NONE) {
			continue;
		}
		raw = Winmm_JoyAxisIsTrigger(a) ? Winmm_JoyTriggerAxisRaw(pad->axes[a], invert)
										: Winmm_JoyStickAxisRaw(pad->axes[a], invert);
		switch (target) {
			case WINMM_JOY_AXIS_X:
				pji->dwXpos = raw;
				trace.sourceAxisX = (int8_t)a;
				trace.sourceValueX = pad->axes[a];
				break;
			case WINMM_JOY_AXIS_Y:
				pji->dwYpos = raw;
				trace.sourceAxisY = (int8_t)a;
				trace.sourceValueY = pad->axes[a];
				break;
			case WINMM_JOY_AXIS_Z:
				pji->dwZpos = raw;
				break;
			case WINMM_JOY_AXIS_R:
				pji->dwRpos = raw;
				trace.sourceAxisR = (int8_t)a;
				trace.sourceValueR = pad->axes[a];
				break;
			default:
				break;
		}
	}

	trace.winmmX = pji->dwXpos;
	trace.winmmY = pji->dwYpos;
	trace.winmmR = pji->dwRpos;
	g_winmmJoystickTraceSample = trace;
	g_winmmJoystickTraceValid = 1;

	/* Buttons: each bound Aeron button sets its XWA button bit. */
	for (a = 0; a < AERON_GAMEPAD_BUTTON_COUNT; ++a) {
		int xwaButton = g_winmmJoyConfig.buttons[a];
		if (xwaButton >= 1 && (pad->buttons & (1u << a))) {
			pji->dwButtons |= 1u << (xwaButton - 1);
		}
	}

	return JOYERR_NOERROR;
}

int WinmmJoystick_GetLastTraceSample(WinmmJoystickTraceSample* sample) {
	if (!sample || !g_winmmJoystickTraceValid) {
		return 0;
	}
	*sample = g_winmmJoystickTraceSample;
	return 1;
}
