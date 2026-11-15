#include "mod_update.h"

#include "aeron/log.h"
#include "aeron/sync.h"
#include "aeron/vfs.h"
#include "install_paths.h"
#include "mod_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOD_UPDATE_PATH_CAP XWA_INSTALL_PATH_CAPACITY
#define STAGING_DIR ".openxwa-update"
#define TRASH_DIR ".openxwa-trash"
#define DELETE_MARKER ".openxwa-delete"

/* ---- Paths -------------------------------------------------------------- */

static bool is_separator(char c) { return c == '/' || c == '\\'; }

/* Splits folder `path` into its parent folder and its name. */
static bool split_path(const char* path, char* parent, size_t parent_cap, char* name, size_t name_cap) {
	size_t end = path ? strlen(path) : 0;
	while (end > 1 && is_separator(path[end - 1]))
		--end;
	size_t start = end;
	while (start > 0 && !is_separator(path[start - 1]))
		--start;
	if (start == end)
		return false;
	const int name_length = snprintf(name, name_cap, "%.*s", (int)(end - start), path + start);
	int parent_length;
	if (start == 0)
		parent_length = snprintf(parent, parent_cap, ".");
	else if (start == 1)
		parent_length = snprintf(parent, parent_cap, "%c", path[0]);
	else
		parent_length = snprintf(parent, parent_cap, "%.*s", (int)(start - 1), path);
	return name_length > 0 && (size_t)name_length < name_cap && parent_length > 0 &&
		   (size_t)parent_length < parent_cap;
}

static bool join_path(char* out, size_t capacity, const char* folder, const char* relative) {
	const size_t end = strlen(folder);
	const char* separator = end && is_separator(folder[end - 1]) ? "" : "/";
	const int length = snprintf(out, capacity, "%s%s%s", folder, separator, relative);
	return length > 0 && (size_t)length < capacity;
}

/* A VFS whose roots are all `root`, as in mod_list.c. */
static AeronVfs* open_vfs(const char* root) {
	AeronVfsConfig config;
	memset(&config, 0, sizeof config);
	config.asset_root = root;
	config.resource_root = root;
	config.user_root = root;
	config.temp_root = root;
	return AeronVfs_Create(&config);
}

static bool exists(AeronVfs* vfs, const char* relative) {
	return AeronVfs_Exists(vfs, AERON_VFS_ROOT_ASSET, relative) != 0;
}

/* Whether folder `path` holds a mod that the importer made. */
static bool is_imported(const char* path) {
	XwaModEntry entry;
	return XwaModList_Read(path, &entry) && entry.imported;
}

/* Whether folder `path` holds a complete import for this version. */
static bool is_ready_import(const char* path) {
	XwaModEntry entry;
	return XwaModList_Read(path, &entry) && entry.imported && entry.state == XWA_MOD_READY;
}

/* A rename fails on Windows while a program, such as an antivirus, has a
   file of the folder open. The caller leaves things as they are, and the
   next call tries again. */
static bool rename_folder(AeronVfs* vfs, const char* from, const char* to) {
	if (AeronVfs_Rename(vfs, AERON_VFS_ROOT_ASSET, from, to))
		return true;
	Aeron_LogWarn("xwa.mods", "can't rename %s to %s", from, to);
	return false;
}

/* Moves folder `relative` of the parent `vfs` into the trash folder, under
   its name, or under the first free "<name>-<n>" when an earlier one is still
   there. */
static bool move_to_trash(AeronVfs* vfs, const char* relative, const char* name) {
	char target[MOD_UPDATE_PATH_CAP];
	if (!AeronVfs_CreateDirectory(vfs, AERON_VFS_ROOT_ASSET, TRASH_DIR))
		return false;
	for (int n = 1; n < 1000; ++n) {
		const int length = n == 1 ? snprintf(target, sizeof target, "%s/%s", TRASH_DIR, name)
								  : snprintf(target, sizeof target, "%s/%s-%d", TRASH_DIR, name, n);
		if (length <= 0 || (size_t)length >= sizeof target)
			return false;
		if (!exists(vfs, target))
			return rename_folder(vfs, relative, target);
	}
	return false;
}

