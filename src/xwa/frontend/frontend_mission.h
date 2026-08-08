#ifndef XWA_FRONTEND_FRONTEND_MISSION_H
#define XWA_FRONTEND_FRONTEND_MISSION_H

#include <stddef.h>
#include <stdint.h>

#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FrontendMission FrontendMission;
struct FrontendBriefingContent;

typedef enum XwaMissionType {
	XWA_MISSION_TYPE_JUNKYARD = 0,
	XWA_MISSION_TYPE_SIMULATOR_1 = 1,
	XWA_MISSION_TYPE_QUICK_START = 2,
	XWA_MISSION_TYPE_SIMULATOR_2 = 3,
	XWA_MISSION_TYPE_SKIRMISH = 4,
	XWA_MISSION_TYPE_DEATH_STAR = 5,
	XWA_MISSION_TYPE_ALLIANCE_CAMPAIGN = 6,
	XWA_MISSION_TYPE_FAMILY_CAMPAIGN = 7,
} XwaMissionType;

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define XWA_MISSION_PACKED_STRUCT
#else
#define XWA_MISSION_PACKED_STRUCT __attribute__((packed))
#endif

typedef uint8_t TeamAllegianceByte;

typedef struct XWA_MISSION_PACKED_STRUCT XwaTrigger {
	uint8_t condition;
	uint8_t variableType;
	uint8_t variable;
	uint8_t amount;
	int16_t parameter;
} XwaTrigger;

typedef struct XWA_MISSION_PACKED_STRUCT XwaTriggerPair {
	XwaTrigger triggers[2];
	uint8_t unused[2];
	uint8_t t1OrT2;
	uint8_t ioReserved;
} XwaTriggerPair;

typedef struct XWA_MISSION_PACKED_STRUCT XwaWaypoint {
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t enabled;
} XwaWaypoint;

typedef enum XwaFlightGroupMissionPoint {
	XWA_FG_POINT_START_1 = 0,
	XWA_FG_POINT_START_2 = 1,
	XWA_FG_POINT_CAPTURE_HYPER = 2,
	XWA_FG_POINT_HYPER = 3,
	XWA_FG_POINT_COUNT = 4,
} XwaFlightGroupMissionPoint;

typedef struct XWA_MISSION_PACKED_STRUCT XwaFlightGroupGoalPayload {
	uint8_t argument;
	uint8_t condition;
	uint8_t amount;
	int8_t points;
	uint8_t enabledForTeam[10];
	uint8_t parameter;
	uint8_t activeSequence;
} XwaFlightGroupGoalPayload;

typedef struct XWA_MISSION_PACKED_STRUCT XwaGoalFG {
	XwaFlightGroupGoalPayload payload;
	uint8_t reserved[64];
} XwaGoalFG;

typedef enum XwaOrderSecondaryTarget {
	XWA_ORDER_TARGET_3 = 0,
	XWA_ORDER_TARGET_4 = 1,
	XWA_ORDER_SECONDARY_TARGET_COUNT = 2,
} XwaOrderSecondaryTarget;

typedef struct XWA_MISSION_PACKED_STRUCT XwaOrder {
	uint8_t order;
	uint8_t throttle;
	uint8_t variable1;
	uint8_t variable2;
	uint8_t variable3;
	uint8_t variable4;
	uint8_t secondaryTargetTypes[XWA_ORDER_SECONDARY_TARGET_COUNT];
	uint8_t secondaryTargets[XWA_ORDER_SECONDARY_TARGET_COUNT];
	uint8_t target3OrTarget4;
	uint8_t unused0;
	uint8_t target1Type;
	uint8_t target1;
	uint8_t target2Type;
	uint8_t target2;
	uint8_t target1OrTarget2;
	uint8_t unused1;
	uint8_t speed;
	uint8_t ioReserved;
	XwaWaypoint waypoints[8];
	uint8_t reserved[64];
} XwaOrder;

