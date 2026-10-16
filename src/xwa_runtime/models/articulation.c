#include "xwa_runtime/models/articulation.h"

#include "aeron/asset/flight_model.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/util/memory.h"
#include "xwa/util/random.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/models/opt_synth.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_runtime/timing/modern_flight_timing.h"

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>
#include <string.h>

/* Modern port module; no original counterpart. It does what XWAU's
   hook_sfoils does for the craft whose GLB model lists articulations,
   on the stock CraftData
   state. Craft without them never reach it. */

/* CraftData.articulationMotion: the groups a key started; 0 moves both. */
enum { ARTICULATION_MOTION_BOTH = 0, ARTICULATION_MOTION_SFOILS = 1, ARTICULATION_MOTION_LANDING_GEAR = 2 };

/* Which lists a step or a query covers. */
enum { ARTICULATION_SFOILS = 1, ARTICULATION_LANDING_GEAR = 2 };

/* CraftData.articulationTicks slots. */
enum {
	ARTICULATION_CLOCK_SFOILS,
	ARTICULATION_CLOCK_HANGAR_DOORS,
	ARTICULATION_CLOCK_HATCHES,
	ARTICULATION_CLOCK_WARHEADS_BAY,
	ARTICULATION_CLOCK_CARGO_CLAMPS,
	ARTICULATION_CLOCK_SHUTTLE
};

/* The into-hyperspace maneuver (g_maneuverInitTable[21]). */
#define ARTICULATION_INTO_HYPERSPACE_MANEUVER 21
/* XWAU's step period for SFoilsAnimationSpeed 1, in ticks. */
#define ARTICULATION_STEP_TICKS 15
/* AI maneuvers that drive the cargo clamps (g_maneuverInitTable). */
#define ARTICULATION_BOARD_MANEUVER 18
#define ARTICULATION_RELEASE_MANEUVER 35
/* The hangar shuttle's S-foil speeds are units per this many shuttle traffic
   dt units (4 ticks): the original's 8-tick frames. */
#define ARTICULATION_SHUTTLE_CADENCE 2
/* Stock S-foil and crew speeds are units per these many ticks (the phase-1
   and crew timers of FlightObject_UpdateSpecialBehavior). */
#define ARTICULATION_STOCK_STEP_TICKS 14
#define ARTICULATION_CREW_TICKS 15
#define ARTICULATION_STOCK_SFOILS_MAX 32
#define ARTICULATION_HANGAR_DOORS_DISTANCE 0x4000
/* XWAU's AI out-of-hyperspace maneuver opens the S-foils from this timer. */
#define ARTICULATION_OUT_OF_HYPERSPACE_OPEN_TIMER 0x2ac

static CraftData* Articulation_Craft(unsigned int objIdx) {
	ObjectRecord* object;

	if (g_objectTable == NULL || objIdx >= g_objectTableSlotCount) {
		return NULL;
	}
	object = &g_objectTable[objIdx];
	return object->objectType != OBJ_None && object->mobj != NULL ? object->mobj->pCraft : NULL;
}

/* The articulations of an object: its model's, with the variant its flight
   group selects merged over them. */
typedef struct ArticulationModel {
	const XwaOptSynthData* synth;
	uint32_t articulationCount;
	const AeronFlightArticulation* articulations;
	uint32_t articulationSettingCount;
	const AeronFlightArticulationSetting* articulationSettings;
} ArticulationModel;

/* g_missionFlightGroups holds 192. */
#define ARTICULATION_FLIGHT_GROUPS 192
#define ARTICULATION_VARIANT_PREFIX "variant."
/* The longest variant name a group can follow: `variant.<name>.sfoils`. */
#define ARTICULATION_VARIANT_MAX (AERON_FLIGHT_TAG_MAX - 1 - 8 - 1 - 6)

/* The current mission's selections for a flight group (hook_sfoils'
   [<Section>_fg_N] sections, as a model variant, and [mission_tie] start
   lines). */
typedef struct ArticulationFlightGroup {
	int start; /* ARTICULATION_SFOILS and ARTICULATION_LANDING_GEAR, set to their angle at spawn */
	char variant[ARTICULATION_VARIANT_MAX + 1];
} ArticulationFlightGroup;

static ArticulationFlightGroup s_flightGroups[ARTICULATION_FLIGHT_GROUPS];

/* A model's articulations without its variant entries, or with variant
   `variant`'s merged over them. */
typedef struct ArticulationMerged {
	const AeronFlightArticulation* source;
	uint32_t sourceCount;
	char variant[ARTICULATION_VARIANT_MAX + 1];
	uint32_t count;
	AeronFlightArticulation* entries; /* NULL: the source as is */
} ArticulationMerged;

static ArticulationMerged* s_merged;
static uint32_t s_mergedCount;
static uint32_t s_mergedCapacity;

/* The group of variant entry `articulation` of variant `variant`, or NULL
   for an entry of another variant or of no variant. */
static const char* Articulation_VariantGroup(const AeronFlightArticulation* articulation, const char* variant) {
	const size_t prefix = strlen(ARTICULATION_VARIANT_PREFIX);
	const size_t length = strlen(variant);

	if (variant[0] == '\0' || strncmp(articulation->group, ARTICULATION_VARIANT_PREFIX, prefix) != 0 ||
		strncmp(articulation->group + prefix, variant, length) != 0 || articulation->group[prefix + length] != '.') {
		return NULL;
	}
	return articulation->group + prefix + length + 1;
}

/* hook_sfoils MergeFileLines: the variant's entries first, then the model's
   whose group and component the variant does not list. */
static void Articulation_BuildMerged(ArticulationMerged* merged, const XwaOptSynthData* synth,
									 const char* variant) {
	uint32_t index;
	uint32_t other;
	uint32_t variantCount = 0;
	int hasVariants = 0;

	merged->entries = NULL;
	merged->count = synth->articulationCount;
	for (index = 0; index < synth->articulationCount; ++index) {
		hasVariants |= strncmp(synth->articulations[index].group, ARTICULATION_VARIANT_PREFIX,
							   strlen(ARTICULATION_VARIANT_PREFIX)) == 0;
	}
	if (!hasVariants) {
		if (variant[0] != '\0') {
			Aeron_LogWarn("xwa.models", "replacement '%s' has no articulation variant '%s'", synth->key, variant);
		}
		return;
	}
	merged->entries = (AeronFlightArticulation*)malloc(synth->articulationCount * sizeof *merged->entries);
	if (merged->entries == NULL) {
		return;
	}
	merged->count = 0;
	for (index = 0; index < synth->articulationCount; ++index) {
		const char* group = Articulation_VariantGroup(&synth->articulations[index], variant);

		if (group != NULL) {
			merged->entries[merged->count] = synth->articulations[index];
			snprintf(merged->entries[merged->count].group, sizeof merged->entries[merged->count].group, "%s", group);
			++merged->count;
		}
	}
	variantCount = merged->count;
	if (variant[0] != '\0' && variantCount == 0) {
		Aeron_LogWarn("xwa.models", "replacement '%s' has no articulation variant '%s'", synth->key, variant);
	}
	for (index = 0; index < synth->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &synth->articulations[index];

		if (strncmp(articulation->group, ARTICULATION_VARIANT_PREFIX, strlen(ARTICULATION_VARIANT_PREFIX)) == 0) {
			continue;
		}
		for (other = 0; other < variantCount; ++other) {
			if (merged->entries[other].component == articulation->component &&
				strcmp(merged->entries[other].group, articulation->group) == 0) {
				break;
			}
		}
		if (other == variantCount) {
			merged->entries[merged->count++] = *articulation;
		}
	}
}

