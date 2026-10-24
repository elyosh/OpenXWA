#include "xwa_runtime/models/warhead_launchers.h"

#include "xwa/assets/model_def.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/object/object.h"

/* Modern port module; no original counterpart. */

int XwaWarheadLaunchers_HasSecond(unsigned int objectType) {
	const uint16_t modelIndex =
		objectType < OBJ_Count ? (uint16_t)g_objectTypes[objectType].modelDefIndex : 0xFFFF;

	return modelIndex != 0xFFFF && g_modelDefs[modelIndex].warheadLauncherType[1] != 0;
}

uint8_t XwaWarheadLaunchers_Loaded(const CraftData* craft) {
	if (craft->warheadSlotTypeIds[0] == 0) {
		return 0;
	}
	return craft->warheadSlotTypeIds[1] != 0 ? 2 : 1;
}

int XwaWarheadLaunchers_Warheads(const CraftData* craft, int launcher) {
	const ModelDef* record = &g_modelDefs[(uint16_t)craft->modelDefIndex];
	int count = 0;
	int rack;

	if (launcher >= craft->warheadLauncherCount) {
		return 0;
	}
	for (rack = record->warheadLauncherFirstSlot[launcher]; rack <= record->warheadLauncherLastSlot[launcher];
		 ++rack) {
		count += craft->warheadData[rack].count;
	}
	return count;
}

uint16_t XwaWarheadLaunchers_Index(uint16_t warheadType) {
	uint16_t index;

	for (index = 0; index < 11; ++index) {
		if (g_warheadTypeIds[index] == warheadType) {
			return index;
		}
	}
	return 0;
}

uint8_t XwaWarheadLaunchers_RackCount(uint16_t modelDefIndex) {
	const ModelDef* record = &g_modelDefs[modelDefIndex];
	int count = 0;
	int index;

	for (index = 0; index < 3; ++index) {
		if (record->laserGroupLastSlot[index] + 1 > count) {
			count = record->laserGroupLastSlot[index] + 1;
		}
	}
	for (index = 0; index < 2; ++index) {
		if (record->warheadLauncherLastSlot[index] + 1 > count) {
			count = record->warheadLauncherLastSlot[index] + 1;
		}
	}
	return (uint8_t)count;
}
