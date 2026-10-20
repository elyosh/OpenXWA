#include "xwa_runtime/models/mission_tie.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa_runtime/models/mission_file.h"

#include <stdint.h>
#include <string.h>

/* Modern port module; no original counterpart. */

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

void XwaMissionTie_LoadMission(const char* missionPath, XwaFlightGroup* flightGroups, size_t stride,
							   int count) {
	char path[300];
	AeronConfigFile* config = NULL;
	const AeronConfigNode* values;
	size_t index;
	int set = 0;

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
	}
	AeronConfigFile_Destroy(config);
	if (set != 0) {
		Aeron_LogInfo("xwa.mission", "%s: %d flight-group values set", path, set);
	}
}
