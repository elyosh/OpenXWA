#include "xwa_runtime/frontend/frontend_screens.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/film.h"
#include "xwa/frontend/briefing_room.h"
#include "xwa/frontend/combat_sim_menu.h"
#include "xwa/frontend/concourse.h"
#include "xwa/frontend/family_transport_room.h"
#include "xwa/frontend/film_room.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/frontend/frontend_dialog.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/mission_briefing.h"
#include "xwa/frontend/mission_debrief.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/frontend/tech_library.h"
#include "xwa_runtime/frontend/frontend_geometry.h"

#include <string.h>

#define XWA_SCREENS_PATH "frontend/frontend.yaml"

/* Screen and view names, in k_screens order. */
typedef enum XwaScreenId {
	SCREEN_NONE = -1,
	SCREEN_CONCOURSE,
	SCREEN_COMBAT_ROOM,
	SCREEN_COMBAT_ROOM_FAMILY,
	SCREEN_FAMILY_ROOM,
	SCREEN_FAMILY_ROOM_MONITOR,
	SCREEN_FAMILY_ROOM_DETAIL,
	SCREEN_FAMILY_ROOM_MEDAL_CASE,
	SCREEN_FAMILY_ROOM_RANK,
	SCREEN_FILM_ROOM,
	SCREEN_TECH_ROOM,
	SCREEN_BRIEFING_ROOM,
	SCREEN_BRIEFING_ROOM_START,
	SCREEN_MISSION_BRIEFING,
	SCREEN_MISSION_SETUP,
	SCREEN_MULTIPLAYER_SETUP,
	SCREEN_JOIN_GAME,
	SCREEN_HOST_GAME,
	SCREEN_DEBRIEF,
	SCREEN_GET_READY,
	SCREEN_LOADING,
	SCREEN_DATAPAD,
	SCREEN_CREATE_PILOT,
	SCREEN_CONFIRM,
	SCREEN_EDIT_TEXT,
	SCREEN_FILM_NAME,
	SCREEN_DEATH_STAR_OUTCOME,
	SCREEN_JOYSTICK_ACTION,
	SCREEN_COUNT
} XwaScreenId;

typedef struct XwaScreen {
	const char* name;
	FrontendScreenUpdateFn update; /* NULL for a view and for the loading screen */
	XwaScreenId parent;            /* a view's screen */
	int dialog;                    /* pushed over another screen */
} XwaScreen;

static const XwaScreen k_screens[SCREEN_COUNT] = {
	{ "concourse", Concourse_Update, SCREEN_NONE, 0 },
	{ "combat_room", CombatSimMenu_Update, SCREEN_NONE, 0 },
	{ "combat_room.family", NULL, SCREEN_COMBAT_ROOM, 0 },
	{ "family_room", FamilyTransportRoom_Update, SCREEN_NONE, 0 },
	{ "family_room.monitor", NULL, SCREEN_FAMILY_ROOM, 0 },
	{ "family_room.detail", NULL, SCREEN_FAMILY_ROOM, 0 },
	{ "family_room.medal_case", NULL, SCREEN_FAMILY_ROOM, 0 },
	{ "family_room.rank", NULL, SCREEN_FAMILY_ROOM, 0 },
	{ "film_room", FilmRoom_Update, SCREEN_NONE, 0 },
	{ "tech_room", TechLibrary_Update, SCREEN_NONE, 0 },
	{ "briefing_room", BriefingRoom_Update, SCREEN_NONE, 0 },
	{ "briefing_room.start", NULL, SCREEN_BRIEFING_ROOM, 0 },
	{ "mission_briefing", MissionBriefing_Update, SCREEN_NONE, 0 },
	{ "mission_setup", MissionSetup_Update, SCREEN_NONE, 0 },
	{ "multiplayer_setup", MultiplayerSetup_Update, SCREEN_NONE, 0 },
	{ "join_game", FrontendNet_JoinGameScreen, SCREEN_NONE, 0 },
	{ "host_game", FrontendNet_HostGameScreen, SCREEN_NONE, 0 },
	{ "debrief", MissionDebrief_Update, SCREEN_NONE, 0 },
	{ "get_ready", FlightLoading_GetReadyScreen, SCREEN_NONE, 0 },
	{ "loading", NULL, SCREEN_NONE, 0 },
	{ "datapad", Config_OptionsDatapadUpdate, SCREEN_NONE, 1 },
	{ "create_pilot", FrontendDialog_CreatePilotNameCallback, SCREEN_NONE, 1 },
	{ "confirm", FrontendDialog_ConfirmUpdateCallback, SCREEN_NONE, 1 },
	{ "edit_text", FrontendDialog_EditTextCallback, SCREEN_NONE, 1 },
	{ "film_name", FilmNamePrompt_Update, SCREEN_NONE, 1 },
	{ "death_star_outcome", FrontendDialog_DeathStarTourOutcomeCallback, SCREEN_NONE, 1 },
	{ "joystick_action", Config_JoystickActionPickerUpdate, SCREEN_NONE, 1 },
};

