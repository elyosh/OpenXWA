#include "xwa/input/forcefeedback.h"

#include "xwa_runtime/compat/directx/dinput.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/util/debug.h"
#ifdef XWA_MODERN
#include "xwa_runtime/timing/modern_flight_timing.h"
#endif

#include <stddef.h>
#include <string.h>

/* Main window handle used for SetCooperativeLevel; owned by the display layer. The
 * DirectInput shim ignores it, but the recovered init passes it as the original did. */
extern void* g_hWnd;
#ifndef XWA_MODERN
extern void* g_hInstance;
#endif

/* Original debugFuncPtr @ 0x8D5758 (channel-tagged logger); channel 2 is the FF channel. */
#define debugFuncPtr DebugPrintfChannel

/* Runtime record for one preallocated DirectInput force-feedback effect. In the 32-bit
 * original this is a 14-byte packed struct walked by pointer; the port uses named fields
 * over a fixed [10] array (runtime state only, so layout is free). field_0D == 1 marks an
 * effect that is not auto-timed (played until explicitly stopped); when 0 the effect
 * expires after remainingTicks reaches zero. */
#pragma pack(push, 1)
typedef struct ForceFeedbackEffectSlot {
	uint8_t isPlaying;
	IDirectInputEffect* effect;
	int32_t durationTicks;
	int32_t remainingTicks;
	uint8_t field_0D;
} ForceFeedbackEffectSlot;
#pragma pack(pop)

// GLOBAL: XWA 0x63CF78
IDirectInputA* g_directInputInterface;
// GLOBAL: XWA 0x63CF7C
IDirectInputDevice2A* g_forceFeedbackDevice;
// GLOBAL: XWA 0x9E8F60
ForceFeedbackEffectSlot g_forceFeedbackEffects[10];
// GLOBAL: XWA 0x5B3408
unsigned int g_forceFeedbackStrength = 10000;
// GLOBAL: XWA 0x5B340C
unsigned int g_forceFeedbackCenterStrength = 10000;
// GLOBAL: XWA 0x5B3410
uint8_t g_forceFeedbackEffectsEnabled;
// GLOBAL: XWA 0x5B3414
int g_forceFeedbackEffectGainSupported;
// GLOBAL: XWA 0x63CF80
uint8_t g_forceFeedbackSuppressEffect7RestartOnce;

static void ForceFeedback_StartEffect(int effectNum);
int ForceFeedback_ReportError(int hresult);
int ForceFeedback_InitEffect(void);
int XWA_DXAPI ForceFeedback_EnumDeviceCallback(const DIDEVICEINSTANCEA* deviceInstance, void* context);
int XWA_DXAPI ForceFeedback_EnumEffectsNoopCallback(const DIEFFECTINFOA* effectInfo, void* context);

// FUNCTION: XWA 0x435490
uint8_t ForceFeedback_CheckDevice(void) {
	uint8_t createdInterface;
	HRESULT hr;

	createdInterface = 0;
	if (g_forceFeedbackDevice) {
		debugFuncPtr(2, "FF presence check TRUE when we already have a device acquired.\n");
		return 1;
	}
	if (!g_directInputInterface) {
#ifdef XWA_MODERN
		hr = DirectInputCreateA(NULL, 0x0500u, &g_directInputInterface, NULL);
#else
		hr = DirectInputCreateA(g_hInstance, 0x0500u, &g_directInputInterface, NULL);
#endif
		if (hr || g_directInputInterface == NULL) {
			debugFuncPtr(2, "Unable to create DI for FF check ");
			ForceFeedback_ReportError(hr);
			return 0;
		}
		createdInterface = 1;
	}
	if (g_directInputInterface->lpVtbl->EnumDevices(g_directInputInterface, DIDEVTYPE_JOYSTICK,
													ForceFeedback_EnumDeviceCallback, NULL,
													DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK) >= 0) {
		if (!g_forceFeedbackDevice) {
			debugFuncPtr(2, "FF check found no FF devices.\n");
			if (createdInterface && g_directInputInterface) {
				g_directInputInterface->lpVtbl->Release(g_directInputInterface);
				g_directInputInterface = NULL;
			}
			return 0;
		}
		g_forceFeedbackDevice->lpVtbl->Release(g_forceFeedbackDevice);
		g_forceFeedbackDevice = NULL;
		if (createdInterface && g_directInputInterface) {
			g_directInputInterface->lpVtbl->Release(g_directInputInterface);
			g_directInputInterface = NULL;
		}
		debugFuncPtr(2, "FF check found a FF device.\n");
		return 1;
	}
	debugFuncPtr(2, "FF device check failed.  EnumDevices failed.\n");
	if (createdInterface && g_directInputInterface) {
		g_directInputInterface->lpVtbl->Release(g_directInputInterface);
		g_directInputInterface = NULL;
	}
	return 0;
}

// FUNCTION: XWA 0x4355D0
int XWA_DXAPI ForceFeedback_EnumDeviceCallback(const DIDEVICEINSTANCEA* deviceInstance, void* context) {
	IDirectInputDeviceA* baseDevice;
	DxGuid guidInstance;
	HRESULT hr;
	(void)context;

	guidInstance = deviceInstance->guidInstance;

	if (g_directInputInterface->lpVtbl->CreateDevice(g_directInputInterface, &guidInstance, &baseDevice,
													 NULL) >= 0) {
		hr = baseDevice->lpVtbl->QueryInterface(baseDevice, &IID_IDirectInputDevice2A,
												(void**)&g_forceFeedbackDevice);
		baseDevice->lpVtbl->Release(baseDevice);
		if (hr < 0) {
			g_forceFeedbackDevice = NULL;
		}
	} else {
		g_forceFeedbackDevice = NULL;
	}
	return DIENUM_STOP;
}