/* Removes folder `relative` of `vfs` when it is empty. */
static void remove_if_empty(AeronVfs* vfs, const char* relative) {
	AeronVfs_Remove(vfs, AERON_VFS_ROOT_ASSET, relative);
}

/* ---- Deleting ----------------------------------------------------------- */

typedef struct NameList {
	char** names;
	unsigned char* folders;
	size_t count, capacity;
	bool failed;
} NameList;

static int collect_name(void* user, const AeronVfsEntry* entry) {
	NameList* list = user;
	if (list->count == list->capacity) {
		const size_t capacity = list->capacity ? list->capacity * 2 : 32;
		char** names = realloc(list->names, capacity * sizeof *names);
		unsigned char* folders = names ? realloc(list->folders, capacity) : NULL;
		if (names)
			list->names = names;
		if (!names || !folders) {
			list->failed = true;
			return 0;
		}
		list->folders = folders;
		list->capacity = capacity;
	}
	const size_t length = strlen(entry->name);
	char* name = malloc(length + 1);
	if (!name) {
		list->failed = true;
		return 0;
	}
	memcpy(name, entry->name, length + 1);
	list->names[list->count] = name;
	list->folders[list->count] = (unsigned char)(entry->is_directory != 0);
	++list->count;
	return 1;
}

static void free_names(NameList* list) {
	for (size_t i = 0; i < list->count; ++i)
		free(list->names[i]);
	free(list->names);
	free(list->folders);
	memset(list, 0, sizeof *list);
}

/* Deletes what folder `relative` of `vfs` holds, then the folder. With
   `files` false, a file stops it: only empty folders are deleted. At the
   top, mod.yaml is deleted last. */
static bool delete_tree(AeronVfs* vfs, const char* relative, bool files, bool top, volatile int* stop) {
	NameList list = { 0 };
	char path[MOD_UPDATE_PATH_CAP];
	bool ok = AeronVfs_Glob(vfs, AERON_VFS_ROOT_ASSET, relative, "*", 0, collect_name, &list) && !list.failed;
	bool has_mod_yaml = false;
	for (size_t i = 0; ok && i < list.count; ++i) {
		if (stop && Aeron_AtomicLoad(stop)) {
			ok = false;
			break;
		}
		if (top && !list.folders[i] && strcmp(list.names[i], "mod.yaml") == 0) {
			has_mod_yaml = true;
			continue;
		}
		const int length = snprintf(path, sizeof path, "%s/%s", relative, list.names[i]);
		if (length <= 0 || (size_t)length >= sizeof path)
			ok = false;
		else if (list.folders[i])
			ok = delete_tree(vfs, path, files, false, stop);
		else
			ok = files && AeronVfs_Remove(vfs, AERON_VFS_ROOT_ASSET, path);
	}
	free_names(&list);
	if (ok && has_mod_yaml) {
		ok = snprintf(path, sizeof path, "%s/mod.yaml", relative) > 0 &&
			 AeronVfs_Remove(vfs, AERON_VFS_ROOT_ASSET, path);
	}
	return ok && AeronVfs_Remove(vfs, AERON_VFS_ROOT_ASSET, relative);
}

bool XwaModUpdate_DeleteFolder(const char* dir, volatile int* stop) {
	char parent[MOD_UPDATE_PATH_CAP], name[MOD_UPDATE_PATH_CAP];
	if (!split_path(dir, parent, sizeof parent, name, sizeof name))
		return false;
	AeronVfs* vfs = open_vfs(parent);
	if (!vfs)
		return false;
	bool ok = true;
	if (exists(vfs, name))
		ok = delete_tree(vfs, name, is_imported(dir), true, stop);
	AeronVfs_Destroy(vfs);
	return ok;
}

/* ---- Updates ------------------------------------------------------------ */

