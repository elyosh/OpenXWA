#ifndef XWA_FLIGHT_PLAYER_PLAYER_H
#define XWA_FLIGHT_PLAYER_PLAYER_H

#include "xwa/flight/object/object.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { XWA_PLAYER_COUNT = 8 };

#pragma pack(push, 1)
#define XWA_PLAYER_PACKED_STRUCT
#if defined(__GNUC__) || defined(__clang__)
#undef XWA_PLAYER_PACKED_STRUCT
#define XWA_PLAYER_PACKED_STRUCT __attribute__((packed))
#endif

typedef uint8_t PlayerHyperspacePhase;
enum {
	PLAYER_HYPERSPACE_PHASE_NONE = 0,
	PLAYER_HYPERSPACE_OUTBOUND = 2,
	PLAYER_HYPERSPACE_INBOUND = 3,
	PLAYER_HYPERSPACE_REGION_TRANSFER = 4,
};

typedef struct XWA_PLAYER_PACKED_STRUCT MfdCommandMenuState {
	uint8_t gap0[2];
	uint8_t commandableTargetCount;
	uint8_t selectedTargetSlot;
	uint8_t targetSlotValid[7];
	int primaryTargetObjIdx[7];
	int secondaryTargetObjIdx[7];
	uint8_t gap43[12];
	uint8_t secondaryTargetCount;
	uint16_t primaryTargetSignature[7];
	uint16_t secondaryTargetSignature[7];
	uint8_t gap6C[6];
} MfdCommandMenuState;

typedef struct XWA_PLAYER_PACKED_STRUCT MfdState {
	uint8_t enabled[3];
	char savedSideEnabled[2];
	uint8_t consolePageAvailable;
	uint8_t activeIndex;
	char savedActiveIndex;
	uint8_t page[3];
	char savedPage[3];
	uint8_t menuRow;
	uint8_t menuItem;
	MfdCommandMenuState commandMenu;
	uint8_t reinforcementCommandAvailable;
} MfdState;

typedef struct XWA_PLAYER_PACKED_STRUCT PlayerMissionRuntimeStats {
	int missionScore;
	int missionBonusScoreTenths;
	int ratingPromoPoints;
	int worseRatingPromoPoints;
	int field10;
	int field14;
	int field18;
	uint16_t laserShotsFired;
	uint16_t laserHitsScored;
	uint16_t ionShotsFired;
	uint16_t ionHitsScored;
} PlayerMissionRuntimeStats;

typedef struct XWA_PLAYER_PACKED_STRUCT PerMissionKills {
	uint16_t warheadHits;
	uint16_t field2;
	uint16_t numSpecialInspected;
	uint16_t killsFullOnFlightGroup[192];
	uint16_t killsSharedOnFlightGroup[192];
	uint16_t killsAssistOnFlightGroup[192];
	uint16_t killsFullOnPlayerRating[25];
	uint16_t killsSharedOnPlayerRating[25];
	uint16_t killsAssistOnPlayerRating[25];
	uint16_t killsFullOnAiRating[6];
	uint16_t killsSharedOnAiRating[6];
	uint16_t killsAssistOnAiRating[6];
	uint16_t killsFullOnPlayer[8];
	uint16_t killsSharedOnPlayer[8];
	uint16_t friendliesKilled;
	uint16_t totalCraftLosses;
	uint16_t lossesByCollisions;
	uint16_t lossesByStarships;
	uint16_t lossesByMines;
	uint16_t killsFullFromPlayer[8];
	uint16_t killsSharedFromPlayer[8];
	uint16_t killsFullFromFlightGroup[192];
	uint16_t killsSharedFromFlightGroup[192];
	uint16_t killedByPlayerRating[25];
	uint16_t killedByAiRating;
} PerMissionKills;

typedef struct XWA_PLAYER_PACKED_STRUCT PlayerNetworkRuntimeTail {
	uint16_t flightResolutionMode;
	int directPlayId;
} PlayerNetworkRuntimeTail;

typedef struct XWA_PLAYER_PACKED_STRUCT PlayerHyperspaceRuntime {
	uint8_t targetRegionOrMode;
	uint8_t hyperBuoyPromptCooldown;
	uint8_t regionTransferArrivalCounted[5];
	uint32_t phaseElapsedTicks;
	uint8_t targetBoxEnabled;
} PlayerHyperspaceRuntime;

