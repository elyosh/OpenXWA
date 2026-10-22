#include "xwa_runtime/models/craft_sounds.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/audio/fsfx.h"
#include "xwa/audio/sound.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/models/hangars.h"
#include "xwa_runtime/models/mission_settings.h"
#include "xwa_runtime/models/model_settings.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* The weapon types of `weapon_ranges`: 280 to 307. */
#define CRAFT_SOUNDS_FIRST_WEAPON 280
#define CRAFT_SOUNDS_WEAPONS 28

enum { CRAFT_SOUNDS_FLY_BY_BOTH, CRAFT_SOUNDS_FLY_BY_2D, CRAFT_SOUNDS_FLY_BY_3D };

enum {
	CRAFT_SOUNDS_PLAIN,
	CRAFT_SOUNDS_CORELLIAN_TRANSPORT,
	CRAFT_SOUNDS_TIE_FIGHTER,
	CRAFT_SOUNDS_TIE_FIGHTER_WITH_ION,
	CRAFT_SOUNDS_REBEL,
	CRAFT_SOUNDS_REBEL_TURBO,
	CRAFT_SOUNDS_EMPIRE,
	CRAFT_SOUNDS_EMPIRE_TURBO,
	CRAFT_SOUNDS_FALCON,
	CRAFT_SOUNDS_FALCON_TURBO,
	CRAFT_SOUNDS_ION,
	CRAFT_SOUNDS_ION_TURBO,
	CRAFT_SOUNDS_BEHAVIORS
};

static const char* const s_engineNames[] = { "xwing",   "ywing",     "awing",  "tie",
											 "gunboat", "corellian", "falcon", "slave" };
static const char* const s_washNames[] = { "normal", "starship" };
static const char* const s_takeOffNames[] = { "gear", "falcon" };
static const char* const s_hyperNames[] = { "rebel", "imperial", "imperial_zoom" };
static const char* const s_flyByModeNames[] = { "2d", "3d" };
static const char* const s_behaviorNames[CRAFT_SOUNDS_BEHAVIORS] = {
	"plain",  "corellian_transport", "tie_fighter", "tie_fighter_with_ion", "rebel", "rebel_turbo",
	"empire", "empire_turbo",        "falcon",      "falcon_turbo",         "ion",   "ion_turbo"
};

/* The cockpit engine slot and base frequency for each name, from xwing to
   falcon. */
static const int s_interiorSlots[] = { 94, 95, 96, 97, 98, 99, 100 };
static const int s_interiorFrequencies[] = { 11000, 11000, 5500, 11000, 11000, 11000, 11000 };
/* The 2D fly-by slot for each name, from xwing to slave. The 3D slot is the
   next one. */
static const int s_flyBySlots[] = { 105, 107, 109, 101, 103, 111, 111, 129 };

/* The `sound` group of a model file. 0 means not set. A name is stored as its
   index in its name table plus 1. */
typedef struct CraftSoundsSet {
	uint8_t interior;
	uint8_t flyBy;
	uint8_t flyByMode;
	uint8_t wash;
	uint8_t takeOff;
	uint8_t hyper;
	uint8_t weapons;
	struct {
		uint16_t first;
		uint16_t count; /* 0 for no range */
	} ranges[CRAFT_SOUNDS_WEAPONS];
	/* The centre and radii of the ambient sound, in metres, for each axis:
	   right, up and forward. */
	int32_t ambientOffset[3];
	uint8_t hasAmbientDistance[3];
	int32_t ambientDistance[3];
} CraftSoundsSet;

static CraftSoundsSet s_models[OBJ_Count];
static int s_washSlot = -1;
static int s_mapEnter;
/* The command ship type that the hangar ambience last used, for the log. */
static int s_hangarAmbientType = -2;
/* The sound effect ids of the ambient sounds that the update started, and
   the nearest distance of each sound in the current update. */
static int s_ambientIds[XWA_SOUND_EFFECT_COUNT];
static int s_ambientIdCount;
static float s_ambientNearest[XWA_SOUND_EFFECT_COUNT];
/* The module's random numbers. */
static uint32_t s_random = 1;
/* The explosion sounds used so far for each object type. As in the game's
   picker, there are 8 sounds and none repeats until 4 have played. */
