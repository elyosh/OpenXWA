#include "xwa/assets/sprite_resource.h"

#include "aeron/log.h"

#include "xwa/assets/file_io.h"
#include "xwa/util/byte_order.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#else
#include "xwa/frontend/frontend_display.h"
#endif

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPRITE_RESOURCE_LIST_READ_LIMIT 10000
#define SPRITE_RESOURCE_INVALID_GROUP 0xffffu
#define SPRITE_RESOURCE_INDEX_ENTRY_SIZE 8
#define SPRITE_RESOURCE_SPRITE_HEADER_SIZE 18

enum {
	SPRITE_RESOURCE_SPRITE_TYPE_OFFSET = 0,
	SPRITE_RESOURCE_SPRITE_WIDTH_OFFSET = 2,
	SPRITE_RESOURCE_SPRITE_HEIGHT_OFFSET = 4,
	SPRITE_RESOURCE_SPRITE_COLOR_KEY_OFFSET = 6,
	SPRITE_RESOURCE_SPRITE_GROUP_ID_OFFSET = 10,
	SPRITE_RESOURCE_SPRITE_ID_OFFSET = 12,
	SPRITE_RESOURCE_SPRITE_PIXEL_DATA_SIZE_OFFSET = 14,
};

enum {
	SPRITE_RESOURCE_PAYLOAD_COLOR_TABLE_24_OFFSET_OFFSET = 4,
	SPRITE_RESOURCE_PAYLOAD_ROW_DATA_OFFSET_OFFSET = 8,
	SPRITE_RESOURCE_PAYLOAD_PALETTE16_OFFSET_OFFSET = 12,
	SPRITE_RESOURCE_PAYLOAD_ANCHOR_X_OFFSET = 24,
	SPRITE_RESOURCE_PAYLOAD_ANCHOR_Y_OFFSET = 28,
	SPRITE_RESOURCE_PAYLOAD_COLOR_COUNT_OFFSET = 40,
};

typedef struct SpriteResourceFileEntry {
	uint16_t groupId;
	uint16_t spriteCount;
	uint32_t dataBytes;
	uint32_t colorCount;
	uint32_t field0C;
	uint32_t field10;
	uint32_t dataOffset;
} SpriteResourceFileEntry;

#ifndef XWA_MODERN
typedef struct SpriteResourceDatHeader {
	unsigned char unused[8];
	uint16_t format;
} SpriteResourceDatHeader;

typedef struct SpriteResourceDirectoryHeader12 {
	uint16_t entryCount;
	unsigned char unused[10];
} SpriteResourceDirectoryHeader12;

typedef struct SpriteResourceDirectoryHeader24 {
	uint16_t entryCount;
	unsigned char unused[22];
} SpriteResourceDirectoryHeader24;

typedef struct SpriteResourceDirectoryEntry12 {
	uint16_t groupId;
	uint16_t spriteCount;
	uint32_t dataBytes;
	int32_t dataOffset;
} SpriteResourceDirectoryEntry12;

typedef struct SpriteResourceDirectoryEntry24 {
	uint16_t groupId;
	uint16_t spriteCount;
	uint32_t dataBytes;
	uint32_t colorCount;
	uint32_t field0C;
	uint32_t field10;
	int32_t dataOffset;
} SpriteResourceDirectoryEntry24;

typedef struct SpriteResourceSpriteHeader {
	uint16_t type;
	uint16_t width;
	uint16_t height;
	uint16_t colorKey;
	uint16_t field8;
	uint16_t groupId;
	uint16_t spriteId;
	int32_t pixelDataSize;
} SpriteResourceSpriteHeader;
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define XWA_SPRITE_RESOURCE_PACKED_STRUCT
#else
#define XWA_SPRITE_RESOURCE_PACKED_STRUCT __attribute__((packed))
#endif

typedef struct XWA_SPRITE_RESOURCE_PACKED_STRUCT SpriteResourceIndexEntry {
	uint16_t spriteId;
	uint32_t dataOffset;
	uint16_t unused;
} SpriteResourceIndexEntry;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
#undef XWA_SPRITE_RESOURCE_PACKED_STRUCT

// GLOBAL: XWA 0x968BC0
char g_resdataPath[256];
// GLOBAL: XWA 0x968CC0
int g_resourceFileCount = 0;
// GLOBAL: XWA 0x968BB4
int g_unusedFlightSwRotSpriteInitWord = 0;
// GLOBAL: XWA 0x96A140
SpriteCatalogEntry g_spriteCatalog[SPRITE_RESOURCE_MAX_CATALOG];
// GLOBAL: XWA 0x968CE0
SpriteGroup g_spriteGroups[SPRITE_RESOURCE_MAX_GROUPS];

static int g_spriteResourceUsePixelFormat555 = 0;

#ifndef XWA_MODERN
#if defined(_MSC_VER) && _MSC_VER < 1300
typedef int(__stdcall* SpriteResourceGlobalUnlockProc)(unsigned char*);
typedef unsigned char*(__stdcall* SpriteResourceGlobalFreeProc)(unsigned char*);
typedef unsigned char*(__stdcall* SpriteResourceGlobalAllocProc)(unsigned int, uint32_t);
typedef unsigned char*(__stdcall* SpriteResourceGlobalLockProc)(unsigned char*);
typedef unsigned char*(__stdcall* SpriteResourceGlobalReAllocProc)(unsigned char*, uint32_t, unsigned int);
typedef int(__stdcall* SpriteResourceLegacyOpenProc)(const char*, int);
typedef unsigned int(__stdcall* SpriteResourceLegacyReadProc)(int, void*, unsigned int);
typedef long(__stdcall* SpriteResourceLegacyHugeReadProc)(int, void*, long);
typedef long(__stdcall* SpriteResourceLegacySeekProc)(int, long, int);
typedef int(__stdcall* SpriteResourceLegacyCloseProc)(int);
#else
typedef int (*SpriteResourceGlobalUnlockProc)(unsigned char*);
typedef unsigned char* (*SpriteResourceGlobalFreeProc)(unsigned char*);
#endif

