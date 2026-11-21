#include "xwa_runtime/models/weapon_stats.h"

#include "aeron/asset/flight_names.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/models/model_settings.h"
#include "xwa_runtime/models/names.h"
#include "xwa_runtime/models/units.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

enum {
	WEAPON_STATS_FIRST_WEAPON = 280,
	WEAPON_STATS_WEAPONS = 28,
	WEAPON_STATS_WARHEADS = 11,
	/* Marks a hardpoint type that keeps its stock kind. */
	WEAPON_STATS_STOCK_KIND = 0xFF,
	WEAPON_STATS_HARDPOINT_TYPES = 40,
	/* g_missionFlightGroups holds 192. */
	WEAPON_STATS_FLIGHT_GROUPS = 192,
	WEAPON_STATS_NAME_MAX = 48,
	WEAPON_STATS_RANKS = 6,
	WEAPON_STATS_DIFFICULTIES = 3,
	/* The factor sets: all, player, one for each rank, then one for each rank
	   of a player's craft. */
	WEAPON_STATS_FACTORS_ALL = 0,
	WEAPON_STATS_FACTORS_PLAYER = 1,
	WEAPON_STATS_FACTORS_RANK = 2,
	WEAPON_STATS_FACTORS_PLAYER_RANK = WEAPON_STATS_FACTORS_RANK + WEAPON_STATS_RANKS,
	WEAPON_STATS_FACTOR_SETS = WEAPON_STATS_FACTORS_PLAYER_RANK + WEAPON_STATS_RANKS,
};

/* The groups of the `weapons` section that have profiles. */
enum {
	WEAPON_STATS_GROUP_STATS,
	WEAPON_STATS_GROUP_WARHEADS,
	WEAPON_STATS_GROUP_ENERGY,
	WEAPON_STATS_GROUP_LINKING,
	WEAPON_STATS_GROUPS
};

static const char* const s_groups[WEAPON_STATS_GROUPS] = { "stats", "warhead_capacity_percent",
														   "energy_speed_percent", "linking" };
/* The keys that a flight group's entry uses to pick a profile in each group. */
static const char* const s_profileKeys[WEAPON_STATS_GROUPS] = { "weapon_profile", "warhead_profile",
																"energy_profile", "linking_profile" };

static const char* const s_stats[XWA_WEAPON_STATS] = {
	/* `duration_ms` sets both parts of the duration, so the fraction has no
	   key of its own. */
	"duration_ms",
	"",
	"score",
	"speed_mglt",
	"power",
	"power_speed_percent",
	"fire_rate_ms",
	"duration_offset_ms",
	"fire_ratio_ms",
	"range_m",
	"precise",
	"spread_mask",
	"decharge_rate",
	"recharge_rate",
	"high_power_charge",
	"side",
	"side_model",
};

/* The key of each stat in a factor set. A stat without a key has no factor. */
static const char* const s_factorStats[XWA_WEAPON_STATS] = {
	[XWA_WEAPON_STAT_SCORE] = "score",
	[XWA_WEAPON_STAT_SPEED] = "speed",
	[XWA_WEAPON_STAT_POWER] = "power",
	[XWA_WEAPON_STAT_POWER_SPEED_PERCENT] = "power_speed_percent",
	[XWA_WEAPON_STAT_FIRE_RATE] = "fire_rate",
	[XWA_WEAPON_STAT_FIRE_RATIO] = "fire_ratio",
	[XWA_WEAPON_STAT_RANGE] = "range",
	[XWA_WEAPON_STAT_DECHARGE_RATE] = "decharge_rate",
	[XWA_WEAPON_STAT_RECHARGE_RATE] = "recharge_rate",
};

static const char* const s_sideNames[3] = { "rebel", "imperial", "none" };
static const int32_t s_sideValues[3] = { XWA_WEAPON_SIDE_REBEL, XWA_WEAPON_SIDE_IMPERIAL,
										 XWA_WEAPON_SIDE_NONE };

/* The unit of each stat in the file. The reader converts these stats to the
   game's units. The stats that aren't listed are the game's own values. */
static const XwaUnit s_statUnits[XWA_WEAPON_STATS] = {
	[XWA_WEAPON_STAT_SPEED] = XWA_UNIT_MGLT,
	[XWA_WEAPON_STAT_FIRE_RATE] = XWA_UNIT_MILLISECONDS,
	[XWA_WEAPON_STAT_DURATION_OFFSET] = XWA_UNIT_MILLISECONDS,
	[XWA_WEAPON_STAT_FIRE_RATIO] = XWA_UNIT_MILLISECONDS,
	[XWA_WEAPON_STAT_RANGE] = XWA_UNIT_METRES,
};

/* The forms of a stat: plain, for the player, for one difficulty, or for
   both. */
enum {
	WEAPON_STATS_FORM_PLAIN,
	WEAPON_STATS_FORM_PLAYER,
	WEAPON_STATS_FORM_EASY,
	WEAPON_STATS_FORM_PLAYER_EASY = WEAPON_STATS_FORM_EASY + WEAPON_STATS_DIFFICULTIES,
	WEAPON_STATS_FORMS = WEAPON_STATS_FORM_PLAYER_EASY + WEAPON_STATS_DIFFICULTIES
};

static const char* const s_forms[WEAPON_STATS_FORMS] = {
	"", "player", "easy", "medium", "hard", "player_easy", "player_medium", "player_hard"
};

static const char* const s_energyKeys[2] = { "all", "player" };

static const char* const s_linkingRuleNames[XWA_WEAPON_LINKING_RULES] = {
	"prevent_ai_lasers_ion_when_disabling",
	"prevent_ai_ion_when_not_disabling",
	"prevent_player_linked_lasers_ion",
};

