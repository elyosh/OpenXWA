#include "xwa_runtime/runtime/original_film.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/file_io.h"
#include "xwa/flight/film.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/util/byte_order.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/models/object_limits.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart.

   The world state uses the original's record layout (the FlightSerialized*
   structs in flight.c). Each slot starts with a 16-bit object type. A slot
   with an object then has its 39-byte ObjectRecord. If the object has a
   mobile object, its 229-byte MobileObject follows, then a craft, warhead
   guidance and character record for each of the MobileObject's pool
   offsets that isn't 0. */

enum {
	STOCK_CRAFT_SIZE = 1017,
	OBJECT_SIZE = 39,
	MOBILE_SIZE = 229,
	GUIDANCE_SIZE = 10,
	CHAR_DATA_SIZE = 114,
	OBJECT_MOBILE = 0x23,
	MOBILE_GUIDANCE = 0xD9,
	MOBILE_CRAFT = 0xDD,
	MOBILE_CHAR_DATA = 0xE1,

	/* After the objects: the flight group statistics, the mission state, the
	   proving grounds' context when they're active, the countdown timers,
	   then the tail. */
	FG_STATS_SIZE = 370,
	MISSION_STATE_SIZE = 0x2D32,
	MISSION_STATE_PROVING_GROUNDS = 5,
	YARD_CONTEXT_SIZE = 3964,
	COUNTDOWN_TIMERS_SIZE = 24,
	/* The tail: the player count and 8 slot counts, the plans, the plan
	   count, an unused dword, the plan id cache and the random seed, then the
	   players. */
	TAIL_FIXED_SIZE = 36 + 256 * 0x55 + 4 + 4 + 256 + 2,
	PLAYER_SIZE = 3023,
	MAX_PLAYERS = 8,

	/* The stock craft record. */
	CRAFT_COMPONENT_STATE = 0x22E,
	CRAFT_ANIM_FRAME = 0x25F,
	CRAFT_MESH_ROTATION = 0x260,
	CRAFT_COMPONENT_HP = 0x292,
	CRAFT_ENGINES = 0x2CF,
	CRAFT_RACKS = 0x2DF,
	RACK_SIZE = 14,
};

typedef char original_film_rack_size[(sizeof(WarheadInventoryEntry) == RACK_SIZE) ? 1 : -1];

/* The craft record layout of original films. Without the section, records
   have the stock size and nothing is moved. */
typedef struct OriginalFilmLayout {
	uint32_t craftRecordSize;
	uint32_t meshes, engines, weaponRacks;
	uint32_t componentState, meshRotation, componentHp, engineHealth, weaponRackData;
} OriginalFilmLayout;

static OriginalFilmLayout s_layout = { STOCK_CRAFT_SIZE, 0, 0, 0, 0, 0, 0, 0, 0 };

static int OriginalFilm_ReadValue(const AeronConfigNode* section, const char* key, uint32_t min, uint32_t max,
								  uint32_t* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(section, key);
	int64_t value;

	if (node == NULL || AeronConfigNode_Type(node) != AERON_CONFIG_INT) {
		Aeron_LogError("xwa.film", "%s: original_films.%s must be a number; the section is ignored",
					   XWA_FLIGHT_CONFIG_PATH, key);
		return 0;
	}
	value = AeronConfigNode_Int(node, 0);
	if (value < (int64_t)min || value > (int64_t)max) {
		Aeron_LogError("xwa.film", "%s:%d: original_films.%s must be from %u to %u; the section is ignored",
					   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), key, min, max);
		return 0;
	}
	*out = (uint32_t)value;
	return 1;
}

