#ifndef XWA_FRONTEND_MISSION_SETUP_H
#define XWA_FRONTEND_MISSION_SETUP_H

#include <stddef.h>
#include <stdint.h>

#include "xwa/frontend/frontend_rect.h"
#include "xwa/frontend/tech_library.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MissionDirectoryId {
	MISSION_DIRECTORY_MISSIONS = 0,
	MISSION_DIRECTORY_MELEE = 1,
	MISSION_DIRECTORY_COMBAT = 2,
	MISSION_DIRECTORY_SKIRMISH = 3,
	MISSION_DIRECTORY_TOUR = 4,
	MISSION_DIRECTORY_MISSIONS_RESERVED = 5
} MissionDirectoryId;

typedef enum MissionSetupSlotSummaryMode {
	MISSION_SETUP_SLOT_SUMMARY_CRAFT = 0,
	MISSION_SETUP_SLOT_SUMMARY_LOADOUT = 1,
	MISSION_SETUP_SLOT_SUMMARY_DUTY = 2
} MissionSetupSlotSummaryMode;

typedef enum MissionSetupActivePanel {
	MISSION_SETUP_PANEL_DEFAULT = 0,
	MISSION_SETUP_PANEL_TEAM_ASSIGNMENT = 1,
	MISSION_SETUP_PANEL_SELECTED_SLOT_EDIT = 2,
	MISSION_SETUP_PANEL_SAVE_SKIRMISH = 3,
	MISSION_SETUP_PANEL_SKIRMISH_GOALS = 4,
	MISSION_SETUP_PANEL_MISSION_LIST = 5,
	MISSION_SETUP_PANEL_BATTLE_SELECT = 6
} MissionSetupActivePanel;

typedef enum MissionSetupTransition {
	MISSION_SETUP_TRANSITION_NONE = 0,
	MISSION_SETUP_TRANSITION_CONCOURSE = 1,
	MISSION_SETUP_TRANSITION_JOIN_GAME = 2,
	MISSION_SETUP_TRANSITION_MISSION_BRIEFING = 3
} MissionSetupTransition;

typedef struct MissionListEntry {
	char fileName[64];
	char description[128];
	char sectionName[128];
	int missionIdx;
	int lockedFlag;
} MissionListEntry;

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define XWA_PACKED_STRUCT
#else
#define XWA_PACKED_STRUCT __attribute__((packed))
#endif

typedef struct XWA_PACKED_STRUCT CombatSimSlot {
	int16_t fgIndex;
	uint16_t craftType;
	int32_t ownerPlayerId;
	int32_t gunnerPlayerId;
	uint8_t numberOfWaves;
	uint8_t numberOfCraft;
	uint8_t groupAI;
	uint8_t craftRole;
	uint8_t warhead;
	uint8_t countermeasures;
	uint8_t beam;
	uint8_t primaryFg;
} CombatSimSlot;

typedef struct XWA_PACKED_STRUCT CombatSimLoadoutOptions {
	int32_t warheadOptions[10];
	int32_t warheadOptionCount;
	int32_t selectedWarheadOption;
	int32_t beamOptions[8];
	int32_t beamOptionCount;
	int32_t selectedBeamOption;
	int32_t countermeasureOptions[6];
	int32_t countermeasureOptionCount;
	int32_t selectedCountermeasureOption;
	int32_t optionalCraftCategory;
	int32_t craftOptionCount;
	int32_t selectedCraftOption;
	int32_t craftTypeOptions[11];
	int32_t unusedCraftTypeSlot;
	int32_t craftCountOptions[11];
	int32_t unusedCraftCountSlot;
	int32_t craftWaveOptions[11];
	int32_t unusedCraftWaveSlot;
} CombatSimLoadoutOptions;

typedef char xwa_combat_sim_slot_size[(sizeof(CombatSimSlot) == 0x14) ? 1 : -1];
typedef char xwa_combat_sim_loadout_options_size[(sizeof(CombatSimLoadoutOptions) == 0x114) ? 1 : -1];

