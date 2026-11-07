#include "xwa_runtime/audio/sound_banks.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/audio/fsfx.h"
#include "xwa/audio/sound.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/util/string.h"
#include "xwa_runtime/models/mod_files.h"
#include "xwa_runtime/models/names.h"
#include "xwa_runtime/models/units.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define SOUND_BANKS_YAML "flight/sounds.yaml"
#define SOUND_BANKS_SLOT_COUNT (XWA_SFX_SLOT_COUNT - XWA_SOUND_BANKS_FIRST_SLOT)

/* The keys of a bank's entries. */
typedef enum SoundBankLayout {
	/* Object type names. */
	SOUND_BANKS_BY_TYPE,
	/* Object type names, then weapon sound group names. */
	SOUND_BANKS_BY_WEAPON,
	/* Object type names. Each entry is a list of sounds. */
	SOUND_BANKS_LIST,
	/* The names in s_soundNames. */
	SOUND_BANKS_BY_SOUND,
	/* Names that the mod chooses. */
	SOUND_BANKS_BY_RANGE,
} SoundBankLayout;

typedef struct SoundBankInfo {
	const char* key;  /* the key in flight/sounds.yaml */
	const char* name; /* the prefix of its sound effect names */
	SoundBankLayout layout;
	/* The number of entries per object type, for SOUND_BANKS_BY_WEAPON and
	   SOUND_BANKS_LIST. */
	int perType;
} SoundBankInfo;

static const SoundBankInfo s_banks[XWA_SOUND_BANK_COUNT] = {
	{ "canopy_opening", "CanopyOpening", SOUND_BANKS_BY_TYPE, 1 },
	{ "engine_slowing", "EngineSlowing", SOUND_BANKS_BY_TYPE, 1 },
	{ "engine_wash", "EngineWash", SOUND_BANKS_BY_TYPE, 1 },
	{ "fly_by", "FlyBy", SOUND_BANKS_BY_TYPE, 1 },
	{ "hangar_ambient", "HangarAmbient", SOUND_BANKS_LIST, XWA_SOUND_BANKS_HANGAR_AMBIENT_MAX },
	{ "hyper_abort", "HyperAbort", SOUND_BANKS_BY_TYPE, 1 },
	{ "hyper_end", "HyperEnd", SOUND_BANKS_BY_TYPE, 1 },
	{ "hyper_start", "HyperStart", SOUND_BANKS_BY_TYPE, 1 },
	{ "hyper_zoom", "HyperZoom", SOUND_BANKS_BY_TYPE, 1 },
	{ "interior", "Interior", SOUND_BANKS_BY_TYPE, 1 },
	{ "s_foil", "SFoil", SOUND_BANKS_BY_TYPE, 1 },
	{ "shuttle_blast_off", "ShuttleBlastOff", SOUND_BANKS_BY_TYPE, 1 },
	{ "shuttle_shut_down", "ShuttleShutDown", SOUND_BANKS_BY_TYPE, 1 },
	{ "shuttle_take_off", "ShuttleTakeOff", SOUND_BANKS_BY_TYPE, 1 },
	{ "sounds", "Sounds", SOUND_BANKS_BY_SOUND, 1 },
	{ "starship_ambient", "StarshipAmbient", SOUND_BANKS_BY_TYPE, 1 },
	{ "take_off", "TakeOff", SOUND_BANKS_BY_TYPE, 1 },
	{ "weapon", "Weapon", SOUND_BANKS_BY_WEAPON, XWA_SOUND_BANKS_WEAPON_GROUPS },
	{ "weapon_explosion", "WeaponExplosion", SOUND_BANKS_LIST, XWA_SOUND_BANKS_EXPLOSION_MAX },
	{ "weapon_range", "WeaponRange", SOUND_BANKS_BY_RANGE, 1 },
};

/* Describes the entry keys of each layout, in SoundBankLayout order. */
static const char* const s_layoutKeys[] = {
	"object type", "object type, then weapon sound group", "object type, then a list",
	"sound name",  "names that the mod chooses",
};

const char* XwaSoundBanks_Key(XwaSoundBank bank) {
	return (unsigned)bank < XWA_SOUND_BANK_COUNT ? s_banks[bank].key : NULL;
}

const char* XwaSoundBanks_EntryKeys(XwaSoundBank bank) {
	return (unsigned)bank < XWA_SOUND_BANK_COUNT ? s_layoutKeys[s_banks[bank].layout] : NULL;
}