void XwaOriginalFilm_Init(void) {
	const AeronConfigNode* section = AeronConfigNode_MapGet(XwaFlightConfig_Root(), "original_films");
	OriginalFilmLayout layout;

	memset(&s_layout, 0, sizeof s_layout);
	s_layout.craftRecordSize = STOCK_CRAFT_SIZE;
	if (section == NULL) {
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP ||
		!OriginalFilm_ReadValue(section, "craft_record_size", STOCK_CRAFT_SIZE, 65535, &layout.craftRecordSize) ||
		!OriginalFilm_ReadValue(section, "meshes", 50, XWA_CRAFT_MESH_SLOT_COUNT, &layout.meshes) ||
		!OriginalFilm_ReadValue(section, "engines", 16, XWA_CRAFT_ENGINE_EMITTER_COUNT, &layout.engines) ||
		!OriginalFilm_ReadValue(section, "weapon_racks", 16, 65535, &layout.weaponRacks) ||
		!OriginalFilm_ReadValue(section, "component_state", 0, 65535, &layout.componentState) ||
		!OriginalFilm_ReadValue(section, "mesh_rotation", 0, 65535, &layout.meshRotation) ||
		!OriginalFilm_ReadValue(section, "component_hp", 0, 65535, &layout.componentHp) ||
		!OriginalFilm_ReadValue(section, "engine_health", 0, 65535, &layout.engineHealth) ||
		!OriginalFilm_ReadValue(section, "weapon_rack_data", 0, 65535, &layout.weaponRackData)) {
		return;
	}
	if (layout.componentState + layout.meshes > layout.craftRecordSize ||
		layout.meshRotation + layout.meshes > layout.craftRecordSize ||
		layout.componentHp + layout.meshes > layout.craftRecordSize ||
		layout.engineHealth + layout.engines > layout.craftRecordSize ||
		layout.weaponRackData + layout.weaponRacks * RACK_SIZE > layout.craftRecordSize) {
		Aeron_LogError("xwa.film", "%s: the arrays of original_films don't fit craft_record_size; the section is "
								   "ignored",
					   XWA_FLIGHT_CONFIG_PATH);
		return;
	}
	s_layout = layout;
}

uint32_t XwaOriginalFilm_CraftRecordSize(void) { return s_layout.craftRecordSize; }

int XwaOriginalFilm_UseTailCounts(void) {
	static uint8_t tail[TAIL_FIXED_SIZE + MAX_PLAYERS * PLAYER_SIZE];
	uint32_t worldStateSize;
	uint32_t length;
	int start;
	uint32_t players;

	Film_SeekPastEmbeddedMissionData();
	if (!Film_ReadBytes(&worldStateSize, sizeof worldStateSize)) {
		return 0;
	}
	worldStateSize = ByteOrder_ReadU32Le((const uint8_t*)&worldStateSize);
	start = File_Tell(g_filmFile);
	length = worldStateSize < sizeof tail ? worldStateSize : (uint32_t)sizeof tail;
	File_Seek(g_filmFile, start + (int)(worldStateSize - length), SEEK_SET);
	if (!Film_ReadBytes(tail, (int)length)) {
		return 0;
	}
	/* The film has nothing after the players, so the tail is at a fixed
	   distance from the end for each player count. */
	for (players = 1; players <= MAX_PLAYERS; ++players) {
		const uint32_t tailSize = TAIL_FIXED_SIZE + players * PLAYER_SIZE;
		const uint8_t* counts;
		uint32_t craft, projectiles, debris, explosions, localDebris, regionMain, regionStatic, regions;

		if (tailSize > length) {
			break;
		}
		counts = tail + length - tailSize;
		if (ByteOrder_ReadU32Le(counts) != players) {
			continue;
		}
		craft = ByteOrder_ReadU32Le(counts + 4);
		projectiles = ByteOrder_ReadU32Le(counts + 12);
		debris = ByteOrder_ReadU32Le(counts + 16);
		explosions = ByteOrder_ReadU32Le(counts + 20);
		localDebris = ByteOrder_ReadU32Le(counts + 24);
		regionMain = ByteOrder_ReadU32Le(counts + 28);
		regionStatic = ByteOrder_ReadU32Le(counts + 32);
		regions = regionStatic / 128;
		/* The main slots of the regions are the craft, projectile, explosion
		   and debris slots, and the proving grounds' 224 salvage slots of
		   each region. */
		if (localDebris != 8 || regionStatic % 128 != 0 || regions != (uint32_t)g_pilotData.regionsCount ||
			debris != explosions ||
			(regionMain != craft + projectiles + explosions + debris &&
			 regionMain != craft + projectiles + explosions + debris + 224 * regions)) {
			continue;
		}
		/* The original game gives each player 12 projectile slots. */
		XwaObjectLimits_UseFilmCounts(craft, projectiles, 12u * players, explosions);
		return 1;
	}
	Aeron_LogError("xwa.film", "film: the object counts in its world state can't be found; it can't play");
	return 0;
}

