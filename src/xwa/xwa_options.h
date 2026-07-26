#ifndef XWA_XWA_OPTIONS_H
#define XWA_XWA_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

extern int g_optWkey;
extern int g_optIsHost;
extern int g_optGenerate;
extern int g_optNoFullscreen;
extern int g_optMultiRegion;
extern int g_optIsClient;
extern int g_optSkipIntro;
extern int g_noPageFlip;

void XwaOptions_ParseCommandLine(const char* commandLine);

#ifdef __cplusplus
}
#endif

#endif
