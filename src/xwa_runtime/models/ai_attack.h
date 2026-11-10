#ifndef XWA_RUNTIME_MODELS_AI_ATTACK_H
#define XWA_RUNTIME_MODELS_AI_ATTACK_H

/* How an AI craft attacks, from the `ai_attack` group of its model settings
 * file and the `ai_attack` section of flight/flight.yaml:
 *   - break_range_m: how close the craft gets to its target before it breaks
 *     off the attack run;
 *   - laser_range_m: from how far it fires its lasers, by the flight group's
 *     rank;
 *   - avoid_hits: whether it dodges warheads and attackers.
 *
 * Each value comes from the AI craft's model first, then from flight.yaml,
 * then from the stock game. The values belong to the model that the object
 * type loads in the mission, so a swapped model has its own. The file has
 * the ranges in metres, and the values here are in game units. Everything
 * here changes the simulation. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

enum { XWA_AI_ATTACK_RANKS = 6 };

typedef enum XwaAiAttackValue {
	/* A target that isn't a craft, such as a warhead. */
	XWA_AI_ATTACK_BREAK_WARHEAD,
	/* Small targets, by rank: XWA_AI_ATTACK_BREAK_SMALL + rank. */
	XWA_AI_ATTACK_BREAK_SMALL,
	/* The cap of the break range against a whole starship, platform,
	   container or freighter. */
	XWA_AI_ATTACK_BREAK_BIG_MAX = XWA_AI_ATTACK_BREAK_SMALL + XWA_AI_ATTACK_RANKS,
	/* The laser range, by rank: XWA_AI_ATTACK_LASER_RANGE + rank. */
	XWA_AI_ATTACK_LASER_RANGE,
	/* 1 when the craft dodges warheads and attackers, else 0. */
	XWA_AI_ATTACK_AVOID_HITS = XWA_AI_ATTACK_LASER_RANGE + XWA_AI_ATTACK_RANKS,
	XWA_AI_ATTACK_VALUES
} XwaAiAttackValue;

/* Reads the `ai_attack` section of flight/flight.yaml. Called once at
   startup. */
void XwaAiAttack_Init(void);
/* Drops the previous mission's values. Called by the model settings reader. */
void XwaAiAttack_ClearModelSettings(void);
/* Reads the `ai_attack` group of `objectType`'s model settings file, or
   does nothing when `group` is NULL. `path` is the file, for the log.
   Called by the model settings reader. */
void XwaAiAttack_ReadModelSettings(unsigned int objectType, const char* path,
								   const struct AeronConfigNode* group);

/* Returns `value` for the AI craft `aiObjIdx`, or `stock` when neither its
   model nor flight.yaml sets it. */
int32_t XwaAiAttack_Value(unsigned int aiObjIdx, XwaAiAttackValue value, int32_t stock);

/* Returns the break range offset of the AI craft `aiObjIdx` for the target
   `targetObjIdx`, by the model that the target loads, or `stock` when
   neither the AI craft's model nor flight.yaml sets one for it. */
int32_t XwaAiAttack_TargetOffset(unsigned int aiObjIdx, unsigned int targetObjIdx, int32_t stock);

/* Writes into `*range` the laser range of the AI craft `aiObjIdx` for the
   flight group rank `rank`, and returns 1. Returns 0 for the stock rule, by
   the craft's skill tier, which applies when neither the craft's model nor
   flight.yaml sets a laser range. */
int XwaAiAttack_LaserRange(unsigned int aiObjIdx, unsigned int rank, uint32_t* range);

#ifdef __cplusplus
}
#endif

#endif
