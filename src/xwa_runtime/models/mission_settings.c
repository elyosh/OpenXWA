#include "xwa_runtime/models/mission_settings.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/flight/ai/pai.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/math/trig2.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/models/mission_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* g_missionFlightGroups and FrontendMission.flightGroups hold 192. */
#define SETTINGS_FLIGHT_GROUPS 192
#define SETTINGS_NAMES_MAX 32
#define SETTINGS_LINES_MAX 32
#define SETTINGS_TEXT_SIZE 128
/* skipArrivalIff: the stock rule, no IFF, every IFF. */
#define SETTINGS_IFF_STOCK (-1)
#define SETTINGS_IFF_NONE (-2)
#define SETTINGS_IFF_ALL 256

typedef struct MissionSettingsName {
	int flightGroup;
	char text[SETTINGS_TEXT_SIZE];
} MissionSettingsName;

typedef struct MissionSettings {
	int redAlert; /* -1: the stock rule */
	int hideHyperspaceExitMessages; /* -1: the stock rule */
	int hideFriendlyArrivalMessages;
	int skipArrivalIff; /* an IFF, or SETTINGS_IFF_* */
	int forceTurret;    /* -1: the stock rule; 1: at turretHours:turretMinutes:turretSeconds */
	int turretHours, turretMinutes, turretSeconds;
	int disableLasers, disableWarheads;
	int warheadCollisionDamage;
	int damageThroughShields;
	int proximityCraftOnly;
	int useHyperspaceSpeeds;
	uint8_t hasRegionRule[XWA_HYPERSPACE_REGIONS];
	XwaHyperspaceRegionRule regionRules[XWA_HYPERSPACE_REGIONS];
	int hasTargetKey, targetCycle, targetOnlyNotInspected;
	uint8_t targetFlightGroups[SETTINGS_FLIGHT_GROUPS / 8];
	int hasOrderOfBattleList;
	uint8_t orderOfBattleFlightGroups[SETTINGS_FLIGHT_GROUPS / 8];
	MissionSettingsName names[SETTINGS_NAMES_MAX];
	int nameCount;
	char lines[SETTINGS_LINES_MAX][SETTINGS_TEXT_SIZE];
	int lineCount;
	int linesTop;
} MissionSettings;

static MissionSettings s_defaults;
static MissionSettings s_settings;
static int s_resetCombatMusic;
static int s_redAlertAnyMothership;
static int s_classicHideDestroyedEngineGlows;
static XwaWeaponIffSwap s_weaponIffSwap;
static int s_aiAttackUsesMaxSpeedCache;
static int s_fireNeedsDechargeRate;
static int s_noStarshipFireFromDestroyedRacks;
static int s_starshipAmbientSounds;
static int s_noPlayerWarheadSlotCap;
static int s_gunnerWeaponSelectInSeat;
static int s_gunnerFiresSelectedGroup;
static int s_gunnerWarheadsNeedCockpitHardpoint;
static int s_noFireFromDestroyedHardpoints;
static int s_tieFighterEnergyStepAlwaysHigh;
static int s_playerTurretRacksKeepCharge;
static int s_rearmFollowsBothStatuses;
static int s_warheadCollisionBonusAnyTarget;
static int s_turbolaserHardpointTypes;
static int s_gunTurretHardpoints;
static int s_playerWeaponJamming;
static int s_superStarDestroyerJamming;
static int s_twoLauncherFixes;
static int s_missileDefenseAnyWarhead;
static int s_rackCountFromRecord;
static int s_overlappingFlyBySounds;
static int s_hangarDroidPartsFromModel;
/* The craft that the O key targeted last, per player, for the cycle
   method. */
static int s_lastTargetObjIdx[XWA_PLAYER_COUNT];

/* The stock rules: no value set. */
static void MissionSettings_Stock(MissionSettings* s) {
	memset(s, 0, sizeof *s);
	s->redAlert = -1;
	s->hideHyperspaceExitMessages = -1;
	s->skipArrivalIff = SETTINGS_IFF_STOCK;
	s->forceTurret = -1;
	s->warheadCollisionDamage = 1;
	s->useHyperspaceSpeeds = 1;
	s->linesTop = -1;
}

static void MissionSettings_ResetTargets(void) {
	int i;

	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		s_lastTargetObjIdx[i] = -1;
	}
}

void XwaMissionSettings_SaveFlightState(int32_t* lastTarget) {
	int i;

	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		lastTarget[i] = s_lastTargetObjIdx[i];
	}
}

void XwaMissionSettings_RestoreFlightState(const int32_t* lastTarget) {
	int i;

	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		s_lastTargetObjIdx[i] = lastTarget[i];
	}
}

static int MissionSettings_Has(const uint8_t* bits, int index) {
	return index >= 0 && index < SETTINGS_FLIGHT_GROUPS && (bits[index / 8] & (1u << (index % 8))) != 0;
}

