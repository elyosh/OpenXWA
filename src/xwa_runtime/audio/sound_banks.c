#include "xwa_runtime/audio/sound_banks.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/audio/fsfx.h"
#include "xwa/audio/sound.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/frontend_mission.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define SOUND_BANKS_YAML "flight/sounds.yaml"
#define SOUND_BANKS_SLOT_COUNT (XWA_SFX_SLOT_COUNT - XWA_SOUND_BANKS_FIRST_SLOT)

/* How a bank's entries are indexed. */
enum {
	SOUND_BANKS_BY_NAME = -1,
	SOUND_BANKS_BY_POSITION = 0,
	SOUND_BANKS_BY_TYPE = 1 /* more for several entries by object type */
};

typedef struct SoundBankInfo {
	const char* key;  /* the key in flight/sounds.yaml */
	const char* name; /* the start of its sound effect names */
	int layout;
} SoundBankInfo;

static const SoundBankInfo s_banks[XWA_SOUND_BANK_COUNT] = {
	{ "canopy_opening", "CanopyOpening", SOUND_BANKS_BY_TYPE },
	{ "engine_slowing", "EngineSlowing", SOUND_BANKS_BY_TYPE },
	{ "engine_wash", "EngineWash", SOUND_BANKS_BY_TYPE },
	{ "enter_hyperspace", "EnterHyperspace", SOUND_BANKS_BY_TYPE },
	{ "exit_hyperspace", "ExitHyperspace", SOUND_BANKS_BY_TYPE },
	{ "fly_by", "FlyBy", SOUND_BANKS_BY_TYPE },
	{ "hangar_ambient", "HangarAmbient", 10 },
	{ "hyper_abort", "HyperAbort", SOUND_BANKS_BY_TYPE },
	{ "hyper_end", "HyperEnd", SOUND_BANKS_BY_TYPE },
	{ "hyper_start", "HyperStart", SOUND_BANKS_BY_TYPE },
	{ "hyper_zoom", "HyperZoom", SOUND_BANKS_BY_TYPE },
	{ "interior", "Interior", SOUND_BANKS_BY_TYPE },
	{ "s_foil", "SFoil", SOUND_BANKS_BY_TYPE },
	{ "shuttle_blast_off", "ShuttleBlastOff", SOUND_BANKS_BY_TYPE },
	{ "shuttle_shut_down", "ShuttleShutDown", SOUND_BANKS_BY_TYPE },
	{ "shuttle_take_off", "ShuttleTakeOff", SOUND_BANKS_BY_TYPE },
	{ "sounds", "Sounds", SOUND_BANKS_BY_NAME },
	{ "starship_ambient", "StarshipAmbient", SOUND_BANKS_BY_TYPE },
	{ "take_off", "TakeOff", SOUND_BANKS_BY_TYPE },
	{ "weapon", "Weapon", 10 },
	{ "weapon_explosion", "WeaponExplosion", 8 },
	{ "weapon_range", "WeaponRange", SOUND_BANKS_BY_POSITION },
};

/* The entries of the `sounds` bank, in XwaSoundBankSound's order. */
static const char* const s_soundNames[] = { "player_targeted", "map_enter", "map_exit", "turret_switch",
											"target_select" };

typedef struct SoundBankEntry {
	const char* file; /* NULL for a silent entry */
	uint8_t volume;
	uint16_t distance;
} SoundBankEntry;

static AeronConfigFile* s_file; /* holds the entries' file names */
static int s_first[XWA_SOUND_BANK_COUNT];
static int s_count[XWA_SOUND_BANK_COUNT];
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

/* Reads a map key that is an index from 0 to `max`, or returns -1. */
static int SoundBanks_ReadIndex(const char* bank, const char* key, int max) {
	char* end;
	const long value = strtol(key, &end, 10);

	if (*key == '\0' || *end != '\0' || value < 0 || value > max) {
		Aeron_LogWarn("xwa.audio", SOUND_BANKS_YAML ": %s: '%s' isn't an entry from 0 to %d. It is ignored",
					  bank, key, max);
		return -1;
	}
	return (int)value;
}

