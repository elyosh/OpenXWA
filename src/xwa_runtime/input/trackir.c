/* Head tracking for the local player's camera: native TrackIR through the
 * NPClient64.dll that TrackIR installs (Windows only), or OpenTrack's UDP
 * output. No NaturalPoint SDK headers, code, or DLLs are distributed. */
#include "xwa_runtime/input/trackir.h"
#include "xwa_runtime/config/modern_input_options.h"
#include "xwa_runtime/input/opentrack_udp.h"

#include <math.h>
#include <string.h>

/* Stored separately from recovered simulation/player state. */
static XwaTrackIRPose s_pose;
static int s_pose_valid;
static float s_camera_offset[3];

void XwaTrackIR_ClearPose(void) {
	memset(&s_pose, 0, sizeof s_pose);
	memset(s_camera_offset, 0, sizeof s_camera_offset);
	s_pose_valid = 0;
}

void XwaTrackIR_SetCameraOffset(const float world_offset[3]) {
	if (world_offset && s_pose_valid) {
		memcpy(s_camera_offset, world_offset, sizeof s_camera_offset);
	}
}

void XwaTrackIR_GetCameraOffset(float world_offset[3]) {
	if (world_offset) {
		memcpy(world_offset, s_camera_offset, sizeof s_camera_offset);
	}
}

int XwaTrackIR_CurrentPose(XwaTrackIRPose* out) {
	if (out) {
		*out = s_pose;
	}
	return s_pose_valid;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <wchar.h>
#include <windows.h>

/* Minimal ABI for the TrackIR data endpoint (not the SDK implementation). */
#pragma pack(push, 1)
typedef struct TrackIRRawFrame {
	uint16_t status;
	uint16_t signature;
	uint32_t reserved;
	float roll, pitch, yaw;
	float x, y, z;
	float remaining[9];
} TrackIRRawFrame;
#pragma pack(pop)

typedef int(__stdcall* TrackIRRegisterWindow)(HWND);
typedef int(__stdcall* TrackIRUnregisterWindow)(void);
typedef int(__stdcall* TrackIRRegisterProfile)(uint16_t);
typedef int(__stdcall* TrackIRRequestData)(uint16_t);
typedef int(__stdcall* TrackIRTransmission)(void);
typedef int(__stdcall* TrackIRGetData)(TrackIRRawFrame*);

static HMODULE s_module;
static HWND s_window;
static TrackIRUnregisterWindow s_unregister;
static TrackIRTransmission s_stop;
static TrackIRGetData s_get;
static ULONGLONG s_next_attempt;
static ULONGLONG s_last_data_time;
static uint16_t s_signature;
static int s_has_signature;

static int16_t angle_q16(float raw, int invert) {
	float value = raw * (32768.0f / 16383.0f) * (invert ? -1.0f : 1.0f);
	if (value > 32767.0f) {
		value = 32767.0f;
	}
	if (value < -32767.0f) {
		value = -32767.0f;
	}
	return (int16_t)lrintf(value);
}

static void trackir_native_shutdown(void) {
	if (s_module) {
		if (s_stop) {
			s_stop();
		}
		if (s_unregister) {
			s_unregister();
		}
		FreeLibrary(s_module);
	}
	s_module = NULL;
	s_window = NULL;
	s_unregister = NULL;
	s_stop = NULL;
	s_get = NULL;
	s_has_signature = 0;
	s_last_data_time = 0;
	s_next_attempt = 0;
}

static int trackir_native_connect(void) {
	wchar_t dll_path[MAX_PATH];
	DWORD bytes = sizeof(dll_path);
	DWORD pid = 0;
	HWND hwnd = GetActiveWindow();
	TrackIRRegisterWindow reg_window;
	TrackIRRegisterProfile reg_profile;
	TrackIRRequestData request;
	TrackIRTransmission start;
	size_t len;

	if (!hwnd) {
		hwnd = GetForegroundWindow();
	}
	if (!hwnd) {
		return 0;
	}
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid != GetCurrentProcessId()) {
		return 0;
	}
	if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\NaturalPoint\\NATURALPOINT\\NPClient Location", L"Path",
					 RRF_RT_REG_SZ, NULL, dll_path, &bytes) != ERROR_SUCCESS) {
		return 0;
	}
	len = wcslen(dll_path);
	if (len > 0 && dll_path[len - 1] != L'\\' && dll_path[len - 1] != L'/') {
		if (len + 1 >= MAX_PATH) {
			return 0;
		}
		dll_path[len++] = L'\\';
	}
	if (len + (sizeof(L"NPClient64.dll") / sizeof(wchar_t)) > MAX_PATH) {
		return 0;
	}
	wcscpy(dll_path + len, L"NPClient64.dll");
	s_module = LoadLibraryW(dll_path);
	if (!s_module) {
		return 0;
	}

	reg_window = (TrackIRRegisterWindow)GetProcAddress(s_module, "NP_RegisterWindowHandle");
	s_unregister = (TrackIRUnregisterWindow)GetProcAddress(s_module, "NP_UnregisterWindowHandle");
	reg_profile = (TrackIRRegisterProfile)GetProcAddress(s_module, "NP_RegisterProgramProfileID");
	request = (TrackIRRequestData)GetProcAddress(s_module, "NP_RequestData");
	start = (TrackIRTransmission)GetProcAddress(s_module, "NP_StartDataTransmission");
	s_stop = (TrackIRTransmission)GetProcAddress(s_module, "NP_StopDataTransmission");
	s_get = (TrackIRGetData)GetProcAddress(s_module, "NP_GetData");
	if (!reg_window || !s_unregister || !reg_profile || !request || !start || !s_stop || !s_get ||
		reg_window(hwnd) != 0) {
		trackir_native_shutdown();
		return 0;
	}
	s_window = hwnd;
	/* 1000 is NaturalPoint's public SDK/developer test profile. */
	if (reg_profile(1000) != 0 || request(1 | 2 | 4 | 16 | 32 | 64) != 0 || start() != 0) {
		trackir_native_shutdown();
		return 0;
	}
	return 1;
}

