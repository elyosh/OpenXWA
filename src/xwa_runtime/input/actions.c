#include "xwa_runtime/input/actions.h"

#include "xwa/flight/flight.h"

#include <string.h>

typedef struct XwaActionDefinition {
	const char *name, *label;
	XwaInputActionCategory category;
	uint16_t key;
	XwaInputActionKind kind;
} XwaActionDefinition;

static const XwaActionDefinition g_actions[XWA_INPUT_ACTION_COUNT] = {
	{ "none", "None", XWA_INPUT_ACTION_CATEGORY_SYSTEM, KEY_NONE, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "fire_weapon", "Fire Weapon", XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_ALT_2,
	  XWA_INPUT_ACTION_KIND_HELD },
	{ "target_roll_modifier", "Target / Roll Modifier", XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_ALT_3,
	  XWA_INPUT_ACTION_KIND_HELD },
	{ "cycle_weapon", "Cycle Weapon", XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_W,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "cycle_firing_mode", "Cycle Firing Mode / Turret Auto-Fire", XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_X,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "laser_convergence_or_zoom_map_target", "Cycle Laser Convergence / Zoom Map to Target",
	  XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_Z, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_beam", "Toggle Beam Weapon", XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_B,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "fire_countermeasure_or_center_map_target", "Fire Countermeasure / Center Map on Target",
	  XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_C, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_auto_gunner", "Toggle Automatic Gunner", XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_F,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "cycle_beam_recharge_rate", "Cycle Beam Recharge Rate", XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_F8,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "cycle_laser_recharge_rate", "Cycle Laser Recharge Rate", XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_F9,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "cycle_shield_recharge_rate", "Cycle Shield Recharge Rate", XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_F10,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "cycle_shield_mode", "Cycle Shield Setting", XWA_INPUT_ACTION_CATEGORY_WEAPONS, KEY_S,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "xfer_shields_to_lasers", "Transfer Shield Energy to Lasers", XWA_INPUT_ACTION_CATEGORY_WEAPONS,
	  KEY_SEMICOLON, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "xfer_lasers_to_shields", "Transfer Laser Energy to Shields", XWA_INPUT_ACTION_CATEGORY_WEAPONS,
	  KEY_APOSTROPHE, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "xfer_all_lasers_to_shields", "Transfer All Laser Energy to Shields", XWA_INPUT_ACTION_CATEGORY_WEAPONS,
	  KEY_QUOTES, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_in_sights", "Target Craft in Sights", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_ALT_1,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_next", "Next Target", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_T,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_prev", "Previous Target", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_Y,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_nearest_fighter", "Nearest Enemy Fighter", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_R,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_attacker_of_target", "Attacker of Target", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_A,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_my_attackers", "Next Craft Attacking Me", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_E,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_newest", "Newest Craft", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_U,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_objective", "Nearest Objective Craft", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_O,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_nearest_player", "Nearest Enemy Player", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_P,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_next_player_craft", "Next Player Craft", XWA_INPUT_ACTION_CATEGORY_TARGETING,
	  KEY_NEXT_PLAYER_CRAFT, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_incoming_warhead", "Nearest Incoming Warhead", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_I,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_next_buoy", "Next Navigation Buoy", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_N,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_next_friendly", "Next Friendly Craft", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_F1,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_prev_friendly", "Previous Friendly Craft", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_SHIFT_F1,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_next_neutral", "Next Neutral Craft", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_F2,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_prev_neutral", "Previous Neutral Craft", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_SHIFT_F2,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_next_enemy", "Next Enemy Craft", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_F3,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_prev_enemy", "Previous Enemy Craft", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_F4,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_component_next", "Next Target Component", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_COMMA,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_component_prev", "Previous Target Component", XWA_INPUT_ACTION_CATEGORY_TARGETING,
	  KEY_LESS_THAN, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_clear", "Clear Target", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_ALT_C,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_preset_recall_1", "Recall Target Preset 1", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_F5,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_preset_recall_2", "Recall Target Preset 2", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_F6,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_preset_recall_3", "Recall Target Preset 3", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_F7,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_preset_store_1", "Store Target Preset 1", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_SHIFT_F5,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_preset_store_2", "Store Target Preset 2", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_SHIFT_F6,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_preset_store_3", "Store Target Preset 3", XWA_INPUT_ACTION_CATEGORY_TARGETING, KEY_SHIFT_F7,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "roll_left", "Roll Left", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_NONE, XWA_INPUT_ACTION_KIND_ROLL },
	{ "roll_right", "Roll Right", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_NONE, XWA_INPUT_ACTION_KIND_ROLL },
	{ "throttle_up", "Increase Throttle", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_EQUAL,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "throttle_down", "Decrease Throttle", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_MINUS,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "throttle_down_or_track_map_target", "Decrease Throttle / Track Map Target",
	  XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_PAD_MINUS, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "throttle_zero", "Zero Throttle", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_BACKSLASH,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "throttle_one_third", "One-Third Throttle", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_LEFT_BRACKET,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "throttle_two_thirds", "Two-Thirds Throttle", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_RIGHT_BRACKET,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "throttle_full", "Full Throttle", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_BACKSPACE,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "match_speed_or_type_message", "Match Target Speed / Type Message", XWA_INPUT_ACTION_CATEGORY_FLIGHT,
	  KEY_ENTER, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "power_preset_recall_1", "Use Power Preset 1", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_F11,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "power_preset_recall_2", "Use Power Preset 2", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_F12,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "power_preset_store_1", "Save Power Preset 1", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_SHIFT_F11,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "power_preset_store_2", "Save Power Preset 2", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_SHIFT_F12,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_s_foils", "Toggle S-Foils", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_V,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_landing_gear", "Toggle Landing Gear", XWA_INPUT_ACTION_CATEGORY_FLIGHT, KEY_CTRL_L,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "look_up", "Look Up", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_PAD_8, XWA_INPUT_ACTION_KIND_LOOK },
	{ "look_down", "Look Down", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_PAD_2, XWA_INPUT_ACTION_KIND_LOOK },
	{ "look_left", "Look Left", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_PAD_4, XWA_INPUT_ACTION_KIND_LOOK },
	{ "look_right", "Look Right", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_PAD_6, XWA_INPUT_ACTION_KIND_LOOK },
	{ "look_center", "Center View", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_PAD_5,
	  XWA_INPUT_ACTION_KIND_LOOK_CENTER },
	{ "toggle_cockpit", "Toggle Cockpit", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_PERIOD,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_padlock", "Toggle Padlock View", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_L,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_mouse_look", "Toggle Cockpit Mouse Look", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_SCROLL_LOCK,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "view_toggle_high_angle", "Toggle High-Angle View", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_PAD_0,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_external_camera", "Toggle External Camera", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_SLASH,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_camera_control_or_map_view", "Toggle Free Camera / Step Map View",
	  XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_PAD_STAR, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "flyby_camera", "Flyby Camera", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_ALT_J,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "missile_camera", "Missile Camera", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_ALT_N,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "target_camera", "Target Camera", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_ALT_U,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "switch_seat", "Switch Pilot / Turret Seat", XWA_INPUT_ACTION_CATEGORY_VIEW, KEY_G,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "mfd_prev_page", "Previous MFD Page", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_LEFT,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "mfd_next_page", "Next MFD Page", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_RIGHT,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "mfd_up", "MFD Up", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_UP, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "mfd_down", "MFD Down", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_DOWN, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "mfd_select", "Select MFD Item", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_PAD_ENTER,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_left_mfd", "Toggle Left MFD", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_DELETE,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_right_mfd", "Toggle Right MFD", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_PAGEDOWN,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_left_indicator", "Toggle Left Sensor / Shield Indicator", XWA_INPUT_ACTION_CATEGORY_HUD,
	  KEY_INSERT, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_right_indicator", "Toggle Right Sensor / Beam Indicator", XWA_INPUT_ACTION_CATEGORY_HUD,
	  KEY_PAGEUP, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_center_indicators", "Toggle Center Indicators", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_HOME,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_cmd", "Toggle Combat Multiview Display", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_END,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_hud", "Toggle HUD", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_HUD_TOGGLE,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_mfd_overlay", "Toggle MFD Overlay", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_MFD_OVERLAY,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "map_mode", "Toggle Map", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_M, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "toggle_system_messages", "Toggle System Messages", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_ALT_S,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "console_page", "Toggle Console Page", XWA_INPUT_ACTION_CATEGORY_HUD, KEY_CONSOLE_TOGGLE,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "wingman_command_page", "Toggle Wingman Command Page", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS,
	  KEY_TAB, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "command_option_1", "Command Option 1", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_1,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "command_option_2", "Command Option 2", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_2,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "command_option_3", "Command Option 3", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_3,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "command_option_4", "Command Option 4", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_4,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "command_option_5", "Command Option 5", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_5,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "command_option_6", "Command Option 6", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_6,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "command_option_7", "Command Option 7", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_7,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "command_option_9", "Command Option 9", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_9,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "command_menu_back", "Command Menu Back", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_0,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "comm_attack_target", "Wingmen Attack My Target", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_A,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "comm_resupply", "Call Resupply Ship", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_B,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "comm_cover_me", "Wingmen Cover Me", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_C,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "dock_with_target", "Dock With Target", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_D,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "comm_evade", "Order Target to Evade", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_E,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "comm_go", "Order Waiting Craft to Go", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_G,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "comm_go_home", "Order Target Home", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_H,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "comm_ignore_target", "Wingmen Ignore Target", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_I,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "pick_up_target", "Pick Up Target", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_P,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "release_cargo", "Release Carried Object", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_R,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "comm_reinforcements", "Call Reinforcements", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_S,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "comm_report_in", "Wingmen Report In", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_U,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "comm_wait", "Order Target to Wait", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_W,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "taunt_1", "Send Taunt 1", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_1,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "taunt_2", "Send Taunt 2", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_2,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "taunt_3", "Send Taunt 3", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_3,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "taunt_4", "Send Taunt 4", XWA_INPUT_ACTION_CATEGORY_COMMUNICATIONS, KEY_SHIFT_4,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "confirm_or_switch_map_view", "Confirm Order / Switch Map View", XWA_INPUT_ACTION_CATEGORY_SYSTEM,
	  KEY_SPACE, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "options_menu", "Open Options", XWA_INPUT_ACTION_CATEGORY_SYSTEM, KEY_ESCAPE,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "pause", "Pause", XWA_INPUT_ACTION_CATEGORY_SYSTEM, KEY_ALT_P, XWA_INPUT_ACTION_KIND_PAUSE },
	{ "end_mission", "End Mission", XWA_INPUT_ACTION_CATEGORY_SYSTEM, KEY_Q, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "abort_mission", "Abort / Disconnect", XWA_INPUT_ACTION_CATEGORY_SYSTEM, KEY_ALT_Q,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "eject", "Eject", XWA_INPUT_ACTION_CATEGORY_SYSTEM, KEY_ALT_E, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "jump_craft", "Jump to Another Craft", XWA_INPUT_ACTION_CATEGORY_SYSTEM, KEY_J,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "return_to_hangar_or_map_legend", "Return to Hangar / Map Legend", XWA_INPUT_ACTION_CATEGORY_SYSTEM,
	  KEY_H, XWA_INPUT_ACTION_KIND_COMMAND },
	{ "film_recording", "Start / Stop Film Recording", XWA_INPUT_ACTION_CATEGORY_SYSTEM, KEY_SHIFT_F,
	  XWA_INPUT_ACTION_KIND_COMMAND },
	{ "cycle_force_feedback_strength", "Cycle Rumble Strength", XWA_INPUT_ACTION_CATEGORY_SYSTEM, KEY_ALT_F,
	  XWA_INPUT_ACTION_KIND_COMMAND },
};

XwaInputAction XwaInputActions_FromName(const char* name) {
	if (name)
		for (int i = 0; i < XWA_INPUT_ACTION_COUNT; ++i)
			if (!strcmp(name, g_actions[i].name))
				return (XwaInputAction)i;
	return XWA_INPUT_ACTION_NONE;
}

const char* XwaInputActions_ToName(XwaInputAction action) {
	return (unsigned)action < XWA_INPUT_ACTION_COUNT ? g_actions[action].name : "none";
}

const char* XwaInputActions_DisplayName(XwaInputAction action) {
	return (unsigned)action < XWA_INPUT_ACTION_COUNT ? g_actions[action].label : "None";
}

XwaInputActionCategory XwaInputActions_Category(XwaInputAction action) {
	return (unsigned)action < XWA_INPUT_ACTION_COUNT ? g_actions[action].category
													 : XWA_INPUT_ACTION_CATEGORY_SYSTEM;
}

const char* XwaInputActions_CategoryName(XwaInputActionCategory category) {
	static const char* names[] = {
		"Weapons", "Targeting", "Flight", "View", "HUD", "System", "Communications"
	};
	return (unsigned)category < XWA_INPUT_ACTION_CATEGORY_COUNT ? names[category] : "";
}

uint16_t XwaInputActions_Key(XwaInputAction action) {
	return (unsigned)action < XWA_INPUT_ACTION_COUNT ? g_actions[action].key : 0;
}

XwaInputActionKind XwaInputActions_Kind(XwaInputAction action) {
	return (unsigned)action < XWA_INPUT_ACTION_COUNT ? g_actions[action].kind : XWA_INPUT_ACTION_KIND_COMMAND;
}

int XwaInputActions_Look(XwaInputAction action) {
	switch (action) {
		case XWA_INPUT_ACTION_LOOK_UP:
			return XWA_INPUT_LOOK_UP;
		case XWA_INPUT_ACTION_LOOK_DOWN:
			return XWA_INPUT_LOOK_DOWN;
		case XWA_INPUT_ACTION_LOOK_LEFT:
			return XWA_INPUT_LOOK_LEFT;
		case XWA_INPUT_ACTION_LOOK_RIGHT:
			return XWA_INPUT_LOOK_RIGHT;
		case XWA_INPUT_ACTION_LOOK_CENTER:
			return XWA_INPUT_LOOK_CENTER;
		default:
			return -1;
	}
}

int XwaInputActions_Roll(XwaInputAction action) {
	return action == XWA_INPUT_ACTION_ROLL_LEFT    ? XWA_INPUT_ROLL_LEFT
		   : action == XWA_INPUT_ACTION_ROLL_RIGHT ? XWA_INPUT_ROLL_RIGHT
												   : -1;
}

/* Every action can be bound on the keyboard. OpenXvT excludes its two
 * controller-only chat actions; XWA has none. */
bool XwaInputActions_KeyboardBindable(XwaInputAction action) {
	return action > XWA_INPUT_ACTION_NONE && action < XWA_INPUT_ACTION_COUNT;
}