int XwaSoundBanks_ListMax(XwaSoundBank bank) {
	return (unsigned)bank < XWA_SOUND_BANK_COUNT && s_banks[bank].layout == SOUND_BANKS_LIST
			   ? s_banks[bank].perType
			   : 0;
}

/* The entry names of the `sounds` bank, in XwaSoundBankSound order. */
static const char* const s_soundNames[XWA_SOUND_BANK_SOUND_COUNT] = {
	"player_targeted", "map_enter", "map_exit", "turret_switch", "target_select",
};

const char* XwaSoundBanks_SoundName(XwaSoundBankSound sound) {
	return (unsigned)sound < XWA_SOUND_BANK_SOUND_COUNT ? s_soundNames[sound] : NULL;
}

typedef struct SoundBankEntry {
	const char* file; /* NULL for a silent entry */
	uint8_t volume;
	uint16_t distance;
} SoundBankEntry;

/* A bank loaded from flight/sounds.yaml. */
typedef struct SoundBank {
	int present;
	/* The slot for entries that the bank doesn't list. This slot has no
	   sound. -1 when those entries play the stock sound instead. */
	int unlistedSlot;
	/* For each entry, its slot plus 1, or 0 when the bank doesn't list it. */
	int16_t* slots;
	int entryCount;
	/* For SOUND_BANKS_LIST, the number of sounds for each object type. */
	uint8_t* lengths;
	/* For SOUND_BANKS_BY_RANGE, the name of each entry. */
	const char** names;
} SoundBank;

static AeronConfigFile* s_file; /* holds the entries' file names */
static SoundBank s_state[XWA_SOUND_BANK_COUNT];
static int s_slotCount;
static SoundBankEntry s_entries[SOUND_BANKS_SLOT_COUNT];
/* The entries that this flight has tried to load. */
static uint8_t s_tried[SOUND_BANKS_SLOT_COUNT];

/* Reads a number from `min` to `max` into `out`. Logs and returns 0
   otherwise. */
static int SoundBanks_ReadInt(const char* where, const AeronConfigNode* node, int64_t min, int64_t max,
							  int64_t* out) {
	const int64_t value = AeronConfigNode_Int(node, min - 1);

	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < min || value > max) {
		Aeron_LogWarn("xwa.audio",
					  SOUND_BANKS_YAML ":%d: %s must be a number from %lld to %lld. It is ignored",
					  AeronConfigNode_Line(node), where, (long long)min, (long long)max);
		return 0;
	}
	*out = value;
	return 1;
}

/* Returns a new slot for an entry of `bank`. Logs a warning and returns -1
   when no slot is left. */
static int SoundBanks_NewSlot(XwaSoundBank bank) {
	if (s_slotCount == SOUND_BANKS_SLOT_COUNT) {
		Aeron_LogWarn("xwa.audio",
					  SOUND_BANKS_YAML ": %s: the banks hold at most %d sounds. The rest are ignored",
					  s_banks[bank].key, SOUND_BANKS_SLOT_COUNT);
		return -1;
	}
	memset(&s_entries[s_slotCount], 0, sizeof s_entries[s_slotCount]);
	return s_slotCount++;
}

/* Reads `node` into entry `index` of `bank`. The node is a sound or
   {silent: true}. `where` identifies the entry in log messages. Returns 0,
   after a log, when the entry is ignored. */