bool XwaModUpdate_StagingFolder(const char* mod, char* out, size_t capacity) {
	char parent[MOD_UPDATE_PATH_CAP], name[MOD_UPDATE_PATH_CAP], relative[MOD_UPDATE_PATH_CAP];
	return split_path(mod, parent, sizeof parent, name, sizeof name) &&
		   snprintf(relative, sizeof relative, "%s/%s", STAGING_DIR, name) > 0 &&
		   join_path(out, capacity, parent, relative);
}

bool XwaModUpdate_HasUpdate(const char* mod) {
	char staging[MOD_UPDATE_PATH_CAP];
	return XwaModUpdate_StagingFolder(mod, staging, sizeof staging) && is_ready_import(staging);
}

bool XwaModUpdate_HasStaging(const char* mod) {
	char staging[MOD_UPDATE_PATH_CAP];
	return XwaModUpdate_StagingFolder(mod, staging, sizeof staging) && XwaModList_IsFolder(staging);
}

XwaModUpdateResult XwaModUpdate_Apply(const char* mod) {
	char parent[MOD_UPDATE_PATH_CAP], name[MOD_UPDATE_PATH_CAP];
	char staging[MOD_UPDATE_PATH_CAP], trash[MOD_UPDATE_PATH_CAP];
	char staging_path[MOD_UPDATE_PATH_CAP], trash_path[MOD_UPDATE_PATH_CAP], marker[MOD_UPDATE_PATH_CAP];
	if (!mod || !mod[0] || !split_path(mod, parent, sizeof parent, name, sizeof name) ||
		snprintf(staging, sizeof staging, "%s/%s", STAGING_DIR, name) <= 0 ||
		snprintf(trash, sizeof trash, "%s/%s", TRASH_DIR, name) <= 0 ||
		snprintf(marker, sizeof marker, "%s/%s", trash, DELETE_MARKER) <= 0 ||
		!join_path(staging_path, sizeof staging_path, parent, staging) ||
		!join_path(trash_path, sizeof trash_path, parent, trash))
		return XWA_MOD_UPDATE_NONE;
	AeronVfs* vfs = open_vfs(parent);
	if (!vfs)
		return XWA_MOD_UPDATE_NONE;
	XwaModUpdateResult result = XWA_MOD_UPDATE_NONE;
	const bool ready = is_ready_import(staging_path);
	if (!exists(vfs, name)) {
		/* A swap stopped after the old mod went to the trash: finish it, or
		   put the old mod back. A mod in the trash for a delete stays
		   there. */
		if (ready)
			result = rename_folder(vfs, staging, name) ? XWA_MOD_UPDATE_APPLIED : XWA_MOD_UPDATE_FAILED;
		else if (exists(vfs, trash) && !exists(vfs, marker) && is_imported(trash_path))
			result = rename_folder(vfs, trash, name) ? XWA_MOD_UPDATE_NONE : XWA_MOD_UPDATE_FAILED;
	} else if (ready && is_imported(mod)) {
		/* The trash name is the one that a swap that stops halfway leaves,
		   so an earlier mod still there moves aside first. */
		if (exists(vfs, trash) && !move_to_trash(vfs, trash, name))
			result = XWA_MOD_UPDATE_FAILED;
		else if (!AeronVfs_CreateDirectory(vfs, AERON_VFS_ROOT_ASSET, TRASH_DIR) ||
				 !rename_folder(vfs, name, trash))
			result = XWA_MOD_UPDATE_FAILED;
		else if (!rename_folder(vfs, staging, name)) {
			/* The old mod goes back, and the update stays staged. */
			rename_folder(vfs, trash, name);
			result = XWA_MOD_UPDATE_FAILED;
		} else {
			result = XWA_MOD_UPDATE_APPLIED;
		}
	}
	remove_if_empty(vfs, STAGING_DIR);
	AeronVfs_Destroy(vfs);
	if (result == XWA_MOD_UPDATE_APPLIED)
		Aeron_LogInfo("xwa.mods", "the new import of %s replaced the old one", mod);
	return result;
}

/* ---- Deleting at the next start ----------------------------------------- */