typedef struct XWA_PLAYER_PACKED_STRUCT PlayerViewState {
	int savedTargetX;
	int savedTargetY;
	int savedTargetZ;
	int cameraFocusObjIdx;
	int aimTargetIdx;
	uint16_t viewPitch;
	uint16_t viewYaw;
	uint16_t viewRoll;
	uint16_t viewAngleD;
	uint16_t hudAimX;
	uint16_t hudAimY;
	int cameraPanDeltaX;
	int cameraPanDeltaY;
	int cameraPanDeltaZ;
	uint16_t cameraPitchDelta;
	uint16_t cameraYawDelta;
	uint16_t cameraRollDelta;
	uint16_t field_32;
	uint8_t hudStateLive;
	uint8_t hudStateMirror;
	uint8_t hudAimXSnapState;
	uint8_t savedHudStateByte;
	uint8_t gap_38;
	uint16_t savedHudAimX;
	uint16_t savedHudAimY;
	uint16_t playerInputBlocked;
	uint16_t cameraDistanceStep;
	uint16_t externalCameraActive;
	int cameraDistance;
	uint16_t transitionTimer;
	uint16_t transitionDuration;
	uint16_t gap_4B;
} PlayerViewState;

typedef struct XWA_PLAYER_PACKED_STRUCT PlayerData {
	int objectIndex;
	unsigned int boundObjectSignature;
	uint16_t pilotRating;
	int16_t iff;
	int16_t playerIff;
	uint16_t boundFlightGroupIdx;
	uint8_t regionIndex;
	uint8_t connectedFlag;
	uint8_t regionSessionId;
	uint8_t inputDisabledFlag;
	// Copy of the bound craft model's engine-glow count, written at spawn bind;
	// never read anywhere in the binary (vestigial).
	uint8_t boundCraftEngineGlowCount;
	uint8_t mapCameraState;
	uint8_t aiControlledFlag;
	PlayerHyperspacePhase hyperspacePhase;
	PlayerHyperspaceRuntime hyperspaceRuntime;
	uint8_t missileLockState;
	uint16_t currentTargetObjectIdx;
	int16_t targetSubState;
	int16_t targetCycleStart;
	int16_t targetPresetSlot[4];
	uint8_t selectedWarhead;
	uint8_t selectedWeaponMode;
	int16_t selectedTargetComponent;
	int16_t targetingState;
	ObjectIndex engineWashSourceObjIdx;
	uint16_t engineWashStrength;
	int16_t throttlePreset[2];
	uint8_t laserPreset[2];
	uint8_t shieldPreset[2];
	uint8_t beamPreset[2];
	char savedCraftSettingsRaw[11];
	uint8_t pendingActionId;
	int16_t pendingActionParam;
	uint16_t pendingActionIssuerPlayerIdx;
	int16_t gap71_field16;
	int16_t smoothedInputYaw;
	int16_t smoothedInputPitch;
	int16_t smoothedInputRoll;
	uint16_t savedKeyMods;
	uint16_t keyModsHoldTimer;
	uint8_t yawRollSwap;
	uint8_t hudEnabled;
	uint8_t savedHudEnabled;
	MfdState mfd;
	char consoleInputBuf[256];
	uint8_t consoleKeyDelay;
	int16_t consoleInputLen;
	int altViewObjectIdx;
	uint8_t mfdCommandMenuItemCount[9];
	uint8_t cockpitVisible;
	uint8_t cockpitLookAvailable;
	char cockpitToggleAvailable;
	uint8_t padlockActive;
	int16_t lookYawOffset;
	int16_t lookPitchOffset;
	float hardpointWorldX;
	float hardpointWorldY;
	float hardpointWorldZ;
	float hardpointLocalX;
	float hardpointLocalY;
	float hardpointLocalZ;
	int16_t currentSeatIdx;
	int16_t turretAutoFireState;
	uint8_t gunnerHardpointToggle;
	int16_t turretCamMat[9];
	PlayerMissionRuntimeStats missionStats;
	uint16_t warheadsFired;
	PerMissionKills perMissionKills;
	char msgText[50];
	uint8_t msgLength;
	uint8_t msgTypeId;
	PlayerViewState viewState;
	PlayerNetworkRuntimeTail network;
	int lockstepTimestamp;
	int savedX;
	int savedY;
	int savedZ;
	int16_t savedRoll;
	int16_t savedPitch;
	int16_t savedYaw;
	int savedLifetimeTimer;
	int16_t savedSpeed;
	int16_t savedSpeedRemainder;
	int16_t savedRollImpulseRate;
	int16_t savedFieldId;
	uint8_t savedRegion;
	int pendingActionTimer;
	int beamFireCooldownTimer;
	int field_BC6;
	int impactDamageCooldownTime;
	uint8_t hasCheckpointFlag;
} PlayerData;

