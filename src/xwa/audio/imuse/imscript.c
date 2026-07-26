#include "xwa/audio/imuse/imuse.h"

#include "xwa_runtime/compat/middleware_crt.h"

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

typedef struct ImMusicFileMapEntry {
	char logicalName[32];
	char path[32];
} ImMusicFileMapEntry;

typedef struct ImRoomStateRule {
	int roomId;
	int baseState;
	int attrIndexA;
	int stateIfA;
	int attrIndexB;
	int stateIfB;
} ImRoomStateRule;

// GLOBAL: XWA 0x788E48
int g_imScriptInitialized;
// GLOBAL: XWA 0x788E04
int g_imSeqEndPending;
// GLOBAL: XWA 0x6075A8
int g_imScriptEnabled;
// GLOBAL: XWA 0x788D10
int g_imScriptInitArg6;
// GLOBAL: XWA 0x788D14
int g_imScriptInitArg7;
// GLOBAL: XWA 0x788D18
int g_imCurMusicState;
// GLOBAL: XWA 0x788D20
int g_imScriptAttribs[58];
// GLOBAL: XWA 0x788D2C
int g_imStateVariationCtr[30];
// GLOBAL: XWA 0x788DA4
int g_imSeqStartedFlags[24];
// GLOBAL: XWA 0x788E08
int g_imCurMusicSeq;
// GLOBAL: XWA 0x788E0C
int g_imPendingMusicSeq;
// GLOBAL: XWA 0x788E10
char g_imScriptNameBuf[32];
// GLOBAL: XWA 0x788E30
ImRoomStateRule g_imRoomStateMap[1];
// GLOBAL: XWA 0x7AAC98
ImHostOpenResourceFn g_imHostOpenStream;
// GLOBAL: XWA 0x7AAC9C
ImHostCloseResourceFn g_imHostCloseStream;
// GLOBAL: XWA 0x7AACA0
ImSoundFile g_imSoundFiles[10];
// GLOBAL: XWA 0x7AAEA8
ImHostLoadResourceFn g_imHostLoadSound;
// GLOBAL: XWA 0x7AAEAC
ImHostFreeResourceFn g_imHostFreeSound;
// GLOBAL: XWA 0x7AAEB0
ImSoundFile* g_imLoadedSoundList;
// GLOBAL: XWA 0x7AAEB4
ImSoundFile* g_imStreamSoundList;
// GLOBAL: XWA 0x7AAEB8
int g_imFilelistReady;

// GLOBAL: XWA 0x605930
const ImScriptEntry g_imStateTable[29] = {
	{ "", 0, 1000, "STATE_NULL", 0, 0, 0 },
	{ "StIntroReb", 3, 1100, "stateIntroReb", 0, 0, 0 },
	{ "StIntroFam", 3, 1102, "stateIntroFam", 0, 0, 0 },
	{ "StWaitReb", 3, 1106, "stateWaitingReb", 0, 0, 0 },
	{ "StWaitFam", 3, 1108, "stateWaitingFam", 0, 0, 0 },
	{ "StEmpire", 3, 1110, "stateEngageEmpire", 0, 0, 0 },
	{ "StRival", 3, 1115, "stateEngageRival", 0, 0, 0 },
	{ "StPirate", 3, 1120, "stateEngagePirate", 0, 0, 0 },
	{ "StPanicReb", 3, 1125, "statePanicReb", 0, 2, 0 },
	{ "StPanicFam", 3, 1127, "statePanicFam", 0, 2, 0 },
	{ "StChalReb", 3, 1130, "stateChallengeReb", 0, 6, 0 },
	{ "StChalFam", 3, 1132, "stateChallengeFam", 0, 6, 0 },
	{ "StConfReb", 3, 1135, "stateConfidentReb", 0, 2, 0 },
	{ "StConfFam", 3, 1140, "stateConfidentFam", 0, 2, 0 },
	{ "StClimReb", 3, 1145, "stateClimaxReb", 0, 2, 0 },
	{ "StClimFam", 3, 1150, "stateClimaxFam", 0, 2, 0 },
	{ "StFailReb", 3, 1155, "stateFailureReb", 16, 0, 0 },
	{ "StFailFam", 3, 1160, "stateFailureFam", 17, 0, 0 },
	{ "StSuccReb", 3, 1165, "stateSuccessReb", 0, 0, 0 },
	{ "StSuccFam", 3, 1170, "stateSuccessFam", 0, 0, 0 },
	{ "FrConcourse", 3, 1200, "stateConcourse", 20, 0, 0 },
	{ "FrFamRoom", 3, 1210, "stateFamRoom", 0, 2, 0 },
	{ "FrHangar", 3, 1220, "stateHangar", 0, 0, 0 },
	{ "FrReadyRoom", 3, 1230, "stateReadyRoom", 20, 0, 0 },
	{ "FrSimulator", 3, 1240, "stateSimulator", 20, 0, 0 },
	{ "FrTechRoom", 3, 1250, "stateTechRoom", 20, 0, 0 },
	{ "FrLoseReb", 3, 1260, "stateLoseReb", 16, 0, 0 },
	{ "FrLoseFam", 3, 1270, "stateLoseFam", 17, 0, 0 },
	{ "FrWin", 3, 1280, "stateWin", 0, 0, 0 },
};

