/* Writes the reference pages of the mod documentation:
 *
 *   xwa_mod_tables --game-data <game folder> --out modding/reference
 *
 * The names and numbers come from the game's own tables, so the pages can't
 * drift from the code. The game folder gives the model of each object type
 * and the ship list name of each craft type. The descriptions are written
 * here, by name. The tool fails when a name has no description, or when a
 * description names a value that doesn't exist.
 *
 * With --addon, it prints the Blender add-on's name lists as Python instead.
 * Paste them into the add-on when the lists change. */

#include "aeron/asset/flight_names.h"
#include "aeron/scene/gltf_mesh.h"
#include "aeron/vfs.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/flight/object/laser.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/audio/sound_banks.h"
#include "xwa_runtime/frontend/frontend_screens.h"
#include "xwa_runtime/models/model_roles.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/models/names.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The description of the value called `name`. */
typedef struct NameText {
	const char* name;
	const char* text;
} NameText;

/* The values of a table: names[i] is the name of value first + i, or NULL
   for a value without a name. */
typedef struct NameColumn {
	const char* const* names;
	int first;
	int count;
} NameColumn;

/* Set when a check fails. The pages are still written, so that all the
   problems are reported at once. */
static int s_failed;

static NameColumn ColumnOfNameTable(const XwaNameTable* table) {
	const NameColumn column = { table->names, table->first, table->count };
	return column;
}

/* Fills `names` with the names of Aeron's `list`. */
static NameColumn ColumnOfFlightNames(AeronFlightNameList list, const char** names, int capacity) {
	NameColumn column = { names, 0, (int)Aeron_FlightNameCount(list) };
	if (column.count > capacity) {
		fprintf(stderr, "xwa_mod_tables: flight name list %d has more than %d names\n", (int)list, capacity);
		s_failed = 1;
		column.count = capacity;
	}
	for (int value = 0; value < column.count; ++value) {
		names[value] = Aeron_FlightName(list, (uint32_t)value);
	}
	return column;
}

/* Returns the description of `name`, or NULL after reporting that it has
   none. */
static const char* DescriptionOf(const NameText* texts, const char* name) {
	for (int i = 0; texts[i].name != NULL; ++i) {
		if (strcmp(texts[i].name, name) == 0) {
			return texts[i].text;
		}
	}
	fprintf(stderr, "xwa_mod_tables: `%s` has no description\n", name);
	s_failed = 1;
	return NULL;
}

/* Reports the descriptions whose name isn't in `column`. */
static void CheckDescriptionsAreUsed(const NameText* texts, NameColumn column) {
	for (int i = 0; texts[i].name != NULL; ++i) {
		int found = 0;
		for (int index = 0; index < column.count && !found; ++index) {
			found = column.names[index] != NULL && strcmp(column.names[index], texts[i].name) == 0;
		}
		if (!found) {
			fprintf(stderr, "xwa_mod_tables: the description of `%s` names no value\n", texts[i].name);
			s_failed = 1;
		}
	}
}

/* Opens page `file` in folder `folder` and writes its title and a comment
   that says where the page comes from. */
static FILE* OpenPage(const char* folder, const char* file, const char* title) {
	char path[1024];
	snprintf(path, sizeof path, "%s/%s", folder, file);
	FILE* page = fopen(path, "w");
	if (page == NULL) {
		fprintf(stderr, "xwa_mod_tables: can't write %s\n", path);
		s_failed = 1;
		return NULL;
	}
	fprintf(page, "<!-- Made by xwa_mod_tables. Change the tool, not this page. -->\n\n# %s\n\n", title);
	return page;
}

static void ClosePage(FILE* page) {
	if (page != NULL && fclose(page) != 0) {
		fprintf(stderr, "xwa_mod_tables: can't finish writing a page\n");
		s_failed = 1;
	}
}

static void WriteHeading(FILE* page, const char* title) { fprintf(page, "## %s\n\n", title); }

static void WriteParagraph(FILE* page, const char* text) { fprintf(page, "%s\n\n", text); }

/* Writes a table of the values in `column` and their names. `heading` is the
   heading of the value column, such as "Number". */
static void WriteValueTable(FILE* page, const char* heading, NameColumn column) {
	fprintf(page, "| %s | Name |\n|---:|---|\n", heading);
	for (int index = 0; index < column.count; ++index) {
		if (column.names[index] != NULL) {
			fprintf(page, "| %d | `%s` |\n", column.first + index, column.names[index]);
		}
	}
	fprintf(page, "\n");
}

/* Writes a table of the names in `column` without their values. With
   `texts`, a second column with the heading `text_heading` describes each
   name. */
static void WriteNameList(FILE* page, NameColumn column, const NameText* texts, const char* text_heading) {
	fprintf(page, "| Name |");
	if (texts != NULL) {
		fprintf(page, " %s |", text_heading);
	}
	fprintf(page, "\n|---|%s\n", texts ? "---|" : "");
	for (int index = 0; index < column.count; ++index) {
		const char* name = column.names[index];
		if (name == NULL) {
			continue;
		}
		fprintf(page, "| `%s` |", name);
		if (texts != NULL) {
			const char* text = DescriptionOf(texts, name);
			fprintf(page, " %s |", text ? text : "");
		}
		fprintf(page, "\n");
	}
	fprintf(page, "\n");
	if (texts != NULL) {
		CheckDescriptionsAreUsed(texts, column);
	}
}

/* ---- The game data -------------------------------------------------- */

enum { MAX_LIST_ROWS = 1024, MAX_TEXT = 256 };

/* The model keys of a model list, by row. */
typedef struct ModelList {
	char keys[MAX_LIST_ROWS][XWA_MODEL_SOURCE_KEY_MAX];
	int count;
} ModelList;

/* The ship list line of each craft type. */
typedef struct ShipList {
	char names[XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT][MAX_TEXT];
	char categories[XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT][MAX_TEXT];
} ShipList;

typedef struct GameData {
	ModelList lists[2]; /* spacecraft, then equipment */
	ShipList ships;
} GameData;

/* Reads text file `path` of the game folder and calls `line_read` for each
   line, without its line break. */
