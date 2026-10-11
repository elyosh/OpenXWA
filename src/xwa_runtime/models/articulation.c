#include "xwa_runtime/models/articulation.h"

#include "aeron/asset/flight_model.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
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
/* The hangar shuttle's S-foils step on this legacy cadence, in the shuttle
   traffic's dt units (4 ticks): the original's 8-tick frames. */
#define ARTICULATION_SHUTTLE_CADENCE 2
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

/* Sets the meshes of `lists` to their angle (the byte wraps, as XWAU's). */
static void Articulation_SetToAngle(const ArticulationModel* model, CraftData* craft, int lists) {
	uint32_t index;

	for (index = 0; index < model->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];

		if (Articulation_InLists(articulation, lists)) {
			craft->meshRotation[articulation->component] = (uint8_t)articulation->angle;
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

/* hook_sfoils' common step: toward each entry's angle, or back to 0, by its
   speeds times `timeSpeed`. */
static void Articulation_StepGroup(const ArticulationModel* model, CraftData* craft, const char* group, int toAngle,
								   int timeSpeed) {
	uint32_t index;

	for (index = 0; index < model->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];
		uint8_t* rotation = &craft->meshRotation[articulation->component];
		const int speed = articulation->speed * timeSpeed;
		const int speedBack = articulation->speed_back * timeSpeed;

		if (!Articulation_InGroup(articulation, group)) {
			continue;
		}
		if (toAngle) {
			if (*rotation < articulation->angle) {
				*rotation = (uint8_t)(*rotation + speed);
				if (*rotation > articulation->angle) {
					*rotation = (uint8_t)articulation->angle;
				}
			}
		} else if (*rotation >= speedBack) {
			*rotation = (uint8_t)(*rotation - speedBack);
			if (*rotation < speedBack) {
				*rotation = 0;
			}
		}
	}
}

/* hook_sfoils SetCraftMeshRotationAngles: toward `angle` (each entry's own
   when `angle` is -1), from either side. */
static void Articulation_StepGroupToward(const ArticulationModel* model, CraftData* craft, const char* group,
										 int angle, int timeSpeed) {
	uint32_t index;

	for (index = 0; index < model->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];
		uint8_t* rotation = &craft->meshRotation[articulation->component];
		const int target = angle == -1 ? articulation->angle : angle;
		const int speed = articulation->speed * timeSpeed;
		const int speedBack = articulation->speed_back * timeSpeed;

		if (!Articulation_InGroup(articulation, group) || *rotation == target) {
			continue;
		}
		if (*rotation < target) {
			*rotation = *rotation < target - speed ? (uint8_t)(*rotation + speed) : (uint8_t)target;
		} else {
			*rotation = *rotation >= target + speedBack ? (uint8_t)(*rotation - speedBack) : (uint8_t)target;
		}
	}
}