static int s_explosionLeft[OBJ_Count];
static uint8_t s_explosionUsed[OBJ_Count][8];

static unsigned int CraftSounds_Random(void) {
	s_random = s_random * 1103515245u + 12345u;
	return s_random >> 16;
}

/* Looks up the value of `node` in `names`, and writes its index plus 1 into
   `out`. Logs a value that isn't in the list. */
static void CraftSounds_ReadName(const char* path, const char* key, const AeronConfigNode* node,
								 const char* const* names, size_t count, uint8_t* out) {
	const char* value = AeronConfigNode_String(node, "");
	size_t i;

	if (node == NULL) {
		return;
	}
	for (i = 0; i < count; ++i) {
		if (AeronConfigNode_Type(node) == AERON_CONFIG_STRING && strcmp(value, names[i]) == 0) {
			*out = (uint8_t)(i + 1);
			return;
		}
	}
	Aeron_LogWarn("xwa.models", "%s:%d: sound.%s: '%s' isn't one of its values. It is ignored", path,
				  AeronConfigNode_Line(node), key, value);
}

static void CraftSounds_ReadRanges(const char* path, const AeronConfigNode* node, CraftSoundsSet* set) {
	size_t i;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: sound.weapon_ranges isn't a map. It is ignored", path,
					  AeronConfigNode_Line(node));
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(node); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(node, i);
		const AeronConfigNode* range = AeronConfigNode_MapValueAt(node, i);
		const AeronConfigNode* first = AeronConfigNode_MapGet(range, "first");
		const AeronConfigNode* count = AeronConfigNode_MapGet(range, "count");
		char* end;
		const long weapon = strtol(key, &end, 10) - CRAFT_SOUNDS_FIRST_WEAPON;

		if (*key == '\0' || *end != '\0' || weapon < 0 || weapon >= CRAFT_SOUNDS_WEAPONS ||
			AeronConfigNode_Type(first) != AERON_CONFIG_INT ||
			AeronConfigNode_Type(count) != AERON_CONFIG_INT || AeronConfigNode_Int(first, -1) < 0 ||
			AeronConfigNode_Int(first, -1) > 0xffff || AeronConfigNode_Int(count, -1) < 1 ||
			AeronConfigNode_Int(count, -1) > 0xffff) {
			Aeron_LogWarn(
				"xwa.models",
				"%s:%d: sound.weapon_ranges: '%s' needs a weapon type from 280 to 307, a first sound "
				"and a count of 1 or more. It is ignored",
				path, AeronConfigNode_Line(range), key);
			continue;
		}
		set->ranges[weapon].first = (uint16_t)AeronConfigNode_Int(first, 0);
		set->ranges[weapon].count = (uint16_t)AeronConfigNode_Int(count, 0);
	}
}

void XwaCraftSounds_ClearModelSettings(void) {
	memset(s_models, 0, sizeof s_models);
	memset(s_explosionLeft, 0, sizeof s_explosionLeft);
	s_washSlot = -1;
	s_mapEnter = 0;
	s_ambientIdCount = 0;
	s_hangarAmbientType = -2;
}

/* Reads a list of three values in metres into `out`. When `set` isn't NULL,
   the node can also be a single number for all three, and a null in the list
   leaves that axis unset; `set` records which axes are set. Logs and returns
   0 if the node has another form. */
static int CraftSounds_ReadAxes(const char* path, const char* key, const AeronConfigNode* node,
								int32_t out[3], uint8_t set[3]) {
	size_t i;

	if (AeronConfigNode_Type(node) == AERON_CONFIG_INT && set != NULL) {
		for (i = 0; i < 3; ++i) {
			out[i] = (int32_t)AeronConfigNode_Int(node, 0);
			set[i] = 1;
		}
		return 1;
	}
	if (AeronConfigNode_Type(node) == AERON_CONFIG_SEQUENCE && AeronConfigNode_SequenceCount(node) == 3) {
		for (i = 0; i < 3; ++i) {
			const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, i);

			if (AeronConfigNode_Type(item) == AERON_CONFIG_INT) {
				out[i] = (int32_t)AeronConfigNode_Int(item, 0);
				if (set != NULL) {
					set[i] = 1;
				}
			} else if (AeronConfigNode_Type(item) != AERON_CONFIG_NULL || set == NULL) {
				break;
			}
		}
		if (i == 3) {
			return 1;
		}
	}
	Aeron_LogWarn("xwa.models", "%s:%d: sound.ambient.%s needs %s. It is ignored", path,
				  AeronConfigNode_Line(node), key,
				  set != NULL ? "a number or a list of 3 numbers or nulls" : "a list of 3 numbers");
	memset(out, 0, 3 * sizeof *out);
	if (set != NULL) {
		memset(set, 0, 3);
	}
	return 0;
}

