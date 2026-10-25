#include "xwa_runtime/runtime/film_state.h"

#include "aeron/log.h"
#include "xwa/flight/death_star.h"
#include "xwa/flight/film.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/util/byte_order.h"
#include "xwa_runtime/models/hangar_scene.h"
#include "xwa_runtime/models/hangars.h"
#include "xwa_runtime/models/mission_settings.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/models/shield_recharge.h"
#include "xwa_runtime/models/warhead_pools.h"
#include "xwa_runtime/timing/modern_flight_timing.h"

#include <stdint.h>
#include <string.h>

/* Modern port module; no original counterpart.

   The block, in little-endian order:
	 u32 size of what follows, u16 block version 1,
	 u16 timing step, u8 model replacements, u8 mod name length, the name,
	 i32 shield recharge delays[256], i32 warhead pools[192][10],
	 i32 last targets[8],
	 hangars: u8 resolved, in hangar, proving ground, ship markings,
	   i32 ship IFF, char ship key[64],
	 hangar scene: i32 launch reference offset[2], i32 re-entry start Y,
	   u8 shuttle loaded,
	 Death Star reactor: i32 random drift[3], drift remainders[3], cylinder
	   yaw remainder, core drift speed remainder, core pitch remainder,
	   f64 core directed drift remainders[3], u16 shockwave light value.
   A newer block version is skipped. */

enum {
	FILM_STATE_VERSION = 1,
	FILM_STATE_MOD_NAME_MAX = 255,
	FILM_STATE_MAX_SIZE = 2 + 2 + 1 + 1 + FILM_STATE_MOD_NAME_MAX + 4 * XWA_SHIELD_RECHARGE_FLIGHT_GROUPS +
						  4 * XWA_WARHEAD_POOLS_FLIGHT_GROUPS * XWA_WARHEAD_POOLS_TYPES +
						  4 * XWA_PLAYER_COUNT + 4 + 4 + 64 + 4 * 2 + 4 + 1 + 4 * 9 + 8 * 3 + 2,
};

static char s_modName[FILM_STATE_MOD_NAME_MAX + 1];

typedef struct FilmStateCursor {
	uint8_t* data;
	size_t size;
	size_t at;
	int ok;
} FilmStateCursor;

static uint8_t* FilmState_Take(FilmStateCursor* cursor, size_t size) {
	uint8_t* data;

	if (!cursor->ok || cursor->size - cursor->at < size) {
		cursor->ok = 0;
		return NULL;
	}
	data = cursor->data + cursor->at;
	cursor->at += size;
	return data;
}

static void FilmState_PutU8(FilmStateCursor* cursor, uint8_t value) {
	uint8_t* data = FilmState_Take(cursor, 1);

	if (data != NULL) {
		*data = value;
	}
}

static void FilmState_PutU16(FilmStateCursor* cursor, uint16_t value) {
	uint8_t* data = FilmState_Take(cursor, 2);

	if (data != NULL) {
		ByteOrder_WriteU16Le(data, value);
	}
}

static void FilmState_PutI32(FilmStateCursor* cursor, int32_t value) {
	uint8_t* data = FilmState_Take(cursor, 4);

	if (data != NULL) {
		ByteOrder_WriteU32Le(data, (uint32_t)value);
	}
}

static void FilmState_PutF64(FilmStateCursor* cursor, double value) {
	uint8_t* data = FilmState_Take(cursor, 8);
	uint64_t bits;

	if (data != NULL) {
		memcpy(&bits, &value, sizeof bits);
		ByteOrder_WriteU32Le(data, (uint32_t)bits);
		ByteOrder_WriteU32Le(data + 4, (uint32_t)(bits >> 32));
	}
}

static void FilmState_PutBytes(FilmStateCursor* cursor, const void* bytes, size_t size) {
	uint8_t* data = FilmState_Take(cursor, size);

	if (data != NULL) {
		memcpy(data, bytes, size);
	}
}

static uint8_t FilmState_GetU8(FilmStateCursor* cursor) {
	const uint8_t* data = FilmState_Take(cursor, 1);

	return data != NULL ? *data : 0;
}

static uint16_t FilmState_GetU16(FilmStateCursor* cursor) {
	const uint8_t* data = FilmState_Take(cursor, 2);

	return data != NULL ? ByteOrder_ReadU16Le(data) : 0;
}

