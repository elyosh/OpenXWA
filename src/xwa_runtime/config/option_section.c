#include "xwa_runtime/config/option_section.h"

#include "aeron/aeron.h"
#include "aeron/log.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

enum { OPTION_MAX_SECTIONS = 16, OPTION_MAX_KEYS = 32 };

/* The sections read from the shipped file, which FlushAll saves. */
static XwaOptionSection* s_sections[OPTION_MAX_SECTIONS];
static size_t s_sectionCount;
static XwaOptionPersistFn s_persist;

static size_t option_field_size(const XwaOptionKey* key) {
	switch (key->kind) {
		case XWA_OPTION_KIND_NUMBER:
			return sizeof(float);
		case XWA_OPTION_KIND_COLOR:
			return 3 * sizeof(float);
		case XWA_OPTION_KIND_SWITCH:
		case XWA_OPTION_KIND_CHOICE:
		default:
			return sizeof(int);
	}
}

static int option_is_number(const AeronConfigNode* node) {
	const AeronConfigNodeType type = AeronConfigNode_Type(node);
	return type == AERON_CONFIG_INT || type == AERON_CONFIG_FLOAT;
}

/* Writes the choice's words as "a, b or c". */
static void option_choice_words(const char* const* words, char* out, size_t out_size) {
	size_t count = 0;
	size_t used = 0;
	size_t i;

	while (words[count]) {
		++count;
	}
	out[0] = '\0';
	for (i = 0; i < count && used < out_size; ++i) {
		const char* separator = i == 0 ? "" : (i + 1 == count ? " or " : ", ");
		const int written = snprintf(out + used, out_size - used, "%s%s", separator, words[i]);
		if (written < 0) {
			return;
		}
		used += (size_t)written;
	}
}

int XwaOptionKey_Read(const XwaOptionKey* key, const AeronConfigNode* node, void* out, char* expected,
					  size_t expected_size) {
	char* field;
	char unused[1];

	if (!expected || expected_size == 0) {
		expected = unused;
		expected_size = sizeof unused;
	}
	expected[0] = '\0';
	if (!key || !out) {
		return 0;
	}
	field = (char*)out + key->offset;
	switch (key->kind) {
		case XWA_OPTION_KIND_SWITCH: {
			int value;

			if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
				snprintf(expected, expected_size, "true or false");
				return 0;
			}
			value = AeronConfigNode_Bool(node, 0) != 0;
			memcpy(field, &value, sizeof value);
			return 1;
		}
		case XWA_OPTION_KIND_NUMBER: {
			const double number = AeronConfigNode_Float(node, NAN);
			float value;

			if (!option_is_number(node) || !(number >= key->min && number <= key->max)) {
				snprintf(expected, expected_size, "a number from %g to %g", (double)key->min,
						 (double)key->max);
				return 0;
			}
			value = (float)number;
			memcpy(field, &value, sizeof value);
			return 1;
		}
		case XWA_OPTION_KIND_COLOR: {
			float value[3];
			size_t i;

			if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE ||
				AeronConfigNode_SequenceCount(node) != 3) {
				snprintf(expected, expected_size, "three numbers of 0 or more");
				return 0;
			}
			for (i = 0; i < 3; ++i) {
				const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, i);
				const double number = AeronConfigNode_Float(item, NAN);

				if (!option_is_number(item) || !isfinite(number) || !(number >= 0.0)) {
					snprintf(expected, expected_size, "three numbers of 0 or more");
					return 0;
				}
				value[i] = (float)number;
			}
			memcpy(field, value, sizeof value);
			return 1;
		}
		case XWA_OPTION_KIND_CHOICE: {
			const char* word =
				AeronConfigNode_Type(node) == AERON_CONFIG_STRING ? AeronConfigNode_String(node, NULL) : NULL;
			int i;

			for (i = 0; word && key->choices[i]; ++i) {
				if (strcmp(word, key->choices[i]) == 0) {
					memcpy(field, &i, sizeof i);
					return 1;
				}
			}
			option_choice_words(key->choices, expected, expected_size);
			return 0;
		}
	}
	return 0;
}

/* The index of the key named `name`, or key_count. */
static size_t option_find_key(const XwaOptionSection* section, const char* name) {
	size_t i;

	for (i = 0; i < section->key_count && strcmp(section->keys[i].name, name) != 0; ++i) {
	}
	return i;
}

static void option_check(const XwaOptionSection* section) {
	void* options;

	if (!section->check) {
		return;
	}
	options = malloc(section->size);
	if (!options) {
		return;
	}
	XwaOptionSection_Get(section, options);
	section->check(options);
	free(options);
}

static int option_error(char* error, size_t error_size, const char* format, ...) {
	if (error && error_size) {
		va_list args;
		va_start(args, format);
		vsnprintf(error, error_size, format, args);
		va_end(args);
	}
	return 0;
}

/* The int value of key `k` in use. */
static int option_current_int(const XwaOptionSection* section, size_t k) {
	const void* source = (section->player_mask & (1u << k))
							 ? section->player
							 : (section->has_mod ? section->mod : section->shipped);
	int value;

	memcpy(&value, (const char*)source + section->keys[k].offset, sizeof value);
	return value;
}

