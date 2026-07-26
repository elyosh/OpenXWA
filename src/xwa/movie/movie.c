#include "xwa/movie/movie.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/linez.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_text.h"
#ifdef XWA_MODERN
#include "xwa_runtime/runtime/movie_task.h"
#endif
#include "xwa/util/memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0x9F4B38
int g_movieSubtitleCount = 0;
// GLOBAL: XWA 0x9F4B3C
MovieSubtitleEntry* g_movieSubtitles = NULL;
// GLOBAL: XWA 0x9F4B40
int g_movieSkipRequested = 0;
// GLOBAL: XWA 0x783870
int g_movieSubtitleCurrentIndex = 0;
// GLOBAL: XWA 0x78387C
int g_movieFrameNumber = 0;

// FUNCTION: XWA 0x55BC20
int Movie_Play(const char* name, int noFade) {
#ifdef XWA_MODERN
	return XwaMovieTask_Begin(name, noFade);
#else
	(void)name;
	(void)noFade;

	g_movieSkipRequested = 0;
	/* TODO: Reimplement Movie_Play @ 0x55BC20. */
	return 0;
#endif
}

#ifdef XWA_MODERN
void Movie_FreeSubtitles(void) {
	if (g_movieSubtitles != NULL) {
		Mem_Free(g_movieSubtitles);
		g_movieSubtitles = NULL;
	}
	g_movieSubtitleCount = 0;
	g_movieSubtitleCurrentIndex = 0;
}
#endif

// FUNCTION: XWA 0x55C120
int Movie_LoadSubtitles(const char* moviePath) {
	enum {
		MOVIE_EXTENSION_LENGTH = 4,
		MOVIE_SUBTITLE_LINE_READ_SIZE = 256,
		MOVIE_SUBTITLE_LINE_BUFFER_SIZE = 1024,
		MOVIE_SUBTITLE_DRIVE_SEPARATOR = 1,
		MOVIE_SUBTITLE_DRIVE_PATH_START = 3,
		MOVIE_SUBTITLE_FULL_FADE_STEP = MOVIE_SUBTITLE_FADE_STEPS - 1,
	};

	XwaFile* stream;
	char line[MOVIE_SUBTITLE_LINE_BUFFER_SIZE];
	int loadedCount;
	int entryOffset;
#ifdef XWA_MODERN
	char subtitlePath[512];
	const char* extension;
#endif

	if (g_movieSubtitles != NULL) {
		Mem_Free(g_movieSubtitles);
		g_movieSubtitles = NULL;
		g_movieSubtitleCount = 0;
	}

	g_movieSubtitleCurrentIndex = 0;

#ifdef XWA_MODERN
	if (moviePath == NULL) {
		return 0;
	}
	extension = strrchr(moviePath, '.');
	if (extension == NULL) {
		if (snprintf(subtitlePath, sizeof(subtitlePath), "%s.sub", moviePath) >= (int)sizeof(subtitlePath)) {
			return 0;
		}
	} else {
		const size_t stemLength = (size_t)(extension - moviePath);
		if (stemLength + sizeof(".sub") > sizeof(subtitlePath)) {
			return 0;
		}
		memcpy(subtitlePath, moviePath, stemLength);
		memcpy(subtitlePath + stemLength, ".sub", sizeof(".sub"));
	}
	stream = File_OpenAsset(AERON_VFS_ROOT_ASSET, subtitlePath, "r");
#else
	strcpy(g_frontendScratchBuffer, moviePath);
	g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - MOVIE_EXTENSION_LENGTH] = '\0';
	strcat(g_frontendScratchBuffer, ".sub");

	if (g_frontendScratchBuffer[MOVIE_SUBTITLE_DRIVE_SEPARATOR] == ':') {
		stream = File_OpenAsset(AERON_VFS_ROOT_ASSET,
								&g_frontendScratchBuffer[MOVIE_SUBTITLE_DRIVE_PATH_START], "r");
	} else {
		stream = File_OpenAsset(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "r");
	}