static int ReadGameLines(AeronVfs* vfs, const char* path, void (*line_read)(void* data, char* line),
						 void* data) {
	uint8_t* bytes = NULL;
	size_t size = 0;
	if (!AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_ASSET, path, 0, &bytes, &size)) {
		fprintf(stderr, "xwa_mod_tables: can't read %s in the game folder\n", path);
		return 0;
	}
	char* text = (char*)realloc(bytes, size + 1);
	if (text == NULL) {
		free(bytes);
		return 0;
	}
	text[size] = '\0';
	for (char* line = text; line != NULL && *line != '\0';) {
		char* end = line + strcspn(line, "\r\n");
		char* next = NULL;
		if (*end != '\0') {
			next = end[0] == '\r' && end[1] == '\n' ? end + 2 : end + 1;
		}
		*end = '\0';
		line_read(data, line);
		line = next;
	}
	free(text);
	return 1;
}

static void AddModelListRow(void* data, char* line) {
	ModelList* list = (ModelList*)data;
	if (list->count < MAX_LIST_ROWS) {
		if (!XwaModelSource_NormalizeKey(line, list->keys[list->count])) {
			list->keys[list->count][0] = '\0';
		}
		++list->count;
	}
}

/* Reads a line such as "!SPECIES_1!Xwing,Fighter,Flyable,...". */
static void AddShipListLine(void* data, char* line) {
	ShipList* ships = (ShipList*)data;
	char* end;
	if (strncmp(line, "!SPECIES_", 9) != 0) {
		return;
	}
	const long craft_type = strtol(line + 9, &end, 10);
	if (*end != '!' || craft_type <= 0 || craft_type >= XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT) {
		return;
	}
	char* name = end + 1;
	char* category = strchr(name, ',');
	if (category == NULL) {
		return;
	}
	*category++ = '\0';
	category[strcspn(category, ",")] = '\0';
	snprintf(ships->names[craft_type], MAX_TEXT, "%s", name);
	snprintf(ships->categories[craft_type], MAX_TEXT, "%s", category);
}

static int ReadGameData(const char* folder, GameData* out) {
	AeronVfsConfig config;
	memset(&config, 0, sizeof config);
	config.asset_root = folder;
	config.resource_root = folder;
	config.user_root = folder;
	config.temp_root = folder;
	AeronVfs* vfs = AeronVfs_Create(&config);
	if (vfs == NULL ||
		!AeronVfs_SetRootOptions(vfs, AERON_VFS_ROOT_ASSET, AERON_VFS_ROOT_OPTION_CASE_INSENSITIVE_LOOKUP)) {
		fprintf(stderr, "xwa_mod_tables: can't open the game folder %s\n", folder);
		AeronVfs_Destroy(vfs);
		return 0;
	}
	memset(out, 0, sizeof *out);
	const int ok = ReadGameLines(vfs, "FlightModels/Spacecraft0.LST", AddModelListRow, &out->lists[0]) &&
				   ReadGameLines(vfs, "FlightModels/Equipment0.LST", AddModelListRow, &out->lists[1]) &&
				   ReadGameLines(vfs, "SHIPLIST.TXT", AddShipListLine, &out->ships);
	AeronVfs_Destroy(vfs);
	return ok;
}

/* ---- Object types ---------------------------------------------------- */

static const NameText kSettableFlagTexts[] = {
	{ "player_targetable", "The player can target it." },
	{ "ai_targetable", "Other craft can target it." },
	{ "nav_buoy", "It is a navigation buoy. The key that targets navigation buoys selects it." },
	{ "static_object", "Its flight groups are placed when the mission starts, instead of flying in. Mines "
					   "and buoys use it." },
	{ "float_hardpoints", "It fires from points spread over its hull, not from its weapon hardpoints. The "
						  "large starships use it." },
	{ "spins", "It turns all the time. Gun platforms, mines, satellites and buoys use it." },
	{ "collision_face_groups", "Shots between AI craft test their hits against a lower detail level of the "
							   "model." },
	{ NULL, NULL },
};

/* Writes the name of `value` in `table`, or the number when it has none. */
static void WriteNameOrNumber(FILE* page, const XwaNameTable* table, int value) {
	const char* name = XwaNames_StockName(table, value);
	if (name != NULL) {
		fprintf(page, "`%s`", name);
	} else {
		fprintf(page, "%d", value);
	}
}

static void WriteObjectTypeTable(FILE* page, const GameData* data) {
	static const char* const kLists[2] = { "spacecraft", "equipment" };
	fprintf(page, "| Number | Name | Model | List | Genus | Family |\n|---:|---|---|---|---|---|\n");
	for (int type = 0; type < XWA_STOCK_OBJECT_TYPE_COUNT; ++type) {
		const ObjectTypeInfo* info = &g_objectTypes[type];
		const char* name = XwaNames_StockName(&g_xwaObjectTypeNames, type);
		fprintf(page, "| %d | `%s` | ", type, name ? name : "?");
		if ((info->recordFlags & OBJECT_TYPE_RECORD_TEXTURE_BACKED) != 0 && info->textureGroup <= 1) {
			const ModelList* list = &data->lists[info->textureGroup];
			if ((int)info->frameCount < list->count && list->keys[info->frameCount][0] != '\0') {
				fprintf(page, "`%s` | %s", list->keys[info->frameCount], kLists[info->textureGroup]);
			} else {
				fprintf(page, " | ");
			}
		} else if ((info->recordFlags & OBJECT_TYPE_RECORD_TEXTURE_BACKED) != 0 &&
				   info->textureGroup != 0xffff) {
			fprintf(page, "image group %d | ", info->textureGroup);
		} else {
			fprintf(page, " | ");
		}
		/* `none` has no data. */
		if (info->recordFlags == 0) {
			fprintf(page, " |  |  |\n");
			continue;
		}
		fprintf(page, " | ");
		WriteNameOrNumber(page, &g_xwaGenusNames, info->genusId);
		fprintf(page, " | ");
		WriteNameOrNumber(page, &g_xwaFamilyNames, info->familyId);
		fprintf(page, " |\n");
	}
	fprintf(page, "\n");
}