typedef struct WeaponStatsForm {
	uint32_t set; /* one bit for each stat that is set */
	int32_t value[XWA_WEAPON_STATS];
} WeaponStatsForm;

/* One profile of a model file or of flight/flight.yaml. Each group has its own
   profiles, so `groups` has one bit for each group that has a profile with this
   name. */
typedef struct WeaponStatsProfile {
	char name[WEAPON_STATS_NAME_MAX]; /* "" for `default` */
	uint8_t groups;
	WeaponStatsForm weapons[WEAPON_STATS_WEAPONS][WEAPON_STATS_FORMS];
	int32_t warheadPercent[WEAPON_STATS_WARHEADS]; /* -1: not set */
	int32_t energy[2];                             /* all, player; -1: not set */
	int32_t laserLink;                             /* -1: not set */
	int8_t linking[XWA_WEAPON_LINKING_RULES];      /* -1: not set */
} WeaponStatsProfile;

/* One set of `factors` in a model file. */
typedef struct WeaponStatsFactors {
	int present;
	float value[XWA_WEAPON_STATS]; /* 1 for a stat that the set doesn't list */
} WeaponStatsFactors;

typedef struct WeaponStatsSet {
	WeaponStatsProfile* profiles;
	int profileCount;
	int hasHardpointKinds;
	int hasFactors;
	WeaponStatsFactors factors[WEAPON_STATS_FACTOR_SETS];
	/* The kind of each hardpoint type, or WEAPON_STATS_STOCK_KIND to keep the
	   stock kind. */
	uint8_t hardpointKinds[WEAPON_STATS_HARDPOINT_TYPES];
} WeaponStatsSet;

static WeaponStatsSet s_defaults;
static WeaponStatsSet s_models[OBJ_Count];
static char s_flightGroupProfiles[WEAPON_STATS_FLIGHT_GROUPS][WEAPON_STATS_GROUPS][WEAPON_STATS_NAME_MAX];
/* Bit `group` is set after the log says that the flight group's profile for
   that group doesn't exist. */
static uint8_t s_missingProfileLogged[WEAPON_STATS_FLIGHT_GROUPS];
static int s_hasRankFactors;
static float s_rankFactors[WEAPON_STATS_RANKS];
static int s_hasDifficultyFactors;
static float s_difficultyFactors[WEAPON_STATS_DIFFICULTIES];
static int s_limitLinkedWarheadFireRate;
static int8_t s_linkingRules[XWA_WEAPON_LINKING_RULES];

static void WeaponStats_FreeSet(WeaponStatsSet* set) {
	free(set->profiles);
	memset(set, 0, sizeof *set);
	memset(set->hardpointKinds, WEAPON_STATS_STOCK_KIND, sizeof set->hardpointKinds);
}

/* Copies `name` into `out` in lower case. The files call the values without a
   profile "default", and this module calls them "". Returns 0 when the name is
   too long. */
static int WeaponStats_ProfileName(const char* name, char out[WEAPON_STATS_NAME_MAX]) {
	size_t length = strlen(name);
	size_t index;

	if (length >= WEAPON_STATS_NAME_MAX) {
		return 0;
	}
	for (index = 0; index <= length; ++index) {
		out[index] = (char)tolower((unsigned char)name[index]);
	}
	if (strcmp(out, "default") == 0) {
		out[0] = '\0';
	}
	return 1;
}

/* Returns the profile `name` of `set`, and adds it if it is missing. Returns
   NULL when out of memory. */
static WeaponStatsProfile* WeaponStats_AddProfile(WeaponStatsSet* set, const char* name) {
	WeaponStatsProfile* profiles;
	WeaponStatsProfile* profile;
	int index;

	for (index = 0; index < set->profileCount; ++index) {
		if (strcmp(set->profiles[index].name, name) == 0) {
			return &set->profiles[index];
		}
	}
	profiles =
		(WeaponStatsProfile*)realloc(set->profiles, (size_t)(set->profileCount + 1) * sizeof *profiles);
	if (profiles == NULL) {
		return NULL;
	}
	set->profiles = profiles;
	profile = &profiles[set->profileCount++];
	memset(profile, 0, sizeof *profile);
	strcpy(profile->name, name);
	memset(profile->warheadPercent, 0xff, sizeof profile->warheadPercent);
	memset(profile->energy, 0xff, sizeof profile->energy);
	profile->laserLink = -1;
	memset(profile->linking, 0xff, sizeof profile->linking);
	return profile;
}

/* Reads an integer into `out`. If the node isn't an integer, logs it and
   returns 0. */
static int WeaponStats_ReadInt(const char* path, const char* where, const AeronConfigNode* node,
							   int32_t* out) {
	const int64_t value = AeronConfigNode_Int(node, 0);

	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < INT32_MIN || value > INT32_MAX) {
		Aeron_LogWarn("xwa.models", "%s: %s must be a number. It is ignored", path, where);
		return 0;
	}
	*out = (int32_t)value;
	return 1;
}

static int WeaponStats_ReadBool(const char* path, const char* where, const AeronConfigNode* node,
								int32_t* out) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogWarn("xwa.models", "%s: %s must be true or false. It is ignored", path, where);
		return 0;
	}
	*out = AeronConfigNode_Bool(node, 0) != 0;
	return 1;
}

/* Returns the index of `key` in `names`, or -1. */
static int WeaponStats_Find(const char* key, const char* const* names, int count) {
	int index;

	for (index = 0; index < count; ++index) {
		if (strcmp(key, names[index]) == 0) {
			return index;
		}
	}
	return -1;
}

/* Reads the stats in `node` into `form`. When `forms` is set, `node` can also
   have forms, which the caller reads. */