#endif

	if (stream == NULL) {
		return 0;
	}

	g_movieSubtitleCount = 0;
	while (File_ReadLine(stream, line, MOVIE_SUBTITLE_LINE_READ_SIZE)) {
		strcpy(g_frontendScratchBuffer, Linez_ResolveString(line));
		if (g_frontendScratchBuffer[0] == '*') {
			++g_movieSubtitleCount;
		}
	}

	File_Seek(stream, 0, SEEK_SET);

	g_movieSubtitles =
		(MovieSubtitleEntry*)Mem_Alloc(sizeof(MovieSubtitleEntry) * (size_t)g_movieSubtitleCount);
	if (g_movieSubtitles == NULL) {
		File_Close(stream);
		return 0;
	}

	loadedCount = 0;
	entryOffset = 0;
	while (1) {
		MovieSubtitleEntry* entry;
		int red;
		int green;
		int blue;
		int fadeStep;
		size_t textLength;

		if (!File_ReadLine(stream, line, MOVIE_SUBTITLE_LINE_READ_SIZE)) {
			break;
		}

		strcpy(g_frontendScratchBuffer, Linez_ResolveString(line));
		if (line[0] == '/' && line[1] == '/') {
			continue;
		}

		entry = &g_movieSubtitles[entryOffset];
		sscanf(g_frontendScratchBuffer, "%d,%d", &entry->x, &entry->y);

		if (!File_ReadLine(stream, line, MOVIE_SUBTITLE_LINE_READ_SIZE)) {
			break;
		}
		strcpy(g_frontendScratchBuffer, Linez_ResolveString(line));
		sscanf(g_frontendScratchBuffer, "%d,%d,%d", &red, &green, &blue);

		for (fadeStep = 0; fadeStep < MOVIE_SUBTITLE_FADE_STEPS; ++fadeStep) {
			entry->fadeColors[fadeStep] = FrontendDisplay_PackRGB(
				(unsigned char)(((unsigned int)(fadeStep * red)) / MOVIE_SUBTITLE_FULL_FADE_STEP),
				(unsigned char)(((unsigned int)(fadeStep * green)) / MOVIE_SUBTITLE_FULL_FADE_STEP),
				(unsigned char)(((unsigned int)(fadeStep * blue)) / MOVIE_SUBTITLE_FULL_FADE_STEP));
		}

		if (!File_ReadLine(stream, line, MOVIE_SUBTITLE_LINE_READ_SIZE)) {
			break;
		}
		strcpy(g_frontendScratchBuffer, Linez_ResolveString(line));
		sscanf(g_frontendScratchBuffer, "%d,%d", &entry->startFrame, &entry->endFrame);

		if (!File_ReadLine(stream, line, MOVIE_SUBTITLE_LINE_READ_SIZE)) {
			break;
		}
		strcpy(g_frontendScratchBuffer, Linez_ResolveString(line));
		entry->fontSize = atoi(g_frontendScratchBuffer);

		if (!File_ReadLine(stream, line, MOVIE_SUBTITLE_LINE_READ_SIZE)) {
			break;
		}
		strcpy(g_frontendScratchBuffer, Linez_ResolveString(line));
		textLength = strlen(g_frontendScratchBuffer);
		if (textLength != 0 && g_frontendScratchBuffer[textLength - 1] == '\n') {
			g_frontendScratchBuffer[textLength - 1] = '\0';
		}
		strcpy(entry->text, Linez_ResolveString(g_frontendScratchBuffer));

#ifdef XWA_MODERN
		if (!File_ReadLine(stream, line, MOVIE_SUBTITLE_LINE_READ_SIZE)) {
			break;
		}
#else
		if (!File_ReadLine(stream, g_frontendScratchBuffer, MOVIE_SUBTITLE_LINE_READ_SIZE)) {
			break;
		}
		strcpy(g_frontendScratchBuffer, Linez_ResolveString(g_frontendScratchBuffer));
#endif

		++loadedCount;
		++entryOffset;
	}

	File_Close(stream);
	g_movieSubtitleCount = loadedCount;
	return 1;
}

// FUNCTION: XWA 0x55C5F0
int Movie_DrawSubtitlesForCurrentFrame(void) {
	int subtitleIndex;

	subtitleIndex = g_movieSubtitleCurrentIndex;
	while ((unsigned int)subtitleIndex < (unsigned int)g_movieSubtitleCount) {
		MovieSubtitleEntry* entry;
		int color;
		int x;
		int y;
		char firstLine[1024];
		char secondLine[1024];

		entry = &g_movieSubtitles[subtitleIndex];
		if ((unsigned int)entry->startFrame > (unsigned int)g_movieFrameNumber) {
			break;
		}
		if ((unsigned int)g_movieFrameNumber >= (unsigned int)entry->endFrame + MOVIE_SUBTITLE_FADE_STEPS) {
			if (subtitleIndex == g_movieSubtitleCurrentIndex) {
				++g_movieSubtitleCurrentIndex;
			}
			++subtitleIndex;
			continue;
		}

		if ((unsigned int)(g_movieFrameNumber - entry->startFrame) < MOVIE_SUBTITLE_FADE_STEPS) {
			color = entry->fadeColors[g_movieFrameNumber - entry->startFrame];
		} else if ((unsigned int)g_movieFrameNumber >= (unsigned int)entry->endFrame) {
			color = entry->fadeColors[entry->endFrame - g_movieFrameNumber + MOVIE_SUBTITLE_FADE_STEPS - 1];
		} else {
			color = entry->fadeColors[MOVIE_SUBTITLE_FADE_STEPS - 1];
		}

		strcpy(firstLine, entry->text);
		secondLine[0] = '\0';
		x = entry->x;
		if (x < 0) {
			if (FrontendText_MeasureWidth(firstLine, entry->fontSize) >= 640) {
				size_t split;
				size_t length = strlen(firstLine);

				for (split = length / 2; split < length; ++split) {
					if (firstLine[split] == ' ') {
						size_t secondStart = split + 1;
						firstLine[split] = '\0';
						while (firstLine[secondStart] == ' ') {
							++secondStart;
						}
						strcpy(secondLine, &firstLine[secondStart]);
						break;
					}
				}
			}
			x = (640 - FrontendText_MeasureWidth(firstLine, entry->fontSize)) / 2;
		}

		y = entry->y;
		if (y < 0) {
			y = (480 - FrontendText_GetFontHeight(entry->fontSize)) / 2;
		}
		FrontendText_Draw(entry->fontSize, firstLine, x, y, color);
		if (secondLine[0] != '\0') {
			x = (640 - FrontendText_MeasureWidth(secondLine, entry->fontSize)) / 2;
			FrontendText_Draw(entry->fontSize, secondLine, x, y + entry->fontSize + 4, color);
		}
		++subtitleIndex;
	}

	return 1;
}