// GLOBAL: XWA 0x5A9078
SpriteResourceGlobalUnlockProc GlobalUnlock;
// GLOBAL: XWA 0x5A909C
SpriteResourceGlobalFreeProc GlobalFree;
#if defined(_MSC_VER) && _MSC_VER < 1300
// GLOBAL: XWA 0x5A908C
SpriteResourceGlobalAllocProc GlobalAlloc;
// GLOBAL: XWA 0x5A9088
SpriteResourceGlobalLockProc GlobalLock;
// GLOBAL: XWA 0x5A907C
SpriteResourceGlobalReAllocProc GlobalReAlloc;
// GLOBAL: XWA 0x5A90A0
SpriteResourceLegacyOpenProc lopen;
// GLOBAL: XWA 0x5A90A4
SpriteResourceLegacyReadProc lread;
// GLOBAL: XWA 0x5A9080
SpriteResourceLegacyHugeReadProc hread;
// GLOBAL: XWA 0x5A9084
SpriteResourceLegacySeekProc llseek;
// GLOBAL: XWA 0x5A90A8
SpriteResourceLegacyCloseProc lclose;
#endif
#endif

// FLAGS: /O2 /G6
static const unsigned char* SpriteResource_GetSpriteBytes(const Sprite* sprite) {
	return (const unsigned char*)sprite;
}

static unsigned char* SpriteResource_GetMutableSpriteBytes(Sprite* sprite) { return (unsigned char*)sprite; }

static int SpriteResource_IsPaletteSpriteType(uint16_t type) {
	switch (type) {
		case 7:
		case 9:
		case 11:
		case 13:
		case 15:
		case 17:
		case 19:
		case 21:
		case 23:
			return 1;
		default:
			return 0;
	}
}

static int SpriteResource_DisplayIsPixelFormat555(void) { return g_spriteResourceUsePixelFormat555; }

static void SpriteResource_UppercasePath(char* path) {
	for (; *path != '\0'; ++path) {
		*path = (char)toupper((unsigned char)*path);
	}
}

static XwaFile* SpriteResource_OpenRead(char* path) {
	char uppercasePath[512];
	XwaFile* stream = File_Open(AERON_VFS_ROOT_ASSET, path, "rb");

	if (stream != NULL) {
		return stream;
	}

	snprintf(uppercasePath, sizeof(uppercasePath), "%s", path);
	SpriteResource_UppercasePath(uppercasePath);
	return File_Open(AERON_VFS_ROOT_ASSET, uppercasePath, "rb");
}

static int SpriteResource_ReadExact(XwaFile* stream, void* buffer, size_t size) {
	return File_ReadCount(stream, buffer, size);
}

static void SpriteResource_InitGroups(void) {
	int i;

#ifdef XWA_MODERN
	XwaSnapshot_NoteSpriteGroupsReset();
#endif
	for (i = 0; i < SPRITE_RESOURCE_MAX_GROUPS; ++i) {
		g_spriteGroups[i].groupId = -1;
	}
}

static void SpriteResource_InitCatalog(void) {
	int i;

	for (i = 0; i < SPRITE_RESOURCE_MAX_CATALOG; ++i) {
		g_spriteCatalog[i].groupId = SPRITE_RESOURCE_INVALID_GROUP;
	}
}

static int SpriteResource_ReadListLine(const unsigned char* buffer, int size, int* offset, char* path) {
	int length = 0;

	if (*offset >= size) {
		return 0;
	}

	while (*offset < size && buffer[*offset] != '\n') {
		if (length < 255) {
			path[length++] = (char)buffer[*offset];
		}
		++*offset;
	}

	if (*offset < size && buffer[*offset] == '\n') {
		++*offset;
	}

	while (length > 0 && (path[length - 1] == '\r' || path[length - 1] == '\n')) {
		--length;
	}

	path[length] = '\0';
	return length > 0 || *offset <= size;
}

static int SpriteResource_ReadDatDirectory(XwaFile* stream, SpriteResourceFileEntry* entries, int maxEntries,
										   int* entryCount, int* recordFormat) {
	unsigned char header[10];
	unsigned char record[24];
	int i;
	int recordSize;

	if (!SpriteResource_ReadExact(stream, header, sizeof(header))) {
		return 0;
	}

	*recordFormat = ByteOrder_ReadU16Le(header + 8);
	recordSize = *recordFormat == 0 ? 12 : 24;

	if (!SpriteResource_ReadExact(stream, record, (size_t)recordSize)) {
		return 0;
	}

	*entryCount = ByteOrder_ReadU16Le(record);
	if (*entryCount > maxEntries) {
		return 0;
	}

	for (i = 0; i < *entryCount; ++i) {
		if (!SpriteResource_ReadExact(stream, record, (size_t)recordSize)) {
			return 0;
		}

		entries[i].groupId = ByteOrder_ReadU16Le(record);
		entries[i].spriteCount = ByteOrder_ReadU16Le(record + 2);
		entries[i].dataBytes = ByteOrder_ReadU32Le(record + 4);
		entries[i].colorCount = *recordFormat == 0 ? 0 : ByteOrder_ReadU32Le(record + 8);
		entries[i].field0C = *recordFormat == 0 ? 0 : ByteOrder_ReadU32Le(record + 12);
		entries[i].field10 = *recordFormat == 0 ? 0 : ByteOrder_ReadU32Le(record + 16);
		entries[i].dataOffset = ByteOrder_ReadU32Le(record + (recordSize - 4));
		if (*recordFormat != 0) {
			entries[i].dataBytes += 4 * entries[i].colorCount;
		}
	}

	return 1;
}

static int SpriteResource_FindLoadedGroup(int16_t groupId) {
	int i;

	for (i = 0; i < SPRITE_RESOURCE_MAX_GROUPS; ++i) {
		if (g_spriteGroups[i].groupId == groupId) {
			return i;
		}
	}

	return -1;
}

static int SpriteResource_FindFreeGroup(void) {
	int i;

	for (i = 0; i < SPRITE_RESOURCE_MAX_GROUPS; ++i) {
		if (g_spriteGroups[i].groupId == -1) {
			return i;
		}
	}

	return -1;
}

static unsigned char* SpriteResource_GetIndexEntry(SpriteGroup* group, int index) {
	return group->indexBase + SPRITE_RESOURCE_INDEX_ENTRY_SIZE * index;
}

static uint16_t SpriteResource_GetIndexSpriteId(SpriteGroup* group, int index) {
	return ByteOrder_ReadU16Le(SpriteResource_GetIndexEntry(group, index));
}

static void SpriteResource_SetIndexEntry(SpriteGroup* group, int index, uint16_t spriteId,
										 uint32_t dataOffset) {
	unsigned char* entry = SpriteResource_GetIndexEntry(group, index);

	ByteOrder_WriteU16Le(entry, spriteId);
	ByteOrder_WriteU32Le(entry + 2, dataOffset);
}