static int SoundBanks_ReadEntry(XwaSoundBank bank, int index, const char* where,
								const AeronConfigNode* node) {
	SoundBank* state = &s_state[bank];
	const AeronConfigNode* file = AeronConfigNode_MapGet(node, "file");
	const AeronConfigNode* silent = AeronConfigNode_MapGet(node, "silent");
	const AeronConfigNode* value;
	SoundBankEntry* entry;
	int64_t number;
	int slot;

	if (state->slots[index] != 0) {
		Aeron_LogWarn("xwa.audio", SOUND_BANKS_YAML ":%d: %s.%s is listed twice. It is ignored",
					  AeronConfigNode_Line(node), s_banks[bank].key, where);
		return 0;
	}
	if (silent != NULL) {
		if (AeronConfigNode_Type(silent) != AERON_CONFIG_BOOL || !AeronConfigNode_Bool(silent, 0) ||
			file != NULL) {
			Aeron_LogWarn("xwa.audio",
						  SOUND_BANKS_YAML ":%d: %s.%s needs a file or silent: true. It is ignored",
						  AeronConfigNode_Line(node), s_banks[bank].key, where);
			return 0;
		}
	} else if (AeronConfigNode_Type(file) != AERON_CONFIG_STRING ||
			   AeronConfigNode_String(file, "")[0] == '\0') {
		Aeron_LogWarn("xwa.audio", SOUND_BANKS_YAML ":%d: %s.%s needs a file or silent: true. It is ignored",
					  AeronConfigNode_Line(node), s_banks[bank].key, where);
		return 0;
	}
	if ((slot = SoundBanks_NewSlot(bank)) < 0) {
		return 0;
	}
	state->slots[index] = (int16_t)(slot + 1);
	entry = &s_entries[slot];
	if (silent != NULL) {
		return 1;
	}
	entry->file = AeronConfigNode_String(file, "");
	entry->volume = g_fsfxDefaultBaseVolume;
	entry->distance = (uint16_t)g_fsfxDefaultMinDistanceOrRolloff;
	if ((value = AeronConfigNode_MapGet(node, "volume")) != NULL &&
		SoundBanks_ReadInt("volume", value, 0, 255, &number)) {
		entry->volume = (uint8_t)number;
	}
	if ((value = AeronConfigNode_MapGet(node, "distance_m")) != NULL) {
		int32_t units;

		if (XwaUnits_Read(value, XWA_UNIT_METRES, 0, UINT16_MAX, &units)) {
			entry->distance = (uint16_t)units;
		} else {
			Aeron_LogWarn("xwa.audio",
						  SOUND_BANKS_YAML
						  ":%d: distance_m must be a number of metres from 0 to %.2f. It is ignored",
						  AeronConfigNode_Line(value), XwaUnits_FromGame(XWA_UNIT_METRES, UINT16_MAX));
		}
	}
	return 1;
}

/* Reads the entries of `objectType` in a bank keyed by weapon sound group, or
   in a list bank. */
static void SoundBanks_ReadTypeEntries(XwaSoundBank bank, int objectType, const char* name,
									   const AeronConfigNode* node) {
	const SoundBankInfo* info = &s_banks[bank];
	char where[96];
	int stored = 0;
	size_t i;

	if (info->layout == SOUND_BANKS_BY_WEAPON) {
		if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.audio",
						  SOUND_BANKS_YAML ":%d: %s.%s must map weapon sound groups to sounds. It is ignored",
						  AeronConfigNode_Line(node), info->key, name);
			return;
		}
		for (i = 0; i < AeronConfigNode_MapCount(node); ++i) {
			const char* group = AeronConfigNode_MapKeyAt(node, i);
			const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, i);
			const int at = XwaNames_Find(&g_xwaSoundWeaponGroupNames, group);

			snprintf(where, sizeof where, "%s.%s", name, group);
			if (at < 0) {
				snprintf(where, sizeof where, "%s.%s", info->key, name);
				XwaNames_LogUnknown(&g_xwaSoundWeaponGroupNames, SOUND_BANKS_YAML,
									AeronConfigNode_Line(value), where, group);
				continue;
			}
			SoundBanks_ReadEntry(bank, objectType * info->perType + at, where, value);
		}
		return;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) == 0 ||
		AeronConfigNode_SequenceCount(node) > (size_t)info->perType) {
		Aeron_LogWarn("xwa.audio",
					  SOUND_BANKS_YAML ":%d: %s.%s needs a list of 1 to %d sounds. It is ignored",
					  AeronConfigNode_Line(node), info->key, name, info->perType);
		return;
	}
	/* An ignored entry leaves no gap, so a random pick always finds a
	   sound. */
	for (i = 0; i < AeronConfigNode_SequenceCount(node); ++i) {
		snprintf(where, sizeof where, "%s[%zu]", name, i);
		stored += SoundBanks_ReadEntry(bank, objectType * info->perType + stored, where,
									   AeronConfigNode_SequenceGet(node, i));
	}
	s_state[bank].lengths[objectType] = (uint8_t)stored;
}

