#ifndef XWA_MOD_IMPORT_STOCK_SYMBOLS_H
#define XWA_MOD_IMPORT_STOCK_SYMBOLS_H

/*
 * The importer compiles its own copy of some game tables without
 * XWA_MODERN, so that the tables keep their stock sizes. The files are
 * model_def_static_data.inc, paiman_formation_tables.inc, object_type.c,
 * object_type_info.c and names.c.
 *
 * The game compiles the same files with XWA_MODERN, and a mod can change its
 * tables while the game runs. To link both copies into one program, these
 * defines rename every symbol of the importer's copy.
 *
 * Include this header first in the stock_*.c files and in every importer
 * file that uses these tables. When one of the files gets a new external
 * symbol, add it here.
 */

/* model_def_static_data.inc */
#define g_modelDefs g_stockModelDefs

/* paiman_formation_tables.inc */
#define g_formPosX g_stockFormPosX
#define g_formPosY g_stockFormPosY
#define g_formPosZ g_stockFormPosZ
#define g_formationDivisor g_stockFormationDivisor

/* object_type.c */
#define g_objectTypeTables g_stockObjectTypeTables
#define ObjectType_GetWarheadKindIndex StockObjectType_GetWarheadKindIndex

/* object_type_info.c */
#define g_objectTypes g_stockObjectTypes
#define GetModelDefIndexFromType Stock_GetModelDefIndexFromType

/* names.c */
#define XwaNames_Find StockNames_Find
#define XwaNames_LogUnknown StockNames_LogUnknown
#define XwaNames_Name StockNames_Name
#define XwaNames_Read StockNames_Read
#define XwaNames_RenameObjectType StockNames_RenameObjectType
#define XwaNames_ResetObjectTypeNames StockNames_ResetObjectTypeNames
#define XwaNames_StockName StockNames_StockName
#define XwaNames_ValidName StockNames_ValidName
#define g_xwaDifficultyNames g_stockDifficultyNames
#define g_xwaFamilyNames g_stockFamilyNames
#define g_xwaGenusNames g_stockGenusNames
#define g_xwaHardpointKindNames g_stockHardpointKindNames
#define g_xwaHudAreaNames g_stockHudAreaNames
#define g_xwaLaserKindNames g_stockLaserKindNames
#define g_xwaObjectTypeFlagNames g_stockObjectTypeFlagNames
#define g_xwaObjectTypeNames g_stockObjectTypeNames
#define g_xwaRankNames g_stockRankNames
#define g_xwaReticleSlotNames g_stockReticleSlotNames
#define g_xwaScreenSourceNames g_stockScreenSourceNames
#define g_xwaShipListCategoryNames g_stockShipListCategoryNames
#define g_xwaShipListCategoryTexts g_stockShipListCategoryTexts
#define g_xwaSoundWeaponGroupNames g_stockSoundWeaponGroupNames
#define g_xwaWarheadNames g_stockWarheadNames
#define g_xwaWeaponTypeNames g_stockWeaponTypeNames

#endif /* XWA_MOD_IMPORT_STOCK_SYMBOLS_H */