void XwaCraftSounds_ReadModelSettings(unsigned int objectType, const char* path,
									  const AeronConfigNode* group) {
	CraftSoundsSet* set;
	const AeronConfigNode* node;

	if (objectType >= OBJ_Count) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: sound isn't a map. It is ignored", path,
					  AeronConfigNode_Line(group));
		return;
	}
	set = &s_models[objectType];
	CraftSounds_ReadName(path, "interior", AeronConfigNode_MapGet(group, "interior"), s_engineNames, 7,
						 &set->interior);
	CraftSounds_ReadName(path, "fly_by", AeronConfigNode_MapGet(group, "fly_by"), s_engineNames, 8,
						 &set->flyBy);
	CraftSounds_ReadName(path, "fly_by_mode", AeronConfigNode_MapGet(group, "fly_by_mode"), s_flyByModeNames,
						 2, &set->flyByMode);
	CraftSounds_ReadName(path, "engine_wash", AeronConfigNode_MapGet(group, "engine_wash"), s_washNames, 2,
						 &set->wash);
	CraftSounds_ReadName(path, "take_off", AeronConfigNode_MapGet(group, "take_off"), s_takeOffNames, 2,
						 &set->takeOff);
	CraftSounds_ReadName(path, "hyper", AeronConfigNode_MapGet(group, "hyper"), s_hyperNames, 3, &set->hyper);
	CraftSounds_ReadName(path, "weapons", AeronConfigNode_MapGet(group, "weapons"), s_behaviorNames,
						 CRAFT_SOUNDS_BEHAVIORS, &set->weapons);
	if ((node = AeronConfigNode_MapGet(group, "weapon_ranges")) != NULL) {
		CraftSounds_ReadRanges(path, node, set);
	}
	if ((node = AeronConfigNode_MapGet(group, "ambient")) != NULL) {
		const AeronConfigNode* axes;

		if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s:%d: sound.ambient isn't a map. It is ignored", path,
						  AeronConfigNode_Line(node));
		} else {
			if ((axes = AeronConfigNode_MapGet(node, "offset_m")) != NULL) {
				CraftSounds_ReadAxes(path, "offset_m", axes, set->ambientOffset, NULL);
			}
			if ((axes = AeronConfigNode_MapGet(node, "distance_m")) != NULL) {
				CraftSounds_ReadAxes(path, "distance_m", axes, set->ambientDistance, set->hasAmbientDistance);
			}
		}
	}
}

static const CraftSoundsSet* CraftSounds_Model(unsigned int objectType) {
	XwaModelSettings_Ensure(objectType);
	return objectType < OBJ_Count ? &s_models[objectType] : NULL;
}

void XwaCraftSounds_Interior(unsigned int objectType, int* engineSlot, int* baseFrequency,
							 uint8_t* useHighQualityStep) {
	const CraftSoundsSet* set = CraftSounds_Model(objectType);

	if (set != NULL && set->interior != 0) {
		*engineSlot = s_interiorSlots[set->interior - 1];
		*baseFrequency = s_interiorFrequencies[set->interior - 1];
	} else if (XwaSoundBanks_Count(XWA_SOUND_BANK_INTERIOR) != 0) {
		*engineSlot = XwaSoundBanks_Slot(XWA_SOUND_BANK_INTERIOR, objectType);
		*baseFrequency = 11000;
	} else {
		return;
	}
	*useHighQualityStep = *baseFrequency == 11000 && g_gameConfig.sfxQuality;
}

