#ifndef XWA_RUNTIME_MODELS_MISSION_SETTINGS_H
#define XWA_RUNTIME_MODELS_MISSION_SETTINGS_H

/* Mission settings and game rules for mods.
 *
 * Mission settings change things that the stock game hard-codes for some
 * missions. They come from the `settings` section of a mission file, with
 * defaults for every mission in the `mission_settings` section of the mod's
 * flight settings. A key of the mission file wins. When neither sets a key,
 * the game keeps the stock behaviour, which depends on the mission number:
 *   - red alert in mission 20;
 *   - message 143, shown when a fighter leaves by hyperspace, hidden in
 *     mission 49;
 *   - arrival messages of IFF 1 skipped in missions 49 to 52;
 *   - the player put in the turret at 0:00:08 in mission 1.
 *
 * Rules come from the `rules` section of the mod's flight settings and apply
 * to every mission. They are off by default, except player_weapon_jamming,
 * and weapon_iff_swap, whose default is the stock `turrets`.
 * classic_hide_destroyed_engine_glows only changes drawing. The other rules
 * and the mission settings change the simulation.
 *
 * Some rules replace a stock value or call:
 *   - ai_attack_uses_max_speed_cache compares top speeds with
 *     aiFlight.maxSpeedCache, which stats profiles change, in place of the
 *     speeds of the craft records;
 *   - gun_turret_hardpoints turns on the turret hardpoint rules
 *     (XwaTurretHardpoints_*);
 *   - weapon_rack_count_from_record counts laserSlotCount from every laser
 *     group and warhead launcher of the craft record. The stock game counts
 *     the laser groups, plus the turret racks of craft that fire from float
 *     hardpoints;
 *   - overlapping_fly_by_sounds lets fsfx_PlaySound start a fly-by or engine
 *     wash sound while it is already playing. */

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
