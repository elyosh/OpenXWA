#include "xwa_runtime/models/craft_hyperspace.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/models/mission_settings.h"
#include "xwa_runtime/models/units.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* The stock departure: AI craft speed up by 1500 game units per game second
   and jump at 3600. */
#define HYPERSPACE_STOCK_DEPARTURE_SPEED 3600
#define HYPERSPACE_STOCK_DEPARTURE_ACCELERATION 1500
/* The highest departure acceleration, in game units per game second. */
#define HYPERSPACE_MAX_ACCELERATION 10000000
/* The default braking time, in ticks: half a game second. */
#define HYPERSPACE_DEFAULT_BRAKING_TICKS 118
/* The longest braking time, in ticks: a minute. */
#define HYPERSPACE_MAX_BRAKING_TICKS (60 * 236)

typedef struct HyperspaceModel {
	int8_t gravityWell;     /* -1: the stock rule */
	int32_t arrivalSpeed;   /* 0: the default */
	int32_t departureSpeed; /* 0: the default */
} HyperspaceModel;

static HyperspaceModel s_models[OBJ_Count];
static int32_t s_arrivalSpeed;          /* 0: stock arrivals */
static int32_t s_departureSpeed;        /* 0: the stock speed */
static int32_t s_departureAcceleration; /* 0: the stock acceleration */
static int s_brakingTicks;
static XwaCraftHyperspaceArrival s_arrivals[XWA_CRAFT_HYPERSPACE_ARRIVALS];

/* Reads `key` of `group`, a speed in MGLT, into `out`. When the value isn't a
   speed from 1 game unit to `max`, logs a warning and leaves `out` as it
   is. */
static void Hyperspace_ReadSpeed(const char* path, const char* where, const AeronConfigNode* group,
								 const char* key, int32_t max, int32_t* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(group, key);

	if (node != NULL && !XwaUnits_Read(node, XWA_UNIT_MGLT, 1, max, out)) {
		Aeron_LogWarn("xwa.models", "%s:%d: %s.%s must be a speed from %.3f to %.3f MGLT; it is ignored",
					  path, AeronConfigNode_Line(node), where, key, XwaUnits_FromGame(XWA_UNIT_MGLT, 1.0),
					  XwaUnits_FromGame(XWA_UNIT_MGLT, (double)max));
	}
}

void XwaCraftHyperspace_Init(void) {
	const AeronConfigNode* section = AeronConfigNode_MapGet(XwaFlightConfig_Root(), "hyperspace");
	const AeronConfigNode* braking;

	s_arrivalSpeed = 0;
	s_departureSpeed = 0;
	s_departureAcceleration = 0;
	s_brakingTicks = HYPERSPACE_DEFAULT_BRAKING_TICKS;
	/* Until a mission loads, no model has values and no craft arrives. */
	XwaCraftHyperspace_ClearModelSettings();
	XwaCraftHyperspace_LoadMission();
	if (section == NULL) {
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.models", "%s:%d: hyperspace must be a map; it is ignored", XWA_FLIGHT_CONFIG_PATH,
					   AeronConfigNode_Line(section));
		return;
	}
	Hyperspace_ReadSpeed(XWA_FLIGHT_CONFIG_PATH, "hyperspace", section, "arrival_speed_mglt",
						 XWA_CRAFT_HYPERSPACE_MAX_SPEED, &s_arrivalSpeed);
	Hyperspace_ReadSpeed(XWA_FLIGHT_CONFIG_PATH, "hyperspace", section, "departure_speed_mglt",
						 XWA_CRAFT_HYPERSPACE_MAX_SPEED, &s_departureSpeed);
	Hyperspace_ReadSpeed(XWA_FLIGHT_CONFIG_PATH, "hyperspace", section, "departure_acceleration_mglt",
						 HYPERSPACE_MAX_ACCELERATION, &s_departureAcceleration);
	braking = AeronConfigNode_MapGet(section, "arrival_braking_seconds");
	if (braking != NULL) {
		const AeronConfigNodeType type = AeronConfigNode_Type(braking);
		const double seconds = AeronConfigNode_Float(braking, -1.0);

		if ((type != AERON_CONFIG_FLOAT && type != AERON_CONFIG_INT) || seconds < 0.0 ||
			seconds * 236.0 > HYPERSPACE_MAX_BRAKING_TICKS) {
			Aeron_LogWarn("xwa.models",
						  "%s:%d: hyperspace.arrival_braking_seconds must be from 0 to 60; it is ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(braking));
		} else {
			s_brakingTicks = (int)(seconds * 236.0 + 0.5);
		}
	}
}

