#ifndef XWA_RUNTIME_AUDIO_SOUND_BANKS_H
#define XWA_RUNTIME_AUDIO_SOUND_BANKS_H

/* Sound banks let a mod give each object type its own sounds, such as engine
 * or weapon sounds. The mod lists them by bank in its sound settings. Each
 * entry is a sound file, with a volume and a 3D sound distance that work
 * like the columns of the game's .lst files, or a silent entry. A bank can
 * also make the entries it doesn't list silent instead of stock.
 *
 * Each listed entry gets its own sound effect slot. These slots start at
 * 2872, after the game's own slots, so the game's sound functions can play
 * them like any other slot.
 *
 * At flight start, the game loads the sounds of the object types that the
 * mission's flight groups use. It loads any other entry the first time it
 * plays. The banks load whenever sound effects are on, even with voices
 * off. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The first sound effect slot of the banks. */
#define XWA_SOUND_BANKS_FIRST_SLOT 2872

enum {
	/* The number of weapon bank entries for each object type. */
	XWA_SOUND_BANKS_WEAPON_GROUPS = 10,
	/* The maximum number of sounds for one object type in hangar_ambient and
	   weapon_explosion. */
	XWA_SOUND_BANKS_HANGAR_AMBIENT_MAX = 10,
	XWA_SOUND_BANKS_EXPLOSION_MAX = 8,
};

struct XwaFlightGroup;

typedef enum XwaSoundBank {
	XWA_SOUND_BANK_CANOPY_OPENING,    /* keyed by object type */
	XWA_SOUND_BANK_ENGINE_SLOWING,    /* keyed by object type */
	XWA_SOUND_BANK_ENGINE_WASH,       /* keyed by object type */
	XWA_SOUND_BANK_FLY_BY,            /* keyed by object type */
	XWA_SOUND_BANK_HANGAR_AMBIENT,    /* a list for each command ship type */
	XWA_SOUND_BANK_HYPER_ABORT,       /* keyed by object type */
	XWA_SOUND_BANK_HYPER_END,         /* keyed by object type */
	XWA_SOUND_BANK_HYPER_START,       /* keyed by object type */
	XWA_SOUND_BANK_HYPER_ZOOM,        /* keyed by object type */
	XWA_SOUND_BANK_INTERIOR,          /* keyed by object type */
	XWA_SOUND_BANK_S_FOIL,            /* keyed by object type */
	XWA_SOUND_BANK_SHUTTLE_BLAST_OFF, /* keyed by object type */
	XWA_SOUND_BANK_SHUTTLE_SHUT_DOWN, /* keyed by object type */
	XWA_SOUND_BANK_SHUTTLE_TAKE_OFF,  /* keyed by object type */
	XWA_SOUND_BANK_SOUNDS,            /* keyed by XwaSoundBankSound name */
	XWA_SOUND_BANK_STARSHIP_AMBIENT,  /* keyed by object type */
	XWA_SOUND_BANK_TAKE_OFF,          /* keyed by object type */
	XWA_SOUND_BANK_WEAPON,            /* 10 groups for each firing object type */
	XWA_SOUND_BANK_WEAPON_EXPLOSION,  /* a list for each object type */
	XWA_SOUND_BANK_WEAPON_RANGE,      /* keyed by names that the mod chooses */
	XWA_SOUND_BANK_COUNT
} XwaSoundBank;

/* The entries of XWA_SOUND_BANK_SOUNDS. */
typedef enum XwaSoundBankSound {
	XWA_SOUND_BANK_PLAYER_TARGETED,
	XWA_SOUND_BANK_MAP_ENTER,
	XWA_SOUND_BANK_MAP_EXIT,
	XWA_SOUND_BANK_TURRET_SWITCH,
	XWA_SOUND_BANK_TARGET_SELECT,
	XWA_SOUND_BANK_SOUND_COUNT
} XwaSoundBankSound;

/* Returns the key of `bank` in flight/sounds.yaml, such as "canopy_opening". */
const char* XwaSoundBanks_Key(XwaSoundBank bank);
/* Describes the keys of the entries in `bank`, such as "object type, then
   weapon sound group". */
const char* XwaSoundBanks_EntryKeys(XwaSoundBank bank);
/* Returns the maximum number of sounds that one object type can have in a
   list bank, or 0 when `bank` isn't a list bank. */
int XwaSoundBanks_ListMax(XwaSoundBank bank);
/* Returns the name of `sound` in the `sounds` bank, such as "map_enter". */
const char* XwaSoundBanks_SoundName(XwaSoundBankSound sound);

/* Reads flight/sounds.yaml. Called once at startup. */
void XwaSoundBanks_Init(void);

/* Loads the bank sounds of the object types that the mission's flight groups
   use. `flightGroups` points to the first flight group, `stride` is the
   distance between two flight groups, and `count` is their number. Called at
   flight start, after the game's sound effects. */
void XwaSoundBanks_LoadFlight(const struct XwaFlightGroup* flightGroups, size_t stride, int count);

/* Returns whether flight/sounds.yaml has `bank`. */
int XwaSoundBanks_Has(XwaSoundBank bank);

/* Returns the sound effect slot of entry `index` in `bank`. Returns -1 when
   the stock sound should play: the mod has no such bank, or the bank doesn't
   list the entry and `unlisted` is stock. Loads the entry's sound the first
   time. A silent entry has a slot without a sound: its g_sfxIds value is -1.

   `index` depends on the bank:
	 - banks keyed by object type: the object type;
	 - weapon: object type * XWA_SOUND_BANKS_WEAPON_GROUPS + group;
	 - list banks: object type * the list's maximum length + position;
	 - sounds: an XwaSoundBankSound;
	 - weapon_range: an entry from XwaSoundBanks_FindRange. */
int XwaSoundBanks_Slot(XwaSoundBank bank, unsigned int index);

/* Returns the number of sounds that `objectType` has in a list bank, or 0. */
int XwaSoundBanks_ListLength(XwaSoundBank bank, unsigned int objectType);

/* Returns the weapon_range entry named `name`, or -1. */
int XwaSoundBanks_FindRange(const char* name);

/* The volume and the distance of bank slot `sfxSlot`. */
uint8_t XwaSoundBanks_Volume(unsigned int sfxSlot);
uint16_t XwaSoundBanks_Distance(unsigned int sfxSlot);

#ifdef __cplusplus
}
#endif

#endif