static int32_t FilmState_GetI32(FilmStateCursor* cursor) {
	const uint8_t* data = FilmState_Take(cursor, 4);

	return data != NULL ? ByteOrder_ReadI32Le(data) : 0;
}

static double FilmState_GetF64(FilmStateCursor* cursor) {
	const uint8_t* data = FilmState_Take(cursor, 8);
	uint64_t bits;
	double value = 0.0;

	if (data != NULL) {
		bits = (uint64_t)ByteOrder_ReadU32Le(data) | (uint64_t)ByteOrder_ReadU32Le(data + 4) << 32;
		memcpy(&value, &bits, sizeof value);
	}
	return value;
}

static void FilmState_GetBytes(FilmStateCursor* cursor, void* bytes, size_t size) {
	const uint8_t* data = FilmState_Take(cursor, size);

	if (data != NULL) {
		memcpy(bytes, data, size);
	} else {
		memset(bytes, 0, size);
	}
}

void XwaFilmState_Configure(const char* modFolder) {
	const char* name = modFolder;
	const char* p;
	size_t length;

	for (p = modFolder; *p != '\0'; ++p) {
		if ((*p == '/' || *p == '\\') && p[1] != '\0') {
			name = p + 1;
		}
	}
	length = strcspn(name, "/\\");
	if (length > FILM_STATE_MOD_NAME_MAX) {
		length = FILM_STATE_MOD_NAME_MAX;
	}
	memcpy(s_modName, name, length);
	s_modName[length] = '\0';
}

void XwaFilmState_Write(void) {
	static uint8_t buffer[4 + FILM_STATE_MAX_SIZE];
	FilmStateCursor cursor = { buffer, sizeof buffer, 4, 1 };
	int32_t shieldDelays[XWA_SHIELD_RECHARGE_FLIGHT_GROUPS];
	static int32_t warheadPools[XWA_WARHEAD_POOLS_FLIGHT_GROUPS][XWA_WARHEAD_POOLS_TYPES];
	int32_t lastTargets[XWA_PLAYER_COUNT];
	XwaHangarsFlightState hangars;
	XwaHangarSceneFlightState scene;
	DeathStarModernFlightState deathStar;
	size_t length = strlen(s_modName);
	int i;
	int j;

	XwaShieldRecharge_SaveFlightState(shieldDelays);
	XwaWarheadPools_SaveFlightState(warheadPools);
	XwaMissionSettings_SaveFlightState(lastTargets);
	XwaHangars_SaveFlightState(&hangars);
	XwaHangarScene_SaveFlightState(&scene);
	DeathStar_ModernSaveFlightState(&deathStar);

	FilmState_PutU16(&cursor, FILM_STATE_VERSION);
	FilmState_PutU16(&cursor, (uint16_t)XwaModernFlightTiming_StepTicks());
	FilmState_PutU8(&cursor, (uint8_t)(XwaModelSource_ReplacementsEnabled() != 0));
	FilmState_PutU8(&cursor, (uint8_t)length);
	FilmState_PutBytes(&cursor, s_modName, length);
	for (i = 0; i < XWA_SHIELD_RECHARGE_FLIGHT_GROUPS; ++i) {
		FilmState_PutI32(&cursor, shieldDelays[i]);
	}
	for (i = 0; i < XWA_WARHEAD_POOLS_FLIGHT_GROUPS; ++i) {
		for (j = 0; j < XWA_WARHEAD_POOLS_TYPES; ++j) {
			FilmState_PutI32(&cursor, warheadPools[i][j]);
		}
	}
	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		FilmState_PutI32(&cursor, lastTargets[i]);
	}
	FilmState_PutU8(&cursor, hangars.resolved);
	FilmState_PutU8(&cursor, hangars.inHangar);
	FilmState_PutU8(&cursor, hangars.provingGround);
	FilmState_PutU8(&cursor, hangars.shipMarkings);
	FilmState_PutI32(&cursor, hangars.shipIff);
	FilmState_PutBytes(&cursor, hangars.shipKey, sizeof hangars.shipKey);
	FilmState_PutI32(&cursor, scene.launchRefOffset[0]);
	FilmState_PutI32(&cursor, scene.launchRefOffset[1]);
	FilmState_PutI32(&cursor, scene.reentryStartY);
	FilmState_PutU8(&cursor, scene.shuttleLoaded);
	for (i = 0; i < 3; ++i) {
		FilmState_PutI32(&cursor, deathStar.reactorRandomDrift[i]);
	}
	for (i = 0; i < 3; ++i) {
		FilmState_PutI32(&cursor, deathStar.reactorRandomDriftRemainder[i]);
	}
	FilmState_PutI32(&cursor, deathStar.reactorCylinderYawRemainder);
	FilmState_PutI32(&cursor, deathStar.reactorCoreDriftSpeedRemainder);
	FilmState_PutI32(&cursor, deathStar.reactorCorePitchRemainder);
	for (i = 0; i < 3; ++i) {
		FilmState_PutF64(&cursor, deathStar.reactorCoreDirectedDriftRemainder[i]);
	}
	FilmState_PutU16(&cursor, deathStar.shockwaveLightValue);

	ByteOrder_WriteU32Le(buffer, (uint32_t)(cursor.at - 4));
	Film_WriteBytesBuffered(buffer, cursor.at);
}