static void SoundBanks_ReadBank(XwaSoundBank bank, const AeronConfigNode* node) {
	static const char* const keys[] = { "unlisted", "sounds" };
	const SoundBankInfo* info = &s_banks[bank];
	SoundBank* state = &s_state[bank];
	const AeronConfigNode* sounds = AeronConfigNode_MapGet(node, "sounds");
	const AeronConfigNode* unlisted = AeronConfigNode_MapGet(node, "unlisted");
	size_t i;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.audio", SOUND_BANKS_YAML ":%d: %s isn't a map. It is ignored",
					  AeronConfigNode_Line(node), info->key);
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(node); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(node, i);

		if (strcmp(key, keys[0]) != 0 && strcmp(key, keys[1]) != 0) {
			Aeron_LogWarn("xwa.audio", SOUND_BANKS_YAML ":%d: %s.%s isn't a bank key. It is ignored",
						  AeronConfigNode_Line(AeronConfigNode_MapValueAt(node, i)), info->key, key);
		}
	}
	switch (info->layout) {
		case SOUND_BANKS_BY_SOUND:
			state->entryCount = (int)(sizeof s_soundNames / sizeof s_soundNames[0]);
			break;
		case SOUND_BANKS_BY_RANGE:
			state->entryCount = (int)AeronConfigNode_MapCount(sounds);
			break;
		default:
			state->entryCount = OBJ_Count * info->perType;
			break;
	}
	state->slots =
		(int16_t*)calloc(state->entryCount != 0 ? (size_t)state->entryCount : 1, sizeof *state->slots);
	state->lengths = info->layout == SOUND_BANKS_LIST ? (uint8_t*)calloc(OBJ_Count, 1) : NULL;
	state->names = info->layout == SOUND_BANKS_BY_RANGE
					   ? (const char**)calloc(state->entryCount != 0 ? (size_t)state->entryCount : 1,
											  sizeof *state->names)
					   : NULL;
	if (state->slots == NULL || (info->layout == SOUND_BANKS_LIST && state->lengths == NULL) ||
		(info->layout == SOUND_BANKS_BY_RANGE && state->names == NULL)) {
		Aeron_LogError("xwa.audio", SOUND_BANKS_YAML ": out of memory for bank %s", info->key);
		return;
	}
	state->present = 1;
	state->unlistedSlot = -1;
	if (unlisted != NULL) {
		const char* text = AeronConfigNode_String(unlisted, "");

		if (info->layout == SOUND_BANKS_BY_RANGE) {
			Aeron_LogWarn("xwa.audio",
						  SOUND_BANKS_YAML ":%d: weapon_range doesn't use unlisted. It is ignored",
						  AeronConfigNode_Line(unlisted));
		} else if (strcmp(text, "silent") == 0) {
			state->unlistedSlot = SoundBanks_NewSlot(bank);
		} else if (strcmp(text, "stock") != 0) {
			Aeron_LogWarn("xwa.audio",
						  SOUND_BANKS_YAML ":%d: %s.unlisted must be stock or silent. It is ignored",
						  AeronConfigNode_Line(unlisted), info->key);
		}
	}
	if (sounds == NULL) {
		return;
	}
	if (AeronConfigNode_Type(sounds) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.audio", SOUND_BANKS_YAML ": %s: sounds isn't a map. It is ignored", info->key);
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(sounds); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(sounds, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(sounds, i);
		char where[64];
		int at;

		snprintf(where, sizeof where, "banks.%s.sounds", info->key);
		switch (info->layout) {
			case SOUND_BANKS_BY_SOUND:
				for (at = 0; at < state->entryCount && strcmp(key, s_soundNames[at]) != 0; ++at) {
				}
				if (at == state->entryCount) {
					Aeron_LogWarn("xwa.audio",
								  SOUND_BANKS_YAML
								  ":%d: %s: '%s' isn't a sound name of this bank. Use player_targeted, "
								  "map_enter, map_exit, turret_switch or target_select. It is "
								  "ignored",
								  AeronConfigNode_Line(value), info->key, key);
					continue;
				}
				SoundBanks_ReadEntry(bank, at, key, value);
				break;
			case SOUND_BANKS_BY_RANGE:
				if (!XwaNames_ValidName(key)) {
					Aeron_LogWarn("xwa.audio",
								  SOUND_BANKS_YAML
								  ":%d: %s: '%s' needs lower-case letters, digits and '_'. It is "
								  "ignored",
								  AeronConfigNode_Line(value), info->key, key);
					continue;
				}
				state->names[i] = key;
				SoundBanks_ReadEntry(bank, (int)i, key, value);
				break;
			default:
				if ((at = XwaNames_Find(&g_xwaObjectTypeNames, key)) < 0) {
					XwaNames_LogUnknown(&g_xwaObjectTypeNames, SOUND_BANKS_YAML, AeronConfigNode_Line(value),
										where, key);
				} else if (info->layout == SOUND_BANKS_BY_TYPE) {
					SoundBanks_ReadEntry(bank, at, key, value);
				} else {
					SoundBanks_ReadTypeEntries(bank, at, key, value);
				}
				break;
		}
	}
}

