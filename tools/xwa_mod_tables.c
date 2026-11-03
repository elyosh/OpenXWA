/* Prints Markdown tables of the names that mod files use. The names come
 * from the game's own tables:
 *
 *   xwa_mod_tables > tables.md
 *
 * With --addon, it prints the Blender add-on's name lists as Python instead.
 * Paste them into the add-on when the lists change. */

#include "aeron/asset/flight_names.h"
#include "aeron/scene/gltf_mesh.h"
#include "xwa_runtime/audio/sound_banks.h"
#include "xwa_runtime/frontend/frontend_screens.h"
#include "xwa_runtime/models/model_roles.h"
#include "xwa_runtime/models/names.h"

#include <stdio.h>
#include <string.h>

/* Writes `text` as a Markdown paragraph. */
static void WriteParagraph(const char* text) { printf("%s\n\n", text); }

/* Writes a section with a table of the values in `table` and their names.
   `column` is the heading of the value column, such as "Number" or "Bit". */
static void WriteNameTable(const char* title, const char* text, const char* column,
						   const XwaNameTable* table) {
	printf("## %s\n\n", title);
	WriteParagraph(text);
	printf("| %s | Name |\n|---:|---|\n", column);
	for (int value = table->first; value < table->first + table->count; ++value) {
		const char* name = XwaNames_StockName(table, value);
		if (name != NULL) {
			printf("| %d | `%s` |\n", value, name);
		}
	}
	printf("\n");
}

/* Writes a section with a table of the values in Aeron's `list` and their
   names. */
static void WriteFlightNameTable(const char* title, const char* text, const char* column,
								 AeronFlightNameList list) {
	printf("## %s\n\n", title);
	WriteParagraph(text);
	printf("| %s | Name |\n|---:|---|\n", column);
	for (uint32_t value = 0; value < Aeron_FlightNameCount(list); ++value) {
		printf("| %u | `%s` |\n", value, Aeron_FlightName(list, value));
	}
	printf("\n");
}

static void WriteObjectTypeFlags(void) {
	printf("## Object type flags\n\n");
	WriteParagraph("The `flags` list of an object type file gives the flags that are on. The settable "
				   "flags that it doesn't list are off. A file can only set the flags marked as settable. "
				   "The table lists the others too, because the game sets them.");
	printf("| Bit | Name | Settable |\n|---:|---|---|\n");
	for (int bit = 0; bit < g_xwaObjectTypeFlagNames.count; ++bit) {
		const char* name = XwaNames_StockName(&g_xwaObjectTypeFlagNames, bit);
		if (name != NULL) {
			printf("| %d | `%s` | %s |\n", bit, name,
				   (XWA_OBJECT_TYPE_SETTABLE_FLAGS >> bit) & 1 ? "yes" : "no");
		}
	}
	printf("\n");
}

static void WriteShipListCategories(void) {
	printf("## Ship list categories\n\n");
	WriteParagraph("Values for `ship_list.category` in craft type files. The second column shows the text "
				   "that the game displays.");
	printf("| Name | Shown as |\n|---|---|\n");
	for (int category = 0; category < g_xwaShipListCategoryNames.count; ++category) {
		printf("| `%s` | %s |\n", XwaNames_StockName(&g_xwaShipListCategoryNames, category),
			   XwaNames_StockName(&g_xwaShipListCategoryTexts, category));
	}
	printf("\n");
}

static void WriteAnimStates(void) {
	printf("## Flipbook states\n\n");
	WriteParagraph("Values for a flipbook's `state` in a model's `AERON_flight_model` extension. The "
				   "flipbook plays while the player is in this state. `always` plays all the time.");
	printf("| Value | Name |\n|---:|---|\n");
	for (uint32_t state = 0; Aeron_GltfAnimStateName(state) != NULL; ++state) {
		printf("| %u | `%s` |\n", state, Aeron_GltfAnimStateName(state));
	}
	printf("\n");
}

static void WritePartTags(void) {
	printf("## Part tags\n\n");
	WriteParagraph("Some scenes move or hide specific parts of a model. A replacement model marks these "
				   "parts with the tags below. Some tags mark exactly one part, others any number of "
				   "parts.");
	printf("| Tag | Parts | Models |\n|---|---|---|\n");
	for (int role = 0; role < XWA_MODEL_ROLE_COUNT; ++role) {
		printf("| `%s` | %s | ", XwaModelRole_Tag((XwaModelRole)role),
			   XwaModelRole_IsSet((XwaModelRole)role) ? "any number" : "one");
		const char* const* models = XwaModelRole_Models((XwaModelRole)role);
		for (int i = 0; models[i] != NULL; ++i) {
			printf("%s`%s`", i > 0 ? ", " : "", models[i]);
		}
		printf(" |\n");
	}
	printf("\n");
}

