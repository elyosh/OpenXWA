#ifndef XWA_RUNTIME_MODELS_MISSION_SETTINGS_H
#define XWA_RUNTIME_MODELS_MISSION_SETTINGS_H

/* Mission settings and game rules for mods.
 *
 * Mission settings change things that the stock game hard-codes for some
 * missions. A mission file sets them in its `settings` section. The
 * `mission_settings` section of flight/flight.yaml sets defaults for every
 * mission. Both sections take the same keys, and all keys are optional:
 *
 *   settings:
 *     red_alert: true                       # the hangar's red alert
 *     hide_hyperspace_exit_messages: true   # message 143
 *     hide_friendly_arrival_messages: true  # "entering area" messages
 *     skip_arrival_messages_iff: 1          # an IFF, all or none
 *     force_player_in_turret: "0:00:08"     # a mission time, or false
 *     disable_player_lasers: true
 *     disable_player_warheads: true
 *     warhead_collision_damage: false
 *     damage_through_shields_on_hard: true
 *     target_key: { flight_groups: [5, 6], method: cycle, only_not_inspected: true }
 *     proximity_triggers_check_craft_only: true
 *     order_of_battle:
 *       flight_groups: [3, 4]
 *       names: { 3: "Home One" }
 *       lines: ["Fleet Assignment: Admiral Thrawn", "", "Command Ship: Chimaera"]
 *       top: 140
 *     hyperspace_abort_sound_regions: [2]
 *
 * When a key is missing, the game does what the stock game does:
 *   - red alert in mission 20;
 *   - message 143 hidden in mission 49;
 *   - arrival messages of IFF 1 skipped in missions 49 to 52;
 *   - the player put in the turret at 0:00:08 in mission 1.
 *
 * Some keys need more explanation:
 *   - hide_hyperspace_exit_messages: message 143 is the message shown when a
 *     fighter leaves by hyperspace.
 *   - hide_friendly_arrival_messages: the "entering area" messages are shown
 *     when a friendly flight group arrives in the player's region.
 *   - proximity_triggers_check_craft_only: proximity triggers ignore every
 *     object that isn't a craft.
 *   - hyperspace_abort_sound_regions: when the player arrives by hyperspace
 *     in one of these regions, the game plays the abort sound instead of the
 *     arrival sound.
 *
 * The `rules` section of flight/flight.yaml changes rules for every mission.
 * The true or false rules are off by default, except player_weapon_jamming:
 *
 *   rules:
 *     reset_combat_music_at_mission_load: true
 *     hangar_red_alert_any_mothership: true
 *     classic_hide_destroyed_engine_glows: true
 *     weapon_iff_swap: none              # turrets, none or all
 *     ai_attack_uses_max_speed_cache: true
 *     fire_needs_decharge_rate: true
 *     no_starship_fire_from_destroyed_racks: true
 *     starship_ambient_sounds: true
 *     no_player_warhead_slot_cap: true
 *     gunner_weapon_select_in_seat: true
 *     gunner_fires_selected_group: true
 *     gunner_warheads_need_cockpit_hardpoint: true
 *     no_fire_from_destroyed_hardpoints: true
 *     tie_fighter_energy_step_always_high: true
 *     player_turret_racks_keep_charge: true
 *     rearm_follows_both_statuses: true
 *     warhead_collision_bonus_any_target: true
 *     turbolaser_hardpoint_types: true
 *     gun_turret_hardpoints: true
 *     player_weapon_jamming: false
 *     super_star_destroyer_jamming: true
 *     two_launcher_fixes: true
 *     starship_missile_defense_any_warhead: true
 *     weapon_rack_count_from_record: true
 *     overlapping_fly_by_sounds: true
 *
 * classic_hide_destroyed_engine_glows only changes drawing. The classic
 * renderer stops drawing the engine glows and lights of a destroyed or
 * hidden mesh, like the HD renderer already does. All other rules change the
 * simulation.
 *
 * weapon_iff_swap decides which shots use the weapon of the firing craft's
 * side. For example, an Imperial craft fires Imperial lasers instead of
 * Rebel lasers. The values are:
 *   - turrets: only shots from turret racks. This is what the stock game
 *     does, and the default;
 *   - none: no shots;
 *   - all: all shots.
 * The `weapons.switch_by_iff` key of a model settings file overrides it for
 * that model.
 *
 * ai_attack_uses_max_speed_cache: a craft under attack compares its top
 * speed with its attacker's using aiFlight.maxSpeedCache, which stats
 * profiles can change. The stock game uses the top speeds of the craft
 * records.
 *
 * fire_needs_decharge_rate and no_starship_fire_from_destroyed_racks change
 * when a craft can fire.
 *
 * starship_ambient_sounds: a craft plays its ambient sound from the sound
 * bank when the player is near it.
 *
 * no_player_warhead_slot_cap: each warhead rack of a player's craft can hold
 * more than 9 warheads, at the start and when the craft reloads in the
 * hangar. The stock game only allows this for the Missile Boat.
 *
 * Gunner rules, for a player in a gunner seat:
 *   - gunner_weapon_select_in_seat: the weapon key works in a seat, and
 *     changing seats keeps the selected weapon. In the stock game, the key
 *     doesn't work and changing seats selects the lasers again.
 *   - gunner_fires_selected_group: the seat fires the first turret laser
 *     group, or the first ion group when the player selected ions, if the
 *     gunner cockpit has a hardpoint for it. The stock game fires all turret
 *     laser groups.
 *   - gunner_warheads_need_cockpit_hardpoint: the seat fires warheads only
 *     when the gunner cockpit has a warhead hardpoint. The stock game always
 *     allows it.
 *
 * Weapon rules:
 *   - no_fire_from_destroyed_hardpoints: a craft doesn't fire lasers or
 *     warheads from a hardpoint on a destroyed mesh.
 *   - tie_fighter_energy_step_always_high: the TIE Fighter always gets its
 *     larger speed change for each step of energy moved to or from its
 *     engines. In the stock game, it gets it only with spare engine power.
 *   - player_turret_racks_keep_charge: a player's shot doesn't drain the
 *     turret racks. In the stock game, a starfighter's shot drains every rack
 *     that fires.
 *   - rearm_follows_both_statuses: when a supply craft rearms a craft, it
 *     reads both warhead statuses of the craft's flight group, like the other
 *     warhead loads. The stock game only reads the first status.
 *   - warhead_collision_bonus_any_target: when a craft hits another craft,
 *     its warheads always add to the damage. In the stock game, they add to
 *     it only when the target is a starship or a platform.
 *   - turbolaser_hardpoint_types: OPT hardpoint types 33 to 37 fire weapon
 *     types 301 to 305, and type 38 fires weapon type 307. In the stock game,
 *     hardpoint type N fires weapon type N + 279.
 *   - gun_turret_hardpoints: the hardpoints of gun turret meshes follow the
 *     rules in turret_hardpoints.h. A gun turret mesh doesn't fire at targets
 *     behind it.
 *
 * Jamming rules, for a player near an enemy starship or platform:
 *   - player_weapon_jamming: false turns all jamming off.
 *   - super_star_destroyer_jamming: the Super Star Destroyer jams near its
 *     hangar. In the stock game, it never jams.
 *
 * Warhead rules:
 *   - two_launcher_fixes: fixes for craft with two warhead launchers:
 *       - the hangar menu shows the second launcher for any such craft, not
 *         only the Missile Boat, and lists its warheads;
 *       - loading a launcher keeps both launchers counted;
 *       - the second launcher gets the warhead count of the warhead it
 *         loads;
 *       - a supply craft reloads the warhead that each launcher holds, not
 *         the flight group's warhead;
 *       - the weapon key skips an empty launcher.
 *   - starship_missile_defense_any_warhead: a starship's missile defense can
 *     fire any warhead. The stock game fires only missiles and advanced
 *     missiles.
 *   - weapon_rack_count_from_record: a craft's rack count (laserSlotCount)
 *     includes the racks of all laser groups and warhead launchers of its
 *     craft record. The stock game counts the racks of the laser groups, plus
 *     the turret racks of craft that fire from float hardpoints.
 *
 * overlapping_fly_by_sounds: the fly-by and engine wash sounds of several
 * craft can play at the same time. In the stock game, fsfx_PlaySound doesn't
 * start one of these sounds while it is already playing. */

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

