#include "xwa_runtime/frontend/awards.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/frontend/frontend_medals.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa_runtime/text/text_overrides.h"

#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define XWA_AWARDS_PATH "frontend/awards.yaml"

/* The stock game's counts. */
#define AWARDS_STOCK_KALIDOR_LEVELS 6
#define AWARDS_STOCK_BATTLE_NAMES 7
#define AWARDS_STOCK_MEMENTO_NAMES (STR_FAMILY_MEDAL50 - STR_FAMILY_MEDAL0 + 1)
#define AWARDS_STOCK_MEMENTO_SPRITES 53

/* The largest counts. The Kalidor sprites use the atlas images 600 to 699,
   and the memento sprites are loaded for missions 0 to 255. */
#define AWARDS_MAX_KALIDOR_LEVELS 100
#define AWARDS_MAX_RANKS 8
#define AWARDS_MAX_RATINGS 25
#define AWARDS_MAX_MEMENTOS 256

/* The longest texts that the game's buffers hold: the title and the image
   path of the memento detail view. */
#define AWARDS_MAX_NAME 167
#define AWARDS_MAX_IMAGE 139

typedef struct XwaAwards {
	int battleCount;
	char* battleNames[FRONTEND_MEDAL_VALUE_COUNT];
	int kalidorCount;                                      /* 0 without a `kalidor` list */
	unsigned int kalidorPoints[AWARDS_MAX_KALIDOR_LEVELS]; /* 0 keeps the stock threshold */
	char* kalidorNames[AWARDS_MAX_KALIDOR_LEVELS];
	int rankCount;
	unsigned int ranks[AWARDS_MAX_RANKS];
	int ratingCount;
	int ratings[AWARDS_MAX_RATINGS];
	int hasMementoImages;
	char* mementoImages[AWARDS_MAX_MEMENTOS];
	char* mementoNames[AWARDS_MAX_MEMENTOS];
	int mementoSprites;
	int hasUnlockMission[XWA_AWARDS_UNLOCK_ITEMS];
	int unlockMission[XWA_AWARDS_UNLOCK_ITEMS];
} XwaAwards;

/* The `mementos` key of each XwaAwardsUnlockItem, and the full key path
   used in log lines. */
static const char* const k_unlockItems[XWA_AWARDS_UNLOCK_ITEMS] = { "lady_blue", "cologne" };
static const char* const k_unlockPaths[XWA_AWARDS_UNLOCK_ITEMS] = { "mementos.lady_blue",
																	"mementos.cologne" };

static XwaAwards g_awards;
static int g_awardsLoaded;

static void awards_free(XwaAwards* awards) {
	int i;

	for (i = 0; i < FRONTEND_MEDAL_VALUE_COUNT; i++) {
		free(awards->battleNames[i]);
	}
	for (i = 0; i < AWARDS_MAX_KALIDOR_LEVELS; i++) {
		free(awards->kalidorNames[i]);
	}
	for (i = 0; i < AWARDS_MAX_MEMENTOS; i++) {
		free(awards->mementoImages[i]);
		free(awards->mementoNames[i]);
	}
	memset(awards, 0, sizeof(*awards));
}

static int awards_error(const AeronConfigNode* node, const char* key, const char* message) {
	Aeron_LogError("xwa.frontend", "%s:%d: '%s' %s", XWA_AWARDS_PATH, AeronConfigNode_Line(node), key,
				   message);
	return 0;
}

/* Copies the text of `node` in Latin-1, the game's character set. */
static int awards_text(const AeronConfigNode* node, const char* key, size_t maxLength, char** out) {
	const char* text = AeronConfigNode_String(node, NULL);

	if (text == NULL) {
		return awards_error(node, key, "must be text in quotes");
	}
	*out = (char*)malloc(strlen(text) + 1);
	if (*out == NULL) {
		return awards_error(node, key, "can't be read: out of memory");
	}
	strcpy(*out, text);
	if (!XwaText_ToLatin1(*out)) {
		return awards_error(node, key, "has a character that isn't in Latin-1, so the game can't show it");
	}
	if (strlen(*out) > maxLength) {
		Aeron_LogError("xwa.frontend", "%s:%d: '%s' is longer than the game's %zu characters",
					   XWA_AWARDS_PATH, AeronConfigNode_Line(node), key, maxLength);
		return 0;
	}
	return 1;
}

static int awards_positive(const AeronConfigNode* node, const char* key, int64_t max, int64_t* out) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || AeronConfigNode_Int(node, 0) <= 0 ||
		AeronConfigNode_Int(node, 0) > max) {
		return awards_error(node, key, "must be a whole number above 0");
	}
	*out = AeronConfigNode_Int(node, 0);
	return 1;
}

