#ifndef XWA_RUNTIME_MODELS_NAMES_H
#define XWA_RUNTIME_MODELS_NAMES_H

/* Mod files use names instead of numbers for many game values, such as
 * object types, weapon types, warheads, genus, family and object type flags.
 * This file has the name tables. Every reader looks up names here, so a value
 * has the same name in every file.
 *
 * A table has a name for each value from `first` to `first + count - 1`. A
 * NULL entry is a value without a name, and files can't use it.
 *
 * A mod can rename object types. Lookups then use the new names, and the old
 * stock names no longer work.
 *
 * This file doesn't depend on game code, so tools can link it too. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

typedef struct XwaNameTable {
	/* What the values are, for log messages, such as "object type". */
	const char* what;
	const char* const* names;
	int first;
	int count;
	/* The mod's new names, or NULL for a table that a mod can't rename. A
	   NULL entry keeps the name from `names`. */
	const char** renames;
} XwaNameTable;

/* Names use lower-case letters, digits and '_', and are shorter than this. */
enum { XWA_NAME_MAX = 64 };

/* The 557 stock object types, from `none` (0) to `ds_laser_power_source`. */
extern const XwaNameTable g_xwaObjectTypeNames;
/* Weapon types 280 to 307. They are also object types, and have the same
   names. */
extern const XwaNameTable g_xwaWeaponTypeNames;
/* Flight group warhead indexes 1 to 10, as in g_warheadTypeIds. Index 0 is
   no warhead, and no key accepts it, so it has no name. */
extern const XwaNameTable g_xwaWarheadNames;
extern const XwaNameTable g_xwaGenusNames;
extern const XwaNameTable g_xwaFamilyNames;
/* Object type flags, indexed by bit. */
extern const XwaNameTable g_xwaObjectTypeFlagNames;
/* The object type flags that a file can set. */
enum { XWA_OBJECT_TYPE_SETTABLE_FLAGS = 0x0001 | 0x0002 | 0x0004 | 0x0080 | 0x0800 | 0x1000 | 0x2000 };
/* Ship list categories for craft type files. */
extern const XwaNameTable g_xwaShipListCategoryNames;
/* The text that the game shows for each ship list category. */
extern const XwaNameTable g_xwaShipListCategoryTexts;
/* Laser group kinds, the weaponType of the group's slots: 1, 2, 4 and 5, or
   none (0). */
extern const XwaNameTable g_xwaLaserKindNames;
/* What a hardpoint type fires: none, laser or warhead. */
extern const XwaNameTable g_xwaHardpointKindNames;
/* Pilot ranks for fire_ratio_by_rank, starting with novice (0). */
extern const XwaNameTable g_xwaRankNames;
/* Difficulties for damage_by_difficulty, starting with easy (0). */
extern const XwaNameTable g_xwaDifficultyNames;
/* The slots of HUD image group 12000 that a model's `hud.reticle` can
   replace. The threat indicators have dotted names, such as
   `threats.laser_mode.turret`. */
extern const XwaNameTable g_xwaReticleSlotNames;
/* Weapon sound groups, the entries for each object type in the `weapon`
   sound bank. */
extern const XwaNameTable g_xwaSoundWeaponGroupNames;
/* Sources for cockpit screen rectangles. The cockpit screen code has a
   rectangle for each one. */
enum { XWA_SCREEN_SOURCE_COUNT = 57 };
extern const XwaNameTable g_xwaScreenSourceNames;
/* HUD areas that a cockpit screen can hide, indexed by bit. */
extern const XwaNameTable g_xwaHudAreaNames;

/* Returns the name of `value`, using the mod's renames, or NULL when the
   value has no name. */
const char* XwaNames_Name(const XwaNameTable* table, int value);
/* Returns the stock name of `value`, or NULL when the value has no name. */
const char* XwaNames_StockName(const XwaNameTable* table, int value);
/* Returns whether `text` is a valid name (see XWA_NAME_MAX). */
int XwaNames_ValidName(const char* text);
/* Renames `objectType` to `name`. The table keeps a copy of `name`. NULL or an
   invalid name restores the stock name. Weapon types are object types, so
   this renames them too. */
void XwaNames_RenameObjectType(int objectType, const char* name);
/* Restores the stock names of all object types. */
void XwaNames_ResetObjectTypeNames(void);
/* Returns the value named `text`, or -1 when no value has this name. */
int XwaNames_Find(const XwaNameTable* table, const char* text);

/* Reads `node` as a name from `table` and stores its value in `*out`. `node`
   is the value of `key` in file `path`. If the name isn't in the table, logs
   the file, the line, the key and the valid names, and returns 0. */
int XwaNames_Read(const XwaNameTable* table, const char* path, const char* key,
				  const struct AeronConfigNode* node, int* out);
/* Logs that `text` isn't a name in `table`. `text` is the value of `key` in
   file `path` at `line`. Lists the valid names when the table is short. */
void XwaNames_LogUnknown(const XwaNameTable* table, const char* path, int line, const char* key,
						 const char* text);

#ifdef __cplusplus
}
#endif

#endif