bool XwaModUpdate_MarkDelete(const char* mod, bool marked) {
	AeronVfs* vfs = mod && mod[0] ? open_vfs(mod) : NULL;
	if (!vfs)
		return false;
	const bool ok =
		marked ? AeronVfs_WriteAllAtomic(vfs, AERON_VFS_ROOT_ASSET, DELETE_MARKER, "", 0) != 0
			   : !exists(vfs, DELETE_MARKER) || AeronVfs_Remove(vfs, AERON_VFS_ROOT_ASSET, DELETE_MARKER);
	AeronVfs_Destroy(vfs);
	return ok;
}

bool XwaModUpdate_IsMarkedDelete(const char* mod) {
	AeronVfs* vfs = mod && mod[0] ? open_vfs(mod) : NULL;
	const bool marked = vfs && exists(vfs, DELETE_MARKER);
	AeronVfs_Destroy(vfs);
	return marked;
}

/* Moves mod folder `mod` to its trash folder, with its staging folder, if
   it is marked for deletion and the importer made it. */
static void delete_if_marked(const char* mod) {
	char parent[MOD_UPDATE_PATH_CAP], name[MOD_UPDATE_PATH_CAP], staging[MOD_UPDATE_PATH_CAP];
	if (!XwaModUpdate_IsMarkedDelete(mod) || !is_imported(mod) ||
		!split_path(mod, parent, sizeof parent, name, sizeof name) ||
		snprintf(staging, sizeof staging, "%s/%s", STAGING_DIR, name) <= 0)
		return;
	AeronVfs* vfs = open_vfs(parent);
	if (!vfs)
		return;
	if (move_to_trash(vfs, name, name)) {
		Aeron_LogInfo("xwa.mods", "deleting %s, as it was marked", mod);
		if (exists(vfs, staging))
			move_to_trash(vfs, staging, name);
		remove_if_empty(vfs, STAGING_DIR);
	}
	AeronVfs_Destroy(vfs);
}

static int collect_folder(void* user, const AeronVfsEntry* entry) {
	/* The staging and trash folders, and other hidden folders, aren't mods. */
	return !entry->is_directory || entry->name[0] == '.' || collect_name(user, entry);
}

void XwaModUpdate_DeletePending(const char* mods_folder, const char* configured_mod) {
	char path[MOD_UPDATE_PATH_CAP];
	if (mods_folder && mods_folder[0]) {
		AeronVfs* vfs = open_vfs(mods_folder);
		NameList list = { 0 };
		if (vfs && AeronVfs_Glob(vfs, AERON_VFS_ROOT_ASSET, "", "*", AERON_VFS_GLOB_DIRECTORIES,
								 collect_folder, &list)) {
			for (size_t i = 0; i < list.count; ++i)
				if (join_path(path, sizeof path, mods_folder, list.names[i]))
					delete_if_marked(path);
		}
		free_names(&list);
		AeronVfs_Destroy(vfs);
	}
	if (configured_mod && configured_mod[0])
		delete_if_marked(configured_mod);
}

/* ---- Updating at startup ------------------------------------------------ */

void XwaModUpdate_ApplyPending(const char* mods_folder, const char* configured_mod) {
	char path[MOD_UPDATE_PATH_CAP];
	if (mods_folder && mods_folder[0]) {
		/* Each staging folder has the same name as the mod it updates. Going
		   through the staging folders also finds a mod whose folder is in the
		   trash because a swap stopped halfway. */
		AeronVfs* vfs = open_vfs(mods_folder);
		NameList list = { 0 };
		if (vfs && exists(vfs, STAGING_DIR) &&
			AeronVfs_Glob(vfs, AERON_VFS_ROOT_ASSET, STAGING_DIR, "*", AERON_VFS_GLOB_DIRECTORIES,
						  collect_folder, &list)) {
			for (size_t i = 0; i < list.count; ++i)
				if (join_path(path, sizeof path, mods_folder, list.names[i]))
					XwaModUpdate_Apply(path);
		}
		free_names(&list);
		AeronVfs_Destroy(vfs);
	}
	/* paths.mod can be outside the mods folder. */
	if (configured_mod && configured_mod[0])
		XwaModUpdate_Apply(configured_mod);
}