// FUNCTION: XWA 0x435650
int ForceFeedback_Init(void) {
	DIPROPDWORD autoCenter;
	DIPROPDWORD gain;
	unsigned int strength;
	unsigned int centerStrength;
	int loaded;
	int i;
	HRESULT hr;

	debugFuncPtr(2, "Initializing Force Feedback...\n");
	if (!g_directInputInterface) {
		if (DirectInputCreateA(NULL, 0x0500u, &g_directInputInterface, NULL)) {
			debugFuncPtr(2, "Unable to create DI object.\n");
			return 0;
		}
	}
	if (g_forceFeedbackDevice) {
		debugFuncPtr(2, "FF already initialized.\n");
		return 1;
	}
	if (!g_gameConfig.ffEnabled) {
		debugFuncPtr(2, "FF Marked as Disabled.  Aborting initialization.\n");
		return 0;
	}
	if (g_directInputInterface->lpVtbl->EnumDevices(g_directInputInterface, DIDEVTYPE_JOYSTICK,
													ForceFeedback_EnumDeviceCallback, NULL,
													DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK) < 0) {
		debugFuncPtr(2, "Enum on FF devices failed.\n");
		return 0;
	}
	if (!g_forceFeedbackDevice) {
		debugFuncPtr(2, "No FF devices available.\n");
		return 0;
	}
	if (g_forceFeedbackDevice->lpVtbl->SetCooperativeLevel(g_forceFeedbackDevice, g_hWnd,
														   DISCL_EXCLUSIVE | DISCL_FOREGROUND) < 0) {
		g_forceFeedbackDevice->lpVtbl->Release(g_forceFeedbackDevice);
		g_forceFeedbackDevice = NULL;
		g_directInputInterface->lpVtbl->Release(g_directInputInterface);
		g_directInputInterface = NULL;
		debugFuncPtr(2, "FF init failed; couldn't set coop level.\n");
		return 0;
	}
	if (g_forceFeedbackDevice->lpVtbl->SetDataFormat(g_forceFeedbackDevice, &c_dfDIJoystick) < 0) {
		g_forceFeedbackDevice->lpVtbl->Release(g_forceFeedbackDevice);
		g_forceFeedbackDevice = NULL;
		g_directInputInterface->lpVtbl->Release(g_directInputInterface);
		g_directInputInterface = NULL;
		debugFuncPtr(2, "FF init failed; couldn't set data format.\n");
		return 0;
	}
	/* Disable joystick autocentering so it does not fight commanded effects. */
	autoCenter.diph.dwSize = sizeof(DIPROPDWORD);
	autoCenter.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	autoCenter.diph.dwObj = 0;
	autoCenter.diph.dwHow = 0;
	autoCenter.dwData = 0;
	hr = g_forceFeedbackDevice->lpVtbl->SetProperty(g_forceFeedbackDevice, DINPUT_DIPROP_AUTOCENTER,
													&autoCenter.diph);
	if (hr < 0) {
		g_forceFeedbackDevice->lpVtbl->Release(g_forceFeedbackDevice);
		g_forceFeedbackDevice = NULL;
		g_directInputInterface->lpVtbl->Release(g_directInputInterface);
		g_directInputInterface = NULL;
		debugFuncPtr(2, "FF init failed; setting autocentering property.\n");
		ForceFeedback_ReportError(hr);
		return 0;
	}
	strength = 1250 * g_gameConfig.ffStrength;
	if (strength > 10000) {
		strength = 10000;
	}
	centerStrength = 1250 * g_gameConfig.ffCenter;
	if (centerStrength > 10000) {
		centerStrength = 10000;
	}
	g_forceFeedbackStrength = strength;
	g_gameConfig.ffStrength = (uint8_t)(strength / 1250u);

	/* Stop any effects still marked playing before re-arming the amplification gain. */
	if (g_forceFeedbackEffectsEnabled) {
		for (i = 0; i < 10; ++i) {
			if (g_forceFeedbackEffects[i].isPlaying && g_forceFeedbackDevice &&
				g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[i].effect) {
				hr = g_forceFeedbackEffects[i].effect->lpVtbl->Stop(g_forceFeedbackEffects[i].effect);
				if (hr < 0) {
					debugFuncPtr(2, "StopEffect on effect %d failed ", i);
					ForceFeedback_ReportError(hr);
				}
				g_forceFeedbackEffects[i].isPlaying = 0;
				g_forceFeedbackEffects[i].remainingTicks = 0;
			}
		}
		g_forceFeedbackEffectsEnabled = 0;
	}

	gain.diph.dwSize = sizeof(DIPROPDWORD);
	gain.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	gain.diph.dwObj = 0;
	gain.diph.dwHow = 0;
	gain.dwData = strength ? strength : 1;
	hr = g_forceFeedbackDevice->lpVtbl->SetProperty(g_forceFeedbackDevice, DINPUT_DIPROP_FFGAIN, &gain.diph);
	if (hr < 0) {
		debugFuncPtr(2, "UpdateFFAmplification failed ");
		ForceFeedback_ReportError(hr);
		g_forceFeedbackDevice->lpVtbl->Release(g_forceFeedbackDevice);
		g_forceFeedbackDevice = NULL;
		g_directInputInterface->lpVtbl->Release(g_directInputInterface);
		g_directInputInterface = NULL;
		debugFuncPtr(2, "FF init failed; setting amplification level.\n");
		return 0;
	}
	if (g_forceFeedbackDevice && g_forceFeedbackStrength) {
		g_forceFeedbackEffectsEnabled = 1;
		ForceFeedback_SetCenteringStrength(g_forceFeedbackCenterStrength);
	}
	if (g_forceFeedbackDevice->lpVtbl->Acquire(g_forceFeedbackDevice) < 0) {
		g_forceFeedbackDevice->lpVtbl->Release(g_forceFeedbackDevice);
		g_forceFeedbackDevice = NULL;
		g_directInputInterface->lpVtbl->Release(g_directInputInterface);
		g_directInputInterface = NULL;
		debugFuncPtr(2, "FF init failed; acquiring device.\n");
		return 0;
	}
	memset(g_forceFeedbackEffects, 0, sizeof(g_forceFeedbackEffects));
	loaded = ForceFeedback_InitEffect();
	ForceFeedback_SetCenteringStrength(centerStrength);
	if (loaded == 10) {
		debugFuncPtr(2, "Force Feedback initialized completely, successfully\n");
		return 1;
	}
	if (loaded > 0) {
		debugFuncPtr(2, "** Unable to load all FF effects (loaded %d of %d).\n", loaded, 10);
		return 1;
	}
	g_forceFeedbackEffectsEnabled = 0;
	g_forceFeedbackDevice->lpVtbl->Release(g_forceFeedbackDevice);
	g_forceFeedbackDevice = NULL;
	g_directInputInterface->lpVtbl->Release(g_directInputInterface);
	g_directInputInterface = NULL;
	debugFuncPtr(2, "Unable to initialize Force Feedback.\n");
	return 0;
}

// FUNCTION: XWA 0x435A80
void ForceFeedback_ShutdownDevice(void) {
	int i;

	for (i = 0; i < 10; ++i) {
		if (g_forceFeedbackEffects[i].effect) {
			g_forceFeedbackEffects[i].effect->lpVtbl->Release(g_forceFeedbackEffects[i].effect);
			g_forceFeedbackEffects[i].effect = NULL;
		}
	}
	if (g_forceFeedbackDevice) {
		g_forceFeedbackDevice->lpVtbl->Release(g_forceFeedbackDevice);
		g_forceFeedbackDevice = NULL;
	}
	if (g_directInputInterface) {
		g_directInputInterface->lpVtbl->Release(g_directInputInterface);
		g_directInputInterface = NULL;
	}
}

