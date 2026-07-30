#include "xwa/frontend/cutscene.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/linez.h"
#ifdef XWA_MODERN
#include "xwa/audio/cd_audio.h"
#include "xwa/audio/music.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/mission_setup.h"
#endif
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/movie/movie.h"
#ifdef XWA_MODERN
#include "xwa_runtime/runtime/movie_task.h"
#endif
#include "xwa/util/memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0xABD7B8
int g_cutsceneCount = 0;

// GLOBAL: XWA 0xABD7C4
CutsceneEntry* g_cutsceneTable = NULL;

#ifdef XWA_MODERN
static int g_cutscenePlaybackActive;
static int g_cutscenePlaybackPhase;
static int g_cutscenePlaybackIndex;
static int g_cutsceneMoviePending;
#endif

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x55FAA0
int Cutscene_LoadTable(char* fileName) {
	XwaFile* stream;

	if (g_cutsceneTable != NULL) {
		Mem_Free(g_cutsceneTable);
		g_cutsceneTable = NULL;
	}

	g_cutsceneCount = 0;

	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "r");
	if (stream == NULL) {
		return 0;
	}

	{
		int recordCount = 0;
		int allocationSize;
		char* line;
		char linePrefix;
		CutsceneEntry* table;

		linePrefix = '*';
		while (1) {
#ifdef XWA_MODERN
			line = File_ReadLine(stream, g_frontendScratchBuffer, 255) ? g_frontendScratchBuffer : NULL;
#else
			line = fgets(g_frontendScratchBuffer, 255, stream);
#endif
			if (line == NULL) {
				break;
			}
			if (g_frontendScratchBuffer[0] == linePrefix) {
				++recordCount;
			}
		}

		File_Seek(stream, 0, SEEK_SET);

		allocationSize = (int)(sizeof(CutsceneEntry) * (size_t)recordCount);
		table = (CutsceneEntry*)Mem_Alloc((size_t)allocationSize);
		g_cutsceneTable = table;
		if (table == NULL) {
			File_Close(stream);
			return 0;
		}

		memset(table, 0, (size_t)allocationSize);
		g_cutsceneCount = 0;
		linePrefix = '/';

		while ((unsigned int)g_cutsceneCount < (unsigned int)recordCount) {
			do {
#ifdef XWA_MODERN
				line = File_ReadLine(stream, g_frontendScratchBuffer, 255) ? g_frontendScratchBuffer : NULL;
#else
				line = fgets(g_frontendScratchBuffer, 255, stream);
#endif
				if (line == NULL) {
					File_Close(stream);
					return 1;
				}
			} while (line[0] == linePrefix && line[1] == linePrefix);

			if (g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] == '\n') {
				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] = '\0';
			}
			memcpy(g_cutsceneTable[g_cutsceneCount].movieName, g_frontendScratchBuffer,
				   sizeof(g_cutsceneTable[g_cutsceneCount].movieName));

			do {
#ifdef XWA_MODERN
				line = File_ReadLine(stream, g_frontendScratchBuffer, 255) ? g_frontendScratchBuffer : NULL;
#else
				line = fgets(g_frontendScratchBuffer, 255, stream);
#endif
				if (line == NULL) {
					File_Close(stream);
					return 1;
				}
			} while (line[0] == linePrefix && line[1] == linePrefix);

			if (g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] == '\n') {
				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] = '\0';
			}
			memcpy(g_cutsceneTable[g_cutsceneCount].thumbnailSprite, g_frontendScratchBuffer,
				   sizeof(g_cutsceneTable[g_cutsceneCount].thumbnailSprite));

			do {
#ifdef XWA_MODERN
				line = File_ReadLine(stream, g_frontendScratchBuffer, 255) ? g_frontendScratchBuffer : NULL;
#else
				line = fgets(g_frontendScratchBuffer, 255, stream);
#endif
				if (line == NULL) {
					File_Close(stream);
					return 1;
				}
			} while (line[0] == linePrefix && line[1] == linePrefix);

			if (g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] == '\n') {
				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 1] = '\0';
			}
			memcpy(g_cutsceneTable[g_cutsceneCount].description, Linez_ResolveString(g_frontendScratchBuffer),
				   sizeof(g_cutsceneTable[g_cutsceneCount].description));

			do {
#ifdef XWA_MODERN
				line = File_ReadLine(stream, g_frontendScratchBuffer, 255) ? g_frontendScratchBuffer : NULL;
#else
				line = fgets(g_frontendScratchBuffer, 255, stream);
#endif
				if (line == NULL) {
					g_cutsceneTable[g_cutsceneCount].movieName[0] = '\0';
					File_Close(stream);
					return 1;
				}
			} while (line[0] == linePrefix && line[1] == linePrefix);
			g_cutsceneTable[g_cutsceneCount].missionNumber = atoi(g_frontendScratchBuffer);

			do {
#ifdef XWA_MODERN
				line = File_ReadLine(stream, g_frontendScratchBuffer, 255) ? g_frontendScratchBuffer : NULL;
#else
				line = fgets(g_frontendScratchBuffer, 255, stream);
#endif
				if (line == NULL) {
					File_Close(stream);
					return 1;
				}
			} while (line[0] == linePrefix && line[1] == linePrefix);
			g_cutsceneTable[g_cutsceneCount].playAfterDebriefing = atoi(g_frontendScratchBuffer);

			do {
#ifdef XWA_MODERN
				line = File_ReadLine(stream, g_frontendScratchBuffer, 255) ? g_frontendScratchBuffer : NULL;
#else
				line = fgets(g_frontendScratchBuffer, 255, stream);
#endif
				if (line == NULL) {
					File_Close(stream);
					return 1;
				}
			} while (line[0] == linePrefix && line[1] == linePrefix);

			++g_cutsceneCount;
		}
	}

	File_Close(stream);
	return 1;
}

