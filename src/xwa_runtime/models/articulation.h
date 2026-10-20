#ifndef XWA_RUNTIME_MODELS_ARTICULATION_H
#define XWA_RUNTIME_MODELS_ARTICULATION_H

/* Articulations: the S-foils, landing gear and other moving parts a GLB
 * replacement lists on its model root. They work on the stock state: each
 * articulation turns CraftData.meshRotation[component], and S-foils and
 * landing gear share CraftData.sFoilState. They move on every sim step by a
 * fine angle (CraftData.articulationAngles).
 *
 * Each entry point applies only to a craft whose model lists the group it
 * drives; for every other craft the caller keeps the original code. All
 * entry points take an object-table index and are safe on any object. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ObjectRecord;
struct XwaFlightGroup;

typedef enum XwaArticulationTrigger {
	XWA_ARTICULATION_KEY_SFOILS,       /* V */
	XWA_ARTICULATION_KEY_LANDING_GEAR, /* Ctrl+L */
	XWA_ARTICULATION_HYPERSPACE_DEPARTURE,
	XWA_ARTICULATION_HYPERSPACE_ARRIVAL,
	XWA_ARTICULATION_LAUNCH
} XwaArticulationTrigger;

typedef enum XwaArticulationTriggerResult {
	XWA_ARTICULATION_STOCK,        /* no S-foils or landing gear: the original test */
	XWA_ARTICULATION_PASS,         /* go on with the original toggle */
	XWA_ARTICULATION_SKIP,         /* no toggle */
	XWA_ARTICULATION_NOT_EQUIPPED, /* no toggle; a key's "not equipped" message */
	XWA_ARTICULATION_REFUSED       /* no toggle: the other group is moving or in the way */
} XwaArticulationTriggerResult;

/* Nonzero when the object's model lists S-foils or landing gear. */
int XwaArticulation_HasSFoilsOrLandingGear(unsigned int objIdx);

/* FlightObject_InitMeshAnimationDefaults: S-foils and landing gear at their
   angle, state 2 (0 without them). Returns 0 for a craft without them. */
int XwaArticulation_InitLeavingHangar(unsigned int objIdx);

/* Every sim step: steps the S-foils and landing gear by sFoilState and
   settles it, with the original completion messages. In the high-rate mode,
   it also moves the stock S-foils of an original OPT (the X-wing, B-wing,
   Missile Boat, Skipray and shuttle), at the speeds of
   FlightObject_UpdateSpecialBehavior's phase 1. */
void XwaArticulation_StepSFoils(unsigned int objIdx);
/* Whether XwaArticulation_StepSFoils moves the object's S-foils, so phase 1
   skips it. */
int XwaArticulation_StepsSFoils(unsigned int objIdx);

/* Replaces the original X-wing/B-wing test at a trigger. For the keys, a
   pass also sets sFoilState so the original toggle moves the group the
   other way from where it is, and a refusal or "not equipped" sets
   *message (when not NULL) to the message to show. */
XwaArticulationTriggerResult XwaArticulation_Trigger(unsigned int objIdx, XwaArticulationTrigger trigger,
													  uint16_t* message);
/* XwaArticulation_Trigger for an automatic trigger: nonzero to go on with
   the original toggle, `stockTest` (the original X-wing/B-wing test) for a
   craft without S-foils or landing gear. */
int XwaArticulation_TriggerPasses(unsigned int objIdx, XwaArticulationTrigger trigger, int stockTest);

/* The message to show for the object in place of stock message
   `messageId`: for an articulated model, its own fire-gate text, and the
   landing-gear texts for the S-foil motion messages while a Ctrl+L motion
   runs; `messageId` otherwise. */
uint16_t XwaArticulation_Message(unsigned int objIdx, uint16_t messageId);

/* laser_firelasersystem's gate: nonzero when the craft may not fire;
   `stockBlocked` (the original sFoilState test) for a craft without S-foils
   or landing gear. */
int XwaArticulation_FireBlocked(unsigned int objIdx, int stockBlocked);

/* AI events: leaving and entering hyperspace, parking and entering a
   hangar. */