static void SoundBanks_Free(void) {
	int bank;

	for (bank = 0; bank < XWA_SOUND_BANK_COUNT; ++bank) {
		free(s_state[bank].slots);
		free(s_state[bank].lengths);
		free((void*)s_state[bank].names);
	}
	memset(s_state, 0, sizeof s_state);
}

void XwaSoundBanks_Init(void) {
	const AeronConfigNode* root;
	const AeronConfigNode* banks;
	int sounds = 0;
	int bank;
	size_t i;

	AeronConfigFile_Destroy(s_file);
	s_file = NULL;
	SoundBanks_Free();
	memset(s_entries, 0, sizeof s_entries);
	s_slotCount = 0;
	if (XwaModFiles_Load(SOUND_BANKS_YAML, 0, &s_file) != XWA_MOD_FILE_LOADED) {
		return;
	}
	root = AeronConfigFile_Root(s_file);
	banks = AeronConfigNode_MapGet(root, "banks");
	if (AeronConfigNode_Type(banks) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.audio", SOUND_BANKS_YAML ": the file needs a 'banks' map. It is ignored");
		AeronConfigFile_Destroy(s_file);
		s_file = NULL;
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(banks); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(banks, i);

		for (bank = 0; bank < XWA_SOUND_BANK_COUNT && strcmp(key, s_banks[bank].key) != 0; ++bank) {
		}
		if (bank == XWA_SOUND_BANK_COUNT) {
			Aeron_LogWarn("xwa.audio", SOUND_BANKS_YAML ": '%s' isn't a sound bank. It is ignored", key);
		} else if (!s_state[bank].present) {
			SoundBanks_ReadBank((XwaSoundBank)bank, AeronConfigNode_MapValueAt(banks, i));
		}
	}
	for (i = 0; i < (size_t)s_slotCount; ++i) {
		sounds += s_entries[i].file != NULL;
	}
	Aeron_LogInfo("xwa.audio", SOUND_BANKS_YAML ": %d sounds in %d entries", sounds, s_slotCount);
}

/* Whether the game loads sound effects in this flight, as in
   FeDiskIo_LoadFlightSfxBanks. */
static int SoundBanks_SfxEnabled(void) {
	return g_flightConfSfxEnabled && (g_gameConfig.sfxExteriorEnabled || g_gameConfig.sfxInteriorEnabled ||
									  g_gameConfig.sfxEngineEnabled);
}

/* Loads the sound of bank slot `slot`, counted from the first bank slot.
   Returns 1 if the entry has a sound and this flight hadn't tried to load it
   yet. */
static int SoundBanks_Load(XwaSoundBank bank, int slot) {
	const SoundBankEntry* entry = &s_entries[slot];
	const char* file;
	const char* stem;
	char name[64];
	int length;

	if (s_tried[slot] || entry->file == NULL) {
		return 0;
	}
	s_tried[slot] = 1;
	/* The effect's name is the bank's name plus the file name, without the
	   folder and the extension. So when several entries of a bank use the
	   same file, it only loads once. */
	file = entry->file;
	stem = file + strlen(file);
	while (stem > file && stem[-1] != '\\' && stem[-1] != '/') {
		--stem;
	}
	length = (int)(strrchr(stem, '.') != NULL ? strrchr(stem, '.') - stem : (ptrdiff_t)strlen(stem));
	snprintf(name, sizeof name, "%s_%.*s", s_banks[bank].name, length, stem);
	if (Sound_LoadEffect(file, name, entry->distance)) {
		g_sfxIds[XWA_SOUND_BANKS_FIRST_SLOT + slot] = Sound_FindEffectByName(name);
	}
	return 1;
}

