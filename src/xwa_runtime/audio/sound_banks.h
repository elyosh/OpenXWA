#ifndef XWA_RUNTIME_AUDIO_SOUND_BANKS_H
#define XWA_RUNTIME_AUDIO_SOUND_BANKS_H

/* Sound banks let a mod give each object type its own sounds, such as engine
 * or weapon sounds. A mod lists them in flight/sounds.yaml:
 *
 *   banks:
 *     interior:                     # keyed by object type
 *       sounds:
 *         xwing: {file: 'Wave/Sfx_Interior/EngineIntXW.wav', volume: 76, distance_m: 200}
 *         awing: {silent: true}
 *     weapon:                       # keyed by object type, then weapon sound group
 *       unlisted: silent
 *       sounds:
 *         xwing:
 *           rebel_laser: {file: ...}
 *           turbolaser_red: {file: ...}
 *     hangar_ambient:               # a list of sounds for each object type
 *       sounds:
 *         imperial_star_destroyer2:
 *           - {file: ...}
 *     sounds:                       # keyed by sound name
 *       sounds:
 *         map_exit: {file: ...}
 *     weapon_range:                 # keyed by names that the mod chooses
 *       sounds:
 *         isd_turbolaser_near: {file: ..., distance_m: 50}
 *
 * An entry is either a sound or `{silent: true}`. `volume` goes from 0 to
 * 255 and defaults to 127, which plays the file at its own level. Lower
 * values play it quieter, and the game never plays a sound louder than its
 * file. `distance_m` is the 3D sound distance in metres, from 0 to 1599.98,
 * and defaults to 200. Both work like the columns of the game's .lst files,
 * whose distance is in game units, 40.96 to the metre.
 *
 * `unlisted` sets what plays for an entry that the bank doesn't list.
 * `stock` is the default and plays the stock game's sound. `silent` plays
 * nothing.
 *
 * The banks:
 *   - canopy_opening, engine_slowing, engine_wash, fly_by, hyper_abort,
 *     hyper_end, hyper_start, hyper_zoom, interior, s_foil,
 *     shuttle_blast_off, shuttle_shut_down, shuttle_take_off,
 *     starship_ambient and take_off have one sound for each object type.
 *   - weapon is keyed by the object type of the craft that fires, then by
 *     weapon sound group. The turbolaser_red sound also plays for the light,
 *     medium and heavy red turbolasers, and turbolaser_green for the green
 *     ones.
 *   - hangar_ambient has a list of up to 10 sounds for each command ship
 *     object type. The game picks one at random.
 *   - weapon_explosion has a list of up to 8 sounds for each object type.
 *     The game picks one at random, and doesn't repeat a sound until half of
 *     them have played.
 *   - sounds has player_targeted, map_enter, map_exit, turret_switch and
 *     target_select.
 *   - weapon_range has sounds with names that the mod chooses. A model's
 *     `sound.weapon_ranges` setting uses these names.
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