static int SpriteResource_FindCatalogStartForFile(int fileIndex) {
	int catalogIndex = 0;
	int currentFile;

	for (currentFile = 0; currentFile < fileIndex; ++currentFile) {
		while (catalogIndex < SPRITE_RESOURCE_MAX_CATALOG &&
			   g_spriteCatalog[catalogIndex].groupId != SPRITE_RESOURCE_INVALID_GROUP) {
			++catalogIndex;
		}

		++catalogIndex;
	}

	return catalogIndex;
}

static int SpriteResource_FileContainsGroup(int catalogIndex, int16_t groupId) {
	while (catalogIndex < SPRITE_RESOURCE_MAX_CATALOG &&
		   g_spriteCatalog[catalogIndex].groupId != SPRITE_RESOURCE_INVALID_GROUP) {
		if (g_spriteCatalog[catalogIndex].groupId == (uint16_t)groupId) {
			return 1;
		}

		++catalogIndex;
	}

	return 0;
}

static int SpriteResource_FindDirectoryEntry(SpriteResourceFileEntry* entries, int count, int16_t groupId) {
	int i;

	for (i = 0; i < count; ++i) {
		if (entries[i].groupId == (uint16_t)groupId) {
			return i;
		}
	}

	return -1;
}

static int SpriteResource_FindInsertIndex(SpriteGroup* group, uint16_t spriteId, int* duplicate) {
	int i;

	*duplicate = 0;
	for (i = 0; i < group->spriteCount; ++i) {
		uint16_t indexedSpriteId = SpriteResource_GetIndexSpriteId(group, i);

		if (spriteId == indexedSpriteId) {
			*duplicate = 1;
			return i;
		}

		if (spriteId < indexedSpriteId) {
			return i;
		}
	}

	return group->spriteCount;
}

static void SpriteResource_InsertIndexEntry(SpriteGroup* group, int insertIndex, uint16_t spriteId,
											uint32_t dataOffset) {
	int i;

	for (i = group->spriteCount; i > insertIndex; --i) {
		memcpy(SpriteResource_GetIndexEntry(group, i), SpriteResource_GetIndexEntry(group, i - 1),
			   SPRITE_RESOURCE_INDEX_ENTRY_SIZE);
	}

	SpriteResource_SetIndexEntry(group, insertIndex, spriteId, dataOffset);
	++group->spriteCount;
}

static uint16_t SpriteResource_ConvertColor555(const unsigned char* rgb) {
	return (uint16_t)(((rgb[0] & 0xf8u) << 7) | ((rgb[1] & 0xf8u) << 2) | (rgb[2] >> 3));
}

static uint16_t SpriteResource_ConvertColor565(const unsigned char* rgb) {
	return (uint16_t)(((rgb[0] & 0xf8u) << 8) | ((rgb[1] & 0xfcu) << 3) | (rgb[2] >> 3));
}

static uint32_t SpriteResource_AppendPalette16(unsigned char* destination, const unsigned char* spriteData) {
	uint32_t colorTableOffset =
		ByteOrder_ReadU32Le(spriteData + SPRITE_RESOURCE_PAYLOAD_COLOR_TABLE_24_OFFSET_OFFSET);
	uint32_t colorCount = ByteOrder_ReadU32Le(spriteData + SPRITE_RESOURCE_PAYLOAD_COLOR_COUNT_OFFSET);
	const unsigned char* rgb = spriteData + colorTableOffset;
	uint32_t i;

	for (i = 0; i < colorCount; ++i) {
		uint16_t color = SpriteResource_DisplayIsPixelFormat555() ? SpriteResource_ConvertColor555(rgb)
																  : SpriteResource_ConvertColor565(rgb);
		ByteOrder_WriteU16Le(destination + 2 * i, color);
		rgb += 3;
	}

	return 2 * colorCount;
}

static void SpriteResource_ResetGroup(SpriteGroup* group) {
	free(group->hGlobal);
	group->groupId = -1;
	group->spriteCount = 0;
	group->indexSize = 0;
	group->dataSize = 0;
	group->hGlobal = NULL;
	group->lockState = 0;
	group->indexBase = NULL;
	group->dataBase = NULL;
}

uint16_t SpriteResource_GetSpriteType(const Sprite* sprite) {
	return sprite == NULL ? 0
						  : ByteOrder_ReadU16Le(SpriteResource_GetSpriteBytes(sprite) +
												SPRITE_RESOURCE_SPRITE_TYPE_OFFSET);
}

uint16_t SpriteResource_GetSpriteWidth(const Sprite* sprite) {
	return sprite == NULL ? 0
						  : ByteOrder_ReadU16Le(SpriteResource_GetSpriteBytes(sprite) +
												SPRITE_RESOURCE_SPRITE_WIDTH_OFFSET);
}

uint16_t SpriteResource_GetSpriteHeight(const Sprite* sprite) {
	return sprite == NULL ? 0
						  : ByteOrder_ReadU16Le(SpriteResource_GetSpriteBytes(sprite) +
												SPRITE_RESOURCE_SPRITE_HEIGHT_OFFSET);
}

uint16_t SpriteResource_GetSpriteColorKey(const Sprite* sprite) {
	return sprite == NULL ? 0
						  : ByteOrder_ReadU16Le(SpriteResource_GetSpriteBytes(sprite) +
												SPRITE_RESOURCE_SPRITE_COLOR_KEY_OFFSET);
}

uint16_t SpriteResource_GetSpriteGroupId(const Sprite* sprite) {
	return sprite == NULL ? 0
						  : ByteOrder_ReadU16Le(SpriteResource_GetSpriteBytes(sprite) +
												SPRITE_RESOURCE_SPRITE_GROUP_ID_OFFSET);
}

uint16_t SpriteResource_GetSpriteId(const Sprite* sprite) {
	return sprite == NULL ? 0
						  : ByteOrder_ReadU16Le(SpriteResource_GetSpriteBytes(sprite) +
												SPRITE_RESOURCE_SPRITE_ID_OFFSET);
}

uint32_t SpriteResource_GetSpritePixelDataSize(const Sprite* sprite) {
	return sprite == NULL ? 0
						  : ByteOrder_ReadU32Le(SpriteResource_GetSpriteBytes(sprite) +
												SPRITE_RESOURCE_SPRITE_PIXEL_DATA_SIZE_OFFSET);
}

const unsigned char* SpriteResource_GetSpritePayload(const Sprite* sprite) {
	return sprite == NULL ? NULL : SpriteResource_GetSpriteBytes(sprite) + SPRITE_RESOURCE_SPRITE_HEADER_SIZE;
}

