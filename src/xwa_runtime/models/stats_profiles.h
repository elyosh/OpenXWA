#ifndef XWA_RUNTIME_MODELS_STATS_PROFILES_H
#define XWA_RUNTIME_MODELS_STATS_PROFILES_H

/* Stats profiles: values of a craft that replace those of its craft record,
 * chosen by mission, by markings, for players and by difficulty.
 *
 * A model settings file holds the model's profiles, by name. Each profile is
 * whole: a variant doesn't take values from another one. The keys are those of
 * the object type files' `craft_record`, in the record's units, and all are
 * optional:
 *
 *   stats_profiles:
 *     default: {speed: 86, acceleration: 9, deceleration: 20, pitch_rate: 512,
 *               roll_rate: 256, yaw_rate: 1536, hull: 3045, shields: 3000,
 *               system_strength: 60, system_damage_hull_threshold: 1000,
 *               has_shields: true, has_hyperdrive: true,
 *               speed_increment: 1, speed_decrement: 1,
 *               warhead_collision_damage: false}
 *     default_easy: {...}
 *     default_player: {...}
 *     default_player_hard: {...}
 *
 * A mission file picks the profiles of its craft, by model key, and can scale
 * some values for every craft, in percent:
 *
 *   stats_profiles:
 *     models:
 *       escorttransport: {player: x1, markings: {2: etrmod}, all: rebel}
 *     percent: {hull: 120}
 *     player_percent: {hull: 400}
 *
 * The name of a craft's profile is its model's `player` value when a player
 * flies it, else the value for its markings, else `all`, else `default`.
 * The model is the one that the craft's object type loads in the mission,
 * a swap's target included. A name that the model has no profile for gives
 * no profile, even when the model has a `default` one. The profile is then
 * <name>_player_<difficulty>, else <name>_player for a player's craft, else
 * <name>_<difficulty>, else <name>. The difficulty is easy, medium or hard.
 *
 * `percent` scales speed, acceleration, deceleration, the three rates,
 * system_damage_hull_threshold, hull, system_strength and shields, after
 * the profile and also for craft without one. For a player's craft,
 * `player_percent`'s value replaces `percent`'s.
 *
 * The values apply when a craft is made (XwaStatsProfiles_ApplyNewCraft),
 * and where the game reads its record for them. They change the
 * simulation. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

typedef enum XwaStatsField {
	XWA_STATS_SPEED,
	XWA_STATS_ACCELERATION,
	XWA_STATS_DECELERATION,
	XWA_STATS_PITCH_RATE,
	XWA_STATS_ROLL_RATE,
	XWA_STATS_YAW_RATE,
	XWA_STATS_DAMAGE_THRESHOLD,
	XWA_STATS_HULL,
	XWA_STATS_SYSTEM_STRENGTH,
	XWA_STATS_SHIELDS,
	/* The fields above take the percents. */
	XWA_STATS_PERCENT_FIELDS,
	XWA_STATS_SPEED_INCREMENT = XWA_STATS_PERCENT_FIELDS,
	XWA_STATS_SPEED_DECREMENT,
	XWA_STATS_HAS_SHIELDS,
	XWA_STATS_HAS_HYPERDRIVE,
	XWA_STATS_WARHEAD_COLLISION_DAMAGE,
	XWA_STATS_FIELDS
} XwaStatsField;

/* Drops the previous mission's profiles. Called by the model settings
   reader. */
void XwaStatsProfiles_ClearModelSettings(void);
/* Reads the `stats_profiles` group of `objectType`'s model settings file,
   or does nothing when `group` is NULL. `path` is the file, for the log.
   Called by the model settings reader. */
void XwaStatsProfiles_ReadModelSettings(unsigned int objectType, const char* path,
										const struct AeronConfigNode* group);

/* Reads the `stats_profiles` section of mission `missionPath` (for example
   "missions\1b2m5g.tie"), replacing the previous mission's. */
void XwaStatsProfiles_LoadMission(const char* missionPath);

/* Returns the value of `field` for the craft of object `objIdx`: its
   profile's value, else `stock`, the craft record's value; then scaled by
   the mission's percent for the fields that take one. */
int32_t XwaStatsProfiles_Value(unsigned int objIdx, XwaStatsField field, int32_t stock);

/* Sets `value` to `field` of the `default` profile of `objectType`'s model,
   without the mission's choices and percents, for the ratings of a craft
   type. Returns 0 when the profile doesn't set it. */
int XwaStatsProfiles_DefaultValue(unsigned int objectType, XwaStatsField field, int32_t* value);

/* Gives a craft that Mission_InitFlightGroupObjectSlot has just set up the
   values of its profile and the mission's percents: the top speed, the three
   rates, the hull, the damage threshold and the shields. Then removes the
   shields and the hyperdrive that the craft doesn't have, which the stock code
   does before, and scales the craft's speed to its new top speed. */
void XwaStatsProfiles_ApplyNewCraft(unsigned int objIdx);

/* Scales the speed of object `objIdx` from its record's top speed to its
   own top speed (aiFlight.maxSpeedCache), for code that sets the speed
   from the record. Does nothing when both are the same. */
void XwaStatsProfiles_ScaleSpeed(unsigned int objIdx);

#ifdef __cplusplus
}
#endif

#endif
