#include "xwa_runtime/models/object_profiles.h"

#include "aeron/aeron.h"
#include "aeron/log.h"
#include "aeron/vfs.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/flight/object/object.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/util/memory.h"
#include "xwa/util/string.h"
#include "xwa_runtime/models/model_roles.h"
#include "xwa_runtime/models/opt_synth.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. It selects each object's
   profile from the mission's ObjectProfile_* lines; the profiles themselves
   are component tags written by the model converter. */

/* The skirmish test reads header.missionType, at its original offset. */
typedef char object_profiles_mission_type_offset
	[(offsetof(FrontendMission, header.missionType) == 0xB1B82) ? 1 : -1];

#define OBJECT_PROFILE_TAG_PREFIX "hidden_by_profile."
#define OBJECT_PROFILE_KEY_PREFIX "ObjectProfile_"
/* AERON_FLIGHT_TAG_MAX minus its terminator. */
#define OBJECT_PROFILE_TAG_LENGTH_MAX 63

enum { PROFILE_SECTION_OBJECTS, PROFILE_SECTION_HANGAR_OBJECTS, PROFILE_SECTION_COUNT };

static const char* const s_sectionNames[PROFILE_SECTION_COUNT] = { "Objects", "HangarObjects" };

typedef struct ObjectProfileLine {
	char* key;
	char* value;
} ObjectProfileLine;

/* The ObjectProfile_* lines of one section, in file order. */
typedef struct ObjectProfileLines {
	ObjectProfileLine* lines;
	int count;
} ObjectProfileLines;

/* FlightModels\<section>.txt or FlightModels\default.ini. */
static ObjectProfileLines s_startup[PROFILE_SECTION_COUNT];
/* The current mission's own lines; a section without them uses s_startup. */
static ObjectProfileLines s_mission[PROFILE_SECTION_COUNT];
static int s_missionHasSection[PROFILE_SECTION_COUNT];

static void ObjectProfiles_Clear(ObjectProfileLines* lines) {
	int index;

	for (index = 0; index < lines->count; ++index) {
		free(lines->lines[index].key);
		free(lines->lines[index].value);
	}
	free(lines->lines);
	lines->lines = NULL;
	lines->count = 0;
}

static int ObjectProfiles_IsSpace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* The span of [begin, end) without leading and trailing whitespace. */
static void ObjectProfiles_Trim(const char** begin, const char** end) {
	while (*begin < *end && ObjectProfiles_IsSpace(**begin)) {
		++*begin;
	}
	while (*end > *begin && ObjectProfiles_IsSpace((*end)[-1])) {
		--*end;
	}
}

static char* ObjectProfiles_Copy(const char* begin, const char* end) {
	char* copy = (char*)malloc((size_t)(end - begin) + 1);

	if (copy != NULL) {
		memcpy(copy, begin, (size_t)(end - begin));
		copy[end - begin] = '\0';
	}
	return copy;
}