unsigned char* SpriteResource_GetMutableSpritePayload(Sprite* sprite) {
	return sprite == NULL ? NULL
						  : SpriteResource_GetMutableSpriteBytes(sprite) + SPRITE_RESOURCE_SPRITE_HEADER_SIZE;
}

const unsigned char* SpriteResource_GetSpritePalette16(const Sprite* sprite) {
	const unsigned char* payload;
	uint32_t paletteOffset;

	if (sprite == NULL) {
		return NULL;
	}

	payload = SpriteResource_GetSpritePayload(sprite);
	paletteOffset = ByteOrder_ReadU32Le(payload + SPRITE_RESOURCE_PAYLOAD_PALETTE16_OFFSET_OFFSET);
	return payload + paletteOffset;
}

int16_t SpriteResource_GetSpriteAnchorX(const Sprite* sprite) {
	const unsigned char* payload;

	if (sprite == NULL) {
		return 0;
	}

	payload = SpriteResource_GetSpritePayload(sprite);
	return (int16_t)ByteOrder_ReadU16Le(payload + SPRITE_RESOURCE_PAYLOAD_ANCHOR_X_OFFSET);
}

int16_t SpriteResource_GetSpriteAnchorY(const Sprite* sprite) {
	const unsigned char* payload;

	if (sprite == NULL) {
		return 0;
	}

	payload = SpriteResource_GetSpritePayload(sprite);
	return (int16_t)ByteOrder_ReadU16Le(payload + SPRITE_RESOURCE_PAYLOAD_ANCHOR_Y_OFFSET);
}

void SpriteResource_SetPixelFormat555(int enabled) { g_spriteResourceUsePixelFormat555 = enabled != 0; }

// FUNCTION: XWA 0x4CD390
int SpriteResource_LoadCatalog(char* listFile) {
	XwaFile* listStream;
	unsigned char listBuffer[SPRITE_RESOURCE_LIST_READ_LIMIT];
	int listSize;
	int offset = 0;
	int catalogIndex = 0;
	char path[256];

	g_unusedFlightSwRotSpriteInitWord = 0;
	SpriteResource_InitGroups();
	SpriteResource_InitCatalog();
	strncpy(g_resdataPath, listFile, sizeof(g_resdataPath));

	listStream = SpriteResource_OpenRead(g_resdataPath);
	if (listStream == NULL) {
		Aeron_Log("xwa.assets", "Failed to open sprite resource catalog '%s'", g_resdataPath);
		return -1;
	}

	listSize = File_GetSize(listStream);
	if (listSize > (int)sizeof(listBuffer)) {
		listSize = (int)sizeof(listBuffer);
	}
	if (listSize > 0 && !File_ReadCount(listStream, listBuffer, (size_t)listSize)) {
		File_Close(listStream);
		Aeron_Log("xwa.assets", "Failed to read sprite resource catalog '%s'", g_resdataPath);
		return -1;
	}
	File_Close(listStream);
	g_resourceFileCount = 0;
	if (listSize <= 0) {
		return 0;
	}

	while (offset < listSize) {
		XwaFile* datStream;
		SpriteResourceFileEntry entries[512];
		int entryCount;
		int recordFormat;
		int i;

		if (!SpriteResource_ReadListLine(listBuffer, listSize, &offset, path) || path[0] == '\0') {
			break;
		}

		datStream = SpriteResource_OpenRead(path);
		if (datStream != NULL) {
			if (!SpriteResource_ReadDatDirectory(datStream, entries, 512, &entryCount, &recordFormat)) {
				File_Close(datStream);
				Aeron_Log("xwa.assets", "Failed to read sprite DAT directory '%s'", path);
				return -1;
			}

			for (i = 0; i < entryCount; ++i) {
				(void)recordFormat;
				g_spriteCatalog[catalogIndex].groupId = entries[i].groupId;
				g_spriteCatalog[catalogIndex].spriteCount = entries[i].spriteCount;
				g_spriteCatalog[catalogIndex].dataBytes = entries[i].dataBytes;
				++catalogIndex;
			}

			File_Close(datStream);
		}

		g_spriteCatalog[catalogIndex].groupId = SPRITE_RESOURCE_INVALID_GROUP;
		g_spriteCatalog[catalogIndex].spriteCount = 0;
		g_spriteCatalog[catalogIndex].dataBytes = 0;
		++catalogIndex;
		++g_resourceFileCount;
	}

	return 0;
}

// FUNCTION: XWA 0x4CD680
void SpriteResource_FreeGroups(void) {
	int i;
	SpriteGroup* group;

#if defined(_MSC_VER) && _MSC_VER < 1300
	SpriteResourceGlobalUnlockProc unlockMemory = GlobalUnlock;
	SpriteResourceGlobalFreeProc freeMemory = GlobalFree;
#endif

	group = g_spriteGroups;
	for (i = 0; i < SPRITE_RESOURCE_MAX_GROUPS; ++i, ++group) {
		if ((uint16_t)group->groupId != SPRITE_RESOURCE_INVALID_GROUP) {
#if defined(_MSC_VER) && _MSC_VER < 1300
			if (group->lockState == 1) {
				unlockMemory(group->hGlobal);
			}
			freeMemory(group->hGlobal);
#else
			free(group->hGlobal);
#endif
		}
	}
#ifdef XWA_MODERN
	XwaSnapshot_NoteSpriteGroupsReset();
#endif
}