typedef struct XWA_MISSION_PACKED_STRUCT XwaFlightGroup {
	char name[20];
	uint8_t enableDesignation;
	uint8_t enableDesignation2;
	uint8_t designation1;
	uint8_t designation2;
	uint8_t comm;
	uint8_t globalCargoIndex;
	uint8_t globalSpecialCargoIndex;
	uint8_t unused0x1B[13];
	char cargo[20];
	char specialCargo[20];
	char craftRole[25];
	uint8_t specialCargoCraft;
	uint8_t randomSpecialCargoCraft;
	uint8_t craftType;
	uint8_t numberOfCraft;
	uint8_t status1;
	uint8_t warhead;
	uint8_t beam;
	uint8_t iff;
	uint8_t team;
	uint8_t groupAI;
	uint8_t markings;
	uint8_t radio;
	uint8_t unused0x75;
	uint8_t formation;
	uint8_t formationSpacing;
	uint8_t globalGroup;
	uint8_t unused0x79;
	uint8_t numberOfWaves;
	uint8_t wavesDelay;
	uint8_t stopArrivingWhen;
	uint8_t playerNumber;
	uint8_t arriveOnlyIfHuman;
	uint8_t playerCraft;
	uint8_t yaw;
	uint8_t pitch;
	uint8_t roll;
	uint8_t ioReserved0x83;
	int16_t linkId;
	uint8_t unused0x86;
	uint8_t arrivalDifficulty;
	XwaTriggerPair arrival[2];
	uint8_t arrivals12OrArrivals34;
	uint8_t arrivalRandDelayMinutes;
	uint8_t arrivalDelayMinutes;
	uint8_t arrivalDelaySeconds;
	XwaTriggerPair departure;
	uint8_t departureDelayMinutes;
	uint8_t departureDelaySeconds;
	uint8_t abortTrigger;
	uint8_t arrivalRandDelaySeconds;
	int16_t editorMothership;
	uint8_t arrivalMothership;
	uint8_t arrivalMethod;
	uint8_t departureMothership;
	uint8_t departMethod;
	uint8_t alternateMothership;
	uint8_t alternateMothershipUsed;
	uint8_t capturedDepartureMothership;
	uint8_t capturedDepartViaMothership;
	XwaOrder orders[16];
	XwaTriggerPair skipTriggers[16];
	XwaGoalFG fgGoals[8];
	XwaWaypoint missionPoints[XWA_FG_POINT_COUNT];
	uint8_t missionPointRegions[XWA_FG_POINT_COUNT];
	uint8_t unused0xDAE[20];
	uint8_t editorWaypointShown;
	uint8_t unused0xDC3;
	uint8_t disableWaveNumbering;
	uint8_t departureClockMin;
	uint8_t departureClockSec;
	uint8_t countermeasures;
	uint8_t craftExplosionTime;
	uint8_t status2;
	uint8_t globalUnit;
	uint8_t handicap;
	uint8_t optionalWarheads[8];
	uint8_t optionalBeams[6];
	uint8_t optionalCountermeasures[4];
	uint8_t optionalCraftCategory;
	uint8_t optionalCraft[10];
	uint8_t numberOfOptionalCraft[10];
	uint8_t numberOfOptionalCraftWaves[10];
	char pilotID[20];
	uint8_t ioReservedE11;
	int32_t backdrop;
	uint8_t reservedE16[40];
} XwaFlightGroup;

typedef struct XWA_MISSION_PACKED_STRUCT XwaMessage {
	char message[80];
	uint8_t sentToTeam[10];
	XwaTriggerPair triggers[2];
	char voice[8];
	int32_t originatingFG;
	int32_t type;
	uint8_t rawDelay;
	uint8_t triggers12OrTriggers34;
	uint8_t colorIff;
	uint8_t speakerHeader;
	XwaTriggerPair special;
	uint8_t specialMeaning;
	uint8_t ioReserved;
} XwaMessage;

typedef struct XWA_MISSION_PACKED_STRUCT XwaGlobalGoal {
	XwaTriggerPair triggerPairs[2];
	char name[16];
	uint8_t version;
	uint8_t t12AndOrT34;
	uint8_t rawDelay;
	int8_t rawPoints;
	int8_t rawPointsPerTrigger[4];
	uint8_t activeSequence;
	uint8_t reserved[65];
} XwaGlobalGoal;

typedef struct XWA_MISSION_PACKED_STRUCT XwaRegion {
	char name[64];
	int32_t id;
	uint8_t reserved[64];
} XwaRegion;

typedef struct XWA_MISSION_PACKED_STRUCT XwaGlobalCargo {
	char name[64];
	int32_t id;
	int32_t count;
	uint8_t type;
	uint8_t volume;
	uint8_t value;
	uint8_t volatility;
	uint8_t reserved[64];
} XwaGlobalCargo;

typedef struct XWA_MISSION_PACKED_STRUCT XwaGlobalUnit {
	char name[64];
	uint8_t leader;
	uint8_t specialCargoCraft;
	char specialCargo[20];
	uint8_t randomSpecialCraft;
} XwaGlobalUnit;