static int awards_list(const AeronConfigNode* node, const char* key, int max) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE) {
		return awards_error(node, key, "must be a list");
	}
	if (AeronConfigNode_SequenceCount(node) > (size_t)max) {
		Aeron_LogError("xwa.frontend", "%s:%d: '%s' has more than %d entries", XWA_AWARDS_PATH,
					   AeronConfigNode_Line(node), key, max);
		return 0;
	}
	return 1;
}

/* Checks that the map `node` only has the keys in `keys`, a list that ends
   with NULL. `key` is the map's key, or NULL for the whole file. */
static int awards_keys(const AeronConfigNode* node, const char* key, const char* const* keys) {
	size_t i;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		if (key == NULL) {
			Aeron_LogError("xwa.frontend", "%s: must be a map", XWA_AWARDS_PATH);
			return 0;
		}
		return awards_error(node, key, "must be a map");
	}
	for (i = 0; i < AeronConfigNode_MapCount(node); i++) {
		const char* name = AeronConfigNode_MapKeyAt(node, i);
		size_t k = 0;

		while (keys[k] != NULL && strcmp(keys[k], name) != 0) {
			k++;
		}
		if (keys[k] == NULL) {
			Aeron_LogError("xwa.frontend", "%s:%d: unknown key '%s'%s%s%s", XWA_AWARDS_PATH,
						   AeronConfigNode_Line(AeronConfigNode_MapValueAt(node, i)), name,
						   key ? " in '" : "", key ? key : "", key ? "'" : "");
			return 0;
		}
	}
	return 1;
}

static int awards_read_battle_medals(const AeronConfigNode* list, XwaAwards* awards) {
	static const char* const keys[] = { "name", NULL };
	size_t i;

	if (!awards_list(list, "battle_medals", FRONTEND_MEDAL_VALUE_COUNT)) {
		return 0;
	}
	for (i = 0; i < AeronConfigNode_SequenceCount(list); i++) {
		const AeronConfigNode* entry = AeronConfigNode_SequenceGet(list, i);
		const AeronConfigNode* name = AeronConfigNode_MapGet(entry, "name");

		if (!awards_keys(entry, "battle_medals", keys) ||
			(name != NULL &&
			 !awards_text(name, "battle_medals.name", AWARDS_MAX_NAME, &awards->battleNames[i]))) {
			return 0;
		}
	}
	awards->battleCount = (int)AeronConfigNode_SequenceCount(list);
	return 1;
}

static int awards_read_kalidor(const AeronConfigNode* list, XwaAwards* awards) {
	static const char* const keys[] = { "points", "name", NULL };
	size_t i;

	if (!awards_list(list, "kalidor", AWARDS_MAX_KALIDOR_LEVELS)) {
		return 0;
	}
	for (i = 0; i < AeronConfigNode_SequenceCount(list); i++) {
		const AeronConfigNode* entry = AeronConfigNode_SequenceGet(list, i);
		const AeronConfigNode* points = AeronConfigNode_MapGet(entry, "points");
		const AeronConfigNode* name = AeronConfigNode_MapGet(entry, "name");
		int64_t value;

		if (!awards_keys(entry, "kalidor", keys)) {
			return 0;
		}
		if (points != NULL) {
			/* The game compares the bonus score in tenths of a point. */
			if (!awards_positive(points, "kalidor.points", 0xffffffffu / 10, &value)) {
				return 0;
			}
			awards->kalidorPoints[i] = (unsigned int)value;
		} else if (i >= AWARDS_STOCK_KALIDOR_LEVELS) {
			return awards_error(entry, "kalidor", "needs 'points' after the first 6 levels");
		}
		if (name != NULL && !awards_text(name, "kalidor.name", AWARDS_MAX_NAME, &awards->kalidorNames[i])) {
			return 0;
		}
	}
	awards->kalidorCount = (int)AeronConfigNode_SequenceCount(list);
	return 1;
}

static int awards_read_thresholds(const AeronConfigNode* list, const char* key, int max, int64_t* values,
								  int* count) {
	size_t i;

	if (!awards_list(list, key, max)) {
		return 0;
	}
	for (i = 0; i < AeronConfigNode_SequenceCount(list); i++) {
		if (!awards_positive(AeronConfigNode_SequenceGet(list, i), key, 0x7fffffff, &values[i])) {
			return 0;
		}
	}
	*count = (int)AeronConfigNode_SequenceCount(list);
	return 1;
}