// FUNCTION: XWA 0x4CD6C0
int16_t SpriteResource_LoadGroup(int16_t groupId) {
#ifndef XWA_MODERN
	unsigned char listBuffer[SPRITE_RESOURCE_LIST_READ_LIMIT];
	SpriteResourceDirectoryEntry12 entries12[64];
	SpriteResourceDirectoryEntry24 entries24[64];
	char path[256];
	char cdPath[259];
	SpriteResourceDatHeader datHeader;
	SpriteResourceDirectoryHeader12 directoryHeader12;
	SpriteResourceDirectoryHeader24 directoryHeader24;
	SpriteResourceSpriteHeader spriteHeader;
	unsigned char* dataCursor;
	int discardedBytes;
	uint16_t groupIndex;
	int indexSize;
	int dataSize;
	int allocationClass;
	int listFile;
	int listOffset;
	int catalogOffset;
	int fileIndex;
	int dataUsed;
	int paletteBytes;
	SpriteResourceLegacyReadProc readFile;

	cdPath[0] = File_GetCdDriveLetter();
	cdPath[1] = ':';
	cdPath[2] = '\\';
	indexSize = 0;
	for (groupIndex = 0; groupIndex < SPRITE_RESOURCE_MAX_GROUPS; ++groupIndex) {
		if (g_spriteGroups[groupIndex].groupId == groupId) {
			return (int16_t)groupIndex;
		}
	}

	for (groupIndex = 0; groupIndex < SPRITE_RESOURCE_MAX_GROUPS; ++groupIndex) {
		if (g_spriteGroups[groupIndex].groupId == -1) {
			break;
		}
	}
	if (groupIndex < SPRITE_RESOURCE_MAX_GROUPS) {

		g_spriteGroups[groupIndex].groupId = groupId;
		g_spriteGroups[groupIndex].spriteCount = 0;
		g_spriteGroups[groupIndex].lockState = 3;
		dataSize = 0;
		{
			SpriteCatalogEntry* catalogEntry = g_spriteCatalog;
			int entriesRemaining = SPRITE_RESOURCE_MAX_CATALOG;

			do {
				if (catalogEntry->groupId == (uint16_t)groupId) {
					dataSize += catalogEntry->dataBytes;
					indexSize += SPRITE_RESOURCE_INDEX_ENTRY_SIZE * catalogEntry->spriteCount;
				}
				++catalogEntry;
			} while (--entriesRemaining != 0);
		}

		g_spriteGroups[groupIndex].indexSize = indexSize;
		g_spriteGroups[groupIndex].dataSize = indexSize + dataSize;
		if (g_spriteGroups[groupIndex].dataSize == 0) {
			g_spriteGroups[groupIndex].groupId = -1;
			return SPRITE_RESOURCE_GROUP_NOT_FOUND;
		}

		allocationClass = groupId & 0x8000;
		g_spriteGroups[groupIndex].hGlobal =
			GlobalAlloc(allocationClass != 0 ? 0x102 : 2, g_spriteGroups[groupIndex].dataSize);
		g_spriteGroups[groupIndex].indexBase = GlobalLock(g_spriteGroups[groupIndex].hGlobal);
		g_spriteGroups[groupIndex].dataBase =
			g_spriteGroups[groupIndex].indexBase + g_spriteGroups[groupIndex].indexSize;
		dataCursor = g_spriteGroups[groupIndex].dataBase;
		g_spriteGroups[groupIndex].lockState = 1;
		discardedBytes = 0;

		listFile = lopen(g_resdataPath, 0);
		if (listFile == -1) {
			return -1;
		}
		readFile = lread;
		readFile(listFile, listBuffer, sizeof(listBuffer));
		lclose(listFile);

		listOffset = 0;
		catalogOffset = 0;
		dataUsed = 0;
		for (fileIndex = 0; (uint16_t)fileIndex < (uint16_t)g_resourceFileCount; ++fileIndex) {
			uint16_t pathLength = 0;
			int containsGroup = 0;
			int nextListOffset;
			int nextCatalogOffset;

			while (listBuffer[(uint16_t)listOffset] != '\n') {
				path[pathLength++] = listBuffer[(uint16_t)listOffset++];
			}
			++listOffset;
			nextListOffset = listOffset;
			path[pathLength] = '\0';

			while (g_spriteCatalog[(uint16_t)catalogOffset].groupId != SPRITE_RESOURCE_INVALID_GROUP) {
				if (g_spriteCatalog[(uint16_t)catalogOffset].groupId == (uint16_t)groupId) {
					containsGroup = 1;
				}
				++catalogOffset;
			}
			nextCatalogOffset = ++catalogOffset;

			if (containsGroup == 1) {
				int datFile = lopen(path, 0);
				uint16_t entryCount;
				uint16_t entryIndex;
				uint16_t spriteCount;

				if (datFile == -1) {
					strcpy(cdPath + 3, path);
					datFile = lopen(cdPath, 0);
					if (datFile == -1) {
						return -1;
					}
				}

				if (readFile(datFile, &datHeader, sizeof(datHeader)) == (unsigned int)-1) {
					return -1;
				}
				if (datHeader.format == 0) {
					if (readFile(datFile, &directoryHeader12, sizeof(directoryHeader12)) ==
						(unsigned int)-1) {
						return -1;
					}
					entryCount = directoryHeader12.entryCount;
					if (readFile(datFile, entries12, sizeof(entries12[0]) * entryCount) == (unsigned int)-1) {
						return -1;
					}
					for (entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
						if (entries12[entryIndex].groupId == (uint16_t)groupId) {
							break;
						}
					}
				} else {
					if (readFile(datFile, &directoryHeader24, sizeof(directoryHeader24)) ==
						(unsigned int)-1) {
						return -1;
					}
					entryCount = directoryHeader24.entryCount;
					if (readFile(datFile, entries24, sizeof(entries24[0]) * entryCount) == (unsigned int)-1) {
						return -1;
					}
					for (entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
						if (entries24[entryIndex].groupId == (uint16_t)groupId) {
							break;
						}
					}
				}

				if (entryIndex >= entryCount) {
					lclose(datFile);
					return SPRITE_RESOURCE_GROUP_NOT_FOUND;
				}

				if (datHeader.format == 0) {
					if (entries12[entryIndex].dataOffset != 0) {
						llseek(datFile, entries12[entryIndex].dataOffset, SEEK_CUR);
					}
					spriteCount = entries12[entryIndex].spriteCount;
				} else {
					if (entries24[entryIndex].dataOffset != 0) {
						llseek(datFile, entries24[entryIndex].dataOffset, SEEK_CUR);
					}
					spriteCount = entries24[entryIndex].spriteCount;
				}

				while (spriteCount != 0) {
					unsigned char* spriteStart;
					unsigned char* payloadStart;
					unsigned char* headerByte;
					uint16_t insertIndex;
					uint16_t currentCount;
					int byteIndex;

					readFile(datFile, &spriteHeader, sizeof(spriteHeader));
					spriteStart = dataCursor;
					if (spriteHeader.groupId == (uint16_t)groupId) {

						headerByte = (unsigned char*)&spriteHeader;
						for (byteIndex = sizeof(spriteHeader); byteIndex != 0; --byteIndex) {
							*dataCursor++ = *headerByte++;
						}
						payloadStart = dataCursor;
						hread(datFile, payloadStart, spriteHeader.pixelDataSize);
						dataCursor = payloadStart + spriteHeader.pixelDataSize;
						switch (spriteHeader.type) {
							case 7:
							case 9:
							case 11:
							case 13:
							case 15:
							case 17:
							case 19:
							case 21:
							case 23: {
								SpritePayload* payload = (SpritePayload*)payloadStart;
								unsigned char* rgb = payloadStart + payload->colorTable24Offset;
								uint16_t* palette = (uint16_t*)dataCursor;
								int colorCount = payload->colorCount;
								int colorIndex;

								if (Display_IsPixelFormat555()) {
									for (colorIndex = 0; colorIndex < colorCount; ++colorIndex) {
										*palette++ = (uint16_t)(((rgb[0] & 0xf8) << 7) |
																((rgb[1] & 0xf8) << 2) | (rgb[2] >> 3));
										rgb += 3;
									}
								} else {
									for (colorIndex = 0; colorIndex < colorCount; ++colorIndex) {
										*palette++ = (uint16_t)(((rgb[0] & 0xf8) << 8) |
																((rgb[1] & 0xfc) << 3) | (rgb[2] >> 3));
										rgb += 3;
									}
								}
								paletteBytes = 2 * colorCount;
								dataCursor += paletteBytes;
								break;
							}
							case 24:
							case 25:
							default:
								paletteBytes = 0;
								break;
						}

						currentCount = (uint16_t)g_spriteGroups[groupIndex].spriteCount;
						for (insertIndex = 0; insertIndex < currentCount; ++insertIndex) {
							SpriteResourceIndexEntry* indexEntries =
								(SpriteResourceIndexEntry*)g_spriteGroups[groupIndex].indexBase;
							if (spriteHeader.spriteId < indexEntries[insertIndex].spriteId) {
								break;
							}
						}

						if (insertIndex != currentCount) {
							SpriteResourceIndexEntry* indexEntries =
								(SpriteResourceIndexEntry*)g_spriteGroups[groupIndex].indexBase;
							if (spriteHeader.spriteId != indexEntries[insertIndex - 1].spriteId &&
								(uint16_t)(currentCount + 1) > insertIndex) {
								int moveIndex;

								for (moveIndex = currentCount; moveIndex >= insertIndex; --moveIndex) {
									indexEntries[moveIndex] = indexEntries[moveIndex - 1];
								}
							}
						}

						if (currentCount != 0 &&
							((SpriteResourceIndexEntry*)g_spriteGroups[groupIndex].indexBase)[insertIndex - 1]
									.spriteId == spriteHeader.spriteId) {
							discardedBytes += dataCursor - spriteStart;
							dataCursor = spriteStart;
						} else {
							SpriteResourceIndexEntry* indexEntries =
								(SpriteResourceIndexEntry*)g_spriteGroups[groupIndex].indexBase;

							indexEntries[insertIndex].spriteId = spriteHeader.spriteId;
							indexEntries[insertIndex].dataOffset = dataUsed;
							++g_spriteGroups[groupIndex].spriteCount;
							dataUsed += spriteHeader.pixelDataSize + paletteBytes + sizeof(spriteHeader);
						}
					} else {
						llseek(datFile, spriteHeader.pixelDataSize, SEEK_CUR);
					}
					--spriteCount;
				}

				lclose(datFile);
				catalogOffset = nextCatalogOffset;
				listOffset = nextListOffset;
				allocationClass = groupId & 0x8000;
			}
		}

		GlobalUnlock(g_spriteGroups[groupIndex].hGlobal);
		g_spriteGroups[groupIndex].lockState = 2;
		if (discardedBytes != 0) {
			unsigned char* resized;

			g_spriteGroups[groupIndex].dataSize -= discardedBytes;
			resized = GlobalReAlloc(g_spriteGroups[groupIndex].hGlobal, g_spriteGroups[groupIndex].dataSize,
									allocationClass != 0 ? 0x102 : 2);
			if (resized == NULL) {
				return SPRITE_RESOURCE_REALLOC_FAILED;
			}
			g_spriteGroups[groupIndex].hGlobal = resized;
		}

		return (int16_t)groupIndex;
	}
	return SPRITE_RESOURCE_NO_FREE_GROUP;
#else
	int loadedIndex = SpriteResource_FindLoadedGroup(groupId);
	int freeIndex;
	SpriteGroup* group;
	int catalogIndex;
	int i;
	int32_t indexSize = 0;
	int32_t dataSize = 0;
	uint32_t dataUsed = 0;
	unsigned char listBuffer[SPRITE_RESOURCE_LIST_READ_LIMIT];
	int listSize;
	int listOffset = 0;
	XwaFile* listStream;
	size_t allocatedSize;

	if (loadedIndex >= 0) {
		return loadedIndex;
	}

	freeIndex = SpriteResource_FindFreeGroup();
	if (freeIndex < 0) {
		return SPRITE_RESOURCE_NO_FREE_GROUP;
	}

	for (i = 0; i < SPRITE_RESOURCE_MAX_CATALOG; ++i) {
		if (g_spriteCatalog[i].groupId == (uint16_t)groupId) {
			indexSize += 8 * g_spriteCatalog[i].spriteCount;
			dataSize += (int32_t)g_spriteCatalog[i].dataBytes;
		}
	}

	if (indexSize + dataSize == 0) {
		Aeron_Log("xwa.assets", "Sprite group %d not found in catalog '%s'", groupId, g_resdataPath);
		return SPRITE_RESOURCE_GROUP_NOT_FOUND;
	}

	group = &g_spriteGroups[freeIndex];
	group->groupId = groupId;
	group->spriteCount = 0;
	group->indexSize = indexSize;
	group->dataSize = indexSize + dataSize;
	group->lockState = 3;
	group->hGlobal = (unsigned char*)malloc((size_t)indexSize + (size_t)dataSize);
	if (group->hGlobal == NULL) {
		SpriteResource_ResetGroup(group);
		Aeron_Log("xwa.assets", "Failed to allocate sprite group %d (%d bytes)", groupId,
				  indexSize + dataSize);
		return -1;
	}

	group->indexBase = group->hGlobal;
	group->dataBase = group->hGlobal + indexSize;
	group->lockState = 1;

	listStream = SpriteResource_OpenRead(g_resdataPath);
	if (listStream == NULL) {
		SpriteResource_ResetGroup(group);
		Aeron_Log("xwa.assets", "Failed to reopen sprite resource catalog '%s'", g_resdataPath);
		return -1;
	}

	listSize = File_GetSize(listStream);
	if (listSize > (int)sizeof(listBuffer)) {
		listSize = (int)sizeof(listBuffer);
	}
	if (listSize > 0 && !File_ReadCount(listStream, listBuffer, (size_t)listSize)) {
		File_Close(listStream);
		SpriteResource_ResetGroup(group);
		Aeron_Log("xwa.assets", "Failed to read sprite resource catalog '%s'", g_resdataPath);
		return -1;
	}
	File_Close(listStream);

	for (i = 0; i < g_resourceFileCount && listOffset < listSize; ++i) {
		char path[256];
		int fileCatalogIndex = SpriteResource_FindCatalogStartForFile(i);

		if (!SpriteResource_ReadListLine(listBuffer, listSize, &listOffset, path)) {
			break;
		}

		if (SpriteResource_FileContainsGroup(fileCatalogIndex, groupId)) {
			XwaFile* datStream;
			SpriteResourceFileEntry entries[512];
			int entryCount;
			int recordFormat;
			int entryIndex;

			datStream = SpriteResource_OpenRead(path);
			if (datStream == NULL) {
				SpriteResource_ResetGroup(group);
				Aeron_Log("xwa.assets", "Failed to open sprite DAT '%s' for group %d", path, groupId);
				return -1;
			}

			if (!SpriteResource_ReadDatDirectory(datStream, entries, 512, &entryCount, &recordFormat)) {
				File_Close(datStream);
				SpriteResource_ResetGroup(group);
				Aeron_Log("xwa.assets", "Failed to read sprite DAT directory '%s' for group %d", path,
						  groupId);
				return -1;
			}

			entryIndex = SpriteResource_FindDirectoryEntry(entries, entryCount, groupId);
			if (entryIndex < 0) {
				File_Close(datStream);
				SpriteResource_ResetGroup(group);
				Aeron_Log("xwa.assets", "Sprite group %d missing from DAT '%s'", groupId, path);
				return SPRITE_RESOURCE_GROUP_NOT_FOUND;
			}

			if (entries[entryIndex].dataOffset != 0) {
				File_Seek(datStream, (int)entries[entryIndex].dataOffset, SEEK_CUR);
			}

			for (catalogIndex = 0; catalogIndex < entries[entryIndex].spriteCount; ++catalogIndex) {
				unsigned char header[18];
				uint16_t type;
				uint16_t spriteGroupId;
				uint16_t spriteId;
				uint32_t pixelDataSize;
				uint32_t spriteOffset;
				uint32_t memorySize;
				uint32_t paletteBytes = 0;
				int duplicate;
				int insertIndex;

				if (!SpriteResource_ReadExact(datStream, header, sizeof(header))) {
					File_Close(datStream);
					SpriteResource_ResetGroup(group);
					return -1;
				}

				type = ByteOrder_ReadU16Le(header);
				spriteGroupId = ByteOrder_ReadU16Le(header + 10);
				spriteId = ByteOrder_ReadU16Le(header + 12);
				pixelDataSize = ByteOrder_ReadU32Le(header + 14);

				if (spriteGroupId != (uint16_t)groupId) {
					File_Seek(datStream, (int)pixelDataSize, SEEK_CUR);
					continue;
				}

				spriteOffset = dataUsed;
				memorySize = 18 + pixelDataSize;

				memcpy(group->dataBase + dataUsed, header, sizeof(header));
				if (!SpriteResource_ReadExact(datStream, group->dataBase + dataUsed + 18, pixelDataSize)) {
					File_Close(datStream);
					SpriteResource_ResetGroup(group);
					return -1;
				}

				if (SpriteResource_IsPaletteSpriteType(type)) {
					paletteBytes = SpriteResource_AppendPalette16(group->dataBase + dataUsed + memorySize,
																  group->dataBase + dataUsed + 18);
					memorySize += paletteBytes;
				}

				insertIndex = SpriteResource_FindInsertIndex(group, spriteId, &duplicate);
				if (!duplicate) {
					SpriteResource_InsertIndexEntry(group, insertIndex, spriteId, spriteOffset);
					dataUsed += memorySize;
				}
			}

			(void)recordFormat;
			File_Close(datStream);
		}
	}

	group->lockState = 2;
	allocatedSize = (size_t)group->indexSize + (size_t)dataUsed;
	if (allocatedSize < (size_t)group->dataSize) {
		unsigned char* resized = (unsigned char*)realloc(group->hGlobal, allocatedSize);

		if (resized == NULL && allocatedSize != 0) {
			SpriteResource_ResetGroup(group);
			return SPRITE_RESOURCE_REALLOC_FAILED;
		}

		group->hGlobal = resized;
		group->dataSize = (int32_t)allocatedSize;
	}
	if (group->hGlobal == NULL && allocatedSize != 0) {
		SpriteResource_ResetGroup(group);
		return SPRITE_RESOURCE_REALLOC_FAILED;
	}

	group->indexBase = group->hGlobal;
	group->dataBase = group->hGlobal + group->indexSize;
#ifdef XWA_MODERN
	XwaSnapshot_NoteSpriteGroupLoad(groupId);
#endif
	return freeIndex;
#endif
}