static void WeaponStats_ReadForm(const char* path, const char* where, const AeronConfigNode* node, int forms,
								 WeaponStatsForm* form) {
	size_t index;

	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
		const int stat = WeaponStats_Find(key, s_stats, XWA_WEAPON_STATS);
		char name[160];
		int32_t read;

		if (stat < 0 || stat == XWA_WEAPON_STAT_DURATION_FRACTION) {
			if (!forms || WeaponStats_Find(key, s_forms + 1, WEAPON_STATS_FORMS - 1) < 0) {
				Aeron_LogWarn("xwa.models", "%s: %s.%s isn't a weapon stat. It is ignored", path, where, key);
			}
			continue;
		}
		snprintf(name, sizeof name, "%s.%s", where, key);
		if (stat == XWA_WEAPON_STAT_DURATION_SECONDS) {
			int32_t ticks;

			/* The game counts 236 ticks for each second, adds the fraction
			   rounded to a whole tick, and keeps the result in 16 bits. */
			if (!XwaUnits_Read(value, XWA_UNIT_MILLISECONDS, 0, UINT16_MAX, &ticks)) {
				Aeron_LogWarn("xwa.models",
							  "%s: %s must be a number of milliseconds from 0 to %d. It is ignored", path,
							  name, UINT16_MAX * 4);
				continue;
			}
			form->value[XWA_WEAPON_STAT_DURATION_SECONDS] = ticks / 236;
			form->value[XWA_WEAPON_STAT_DURATION_FRACTION] = (int32_t)(((ticks % 236) * 65536 + 118) / 236);
			form->set |= (1u << XWA_WEAPON_STAT_DURATION_SECONDS) | (1u << XWA_WEAPON_STAT_DURATION_FRACTION);
		} else if (s_statUnits[stat] != XWA_UNIT_NONE) {
			if (!XwaUnits_Read(value, s_statUnits[stat], INT32_MIN, INT32_MAX, &read)) {
				Aeron_LogWarn("xwa.models", "%s: %s must be a number of %s. It is ignored", path, name,
							  XwaUnits_Name(s_statUnits[stat]));
				continue;
			}
			form->value[stat] = read;
			form->set |= 1u << stat;
		} else if (stat == XWA_WEAPON_STAT_SIDE) {
			const int side = WeaponStats_Find(AeronConfigNode_String(value, ""), s_sideNames, 3);

			if (side < 0) {
				Aeron_LogWarn("xwa.models", "%s: %s must be rebel, imperial or none. It is ignored", path,
							  name);
				continue;
			}
			form->value[stat] = s_sideValues[side];
			form->set |= 1u << stat;
		} else if (stat == XWA_WEAPON_STAT_SIDE_MODEL) {
			int weapon;

			if (XwaNames_Read(&g_xwaWeaponTypeNames, path, name, value, &weapon)) {
				form->value[stat] = weapon;
				form->set |= 1u << stat;
			}
		} else if (stat == XWA_WEAPON_STAT_PRECISE ? WeaponStats_ReadBool(path, name, value, &read)
												   : WeaponStats_ReadInt(path, name, value, &read)) {
			form->value[stat] = read;
			form->set |= 1u << stat;
		}
	}
}

/* Reads the stats of one weapon type in a `stats` profile. */
static void WeaponStats_ReadWeapon(const char* path, const char* where, const AeronConfigNode* node,
								   WeaponStatsForm forms[WEAPON_STATS_FORMS]) {
	int form;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: %s isn't a map. It is ignored", path, where);
		return;
	}
	WeaponStats_ReadForm(path, where, node, 1, &forms[WEAPON_STATS_FORM_PLAIN]);
	for (form = WEAPON_STATS_FORM_PLAIN + 1; form < WEAPON_STATS_FORMS; ++form) {
		const AeronConfigNode* formNode = AeronConfigNode_MapGet(node, s_forms[form]);
		char name[128];

		if (formNode == NULL) {
			continue;
		}
		snprintf(name, sizeof name, "%s.%s", where, s_forms[form]);
		if (AeronConfigNode_Type(formNode) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s: %s isn't a map. It is ignored", path, name);
			continue;
		}
		WeaponStats_ReadForm(path, name, formNode, 0, &forms[form]);
	}
}

/* Looks up the map key `key` in `table` and returns its value. If the name
   isn't in the table, or its value is below `first`, logs a warning and
   returns -1. */
static int WeaponStats_ReadNameKey(const char* path, const char* where, const AeronConfigNode* node,
								   const char* key, const XwaNameTable* table) {
	const int value = XwaNames_Find(table, key);

	if (value < 0) {
		XwaNames_LogUnknown(table, path, AeronConfigNode_Line(node), where, key);
		return -1;
	}
	return value;
}