void XwaArticulation_AiOutOfHyperspaceInit(unsigned int objIdx);
void XwaArticulation_AiOutOfHyperspaceStep(unsigned int objIdx, int maneuverTimer);
void XwaArticulation_AiIntoHyperspace(unsigned int objIdx);
void XwaArticulation_AiParkOrder(unsigned int objIdx);
void XwaArticulation_AiParkStart(unsigned int objIdx);
void XwaArticulation_AiParkEnd(unsigned int objIdx);
void XwaArticulation_AiEnterHangarOrder(unsigned int objIdx);

/* FlightObject_AnimateCrewMeshRotations for a GLB replacement: steps the
   model's `crew` articulations (behaviors swing, random and random_pause),
   the `random` ones first, then the others, each in component order (the
   stock order: side, forward, limbs). With `resetToNeutral` they return to
   0. Returns 0 for a stock OPT, for which the original code runs; a
   replacement without crew animates nothing. */
int XwaArticulation_AnimateCrew(unsigned int objIdx, int resetToNeutral);

/* Nonzero when the object's model lists crew and sets crew.update_all: its
   crew animates for every object and in any view. */
int XwaArticulation_CrewUpdatesAll(unsigned int objIdx);

/* The hangar crew step for every object whose model sets
   crew.update_all. */
void XwaArticulation_StepHangarCrew(void);

/* Every sim step: the crew meshes' drawn angles follow the crew animation's
   own steps (on the crew timer) at their speed. For one object, and in the
   hangar for every object. */
void XwaArticulation_StepCrewAngles(unsigned int objIdx);
void XwaArticulation_StepHangarCrewAngles(void);

/* Hatches, hangar doors, warheads bay and cargo clamps. Hatches are off in
   multiplayer. */

/* Hangar exit: the hatches close at once. */
void XwaArticulation_CloseHatches(unsigned int objIdx);
/* Hangar_UpdateLaunch: the player's hatches by g_launchSeqPhase. */
void XwaArticulation_StepLaunchHatches(unsigned int objIdx);
/* Hangar_UpdateShuttleTrafficCycle: the hangar shuttle's hatches by its
   g_hangarShuttleState.moveState. */
void XwaArticulation_StepShuttleHatches(unsigned int objIdx);

typedef enum XwaArticulationShuttleStep {
	XWA_ARTICULATION_SHUTTLE_HATCHES, /* the original hatch/custom-mesh motion */
	XWA_ARTICULATION_SHUTTLE_OPEN,    /* S-foils and landing gear toward 0 */
	XWA_ARTICULATION_SHUTTLE_CLOSE    /* toward their angle */
} XwaArticulationShuttleStep;
/* Replaces one of the hangar shuttle's original mesh-rotation blocks: nonzero
   when the model has the groups it drives, so the caller skips the block. */
int XwaArticulation_StepShuttleBlock(unsigned int objIdx, XwaArticulationShuttleStep step, int dtTicks);

/* Once per region and sim step, after collide_collisions: the local player's
   warheads bay, then the region's cargo clamps and hangar doors. */
void XwaArticulation_StepWorld(void);

/* Reads the selections of mission `missionPath` (the `articulations` map
   of its mission file), replacing the previous mission's; the
   arguments are XwaObjectProfiles_LoadMission's. An object of a flight group
   that selects variant <name> uses its model's `variant.<name>.<group>`
   entries in place of the model's for the same group and component, and
   gains the others; every other object ignores variant entries. */
void XwaArticulation_LoadMission(const char* missionPath, const struct XwaFlightGroup* flightGroups, size_t stride,
								 int count);

/* Mission_InitFlightGroupObjectSlot, after the per-mesh state is reset: the
   S-foils and landing gear the mission starts at their angle for the
   object's flight group. sFoilState is left as it is. */
void XwaArticulation_InitMissionStart(unsigned int objIdx);

/* Nonzero when `object` is drawn with the B-wing's bridge alignment: the
   B-wing, and any object-table craft whose model has articulations. */
int XwaArticulation_AlignsBridge(const struct ObjectRecord* object);

/* The renderer's angles of the object's first `count` meshes, in 1/256 of a
   meshRotation unit (a full turn is 65536): an articulated mesh's fine angle,
   a crew mesh's followed angle, else meshRotation. */
void XwaArticulation_DrawnAngles(const struct ObjectRecord* object, uint16_t* angles, size_t count);

#ifdef __cplusplus
}
#endif

#endif
