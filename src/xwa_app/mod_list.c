#include "mod_list.h"

#include "mod_format.h"

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

static int mod_list_add(XwaModList* list, const XwaModEntry* mod) {
	XwaModEntry* grown = (XwaModEntry*)realloc(list->entries, (list->count + 1) * sizeof *grown);
	if (!grown) {
		return 0;
	}
	list->entries = grown;
	grown[list->count++] = *mod;
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

/* Sets the state and the problem text of `mod` from its mod.yaml. The text
 * tells the player to import the mod again when the importer made it. */
static void mod_list_check(const AeronConfigFile* manifest, XwaModEntry* mod) {
	const int64_t version = AeronConfigFile_GetInt(manifest, "version", 0);
	const int imported = AeronConfigFile_Has(manifest, "imported");

	mod->problem[0] = '\0';
	if (version != XWA_MOD_FORMAT_VERSION) {
		mod->state = XWA_MOD_OTHER_VERSION;
		if (imported) {
			snprintf(mod->problem, sizeof mod->problem,
					 "Made for another version of OpenXWA. Import it again.");
		} else if (!AeronConfigFile_Has(manifest, "version")) {
			snprintf(mod->problem, sizeof mod->problem,
					 "Its mod.yaml has no version, and this OpenXWA needs version %d.",
					 XWA_MOD_FORMAT_VERSION);
		} else {
			snprintf(mod->problem, sizeof mod->problem,
					 "Made for mod format version %lld, and this OpenXWA needs version %d.",
					 (long long)version, XWA_MOD_FORMAT_VERSION);
		}
	} else if (!AeronConfigFile_GetBool(manifest, "complete", 1)) {
		mod->state = XWA_MOD_UNFINISHED;
		snprintf(mod->problem, sizeof mod->problem, "%s",
				 imported ? "Its import didn't finish. Import it again."
						  : "Its mod.yaml says that it isn't complete.");
	} else {
		mod->state = XWA_MOD_READY;
	}
}

/* Reads the mod.yaml of folder `path` with `vfs`. Returns 0 when there is
 * none. */
static int mod_list_read(AeronVfs* vfs, const char* path, XwaModEntry* out) {
	AeronConfigFile* manifest = NULL;
	AeronConfigError error;
	const char* name;
	char folder[256];

	memset(out, 0, sizeof *out);
	if (strlen(path) >= sizeof out->path || !AeronVfs_SetRoot(vfs, AERON_VFS_ROOT_MOD, path) ||
		!AeronVfs_Exists(vfs, AERON_VFS_ROOT_MOD, "mod.yaml")) {
		return 0;
	}
	mod_list_folder_name(path, folder, sizeof folder);
	snprintf(out->path, sizeof out->path, "%s", path);
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(vfs, AERON_VFS_ROOT_MOD, "mod.yaml", &manifest, &error)) {
		snprintf(out->name, sizeof out->name, "%s", folder[0] ? folder : path);
		out->state = XWA_MOD_UNREADABLE;
		if (error.line > 0) {
			snprintf(out->problem, sizeof out->problem, "Its mod.yaml has an error on line %d: %s",
					 error.line, error.message);
		} else {
			snprintf(out->problem, sizeof out->problem, "Its mod.yaml can't be read.");
		}
		return 1;
	}
	name = AeronConfigFile_GetString(manifest, "name", NULL);
	snprintf(out->name, sizeof out->name, "%s", name && name[0] ? name : folder[0] ? folder : path);
	mod_list_check(manifest, out);
	AeronConfigFile_Destroy(manifest);
	return 1;
}

/* Adds the mod in folder `path` when it has a mod.yaml and isn't listed
 * yet. */
static int mod_list_add_folder(XwaModList* list, AeronVfs* vfs, const char* path) {
	XwaModEntry mod;

	for (size_t i = 0; i < list->count; ++i) {
		if (XwaModList_SamePath(list->entries[i].path, path)) {
			return 1;
		}
	}
	return !mod_list_read(vfs, path, &mod) || mod_list_add(list, &mod);
}

/* Adds the mods of the mods folder, by folder name. */
static int mod_list_scan_folder(XwaModList* list, AeronVfs* vfs, const char* mods_folder) {
	ModFolders folders = { 0 };
	char path[XWA_INSTALL_PATH_CAPACITY];
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
			ok = mod_list_add_folder(list, vfs, path);
		}
		free(folders.names[i]);
	}
	free(folders.names);
	return ok;
}

int XwaModList_Folder(const char* configured, char* out, size_t capacity) {
	char local[XWA_INSTALL_PATH_CAPACITY];
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

/* A VFS for reading mod.yaml files. Every root is set to `root`, so it
 * doesn't create the user folder. */
static AeronVfs* mod_list_vfs(const char* root) {
	AeronVfsConfig config;

	memset(&config, 0, sizeof config);
	config.asset_root = root && root[0] ? root : ".";
	config.resource_root = config.asset_root;
	config.user_root = config.asset_root;
	config.temp_root = config.asset_root;
	return AeronVfs_Create(&config);
}

int XwaModList_IsFolder(const char* path) {
	AeronVfs* vfs;
	AeronFileInfo info;
	int found;

	if (!path || !path[0]) {
		return 0;
	}
	vfs = mod_list_vfs(path);
	found = vfs && AeronVfs_Stat(vfs, AERON_VFS_ROOT_ASSET, ".", &info) && info.is_directory;
	AeronVfs_Destroy(vfs);
	return found;
}

int XwaModList_Read(const char* path, XwaModEntry* out) {
	AeronVfs* vfs = mod_list_vfs(NULL);
	int found;

	memset(out, 0, sizeof *out);
	if (!vfs || !path || !path[0]) {
		AeronVfs_Destroy(vfs);
		return 0;
	}
	found = mod_list_read(vfs, path, out);
	AeronVfs_Destroy(vfs);
	return found;
}

int XwaModList_Scan(const char* mods_folder, const char* configured_mod, XwaModList* out) {
	const XwaModEntry original = { .name = "Original game", .path = "", .state = XWA_MOD_READY };
	AeronVfs* vfs;
	int ok;

	memset(out, 0, sizeof *out);
	vfs = mod_list_vfs(mods_folder);
	if (!vfs) {
		return 0;
	}
	ok = mod_list_add(out, &original);
	if (ok && mods_folder && mods_folder[0]) {
		ok = mod_list_scan_folder(out, vfs, mods_folder);
	}
	if (ok && configured_mod && configured_mod[0]) {
		ok = mod_list_add_folder(out, vfs, configured_mod);
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