// GLOBAL: XWA 0x6061D0
const ImScriptEntry g_imSeqTable[23] = {
	{ "", 0, 2000, "SEQ_NULL", 0, 0, 0 },
	{ "SqRebGdLg2", 4, 2100, "seqRebGdLg", 0, 0, 0 },
	{ "SqRebGdSm2", 4, 2105, "seqRebGdSm", 0, 0, 0 },
	{ "SqRebBdLg1", 4, 2110, "seqRebBdLg", 0, 0, 0 },
	{ "SqRebBdSm1", 4, 2115, "seqRebBdSm", 0, 0, 0 },
	{ "SqFamGdLg4", 4, 2120, "seqFamGdLg", 0, 0, 0 },
	{ "SqFamGdSm2", 4, 2125, "seqFamGdSm", 0, 0, 0 },
	{ "SqFamBdLg3", 4, 2130, "seqFamBdLg", 0, 0, 0 },
	{ "SqFamBdSm1", 4, 2135, "seqFamBdSm", 0, 0, 0 },
	{ "SqEmpLg1", 4, 2140, "seqEntEmpLg", 0, 0, 0 },
	{ "SqEmpSm2", 4, 2145, "seqEntEmpSm", 0, 0, 0 },
	{ "SqRebLg3", 4, 2150, "seqEntRebLg", 0, 0, 0 },
	{ "SqRebSm2", 4, 2155, "seqEntRebSm", 0, 0, 0 },
	{ "SqPirLg", 4, 2160, "seqEntPirLg", 0, 0, 0 },
	{ "SqPirSm", 4, 2165, "seqEntPirSm", 0, 0, 0 },
	{ "SqFamLg", 4, 2170, "seqEntFamLg", 0, 0, 0 },
	{ "SqFamSm1", 4, 2175, "seqEntFamSm", 0, 0, 0 },
	{ "SqRivLg", 4, 2180, "seqEntRivLg", 0, 0, 0 },
	{ "SqRivSm", 4, 2185, "seqEntRivSm", 0, 0, 0 },
	{ "SqHyper1", 4, 2190, "seqHyperspace", 0, 0, 0 },
	{ "SqEject1", 4, 2195, "seqEjected", 0, 0, 0 },
	{ "SqRampReb", 4, 2200, "seqRampReb", 0, 0, 0 },
	{ "SqRampFam", 4, 2205, "seqRampFam", 0, 0, 0 },
};

// GLOBAL: XWA 0x6068A8
const ImMusicFileMapEntry g_imMusicFileMap[52] = {
	{ "FrConcourse", "music/FrConcourse.IMC" },
	{ "FrFamRoom", "music/FrFamRoom.IMC" },
	{ "FrHangar", "music/FrHangar.IMC" },
	{ "FrLoseFam", "music/FrLoseFam.IMC" },
	{ "FrLoseReb", "music/FrLoseReb.IMC" },
	{ "FrReadyRoom", "music/FrReadyRoom.IMC" },
	{ "FrSimulator", "music/FrSimulator.IMC" },
	{ "FrTechRoom", "music/FrTechRoom.IMC" },
	{ "FrWin", "music/FrWin.IMC" },
	{ "SqEject1", "music/SqEject1.IMC" },
	{ "SqEmpLg1", "music/SqEmpLg1.IMC" },
	{ "SqEmpSm2", "music/SqEmpSm2.IMC" },
	{ "SqFamBdLg3", "music/SqFamBdLg3.IMC" },
	{ "SqFamBdSm1", "music/SqFamBdSm1.IMC" },
	{ "SqFamGdLg4", "music/SqFamGdLg4.IMC" },
	{ "SqFamGdSm2", "music/SqFamGdSm2.IMC" },
	{ "SqFamLg", "music/SqFamLg.IMC" },
	{ "SqFamLg1", "music/SqFamLg1.IMC" },
	{ "SqFamSm1", "music/SqFamSm1.IMC" },
	{ "SqHyper1", "music/SqHyper1.IMC" },
	{ "SqPirLg", "music/SqPirLg.IMC" },
	{ "SqPirSm", "music/SqPirSm.IMC" },
	{ "SqRampFam", "music/SqRampFam.IMC" },
	{ "SqRampReb", "music/SqRampReb.IMC" },
	{ "SqRebBdLg1", "music/SqRebBdLg1.IMC" },
	{ "SqRebBdSm1", "music/SqRebBdSm1.IMC" },
	{ "SqRebGdLg2", "music/SqRebGdLg2.IMC" },
	{ "SqRebGdSm2", "music/SqRebGdSm2.IMC" },
	{ "SqRebLg3", "music/SqRebLg3.IMC" },
	{ "SqRebSm2", "music/SqRebSm2.IMC" },
	{ "SqRivLg", "music/SqRivLg.IMC" },
	{ "SqRivSm", "music/SqRivSm.IMC" },
	{ "StChalFam", "music/StChalFam.IMC" },
	{ "StChalReb", "music/StChalReb.IMC" },
	{ "StClimFam", "music/StClimFam.IMC" },
	{ "StClimReb", "music/StClimReb.IMC" },
	{ "StConfFam", "music/StConfFam.IMC" },
	{ "StConfReb", "music/StConfReb.IMC" },
	{ "StEmpire", "music/StEmpire.IMC" },
	{ "StFailFam", "music/StFailFam.IMC" },
	{ "StFailReb", "music/StFailReb.IMC" },
	{ "StIntroFam", "music/StIntroFam.IMC" },
	{ "StIntroReb", "music/StIntroReb.IMC" },
	{ "StPanicFam", "music/StPanicFam.IMC" },
	{ "StPanicReb", "music/StPanicReb.IMC" },
	{ "StPirate", "music/StPirate.IMC" },
	{ "StRival", "music/StRival.IMC" },
	{ "StSuccFam", "music/StSuccFam.IMC" },
	{ "StSuccReb", "music/StSuccReb.IMC" },
	{ "StWaitFam", "music/StWaitFam.IMC" },
	{ "StWaitReb", "music/StWaitReb.IMC" },
	{ "", "" },
};

// FUNCTION: XWA 0x58BC9A
void ImScriptOnSoundEnd(char* marker) {
	char* callbackMarker;
	int markerPrefix;

	ImPrintf("CALLBACK:");
	if (*marker == '_') {
		g_imSeqEndPending = 1;
	} else {
		ImPrintf("ERR:callback got marker != '_end'...");
	}
}

// FUNCTION: XWA 0x58BCD5
void ImScriptStopAll(void) {
	if (!g_imScriptEnabled) {
		return;
	}

	ImStopAllSounds();
	ImUnloadAll();
	ImCloseAll();
}