static void SoundBanks_ReadEntry(XwaSoundBank bank, int index, const AeronConfigNode* node) {
	SoundBankEntry* entry = &s_entries[s_first[bank] + index];
	const AeronConfigNode* file = AeronConfigNode_MapGet(node, "file");
	const AeronConfigNode* value;
	int64_t number;

	if (index >= s_count[bank]) {
		Aeron_LogWarn("xwa.audio",
					  SOUND_BANKS_YAML ":%d: %s: entry %d is past the bank's count, %d. It is ignored",
					  AeronConfigNode_Line(node), s_banks[bank].key, index, s_count[bank]);
		return;
	}
	if (AeronConfigNode_Type(file) != AERON_CONFIG_STRING || AeronConfigNode_String(file, "")[0] == '\0') {
		Aeron_LogWarn("xwa.audio", SOUND_BANKS_YAML ":%d: %s: an entry needs a file. It is ignored",
					  AeronConfigNode_Line(node), s_banks[bank].key);
		return;
	}
	entry->file = AeronConfigNode_String(file, "");
	entry->volume = g_fsfxDefaultBaseVolume;
	entry->distance = (uint16_t)g_fsfxDefaultMinDistanceOrRolloff;
	if ((value = AeronConfigNode_MapGet(node, "volume")) != NULL &&
		SoundBanks_ReadInt("volume", value, 0, 255, &number)) {
		entry->volume = (uint8_t)number;
	}
	if ((value = AeronConfigNode_MapGet(node, "distance")) != NULL &&
		SoundBanks_ReadInt("distance", value, 0, 65535, &number)) {
		entry->distance = (uint16_t)number;
	}
}

static void SoundBanks_ReadBank(XwaSoundBank bank, const AeronConfigNode* node) {
	const SoundBankInfo* info = &s_banks[bank];
	const AeronConfigNode* sounds = AeronConfigNode_MapGet(node, "sounds");
	int64_t count;
	size_t i;

	if (!SoundBanks_ReadInt("count", AeronConfigNode_MapGet(node, "count"), 0, SOUND_BANKS_SLOT_COUNT,
							&count)) {
		return;
	}
	if (s_slotCount + count > SOUND_BANKS_SLOT_COUNT) {
		Aeron_LogWarn("xwa.audio",
					  SOUND_BANKS_YAML ": %s: the banks have more than %d entries. It is ignored", info->key,
					  SOUND_BANKS_SLOT_COUNT);
		return;
	}
	s_first[bank] = s_slotCount;
	s_count[bank] = (int)count;
	s_slotCount += (int)count;
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

		if (info->layout == SOUND_BANKS_BY_NAME) {
			size_t name;

			for (name = 0; name < sizeof s_soundNames / sizeof s_soundNames[0]; ++name) {
				if (strcmp(key, s_soundNames[name]) == 0) {
					SoundBanks_ReadEntry(bank, (int)name, value);
					break;
				}
			}
			if (name == sizeof s_soundNames / sizeof s_soundNames[0]) {
				Aeron_LogWarn("xwa.audio",
							  SOUND_BANKS_YAML ": %s: '%s' isn't a sound of the bank. It is ignored",
							  info->key, key);
			}
		} else if (info->layout > SOUND_BANKS_BY_TYPE) {
			const int objectType = SoundBanks_ReadIndex(info->key, key, OBJ_Count - 1);
			size_t j;

			if (objectType < 0) {
				continue;
			}
			for (j = 0; j < AeronConfigNode_MapCount(value); ++j) {
				const int index =
					SoundBanks_ReadIndex(info->key, AeronConfigNode_MapKeyAt(value, j), info->layout - 1);

				if (index >= 0) {
					SoundBanks_ReadEntry(bank, objectType * info->layout + index,
										 AeronConfigNode_MapValueAt(value, j));
				}
			}
		} else {
			const int index = SoundBanks_ReadIndex(info->key, key, SOUND_BANKS_SLOT_COUNT - 1);

			if (index >= 0) {
				SoundBanks_ReadEntry(bank, index, value);
			}
		}
	}
}

