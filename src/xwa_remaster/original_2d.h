#ifndef XWA_REMASTER_ORIGINAL_2D_H
#define XWA_REMASTER_ORIGINAL_2D_H

#include "aeron/vfs.h"
#include "xwa_2d.h"

typedef struct XwaRemasterOriginal2d XwaRemasterOriginal2d;

typedef enum XwaRemasterOriginal2dLoadStatus {
	XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS = 0,
	XWA_REMASTER_ORIGINAL_2D_LOAD_MISSING,
	XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED,
} XwaRemasterOriginal2dLoadStatus;

XwaRemasterOriginal2d* XwaRemasterOriginal2d_Create(AeronVfs* vfs);
void XwaRemasterOriginal2d_Destroy(XwaRemasterOriginal2d* reader);

XwaRemasterOriginal2dLoadStatus
XwaRemasterOriginal2d_LoadFrontend(XwaRemasterOriginal2d* reader, const char* source_path,
								   Xwa2dFrameSet* out, char* error, size_t error_size);
XwaRemasterOriginal2dLoadStatus
XwaRemasterOriginal2d_LoadDatGroup(XwaRemasterOriginal2d* reader, int group, Xwa2dFrameSet* out,
								   char* error, size_t error_size);
XwaRemasterOriginal2dLoadStatus
XwaRemasterOriginal2d_LoadFrontendFont(XwaRemasterOriginal2d* reader, int point_size,
									   Xwa2dFontAtlas* out, char* error, size_t error_size);

#endif