int XwaCraftSounds_FlyBy(unsigned int objectType, int stockSlot) {
	const CraftSoundsSet* set = CraftSounds_Model(objectType);
	int slot;

	if (set != NULL && set->flyBy != 0) {
		slot = s_flyBySlots[set->flyBy - 1];
		switch (set->flyByMode) {
			case CRAFT_SOUNDS_FLY_BY_2D:
				return slot;
			case CRAFT_SOUNDS_FLY_BY_3D:
				return slot + 1;
			default:
				return g_sound3DEnabled && g_sfxIds[slot + 1] != -1 ? slot + 1 : slot;
		}
	}
	if (XwaSoundBanks_Count(XWA_SOUND_BANK_FLY_BY) != 0) {
		return XwaSoundBanks_Slot(XWA_SOUND_BANK_FLY_BY, objectType);
	}
	return stockSlot;
}

int XwaCraftSounds_Wash(unsigned int objectType, int stockSlot) {
	const CraftSoundsSet* set = CraftSounds_Model(objectType);
	int slot;

	if ((set == NULL || set->wash == 0) && XwaSoundBanks_Count(XWA_SOUND_BANK_ENGINE_WASH) != 0 &&
		(slot = XwaSoundBanks_Slot(XWA_SOUND_BANK_ENGINE_WASH, objectType)) != -1) {
		return slot;
	}
	if (set != NULL && set->wash != 0) {
		return set->wash == 2 ? SFX_ENGINE_WASH_CAPITAL : SFX_ENGINE_WASH_OTHER;
	}
	/* The type is past the bank's count: play the normal wash. */
	return XwaSoundBanks_Count(XWA_SOUND_BANK_ENGINE_WASH) != 0 ? SFX_ENGINE_WASH_OTHER : stockSlot;
}

int XwaCraftSounds_TakeOff(unsigned int objectType, int stockSlot) {
	const CraftSoundsSet* set = CraftSounds_Model(objectType);
	int slot;

	if ((set == NULL || set->takeOff == 0) && XwaSoundBanks_Count(XWA_SOUND_BANK_TAKE_OFF) != 0 &&
		(slot = XwaSoundBanks_Slot(XWA_SOUND_BANK_TAKE_OFF, objectType)) != -1) {
		return slot;
	}
	if (set != NULL && set->takeOff != 0) {
		return set->takeOff == 2 ? 139 : 132;
	}
	/* The type is past the bank's count: play the landing gear sound. */
	return XwaSoundBanks_Count(XWA_SOUND_BANK_TAKE_OFF) != 0 ? 132 : stockSlot;
}

int XwaCraftSounds_Hangar(XwaSoundBank bank, unsigned int objIdx, int stockSlot) {
	int slot;

	if (objIdx == 0xffffu || (slot = XwaSoundBanks_Slot(bank, g_objectTable[objIdx].objectType)) == -1) {
		return stockSlot;
	}
	return slot;
}

int XwaCraftSounds_Explosion(unsigned int objIdx, int stockSlot) {
	const MobileObject* mobj;
	unsigned int objectType;
	int slot;
	int choice;

	if (objIdx == 0xffffu || (mobj = g_objectTable[objIdx].mobj) == NULL ||
		(objectType = mobj->sourceObjectType) == 0 || objectType >= OBJ_Count ||
		(unsigned int)XwaSoundBanks_Count(XWA_SOUND_BANK_WEAPON_EXPLOSION) / 8 <= objectType) {
		return stockSlot;
	}
	if (s_explosionLeft[objectType] <= 0) {
		s_explosionLeft[objectType] = 4;
		memset(s_explosionUsed[objectType], 0, sizeof s_explosionUsed[objectType]);
	}
	do {
		choice = (int)(CraftSounds_Random() & 7);
	} while (s_explosionUsed[objectType][choice] != 0);
	s_explosionUsed[objectType][choice] = 1;
	--s_explosionLeft[objectType];
	slot = XwaSoundBanks_Slot(XWA_SOUND_BANK_WEAPON_EXPLOSION, objectType * 8 + (unsigned int)choice);
	return slot != -1 ? slot : stockSlot;
}

int XwaCraftSounds_Interface(XwaSoundBankSound sound, int stockSlot) {
	const int slot = XwaSoundBanks_Slot(XWA_SOUND_BANK_SOUNDS, sound);

	return slot != -1 ? slot : stockSlot;
}