// FUNCTION: XWA 0x58B3A0
void ImScriptProcessTransition(ImScriptEntry* entry, int entryIndex, int isSequence) {
	struct {
		int soundId;
		char* src;
		int j;
		int curStreamSound;
		char ch;
		char alignment[3];
		char* dst;
		int variation;
		const ImMusicFileMapEntry* map;
	} transition;

	if (!g_imScriptEnabled) {
		return;
	}

	if (entry) {
		transition.dst = g_imScriptNameBuf;
		transition.src = entry->filename;
		for (;;) {
			if ((*transition.dst++ = *transition.src++)) {
				continue;
			}
			break;
		}

		transition.map = g_imMusicFileMap;
		while (transition.map->logicalName[0]) {
			for (transition.j = 0;; ++transition.j) {
				transition.ch = transition.map->logicalName[transition.j];
				if (transition.ch) {
					if (g_imScriptNameBuf[transition.j] != transition.ch) {
						break;
					}
					continue;
				}
				break;
			}
			if (transition.j && !g_imScriptNameBuf[transition.j] &&
				!transition.map->logicalName[transition.j]) {
				transition.dst = g_imScriptNameBuf;
				transition.src = (char*)transition.map->path;
				for (;;) {
					if ((*transition.dst++ = *transition.src++)) {
						continue;
					}
					break;
				}
				break;
			}
			++transition.map;
		}
	}

	transition.variation = 0;
	if (entry && entryIndex && !transition.variation) {
		if (entry->group) {
			entryIndex = entry->group;
		}
		transition.variation = g_imStateVariationCtr[entryIndex];
		if (entry->hook && ++g_imStateVariationCtr[entryIndex] > entry->hook) {
			g_imStateVariationCtr[entryIndex] = 0;
		}
	}

	transition.soundId = 0;
	while ((transition.soundId = ImGetNextSound(transition.soundId)) != 0) {
		if (ImGetParam(transition.soundId, P_VGROUP) == 4 &&
			!ImGetParam(transition.soundId, P_IS_STREAMING)) {
			ImFadeParam(transition.soundId, P_VOLUME, 0, 120);
		}
	}

	transition.soundId = 0;
	transition.curStreamSound = 0;
	while ((transition.soundId = ImGetNextSound(transition.soundId)) != 0) {
		if (ImGetParam(transition.soundId, P_IS_STREAMING) &&
			ImGetParam(transition.soundId, P_STREAM_SIZE) == 2) {
			transition.curStreamSound = transition.soundId;
			break;
		}
	}

	if (!entry) {
		if (transition.curStreamSound) {
			ImFadeParam(transition.curStreamSound, P_VOLUME, 0, 120);
		}
		return;
	}

	switch (entry->transType) {
		case 0:
			ImPrintf("Trans null...");
			return;

		case 1:
			ImPrintf("loading ");
			ImPrintf(g_imScriptNameBuf);
			ImPrintf("...");
			transition.soundId = ImLoadSound(g_imScriptNameBuf, 0);
			if (!transition.soundId) {
				ImPrintf("SCRIPT ERR: Unable to load ");
				ImPrintf(g_imScriptNameBuf);
				ImPrintf("...");
				return;
			}
			if (ImStartSound((unsigned int)transition.soundId, 126)) {
				ImPrintf("SCRIPT ERR: failed start sound...");
			}
			ImSetParam(transition.soundId, P_VOLUME, 1);
			ImFadeParam(transition.soundId, P_VOLUME, 127, 120);
			ImUnloadSound(transition.soundId);
			break;

		case 5:
			return;

		case 6:
			g_imSeqEndPending = 0;
			ImSetTrigger(12345680, "_end", 0);
			return;

		case 2:
		case 3:
		case 4:
		case 10:
		case 11:
		case 12:
			ImPrintf("opening ");
			ImPrintf(g_imScriptNameBuf);
			ImPrintf("...");
			transition.soundId = ImOpenSound(g_imScriptNameBuf, 0);
			if (!transition.soundId) {
				ImPrintf("SCRIPT ERR: Unable to open ");
				ImPrintf(g_imScriptNameBuf);
				ImPrintf("...");
				if (transition.curStreamSound) {
					ImFadeParam(transition.curStreamSound, P_VOLUME, 0, 60);
				}
				return;
			}

			if (entry->transType == 4) {
				g_imSeqEndPending = 0;
				ImSetTrigger(transition.soundId, "_end", 0);
			}

			if (transition.curStreamSound) {
				if (entry->transType == 2) {
					ImSwitchStream(transition.curStreamSound, transition.soundId,
								   entry->fadeMs ? entry->fadeMs : 800, 0, 0);
					ImSetParam(transition.soundId, P_VOLUME, 127);
					ImSetParam(transition.soundId, P_VGROUP, 4);
					ImSetHook(transition.soundId, entry->hook);
					ImPrintf("HOOK=%lu...", entry->hook);
					ImProcessStreams();
				} else if (transition.curStreamSound != transition.soundId) {
					if (!isSequence && entry->group &&
						entry->group == g_imStateTable[g_imCurMusicState].group) {
						ImSwitchStream(transition.curStreamSound, transition.soundId,
									   entry->fadeMs ? entry->fadeMs : 800, 0, 1);
						ImSetParam(transition.soundId, P_VOLUME, 127);
						ImSetParam(transition.soundId, P_VGROUP, 4);
					} else if (entry->transType == 10) {
						ImSetTrigger(transition.curStreamSound, "exit", 26, transition.curStreamSound,
									 transition.soundId, entry->fadeMs ? entry->fadeMs : 800, 1, 0);
						ImSetTrigger(transition.curStreamSound, "exit", 12, transition.soundId, P_VOLUME,
									 127);
						ImSetTrigger(transition.curStreamSound, "exit", 12, transition.soundId, P_VGROUP, 4);
						ImSetTrigger(transition.curStreamSound, "exit", 15, transition.soundId,
									 transition.variation);
					} else if (entry->transType == 11) {
						ImSetTrigger(transition.curStreamSound, "exit2", 26, transition.curStreamSound,
									 transition.soundId, entry->fadeMs ? entry->fadeMs : 800, 1, 0);
						ImSetTrigger(transition.curStreamSound, "exit2", 12, transition.soundId, P_VOLUME,
									 127);
						ImSetTrigger(transition.curStreamSound, "exit2", 12, transition.soundId, P_VGROUP, 4);
						ImSetTrigger(transition.curStreamSound, "exit2", 15, transition.soundId,
									 transition.variation);
					} else if (entry->transType == 12) {
						ImSetHook(transition.curStreamSound, entry->hook);
						ImSetTrigger(transition.curStreamSound, "exit", 26, transition.curStreamSound,
									 transition.soundId, entry->fadeMs ? entry->fadeMs : 800, 1, 0);
						ImSetTrigger(transition.curStreamSound, "exit", 12, transition.soundId, P_VOLUME,
									 127);
						ImSetTrigger(transition.curStreamSound, "exit", 12, transition.soundId, P_VGROUP, 4);
						ImSetTrigger(transition.curStreamSound, "exit", 15, transition.soundId,
									 transition.variation);
					} else {
						ImSwitchStream(transition.curStreamSound, transition.soundId,
									   entry->fadeMs ? entry->fadeMs : 800, 0, 0);
						ImSetParam(transition.soundId, P_VOLUME, 127);
						ImSetParam(transition.soundId, P_VGROUP, 4);
						ImSetHook(transition.soundId, transition.variation);
					}
					ImProcessStreams();
				}
			} else {
				if (ImStartStream(transition.soundId, 126, 2)) {
					ImPrintf("SCRIPT ERR: failed start stream...");
				}
				ImSetParam(transition.soundId, P_VOLUME, 127);
				ImSetParam(transition.soundId, P_VGROUP, 4);
				ImSetHook(transition.soundId, transition.variation);
			}
			ImCloseSound(transition.soundId);
			break;

		case 7:
			if (transition.curStreamSound) {
				ImFadeParam(transition.curStreamSound, P_VOLUME, 0, 60);
			}
			return;

		case 8:
			if (transition.curStreamSound) {
				ImSetHook(transition.curStreamSound, entry->hook);
			}
			return;

		case 9:
			if (transition.curStreamSound) {
				ImSetHook(transition.curStreamSound, entry->hook);
			}
			g_imSeqEndPending = 0;
			ImSetTrigger(transition.curStreamSound, "_end", 0);
			return;

		default:
			ImPrintf("SCRIPT ERR: bogus trans type...");
			return;
	}

	ImSetParam(transition.soundId, P_VGROUP, 4);
}