void XwaCraftHyperspace_ClearModelSettings(void) {
	unsigned int type;

	memset(s_models, 0, sizeof s_models);
	for (type = 0; type < OBJ_Count; ++type) {
		s_models[type].gravityWell = -1;
	}
}

void XwaCraftHyperspace_ReadModelSettings(unsigned int objectType, const char* path,
										  const AeronConfigNode* group) {
	HyperspaceModel* model;
	const AeronConfigNode* well;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: hyperspace isn't a map. It is ignored", path);
		return;
	}
	model = &s_models[objectType];
	well = AeronConfigNode_MapGet(group, "gravity_well");
	if (well != NULL) {
		if (AeronConfigNode_Type(well) != AERON_CONFIG_BOOL) {
			Aeron_LogWarn("xwa.models", "%s:%d: hyperspace.gravity_well must be true or false; it is ignored",
						  path, AeronConfigNode_Line(well));
		} else {
			model->gravityWell = AeronConfigNode_Bool(well, 0) != 0;
		}
	}
	Hyperspace_ReadSpeed(path, "hyperspace", group, "arrival_speed_mglt", XWA_CRAFT_HYPERSPACE_MAX_SPEED,
						 &model->arrivalSpeed);
	Hyperspace_ReadSpeed(path, "hyperspace", group, "departure_speed_mglt", XWA_CRAFT_HYPERSPACE_MAX_SPEED,
						 &model->departureSpeed);
}

int XwaCraftHyperspace_HasGravityWell(unsigned int objectType) {
	if (objectType < OBJ_Count && s_models[objectType].gravityWell != -1) {
		return s_models[objectType].gravityWell;
	}
	return objectType == OBJ_Interdictor2 || objectType == OBJ_ModStrikeCruiser;
}

int XwaCraftHyperspace_ArrivalSpeed(unsigned int objectType, int* speed) {
	int32_t value;

	if (objectType >= OBJ_Count || !XwaMissionSettings_UseHyperspaceSpeeds()) {
		return 0;
	}
	value = s_models[objectType].arrivalSpeed != 0 ? s_models[objectType].arrivalSpeed : s_arrivalSpeed;
	if (value == 0) {
		return 0;
	}
	*speed = value;
	return 1;
}

int XwaCraftHyperspace_ArrivalBrakingTicks(void) { return s_brakingTicks; }

int XwaCraftHyperspace_DepartureSpeed(unsigned int objectType, int* speed, int* acceleration) {
	int32_t value;

	if (objectType >= OBJ_Count || !XwaMissionSettings_UseHyperspaceSpeeds()) {
		return 0;
	}
	value = s_models[objectType].departureSpeed != 0 ? s_models[objectType].departureSpeed : s_departureSpeed;
	if (value == 0 && s_departureAcceleration == 0) {
		return 0;
	}
	*speed = value != 0 ? value : HYPERSPACE_STOCK_DEPARTURE_SPEED;
	*acceleration =
		s_departureAcceleration != 0 ? s_departureAcceleration : HYPERSPACE_STOCK_DEPARTURE_ACCELERATION;
	return 1;
}

/* ---- Arrivals --------------------------------------------------------------- */

/* World units that a speed of 1 moves in one tick: (4660 / 256) / 236. */
#define HYPERSPACE_UNITS_PER_SPEED_TICK (4660.0 / 256.0 / 236.0)
/* An arrival this close to its point is done. */
#define HYPERSPACE_ARRIVAL_DONE_UNITS 8.0

/* One step can move a craft at most this far: Object_AddForwardMove takes a
   16-bit distance. */
#define HYPERSPACE_MAX_STEP_UNITS 65535

