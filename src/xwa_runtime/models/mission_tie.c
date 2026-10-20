#include "xwa_runtime/models/mission_tie.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/models/skins.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* XU's missions give up to 18 craft markings of their own. */
#define MISSION_TIE_CRAFT_MARKINGS_MAX 512

/* The markings of craft `craft` of flight group `flightGroup`. */
typedef struct MissionTieCraftMarkings {
	int16_t flightGroup;
	uint8_t craft;
	uint8_t markings;
} MissionTieCraftMarkings;

static MissionTieCraftMarkings s_craftMarkings[MISSION_TIE_CRAFT_MARKINGS_MAX];
static int s_craftMarkingCount;

/* Reads `key` of `entry`, a number from 0 to 255, into `out`. Returns 0
   when the key is missing, -1 when it isn't such a number (logged). */
static int MissionTie_ReadByte(const char* path, const char* fgKey, const AeronConfigNode* entry,
							   const char* key, uint8_t* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(entry, key);
	int64_t value;

	if (node == NULL) {
		return 0;
	}
	value = AeronConfigNode_Int(node, -1);
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 0 || value > 255) {
		Aeron_LogWarn("xwa.mission", "%s: flight group %s: %s must be a number from 0 to 255; it is ignored",
					  path, fgKey, key);
		return -1;
	}
	*out = (uint8_t)value;
	return 1;
}

/* Reads the `craft_markings` map of flight group `fgIdx`: craft numbers from
   0 to 255 and their markings. Returns the number of values read. */
static int MissionTie_ReadCraftMarkings(const char* path, const char* fgKey, const AeronConfigNode* node,
										int fgIdx) {
	size_t index;
	int set = 0;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.mission",
					  "%s: flight group %s: craft_markings must be a map of craft numbers; it is ignored",
					  path, fgKey);
		return 0;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
		const int64_t markings = AeronConfigNode_Int(value, -1);
		char* end = NULL;
		const long craft = strtol(key, &end, 10);
		int at;

		if (end == key || *end != '\0' || craft < 0 || craft > 255) {
			Aeron_LogWarn(
				"xwa.mission",
				"%s: flight group %s: craft_markings: '%s' isn't a craft number from 0 to 255; it is ignored",
				path, fgKey, key);
			continue;
		}
		if (AeronConfigNode_Type(value) != AERON_CONFIG_INT || markings < 0 || markings > 255) {
			Aeron_LogWarn(
				"xwa.mission",
				"%s: flight group %s: the markings of craft %ld must be a number from 0 to 255; they are "
				"ignored",
				path, fgKey, craft);
			continue;
		}
		for (at = 0; at < s_craftMarkingCount; ++at) {
			if (s_craftMarkings[at].flightGroup == fgIdx && s_craftMarkings[at].craft == craft) {
				break;
			}
		}
		if (at == MISSION_TIE_CRAFT_MARKINGS_MAX) {
			Aeron_LogWarn("xwa.mission", "%s: more than %d craft markings; the rest are ignored", path,
						  MISSION_TIE_CRAFT_MARKINGS_MAX);
			break;
		}
		if (at == s_craftMarkingCount) {
			++s_craftMarkingCount;
		}
		s_craftMarkings[at].flightGroup = (int16_t)fgIdx;
		s_craftMarkings[at].craft = (uint8_t)craft;
		s_craftMarkings[at].markings = (uint8_t)markings;
		++set;
	}
	return set;
}

void XwaMissionTie_LoadMission(const char* missionPath, XwaFlightGroup* flightGroups, size_t stride,
							   int count) {
	char path[300];
	AeronConfigFile* config = NULL;
	const AeronConfigNode* values;
	size_t index;
	int set = 0;

	s_craftMarkingCount = 0;
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	/* The section is optional: the file may hold other settings only. */
	values = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "flight_groups");
	if (values != NULL && AeronConfigNode_Type(values) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.mission", "%s:%d: flight_groups must be a map of flight groups; it is ignored",
					   path, AeronConfigNode_Line(values));
		values = NULL;
	}
	for (index = 0; index < AeronConfigNode_MapCount(values); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(values, index);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(values, index);
		const AeronConfigNode* voice = AeronConfigNode_MapGet(entry, "pilot_voice");
		const AeronConfigNode* craftMarkings = AeronConfigNode_MapGet(entry, "craft_markings");
		const int fgIdx = XwaMissionFile_FlightGroup(path, key, entry, flightGroups, stride, count);
		XwaFlightGroup* fg;
		uint8_t byte;

		if (fgIdx < 0) {
			continue;
		}
		fg = (XwaFlightGroup*)((unsigned char*)flightGroups + (size_t)fgIdx * stride);
		if (MissionTie_ReadByte(path, key, entry, "markings", &byte) > 0) {
			fg->markings = byte;
			++set;
		}
		if (MissionTie_ReadByte(path, key, entry, "iff", &byte) > 0) {
			fg->iff = byte;
			++set;
		}
		if (voice != NULL) {
			const char* name = AeronConfigNode_String(voice, NULL);

			if (AeronConfigNode_Type(voice) != AERON_CONFIG_STRING || strlen(name) >= sizeof fg->pilotID) {
				Aeron_LogWarn("xwa.mission",
							  "%s: flight group %s: pilot_voice must be a name of up to %d characters; it is "
							  "ignored",
							  path, key, (int)sizeof fg->pilotID - 1);
			} else {
				memset(fg->pilotID, 0, sizeof fg->pilotID);
				memcpy(fg->pilotID, name, strlen(name));
				++set;
			}
		}
		if (craftMarkings != NULL) {
			set += MissionTie_ReadCraftMarkings(path, key, craftMarkings, fgIdx);
		}
	}
	AeronConfigFile_Destroy(config);
	if (set != 0) {
		Aeron_LogInfo("xwa.mission", "%s: %d flight-group values set", path, set);
	}
}

int XwaMissionTie_CraftMarkings(int flightGroupIdx, int craft) {
	int index;

	for (index = 0; index < s_craftMarkingCount; ++index) {
		if (s_craftMarkings[index].flightGroup == flightGroupIdx && s_craftMarkings[index].craft == craft) {
			return s_craftMarkings[index].markings;
		}
	}
	return -1;
}

void XwaMissionTie_UseSkins(void) {
	int index;

	for (index = 0; index < s_craftMarkingCount; ++index) {
		XwaSkins_UseCraftMarkings(s_craftMarkings[index].flightGroup, s_craftMarkings[index].craft,
								  s_craftMarkings[index].markings);
	}
}