static char ObjectProfiles_Lower(char c) {
	return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static int ObjectProfiles_HasKeyPrefix(const char* begin, const char* end) {
	const char* prefix;

	for (prefix = OBJECT_PROFILE_KEY_PREFIX; *prefix != '\0'; ++prefix, ++begin) {
		if (begin == end || ObjectProfiles_Lower(*begin) != ObjectProfiles_Lower(*prefix)) {
			return 0;
		}
	}
	return 1;
}

/* Reads the lines of [section] in `path`, or of the whole file when
   `section` is "" (a header line other than "[]" still stops it). Lines are
   trimmed; blank lines and lines starting with '#', ';' or "//" are skipped.
   Appends the ObjectProfile_* key lines to `out` and returns how many lines
   the section has in all, which decides whether this source is used. */
static int ObjectProfiles_ReadSource(const char* path, const char* section, ObjectProfileLines* out) {
	uint8_t* data;
	size_t size;
	const char* cursor;
	const char* end;
	int reading;
	int lineCount;

	if (!AeronVfs_ReadAll(Aeron_GetVfs(), AERON_VFS_ROOT_ASSET, path, 0, &data, &size)) {
		return 0;
	}
	reading = section[0] == '\0';
	lineCount = 0;
	cursor = (const char*)data;
	end = cursor + size;
	while (cursor < end) {
		const char* lineEnd = (const char*)memchr(cursor, '\n', (size_t)(end - cursor));
		const char* next = lineEnd != NULL ? lineEnd + 1 : end;
		const char* begin = cursor;
		const char* equals;

		if (lineEnd == NULL) {
			lineEnd = end;
		}
		cursor = next;
		ObjectProfiles_Trim(&begin, &lineEnd);
		if (begin == lineEnd || *begin == '#' || *begin == ';' ||
			(lineEnd - begin >= 2 && begin[0] == '/' && begin[1] == '/')) {
			continue;
		}
		if (*begin == '[' && lineEnd[-1] == ']') {
			/* A one-character line cannot be both '[' and ']'. */
			char* name = ObjectProfiles_Copy(begin + 1, lineEnd - 1);

			reading = name != NULL && Xwa_Stricmp(name, section) == 0;
			free(name);
			continue;
		}
		if (!reading) {
			continue;
		}
		++lineCount;
		equals = (const char*)memchr(begin, '=', (size_t)(lineEnd - begin));
		if (equals != NULL) {
			const char* keyBegin = begin;
			const char* keyEnd = equals;
			const char* valueBegin = equals + 1;
			const char* valueEnd = lineEnd;

			ObjectProfiles_Trim(&keyBegin, &keyEnd);
			ObjectProfiles_Trim(&valueBegin, &valueEnd);
			if (ObjectProfiles_HasKeyPrefix(keyBegin, keyEnd)) {
				ObjectProfileLine* grown =
					(ObjectProfileLine*)realloc(out->lines, (size_t)(out->count + 1) * sizeof *out->lines);
				char* key = ObjectProfiles_Copy(keyBegin, keyEnd);
				char* value = ObjectProfiles_Copy(valueBegin, valueEnd);

				if (grown != NULL) {
					out->lines = grown;
				}
				if (grown == NULL || key == NULL || value == NULL) {
					free(key);
					free(value);
					Aeron_LogError("xwa.models", "out of memory reading object profiles from '%s'", path);
					break;
				}
				out->lines[out->count].key = key;
				out->lines[out->count].value = value;
				++out->count;
			}
		}
	}
	free(data);
	return lineCount;
}

/* Reads `txtPath` as a whole, else [section] of `iniPath`: the first that
   has any line wins. Returns nonzero when either has lines. */
static int ObjectProfiles_ReadSources(const char* txtPath, const char* iniPath, const char* section,
									  ObjectProfileLines* out) {
	ObjectProfiles_Clear(out);
	if (ObjectProfiles_ReadSource(txtPath, "", out) > 0) {
		return 1;
	}
	ObjectProfiles_Clear(out);
	if (ObjectProfiles_ReadSource(iniPath, section, out) > 0) {
		return 1;
	}
	ObjectProfiles_Clear(out);
	return 0;
}

void XwaObjectProfiles_Init(void) {
	int section;

	for (section = 0; section < PROFILE_SECTION_COUNT; ++section) {
		char txtPath[64];

		snprintf(txtPath, sizeof txtPath, "FlightModels\\%s.txt", s_sectionNames[section]);
		ObjectProfiles_ReadSources(txtPath, "FlightModels\\default.ini", s_sectionNames[section],
								   &s_startup[section]);
	}
}

void XwaObjectProfiles_LoadMission(const char* missionPath) {
	char base[260];
	char* dot;
	int section;

	snprintf(base, sizeof base, "%s", missionPath != NULL ? missionPath : "");
	/* Strip the extension. */
	dot = strrchr(base, '.');
	if (dot != NULL) {
		*dot = '\0';
	}
	for (section = 0; section < PROFILE_SECTION_COUNT; ++section) {
		char txtPath[300];
		char iniPath[300];

		snprintf(txtPath, sizeof txtPath, "%s_%s.txt", base, s_sectionNames[section]);
		snprintf(iniPath, sizeof iniPath, "%s.ini", base);
		s_missionHasSection[section] =
			missionPath != NULL &&
			ObjectProfiles_ReadSources(txtPath, iniPath, s_sectionNames[section], &s_mission[section]);
		if (s_missionHasSection[section] && s_mission[section].count > 0) {
			Aeron_LogInfo("xwa.models", "mission '%s': %d [%s] object profile selections", missionPath,
						  s_mission[section].count, s_sectionNames[section]);
		}
	}
}

/* The value of the first line whose key is `key`, case-insensitively. */
static const char* ObjectProfiles_Value(const ObjectProfileLines* lines, const char* key) {
	int index;

	for (index = 0; index < lines->count; ++index) {
		if (Xwa_Stricmp(lines->lines[index].key, key) == 0) {
			return lines->lines[index].value;
		}
	}
	return "";
}

/* The components carrying profile `profile`'s tag: the lower-case name, with
   every byte outside [a-z0-9_] written as '.' and two hex digits, as the
   model converter writes it. */
static int ObjectProfiles_Collect(uint16_t objectType, const char* profile, int* components, int capacity) {
	char tag[OBJECT_PROFILE_TAG_LENGTH_MAX + 1];
	size_t length;
	const unsigned char* cursor;

	length = strlen(OBJECT_PROFILE_TAG_PREFIX);
	memcpy(tag, OBJECT_PROFILE_TAG_PREFIX, length);
	for (cursor = (const unsigned char*)profile; *cursor != '\0'; ++cursor) {
		unsigned char c = *cursor;

		c = (unsigned char)ObjectProfiles_Lower((char)c);
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
			if (length + 1 > OBJECT_PROFILE_TAG_LENGTH_MAX) {
				return 0;
			}
			tag[length++] = (char)c;
		} else {
			if (length + 3 > OBJECT_PROFILE_TAG_LENGTH_MAX) {
				return 0;
			}
			snprintf(tag + length, 4, ".%02x", c);
			length += 3;
		}
	}
	tag[length] = '\0';
	return XwaModelRole_CollectTag(objectType, tag, components, capacity);
}

