#include "xwa_runtime/models/object_profiles.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/opt_model.h"
#include "xwa/flight/object/object.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/models/model_roles.h"
#include "xwa_runtime/models/opt_synth.h"
#include "xwa/util/memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. It selects each object's
   profile from the mission's YAML selections; the profiles themselves are
   component sets written by the model converter. */

/* The skirmish test reads header.missionType, at its original offset. */
typedef char object_profiles_mission_type_offset
	[(offsetof(FrontendMission, header.missionType) == 0xB1B82) ? 1 : -1];

#define OBJECT_PROFILE_SET_PREFIX "hidden_by_profile."
/* AERON_FLIGHT_TAG_MAX minus its terminator. */
#define OBJECT_PROFILE_SET_LENGTH_MAX 63
/* g_missionFlightGroups and FrontendMission.flightGroups hold 192. */
#define OBJECT_PROFILE_FLIGHT_GROUPS 192

/* The current mission's profile per flight group; NULL selects nothing. */
static char* s_profiles[OBJECT_PROFILE_FLIGHT_GROUPS];

static void ObjectProfiles_Clear(void) {
	int index;

	for (index = 0; index < OBJECT_PROFILE_FLIGHT_GROUPS; ++index) {
		free(s_profiles[index]);
		s_profiles[index] = NULL;
	}
}

static char ObjectProfiles_Lower(char c) {
	return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

void XwaObjectProfiles_LoadMission(const char* missionPath, const struct XwaFlightGroup* flightGroups,
								   size_t stride, int count) {
	char path[300];
	AeronConfigFile* config = NULL;
	const AeronConfigNode* section;
	const AeronConfigNode* selections;
	size_t index;
	int loaded;

	ObjectProfiles_Clear();
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	/* The section is optional: the file may hold other selections only. */
	section = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "object_profiles");
	selections = AeronConfigNode_MapGet(section, "flight_groups");
	if (section == NULL) {
		AeronConfigFile_Destroy(config);
		return;
	}
	if (AeronConfigNode_Type(selections) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.models", "%s: expected an 'object_profiles.flight_groups' map", path);
		AeronConfigFile_Destroy(config);
		return;
	}

	loaded = 0;
	for (index = 0; index < AeronConfigNode_MapCount(selections); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(selections, index);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(selections, index);
		const char* profile = AeronConfigNode_String(AeronConfigNode_MapGet(entry, "profile"), NULL);
		const int fgIdx = XwaMissionFile_FlightGroup(path, key, entry, flightGroups, stride, count);

		if (fgIdx < 0) {
			continue;
		}
		if (profile == NULL || profile[0] == '\0') {
			Aeron_LogWarn("xwa.models", "%s: flight group %s has no 'profile'; ignored", path, key);
			continue;
		}
		free(s_profiles[fgIdx]);
		s_profiles[fgIdx] = (char*)malloc(strlen(profile) + 1);
		if (s_profiles[fgIdx] != NULL) {
			memcpy(s_profiles[fgIdx], profile, strlen(profile) + 1);
			++loaded;
		}
	}
	AeronConfigFile_Destroy(config);
	Aeron_LogInfo("xwa.models", "%s: %d object profile selections", path, loaded);
}

/* The components of profile `profile`'s set, `hidden_by_profile.` and the
   lower-case name with every byte outside [a-z0-9_] written as '.' and two
   hex digits, as the model converter writes it. A member may be past the
   model's components: it hides a component of the exterior or cockpit model
   drawn with the craft's state. */
static int ObjectProfiles_Collect(uint16_t objectType, const char* profile, int* components, int capacity) {
	char name[OBJECT_PROFILE_SET_LENGTH_MAX + 1];
	size_t length;
	const unsigned char* cursor;
	MemoryHandle handle;
	const OptimizedPolyObject* model;
	const AeronFlightComponentSet* set;
	int component;
	int count;

	length = strlen(OBJECT_PROFILE_SET_PREFIX);
	memcpy(name, OBJECT_PROFILE_SET_PREFIX, length);
	for (cursor = (const unsigned char*)profile; *cursor != '\0'; ++cursor) {
		const unsigned char c = (unsigned char)ObjectProfiles_Lower((char)*cursor);

		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
			if (length + 1 > OBJECT_PROFILE_SET_LENGTH_MAX) {
				return 0;
			}
			name[length++] = (char)c;
		} else {
			if (length + 3 > OBJECT_PROFILE_SET_LENGTH_MAX) {
				return 0;
			}
			snprintf(name + length, 4, ".%02x", c);
			length += 3;
		}
	}
	name[length] = '\0';

	handle = objectType < OBJ_Count ? g_loadedModels.byObjectType[objectType] : 0;
	model = handle != 0 ? (const OptimizedPolyObject*)Memory_LockHandle(handle) : NULL;
	if (model == NULL) {
		return 0;
	}
	set = OptSynth_FindComponentSet(model, name);
	count = 0;
	for (component = 0; set != NULL && component < XWA_CRAFT_MESH_SLOT_COUNT && count < capacity; ++component) {
		if ((set->members[component / 32] & (1u << (component % 32))) != 0) {
			components[count++] = component;
		}
	}
	Memory_UnlockHandle(handle);
	return count;
}

void XwaObjectProfiles_Apply(uint16_t objectType, int flightGroupIdx, uint8_t markings,
							 struct CraftData* craft) {
	char profile[32];
	const char* selected;
	int components[XWA_OPT_SYNTH_MAX_COMPONENTS];
	int count;
	int index;

	if (craft == NULL || !XwaModelRole_IsReplacement(objectType)) {
		return;
	}

	/* The flight group's profile. */
	selected = flightGroupIdx >= 0 && flightGroupIdx < OBJECT_PROFILE_FLIGHT_GROUPS ? s_profiles[flightGroupIdx]
																				   : NULL;
	/* In a quick start or skirmish, Skirmish when the model has it. */
	if (selected == NULL && g_frontendMission != NULL &&
		(g_frontendMission->header.missionType == XWA_MISSION_TYPE_QUICK_START ||
		 g_frontendMission->header.missionType == XWA_MISSION_TYPE_SKIRMISH) &&
		ObjectProfiles_Collect(objectType, "Skirmish", components, XWA_OPT_SYNTH_MAX_COMPONENTS) > 0) {
		selected = "Skirmish";
	}
	if (selected == NULL) {
		snprintf(profile, sizeof profile, "Default_%u", (unsigned int)markings);
		selected = profile;
	}

	/* A profile that hides nothing (undefined, empty or "-1") uses Default. */
	count = ObjectProfiles_Collect(objectType, selected, components, XWA_OPT_SYNTH_MAX_COMPONENTS);
	if (count == 0) {
		count = ObjectProfiles_Collect(objectType, "Default", components, XWA_OPT_SYNTH_MAX_COMPONENTS);
	}
	for (index = 0; index < count; ++index) {
		craft->componentHp[components[index]] = 0;
		craft->componentState[components[index]] = 4;
	}
}
