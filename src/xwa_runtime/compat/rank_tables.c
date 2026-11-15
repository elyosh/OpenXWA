#include "xwa_runtime/compat/rank_tables.h"

#include "xwa/assets/object_type.h"
#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/laser.h"
#include "xwa/render/effects.h"

#include <stdint.h>

/* Modern port module; no original counterpart. */

#include "rank_table_overflow.inc"

#define RANK_TABLE_COUNT(table) (sizeof(table) / sizeof((table)[0]))

/* The entry of `table` for `rank`, the stock value past it from `overflow`,
   or the table's last entry. */
#define RANK_TABLE_VALUE(table, overflow, rank)                                                              \
	((rank) < RANK_TABLE_COUNT(table) ? (table)[rank]                                                        \
	 : (rank) - RANK_TABLE_COUNT(table) < RANK_TABLE_COUNT(overflow)                                         \
		 ? (overflow)[(rank) - RANK_TABLE_COUNT(table)]                                                      \
		 : (table)[RANK_TABLE_COUNT(table) - 1])

enum {
	/* The first rank of the skill and think tables that reads the craft type
	   table, which follows them at 0x5B0F70. */
	SKILL_CRAFT_TYPE_RANK = 88,
	THINK_CRAFT_TYPE_RANK = 80,
	/* The particle template that ranks 254 and 255 of the avoid-attacker
	   fraction read: the two halves of its renderFlags. */
	FRACTION_TEMPLATE = 2,
};

/* The rank of a mod's flight group that the port reads from data that
   changes at run time. */
static int RankTables_Live(unsigned int rank) { return rank == 254 || rank == 255; }

uint16_t XwaRankTables_SkillValue(unsigned int rank) {
	if (rank >= SKILL_CRAFT_TYPE_RANK && RankTables_Live(rank)) {
		return g_objectTypeTables.craftTypeToObjectType[rank - SKILL_CRAFT_TYPE_RANK];
	}
	return RANK_TABLE_VALUE(g_aiSkillValueQ16ByLevel, k_skillValueOverflow, rank);
}

uint16_t XwaRankTables_ThinkInterval(unsigned int rank) {
	if (rank >= THINK_CRAFT_TYPE_RANK && RankTables_Live(rank)) {
		return g_objectTypeTables.craftTypeToObjectType[rank - THINK_CRAFT_TYPE_RANK];
	}
	return RANK_TABLE_VALUE(g_aiThinkIntervalByGroupAI, k_thinkIntervalOverflow, rank);
}

uint16_t XwaRankTables_AvoidAttackerDelaySeconds(unsigned int rank) {
	const unsigned int count = RANK_TABLE_COUNT(g_aiAvoidAttackerDelaySecondsByGroupAI);
	const unsigned int formation = rank - count;
	const unsigned int sideCount = sizeof(g_formPosX) / sizeof(g_formPosX[0][0]);

	if (rank < count) {
		return g_aiAvoidAttackerDelaySecondsByGroupAI[rank];
	}
	/* g_formPosX follows the table at 0x5B7080, then g_formPosY. */
	if (formation < sideCount) {
		return (uint16_t)g_formPosX[0][formation];
	}
	return (uint16_t)g_formPosY[0][formation - sideCount];
}

uint16_t XwaRankTables_AvoidAttackerDelayFraction(unsigned int rank) {
	const unsigned int count = RANK_TABLE_COUNT(g_aiAvoidAttackerDelayFracQ16ByGroupAI);
	const uint32_t renderFlags = g_particleEffectTemplates[FRACTION_TEMPLATE].renderFlags;

	if (rank < count) {
		return g_aiAvoidAttackerDelayFracQ16ByGroupAI[rank];
	}
	if (rank == 254) {
		return (uint16_t)renderFlags;
	}
	if (rank == 255) {
		return (uint16_t)(renderFlags >> 16);
	}
	return g_aiAvoidAttackerDelayFracQ16ByGroupAI[count - 1];
}

uint16_t XwaRankTables_WarheadLeadError(unsigned int rank) {
	return RANK_TABLE_VALUE(g_warheadLeadErrorModulusByAiLevel, k_warheadLeadErrorOverflow, rank);
}

uint16_t XwaRankTables_WarheadFireChance(unsigned int rank) {
	return RANK_TABLE_VALUE(g_warheadFireChanceThresholdByAiLevel, k_warheadFireChanceOverflow, rank);
}

int XwaRankTables_DefaultPilotRating(unsigned int rank) {
	/* Ranks 46 and 47 read g_sound3DEnabled and g_maxActiveSounds. */
	if (rank == 46 || rank == 47) {
		return g_defaultPilotRatingByAiLevel[RANK_TABLE_COUNT(g_defaultPilotRatingByAiLevel) - 1];
	}
	return RANK_TABLE_VALUE(g_defaultPilotRatingByAiLevel, k_pilotRatingOverflow, rank);
}

int XwaRankTables_KillScoreBase(unsigned int rank) {
	return RANK_TABLE_VALUE(g_pilotKillScoreBaseByAiLevel, k_killScoreBaseOverflow, rank);
}
