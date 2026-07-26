#include "xwa/frontend/frontend_bootstrap.h"

#include "xwa/frontend/concourse.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/movie/movie.h"

#ifdef XWA_MODERN
static int g_frontendBootstrapIntroStarted;
static int g_frontendBootstrapIntroIndex;
#endif

// FUNCTION: XWA 0x57E4F0
int FrontendBootstrap_RunIntroAndEnterConcourse(int frameCounter) {
#ifdef XWA_MODERN
	static const char* const introMovies[] = {
		"logofinal",
		"tgintro",
		"intro_final",
	};

	(void)frameCounter;
	if (!g_frontendBootstrapIntroStarted) {
		FrontendDisplay_UnlockBackBuffer();
		g_frontendBootstrapIntroStarted = 1;
		g_frontendBootstrapIntroIndex = 0;
	}
	while (!g_movieSkipRequested &&
		   g_frontendBootstrapIntroIndex < (int)(sizeof(introMovies) / sizeof(introMovies[0]))) {
		const char* movie = introMovies[g_frontendBootstrapIntroIndex++];
		if (Movie_Play(movie, 0)) {
			return 0;
		}
	}

	g_frontendBootstrapIntroStarted = 0;
	g_frontendBootstrapIntroIndex = 0;
#else
	(void)frameCounter;

	FrontendDisplay_UnlockBackBuffer();
	Movie_Play("logofinal", 0);
	if (!g_movieSkipRequested) {
		Movie_Play("tgintro", 0);
		if (!g_movieSkipRequested) {
			Movie_Play("intro_final", 0);
		}
	}
#endif
	FrontendDisplay_ClearBackBuffer();
	FrontendScreen_SetCallbacks(FrontendBootstrap_EnterConcourse, FrontendBootstrap_LoadResources);
	g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	return 0;
}

// FUNCTION: XWA 0x57E560
int FrontendBootstrap_InitMode(void) {
	FrontendDisplay_DisableEscapeClose();
	FrontendDisplay_SetSurfaceClearColor(0);
	FrontendCursor_Hide();
	FrontendDisplay_ClearPresentFrameReady();
	FrontendDisplay_DisableOffscreenRestore();
	FrontendText_LoadFont(10);
	FrontendText_LoadFont(12);
	FrontendText_LoadFont(15);
	return 0;
}

// FUNCTION: XWA 0x584F30
int FrontendBootstrap_LoadResources(int frameCounter) {
	(void)frameCounter;

	FrontendText_ResetGlyphScratch();
	Frontend_LoadResources();
	return 0;
}

// FUNCTION: XWA 0x584F50
int FrontendBootstrap_EnterConcourse(int frameCounter) {
	(void)frameCounter;

	FrontendText_SetGlyphGradientBg(g_colorNearBlack);
	FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
	return 0;
}