/* For films: copies the last target that each player picked with the
   targetCycle key (-1: none yet) to `lastTarget`, or back from it. The array
   has one value per player (XWA_PLAYER_COUNT). */
void XwaMissionSettings_SaveFlightState(int32_t* lastTarget);
void XwaMissionSettings_RestoreFlightState(const int32_t* lastTarget);

/* The rules of flight/flight.yaml. */
int XwaMissionSettings_ResetCombatMusicAtMissionLoad(void);
int XwaMissionSettings_HangarRedAlertAnyMothership(void);
int XwaMissionSettings_ClassicHideDestroyedEngineGlows(void);
/* Which shots switch to the weapon of the firing craft's side. Used by
   weapon_iff_swap and by a model's weapons.switch_by_iff. */
typedef enum XwaWeaponIffSwap {
	XWA_WEAPON_IFF_SWAP_TURRETS = -1, /* the stock rule: only turret racks switch */
	XWA_WEAPON_IFF_SWAP_NONE = 0,
	XWA_WEAPON_IFF_SWAP_ALL = 1,
} XwaWeaponIffSwap;

/* Reads `turrets`, `none` or `all` into `out`. Returns 0 for any other
   text, or NULL. */
int XwaMissionSettings_ParseWeaponIffSwap(const char* text, XwaWeaponIffSwap* out);
XwaWeaponIffSwap XwaMissionSettings_WeaponIffSwap(void);
int XwaMissionSettings_AiAttackUsesMaxSpeedCache(void);
int XwaMissionSettings_FireNeedsDechargeRate(void);
int XwaMissionSettings_NoStarshipFireFromDestroyedRacks(void);
int XwaMissionSettings_StarshipAmbientSounds(void);
int XwaMissionSettings_NoPlayerWarheadSlotCap(void);
int XwaMissionSettings_GunnerWeaponSelectInSeat(void);
int XwaMissionSettings_GunnerFiresSelectedGroup(void);
int XwaMissionSettings_GunnerWarheadsNeedCockpitHardpoint(void);
int XwaMissionSettings_NoFireFromDestroyedHardpoints(void);
int XwaMissionSettings_TieFighterEnergyStepAlwaysHigh(void);
int XwaMissionSettings_PlayerTurretRacksKeepCharge(void);
int XwaMissionSettings_RearmFollowsBothStatuses(void);
int XwaMissionSettings_WarheadCollisionBonusAnyTarget(void);
int XwaMissionSettings_TurbolaserHardpointTypes(void);
int XwaMissionSettings_GunTurretHardpoints(void);
/* 1 unless flight/flight.yaml sets player_weapon_jamming to false. */
int XwaMissionSettings_PlayerWeaponJamming(void);
int XwaMissionSettings_SuperStarDestroyerJamming(void);
int XwaMissionSettings_TwoLauncherFixes(void);
int XwaMissionSettings_StarshipMissileDefenseAnyWarhead(void);
int XwaMissionSettings_WeaponRackCountFromRecord(void);
int XwaMissionSettings_OverlappingFlyBySounds(void);

/* 1 or 0 when the mission sets the value, -1 for the stock rule. */
int XwaMissionSettings_RedAlert(void);
int XwaMissionSettings_HideHyperspaceExitMessages(void);
/* 1 when the mission hides the "entering area" messages. */
int XwaMissionSettings_HideFriendlyArrivalMessages(void);
/* Whether the arrival messages of a flight group of `iff` are skipped. */
int XwaMissionSettings_SkipArrivalMessages(int iff);
/* Whether the player is put in the turret at this mission time. */
int XwaMissionSettings_ForcePlayerInTurretAt(int hours, int minutes, int seconds);
int XwaMissionSettings_ForcePlayerInTurret(void);

int XwaMissionSettings_PlayerLasersDisabled(void);
int XwaMissionSettings_PlayerWarheadsDisabled(void);
int XwaMissionSettings_WarheadCollisionDamage(void);
int XwaMissionSettings_DamageThroughShieldsOnHard(void);
int XwaMissionSettings_ProximityTriggersCheckCraftOnly(void);
/* Whether a hyperspace jump to `region` plays the abort sound. */
int XwaMissionSettings_HyperspaceAbortSoundRegion(int region);

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