/* Reads `node` as a boolean into `out`. Logs and keeps `out` when it is
   another type. */
static void MissionSettings_ReadBool(const char* path, const char* key, const AeronConfigNode* node,
									 int* out) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogWarn("xwa.mission", "%s:%d: %s must be true or false; it is ignored", path,
					  AeronConfigNode_Line(node), key);
		return;
	}
	*out = AeronConfigNode_Bool(node, 0) != 0;
}

/* Reads a list of flight-group positions into `bits`. Returns 0, logged,
   when `node` isn't a list; positions out of range are logged and left
   out. */
static int MissionSettings_ReadFlightGroups(const char* path, const char* key, const AeronConfigNode* node,
											uint8_t* bits) {
	size_t i;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE) {
		Aeron_LogWarn("xwa.mission", "%s:%d: %s must be a list of flight groups; it is ignored", path,
					  AeronConfigNode_Line(node), key);
		return 0;
	}
	memset(bits, 0, SETTINGS_FLIGHT_GROUPS / 8);
	for (i = 0; i < AeronConfigNode_SequenceCount(node); ++i) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, i);
		const int64_t fg = AeronConfigNode_Int(item, -1);

		if (AeronConfigNode_Type(item) != AERON_CONFIG_INT || fg < 0 || fg >= SETTINGS_FLIGHT_GROUPS) {
			Aeron_LogWarn("xwa.mission", "%s:%d: %s: flight groups are numbers from 0 to %d; one is ignored",
						  path, AeronConfigNode_Line(item), key, SETTINGS_FLIGHT_GROUPS - 1);
			continue;
		}
		bits[fg / 8] |= (uint8_t)(1u << (fg % 8));
	}
	return 1;
}

/* Copies string `node` into `out`. Returns 0, logged, when it isn't a
   string or is too long. */
static int MissionSettings_ReadText(const char* path, const char* key, const AeronConfigNode* node,
									char* out) {
	const char* text = AeronConfigNode_String(node, NULL);

	if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING || strlen(text) >= SETTINGS_TEXT_SIZE) {
		Aeron_LogWarn("xwa.mission", "%s:%d: %s must be a text of up to %d characters; it is ignored", path,
					  AeronConfigNode_Line(node), key, SETTINGS_TEXT_SIZE - 1);
		return 0;
	}
	snprintf(out, SETTINGS_TEXT_SIZE, "%s", text);
	return 1;
}

static void MissionSettings_ReadArrivalIff(const char* path, const AeronConfigNode* node,
										   MissionSettings* s) {
	const char* text = AeronConfigNode_String(node, NULL);
	const int64_t iff = AeronConfigNode_Int(node, -1);

	if (AeronConfigNode_Type(node) == AERON_CONFIG_STRING && strcmp(text, "all") == 0) {
		s->skipArrivalIff = SETTINGS_IFF_ALL;
	} else if (AeronConfigNode_Type(node) == AERON_CONFIG_STRING && strcmp(text, "none") == 0) {
		s->skipArrivalIff = SETTINGS_IFF_NONE;
	} else if (AeronConfigNode_Type(node) == AERON_CONFIG_INT && iff >= 0 && iff <= 254) {
		s->skipArrivalIff = (int)iff;
	} else {
		Aeron_LogWarn(
			"xwa.mission",
			"%s:%d: skip_arrival_messages_iff must be an IFF from 0 to 254, all or none; it is ignored", path,
			AeronConfigNode_Line(node));
	}
}

static void MissionSettings_ReadTurret(const char* path, const AeronConfigNode* node, MissionSettings* s) {
	const char* text = AeronConfigNode_String(node, NULL);
	unsigned hours;
	unsigned minutes;
	unsigned seconds;
	char end;

	if (AeronConfigNode_Type(node) == AERON_CONFIG_BOOL && !AeronConfigNode_Bool(node, 1)) {
		s->forceTurret = 0;
		return;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING ||
		sscanf(text, "%u:%u:%u%c", &hours, &minutes, &seconds, &end) != 3 || hours > 255 || minutes > 59 ||
		seconds > 59) {
		Aeron_LogWarn(
			"xwa.mission",
			"%s:%d: force_player_in_turret must be a mission time such as \"0:00:08\", or false; it "
			"is ignored",
			path, AeronConfigNode_Line(node));
		return;
	}
	s->forceTurret = 1;
	s->turretHours = (int)hours;
	s->turretMinutes = (int)minutes;
	s->turretSeconds = (int)seconds;
}