/* Returns the game's sound slot for weapon type `weapon` with behaviour
   `behavior`, or -1 for no sound. */
static int CraftSounds_BehaviorSlot(int behavior, unsigned int weapon) {
	switch (weapon) {
		case OBJ_LaserRebel:
		case OBJ_LaserRebelTurbo:
		case OBJ_LaserImperial:
		case OBJ_LaserImperialTurbo:
		case OBJ_LaserIon:
		case OBJ_LaserIonTurbo: {
			const int rebel = weapon == OBJ_LaserRebel || weapon == OBJ_LaserRebelTurbo;
			const int imperial = weapon == OBJ_LaserImperial || weapon == OBJ_LaserImperialTurbo;
			const int ion = weapon == OBJ_LaserIon || weapon == OBJ_LaserIonTurbo;

			switch (behavior) {
				case CRAFT_SOUNDS_CORELLIAN_TRANSPORT:
					if (rebel) {
						return weapon == OBJ_LaserRebel ? 15 : 16;
					}
					break;
				case CRAFT_SOUNDS_TIE_FIGHTER:
					return 20;
				case CRAFT_SOUNDS_TIE_FIGHTER_WITH_ION:
					if (!ion) {
						return 20;
					}
					break;
				case CRAFT_SOUNDS_REBEL:
				case CRAFT_SOUNDS_REBEL_TURBO:
					if (rebel) {
						return behavior == CRAFT_SOUNDS_REBEL ? 4 : 5;
					}
					break;
				case CRAFT_SOUNDS_EMPIRE:
				case CRAFT_SOUNDS_EMPIRE_TURBO:
					if (imperial) {
						return behavior == CRAFT_SOUNDS_EMPIRE ? 6 : 7;
					}
					break;
				case CRAFT_SOUNDS_FALCON:
				case CRAFT_SOUNDS_FALCON_TURBO:
					if (!ion) {
						return behavior == CRAFT_SOUNDS_FALCON ? 15 : 16;
					}
					break;
				case CRAFT_SOUNDS_ION:
				case CRAFT_SOUNDS_ION_TURBO:
					if (ion) {
						return behavior == CRAFT_SOUNDS_ION ? 8 : 9;
					}
					break;
			}
			return 4 + (int)weapon - OBJ_LaserRebel;
		}
		case 286:
		case 291:
			return 10;
		case 287:
		case 292:
			return 11;
		case 288:
		case 297:
		case 301:
		case 302:
			return 12;
		case 289:
		case 303:
		case 304:
		case 305:
			return 13;
		case 290:
			return 14;
		case 293:
			return 17;
		case 294:
			return 18;
		case 295:
		case 296:
		case 298:
			return 19;
		case 307:
			switch (behavior) {
				case CRAFT_SOUNDS_REBEL:
					return 4;
				case CRAFT_SOUNDS_REBEL_TURBO:
					return 5;
				case CRAFT_SOUNDS_EMPIRE:
					return 6;
				case CRAFT_SOUNDS_EMPIRE_TURBO:
					return 7;
				case CRAFT_SOUNDS_FALCON:
					return 15;
				case CRAFT_SOUNDS_FALCON_TURBO:
					return 16;
				case CRAFT_SOUNDS_ION:
					return 8;
				case CRAFT_SOUNDS_ION_TURBO:
					return 9;
				case CRAFT_SOUNDS_TIE_FIGHTER:
				case CRAFT_SOUNDS_TIE_FIGHTER_WITH_ION:
					return 20;
			}
			return -1;
	}
	return -1;
}

/* Returns which of an object type's 10 weapon bank entries weapon type
   `weapon` uses, or -1 if it uses none. */
static int CraftSounds_WeaponEntry(unsigned int weapon) {
	switch (weapon) {
		case 280:
		case 281:
		case 282:
		case 283:
		case 284:
		case 285:
			return (int)weapon - 280;
		case 288:
		case 297:
		case 301:
		case 302:
			return 6;
		case 289:
		case 303:
		case 304:
		case 305:
			return 7;
		case 290:
			return 8;
		case 307:
			return 9;
	}
	return -1;
}

