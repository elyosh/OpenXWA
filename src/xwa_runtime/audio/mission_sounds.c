#include "xwa_runtime/audio/mission_sounds.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "aeron/vfs.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/runtime/game_files.h"

#include <stddef.h>
#include <string.h>

/* Modern port module; no original counterpart. */

static AeronConfigFile* s_file; /* the mission file, which holds the paths */
static const AeronConfigNode* s_sounds;

static char MissionSounds_Lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

/* Whether `a` and `b` are the same, ignoring the case of ASCII letters. */
static int MissionSounds_SameText(const char* a, const char* b) {
	for (; *a && *b; ++a, ++b) {
		if (MissionSounds_Lower(*a) != MissionSounds_Lower(*b)) {
			return 0;
		}
	}
	return *a == *b;
}

void XwaMissionSounds_LoadMission(const char* missionPath) {
	char path[300];
	size_t i;

	AeronConfigFile_Destroy(s_file);
	s_file = NULL;
	s_sounds = NULL;
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &s_file, path, sizeof path) <= 0) {
		s_file = NULL;
		return;
	}
	s_sounds = AeronConfigNode_MapGet(AeronConfigFile_Root(s_file), "sounds");
	if (s_sounds == NULL) {
		return;
	}
	if (AeronConfigNode_Type(s_sounds) != AERON_CONFIG_SEQUENCE) {
		Aeron_LogError("xwa.mission", "%s:%d: sounds must be a list of [from, to] pairs. It is ignored", path,
					   AeronConfigNode_Line(s_sounds));
		s_sounds = NULL;
		return;
	}
	for (i = 0; i < AeronConfigNode_SequenceCount(s_sounds); ++i) {
		const AeronConfigNode* pair = AeronConfigNode_SequenceGet(s_sounds, i);

		if (AeronConfigNode_Type(pair) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(pair) != 2 ||
			AeronConfigNode_Type(AeronConfigNode_SequenceGet(pair, 0)) != AERON_CONFIG_STRING ||
			AeronConfigNode_Type(AeronConfigNode_SequenceGet(pair, 1)) != AERON_CONFIG_STRING) {
			Aeron_LogWarn("xwa.mission", "%s:%d: each entry of sounds must be a pair of paths. It is ignored",
						  path, AeronConfigNode_Line(pair));
		}
	}
	Aeron_LogInfo("xwa.mission", "%s: %d sound replacements", path,
				  (int)AeronConfigNode_SequenceCount(s_sounds));
}

/* Whether the game can open sound file `fileName`, from the mod folder or
   from the game folder. */
static int MissionSounds_Exists(const char* fileName) {
	AeronVfs* vfs = Aeron_GetVfs();
	char modPath[XWA_GAME_FILES_PATH_SIZE];

	return XwaGameFiles_Find(vfs, fileName, modPath, sizeof modPath) ||
		   AeronVfs_Exists(vfs, AERON_VFS_ROOT_ASSET, fileName);
}

const char* XwaMissionSounds_Replace(const char* fileName) {
	size_t i;

	for (i = 0; i < AeronConfigNode_SequenceCount(s_sounds); ++i) {
		const AeronConfigNode* pair = AeronConfigNode_SequenceGet(s_sounds, i);
		const char* from = AeronConfigNode_String(AeronConfigNode_SequenceGet(pair, 0), NULL);
		const char* to = AeronConfigNode_String(AeronConfigNode_SequenceGet(pair, 1), NULL);

		if (AeronConfigNode_SequenceCount(pair) != 2 || from == NULL || to == NULL ||
			!MissionSounds_SameText(from, fileName)) {
			continue;
		}
		/* Only the first entry for a path counts. */
		if (MissionSounds_Exists(to)) {
			Aeron_LogInfo("xwa.audio", "%s: the mission loads %s instead", fileName, to);
			return to;
		}
		Aeron_LogWarn("xwa.mission", "sounds: %s doesn't exist, so %s keeps its own sound", to, fileName);
		return fileName;
	}
	return fileName;
}