static void MissionSettings_ReadTargetKey(const char* path, const AeronConfigNode* node, MissionSettings* s) {
	const AeronConfigNode* method = AeronConfigNode_MapGet(node, "method");
	const AeronConfigNode* onlyNotInspected = AeronConfigNode_MapGet(node, "only_not_inspected");
	MissionSettings read = *s;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP ||
		!MissionSettings_ReadFlightGroups(path, "target_key: flight_groups",
										  AeronConfigNode_MapGet(node, "flight_groups"),
										  read.targetFlightGroups)) {
		Aeron_LogWarn("xwa.mission", "%s:%d: target_key needs a list of flight_groups; it is ignored", path,
					  AeronConfigNode_Line(node));
		return;
	}
	read.hasTargetKey = 1;
	read.targetCycle = 0;
	read.targetOnlyNotInspected = 0;
	if (method != NULL) {
		const char* name = AeronConfigNode_String(method, "");

		if (AeronConfigNode_Type(method) == AERON_CONFIG_STRING && strcmp(name, "cycle") == 0) {
			read.targetCycle = 1;
		} else if (AeronConfigNode_Type(method) != AERON_CONFIG_STRING || strcmp(name, "nearest") != 0) {
			Aeron_LogWarn("xwa.mission",
						  "%s:%d: target_key: method must be nearest or cycle; nearest is used", path,
						  AeronConfigNode_Line(method));
		}
	}
	if (onlyNotInspected != NULL) {
		MissionSettings_ReadBool(path, "target_key: only_not_inspected", onlyNotInspected,
								 &read.targetOnlyNotInspected);
	}
	*s = read;
}

static void MissionSettings_ReadOrderOfBattle(const char* path, const AeronConfigNode* node,
											  MissionSettings* s) {
	const AeronConfigNode* fgs = AeronConfigNode_MapGet(node, "flight_groups");
	const AeronConfigNode* names = AeronConfigNode_MapGet(node, "names");
	const AeronConfigNode* lines = AeronConfigNode_MapGet(node, "lines");
	const AeronConfigNode* top = AeronConfigNode_MapGet(node, "top");
	size_t i;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.mission", "%s:%d: order_of_battle must be a map; it is ignored", path,
					  AeronConfigNode_Line(node));
		return;
	}
	s->hasOrderOfBattleList = 0;
	s->nameCount = 0;
	s->lineCount = 0;
	s->linesTop = -1;
	if (fgs != NULL) {
		s->hasOrderOfBattleList = MissionSettings_ReadFlightGroups(path, "order_of_battle: flight_groups",
																   fgs, s->orderOfBattleFlightGroups);
	}
	if (names != NULL && AeronConfigNode_Type(names) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.mission",
					  "%s:%d: order_of_battle: names must be a map of flight groups; it is ignored", path,
					  AeronConfigNode_Line(names));
		names = NULL;
	}
	for (i = 0; i < AeronConfigNode_MapCount(names); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(names, i);
		char* end = NULL;
		const long fg = strtol(key, &end, 10);
		MissionSettingsName* name = &s->names[s->nameCount];

		if (end == key || *end != '\0' || fg < 0 || fg >= SETTINGS_FLIGHT_GROUPS) {
			Aeron_LogWarn("xwa.mission",
						  "%s:%d: order_of_battle: names: '%s' isn't a flight group; it is ignored", path,
						  AeronConfigNode_Line(names), key);
			continue;
		}
		if (s->nameCount == SETTINGS_NAMES_MAX) {
			Aeron_LogWarn("xwa.mission", "%s:%d: order_of_battle: more than %d names; the rest are ignored",
						  path, AeronConfigNode_Line(names), SETTINGS_NAMES_MAX);
			break;
		}
		if (MissionSettings_ReadText(path, "order_of_battle: names", AeronConfigNode_MapValueAt(names, i),
									 name->text)) {
			name->flightGroup = (int)fg;
			++s->nameCount;
		}
	}
	if (lines != NULL && AeronConfigNode_Type(lines) != AERON_CONFIG_SEQUENCE) {
		Aeron_LogWarn("xwa.mission", "%s:%d: order_of_battle: lines must be a list of texts; it is ignored",
					  path, AeronConfigNode_Line(lines));
		lines = NULL;
	}
	for (i = 0; i < AeronConfigNode_SequenceCount(lines); ++i) {
		if (s->lineCount == SETTINGS_LINES_MAX) {
			Aeron_LogWarn("xwa.mission", "%s:%d: order_of_battle: more than %d lines; the rest are ignored",
						  path, AeronConfigNode_Line(lines), SETTINGS_LINES_MAX);
			break;
		}
		if (MissionSettings_ReadText(path, "order_of_battle: lines", AeronConfigNode_SequenceGet(lines, i),
									 s->lines[s->lineCount])) {
			++s->lineCount;
		}
	}
	if (top != NULL) {
		const int64_t value = AeronConfigNode_Int(top, -1);

		if (AeronConfigNode_Type(top) != AERON_CONFIG_INT || value < 0 || value > 479) {
			Aeron_LogWarn("xwa.mission",
						  "%s:%d: order_of_battle: top must be a number from 0 to 479; it is ignored", path,
						  AeronConfigNode_Line(top));
		} else {
			s->linesTop = (int)value;
		}
	}
}

