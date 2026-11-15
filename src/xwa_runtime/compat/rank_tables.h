#ifndef XWA_RUNTIME_COMPAT_RANK_TABLES_H
#define XWA_RUNTIME_COMPAT_RANK_TABLES_H

/* The AI rank tables, for any rank from 0 to 255.
 *
 * A flight group's AI rank (groupAI) goes from 0 to 5, but a mission file
 * can hold any byte, and some mods have flight groups with 255. The original
 * reads its rank tables with the rank as it is, so a rank past the end of a
 * table reads the data that follows it. In C, that would be undefined
 * behavior, so the port calls these functions instead.
 *
 * Each function returns the table's entry for a rank inside the table, and
 * what the original reads for a rank past it where that data never changes.
 * Where the original reads data that changes at run time, ranks 254 and 255
 * read the same data in the port: they are the ranks that mods use, 255 in
 * the mission files and 254 after the Easy difficulty takes one rank from it.
 * Other such ranks return the table's last entry. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* g_aiSkillValueQ16ByLevel. Past rank 87, the original reads the craft type
   table. */
uint16_t XwaRankTables_SkillValue(unsigned int rank);
/* g_aiThinkIntervalByGroupAI. Past rank 79, the original reads the craft
   type table. */
uint16_t XwaRankTables_ThinkInterval(unsigned int rank);
/* g_aiAvoidAttackerDelaySecondsByGroupAI. Past it, the original reads the
   formation tables, for every rank. */
uint16_t XwaRankTables_AvoidAttackerDelaySeconds(unsigned int rank);
/* g_aiAvoidAttackerDelayFracQ16ByGroupAI. Past it, the original reads MFD
   state, then the particle templates. */
uint16_t XwaRankTables_AvoidAttackerDelayFraction(unsigned int rank);
/* g_warheadLeadErrorModulusByAiLevel and g_warheadFireChanceThresholdByAiLevel. */
uint16_t XwaRankTables_WarheadLeadError(unsigned int rank);
uint16_t XwaRankTables_WarheadFireChance(unsigned int rank);
/* g_defaultPilotRatingByAiLevel and g_pilotKillScoreBaseByAiLevel. */
int XwaRankTables_DefaultPilotRating(unsigned int rank);
int XwaRankTables_KillScoreBase(unsigned int rank);

#ifdef __cplusplus
}
#endif

#endif