/* Reads a map of missions to texts, such as `mementos.images`. */
static int awards_read_missions(const AeronConfigNode* map, const char* key, size_t maxLength, char** texts,
								int* sprites) {
	size_t i;

	if (AeronConfigNode_Type(map) != AERON_CONFIG_MAP) {
		return awards_error(map, key, "must be a map of missions to texts");
	}
	for (i = 0; i < AeronConfigNode_MapCount(map); i++) {
		const char* name = AeronConfigNode_MapKeyAt(map, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(map, i);
		char* end = NULL;
		long mission = strtol(name, &end, 10);

		if (end == name || *end != '\0' || mission < 0 || mission >= AWARDS_MAX_MEMENTOS) {
			Aeron_LogError("xwa.frontend", "%s:%d: '%s' has the mission '%s', which isn't from 0 to %d",
						   XWA_AWARDS_PATH, AeronConfigNode_Line(value), key, name, AWARDS_MAX_MEMENTOS - 1);
			return 0;
		}
		if (texts[mission] != NULL) {
			Aeron_LogError("xwa.frontend", "%s:%d: '%s' has mission %ld twice", XWA_AWARDS_PATH,
						   AeronConfigNode_Line(value), key, mission);
			return 0;
		}
		if (!awards_text(value, key, maxLength, &texts[mission])) {
			return 0;
		}
		if (mission + 1 > *sprites) {
			*sprites = (int)mission + 1;
		}
	}
	return 1;
}

static int awards_read(const AeronConfigNode* root, XwaAwards* awards) {
	static const char* const keys[] = { "battle_medals",     "kalidor",  "rank_thresholds",
										"rating_thresholds", "mementos", NULL };
	static const char* const mementoKeys[] = { "images", "names", "lady_blue", "cologne", NULL };
	static const char* const unlockKeys[] = { "mission", NULL };
	const AeronConfigNode* node;
	int64_t values[AWARDS_MAX_RATINGS];
	int i;

	if (!awards_keys(root, NULL, keys)) {
		return 0;
	}
	node = AeronConfigNode_MapGet(root, "battle_medals");
	if (node != NULL && !awards_read_battle_medals(node, awards)) {
		return 0;
	}
	node = AeronConfigNode_MapGet(root, "kalidor");
	if (node != NULL && !awards_read_kalidor(node, awards)) {
		return 0;
	}
	node = AeronConfigNode_MapGet(root, "rank_thresholds");
	if (node != NULL) {
		if (!awards_read_thresholds(node, "rank_thresholds", AWARDS_MAX_RANKS, values, &awards->rankCount)) {
			return 0;
		}
		for (i = 0; i < awards->rankCount; i++) {
			awards->ranks[i] = (unsigned int)values[i];
		}
	}
	node = AeronConfigNode_MapGet(root, "rating_thresholds");
	if (node != NULL) {
		if (!awards_read_thresholds(node, "rating_thresholds", AWARDS_MAX_RATINGS, values,
									&awards->ratingCount)) {
			return 0;
		}
		for (i = 0; i < awards->ratingCount; i++) {
			awards->ratings[i] = (int)values[i];
		}
	}
	awards->mementoSprites = AWARDS_STOCK_MEMENTO_SPRITES;
	node = AeronConfigNode_MapGet(root, "mementos");
	if (node != NULL) {
		const AeronConfigNode* images = AeronConfigNode_MapGet(node, "images");
		const AeronConfigNode* names = AeronConfigNode_MapGet(node, "names");

		if (!awards_keys(node, "mementos", mementoKeys) ||
			(images != NULL && !awards_read_missions(images, "mementos.images", AWARDS_MAX_IMAGE,
													 awards->mementoImages, &awards->mementoSprites)) ||
			(names != NULL && !awards_read_missions(names, "mementos.names", AWARDS_MAX_NAME,
													awards->mementoNames, &awards->mementoSprites))) {
			return 0;
		}
		awards->hasMementoImages = images != NULL;
		for (i = 0; i < XWA_AWARDS_UNLOCK_ITEMS; i++) {
			const AeronConfigNode* item = AeronConfigNode_MapGet(node, k_unlockItems[i]);
			const AeronConfigNode* mission = AeronConfigNode_MapGet(item, "mission");

			if (item == NULL) {
				continue;
			}
			if (!awards_keys(item, k_unlockPaths[i], unlockKeys)) {
				return 0;
			}
			if (mission == NULL || AeronConfigNode_Type(mission) != AERON_CONFIG_INT) {
				return awards_error(item, k_unlockPaths[i], "needs a mission number");
			}
			awards->hasUnlockMission[i] = 1;
			awards->unlockMission[i] = (int)AeronConfigNode_Int(mission, 0);
		}
	}
	return 1;
}

static const XwaAwards* awards_get(void) {
	if (!g_awardsLoaded) {
		AeronConfigFile* file = NULL;
		AeronConfigError error;

		g_awardsLoaded = 1;
		g_awards.mementoSprites = AWARDS_STOCK_MEMENTO_SPRITES;
		memset(&error, 0, sizeof(error));
		if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, XWA_AWARDS_PATH, &file, &error)) {
			if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
				Aeron_LogError("xwa.frontend", "%s:%d: %s", XWA_AWARDS_PATH, error.line,
							   error.message[0] ? error.message : "cannot be read");
				Aeron_LogError("xwa.frontend", "%s is ignored", XWA_AWARDS_PATH);
			}
		} else if (!awards_read(AeronConfigFile_Root(file), &g_awards)) {
			Aeron_LogError("xwa.frontend", "%s is ignored", XWA_AWARDS_PATH);
			awards_free(&g_awards);
			g_awards.mementoSprites = AWARDS_STOCK_MEMENTO_SPRITES;
		} else {
			Aeron_LogInfo("xwa.frontend", "%s: loaded the medals, ranks and mementos", XWA_AWARDS_PATH);
		}
		AeronConfigFile_Destroy(file);
	}
	return &g_awards;
}