static void WriteObjectTypeFlags(FILE* page) {
	fprintf(page, "| Name | Meaning |\n|---|---|\n");
	for (int bit = 0; bit < g_xwaObjectTypeFlagNames.count; ++bit) {
		const char* name = XwaNames_StockName(&g_xwaObjectTypeFlagNames, bit);
		if (name == NULL) {
			continue;
		}
		if ((XWA_OBJECT_TYPE_SETTABLE_FLAGS >> bit) & 1) {
			const char* text = DescriptionOf(kSettableFlagTexts, name);
			fprintf(page, "| `%s` | %s |\n", name, text ? text : "");
		} else {
			fprintf(page, "| `%s` | Set by the game. A file can't change it. |\n", name);
		}
	}
	fprintf(page, "\n");
	CheckDescriptionsAreUsed(kSettableFlagTexts, ColumnOfNameTable(&g_xwaObjectTypeFlagNames));
}

static void WriteObjectTypesPage(const char* folder, const GameData* data) {
	FILE* page = OpenPage(folder, "object_types.md", "Object types");
	if (page == NULL) {
		return;
	}
	WriteParagraph(page, "An object type is a kind of object in flight, such as a craft, a weapon, a mine or "
						 "an explosion. Each object type has a number and a name. Mod files use the name. "
						 "The community tools show the number.");
	WriteParagraph(page,
				   "An object type file can give its object type a new name with `name`. The other files "
				   "of the mod must then use the new name. The slots named `slot_<number>` are free. A "
				   "mod can use them for new object types.");
	WriteParagraph(
		page,
		"**Model** is the model that the object type loads in the original game. The mod replaces it with "
		"`flight/models/<model>.glb`. **List** is the model list that the model is in. Object type files "
		"give it in `list`. Object types without a model are drawn with the images of an image group.");
	WriteObjectTypeTable(page, data);

	WriteHeading(page, "Genus");
	WriteParagraph(page, "Values for `genus` in object type files.");
	WriteValueTable(page, "Number", ColumnOfNameTable(&g_xwaGenusNames));

	WriteHeading(page, "Family");
	WriteParagraph(page, "Values for `family` in object type files.");
	WriteValueTable(page, "Number", ColumnOfNameTable(&g_xwaFamilyNames));

	WriteHeading(page, "Flags");
	WriteParagraph(page, "The `flags` list of an object type file gives the flags that are on. A flag that a "
						 "file can change is off when the list doesn't name it. Without `flags`, the object "
						 "type keeps the flags of the original game.");
	WriteObjectTypeFlags(page);
	ClosePage(page);
}

/* ---- Craft types ----------------------------------------------------- */

/* Writes the name of ship list category `text`, or `text` when it has no
   name. */
static void WriteShipListCategory(FILE* page, const char* text) {
	for (int category = 0; category < g_xwaShipListCategoryTexts.count; ++category) {
		if (strcmp(XwaNames_StockName(&g_xwaShipListCategoryTexts, category), text) == 0) {
			fprintf(page, "`%s`", XwaNames_StockName(&g_xwaShipListCategoryNames, category));
			return;
		}
	}
	fprintf(page, "%s", text);
}

static void WriteCraftTypesPage(const char* folder, const GameData* data) {
	FILE* page = OpenPage(folder, "craft_types.md", "Craft types");
	if (page == NULL) {
		return;
	}
	WriteParagraph(page,
				   "A craft type is the type that a mission gives a flight group. It is also a line of "
				   "the ship list and of the tech room. Each craft type points to an object type, which "
				   "gives the model and the flight data.");
	WriteParagraph(page, "Craft types have numbers, not names. A craft type file gives the number in "
						 "`craft_type`. **Name** is the name in the original ship list.");
	fprintf(page, "| Number | Name | Object type | Category |\n|---:|---|---|---|\n");
	for (int craft_type = 1; craft_type < XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT; ++craft_type) {
		const int object_type = g_objectTypeTables.craftTypeToObjectType[craft_type];
		const char* object_name = XwaNames_StockName(&g_xwaObjectTypeNames, object_type);
		fprintf(page, "| %d | %s | `%s` | ", craft_type, data->ships.names[craft_type],
				object_name ? object_name : "?");
		WriteShipListCategory(page, data->ships.categories[craft_type]);
		fprintf(page, " |\n");
	}
	fprintf(page, "\n");

	WriteHeading(page, "Ship list categories");
	WriteParagraph(page,
				   "Values for `ship_list.category` in craft type files. **Shown as** is the text that the "
				   "game shows. The original game also has the category Planet/asteroid, which a file "
				   "can't set.");
	fprintf(page, "| Name | Shown as |\n|---|---|\n");
	for (int category = 0; category < g_xwaShipListCategoryNames.count; ++category) {
		fprintf(page, "| `%s` | %s |\n", XwaNames_StockName(&g_xwaShipListCategoryNames, category),
				XwaNames_StockName(&g_xwaShipListCategoryTexts, category));
	}
	fprintf(page, "\n");
	ClosePage(page);
}

/* ---- Weapons --------------------------------------------------------- */

static const NameText kLaserKindTexts[] = {
	{ "none", "An empty group." },
	{ "laser_cannon", "Cannons that the pilot fires. They use the laser charge." },
	{ "ion_cannon", "Ion cannons that the pilot fires. They are the second set of cannons." },
	{ "turret", "Turrets that aim by themselves. They also shoot at incoming warheads. A gunner can fire "
				"them." },
	{ "long_range_turret", "Turrets with twice the default range. They don't fire at close targets or at "
						   "warheads." },
	{ NULL, NULL },
};

static const NameText kHardpointKindTexts[] = {
	{ "none", "Fires nothing." },
	{ "laser", "Fires the craft's lasers." },
	{ "warhead", "Fires the craft's warheads." },
	{ NULL, NULL },
};

