#include "xwa_runtime/models/model_swaps.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/tech_library.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/models/model_source.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* XU's missions have up to 16. */
#define MODEL_SWAPS_MAX 64

typedef struct ModelSwap {
	char source[XWA_MODEL_SOURCE_KEY_MAX];
	char target[XWA_MODEL_SOURCE_KEY_MAX];
} ModelSwap;

static ModelSwap s_swaps[MODEL_SWAPS_MAX];
static int s_swapCount;
/* Counts the calls to XwaModelSwaps_LoadMission. */
static int s_loadCount;

/* The tech room shows every model as it is. */
static int ModelSwaps_Active(void) {
	return s_swapCount != 0 && !(g_screenStackTop >= 0 && g_screenStackTop < FRONTEND_SCREEN_MAX_STACK &&
								 g_screenStates[g_screenStackTop].updateFn == TechLibrary_Update);
}

static const ModelSwap* ModelSwaps_Find(const char* source) {
	int i;

	for (i = 0; i < s_swapCount; ++i) {
		if (strcmp(s_swaps[i].source, source) == 0) {
			return &s_swaps[i];
		}
	}
	return NULL;
}

void XwaModelSwaps_LoadMission(const char* missionPath) {
	AeronConfigFile* config = NULL;
	const AeronConfigNode* swaps;
	char path[300];
	size_t i;

	s_swapCount = 0;
	++s_loadCount;
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	swaps = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "model_swaps");
	if (swaps != NULL && AeronConfigNode_Type(swaps) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.models", "%s:%d: model_swaps must be a map of model keys; it is ignored", path,
					   AeronConfigNode_Line(swaps));
		swaps = NULL;
	}
	for (i = 0; i < AeronConfigNode_MapCount(swaps); ++i) {
		const char* source = AeronConfigNode_MapKeyAt(swaps, i);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(swaps, i);
		const char* target = AeronConfigNode_String(node, NULL);
		ModelSwap* swap = &s_swaps[s_swapCount];

		if (s_swapCount == MODEL_SWAPS_MAX) {
			Aeron_LogError("xwa.models", "%s: more than %d model swaps; the rest are ignored", path,
						   MODEL_SWAPS_MAX);
			break;
		}
		if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING ||
			!XwaModelSource_NormalizeKey(source, swap->source) ||
			!XwaModelSource_NormalizeKey(target, swap->target) || strcmp(swap->source, source) != 0 ||
			strcmp(swap->target, target) != 0) {
			Aeron_LogWarn("xwa.models",
						  "%s:%d: model swap '%s' must give a model key for a model key; it is "
						  "ignored",
						  path, AeronConfigNode_Line(node), source);
			continue;
		}
		if (ModelSwaps_Find(swap->source) != NULL) {
			continue;
		}
		/* A target that no list has, such as superstardestroyerrecentered,
		   must be made loadable by its own name. */
		if (!XwaModelSource_AddKey(swap->target)) {
			Aeron_LogWarn("xwa.models",
						  "%s:%d: model swap '%s': flight/models/%s.glb can't be loaded; %s keeps "
						  "its model",
						  path, AeronConfigNode_Line(node), source, swap->target, source);
			continue;
		}
		++s_swapCount;
	}
	AeronConfigFile_Destroy(config);
	if (s_swapCount != 0) {
		Aeron_LogInfo("xwa.models", "%s: %d model swaps", path, s_swapCount);
	}
}

int XwaModelSwaps_FileName(const char* fileName, char* out, size_t outSize) {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	const ModelSwap* swap;

	if (!ModelSwaps_Active() || !XwaModelSource_NormalizeKey(fileName, key) ||
		(swap = ModelSwaps_Find(key)) == NULL) {
		return 0;
	}
	return snprintf(out, outSize, "FlightModels\\%s.opt", swap->target) < (int)outSize;
}

int XwaModelSwaps_TypeKey(unsigned int objectType, char* key) {
	const ModelSwap* swap;

	if (!XwaModelSource_TypeKey(objectType, key)) {
		return 0;
	}
	if (ModelSwaps_Active() && (swap = ModelSwaps_Find(key)) != NULL) {
		snprintf(key, XWA_MODEL_SOURCE_KEY_MAX, "%s", swap->target);
	}
	return 1;
}

int XwaModelSwaps_State(void) { return s_loadCount * 2 + ModelSwaps_Active(); }

const char* XwaModelSwaps_SourceOf(const char* key) {
	char normalized[XWA_MODEL_SOURCE_KEY_MAX];
	int i;

	if (!ModelSwaps_Active() || !XwaModelSource_NormalizeKey(key, normalized)) {
		return NULL;
	}
	for (i = 0; i < s_swapCount; ++i) {
		if (strcmp(s_swaps[i].target, normalized) == 0) {
			return s_swaps[i].source;
		}
	}
	return NULL;
}
