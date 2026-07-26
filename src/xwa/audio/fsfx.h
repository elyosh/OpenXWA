#ifndef XWA_AUDIO_FSFX_H
#define XWA_AUDIO_FSFX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FsfxVoiceVariantReplacement {
	int fromVariant;
	int toVariant;
} FsfxVoiceVariantReplacement;

typedef struct FsfxMissionVoiceFgSlot {
	int16_t flightGroupIdx;
	int16_t reserved;
} FsfxMissionVoiceFgSlot;

typedef enum FlightSfxSlot {
	SFX_TARGET_INSPECT_CMP  = 69,
	SFX_ENGINE_WASH_CAPITAL = 113,
	SFX_ENGINE_WASH_OTHER   = 114,
	SFX_REPORT_REQUEST      = 127,
	SFX_AUTOGUNNER_TOGGLE   = 133,
} FlightSfxSlot;

extern unsigned char                     g_fsfxVoiceQueueCount;
extern int                               g_fsfxCurrentVoiceSfxSlot;
extern uint8_t                           g_fsfxLoaded;
extern uint8_t                           g_fsfxCurrentVoiceSpeakerType;
extern uint8_t                           g_fsfxCurrentVoiceCategory;
extern uint16_t                          g_fsfxCurrentVoiceObjectSerial;
extern uint8_t                           g_fsfxCurrentVoiceChainFlag;
extern int                               g_nextNearbyWeaponSfxScanTime;
extern int                               g_targetingToneLastSeekBeepTime;
extern uint8_t                           g_targetingToneWeaponReadyQueued;
extern int                               g_playerEngineLoopObjectType;
extern int                               g_playerEngineLoopVolume;
extern int                               g_playerEngineLoopFrequency;
extern int                               g_fsfxSmallExplosionRemainingChoices;
extern uint8_t                           g_fsfxSmallExplosionUsedFlags[8];
extern uint8_t                           g_fsfxEnemyWarheadAttackCalloutPlayed;
extern uint8_t                           g_fsfxEnemyFighterAttackCalloutPlayed;
extern uint16_t                          g_fsfxLocalPlayerTargetedProjectileFriendlyHitCount;
extern int                               g_unusedFsfxVoiceLoadState;
extern uint8_t                           g_fsfxVoiceQueueChainFlag[128];
extern uint16_t                          g_fsfxVoiceQueueObjectSerial[128];
extern uint8_t                           g_fsfxVoiceQueueSpeakerType[128];
extern uint8_t                           g_fsfxVoiceQueueCategory[128];
extern int                               g_fsfxVoiceQueueSfxSlot[128];
extern int                               g_fsfxVoiceQueueObjIdx[128];
extern int                               g_fsfxTacOfficerLastSpeakSecondsByObj[1664];
extern uint8_t                           g_fsfxVoiceLinePlayCounts[12][196];
extern const uint8_t                     g_fsfxDesignationToVoiceVariant[24];
extern const uint8_t                     g_fsfxHostileVoiceVariantRemap[16];
extern const FsfxVoiceVariantReplacement g_fsfxRandomVoiceVariantReplacementPairs[18];
extern FsfxMissionVoiceFgSlot            g_fsfxMissionVoiceFgSlots[16];
extern int                               g_fsfxMissionVoiceBaseSfxSlot[16];
extern int                               g_fsfxLoadedWingmanVoiceCount;
extern char                              g_fsfxSfxNameTable[2872][32];
extern int                               g_incomingMissileWarningState;
extern int16_t                           g_incomingMissileLockStrength;
extern int                               g_fsfxVoiceLoadFilter;
extern uint16_t                          g_fsfxMinDistanceOrRolloffBySfxSlot[196];
extern int                               g_fsfxDefaultMinDistanceOrRolloff;
extern uint8_t                           g_fsfxBaseVolumeBySfxSlot[196];
extern uint8_t                           g_fsfxDefaultBaseVolume;
extern const uint8_t                     g_fsfxVoiceCategoryBaseOffset[48];
extern const uint8_t                     g_fsfxVoiceCategoryVariantCount[48];
extern const uint8_t                     g_fsfxVoiceCategoryRepeatThreshold[48];
extern const uint16_t                    g_fsfxVoiceCategoryLoadFlags[42];
extern char                              g_fsfxWaveDir[8];
extern char                              g_fsfxSfxLoadPath[128];
extern char                              g_fsfxWingmanVoiceListPathBySlot[12][128];

