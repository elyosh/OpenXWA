#include "xwa_runtime/models/object_limits.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/flight/object/object.h"
#include "xwa_runtime/models/mission_file.h"

#include <string.h>

/* Modern port module; no original counterpart. */

#define LIMITS_FLIGHT_YAML "flight/flight.yaml"

enum {
	LIMITS_STOCK_CRAFT = 96,
	LIMITS_STOCK_PLAYER_PROJECTILES = 12,
	LIMITS_STOCK_AI_PROJECTILES = 64,
	LIMITS_STOCK_EXPLOSIONS = 32,
	LIMITS_STOCK_RADAR_BLIPS = 48,
	/* The proving grounds' slots, which the counts don't change. */
	LIMITS_PROVING_CRAFT = 36,
	LIMITS_PROVING_SALVAGE = 224,
	/* The static objects of each region, and the local slots after the
	   regions. */
	LIMITS_STATIC_PER_REGION = 128,
	LIMITS_LOCAL_SLOTS = 32,
};

/* The counts. 0 means the stock count. */
typedef struct ObjectLimits {
	uint32_t craft, playerProjectiles, aiProjectiles, explosions, radarBlips;
} ObjectLimits;

static ObjectLimits s_defaults;
static ObjectLimits s_limits;

static void ObjectLimits_ReadCount(const char* path, const AeronConfigNode* section, const char* key,
								   uint32_t max, uint32_t* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(section, key);
	int64_t value;

	if (node == NULL) {
		return;
	}
	value = AeronConfigNode_Int(node, 0);
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 1 || value > (int64_t)max) {
		Aeron_LogWarn("xwa.mission", "%s:%d: object_limits.%s must be a number from 1 to %u; it is ignored",
					  path, AeronConfigNode_Line(node), key, max);
		return;
	}
	*out = (uint32_t)value;
}

static void ObjectLimits_Read(const char* path, const AeronConfigNode* section, ObjectLimits* out) {
	size_t i;

	if (section == NULL) {
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.mission", "%s:%d: object_limits must be a map; it is ignored", path,
					   AeronConfigNode_Line(section));
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(section); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(section, i);

		if (strcmp(key, "craft_per_region") != 0 && strcmp(key, "player_projectiles") != 0 &&
			strcmp(key, "ai_projectiles_per_region") != 0 && strcmp(key, "explosions_per_region") != 0 &&
			strcmp(key, "radar_blips") != 0) {
			Aeron_LogWarn("xwa.mission", "%s:%d: object_limits.%s isn't a setting; it is ignored", path,
						  AeronConfigNode_Line(AeronConfigNode_MapValueAt(section, i)), key);
		}
	}
	ObjectLimits_ReadCount(path, section, "craft_per_region", XWA_MAX_OBJECTS, &out->craft);
	ObjectLimits_ReadCount(path, section, "player_projectiles", XWA_MAX_OBJECTS, &out->playerProjectiles);
	ObjectLimits_ReadCount(path, section, "ai_projectiles_per_region", XWA_MAX_OBJECTS, &out->aiProjectiles);
	ObjectLimits_ReadCount(path, section, "explosions_per_region", XWA_MAX_OBJECTS, &out->explosions);
	ObjectLimits_ReadCount(path, section, "radar_blips", HUD_RADAR_MAX_BLIPS, &out->radarBlips);
}

void XwaObjectLimits_Init(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* version;

	memset(&s_defaults, 0, sizeof s_defaults);
	s_limits = s_defaults;
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, LIMITS_FLIGHT_YAML, &file, &error)) {
		/* The backdrops code already logs this error. */
		return;
	}
	root = AeronConfigFile_Root(file);
	version = AeronConfigNode_MapGet(root, "version");
	if (AeronConfigNode_Type(version) == AERON_CONFIG_INT && AeronConfigNode_Int(version, 0) == 1) {
		ObjectLimits_Read(LIMITS_FLIGHT_YAML, AeronConfigNode_MapGet(root, "object_limits"), &s_defaults);
	}
	s_limits = s_defaults;
	AeronConfigFile_Destroy(file);
}

static uint32_t ObjectLimits_Or(uint32_t value, uint32_t stock) { return value != 0 ? value : stock; }

