#ifndef XWA_RUNTIME_MODELS_DOCKING_H
#define XWA_RUNTIME_MODELS_DOCKING_H

/* A mod's docking rules, from the `docking` section of flight/flight.yaml:
 *
 *   docking:
 *     dock_to_small_sets_big: true
 *     ai_offsets_own_dock_point: true
 *     carried_offsets_carrier_dock_point: true
 *     player_approach_height: true
 *     player_snaps_to_target_orientation: true
 *
 * Every key is optional and false by default, which keeps the stock rule:
 *   - dock_to_small_sets_big: a model's DockToSmall hardpoint also sets its
 *     DockToBig point. A DockToBig hardpoint that comes after it still sets
 *     its own;
 *   - ai_offsets_own_dock_point: an AI craft that boards or picks up a target
 *     lines up its own dock point with the target's. In the stock game, it
 *     only offsets by the target's dock point;
 *   - carried_offsets_carrier_dock_point: a carried craft sits at the
 *     carrier's dock point, less its own. In the stock game, only its own
 *     dock point counts;
 *   - player_approach_height: the player's craft approaches the target at a
 *     height capped by the target's model (see below). The stock game
 *     approaches at twice the gap between the target's DockFromBig point and
 *     the player's DockToBig point, and not at all for SpaceColony2;
 *   - player_snaps_to_target_orientation: once docked, the player's craft
 *     takes the target's yaw, pitch and roll.
 *
 * The cap of the approach height comes from the `docking` group of the
 * target's model settings file:
 *
 *   docking: {approach_height: 0}
 *
 * Without it, the cap is 0 for SpaceColony2 and 932 for other models. The
 * approach height is then the smaller of the cap and the gap above, rounded
 * toward 0 to an even number.
 *
 * The rules change the simulation. */

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Reads the `docking` section of flight/flight.yaml. Called once at
   startup, before any model loads. */
void XwaDocking_Init(void);
/* Drops the previous mission's model values. Called by the model settings
   reader. */
void XwaDocking_ClearModelSettings(void);
/* Reads the `docking` group of `objectType`'s model settings file, or does
   nothing when `group` is NULL. `path` is the file, for the log. Called by
   the model settings reader. */
void XwaDocking_ReadModelSettings(unsigned int objectType, const char* path,
								  const struct AeronConfigNode* group);

int XwaDocking_DockToSmallSetsBig(void);
int XwaDocking_AiOffsetsOwnDockPoint(void);
int XwaDocking_CarriedOffsetsCarrierDockPoint(void);
int XwaDocking_PlayerApproachHeight(void);
int XwaDocking_PlayerSnapsToTargetOrientation(void);

/* The approach height of the player's craft towards a target of
   `targetType`, with player_approach_height: the smaller of the cap of the
   target's model and `dockGap`, the target's DockFromBig point less the
   player's DockToBig point. */
int XwaDocking_ApproachHeight(unsigned int targetType, int dockGap);

#ifdef __cplusplus
}
#endif

#endif
