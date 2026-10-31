#include "mod_list.h"

#include "aeron/config_file.h"
#include "aeron/paths.h"
#include "aeron/vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ModFolders {
	char** names;
	size_t count;
	int failed;
} ModFolders;

static int mod_list_collect(void* userdata, const AeronVfsEntry* entry) {
	ModFolders* folders = (ModFolders*)userdata;
	char** grown = (char**)realloc(folders->names, (folders->count + 1) * sizeof *grown);
	char* name = grown ? (char*)malloc(strlen(entry->name) + 1) : NULL;
	if (grown) {
		folders->names = grown;
	}
	if (!name) {
		folders->failed = 1;
		return 0;
	}
	strcpy(name, entry->name);
	folders->names[folders->count++] = name;
	return 1;
}

static int mod_list_compare(const void* a, const void* b) {
	return strcmp(*(char* const*)a, *(char* const*)b);
}

static int mod_list_add(XwaModList* list, const char* name, const char* path, int complete) {
	XwaModEntry* grown = (XwaModEntry*)realloc(list->entries, (list->count + 1) * sizeof *grown);
	XwaModEntry* entry;
	if (!grown) {
		return 0;
	}
	list->entries = grown;
	entry = &grown[list->count++];
	memset(entry, 0, sizeof *entry);
	snprintf(entry->name, sizeof entry->name, "%s", name);
	snprintf(entry->path, sizeof entry->path, "%s", path);
	entry->complete = complete;
	return 1;
}

int XwaModList_SamePath(const char* a, const char* b) {
	size_t length_a = strlen(a);
	size_t length_b = strlen(b);
	while (length_a > 1 && (a[length_a - 1] == '/' || a[length_a - 1] == '\\')) {
		--length_a;
	}
	while (length_b > 1 && (b[length_b - 1] == '/' || b[length_b - 1] == '\\')) {
		--length_b;
	}
	return length_a == length_b && strncmp(a, b, length_a) == 0;
}

/* Adds the mod in folder `path`, named by its mod.yaml, else by `folder`.
 * With `manifest_required`, a folder without a mod.yaml isn't added. */
static int mod_list_add_folder(XwaModList* list, AeronVfs* vfs, const char* path, const char* folder,
							   int manifest_required) {
	AeronConfigFile* manifest = NULL;
	const char* name = folder;
	int complete = 1;
	int ok;

	for (size_t i = 0; i < list->count; ++i) {
		if (XwaModList_SamePath(list->entries[i].path, path)) {
			return 1;
		}
	}
	if (strlen(path) >= XWA_HOST_CONFIG_PATH_CAPACITY || !AeronVfs_SetRoot(vfs, AERON_VFS_ROOT_MOD, path)) {
		return 1;
	}
	if (AeronVfs_Exists(vfs, AERON_VFS_ROOT_MOD, "mod.yaml") &&
		AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_MOD, "mod.yaml", &manifest)) {
		name = AeronConfigFile_GetString(manifest, "name", folder);
		if (!name || !name[0]) {
			name = folder;
		}
		complete = AeronConfigFile_GetBool(manifest, "complete", 0);
	} else if (manifest_required) {
		return 1;
	}
	ok = mod_list_add(list, name, path, complete);
	AeronConfigFile_Destroy(manifest);
	return ok;
}

/* The last component of `path`, without a final separator. */
static void mod_list_folder_name(const char* path, char* out, size_t capacity) {
	size_t end = strlen(path);
	size_t start;
	while (end > 1 && (path[end - 1] == '/' || path[end - 1] == '\\')) {
		--end;
	}
	start = end;
	while (start > 0 && path[start - 1] != '/' && path[start - 1] != '\\') {
		--start;
	}
	snprintf(out, capacity, "%.*s", (int)(end - start), path + start);
}

/* Adds the mods of the mods folder, by folder name. */
static int mod_list_scan_folder(XwaModList* list, AeronVfs* vfs, const char* mods_folder) {
	ModFolders folders = { 0 };
	char path[XWA_HOST_CONFIG_PATH_CAPACITY];
	int ok = 1;

	AeronVfs_Glob(vfs, AERON_VFS_ROOT_ASSET, "", "*", AERON_VFS_GLOB_DIRECTORIES, mod_list_collect, &folders);
	if (folders.failed) {
		ok = 0;
	}
	if (folders.count) {
		qsort(folders.names, folders.count, sizeof *folders.names, mod_list_compare);
	}
	for (size_t i = 0; i < folders.count; ++i) {
		const size_t end = strlen(mods_folder);
		const char* separator =
			end && (mods_folder[end - 1] == '/' || mods_folder[end - 1] == '\\') ? "" : "/";
		const int length = snprintf(path, sizeof path, "%s%s%s", mods_folder, separator, folders.names[i]);
		if (ok && length > 0 && (size_t)length < sizeof path) {
			ok = mod_list_add_folder(list, vfs, path, folders.names[i], 1);
		}
		free(folders.names[i]);
	}
	free(folders.names);
	return ok;
}

int XwaModList_Folder(const char* configured, char* out, size_t capacity) {
	char local[XWA_HOST_CONFIG_PATH_CAPACITY];
	int length;

	if (configured && configured[0]) {
		length = snprintf(out, capacity, "%s", configured);
	} else if (Aeron_LocalDataPath(local, sizeof local)) {
		length = snprintf(out, capacity, "%smods", local);
	} else {
		return 0;
	}
	return length > 0 && (size_t)length < capacity;
}

int XwaModList_Scan(const char* mods_folder, const char* configured_mod, XwaModList* out) {
	AeronVfsConfig config;
	AeronVfs* vfs;
	int ok;

	memset(out, 0, sizeof *out);
	memset(&config, 0, sizeof config);
	/* Every root is set, so the scan doesn't create the user folder. */
	config.asset_root = mods_folder && mods_folder[0] ? mods_folder : ".";
	config.resource_root = config.asset_root;
	config.user_root = config.asset_root;
	config.temp_root = config.asset_root;
	vfs = AeronVfs_Create(&config);
	if (!vfs) {
		return 0;
	}
	ok = mod_list_add(out, "Original game", "", 1);
	if (ok && mods_folder && mods_folder[0]) {
		ok = mod_list_scan_folder(out, vfs, mods_folder);
	}
	if (ok && configured_mod && configured_mod[0]) {
		char folder[256];
		mod_list_folder_name(configured_mod, folder, sizeof folder);
		ok = mod_list_add_folder(out, vfs, configured_mod, folder[0] ? folder : configured_mod, 0);
	}
	AeronVfs_Destroy(vfs);
	if (!ok) {
		XwaModList_Free(out);
	}
	return ok;
}

void XwaModList_Free(XwaModList* list) {
	free(list->entries);
	list->entries = NULL;
	list->count = 0;
}