// FUNCTION: XWA 0x435AE0
int ForceFeedback_InitEffect(void) {
	int32_t dir[2];
	uint32_t axes[2];
	DxGuid guid;
	DIEFFECT eff;
	int i;
	struct ForceFeedbackEffectParams {
		DIENVELOPE env;
		DICONSTANTFORCE constForce;
		DIPERIODIC periodic;
	} params;
	DICONDITION cond[2];
	DIEFFECTINFOA info;
	HRESULT hr;

	if (!g_forceFeedbackDevice) {
		return 0;
	}
	g_forceFeedbackDevice->lpVtbl->EnumEffects(g_forceFeedbackDevice, ForceFeedback_EnumEffectsNoopCallback,
											   NULL, 0);

	i = 0;
	while (1) {
		switch (i) {
			case 5: /* Constant force, direction 0, gain 6000, 40 ms, no envelope. */
				guid = GUID_ConstantForce;
				memset(&eff, 0, sizeof(eff));
				eff.rgdwAxes = axes;
				axes[1] = 4;
				eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
				axes[0] = 0;
				dir[0] = 0;
				dir[1] = 0;
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
				eff.dwDuration = 40000;
				eff.dwSamplePeriod = 0;
				eff.dwGain = 6000;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 2;
				eff.rglDirection = dir;
				eff.lpEnvelope = NULL;
				eff.lpvTypeSpecificParams = &params.constForce;
				params.constForce.lMagnitude = 10000;
				g_forceFeedbackEffects[5].durationTicks = 0;
				g_forceFeedbackEffects[5].isPlaying = 0;
				g_forceFeedbackEffects[5].field_0D = 1;
				g_forceFeedbackEffects[5].effect = NULL;
				if (!g_forceFeedbackDevice) {
					debugFuncPtr(2, "FF Device Failure in InitializeEffect");
					return 5;
				}
				hr = g_forceFeedbackDevice->lpVtbl->CreateEffect(g_forceFeedbackDevice, &guid, &eff,
																 &g_forceFeedbackEffects[5].effect, NULL);
				if (hr < 0) {
					debugFuncPtr(2, "Failed to create effect %d ", 5);
					ForceFeedback_ReportError(hr);
					return 5;
				}
				break;
			case 1: /* Constant force, direction 0, gain 8000, 0.15 s, fade 90 ms. */
				guid = GUID_ConstantForce;
				memset(&eff, 0, sizeof(eff));
				axes[0] = 0;
				axes[1] = 4;
				dir[0] = 0;
				dir[1] = 0;
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
				eff.dwDuration = 150000;
				eff.dwSamplePeriod = 0;
				eff.dwGain = 8000;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 2;
				eff.rgdwAxes = axes;
				eff.rglDirection = dir;
				eff.lpEnvelope = &params.env;
				eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
				eff.lpvTypeSpecificParams = &params.constForce;
				params.env.dwSize = sizeof(DIENVELOPE);
				params.env.dwAttackLevel = 0;
				params.env.dwAttackTime = 0;
				params.env.dwFadeLevel = 0;
				params.env.dwFadeTime = 90000;
				params.constForce.lMagnitude = 10000;
				g_forceFeedbackEffects[1].durationTicks = 0;
				g_forceFeedbackEffects[1].isPlaying = 0;
				g_forceFeedbackEffects[1].field_0D = 1;
				g_forceFeedbackEffects[1].effect = NULL;
				if (!g_forceFeedbackDevice) {
					debugFuncPtr(2, "FF Device Failure in InitializeEffect");
					return 1;
				}
				hr = g_forceFeedbackDevice->lpVtbl->CreateEffect(g_forceFeedbackDevice, &guid, &eff,
																 &g_forceFeedbackEffects[1].effect, NULL);
				if (hr < 0) {
					debugFuncPtr(2, "Failed to create effect %d ", 1);
					ForceFeedback_ReportError(hr);
					return 1;
				}
				break;
			case 0: /* Spring centering condition, both axes, infinite. */
				/* Probe whether centering gain can be changed while the effect is active. */
				guid = GUID_Spring;
				memset(&info, 0, sizeof(info));
				info.dwSize = sizeof(DIEFFECTINFOA);
				g_forceFeedbackDevice->lpVtbl->GetEffectInfo(g_forceFeedbackDevice, &info, &guid);
				if (info.dwDynamicParams & DIEP_GAIN) {
					*(unsigned char*)&g_forceFeedbackEffectGainSupported = 1;
					debugFuncPtr(2, "Can change gain of centering force while active..\n");
				} else {
					*(unsigned char*)&g_forceFeedbackEffectGainSupported = 0;
					debugFuncPtr(2, "Cannot change gain of centering force while active..\n");
				}
				guid = GUID_Spring;
				memset(&eff, 0, sizeof(eff));
				axes[0] = 0;
				axes[1] = 4;
				dir[0] = 9000;
				dir[1] = 0;
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
				eff.dwDuration = DI_INFINITE;
				eff.dwSamplePeriod = 0;
				eff.dwGain = 10000;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 2;
				eff.rgdwAxes = axes;
				eff.rglDirection = dir;
				eff.lpEnvelope = NULL;
				eff.cbTypeSpecificParams = 2 * sizeof(DICONDITION);
				eff.lpvTypeSpecificParams = cond;
				memset(cond, 0, sizeof(cond));
				cond[0].lOffset = 0;
				cond[0].lPositiveCoefficient = 10000;
				cond[0].lNegativeCoefficient = 10000;
				cond[0].dwPositiveSaturation = 10000;
				cond[0].dwNegativeSaturation = 10000;
				cond[0].lDeadBand = 0;
				cond[1] = cond[0];
				g_forceFeedbackEffects[0].durationTicks = 0;
				g_forceFeedbackEffects[0].isPlaying = 0;
				g_forceFeedbackEffects[0].field_0D = 1;
				g_forceFeedbackEffects[0].effect = NULL;
				if (!g_forceFeedbackDevice) {
					debugFuncPtr(2, "FF Device Failure in InitializeEffect");
					return i;
				}
				hr = g_forceFeedbackDevice->lpVtbl->CreateEffect(g_forceFeedbackDevice, &guid, &eff,
																 &g_forceFeedbackEffects[0].effect, NULL);
				if (hr < 0) {
					debugFuncPtr(2, "Failed to create effect %d ", 0);
					ForceFeedback_ReportError(hr);
					return i;
				}
				break;
			case 2: /* Constant force, direction 18000, gain 10000, 0.15 s, fade 200 ms. */
				guid = GUID_ConstantForce;
				memset(&eff, 0, sizeof(eff));
				axes[0] = 0;
				axes[1] = 4;
				dir[0] = 18000;
				dir[1] = 0;
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
				eff.dwDuration = 150000;
				eff.dwSamplePeriod = 0;
				eff.dwGain = 10000;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 2;
				eff.rgdwAxes = axes;
				eff.rglDirection = dir;
				eff.lpEnvelope = &params.env;
				eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
				eff.lpvTypeSpecificParams = &params.constForce;
				params.env.dwSize = sizeof(DIENVELOPE);
				params.env.dwAttackLevel = 0;
				params.env.dwAttackTime = 0;
				params.env.dwFadeLevel = 0;
				params.env.dwFadeTime = 200000;
				params.constForce.lMagnitude = 10000;
				g_forceFeedbackEffects[2].durationTicks = 0;
				g_forceFeedbackEffects[2].isPlaying = 0;
				g_forceFeedbackEffects[2].field_0D = 1;
				g_forceFeedbackEffects[2].effect = NULL;
				if (!g_forceFeedbackDevice) {
					debugFuncPtr(2, "FF Device Failure in InitializeEffect");
					return 2;
				}
				hr = g_forceFeedbackDevice->lpVtbl->CreateEffect(g_forceFeedbackDevice, &guid, &eff,
																 &g_forceFeedbackEffects[2].effect, NULL);
				if (hr < 0) {
					debugFuncPtr(2, "Failed to create effect %d ", 2);
					ForceFeedback_ReportError(hr);
					return 2;
				}
				break;
			case 3: /* Sine, direction 18000, gain 6000, 0.15 s, period 50 ms. */
				guid = GUID_Sine;
				memset(&eff, 0, sizeof(eff));
				axes[0] = 0;
				axes[1] = 4;
				dir[0] = 18000;
				dir[1] = 0;
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
				eff.dwDuration = 150000;
				eff.dwSamplePeriod = 0;
				eff.dwGain = 6000;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 2;
				eff.rgdwAxes = axes;
				eff.rglDirection = dir;
				eff.lpEnvelope = NULL;
				eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
				eff.lpvTypeSpecificParams = &params.periodic;
				params.periodic.dwMagnitude = 10000;
				params.periodic.lOffset = 0;
				params.periodic.dwPhase = 0;
				params.periodic.dwPeriod = 50000;
				g_forceFeedbackEffects[3].durationTicks = 0;
				g_forceFeedbackEffects[3].isPlaying = 0;
				g_forceFeedbackEffects[3].field_0D = 1;
				g_forceFeedbackEffects[3].effect = NULL;
				if (!g_forceFeedbackDevice) {
					debugFuncPtr(2, "FF Device Failure in InitializeEffect");
					return 3;
				}
				hr = g_forceFeedbackDevice->lpVtbl->CreateEffect(g_forceFeedbackDevice, &guid, &eff,
																 &g_forceFeedbackEffects[3].effect, NULL);
				if (hr < 0) {
					debugFuncPtr(2, "Failed to create effect %d ", 3);
					ForceFeedback_ReportError(hr);
					return 3;
				}
				break;
			case 8: /* Sine, direction 18000, gain 10000, 0.15 s, auto-timed (47 ticks). */
				guid = GUID_Sine;
				memset(&eff, 0, sizeof(eff));
				axes[0] = 0;
				axes[1] = 4;
				dir[0] = 18000;
				dir[1] = 0;
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
				eff.dwDuration = 150000;
				eff.dwSamplePeriod = 0;
				eff.dwGain = 10000;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 2;
				eff.rgdwAxes = axes;
				eff.rglDirection = dir;
				eff.lpEnvelope = NULL;
				eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
				eff.lpvTypeSpecificParams = &params.periodic;
				params.periodic.dwMagnitude = 10000;
				params.periodic.lOffset = 0;
				params.periodic.dwPhase = 0;
				params.periodic.dwPeriod = 50000;
				g_forceFeedbackEffects[8].durationTicks = 47;
				g_forceFeedbackEffects[8].isPlaying = 0;
				g_forceFeedbackEffects[8].field_0D = 1;
				g_forceFeedbackEffects[8].effect = NULL;
				if (!g_forceFeedbackDevice) {
					debugFuncPtr(2, "FF Device Failure in InitializeEffect");
					return 8;
				}
				hr = g_forceFeedbackDevice->lpVtbl->CreateEffect(g_forceFeedbackDevice, &guid, &eff,
																 &g_forceFeedbackEffects[8].effect, NULL);
				if (hr < 0) {
					debugFuncPtr(2, "Failed to create effect %d ", 8);
					ForceFeedback_ReportError(hr);
					return 8;
				}
				break;
			case 9: /* Sine, direction 18000, gain 5000, 0.3 s. */
				guid = GUID_Sine;
				memset(&eff, 0, sizeof(eff));
				axes[0] = 0;
				axes[1] = 4;
				dir[0] = 18000;
				dir[1] = 0;
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
				eff.dwDuration = 300000;
				eff.dwSamplePeriod = 0;
				eff.dwGain = 5000;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 2;
				eff.rgdwAxes = axes;
				eff.rglDirection = dir;
				eff.lpEnvelope = NULL;
				eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
				eff.lpvTypeSpecificParams = &params.periodic;
				params.periodic.dwMagnitude = 10000;
				params.periodic.lOffset = 0;
				params.periodic.dwPhase = 0;
				params.periodic.dwPeriod = 50000;
				g_forceFeedbackEffects[9].durationTicks = 0;
				g_forceFeedbackEffects[9].isPlaying = 0;
				g_forceFeedbackEffects[9].field_0D = 1;
				g_forceFeedbackEffects[9].effect = NULL;
				if (!g_forceFeedbackDevice) {
					debugFuncPtr(2, "FF Device Failure in InitializeEffect");
					return i;
				}
				hr = g_forceFeedbackDevice->lpVtbl->CreateEffect(g_forceFeedbackDevice, &guid, &eff,
																 &g_forceFeedbackEffects[9].effect, NULL);
				if (hr < 0) {
					debugFuncPtr(2, "Failed to create effect %d ", 9);
					ForceFeedback_ReportError(hr);
					return i;
				}
				g_forceFeedbackEffects[8].field_0D = 0;
				break;
			case 4: /* Impact constant force, direction 9000, gain 10000, fade 1 s. */
				guid = GUID_ConstantForce;
				memset(&eff, 0, sizeof(eff));
				eff.rglDirection = dir;
				eff.lpEnvelope = &params.env;
				eff.rgdwAxes = axes;
				axes[0] = 0;
				axes[1] = 4;
				dir[0] = 9000;
				dir[1] = 0;
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
				eff.dwDuration = 6000000;
				eff.dwSamplePeriod = 0;
				eff.dwGain = 10000;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 2;
				eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
				eff.lpvTypeSpecificParams = &params.constForce;
				params.env.dwSize = sizeof(DIENVELOPE);
				params.env.dwAttackLevel = 0;
				params.env.dwAttackTime = 0;
				params.env.dwFadeLevel = 0;
				params.env.dwFadeTime = 1000000;
				params.constForce.lMagnitude = 10000;
				g_forceFeedbackEffects[4].durationTicks = 0;
				g_forceFeedbackEffects[4].isPlaying = 0;
				g_forceFeedbackEffects[4].field_0D = 1;
				g_forceFeedbackEffects[4].effect = NULL;
				if (!g_forceFeedbackDevice) {
					debugFuncPtr(2, "FF Device Failure in InitializeEffect");
					return 4;
				}
				hr = g_forceFeedbackDevice->lpVtbl->CreateEffect(g_forceFeedbackDevice, &guid, &eff,
																 &g_forceFeedbackEffects[4].effect, NULL);
				if (hr < 0) {
					debugFuncPtr(2, "Failed to create effect %d ", 4);
					ForceFeedback_ReportError(hr);
					return 4;
				}
				break;
			case 6: /* Sine engine rumble, direction 0, gain 4000, infinite, 2 s attack. */
				guid = GUID_Sine;
				memset(&eff, 0, sizeof(eff));
				axes[0] = 0;
				axes[1] = 4;
				dir[0] = 0;
				dir[1] = 0;
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
				eff.dwDuration = DI_INFINITE;
				eff.dwSamplePeriod = 0;
				eff.dwGain = 4000;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 2;
				eff.rgdwAxes = axes;
				eff.rglDirection = dir;
				eff.lpEnvelope = &params.env;
				eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
				eff.lpvTypeSpecificParams = &params.periodic;
				params.env.dwSize = sizeof(DIENVELOPE);
				params.env.dwAttackLevel = 0;
				params.env.dwAttackTime = 2000000;
				params.env.dwFadeLevel = 0;
				params.env.dwFadeTime = 0;
				params.periodic.dwMagnitude = 10000;
				params.periodic.lOffset = 0;
				params.periodic.dwPhase = 0;
				params.periodic.dwPeriod = 50000;
				g_forceFeedbackEffects[6].durationTicks = 0;
				g_forceFeedbackEffects[6].isPlaying = 0;
				g_forceFeedbackEffects[6].field_0D = 1;
				g_forceFeedbackEffects[6].effect = NULL;
				if (!g_forceFeedbackDevice) {
					debugFuncPtr(2, "FF Device Failure in InitializeEffect");
					return 6;
				}
				hr = g_forceFeedbackDevice->lpVtbl->CreateEffect(g_forceFeedbackDevice, &guid, &eff,
																 &g_forceFeedbackEffects[6].effect, NULL);
				if (hr < 0) {
					debugFuncPtr(2, "Failed to create effect %d ", 6);
					ForceFeedback_ReportError(hr);
					return 6;
				}
				break;
			case 7: /* Sine, direction 0, gain 4000, 1 s, 1 s fade. */
				guid = GUID_Sine;
				memset(&eff, 0, sizeof(eff));
				eff.rgdwAxes = axes;
				eff.lpEnvelope = &params.env;
				eff.dwDuration = 1000000;
				eff.rglDirection = dir;
				params.env.dwFadeTime = 1000000;
				axes[0] = 0;
				axes[1] = 4;
				dir[0] = 0;
				dir[1] = 0;
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
				eff.dwSamplePeriod = 0;
				eff.dwGain = 4000;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 2;
				eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
				eff.lpvTypeSpecificParams = &params.periodic;
				params.env.dwSize = sizeof(DIENVELOPE);
				params.env.dwAttackLevel = 0;
				params.env.dwAttackTime = 0;
				params.env.dwFadeLevel = 0;
				params.periodic.dwMagnitude = 10000;
				params.periodic.lOffset = 0;
				params.periodic.dwPhase = 0;
				params.periodic.dwPeriod = 50000;
				g_forceFeedbackEffects[7].durationTicks = 0;
				g_forceFeedbackEffects[7].isPlaying = 0;
				g_forceFeedbackEffects[7].field_0D = 1;
				g_forceFeedbackEffects[7].effect = NULL;
				if (!g_forceFeedbackDevice) {
					debugFuncPtr(2, "FF Device Failure in InitializeEffect");
					return 7;
				}
				hr = g_forceFeedbackDevice->lpVtbl->CreateEffect(g_forceFeedbackDevice, &guid, &eff,
																 &g_forceFeedbackEffects[7].effect, NULL);
				if (hr < 0) {
					debugFuncPtr(2, "Failed to create effect %d ", 7);
					ForceFeedback_ReportError(hr);
					return 7;
				}
				break;
			default:
				break;
		}

		++i;
		if (i >= 10) {
			return i;
		}
	}
}

