#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_filesystem.h>

#include "import_internal.h"

bool ImportAuthoring_Ignored(const char* name) {
	const size_t length = strlen(name);
	if (strcmp(name, ".DS_Store") == 0 || strcmp(name, "Thumbs.db") == 0 ||
		strcmp(name, "desktop.ini") == 0 || strncmp(name, "._", 2) == 0 || strncmp(name, ".#", 2) == 0)
		return true;
	/* Vim's .<name>.swp to .<name>.swa, Emacs' #<name># and <name>~. */
	if (name[0] == '.' && length > 5 && name[length - 4] == '.' && strncmp(name + length - 3, "sw", 2) == 0 &&
		name[length - 1] >= 'a' && name[length - 1] <= 'p')
		return true;
	return (name[0] == '#' && length > 1 && name[length - 1] == '#') || (length && name[length - 1] == '~');
}

/* Adds the files of `path` to `files`, named by their path under the entry,
 * `rel`. */
static bool add_files(cJSON* files, const char* path, const char* rel) {
	SDL_PathInfo info;
	if (!SDL_GetPathInfo(path, &info))
		return false;
	if (info.type == SDL_PATHTYPE_FILE) {
		cJSON* entry = cJSON_CreateArray();
		cJSON_AddItemToArray(entry, cJSON_CreateNumber((double)info.size));
		/* In microseconds, which a double holds exactly. */
		cJSON_AddItemToArray(entry, cJSON_CreateNumber((double)(info.modify_time / 1000)));
		cJSON_AddItemToObject(files, rel, entry);
		return true;
	}
	size_t count = 0;
	char** names = ListDir(path, true, &count);
	bool ok = names || !count;
	for (size_t i = 0; ok && i < count; ++i) {
		char child[IMPORT_PATH_CAP], child_rel[IMPORT_PATH_CAP];
		if (ImportAuthoring_Ignored(names[i]))
			continue;
		ok = PathFormat(child, sizeof child, "%s/%s", path, names[i]) &&
			 PathFormat(child_rel, sizeof child_rel, "%s%s%s", rel, *rel ? "/" : "", names[i]) &&
			 add_files(files, child, child_rel);
	}
	FreeList(names, count);
	return ok;
}

/* The files of the file or folder `path`, or NULL when it can't be read. A
 * file is listed under its name. */
static cJSON* list_files(const char* path) {
	SDL_PathInfo info;
	if (!SDL_GetPathInfo(path, &info))
		return NULL;
	const char* name = strrchr(path, '/');
	cJSON* files = cJSON_CreateObject();
	if (files && !add_files(files, path, info.type == SDL_PATHTYPE_FILE ? (name ? name + 1 : path) : "")) {
		cJSON_Delete(files);
		files = NULL;
	}
	return files;
}

bool ImportAuthoring_Load(ImportAuthoring* authoring, const char* folder) {
	char path[IMPORT_PATH_CAP];
	memset(authoring, 0, sizeof *authoring);
	if (!PathFormat(authoring->folder, sizeof authoring->folder, "%s", folder) ||
		!PathFormat(path, sizeof path, "%s/import.json", folder))
		return false;
	size_t size = 0;
	char* text = ReadWholeFile(path, &size);
	cJSON* root = text ? cJSON_Parse(text) : NULL;
	free(text);
	if (text && !root)
		ToolLog_Write(TOOL_LOG_WARNING, "%s isn't valid JSON. Every entry is kept as the user's own", path);
	cJSON* entries = cJSON_DetachItemFromObjectCaseSensitive(root, "entries");
	cJSON_Delete(root);
	if (!cJSON_IsObject(entries)) {
		cJSON_Delete(entries);
		entries = cJSON_CreateObject();
	}
	authoring->entries = entries;
	return entries != NULL;
}

void ImportAuthoring_Free(ImportAuthoring* authoring) {
	cJSON_Delete(authoring->entries);
	authoring->entries = NULL;
}

ImportAuthoringState ImportAuthoring_State(const ImportAuthoring* authoring, const char* rel) {
	char path[IMPORT_PATH_CAP];
	if (!PathFormat(path, sizeof path, "%s/%s", authoring->folder, rel) || !SDL_GetPathInfo(path, NULL))
		return IMPORT_AUTHORING_MISSING;
	const cJSON* recorded = cJSON_GetObjectItemCaseSensitive(authoring->entries, rel);
	if (!recorded)
		return IMPORT_AUTHORING_AUTHORED;
	cJSON* files = list_files(path);
	const bool same = files && cJSON_Compare(files, recorded, true);
	cJSON_Delete(files);
	return same ? IMPORT_AUTHORING_IMPORTED : IMPORT_AUTHORING_EDITED;
}

cJSON* ImportAuthoring_Place(const ImportAuthoring* authoring, const char* staged, const char* rel) {
	char path[IMPORT_PATH_CAP], parent[IMPORT_PATH_CAP];
	if (!PathFormat(path, sizeof path, "%s/%s", authoring->folder, rel) ||
		!PathFormat(parent, sizeof parent, "%s", path))
		return NULL;
	*strrchr(parent, '/') = '\0';
	if (!RemoveTree(path) || !MakeDirs(parent) || !Import_Rename(staged, path)) {
		ToolLog_Write(TOOL_LOG_ERROR, "Can't place %s in %s", rel, authoring->folder);
		return NULL;
	}
	cJSON* files = list_files(path);
	if (!files)
		ToolLog_Write(TOOL_LOG_ERROR, "Can't read %s", path);
	return files;
}

void ImportAuthoring_Set(ImportAuthoring* authoring, const char* rel, cJSON* files) {
	if (!files)
		return;
	if (cJSON_GetObjectItemCaseSensitive(authoring->entries, rel))
		cJSON_ReplaceItemInObjectCaseSensitive(authoring->entries, rel, files);
	else
		cJSON_AddItemToObject(authoring->entries, rel, files);
}

bool ImportAuthoring_Save(const ImportAuthoring* authoring) {
	char path[IMPORT_PATH_CAP], temporary[IMPORT_PATH_CAP];
	cJSON* root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "version", 1);
	cJSON_AddItemReferenceToObject(root, "entries", authoring->entries);
	char* text = cJSON_Print(root);
	cJSON_Delete(root);
	bool ok = text && PathFormat(path, sizeof path, "%s/import.json", authoring->folder) &&
			  PathFormat(temporary, sizeof temporary, "%s/.import.json.tmp", authoring->folder) &&
			  MakeDirs(authoring->folder) && WriteWholeFile(temporary, text, strlen(text));
#ifdef _WIN32
	/* Windows' rename doesn't replace an existing file. */
	if (ok)
		remove(path);
#endif
	ok = ok && rename(temporary, path) == 0;
	free(text);
	if (!ok)
		ToolLog_Write(TOOL_LOG_ERROR, "Can't write %s/import.json", authoring->folder);
	return ok;
}
