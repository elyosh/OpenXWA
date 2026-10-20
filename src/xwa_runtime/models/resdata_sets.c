#include "xwa_runtime/models/resdata_sets.h"

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

#define RESDATA_SETS_MAX 8
#define RESDATA_SETS_GROUPS_MAX XWA_RESDATA_SET_GROUPS_MAX
#define RESDATA_SETS_FOLDER "resdata/sets"

typedef struct ResdataGroup {
	int group;
	int set; /* index in ResdataSets.names */
} ResdataGroup;

typedef struct ResdataSets {
	char names[RESDATA_SETS_MAX][XWA_RESDATA_SET_NAME_MAX];
	int nameCount;
	ResdataGroup groups[RESDATA_SETS_GROUPS_MAX];
	int groupCount;
} ResdataSets;

static ResdataSets s_active;
static uint64_t s_generation;

/* A set name is a folder name, so it may only hold lower-case letters,
   digits, '_' and '-'. */
static int ResdataSets_ValidName(const char* name) {
	const char* c;

	if (name == NULL || name[0] == '\0' || strlen(name) >= XWA_RESDATA_SET_NAME_MAX) {
		return 0;
	}
	for (c = name; *c != '\0'; ++c) {
		if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) {
			return 0;
		}
	}
	return 1;
}

static const char* ResdataSets_Find(const ResdataSets* sets, int group) {
	int i;

	for (i = 0; i < sets->groupCount; ++i) {
		if (sets->groups[i].group == group) {
			return sets->names[sets->groups[i].set];
		}
	}
	return NULL;
}

typedef struct ResdataSetsGlob {
	ResdataSets* sets;
	int set;
	int found;
} ResdataSetsGlob;

/* Adds group_<g>.yaml of a set's folder, unless an earlier set has the
   group. */
static int ResdataSets_AddFile(void* userdata, const AeronVfsEntry* entry) {
	ResdataSetsGlob* glob = (ResdataSetsGlob*)userdata;
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
	if (ResdataSets_Find(glob->sets, (int)group) != NULL) {
		return 1;
	}
	if (glob->sets->groupCount == RESDATA_SETS_GROUPS_MAX) {
		Aeron_LogError("xwa.mission",
					   "%s/%s: more than %d groups in the mission's sets; group %ld is left out",
					   RESDATA_SETS_FOLDER, glob->sets->names[glob->set], RESDATA_SETS_GROUPS_MAX, group);
		return 1;
	}
	glob->sets->groups[glob->sets->groupCount].group = (int)group;
	glob->sets->groups[glob->sets->groupCount].set = glob->set;
	++glob->sets->groupCount;
	return 1;
}

/* Reads the sets that mission `missionPath` lists. */
static void ResdataSets_Read(const char* missionPath, ResdataSets* sets) {
	AeronConfigFile* config = NULL;
	char path[512];
	const AeronConfigNode* list;
	size_t i;

	memset(sets, 0, sizeof *sets);
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	list = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "resdata");
	if (list != NULL && AeronConfigNode_Type(list) != AERON_CONFIG_SEQUENCE) {
		Aeron_LogError("xwa.mission", "%s:%d: resdata must be a list of file names; the mission adds no art",
					   path, AeronConfigNode_Line(list));
		list = NULL;
	}
	for (i = 0; i < AeronConfigNode_SequenceCount(list); ++i) {
		const AeronConfigNode* node = AeronConfigNode_SequenceGet(list, i);
		const char* name = AeronConfigNode_String(node, NULL);
		char folder[64];
		ResdataSetsGlob glob;

		if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING || !ResdataSets_ValidName(name)) {
			Aeron_LogError(
				"xwa.mission",
				"%s:%d: a resdata name must have only lower-case letters, digits, _ and -, and fewer "
				"than %d characters; '%s' is ignored",
				path, AeronConfigNode_Line(node), XWA_RESDATA_SET_NAME_MAX, name != NULL ? name : "");
			continue;
		}
		if (sets->nameCount == RESDATA_SETS_MAX) {
			Aeron_LogError("xwa.mission", "%s: more than %d sets; %s and the next ones are ignored", path,
						   RESDATA_SETS_MAX, name);
			break;
		}
		snprintf(sets->names[sets->nameCount], sizeof sets->names[0], "%s", name);
		snprintf(folder, sizeof folder, RESDATA_SETS_FOLDER "/%s", name);
		glob.sets = sets;
		glob.set = sets->nameCount;
		glob.found = 0;
		/* A file without a folder was baked into the shared atlases. */
		(void)AeronVfs_Glob(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, folder, "group_*.yaml", AERON_VFS_GLOB_FILES,
							ResdataSets_AddFile, &glob);
		if (glob.found) {
			++sets->nameCount;
		}
	}
	AeronConfigFile_Destroy(config);
}

static int ResdataSets_Same(const ResdataSets* a, const ResdataSets* b) {
	int i;

	if (a->groupCount != b->groupCount) {
		return 0;
	}
	for (i = 0; i < a->groupCount; ++i) {
		const char* other = ResdataSets_Find(b, a->groups[i].group);

		if (other == NULL || strcmp(other, a->names[a->groups[i].set]) != 0) {
			return 0;
		}
	}
	return 1;
}

/* Unloads each group of `from` whose set isn't the same in `to`. */
static void ResdataSets_Unload(const ResdataSets* from, const ResdataSets* to) {
	int i;

	for (i = 0; i < from->groupCount; ++i) {
		const char* other = ResdataSets_Find(to, from->groups[i].group);

		if (other == NULL || strcmp(other, from->names[from->groups[i].set]) != 0) {
			SpriteResource_UnloadGroup((int16_t)from->groups[i].group);
		}
	}
}

void XwaResdataSets_LoadMission(const char* missionPath) {
	ResdataSets sets;
	int i;

	ResdataSets_Read(missionPath, &sets);
	if (ResdataSets_Same(&sets, &s_active)) {
		return;
	}
	ResdataSets_Unload(&s_active, &sets);
	ResdataSets_Unload(&sets, &s_active);
	s_active = sets;
	++s_generation;
	for (i = 0; i < s_active.nameCount; ++i) {
		Aeron_LogInfo("xwa.mission", "%s: the mission draws art from %s/%s", missionPath, RESDATA_SETS_FOLDER,
					  s_active.names[i]);
	}
}

const char* XwaResdataSets_GroupSet(int group) { return ResdataSets_Find(&s_active, group); }

int XwaResdataSets_Count(void) { return s_active.groupCount; }

int XwaResdataSets_At(int index, int* group, const char** set) {
	if (index < 0 || index >= s_active.groupCount) {
		return 0;
	}
	*group = s_active.groups[index].group;
	*set = s_active.names[s_active.groups[index].set];
	return 1;
}

uint64_t XwaResdataSets_Generation(void) { return s_generation; }
