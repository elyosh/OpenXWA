#define XWA_FRONTEND_FILE_LIST_IMPLEMENTATION
#include "xwa/frontend/frontend_file_list.h"

#include "xwa/assets/file_io.h"
#include "xwa/util/memory.h"

#include <string.h>

typedef struct FrontFilenameListBuildState {
	FrontFilenameList* list;
	int allocFailed;
} FrontFilenameListBuildState;

static int FrontendFileList_AddGlobEntry(void* userdata, const XwaFileEntry* entry) {
	FrontFilenameListBuildState* state;
	FrontFilenameListNode* node;
	char* storedName;

	state = (FrontFilenameListBuildState*)userdata;
	if (state->allocFailed || entry->is_directory) {
		return !state->allocFailed;
	}

	node = (FrontFilenameListNode*)Mem_Alloc(sizeof(*node));
	if (node == 0) {
		state->allocFailed = 1;
		return 0;
	}

	storedName = (char*)Mem_Alloc(strlen(entry->name) + 2);
	node->fileName = storedName;
	if (storedName == 0) {
		Mem_Free(node);
		state->allocFailed = 1;
		return 0;
	}

	strcpy(storedName, entry->name);
	node->next = 0;

	if (state->list->head == 0) {
		state->list->head = node;
		state->list->count = 1;
	} else {
		FrontendFileList_InsertNodeSorted(state->list, node);
	}

	return 1;
}

// FUNCTION: XWA 0x56D6C0
#ifdef XWA_MODERN
FrontFilenameList* FrontendFileList_BuildSorted(AeronVfsRoot root, const char* wildcard) {
#else
FrontFilenameList* FrontendFileList_BuildSorted(const char* wildcard) {
#endif
	FrontFilenameList* list;
	FrontFilenameListBuildState state;

	list = (FrontFilenameList*)Mem_Alloc(sizeof(*list));
	if (list == 0) {
		return 0;
	}

	list->head = 0;
	list->count = 0;

	state.list = list;
	state.allocFailed = 0;

	File_Glob(
#ifdef XWA_MODERN
		root,
#else
		AERON_VFS_ROOT_ASSET,
#endif
		wildcard, AERON_VFS_GLOB_FILES | AERON_VFS_GLOB_CASE_INSENSITIVE, FrontendFileList_AddGlobEntry,
		&state);

	if (state.allocFailed && list->count == 0) {
		FrontendFileList_Free(list);
		return 0;
	}

	return list;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x56D8B0
void FrontendFileList_Free(FrontFilenameList* list) {
	FrontFilenameListNode* node;

	if (list == 0) {
		return;
	}

	node = list->head;
	while (node != 0) {
		FrontFilenameListNode* next = node->next;

		Mem_Free(node->fileName);
		Mem_Free(node);
		node = next;
	}

	Mem_Free(list);
	return;
}

// FUNCTION: XWA 0x56D8F0
FrontFilenameList* FrontendFileList_InsertNodeSorted(FrontFilenameList* list, FrontFilenameListNode* node) {
	FrontFilenameListNode* current;
	FrontFilenameListNode* next;
	char* fileName;

	fileName = node->fileName;
	current = list->head;
	if (strcmp(fileName, current->fileName) < 0) {
		node->next = current;
		list->head = node;
		++list->count;
		return list;
	}

	next = current->next;
	while (next != 0) {
		if (strcmp(fileName, next->fileName) < 0) {
			node->next = current->next;
			current->next = node;
			++list->count;
			return list;
		}

		current = next;
		next = next->next;
	}

	current->next = node;
	++list->count;
	return list;
}