// FUNCTION: XWA 0x436A20
int XWA_DXAPI ForceFeedback_EnumEffectsNoopCallback(const DIEFFECTINFOA* effectInfo, void* context) {
	(void)effectInfo;
	(void)context;
	return DIENUM_STOP;
}

// FUNCTION: XWA 0x436A30
int ForceFeedback_ReportError(int hresult) {
	if ((unsigned int)hresult <= 0x80004005) {
		if (hresult == (int)0x80004005) {
			return debugFuncPtr(2, "Due to GENERIC error.\n");
		}
		if (hresult == (int)0x80004001) {
			return debugFuncPtr(2, "Due to UNSUPPORTED error.\n");
		}
		return debugFuncPtr(2, "Due to unknown error (%lx).\n", hresult);
	}
	if ((unsigned int)hresult <= 0x80040201) {
		if (hresult == (int)0x80040201) {
			return debugFuncPtr(2, "Due to DEVICEFULL error.\n");
		}
		if (hresult == (int)0x80040154) {
			return debugFuncPtr(2, "Due to DEVICENOTREG error.\n");
		}
		return debugFuncPtr(2, "Due to unknown error (%lx).\n", hresult);
	}
	if ((unsigned int)hresult <= 0x8007000E) {
		switch (hresult) {
			case (int)0x8007000E:
				return debugFuncPtr(2, "Due to OUTOFMEMORY error.\n");
			case (int)0x80040206:
				return debugFuncPtr(2, "Due to INCOMPLETEEFFECT error.\n");
			case (int)0x80040208:
				return debugFuncPtr(2, "Due to EFFECTPLAYING error.\n");
		}
		return debugFuncPtr(2, "Due to unknown error (%lx).\n", hresult);
	}
	if ((unsigned int)hresult <= 0x80070057) {
		if (hresult == (int)0x80070057) {
			return debugFuncPtr(2, "Due to INVALIDPARM error.\n");
		}
		if (hresult == (int)0x80070015) {
			return debugFuncPtr(2, "Due to NOTINITIALIZED error.\n");
		}
		return debugFuncPtr(2, "Due to unknown error (%lx).\n", hresult);
	}
	if ((unsigned int)hresult <= 0x800700AA) {
		if (hresult == (int)0x800700AA) {
			return debugFuncPtr(2, "Due to ACQUIRED error.\n");
		}
		if (hresult == (int)0x80070077) {
			return debugFuncPtr(2, "Due to BADDRIVERVER error.\n");
		}
		return debugFuncPtr(2, "Due to unknown error (%lx).\n", hresult);
	}
	if ((unsigned int)hresult <= 0x800704DF) {
		if (hresult == (int)0x800704DF) {
			return debugFuncPtr(2, "Due to ALREADYINITIALIZED error.\n");
		}
		if (hresult == (int)0x80070481) {
			return debugFuncPtr(2, "Due to BETADIRECTINPUTVERSION error.\n");
		}
		return debugFuncPtr(2, "Due to unknown error (%lx).\n", hresult);
	}
	switch (hresult) {
		case 0:
			return debugFuncPtr(2, "Due to... DI_OK.  o.O\n");
		case 1:
			return debugFuncPtr(2, "Due to BUFFEROVERFLOW or NOEFFECT or NOTATTACHED error.\n");
		case 2:
			return debugFuncPtr(2, "Due to POLLDEVICE error.\n");
		case 3:
			return debugFuncPtr(2, "Due to DOWNLOADSKIPPED error.\n");
		case 4:
			return debugFuncPtr(2, "Due to EFFECTRESTARTED error.\n");
		case 8:
			return debugFuncPtr(2, "Due to TRUNCATED success.\n");
		default:
			return debugFuncPtr(2, "Due to unknown error (%lx).\n", hresult);
	}
}