/* The model's articulations for variant `variant` ("" for none). Built once
   per model and variant, until the mission changes. */
static const AeronFlightArticulation* Articulation_Merged(const XwaOptSynthData* synth, const char* variant,
														  uint32_t* count) {
	ArticulationMerged* merged;
	uint32_t index;

	for (index = 0; index < s_mergedCount; ++index) {
		merged = &s_merged[index];
		if (merged->source == synth->articulations && merged->sourceCount == synth->articulationCount &&
			strcmp(merged->variant, variant) == 0) {
			break;
		}
	}
	if (index == s_mergedCount) {
		if (s_mergedCount == s_mergedCapacity) {
			const uint32_t capacity = s_mergedCapacity != 0 ? s_mergedCapacity * 2 : 32;
			ArticulationMerged* grown = (ArticulationMerged*)realloc(s_merged, capacity * sizeof *grown);

			if (grown == NULL) {
				*count = synth->articulationCount;
				return synth->articulations;
			}
			s_merged = grown;
			s_mergedCapacity = capacity;
		}
		merged = &s_merged[s_mergedCount++];
		merged->source = synth->articulations;
		merged->sourceCount = synth->articulationCount;
		snprintf(merged->variant, sizeof merged->variant, "%s", variant);
		Articulation_BuildMerged(merged, synth, variant);
	}
	merged = &s_merged[index];
	*count = merged->count;
	return merged->entries != NULL ? merged->entries : synth->articulations;
}

/* The object's articulations in `view`, or NULL when its model is not
   synthesized. The model data outlives the handle lock: it is freed with
   the model. */
static const ArticulationModel* Articulation_Model(unsigned int objIdx, ArticulationModel* view) {
	MemoryHandle handle;
	const OptimizedPolyObject* optModel;
	const XwaOptSynthData* synth;
	const uint16_t objectType = Articulation_Craft(objIdx) != NULL ? (uint16_t)g_objectTable[objIdx].objectType : 0;
	const char* variant;

	handle = objectType != 0 && objectType < OBJ_Count ? g_loadedModels.byObjectType[objectType] : 0;
	optModel = handle != 0 ? (const OptimizedPolyObject*)Memory_LockHandle(handle) : NULL;
	if (optModel == NULL) {
		return NULL;
	}
	synth = OptSynth_GetModelData(optModel);
	Memory_UnlockHandle(handle);
	if (synth == NULL) {
		return NULL;
	}
	view->synth = synth;
	view->articulationCount = synth->articulationCount;
	view->articulations = synth->articulations;
	view->articulationSettingCount = synth->articulationSettingCount;
	view->articulationSettings = synth->articulationSettings;
	if (synth->articulationCount != 0) {
		/* Hangar scene objects take a flight group past the mission's, which
		   selects nothing. */
		variant = g_objectTable[objIdx].flightGroupIdx < ARTICULATION_FLIGHT_GROUPS
					  ? s_flightGroups[g_objectTable[objIdx].flightGroupIdx].variant
					  : "";
		view->articulations = Articulation_Merged(synth, variant, &view->articulationCount);
	}
	return view;
}

/* Whether the object's loaded model is an original OPT, without replacement
   data; such a model has no articulations. */
static int Articulation_IsOriginalOpt(unsigned int objIdx) {
	MemoryHandle handle;
	const OptimizedPolyObject* optModel;
	int original;
	const uint16_t objectType =
		Articulation_Craft(objIdx) != NULL ? (uint16_t)g_objectTable[objIdx].objectType : 0;

	handle = objectType != 0 && objectType < OBJ_Count ? g_loadedModels.byObjectType[objectType] : 0;
	optModel = handle != 0 ? (const OptimizedPolyObject*)Memory_LockHandle(handle) : NULL;
	if (optModel == NULL) {
		return 0;
	}
	original = OptSynth_GetModelData(optModel) == NULL;
	Memory_UnlockHandle(handle);
	return original;
}

static int Articulation_InLists(const AeronFlightArticulation* articulation, int lists) {
	return ((lists & ARTICULATION_SFOILS) != 0 && strcmp(articulation->group, "sfoils") == 0) ||
		   ((lists & ARTICULATION_LANDING_GEAR) != 0 && strcmp(articulation->group, "landing_gear") == 0);
}

static int Articulation_Has(const ArticulationModel* model, int lists) {
	uint32_t index;

	for (index = 0; model != NULL && index < model->articulationCount; ++index) {
		if (Articulation_InLists(&model->articulations[index], lists)) {
			return 1;
		}
	}
	return 0;
}

static int32_t Articulation_Setting(const ArticulationModel* model, const char* name, int32_t fallback) {
	uint32_t index;

	for (index = 0; model != NULL && index < model->articulationSettingCount; ++index) {
		if (strcmp(model->articulationSettings[index].name, name) == 0) {
			return model->articulationSettings[index].value;
		}
	}
	return fallback;
}

/* hook_sfoils AreSFoilsOpened / AreLandingGearsClosed: every mesh of the
   list at 0. A missing landing-gear list counts as closed; a missing S-foil
   list does not count as open. */
static int Articulation_AtRest(const ArticulationModel* model, const CraftData* craft, int list) {
	uint32_t index;

	if (!Articulation_Has(model, list)) {
		return list == ARTICULATION_LANDING_GEAR;
	}
	for (index = 0; index < model->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];

		if (Articulation_InLists(articulation, list) && craft->meshRotation[articulation->component] != 0) {
			return 0;
		}
	}
	return 1;
}

/* Each articulated mesh moves by its fine angle, CraftData.articulationAngles,
   in 1/256 meshRotation units on every step; meshRotation follows it in whole
   units. Code that sets meshRotation itself restarts the fine angle there. A
   mesh with a negative angle moves on the signed side of 0. */
static int32_t Articulation_FineAngle(CraftData* craft, int component, int signedSide) {
	uint16_t* fine = &craft->articulationAngles[component];

	if ((uint8_t)(*fine >> 8) != craft->meshRotation[component]) {
		*fine = (uint16_t)(craft->meshRotation[component] << 8);
	}
	return signedSide ? (int32_t)(int16_t)*fine : (int32_t)*fine;
}

static void Articulation_SetAngle(CraftData* craft, int component, int angle) {
	craft->articulationAngles[component] = (uint16_t)(angle * 256);
	craft->meshRotation[component] = (uint8_t)angle;
}

/* Moves `component` toward `angle` (meshRotation units) by `stepUp` or
   `stepDown` 1/256 units. Returns whether it was away from it. */
static int Articulation_MoveToward(CraftData* craft, int component, int angle, int32_t stepUp,
								   int32_t stepDown, int signedSide) {
	const int32_t target = angle * 256;
	int32_t position = Articulation_FineAngle(craft, component, signedSide);

	if (position == target) {
		return 0;
	}
	if (position < target) {
		position = position + stepUp < target ? position + stepUp : target;
	} else {
		position = position - stepDown > target ? position - stepDown : target;
	}
	craft->articulationAngles[component] = (uint16_t)position;
	craft->meshRotation[component] = (uint8_t)(craft->articulationAngles[component] >> 8);
	return 1;
}

/* Sets the meshes of `lists` to their angle (the byte wraps, as XWAU's). */
static void Articulation_SetToAngle(const ArticulationModel* model, CraftData* craft, int lists) {
	uint32_t index;

	for (index = 0; index < model->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];

		if (Articulation_InLists(articulation, lists)) {
			Articulation_SetAngle(craft, articulation->component, articulation->angle);
		}
	}
}

