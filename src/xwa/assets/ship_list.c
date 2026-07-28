#include "xwa/assets/ship_list.h"

#include "aeron/aeron.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/linez.h"
#include "xwa/assets/string_table.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/util/memory.h"
#include "xwa/util/string.h"

#include <stdlib.h>
#include <string.h>
#ifndef XWA_MODERN
#include <stdio.h>
#endif

// GLOBAL: XWA 0xABD22C
ShipListEntry* g_shipList;
// GLOBAL: XWA 0xABD280
int g_shipTypeToShipListIndex[SHIP_TYPE_TO_SHIP_LIST_CAPACITY];
// GLOBAL: XWA 0xABD7DC
int g_shipCount;

// FUNCTION: XWA 0x529950
int ShipList_Load(void) {
	XwaFile* stream;
	ShipListEntry* list;
	unsigned int i;
	char lineBuffer[256];
	char* token;

	if (g_shipList != NULL) {
		return 0;
	}

#ifdef XWA_MODERN
	stream = File_Open(AERON_VFS_ROOT_ASSET, "ALLIANCE/SHIPLIST.TXT", "r");
#else
	stream = File_Open(AERON_VFS_ROOT_ASSET, "shiplist.txt", "r");
#endif
	if (stream == NULL) {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.assets", "Failed to open ship list 'ALLIANCE/SHIPLIST.TXT'");
#endif
		return 0;
	}

	list = (ShipListEntry*)Mem_Alloc(sizeof(ShipListEntry) * SHIP_LIST_MAX_ENTRIES);
	g_shipList = list;
	if (list == NULL) {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.assets", "Failed to allocate ship list");
		File_Close(stream);
#endif
		return 0;
	}

	memset(list, 0, sizeof(ShipListEntry) * SHIP_LIST_MAX_ENTRIES);
	strcpy(g_shipList->name, FrontendString_Get(STR_SKIRMISH_NOT_USED));
	g_shipList->typeId = 0;
	g_shipList->category = 0;
	g_shipList->field264 = 0;
	g_shipList->flyable = 0;
	g_shipList->skirmish = 1;
	g_shipCount = 1;

	for (i = 1; i < 0xffu; ++i) {
#ifdef XWA_MODERN
		if (!File_ReadLine(stream, g_frontendScratchBuffer, sizeof(g_frontendScratchBuffer))) {
			break;
		}
		token = g_frontendScratchBuffer;
#else
		token = fgets(g_frontendScratchBuffer, sizeof(g_frontendScratchBuffer), stream);
		if (token == NULL) {
			break;
		}
#endif

		strcpy(lineBuffer, Linez_ResolveString(token));
		strcpy(g_frontendScratchBuffer, lineBuffer);
		lineBuffer[0] = '\0';

		token = strtok(g_frontendScratchBuffer, ",\n");
		if (token == NULL) {
			break;
		}

		if (*token != '*') {
			strcpy(g_shipList[g_shipCount].name, token);
			g_shipList[g_shipCount].typeId = (int)i;
			g_shipList[g_shipCount].category = 255;

			strcpy(lineBuffer, strtok(NULL, ",\n"));
			if (!strcmp(lineBuffer, "Fighter")) {
				g_shipList[g_shipCount].category = 1;
			} else if (!strcmp(lineBuffer, "Shuttle/Light Transport")) {
				g_shipList[g_shipCount].category = 2;
			} else if (!strcmp(lineBuffer, "Utility Craft")) {
				g_shipList[g_shipCount].category = 3;
			} else if (!strcmp(lineBuffer, "Container")) {
				g_shipList[g_shipCount].category = 4;
			} else if (!strcmp(lineBuffer, "Freighter/Heavy Transport")) {
				g_shipList[g_shipCount].category = 5;
			} else if (!strcmp(lineBuffer, "Starship")) {
				g_shipList[g_shipCount].category = 6;
			} else if (!strcmp(lineBuffer, "Station")) {
				g_shipList[g_shipCount].category = 7;
			} else if (!strcmp(lineBuffer, "Weapon emplacement")) {
				g_shipList[g_shipCount].category = 8;
			} else if (!strcmp(lineBuffer, "Mine")) {
				g_shipList[g_shipCount].category = 9;
			} else if (!strcmp(lineBuffer, "Satellite/Buoy")) {
				g_shipList[g_shipCount].category = 10;
			} else if (!strcmp(lineBuffer, "Droid")) {
				g_shipList[g_shipCount].category = 11;
			} else {
				continue;
			}

			token = strtok(NULL, ",\n");
			if (!strcmp(token, "Flyable")) {
				g_shipList[g_shipCount].flyable = 1;
			} else if (!strcmp(token, "GunnerFlyable")) {
				g_shipList[g_shipCount].flyable = 2;
			} else {
				g_shipList[g_shipCount].flyable = 0;
			}

			token = strtok(NULL, ",\n");
			if (!strcmp(token, "Known")) {
				g_shipList[g_shipCount].known = 1;
			} else {
				g_shipList[g_shipCount].known = 0;
			}

			token = strtok(NULL, ",\n");
			if (!strcmp(token, "Skirmish")) {
				g_shipList[g_shipCount].skirmish = 1;
			} else {
				g_shipList[g_shipCount].skirmish = 0;
			}

			strtok(NULL, ",\n");
			strtok(NULL, ",\n");
			strtok(NULL, ",\n");
			strtok(NULL, ",\n");
			token = strtok(NULL, ",\n");
			g_shipList[g_shipCount].iconRect.left = atoi(token);
			token = strtok(NULL, ",\n");
			g_shipList[g_shipCount].iconRect.top = atoi(token);
			token = strtok(NULL, ",\n");
			g_shipList[g_shipCount].iconRect.right = atoi(token);
			token = strtok(NULL, ",\n");
			g_shipList[g_shipCount].iconRect.bottom = atoi(token);

			++g_shipCount;
		}
	}

	qsort(g_shipList, (size_t)g_shipCount, sizeof(ShipListEntry), ShipList_CompareEntriesByCategory);
	File_Close(stream);
	memset(g_shipTypeToShipListIndex, 0, sizeof(g_shipTypeToShipListIndex));

	{
		unsigned int index;

		for (index = 0; index < (unsigned int)g_shipCount; ++index) {
			g_shipTypeToShipListIndex[g_shipList[index].typeId] = (int)index;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x52A210
int ShipList_CompareEntriesByCategory(const void* left, const void* right) {
	const ShipListEntry* leftEntry = (const ShipListEntry*)left;
	const ShipListEntry* rightEntry = (const ShipListEntry*)right;
	unsigned int category = (unsigned int)leftEntry->category;
	unsigned int rightCategory;

	rightCategory = (unsigned int)rightEntry->category;
	if (rightCategory > category) {
		return -1;
	}

	if (rightCategory < category) {
		return 1;
	}

	return Xwa_CrtStricmp(leftEntry->name, rightEntry->name);
}