static int OriginalFilm_IsDefault(const uint8_t* bytes, size_t size, uint8_t value) {
	size_t i;

	for (i = 0; i < size; ++i) {
		if (bytes[i] != value) {
			return 0;
		}
	}
	return 1;
}

/* Converts the craft record `record` to the stock record `stock`. When its
   entries past the stock counts aren't at their spawn values, writes its
   extension record to `extension` and returns 1. */
static int OriginalFilm_ConvertCraft(const uint8_t* record, uint16_t slot, uint8_t stock[STOCK_CRAFT_SIZE],
									 uint8_t* extension, uint32_t* droppedRacks) {
	static CraftData craft;
	const OriginalFilmLayout* layout = &s_layout;
	const uint32_t racks =
		layout->weaponRacks < XWA_CRAFT_WEAPON_SLOT_COUNT ? layout->weaponRacks : XWA_CRAFT_WEAPON_SLOT_COUNT;
	uint8_t animFrame;
	uint32_t i;

	/* The spawn values of the arrays (FlightWorldState_RestoreExtension). */
	memset(craft.componentState, 0, sizeof craft.componentState);
	memset(craft.meshRotation, 0, sizeof craft.meshRotation);
	memset(craft.componentHp, 0xFF, sizeof craft.componentHp);
	memset(craft.engineEmitterHealth, 0, sizeof craft.engineEmitterHealth);
	memset(craft.warheadData, 0, sizeof craft.warheadData);
	memcpy(craft.componentState, record + layout->componentState, layout->meshes);
	memcpy(craft.meshRotation, record + layout->meshRotation, layout->meshes);
	memcpy(craft.componentHp, record + layout->componentHp, layout->meshes);
	memcpy(craft.engineEmitterHealth, record + layout->engineHealth, layout->engines);
	memcpy(craft.warheadData, record + layout->weaponRackData, (size_t)racks * RACK_SIZE);
	for (i = racks; i < layout->weaponRacks; ++i) {
		if (!OriginalFilm_IsDefault(record + layout->weaponRackData + i * RACK_SIZE, RACK_SIZE, 0)) {
			++*droppedRacks;
		}
	}
	/* XWAU's hook moves the animation frame, the stock componentState[49], to
	   the last entry of the moved array. The port keeps it in its own field. */
	animFrame = craft.componentState[layout->meshes - 1];
	craft.componentState[layout->meshes - 1] = 0;

	memcpy(stock, record, STOCK_CRAFT_SIZE);
	memcpy(stock + CRAFT_COMPONENT_STATE, craft.componentState, 49);
	stock[CRAFT_ANIM_FRAME] = animFrame;
	memcpy(stock + CRAFT_MESH_ROTATION, craft.meshRotation, XWA_STOCK_MESH_SLOT_COUNT);
	memcpy(stock + CRAFT_COMPONENT_HP, craft.componentHp, XWA_STOCK_MESH_SLOT_COUNT);
	memcpy(stock + CRAFT_ENGINES, craft.engineEmitterHealth, XWA_STOCK_ENGINE_EMITTER_COUNT);
	memcpy(stock + CRAFT_RACKS, craft.warheadData, XWA_STOCK_WEAPON_SLOT_COUNT * RACK_SIZE);

	if (OriginalFilm_IsDefault(craft.componentState + 49, XWA_CRAFT_MESH_SLOT_COUNT - 49, 0) &&
		OriginalFilm_IsDefault(craft.meshRotation + XWA_STOCK_MESH_SLOT_COUNT,
							   XWA_CRAFT_MESH_SLOT_COUNT - XWA_STOCK_MESH_SLOT_COUNT, 0) &&
		OriginalFilm_IsDefault(craft.componentHp + XWA_STOCK_MESH_SLOT_COUNT,
							   XWA_CRAFT_MESH_SLOT_COUNT - XWA_STOCK_MESH_SLOT_COUNT, 0xFF) &&
		OriginalFilm_IsDefault(craft.engineEmitterHealth + XWA_STOCK_ENGINE_EMITTER_COUNT,
							   XWA_CRAFT_ENGINE_EMITTER_COUNT - XWA_STOCK_ENGINE_EMITTER_COUNT, 0) &&
		OriginalFilm_IsDefault((const uint8_t*)(craft.warheadData + XWA_STOCK_WEAPON_SLOT_COUNT),
							   (XWA_CRAFT_WEAPON_SLOT_COUNT - XWA_STOCK_WEAPON_SLOT_COUNT) * RACK_SIZE, 0)) {
		return 0;
	}
	FlightWorldState_EncodeExtensionRecord(extension, slot, &craft);
	return 1;
}

