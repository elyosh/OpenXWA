#ifndef XWA_INPUT_FORCEFEEDBACK_H
#define XWA_INPUT_FORCEFEEDBACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t ForceFeedback_CheckDevice(void);
int ForceFeedback_Init(void);
void ForceFeedback_ShutdownDevice(void);
unsigned int ForceFeedback_GetStrength(void);
unsigned int ForceFeedback_GetCenteringStrength(void);
uint8_t ForceFeedback_SetStrength(unsigned int strength);
int ForceFeedback_SetCenteringStrength(unsigned int centerStrength);
void ForceFeedback_StopAllEffects(void);
int ForceFeedback_EnableEffects(void);
void ForceFeedback_PlayProximityEffectForObject(int proximityEffectKind, unsigned int objIdx);
void ForceFeedback_PlayImpactEffect(int directionDegrees, int impactMagnitude);
void ForceFeedback_PlayLongDirectionalDamageEffect(int directionDegrees);
void ForceFeedback_PlayShortDirectionalDamageEffect(int directionDegrees);
void ForceFeedback_PlayCriticalDamageEffect(void);
void ForceFeedback_PlayCraftDestructionEffect(void);
void ForceFeedback_PlayLaserFireEffect(void);
void ForceFeedback_PlayWarheadFireEffect(void);
void ForceFeedback_PlayBoardOrPickupReleaseEffect(void);
void ForceFeedback_PlayHyperspaceOutboundEffect(void);
void ForceFeedback_PlayHyperspaceInboundEffect(void);
void ForceFeedback_UpdateActiveEffects(int elapsedTicks);

#ifdef __cplusplus
}
#endif

#endif