static int Articulation_InGroup(const AeronFlightArticulation* articulation, const char* group) {
	return strcmp(articulation->group, group) == 0;
}

static int Articulation_HasGroup(const ArticulationModel* model, const char* group) {
	uint32_t index;

	for (index = 0; model != NULL && index < model->articulationCount; ++index) {
		if (Articulation_InGroup(&model->articulations[index], group)) {
			return 1;
		}
	}
	return 0;
}

/* One entry toward its angle by its speed, or back to 0 by its speed back,
   `unit` 1/256 units per unit of speed. Returns whether it was away. */
static int Articulation_StepEntry(CraftData* craft, const AeronFlightArticulation* articulation, int toAngle,
								  int32_t unit) {
	const int32_t step = (toAngle ? articulation->speed : articulation->speed_back) * unit;

	return Articulation_MoveToward(craft, articulation->component, toAngle ? articulation->angle : 0, step,
								   step, articulation->angle < 0);
}

/* hook_sfoils' common step over a group. */
static void Articulation_StepGroup(const ArticulationModel* model, CraftData* craft, const char* group,
								   int toAngle, int32_t unit) {
	uint32_t index;

	for (index = 0; index < model->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];

		if (Articulation_InGroup(articulation, group)) {
			Articulation_StepEntry(craft, articulation, toAngle, unit);
		}
	}
}

/* hook_sfoils SetCraftMeshRotationAngles: toward `angle` (each entry's own
   when `angle` is -1), from either side. */
static void Articulation_StepGroupToward(const ArticulationModel* model, CraftData* craft, const char* group,
										 int angle, int32_t unit) {
	uint32_t index;

	for (index = 0; index < model->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];

		if (Articulation_InGroup(articulation, group)) {
			Articulation_MoveToward(craft, articulation->component, angle == -1 ? articulation->angle : angle,
									articulation->speed * unit, articulation->speed_back * unit,
									articulation->angle < 0);
		}
	}
}

static void Articulation_SetGroup(const ArticulationModel* model, CraftData* craft, const char* group, int toAngle) {
	uint32_t index;

	for (index = 0; index < model->articulationCount; ++index) {
		if (Articulation_InGroup(&model->articulations[index], group)) {
			Articulation_SetAngle(craft, model->articulations[index].component,
								  toAngle ? model->articulations[index].angle : 0);
		}
	}
}

/* hook_sfoils GetHatches: no hatches in multiplayer. */
static int Articulation_HatchesActive(void) {
	return g_flightPlayerCount <= 1;
}

int XwaArticulation_HasSFoilsOrLandingGear(unsigned int objIdx) {
	ArticulationModel view;

	return Articulation_Has(Articulation_Model(objIdx, &view), ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR);
}

int XwaArticulation_InitLeavingHangar(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);

	/* hook_sfoils SFoilsHook1: the hatches open at the start of the hangar
	   scene, whatever else the model has. */
	if (craft != NULL && model != NULL && g_launchSeqPhase == 0 && Articulation_HatchesActive()) {
		Articulation_SetGroup(model, craft, "hatches", 1);
	}
	if (craft == NULL || !Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR)) {
		return 0;
	}
	Articulation_SetToAngle(model, craft, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR);
	craft->sFoilState = 2;
	return 1;
}

/* How far a unit of speed moves over `elapsed` time units, in 1/256 units: a
   speed is in units per `period`. XWAU steps once per period
   (ObjectIndexTime::RetrieveTimeSpeed); here the motion is spread over it.
   The remainder stays in CraftData.articulationTicks, one per `clock` as
   XWAU keeps one accumulator per feature. */
static int32_t Articulation_Advance(CraftData* craft, int clock, int elapsed, int32_t period) {
	uint32_t total;

	if (period < 1) {
		period = 1;
	}
	total = (uint32_t)craft->articulationTicks[clock] + (uint32_t)(elapsed > 0 ? elapsed : 0) * 256u;
	craft->articulationTicks[clock] = (uint16_t)(total % (uint32_t)period);
	return (int32_t)(total / (uint32_t)period);
}

/* Over this sim step, at the model's step_ticks period (15 x XWAU's
   SFoilsAnimationSpeed). */
static int32_t Articulation_StepUnit(const ArticulationModel* model, CraftData* craft, int clock) {
	return Articulation_Advance(craft, clock, g_elapsedTicks,
								Articulation_Setting(model, "step_ticks", ARTICULATION_STEP_TICKS));
}

/* Original OPTs: the stock S-foils of FlightObject_UpdateSpecialBehavior
   (phase 1) move the same way in the high-rate mode, where phase 1 leaves
   them to this; the compatibility mode keeps phase 1. Its per-type numbers,
   a speed being units per ARTICULATION_STOCK_STEP_TICKS: */
typedef struct ArticulationStockSFoil {
	int component;
	int angle;
	int speed;
	int counted; /* its motion holds the completion (not the B-wing's bridge) */
} ArticulationStockSFoil;

static int Articulation_StockSFoils(uint16_t objectType, ArticulationStockSFoil* entries, int capacity) {
	int count = 0;
	int meshCount;
	int mesh;

	if (objectType != OBJ_XWing && objectType != OBJ_BWing && objectType != OBJ_MissileBoat &&
		objectType != OBJ_SkiprayBlastBoat && objectType != OBJ_Shuttle) {
		return 0;
	}
	meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
	for (mesh = 0; mesh < meshCount && count < capacity; ++mesh) {
		const MeshType meshType = ModelMesh_GetObjectTypeMeshType(objectType, mesh);
		ArticulationStockSFoil* entry = &entries[count];

		entry->component = mesh;
		entry->counted = 1;
		entry->speed = 2;
		if (meshType == MESH_RotaryWing) {
			if (objectType == OBJ_XWing) {
				entry->angle = ModelMesh_GetCenterZ(OBJ_XWing, mesh) < 0 ? 12 : 8;
				entry->speed = 1;
			} else if (objectType == OBJ_MissileBoat) {
				entry->angle = 0x42;
			} else if (objectType == OBJ_Shuttle) {
				entry->angle = 0x60;
			} else {
				entry->angle = 0x40;
			}
		} else if (meshType == MESH_Bridge && objectType == OBJ_BWing) {
			entry->angle = 0x40;
			entry->counted = 0;
		} else if (meshType == MESH_Launcher && objectType == OBJ_MissileBoat) {
			entry->angle = 0x20;
			entry->speed = 1;
		} else {
			continue;
		}
		++count;
	}
	return count;
}

static int Articulation_StockSFoilsActive(unsigned int objIdx) {
	ArticulationStockSFoil entries[ARTICULATION_STOCK_SFOILS_MAX];

	return XwaModernFlightTiming_IsHighRate() && Articulation_IsOriginalOpt(objIdx) &&
		   Articulation_StockSFoils((uint16_t)g_objectTable[objIdx].objectType, entries,
									ARTICULATION_STOCK_SFOILS_MAX) != 0;
}

