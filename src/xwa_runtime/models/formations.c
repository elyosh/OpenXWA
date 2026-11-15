#include "xwa_runtime/models/formations.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa_runtime/models/flight_config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

enum {
	FORMATION_COUNT = 34,
	FORMATION_SLOTS = 6,
};

/* Reads a whole number from -32768 to 32767. */
static int Formations_ReadInt16(const AeronConfigNode* node, int16_t* out) {
	int64_t value;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT) {
		return 0;
	}
	value = AeronConfigNode_Int(node, 0);
	if (value < INT16_MIN || value > INT16_MAX) {
		return 0;
	}
	*out = (int16_t)value;
	return 1;
}

/* Reads `slots` into the three rows. Returns 0, with a warning, when it
   isn't a list of six lists of three numbers. */
static int Formations_ReadSlots(const char* path, int formation, const AeronConfigNode* node, int16_t* x,
								int16_t* y, int16_t* z) {
	size_t slot;

	if (AeronConfigNode_Type(node) == AERON_CONFIG_SEQUENCE &&
		AeronConfigNode_SequenceCount(node) == FORMATION_SLOTS) {
		for (slot = 0; slot < FORMATION_SLOTS; ++slot) {
			const AeronConfigNode* values = AeronConfigNode_SequenceGet(node, slot);

			if (AeronConfigNode_Type(values) != AERON_CONFIG_SEQUENCE ||
				AeronConfigNode_SequenceCount(values) != 3 ||
				!Formations_ReadInt16(AeronConfigNode_SequenceGet(values, 0), &x[slot]) ||
				!Formations_ReadInt16(AeronConfigNode_SequenceGet(values, 1), &y[slot]) ||
				!Formations_ReadInt16(AeronConfigNode_SequenceGet(values, 2), &z[slot])) {
				break;
			}
		}
		if (slot == FORMATION_SLOTS) {
			return 1;
		}
	}
	Aeron_LogWarn("xwa.mission",
				  "%s:%d: formations.%d.slots must be six lists of three numbers from -32768 to 32767; the "
				  "formation keeps its slots",
				  path, AeronConfigNode_Line(node), formation);
	return 0;
}

static void Formations_ReadFormation(const char* path, int formation, const AeronConfigNode* entry) {
	const AeronConfigNode* node;
	int16_t x[FORMATION_SLOTS], y[FORMATION_SLOTS], z[FORMATION_SLOTS];
	int16_t divisor;
	size_t i;

	if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.mission", "%s:%d: formations.%d must be a map; it is ignored", path,
					  AeronConfigNode_Line(entry), formation);
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(entry); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(entry, i);

		if (strcmp(key, "slots") != 0 && strcmp(key, "divisor") != 0) {
			Aeron_LogWarn("xwa.mission", "%s:%d: formations.%d.%s isn't a setting; it is ignored", path,
						  AeronConfigNode_Line(AeronConfigNode_MapValueAt(entry, i)), formation, key);
		}
	}
	if ((node = AeronConfigNode_MapGet(entry, "slots")) != NULL &&
		Formations_ReadSlots(path, formation, node, x, y, z)) {
		memcpy(g_formPosX[formation], x, sizeof x);
		memcpy(g_formPosY[formation], y, sizeof y);
		memcpy(g_formPosZ[formation], z, sizeof z);
	}
	if ((node = AeronConfigNode_MapGet(entry, "divisor")) != NULL) {
		/* The game divides by it. */
		if (Formations_ReadInt16(node, &divisor) && divisor != 0) {
			g_formationDivisor[formation] = divisor;
		} else {
			Aeron_LogWarn("xwa.mission",
						  "%s:%d: formations.%d.divisor must be a number from -32768 to 32767 other than 0; "
						  "the formation keeps its divisor",
						  path, AeronConfigNode_Line(node), formation);
		}
	}
}

void XwaFormations_Init(void) {
	const AeronConfigNode* section = AeronConfigNode_MapGet(XwaFlightConfig_Root(), "formations");
	size_t i;

	if (section == NULL) {
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.mission", "%s:%d: formations must be a map; it is ignored",
					   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(section));
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(section); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(section, i);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(section, i);
		char* end;
		long formation = strtol(key, &end, 10);

		if (key[0] == '\0' || *end != '\0' || formation < 0 || formation >= FORMATION_COUNT) {
			Aeron_LogWarn("xwa.mission",
						  "%s:%d: formations.%s isn't a formation number from 0 to %d; it is "
						  "ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(entry), key, FORMATION_COUNT - 1);
			continue;
		}
		Formations_ReadFormation(XWA_FLIGHT_CONFIG_PATH, (int)formation, entry);
	}
}
