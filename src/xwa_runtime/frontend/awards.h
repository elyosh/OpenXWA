#ifndef XWA_RUNTIME_FRONTEND_AWARDS_H
#define XWA_RUNTIME_FRONTEND_AWARDS_H

/* Medals, Kalidor Crescent levels, promotion thresholds and family-room
 * mementos from frontend/awards.yaml.
 *
 *     version: 1
 *     battle_medals:            # names, by medal in medals.txt order
 *       - { name: 'Medal of Redemption' }
 *     kalidor:                  # one entry per level
 *       - { points: 5000, name: 'First Initiate' }
 *     rank_thresholds: [7500, 15000, 25000]
 *     rating_thresholds: [100, 200, 350]
 *     mementos:                 # by Tour of Duty mission index
 *       images: { 9: 'frontres\medals\tf.bmp' }
 *       names: { 9: 'TIE Fighter Model' }
 *
 * The file is read once. Keys are optional, and a missing key keeps the
 * stock behaviour. If the file has an error, the error is logged with its
 * line and the whole file is ignored.
 *
 * - `battle_medals` only names the medals. medals.txt still sets how many
 *   there are and which missions give them.
 * - `kalidor` sets the number of levels, up to 100, and each level's
 *   threshold in whole bonus points. A level without `points` keeps its
 *   stock threshold, so only the first 6 levels can leave it out.
 * - `rank_thresholds` (up to 8 values) and `rating_thresholds` (up to 25)
 *   replace the stock values in order. A shorter list keeps the stock values
 *   for the rest.
 * - `mementos.images` replaces the stock detail images. A mission that isn't
 *   listed then has no detail image and can't be clicked.
 *   `mementos.names` sets the tooltip and the title of the detail view.
 *
 * A name that the file doesn't set is the stock text when the game has one,
 * and empty otherwise. */

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

void XwaAwards_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