static void Articulation_StepStockSFoils(unsigned int objIdx, CraftData* craft) {
	ArticulationStockSFoil entries[ARTICULATION_STOCK_SFOILS_MAX];
	const ObjectRecord* object = &g_objectTable[objIdx];
	int32_t unit;
	int count;
	int moved = 0;
	int index;

	count = Articulation_StockSFoils((uint16_t)object->objectType, entries, ARTICULATION_STOCK_SFOILS_MAX);
	if (count == 0) {
		return;
	}
	unit =
		Articulation_Advance(craft, ARTICULATION_CLOCK_SFOILS, g_elapsedTicks, ARTICULATION_STOCK_STEP_TICKS);
	if ((craft->sFoilState & 1) == 0) {
		return;
	}
	for (index = 0; index < count; ++index) {
		const int32_t step = entries[index].speed * unit;

		if (Articulation_MoveToward(craft, entries[index].component,
									(craft->sFoilState & 2) != 0 ? entries[index].angle : 0, step, step, 0) &&
			entries[index].counted) {
			moved = 1;
		}
	}
	/* Phase 1's completion. */
	if (!moved) {
		if ((craft->sFoilState & 2) != 0) {
			craft->sFoilState = 2;
			msg_emitInFlightMessage(MSG_SFOILS_CLOSED, object->playerOwnerIdx);
		} else {
			craft->sFoilState = 0;
			msg_emitInFlightMessage(MSG_SFOILS_OPEN, object->playerOwnerIdx);
		}
	}
}

int XwaArticulation_StepsSFoils(unsigned int objIdx) {
	return XwaArticulation_HasSFoilsOrLandingGear(objIdx) || Articulation_StockSFoilsActive(objIdx);
}

void XwaArticulation_StepSFoils(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	const ObjectRecord* object;
	int lists;
	int32_t unit;
	int moved;
	uint32_t index;

	if (craft == NULL) {
		return;
	}
	if (!Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR)) {
		if (XwaModernFlightTiming_IsHighRate() && Articulation_IsOriginalOpt(objIdx)) {
			Articulation_StepStockSFoils(objIdx, craft);
		}
		return;
	}
	object = &g_objectTable[objIdx];

	/* hook_sfoils SFoilsHook2: a key moves its own group; a player's closing
	   (hyperspace) and the AI's into-hyperspace maneuver move the S-foils;
	   anything else moves both. XWAU keeps the key flags for the local
	   player only; here they are the craft's. */
	if (craft->articulationMotion == ARTICULATION_MOTION_SFOILS) {
		lists = ARTICULATION_SFOILS;
	} else if (craft->articulationMotion == ARTICULATION_MOTION_LANDING_GEAR) {
		lists = ARTICULATION_LANDING_GEAR;
	} else if (craft->sFoilState == 3 && (craft->aiController.maneuverMode == ARTICULATION_INTO_HYPERSPACE_MANEUVER ||
										  object->playerOwnerIdx != -1)) {
		lists = ARTICULATION_SFOILS;
	} else {
		lists = ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR;
	}
	if (!Articulation_Has(model, lists)) {
		craft->sFoilState = 0;
		return;
	}

	unit = Articulation_StepUnit(model, craft, ARTICULATION_CLOCK_SFOILS);
	moved = 0;
	for (index = 0; index < model->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];

		if (Articulation_InLists(articulation, lists) && (craft->sFoilState & 1) != 0 &&
			Articulation_StepEntry(craft, articulation, (craft->sFoilState & 2) != 0, unit)) {
			moved = 1;
		}
	}

	/* The original completion (FlightObject_UpdateSpecialBehavior, phase 1),
	   with the landing-gear texts after Ctrl+L (hook_sfoils SetSFoilsText,
	   before the key motion ends). */
	if ((craft->sFoilState & 1) != 0 && !moved) {
		const uint16_t messageId = XwaArticulation_Message(objIdx, (craft->sFoilState & 2) != 0 ? MSG_SFOILS_CLOSED
																								 : MSG_SFOILS_OPEN);

		if (object->playerOwnerIdx != -1) {
			craft->articulationMotion = ARTICULATION_MOTION_BOTH;
		}
		craft->sFoilState = (craft->sFoilState & 2) != 0 ? 2 : 0;
		msg_emitInFlightMessage(messageId, object->playerOwnerIdx);
	}
}

static void Articulation_SetMessage(uint16_t* message, uint16_t value) {
	if (message != NULL) {
		*message = value;
	}
}

XwaArticulationTriggerResult XwaArticulation_Trigger(unsigned int objIdx, XwaArticulationTrigger trigger,
													  uint16_t* message) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	int hasSFoils;
	int hasLandingGear;
	int sfoilsOpen;
	int landingGearClosed;

	if (craft == NULL || !Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR)) {
		return XWA_ARTICULATION_STOCK;
	}
	hasSFoils = Articulation_Has(model, ARTICULATION_SFOILS);
	hasLandingGear = Articulation_Has(model, ARTICULATION_LANDING_GEAR);
	sfoilsOpen = Articulation_AtRest(model, craft, ARTICULATION_SFOILS);
	landingGearClosed = Articulation_AtRest(model, craft, ARTICULATION_LANDING_GEAR);

	/* hook_sfoils SFoilsFilterHook. */
	switch (trigger) {
		case XWA_ARTICULATION_KEY_SFOILS:
			if (craft->articulationMotion == ARTICULATION_MOTION_LANDING_GEAR) {
				Articulation_SetMessage(message, MSG_SFOILS_REFUSED_GEAR_MOVING);
				return XWA_ARTICULATION_REFUSED;
			}
			if (hasSFoils && !sfoilsOpen && !landingGearClosed) {
				Articulation_SetMessage(message, MSG_SFOILS_REFUSED_GEAR_DOWN);
				return XWA_ARTICULATION_REFUSED;
			}
			if (!hasSFoils) {
				Articulation_SetMessage(message, MSG_NOT_EQUIPPED_SFOIL);
				return XWA_ARTICULATION_NOT_EQUIPPED;
			}
			craft->articulationMotion = ARTICULATION_MOTION_SFOILS;
			craft->sFoilState = sfoilsOpen ? 0 : 2;
			return XWA_ARTICULATION_PASS;

		case XWA_ARTICULATION_KEY_LANDING_GEAR:
			if (craft->articulationMotion == ARTICULATION_MOTION_SFOILS) {
				Articulation_SetMessage(message, MSG_LANDING_GEAR_REFUSED_SFOILS_MOVING);
				return XWA_ARTICULATION_REFUSED;
			}
			if (hasLandingGear && sfoilsOpen && landingGearClosed) {
				Articulation_SetMessage(message, MSG_LANDING_GEAR_REFUSED_SFOILS_OPEN);
				return XWA_ARTICULATION_REFUSED;
			}
			if (!hasLandingGear) {
				Articulation_SetMessage(message, MSG_NOT_EQUIPPED_LANDING_GEAR);
				return XWA_ARTICULATION_NOT_EQUIPPED;
			}
			craft->articulationMotion = ARTICULATION_MOTION_LANDING_GEAR;
			craft->sFoilState = landingGearClosed ? 0 : 2;
			return XWA_ARTICULATION_PASS;

		default:
			break;
	}

	/* The automatic triggers. A model with landing gear only, gear up, has
	   nothing to move. */
	if (!hasSFoils && hasLandingGear && landingGearClosed) {
		return XWA_ARTICULATION_SKIP;
	}
	if (trigger == XWA_ARTICULATION_HYPERSPACE_DEPARTURE) {
		return Articulation_Setting(model, "sfoils.close_in_hyperspace", 0) != 0 ? XWA_ARTICULATION_PASS
																				  : XWA_ARTICULATION_SKIP;
	}
	if (trigger == XWA_ARTICULATION_HYPERSPACE_ARRIVAL) {
		if (craft->sFoilState == 0) {
			return XWA_ARTICULATION_SKIP;
		}
		if (hasSFoils) {
			craft->sFoilState = 2;
		}
	}
	return XWA_ARTICULATION_PASS;
}

