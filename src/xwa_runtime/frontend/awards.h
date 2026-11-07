#ifndef XWA_RUNTIME_FRONTEND_AWARDS_H
#define XWA_RUNTIME_FRONTEND_AWARDS_H

/* A mod's awards: the names of the battle medals, the Kalidor Crescent
 * levels and their thresholds, the rank and rating thresholds, and the
 * mementos of the family room. The mod documentation describes the keys.
 *
 * The game reads the mod's awards file once. A missing key keeps the stock
 * behaviour, and a file with an error is ignored as a whole. The medal list
 * of the game still sets how many battle medals there are and which missions
 * give them. A Kalidor level without a threshold keeps its stock one, so
 * only the first 6 levels can leave it out. A threshold list shorter than
 * the stock one keeps the stock values of the levels it doesn't reach. The
 * Lady Blue and cologne missions are counted from 1. When the file doesn't
 * name an award, the game uses its stock text if it has one, and an empty
 * text otherwise. */

#ifdef __cplusplus
extern "C" {
#endif

/* The number of Kalidor Crescent levels that the pilot can reach: the
 * length of the `kalidor` list, or 6. */
int XwaAwards_KalidorLevels(void);
/* The bonus score, in tenths of a point, that reaches Kalidor level `level`
 * (0 for the first level). `stock` is the stock table of 6 thresholds. */
unsigned int XwaAwards_KalidorThreshold(int level, const unsigned int* stock);
/* The number of Kalidor sprites to load and draw: the larger of 6 and the
 * number of levels. */
int XwaAwards_KalidorSprites(void);

/* The score that promotes the pilot from rank `rank`, or `stock`. */
unsigned int XwaAwards_RankThreshold(int rank, unsigned int stock);
/* The points that promote the pilot from rating `rating`, or `stock`. */
int XwaAwards_RatingThreshold(int rating, int stock);

/* The names of battle medal `index` (0 for the first), of Kalidor level
 * `level` (0 for the first) and of the memento of mission `mission`. */
const char* XwaAwards_BattleMedalName(int index);
const char* XwaAwards_KalidorName(int level);
const char* XwaAwards_MementoName(int mission);

/* Whether the file replaces the memento images (`mementos.images`). */
int XwaAwards_HasMementoImages(void);
/* The detail image of the memento of mission `mission`, or NULL when it has
 * none. Only for when XwaAwards_HasMementoImages() is true. */
const char* XwaAwards_MementoImage(int mission);
/* The number of memento sprites to load: the larger of 53 and the highest
 * mission in `mementos` plus one. */
int XwaAwards_MementoSprites(void);

/* The family-room items that a Tour of Duty mission unlocks. */
typedef enum XwaAwardsUnlockItem {
	XWA_AWARDS_LADY_BLUE,
	XWA_AWARDS_COLOGNE,
	XWA_AWARDS_UNLOCK_ITEMS,
} XwaAwardsUnlockItem;

/* The Tour of Duty mission, from 1, that unlocks `item`. `stock` is the
 * stock mission. Returns 0 when the item is always unlocked. */
int XwaAwards_UnlockMission(XwaAwardsUnlockItem item, int stock);
/* Whether `item` is unlocked: its mission is completed or it has none. */
int XwaAwards_Unlocked(XwaAwardsUnlockItem item, int stock);

void XwaAwards_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
