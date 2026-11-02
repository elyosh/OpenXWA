#ifndef XWA_RUNTIME_FRONTEND_AWARDS_H
#define XWA_RUNTIME_FRONTEND_AWARDS_H

/* Medals, Kalidor Crescent levels, promotion thresholds and family-room
 * mementos from frontend/awards.yaml.
 *
 *     version: 1
 *     battle_medals:            # names, in the order of medals.txt
 *       - { name: 'Medal of Redemption' }
 *     kalidor:                  # one entry per level
 *       - { points: 5000, name: 'First Initiate' }
 *     rank_thresholds: [7500, 15000, 25000]
 *     rating_thresholds: [100, 200, 350]
 *     mementos:                 # keyed by Tour of Duty mission index
 *       images: { 9: 'frontres\medals\tf.bmp' }
 *       names: { 9: 'TIE Fighter Model' }
 *       lady_blue: { mission: 23 }
 *       cologne: { mission: 9 }
 *
 * The game reads the file once. All keys are optional, and a missing key
 * keeps the stock behaviour. If the file has an error, the game logs it
 * with its line number and ignores the whole file.
 *
 * - `battle_medals` only names the medals. medals.txt still sets how many
 *   medals there are and which missions give them.
 * - `kalidor` sets the number of levels, up to 100, and the threshold of
 *   each level in whole bonus points. A level without `points` keeps its
 *   stock threshold, so only the first 6 levels can leave it out.
 * - `rank_thresholds` (up to 8 values) and `rating_thresholds` (up to 25)
 *   replace the stock values in order. If a list is shorter, the remaining
 *   values stay stock.
 * - `mementos.images` replaces the stock detail images. A mission that
 *   isn't listed has no detail image, and its memento can't be clicked.
 * - `mementos.names` sets the tooltip and the title of the detail view.
 * - `mementos.lady_blue.mission` and `mementos.cologne.mission` set the Tour
 *   of Duty mission that unlocks the Lady Blue and the cologne in the family
 *   room. Missions are counted from 1, and the stock missions are 23 and 9.
 *   A value of 0 or less, or 255 or more, unlocks the item from the start.
 *   The item's detail view shows the award text of that mission.
 *
 * When the file doesn't set a name, the game uses its stock text if it has
 * one, and an empty text otherwise. */

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
