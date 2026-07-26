#include "xwa/assets/linez.h"

#include "xwa/assets/file_io.h"
#include "xwa/util/byte_order.h"
#include "xwa/util/memory.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define LINEZ_ENCRYPTED_MAGIC 0x454e4352u

// GLOBAL: XWA 0x7829C0
char* g_linezTextBuffer = NULL;
// GLOBAL: XWA 0x7829BC
char** g_linezDict = NULL;
// GLOBAL: XWA 0x7829C4
int g_linezDictCount = 0;

#ifdef XWA_MODERN
static void Linez_UppercaseInPlace(char* text) {
	while (*text != '\0') {
		*text = (char)toupper((unsigned char)*text);
		++text;
	}
}
#endif

static int Linez_IsLineBreak(char ch) { return ch == '\r' || ch == '\n'; }

static int Linez_CountLines(const char* text, int size) {
	const char* cursor = text;
	const char* end = text + size;
	int count = 0;

	if (size <= 0) {
		return 0;
	}

	do {
		if (cursor < end) {
			while (cursor < end && !Linez_IsLineBreak(*cursor)) {
				++cursor;
			}

			while (cursor < end && Linez_IsLineBreak(*cursor)) {
				++cursor;
			}
		}

		++count;
	} while (cursor < end - 1);

	return count;
}

// FUNCTION: XWA 0x52AA20
int Linez_LoadDict(char* fileName) {
	XwaFile* stream;
	int fileSize;
	unsigned char header[4] = { 0 };
	int i;
	int entryIndex = 0;
	char* cursor;
	char* end;

	g_linezDictCount = 0;

	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	if (stream == NULL) {
		return 0;
	}

	File_ReadCount(stream, header, sizeof(header));
	fileSize = File_GetSize(stream);
	File_Seek(stream, 0, SEEK_SET);

	g_linezTextBuffer = (char*)Mem_Alloc((size_t)fileSize + 1);
	if (g_linezTextBuffer == NULL) {
		File_Close(stream);
		return 0;
	}

	if (!File_ReadCount(stream, g_linezTextBuffer, (size_t)fileSize)) {
		File_Close(stream);
		return 0;
	}

	File_Close(stream);

	if (fileSize >= 4 && ByteOrder_ReadU32Le(header) == LINEZ_ENCRYPTED_MAGIC) {
		fileSize -= 4;
		for (i = 0; i < fileSize; ++i) {
			g_linezTextBuffer[i] = (char)(g_linezTextBuffer[i + 4] ^ 0xdd);
		}
	}

	g_linezTextBuffer[fileSize] = '\0';
	g_linezDictCount = Linez_CountLines(g_linezTextBuffer, fileSize);
	if (g_linezDictCount <= 0) {
		return 0;
	}

	g_linezDict = (char**)Mem_Alloc(sizeof(char*) * (size_t)g_linezDictCount);
	if (g_linezDict == NULL) {
		return 0;
	}

	cursor = g_linezTextBuffer;
	end = g_linezTextBuffer + fileSize;

	do {
		char* lineEnd = cursor;
		char* tab;

		while (lineEnd < end && !Linez_IsLineBreak(*lineEnd)) {
			++lineEnd;
		}

		if (lineEnd < end) {
			do {
				*lineEnd++ = '\0';
			} while (lineEnd < end && Linez_IsLineBreak(*lineEnd));
		}

		tab = strchr(cursor, '\t');
		if (tab != NULL) {
			*tab = '\0';
		}

#ifdef XWA_MODERN
		Linez_UppercaseInPlace(cursor);
#else
		strupr(cursor);
#endif
		g_linezDict[entryIndex++] = cursor;
		cursor = lineEnd;
	} while (cursor < end - 1 && entryIndex < g_linezDictCount);

	g_linezDictCount = entryIndex;
	qsort(g_linezDict, (size_t)g_linezDictCount, sizeof(char*), Linez_CompareCachedEntries);

	return 1;
}

// FUNCTION: XWA 0x52ABD0
int Linez_CompareCachedEntries(const void* left, const void* right) {
	const char* const* leftEntry = (const char* const*)left;
	const char* const* rightEntry = (const char* const*)right;

	return strcmp(*leftEntry, *rightEntry);
}

// FUNCTION: XWA 0x52AC10
void Linez_FreeDict(void) {
	if (g_linezTextBuffer != NULL) {
		Mem_Free(g_linezTextBuffer);
		g_linezTextBuffer = NULL;
	}

	if (g_linezDict != NULL) {
		Mem_Free(g_linezDict);
		g_linezDict = NULL;
	}

	g_linezDictCount = 0;
}

// FUNCTION: XWA 0x52AC40
char* Linez_ResolveString(char* key) {
	char* defaultText;
	char lookup[256];
	char* lookupPtr = lookup;
	char* closingBang;
	char** found;

	if (key == NULL) {
		return key;
	}

	if (key[0] == '\0' || key[0] != '!') {
		return key;
	}

	defaultText = strchr(key + 1, '!');
	++defaultText;
	if (g_linezTextBuffer == NULL) {
		return defaultText;
	}

	strncpy(lookup, key + 1, 254);
#ifdef XWA_MODERN
	lookup[254] = '\0';
#endif
	closingBang = strchr(lookup, '!');
	if (closingBang != NULL) {
		*closingBang = '\0';
	}

#ifdef XWA_MODERN
	Linez_UppercaseInPlace(lookup);
#else
	strupr(lookup);
#endif

	found = (char**)bsearch(&lookupPtr, g_linezDict, (size_t)g_linezDictCount, sizeof(char*),
							Linez_CompareCachedEntries);
	if (found == NULL) {
		return defaultText;
	}

	return *found + strlen(*found) + 1;
}

// FUNCTION: XWA 0x52AD20
int Linez_IsLoaded(void) { return g_linezTextBuffer != NULL; }
