#include "xwa_runtime/models/object_profiles.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/opt_model.h"
#include "xwa/flight/object/object.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/models/model_roles.h"
#include "xwa_runtime/models/names.h"
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

/* The most profiles a mission can set in `object_profiles.weapons`. */
#define OBJECT_PROFILE_WEAPONS 64
/* How many object and source type pairs the cache of hidden components
   holds. */
#define OBJECT_PROFILE_CACHE 32

/* The current mission's profile per flight group; NULL selects nothing. */
static char* s_profiles[OBJECT_PROFILE_FLIGHT_GROUPS];

/* The current mission's profile for objects of type `objectType`, such as
   projectiles, when craft of type `sourceType` fire them. */
typedef struct ObjectProfileWeapon {
	uint16_t sourceType;
	uint16_t objectType;
	char* profile;
} ObjectProfileWeapon;
static ObjectProfileWeapon s_weapons[OBJECT_PROFILE_WEAPONS];
static int s_weaponCount;

/* The components hidden on objects without craft data, for each object type
   and source type. Bit c of `hidden` is set when component c is hidden. */
typedef struct ObjectProfileHidden {
	uint16_t objectType;
	uint16_t sourceType;
	int any;
	uint32_t hidden[(XWA_OPT_SYNTH_MAX_COMPONENTS + 31) / 32];
} ObjectProfileHidden;
static ObjectProfileHidden s_hidden[OBJECT_PROFILE_CACHE];
static int s_hiddenCount;
static int s_hiddenNext;

static void ObjectProfiles_Clear(void) {
	int index;

	for (index = 0; index < OBJECT_PROFILE_FLIGHT_GROUPS; ++index) {
		free(s_profiles[index]);
		s_profiles[index] = NULL;
	}
	for (index = 0; index < s_weaponCount; ++index) {
		free(s_weapons[index].profile);
	}
	s_weaponCount = 0;
	s_hiddenCount = 0;
	s_hiddenNext = 0;
}

static char* ObjectProfiles_Copy(const char* text) {
	char* copy = (char*)malloc(strlen(text) + 1);

	if (copy != NULL) {
		memcpy(copy, text, strlen(text) + 1);
	}
	return copy;
}

/* Reads `weapons`, the object_profiles.weapons map of file `path`. For each
   craft type, it maps the types of the objects that the craft fires to a
   profile. Returns the number of profiles it keeps. */
static int ObjectProfiles_ReadWeapons(const char* path, const AeronConfigNode* weapons) {
	size_t sourceIndex;
	size_t index;
	int loaded = 0;

	if (AeronConfigNode_Type(weapons) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.models", "%s: expected an 'object_profiles.weapons' map", path);
		return 0;
	}
	for (sourceIndex = 0; sourceIndex < AeronConfigNode_MapCount(weapons); ++sourceIndex) {
		const char* sourceName = AeronConfigNode_MapKeyAt(weapons, sourceIndex);
		const AeronConfigNode* byType = AeronConfigNode_MapValueAt(weapons, sourceIndex);
		const int sourceType = XwaNames_Find(&g_xwaObjectTypeNames, sourceName);

		if (sourceType < 0) {
			Aeron_LogWarn("xwa.models", "%s: object_profiles.weapons: '%s' isn't an object type; ignored", path,
						  sourceName);
			continue;
		}
		if (AeronConfigNode_Type(byType) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s: object_profiles.weapons.%s isn't a map; ignored", path, sourceName);
			continue;
		}
		for (index = 0; index < AeronConfigNode_MapCount(byType); ++index) {
			const char* typeName = AeronConfigNode_MapKeyAt(byType, index);
			const char* profile = AeronConfigNode_String(AeronConfigNode_MapValueAt(byType, index), NULL);
			const int objectType = XwaNames_Find(&g_xwaObjectTypeNames, typeName);

			if (objectType < 0 || profile == NULL || profile[0] == '\0') {
				Aeron_LogWarn("xwa.models",
							  "%s: object_profiles.weapons.%s.%s needs an object type and a profile; ignored", path,
							  sourceName, typeName);
				continue;
			}
			if (s_weaponCount == OBJECT_PROFILE_WEAPONS) {
				Aeron_LogWarn("xwa.models", "%s: object_profiles.weapons has more than %d selections; the rest are ignored",
							  path, OBJECT_PROFILE_WEAPONS);
				return loaded;
			}
			s_weapons[s_weaponCount].profile = ObjectProfiles_Copy(profile);
			if (s_weapons[s_weaponCount].profile == NULL) {
				return loaded;
			}
			s_weapons[s_weaponCount].sourceType = (uint16_t)sourceType;
			s_weapons[s_weaponCount].objectType = (uint16_t)objectType;
			++s_weaponCount;
			++loaded;
		}
	}
	return loaded;
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
	const AeronConfigNode* weapons;
	size_t index;
	int loaded;
	int weaponsLoaded;

	ObjectProfiles_Clear();
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	/* The section is optional: the file may hold other selections only. */
	section = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "object_profiles");
	selections = AeronConfigNode_MapGet(section, "flight_groups");
	weapons = AeronConfigNode_MapGet(section, "weapons");
	if (section == NULL) {
		AeronConfigFile_Destroy(config);
		return;
	}
	if (selections != NULL && AeronConfigNode_Type(selections) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.models", "%s: expected an 'object_profiles.flight_groups' map", path);
		selections = NULL;
	}

	loaded = 0;
	for (index = 0; selections != NULL && index < AeronConfigNode_MapCount(selections); ++index) {
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
		s_profiles[fgIdx] = ObjectProfiles_Copy(profile);
		if (s_profiles[fgIdx] != NULL) {
			++loaded;
		}
	}
	weaponsLoaded = weapons != NULL ? ObjectProfiles_ReadWeapons(path, weapons) : 0;
	AeronConfigFile_Destroy(config);
	Aeron_LogInfo("xwa.models", "%s: %d object profile selections, %d for weapons", path, loaded, weaponsLoaded);
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

