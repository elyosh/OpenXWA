#include "xwa_runtime/models/cockpit_eye.h"

#include "aeron/log.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa_runtime/models/model_roles.h"

/* Modern port module; no original counterpart. */

static int16_t s_baseEye[XWA_MODEL_DEF_COUNT][3];
static int s_baseEyeSaved;

/* Debug override: the original values of the one model edited. */
static int s_debugSavedModel = -1;
static int16_t s_debugSaved[3];

static void CockpitEye_SaveBase(void) {
	int index;

	if (s_baseEyeSaved) {
		return;
	}
	for (index = 0; index < XWA_MODEL_DEF_COUNT; ++index) {
		s_baseEye[index][0] = g_modelDefs[index].cockpitEyeX;
		s_baseEye[index][1] = g_modelDefs[index].cockpitEyeY;
		s_baseEye[index][2] = g_modelDefs[index].cockpitEyeZ;
	}
	s_baseEyeSaved = 1;
}

void XwaCockpitEye_ResetModelDef(uint16_t modelEntryIdx) {
	CockpitEye_SaveBase();
	if (modelEntryIdx >= XWA_MODEL_DEF_COUNT) {
		return;
	}
	g_modelDefs[modelEntryIdx].cockpitEyeX = s_baseEye[modelEntryIdx][0];
	g_modelDefs[modelEntryIdx].cockpitEyeY = s_baseEye[modelEntryIdx][1];
	g_modelDefs[modelEntryIdx].cockpitEyeZ = s_baseEye[modelEntryIdx][2];
}

void XwaCockpitEye_ApplyHardpointX(uint16_t modelEntryIdx, unsigned short objectType, int x) {
	if (modelEntryIdx < XWA_MODEL_DEF_COUNT && XwaModelRole_IsReplacement(objectType)) {
		g_modelDefs[modelEntryIdx].cockpitEyeX = (int16_t)x;
	}
}

static int CockpitEye_PlayerModelIndex(void) {
	int objectIdx;
	ModelIndex modelIndex;

	if (g_localPlayer < 0 || (uint16_t)g_players[g_localPlayer].objectIndex == 0xffffu) {
		return -1;
	}
	objectIdx = (uint16_t)g_players[g_localPlayer].objectIndex;
	modelIndex = GetModelIndexFromType(g_objectTable[objectIdx].objectType);
	return modelIndex >= XWA_MODEL_DEF_COUNT ? -1 : (int)modelIndex;
}

int XwaCockpitEye_GetPlayer(int* modelIndex, int eye[3]) {
	const int index = CockpitEye_PlayerModelIndex();

	if (index < 0) {
		return 0;
	}
	if (modelIndex) {
		*modelIndex = index;
	}
	eye[0] = g_modelDefs[index].cockpitEyeX;
	eye[1] = g_modelDefs[index].cockpitEyeY;
	eye[2] = g_modelDefs[index].cockpitEyeZ;
	return 1;
}

int XwaCockpitEye_SetPlayer(const int eye[3]) {
	const int index = CockpitEye_PlayerModelIndex();

	if (index < 0) {
		return 0;
	}
	if (s_debugSavedModel != index) {
		XwaCockpitEye_RestorePlayer();
		s_debugSavedModel = index;
		s_debugSaved[0] = g_modelDefs[index].cockpitEyeX;
		s_debugSaved[1] = g_modelDefs[index].cockpitEyeY;
		s_debugSaved[2] = g_modelDefs[index].cockpitEyeZ;
	}
	g_modelDefs[index].cockpitEyeX = (int16_t)eye[0];
	g_modelDefs[index].cockpitEyeY = (int16_t)eye[1];
	g_modelDefs[index].cockpitEyeZ = (int16_t)eye[2];
	Aeron_LogInfo("xwa.debug", "cockpit eye model=%d X=%d Y=%d Z=%d", index, eye[0], eye[1], eye[2]);
	return 1;
}

int XwaCockpitEye_RestorePlayer(void) {
	if (s_debugSavedModel < 0) {
		return 0;
	}
	g_modelDefs[s_debugSavedModel].cockpitEyeX = s_debugSaved[0];
	g_modelDefs[s_debugSavedModel].cockpitEyeY = s_debugSaved[1];
	g_modelDefs[s_debugSavedModel].cockpitEyeZ = s_debugSaved[2];
	s_debugSavedModel = -1;
	return 1;
}