// FUNCTION: XWA 0x436C40
uint8_t ForceFeedback_SetStrength(unsigned int strength) {
	DIPROPDWORD gain;
	ForceFeedbackEffectSlot* slot;
	IDirectInputEffect** effectPtr;
	int i;
	HRESULT hr;

	g_forceFeedbackStrength = strength;
	g_gameConfig.ffStrength = (uint8_t)(strength / 1250u);
	if (!g_forceFeedbackDevice) {
		return 0;
	}

	gain.diph.dwSize = sizeof(DIPROPDWORD);
	gain.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	gain.diph.dwObj = 0;
	gain.diph.dwHow = 0;
	gain.dwData = 1;
	if (strength) {
		gain.dwData = strength;
	}

	if (g_forceFeedbackEffectsEnabled) {
		i = 0;
		effectPtr = &g_forceFeedbackEffects[0].effect;
		while ((intptr_t)effectPtr <
			   (intptr_t)&g_forceFeedbackEffects[0].effect + (intptr_t)sizeof(g_forceFeedbackEffects)) {
			slot = (ForceFeedbackEffectSlot*)((char*)effectPtr - offsetof(ForceFeedbackEffectSlot, effect));
			if (slot->isPlaying && g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled) {
				if ((intptr_t)effectPtr >= (intptr_t)&g_forceFeedbackEffects[0].effect +
											   (intptr_t)sizeof(g_forceFeedbackEffects) ||
					(intptr_t)effectPtr < (intptr_t)&g_forceFeedbackEffects[0].effect) {
					debugFuncPtr(2, "EffectNum out of range in StopEffect");
				} else if (*effectPtr) {
					hr = (*effectPtr)->lpVtbl->Stop(*effectPtr);
					if (hr < 0) {
						debugFuncPtr(2, "StopEffect on effect %d failed ", i);
						ForceFeedback_ReportError(hr);
					}
					slot->isPlaying = 0;
					slot->remainingTicks = 0;
				}
			}
			effectPtr = (IDirectInputEffect**)((char*)effectPtr + sizeof(ForceFeedbackEffectSlot));
			++i;
		}
		g_forceFeedbackEffectsEnabled = 0;
	}

	hr = g_forceFeedbackDevice->lpVtbl->SetProperty(g_forceFeedbackDevice, DINPUT_DIPROP_FFGAIN, &gain.diph);
	if (hr < 0) {
		debugFuncPtr(2, "UpdateFFAmplification failed ");
		ForceFeedback_ReportError(hr);
		return 0;
	}
	if (g_forceFeedbackDevice && g_forceFeedbackStrength) {
		g_forceFeedbackEffectsEnabled = 1;
		ForceFeedback_SetCenteringStrength(g_forceFeedbackCenterStrength);
	}
	return 1;
}

// FUNCTION: XWA 0x436DA0
unsigned int ForceFeedback_GetStrength(void) { return g_forceFeedbackStrength; }

// FUNCTION: XWA 0x436DB0
int ForceFeedback_SetCenteringStrength(unsigned int centerStrength) {
	DIEFFECT eff;
	DICONDITION cond[2];
	unsigned int coefficient;
	HRESULT hr;

	if (!g_forceFeedbackDevice || !g_forceFeedbackEffectsEnabled || !g_forceFeedbackStrength) {
		return (int)(intptr_t)g_forceFeedbackDevice;
	}

	g_gameConfig.ffCenter = (uint8_t)(centerStrength / 1250u);
	g_forceFeedbackCenterStrength = centerStrength;
	coefficient = centerStrength ? centerStrength : 1;

	cond[0].lOffset = 0;
	cond[0].lPositiveCoefficient = coefficient;
	cond[0].lNegativeCoefficient = coefficient;
	cond[0].dwPositiveSaturation = coefficient;
	cond[0].dwNegativeSaturation = coefficient;
	cond[0].lDeadBand = 0;
	cond[1] = cond[0];

	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.cbTypeSpecificParams = 2 * sizeof(DICONDITION);
	eff.lpvTypeSpecificParams = cond;

	if (g_forceFeedbackEffects[0].effect) {
		hr = g_forceFeedbackEffects[0].effect->lpVtbl->Stop(g_forceFeedbackEffects[0].effect);
		if (hr < 0) {
			debugFuncPtr(2, "StopEffect on effect %d failed ", 0);
			ForceFeedback_ReportError(hr);
		}
		g_forceFeedbackEffects[0].isPlaying = 0;
		g_forceFeedbackEffects[0].remainingTicks = 0;
	}
	if (!g_forceFeedbackDevice) {
		return (int)(intptr_t)g_forceFeedbackEffects[0].effect;
	}
	if (g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[0].effect && g_forceFeedbackStrength) {
		hr = g_forceFeedbackEffects[0].effect->lpVtbl->SetParameters(g_forceFeedbackEffects[0].effect, &eff,
																	 DIEP_TYPESPECIFICPARAMS);
		if (hr < 0) {
			debugFuncPtr(2, "Failed setting parameters for effect %d ", 0);
			ForceFeedback_ReportError(hr);
		}
	}
	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[0].effect &&
		g_forceFeedbackStrength) {
		hr = g_forceFeedbackEffects[0].effect->lpVtbl->Start(g_forceFeedbackEffects[0].effect, 1, 0);
		if (hr < 0) {
			debugFuncPtr(2, "StartEffect on effect %d failed ", 0);
			ForceFeedback_ReportError(hr);
		}
		g_forceFeedbackEffects[0].isPlaying = 1;
		g_forceFeedbackEffects[0].remainingTicks = g_forceFeedbackEffects[0].durationTicks;
	}
	return (int)(intptr_t)g_forceFeedbackEffects[0].effect;
}

// FUNCTION: XWA 0x436F50
unsigned int ForceFeedback_GetCenteringStrength(void) { return g_forceFeedbackCenterStrength; }