// FUNCTION: XWA 0x590EC0
int ImInitFilelist(ImHostLoadResourceFn loadFn, ImHostFreeResourceFn freeFn,
				   ImHostOpenResourceFn openStreamFn, ImHostCloseResourceFn closeStreamFn) {
	int i;

	g_imHostLoadSound = loadFn;
	g_imHostFreeSound = freeFn;
	g_imHostOpenStream = openStreamFn;
	g_imHostCloseStream = closeStreamFn;
	for (i = 0; i < 10; ++i) {
		g_imSoundFiles[i].prev = NULL;
		g_imSoundFiles[i].next = NULL;
		g_imSoundFiles[i].sound = 0;
	}
	g_imFilelistReady = 1;
	return 0;
}

// FUNCTION: XWA 0x5916C1
int ImFindSound(char* name) {
	struct {
		int index;
		ImSoundFile* file;
		char ch;
	} search;

	search.file = g_imLoadedSoundList;
	while (search.file) {
		search.ch = *name;
		for (search.index = 0; search.ch; ++search.index) {
			if (search.file->name[search.index] != search.ch) {
				break;
			}
			search.ch = name[search.index];
		}
		if (search.index && !name[search.index] && !search.file->name[search.index]) {
			return search.file->sound;
		}
		search.file = search.file->next;
	}

	search.file = g_imStreamSoundList;
	while (search.file) {
		search.ch = *name;
		for (search.index = 0; search.ch; ++search.index) {
			if (search.file->name[search.index] != search.ch) {
				break;
			}
			search.ch = name[search.index];
		}
		if (search.index && !name[search.index] && !search.file->name[search.index]) {
			return search.file->sound;
		}
		search.file = search.file->next;
	}

	return 0;
}

// FUNCTION: XWA 0x5911FD
int ImLoadSound(char* name, int mode) {
	int sound;
	int i;
	int j;
	ImSoundFile* file;

	if (!g_imFilelistReady || !*name || !g_imHostLoadSound || !g_imHostFreeSound) {
		return 0;
	}

	sound = ImFindSound(name);
	if (sound) {
		for (file = g_imLoadedSoundList; file; file = file->next) {
			if (sound == file->sound) {
				++file->refCount;
				return sound;
			}
		}
	}

	for (i = 0;; ++i) {
		if (i >= 10) {
			ImPrintf((char*)"Sound List FULL!...");
			return 0;
		}

		file = &g_imSoundFiles[i];
		if (!file->sound) {
			break;
		}
	}

	sound = g_imHostLoadSound(name, mode);
	if (!sound) {
		ImPrintf((char*)"Host couldn't load sound ");
		ImPrintf(name);
		ImPrintf((char*)"...");
		return 0;
	}

	for (j = 0; name[j]; ++j) {
		if (j >= 32) {
			ImPrintf((char*)"Name too long: ");
			ImPrintf(name);
			ImPrintf((char*)"...");
			return 0;
		}
		file->name[j] = name[j];
	}
	file->name[j] = 0;
	file->sound = sound;
	file->refCount = 1;
	ImListAdd((ImListNode**)&g_imLoadedSoundList, (ImListNode*)file);
	return sound;
}

