#ifndef XWA_FRONTEND_FRONTEND_COLOR_H
#define XWA_FRONTEND_FRONTEND_COLOR_H

#ifdef __cplusplus
extern "C" {
#endif

extern int g_colorMutedGreen;
extern int g_colorNearBlack;
extern int g_hudPreviewColors[5];
extern int g_colorGray;
extern int g_colorOrangeRed;
extern int g_colorLightBlue;
extern int g_colorPaleBlue;
extern int g_colorSlateBlue;
extern int g_colorRed2;
extern int g_colorNavy2;
extern int g_colorBlue2;
extern int g_colorYellow2;
extern int g_colorViolet;
extern int g_colorSpringGreen;
extern int g_colorMutedGreen2;
extern int g_colorCyan;
extern int g_colorAzure;
extern int g_colorOrange;
extern int g_colorDimGreenBlend;
extern int g_colorDarkGray;
extern int g_colorPaleCyan;
extern int g_colorRed;
extern int g_colorMidGray;
extern int g_colorTeal;
extern int g_colorGreen;
extern int g_colorBlue;
extern int g_colorYellow;
extern int g_colorGreen2;
extern int g_colorNavy;

extern int g_indexedColors[5];
extern int g_pulseColorRamp[12];
extern int g_textShadeRamps[5][8];

int FrontendColor_GetIndexed(int index);
int FrontendColor_Init(void);

#ifdef __cplusplus
}
#endif

#endif
