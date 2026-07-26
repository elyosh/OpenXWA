#ifndef XWA_ASSETS_SHIP_LIST_H
#define XWA_ASSETS_SHIP_LIST_H

#include "xwa/assets/object_type.h"
#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	SHIP_LIST_MAX_ENTRIES = 256,
	SHIP_TYPE_TO_SHIP_LIST_CAPACITY = 256,
};

typedef struct ShipListEntry {
	char name[256];
	ObjectTypeId typeId;
	int category;
	int field264;
	int flyable;
	int known;
	int skirmish;
	FrontendRect iconRect;
} ShipListEntry;

extern ShipListEntry* g_shipList;
extern int g_shipTypeToShipListIndex[SHIP_TYPE_TO_SHIP_LIST_CAPACITY];
extern int g_shipCount;

int ShipList_Load(void);
int ShipList_CompareEntriesByCategory(const void* left, const void* right);

#ifdef __cplusplus
}
#endif

#endif