void XwaSoundBanks_LoadFlight(const struct XwaFlightGroup* flightGroups, size_t stride, int count) {
	uint8_t used[OBJ_Count];
	int loaded = 0;
	int bank;
	int index;

	memset(s_tried, 0, sizeof s_tried);
	if (s_slotCount == 0) {
		return;
	}
	memset(used, 0, sizeof used);
	for (index = 0; index < count; ++index) {
		const struct XwaFlightGroup* fg =
			(const struct XwaFlightGroup*)((const unsigned char*)flightGroups + (size_t)index * stride);

		if (g_objectTypeTables.craftTypeToObjectType[fg->craftType] < OBJ_Count) {
			used[g_objectTypeTables.craftTypeToObjectType[fg->craftType]] = 1;
		}
	}
	for (bank = 0; bank < XWA_SOUND_BANK_COUNT; ++bank) {
		const SoundBank* state = &s_state[bank];
		const SoundBankLayout layout = s_banks[bank].layout;
		const int perType = s_banks[bank].perType;

		for (index = 0; index < state->entryCount; ++index) {
			if (state->slots[index] != 0 &&
				(layout == SOUND_BANKS_BY_SOUND || layout == SOUND_BANKS_BY_RANGE || used[index / perType])) {
				loaded += SoundBanks_Load((XwaSoundBank)bank, state->slots[index] - 1);
			}
		}
	}
	Aeron_LogInfo("xwa.audio", "sound banks: %d sounds for the mission's flight groups", loaded);
}

int XwaSoundBanks_Has(XwaSoundBank bank) {
	return (unsigned int)bank < XWA_SOUND_BANK_COUNT && s_state[bank].present;
}

int XwaSoundBanks_Slot(XwaSoundBank bank, unsigned int index) {
	const SoundBank* state;
	int slot;

	if (!XwaSoundBanks_Has(bank)) {
		return -1;
	}
	state = &s_state[bank];
	if (index >= (unsigned int)state->entryCount || state->slots[index] == 0) {
		return state->unlistedSlot != -1 ? XWA_SOUND_BANKS_FIRST_SLOT + state->unlistedSlot : -1;
	}
	slot = state->slots[index] - 1;
	if (SoundBanks_SfxEnabled() && SoundBanks_Load(bank, slot)) {
		Aeron_LogInfo("xwa.audio", "%s: loaded on first use", s_entries[slot].file);
	}
	return XWA_SOUND_BANKS_FIRST_SLOT + slot;
}

int XwaSoundBanks_ListLength(XwaSoundBank bank, unsigned int objectType) {
	return XwaSoundBanks_Has(bank) && s_state[bank].lengths != NULL && objectType < OBJ_Count
			   ? s_state[bank].lengths[objectType]
			   : 0;
}

int XwaSoundBanks_FindRange(const char* name) {
	const SoundBank* state = &s_state[XWA_SOUND_BANK_WEAPON_RANGE];
	int index;

	for (index = 0; state->present && name != NULL && index < state->entryCount; ++index) {
		if (state->names[index] != NULL && strcmp(state->names[index], name) == 0) {
			return index;
		}
	}
	return -1;
}

uint8_t XwaSoundBanks_Volume(unsigned int sfxSlot) {
	const unsigned int slot = sfxSlot - XWA_SOUND_BANKS_FIRST_SLOT;

	return slot < (unsigned int)s_slotCount && s_entries[slot].file != NULL ? s_entries[slot].volume
																			: g_fsfxDefaultBaseVolume;
}

uint16_t XwaSoundBanks_Distance(unsigned int sfxSlot) {
	const unsigned int slot = sfxSlot - XWA_SOUND_BANKS_FIRST_SLOT;

	return slot < (unsigned int)s_slotCount && s_entries[slot].file != NULL
			   ? s_entries[slot].distance
			   : (uint16_t)g_fsfxDefaultMinDistanceOrRolloff;
}

/* Whether `text` starts with `prefix`, ignoring case. `prefix` is lower case. */
static int SoundBanks_StartsWith(const char* text, const char* prefix) {
	for (; *prefix != '\0'; ++text, ++prefix) {
		if (tolower((unsigned char)*text) != *prefix) {
			return 0;
		}
	}
	return 1;
}

int XwaSoundBanks_IsPlaceholder(const char* fileName) {
	const char* name = fileName;
	const char* c;

	for (c = fileName; *c != '\0'; ++c) {
		if (*c == '\\' || *c == '/') {
			name = c + 1;
		}
	}
	return Xwa_Stricmp(name, "none") == 0 || Xwa_Stricmp(name, "none.wav") == 0 ||
		   SoundBanks_StartsWith(name, "unused") || SoundBanks_StartsWith(name, "not-used");
}