static void WriteSoundBanks(void) {
	printf("## Sound banks\n\n");
	WriteParagraph("The banks in `flight/sounds.yaml`, and the keys of their entries.");
	printf("| Bank | Entries |\n|---|---|\n");
	for (int bank = 0; bank < XWA_SOUND_BANK_COUNT; ++bank) {
		printf("| `%s` | %s", XwaSoundBanks_Key((XwaSoundBank)bank),
			   XwaSoundBanks_EntryKeys((XwaSoundBank)bank));
		if (XwaSoundBanks_ListMax((XwaSoundBank)bank) > 0) {
			printf(" of up to %d sounds", XwaSoundBanks_ListMax((XwaSoundBank)bank));
		}
		printf(" |\n");
	}
	printf("\n");
	printf("## Sound names in the `sounds` bank\n\n");
	printf("| Name |\n|---|\n");
	for (int sound = 0; sound < XWA_SOUND_BANK_SOUND_COUNT; ++sound) {
		printf("| `%s` |\n", XwaSoundBanks_SoundName((XwaSoundBankSound)sound));
	}
	printf("\n");
}

static void WriteFrontendScreens(void) {
	printf("## Frontend screens\n\n");
	WriteParagraph("The screens and views that can have a layout in the `layouts` table of "
				   "`frontend/frontend.yaml`. A view's name starts with its screen's name and a dot.");
	printf("| Name |\n|---|\n");
	for (int screen = 0; screen < XwaFrontendScreens_Count(); ++screen) {
		printf("| `%s` |\n", XwaFrontendScreens_Name(screen));
	}
	printf("\n");
}

static void WriteReferenceTables(void) {
	printf("# Mod reference tables\n\n");
	WriteParagraph("Mod files use these names instead of numbers for many game values. The "
				   "`xwa_mod_tables` tool generates these tables from the game's own tables.");
	WriteNameTable("Object types",
				   "Object type files, craft type files, mission files (`craft` checks) and sound banks "
				   "use these names. An object type file can rename its object type with `name`. The other "
				   "files must then use the new name.",
				   "Number", &g_xwaObjectTypeNames);
	WriteNameTable("Weapon types",
				   "Weapon types are object types 280 to 307 and have the same names. Laser groups, weapon "
				   "profiles and weapon colours use them.",
				   "Number", &g_xwaWeaponTypeNames);
	WriteNameTable("Warheads",
				   "Mission files use these names for the warheads that a flight group has in its hangar. "
				   "Weapon profiles use them in `warhead_capacity_percent`.",
				   "Number", &g_xwaWarheadNames);
	WriteNameTable("Genus", "Values for `genus` in object type files.", "Number", &g_xwaGenusNames);
	WriteNameTable("Family", "Values for `family` in object type files.", "Number", &g_xwaFamilyNames);
	WriteObjectTypeFlags();
	WriteShipListCategories();
	WriteNameTable("Laser group kinds", "Values for the `kind` of a laser group in `craft_record.lasers`.",
				   "Value", &g_xwaLaserKindNames);
	WriteNameTable("Hardpoint kinds",
				   "What a hardpoint type fires. Weapon profiles use these values in `hardpoint_kinds`.",
				   "Value", &g_xwaHardpointKindNames);
	WriteNameTable("Ranks", "The keys of `weapons.fire_ratio_by_rank` in `flight/flight.yaml`.", "Value",
				   &g_xwaRankNames);
	WriteNameTable("Difficulties", "The keys of `weapons.damage_by_difficulty` in `flight/flight.yaml`.",
				   "Value", &g_xwaDifficultyNames);
	WriteNameTable("Reticle slots",
				   "The images of HUD image group 12000 that a model file can replace with `hud.reticle`. "
				   "The threat indicators are in nested maps, such as "
				   "`threats: {laser_mode: {turret: 135}}`.",
				   "Slot", &g_xwaReticleSlotNames);
	WriteNameTable("Weapon sound groups", "The keys under each object type in the `weapon` sound bank.",
				   "Value", &g_xwaSoundWeaponGroupNames);
	WriteNameTable("Cockpit screen sources",
				   "Values for the `source` of a cockpit screen rectangle in a model.", "Value",
				   &g_xwaScreenSourceNames);
	WriteNameTable("HUD areas", "The HUD areas that a cockpit screen can hide with `hide`.", "Bit",
				   &g_xwaHudAreaNames);
	WriteFlightNameTable(
		"Hardpoint types",
		"Values for a hardpoint's `type` in a model's `AERON_flight_model` extension. Weapon "
		"profiles use the same names as keys in `hardpoint_kinds`.",
		"Value", AERON_FLIGHT_HARDPOINT_TYPES);
	WriteFlightNameTable("Mesh types",
						 "Values for a part's `meshType` in a model's `AERON_flight_model` extension. "
						 "`component_strength` uses the same names as keys.",
						 "Value", AERON_FLIGHT_MESH_TYPES);
	WriteFlightNameTable("Explosion flags", "Values for the `explosion` list of a part.", "Bit",
						 AERON_FLIGHT_EXPLOSION_FLAGS);
	WriteFlightNameTable("Screen layers", "Values for the `layers` list of a cockpit screen rectangle.",
						 "Bit", AERON_FLIGHT_SCREEN_LAYERS);
	WriteAnimStates();
	WritePartTags();
	WriteSoundBanks();
	WriteFrontendScreens();
}

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
	if (argc == 2 && strcmp(argv[1], "--addon") == 0) {
		WriteAddonLists();
		return 0;
	}
	if (argc != 1) {
		fprintf(stderr, "usage: xwa_mod_tables [--addon]\n");
		return 2;
	}
	WriteReferenceTables();
	return 0;
}