static int trackir_native_poll(XwaTrackIRPose* out, const XwaHeadTrackingOptions* options) {
	const ULONGLONG now = GetTickCount64();
	const float cm_scale = 50.0f / 16383.0f;
	TrackIRRawFrame frame;

	if (!s_module) {
		if (now < s_next_attempt) {
			return 0;
		}
		if (!trackir_native_connect()) {
			/* A failed connect clears the deadline; never try loading the DLL
			   every render frame. */
			s_next_attempt = now + 3000;
			return 0;
		}
	}
	if (s_window != GetForegroundWindow()) {
		return 0;
	}
	memset(&frame, 0, sizeof frame);
	if (s_get(&frame) != 0 || frame.status != 0) {
		return 0;
	}
	if (!isfinite(frame.yaw) || !isfinite(frame.pitch) || !isfinite(frame.roll) || !isfinite(frame.x) ||
		!isfinite(frame.y) || !isfinite(frame.z)) {
		return 0;
	}
	if (!s_has_signature || frame.signature != s_signature) {
		s_signature = frame.signature;
		s_has_signature = 1;
		s_last_data_time = now;
	} else if (now - s_last_data_time > 500) {
		return 0;
	}

	out->yaw_q16 = angle_q16(frame.yaw, !options->invert[XWA_HEAD_TRACK_AXIS_YAW]);
	out->pitch_q16 = angle_q16(frame.pitch, !options->invert[XWA_HEAD_TRACK_AXIS_PITCH]);
	out->roll_q16 = angle_q16(frame.roll, !options->invert[XWA_HEAD_TRACK_AXIS_ROLL]);
	out->left_cm = frame.x * cm_scale * (options->invert[XWA_HEAD_TRACK_AXIS_X] ? -1.0f : 1.0f);
	/* NaturalPoint's vertical value is reversed from the seat-up direction. */
	out->up_cm = -frame.y * cm_scale * (options->invert[XWA_HEAD_TRACK_AXIS_Y] ? -1.0f : 1.0f);
	out->back_cm = frame.z * cm_scale * (options->invert[XWA_HEAD_TRACK_AXIS_Z] ? -1.0f : 1.0f);
	return 1;
}
#else
static void trackir_native_shutdown(void) {}

static int trackir_native_poll(XwaTrackIRPose* out, const XwaHeadTrackingOptions* options) {
	(void)out;
	(void)options;
	return 0;
}
#endif

void XwaTrackIR_Shutdown(void) {
	XwaOpenTrackUdp_Shutdown();
	trackir_native_shutdown();
	XwaTrackIR_ClearPose();
}

int XwaTrackIR_Poll(XwaTrackIRPose* out) {
	XwaModernInputOptions options;
	int polled;

	if (!out) {
		return 0;
	}
	memset(out, 0, sizeof *out);
	XwaTrackIR_ClearPose();
	XwaModernInputOptions_Get(&options);
	if (!options.head_tracking.enabled) {
		/* Disabled means no connection, polling, or leftover pose. */
		XwaOpenTrackUdp_Shutdown();
		trackir_native_shutdown();
		return 0;
	}
	/* Only one source stays connected; the other is released on a switch. */
	if (options.head_tracking.source == XWA_HEAD_TRACK_SOURCE_OPENTRACK_UDP) {
		trackir_native_shutdown();
		polled = XwaOpenTrackUdp_Poll(out, &options.head_tracking);
	} else {
		XwaOpenTrackUdp_Shutdown();
		polled = trackir_native_poll(out, &options.head_tracking);
	}
	if (!polled) {
		return 0;
	}
	s_pose = *out;
	s_pose_valid = 1;
	return 1;
}