static XwaCraftHyperspaceArrival* Hyperspace_FindArrival(unsigned int objIdx) {
	int i;

	for (i = 0; i < XWA_CRAFT_HYPERSPACE_ARRIVALS; ++i) {
		if (s_arrivals[i].objIdx == (int32_t)objIdx) {
			return &s_arrivals[i];
		}
	}
	return NULL;
}

void XwaCraftHyperspace_LoadMission(void) {
	int i;

	memset(s_arrivals, 0, sizeof s_arrivals);
	for (i = 0; i < XWA_CRAFT_HYPERSPACE_ARRIVALS; ++i) {
		s_arrivals[i].objIdx = -1;
	}
}

/* The distance that `arrival` has left along the craft's heading, in world
   units: to the point where the craft's path passes closest to the aim. */
static double Hyperspace_DistanceLeft(const XwaCraftHyperspaceArrival* arrival) {
	const ObjectRecord* obj = &g_objectTable[arrival->objIdx];
	const MobileObject* mobj = obj->mobj;
	const double dx = (double)arrival->aim[0] - obj->world_x;
	const double dy = (double)arrival->aim[1] - obj->world_y;
	const double dz = (double)arrival->aim[2] - obj->world_z;
	double fx = mobj ? mobj->moveX : 0.0, fy = mobj ? mobj->moveY : 0.0, fz = mobj ? mobj->moveZ : 0.0;
	double length = sqrt(fx * fx + fy * fy + fz * fz);

	/* Before the craft's first move, its heading points at the aim. */
	if (length < 1.0) {
		fx = dx;
		fy = dy;
		fz = dz;
		length = sqrt(fx * fx + fy * fy + fz * fz);
		if (length < 1.0) {
			return 0.0;
		}
	}
	return (dx * fx + dy * fy + dz * fz) / length;
}

int XwaCraftHyperspace_BeginArrival(unsigned int objIdx, int cruise, int endSpeed, const int aim[3]) {
	XwaCraftHyperspaceArrival* arrival = Hyperspace_FindArrival(objIdx);

	if (objIdx >= XWA_MAX_OBJECTS || g_objectTable[objIdx].mobj == NULL) {
		return 0;
	}
	if (arrival == NULL) {
		arrival = Hyperspace_FindArrival((unsigned int)-1);
	}
	if (arrival == NULL) {
		Aeron_LogWarn("xwa.models",
					  "more than %d craft arrive by hyperspace at once; one arrives the stock way",
					  XWA_CRAFT_HYPERSPACE_ARRIVALS);
		return 0;
	}
	arrival->objIdx = (int32_t)objIdx;
	arrival->cruise = cruise;
	arrival->endSpeed = endSpeed < 0 ? 0 : (endSpeed > cruise ? cruise : endSpeed);
	arrival->aim[0] = aim[0];
	arrival->aim[1] = aim[1];
	arrival->aim[2] = aim[2];
	arrival->finished = 0;
	g_objectTable[objIdx].mobj->speed = (uint16_t)cruise;
	return 1;
}

int XwaCraftHyperspace_ArrivalActive(unsigned int objIdx) { return Hyperspace_FindArrival(objIdx) != NULL; }

int XwaCraftHyperspace_ArrivalFinished(unsigned int objIdx) {
	const XwaCraftHyperspaceArrival* arrival = Hyperspace_FindArrival(objIdx);

	return arrival != NULL && arrival->finished;
}

int XwaCraftHyperspace_ArrivalTicksLeft(unsigned int objIdx) {
	const XwaCraftHyperspaceArrival* arrival = Hyperspace_FindArrival(objIdx);
	double distance;

	if (arrival == NULL || arrival->finished) {
		return 0;
	}
	distance = Hyperspace_DistanceLeft(arrival);
	if (distance <= 0.0) {
		return 0;
	}
	return (int)(distance / (arrival->cruise * HYPERSPACE_UNITS_PER_SPEED_TICK)) + s_brakingTicks / 2;
}

void XwaCraftHyperspace_EndArrival(unsigned int objIdx) {
	XwaCraftHyperspaceArrival* arrival = Hyperspace_FindArrival(objIdx);

	if (arrival != NULL) {
		memset(arrival, 0, sizeof *arrival);
		arrival->objIdx = -1;
	}
}

