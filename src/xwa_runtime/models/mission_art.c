#include "xwa_runtime/models/mission_art.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "aeron/vfs.h"
#include "xwa/assets/sprite_resource.h"
#include "xwa_runtime/models/mission_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define MISSION_ART_MAX 8
#define MISSION_ART_GROUPS_MAX XWA_MISSION_ART_GROUPS_MAX
#define MISSION_ART_FOLDER "resdata/mission_art"

typedef struct MissionArtGroup {
	int group;
	int art; /* index in MissionArtList.names */
} MissionArtGroup;

/* A mission's art: the names of its art folders, in its order, and the
   group that each one gives. */
typedef struct MissionArtList {
	char names[MISSION_ART_MAX][XWA_MISSION_ART_NAME_MAX];
	int nameCount;
	MissionArtGroup groups[MISSION_ART_GROUPS_MAX];
	int groupCount;
} MissionArtList;

static MissionArtList s_active;
static uint64_t s_generation;

/* An art name is a folder name, so it may only hold lower-case letters,
   digits, '_' and '-'. */
static int MissionArt_ValidName(const char* name) {
	const char* c;

	if (name == NULL || name[0] == '\0' || strlen(name) >= XWA_MISSION_ART_NAME_MAX) {
		return 0;
	}
	for (c = name; *c != '\0'; ++c) {
		if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) {
			return 0;
		}
	}
	return 1;
}

static const char* MissionArt_Find(const MissionArtList* list, int group) {
	int i;

	for (i = 0; i < list->groupCount; ++i) {
		if (list->groups[i].group == group) {
			return list->names[list->groups[i].art];
		}
	}
	return NULL;
}

typedef struct MissionArtGlob {
	MissionArtList* list;
	int art;
	int found;
} MissionArtGlob;

/* Adds group_<g>.yaml of an art folder, unless an earlier folder has the
   group. */
static int MissionArt_AddFile(void* userdata, const AeronVfsEntry* entry) {
	MissionArtGlob* glob = (MissionArtGlob*)userdata;
	char* end = NULL;
	long group;

	if (strncmp(entry->name, "group_", 6) != 0) {
		return 1;
	}
	group = strtol(entry->name + 6, &end, 10);
	if (end == entry->name + 6 || strcmp(end, ".yaml") != 0 || group < 0 || group > 0xFFFF) {
		return 1;
	}
	glob->found = 1;
	if (MissionArt_Find(glob->list, (int)group) != NULL) {
		return 1;
	}
	if (glob->list->groupCount == MISSION_ART_GROUPS_MAX) {
		Aeron_LogError("xwa.mission",
					   "%s/%s: more than %d groups in the mission's art; group %ld is left out",
					   MISSION_ART_FOLDER, glob->list->names[glob->art], MISSION_ART_GROUPS_MAX, group);
		return 1;
	}
	glob->list->groups[glob->list->groupCount].group = (int)group;
	glob->list->groups[glob->list->groupCount].art = glob->art;
	++glob->list->groupCount;
	return 1;
}

/* Reads the art that mission `missionPath` lists. */
static void MissionArt_Read(const char* missionPath, MissionArtList* list) {
	AeronConfigFile* config = NULL;
	char path[512];
	const AeronConfigNode* names;
	size_t i;

	memset(list, 0, sizeof *list);
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	names = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "resdata");
	if (names != NULL && AeronConfigNode_Type(names) != AERON_CONFIG_SEQUENCE) {
		Aeron_LogError("xwa.mission", "%s:%d: resdata must be a list of file names; the mission adds no art",
					   path, AeronConfigNode_Line(names));
		names = NULL;
	}
	for (i = 0; i < AeronConfigNode_SequenceCount(names); ++i) {
		const AeronConfigNode* node = AeronConfigNode_SequenceGet(names, i);
		const char* name = AeronConfigNode_String(node, NULL);
		char folder[64];
		MissionArtGlob glob;

		if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING || !MissionArt_ValidName(name)) {
			Aeron_LogError(
				"xwa.mission",
				"%s:%d: a resdata name must have only lower-case letters, digits, _ and -, and fewer "
				"than %d characters; '%s' is ignored",
				path, AeronConfigNode_Line(node), XWA_MISSION_ART_NAME_MAX, name != NULL ? name : "");
			continue;
		}
		if (list->nameCount == MISSION_ART_MAX) {
			Aeron_LogError("xwa.mission", "%s: more than %d art folders; %s and the next ones are ignored",
						   path, MISSION_ART_MAX, name);
			break;
		}
		snprintf(list->names[list->nameCount], sizeof list->names[0], "%s", name);
		snprintf(folder, sizeof folder, MISSION_ART_FOLDER "/%s", name);
		glob.list = list;
		glob.art = list->nameCount;
		glob.found = 0;
		/* A file without a folder is already in the shared atlases. */
		(void)AeronVfs_Glob(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, folder, "group_*.yaml", AERON_VFS_GLOB_FILES,
							MissionArt_AddFile, &glob);
		if (glob.found) {
			++list->nameCount;
		}
	}
	AeronConfigFile_Destroy(config);
}

static int MissionArt_Same(const MissionArtList* a, const MissionArtList* b) {
	int i;

	if (a->groupCount != b->groupCount) {
		return 0;
	}
	for (i = 0; i < a->groupCount; ++i) {
		const char* other = MissionArt_Find(b, a->groups[i].group);

		if (other == NULL || strcmp(other, a->names[a->groups[i].art]) != 0) {
			return 0;
		}
	}
	return 1;
}

/* Unloads each group of `from` whose source isn't the same in `to`. */
static void MissionArt_Unload(const MissionArtList* from, const MissionArtList* to) {
	int i;

	for (i = 0; i < from->groupCount; ++i) {
		const char* other = MissionArt_Find(to, from->groups[i].group);

		if (other == NULL || strcmp(other, from->names[from->groups[i].art]) != 0) {
			SpriteResource_UnloadGroup((int16_t)from->groups[i].group);
		}
	}
}

void XwaMissionArt_LoadMission(const char* missionPath) {
	MissionArtList list;
	int i;

	MissionArt_Read(missionPath, &list);
	if (MissionArt_Same(&list, &s_active)) {
		return;
	}
	MissionArt_Unload(&s_active, &list);
	MissionArt_Unload(&list, &s_active);
	s_active = list;
	++s_generation;
	for (i = 0; i < s_active.nameCount; ++i) {
		Aeron_LogInfo("xwa.mission", "%s: the mission draws art from %s/%s", missionPath, MISSION_ART_FOLDER,
					  s_active.names[i]);
	}
}

const char* XwaMissionArt_GroupSource(int group) { return MissionArt_Find(&s_active, group); }

int XwaMissionArt_Count(void) { return s_active.groupCount; }

int XwaMissionArt_At(int index, int* group, const char** art) {
	if (index < 0 || index >= s_active.groupCount) {
		return 0;
	}
	*group = s_active.groups[index].group;
	*art = s_active.names[s_active.groups[index].art];
	return 1;
}

uint64_t XwaMissionArt_Generation(void) { return s_generation; }