void XwaFilmState_Read(void) {
	static uint8_t buffer[FILM_STATE_MAX_SIZE];
	uint8_t sizeBytes[4];
	FilmStateCursor cursor = { buffer, 0, 0, 1 };
	int32_t shieldDelays[XWA_SHIELD_RECHARGE_FLIGHT_GROUPS];
	static int32_t warheadPools[XWA_WARHEAD_POOLS_FLIGHT_GROUPS][XWA_WARHEAD_POOLS_TYPES];
	int32_t lastTargets[XWA_PLAYER_COUNT];
	XwaHangarsFlightState hangars;
	XwaHangarSceneFlightState scene;
	DeathStarModernFlightState deathStar;
	char modName[FILM_STATE_MOD_NAME_MAX + 1];
	uint32_t size;
	uint32_t left;
	int stepTicks;
	int replacements;
	size_t length;
	int i;
	int j;

	if (!Film_ReadBytes(sizeBytes, sizeof sizeBytes)) {
		return;
	}
	size = ByteOrder_ReadU32Le(sizeBytes);
	cursor.size = size < sizeof buffer ? size : sizeof buffer;
	if (!Film_ReadBytes(buffer, (int)cursor.size)) {
		return;
	}
	/* The rest of a larger block, from a newer version, is skipped. */
	for (left = size - (uint32_t)cursor.size; left > 0;) {
		uint8_t skip[256];
		const uint32_t chunk = left < sizeof skip ? left : (uint32_t)sizeof skip;

		if (!Film_ReadBytes(skip, (int)chunk)) {
			return;
		}
		left -= chunk;
	}
	if (FilmState_GetU16(&cursor) != FILM_STATE_VERSION || !cursor.ok) {
		Aeron_LogWarn("xwa.film", "film: the port's state block has another version; it is ignored");
		return;
	}

	stepTicks = FilmState_GetU16(&cursor);
	replacements = FilmState_GetU8(&cursor);
	length = FilmState_GetU8(&cursor);
	FilmState_GetBytes(&cursor, modName, length);
	modName[length] = '\0';
	for (i = 0; i < XWA_SHIELD_RECHARGE_FLIGHT_GROUPS; ++i) {
		shieldDelays[i] = FilmState_GetI32(&cursor);
	}
	for (i = 0; i < XWA_WARHEAD_POOLS_FLIGHT_GROUPS; ++i) {
		for (j = 0; j < XWA_WARHEAD_POOLS_TYPES; ++j) {
			warheadPools[i][j] = FilmState_GetI32(&cursor);
		}
	}
	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		lastTargets[i] = FilmState_GetI32(&cursor);
	}
	hangars.resolved = FilmState_GetU8(&cursor);
	hangars.inHangar = FilmState_GetU8(&cursor);
	hangars.provingGround = FilmState_GetU8(&cursor);
	hangars.shipMarkings = FilmState_GetU8(&cursor);
	hangars.shipIff = FilmState_GetI32(&cursor);
	FilmState_GetBytes(&cursor, hangars.shipKey, sizeof hangars.shipKey);
	scene.launchRefOffset[0] = FilmState_GetI32(&cursor);
	scene.launchRefOffset[1] = FilmState_GetI32(&cursor);
	scene.reentryStartY = FilmState_GetI32(&cursor);
	scene.shuttleLoaded = FilmState_GetU8(&cursor);
	for (i = 0; i < 3; ++i) {
		deathStar.reactorRandomDrift[i] = FilmState_GetI32(&cursor);
	}
	for (i = 0; i < 3; ++i) {
		deathStar.reactorRandomDriftRemainder[i] = FilmState_GetI32(&cursor);
	}
	deathStar.reactorCylinderYawRemainder = FilmState_GetI32(&cursor);
	deathStar.reactorCoreDriftSpeedRemainder = FilmState_GetI32(&cursor);
	deathStar.reactorCorePitchRemainder = FilmState_GetI32(&cursor);
	for (i = 0; i < 3; ++i) {
		deathStar.reactorCoreDirectedDriftRemainder[i] = FilmState_GetF64(&cursor);
	}
	deathStar.shockwaveLightValue = FilmState_GetU16(&cursor);
	if (!cursor.ok) {
		Aeron_LogWarn("xwa.film", "film: the port's state block is too short; it is ignored");
		return;
	}

	if (strcmp(modName, s_modName) != 0) {
		Aeron_LogWarn("xwa.film",
					  "film: recorded with the mod '%s', played with '%s'; it can play differently", modName,
					  s_modName);
	}
	if (replacements != (XwaModelSource_ReplacementsEnabled() != 0)) {
		Aeron_LogWarn("xwa.film", "film: recorded with the model replacements %s; it can play differently",
					  replacements ? "on" : "off");
	}
	XwaModernFlightTiming_UseFilmStepTicks(stepTicks);
	XwaShieldRecharge_RestoreFlightState(shieldDelays);
	XwaWarheadPools_RestoreFlightState(warheadPools);
	XwaMissionSettings_RestoreFlightState(lastTargets);
	XwaHangars_RestoreFlightState(&hangars);
	XwaHangarScene_RestoreFlightState(&scene);
	DeathStar_ModernRestoreFlightState(&deathStar);
}

