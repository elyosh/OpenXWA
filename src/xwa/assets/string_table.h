#ifndef XWA_ASSETS_STRING_TABLE_H
#define XWA_ASSETS_STRING_TABLE_H

#include "xwa/assets/object_type.h"
#include "xwa/assets/ui_string.h"
#include "xwa/util/memory.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	XWA_GOAL_CONDITION_COUNT = 188,
	XWA_GOAL_CONDITION_TEXT_SLOTS = 15,
	XWA_GOAL_CONDITION_TEXT_COUNT = XWA_GOAL_CONDITION_COUNT * XWA_GOAL_CONDITION_TEXT_SLOTS,
	XWA_CRAFT_GENDER_COUNT = 222,
};

typedef enum PanelStringId {
	PANEL_STRING_DIST = 0,
	PANEL_STRING_SHD = 1,
	PANEL_STRING_HULL = 2,
	PANEL_STRING_HUD_S = 3,
	PANEL_STRING_HUD_L = 4,
	PANEL_STRING_HUD_E = 5,
	PANEL_STRING_HUD_B = 6,
	PANEL_STRING_HUD_H = 7,
	PANEL_STRING_HUD_FILM_REC = 8,
	PANEL_STRING_HUD_FILM_FF = 9,
	PANEL_STRING_HUD_FILM_MISSION_TIME = 10,
	PANEL_STRING_HUD_FILM_FOLLOWING = 11,
	PANEL_STRING_HUD_FILM_TRACKING = 12,
	PANEL_STRING_SYS = 13,
	PANEL_STRING_TRG = 14,
	PANEL_STRING_NO_CARGO = 15,
	PANEL_STRING_THIS_CRAFT = 16,
	PANEL_STRING_CURRENT_ORDERS = 17,
	PANEL_STRING_NONE = 18,
	PANEL_STRING_CURRENT_TARGET = 19,
	PANEL_STRING_CURRENT_DEST = 20,
	PANEL_STRING_DIST_FROM_TARG = 21,
	PANEL_STRING_DIST_FROM_DEST = 22,
	PANEL_STRING_TIME_REMAIN = 23,
	PANEL_STRING_TIME_2_TARG = 24,
	PANEL_STRING_TIME_2_DEST = 25,
	PANEL_STRING_WARP_STRING = 26,
	PANEL_STRING_LATENCY_STRING = 27,
	PANEL_STRING_NAME = 28,
	PANEL_STRING_NAME_G = 29,
	PANEL_STRING_CARRIED_BY = 30,
	PANEL_STRING_LAST_STRING = 31,
	PANEL_STRING_COUNT = 32,
} PanelStringId;

typedef enum MfdStringId {
	MFD_STRING_SCORE = 0,
	MFD_STRING_GOALS = 1,
	MFD_STRING_RADIO = 2,
	MFD_STRING_DAMAGE = 3,
	MFD_STRING_ENEMY = 4,
	MFD_STRING_FRIENDLY = 5,
	MFD_STRING_FLIGHT_COMMANDS = 6,
	MFD_STRING_CONSOLE = 7,
	MFD_STRING_MAP_HELP = 8,
	MFD_STRING_FILM_COMMANDS = 9,
	MFD_STRING_FILM_OPTIONS = 10,
	MFD_STRING_PLAYER_LABEL = 11,
	MFD_STRING_NO_RADIO_MSG = 12,
	MFD_STRING_TARGETING_DAMAGED = 13,
	MFD_STRING_NO_FRIENDLY_FGS = 14,
	MFD_STRING_NO_ENEMY_FGS = 15,
	MFD_STRING_NOT_AVAILABLE = 16,
	MFD_STRING_ALL_SYSTEMS_GO = 17,
	MFD_STRING_CRAFT_TYPE = 18,
	MFD_STRING_SIDE = 19,
	MFD_STRING_PLAYER = 20,
	MFD_STRING_BACK_TO_MAIN = 21,
	MFD_STRING_LAST_STRING = 22,
	MFD_STRING_COUNT = 23,
} MfdStringId;

enum {
	HANGAR_MENU_TITLE_INSTRUCTIONS = 9,
	HANGAR_MENU_TITLE_COUNT = 12,
};