/* Walks the object records using the mod's craft record size and checks
   that the tail comes right after them. Returns 0 if it doesn't; otherwise
   sets the end of the objects and the craft count. */
static int OriginalFilm_Walk(const uint8_t* ws, uint32_t size, uint32_t* objectsEnd, uint32_t* crafts) {
	const uint32_t craftSize = s_layout.craftRecordSize;
	uint32_t at = 0;
	uint32_t slot;
	uint64_t state, tail;

	*crafts = 0;
	for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
		uint16_t type;

		if (at + 2 > size) {
			return 0;
		}
		type = ByteOrder_ReadU16Le(ws + at);
		at += 2;
		if (type == 0) {
			continue;
		}
		if (at + OBJECT_SIZE > size || ByteOrder_ReadU16Le(ws + at + 2) != type) {
			return 0;
		}
		if (ByteOrder_ReadU32Le(ws + at + OBJECT_MOBILE) == 0) {
			at += OBJECT_SIZE;
			continue;
		}
		at += OBJECT_SIZE;
		if (at + MOBILE_SIZE > size) {
			return 0;
		}
		{
			const uint32_t craft = ByteOrder_ReadU32Le(ws + at + MOBILE_CRAFT);
			const uint32_t guidance = ByteOrder_ReadU32Le(ws + at + MOBILE_GUIDANCE);
			const uint32_t charData = ByteOrder_ReadU32Le(ws + at + MOBILE_CHAR_DATA);

			if (craft != 0 && (craft - 1u) % craftSize != 0) {
				return 0;
			}
			at += MOBILE_SIZE + (craft ? craftSize : 0) + (guidance ? GUIDANCE_SIZE : 0) +
				  (charData ? CHAR_DATA_SIZE : 0);
			*crafts += craft != 0;
		}
		if (at > size) {
			return 0;
		}
	}
	state = (uint64_t)at + (uint64_t)FG_STATS_SIZE * (uint16_t)g_missionHeader.numFlightGroups;
	if (state + MISSION_STATE_SIZE > size) {
		return 0;
	}
	tail = state + MISSION_STATE_SIZE + (ws[state + MISSION_STATE_PROVING_GROUNDS] ? YARD_CONTEXT_SIZE : 0) +
		   COUNTDOWN_TIMERS_SIZE;
	if (tail + 4 > size) {
		return 0;
	}
	if (tail + TAIL_FIXED_SIZE + (uint64_t)PLAYER_SIZE * ByteOrder_ReadU32Le(ws + tail) != size) {
		return 0;
	}
	*objectsEnd = at;
	return 1;
}