typedef struct XWA_MISSION_PACKED_STRUCT XwaMissionHeaderBody {
	uint8_t legacyTimeLimitMin;
	uint8_t legacyTimeLimitSec;
	uint8_t legacyWinType;
	uint8_t legacyBackdrop;
	uint8_t legacyRescue;
	uint8_t legacyAllWayShown;
	uint8_t legacyVars[8];
	char iffNames[4][20];
	XwaRegion regions[4];
	XwaGlobalCargo globalCargos[16];
	XwaGlobalUnit globalGroups[32];
	XwaGlobalUnit globalUnits[40];
	uint8_t missionType;
	uint8_t goalsUnimportant;
	uint8_t timeLimitMin;
	uint8_t endMissionWhenComplete;
	uint8_t briefingOfficer;
	uint8_t briefingLogo;
	uint8_t briefingOfficerEntryLine;
	uint8_t secondaryVersion;
	uint8_t winOfficer;
	uint8_t failOfficer;
	uint8_t reserved[58];
} XwaMissionHeaderBody;

typedef struct XWA_MISSION_PACKED_STRUCT XwaMissionHeader {
	uint16_t numFlightGroups;
	uint16_t numMessages;
	XwaMissionHeaderBody body;
} XwaMissionHeader;

typedef struct XWA_MISSION_PACKED_STRUCT XwaTeam {
	char name[16];
	uint8_t gap_10[8];
	TeamAllegianceByte allies[10];
	char endOfMissionMessages[6][64];
	uint8_t eomRawDelay[3];
	uint8_t eomSourceFG[3];
	char voiceIDs[3][20];
	uint8_t pad_1E4;
} XwaTeam;

typedef struct XWA_MISSION_PACKED_STRUCT FrontendMissionTextTail {
	char fgGoalStrings[192][8][3][64];
	char globalGoalStrings[10][28][3][64];
	char orderStrings[192][16][64];
	char missionSuccessfulText[4096];
	char missionFailedText[4096];
	char missionDescriptionText[4096];
} FrontendMissionTextTail;

struct XWA_MISSION_PACKED_STRUCT FrontendMission {
	XwaFlightGroup flightGroups[192];
	XwaMessage messages[64];
	XwaGlobalGoal globalGoals[10][7];
	XwaMissionHeaderBody header;
	XwaTeam teams[10];
	int16_t flightGroupCount;
	uint16_t messageCount;
	uint16_t field_B2DBC;
	uint16_t formatVersion;
	FrontendMissionTextTail textTail;
};

#define XWA_MISSION_STATIC_ASSERT(name, condition) typedef char name[(condition) ? 1 : -1]

XWA_MISSION_STATIC_ASSERT(xwa_trigger_size, sizeof(XwaTrigger) == 0x6);
XWA_MISSION_STATIC_ASSERT(xwa_trigger_pair_size, sizeof(XwaTriggerPair) == 0x10);
XWA_MISSION_STATIC_ASSERT(xwa_waypoint_size, sizeof(XwaWaypoint) == 0x8);
XWA_MISSION_STATIC_ASSERT(xwa_flight_group_goal_payload_size, sizeof(XwaFlightGroupGoalPayload) == 0x10);
XWA_MISSION_STATIC_ASSERT(xwa_goal_fg_size, sizeof(XwaGoalFG) == 0x50);
XWA_MISSION_STATIC_ASSERT(xwa_order_size, sizeof(XwaOrder) == 0x94);
XWA_MISSION_STATIC_ASSERT(xwa_flight_group_size, sizeof(XwaFlightGroup) == 0xE3E);
XWA_MISSION_STATIC_ASSERT(xwa_message_size, sizeof(XwaMessage) == 0xA0);
XWA_MISSION_STATIC_ASSERT(xwa_global_goal_size, sizeof(XwaGlobalGoal) == 0x7A);
XWA_MISSION_STATIC_ASSERT(xwa_region_size, sizeof(XwaRegion) == 0x84);
XWA_MISSION_STATIC_ASSERT(xwa_global_cargo_size, sizeof(XwaGlobalCargo) == 0x8C);
XWA_MISSION_STATIC_ASSERT(xwa_global_unit_size, sizeof(XwaGlobalUnit) == 0x57);
XWA_MISSION_STATIC_ASSERT(xwa_mission_header_body_size, sizeof(XwaMissionHeaderBody) == 0x23EA);
XWA_MISSION_STATIC_ASSERT(xwa_mission_header_size, sizeof(XwaMissionHeader) == 0x23EE);
XWA_MISSION_STATIC_ASSERT(xwa_team_size, sizeof(XwaTeam) == 0x1E5);
XWA_MISSION_STATIC_ASSERT(xwa_frontend_mission_text_tail_size, sizeof(FrontendMissionTextTail) == 0x88200);
XWA_MISSION_STATIC_ASSERT(xwa_frontend_mission_size, sizeof(FrontendMission) == 0x13B0C0);
XWA_MISSION_STATIC_ASSERT(xwa_order_variable1_offset, offsetof(XwaOrder, variable1) == 0x2);
XWA_MISSION_STATIC_ASSERT(xwa_order_variable2_offset, offsetof(XwaOrder, variable2) == 0x3);
XWA_MISSION_STATIC_ASSERT(xwa_order_variable3_offset, offsetof(XwaOrder, variable3) == 0x4);
XWA_MISSION_STATIC_ASSERT(xwa_order_secondary_target_types_offset,
						  offsetof(XwaOrder, secondaryTargetTypes) == 0x6);