int XwaAwards_KalidorLevels(void) {
	const XwaAwards* awards = awards_get();
	return awards->kalidorCount != 0 ? awards->kalidorCount : AWARDS_STOCK_KALIDOR_LEVELS;
}

unsigned int XwaAwards_KalidorThreshold(int level, const unsigned int* stock) {
	const XwaAwards* awards = awards_get();
	if (level >= 0 && level < awards->kalidorCount && awards->kalidorPoints[level] != 0) {
		return awards->kalidorPoints[level] * 10;
	}
	return stock[level];
}

int XwaAwards_KalidorSprites(void) {
	const int levels = XwaAwards_KalidorLevels();
	return levels > AWARDS_STOCK_KALIDOR_LEVELS ? levels : AWARDS_STOCK_KALIDOR_LEVELS;
}

unsigned int XwaAwards_RankThreshold(int rank, unsigned int stock) {
	const XwaAwards* awards = awards_get();
	return rank >= 0 && rank < awards->rankCount ? awards->ranks[rank] : stock;
}

int XwaAwards_RatingThreshold(int rating, int stock) {
	const XwaAwards* awards = awards_get();
	return rating >= 0 && rating < awards->ratingCount ? awards->ratings[rating] : stock;
}

const char* XwaAwards_BattleMedalName(int index) {
	const XwaAwards* awards = awards_get();
	if (index >= 0 && index < awards->battleCount && awards->battleNames[index] != NULL) {
		return awards->battleNames[index];
	}
	return index < AWARDS_STOCK_BATTLE_NAMES
			   ? FrontendString_Get((UIString)(STR_FAMILY_BATTLE_MEDAL1 + index))
			   : "";
}

const char* XwaAwards_KalidorName(int level) {
	const XwaAwards* awards = awards_get();
	if (level >= 0 && level < awards->kalidorCount && awards->kalidorNames[level] != NULL) {
		return awards->kalidorNames[level];
	}
	return level < AWARDS_STOCK_KALIDOR_LEVELS ? FrontendString_Get((UIString)(STR_FAMILY_KALIDOR1 + level))
											   : "";
}

const char* XwaAwards_MementoName(int mission) {
	const XwaAwards* awards = awards_get();
	if (mission >= 0 && mission < AWARDS_MAX_MEMENTOS && awards->mementoNames[mission] != NULL) {
		return awards->mementoNames[mission];
	}
	return mission < AWARDS_STOCK_MEMENTO_NAMES ? FrontendString_Get((UIString)(STR_FAMILY_MEDAL0 + mission))
												: "";
}

int XwaAwards_HasMementoImages(void) { return awards_get()->hasMementoImages; }

const char* XwaAwards_MementoImage(int mission) {
	const XwaAwards* awards = awards_get();
	return mission >= 0 && mission < AWARDS_MAX_MEMENTOS ? awards->mementoImages[mission] : NULL;
}

int XwaAwards_MementoSprites(void) { return awards_get()->mementoSprites; }

int XwaAwards_UnlockMission(XwaAwardsUnlockItem item, int stock) {
	const XwaAwards* awards = awards_get();
	const int value = awards->hasUnlockMission[item] ? awards->unlockMission[item] : stock;
	return value < 0 || value >= 255 ? 0 : value;
}

int XwaAwards_Unlocked(XwaAwardsUnlockItem item, int stock) {
	const int mission = XwaAwards_UnlockMission(item, stock);
	return mission <= 0 || g_pilotData.tourOfDutyMissions[mission - 1].completedCount != 0;
}

void XwaAwards_Shutdown(void) {
	awards_free(&g_awards);
	g_awardsLoaded = 0;
}
