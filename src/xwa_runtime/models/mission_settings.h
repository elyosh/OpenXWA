#ifndef XWA_RUNTIME_MODELS_MISSION_SETTINGS_H
#define XWA_RUNTIME_MODELS_MISSION_SETTINGS_H

/* A mod's mission settings: rules that the stock game ties to some missions,
 * made data, and a few more. They come from the `settings` section of a
 * mission's file (mission_file.h), over the `mission_settings` section of
 * flight/flight.yaml, which gives defaults for every mission. Both sections
 * take the same keys, and every key is optional:
 *
 *   settings:
 *     red_alert: true                    # the hangar's red alert
 *     skip_hyperspaced_messages: true    # message 143 and "entering area"
 *     skip_arrival_messages_iff: 1       # an IFF, all or none
 *     force_player_in_turret: "0:00:08"  # the mission time, or false
 *     disable_player_lasers: true
 *     disable_player_warheads: true
 *     warhead_collision_damage: false
 *     damage_through_shields_on_hard: true
 *     target_key: { flight_groups: [5, 6], method: cycle, only_not_inspected: true }
 *     skip_projectiles_proximity_check: true
 *     order_of_battle:
 *       flight_groups: [3, 4]
 *       names: { 3: "Home One" }
 *       lines: ["Fleet Assignment: Admiral Thrawn", "", "Command Ship: Chimaera"]
 *       top: 140
 *     interdiction_regions: [2]
 *
 * Without a value, the stock rule applies: the red alert of mission 20,
 * message 143 skipped in mission 49, the arrival messages of IFF 1 skipped
 * in missions 49 to 52, and the player put in the turret at 0:00:08 in
 * mission 1. Without the files, the game is stock.
 *
 * flight/flight.yaml can also change two rules for every mission:
 *
 *   rules:
 *     reset_combat_music_at_mission_load: true
 *     hangar_red_alert_any_mothership: true
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the rules and the defaults of flight/flight.yaml. Called once at
   startup. */
void XwaMissionSettings_Init(void);

/* Reads the settings of mission `missionPath` (for example
   "missions\1b1m5fg.tie"), replacing the previous mission's. Mission_Init
   calls it at each mission load, and the frontend when it loads a mission
   for its briefing. */
void XwaMissionSettings_LoadMission(const char* missionPath);

/* The rules of flight/flight.yaml. */
int XwaMissionSettings_ResetCombatMusicAtMissionLoad(void);
int XwaMissionSettings_HangarRedAlertAnyMothership(void);

/* 1 or 0 when the mission sets the value, -1 for the stock rule. */
int XwaMissionSettings_RedAlert(void);
int XwaMissionSettings_SkipHyperspacedMessages(void);
/* Whether the arrival messages of a flight group of `iff` are skipped. */
int XwaMissionSettings_SkipArrivalMessages(int iff);
/* Whether the player is put in the turret at this mission time. */
int XwaMissionSettings_ForcePlayerInTurretAt(int hours, int minutes, int seconds);
int XwaMissionSettings_ForcePlayerInTurret(void);

int XwaMissionSettings_PlayerLasersDisabled(void);
int XwaMissionSettings_PlayerWarheadsDisabled(void);
int XwaMissionSettings_WarheadCollisionDamage(void);
int XwaMissionSettings_DamageThroughShieldsOnHard(void);
int XwaMissionSettings_SkipProjectilesProximityCheck(void);
/* Whether a hyperspace jump to `region` plays the abort sound. */
int XwaMissionSettings_InterdictedRegion(int region);

/* The O key, outside the proving grounds: when the mission lists flight
   groups for it, targets a craft of them for player `playerIdx` and returns
   1. Returns 0 to keep the stock key. */
int XwaMissionSettings_TargetKey(int playerIdx);

/* The order of battle. ..._Listed is -1 without a list (the stock rule),
   else whether flight group `flightGroupIdx` is listed; ..._Name is its
   name, or NULL. ..._LineCount is the number of text lines that replace
   the order of battle, and ..._LinesTop their top, or -1. */
int XwaMissionSettings_OrderOfBattleListed(int flightGroupIdx);
const char* XwaMissionSettings_OrderOfBattleName(int flightGroupIdx);
int XwaMissionSettings_OrderOfBattleLineCount(void);
const char* XwaMissionSettings_OrderOfBattleLine(int index);
int XwaMissionSettings_OrderOfBattleLinesTop(void);

#ifdef __cplusplus
}
#endif

#endif