static const NameText kWeaponSoundGroupTexts[] = {
	{ "rebel_laser", "`rebel_laser`" },
	{ "rebel_laser_turbo", "`rebel_laser_turbo`" },
	{ "imperial_laser", "`imperial_laser`" },
	{ "imperial_laser_turbo", "`imperial_laser_turbo`" },
	{ "ion_cannon", "`ion_cannon`" },
	{ "ion_cannon_turbo", "`ion_cannon_turbo`" },
	{ "turbolaser_red", "`turbolaser_red`, `turbolaser_red_light`, `turbolaser_red_medium` and "
						"`turbolaser_red_heavy`" },
	{ "turbolaser_green", "`turbolaser_green`, `turbolaser_green_light`, `turbolaser_green_medium` "
						  "and `turbolaser_green_heavy`" },
	{ "turbolaser_ion", "`turbolaser_ion`" },
	{ "custom_weapon", "`custom_weapon`" },
	{ NULL, NULL },
};

/* Writes the weapon types with their stats in the original game. */
static void WriteWeaponTypeTable(FILE* page) {
	fprintf(page, "| Number | Name | Speed | Power | Life | Score |\n|---:|---|---:|---:|---:|---:|\n");
	for (int type = g_xwaWeaponTypeNames.first;
		 type < g_xwaWeaponTypeNames.first + g_xwaWeaponTypeNames.count; ++type) {
		const char* name = XwaNames_StockName(&g_xwaWeaponTypeNames, type);
		if (type > OBJ_LaserImperialDS) {
			fprintf(page, "| %d | `%s` |  |  |  |  |\n", type, name);
			continue;
		}
		const int index = type - OBJ_LaserRebel;
		/* The life in ticks of 4 ms, computed the same way as the game. */
		const unsigned life_ticks = 236u * g_projectileLifetimeSecondsByType[index] +
									MATH2_fraction(g_projectileLifetimeFracQ16ByType[index], 236u);
		/* A speed unit is 1/2.25 MGLT. Two decimals are enough to get the
		   exact speed back. */
		const double mglt = round(g_projectileSpeedByType[index] / 2.25 * 100.0) / 100.0;
		fprintf(page, "| %d | `%s` | %g | %d | %u | %u |\n", type, name, mglt,
				g_projectileDamageByType[index], life_ticks * 4,
				g_warheadProjectilePointValueByObjectType[type]);
	}
	fprintf(page, "\n");
}

static void WriteWeaponsPage(const char* folder) {
	FILE* page = OpenPage(folder, "weapons.md", "Weapons");
	if (page == NULL) {
		return;
	}
	WriteHeading(page, "Weapon types");
	WriteParagraph(page,
				   "A weapon type is the object type of a shot, such as a laser bolt or a torpedo. Weapon "
				   "types are object types 280 to 307, with the same names. Laser groups, weapon settings "
				   "and weapon colours use them.");
	WriteParagraph(page,
				   "The other columns give the stats of the original game, which the `stats` weapon "
				   "profiles of `flight/flight.yaml` and of the model settings can change. **Speed** is in "
				   "MGLT, and a shot of a fighter adds the fighter's speed. **Power** is the damage of a "
				   "shot, in the points of hull and shields. **Life** is how long a shot lives, in "
				   "milliseconds. **Score** is the score of a warhead hit.");
	WriteWeaponTypeTable(page);

	WriteHeading(page, "Warheads");
	WriteParagraph(page, "The warheads that a mission can give a flight group. Mission files and "
						 "`warhead_capacity_percent` use these names. The numbers are the values in the "
						 "`.TIE` mission files.");
	WriteValueTable(page, "Number", ColumnOfNameTable(&g_xwaWarheadNames));

	WriteHeading(page, "Laser group kinds");
	WriteParagraph(page, "Values for the `kind` of a laser group in `craft_record.lasers`.");
	WriteNameList(page, ColumnOfNameTable(&g_xwaLaserKindNames), kLaserKindTexts, "Meaning");

	WriteHeading(page, "Hardpoint kinds");
	WriteParagraph(page,
				   "What a hardpoint type fires. The values of `hardpoint_kinds` in the weapon settings.");
	WriteNameList(page, ColumnOfNameTable(&g_xwaHardpointKindNames), kHardpointKindTexts, "Meaning");

	WriteHeading(page, "Weapon sound groups");
	WriteParagraph(page,
				   "The keys under each object type in the `weapon` sound bank of `flight/sounds.yaml`. "
				   "Each group plays for the weapon types in the second column. The other weapon types, "
				   "such as warheads, have no group in this bank.");
	WriteNameList(page, ColumnOfNameTable(&g_xwaSoundWeaponGroupNames), kWeaponSoundGroupTexts,
				  "Weapon types");

	WriteHeading(page, "Ranks");
	WriteParagraph(page, "The keys of `weapons.fire_ratio_by_rank` in `flight/flight.yaml`, from the lowest "
						 "rank to the highest.");
	WriteNameList(page, ColumnOfNameTable(&g_xwaRankNames), NULL, NULL);

	WriteHeading(page, "Difficulties");
	WriteParagraph(page, "The keys of `weapons.damage_by_difficulty` in `flight/flight.yaml`.");
	WriteNameList(page, ColumnOfNameTable(&g_xwaDifficultyNames), NULL, NULL);
	ClosePage(page);
}

/* ---- Model values ---------------------------------------------------- */

/* The descriptions of the hardpoint types that aren't weapon positions. The
   weapon positions, types 1 to 18, are described from the weapon type that
   they fire. */
enum { FIRST_WEAPON_HARDPOINT = 1, LAST_WEAPON_HARDPOINT = 18, HARDPOINT_TO_WEAPON_TYPE = 279 };