/* -1 where the table doesn't list a name. */
static int g_screenLayouts[SCREEN_COUNT];
static int g_screenDefault = -1;
static int g_screensLoaded;

static int g_screenRunIndex = -1; /* the stack index that ran last */
static XwaScreenId g_screenLogged = SCREEN_NONE;
static int g_screenLoggedLayout = -1;

static const char* const k_layoutNames[] = { "classic", "wide", "fill" };

static int screens_parse_layout(const char* text) {
	for (int i = 0; i < 3; i++) {
		if (strcmp(text, k_layoutNames[i]) == 0) {
			return i;
		}
	}
	return -1;
}

/* Reads `layouts` once. A wrong name or value makes the whole table be
 * ignored, so that a typo can't leave a mod half wide. */
static void screens_load(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* table;
	int layouts[SCREEN_COUNT];
	int defaultLayout = -1;
	int ok = 1;

	if (g_screensLoaded) {
		return;
	}
	g_screensLoaded = 1;
	for (int i = 0; i < SCREEN_COUNT; i++) {
		g_screenLayouts[i] = -1;
		layouts[i] = -1;
	}
	memset(&error, 0, sizeof(error));
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, XWA_SCREENS_PATH, &file, &error)) {
		return; /* frontend_geometry.c reports errors in this file */
	}
	table = AeronConfigNode_MapGet(AeronConfigFile_Root(file), "layouts");
	if (table != NULL && AeronConfigNode_Type(table) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.frontend", "%s:%d: layouts must be a map of screens to layouts", XWA_SCREENS_PATH,
					   AeronConfigNode_Line(table));
		ok = 0;
	}
	for (size_t i = 0; ok && i < AeronConfigNode_MapCount(table); i++) {
		const char* key = AeronConfigNode_MapKeyAt(table, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(table, i);
		const int layout = screens_parse_layout(AeronConfigNode_String(value, ""));
		int screen = 0;

		if (layout < 0) {
			Aeron_LogError("xwa.frontend", "%s:%d: '%s' must be wide, fill or classic", XWA_SCREENS_PATH,
						   AeronConfigNode_Line(value), key);
			ok = 0;
			break;
		}
		if (strcmp(key, "default") == 0) {
			defaultLayout = layout;
			continue;
		}
		while (screen < SCREEN_COUNT && strcmp(key, k_screens[screen].name) != 0) {
			screen++;
		}
		if (screen == SCREEN_COUNT) {
			Aeron_LogError("xwa.frontend", "%s:%d: unknown screen '%s'", XWA_SCREENS_PATH,
						   AeronConfigNode_Line(value), key);
			ok = 0;
			break;
		}
		layouts[screen] = layout;
	}
	AeronConfigFile_Destroy(file);
	if (!ok) {
		Aeron_LogError("xwa.frontend", "%s: layouts is ignored; every screen is classic", XWA_SCREENS_PATH);
		return;
	}
	memcpy(g_screenLayouts, layouts, sizeof(layouts));
	g_screenDefault = defaultLayout;
}

static XwaScreenId screens_find(FrontendScreenUpdateFn update) {
	for (int i = 0; update != NULL && i < SCREEN_COUNT; i++) {
		if (k_screens[i].update == update) {
			return (XwaScreenId)i;
		}
	}
	return SCREEN_NONE;
}