/* Reads one region's rule of hyperspace_regions. Returns 0, logged, when it
   isn't a valid rule. */
static int MissionSettings_ReadRegionRule(const char* path, const AeronConfigNode* node,
										  XwaHyperspaceRegionRule* rule) {
	size_t i;

	memset(rule, 0, sizeof *rule);
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.mission",
					  "%s:%d: hyperspace_regions: a region's rule must be a map; it is ignored", path,
					  AeronConfigNode_Line(node));
		return 0;
	}
	for (i = 0; i < AeronConfigNode_MapCount(node); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(node, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, i);
		const char* text = AeronConfigNode_String(value, NULL);

		if (strcmp(key, "look") == 0) {
			size_t length = text != NULL ? strlen(text) : 0;
			size_t c;
			int valid = AeronConfigNode_Type(value) == AERON_CONFIG_STRING && length > 0 &&
						length < XWA_HYPERSPACE_LOOK_NAME_MAX;

			for (c = 0; valid && c < length; ++c) {
				valid = (text[c] >= 'a' && text[c] <= 'z') || (text[c] >= '0' && text[c] <= '9') ||
						text[c] == '_';
			}
			if (!valid) {
				Aeron_LogWarn(
					"xwa.mission",
					"%s:%d: hyperspace_regions: look must be a name of lower-case letters, digits and "
					"'_'; it is ignored",
					path, AeronConfigNode_Line(value));
				continue;
			}
			memcpy(rule->look, text, length + 1);
		} else if (strcmp(key, "arrival_sound") == 0) {
			if (AeronConfigNode_Type(value) != AERON_CONFIG_STRING ||
				(strcmp(text, "abort") != 0 && strcmp(text, "normal") != 0)) {
				Aeron_LogWarn(
					"xwa.mission",
					"%s:%d: hyperspace_regions: arrival_sound must be abort or normal; it is ignored", path,
					AeronConfigNode_Line(value));
				continue;
			}
			rule->abortSound = strcmp(text, "abort") == 0;
		} else {
			Aeron_LogWarn("xwa.mission",
						  "%s:%d: hyperspace_regions: '%s' isn't a key of a region's rule; it is ignored",
						  path, AeronConfigNode_Line(value), key);
		}
	}
	return 1;
}

static void MissionSettings_ReadHyperspaceRegions(const char* path, const AeronConfigNode* node,
												  MissionSettings* s) {
	size_t i;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.mission", "%s:%d: hyperspace_regions must be a map of regions; it is ignored",
					  path, AeronConfigNode_Line(node));
		return;
	}
	memset(s->hasRegionRule, 0, sizeof s->hasRegionRule);
	memset(s->regionRules, 0, sizeof s->regionRules);
	for (i = 0; i < AeronConfigNode_MapCount(node); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(node, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, i);
		const int region =
			key[0] >= '0' && key[0] < '0' + XWA_HYPERSPACE_REGIONS && key[1] == '\0' ? key[0] - '0' : -1;

		if (region < 0) {
			Aeron_LogWarn("xwa.mission",
						  "%s:%d: hyperspace_regions: '%s' isn't a region from 0 to %d; it is ignored", path,
						  AeronConfigNode_Line(value), key, XWA_HYPERSPACE_REGIONS - 1);
			continue;
		}
		if (MissionSettings_ReadRegionRule(path, value, &s->regionRules[region])) {
			s->hasRegionRule[region] = 1;
		}
	}
}

/* Applies the keys of `settings`, a `settings` or `mission_settings` map,
   to `s`. */
static void MissionSettings_Read(const char* path, const AeronConfigNode* settings, MissionSettings* s) {
	size_t i;

	if (settings == NULL) {
		return;
	}
	if (AeronConfigNode_Type(settings) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.mission", "%s:%d: the settings must be a map; they are ignored", path,
					   AeronConfigNode_Line(settings));
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(settings); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(settings, i);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(settings, i);

		if (strcmp(key, "red_alert") == 0) {
			MissionSettings_ReadBool(path, key, node, &s->redAlert);
		} else if (strcmp(key, "hide_hyperspace_exit_messages") == 0) {
			MissionSettings_ReadBool(path, key, node, &s->hideHyperspaceExitMessages);
		} else if (strcmp(key, "hide_friendly_arrival_messages") == 0) {
			MissionSettings_ReadBool(path, key, node, &s->hideFriendlyArrivalMessages);
		} else if (strcmp(key, "skip_arrival_messages_iff") == 0) {
			MissionSettings_ReadArrivalIff(path, node, s);
		} else if (strcmp(key, "force_player_in_turret") == 0) {
			MissionSettings_ReadTurret(path, node, s);
		} else if (strcmp(key, "disable_player_lasers") == 0) {
			MissionSettings_ReadBool(path, key, node, &s->disableLasers);
		} else if (strcmp(key, "disable_player_warheads") == 0) {
			MissionSettings_ReadBool(path, key, node, &s->disableWarheads);
		} else if (strcmp(key, "warhead_collision_damage") == 0) {
			MissionSettings_ReadBool(path, key, node, &s->warheadCollisionDamage);
		} else if (strcmp(key, "damage_through_shields_on_hard") == 0) {
			MissionSettings_ReadBool(path, key, node, &s->damageThroughShields);
		} else if (strcmp(key, "target_key") == 0) {
			MissionSettings_ReadTargetKey(path, node, s);
		} else if (strcmp(key, "proximity_triggers_check_craft_only") == 0) {
			MissionSettings_ReadBool(path, key, node, &s->proximityCraftOnly);
		} else if (strcmp(key, "order_of_battle") == 0) {
			MissionSettings_ReadOrderOfBattle(path, node, s);
		} else if (strcmp(key, "use_hyperspace_speeds") == 0) {
			MissionSettings_ReadBool(path, key, node, &s->useHyperspaceSpeeds);
		} else if (strcmp(key, "hyperspace_regions") == 0) {
			MissionSettings_ReadHyperspaceRegions(path, node, s);
		} else {
			Aeron_LogWarn("xwa.mission", "%s:%d: '%s' isn't a mission setting; it is ignored", path,
						  AeronConfigNode_Line(node), key);
		}
	}
}

