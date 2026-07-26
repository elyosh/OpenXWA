#include "xwa/audio/sound.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/audio/fsfx.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/math/fixed.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/util/debug.h"
#include "xwa/util/time.h"

#ifndef XWA_MODERN
#include <stdio.h>
#endif
#include <string.h>

// GLOBAL: XWA 0x917E80
int g_sfxIds[2872];
// GLOBAL: XWA 0x91B844
void* g_directSound;
// GLOBAL: XWA 0x5BA994
int g_maxActiveSounds = 8;
// GLOBAL: XWA 0x968B99
int g_activeSoundCount;
// GLOBAL: XWA 0x968B9D
int g_nextSoundInstanceSeq;
// GLOBAL: XWA 0x968B91
int g_soundQueueCount;
// GLOBAL: XWA 0x5BA990
uint8_t g_sound3DEnabled = 2;
// GLOBAL: XWA 0x968770
ActiveSoundInstance g_activeSoundInstances[32];
// GLOBAL: XWA 0x968AB0
SoundQueueEntry g_soundQueue[5];
// GLOBAL: XWA 0x968B95
int g_soundCount;
// GLOBAL: XWA 0x91B850
SoundEffectDef g_soundDefs[1536];
// GLOBAL: XWA 0x91B848
IDirectSoundBuffer* g_primarySoundBuffer;
// GLOBAL: XWA 0x91B84C
IDirectSound3DListener* g_sound3DListener;
// GLOBAL: XWA 0x74D5D0
uint8_t g_soundHardware3DBuffersAvailable;
// GLOBAL: XWA 0x74D5CC
uint8_t g_flightConfAudio22k;
// GLOBAL: XWA 0x74D5D4
int g_sound3DListenerLastGameTime;
// GLOBAL: XWA 0x5A9E2C
const float g_soundMinDistanceScale = 0.5f;
// GLOBAL: XWA 0x5A9E34
const float g_soundQ15ToFloatScale = 0.000030518509f;
// GLOBAL: XWA 0x602970
const int g_directSoundVolumeMillibelTable[128] = {
	-10000, -6000, -5415, -5000, -4678, -4415, -4192, -4000, -3830, -3678, -3540, -3415, -3299, -3192, -3093,
	-3000,  -2912, -2830, -2752, -2678, -2607, -2540, -2476, -2415, -2356, -2299, -2245, -2192, -2142, -2093,
	-2045,  -2000, -1955, -1912, -1870, -1830, -1790, -1752, -1714, -1678, -1642, -1607, -1573, -1540, -1508,
	-1476,  -1445, -1415, -1385, -1356, -1327, -1299, -1272, -1245, -1218, -1192, -1167, -1142, -1117, -1093,
	-1069,  -1045, -1022, -1000, -977,  -955,  -933,  -912,  -891,  -870,  -850,  -830,  -810,  -790,  -771,
	-752,   -733,  -714,  -696,  -678,  -660,  -642,  -624,  -607,  -590,  -573,  -557,  -540,  -524,  -508,
	-492,   -476,  -460,  -445,  -430,  -415,  -400,  -385,  -370,  -356,  -341,  -327,  -313,  -299,  -285,
	-272,   -258,  -245,  -231,  -218,  -205,  -192,  -179,  -167,  -154,  -142,  -129,  -117,  -105,  -93,
	-81,    -69,   -57,   -45,   -34,   -22,   -11,   0,
};