typedef enum HangarMiscStringId {
	HANGAR_MISC_BEAM_SYSTEM = 0,
	HANGAR_MISC_DEFENSE_SYSTEM = 1,
	HANGAR_MISC_ENTERING_HYPERSPACE = 2,
	HANGAR_MISC_IN_HYPERSPACE = 3,
	HANGAR_MISC_LEAVING_HYPERSPACE = 4,
	HANGAR_MISC_MOVING_TO_JUMPPOINT = 5,
	HANGAR_MISC_HYPERSPACE_IN_SECONDS1 = 6,
	HANGAR_MISC_SECONDS = 7,
	HANGAR_MISC_EVACUATE = 8,
	HANGAR_MISC_WARNING = 9,
	HANGAR_MISC_HAS_HULL_DAMAGE = 10,
	HANGAR_MISC_BEAM_NONE = 11,
	HANGAR_MISC_BEAM_TRACTOR = 12,
	HANGAR_MISC_BEAM_JAMMING = 13,
	HANGAR_MISC_BEAM_DECOY = 14,
	HANGAR_MISC_BEAM_ENERGY = 15,
	HANGAR_MISC_WARHEAD_NONE = 16,
	HANGAR_MISC_FIRST_PLACE = 17,
	HANGAR_MISC_SECOND_PLACE = 18,
	HANGAR_MISC_THIRD_PLACE = 19,
	HANGAR_MISC_FOURTH_PLACE = 20,
	HANGAR_MISC_FIFTH_PLACE = 21,
	HANGAR_MISC_YOU_PLACED = 22,
	HANGAR_MISC_ALMOST_TENTH_PLACE = 23,
	HANGAR_MISC_AT_LEAST_FINISHED = 24,
	HANGAR_MISC_NOT_BAD = 25,
	HANGAR_MISC_GLAD_TO_SEE_COURAGE = 26,
	HANGAR_MISC_YOU_MUST_PUSH_YOURSELF = 27,
	HANGAR_MISC_BEFORE_GOING_TO_GROUND = 28,
	HANGAR_MISC_NOT_OFFICIAL_DESC = 29,
	HANGAR_MISC_HALL_OF_FAME_DESC = 30,
	HANGAR_MISC_READ_INSTRUCTIONS = 31,
	HANGAR_MISC_PAY_CLOSE_ATTENTION = 32,
	HANGAR_MISC_YARD_HUD_RINGS = 33,
	HANGAR_MISC_YARD_HUD_LAPS = 34,
	HANGAR_MISC_YARD_HUD_PENALTY = 35,
	HANGAR_MISC_YARD_HUD_GOT_R2 = 36,
	HANGAR_MISC_YOUR_CRAFT = 37,
	HANGAR_MISC_ARROWS_TO_MOVE = 38,
	HANGAR_MISC_NUMBER_KEYS_FOR_CAMERA = 39,
	HANGAR_MISC_ZERO_FOR_COCKPIT = 40,
	HANGAR_MISC_SPACE_TO_LAUNCH = 41,
	HANGAR_MISC_NEW_CRAFT_IS_READY = 42,
	HANGAR_MISC_RINGS_AND_LAPS_LEFT = 43,
	HANGAR_MISC_COMPLETED = 44,
	HANGAR_MISC_LAST_STRING = 45,
	HANGAR_MISC_COUNT = 46,
} HangarMiscStringId;

extern MemoryHandle g_stringDataHandle;
extern int g_gameStringCount;
extern int g_stringDataBufferSize;
extern int g_stringDataBufferUsed;
extern int* g_uiStringOffsets;
extern char* g_uiStringData;
extern int g_uiStringCount;
extern int g_uiStringCapacity;

extern char* g_strDamageSystemNames[12];
extern char* g_strFileErrorMessages[5];
extern char* g_strDiskIoMessages[43];
extern char* g_strPressSpaceBar;
extern char* g_strGoalCondMasculine[XWA_GOAL_CONDITION_TEXT_COUNT];
extern char* g_strGoalCondFeminine[XWA_GOAL_CONDITION_TEXT_COUNT];
extern char* g_strGoalCondNeutered[XWA_GOAL_CONDITION_TEXT_COUNT];
extern const unsigned char g_goalConditionTextVariantCount[47];
extern char* g_strPercentages[15];
extern char* g_strOperators[3];
extern char* g_strGoalTitles[10];
extern char* g_strConjunctions[9];
extern char* g_strSides[4];
extern char* g_strShipFamily[8];
extern char* g_strShipGenus[17];
extern char* g_strMapStrings[17];
extern char* g_strInFlightMessages[538];
extern char* g_strPanelStrings[PANEL_STRING_COUNT];
extern char* g_strFilmCommands[11];
extern char* g_strFilmOptions[15];
extern char* g_strWaypointStrings[15];
extern char* g_strComponentStrings[34];
extern char* g_strOverlayStrings[42];
extern char* g_strThreatStrings[5];
extern char* g_strStatusStrings[10];
extern char* g_strWarheadNames[16];
extern char* g_strWarheadUnknown;
extern char* g_strWarheadSurvivors;
extern char* g_strBuoyNames[14];
extern char* g_strSpeciesNamesLong[222];
extern char* g_strSpeciesNamesPlural[225];
extern char* g_strDefaultModelName;
extern char* g_strSpeciesNames[222];
extern char* g_strWingmanCommands[11];
extern char* g_strFlightCmdMainMenu[10];
extern char* g_strFlightCmdSubMenu[5];
extern char* g_strFlightCmdMenuItems[7];
extern char* g_strFlightCmdSubMenuItems[27];
extern char* g_strMfdStrings[MFD_STRING_COUNT];
extern char* g_strConsoleStrings[2];
extern char* g_strProvingGroundDescs[64];
extern char* g_strHangarMenuTitles[HANGAR_MENU_TITLE_COUNT];
extern char* g_strHangarMenuItems[50];
extern char* g_strHangarMiscStrings[HANGAR_MISC_COUNT];
extern char* g_strWarheadNamesPlural[16];
extern char* g_strYardStrings[19];
extern char* g_strDiStrings[12];
extern unsigned char g_craftGender[XWA_CRAFT_GENDER_COUNT];

int StringTable_LoadGameStrings(void);
int FrontendString_LoadTable(char* fileName);
void FrontendString_UnloadTable(void);
const char* FrontendString_Get(UIString index);

#ifdef __cplusplus
}
#endif

#endif