uint16_t XwaArticulation_Message(unsigned int objIdx, uint16_t messageId) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	const CraftData* craft = Articulation_Craft(objIdx);

	if (craft == NULL || !Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR)) {
		return messageId;
	}
	/* hook_sfoils NoFireMessageHook's gate has its own text. */
	if (messageId == MSG_SFOILS_NO_FIRE) {
		return MSG_ARTICULATION_NO_FIRE;
	}
	if (craft->articulationMotion != ARTICULATION_MOTION_LANDING_GEAR) {
		return messageId;
	}
	/* hook_sfoils SetSFoilsText: closing lowers the gear. */
	switch (messageId) {
		case MSG_SFOILS_OPENING:
			return MSG_LANDING_GEAR_RAISING;
		case MSG_SFOILS_CLOSING:
			return MSG_LANDING_GEAR_LOWERING;
		case MSG_SFOILS_OPEN:
			return MSG_LANDING_GEAR_UP;
		case MSG_SFOILS_CLOSED:
			return MSG_LANDING_GEAR_DOWN;
		default:
			return messageId;
	}
}


int XwaArticulation_TriggerPasses(unsigned int objIdx, XwaArticulationTrigger trigger, int stockTest) {
	const XwaArticulationTriggerResult result = XwaArticulation_Trigger(objIdx, trigger, NULL);

	return result == XWA_ARTICULATION_STOCK ? stockTest : result == XWA_ARTICULATION_PASS;
}

int XwaArticulation_FireBlocked(unsigned int objIdx, int stockBlocked) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	const CraftData* craft = Articulation_Craft(objIdx);
	int hasSFoils;
	int blocked;

	if (craft == NULL || !Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR)) {
		return stockBlocked;
	}
	/* hook_sfoils NoFireMessageHook. */
	hasSFoils = Articulation_Has(model, ARTICULATION_SFOILS);
	blocked = 0;
	if (hasSFoils && !Articulation_AtRest(model, craft, ARTICULATION_SFOILS)) {
		blocked = Articulation_Setting(model, "sfoils.allow_fire_closed", 0) == 0;
	}
	if (Articulation_Has(model, ARTICULATION_LANDING_GEAR) &&
		!Articulation_AtRest(model, craft, ARTICULATION_LANDING_GEAR)) {
		blocked = 1;
	}
	return (hasSFoils && (craft->sFoilState & 1) != 0) || blocked;
}

void XwaArticulation_AiOutOfHyperspaceInit(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	uint32_t index;

	/* hook_sfoils SFoilsAIOutOfHyperspace1Hook. XWAU clears every mesh
	   rotation; only the articulated ones are cleared here, so turret aim
	   survives. */
	if (craft == NULL || !Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR) ||
		Articulation_Setting(model, "sfoils.close_in_hyperspace", 0) == 0) {
		return;
	}
	for (index = 0; index < model->articulationCount; ++index) {
		craft->meshRotation[model->articulations[index].component] = 0;
	}
	if (!Articulation_Has(model, ARTICULATION_SFOILS)) {
		craft->sFoilState = 0;
		return;
	}
	Articulation_SetToAngle(model, craft, ARTICULATION_SFOILS);
	craft->sFoilState = 2;
}

void XwaArticulation_AiOutOfHyperspaceStep(unsigned int objIdx, int maneuverTimer) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);

	/* hook_sfoils SFoilsAIOutOfHyperspace2Hook. */
	if (craft == NULL || !Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR) ||
		Articulation_Setting(model, "sfoils.close_in_hyperspace", 0) == 0) {
		return;
	}
	if (maneuverTimer != 0 && maneuverTimer <= ARTICULATION_OUT_OF_HYPERSPACE_OPEN_TIMER && craft->sFoilState != 0) {
		craft->sFoilState = 1;
	}
}

void XwaArticulation_AiIntoHyperspace(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);

	/* hook_sfoils SFoilsAIIntoHyperspace1Hook and SFoilsAIHyperspaceOrderHook. */
	if (craft != NULL && Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR) &&
		Articulation_Setting(model, "sfoils.close_in_hyperspace", 0) != 0) {
		craft->sFoilState = 3;
	}
}

void XwaArticulation_AiParkOrder(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);

	/* hook_sfoils AILookForParkOrderHook. */
	if (craft != NULL && Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR) &&
		Articulation_Setting(model, "sfoils.park_closed", 0) != 0) {
		craft->sFoilState = 3;
	}
}

void XwaArticulation_AiParkStart(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);

	/* hook_sfoils AIParkManrHook. */
	if (craft != NULL && Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR) &&
		Articulation_Setting(model, "sfoils.park_closed", 0) != 0) {
		Articulation_SetToAngle(model, craft, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR);
		craft->sFoilState = 2;
	}
}

void XwaArticulation_AiParkEnd(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);

	/* hook_sfoils AIParkManrFunctionHook. */
	if (craft != NULL && Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR) &&
		Articulation_Setting(model, "sfoils.park_closed", 0) != 0) {
		craft->sFoilState = 1;
	}
}

void XwaArticulation_AiEnterHangarOrder(unsigned int objIdx) {
	uint32_t follower;

	/* hook_sfoils AIEnterHangarOrderHook: the leader and every craft that
	   follows it close their S-foils (and lower their landing gear). XWAU
	   does it for any craft; here for those with S-foils or landing gear. */
	if (XwaArticulation_HasSFoilsOrLandingGear(objIdx)) {
		Articulation_Craft(objIdx)->sFoilState = 3;
	}
	for (follower = g_activeRegionObjectSlotStart; follower < g_activeRegionCraftObjectSlotEnd; ++follower) {
		CraftData* craft = Articulation_Craft(follower);

		if (craft != NULL && craft->leader_obj_idx == (int)objIdx && XwaArticulation_HasSFoilsOrLandingGear(follower)) {
			craft->sFoilState = 3;
		}
	}
}

/* hook_pilot PilotHook, behaviors 0 (swing), 1 (random) and 2
   (random_pause), on the signed byte as the hook's `char&`. */
static void Articulation_StepCrewEntry(uint8_t* byte, const AeronFlightArticulation* articulation) {
	const int angle = articulation->angle;
	const int speed = articulation->speed;
	const int random = strcmp(articulation->behavior, "random") == 0;
	const int pause = strcmp(articulation->behavior, "random_pause") == 0;
	int value = (int8_t)*byte;

	if (pause) {
		if ((value & 1) != 0) {
			value = (int8_t)(value - speed);
			if (value < -angle + 1) {
				value = (int8_t)(-angle + 1);
			}
			if (value == -angle / 2 + 1 && GameRand() > 0x300) {
				value = (int8_t)(value + speed);
			}
			if (value == -angle + 1 || GameRand() < 0x300) {
				value ^= 1;
			}
		} else {
			value = (int8_t)(value + speed);
			if (value > angle) {
				value = (int8_t)angle;
			}
			if (value == angle / 2 && GameRand() > 0x300) {
				value = (int8_t)(value - speed);
			}
			if (value == angle || GameRand() < 0x300) {
				value ^= 1;
			}
		}
	} else if ((value & 1) != 0) {
		value = (int8_t)(value + speed);
		if (value > angle + 1) {
			value = (int8_t)(angle + 1);
		}
		if (value == angle + 1 || (random && GameRand() < 0x300)) {
			value ^= 1;
		}
	} else {
		value = (int8_t)(value - speed);
		if (value < -angle) {
			value = (int8_t)-angle;
		}
		if (value == -angle || (random && GameRand() < 0x300)) {
			value ^= 1;
		}
	}
	*byte = (uint8_t)value;
}

static int Articulation_IsCrew(const AeronFlightArticulation* articulation) {
	return strcmp(articulation->group, "crew") == 0 &&
		   (articulation->behavior[0] == '\0' || strcmp(articulation->behavior, "swing") == 0 ||
			strcmp(articulation->behavior, "random") == 0 || strcmp(articulation->behavior, "random_pause") == 0);
}