// FUNCTION: XWA 0x59145F
int ImOpenSound(char* name, int mode) {
	ImSoundFile* file;

	if (!g_imFilelistReady || !*name || !g_imHostOpenStream || !g_imHostCloseStream) {
		return 0;
	}

	{
		int sound;
		int i;
		int j;

		sound = ImFindSound(name);
		if (sound) {
			file = g_imStreamSoundList;
			while (file) {
				if (sound == file->sound) {
					++file->refCount;
					return sound;
				}
				file = file->next;
			}
		}

		for (i = 0; i < 10; ++i) {
			file = &g_imSoundFiles[i];
			if (!file->sound) {
				sound = g_imHostOpenStream(name, mode);
				if (!sound) {
					ImPrintf((char*)"Host couldn't open sound ");
					ImPrintf(name);
					ImPrintf((char*)"...");
					return 0;
				}

				for (j = 0; name[j]; ++j) {
					if (j >= 32) {
						ImPrintf((char*)"Name too long: ");
						ImPrintf(name);
						ImPrintf((char*)"...");
						return 0;
					}
					file->name[j] = name[j];
				}
				file->name[j] = 0;
				file->sound = sound;
				file->refCount = 1;
				ImListAdd((ImListNode**)&g_imStreamSoundList, (ImListNode*)file);
				return sound;
			}
		}
	}

	ImPrintf((char*)"Sound List FULL!...");
	return 0;
}

// FUNCTION: XWA 0x5917DB
ImSoundFile* ImFlushSounds(void) {
	ImSoundFile* result;
	ImSoundFile* node;
	ImSoundFile* next;
	int playingOrPending;

	result = g_imLoadedSoundList;
	for (node = g_imLoadedSoundList; node; node = next) {
		next = node->next;
		if (!node->refCount && !ImGetParam(node->sound, P_COUNT) && !ImGetParam(node->sound, P_STATUS)) {
			if (g_imHostFreeSound) {
				g_imHostFreeSound(node->sound);
			}
			node->sound = 0;
			ImListRemove((ImListNode**)&g_imLoadedSoundList, (ImListNode*)node);
		}
		result = next;
	}

	for (node = g_imStreamSoundList; node; node = next) {
		result = node->next;
		next = result;
		if (!node->refCount) {
			playingOrPending = ImGetParam(node->sound, P_COUNT);
			if (!playingOrPending) {
				playingOrPending = ImGetParam(node->sound, P_STATUS);
				if (!playingOrPending) {
					if (g_imHostCloseStream) {
						g_imHostCloseStream(node->sound);
					}
					node->sound = 0;
					ImListRemove((ImListNode**)&g_imStreamSoundList, (ImListNode*)node);
					result = NULL;
				}
			}
		}
	}
	return result;
}

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x5913AA
void ImUnloadSound(int soundId) {
	ImSoundFile* file;

	file = g_imLoadedSoundList;
	if (!g_imFilelistReady || !g_imHostLoadSound || !g_imHostFreeSound) {
		return;
	}

	while (file) {
		if (file->sound == soundId) {
			if (file->refCount) {
				--file->refCount;
			}
			break;
		}
		file = file->next;
	}
	ImFlushSounds();
}

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x591412
void ImUnloadAll(void) {
	ImSoundFile* file;

	file = g_imLoadedSoundList;
	if (!g_imFilelistReady || !g_imHostLoadSound || !g_imHostFreeSound) {
		return;
	}

	while (file) {
		file->refCount = 0;
		file = file->next;
	}
	ImFlushSounds();
}

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x59160C
void ImCloseSound(int soundId) {
	ImSoundFile* file;

	file = g_imStreamSoundList;
	if (!g_imFilelistReady || !g_imHostOpenStream || !g_imHostCloseStream) {
		return;
	}

	while (file) {
		if (file->sound == soundId) {
			if (file->refCount) {
				--file->refCount;
			}
			break;
		}
		file = file->next;
	}
	ImFlushSounds();
}

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x591674
void ImCloseAll(void) {
	ImSoundFile* file;

	file = g_imStreamSoundList;
	if (!g_imFilelistReady || !g_imHostOpenStream || !g_imHostCloseStream) {
		return;
	}

	while (file) {
		file->refCount = 0;
		file = file->next;
	}
	ImFlushSounds();
}

// FUNCTION: XWA 0x590F40
int ImSaveFilelist(int* buf, unsigned int bufSize) {
	int savedSize;

	savedSize = 0;
	if (bufSize < 0x210) {
		ImPrintf((char*)"ERR: filelist save too big...");
		return 0;
	}

	{
		int* loadedCount;
		ImSoundFile* file;

		loadedCount = (int*)((char*)buf + savedSize);
		savedSize += 4;
		{
			int* streamCount;

			streamCount = (int*)((char*)buf + savedSize);
			savedSize += 4;

			file = g_imLoadedSoundList;
			*loadedCount = 0;
			while (file) {
				++*loadedCount;
				memcpy_0((char*)buf + savedSize, file, sizeof(*file));
				savedSize += (int)sizeof(*file);
				file = file->next;
			}

			file = g_imStreamSoundList;
			*streamCount = 0;
			while (file) {
				++*streamCount;
				memcpy_0((char*)buf + savedSize, file, sizeof(*file));
				savedSize += (int)sizeof(*file);
				file = file->next;
			}
		}
	}
	return savedSize;
}