void XwaMissionSettings_Init(void) {
	const AeronConfigNode* root;
	const AeronConfigNode* rules;
	const AeronConfigNode* node;

	MissionSettings_Stock(&s_defaults);
	s_settings = s_defaults;
	s_resetCombatMusic = 0;
	s_redAlertAnyMothership = 0;
	s_classicHideDestroyedEngineGlows = 0;
	s_weaponIffSwap = XWA_WEAPON_IFF_SWAP_TURRETS;
	s_aiAttackUsesMaxSpeedCache = 0;
	s_fireNeedsDechargeRate = 0;
	s_noStarshipFireFromDestroyedRacks = 0;
	s_starshipAmbientSounds = 0;
	s_noPlayerWarheadSlotCap = 0;
	s_gunnerWeaponSelectInSeat = 0;
	s_gunnerFiresSelectedGroup = 0;
	s_gunnerWarheadsNeedCockpitHardpoint = 0;
	s_noFireFromDestroyedHardpoints = 0;
	s_tieFighterEnergyStepAlwaysHigh = 0;
	s_playerTurretRacksKeepCharge = 0;
	s_rearmFollowsBothStatuses = 0;
	s_warheadCollisionBonusAnyTarget = 0;
	s_turbolaserHardpointTypes = 0;
	s_gunTurretHardpoints = 0;
	s_playerWeaponJamming = 1;
	s_superStarDestroyerJamming = 0;
	s_twoLauncherFixes = 0;
	s_missileDefenseAnyWarhead = 0;
	s_rackCountFromRecord = 0;
	s_overlappingFlyBySounds = 0;
	s_hangarDroidPartsFromModel = 0;
	MissionSettings_ResetTargets();
	root = XwaFlightConfig_Root();
	rules = AeronConfigNode_MapGet(root, "rules");
	if (rules != NULL && AeronConfigNode_Type(rules) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.mission", "%s:%d: rules must be a map; they are ignored", XWA_FLIGHT_CONFIG_PATH,
					   AeronConfigNode_Line(rules));
		rules = NULL;
	}
	if ((node = AeronConfigNode_MapGet(rules, "reset_combat_music_at_mission_load")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "reset_combat_music_at_mission_load", node,
								 &s_resetCombatMusic);
	}
	if ((node = AeronConfigNode_MapGet(rules, "hangar_red_alert_any_mothership")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "hangar_red_alert_any_mothership", node,
								 &s_redAlertAnyMothership);
	}
	if ((node = AeronConfigNode_MapGet(rules, "classic_hide_destroyed_engine_glows")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "classic_hide_destroyed_engine_glows", node,
								 &s_classicHideDestroyedEngineGlows);
	}
	if ((node = AeronConfigNode_MapGet(rules, "weapon_iff_swap")) != NULL &&
		!XwaMissionSettings_ParseWeaponIffSwap(AeronConfigNode_String(node, NULL), &s_weaponIffSwap)) {
		Aeron_LogWarn("xwa.mission", "%s:%d: weapon_iff_swap must be turrets, none or all; it is ignored",
					  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node));
	}
	if ((node = AeronConfigNode_MapGet(rules, "ai_attack_uses_max_speed_cache")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "ai_attack_uses_max_speed_cache", node,
								 &s_aiAttackUsesMaxSpeedCache);
	}
	if ((node = AeronConfigNode_MapGet(rules, "fire_needs_decharge_rate")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "fire_needs_decharge_rate", node,
								 &s_fireNeedsDechargeRate);
	}
	if ((node = AeronConfigNode_MapGet(rules, "no_starship_fire_from_destroyed_racks")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "no_starship_fire_from_destroyed_racks", node,
								 &s_noStarshipFireFromDestroyedRacks);
	}
	if ((node = AeronConfigNode_MapGet(rules, "starship_ambient_sounds")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "starship_ambient_sounds", node,
								 &s_starshipAmbientSounds);
	}
	if ((node = AeronConfigNode_MapGet(rules, "no_player_warhead_slot_cap")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "no_player_warhead_slot_cap", node,
								 &s_noPlayerWarheadSlotCap);
	}
	if ((node = AeronConfigNode_MapGet(rules, "gunner_weapon_select_in_seat")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "gunner_weapon_select_in_seat", node,
								 &s_gunnerWeaponSelectInSeat);
	}
	if ((node = AeronConfigNode_MapGet(rules, "gunner_fires_selected_group")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "gunner_fires_selected_group", node,
								 &s_gunnerFiresSelectedGroup);
	}
	if ((node = AeronConfigNode_MapGet(rules, "gunner_warheads_need_cockpit_hardpoint")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "gunner_warheads_need_cockpit_hardpoint", node,
								 &s_gunnerWarheadsNeedCockpitHardpoint);
	}
	if ((node = AeronConfigNode_MapGet(rules, "no_fire_from_destroyed_hardpoints")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "no_fire_from_destroyed_hardpoints", node,
								 &s_noFireFromDestroyedHardpoints);
	}
	if ((node = AeronConfigNode_MapGet(rules, "tie_fighter_energy_step_always_high")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "tie_fighter_energy_step_always_high", node,
								 &s_tieFighterEnergyStepAlwaysHigh);
	}
	if ((node = AeronConfigNode_MapGet(rules, "player_turret_racks_keep_charge")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "player_turret_racks_keep_charge", node,
								 &s_playerTurretRacksKeepCharge);
	}
	if ((node = AeronConfigNode_MapGet(rules, "rearm_follows_both_statuses")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "rearm_follows_both_statuses", node,
								 &s_rearmFollowsBothStatuses);
	}
	if ((node = AeronConfigNode_MapGet(rules, "warhead_collision_bonus_any_target")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "warhead_collision_bonus_any_target", node,
								 &s_warheadCollisionBonusAnyTarget);
	}
	if ((node = AeronConfigNode_MapGet(rules, "turbolaser_hardpoint_types")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "turbolaser_hardpoint_types", node,
								 &s_turbolaserHardpointTypes);
	}
	if ((node = AeronConfigNode_MapGet(rules, "gun_turret_hardpoints")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "gun_turret_hardpoints", node,
								 &s_gunTurretHardpoints);
	}
	if ((node = AeronConfigNode_MapGet(rules, "player_weapon_jamming")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "player_weapon_jamming", node,
								 &s_playerWeaponJamming);
	}
	if ((node = AeronConfigNode_MapGet(rules, "super_star_destroyer_jamming")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "super_star_destroyer_jamming", node,
								 &s_superStarDestroyerJamming);
	}
	if ((node = AeronConfigNode_MapGet(rules, "two_launcher_fixes")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "two_launcher_fixes", node, &s_twoLauncherFixes);
	}
	if ((node = AeronConfigNode_MapGet(rules, "starship_missile_defense_any_warhead")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "starship_missile_defense_any_warhead", node,
								 &s_missileDefenseAnyWarhead);
	}
	if ((node = AeronConfigNode_MapGet(rules, "weapon_rack_count_from_record")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "weapon_rack_count_from_record", node,
								 &s_rackCountFromRecord);
	}
	if ((node = AeronConfigNode_MapGet(rules, "overlapping_fly_by_sounds")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "overlapping_fly_by_sounds", node,
								 &s_overlappingFlyBySounds);
	}
	if ((node = AeronConfigNode_MapGet(rules, "hangar_droid_parts_from_model")) != NULL) {
		MissionSettings_ReadBool(XWA_FLIGHT_CONFIG_PATH, "hangar_droid_parts_from_model", node,
								 &s_hangarDroidPartsFromModel);
	}
	MissionSettings_Read(XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_MapGet(root, "mission_settings"),
						 &s_defaults);
	s_settings = s_defaults;
}