// FUNCTION: XWA 0x4CDE40
int16_t SpriteResource_UnloadGroup(int16_t groupId) {
#ifndef XWA_MODERN
	uint16_t index;

	for (index = 0; index < SPRITE_RESOURCE_MAX_GROUPS; ++index) {
		if (g_spriteGroups[index].groupId == groupId) {
			break;
		}
	}

	if (index < SPRITE_RESOURCE_MAX_GROUPS) {
		if (g_spriteGroups[index].lockState == 1) {
			if (!GlobalUnlock(g_spriteGroups[index].hGlobal)) {
				g_spriteGroups[index].lockState = 2;
			} else {
				return SPRITE_RESOURCE_GROUP_STILL_LOCKED;
			}
		}

		GlobalFree(g_spriteGroups[index].hGlobal);
		g_spriteGroups[index].groupId = -1;
		return 0;
	}

	return SPRITE_RESOURCE_GROUP_NOT_FOUND;
#else
	int index = SpriteResource_FindLoadedGroup(groupId);

	if (index < 0) {
		return SPRITE_RESOURCE_GROUP_NOT_FOUND;
	}

	g_spriteGroups[index].lockState = 2;
	SpriteResource_ResetGroup(&g_spriteGroups[index]);
#ifdef XWA_MODERN
	XwaSnapshot_NoteSpriteGroupFree(groupId);
#endif
	return 0;
#endif
}