// FUNCTION: XWA 0x55FDE0
int Cutscene_PlayForCurrentMissionPhase(int phase) {
#ifdef XWA_MODERN
	if (g_cutsceneTable == NULL) {
		return 0;
	}
	if (!g_cutscenePlaybackActive) {
		g_cutscenePlaybackActive = 1;
		g_cutscenePlaybackPhase = phase;
		g_cutscenePlaybackIndex = 0;
		g_cutsceneMoviePending = 0;
	} else if (phase != g_cutscenePlaybackPhase) {
		return -1;
	}

	if (g_cutsceneMoviePending) {
		const int result = XwaMovieTask_GetResult();
		FrontendDisplay_EnableOffscreenRestore();
		Music_ResumeIfInitialized();
		CDAudio_RequestResumePlayback();
		g_cutsceneMoviePending = 0;
		++g_cutscenePlaybackIndex;
		if (!result) {
			g_cutscenePlaybackActive = 0;
			return 0;
		}
	}

	while (g_cutscenePlaybackIndex < g_cutsceneCount) {
		CutsceneEntry* entry = &g_cutsceneTable[g_cutscenePlaybackIndex];
		if (entry->playAfterDebriefing != phase ||
			entry->missionNumber != g_pilotData.missionDescriptionIds[4]) {
			++g_cutscenePlaybackIndex;
			continue;
		}

		CDAudio_SuspendPlayback();
		Music_PauseIfInitialized();
		FrontendDisplay_DisableOffscreenRestore();
		if (Movie_Play(entry->movieName, 0)) {
			g_cutsceneMoviePending = 1;
			return -1;
		}
		FrontendDisplay_EnableOffscreenRestore();
		Music_ResumeIfInitialized();
		CDAudio_RequestResumePlayback();
		g_cutscenePlaybackActive = 0;
		return 0;
	}

	g_cutscenePlaybackActive = 0;
	return 1;
#else
	(void)phase;

	/* TODO: Reimplement Cutscene_PlayForCurrentMissionPhase @ 0x55FDE0. */
	return 1;
#endif
}
