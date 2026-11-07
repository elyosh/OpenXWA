#ifndef XWA_RUNTIME_MODELS_WARHEAD_LOCK_H
#define XWA_RUNTIME_MODELS_WARHEAD_LOCK_H

/* The warhead lock settings of a model, from the `warhead_lock` group of its
 * model settings file: the lock ranges and times of the player and of the
 * AI, and the homing level of warheads. Each value that the group leaves
 * out keeps the stock value that the caller passes.
 *
 * The values belong to the model that an object type loads in the mission,
 * so a swapped model has its own. The file has the ranges in metres and
 * the times in milliseconds, and the values here are in game units and
 * ticks. lock_time_ms only changes the HUD and the sounds. The other values
 * change the simulation. */

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