typedef struct XWA_PACKED_STRUCT PilotMission {
	int m00;
	int numberTimesFlown;
	int firstPlaceCount;
	int secondPlaceCount;
	int thirdPlaceCount;
	int completedCount;
	int failedCount;
	int bestScore;
	unsigned int bestTime;
	unsigned int bestPlacement;
	unsigned int awardId;
	unsigned int bestBonus;
} PilotMission;

typedef struct XWA_PACKED_STRUCT PilotStats {
	int totalScorePerMT[3];
	int totalMissionsPlayedPerMT[3];
	int totalKillsPerMT[3];
	int totalFriendliesKilledPerMT[3];
	int killsPerCraftPerMT[3][512];
	int killsSharedPerCraftPerMT[3][512];
	int killsAssistsPerCraftPerMT[3][512];
	int killsFullOnPlayerRatingPerMT[3][25];
	int killsSharedOnPlayerRatingPerMT[3][25];
	int killsAssistOnPlayerRatingPerMT[3][25];
	int killsFullOnAIRatingPerMT[3][6];
	int killsSharedOnAIRatingPerMT[3][6];
	int killsAssistOnAIRatingPerMT[3][6];
	int numSpecialInspectedPerMT[3];
	int energyHitsPerMT[3];
	int energyFiredPerMT[3];
	int warheadsHitsPerMT[3];
	int warheadsFiredPerMT[3];
	int totalCraftLossesPerMT[3];
	int lossesByCollisionsPerMT[3];
	int lossesByStarshipsPerMT[3];
	int lossesByMinesPerMT[3];
	int killedByPlayerRatingPerMT[3][25];
	int killedByAIRatingPerMT[3][6];
} PilotStats;

typedef struct XWA_PACKED_STRUCT PilotNetworkPlayer {
	char formalName[14];
	char friendlyName[14];
	int flightGroupId;
	int m20;
	int directPlayId;
	int rating;
	int totalScore;
	int kills;
	int killsShared;
	int m38;
	int killsAssist;
	int totalLosses;
	int m44;
	int craftId;
	int warheadType;
	int beamType;
	int counterMeasuresType;
	int craftsCount;
	int wavesCount;
	int m60;
} PilotNetworkPlayer;

typedef char pilot_network_player_size[(sizeof(PilotNetworkPlayer) == 0x64) ? 1 : -1];
typedef char
	pilot_network_player_direct_play_id_offset[(offsetof(PilotNetworkPlayer, directPlayId) == 0x24) ? 1 : -1];
typedef char pilot_network_player_m60_offset[(offsetof(PilotNetworkPlayer, m60) == 0x60) ? 1 : -1];

typedef struct XWA_PACKED_STRUCT PilotTeam {
	int missionScore;
	int isMissionCompleted;
	int m08;
	int missionTime;
	int kills;
	int killsShared;
	int killsAssist;
} PilotTeam;

typedef struct XWA_PACKED_STRUCT PilotFaction {
	int totalMissionsPlayedCount;
	int team;
	int missionDirectoryId;
	int missionDescriptionIds[7];
	char gap_28[32];
	int m0048;
	int awardsCount1[6];
	int m0064[6];
	int awardsCount2[6];
	int m0094[6];
	int kalidorCrescentPoints[4];
	char gap_BC[12];
	int score;
	char gap_CC[4];
	int bonusScore;
	int totalScore;
	PilotStats stats;
} PilotFaction;