void XwaSoundBanks_Init(void) {
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* banks;
	int sounds = 0;
	int bank;
	size_t i;

	AeronConfigFile_Destroy(s_file);
	s_file = NULL;
	memset(s_count, 0, sizeof s_count);
	memset(s_entries, 0, sizeof s_entries);
	s_slotCount = 0;
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, SOUND_BANKS_YAML, &s_file, &error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.audio", SOUND_BANKS_YAML ":%d: %s. The file is ignored", error.line,
						   error.message);
		}
		s_file = NULL;
		return;
	}
	root = AeronConfigFile_Root(s_file);
	banks = AeronConfigNode_MapGet(root, "banks");
	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1 ||
		AeronConfigNode_Type(banks) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.audio",
					   SOUND_BANKS_YAML ": the file needs 'version: 1' and a 'banks' map. It is ignored");
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
		} else if (s_count[bank] == 0) {
			SoundBanks_ReadBank((XwaSoundBank)bank, AeronConfigNode_MapValueAt(banks, i));
		}
	}
	for (i = 0; i < (size_t)s_slotCount; ++i) {
		sounds += s_entries[i].file != NULL;
	}
	Aeron_LogInfo("xwa.audio", SOUND_BANKS_YAML ": %d sounds in %d entries", sounds, s_slotCount);
}

/* Whether the game loads sound effects in this flight (FeDiskIo_LoadFlightSfxBanks). */
static int SoundBanks_SfxEnabled(void) {
	return g_flightConfSfxEnabled && (g_gameConfig.sfxExteriorEnabled || g_gameConfig.sfxInteriorEnabled ||
									  g_gameConfig.sfxEngineEnabled);
}

/* Loads the sound of bank slot `slot`, from the first bank slot. Returns 1
   when the entry has a sound that wasn't tried yet. */
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
	/* The effect's name is the bank's name and the file's name without
	   its folder and extension, so a file that several entries of a bank
	   use loads once. */
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

		if (fg->craftType < XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT &&
			g_objectTypeTables.craftTypeToObjectType[fg->craftType] < OBJ_Count) {
			used[g_objectTypeTables.craftTypeToObjectType[fg->craftType]] = 1;
		}
	}
	for (bank = 0; bank < XWA_SOUND_BANK_COUNT; ++bank) {
		const int layout = s_banks[bank].layout;

		for (index = 0; index < s_count[bank]; ++index) {
			if (layout <= SOUND_BANKS_BY_POSITION || (index / layout < OBJ_Count && used[index / layout])) {
				loaded += SoundBanks_Load((XwaSoundBank)bank, s_first[bank] + index);
			}
		}
	}
	Aeron_LogInfo("xwa.audio", "sound banks: %d sounds for the mission's flight groups", loaded);
}

int XwaSoundBanks_Count(XwaSoundBank bank) {
	return (unsigned int)bank < XWA_SOUND_BANK_COUNT ? s_count[bank] : 0;
}

int XwaSoundBanks_Slot(XwaSoundBank bank, unsigned int index) {
	int slot;

	if (index >= (unsigned int)XwaSoundBanks_Count(bank)) {
		return -1;
	}
	slot = s_first[bank] + (int)index;
	if (SoundBanks_SfxEnabled() && SoundBanks_Load(bank, slot)) {
		Aeron_LogInfo("xwa.audio", "%s: loaded on first use", s_entries[slot].file);
	}
	return XWA_SOUND_BANKS_FIRST_SLOT + slot;
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