/* Whether the current mission is a quick start or a skirmish. */
static int ObjectProfiles_InSkirmish(void) {
	return g_frontendMission != NULL && (g_frontendMission->header.missionType == XWA_MISSION_TYPE_QUICK_START ||
										 g_frontendMission->header.missionType == XWA_MISSION_TYPE_SKIRMISH);
}

int XwaObjectProfiles_HideWithoutCraft(uint16_t objectType, uint16_t sourceType, uint8_t* states, int count) {
	int components[XWA_OPT_SYNTH_MAX_COMPONENTS];
	ObjectProfileHidden* entry;
	const char* selected;
	int found;
	int index;

	if (states == NULL || objectType >= OBJ_Count || !XwaModelRole_IsReplacement(objectType) ||
		g_loadedModels.byObjectType[objectType] == 0) {
		return 0;
	}
	entry = NULL;
	for (index = 0; index < s_hiddenCount; ++index) {
		if (s_hidden[index].objectType == objectType && s_hidden[index].sourceType == sourceType) {
			entry = &s_hidden[index];
			break;
		}
	}
	if (entry == NULL) {
		/* Use the mission's profile for this object type and source type,
		   then Skirmish in a quick start or skirmish if the model has it,
		   then Default. */
		selected = NULL;
		for (index = 0; index < s_weaponCount; ++index) {
			if (s_weapons[index].objectType == objectType && s_weapons[index].sourceType == sourceType) {
				selected = s_weapons[index].profile;
				break;
			}
		}
		if (selected == NULL && ObjectProfiles_InSkirmish() &&
			ObjectProfiles_Collect(objectType, "Skirmish", components, XWA_OPT_SYNTH_MAX_COMPONENTS) > 0) {
			selected = "Skirmish";
		}
		if (selected == NULL) {
			selected = "Default";
		}
		found = ObjectProfiles_Collect(objectType, selected, components, XWA_OPT_SYNTH_MAX_COMPONENTS);
		if (s_hiddenCount < OBJECT_PROFILE_CACHE) {
			entry = &s_hidden[s_hiddenCount++];
		} else {
			entry = &s_hidden[s_hiddenNext];
			s_hiddenNext = (s_hiddenNext + 1) % OBJECT_PROFILE_CACHE;
		}
		memset(entry, 0, sizeof *entry);
		entry->objectType = objectType;
		entry->sourceType = sourceType;
		for (index = 0; index < found; ++index) {
			entry->hidden[components[index] / 32] |= 1u << (components[index] % 32);
			entry->any = 1;
		}
	}
	if (!entry->any) {
		return 0;
	}
	found = 0;
	for (index = 0; index < count && index < XWA_OPT_SYNTH_MAX_COMPONENTS; ++index) {
		if ((entry->hidden[index / 32] & (1u << (index % 32))) != 0) {
			states[index] = 4;
			found = 1;
		}
	}
	return found;
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
	if (selected == NULL && ObjectProfiles_InSkirmish() &&
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

void XwaObjectProfiles_ApplyNamed(uint16_t objectType, const char* profile, struct CraftData* craft) {
	int components[XWA_OPT_SYNTH_MAX_COMPONENTS];
	int count;
	int index;

	if (craft == NULL || profile == NULL || !XwaModelRole_IsReplacement(objectType)) {
		return;
	}
	for (index = 0; index < XWA_CRAFT_MESH_SLOT_COUNT; ++index) {
		craft->componentHp[index] = 0xFF;
		craft->componentState[index] = 0;
	}
	count = ObjectProfiles_Collect(objectType, profile, components, XWA_OPT_SYNTH_MAX_COMPONENTS);
	for (index = 0; index < count; ++index) {
		craft->componentHp[components[index]] = 0;
		craft->componentState[components[index]] = 4;
	}
}
