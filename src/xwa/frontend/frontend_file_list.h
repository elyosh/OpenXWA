#ifndef XWA_FRONTEND_FRONTEND_FILE_LIST_H
#define XWA_FRONTEND_FRONTEND_FILE_LIST_H

#include "aeron/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FrontFilenameListNode {
	char* fileName;
	struct FrontFilenameListNode* next;
} FrontFilenameListNode;

typedef struct FrontFilenameList {
	FrontFilenameListNode* head;
	int count;
} FrontFilenameList;

#ifdef XWA_MODERN
FrontFilenameList* FrontendFileList_BuildSorted(AeronVfsRoot root, const char* wildcard);
#else
FrontFilenameList* FrontendFileList_BuildSorted(const char* wildcard);
#ifndef XWA_FRONTEND_FILE_LIST_IMPLEMENTATION
#define FrontendFileList_BuildSorted(root, wildcard) FrontendFileList_BuildSorted(wildcard)
#endif
#endif
void FrontendFileList_Free(FrontFilenameList* list);
FrontFilenameList* FrontendFileList_InsertNodeSorted(FrontFilenameList* list, FrontFilenameListNode* node);

#ifdef __cplusplus
}
#endif

#endif