/* The table's layout: its slots, and the slots of its craft, projectile,
   player projectile and explosion blocks. */
enum { FILM_LAYOUT_VALUES = 5 };

static void FilmState_Layout(uint32_t values[FILM_LAYOUT_VALUES]) {
	values[0] = g_objectTableSlotCount;
	values[1] = g_craftObjectSlotsTotal;
	values[2] = g_projectileObjectSlotsTotal;
	values[3] = g_playerProjectileSlotsTotal;
	values[4] = g_explosionObjectSlotsTotal;
}

void XwaFilmState_WriteLayout(void) {
	uint8_t bytes[4 * FILM_LAYOUT_VALUES];
	uint32_t values[FILM_LAYOUT_VALUES];
	int i;

	FilmState_Layout(values);
	for (i = 0; i < FILM_LAYOUT_VALUES; ++i) {
		ByteOrder_WriteU32Le(bytes + 4 * i, values[i]);
	}
	Film_WriteBytesBuffered(bytes, sizeof bytes);
}

int XwaFilmState_ReadLayout(int filmHasLayout) {
	uint8_t bytes[4 * FILM_LAYOUT_VALUES];
	uint32_t values[FILM_LAYOUT_VALUES];
	int i;

	FilmState_Layout(values);
	if (!filmHasLayout) {
		/* Older films have the stock layout. */
		if (g_craftObjectSlotsPerRegion == (g_provingGroundsModeActive ? 36u : 96u) &&
			g_playerProjectileSlotsTotal == 12u * (uint32_t)g_flightPlayerCount &&
			g_projectileObjectSlotsPerRegion ==
				g_sharedPlayerProjectileSlotsPerRegion + g_playerProjectileSlotsTotal + 64u &&
			g_explosionObjectSlotsTotal == 32u * (uint32_t)g_missionRegionCount) {
			return 1;
		}
		Aeron_LogError("xwa.film",
					   "film: recorded with the stock object counts, and the mod changes them; it "
					   "can't play");
		return 0;
	}
	if (!Film_ReadBytes(bytes, sizeof bytes)) {
		return 0;
	}
	for (i = 0; i < FILM_LAYOUT_VALUES; ++i) {
		if (ByteOrder_ReadU32Le(bytes + 4 * i) != values[i]) {
			Aeron_LogError(
				"xwa.film",
				"film: recorded with an object table of %u slots, and this one has %u, or its blocks "
				"differ; it can't play",
				ByteOrder_ReadU32Le(bytes), values[0]);
			return 0;
		}
	}
	return 1;
}
