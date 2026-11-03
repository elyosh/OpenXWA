#ifndef XWA_RUNTIME_MODELS_STATS_PROFILES_H
#define XWA_RUNTIME_MODELS_STATS_PROFILES_H

/* Stats profiles: values of a craft that replace those of its craft
 * record, chosen by mission, by markings, for players and by difficulty. A
 * model settings file holds the model's profiles in its `stats_profiles`
 * group. A mission file picks the profiles of its craft, and can scale some
 * values of every craft, in its `stats_profiles` section.
 *
 * The model is the one that the craft's object type loads in the mission, a
 * swap's target included. A name that the model has no profile for gives no
 * profile, even when the model has a `default` one. The percents apply
 * after the profile, and also to craft without one.
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
