#ifndef XWA_RUNTIME_MODELS_WARHEAD_LOCK_H
#define XWA_RUNTIME_MODELS_WARHEAD_LOCK_H

/* The warhead lock settings of a model, from the `warhead_lock` group of its
 * model settings file. All keys are optional:
 *
 *   warhead_lock:
 *     lock_range: {small: 102400, big: 244332, transport: 101805}
 *     lock_ticks: 354
 *     ticks_per_homing_level: 118
 *     ai_lock_range: {small: 102400, big: 244332, big_top_tier: 101805}
 *     ai_lock_ticks_per_tier: 472
 *
 *   - lock_range: how close the player's target must be for a lock. `big`
 *     is for starships, platforms, containers and freighters, `transport`
 *     for transports, and `small` for other targets. The stock values are
 *     101805, 244332 and 162888;
 *   - lock_ticks: how long the lock takes on the HUD and in the lock tone,
 *     in ticks. The stock value is 708, or 354 for the Missile Boat;
 *   - ticks_per_homing_level: a warhead's homing level is its craft's lock
 *     ticks divided by this value, up to 6. It applies to the player and to
 *     the AI. The stock value is 236;
 *   - ai_lock_range: how close the AI's target must be for the AI to fire a
 *     warhead. `big` is for starships, platforms, freighters, containers,
 *     weapon emplacements and, in newer missions, transports, `big_top_tier`
 *     replaces it for the AI of the top skill tier, and `small` is for
 *     other targets. The stock values are 101805, 203610 and 244332;
 *   - ai_lock_ticks_per_tier: the AI needs this many lock ticks for each
 *     skill tier, plus one, before it fires. The stock value is 472.
 *
 * The values belong to the model that an object type loads in the mission,
 * so a swapped model has its own. lock_ticks only changes the HUD and the
 * sounds. The other values change the simulation. */

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

typedef enum XwaWarheadLockValue {
	XWA_WARHEAD_LOCK_RANGE_SMALL,
	XWA_WARHEAD_LOCK_RANGE_BIG,
	XWA_WARHEAD_LOCK_RANGE_TRANSPORT,
	XWA_WARHEAD_LOCK_TICKS,
	XWA_WARHEAD_LOCK_TICKS_PER_HOMING_LEVEL,
	XWA_WARHEAD_LOCK_AI_RANGE_SMALL,
	XWA_WARHEAD_LOCK_AI_RANGE_BIG,
	XWA_WARHEAD_LOCK_AI_RANGE_BIG_TOP_TIER,
	XWA_WARHEAD_LOCK_AI_TICKS_PER_TIER,
	XWA_WARHEAD_LOCK_VALUES
} XwaWarheadLockValue;

/* Drops the previous mission's values. Called by the model settings reader. */
void XwaWarheadLock_ClearModelSettings(void);
/* Reads the `warhead_lock` group of `objectType`'s model settings file, or
   does nothing when `group` is NULL. `path` is the file, for the log.
   Called by the model settings reader. */
void XwaWarheadLock_ReadModelSettings(unsigned int objectType, const char* path,
									  const struct AeronConfigNode* group);

/* Returns `value` for object `objIdx`, or `stock` when its model doesn't set
   it. */
int XwaWarheadLock_Value(unsigned int objIdx, XwaWarheadLockValue value, int stock);

#ifdef __cplusplus
}
#endif

#endif
