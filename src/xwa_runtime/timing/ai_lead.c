#include "xwa_runtime/timing/ai_lead.h"

#include "xwa/assets/object_type.h"
#include "xwa/flight/object/object.h"
#include "xwa/util/time.h"
#include "xwa_runtime/timing/modern_flight_timing.h"

#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart.
 *
 * Each time the object step sets an object's prevWorld, this module first
 * adds world - prevWorld to a sum. Code that puts an object at a new place,
 * such as docking or the proving grounds reset, also sets prevWorld, so the
 * jump isn't added and doesn't count as movement, as in the original.
 *
 * At the AI step, world - prevWorld is movement that isn't in the sum yet.
 * For most objects it is the last step's movement. For the player's craft
 * it is the movement of the input frames played since the last object step:
 * the craft moves before the AI step, and the object step sets its
 * prevWorld after the AI step. This
 * part is counted in this AI step's movement, kept in `pending`, and taken
 * out of the next AI step's movement, because the object step adds it to
 * the sum in between. */

/* The movement of one object slot. */
typedef struct XwaAiLeadRecord {
	int32_t moved[3];    /* the sum of world - prevWorld since the last AI step */
	int32_t pending[3];  /* world - prevWorld at the last AI step */
	int32_t delta[3];    /* the movement between the last two AI steps */
	int time;            /* the object's game time at the last AI step */
	uint16_t ticks;      /* the ticks between the last two AI steps, 0 if unknown */
	uint8_t seen;        /* 1 after the object's first AI step */
	uint16_t objectType; /* with `signature`, tells when a new object has taken the slot */
	uint16_t signature;
} XwaAiLeadRecord;

static XwaAiLeadRecord* s_records;
static uint32_t s_recordCount;
/* The ticks of the last AI step, 0 before the first one. */
static uint16_t s_aiStepTicks;

void XwaAiLead_Reset(void) {
	if (s_records != NULL) {
		memset(s_records, 0, sizeof(*s_records) * s_recordCount);
	}
	s_aiStepTicks = 0;
}

/* Makes the table as large as the object table. Returns 0 if the table can't
   be allocated. */
static int XwaAiLead_Reserve(void) {
	XwaAiLeadRecord* records;

	if (s_recordCount >= g_objectTableSlotCount) {
		return s_records != NULL;
	}
	records = (XwaAiLeadRecord*)realloc(s_records, sizeof(*records) * g_objectTableSlotCount);
	if (records == NULL) {
		return 0;
	}
	memset(records + s_recordCount, 0, sizeof(*records) * (g_objectTableSlotCount - s_recordCount));
	s_records = records;
	s_recordCount = g_objectTableSlotCount;
	return 1;
}

/* Returns the record of object `slot`, or NULL if the table is too small. If
   a new object has taken the slot, the record is cleared first. */
static XwaAiLeadRecord* XwaAiLead_Record(uint32_t slot) {
	const ObjectRecord* object;
	XwaAiLeadRecord* record;

	if (slot >= s_recordCount) {
		return NULL;
	}
	object = &g_objectTable[slot];
	record = &s_records[slot];
	if (record->objectType != object->objectType || record->signature != object->objectSignature) {
		memset(record, 0, sizeof(*record));
		record->objectType = object->objectType;
		record->signature = object->objectSignature;
	}
	return record;
}

void XwaAiLead_AddMovement(uint32_t slot) {
	const ObjectRecord* object;
	XwaAiLeadRecord* record;

	if (!XwaModernFlightTiming_IsHighRate()) {
		return;
	}
	record = XwaAiLead_Record(slot);
	if (record == NULL) {
		return;
	}
	object = &g_objectTable[slot];
	record->moved[0] += object->world_x - object->mobj->prevWorldX;
	record->moved[1] += object->world_y - object->mobj->prevWorldY;
	record->moved[2] += object->world_z - object->mobj->prevWorldZ;
}

void XwaAiLead_BeginAiStep(uint16_t elapsed_ticks) {
	uint32_t slot;

	if (!XwaModernFlightTiming_IsHighRate() || !XwaAiLead_Reserve()) {
		return;
	}
	s_aiStepTicks = elapsed_ticks;
	for (slot = g_regionMainObjectSlotStart; slot < g_regionMainObjectSlotEnd; ++slot) {
		const ObjectRecord* object;
		const MobileObject* mobj;
		XwaAiLeadRecord* record;
		int32_t current[3];
		int time;
		int axis;

		object = &g_objectTable[slot];
		mobj = object->mobj;
		if (object->objectType == OBJ_None || mobj == NULL) {
			continue;
		}
		record = XwaAiLead_Record(slot);
		current[0] = object->world_x - mobj->prevWorldX;
		current[1] = object->world_y - mobj->prevWorldY;
		current[2] = object->world_z - mobj->prevWorldZ;
		/* The player's craft has its own time, which can be ahead of the
		   step. */
		time = mobj->simStateTimestamp != 0 ? mobj->simStateTimestamp : g_gameTime;
		record->ticks = 0;
		if (record->seen && time > record->time && time - record->time <= UINT16_MAX) {
			for (axis = 0; axis < 3; ++axis) {
				record->delta[axis] = record->moved[axis] + current[axis] - record->pending[axis];
			}
			record->ticks = (uint16_t)(time - record->time);
		}
		for (axis = 0; axis < 3; ++axis) {
			record->moved[axis] = 0;
			record->pending[axis] = current[axis];
		}
		record->time = time;
		record->seen = 1;
	}
}

int XwaAiLead_Movement(uint32_t slot, int* delta_x, int* delta_y, int* delta_z) {
	XwaAiLeadRecord* record;

	if (!XwaModernFlightTiming_IsHighRate() || s_aiStepTicks == 0) {
		return 0;
	}
	record = XwaAiLead_Record(slot);
	if (record == NULL || record->ticks == 0) {
		return 0;
	}
	*delta_x = (int)((int64_t)record->delta[0] * s_aiStepTicks / record->ticks);
	*delta_y = (int)((int64_t)record->delta[1] * s_aiStepTicks / record->ticks);
	*delta_z = (int)((int64_t)record->delta[2] * s_aiStepTicks / record->ticks);
	return 1;
}