// FUNCTION: XWA 0x436F60
void ForceFeedback_StopAllEffects(void) {
	ForceFeedbackEffectSlot* slot;
	IDirectInputEffect** effectPtr;
	int i;
	HRESULT hr;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled) {
#ifdef XWA_MODERN
		for (i = 0; i < 10; ++i) {
			slot = &g_forceFeedbackEffects[i];
			effectPtr = &slot->effect;
#else
		i = 0;
		effectPtr = &g_forceFeedbackEffects[0].effect;
		while ((intptr_t)effectPtr <
			   (intptr_t)&g_forceFeedbackEffects[0].effect + (intptr_t)sizeof(g_forceFeedbackEffects)) {
			slot = (ForceFeedbackEffectSlot*)((char*)effectPtr - offsetof(ForceFeedbackEffectSlot, effect));
#endif
			if (slot->isPlaying && g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled) {
#ifdef XWA_MODERN
				if ((unsigned int)i >= 10) {
#else
				if ((intptr_t)effectPtr >= (intptr_t)&g_forceFeedbackEffects[0].effect +
											   (intptr_t)sizeof(g_forceFeedbackEffects) ||
					(intptr_t)effectPtr < (intptr_t)&g_forceFeedbackEffects[0].effect) {
#endif
					debugFuncPtr(2, "EffectNum out of range in StopEffect");
				} else if (*effectPtr) {
					hr = (*effectPtr)->lpVtbl->Stop(*effectPtr);
					if (hr < 0) {
						debugFuncPtr(2, "StopEffect on effect %d failed ", i);
						ForceFeedback_ReportError(hr);
					}
					slot->isPlaying = 0;
					slot->remainingTicks = 0;
				}
			}
#ifndef XWA_MODERN
			effectPtr = (IDirectInputEffect**)((char*)effectPtr + sizeof(ForceFeedbackEffectSlot));
			++i;
#endif
		}
		g_forceFeedbackEffectsEnabled = 0;
	}
}

// FUNCTION: XWA 0x437010
int ForceFeedback_EnableEffects(void) {
	if (g_forceFeedbackDevice && g_forceFeedbackStrength) {
		g_forceFeedbackEffectsEnabled = 1;
		return ForceFeedback_SetCenteringStrength(g_forceFeedbackCenterStrength);
	}
	return (int)(intptr_t)g_forceFeedbackDevice;
}

// FUNCTION: XWA 0x437040
void ForceFeedback_UpdateActiveEffects(int elapsedTicks) {
	int restartPending;
	int i;
	HRESULT hr;

	if (!g_forceFeedbackDevice || !g_forceFeedbackEffectsEnabled || !g_forceFeedbackStrength) {
		return;
	}

	restartPending = 0;
	for (i = 0; i < 10; ++i) {
		if (restartPending) {
			if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[i].effect &&
				g_forceFeedbackStrength) {
				hr = g_forceFeedbackEffects[i].effect->lpVtbl->Start(g_forceFeedbackEffects[i].effect, 1, 0);
				if (hr < 0) {
					debugFuncPtr(2, "StartEffect on effect %d failed ", i);
					ForceFeedback_ReportError(hr);
				}
				g_forceFeedbackEffects[i].isPlaying = 1;
				g_forceFeedbackEffects[i].remainingTicks = g_forceFeedbackEffects[i].durationTicks;
			}
			restartPending = 0;
		}
		if (!g_forceFeedbackEffects[i].field_0D && g_forceFeedbackEffects[i].isPlaying == 1) {
			g_forceFeedbackEffects[i].remainingTicks -= elapsedTicks;
			if (g_forceFeedbackEffects[i].remainingTicks <= 0) {
				if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled &&
					g_forceFeedbackEffects[i].effect) {
					hr = g_forceFeedbackEffects[i].effect->lpVtbl->Stop(g_forceFeedbackEffects[i].effect);
					if (hr < 0) {
						debugFuncPtr(2, "StopEffect on effect %d failed ", i);
						ForceFeedback_ReportError(hr);
					}
					g_forceFeedbackEffects[i].isPlaying = 0;
					g_forceFeedbackEffects[i].remainingTicks = 0;
				}
				restartPending = 1;
			}
		}
	}

#ifdef XWA_MODERN
	if (!XwaModernFlightTiming_IsHighRate() || XwaModernFlightTiming_IsLegacyCadenceDue()) {
#endif
		if (g_forceFeedbackSuppressEffect7RestartOnce) {
			g_forceFeedbackSuppressEffect7RestartOnce = 0;
		} else if (g_forceFeedbackEffects[6].isPlaying) {
			if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[7].effect &&
				g_forceFeedbackStrength) {
				hr = g_forceFeedbackEffects[7].effect->lpVtbl->Start(g_forceFeedbackEffects[7].effect, 1, 0);
				if (hr < 0) {
					debugFuncPtr(2, "StartEffect on effect %d failed ", 7);
					ForceFeedback_ReportError(hr);
				}
				g_forceFeedbackEffects[7].isPlaying = 1;
				g_forceFeedbackEffects[7].remainingTicks = g_forceFeedbackEffects[7].durationTicks;
			}
			if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[6].effect) {
				hr = g_forceFeedbackEffects[6].effect->lpVtbl->Stop(g_forceFeedbackEffects[6].effect);
				if (hr < 0) {
					debugFuncPtr(2, "StopEffect on effect %d failed ", 6);
					ForceFeedback_ReportError(hr);
				}
				g_forceFeedbackEffects[6].isPlaying = 0;
				g_forceFeedbackEffects[6].remainingTicks = 0;
			}
		}
#ifdef XWA_MODERN
	}
#endif

	if (g_players[g_localPlayer].objectIndex) {
		/* Roll the current local speed snapshot into the unused previous-snapshot pair. */
		g_unusedForceFeedbackPrevSpeedSnapshotLo = g_forceFeedbackLocalSpeedSnapshot.packed;
		g_unusedForceFeedbackPrevSpeedSnapshotHigh = g_forceFeedbackLocalSpeedSnapshotHigh;
	}
}

// FUNCTION: XWA 0x437280
void ForceFeedback_PlayLaserFireEffect(void) {
	HRESULT hr;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[5].effect &&
		g_forceFeedbackStrength) {
		hr = g_forceFeedbackEffects[5].effect->lpVtbl->Start(g_forceFeedbackEffects[5].effect, 1, 0);
		if (hr < 0) {
			debugFuncPtr(2, "StartEffect on effect %d failed ", 5);
			ForceFeedback_ReportError(hr);
		}
		g_forceFeedbackEffects[5].isPlaying = 1;
		g_forceFeedbackEffects[5].remainingTicks = g_forceFeedbackEffects[5].durationTicks;
	}
}

// FUNCTION: XWA 0x4372F0
void ForceFeedback_PlayWarheadFireEffect(void) {
	HRESULT hr;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[1].effect &&
		g_forceFeedbackStrength) {
		hr = g_forceFeedbackEffects[1].effect->lpVtbl->Start(g_forceFeedbackEffects[1].effect, 1, 0);
		if (hr < 0) {
			debugFuncPtr(2, "StartEffect on effect %d failed ", 1);
			ForceFeedback_ReportError(hr);
		}
		g_forceFeedbackEffects[1].isPlaying = 1;
		g_forceFeedbackEffects[1].remainingTicks = g_forceFeedbackEffects[1].durationTicks;
	}
}

// FUNCTION: XWA 0x437360
void ForceFeedback_PlayLongDirectionalDamageEffect(int directionDegrees) {
	DIEFFECT eff;
	uint32_t axes[2];
	int32_t dir[2];
	IDirectInputEffect* effect;
	HRESULT hr;

	dir[0] = 100 * directionDegrees;
	dir[1] = 0;
	axes[0] = 0;
	axes[1] = 4;
	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
	eff.dwGain = 6000;
	eff.cAxes = 2;
	eff.rgdwAxes = axes;
	eff.rglDirection = dir;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect) {
		hr = g_forceFeedbackEffects[2].effect->lpVtbl->Stop(g_forceFeedbackEffects[2].effect);
		if (hr < 0) {
			debugFuncPtr(2, "StopEffect on effect %d failed ", 2);
			ForceFeedback_ReportError(hr);
		}
		g_forceFeedbackEffects[2].isPlaying = 0;
		g_forceFeedbackEffects[2].remainingTicks = 0;
	}
	effect = g_forceFeedbackEffects[2].effect;
	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && effect && g_forceFeedbackStrength) {
		hr = effect->lpVtbl->SetParameters(effect, &eff, DIEP_GAIN | DIEP_DIRECTION);
		if (hr < 0) {
			debugFuncPtr(2, "Failed setting parameters for effect %d ", 2);
			ForceFeedback_ReportError(hr);
			return;
		}
		if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[2].effect->lpVtbl->Start(g_forceFeedbackEffects[2].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 2);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[2].isPlaying = 1;
			g_forceFeedbackEffects[2].remainingTicks = g_forceFeedbackEffects[2].durationTicks;
		}
	}
}