/* Reads `node`, a profile of group `group`, into `profile`. */
static void WeaponStats_ReadProfile(const char* path, const char* where, int group,
									const AeronConfigNode* node, WeaponStatsProfile* profile) {
	size_t index;

	profile->groups |= (uint8_t)(1u << group);
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
		char name[128];
		int at;

		snprintf(name, sizeof name, "%s.%s", where, key);
		switch (group) {
			case WEAPON_STATS_GROUP_STATS:
				if ((at = WeaponStats_ReadNameKey(path, where, value, key, &g_xwaWeaponTypeNames)) >= 0) {
					WeaponStats_ReadWeapon(path, name, value,
										   profile->weapons[at - WEAPON_STATS_FIRST_WEAPON]);
				}
				break;
			case WEAPON_STATS_GROUP_WARHEADS:
				if ((at = WeaponStats_ReadNameKey(path, where, value, key, &g_xwaWarheadNames)) >= 0 &&
					WeaponStats_ReadInt(path, name, value, &profile->warheadPercent[at]) &&
					profile->warheadPercent[at] < 0) {
					Aeron_LogWarn("xwa.models", "%s: %s must be 0 or more. It is ignored", path, name);
					profile->warheadPercent[at] = -1;
				}
				break;
			case WEAPON_STATS_GROUP_ENERGY:
				if ((at = WeaponStats_Find(key, s_energyKeys, 2)) < 0) {
					Aeron_LogWarn("xwa.models", "%s: %s isn't 'all' or 'player'. It is ignored", path, name);
				} else if (!XwaUnits_Read(value, XWA_UNIT_PERCENT_Q16, 0, 0x10000, &profile->energy[at])) {
					Aeron_LogWarn("xwa.models", "%s: %s must be a percent from 0 to 100. It is ignored", path,
								  name);
				}
				break;
			default: {
				int32_t read;

				if (strcmp(key, "laser_link") == 0) {
					if (WeaponStats_ReadInt(path, name, value, &read)) {
						if (read >= 1 && read <= 3) {
							profile->laserLink = read;
						} else {
							Aeron_LogWarn("xwa.models", "%s: %s must be 1, 2 or 3. It is ignored", path,
										  name);
						}
					}
				} else if ((at = WeaponStats_Find(key, s_linkingRuleNames, XWA_WEAPON_LINKING_RULES)) < 0) {
					Aeron_LogWarn("xwa.models", "%s: %s isn't a linking key. It is ignored", path, name);
				} else if (WeaponStats_ReadBool(path, name, value, &read)) {
					profile->linking[at] = (int8_t)read;
				}
				break;
			}
		}
	}
}

/* Reads the profiles and the hardpoint kinds of `weapons`, a `weapons` section
   or group, into `set`. */
static void WeaponStats_ReadSet(const char* path, const AeronConfigNode* weapons, WeaponStatsSet* set) {
	const AeronConfigNode* kinds = AeronConfigNode_MapGet(weapons, "hardpoint_kinds");
	int group;

	memset(set->hardpointKinds, WEAPON_STATS_STOCK_KIND, sizeof set->hardpointKinds);
	for (group = 0; group < WEAPON_STATS_GROUPS; ++group) {
		const AeronConfigNode* node = AeronConfigNode_MapGet(weapons, s_groups[group]);
		size_t index;

		if (node == NULL) {
			continue;
		}
		if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s: weapons.%s isn't a map. It is ignored", path, s_groups[group]);
			continue;
		}
		for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
			const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
			char name[WEAPON_STATS_NAME_MAX];
			char where[128];
			WeaponStatsProfile* profile;

			snprintf(where, sizeof where, "weapons.%s.%s", s_groups[group],
					 AeronConfigNode_MapKeyAt(node, index));
			if (!WeaponStats_ProfileName(AeronConfigNode_MapKeyAt(node, index), name) ||
				AeronConfigNode_Type(value) != AERON_CONFIG_MAP) {
				Aeron_LogWarn("xwa.models",
							  "%s: %s must be a map with a name of up to %d characters. It is ignored", path,
							  where, WEAPON_STATS_NAME_MAX - 1);
				continue;
			}
			if ((profile = WeaponStats_AddProfile(set, name)) == NULL) {
				Aeron_LogError("xwa.models", "%s: out of memory", path);
				return;
			}
			WeaponStats_ReadProfile(path, where, group, value, profile);
		}
	}
	if (kinds != NULL) {
		size_t index;

		if (AeronConfigNode_Type(kinds) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models",
						  "%s: weapons.hardpoint_kinds must map hardpoint types to kinds. It is ignored",
						  path);
			return;
		}
		for (index = 0; index < AeronConfigNode_MapCount(kinds); ++index) {
			const char* key = AeronConfigNode_MapKeyAt(kinds, index);
			const AeronConfigNode* node = AeronConfigNode_MapValueAt(kinds, index);
			const int type = Aeron_FlightNameFind(AERON_FLIGHT_HARDPOINT_TYPES, key);
			int kind;

			if (type < 0) {
				Aeron_LogWarn("xwa.models",
							  "%s:%d: weapons.hardpoint_kinds.%s isn't a hardpoint type. It is ignored", path,
							  AeronConfigNode_Line(node), key);
				continue;
			}
			if (XwaNames_Read(&g_xwaHardpointKindNames, path, "weapons.hardpoint_kinds", node, &kind)) {
				set->hardpointKinds[type] = (uint8_t)kind;
				set->hasHardpointKinds = 1;
			}
		}
	}
}

/* Reads a map from the names in `table` to float32 factors into `out`. The
   map must have every name. Otherwise logs a warning and returns 0. */
static int WeaponStats_ReadFactors(const char* key, const AeronConfigNode* node, const XwaNameTable* table,
								   float* out) {
	/* The ranks are the longest table that this reads. */
	float read[WEAPON_STATS_RANKS];
	int set = 0;
	size_t index;

	if (table->count > WEAPON_STATS_RANKS) {
		return 0;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: weapons.%s must map each %s to a number. It is ignored",
					  XWA_FLIGHT_CONFIG_PATH, key, table->what);
		return 0;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, index);
		const int at = XwaNames_Find(table, AeronConfigNode_MapKeyAt(node, index));
		char where[64];

		snprintf(where, sizeof where, "weapons.%s", key);
		if (at < 0) {
			XwaNames_LogUnknown(table, XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(value), where,
								AeronConfigNode_MapKeyAt(node, index));
			return 0;
		}
		if (AeronConfigNode_Type(value) != AERON_CONFIG_INT &&
			AeronConfigNode_Type(value) != AERON_CONFIG_FLOAT) {
			Aeron_LogWarn("xwa.models", "%s:%d: weapons.%s.%s must be a number. It is ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(value), key,
						  AeronConfigNode_MapKeyAt(node, index));
			return 0;
		}
		/* The importer writes enough digits to get the exact float32 value
		   back. */
		read[at] = AeronConfigNode_Type(value) == AERON_CONFIG_INT ? (float)AeronConfigNode_Int(value, 0)
																   : (float)AeronConfigNode_Float(value, 0.0);
		set |= 1 << at;
	}
	if (set != (1 << table->count) - 1) {
		Aeron_LogWarn("xwa.models", "%s: weapons.%s must have a number for every %s. It is ignored",
					  XWA_FLIGHT_CONFIG_PATH, key, table->what);
		return 0;
	}
	memcpy(out, read, (size_t)table->count * sizeof *out);
	return 1;
}