typedef struct XWA_PACKED_STRUCT PilotData {
	char name[14];
	int totalScore;
	char gap_seek8[8];
	int unk1;
	int numHumanPlayersLastMission;
	int gameMode;
	int team;
	int missionDirectoryId;
	int missionDescriptionIds[7];
	char multiplayerGameName[32];
	char multiplayerHostName[32];
	int unk2;
	int currentRatingPromoPoints;
	int currentRatingWorsePromoPoints;
	int newPromotion;
	int nextPromotionPercent;
	PilotStats mainStats;
	int missionScore;
	int killsFullOnPlayer[8];
	int killsSharedOnPlayer[8];
	int killsFullOnFlightGroup[192];
	int killsSharedOnFlightGroup[192];
	int killsFullFromPlayer[8];
	int killsSharedFromPlayer[8];
	int killsFullFromFlightGroup[192];
	int killsSharedFromFlightGroup[192];
	int flightGroupRating[192];
	PilotStats objectStats;
	PilotMission tourOfDutyMissions[255];
	PilotMission combatChamberMissions[255];
	uint8_t craftKnown[512];
	int newCraftAddedToTechRoom;
	int pilotRating;
	int pilotRank;
	int kalidorCresent;
	int totalMissionsPlayedCount;
	int totalMissionsPlayedCountPerRating[25];
	char pilotRatingName[32];
	int emkayAnnounceNewAward;
	int emkayAnnounceNewRank;
	int familyNewMedal;
	int tacOfficerAnnounceNewRank;
	PilotNetworkPlayer networkPlayers[8];
	PilotTeam teamsStatistics[10];
	int currentFactionId;
	PilotFaction factionStatistics[4];
	uint8_t emailsStatus[100];
	uint8_t emailsSortCriterion;
	int campaignMode;
	int skipMissionsRemaining;
	int regionsCount;
	char missionFileName[256];
	uint8_t provingGroundsMissionPerPlayer[8];
	int meleeMissionIndex;
	uint8_t hangarType;
} PilotData;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#undef XWA_PACKED_STRUCT

extern MissionListEntry* g_missionList;
extern int g_missionCount;
extern int g_selectedMissionListIndex;
extern int g_missionSetupShipBmpGroupId;
extern const char* g_campaignDirNames[6];
extern PilotData g_pilotData;
extern CombatSimSlot g_combatSimSlots[16];
extern CombatSimLoadoutOptions g_combatSimLoadoutOptions[16];
extern char g_combatSimSlotNames[16][20];
extern CombatSimSlot g_missionSetupSlotEditBackup;
extern int g_missionSetupSlotEditBackupValid;
extern CombatSimSlot g_selectedCombatSimSlot;
extern int g_selectedCombatSimSlotIdx;
extern char g_combatSimSkirmishFileName[64];
extern CraftTechStats* g_cachedCraftTechStats;
extern int g_cachedCraftTechStatsCount;
extern int g_missionSetupRosterAuthoritative;
extern int g_missionSetupDraggedPlayerId;
extern MissionSetupSlotSummaryMode g_missionSetupSlotSummaryMode;
extern MissionSetupActivePanel g_missionSetupActivePanel;
extern int g_missionSetupBattleFirstMissionIdx;
extern int g_missionSetupBattleLastMissionIdx;
extern int g_missionSetupBattleSelectableMissionCount;
extern int g_missionSetupBattleSelectedMissionOrdinal;
extern int g_missionSetupMissionListScrollOffset;
extern int g_missionSetupMissionListRowCount;
extern int g_missionSetupSelectedMissionListIndex;
extern int g_missionSetupSubpanelMode;
extern int g_missionSetupPendingRandomSeed;
extern int g_skirmishFileRandomSeed;
extern int g_missionSetupUseCombatSimPilotState;
extern int g_missionSetupSkirmishSeedInitialized;
extern int frame;
extern int g_teamCount;
extern int g_teamFgCountScratch[10];
extern int g_missionSetupConnectionStatsHoverActive;
extern int g_missionSetupPlayerDragState;
extern int g_missionSetupRemoteTeamGoalType;
extern char g_missionSetupSaveNameBuffer[64];
extern char g_selectedCombatSimSlotNameEditBuffer[32];
extern int g_missionSetupCarouselSlideOffset;
extern int g_missionSetupCarouselQueuedSlideOffset;
extern int g_missionSetupCraftSelectionChangedFlag;
extern int g_missionSetupCraftListScrollOffset;
extern int g_missionSetupDisplayedCraftOrdinal;
extern int g_missionSetupFilteredCraftCount;
extern int g_missionSetupSelectedCraftCategory;
extern int g_missionSetupTourButtonEnabled;
extern char g_missionSetupCurrentBattleSectionName[128];
extern MissionSetupTransition g_missionSetupPendingTransition;
extern int g_unusedMissionSetupCarouselPanelColor;
extern int g_missionSetupJoinBroadcastCooldownFrames;
extern int g_frontendMissionInitClearedDword;
extern int g_unusedMissionSetupInitDword7830B8;
extern int g_unusedMissionSetupMissionListIndexLatch;
extern uint32_t g_missionSetupLastHostBroadcastTick;
extern int g_missionSetupParsedMissionNumber;

