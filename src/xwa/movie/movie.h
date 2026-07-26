#ifndef XWA_MOVIE_MOVIE_H
#define XWA_MOVIE_MOVIE_H

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
int Movie_LoadSubtitles(const char* moviePath);
#ifdef XWA_MODERN
void Movie_FreeSubtitles(void);
#endif
int Movie_DrawSubtitlesForCurrentFrame(void);

#ifdef __cplusplus
}
#endif

#endif