// FUNCTION: XWA 0x591031
int ImRestoreFilelist(int* buf) {
	int loadedCount;
	int streamCount;
	int savedOffset;
	int i;
	int* savedFile;

	for (i = 0; i < 10; ++i) {
		g_imSoundFiles[i].prev = NULL;
		g_imSoundFiles[i].next = NULL;
		g_imSoundFiles[i].sound = 0;
	}

	loadedCount = buf[0];
	streamCount = buf[1];
	savedOffset = 2;

	while (loadedCount--) {
		savedFile = &buf[savedOffset];
		savedOffset += 13;
		if (!ImLoadSound((char*)savedFile + 12, savedFile[2])) {
			ImPrintf((char*)"ERR: ImRestoreFilelist() couldn't load file ");
			ImPrintf((char*)savedFile + 12);
			ImPrintf((char*)"...");
			return 0;
		}
		if (savedFile[2] != g_imLoadedSoundList->sound) {
			ImPrintf((char*)"ERR: ImRestoreFilelist() detected modified sound number...");
			return 0;
		}
		g_imLoadedSoundList->refCount = savedFile[12];
	}

	while (streamCount--) {
		savedFile = &buf[savedOffset];
		savedOffset += 13;
		if (!ImOpenSound((char*)savedFile + 12, savedFile[2])) {
			ImPrintf((char*)"ERR: ImRestoreFilelist() couldn't open file ");
			ImPrintf((char*)savedFile + 12);
			ImPrintf((char*)"...");
		}
		if (savedFile[2] != g_imStreamSoundList->sound) {
			ImPrintf((char*)"ERR: ImRestoreFilelist() detected modified sound number...");
			return 0;
		}
		g_imStreamSoundList->refCount = savedFile[12];
	}

	return savedOffset * 4;
}

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x58AAD7
int ImScriptHandleInit(ImScriptHost* host, ImHostLoadResourceFn loadFn, ImHostFreeResourceFn freeFn,
					   ImHostOpenResourceFn openStreamFn, ImHostCloseResourceFn closeStreamFn, int arg6,
					   int arg7, int enabled) {
	int i;

	host->onSoundEnd = ImScriptOnSoundEnd;
	g_imScriptEnabled = enabled;
	ImInitFilelist(loadFn, freeFn, openStreamFn, closeStreamFn);
	g_imScriptInitArg6 = arg6;
	g_imScriptInitArg7 = arg7;
	g_imCurMusicState = 0;
	g_imCurMusicSeq = 0;
	g_imPendingMusicSeq = 0;
	for (i = 0; i < 58; ++i) {
		g_imScriptAttribs[i] = 0;
	}
	return 0;
}

// FUNCTION: XWA 0x58AB63
int ImScriptHandleTerminate(void* host) {
	int i;

	ImScriptStopAll();
	g_imCurMusicState = 0;
	g_imCurMusicSeq = 0;
	g_imPendingMusicSeq = 0;
	for (i = 0; i < 58; ++i) {
		g_imScriptAttribs[i] = 0;
	}
	return 0;
}

// FUNCTION: XWA 0x58ABB8
int ImScriptHandleSave(int* buf, int bufSize) {
	int i;
	int savedSize;
	int filelistSize;

	savedSize = 0;
	if (bufSize < 244) {
		ImPrintf((char*)"SCRIPT ERR: save buffer too small...");
		return -1;
	}

	*(int*)((char*)buf + savedSize) = g_imCurMusicState;
	savedSize += 4;
	*(int*)((char*)buf + savedSize) = g_imCurMusicSeq;
	savedSize += 4;
	*(int*)((char*)buf + savedSize) = g_imPendingMusicSeq;
	savedSize += 4;

	for (i = 0; i < 58; ++i) {
		*(int*)((char*)buf + savedSize) = g_imScriptAttribs[i];
		savedSize += 4;
	}

	filelistSize = ImSaveFilelist((int*)((char*)buf + savedSize), bufSize - savedSize);
	if (filelistSize < 0) {
		return filelistSize;
	}
	savedSize += filelistSize;
	return savedSize;
}

// FUNCTION: XWA 0x58AC91
int ImScriptHandleRestore(int* buf) {
	int i;
	int savedSize;

	savedSize = 0;
	ImUnloadAll();
	ImCloseAll();

	g_imCurMusicState = *(int*)((char*)buf + savedSize);
	savedSize += 4;
	g_imCurMusicSeq = *(int*)((char*)buf + savedSize);
	savedSize += 4;
	g_imPendingMusicSeq = *(int*)((char*)buf + savedSize);
	savedSize += 4;

	for (i = 0; i < 58; ++i) {
		g_imScriptAttribs[i] = *(int*)((char*)buf + savedSize);
		savedSize += 4;
	}

	savedSize += ImRestoreFilelist((int*)((char*)buf + savedSize));
	return savedSize;
}

// FUNCTION: XWA 0x58AD40
int ImScriptHandleRefresh(void) {
	int soundId;

	if (g_imSeqEndPending) {
		ImScriptSetSequence(0);
		g_imSeqEndPending = 0;
	}

	soundId = 0;
	while ((soundId = ImGetNextSound(soundId)) != 0) {
		if (ImGetParam(soundId, P_IS_STREAMING) && ImGetParam(soundId, P_STREAM_SIZE) == 2) {
			break;
		}
	}

	if (!soundId && g_imCurMusicSeq) {
		ImScriptSetSequence(0);
	}

	ImFlushSounds();
	return 0;
}

// FUNCTION: XWA 0x58ADD0
int ImScriptSetState(int stateIdOrRoom) {
	ImScriptEntry* entry;
	int stateIndex;
	int roomIndex;

	if (stateIdOrRoom == -1) {
		return g_imStateTable[g_imCurMusicState].id;
	}

	for (stateIndex = 0; stateIndex < 29; ++stateIndex) {
		if (stateIdOrRoom == g_imStateTable[stateIndex].id) {
			stateIdOrRoom = stateIndex;
			ImPrintf((char*)g_imStateTable[stateIndex].label);
			ImPrintf("...");
			if (stateIdOrRoom != g_imCurMusicState) {
				if (!g_imCurMusicSeq) {
					if (stateIdOrRoom) {
						entry = (ImScriptEntry*)&g_imStateTable[stateIdOrRoom];
					} else {
						entry = NULL;
					}
					ImScriptProcessTransition(entry, stateIdOrRoom, 0);
				}
				g_imCurMusicState = stateIdOrRoom;
			}
			return g_imStateTable[g_imCurMusicState].id;
		}
	}

	for (roomIndex = 0;
		 g_imRoomStateMap[roomIndex].roomId != stateIdOrRoom && g_imRoomStateMap[roomIndex].roomId != -1;
		 ++roomIndex) {
	}

	if (g_imScriptAttribs[g_imRoomStateMap[roomIndex].attrIndexA]) {
		if (g_imRoomStateMap[roomIndex].stateIfA) {
			stateIdOrRoom = g_imRoomStateMap[roomIndex].stateIfA;
		} else {
			stateIdOrRoom = g_imScriptAttribs[g_imRoomStateMap[roomIndex].attrIndexA] +
							g_imRoomStateMap[roomIndex].baseState;
		}
	} else if (g_imScriptAttribs[g_imRoomStateMap[roomIndex].attrIndexB]) {
		stateIdOrRoom = g_imRoomStateMap[roomIndex].stateIfB;
	} else {
		stateIdOrRoom = g_imRoomStateMap[roomIndex].baseState;
	}

	ImPrintf("room %lu ", g_imRoomStateMap[roomIndex].roomId);
	ImPrintf((char*)g_imStateTable[stateIdOrRoom].label);
	ImPrintf("...");
	if (stateIdOrRoom != g_imCurMusicState) {
		if (!g_imCurMusicSeq) {
			if (stateIdOrRoom) {
				entry = (ImScriptEntry*)&g_imStateTable[stateIdOrRoom];
			} else {
				entry = NULL;
			}
			ImScriptProcessTransition(entry, stateIdOrRoom, 0);
		}
		g_imCurMusicState = stateIdOrRoom;
	}
	return 0;
}

