#ifndef XWA_MOD_IMPORT_STOCK_TABLES_H
#define XWA_MOD_IMPORT_STOCK_TABLES_H

#include "xwau_game_tables.h"
#include "xwau_names.h"

/* Creates the stock game's craft tables. They come from the tables built into
 * OpenXWA, which a mod's changes apply to, and from the ship lists in the
 * game folder `game`. Returns NULL and reports the problem when a list can't
 * be read. */
XwauGameTables* ImportStockTables_Create(const char* game);

/* Fills `out` with the game's name tables and the stock object type
 * names. */
void ImportStockTables_Names(XwauNames* out);

/* Fills `out` with the stock hull icons, map icons and hardpoint kinds, from
 * the game's own tables. */
void ImportStockTables_Values(XwauStockValues* out);

#endif