void XwaMissionSettings_LoadMission(const char* missionPath) {
	AeronConfigFile* config = NULL;
	char path[300];

	s_settings = s_defaults;
	MissionSettings_ResetTargets();
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	MissionSettings_Read(path, AeronConfigNode_MapGet(AeronConfigFile_Root(config), "settings"), &s_settings);
	AeronConfigFile_Destroy(config);
}

int XwaMissionSettings_ResetCombatMusicAtMissionLoad(void) { return s_resetCombatMusic; }

int XwaMissionSettings_HangarRedAlertAnyMothership(void) { return s_redAlertAnyMothership; }

int XwaMissionSettings_ClassicHideDestroyedEngineGlows(void) { return s_classicHideDestroyedEngineGlows; }

int XwaMissionSettings_ParseWeaponIffSwap(const char* text, XwaWeaponIffSwap* out) {
	if (text == NULL) {
		return 0;
	}
	if (strcmp(text, "turrets") == 0) {
		*out = XWA_WEAPON_IFF_SWAP_TURRETS;
	} else if (strcmp(text, "none") == 0) {
		*out = XWA_WEAPON_IFF_SWAP_NONE;
	} else if (strcmp(text, "all") == 0) {
		*out = XWA_WEAPON_IFF_SWAP_ALL;
	} else {
		return 0;
	}
	return 1;
}

