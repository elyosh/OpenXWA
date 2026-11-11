#include "keyboard_config.h"

#include "aeron/aeron.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool ConfigError(char* error, size_t capacity, const char* format, ...) {
	if (error && capacity) {
		va_list args;
		va_start(args, format);
		vsnprintf(error, capacity, format, args);
		va_end(args);
	}
	return false;
}

static const char* const modifier_names[] = { "shift", "ctrl", "alt", "gui" };

static bool ReadSource(const AeronConfigNode* node, AeronKeyChord* source, char* error, size_t capacity) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP)
		return ConfigError(error, capacity, "keyboard source must be a key mapping");
	for (size_t i = 0; i < AeronConfigNode_MapCount(node); ++i) {
		const char* name = AeronConfigNode_MapKeyAt(node, i);
		if (strcmp(name, "key") && strcmp(name, "modifiers"))
			return ConfigError(error, capacity, "unknown keyboard source field '%s'", name);
	}
	const char* key = AeronConfigNode_String(AeronConfigNode_MapGet(node, "key"), NULL);
	AeronKey parsed;
	if (!key || !AeronKey_FromName(key, &parsed))
		return ConfigError(error, capacity, "unknown keyboard key '%s'", key ? key : "");
	*source = (AeronKeyChord) { .key = (uint16_t)parsed };
	const AeronConfigNode* mods = AeronConfigNode_MapGet(node, "modifiers");
	if (mods && AeronConfigNode_Type(mods) != AERON_CONFIG_SEQUENCE)
		return ConfigError(error, capacity, "keyboard modifiers must be a sequence");
	for (size_t i = 0; i < AeronConfigNode_SequenceCount(mods); ++i) {
		const char* name = AeronConfigNode_String(AeronConfigNode_SequenceGet(mods, i), NULL);
		int bit = 0;
		for (; bit < 4; ++bit)
			if (name && !strcmp(name, modifier_names[bit]))
				break;
		if (bit == 4 || (source->modifiers & (1u << bit)))
			return ConfigError(error, capacity, "invalid or repeated keyboard modifier '%s'",
							   name ? name : "");
		source->modifiers |= (uint8_t)(1u << bit);
	}
	return true;
}

static bool AddSource(XwaKeyboardBindings* profile, XwaInputAction action, AeronKeyChord source, char* error,
					  size_t capacity) {
	if (!XwaKeyboardMapping_SourceValid(source))
		return ConfigError(error, capacity, "keyboard source '%s' is reserved or unsupported",
						   AeronKey_Name((AeronKey)source.key));
	if (XwaKeyboardMapping_Find(profile, source) != SIZE_MAX)
		return ConfigError(error, capacity, "keyboard source '%s' is bound twice",
						   AeronKey_Name((AeronKey)source.key));
	if (profile->count == XWA_KEYBOARD_BINDING_CAP)
		return ConfigError(error, capacity, "keyboard binding capacity exceeded");
	profile->bindings[profile->count++] = (XwaKeyboardBinding) { source, action };
	return true;
}

bool XwaKeyboardConfig_Read(const AeronConfigFile* document, XwaKeyboardBindings* profile, char* error,
							size_t capacity) {
	memset(profile, 0, sizeof *profile);
	const AeronConfigNode* map = AeronConfigFile_GetNode(document, "input.keyboard");
	if (AeronConfigNode_Type(map) != AERON_CONFIG_MAP)
		return ConfigError(error, capacity, "input.keyboard must be a mapping");
	for (size_t i = 0; i < AeronConfigNode_MapCount(map); ++i) {
		const char* name = AeronConfigNode_MapKeyAt(map, i);
		const XwaInputAction action = XwaInputActions_FromName(name);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(map, i);
		/* An action from another version, such as a removed one, is skipped.
		 * The next save drops it. */
		if (!XwaInputActions_KeyboardBindable(action)) {
			Aeron_LogWarn("xwa.config", "unknown keyboard action '%s' is ignored", name);
			continue;
		}
		if (AeronConfigNode_Type(value) == AERON_CONFIG_STRING) {
			AeronKey key;
			const char* label = AeronConfigNode_String(value, NULL);
			if (!AeronKey_FromName(label, &key))
				return ConfigError(error, capacity, "unknown keyboard key '%s'", label);
			if (!AddSource(profile, action, (AeronKeyChord) { .key = (uint16_t)key }, error, capacity))
				return false;
		} else if (AeronConfigNode_Type(value) == AERON_CONFIG_SEQUENCE) {
			for (size_t j = 0; j < AeronConfigNode_SequenceCount(value); ++j) {
				AeronKeyChord source;
				if (!ReadSource(AeronConfigNode_SequenceGet(value, j), &source, error, capacity) ||
					!AddSource(profile, action, source, error, capacity))
					return false;
			}
		} else {
			return ConfigError(error, capacity, "keyboard action '%s' requires a key name or sequence", name);
		}
	}
	XwaKeyboardMapping_Sort(profile);
	return true;
}