// FUNCTION: XWA 0x4CDED0
Sprite* SpriteResource_ResolveSprite(int16_t groupId, uint16_t spriteId) {
	uint16_t i;
	uint16_t groupIndex;
	uint16_t targetSpriteId;
	SpriteResourceIndexEntry* indexBase;
	uint16_t spriteCount;
	uint16_t index;
	int attempts;
	int found;

	found = 0;

	for (i = 0; i < SPRITE_RESOURCE_MAX_GROUPS; ++i) {
		if (g_spriteGroups[i].groupId == groupId) {
			break;
		}
	}

	if (i < SPRITE_RESOURCE_MAX_GROUPS) {
		groupIndex = i;
	} else {
		groupIndex = (uint16_t)SpriteResource_LoadGroup(groupId);
	}

	if (groupIndex >= SPRITE_RESOURCE_MAX_GROUPS) {
		return NULL;
	}

	if (g_spriteGroups[groupIndex].lockState - 1 != 0) {
		unsigned char* lockedMemory;

#if defined(_MSC_VER) && _MSC_VER < 1300
		extern unsigned char*(__stdcall * GlobalLock)(unsigned char*);

		if (g_spriteGroups[groupIndex].hGlobal != NULL) {
			lockedMemory = GlobalLock(g_spriteGroups[groupIndex].hGlobal);
		} else {
			lockedMemory = NULL;
		}
#else
		lockedMemory = g_spriteGroups[groupIndex].hGlobal;
#endif
		if (lockedMemory == NULL) {
			g_spriteGroups[groupIndex].groupId = -1;
			groupIndex = (uint16_t)SpriteResource_LoadGroup(groupId);
			if (groupIndex >= 0xfff0u) {
				return NULL;
			}

#if defined(_MSC_VER) && _MSC_VER < 1300
			lockedMemory = GlobalLock(g_spriteGroups[groupIndex].hGlobal);
#else
			lockedMemory = g_spriteGroups[groupIndex].hGlobal;
#endif
			if (lockedMemory == NULL) {
				return NULL;
			}
		}

		g_spriteGroups[groupIndex].indexBase = lockedMemory;
		g_spriteGroups[groupIndex].dataBase = lockedMemory + g_spriteGroups[groupIndex].indexSize;
		g_spriteGroups[groupIndex].lockState = 1;
	}

	spriteCount = g_spriteGroups[groupIndex].spriteCount;
	targetSpriteId = spriteId;
	indexBase = (SpriteResourceIndexEntry*)g_spriteGroups[groupIndex].indexBase;
	index = spriteCount >> 1;
	groupId = index;
	attempts = 16;

	for (;;) {
		uint16_t indexedSpriteId = indexBase[index].spriteId;

		if (indexedSpriteId == targetSpriteId) {
			found = 1;
			break;
		}

		if (indexedSpriteId > targetSpriteId) {
			groupId = (uint16_t)groupId >> 1;
			if ((uint16_t)groupId == 0) {
				groupId = 1;
			}
			index -= (uint16_t)groupId;
			if (index >= 0x8000u) {
				index = 0;
			}
		} else {
			groupId = (uint16_t)groupId >> 1;
			if ((uint16_t)groupId == 0) {
				groupId = 1;
			}
			index += (uint16_t)groupId;
			if (index >= spriteCount) {
				index = spriteCount - 1;
			}
		}

		if (attempts-- == 0) {
			break;
		}
	}

	if (found == 1) {
		return (Sprite*)(g_spriteGroups[groupIndex].dataBase + indexBase[index].dataOffset);
	}

	for (index = 0; index < spriteCount; ++index) {
		if (indexBase[index].spriteId == targetSpriteId) {
			break;
		}
	}

	if (index < spriteCount) {
		return (Sprite*)(g_spriteGroups[groupIndex].dataBase + indexBase[index].dataOffset);
	}

	return NULL;
}

