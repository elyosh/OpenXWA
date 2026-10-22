#include "xwa_runtime/models/warhead_pools.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/frontend_mission.h"

#include <stdint.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* g_missionFlightGroups holds 192. */
#define WARHEAD_POOLS_FLIGHT_GROUPS 192
#define WARHEAD_POOLS_TYPES 10

static const char* const s_types[WARHEAD_POOLS_TYPES] = {
	"space_bombs",        "heavy_rockets", "missiles",   "torpedoes",           "advanced_missiles",
	"advanced_torpedoes", "mag_pulses",    "ion_pulses", "advanced_mag_pulses", "cluster_bombs",
};

/* The warheads left by flight group and type. -1 means no limit. They are
   only valid once a mission has cleared them. */
static int32_t s_left[WARHEAD_POOLS_FLIGHT_GROUPS][WARHEAD_POOLS_TYPES];
static int s_cleared;

void XwaWarheadPools_Clear(void) {
	memset(s_left, 0xff, sizeof s_left);
	s_cleared = 1;
}

int XwaWarheadPools_ReadFlightGroup(const char* path, const char* fgKey, int fgIdx, const XwaFlightGroup* fg,
									const AeronConfigNode* warheads) {
	size_t index;
	int set = 0;

	if (warheads == NULL || fgIdx < 0 || fgIdx >= WARHEAD_POOLS_FLIGHT_GROUPS) {
		return 0;
	}
	if (AeronConfigNode_Type(warheads) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.mission", "%s: flight group %s: warheads must be a map; it is ignored", path,
					  fgKey);
		return 0;
	}
	for (index = 0; index < AeronConfigNode_MapCount(warheads); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(warheads, index);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(warheads, index);
		int64_t value = AeronConfigNode_Int(node, -1);
		int type;

		for (type = 0; type < WARHEAD_POOLS_TYPES; ++type) {
			if (strcmp(key, s_types[type]) == 0) {
				break;
			}
		}
		if (type == WARHEAD_POOLS_TYPES) {
			Aeron_LogWarn("xwa.mission",
						  "%s: flight group %s: warheads: '%s' isn't a warhead type; it is ignored", path,
						  fgKey, key);
			continue;
		}
		if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 0 || value > INT32_MAX / 2) {
			Aeron_LogWarn("xwa.mission",
						  "%s: flight group %s: warheads.%s must be a number of 0 or more; it is ignored",
						  path, fgKey, key);
			continue;
		}
		/* The flight group's double and half warhead statuses. */
		if (value > 0) {
			if (fg->status1 == 1 || fg->status2 == 1) {
				value *= 2;
			} else if (fg->status1 == 2 || fg->status2 == 2) {
				value /= 2;
			}
		}
		s_left[fgIdx][type] = (int32_t)value;
		++set;
	}
	return set;
}

int XwaWarheadPools_Left(int fgIdx, int warheadType) {
	if (!s_cleared || fgIdx < 0 || fgIdx >= WARHEAD_POOLS_FLIGHT_GROUPS || warheadType < 1 ||
		warheadType > WARHEAD_POOLS_TYPES) {
		return -1;
	}
	return s_left[fgIdx][warheadType - 1];
}

int XwaWarheadPools_Limit(int left, int racks, int count) {
	if (left < 0 || racks <= 0) {
		return count;
	}
	return count < left / racks ? count : left / racks;
}

void XwaWarheadPools_Take(int fgIdx, int warheadType, int count) {
	int32_t* left;

	if (g_flightPlayerCount != 1 || count <= 0 || XwaWarheadPools_Left(fgIdx, warheadType) < 0) {
		return;
	}
	left = &s_left[fgIdx][warheadType - 1];
	*left = *left > count ? *left - count : 0;
	Aeron_LogInfo("xwa.mission", "flight group %d: %d %s taken from the pool, %d left", fgIdx, count,
				  s_types[warheadType - 1], (int)*left);
}
