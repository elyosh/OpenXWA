#ifndef XWA_RUNTIME_MODELS_DOCKING_H
#define XWA_RUNTIME_MODELS_DOCKING_H

/* A mod's docking rules, from the `docking` section of its flight
 * settings, and the cap of the approach height of each model, from the
 * `docking` group of its model settings file. Every rule is off by default,
 * which keeps the stock behaviour.
 *
 * The approach height is the smaller of the target model's cap and the gap
 * between the target's DockFromBig point and the player's DockToBig point,
 * rounded toward 0 to an even number. The stock game approaches at twice
 * the gap, and not at all for SpaceColony2.
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