int MissionSetup_CountMissionListEntries(void* stream);
int MissionSetup_IsSkirmishMeleeFile(const char* fileName);
int MissionSetup_CompareMissionListEntries(const void* left, const void* right);
void MissionSetup_LoadMissionList(MissionDirectoryId missionDirectoryId);
void MissionSetup_LoadMissionDescText(char* outText4096);
void MissionSetup_CountActiveTeams(void);
int MissionSetup_RebuildCombatSimSlotsFromFrontendMission(void);
int MissionSetup_GetCachedCraftTechStats(CraftTechStats* stats);
int MissionSetup_ComputeCraftPointTotal(int craftType, int warheadType, int beamType, int countermeasureType,
										int groupAI, int includeLoadoutAndAi);
int MissionSetup_GetTeamCraftPointTotal(int team);
int MissionSetup_IsSlotWithinPointLimit(int slotIndex);
int MissionSetup_CanSelectSlotCraft(CombatSimSlot* slot);
int MissionSetup_SelectedSlotHasChanges(void);
int MissionSetup_SyncSlotLoadoutSelection(int slotIdx);
int MissionSetup_AreReadyPlayersAssignedToSlots(void);
int MissionSetup_FormatCombatSimSlotSummaryText(int slotIdx);
int MissionSetup_LoadShipBmpForCraftType(int craftType);
int MissionSetup_UnloadShipBmp(void);
int MissionSetup_LoadBattleSprites(void);
int MissionSetup_FreeBattleSprites(void);
int MultiplayerSetup_FreeTransportSprites(void);
int MissionSetup_DrawBackgroundAndPreview(int drawToCurrentSurface);
int MissionSetup_BroadcastLobbySelectionPacket(void);
int MissionSetup_BroadcastStatePacket(int stateKind);
int MissionSetup_BroadcastSkirmishMetadata(void);
int MissionSetup_BroadcastGameConfigPacket(void);
int MissionSetup_BroadcastReadyPlayerRosterPacket(int toPlayerId);
int MissionSetup_LoadSkirmishFile(const char* fileName, int updateCurrentName);
int MissionSetup_SaveSkirmishFile(const char* baseName, int interactiveSave);
#ifdef XWA_MODERN
int MissionSetup_UpdateSaveSkirmishDialog(void);
#else
int MissionSetup_UpdateSaveSkirmishDialog();
#endif
int MissionSetup_IsSkirmishSetupValid(void);
int MissionSetup_DrawMissionDescriptionPanel(void);
int MissionSetup_DrawPlayerConnectionStats(FrontendRect* rect, int playerId);
int MissionSetup_DrawUnassignedPlayersPanel(int panelMode);
#ifdef XWA_MODERN
int MissionSetup_DrawSkirmishGoalTypePanel(void);
#else
int MissionSetup_DrawSkirmishGoalTypePanel();
#endif
int MissionSetup_DrawTeamAssignmentPanel(int panelMode);
int MissionSetup_DrawFlightGroupAssignmentPanel(int panelMode);
int MissionSetup_DrawSelectedSlotEditPanel(int panelMode);
int MissionSetup_DrawMissionNavigationPanel(int panelMode);
#ifdef XWA_MODERN
int MissionSetup_DrawMissionListPanel(void);
#else
int MissionSetup_DrawMissionListPanel();
#endif
int MissionSetup_DrawGameSettingsPanel(void);
int MissionSetup_DrawMissionTypeControls(void);
void MissionSetup_BuildBriefingText(char* outText);
int MissionSetup_Update(int frameCounter);
int MissionSetup_Exit(int frameCounter);

#ifdef __cplusplus
}
#endif

#endif