// FUNCTION: XWA 0x4DA640
uint8_t Sound_Init_Sound_Engine(void* hwnd) {
	DSoundDeviceCaps deviceCaps;
	struct PrimarySoundSetup {
		DSBufferDesc primaryDesc;
		uint32_t speakerConfig;
	} primarySetup;
	DSWaveFormat primaryFormat;
	int result;
	int index;

	g_sound3DEnabled = g_gameConfig.sound3dEnabled;
	g_maxActiveSounds = g_gameConfig.numberOfSfx;
	if (g_maxActiveSounds < 6) {
		g_maxActiveSounds = 6;
	}
	if (g_maxActiveSounds > 32) {
		g_maxActiveSounds = 32;
	}

#ifdef XWA_MODERN
	/* The Aeron shim exposes no hardware buffers, so forced resampling does not
	 * apply to the modern audio path. */
	g_flightConfAudio22k = 0;
#else
	{
		FILE* force22kFile = fopen("force22k.txt", "r");
		if (force22kFile) {
			g_flightConfAudio22k = 1;
			fclose(force22kFile);
		} else {
			g_flightConfAudio22k = 0;
		}
	}
#endif

	if (g_directSound) {
		DebugPrintf("ERROR:Already a DS object in Aldraw_Init_Sound_Engine");
		return 1;
	}

	for (index = 0; index < 32; ++index) {
		g_activeSoundInstances[index].soundId = -1;
		g_activeSoundInstances[index].sequence = 0;
		g_activeSoundInstances[index].sourceObjOrPointRef = 0xFFFF;
		g_activeSoundInstances[index].sourceCreationIdx = -1;
		g_activeSoundInstances[index].buffer = NULL;
		g_activeSoundInstances[index].buffer3D = NULL;
	}
	g_activeSoundCount = 0;
	g_soundCount = 0;
	g_nextSoundInstanceSeq = 0;
	for (index = 0; index < 1536; ++index) {
		g_soundDefs[index].buffer = 0;
		g_soundDefs[index].buffer3D = 0;
		g_soundDefs[index].name[0] = '\0';
	}
	memset(g_soundQueue, 0, sizeof(g_soundQueue));

	g_directSound = FrontendSound_GetDirectSound();
	if (!g_directSound) {
		return 0;
	}

	g_soundHardware3DBuffersAvailable = 0;
	memset(&deviceCaps, 0, sizeof(deviceCaps));
	deviceCaps.dwSize = sizeof(deviceCaps);
	{
		int capsResult =
			((IDirectSound*)g_directSound)->lpVtbl->GetCaps((IDirectSound*)g_directSound, &deviceCaps);
		if (capsResult < 0) {
			DebugPrintfChannel(1, "Error %d autodetecting 3D sound capability.  Assuming NO.\n", capsResult);
			if (g_sound3DEnabled == 2) {
				g_sound3DEnabled = 0;
			}
		} else if (deviceCaps.dwMaxHw3DAllBuffers > 0) {
			DebugPrintfChannel(1, "3D sound supported in hardware with %d sounds.\n",
							   deviceCaps.dwMaxHw3DAllBuffers);
			g_soundHardware3DBuffersAvailable = 1;
			if (g_sound3DEnabled == 2) {
				g_sound3DEnabled = 1;
			}
			if (deviceCaps.dwMaxHw3DAllBuffers < (uint32_t)g_maxActiveSounds) {
				g_maxActiveSounds = deviceCaps.dwMaxHw3DAllBuffers;
				if (g_maxActiveSounds > 32) {
					g_maxActiveSounds = 32;
				}
			}
		} else {
			DebugPrintfChannel(1, "3D sound unlikely to be supported in hardware.\n");
			g_soundHardware3DBuffersAvailable = 0;
			if (g_sound3DEnabled == 2) {
				g_sound3DEnabled = 0;
			}
		}
	}
	if (!g_sound3DEnabled) {
		g_soundHardware3DBuffersAvailable = 0;
	}

	result =
		((IDirectSound*)g_directSound)->lpVtbl->SetCooperativeLevel((IDirectSound*)g_directSound, hwnd, 2);
	if (result != 0) {
		Sound_Shutdown_Sound_Engine();
		return 0;
	}

	memset(&primarySetup.primaryDesc, 0, sizeof(primarySetup.primaryDesc));
	primarySetup.primaryDesc.dwSize = 20;
	primarySetup.primaryDesc.dwFlags = g_sound3DEnabled ? 17u : 1u;
	primarySetup.primaryDesc.dwBufferBytes = 0;
	primarySetup.primaryDesc.lpwfxFormat = NULL;
	result = ((IDirectSound*)g_directSound)
				 ->lpVtbl->CreateSoundBuffer((IDirectSound*)g_directSound, &primarySetup.primaryDesc,
											 &g_primarySoundBuffer, NULL);
	if (result != 0) {
		Sound_Shutdown_Sound_Engine();
		return 0;
	}

	result = ((IDirectSound*)g_directSound)
				 ->lpVtbl->GetSpeakerConfig((IDirectSound*)g_directSound, &primarySetup.speakerConfig);
	if (result < 0) {
		DebugPrintfChannel(1, "Unable to determine speaker configuration.\n");
	} else if (primarySetup.speakerConfig & 1) {
		DebugPrintfChannel(1, "Speaker config set to: Headphones\n");
	} else if (primarySetup.speakerConfig & 2) {
		DebugPrintfChannel(1, "Speaker config set to: Mono\n");
	} else if (primarySetup.speakerConfig & 3) {
		DebugPrintfChannel(1, "Speaker config set to: Quadrophonic\n");
	} else if (primarySetup.speakerConfig & 4) {
		DebugPrintfChannel(1, "Speaker config set to: Stereo\n");
	} else if (primarySetup.speakerConfig & 5) {
		DebugPrintfChannel(1, "Speaker config set to: Surround\n");
	} else {
		DebugPrintfChannel(1, "Speaker config set to: Unknown\n");
	}

	if (g_sound3DEnabled) {
		result = g_primarySoundBuffer->lpVtbl->QueryInterface(
			g_primarySoundBuffer, &IID_IDirectSound3DListener, (void**)&g_sound3DListener);
		if (result < 0) {
			g_sound3DEnabled = 0;
			DebugPrintfChannel(1, "Can't use 3D sound.  Reverting to standard.\n");
		} else {
			result = g_sound3DListener->lpVtbl->SetDistanceFactor(g_sound3DListener, 0.0244144f, 0);
			if (result < 0) {
				DebugPrintfChannel(1, "Unable to set 3ds distance factor.\n");
			}
			result = g_sound3DListener->lpVtbl->SetDopplerFactor(g_sound3DListener, 0.0f, 0);
			if (result < 0) {
				DebugPrintfChannel(1, "Unable to set 3ds doppler factor.\n");
			}
		}
	}

	memset(&primaryFormat, 0, sizeof(primaryFormat));
	result = g_primarySoundBuffer->lpVtbl->GetFormat(g_primarySoundBuffer, &primaryFormat,
													 sizeof(primaryFormat), NULL);
	if (result != 0) {
		DebugPrintfChannel(1, "Error getting sound format.  Shutting down sound.\n");
		Sound_Shutdown_Sound_Engine();
		return 0;
	}
	DebugPrintfChannel(1, "Primary sound buffer originally at %ld hz, %d bits, %d channels.\n",
					   primaryFormat.nSamplesPerSec, primaryFormat.wBitsPerSample, primaryFormat.nChannels);
	primaryFormat.nSamplesPerSec = 22050;
	primaryFormat.wBitsPerSample = 16;
	primaryFormat.wFormatTag = 1;
	primaryFormat.nBlockAlign = primaryFormat.nChannels * 2;
	primaryFormat.nAvgBytesPerSec = 22050 * primaryFormat.nBlockAlign;
	g_primarySoundBuffer->lpVtbl->SetFormat(g_primarySoundBuffer, &primaryFormat);
	g_primarySoundBuffer->lpVtbl->GetFormat(g_primarySoundBuffer, &primaryFormat, sizeof(primaryFormat),
											NULL);
	DebugPrintfChannel(1, "Primary sound buffer set to %ld hz, %d bits, %d channels.\n",
					   primaryFormat.nSamplesPerSec, primaryFormat.wBitsPerSample, primaryFormat.nChannels);

	if (g_gameConfig.musicEnabled) {
		if (g_gameConfig.musicVolume == 10) {
			Music_SetVolume(127);
			return 1;
		}
		Music_SetVolume(13 * g_gameConfig.musicVolume);
	}
	return 1;
}

static __inline int Sound_FindEffectByNameInline(const char* name, int soundCount) {
	int soundId;

	soundId = soundCount - 1;
	while (soundId >= 0) {
		if (strcmp(name, g_soundDefs[soundId].name) == 0) {
			break;
		}
		--soundId;
	}

	return soundId;
}

static __inline uint8_t Sound_StopOldestInstanceByName(const char* name) {
	int soundId;

	if (name[0] == '\0') {
		return 0;
	}

	soundId = g_soundCount - 1;
	while (soundId >= 0) {
		if (strcmp(name, g_soundDefs[soundId].name) == 0) {
			break;
		}
		--soundId;
	}

	return Sound_StopOldestInstance(soundId);
}