int XwaCraftSounds_Weapon(unsigned int projectileObjIdx, int* sfxSlot) {
	const ObjectRecord* projectile = &g_objectTable[projectileObjIdx];
	const unsigned int weapon = projectile->objectType;
	unsigned int shooter;
	const CraftSoundsSet* set;
	int entry;

	if (projectile->mobj == NULL) {
		return 0;
	}
	shooter = projectile->mobj->sourceObjectType;
	if (shooter == 0 && projectile->mobj->sourceObjIdx > 0) {
		shooter = g_objectTable[projectile->mobj->sourceObjIdx].objectType;
	}
	set = CraftSounds_Model(shooter);
	if (set != NULL && set->weapons != 0) {
		*sfxSlot = CraftSounds_BehaviorSlot(set->weapons - 1, weapon);
		return 1;
	}
	if (set != NULL && weapon >= CRAFT_SOUNDS_FIRST_WEAPON &&
		weapon < CRAFT_SOUNDS_FIRST_WEAPON + CRAFT_SOUNDS_WEAPONS) {
		const unsigned int first = set->ranges[weapon - CRAFT_SOUNDS_FIRST_WEAPON].first;
		const unsigned int count = set->ranges[weapon - CRAFT_SOUNDS_FIRST_WEAPON].count;

		if (count != 0 && first + count <= (unsigned int)XwaSoundBanks_Count(XWA_SOUND_BANK_WEAPON_RANGE)) {
			*sfxSlot = XwaSoundBanks_Slot(XWA_SOUND_BANK_WEAPON_RANGE, first + CraftSounds_Random() % count);
			return 1;
		}
	}
	entry = CraftSounds_WeaponEntry(weapon);
	if (entry == -1 || XwaSoundBanks_Count(XWA_SOUND_BANK_WEAPON) == 0) {
		return 0;
	}
	*sfxSlot = shooter < (unsigned int)XwaSoundBanks_Count(XWA_SOUND_BANK_WEAPON) / 10
				   ? XwaSoundBanks_Slot(XWA_SOUND_BANK_WEAPON, shooter * 10 + (unsigned int)entry)
				   : -1;
	return 1;
}

static const CraftSoundsSet* CraftSounds_PlayerModel(unsigned int playerIdx, unsigned int* objectType) {
	*objectType = g_objectTable[g_players[playerIdx].objectIndex].objectType;
	return CraftSounds_Model(*objectType);
}

int XwaCraftSounds_HyperStart(unsigned int playerIdx, int stockSlot) {
	unsigned int objectType;
	const CraftSoundsSet* set = CraftSounds_PlayerModel(playerIdx, &objectType);

	if (set != NULL && set->hyper != 0) {
		return set->hyper == 1 ? 118 : 115;
	}
	if (XwaSoundBanks_Count(XWA_SOUND_BANK_HYPER_START) != 0) {
		return XwaSoundBanks_Slot(XWA_SOUND_BANK_HYPER_START, objectType);
	}
	return stockSlot;
}

int XwaCraftSounds_HyperZoom(unsigned int playerIdx, int stockSlot) {
	unsigned int objectType;
	const CraftSoundsSet* set = CraftSounds_PlayerModel(playerIdx, &objectType);

	if (set != NULL && set->hyper != 0) {
		return set->hyper == 3 ? 116 : -1;
	}
	if (XwaSoundBanks_Count(XWA_SOUND_BANK_HYPER_ZOOM) != 0) {
		return XwaSoundBanks_Slot(XWA_SOUND_BANK_HYPER_ZOOM, objectType);
	}
	return stockSlot;
}

int XwaCraftSounds_HyperEnd(unsigned int playerIdx, int interdicted, int stockSlot) {
	const XwaSoundBank bank = interdicted ? XWA_SOUND_BANK_HYPER_ABORT : XWA_SOUND_BANK_HYPER_END;

	if (XwaSoundBanks_Count(bank) != 0) {
		return XwaSoundBanks_Slot(bank, g_objectTable[g_players[playerIdx].objectIndex].objectType);
	}
	return stockSlot;
}

int XwaCraftSounds_WashPlaying(int sfxSlot) {
	return Sound_CountPlayingInstances(g_sfxIds[sfxSlot]) != 0 ||
		   (g_sfxIds[sfxSlot] == -1 && sfxSlot == s_washSlot);
}

