#ifndef XWA_AUDIO_MUSIC_H
#define XWA_AUDIO_MUSIC_H

#include <stdint.h>

#include "xwa/audio/imuse/imuse.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	MUSIC_STATE_NONE = 0,
	MUSIC_STATE_DEFAULT_ALIAS = 1011,
	MUSIC_STATE_NO_ENEMIES_INTRO = 1100,
	MUSIC_STATE_NO_ENEMIES_INTRO_ALT = 1102,
	MUSIC_STATE_NO_ENEMIES_CALM = 1106,
	MUSIC_STATE_NO_ENEMIES_CALM_ALT = 1108,
	MUSIC_STATE_CONFLICT = 1110,
	MUSIC_STATE_1115 = 1115,
	MUSIC_STATE_1120 = 1120,
	MUSIC_STATE_PANIC = 1125,
	MUSIC_STATE_PANIC_ALT = 1127,
	MUSIC_STATE_COMBAT_STEADY = 1130,
	MUSIC_STATE_COMBAT_STEADY_ALT = 1132,
	MUSIC_STATE_COMBAT_ACTIVE = 1135,
	MUSIC_STATE_COMBAT_ACTIVE_ALT = 1140,
	MUSIC_STATE_1145 = 1145,
	MUSIC_STATE_CLIMAX = 1150,
	MUSIC_STATE_1155 = 1155,
	MUSIC_STATE_MISSION_LOSS_ALT = 1160,
	MUSIC_STATE_MISSION_SUCCESS = 1165,
	MUSIC_STATE_MISSION_SUCCESS_ALT = 1170,
	MUSIC_STATE_FRONTEND_1200 = 1200,
	MUSIC_STATE_FRONTEND_1210 = 1210,
	MUSIC_STATE_HANGAR_READY = 1220,
	MUSIC_STATE_FRONTEND_1230 = 1230,
	MUSIC_STATE_FRONTEND_1240 = 1240,
	MUSIC_STATE_FRONTEND_1250 = 1250,
	MUSIC_STATE_FRONTEND_1260 = 1260,
	MUSIC_STATE_FRONTEND_1270 = 1270,
	MUSIC_STATE_FRONTEND_1280 = 1280,
};

extern int musicState;
extern char g_musicInitialized;
extern int g_selectedMusicState;
extern int g_currentMusicState;
extern char g_musicCombatSeen;
extern ImApiTable g_imApiTable;
extern int g_lastMusicSeq;
extern int g_musicSeqCooldown;
extern int g_setMusicState;

int Music_Init(void* directSound);
void Music_Shutdown(void);
int Music_SetState(int state);
void Music_SetDatapadState(int state);
int Music_TriggerSequence(int seqId, int playerSlot, char flags);
int Music_TriggerOutOfHyperspaceSequenceForObject(int objectIdx);
void Music_SetVolume(int volume);
void Music_Stop(void);
void Music_PauseIfInitialized(void);
void Music_ResumeIfInitialized(void);
void Music_Update(void);

#ifdef __cplusplus
}
#endif

#endif