/* ---- The cleanup -------------------------------------------------------- */

static struct {
	AeronThread* thread;
	volatile int stop;
	char trash[2][MOD_UPDATE_PATH_CAP];
	int trash_count;
} g_cleanup;

/* Deletes what trash folder `trash` holds, then the trash folder when it is
   empty. */
static void clean_trash(const char* trash) {
	char path[MOD_UPDATE_PATH_CAP];
	AeronVfs* vfs = XwaModList_IsFolder(trash) ? open_vfs(trash) : NULL;
	NameList list = { 0 };
	if (vfs &&
		AeronVfs_Glob(vfs, AERON_VFS_ROOT_ASSET, "", "*", AERON_VFS_GLOB_DIRECTORIES, collect_name, &list)) {
		for (size_t i = 0; i < list.count && !Aeron_AtomicLoad(&g_cleanup.stop); ++i)
			if (join_path(path, sizeof path, trash, list.names[i]) &&
				!XwaModUpdate_DeleteFolder(path, &g_cleanup.stop) && !Aeron_AtomicLoad(&g_cleanup.stop))
				Aeron_LogWarn("xwa.mods", "can't delete %s", path);
	}
	free_names(&list);
	AeronVfs_Destroy(vfs);
	if (!Aeron_AtomicLoad(&g_cleanup.stop) && XwaModList_IsFolder(trash)) {
		char parent[MOD_UPDATE_PATH_CAP], name[MOD_UPDATE_PATH_CAP];
		AeronVfs* parent_vfs =
			split_path(trash, parent, sizeof parent, name, sizeof name) ? open_vfs(parent) : NULL;
		if (parent_vfs)
			remove_if_empty(parent_vfs, name);
		AeronVfs_Destroy(parent_vfs);
	}
}

static int cleanup_thread(void* user) {
	(void)user;
	for (int i = 0; i < g_cleanup.trash_count; ++i)
		clean_trash(g_cleanup.trash[i]);
	return 0;
}

/* Adds the trash folder in folder `path` when `in_folder` is set, and else
   the one beside mod folder `path`. */
static void add_trash(const char* path, bool in_folder) {
	char parent[MOD_UPDATE_PATH_CAP], name[MOD_UPDATE_PATH_CAP], trash[MOD_UPDATE_PATH_CAP];
	if (!path || !path[0] || g_cleanup.trash_count >= 2)
		return;
	if (in_folder) {
		if (!join_path(trash, sizeof trash, path, TRASH_DIR))
			return;
	} else if (!split_path(path, parent, sizeof parent, name, sizeof name) ||
			   !join_path(trash, sizeof trash, parent, TRASH_DIR)) {
		return;
	}
	for (int i = 0; i < g_cleanup.trash_count; ++i)
		if (XwaModList_SamePath(g_cleanup.trash[i], trash))
			return;
	snprintf(g_cleanup.trash[g_cleanup.trash_count++], MOD_UPDATE_PATH_CAP, "%s", trash);
}

void XwaModUpdate_StartCleanup(const char* mods_folder, const char* configured_mod) {
	if (g_cleanup.thread)
		return;
	g_cleanup.trash_count = 0;
	add_trash(mods_folder, true);
	add_trash(configured_mod, false);
	bool any = false;
	for (int i = 0; i < g_cleanup.trash_count; ++i)
		any = any || XwaModList_IsFolder(g_cleanup.trash[i]);
	if (!any)
		return;
	Aeron_AtomicStore(&g_cleanup.stop, 0);
	g_cleanup.thread = Aeron_ThreadCreate("mod cleanup", cleanup_thread, NULL);
}

void XwaModUpdate_StopCleanup(void) {
	if (!g_cleanup.thread)
		return;
	Aeron_AtomicStore(&g_cleanup.stop, 1);
	Aeron_ThreadJoin(g_cleanup.thread);
	g_cleanup.thread = NULL;
}