XwaWeaponIffSwap XwaMissionSettings_WeaponIffSwap(void) { return s_weaponIffSwap; }

int XwaMissionSettings_AiAttackUsesMaxSpeedCache(void) { return s_aiAttackUsesMaxSpeedCache; }

int XwaMissionSettings_FireNeedsDechargeRate(void) { return s_fireNeedsDechargeRate; }

int XwaMissionSettings_NoStarshipFireFromDestroyedRacks(void) { return s_noStarshipFireFromDestroyedRacks; }

int XwaMissionSettings_StarshipAmbientSounds(void) { return s_starshipAmbientSounds; }

int XwaMissionSettings_NoPlayerWarheadSlotCap(void) { return s_noPlayerWarheadSlotCap; }

int XwaMissionSettings_GunnerWeaponSelectInSeat(void) { return s_gunnerWeaponSelectInSeat; }

int XwaMissionSettings_GunnerFiresSelectedGroup(void) { return s_gunnerFiresSelectedGroup; }

int XwaMissionSettings_GunnerWarheadsNeedCockpitHardpoint(void) {
	return s_gunnerWarheadsNeedCockpitHardpoint;
}

int XwaMissionSettings_NoFireFromDestroyedHardpoints(void) { return s_noFireFromDestroyedHardpoints; }

int XwaMissionSettings_TieFighterEnergyStepAlwaysHigh(void) { return s_tieFighterEnergyStepAlwaysHigh; }

int XwaMissionSettings_PlayerTurretRacksKeepCharge(void) { return s_playerTurretRacksKeepCharge; }

int XwaMissionSettings_RearmFollowsBothStatuses(void) { return s_rearmFollowsBothStatuses; }

int XwaMissionSettings_WarheadCollisionBonusAnyTarget(void) { return s_warheadCollisionBonusAnyTarget; }

int XwaMissionSettings_TurbolaserHardpointTypes(void) { return s_turbolaserHardpointTypes; }

int XwaMissionSettings_GunTurretHardpoints(void) { return s_gunTurretHardpoints; }

int XwaMissionSettings_PlayerWeaponJamming(void) { return s_playerWeaponJamming; }

int XwaMissionSettings_SuperStarDestroyerJamming(void) { return s_superStarDestroyerJamming; }

int XwaMissionSettings_TwoLauncherFixes(void) { return s_twoLauncherFixes; }

int XwaMissionSettings_StarshipMissileDefenseAnyWarhead(void) { return s_missileDefenseAnyWarhead; }

int XwaMissionSettings_WeaponRackCountFromRecord(void) { return s_rackCountFromRecord; }

int XwaMissionSettings_OverlappingFlyBySounds(void) { return s_overlappingFlyBySounds; }
int XwaMissionSettings_HangarDroidPartsFromModel(void) { return s_hangarDroidPartsFromModel; }

int XwaMissionSettings_RedAlert(void) { return s_settings.redAlert; }

int XwaMissionSettings_HideHyperspaceExitMessages(void) { return s_settings.hideHyperspaceExitMessages; }

int XwaMissionSettings_HideFriendlyArrivalMessages(void) { return s_settings.hideFriendlyArrivalMessages; }

int XwaMissionSettings_SkipArrivalMessages(int iff) {
	switch (s_settings.skipArrivalIff) {
		case SETTINGS_IFF_STOCK:
			return -1;
		case SETTINGS_IFF_NONE:
			return 0;
		case SETTINGS_IFF_ALL:
			return 1;
		default:
			return s_settings.skipArrivalIff == iff;
	}
}

int XwaMissionSettings_ForcePlayerInTurret(void) { return s_settings.forceTurret; }

int XwaMissionSettings_ForcePlayerInTurretAt(int hours, int minutes, int seconds) {
	if (s_settings.forceTurret <= 0) {
		return s_settings.forceTurret;
	}
	return hours == s_settings.turretHours && minutes == s_settings.turretMinutes &&
		   seconds == s_settings.turretSeconds;
}

int XwaMissionSettings_PlayerLasersDisabled(void) { return s_settings.disableLasers; }

int XwaMissionSettings_PlayerWarheadsDisabled(void) { return s_settings.disableWarheads; }

int XwaMissionSettings_WarheadCollisionDamage(void) { return s_settings.warheadCollisionDamage; }