int XwaOriginalFilm_ConvertWorldState(uint8_t* ws, int* sizeInOut) {
	const uint32_t craftSize = s_layout.craftRecordSize;
	const uint32_t size = (uint32_t)*sizeInOut;
	const size_t recordSize = FlightWorldState_ExtensionRecordSize();
	uint32_t objectsEnd, crafts;
	uint32_t in = 0, out = 0;
	uint32_t slot;
	uint8_t* records = NULL;
	uint16_t recordCount = 0;
	uint32_t droppedRacks = 0;

	if (craftSize == STOCK_CRAFT_SIZE) {
		return 1;
	}
	if (!OriginalFilm_Walk(ws, size, &objectsEnd, &crafts)) {
		Aeron_LogError("xwa.film",
					   "film: its world state doesn't match craft records of %u bytes "
					   "(original_films.craft_record_size); it can't play",
					   craftSize);
		return 0;
	}
	if (crafts != 0) {
		records = malloc(crafts * recordSize);
		if (records == NULL) {
			Aeron_LogError("xwa.film", "film: out of memory");
			return 0;
		}
	}
	/* Records move down from `in` to `out`. Craft records shrink, so `out`
	   never passes `in`. */
	for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
		const uint16_t type = ByteOrder_ReadU16Le(ws + in);

		memmove(ws + out, ws + in, 2);
		in += 2;
		out += 2;
		if (type == 0) {
			continue;
		}
		{
			const uint32_t mobile = ByteOrder_ReadU32Le(ws + in + OBJECT_MOBILE);

			memmove(ws + out, ws + in, OBJECT_SIZE);
			in += OBJECT_SIZE;
			out += OBJECT_SIZE;
			if (mobile == 0) {
				continue;
			}
		}
		{
			const uint32_t craft = ByteOrder_ReadU32Le(ws + in + MOBILE_CRAFT);
			const uint32_t guidance = ByteOrder_ReadU32Le(ws + in + MOBILE_GUIDANCE);
			const uint32_t charData = ByteOrder_ReadU32Le(ws + in + MOBILE_CHAR_DATA);

			memmove(ws + out, ws + in, MOBILE_SIZE);
			if (craft != 0) {
				/* Rewrite the craft's pool offset for stock-size records. */
				ByteOrder_WriteU32Le(ws + out + MOBILE_CRAFT, (craft - 1u) / craftSize * STOCK_CRAFT_SIZE + 1u);
			}
			in += MOBILE_SIZE;
			out += MOBILE_SIZE;
			if (craft != 0) {
				uint8_t stock[STOCK_CRAFT_SIZE];

				if (OriginalFilm_ConvertCraft(ws + in, (uint16_t)slot, stock, records + recordCount * recordSize,
											  &droppedRacks)) {
					++recordCount;
				}
				memcpy(ws + out, stock, STOCK_CRAFT_SIZE);
				in += craftSize;
				out += STOCK_CRAFT_SIZE;
			}
			if (guidance != 0) {
				memmove(ws + out, ws + in, GUIDANCE_SIZE);
				in += GUIDANCE_SIZE;
				out += GUIDANCE_SIZE;
			}
			if (charData != 0) {
				memmove(ws + out, ws + in, CHAR_DATA_SIZE);
				in += CHAR_DATA_SIZE;
				out += CHAR_DATA_SIZE;
			}
		}
	}
	/* The flight group statistics, the mission state and the tail. */
	memmove(ws + out, ws + in, size - in);
	out += size - in;
	if (recordCount != 0) {
		out += (uint32_t)FlightWorldState_EncodeExtensionBlock(ws + out, records, recordCount);
	}
	free(records);
	if (droppedRacks != 0) {
		Aeron_LogWarn("xwa.film", "film: dropped %u weapon racks beyond the port's %d", droppedRacks,
					  XWA_CRAFT_WEAPON_SLOT_COUNT);
	}
	Aeron_LogInfo("xwa.film", "film: %u craft records of %u bytes converted, %u with entries past the stock counts",
				  crafts, craftSize, recordCount);
	*sizeInOut = (int)out;
	return 1;
}
