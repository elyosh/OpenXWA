#ifndef XWA_UTIL_RANDOM_H
#define XWA_UTIL_RANDOM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Math_SeedRandom(uint16_t seed);
uint16_t GameRand_SetSavedSeed(uint16_t seed);
int16_t  GameRand_SetSecondarySeed(int16_t seed);
void GameRand_SavePrimarySeed(void);
void GameRand_RestorePrimarySeed(void);
uint16_t GameRand_GetPrimarySeed(void);
uint16_t GameRand_GetSecondarySeed(void);
uint16_t GameRand_GetSavedSeed(void);
uint16_t GameRand(void);
uint16_t GameRandRange(uint16_t modulus);
uint16_t GameRand2(void);

#ifdef __cplusplus
}
#endif

#endif