static const NameText kHardpointTypeTexts[] = {
	{ "none", "Not used, except on object types with the `float_hardpoints` flag, where it is a turret "
			  "position." },
	{ "gunner", "Not a gunner seat. The game also tests the craft's collisions from this point." },
	{ "cockpit_sparks", "In a cockpit model: where sparks appear when the player's craft is hit. Up to 16." },
	{ "docking_point", "Where another craft parks when it is ordered to dock with this one. Up to 9." },
	{ "towing", "The front link point. In a chain of linked objects, the next object attaches its back link "
				"point here." },
	{ "acceleration_start", "The same as `towing`. When a model has both, the last one counts." },
	{ "acceleration_end",
	  "The back link point. The game also tests the craft's collisions from this point." },
	{ "inside_hangar", "The hangar: where craft launch from and where they land. The hangar's tractor beam "
					   "pulls the player in near it." },
	{ "outside_hangar", "The hangar entrance. Craft fly to it before they land, and head toward it when "
						"they launch." },
	{ "dock_from_big", "On a craft that another craft carries: the height at which it is held, when both "
					   "craft are freighters or larger." },
	{ "dock_from_small", "On a craft that another craft carries: the height at which it is held, when one of "
						 "the craft is smaller than a freighter." },
	{ "dock_to_big", "On a carrier: the height at which it holds a craft that is a freighter or larger." },
	{ "dock_to_small", "On a carrier: the height at which it holds a craft that is smaller than a "
					   "freighter." },
	{ "pilot_eye", "The pilot's eye: where the cockpit view is." },
	{ "engine_glow", "Not used. Engine glows are separate nodes of the model." },
	{ "custom_1", "Not used, unless a weapon setting makes it a firing position." },
	{ "custom_2", "Not used, unless a weapon setting makes it a firing position." },
	{ "custom_3", "Not used, unless a weapon setting makes it a firing position." },
	{ "custom_4", "Not used, unless a weapon setting makes it a firing position." },
	{ "custom_5", "Not used, unless a weapon setting makes it a firing position." },
	{ "custom_6", "Not used, unless a weapon setting makes it a firing position." },
	{ "jamming_point", "An enemy craft with working systems jams the player's weapons near this point. Up to "
					   "8." },
	{ NULL, NULL },
};

static const NameText kExplosionFlagTexts[] = {
	{ "hit_points", "The part has its own hit points." },
	{ "detachable", "The part can be damaged and break off." },
	{ NULL, NULL },
};

static const NameText kScreenLayerTexts[] = {
	{ "graphics", "The display's images." },
	{ "text", "The display's text." },
	{ "bar", "A fill bar in a plain colour, given by the rectangle's `bar`." },
	{ NULL, NULL },
};

static const NameText kReticleSlotTexts[] = {
	{ "laser", "The laser reticle." },
	{ "laser_in_range", "Drawn over the laser reticle when the target is in range." },
	{ "warhead", "The warhead reticle." },
	{ "warhead_lock", "Drawn over the warhead reticle while the player gets a lock, and when the lock is "
					  "done." },
	{ "cannon_ready", "Drawn over the mark of a cannon when that cannon fires next." },
	{ "cannon", "The mark of each cannon around the reticle." },
	{ "threats.laser_mode.laser", "The light that shows a laser threat, with the laser reticle." },
	{ "threats.laser_mode.turret", "The light that shows a turret threat, with the laser reticle." },
	{ "threats.laser_mode.beam", "The light that shows a beam threat, with the laser reticle." },
	{ "threats.laser_mode.warhead", "The light that shows a warhead threat, with the laser reticle." },
	{ "threats.warhead_mode.laser", "The light that shows a laser threat, with the warhead reticle." },
	{ "threats.warhead_mode.turret", "The light that shows a turret threat, with the warhead reticle." },
	{ "threats.warhead_mode.beam", "The light that shows a beam threat, with the warhead reticle." },
	{ "threats.warhead_mode.warhead", "The light that shows a warhead threat, with the warhead reticle." },
	{ NULL, NULL },
};

static void WriteHardpointTypes(FILE* page, NameColumn column) {
	fprintf(page, "| Number | Name | Meaning |\n|---:|---|---|\n");
	for (int type = 0; type < column.count; ++type) {
		if (type >= FIRST_WEAPON_HARDPOINT && type <= LAST_WEAPON_HARDPOINT) {
			const char* weapon = XwaNames_StockName(&g_xwaWeaponTypeNames, type + HARDPOINT_TO_WEAPON_TYPE);
			fprintf(page, "| %d | `%s` | A firing position for `%s`. |\n", type, column.names[type],
					weapon ? weapon : "?");
		} else {
			const char* text = DescriptionOf(kHardpointTypeTexts, column.names[type]);
			fprintf(page, "| %d | `%s` | %s |\n", type, column.names[type], text ? text : "");
		}
	}
	fprintf(page, "\n");
	CheckDescriptionsAreUsed(kHardpointTypeTexts, column);
}

static void WriteModelValuesPage(const char* folder) {
	const char* names[64];
	FILE* page = OpenPage(folder, "model_values.md", "Model values");
	if (page == NULL) {
		return;
	}
	WriteParagraph(page, "Names that models and their settings files use for game values.");

	WriteHeading(page, "Hardpoint types");
	WriteParagraph(page, "Values for the `type` of a hardpoint in a model. The keys of `hardpoint_kinds` in "
						 "the weapon settings are the same names.");
	WriteParagraph(page, "The weapons of a craft come from its craft record, in `craft_record.lasers` and "
						 "`craft_record.warheads`. A weapon hardpoint only gives a position from which a "
						 "weapon fires. When the craft record has a free group, a weapon hardpoint also adds "
						 "its weapon to that group.");
	WriteHardpointTypes(page, ColumnOfFlightNames(AERON_FLIGHT_HARDPOINT_TYPES, names, 64));

	WriteHeading(page, "Mesh types");
	WriteParagraph(page, "Values for the `meshType` of a part in a model. `component_strength` in the model "
						 "settings uses the same names as keys.");
	WriteValueTable(page, "Number", ColumnOfFlightNames(AERON_FLIGHT_MESH_TYPES, names, 64));

	WriteHeading(page, "Explosion flags");
	WriteParagraph(page, "Values for the `explosion` list of a part.");
	WriteNameList(page, ColumnOfFlightNames(AERON_FLIGHT_EXPLOSION_FLAGS, names, 64), kExplosionFlagTexts,
				  "Meaning");

	WriteHeading(page, "Cockpit screen sources");
	WriteParagraph(page, "Values for the `source` of a cockpit screen rectangle in a model. Each name is a "
						 "cockpit display. The rectangle shows that display as the game draws it.");
	WriteNameList(page, ColumnOfNameTable(&g_xwaScreenSourceNames), NULL, NULL);

	WriteHeading(page, "Cockpit screen layers");
	WriteParagraph(page,
				   "Values for the `layers` list of a cockpit screen rectangle: the parts of the display "
				   "that the rectangle shows.");
	WriteNameList(page, ColumnOfFlightNames(AERON_FLIGHT_SCREEN_LAYERS, names, 64), kScreenLayerTexts,
				  "Meaning");

	WriteHeading(page, "HUD areas");
	WriteParagraph(page,
				   "Values for the `hide` list of a cockpit screen. The game doesn't draw these areas of "
				   "the 2D HUD while the screen is visible.");
	WriteNameList(page, ColumnOfNameTable(&g_xwaHudAreaNames), NULL, NULL);

	WriteHeading(page, "Reticle slots");
	WriteParagraph(page, "The parts of the reticle that the `hud.reticle` group of a model settings file can "
						 "change. Each value is the number of an image of image group 12000, counted from 1. "
						 "The threat lights are written as nested maps, such as "
						 "`threats: {laser_mode: {turret: 135}}`.");
	WriteNameList(page, ColumnOfNameTable(&g_xwaReticleSlotNames), kReticleSlotTexts, "Meaning");
	ClosePage(page);
}