XWA_MISSION_STATIC_ASSERT(xwa_order_secondary_targets_offset, offsetof(XwaOrder, secondaryTargets) == 0x8);
XWA_MISSION_STATIC_ASSERT(xwa_fg_goal_parameter_offset,
						  offsetof(XwaFlightGroupGoalPayload, parameter) == 0xE);
XWA_MISSION_STATIC_ASSERT(xwa_message_raw_delay_offset, offsetof(XwaMessage, rawDelay) == 0x8A);
XWA_MISSION_STATIC_ASSERT(xwa_flight_group_craft_role_offset, offsetof(XwaFlightGroup, craftRole) == 0x50);
XWA_MISSION_STATIC_ASSERT(xwa_flight_group_craft_type_offset, offsetof(XwaFlightGroup, craftType) == 0x6B);
XWA_MISSION_STATIC_ASSERT(xwa_flight_group_player_number_offset,
						  offsetof(XwaFlightGroup, playerNumber) == 0x7D);
XWA_MISSION_STATIC_ASSERT(xwa_flight_group_mission_point_regions_offset,
						  offsetof(XwaFlightGroup, missionPointRegions) == 0xDAA);
XWA_MISSION_STATIC_ASSERT(xwa_flight_group_backdrop_offset, offsetof(XwaFlightGroup, backdrop) == 0xE12);
XWA_MISSION_STATIC_ASSERT(xwa_mission_header_mission_type_offset,
						  offsetof(XwaMissionHeaderBody, missionType) == 0x23A6);
XWA_MISSION_STATIC_ASSERT(xwa_frontend_mission_messages_offset,
						  offsetof(FrontendMission, messages) == 0xAAE80);
XWA_MISSION_STATIC_ASSERT(xwa_frontend_mission_global_goals_offset,
						  offsetof(FrontendMission, globalGoals) == 0xAD680);
XWA_MISSION_STATIC_ASSERT(xwa_frontend_mission_header_offset, offsetof(FrontendMission, header) == 0xAF7DC);
XWA_MISSION_STATIC_ASSERT(xwa_frontend_mission_teams_offset, offsetof(FrontendMission, teams) == 0xB1BC6);
XWA_MISSION_STATIC_ASSERT(xwa_frontend_mission_fg_count_offset,
						  offsetof(FrontendMission, flightGroupCount) == 0xB2EB8);
XWA_MISSION_STATIC_ASSERT(xwa_frontend_mission_text_tail_offset,
						  offsetof(FrontendMission, textTail) == 0xB2EC0);

#undef XWA_MISSION_STATIC_ASSERT

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#undef XWA_MISSION_PACKED_STRUCT

int FrontendMission_ConvertLegacyDelayValues(FrontendMission* mission);
int FrontendMission_Localize(FrontendMission* mission, struct FrontendBriefingContent* briefing,
							 int teamIndex, char* missionFileName);
int FrontendMission_LoadForBriefing(void);
void FrontendMission_Reset(void);
int FrontendMission_LoadCurrentMissionData(void);
int FrontendMission_LoadFile(char* fileName);
int FrontendMission_LoadCurrent(void);
int FrontendMission_InitPlayerState(void);

#ifdef __cplusplus
}
#endif

#endif