/* Returns the factor set of `key`: all, player, a rank name, or player_ and a
   rank name. Returns -1 for another key. */
static int WeaponStats_FactorSet(const char* key) {
	const int player = strncmp(key, "player_", 7) == 0;
	int rank;

	if (strcmp(key, "all") == 0) {
		return WEAPON_STATS_FACTORS_ALL;
	}
	if (strcmp(key, "player") == 0) {
		return WEAPON_STATS_FACTORS_PLAYER;
	}
	rank = XwaNames_Find(&g_xwaRankNames, player ? key + 7 : key);
	if (rank < 0) {
		return -1;
	}
	return (player ? WEAPON_STATS_FACTORS_PLAYER_RANK : WEAPON_STATS_FACTORS_RANK) + rank;
}

/* Reads the `factors` map of a model's `weapons` group into `set`. */
static void WeaponStats_ReadFactorSets(const char* path, const AeronConfigNode* node, WeaponStatsSet* set) {
	size_t index;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: weapons.factors isn't a map. It is ignored", path);
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(node, index);
		const int at = WeaponStats_FactorSet(key);
		WeaponStatsFactors* factors;
		size_t stat;

		if (at < 0 || AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models",
						  "%s:%d: weapons.factors.%s isn't a factor set. Use all, player, a rank or "
						  "player_<rank>, with a map of factors. It is ignored",
						  path, AeronConfigNode_Line(entry), key);
			continue;
		}
		factors = &set->factors[at];
		factors->present = 1;
		set->hasFactors = 1;
		for (stat = 0; stat < XWA_WEAPON_STATS; ++stat) {
			factors->value[stat] = 1.0f;
		}
		for (stat = 0; stat < AeronConfigNode_MapCount(entry); ++stat) {
			const char* statKey = AeronConfigNode_MapKeyAt(entry, stat);
			const AeronConfigNode* value = AeronConfigNode_MapValueAt(entry, stat);
			int found;

			for (found = 0; found < XWA_WEAPON_STATS; ++found) {
				if (s_factorStats[found] != NULL && strcmp(statKey, s_factorStats[found]) == 0) {
					break;
				}
			}
			if (found == XWA_WEAPON_STATS) {
				Aeron_LogWarn("xwa.models",
							  "%s:%d: weapons.factors.%s.%s isn't a stat with a factor. It is ignored", path,
							  AeronConfigNode_Line(value), key, statKey);
			} else if (AeronConfigNode_Type(value) != AERON_CONFIG_INT &&
					   AeronConfigNode_Type(value) != AERON_CONFIG_FLOAT) {
				Aeron_LogWarn("xwa.models", "%s:%d: weapons.factors.%s.%s must be a number. It is ignored",
							  path, AeronConfigNode_Line(value), key, statKey);
			} else {
				/* The importer writes enough digits to get the exact float32
				   value back. */
				factors->value[found] = AeronConfigNode_Type(value) == AERON_CONFIG_INT
											? (float)AeronConfigNode_Int(value, 0)
											: (float)AeronConfigNode_Float(value, 0.0);
			}
		}
	}
}

void XwaWeaponStats_Init(void) {
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	const AeronConfigNode* node;

	WeaponStats_FreeSet(&s_defaults);
	s_hasRankFactors = 0;
	s_hasDifficultyFactors = 0;
	s_limitLinkedWarheadFireRate = 0;
	memset(s_linkingRules, 0, sizeof s_linkingRules);
	root = XwaFlightConfig_Root();
	section = AeronConfigNode_MapGet(root, "weapons");
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		return;
	}
	WeaponStats_ReadSet(XWA_FLIGHT_CONFIG_PATH, section, &s_defaults);
	if ((node = AeronConfigNode_MapGet(section, "fire_ratio_by_rank")) != NULL) {
		s_hasRankFactors =
			WeaponStats_ReadFactors("fire_ratio_by_rank", node, &g_xwaRankNames, s_rankFactors);
	}
	if ((node = AeronConfigNode_MapGet(section, "damage_by_difficulty")) != NULL) {
		s_hasDifficultyFactors =
			WeaponStats_ReadFactors("damage_by_difficulty", node, &g_xwaDifficultyNames, s_difficultyFactors);
	}
	if ((node = AeronConfigNode_MapGet(section, "limit_linked_warhead_fire_rate")) != NULL) {
		int32_t value;

		if (WeaponStats_ReadBool(XWA_FLIGHT_CONFIG_PATH, "weapons.limit_linked_warhead_fire_rate", node,
								 &value)) {
			s_limitLinkedWarheadFireRate = value;
		}
	}
	if ((node = AeronConfigNode_MapGet(section, "linking_rules")) != NULL) {
		int rule;

		if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s: weapons.linking_rules isn't a map. It is ignored",
						  XWA_FLIGHT_CONFIG_PATH);
		}
		for (rule = 0; rule < XWA_WEAPON_LINKING_RULES; ++rule) {
			const AeronConfigNode* value = AeronConfigNode_MapGet(node, s_linkingRuleNames[rule]);
			char name[96];
			int32_t read;

			snprintf(name, sizeof name, "weapons.linking_rules.%s", s_linkingRuleNames[rule]);
			if (value != NULL && WeaponStats_ReadBool(XWA_FLIGHT_CONFIG_PATH, name, value, &read)) {
				s_linkingRules[rule] = (int8_t)read;
			}
		}
	}
}