typedef char xwa_mfd_command_menu_state_size[(sizeof(MfdCommandMenuState) == 0x72) ? 1 : -1];
typedef char xwa_mfd_state_size[(sizeof(MfdState) == 0x83) ? 1 : -1];
typedef char xwa_player_hyperspace_runtime_size[(sizeof(PlayerHyperspaceRuntime) == 0xc) ? 1 : -1];
typedef char xwa_player_mission_runtime_stats_size[(sizeof(PlayerMissionRuntimeStats) == 0x24) ? 1 : -1];
typedef char xwa_per_mission_kills_size[(sizeof(PerMissionKills) == 0x8be) ? 1 : -1];
typedef char xwa_player_network_runtime_tail_size[(sizeof(PlayerNetworkRuntimeTail) == 0x6) ? 1 : -1];
typedef char xwa_player_view_state_size[(sizeof(PlayerViewState) == 0x4d) ? 1 : -1];
typedef char xwa_player_data_size[(sizeof(PlayerData) == 0xbcf) ? 1 : -1];

#pragma pack(pop)
#undef XWA_PLAYER_PACKED_STRUCT

extern PlayerData g_players[XWA_PLAYER_COUNT];
extern PlayerData g_localPlayerSnapshotOnFlightExit;
extern PlayerViewState g_filmOverlayViewState;
extern PlayerViewState g_savedPlayerViewStateForPlaybackCamera;
extern int g_localPlayer;
extern uint8_t g_padlockMouseLookEnabled;
extern uint8_t g_padlockMouseLookInvertPitch;
extern uint8_t g_padlockMouseLookIgnoreNextDelta;

void Player_EmitRemotePlayerDepartedMessages(unsigned int playerIdx);
void Player_ReleaseCarriedObject(unsigned int playerIdx);
void Player_AutoGunnerToggle(unsigned int playerIdx);
void Player_HandlePickupCommand(unsigned int playerIdx);
void Player_HandleDockBoardCommand(unsigned int playerIdx);
void Player_HandleReportInCommand(int playerIdx, int targetObjIdx);
void Player_HandleResupplyCommand(int playerIdx, int targetObjIdx);
void Player_HandleEvadeCommand(int playerIdx, int targetObjIdx);
void Player_HandleHyperspaceCommand(struct CraftData* craft, unsigned int playerIdx, char hyperMode);
void Player_IssueAiWingmanTargetOrder(uint16_t targetObjIdx, int wingmanObjIdx, int16_t hudMessageId,
									  uint16_t voiceVariant, int playerIdx);
void Player_HandleCoverMeCommand(int playerIdx, int targetObjIdx);
int Player_UnbindFromCurrentCraft(int playerIdx, int requireAnotherOwnedCraft, int restoreMissionAiPlan);
int Player_BindToAvailableCraft(unsigned int playerIdx, unsigned int previousObjIdx,
								unsigned int preferredObjectSignature, int resetTargetState);
int Player_FindLinkedGunnerForFlightGroup(int playerIdx);
int Player_FindNearestEnemyFighter(int playerIdx, int excludeObjIdx);
int16_t Player_FindAttackerOfTarget(uint16_t targetObjIdx, int16_t playerObjIdx);
uint16_t Player_PickTargetInSight(int playerIdx);
void Player_StartPostDestructionState(unsigned int playerIdx, unsigned int killerObjIdx, int killerPlayerIdx);
int Player_HandleCraftDestruction(unsigned int playerIdx);
void Player_UpdateParticipationState(void);
void Player_EndFlightParticipation(int playerIdx);
int Player_AppendKillMessageActorName(unsigned int msgSlot, char* nameBuffer, int objIdx);
void Player_SaveCraftSettings(int playerIdx);
void Player_UpdateHudViewForCameraFocus(int playerIdx);
int Player_ResetGunnerSeatCameraState(unsigned int playerIdx, int objectIndex, int resetFlags);
void Player_CycleGunnerSeat(int playerIdx, void* forcePilotFlag);
void Player_StepExtView(int playerIdx);
void Player_ComputePolarToObjectRef(int playerIdx, unsigned int objectRef);
int8_t Player_CanRadioCommandCraft(unsigned int targetObjIdx, int playerIdx);
void Player_SetTarget(uint16_t newTargetObjIdx, unsigned int playerIdx);
void Player_ValidateCurrentTargets(int playerIdx);
void Player_ValidateAllCurrentTargets(void);
uint16_t Player_CycleTargetAnyIFF(int startObjIdx, int direction, int playerIdx);
uint16_t Player_CycleTarget(int startObjIdx, int direction, int playerIdx, int iffCategory, char modeFlags);
int16_t Player_FindNearestObjective(int goalCondType, int playerIdx);
int Player_HasAvailableOwnedCraft(int playerIdx);
void Player_TransferShieldBankEnergy(uint16_t dstBank, uint16_t srcBank, int playerIdx);

#ifdef __cplusplus
}
#endif

#endif
