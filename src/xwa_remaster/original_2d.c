#include "xwa_remaster/original_2d.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ORIGINAL_2D_MAX_FILE_BYTES (64u * 1024u * 1024u)
#define ORIGINAL_2D_MAX_DAT_FILES 64
#define ORIGINAL_2D_MAX_DAT_GROUPS 512
#define ORIGINAL_2D_PATH_MAX 512

typedef struct OriginalDatFile {
	char path[ORIGINAL_2D_PATH_MAX];
	uint16_t groups[ORIGINAL_2D_MAX_DAT_GROUPS];
	int group_count;
} OriginalDatFile;

struct XwaRemasterOriginal2d {
	AeronVfs* vfs;
	OriginalDatFile dat_files[ORIGINAL_2D_MAX_DAT_FILES];
	int dat_file_count;
	int dat_paths_loaded;
};

static void original_normalize_path(const char* source, char* out, size_t capacity, int uppercase) {
	size_t count = 0;
	for (; source && *source && count + 1 < capacity; source++) {
		unsigned char c = (unsigned char)*source;
		if (c == '\\')
			c = '/';
		if (uppercase)
			c = (unsigned char)toupper(c);
		out[count++] = (char)c;
	}
	out[count] = '\0';
}

static XwaRemasterOriginal2dLoadStatus
original_read_candidate(XwaRemasterOriginal2d* reader, AeronVfsRoot root, const char* path,
						uint8_t** out_bytes, size_t* out_size) {
	if (!AeronVfs_Exists(reader->vfs, root, path))
		return XWA_REMASTER_ORIGINAL_2D_LOAD_MISSING;
	return AeronVfs_ReadAll(reader->vfs, root, path, ORIGINAL_2D_MAX_FILE_BYTES, out_bytes, out_size)
			   ? XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS
			   : XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
}

static XwaRemasterOriginal2dLoadStatus
original_read_asset(XwaRemasterOriginal2d* reader, AeronVfsRoot root, const char* path,
					uint8_t** out_bytes, size_t* out_size) {
	char normalized[ORIGINAL_2D_PATH_MAX];
	XwaRemasterOriginal2dLoadStatus status;
	original_normalize_path(path, normalized, sizeof normalized, 0);
	status = original_read_candidate(reader, root, normalized, out_bytes, out_size);
	if (status != XWA_REMASTER_ORIGINAL_2D_LOAD_MISSING)
		return status;
	original_normalize_path(path, normalized, sizeof normalized, 1);
	return original_read_candidate(reader, root, normalized, out_bytes, out_size);
}

static void original_cbm_path(const char* source, char out[ORIGINAL_2D_PATH_MAX]) {
	snprintf(out, ORIGINAL_2D_PATH_MAX, "%s", source ? source : "");
	char* slash = strrchr(out, '/');
	char* backslash = strrchr(out, '\\');
	char* base = slash;
	if (!base || (backslash && backslash > base))
		base = backslash;
	char* dot = strrchr(base ? base + 1 : out, '.');
	if (dot)
		snprintf(dot, (size_t)(out + ORIGINAL_2D_PATH_MAX - dot), ".cbm");
	else if (strlen(out) + 4 < ORIGINAL_2D_PATH_MAX)
		strcat(out, ".cbm");
}

static int original_extension_is(const char* path, const char* extension) {
	const char* actual = strrchr(path, '.');
	if (!actual)
		return 0;
	while (*actual && *extension) {
		if (toupper((unsigned char)*actual) != toupper((unsigned char)*extension))
			return 0;
		actual++;
		extension++;
	}
	return *actual == '\0' && *extension == '\0';
}

XwaRemasterOriginal2d* XwaRemasterOriginal2d_Create(AeronVfs* vfs) {
	if (!vfs)
		return NULL;
	XwaRemasterOriginal2d* reader = (XwaRemasterOriginal2d*)calloc(1, sizeof *reader);
	if (reader)
		reader->vfs = vfs;
	return reader;
}

void XwaRemasterOriginal2d_Destroy(XwaRemasterOriginal2d* reader) { free(reader); }

XwaRemasterOriginal2dLoadStatus
XwaRemasterOriginal2d_LoadFrontend(XwaRemasterOriginal2d* reader, const char* source_path,
								   Xwa2dFrameSet* out, char* error, size_t error_size) {
	if (!reader || !source_path || !source_path[0] || !out)
		return XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
	memset(out, 0, sizeof *out);
	uint8_t* bytes = NULL;
	size_t size = 0;
	char cbm[ORIGINAL_2D_PATH_MAX];
	original_cbm_path(source_path, cbm);
	XwaRemasterOriginal2dLoadStatus status =
		original_read_asset(reader, AERON_VFS_ROOT_USER, cbm, &bytes, &size);
	if (status == XWA_REMASTER_ORIGINAL_2D_LOAD_MISSING) {
		status = original_read_asset(reader, AERON_VFS_ROOT_ASSET, cbm, &bytes, &size);
	}
	if (status == XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS) {
		int ok = Xwa2d_DecodeCbm(bytes, size, out, error, error_size);
		free(bytes);
		return ok ? XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS
				  : XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
	}
	if (status != XWA_REMASTER_ORIGINAL_2D_LOAD_MISSING)
		return status;
	status = original_read_asset(reader, AERON_VFS_ROOT_ASSET, source_path, &bytes, &size);
	if (status != XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS) {
		if (error && error_size)
			snprintf(error, error_size, "original frontend file %s: %s",
					 status == XWA_REMASTER_ORIGINAL_2D_LOAD_MISSING ? "not found" : "read failed",
					 source_path);
		return status;
	}
	int ok = original_extension_is(source_path, ".flc")
				 ? Xwa2d_DecodeFlc(bytes, size, out, error, error_size)
				 : Xwa2d_DecodeBmp(bytes, size, out, error, error_size);
	free(bytes);
	return ok ? XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS
			  : XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
}

