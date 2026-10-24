#ifndef XWA_RUNTIME_MODELS_JAMMING_H
#define XWA_RUNTIME_MODELS_JAMMING_H

/* How a model jams the weapons of an enemy player near its hangar, from the
 * `jamming` group of its model settings file. All keys are optional:
 *
 *   jamming:
 *     enabled: true
 *     direction: down                 # all, up or down
 *     range: none                     # a distance, or none
 *     rule: hangar_and_jamming_points
 *
 *   - enabled: false turns jamming off for the model;
 *   - range: the player is jammed within this distance of the model's
 *     centre, whatever the other keys say. With none, `direction` and
 *     `rule` apply;
 *   - direction: up or down jams only above or below the model;
 *   - rule: where the model jams:
 *     - none: nowhere;
 *     - hangar_mesh_center: near the centre of a hangar mesh, but not near
 *       the hangar point;
 *     - hangar_point: near the hangar point, and not too far above or below
 *       it;
 *     - hangar_and_jamming_points: near the hangar point or the jamming
 *       points of the model.
 *
 * A key that the group leaves out keeps the stock rule of the object type:
 *   - the Imperial Research Centre doesn't jam;
 *   - the Factory jams within 4096;
 *   - the Imperial Star Destroyer, the Victory Star Destroyer and the
 *     Interdictor use hangar_and_jamming_points, and jam only below;
 *   - the Super Star Destroyer uses hangar_point. In the stock game, its
 *     test never jams, so it doesn't jam unless the model has the group, or
 *     flight/flight.yaml sets `rules.super_star_destroyer_jamming`;
 *   - the Repair Yard uses hangar_mesh_center;
 *   - the other types use hangar_and_jamming_points.
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