/* The view a screen shows now, from the state the game keeps for it. */
static XwaScreenId screens_view(XwaScreenId screen) {
	switch (screen) {
		case SCREEN_FAMILY_ROOM:
			switch (g_frontendFamilyDetailMode) {
				case 1:
					return SCREEN_FAMILY_ROOM_MONITOR;
				case 2:
					return SCREEN_FAMILY_ROOM_DETAIL;
				case 3:
					return SCREEN_FAMILY_ROOM_MEDAL_CASE;
				case 4:
					return SCREEN_FAMILY_ROOM_RANK;
				default:
					return screen;
			}
		case SCREEN_COMBAT_ROOM:
			/* The test CombatSimMenu_Update makes to show podopen. */
			return g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u
					   ? SCREEN_COMBAT_ROOM_FAMILY
					   : screen;
		case SCREEN_BRIEFING_ROOM:
			/* briefstart shows until the briefing text starts. */
			switch (g_briefingRoomState) {
				case BRIEFING_ROOM_START_BRIEFING_REVEAL:
				case BRIEFING_ROOM_REVEAL_BRIEFING_TEXT:
				case BRIEFING_ROOM_CLOSE_REVEAL_MASK:
				case BRIEFING_ROOM_ENTER_MISSION_BRIEFING:
				case BRIEFING_ROOM_EXIT_TO_CONCOURSE:
					return screen;
				default:
					return SCREEN_BRIEFING_ROOM_START;
			}
		default:
			return screen;
	}
}

/* A view's layout, else its screen's, else `default`; -1 when none. */
static int screens_listed_layout(XwaScreenId screen) {
	if (g_screenLayouts[screen] >= 0) {
		return g_screenLayouts[screen];
	}
	if (k_screens[screen].parent != SCREEN_NONE && g_screenLayouts[k_screens[screen].parent] >= 0) {
		return g_screenLayouts[k_screens[screen].parent];
	}
	return -1;
}

/* The layout of the screen at stack index `index`, and its name. Dialogs
 * that the table doesn't list take the layout of the screen below them. */
static int screens_stack_layout(int index, XwaScreenId* shown) {
	for (; index >= 0; index--) {
		const XwaScreenId screen = screens_find(g_screenStates[index].updateFn);
		int layout;

		if (screen == SCREEN_NONE) {
			continue; /* steps between screens, such as the bootstrap */
		}
		*shown = screens_view(screen);
		layout = screens_listed_layout(*shown);
		if (layout >= 0 || !k_screens[screen].dialog) {
			return layout >= 0 ? layout : g_screenDefault;
		}
	}
	return g_screenDefault;
}

void XwaFrontendScreens_NoteRun(void) { g_screenRunIndex = g_screenStackTop; }

XwaFrontendLayout XwaFrontendScreens_Layout(void) {
	XwaScreenId shown = SCREEN_NONE;
	int layout;

	screens_load();
	if (XwaSnapshot_SceneKind() == XWA_SCENE_LOADING) {
		shown = SCREEN_LOADING;
		layout = screens_listed_layout(SCREEN_LOADING);
		if (layout < 0) {
			layout = g_screenDefault;
		}
	} else {
		layout = screens_stack_layout(g_screenRunIndex < 0 ? g_screenStackTop : g_screenRunIndex, &shown);
	}
	if (layout < 0) {
		layout = XWA_FRONTEND_LAYOUT_CLASSIC;
	}
	if (shown != g_screenLogged || layout != g_screenLoggedLayout) {
		g_screenLogged = shown;
		g_screenLoggedLayout = layout;
		Aeron_LogInfo("xwa.frontend", "screen %s: %s",
					  shown != SCREEN_NONE ? k_screens[shown].name : "(none)", k_layoutNames[layout]);
	}
	return (XwaFrontendLayout)layout;
}

float XwaFrontendScreens_TextWidthScale(void) {
	float width;
	float height;

	if (XwaFrontendScreens_Layout() != XWA_FRONTEND_LAYOUT_WIDE) {
		return 1.0f;
	}
	/* A unit is width/640 across and height/480 down, and text keeps its
	   shape at height/480. */
	XwaFrontendGeometry_DesignSize(&width, &height);
	return 640.0f * height / (480.0f * width);
}

int XwaFrontendScreens_TextScale256(void) {
	return (int)(XwaFrontendScreens_TextWidthScale() * 256.0f + 0.5f);
}

void XwaFrontendScreens_AdvanceText(int* x, int* x_frac, int advance, int scale256) {
	const int total = *x_frac + advance * scale256;
	const int whole = total >= 0 ? total / 256 : -((255 - total) / 256);

	*x += whole;
	*x_frac = total - whole * 256;
}

void XwaFrontendScreens_Shutdown(void) {
	g_screensLoaded = 0;
	g_screenDefault = -1;
	g_screenRunIndex = -1;
	g_screenLogged = SCREEN_NONE;
	g_screenLoggedLayout = -1;
}