void XwaCraftSounds_WashStarted(int sfxSlot) { s_washSlot = sfxSlot; }

void XwaCraftSounds_StopWash(void) {
	if (s_washSlot >= XWA_SOUND_BANKS_FIRST_SLOT && Sound_CountPlayingInstances(g_sfxIds[s_washSlot]) != 0) {
		Sound_StopOldestInstance(g_sfxIds[s_washSlot]);
	}
	s_washSlot = -1;
}

void XwaCraftSounds_SetMapEnter(int mapEnter) { s_mapEnter = mapEnter; }

int XwaCraftSounds_TargetSelect(unsigned int playerIdx, int stockSlot) {
	if (s_mapEnter) {
		const int slot = XwaSoundBanks_Slot(XWA_SOUND_BANK_SOUNDS, XWA_SOUND_BANK_MAP_ENTER);

		if (slot != -1) {
			fsfx_PlaySound(slot, 0xffffu, playerIdx);
			return -1;
		}
	}
	return XwaCraftSounds_Interface(XWA_SOUND_BANK_TARGET_SELECT, stockSlot);
}

int XwaCraftSounds_HangarAmbient(unsigned int index) {
	const int objectType = XwaHangars_CommandShipType();
	const int count = XwaSoundBanks_Count(XWA_SOUND_BANK_HANGAR_AMBIENT);

	if (objectType != s_hangarAmbientType) {
		s_hangarAmbientType = objectType;
		if (objectType < 0) {
			Aeron_LogInfo("xwa.audio", "hangar ambience: no command ship, so the stock sounds play");
		} else if (objectType >= count / 10) {
			Aeron_LogInfo("xwa.audio",
						  "hangar ambience: command ship type %d has no bank entries, so the stock "
						  "sounds play",
						  objectType);
		} else {
			Aeron_LogInfo("xwa.audio", "hangar ambience: the bank entries of command ship type %d",
						  objectType);
		}
	}
	if (objectType >= 0 && objectType < count / 10) {
		return XwaSoundBanks_Slot(XWA_SOUND_BANK_HANGAR_AMBIENT, (unsigned int)objectType * 10 + index);
	}
	return 151 + (int)index;
}

/* Converts metres to game units (40.96 units per metre), rounding towards
   0. */
static int64_t CraftSounds_Units(int64_t metres) { return metres * 1024 / 25; }

/* Returns the squared distance from `player` to the ambient sound of
   `object`. The edge of the sound's ellipsoid is at 1. */
static float CraftSounds_AmbientDistance(const ObjectRecord* player, const ObjectRecord* object,
										 const CraftSoundsSet* set) {
	const MobileObject* mobj = object->mobj;
	/* The default radius is the object's size, in whole metres. */
	const int64_t size = (int64_t)g_objectTypes[object->objectType].maxBoundsExtent * 25 / 1024;
	const int64_t right = CraftSounds_Units(set->ambientOffset[0]);
	const int64_t up = CraftSounds_Units(set->ambientOffset[1]);
	const int64_t forward = CraftSounds_Units(set->ambientOffset[2]);
	const int64_t x = player->world_x - (object->world_x + mobj->cachedSideX * right / 32767 +
										 mobj->cachedUpX * up / 32767 + mobj->cachedFwdX * forward / 32767);
	const int64_t y = player->world_y - (object->world_y + mobj->cachedSideY * right / 32767 +
										 mobj->cachedUpY * up / 32767 + mobj->cachedFwdY * forward / 32767);
	const int64_t z = player->world_z - (object->world_z + mobj->cachedSideZ * right / 32767 +
										 mobj->cachedUpZ * up / 32767 + mobj->cachedFwdZ * forward / 32767);
	const int64_t along[3] = { (mobj->cachedSideX * x + mobj->cachedSideY * y + mobj->cachedSideZ * z) /
								   32767,
							   (mobj->cachedUpX * x + mobj->cachedUpY * y + mobj->cachedUpZ * z) / 32767,
							   (mobj->cachedFwdX * x + mobj->cachedFwdY * y + mobj->cachedFwdZ * z) / 32767 };
	float result = 0.0f;
	int axis;

	for (axis = 0; axis < 3; ++axis) {
		int64_t radius = set->hasAmbientDistance[axis] ? set->ambientDistance[axis] : size;
		float d;

		if (radius == 0) {
			radius = 1;
		}
		d = (float)along[axis] / (float)CraftSounds_Units(radius);
		result += d * d;
	}
	return result;
}