/* Returns the arrival of `objIdx` while its craft is leaving hyperspace.
   When the craft is no longer leaving hyperspace, for example because it was
   destroyed on its way, drops the arrival and returns NULL. */
static XwaCraftHyperspaceArrival* Hyperspace_LiveArrival(unsigned int objIdx) {
	XwaCraftHyperspaceArrival* arrival = Hyperspace_FindArrival(objIdx);
	const MobileObject* mobj;

	if (arrival == NULL) {
		return NULL;
	}
	mobj = g_objectTable[objIdx].mobj;
	if (mobj == NULL || mobj->pCraft == NULL || mobj->pCraft->craftState != CRAFT_STATE_LEAVING_HYPERSPACE) {
		XwaCraftHyperspace_EndArrival(objIdx);
		return NULL;
	}
	return arrival;
}

void XwaCraftHyperspace_StepArrivalSpeed(unsigned int objIdx) {
	const XwaCraftHyperspaceArrival* arrival = Hyperspace_LiveArrival(objIdx);
	const double k = HYPERSPACE_UNITS_PER_SPEED_TICK;
	double cruise, end, distance, speed;

	if (arrival == NULL || arrival->finished) {
		return;
	}
	cruise = arrival->cruise * k;
	end = arrival->endSpeed * k;
	distance = Hyperspace_DistanceLeft(arrival);
	if (distance < 0.0) {
		distance = 0.0;
	}
	speed = cruise;
	if (s_brakingTicks > 0 && cruise > end) {
		/* A steady deceleration that reaches the end speed at the point. */
		const double deceleration = (cruise - end) / s_brakingTicks;
		const double braking = sqrt(end * end + 2.0 * deceleration * distance);

		if (braking < speed) {
			speed = braking;
		}
	}
	speed /= k;
	if (speed < 1.0) {
		speed = 1.0;
	}
	g_objectTable[objIdx].mobj->speed = (uint16_t)(speed + 0.5);
}

int XwaCraftHyperspace_LimitArrivalMove(unsigned int objIdx, int scalar) {
	XwaCraftHyperspaceArrival* arrival = Hyperspace_LiveArrival(objIdx);
	double distance;

	if (arrival == NULL || arrival->finished) {
		return scalar;
	}
	distance = Hyperspace_DistanceLeft(arrival);
	if (distance > HYPERSPACE_ARRIVAL_DONE_UNITS && scalar < distance) {
		/* A long step moves the craft less than its speed would; the next
		   step measures the distance again. */
		return scalar > HYPERSPACE_MAX_STEP_UNITS ? HYPERSPACE_MAX_STEP_UNITS : scalar;
	}
	if (distance > HYPERSPACE_MAX_STEP_UNITS) {
		return HYPERSPACE_MAX_STEP_UNITS;
	}
	/* This step reaches the point: it moves the craft there, and the craft
	   goes on at its end speed until the maneuver ends. */
	arrival->finished = 1;
	g_objectTable[objIdx].mobj->speed = (uint16_t)arrival->endSpeed;
	return distance > 0.0 ? (int)(distance + 0.5) : 0;
}

void XwaCraftHyperspace_SaveFlightState(XwaCraftHyperspaceArrival arrivals[XWA_CRAFT_HYPERSPACE_ARRIVALS]) {
	memcpy(arrivals, s_arrivals, sizeof s_arrivals);
}

void XwaCraftHyperspace_RestoreFlightState(
	const XwaCraftHyperspaceArrival arrivals[XWA_CRAFT_HYPERSPACE_ARRIVALS]) {
	memcpy(s_arrivals, arrivals, sizeof s_arrivals);
}

/* The speed of `craft` when its arrival ends. `craft` is the craft of the AI
   context. When the arrival ends, paiman_outofhyperspacemaneuver picks the
   next plan, and gives the craft a speed of 0 for a null or stationary plan,
   else its top speed. This function picks the plan in the same way. */
