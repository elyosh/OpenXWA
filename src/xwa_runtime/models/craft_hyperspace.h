#ifndef XWA_RUNTIME_MODELS_CRAFT_HYPERSPACE_H
#define XWA_RUNTIME_MODELS_CRAFT_HYPERSPACE_H

/* How craft behave around hyperspace, from the `hyperspace` group of their
 * model settings file and the `hyperspace` section of the mod's flight
 * settings:
 *   - gravity_well: whether an enemy craft of the model stops the player
 *     from jumping. Without the key, the stock rule of the object type
 *     applies: only the Interdictor and the Modified Strike Cruiser have one.
 *   - arrival and departure speeds of AI craft. A model's value wins over
 *     the flight settings' default. Without either, and in a mission whose
 *     use_hyperspace_speeds setting is false, arrivals and departures are
 *     the stock ones.
 *
 * The values belong to the model that an object type loads in the mission,
 * so a swapped model has its own. They change the simulation. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* The highest speed, in game units, that a hyperspace speed can have. Some
   code reads MobileObject.speed as signed. */
#define XWA_CRAFT_HYPERSPACE_MAX_SPEED 32767

/* Reads the `hyperspace` section of flight/flight.yaml. Called once at
   startup. */
void XwaCraftHyperspace_Init(void);
/* Drops the previous mission's values. Called by the model settings
   reader. */
void XwaCraftHyperspace_ClearModelSettings(void);
/* Reads the `hyperspace` group of `objectType`'s model settings file, or
   does nothing when `group` is NULL. `path` is the file, for the log. Called
   by the model settings reader. */
void XwaCraftHyperspace_ReadModelSettings(unsigned int objectType, const char* path,
										  const struct AeronConfigNode* group);

/* Nonzero when an enemy craft of `objectType` with working systems stops a
   player from jumping. */
int XwaCraftHyperspace_HasGravityWell(unsigned int objectType);

/* Sets `*speed` to the speed, in game units, at which AI craft of
   `objectType` arrive by hyperspace. Returns 0, and leaves `*speed` alone,
   when the stock arrival applies. */
int XwaCraftHyperspace_ArrivalSpeed(unsigned int objectType, int* speed);
/* How long an arriving craft takes to slow down to its normal speed, in
   ticks. */
int XwaCraftHyperspace_ArrivalBrakingTicks(void);

/* Sets `*speed` to the speed, in game units, at which AI craft of
   `objectType` jump away, and `*acceleration` to the speed they gain per
   game second while they do. Returns 0, and leaves both alone, when the
   stock departure applies. */
int XwaCraftHyperspace_DepartureSpeed(unsigned int objectType, int* speed, int* acceleration);

/* Called by Flight_UpdateCraftSteeringAndSpeed for a craft that enters
   hyperspace with working systems. When it is an AI craft whose departure
   has a set speed, speeds it up by its acceleration, up to that speed, and
   returns 1. Returns 0 for the stock acceleration, which a player's craft
   always keeps. */
int XwaCraftHyperspace_StepDeparture(unsigned int objIdx);
/* The speed at which object `objIdx` jumps away: 3600, or the set speed of
   an AI craft's departure. */
int XwaCraftHyperspace_DepartureJumpSpeed(unsigned int objIdx);

/* Arrivals at a set speed. The craft flies straight along its heading at
 * the arrival speed, then slows down to `endSpeed` over the braking time, so
 * that it stops where its path passes closest to its arrival point: the
 * point itself for a flight group leader, and its place in the formation for
 * a wingman. The step that gets there moves it by exactly the distance left,
 * so it never needs a snap. The maneuver then ends as in the stock game. */

/* The number of craft that can arrive at once. */
#define XWA_CRAFT_HYPERSPACE_ARRIVALS 128

/* Starts the arrival of object `objIdx` at `cruise` (game units), toward
   `aim` (world units), ending at `endSpeed`. Sets the craft's speed. Returns
   0 when too many craft are arriving; the stock arrival then runs. */
int XwaCraftHyperspace_BeginArrival(unsigned int objIdx, int cruise, int endSpeed, const int aim[3]);
/* Nonzero while `objIdx` arrives at a set speed. */
int XwaCraftHyperspace_ArrivalActive(unsigned int objIdx);
/* Nonzero once the arrival of `objIdx` has reached its point. */
int XwaCraftHyperspace_ArrivalFinished(unsigned int objIdx);
/* About how many ticks the arrival of `objIdx` still takes. */
int XwaCraftHyperspace_ArrivalTicksLeft(unsigned int objIdx);
/* Drops the arrival of `objIdx`, at the end of its maneuver. */
void XwaCraftHyperspace_EndArrival(unsigned int objIdx);
/* Every sim step, for a craft leaving hyperspace: sets its speed from the
   distance it has left. */
void XwaCraftHyperspace_StepArrivalSpeed(unsigned int objIdx);
/* Every sim step, before the craft moves: returns `scalar`, the distance it
   moves forward, limited to what it has left and to the 16 bits that a move
   can have. The step that reaches the point finishes the arrival and sets
   the end speed. */
int XwaCraftHyperspace_LimitArrivalMove(unsigned int objIdx, int scalar);
/* Called by paiman_initoutofhyperspacemaneuver for the AI craft `objIdx`,
   the craft of the AI context. When its model or the flight settings have an
   arrival speed, starts an arrival at that speed. The craft ends at the
   speed of its next plan, which is picked the same way as at the end of
   paiman_outofhyperspacemaneuver. The maneuver timer then counts down the
   ticks left, so the S-foils open about as long before the craft stops as in
   the stock arrival. */
void XwaCraftHyperspace_AiOutOfHyperspaceInit(unsigned int objIdx);
/* Called by paiman_outofhyperspacemaneuver. Returns 0 when `objIdx` has the
   stock arrival. Otherwise sets `*reached` when the arrival is over, and then
   drops the arrival. */
int XwaCraftHyperspace_AiOutOfHyperspaceStep(unsigned int objIdx, char* reached);
/* Starts a mission with no arrivals. */
void XwaCraftHyperspace_LoadMission(void);

/* The state of one arrival, for films. objIdx is -1 for a free entry. */
typedef struct XwaCraftHyperspaceArrival {
	int32_t objIdx;
	int32_t cruise;
	int32_t endSpeed;
	int32_t aim[3];
	int32_t finished;
} XwaCraftHyperspaceArrival;

void XwaCraftHyperspace_SaveFlightState(XwaCraftHyperspaceArrival arrivals[XWA_CRAFT_HYPERSPACE_ARRIVALS]);
void XwaCraftHyperspace_RestoreFlightState(
	const XwaCraftHyperspaceArrival arrivals[XWA_CRAFT_HYPERSPACE_ARRIVALS]);

#ifdef __cplusplus
}
#endif

#endif