static void CraftSounds_StopAmbient(int soundId) {
	if (Sound_CountPlayingInstances(soundId) != 0) {
		Sound_StopOldestInstance(soundId);
	}
}

void XwaCraftSounds_UpdateAmbient(void) {
	const PlayerData* player = &g_players[g_localPlayer];
	const int count = XwaSoundBanks_Count(XWA_SOUND_BANK_STARSHIP_AMBIENT);
	const int playerObjIdx = player->mapCameraState ? player->altViewObjectIdx : player->objectIndex;
	uint32_t first;
	uint32_t end;
	uint32_t objIdx;
	int touched[XWA_SOUND_EFFECT_COUNT];
	int touchedCount = 0;
	int i;

	if (!XwaMissionSettings_StarshipAmbientSounds() || count == 0 || !g_flightConfSfxEnabled) {
		return;
	}
	if ((playerObjIdx & 0xffff) == 0xffff || g_inHangarReady || g_missionRegionCount <= 0) {
		for (i = 0; i < s_ambientIdCount; ++i) {
			CraftSounds_StopAmbient(s_ambientIds[i]);
		}
		s_ambientIdCount = 0;
		return;
	}

	/* The craft slots of the player's region. The formula is the one in
	   Mission_SetActiveRegionObjectRanges, which this code doesn't call
	   because it would change the active region. */
	first = player->regionIndex * (g_regionObjectSlotEnd / (uint32_t)g_missionRegionCount);
	end = first + g_craftObjectSlotsTotal / (uint32_t)g_missionRegionCount;
	for (objIdx = first; objIdx < end; ++objIdx) {
		const ObjectRecord* object = &g_objectTable[objIdx];
		const unsigned int objectType = object->objectType;
		int slot;
		int soundId;
		float distance;

		if (objectType == 0 || objectType >= (unsigned int)count || object->mobj == NULL ||
			object->regionIdx != player->regionIndex || object->playerOwnerIdx == g_localPlayer) {
			continue;
		}
		slot = XwaSoundBanks_Slot(XWA_SOUND_BANK_STARSHIP_AMBIENT, objectType);
		soundId = slot != -1 ? g_sfxIds[slot] : -1;
		if (soundId < 0 || soundId >= XWA_SOUND_EFFECT_COUNT) {
			continue;
		}
		distance =
			CraftSounds_AmbientDistance(&g_objectTable[playerObjIdx], object, CraftSounds_Model(objectType));
		for (i = 0; i < touchedCount && touched[i] != soundId; ++i) {
		}
		if (i == touchedCount) {
			touched[touchedCount++] = soundId;
			s_ambientNearest[soundId] = distance;
		} else if (distance < s_ambientNearest[soundId]) {
			s_ambientNearest[soundId] = distance;
		}
	}

	/* Each sound plays at the volume of its nearest craft. */
	for (i = 0; i < touchedCount; ++i) {
		const int soundId = touched[i];
		const float distance = s_ambientNearest[soundId];
		int known;

		if (distance >= 1.0f) {
			CraftSounds_StopAmbient(soundId);
			continue;
		}
		if (Sound_CountPlayingInstances(soundId) == 0) {
			Sound_QueueEffect(soundId, 1, 1, 127, 65, 64, -1, 0xffffu);
		}
		Sound_SetLatestInstanceVolume(soundId, (int)(126.0f * (1.0f - distance)) + 1);
		for (known = 0; known < s_ambientIdCount && s_ambientIds[known] != soundId; ++known) {
		}
		if (known == s_ambientIdCount) {
			s_ambientIds[s_ambientIdCount++] = soundId;
		}
	}

	/* Stop the sounds that no craft in the region uses any more. */
	for (i = 0; i < s_ambientIdCount; ++i) {
		int t;

		for (t = 0; t < touchedCount && touched[t] != s_ambientIds[i]; ++t) {
		}
		if (t == touchedCount) {
			CraftSounds_StopAmbient(s_ambientIds[i]);
		}
	}
}