/* Previous player object positions; written by player code and consumed by the
   flight-SFX Doppler/velocity path. Part of the XWA fsfx .data block. Declared
   without a bound to avoid coupling this header to the player-count macro; the
   definition in fsfx.c uses XWA_PLAYER_COUNT. */
extern int g_objPrevX[];
extern int g_objPrevY[];
extern int g_objPrevZ[];

void         fsfx_UnloadAllEffects_Thunk(void);
void         fsfx_ResetFlightSfxState(int clearSfxIdTable);
int          fsfx_IsVoiceQueueEmpty(void);
int          fsfx_ClearSfxNameTable(void);
int          fsfx_LoadSfxList(char* fileName, int baseSoundId, const char* waveDir);
void         fsfx_LoadMissionVoiceSfx(void);
int          fsfx_PickRandomSmallExplosionSfx(void);
int          fsfx_SelectTacOfficerObjectVoiceVariant(unsigned int objIdx);
int          fsfx_speakorderack(int playerIdx, int speakerObjIdx, int voiceCategory, int voiceVariant,
								unsigned int relatedObjIdx, uint16_t probability);
int          fsfx_SpeakTacticalOfficerEvent(int voiceCategory, int messageId, unsigned int objIdx,
											unsigned int probability);
int          fsfx_PlaySound(int sfxSlot, unsigned int sourceObjOrPointRef, unsigned int playerIdx);
int          fsfx_triggerweaponsfx(unsigned int sourceObjIdx, unsigned int playerIdx);
char         fsfx_ShouldSuppressFlightSfx(unsigned int sfxSlot, int playerIdx);
unsigned int fsfx_ComputeSourceVolume(unsigned int sourceObjOrPointRef, unsigned int sfxSlot,
									  unsigned int* outPriority);
unsigned int fsfx_ComputeDistanceVolume(int dx, int dy, int dz, unsigned int sfxSlot,
										unsigned int* outPriority);
int          fsfx_ComputeSoftwarePan(int dx, int dy, int dz, unsigned int* volumeInOut);
int          fsfx_ComputeSourcePan(unsigned int sourceObjOrPointRef, unsigned int* volumeInOut);
int          fsfx_QueueVoiceSfx(int sfxSlot, uint8_t speakerType, uint8_t voiceCategory, uint8_t chainFlag,
								uint16_t objectSerial, int objIdx);
void         fsfx_RemoveVoiceQueueEntryChain(unsigned int queueIndex);
void         fsfx_UpdateVoiceQueue(void);
void         fsfx_UpdateMissileThreatWarning(void);
void         fsfx_UpdateIncomingMissileWarning(int warningState);
int          fsfx_UpdateTargetingTone(int state);
void         fsfx_UpdatePlayerEngineLoop(void);
void         fsfx_UpdateChaffLoop(void);
int          fsfx_PlaySfxAtWorldPosition(unsigned int sfxSlot, float pitchScale, int worldX, int worldY,
										 int worldZ, int playerIdx);
void         fsfx_UpdateBeamSystemLoop(int enabled, unsigned int playerIdx);
void         fsfx_UpdateBeamEffectLoops(void);
void         fsfx_UpdateFlightSfx(void);
void         fsfx_UpdateNearbyWeaponLoop(void);
void fsfx_StopHyperZoomImp(unsigned int playerIdx);

#ifdef __cplusplus
}
#endif

#endif