// FUNCTION: XWA 0x4374E0
void ForceFeedback_PlayShortDirectionalDamageEffect(int directionDegrees) {
	DIEFFECT eff;
	uint32_t axes[2];
	int32_t dir[2];
	IDirectInputEffect* effect;
	HRESULT hr;

	dir[0] = 100 * directionDegrees;
	dir[1] = 0;
	axes[0] = 0;
	axes[1] = 4;
	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
	eff.dwGain = 3000;
	eff.cAxes = 2;
	eff.rgdwAxes = axes;
	eff.rglDirection = dir;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect) {
		hr = g_forceFeedbackEffects[2].effect->lpVtbl->Stop(g_forceFeedbackEffects[2].effect);
		if (hr < 0) {
			debugFuncPtr(2, "StopEffect on effect %d failed ", 2);
			ForceFeedback_ReportError(hr);
		}
		g_forceFeedbackEffects[2].isPlaying = 0;
		g_forceFeedbackEffects[2].remainingTicks = 0;
	}
	effect = g_forceFeedbackEffects[2].effect;
	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && effect && g_forceFeedbackStrength) {
		hr = effect->lpVtbl->SetParameters(effect, &eff, DIEP_GAIN | DIEP_DIRECTION);
		if (hr < 0) {
			debugFuncPtr(2, "Failed setting parameters for effect %d ", 2);
			ForceFeedback_ReportError(hr);
			return;
		}
		if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[2].effect->lpVtbl->Start(g_forceFeedbackEffects[2].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 2);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[2].isPlaying = 1;
			g_forceFeedbackEffects[2].remainingTicks = g_forceFeedbackEffects[2].durationTicks;
		}
	}
}

// FUNCTION: XWA 0x437660
void ForceFeedback_PlayProximityEffectForObject(int proximityEffectKind, unsigned int objIdx) {
	MobileObject* mobj;
	int localPlayerIdx;
	int dx;
	int dy;
	int dz;
	unsigned int distance;
	HRESULT hr;

	if (!g_flightSideEffectsEnabled || g_flightSimSideEffectsSuppressed || !g_forceFeedbackDevice ||
		!g_forceFeedbackStrength || !g_forceFeedbackEffectsEnabled) {
		return;
	}
	if (objIdx == 0xFFFF) {
		return;
	}

	mobj = g_objectTable[objIdx].mobj;
	if (mobj) {
		localPlayerIdx = g_localPlayer;
		dx = mobj->prevWorldX - g_players[g_localPlayer].viewState.savedTargetX;
		dy = mobj->prevWorldY - g_players[g_localPlayer].viewState.savedTargetY;
		dz = mobj->prevWorldZ - g_players[localPlayerIdx].viewState.savedTargetZ;
	} else {
		Mission_ResolveObjectOrMissionPointWorldLoc(objIdx, 0, 0, 0);
		localPlayerIdx = g_localPlayer;
		dx = worldlocx - g_players[g_localPlayer].viewState.savedTargetX;
		dy = worldlocy - g_players[g_localPlayer].viewState.savedTargetY;
		dz = worldlocz - g_players[localPlayerIdx].viewState.savedTargetZ;
	}
	distance = collide_roughdistance3d(dx, dy, dz);

	switch (proximityEffectKind) {
		case 1:
			if (distance < 0x4000) {
				if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled &&
					g_forceFeedbackEffects[3].effect && g_forceFeedbackStrength) {
					hr = g_forceFeedbackEffects[3].effect->lpVtbl->Start(g_forceFeedbackEffects[3].effect, 1,
																		 0);
					if (hr < 0) {
						debugFuncPtr(2, "StartEffect on effect %d failed ", 3);
						ForceFeedback_ReportError(hr);
					}
					g_forceFeedbackEffects[3].isPlaying = 1;
					g_forceFeedbackEffects[3].remainingTicks = g_forceFeedbackEffects[3].durationTicks;
				}
			}
			return;
		case 0:
			break;
		default:
			return;
	}

	if (distance >= 0x6000) {
		return;
	}
	if (distance < 0x4000) {
		/* Very close: fire the short close-impact effect 8, else fall back to effect 3. */
		if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[8].effect &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[8].effect->lpVtbl->Start(g_forceFeedbackEffects[8].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 8);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[8].isPlaying = 1;
			g_forceFeedbackEffects[8].remainingTicks = g_forceFeedbackEffects[8].durationTicks;
		}
		if (g_forceFeedbackEffects[8].isPlaying != 1) {
			ForceFeedback_StartEffect(3);
		}
		return;
	}
	/* Mid range: play the periodic proximity effect 3. */
	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[3].effect &&
		g_forceFeedbackStrength) {
		hr = g_forceFeedbackEffects[3].effect->lpVtbl->Start(g_forceFeedbackEffects[3].effect, 1, 0);
		if (hr < 0) {
			debugFuncPtr(2, "StartEffect on effect %d failed ", 3);
			ForceFeedback_ReportError(hr);
		}
		g_forceFeedbackEffects[3].isPlaying = 1;
		g_forceFeedbackEffects[3].remainingTicks = g_forceFeedbackEffects[3].durationTicks;
	}
}

// FUNCTION: XWA 0x4378A0
static void ForceFeedback_StartEffect(int effectNum) {
	HRESULT hr;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled) {
		if (effectNum >= 10 || effectNum < 0) {
			debugFuncPtr(2, "EffectNum out of range in StartEffect");
		} else if (g_forceFeedbackEffects[effectNum].effect && g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[effectNum].effect->lpVtbl->Start(
				g_forceFeedbackEffects[effectNum].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", effectNum);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[effectNum].isPlaying = 1;
			g_forceFeedbackEffects[effectNum].remainingTicks =
				g_forceFeedbackEffects[effectNum].durationTicks;
		}
	}
}

// FUNCTION: XWA 0x437A90
void ForceFeedback_PlayImpactEffect(int directionDegrees, int impactMagnitude) {
	DIEFFECT eff;
	uint32_t axes[2];
	int32_t dir[2];
	HRESULT hr;

	dir[0] = 100 * directionDegrees;
	dir[1] = 0;
	axes[0] = 0;
	axes[1] = 4;
	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
	eff.cAxes = 2;
	eff.rgdwAxes = axes;
	eff.rglDirection = dir;
	eff.dwDuration = 244 * (int16_t)impactMagnitude;
	if (eff.dwDuration < 1000000u) {
		eff.dwDuration = 1000000;
	}

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[4].effect &&
		g_forceFeedbackStrength) {
		hr = g_forceFeedbackEffects[4].effect->lpVtbl->SetParameters(g_forceFeedbackEffects[4].effect, &eff,
																	 DIEP_DURATION | DIEP_DIRECTION);
		if (hr < 0) {
			debugFuncPtr(2, "Failed setting parameters for effect %d ", 4);
			ForceFeedback_ReportError(hr);
			return;
		}
		if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[4].effect &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[4].effect->lpVtbl->Start(g_forceFeedbackEffects[4].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 4);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[4].isPlaying = 1;
			g_forceFeedbackEffects[4].remainingTicks = g_forceFeedbackEffects[4].durationTicks;
		}
	}
}