void XwaObjectLimits_LoadMission(const char* missionPath, uint32_t regions, uint32_t players,
								 int provingGrounds) {
	AeronConfigFile* config = NULL;
	char path[300];
	uint32_t craft, salvage, shared, perRegion, available, excess;

	s_limits = s_defaults;
	if (missionPath != NULL && XwaMissionFile_Load(missionPath, &config, path, sizeof path) > 0) {
		ObjectLimits_Read(path, AeronConfigNode_MapGet(AeronConfigFile_Root(config), "object_limits"),
						  &s_limits);
		AeronConfigFile_Destroy(config);
	}
	if (regions == 0 || players == 0) {
		return;
	}
	s_limits.craft = ObjectLimits_Or(s_limits.craft, LIMITS_STOCK_CRAFT);
	s_limits.playerProjectiles = ObjectLimits_Or(s_limits.playerProjectiles, LIMITS_STOCK_PLAYER_PROJECTILES);
	s_limits.aiProjectiles = ObjectLimits_Or(s_limits.aiProjectiles, LIMITS_STOCK_AI_PROJECTILES);
	s_limits.explosions = ObjectLimits_Or(s_limits.explosions, LIMITS_STOCK_EXPLOSIONS);
	s_limits.radarBlips = ObjectLimits_Or(s_limits.radarBlips, LIMITS_STOCK_RADAR_BLIPS);

	/* The slots of one region, as Mission_Init lays them out. */
	craft = provingGrounds ? LIMITS_PROVING_CRAFT : s_limits.craft;
	salvage = provingGrounds ? LIMITS_PROVING_SALVAGE : 0;
	shared = players == 1 ? 16 : 32;
	perRegion = craft + salvage + shared + players * s_limits.playerProjectiles + s_limits.aiProjectiles +
				2 * s_limits.explosions + LIMITS_STATIC_PER_REGION;
	available = (XWA_MAX_OBJECTS - LIMITS_LOCAL_SLOTS) / regions;
	if (perRegion <= available) {
		return;
	}
	excess = perRegion - available;
	{
		/* What to lower, in order, down to which count, and how many slots of
		   a region each step of it frees. */
		const struct {
			uint32_t* count;
			uint32_t floor;
			uint32_t slots;
		} steps[] = {
			{ &s_limits.craft, provingGrounds ? s_limits.craft : LIMITS_STOCK_CRAFT, 1 },
			{ &s_limits.aiProjectiles, LIMITS_STOCK_AI_PROJECTILES, 1 },
			/* Each explosion slot comes with a debris slot. */
			{ &s_limits.explosions, LIMITS_STOCK_EXPLOSIONS, 2 },
			{ &s_limits.playerProjectiles, LIMITS_STOCK_PLAYER_PROJECTILES, players },
		};
		size_t i;

		for (i = 0; excess != 0 && i < sizeof steps / sizeof steps[0]; ++i) {
			const uint32_t room = *steps[i].count > steps[i].floor ? *steps[i].count - steps[i].floor : 0;
			uint32_t lower = (excess + steps[i].slots - 1) / steps[i].slots;

			if (lower > room) {
				lower = room;
			}
			*steps[i].count -= lower;
			excess = lower * steps[i].slots >= excess ? 0 : excess - lower * steps[i].slots;
		}
	}
	Aeron_LogWarn(
		"xwa.mission",
		"the object counts need %u slots in each of the %u regions, more than the %u that fit: they "
		"are lowered to %u craft, %u projectiles for each player, %u projectiles for the AI and %u "
		"explosions",
		perRegion, regions, available, s_limits.craft, s_limits.playerProjectiles, s_limits.aiProjectiles,
		s_limits.explosions);
}

uint32_t XwaObjectLimits_CraftPerRegion(void) { return ObjectLimits_Or(s_limits.craft, LIMITS_STOCK_CRAFT); }

uint32_t XwaObjectLimits_PlayerProjectiles(void) {
	return ObjectLimits_Or(s_limits.playerProjectiles, LIMITS_STOCK_PLAYER_PROJECTILES);
}

uint32_t XwaObjectLimits_AiProjectilesPerRegion(void) {
	return ObjectLimits_Or(s_limits.aiProjectiles, LIMITS_STOCK_AI_PROJECTILES);
}

uint32_t XwaObjectLimits_ExplosionsPerRegion(void) {
	return ObjectLimits_Or(s_limits.explosions, LIMITS_STOCK_EXPLOSIONS);
}

uint32_t XwaObjectLimits_RadarBlips(void) {
	return ObjectLimits_Or(s_limits.radarBlips, LIMITS_STOCK_RADAR_BLIPS);
}