void XwaWeaponStats_ClearModelSettings(void) {
	unsigned int objectType;

	for (objectType = 0; objectType < OBJ_Count; ++objectType) {
		WeaponStats_FreeSet(&s_models[objectType]);
	}
}

void XwaWeaponStats_ReadModelSettings(unsigned int objectType, const char* path,
									  const AeronConfigNode* group) {
	/* The weapon colours code logs a `weapons` group that isn't a map. */
	if (objectType >= OBJ_Count || AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		return;
	}
	WeaponStats_ReadSet(path, group, &s_models[objectType]);
	if (AeronConfigNode_MapGet(group, "factors") != NULL) {
		WeaponStats_ReadFactorSets(path, AeronConfigNode_MapGet(group, "factors"), &s_models[objectType]);
	}
}

void XwaWeaponStats_ClearFlightGroups(void) {
	memset(s_flightGroupProfiles, 0, sizeof s_flightGroupProfiles);
	memset(s_missingProfileLogged, 0, sizeof s_missingProfileLogged);
}

int XwaWeaponStats_ReadFlightGroup(const char* path, const char* fgKey, int fgIdx,
								   const AeronConfigNode* entry) {
	int group;
	int set = 0;

	if (fgIdx < 0 || fgIdx >= WEAPON_STATS_FLIGHT_GROUPS) {
		return 0;
	}
	for (group = 0; group < WEAPON_STATS_GROUPS; ++group) {
		const AeronConfigNode* node = AeronConfigNode_MapGet(entry, s_profileKeys[group]);

		if (node == NULL) {
			continue;
		}
		if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING ||
			!WeaponStats_ProfileName(AeronConfigNode_String(node, ""), s_flightGroupProfiles[fgIdx][group])) {
			Aeron_LogWarn("xwa.mission",
						  "%s: flight group %s: %s must be a name of up to %d characters; it is ignored",
						  path, fgKey, s_profileKeys[group], WEAPON_STATS_NAME_MAX - 1);
			s_flightGroupProfiles[fgIdx][group][0] = '\0';
			continue;
		}
		++set;
	}
	return set;
}

/* Returns the values of the model of object type `objectType`, or NULL if it
   has none. */
static const WeaponStatsSet* WeaponStats_Model(unsigned int objectType) {
	XwaModelSettings_Ensure(objectType);
	return objectType < OBJ_Count &&
				   (s_models[objectType].profileCount != 0 || s_models[objectType].hasHardpointKinds ||
					s_models[objectType].hasFactors)
			   ? &s_models[objectType]
			   : NULL;
}

static const WeaponStatsProfile* WeaponStats_FindProfile(const WeaponStatsSet* set, const char* name,
														 int group) {
	int index;

	if (set == NULL) {
		return NULL;
	}
	for (index = 0; index < set->profileCount; ++index) {
		if ((set->profiles[index].groups & (1u << group)) && strcmp(set->profiles[index].name, name) == 0) {
			return &set->profiles[index];
		}
	}
	return NULL;
}

/* Finds the profiles of group `group` for a craft of object type `objectType`
   and flight group `fgIdx`, in the model file and in flight/flight.yaml. If
   neither has the flight group's profile, both are NULL, so the craft gets
   the original game's values, and the log says so once. */
static void WeaponStats_Profiles(unsigned int objectType, int fgIdx, int group,
								 const WeaponStatsProfile** model, const WeaponStatsProfile** global) {
	const WeaponStatsSet* set = WeaponStats_Model(objectType);
	const char* name =
		fgIdx >= 0 && fgIdx < WEAPON_STATS_FLIGHT_GROUPS ? s_flightGroupProfiles[fgIdx][group] : "";

	*model = WeaponStats_FindProfile(set, name, group);
	*global = WeaponStats_FindProfile(&s_defaults, name, group);
	if (name[0] != '\0' && *model == NULL && *global == NULL &&
		!(s_missingProfileLogged[fgIdx] & (1u << group))) {
		s_missingProfileLogged[fgIdx] |= (uint8_t)(1u << group);
		Aeron_LogWarn("xwa.mission",
					  "flight group %d picks the %s profile '%s', but neither the model of %s nor "
					  "flight/flight.yaml has it. The flight group gets the original game's values",
					  fgIdx, s_groups[group], name, XwaNames_Name(&g_xwaObjectTypeNames, (int)objectType));
	}
}

/* Returns `value` with the type and the limits that the hook gives the stat. */
static int WeaponStats_Cast(XwaWeaponStat stat, int32_t value) {
	switch (stat) {
		case XWA_WEAPON_STAT_DURATION_SECONDS:
		case XWA_WEAPON_STAT_DURATION_FRACTION:
			return (uint16_t)value;
		case XWA_WEAPON_STAT_SCORE:
		case XWA_WEAPON_STAT_SPEED:
			return (int16_t)value;
		case XWA_WEAPON_STAT_POWER_SPEED_PERCENT:
			return value < 0 ? 0 : value;
		case XWA_WEAPON_STAT_PRECISE:
			return value != 0;
		case XWA_WEAPON_STAT_SIDE:
			return (uint8_t)value;
		case XWA_WEAPON_STAT_SIDE_MODEL:
			return (uint16_t)value;
		default:
			return value;
	}
}

/* Writes the value that a profile gives `stat` into `out`. Returns 0 when no
   profile sets it. */