int XwaArticulation_AnimateCrew(unsigned int objIdx, int resetToNeutral) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	int pass;
	int component;
	uint32_t index;

	if (model == NULL) {
		return 0;
	}
	if (craft == NULL) {
		return 1;
	}
	/* Pass 0 steps the `random` (side) entries, pass 1 the others; the stock
	   function steps side, forward, then the limbs, whatever their numbers.
	   Within a pass, entries step by component, then by index: GameRand
	   makes the order matter. Only the components that some entry uses are
	   visited, in ascending order; the others step nothing. */
	for (pass = 0; pass < 2; ++pass) {
		component = -1;
		for (;;) {
			int next = XWA_CRAFT_MESH_SLOT_COUNT;

			for (index = 0; index < model->articulationCount; ++index) {
				const int used = model->articulations[index].component;

				if (used > component && used < next) {
					next = used;
				}
			}
			if (next == XWA_CRAFT_MESH_SLOT_COUNT) {
				break;
			}
			component = next;
			for (index = 0; index < model->articulationCount; ++index) {
				const AeronFlightArticulation* articulation = &model->articulations[index];

				if (articulation->component != component || !Articulation_IsCrew(articulation) ||
					(strcmp(articulation->behavior, "random") == 0) != (pass == 0)) {
					continue;
				}
				if (resetToNeutral) {
					craft->meshRotation[component] = 0;
				} else {
					Articulation_StepCrewEntry(&craft->meshRotation[component], articulation);
				}
			}
		}
	}
	return 1;
}

int XwaArticulation_CrewUpdatesAll(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	uint32_t index;

	if (model == NULL || Articulation_Setting(model, "crew.update_all", 0) == 0) {
		return 0;
	}
	for (index = 0; index < model->articulationCount; ++index) {
		if (Articulation_IsCrew(&model->articulations[index])) {
			return 1;
		}
	}
	return 0;
}

void XwaArticulation_StepHangarCrew(void) {
	uint32_t objIdx;

	for (objIdx = 0; g_objectTable != NULL && objIdx < g_objectTableSlotCount; ++objIdx) {
		if (XwaArticulation_CrewUpdatesAll(objIdx)) {
			FlightObject_AnimateCrewMeshRotations((int)objIdx, 0);
		}
	}
}

/* The crew meshes the crew animation moves, with their speed in units per
   ARTICULATION_CREW_TICKS: a replacement's crew articulations, or for an
   original OPT the stock ordinals of FlightObject_AnimateCrewMeshRotations
   (speed 2). */
typedef struct ArticulationCrewMesh {
	int component;
	int speed;
} ArticulationCrewMesh;

#define ARTICULATION_CREW_MAX 16

static int Articulation_CrewMeshes(unsigned int objIdx, const ArticulationModel* model,
								   ArticulationCrewMesh* meshes) {
	static const int8_t stockMeshes[][5] = {
		{ 6, 9, -1, -1, -1 },   /* OBJ_XWing */
		{ 4, -1, -1, -1, -1 },  /* OBJ_AWing */
		{ 8, 13, -1, -1, -1 },  /* OBJ_YWing */
		{ 19, 17, -1, -1, -1 }, /* OBJ_CorellianTransport2 */
		{ 13, 9, -1, -1, -1 },  /* OBJ_FamilyTransport */
		{ 21, 19, -1, -1, -1 }, /* OBJ_MilleniumFalcon2 */
		{ 1, 2, 3, 4, 5 },      /* OBJ_RebelPilot, OBJ_ImperialPilot */
	};
	int count = 0;
	int row;
	int index;

	if (model != NULL) {
		uint32_t entry;

		for (entry = 0; entry < model->articulationCount && count < ARTICULATION_CREW_MAX; ++entry) {
			if (Articulation_IsCrew(&model->articulations[entry])) {
				meshes[count].component = model->articulations[entry].component;
				meshes[count].speed = model->articulations[entry].speed;
				++count;
			}
		}
		return count;
	}
	switch (g_objectTable[objIdx].objectType) {
		case OBJ_XWing:
			row = 0;
			break;
		case OBJ_AWing:
			row = 1;
			break;
		case OBJ_YWing:
			row = 2;
			break;
		case OBJ_CorellianTransport2:
			row = 3;
			break;
		case OBJ_FamilyTransport:
			row = 4;
			break;
		case OBJ_MilleniumFalcon2:
			row = 5;
			break;
		case OBJ_RebelPilot:
		case OBJ_ImperialPilot:
			row = 6;
			break;
		default:
			return 0;
	}
	if (!Articulation_IsOriginalOpt(objIdx)) {
		return 0;
	}
	for (index = 0; index < 5 && stockMeshes[row][index] >= 0; ++index) {
		meshes[count].component = stockMeshes[row][index];
		meshes[count].speed = 2;
		++count;
	}
	return count;
}

/* The crew logic keeps its stock and XWAU steps, with the direction in the
   angle's low bit and its random choices on the crew timer; the crew meshes'
   fine angles follow those signed angles at their speed on every step, and
   only the drawing reads them. */
void XwaArticulation_StepCrewAngles(unsigned int objIdx) {
	ArticulationModel view;
	ArticulationCrewMesh meshes[ARTICULATION_CREW_MAX];
	CraftData* craft = Articulation_Craft(objIdx);
	int count;
	int index;

	if (craft == NULL) {
		return;
	}
	count = Articulation_CrewMeshes(objIdx, Articulation_Model(objIdx, &view), meshes);
	for (index = 0; index < count; ++index) {
		uint16_t* fine = &craft->articulationAngles[meshes[index].component];
		const int32_t target = (int8_t)craft->meshRotation[meshes[index].component] * 256;
		const int32_t step =
			(meshes[index].speed * 256 * (int32_t)g_elapsedTicks + ARTICULATION_CREW_TICKS - 1) /
			ARTICULATION_CREW_TICKS;
		int32_t position = (int16_t)*fine;

		if (position < target) {
			position = position + step < target ? position + step : target;
		} else {
			position = position - step > target ? position - step : target;
		}
		*fine = (uint16_t)position;
	}
}

void XwaArticulation_StepHangarCrewAngles(void) {
	uint32_t objIdx;

	for (objIdx = 0; g_objectTable != NULL && objIdx < g_objectTableSlotCount; ++objIdx) {
		XwaArticulation_StepCrewAngles(objIdx);
	}
}

void XwaArticulation_CloseHatches(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);

	/* hook_sfoils SFoilsFilterHook at the hangar exit. */
	if (craft != NULL && model != NULL && Articulation_HatchesActive()) {
		Articulation_SetGroup(model, craft, "hatches", 0);
	}
}

void XwaArticulation_StepLaunchHatches(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	int32_t unit;

	/* hook_sfoils PlayerCraftHatchHook: phases 1-4 close the hatches (and
	   raise the landing gear from phase 2), 7-8 open them. */
	if (craft == NULL || !Articulation_HasGroup(model, "hatches") || !Articulation_HatchesActive()) {
		return;
	}
	unit = Articulation_StepUnit(model, craft, ARTICULATION_CLOCK_HATCHES);
	if (g_launchSeqPhase >= 1 && g_launchSeqPhase <= 4) {
		Articulation_StepGroup(model, craft, "hatches", 0, unit);
		if (g_launchSeqPhase >= 2) {
			Articulation_StepGroup(model, craft, "landing_gear", 0, unit);
		}
	} else if (g_launchSeqPhase == 7 || g_launchSeqPhase == 8) {
		Articulation_StepGroup(model, craft, "hatches", 1, unit);
	}
}