// FUNCTION: XWA 0x58B01F
int ImScriptSetSequence(int seqId) {
	ImScriptEntry* entry;
	int seqIndex;

	if (seqId == -1) {
		return g_imSeqTable[g_imCurMusicSeq].id;
	}

	if (!seqId) {
		seqId = 2000;
	}

	for (seqIndex = 0; seqIndex < 23; ++seqIndex) {
		if (seqId == g_imSeqTable[seqIndex].id) {
			seqId = seqIndex;
			ImPrintf((char*)g_imSeqTable[seqIndex].label);
			ImPrintf("...");
			if (seqId != g_imCurMusicSeq) {
				if (seqId) {
					if (g_imCurMusicSeq && (g_imSeqTable[g_imCurMusicSeq].transType == 4 ||
											g_imSeqTable[g_imCurMusicSeq].transType == 6)) {
						g_imPendingMusicSeq = seqId;
					} else {
						entry = (ImScriptEntry*)&g_imSeqTable[seqId];
						ImScriptProcessTransition(entry, 0, 1);
						g_imPendingMusicSeq = 0;
						g_imSeqStartedFlags[seqId] = 1;
						g_imCurMusicSeq = seqId;
					}
				} else if (g_imPendingMusicSeq) {
					entry = (ImScriptEntry*)&g_imSeqTable[g_imPendingMusicSeq];
					ImScriptProcessTransition(entry, 0, 1);
					g_imSeqStartedFlags[g_imPendingMusicSeq] = 1;
					g_imCurMusicSeq = g_imPendingMusicSeq;
					g_imPendingMusicSeq = 0;
				} else {
					if (g_imCurMusicState) {
						entry = (ImScriptEntry*)&g_imStateTable[g_imCurMusicState];
					} else {
						entry = NULL;
					}
					ImScriptProcessTransition(entry, g_imCurMusicState, 1);
					g_imCurMusicSeq = 0;
				}
			}
			return g_imSeqTable[g_imCurMusicSeq].id;
		}
	}

	ImPrintf("ERR: bogus sequence...");
	return -1;
}

// FUNCTION: XWA 0x58B1FA
int ImScriptSetCuePoint(void) { return 0; }

// FUNCTION: XWA 0x58B204
int ImScriptSetAttribute(unsigned int attrIndex, int value) {
	char* dst;
	const ImScriptEntry* entry;
	int count;
	int i;
	int j;

	if (attrIndex == 0xffffffffu) {
		if (!value) {
			ImPauseRefInc();
		} else if (value == 1) {
			ImResumeRefDec();
		} else if (value == -1) {
			for (i = 0; i < 58; ++i) {
				g_imScriptAttribs[i] = 0;
			}
		}
		return 0;
	}

	if (attrIndex == 10000u) {
		dst = (char*)(intptr_t)value;
		count = 0;
		for (i = 0; i < 29; ++i) {
			entry = &g_imStateTable[i];
			do {
				*dst = entry->filename[0];
				++dst;
				entry = (const ImScriptEntry*)((const char*)entry + 1);
			} while (dst[-1]);
			++count;
		}

		for (i = 0; i < 23; ++i) {
			for (j = 0; j < 1; ++j) {
				entry = &g_imSeqTable[i + j];
				do {
					*dst = entry->filename[0];
					++dst;
					entry = (const ImScriptEntry*)((const char*)entry + 1);
				} while (dst[-1]);
				++count;
			}
		}
		return count;
	}

	if (attrIndex < 58u) {
		if (value != -1) {
			ImPrintf((char*)"Set attr %lu to %lu...", attrIndex, value);
			g_imScriptAttribs[attrIndex] = value;
		}
		return g_imScriptAttribs[attrIndex];
	}

	ImPrintf((char*)"ERR: bogus attribute...");
	return 0;
}