/* ---- States and tags ------------------------------------------------- */

static const NameText kFlipbookStateTexts[] = {
	{ "threat.laser", "A craft is firing lasers at the player." },
	{ "threat.turret", "An enemy turret is aimed at the player." },
	{ "threat.beam", "An enemy craft aims its beam at the player." },
	{ "threat.missile_locking", "An enemy is getting a warhead lock on the player. The state blinks, so the "
								"flipbook starts again at each blink." },
	{ "threat.missile_locked", "An enemy has a warhead lock on the player." },
	{ "cannon.1", "Cannon 1 fires next." },
	{ "cannon.2", "Cannon 2 fires next." },
	{ "cannon.3", "Cannon 3 fires next." },
	{ "cannon.4", "Cannon 4 fires next." },
	{ "cannon.5", "Cannon 5 fires next." },
	{ "cannon.6", "Cannon 6 fires next." },
	{ "cannon.7", "Cannon 7 fires next." },
	{ "cannon.8", "Cannon 8 fires next." },
	{ "broken.cmd", "The targeting computer is damaged." },
	{ "broken.laser_ion", "The lasers or ion cannons are damaged." },
	{ "broken.beam_weapon", "The beam weapon is damaged." },
	{ "broken.shields", "The shields are damaged." },
	{ "broken.throttle", "The throttle is damaged." },
	{ "broken.sensors", "A sensor is damaged." },
	{ "broken.laser_recharge", "The laser recharge is damaged." },
	{ "broken.engine_power", "The engine power is damaged." },
	{ "broken.shield_recharge", "The shield recharge is damaged." },
	{ "broken.beam_recharge", "The beam recharge is damaged." },
	{ "hull.25", "The hull is at 25% or less." },
	{ "hull.50", "The hull is at 50% or less, and above 25%." },
	{ "hull.75", "The hull is at 75% or less, and above 50%." },
	{ "laser_lock", "The target is in laser range." },
	{ "warhead.locking", "The player is getting a warhead lock." },
	{ "warhead.locked", "The player has a warhead lock." },
	{ "hangar", "The player is in a hangar." },
	{ "target", "The player has a target." },
	{ "always", "Always on. Put it last, as the flipbook to play when no other state is on." },
	{ NULL, NULL },
};

static const NameText kPartTagTexts[] = {
	{ "platform.hidden_by_beam", "Hidden when the flight group has a beam weapon." },
	{ "platform.hidden_by_jamming_decoy", "Hidden when the flight group has a jamming or decoy beam." },
	{ "hangarcrane.arm", "The arm that the hangar scene moves." },
	{ "hangardroid.nod", "The part that nods in the hangar scene." },
	{ "hangardroid2.swing", "The part that swings in the hangar scene." },
	{ "hangardroid2.spin", "The part that spins in the hangar scene." },
	{ "workdroid1.spin", "The part that spins in the hangar scene." },
	{ "workdroid1.turn", "The part that turns in the hangar scene." },
	{ "workdroid1.nod.1", "The first part that nods in the hangar scene." },
	{ "workdroid1.nod.2", "The second part that nods in the hangar scene." },
	{ "accelring.core", "The core that the player destroys in the proving grounds." },
	{ "compactor.door.1", "The first door." },
	{ "compactor.door.2", "The second door. It decides when the compactor is fully open or closed." },
	{ "compactor.door.3", "The third door." },
	{ "compactor.door.4", "The fourth door." },
	{ "centrifuge.spin.1", "The spinning part of the first mechanism." },
	{ "centrifuge.spin.2", "The spinning part of the second mechanism." },
	{ "centrifuge.spin.3", "The spinning part of the third mechanism." },
	{ "centrifuge.door.1", "The door of the first mechanism." },
	{ "centrifuge.door.2", "The door of the second mechanism." },
	{ "centrifuge.door.3", "The door of the third mechanism." },
	{ "smeltingroom.turret.1", "The turret that fires first, from weapon hardpoint 1." },
	{ "smeltingroom.turret.2", "The turret that fires second, from weapon hardpoint 2." },
	{ "smeltingroom.turret.3", "The turret that fires third, from weapon hardpoint 3." },
	{ "smeltingroom.turret.4", "The turret that fires fourth, from weapon hardpoint 4." },
	{ "smeltingroom.turret.5", "The turret that fires fifth, from weapon hardpoint 5." },
	{ "smeltingroom.turret.6", "The turret that fires sixth, from weapon hardpoint 6." },
	{ "smeltingroom.power.1", "Destroying it stops turrets 1 to 3." },
	{ "smeltingroom.power.2", "Destroying it stops turrets 4 to 6." },
	{ "smeltingroom.target", "The only part that the proving grounds turbolaser can hit." },
	{ "dsreactorcore.regenerator", "Must be destroyed before the reactor can be damaged." },
	{ "dsreactorcore.reactor", "Destroying it starts the destruction of the Death Star." },
	{ NULL, NULL },
};

static void WriteFlipbookStates(FILE* page) {
	const char* names[64];
	NameColumn column = { names, 0, 0 };
	while (column.count < 64 && Aeron_GltfAnimStateName((uint32_t)column.count) != NULL) {
		names[column.count] = Aeron_GltfAnimStateName((uint32_t)column.count);
		++column.count;
	}
	WriteNameList(page, column, kFlipbookStateTexts, "On when");
}

