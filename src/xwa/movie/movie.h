#ifndef XWA_MOVIE_MOVIE_H
#define XWA_MOVIE_MOVIE_H

#include "aeron/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	MOVIE_SUBTITLE_TEXT_SIZE = 256,
	MOVIE_SUBTITLE_FADE_STEPS = 10,
};

typedef struct MovieSubtitleEntry {
	char text[MOVIE_SUBTITLE_TEXT_SIZE];
	int fontSize;
	int x;
	int y;
	int fadeColors[MOVIE_SUBTITLE_FADE_STEPS];
	int startFrame;
	int endFrame;
} MovieSubtitleEntry;

extern int g_movieSubtitleCount;
extern MovieSubtitleEntry* g_movieSubtitles;
extern int g_movieSkipRequested;
extern int g_movieSubtitleCurrentIndex;
extern int g_movieFrameNumber;

int Movie_Play(const char* name, int noFade);
#ifdef XWA_MODERN
/* `root` is the VFS root that holds `moviePath`. */
int Movie_LoadSubtitles(AeronVfsRoot root, const char* moviePath);
#else
int Movie_LoadSubtitles(const char* moviePath);
#endif
#ifdef XWA_MODERN
void Movie_FreeSubtitles(void);
#endif
int Movie_DrawSubtitlesForCurrentFrame(void);

#ifdef __cplusplus
}
#endif

#endif
