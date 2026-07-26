#include "xwa/xwa_options.h"

#include "xwa/frontend/frontend_image.h"

#include <string.h>

// GLOBAL: XWA 0xABC940
int g_optWkey;
// GLOBAL: XWA 0xABC944
int g_optIsHost;
// GLOBAL: XWA 0xABC948
int g_optGenerate;
// GLOBAL: XWA 0xABC950
int g_optNoFullscreen;
// GLOBAL: XWA 0xABC958
int g_optMultiRegion;
// GLOBAL: XWA 0xABC95C
int g_optIsClient;
// GLOBAL: XWA 0xABC960
int g_optSkipIntro;
// GLOBAL: XWA 0xABC964
int g_noPageFlip;

/* Reimplements the command-line switch parsing prefix of GameMain @ 0x53DED0. */
void XwaOptions_ParseCommandLine(const char* commandLine) {
	if (commandLine == 0) {
		commandLine = "";
	}

	g_optWkey = strstr(commandLine, "wkey") != 0;
	g_optMultiRegion = strstr(commandLine, "multiregion") != 0;
	g_bmpSaveEnabled = 1;
	g_optGenerate = strstr(commandLine, "generate") != 0;
	g_noPageFlip = 1;
	g_optNoFullscreen = strstr(commandLine, "nopageflip") != 0 || strstr(commandLine, "nofullscreen") != 0;
	g_optSkipIntro = strstr(commandLine, "skipintro") != 0;
	g_optIsHost = strstr(commandLine, "ishost") != 0;
	g_optIsClient = strstr(commandLine, "isclient") != 0;
}