static void WritePartTags(FILE* page) {
	const char* tags[XWA_MODEL_ROLE_COUNT];
	fprintf(page, "| Tag | Models | Parts | Meaning |\n|---|---|---|---|\n");
	for (int role = 0; role < XWA_MODEL_ROLE_COUNT; ++role) {
		const char* tag = XwaModelRole_Tag((XwaModelRole)role);
		const char* const* models = XwaModelRole_Models((XwaModelRole)role);
		const char* text = DescriptionOf(kPartTagTexts, tag);
		tags[role] = tag;
		fprintf(page, "| `%s` | ", tag);
		for (int i = 0; models[i] != NULL; ++i) {
			fprintf(page, "%s`%s`", i > 0 ? ", " : "", models[i]);
		}
		fprintf(page, " | %s | %s |\n", XwaModelRole_IsSet((XwaModelRole)role) ? "any number" : "one",
				text ? text : "");
	}
	fprintf(page, "\n");
	const NameColumn column = { tags, 0, XWA_MODEL_ROLE_COUNT };
	CheckDescriptionsAreUsed(kPartTagTexts, column);
}

static void WriteStatesAndTagsPage(const char* folder) {
	FILE* page = OpenPage(folder, "states_and_tags.md", "States and tags");
	if (page == NULL) {
		return;
	}
	WriteHeading(page, "Flipbook states");
	WriteParagraph(page,
				   "Values for the `state` of a flipbook in a model's material. The states are those "
				   "of the player's craft, so most flipbooks only play on the player's craft and cockpit. "
				   "A flipbook with `always` plays on every object.");
	WriteParagraph(page,
				   "A material plays the first flipbook in its list whose state is on. When no state is "
				   "on, it shows its normal texture. A flipbook starts again from its first frame each "
				   "time its state comes on.");
	WriteParagraph(page,
				   "`broken.cmd`, `broken.laser_ion`, `broken.throttle`, `broken.sensors`, "
				   "`broken.laser_recharge` and `broken.engine_power` are also on for a craft that doesn't "
				   "have the system.");
	WriteFlipbookStates(page);

	WriteHeading(page, "Part tags");
	WriteParagraph(page,
				   "In a few models, the game moves or hides some parts, or reacts when they are "
				   "destroyed. A model that replaces one of these models marks these parts with the tags "
				   "below, in the `tags` of the part. **Parts** says how many parts can have the tag.");
	WriteParagraph(page, "When no part has a tag that marks one part, the game uses the part with the same "
						 "number as in the original model. Parts are numbered in the order of the model's "
						 "children.");
	WritePartTags(page);

	WriteHeading(page, "Cockpit control tags");
	WriteParagraph(page,
				   "A control is a part of a cockpit model that the game moves, such as the flight stick. "
				   "Its `tags` say which value moves it. The game uses the first tag of the control that "
				   "it knows. A control without one of these tags doesn't move. Only the controls of the "
				   "player's cockpit move.");
	fprintf(page, "| Tag | Value |\n|---|---|\n"
				  "| `cockpit.stick.x` | The stick from left to right, -1 to 1. |\n"
				  "| `cockpit.stick.y` | The stick from forward to back, -1 to 1. |\n"
				  "| `cockpit.stick.roll` | The roll, -1 to 1. |\n"
				  "| `cockpit.throttle` | The throttle, 0 to 1. |\n"
				  "| `cockpit.hyperspace` | Goes from 0 to 1 when the craft enters hyperspace, stays at 1 in "
				  "hyperspace, and goes back to 0 when it leaves. |\n\n");

	WriteHeading(page, "Material and light tags");
	WriteParagraph(page,
				   "Material and light tags have no fixed names. The `surfaces` group of the model "
				   "settings file names them, and hides the materials and lights with a tag while the "
				   "craft is disabled or slow. For example, a tag `engine_glow` on the engine materials "
				   "can hide them when the craft stops. A model can use up to 32 different material and "
				   "light tags.");
	ClosePage(page);
}

/* ---- Sound banks ----------------------------------------------------- */

static const NameText kSoundBankTexts[] = {
	{ "canopy_opening", "When the player's craft stops in the hangar after it lands." },
	{ "engine_slowing", "When the player's craft has turned around in the hangar after it lands, and when "
						"the hangar shuttle lands." },
	{ "engine_wash", "While the player's craft is in the engine wash of another craft. The craft that makes "
					 "the wash picks the sound." },
	{ "fly_by", "When a craft passes close to the player's craft. The passing craft picks the sound. Also "
				"when the player's craft launches from the hangar." },
	{ "hangar_ambient", "Now and then in the hangar, chosen at random. The command ship of the hangar picks "
						"the list." },
	{ "hyper_abort", "When the player's craft leaves hyperspace in a region that the mission marks as "
					 "interdicted." },
	{ "hyper_end", "When the player's craft leaves hyperspace." },
	{ "hyper_start", "When the player's craft starts a hyperspace jump." },
	{ "hyper_zoom", "A moment after `hyper_start`, when the player's craft enters hyperspace." },
	{ "interior", "The engine sound in the player's cockpit." },
	{ "s_foil", "When the S-foils or the landing gear of the player's craft open or close." },
	{ "shuttle_blast_off", "When the hangar shuttle speeds out of the hangar." },
	{ "shuttle_shut_down", "When the hangar shuttle lands and stops." },
	{ "shuttle_take_off", "When the hangar shuttle starts to leave." },
	{ "sounds", "Single sounds, listed below." },
	{ "starship_ambient",
	  "While the player's craft is close to another craft. Each craft plays the sound of "
	  "its own object type. Needs the `starship_ambient_sounds` rule of `flight/flight.yaml`." },
	{ "take_off", "When the player's craft launches from the hangar, and when the hangar shuttle starts to "
				  "leave." },
	{ "weapon", "When a craft fires. The firing craft picks the entry, then the weapon sound group of the "
				"shot." },
	{ "weapon_explosion", "When a shot hits or an object explodes. The craft that fired the shot, or the "
						  "object that explodes, picks the list." },
	{ "weapon_range",
	  "When a craft fires a weapon that its model's `sound.weapon_ranges` setting lists. Each "
	  "shot plays one of the listed sounds, chosen at random." },
	{ NULL, NULL },
};

