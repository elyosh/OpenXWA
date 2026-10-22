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
 * flight/flight.yaml can also change rules for every mission:
 *
 *   rules:
 *     reset_combat_music_at_mission_load: true
 *     hangar_red_alert_any_mothership: true
 *     classic_hide_destroyed_engine_glows: true
 *     weapon_iff_swap: false
 *     ai_attack_uses_max_speed_cache: true
 *     fire_needs_decharge_rate: true
 *     no_starship_fire_from_destroyed_racks: true
 *     starship_ambient_sounds: true
 *     no_player_warhead_slot_cap: true
 *
 * classic_hide_destroyed_engine_glows only changes drawing: the classic
 * renderer doesn't draw the engine glows of a destroyed or hidden mesh, nor
 * their lights, as the HD renderer already does.
 *
 * weapon_iff_swap changes which weapon a craft fires. The stock game gives
 * starship and launcher fire the weapon of the firing craft's side, for
 * example Imperial lasers instead of Rebel lasers. With false, no fire
 * changes; with true, every shot changes. A model settings file can choose
 * for its craft (craft_weapons.h).
 *
 * ai_attack_uses_max_speed_cache makes a craft under attack compare its own
 * top speed and its attacker's (aiFlight.maxSpeedCache), which stats
 * profiles can change (stats_profiles.h), instead of their records' top
 * speeds. It changes the simulation.
 *
 * fire_needs_decharge_rate and no_starship_fire_from_destroyed_racks change
 * when a craft can fire. They change the simulation.
 *
 * starship_ambient_sounds plays a craft's ambient sound from the sound bank
 * when the player is near it.
 *
 * no_player_warhead_slot_cap lets each warhead rack of a player's craft hold
 * more than 9 warheads, when the craft starts and when it reloads in the
 * hangar. The stock game only allows it for the Missile Boat. It changes the
 * simulation. */

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
int XwaMissionSettings_ClassicHideDestroyedEngineGlows(void);
/* 1 or 0 when flight/flight.yaml sets weapon_iff_swap, -1 for the stock
   rule. */
int XwaMissionSettings_WeaponIffSwap(void);
int XwaMissionSettings_AiAttackUsesMaxSpeedCache(void);
int XwaMissionSettings_FireNeedsDechargeRate(void);
int XwaMissionSettings_NoStarshipFireFromDestroyedRacks(void);
int XwaMissionSettings_StarshipAmbientSounds(void);
int XwaMissionSettings_NoPlayerWarheadSlotCap(void);

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