static __inline uint8_t Sound_UnloadEffectByName(const char* name) {
	int soundId;

	if (name[0] == '\0') {
		return 0;
	}

	soundId = Sound_FindEffectByNameInline(name, g_soundCount);
	if (soundId == -1) {
		return 0;
	}

	while (Sound_StopOldestInstanceByName(g_soundDefs[soundId].name) == 1) {
	}

	if (g_sound3DEnabled) {
		g_soundDefs[soundId].buffer3D->lpVtbl->Release(g_soundDefs[soundId].buffer3D);
	}
	g_soundDefs[soundId].buffer->lpVtbl->Release(g_soundDefs[soundId].buffer);

	if (soundId >= 0 && soundId < g_soundCount) {
		int copyIndex = soundId;

		while (copyIndex < g_soundCount - 1) {
			SoundEffectDef* soundDef = &g_soundDefs[copyIndex];

			soundDef[0] = soundDef[1];
			++copyIndex;
		}
		--g_soundCount;
		{
			int activeIndex;

			for (activeIndex = 0; activeIndex < g_maxActiveSounds; ++activeIndex) {
				if (g_activeSoundInstances[activeIndex].soundId > soundId) {
					--g_activeSoundInstances[activeIndex].soundId;
				}
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x4DAAD0
uint8_t Sound_Shutdown_Sound_Engine(void) {
	int index;

	if (g_directSound == NULL) {
		return 1;
	}

	index = 1535;
	do {
		Sound_UnloadEffectByName(g_soundDefs[index].name);
		--index;
	} while (index >= 0);

	if (g_sound3DListener != NULL) {
		g_sound3DListener->lpVtbl->Release(g_sound3DListener);
		g_sound3DListener = NULL;
	}
	if (g_primarySoundBuffer != NULL) {
		g_primarySoundBuffer->lpVtbl->Release(g_primarySoundBuffer);
	}

	g_directSound = NULL;
	g_sound3DEnabled = 0;
	for (index = 0; index < 1536; ++index) {
		g_soundDefs[index].buffer = NULL;
		g_soundDefs[index].buffer3D = NULL;
		g_soundDefs[index].name[0] = '\0';
	}
	memset(g_soundQueue, 0, sizeof(g_soundQueue));
	for (index = 0; index < 32; ++index) {
		g_activeSoundInstances[index].soundId = -1;
		g_activeSoundInstances[index].sequence = 0;
		g_activeSoundInstances[index].buffer = NULL;
		g_activeSoundInstances[index].buffer3D = NULL;
	}
	g_primarySoundBuffer = NULL;
	g_sound3DListener = NULL;
	g_nextSoundInstanceSeq = 0;
	return 1;
}

// FUNCTION: XWA 0x4DAD40
uint8_t Sound_LoadEffectEx(const char* fileName, const char* name, int create3DFlags,
						   uint16_t minDistanceOrRolloff) {
	SoundEffectDef def;
	int soundId;

	if (fileName[0] == '\0' || name[0] == '\0') {
		return 0;
	}
	if (g_soundCount >= 1536 || g_directSound == NULL) {
		return 0;
	}

	soundId = g_soundCount - 1;
	while (soundId >= 0) {
		if (strcmp(name, g_soundDefs[soundId].name) == 0) {
			break;
		}
		--soundId;
	}
	if (soundId != -1) {
		return 1;
	}

	def.name[0] = '\0';
	def.fileName[0] = '\0';
	DebugPrintfChannel(1, "Loading sfx \"%s\" (%s), 3Ds %s.\n", fileName, name,
					   g_sound3DEnabled ? "On" : "Off");
	def.buffer = DirectSound_LoadWaveBuffer(g_directSound, (char*)fileName, create3DFlags);
	if (!def.buffer) {
		return 0;
	}

	def.buffer->lpVtbl->SetCurrentPosition(def.buffer, 0);
	strncpy(def.name, name, sizeof(def.name));
	def.name[sizeof(def.name) - 1] = '\0';
	strncpy(def.fileName, fileName, sizeof(def.fileName));
	def.fileName[sizeof(def.fileName) - 1] = '\0';
	def.currentPriority = 0;
	DebugPrintfChannel(1, "Loaded  sfx \"%s\" (%s).\n", fileName, name);

	if (g_sound3DEnabled) {
		int queryResult =
			def.buffer->lpVtbl->QueryInterface(def.buffer, &IID_IDirectSound3DBuffer, (void**)&def.buffer3D);
		if (queryResult < 0) {
			def.buffer3D = NULL;
		}
		def.buffer3D->lpVtbl->SetMinDistance(def.buffer3D,
											 (float)minDistanceOrRolloff * g_soundMinDistanceScale, 0);
		def.buffer3D->lpVtbl->SetMaxDistance(def.buffer3D, 131072.0f, 0);
	} else {
		def.buffer3D = NULL;
	}

	{
		SoundEffectDef* destination = &g_soundDefs[g_soundCount];
		memcpy(destination, &def, sizeof(def));
	}
	++g_soundCount;
	return def.buffer != NULL;
}

// FUNCTION: XWA 0x4DAD20
uint8_t Sound_LoadEffect(const char* fileName, const char* name, uint16_t minDistanceOrRolloff) {
	return Sound_LoadEffectEx(fileName, name, 0, minDistanceOrRolloff);
}

// FUNCTION: XWA 0x4DAF80
void Sound_UnloadAllEffects(void) {
	int defIndex;

	defIndex = 1535;
	do {
		Sound_UnloadEffectByName(g_soundDefs[defIndex].name);
		--defIndex;
	} while (defIndex >= 0);
}

// FUNCTION: XWA 0x4DC8C0
int Sound_FindEffectByName(const char* name) { return Sound_FindEffectByNameInline(name, g_soundCount); }

#ifndef XWA_MODERN
#pragma function(memcpy)
#endif
// FUNCTION: XWA 0x4DB1A0
int Sound_QueueEffect(int soundId, int param2, int loop, int priority, int volume, int pan, int pitch,
					  unsigned int sourceObjOrPointRef) {
	int queueIndex;
	int duplicateCount;

	queueIndex = 0;
	if (g_directSound == NULL) {
		return 0;
	}
	if (soundId < 0 || soundId >= g_soundCount) {
		DebugPrintfChannel(128, "Failed to queue sound; out of range.\n");
		return -1;
	}

	duplicateCount = 0;
	while (queueIndex < g_soundQueueCount) {
		SoundQueueEntry* entry;

		entry = &g_soundQueue[queueIndex];
		if (entry->priority < priority) {
			break;
		}

		if (entry->soundId == soundId) {
			if (entry->loop == loop && entry->param2 == param2 && entry->priority == priority &&
				entry->volume == volume && entry->pan == pan && entry->pitch == pitch &&
				entry->sourceObjOrPointRef == sourceObjOrPointRef && ++duplicateCount >= 1) {
				DebugPrintfChannel(128, "Failed to queue sound %d (%s), pri %d; dupecount >= 1.\n", soundId,
								   g_soundDefs[soundId].name, priority);
				return -1;
			}
		}

		if ((soundId == g_sfxIds[52] || soundId == g_sfxIds[53] || soundId == g_sfxIds[54] ||
			 soundId == g_sfxIds[55] || soundId == g_sfxIds[56] || soundId == g_sfxIds[57]) &&
			(entry->soundId == g_sfxIds[52] || entry->soundId == g_sfxIds[53] ||
			 entry->soundId == g_sfxIds[54] || entry->soundId == g_sfxIds[55] ||
			 entry->soundId == g_sfxIds[56] || entry->soundId == g_sfxIds[57])) {
			DebugPrintfChannel(128, "Not queueing sound %d (%s), pri %d; dupe.\n", soundId,
							   g_soundDefs[soundId].name, priority);
			return -1;
		}

		++queueIndex;
	}

	if (queueIndex == 4) {
		DebugPrintfChannel(128, "Failed to queue sound %d (%s), pri %d; queue full.\n", soundId,
						   g_soundDefs[soundId].name, priority);
		return -1;
	}

#ifdef XWA_MODERN
	// DEVIATION: the original uses memcpy here, but the source and destination
	// overlap (dest = src + one entry), so memcpy is undefined behavior on the
	// overlap. memmove is the overlap-safe equivalent for modern builds.
	memmove(&g_soundQueue[queueIndex + 1], &g_soundQueue[queueIndex],
			(size_t)(g_soundQueueCount - queueIndex) * sizeof(g_soundQueue[0]));
#else
	memcpy(&g_soundQueue[queueIndex + 1], &g_soundQueue[queueIndex],
		   (size_t)(g_soundQueueCount - queueIndex) * sizeof(g_soundQueue[0]));
#endif

	g_soundQueue[queueIndex].soundId = soundId;
	g_soundQueue[queueIndex].loop = loop;
	g_soundQueue[queueIndex].param2 = param2;
	g_soundQueue[queueIndex].priority = priority;
	g_soundQueue[queueIndex].volume = volume;
	g_soundQueue[queueIndex].pan = pan;
	g_soundQueue[queueIndex].sourceObjOrPointRef = sourceObjOrPointRef;
	g_soundQueue[queueIndex].pitch = pitch;
	g_soundQueue[queueIndex].hasWorldPosition = 0;

	DebugPrintfChannel(128, "Queued sound %d (%s), v %d, p %d, l %d, pn %d, pi %d, s %d.\n", soundId,
					   g_soundDefs[soundId].name, volume, priority, loop, pan, pitch, sourceObjOrPointRef);

	++g_soundQueueCount;
	if (g_soundQueueCount > 4) {
		g_soundQueueCount = 4;
	}
	return queueIndex;
}
#ifndef XWA_MODERN
#pragma intrinsic(memcpy)
#endif

// FUNCTION: XWA 0x4DC850
int Sound_CountPlayingInstances(int soundId) {
	uint32_t status;
	int count;
	int index;

	if (g_directSound == NULL) {
		return 0;
	}
	if (soundId == -1) {
		return 0;
	}

	count = 0;
	index = 0;
	while (index < g_maxActiveSounds) {
		if (g_activeSoundInstances[index].soundId == soundId &&
			g_activeSoundInstances[index].buffer->lpVtbl->GetStatus(g_activeSoundInstances[index].buffer,
																	&status) == 0 &&
			(status & 5u) != 0) {
			++count;
		}
		++index;
	}

	return count;
}

// FUNCTION: XWA 0x4DC810
int Sound_SetEffectCurrentPriority(int soundId, int priority) {
	int clampedPriority;

	if (soundId == -1) {
		return 0;
	}

	clampedPriority = priority;
	if (clampedPriority > 255) {
		clampedPriority = 255;
	}
	if (clampedPriority < 0) {
		clampedPriority = 0;
	}

	g_soundDefs[soundId].currentPriority = (uint8_t)clampedPriority;
	return 1;
}

// FUNCTION: XWA 0x4DC780
uint8_t Sound_SetLatestInstanceFrequency(int soundId, int frequency) {
	int latestSequence;
	int latestIndex;
	int index;
	int setFrequencyResult;

	if (g_directSound == NULL) {
		return 0;
	}
	if (soundId == -1) {
		return 0;
	}

	latestSequence = -1;
	latestIndex = -1;
	for (index = 0; index < g_maxActiveSounds; ++index) {
		if (g_activeSoundInstances[index].soundId == soundId &&
			g_activeSoundInstances[index].sequence > latestSequence) {
			latestSequence = g_activeSoundInstances[index].sequence;
			latestIndex = index;
		}
	}

	if (latestIndex == -1) {
		return 0;
	}
	setFrequencyResult = g_activeSoundInstances[latestIndex].buffer->lpVtbl->SetFrequency(
		g_activeSoundInstances[latestIndex].buffer, frequency);
	return setFrequencyResult == 0;
}

// FUNCTION: XWA 0x4DC6C0
uint8_t Sound_SetLatestInstancePan(int soundId, int pan) {
	int latestSequence;
	int latestIndex;
	int index;
	int clampedPan;

	if (g_directSound == NULL) {
		return 0;
	}
	if (soundId == -1) {
		return 0;
	}

	latestSequence = -1;
	latestIndex = -1;
	for (index = 0; index < g_maxActiveSounds; ++index) {
		if (g_activeSoundInstances[index].soundId == soundId &&
			g_activeSoundInstances[index].sequence > latestSequence) {
			latestSequence = g_activeSoundInstances[index].sequence;
			latestIndex = index;
		}
	}

	if (latestIndex == -1) {
		return 0;
	}

	clampedPan = pan;
	if (clampedPan > 127) {
		clampedPan = 127;
	}
	if (clampedPan < 0) {
		clampedPan = 0;
	}

	return g_activeSoundInstances[latestIndex].buffer->lpVtbl->SetPan(
			   g_activeSoundInstances[latestIndex].buffer, 2000 * (clampedPan - 63) / 63) == 0;
}

// FUNCTION: XWA 0x4DC5D0
uint8_t Sound_SetLatestInstanceVolume(int soundId, int volume) {
	int latestSequence;
	int latestIndex;
	int index;
	int clampedVolume;
	int setVolumeResult;

	if (g_directSound == NULL) {
		return 0;
	}
	if (soundId == -1) {
		return 0;
	}

	latestSequence = -1;
	latestIndex = -1;
	for (index = 0; index < g_maxActiveSounds; ++index) {
		if (g_activeSoundInstances[index].soundId == soundId &&
			g_activeSoundInstances[index].sequence > latestSequence) {
			latestSequence = g_activeSoundInstances[index].sequence;
			latestIndex = index;
		}
	}

	if (latestIndex == -1) {
		return 0;
	}

	clampedVolume = volume;
	if (clampedVolume > 127) {
		clampedVolume = 127;
	} else if (clampedVolume < 0) {
		clampedVolume = 0;
	}

	DebugPrintfChannel(0x80, "Set volume of %d (%s) to %d.\n", g_activeSoundInstances[latestIndex].soundId,
					   g_soundDefs[soundId].name, clampedVolume);
	setVolumeResult = g_activeSoundInstances[latestIndex].buffer->lpVtbl->SetVolume(
		g_activeSoundInstances[latestIndex].buffer, 2000 * (clampedVolume - 127) / 127);
	return setVolumeResult == 0;
}

// FUNCTION: XWA 0x4DC400
uint8_t Sound_StopOldestInstance(int soundId) {
	int oldestSequence;
	int oldestIndex;
	int index;
	IDirectSoundBuffer* buffer;
	IDirectSound3DBuffer* buffer3D;
	int stopResult;

	if (g_directSound != NULL && soundId != -1) {
		oldestSequence = g_nextSoundInstanceSeq + 1;
		oldestIndex = -1;
		for (index = 0; index < g_maxActiveSounds; ++index) {
			if (g_activeSoundInstances[index].soundId == soundId &&
				g_activeSoundInstances[index].sequence < oldestSequence) {
				oldestSequence = g_activeSoundInstances[index].sequence;
				oldestIndex = index;
			}
		}

		if (oldestIndex != -1) {
			buffer = g_activeSoundInstances[oldestIndex].buffer;
			buffer3D = g_activeSoundInstances[oldestIndex].buffer3D;
			if (buffer != NULL) {
				stopResult = buffer->lpVtbl->Stop(buffer);
				if (g_sound3DEnabled) {
					if (buffer3D != NULL) {
						buffer3D->lpVtbl->Release(buffer3D);
					} else {
						DebugPrintfChannel(1, "3D Sound Buffer is null in Stop!\n");
					}
				}
				buffer->lpVtbl->Release(buffer);

				g_activeSoundInstances[oldestIndex].buffer = NULL;
				g_activeSoundInstances[oldestIndex].buffer3D = NULL;
				g_activeSoundInstances[oldestIndex].soundId = -1;
				--g_activeSoundCount;
				{
					uint8_t stopped = stopResult >= 0;
					stopped &= 1;
					return stopped;
				}
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4DBDE0
uint8_t Sound_DuplicateOrCreateHardwareBuffer(IDirectSoundBuffer* sourceBuffer,
											  IDirectSoundBuffer** outBuffer) {
	IDirectSoundBuffer** output;
	DSBufferCaps sourceCaps;
	DSWaveFormat sourceFormat;
	DSBufferDesc destinationDesc;
	IDirectSoundBuffer* destinationBuffer;
	uint32_t sourceBufferBytes;
	uint32_t destinationBufferBytes;
	int createResult;

	*outBuffer = NULL;
	if (!g_soundHardware3DBuffersAvailable) {
		IDirectSound* device = (IDirectSound*)g_directSound;
		return device->lpVtbl->DuplicateSoundBuffer(device, sourceBuffer, outBuffer) >= 0;
	}

	memset(&sourceCaps, 0, sizeof(sourceCaps));
	sourceCaps.dwSize = 20;
	if (sourceBuffer->lpVtbl->GetCaps(sourceBuffer, &sourceCaps) < 0) {
		DebugPrintfChannel(0x400000, "GetCaps on src failed.\n");
		return 0;
	}

	DebugPrintfChannel(0x400000, "Src caps: Flags %x, Size %d, UlkXferRt %d, CPUOver %d.\n",
					   sourceCaps.dwFlags, sourceCaps.dwBufferBytes, sourceCaps.dwUnlockTransferRate,
					   sourceCaps.dwPlayCpuOverhead);
	sourceBufferBytes = sourceCaps.dwBufferBytes;
	sourceBuffer->lpVtbl->GetFormat(sourceBuffer, &sourceFormat, sizeof(sourceFormat), NULL);
	DebugPrintfChannel(0x400000,
					   "Source wave format: Channels %d, SampleRate %d, BitsPerSample %d, Size %d.\n",
					   sourceFormat.nChannels, sourceFormat.nSamplesPerSec, sourceFormat.wBitsPerSample,
					   sourceFormat.cbSize);

	destinationBufferBytes = sourceBufferBytes;
	memset(&destinationDesc, 0, sizeof(destinationDesc));
	destinationBuffer = NULL;
	if (g_flightConfAudio22k && sourceFormat.nSamplesPerSec != 22050) {
		uint32_t sourceRate = sourceFormat.nSamplesPerSec;

		sourceFormat.nSamplesPerSec = 22050;
		sourceFormat.cbSize = (uint16_t)(22050u * sourceFormat.cbSize / sourceRate);
		destinationBufferBytes = 22050u * sourceBufferBytes / sourceRate;
		sourceFormat.nAvgBytesPerSec = 22050u * sourceFormat.nBlockAlign;
	}

	output = outBuffer;
	destinationDesc.dwBufferBytes = destinationBufferBytes;
	destinationDesc.lpwfxFormat = &sourceFormat;
	destinationDesc.dwSize = sourceCaps.dwSize;
	destinationDesc.dwFlags = DSBCAPS_STATIC_XWA | DSBCAPS_LOCHARDWARE_XWA | DSBCAPS_CTRL3D_XWA |
							  DSBCAPS_CTRLFREQUENCY_XWA | DSBCAPS_CTRLVOLUME_XWA |
							  DSBCAPS_MUTE3DATMAXDISTANCE_XWA;
	{
		IDirectSound* device = (IDirectSound*)g_directSound;
		createResult = device->lpVtbl->CreateSoundBuffer(device, &destinationDesc, &destinationBuffer, NULL);
	}

	if (createResult < 0) {
		DebugPrintfChannel(0x400000, "Error creating hardware buffer: %0.8lx.\n", createResult);
		destinationBuffer = NULL;
		switch (createResult) {
			case -2147467263:
				DebugPrintfChannel(0x400000, "  Error code is DSERR_UNSUPPORTED.\n");
				break;
			case -2147221232:
				DebugPrintfChannel(0x400000, "  Error code is DSERR_NOAGGREGATION.\n");
				break;
			case -2147024882:
				DebugPrintfChannel(0x400000, "  Error code is DSERR_OUTOFMEMORY.\n");
				break;
			case -2147024809:
				DebugPrintfChannel(0x400000, "  Error code is DSERR_INVALIDPARAM.\n");
				break;
			case -2005401590:
				DebugPrintfChannel(0x400000, "  Error code is DSERR_ALLOCATED.\n");
				break;
			case -2005401500:
				DebugPrintfChannel(0x400000, "  Error code is DSERR_BADFORMAT.\n");
				break;
			case -2005401550:
				DebugPrintfChannel(0x400000, "  Error code is DSERR_INVALIDCALL.\n");
				break;
			case -2005401430:
				DebugPrintfChannel(0x400000, "  Error code is DSERR_UNINITIALIZED.\n");
				break;
			default:
				DebugPrintfChannel(0x400000, "  Error code is UNKNOWN.\n");
				break;
		}
	} else {
		DSWaveFormat destinationFormat;

		destinationBuffer->lpVtbl->GetFormat(destinationBuffer, &destinationFormat, sizeof(destinationFormat),
											 NULL);
		DebugPrintfChannel(
			0x400000, "Destination wave format: Channels %d, SampleRate %d, BitsPerSample %d, Size %d.\n",
			destinationFormat.nChannels, destinationFormat.nSamplesPerSec, destinationFormat.wBitsPerSample,
			destinationFormat.cbSize);
	}

	*output = destinationBuffer;
	if (!*output) {
		DebugPrintfChannel(0x400000, "CreateHardwareBuffer failed.\n");
		return 0;
	}

	{
		IDirectSoundBuffer* playbackBuffer = *output;
		void* sourceAudio;
		void* destinationAudio;
		uint32_t sourceAudioBytes;
		uint32_t destinationAudioBytes;
		uint32_t unusedAudioBytes;

		playbackBuffer->lpVtbl->GetCaps(playbackBuffer, &sourceCaps);
		if (sourceBuffer->lpVtbl->Lock(sourceBuffer, 0, sourceBufferBytes, &sourceAudio, &sourceAudioBytes,
									   NULL, &unusedAudioBytes, DSBLOCK_ENTIREBUFFER_XWA) >= 0) {
			int destinationLockResult = playbackBuffer->lpVtbl->Lock(
				playbackBuffer, 0, sourceBufferBytes, &destinationAudio, &destinationAudioBytes, NULL,
				&unusedAudioBytes, DSBLOCK_ENTIREBUFFER_XWA);
			if (destinationLockResult < 0) {
				sourceBuffer->lpVtbl->Unlock(sourceBuffer, sourceAudio, sourceAudioBytes, NULL, 0);
			} else {
				if (sourceBufferBytes == sourceCaps.dwBufferBytes) {
					memcpy(destinationAudio, sourceAudio, sourceBufferBytes);
				} else if (sourceBufferBytes == sourceCaps.dwBufferBytes / 2) {
					uint32_t sourceIndex;
					uint32_t destinationIndex = 0;

					DebugPrintfChannel(0x400000, "Converting buffer size from %d to %d.\n", sourceBufferBytes,
									   sourceCaps.dwBufferBytes);
					for (sourceIndex = 0; sourceIndex < sourceBufferBytes; ++sourceIndex) {
						((uint8_t*)destinationAudio)[destinationIndex++] =
							((const uint8_t*)sourceAudio)[sourceIndex];
						((uint8_t*)destinationAudio)[destinationIndex++] =
							((const uint8_t*)sourceAudio)[sourceIndex];
					}
				} else {
					DebugPrintfChannel(0x400000, "Unsupported buffer size conversion, %d to %d.\n",
									   sourceBufferBytes, sourceCaps.dwBufferBytes);
				}
				playbackBuffer->lpVtbl->Unlock(playbackBuffer, destinationAudio, destinationAudioBytes,
											   &unusedAudioBytes, 0);
				sourceBuffer->lpVtbl->Unlock(sourceBuffer, sourceAudio, sourceAudioBytes, NULL, 0);
			}
		}
	}

	{
		IDirectSoundBuffer* playbackBuffer = *outBuffer;
		IDirectSound3DBuffer* destinationBuffer3D;
		IDirectSound3DBuffer* sourceBuffer3D;

		if (sourceBuffer->lpVtbl->QueryInterface(sourceBuffer, &IID_IDirectSound3DBuffer,
												 (void**)&sourceBuffer3D) < 0) {
			DebugPrintfChannel(0x400000, "Unable to get 3Ds interface on src.\n");
			return 1;
		}
		if (playbackBuffer->lpVtbl->QueryInterface(playbackBuffer, &IID_IDirectSound3DBuffer,
												   (void**)&destinationBuffer3D) < 0) {
			sourceBuffer3D->lpVtbl->Release(sourceBuffer3D);
			DebugPrintfChannel(0x400000, "Unable to get 3Ds interface on dest. o.O\n");
			return 1;
		}

		{
			float minDistance;
			float maxDistance;

			sourceBuffer3D->lpVtbl->GetMinDistance(sourceBuffer3D, &minDistance);
			sourceBuffer3D->lpVtbl->GetMaxDistance(sourceBuffer3D, &maxDistance);
			destinationBuffer3D->lpVtbl->SetMinDistance(destinationBuffer3D, minDistance, 0);
			destinationBuffer3D->lpVtbl->SetMaxDistance(destinationBuffer3D, maxDistance, 0);
		}
		destinationBuffer3D->lpVtbl->Release(destinationBuffer3D);
		sourceBuffer3D->lpVtbl->Release(sourceBuffer3D);
	}

	return 1;
}

// FUNCTION: XWA 0x4DB540
uint8_t Sound_PlayEffectNow(int soundId, int param2, int loop, int priority, int volume, int pan, int pitch,
							unsigned int sourceObjOrPointRef, uint8_t hasWorldPosition, int worldX,
							int worldY, int worldZ) {
	int slot;
	int loopFlag;
	int playResult;
	int soundDefOffset;
	uint32_t status;
	IDirectSoundBuffer* outBuffer;
	IDirectSound3DBuffer* buffer3D;

	(void)param2;

	if (!g_directSound) {
		return 0;
	}

	if (g_activeSoundCount >= g_maxActiveSounds) {
		for (slot = 0; slot < g_maxActiveSounds; ++slot) {
			if (g_activeSoundInstances[slot].soundId == -1) {
				continue;
			}
			if (g_activeSoundInstances[slot].buffer->lpVtbl->GetStatus(g_activeSoundInstances[slot].buffer,
																	   &status) == 0 &&
				(status & 5) != 0) {
				continue; /* still playing */
			}
			if (g_sound3DEnabled) {
				g_activeSoundInstances[slot].buffer3D->lpVtbl->Release(g_activeSoundInstances[slot].buffer3D);
			}
			g_activeSoundInstances[slot].buffer->lpVtbl->Release(g_activeSoundInstances[slot].buffer);
			g_activeSoundInstances[slot].soundId = -1;
			g_activeSoundInstances[slot].buffer = NULL;
			g_activeSoundInstances[slot].buffer3D = NULL;
			--g_activeSoundCount;
			DebugPrintfChannel(128, "Buffer %d is done playing; releasing.\n", slot);
			break;
		}

		if (slot == g_maxActiveSounds) {
			int lowestPriority = priority;
			int i;
			for (i = 0; i < g_maxActiveSounds; ++i) {
				if (lowestPriority > g_activeSoundInstances[i].priority) {
					slot = i;
					lowestPriority = g_activeSoundInstances[i].priority;
				}
			}
			if (slot != g_maxActiveSounds) {
				if (g_activeSoundInstances[slot].soundId == soundId) {
					DebugPrintfChannel(128, "Restarting sound %d (%s) (priority).\n",
									   g_activeSoundInstances[slot].soundId,
									   g_soundDefs[g_activeSoundInstances[slot].soundId].name);
					g_activeSoundInstances[slot].buffer->lpVtbl->SetCurrentPosition(
						g_activeSoundInstances[slot].buffer, 0);
					g_activeSoundInstances[slot].sequence = g_nextSoundInstanceSeq;
					g_nextSoundInstanceSeq++;
					g_activeSoundInstances[slot].sourceObjOrPointRef = sourceObjOrPointRef;
					g_activeSoundInstances[slot].priority = priority;
					if (sourceObjOrPointRef != 0xFFFF) {
						g_activeSoundInstances[slot].sourceCreationIdx =
							g_objectTable[sourceObjOrPointRef].objectSignature;
					}
					return 1;
				}

				DebugPrintfChannel(128, "Replacing sound %d (%s) with %d (%s) (priority %d with %d).\n",
								   g_activeSoundInstances[slot].soundId,
								   g_soundDefs[g_activeSoundInstances[slot].soundId].name, soundId,
								   g_soundDefs[soundId].name,
								   g_soundDefs[g_activeSoundInstances[slot].soundId].currentPriority,
								   priority);
				g_activeSoundInstances[slot].buffer->lpVtbl->Stop(g_activeSoundInstances[slot].buffer);
				if (g_sound3DEnabled) {
					g_activeSoundInstances[slot].buffer3D->lpVtbl->Release(
						g_activeSoundInstances[slot].buffer3D);
				}
				g_activeSoundInstances[slot].buffer->lpVtbl->Release(g_activeSoundInstances[slot].buffer);
				g_activeSoundInstances[slot].soundId = -1;
				g_activeSoundInstances[slot].buffer = NULL;
				g_activeSoundInstances[slot].buffer3D = NULL;
				--g_activeSoundCount;
			} else {
				DebugPrintfChannel(128, "Couldn't find sound to override/replace.  Sound failed.\n");
				return 0;
			}
		}
	} else {
		for (slot = 0; slot < g_maxActiveSounds; ++slot) {
			if (g_activeSoundInstances[slot].soundId == -1) {
				break;
			}
		}
	}

	loopFlag = (loop == 1);
	soundDefOffset = soundId;
	if (sourceObjOrPointRef != 0xFFFF || hasWorldPosition) {
		Sound_DuplicateOrCreateHardwareBuffer(g_soundDefs[soundId].buffer, &outBuffer);
	} else {
		IDirectSound* device = (IDirectSound*)g_directSound;
		device->lpVtbl->DuplicateSoundBuffer(device, g_soundDefs[soundId].buffer, &outBuffer);
	}
	if (!outBuffer) {
		DebugPrintfChannel(1, "Failed to duplicate sound buffer.  Sound failed.\n");
		if (g_soundHardware3DBuffersAvailable) {
			if (g_maxActiveSounds > 4) {
				--g_maxActiveSounds;
				DebugPrintfChannel(0x400000,
								   "Reducing max sounds to %d, because we're running out of buffers.\n",
								   g_maxActiveSounds);
				if (g_activeSoundInstances[g_maxActiveSounds].soundId != -1) {
					int freeSlot;

					DebugPrintfChannel(0x400000, "Last sound was in use...\n");
					freeSlot = 0;
					while (freeSlot < g_maxActiveSounds) {
						if (g_activeSoundInstances[freeSlot].soundId == -1) {
							DebugPrintfChannel(0x400000, "Moving from slot %d to slot %d.\n",
											   g_maxActiveSounds, freeSlot);
							memcpy(&g_activeSoundInstances[freeSlot],
								   &g_activeSoundInstances[g_maxActiveSounds],
								   sizeof(g_activeSoundInstances[0]));
							g_activeSoundInstances[g_maxActiveSounds].soundId = -1;
							g_activeSoundInstances[g_maxActiveSounds].buffer = NULL;
							g_activeSoundInstances[g_maxActiveSounds].buffer3D = NULL;
							return 0;
						}
						++freeSlot;
					}
					return 0;
				}
			} else {
				DebugPrintfChannel(0x400000, "We're running out of buffers, but can't reduce max sounds.  "
											 "Rendering sound in software...\n");
				g_soundHardware3DBuffersAvailable = 0;
				Sound_DuplicateOrCreateHardwareBuffer(g_soundDefs[soundDefOffset].buffer, &outBuffer);
				g_soundHardware3DBuffersAvailable = 1;
				return 0;
			}
		}
		return 0;
	}

	{
		uint32_t frequency = (uint32_t)pitch;
		DSWaveFormat fmt;
		if (pitch == -1) {
			if (outBuffer->lpVtbl->GetFormat(outBuffer, &fmt, sizeof(fmt), NULL) < 0) {
				DebugPrintfChannel(1, "Error getting pitch information.\n");
				return 0;
			}
			frequency = fmt.nSamplesPerSec;
		}
		DebugPrintfChannel(1, "Setting initial pitch to %d.\n", fmt.nSamplesPerSec);
		outBuffer->lpVtbl->SetFrequency(outBuffer, frequency);
	}
	outBuffer->lpVtbl->SetCurrentPosition(outBuffer, 0);

	if (g_sound3DEnabled) {
		outBuffer->lpVtbl->QueryInterface(outBuffer, &IID_IDirectSound3DBuffer, (void**)&buffer3D);
		if (!buffer3D) {
			DebugPrintfChannel(1, "Buffer to 3D failed.\n");
			return 0;
		}
		if (sourceObjOrPointRef != 0xFFFF) {
			ObjectRecord* obj = &g_objectTable[sourceObjOrPointRef];
			struct MobileObject* mobj = obj->mobj;
			float velX, velY, velZ, posX, posY, posZ;
			if (mobj) {
				if (mobj->moveVectorDirty) {
					FVIEW_calcrotatemove(obj->pitch, obj->yaw, NULL);
					velX = (float)Xwa_Q15Mul((int)mobj->speed, g_fviewMoveX_Q15);
					velY = (float)Xwa_Q15Mul((int)mobj->speed, g_fviewMoveY_Q15);
					velZ = (float)Xwa_Q15Mul((int)mobj->speed, g_fviewMoveZ_Q15);
				} else {
					velX = (float)Xwa_Q15Mul((int)mobj->speed, mobj->moveX);
					velY = (float)Xwa_Q15Mul((int)mobj->speed, mobj->moveY);
					velZ = (float)Xwa_Q15Mul((int)mobj->speed, mobj->moveZ);
				}
				posX = (float)mobj->prevWorldX;
				posY = (float)mobj->prevWorldY;
				posZ = (float)mobj->prevWorldZ;
			} else {
				Mission_ResolveObjectOrMissionPointWorldLoc(sourceObjOrPointRef, 0, 0, 0);
				velX = velY = velZ = 0.0f;
				posX = (float)worldlocx;
				posY = (float)worldlocy;
				posZ = (float)worldlocz;
			}
			buffer3D->lpVtbl->SetPosition(buffer3D, posX, posY, -posZ, 0);
			buffer3D->lpVtbl->SetVelocity(buffer3D, velX, velY, -velZ, 0);
		} else {
			if (hasWorldPosition) {
				buffer3D->lpVtbl->SetPosition(buffer3D, (float)worldX, (float)worldY, -(float)worldZ, 0);
				buffer3D->lpVtbl->SetVelocity(buffer3D, 0.0f, 0.0f, 0.0f, 0);
			} else {
				buffer3D->lpVtbl->SetMode(buffer3D, 2, 0); /* DS3DMODE_DISABLE */
			}
		}
	} else {
		int clampedPan = pan;
		buffer3D = NULL;
		if (clampedPan > 127) {
			clampedPan = 127;
		} else if (clampedPan < 0) {
			clampedPan = 0;
		}
		outBuffer->lpVtbl->SetPan(outBuffer, 2000 * (clampedPan - 63) / 63);
	}

	{
		int clampedVolume = volume;
		if (clampedVolume > 127) {
			clampedVolume = 127;
		} else if (clampedVolume < 0) {
			clampedVolume = 0;
		}
		outBuffer->lpVtbl->SetVolume(outBuffer, 2000 * (clampedVolume - 127) / 127);
	}

	g_soundDefs[soundId].currentPriority = (uint8_t)priority;

	playResult = outBuffer->lpVtbl->Play(outBuffer, 0, 0, (uint32_t)loopFlag);
	if (playResult == (int)0x88780096u) { /* DSERR_BUFFERLOST */
		int reloadResult;

		DebugPrintfChannel(1, "Sound buffer %d (%s)lost.  Reloading...\n", soundId,
						   g_soundDefs[soundId].name);
		reloadResult =
			DirectSound_ReloadWaveBuffer(g_soundDefs[soundId].buffer, g_soundDefs[soundId].fileName);
		if (reloadResult != 1) {
			return 0;
		}
		outBuffer->lpVtbl->SetCurrentPosition(outBuffer, 0);
		playResult = outBuffer->lpVtbl->Play(outBuffer, 0, 0, (uint32_t)loopFlag);
		if (playResult == 0) {
			DebugPrintfChannel(1, "Successfully played reloaded sound effect.\n");
		} else {
			DebugPrintfChannel(1, "Failed to play reloaded buffer.\n");
			return 0;
		}
	}
	if (playResult) {
		DebugPrintfChannel(1, "Failed to play buffer.\n");
		return 0;
	}

	g_activeSoundInstances[slot].soundId = soundId;
	g_activeSoundInstances[slot].buffer = outBuffer;
	g_activeSoundInstances[slot].buffer3D = buffer3D;
	g_activeSoundInstances[slot].sequence = g_nextSoundInstanceSeq;
	g_nextSoundInstanceSeq++;
	g_activeSoundInstances[slot].sourceObjOrPointRef = sourceObjOrPointRef;
	g_activeSoundInstances[slot].priority = priority;
	if (sourceObjOrPointRef != 0xFFFF) {
		g_activeSoundInstances[slot].sourceCreationIdx = g_objectTable[sourceObjOrPointRef].objectSignature;
	}
	{
		int activeSoundCount = g_activeSoundCount;
		++activeSoundCount;
		g_activeSoundCount = activeSoundCount;
	}
	return 1;
}

// FUNCTION: XWA 0x4DB130
uint8_t Sound_QueueEffectAtWorldPosition(int soundId, int param2, int loop, int priority, int volume, int pan,
										 int pitch, int worldX, int worldY, int worldZ) {
	int queueIndex = Sound_QueueEffect(soundId, param2, loop, priority, volume, pan, pitch, 0xFFFFu);
	if (queueIndex != -1) {
		g_soundQueue[queueIndex].worldX = worldX;
		g_soundQueue[queueIndex].worldY = worldY;
		g_soundQueue[queueIndex].worldZ = worldZ;
		g_soundQueue[queueIndex].hasWorldPosition = 1;
		return 1;
	}
	return 0;
}

// FUNCTION: XWA 0x4DB440
uint8_t Sound_FlushQueuedEffects(void) {
	int index;

	for (index = 0; index < g_maxActiveSounds; ++index) {
		if (g_activeSoundInstances[index].soundId != -1) {
			int currentPriority = g_activeSoundInstances[index].priority;
			if (currentPriority > 0 && currentPriority < 126) {
				g_activeSoundInstances[index].priority = currentPriority - 1;
			}
		}
	}

	for (index = 0; index < g_soundQueueCount; ++index) {
		SoundQueueEntry* entry = &g_soundQueue[index];
		DebugPrintfChannel(128, "Flushing sound %d (%s), v %d, p %d, l %d, pn %d, pi %d, s %d.\n",
						   entry->soundId, g_soundDefs[entry->soundId].name, entry->volume, entry->priority,
						   entry->loop, entry->pan, entry->pitch, entry->sourceObjOrPointRef);
		if (!Sound_PlayEffectNow(entry->soundId, entry->param2, entry->loop, entry->priority, entry->volume,
								 entry->pan, entry->pitch, entry->sourceObjOrPointRef,
								 entry->hasWorldPosition, entry->worldX, entry->worldY, entry->worldZ)) {
			DebugPrintfChannel(128, "Flushed sound failed.\n");
		}
	}
	if (g_soundQueueCount > 0) {
		Sound_Update3DListenerAndSources();
	}
	g_soundQueueCount = 0;
	return 1;
}

static inline void Sound_SetSpeedAlongDirection(float* output, int direction, uint16_t speed) {
	*output = (float)Xwa_Q15MulReuseFirstSlot(direction, (int)speed);
}

// FUNCTION: XWA 0x4DC930
void Sound_Update3DListenerAndSources(void) {
	int listenerObjIdx;
	float fwdX, fwdY, fwdZ, upX, upY, upZ;
	float posX, posY, posZ;
	float velX, velY, velZ;
	uint32_t status;
	int index;

	if (!g_directSound || !g_sound3DEnabled || g_sound3DListenerLastGameTime == g_gameTime) {
		return;
	}
	g_sound3DListenerLastGameTime = g_gameTime;

	posX = (float)g_players[g_localPlayer].viewState.savedTargetX;
	posY = (float)g_players[g_localPlayer].viewState.savedTargetY;
	velZ = 0.0f;
	velY = 0.0f;
	posZ = (float)g_players[g_localPlayer].viewState.savedTargetZ;
	velX = 0.0f;

	FVIEW_SetObjectTransform(
		g_players[g_localPlayer].viewState.viewRoll, g_players[g_localPlayer].viewState.viewPitch,
		g_players[g_localPlayer].viewState.viewYaw, g_players[g_localPlayer].viewState.viewAngleD, NULL);
	fwdX = (float)g_fviewFwdX_Q15 * g_soundQ15ToFloatScale;
	listenerObjIdx = g_players[g_localPlayer].objectIndex;
	fwdY = (float)g_fviewFwdY_Q15 * g_soundQ15ToFloatScale;
	fwdZ = (float)g_fviewFwdZ_Q15 * g_soundQ15ToFloatScale;
	upX = (float)g_fviewUpX_Q15 * g_soundQ15ToFloatScale;
	upY = (float)g_fviewUpY_Q15 * g_soundQ15ToFloatScale;
	upZ = (float)g_fviewUpZ_Q15 * g_soundQ15ToFloatScale;

	if (listenerObjIdx != 0xFFFF) {
		ObjectRecord* obj = &g_objectTable[listenerObjIdx];
		struct MobileObject* mobj = obj->mobj;
		if (mobj) {
			if (mobj->moveVectorDirty) {
				FVIEW_calcrotatemove(obj->pitch, obj->yaw, NULL);
				Sound_SetSpeedAlongDirection(&velX, g_fviewMoveX_Q15, mobj->speed);
				Sound_SetSpeedAlongDirection(&velY, g_fviewMoveY_Q15, mobj->speed);
				Sound_SetSpeedAlongDirection(&velZ, g_fviewMoveZ_Q15, mobj->speed);
			} else {
				Sound_SetSpeedAlongDirection(&velX, (int)mobj->moveX, mobj->speed);
				Sound_SetSpeedAlongDirection(&velY, (int)mobj->moveY, mobj->speed);
				Sound_SetSpeedAlongDirection(&velZ, (int)mobj->moveZ, mobj->speed);
			}
		}
	}

	if (g_sound3DEnabled) {
		g_sound3DListener->lpVtbl->SetPosition(g_sound3DListener, posX, posY, -posZ, 1);
		g_sound3DListener->lpVtbl->SetVelocity(g_sound3DListener, velX, velY, -velZ, 1);
		g_sound3DListener->lpVtbl->SetOrientation(g_sound3DListener, fwdX, fwdY, -fwdZ, upX, upY, -upZ, 1);
	}

	for (index = 0; index < g_maxActiveSounds; ++index) {
		ActiveSoundInstance* inst = &g_activeSoundInstances[index];
		struct MobileObject* mobj;

		if (inst->soundId == -1) {
			continue;
		}
		if (inst->buffer->lpVtbl->GetStatus(inst->buffer, &status) != 0 || (status & 5) == 0) {
			continue;
		}
		if (inst->sourceObjOrPointRef == 0xFFFF) {
			continue;
		}
		if (inst->sourceCreationIdx != g_objectTable[inst->sourceObjOrPointRef].objectSignature) {
			continue;
		}

		mobj = g_objectTable[inst->sourceObjOrPointRef].mobj;
		if (mobj) {
			if (mobj->moveVectorDirty) {
				FVIEW_calcrotatemove(g_objectTable[inst->sourceObjOrPointRef].pitch,
									 g_objectTable[inst->sourceObjOrPointRef].yaw, NULL);
				Sound_SetSpeedAlongDirection(&velX, g_fviewMoveX_Q15, mobj->speed);
				Sound_SetSpeedAlongDirection(&velY, g_fviewMoveY_Q15, mobj->speed);
				Sound_SetSpeedAlongDirection(&velZ, g_fviewMoveZ_Q15, mobj->speed);
			} else {
				Sound_SetSpeedAlongDirection(&velX, (int)mobj->moveX, mobj->speed);
				Sound_SetSpeedAlongDirection(&velY, (int)mobj->moveY, mobj->speed);
				Sound_SetSpeedAlongDirection(&velZ, (int)mobj->moveZ, mobj->speed);
			}
			posX = (float)mobj->prevWorldX;
			posY = (float)mobj->prevWorldY;
			posZ = (float)mobj->prevWorldZ;
		} else {
			Mission_ResolveObjectOrMissionPointWorldLoc(inst->sourceObjOrPointRef, 0, 0, 0);
			velX = velY = velZ = 0.0f;
			posX = (float)worldlocx;
			posY = (float)worldlocy;
			posZ = (float)worldlocz;
		}
		inst->buffer3D->lpVtbl->SetPosition(inst->buffer3D, posX, posY, -posZ, 1);
		inst->buffer3D->lpVtbl->SetVelocity(inst->buffer3D, velX, velY, -velZ, 1);
	}

	g_sound3DListener->lpVtbl->CommitDeferredSettings(g_sound3DListener);
}

// FUNCTION: XWA 0x4DC4E0
uint8_t Sound_StopAllInstances(void) {
	int stopSucceeded;
	int activeIndex;

	stopSucceeded = 1;
	for (activeIndex = 0; activeIndex < g_maxActiveSounds; ++activeIndex) {
		int activeSoundId;

		activeSoundId = g_activeSoundInstances[activeIndex].soundId;
		if (activeSoundId != -1) {
			SoundEffectDef* activeDef;
			signed char instanceStopped;

			activeDef = &g_soundDefs[activeSoundId];
			if (activeDef->name[0] == '\0') {
				instanceStopped = 0;
			} else {
				int soundId;

				soundId = g_soundCount - 1;
				while (soundId >= 0) {
					if (strcmp(activeDef->name, g_soundDefs[soundId].name) == 0) {
						break;
					}
					--soundId;
				}
				instanceStopped = Sound_StopOldestInstance(soundId);
			}
			stopSucceeded &= instanceStopped;
		}
	}

	return stopSucceeded;
}

// FUNCTION: XWA 0x538FE0
int DirectSound_VolumeToMillibels(int volume0To127) {
	int volume;

	volume = volume0To127;
	if (volume > 127) {
		volume = 127;
	}
	if (volume < 0) {
		volume = 0;
	}
	return g_directSoundVolumeMillibelTable[volume];
}