void XwaArticulation_StepShuttleHatches(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	int32_t unit;

	/* hook_sfoils ShuttleHatchHook: open while parked (0), closing while
	   leaving (1-7), opening while landing (9). */
	if (craft == NULL || !Articulation_HasGroup(model, "hatches") || !Articulation_HatchesActive()) {
		return;
	}
	unit = Articulation_StepUnit(model, craft, ARTICULATION_CLOCK_HATCHES);
	if (g_hangarShuttleState.moveState == 0) {
		Articulation_SetGroup(model, craft, "hatches", 1);
	} else if (g_hangarShuttleState.moveState >= 1 && g_hangarShuttleState.moveState <= 7) {
		Articulation_StepGroup(model, craft, "hatches", 0, unit);
	} else if (g_hangarShuttleState.moveState == 9) {
		Articulation_StepGroup(model, craft, "hatches", 1, unit);
	}
}

int XwaArticulation_StepShuttleBlock(unsigned int objIdx, XwaArticulationShuttleStep step, int dtTicks) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	int32_t unit;
	uint32_t index;

	if (craft == NULL || model == NULL) {
		return 0;
	}
	/* XWAU removes the original hatch and custom-mesh motion; the hatch group
	   takes it over (XwaArticulation_StepShuttleHatches). */
	if (step == XWA_ARTICULATION_SHUTTLE_HATCHES) {
		return Articulation_HasGroup(model, "hatches");
	}
	if (!Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR)) {
		return 0;
	}
	/* hook_sfoils SFoilsHangarShuttleHook moves (speed * dt + 1) / 2 per call:
	   at the original's frames (dt 2), a speed is units per frame. */
	unit = Articulation_Advance(craft, ARTICULATION_CLOCK_SHUTTLE, dtTicks, ARTICULATION_SHUTTLE_CADENCE);
	for (index = 0; index < model->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];

		if (Articulation_InLists(articulation, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR)) {
			Articulation_StepEntry(craft, articulation, step == XWA_ARTICULATION_SHUTTLE_CLOSE, unit);
		}
	}
	return 1;
}

/* hook_sfoils HandleWarheadsBay: the local player's bay opens while warheads
   are armed. */
static void Articulation_StepWarheadsBay(void) {
	const int objIdx = g_players[g_localPlayer].objectIndex;
	ArticulationModel view;
	const ArticulationModel* model;
	CraftData* craft;

	if (objIdx == 0xffff || (craft = Articulation_Craft((unsigned int)objIdx)) == NULL) {
		return;
	}
	model = Articulation_Model((unsigned int)objIdx, &view);
	if (!Articulation_HasGroup(model, "warheads_bay")) {
		return;
	}
	Articulation_StepGroup(model, craft, "warheads_bay", g_players[g_localPlayer].selectedWeaponMode == 1,
						   Articulation_StepUnit(model, craft, ARTICULATION_CLOCK_WARHEADS_BAY));
}

/* The cargo-clamp group for carrying object `carriedObjIdx`:
   `cargo_clamps.` and its model's encoded name when the model lists it, else
   `cargo_clamps.close` (hook_sfoils GetCargoClampsList). */
static void Articulation_CarriedGroup(const ArticulationModel* model, unsigned int carriedObjIdx, char* group,
									  size_t size) {
	ArticulationModel carriedView;
	const ArticulationModel* carried = Articulation_Model(carriedObjIdx, &carriedView);
	const char* name = carried != NULL ? carried->synth->key : NULL;
	size_t length;
	const unsigned char* cursor;

	if (name == NULL || name[0] == '\0') {
		name = Articulation_Craft(carriedObjIdx) != NULL
				   ? XwaSnapshotExport_ModelName((int)g_objectTable[carriedObjIdx].objectType)
				   : NULL;
	}
	snprintf(group, size, "cargo_clamps.");
	length = strlen(group);
	for (cursor = (const unsigned char*)(name != NULL ? name : ""); *cursor != '\0' && length + 4 < size;
		 ++cursor) {
		const unsigned char c = *cursor >= 'A' && *cursor <= 'Z' ? (unsigned char)(*cursor - 'A' + 'a') : *cursor;

		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
			group[length++] = (char)c;
		} else {
			snprintf(group + length, 4, ".%02x", c);
			length += 3;
		}
	}
	group[length] = '\0';
	if (name == NULL || !Articulation_HasGroup(model, group)) {
		snprintf(group, size, "cargo_clamps.close");
	}
}

/* hook_sfoils HandleCargoClamps. */
static void Articulation_StepCargoClamps(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	char group[AERON_FLIGHT_TAG_MAX];
	int angle = -1;

	if (craft == NULL || model == NULL) {
		return;
	}
	if (craft->aiController.maneuverMode == ARTICULATION_BOARD_MANEUVER &&
		craft->aiController.targetObjIdx != 0xffffu && craft->carriedObjectIndex == 0xffffu) {
		snprintf(group, sizeof group, "cargo_clamps.open");
	} else if (craft->aiController.maneuverMode == ARTICULATION_BOARD_MANEUVER &&
			   craft->carriedObjectIndex != 0xffffu) {
		Articulation_CarriedGroup(model, craft->carriedObjectIndex, group, sizeof group);
	} else if (craft->aiController.maneuverMode == ARTICULATION_RELEASE_MANEUVER) {
		snprintf(group, sizeof group, "cargo_clamps.open");
		if (craft->carriedObjectIndex == 0xffffu) {
			angle = 0;
		}
	} else {
		return;
	}
	if (Articulation_HasGroup(model, group)) {
		Articulation_StepGroupToward(model, craft, group, angle,
									 Articulation_StepUnit(model, craft, ARTICULATION_CLOCK_CARGO_CLAMPS));
	}
}

/* hook_sfoils Distance: the octagonal distance. */
static int Articulation_Distance(const ObjectRecord* a, const ObjectRecord* b) {
	const int x = abs(a->world_x - b->world_x);
	const int y = abs(a->world_y - b->world_y);
	const int z = abs(a->world_z - b->world_z);

	if (x > y && x > z) {
		return x + y / 4 + z / 4;
	}
	if (y > x && y > z) {
		return x / 4 + y + z / 4;
	}
	return x / 4 + y / 4 + z;
}

/* hook_sfoils HangarDoorsHook: a starship, freighter or platform opens its
   hangar doors while a craft of its team is within the distance and no other
   team's is. */
static void Articulation_StepHangarDoors(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model;
	CraftData* craft = Articulation_Craft(objIdx);
	const ObjectRecord* object;
	int sameTeam = 0x7fffffff;
	int otherTeams = 0x7fffffff;
	int32_t doorsDistance;
	uint32_t other;

	if (craft == NULL) {
		return;
	}
	object = &g_objectTable[objIdx];
	if (object->genusId != GENUS_Starship && object->genusId != GENUS_Platform && object->genusId != GENUS_Freighter) {
		return;
	}
	model = Articulation_Model(objIdx, &view);
	if (!Articulation_HasGroup(model, "hangar_doors")) {
		return;
	}
	for (other = g_activeRegionObjectSlotStart; other < g_projectileObjectSlotStart; ++other) {
		const ObjectRecord* craftObject = &g_objectTable[other];
		int distance;

		if (other == objIdx || Articulation_Craft(other) == NULL || craftObject->regionIdx != object->regionIdx ||
			(craftObject->genusId != GENUS_Fighter && craftObject->genusId != GENUS_Utility &&
			 craftObject->genusId != GENUS_Transport && craftObject->genusId != GENUS_PilotDroid)) {
			continue;
		}
		distance = Articulation_Distance(object, craftObject);
		if (craftObject->mobj->team == object->mobj->team) {
			sameTeam = distance < sameTeam ? distance : sameTeam;
		} else {
			otherTeams = distance < otherTeams ? distance : otherTeams;
		}
	}
	doorsDistance = Articulation_Setting(model, "hangar_doors.distance", ARTICULATION_HANGAR_DOORS_DISTANCE);
	Articulation_StepGroup(model, craft, "hangar_doors",
						   sameTeam < doorsDistance && otherTeams >= doorsDistance,
						   Articulation_StepUnit(model, craft, ARTICULATION_CLOCK_HANGAR_DOORS));
}