int XwaOptionSection_ReadConfig(XwaOptionSection* section, const AeronConfigFile* config, int required,
								char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, section->name);
	void* out = required ? section->shipped : section->player;
	char key[96];
	char expected[128];
	size_t i;

	if (section->key_count > OPTION_MAX_KEYS) {
		return option_error(error, error_size, "'%s' has too many keys", section->name);
	}
	if (required) {
		size_t k;

		memset(section->player, 0, section->size);
		section->player_mask = 0;
		section->has_mod = 0;
		section->dirty = 0;
		for (k = 0; k < s_sectionCount && s_sections[k] != section; ++k) {
		}
		if (k == s_sectionCount && s_sectionCount < OPTION_MAX_SECTIONS) {
			s_sections[s_sectionCount++] = section;
		}
	}
	if (!node) {
		if (required) {
			return option_error(error, error_size, "missing required remaster/config.yaml section '%s'",
								section->name);
		}
		return 1;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		return option_error(error, error_size, "'%s' must be a mapping in %s", section->name,
							AeronConfigNode_SourcePath(node));
	}
	for (i = 0; i < section->key_count; ++i) {
		const XwaOptionKey* option = &section->keys[i];
		const AeronConfigNode* value = AeronConfigNode_MapGet(node, option->name);

		if (!required && !option->player) {
			continue;
		}
		snprintf(key, sizeof key, "%s.%s", section->name, option->name);
		if (!value) {
			if (required) {
				return option_error(error, error_size, "missing required remaster/config.yaml setting '%s'",
									key);
			}
			continue;
		}
		if (!XwaOptionKey_Read(option, value, out, expected, sizeof expected)) {
			return option_error(error, error_size, "invalid '%s': expected %s", key, expected);
		}
		if (!required) {
			section->player_mask |= 1u << i;
		}
	}
	option_check(section);
	return 1;
}

int XwaOptionSection_ReadMod(XwaOptionSection* section, const AeronConfigNode* root) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(root, section->name);
	const size_t enabled = option_find_key(section, "enabled");
	size_t i;

	section->has_mod = 0;
	if (node == NULL) {
		return 0;
	}
	/* `name:` with nothing after it uses the shipped values. */
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP && AeronConfigNode_Type(node) != AERON_CONFIG_NULL) {
		Aeron_LogError(section->log_channel, "%s:%d: %s must be a map; it is ignored", section->mod_file,
					   AeronConfigNode_Line(node), section->name);
		return 0;
	}
	memcpy(section->mod, section->shipped, section->size);
	if (enabled < section->key_count) {
		const int on = 1;
		memcpy((char*)section->mod + section->keys[enabled].offset, &on, sizeof on);
	}
	for (i = 0; i < AeronConfigNode_MapCount(node); ++i) {
		const char* name = AeronConfigNode_MapKeyAt(node, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, i);
		const size_t k = option_find_key(section, name);
		char expected[128];

		if (k == section->key_count) {
			const char* const* other = section->other_keys;

			while (other && *other && strcmp(*other, name) != 0) {
				++other;
			}
			if (!other || !*other) {
				Aeron_LogWarn(section->log_channel, "%s:%d: %s.%s isn't a known setting; it is ignored",
							  section->mod_file, AeronConfigNode_Line(value), section->name, name);
			}
			continue;
		}
		if (!XwaOptionKey_Read(&section->keys[k], value, section->mod, expected, sizeof expected)) {
			Aeron_LogWarn(section->log_channel, "%s:%d: %s.%s must be %s; it is ignored", section->mod_file,
						  AeronConfigNode_Line(value), section->name, name, expected);
		}
	}
	section->has_mod = 1;
	option_check(section);
	return 1;
}

void XwaOptionSection_LoadMod(XwaOptionSection* section) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;

	section->has_mod = 0;
	memset(&error, 0, sizeof error);
	/* The code that reads the rest of the file logs its errors. */
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, section->mod_file, &file, &error)) {
		return;
	}
	XwaOptionSection_ReadMod(section, AeronConfigFile_Root(file));
	AeronConfigFile_Destroy(file);
}

void XwaOptionSection_Get(const XwaOptionSection* section, void* out) {
	size_t i;

	if (!section || !out) {
		return;
	}
	memcpy(out, section->has_mod ? section->mod : section->shipped, section->size);
	for (i = 0; i < section->key_count; ++i) {
		if (section->player_mask & (1u << i)) {
			const size_t offset = section->keys[i].offset;
			memcpy((char*)out + offset, (const char*)section->player + offset,
				   option_field_size(&section->keys[i]));
		}
	}
}

void XwaOptionSection_SetPersist(XwaOptionPersistFn persist) { s_persist = persist; }

void XwaOptionSection_SetEnabled(XwaOptionSection* section, int enabled) {
	const size_t k = option_find_key(section, "enabled");
	const int value = enabled != 0;

	if (k == section->key_count || option_current_int(section, k) == value) {
		return;
	}
	memcpy((char*)section->player + section->keys[k].offset, &value, sizeof value);
	section->player_mask |= 1u << k;
	section->dirty = 1;
}

int XwaOptionSection_FlushAll(void) {
	int ok = 1;
	size_t i;

	for (i = 0; i < s_sectionCount; ++i) {
		XwaOptionSection* section = s_sections[i];
		const size_t k = option_find_key(section, "enabled");
		char key[96];
		char error[512];
		int value;

		if (!section->dirty || k == section->key_count) {
			continue;
		}
		snprintf(key, sizeof key, "%s.enabled", section->name);
		if (!s_persist) {
			Aeron_LogWarn("xwa.config", "%s changed, but nothing saves it", key);
			ok = 0;
			continue;
		}
		memcpy(&value, (const char*)section->player + section->keys[k].offset, sizeof value);
		error[0] = '\0';
		if (!s_persist(key, value, error, sizeof error)) {
			Aeron_LogError("xwa.config", "%s", error[0] ? error : "could not save a switch to config.yaml");
			ok = 0;
			continue;
		}
		section->dirty = 0;
	}
	return ok;
}