static int WeaponStats_Lookup(unsigned int objectType, int fgIdx, int player, unsigned int weapon,
							  XwaWeaponStat stat, int* out) {
	const WeaponStatsProfile* profiles[2];
	int forms[4];
	int formCount = 0;
	int form;

	if (weapon < WEAPON_STATS_FIRST_WEAPON || weapon >= WEAPON_STATS_FIRST_WEAPON + WEAPON_STATS_WEAPONS ||
		stat >= XWA_WEAPON_STATS) {
		return 0;
	}
	WeaponStats_Profiles(objectType, fgIdx, WEAPON_STATS_GROUP_STATS, &profiles[0], &profiles[1]);
	if (profiles[0] == NULL && profiles[1] == NULL) {
		return 0;
	}
	/* The forms for the player and for the difficulty come first. */
	if (player) {
		if (g_flightDifficulty < WEAPON_STATS_DIFFICULTIES) {
			forms[formCount++] = WEAPON_STATS_FORM_PLAYER_EASY + g_flightDifficulty;
		}
		forms[formCount++] = WEAPON_STATS_FORM_PLAYER;
	}
	if (g_flightDifficulty < WEAPON_STATS_DIFFICULTIES) {
		forms[formCount++] = WEAPON_STATS_FORM_EASY + g_flightDifficulty;
	}
	forms[formCount++] = WEAPON_STATS_FORM_PLAIN;
	for (form = 0; form < formCount; ++form) {
		int at;

		for (at = 0; at < 2; ++at) {
			const WeaponStatsForm* values;

			if (profiles[at] == NULL) {
				continue;
			}
			values = &profiles[at]->weapons[weapon - WEAPON_STATS_FIRST_WEAPON][forms[form]];
			if (values->set & (1u << stat)) {
				*out = WeaponStats_Cast(stat, values->value[stat]);
				return 1;
			}
		}
	}
	return 0;
}

/* Returns the first factor set that the model of `object` has, or NULL: for
   a player's craft, player_<rank> then player; then <rank>; then all. A craft
   without a flight group, or with a rank past the last one, has no rank. */
static const WeaponStatsFactors* WeaponStats_Factors(const ObjectRecord* object) {
	const WeaponStatsSet* set = WeaponStats_Model(object->objectType);
	const unsigned int fgIdx = object->flightGroupIdx;
	const int player = object->playerOwnerIdx != -1;
	int rank = -1;
	int candidates[4];
	int count = 0;
	int at;

	if (set == NULL || !set->hasFactors) {
		return NULL;
	}
	if (fgIdx < WEAPON_STATS_FLIGHT_GROUPS && g_missionFlightGroups[fgIdx].fg.groupAI < WEAPON_STATS_RANKS) {
		rank = g_missionFlightGroups[fgIdx].fg.groupAI;
	}
	if (player && rank >= 0) {
		candidates[count++] = WEAPON_STATS_FACTORS_PLAYER_RANK + rank;
	}
	if (player) {
		candidates[count++] = WEAPON_STATS_FACTORS_PLAYER;
	}
	if (rank >= 0) {
		candidates[count++] = WEAPON_STATS_FACTORS_RANK + rank;
	}
	candidates[count++] = WEAPON_STATS_FACTORS_ALL;
	for (at = 0; at < count; ++at) {
		if (set->factors[candidates[at]].present) {
			return &set->factors[candidates[at]];
		}
	}
	return NULL;
}

/* Returns `value` times `factor`, multiplied in float32 and truncated. Score
   and speed are then cut to 16 bits. */
static int WeaponStats_Scale(XwaWeaponStat stat, int value, float factor) {
	const float scaled = (float)value * factor;

	return stat == XWA_WEAPON_STAT_SCORE || stat == XWA_WEAPON_STAT_SPEED ? (int16_t)(int)scaled
																		  : (int)scaled;
}

int XwaWeaponStats_Value(int srcObjIdx, unsigned int weapon, XwaWeaponStat stat, int stock) {
	const ObjectRecord* object;
	const WeaponStatsFactors* factors;
	/* An unset fire ratio or range counts as -1, and the factor multiplies
	   that -1 too. A result other than -1 then replaces the stock value. */
	const int marked = stat == XWA_WEAPON_STAT_FIRE_RATIO || stat == XWA_WEAPON_STAT_RANGE;
	int value;
	int set;

	if (srcObjIdx < 0) {
		return stock;
	}
	object = &g_objectTable[srcObjIdx];
	set = WeaponStats_Lookup(object->objectType, object->flightGroupIdx, object->playerOwnerIdx != -1, weapon,
							 stat, &value);
	if (!set) {
		value = marked ? -1 : stock;
	}
	/* The callers of the charge rates apply their percents first, then the
	   factor. */
	if (stat < XWA_WEAPON_STATS && stat != XWA_WEAPON_STAT_DECHARGE_RATE &&
		stat != XWA_WEAPON_STAT_RECHARGE_RATE && s_factorStats[stat] != NULL &&
		(factors = WeaponStats_Factors(object)) != NULL) {
		value = WeaponStats_Scale(stat, value, factors->value[stat]);
	}
	return !set && marked && value == -1 ? stock : value;
}

int XwaWeaponStats_TypeValue(unsigned int objectType, unsigned int weapon, XwaWeaponStat stat, int stock) {
	int value;

	return WeaponStats_Lookup(objectType, -1, 0, weapon, stat, &value) ? value : stock;
}

int XwaWeaponStats_HasFactors(int objIdx) {
	return objIdx >= 0 && WeaponStats_Factors(&g_objectTable[objIdx]) != NULL;
}