// FUNCTION: XWA 0x4CE080
void* SpriteResource_GetRowData(Sprite* sprite) {
	void* rowData = NULL;

	if (sprite != NULL) {
		switch (sprite->type) {
			case 7:
			case 9:
			case 11:
			case 13:
			case 15:
			case 17:
			case 19:
			case 21:
			case 23:
			case 24:
			case 25: {
#ifdef XWA_MODERN
				SpritePayload* payload = (SpritePayload*)SpriteResource_GetMutableSpritePayload(sprite);
#else
				SpritePayload* payload = (SpritePayload*)sprite->pixels;
#endif

				return (unsigned char*)payload + payload->rowDataOffset;
			}
			default:
#ifdef XWA_MODERN
				rowData = SpriteResource_GetMutableSpritePayload(sprite);
#else
				rowData = sprite->pixels;
#endif
				break;
		}
	}

	return rowData;
}

// FUNCTION: XWA 0x4CF920
uint16_t SpriteResource_GetGroupSpriteCount(int16_t groupId) {
	uint16_t i;
	uint16_t groupIndex;
	uint16_t spriteCount;

	for (i = 0; i < SPRITE_RESOURCE_MAX_GROUPS; ++i) {
		if (g_spriteGroups[i].groupId == groupId) {
			break;
		}
	}

	if (i < SPRITE_RESOURCE_MAX_GROUPS) {
		groupIndex = i;
	} else {
		groupIndex = SpriteResource_LoadGroup(groupId);
	}

	if (groupIndex < SPRITE_RESOURCE_MAX_GROUPS) {
		spriteCount = (uint16_t)g_spriteGroups[groupIndex].spriteCount;
	} else {
		spriteCount = 0xffff;
	}

	return spriteCount;
}

/* Port-side helper (not in the original binary): enumerate the actual
 * sprite ids of a group's index. Ids are NOT dense — frontend atlas
 * resources use structured base offsets (4002, 5002, ...) — so tools
 * must walk the index rather than assume 0..count-1. Loads the group
 * when needed; returns the id count written (capped at max_ids). */
int SpriteResource_GetGroupSpriteIds(int16_t groupId, uint16_t* out_ids, int max_ids) {
	int i;
	SpriteGroup* group = NULL;

	for (i = 0; i < SPRITE_RESOURCE_MAX_GROUPS; ++i) {
		if (g_spriteGroups[i].groupId == groupId) {
			group = &g_spriteGroups[i];
			break;
		}
	}
	if (group == NULL) {
		int16_t loaded = SpriteResource_LoadGroup(groupId);
		if (loaded < 0 || loaded >= SPRITE_RESOURCE_MAX_GROUPS) {
			return 0;
		}
		group = &g_spriteGroups[loaded];
	}

	if (out_ids == NULL || max_ids <= 0) {
		return 0;
	}
	{
		int count = group->spriteCount;
		if (count > max_ids) {
			count = max_ids;
		}
		for (i = 0; i < count; ++i) {
			out_ids[i] = SpriteResource_GetIndexSpriteId(group, i);
		}
		return count;
	}
}
