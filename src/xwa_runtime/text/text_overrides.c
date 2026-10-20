#include "xwa_runtime/text/text_overrides.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

typedef struct XwaTextEntry {
	char* id; /* in upper case, as Linez_ResolveString compares them */
	char* text;
} XwaTextEntry;

static const char s_stringsPath[] = "text/strings.yaml";
/* Sorted by ID. */
static XwaTextEntry* s_entries;
static size_t s_count;
static size_t s_capacity;

/* Compares `id`, which can be in any case and end with '!', with the upper
   case ID `stored`. */
static int XwaText_CompareId(const char* id, const char* stored) {
	size_t i;

	for (i = 0;; ++i) {
		const int left = id[i] == '!' ? 0 : toupper((unsigned char)id[i]);
		const int right = (unsigned char)stored[i];

		if (left != right) {
			return left - right;
		}
		if (left == 0) {
			return 0;
		}
	}
}

/* Returns the position of `id` in s_entries, or the position where it would
   go, and sets `*found`. */
static size_t XwaText_Search(const char* id, int* found) {
	size_t low = 0;
	size_t high = s_count;

	while (low < high) {
		const size_t middle = low + (high - low) / 2;
		const int order = XwaText_CompareId(id, s_entries[middle].id);

		if (order == 0) {
			*found = 1;
			return middle;
		}
		if (order < 0) {
			high = middle;
		} else {
			low = middle + 1;
		}
	}
	*found = 0;
	return low;
}

static char* XwaText_Copy(const char* text) {
	const size_t size = strlen(text) + 1;
	char* copy = (char*)malloc(size);

	if (copy != NULL) {
		memcpy(copy, text, size);
	}
	return copy;
}

int XwaText_Set(const char* id, const char* text) {
	char* idCopy;
	char* textCopy;
	size_t index;
	size_t i;
	int found;

	if (id == NULL || id[0] == '\0' || strchr(id, '!') != NULL || text == NULL) {
		return 0;
	}
	textCopy = XwaText_Copy(text);
	if (textCopy == NULL) {
		return 0;
	}
	index = XwaText_Search(id, &found);
	if (found) {
		/* The old text stays allocated, because the game may hold it. */
		s_entries[index].text = textCopy;
		return 1;
	}
	if (s_count == s_capacity) {
		const size_t capacity = s_capacity != 0 ? s_capacity * 2 : 256;
		XwaTextEntry* entries = (XwaTextEntry*)realloc(s_entries, capacity * sizeof *entries);

		if (entries == NULL) {
			free(textCopy);
			return 0;
		}
		s_entries = entries;
		s_capacity = capacity;
	}
	idCopy = XwaText_Copy(id);
	if (idCopy == NULL) {
		free(textCopy);
		return 0;
	}
	for (i = 0; idCopy[i] != '\0'; ++i) {
		idCopy[i] = (char)toupper((unsigned char)idCopy[i]);
	}
	memmove(&s_entries[index + 1], &s_entries[index], (s_count - index) * sizeof *s_entries);
	s_entries[index].id = idCopy;
	s_entries[index].text = textCopy;
	++s_count;
	return 1;
}

char* XwaText_Find(const char* id) {
	size_t index;
	int found;

	if (s_count == 0 || id == NULL) {
		return NULL;
	}
	index = XwaText_Search(id, &found);
	return found ? s_entries[index].text : NULL;
}

/* Latin-1 holds the characters up to U+00FF, which UTF-8 writes as one byte
   below 0x80, or as 0xC2 or 0xC3 followed by a byte from 0x80 to 0xBF. */
int XwaText_ToLatin1(char* text) {
	const unsigned char* read = (const unsigned char*)text;
	char* write = text;

	while (*read != 0) {
		if (*read < 0x80) {
			*write++ = (char)*read++;
		} else if ((read[0] == 0xC2 || read[0] == 0xC3) && read[1] >= 0x80 && read[1] <= 0xBF) {
			*write++ = (char)(((read[0] & 0x1F) << 6) | (read[1] & 0x3F));
			read += 2;
		} else {
			return 0;
		}
	}
	*write = '\0';
	return 1;
}

void XwaText_Load(void) {
	AeronConfigError error;
	AeronConfigFile* file = NULL;
	const AeronConfigNode* root;
	const AeronConfigNode* strings;
	size_t index;
	size_t count = 0;

	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, s_stringsPath, &file, &error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.text", "%s:%d: %s. The file is ignored", s_stringsPath, error.line,
						   error.message);
		}
		return;
	}
	root = AeronConfigFile_Root(file);
	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1) {
		Aeron_LogError("xwa.text", "%s: the file needs 'version: 1'. It is ignored", s_stringsPath);
		AeronConfigFile_Destroy(file);
		return;
	}
	strings = AeronConfigNode_MapGet(root, "strings");
	if (strings != NULL && AeronConfigNode_Type(strings) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.text", "%s: 'strings' isn't a map of IDs to texts. It is ignored", s_stringsPath);
		strings = NULL;
	}
	for (index = 0; index < AeronConfigNode_MapCount(strings); ++index) {
		const char* id = AeronConfigNode_MapKeyAt(strings, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(strings, index);
		const char* text = AeronConfigNode_String(value, NULL);
		char* latin1;
		int found;

		if (text == NULL) {
			Aeron_LogWarn("xwa.text", "%s:%d: the value of %s isn't text. Put it in quotes. It is ignored",
						  s_stringsPath, AeronConfigNode_Line(value), id);
			continue;
		}
		if (id[0] == '\0' || strchr(id, '!') != NULL) {
			Aeron_LogWarn("xwa.text",
						  "%s:%d: '%s' isn't an ID: IDs can't be empty or have a '!'. It is ignored",
						  s_stringsPath, AeronConfigNode_Line(value), id);
			continue;
		}
		latin1 = XwaText_Copy(text);
		if (latin1 == NULL) {
			Aeron_LogError("xwa.text", "%s: out of memory. The rest of the file is ignored", s_stringsPath);
			break;
		}
		if (!XwaText_ToLatin1(latin1)) {
			Aeron_LogWarn("xwa.text",
						  "%s:%d: the text of %s has a character that isn't in Latin-1, so the game can't "
						  "show it. It is ignored",
						  s_stringsPath, AeronConfigNode_Line(value), id);
			free(latin1);
			continue;
		}
		XwaText_Search(id, &found);
		if (found) {
			Aeron_LogWarn("xwa.text", "%s:%d: %s is set more than once. The last text is used", s_stringsPath,
						  AeronConfigNode_Line(value), id);
		}
		if (!XwaText_Set(id, latin1)) {
			free(latin1);
			Aeron_LogError("xwa.text", "%s: out of memory. The rest of the file is ignored", s_stringsPath);
			break;
		}
		free(latin1);
		if (!found) {
			++count;
		}
	}
	AeronConfigFile_Destroy(file);
	Aeron_LogInfo("xwa.text", "%s: %zu texts", s_stringsPath, count);
}
