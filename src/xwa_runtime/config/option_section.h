#ifndef XWA_RUNTIME_OPTION_SECTION_H
#define XWA_RUNTIME_OPTION_SECTION_H

#include "aeron/config_file.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A section of options, such as `camera`, read into a struct of the same
 * keys. The values come from three places, and each one overrides the one
 * before it:
 *   1. the section in the shipped remaster/config.yaml, which has every key;
 *   2. the section in a mod file, such as flight/flight.yaml. When the mod
 *      has the section, `enabled` is on unless the section turns it off;
 *   3. the section in the player's config.yaml.
 * A key that a place leaves out keeps the value before it. A wrong value in
 * the shipped or the player's file is an error. A wrong value in the mod is
 * logged and ignored. */

typedef enum XwaOptionKind {
	/* true or false, stored as an int. */
	XWA_OPTION_KIND_SWITCH = 0,
	/* A float from `min` to `max`. */
	XWA_OPTION_KIND_NUMBER,
	/* Three floats of 0 or more. */
	XWA_OPTION_KIND_COLOR,
	/* One of the words in `choices`, stored as its index in an int. */
	XWA_OPTION_KIND_CHOICE,
} XwaOptionKind;

typedef struct XwaOptionKey {
	const char* name;
	XwaOptionKind kind;
	float min;
	float max;
	/* For a choice: the words, ending with NULL. */
	const char* const* choices;
	/* The field's offset in the options struct. */
	size_t offset;
	/* 0 when the player's file can't set the key. */
	int player;
} XwaOptionKey;

/* Key table entries. The key has the name of the struct field. */
#define XWA_OPTION_SWITCH(Type, field)                                                                       \
	{ #field, XWA_OPTION_KIND_SWITCH, 0.0f, 1.0f, NULL, offsetof(Type, field), 1 }
#define XWA_OPTION_NUMBER(Type, field, min, max)                                                             \
	{ #field, XWA_OPTION_KIND_NUMBER, min, max, NULL, offsetof(Type, field), 1 }
/* A number that only the shipped file and the mod can set. */
#define XWA_OPTION_MOD_NUMBER(Type, field, min, max)                                                         \
	{ #field, XWA_OPTION_KIND_NUMBER, min, max, NULL, offsetof(Type, field), 0 }
#define XWA_OPTION_COLOR(Type, field)                                                                        \
	{ #field, XWA_OPTION_KIND_COLOR, 0.0f, 0.0f, NULL, offsetof(Type, field), 1 }
#define XWA_OPTION_CHOICE(Type, field, words)                                                                \
	{ #field, XWA_OPTION_KIND_CHOICE, 0.0f, 0.0f, words, offsetof(Type, field), 1 }

typedef struct XwaOptionSection XwaOptionSection;

struct XwaOptionSection {
	const char* name;
	/* The mod file that can have the section, and the log channel of its
	   messages. */
	const char* mod_file;
	const char* log_channel;
	/* At most 32 keys. */
	const XwaOptionKey* keys;
	size_t key_count;
	/* Other keys of the mod's section, read by other code. They end with
	   NULL. */
	const char* const* other_keys;
	/* The options struct: its size, and the module's three copies of it. */
	size_t size;
	void* shipped;
	void* mod;
	void* player;
	/* Checks the values in use after each place is read, and logs what it
	   changes. Optional. */
	void (*check)(const void* options);
	/* Bit `i` is set when the player's file sets key `i`. */
	unsigned int player_mask;
	int has_mod;
	/* The player turned `enabled` on or off, and it isn't saved yet. */
	int dirty;
};

/* Sets key `key` of the struct `out` from `node`. When the value is wrong,
   it returns 0, leaves `out` unchanged and writes what the key expects in
   `expected`, such as "true or false". */
int XwaOptionKey_Read(const XwaOptionKey* key, const AeronConfigNode* node, void* out, char* expected,
					  size_t expected_size);

/* Reads the section of the shipped file, with `required`, which also resets
   the section, or of the player's file. */
int XwaOptionSection_ReadConfig(XwaOptionSection* section, const AeronConfigFile* config, int required,
								char* error, size_t error_size);

/* Reads the section from `root`, the root of the mod file, over the shipped
   values. Returns 1 when the mod file has the section. */
int XwaOptionSection_ReadMod(XwaOptionSection* section, const AeronConfigNode* root);
/* Loads the mod file and reads the section from it. */
void XwaOptionSection_LoadMod(XwaOptionSection* section);

/* Writes the values in use in `out`. */
void XwaOptionSection_Get(const XwaOptionSection* section, void* out);

/* Saves `<section>.enabled` in the player's config.yaml. */
typedef int (*XwaOptionPersistFn)(const char* key, int value, char* error, size_t error_size);
void XwaOptionSection_SetPersist(XwaOptionPersistFn persist);

/* The player turns the section's `enabled` key on or off. FlushAll saves the
   changes. */
void XwaOptionSection_SetEnabled(XwaOptionSection* section, int enabled);
int XwaOptionSection_FlushAll(void);

#ifdef __cplusplus
}
#endif

#endif