int XwaMissionSettings_DamageThroughShieldsOnHard(void) { return s_settings.damageThroughShields; }

int XwaMissionSettings_ProximityTriggersCheckCraftOnly(void) { return s_settings.proximityCraftOnly; }

int XwaMissionSettings_UseHyperspaceSpeeds(void) { return s_settings.useHyperspaceSpeeds; }

const XwaHyperspaceRegionRule* XwaMissionSettings_HyperspaceRegion(int region) {
	if (region < 0 || region >= XWA_HYPERSPACE_REGIONS || !s_settings.hasRegionRule[region]) {
		return NULL;
	}
	return &s_settings.regionRules[region];
}

/* Whether the O key may target object `objIdx` for player `player`: a craft
   of a listed flight group in the player's region, not yet inspected by the
   player's team when the setting asks for it. */
static int MissionSettings_IsTargetCraft(int objIdx, const PlayerData* player) {
	const ObjectRecord* object = &g_objectTable[objIdx];
	const CraftData* craft;

	if (object->objectType == OBJ_None || object->regionIdx != player->regionIndex || object->mobj == NULL ||
		object->mobj->pCraft == NULL) {
		return 0;
	}
	craft = object->mobj->pCraft;
	if (s_settings.targetOnlyNotInspected && player->playerIff >= 0 && player->playerIff < 10 &&
		(int8_t)craft->iffVisibility[player->playerIff] >= 1) {
		return 0;
	}
	return MissionSettings_Has(s_settings.targetFlightGroups, object->flightGroupIdx);
}

/* The nearest such craft, or -1. Without a craft, the player's distance is
   from its map camera. */
static int MissionSettings_NearestTarget(int playerIdx) {
	const PlayerData* player = &g_players[playerIdx];
	unsigned int bestDistance = 0xFFFFFFFFu;
	int best = -1;
	uint32_t objIdx;

	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
		if (!MissionSettings_IsTargetCraft((int)objIdx, player)) {
			continue;
		}
		if (player->objectIndex == 0xFFFF) {
			Mission_ResolveObjectOrMissionPointWorldLoc(objIdx, 0, 0, 0);
			trig2_ctop(worldlocx - player->viewState.savedTargetX, worldlocy - player->viewState.savedTargetY,
					   worldlocz - player->viewState.savedTargetZ);
		} else {
			pai_ObjectRefDirectionToObjectRef((unsigned int)player->objectIndex, objIdx);
		}
		if ((unsigned int)trig2_polardistance < bestDistance) {
			best = (int)objIdx;
			bestDistance = (unsigned int)trig2_polardistance;
		}
	}
	return best;
}

/* The next such craft after the one targeted last, or -1. */
static int MissionSettings_CycleTarget(int playerIdx) {
	const PlayerData* player = &g_players[playerIdx];
	const int first = (int)g_activeRegionObjectSlotStart;
	const int end = (int)g_activeRegionCraftObjectSlotEnd;
	int objIdx = s_lastTargetObjIdx[playerIdx];
	int step;

	if (objIdx < first || objIdx >= end) {
		objIdx = first - 1;
	}
	for (step = 0; step < end - first; ++step) {
		if (++objIdx >= end) {
			objIdx = first;
		}
		s_lastTargetObjIdx[playerIdx] = objIdx;
		if (MissionSettings_IsTargetCraft(objIdx, player)) {
			return objIdx;
		}
	}
	return -1;
}

int XwaMissionSettings_TargetKey(int playerIdx) {
	int target;

	if (!s_settings.hasTargetKey || playerIdx < 0 || playerIdx >= XWA_PLAYER_COUNT) {
		return 0;
	}
	target = s_settings.targetCycle ? MissionSettings_CycleTarget(playerIdx)
									: MissionSettings_NearestTarget(playerIdx);
	if (target >= 0) {
		Player_SetTarget((uint16_t)target, (unsigned int)playerIdx);
	}
	return 1;
}

int XwaMissionSettings_OrderOfBattleListed(int flightGroupIdx) {
	if (!s_settings.hasOrderOfBattleList) {
		return -1;
	}
	return MissionSettings_Has(s_settings.orderOfBattleFlightGroups, flightGroupIdx);
}

const char* XwaMissionSettings_OrderOfBattleName(int flightGroupIdx) {
	int i;

	for (i = 0; i < s_settings.nameCount; ++i) {
		if (s_settings.names[i].flightGroup == flightGroupIdx) {
			return s_settings.names[i].text;
		}
	}
	return NULL;
}

int XwaMissionSettings_OrderOfBattleLineCount(void) { return s_settings.lineCount; }

const char* XwaMissionSettings_OrderOfBattleLine(int index) {
	return index >= 0 && index < s_settings.lineCount ? s_settings.lines[index] : "";
}

int XwaMissionSettings_OrderOfBattleLinesTop(void) { return s_settings.linesTop; }
