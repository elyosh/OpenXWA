#ifndef XWA_CONFIG_GAME_CONFIG_H
#define XWA_CONFIG_GAME_CONFIG_H

#include "xwa/assets/ui_string.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GameConfig {
	uint8_t backdrop[2];
	uint8_t starDensity[2];
	uint8_t debris[2];
	uint8_t localLights[2];
	uint8_t specular[2];
	uint8_t diffuse[2];
	uint8_t dither[2];
	uint8_t textureRes[2];
	uint8_t mipmap[2];
	uint8_t lod[2];
	uint8_t yardLod[2];
	uint8_t screenRes[2];
	uint8_t bpp[2];
	uint8_t brightness[2];
	uint8_t use3dHardware[2];
	uint8_t bilinear[2];
	uint8_t hitEffects[2];
	uint8_t engineGlow[2];
	uint8_t lensFlare[2];
	uint8_t threedDevice[2];
	uint8_t hudColor[2];
	uint8_t hardwareMipmap[2];
	uint8_t palettizedTextures[2];
	uint8_t debrisDensity[2];
	uint8_t particleEffects[2];
	uint8_t trails[2];
	uint8_t explosionRes[2];
	uint8_t networkType;
	char phoneNumber[64];
	char ipAddress[64];
	char lastPilotName[14];
	char password[16];
	uint8_t asyncFlag;
	uint8_t serverUpdateRate;
	uint8_t firewall;
	uint16_t portNumber;
	uint8_t comPort;
	uint8_t baudRate;
	uint8_t stopBits;
	uint8_t parity;
	uint8_t flowControl;
	uint8_t curModem;
	uint8_t sfxExteriorEnabled;
	uint8_t sfxInteriorEnabled;
	uint8_t sfxEngineEnabled;
	uint8_t voicePilotEnabled;
	uint8_t voiceTacticalOfficerEnabled;
	uint8_t voiceCommanderEnabled;
	uint8_t voiceSpecialEnabled;
	uint8_t sfxDatapadVolume;
	uint8_t sfxExteriorVolume;
	uint8_t sfxInteriorVolume;
	uint8_t sfxEngineVolume;
	uint8_t voiceVolume;
	uint8_t musicEnabled;
	uint8_t musicVolume;
	uint8_t datapadMusicEnabled;
	uint8_t datapadMusicVolume;
	uint8_t sfxQuality;
	uint16_t joyButtons[20];
	uint8_t tourDifficulty;
	uint8_t tourCollisions;
	uint8_t difficulty;
	uint8_t collisions;
	uint8_t craftJumping;
	uint8_t requirePassword;
	uint8_t inProgressJoin;
	uint8_t craftSelection;
	uint8_t locatePlayers;
	uint8_t laps;
	uint8_t lastTeamTimeLimit;
	int32_t randomSeed;
	uint8_t aiOpponents;
	uint8_t eachTeamOwnRegion;
	uint8_t environment;
	uint8_t numberOfTeams;
	uint8_t initialDistance;
	uint8_t maxPoints;
	char gap_12E[1];
	uint8_t invulnerable;
	uint8_t unlimitedAmmo;
	uint8_t tourInvulnerable;
	uint8_t tourUnlimitedAmmo;
	uint8_t timeLimit;
	uint8_t sfxDatapadEnabled;
	uint8_t helpOn;
	uint8_t rudderEnabled;
	uint8_t flipRudder;
	uint8_t ffStrength;
	uint8_t ffCenter;
	uint8_t flipY;
	uint8_t sound3dEnabled;
	uint8_t ffEnabled;
	uint8_t numberOfSfx;
	uint8_t goalType;
	uint8_t teamGoals[10];
	char taunt1[70];
	char taunt2[70];
	char taunt3[70];
	char taunt4[70];
	uint8_t presetThrottle[2];
	uint8_t presetLaser[2];
	uint8_t presetShield[2];
	uint8_t presetBeam[2];
	uint8_t performance;
} GameConfig;

