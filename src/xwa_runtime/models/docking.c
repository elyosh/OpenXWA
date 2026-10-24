#include "xwa_runtime/models/docking.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa_runtime/models/model_settings.h"

#include <stdint.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define DOCKING_FLIGHT_YAML "flight/flight.yaml"

enum {
	/* The approach height cap of a model without the setting. */
	DOCKING_DEFAULT_APPROACH_HEIGHT = 932,
};

static int s_dockToSmallSetsBig;
static int s_aiOffsetsOwnDockPoint;
static int s_carriedOffsetsCarrierDockPoint;
static int s_playerApproachHeight;
static int s_playerSnapsToTargetOrientation;
static uint8_t s_hasApproachHeight[OBJ_Count];
static int32_t s_approachHeight[OBJ_Count];

static void Docking_ReadBool(const AeronConfigNode* section, const char* key, int* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(section, key);

	if (node == NULL) {
		return;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogWarn("xwa.models", "%s:%d: docking.%s must be true or false; it is ignored",
					  DOCKING_FLIGHT_YAML, AeronConfigNode_Line(node), key);
		return;
	}
	*out = AeronConfigNode_Bool(node, 0) != 0;
}

void XwaDocking_Init(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* section;

	s_dockToSmallSetsBig = 0;
	s_aiOffsetsOwnDockPoint = 0;
	s_carriedOffsetsCarrierDockPoint = 0;
	s_playerApproachHeight = 0;
	s_playerSnapsToTargetOrientation = 0;
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, DOCKING_FLIGHT_YAML, &file, &error)) {
		/* The backdrops code already logs this error. */
		return;
	}
	root = AeronConfigFile_Root(file);
	section = AeronConfigNode_MapGet(root, "docking");
	if (AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1 || section == NULL) {
		AeronConfigFile_Destroy(file);
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.models", "%s:%d: docking must be a map; it is ignored", DOCKING_FLIGHT_YAML,
					   AeronConfigNode_Line(section));
		AeronConfigFile_Destroy(file);
		return;
	}
	Docking_ReadBool(section, "dock_to_small_sets_big", &s_dockToSmallSetsBig);
	Docking_ReadBool(section, "ai_offsets_own_dock_point", &s_aiOffsetsOwnDockPoint);
	Docking_ReadBool(section, "carried_offsets_carrier_dock_point", &s_carriedOffsetsCarrierDockPoint);
	Docking_ReadBool(section, "player_approach_height", &s_playerApproachHeight);
	Docking_ReadBool(section, "player_snaps_to_target_orientation", &s_playerSnapsToTargetOrientation);
	AeronConfigFile_Destroy(file);
}

void XwaDocking_ClearModelSettings(void) { memset(s_hasApproachHeight, 0, sizeof s_hasApproachHeight); }

void XwaDocking_ReadModelSettings(unsigned int objectType, const char* path, const AeronConfigNode* group) {
	size_t index;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: docking isn't a map. It is ignored", path);
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(group); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(group, index);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(group, index);
		const int64_t value = AeronConfigNode_Int(node, 0);

		if (strcmp(key, "approach_height") != 0) {
			Aeron_LogWarn("xwa.models", "%s: docking.%s isn't a setting. It is ignored", path, key);
			continue;
		}
		if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < INT32_MIN || value > INT32_MAX) {
			Aeron_LogWarn("xwa.models", "%s: docking.approach_height must be a number. It is ignored", path);
			continue;
		}
		s_approachHeight[objectType] = (int32_t)value;
		s_hasApproachHeight[objectType] = 1;
	}
}

int XwaDocking_DockToSmallSetsBig(void) { return s_dockToSmallSetsBig; }

int XwaDocking_AiOffsetsOwnDockPoint(void) { return s_aiOffsetsOwnDockPoint; }

int XwaDocking_CarriedOffsetsCarrierDockPoint(void) { return s_carriedOffsetsCarrierDockPoint; }

int XwaDocking_PlayerApproachHeight(void) { return s_playerApproachHeight; }

int XwaDocking_PlayerSnapsToTargetOrientation(void) { return s_playerSnapsToTargetOrientation; }

int XwaDocking_ApproachHeight(unsigned int targetType, int dockGap) {
	int cap = targetType == OBJ_SpaceColony2 ? 0 : DOCKING_DEFAULT_APPROACH_HEIGHT;

	if (targetType < OBJ_Count) {
		XwaModelSettings_Ensure(targetType);
		if (s_hasApproachHeight[targetType]) {
			cap = s_approachHeight[targetType];
		}
	}
	return dockGap < cap ? dockGap : cap;
}