// FUNCTION: XWA 0x437BF0
void ForceFeedback_PlayCriticalDamageEffect(void) {
	DIEFFECT eff;
	uint32_t axes[2];
	int32_t dir[2];
	HRESULT hr;

	if (g_forceFeedbackEffects[6].effect) {
		if (!g_forceFeedbackEffects[6].isPlaying && g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[6].effect->lpVtbl->Start(g_forceFeedbackEffects[6].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 6);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[6].isPlaying = 1;
			g_forceFeedbackEffects[6].remainingTicks = g_forceFeedbackEffects[6].durationTicks;
		}
		g_forceFeedbackSuppressEffect7RestartOnce = 1;
		return;
	}

	/* No dedicated effect 6: fall back to a short constant-force pulse on effect 2. */
	dir[0] = 0;
	dir[1] = 0;
	axes[0] = 0;
	axes[1] = 4;
	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
	eff.dwGain = 3000;
	eff.cAxes = 2;
	eff.rgdwAxes = axes;
	eff.rglDirection = dir;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect) {
		hr = g_forceFeedbackEffects[2].effect->lpVtbl->Stop(g_forceFeedbackEffects[2].effect);
		if (hr < 0) {
			debugFuncPtr(2, "StopEffect on effect %d failed ", 2);
			ForceFeedback_ReportError(hr);
		}
		g_forceFeedbackEffects[2].isPlaying = 0;
		g_forceFeedbackEffects[2].remainingTicks = 0;
	}
	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect &&
		g_forceFeedbackStrength) {
		hr = g_forceFeedbackEffects[2].effect->lpVtbl->SetParameters(g_forceFeedbackEffects[2].effect, &eff,
																	 DIEP_GAIN | DIEP_DIRECTION);
		if (hr < 0) {
			debugFuncPtr(2, "Failed setting parameters for effect %d ", 2);
			ForceFeedback_ReportError(hr);
			return;
		}
		if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[2].effect->lpVtbl->Start(g_forceFeedbackEffects[2].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 2);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[2].isPlaying = 1;
			g_forceFeedbackEffects[2].remainingTicks = g_forceFeedbackEffects[2].durationTicks;
		}
	}
}

// FUNCTION: XWA 0x437DE0
void ForceFeedback_PlayBoardOrPickupReleaseEffect(void) {
	DIEFFECT eff;
	uint32_t axes[2];
	int32_t dir[2];
	IDirectInputEffect* effect;
	HRESULT hr;

	dir[0] = 0;
	dir[1] = 0;
	axes[0] = 0;
	axes[1] = 4;
	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
	eff.dwGain = 6000;
	eff.cAxes = 2;
	eff.rgdwAxes = axes;
	eff.rglDirection = dir;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect) {
		hr = g_forceFeedbackEffects[2].effect->lpVtbl->Stop(g_forceFeedbackEffects[2].effect);
		if (hr < 0) {
			debugFuncPtr(2, "StopEffect on effect %d failed ", 2);
			ForceFeedback_ReportError(hr);
		}
		g_forceFeedbackEffects[2].isPlaying = 0;
		g_forceFeedbackEffects[2].remainingTicks = 0;
	}
	effect = g_forceFeedbackEffects[2].effect;
	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && effect && g_forceFeedbackStrength) {
		hr = effect->lpVtbl->SetParameters(effect, &eff, DIEP_GAIN | DIEP_DIRECTION);
		if (hr < 0) {
			debugFuncPtr(2, "Failed setting parameters for effect %d ", 2);
			ForceFeedback_ReportError(hr);
			return;
		}
		if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[2].effect->lpVtbl->Start(g_forceFeedbackEffects[2].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 2);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[2].isPlaying = 1;
			g_forceFeedbackEffects[2].remainingTicks = g_forceFeedbackEffects[2].durationTicks;
		}
	}
}

// FUNCTION: XWA 0x437F60
void ForceFeedback_PlayHyperspaceOutboundEffect(void) {
	DIEFFECT eff;
	uint32_t axes[2];
	int32_t dir[2];
	HRESULT hr;

	dir[0] = 0;
	dir[1] = 0;
	axes[0] = 0;
	axes[1] = 4;
	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
	eff.cAxes = 2;
	eff.rgdwAxes = axes;
	eff.rglDirection = dir;
	eff.dwDuration = 4997120;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[4].effect &&
		g_forceFeedbackStrength) {
		hr = g_forceFeedbackEffects[4].effect->lpVtbl->SetParameters(g_forceFeedbackEffects[4].effect, &eff,
																	 DIEP_DURATION | DIEP_DIRECTION);
		if (hr < 0) {
			debugFuncPtr(2, "Failed setting parameters for effect %d ", 4);
			ForceFeedback_ReportError(hr);
			return;
		}
		if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[4].effect &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[4].effect->lpVtbl->Start(g_forceFeedbackEffects[4].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 4);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[4].isPlaying = 1;
			g_forceFeedbackEffects[4].remainingTicks = g_forceFeedbackEffects[4].durationTicks;
		}
	}
}

// FUNCTION: XWA 0x4380A0
void ForceFeedback_PlayHyperspaceInboundEffect(void) {
	DIEFFECT eff;
	uint32_t axes[2];
	int32_t dir[2];
	HRESULT hr;

	dir[0] = 18000;
	dir[1] = 0;
	axes[0] = 0;
	axes[1] = 4;
	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
	eff.cAxes = 2;
	eff.rgdwAxes = axes;
	eff.rglDirection = dir;
	eff.dwDuration = 1000000;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[4].effect &&
		g_forceFeedbackStrength) {
		hr = g_forceFeedbackEffects[4].effect->lpVtbl->SetParameters(g_forceFeedbackEffects[4].effect, &eff,
																	 DIEP_DURATION | DIEP_DIRECTION);
		if (hr < 0) {
			debugFuncPtr(2, "Failed setting parameters for effect %d ", 4);
			ForceFeedback_ReportError(hr);
			return;
		}
		if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[4].effect &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[4].effect->lpVtbl->Start(g_forceFeedbackEffects[4].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 4);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[4].isPlaying = 1;
			g_forceFeedbackEffects[4].remainingTicks = g_forceFeedbackEffects[4].durationTicks;
		}
	}
}

// FUNCTION: XWA 0x4384E0
void ForceFeedback_PlayCraftDestructionEffect(void) {
	DIEFFECT eff;
	uint32_t axes[2];
	int32_t dir[2];
	IDirectInputEffect* effect;
	HRESULT hr;

	if (g_forceFeedbackEffects[6].effect) {
		if (!g_forceFeedbackEffects[6].isPlaying && g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[6].effect->lpVtbl->Start(g_forceFeedbackEffects[6].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 6);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[6].isPlaying = 1;
			g_forceFeedbackEffects[6].remainingTicks = g_forceFeedbackEffects[6].durationTicks;
		}
		g_forceFeedbackSuppressEffect7RestartOnce = 1;
		return;
	}

	/* No dedicated effect 6: fall back to a short constant-force pulse on effect 2. */
	dir[0] = 0;
	dir[1] = 0;
	axes[0] = 0;
	axes[1] = 4;
	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_POLAR;
	eff.dwGain = 3000;
	eff.cAxes = 2;
	eff.rgdwAxes = axes;
	eff.rglDirection = dir;

	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect) {
		hr = g_forceFeedbackEffects[2].effect->lpVtbl->Stop(g_forceFeedbackEffects[2].effect);
		if (hr < 0) {
			debugFuncPtr(2, "StopEffect on effect %d failed ", 2);
			ForceFeedback_ReportError(hr);
		}
		g_forceFeedbackEffects[2].isPlaying = 0;
		g_forceFeedbackEffects[2].remainingTicks = 0;
	}
	effect = g_forceFeedbackEffects[2].effect;
	if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && effect && g_forceFeedbackStrength) {
		hr = effect->lpVtbl->SetParameters(effect, &eff, DIEP_GAIN | DIEP_DIRECTION);
		if (hr < 0) {
			debugFuncPtr(2, "Failed setting parameters for effect %d ", 2);
			ForceFeedback_ReportError(hr);
			return;
		}
		if (g_forceFeedbackDevice && g_forceFeedbackEffectsEnabled && g_forceFeedbackEffects[2].effect &&
			g_forceFeedbackStrength) {
			hr = g_forceFeedbackEffects[2].effect->lpVtbl->Start(g_forceFeedbackEffects[2].effect, 1, 0);
			if (hr < 0) {
				debugFuncPtr(2, "StartEffect on effect %d failed ", 2);
				ForceFeedback_ReportError(hr);
			}
			g_forceFeedbackEffects[2].isPlaying = 1;
			g_forceFeedbackEffects[2].remainingTicks = g_forceFeedbackEffects[2].durationTicks;
		}
	}
}
