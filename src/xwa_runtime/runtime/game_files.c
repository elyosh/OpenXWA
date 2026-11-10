#include "xwa_runtime/runtime/game_files.h"

#include "aeron/log.h"
#include "xwa_runtime/models/mod_files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define GAME_FILES_FOLDER "game/"
#define GAME_FILES_SETTINGS GAME_FILES_FOLDER "files.yaml"

/* The game files that the mod removes, in lower case with '/' separators,
   sorted. */
static char** s_removedFiles;
static size_t s_removedFileCount;

static char GameFiles_Lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

static int GameFiles_EndsWith(const char* text, const char* suffix) {
	size_t length = strlen(text);
	size_t suffixLength = strlen(suffix);

	return length >= suffixLength && strcmp(text + length - suffixLength, suffix) == 0;
}

/* Whether `path`, in lower case with '/' separators, is one of the files that
   the mod folder can replace. */
static int GameFiles_IsModFile(const char* path) {
	static const char* const missionFolders[] = { "missions/", "melee/", "combat/" };
	size_t i;

	for (i = 0; i < sizeof missionFolders / sizeof missionFolders[0]; ++i) {
		size_t length = strlen(missionFolders[i]);
		const char* name = path + length;

		if (strncmp(path, missionFolders[i], length) == 0 && strchr(name, '/') == NULL &&
			(GameFiles_EndsWith(name, ".tie") || strcmp(name, "mission.lst") == 0)) {
			return 1;
		}
	}
	if (strcmp(path, "missions/email.txt") == 0 || strcmp(path, "frontres/cutscene/cutscene.txt") == 0 ||
		strcmp(path, "frontres/medals/medals.txt") == 0 || strcmp(path, "frontres/medals/medals.lst") == 0 ||
		strcmp(path, "frontres/medals/familyawards.txt") == 0) {
		return 1;
	}
	if (strncmp(path, "music/", 6) == 0 && strchr(path + 6, '/') == NULL && GameFiles_EndsWith(path, ".imc")) {
		return 1;
	}
	return strncmp(path, "wave/", 5) == 0;
}

/* Copies `path` into `lower` in lower case, with '/' separators and without a
   leading '/' or "./". Returns 0 when it doesn't fit. */
static int GameFiles_Normalize(const char* path, char* lower, size_t lowerSize) {
	size_t length;
	size_t i;

	while (*path == '/' || *path == '\\' || (path[0] == '.' && (path[1] == '/' || path[1] == '\\'))) {
		path += *path == '.' ? 2 : 1;
	}
	length = strlen(path);
	if (length >= lowerSize) {
		return 0;
	}
	for (i = 0; i <= length; ++i) {
		lower[i] = path[i] == '\\' ? '/' : GameFiles_Lower(path[i]);
	}
	return 1;
}

static int GameFiles_Compare(const void* a, const void* b) {
	return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static void GameFiles_FreeRemovedFiles(void) {
	size_t i;

	for (i = 0; i < s_removedFileCount; ++i) {
		free(s_removedFiles[i]);
	}
	free(s_removedFiles);
	s_removedFiles = NULL;
	s_removedFileCount = 0;
}

/* Reads the `removed` list of game/files.yaml. */
static void GameFiles_ReadRemovedFiles(const AeronConfigNode* removed) {
	size_t count = AeronConfigNode_SequenceCount(removed);
	size_t i;

	s_removedFiles = (char**)calloc(count > 0 ? count : 1, sizeof *s_removedFiles);
	if (s_removedFiles == NULL) {
		Aeron_LogError("xwa.file", "%s: out of memory. The removed list is ignored", GAME_FILES_SETTINGS);
		return;
	}
	for (i = 0; i < count; ++i) {
		const AeronConfigNode* entry = AeronConfigNode_SequenceGet(removed, i);
		const char* path = AeronConfigNode_String(entry, NULL);
		char lower[XWA_GAME_FILES_PATH_SIZE];

		if (path == NULL) {
			Aeron_LogWarn("xwa.file", "%s:%d: each entry of the removed list must be a path. It is ignored",
						  GAME_FILES_SETTINGS, AeronConfigNode_Line(entry));
			continue;
		}
		if (!GameFiles_Normalize(path, lower, sizeof lower) || !GameFiles_IsModFile(lower)) {
			Aeron_LogWarn("xwa.file",
						  "%s:%d: %s can't be removed, because a mod can't replace it. It is ignored",
						  GAME_FILES_SETTINGS, AeronConfigNode_Line(entry), path);
			continue;
		}
		s_removedFiles[s_removedFileCount] = (char*)malloc(strlen(lower) + 1);
		if (s_removedFiles[s_removedFileCount] == NULL) {
			Aeron_LogError("xwa.file", "%s: out of memory. The rest of the removed list is ignored", GAME_FILES_SETTINGS);
			break;
		}
		strcpy(s_removedFiles[s_removedFileCount++], lower);
	}
	qsort(s_removedFiles, s_removedFileCount, sizeof *s_removedFiles, GameFiles_Compare);
}

void XwaGameFiles_Load(void) {
	AeronConfigFile* file = NULL;
	const AeronConfigNode* root;
	size_t i;

	GameFiles_FreeRemovedFiles();
	/* An empty file sets nothing. */
	if (XwaModFiles_Load(GAME_FILES_SETTINGS, 1, &file) != XWA_MOD_FILE_LOADED) {
		return;
	}
	root = AeronConfigFile_Root(file);
	for (i = 0; i < AeronConfigNode_MapCount(root); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(root, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(root, i);

		if (strcmp(key, "removed") != 0) {
			Aeron_LogWarn("xwa.file", "%s:%d: %s isn't a setting. It is ignored", GAME_FILES_SETTINGS,
						  AeronConfigNode_Line(value), key);
		} else if (AeronConfigNode_Type(value) != AERON_CONFIG_SEQUENCE) {
			Aeron_LogError("xwa.file", "%s:%d: removed must be a list of paths. It is ignored",
						   GAME_FILES_SETTINGS, AeronConfigNode_Line(value));
		} else {
			GameFiles_ReadRemovedFiles(value);
		}
	}
	AeronConfigFile_Destroy(file);
	Aeron_LogInfo("xwa.file", "%s: %zu removed game files", GAME_FILES_SETTINGS, s_removedFileCount);
}

XwaGameFileSource XwaGameFiles_Find(AeronVfs* vfs, const char* path, char* modPath, size_t modPathSize) {
	char lower[XWA_GAME_FILES_PATH_SIZE];
	const char* key = lower;

	if (vfs == NULL || path == NULL || !GameFiles_Normalize(path, lower, sizeof lower) ||
		!GameFiles_IsModFile(lower)) {
		return XWA_GAME_FILE_ORIGINAL;
	}
	if (s_removedFileCount > 0 &&
		bsearch(&key, s_removedFiles, s_removedFileCount, sizeof *s_removedFiles, GameFiles_Compare) != NULL) {
		Aeron_LogDebug("xwa.file", "%s: removed by the mod", path);
		return XWA_GAME_FILE_REMOVED;
	}
	if (snprintf(modPath, modPathSize, GAME_FILES_FOLDER "%s", lower) < (int)modPathSize &&
		AeronVfs_Exists(vfs, AERON_VFS_ROOT_MOD, modPath)) {
		Aeron_LogDebug("xwa.file", "%s: from the mod folder", path);
		return XWA_GAME_FILE_MOD;
	}
	return XWA_GAME_FILE_ORIGINAL;
}