static void Articulation_SetGroup(const ArticulationModel* model, CraftData* craft, const char* group, int toAngle) {
	uint32_t index;

	for (index = 0; index < model->articulationCount; ++index) {
		if (Articulation_InGroup(&model->articulations[index], group)) {
			craft->meshRotation[model->articulations[index].component] =
				toAngle ? (uint8_t)model->articulations[index].angle : 0;
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

/* hook_sfoils ObjectIndexTime::RetrieveTimeSpeed: 1 once per step period of
   accumulated ticks, else 0. XWAU keeps one accumulator per feature; so does
   CraftData.articulationTicks, by `clock`. */
static int Articulation_TimeSpeed(const ArticulationModel* model, CraftData* craft, int clock) {
	int32_t period = Articulation_Setting(model, "step_ticks", ARTICULATION_STEP_TICKS);
	uint32_t time = (uint32_t)craft->articulationTicks[clock] + g_elapsedTicks;

	if (period < 1) {
		period = 1;
	}
	if (time < (uint32_t)period) {
		craft->articulationTicks[clock] = (uint16_t)time;
		return 0;
	}
	craft->articulationTicks[clock] = (uint16_t)(time % (uint32_t)period);
	return 1;
}

void XwaArticulation_StepSFoils(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	const ObjectRecord* object;
	int lists;
	int timeSpeed;
	int moved;
	uint32_t index;

	if (craft == NULL || !Articulation_Has(model, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR)) {
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

	timeSpeed = Articulation_TimeSpeed(model, craft, ARTICULATION_CLOCK_SFOILS);
	moved = 0;
	for (index = 0; index < model->articulationCount; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];
		uint8_t* rotation = &craft->meshRotation[articulation->component];
		const int speed = articulation->speed * timeSpeed;
		const int speedBack = articulation->speed_back * timeSpeed;

		if (!Articulation_InLists(articulation, lists) || (craft->sFoilState & 1) == 0) {
			continue;
		}
		if ((craft->sFoilState & 2) != 0) {
			if (*rotation < articulation->angle) {
				*rotation = (uint8_t)(*rotation + speed);
				if (*rotation > articulation->angle) {
					*rotation = (uint8_t)articulation->angle;
				}
				moved = 1;
			}
		} else if (*rotation >= speedBack) {
			*rotation = (uint8_t)(*rotation - speedBack);
			if (*rotation < speedBack) {
				*rotation = 0;
			}
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
	   function steps side, forward, then the limbs, whatever their numbers. */
	for (pass = 0; pass < 2; ++pass) {
		for (component = 0; component < XWA_CRAFT_MESH_SLOT_COUNT; ++component) {
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
	int timeSpeed;

	/* hook_sfoils PlayerCraftHatchHook: phases 1-4 close the hatches (and
	   raise the landing gear from phase 2), 7-8 open them. */
	if (craft == NULL || !Articulation_HasGroup(model, "hatches") || !Articulation_HatchesActive()) {
		return;
	}
	timeSpeed = Articulation_TimeSpeed(model, craft, ARTICULATION_CLOCK_HATCHES);
	if (g_launchSeqPhase >= 1 && g_launchSeqPhase <= 4) {
		Articulation_StepGroup(model, craft, "hatches", 0, timeSpeed);
		if (g_launchSeqPhase >= 2) {
			Articulation_StepGroup(model, craft, "landing_gear", 0, timeSpeed);
		}
	} else if (g_launchSeqPhase == 7 || g_launchSeqPhase == 8) {
		Articulation_StepGroup(model, craft, "hatches", 1, timeSpeed);
	}
}

void XwaArticulation_StepShuttleHatches(unsigned int objIdx) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	int timeSpeed;

	/* hook_sfoils ShuttleHatchHook: open while parked (0), closing while
	   leaving (1-7), opening while landing (9). */
	if (craft == NULL || !Articulation_HasGroup(model, "hatches") || !Articulation_HatchesActive()) {
		return;
	}
	timeSpeed = Articulation_TimeSpeed(model, craft, ARTICULATION_CLOCK_HATCHES);
	if (g_hangarShuttleState.moveState == 0) {
		Articulation_SetGroup(model, craft, "hatches", 1);
	} else if (g_hangarShuttleState.moveState >= 1 && g_hangarShuttleState.moveState <= 7) {
		Articulation_StepGroup(model, craft, "hatches", 0, timeSpeed);
	} else if (g_hangarShuttleState.moveState == 9) {
		Articulation_StepGroup(model, craft, "hatches", 1, timeSpeed);
	}
}

int XwaArticulation_StepShuttleBlock(unsigned int objIdx, XwaArticulationShuttleStep step, int dtTicks) {
	ArticulationModel view;
	const ArticulationModel* model = Articulation_Model(objIdx, &view);
	CraftData* craft = Articulation_Craft(objIdx);
	uint32_t ticks;

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
	/* hook_sfoils SFoilsHangarShuttleHook steps by (speed * dt + 1) / 2, which
	   rounds differently at each dt; it runs here on the original's frames
	   (dt 2). */
	ticks = (uint32_t)craft->articulationTicks[ARTICULATION_CLOCK_SHUTTLE] + (uint32_t)(dtTicks > 0 ? dtTicks : 0);
	for (; ticks >= ARTICULATION_SHUTTLE_CADENCE; ticks -= ARTICULATION_SHUTTLE_CADENCE) {
		uint32_t index;

		for (index = 0; index < model->articulationCount; ++index) {
			const AeronFlightArticulation* articulation = &model->articulations[index];
			uint8_t* rotation = &craft->meshRotation[articulation->component];

			if (!Articulation_InLists(articulation, ARTICULATION_SFOILS | ARTICULATION_LANDING_GEAR)) {
				continue;
			}
			if (step == XWA_ARTICULATION_SHUTTLE_CLOSE) {
				const int speed = (articulation->speed * ARTICULATION_SHUTTLE_CADENCE + 1) / 2;

				if (*rotation < articulation->angle) {
					*rotation = (uint8_t)(*rotation + speed);
					if (*rotation > articulation->angle) {
						*rotation = (uint8_t)articulation->angle;
					}
				}
			} else {
				const int speed = (articulation->speed_back * ARTICULATION_SHUTTLE_CADENCE + 1) / 2;

				if (*rotation >= speed) {
					*rotation = (uint8_t)(*rotation - speed);
					if (*rotation < speed) {
						*rotation = 0;
					}
				}
			}
		}
	}
	craft->articulationTicks[ARTICULATION_CLOCK_SHUTTLE] = (uint16_t)ticks;
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
						   Articulation_TimeSpeed(model, craft, ARTICULATION_CLOCK_WARHEADS_BAY));
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
									 Articulation_TimeSpeed(model, craft, ARTICULATION_CLOCK_CARGO_CLAMPS));
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
	Articulation_StepGroup(model, craft, "hangar_doors", sameTeam < doorsDistance && otherTeams >= doorsDistance,
						   Articulation_TimeSpeed(model, craft, ARTICULATION_CLOCK_HANGAR_DOORS));
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