static int Hyperspace_AiArrivalEndSpeed(const CraftData* craft) {
	uint8_t planId;
	const PlanRecord* plan;

	if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "homeviahyperspacepln") == 0) {
		planId =
			craft->leader_obj_idx == -1 ? g_builtinPlanIdByNameIndex[47] : g_builtinPlanIdByNameIndex[48];
	} else {
		const uint16_t order = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								   .fg
								   .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										   g_paiContext.curOrderCoord.fields.orderSlot]
								   .order;
		planId = craft->leader_obj_idx == -1
					 ? g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]]
					 : g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
	}
	plan = &g_planTable[planId];
	if (strcmp(plan->name, "nullpln") == 0 || strcmp(plan->name, "stationaryldrpln") == 0 ||
		strcmp(plan->name, "stationaryflwpln") == 0) {
		return 0;
	}
	return craft->aiFlight.maxSpeedCache;
}

void XwaCraftHyperspace_AiOutOfHyperspaceInit(unsigned int objIdx) {
	int cruise;
	int aim[3];
	int ticks;

	if (objIdx >= XWA_MAX_OBJECTS || g_objectTable[objIdx].mobj == NULL ||
		g_objectTable[objIdx].mobj->pCraft == NULL ||
		!XwaCraftHyperspace_ArrivalSpeed(g_objectTable[objIdx].objectType, &cruise)) {
		return;
	}
	aim[0] = g_paiContext.aiController->aimPointX;
	aim[1] = g_paiContext.aiController->aimPointY;
	aim[2] = g_paiContext.aiController->aimPointZ;
	if (!XwaCraftHyperspace_BeginArrival(
			objIdx, cruise, Hyperspace_AiArrivalEndSpeed(g_objectTable[objIdx].mobj->pCraft), aim)) {
		return;
	}
	/* The stock timer starts at 2596 ticks; the S-foils open at 684. */
	ticks = XwaCraftHyperspace_ArrivalTicksLeft(objIdx);
	g_paiContext.aiController->maneuverTimer = (uint16_t)(ticks < 1 ? 1 : (ticks > 2596 ? 2596 : ticks));
}

int XwaCraftHyperspace_AiOutOfHyperspaceStep(unsigned int objIdx, char* reached) {
	if (!XwaCraftHyperspace_ArrivalActive(objIdx)) {
		return 0;
	}
	*reached = (char)XwaCraftHyperspace_ArrivalFinished(objIdx);
	if (*reached) {
		XwaCraftHyperspace_EndArrival(objIdx);
	}
	return 1;
}

/* ---- Departures ------------------------------------------------------------- */

/* Sets `*speed` and `*acceleration` for an AI craft whose departure has a
   set speed. A player's craft keeps the stock departure. */
static int Hyperspace_AiDeparture(unsigned int objIdx, int* speed, int* acceleration) {
	return objIdx < XWA_MAX_OBJECTS && g_objectTable[objIdx].mobj != NULL &&
		   g_objectTable[objIdx].playerOwnerIdx == -1 &&
		   XwaCraftHyperspace_DepartureSpeed(g_objectTable[objIdx].objectType, speed, acceleration);
}

int XwaCraftHyperspace_StepDeparture(unsigned int objIdx) {
	MobileObject* mobj;
	int speed, acceleration;
	uint64_t product, whole;
	uint16_t oldRemainder;
	uint32_t next;

	if (!Hyperspace_AiDeparture(objIdx, &speed, &acceleration)) {
		return 0;
	}
	/* The stock arithmetic (Flight_AccelerateHyperspaceSpeed), in 64 bits
	   for large accelerations. */
	mobj = g_objectTable[objIdx].mobj;
	product = (uint64_t)g_elapsedTicks * (uint64_t)acceleration;
	whole = product / 236u;
	oldRemainder = mobj->speedRemainder;
	mobj->speedRemainder = (uint16_t)(oldRemainder + (uint16_t)(((product - whole * 236u) << 16) / 236u));
	next = (uint32_t)mobj->speed + (mobj->speedRemainder < oldRemainder ? 1u : 0u);
	next = whole > (uint64_t)speed ? (uint32_t)speed + 1u : next + (uint32_t)whole;
	mobj->speed = (uint16_t)(next > (uint32_t)speed ? (uint32_t)speed : next);
	return 1;
}

int XwaCraftHyperspace_DepartureJumpSpeed(unsigned int objIdx) {
	int speed, acceleration;

	return Hyperspace_AiDeparture(objIdx, &speed, &acceleration) ? speed : HYPERSPACE_STOCK_DEPARTURE_SPEED;
}