static const NameText kBankSoundTexts[] = {
	{ "player_targeted", "When a warhead is fired at the player's craft." },
	{ "map_enter", "When the player opens the map and the map selects a target." },
	{ "map_exit", "When the view comes back to the player's craft, such as when the player closes the "
				  "map." },
	{ "turret_switch", "When the player changes gunner seat." },
	{ "target_select", "When the player selects a new target." },
	{ NULL, NULL },
};

static void WriteSoundBanksPage(const char* folder) {
	const char* banks[XWA_SOUND_BANK_COUNT];
	const char* sounds[XWA_SOUND_BANK_SOUND_COUNT];
	FILE* page = OpenPage(folder, "sound_banks.md", "Sound banks");
	if (page == NULL) {
		return;
	}
	WriteParagraph(page,
				   "The banks of `flight/sounds.yaml`. Most banks have one sound for each object type. "
				   "**Entries** gives the keys of a bank's entries. An object type that a bank doesn't "
				   "list plays the sound of the original game, unless the bank sets `unlisted: silent`.");
	WriteParagraph(page, "For the cockpit, fly-by, engine wash, take-off, hyperspace and weapon sounds, a "
						 "model's `sound` settings take priority over the banks.");
	fprintf(page, "| Bank | Entries | When it plays |\n|---|---|---|\n");
	for (int bank = 0; bank < XWA_SOUND_BANK_COUNT; ++bank) {
		const char* key = XwaSoundBanks_Key((XwaSoundBank)bank);
		const char* text = DescriptionOf(kSoundBankTexts, key);
		banks[bank] = key;
		fprintf(page, "| `%s` | %s", key, XwaSoundBanks_EntryKeys((XwaSoundBank)bank));
		if (XwaSoundBanks_ListMax((XwaSoundBank)bank) > 0) {
			fprintf(page, " of up to %d sounds", XwaSoundBanks_ListMax((XwaSoundBank)bank));
		}
		fprintf(page, " | %s |\n", text ? text : "");
	}
	fprintf(page, "\n");
	const NameColumn bank_column = { banks, 0, XWA_SOUND_BANK_COUNT };
	CheckDescriptionsAreUsed(kSoundBankTexts, bank_column);

	WriteHeading(page, "Sounds of the `sounds` bank");
	WriteParagraph(page, "The `sounds` bank has these entries, by name.");
	for (int sound = 0; sound < XWA_SOUND_BANK_SOUND_COUNT; ++sound) {
		sounds[sound] = XwaSoundBanks_SoundName((XwaSoundBankSound)sound);
	}
	const NameColumn sound_column = { sounds, 0, XWA_SOUND_BANK_SOUND_COUNT };
	WriteNameList(page, sound_column, kBankSoundTexts, "When it plays");
	ClosePage(page);
}

/* ---- Frontend screens ------------------------------------------------ */

static void WriteFrontendScreensPage(const char* folder) {
	const char* names[256];
	FILE* page = OpenPage(folder, "frontend_screens.md", "Frontend screens");
	if (page == NULL) {
		return;
	}
	WriteParagraph(page, "The screens and views that can have a layout in the `layouts` table of "
						 "`frontend/frontend.yaml`. A view is a part of a screen. Its name starts with the "
						 "screen's name and a dot. A view without a layout uses the layout of its screen.");
	NameColumn column = { names, 0, XwaFrontendScreens_Count() };
	if (column.count > 256) {
		column.count = 256;
	}
	for (int screen = 0; screen < column.count; ++screen) {
		names[screen] = XwaFrontendScreens_Name(screen);
	}
	WriteNameList(page, column, NULL, NULL);
	ClosePage(page);
}

/* ---- The add-on lists ------------------------------------------------ */

/* Prints the names in Aeron's `list` as a Python tuple named `variable`, in
   lines of at most 100 characters. */
static void WriteAddonList(const char* variable, AeronFlightNameList list) {
	enum { LINE_MAX = 100 };
	size_t column = 4;

	printf("%s = (\n   ", variable);
	for (uint32_t value = 0; value < Aeron_FlightNameCount(list); ++value) {
		const char* name = Aeron_FlightName(list, value);
		const size_t width = strlen(name) + 4;
		if (column > 4 && column + width > LINE_MAX) {
			printf("\n   ");
			column = 4;
		}
		printf(" \"%s\",", name);
		column += width;
	}
	printf("\n)\n");
}

static void WriteAddonLists(void) {
	WriteAddonList("MESH_TYPE_IDS", AERON_FLIGHT_MESH_TYPES);
	printf("\n");
	WriteAddonList("HARDPOINT_TYPE_IDS", AERON_FLIGHT_HARDPOINT_TYPES);
	printf("\n");
	WriteAddonList("EXPLOSION_IDS", AERON_FLIGHT_EXPLOSION_FLAGS);
}

int main(int argc, char** argv) {
	const char* game_folder = NULL;
	const char* out_folder = NULL;
	static GameData data;

	if (argc == 2 && strcmp(argv[1], "--addon") == 0) {
		WriteAddonLists();
		return 0;
	}
	for (int i = 1; i + 1 < argc; i += 2) {
		if (strcmp(argv[i], "--game-data") == 0) {
			game_folder = argv[i + 1];
		} else if (strcmp(argv[i], "--out") == 0) {
			out_folder = argv[i + 1];
		}
	}
	if (argc != 5 || game_folder == NULL || out_folder == NULL) {
		fprintf(stderr, "usage: xwa_mod_tables --game-data <game folder> --out <folder>\n"
						"       xwa_mod_tables --addon\n");
		return 2;
	}
	if (!ReadGameData(game_folder, &data)) {
		return 1;
	}
	WriteObjectTypesPage(out_folder, &data);
	WriteCraftTypesPage(out_folder, &data);
	WriteWeaponsPage(out_folder);
	WriteModelValuesPage(out_folder);
	WriteStatesAndTagsPage(out_folder);
	WriteSoundBanksPage(out_folder);
	WriteFrontendScreensPage(out_folder);
	return s_failed ? 1 : 0;
}
