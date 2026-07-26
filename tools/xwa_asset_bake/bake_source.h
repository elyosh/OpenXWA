#ifndef XWA_ASSET_BAKE_SOURCE_H
#define XWA_ASSET_BAKE_SOURCE_H

#include "aeron/vfs.h"
#include "xwa_2d.h"

typedef struct BakeDatFile {
	uint8_t* bytes;
	size_t size;
} BakeDatFile;

typedef struct BakeDatCatalog {
	BakeDatFile files[64];
	int file_count;
	uint16_t* groups;
	int group_count;
} BakeDatCatalog;

int BakeSource_LoadFrontend(AeronVfs* vfs, const char* source_path, Xwa2dFrameSet* out,
							char* error, size_t error_size);
int BakeSource_InitDatCatalog(AeronVfs* vfs, BakeDatCatalog* catalog, char* error, size_t error_size);
int BakeSource_LoadDatGroup(const BakeDatCatalog* catalog, uint16_t group, Xwa2dFrameSet* out,
							char* error, size_t error_size);
void BakeSource_FreeDatCatalog(BakeDatCatalog* catalog);

#endif
