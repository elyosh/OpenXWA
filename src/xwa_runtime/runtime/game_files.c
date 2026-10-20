#include "xwa_runtime/runtime/game_files.h"

#include "aeron/log.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define GAME_FILES_FOLDER "game/"

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
	static const char* const voiceFolders[] = { "wave/missionvoice/", "wave/frontend/" };
	size_t i;

	for (i = 0; i < sizeof missionFolders / sizeof missionFolders[0]; ++i) {
		size_t length = strlen(missionFolders[i]);
		const char* name = path + length;

		if (strncmp(path, missionFolders[i], length) == 0 && strchr(name, '/') == NULL &&
			(GameFiles_EndsWith(name, ".tie") || strcmp(name, "mission.lst") == 0)) {
			return 1;
		}
	}
	if (strcmp(path, "missions/email.txt") == 0 || strcmp(path, "frontres/cutscene/cutscene.txt") == 0) {
		return 1;
	}
	for (i = 0; i < sizeof voiceFolders / sizeof voiceFolders[0]; ++i) {
		if (strncmp(path, voiceFolders[i], strlen(voiceFolders[i])) == 0) {
			return 1;
		}
	}
	return 0;
}

int XwaGameFiles_Find(AeronVfs* vfs, const char* path, char* modPath, size_t modPathSize) {
	char lower[XWA_GAME_FILES_PATH_SIZE];
	size_t length;
	size_t i;

	if (vfs == NULL || path == NULL) {
		return 0;
	}
	while (*path == '/' || *path == '\\' || (path[0] == '.' && (path[1] == '/' || path[1] == '\\'))) {
		path += *path == '.' ? 2 : 1;
	}
	length = strlen(path);
	if (length >= sizeof lower) {
		return 0;
	}
	for (i = 0; i <= length; ++i) {
		lower[i] = path[i] == '\\' ? '/' : GameFiles_Lower(path[i]);
	}
	if (!GameFiles_IsModFile(lower)) {
		return 0;
	}
	if (snprintf(modPath, modPathSize, GAME_FILES_FOLDER "%s", lower) >= (int)modPathSize ||
		!AeronVfs_Exists(vfs, AERON_VFS_ROOT_MOD, modPath)) {
		return 0;
	}
	Aeron_LogDebug("xwa.file", "%s: from the mod folder", path);
	return 1;
}