typedef struct SourceYaml {
	AeronConfigMapValue fields[2];
	AeronConfigValue key;
	AeronConfigValue modifiers;
	AeronConfigValue modifier_values[4];
} SourceYaml;

bool XwaKeyboardConfig_Write(AeronConfigFile* document, const XwaKeyboardBindings* profile,
							 AeronConfigError* error) {
	if (profile->count > XWA_KEYBOARD_BINDING_CAP) {
		snprintf(error->message, sizeof error->message, "keyboard binding capacity exceeded");
		return false;
	}
	for (size_t i = 0; i < profile->count; ++i) {
		if (!XwaInputActions_KeyboardBindable(profile->bindings[i].action) ||
			!XwaKeyboardMapping_SourceValid(profile->bindings[i].source)) {
			snprintf(error->message, sizeof error->message, "invalid keyboard action");
			return false;
		}
	}
	SourceYaml* scratch = calloc(XWA_KEYBOARD_BINDING_CAP, sizeof *scratch);
	if (!scratch) {
		snprintf(error->message, sizeof error->message, "keyboard serialization allocation failed");
		return false;
	}
	AeronConfigValue sources[XWA_KEYBOARD_BINDING_CAP];
	AeronConfigValue actions[XWA_INPUT_ACTION_COUNT - 1];
	AeronConfigMapValue entries[XWA_INPUT_ACTION_COUNT - 1];
	size_t used = 0, entry_count = 0;
	for (int action = XWA_INPUT_ACTION_NONE + 1; action < XWA_INPUT_ACTION_COUNT; ++action) {
		if (!XwaInputActions_KeyboardBindable((XwaInputAction)action))
			continue;
		size_t first = used;
		for (size_t i = 0; i < profile->count; ++i) {
			const XwaKeyboardBinding* b = &profile->bindings[i];
			if ((int)b->action != action)
				continue;
			if (used == XWA_KEYBOARD_BINDING_CAP) {
				free(scratch);
				snprintf(error->message, sizeof error->message, "keyboard binding capacity exceeded");
				return false;
			}
			SourceYaml* s = &scratch[used];
			s->key = (AeronConfigValue) { .type = AERON_CONFIG_STRING,
										  .value.string_value = AeronKey_Name((AeronKey)b->source.key) };
			size_t mods = 0;
			for (int bit = 0; bit < 4; ++bit)
				if (b->source.modifiers & (1u << bit))
					s->modifier_values[mods++] =
						(AeronConfigValue) { .type = AERON_CONFIG_STRING,
											 .value.string_value = modifier_names[bit] };
			s->modifiers = (AeronConfigValue) { .type = AERON_CONFIG_SEQUENCE,
												.value.sequence = { s->modifier_values, mods } };
			s->fields[0] = (AeronConfigMapValue) { "key", &s->key };
			s->fields[1] = (AeronConfigMapValue) { "modifiers", &s->modifiers };
			sources[used++] =
				(AeronConfigValue) { .type = AERON_CONFIG_MAP, .value.map = { s->fields, mods ? 2 : 1 } };
		}
		actions[action - 1] = (AeronConfigValue) { .type = AERON_CONFIG_SEQUENCE,
												   .value.sequence = { sources + first, used - first } };
		entries[entry_count++] =
			(AeronConfigMapValue) { XwaInputActions_ToName((XwaInputAction)action), &actions[action - 1] };
	}
	AeronConfigValue value = { .type = AERON_CONFIG_MAP, .value.map = { entries, entry_count } };
	bool ok = AeronConfigFile_SetValue(document, "input.keyboard", &value, error) != 0;
	free(scratch);
	return ok;
}

bool XwaKeyboardConfig_Resolve(const XwaKeyboardBindings* defaults, const AeronConfigFile* user,
							   XwaKeyboardBindings* effective, char* error, size_t capacity) {
	if (!AeronConfigFile_Has(user, "input.keyboard")) {
		*effective = *defaults;
		return true;
	}
	if (!XwaKeyboardConfig_Read(user, effective, error, capacity))
		return false;
	const AeronConfigNode* overrides = AeronConfigFile_GetNode(user, "input.keyboard");
	for (size_t i = 0; i < defaults->count; ++i) {
		const XwaKeyboardBinding* binding = &defaults->bindings[i];
		/* Explicit action lists replace their defaults, including empty lists.
		 * Explicit sources also displace matching sources from inherited actions. */
		if (AeronConfigNode_MapGet(overrides, XwaInputActions_ToName(binding->action)) ||
			XwaKeyboardMapping_Find(effective, binding->source) != SIZE_MAX)
			continue;
		if (!AddSource(effective, binding->action, binding->source, error, capacity))
			return false;
	}
	XwaKeyboardMapping_Sort(effective);
	return true;
}