extern GameConfig g_gameConfig;
extern int g_configRestrictedOptionsModalActive;
extern int g_configDatapadQuitConfirmed;
extern int g_configAuxScreenFrameCount;
extern int g_frontendSetupNeedsBaseRedraw;

int Config_SetDetailDefaultsLow(char groupMask);
int Config_SetDetailDefaultsMedium(char groupMask);
int Config_SetDetailDefaultsHigh(char groupMask);
int Config_OptionsDatapadUpdate(int frameState);
int Config_DrawBackgroundToScreens(void);
int Config_RunRestrictedOptionsModal(void);
int Config_GetMenuNavKey(void);
int Config_MainMenuScreen(void);
int Config_GeneralOptionsScreen(void);
int Config_SoundOptionsScreen(void);
int Config_ControllerOptionsScreen(void);
int Config_JoystickRemapScreen(void);
int Config_SinglePlayerVideoOptionsScreen(void);
int Config_MultiplayerVideoOptionsScreen(void);
int Config_SinglePlayerSoftwareVideoScreen(void);
int Config_MultiplayerSoftwareVideoScreen(void);
int Config_CreditsScreen(void);
int Config_CutsceneViewerScreen(void);
int Config_DrawOptionCycle(uint8_t* value, UIString labelId, int valueBaseStrId, int optionCount, int* y,
						   int* rowIndex, char* keyState, int buttonId);
int Config_DrawOptionCycleDisabled(uint8_t* value, UIString labelId, int valueBaseStrId, int optionCount,
								   int* y, int* rowIndex, char* keyState, int buttonId);
int Config_DrawOptionCycleImpl(uint8_t* value, UIString labelId, int valueBaseStrId, int optionCount, int* y,
							   int* rowIndex, char* keyState, int buttonId, int disabled);
int Config_DrawOptionNumericStepper(uint8_t* value, UIString labelId, unsigned int fontSize, int maxValue,
									int* y, int* rowIndex, char* keyState, int buttonId);
int Config_DrawOptionSlider(uint8_t* value, UIString labelId, UIString rangeLabelId, int notchCount, int* y,
							int* rowIndex, char* keyState, int buttonId);
int Config_DrawOptionSliderDisabled(uint8_t* value, UIString labelId, UIString rangeLabelId, int notchCount,
									int* y, int* rowIndex, char* keyState, int buttonId);
int Config_DrawOptionSliderImpl(uint8_t* value, UIString labelId, UIString rangeLabelId, int notchCount,
								int* y, int* rowIndex, char* keyState, int buttonId, int disabled);
int Config_DrawSoftwareVideoAdvancedRows(int profileIdx, int* y, int* rowIndex, char* keyState);
int Config_DrawOptionTextEdit(char* text, unsigned int maxLen, int unused, char* label, int* y, int* rowIndex,
							  char* keyState, int buttonId);
int Config_DrawHardwareVideoAdvancedRows(int profileIdx, int* y, int* rowIndex, char* keyState);
int Config_DrawVideoOptionRows(int profileIdx, int* y, int* rowIndex, char* keyState);
int Config_SinglePlayerHardwareVideoScreen(void);
int Config_MultiplayerHardwareVideoScreen(void);
int Config_PerformanceOptionsScreen(void);
int Config_NetworkOptionsScreen(void);
int Config_VideoOptionsMenu(void);
int Config_DrawJoystickBindingRow(uint16_t* boundActionCode, char* label, int* y, int* rowIndex,
								  char* keyState, int buttonId);
int Config_LoadJoystickActionDictionary(void);
int Config_RunJoystickActionPicker(const char* title, uint16_t* boundActionCode);
int Config_JoystickActionPickerUpdate(int frameState);
uint16_t Config_ReadJoystickActionPickerKey(void);
int Config_GetSinglePlayerHardware3D(void);
int Config_SetSinglePlayerHardware3D(uint8_t value);
int Config_GetDisplayDriverIndex(int profileIdx);
int Config_Load(void);
int Config_Write(void);

#ifdef __cplusplus
}
#endif

#endif