void XwaArticulation_StepWorld(void) {
	uint32_t objIdx;

	if (g_objectTable == NULL) {
		return;
	}
	Articulation_StepWarheadsBay();
	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_projectileObjectSlotStart; ++objIdx) {
		Articulation_StepCargoClamps(objIdx);
	}
	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_projectileObjectSlotStart; ++objIdx) {
		Articulation_StepHangarDoors(objIdx);
	}
}

int XwaArticulation_AlignsBridge(const ObjectRecord* object) {
	ArticulationModel view;
	const ArticulationModel* model;

	if (object == NULL) {
		return 0;
	}
	if (object->objectType == OBJ_BWing) {
		return 1;
	}
	/* Preview and scene objects may live outside the object table. */
	if (g_objectTable == NULL || object < g_objectTable || object >= g_objectTable + g_objectTableSlotCount) {
		return 0;
	}
	model = Articulation_Model((unsigned int)(object - g_objectTable), &view);
	return model != NULL && model->articulationCount != 0;
}

void XwaArticulation_DrawnAngles(const ObjectRecord* object, uint16_t* angles, size_t count) {
	const CraftData* craft;
	size_t component;

	if (object == NULL || object->mobj == NULL || object->mobj->pCraft == NULL) {
		return;
	}
	craft = object->mobj->pCraft;
	if (count > XWA_CRAFT_MESH_SLOT_COUNT) {
		count = XWA_CRAFT_MESH_SLOT_COUNT;
	}
	/* A fine angle holds while meshRotation is its whole part; other code
	   that set meshRotation draws as it is. */
	for (component = 0; component < count; ++component) {
		const uint16_t fine = craft->articulationAngles[component];

		angles[component] = (uint8_t)(fine >> 8) == craft->meshRotation[component]
								? fine
								: (uint16_t)(craft->meshRotation[component] << 8);
	}
	if (g_objectTable != NULL && object >= g_objectTable && object < g_objectTable + g_objectTableSlotCount) {
		const unsigned int objIdx = (unsigned int)(object - g_objectTable);
		ArticulationModel view;
		ArticulationCrewMesh meshes[ARTICULATION_CREW_MAX];
		const int crewCount = Articulation_CrewMeshes(objIdx, Articulation_Model(objIdx, &view), meshes);
		int index;

		for (index = 0; index < crewCount; ++index) {
			if ((size_t)meshes[index].component < count) {
				angles[meshes[index].component] = craft->articulationAngles[meshes[index].component];
			}
		}
	}
}

static void Articulation_ClearMission(void) {
	uint32_t index;

	for (index = 0; index < s_mergedCount; ++index) {
		free(s_merged[index].entries);
	}
	s_mergedCount = 0;
	memset(s_flightGroups, 0, sizeof s_flightGroups);
}

/* One flight group's entry: `start` and `variant`. Returns 0 with *error
   set when the entry is malformed. */
static int Articulation_ReadFlightGroup(const AeronConfigNode* entry, ArticulationFlightGroup* fg,
										const char** error) {
	size_t index;

	for (index = 0; index < AeronConfigNode_MapCount(entry); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(entry, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(entry, index);
		size_t item;

		if (strcmp(key, "name") == 0 || strcmp(key, "craft") == 0) {
			continue;
		}
		if (strcmp(key, "variant") == 0) {
			const char* variant = AeronConfigNode_String(value, NULL);
			const char* cursor;

			if (AeronConfigNode_Type(value) != AERON_CONFIG_STRING || variant == NULL || variant[0] == '\0' ||
				strlen(variant) > ARTICULATION_VARIANT_MAX) {
				*error = "'variant' is not a name";
				return 0;
			}
			for (cursor = variant; *cursor != '\0'; ++cursor) {
				if (!((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= '0' && *cursor <= '9') || *cursor == '_')) {
					*error = "'variant' has a character outside [a-z0-9_]";
					return 0;
				}
			}
			snprintf(fg->variant, sizeof fg->variant, "%s", variant);
			continue;
		}
		if (strcmp(key, "start") != 0) {
			*error = "a key is not name, craft, start or variant";
			return 0;
		}
		if (AeronConfigNode_Type(value) != AERON_CONFIG_SEQUENCE) {
			*error = "'start' is not a list";
			return 0;
		}
		for (item = 0; item < AeronConfigNode_SequenceCount(value); ++item) {
			const char* group = AeronConfigNode_String(AeronConfigNode_SequenceGet(value, item), "");

			if (strcmp(group, "sfoils") == 0) {
				fg->start |= ARTICULATION_SFOILS;
			} else if (strcmp(group, "landing_gear") == 0) {
				fg->start |= ARTICULATION_LANDING_GEAR;
			} else {
				*error = "'start' lists a group other than sfoils and landing_gear";
				return 0;
			}
		}
	}
	return 1;
}

void XwaArticulation_LoadMission(const char* missionPath, const struct XwaFlightGroup* flightGroups, size_t stride,
								 int count) {
	char path[300];
	AeronConfigFile* config = NULL;
	const AeronConfigNode* section;
	const AeronConfigNode* groups;
	size_t index;
	int loaded = 0;

	Articulation_ClearMission();
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	section = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "articulations");
	groups = AeronConfigNode_MapGet(section, "flight_groups");
	if (section == NULL) {
		AeronConfigFile_Destroy(config);
		return;
	}
	if (AeronConfigNode_Type(groups) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.models", "%s: expected an 'articulations.flight_groups' map", path);
		AeronConfigFile_Destroy(config);
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(groups); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(groups, index);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(groups, index);
		const int fgIdx = XwaMissionFile_FlightGroup(path, key, entry, flightGroups, stride, count);
		ArticulationFlightGroup fg;
		const char* error = NULL;

		if (fgIdx < 0) {
			continue;
		}
		memset(&fg, 0, sizeof fg);
		if (!Articulation_ReadFlightGroup(entry, &fg, &error)) {
			Aeron_LogWarn("xwa.models", "%s: flight group %s: %s; ignored", path, key, error);
			continue;
		}
		s_flightGroups[fgIdx] = fg;
		++loaded;
	}
	AeronConfigFile_Destroy(config);
	Aeron_LogInfo("xwa.models", "%s: %d flight groups with articulation selections", path, loaded);
}

void XwaArticulation_InitMissionStart(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	const unsigned int fgIdx = craft != NULL ? g_objectTable[objIdx].flightGroupIdx : ARTICULATION_FLIGHT_GROUPS;

	/* hook_sfoils InitSFoilsLandingGearsHook: the meshes at their angle;
	   sFoilState stays as it is. */
	if (model != NULL && fgIdx < ARTICULATION_FLIGHT_GROUPS && s_flightGroups[fgIdx].start != 0) {
		Articulation_SetToAngle(model, craft, s_flightGroups[fgIdx].start);
	}
}