// FUNCTION: XWA 0x58A7C0
int ImScriptCommand(int opcode, ...) {
#ifdef XWA_MODERN
	va_list args;
	int result;

	result = -1;
	va_start(args, opcode);

	if (g_imScriptInitialized || opcode == 0) {
		switch (opcode) {
			case 0:
				if (g_imScriptInitialized) {
					ImPrintf((char*)"ERROR:script already initialized...");
				} else {
					ImScriptHost* host;
					ImHostLoadResourceFn loadFn;
					ImHostFreeResourceFn freeFn;
					ImHostOpenResourceFn openStreamFn;
					ImHostCloseResourceFn closeStreamFn;
					int arg6;
					int arg7;
					int enabled;

					host = va_arg(args, ImScriptHost*);
					loadFn = va_arg(args, ImHostLoadResourceFn);
					freeFn = va_arg(args, ImHostFreeResourceFn);
					openStreamFn = va_arg(args, ImHostOpenResourceFn);
					closeStreamFn = va_arg(args, ImHostCloseResourceFn);
					arg6 = va_arg(args, int);
					arg7 = va_arg(args, int);
					enabled = va_arg(args, int);

					g_imScriptInitialized = 1;
					result = ImScriptHandleInit(host, loadFn, freeFn, openStreamFn, closeStreamFn, arg6, arg7,
												enabled);
				}
				break;

			case 1:
				g_imScriptInitialized = 0;
				result = ImScriptHandleTerminate(NULL);
				break;

			case 2: {
				int* buf;
				int bufSize;

				buf = va_arg(args, int*);
				bufSize = va_arg(args, int);
				result = ImScriptHandleSave(buf, bufSize);
			} break;

			case 3:
				result = ImScriptHandleRestore(va_arg(args, int*));
				break;

			case 4:
				result = ImScriptHandleRefresh();
				break;

			case 5:
				result = ImScriptSetState(va_arg(args, int));
				break;

			case 6:
				result = ImScriptSetSequence(va_arg(args, int));
				break;

			case 7:
				result = ImScriptSetCuePoint();
				break;

			case 8: {
				unsigned int attrIndex;
				int value;

				attrIndex = va_arg(args, unsigned int);
				value = va_arg(args, int);
				result = ImScriptSetAttribute(attrIndex, value);
			} break;

			default:
				ImPrintf((char*)"ERROR:unrecognized opcode in script...");
				break;
		}
	} else {
		ImPrintf((char*)"ERROR:script not initialized...");
	}

	va_end(args);
	return result;
#else
	typedef int (*ImScriptCommandHandler)(int, int, int, int, int, int, int, int, int, int);

	struct {
		int i;
		int result;
		ImScriptCommandHandler handler;
		int params[10];
		va_list args;
		va_list dispatchArgs;
	} dispatch;

	dispatch.result = -1;
	va_start(dispatch.args, opcode);
	for (dispatch.i = 0; dispatch.i < 10; ++dispatch.i) {
		dispatch.params[dispatch.i] = va_arg(dispatch.args, int);
	}

	if (!g_imScriptInitialized && opcode != 0) {
		ImPrintf((char*)"ERROR:script not initialized...");
	} else {
		dispatch.dispatchArgs = (va_list)&opcode;
		dispatch.dispatchArgs += sizeof(dispatch.params);
		switch (opcode) {
			case 0:
				if (!g_imScriptInitialized) {
					g_imScriptInitialized = 1;
					dispatch.handler = (ImScriptCommandHandler)ImScriptHandleInit;
					dispatch.result = dispatch.handler(
						dispatch.params[0], dispatch.params[1], dispatch.params[2], dispatch.params[3],
						dispatch.params[4], dispatch.params[5], dispatch.params[6], dispatch.params[7],
						dispatch.params[8], dispatch.params[8]);
				} else {
					ImPrintf((char*)"ERROR:script already initialized...");
				}
				break;

			case 1:
				g_imScriptInitialized = 0;
				dispatch.handler = (ImScriptCommandHandler)ImScriptHandleTerminate;
				dispatch.result = dispatch.handler(dispatch.params[0], dispatch.params[1], dispatch.params[2],
												   dispatch.params[3], dispatch.params[4], dispatch.params[5],
												   dispatch.params[6], dispatch.params[7], dispatch.params[8],
												   dispatch.params[8]);
				break;

			case 2:
				dispatch.handler = (ImScriptCommandHandler)ImScriptHandleSave;
				dispatch.result = dispatch.handler(dispatch.params[0], dispatch.params[1], dispatch.params[2],
												   dispatch.params[3], dispatch.params[4], dispatch.params[5],
												   dispatch.params[6], dispatch.params[7], dispatch.params[8],
												   dispatch.params[8]);
				break;

			case 3:
				dispatch.handler = (ImScriptCommandHandler)ImScriptHandleRestore;
				dispatch.result = dispatch.handler(dispatch.params[0], dispatch.params[1], dispatch.params[2],
												   dispatch.params[3], dispatch.params[4], dispatch.params[5],
												   dispatch.params[6], dispatch.params[7], dispatch.params[8],
												   dispatch.params[8]);
				break;

			case 4:
				dispatch.handler = (ImScriptCommandHandler)ImScriptHandleRefresh;
				dispatch.result = dispatch.handler(dispatch.params[0], dispatch.params[1], dispatch.params[2],
												   dispatch.params[3], dispatch.params[4], dispatch.params[5],
												   dispatch.params[6], dispatch.params[7], dispatch.params[8],
												   dispatch.params[8]);
				break;

			case 5:
				dispatch.handler = (ImScriptCommandHandler)ImScriptSetState;
				dispatch.result = dispatch.handler(dispatch.params[0], dispatch.params[1], dispatch.params[2],
												   dispatch.params[3], dispatch.params[4], dispatch.params[5],
												   dispatch.params[6], dispatch.params[7], dispatch.params[8],
												   dispatch.params[8]);
				break;

			case 6:
				dispatch.handler = (ImScriptCommandHandler)ImScriptSetSequence;
				dispatch.result = dispatch.handler(dispatch.params[0], dispatch.params[1], dispatch.params[2],
												   dispatch.params[3], dispatch.params[4], dispatch.params[5],
												   dispatch.params[6], dispatch.params[7], dispatch.params[8],
												   dispatch.params[8]);
				break;

			case 7:
				dispatch.handler = (ImScriptCommandHandler)ImScriptSetCuePoint;
				dispatch.result = dispatch.handler(dispatch.params[0], dispatch.params[1], dispatch.params[2],
												   dispatch.params[3], dispatch.params[4], dispatch.params[5],
												   dispatch.params[6], dispatch.params[7], dispatch.params[8],
												   dispatch.params[8]);
				break;

			case 8:
				dispatch.handler = (ImScriptCommandHandler)ImScriptSetAttribute;
				dispatch.result = dispatch.handler(dispatch.params[0], dispatch.params[1], dispatch.params[2],
												   dispatch.params[3], dispatch.params[4], dispatch.params[5],
												   dispatch.params[6], dispatch.params[7], dispatch.params[8],
												   dispatch.params[8]);
				break;

			default:
				ImPrintf((char*)"ERROR:unrecognized opcode in script...");
				break;
		}
	}

	va_end(dispatch.args);
	return dispatch.result;
#endif
}
