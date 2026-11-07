#ifndef XWA_RUNTIME_MODELS_JAMMING_H
#define XWA_RUNTIME_MODELS_JAMMING_H

/* How a model jams the weapons of an enemy player near its hangar, from the
 * `jamming` group of its model settings file. A key that the group leaves
 * out keeps the stock rule of the object type. In the stock game, the Super
 * Star Destroyer's test never jams, so it only jams when its model has the
 * group or the flight settings turn on `rules.super_star_destroyer_jamming`.
 *
 * The values belong to the model that an object type loads in the mission,
 * so a swapped model has its own. They change the simulation. */

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

typedef enum XwaJammingDirection {
	XWA_JAMMING_ALL,
	XWA_JAMMING_UP,
	XWA_JAMMING_DOWN,
} XwaJammingDirection;

typedef enum XwaJammingBehavior {
	XWA_JAMMING_RULE_NONE,
	XWA_JAMMING_RULE_HANGAR_MESH_CENTER,
	XWA_JAMMING_RULE_HANGAR_POINT,
	XWA_JAMMING_RULE_HANGAR_AND_JAMMING_POINTS,
} XwaJammingBehavior;

typedef struct XwaJammingRule {
	int enabled;
	XwaJammingDirection direction;
	/* -1 for none. */
	int range;
	XwaJammingBehavior behavior;
} XwaJammingRule;

/* Drops the previous mission's values. Called by the model settings
   reader. */
void XwaJamming_ClearModelSettings(void);
/* Reads the `jamming` group of `objectType`'s model settings file, or does
   nothing when `group` is NULL. `path` is the file, for the log. Called by
   the model settings reader. */
void XwaJamming_ReadModelSettings(unsigned int objectType, const char* path,
								  const struct AeronConfigNode* group);

/* Sets `rule` to the jamming rule of `objectType`: its model's, or the stock
   rule of the type. Returns whether the model has the `jamming` group. */
int XwaJamming_Rule(unsigned int objectType, XwaJammingRule* rule);

#ifdef __cplusplus
}
#endif

#endif