int XwaWeaponStats_ChargeFactor(int objIdx, XwaWeaponStat stat, int rate) {
	const WeaponStatsFactors* factors;

	if (objIdx < 0 || (factors = WeaponStats_Factors(&g_objectTable[objIdx])) == NULL) {
		return rate;
	}
	return WeaponStats_Scale(stat, rate, factors->value[stat]);
}

uint16_t XwaWeaponStats_WarheadCapacity(unsigned int objIdx, int warheadType, uint16_t capacity,
										uint16_t fracQ16) {
	const WeaponStatsProfile* profiles[2];
	int at;

	if (warheadType >= 0 && warheadType < WEAPON_STATS_WARHEADS) {
		WeaponStats_Profiles(g_objectTable[objIdx].objectType, g_objectTable[objIdx].flightGroupIdx,
							 WEAPON_STATS_GROUP_WARHEADS, &profiles[0], &profiles[1]);
		for (at = 0; at < 2; ++at) {
			if (profiles[at] != NULL && profiles[at]->warheadPercent[warheadType] >= 0) {
				return (uint16_t)(((int64_t)capacity * profiles[at]->warheadPercent[warheadType] + 50) / 100);
			}
		}
	}
	return MATH2_fraction(capacity, fracQ16);
}

int XwaWeaponStats_EnergySpeedStep(unsigned int objIdx, int stock) {
	const WeaponStatsProfile* profiles[2];
	int key;
	int at;

	WeaponStats_Profiles(g_objectTable[objIdx].objectType, g_objectTable[objIdx].flightGroupIdx,
						 WEAPON_STATS_GROUP_ENERGY, &profiles[0], &profiles[1]);
	for (key = g_objectTable[objIdx].playerOwnerIdx != -1; key >= 0; --key) {
		for (at = 0; at < 2; ++at) {
			if (profiles[at] != NULL && profiles[at]->energy[key] >= 0) {
				return profiles[at]->energy[key];
			}
		}
	}
	return stock;
}

int XwaWeaponStats_LaserLink(unsigned int objIdx, int stock) {
	const WeaponStatsProfile* profiles[2];
	int at;

	WeaponStats_Profiles(g_objectTable[objIdx].objectType, g_objectTable[objIdx].flightGroupIdx,
						 WEAPON_STATS_GROUP_LINKING, &profiles[0], &profiles[1]);
	for (at = 0; at < 2; ++at) {
		if (profiles[at] != NULL && profiles[at]->laserLink >= 0) {
			return profiles[at]->laserLink;
		}
	}
	return stock;
}

int XwaWeaponStats_LinkingRule(unsigned int objIdx, XwaWeaponLinkingRule rule) {
	const WeaponStatsProfile* profiles[2];
	int at;

	if (rule >= XWA_WEAPON_LINKING_RULES) {
		return 0;
	}
	WeaponStats_Profiles(g_objectTable[objIdx].objectType, g_objectTable[objIdx].flightGroupIdx,
						 WEAPON_STATS_GROUP_LINKING, &profiles[0], &profiles[1]);
	for (at = 0; at < 2; ++at) {
		if (profiles[at] != NULL && profiles[at]->linking[rule] >= 0) {
			return profiles[at]->linking[rule];
		}
	}
	return s_linkingRules[rule];
}

int XwaWeaponStats_HardpointKind(unsigned int objectType, int hardpointType, int stock) {
	const WeaponStatsSet* model = WeaponStats_Model(objectType);

	if (hardpointType < 0 || hardpointType >= WEAPON_STATS_HARDPOINT_TYPES) {
		return stock;
	}
	if (model != NULL && model->hardpointKinds[hardpointType] != WEAPON_STATS_STOCK_KIND) {
		return model->hardpointKinds[hardpointType];
	}
	if (s_defaults.hardpointKinds[hardpointType] != WEAPON_STATS_STOCK_KIND) {
		return s_defaults.hardpointKinds[hardpointType];
	}
	return stock;
}

int XwaWeaponStats_HardpointWeaponType(int hardpointType) {
	if (hardpointType >= 1 && hardpointType <= 18) {
		return 280 + hardpointType - 1;
	}
	if (hardpointType >= 33 && hardpointType <= 37) {
		return 301 + hardpointType - 33;
	}
	return hardpointType == 38 ? 307 : 0;
}

int XwaWeaponStats_WeaponHardpointType(int weapon) {
	if (weapon >= 301 && weapon <= 305) {
		return 33 + weapon - 301;
	}
	return weapon == 307 ? 38 : weapon - 279;
}

int XwaWeaponStats_HasFireRatioByRank(void) { return s_hasRankFactors; }

int XwaWeaponStats_FireRatioByRank(unsigned int objIdx, int fireRatio) {
	const unsigned int fgIdx = g_objectTable[objIdx].flightGroupIdx;
	unsigned int rank;
	float scaled;

	if (fgIdx >= WEAPON_STATS_FLIGHT_GROUPS) {
		return fireRatio;
	}
	rank = g_missionFlightGroups[fgIdx].fg.groupAI;
	if (rank >= WEAPON_STATS_RANKS) {
		return fireRatio;
	}
	/* The hook multiplies the 16-bit fire ratio in float32. */
	scaled = (float)(uint16_t)fireRatio * s_rankFactors[rank];
	return (int)scaled;
}

int XwaWeaponStats_DamageByDifficulty(int damage, int* out) {
	float scaled;

	if (!s_hasDifficultyFactors) {
		return 0;
	}
	scaled = (float)damage *
			 s_difficultyFactors[g_flightDifficulty < WEAPON_STATS_DIFFICULTIES ? g_flightDifficulty : 2];
	*out = (int)scaled;
	return 1;
}

int XwaWeaponStats_LimitLinkedWarheadFireRate(void) { return s_limitLinkedWarheadFireRate; }
