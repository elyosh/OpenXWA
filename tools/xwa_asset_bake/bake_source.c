#include "bake_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int read_asset(AeronVfs* vfs, const char* path, uint8_t** out_data, size_t* out_size) {
	char normalized[512];
	char uppercase[512];
	size_t i = 0;
	for (; path[i] && i + 1 < sizeof normalized; i++) {
		normalized[i] = path[i] == '\\' ? '/' : path[i];
		uppercase[i] = normalized[i] >= 'a' && normalized[i] <= 'z'
						 ? (char)(normalized[i] - ('a' - 'A'))
						 : normalized[i];
	}
	normalized[i] = uppercase[i] = '\0';
	return AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_ASSET, normalized, 0, out_data, out_size) ||
		   AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_ASSET, uppercase, 0, out_data, out_size);
}

int BakeSource_LoadFrontend(AeronVfs* vfs, const char* source_path, Xwa2dFrameSet* out,
							char* error, size_t error_size) {
	char cbm_path[512];
	snprintf(cbm_path, sizeof cbm_path, "%s", source_path);
	char* extension = strrchr(cbm_path, '.');
	if (extension)
		snprintf(extension, (size_t)(cbm_path + sizeof cbm_path - extension), ".CBM");
	uint8_t* bytes = NULL;
	size_t size = 0;
	int result = 0;
	if (read_asset(vfs, cbm_path, &bytes, &size)) {
		result = Xwa2d_DecodeCbm(bytes, size, out, error, error_size);
	} else if (read_asset(vfs, source_path, &bytes, &size)) {
		extension = strrchr(source_path, '.');
		result = extension && strcasecmp(extension, ".FLC") == 0
				 ? Xwa2d_DecodeFlc(bytes, size, out, error, error_size)
				 : Xwa2d_DecodeBmp(bytes, size, out, error, error_size);
	} else if (error && error_size) {
		snprintf(error, error_size, "file not found");
	}
	free(bytes);
	return result;
}

int BakeSource_InitDatCatalog(AeronVfs* vfs, BakeDatCatalog* catalog, char* error, size_t error_size) {
	memset(catalog, 0, sizeof *catalog);
	catalog->groups = (uint16_t*)malloc(0x10000u * sizeof *catalog->groups);
	uint8_t* bytes = NULL;
	size_t size = 0;
	if (!catalog->groups || !read_asset(vfs, "RESDATA.TXT", &bytes, &size))
		goto failed;
	size_t cursor = 0;
	while (cursor < size && catalog->file_count < 64) {
		const size_t start = cursor;
		while (cursor < size && bytes[cursor] != '\r' && bytes[cursor] != '\n')
			cursor++;
		const size_t length = cursor - start;
		while (cursor < size && (bytes[cursor] == '\r' || bytes[cursor] == '\n'))
			cursor++;
		if (!length || length >= 512)
			continue;
		char path[512];
		memcpy(path, bytes + start, length);
		path[length] = '\0';
		BakeDatFile* file = &catalog->files[catalog->file_count];
		if (!read_asset(vfs, path, &file->bytes, &file->size))
			continue;
		if (!Xwa2d_DatListGroups(file->bytes, file->size, catalog->groups, 0x10000,
								 &catalog->group_count, error, error_size))
			goto failed;
		catalog->file_count++;
	}
	free(bytes);
	if (catalog->file_count > 0)
		return 1;
	if (error && error_size)
		snprintf(error, error_size, "no DAT files found");
	return 0;

failed:
	free(bytes);
	BakeSource_FreeDatCatalog(catalog);
	if (error && error_size && !error[0])
		snprintf(error, error_size, "RESDATA catalog read failed");
	return 0;
}

int BakeSource_LoadDatGroup(const BakeDatCatalog* catalog, uint16_t group, Xwa2dFrameSet* out,
							char* error, size_t error_size) {
	memset(out, 0, sizeof *out);
	for (int i = 0; i < catalog->file_count; i++) {
		if (!Xwa2d_DatAppendGroup(catalog->files[i].bytes, catalog->files[i].size, group, out,
								  error, error_size)) {
			Xwa2dFrameSet_Free(out);
			return 0;
		}
	}
	if (out->count > 0)
		return 1;
	if (error && error_size)
		snprintf(error, error_size, "DAT group %u not found", group);
	return 0;
}

void BakeSource_FreeDatCatalog(BakeDatCatalog* catalog) {
	if (!catalog)
		return;
	for (int i = 0; i < catalog->file_count; i++)
		free(catalog->files[i].bytes);
	free(catalog->groups);
	memset(catalog, 0, sizeof *catalog);
}