/* The replacement's model key (its GLB name), or 0 for a stock model. */
static int ObjectProfiles_ModelKey(uint16_t objectType, char* key, size_t keySize) {
	MemoryHandle handle;
	const XwaOptSynthData* synth;

	handle = objectType < OBJ_Count ? g_loadedModels.byObjectType[objectType] : 0;
	if (handle == 0) {
		return 0;
	}
	synth = OptSynth_GetModelData((const OptimizedPolyObject*)Memory_LockHandle(handle));
	if (synth != NULL) {
		snprintf(key, keySize, "%s", synth->key);
	}
	Memory_UnlockHandle(handle);
	return synth != NULL;
}

void XwaObjectProfiles_Apply(uint16_t objectType, int flightGroupIdx, uint8_t markings, int hangar,
							 int noMission, struct CraftData* craft) {
	const int section = hangar ? PROFILE_SECTION_HANGAR_OBJECTS : PROFILE_SECTION_OBJECTS;
	const ObjectProfileLines* lines;
	char modelKey[64];
	char key[96];
	char profile[256];
	int components[XWA_OPT_SYNTH_MAX_COMPONENTS];
	int count;
	int index;

	if (craft == NULL || !ObjectProfiles_ModelKey(objectType, modelKey, sizeof modelKey)) {
		return;
	}
	lines = !noMission && s_missionHasSection[section] ? &s_mission[section] : &s_startup[section];

	/* The flight group's profile, then the model's. */
	snprintf(key, sizeof key, OBJECT_PROFILE_KEY_PREFIX "fg_%d", flightGroupIdx);
	snprintf(profile, sizeof profile, "%s", ObjectProfiles_Value(lines, key));
	if (profile[0] == '\0') {
		snprintf(key, sizeof key, OBJECT_PROFILE_KEY_PREFIX "%s", modelKey);
		snprintf(profile, sizeof profile, "%s", ObjectProfiles_Value(lines, key));
	}
	/* In a quick start or skirmish, Skirmish when the model has it. */
	if (profile[0] == '\0' && g_frontendMission != NULL &&
		(g_frontendMission->header.missionType == XWA_MISSION_TYPE_QUICK_START ||
		 g_frontendMission->header.missionType == XWA_MISSION_TYPE_SKIRMISH) &&
		ObjectProfiles_Collect(objectType, "Skirmish", components, XWA_OPT_SYNTH_MAX_COMPONENTS) > 0) {
		snprintf(profile, sizeof profile, "Skirmish");
	}
	if (profile[0] == '\0') {
		snprintf(profile, sizeof profile, "Default_%u", (unsigned int)markings);
	}

	/* A profile that hides nothing (undefined, empty or "-1") uses Default. */
	count = ObjectProfiles_Collect(objectType, profile, components, XWA_OPT_SYNTH_MAX_COMPONENTS);
	if (count == 0) {
		count = ObjectProfiles_Collect(objectType, "Default", components, XWA_OPT_SYNTH_MAX_COMPONENTS);
	}
	for (index = 0; index < count; ++index) {
		craft->componentHp[components[index]] = 0;
		craft->componentState[components[index]] = 4;
	}
}