static XwaRemasterOriginal2dLoadStatus
original_load_dat_paths(XwaRemasterOriginal2d* reader, char* error, size_t error_size) {
	if (reader->dat_paths_loaded)
		return XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS;
	uint8_t* bytes = NULL;
	size_t size = 0;
	XwaRemasterOriginal2dLoadStatus status =
		original_read_asset(reader, AERON_VFS_ROOT_ASSET, "RESDATA.TXT", &bytes, &size);
	if (status != XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS) {
		if (error && error_size)
			snprintf(error, error_size, "RESDATA.TXT %s",
					 status == XWA_REMASTER_ORIGINAL_2D_LOAD_MISSING ? "not found" : "read failed");
		return status;
	}
	size_t cursor = 0;
	while (cursor < size && reader->dat_file_count < ORIGINAL_2D_MAX_DAT_FILES) {
		size_t start = cursor;
		while (cursor < size && bytes[cursor] != '\r' && bytes[cursor] != '\n')
			cursor++;
		size_t length = cursor - start;
		while (cursor < size && (bytes[cursor] == '\r' || bytes[cursor] == '\n'))
			cursor++;
		if (!length || length >= ORIGINAL_2D_PATH_MAX)
			continue;
		OriginalDatFile* dat_file = &reader->dat_files[reader->dat_file_count];
		memset(dat_file, 0, sizeof *dat_file);
		char* path = dat_file->path;
		memcpy(path, bytes + start, length);
		path[length] = '\0';
		uint8_t* dat_bytes = NULL;
		size_t dat_size = 0;
		status = original_read_asset(reader, AERON_VFS_ROOT_ASSET, path, &dat_bytes, &dat_size);
		if (status != XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS) {
			if (error && error_size)
				snprintf(error, error_size, "listed DAT unavailable: %s", path);
			free(bytes);
			return XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
		}
		char dat_error[128] = { 0 };
		const int listed = Xwa2d_DatListGroups(dat_bytes, dat_size, dat_file->groups,
											ORIGINAL_2D_MAX_DAT_GROUPS, &dat_file->group_count,
											dat_error, sizeof dat_error);
		free(dat_bytes);
		if (!listed) {
			if (error && error_size)
				snprintf(error, error_size, "%s: %s", path, dat_error);
			free(bytes);
			return XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
		}
		reader->dat_file_count++;
	}
	free(bytes);
	if (!reader->dat_file_count) {
		if (error && error_size)
			snprintf(error, error_size, "RESDATA.TXT contains no DAT files");
		return XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
	}
	if (cursor < size) {
		if (error && error_size)
			snprintf(error, error_size, "RESDATA.TXT exceeds DAT file capacity");
		return XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
	}
	reader->dat_paths_loaded = 1;
	return XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS;
}

static int original_dat_contains_group(const OriginalDatFile* file, uint16_t group) {
	for (int i = 0; i < file->group_count; i++) {
		if (file->groups[i] == group)
			return 1;
	}
	return 0;
}

XwaRemasterOriginal2dLoadStatus
XwaRemasterOriginal2d_LoadDatGroup(XwaRemasterOriginal2d* reader, int group, Xwa2dFrameSet* out,
								   char* error, size_t error_size) {
	if (!reader || !out || group < 0 || group > 0xffff)
		return XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
	memset(out, 0, sizeof *out);
	XwaRemasterOriginal2dLoadStatus status =
		original_load_dat_paths(reader, error, error_size);
	if (status != XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS)
		return status;
	for (int i = 0; i < reader->dat_file_count; i++) {
		const OriginalDatFile* file = &reader->dat_files[i];
		if (!original_dat_contains_group(file, (uint16_t)group))
			continue;
		uint8_t* bytes = NULL;
		size_t size = 0;
		status = original_read_asset(reader, AERON_VFS_ROOT_ASSET, file->path, &bytes, &size);
		if (status != XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS) {
			if (error && error_size)
				snprintf(error, error_size, "DAT group %d source unavailable: %s", group, file->path);
			Xwa2dFrameSet_Free(out);
			return XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
		}
		int ok = Xwa2d_DatAppendGroup(bytes, size, (uint16_t)group, out, error, error_size);
		free(bytes);
		if (!ok) {
			Xwa2dFrameSet_Free(out);
			return XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
		}
	}
	if (out->count)
		return XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS;
	if (error && error_size)
		snprintf(error, error_size, "DAT group %d not found", group);
	return XWA_REMASTER_ORIGINAL_2D_LOAD_MISSING;
}

XwaRemasterOriginal2dLoadStatus
XwaRemasterOriginal2d_LoadFrontendFont(XwaRemasterOriginal2d* reader, int point_size,
									   Xwa2dFontAtlas* out, char* error, size_t error_size) {
	if (!reader || !out || point_size <= 0 || point_size > 255)
		return XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
	char path[64];
	snprintf(path, sizeof path, "TIMES%d.ABP", point_size);
	uint8_t* bytes = NULL;
	size_t size = 0;
	XwaRemasterOriginal2dLoadStatus status =
		original_read_asset(reader, AERON_VFS_ROOT_ASSET, path, &bytes, &size);
	if (status != XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS) {
		if (error && error_size)
			snprintf(error, error_size, "frontend font %d %s", point_size,
					 status == XWA_REMASTER_ORIGINAL_2D_LOAD_MISSING ? "not found" : "read failed");
		return status;
	}
	int ok = Xwa2d_DecodeAbpFont(bytes, size, out, error, error_size);
	free(bytes);
	return ok ? XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS
			  : XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;
}
